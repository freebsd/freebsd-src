;------------------------------------------------------------------------------
;
; Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
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
;   IsZeroBuffer.nasm
;
; Abstract:
;
;   IsZeroBuffer function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  BOOLEAN
;  EFIAPI
;  InternalMemIsZeroBuffer (
;    IN CONST VOID  *Buffer,
;    IN UINTN       Length
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemIsZeroBuffer)
ASM_PFX(InternalMemIsZeroBuffer):
    push         edi
    mov          edi, [esp + 8]        ; edi <- Buffer
    mov          edx, [esp + 12]       ; edx <- Length
    xor          ecx, ecx              ; ecx <- 0
    sub          ecx, edi
    and          ecx, 15               ; ecx + edi aligns on 16-byte boundary
    jz           @Is16BytesZero
    cmp          ecx, edx
    cmova        ecx, edx              ; bytes before the 16-byte boundary
    sub          edx, ecx
    xor          eax, eax              ; eax <- 0, also set ZF
    repe         scasb
    jnz          @ReturnFalse          ; ZF=0 means non-zero element found
@Is16BytesZero:
    mov          ecx, edx
    and          edx, 15
    shr          ecx, 4
    jz           @IsBytesZero
.0:
    pxor         xmm0, xmm0            ; xmm0 <- 0
    pcmpeqb      xmm0, [edi]           ; check zero for 16 bytes
    pmovmskb     eax, xmm0             ; eax <- compare results
    cmp          eax, 0xffff
    jnz          @ReturnFalse
    add          edi, 16
    loop         .0
@IsBytesZero:
    mov          ecx, edx
    xor          eax, eax              ; eax <- 0, also set ZF
    repe         scasb
    jnz          @ReturnFalse          ; ZF=0 means non-zero element found
    pop          edi
    mov          eax, 1                ; return TRUE
    ret
@ReturnFalse:
    pop          edi
    xor          eax, eax
    ret                                ; return FALSE

