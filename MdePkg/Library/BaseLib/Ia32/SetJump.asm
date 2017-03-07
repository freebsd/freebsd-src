;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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

    .386
    .model  flat,C
    .code

InternalAssertJumpBuffer    PROTO   C

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; SetJump (
;   OUT     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
;   );
;------------------------------------------------------------------------------
SetJump     PROC
    push    DWORD [esp + 4]
    call    InternalAssertJumpBuffer    ; To validate JumpBuffer
    pop     ecx
    pop     ecx                         ; ecx <- return address
    mov     edx, [esp]
    mov     [edx], ebx
    mov     [edx + 4], esi
    mov     [edx + 8], edi
    mov     [edx + 12], ebp
    mov     [edx + 16], esp
    mov     [edx + 20], ecx             ; eip value to restore in LongJump
    xor     eax, eax
    jmp     ecx
SetJump     ENDP

    END
