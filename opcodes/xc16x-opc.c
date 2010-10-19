/* Instruction opcode table for xc16x.

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
#include "xc16x-desc.h"
#include "xc16x-opc.h"
#include "libiberty.h"

/* -- opc.c */
                                                                                
/* -- */
/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p        (const CGEN_INSN *);
static unsigned int asm_hash_insn (const char *);
static int dis_hash_insn_p        (const CGEN_INSN *);
static unsigned int dis_hash_insn (const char *, CGEN_INSN_INT);

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & xc16x_cgen_ifld_table[XC16X_##f]
#else
#define F(f) & xc16x_cgen_ifld_table[XC16X_/**/f]
#endif
static const CGEN_IFMT ifmt_empty ATTRIBUTE_UNUSED = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_addrpof ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addbrpof ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REGB8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addrpag ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_UIMM16) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addbrpag ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_UIMM16) }, { F (F_REGB8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addrhpof ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_UIMM16) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addrhpof3 ATTRIBUTE_UNUSED = {
  16, 16, 0x8ff, { { F (F_R1) }, { F (F_OP_BIT1) }, { F (F_UIMM3) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addbrhpag3 ATTRIBUTE_UNUSED = {
  16, 16, 0x8ff, { { F (F_R1) }, { F (F_OP_BIT1) }, { F (F_UIMM3) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addrbhpof ATTRIBUTE_UNUSED = {
  32, 32, 0xff0000ff, { { F (F_OP_BIT8) }, { F (F_UIMM8) }, { F (F_REGB8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addr ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_R1) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addbr ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_R1) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_add2 ATTRIBUTE_UNUSED = {
  16, 16, 0xcff, { { F (F_R1) }, { F (F_OP_BIT2) }, { F (F_R0) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addb2 ATTRIBUTE_UNUSED = {
  16, 16, 0xcff, { { F (F_R1) }, { F (F_OP_BIT2) }, { F (F_R0) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addrm2 ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMGR8) }, { F (F_REGMEM8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addrm ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addbrm2 ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMGR8) }, { F (F_REGMEM8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_addbrm ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REGB8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_muls ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_R1) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_div ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_cpl ATTRIBUTE_UNUSED = {
  16, 16, 0xfff, { { F (F_R1) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_cplb ATTRIBUTE_UNUSED = {
  16, 16, 0xfff, { { F (F_R1) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movri ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_UIMM4) }, { F (F_R4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movbri ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_UIMM4) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movbr2 ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_R1) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_mov9i ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_UIMM16) }, { F (F_R1) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movb9i ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_UIMM16) }, { F (F_R1) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movri11 ATTRIBUTE_UNUSED = {
  32, 32, 0xf0ff, { { F (F_MEMORY) }, { F (F_OP_LBIT4) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movehm5 ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REGOFF8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movehm6 ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_UIMM16) }, { F (F_REGOFF8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movehm7 ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_OFFSET16) }, { F (F_REGOFF8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movehm8 ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_OFFSET16) }, { F (F_REGOFF8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movehm10 ATTRIBUTE_UNUSED = {
  32, 32, 0xff0000ff, { { F (F_OP_BIT8) }, { F (F_UIMM8) }, { F (F_REGOFF8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movbsrpofm ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REGMEM8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_movbspofmr ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_MEMORY) }, { F (F_REGMEM8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpa0 ATTRIBUTE_UNUSED = {
  32, 32, 0x4ff, { { F (F_OFFSET16) }, { F (F_EXTCCODE) }, { F (F_OP_BITONE) }, { F (F_OP_ONEBIT) }, { F (F_OP_1BIT) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpa_ ATTRIBUTE_UNUSED = {
  32, 32, 0x5ff, { { F (F_OFFSET16) }, { F (F_EXTCCODE) }, { F (F_OP_BITONE) }, { F (F_OP_ONEBIT) }, { F (F_OP_1BIT) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpi ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_ICONDCODE) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpr_nenz ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_REL8) }, { F (F_RCOND) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpseg ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_OFFSET16) }, { F (F_SEG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmps ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_OFFSET16) }, { F (F_SEG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_jb ATTRIBUTE_UNUSED = {
  32, 32, 0xf0000ff, { { F (F_QLOBIT) }, { F (F_QHIBIT) }, { F (F_RELHI8) }, { F (F_REGB8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_calla0 ATTRIBUTE_UNUSED = {
  32, 32, 0x6ff, { { F (F_OFFSET16) }, { F (F_EXTCCODE) }, { F (F_OP_2BIT) }, { F (F_OP_1BIT) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_calla_ ATTRIBUTE_UNUSED = {
  32, 32, 0x7ff, { { F (F_OFFSET16) }, { F (F_EXTCCODE) }, { F (F_OP_BIT3) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_callr ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_REL8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_callseg ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_OFFSET16) }, { F (F_SEG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_pcall ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_OFFSET16) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_trap ATTRIBUTE_UNUSED = {
  16, 16, 0x1ff, { { F (F_UIMM7) }, { F (F_OP_1BIT) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_ret ATTRIBUTE_UNUSED = {
  16, 16, 0xff0000ff, { { F (F_OP_BIT8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_retp ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_reti ATTRIBUTE_UNUSED = {
  16, 16, 0xffff, { { F (F_OP_LBIT4) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_srstm ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_OP_BIT8) }, { F (F_DATA8) }, { F (F_OP_LBIT4) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_atomic ATTRIBUTE_UNUSED = {
  16, 16, 0xcfff, { { F (F_OP_LBIT2) }, { F (F_UIMM2) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_extp ATTRIBUTE_UNUSED = {
  16, 16, 0xc0ff, { { F (F_OP_LBIT2) }, { F (F_UIMM2) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_extp1 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00cfff, { { F (F_QLOBIT) }, { F (F_QLOBIT2) }, { F (F_PAGENUM) }, { F (F_OP_LBIT2) }, { F (F_UIMM2) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_extpg1 ATTRIBUTE_UNUSED = {
  32, 32, 0xcfff, { { F (F_UIMM16) }, { F (F_OP_LBIT2) }, { F (F_UIMM2) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_exts1 ATTRIBUTE_UNUSED = {
  32, 32, 0xff00cfff, { { F (F_OP_BIT8) }, { F (F_SEGNUM8) }, { F (F_OP_LBIT2) }, { F (F_UIMM2) }, { F (F_OP_BIT4) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_bclr18 ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_bclr0 ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_REG8) }, { F (F_QCOND) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_bmov ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_QLOBIT) }, { F (F_QHIBIT) }, { F (F_REGHI8) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_bfldl ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_DATAHI8) }, { F (F_MASK8) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_bfldh ATTRIBUTE_UNUSED = {
  32, 32, 0xff, { { F (F_DATAHI8) }, { F (F_DATA8) }, { F (F_REG8) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmpri ATTRIBUTE_UNUSED = {
  16, 16, 0x8ff, { { F (F_R1) }, { F (F_OP_BIT1) }, { F (F_UIMM3) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmpd1ri ATTRIBUTE_UNUSED = {
  16, 16, 0xff, { { F (F_UIMM4) }, { F (F_R2) }, { F (F_OP1) }, { F (F_OP2) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) XC16X_OPERAND_##op
#else
#define OPERAND(op) XC16X_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE xc16x_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* add $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x2 }
  },
/* sub $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x22 }
  },
/* addb $regb8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x3 }
  },
/* subb $regb8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x23 }
  },
/* add $reg8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addrpag, { 0x2 }
  },
/* sub $reg8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addrpag, { 0x22 }
  },
/* addb $regb8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addbrpag, { 0x3 }
  },
/* subb $regb8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addbrpag, { 0x23 }
  },
/* addc $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x12 }
  },
/* subc $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x32 }
  },
/* addcb $regb8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x13 }
  },
/* subcb $regb8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x33 }
  },
/* addc $reg8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addrpag, { 0x12 }
  },
/* subc $reg8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addrpag, { 0x32 }
  },
/* addcb $regb8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addbrpag, { 0x13 }
  },
/* subcb $regb8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addbrpag, { 0x33 }
  },
/* add $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x4 }
  },
/* sub $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x24 }
  },
/* addb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x5 }
  },
/* subb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x25 }
  },
/* addc $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x14 }
  },
/* subc $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x34 }
  },
/* addcb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x15 }
  },
/* subcb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x35 }
  },
/* add $reg8,$hash$pof$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (POF), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x6 }
  },
/* sub $reg8,$hash$pof$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (POF), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x26 }
  },
/* add $reg8,$hash$pag$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (PAG), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x6 }
  },
/* sub $reg8,$hash$pag$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (PAG), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x26 }
  },
/* add $dr,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x8 }
  },
/* sub $dr,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x28 }
  },
/* addb $drb,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x9 }
  },
/* subb $drb,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x29 }
  },
/* add $dr,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x8 }
  },
/* sub $dr,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x28 }
  },
/* addb $drb,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x9 }
  },
/* subb $drb,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x29 }
  },
/* addb $regb8,$hash$pof$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (POF), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x7 }
  },
/* subb $regb8,$hash$pof$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (POF), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x27 }
  },
/* addb $regb8,$hash$pag$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (PAG), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x7 }
  },
/* subb $regb8,$hash$pag$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (PAG), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x27 }
  },
/* addc $reg8,$hash$pof$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (POF), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x16 }
  },
/* subc $reg8,$hash$pof$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (POF), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x36 }
  },
/* addc $reg8,$hash$pag$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (PAG), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x16 }
  },
/* subc $reg8,$hash$pag$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (PAG), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x36 }
  },
/* addc $dr,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x18 }
  },
/* subc $dr,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x38 }
  },
/* addcb $drb,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x19 }
  },
/* subcb $drb,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x39 }
  },
/* addc $dr,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x18 }
  },
/* subc $dr,$hash$pag$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (PAG), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x38 }
  },
/* addcb $drb,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x19 }
  },
/* subcb $drb,$hash$pof$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (POF), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x39 }
  },
/* addcb $regb8,$hash$pof$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (POF), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x17 }
  },
/* subcb $regb8,$hash$pof$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (POF), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x37 }
  },
/* addcb $regb8,$hash$pag$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (PAG), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x17 }
  },
/* subcb $regb8,$hash$pag$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (PAG), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x37 }
  },
/* add $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x8 }
  },
/* sub $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x28 }
  },
/* addb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x9 }
  },
/* subb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x29 }
  },
/* add $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x6 }
  },
/* sub $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x26 }
  },
/* addb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x7 }
  },
/* subb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x27 }
  },
/* addc $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x18 }
  },
/* subc $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x38 }
  },
/* addcb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x19 }
  },
/* subcb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x39 }
  },
/* addc $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x16 }
  },
/* subc $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x36 }
  },
/* addcb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x17 }
  },
/* subcb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x37 }
  },
/* add $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x0 }
  },
/* sub $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x20 }
  },
/* addb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x1 }
  },
/* subb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x21 }
  },
/* add $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x808 }
  },
/* sub $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x828 }
  },
/* addb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x809 }
  },
/* subb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x829 }
  },
/* add $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc08 }
  },
/* sub $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc28 }
  },
/* addb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc09 }
  },
/* subb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc29 }
  },
/* addc $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x10 }
  },
/* subc $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x30 }
  },
/* addcb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x11 }
  },
/* subcb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x31 }
  },
/* addc $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x818 }
  },
/* subc $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x838 }
  },
/* addcb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x819 }
  },
/* subcb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x839 }
  },
/* addc $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc18 }
  },
/* subc $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc38 }
  },
/* addcb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc19 }
  },
/* subcb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc39 }
  },
/* add $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x2 }
  },
/* add $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x4 }
  },
/* add $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x2 }
  },
/* add $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x4 }
  },
/* sub $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x22 }
  },
/* sub $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x24 }
  },
/* sub $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x22 }
  },
/* sub $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x24 }
  },
/* addb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x3 }
  },
/* addb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x5 }
  },
/* addb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x3 }
  },
/* addb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x5 }
  },
/* subb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x23 }
  },
/* subb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x25 }
  },
/* subb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x23 }
  },
/* subb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x25 }
  },
/* addc $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x12 }
  },
/* addc $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x14 }
  },
/* addc $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x12 }
  },
/* addc $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x14 }
  },
/* subc $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x32 }
  },
/* subc $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x34 }
  },
/* subc $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x32 }
  },
/* subc $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x34 }
  },
/* addcb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x13 }
  },
/* addcb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x15 }
  },
/* addcb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x13 }
  },
/* addcb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x15 }
  },
/* subcb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x33 }
  },
/* subcb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x35 }
  },
/* subcb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x33 }
  },
/* subcb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x35 }
  },
/* mul $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_muls, { 0xb }
  },
/* mulu $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_muls, { 0x1b }
  },
/* div $srdiv */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRDIV), 0 } },
    & ifmt_div, { 0x4b }
  },
/* divl $srdiv */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRDIV), 0 } },
    & ifmt_div, { 0x6b }
  },
/* divlu $srdiv */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRDIV), 0 } },
    & ifmt_div, { 0x7b }
  },
/* divu $srdiv */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRDIV), 0 } },
    & ifmt_div, { 0x5b }
  },
/* cpl $dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), 0 } },
    & ifmt_cpl, { 0x91 }
  },
/* cplb $drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), 0 } },
    & ifmt_cplb, { 0xb1 }
  },
/* neg $dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), 0 } },
    & ifmt_cpl, { 0x81 }
  },
/* negb $drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), 0 } },
    & ifmt_cplb, { 0xa1 }
  },
/* and $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x60 }
  },
/* or $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x70 }
  },
/* xor $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x50 }
  },
/* andb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x61 }
  },
/* orb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x71 }
  },
/* xorb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x51 }
  },
/* and $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x68 }
  },
/* or $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x78 }
  },
/* xor $dr,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addrhpof3, { 0x58 }
  },
/* andb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x69 }
  },
/* orb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x79 }
  },
/* xorb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x59 }
  },
/* and $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x66 }
  },
/* or $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x76 }
  },
/* xor $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x56 }
  },
/* andb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x67 }
  },
/* orb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x77 }
  },
/* xorb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x57 }
  },
/* and $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x868 }
  },
/* or $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x878 }
  },
/* xor $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x858 }
  },
/* andb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x869 }
  },
/* orb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x879 }
  },
/* xorb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x859 }
  },
/* and $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc68 }
  },
/* or $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc78 }
  },
/* xor $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc58 }
  },
/* andb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc69 }
  },
/* orb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc79 }
  },
/* xorb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc59 }
  },
/* and $pof$reg8,$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (REG8), ',', OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x62 }
  },
/* or $pof$reg8,$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (REG8), ',', OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x72 }
  },
/* xor $pof$reg8,$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (REG8), ',', OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x52 }
  },
/* andb $pof$regb8,$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (REGB8), ',', OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x63 }
  },
/* orb $pof$regb8,$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (REGB8), ',', OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x73 }
  },
/* xorb $pof$regb8,$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (REGB8), ',', OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x53 }
  },
/* and $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x64 }
  },
/* or $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x74 }
  },
/* xor $pof$upof16,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REG8), 0 } },
    & ifmt_addrpof, { 0x54 }
  },
/* andb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x65 }
  },
/* orb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x75 }
  },
/* xorb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0x55 }
  },
/* and $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x62 }
  },
/* and $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x64 }
  },
/* and $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x62 }
  },
/* and $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x64 }
  },
/* or $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x72 }
  },
/* or $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x74 }
  },
/* or $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x72 }
  },
/* or $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x74 }
  },
/* xor $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x52 }
  },
/* xor $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0x54 }
  },
/* xor $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x52 }
  },
/* xor $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0x54 }
  },
/* andb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x63 }
  },
/* andb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x65 }
  },
/* andb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x63 }
  },
/* andb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x65 }
  },
/* orb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x73 }
  },
/* orb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x75 }
  },
/* orb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x73 }
  },
/* orb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x75 }
  },
/* xorb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x53 }
  },
/* xorb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0x55 }
  },
/* xorb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x53 }
  },
/* xorb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0x55 }
  },
/* mov $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0xf0 }
  },
/* movb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0xf1 }
  },
/* mov $dri,$hash$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRI), ',', OP (HASH), OP (U4), 0 } },
    & ifmt_movri, { 0xe0 }
  },
/* movb $srb,$hash$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRB), ',', OP (HASH), OP (U4), 0 } },
    & ifmt_movbri, { 0xe1 }
  },
/* mov $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0xe6 }
  },
/* movb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0xe7 }
  },
/* mov $dr,[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR), ']', 0 } },
    & ifmt_addr, { 0xa8 }
  },
/* movb $drb,[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR), ']', 0 } },
    & ifmt_movbr2, { 0xa9 }
  },
/* mov [$sr],$dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (SR), ']', ',', OP (DR), 0 } },
    & ifmt_addr, { 0xb8 }
  },
/* movb [$sr],$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (SR), ']', ',', OP (DRB), 0 } },
    & ifmt_movbr2, { 0xb9 }
  },
/* mov [-$sr],$dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', '-', OP (SR), ']', ',', OP (DR), 0 } },
    & ifmt_addr, { 0x88 }
  },
/* movb [-$sr],$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', '-', OP (SR), ']', ',', OP (DRB), 0 } },
    & ifmt_movbr2, { 0x89 }
  },
/* mov $dr,[$sr+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR), '+', ']', 0 } },
    & ifmt_addr, { 0x98 }
  },
/* movb $drb,[$sr+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR), '+', ']', 0 } },
    & ifmt_movbr2, { 0x99 }
  },
/* mov [$dr],[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (DR), ']', ',', '[', OP (SR), ']', 0 } },
    & ifmt_addr, { 0xc8 }
  },
/* movb [$dr],[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (DR), ']', ',', '[', OP (SR), ']', 0 } },
    & ifmt_addr, { 0xc9 }
  },
/* mov [$dr+],[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (DR), '+', ']', ',', '[', OP (SR), ']', 0 } },
    & ifmt_addr, { 0xd8 }
  },
/* movb [$dr+],[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (DR), '+', ']', ',', '[', OP (SR), ']', 0 } },
    & ifmt_addr, { 0xd9 }
  },
/* mov [$dr],[$sr+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (DR), ']', ',', '[', OP (SR), '+', ']', 0 } },
    & ifmt_addr, { 0xe8 }
  },
/* movb [$dr],[$sr+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (DR), ']', ',', '[', OP (SR), '+', ']', 0 } },
    & ifmt_addr, { 0xe9 }
  },
/* mov $dr,[$sr+$hash$uimm16] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR), '+', OP (HASH), OP (UIMM16), ']', 0 } },
    & ifmt_mov9i, { 0xd4 }
  },
/* movb $drb,[$sr+$hash$uimm16] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR), '+', OP (HASH), OP (UIMM16), ']', 0 } },
    & ifmt_movb9i, { 0xf4 }
  },
/* mov [$sr+$hash$uimm16],$dr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (SR), '+', OP (HASH), OP (UIMM16), ']', ',', OP (DR), 0 } },
    & ifmt_mov9i, { 0xc4 }
  },
/* movb [$sr+$hash$uimm16],$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (SR), '+', OP (HASH), OP (UIMM16), ']', ',', OP (DRB), 0 } },
    & ifmt_movb9i, { 0xe4 }
  },
/* mov [$src2],$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (SRC2), ']', ',', OP (MEMORY), 0 } },
    & ifmt_movri11, { 0x84 }
  },
/* movb [$src2],$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', '[', OP (SRC2), ']', ',', OP (MEMORY), 0 } },
    & ifmt_movri11, { 0xa4 }
  },
/* mov $memory,[$src2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', '[', OP (SRC2), ']', 0 } },
    & ifmt_movri11, { 0x94 }
  },
/* movb $memory,[$src2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', '[', OP (SRC2), ']', 0 } },
    & ifmt_movri11, { 0xb4 }
  },
/* mov $regoff8,$hash$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (HASH), OP (POF), OP (UPOF16), 0 } },
    & ifmt_movehm5, { 0xe6 }
  },
/* mov $regoff8,$hash$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (HASH), OP (PAG), OP (UPAG16), 0 } },
    & ifmt_movehm6, { 0xe6 }
  },
/* mov $regoff8,$hash$segm$useg16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (HASH), OP (SEGM), OP (USEG16), 0 } },
    & ifmt_movehm7, { 0xe6 }
  },
/* mov $regoff8,$hash$sof$usof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (HASH), OP (SOF), OP (USOF16), 0 } },
    & ifmt_movehm8, { 0xe6 }
  },
/* movb $regb8,$hash$pof$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (POF), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0xe7 }
  },
/* movb $regoff8,$hash$pag$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (HASH), OP (PAG), OP (UIMM8), 0 } },
    & ifmt_movehm10, { 0xe7 }
  },
/* mov $regoff8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_movehm5, { 0xf2 }
  },
/* movb $regb8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0xf3 }
  },
/* mov $regoff8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGOFF8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_movehm6, { 0xf2 }
  },
/* movb $regb8,$pag$upag16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (PAG), OP (UPAG16), 0 } },
    & ifmt_addbrpag, { 0xf3 }
  },
/* mov $pof$upof16,$regoff8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGOFF8), 0 } },
    & ifmt_movehm5, { 0xf6 }
  },
/* movb $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0xf7 }
  },
/* mov $dri,$hash$pof$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRI), ',', OP (HASH), OP (POF), OP (U4), 0 } },
    & ifmt_movri, { 0xe0 }
  },
/* movb $srb,$hash$pof$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRB), ',', OP (HASH), OP (POF), OP (U4), 0 } },
    & ifmt_movbri, { 0xe1 }
  },
/* mov $dri,$hash$pag$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRI), ',', OP (HASH), OP (PAG), OP (U4), 0 } },
    & ifmt_movri, { 0xe0 }
  },
/* movb $srb,$hash$pag$u4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRB), ',', OP (HASH), OP (PAG), OP (U4), 0 } },
    & ifmt_movbri, { 0xe1 }
  },
/* mov $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0xf2 }
  },
/* mov $memgr8,$regmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGMEM8), 0 } },
    & ifmt_addrm2, { 0xf6 }
  },
/* mov $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0xf2 }
  },
/* mov $memory,$reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REG8), 0 } },
    & ifmt_addrm, { 0xf6 }
  },
/* movb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0xf3 }
  },
/* movb $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0xf7 }
  },
/* movb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0xf3 }
  },
/* movb $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0xf7 }
  },
/* movbs $sr,$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (DRB), 0 } },
    & ifmt_movbr2, { 0xd0 }
  },
/* movbz $sr,$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (DRB), 0 } },
    & ifmt_movbr2, { 0xc0 }
  },
/* movbs $regmem8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_movbsrpofm, { 0xd2 }
  },
/* movbs $pof$upof16,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGBMEM8), 0 } },
    & ifmt_movbspofmr, { 0xd5 }
  },
/* movbz $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0xc2 }
  },
/* movbz $pof$upof16,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (POF), OP (UPOF16), ',', OP (REGB8), 0 } },
    & ifmt_addbrpof, { 0xc5 }
  },
/* movbs $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0xd2 }
  },
/* movbs $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0xd5 }
  },
/* movbs $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0xd2 }
  },
/* movbs $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0xd5 }
  },
/* movbz $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0xc2 }
  },
/* movbz $memgr8,$regbmem8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMGR8), ',', OP (REGBMEM8), 0 } },
    & ifmt_addbrm2, { 0xc5 }
  },
/* movbz $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0xc2 }
  },
/* movbz $memory,$regb8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (MEMORY), ',', OP (REGB8), 0 } },
    & ifmt_addbrm, { 0xc5 }
  },
/* movbs $sr,$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (DRB), 0 } },
    & ifmt_movbr2, { 0xd0 }
  },
/* movbz $sr,$drb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (DRB), 0 } },
    & ifmt_movbr2, { 0xc0 }
  },
/* jmpa+ $extcond,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (EXTCOND), ',', OP (CADDR), 0 } },
    & ifmt_jmpa0, { 0xea }
  },
/* jmpa $extcond,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (EXTCOND), ',', OP (CADDR), 0 } },
    & ifmt_jmpa0, { 0xea }
  },
/* jmpa- $extcond,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (EXTCOND), ',', OP (CADDR), 0 } },
    & ifmt_jmpa_, { 0x1ea }
  },
/* jmpi $icond,[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ICOND), ',', '[', OP (SR), ']', 0 } },
    & ifmt_jmpi, { 0x9c }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x3d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xad }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x2d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x4d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x5d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x6d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x7d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x8d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x9d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x2d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x3d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x8d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xfd }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x9d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xed }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xbd }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xdd }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0x1d }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xd }
  },
/* jmpr $cond,$rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (COND), ',', OP (REL), 0 } },
    & ifmt_jmpr_nenz, { 0xcd }
  },
/* jmps $hash$segm$useg8,$hash$sof$usof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (SEGM), OP (USEG8), ',', OP (HASH), OP (SOF), OP (USOF16), 0 } },
    & ifmt_jmpseg, { 0xfa }
  },
/* jmps $seg,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SEG), ',', OP (CADDR), 0 } },
    & ifmt_jmps, { 0xfa }
  },
/* jb $genreg$dot$qlobit,$relhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (GENREG), OP (DOT), OP (QLOBIT), ',', OP (RELHI), 0 } },
    & ifmt_jb, { 0x8a }
  },
/* jbc $genreg$dot$qlobit,$relhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (GENREG), OP (DOT), OP (QLOBIT), ',', OP (RELHI), 0 } },
    & ifmt_jb, { 0xaa }
  },
/* jnb $genreg$dot$qlobit,$relhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (GENREG), OP (DOT), OP (QLOBIT), ',', OP (RELHI), 0 } },
    & ifmt_jb, { 0x9a }
  },
/* jnbs $genreg$dot$qlobit,$relhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (GENREG), OP (DOT), OP (QLOBIT), ',', OP (RELHI), 0 } },
    & ifmt_jb, { 0xba }
  },
/* calla+ $extcond,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (EXTCOND), ',', OP (CADDR), 0 } },
    & ifmt_calla0, { 0xca }
  },
/* calla $extcond,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (EXTCOND), ',', OP (CADDR), 0 } },
    & ifmt_calla0, { 0xca }
  },
/* calla- $extcond,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (EXTCOND), ',', OP (CADDR), 0 } },
    & ifmt_calla_, { 0x1ca }
  },
/* calli $icond,[$sr] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (ICOND), ',', '[', OP (SR), ']', 0 } },
    & ifmt_jmpi, { 0xab }
  },
/* callr $rel */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REL), 0 } },
    & ifmt_callr, { 0xbb }
  },
/* calls $hash$segm$useg8,$hash$sof$usof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (SEGM), OP (USEG8), ',', OP (HASH), OP (SOF), OP (USOF16), 0 } },
    & ifmt_callseg, { 0xda }
  },
/* calls $seg,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SEG), ',', OP (CADDR), 0 } },
    & ifmt_jmps, { 0xda }
  },
/* pcall $reg8,$caddr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (CADDR), 0 } },
    & ifmt_pcall, { 0xe2 }
  },
/* trap $hash$uimm7 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (UIMM7), 0 } },
    & ifmt_trap, { 0x9b }
  },
/* ret */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ret, { 0xcb }
  },
/* rets */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ret, { 0xdb }
  },
/* retp $reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), 0 } },
    & ifmt_retp, { 0xeb }
  },
/* reti */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_reti, { 0x88fb }
  },
/* pop $reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), 0 } },
    & ifmt_retp, { 0xfc }
  },
/* push $reg8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), 0 } },
    & ifmt_retp, { 0xec }
  },
/* scxt $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0xc6 }
  },
/* scxt $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0xd6 }
  },
/* scxt $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0xd6 }
  },
/* scxt $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0xd6 }
  },
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ret, { 0xcc }
  },
/* srst */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0xb7b748b7 }
  },
/* idle */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0x87877887 }
  },
/* pwrdn */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0x97976897 }
  },
/* diswdt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0xa5a55aa5 }
  },
/* enwdt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0x85857a85 }
  },
/* einit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0xb5b54ab5 }
  },
/* srvwdt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_srstm, { 0xa7a758a7 }
  },
/* sbrk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_ret, { 0x8c }
  },
/* atomic $hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_atomic, { 0xd1 }
  },
/* extr $hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_atomic, { 0x80d1 }
  },
/* extp $sr,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extp, { 0x40dc }
  },
/* extp $hash$pagenum,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (PAGENUM), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extp1, { 0x40d7 }
  },
/* extp $hash$pag$upag16,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (PAG), OP (UPAG16), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extpg1, { 0x40d7 }
  },
/* extpr $sr,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extp, { 0xc0dc }
  },
/* extpr $hash$pagenum,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (PAGENUM), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extp1, { 0xc0d7 }
  },
/* exts $sr,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extp, { 0xdc }
  },
/* exts $hash$seghi8,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (SEGHI8), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_exts1, { 0xd7 }
  },
/* extsr $sr,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_extp, { 0x80dc }
  },
/* extsr $hash$seghi8,$hash$uimm2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (HASH), OP (SEGHI8), ',', OP (HASH), OP (UIMM2), 0 } },
    & ifmt_exts1, { 0x80d7 }
  },
/* prior $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x2b }
  },
/* bclr $RegNam */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGNAM), 0 } },
    & ifmt_bclr18, { 0xbe }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xe }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x1e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x2e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x3e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x4e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x5e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x6e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x7e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x8e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x9e }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xae }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xbe }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xce }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xde }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xee }
  },
/* bclr $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xfe }
  },
/* bset $RegNam */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGNAM), 0 } },
    & ifmt_bclr18, { 0xbf }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xf }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x1f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x2f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x3f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x4f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x5f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x6f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x7f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x8f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0x9f }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xaf }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xbf }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xcf }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xdf }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xef }
  },
/* bset $reg8$dot$qbit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), OP (DOT), OP (QBIT), 0 } },
    & ifmt_bclr0, { 0xff }
  },
/* bmov $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGHI8), OP (DOT), OP (QHIBIT), ',', OP (REG8), OP (DOT), OP (QLOBIT), 0 } },
    & ifmt_bmov, { 0x4a }
  },
/* bmovn $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGHI8), OP (DOT), OP (QHIBIT), ',', OP (REG8), OP (DOT), OP (QLOBIT), 0 } },
    & ifmt_bmov, { 0x3a }
  },
/* band $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGHI8), OP (DOT), OP (QHIBIT), ',', OP (REG8), OP (DOT), OP (QLOBIT), 0 } },
    & ifmt_bmov, { 0x6a }
  },
/* bor $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGHI8), OP (DOT), OP (QHIBIT), ',', OP (REG8), OP (DOT), OP (QLOBIT), 0 } },
    & ifmt_bmov, { 0x5a }
  },
/* bxor $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGHI8), OP (DOT), OP (QHIBIT), ',', OP (REG8), OP (DOT), OP (QLOBIT), 0 } },
    & ifmt_bmov, { 0x7a }
  },
/* bcmp $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGHI8), OP (DOT), OP (QHIBIT), ',', OP (REG8), OP (DOT), OP (QLOBIT), 0 } },
    & ifmt_bmov, { 0x2a }
  },
/* bfldl $reg8,$hash$mask8,$hash$datahi8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (MASK8), ',', OP (HASH), OP (DATAHI8), 0 } },
    & ifmt_bfldl, { 0xa }
  },
/* bfldh $reg8,$hash$masklo8,$hash$data8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (MASKLO8), ',', OP (HASH), OP (DATA8), 0 } },
    & ifmt_bfldh, { 0x1a }
  },
/* cmp $src1,$src2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (SRC2), 0 } },
    & ifmt_muls, { 0x40 }
  },
/* cmpb $drb,$srb */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (SRB), 0 } },
    & ifmt_addbr, { 0x41 }
  },
/* cmp $src1,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SRC1), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_cmpri, { 0x48 }
  },
/* cmpb $drb,$hash$uimm3 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', OP (HASH), OP (UIMM3), 0 } },
    & ifmt_addbrhpag3, { 0x49 }
  },
/* cmp $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x46 }
  },
/* cmpb $regb8,$hash$uimm8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (HASH), OP (UIMM8), 0 } },
    & ifmt_addrbhpof, { 0x47 }
  },
/* cmp $dr,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_add2, { 0x848 }
  },
/* cmpb $drb,[$sr2] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), ']', 0 } },
    & ifmt_addb2, { 0x849 }
  },
/* cmp $dr,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_add2, { 0xc48 }
  },
/* cmpb $drb,[$sr2+] */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DRB), ',', '[', OP (SR2), '+', ']', 0 } },
    & ifmt_addb2, { 0xc49 }
  },
/* cmp $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x42 }
  },
/* cmpb $regb8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addbrpof, { 0x43 }
  },
/* cmp $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x42 }
  },
/* cmp $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x42 }
  },
/* cmpb $regbmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGBMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addbrm2, { 0x43 }
  },
/* cmpb $regb8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGB8), ',', OP (MEMORY), 0 } },
    & ifmt_addbrm, { 0x43 }
  },
/* cmpd1 $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0xa0 }
  },
/* cmpd2 $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0xb0 }
  },
/* cmpi1 $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0x80 }
  },
/* cmpi2 $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0x90 }
  },
/* cmpd1 $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0xa6 }
  },
/* cmpd2 $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0xb6 }
  },
/* cmpi1 $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x86 }
  },
/* cmpi2 $reg8,$hash$uimm16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (HASH), OP (UIMM16), 0 } },
    & ifmt_addrhpof, { 0x96 }
  },
/* cmpd1 $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0xa2 }
  },
/* cmpd2 $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0xb2 }
  },
/* cmpi1 $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x82 }
  },
/* cmpi2 $reg8,$pof$upof16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (POF), OP (UPOF16), 0 } },
    & ifmt_addrpof, { 0x92 }
  },
/* cmpd1 $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0xa2 }
  },
/* cmpd2 $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0xb2 }
  },
/* cmpi1 $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x82 }
  },
/* cmpi2 $regmem8,$memgr8 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REGMEM8), ',', OP (MEMGR8), 0 } },
    & ifmt_addrm2, { 0x92 }
  },
/* cmpd1 $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0xa2 }
  },
/* cmpd2 $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0xb2 }
  },
/* cmpi1 $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x82 }
  },
/* cmpi2 $reg8,$memory */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (REG8), ',', OP (MEMORY), 0 } },
    & ifmt_addrm, { 0x92 }
  },
/* shl $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x4c }
  },
/* shr $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x6c }
  },
/* rol $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0xc }
  },
/* ror $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0x2c }
  },
/* ashr $dr,$sr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (DR), ',', OP (SR), 0 } },
    & ifmt_addr, { 0xac }
  },
/* shl $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0x5c }
  },
/* shr $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0x7c }
  },
/* rol $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0x1c }
  },
/* ror $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0x3c }
  },
/* ashr $sr,$hash$uimm4 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (SR), ',', OP (HASH), OP (UIMM4), 0 } },
    & ifmt_cmpd1ri, { 0xbc }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & xc16x_cgen_ifld_table[XC16X_##f]
#else
#define F(f) & xc16x_cgen_ifld_table[XC16X_/**/f]
#endif
#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) XC16X_OPERAND_##op
#else
#define OPERAND(op) XC16X_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE xc16x_cgen_macro_insn_table[] =
{
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE xc16x_cgen_macro_insn_opcode_table[] =
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
xc16x_cgen_init_opcode_table (CGEN_CPU_DESC cd)
{
  int i;
  int num_macros = (sizeof (xc16x_cgen_macro_insn_table) /
		    sizeof (xc16x_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & xc16x_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & xc16x_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = xmalloc (num_macros * sizeof (CGEN_INSN));

  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      xc16x_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & xc16x_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      xc16x_cgen_build_insn_regex (& insns[i]);
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
