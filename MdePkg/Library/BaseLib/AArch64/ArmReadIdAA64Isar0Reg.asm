;------------------------------------------------------------------------------
;
; ArmReadIdAA64Isar0Reg() for AArch64
;
; Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
;
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT ArmReadIdAA64Isar0Reg
  AREA BaseLib_LowLevel, CODE, READONLY

;/**
;  Reads the ID_AA64ISAR0 Register.
;
; @return The contents of the ID_AA64ISAR0 register.
;
;**/
;UINT64
;EFIAPI
;ArmReadIdAA64Isar0Reg (
;  VOID
;  );
;
ArmReadIdAA64Isar0Reg
  mrs  x0, id_aa64isar0_el1
  ret

  END
