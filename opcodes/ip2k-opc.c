/* Instruction opcode table for ip2k.

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
#include "ip2k-desc.h"
#include "ip2k-opc.h"
#include "libiberty.h"

/* -- opc.c */

#include "safe-ctype.h"

/* A better hash function for instruction mnemonics. */
unsigned int
ip2k_asm_hash (insn)
     const char* insn;
{
  unsigned int hash;
  const char* m = insn;

  for (hash = 0; *m && !ISSPACE(*m); m++)
    hash = (hash * 23) ^ (0x1F & TOLOWER(*m));

  /* printf ("%s %d\n", insn, (hash % CGEN_ASM_HASH_SIZE)); */

  return hash % CGEN_ASM_HASH_SIZE;
}


/* Special check to ensure that instruction exists for given machine. */
int
ip2k_cgen_insn_supported (cd, insn)
     CGEN_CPU_DESC cd;
     const CGEN_INSN *insn;
{
  int machs = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_MACH);

  /* No mach attribute?  Assume it's supported for all machs.  */
  if (machs == 0)
    return 1;
  
  return ((machs & cd->machs) != 0);
}


/* -- asm.c */
/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int asm_hash_insn PARAMS ((const char *));
static int dis_hash_insn_p PARAMS ((const CGEN_INSN *));
static unsigned int dis_hash_insn PARAMS ((const char *, CGEN_INSN_INT));

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & ip2k_cgen_ifld_table[IP2K_##f]
#else
#define F(f) & ip2k_cgen_ifld_table[IP2K_/**/f]
#endif
static const CGEN_IFMT ifmt_empty = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_jmp = {
  16, 16, 0xe000, { { F (F_OP3) }, { F (F_ADDR16CJP) }, { 0 } }
};

static const CGEN_IFMT ifmt_sb = {
  16, 16, 0xf000, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_xorw_l = {
  16, 16, 0xff00, { { F (F_OP4) }, { F (F_OP4MID) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_loadl_a = {
  16, 16, 0xff00, { { F (F_OP4) }, { F (F_OP4MID) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_loadh_a = {
  16, 16, 0xff00, { { F (F_OP4) }, { F (F_OP4MID) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_addcfr_w = {
  16, 16, 0xfe00, { { F (F_OP6) }, { F (F_DIR) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_speed = {
  16, 16, 0xff00, { { F (F_OP8) }, { F (F_IMM8) }, { 0 } }
};

static const CGEN_IFMT ifmt_ireadi = {
  16, 16, 0xffff, { { F (F_OP6) }, { F (F_OP6_10LOW) }, { 0 } }
};

static const CGEN_IFMT ifmt_page = {
  16, 16, 0xfff8, { { F (F_OP6) }, { F (F_OP6_7LOW) }, { F (F_PAGE3) }, { 0 } }
};

static const CGEN_IFMT ifmt_reti = {
  16, 16, 0xfff8, { { F (F_OP6) }, { F (F_OP6_7LOW) }, { F (F_RETI3) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) IP2K_OPERAND_##op
#else
#define OPERAND(op) IP2K_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE ip2k_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* jmp $addr16cjp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ADDR16CJP), 0 } },
    & ifmt_jmp, { 0xe000 }
  },
/* call $addr16cjp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ADDR16CJP), 0 } },
    & ifmt_jmp, { 0xc000 }
  },
/* sb $fr,$bitno */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', OP (BITNO), 0 } },
    & ifmt_sb, { 0xb000 }
  },
/* snb $fr,$bitno */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', OP (BITNO), 0 } },
    & ifmt_sb, { 0xa000 }
  },
/* setb $fr,$bitno */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', OP (BITNO), 0 } },
    & ifmt_sb, { 0x9000 }
  },
/* clrb $fr,$bitno */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', OP (BITNO), 0 } },
    & ifmt_sb, { 0x8000 }
  },
/* xor W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7f00 }
  },
/* and W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7e00 }
  },
/* or W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7d00 }
  },
/* add W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7b00 }
  },
/* sub W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7a00 }
  },
/* cmp W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7900 }
  },
/* retw #$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7800 }
  },
/* cse W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7700 }
  },
/* csne W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7600 }
  },
/* push #$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7400 }
  },
/* muls W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7300 }
  },
/* mulu W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7200 }
  },
/* loadl #$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7100 }
  },
/* loadh #$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7000 }
  },
/* loadl $addr16l */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ADDR16L), 0 } },
    & ifmt_loadl_a, { 0x7100 }
  },
/* loadh $addr16h */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ADDR16H), 0 } },
    & ifmt_loadh_a, { 0x7000 }
  },
/* addc $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x5e00 }
  },
/* addc W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x5c00 }
  },
/* incsnz $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x5a00 }
  },
/* incsnz W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x5800 }
  },
/* muls W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x5400 }
  },
/* mulu W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x5000 }
  },
/* decsnz $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4e00 }
  },
/* decsnz W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4c00 }
  },
/* subc W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4800 }
  },
/* subc $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x4a00 }
  },
/* pop $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4600 }
  },
/* push $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4400 }
  },
/* cse W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4200 }
  },
/* csne W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x4000 }
  },
/* incsz $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3e00 }
  },
/* incsz W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3c00 }
  },
/* swap $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3a00 }
  },
/* swap W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3800 }
  },
/* rl $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3600 }
  },
/* rl W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3400 }
  },
/* rr $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3200 }
  },
/* rr W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x3000 }
  },
/* decsz $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2e00 }
  },
/* decsz W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2c00 }
  },
/* inc $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2a00 }
  },
/* inc W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2800 }
  },
/* not $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2600 }
  },
/* not W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2400 }
  },
/* test $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2200 }
  },
/* mov W,#$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', '#', OP (LIT8), 0 } },
    & ifmt_xorw_l, { 0x7c00 }
  },
/* mov $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x200 }
  },
/* mov W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x2000 }
  },
/* add $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x1e00 }
  },
/* add W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x1c00 }
  },
/* xor $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x1a00 }
  },
/* xor W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x1800 }
  },
/* and $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x1600 }
  },
/* and W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x1400 }
  },
/* or $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0x1200 }
  },
/* or W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x1000 }
  },
/* dec $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0xe00 }
  },
/* dec W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0xc00 }
  },
/* sub $fr,W */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), ',', 'W', 0 } },
    & ifmt_addcfr_w, { 0xa00 }
  },
/* sub W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x800 }
  },
/* clr $fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x600 }
  },
/* cmp W,$fr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', 'W', ',', OP (FR), 0 } },
    & ifmt_addcfr_w, { 0x400 }
  },
/* speed #$lit8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (LIT8), 0 } },
    & ifmt_speed, { 0x100 }
  },
/* ireadi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x1d }
  },
/* iwritei */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x1c }
  },
/* fread */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x1b }
  },
/* fwrite */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x1a }
  },
/* iread */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x19 }
  },
/* iwrite */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x18 }
  },
/* page $addr16p */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ADDR16P), 0 } },
    & ifmt_page, { 0x10 }
  },
/* system */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0xff }
  },
/* reti #$reti3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '#', OP (RETI3), 0 } },
    & ifmt_reti, { 0x8 }
  },
/* ret */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x7 }
  },
/* int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x6 }
  },
/* breakx */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x5 }
  },
/* cwdt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x4 }
  },
/* ferase */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x3 }
  },
/* retnp */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x2 }
  },
/* break */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x1 }
  },
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ireadi, { 0x0 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & ip2k_cgen_ifld_table[IP2K_##f]
#else
#define F(f) & ip2k_cgen_ifld_table[IP2K_/**/f]
#endif
static const CGEN_IFMT ifmt_sc = {
  16, 16, 0xffff, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_snc = {
  16, 16, 0xffff, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_sz = {
  16, 16, 0xffff, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_snz = {
  16, 16, 0xffff, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_skip = {
  16, 16, 0xffff, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

static const CGEN_IFMT ifmt_skipb = {
  16, 16, 0xffff, { { F (F_OP4) }, { F (F_BITNO) }, { F (F_REG) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) IP2K_OPERAND_##op
#else
#define OPERAND(op) IP2K_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE ip2k_cgen_macro_insn_table[] =
{
/* sc */
  {
    -1, "sc", "sc", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* snc */
  {
    -1, "snc", "snc", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* sz */
  {
    -1, "sz", "sz", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* snz */
  {
    -1, "snz", "snz", 16,
    { 0|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* skip */
  {
    -1, "skip", "skip", 16,
    { 0|A(SKIPA)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* skip */
  {
    -1, "skipb", "skip", 16,
    { 0|A(SKIPA)|A(ALIAS), { (1<<MACH_BASE) } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE ip2k_cgen_macro_insn_opcode_table[] =
{
/* sc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_sc, { 0xb00b }
  },
/* snc */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_snc, { 0xa00b }
  },
/* sz */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_sz, { 0xb40b }
  },
/* snz */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_snz, { 0xa40b }
  },
/* skip */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_skip, { 0xa009 }
  },
/* skip */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_skipb, { 0xb009 }
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
ip2k_cgen_init_opcode_table (cd)
     CGEN_CPU_DESC cd;
{
  int i;
  int num_macros = (sizeof (ip2k_cgen_macro_insn_table) /
		    sizeof (ip2k_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & ip2k_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & ip2k_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (num_macros * sizeof (CGEN_INSN));
  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      ip2k_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & ip2k_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      ip2k_cgen_build_insn_regex (& insns[i]);
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
