//------------------------------------------------------------------------------
//
// Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
// Copyright (c) 2018, Pete Batard. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------


    EXPORT  __aeabi_uidiv
    EXPORT  __aeabi_uidivmod
    EXPORT  __aeabi_idiv
    EXPORT  __aeabi_idivmod
    EXPORT  __rt_udiv
    EXPORT  __rt_sdiv

    AREA  Math, CODE, READONLY

;
;UINT32
;EFIAPI
;__aeabi_uidivmod (
;  IN UINT32  Dividend
;  IN UINT32  Divisor
;  );
;
__aeabi_uidiv
__aeabi_uidivmod
    RSBS    r12, r1, r0, LSR #4
    MOV     r2, #0
    BCC     __arm_div4
    RSBS    r12, r1, r0, LSR #8
    BCC     __arm_div8
    MOV     r3, #0
    B       __arm_div_large

;
;UINT64
;EFIAPI
;__rt_udiv (
;  IN UINT32  Divisor,
;  IN UINT32  Dividend
;  );
;
__rt_udiv
    ; Swap R0 and R1
    MOV     r12, r0
    MOV     r0, r1
    MOV     r1, r12
    B       __aeabi_uidivmod

;
;UINT64
;EFIAPI
;__rt_sdiv (
;  IN INT32  Divisor,
;  IN INT32  Dividend
;  );
;
__rt_sdiv
    ; Swap R0 and R1
    MOV     r12, r0
    MOV     r0, r1
    MOV     r1, r12
    B       __aeabi_idivmod

;
;INT32
;EFIAPI
;__aeabi_idivmod (
;  IN INT32  Dividend
;  IN INT32  Divisor
;  );
;
__aeabi_idiv
__aeabi_idivmod
    ORRS    r12, r0, r1
    BMI     __arm_div_negative
    RSBS    r12, r1, r0, LSR #1
    MOV     r2, #0
    BCC     __arm_div1
    RSBS    r12, r1, r0, LSR #4
    BCC     __arm_div4
    RSBS    r12, r1, r0, LSR #8
    BCC     __arm_div8
    MOV     r3, #0
    B       __arm_div_large
__arm_div8
    RSBS    r12, r1, r0, LSR #7
    SUBCS   r0, r0, r1, LSL #7
    ADC     r2, r2, r2
    RSBS    r12, r1, r0,LSR #6
    SUBCS   r0, r0, r1, LSL #6
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #5
    SUBCS   r0, r0, r1, LSL #5
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #4
    SUBCS   r0, r0, r1, LSL #4
    ADC     r2, r2, r2
__arm_div4
    RSBS    r12, r1, r0, LSR #3
    SUBCS   r0, r0, r1, LSL #3
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #2
    SUBCS   r0, r0, r1, LSL #2
    ADCS    r2, r2, r2
    RSBS    r12, r1, r0, LSR #1
    SUBCS   r0, r0, r1, LSL #1
    ADC     r2, r2, r2
__arm_div1
    SUBS    r1, r0, r1
    MOVCC   r1, r0
    ADC     r0, r2, r2
    BX      r14
__arm_div_negative
    ANDS    r2, r1, #0x80000000
    RSBMI   r1, r1, #0
    EORS    r3, r2, r0, ASR #32
    RSBCS   r0, r0, #0
    RSBS    r12, r1, r0, LSR #4
    BCC     label1
    RSBS    r12, r1, r0, LSR #8
    BCC     label2
__arm_div_large
    LSL     r1, r1, #6
    RSBS    r12, r1, r0, LSR #8
    ORR     r2, r2, #0xfc000000
    BCC     label2
    LSL     r1, r1, #6
    RSBS    r12, r1, r0, LSR #8
    ORR     r2, r2, #0x3f00000
    BCC     label2
    LSL     r1, r1, #6
    RSBS    r12, r1, r0, LSR #8
    ORR     r2, r2, #0xfc000
    ORRCS   r2, r2, #0x3f00
    LSLCS   r1, r1, #6
    RSBS    r12, r1, #0
    BCS     __aeabi_idiv0
label3
    LSRCS   r1, r1, #6
label2
    RSBS    r12, r1, r0, LSR #7
    SUBCS   r0, r0, r1, LSL #7
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #6
    SUBCS   r0, r0, r1, LSL #6
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #5
    SUBCS   r0, r0, r1, LSL #5
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #4
    SUBCS   r0, r0, r1, LSL #4
    ADC     r2, r2, r2
label1
    RSBS    r12, r1, r0, LSR #3
    SUBCS   r0, r0, r1, LSL #3
    ADC     r2, r2, r2
    RSBS    r12, r1, r0, LSR #2
    SUBCS   r0, r0, r1, LSL #2
    ADCS    r2, r2, r2
    BCS     label3
    RSBS    r12, r1, r0, LSR #1
    SUBCS   r0, r0, r1, LSL #1
    ADC     r2, r2, r2
    SUBS    r1, r0, r1
    MOVCC   r1, r0
    ADC     r0, r2, r2
    ASRS    r3, r3, #31
    RSBMI   r0, r0, #0
    RSBCS   r1, r1, #0
    BX      r14

    ; What to do about division by zero?  For now, just return.
__aeabi_idiv0
    BX      r14

    END
