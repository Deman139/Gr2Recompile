/* PS4 (FreeBSD libthr) rwlocks behind the pointer-to-handle ABI, implemented here on one mutex and
   per-lock condition variables instead of winpthreads' rwlocks:
   - like libthr, a thread that already holds read locks may take another one while a writer
     waits (winpthreads blocks it behind the writer: a deadlock libthr avoids on purpose);
   - a hold may be released by another thread (Gravity Rush 2 runs its jobs as fibers, which
     can end on another worker thread than the one they started on);
   - every hold remembers its thread, fiber, guest caller and age, and a wait longer than 5 s
     prints the holders and the event flag waiters (stderr): the game then hangs, and this is
     what the report needs. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

typedef struct Holder {
    unsigned long tid;
    unsigned readers, writer, pending;
    uintptr_t caller;   /* guest return address of the last acquisition */
    uint64_t since_ms;  /* when it was taken */
    char fiber[32];     /* the fiber that took it, if any */
    struct Holder *next;
} Holder;
typedef struct Rwlock {
    pthread_cond_t changed;
    unsigned readers, writer, waiting_writers;
    Holder *holders;
    unsigned inflight;
    struct Rwlock *next;
} Rwlock;
typedef struct { int64_t seconds, nanoseconds; } GuestTime;
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
static Rwlock *registry;
static size_t created, reads, writes, unlocks, foreign_unlocks;

static uint64_t now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
static void describe(char *out, size_t size, uintptr_t address) {
    if (address >= runtime_image_start && address < runtime_image_end)
        snprintf(out, size, "image+0x%llx", (unsigned long long)(address - runtime_image_start));
    else
        snprintf(out, size, "%p", (void *)address);
}
static int32_t error(int e) {
    if (!e) return 0;
    unsigned code;
    switch(e) {
    case EPERM: code=1; break;
    case EDEADLK: code=11; break;
    case ENOMEM: code=12; break;
    case EBUSY: code=16; break;
    case EINVAL: code=22; break;
    case EAGAIN: code=35; break;
    case ETIMEDOUT: code=60; break;
    default: fprintf(stderr,"STOP: unmapped rwlock error %d\n",e); exit(21);
    }
    return (int32_t)(UINT32_C(0x80020000)|code);
}
/* All helpers ending in _locked require registry_lock. */
static Rwlock *find_locked(Rwlock *handle) {
    for (Rwlock *r=registry; r; r=r->next) if (r==handle) return r;
    return NULL;
}
static int create_locked(Rwlock **out) {
    Rwlock *r=calloc(1,sizeof(*r));
    if (!r) return ENOMEM;
    int e=pthread_cond_init(&r->changed,NULL);
    if (e) { free(r); return e; }
    r->next=registry; registry=r; *out=r; ++created; return 0;
}
static ABI int32_t rw_init(Rwlock **out, void *const *attr, const char *name) {
    (void)name;
    if (!out || (attr && !*attr)) return error(EINVAL);
    if (attr) { fputs("STOP: non-default PS4 rwlock attributes are not implemented\n",stderr); exit(21); }
    pthread_mutex_lock(&registry_lock);
    /* POSIX allows uninitialized storage, so never read *out during init. */
    int e=create_locked(out);
    pthread_mutex_unlock(&registry_lock); return error(e);
}
static void prune_locked(Rwlock *r, Holder *h) {
    if (h->readers || h->writer || h->pending) return;
    Holder **link=&r->holders;
    while (*link!=h) link=&(*link)->next;
    *link=h->next; free(h);
}
static void dump_lock_locked(const Rwlock *r) {
    const uint64_t now=now_ms();
    fprintf(stderr,"  rwlock %p: readers %u, writer %u, waiting writers %u\n",(const void *)r,r->readers,r->writer,
            r->waiting_writers);
    for (const Holder *h=r->holders; h; h=h->next) {
        if (!h->readers && !h->writer) { fprintf(stderr,"    thread %lu: waiting\n",h->tid); continue; }
        char caller[48];
        describe(caller,sizeof(caller),h->caller);
        fprintf(stderr,"    thread %lu: %u read(s)%s%s, taken %.1f s ago by %s%s%s%s\n",h->tid,h->readers,
                h->writer ? ", WRITE" : "",h->pending ? ", waiting" : "",(now-h->since_ms)/1000.0,caller,
                h->fiber[0] ? " in fiber '" : "",h->fiber,h->fiber[0] ? "'" : "");
    }
}
enum Operation { READ, WRITE, TRY_READ, TRY_WRITE, TIMED_READ, TIMED_WRITE };
static int32_t acquire(Rwlock **handle, enum Operation op, const GuestTime *time, uintptr_t caller) {
    if (!handle) return error(EINVAL);
    int writer=op==WRITE || op==TRY_WRITE || op==TIMED_WRITE;
    int attempt=op==TRY_READ || op==TRY_WRITE;
    int timed=op==TIMED_READ || op==TIMED_WRITE;
    const unsigned long self=runtime_tid();
    pthread_mutex_lock(&registry_lock);
    int e=0;
    if (!*handle) e=create_locked(handle);
    Rwlock *r=e ? NULL : find_locked(*handle);
    if (!r) { pthread_mutex_unlock(&registry_lock); return error(e ? e : EINVAL); }
    Holder *h=r->holders;
    while (h && h->tid!=self) h=h->next;
    if (h && (h->writer || (writer && h->readers))) {
        pthread_mutex_unlock(&registry_lock); return error(attempt ? EBUSY : EDEADLK);
    }
    if (h && h->readers==UINT_MAX) { pthread_mutex_unlock(&registry_lock); return error(EAGAIN); }
    if (timed && (!time || time->nanoseconds<0 || time->nanoseconds>=1000000000)) {
        pthread_mutex_unlock(&registry_lock); return error(EINVAL);
    }
    if (!h) {
        h=calloc(1,sizeof(*h));
        if (!h) { pthread_mutex_unlock(&registry_lock); return error(ENOMEM); }
        h->tid=self; h->next=r->holders; r->holders=h;
    }
    ++r->inflight; ++h->pending;
    const uint64_t start=now_ms();
    int reported=0;
    for (;;) {
        /* libthr: readers wait behind waiting writers unless they already hold this lock. */
        const int free_now=writer ? !r->writer && !r->readers
                                  : !r->writer && (!r->waiting_writers || h->readers);
        if (free_now) break;
        if (attempt) { e=EBUSY; break; }
        struct timespec deadline;
        if (timed) {
            deadline.tv_sec=(time_t)time->seconds; deadline.tv_nsec=(long)time->nanoseconds;
        } else {
            clock_gettime(CLOCK_REALTIME,&deadline); deadline.tv_sec+=1;
        }
        if (writer) ++r->waiting_writers;
        int w=pthread_cond_timedwait(&r->changed,&registry_lock,&deadline);
        if (writer) --r->waiting_writers;
        if (w==ETIMEDOUT && timed) { e=ETIMEDOUT; break; }
        if (w && w!=ETIMEDOUT) { e=w; break; }
        if (!reported && now_ms()-start>=5000) {
            reported=1;
            char where[48];
            describe(where,sizeof(where),caller);
            const char *fiber=runtime_fiber_current();
            fprintf(stderr,"Runtime: thread %lu%s%s%s has waited 5 s for a %s lock (caller %s); the game hangs here "
                    "unless a holder releases it:\n",self,fiber ? " (fiber '" : "",fiber ? fiber : "",fiber ? "')" : "",
                    writer ? "write" : "read",where);
            dump_lock_locked(r);
            fflush(stderr);
            pthread_mutex_unlock(&registry_lock);
            runtime_eventflag_dump();
            pthread_mutex_lock(&registry_lock);
        }
    }
    --r->inflight; --h->pending;
    if (!e) {
        if (writer) { r->writer=1; h->writer=1; ++writes; }
        else { ++r->readers; ++h->readers; ++reads; }
        h->caller=caller; h->since_ms=now_ms();
        const char *fiber=runtime_fiber_current();
        snprintf(h->fiber,sizeof(h->fiber),"%s",fiber ? fiber : "");
    } else if (writer) {
        pthread_cond_broadcast(&r->changed); /* readers held back by this writer go on */
    }
    prune_locked(r,h);
    pthread_mutex_unlock(&registry_lock); return error(e);
}
#define CALLER ((uintptr_t)__builtin_return_address(0))
static ABI int32_t rw_read(Rwlock **r) { return acquire(r,READ,NULL,CALLER); }
static ABI int32_t rw_write(Rwlock **r) { return acquire(r,WRITE,NULL,CALLER); }
static ABI int32_t rw_tryread(Rwlock **r) { return acquire(r,TRY_READ,NULL,CALLER); }
static ABI int32_t rw_trywrite(Rwlock **r) { return acquire(r,TRY_WRITE,NULL,CALLER); }
static ABI int32_t rw_timedread(Rwlock **r,const GuestTime *t) { return acquire(r,TIMED_READ,t,CALLER); }
static ABI int32_t rw_timedwrite(Rwlock **r,const GuestTime *t) { return acquire(r,TIMED_WRITE,t,CALLER); }
static ABI int32_t rw_unlock(Rwlock **handle) {
    if (!handle) return error(EINVAL);
    const unsigned long self=runtime_tid();
    pthread_mutex_lock(&registry_lock);
    Rwlock *r=find_locked(*handle);
    if (!r) { pthread_mutex_unlock(&registry_lock); return error(EINVAL); }
    Holder *h=r->holders;
    while (h && h->tid!=self) h=h->next;
    if (!h || (!h->readers && !h->writer)) {
        /* FreeBSD (libthr) lets any thread drop a read hold: Killzone takes read locks on one
           thread and releases them on another. Gravity Rush 2's job fibers may also end on
           another worker thread than the one that took the lock: its hold is dropped too. */
        Holder *owner=NULL;
        for (Holder *o=r->holders; o && !owner; o=o->next) if (o->writer) owner=o;
        for (Holder *o=r->holders; o && !owner; o=o->next) if (o->readers) owner=o;
        if (!owner) { pthread_mutex_unlock(&registry_lock); return error(EPERM); }
        if (!foreign_unlocks++) {
            char where[48];
            describe(where,sizeof(where),CALLER);
            fprintf(stderr,"Runtime: rwlock %s hold of thread %lu released by thread %lu (%s)\n",
                    owner->writer ? "write" : "read",owner->tid,self,where);
        }
        h=owner;
    }
    if (h->writer) { h->writer=0; r->writer=0; }
    else { --h->readers; --r->readers; }
    ++unlocks;
    if (!r->writer && (!r->readers || r->waiting_writers)) pthread_cond_broadcast(&r->changed);
    prune_locked(r,h);
    pthread_mutex_unlock(&registry_lock); return 0;
}
static ABI int32_t rw_destroy(Rwlock **handle) {
    if (!handle) return error(EINVAL);
    pthread_mutex_lock(&registry_lock);
    if (!*handle) { pthread_mutex_unlock(&registry_lock); return 0; }
    Rwlock *r=find_locked(*handle);
    if (!r) { pthread_mutex_unlock(&registry_lock); return error(EINVAL); }
    if (r->inflight || r->holders) { pthread_mutex_unlock(&registry_lock); return error(EBUSY); }
    pthread_cond_destroy(&r->changed);
    Rwlock **link=&registry;
    while (*link!=r) link=&(*link)->next;
    *link=r->next; free(r); *handle=(Rwlock *)(uintptr_t)1;
    pthread_mutex_unlock(&registry_lock); return 0;
}
uintptr_t runtime_rwlock_resolve(const char *name) {
    if (!strcmp(name,"6ULAa0fq4jA#libkernel")) return (uintptr_t)rw_init;
    if (!strcmp(name,"BB+kb08Tl9A#libkernel")) return (uintptr_t)rw_destroy;
    if (!strcmp(name,"Ox9i0c7L5w0#libkernel")) return (uintptr_t)rw_read;
    if (!strcmp(name,"mqdNorrB+gI#libkernel")) return (uintptr_t)rw_write;
    if (!strcmp(name,"XD3mDeybCnk#libkernel")) return (uintptr_t)rw_tryread;
    if (!strcmp(name,"bIHoZCTomsI#libkernel")) return (uintptr_t)rw_trywrite;
    if (!strcmp(name,"+L98PIbGttk#libkernel")) return (uintptr_t)rw_unlock;
    if (!strcmp(name,"iPtZRWICjrM#libkernel")) return (uintptr_t)rw_timedread;
    if (!strcmp(name,"adh--6nIqTk#libkernel")) return (uintptr_t)rw_timedwrite;
    return 0;
}
int runtime_rwlock_stale_holds(double seconds) {
    const uint64_t now=now_ms();
    int count=0;
    pthread_mutex_lock(&registry_lock);
    for (Rwlock *r=registry; r; r=r->next)
        for (Holder *h=r->holders; h; h=h->next)
            if ((h->readers || h->writer) && (now-h->since_ms)/1000.0>seconds) ++count;
    pthread_mutex_unlock(&registry_lock);
    return count;
}
void runtime_rwlock_dump(void) {
    pthread_mutex_lock(&registry_lock);
    fputs("Runtime: rwlocks held now:\n",stderr);
    for (Rwlock *r=registry; r; r=r->next) if (r->readers || r->writer || r->waiting_writers) dump_lock_locked(r);
    fflush(stderr);
    pthread_mutex_unlock(&registry_lock);
}
void runtime_rwlock_report(void) {
    pthread_mutex_lock(&registry_lock);
    printf("Runtime: rwlocks created=%zu, reads=%zu, writes=%zu, unlocks=%zu, released by another thread=%zu\n",
           created,reads,writes,unlocks,foreign_unlocks);
    pthread_mutex_unlock(&registry_lock);
}
