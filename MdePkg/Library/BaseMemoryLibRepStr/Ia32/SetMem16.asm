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
;   SetMem16.Asm
;
; Abstract:
;
;   SetMem16 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem16 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT16 Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem16 PROC    USES    edi
    mov     eax, [esp + 16]
    mov     edi, [esp + 8]
    mov     ecx, [esp + 12]
    rep     stosw
    mov     eax, [esp + 8]
    ret
InternalMemSetMem16 ENDP

    END
