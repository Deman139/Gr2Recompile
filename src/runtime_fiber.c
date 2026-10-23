/* libSceFiber: cooperative fibers on the calling guest thread (Gravity Rush 2's job system).
 * Ported from shadPS4's implementation (src/core/libraries/fiber, GPL-2.0-or-later, as used by
 * GR2fork): the same context layout, setjmp/longjmp and entry trampoline, with the fiber
 * context of the current thread kept in host TLS instead of the shadPS4 TCB. Everything runs
 * on the thread that called sceFiberRun; no host threads are created.
 *
 * A fiber with a context does not run on the guest's context memory but on a host stack that is
 * larger by GOW3_FIBER_STACK_KB (default 512 KiB): host functions the fiber calls (FFmpeg for
 * the AvPlayer cutscenes, the Vulkan driver, Windows) need far more stack than the PS4 system
 * libraries, and on the game's small heap-allocated contexts they wrote over the game's heap
 * (the "A heap error is detected" crashes at the first save and at cutscenes). The host stacks
 * come from runtime_low_map (below 1 TiB, an unmapped gap under each one catches an overrun) and
 * are pooled: sceFiberFinalize or a new sceFiberInitialize of the same OrbisFiber returns one.
 * GOW3_FIBER_STACK_KB=0 runs fibers on the guest's context memory as on the PS4. */
#include "runtime.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIBER_ERROR_NULL ((int32_t)0x80590001)
#define FIBER_ERROR_ALIGNMENT ((int32_t)0x80590002)
#define FIBER_ERROR_RANGE ((int32_t)0x80590003)
#define FIBER_ERROR_INVALID ((int32_t)0x80590004)
#define FIBER_ERROR_PERMISSION ((int32_t)0x80590005)
#define FIBER_ERROR_STATE ((int32_t)0x80590006)

#define FIBER_MAX_NAME 31
#define FIBER_CONTEXT_MINIMUM 512
enum { STATE_RUN = 1, STATE_IDLE = 2, STATE_TERMINATED = 3 };
enum { FLAG_CONTEXT_SIZE_CHECK = 0x10, FLAG_SET_FPU_REGS = 0x100 };
#define SIGNATURE0 UINT32_C(0xdef1649c)
#define SIGNATURE1 UINT32_C(0xb37592a0)
#define OPT_SIGNATURE UINT32_C(0xbb40e64d)
#define STACK_SIGNATURE UINT64_C(0x7149f2ca7149f2ca)
#define STACK_SIZE_CHECK UINT64_C(0xdeadbeefdeadbeef)
#define SDK_350 UINT32_C(0x03500000)

typedef void (ABI *FiberEntry)(uint64_t arg_on_initialize, uint64_t arg_on_run);
typedef struct Fiber Fiber;
/* Offsets are used by the assembly below: registers 0x00-0x68, fpucw 0x70, mxcsr 0x72. */
typedef struct __attribute__((packed, aligned(16))) {
    uint64_t rax, rcx, rdx, rbx, rsp, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint16_t fpucw;
    uint32_t mxcsr;
    uint16_t pad;
    Fiber *current_fiber, *prev_fiber;
    uint64_t arg_on_run_to, arg_on_return, return_val;
} FiberContext;
_Static_assert(__builtin_offsetof(FiberContext, fpucw) == 0x70, "fiber context layout");
_Static_assert(__builtin_offsetof(FiberContext, mxcsr) == 0x72, "fiber context layout");
/* Entry trampoline data: entry 0x00, arg_on_initialize 0x08, arg_on_run_to 0x10, stack 0x18,
 * state 0x20, fpucw 0x28, mxcsr 0x2c. */
typedef struct {
    FiberEntry entry;
    uint64_t arg_on_initialize, arg_on_run_to;
    void *stack_addr;
    _Atomic uint32_t *state;
    uint16_t fpucw;
    int8_t pad[2];
    uint32_t mxcsr;
} FiberData;
_Static_assert(__builtin_offsetof(FiberData, mxcsr) == 0x2c, "fiber data layout");
/* The guest's OrbisFiber (the game allocates it; at most 256 bytes). */
struct Fiber {
    uint32_t magic_start;
    _Atomic uint32_t state;
    FiberEntry entry;
    uint64_t arg_on_initialize;
    void *addr_context;
    uint64_t size_context;
    char name[FIBER_MAX_NAME + 1];
    FiberContext *context;
    uint32_t flags;
    void *context_start, *context_end;
    uint32_t magic_end;
};
_Static_assert(sizeof(Fiber) <= 256, "OrbisFiber size");
typedef struct {
    uint64_t size;
    FiberEntry entry;
    uint64_t arg_on_initialize;
    void *addr_context;
    uint64_t size_context;
    char name[FIBER_MAX_NAME + 1];
    uint64_t size_context_margin;
    uint8_t pad[48];
} FiberInfo;
_Static_assert(sizeof(FiberInfo) == 128, "OrbisFiberInfo size");

/* ---- host stacks for fibers ---- */
typedef struct {
    unsigned char *base;
    uint64_t size;
    const Fiber *owner;
} HostStack;
static pthread_mutex_t host_stack_lock = PTHREAD_MUTEX_INITIALIZER;
static HostStack *host_stacks;
static size_t host_stack_count, host_stack_capacity;
static uint64_t host_stack_bytes;

static uint64_t host_stack_extra(void) {
    static _Atomic int64_t extra = -1;
    int64_t value = atomic_load(&extra);
    if (value < 0) {
        const char *env = getenv("GOW3_FIBER_STACK_KB");
        value = env ? (int64_t)strtoull(env, NULL, 0) * 1024 : 512 * 1024;
        atomic_store(&extra, value);
        if (value) printf("Runtime: fibers run on host stacks %lld KiB larger than their contexts\n",
                          (long long)(value / 1024));
        else printf("Runtime: fibers run on their guest contexts (GOW3_FIBER_STACK_KB=0)\n");
    }
    return (uint64_t)value;
}

/* The stack of `fiber` (one it already holds, a free pooled one large enough, or a new one).
 * NULL: no host stack (switched off or out of memory); the guest context is used. */
static HostStack *host_stack_acquire(const Fiber *fiber) {
    uint64_t extra = host_stack_extra();
    if (!extra) return NULL;
    uint64_t need = (fiber->size_context + extra + 0xffff) & ~(uint64_t)0xffff;
    HostStack *found = NULL, *spare = NULL;
    pthread_mutex_lock(&host_stack_lock);
    for (size_t i = 0; i < host_stack_count && !found; ++i) {
        HostStack *s = &host_stacks[i];
        if (s->owner == fiber) {
            if (s->size >= need) found = s;
            else s->owner = NULL; /* the fiber got a larger context */
        } else if (!s->owner && s->size >= need && (!spare || s->size < spare->size)) {
            spare = s;
        }
    }
    if (!found) found = spare;
    if (!found) {
        unsigned char *base = runtime_low_map(need, 3 /* PROT_READ | PROT_WRITE */);
        if (base && host_stack_count == host_stack_capacity) {
            size_t capacity = host_stack_capacity ? host_stack_capacity * 2 : 64;
            HostStack *grown = realloc(host_stacks, capacity * sizeof(*grown));
            if (grown) { host_stacks = grown; host_stack_capacity = capacity; }
        }
        if (base && host_stack_count < host_stack_capacity) {
            found = &host_stacks[host_stack_count++];
            found->base = base;
            found->size = need;
            host_stack_bytes += need;
        } else if (!base) {
            static int warned;
            if (!warned++) fprintf(stderr, "Runtime: no memory for a fiber host stack; using the guest context\n");
        }
    }
    if (found) found->owner = fiber;
    pthread_mutex_unlock(&host_stack_lock);
    return found;
}

static void host_stack_release(const Fiber *fiber) {
    pthread_mutex_lock(&host_stack_lock);
    for (size_t i = 0; i < host_stack_count; ++i)
        if (host_stacks[i].owner == fiber) host_stacks[i].owner = NULL;
    pthread_mutex_unlock(&host_stack_lock);
}

static HostStack *host_stack_of(const Fiber *fiber) {
    HostStack *found = NULL;
    pthread_mutex_lock(&host_stack_lock);
    for (size_t i = 0; i < host_stack_count && !found; ++i)
        if (host_stacks[i].owner == fiber) found = &host_stacks[i];
    pthread_mutex_unlock(&host_stack_lock);
    return found;
}

static __thread FiberContext *thread_fiber;
static atomic_uint context_size_check;
static atomic_ullong fibers_initialized, fiber_runs, fiber_switches;

int32_t ABI __attribute__((returns_twice)) runtime_fiber_setjmp(FiberContext *ctx) __asm__("runtime_fiber_setjmp");
void ABI runtime_fiber_longjmp(FiberContext *ctx) __asm__("runtime_fiber_longjmp");
void ABI runtime_fiber_entry(FiberData *data, int set_fpu) __asm__("runtime_fiber_entry");
void ABI runtime_fiber_force_quit(uint64_t value) __asm__("runtime_fiber_force_quit");

__asm__(
    ".text\n"
    ".globl runtime_fiber_setjmp\n"
    "runtime_fiber_setjmp:\n"
    "    movq %rax, 0x00(%rdi)\n"
    "    movq (%rsp), %rdx\n"           /* the return address, resumed by longjmp */
    "    movq %rdx, 0x10(%rdi)\n"
    "    movq %rcx, 0x08(%rdi)\n"
    "    movq %rbx, 0x18(%rdi)\n"
    "    movq %rsp, 0x20(%rdi)\n"
    "    movq %rbp, 0x28(%rdi)\n"
    "    movq %r8,  0x30(%rdi)\n"
    "    movq %r9,  0x38(%rdi)\n"
    "    movq %r10, 0x40(%rdi)\n"
    "    movq %r11, 0x48(%rdi)\n"
    "    movq %r12, 0x50(%rdi)\n"
    "    movq %r13, 0x58(%rdi)\n"
    "    movq %r14, 0x60(%rdi)\n"
    "    movq %r15, 0x68(%rdi)\n"
    "    fnstcw  0x70(%rdi)\n"
    "    stmxcsr 0x72(%rdi)\n"
    "    xorl %eax, %eax\n"
    "    ret\n"
    ".globl runtime_fiber_longjmp\n"
    "runtime_fiber_longjmp:\n"
    /* MXCSR = (current & 0x3f status flags) ^ (saved & ~0x3f) */
    "    stmxcsr -0x4(%rsp)\n"
    "    movl 0x72(%rdi), %eax\n"
    "    andl $0xffffffc0, %eax\n"
    "    movl -0x4(%rsp), %ecx\n"
    "    andl $0x3f, %ecx\n"
    "    xorl %eax, %ecx\n"
    "    movl %ecx, -0x4(%rsp)\n"
    "    ldmxcsr -0x4(%rsp)\n"
    "    movq 0x00(%rdi), %rax\n"
    "    movq 0x08(%rdi), %rcx\n"
    "    movq 0x10(%rdi), %rdx\n"
    "    movq 0x18(%rdi), %rbx\n"
    "    movq 0x20(%rdi), %rsp\n"
    "    movq 0x28(%rdi), %rbp\n"
    "    movq 0x30(%rdi), %r8\n"
    "    movq 0x38(%rdi), %r9\n"
    "    movq 0x40(%rdi), %r10\n"
    "    movq 0x48(%rdi), %r11\n"
    "    movq 0x50(%rdi), %r12\n"
    "    movq 0x58(%rdi), %r13\n"
    "    movq 0x60(%rdi), %r14\n"
    "    movq 0x68(%rdi), %r15\n"
    "    fldcw 0x70(%rdi)\n"
    "    movq %rdx, 0x00(%rsp)\n"       /* return into setjmp's caller with eax = 1 */
    "    movl $1, %eax\n"
    "    ret\n"
    ".globl runtime_fiber_entry\n"
    "runtime_fiber_entry:\n"
    "    movq %rdi, %r11\n"
    "    movq 0x18(%r11), %rsp\n"
    "    xorl %ebp, %ebp\n"
    "    movq 0x20(%r11), %r10\n"
    "    testq %r10, %r10\n"
    "    jz 1f\n"
    "    movl $2, (%r10)\n"             /* the fiber switched away from is idle now */
    "1:\n"
    "    testl %esi, %esi\n"
    "    jz 2f\n"
    "    ldmxcsr 0x2c(%r11)\n"
    "    fldcw 0x28(%r11)\n"
    "2:\n"
    "    movq 0x08(%r11), %rdi\n"
    "    movq 0x10(%r11), %rsi\n"
    "    movq 0x00(%r11), %r11\n"
    "    xorl %eax, %eax\n"
    "    xorl %ebx, %ebx\n"
    "    xorl %ecx, %ecx\n"
    "    xorl %edx, %edx\n"
    "    xorq %r8, %r8\n"
    "    xorq %r9, %r9\n"
    "    xorq %r10, %r10\n"
    "    xorq %r12, %r12\n"
    "    xorq %r13, %r13\n"
    "    xorq %r14, %r14\n"
    "    xorq %r15, %r15\n"
    "    vzeroall\n"
    "    call *%r11\n"
    "    movl $1, %edi\n"               /* a fiber entry must never return */
    "    call runtime_fiber_force_quit\n"
    "    ud2\n");

/* A fiber that left one thread can be resumed on another (Gravity Rush 2's worker threads share
 * their fibers), so after any setjmp return the thread's TLS must be read again: never inlined,
 * and the barrier keeps the compiler from reusing a TLS address computed on the old thread. */
static __attribute__((noinline)) FiberContext *current_context(void) {
    __asm__ volatile("" ::: "memory");
    return thread_fiber;
}
static __attribute__((noinline)) void set_current_context(FiberContext *ctx) {
    __asm__ volatile("" ::: "memory");
    thread_fiber = ctx;
}

void ABI __attribute__((used)) runtime_fiber_force_quit(uint64_t value) {
    FiberContext *ctx = current_context();
    ctx->return_val = value;
    runtime_fiber_longjmp(ctx);
    __builtin_trap();
}

/* Copies at most FIBER_MAX_NAME characters and always terminates (dst holds FIBER_MAX_NAME + 1). */
static void copy_name(char *dst, const char *src) {
    size_t n = 0;
    while (n < FIBER_MAX_NAME && src[n]) ++n;
    memcpy(dst, src, n);
    memset(dst + n, 0, FIBER_MAX_NAME + 1 - n);
}

static int valid(const Fiber *fiber) {
    return fiber->magic_start == SIGNATURE0 && fiber->magic_end == SIGNATURE1;
}

static void check_stack_overflow(FiberContext *ctx) {
    const HostStack *host = host_stack_extra() ? host_stack_of(ctx->current_fiber) : NULL;
    const uint64_t *base = host ? (const uint64_t *)host->base : ctx->current_fiber->addr_context;
    if (base && *base != STACK_SIGNATURE) {
        fprintf(stderr, "STOP: fiber '%s' overflowed its context (0x%llx bytes)\n",
                ctx->current_fiber->name, (unsigned long long)ctx->current_fiber->size_context);
        fflush(NULL);
        abort();
    }
}

static void fill_stack(Fiber *fiber) {
    *(uint64_t *)fiber->context_start = STACK_SIGNATURE;
    if (fiber->flags & FLAG_CONTEXT_SIZE_CHECK)
        for (uint64_t *p = (uint64_t *)fiber->context_start + 1; p < (uint64_t *)fiber->context_end; ++p)
            *p = STACK_SIZE_CHECK;
}

static int32_t attach_context(Fiber *fiber, void *addr, uint64_t size) {
    if (size && size < FIBER_CONTEXT_MINIMUM) return FIBER_ERROR_RANGE;
    if (size & 15) return FIBER_ERROR_INVALID;
    if (!addr || !size) return FIBER_ERROR_INVALID;
    if (fiber->addr_context) return FIBER_ERROR_INVALID;
    fiber->addr_context = addr;
    fiber->size_context = size;
    fiber->context_start = addr;
    fiber->context_end = (unsigned char *)addr + size;
    fill_stack(fiber);
    return 0;
}

static __attribute__((noreturn)) void enter(Fiber *fiber, uint64_t arg_on_run_to, void *stack,
                                            _Atomic uint32_t *state) {
    FiberData data = {0};
    data.entry = fiber->entry;
    data.arg_on_initialize = fiber->arg_on_initialize;
    data.arg_on_run_to = arg_on_run_to;
    data.stack_addr = stack;
    data.state = state;
    int fpu = (fiber->flags & FLAG_SET_FPU_REGS) != 0;
    if (fpu) { data.fpucw = 0x037f; data.mxcsr = 0x9fc0; }
    runtime_fiber_entry(&data, fpu);
    __builtin_trap();
}

static __attribute__((noreturn)) void switch_to_fiber(Fiber *fiber, uint64_t arg_on_run_to, FiberContext *ctx) {
    FiberContext *fiber_ctx = fiber->context;
    if (fiber_ctx) {
        ctx->arg_on_run_to = arg_on_run_to;
        runtime_fiber_longjmp(fiber_ctx);
        __builtin_trap();
    }
    _Atomic uint32_t *state = NULL;
    if (ctx->prev_fiber) {
        state = &ctx->prev_fiber->state;
        ctx->prev_fiber = NULL;
    }
    unsigned char *top = (unsigned char *)fiber->addr_context + fiber->size_context;
    HostStack *host = host_stack_acquire(fiber);
    if (host) {
        /* The fiber starts afresh: mark the stack as fill_stack marks the guest context. */
        uint64_t *p = (uint64_t *)host->base, *end = (uint64_t *)(host->base + host->size);
        *p++ = STACK_SIGNATURE;
        if (fiber->flags & FLAG_CONTEXT_SIZE_CHECK)
            while (p < end) *p++ = STACK_SIZE_CHECK;
        top = host->base + host->size;
    }
    enter(fiber, arg_on_run_to, top, state);
}

static __attribute__((noreturn)) void do_switch(Fiber *cur, Fiber *fiber, uint64_t arg_on_run_to, FiberContext *ctx) {
    ctx->prev_fiber = cur;
    ctx->current_fiber = fiber;
    if (!fiber->addr_context) {
        /* A fiber without its own context runs on the thread's stack where sceFiberRun left it. */
        ctx->prev_fiber = NULL;
        enter(fiber, arg_on_run_to, (void *)(ctx->rsp & ~(uint64_t)15), &cur->state);
    }
    switch_to_fiber(fiber, arg_on_run_to, ctx);
}

static ABI int32_t fiber_initialize_impl(Fiber *fiber, const char *name, FiberEntry entry, uint64_t arg_on_initialize,
                                         void *addr_context, uint64_t size_context, const uint32_t *opt_param,
                                         uint32_t flags, uint32_t build_version) {
    if (!fiber || !name || !entry) return FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)addr_context & 15)) return FIBER_ERROR_ALIGNMENT;
    if (opt_param && ((uintptr_t)opt_param & 7)) return FIBER_ERROR_ALIGNMENT;
    if (size_context && size_context < FIBER_CONTEXT_MINIMUM) return FIBER_ERROR_RANGE;
    if (size_context & 15) return FIBER_ERROR_INVALID;
    if (!addr_context != !size_context) return FIBER_ERROR_INVALID;
    if (opt_param && *opt_param != OPT_SIGNATURE) return FIBER_ERROR_INVALID;
    uint32_t user_flags = flags;
    if (build_version >= SDK_350) user_flags |= FLAG_SET_FPU_REGS;
    if (atomic_load(&context_size_check)) user_flags |= FLAG_CONTEXT_SIZE_CHECK;
    host_stack_release(fiber);
    copy_name(fiber->name, name);
    fiber->entry = entry;
    fiber->arg_on_initialize = arg_on_initialize;
    fiber->addr_context = addr_context;
    fiber->size_context = size_context;
    fiber->context = NULL;
    fiber->flags = user_flags;
    fiber->magic_start = SIGNATURE0;
    fiber->magic_end = SIGNATURE1;
    if (addr_context) {
        fiber->context_start = addr_context;
        fiber->context_end = (unsigned char *)addr_context + size_context;
        fill_stack(fiber);
    } else {
        fiber->context_start = fiber->context_end = NULL;
    }
    atomic_store(&fiber->state, STATE_IDLE);
    atomic_fetch_add(&fibers_initialized, 1);
    return 0;
}

static ABI int32_t fiber_initialize(Fiber *fiber, const char *name, FiberEntry entry, uint64_t arg_on_initialize,
                                    void *addr_context, uint64_t size_context, const uint32_t *opt_param,
                                    uint32_t build_version) {
    return fiber_initialize_impl(fiber, name, entry, arg_on_initialize, addr_context, size_context, opt_param, 0,
                                 build_version);
}

static ABI int32_t fiber_opt_param_initialize(uint32_t *opt_param) {
    if (!opt_param) return FIBER_ERROR_NULL;
    if ((uintptr_t)opt_param & 7) return FIBER_ERROR_ALIGNMENT;
    *opt_param = OPT_SIGNATURE;
    return 0;
}

static ABI int32_t fiber_finalize(Fiber *fiber) {
    if (!fiber) return FIBER_ERROR_NULL;
    if ((uintptr_t)fiber & 7) return FIBER_ERROR_ALIGNMENT;
    if (!valid(fiber)) return FIBER_ERROR_INVALID;
    uint32_t expected = STATE_IDLE;
    if (!atomic_compare_exchange_strong(&fiber->state, &expected, STATE_TERMINATED)) return FIBER_ERROR_STATE;
    host_stack_release(fiber);
    return 0;
}

static ABI int32_t fiber_run_impl(Fiber *fiber, void *addr_context, uint64_t size_context, uint64_t arg_on_run_to,
                                  uint64_t *arg_on_return) {
    if (!fiber) return FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)addr_context & 15)) return FIBER_ERROR_ALIGNMENT;
    if (!valid(fiber)) return FIBER_ERROR_INVALID;
    if (current_context()) return FIBER_ERROR_PERMISSION;
    if (addr_context || size_context) {
        int32_t result = attach_context(fiber, addr_context, size_context);
        if (result < 0) return result;
    }
    uint32_t expected = STATE_IDLE;
    if (!atomic_compare_exchange_strong(&fiber->state, &expected, STATE_RUN)) return FIBER_ERROR_STATE;
    atomic_fetch_add(&fiber_runs, 1);
    /* volatile: the context is written again by longjmp paths the compiler cannot see. */
    FiberContext ctx_storage;
    FiberContext *volatile ctx = &ctx_storage;
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_fiber = fiber;
    set_current_context(ctx);
    if (!runtime_fiber_setjmp(ctx)) {
        if (fiber->addr_context) switch_to_fiber(fiber, arg_on_run_to, ctx);
        enter(fiber, arg_on_run_to, (void *)(ctx->rsp & ~(uint64_t)15), NULL);
    }
    ctx = current_context();
    Fiber *cur = ctx->current_fiber;
    ctx->current_fiber = NULL;
    atomic_store(&cur->state, STATE_IDLE);
    if (ctx->return_val) {
        fprintf(stderr, "STOP: the entry function of fiber '%s' returned\n", cur->name);
        fflush(NULL);
        abort();
    }
    if (arg_on_return) *arg_on_return = ctx->arg_on_return;
    set_current_context(NULL);
    return 0;
}

static ABI int32_t fiber_switch_impl(Fiber *fiber, void *addr_context, uint64_t size_context, uint64_t arg_on_run_to,
                                     uint64_t *arg_on_run) {
    if (!fiber) return FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)addr_context & 15)) return FIBER_ERROR_ALIGNMENT;
    if (!valid(fiber)) return FIBER_ERROR_INVALID;
    FiberContext *g_ctx = current_context();
    if (!g_ctx) return FIBER_ERROR_PERMISSION;
    if (addr_context || size_context) {
        int32_t result = attach_context(fiber, addr_context, size_context);
        if (result < 0) return result;
    }
    uint32_t expected = STATE_IDLE;
    if (!atomic_compare_exchange_strong(&fiber->state, &expected, STATE_RUN)) return FIBER_ERROR_STATE;
    atomic_fetch_add(&fiber_switches, 1);
    Fiber *cur = g_ctx->current_fiber;
    if (!cur->addr_context) do_switch(cur, fiber, arg_on_run_to, g_ctx);
    FiberContext ctx_storage;
    FiberContext *volatile ctx = &ctx_storage;
    memset(ctx, 0, sizeof(*ctx));
    if (!runtime_fiber_setjmp(ctx)) {
        cur->context = ctx;
        check_stack_overflow(g_ctx);
        do_switch(cur, fiber, arg_on_run_to, g_ctx);
    }
    g_ctx = current_context();
    if (g_ctx->prev_fiber) {
        atomic_store(&g_ctx->prev_fiber->state, STATE_IDLE);
        g_ctx->prev_fiber = NULL;
    }
    if (arg_on_run) *arg_on_run = g_ctx->arg_on_run_to;
    return 0;
}

static ABI int32_t fiber_run(Fiber *fiber, uint64_t arg_on_run_to, uint64_t *arg_on_return) {
    return fiber_run_impl(fiber, NULL, 0, arg_on_run_to, arg_on_return);
}

static ABI int32_t fiber_switch(Fiber *fiber, uint64_t arg_on_run_to, uint64_t *arg_on_run) {
    return fiber_switch_impl(fiber, NULL, 0, arg_on_run_to, arg_on_run);
}

static ABI int32_t fiber_get_self(Fiber **fiber) {
    if (!fiber) return FIBER_ERROR_NULL;
    FiberContext *ctx = current_context();
    if (!ctx) return FIBER_ERROR_PERMISSION;
    *fiber = ctx->current_fiber;
    return 0;
}

static ABI int32_t fiber_return_to_thread(uint64_t arg_on_return, uint64_t *arg_on_run) {
    FiberContext *g_ctx = current_context();
    if (!g_ctx) return FIBER_ERROR_PERMISSION;
    Fiber *cur = g_ctx->current_fiber;
    if (cur->addr_context) {
        FiberContext ctx_storage;
        FiberContext *volatile ctx = &ctx_storage;
        memset(ctx, 0, sizeof(*ctx));
        if (runtime_fiber_setjmp(ctx)) {
            /* Resumed later by sceFiberRun or sceFiberSwitch. */
            g_ctx = current_context();
            if (g_ctx->prev_fiber) {
                atomic_store(&g_ctx->prev_fiber->state, STATE_IDLE);
                g_ctx->prev_fiber = NULL;
            }
            if (arg_on_run) *arg_on_run = g_ctx->arg_on_run_to;
            return 0;
        }
        cur->context = ctx;
        check_stack_overflow(g_ctx);
    }
    g_ctx->arg_on_return = arg_on_return;
    runtime_fiber_longjmp(g_ctx);
    __builtin_trap();
}

static ABI int32_t fiber_get_info(Fiber *fiber, FiberInfo *info) {
    if (!fiber || !info) return FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)info & 7)) return FIBER_ERROR_ALIGNMENT;
    if (info->size != sizeof(FiberInfo)) return FIBER_ERROR_INVALID;
    if (!valid(fiber)) return FIBER_ERROR_INVALID;
    info->entry = fiber->entry;
    info->arg_on_initialize = fiber->arg_on_initialize;
    info->addr_context = fiber->addr_context;
    info->size_context = fiber->size_context;
    copy_name(info->name, fiber->name);
    info->size_context_margin = UINT64_MAX;
    if ((fiber->flags & FLAG_CONTEXT_SIZE_CHECK) && fiber->addr_context) {
        const HostStack *host = host_stack_extra() ? host_stack_of(fiber) : NULL;
        const uint64_t *start = fiber->context_start, *end = fiber->context_end, *p;
        /* On a host stack: the part of it larger than the guest context counts as used. */
        uint64_t larger = 0;
        if (host) {
            start = (const uint64_t *)host->base;
            end = (const uint64_t *)(host->base + host->size);
            larger = host->size - fiber->size_context;
        }
        p = start + 1;
        uint64_t margin = 0;
        if (*start == STACK_SIGNATURE) {
            while (p < end && *p == STACK_SIZE_CHECK) ++p;
            margin = (uint64_t)((uintptr_t)p - (uintptr_t)(start + 1));
        }
        info->size_context_margin = margin > larger ? margin - larger : 0;
    }
    return 0;
}

static ABI int32_t fiber_start_context_size_check(uint32_t flags) {
    if (flags) return FIBER_ERROR_INVALID;
    unsigned expected = 0;
    return atomic_compare_exchange_strong(&context_size_check, &expected, 1) ? 0 : FIBER_ERROR_STATE;
}

static ABI int32_t fiber_stop_context_size_check(void) {
    unsigned expected = 1;
    return atomic_compare_exchange_strong(&context_size_check, &expected, 0) ? 0 : FIBER_ERROR_STATE;
}

static ABI int32_t fiber_rename(Fiber *fiber, const char *name) {
    if (!fiber || !name) return FIBER_ERROR_NULL;
    if ((uintptr_t)fiber & 7) return FIBER_ERROR_ALIGNMENT;
    if (!valid(fiber)) return FIBER_ERROR_INVALID;
    copy_name(fiber->name, name);
    return 0;
}

static ABI int32_t fiber_get_thread_frame_pointer(uint64_t *address) {
    if (!address) return FIBER_ERROR_NULL;
    FiberContext *ctx = current_context();
    if (!ctx) return FIBER_ERROR_PERMISSION;
    *address = ctx->rbp;
    return 0;
}

static const RuntimeExport exports[] = {
    {"_sceFiberInitializeImpl", fiber_initialize},
    {"_sceFiberInitializeWithInternalOptionImpl", fiber_initialize_impl},
    {"sceFiberOptParamInitialize", fiber_opt_param_initialize},
    {"sceFiberFinalize", fiber_finalize},
    {"sceFiberRun", fiber_run},
    {"sceFiberSwitch", fiber_switch},
    {"sceFiberGetSelf", fiber_get_self},
    {"sceFiberReturnToThread", fiber_return_to_thread},
    {"_sceFiberAttachContextAndRun", fiber_run_impl},
    {"_sceFiberAttachContextAndSwitch", fiber_switch_impl},
    {"sceFiberGetInfo", fiber_get_info},
    {"sceFiberStartContextSizeCheck", fiber_start_context_size_check},
    {"sceFiberStopContextSizeCheck", fiber_stop_context_size_check},
    {"sceFiberRename", fiber_rename},
    {"sceFiberGetThreadFramePointerAddress", fiber_get_thread_frame_pointer},
};

uintptr_t runtime_fiber_resolve(const char *name) { return RUNTIME_LOOKUP(exports, name); }

void runtime_fiber_report(void) {
    pthread_mutex_lock(&host_stack_lock);
    size_t stacks = host_stack_count;
    uint64_t bytes = host_stack_bytes;
    pthread_mutex_unlock(&host_stack_lock);
    printf("Runtime: fibers initialized=%llu, runs=%llu, switches=%llu, host stacks=%zu (%llu KiB)\n",
           (unsigned long long)atomic_load(&fibers_initialized), (unsigned long long)atomic_load(&fiber_runs),
           (unsigned long long)atomic_load(&fiber_switches), stacks, (unsigned long long)(bytes / 1024));
}
