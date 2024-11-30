//
// Copyright (c) 2013, Linaro Limited
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

// Assumptions:
//
// ARMv8-a, AArch64
//


// Parameters and result.
#define src1      x0
#define src2      x1
#define limit     x2
#define result    x0

// Internal variables.
#define data1     x3
#define data1w    w3
#define data2     x4
#define data2w    w4
#define diff      x6
#define endloop   x7
#define tmp1      x8
#define tmp2      x9
#define pos       x11
#define limit_wd  x12
#define mask      x13

    .p2align 6
ASM_GLOBAL ASM_PFX(InternalMemCompareMem)
ASM_PFX(InternalMemCompareMem):
    AARCH64_BTI(c)
    eor     tmp1, src1, src2
    tst     tmp1, #7
    b.ne    .Lmisaligned8
    ands    tmp1, src1, #7
    b.ne    .Lmutual_align
    add     limit_wd, limit, #7
    lsr     limit_wd, limit_wd, #3

    // Start of performance-critical section  -- one 64B cache line.
.Lloop_aligned:
    ldr     data1, [src1], #8
    ldr     data2, [src2], #8
.Lstart_realigned:
    subs    limit_wd, limit_wd, #1
    eor     diff, data1, data2        // Non-zero if differences found.
    csinv   endloop, diff, xzr, ne    // Last Dword or differences.
    cbz     endloop, .Lloop_aligned
    // End of performance-critical section  -- one 64B cache line.

    // Not reached the limit, must have found a diff.
    cbnz    limit_wd, .Lnot_limit

    // Limit % 8 == 0 => all bytes significant.
    ands    limit, limit, #7
    b.eq    .Lnot_limit

    lsl     limit, limit, #3              // Bits -> bytes.
    mov     mask, #~0
    lsl     mask, mask, limit
    bic     data1, data1, mask
    bic     data2, data2, mask

    orr     diff, diff, mask

.Lnot_limit:
    rev     diff, diff
    rev     data1, data1
    rev     data2, data2

    // The MS-non-zero bit of DIFF marks either the first bit
    // that is different, or the end of the significant data.
    // Shifting left now will bring the critical information into the
    // top bits.
    clz     pos, diff
    lsl     data1, data1, pos
    lsl     data2, data2, pos

    // But we need to zero-extend (char is unsigned) the value and then
    // perform a signed 32-bit subtraction.
    lsr     data1, data1, #56
    sub     result, data1, data2, lsr #56
    ret

.Lmutual_align:
    // Sources are mutually aligned, but are not currently at an
    // alignment boundary.  Round down the addresses and then mask off
    // the bytes that precede the start point.
    bic     src1, src1, #7
    bic     src2, src2, #7
    add     limit, limit, tmp1          // Adjust the limit for the extra.
    lsl     tmp1, tmp1, #3              // Bytes beyond alignment -> bits.
    ldr     data1, [src1], #8
    neg     tmp1, tmp1                  // Bits to alignment -64.
    ldr     data2, [src2], #8
    mov     tmp2, #~0

    // Little-endian.  Early bytes are at LSB.
    lsr     tmp2, tmp2, tmp1            // Shift (tmp1 & 63).
    add     limit_wd, limit, #7
    orr     data1, data1, tmp2
    orr     data2, data2, tmp2
    lsr     limit_wd, limit_wd, #3
    b       .Lstart_realigned

    .p2align 6
.Lmisaligned8:
    sub     limit, limit, #1
1:
    // Perhaps we can do better than this.
    ldrb    data1w, [src1], #1
    ldrb    data2w, [src2], #1
    subs    limit, limit, #1
    ccmp    data1w, data2w, #0, cs      // NZCV = 0b0000.
    b.eq    1b
    sub     result, data1, data2
    ret
