;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   SetJump.Asm
;
; Abstract:
;
;   Implementation of SetJump() on IA-32.
;
;------------------------------------------------------------------------------

%include "Nasm.inc"

    SECTION .text

extern ASM_PFX(InternalAssertJumpBuffer)
extern ASM_PFX(PcdGet32 (PcdControlFlowEnforcementPropertyMask))

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; SetJump (
;   OUT     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
;   );
;------------------------------------------------------------------------------
global ASM_PFX(SetJump)
ASM_PFX(SetJump):
    push    DWORD [esp + 4]
    call    ASM_PFX(InternalAssertJumpBuffer)    ; To validate JumpBuffer
    pop     ecx
    pop     ecx                         ; ecx <- return address
    mov     edx, [esp]

    xor     eax, eax
    mov     [edx + 24], eax        ; save 0 to SSP

    mov     eax, [ASM_PFX(PcdGet32 (PcdControlFlowEnforcementPropertyMask))]
    test    eax, eax
    jz      CetDone
    mov     eax, cr4
    bt      eax, 23                ; check if CET is enabled
    jnc     CetDone

    mov     eax, 1
    incsspd eax                    ; to read original SSP
    rdsspd  eax
    mov     [edx + 0x24], eax      ; save SSP

CetDone:

    mov     [edx], ebx
    mov     [edx + 4], esi
    mov     [edx + 8], edi
    mov     [edx + 12], ebp
    mov     [edx + 16], esp
    mov     [edx + 20], ecx             ; eip value to restore in LongJump
    xor     eax, eax
    jmp     ecx

