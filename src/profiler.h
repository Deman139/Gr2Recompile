/* Sampling profiler and hang reporter: see profiler_win.c. No-op off Windows. */
#ifndef GOW3_PROFILER_H
#define GOW3_PROFILER_H
#include <stddef.h>
#include <stdint.h>
typedef void (*ProfilerDescribe)(char *out, size_t size, uintptr_t address);
/* Starts the sampler when GOW3_PROFILE names a report file. */
void profiler_start(ProfilerDescribe describe_address);
#endif
