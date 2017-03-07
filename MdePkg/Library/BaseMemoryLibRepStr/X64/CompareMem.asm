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
;   CompareMem.Asm
;
; Abstract:
;
;   CompareMem function
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
; INTN
; EFIAPI
; InternalMemCompareMem (
;   IN      CONST VOID                *DestinationBuffer,
;   IN      CONST VOID                *SourceBuffer,
;   IN      UINTN                     Length
;   );
;------------------------------------------------------------------------------
InternalMemCompareMem   PROC    USES    rsi rdi
    mov     rsi, rcx
    mov     rdi, rdx
    mov     rcx, r8
    repe    cmpsb
    movzx   rax, byte ptr [rsi - 1]
    movzx   rdx, byte ptr [rdi - 1]
    sub     rax, rdx
    ret
InternalMemCompareMem   ENDP

    END
