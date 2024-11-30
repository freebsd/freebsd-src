;------------------------------------------------------------------------------
;
; Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   VmgExitSvsm.Asm
;
; Abstract:
;
;   AsmVmgExitSvsm function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; typedef struct {
;   VOID      *Caa;
;   UINT64    RaxIn;
;   UINT64    RcxIn;
;   UINT64    RdxIn;
;   UINT64    R8In;
;   UINT64    R9In;
;   UINT64    RaxOut;
;   UINT64    RcxOut;
;   UINT64    RdxOut;
;   UINT64    R8Out;
;   UINT64    R9Out;
;   UINT8     *CallPending;
; } SVSM_CALL_DATA;
;
; UINT32
; EFIAPI
; AsmVmgExitSvsm (
;   SVSM_CALL_DATA *SvsmCallData
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmVmgExitSvsm)
ASM_PFX(AsmVmgExitSvsm):
    push    r10
    push    r11
    push    r12

;
; Calling convention has SvsmCallData in RCX. Move RCX to R12 in order to
; properly populate the SVSM register state.
;
    mov     r12, rcx

    mov     rax, [r12 + 8]
    mov     rcx, [r12 + 16]
    mov     rdx, [r12 + 24]
    mov     r8,  [r12 + 32]
    mov     r9,  [r12 + 40]

;
; Set CA call pending
;
    mov     r10, [r12]
    mov     byte [r10], 1

    rep     vmmcall

    mov     [r12 + 48], rax
    mov     [r12 + 56], rcx
    mov     [r12 + 64], rdx
    mov     [r12 + 72], r8
    mov     [r12 + 80], r9

;
; Perform the atomic exchange and return the CA call pending value.
; The call pending value is a one-byte field at offset 0 into the CA,
; which is currently the value in R10.
;

    mov     r11, [r12 + 88]     ; Get CallPending address
    mov     cl, byte [r11]
    xchg    byte [r10], cl
    mov     byte [r11], cl      ; Return the exchanged value

    pop     r12
    pop     r11
    pop     r10

;
; RAX has the value to be returned from the SVSM
;
    ret

