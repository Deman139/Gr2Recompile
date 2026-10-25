/* Sampling profiler and hang reporter (Windows; GR2_PROFILE=<report file>).
 * A thread suspends every other thread of the process every GR2_PROFILE_MS (default 5) ms,
 * records its call stack and resumes it. Host frames unwind through the modules' .pdata;
 * guest frames (no unwind data) follow the rbp chain. Every GR2_PROFILE_PERIOD (default 5) s
 * the report gets, per thread: CPU time used in the window, the share of samples blocked in a
 * kernel wait, the most frequent stacks and the stack seen last (what a frozen thread is
 * doing). Host frames are module+offset: tools/symbolize_profile.py names them. */
#include "profiler.h"
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "compat_win.h"

enum { MAX_THREADS = 96, MAX_FRAMES = 24, KEY_FRAMES = 8, STACKS = 256, TOP = 6, FUNCS = 1024, FUNC_TOP = 30 };
/* func: the function each frame is in (host: its .pdata start; leaf and guest frames: the
 * address itself), for the per-function totals of the busy threads. */
typedef struct { uintptr_t frame[MAX_FRAMES], func[MAX_FRAMES]; int depth; } Stack;
typedef struct { uintptr_t func; unsigned self, total; } FuncCount;
typedef struct { uintptr_t key[KEY_FRAMES]; int depth; unsigned count; } StackCount;
typedef struct {
    DWORD tid;
    HANDLE handle;
    int live;
    uintptr_t stack_low, stack_high;
    char name[64];
    unsigned samples, waiting;
    uint64_t cpu_start; /* 100 ns units at the window start */
    StackCount stacks[STACKS];
    unsigned distinct, dropped;
    Stack last;
    FuncCount funcs[FUNCS];
    unsigned func_count;
} ThreadInfo;

static ThreadInfo threads[MAX_THREADS];
static int thread_count;
static FILE *report;
static ProfilerDescribe describe;
static uintptr_t ntdll_base, ntdll_end;

typedef LONG (NTAPI *QueryThread)(HANDLE, int, void *, ULONG, ULONG *);
typedef struct { LONG status; void *teb; uintptr_t pid, tid; uintptr_t affinity; LONG prio, base; } ThreadBasic;
typedef HRESULT (WINAPI *GetDescription)(HANDLE, PWSTR *);
static QueryThread query_thread;
static GetDescription get_description;

static uint64_t thread_cpu(HANDLE h) {
    FILETIME c, e, k, u;
    if (!GetThreadTimes(h, &c, &e, &k, &u)) return 0;
    return ((uint64_t)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((uint64_t)u.dwHighDateTime << 32 | u.dwLowDateTime);
}

static void read_name(ThreadInfo *t) {
    PWSTR wide = NULL;
    t->name[0] = 0;
    if (get_description && SUCCEEDED(get_description(t->handle, &wide)) && wide) {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, t->name, sizeof(t->name), NULL, NULL);
        LocalFree(wide);
    }
    if (!t->name[0]) snprintf(t->name, sizeof(t->name), "tid%lu", (unsigned long)t->tid);
}

/* The TEB's NT_TIB holds the stack bounds; the rbp walk stays inside them. */
static void read_stack_bounds(ThreadInfo *t) {
    ThreadBasic basic;
    t->stack_low = t->stack_high = 0;
    if (!query_thread || query_thread(t->handle, 0, &basic, sizeof(basic), NULL) < 0 || !basic.teb) return;
    NT_TIB *tib = (NT_TIB *)basic.teb;
    t->stack_high = (uintptr_t)tib->StackBase;
    t->stack_low = (uintptr_t)tib->StackLimit;
}

/* Adds the threads that started since the last scan; marks the ones that exited. */
static void scan_threads(DWORD self) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    for (int i = 0; i < thread_count; ++i) threads[i].live = 0;
    THREADENTRY32 e = {.dwSize = sizeof(e)};
    DWORD pid = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snap, &e); ok; ok = Thread32Next(snap, &e)) {
        if (e.th32OwnerProcessID != pid || e.th32ThreadID == self) continue;
        int i = 0;
        while (i < thread_count && threads[i].tid != e.th32ThreadID) ++i;
        if (i == thread_count) {
            if (thread_count == MAX_THREADS) continue;
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, e.th32ThreadID);
            if (!h) continue;
            ThreadInfo *t = &threads[thread_count++];
            memset(t, 0, sizeof(*t));
            t->tid = e.th32ThreadID;
            t->handle = h;
            t->cpu_start = thread_cpu(h);
            read_stack_bounds(t);
        }
        threads[i].live = 1;
    }
    CloseHandle(snap);
    for (int i = 0; i < thread_count; ++i)
        if (threads[i].live) read_name(&threads[i]); /* guest threads name themselves after starting */
}

/* Module ranges read outside sampling: asking the loader while a thread is suspended could
 * wait on a loader lock that thread holds. */
enum { MAX_MODULES = 256 };
static struct { uintptr_t start, end; } host_modules[MAX_MODULES];
static int host_module_count;
static void scan_modules(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32 e = {.dwSize = sizeof(e)};
    int n = 0;
    for (BOOL ok = Module32First(snap, &e); ok && n < MAX_MODULES; ok = Module32Next(snap, &e)) {
        host_modules[n].start = (uintptr_t)e.modBaseAddr;
        host_modules[n].end = (uintptr_t)e.modBaseAddr + e.modBaseSize;
        ++n;
    }
    host_module_count = n;
    CloseHandle(snap);
}
static int in_host_module(uintptr_t address) {
    for (int i = 0; i < host_module_count; ++i)
        if (address >= host_modules[i].start && address < host_modules[i].end) return 1;
    return 0;
}

/* Runs with the thread suspended: no allocation and no stdio (the thread may hold their locks). */
static void walk(const ThreadInfo *t, CONTEXT *c, Stack *s) {
    s->depth = 0;
    uintptr_t low = t->stack_low, high = t->stack_high;
    while (s->depth < MAX_FRAMES && c->Rip) {
        s->frame[s->depth] = s->func[s->depth] = c->Rip;
        ++s->depth;
        DWORD64 base = 0;
        int host = in_host_module(c->Rip);
        PRUNTIME_FUNCTION f = host ? RtlLookupFunctionEntry(c->Rip, &base, NULL) : NULL;
        if (f) s->func[s->depth - 1] = (uintptr_t)(base + f->BeginAddress);
        if (f) {
            void *handler_data; DWORD64 frame;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c->Rip, f, c, &handler_data, &frame, NULL);
        } else if (host) { /* leaf function */
            if (c->Rsp < low || c->Rsp + 8 > high) break;
            c->Rip = *(DWORD64 *)c->Rsp;
            c->Rsp += 8;
        } else { /* guest code: frame pointer chain */
            uintptr_t bp = c->Rbp;
            if (bp < c->Rsp || bp < low || bp + 16 > high || (bp & 7)) break;
            c->Rip = ((DWORD64 *)bp)[1];
            c->Rbp = ((DWORD64 *)bp)[0];
            c->Rsp = bp + 16;
        }
        if (c->Rsp < low || c->Rsp > high) break;
    }
}

static FuncCount *func_slot(ThreadInfo *t, uintptr_t func) {
    unsigned i = (unsigned)((func >> 4) * 2654435761u) % FUNCS;
    for (unsigned n = 0; n < FUNCS; ++n, i = (i + 1) % FUNCS) {
        FuncCount *e = &t->funcs[i];
        if (e->func == func) return e;
        if (!e->func) {
            if (t->func_count * 4 >= FUNCS * 3) return NULL; /* full enough: drop new ones */
            e->func = func;
            ++t->func_count;
            return e;
        }
    }
    return NULL;
}

static void record(ThreadInfo *t, const Stack *s) {
    ++t->samples;
    t->last = *s;
    for (int i = 0; i < s->depth; ++i) {
        int seen = 0; /* recursion counts once per sample */
        for (int j = 0; j < i && !seen; ++j) seen = s->func[j] == s->func[i];
        if (seen) continue;
        FuncCount *e = func_slot(t, s->func[i]);
        if (!e) continue;
        ++e->total;
        if (!i) ++e->self;
    }
    if (s->depth && s->frame[0] >= ntdll_base && s->frame[0] < ntdll_end) ++t->waiting; /* syscall stubs: waits/sleeps */
    int depth = s->depth < KEY_FRAMES ? s->depth : KEY_FRAMES;
    for (unsigned i = 0; i < t->distinct; ++i) {
        StackCount *e = &t->stacks[i];
        if (e->depth == depth && !memcmp(e->key, s->frame, depth * sizeof(uintptr_t))) { ++e->count; return; }
    }
    if (t->distinct == STACKS) { ++t->dropped; return; }
    StackCount *e = &t->stacks[t->distinct++];
    memcpy(e->key, s->frame, depth * sizeof(uintptr_t));
    e->depth = depth;
    e->count = 1;
}

static void print_stack(const uintptr_t *frames, int depth) {
    char where[MAX_PATH + 96];
    for (int i = 0; i < depth; ++i) {
        describe(where, sizeof(where), frames[i]);
        fprintf(report, "        %s\n", where);
    }
}

static int by_total(const void *a, const void *b) {
    unsigned x = ((const FuncCount *)a)->total, y = ((const FuncCount *)b)->total;
    return x < y ? 1 : x > y ? -1 : 0;
}
static int by_self(const void *a, const void *b) {
    unsigned x = ((const FuncCount *)a)->self, y = ((const FuncCount *)b)->self;
    return x < y ? 1 : x > y ? -1 : 0;
}

static int by_count(const void *a, const void *b) {
    unsigned x = ((const StackCount *)a)->count, y = ((const StackCount *)b)->count;
    return x < y ? 1 : x > y ? -1 : 0;
}

static void write_report(uint64_t window_ms, unsigned round) {
    fprintf(report, "==== window %u (%" PRIu64 " ms) ====\n", round, window_ms);
    for (int i = 0; i < thread_count; ++i) {
        ThreadInfo *t = &threads[i];
        if (!t->samples) continue;
        uint64_t cpu = thread_cpu(t->handle);
        double busy = window_ms ? (double)(cpu - t->cpu_start) / 1e4 / (double)window_ms * 100.0 : 0.0;
        t->cpu_start = cpu;
        fprintf(report, "-- thread %lu '%s': cpu %.0f%%, samples %u, waiting %.0f%%%s\n", (unsigned long)t->tid, t->name, busy,
                t->samples, 100.0 * t->waiting / t->samples, t->live ? "" : " (exited)");
        qsort(t->stacks, t->distinct, sizeof(StackCount), by_count);
        for (unsigned k = 0; k < t->distinct && k < TOP; ++k) {
            if (t->stacks[k].count * 50 < t->samples) break; /* under 2% */
            fprintf(report, "   %4.1f%%\n", 100.0 * t->stacks[k].count / t->samples);
            print_stack(t->stacks[k].key, t->stacks[k].depth);
        }
        fprintf(report, "   last:\n");
        print_stack(t->last.frame, t->last.depth);
        if (busy >= 30.0) {
            /* Per function: samples inside it (total) and at its own code (self). */
            char where[MAX_PATH + 96];
            qsort(t->funcs, FUNCS, sizeof(FuncCount), by_total);
            fprintf(report, "   functions by total (in it or below):\n");
            for (unsigned k = 0; k < FUNC_TOP && t->funcs[k].total * 100 >= t->samples; ++k) {
                describe(where, sizeof(where), t->funcs[k].func);
                fprintf(report, "     %5.1f%% total %5.1f%% self  %s\n", 100.0 * t->funcs[k].total / t->samples,
                        100.0 * t->funcs[k].self / t->samples, where);
            }
            qsort(t->funcs, FUNCS, sizeof(FuncCount), by_self);
            fprintf(report, "   functions by self:\n");
            for (unsigned k = 0; k < FUNC_TOP && t->funcs[k].self * 200 >= t->samples; ++k) {
                describe(where, sizeof(where), t->funcs[k].func);
                fprintf(report, "     %5.1f%% self  %s\n", 100.0 * t->funcs[k].self / t->samples, where);
            }
        }
        memset(t->funcs, 0, sizeof(t->funcs));
        t->func_count = 0;
        t->samples = t->waiting = t->distinct = t->dropped = 0;
    }
    fflush(report);
}

static DWORD WINAPI profiler_main(void *unused) {
    (void)unused;
    const char *ms = getenv("GR2_PROFILE_MS"), *period = getenv("GR2_PROFILE_PERIOD");
    uint64_t interval_ns = (uint64_t)(ms ? atof(ms) * 1e6 : 5e6), period_ms = (uint64_t)(period ? atof(period) * 1000 : 5000);
    DWORD self = GetCurrentThreadId();
    uint64_t window_start = GetTickCount64(), last_scan = 0;
    for (unsigned round = 1;;) {
        compat_sleep_ns(interval_ns);
        uint64_t now = GetTickCount64();
        if (now - last_scan >= 1000) { scan_modules(); scan_threads(self); last_scan = now; }
        for (int i = 0; i < thread_count; ++i) {
            ThreadInfo *t = &threads[i];
            if (!t->live || SuspendThread(t->handle) == (DWORD)-1) continue;
            CONTEXT c = {.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER};
            Stack s = {.depth = 0};
            if (GetThreadContext(t->handle, &c)) walk(t, &c, &s);
            ResumeThread(t->handle);
            if (s.depth) record(t, &s);
        }
        if (now - window_start >= period_ms) {
            write_report(now - window_start, round++);
            window_start = now;
        }
    }
    return 0;
}

void profiler_start(ProfilerDescribe describe_address) {
    const char *path = getenv("GR2_PROFILE");
    if (!path || !path[0]) return;
    report = fopen(path, "w");
    if (!report) { fprintf(stderr, "Profiler: cannot write %s\n", path); return; }
    describe = describe_address;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    query_thread = (QueryThread)(void *)GetProcAddress(ntdll, "NtQueryInformationThread");
    get_description = (GetDescription)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadDescription");
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)ntdll + ((IMAGE_DOS_HEADER *)ntdll)->e_lfanew);
    ntdll_base = (uintptr_t)ntdll;
    ntdll_end = ntdll_base + nt->OptionalHeader.SizeOfImage;
    HANDLE h = CreateThread(NULL, 0, profiler_main, NULL, 0, NULL);
    if (!h) return;
    SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
    CloseHandle(h);
    printf("Profiler: sampling all threads into %s\n", path);
}
#else
void profiler_start(ProfilerDescribe describe_address) { (void)describe_address; }
#endif
