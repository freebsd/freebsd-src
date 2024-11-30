#------------------------------------------------------------------------------
#
# ArmReadIdAA64Isar0Reg() for AArch64
#
# Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.p2align 2
GCC_ASM_EXPORT(ArmReadIdAA64Isar0Reg)

#/**
#  Reads the ID_AA64ISAR0 Register.
#
#  @return The contents of the ID_AA64ISAR0 register.
#
#**/
#UINT64
#EFIAPI
#ArmReadIdAA64Isar0Reg (
#  VOID
#  );
#
ASM_PFX(ArmReadIdAA64Isar0Reg):
  AARCH64_BTI(c)
  mrs  x0, id_aa64isar0_el1
  ret
