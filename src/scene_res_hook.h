/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GR2_SCENE_RES_HOOK_H
#define GR2_SCENE_RES_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Killzone Shadow Fall CUSA00190 v01.81: the 3D scene at a smaller size than the display, for the
 * upscaler's quality modes (GR2_RENDER_RES). The display size (device 0x3a3fa68 +0x3be8/+0x3bec,
 * set from the init rectangle at 0x8e6256) stays 1920x1080: the UI, menus, movies and loading
 * screen are drawn with it, which is what the old resolution patch shrank along with the scene.
 *
 * The scene renderer (0x3b490e8, ctor 0x509e40) creates every scene target from the size given to
 * 0x50a670, and each caller copies the display size there with "mov r32,[reg+0x3be8/0x3bec]":
 * boot (0x8e6470), the two target re-creations (0x50f930, 0x50fc43) and level load (0xdc5120).
 * Each load becomes "mov r32,imm32; nop". */
enum { GR2_SCENE_SIZE_SITES = 4 };
typedef struct { const char *name; Gr2Signature site; int width_first; } Gr2SceneSizeSite;
static const Gr2SceneSizeSite gr2_scene_size_sites[GR2_SCENE_SIZE_SITES] = {
    /* mov eax,[rbx+0x3be8]; mov ecx,[rbx+0x3bec] */
    {"boot", {"8b 83 e8 3b 00 00 8b 8b ec 3b 00 00 c7 85 48 ff ff ff ff ff ff ff", 0}, 0},
    /* mov ecx,[rax+0x3be8]; mov eax,[rax+0x3bec] */
    {"re-creation", {"8b 88 e8 3b 00 00 8b 80 ec 3b 00 00 c7 45 a8 ff ff ff ff", 0}, 1},
    {"re-creation", {"8b 88 e8 3b 00 00 8b 80 ec 3b 00 00 c7 45 c0 ff ff ff ff", 0}, 1},
    {"level load", {"8b 88 e8 3b 00 00 8b 80 ec 3b 00 00 48 8d b5 68 ff ff ff", 0}, 1},
};

/* The two loads replaced in place: eax/ecx get the width/height the site's own order gives them. */
static inline void gr2_scene_size_write(unsigned char *at, int width_first, uint32_t width, uint32_t height) {
    const unsigned char first = width_first ? 0xb9 : 0xb8, second = width_first ? 0xb8 : 0xb9;
    at[0] = first;  memcpy(at + 1, &width, 4);  at[5] = 0x90;  /* mov ecx|eax, width */
    at[6] = second; memcpy(at + 7, &height, 4); at[11] = 0x90; /* mov eax|ecx, height */
}

/* Image offset 0x50fe57 in the scene's per-frame view copy (0x50fcb0): each view (0x730 bytes,
 * render rectangle x,y,w,h at +0x1f0) has just been copied twice, to r14+r13 and r12+r13. The
 * game's own half-width mode (multiplayer at 60 Hz) halves w right after, in those copies only:
 * the original rectangle is the UI's reference (0xd94eef). The stub does the same for a full-display
 * rectangle {0,0,1920,1080}, which becomes {0,0,w,h}, then replays the 10 bytes it replaced. */
enum { GR2_SCENE_RECT_HOOK_LENGTH = 10 };
static const Gr2Signature gr2_scene_rect_site = {"48 8b 75 c0 80 7e 10 00 74 70 4b 8b 8c 2f f0 01 00 00", 0};

/* Returns the stub's length, or 0 when a branch back is out of rel32 range. rax and the flags are
 * free here (both paths overwrite them before reading). */
static inline unsigned gr2_scene_rect_build_stub(unsigned char *out, uintptr_t out_address, uintptr_t site,
                                                  uint32_t width, uint32_t height) {
    unsigned n = 0;
    for (int copy = 0; copy < 2; ++copy) {
        /* lea rax,[r14|r12 + r13 + 0x1f0] */
        const unsigned char lea[8] = {0x4b, 0x8d, 0x84, copy ? 0x2c : 0x2e, 0xf0, 0x01, 0x00, 0x00};
        memcpy(out + n, lea, 8); n += 8;
        const unsigned char checks[] = {
            0x83, 0x38, 0x00,                         /* cmp dword [rax],0 */
            0x75, 0x26,                               /* jne skip */
            0x83, 0x78, 0x04, 0x00,                   /* cmp dword [rax+4],0 */
            0x75, 0x20,                               /* jne skip */
            0x81, 0x78, 0x08, 0x80, 0x07, 0x00, 0x00, /* cmp dword [rax+8],1920 */
            0x75, 0x17,                               /* jne skip */
            0x81, 0x78, 0x0c, 0x38, 0x04, 0x00, 0x00, /* cmp dword [rax+0xc],1080 */
            0x75, 0x0e};                              /* jne skip (the two 7-byte writes) */
        memcpy(out + n, checks, sizeof checks); n += sizeof checks;
        out[n++] = 0xc7; out[n++] = 0x40; out[n++] = 0x08; memcpy(out + n, &width, 4); n += 4;  /* mov [rax+8],w */
        out[n++] = 0xc7; out[n++] = 0x40; out[n++] = 0x0c; memcpy(out + n, &height, 4); n += 4; /* mov [rax+0xc],h */
        /* skip: */
    }
    const unsigned char replay[8] = {0x48, 0x8b, 0x75, 0xc0, 0x80, 0x7e, 0x10, 0x00}; /* mov rsi,[rbp-0x40]; cmp byte [rsi+0x10],0 */
    memcpy(out + n, replay, 8); n += 8;
    const int64_t to_half = (int64_t)(site + 10 + 0x70) - (int64_t)(out_address + n + 6); /* the original jz target */
    const int64_t to_resume = (int64_t)(site + GR2_SCENE_RECT_HOOK_LENGTH) - (int64_t)(out_address + n + 6 + 5);
    if (to_half < INT32_MIN || to_half > INT32_MAX || to_resume < INT32_MIN || to_resume > INT32_MAX) return 0;
    const int32_t z = (int32_t)to_half, r = (int32_t)to_resume;
    out[n++] = 0x0f; out[n++] = 0x84; memcpy(out + n, &z, 4); n += 4; /* jz 0x50fed1 */
    out[n++] = 0xe9; memcpy(out + n, &r, 4); n += 4;                  /* jmp 0x50fe61 */
    return n;
}
#endif
