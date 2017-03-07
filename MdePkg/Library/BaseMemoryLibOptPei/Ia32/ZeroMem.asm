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
;   ZeroMem.Asm
;
; Abstract:
;
;   ZeroMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemZeroMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count
;    );
;------------------------------------------------------------------------------
InternalMemZeroMem  PROC    USES    edi
    xor     eax, eax
    mov     edi, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edx, ecx
    shr     ecx, 2
    and     edx, 3
    push    edi
    rep     stosd
    mov     ecx, edx
    rep     stosb
    pop     eax
    ret
InternalMemZeroMem  ENDP

    END
