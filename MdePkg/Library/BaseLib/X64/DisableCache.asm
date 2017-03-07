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
;   DisableCache.Asm
;
; Abstract:
;
;   Set the CD bit of CR0 to 1, clear the NW bit of CR0 to 0, and flush all caches with a
;   WBINVD instruction.
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmDisableCache (
;   VOID
;   );
;------------------------------------------------------------------------------
AsmDisableCache PROC
    mov     rax, cr0
    bts     rax, 30
    btr     rax, 29
    mov     cr0, rax
    wbinvd
    ret
AsmDisableCache ENDP

    END
