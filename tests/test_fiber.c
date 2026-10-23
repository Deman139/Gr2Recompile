/* libSceFiber runtime test (Linux/Windows host, no game): two fibers with their own contexts
 * switch to each other and return to the thread, as Gravity Rush 2's job system does.
 * Build: cc -std=c11 -O2 -pthread -Isrc tests/test_fiber.c -o out/fiber-test */
#include "../src/runtime_fiber.c"
#include <assert.h>

/* runtime.c is not linked: the lookup is not used here. */
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) {
    (void)table; (void)count; (void)nid;
    return 0;
}

static Fiber fiber_a, fiber_b;
static _Alignas(16) unsigned char stack_a[64 * 1024], stack_b[64 * 1024];
static int trace[16], steps;

static void ABI entry_a(uint64_t init, uint64_t run) {
    assert(init == 0xA);
    trace[steps++] = (int)run;               /* 1: started by sceFiberRun */
    uint64_t back = 0;
    int32_t r = fiber_switch(&fiber_b, 2, &back);
    assert(r == 0);
    trace[steps++] = (int)back;              /* 4: B switched back with 4 */
    uint64_t again = 0;
    r = fiber_return_to_thread(5, &again);   /* to the thread with 5 */
    assert(r == 0);
    trace[steps++] = (int)again;             /* 7: run again with 7 */
    fiber_return_to_thread(8, NULL);
    assert(!"not resumed");
}

static void ABI entry_b(uint64_t init, uint64_t run) {
    assert(init == 0xB);
    trace[steps++] = (int)run;               /* 2 */
    Fiber *self = NULL;
    assert(fiber_get_self(&self) == 0 && self == &fiber_b);
    trace[steps++] = 3;
    uint64_t back = 0;
    fiber_switch(&fiber_a, 4, &back);
    assert(!"B is not resumed in this test");
}

int main(void) {
    assert(fiber_initialize(&fiber_a, "fiber A", entry_a, 0xA, stack_a, sizeof(stack_a), NULL, SDK_350) == 0);
    assert(fiber_initialize(&fiber_b, "fiber B", entry_b, 0xB, stack_b, sizeof(stack_b), NULL, SDK_350) == 0);
    Fiber *none;
    assert(fiber_get_self(&none) == FIBER_ERROR_PERMISSION);
    uint64_t returned = 0;
    assert(fiber_run(&fiber_a, 1, &returned) == 0);
    assert(returned == 5);
    assert(atomic_load(&fiber_a.state) == STATE_IDLE);
    assert(fiber_run(&fiber_a, 7, &returned) == 0);
    assert(returned == 8);
    static const int expected[] = {1, 2, 3, 4, 7};
    assert(steps == 5);
    for (int i = 0; i < steps; ++i) assert(trace[i] == expected[i]);
    assert(fiber_finalize(&fiber_a) == 0);
    assert(fiber_finalize(&fiber_a) == FIBER_ERROR_STATE);
    /* B was switched away from (A set it idle when resumed): it can be finalized too. */
    assert(fiber_finalize(&fiber_b) == 0);
    FiberInfo info = {.size = sizeof(info)};
    assert(fiber_get_info(&fiber_a, &info) == 0 && !strcmp(info.name, "fiber A"));
    puts("fiber test: OK");
    return 0;
}
