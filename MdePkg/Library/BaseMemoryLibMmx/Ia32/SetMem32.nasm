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
;   SetMem32.nasm
;
; Abstract:
;
;   SetMem32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem32 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT32 Value
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem32)
ASM_PFX(InternalMemSetMem32):
    mov     eax, [esp + 4]              ; eax <- Buffer as return value
    mov     ecx, [esp + 8]              ; ecx <- Count
    movd    mm0, dword [esp + 12]             ; mm0 <- Value
    shr     ecx, 1                      ; ecx <- number of qwords to set
    mov     edx, eax                    ; edx <- Buffer
    jz      @SetDwords
    movq    mm1, mm0
    psllq   mm1, 32
    por     mm0, mm1
.0:
    movq    qword [edx], mm0
    lea     edx, [edx + 8]              ; use "lea" to avoid change in flags
    loop    .0
@SetDwords:
    jnc     .1
    movd    dword [edx], mm0
.1:
    ret

