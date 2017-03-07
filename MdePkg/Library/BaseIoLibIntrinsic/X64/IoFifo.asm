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

    .code

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo8 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
IoReadFifo8 PROC
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
rep insb
    mov     rdi, r8             ; restore rdi
    ret
IoReadFifo8 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo16 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
IoReadFifo16 PROC
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
rep insw
    mov     rdi, r8             ; restore rdi
    ret
IoReadFifo16 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo32 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
IoReadFifo32 PROC
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
rep insd
    mov     rdi, r8             ; restore rdi
    ret
IoReadFifo32 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
IoWriteFifo8 PROC
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsb
    mov     rsi, r8             ; restore rsi
    ret
IoWriteFifo8 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo16 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
IoWriteFifo16 PROC
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsw
    mov     rsi, r8             ; restore rsi
    ret
IoWriteFifo16 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo32 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
IoWriteFifo32 PROC
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsd
    mov     rsi, r8             ; restore rsi
    ret
IoWriteFifo32 ENDP

    END

