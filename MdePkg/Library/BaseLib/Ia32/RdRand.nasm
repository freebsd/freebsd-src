;------------------------------------------------------------------------------
;
; Copyright (c) 2015 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   RdRand.nasm
;
; Abstract:
;
;   Generates random number through CPU RdRand instruction under 32-bit platform.
;
; Notes:
;
;------------------------------------------------------------------------------

SECTION .text

;------------------------------------------------------------------------------
;  Generates a 16 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand16 (UINT16 *Rand);
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86RdRand16)
ASM_PFX(InternalX86RdRand16):
    rdrand eax                     ; generate a 16 bit RN into ax
                                   ; CF=1 if RN generated ok, otherwise CF=0
    jc     rn16_ok                 ; jmp if CF=1
    xor    eax, eax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn16_ok:
    mov    edx, dword [esp + 4]
    mov    [edx], ax
    mov    eax,  1
    ret

;------------------------------------------------------------------------------
;  Generates a 32 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand32 (UINT32 *Rand);
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86RdRand32)
ASM_PFX(InternalX86RdRand32):
    rdrand eax                     ; generate a 32 bit RN into eax
                                   ; CF=1 if RN generated ok, otherwise CF=0
    jc     rn32_ok                 ; jmp if CF=1
    xor    eax, eax                ; reg=0 if CF=0
    ret                            ; return with failure status
rn32_ok:
    mov    edx, dword [esp + 4]
    mov    [edx], eax
    mov    eax,  1
    ret

;------------------------------------------------------------------------------
;  Generates a 64 bit random number through RDRAND instruction.
;  Return TRUE if Rand generated successfully, or FALSE if not.
;
;  BOOLEAN EFIAPI InternalX86RdRand64 (UINT64 *Rand);
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86RdRand64)
ASM_PFX(InternalX86RdRand64):
    rdrand eax                     ; generate a 32 bit RN into eax
                                   ; CF=1 if RN generated ok, otherwise CF=0
    jnc    rn64_ret                ; jmp if CF=0
    mov    edx, dword [esp + 4]
    mov    [edx], eax

    rdrand eax                     ; generate another 32 bit RN
    jnc    rn64_ret                ; jmp if CF=0
    mov    [edx + 4], eax

    mov    eax,  1
    ret
rn64_ret:
    xor    eax, eax
    ret                            ; return with failure status

