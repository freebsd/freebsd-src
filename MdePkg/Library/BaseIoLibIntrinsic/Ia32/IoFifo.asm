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

    .586P
    .model  flat,C
    .code

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo8 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
IoReadFifo8 PROC
    push    edi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]
rep insb
    pop     edi
    ret
IoReadFifo8 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo16 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
IoReadFifo16 PROC
    push    edi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]
rep insw
    pop     edi
    ret
IoReadFifo16 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo32 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
IoReadFifo32 PROC
    push    edi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]
rep insd
    pop     edi
    ret
IoReadFifo32 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
IoWriteFifo8 PROC
    push    esi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]
rep outsb
    pop     esi
    ret
IoWriteFifo8 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo16 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
IoWriteFifo16 PROC
    push    esi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]
rep outsw
    pop     esi
    ret
IoWriteFifo16 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo32 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
IoWriteFifo32 PROC
    push    esi
    cld
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]
rep outsd
    pop     esi
    ret
IoWriteFifo32 ENDP

    END

