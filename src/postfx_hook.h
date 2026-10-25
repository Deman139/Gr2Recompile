/* SPDX-License-Identifier: GPL-2.0-or-later
 * Sites and checks adapted from pearlxcore/KillzoneShadowFall-PC (src/kzsf_motion_blur.h,
 * kzsf_depth_of_field.h, kzsf_post_effects.h, kzsf_camera_controls_win.h). */
#ifndef GR2_POSTFX_HOOK_H
#define GR2_POSTFX_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Killzone Shadow Fall CUSA00190 v01.81: the post-process feature mask (image offset 0x161d504,
 * 0xfff on retail). Each pass tests its bit with "lea rax,[mask]; test byte [rax],bit" and takes
 * the game's own off branch when the bit is clear, which still writes the pass's target with the
 * neutral result. Bit 4 (vignette) also gates other effects and is never touched. The mask is
 * cleared in the image before the game starts, so these apply on restart. */
enum { GR2_POSTFX_COUNT = 4 };
typedef struct { const char *name; uint32_t bit; Gr2Signature site; } Gr2PostFx;
static const Gr2PostFx gr2_postfx[GR2_POSTFX_COUNT] = {
    /* 0x577c1a: motion blur, then a call to its pass (0x5787f0). */
    {"motion blur", 1, {"48 8d 05 ?? ?? ?? ?? f6 00 01 74 10 48 89 df 4c 89 fe e8 ?? ?? ?? ?? e9", 0}},
    /* 0x57835d: depth of field (far and near passes, 0x57a600). */
    {"depth of field", 2, {"48 8d 05 ?? ?? ?? ?? f6 00 02 74 2b 44 0f b6 8d 4e fe ff ff", 0}},
    /* 0x57a15d: bloom capture and integrate. */
    {"bloom", 4, {"48 8d 05 ?? ?? ?? ?? f6 00 04 48 8b 9d 88 fe ff ff 0f 85", 0}},
    /* 0x57824e: film grain weight pass. */
    {"film grain", 8, {"48 8d 05 ?? ?? ?? ?? f6 00 08 74 29 48 83 7b 68 00 74 22", 0}},
};

/* The mask's image offset from the site's lea. */
static inline uint64_t gr2_postfx_mask(const unsigned char *image, uint64_t at) {
    int32_t disp;
    memcpy(&disp, image + at + 3, 4);
    return at + 7 + (int64_t)disp;
}

/* Only a known mask: bits 4..11 set, 12..31 clear; bits 0..3 may already be cleared. */
static inline int gr2_postfx_mask_known(uint32_t flags) { return (flags | 0x0fu) == 0xfffu; }

/* Image offset 0xd92b4e, the player view's camera update (0xd92a00): right after the camera
 * shake is evaluated (call at 0xd92b49 into the shake transform at rbp-0x170 and the FOV factor
 * at rbp-0x234), "vmovss xmm0,[rbp-0x234]" reads the factor. With shake off the stub first writes
 * the game's no-shake values there (identity transform, factor 1), as its no-shake branch does. */
enum { GR2_SHAKE_HOOK_LENGTH = 8 };
static const Gr2Signature gr2_shake_site = {
    "e8 ?? ?? ?? ?? c5 fa 10 85 cc fd ff ff c5 fa 59 43 6c c5 fa 11 43 6c", 5};
static const unsigned char gr2_shake_original[GR2_SHAKE_HOOK_LENGTH] = {
    0xc5, 0xfa, 0x10, 0x85, 0xcc, 0xfd, 0xff, 0xff /* vmovss xmm0, [rbp-0x234] */
};

/* The stub at `out` (address out_address): cell is an int, nonzero keeps the game's shake. The
 * flags are free here (the next instructions do not read them; a call just clobbered them).
 * Returns its length, or 0 when the cell or the resume point is out of rel32 range. */
static inline unsigned gr2_shake_build_stub(unsigned char *out, uintptr_t out_address, uintptr_t cell,
                                             uintptr_t resume) {
    unsigned n = 0;
    const int64_t to_cell = (int64_t)cell - (int64_t)(out_address + 7);
    if (to_cell < INT32_MIN || to_cell > INT32_MAX) return 0;
    const int32_t c = (int32_t)to_cell;
    out[n++] = 0x83; out[n++] = 0x3d;                /* cmp dword [rip+cell], 0 */
    memcpy(out + n, &c, 4); n += 4;
    out[n++] = 0x00;
    out[n++] = 0x0f; out[n++] = 0x85;                /* jne replay */
    const unsigned jne = n;
    n += 4;
    const struct { int32_t disp; uint32_t value; } writes[17] = {
        {-0x234, 0x3f800000},
        {-0x170, 0x3f800000}, {-0x16c, 0}, {-0x168, 0}, {-0x164, 0},
        {-0x160, 0}, {-0x15c, 0x3f800000}, {-0x158, 0}, {-0x154, 0},
        {-0x150, 0}, {-0x14c, 0}, {-0x148, 0x3f800000}, {-0x144, 0},
        {-0x140, 0}, {-0x13c, 0}, {-0x138, 0}, {-0x134, 0x3f800000}};
    for (unsigned i = 0; i < 17; ++i) {
        out[n++] = 0xc7; out[n++] = 0x85;            /* mov dword [rbp+disp32], imm32 */
        memcpy(out + n, &writes[i].disp, 4); n += 4;
        memcpy(out + n, &writes[i].value, 4); n += 4;
    }
    const int32_t skip = (int32_t)(n - (jne + 4));
    memcpy(out + jne, &skip, 4);
    memcpy(out + n, gr2_shake_original, GR2_SHAKE_HOOK_LENGTH);
    n += GR2_SHAKE_HOOK_LENGTH;
    const int64_t back = (int64_t)resume - (int64_t)(out_address + n + 5);
    if (back < INT32_MIN || back > INT32_MAX) return 0;
    const int32_t b = (int32_t)back;
    out[n++] = 0xe9;                                 /* jmp resume */
    memcpy(out + n, &b, 4);
    return n + 4;
}
#endif
