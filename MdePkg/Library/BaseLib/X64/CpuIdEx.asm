;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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
AsmCpuidEx  PROC    USES    rbx
    mov     eax, ecx
    mov     ecx, edx
    push    rax                         ; save Index on stack
    cpuid
    mov     r10, [rsp + 38h]
    test    r10, r10
    jz      @F
    mov     [r10], ecx
@@:
    mov     rcx, r8
    jrcxz   @F
    mov     [rcx], eax
@@:
    mov     rcx, r9
    jrcxz   @F
    mov     [rcx], ebx
@@:
    mov     rcx, [rsp + 40h]
    jrcxz   @F
    mov     [rcx], edx
@@:
    pop     rax                         ; restore Index to rax as return value
    ret
AsmCpuidEx  ENDP

    END
