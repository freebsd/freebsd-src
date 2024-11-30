#------------------------------------------------------------------------------
#
# ArmReadCntPctReg() for AArch64
#
# Copyright (c) 2023 - 2024, Arm Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.p2align 2
GCC_ASM_EXPORT(ArmReadCntPctReg)

#/**
#  Reads the CNTPCT_EL0 Register.
#
#  @return The contents of the CNTPCT_EL0 register.
#
#**/
#UINT64
#EFIAPI
#ArmReadCntPctReg (
#  VOID
#  );
#
ASM_PFX(ArmReadCntPctReg):
  AARCH64_BTI(c)
  mrs   x0, cntpct_el0
  ret
