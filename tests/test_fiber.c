/* libSceFiber runtime test (Linux/Windows host, no game): two fibers with their own contexts
 * switch to each other and return to the thread, as Gravity Rush 2's job system does.
 * Build: cc -std=c11 -O2 -pthread -Isrc tests/test_fiber.c -o out/fiber-test */
#include "../src/runtime_fiber.c"
#include <assert.h>
#include <pthread.h>

/* runtime.c is not linked: the lookup is not used here. */
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) {
    (void)table; (void)count; (void)nid;
    return 0;
}
/* runtime_memory.c is not linked either: host stacks come from the C heap here. */
void *runtime_low_map(size_t size, int prot) {
    (void)prot;
    uintptr_t p = (uintptr_t)malloc(size + 65536); /* never freed: a test */
    return p ? (void *)((p + 65535) & ~(uintptr_t)65535) : NULL;
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

/* A fiber that returns to one thread and is run again by another (GR2's workers share fibers). */
static Fiber migrant;
static _Alignas(16) unsigned char stack_m[64 * 1024];
static volatile int migrant_steps;
static void ABI entry_migrant(uint64_t init, uint64_t run) {
    (void)init;
    assert(run == 10);
    migrant_steps = 1;
    uint64_t again = 0;
    assert(fiber_return_to_thread(11, &again) == 0); /* resumed on the second thread */
    assert(again == 12);
    Fiber *self = NULL;
    assert(fiber_get_self(&self) == 0 && self == &migrant);
    migrant_steps = 2;
    fiber_return_to_thread(13, NULL);
    assert(!"not resumed");
}
static void *run_migrant(void *arg) {
    uint64_t returned = 0;
    uint64_t run = (uint64_t)(uintptr_t)arg;
    assert(fiber_run(&migrant, run, &returned) == 0);
    assert(returned == run + 1);
    return NULL;
}

/* A small guest context in the middle of "heap" memory: host code on the fiber needs far more
 * stack (FFmpeg, drivers). It runs on a host stack; the heap around the context stays intact. */
static Fiber deep;
static _Alignas(16) unsigned char heap[3 * 4096];
static volatile uint64_t deep_sum;
static void ABI entry_deep(uint64_t init, uint64_t run) {
    (void)init; (void)run;
    volatile unsigned char big[200 * 1024];
    for (size_t i = 0; i < sizeof(big); i += 64) big[i] = (unsigned char)i;
    uint64_t sum = 0;
    for (size_t i = 0; i < sizeof(big); i += 64) sum += big[i];
    deep_sum = sum;
    unsigned char *here = (unsigned char *)&sum;
    assert(!(here >= heap && here < heap + sizeof(heap)));
    fiber_return_to_thread(1, NULL);
    assert(!"not resumed");
}
static void test_deep_stack(void) {
    if (!host_stack_extra()) return; /* GOW3_FIBER_STACK_KB=0: the guest context, as on the PS4 */
    memset(heap, 0x5a, sizeof(heap));
    unsigned char *context = heap + 4096;
    assert(fiber_initialize(&deep, "deep", entry_deep, 0, context, 4096, NULL, SDK_350) == 0);
    uint64_t returned = 0;
    assert(fiber_run(&deep, 0, &returned) == 0 && returned == 1 && deep_sum);
    for (size_t i = 0; i < 4096; ++i) assert(heap[i] == 0x5a && heap[2 * 4096 + i] == 0x5a);
    assert(fiber_finalize(&deep) == 0);
    /* Finalized: its host stack goes to the next fiber. */
    pthread_mutex_lock(&host_stack_lock);
    size_t before = host_stack_count;
    pthread_mutex_unlock(&host_stack_lock);
    assert(fiber_initialize(&deep, "deep again", entry_deep, 0, context, 4096, NULL, SDK_350) == 0);
    assert(fiber_run(&deep, 0, &returned) == 0 && returned == 1);
    assert(host_stack_count == before);
}

int main(void) {
    test_deep_stack();
    assert(fiber_initialize(&migrant, "migrant", entry_migrant, 0, stack_m, sizeof(stack_m), NULL, SDK_350) == 0);
    pthread_t first, second;
    pthread_create(&first, NULL, run_migrant, (void *)(uintptr_t)10);
    pthread_join(first, NULL);
    assert(migrant_steps == 1);
    pthread_create(&second, NULL, run_migrant, (void *)(uintptr_t)12);
    pthread_join(second, NULL);
    assert(migrant_steps == 2);

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
