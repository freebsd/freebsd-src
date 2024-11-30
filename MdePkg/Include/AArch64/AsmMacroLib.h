/** @file
  Macros to work around lack of Clang support for LDR register, =expr

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Portions copyright (c) 2011 - 2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ASM_MACRO_IO_LIBV8_H_
#define ASM_MACRO_IO_LIBV8_H_

// CurrentEL : 0xC = EL3; 8 = EL2; 4 = EL1
// This only selects between EL1 and EL2, else we die.
// Provide the Macro with a safe temp xreg to use.
#define EL1_OR_EL2(SAFE_XREG)        \
        mrs    SAFE_XREG, CurrentEL ;\
        cmp    SAFE_XREG, #0x8      ;\
        b.gt   .                    ;\
        b.eq   2f                   ;\
        cbnz   SAFE_XREG, 1f        ;\
        b      .                    ;// We should never get here

#define _ASM_FUNC(Name, Section)    \
  .global   Name                  ; \
  .section  #Section, "ax"        ; \
  .type     Name, %function       ; \
  Name:                           ; \
  AARCH64_BTI(c)

#define _ASM_FUNC_ALIGN(Name, Section, Align)       \
  .global   Name                                  ; \
  .section  #Section, "ax"                        ; \
  .type     Name, %function                       ; \
  .balign   Align                                 ; \
  Name:                                           ; \
  AARCH64_BTI(c)

#define ASM_FUNC(Name)  _ASM_FUNC(ASM_PFX(Name), .text. ## Name)

#define ASM_FUNC_ALIGN(Name, Align)  \
  _ASM_FUNC_ALIGN(ASM_PFX(Name), .text. ## Name, Align)

#define MOV32(Reg, Val)                   \
  movz      Reg, (Val) >> 16, lsl #16   ; \
  movk      Reg, (Val) & 0xffff

#define MOV64(Reg, Val)                             \
  movz      Reg, (Val) >> 48, lsl #48             ; \
  movk      Reg, ((Val) >> 32) & 0xffff, lsl #32  ; \
  movk      Reg, ((Val) >> 16) & 0xffff, lsl #16  ; \
  movk      Reg, (Val) & 0xffff

#endif // ASM_MACRO_IO_LIBV8_H_
