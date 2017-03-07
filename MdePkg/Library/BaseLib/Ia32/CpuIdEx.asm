;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
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
;   CpuIdEx.Asm
;
; Abstract:
;
;   AsmCpuidEx function
;
; Notes:
;
;------------------------------------------------------------------------------

    .686
    .model  flat,C
    .code

;------------------------------------------------------------------------------
;  UINT32
;  EFIAPI
;  AsmCpuidEx (
;    IN   UINT32  RegisterInEax,
;    IN   UINT32  RegisterInEcx,
;    OUT  UINT32  *RegisterOutEax  OPTIONAL,
;    OUT  UINT32  *RegisterOutEbx  OPTIONAL,
;    OUT  UINT32  *RegisterOutEcx  OPTIONAL,
;    OUT  UINT32  *RegisterOutEdx  OPTIONAL
;    )
;------------------------------------------------------------------------------
AsmCpuidEx  PROC    USES    ebx
    push    ebp
    mov     ebp, esp
    mov     eax, [ebp + 12]
    mov     ecx, [ebp + 16]
    cpuid
    push    ecx
    mov     ecx, [ebp + 20]
    jecxz   @F
    mov     [ecx], eax
@@:
    mov     ecx, [ebp + 24]
    jecxz   @F
    mov     [ecx], ebx
@@:
    mov     ecx, [ebp + 32]
    jecxz   @F
    mov     [ecx], edx
@@:
    mov     ecx, [ebp + 28]
    jecxz   @F
    pop     DWORD [ecx]
@@:
    mov     eax, [ebp + 12]
    leave
    ret
AsmCpuidEx  ENDP

    END
