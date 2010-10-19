/* CPU data header for openrisc.

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

#ifndef OPENRISC_CPU_H
#define OPENRISC_CPU_H

#include "opcode/cgen-bitset.h"

#define CGEN_ARCH openrisc

/* Given symbol S, return openrisc_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) openrisc##_cgen_##s
#else
#define CGEN_SYM(s) openrisc/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_OPENRISCBF

#define CGEN_INSN_LSB0_P 1

/* Minimum size of any insn (in bytes).  */
#define CGEN_MIN_INSN_SIZE 4

/* Maximum size of any insn (in bytes).  */
#define CGEN_MAX_INSN_SIZE 4

#define CGEN_INT_INSN_P 1

/* Maximum number of syntax elements in an instruction.  */
#define CGEN_ACTUAL_MAX_SYNTAX_ELEMENTS 14

/* CGEN_MNEMONIC_OPERANDS is defined if mnemonics have operands.
   e.g. In "b,a foo" the ",a" is an operand.  If mnemonics have operands
   we can't hash on everything up to the space.  */
#define CGEN_MNEMONIC_OPERANDS

/* Maximum number of fields in an instruction.  */
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 9

/* Enums.  */

/* Enum declaration for exception vectors.  */
typedef enum e_exception {
  E_RESET, E_BUSERR, E_DPF, E_IPF
 , E_EXTINT, E_ALIGN, E_ILLEGAL, E_PEINT
 , E_DTLBMISS, E_ITLBMISS, E_RRANGE, E_SYSCALL
 , E_BREAK, E_RESERVED
} E_EXCEPTION;

/* Enum declaration for FIXME.  */
typedef enum insn_class {
  OP1_0, OP1_1, OP1_2, OP1_3
} INSN_CLASS;

/* Enum declaration for FIXME.  */
typedef enum insn_sub {
  OP2_0, OP2_1, OP2_2, OP2_3
 , OP2_4, OP2_5, OP2_6, OP2_7
 , OP2_8, OP2_9, OP2_10, OP2_11
 , OP2_12, OP2_13, OP2_14, OP2_15
} INSN_SUB;

/* Enum declaration for FIXME.  */
typedef enum insn_op3 {
  OP3_0, OP3_1, OP3_2, OP3_3
} INSN_OP3;

/* Enum declaration for FIXME.  */
typedef enum insn_op4 {
  OP4_0, OP4_1, OP4_2, OP4_3
 , OP4_4, OP4_5, OP4_6, OP4_7
} INSN_OP4;

/* Enum declaration for FIXME.  */
typedef enum insn_op5 {
  OP5_0, OP5_1, OP5_2, OP5_3
 , OP5_4, OP5_5, OP5_6, OP5_7
 , OP5_8, OP5_9, OP5_10, OP5_11
 , OP5_12, OP5_13, OP5_14, OP5_15
 , OP5_16, OP5_17, OP5_18, OP5_19
 , OP5_20, OP5_21, OP5_22, OP5_23
 , OP5_24, OP5_25, OP5_26, OP5_27
 , OP5_28, OP5_29, OP5_30, OP5_31
} INSN_OP5;

/* Enum declaration for FIXME.  */
typedef enum insn_op6 {
  OP6_0, OP6_1, OP6_2, OP6_3
 , OP6_4, OP6_5, OP6_6, OP6_7
} INSN_OP6;

/* Enum declaration for FIXME.  */
typedef enum insn_op7 {
  OP7_0, OP7_1, OP7_2, OP7_3
 , OP7_4, OP7_5, OP7_6, OP7_7
 , OP7_8, OP7_9, OP7_10, OP7_11
 , OP7_12, OP7_13, OP7_14, OP7_15
} INSN_OP7;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_OPENRISC, MACH_OR1300, MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_OR32, ISA_MAX
} ISA_ATTR;

/* Enum declaration for if this model has caches.  */
typedef enum has_cache_attr {
  HAS_CACHE_DATA_CACHE, HAS_CACHE_INSN_CACHE
} HAS_CACHE_ATTR;

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

/* Enum declaration for openrisc ifield types.  */
typedef enum ifield_type {
  OPENRISC_F_NIL, OPENRISC_F_ANYOF, OPENRISC_F_CLASS, OPENRISC_F_SUB
 , OPENRISC_F_R1, OPENRISC_F_R2, OPENRISC_F_R3, OPENRISC_F_SIMM16
 , OPENRISC_F_UIMM16, OPENRISC_F_UIMM5, OPENRISC_F_HI16, OPENRISC_F_LO16
 , OPENRISC_F_OP1, OPENRISC_F_OP2, OPENRISC_F_OP3, OPENRISC_F_OP4
 , OPENRISC_F_OP5, OPENRISC_F_OP6, OPENRISC_F_OP7, OPENRISC_F_I16_1
 , OPENRISC_F_I16_2, OPENRISC_F_DISP26, OPENRISC_F_ABS26, OPENRISC_F_I16NC
 , OPENRISC_F_F_15_8, OPENRISC_F_F_10_3, OPENRISC_F_F_4_1, OPENRISC_F_F_7_3
 , OPENRISC_F_F_10_7, OPENRISC_F_F_10_11, OPENRISC_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) OPENRISC_F_MAX)

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

/* Enum declaration for openrisc hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_PC, HW_H_GR, HW_H_SR
 , HW_H_HI16, HW_H_LO16, HW_H_CBIT, HW_H_DELAY_INSN
 , HW_MAX
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

/* Enum declaration for openrisc operand types.  */
typedef enum cgen_operand_type {
  OPENRISC_OPERAND_PC, OPENRISC_OPERAND_SR, OPENRISC_OPERAND_CBIT, OPENRISC_OPERAND_SIMM_16
 , OPENRISC_OPERAND_UIMM_16, OPENRISC_OPERAND_DISP_26, OPENRISC_OPERAND_ABS_26, OPENRISC_OPERAND_UIMM_5
 , OPENRISC_OPERAND_RD, OPENRISC_OPERAND_RA, OPENRISC_OPERAND_RB, OPENRISC_OPERAND_OP_F_23
 , OPENRISC_OPERAND_OP_F_3, OPENRISC_OPERAND_HI16, OPENRISC_OPERAND_LO16, OPENRISC_OPERAND_UI16NC
 , OPENRISC_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 16

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 8

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_NOT_IN_DELAY_SLOT, CGEN_INSN_END_BOOLS
 , CGEN_INSN_START_NBOOLS = 31, CGEN_INSN_MACH, CGEN_INSN_END_NBOOLS
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
#define CGEN_ATTR_CGEN_INSN_NOT_IN_DELAY_SLOT_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_NOT_IN_DELAY_SLOT)) != 0)

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

extern const struct cgen_ifld openrisc_cgen_ifld_table[];

/* Attributes.  */
extern const CGEN_ATTR_TABLE openrisc_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE openrisc_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE openrisc_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE openrisc_cgen_insn_attr_table[];

/* Hardware decls.  */

extern CGEN_KEYWORD openrisc_cgen_opval_h_gr;

extern const CGEN_HW_ENTRY openrisc_cgen_hw_table[];



#endif /* OPENRISC_CPU_H */
