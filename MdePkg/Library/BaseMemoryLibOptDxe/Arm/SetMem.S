#------------------------------------------------------------------------------
#
# Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

    .text
    .thumb
    .syntax unified
    .align  5
    .type   ASM_PFX(InternalMemSetMem16), %function
ASM_GLOBAL ASM_PFX(InternalMemSetMem16)
ASM_PFX(InternalMemSetMem16):
    uxth    r2, r2
    lsl     r1, r1, #1
    orr     r2, r2, r2, lsl #16
    b       0f

    .type   ASM_PFX(InternalMemSetMem32), %function
ASM_GLOBAL ASM_PFX(InternalMemSetMem32)
ASM_PFX(InternalMemSetMem32):
    lsl     r1, r1, #2
    b       0f

    .type   ASM_PFX(InternalMemSetMem64), %function
ASM_GLOBAL ASM_PFX(InternalMemSetMem64)
ASM_PFX(InternalMemSetMem64):
    lsl     r1, r1, #3
    b       1f

    .align  5
    .type   ASM_PFX(InternalMemSetMem), %function
ASM_GLOBAL ASM_PFX(InternalMemSetMem)
ASM_PFX(InternalMemSetMem):
    uxtb    r2, r2
    orr     r2, r2, r2, lsl #8
    orr     r2, r2, r2, lsl #16
    b       0f

    .type   ASM_PFX(InternalMemZeroMem), %function
ASM_GLOBAL ASM_PFX(InternalMemZeroMem)
ASM_PFX(InternalMemZeroMem):
    movs    r2, #0
0:  mov     r3, r2

1:  push    {r4, lr}
    cmp     r1, #16                 // fewer than 16 bytes of input?
    add     r1, r1, r0              // r1 := dst + length
    add     lr, r0, #16
    blt     2f
    bic     lr, lr, #15             // align output pointer

    str     r2, [r0]                // potentially unaligned store of 4 bytes
    str     r3, [r0, #4]            // potentially unaligned store of 4 bytes
    str     r2, [r0, #8]            // potentially unaligned store of 4 bytes
    str     r3, [r0, #12]           // potentially unaligned store of 4 bytes
    beq     1f

0:  add     lr, lr, #16             // advance the output pointer by 16 bytes
    subs    r4, r1, lr              // past the output?
    blt     3f                      // break out of the loop
    strd    r2, r3, [lr, #-16]      // aligned store of 16 bytes
    strd    r2, r3, [lr, #-8]
    bne     0b                      // goto beginning of loop
1:  pop     {r4, pc}

2:  subs    r4, r1, lr
3:  adds    r4, r4, #16
    subs    r1, r1, #8
    cmp     r4, #4                  // between 4 and 15 bytes?
    blt     4f
    cmp     r4, #8                  // between 8 and 15 bytes?
    sub     r4, lr, #16
    str     r2, [r4]                // overlapping store of 4 + (4 + 4) + 4 bytes
    it      gt
    strgt.n r3, [r4, #4]
    it      gt
    strgt.n r2, [r1]
    str     r3, [r1, #4]
    pop     {r4, pc}

4:  cmp     r4, #2                  // 2 or 3 bytes?
    strb    r2, [lr, #-16]          // store 1 byte
    it      ge
    strhge.n r2, [r1, #6]           // store 2 bytes
    pop     {r4, pc}
