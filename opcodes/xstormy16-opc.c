/* Instruction opcode table for xstormy16.

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
#include "xstormy16-desc.h"
#include "xstormy16-opc.h"
#include "libiberty.h"

/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int asm_hash_insn PARAMS ((const char *));
static int dis_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int dis_hash_insn PARAMS ((const char *, CGEN_INSN_INT));

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & xstormy16_cgen_ifld_table[XSTORMY16_##f]
#else
#define F(f) & xstormy16_cgen_ifld_table[XSTORMY16_/**/f]
#endif
static const CGEN_IFMT ifmt_empty = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_movlmemimm = {
  32, 32, 0xfe000000, { { F (F_OP1) }, { F (F_OP2A) }, { F (F_OP2M) }, { F (F_LMEM8) }, { F (F_IMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_movhmemimm = {
  32, 32, 0xfe000000, { { F (F_OP1) }, { F (F_OP2A) }, { F (F_OP2M) }, { F (F_HMEM8) }, { F (F_IMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_movlgrmem = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_RM) }, { F (F_OP2M) }, { F (F_LMEM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_movhgrmem = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_RM) }, { F (F_OP2M) }, { F (F_HMEM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgrgri = {
  16, 16, 0xfe08, { { F (F_OP1) }, { F (F_OP2A) }, { F (F_OP2M) }, { F (F_RS) }, { F (F_OP4M) }, { F (F_RDM) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgrgrii = {
  32, 32, 0xfe08f000, { { F (F_OP1) }, { F (F_OP2A) }, { F (F_OP2M) }, { F (F_RS) }, { F (F_OP4M) }, { F (F_RDM) }, { F (F_OP5) }, { F (F_IMM12) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgrgr = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_RS) }, { F (F_RD) }, { 0 } }
};

static const CGEN_IFMT ifmt_movwimm8 = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_movwgrimm8 = {
  16, 16, 0xf100, { { F (F_OP1) }, { F (F_RM) }, { F (F_OP2M) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_movwgrimm16 = {
  32, 32, 0xfff00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RD) }, { F (F_IMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_movlowgr = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RD) }, { 0 } }
};

static const CGEN_IFMT ifmt_movfgrgrii = {
  32, 32, 0xfe088000, { { F (F_OP1) }, { F (F_OP2A) }, { F (F_OP2M) }, { F (F_RS) }, { F (F_OP4M) }, { F (F_RDM) }, { F (F_OP5A) }, { F (F_RB) }, { F (F_IMM12) }, { 0 } }
};

static const CGEN_IFMT ifmt_addgrimm4 = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_IMM4) }, { F (F_RD) }, { 0 } }
};

static const CGEN_IFMT ifmt_incgrimm2 = {
  16, 16, 0xffc0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3A) }, { F (F_IMM2) }, { F (F_RD) }, { 0 } }
};

static const CGEN_IFMT ifmt_set1lmemimm = {
  16, 16, 0xf100, { { F (F_OP1) }, { F (F_IMM3) }, { F (F_OP2M) }, { F (F_LMEM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_set1hmemimm = {
  16, 16, 0xf100, { { F (F_OP1) }, { F (F_IMM3) }, { F (F_OP2M) }, { F (F_HMEM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bccgrgr = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_RS) }, { F (F_RD) }, { F (F_OP5) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_bccgrimm8 = {
  32, 32, 0xf1000000, { { F (F_OP1) }, { F (F_RM) }, { F (F_OP2M) }, { F (F_IMM8) }, { F (F_OP5) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_bccimm16 = {
  32, 32, 0xf0000000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_REL8_4) }, { F (F_IMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_bngrimm4 = {
  32, 32, 0xff00f000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_IMM4) }, { F (F_RD) }, { F (F_OP5) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_bngrgr = {
  32, 32, 0xff00f000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_RS) }, { F (F_RD) }, { F (F_OP5) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_bnlmemimm = {
  32, 32, 0xff008000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_LMEM8) }, { F (F_OP5A) }, { F (F_IMM3B) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_bnhmemimm = {
  32, 32, 0xff008000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_HMEM8) }, { F (F_OP5A) }, { F (F_IMM3B) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_bcc = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_REL8_2) }, { 0 } }
};

static const CGEN_IFMT ifmt_br = {
  16, 16, 0xf001, { { F (F_OP1) }, { F (F_REL12A) }, { F (F_OP4B) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmp = {
  16, 16, 0xffe0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3B) }, { F (F_RBJ) }, { F (F_RD) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpf = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_ABS24) }, { 0 } }
};

static const CGEN_IFMT ifmt_iret = {
  16, 16, 0xffff, { { F (F_OP) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) XSTORMY16_OPERAND_##op
#else
#define OPERAND(op) XSTORMY16_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE xstormy16_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* mov$ws2 $lmem8,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (LMEM8), ',', '#', OP (IMM16), 0 } },
    & ifmt_movlmemimm, { 0x78000000 }
  },
/* mov$ws2 $hmem8,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (HMEM8), ',', '#', OP (IMM16), 0 } },
    & ifmt_movhmemimm, { 0x7a000000 }
  },
/* mov$ws2 $Rm,$lmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RM), ',', OP (LMEM8), 0 } },
    & ifmt_movlgrmem, { 0x8000 }
  },
/* mov$ws2 $Rm,$hmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RM), ',', OP (HMEM8), 0 } },
    & ifmt_movhgrmem, { 0xa000 }
  },
/* mov$ws2 $lmem8,$Rm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (LMEM8), ',', OP (RM), 0 } },
    & ifmt_movlgrmem, { 0x9000 }
  },
/* mov$ws2 $hmem8,$Rm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (HMEM8), ',', OP (RM), 0 } },
    & ifmt_movhgrmem, { 0xb000 }
  },
/* mov$ws2 $Rdm,($Rs) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RS), ')', 0 } },
    & ifmt_movgrgri, { 0x7000 }
  },
/* mov$ws2 $Rdm,($Rs++) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RS), '+', '+', ')', 0 } },
    & ifmt_movgrgri, { 0x6000 }
  },
/* mov$ws2 $Rdm,(--$Rs) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', '-', '-', OP (RS), ')', 0 } },
    & ifmt_movgrgri, { 0x6800 }
  },
/* mov$ws2 ($Rs),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RS), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgri, { 0x7200 }
  },
/* mov$ws2 ($Rs++),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RS), '+', '+', ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgri, { 0x6200 }
  },
/* mov$ws2 (--$Rs),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', '-', '-', OP (RS), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgri, { 0x6a00 }
  },
/* mov$ws2 $Rdm,($Rs,$imm12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RS), ',', OP (IMM12), ')', 0 } },
    & ifmt_movgrgrii, { 0x70080000 }
  },
/* mov$ws2 $Rdm,($Rs++,$imm12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RS), '+', '+', ',', OP (IMM12), ')', 0 } },
    & ifmt_movgrgrii, { 0x60080000 }
  },
/* mov$ws2 $Rdm,(--$Rs,$imm12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', '-', '-', OP (RS), ',', OP (IMM12), ')', 0 } },
    & ifmt_movgrgrii, { 0x68080000 }
  },
/* mov$ws2 ($Rs,$imm12),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RS), ',', OP (IMM12), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgrii, { 0x72080000 }
  },
/* mov$ws2 ($Rs++,$imm12),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RS), '+', '+', ',', OP (IMM12), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgrii, { 0x62080000 }
  },
/* mov$ws2 (--$Rs,$imm12),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', '-', '-', OP (RS), ',', OP (IMM12), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgrii, { 0x6a080000 }
  },
/* mov $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4600 }
  },
/* mov.w Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x4700 }
  },
/* mov.w $Rm,#$imm8small */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RM), ',', '#', OP (IMM8SMALL), 0 } },
    & ifmt_movwgrimm8, { 0x2100 }
  },
/* mov.w $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31300000 }
  },
/* mov.b $Rd,RxL */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', 'R', 'x', 'L', 0 } },
    & ifmt_movlowgr, { 0x30c0 }
  },
/* mov.b $Rd,RxH */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', 'R', 'x', 'H', 0 } },
    & ifmt_movlowgr, { 0x30d0 }
  },
/* movf$ws2 $Rdm,($Rs) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RS), ')', 0 } },
    & ifmt_movgrgri, { 0x7400 }
  },
/* movf$ws2 $Rdm,($Rs++) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RS), '+', '+', ')', 0 } },
    & ifmt_movgrgri, { 0x6400 }
  },
/* movf$ws2 $Rdm,(--$Rs) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', '-', '-', OP (RS), ')', 0 } },
    & ifmt_movgrgri, { 0x6c00 }
  },
/* movf$ws2 ($Rs),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RS), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgri, { 0x7600 }
  },
/* movf$ws2 ($Rs++),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RS), '+', '+', ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgri, { 0x6600 }
  },
/* movf$ws2 (--$Rs),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', '-', '-', OP (RS), ')', ',', OP (RDM), 0 } },
    & ifmt_movgrgri, { 0x6e00 }
  },
/* movf$ws2 $Rdm,($Rb,$Rs,$imm12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RB), ',', OP (RS), ',', OP (IMM12), ')', 0 } },
    & ifmt_movfgrgrii, { 0x74080000 }
  },
/* movf$ws2 $Rdm,($Rb,$Rs++,$imm12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RB), ',', OP (RS), '+', '+', ',', OP (IMM12), ')', 0 } },
    & ifmt_movfgrgrii, { 0x64080000 }
  },
/* movf$ws2 $Rdm,($Rb,--$Rs,$imm12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', OP (RDM), ',', '(', OP (RB), ',', '-', '-', OP (RS), ',', OP (IMM12), ')', 0 } },
    & ifmt_movfgrgrii, { 0x6c080000 }
  },
/* movf$ws2 ($Rb,$Rs,$imm12),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RB), ',', OP (RS), ',', OP (IMM12), ')', ',', OP (RDM), 0 } },
    & ifmt_movfgrgrii, { 0x76080000 }
  },
/* movf$ws2 ($Rb,$Rs++,$imm12),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RB), ',', OP (RS), '+', '+', ',', OP (IMM12), ')', ',', OP (RDM), 0 } },
    & ifmt_movfgrgrii, { 0x66080000 }
  },
/* movf$ws2 ($Rb,--$Rs,$imm12),$Rdm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (WS2), ' ', '(', OP (RB), ',', '-', '-', OP (RS), ',', OP (IMM12), ')', ',', OP (RDM), 0 } },
    & ifmt_movfgrgrii, { 0x6e080000 }
  },
/* mask $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3300 }
  },
/* mask $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x30e00000 }
  },
/* push $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x80 }
  },
/* pop $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x90 }
  },
/* swpn $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x3090 }
  },
/* swpb $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x3080 }
  },
/* swpw $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3200 }
  },
/* and $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4000 }
  },
/* and Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x4100 }
  },
/* and $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31000000 }
  },
/* or $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4200 }
  },
/* or Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x4300 }
  },
/* or $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31100000 }
  },
/* xor $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4400 }
  },
/* xor Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x4500 }
  },
/* xor $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31200000 }
  },
/* not $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x30b0 }
  },
/* add $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4900 }
  },
/* add $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x5100 }
  },
/* add Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x5900 }
  },
/* add $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31400000 }
  },
/* adc $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4b00 }
  },
/* adc $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x5300 }
  },
/* adc Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x5b00 }
  },
/* adc $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31500000 }
  },
/* sub $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4d00 }
  },
/* sub $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x5500 }
  },
/* sub Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x5d00 }
  },
/* sub $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31600000 }
  },
/* sbc $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x4f00 }
  },
/* sbc $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x5700 }
  },
/* sbc Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movwimm8, { 0x5f00 }
  },
/* sbc $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movwgrimm16, { 0x31700000 }
  },
/* inc $Rd,#$imm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM2), 0 } },
    & ifmt_incgrimm2, { 0x3000 }
  },
/* dec $Rd,#$imm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM2), 0 } },
    & ifmt_incgrimm2, { 0x3040 }
  },
/* rrc $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3800 }
  },
/* rrc $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x3900 }
  },
/* rlc $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3a00 }
  },
/* rlc $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x3b00 }
  },
/* shr $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3c00 }
  },
/* shr $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x3d00 }
  },
/* shl $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3e00 }
  },
/* shl $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x3f00 }
  },
/* asr $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0x3600 }
  },
/* asr $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x3700 }
  },
/* set1 $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x900 }
  },
/* set1 $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0xb00 }
  },
/* set1 $lmem8,#$imm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LMEM8), ',', '#', OP (IMM3), 0 } },
    & ifmt_set1lmemimm, { 0xe100 }
  },
/* set1 $hmem8,#$imm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HMEM8), ',', '#', OP (IMM3), 0 } },
    & ifmt_set1hmemimm, { 0xf100 }
  },
/* clr1 $Rd,#$imm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), 0 } },
    & ifmt_addgrimm4, { 0x800 }
  },
/* clr1 $Rd,$Rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_movgrgr, { 0xa00 }
  },
/* clr1 $lmem8,#$imm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LMEM8), ',', '#', OP (IMM3), 0 } },
    & ifmt_set1lmemimm, { 0xe000 }
  },
/* clr1 $hmem8,#$imm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HMEM8), ',', '#', OP (IMM3), 0 } },
    & ifmt_set1hmemimm, { 0xf000 }
  },
/* cbw $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x30a0 }
  },
/* rev $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x30f0 }
  },
/* b$bcond5 $Rd,$Rs,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (BCOND5), ' ', OP (RD), ',', OP (RS), ',', OP (REL12), 0 } },
    & ifmt_bccgrgr, { 0xd000000 }
  },
/* b$bcond5 $Rm,#$imm8,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (BCOND5), ' ', OP (RM), ',', '#', OP (IMM8), ',', OP (REL12), 0 } },
    & ifmt_bccgrimm8, { 0x20000000 }
  },
/* b$bcond2 Rx,#$imm16,${rel8-4} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (BCOND2), ' ', 'R', 'x', ',', '#', OP (IMM16), ',', OP (REL8_4), 0 } },
    & ifmt_bccimm16, { 0xc0000000 }
  },
/* bn $Rd,#$imm4,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), ',', OP (REL12), 0 } },
    & ifmt_bngrimm4, { 0x4000000 }
  },
/* bn $Rd,$Rs,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (REL12), 0 } },
    & ifmt_bngrgr, { 0x6000000 }
  },
/* bn $lmem8,#$imm3b,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LMEM8), ',', '#', OP (IMM3B), ',', OP (REL12), 0 } },
    & ifmt_bnlmemimm, { 0x7c000000 }
  },
/* bn $hmem8,#$imm3b,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HMEM8), ',', '#', OP (IMM3B), ',', OP (REL12), 0 } },
    & ifmt_bnhmemimm, { 0x7e000000 }
  },
/* bp $Rd,#$imm4,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM4), ',', OP (REL12), 0 } },
    & ifmt_bngrimm4, { 0x5000000 }
  },
/* bp $Rd,$Rs,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (REL12), 0 } },
    & ifmt_bngrgr, { 0x7000000 }
  },
/* bp $lmem8,#$imm3b,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LMEM8), ',', '#', OP (IMM3B), ',', OP (REL12), 0 } },
    & ifmt_bnlmemimm, { 0x7d000000 }
  },
/* bp $hmem8,#$imm3b,$rel12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HMEM8), ',', '#', OP (IMM3B), ',', OP (REL12), 0 } },
    & ifmt_bnhmemimm, { 0x7f000000 }
  },
/* b$bcond2 ${rel8-2} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (BCOND2), ' ', OP (REL8_2), 0 } },
    & ifmt_bcc, { 0xd000 }
  },
/* br $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x20 }
  },
/* br $rel12a */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REL12A), 0 } },
    & ifmt_br, { 0x1000 }
  },
/* jmp $Rbj,$Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RBJ), ',', OP (RD), 0 } },
    & ifmt_jmp, { 0x40 }
  },
/* jmpf $abs24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ABS24), 0 } },
    & ifmt_jmpf, { 0x2000000 }
  },
/* callr $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x10 }
  },
/* callr $rel12a */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REL12A), 0 } },
    & ifmt_br, { 0x1001 }
  },
/* call $Rbj,$Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RBJ), ',', OP (RD), 0 } },
    & ifmt_jmp, { 0xa0 }
  },
/* callf $abs24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ABS24), 0 } },
    & ifmt_jmpf, { 0x1000000 }
  },
/* icallr $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_movlowgr, { 0x30 }
  },
/* icall $Rbj,$Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RBJ), ',', OP (RD), 0 } },
    & ifmt_jmp, { 0x60 }
  },
/* icallf $abs24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ABS24), 0 } },
    & ifmt_jmpf, { 0x3000000 }
  },
/* iret */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0x2 }
  },
/* ret */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0x3 }
  },
/* mul */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xd0 }
  },
/* div */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xc0 }
  },
/* sdiv */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xc8 }
  },
/* sdivlh */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xe8 }
  },
/* divlh */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xe0 }
  },
/* reset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xf }
  },
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0x0 }
  },
/* halt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0x8 }
  },
/* hold */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xa }
  },
/* holdx */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0xb }
  },
/* brk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0x5 }
  },
/* --unused-- */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_iret, { 0x1 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & xstormy16_cgen_ifld_table[XSTORMY16_##f]
#else
#define F(f) & xstormy16_cgen_ifld_table[XSTORMY16_/**/f]
#endif
static const CGEN_IFMT ifmt_movimm8 = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgrimm8 = {
  16, 16, 0xf100, { { F (F_OP1) }, { F (F_RM) }, { F (F_OP2M) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgrimm16 = {
  32, 32, 0xfff00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RD) }, { F (F_IMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_incgr = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3A) }, { F (F_IMM2) }, { F (F_RD) }, { 0 } }
};

static const CGEN_IFMT ifmt_decgr = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3A) }, { F (F_IMM2) }, { F (F_RD) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) XSTORMY16_OPERAND_##op
#else
#define OPERAND(op) XSTORMY16_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE xstormy16_cgen_macro_insn_table[] =
{
/* mov Rx,#$imm8 */
  {
    -1, "movimm8", "mov", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* mov $Rm,#$imm8small */
  {
    -1, "movgrimm8", "mov", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* mov $Rd,#$imm16 */
  {
    -1, "movgrimm16", "mov", 32,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* inc $Rd */
  {
    -1, "incgr", "inc", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* dec $Rd */
  {
    -1, "decgr", "dec", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE xstormy16_cgen_macro_insn_opcode_table[] =
{
/* mov Rx,#$imm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'R', 'x', ',', '#', OP (IMM8), 0 } },
    & ifmt_movimm8, { 0x4700 }
  },
/* mov $Rm,#$imm8small */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RM), ',', '#', OP (IMM8SMALL), 0 } },
    & ifmt_movgrimm8, { 0x2100 }
  },
/* mov $Rd,#$imm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', '#', OP (IMM16), 0 } },
    & ifmt_movgrimm16, { 0x31300000 }
  },
/* inc $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_incgr, { 0x3000 }
  },
/* dec $Rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_decgr, { 0x3040 }
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
xstormy16_cgen_init_opcode_table (cd)
     CGEN_CPU_DESC cd;
{
  int i;
  int num_macros = (sizeof (xstormy16_cgen_macro_insn_table) /
		    sizeof (xstormy16_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & xstormy16_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & xstormy16_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (num_macros * sizeof (CGEN_INSN));
  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      xstormy16_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & xstormy16_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      xstormy16_cgen_build_insn_regex (& insns[i]);
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
