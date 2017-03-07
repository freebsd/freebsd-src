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
;   SetMem16.nasm
;
; Abstract:
;
;   SetMem16 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID *
; EFIAPI
; InternalMemSetMem16 (
;   OUT     VOID                      *Buffer,
;   IN      UINTN                     Length,
;   IN      UINT16                    Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem16)
ASM_PFX(InternalMemSetMem16):
    push    rdi
    mov     rax, r8
    DB      0x48, 0xf, 0x6e, 0xc0         ; movd mm0, rax
    mov     r8, rcx
    mov     rdi, r8
    mov     rcx, rdx
    and     edx, 3
    shr     rcx, 2
    jz      @SetWords
    DB      0xf, 0x70, 0xC0, 0x0         ; pshufw mm0, mm0, 0h
.0:
    DB      0xf, 0xe7, 0x7              ; movntq [rdi], mm0
    add     rdi, 8
    loop    .0
    mfence
@SetWords:
    mov     ecx, edx
    rep     stosw
    mov     rax, r8
    pop     rdi
    ret

