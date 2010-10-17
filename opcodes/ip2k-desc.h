/* CPU data header for ip2k.

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

#ifndef IP2K_CPU_H
#define IP2K_CPU_H

#define CGEN_ARCH ip2k

/* Given symbol S, return ip2k_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) ip2k##_cgen_##s
#else
#define CGEN_SYM(s) ip2k/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_IP2KBF

#define CGEN_INSN_LSB0_P 1

/* Minimum size of any insn (in bytes).  */
#define CGEN_MIN_INSN_SIZE 2

/* Maximum size of any insn (in bytes).  */
#define CGEN_MAX_INSN_SIZE 2

#define CGEN_INT_INSN_P 1

/* Maximum number of syntax elements in an instruction.  */
#define CGEN_ACTUAL_MAX_SYNTAX_ELEMENTS 12

/* CGEN_MNEMONIC_OPERANDS is defined if mnemonics have operands.
   e.g. In "b,a foo" the ",a" is an operand.  If mnemonics have operands
   we can't hash on everything up to the space.  */
#define CGEN_MNEMONIC_OPERANDS

/* Maximum number of fields in an instruction.  */
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 3

/* Enums.  */

/* Enum declaration for op6 enums.  */
typedef enum insn_op6 {
  OP6_OTHER1, OP6_OTHER2, OP6_SUB, OP6_DEC
 , OP6_OR, OP6_AND, OP6_XOR, OP6_ADD
 , OP6_TEST, OP6_NOT, OP6_INC, OP6_DECSZ
 , OP6_RR, OP6_RL, OP6_SWAP, OP6_INCSZ
 , OP6_CSE, OP6_POP, OP6_SUBC, OP6_DECSNZ
 , OP6_MULU, OP6_MULS, OP6_INCSNZ, OP6_ADDC
} INSN_OP6;

/* Enum declaration for dir enums.  */
typedef enum insn_dir {
  DIR_TO_W, DIR_NOTTO_W
} INSN_DIR;

/* Enum declaration for op4 enums.  */
typedef enum insn_op4 {
  OP4_LITERAL = 7, OP4_CLRB = 8, OP4_SETB = 9, OP4_SNB = 10
 , OP4_SB = 11
} INSN_OP4;

/* Enum declaration for op4mid enums.  */
typedef enum insn_op4mid {
  OP4MID_LOADH_L = 0, OP4MID_LOADL_L = 1, OP4MID_MULU_L = 2, OP4MID_MULS_L = 3
 , OP4MID_PUSH_L = 4, OP4MID_CSNE_L = 6, OP4MID_CSE_L = 7, OP4MID_RETW_L = 8
 , OP4MID_CMP_L = 9, OP4MID_SUB_L = 10, OP4MID_ADD_L = 11, OP4MID_MOV_L = 12
 , OP4MID_OR_L = 13, OP4MID_AND_L = 14, OP4MID_XOR_L = 15
} INSN_OP4MID;

/* Enum declaration for op3 enums.  */
typedef enum insn_op3 {
  OP3_CALL = 6, OP3_JMP = 7
} INSN_OP3;

/* Enum declaration for .  */
typedef enum register_names {
  H_REGISTERS_ADDRSEL = 2, H_REGISTERS_ADDRX = 3, H_REGISTERS_IPH = 4, H_REGISTERS_IPL = 5
 , H_REGISTERS_SPH = 6, H_REGISTERS_SPL = 7, H_REGISTERS_PCH = 8, H_REGISTERS_PCL = 9
 , H_REGISTERS_WREG = 10, H_REGISTERS_STATUS = 11, H_REGISTERS_DPH = 12, H_REGISTERS_DPL = 13
 , H_REGISTERS_SPDREG = 14, H_REGISTERS_MULH = 15, H_REGISTERS_ADDRH = 16, H_REGISTERS_ADDRL = 17
 , H_REGISTERS_DATAH = 18, H_REGISTERS_DATAL = 19, H_REGISTERS_INTVECH = 20, H_REGISTERS_INTVECL = 21
 , H_REGISTERS_INTSPD = 22, H_REGISTERS_INTF = 23, H_REGISTERS_INTE = 24, H_REGISTERS_INTED = 25
 , H_REGISTERS_FCFG = 26, H_REGISTERS_TCTRL = 27, H_REGISTERS_XCFG = 28, H_REGISTERS_EMCFG = 29
 , H_REGISTERS_IPCH = 30, H_REGISTERS_IPCL = 31, H_REGISTERS_RAIN = 32, H_REGISTERS_RAOUT = 33
 , H_REGISTERS_RADIR = 34, H_REGISTERS_LFSRH = 35, H_REGISTERS_RBIN = 36, H_REGISTERS_RBOUT = 37
 , H_REGISTERS_RBDIR = 38, H_REGISTERS_LFSRL = 39, H_REGISTERS_RCIN = 40, H_REGISTERS_RCOUT = 41
 , H_REGISTERS_RCDIR = 42, H_REGISTERS_LFSRA = 43, H_REGISTERS_RDIN = 44, H_REGISTERS_RDOUT = 45
 , H_REGISTERS_RDDIR = 46, H_REGISTERS_REIN = 48, H_REGISTERS_REOUT = 49, H_REGISTERS_REDIR = 50
 , H_REGISTERS_RFIN = 52, H_REGISTERS_RFOUT = 53, H_REGISTERS_RFDIR = 54, H_REGISTERS_RGOUT = 57
 , H_REGISTERS_RGDIR = 58, H_REGISTERS_RTTMR = 64, H_REGISTERS_RTCFG = 65, H_REGISTERS_T0TMR = 66
 , H_REGISTERS_T0CFG = 67, H_REGISTERS_T1CNTH = 68, H_REGISTERS_T1CNTL = 69, H_REGISTERS_T1CAP1H = 70
 , H_REGISTERS_T1CAP1L = 71, H_REGISTERS_T1CAP2H = 72, H_REGISTERS_T1CMP2H = 72, H_REGISTERS_T1CAP2L = 73
 , H_REGISTERS_T1CMP2L = 73, H_REGISTERS_T1CMP1H = 74, H_REGISTERS_T1CMP1L = 75, H_REGISTERS_T1CFG1H = 76
 , H_REGISTERS_T1CFG1L = 77, H_REGISTERS_T1CFG2H = 78, H_REGISTERS_T1CFG2L = 79, H_REGISTERS_ADCH = 80
 , H_REGISTERS_ADCL = 81, H_REGISTERS_ADCCFG = 82, H_REGISTERS_ADCTMR = 83, H_REGISTERS_T2CNTH = 84
 , H_REGISTERS_T2CNTL = 85, H_REGISTERS_T2CAP1H = 86, H_REGISTERS_T2CAP1L = 87, H_REGISTERS_T2CAP2H = 88
 , H_REGISTERS_T2CMP2H = 88, H_REGISTERS_T2CAP2L = 89, H_REGISTERS_T2CMP2L = 89, H_REGISTERS_T2CMP1H = 90
 , H_REGISTERS_T2CMP1L = 91, H_REGISTERS_T2CFG1H = 92, H_REGISTERS_T2CFG1L = 93, H_REGISTERS_T2CFG2H = 94
 , H_REGISTERS_T2CFG2L = 95, H_REGISTERS_S1TMRH = 96, H_REGISTERS_S1TMRL = 97, H_REGISTERS_S1TBUFH = 98
 , H_REGISTERS_S1TBUFL = 99, H_REGISTERS_S1TCFG = 100, H_REGISTERS_S1RCNT = 101, H_REGISTERS_S1RBUFH = 102
 , H_REGISTERS_S1RBUFL = 103, H_REGISTERS_S1RCFG = 104, H_REGISTERS_S1RSYNC = 105, H_REGISTERS_S1INTF = 106
 , H_REGISTERS_S1INTE = 107, H_REGISTERS_S1MODE = 108, H_REGISTERS_S1SMASK = 109, H_REGISTERS_PSPCFG = 110
 , H_REGISTERS_CMPCFG = 111, H_REGISTERS_S2TMRH = 112, H_REGISTERS_S2TMRL = 113, H_REGISTERS_S2TBUFH = 114
 , H_REGISTERS_S2TBUFL = 115, H_REGISTERS_S2TCFG = 116, H_REGISTERS_S2RCNT = 117, H_REGISTERS_S2RBUFH = 118
 , H_REGISTERS_S2RBUFL = 119, H_REGISTERS_S2RCFG = 120, H_REGISTERS_S2RSYNC = 121, H_REGISTERS_S2INTF = 122
 , H_REGISTERS_S2INTE = 123, H_REGISTERS_S2MODE = 124, H_REGISTERS_S2SMASK = 125, H_REGISTERS_CALLH = 126
 , H_REGISTERS_CALLL = 127
} REGISTER_NAMES;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_IP2022, MACH_IP2022EXT, MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_IP2K, ISA_MAX
} ISA_ATTR;

/* Number of architecture variants.  */
#define MAX_ISAS  1
#define MAX_MACHS ((int) MACH_MAX)

/* Ifield support.  */

extern const struct cgen_ifld ip2k_cgen_ifld_table[];

/* Ifield attribute indices.  */

/* Enum declaration for cgen_ifld attrs.  */
typedef enum cgen_ifld_attr {
  CGEN_IFLD_VIRTUAL, CGEN_IFLD_PCREL_ADDR, CGEN_IFLD_ABS_ADDR, CGEN_IFLD_RESERVED
 , CGEN_IFLD_SIGN_OPT, CGEN_IFLD_SIGNED, CGEN_IFLD_END_BOOLS, CGEN_IFLD_START_NBOOLS = 31
 , CGEN_IFLD_MACH, CGEN_IFLD_END_NBOOLS
} CGEN_IFLD_ATTR;

/* Number of non-boolean elements in cgen_ifld_attr.  */
#define CGEN_IFLD_NBOOL_ATTRS (CGEN_IFLD_END_NBOOLS - CGEN_IFLD_START_NBOOLS - 1)

/* Enum declaration for ip2k ifield types.  */
typedef enum ifield_type {
  IP2K_F_NIL, IP2K_F_ANYOF, IP2K_F_IMM8, IP2K_F_REG
 , IP2K_F_ADDR16CJP, IP2K_F_DIR, IP2K_F_BITNO, IP2K_F_OP3
 , IP2K_F_OP4, IP2K_F_OP4MID, IP2K_F_OP6, IP2K_F_OP8
 , IP2K_F_OP6_10LOW, IP2K_F_OP6_7LOW, IP2K_F_RETI3, IP2K_F_SKIPB
 , IP2K_F_PAGE3, IP2K_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) IP2K_F_MAX)

/* Hardware attribute indices.  */

/* Enum declaration for cgen_hw attrs.  */
typedef enum cgen_hw_attr {
  CGEN_HW_VIRTUAL, CGEN_HW_CACHE_ADDR, CGEN_HW_PC, CGEN_HW_PROFILE
 , CGEN_HW_END_BOOLS, CGEN_HW_START_NBOOLS = 31, CGEN_HW_MACH, CGEN_HW_END_NBOOLS
} CGEN_HW_ATTR;

/* Number of non-boolean elements in cgen_hw_attr.  */
#define CGEN_HW_NBOOL_ATTRS (CGEN_HW_END_NBOOLS - CGEN_HW_START_NBOOLS - 1)

/* Enum declaration for ip2k hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_SPR, HW_H_REGISTERS, HW_H_STACK
 , HW_H_PABITS, HW_H_ZBIT, HW_H_CBIT, HW_H_DCBIT
 , HW_H_PC, HW_MAX
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

/* Enum declaration for ip2k operand types.  */
typedef enum cgen_operand_type {
  IP2K_OPERAND_PC, IP2K_OPERAND_ADDR16CJP, IP2K_OPERAND_FR, IP2K_OPERAND_LIT8
 , IP2K_OPERAND_BITNO, IP2K_OPERAND_ADDR16P, IP2K_OPERAND_ADDR16H, IP2K_OPERAND_ADDR16L
 , IP2K_OPERAND_RETI3, IP2K_OPERAND_PABITS, IP2K_OPERAND_ZBIT, IP2K_OPERAND_CBIT
 , IP2K_OPERAND_DCBIT, IP2K_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 13

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 8

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_EXT_SKIP_INSN, CGEN_INSN_SKIPA
 , CGEN_INSN_END_BOOLS, CGEN_INSN_START_NBOOLS = 31, CGEN_INSN_MACH, CGEN_INSN_END_NBOOLS
} CGEN_INSN_ATTR;

/* Number of non-boolean elements in cgen_insn_attr.  */
#define CGEN_INSN_NBOOL_ATTRS (CGEN_INSN_END_NBOOLS - CGEN_INSN_START_NBOOLS - 1)

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

/* Attributes.  */
extern const CGEN_ATTR_TABLE ip2k_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE ip2k_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE ip2k_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE ip2k_cgen_insn_attr_table[];

/* Hardware decls.  */





#endif /* IP2K_CPU_H */
