/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GR2_MOUSE_HOOK_H
#define GR2_MOUSE_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Killzone Shadow Fall CUSA00190 v01.81, image offset 0x8ee040: the look input of a player
 * (rdi: the player's input object, rsi/rdx: float *x, *y out, rcx: settings). It reads the right
 * stick, applies the game's dead zone and response curve and writes x/y in [-1, 1]; every look
 * consumer calls it (on foot, mounted guns, vehicles, the OWL, weapon sway, radial menus) and
 * turns at speed * x. Mouse look (GR2_MOUSE_DIRECT=0 turns this off) wraps it: the stub runs the game's
 * routine, then gr2_mouse_look_adjust adds the mouse's speed to x/y, linear and unclamped, so
 * the turn follows the mouse instead of a saturated stick. */
enum { GR2_MOUSE_HOOK_LENGTH = 10 };
static const unsigned char gr2_mouse_original[GR2_MOUSE_HOOK_LENGTH] = {
    0x55,                /* push rbp */
    0x48,0x89,0xe5,      /* mov rbp, rsp */
    0x41,0x57,           /* push r15 */
    0x41,0x56,           /* push r14 */
    0x41,0x55            /* push r13 */
};
static const Gr2Signature gr2_mouse_site = {
    "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 ec 28 49 89 cc 49 89 d6 49 89 f7 48 89 fb "
    "48 8b 05 ?? ?? ?? ?? 48 8b 00 48 89 45 d0 80 bb 48 01 00 00 00 74", 0};

static inline int gr2_mouse_signature_match(const unsigned char *code, uint64_t size) {
    return size >= GR2_MOUSE_HOOK_LENGTH && !memcmp(code, gr2_mouse_original, GR2_MOUSE_HOOK_LENGTH);
}

/* The stub replaces the routine's entry: it calls the routine (its relocated prologue, then a jump
 * back past it) with the out pointers saved, then helper(x, y) (SysV). Returns its length. */
static inline unsigned gr2_mouse_build_stub(unsigned char *out, uintptr_t helper, uintptr_t resume) {
    static const unsigned char body[] = {
        0x56,                           /* push rsi */
        0x52,                           /* push rdx */
        0x57,                           /* push rdi (stack 16-aligned for the call) */
        0xe8,0,0,0,0,                   /* call routine (rel32 patched below) */
        0x5f,                           /* pop rdi */
        0x5a,                           /* pop rdx */
        0x5e,                           /* pop rsi */
        0x48,0x89,0xf7,                 /* mov rdi, rsi (x) */
        0x48,0x89,0xd6,                 /* mov rsi, rdx (y) */
        0x48,0x83,0xec,0x08,            /* sub rsp, 8 */
        0x48,0xb8, 0,0,0,0,0,0,0,0,     /* movabs rax, helper */
        0xff,0xd0,                      /* call rax */
        0x48,0x83,0xc4,0x08,            /* add rsp, 8 */
        0xc3                            /* ret */
    };
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    unsigned n = sizeof(body);
    memcpy(out, body, n);
    memcpy(out + 23, &helper, 8);
    const int32_t routine = (int32_t)(n - 8);  /* from the end of the call (offset 8) to here */
    memcpy(out + 4, &routine, 4);
    memcpy(out + n, gr2_mouse_original, GR2_MOUSE_HOOK_LENGTH);
    n += GR2_MOUSE_HOOK_LENGTH;
    memcpy(out + n, jump, sizeof(jump));
    n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}

/* Image offset 0xd92f1b, the player view's FOV update (vtable 0x17c38f8, 0xd92a00): after the
 * base camera wrote the FOV (degrees), fov = FOVMultiplier (singleton +0x1f7c) * zoom * fov *
 * pi/180 ... * 180/pi. The vmulss by pi/180 at +0x17 (its rel32 at +0x1b) is pointed at a float
 * the loader keeps writable, pi/180 * FOV scale, which the FOV setting rewrites live. */
enum { GR2_FOV_DISP_OFFSET = 27 };
static const Gr2Signature gr2_fov_site = {
    "c4 c1 7a 10 87 7c 1f 00 00 c4 c1 7a 59 86 d4 01 00 00 c5 fa 59 43 6c c5 fa 59 05 ?? ?? ?? ?? "
    "c4 c1 7a 10 8e d8 01 00 00", 0};

/* Image offset 0xbb3130: the player's look limits (vtable slot 0x120 of the player entity class,
 * rdx: out). From the Humanoid resource it fills turn speeds (+0x0c heading, +0x1c pitch), the
 * accelerations that ramp the turn up to them (+0x10, +0x20) and the angle limits; it returns
 * the look mode in eax. While the mouse moves, gr2_mouse_look_limits(out) raises the
 * accelerations so a flick turns at once (no ramp); a gamepad keeps the game's own. */
enum { GR2_MOUSE_LIMITS_LENGTH = 10 };
static const unsigned char gr2_mouse_limits_original[GR2_MOUSE_LIMITS_LENGTH] = {
    0x55,                /* push rbp */
    0x48,0x89,0xe5,      /* mov rbp, rsp */
    0x41,0x56,           /* push r14 */
    0x53,                /* push rbx */
    0x48,0x89,0xd3       /* mov rbx, rdx */
};
static const Gr2Signature gr2_mouse_limits_site = {
    "55 48 89 e5 41 56 53 48 89 d3 49 89 fe 49 8b 06 ff 90 98 03 00 00 83 f8 02 0f 84 ?? ?? ?? ?? "
    "49 8b 4e 28 48 8b 89 d0 01 00 00 c6 03 00", 0};

static inline int gr2_mouse_limits_signature_match(const unsigned char *code, uint64_t size) {
    return size >= GR2_MOUSE_LIMITS_LENGTH &&
           !memcmp(code, gr2_mouse_limits_original, GR2_MOUSE_LIMITS_LENGTH);
}

/* Calls the routine with out saved, then helper(out) with the routine's eax kept. */
static inline unsigned gr2_mouse_limits_build_stub(unsigned char *out, uintptr_t helper, uintptr_t resume) {
    static const unsigned char body[] = {
        0x52,                           /* push rdx (stack 16-aligned for the call) */
        0xe8,0,0,0,0,                   /* call routine (rel32 patched below) */
        0x5f,                           /* pop rdi (out) */
        0x50,                           /* push rax (look mode; aligned again) */
        0x48,0xb8, 0,0,0,0,0,0,0,0,     /* movabs rax, helper */
        0xff,0xd0,                      /* call rax */
        0x58,                           /* pop rax */
        0xc3                            /* ret */
    };
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    unsigned n = sizeof(body);
    memcpy(out, body, n);
    memcpy(out + 10, &helper, 8);
    const int32_t routine = (int32_t)(n - 6);  /* from the end of the call (offset 6) to here */
    memcpy(out + 2, &routine, 4);
    memcpy(out + n, gr2_mouse_limits_original, GR2_MOUSE_LIMITS_LENGTH);
    n += GR2_MOUSE_LIMITS_LENGTH;
    memcpy(out + n, jump, sizeof(jump));
    n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}
#endif
