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
;   ReadMsr64.Asm
;
; Abstract:
;
;   AsmReadMsr64 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMsr64 (
;   IN UINT32  Index
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMsr64)
ASM_PFX(AsmReadMsr64):
    rdmsr                               ; edx & eax are zero extended
    shl     rdx, 0x20
    or      rax, rdx
    ret

