/* CPU data header for m32r.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef M32R_CPU_H
#define M32R_CPU_H

#include "opcode/cgen-bitset.h"

#define CGEN_ARCH m32r

/* Given symbol S, return m32r_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) m32r##_cgen_##s
#else
#define CGEN_SYM(s) m32r/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_M32RBF
#define HAVE_CPU_M32RXF
#define HAVE_CPU_M32R2F

#define CGEN_INSN_LSB0_P 0

/* Minimum size of any insn (in bytes).  */
#define CGEN_MIN_INSN_SIZE 2

/* Maximum size of any insn (in bytes).  */
#define CGEN_MAX_INSN_SIZE 4

#define CGEN_INT_INSN_P 1

/* Maximum number of syntax elements in an instruction.  */
#define CGEN_ACTUAL_MAX_SYNTAX_ELEMENTS 15

/* CGEN_MNEMONIC_OPERANDS is defined if mnemonics have operands.
   e.g. In "b,a foo" the ",a" is an operand.  If mnemonics have operands
   we can't hash on everything up to the space.  */
#define CGEN_MNEMONIC_OPERANDS

/* Maximum number of fields in an instruction.  */
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 7

/* Enums.  */

/* Enum declaration for insn format enums.  */
typedef enum insn_op1 {
  OP1_0, OP1_1, OP1_2, OP1_3
 , OP1_4, OP1_5, OP1_6, OP1_7
 , OP1_8, OP1_9, OP1_10, OP1_11
 , OP1_12, OP1_13, OP1_14, OP1_15
} INSN_OP1;

/* Enum declaration for op2 enums.  */
typedef enum insn_op2 {
  OP2_0, OP2_1, OP2_2, OP2_3
 , OP2_4, OP2_5, OP2_6, OP2_7
 , OP2_8, OP2_9, OP2_10, OP2_11
 , OP2_12, OP2_13, OP2_14, OP2_15
} INSN_OP2;

/* Enum declaration for .  */
typedef enum gr_names {
  H_GR_FP = 13, H_GR_LR = 14, H_GR_SP = 15, H_GR_R0 = 0
 , H_GR_R1 = 1, H_GR_R2 = 2, H_GR_R3 = 3, H_GR_R4 = 4
 , H_GR_R5 = 5, H_GR_R6 = 6, H_GR_R7 = 7, H_GR_R8 = 8
 , H_GR_R9 = 9, H_GR_R10 = 10, H_GR_R11 = 11, H_GR_R12 = 12
 , H_GR_R13 = 13, H_GR_R14 = 14, H_GR_R15 = 15
} GR_NAMES;

/* Enum declaration for .  */
typedef enum cr_names {
  H_CR_PSW = 0, H_CR_CBR = 1, H_CR_SPI = 2, H_CR_SPU = 3
 , H_CR_BPC = 6, H_CR_BBPSW = 8, H_CR_BBPC = 14, H_CR_EVB = 5
 , H_CR_CR0 = 0, H_CR_CR1 = 1, H_CR_CR2 = 2, H_CR_CR3 = 3
 , H_CR_CR4 = 4, H_CR_CR5 = 5, H_CR_CR6 = 6, H_CR_CR7 = 7
 , H_CR_CR8 = 8, H_CR_CR9 = 9, H_CR_CR10 = 10, H_CR_CR11 = 11
 , H_CR_CR12 = 12, H_CR_CR13 = 13, H_CR_CR14 = 14, H_CR_CR15 = 15
} CR_NAMES;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_M32R, MACH_M32RX, MACH_M32R2
 , MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_M32R, ISA_MAX
} ISA_ATTR;

/* Enum declaration for parallel execution pipeline selection.  */
typedef enum pipe_attr {
  PIPE_NONE, PIPE_O, PIPE_S, PIPE_OS
 , PIPE_O_OS
} PIPE_ATTR;

/* Number of architecture variants.  */
#define MAX_ISAS  1
#define MAX_MACHS ((int) MACH_MAX)

/* Ifield support.  */

/* Ifield attribute indices.  */

/* Enum declaration for cgen_ifld attrs.  */
typedef enum cgen_ifld_attr {
  CGEN_IFLD_VIRTUAL, CGEN_IFLD_PCREL_ADDR, CGEN_IFLD_ABS_ADDR, CGEN_IFLD_RESERVED
 , CGEN_IFLD_SIGN_OPT, CGEN_IFLD_SIGNED, CGEN_IFLD_RELOC, CGEN_IFLD_END_BOOLS
 , CGEN_IFLD_START_NBOOLS = 31, CGEN_IFLD_MACH, CGEN_IFLD_END_NBOOLS
} CGEN_IFLD_ATTR;

/* Number of non-boolean elements in cgen_ifld_attr.  */
#define CGEN_IFLD_NBOOL_ATTRS (CGEN_IFLD_END_NBOOLS - CGEN_IFLD_START_NBOOLS - 1)

/* cgen_ifld attribute accessor macros.  */
#define CGEN_ATTR_CGEN_IFLD_MACH_VALUE(attrs) ((attrs)->nonbool[CGEN_IFLD_MACH-CGEN_IFLD_START_NBOOLS-1].nonbitset)
#define CGEN_ATTR_CGEN_IFLD_VIRTUAL_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_VIRTUAL)) != 0)
#define CGEN_ATTR_CGEN_IFLD_PCREL_ADDR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_PCREL_ADDR)) != 0)
#define CGEN_ATTR_CGEN_IFLD_ABS_ADDR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_ABS_ADDR)) != 0)
#define CGEN_ATTR_CGEN_IFLD_RESERVED_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_RESERVED)) != 0)
#define CGEN_ATTR_CGEN_IFLD_SIGN_OPT_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_SIGN_OPT)) != 0)
#define CGEN_ATTR_CGEN_IFLD_SIGNED_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_SIGNED)) != 0)
#define CGEN_ATTR_CGEN_IFLD_RELOC_VALUE(attrs) (((attrs)->bool & (1 << CGEN_IFLD_RELOC)) != 0)

/* Enum declaration for m32r ifield types.  */
typedef enum ifield_type {
  M32R_F_NIL, M32R_F_ANYOF, M32R_F_OP1, M32R_F_OP2
 , M32R_F_COND, M32R_F_R1, M32R_F_R2, M32R_F_SIMM8
 , M32R_F_SIMM16, M32R_F_SHIFT_OP2, M32R_F_UIMM3, M32R_F_UIMM4
 , M32R_F_UIMM5, M32R_F_UIMM8, M32R_F_UIMM16, M32R_F_UIMM24
 , M32R_F_HI16, M32R_F_DISP8, M32R_F_DISP16, M32R_F_DISP24
 , M32R_F_OP23, M32R_F_OP3, M32R_F_ACC, M32R_F_ACCS
 , M32R_F_ACCD, M32R_F_BITS67, M32R_F_BIT4, M32R_F_BIT14
 , M32R_F_IMM1, M32R_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) M32R_F_MAX)

/* Hardware attribute indices.  */

/* Enum declaration for cgen_hw attrs.  */
typedef enum cgen_hw_attr {
  CGEN_HW_VIRTUAL, CGEN_HW_CACHE_ADDR, CGEN_HW_PC, CGEN_HW_PROFILE
 , CGEN_HW_END_BOOLS, CGEN_HW_START_NBOOLS = 31, CGEN_HW_MACH, CGEN_HW_END_NBOOLS
} CGEN_HW_ATTR;

/* Number of non-boolean elements in cgen_hw_attr.  */
#define CGEN_HW_NBOOL_ATTRS (CGEN_HW_END_NBOOLS - CGEN_HW_START_NBOOLS - 1)

/* cgen_hw attribute accessor macros.  */
#define CGEN_ATTR_CGEN_HW_MACH_VALUE(attrs) ((attrs)->nonbool[CGEN_HW_MACH-CGEN_HW_START_NBOOLS-1].nonbitset)
#define CGEN_ATTR_CGEN_HW_VIRTUAL_VALUE(attrs) (((attrs)->bool & (1 << CGEN_HW_VIRTUAL)) != 0)
#define CGEN_ATTR_CGEN_HW_CACHE_ADDR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_HW_CACHE_ADDR)) != 0)
#define CGEN_ATTR_CGEN_HW_PC_VALUE(attrs) (((attrs)->bool & (1 << CGEN_HW_PC)) != 0)
#define CGEN_ATTR_CGEN_HW_PROFILE_VALUE(attrs) (((attrs)->bool & (1 << CGEN_HW_PROFILE)) != 0)

/* Enum declaration for m32r hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_PC, HW_H_HI16, HW_H_SLO16
 , HW_H_ULO16, HW_H_GR, HW_H_CR, HW_H_ACCUM
 , HW_H_ACCUMS, HW_H_COND, HW_H_PSW, HW_H_BPSW
 , HW_H_BBPSW, HW_H_LOCK, HW_MAX
} CGEN_HW_TYPE;

#define MAX_HW ((int) HW_MAX)

/* Operand attribute indices.  */

/* Enum declaration for cgen_operand attrs.  */
typedef enum cgen_operand_attr {
  CGEN_OPERAND_VIRTUAL, CGEN_OPERAND_PCREL_ADDR, CGEN_OPERAND_ABS_ADDR, CGEN_OPERAND_SIGN_OPT
 , CGEN_OPERAND_SIGNED, CGEN_OPERAND_NEGATIVE, CGEN_OPERAND_RELAX, CGEN_OPERAND_SEM_ONLY
 , CGEN_OPERAND_RELOC, CGEN_OPERAND_HASH_PREFIX, CGEN_OPERAND_END_BOOLS, CGEN_OPERAND_START_NBOOLS = 31
 , CGEN_OPERAND_MACH, CGEN_OPERAND_END_NBOOLS
} CGEN_OPERAND_ATTR;

/* Number of non-boolean elements in cgen_operand_attr.  */
#define CGEN_OPERAND_NBOOL_ATTRS (CGEN_OPERAND_END_NBOOLS - CGEN_OPERAND_START_NBOOLS - 1)

/* cgen_operand attribute accessor macros.  */
#define CGEN_ATTR_CGEN_OPERAND_MACH_VALUE(attrs) ((attrs)->nonbool[CGEN_OPERAND_MACH-CGEN_OPERAND_START_NBOOLS-1].nonbitset)
#define CGEN_ATTR_CGEN_OPERAND_VIRTUAL_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_VIRTUAL)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_PCREL_ADDR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_PCREL_ADDR)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_ABS_ADDR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_ABS_ADDR)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_SIGN_OPT_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_SIGN_OPT)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_SIGNED_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_SIGNED)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_NEGATIVE_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_NEGATIVE)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_RELAX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_RELAX)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_SEM_ONLY_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_SEM_ONLY)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_RELOC_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_RELOC)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_HASH_PREFIX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_HASH_PREFIX)) != 0)

/* Enum declaration for m32r operand types.  */
typedef enum cgen_operand_type {
  M32R_OPERAND_PC, M32R_OPERAND_SR, M32R_OPERAND_DR, M32R_OPERAND_SRC1
 , M32R_OPERAND_SRC2, M32R_OPERAND_SCR, M32R_OPERAND_DCR, M32R_OPERAND_SIMM8
 , M32R_OPERAND_SIMM16, M32R_OPERAND_UIMM3, M32R_OPERAND_UIMM4, M32R_OPERAND_UIMM5
 , M32R_OPERAND_UIMM8, M32R_OPERAND_UIMM16, M32R_OPERAND_IMM1, M32R_OPERAND_ACCD
 , M32R_OPERAND_ACCS, M32R_OPERAND_ACC, M32R_OPERAND_HASH, M32R_OPERAND_HI16
 , M32R_OPERAND_SLO16, M32R_OPERAND_ULO16, M32R_OPERAND_UIMM24, M32R_OPERAND_DISP8
 , M32R_OPERAND_DISP16, M32R_OPERAND_DISP24, M32R_OPERAND_CONDBIT, M32R_OPERAND_ACCUM
 , M32R_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 28

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 11

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_FILL_SLOT, CGEN_INSN_SPECIAL
 , CGEN_INSN_SPECIAL_M32R, CGEN_INSN_SPECIAL_FLOAT, CGEN_INSN_END_BOOLS, CGEN_INSN_START_NBOOLS = 31
 , CGEN_INSN_MACH, CGEN_INSN_PIPE, CGEN_INSN_END_NBOOLS
} CGEN_INSN_ATTR;

/* Number of non-boolean elements in cgen_insn_attr.  */
#define CGEN_INSN_NBOOL_ATTRS (CGEN_INSN_END_NBOOLS - CGEN_INSN_START_NBOOLS - 1)

/* cgen_insn attribute accessor macros.  */
#define CGEN_ATTR_CGEN_INSN_MACH_VALUE(attrs) ((attrs)->nonbool[CGEN_INSN_MACH-CGEN_INSN_START_NBOOLS-1].nonbitset)
#define CGEN_ATTR_CGEN_INSN_PIPE_VALUE(attrs) ((attrs)->nonbool[CGEN_INSN_PIPE-CGEN_INSN_START_NBOOLS-1].nonbitset)
#define CGEN_ATTR_CGEN_INSN_ALIAS_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_ALIAS)) != 0)
#define CGEN_ATTR_CGEN_INSN_VIRTUAL_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_VIRTUAL)) != 0)
#define CGEN_ATTR_CGEN_INSN_UNCOND_CTI_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_UNCOND_CTI)) != 0)
#define CGEN_ATTR_CGEN_INSN_COND_CTI_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_COND_CTI)) != 0)
#define CGEN_ATTR_CGEN_INSN_SKIP_CTI_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_SKIP_CTI)) != 0)
#define CGEN_ATTR_CGEN_INSN_DELAY_SLOT_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_DELAY_SLOT)) != 0)
#define CGEN_ATTR_CGEN_INSN_RELAXABLE_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_RELAXABLE)) != 0)
#define CGEN_ATTR_CGEN_INSN_RELAXED_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_RELAXED)) != 0)
#define CGEN_ATTR_CGEN_INSN_NO_DIS_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_NO_DIS)) != 0)
#define CGEN_ATTR_CGEN_INSN_PBB_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_PBB)) != 0)
#define CGEN_ATTR_CGEN_INSN_FILL_SLOT_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_FILL_SLOT)) != 0)
#define CGEN_ATTR_CGEN_INSN_SPECIAL_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_SPECIAL)) != 0)
#define CGEN_ATTR_CGEN_INSN_SPECIAL_M32R_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_SPECIAL_M32R)) != 0)
#define CGEN_ATTR_CGEN_INSN_SPECIAL_FLOAT_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_SPECIAL_FLOAT)) != 0)

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

extern const struct cgen_ifld m32r_cgen_ifld_table[];

/* Attributes.  */
extern const CGEN_ATTR_TABLE m32r_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE m32r_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE m32r_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE m32r_cgen_insn_attr_table[];

/* Hardware decls.  */

extern CGEN_KEYWORD m32r_cgen_opval_gr_names;
extern CGEN_KEYWORD m32r_cgen_opval_cr_names;
extern CGEN_KEYWORD m32r_cgen_opval_h_accums;

extern const CGEN_HW_ENTRY m32r_cgen_hw_table[];



#endif /* M32R_CPU_H */
