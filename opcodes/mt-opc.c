/* Instruction opcode table for mt.

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

#include "sysdep.h"
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "mt-desc.h"
#include "mt-opc.h"
#include "libiberty.h"

/* -- opc.c */
#include "safe-ctype.h"

/* Special check to ensure that instruction exists for given machine.  */

int
mt_cgen_insn_supported (CGEN_CPU_DESC cd,
			const CGEN_INSN *insn)
{
  int machs = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_MACH);

  /* No mach attribute?  Assume it's supported for all machs.  */
  if (machs == 0)
    return 1;
  
  return ((machs & cd->machs) != 0);
}

/* A better hash function for instruction mnemonics.  */

unsigned int
mt_asm_hash (const char* insn)
{
  unsigned int hash;
  const char* m = insn;

  for (hash = 0; *m && ! ISSPACE (*m); m++)
    hash = (hash * 23) ^ (0x1F & TOLOWER (*m));

  /* printf ("%s %d\n", insn, (hash % CGEN_ASM_HASH_SIZE)); */

  return hash % CGEN_ASM_HASH_SIZE;
}


/* -- asm.c */
/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p        (const CGEN_INSN *);
static unsigned int asm_hash_insn (const char *);
static int dis_hash_insn_p        (const CGEN_INSN *);
static unsigned int dis_hash_insn (const char *, CGEN_INSN_INT);

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & mt_cgen_ifld_table[MT_##f]
#else
#define F(f) & mt_cgen_ifld_table[MT_/**/f]
#endif
static const CGEN_IFMT ifmt_empty ATTRIBUTE_UNUSED = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_add ATTRIBUTE_UNUSED = {
  32, 32, 0xff000fff, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_DRRR) }, { F (F_UU12) }, { 0 } }
};

static const CGEN_IFMT ifmt_addi ATTRIBUTE_UNUSED = {
  32, 32, 0xff000000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_DR) }, { F (F_IMM16S) }, { 0 } }
};

static const CGEN_IFMT ifmt_addui ATTRIBUTE_UNUSED = {
  32, 32, 0xff000000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_DR) }, { F (F_IMM16U) }, { 0 } }
};

static const CGEN_IFMT ifmt_nop ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_UU24) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldui ATTRIBUTE_UNUSED = {
  32, 32, 0xfff00000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_UU4B) }, { F (F_DR) }, { F (F_IMM16U) }, { 0 } }
};

static const CGEN_IFMT ifmt_brlt ATTRIBUTE_UNUSED = {
  32, 32, 0xff000000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_IMM16S) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmp ATTRIBUTE_UNUSED = {
  32, 32, 0xffff0000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_UU4B) }, { F (F_UU4A) }, { F (F_IMM16S) }, { 0 } }
};

static const CGEN_IFMT ifmt_jal ATTRIBUTE_UNUSED = {
  32, 32, 0xff0f0fff, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_UU4A) }, { F (F_DRRR) }, { F (F_UU12) }, { 0 } }
};

static const CGEN_IFMT ifmt_dbnz ATTRIBUTE_UNUSED = {
  32, 32, 0xff0f0000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_UU4A) }, { F (F_IMM16S) }, { 0 } }
};

static const CGEN_IFMT ifmt_ei ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_UU4B) }, { F (F_UU4A) }, { F (F_UU16) }, { 0 } }
};

static const CGEN_IFMT ifmt_si ATTRIBUTE_UNUSED = {
  32, 32, 0xffff0fff, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_UU4B) }, { F (F_UU4A) }, { F (F_DRRR) }, { F (F_UU12) }, { 0 } }
};

static const CGEN_IFMT ifmt_reti ATTRIBUTE_UNUSED = {
  32, 32, 0xff0fffff, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_UU4A) }, { F (F_UU16) }, { 0 } }
};

static const CGEN_IFMT ifmt_stw ATTRIBUTE_UNUSED = {
  32, 32, 0xff000000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_IMM16S) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldctxt ATTRIBUTE_UNUSED = {
  32, 32, 0xff000e00, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_UU_2_25) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_RC) }, { F (F_RCNUM) }, { F (F_UU_3_11) }, { F (F_CONTNUM) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldfb ATTRIBUTE_UNUSED = {
  32, 32, 0xff000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_UU_2_25) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_IMM16U) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbcb ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00f000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_BALL) }, { F (F_BRC) }, { F (F_UU_4_15) }, { F (F_RC) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mfbcb ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00f000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_UU_4_15) }, { F (F_RC1) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbcci ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_BALL) }, { F (F_BRC) }, { F (F_FBDISP) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mfbcci ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_FBDISP) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbcbdr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_BALL2) }, { F (F_BRC2) }, { F (F_RC1) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_rcfbcb ATTRIBUTE_UNUSED = {
  32, 32, 0xfcc08000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_UU_2_23) }, { F (F_TYPE) }, { F (F_BALL) }, { F (F_BRC) }, { F (F_UU_1_15) }, { F (F_ROWNUM) }, { F (F_RC1) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrcfbcb ATTRIBUTE_UNUSED = {
  32, 32, 0xfcc08000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_UU_2_23) }, { F (F_TYPE) }, { F (F_SR2) }, { F (F_UU_1_15) }, { F (F_ROWNUM) }, { F (F_RC1) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_cbcast ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000380, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_MASK) }, { F (F_UU_3_9) }, { F (F_RC2) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_dupcbcast ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_MASK) }, { F (F_CELL) }, { F (F_RC2) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_wfbi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_BANKADDR) }, { F (F_ROWNUM1) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_wfb ATTRIBUTE_UNUSED = {
  32, 32, 0xff000040, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_UU_2_25) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_FBDISP) }, { F (F_ROWNUM2) }, { F (F_UU_1_6) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_rcrisc ATTRIBUTE_UNUSED = {
  32, 32, 0xfc080000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_UU_1_19) }, { F (F_COLNUM) }, { F (F_DRRR) }, { F (F_RC1) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbcbinc ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RBBC) }, { F (F_SR1) }, { F (F_INCAMT) }, { F (F_RC1) }, { F (F_CBRB) }, { F (F_CELL) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_rcxmode ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RDA) }, { F (F_WR) }, { F (F_XMODE) }, { F (F_MASK1) }, { F (F_SR2) }, { F (F_FBDISP) }, { F (F_ROWNUM2) }, { F (F_RC2) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_interleaver ATTRIBUTE_UNUSED = {
  32, 32, 0xfc008000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_MODE) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_UU_1_15) }, { F (F_ID) }, { F (F_SIZE) }, { 0 } }
};

static const CGEN_IFMT ifmt_wfbinc ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RDA) }, { F (F_WR) }, { F (F_FBINCR) }, { F (F_BALL) }, { F (F_COLNUM) }, { F (F_LENGTH) }, { F (F_ROWNUM1) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mwfbinc ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RDA) }, { F (F_WR) }, { F (F_FBINCR) }, { F (F_SR2) }, { F (F_LENGTH) }, { F (F_ROWNUM1) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_wfbincr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RDA) }, { F (F_WR) }, { F (F_SR1) }, { F (F_BALL) }, { F (F_COLNUM) }, { F (F_LENGTH) }, { F (F_ROWNUM1) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mwfbincr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_RDA) }, { F (F_WR) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_LENGTH) }, { F (F_ROWNUM1) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbcbincs ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_PERM) }, { F (F_A23) }, { F (F_CR) }, { F (F_CBS) }, { F (F_INCR) }, { F (F_CCB) }, { F (F_CDB) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mfbcbincs ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_PERM) }, { F (F_SR1) }, { F (F_CBS) }, { F (F_INCR) }, { F (F_CCB) }, { F (F_CDB) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbcbincrs ATTRIBUTE_UNUSED = {
  32, 32, 0xfc008000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_PERM) }, { F (F_SR1) }, { F (F_BALL) }, { F (F_COLNUM) }, { F (F_UU_1_15) }, { F (F_CBX) }, { F (F_CCB) }, { F (F_CDB) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_mfbcbincrs ATTRIBUTE_UNUSED = {
  32, 32, 0xfc008000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_PERM) }, { F (F_SR1) }, { F (F_SR2) }, { F (F_UU_1_15) }, { F (F_CBX) }, { F (F_CCB) }, { F (F_CDB) }, { F (F_ROWNUM2) }, { F (F_DUP) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_loop ATTRIBUTE_UNUSED = {
  32, 32, 0xff0fff00, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_SR1) }, { F (F_UU4A) }, { F (F_UU8) }, { F (F_LOOPO) }, { 0 } }
};

static const CGEN_IFMT ifmt_loopi ATTRIBUTE_UNUSED = {
  32, 32, 0xff000000, { { F (F_MSYS) }, { F (F_OPC) }, { F (F_IMM) }, { F (F_IMM16L) }, { F (F_LOOPO) }, { 0 } }
};

static const CGEN_IFMT ifmt_dfbc ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_CB1SEL) }, { F (F_CB2SEL) }, { F (F_CB1INCR) }, { F (F_CB2INCR) }, { F (F_RC3) }, { F (F_RC2) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_dwfb ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000080, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_CB1SEL) }, { F (F_CB2SEL) }, { F (F_CB1INCR) }, { F (F_CB2INCR) }, { F (F_UU1) }, { F (F_RC2) }, { F (F_CTXDISP) }, { 0 } }
};

static const CGEN_IFMT ifmt_dfbr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_MSYS) }, { F (F_MSOPC) }, { F (F_CB1SEL) }, { F (F_CB2SEL) }, { F (F_SR2) }, { F (F_LENGTH) }, { F (F_ROWNUM1) }, { F (F_ROWNUM2) }, { F (F_RC2) }, { F (F_CTXDISP) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) MT_OPERAND_##op
#else
#define OPERAND(op) MT_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE mt_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* add $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x0 }
  },
/* addu $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x2000000 }
  },
/* addi $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x1000000 }
  },
/* addui $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x3000000 }
  },
/* sub $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x4000000 }
  },
/* subu $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x6000000 }
  },
/* subi $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x5000000 }
  },
/* subui $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x7000000 }
  },
/* mul $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x8000000 }
  },
/* muli $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x9000000 }
  },
/* and $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x10000000 }
  },
/* andi $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x11000000 }
  },
/* or $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x12000000 }
  },
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x12000000 }
  },
/* ori $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x13000000 }
  },
/* xor $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x14000000 }
  },
/* xori $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x15000000 }
  },
/* nand $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x16000000 }
  },
/* nandi $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x17000000 }
  },
/* nor $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x18000000 }
  },
/* nori $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x19000000 }
  },
/* xnor $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x1a000000 }
  },
/* xnori $frdr,$frsr1,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_addui, { 0x1b000000 }
  },
/* ldui $frdr,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_ldui, { 0x1d000000 }
  },
/* lsl $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x20000000 }
  },
/* lsli $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x21000000 }
  },
/* lsr $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x22000000 }
  },
/* lsri $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x23000000 }
  },
/* asr $frdrrr,$frsr1,$frsr2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), ',', OP (FRSR2), 0 } },
    & ifmt_add, { 0x24000000 }
  },
/* asri $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x25000000 }
  },
/* brlt $frsr1,$frsr2,$imm16o */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', OP (IMM16O), 0 } },
    & ifmt_brlt, { 0x31000000 }
  },
/* brle $frsr1,$frsr2,$imm16o */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', OP (IMM16O), 0 } },
    & ifmt_brlt, { 0x33000000 }
  },
/* breq $frsr1,$frsr2,$imm16o */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', OP (IMM16O), 0 } },
    & ifmt_brlt, { 0x35000000 }
  },
/* brne $frsr1,$frsr2,$imm16o */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', OP (IMM16O), 0 } },
    & ifmt_brlt, { 0x3b000000 }
  },
/* jmp $imm16o */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (IMM16O), 0 } },
    & ifmt_jmp, { 0x37000000 }
  },
/* jal $frdrrr,$frsr1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', OP (FRSR1), 0 } },
    & ifmt_jal, { 0x38000000 }
  },
/* dbnz $frsr1,$imm16o */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (IMM16O), 0 } },
    & ifmt_dbnz, { 0x3d000000 }
  },
/* ei */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ei, { 0x60000000 }
  },
/* di */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ei, { 0x62000000 }
  },
/* si $frdrrr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), 0 } },
    & ifmt_si, { 0x64000000 }
  },
/* reti $frsr1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), 0 } },
    & ifmt_reti, { 0x66000000 }
  },
/* ldw $frdr,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDR), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_addi, { 0x41000000 }
  },
/* stw $frsr2,$frsr1,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR2), ',', OP (FRSR1), ',', '#', OP (IMM16), 0 } },
    & ifmt_stw, { 0x43000000 }
  },
/* break */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x68000000 }
  },
/* iflush */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x6a000000 }
  },
/* ldctxt $frsr1,$frsr2,#$rc,#$rcnum,#$contnum */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', '#', OP (RC), ',', '#', OP (RCNUM), ',', '#', OP (CONTNUM), 0 } },
    & ifmt_ldctxt, { 0x80000000 }
  },
/* ldfb $frsr1,$frsr2,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_ldfb, { 0x84000000 }
  },
/* stfb $frsr1,$frsr2,#$imm16z */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', '#', OP (IMM16Z), 0 } },
    & ifmt_ldfb, { 0x88000000 }
  },
/* fbcb $frsr1,#$rbbc,#$ball,#$brc,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', '#', OP (BALL), ',', '#', OP (BRC), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcb, { 0x8c000000 }
  },
/* mfbcb $frsr1,#$rbbc,$frsr2,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', OP (FRSR2), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcb, { 0x90000000 }
  },
/* fbcci $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', '#', OP (BALL), ',', '#', OP (BRC), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcci, { 0x94000000 }
  },
/* fbrci $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', '#', OP (BALL), ',', '#', OP (BRC), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcci, { 0x98000000 }
  },
/* fbcri $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', '#', OP (BALL), ',', '#', OP (BRC), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcci, { 0x9c000000 }
  },
/* fbrri $frsr1,#$rbbc,#$ball,#$brc,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', '#', OP (BALL), ',', '#', OP (BRC), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcci, { 0xa0000000 }
  },
/* mfbcci $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', OP (FRSR2), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcci, { 0xa4000000 }
  },
/* mfbrci $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', OP (FRSR2), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcci, { 0xa8000000 }
  },
/* mfbcri $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', OP (FRSR2), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcci, { 0xac000000 }
  },
/* mfbrri $frsr1,#$rbbc,$frsr2,#$fbdisp,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', OP (FRSR2), ',', '#', OP (FBDISP), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcci, { 0xb0000000 }
  },
/* fbcbdr $frsr1,#$rbbc,$frsr2,#$ball2,#$brc2,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', OP (FRSR2), ',', '#', OP (BALL2), ',', '#', OP (BRC2), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcbdr, { 0xb4000000 }
  },
/* rcfbcb #$rbbc,#$type,#$ball,#$brc,#$rownum,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (RBBC), ',', '#', OP (TYPE), ',', '#', OP (BALL), ',', '#', OP (BRC), ',', '#', OP (ROWNUM), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_rcfbcb, { 0xb8000000 }
  },
/* mrcfbcb $frsr2,#$rbbc,#$type,#$rownum,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR2), ',', '#', OP (RBBC), ',', '#', OP (TYPE), ',', '#', OP (ROWNUM), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mrcfbcb, { 0xbc000000 }
  },
/* cbcast #$mask,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (MASK), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_cbcast, { 0xc0000000 }
  },
/* dupcbcast #$mask,#$cell,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (MASK), ',', '#', OP (CELL), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_dupcbcast, { 0xc4000000 }
  },
/* wfbi #$bankaddr,#$rownum1,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (BANKADDR), ',', '#', OP (ROWNUM1), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_wfbi, { 0xc8000000 }
  },
/* wfb $frsr1,$frsr2,#$fbdisp,#$rownum2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', '#', OP (FBDISP), ',', '#', OP (ROWNUM2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_wfb, { 0xcc000000 }
  },
/* rcrisc $frdrrr,#$rbbc,$frsr1,#$colnum,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRDRRR), ',', '#', OP (RBBC), ',', OP (FRSR1), ',', '#', OP (COLNUM), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_rcrisc, { 0xd0000000 }
  },
/* fbcbinc $frsr1,#$rbbc,#$incamt,#$rc1,#$cbrb,#$cell,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RBBC), ',', '#', OP (INCAMT), ',', '#', OP (RC1), ',', '#', OP (CBRB), ',', '#', OP (CELL), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcbinc, { 0xd4000000 }
  },
/* rcxmode $frsr2,#$rda,#$wr,#$xmode,#$mask1,#$fbdisp,#$rownum2,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR2), ',', '#', OP (RDA), ',', '#', OP (WR), ',', '#', OP (XMODE), ',', '#', OP (MASK1), ',', '#', OP (FBDISP), ',', '#', OP (ROWNUM2), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_rcxmode, { 0xd8000000 }
  },
/* intlvr $frsr1,#$mode,$frsr2,#$id,#$size */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (MODE), ',', OP (FRSR2), ',', '#', OP (ID), ',', '#', OP (SIZE), 0 } },
    & ifmt_interleaver, { 0xdc000000 }
  },
/* wfbinc #$rda,#$wr,#$fbincr,#$ball,#$colnum,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (RDA), ',', '#', OP (WR), ',', '#', OP (FBINCR), ',', '#', OP (BALL), ',', '#', OP (COLNUM), ',', '#', OP (LENGTH), ',', '#', OP (ROWNUM1), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_wfbinc, { 0xe0000000 }
  },
/* mwfbinc $frsr2,#$rda,#$wr,#$fbincr,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR2), ',', '#', OP (RDA), ',', '#', OP (WR), ',', '#', OP (FBINCR), ',', '#', OP (LENGTH), ',', '#', OP (ROWNUM1), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mwfbinc, { 0xe4000000 }
  },
/* wfbincr $frsr1,#$rda,#$wr,#$ball,#$colnum,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (RDA), ',', '#', OP (WR), ',', '#', OP (BALL), ',', '#', OP (COLNUM), ',', '#', OP (LENGTH), ',', '#', OP (ROWNUM1), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_wfbincr, { 0xe8000000 }
  },
/* mwfbincr $frsr1,$frsr2,#$rda,#$wr,#$length,#$rownum1,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', '#', OP (RDA), ',', '#', OP (WR), ',', '#', OP (LENGTH), ',', '#', OP (ROWNUM1), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mwfbincr, { 0xec000000 }
  },
/* fbcbincs #$perm,#$a23,#$cr,#$cbs,#$incr,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (PERM), ',', '#', OP (A23), ',', '#', OP (CR), ',', '#', OP (CBS), ',', '#', OP (INCR), ',', '#', OP (CCB), ',', '#', OP (CDB), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcbincs, { 0xf0000000 }
  },
/* mfbcbincs $frsr1,#$perm,#$cbs,#$incr,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (PERM), ',', '#', OP (CBS), ',', '#', OP (INCR), ',', '#', OP (CCB), ',', '#', OP (CDB), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcbincs, { 0xf4000000 }
  },
/* fbcbincrs $frsr1,#$perm,#$ball,#$colnum,#$cbx,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', '#', OP (PERM), ',', '#', OP (BALL), ',', '#', OP (COLNUM), ',', '#', OP (CBX), ',', '#', OP (CCB), ',', '#', OP (CDB), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_fbcbincrs, { 0xf8000000 }
  },
/* mfbcbincrs $frsr1,$frsr2,#$perm,#$cbx,#$ccb,#$cdb,#$rownum2,#$dup,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (FRSR2), ',', '#', OP (PERM), ',', '#', OP (CBX), ',', '#', OP (CCB), ',', '#', OP (CDB), ',', '#', OP (ROWNUM2), ',', '#', OP (DUP), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_mfbcbincrs, { 0xfc000000 }
  },
/* loop $frsr1,$loopsize */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FRSR1), ',', OP (LOOPSIZE), 0 } },
    & ifmt_loop, { 0x3e000000 }
  },
/* loopi #$imm16l,$loopsize */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (IMM16L), ',', OP (LOOPSIZE), 0 } },
    & ifmt_loopi, { 0x3f000000 }
  },
/* dfbc #$cb1sel,#$cb2sel,#$cb1incr,#$cb2incr,#$rc3,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (CB1SEL), ',', '#', OP (CB2SEL), ',', '#', OP (CB1INCR), ',', '#', OP (CB2INCR), ',', '#', OP (RC3), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_dfbc, { 0x80000000 }
  },
/* dwfb #$cb1sel,#$cb2sel,#$cb1incr,#$cb2incr,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (CB1SEL), ',', '#', OP (CB2SEL), ',', '#', OP (CB1INCR), ',', '#', OP (CB2INCR), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_dwfb, { 0x84000000 }
  },
/* fbwfb #$cb1sel,#$cb2sel,#$cb1incr,#$cb2incr,#$rc3,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (CB1SEL), ',', '#', OP (CB2SEL), ',', '#', OP (CB1INCR), ',', '#', OP (CB2INCR), ',', '#', OP (RC3), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_dfbc, { 0x88000000 }
  },
/* dfbr #$cb1sel,#$cb2sel,$frsr2,#$length,#$rownum1,#$rownum2,#$rc2,#$ctxdisp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (CB1SEL), ',', '#', OP (CB2SEL), ',', OP (FRSR2), ',', '#', OP (LENGTH), ',', '#', OP (ROWNUM1), ',', '#', OP (ROWNUM2), ',', '#', OP (RC2), ',', '#', OP (CTXDISP), 0 } },
    & ifmt_dfbr, { 0x8c000000 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & mt_cgen_ifld_table[MT_##f]
#else
#define F(f) & mt_cgen_ifld_table[MT_/**/f]
#endif
#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) MT_OPERAND_##op
#else
#define OPERAND(op) MT_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE mt_cgen_macro_insn_table[] =
{
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE mt_cgen_macro_insn_opcode_table[] =
{
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

/* Set the recorded length of the insn in the CGEN_FIELDS struct.  */

static void
set_fields_bitsize (CGEN_FIELDS *fields, int size)
{
  CGEN_FIELDS_BITSIZE (fields) = size;
}

/* Function to call before using the operand instance table.
   This plugs the opcode entries and macro instructions into the cpu table.  */

void
mt_cgen_init_opcode_table (CGEN_CPU_DESC cd)
{
  int i;
  int num_macros = (sizeof (mt_cgen_macro_insn_table) /
		    sizeof (mt_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & mt_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & mt_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = xmalloc (num_macros * sizeof (CGEN_INSN));

  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      mt_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & mt_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      mt_cgen_build_insn_regex (& insns[i]);
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
