#------------------------------------------------------------------------------
#
# LoongArch ASM IO CSR operation functions
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX (IoCsrRead8)
ASM_GLOBAL ASM_PFX (IoCsrRead16)
ASM_GLOBAL ASM_PFX (IoCsrRead32)
ASM_GLOBAL ASM_PFX (IoCsrRead64)

ASM_GLOBAL ASM_PFX (IoCsrWrite8)
ASM_GLOBAL ASM_PFX (IoCsrWrite16)
ASM_GLOBAL ASM_PFX (IoCsrWrite32)
ASM_GLOBAL ASM_PFX (IoCsrWrite64)

#/**
#  IO CSR read byte operation.
#
#  @param[in]  Select   IO CSR read instruction select values.
#
#  @return     The return value of iocsrrd.b instruction.
#
#**/
ASM_PFX (IoCsrRead8):
  iocsrrd.b   $a0, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR read half word operation.
#
#  @param[in]  Select   IO CSR read instruction select values.
#
#  @return     The return value of iocsrrd.h instruction.
#
#**/
ASM_PFX (IoCsrRead16):
  iocsrrd.h   $a0, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR read word operation.
#
#  @param[in]  Select   IO CSR read instruction select values.
#
#  @return     The return value of iocsrrd.w instruction.
#
#**/
ASM_PFX (IoCsrRead32):
  iocsrrd.w   $a0, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR read double word operation. Only for LoongArch64.
#
#  @param[in]  Select   IO CSR read instruction select values.
#
#  @return     The return value of iocsrrd.d instruction.
#
#**/
ASM_PFX (IoCsrRead64):
  iocsrrd.d   $a0, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR write byte operation.
#
#  @param[in]  Select   IO CSR write instruction select values.
#  @param[in]  Value    The iocsrwr.b will write the value.
#
#  @return     VOID.
#
#**/
ASM_PFX (IoCsrWrite8):
  iocsrwr.b   $a1, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR write half word operation.
#
#  @param[in]  Select   IO CSR write instruction select values.
#  @param[in]  Value    The iocsrwr.h will write the value.
#
#  @return     VOID.
#
#**/
ASM_PFX (IoCsrWrite16):
  iocsrwr.h   $a1, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR write word operation.
#
#  @param[in]  Select   IO CSR write instruction select values.
#  @param[in]  Value    The iocsrwr.w will write the value.
#
#  @return     VOID.
#
#**/
ASM_PFX (IoCsrWrite32):
  iocsrwr.w   $a1, $a0
  jirl        $zero, $ra, 0

#/**
#  IO CSR write double word operation. Only for LoongArch64.
#
#  @param[in]  Select   IO CSR write instruction select values.
#  @param[in]  Value    The iocsrwr.d will write the value.
#
#  @return     VOID.
#
#**/
ASM_PFX (IoCsrWrite64):
  iocsrwr.d   $a1, $a0
  jirl        $zero, $ra, 0
  .end
