;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   DivU64x64Remainder.nasm
;
; Abstract:
;
;   Calculate the quotient of a 64-bit integer by a 64-bit integer and returns
;   both the quotient and the remainder
;
;------------------------------------------------------------------------------

    SECTION .text

extern ASM_PFX(InternalMathDivRemU64x32)

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathDivRemU64x64 (
;   IN      UINT64                    Dividend,
;   IN      UINT64                    Divisor,
;   OUT     UINT64                    *Remainder    OPTIONAL
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathDivRemU64x64)
ASM_PFX(InternalMathDivRemU64x64):
    mov     ecx, [esp + 16]             ; ecx <- divisor[32..63]
    test    ecx, ecx
    jnz     _@DivRemU64x64              ; call _@DivRemU64x64 if Divisor > 2^32
    mov     ecx, [esp + 20]
    jecxz   .0
    and     dword [ecx + 4], 0      ; zero high dword of remainder
    mov     [esp + 16], ecx             ; set up stack frame to match DivRemU64x32
.0:
    jmp     ASM_PFX(InternalMathDivRemU64x32)

_@DivRemU64x64:
    push    ebx
    push    esi
    push    edi
    mov     edx, dword [esp + 20]
    mov     eax, dword [esp + 16]   ; edx:eax <- dividend
    mov     edi, edx
    mov     esi, eax                    ; edi:esi <- dividend
    mov     ebx, dword [esp + 24]   ; ecx:ebx <- divisor
.1:
    shr     edx, 1
    rcr     eax, 1
    shrd    ebx, ecx, 1
    shr     ecx, 1
    jnz     .1
    div     ebx
    mov     ebx, eax                    ; ebx <- quotient
    mov     ecx, [esp + 28]             ; ecx <- high dword of divisor
    mul     dword [esp + 24]        ; edx:eax <- quotient * divisor[0..31]
    imul    ecx, ebx                    ; ecx <- quotient * divisor[32..63]
    add     edx, ecx                    ; edx <- (quotient * divisor)[32..63]
    mov     ecx, dword [esp + 32]   ; ecx <- addr for Remainder
    jc      @TooLarge                   ; product > 2^64
    cmp     edi, edx                    ; compare high 32 bits
    ja      @Correct
    jb      @TooLarge                   ; product > dividend
    cmp     esi, eax
    jae     @Correct                    ; product <= dividend
@TooLarge:
    dec     ebx                         ; adjust quotient by -1
    jecxz   @Return                     ; return if Remainder == NULL
    sub     eax, dword [esp + 24]
    sbb     edx, dword [esp + 28]   ; edx:eax <- (quotient - 1) * divisor
@Correct:
    jecxz   @Return
    sub     esi, eax
    sbb     edi, edx                    ; edi:esi <- remainder
    mov     [ecx], esi
    mov     [ecx + 4], edi
@Return:
    mov     eax, ebx                    ; eax <- quotient
    xor     edx, edx                    ; quotient is 32 bits long
    pop     edi
    pop     esi
    pop     ebx
    ret

