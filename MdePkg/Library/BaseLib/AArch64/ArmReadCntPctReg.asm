;------------------------------------------------------------------------------
;
; ArmReadCntPctReg() for AArch64
;
; Copyright (c) 2023 - 2024, Arm Limited. All rights reserved.<BR>
;
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT ArmReadCntPctReg
  AREA BaseLib_LowLevel, CODE, READONLY

;/**
;  Reads the CNTPCT_EL0 Register.
;
; @return The contents of the CNTPCT_EL0 register.
;
;**/
;UINT64
;EFIAPI
;ArmReadCntPctReg (
;  VOID
;  );
;
ArmReadCntPctReg
  mrs   x0, cntpct_el0
  ret

  END
