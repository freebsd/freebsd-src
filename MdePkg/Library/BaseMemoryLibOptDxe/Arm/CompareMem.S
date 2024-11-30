//
// Copyright (c) 2013 - 2016, Linaro Limited
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

// Parameters and result.
#define src1      r0
#define src2      r1
#define limit     r2
#define result    r0

// Internal variables.
#define data1     r3
#define data2     r4
#define limit_wd  r5
#define diff      r6
#define tmp1      r7
#define tmp2      r12
#define pos       r8
#define mask      r14

    .text
    .thumb
    .syntax unified
    .align  5
    .type   ASM_PFX(InternalMemCompareMem), %function
ASM_GLOBAL ASM_PFX(InternalMemCompareMem)
ASM_PFX(InternalMemCompareMem):
    push    {r4-r8, lr}
    eor     tmp1, src1, src2
    tst     tmp1, #3
    bne     .Lmisaligned4
    ands    tmp1, src1, #3
    bne     .Lmutual_align
    add     limit_wd, limit, #3
    nop.w
    lsr     limit_wd, limit_wd, #2

    // Start of performance-critical section  -- one 32B cache line.
.Lloop_aligned:
    ldr     data1, [src1], #4
    ldr     data2, [src2], #4
.Lstart_realigned:
    subs    limit_wd, limit_wd, #1
    eor     diff, data1, data2        // Non-zero if differences found.
    cbnz    diff, 0f
    bne     .Lloop_aligned
    // End of performance-critical section  -- one 32B cache line.

    // Not reached the limit, must have found a diff.
0:  cbnz    limit_wd, .Lnot_limit

    // Limit % 4 == 0 => all bytes significant.
    ands    limit, limit, #3
    beq     .Lnot_limit

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
    lsr     data1, data1, #28
    sub     result, data1, data2, lsr #28
    pop     {r4-r8, pc}

.Lmutual_align:
    // Sources are mutually aligned, but are not currently at an
    // alignment boundary.  Round down the addresses and then mask off
    // the bytes that precede the start point.
    bic     src1, src1, #3
    bic     src2, src2, #3
    add     limit, limit, tmp1          // Adjust the limit for the extra.
    lsl     tmp1, tmp1, #3              // Bytes beyond alignment -> bits.
    ldr     data1, [src1], #4
    rsb     tmp1, tmp1, #32             // Bits to alignment -32.
    ldr     data2, [src2], #4
    mov     tmp2, #~0

    // Little-endian.  Early bytes are at LSB.
    lsr     tmp2, tmp2, tmp1            // Shift (tmp1 & 31).
    add     limit_wd, limit, #3
    orr     data1, data1, tmp2
    orr     data2, data2, tmp2
    lsr     limit_wd, limit_wd, #2
    b       .Lstart_realigned

.Lmisaligned4:
    sub     limit, limit, #1
1:
    // Perhaps we can do better than this.
    ldrb    data1, [src1], #1
    ldrb    data2, [src2], #1
    subs    limit, limit, #1
    it      cs
    cmpcs.n data1, data2
    beq     1b
    sub     result, data1, data2
    pop     {r4-r8, pc}
