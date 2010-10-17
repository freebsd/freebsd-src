/* Instruction opcode table for m32r.

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
#include "m32r-desc.h"
#include "m32r-opc.h"
#include "libiberty.h"

/* -- opc.c */
unsigned int
m32r_cgen_dis_hash (buf, value)
     const char * buf ATTRIBUTE_UNUSED;
     CGEN_INSN_INT value;
{
  unsigned int x;
                                                                                
  if (value & 0xffff0000) /* 32bit instructions */
    value = (value >> 16) & 0xffff;
                                                                                
  x = (value>>8) & 0xf0;
  if (x == 0x40 || x == 0xe0 || x == 0x60 || x == 0x50)
    return x;
                                                                                
  if (x == 0x70 || x == 0xf0)
    return x | ((value>>8) & 0x0f);
                                                                                
  if (x == 0x30)
    return x | ((value & 0x70) >> 4);
  else
    return x | ((value & 0xf0) >> 4);
}
                                                                                
/* -- */
/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int asm_hash_insn PARAMS ((const char *));
static int dis_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int dis_hash_insn PARAMS ((const char *, CGEN_INSN_INT));

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & m32r_cgen_ifld_table[M32R_##f]
#else
#define F(f) & m32r_cgen_ifld_table[M32R_/**/f]
#endif
static const CGEN_IFMT ifmt_empty = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_add = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_add3 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_and3 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_or3 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_addi = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_R1) }, { F (F_SIMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_addv3 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_bc8 = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bc24 = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_beq = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_DISP16) }, { 0 } }
};

static const CGEN_IFMT ifmt_beqz = {
  32, 32, 0xfff00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_DISP16) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmp = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmpi = {
  32, 32, 0xfff00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmpz = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_div = {
  32, 32, 0xf0f0ffff, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_jc = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ld24 = {
  32, 32, 0xf0000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_UIMM24) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi16 = {
  32, 32, 0xf0ff0000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_machi_a = {
  16, 16, 0xf070, { { F (F_OP1) }, { F (F_R1) }, { F (F_ACC) }, { F (F_OP23) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_mvfachi = {
  16, 16, 0xf0ff, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_mvfachi_a = {
  16, 16, 0xf0f3, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_OP3) }, { 0 } }
};

static const CGEN_IFMT ifmt_mvfc = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_mvtachi = {
  16, 16, 0xf0ff, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_mvtachi_a = {
  16, 16, 0xf0f3, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_OP3) }, { 0 } }
};

static const CGEN_IFMT ifmt_mvtc = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_nop = {
  16, 16, 0xffff, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_rac_dsi = {
  16, 16, 0xf3f2, { { F (F_OP1) }, { F (F_ACCD) }, { F (F_BITS67) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_BIT14) }, { F (F_IMM1) }, { 0 } }
};

static const CGEN_IFMT ifmt_seth = {
  32, 32, 0xf0ff0000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_HI16) }, { 0 } }
};

static const CGEN_IFMT ifmt_slli = {
  16, 16, 0xf0e0, { { F (F_OP1) }, { F (F_R1) }, { F (F_SHIFT_OP2) }, { F (F_UIMM5) }, { 0 } }
};

static const CGEN_IFMT ifmt_st_d = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_trap = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_UIMM4) }, { 0 } }
};

static const CGEN_IFMT ifmt_satb = {
  32, 32, 0xf0f0ffff, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_UIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_clrpsw = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_UIMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bset = {
  32, 32, 0xf8f00000, { { F (F_OP1) }, { F (F_BIT4) }, { F (F_UIMM3) }, { F (F_OP2) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_btst = {
  16, 16, 0xf8f0, { { F (F_OP1) }, { F (F_BIT4) }, { F (F_UIMM3) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) M32R_OPERAND_##op
#else
#define OPERAND(op) M32R_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE m32r_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* add $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0xa0 }
  },
/* add3 $dr,$sr,$hash$slo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (HASH), OP (SLO16), 0 } },
    & ifmt_add3, { 0x80a00000 }
  },
/* and $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0xc0 }
  },
/* and3 $dr,$sr,$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (UIMM16), 0 } },
    & ifmt_and3, { 0x80c00000 }
  },
/* or $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0xe0 }
  },
/* or3 $dr,$sr,$hash$ulo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (HASH), OP (ULO16), 0 } },
    & ifmt_or3, { 0x80e00000 }
  },
/* xor $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0xd0 }
  },
/* xor3 $dr,$sr,$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (UIMM16), 0 } },
    & ifmt_and3, { 0x80d00000 }
  },
/* addi $dr,$simm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SIMM8), 0 } },
    & ifmt_addi, { 0x4000 }
  },
/* addv $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x80 }
  },
/* addv3 $dr,$sr,$simm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (SIMM16), 0 } },
    & ifmt_addv3, { 0x80800000 }
  },
/* addx $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x90 }
  },
/* bc.s $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8, { 0x7c00 }
  },
/* bc.l $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24, { 0xfc000000 }
  },
/* beq $src1,$src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beq, { 0xb0000000 }
  },
/* beqz $src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beqz, { 0xb0800000 }
  },
/* bgez $src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beqz, { 0xb0b00000 }
  },
/* bgtz $src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beqz, { 0xb0d00000 }
  },
/* blez $src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beqz, { 0xb0c00000 }
  },
/* bltz $src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beqz, { 0xb0a00000 }
  },
/* bnez $src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beqz, { 0xb0900000 }
  },
/* bl.s $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8, { 0x7e00 }
  },
/* bl.l $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24, { 0xfe000000 }
  },
/* bcl.s $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8, { 0x7800 }
  },
/* bcl.l $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24, { 0xf8000000 }
  },
/* bnc.s $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8, { 0x7d00 }
  },
/* bnc.l $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24, { 0xfd000000 }
  },
/* bne $src1,$src2,$disp16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (DISP16), 0 } },
    & ifmt_beq, { 0xb0100000 }
  },
/* bra.s $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8, { 0x7f00 }
  },
/* bra.l $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24, { 0xff000000 }
  },
/* bncl.s $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8, { 0x7900 }
  },
/* bncl.l $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24, { 0xf9000000 }
  },
/* cmp $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x40 }
  },
/* cmpi $src2,$simm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (SIMM16), 0 } },
    & ifmt_cmpi, { 0x80400000 }
  },
/* cmpu $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x50 }
  },
/* cmpui $src2,$simm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), ',', OP (SIMM16), 0 } },
    & ifmt_cmpi, { 0x80500000 }
  },
/* cmpeq $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x60 }
  },
/* cmpz $src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), 0 } },
    & ifmt_cmpz, { 0x70 }
  },
/* div $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90000000 }
  },
/* divu $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90100000 }
  },
/* rem $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90200000 }
  },
/* remu $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90300000 }
  },
/* remh $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90200010 }
  },
/* remuh $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90300010 }
  },
/* remb $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90200018 }
  },
/* remub $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90300018 }
  },
/* divuh $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90100010 }
  },
/* divb $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90000018 }
  },
/* divub $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90100018 }
  },
/* divh $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_div, { 0x90000010 }
  },
/* jc $sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), 0 } },
    & ifmt_jc, { 0x1cc0 }
  },
/* jnc $sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), 0 } },
    & ifmt_jc, { 0x1dc0 }
  },
/* jl $sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), 0 } },
    & ifmt_jc, { 0x1ec0 }
  },
/* jmp $sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), 0 } },
    & ifmt_jc, { 0x1fc0 }
  },
/* ld $dr,@$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), 0 } },
    & ifmt_add, { 0x20c0 }
  },
/* ld $dr,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_add3, { 0xa0c00000 }
  },
/* ldb $dr,@$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), 0 } },
    & ifmt_add, { 0x2080 }
  },
/* ldb $dr,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_add3, { 0xa0800000 }
  },
/* ldh $dr,@$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), 0 } },
    & ifmt_add, { 0x20a0 }
  },
/* ldh $dr,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_add3, { 0xa0a00000 }
  },
/* ldub $dr,@$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), 0 } },
    & ifmt_add, { 0x2090 }
  },
/* ldub $dr,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_add3, { 0xa0900000 }
  },
/* lduh $dr,@$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), 0 } },
    & ifmt_add, { 0x20b0 }
  },
/* lduh $dr,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_add3, { 0xa0b00000 }
  },
/* ld $dr,@$sr+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), '+', 0 } },
    & ifmt_add, { 0x20e0 }
  },
/* ld24 $dr,$uimm24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (UIMM24), 0 } },
    & ifmt_ld24, { 0xe0000000 }
  },
/* ldi8 $dr,$simm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SIMM8), 0 } },
    & ifmt_addi, { 0x6000 }
  },
/* ldi16 $dr,$hash$slo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (SLO16), 0 } },
    & ifmt_ldi16, { 0x90f00000 }
  },
/* lock $dr,@$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', OP (SR), 0 } },
    & ifmt_add, { 0x20d0 }
  },
/* machi $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3040 }
  },
/* machi $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3040 }
  },
/* maclo $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3050 }
  },
/* maclo $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3050 }
  },
/* macwhi $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3060 }
  },
/* macwhi $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3060 }
  },
/* macwlo $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3070 }
  },
/* macwlo $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3070 }
  },
/* mul $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x1060 }
  },
/* mulhi $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3000 }
  },
/* mulhi $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3000 }
  },
/* mullo $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3010 }
  },
/* mullo $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3010 }
  },
/* mulwhi $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3020 }
  },
/* mulwhi $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3020 }
  },
/* mulwlo $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x3030 }
  },
/* mulwlo $src1,$src2,$acc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), ',', OP (ACC), 0 } },
    & ifmt_machi_a, { 0x3030 }
  },
/* mv $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x1080 }
  },
/* mvfachi $dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), 0 } },
    & ifmt_mvfachi, { 0x50f0 }
  },
/* mvfachi $dr,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (ACCS), 0 } },
    & ifmt_mvfachi_a, { 0x50f0 }
  },
/* mvfaclo $dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), 0 } },
    & ifmt_mvfachi, { 0x50f1 }
  },
/* mvfaclo $dr,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (ACCS), 0 } },
    & ifmt_mvfachi_a, { 0x50f1 }
  },
/* mvfacmi $dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), 0 } },
    & ifmt_mvfachi, { 0x50f2 }
  },
/* mvfacmi $dr,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (ACCS), 0 } },
    & ifmt_mvfachi_a, { 0x50f2 }
  },
/* mvfc $dr,$scr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SCR), 0 } },
    & ifmt_mvfc, { 0x1090 }
  },
/* mvtachi $src1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), 0 } },
    & ifmt_mvtachi, { 0x5070 }
  },
/* mvtachi $src1,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (ACCS), 0 } },
    & ifmt_mvtachi_a, { 0x5070 }
  },
/* mvtaclo $src1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), 0 } },
    & ifmt_mvtachi, { 0x5071 }
  },
/* mvtaclo $src1,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (ACCS), 0 } },
    & ifmt_mvtachi_a, { 0x5071 }
  },
/* mvtc $sr,$dcr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (DCR), 0 } },
    & ifmt_mvtc, { 0x10a0 }
  },
/* neg $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x30 }
  },
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x7000 }
  },
/* not $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0xb0 }
  },
/* rac */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x5090 }
  },
/* rac $accd,$accs,$imm1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ACCD), ',', OP (ACCS), ',', OP (IMM1), 0 } },
    & ifmt_rac_dsi, { 0x5090 }
  },
/* rach */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x5080 }
  },
/* rach $accd,$accs,$imm1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ACCD), ',', OP (ACCS), ',', OP (IMM1), 0 } },
    & ifmt_rac_dsi, { 0x5080 }
  },
/* rte */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x10d6 }
  },
/* seth $dr,$hash$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (HI16), 0 } },
    & ifmt_seth, { 0xd0c00000 }
  },
/* sll $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x1040 }
  },
/* sll3 $dr,$sr,$simm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (SIMM16), 0 } },
    & ifmt_addv3, { 0x90c00000 }
  },
/* slli $dr,$uimm5 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (UIMM5), 0 } },
    & ifmt_slli, { 0x5040 }
  },
/* sra $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x1020 }
  },
/* sra3 $dr,$sr,$simm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (SIMM16), 0 } },
    & ifmt_addv3, { 0x90a00000 }
  },
/* srai $dr,$uimm5 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (UIMM5), 0 } },
    & ifmt_slli, { 0x5020 }
  },
/* srl $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x1000 }
  },
/* srl3 $dr,$sr,$simm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), ',', OP (SIMM16), 0 } },
    & ifmt_addv3, { 0x90800000 }
  },
/* srli $dr,$uimm5 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (UIMM5), 0 } },
    & ifmt_slli, { 0x5000 }
  },
/* st $src1,@$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x2040 }
  },
/* st $src1,@($slo16,$src2) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SLO16), ',', OP (SRC2), ')', 0 } },
    & ifmt_st_d, { 0xa0400000 }
  },
/* stb $src1,@$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x2000 }
  },
/* stb $src1,@($slo16,$src2) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SLO16), ',', OP (SRC2), ')', 0 } },
    & ifmt_st_d, { 0xa0000000 }
  },
/* sth $src1,@$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x2020 }
  },
/* sth $src1,@($slo16,$src2) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SLO16), ',', OP (SRC2), ')', 0 } },
    & ifmt_st_d, { 0xa0200000 }
  },
/* st $src1,@+$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '+', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x2060 }
  },
/* sth $src1,@$src2+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', OP (SRC2), '+', 0 } },
    & ifmt_cmp, { 0x2030 }
  },
/* stb $src1,@$src2+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', OP (SRC2), '+', 0 } },
    & ifmt_cmp, { 0x2010 }
  },
/* st $src1,@-$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '-', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x2070 }
  },
/* sub $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x20 }
  },
/* subv $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x0 }
  },
/* subx $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_add, { 0x10 }
  },
/* trap $uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM4), 0 } },
    & ifmt_trap, { 0x10f0 }
  },
/* unlock $src1,@$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x2050 }
  },
/* satb $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_satb, { 0x80600300 }
  },
/* sath $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_satb, { 0x80600200 }
  },
/* sat $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_satb, { 0x80600000 }
  },
/* pcmpbz $src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC2), 0 } },
    & ifmt_cmpz, { 0x370 }
  },
/* sadd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x50e4 }
  },
/* macwu1 $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x50b0 }
  },
/* msblo $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x50d0 }
  },
/* mulwu1 $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x50a0 }
  },
/* maclh1 $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_cmp, { 0x50c0 }
  },
/* sc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x7401 }
  },
/* snc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x7501 }
  },
/* clrpsw $uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM8), 0 } },
    & ifmt_clrpsw, { 0x7200 }
  },
/* setpsw $uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM8), 0 } },
    & ifmt_clrpsw, { 0x7100 }
  },
/* bset $uimm3,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM3), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_bset, { 0xa0600000 }
  },
/* bclr $uimm3,@($slo16,$sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM3), ',', '@', '(', OP (SLO16), ',', OP (SR), ')', 0 } },
    & ifmt_bset, { 0xa0700000 }
  },
/* btst $uimm3,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (UIMM3), ',', OP (SR), 0 } },
    & ifmt_btst, { 0xf0 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & m32r_cgen_ifld_table[M32R_##f]
#else
#define F(f) & m32r_cgen_ifld_table[M32R_/**/f]
#endif
static const CGEN_IFMT ifmt_bc8r = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bc24r = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_bl8r = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bl24r = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_bcl8r = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bcl24r = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_bnc8r = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bnc24r = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_bra8r = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bra24r = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_bncl8r = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP8) }, { 0 } }
};

static const CGEN_IFMT ifmt_bncl24r = {
  32, 32, 0xff000000, { { F (F_OP1) }, { F (F_R1) }, { F (F_DISP24) }, { 0 } }
};

static const CGEN_IFMT ifmt_ld_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ld_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldb_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldb_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldh_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldh_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldub_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldub_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_lduh_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_lduh_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_pop = {
  16, 16, 0xf0ff, { { F (F_OP1) }, { F (F_R1) }, { F (F_OP2) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi8a = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_R1) }, { F (F_SIMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi16a = {
  32, 32, 0xf0ff0000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R2) }, { F (F_R1) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_rac_d = {
  16, 16, 0xf3ff, { { F (F_OP1) }, { F (F_ACCD) }, { F (F_BITS67) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_BIT14) }, { F (F_IMM1) }, { 0 } }
};

static const CGEN_IFMT ifmt_rac_ds = {
  16, 16, 0xf3f3, { { F (F_OP1) }, { F (F_ACCD) }, { F (F_BITS67) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_BIT14) }, { F (F_IMM1) }, { 0 } }
};

static const CGEN_IFMT ifmt_rach_d = {
  16, 16, 0xf3ff, { { F (F_OP1) }, { F (F_ACCD) }, { F (F_BITS67) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_BIT14) }, { F (F_IMM1) }, { 0 } }
};

static const CGEN_IFMT ifmt_rach_ds = {
  16, 16, 0xf3f3, { { F (F_OP1) }, { F (F_ACCD) }, { F (F_BITS67) }, { F (F_OP2) }, { F (F_ACCS) }, { F (F_BIT14) }, { F (F_IMM1) }, { 0 } }
};

static const CGEN_IFMT ifmt_st_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_st_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_stb_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_stb_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_sth_2 = {
  16, 16, 0xf0f0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

static const CGEN_IFMT ifmt_sth_d2 = {
  32, 32, 0xf0f00000, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { F (F_SIMM16) }, { 0 } }
};

static const CGEN_IFMT ifmt_push = {
  16, 16, 0xf0ff, { { F (F_OP1) }, { F (F_OP2) }, { F (F_R1) }, { F (F_R2) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) M32R_OPERAND_##op
#else
#define OPERAND(op) M32R_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE m32r_cgen_macro_insn_table[] =
{
/* bc $disp8 */
  {
    -1, "bc8r", "bc", 16,
    { 0|A(RELAXABLE)|A(COND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* bc $disp24 */
  {
    -1, "bc24r", "bc", 32,
    { 0|A(RELAXED)|A(COND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* bl $disp8 */
  {
    -1, "bl8r", "bl", 16,
    { 0|A(RELAXABLE)|A(FILL_SLOT)|A(UNCOND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* bl $disp24 */
  {
    -1, "bl24r", "bl", 32,
    { 0|A(RELAXED)|A(UNCOND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* bcl $disp8 */
  {
    -1, "bcl8r", "bcl", 16,
    { 0|A(RELAXABLE)|A(FILL_SLOT)|A(COND_CTI)|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_O } }
  },
/* bcl $disp24 */
  {
    -1, "bcl24r", "bcl", 32,
    { 0|A(RELAXED)|A(COND_CTI)|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_NONE } }
  },
/* bnc $disp8 */
  {
    -1, "bnc8r", "bnc", 16,
    { 0|A(RELAXABLE)|A(COND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* bnc $disp24 */
  {
    -1, "bnc24r", "bnc", 32,
    { 0|A(RELAXED)|A(COND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* bra $disp8 */
  {
    -1, "bra8r", "bra", 16,
    { 0|A(RELAXABLE)|A(FILL_SLOT)|A(UNCOND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* bra $disp24 */
  {
    -1, "bra24r", "bra", 32,
    { 0|A(RELAXED)|A(UNCOND_CTI)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* bncl $disp8 */
  {
    -1, "bncl8r", "bncl", 16,
    { 0|A(RELAXABLE)|A(FILL_SLOT)|A(COND_CTI)|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_O } }
  },
/* bncl $disp24 */
  {
    -1, "bncl24r", "bncl", 32,
    { 0|A(RELAXED)|A(COND_CTI)|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_NONE } }
  },
/* ld $dr,@($sr) */
  {
    -1, "ld-2", "ld", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* ld $dr,@($sr,$slo16) */
  {
    -1, "ld-d2", "ld", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* ldb $dr,@($sr) */
  {
    -1, "ldb-2", "ldb", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* ldb $dr,@($sr,$slo16) */
  {
    -1, "ldb-d2", "ldb", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* ldh $dr,@($sr) */
  {
    -1, "ldh-2", "ldh", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* ldh $dr,@($sr,$slo16) */
  {
    -1, "ldh-d2", "ldh", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* ldub $dr,@($sr) */
  {
    -1, "ldub-2", "ldub", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* ldub $dr,@($sr,$slo16) */
  {
    -1, "ldub-d2", "ldub", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* lduh $dr,@($sr) */
  {
    -1, "lduh-2", "lduh", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* lduh $dr,@($sr,$slo16) */
  {
    -1, "lduh-d2", "lduh", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* pop $dr */
  {
    -1, "pop", "pop", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* ldi $dr,$simm8 */
  {
    -1, "ldi8a", "ldi", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE), PIPE_OS } }
  },
/* ldi $dr,$hash$slo16 */
  {
    -1, "ldi16a", "ldi", 32,
    { 0|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* rac $accd */
  {
    -1, "rac-d", "rac", 16,
    { 0|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_S } }
  },
/* rac $accd,$accs */
  {
    -1, "rac-ds", "rac", 16,
    { 0|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_S } }
  },
/* rach $accd */
  {
    -1, "rach-d", "rach", 16,
    { 0|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_S } }
  },
/* rach $accd,$accs */
  {
    -1, "rach-ds", "rach", 16,
    { 0|A(ALIAS), { (1<<MACH_M32RX)|(1<<MACH_M32R2), PIPE_S } }
  },
/* st $src1,@($src2) */
  {
    -1, "st-2", "st", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* st $src1,@($src2,$slo16) */
  {
    -1, "st-d2", "st", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* stb $src1,@($src2) */
  {
    -1, "stb-2", "stb", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* stb $src1,@($src2,$slo16) */
  {
    -1, "stb-d2", "stb", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* sth $src1,@($src2) */
  {
    -1, "sth-2", "sth", 16,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
/* sth $src1,@($src2,$slo16) */
  {
    -1, "sth-d2", "sth", 32,
    { 0|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE), PIPE_NONE } }
  },
/* push $src1 */
  {
    -1, "push", "push", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE), PIPE_O } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE m32r_cgen_macro_insn_opcode_table[] =
{
/* bc $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bc8r, { 0x7c00 }
  },
/* bc $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bc24r, { 0xfc000000 }
  },
/* bl $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bl8r, { 0x7e00 }
  },
/* bl $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bl24r, { 0xfe000000 }
  },
/* bcl $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bcl8r, { 0x7800 }
  },
/* bcl $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bcl24r, { 0xf8000000 }
  },
/* bnc $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bnc8r, { 0x7d00 }
  },
/* bnc $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bnc24r, { 0xfd000000 }
  },
/* bra $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bra8r, { 0x7f00 }
  },
/* bra $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bra24r, { 0xff000000 }
  },
/* bncl $disp8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP8), 0 } },
    & ifmt_bncl8r, { 0x7900 }
  },
/* bncl $disp24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DISP24), 0 } },
    & ifmt_bncl24r, { 0xf9000000 }
  },
/* ld $dr,@($sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ')', 0 } },
    & ifmt_ld_2, { 0x20c0 }
  },
/* ld $dr,@($sr,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ',', OP (SLO16), ')', 0 } },
    & ifmt_ld_d2, { 0xa0c00000 }
  },
/* ldb $dr,@($sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ')', 0 } },
    & ifmt_ldb_2, { 0x2080 }
  },
/* ldb $dr,@($sr,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ',', OP (SLO16), ')', 0 } },
    & ifmt_ldb_d2, { 0xa0800000 }
  },
/* ldh $dr,@($sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ')', 0 } },
    & ifmt_ldh_2, { 0x20a0 }
  },
/* ldh $dr,@($sr,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ',', OP (SLO16), ')', 0 } },
    & ifmt_ldh_d2, { 0xa0a00000 }
  },
/* ldub $dr,@($sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ')', 0 } },
    & ifmt_ldub_2, { 0x2090 }
  },
/* ldub $dr,@($sr,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ',', OP (SLO16), ')', 0 } },
    & ifmt_ldub_d2, { 0xa0900000 }
  },
/* lduh $dr,@($sr) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ')', 0 } },
    & ifmt_lduh_2, { 0x20b0 }
  },
/* lduh $dr,@($sr,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '@', '(', OP (SR), ',', OP (SLO16), ')', 0 } },
    & ifmt_lduh_d2, { 0xa0b00000 }
  },
/* pop $dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), 0 } },
    & ifmt_pop, { 0x20ef }
  },
/* ldi $dr,$simm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SIMM8), 0 } },
    & ifmt_ldi8a, { 0x6000 }
  },
/* ldi $dr,$hash$slo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (SLO16), 0 } },
    & ifmt_ldi16a, { 0x90f00000 }
  },
/* rac $accd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ACCD), 0 } },
    & ifmt_rac_d, { 0x5090 }
  },
/* rac $accd,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ACCD), ',', OP (ACCS), 0 } },
    & ifmt_rac_ds, { 0x5090 }
  },
/* rach $accd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ACCD), 0 } },
    & ifmt_rach_d, { 0x5080 }
  },
/* rach $accd,$accs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ACCD), ',', OP (ACCS), 0 } },
    & ifmt_rach_ds, { 0x5080 }
  },
/* st $src1,@($src2) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SRC2), ')', 0 } },
    & ifmt_st_2, { 0x2040 }
  },
/* st $src1,@($src2,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SRC2), ',', OP (SLO16), ')', 0 } },
    & ifmt_st_d2, { 0xa0400000 }
  },
/* stb $src1,@($src2) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SRC2), ')', 0 } },
    & ifmt_stb_2, { 0x2000 }
  },
/* stb $src1,@($src2,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SRC2), ',', OP (SLO16), ')', 0 } },
    & ifmt_stb_d2, { 0xa0000000 }
  },
/* sth $src1,@($src2) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SRC2), ')', 0 } },
    & ifmt_sth_2, { 0x2020 }
  },
/* sth $src1,@($src2,$slo16) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', '@', '(', OP (SRC2), ',', OP (SLO16), ')', 0 } },
    & ifmt_sth_d2, { 0xa0200000 }
  },
/* push $src1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), 0 } },
    & ifmt_push, { 0x207f }
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
m32r_cgen_init_opcode_table (cd)
     CGEN_CPU_DESC cd;
{
  int i;
  int num_macros = (sizeof (m32r_cgen_macro_insn_table) /
		    sizeof (m32r_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & m32r_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & m32r_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (num_macros * sizeof (CGEN_INSN));
  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      m32r_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & m32r_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      m32r_cgen_build_insn_regex (& insns[i]);
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
