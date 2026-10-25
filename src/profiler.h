/* Sampling profiler and hang reporter: see profiler_win.c. No-op off Windows. */
#ifndef GR2_PROFILER_H
#define GR2_PROFILER_H
#include <stddef.h>
#include <stdint.h>
typedef void (*ProfilerDescribe)(char *out, size_t size, uintptr_t address);
/* Starts the sampler when GR2_PROFILE names a report file. */
void profiler_start(ProfilerDescribe describe_address);
#endif
