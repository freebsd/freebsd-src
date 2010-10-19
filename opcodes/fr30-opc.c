/* Instruction opcode table for fr30.

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
#include "fr30-desc.h"
#include "fr30-opc.h"
#include "libiberty.h"

/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p        (const CGEN_INSN *);
static unsigned int asm_hash_insn (const char *);
static int dis_hash_insn_p        (const CGEN_INSN *);
static unsigned int dis_hash_insn (const char *, CGEN_INSN_INT);

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & fr30_cgen_ifld_table[FR30_##f]
#else
#define F(f) & fr30_cgen_ifld_table[FR30_/**/f]
#endif
static const CGEN_IFMT ifmt_empty ATTRIBUTE_UNUSED = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_add ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_RJ) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_addi ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_U4) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_add2 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_M4) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_div0s ATTRIBUTE_UNUSED = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_div3 ATTRIBUTE_UNUSED = {
  16, 16, 0xffff, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_OP4) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi8 ATTRIBUTE_UNUSED = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_I8) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi20 ATTRIBUTE_UNUSED = {
  16, 32, 0xff00, { { F (F_OP1) }, { F (F_I20) }, { F (F_OP2) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi32 ATTRIBUTE_UNUSED = {
  16, 48, 0xfff0, { { F (F_OP1) }, { F (F_I32) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldr14 ATTRIBUTE_UNUSED = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_DISP10) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldr14uh ATTRIBUTE_UNUSED = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_DISP9) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldr14ub ATTRIBUTE_UNUSED = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_DISP8) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldr15 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_UDISP6) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldr15dr ATTRIBUTE_UNUSED = {
  16, 16, 0xfff0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RS2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movdr ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_RS1) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_call ATTRIBUTE_UNUSED = {
  16, 16, 0xf800, { { F (F_OP1) }, { F (F_OP5) }, { F (F_REL12) }, { 0 } }
};

static const CGEN_IFMT ifmt_int ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_U8) }, { 0 } }
};

static const CGEN_IFMT ifmt_brad ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_CC) }, { F (F_REL9) }, { 0 } }
};

static const CGEN_IFMT ifmt_dmovr13 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_DIR10) }, { 0 } }
};

static const CGEN_IFMT ifmt_dmovr13h ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_DIR9) }, { 0 } }
};

static const CGEN_IFMT ifmt_dmovr13b ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_DIR8) }, { 0 } }
};

static const CGEN_IFMT ifmt_copop ATTRIBUTE_UNUSED = {
  16, 32, 0xfff0, { { F (F_OP1) }, { F (F_CCC) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_CRJ) }, { F (F_U4C) }, { F (F_CRI) }, { 0 } }
};

static const CGEN_IFMT ifmt_copld ATTRIBUTE_UNUSED = {
  16, 32, 0xfff0, { { F (F_OP1) }, { F (F_CCC) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RJC) }, { F (F_U4C) }, { F (F_CRI) }, { 0 } }
};

static const CGEN_IFMT ifmt_copst ATTRIBUTE_UNUSED = {
  16, 32, 0xfff0, { { F (F_OP1) }, { F (F_CCC) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_CRJ) }, { F (F_U4C) }, { F (F_RIC) }, { 0 } }
};

static const CGEN_IFMT ifmt_addsp ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_S10) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldm0 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_REGLIST_LOW_LD) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldm1 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_REGLIST_HI_LD) }, { 0 } }
};

static const CGEN_IFMT ifmt_stm0 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_REGLIST_LOW_ST) }, { 0 } }
};

static const CGEN_IFMT ifmt_stm1 ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_REGLIST_HI_ST) }, { 0 } }
};

static const CGEN_IFMT ifmt_enter ATTRIBUTE_UNUSED = {
  16, 16, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_U10) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) FR30_OPERAND_##op
#else
#define OPERAND(op) FR30_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE fr30_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* add $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xa600 }
  },
/* add $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xa400 }
  },
/* add2 $m4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (M4), ',', OP (RI), 0 } },
    & ifmt_add2, { 0xa500 }
  },
/* addc $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xa700 }
  },
/* addn $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xa200 }
  },
/* addn $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xa000 }
  },
/* addn2 $m4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (M4), ',', OP (RI), 0 } },
    & ifmt_add2, { 0xa100 }
  },
/* sub $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xac00 }
  },
/* subc $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xad00 }
  },
/* subn $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xae00 }
  },
/* cmp $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xaa00 }
  },
/* cmp $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xa800 }
  },
/* cmp2 $m4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (M4), ',', OP (RI), 0 } },
    & ifmt_add2, { 0xa900 }
  },
/* and $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x8200 }
  },
/* or $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x9200 }
  },
/* eor $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x9a00 }
  },
/* and $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x8400 }
  },
/* andh $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x8500 }
  },
/* andb $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x8600 }
  },
/* or $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9400 }
  },
/* orh $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9500 }
  },
/* orb $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9600 }
  },
/* eor $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9c00 }
  },
/* eorh $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9d00 }
  },
/* eorb $Rj,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', '@', OP (RI), 0 } },
    & ifmt_add, { 0x9e00 }
  },
/* bandl $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8000 }
  },
/* borl $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9000 }
  },
/* beorl $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9800 }
  },
/* bandh $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8100 }
  },
/* borh $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9100 }
  },
/* beorh $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x9900 }
  },
/* btstl $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8800 }
  },
/* btsth $u4,@$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), 0 } },
    & ifmt_addi, { 0x8900 }
  },
/* mul $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xaf00 }
  },
/* mulu $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xab00 }
  },
/* mulh $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xbf00 }
  },
/* muluh $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xbb00 }
  },
/* div0s $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9740 }
  },
/* div0u $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9750 }
  },
/* div1 $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9760 }
  },
/* div2 $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9770 }
  },
/* div3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f60 }
  },
/* div4s */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f70 }
  },
/* lsl $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xb600 }
  },
/* lsl $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb400 }
  },
/* lsl2 $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb500 }
  },
/* lsr $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xb200 }
  },
/* lsr $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb000 }
  },
/* lsr2 $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb100 }
  },
/* asr $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0xba00 }
  },
/* asr $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb800 }
  },
/* asr2 $u4,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', OP (RI), 0 } },
    & ifmt_addi, { 0xb900 }
  },
/* ldi:8 $i8,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (I8), ',', OP (RI), 0 } },
    & ifmt_ldi8, { 0xc000 }
  },
/* ldi:20 $i20,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (I20), ',', OP (RI), 0 } },
    & ifmt_ldi20, { 0x9b00 }
  },
/* ldi:32 $i32,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (I32), ',', OP (RI), 0 } },
    & ifmt_ldi32, { 0x9f80 }
  },
/* ld @$Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x400 }
  },
/* lduh @$Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x500 }
  },
/* ldub @$Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x600 }
  },
/* ld @($R13,$Rj),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R13), ',', OP (RJ), ')', ',', OP (RI), 0 } },
    & ifmt_add, { 0x0 }
  },
/* lduh @($R13,$Rj),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R13), ',', OP (RJ), ')', ',', OP (RI), 0 } },
    & ifmt_add, { 0x100 }
  },
/* ldub @($R13,$Rj),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R13), ',', OP (RJ), ')', ',', OP (RI), 0 } },
    & ifmt_add, { 0x200 }
  },
/* ld @($R14,$disp10),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R14), ',', OP (DISP10), ')', ',', OP (RI), 0 } },
    & ifmt_ldr14, { 0x2000 }
  },
/* lduh @($R14,$disp9),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R14), ',', OP (DISP9), ')', ',', OP (RI), 0 } },
    & ifmt_ldr14uh, { 0x4000 }
  },
/* ldub @($R14,$disp8),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R14), ',', OP (DISP8), ')', ',', OP (RI), 0 } },
    & ifmt_ldr14ub, { 0x6000 }
  },
/* ld @($R15,$udisp6),$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', '(', OP (R15), ',', OP (UDISP6), ')', ',', OP (RI), 0 } },
    & ifmt_ldr15, { 0x300 }
  },
/* ld @$R15+,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R15), '+', ',', OP (RI), 0 } },
    & ifmt_div0s, { 0x700 }
  },
/* ld @$R15+,$Rs2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R15), '+', ',', OP (RS2), 0 } },
    & ifmt_ldr15dr, { 0x780 }
  },
/* ld @$R15+,$ps */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R15), '+', ',', OP (PS), 0 } },
    & ifmt_div3, { 0x790 }
  },
/* st $Ri,@$Rj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', OP (RJ), 0 } },
    & ifmt_add, { 0x1400 }
  },
/* sth $Ri,@$Rj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', OP (RJ), 0 } },
    & ifmt_add, { 0x1500 }
  },
/* stb $Ri,@$Rj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', OP (RJ), 0 } },
    & ifmt_add, { 0x1600 }
  },
/* st $Ri,@($R13,$Rj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R13), ',', OP (RJ), ')', 0 } },
    & ifmt_add, { 0x1000 }
  },
/* sth $Ri,@($R13,$Rj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R13), ',', OP (RJ), ')', 0 } },
    & ifmt_add, { 0x1100 }
  },
/* stb $Ri,@($R13,$Rj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R13), ',', OP (RJ), ')', 0 } },
    & ifmt_add, { 0x1200 }
  },
/* st $Ri,@($R14,$disp10) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R14), ',', OP (DISP10), ')', 0 } },
    & ifmt_ldr14, { 0x3000 }
  },
/* sth $Ri,@($R14,$disp9) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R14), ',', OP (DISP9), ')', 0 } },
    & ifmt_ldr14uh, { 0x5000 }
  },
/* stb $Ri,@($R14,$disp8) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R14), ',', OP (DISP8), ')', 0 } },
    & ifmt_ldr14ub, { 0x7000 }
  },
/* st $Ri,@($R15,$udisp6) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '(', OP (R15), ',', OP (UDISP6), ')', 0 } },
    & ifmt_ldr15, { 0x1300 }
  },
/* st $Ri,@-$R15 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', '@', '-', OP (R15), 0 } },
    & ifmt_div0s, { 0x1700 }
  },
/* st $Rs2,@-$R15 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS2), ',', '@', '-', OP (R15), 0 } },
    & ifmt_ldr15dr, { 0x1780 }
  },
/* st $ps,@-$R15 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (PS), ',', '@', '-', OP (R15), 0 } },
    & ifmt_div3, { 0x1790 }
  },
/* mov $Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x8b00 }
  },
/* mov $Rs1,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS1), ',', OP (RI), 0 } },
    & ifmt_movdr, { 0xb700 }
  },
/* mov $ps,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (PS), ',', OP (RI), 0 } },
    & ifmt_div0s, { 0x1710 }
  },
/* mov $Ri,$Rs1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', OP (RS1), 0 } },
    & ifmt_movdr, { 0xb300 }
  },
/* mov $Ri,$ps */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), ',', OP (PS), 0 } },
    & ifmt_div0s, { 0x710 }
  },
/* jmp @$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9700 }
  },
/* jmp:d @$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9f00 }
  },
/* call @$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9710 }
  },
/* call:d @$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RI), 0 } },
    & ifmt_div0s, { 0x9f10 }
  },
/* call $label12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL12), 0 } },
    & ifmt_call, { 0xd000 }
  },
/* call:d $label12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL12), 0 } },
    & ifmt_call, { 0xd800 }
  },
/* ret */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9720 }
  },
/* ret:d */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f20 }
  },
/* int $u8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x1f00 }
  },
/* inte */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f30 }
  },
/* reti */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9730 }
  },
/* bra:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf000 }
  },
/* bra $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe000 }
  },
/* bno:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf100 }
  },
/* bno $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe100 }
  },
/* beq:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf200 }
  },
/* beq $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe200 }
  },
/* bne:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf300 }
  },
/* bne $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe300 }
  },
/* bc:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf400 }
  },
/* bc $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe400 }
  },
/* bnc:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf500 }
  },
/* bnc $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe500 }
  },
/* bn:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf600 }
  },
/* bn $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe600 }
  },
/* bp:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf700 }
  },
/* bp $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe700 }
  },
/* bv:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf800 }
  },
/* bv $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe800 }
  },
/* bnv:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xf900 }
  },
/* bnv $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xe900 }
  },
/* blt:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfa00 }
  },
/* blt $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xea00 }
  },
/* bge:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfb00 }
  },
/* bge $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xeb00 }
  },
/* ble:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfc00 }
  },
/* ble $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xec00 }
  },
/* bgt:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfd00 }
  },
/* bgt $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xed00 }
  },
/* bls:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xfe00 }
  },
/* bls $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xee00 }
  },
/* bhi:d $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xff00 }
  },
/* bhi $label9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (LABEL9), 0 } },
    & ifmt_brad, { 0xef00 }
  },
/* dmov $R13,@$dir10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (R13), ',', '@', OP (DIR10), 0 } },
    & ifmt_dmovr13, { 0x1800 }
  },
/* dmovh $R13,@$dir9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (R13), ',', '@', OP (DIR9), 0 } },
    & ifmt_dmovr13h, { 0x1900 }
  },
/* dmovb $R13,@$dir8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (R13), ',', '@', OP (DIR8), 0 } },
    & ifmt_dmovr13b, { 0x1a00 }
  },
/* dmov @$R13+,@$dir10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R13), '+', ',', '@', OP (DIR10), 0 } },
    & ifmt_dmovr13, { 0x1c00 }
  },
/* dmovh @$R13+,@$dir9 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R13), '+', ',', '@', OP (DIR9), 0 } },
    & ifmt_dmovr13h, { 0x1d00 }
  },
/* dmovb @$R13+,@$dir8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R13), '+', ',', '@', OP (DIR8), 0 } },
    & ifmt_dmovr13b, { 0x1e00 }
  },
/* dmov @$R15+,@$dir10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (R15), '+', ',', '@', OP (DIR10), 0 } },
    & ifmt_dmovr13, { 0x1b00 }
  },
/* dmov @$dir10,$R13 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR10), ',', OP (R13), 0 } },
    & ifmt_dmovr13, { 0x800 }
  },
/* dmovh @$dir9,$R13 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR9), ',', OP (R13), 0 } },
    & ifmt_dmovr13h, { 0x900 }
  },
/* dmovb @$dir8,$R13 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR8), ',', OP (R13), 0 } },
    & ifmt_dmovr13b, { 0xa00 }
  },
/* dmov @$dir10,@$R13+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR10), ',', '@', OP (R13), '+', 0 } },
    & ifmt_dmovr13, { 0xc00 }
  },
/* dmovh @$dir9,@$R13+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR9), ',', '@', OP (R13), '+', 0 } },
    & ifmt_dmovr13h, { 0xd00 }
  },
/* dmovb @$dir8,@$R13+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR8), ',', '@', OP (R13), '+', 0 } },
    & ifmt_dmovr13b, { 0xe00 }
  },
/* dmov @$dir10,@-$R15 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (DIR10), ',', '@', '-', OP (R15), 0 } },
    & ifmt_dmovr13, { 0xb00 }
  },
/* ldres @$Ri+,$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RI), '+', ',', OP (U4), 0 } },
    & ifmt_addi, { 0xbc00 }
  },
/* stres $u4,@$Ri+ */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4), ',', '@', OP (RI), '+', 0 } },
    & ifmt_addi, { 0xbd00 }
  },
/* copop $u4c,$ccc,$CRj,$CRi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (CRJ), ',', OP (CRI), 0 } },
    & ifmt_copop, { 0x9fc0 }
  },
/* copld $u4c,$ccc,$Rjc,$CRi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (RJC), ',', OP (CRI), 0 } },
    & ifmt_copld, { 0x9fd0 }
  },
/* copst $u4c,$ccc,$CRj,$Ric */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (CRJ), ',', OP (RIC), 0 } },
    & ifmt_copst, { 0x9fe0 }
  },
/* copsv $u4c,$ccc,$CRj,$Ric */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U4C), ',', OP (CCC), ',', OP (CRJ), ',', OP (RIC), 0 } },
    & ifmt_copst, { 0x9ff0 }
  },
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9fa0 }
  },
/* andccr $u8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x8300 }
  },
/* orccr $u8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x9300 }
  },
/* stilm $u8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U8), 0 } },
    & ifmt_int, { 0x8700 }
  },
/* addsp $s10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (S10), 0 } },
    & ifmt_addsp, { 0xa300 }
  },
/* extsb $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9780 }
  },
/* extub $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x9790 }
  },
/* extsh $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x97a0 }
  },
/* extuh $Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RI), 0 } },
    & ifmt_div0s, { 0x97b0 }
  },
/* ldm0 ($reglist_low_ld) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '(', OP (REGLIST_LOW_LD), ')', 0 } },
    & ifmt_ldm0, { 0x8c00 }
  },
/* ldm1 ($reglist_hi_ld) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '(', OP (REGLIST_HI_LD), ')', 0 } },
    & ifmt_ldm1, { 0x8d00 }
  },
/* stm0 ($reglist_low_st) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '(', OP (REGLIST_LOW_ST), ')', 0 } },
    & ifmt_stm0, { 0x8e00 }
  },
/* stm1 ($reglist_hi_st) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '(', OP (REGLIST_HI_ST), ')', 0 } },
    & ifmt_stm1, { 0x8f00 }
  },
/* enter $u10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (U10), 0 } },
    & ifmt_enter, { 0xf00 }
  },
/* leave */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_div3, { 0x9f90 }
  },
/* xchb @$Rj,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '@', OP (RJ), ',', OP (RI), 0 } },
    & ifmt_add, { 0x8a00 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & fr30_cgen_ifld_table[FR30_##f]
#else
#define F(f) & fr30_cgen_ifld_table[FR30_/**/f]
#endif
static const CGEN_IFMT ifmt_ldi8m ATTRIBUTE_UNUSED = {
  16, 16, 0xf000, { { F (F_OP1) }, { F (F_I8) }, { F (F_RI) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi20m ATTRIBUTE_UNUSED = {
  16, 32, 0xff00, { { F (F_OP1) }, { F (F_OP2) }, { F (F_RI) }, { F (F_I20) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldi32m ATTRIBUTE_UNUSED = {
  16, 48, 0xfff0, { { F (F_OP1) }, { F (F_OP2) }, { F (F_OP3) }, { F (F_RI) }, { F (F_I32) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) FR30_OPERAND_##op
#else
#define OPERAND(op) FR30_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE fr30_cgen_macro_insn_table[] =
{
/* ldi8 $i8,$Ri */
  {
    -1, "ldi8m", "ldi8", 16,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* ldi20 $i20,$Ri */
  {
    -1, "ldi20m", "ldi20", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* ldi32 $i32,$Ri */
  {
    -1, "ldi32m", "ldi32", 48,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE fr30_cgen_macro_insn_opcode_table[] =
{
/* ldi8 $i8,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (I8), ',', OP (RI), 0 } },
    & ifmt_ldi8m, { 0xc000 }
  },
/* ldi20 $i20,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (I20), ',', OP (RI), 0 } },
    & ifmt_ldi20m, { 0x9b00 }
  },
/* ldi32 $i32,$Ri */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (I32), ',', OP (RI), 0 } },
    & ifmt_ldi32m, { 0x9f80 }
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

/* Set the recorded length of the insn in the CGEN_FIELDS struct.  */

static void
set_fields_bitsize (CGEN_FIELDS *fields, int size)
{
  CGEN_FIELDS_BITSIZE (fields) = size;
}

/* Function to call before using the operand instance table.
   This plugs the opcode entries and macro instructions into the cpu table.  */

void
fr30_cgen_init_opcode_table (CGEN_CPU_DESC cd)
{
  int i;
  int num_macros = (sizeof (fr30_cgen_macro_insn_table) /
		    sizeof (fr30_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & fr30_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & fr30_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = xmalloc (num_macros * sizeof (CGEN_INSN));

  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      fr30_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & fr30_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      fr30_cgen_build_insn_regex (& insns[i]);
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
