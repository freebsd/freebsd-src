/* CPU data header for xc16x.

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

#ifndef XC16X_CPU_H
#define XC16X_CPU_H

#include "opcode/cgen-bitset.h"

#define CGEN_ARCH xc16x

/* Given symbol S, return xc16x_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) xc16x##_cgen_##s
#else
#define CGEN_SYM(s) xc16x/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_XC16XBF

#define CGEN_INSN_LSB0_P 1

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
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 8

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

/* Enum declaration for bit set/clear enums.  */
typedef enum insn_qcond {
  QBIT_0, QBIT_1, QBIT_2, QBIT_3
 , QBIT_4, QBIT_5, QBIT_6, QBIT_7
 , QBIT_8, QBIT_9, QBIT_10, QBIT_11
 , QBIT_12, QBIT_13, QBIT_14, QBIT_15
} INSN_QCOND;

/* Enum declaration for relative jump condition code op2 enums.  */
typedef enum insn_rcond {
  COND_UC = 0, COND_NET = 1, COND_Z = 2, COND_NE_NZ = 3
 , COND_V = 4, COND_NV = 5, COND_N = 6, COND_NN = 7
 , COND_C = 8, COND_NC = 9, COND_SGT = 10, COND_SLE = 11
 , COND_SLT = 12, COND_SGE = 13, COND_UGT = 14, COND_ULE = 15
 , COND_EQ = 2, COND_NE = 3, COND_ULT = 8, COND_UGE = 9
} INSN_RCOND;

/* Enum declaration for .  */
typedef enum gr_names {
  H_GR_R0, H_GR_R1, H_GR_R2, H_GR_R3
 , H_GR_R4, H_GR_R5, H_GR_R6, H_GR_R7
 , H_GR_R8, H_GR_R9, H_GR_R10, H_GR_R11
 , H_GR_R12, H_GR_R13, H_GR_R14, H_GR_R15
} GR_NAMES;

/* Enum declaration for .  */
typedef enum ext_names {
  H_EXT_0X1 = 0, H_EXT_0X2 = 1, H_EXT_0X3 = 2, H_EXT_0X4 = 3
 , H_EXT_1 = 0, H_EXT_2 = 1, H_EXT_3 = 2, H_EXT_4 = 3
} EXT_NAMES;

/* Enum declaration for .  */
typedef enum psw_names {
  H_PSW_IEN = 136, H_PSW_R0_11 = 240, H_PSW_R1_11 = 241, H_PSW_R2_11 = 242
 , H_PSW_R3_11 = 243, H_PSW_R4_11 = 244, H_PSW_R5_11 = 245, H_PSW_R6_11 = 246
 , H_PSW_R7_11 = 247, H_PSW_R8_11 = 248, H_PSW_R9_11 = 249, H_PSW_R10_11 = 250
 , H_PSW_R11_11 = 251, H_PSW_R12_11 = 252, H_PSW_R13_11 = 253, H_PSW_R14_11 = 254
 , H_PSW_R15_11 = 255
} PSW_NAMES;

/* Enum declaration for .  */
typedef enum grb_names {
  H_GRB_RL0, H_GRB_RH0, H_GRB_RL1, H_GRB_RH1
 , H_GRB_RL2, H_GRB_RH2, H_GRB_RL3, H_GRB_RH3
 , H_GRB_RL4, H_GRB_RH4, H_GRB_RL5, H_GRB_RH5
 , H_GRB_RL6, H_GRB_RH6, H_GRB_RL7, H_GRB_RH7
} GRB_NAMES;

/* Enum declaration for .  */
typedef enum conditioncode_names {
  H_CC_CC_UC = 0, H_CC_CC_NET = 1, H_CC_CC_Z = 2, H_CC_CC_EQ = 2
 , H_CC_CC_NZ = 3, H_CC_CC_NE = 3, H_CC_CC_V = 4, H_CC_CC_NV = 5
 , H_CC_CC_N = 6, H_CC_CC_NN = 7, H_CC_CC_ULT = 8, H_CC_CC_UGE = 9
 , H_CC_CC_C = 8, H_CC_CC_NC = 9, H_CC_CC_SGT = 10, H_CC_CC_SLE = 11
 , H_CC_CC_SLT = 12, H_CC_CC_SGE = 13, H_CC_CC_UGT = 14, H_CC_CC_ULE = 15
} CONDITIONCODE_NAMES;

/* Enum declaration for .  */
typedef enum extconditioncode_names {
  H_ECC_CC_UC = 0, H_ECC_CC_NET = 2, H_ECC_CC_Z = 4, H_ECC_CC_EQ = 4
 , H_ECC_CC_NZ = 6, H_ECC_CC_NE = 6, H_ECC_CC_V = 8, H_ECC_CC_NV = 10
 , H_ECC_CC_N = 12, H_ECC_CC_NN = 14, H_ECC_CC_ULT = 16, H_ECC_CC_UGE = 18
 , H_ECC_CC_C = 16, H_ECC_CC_NC = 18, H_ECC_CC_SGT = 20, H_ECC_CC_SLE = 22
 , H_ECC_CC_SLT = 24, H_ECC_CC_SGE = 26, H_ECC_CC_UGT = 28, H_ECC_CC_ULE = 30
 , H_ECC_CC_NUSR0 = 1, H_ECC_CC_NUSR1 = 3, H_ECC_CC_USR0 = 5, H_ECC_CC_USR1 = 7
} EXTCONDITIONCODE_NAMES;

/* Enum declaration for .  */
typedef enum grb8_names {
  H_GRB8_DPP0 = 0, H_GRB8_DPP1 = 1, H_GRB8_DPP2 = 2, H_GRB8_DPP3 = 3
 , H_GRB8_PSW = 136, H_GRB8_CP = 8, H_GRB8_MDL = 7, H_GRB8_MDH = 6
 , H_GRB8_MDC = 135, H_GRB8_SP = 9, H_GRB8_CSP = 4, H_GRB8_VECSEG = 137
 , H_GRB8_STKOV = 10, H_GRB8_STKUN = 11, H_GRB8_CPUCON1 = 12, H_GRB8_CPUCON2 = 13
 , H_GRB8_ZEROS = 142, H_GRB8_ONES = 143, H_GRB8_SPSEG = 134, H_GRB8_TFR = 214
 , H_GRB8_RL0 = 240, H_GRB8_RH0 = 241, H_GRB8_RL1 = 242, H_GRB8_RH1 = 243
 , H_GRB8_RL2 = 244, H_GRB8_RH2 = 245, H_GRB8_RL3 = 246, H_GRB8_RH3 = 247
 , H_GRB8_RL4 = 248, H_GRB8_RH4 = 249, H_GRB8_RL5 = 250, H_GRB8_RH5 = 251
 , H_GRB8_RL6 = 252, H_GRB8_RH6 = 253, H_GRB8_RL7 = 254, H_GRB8_RH7 = 255
} GRB8_NAMES;

/* Enum declaration for .  */
typedef enum r8_names {
  H_R8_DPP0 = 0, H_R8_DPP1 = 1, H_R8_DPP2 = 2, H_R8_DPP3 = 3
 , H_R8_PSW = 136, H_R8_CP = 8, H_R8_MDL = 7, H_R8_MDH = 6
 , H_R8_MDC = 135, H_R8_SP = 9, H_R8_CSP = 4, H_R8_VECSEG = 137
 , H_R8_STKOV = 10, H_R8_STKUN = 11, H_R8_CPUCON1 = 12, H_R8_CPUCON2 = 13
 , H_R8_ZEROS = 142, H_R8_ONES = 143, H_R8_SPSEG = 134, H_R8_TFR = 214
 , H_R8_R0 = 240, H_R8_R1 = 241, H_R8_R2 = 242, H_R8_R3 = 243
 , H_R8_R4 = 244, H_R8_R5 = 245, H_R8_R6 = 246, H_R8_R7 = 247
 , H_R8_R8 = 248, H_R8_R9 = 249, H_R8_R10 = 250, H_R8_R11 = 251
 , H_R8_R12 = 252, H_R8_R13 = 253, H_R8_R14 = 254, H_R8_R15 = 255
} R8_NAMES;

/* Enum declaration for .  */
typedef enum regmem8_names {
  H_REGMEM8_DPP0 = 0, H_REGMEM8_DPP1 = 1, H_REGMEM8_DPP2 = 2, H_REGMEM8_DPP3 = 3
 , H_REGMEM8_PSW = 136, H_REGMEM8_CP = 8, H_REGMEM8_MDL = 7, H_REGMEM8_MDH = 6
 , H_REGMEM8_MDC = 135, H_REGMEM8_SP = 9, H_REGMEM8_CSP = 4, H_REGMEM8_VECSEG = 137
 , H_REGMEM8_STKOV = 10, H_REGMEM8_STKUN = 11, H_REGMEM8_CPUCON1 = 12, H_REGMEM8_CPUCON2 = 13
 , H_REGMEM8_ZEROS = 142, H_REGMEM8_ONES = 143, H_REGMEM8_SPSEG = 134, H_REGMEM8_TFR = 214
 , H_REGMEM8_R0 = 240, H_REGMEM8_R1 = 241, H_REGMEM8_R2 = 242, H_REGMEM8_R3 = 243
 , H_REGMEM8_R4 = 244, H_REGMEM8_R5 = 245, H_REGMEM8_R6 = 246, H_REGMEM8_R7 = 247
 , H_REGMEM8_R8 = 248, H_REGMEM8_R9 = 249, H_REGMEM8_R10 = 250, H_REGMEM8_R11 = 251
 , H_REGMEM8_R12 = 252, H_REGMEM8_R13 = 253, H_REGMEM8_R14 = 254, H_REGMEM8_R15 = 255
} REGMEM8_NAMES;

/* Enum declaration for .  */
typedef enum regdiv8_names {
  H_REGDIV8_R0 = 0, H_REGDIV8_R1 = 17, H_REGDIV8_R2 = 34, H_REGDIV8_R3 = 51
 , H_REGDIV8_R4 = 68, H_REGDIV8_R5 = 85, H_REGDIV8_R6 = 102, H_REGDIV8_R7 = 119
 , H_REGDIV8_R8 = 136, H_REGDIV8_R9 = 153, H_REGDIV8_R10 = 170, H_REGDIV8_R11 = 187
 , H_REGDIV8_R12 = 204, H_REGDIV8_R13 = 221, H_REGDIV8_R14 = 238, H_REGDIV8_R15 = 255
} REGDIV8_NAMES;

/* Enum declaration for .  */
typedef enum reg0_name {
  H_REG0_0X1 = 1, H_REG0_0X2 = 2, H_REG0_0X3 = 3, H_REG0_0X4 = 4
 , H_REG0_0X5 = 5, H_REG0_0X6 = 6, H_REG0_0X7 = 7, H_REG0_0X8 = 8
 , H_REG0_0X9 = 9, H_REG0_0XA = 10, H_REG0_0XB = 11, H_REG0_0XC = 12
 , H_REG0_0XD = 13, H_REG0_0XE = 14, H_REG0_0XF = 15, H_REG0_1 = 1
 , H_REG0_2 = 2, H_REG0_3 = 3, H_REG0_4 = 4, H_REG0_5 = 5
 , H_REG0_6 = 6, H_REG0_7 = 7, H_REG0_8 = 8, H_REG0_9 = 9
 , H_REG0_10 = 10, H_REG0_11 = 11, H_REG0_12 = 12, H_REG0_13 = 13
 , H_REG0_14 = 14, H_REG0_15 = 15
} REG0_NAME;

/* Enum declaration for .  */
typedef enum reg0_name1 {
  H_REG01_0X1 = 1, H_REG01_0X2 = 2, H_REG01_0X3 = 3, H_REG01_0X4 = 4
 , H_REG01_0X5 = 5, H_REG01_0X6 = 6, H_REG01_0X7 = 7, H_REG01_1 = 1
 , H_REG01_2 = 2, H_REG01_3 = 3, H_REG01_4 = 4, H_REG01_5 = 5
 , H_REG01_6 = 6, H_REG01_7 = 7
} REG0_NAME1;

/* Enum declaration for .  */
typedef enum regbmem8_names {
  H_REGBMEM8_DPP0 = 0, H_REGBMEM8_DPP1 = 1, H_REGBMEM8_DPP2 = 2, H_REGBMEM8_DPP3 = 3
 , H_REGBMEM8_PSW = 136, H_REGBMEM8_CP = 8, H_REGBMEM8_MDL = 7, H_REGBMEM8_MDH = 6
 , H_REGBMEM8_MDC = 135, H_REGBMEM8_SP = 9, H_REGBMEM8_CSP = 4, H_REGBMEM8_VECSEG = 137
 , H_REGBMEM8_STKOV = 10, H_REGBMEM8_STKUN = 11, H_REGBMEM8_CPUCON1 = 12, H_REGBMEM8_CPUCON2 = 13
 , H_REGBMEM8_ZEROS = 142, H_REGBMEM8_ONES = 143, H_REGBMEM8_SPSEG = 134, H_REGBMEM8_TFR = 214
 , H_REGBMEM8_RL0 = 240, H_REGBMEM8_RH0 = 241, H_REGBMEM8_RL1 = 242, H_REGBMEM8_RH1 = 243
 , H_REGBMEM8_RL2 = 244, H_REGBMEM8_RH2 = 245, H_REGBMEM8_RL3 = 246, H_REGBMEM8_RH3 = 247
 , H_REGBMEM8_RL4 = 248, H_REGBMEM8_RH4 = 249, H_REGBMEM8_RL5 = 250, H_REGBMEM8_RH5 = 251
 , H_REGBMEM8_RL6 = 252, H_REGBMEM8_RH6 = 253, H_REGBMEM8_RL7 = 254, H_REGBMEM8_RH7 = 255
} REGBMEM8_NAMES;

/* Enum declaration for .  */
typedef enum memgr8_names {
  H_MEMGR8_DPP0 = 65024, H_MEMGR8_DPP1 = 65026, H_MEMGR8_DPP2 = 65028, H_MEMGR8_DPP3 = 65030
 , H_MEMGR8_PSW = 65296, H_MEMGR8_CP = 65040, H_MEMGR8_MDL = 65038, H_MEMGR8_MDH = 65036
 , H_MEMGR8_MDC = 65294, H_MEMGR8_SP = 65042, H_MEMGR8_CSP = 65032, H_MEMGR8_VECSEG = 65298
 , H_MEMGR8_STKOV = 65044, H_MEMGR8_STKUN = 65046, H_MEMGR8_CPUCON1 = 65048, H_MEMGR8_CPUCON2 = 65050
 , H_MEMGR8_ZEROS = 65308, H_MEMGR8_ONES = 65310, H_MEMGR8_SPSEG = 65292, H_MEMGR8_TFR = 65452
} MEMGR8_NAMES;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_XC16X, MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_XC16X, ISA_MAX
} ISA_ATTR;

/* Enum declaration for parallel execution pipeline selection.  */
typedef enum pipe_attr {
  PIPE_NONE, PIPE_OS
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

/* Enum declaration for xc16x ifield types.  */
typedef enum ifield_type {
  XC16X_F_NIL, XC16X_F_ANYOF, XC16X_F_OP1, XC16X_F_OP2
 , XC16X_F_CONDCODE, XC16X_F_ICONDCODE, XC16X_F_RCOND, XC16X_F_QCOND
 , XC16X_F_EXTCCODE, XC16X_F_R0, XC16X_F_R1, XC16X_F_R2
 , XC16X_F_R3, XC16X_F_R4, XC16X_F_UIMM2, XC16X_F_UIMM3
 , XC16X_F_UIMM4, XC16X_F_UIMM7, XC16X_F_UIMM8, XC16X_F_UIMM16
 , XC16X_F_MEMORY, XC16X_F_MEMGR8, XC16X_F_REL8, XC16X_F_RELHI8
 , XC16X_F_REG8, XC16X_F_REGMEM8, XC16X_F_REGOFF8, XC16X_F_REGHI8
 , XC16X_F_REGB8, XC16X_F_SEG8, XC16X_F_SEGNUM8, XC16X_F_MASK8
 , XC16X_F_PAGENUM, XC16X_F_DATAHI8, XC16X_F_DATA8, XC16X_F_OFFSET16
 , XC16X_F_OP_BIT1, XC16X_F_OP_BIT2, XC16X_F_OP_BIT4, XC16X_F_OP_BIT3
 , XC16X_F_OP_2BIT, XC16X_F_OP_BITONE, XC16X_F_OP_ONEBIT, XC16X_F_OP_1BIT
 , XC16X_F_OP_LBIT4, XC16X_F_OP_LBIT2, XC16X_F_OP_BIT8, XC16X_F_OP_BIT16
 , XC16X_F_QBIT, XC16X_F_QLOBIT, XC16X_F_QHIBIT, XC16X_F_QLOBIT2
 , XC16X_F_POF, XC16X_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) XC16X_F_MAX)

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

/* Enum declaration for xc16x hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_PC, HW_H_GR, HW_H_EXT
 , HW_H_PSW, HW_H_GRB, HW_H_CC, HW_H_ECC
 , HW_H_GRB8, HW_H_R8, HW_H_REGMEM8, HW_H_REGDIV8
 , HW_H_R0, HW_H_R01, HW_H_REGBMEM8, HW_H_MEMGR8
 , HW_H_COND, HW_H_CBIT, HW_H_SGTDIS, HW_MAX
} CGEN_HW_TYPE;

#define MAX_HW ((int) HW_MAX)

/* Operand attribute indices.  */

/* Enum declaration for cgen_operand attrs.  */
typedef enum cgen_operand_attr {
  CGEN_OPERAND_VIRTUAL, CGEN_OPERAND_PCREL_ADDR, CGEN_OPERAND_ABS_ADDR, CGEN_OPERAND_SIGN_OPT
 , CGEN_OPERAND_SIGNED, CGEN_OPERAND_NEGATIVE, CGEN_OPERAND_RELAX, CGEN_OPERAND_SEM_ONLY
 , CGEN_OPERAND_RELOC, CGEN_OPERAND_HASH_PREFIX, CGEN_OPERAND_DOT_PREFIX, CGEN_OPERAND_POF_PREFIX
 , CGEN_OPERAND_PAG_PREFIX, CGEN_OPERAND_SOF_PREFIX, CGEN_OPERAND_SEG_PREFIX, CGEN_OPERAND_END_BOOLS
 , CGEN_OPERAND_START_NBOOLS = 31, CGEN_OPERAND_MACH, CGEN_OPERAND_END_NBOOLS
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
#define CGEN_ATTR_CGEN_OPERAND_DOT_PREFIX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_DOT_PREFIX)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_POF_PREFIX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_POF_PREFIX)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_PAG_PREFIX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_PAG_PREFIX)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_SOF_PREFIX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_SOF_PREFIX)) != 0)
#define CGEN_ATTR_CGEN_OPERAND_SEG_PREFIX_VALUE(attrs) (((attrs)->bool & (1 << CGEN_OPERAND_SEG_PREFIX)) != 0)

/* Enum declaration for xc16x operand types.  */
typedef enum cgen_operand_type {
  XC16X_OPERAND_PC, XC16X_OPERAND_SR, XC16X_OPERAND_DR, XC16X_OPERAND_DRI
 , XC16X_OPERAND_SRB, XC16X_OPERAND_DRB, XC16X_OPERAND_SR2, XC16X_OPERAND_SRC1
 , XC16X_OPERAND_SRC2, XC16X_OPERAND_SRDIV, XC16X_OPERAND_REGNAM, XC16X_OPERAND_UIMM2
 , XC16X_OPERAND_UIMM3, XC16X_OPERAND_UIMM4, XC16X_OPERAND_UIMM7, XC16X_OPERAND_UIMM8
 , XC16X_OPERAND_UIMM16, XC16X_OPERAND_UPOF16, XC16X_OPERAND_REG8, XC16X_OPERAND_REGMEM8
 , XC16X_OPERAND_REGBMEM8, XC16X_OPERAND_REGOFF8, XC16X_OPERAND_REGHI8, XC16X_OPERAND_REGB8
 , XC16X_OPERAND_GENREG, XC16X_OPERAND_SEG, XC16X_OPERAND_SEGHI8, XC16X_OPERAND_CADDR
 , XC16X_OPERAND_REL, XC16X_OPERAND_RELHI, XC16X_OPERAND_CONDBIT, XC16X_OPERAND_BIT1
 , XC16X_OPERAND_BIT2, XC16X_OPERAND_BIT4, XC16X_OPERAND_LBIT4, XC16X_OPERAND_LBIT2
 , XC16X_OPERAND_BIT8, XC16X_OPERAND_U4, XC16X_OPERAND_BITONE, XC16X_OPERAND_BIT01
 , XC16X_OPERAND_COND, XC16X_OPERAND_ICOND, XC16X_OPERAND_EXTCOND, XC16X_OPERAND_MEMORY
 , XC16X_OPERAND_MEMGR8, XC16X_OPERAND_CBIT, XC16X_OPERAND_QBIT, XC16X_OPERAND_QLOBIT
 , XC16X_OPERAND_QHIBIT, XC16X_OPERAND_MASK8, XC16X_OPERAND_MASKLO8, XC16X_OPERAND_PAGENUM
 , XC16X_OPERAND_DATA8, XC16X_OPERAND_DATAHI8, XC16X_OPERAND_SGTDISBIT, XC16X_OPERAND_UPAG16
 , XC16X_OPERAND_USEG8, XC16X_OPERAND_USEG16, XC16X_OPERAND_USOF16, XC16X_OPERAND_HASH
 , XC16X_OPERAND_DOT, XC16X_OPERAND_POF, XC16X_OPERAND_PAG, XC16X_OPERAND_SOF
 , XC16X_OPERAND_SEGM, XC16X_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 65

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 8

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_END_BOOLS, CGEN_INSN_START_NBOOLS = 31
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

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

extern const struct cgen_ifld xc16x_cgen_ifld_table[];

/* Attributes.  */
extern const CGEN_ATTR_TABLE xc16x_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE xc16x_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE xc16x_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE xc16x_cgen_insn_attr_table[];

/* Hardware decls.  */

extern CGEN_KEYWORD xc16x_cgen_opval_gr_names;
extern CGEN_KEYWORD xc16x_cgen_opval_ext_names;
extern CGEN_KEYWORD xc16x_cgen_opval_psw_names;
extern CGEN_KEYWORD xc16x_cgen_opval_grb_names;
extern CGEN_KEYWORD xc16x_cgen_opval_conditioncode_names;
extern CGEN_KEYWORD xc16x_cgen_opval_extconditioncode_names;
extern CGEN_KEYWORD xc16x_cgen_opval_grb8_names;
extern CGEN_KEYWORD xc16x_cgen_opval_r8_names;
extern CGEN_KEYWORD xc16x_cgen_opval_regmem8_names;
extern CGEN_KEYWORD xc16x_cgen_opval_regdiv8_names;
extern CGEN_KEYWORD xc16x_cgen_opval_reg0_name;
extern CGEN_KEYWORD xc16x_cgen_opval_reg0_name1;
extern CGEN_KEYWORD xc16x_cgen_opval_regbmem8_names;
extern CGEN_KEYWORD xc16x_cgen_opval_memgr8_names;

extern const CGEN_HW_ENTRY xc16x_cgen_hw_table[];



#endif /* XC16X_CPU_H */
