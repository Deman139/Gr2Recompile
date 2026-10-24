/* Host calls from guest code on small guest stacks (Windows).
 *
 * Gravity Rush 2's job system runs on fibers whose stacks (contexts) are small blocks of the
 * game's heap. Guest code is sized for them, but the host code the game reaches through its
 * imports is not: FFmpeg (AvPlayer cutscenes), the Vulkan driver behind the GPU HLE and Windows
 * itself need far more stack than the PS4 system libraries, and on a fiber they wrote over the
 * heap blocks below its context (libc: "A heap error is detected"). The fiber itself must stay
 * on its context: the game recognises its own stack memory (moving fibers to host stacks drew
 * black lines over every frame).
 *
 * So every function import the host serves is called through a stub: when the thread's stack
 * pointer is outside its Windows stack (a fiber context, or any other guest-provided stack), the
 * call moves to a per-thread host stack of GOW3_HOST_STACK_KB (default 1024; 0 = off) with the
 * first 16 stack arguments copied, and the TEB stack bounds describe that stack during the
 * call (exceptions, stack probes). Nested calls (guest callbacks from host code) are already on
 * it and go straight through. Calls on the thread's own stack pay one bounds check.
 *
 * Not wrapped: libSceFiber (it saves and switches the guest stack itself) and pthread exit (it
 * leaves through a longjmp). The GPU page-tracking fault handler moves the same way
 * (runtime_hoststack_run). Linux builds keep the old behaviour. */
#include "runtime.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

typedef struct AltStack {
    uintptr_t top, bottom; /* read by the assembly: top at 0, bottom at 8 */
    struct AltStack *next;
} AltStack;

/* Where the thread's AltStack pointer is: below 0x1780, the gs offset of a TEB TlsSlots entry
 * (0x1480 + 8 * slot, slots 0-63); from 0x1780, 0x1780 + 8 * (slot - 64): an entry of the
 * TlsExpansionSlots array the TEB points to at gs:0x1780 (slots 64-1087; the Vulkan driver and
 * the other DLLs often hold all 64 first ones). 0 = off. */
uint32_t runtime_hoststack_slot __asm__("runtime_hoststack_slot");
static DWORD slot = TLS_OUT_OF_INDEXES;
static size_t stack_size;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static AltStack *free_stacks;
static unsigned stacks_made, stubs_made;
static unsigned char *stub_page;
static size_t stub_used, stub_capacity;

void runtime_hoststack_common(void) __asm__("runtime_hoststack_common");
long runtime_hoststack_run(long (*function)(void *), void *argument) __asm__("runtime_hoststack_run");

/* r11 = the host function (set by the per-import stub). Only r10, r11, xmm14 and xmm15 are
 * used before the call: they carry no arguments in either ABI's guest-facing convention (SysV),
 * rax keeps the vector-register count of variadic calls. TEB: StackBase gs:0x08, StackLimit
 * gs:0x10, DeallocationStack gs:0x1478.
 * Host stack frame below top T: T-8 guest rsp, T-16 StackBase, T-24 StackLimit,
 * T-32 DeallocationStack, [T-160, T-32) the copied stack arguments; the call is made at T-160. */
__asm__(
    ".text\n"
    ".globl runtime_hoststack_common\n"
    "runtime_hoststack_common:\n"
    "    cmp %gs:0x08, %rsp\n"
    "    jae 1f\n"
    "    cmp %gs:0x1478, %rsp\n"
    "    jae 2f\n"                          /* on the thread's own stack */
    "1:\n"
    "    movl runtime_hoststack_slot(%rip), %r10d\n"
    "    testl %r10d, %r10d\n"
    "    jz 2f\n"
    "    cmpl $0x1780, %r10d\n"
    "    jae 4f\n"
    "    movq %gs:(%r10), %r10\n"
    "    jmp 5f\n"
    "4:\n"                                 /* an expansion slot (r11 is kept in xmm15) */
    "    movq %r11, %xmm15\n"
    "    movq %gs:0x1780, %r11\n"
    "    testq %r11, %r11\n"
    "    jz 6f\n"
    "    subl $0x1780, %r10d\n"
    "    movq (%r11,%r10), %r10\n"
    "    movq %xmm15, %r11\n"
    "5:\n"
    "    testq %r10, %r10\n"
    "    jz 2f\n"                           /* a thread without a host stack */
    "    movq 8(%r10), %xmm14\n"
    "    movq (%r10), %r10\n"
    "    movq %rsp, -8(%r10)\n"
    "    movq %gs:0x08, %xmm15\n"
    "    movq %xmm15, -16(%r10)\n"
    "    movq %gs:0x10, %xmm15\n"
    "    movq %xmm15, -24(%r10)\n"
    "    movq %gs:0x1478, %xmm15\n"
    "    movq %xmm15, -32(%r10)\n"
    "    movdqu 8(%rsp), %xmm15\n"
    "    movdqu %xmm15, -160(%r10)\n"
    "    movdqu 24(%rsp), %xmm15\n"
    "    movdqu %xmm15, -144(%r10)\n"
    "    movdqu 40(%rsp), %xmm15\n"
    "    movdqu %xmm15, -128(%r10)\n"
    "    movdqu 56(%rsp), %xmm15\n"
    "    movdqu %xmm15, -112(%r10)\n"
    "    movdqu 72(%rsp), %xmm15\n"
    "    movdqu %xmm15, -96(%r10)\n"
    "    movdqu 88(%rsp), %xmm15\n"
    "    movdqu %xmm15, -80(%r10)\n"
    "    movdqu 104(%rsp), %xmm15\n"
    "    movdqu %xmm15, -64(%r10)\n"
    "    movdqu 120(%rsp), %xmm15\n"
    "    movdqu %xmm15, -48(%r10)\n"
    "    movq %r10, %gs:0x08\n"
    "    movq %xmm14, %gs:0x10\n"
    "    movq %xmm14, %gs:0x1478\n"
    "    leaq -160(%r10), %rsp\n"
    "    call *%r11\n"
    "    movq 128(%rsp), %xmm15\n"
    "    movq %xmm15, %gs:0x1478\n"
    "    movq 136(%rsp), %xmm15\n"
    "    movq %xmm15, %gs:0x10\n"
    "    movq 144(%rsp), %xmm15\n"
    "    movq %xmm15, %gs:0x08\n"
    "    movq 152(%rsp), %rsp\n"
    "    ret\n"
    "6:\n"
    "    movq %xmm15, %r11\n"
    "2:\n"
    "    jmp *%r11\n"
    /* Microsoft ABI: long runtime_hoststack_run(long (*function)(void *), void *argument). */
    ".globl runtime_hoststack_run\n"
    "runtime_hoststack_run:\n"
    "    cmp %gs:0x08, %rsp\n"
    "    jae 1f\n"
    "    cmp %gs:0x1478, %rsp\n"
    "    jae 3f\n"
    "1:\n"
    "    movl runtime_hoststack_slot(%rip), %eax\n"
    "    testl %eax, %eax\n"
    "    jz 3f\n"
    "    cmpl $0x1780, %eax\n"
    "    jae 4f\n"
    "    movq %gs:(%rax), %rax\n"
    "    jmp 5f\n"
    "4:\n"
    "    movq %gs:0x1780, %r10\n"
    "    testq %r10, %r10\n"
    "    jz 3f\n"
    "    subl $0x1780, %eax\n"
    "    movq (%r10,%rax), %rax\n"
    "5:\n"
    "    testq %rax, %rax\n"
    "    jz 3f\n"
    "    pushq %rbx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    movq %rsp, %rbx\n"
    "    movq %gs:0x08, %rsi\n"
    "    movq %gs:0x10, %rdi\n"
    "    movq %gs:0x1478, %rbp\n"
    "    movq 8(%rax), %r8\n"
    "    movq (%rax), %r9\n"
    "    movq %r9, %gs:0x08\n"
    "    movq %r8, %gs:0x10\n"
    "    movq %r8, %gs:0x1478\n"
    "    leaq -64(%r9), %rsp\n"
    "    movq %rcx, %rax\n"
    "    movq %rdx, %rcx\n"
    "    call *%rax\n"
    "    movq %rsi, %gs:0x08\n"
    "    movq %rdi, %gs:0x10\n"
    "    movq %rbp, %gs:0x1478\n"
    "    movq %rbx, %rsp\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rbx\n"
    "    ret\n"
    "3:\n"
    "    movq %rcx, %rax\n"
    "    movq %rdx, %rcx\n"
    "    jmp *%rax\n");

void runtime_hoststack_init(void) {
    if (slot != TLS_OUT_OF_INDEXES || stack_size) return;
    const char *env = getenv("GOW3_HOST_STACK_KB");
    unsigned long long kb = env ? strtoull(env, NULL, 0) : 1024;
    if (!kb) {
        printf("Runtime: host calls run on the caller's stack (GOW3_HOST_STACK_KB=0)\n");
        stack_size = 1; /* initialized, off */
        return;
    }
    if (kb < 64) kb = 64;
    slot = TlsAlloc();
    if (slot == TLS_OUT_OF_INDEXES || slot >= 64 + 1024) {
        if (slot != TLS_OUT_OF_INDEXES) TlsFree(slot);
        slot = TLS_OUT_OF_INDEXES;
        stack_size = 1;
        fprintf(stderr, "Runtime: no TLS slot: host calls stay on guest stacks\n");
        return;
    }
    stack_size = (size_t)kb * 1024;
    runtime_hoststack_slot = slot < 64 ? 0x1480 + 8 * (uint32_t)slot : 0x1780 + 8 * ((uint32_t)slot - 64);
    printf("Runtime: host calls from guest stacks run on %llu KiB host stacks (TLS slot %lu)\n", kb, slot);
}

void runtime_hoststack_attach(void) {
    if (!runtime_hoststack_slot || TlsGetValue(slot)) return;
    pthread_mutex_lock(&lock);
    AltStack *s = free_stacks;
    if (s) free_stacks = s->next;
    pthread_mutex_unlock(&lock);
    if (!s) {
        /* Below 1 TiB like guest stacks (guest callbacks may run on it); runtime_low_map leaves
         * an unmapped gap under each block, so an overrun faults. */
        unsigned char *base = runtime_low_map(stack_size, 3 /* PROT_READ | PROT_WRITE */);
        s = base ? malloc(sizeof(*s)) : NULL;
        if (!s) {
            fprintf(stderr, "Runtime: no memory for a host call stack\n");
            return;
        }
        s->bottom = (uintptr_t)base;
        s->top = (uintptr_t)base + stack_size;
        pthread_mutex_lock(&lock);
        ++stacks_made;
        pthread_mutex_unlock(&lock);
    }
    s->next = NULL;
    TlsSetValue(slot, s);
}

void runtime_hoststack_detach(void) {
    if (!runtime_hoststack_slot) return;
    AltStack *s = TlsGetValue(slot);
    if (!s) return;
    TlsSetValue(slot, NULL);
    pthread_mutex_lock(&lock);
    s->next = free_stacks;
    free_stacks = s;
    pthread_mutex_unlock(&lock);
}

static int excluded(const char *symbol) {
    if (!symbol) return 0;
    return !strncmp(symbol, "sceFiber", 8) || !strncmp(symbol, "_sceFiber", 9) ||
           !strcmp(symbol, "scePthreadExit") || !strcmp(symbol, "pthread_exit");
}

uintptr_t runtime_hoststack_wrap(uintptr_t function, const char *nid) {
    if (!runtime_hoststack_slot || !function || excluded(runtime_import_name(nid))) return function;
    enum { STUB = 24 };
    pthread_mutex_lock(&lock);
    if (stub_used + STUB > stub_capacity) {
        stub_capacity = 64 * 1024;
        stub_page = VirtualAlloc(NULL, stub_capacity, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        stub_used = 0;
        if (!stub_page) {
            stub_capacity = 0;
            pthread_mutex_unlock(&lock);
            return function;
        }
    }
    unsigned char *stub = stub_page + stub_used;
    stub_used += STUB;
    ++stubs_made;
    pthread_mutex_unlock(&lock);
    /* movabs r11, function; jmp [rip+0]; .quad runtime_hoststack_common */
    uintptr_t common = (uintptr_t)runtime_hoststack_common;
    stub[0] = 0x49; stub[1] = 0xbb; memcpy(stub + 2, &function, 8);
    stub[10] = 0xff; stub[11] = 0x25; memset(stub + 12, 0, 4);
    memcpy(stub + 16, &common, 8);
    FlushInstructionCache(GetCurrentProcess(), stub, STUB);
    return (uintptr_t)stub;
}

void runtime_hoststack_report(void) {
    if (!runtime_hoststack_slot) return;
    pthread_mutex_lock(&lock);
    printf("Runtime: host call stacks=%u (%zu KiB each), wrapped imports=%u\n", stacks_made, stack_size / 1024,
           stubs_made);
    pthread_mutex_unlock(&lock);
}

#else /* Linux: guest code runs on host-sized stacks there; nothing moves. */

void runtime_hoststack_init(void) {}
void runtime_hoststack_attach(void) {}
void runtime_hoststack_detach(void) {}
uintptr_t runtime_hoststack_wrap(uintptr_t function, const char *nid) {
    (void)nid;
    return function;
}
long runtime_hoststack_run(long (*function)(void *), void *argument) { return function(argument); }
void runtime_hoststack_report(void) {}

#endif
