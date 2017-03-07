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
;   DivU64x64Remainder.asm
;
; Abstract:
;
;   Calculate the quotient of a 64-bit integer by a 64-bit integer and returns
;   both the quotient and the remainder
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

EXTERN  InternalMathDivRemU64x32:PROC

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathDivRemU64x64 (
;   IN      UINT64                    Dividend,
;   IN      UINT64                    Divisor,
;   OUT     UINT64                    *Remainder    OPTIONAL
;   );
;------------------------------------------------------------------------------
InternalMathDivRemU64x64    PROC
    mov     ecx, [esp + 16]             ; ecx <- divisor[32..63]
    test    ecx, ecx
    jnz     _@DivRemU64x64              ; call _@DivRemU64x64 if Divisor > 2^32
    mov     ecx, [esp + 20]
    jecxz   @F
    and     dword ptr [ecx + 4], 0      ; zero high dword of remainder
    mov     [esp + 16], ecx             ; set up stack frame to match DivRemU64x32
@@:
    jmp     InternalMathDivRemU64x32
InternalMathDivRemU64x64    ENDP

_@DivRemU64x64  PROC PRIVATE    USES    ebx esi edi
    mov     edx, dword ptr [esp + 20]
    mov     eax, dword ptr [esp + 16]   ; edx:eax <- dividend
    mov     edi, edx
    mov     esi, eax                    ; edi:esi <- dividend
    mov     ebx, dword ptr [esp + 24]   ; ecx:ebx <- divisor
@@:
    shr     edx, 1
    rcr     eax, 1
    shrd    ebx, ecx, 1
    shr     ecx, 1
    jnz     @B
    div     ebx
    mov     ebx, eax                    ; ebx <- quotient
    mov     ecx, [esp + 28]             ; ecx <- high dword of divisor
    mul     dword ptr [esp + 24]        ; edx:eax <- quotient * divisor[0..31]
    imul    ecx, ebx                    ; ecx <- quotient * divisor[32..63]
    add     edx, ecx                    ; edx <- (quotient * divisor)[32..63]
    mov     ecx, dword ptr [esp + 32]   ; ecx <- addr for Remainder
    jc      @TooLarge                   ; product > 2^64
    cmp     edi, edx                    ; compare high 32 bits
    ja      @Correct
    jb      @TooLarge                   ; product > dividend
    cmp     esi, eax
    jae     @Correct                    ; product <= dividend
@TooLarge:
    dec     ebx                         ; adjust quotient by -1
    jecxz   @Return                     ; return if Remainder == NULL
    sub     eax, dword ptr [esp + 24]
    sbb     edx, dword ptr [esp + 28]   ; edx:eax <- (quotient - 1) * divisor
@Correct:
    jecxz   @Return
    sub     esi, eax
    sbb     edi, edx                    ; edi:esi <- remainder
    mov     [ecx], esi
    mov     [ecx + 4], edi
@Return:
    mov     eax, ebx                    ; eax <- quotient
    xor     edx, edx                    ; quotient is 32 bits long
    ret
_@DivRemU64x64  ENDP

    END
