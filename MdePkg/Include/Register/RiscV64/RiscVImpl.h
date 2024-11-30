/** @file
  RISC-V package definitions.

  Copyright (c) 2016 - 2022, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef RISCV_IMPL_H_
#define RISCV_IMPL_H_

#include <Register/RiscV64/RiscVEncoding.h>

#define _ASM_FUNC(Name, Section)    \
  .global   Name                  ; \
  .section  #Section, "ax"        ; \
  .type     Name, %function       ; \
  .p2align  2                     ; \
  Name:

#define ASM_FUNC(Name)  _ASM_FUNC(ASM_PFX(Name), .text. ## Name)

#endif
