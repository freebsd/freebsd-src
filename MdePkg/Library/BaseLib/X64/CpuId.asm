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
;   CpuId.Asm
;
; Abstract:
;
;   AsmCpuid function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  AsmCpuid (
;    IN   UINT32  RegisterInEax,
;    OUT  UINT32  *RegisterOutEax  OPTIONAL,
;    OUT  UINT32  *RegisterOutEbx  OPTIONAL,
;    OUT  UINT32  *RegisterOutEcx  OPTIONAL,
;    OUT  UINT32  *RegisterOutEdx  OPTIONAL
;    )
;------------------------------------------------------------------------------
AsmCpuid    PROC    USES    rbx
    mov     eax, ecx
    push    rax                         ; save Index on stack
    push    rdx
    cpuid
    test    r9, r9
    jz      @F
    mov     [r9], ecx
@@:
    pop     rcx
    jrcxz   @F
    mov     [rcx], eax
@@:
    mov     rcx, r8
    jrcxz   @F
    mov     [rcx], ebx
@@:
    mov     rcx, [rsp + 38h]
    jrcxz   @F
    mov     [rcx], edx
@@:
    pop     rax                         ; restore Index to rax as return value
    ret
AsmCpuid    ENDP

    END
