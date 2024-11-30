;
; Copyright (c) 2016, Linaro Limited
; All rights reserved.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;

    EXPORT  InternalMemCompareGuid
    THUMB
    AREA    CompareGuid, CODE, READONLY, CODEALIGN, ALIGN=5

InternalMemCompareGuid
    push    {r4, lr}
    ldr     r2, [r0]
    ldr     r3, [r0, #4]
    ldr     r4, [r0, #8]
    ldr     r0, [r0, #12]
    cbz     r1, L1
    ldr     ip, [r1]
    ldr     lr, [r1, #4]
    cmp     r2, ip
    it      eq
    cmpeq   r3, lr
    beq     L0
    movs    r0, #0
    pop     {r4, pc}

L0
    ldr     r2, [r1, #8]
    ldr     r3, [r1, #12]
    cmp     r4, r2
    it      eq
    cmpeq   r0, r3
    bne     L2
    movs    r0, #1
    pop     {r4, pc}

L1
    orrs    r2, r2, r3
    orrs    r4, r4, r0
    movs    r0, #1
    orrs    r2, r2, r4

L2
    it      ne
    movne   r0, #0
    pop     {r4, pc}

    END
