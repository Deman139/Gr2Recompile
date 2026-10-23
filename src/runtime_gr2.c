/* Gravity Rush 2 (CUSA04943): system functions it imports beyond what the Killzone port's
 * runtime serves. Kernel and libc helpers are implemented; the network, PSN and sharing
 * services answer as an offline console with nothing installed besides the game; photo
 * encoding/decoding, content export and 3D audio report themselves unavailable for now.
 * Behaviour follows shadPS4 (GPL-2.0-or-later) where it is known. */
#define _GNU_SOURCE
#include "runtime.h"
#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
#endif

#define KERNEL_ERR(n) ((int32_t)(UINT32_C(0x80020000) | (n)))
enum { E_PERM = 1, E_SRCH = 3, E_ACCES = 13, E_FAULT = 14, E_INVAL = 22 };
#define SAVE_DATA_NOT_FOUND ((int32_t)0x809F0008)
#define AUDIO3D_NOT_SUPPORTED ((int32_t)0x80EA0008)
#define JPEG_DEC_UNSUPPORTED ((int32_t)0x80650001)
#define JPEG_ENC_UNSUPPORTED ((int32_t)0x80650108)
#define CONTENT_EXPORT_NOT_IMPLEMENTED ((int32_t)0x809D3FFF)
#define CONTENT_SEARCH_NOT_INITIALIZED ((int32_t)0x80AA0002)
#define CONTENT_DELETE_NOT_FOUND ((int32_t)0x80A80004)
#define SHARE_UTILITY_NOT_SUPPORTED ((int32_t)0x80A90002)
#define SSL_NOT_FOUND ((int32_t)0x8095F00D)
#define HTTP_INVALID_VALUE ((int32_t)0x804311FE)
#define HTTP_OUT_OF_SIZE ((int32_t)0x80431104)
#define NET_CTL_INVALID_ADDR ((int32_t)0x80412107)
#define NET_EINVAL ((int32_t)0x80410116)
#define NET_ENETUNREACH_CODE 51

static atomic_uint warned;
static void once(unsigned bit, const char *what) {
    if (!(atomic_fetch_or(&warned, 1u << bit) & (1u << bit))) printf("Runtime: %s\n", what);
}

static ABI int32_t ok(void) { return 0; }

/* ---- libkernel ---- */
static ABI int32_t posix_clock_settime(int32_t clock, const void *ts) { (void)clock; (void)ts; return 0; }
static ABI int32_t posix_pthread_equal(void *a, void *b) { return a == b; }
static ABI int32_t kernel_error(int32_t posix) { return posix ? KERNEL_ERR(posix) : 0; }
static ABI int32_t kernel_flexible_available(uint64_t *size) {
    if (!size) return KERNEL_ERR(E_INVAL);
    *size = runtime_flexible_available();
    return 0;
}
typedef struct { int32_t tz_minuteswest, tz_dsttime; } KernelTimezone;
typedef struct { int64_t t; uint32_t west_sec, dst_sec; } KernelTimesec;
/* west_sec: seconds east of UTC without daylight saving, as the PS4 names it. */
static void local_offsets(int64_t when, int32_t *west_sec, int32_t *dst_sec) {
    time_t t = (time_t)when;
    struct tm local;
    localtime_r(&t, &local); /* compat_win.h maps it to localtime_s on Windows */
    *dst_sec = local.tm_isdst > 0 ? 3600 : 0;
#ifdef _WIN32
    *west_sec = (int32_t)compat_utc_offset(t) - *dst_sec;
#else
    *west_sec = (int32_t)local.tm_gmtoff - *dst_sec;
#endif
}
static ABI int32_t kernel_utc_to_local(int64_t utc, int64_t *local, KernelTimesec *st, uint64_t *dst) {
    int32_t west, dst_sec;
    local_offsets(utc, &west, &dst_sec);
    if (local) *local = utc + west + dst_sec;
    if (st) { st->t = utc; st->west_sec = (uint32_t)west; st->dst_sec = (uint32_t)dst_sec; }
    if (dst) *dst = (uint64_t)dst_sec;
    return 0;
}
static ABI int32_t kernel_local_to_utc(int64_t local, int64_t unused, int64_t *utc, KernelTimezone *tz, int32_t *dst) {
    (void)unused;
    if (!tz) return KERNEL_ERR(E_INVAL);
    int32_t west, dst_sec;
    local_offsets(local, &west, &dst_sec);
    tz->tz_minuteswest = -west / 60;
    tz->tz_dsttime = dst_sec / 60;
    if (utc) *utc = local - west - dst_sec;
    if (dst) *dst = dst_sec;
    return 0;
}
typedef struct {
    int32_t app_id, mmap_flags, attribute_exe, attribute2;
    char cusa_name[10];
    uint8_t debug_level, slv_flags, mini_app_dmem_flags, render_mode, mdbg_out, required_hdcp_type;
    uint64_t preload_prx_flags;
    int32_t attribute1, has_param_sfo;
    int32_t workaround_version, workaround_align;
    uint64_t workaround_ids[2];
} KernelAppInfo;
static ABI int32_t kernel_app_info(int32_t pid, KernelAppInfo *info) {
    if (pid != -1 && pid != 1) return KERNEL_ERR(E_PERM);
    if (!info) return 0;
    memset(info, 0, sizeof(*info));
    info->has_param_sfo = 1;
    memcpy(info->cusa_name, "CUSA04943", 9);
    return 0;
}
typedef struct { int32_t version, align; uint64_t ids[2]; } TitleWorkaround;
static ABI int32_t kernel_title_workaround(const TitleWorkaround *tw, int32_t bit, int32_t *result) {
    if (!tw || !result) return KERNEL_ERR(E_FAULT);
    if (bit < 0 || bit >= 0x3a) return KERNEL_ERR(E_INVAL);
    *result = (int32_t)((tw->ids[bit >> 6] >> (bit & 63)) & 1);
    return 0;
}
static ABI int32_t kernel_compiled_sdk(uint32_t *version) {
    if (!version) return KERNEL_ERR(E_INVAL);
    const unsigned char *param = runtime_procparam();
    uint32_t sdk = 0;
    if (param) memcpy(&sdk, param + 16, 4); /* SceProcParam: size, magic, entry count, SDK version */
    *version = sdk;
    return 0;
}
static ABI int32_t kernel_current_cpu(void) {
#ifdef _WIN32
    return (int32_t)(GetCurrentProcessorNumber() % 7);
#else
    int cpu = sched_getcpu();
    return cpu < 0 ? 0 : cpu % 7;
#endif
}
static ABI const char *kernel_sandbox_word(void) { return "sys"; }
/* C++ exception unwinding asks for the module holding an address; no module tables are kept
 * here yet, so a thrown exception reaches std::terminate. The log names the first request. */
static ABI int32_t kernel_module_info_for_unwind(uint64_t address, int32_t flags, void *info) {
    if (flags >= 3) { if (info) memset(info, 0, 304); return KERNEL_ERR(E_INVAL); }
    if (!info) return KERNEL_ERR(E_FAULT);
    once(0, "sceKernelGetModuleInfoForUnwind: no unwind tables (C++ exception thrown?)");
    printf("Runtime: unwind info requested for image offset 0x%llx\n",
           (unsigned long long)(address - runtime_image_start));
    return KERNEL_ERR(E_SRCH);
}
static ABI int32_t kernel_segment_info(void) { return KERNEL_ERR(E_SRCH); }
static ABI int32_t kernel_is_neo(void) { return 0; }
static ABI int32_t kernel_is_stack(void *address, void **start, void **end) {
    (void)address;
    if (start) *start = NULL;
    if (end) *end = NULL;
    return 0;
}
static ABI int32_t kernel_sleep(uint32_t seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
    return 0;
}
static atomic_int next_thread_id = 1;
static __thread int32_t thread_id;
static ABI int32_t pthread_get_thread_id(void) {
    if (!thread_id) thread_id = atomic_fetch_add(&next_thread_id, 1);
    return thread_id;
}
/* select() with no descriptors is a sleep; with descriptors (sockets) nothing is ever ready. */
typedef struct { int64_t tv_sec, tv_usec; } GuestTimeval;
static ABI int32_t posix_select(int32_t nfds, void *r, void *w, void *e, const GuestTimeval *timeout) {
    (void)nfds; (void)r; (void)w; (void)e;
    if (timeout) {
        uint64_t us = (uint64_t)timeout->tv_sec * 1000000 + (uint64_t)timeout->tv_usec;
#ifdef _WIN32
        Sleep((DWORD)((us + 999) / 1000));
#else
        struct timespec t = {(time_t)(us / 1000000), (long)(us % 1000000) * 1000};
        nanosleep(&t, NULL);
#endif
    }
    return 0;
}
static ABI int32_t posix_shutdown(int32_t s, int32_t how) { (void)s; (void)how; return 0; }

/* ---- libSceLibcInternal helpers missing from the game's libc.prx (used by libSceNgs2) ---- */
static ABI int32_t libc_isfinite(double x) { return isfinite(x) ? 1 : 0; }
static ABI int32_t libc_isinf(double x) { return isinf(x) ? 1 : 0; }
static ABI int32_t libc_isnan(double x) { return isnan(x) ? 1 : 0; }
static ABI void libc_sincosf(float x, float *s, float *c) { *s = sinf(x); *c = cosf(x); }
typedef struct { uint64_t size; uint32_t flag, get_segment_info; uint64_t *mspace_atomic_id_mask, *mstate_table; } HeapTraceInfo;
static uint64_t heap_id_mask, heap_mstate_table[64];
static ABI void libc_heap_trace_info(HeapTraceInfo *info) {
    if (!info) return;
    info->mspace_atomic_id_mask = &heap_id_mask;
    info->mstate_table = heap_mstate_table;
    info->get_segment_info = 0;
}

/* ---- AppContent, SaveData, DiscMap ---- */
static ABI int32_t save_transferring_mount(const void *mount, void *result) {
    (void)mount; (void)result;
    once(1, "SaveData: no Gravity Rush Remastered save to transfer");
    return SAVE_DATA_NOT_FOUND;
}
/* sceDiscMap 7C980FFB0AA27E7A: (path, offset, size, flags*, ret1*, ret2*) - not on a disc map. */
static ABI int32_t discmap_7c98(const char *path, int64_t offset, int64_t size, int32_t *flags, int32_t *a, int32_t *b) {
    (void)path; (void)offset; (void)size;
    if (flags) *flags = 0;
    if (a) *a = 0;
    if (b) *b = 0;
    return 0;
}

/* ---- Network (offline) ---- */
static ABI int32_t net_mac_address(uint8_t *mac, int32_t flags) {
    (void)flags;
    if (!mac) return NET_EINVAL;
    memset(mac, 0, 6);
    return 0;
}
static ABI int32_t net_resolver_error(int32_t id, int32_t *status) {
    (void)id;
    if (status) *status = NET_ENETUNREACH_CODE;
    return 0;
}
static ABI int32_t netctl_result(int32_t event, int32_t *error) {
    (void)event;
    if (!error) return NET_CTL_INVALID_ADDR;
    *error = 0;
    return 0;
}
static ABI int32_t http_last_errno(int32_t request, int32_t *error) {
    (void)request;
    if (!error) return HTTP_INVALID_VALUE;
    *error = NET_ENETUNREACH_CODE;
    return 0;
}
/* RFC 3986 percent-encoding of everything but unreserved characters. */
static ABI int32_t http_uri_escape(char *out, uint64_t *require, uint64_t prepare, const char *in) {
    if (!in) return HTTP_INVALID_VALUE;
    static const char hex[] = "0123456789ABCDEF";
    uint64_t needed = 1;
    for (const unsigned char *p = (const unsigned char *)in; *p; ++p)
        needed += (isalnum(*p) || strchr("-._~", *p)) ? 1 : 3;
    if (require) *require = needed;
    if (!out) return 0;
    if (prepare < needed) return HTTP_OUT_OF_SIZE;
    char *o = out;
    for (const unsigned char *p = (const unsigned char *)in; *p; ++p) {
        if (isalnum(*p) || strchr("-._~", *p)) *o++ = (char)*p;
        else { *o++ = '%'; *o++ = hex[*p >> 4]; *o++ = hex[*p & 15]; }
    }
    *o = 0;
    return 0;
}
static ABI int32_t ssl_not_found(void) { return SSL_NOT_FOUND; }

/* ---- Content and sharing services: nothing to search, export, delete or share ---- */
static ABI int32_t content_search_unavailable(void) { return CONTENT_SEARCH_NOT_INITIALIZED; }
static ABI int32_t content_export_unavailable(void) { return CONTENT_EXPORT_NOT_IMPLEMENTED; }
static ABI int32_t content_delete_not_found(void) { return CONTENT_DELETE_NOT_FOUND; }
static ABI int32_t share_unavailable(void) { return SHARE_UTILITY_NOT_SUPPORTED; }

/* ---- Photo mode (JPEG) and 3D audio: unavailable for now ---- */
static ABI int32_t jpeg_dec_unavailable(void) { once(2, "JpegDec: not implemented (photo album)"); return JPEG_DEC_UNSUPPORTED; }
static ABI int32_t jpeg_enc_unavailable(void) { once(3, "JpegEnc: not implemented (photo saving)"); return JPEG_ENC_UNSUPPORTED; }
static ABI int32_t audio3d_unavailable(void) { once(4, "Audio3d: not available (the game uses AudioOut)"); return AUDIO3D_NOT_SUPPORTED; }

static const RuntimeExport exports[] = {
    {"clock_settime", posix_clock_settime}, {"pthread_equal", posix_pthread_equal},
    {"sceKernelAddReadEvent", ok}, {"sceKernelAddWriteEvent", ok},
    {"sceKernelDeleteReadEvent", ok}, {"sceKernelDeleteWriteEvent", ok},
    {"sceKernelAvailableFlexibleMemorySize", kernel_flexible_available},
    {"sceKernelConvertLocaltimeToUtc", kernel_local_to_utc}, {"sceKernelConvertUtcToLocaltime", kernel_utc_to_local},
    {"sceKernelError", kernel_error}, {"sceKernelGetAppInfo", kernel_app_info},
    {"sceKernelGetCompiledSdkVersion", kernel_compiled_sdk}, {"sceKernelGetCurrentCpu", kernel_current_cpu},
    {"sceKernelGetFsSandboxRandomWord", kernel_sandbox_word},
    {"sceKernelGetModuleInfoForUnwind", kernel_module_info_for_unwind},
    {"sceKernelInternalMemoryGetModuleSegmentInfo", kernel_segment_info},
    {"sceKernelIsNeoMode", kernel_is_neo}, {"sceKernelIsStack", kernel_is_stack},
    {"sceKernelSetGPO", ok}, {"sceKernelSettimeofday", ok}, {"sceKernelSleep", kernel_sleep},
    {"sceKernelTitleWorkaroundIsEnabled", kernel_title_workaround},
    {"scePthreadGetthreadid", pthread_get_thread_id},
    {"select", posix_select}, {"shutdown", posix_shutdown},
    {"__isfinite", libc_isfinite}, {"__isinf", libc_isinf}, {"__isnan", libc_isnan}, {"sincosf", libc_sincosf},
    {"sceLibcHeapGetTraceInfo", libc_heap_trace_info},
    {"sceAppContentTemporaryDataUnmount", ok}, {"sceSaveDataTransferringMount", save_transferring_mount},
    {"sceDiscMap_7C980FFB0AA27E7A", discmap_7c98},
    {"sceNetGetMacAddress", net_mac_address}, {"sceNetResolverGetError", net_resolver_error},
    {"sceNetCtlGetResult", netctl_result},
    {"sceHttpGetLastErrno", http_last_errno}, {"sceHttpUriEscape", http_uri_escape},
    {"sceSslFreeSslCertName", ok}, {"sceSslGetIssuerName", ssl_not_found},
    {"sceSslGetMemoryPoolStats", ssl_not_found}, {"sceSslGetNameEntryCount", ssl_not_found},
    {"sceSslGetNameEntryInfo", ssl_not_found}, {"sceSslGetNotAfter", ssl_not_found},
    {"sceSslGetNotBefore", ssl_not_found}, {"sceSslGetSerialNumber", ssl_not_found},
    {"sceSslGetSubjectName", ssl_not_found},
    {"sceContentSearchInit", content_search_unavailable}, {"sceContentSearchTerm", ok},
    {"sceContentSearchSearchContent", content_search_unavailable},
    {"sceContentSearchGetNumOfContent", content_search_unavailable},
    {"sceContentSearchGetMyApplicationIndex", content_search_unavailable},
    {"sceContentSearchOpenMetadataByContentId", content_search_unavailable},
    {"sceContentSearchGetMetadataValue", content_search_unavailable},
    {"sceContentSearchCloseMetadata", ok},
    {"sceContentExportInit", content_export_unavailable}, {"sceContentExportTerm", ok},
    {"sceContentExportStart", content_export_unavailable}, {"sceContentExportFinish", ok},
    {"sceContentExportFromData", content_export_unavailable},
    {"sceContentExportFromDataWithThumbnail", content_export_unavailable},
    {"sceContentDeleteInitialize", ok}, {"sceContentDeleteTerminate", ok},
    {"sceContentDeleteById", content_delete_not_found},
    {"sceShareUtilityInitialize", share_unavailable}, {"sceShareUtilityTerminate", ok},
    {"sceShareUtilityOpenShareMenu", share_unavailable}, {"sceShareUtilityOpenShareMenuDefault", share_unavailable},
    {"sceJpegDecCreate", jpeg_dec_unavailable}, {"sceJpegDecDelete", ok}, {"sceJpegDecDecode", jpeg_dec_unavailable},
    {"sceJpegDecParseHeader", jpeg_dec_unavailable}, {"sceJpegDecQueryMemorySize", jpeg_dec_unavailable},
    {"sceJpegEncCreate", jpeg_enc_unavailable}, {"sceJpegEncDelete", ok}, {"sceJpegEncEncode", jpeg_enc_unavailable},
    {"sceJpegEncQueryMemorySize", jpeg_enc_unavailable},
    {"sceAudio3dInitialize", audio3d_unavailable}, {"sceAudio3dTerminate", ok},
    {"sceAudio3dGetDefaultOpenParameters", ok}, {"sceAudio3dPortOpen", audio3d_unavailable},
    {"sceAudio3dPortClose", ok}, {"sceAudio3dPortAdvance", audio3d_unavailable},
    {"sceAudio3dPortPush", audio3d_unavailable}, {"sceAudio3dPortGetQueueLevel", audio3d_unavailable},
    {"sceAudio3dPortSetAttribute", audio3d_unavailable}, {"sceAudio3dObjectReserve", audio3d_unavailable},
    {"sceAudio3dObjectUnreserve", ok}, {"sceAudio3dObjectSetAttributes", audio3d_unavailable},
};

uintptr_t runtime_gr2_resolve(const char *name) { return RUNTIME_LOOKUP(exports, name); }
