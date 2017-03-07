;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
; Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>
;
; This program and the accompanying materials are licensed and made available
; under the terms and conditions of the BSD License which accompanies this
; distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo8 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo8)
ASM_PFX(IoReadFifo8):
    push    edi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]
rep insb
    pop     edi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo16 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo16)
ASM_PFX(IoReadFifo16):
    push    edi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]
rep insw
    pop     edi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo32 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo32)
ASM_PFX(IoReadFifo32):
    push    edi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]
rep insd
    pop     edi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo8)
ASM_PFX(IoWriteFifo8):
    push    esi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]
rep outsb
    pop     esi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo16 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo16)
ASM_PFX(IoWriteFifo16):
    push    esi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]
rep outsw
    pop     esi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo32 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo32)
ASM_PFX(IoWriteFifo32):
    push    esi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]
rep outsd
    pop     esi
    ret

