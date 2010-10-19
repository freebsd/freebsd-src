/* CPU data header for mt.

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

#ifndef MT_CPU_H
#define MT_CPU_H

#include "opcode/cgen-bitset.h"

#define CGEN_ARCH mt

/* Given symbol S, return mt_cgen_<S>.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define CGEN_SYM(s) mt##_cgen_##s
#else
#define CGEN_SYM(s) mt/**/_cgen_/**/s
#endif


/* Selected cpu families.  */
#define HAVE_CPU_MS1BF
#define HAVE_CPU_MS1_003BF
#define HAVE_CPU_MS2BF

#define CGEN_INSN_LSB0_P 1

/* Minimum size of any insn (in bytes).  */
#define CGEN_MIN_INSN_SIZE 4

/* Maximum size of any insn (in bytes).  */
#define CGEN_MAX_INSN_SIZE 4

#define CGEN_INT_INSN_P 1

/* Maximum number of syntax elements in an instruction.  */
#define CGEN_ACTUAL_MAX_SYNTAX_ELEMENTS 40

/* CGEN_MNEMONIC_OPERANDS is defined if mnemonics have operands.
   e.g. In "b,a foo" the ",a" is an operand.  If mnemonics have operands
   we can't hash on everything up to the space.  */
#define CGEN_MNEMONIC_OPERANDS

/* Maximum number of fields in an instruction.  */
#define CGEN_ACTUAL_MAX_IFMT_OPERANDS 14

/* Enums.  */

/* Enum declaration for msys enums.  */
typedef enum insn_msys {
  MSYS_NO, MSYS_YES
} INSN_MSYS;

/* Enum declaration for opc enums.  */
typedef enum insn_opc {
  OPC_ADD = 0, OPC_ADDU = 1, OPC_SUB = 2, OPC_SUBU = 3
 , OPC_MUL = 4, OPC_AND = 8, OPC_OR = 9, OPC_XOR = 10
 , OPC_NAND = 11, OPC_NOR = 12, OPC_XNOR = 13, OPC_LDUI = 14
 , OPC_LSL = 16, OPC_LSR = 17, OPC_ASR = 18, OPC_BRLT = 24
 , OPC_BRLE = 25, OPC_BREQ = 26, OPC_JMP = 27, OPC_JAL = 28
 , OPC_BRNEQ = 29, OPC_DBNZ = 30, OPC_LOOP = 31, OPC_LDW = 32
 , OPC_STW = 33, OPC_EI = 48, OPC_DI = 49, OPC_SI = 50
 , OPC_RETI = 51, OPC_BREAK = 52, OPC_IFLUSH = 53
} INSN_OPC;

/* Enum declaration for msopc enums.  */
typedef enum insn_msopc {
  MSOPC_LDCTXT, MSOPC_LDFB, MSOPC_STFB, MSOPC_FBCB
 , MSOPC_MFBCB, MSOPC_FBCCI, MSOPC_FBRCI, MSOPC_FBCRI
 , MSOPC_FBRRI, MSOPC_MFBCCI, MSOPC_MFBRCI, MSOPC_MFBCRI
 , MSOPC_MFBRRI, MSOPC_FBCBDR, MSOPC_RCFBCB, MSOPC_MRCFBCB
 , MSOPC_CBCAST, MSOPC_DUPCBCAST, MSOPC_WFBI, MSOPC_WFB
 , MSOPC_RCRISC, MSOPC_FBCBINC, MSOPC_RCXMODE, MSOPC_INTLVR
 , MSOPC_WFBINC, MSOPC_MWFBINC, MSOPC_WFBINCR, MSOPC_MWFBINCR
 , MSOPC_FBCBINCS, MSOPC_MFBCBINCS, MSOPC_FBCBINCRS, MSOPC_MFBCBINCRS
} INSN_MSOPC;

/* Enum declaration for imm enums.  */
typedef enum insn_imm {
  IMM_NO, IMM_YES
} INSN_IMM;

/* Enum declaration for .  */
typedef enum msys_syms {
  H_NIL_DUP = 1, H_NIL_XX = 0
} MSYS_SYMS;

/* Attributes.  */

/* Enum declaration for machine type selection.  */
typedef enum mach_attr {
  MACH_BASE, MACH_MS1, MACH_MS1_003, MACH_MS2
 , MACH_MAX
} MACH_ATTR;

/* Enum declaration for instruction set selection.  */
typedef enum isa_attr {
  ISA_MT, ISA_MAX
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

/* Enum declaration for mt ifield types.  */
typedef enum ifield_type {
  MT_F_NIL, MT_F_ANYOF, MT_F_MSYS, MT_F_OPC
 , MT_F_IMM, MT_F_UU24, MT_F_SR1, MT_F_SR2
 , MT_F_DR, MT_F_DRRR, MT_F_IMM16U, MT_F_IMM16S
 , MT_F_IMM16A, MT_F_UU4A, MT_F_UU4B, MT_F_UU12
 , MT_F_UU8, MT_F_UU16, MT_F_UU1, MT_F_MSOPC
 , MT_F_UU_26_25, MT_F_MASK, MT_F_BANKADDR, MT_F_RDA
 , MT_F_UU_2_25, MT_F_RBBC, MT_F_PERM, MT_F_MODE
 , MT_F_UU_1_24, MT_F_WR, MT_F_FBINCR, MT_F_UU_2_23
 , MT_F_XMODE, MT_F_A23, MT_F_MASK1, MT_F_CR
 , MT_F_TYPE, MT_F_INCAMT, MT_F_CBS, MT_F_UU_1_19
 , MT_F_BALL, MT_F_COLNUM, MT_F_BRC, MT_F_INCR
 , MT_F_FBDISP, MT_F_UU_4_15, MT_F_LENGTH, MT_F_UU_1_15
 , MT_F_RC, MT_F_RCNUM, MT_F_ROWNUM, MT_F_CBX
 , MT_F_ID, MT_F_SIZE, MT_F_ROWNUM1, MT_F_UU_3_11
 , MT_F_RC1, MT_F_CCB, MT_F_CBRB, MT_F_CDB
 , MT_F_ROWNUM2, MT_F_CELL, MT_F_UU_3_9, MT_F_CONTNUM
 , MT_F_UU_1_6, MT_F_DUP, MT_F_RC2, MT_F_CTXDISP
 , MT_F_IMM16L, MT_F_LOOPO, MT_F_CB1SEL, MT_F_CB2SEL
 , MT_F_CB1INCR, MT_F_CB2INCR, MT_F_RC3, MT_F_MSYSFRSR2
 , MT_F_BRC2, MT_F_BALL2, MT_F_MAX
} IFIELD_TYPE;

#define MAX_IFLD ((int) MT_F_MAX)

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

/* Enum declaration for mt hardware types.  */
typedef enum cgen_hw_type {
  HW_H_MEMORY, HW_H_SINT, HW_H_UINT, HW_H_ADDR
 , HW_H_IADDR, HW_H_SPR, HW_H_PC, HW_MAX
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

/* Enum declaration for mt operand types.  */
typedef enum cgen_operand_type {
  MT_OPERAND_PC, MT_OPERAND_FRSR1, MT_OPERAND_FRSR2, MT_OPERAND_FRDR
 , MT_OPERAND_FRDRRR, MT_OPERAND_IMM16, MT_OPERAND_IMM16Z, MT_OPERAND_IMM16O
 , MT_OPERAND_RC, MT_OPERAND_RCNUM, MT_OPERAND_CONTNUM, MT_OPERAND_RBBC
 , MT_OPERAND_COLNUM, MT_OPERAND_ROWNUM, MT_OPERAND_ROWNUM1, MT_OPERAND_ROWNUM2
 , MT_OPERAND_RC1, MT_OPERAND_RC2, MT_OPERAND_CBRB, MT_OPERAND_CELL
 , MT_OPERAND_DUP, MT_OPERAND_CTXDISP, MT_OPERAND_FBDISP, MT_OPERAND_TYPE
 , MT_OPERAND_MASK, MT_OPERAND_BANKADDR, MT_OPERAND_INCAMT, MT_OPERAND_XMODE
 , MT_OPERAND_MASK1, MT_OPERAND_BALL, MT_OPERAND_BRC, MT_OPERAND_RDA
 , MT_OPERAND_WR, MT_OPERAND_BALL2, MT_OPERAND_BRC2, MT_OPERAND_PERM
 , MT_OPERAND_A23, MT_OPERAND_CR, MT_OPERAND_CBS, MT_OPERAND_INCR
 , MT_OPERAND_LENGTH, MT_OPERAND_CBX, MT_OPERAND_CCB, MT_OPERAND_CDB
 , MT_OPERAND_MODE, MT_OPERAND_ID, MT_OPERAND_SIZE, MT_OPERAND_FBINCR
 , MT_OPERAND_LOOPSIZE, MT_OPERAND_IMM16L, MT_OPERAND_RC3, MT_OPERAND_CB1SEL
 , MT_OPERAND_CB2SEL, MT_OPERAND_CB1INCR, MT_OPERAND_CB2INCR, MT_OPERAND_MAX
} CGEN_OPERAND_TYPE;

/* Number of operands types.  */
#define MAX_OPERANDS 55

/* Maximum number of operands referenced by any insn.  */
#define MAX_OPERAND_INSTANCES 8

/* Insn attribute indices.  */

/* Enum declaration for cgen_insn attrs.  */
typedef enum cgen_insn_attr {
  CGEN_INSN_ALIAS, CGEN_INSN_VIRTUAL, CGEN_INSN_UNCOND_CTI, CGEN_INSN_COND_CTI
 , CGEN_INSN_SKIP_CTI, CGEN_INSN_DELAY_SLOT, CGEN_INSN_RELAXABLE, CGEN_INSN_RELAXED
 , CGEN_INSN_NO_DIS, CGEN_INSN_PBB, CGEN_INSN_LOAD_DELAY, CGEN_INSN_MEMORY_ACCESS
 , CGEN_INSN_AL_INSN, CGEN_INSN_IO_INSN, CGEN_INSN_BR_INSN, CGEN_INSN_JAL_HAZARD
 , CGEN_INSN_USES_FRDR, CGEN_INSN_USES_FRDRRR, CGEN_INSN_USES_FRSR1, CGEN_INSN_USES_FRSR2
 , CGEN_INSN_SKIPA, CGEN_INSN_END_BOOLS, CGEN_INSN_START_NBOOLS = 31, CGEN_INSN_MACH
 , CGEN_INSN_END_NBOOLS
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
#define CGEN_ATTR_CGEN_INSN_LOAD_DELAY_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_LOAD_DELAY)) != 0)
#define CGEN_ATTR_CGEN_INSN_MEMORY_ACCESS_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_MEMORY_ACCESS)) != 0)
#define CGEN_ATTR_CGEN_INSN_AL_INSN_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_AL_INSN)) != 0)
#define CGEN_ATTR_CGEN_INSN_IO_INSN_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_IO_INSN)) != 0)
#define CGEN_ATTR_CGEN_INSN_BR_INSN_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_BR_INSN)) != 0)
#define CGEN_ATTR_CGEN_INSN_JAL_HAZARD_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_JAL_HAZARD)) != 0)
#define CGEN_ATTR_CGEN_INSN_USES_FRDR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_USES_FRDR)) != 0)
#define CGEN_ATTR_CGEN_INSN_USES_FRDRRR_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_USES_FRDRRR)) != 0)
#define CGEN_ATTR_CGEN_INSN_USES_FRSR1_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_USES_FRSR1)) != 0)
#define CGEN_ATTR_CGEN_INSN_USES_FRSR2_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_USES_FRSR2)) != 0)
#define CGEN_ATTR_CGEN_INSN_SKIPA_VALUE(attrs) (((attrs)->bool & (1 << CGEN_INSN_SKIPA)) != 0)

/* cgen.h uses things we just defined.  */
#include "opcode/cgen.h"

extern const struct cgen_ifld mt_cgen_ifld_table[];

/* Attributes.  */
extern const CGEN_ATTR_TABLE mt_cgen_hardware_attr_table[];
extern const CGEN_ATTR_TABLE mt_cgen_ifield_attr_table[];
extern const CGEN_ATTR_TABLE mt_cgen_operand_attr_table[];
extern const CGEN_ATTR_TABLE mt_cgen_insn_attr_table[];

/* Hardware decls.  */

extern CGEN_KEYWORD mt_cgen_opval_h_spr;

extern const CGEN_HW_ENTRY mt_cgen_hw_table[];



#endif /* MT_CPU_H */
