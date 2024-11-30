//
// Copyright (c) 2012 - 2016, Linaro Limited
// All rights reserved.
// Copyright (c) 2015 ARM Ltd
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

// Assumptions:
//
// ARMv8-a, AArch64, unaligned accesses.
//
//

#define dstin     x0
#define src       x1
#define count     x2
#define dst       x3
#define srcend    x4
#define dstend    x5
#define A_l       x6
#define A_lw      w6
#define A_h       x7
#define A_hw      w7
#define B_l       x8
#define B_lw      w8
#define B_h       x9
#define C_l       x10
#define C_h       x11
#define D_l       x12
#define D_h       x13
#define E_l       x14
#define E_h       x15
#define F_l       srcend
#define F_h       dst
#define tmp1      x9
#define tmp2      x3

#define L(l) .L ## l

// Copies are split into 3 main cases: small copies of up to 16 bytes,
// medium copies of 17..96 bytes which are fully unrolled. Large copies
// of more than 96 bytes align the destination and use an unrolled loop
// processing 64 bytes per iteration.
// Small and medium copies read all data before writing, allowing any
// kind of overlap, and memmove tailcalls memcpy for these cases as
// well as non-overlapping copies.

__memcpy:
    prfm    PLDL1KEEP, [src]
    add     srcend, src, count
    add     dstend, dstin, count
    cmp     count, 16
    b.ls    L(copy16)
    cmp     count, 96
    b.hi    L(copy_long)

    // Medium copies: 17..96 bytes.
    sub     tmp1, count, 1
    ldp     A_l, A_h, [src]
    tbnz    tmp1, 6, L(copy96)
    ldp     D_l, D_h, [srcend, -16]
    tbz     tmp1, 5, 1f
    ldp     B_l, B_h, [src, 16]
    ldp     C_l, C_h, [srcend, -32]
    stp     B_l, B_h, [dstin, 16]
    stp     C_l, C_h, [dstend, -32]
1:
    stp     A_l, A_h, [dstin]
    stp     D_l, D_h, [dstend, -16]
    ret

    .p2align 4
    // Small copies: 0..16 bytes.
L(copy16):
    cmp     count, 8
    b.lo    1f
    ldr     A_l, [src]
    ldr     A_h, [srcend, -8]
    str     A_l, [dstin]
    str     A_h, [dstend, -8]
    ret
    .p2align 4
1:
    tbz     count, 2, 1f
    ldr     A_lw, [src]
    ldr     A_hw, [srcend, -4]
    str     A_lw, [dstin]
    str     A_hw, [dstend, -4]
    ret

    // Copy 0..3 bytes.  Use a branchless sequence that copies the same
    // byte 3 times if count==1, or the 2nd byte twice if count==2.
1:
    cbz     count, 2f
    lsr     tmp1, count, 1
    ldrb    A_lw, [src]
    ldrb    A_hw, [srcend, -1]
    ldrb    B_lw, [src, tmp1]
    strb    A_lw, [dstin]
    strb    B_lw, [dstin, tmp1]
    strb    A_hw, [dstend, -1]
2:  ret

    .p2align 4
    // Copy 64..96 bytes.  Copy 64 bytes from the start and
    // 32 bytes from the end.
L(copy96):
    ldp     B_l, B_h, [src, 16]
    ldp     C_l, C_h, [src, 32]
    ldp     D_l, D_h, [src, 48]
    ldp     E_l, E_h, [srcend, -32]
    ldp     F_l, F_h, [srcend, -16]
    stp     A_l, A_h, [dstin]
    stp     B_l, B_h, [dstin, 16]
    stp     C_l, C_h, [dstin, 32]
    stp     D_l, D_h, [dstin, 48]
    stp     E_l, E_h, [dstend, -32]
    stp     F_l, F_h, [dstend, -16]
    ret

    // Align DST to 16 byte alignment so that we don't cross cache line
    // boundaries on both loads and stores. There are at least 96 bytes
    // to copy, so copy 16 bytes unaligned and then align.  The loop
    // copies 64 bytes per iteration and prefetches one iteration ahead.

    .p2align 4
L(copy_long):
    and     tmp1, dstin, 15
    bic     dst, dstin, 15
    ldp     D_l, D_h, [src]
    sub     src, src, tmp1
    add     count, count, tmp1      // Count is now 16 too large.
    ldp     A_l, A_h, [src, 16]
    stp     D_l, D_h, [dstin]
    ldp     B_l, B_h, [src, 32]
    ldp     C_l, C_h, [src, 48]
    ldp     D_l, D_h, [src, 64]!
    subs    count, count, 128 + 16  // Test and readjust count.
    b.ls    2f
1:
    stp     A_l, A_h, [dst, 16]
    ldp     A_l, A_h, [src, 16]
    stp     B_l, B_h, [dst, 32]
    ldp     B_l, B_h, [src, 32]
    stp     C_l, C_h, [dst, 48]
    ldp     C_l, C_h, [src, 48]
    stp     D_l, D_h, [dst, 64]!
    ldp     D_l, D_h, [src, 64]!
    subs    count, count, 64
    b.hi    1b

    // Write the last full set of 64 bytes.   The remainder is at most 64
    // bytes, so it is safe to always copy 64 bytes from the end even if
    // there is just 1 byte left.
2:
    ldp     E_l, E_h, [srcend, -64]
    stp     A_l, A_h, [dst, 16]
    ldp     A_l, A_h, [srcend, -48]
    stp     B_l, B_h, [dst, 32]
    ldp     B_l, B_h, [srcend, -32]
    stp     C_l, C_h, [dst, 48]
    ldp     C_l, C_h, [srcend, -16]
    stp     D_l, D_h, [dst, 64]
    stp     E_l, E_h, [dstend, -64]
    stp     A_l, A_h, [dstend, -48]
    stp     B_l, B_h, [dstend, -32]
    stp     C_l, C_h, [dstend, -16]
    ret


//
// All memmoves up to 96 bytes are done by memcpy as it supports overlaps.
// Larger backwards copies are also handled by memcpy. The only remaining
// case is forward large copies.  The destination is aligned, and an
// unrolled loop processes 64 bytes per iteration.
//

ASM_GLOBAL ASM_PFX(InternalMemCopyMem)
ASM_PFX(InternalMemCopyMem):
    AARCH64_BTI(c)
    sub     tmp2, dstin, src
    cmp     count, 96
    ccmp    tmp2, count, 2, hi
    b.hs    __memcpy

    cbz     tmp2, 3f
    add     dstend, dstin, count
    add     srcend, src, count

    // Align dstend to 16 byte alignment so that we don't cross cache line
    // boundaries on both loads and stores. There are at least 96 bytes
    // to copy, so copy 16 bytes unaligned and then align. The loop
    // copies 64 bytes per iteration and prefetches one iteration ahead.

    and     tmp2, dstend, 15
    ldp     D_l, D_h, [srcend, -16]
    sub     srcend, srcend, tmp2
    sub     count, count, tmp2
    ldp     A_l, A_h, [srcend, -16]
    stp     D_l, D_h, [dstend, -16]
    ldp     B_l, B_h, [srcend, -32]
    ldp     C_l, C_h, [srcend, -48]
    ldp     D_l, D_h, [srcend, -64]!
    sub     dstend, dstend, tmp2
    subs    count, count, 128
    b.ls    2f
    nop
1:
    stp     A_l, A_h, [dstend, -16]
    ldp     A_l, A_h, [srcend, -16]
    stp     B_l, B_h, [dstend, -32]
    ldp     B_l, B_h, [srcend, -32]
    stp     C_l, C_h, [dstend, -48]
    ldp     C_l, C_h, [srcend, -48]
    stp     D_l, D_h, [dstend, -64]!
    ldp     D_l, D_h, [srcend, -64]!
    subs    count, count, 64
    b.hi    1b

    // Write the last full set of 64 bytes. The remainder is at most 64
    // bytes, so it is safe to always copy 64 bytes from the start even if
    // there is just 1 byte left.
2:
    ldp     E_l, E_h, [src, 48]
    stp     A_l, A_h, [dstend, -16]
    ldp     A_l, A_h, [src, 32]
    stp     B_l, B_h, [dstend, -32]
    ldp     B_l, B_h, [src, 16]
    stp     C_l, C_h, [dstend, -48]
    ldp     C_l, C_h, [src]
    stp     D_l, D_h, [dstend, -64]
    stp     E_l, E_h, [dstin, 48]
    stp     A_l, A_h, [dstin, 32]
    stp     B_l, B_h, [dstin, 16]
    stp     C_l, C_h, [dstin]
3:  ret
