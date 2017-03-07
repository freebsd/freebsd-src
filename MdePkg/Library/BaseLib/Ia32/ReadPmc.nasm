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
;   ReadPmc.Asm
;
; Abstract:
;
;   AsmReadPmc function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadPmc (
;   IN UINT32   PmcIndex
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadPmc)
ASM_PFX(AsmReadPmc):
    mov     ecx, [esp + 4]
    rdpmc
    ret

