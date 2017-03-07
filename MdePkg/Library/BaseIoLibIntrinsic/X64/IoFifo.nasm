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

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo8 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo8)
ASM_PFX(IoReadFifo8):
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
rep insb
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo16 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo16)
ASM_PFX(IoReadFifo16):
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
rep insw
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo32 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo32)
ASM_PFX(IoReadFifo32):
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
rep insd
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo8)
ASM_PFX(IoWriteFifo8):
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsb
    mov     rsi, r8             ; restore rsi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo16 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo16)
ASM_PFX(IoWriteFifo16):
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsw
    mov     rsi, r8             ; restore rsi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo32 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo32)
ASM_PFX(IoWriteFifo32):
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsd
    mov     rsi, r8             ; restore rsi
    ret

