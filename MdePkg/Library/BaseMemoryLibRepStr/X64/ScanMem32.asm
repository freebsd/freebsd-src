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
;   ScanMem32.Asm
;
; Abstract:
;
;   ScanMem32 function
;
; Notes:
;
;   The following BaseMemoryLib instances contain the same copy of this file:
;
;       BaseMemoryLibRepStr
;       BaseMemoryLibMmx
;       BaseMemoryLibSse2
;       BaseMemoryLibOptDxe
;       BaseMemoryLibOptPei
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
; CONST VOID *
; EFIAPI
; InternalMemScanMem32 (
;   IN      CONST VOID                *Buffer,
;   IN      UINTN                     Length,
;   IN      UINT32                    Value
;   );
;------------------------------------------------------------------------------
InternalMemScanMem32    PROC    USES    rdi
    mov     rdi, rcx
    mov     rax, r8
    mov     rcx, rdx
    repne   scasd
    lea     rax, [rdi - 4]
    cmovnz  rax, rcx
    ret
InternalMemScanMem32    ENDP

    END
