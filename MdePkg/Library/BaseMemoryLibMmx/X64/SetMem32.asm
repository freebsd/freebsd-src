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
;   SetMem32.asm
;
; Abstract:
;
;   SetMem32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem32 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT32 Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem32 PROC
    DB      49h, 0fh, 6eh, 0c0h         ; movd mm0, r8 (Value)
    mov     rax, rcx                    ; rax <- Buffer
    xchg    rcx, rdx                    ; rcx <- Count  rdx <- Buffer
    shr     rcx, 1                      ; rcx <- # of qwords to set
    jz      @SetDwords
    DB      0fh, 70h, 0C0h, 44h         ; pshufw mm0, mm0, 44h
@@:
    DB      0fh, 0e7h, 02h              ; movntq [rdx], mm0
    lea     rdx, [rdx + 8]              ; use "lea" to avoid flag changes
    loop    @B
    mfence
@SetDwords:
    jnc     @F
    DB      0fh, 7eh, 02h               ; movd [rdx], mm0
@@:
    ret
InternalMemSetMem32 ENDP

    END
