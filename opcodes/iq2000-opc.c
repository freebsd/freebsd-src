/* Instruction opcode table for iq2000.

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
#include "iq2000-desc.h"
#include "iq2000-opc.h"
#include "libiberty.h"

/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p        (const CGEN_INSN *);
static unsigned int asm_hash_insn (const char *);
static int dis_hash_insn_p        (const CGEN_INSN *);
static unsigned int dis_hash_insn (const char *, CGEN_INSN_INT);

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & iq2000_cgen_ifld_table[IQ2000_##f]
#else
#define F(f) & iq2000_cgen_ifld_table[IQ2000_/**/f]
#endif
static const CGEN_IFMT ifmt_empty ATTRIBUTE_UNUSED = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_add2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RT) }, { F (F_RD_RS) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_add ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_addi2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RT_RS) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_addi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_ram ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000020, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_5) }, { F (F_MASKL) }, { 0 } }
};

static const CGEN_IFMT ifmt_sll ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0003f, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_sllv2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RD_RT) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_slmv2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00003f, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RD_RT) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_slmv ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00003f, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_slti2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RT_RS) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_slti ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_sra2 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0003f, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RD_RT) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_bbi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_OFFSET) }, { 0 } }
};

static const CGEN_IFMT ifmt_bbv ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_OFFSET) }, { 0 } }
};

static const CGEN_IFMT ifmt_bgez ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1f0000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_OFFSET) }, { 0 } }
};

static const CGEN_IFMT ifmt_jalr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1f07ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_jr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1fffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_lb ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_lui ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_break ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_syscall ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00003f, { { F (F_OPCODE) }, { F (F_EXCODE) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_andoui ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_andoui2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RT_RS) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrgb ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00043f, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_10) }, { F (F_MASK) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrgb2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00043f, { { F (F_OPCODE) }, { F (F_RT) }, { F (F_RD_RS) }, { F (F_10) }, { F (F_MASK) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_bc0f ATTRIBUTE_UNUSED = {
  32, 32, 0xffff0000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_OFFSET) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfc0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_10_11) }, { 0 } }
};

static const CGEN_IFMT ifmt_chkhdr ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_lulck ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_pkrlr1 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_COUNT) }, { F (F_INDEX) }, { 0 } }
};

static const CGEN_IFMT ifmt_rfe ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_OPCODE) }, { F (F_25) }, { F (F_24_19) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_j ATTRIBUTE_UNUSED = {
  32, 32, 0xffff0000, { { F (F_OPCODE) }, { F (F_RSRVD) }, { F (F_JTARG) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrgbq10 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00003f, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_MASKQ10) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrgbq102 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00003f, { { F (F_OPCODE) }, { F (F_RT) }, { F (F_RD_RS) }, { F (F_MASKQ10) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_jq10 ATTRIBUTE_UNUSED = {
  32, 32, 0xffff0000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_JTARG) }, { 0 } }
};

static const CGEN_IFMT ifmt_jalq10 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_JTARG) }, { 0 } }
};

static const CGEN_IFMT ifmt_avail ATTRIBUTE_UNUSED = {
  32, 32, 0xffff07ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_rbi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000700, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_cam36 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007c0, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP_10) }, { F (F_CAM_Z) }, { F (F_CAM_Y) }, { 0 } }
};

static const CGEN_IFMT ifmt_cm32and ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_cm32rd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_cm128ria3 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007fc, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_CM_4FUNC) }, { F (F_CM_3Z) }, { 0 } }
};

static const CGEN_IFMT ifmt_cm128ria4 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007f8, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_CM_3FUNC) }, { F (F_CM_4Z) }, { 0 } }
};

static const CGEN_IFMT ifmt_ctc ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) IQ2000_OPERAND_##op
#else
#define OPERAND(op) IQ2000_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE iq2000_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* add ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x20 }
  },
/* add $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x20 }
  },
/* addi ${rt-rs},$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (LO16), 0 } },
    & ifmt_addi2, { 0x20000000 }
  },
/* addi $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_addi, { 0x20000000 }
  },
/* addiu ${rt-rs},$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (LO16), 0 } },
    & ifmt_addi2, { 0x24000000 }
  },
/* addiu $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_addi, { 0x24000000 }
  },
/* addu ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x21 }
  },
/* addu $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x21 }
  },
/* ado16 ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x29 }
  },
/* ado16 $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x29 }
  },
/* and ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x24 }
  },
/* and $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x24 }
  },
/* andi ${rt-rs},$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (LO16), 0 } },
    & ifmt_addi2, { 0x30000000 }
  },
/* andi $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_addi, { 0x30000000 }
  },
/* andoi ${rt-rs},$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (LO16), 0 } },
    & ifmt_addi2, { 0xb0000000 }
  },
/* andoi $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_addi, { 0xb0000000 }
  },
/* nor ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x27 }
  },
/* nor $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x27 }
  },
/* or ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x25 }
  },
/* or $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x25 }
  },
/* ori ${rt-rs},$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (LO16), 0 } },
    & ifmt_addi2, { 0x34000000 }
  },
/* ori $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_addi, { 0x34000000 }
  },
/* ram $rd,$rt,$shamt,$maskl,$maskr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (SHAMT), ',', OP (MASKL), ',', OP (MASKR), 0 } },
    & ifmt_ram, { 0x9c000000 }
  },
/* sll $rd,$rt,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (SHAMT), 0 } },
    & ifmt_sll, { 0x0 }
  },
/* sllv ${rd-rt},$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RT), ',', OP (RS), 0 } },
    & ifmt_sllv2, { 0x4 }
  },
/* sllv $rd,$rt,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), 0 } },
    & ifmt_add, { 0x4 }
  },
/* slmv ${rd-rt},$rs,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RT), ',', OP (RS), ',', OP (SHAMT), 0 } },
    & ifmt_slmv2, { 0x1 }
  },
/* slmv $rd,$rt,$rs,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), ',', OP (SHAMT), 0 } },
    & ifmt_slmv, { 0x1 }
  },
/* slt ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x2a }
  },
/* slt $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x2a }
  },
/* slti ${rt-rs},$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (IMM), 0 } },
    & ifmt_slti2, { 0x28000000 }
  },
/* slti $rt,$rs,$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (IMM), 0 } },
    & ifmt_slti, { 0x28000000 }
  },
/* sltiu ${rt-rs},$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (IMM), 0 } },
    & ifmt_slti2, { 0x2c000000 }
  },
/* sltiu $rt,$rs,$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (IMM), 0 } },
    & ifmt_slti, { 0x2c000000 }
  },
/* sltu ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x2b }
  },
/* sltu $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x2b }
  },
/* sra ${rd-rt},$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RT), ',', OP (SHAMT), 0 } },
    & ifmt_sra2, { 0x3 }
  },
/* sra $rd,$rt,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (SHAMT), 0 } },
    & ifmt_sll, { 0x3 }
  },
/* srav ${rd-rt},$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RT), ',', OP (RS), 0 } },
    & ifmt_sllv2, { 0x7 }
  },
/* srav $rd,$rt,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), 0 } },
    & ifmt_add, { 0x7 }
  },
/* srl $rd,$rt,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (SHAMT), 0 } },
    & ifmt_sll, { 0x2 }
  },
/* srlv ${rd-rt},$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RT), ',', OP (RS), 0 } },
    & ifmt_sllv2, { 0x6 }
  },
/* srlv $rd,$rt,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), 0 } },
    & ifmt_add, { 0x6 }
  },
/* srmv ${rd-rt},$rs,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RT), ',', OP (RS), ',', OP (SHAMT), 0 } },
    & ifmt_slmv2, { 0x5 }
  },
/* srmv $rd,$rt,$rs,$shamt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), ',', OP (SHAMT), 0 } },
    & ifmt_slmv, { 0x5 }
  },
/* sub ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x22 }
  },
/* sub $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x22 }
  },
/* subu ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x23 }
  },
/* subu $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x23 }
  },
/* xor ${rd-rs},$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), 0 } },
    & ifmt_add2, { 0x26 }
  },
/* xor $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x26 }
  },
/* xori ${rt-rs},$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (LO16), 0 } },
    & ifmt_addi2, { 0x38000000 }
  },
/* xori $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_addi, { 0x38000000 }
  },
/* bbi $rs($bitnum),$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), '(', OP (BITNUM), ')', ',', OP (OFFSET), 0 } },
    & ifmt_bbi, { 0x70000000 }
  },
/* bbin $rs($bitnum),$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), '(', OP (BITNUM), ')', ',', OP (OFFSET), 0 } },
    & ifmt_bbi, { 0x78000000 }
  },
/* bbv $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x74000000 }
  },
/* bbvn $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x7c000000 }
  },
/* beq $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x10000000 }
  },
/* beql $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x50000000 }
  },
/* bgez $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4010000 }
  },
/* bgezal $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4110000 }
  },
/* bgezall $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4130000 }
  },
/* bgezl $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4030000 }
  },
/* bltz $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4000000 }
  },
/* bltzl $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4020000 }
  },
/* bltzal $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4100000 }
  },
/* bltzall $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4120000 }
  },
/* bmb0 $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x60000000 }
  },
/* bmb1 $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x64000000 }
  },
/* bmb2 $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x68000000 }
  },
/* bmb3 $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x6c000000 }
  },
/* bne $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x14000000 }
  },
/* bnel $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x54000000 }
  },
/* jalr $rd,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_jalr, { 0x9 }
  },
/* jr $rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), 0 } },
    & ifmt_jr, { 0x8 }
  },
/* lb $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0x80000000 }
  },
/* lbu $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0x90000000 }
  },
/* lh $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0x84000000 }
  },
/* lhu $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0x94000000 }
  },
/* lui $rt,$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (HI16), 0 } },
    & ifmt_lui, { 0x3c000000 }
  },
/* lw $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0x8c000000 }
  },
/* sb $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0xa0000000 }
  },
/* sh $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0xa4000000 }
  },
/* sw $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0xac000000 }
  },
/* break */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_break, { 0xd }
  },
/* syscall */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_syscall, { 0xc }
  },
/* andoui $rt,$rs,$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (HI16), 0 } },
    & ifmt_andoui, { 0xfc000000 }
  },
/* andoui ${rt-rs},$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (HI16), 0 } },
    & ifmt_andoui2, { 0xfc000000 }
  },
/* orui ${rt-rs},$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (HI16), 0 } },
    & ifmt_andoui2, { 0xbc000000 }
  },
/* orui $rt,$rs,$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (HI16), 0 } },
    & ifmt_andoui, { 0xbc000000 }
  },
/* bgtz $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x1c000000 }
  },
/* bgtzl $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x5c000000 }
  },
/* blez $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x18000000 }
  },
/* blezl $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x58000000 }
  },
/* mrgb $rd,$rs,$rt,$mask */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (MASK), 0 } },
    & ifmt_mrgb, { 0x2d }
  },
/* mrgb ${rd-rs},$rt,$mask */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), ',', OP (MASK), 0 } },
    & ifmt_mrgb2, { 0x2d }
  },
/* bctxt $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4060000 }
  },
/* bc0f $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x41000000 }
  },
/* bc0fl $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x41020000 }
  },
/* bc3f $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x4d000000 }
  },
/* bc3fl $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x4d020000 }
  },
/* bc0t $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x41010000 }
  },
/* bc0tl $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x41030000 }
  },
/* bc3t $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x4d010000 }
  },
/* bc3tl $offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (OFFSET), 0 } },
    & ifmt_bc0f, { 0x4d030000 }
  },
/* cfc0 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x40400000 }
  },
/* cfc1 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x44400000 }
  },
/* cfc2 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x48400000 }
  },
/* cfc3 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x4c400000 }
  },
/* chkhdr $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4d200000 }
  },
/* ctc0 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x40c00000 }
  },
/* ctc1 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x44c00000 }
  },
/* ctc2 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x48c00000 }
  },
/* ctc3 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x4cc00000 }
  },
/* jcr $rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), 0 } },
    & ifmt_jr, { 0xa }
  },
/* luc32 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200003 }
  },
/* luc32l $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200007 }
  },
/* luc64 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x4820000b }
  },
/* luc64l $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x4820000f }
  },
/* luk $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200008 }
  },
/* lulck $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_lulck, { 0x48200004 }
  },
/* lum32 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200002 }
  },
/* lum32l $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200006 }
  },
/* lum64 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x4820000a }
  },
/* lum64l $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x4820000e }
  },
/* lur $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200001 }
  },
/* lurl $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200005 }
  },
/* luulck $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_lulck, { 0x48200000 }
  },
/* mfc0 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x40000000 }
  },
/* mfc1 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x44000000 }
  },
/* mfc2 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x48000000 }
  },
/* mfc3 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x4c000000 }
  },
/* mtc0 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x40800000 }
  },
/* mtc1 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x44800000 }
  },
/* mtc2 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x48800000 }
  },
/* mtc3 $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_cfc0, { 0x4c800000 }
  },
/* pkrl $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200007 }
  },
/* pkrlr1 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4fa00000 }
  },
/* pkrlr30 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4fe00000 }
  },
/* rb $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200004 }
  },
/* rbr1 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4f000000 }
  },
/* rbr30 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4f400000 }
  },
/* rfe */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_rfe, { 0x42000010 }
  },
/* rx $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200006 }
  },
/* rxr1 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4f800000 }
  },
/* rxr30 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4fc00000 }
  },
/* sleep */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_syscall, { 0xe }
  },
/* srrd $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_lulck, { 0x48200010 }
  },
/* srrdl $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_lulck, { 0x48200014 }
  },
/* srulck $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_lulck, { 0x48200016 }
  },
/* srwr $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200011 }
  },
/* srwru $rt,$rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RD), 0 } },
    & ifmt_chkhdr, { 0x48200015 }
  },
/* trapqfl */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_break, { 0x4c200008 }
  },
/* trapqne */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_break, { 0x4c200009 }
  },
/* traprel $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_lulck, { 0x4c20000a }
  },
/* wb $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200000 }
  },
/* wbu $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200001 }
  },
/* wbr1 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4e000000 }
  },
/* wbr1u $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4e200000 }
  },
/* wbr30 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4e400000 }
  },
/* wbr30u $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4e600000 }
  },
/* wx $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200002 }
  },
/* wxu $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c200003 }
  },
/* wxr1 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4e800000 }
  },
/* wxr1u $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4ea00000 }
  },
/* wxr30 $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4ec00000 }
  },
/* wxr30u $rt,$_index,$count */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (_INDEX), ',', OP (COUNT), 0 } },
    & ifmt_pkrlr1, { 0x4ee00000 }
  },
/* ldw $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0xc0000000 }
  },
/* sdw $rt,$lo16($base) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), '(', OP (BASE), ')', 0 } },
    & ifmt_lb, { 0xe0000000 }
  },
/* j $jmptarg */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (JMPTARG), 0 } },
    & ifmt_j, { 0x8000000 }
  },
/* jal $jmptarg */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (JMPTARG), 0 } },
    & ifmt_j, { 0xc000000 }
  },
/* bmb $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0xb4000000 }
  },
/* andoui $rt,$rs,$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (HI16), 0 } },
    & ifmt_andoui, { 0xbc000000 }
  },
/* andoui ${rt-rs},$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (HI16), 0 } },
    & ifmt_andoui2, { 0xbc000000 }
  },
/* orui $rt,$rs,$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (HI16), 0 } },
    & ifmt_andoui, { 0x3c000000 }
  },
/* orui ${rt-rs},$hi16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT_RS), ',', OP (HI16), 0 } },
    & ifmt_andoui2, { 0x3c000000 }
  },
/* mrgb $rd,$rs,$rt,$maskq10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (MASKQ10), 0 } },
    & ifmt_mrgbq10, { 0x2d }
  },
/* mrgb ${rd-rs},$rt,$maskq10 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD_RS), ',', OP (RT), ',', OP (MASKQ10), 0 } },
    & ifmt_mrgbq102, { 0x2d }
  },
/* j $jmptarg */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (JMPTARG), 0 } },
    & ifmt_jq10, { 0x8000000 }
  },
/* jal $rt,$jmptarg */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (JMPTARG), 0 } },
    & ifmt_jalq10, { 0xc000000 }
  },
/* jal $jmptarg */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (JMPTARG), 0 } },
    & ifmt_jq10, { 0xc1f0000 }
  },
/* bbil $rs($bitnum),$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), '(', OP (BITNUM), ')', ',', OP (OFFSET), 0 } },
    & ifmt_bbi, { 0xf0000000 }
  },
/* bbinl $rs($bitnum),$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), '(', OP (BITNUM), ')', ',', OP (OFFSET), 0 } },
    & ifmt_bbi, { 0xf8000000 }
  },
/* bbvl $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0xf4000000 }
  },
/* bbvnl $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0xfc000000 }
  },
/* bgtzal $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4150000 }
  },
/* bgtzall $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4170000 }
  },
/* blezal $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4140000 }
  },
/* blezall $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4160000 }
  },
/* bgtz $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4050000 }
  },
/* bgtzl $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4070000 }
  },
/* blez $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4040000 }
  },
/* blezl $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4060000 }
  },
/* bmb $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x18000000 }
  },
/* bmbl $rs,$rt,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (OFFSET), 0 } },
    & ifmt_bbv, { 0x58000000 }
  },
/* bri $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4080000 }
  },
/* brv $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x4090000 }
  },
/* bctx $rs,$offset */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (OFFSET), 0 } },
    & ifmt_bgez, { 0x40c0000 }
  },
/* yield */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_break, { 0xe }
  },
/* crc32 $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000014 }
  },
/* crc32b $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000015 }
  },
/* cnt1s $rd,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_add, { 0x2e }
  },
/* avail $rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_avail, { 0x4c000024 }
  },
/* free $rd,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_jalr, { 0x4c000025 }
  },
/* tstod $rd,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_jalr, { 0x4c000027 }
  },
/* cmphdr $rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_avail, { 0x4c00002c }
  },
/* mcid $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c000020 }
  },
/* dba $rd */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), 0 } },
    & ifmt_avail, { 0x4c000022 }
  },
/* dbd $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000021 }
  },
/* dpwt $rd,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_jalr, { 0x4c000023 }
  },
/* chkhdr $rd,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), 0 } },
    & ifmt_jalr, { 0x4c000026 }
  },
/* rba $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000008 }
  },
/* rbal $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000009 }
  },
/* rbar $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c00000a }
  },
/* wba $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000010 }
  },
/* wbau $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000011 }
  },
/* wbac $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000012 }
  },
/* rbi $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x4c000200 }
  },
/* rbil $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x4c000300 }
  },
/* rbir $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x4c000100 }
  },
/* wbi $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x4c000600 }
  },
/* wbic $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x4c000500 }
  },
/* wbiu $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x4c000700 }
  },
/* pkrli $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x48000000 }
  },
/* pkrlih $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x48000200 }
  },
/* pkrliu $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x48000100 }
  },
/* pkrlic $rd,$rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_rbi, { 0x48000300 }
  },
/* pkrla $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000028 }
  },
/* pkrlau $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000029 }
  },
/* pkrlah $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c00002a }
  },
/* pkrlac $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c00002b }
  },
/* lock $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c000001 }
  },
/* unlk $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c000003 }
  },
/* swrd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c000004 }
  },
/* swrdl $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c000005 }
  },
/* swwr $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000006 }
  },
/* swwru $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c000007 }
  },
/* dwrd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c00000c }
  },
/* dwrdl $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c00000d }
  },
/* cam36 $rd,$rt,${cam-z},${cam-y} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Z), ',', OP (CAM_Y), 0 } },
    & ifmt_cam36, { 0x4c000400 }
  },
/* cam72 $rd,$rt,${cam-y},${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Y), ',', OP (CAM_Z), 0 } },
    & ifmt_cam36, { 0x4c000440 }
  },
/* cam144 $rd,$rt,${cam-y},${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Y), ',', OP (CAM_Z), 0 } },
    & ifmt_cam36, { 0x4c000480 }
  },
/* cam288 $rd,$rt,${cam-y},${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Y), ',', OP (CAM_Z), 0 } },
    & ifmt_cam36, { 0x4c0004c0 }
  },
/* cm32and $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000ab }
  },
/* cm32andn $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000a3 }
  },
/* cm32or $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000aa }
  },
/* cm32ra $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c0000b0 }
  },
/* cm32rd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c0000a1 }
  },
/* cm32ri $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c0000a4 }
  },
/* cm32rs $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_add, { 0x4c0000a0 }
  },
/* cm32sa $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000b8 }
  },
/* cm32sd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c0000a9 }
  },
/* cm32si $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c0000ac }
  },
/* cm32ss $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000a8 }
  },
/* cm32xor $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000a2 }
  },
/* cm64clr $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c000085 }
  },
/* cm64ra $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c000090 }
  },
/* cm64rd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c000081 }
  },
/* cm64ri $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c000084 }
  },
/* cm64ria2 $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c000094 }
  },
/* cm64rs $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c000080 }
  },
/* cm64sa $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c000098 }
  },
/* cm64sd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c000089 }
  },
/* cm64si $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_cm32rd, { 0x4c00008c }
  },
/* cm64sia2 $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c00009c }
  },
/* cm64ss $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c000088 }
  },
/* cm128ria2 $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c000095 }
  },
/* cm128ria3 $rd,$rs,$rt,${cm-3z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (CM_3Z), 0 } },
    & ifmt_cm128ria3, { 0x4c000090 }
  },
/* cm128ria4 $rd,$rs,$rt,${cm-4z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (CM_4Z), 0 } },
    & ifmt_cm128ria4, { 0x4c0000b0 }
  },
/* cm128sia2 $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c00009d }
  },
/* cm128sia3 $rd,$rs,$rt,${cm-3z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (CM_3Z), 0 } },
    & ifmt_cm128ria3, { 0x4c000098 }
  },
/* cm128sia4 $rd,$rs,$rt,${cm-4z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), ',', OP (CM_4Z), 0 } },
    & ifmt_cm128ria4, { 0x4c0000b8 }
  },
/* cm128vsa $rd,$rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RS), ',', OP (RT), 0 } },
    & ifmt_cm32and, { 0x4c0000a6 }
  },
/* cfc $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_chkhdr, { 0x4c000000 }
  },
/* ctc $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_ctc, { 0x4c000002 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & iq2000_cgen_ifld_table[IQ2000_##f]
#else
#define F(f) & iq2000_cgen_ifld_table[IQ2000_/**/f]
#endif
static const CGEN_IFMT ifmt_nop ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_li ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1f0000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_move ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_lb_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_lbu_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_lh_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_lw_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_add ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_addu ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_and ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_j ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1fffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_or ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_sll ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_slt ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_sltu ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_sra ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_srl ATTRIBUTE_UNUSED = {
  32, 32, 0xfc0007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_not ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_subi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_sub ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_subu ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_sb_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_sh_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_sw_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_xor ATTRIBUTE_UNUSED = {
  32, 32, 0xfc000000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldw_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_sdw_base_0 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe00000, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_IMM) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_avail ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cam36 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007c7, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP_10) }, { F (F_CAM_Z) }, { F (F_CAM_Y) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cam72 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007c7, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP_10) }, { F (F_CAM_Z) }, { F (F_CAM_Y) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cam144 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007c7, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP_10) }, { F (F_CAM_Z) }, { F (F_CAM_Y) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cam288 ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007c7, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP_10) }, { F (F_CAM_Z) }, { F (F_CAM_Y) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32read ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64read ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32mlog ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32and ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32andn ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32or ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32ra ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32rd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32ri ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32rs ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32sa ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32sd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32si ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32ss ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm32xor ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64clr ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64ra ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64rd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64ri ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64ria2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64rs ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64sa ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64sd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64si ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64sia2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm64ss ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm128ria2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm128ria3 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00fffc, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_CM_4FUNC) }, { F (F_CM_3Z) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm128ria4 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00fff8, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_CM_3FUNC) }, { F (F_CM_4Z) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm128sia2 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm128sia3 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00fffc, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_CM_4FUNC) }, { F (F_CM_3Z) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cm128sia4 ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00fff8, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_CP_GRP) }, { F (F_CM_3FUNC) }, { F (F_CM_4Z) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_cmphdr ATTRIBUTE_UNUSED = {
  32, 32, 0xffffffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_dbd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe007ff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m2_dbd ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_dpwt ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1fffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_free ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1fffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_lock ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrla ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrlac ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrlah ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrlau ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrli ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrlic ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrlih ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_pkrliu ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_rba ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_rbal ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_rbar ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_rbi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_rbil ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_rbir ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_swwr ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_swwru ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_tstod ATTRIBUTE_UNUSED = {
  32, 32, 0xfc1fffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_unlk ATTRIBUTE_UNUSED = {
  32, 32, 0xffe0ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_wba ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_wbac ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_wbau ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ffff, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_SHAMT) }, { F (F_FUNC) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_wbi ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_wbic ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

static const CGEN_IFMT ifmt_m_wbiu ATTRIBUTE_UNUSED = {
  32, 32, 0xfc00ff00, { { F (F_OPCODE) }, { F (F_RS) }, { F (F_RT) }, { F (F_RD) }, { F (F_CP_OP) }, { F (F_BYTECOUNT) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) IQ2000_OPERAND_##op
#else
#define OPERAND(op) IQ2000_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE iq2000_cgen_macro_insn_table[] =
{
/* nop */
  {
    -1, "nop", "nop", 32,
    { 0|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* li $rs,$imm */
  {
    -1, "li", "li", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* move $rd,$rt */
  {
    -1, "move", "move", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* lb $rt,$lo16 */
  {
    -1, "lb-base-0", "lb", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* lbu $rt,$lo16 */
  {
    -1, "lbu-base-0", "lbu", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* lh $rt,$lo16 */
  {
    -1, "lh-base-0", "lh", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* lw $rt,$lo16 */
  {
    -1, "lw-base-0", "lw", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* add $rt,$rs,$lo16 */
  {
    -1, "m-add", "add", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* addu $rt,$rs,$lo16 */
  {
    -1, "m-addu", "addu", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* and $rt,$rs,$lo16 */
  {
    -1, "m-and", "and", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* j $rs */
  {
    -1, "m-j", "j", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* or $rt,$rs,$lo16 */
  {
    -1, "m-or", "or", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sll $rd,$rt,$rs */
  {
    -1, "m-sll", "sll", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(USES_RT)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* slt $rt,$rs,$imm */
  {
    -1, "m-slt", "slt", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sltu $rt,$rs,$imm */
  {
    -1, "m-sltu", "sltu", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sra $rd,$rt,$rs */
  {
    -1, "m-sra", "sra", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(USES_RT)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* srl $rd,$rt,$rs */
  {
    -1, "m-srl", "srl", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(USES_RT)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* not $rd,$rt */
  {
    -1, "not", "not", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* subi $rt,$rs,$mlo16 */
  {
    -1, "subi", "subi", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub $rt,$rs,$mlo16 */
  {
    -1, "m-sub", "sub", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* subu $rt,$rs,$mlo16 */
  {
    -1, "m-subu", "subu", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sb $rt,$lo16 */
  {
    -1, "sb-base-0", "sb", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sh $rt,$lo16 */
  {
    -1, "sh-base-0", "sh", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sw $rt,$lo16 */
  {
    -1, "sw-base-0", "sw", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor $rt,$rs,$lo16 */
  {
    -1, "m-xor", "xor", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } } } }
  },
/* ldw $rt,$lo16 */
  {
    -1, "ldw-base-0", "ldw", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(USES_RT)|A(LOAD_DELAY)|A(EVEN_REG_NUM)|A(ALIAS), { { { (1<<MACH_IQ2000), 0 } } } }
  },
/* sdw $rt,$lo16 */
  {
    -1, "sdw-base-0", "sdw", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(EVEN_REG_NUM)|A(ALIAS), { { { (1<<MACH_IQ2000), 0 } } } }
  },
/* avail */
  {
    -1, "m-avail", "avail", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cam36 $rd,$rt,${cam-z} */
  {
    -1, "m-cam36", "cam36", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cam72 $rd,$rt,${cam-z} */
  {
    -1, "m-cam72", "cam72", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cam144 $rd,$rt,${cam-z} */
  {
    -1, "m-cam144", "cam144", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cam288 $rd,$rt,${cam-z} */
  {
    -1, "m-cam288", "cam288", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32read $rd,$rt */
  {
    -1, "m-cm32read", "cm32read", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64read $rd,$rt */
  {
    -1, "m-cm64read", "cm64read", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32mlog $rs,$rt */
  {
    -1, "m-cm32mlog", "cm32mlog", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32and $rs,$rt */
  {
    -1, "m-cm32and", "cm32and", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32andn $rs,$rt */
  {
    -1, "m-cm32andn", "cm32andn", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32or $rs,$rt */
  {
    -1, "m-cm32or", "cm32or", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32ra $rs,$rt */
  {
    -1, "m-cm32ra", "cm32ra", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32rd $rt */
  {
    -1, "m-cm32rd", "cm32rd", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32ri $rt */
  {
    -1, "m-cm32ri", "cm32ri", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32rs $rs,$rt */
  {
    -1, "m-cm32rs", "cm32rs", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32sa $rs,$rt */
  {
    -1, "m-cm32sa", "cm32sa", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32sd $rt */
  {
    -1, "m-cm32sd", "cm32sd", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32si $rt */
  {
    -1, "m-cm32si", "cm32si", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32ss $rs,$rt */
  {
    -1, "m-cm32ss", "cm32ss", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm32xor $rs,$rt */
  {
    -1, "m-cm32xor", "cm32xor", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64clr $rt */
  {
    -1, "m-cm64clr", "cm64clr", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64ra $rs,$rt */
  {
    -1, "m-cm64ra", "cm64ra", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64rd $rt */
  {
    -1, "m-cm64rd", "cm64rd", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64ri $rt */
  {
    -1, "m-cm64ri", "cm64ri", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64ria2 $rs,$rt */
  {
    -1, "m-cm64ria2", "cm64ria2", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64rs $rs,$rt */
  {
    -1, "m-cm64rs", "cm64rs", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64sa $rs,$rt */
  {
    -1, "m-cm64sa", "cm64sa", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64sd $rt */
  {
    -1, "m-cm64sd", "cm64sd", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64si $rt */
  {
    -1, "m-cm64si", "cm64si", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64sia2 $rs,$rt */
  {
    -1, "m-cm64sia2", "cm64sia2", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm64ss $rs,$rt */
  {
    -1, "m-cm64ss", "cm64ss", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm128ria2 $rs,$rt */
  {
    -1, "m-cm128ria2", "cm128ria2", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm128ria3 $rs,$rt,${cm-3z} */
  {
    -1, "m-cm128ria3", "cm128ria3", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm128ria4 $rs,$rt,${cm-4z} */
  {
    -1, "m-cm128ria4", "cm128ria4", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm128sia2 $rs,$rt */
  {
    -1, "m-cm128sia2", "cm128sia2", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm128sia3 $rs,$rt,${cm-3z} */
  {
    -1, "m-cm128sia3", "cm128sia3", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cm128sia4 $rs,$rt,${cm-4z} */
  {
    -1, "m-cm128sia4", "cm128sia4", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* cmphdr */
  {
    -1, "m-cmphdr", "cmphdr", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* dbd $rd,$rt */
  {
    -1, "m-dbd", "dbd", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* dbd $rt */
  {
    -1, "m2-dbd", "dbd", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* dpwt $rs */
  {
    -1, "m-dpwt", "dpwt", 32,
    { 0|A(NO_DIS)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* free $rs */
  {
    -1, "m-free", "free", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* lock $rt */
  {
    -1, "m-lock", "lock", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrla $rs,$rt */
  {
    -1, "m-pkrla", "pkrla", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrlac $rs,$rt */
  {
    -1, "m-pkrlac", "pkrlac", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrlah $rs,$rt */
  {
    -1, "m-pkrlah", "pkrlah", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrlau $rs,$rt */
  {
    -1, "m-pkrlau", "pkrlau", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrli $rs,$rt,$bytecount */
  {
    -1, "m-pkrli", "pkrli", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrlic $rs,$rt,$bytecount */
  {
    -1, "m-pkrlic", "pkrlic", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrlih $rs,$rt,$bytecount */
  {
    -1, "m-pkrlih", "pkrlih", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* pkrliu $rs,$rt,$bytecount */
  {
    -1, "m-pkrliu", "pkrliu", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* rba $rs,$rt */
  {
    -1, "m-rba", "rba", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* rbal $rs,$rt */
  {
    -1, "m-rbal", "rbal", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* rbar $rs,$rt */
  {
    -1, "m-rbar", "rbar", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* rbi $rs,$rt,$bytecount */
  {
    -1, "m-rbi", "rbi", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* rbil $rs,$rt,$bytecount */
  {
    -1, "m-rbil", "rbil", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* rbir $rs,$rt,$bytecount */
  {
    -1, "m-rbir", "rbir", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* swwr $rs,$rt */
  {
    -1, "m-swwr", "swwr", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* swwru $rs,$rt */
  {
    -1, "m-swwru", "swwru", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* tstod $rs */
  {
    -1, "m-tstod", "tstod", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* unlk $rt */
  {
    -1, "m-unlk", "unlk", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* wba $rs,$rt */
  {
    -1, "m-wba", "wba", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* wbac $rs,$rt */
  {
    -1, "m-wbac", "wbac", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* wbau $rs,$rt */
  {
    -1, "m-wbau", "wbau", 32,
    { 0|A(NO_DIS)|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* wbi $rs,$rt,$bytecount */
  {
    -1, "m-wbi", "wbi", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* wbic $rs,$rt,$bytecount */
  {
    -1, "m-wbic", "wbic", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
/* wbiu $rs,$rt,$bytecount */
  {
    -1, "m-wbiu", "wbiu", 32,
    { 0|A(NO_DIS)|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(ALIAS), { { { (1<<MACH_IQ10), 0 } } } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE iq2000_cgen_macro_insn_opcode_table[] =
{
/* nop */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_nop, { 0x0 }
  },
/* li $rs,$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (IMM), 0 } },
    & ifmt_li, { 0x34000000 }
  },
/* move $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_move, { 0x25 }
  },
/* lb $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_lb_base_0, { 0x80000000 }
  },
/* lbu $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_lbu_base_0, { 0x90000000 }
  },
/* lh $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_lh_base_0, { 0x84000000 }
  },
/* lw $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_lw_base_0, { 0x8c000000 }
  },
/* add $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_m_add, { 0x20000000 }
  },
/* addu $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_m_addu, { 0x24000000 }
  },
/* and $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_m_and, { 0x30000000 }
  },
/* j $rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), 0 } },
    & ifmt_m_j, { 0x8 }
  },
/* or $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_m_or, { 0x34000000 }
  },
/* sll $rd,$rt,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), 0 } },
    & ifmt_m_sll, { 0x4 }
  },
/* slt $rt,$rs,$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (IMM), 0 } },
    & ifmt_m_slt, { 0x28000000 }
  },
/* sltu $rt,$rs,$imm */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (IMM), 0 } },
    & ifmt_m_sltu, { 0x2c000000 }
  },
/* sra $rd,$rt,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), 0 } },
    & ifmt_m_sra, { 0x7 }
  },
/* srl $rd,$rt,$rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (RS), 0 } },
    & ifmt_m_srl, { 0x6 }
  },
/* not $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_not, { 0x27 }
  },
/* subi $rt,$rs,$mlo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (MLO16), 0 } },
    & ifmt_subi, { 0x24000000 }
  },
/* sub $rt,$rs,$mlo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (MLO16), 0 } },
    & ifmt_m_sub, { 0x24000000 }
  },
/* subu $rt,$rs,$mlo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (MLO16), 0 } },
    & ifmt_m_subu, { 0x24000000 }
  },
/* sb $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_sb_base_0, { 0xa0000000 }
  },
/* sh $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_sh_base_0, { 0xa4000000 }
  },
/* sw $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_sw_base_0, { 0xac000000 }
  },
/* xor $rt,$rs,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (RS), ',', OP (LO16), 0 } },
    & ifmt_m_xor, { 0x38000000 }
  },
/* ldw $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_ldw_base_0, { 0xc0000000 }
  },
/* sdw $rt,$lo16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), ',', OP (LO16), 0 } },
    & ifmt_sdw_base_0, { 0xe0000000 }
  },
/* avail */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_m_avail, { 0x4c000024 }
  },
/* cam36 $rd,$rt,${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Z), 0 } },
    & ifmt_m_cam36, { 0x4c000400 }
  },
/* cam72 $rd,$rt,${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Z), 0 } },
    & ifmt_m_cam72, { 0x4c000440 }
  },
/* cam144 $rd,$rt,${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Z), 0 } },
    & ifmt_m_cam144, { 0x4c000480 }
  },
/* cam288 $rd,$rt,${cam-z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), ',', OP (CAM_Z), 0 } },
    & ifmt_m_cam288, { 0x4c0004c0 }
  },
/* cm32read $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_m_cm32read, { 0x4c0000b0 }
  },
/* cm64read $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_m_cm64read, { 0x4c000090 }
  },
/* cm32mlog $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32mlog, { 0x4c0000aa }
  },
/* cm32and $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32and, { 0x4c0000ab }
  },
/* cm32andn $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32andn, { 0x4c0000a3 }
  },
/* cm32or $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32or, { 0x4c0000aa }
  },
/* cm32ra $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32ra, { 0x4c0000b0 }
  },
/* cm32rd $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm32rd, { 0x4c0000a1 }
  },
/* cm32ri $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm32ri, { 0x4c0000a4 }
  },
/* cm32rs $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32rs, { 0x4c0000a0 }
  },
/* cm32sa $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32sa, { 0x4c0000b8 }
  },
/* cm32sd $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm32sd, { 0x4c0000a9 }
  },
/* cm32si $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm32si, { 0x4c0000ac }
  },
/* cm32ss $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32ss, { 0x4c0000a8 }
  },
/* cm32xor $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm32xor, { 0x4c0000a2 }
  },
/* cm64clr $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm64clr, { 0x4c000085 }
  },
/* cm64ra $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm64ra, { 0x4c000090 }
  },
/* cm64rd $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm64rd, { 0x4c000081 }
  },
/* cm64ri $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm64ri, { 0x4c000084 }
  },
/* cm64ria2 $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm64ria2, { 0x4c000094 }
  },
/* cm64rs $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm64rs, { 0x4c000080 }
  },
/* cm64sa $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm64sa, { 0x4c000098 }
  },
/* cm64sd $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm64sd, { 0x4c000089 }
  },
/* cm64si $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_cm64si, { 0x4c00008c }
  },
/* cm64sia2 $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm64sia2, { 0x4c00009c }
  },
/* cm64ss $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm64ss, { 0x4c000088 }
  },
/* cm128ria2 $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm128ria2, { 0x4c000095 }
  },
/* cm128ria3 $rs,$rt,${cm-3z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (CM_3Z), 0 } },
    & ifmt_m_cm128ria3, { 0x4c000090 }
  },
/* cm128ria4 $rs,$rt,${cm-4z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (CM_4Z), 0 } },
    & ifmt_m_cm128ria4, { 0x4c0000b0 }
  },
/* cm128sia2 $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_cm128sia2, { 0x4c00009d }
  },
/* cm128sia3 $rs,$rt,${cm-3z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (CM_3Z), 0 } },
    & ifmt_m_cm128sia3, { 0x4c000098 }
  },
/* cm128sia4 $rs,$rt,${cm-4z} */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (CM_4Z), 0 } },
    & ifmt_m_cm128sia4, { 0x4c0000b8 }
  },
/* cmphdr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, 0 } },
    & ifmt_m_cmphdr, { 0x4c00002c }
  },
/* dbd $rd,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RD), ',', OP (RT), 0 } },
    & ifmt_m_dbd, { 0x4c000021 }
  },
/* dbd $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m2_dbd, { 0x4c000021 }
  },
/* dpwt $rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), 0 } },
    & ifmt_m_dpwt, { 0x4c000023 }
  },
/* free $rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), 0 } },
    & ifmt_m_free, { 0x4c000025 }
  },
/* lock $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_lock, { 0x4c000001 }
  },
/* pkrla $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_pkrla, { 0x4c000028 }
  },
/* pkrlac $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_pkrlac, { 0x4c00002b }
  },
/* pkrlah $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_pkrlah, { 0x4c00002a }
  },
/* pkrlau $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_pkrlau, { 0x4c000029 }
  },
/* pkrli $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_pkrli, { 0x48000000 }
  },
/* pkrlic $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_pkrlic, { 0x48000300 }
  },
/* pkrlih $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_pkrlih, { 0x48000200 }
  },
/* pkrliu $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_pkrliu, { 0x48000100 }
  },
/* rba $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_rba, { 0x4c000008 }
  },
/* rbal $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_rbal, { 0x4c000009 }
  },
/* rbar $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_rbar, { 0x4c00000a }
  },
/* rbi $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_rbi, { 0x4c000200 }
  },
/* rbil $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_rbil, { 0x4c000300 }
  },
/* rbir $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_rbir, { 0x4c000100 }
  },
/* swwr $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_swwr, { 0x4c000006 }
  },
/* swwru $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_swwru, { 0x4c000007 }
  },
/* tstod $rs */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), 0 } },
    & ifmt_m_tstod, { 0x4c000027 }
  },
/* unlk $rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RT), 0 } },
    & ifmt_m_unlk, { 0x4c000003 }
  },
/* wba $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_wba, { 0x4c000010 }
  },
/* wbac $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_wbac, { 0x4c000012 }
  },
/* wbau $rs,$rt */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), 0 } },
    & ifmt_m_wbau, { 0x4c000011 }
  },
/* wbi $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_wbi, { 0x4c000600 }
  },
/* wbic $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_wbic, { 0x4c000500 }
  },
/* wbiu $rs,$rt,$bytecount */
  {
    { 0, 0, 0, 0 },
    { { MNEM, ' ', OP (RS), ',', OP (RT), ',', OP (BYTECOUNT), 0 } },
    & ifmt_m_wbiu, { 0x4c000700 }
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
iq2000_cgen_init_opcode_table (CGEN_CPU_DESC cd)
{
  int i;
  int num_macros = (sizeof (iq2000_cgen_macro_insn_table) /
		    sizeof (iq2000_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & iq2000_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & iq2000_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = xmalloc (num_macros * sizeof (CGEN_INSN));

  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      iq2000_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & iq2000_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      iq2000_cgen_build_insn_regex (& insns[i]);
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
