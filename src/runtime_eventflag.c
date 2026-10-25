/* Kernel event flags (sceKernel*EventFlag): a 64-bit pattern that threads wait on with AND/OR
 * conditions and optional clearing. Follows shadPS4's event_flag.cpp (GPL-2.0-or-later); the
 * handle the game holds is a pointer to the host object, as there. Priority-ordered queues are
 * served FIFO (host threads have no PS4 priorities), like the semaphores in runtime_sema.c. */
#define _GNU_SOURCE
#include "runtime.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ERR(n) ((int32_t)(UINT32_C(0x80020000) | (n)))
enum { E_PERM = 1, E_SRCH = 3, E_ACCES = 13, E_BUSY = 16, E_INVAL = 22, E_TIMEDOUT = 60,
       E_NAMETOOLONG = 63, E_CANCELED = 85 };
enum { ATTR_TH_PRIO = 0x02, ATTR_SINGLE = 0x10, ATTR_MULTI = 0x20 };
enum { WAIT_AND = 0x01, WAIT_OR = 0x02, CLEAR_ALL = 0x10, CLEAR_PAT = 0x20 };
#ifdef _WIN32
#define WAIT_CLOCK CLOCK_REALTIME /* winpthreads condition variables accept only this clock */
#else
#define WAIT_CLOCK CLOCK_MONOTONIC
#endif

typedef struct Waiter {
    pthread_cond_t event;
    uint64_t bits, result;
    uint32_t mode;
    int ready, canceled, deleted;
    unsigned long tid;
    uintptr_t caller;
    const char *fiber;
    struct Waiter *next;
} Waiter;
typedef struct {
    uint32_t magic;
    pthread_mutex_t lock;
    pthread_cond_t destroyed;
    char name[32];
    int multi, deleted;
    uint64_t bits;
    Waiter *first;
    struct EventFlagLink *link;
} EventFlag;
/* Every live event flag, for the hang report (runtime_eventflag_dump). */
typedef struct EventFlagLink { EventFlag *flag; struct EventFlagLink *next; } EventFlagLink;
static pthread_mutex_t flags_lock = PTHREAD_MUTEX_INITIALIZER;
static EventFlagLink *all_flags;
#define EVF_MAGIC UINT32_C(0x45564631)

static atomic_ullong created, waits, blocked, sets;

static void host_check(int e) {
    if (e) { fprintf(stderr, "STOP: event flag host error %d\n", e); exit(21); }
}

static int condition_met(const EventFlag *f, uint64_t bits, uint32_t mode) {
    return (mode & 0xf) == WAIT_AND ? (f->bits & bits) == bits : (f->bits & bits) != 0;
}

static void apply_clear(EventFlag *f, uint64_t bits, uint32_t mode) {
    if ((mode & 0xf0) == CLEAR_ALL) f->bits = 0;
    else if ((mode & 0xf0) == CLEAR_PAT) f->bits &= ~bits;
}

static void unlink_waiter(EventFlag *f, Waiter *w) {
    for (Waiter **p = &f->first; *p; p = &(*p)->next)
        if (*p == w) { *p = w->next; break; }
    if (f->deleted && !f->first) pthread_cond_broadcast(&f->destroyed);
}

static EventFlag *checked(void *handle) {
    EventFlag *f = handle;
    return f && f->magic == EVF_MAGIC ? f : NULL;
}

static ABI int32_t evf_create(void **out, const char *name, uint32_t attr, uint64_t pattern, const void *options) {
    if (!out || !name) return ERR(E_INVAL);
    if (options || attr > (ATTR_MULTI | ATTR_TH_PRIO) || (attr & 0xf) > 2 || ((attr & 0xf0) && (attr & 0xf0) != ATTR_SINGLE &&
        (attr & 0xf0) != ATTR_MULTI)) return ERR(E_INVAL);
    if (strlen(name) >= 32) return ERR(E_NAMETOOLONG);
    EventFlag *f = calloc(1, sizeof(*f));
    if (!f) return ERR(12);
    f->magic = EVF_MAGIC;
    host_check(pthread_mutex_init(&f->lock, NULL));
    host_check(pthread_cond_init(&f->destroyed, NULL));
    strcpy(f->name, name);
    f->multi = (attr & 0xf0) == ATTR_MULTI;
    f->bits = pattern;
    EventFlagLink *link = calloc(1, sizeof(*link));
    if (link) {
        link->flag = f;
        pthread_mutex_lock(&flags_lock);
        link->next = all_flags; all_flags = link; f->link = link;
        pthread_mutex_unlock(&flags_lock);
    }
    *out = f;
    atomic_fetch_add(&created, 1);
    if (getenv("GR2_TRACE_EVF"))
        printf("Runtime: event flag %p '%s' attr=0x%x pattern=0x%llx\n", (void *)f, name, attr, (unsigned long long)pattern);
    return 0;
}

static ABI int32_t evf_delete(void *handle) {
    EventFlag *f = checked(handle);
    if (!f) return ERR(E_SRCH);
    /* Out of the hang report's list first (it locks flags_lock, then each flag). */
    pthread_mutex_lock(&flags_lock);
    for (EventFlagLink **p = &all_flags; *p; p = &(*p)->next)
        if (*p == f->link) { EventFlagLink *dead = *p; *p = dead->next; free(dead); break; }
    pthread_mutex_unlock(&flags_lock);
    pthread_mutex_lock(&f->lock);
    f->deleted = 1;
    for (Waiter *w = f->first; w; w = w->next) { w->deleted = 1; pthread_cond_signal(&w->event); }
    while (f->first) pthread_cond_wait(&f->destroyed, &f->lock);
    f->magic = 0;
    pthread_mutex_unlock(&f->lock);
    pthread_mutex_destroy(&f->lock);
    pthread_cond_destroy(&f->destroyed);
    free(f);
    return 0;
}

/* timeout: microseconds, NULL waits forever; 0 polls. */
static int32_t wait_flag(EventFlag *f, uint64_t bits, uint32_t mode, uint64_t *result, uint32_t *timeout, uintptr_t caller) {
    if (!bits) return ERR(E_INVAL);
    uint32_t wait = mode & 0xf, clear = mode & 0xf0;
    if ((wait != WAIT_AND && wait != WAIT_OR) || (clear && clear != CLEAR_ALL && clear != CLEAR_PAT) || (mode & ~0xffu))
        return ERR(E_INVAL);
    atomic_fetch_add(&waits, 1);
    pthread_mutex_lock(&f->lock);
    if (f->deleted) { pthread_mutex_unlock(&f->lock); return ERR(E_ACCES); }
    if (!f->multi && f->first) { pthread_mutex_unlock(&f->lock); return ERR(E_PERM); }
    if (condition_met(f, bits, mode)) {
        if (result) *result = f->bits;
        apply_clear(f, bits, mode);
        pthread_mutex_unlock(&f->lock);
        return 0;
    }
    if (timeout && !*timeout) {
        if (result) *result = f->bits;
        pthread_mutex_unlock(&f->lock);
        return ERR(E_TIMEDOUT);
    }
    atomic_fetch_add(&blocked, 1);
    Waiter w = {.bits = bits, .mode = mode, .tid = runtime_tid(), .caller = caller, .fiber = runtime_fiber_current()};
    pthread_condattr_t attr;
    host_check(pthread_condattr_init(&attr));
    host_check(pthread_condattr_setclock(&attr, WAIT_CLOCK));
    host_check(pthread_cond_init(&w.event, &attr));
    pthread_condattr_destroy(&attr);
    Waiter **tail = &f->first;
    while (*tail) tail = &(*tail)->next;
    *tail = &w;
    int timed_out = 0;
    struct timespec deadline;
    if (timeout) {
        clock_gettime(WAIT_CLOCK, &deadline);
        uint64_t ns = (uint64_t)deadline.tv_nsec + (uint64_t)*timeout * 1000;
        deadline.tv_sec += (time_t)(ns / 1000000000);
        deadline.tv_nsec = (long)(ns % 1000000000);
    }
    /* Untimed waits wake every 10 s: while an rwlock has been held for that long too, the game
     * is probably hung, and the first such wait prints what every thread waits for. */
    int slices = 0;
    while (!w.ready && !w.canceled && !w.deleted) {
        int e;
        if (timeout) e = pthread_cond_timedwait(&w.event, &f->lock, &deadline);
        else {
            struct timespec slice;
            clock_gettime(WAIT_CLOCK, &slice);
            slice.tv_sec += 10;
            e = pthread_cond_timedwait(&w.event, &f->lock, &slice);
            if (e == ETIMEDOUT) {
                if (++slices == 1 && runtime_rwlock_stale_holds(10.0)) {
                    static atomic_int reports;
                    if (atomic_fetch_add(&reports, 1) < 2) {
                        pthread_mutex_unlock(&f->lock);
                        fprintf(stderr, "Runtime: thread %lu has waited 10 s on event flag '%s' while an rwlock is held "
                                "as long; the game may hang:\n", w.tid, f->name);
                        runtime_rwlock_dump();
                        runtime_eventflag_dump();
                        pthread_mutex_lock(&f->lock);
                    }
                }
                continue;
            }
        }
        if (e == ETIMEDOUT) {
            if (!w.ready && !w.canceled && !w.deleted) timed_out = 1;
            break;
        }
        if (e) host_check(e);
    }
    unlink_waiter(f, &w);
    int32_t status = 0;
    if (timed_out) {
        if (result) *result = f->bits;
        *timeout = 0;
        status = ERR(E_TIMEDOUT);
    } else {
        if (result) *result = w.result;
        if (w.canceled) status = ERR(E_CANCELED);
        else if (w.deleted) status = ERR(E_ACCES);
    }
    pthread_mutex_unlock(&f->lock);
    pthread_cond_destroy(&w.event);
    return status;
}

static ABI int32_t evf_wait(void *handle, uint64_t bits, uint32_t mode, uint64_t *result, uint32_t *timeout) {
    EventFlag *f = checked(handle);
    return f ? wait_flag(f, bits, mode, result, timeout, (uintptr_t)__builtin_return_address(0)) : ERR(E_SRCH);
}

static ABI int32_t evf_poll(void *handle, uint64_t bits, uint32_t mode, uint64_t *result) {
    EventFlag *f = checked(handle);
    if (!f) return ERR(E_SRCH);
    uint32_t zero = 0;
    int32_t status = wait_flag(f, bits, mode, result, &zero, 0);
    return status == ERR(E_TIMEDOUT) ? ERR(E_BUSY) : status;
}

static ABI int32_t evf_set(void *handle, uint64_t bits) {
    EventFlag *f = checked(handle);
    if (!f) return ERR(E_SRCH);
    atomic_fetch_add(&sets, 1);
    pthread_mutex_lock(&f->lock);
    f->bits |= bits;
    for (Waiter *w = f->first; w; w = w->next) {
        if (w->ready || !condition_met(f, w->bits, w->mode)) continue;
        w->result = f->bits;
        apply_clear(f, w->bits, w->mode);
        w->ready = 1;
        pthread_cond_signal(&w->event);
    }
    pthread_mutex_unlock(&f->lock);
    return 0;
}

static ABI int32_t evf_clear(void *handle, uint64_t bits) {
    EventFlag *f = checked(handle);
    if (!f) return ERR(E_SRCH);
    pthread_mutex_lock(&f->lock);
    f->bits &= bits; /* the pattern keeps the bits that are set in the argument */
    pthread_mutex_unlock(&f->lock);
    return 0;
}

static ABI int32_t evf_cancel(void *handle, uint64_t pattern, int32_t *waiting) {
    EventFlag *f = checked(handle);
    if (!f) return ERR(E_SRCH);
    pthread_mutex_lock(&f->lock);
    int32_t count = 0;
    f->bits = pattern;
    for (Waiter *w = f->first; w; w = w->next) {
        if (w->ready) continue;
        ++count;
        w->result = pattern;
        w->canceled = 1;
        pthread_cond_signal(&w->event);
    }
    if (waiting) *waiting = count;
    pthread_mutex_unlock(&f->lock);
    return 0;
}

static const RuntimeExport exports[] = {
    {"sceKernelCreateEventFlag", evf_create}, {"sceKernelDeleteEventFlag", evf_delete},
    {"sceKernelWaitEventFlag", evf_wait},     {"sceKernelPollEventFlag", evf_poll},
    {"sceKernelSetEventFlag", evf_set},       {"sceKernelClearEventFlag", evf_clear},
    {"sceKernelCancelEventFlag", evf_cancel},
};

uintptr_t runtime_eventflag_resolve(const char *name) { return RUNTIME_LOOKUP(exports, name); }

void runtime_eventflag_dump(void) {
    pthread_mutex_lock(&flags_lock);
    fputs("Runtime: event flags with waiting threads:\n", stderr);
    for (EventFlagLink *link = all_flags; link; link = link->next) {
        EventFlag *f = link->flag;
        pthread_mutex_lock(&f->lock);
        if (f->first) {
            fprintf(stderr, "  '%s' %p: pattern 0x%llx\n", f->name, (void *)f, (unsigned long long)f->bits);
            for (Waiter *w = f->first; w; w = w->next) {
                const uintptr_t at = w->caller - runtime_image_start;
                fprintf(stderr, "    thread %lu waits for 0x%llx (%s%s), caller %s0x%llx%s%s%s\n", w->tid,
                        (unsigned long long)w->bits, (w->mode & 0xf) == WAIT_AND ? "all" : "any",
                        w->ready ? ", woken" : "",
                        w->caller >= runtime_image_start && w->caller < runtime_image_end ? "image+" : "",
                        (unsigned long long)(w->caller >= runtime_image_start && w->caller < runtime_image_end ? at : w->caller),
                        w->fiber ? " in fiber '" : "", w->fiber ? w->fiber : "", w->fiber ? "'" : "");
            }
        }
        pthread_mutex_unlock(&f->lock);
    }
    fflush(stderr);
    pthread_mutex_unlock(&flags_lock);
}

void runtime_eventflag_report(void) {
    printf("Runtime: event flags created=%llu, waits=%llu (blocked %llu), sets=%llu\n",
           (unsigned long long)atomic_load(&created), (unsigned long long)atomic_load(&waits),
           (unsigned long long)atomic_load(&blocked), (unsigned long long)atomic_load(&sets));
}
