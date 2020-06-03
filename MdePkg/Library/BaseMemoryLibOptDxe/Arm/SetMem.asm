;------------------------------------------------------------------------------
;
; Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
;
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

    EXPORT  InternalMemZeroMem
    EXPORT  InternalMemSetMem
    EXPORT  InternalMemSetMem16
    EXPORT  InternalMemSetMem32
    EXPORT  InternalMemSetMem64

    AREA    SetMem, CODE, READONLY, CODEALIGN, ALIGN=5
    THUMB

InternalMemSetMem16
    uxth    r2, r2
    lsl     r1, r1, #1
    orr     r2, r2, r2, lsl #16
    b       B0

InternalMemSetMem32
    lsl     r1, r1, #2
    b       B0

InternalMemSetMem64
    lsl     r1, r1, #3
    b       B1

    ALIGN   32
InternalMemSetMem
    uxtb    r2, r2
    orr     r2, r2, r2, lsl #8
    orr     r2, r2, r2, lsl #16
    b       B0

InternalMemZeroMem
    movs    r2, #0
B0
    mov     r3, r2

B1
    push    {r4, lr}
    cmp     r1, #16                 ; fewer than 16 bytes of input?
    add     r1, r1, r0              ; r1 := dst + length
    add     lr, r0, #16
    blt     L2
    bic     lr, lr, #15             ; align output pointer

    str     r2, [r0]                ; potentially unaligned store of 4 bytes
    str     r3, [r0, #4]            ; potentially unaligned store of 4 bytes
    str     r2, [r0, #8]            ; potentially unaligned store of 4 bytes
    str     r3, [r0, #12]           ; potentially unaligned store of 4 bytes
    beq     L1

L0
    add     lr, lr, #16             ; advance the output pointer by 16 bytes
    subs    r4, r1, lr              ; past the output?
    blt     L3                      ; break out of the loop
    strd    r2, r3, [lr, #-16]      ; aligned store of 16 bytes
    strd    r2, r3, [lr, #-8]
    bne     L0                      ; goto beginning of loop
L1
    pop     {r4, pc}

L2
    subs    r4, r1, lr
L3
    adds    r4, r4, #16
    subs    r1, r1, #8
    cmp     r4, #4                  ; between 4 and 15 bytes?
    blt     L4
    cmp     r4, #8                  ; between 8 and 15 bytes?
    str     r2, [lr, #-16]          ; overlapping store of 4 + (4 + 4) + 4 bytes
    itt     gt
    strgt   r3, [lr, #-12]
    strgt   r2, [r1]
    str     r3, [r1, #4]
    pop     {r4, pc}

L4
    cmp     r4, #2                  ; 2 or 3 bytes?
    strb    r2, [lr, #-16]          ; store 1 byte
    it      ge
    strhge  r2, [r1, #6]            ; store 2 bytes
    pop     {r4, pc}

    END
