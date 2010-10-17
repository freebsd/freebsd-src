/* CPU data header for iq2000.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef IQ2000_CPU_H
#define IQ2000_CPU_H

#define CGEN_ARCH iq2000

/* Given symbol S, return iq2000_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) iq2000##_cgen_##s
#else
#define CGEN_SYM(s) iq2000/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_IQ2000BF
#define HAVE_CPU_IQ10BF

#define CGEN_INSN_LSB0_P 1

/* Minimum size of any insn (in bytes).  */
#define CGEN_MIN_INSN_SIZE 3

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
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 8

/* Enums.  */

/* Enum declaration for .  */
typedef enum gr_names {
  H_GR_R0 = 0, H_GR__0 = 0, H_GR_R1 = 1, H_GR__1 = 1
 , H_GR_R2 = 2, H_GR__2 = 2, H_GR_R3 = 3, H_GR__3 = 3
 , H_GR_R4 = 4, H_GR__4 = 4, H_GR_R5 = 5, H_GR__5 = 5
 , H_GR_R6 = 6, H_GR__6 = 6, H_GR_R7 = 7, H_GR__7 = 7
 , H_GR_R8 = 8, H_GR__8 = 8, H_GR_R9 = 9, H_GR__9 = 9
 , H_GR_R10 = 10, H_GR__10 = 10, H_GR_R11 = 11, H_GR__11 = 11
 , H_GR_R12 = 12, H_GR__12 = 12, H_GR_R13 = 13, H_GR__13 = 13
 , H_GR_R14 = 14, H_GR__14 = 14, H_GR_R15 = 15, H_GR__15 = 15
 , H_GR_R16 = 16, H_GR__16 = 16, H_GR_R17 = 17, H_GR__17 = 17
 , H_GR_R18 = 18, H_GR__18 = 18, H_GR_R19 = 19, H_GR__19 = 19
 , H_GR_R20 = 20, H_GR__20 = 20, H_GR_R21 = 21, H_GR__21 = 21
 , H_GR_R22 = 22, H_GR__22 = 22, H_GR_R23 = 23, H_GR__23 = 23
 , H_GR_R24 = 24, H_GR__24 = 24, H_GR_R25 = 25, H_GR__25 = 25
 , H_GR_R26 = 26, H_GR__26 = 26, H_GR_R27 = 27, H_GR__27 = 27
 , H_GR_R28 = 28, H_GR__28 = 28, H_GR_R29 = 29, H_GR__29 = 29
 , H_GR_R30 = 30, H_GR__30 = 30, H_GR_R31 = 31, H_GR__31 = 31
} GR_NAMES;

/* Enum declaration for primary opcodes.  */
typedef enum opcodes {
  OP_SPECIAL = 0, OP_REGIMM = 1, OP_J = 2, OP_JAL = 3
 , OP_BEQ = 4, OP_BNE = 5, OP_BLEZ = 6, OP_BGTZ = 7
 , OP_ADDI = 8, OP_ADDIU = 9, OP_SLTI = 10, OP_SLTIU = 11
 , OP_ANDI = 12, OP_ORI = 13, OP_XORI = 14, OP_LUI = 15
 , OP_COP0 = 16, OP_COP1 = 17, OP_COP2 = 18, OP_COP3 = 19
 , OP_BEQL = 20, OP_BNEL = 21, OP_BLEZL = 22, OP_BGTZL = 23
 , OP_BMB0 = 24, OP_BMB1 = 25, OP_BMB2 = 26, OP_BMB3 = 27
 , OP_BBI = 28, OP_BBV = 29, OP_BBIN = 30, OP_BBVN = 31
 , OP_LB = 32, OP_LH = 33, OP_LW = 35, OP_LBU = 36
 , OP_LHU = 37, OP_RAM = 39, OP_SB = 40, OP_SH = 41
 , OP_SW = 43, OP_ANDOI = 44, OP_BMB = 45, OP_ORUI = 47
 , OP_LDW = 48, OP_SDW = 56, OP_ANDOUI = 63
} OPCODES;

/* Enum declaration for iq10-only primary opcodes.  */
typedef enum q10_opcodes {
  OP10_BMB = 6, OP10_ORUI = 15, OP10_BMBL = 22, OP10_ANDOUI = 47
 , OP10_BBIL = 60, OP10_BBVL = 61, OP10_BBINL = 62, OP10_BBVNL = 63
} Q10_OPCODES;

/* Enum declaration for branch sub-opcodes.  */
typedef enum regimm_functions {
  FUNC_BLTZ = 0, FUNC_BGEZ = 1, FUNC_BLTZL = 2, FUNC_BGEZL = 3
 , FUNC_BLEZ = 4, FUNC_BGTZ = 5, FUNC_BLEZL = 6, FUNC_BGTZL = 7
 , FUNC_BRI = 8, FUNC_BRV = 9, FUNC_BCTX = 12, FUNC_BLTZAL = 16
 , FUNC_BGEZAL = 17, FUNC_BLTZALL = 18, FUNC_BGEZALL = 19, FUNC_BLEZAL = 20
 , FUNC_BGTZAL = 21, FUNC_BLEZALL = 22, FUNC_BGTZALL = 23
} REGIMM_FUNCTIONS;

/* Enum declaration for function sub-opcodes.  */
typedef enum functions {
  FUNC_SLL = 0, FUNC_SLMV = 1, FUNC_SRL = 2, FUNC_SRA = 3
 , FUNC_SLLV = 4, FUNC_SRMV = 5, FUNC_SRLV = 6, FUNC_SRAV = 7
 , FUNC_JR = 8, FUNC_JALR = 9, FUNC_JCR = 10, FUNC_SYSCALL = 12
 , FUNC_BREAK = 13, FUNC_SLEEP = 14, FUNC_ADD = 32, FUNC_ADDU = 33
 , FUNC_SUB = 34, FUNC_SUBU = 35, FUNC_AND = 36, FUNC_OR = 37
 , FUNC_XOR = 38, FUNC_NOR = 39, FUNC_ADO16 = 41, FUNC_SLT = 42
 , FUNC_SLTU = 43, FUNC_MRGB = 45
} FUNCTIONS;

/* Enum declaration for iq10-only special function sub-opcodes.  */
typedef enum q10s_functions {
  FUNC10_YIELD = 14, FUNC10_CNT1S = 46
} Q10S_FUNCTIONS;

/* Enum declaration for iq10 function sub-opcodes.  */
typedef enum cop_functions {
  FUNC10_CFC = 0, FUNC10_LOCK = 1, FUNC10_CTC = 2, FUNC10_UNLK = 3
 , FUNC10_SWRD = 4, FUNC10_SWRDL = 5, FUNC10_SWWR = 6, FUNC10_SWWRU = 7
 , FUNC10_RBA = 8, FUNC10_RBAL = 9, FUNC10_RBAR = 10, FUNC10_DWRD = 12
 , FUNC10_DWRDL = 13, FUNC10_WBA = 16, FUNC10_WBAU = 17, FUNC10_WBAC = 18
 , FUNC10_CRC32 = 20, FUNC10_CRC32B = 21, FUNC10_MCID = 32, FUNC10_DBD = 33
 , FUNC10_DBA = 34, FUNC10_DPWT = 35, FUNC10_AVAIL = 36, FUNC10_FREE = 37
 , FUNC10_CHKHDR = 38, FUNC10_TSTOD = 39, FUNC10_PKRLA = 40, FUNC10_PKRLAU = 41
 , FUNC10_PKRLAH = 42, FUNC10_PKRLAC = 43, FUNC10_CMPHDR = 44, FUNC10_CM64RS = 0
 , FUNC10_CM64RD = 1, FUNC10_CM64RI = 4, FUNC10_CM64CLR = 5, FUNC10_CM64SS = 8
 , FUNC10_CM64SD = 9, FUNC10_CM64SI = 12, FUNC10_CM64RA = 16, FUNC10_CM64RIA2 = 20
 , FUNC10_CM128RIA2 = 21, FUNC10_CM64SA = 24, FUNC10_CM64SIA2 = 28, FUNC10_CM128SIA2 = 29
 , FUNC10_CM32RS = 32, FUNC10_CM32RD = 33, FUNC10_CM32XOR = 34, FUNC10_CM32ANDN = 35
 , FUNC10_CM32RI = 36, FUNC10_CM128VSA = 38, FUNC10_CM32SS = 40, FUNC10_CM32SD = 41
 , FUNC10_CM32OR = 42, FUNC10_CM32AND = 43, FUNC10_CM32SI = 44, FUNC10_CM32RA = 48
 , FUNC10_CM32SA = 56
} COP_FUNCTIONS;

/* Enum declaration for iq10 function sub-opcodes.  */
typedef enum cop_cm128_4functions {
  FUNC10_CM128RIA3 = 4, FUNC10_CM128SIA3 = 6
} COP_CM128_4FUNCTIONS;

/* Enum declaration for iq10 function sub-opcodes.  */
typedef enum cop_cm128_3functions {
  FUNC10_CM128RIA4 = 6, FUNC10_CM128SIA4 = 7
} COP_CM128_3FUNCTIONS;

/* Enum declaration for iq10 coprocessor sub-opcodes.  */
typedef enum cop2_functions {
  FUNC10_PKRLI = 0, FUNC10_PKRLIU = 1, FUNC10_PKRLIH = 2, FUNC10_PKRLIC = 3
 , FUNC10_RBIR = 1, FUNC10_RBI = 2, FUNC10_RBIL = 3, FUNC10_WBIC = 5
 , FUNC10_WBI = 6, FUNC10_WBIU = 7
} COP2_FUNCTIONS;

/* Enum declaration for iq10 coprocessor cam sub-opcodes.  */
typedef enum cop3_cam_functions {
  FUNC10_CAM36 = 16, FUNC10_CAM72 = 17, FUNC10_CAM144 = 18, FUNC10_CAM288 = 19
} COP3_CAM_FUNCTIONS;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_IQ2000, MACH_IQ10, MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_IQ2000, ISA_MAX
} ISA_ATTR;

/* Number of architecture variants.  */
#define MAX_ISAS  1
#define MAX_MACHS ((int) MACH_MAX)

/* Ifield support.  */

extern const struct cgen_ifld iq2000_cgen_ifld_table[];

/* Ifield attribute indices.  */

/* Enum declaration for cgen_ifld attrs.  */
typedef enum cgen_ifld_attr {
  CGEN_IFLD_VIRTUAL, CGEN_IFLD_PCREL_ADDR, CGEN_IFLD_ABS_ADDR, CGEN_IFLD_RESERVED
 , CGEN_IFLD_SIGN_OPT, CGEN_IFLD_SIGNED, CGEN_IFLD_END_BOOLS, CGEN_IFLD_START_NBOOLS = 31
 , CGEN_IFLD_MACH, CGEN_IFLD_END_NBOOLS
} CGEN_IFLD_ATTR;

/* Number of non-boolean elements in cgen_ifld_attr.  */
#define CGEN_IFLD_NBOOL_ATTRS (CGEN_IFLD_END_NBOOLS - CGEN_IFLD_START_NBOOLS - 1)

/* Enum declaration for iq2000 ifield types.  */
typedef enum ifield_type {
  IQ2000_F_NIL, IQ2000_F_ANYOF, IQ2000_F_OPCODE, IQ2000_F_RS
 , IQ2000_F_RT, IQ2000_F_RD, IQ2000_F_SHAMT, IQ2000_F_CP_OP
 , IQ2000_F_CP_OP_10, IQ2000_F_CP_GRP, IQ2000_F_FUNC, IQ2000_F_IMM
 , IQ2000_F_RD_RS, IQ2000_F_RD_RT, IQ2000_F_RT_RS, IQ2000_F_JTARG
 , IQ2000_F_JTARGQ10, IQ2000_F_OFFSET, IQ2000_F_COUNT, IQ2000_F_BYTECOUNT
 , IQ2000_F_INDEX, IQ2000_F_MASK, IQ2000_F_MASKQ10, IQ2000_F_MASKL
 , IQ2000_F_EXCODE, IQ2000_F_RSRVD, IQ2000_F_10_11, IQ2000_F_24_19
 , IQ2000_F_5, IQ2000_F_10, IQ2000_F_25, IQ2000_F_CAM_Z
 , IQ2000_F_CAM_Y, IQ2000_F_CM_3FUNC, IQ2000_F_CM_4FUNC, IQ2000_F_CM_3Z
 , IQ2000_F_CM_4Z, IQ2000_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) IQ2000_F_MAX)

/* Hardware attribute indices.  */

/* Enum declaration for cgen_hw attrs.  */
typedef enum cgen_hw_attr {
  CGEN_HW_VIRTUAL, CGEN_HW_CACHE_ADDR, CGEN_HW_PC, CGEN_HW_PROFILE
 , CGEN_HW_END_BOOLS, CGEN_HW_START_NBOOLS = 31, CGEN_HW_MACH, CGEN_HW_END_NBOOLS
} CGEN_HW_ATTR;

/* Number of non-boolean elements in cgen_hw_attr.  */
#define CGEN_HW_NBOOL_ATTRS (CGEN_HW_END_NBOOLS - CGEN_HW_START_NBOOLS - 1)

/* Enum declaration for iq2000 hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_PC, HW_H_GR, HW_MAX
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

/* Enum declaration for iq2000 operand types.  */
typedef enum cgen_operand_type {
  IQ2000_OPERAND_PC, IQ2000_OPERAND_RS, IQ2000_OPERAND_RT, IQ2000_OPERAND_RD
 , IQ2000_OPERAND_RD_RS, IQ2000_OPERAND_RD_RT, IQ2000_OPERAND_RT_RS, IQ2000_OPERAND_SHAMT
 , IQ2000_OPERAND_IMM, IQ2000_OPERAND_OFFSET, IQ2000_OPERAND_BASEOFF, IQ2000_OPERAND_JMPTARG
 , IQ2000_OPERAND_MASK, IQ2000_OPERAND_MASKQ10, IQ2000_OPERAND_MASKL, IQ2000_OPERAND_COUNT
 , IQ2000_OPERAND_F_INDEX, IQ2000_OPERAND_EXECODE, IQ2000_OPERAND_BYTECOUNT, IQ2000_OPERAND_CAM_Y
 , IQ2000_OPERAND_CAM_Z, IQ2000_OPERAND_CM_3FUNC, IQ2000_OPERAND_CM_4FUNC, IQ2000_OPERAND_CM_3Z
 , IQ2000_OPERAND_CM_4Z, IQ2000_OPERAND_BASE, IQ2000_OPERAND_MASKR, IQ2000_OPERAND_BITNUM
 , IQ2000_OPERAND_HI16, IQ2000_OPERAND_LO16, IQ2000_OPERAND_MLO16, IQ2000_OPERAND_JMPTARGQ10
 , IQ2000_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 32

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 8

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_YIELD_INSN, CGEN_INSN_LOAD_DELAY
 , CGEN_INSN_EVEN_REG_NUM, CGEN_INSN_UNSUPPORTED, CGEN_INSN_USES_RD, CGEN_INSN_USES_RS
 , CGEN_INSN_USES_RT, CGEN_INSN_USES_R31, CGEN_INSN_END_BOOLS, CGEN_INSN_START_NBOOLS = 31
 , CGEN_INSN_MACH, CGEN_INSN_END_NBOOLS
} CGEN_INSN_ATTR;

/* Number of non-boolean elements in cgen_insn_attr.  */
#define CGEN_INSN_NBOOL_ATTRS (CGEN_INSN_END_NBOOLS - CGEN_INSN_START_NBOOLS - 1)

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

/* Attributes.  */
extern const CGEN_ATTR_TABLE iq2000_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE iq2000_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE iq2000_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE iq2000_cgen_insn_attr_table[];

/* Hardware decls.  */

extern CGEN_KEYWORD iq2000_cgen_opval_gr_names;




#endif /* IQ2000_CPU_H */
