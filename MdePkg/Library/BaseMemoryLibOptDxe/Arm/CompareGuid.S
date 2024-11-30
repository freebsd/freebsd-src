//
// Copyright (c) 2016, Linaro Limited
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

    .text
    .thumb
    .syntax unified
    .align  5
    .type   ASM_PFX(InternalMemCompareGuid), %function
ASM_GLOBAL ASM_PFX(InternalMemCompareGuid)
ASM_PFX(InternalMemCompareGuid):
    push    {r4, lr}
    ldr     r2, [r0]
    ldr     r3, [r0, #4]
    ldr     r4, [r0, #8]
    ldr     r0, [r0, #12]
    cbz     r1, 1f
    ldr     ip, [r1]
    ldr     lr, [r1, #4]
    cmp     r2, ip
    it      eq
    cmpeq.n r3, lr
    beq     0f
    movs    r0, #0
    pop     {r4, pc}

0:  ldr     r2, [r1, #8]
    ldr     r3, [r1, #12]
    cmp     r4, r2
    it      eq
    cmpeq.n r0, r3
    bne     2f
    movs    r0, #1
    pop     {r4, pc}

1:  orrs    r2, r2, r3
    orrs    r4, r4, r0
    movs    r0, #1
    orrs    r2, r2, r4
2:  it      ne
    movne.n r0, #0
    pop     {r4, pc}
