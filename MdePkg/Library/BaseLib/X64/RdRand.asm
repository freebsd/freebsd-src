;------------------------------------------------------------------------------
;
; Copyright (c) 2015 - 2016, Intel Corporation. All rights reserved.<BR>
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
;   RdRand.asm
;
; Abstract:
;
;   Generates random number through CPU RdRand instruction under 64-bit platform.
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  Generates a 16 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand16 (UINT16 *Rand);
;------------------------------------------------------------------------------
InternalX86RdRand16  PROC
    ; rdrand   ax                  ; generate a 16 bit RN into eax,
                                   ; CF=1 if RN generated ok, otherwise CF=0
    db     0fh, 0c7h, 0f0h         ; rdrand r16: "0f c7 /6  ModRM:r/m(w)"
    jc     rn16_ok                 ; jmp if CF=1
    xor    rax, rax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn16_ok:
    mov    [rcx], ax
    mov    rax,  1
    ret
InternalX86RdRand16 ENDP

;------------------------------------------------------------------------------
;  Generates a 32 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand32 (UINT32 *Rand);
;------------------------------------------------------------------------------
InternalX86RdRand32  PROC
    ; rdrand   eax                 ; generate a 32 bit RN into eax,
                                   ; CF=1 if RN generated ok, otherwise CF=0
    db     0fh, 0c7h, 0f0h         ; rdrand r32: "0f c7 /6  ModRM:r/m(w)"
    jc     rn32_ok                 ; jmp if CF=1
    xor    rax, rax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn32_ok:
    mov    [rcx], eax
    mov    rax,  1
    ret
InternalX86RdRand32 ENDP

;------------------------------------------------------------------------------
;  Generates a 64 bit random number through one RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand64 (UINT64 *Random);
;------------------------------------------------------------------------------
InternalX86RdRand64  PROC
    ; rdrand   rax                 ; generate a 64 bit RN into rax,
                                   ; CF=1 if RN generated ok, otherwise CF=0
    db     048h, 0fh, 0c7h, 0f0h   ; rdrand r64: "REX.W + 0f c7 /6 ModRM:r/m(w)"
    jc     rn64_ok                 ; jmp if CF=1
    xor    rax, rax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn64_ok:
    mov    [rcx], rax
    mov    rax, 1
    ret
InternalX86RdRand64 ENDP

    END
