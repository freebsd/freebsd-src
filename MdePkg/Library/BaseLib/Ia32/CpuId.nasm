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
;   CpuId.Asm
;
; Abstract:
;
;   AsmCpuid function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  AsmCpuid (
;    IN   UINT32  RegisterInEax,
;    OUT  UINT32  *RegisterOutEax  OPTIONAL,
;    OUT  UINT32  *RegisterOutEbx  OPTIONAL,
;    OUT  UINT32  *RegisterOutEcx  OPTIONAL,
;    OUT  UINT32  *RegisterOutEdx  OPTIONAL
;    );
;------------------------------------------------------------------------------
global ASM_PFX(AsmCpuid)
ASM_PFX(AsmCpuid):
    push    ebx
    push    ebp
    mov     ebp, esp
    mov     eax, [ebp + 12]
    cpuid
    push    ecx
    mov     ecx, [ebp + 16]
    jecxz   .0
    mov     [ecx], eax
.0:
    mov     ecx, [ebp + 20]
    jecxz   .1
    mov     [ecx], ebx
.1:
    mov     ecx, [ebp + 24]
    jecxz   .2
    pop     DWORD [ecx]
.2:
    mov     ecx, [ebp + 28]
    jecxz   .3
    mov     [ecx], edx
.3:
    mov     eax, [ebp + 12]
    leave
    pop     ebx
    ret

