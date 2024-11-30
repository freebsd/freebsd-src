/** @file
  Macros to work around lack of Apple support for LDR register, =expr

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2011-2012, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ASM_MACRO_IO_LIB_H_
#define ASM_MACRO_IO_LIB_H_

#define _ASM_FUNC(Name, Section)    \
  .global   Name                  ; \
  .section  #Section, "ax"        ; \
  .type     Name, %function       ; \
  .p2align  2                     ; \
  Name:

#define ASM_FUNC(Name)  _ASM_FUNC(ASM_PFX(Name), .text. ## Name)

#define MOV32(Reg, Val)                       \
  movw      Reg, #(Val) & 0xffff            ; \
  movt      Reg, #(Val) >> 16

#define ADRL(Reg, Sym)                        \
  movw      Reg, #:lower16:(Sym) - (. + 16) ; \
  movt      Reg, #:upper16:(Sym) - (. + 12) ; \
  add       Reg, Reg, pc

#define LDRL(Reg, Sym)                        \
  movw      Reg, #:lower16:(Sym) - (. + 16) ; \
  movt      Reg, #:upper16:(Sym) - (. + 12) ; \
  ldr       Reg, [pc, Reg]

#endif // ASM_MACRO_IO_LIB_H_
