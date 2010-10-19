/* CPU data header for xstormy16.

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

#ifndef XSTORMY16_CPU_H
#define XSTORMY16_CPU_H

#include "opcode/cgen-bitset.h"

#define CGEN_ARCH xstormy16

/* Given symbol S, return xstormy16_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) xstormy16##_cgen_##s
#else
#define CGEN_SYM(s) xstormy16/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_XSTORMY16

#define CGEN_INSN_LSB0_P 0

/* Minimum size of any insn (in bytes).  */
#define CGEN_MIN_INSN_SIZE 2

/* Maximum size of any insn (in bytes).  */
#define CGEN_MAX_INSN_SIZE 4

#define CGEN_INT_INSN_P 1

/* Maximum number of syntax elements in an instruction.  */
#define CGEN_ACTUAL_MAX_SYNTAX_ELEMENTS 19

/* CGEN_MNEMONIC_OPERANDS is defined if mnemonics have operands.
   e.g. In "b,a foo" the ",a" is an operand.  If mnemonics have operands
   we can't hash on everything up to the space.  */
#define CGEN_MNEMONIC_OPERANDS

/* Maximum number of fields in an instruction.  */
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 9

/* Enums.  */

/* Enum declaration for .  */
typedef enum gr_names {
  H_GR_R0 = 0, H_GR_R1 = 1, H_GR_R2 = 2, H_GR_R3 = 3
 , H_GR_R4 = 4, H_GR_R5 = 5, H_GR_R6 = 6, H_GR_R7 = 7
 , H_GR_R8 = 8, H_GR_R9 = 9, H_GR_R10 = 10, H_GR_R11 = 11
 , H_GR_R12 = 12, H_GR_R13 = 13, H_GR_R14 = 14, H_GR_R15 = 15
 , H_GR_PSW = 14, H_GR_SP = 15
} GR_NAMES;

/* Enum declaration for .  */
typedef enum gr_rb_names {
  H_RBJ_R8 = 0, H_RBJ_R9 = 1, H_RBJ_R10 = 2, H_RBJ_R11 = 3
 , H_RBJ_R12 = 4, H_RBJ_R13 = 5, H_RBJ_R14 = 6, H_RBJ_R15 = 7
 , H_RBJ_PSW = 6, H_RBJ_SP = 7
} GR_RB_NAMES;

/* Enum declaration for insn op enums.  */
typedef enum insn_op1 {
  OP1_0, OP1_1, OP1_2, OP1_3
 , OP1_4, OP1_5, OP1_6, OP1_7
 , OP1_8, OP1_9, OP1_A, OP1_B
 , OP1_C, OP1_D, OP1_E, OP1_F
} INSN_OP1;

/* Enum declaration for insn op enums.  */
typedef enum insn_op2 {
  OP2_0, OP2_1, OP2_2, OP2_3
 , OP2_4, OP2_5, OP2_6, OP2_7
 , OP2_8, OP2_9, OP2_A, OP2_B
 , OP2_C, OP2_D, OP2_E, OP2_F
} INSN_OP2;

/* Enum declaration for insn op enums.  */
typedef enum insn_op2a {
  OP2A_0, OP2A_2, OP2A_4, OP2A_6
 , OP2A_8, OP2A_A, OP2A_C, OP2A_E
} INSN_OP2A;

/* Enum declaration for insn op enums.  */
typedef enum insn_op2m {
  OP2M_0, OP2M_1
} INSN_OP2M;

/* Enum declaration for insn op enums.  */
typedef enum insn_op3 {
  OP3_0, OP3_1, OP3_2, OP3_3
 , OP3_4, OP3_5, OP3_6, OP3_7
 , OP3_8, OP3_9, OP3_A, OP3_B
 , OP3_C, OP3_D, OP3_E, OP3_F
} INSN_OP3;

/* Enum declaration for insn op enums.  */
typedef enum insn_op3a {
  OP3A_0, OP3A_1, OP3A_2, OP3A_3
} INSN_OP3A;

/* Enum declaration for insn op enums.  */
typedef enum insn_op3b {
  OP3B_0, OP3B_2, OP3B_4, OP3B_6
 , OP3B_8, OP3B_A, OP3B_C, OP3B_E
} INSN_OP3B;

/* Enum declaration for insn op enums.  */
typedef enum insn_op4 {
  OP4_0, OP4_1, OP4_2, OP4_3
 , OP4_4, OP4_5, OP4_6, OP4_7
 , OP4_8, OP4_9, OP4_A, OP4_B
 , OP4_C, OP4_D, OP4_E, OP4_F
} INSN_OP4;

/* Enum declaration for insn op enums.  */
typedef enum insn_op4m {
  OP4M_0, OP4M_1
} INSN_OP4M;

/* Enum declaration for insn op enums.  */
typedef enum insn_op4b {
  OP4B_0, OP4B_1
} INSN_OP4B;

/* Enum declaration for insn op enums.  */
typedef enum insn_op5 {
  OP5_0, OP5_1, OP5_2, OP5_3
 , OP5_4, OP5_5, OP5_6, OP5_7
 , OP5_8, OP5_9, OP5_A, OP5_B
 , OP5_C, OP5_D, OP5_E, OP5_F
} INSN_OP5;

/* Enum declaration for insn op enums.  */
typedef enum insn_op5a {
  OP5A_0, OP5A_1
} INSN_OP5A;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_XSTORMY16, MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_XSTORMY16, ISA_MAX
} ISA_ATTR;

/* Number of architecture variants.  */
#define MAX_ISAS  1
#define MAX_MACHS ((int) MACH_MAX)

/* Ifield support.  */

/* Ifield attribute indices.  */

/* Enum declaration for cgen_ifld attrs.  */
typedef enum cgen_ifld_attr {
  CGEN_IFLD_VIRTUAL, CGEN_IFLD_PCREL_ADDR, CGEN_IFLD_ABS_ADDR, CGEN_IFLD_RESERVED
 , CGEN_IFLD_SIGN_OPT, CGEN_IFLD_SIGNED, CGEN_IFLD_END_BOOLS, CGEN_IFLD_START_NBOOLS = 31
 , CGEN_IFLD_MACH, CGEN_IFLD_END_NBOOLS
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

/* Enum declaration for xstormy16 ifield types.  */
typedef enum ifield_type {
  XSTORMY16_F_NIL, XSTORMY16_F_ANYOF, XSTORMY16_F_RD, XSTORMY16_F_RDM
 , XSTORMY16_F_RM, XSTORMY16_F_RS, XSTORMY16_F_RB, XSTORMY16_F_RBJ
 , XSTORMY16_F_OP1, XSTORMY16_F_OP2, XSTORMY16_F_OP2A, XSTORMY16_F_OP2M
 , XSTORMY16_F_OP3, XSTORMY16_F_OP3A, XSTORMY16_F_OP3B, XSTORMY16_F_OP4
 , XSTORMY16_F_OP4M, XSTORMY16_F_OP4B, XSTORMY16_F_OP5, XSTORMY16_F_OP5A
 , XSTORMY16_F_OP, XSTORMY16_F_IMM2, XSTORMY16_F_IMM3, XSTORMY16_F_IMM3B
 , XSTORMY16_F_IMM4, XSTORMY16_F_IMM8, XSTORMY16_F_IMM12, XSTORMY16_F_IMM16
 , XSTORMY16_F_LMEM8, XSTORMY16_F_HMEM8, XSTORMY16_F_REL8_2, XSTORMY16_F_REL8_4
 , XSTORMY16_F_REL12, XSTORMY16_F_REL12A, XSTORMY16_F_ABS24_1, XSTORMY16_F_ABS24_2
 , XSTORMY16_F_ABS24, XSTORMY16_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) XSTORMY16_F_MAX)

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

/* Enum declaration for xstormy16 hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_PC, HW_H_GR, HW_H_RB
 , HW_H_RBJ, HW_H_RPSW, HW_H_Z8, HW_H_Z16
 , HW_H_CY, HW_H_HC, HW_H_OV, HW_H_PT
 , HW_H_S, HW_H_BRANCHCOND, HW_H_WORDSIZE, HW_MAX
} CGEN_HW_TYPE;

#define MAX_HW ((int) HW_MAX)

/* Operand attribute indices.  */

/* Enum declaration for cgen_operand attrs.  */
typedef enum cgen_operand_attr {
  CGEN_OPERAND_VIRTUAL, CGEN_OPERAND_PCREL_ADDR, CGEN_OPERAND_ABS_ADDR, CGEN_OPERAND_SIGN_OPT
 , CGEN_OPERAND_SIGNED, CGEN_OPERAND_NEGATIVE, CGEN_OPERAND_RELAX, CGEN_OPERAND_SEM_ONLY
 , CGEN_OPERAND_END_BOOLS, CGEN_OPERAND_START_NBOOLS = 31, CGEN_OPERAND_MACH, CGEN_OPERAND_END_NBOOLS
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

/* Enum declaration for xstormy16 operand types.  */
typedef enum cgen_operand_type {
  XSTORMY16_OPERAND_PC, XSTORMY16_OPERAND_PSW_Z8, XSTORMY16_OPERAND_PSW_Z16, XSTORMY16_OPERAND_PSW_CY
 , XSTORMY16_OPERAND_PSW_HC, XSTORMY16_OPERAND_PSW_OV, XSTORMY16_OPERAND_PSW_PT, XSTORMY16_OPERAND_PSW_S
 , XSTORMY16_OPERAND_RD, XSTORMY16_OPERAND_RDM, XSTORMY16_OPERAND_RM, XSTORMY16_OPERAND_RS
 , XSTORMY16_OPERAND_RB, XSTORMY16_OPERAND_RBJ, XSTORMY16_OPERAND_BCOND2, XSTORMY16_OPERAND_WS2
 , XSTORMY16_OPERAND_BCOND5, XSTORMY16_OPERAND_IMM2, XSTORMY16_OPERAND_IMM3, XSTORMY16_OPERAND_IMM3B
 , XSTORMY16_OPERAND_IMM4, XSTORMY16_OPERAND_IMM8, XSTORMY16_OPERAND_IMM8SMALL, XSTORMY16_OPERAND_IMM12
 , XSTORMY16_OPERAND_IMM16, XSTORMY16_OPERAND_LMEM8, XSTORMY16_OPERAND_HMEM8, XSTORMY16_OPERAND_REL8_2
 , XSTORMY16_OPERAND_REL8_4, XSTORMY16_OPERAND_REL12, XSTORMY16_OPERAND_REL12A, XSTORMY16_OPERAND_ABS24
 , XSTORMY16_OPERAND_PSW, XSTORMY16_OPERAND_RPSW, XSTORMY16_OPERAND_SP, XSTORMY16_OPERAND_R0
 , XSTORMY16_OPERAND_R1, XSTORMY16_OPERAND_R2, XSTORMY16_OPERAND_R8, XSTORMY16_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 39

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 8

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_END_BOOLS, CGEN_INSN_START_NBOOLS = 31
 , CGEN_INSN_MACH, CGEN_INSN_END_NBOOLS
} CGEN_INSN_ATTR;

/* Number of non-boolean elements in cgen_insn_attr.  */
#define CGEN_INSN_NBOOL_ATTRS (CGEN_INSN_END_NBOOLS - CGEN_INSN_START_NBOOLS - 1)

/* cgen_insn attribute accessor macros.  */
#define CGEN_ATTR_CGEN_INSN_MACH_VALUE(attrs) ((attrs)->nonbool[CGEN_INSN_MACH-CGEN_INSN_START_NBOOLS-1].nonbitset)
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

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

extern const struct cgen_ifld xstormy16_cgen_ifld_table[];

/* Attributes.  */
extern const CGEN_ATTR_TABLE xstormy16_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE xstormy16_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE xstormy16_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE xstormy16_cgen_insn_attr_table[];

/* Hardware decls.  */

extern CGEN_KEYWORD xstormy16_cgen_opval_gr_names;
extern CGEN_KEYWORD xstormy16_cgen_opval_gr_Rb_names;
extern CGEN_KEYWORD xstormy16_cgen_opval_gr_Rb_names;
extern CGEN_KEYWORD xstormy16_cgen_opval_h_branchcond;
extern CGEN_KEYWORD xstormy16_cgen_opval_h_wordsize;

extern const CGEN_HW_ENTRY xstormy16_cgen_hw_table[];



#endif /* XSTORMY16_CPU_H */
