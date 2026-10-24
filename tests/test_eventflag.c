/* Kernel event flag runtime test: AND/OR waits, clear modes, timeouts, poll and cancel.
 * Build: cc -std=c11 -O2 -pthread -Isrc tests/test_eventflag.c -o out/eventflag-test */
#include "../src/runtime_eventflag.c"
#include <assert.h>
#include <unistd.h>

uintptr_t runtime_image_start, runtime_image_end;
const char *runtime_fiber_current(void) { return NULL; }
int runtime_rwlock_stale_holds(double seconds) { (void)seconds; return 0; }
void runtime_rwlock_dump(void) {}
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) {
    (void)table; (void)count; (void)nid;
    return 0;
}

static void *flag;
static void *setter(void *arg) {
    (void)arg;
    usleep(20000);
    evf_set(flag, 0x6);
    return NULL;
}

int main(void) {
    assert(evf_create(&flag, "test", ATTR_MULTI, 0x1, NULL) == 0);
    uint64_t result = 0;
    /* Already satisfied, clear pattern bits. */
    assert(evf_wait(flag, 0x1, WAIT_AND | CLEAR_PAT, &result, NULL) == 0 && result == 0x1);
    assert(evf_poll(flag, 0x1, WAIT_OR, &result) == ERR(E_BUSY));
    /* Timed out. */
    uint32_t timeout = 1000;
    assert(evf_wait(flag, 0x8, WAIT_OR, &result, &timeout) == ERR(E_TIMEDOUT) && timeout == 0);
    /* Woken by another thread, AND of two bits, clear all. */
    pthread_t t;
    pthread_create(&t, NULL, setter, NULL);
    assert(evf_wait(flag, 0x6, WAIT_AND | CLEAR_ALL, &result, NULL) == 0 && result == 0x6);
    pthread_join(t, NULL);
    assert(evf_poll(flag, 0x6, WAIT_OR, &result) == ERR(E_BUSY));
    /* Clear keeps the bits set in its argument. */
    evf_set(flag, 0xff);
    evf_clear(flag, 0x0f);
    assert(evf_poll(flag, 0xf0, WAIT_OR, &result) == ERR(E_BUSY) && result == 0x0f);
    int32_t waiting = -1;
    assert(evf_cancel(flag, 0x3, &waiting) == 0 && waiting == 0);
    assert(evf_delete(flag) == 0);
    puts("event flag test: OK");
    return 0;
}
