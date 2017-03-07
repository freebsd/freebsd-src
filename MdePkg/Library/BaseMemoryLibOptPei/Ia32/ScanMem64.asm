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
;   ScanMem64.Asm
;
; Abstract:
;
;   ScanMem64 function
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
; CONST VOID *
; EFIAPI
; InternalMemScanMem64 (
;   IN      CONST VOID                *Buffer,
;   IN      UINTN                     Length,
;   IN      UINT64                    Value
;   );
;------------------------------------------------------------------------------
InternalMemScanMem64    PROC    USES    edi
    mov     ecx, [esp + 12]
    mov     eax, [esp + 16]
    mov     edx, [esp + 20]
    mov     edi, [esp + 8]
@@:
    cmp     eax, [edi]
    lea     edi, [edi + 8]
    loopne  @B
    jne     @F
    cmp     edx, [edi - 4]
    jecxz   @F
    jne     @B
@@:
    lea     eax, [edi - 8]
    cmovne  eax, ecx
    ret
InternalMemScanMem64    ENDP

    END
