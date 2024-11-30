#------------------------------------------------------------------------------
#
# LoongArch set exception base address operations
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <Base.h>
#include <Register/LoongArch64/Csr.h>

ASM_GLOBAL ASM_PFX(SetExceptionBaseAddress)
ASM_GLOBAL ASM_PFX(SetTlbRebaseAddress)

#/**
#  Set the exception base address for LoongArch.
#
#  @param  ExceptionBaseAddress   The exception base address, must be aligned greater than or qeual to 4K .
#**/
ASM_PFX(SetExceptionBaseAddress):
  csrrd   $t0, LOONGARCH_CSR_ECFG
  li.d    $t1, ~(BIT16 | BIT17 | BIT18)
  and     $t0, $t0, $t1
  csrwr   $t0, LOONGARCH_CSR_ECFG

  move    $t0, $a0
  csrwr   $t0, LOONGARCH_CSR_EBASE
  jirl    $zero, $ra, 0

#/**
#  Set the TlbRebase address for LoongArch.
#
#  @param  TlbRebaseAddress   The TlbRebase address, must be aligned greater than or qeual to 4K .
#**/
ASM_PFX(SetTlbRebaseAddress):
  move    $t0, $a0
  csrwr   $t0, LOONGARCH_CSR_TLBREBASE
  jirl    $zero, $ra, 0
.end
