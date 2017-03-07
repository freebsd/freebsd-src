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
;   RdRand.nasm
;
; Abstract:
;
;   Generates random number through CPU RdRand instruction under 64-bit platform.
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  Generates a 16 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand16 (UINT16 *Rand);
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86RdRand16)
ASM_PFX(InternalX86RdRand16):
    ; rdrand   ax                  ; generate a 16 bit RN into eax,
                                   ; CF=1 if RN generated ok, otherwise CF=0
    db     0xf, 0xc7, 0xf0         ; rdrand r16: "0f c7 /6  ModRM:r/m(w)"
    jc     rn16_ok                 ; jmp if CF=1
    xor    rax, rax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn16_ok:
    mov    [rcx], ax
    mov    rax,  1
    ret

;------------------------------------------------------------------------------
;  Generates a 32 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand32 (UINT32 *Rand);
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86RdRand32)
ASM_PFX(InternalX86RdRand32):
    ; rdrand   eax                 ; generate a 32 bit RN into eax,
                                   ; CF=1 if RN generated ok, otherwise CF=0
    db     0xf, 0xc7, 0xf0         ; rdrand r32: "0f c7 /6  ModRM:r/m(w)"
    jc     rn32_ok                 ; jmp if CF=1
    xor    rax, rax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn32_ok:
    mov    [rcx], eax
    mov    rax,  1
    ret

;------------------------------------------------------------------------------
;  Generates a 64 bit random number through one RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand64 (UINT64 *Random);
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86RdRand64)
ASM_PFX(InternalX86RdRand64):
    ; rdrand   rax                 ; generate a 64 bit RN into rax,
                                   ; CF=1 if RN generated ok, otherwise CF=0
    db     0x48, 0xf, 0xc7, 0xf0   ; rdrand r64: "REX.W + 0f c7 /6 ModRM:r/m(w)"
    jc     rn64_ok                 ; jmp if CF=1
    xor    rax, rax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn64_ok:
    mov    [rcx], rax
    mov    rax, 1
    ret

