;-----------------------------------------------------------------------------
;
; Copyright (c) 2021, AMD. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;-----------------------------------------------------------------------------

%include "Nasm.inc"

    SECTION .text

;-----------------------------------------------------------------------------
;  UINT32
;  EFIAPI
;  AsmPvalidate (
;    IN   UINT32              PageSize
;    IN   UINT32              Validate,
;    IN   UINT64              Address
;    )
;-----------------------------------------------------------------------------
global ASM_PFX(AsmPvalidate)
ASM_PFX(AsmPvalidate):
  mov     rax, r8

  PVALIDATE

  ; Save the carry flag.
  setc    dl

  ; The PVALIDATE instruction returns the status in rax register.
  cmp     rax, 0
  jne     PvalidateExit

  ; Check the carry flag to determine if RMP entry was updated.
  cmp     dl, 0
  je      PvalidateExit

  ; Return the PVALIDATE_RET_NO_RMPUPDATE.
  mov     rax, 255

PvalidateExit:
  ret
