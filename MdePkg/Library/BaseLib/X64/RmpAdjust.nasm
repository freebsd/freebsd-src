;-----------------------------------------------------------------------------
;
; Copyright (c) 2021, Advanced Micro Devices, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   RmpAdjust.Asm
;
; Abstract:
;
;   AsmRmpAdjust function
;
; Notes:
;
;-----------------------------------------------------------------------------

%include "Nasm.inc"

    SECTION .text

;-----------------------------------------------------------------------------
;  UINT32
;  EFIAPI
;  AsmRmpAdjust (
;    IN  UINT64  Rax,
;    IN  UINT64  Rcx,
;    IN  UINT64  Rdx
;    )
;-----------------------------------------------------------------------------
global ASM_PFX(AsmRmpAdjust)
ASM_PFX(AsmRmpAdjust):
  mov     rax, rcx       ; Input Rax is in RCX by calling convention
  mov     rcx, rdx       ; Input Rcx is in RDX by calling convention
  mov     rdx, r8        ; Input Rdx is in R8  by calling convention

  RMPADJUST

  ; RMPADJUST returns the status in the EAX register.
  ret
