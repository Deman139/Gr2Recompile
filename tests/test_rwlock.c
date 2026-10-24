/* Rwlock runtime test: libthr reader re-entry, cross-thread release, contention, hang report.
 * Build: cc -std=gnu11 -O2 -pthread -Isrc tests/test_rwlock.c -o out/rwlock-test */
#include "../src/runtime_rwlock.c"
#include <assert.h>
#include <unistd.h>
uintptr_t runtime_image_start=0x1000, runtime_image_end=0x2000;
const char *runtime_fiber_current(void) { return NULL; }
int dumps;
void runtime_eventflag_dump(void) { ++dumps; }
static Rwlock *lock;
static volatile int stage;
static void *reader_then_unlock_elsewhere(void *a) { (void)a; assert(rw_read(&lock)==0); return NULL; }
static void *writer(void *a) { (void)a; assert(rw_write(&lock)==0); stage=2; assert(rw_unlock(&lock)==0); return NULL; }
static void *counter(void *a) {
    long *n=a;
    for (int i=0;i<20000;++i) {
        if (i%7==0) { assert(rw_write(&lock)==0); ++*n; assert(rw_unlock(&lock)==0); }
        else { assert(rw_read(&lock)==0); assert(rw_unlock(&lock)==0); }
    }
    return NULL;
}
int main(void) {
    assert(rw_init(&lock,NULL,"t")==0);
    /* recursive read while a writer waits (libthr lets it through) */
    assert(rw_read(&lock)==0);
    pthread_t w; pthread_create(&w,NULL,writer,NULL);
    usleep(100000);
    assert(stage==0);
    assert(rw_read(&lock)==0);          /* would deadlock behind the writer in winpthreads */
    assert(rw_unlock(&lock)==0); assert(rw_unlock(&lock)==0);
    pthread_join(w,NULL); assert(stage==2);
    /* write then read on the same thread: EDEADLK; try: EBUSY */
    assert(rw_write(&lock)==0);
    assert(rw_read(&lock)==error(EDEADLK));
    assert(rw_tryread(&lock)==error(EBUSY));
    assert(rw_unlock(&lock)==0);
    /* read hold released by another thread */
    pthread_t r; pthread_create(&r,NULL,reader_then_unlock_elsewhere,NULL); pthread_join(r,NULL);
    assert(rw_trywrite(&lock)==error(EBUSY));
    assert(rw_unlock(&lock)==0);
    assert(rw_trywrite(&lock)==0);
    assert(rw_unlock(&lock)==0);
    assert(rw_unlock(&lock)==error(EPERM));
    /* contention */
    long n=0; pthread_t t[4];
    for (int i=0;i<4;++i) pthread_create(&t[i],NULL,counter,&n);
    for (int i=0;i<4;++i) pthread_join(t[i],NULL);
    assert(n==4*((20000+6)/7));
    /* a wait over 5 s reports the holders */
    pthread_create(&r,NULL,reader_then_unlock_elsewhere,NULL); pthread_join(r,NULL);
    pthread_create(&w,NULL,writer,NULL);
    sleep(6);
    assert(dumps==1);
    assert(rw_unlock(&lock)==0);
    pthread_join(w,NULL);
    assert(rw_destroy(&lock)==0);
    runtime_rwlock_report();
    puts("rwlock ok");
}
