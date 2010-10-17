/* Instruction opcode table for openrisc.

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

#include "sysdep.h"
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "openrisc-desc.h"
#include "openrisc-opc.h"
#include "libiberty.h"

/* -- opc.c */
/* -- */
/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int asm_hash_insn PARAMS ((const char *));
static int dis_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int dis_hash_insn PARAMS ((const char *, CGEN_INSN_INT));

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & openrisc_cgen_ifld_table[OPENRISC_##f]
#else
#define F(f) & openrisc_cgen_ifld_table[OPENRISC_/**/f]
#endif
static const CGEN_IFMT ifmt_empty = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_l_j = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_ABS26) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_jr = {
  32, 32, 0xffe00000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_OP3) }, { F (F_OP4) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_bal = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_DISP26) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_movhi = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_mfsr = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_mtsr = {
  32, 32, 0xfc0007ff, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_R3) }, { F (F_I16_1) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_lw = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_sw = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R3) }, { F (F_I16NC) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_sll = {
  32, 32, 0xfc0007ff, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_R3) }, { F (F_F_10_3) }, { F (F_OP6) }, { F (F_F_4_1) }, { F (F_OP7) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_slli = {
  32, 32, 0xfc00ffe0, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_F_15_8) }, { F (F_OP6) }, { F (F_UIMM5) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_add = {
  32, 32, 0xfc0007ff, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_R3) }, { F (F_F_10_7) }, { F (F_OP7) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_addi = {
  32, 32, 0xfc000000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_R1) }, { F (F_R2) }, { F (F_LO16) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_sfgts = {
  32, 32, 0xffe007ff, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_OP5) }, { F (F_R2) }, { F (F_R3) }, { F (F_F_10_11) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_sfgtsi = {
  32, 32, 0xffe00000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_OP5) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_l_sfgtui = {
  32, 32, 0xffe00000, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_OP5) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) OPENRISC_OPERAND_##op
#else
#define OPERAND(op) OPENRISC_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE openrisc_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* l.j ${abs-26} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ABS_26), 0 } },
    & ifmt_l_j, { 0x0 }
  },
/* l.jal ${abs-26} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ABS_26), 0 } },
    & ifmt_l_j, { 0x4000000 }
  },
/* l.jr $rA */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), 0 } },
    & ifmt_l_jr, { 0x14000000 }
  },
/* l.jalr $rA */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), 0 } },
    & ifmt_l_jr, { 0x14200000 }
  },
/* l.bal ${disp-26} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP_26), 0 } },
    & ifmt_l_bal, { 0x8000000 }
  },
/* l.bnf ${disp-26} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP_26), 0 } },
    & ifmt_l_bal, { 0xc000000 }
  },
/* l.bf ${disp-26} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP_26), 0 } },
    & ifmt_l_bal, { 0x10000000 }
  },
/* l.brk ${uimm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM_16), 0 } },
    & ifmt_l_jr, { 0x17000000 }
  },
/* l.rfe $rA */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), 0 } },
    & ifmt_l_jr, { 0x14400000 }
  },
/* l.sys ${uimm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM_16), 0 } },
    & ifmt_l_jr, { 0x16000000 }
  },
/* l.nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_l_jr, { 0x15000000 }
  },
/* l.movhi $rD,$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (HI16), 0 } },
    & ifmt_l_movhi, { 0x18000000 }
  },
/* l.mfsr $rD,$rA */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), 0 } },
    & ifmt_l_mfsr, { 0x1c000000 }
  },
/* l.mtsr $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_mtsr, { 0x40000000 }
  },
/* l.lw $rD,${simm-16}($rA) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (SIMM_16), '(', OP (RA), ')', 0 } },
    & ifmt_l_lw, { 0x80000000 }
  },
/* l.lbz $rD,${simm-16}($rA) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (SIMM_16), '(', OP (RA), ')', 0 } },
    & ifmt_l_lw, { 0x84000000 }
  },
/* l.lbs $rD,${simm-16}($rA) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (SIMM_16), '(', OP (RA), ')', 0 } },
    & ifmt_l_lw, { 0x88000000 }
  },
/* l.lhz $rD,${simm-16}($rA) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (SIMM_16), '(', OP (RA), ')', 0 } },
    & ifmt_l_lw, { 0x8c000000 }
  },
/* l.lhs $rD,${simm-16}($rA) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (SIMM_16), '(', OP (RA), ')', 0 } },
    & ifmt_l_lw, { 0x90000000 }
  },
/* l.sw ${ui16nc}($rA),$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UI16NC), '(', OP (RA), ')', ',', OP (RB), 0 } },
    & ifmt_l_sw, { 0xd4000000 }
  },
/* l.sb ${ui16nc}($rA),$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UI16NC), '(', OP (RA), ')', ',', OP (RB), 0 } },
    & ifmt_l_sw, { 0xd8000000 }
  },
/* l.sh ${ui16nc}($rA),$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UI16NC), '(', OP (RA), ')', ',', OP (RB), 0 } },
    & ifmt_l_sw, { 0xdc000000 }
  },
/* l.sll $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sll, { 0xe0000008 }
  },
/* l.slli $rD,$rA,${uimm-5} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (UIMM_5), 0 } },
    & ifmt_l_slli, { 0xb4000000 }
  },
/* l.srl $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sll, { 0xe0000028 }
  },
/* l.srli $rD,$rA,${uimm-5} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (UIMM_5), 0 } },
    & ifmt_l_slli, { 0xb4000020 }
  },
/* l.sra $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sll, { 0xe0000048 }
  },
/* l.srai $rD,$rA,${uimm-5} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (UIMM_5), 0 } },
    & ifmt_l_slli, { 0xb4000040 }
  },
/* l.ror $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sll, { 0xe0000088 }
  },
/* l.rori $rD,$rA,${uimm-5} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (UIMM_5), 0 } },
    & ifmt_l_slli, { 0xb4000080 }
  },
/* l.add $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000000 }
  },
/* l.addi $rD,$rA,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (LO16), 0 } },
    & ifmt_l_addi, { 0x94000000 }
  },
/* l.sub $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000002 }
  },
/* l.subi $rD,$rA,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (LO16), 0 } },
    & ifmt_l_addi, { 0x9c000000 }
  },
/* l.and $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000003 }
  },
/* l.andi $rD,$rA,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (LO16), 0 } },
    & ifmt_l_addi, { 0xa0000000 }
  },
/* l.or $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000004 }
  },
/* l.ori $rD,$rA,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (LO16), 0 } },
    & ifmt_l_addi, { 0xa4000000 }
  },
/* l.xor $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000005 }
  },
/* l.xori $rD,$rA,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (LO16), 0 } },
    & ifmt_l_addi, { 0xa8000000 }
  },
/* l.mul $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000006 }
  },
/* l.muli $rD,$rA,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (LO16), 0 } },
    & ifmt_l_addi, { 0xac000000 }
  },
/* l.div $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe0000009 }
  },
/* l.divu $rD,$rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_add, { 0xe000000a }
  },
/* l.sfgts $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4c00000 }
  },
/* l.sfgtu $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4400000 }
  },
/* l.sfges $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4e00000 }
  },
/* l.sfgeu $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4600000 }
  },
/* l.sflts $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe5000000 }
  },
/* l.sfltu $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4800000 }
  },
/* l.sfles $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe5200000 }
  },
/* l.sfleu $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4a00000 }
  },
/* l.sfgtsi $rA,${simm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (SIMM_16), 0 } },
    & ifmt_l_sfgtsi, { 0xb8c00000 }
  },
/* l.sfgtui $rA,${uimm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (UIMM_16), 0 } },
    & ifmt_l_sfgtui, { 0xb8400000 }
  },
/* l.sfgesi $rA,${simm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (SIMM_16), 0 } },
    & ifmt_l_sfgtsi, { 0xb8e00000 }
  },
/* l.sfgeui $rA,${uimm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (UIMM_16), 0 } },
    & ifmt_l_sfgtui, { 0xb8600000 }
  },
/* l.sfltsi $rA,${simm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (SIMM_16), 0 } },
    & ifmt_l_sfgtsi, { 0xb9000000 }
  },
/* l.sfltui $rA,${uimm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (UIMM_16), 0 } },
    & ifmt_l_sfgtui, { 0xb8800000 }
  },
/* l.sflesi $rA,${simm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (SIMM_16), 0 } },
    & ifmt_l_sfgtsi, { 0xb9200000 }
  },
/* l.sfleui $rA,${uimm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (UIMM_16), 0 } },
    & ifmt_l_sfgtui, { 0xb8a00000 }
  },
/* l.sfeq $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4000000 }
  },
/* l.sfeqi $rA,${simm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (SIMM_16), 0 } },
    & ifmt_l_sfgtsi, { 0xb8000000 }
  },
/* l.sfne $rA,$rB */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (RB), 0 } },
    & ifmt_l_sfgts, { 0xe4200000 }
  },
/* l.sfnei $rA,${simm-16} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RA), ',', OP (SIMM_16), 0 } },
    & ifmt_l_sfgtsi, { 0xb8200000 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & openrisc_cgen_ifld_table[OPENRISC_##f]
#else
#define F(f) & openrisc_cgen_ifld_table[OPENRISC_/**/f]
#endif
static const CGEN_IFMT ifmt_l_ret = {
  32, 32, 0xffffffff, { { F (F_CLASS) }, { F (F_SUB) }, { F (F_OP3) }, { F (F_OP4) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) OPENRISC_OPERAND_##op
#else
#define OPERAND(op) OPENRISC_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE openrisc_cgen_macro_insn_table[] =
{
/* l.ret */
  {
    -1, "l-ret", "l.ret", 32,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE openrisc_cgen_macro_insn_opcode_table[] =
{
/* l.ret */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_l_ret, { 0x140b0000 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

#ifndef CGEN_ASM_HASH_P
#define CGEN_ASM_HASH_P(insn) 1
#endif

#ifndef CGEN_DIS_HASH_P
#define CGEN_DIS_HASH_P(insn) 1
#endif

/* Return non-zero if INSN is to be added to the hash table.
   Targets are free to override CGEN_{ASM,DIS}_HASH_P in the .opc file.  */

static int
asm_hash_insn_p (insn)
     const CGEN_INSN *insn ATTRIBUTE_UNUSED;
{
  return CGEN_ASM_HASH_P (insn);
}

static int
dis_hash_insn_p (insn)
     const CGEN_INSN *insn;
{
  /* If building the hash table and the NO-DIS attribute is present,
     ignore.  */
  if (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_NO_DIS))
    return 0;
  return CGEN_DIS_HASH_P (insn);
}

#ifndef CGEN_ASM_HASH
#define CGEN_ASM_HASH_SIZE 127
#ifdef CGEN_MNEMONIC_OPERANDS
#define CGEN_ASM_HASH(mnem) (*(unsigned char *) (mnem) % CGEN_ASM_HASH_SIZE)
#else
#define CGEN_ASM_HASH(mnem) (*(unsigned char *) (mnem) % CGEN_ASM_HASH_SIZE) /*FIXME*/
#endif
#endif

/* It doesn't make much sense to provide a default here,
   but while this is under development we do.
   BUFFER is a pointer to the bytes of the insn, target order.
   VALUE is the first base_insn_bitsize bits as an int in host order.  */

#ifndef CGEN_DIS_HASH
#define CGEN_DIS_HASH_SIZE 256
#define CGEN_DIS_HASH(buf, value) (*(unsigned char *) (buf))
#endif

/* The result is the hash value of the insn.
   Targets are free to override CGEN_{ASM,DIS}_HASH in the .opc file.  */

static unsigned int
asm_hash_insn (mnem)
     const char * mnem;
{
  return CGEN_ASM_HASH (mnem);
}

/* BUF is a pointer to the bytes of the insn, target order.
   VALUE is the first base_insn_bitsize bits as an int in host order.  */

static unsigned int
dis_hash_insn (buf, value)
     const char * buf ATTRIBUTE_UNUSED;
     CGEN_INSN_INT value ATTRIBUTE_UNUSED;
{
  return CGEN_DIS_HASH (buf, value);
}

static void set_fields_bitsize PARAMS ((CGEN_FIELDS *, int));

/* Set the recorded length of the insn in the CGEN_FIELDS struct.  */

static void
set_fields_bitsize (fields, size)
     CGEN_FIELDS *fields;
     int size;
{
  CGEN_FIELDS_BITSIZE (fields) = size;
}

/* Function to call before using the operand instance table.
   This plugs the opcode entries and macro instructions into the cpu table.  */

void
openrisc_cgen_init_opcode_table (cd)
     CGEN_CPU_DESC cd;
{
  int i;
  int num_macros = (sizeof (openrisc_cgen_macro_insn_table) /
		    sizeof (openrisc_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & openrisc_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & openrisc_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (num_macros * sizeof (CGEN_INSN));
  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      openrisc_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & openrisc_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      openrisc_cgen_build_insn_regex (& insns[i]);
    }

  cd->sizeof_fields = sizeof (CGEN_FIELDS);
  cd->set_fields_bitsize = set_fields_bitsize;

  cd->asm_hash_p = asm_hash_insn_p;
  cd->asm_hash = asm_hash_insn;
  cd->asm_hash_size = CGEN_ASM_HASH_SIZE;

  cd->dis_hash_p = dis_hash_insn_p;
  cd->dis_hash = dis_hash_insn;
  cd->dis_hash_size = CGEN_DIS_HASH_SIZE;
}
