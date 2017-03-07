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

    .686
    .model  flat,C
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
InternalMemCompareMem   PROC    USES    esi edi
    mov     esi, [esp + 12]
    mov     edi, [esp + 16]
    mov     ecx, [esp + 20]
    repe    cmpsb
    movzx   eax, byte ptr [esi - 1]
    movzx   edx, byte ptr [edi - 1]
    sub     eax, edx
    ret
InternalMemCompareMem   ENDP

    END
