/* Assemble Matsushita MN10200 instructions.
   Copyright 1996, 1997, 2000 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include "opcode/mn10200.h"


const struct mn10200_operand mn10200_operands[] = {
#define UNUSED	0
  {0, 0, 0}, 

/* dn register in the first register operand position.  */
#define DN0      (UNUSED+1)
  {2, 0, MN10200_OPERAND_DREG},

/* dn register in the second register operand position.  */
#define DN1      (DN0+1)
  {2, 2, MN10200_OPERAND_DREG},

/* dm register in the first register operand position.  */
#define DM0      (DN1+1)
  {2, 0, MN10200_OPERAND_DREG},

/* dm register in the second register operand position.  */
#define DM1      (DM0+1)
  {2, 2, MN10200_OPERAND_DREG},

/* an register in the first register operand position.  */
#define AN0      (DM1+1)
  {2, 0, MN10200_OPERAND_AREG},

/* an register in the second register operand position.  */
#define AN1      (AN0+1)
  {2, 2, MN10200_OPERAND_AREG},

/* am register in the first register operand position.  */
#define AM0      (AN1+1)
  {2, 0, MN10200_OPERAND_AREG},

/* am register in the second register operand position.  */
#define AM1      (AM0+1)
  {2, 2, MN10200_OPERAND_AREG},

/* 8 bit unsigned immediate which may promote to a 16bit
   unsigned immediate.  */
#define IMM8    (AM1+1)
  {8, 0, MN10200_OPERAND_PROMOTE},

/* 16 bit unsigned immediate which may promote to a 32bit
   unsigned immediate.  */
#define IMM16    (IMM8+1)
  {16, 0, MN10200_OPERAND_PROMOTE},

/* 16 bit pc-relative immediate which may promote to a 16bit
   pc-relative immediate.  */
#define IMM16_PCREL    (IMM16+1)
  {16, 0, MN10200_OPERAND_PCREL | MN10200_OPERAND_RELAX | MN10200_OPERAND_SIGNED},

/* 16bit unsigned dispacement in a memory operation which
   may promote to a 32bit displacement.  */
#define IMM16_MEM    (IMM16_PCREL+1)
  {16, 0, MN10200_OPERAND_PROMOTE | MN10200_OPERAND_MEMADDR},

/* 24 immediate, low 16 bits in the main instruction
   word, 8 in the extension word.  */

#define IMM24    (IMM16_MEM+1)
  {24, 0, MN10200_OPERAND_EXTENDED},

/* 32bit pc-relative offset.  */
#define IMM24_PCREL    (IMM24+1)
  {24, 0, MN10200_OPERAND_EXTENDED | MN10200_OPERAND_PCREL | MN10200_OPERAND_SIGNED},

/* 32bit memory offset.  */
#define IMM24_MEM    (IMM24_PCREL+1)
  {24, 0, MN10200_OPERAND_EXTENDED | MN10200_OPERAND_MEMADDR},

/* Processor status word.  */
#define PSW    (IMM24_MEM+1)
  {0, 0, MN10200_OPERAND_PSW},

/* MDR register.  */
#define MDR    (PSW+1)
  {0, 0, MN10200_OPERAND_MDR},

/* Index register.  */
#define DI (MDR+1)
  {2, 4, MN10200_OPERAND_DREG},

/* 8 bit signed displacement, may promote to 16bit signed dispacement.  */
#define SD8    (DI+1)
  {8, 0, MN10200_OPERAND_SIGNED | MN10200_OPERAND_PROMOTE},

/* 16 bit signed displacement, may promote to 32bit dispacement.  */
#define SD16    (SD8+1)
  {16, 0, MN10200_OPERAND_SIGNED | MN10200_OPERAND_PROMOTE},

/* 8 bit pc-relative displacement.  */
#define SD8N_PCREL    (SD16+1)
  {8, 0, MN10200_OPERAND_SIGNED | MN10200_OPERAND_PCREL | MN10200_OPERAND_RELAX},

/* 8 bit signed immediate which may promote to 16bit signed immediate.  */
#define SIMM8    (SD8N_PCREL+1)
  {8, 0, MN10200_OPERAND_SIGNED | MN10200_OPERAND_PROMOTE},

/* 16 bit signed immediate which may promote to 32bit  immediate.  */
#define SIMM16    (SIMM8+1)
  {16, 0, MN10200_OPERAND_SIGNED | MN10200_OPERAND_PROMOTE},

/* 16 bit signed immediate which may not promote.  */
#define SIMM16N    (SIMM16+1)
  {16, 0, MN10200_OPERAND_SIGNED | MN10200_OPERAND_NOCHECK},

/* Either an open paren or close paren.  */
#define PAREN	(SIMM16N+1)
  {0, 0, MN10200_OPERAND_PAREN}, 

/* dn register that appears in the first and second register positions.  */
#define DN01     (PAREN+1)
  {2, 0, MN10200_OPERAND_DREG | MN10200_OPERAND_REPEATED},

/* an register that appears in the first and second register positions.  */
#define AN01     (DN01+1)
  {2, 0, MN10200_OPERAND_AREG | MN10200_OPERAND_REPEATED},
} ; 

#define MEM(ADDR) PAREN, ADDR, PAREN 
#define MEM2(ADDR1,ADDR2) PAREN, ADDR1, ADDR2, PAREN 

/* The opcode table.

   The format of the opcode table is:

   NAME		OPCODE		MASK		{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct mn10200_opcode mn10200_opcodes[] = {
{ "mov",	0x8000,		0xf000,		FMT_2, {SIMM8, DN01}},
{ "mov",	0x80,		0xf0,		FMT_1, {DN1, DM0}},
{ "mov",	0xf230,		0xfff0,		FMT_4, {DM1, AN0}},
{ "mov",	0xf2f0,		0xfff0,		FMT_4, {AN1, DM0}},
{ "mov",	0xf270,		0xfff0,		FMT_4, {AN1, AM0}},
{ "mov",	0xf3f0,		0xfffc,		FMT_4, {PSW, DN0}},
{ "mov",	0xf3d0,		0xfff3,		FMT_4, {DN1, PSW}},
{ "mov",	0xf3e0,		0xfffc,		FMT_4, {MDR, DN0}},
{ "mov",	0xf3c0,		0xfff3,		FMT_4, {DN1, MDR}},
{ "mov",	0x20,		0xf0,		FMT_1, {MEM(AN1), DM0}},
{ "mov",	0x6000,		0xf000,		FMT_2, {MEM2(SD8, AN1), DM0}},
{ "mov",	0xf7c00000,	0xfff00000,	FMT_6, {MEM2(SD16, AN1), DM0}},
{ "mov",	0xf4800000,	0xfff00000,	FMT_7, {MEM2(IMM24,AN1), DM0}},
{ "mov",	0xf140,		0xffc0,		FMT_4, {MEM2(DI, AN1), DM0}},
{ "mov",	0xc80000,	0xfc0000,	FMT_3, {MEM(IMM16_MEM), DN0}},
{ "mov",	0xf4c00000,	0xfffc0000,	FMT_7, {MEM(IMM24_MEM), DN0}},
{ "mov",	0x7000,		0xf000,		FMT_2, {MEM2(SD8,AN1), AM0}},
{ "mov",	0x7000,		0xf000,		FMT_2, {MEM(AN1), AM0}},
{ "mov",	0xf7b00000,	0xfff00000,	FMT_6, {MEM2(SD16, AN1), AM0}},
{ "mov",	0xf4f00000,	0xfff00000,	FMT_7, {MEM2(IMM24,AN1), AM0}},
{ "mov",	0xf100,		0xffc0,		FMT_4, {MEM2(DI, AN1), AM0}},
{ "mov",	0xf7300000,	0xfffc0000,	FMT_6, {MEM(IMM16_MEM), AN0}},
{ "mov",	0xf4d00000,	0xfffc0000,	FMT_7, {MEM(IMM24_MEM), AN0}},
{ "mov",	0x00,		0xf0,		FMT_1, {DM0, MEM(AN1)}},
{ "mov",	0x4000,		0xf000,		FMT_2, {DM0, MEM2(SD8, AN1)}},
{ "mov",	0xf7800000,	0xfff00000,	FMT_6, {DM0, MEM2(SD16, AN1)}},
{ "mov",	0xf4000000,	0xfff00000,	FMT_7, {DM0, MEM2(IMM24, AN1)}},
{ "mov",	0xf1c0,		0xffc0,		FMT_4, {DM0, MEM2(DI, AN1)}},
{ "mov",	0xc00000,	0xfc0000,	FMT_3, {DN0, MEM(IMM16_MEM)}},
{ "mov",	0xf4400000,	0xfffc0000,	FMT_7, {DN0, MEM(IMM24_MEM)}},
{ "mov",	0x5000,		0xf000,		FMT_2, {AM0, MEM2(SD8, AN1)}},
{ "mov",	0x5000,		0xf000,		FMT_2, {AM0, MEM(AN1)}},
{ "mov",	0xf7a00000,	0xfff00000,	FMT_6, {AM0, MEM2(SD16, AN1)}},
{ "mov",	0xf4100000,	0xfff00000,	FMT_7, {AM0, MEM2(IMM24,AN1)}},
{ "mov",	0xf180,		0xffc0,		FMT_4, {AM0, MEM2(DI, AN1)}},
{ "mov",	0xf7200000,	0xfffc0000,	FMT_6, {AN0, MEM(IMM16_MEM)}},
{ "mov",	0xf4500000,	0xfffc0000,	FMT_7, {AN0, MEM(IMM24_MEM)}},
{ "mov",	0xf80000,	0xfc0000,	FMT_3, {SIMM16, DN0}},
{ "mov",	0xf4700000,	0xfffc0000,	FMT_7, {IMM24, DN0}},
{ "mov",	0xdc0000,	0xfc0000,	FMT_3, {IMM16, AN0}},
{ "mov",	0xf4740000,	0xfffc0000,	FMT_7, {IMM24, AN0}},

{ "movx",	0xf57000,	0xfff000,	FMT_5, {MEM2(SD8, AN1), DM0}},
{ "movx",	0xf7700000,	0xfff00000,	FMT_6, {MEM2(SD16, AN1), DM0}},
{ "movx",	0xf4b00000,	0xfff00000,	FMT_7, {MEM2(IMM24,AN1), DM0}},
{ "movx",	0xf55000,	0xfff000,	FMT_5, {DM0, MEM2(SD8, AN1)}},
{ "movx",	0xf7600000,	0xfff00000,	FMT_6, {DM0, MEM2(SD16, AN1)}},
{ "movx",	0xf4300000,	0xfff00000,	FMT_7, {DM0, MEM2(IMM24, AN1)}},

{ "movb",	0xf52000,	0xfff000,	FMT_5, {MEM2(SD8, AN1), DM0}},
{ "movb",	0xf7d00000,	0xfff00000,	FMT_6, {MEM2(SD16, AN1), DM0}},
{ "movb",	0xf4a00000,	0xfff00000,	FMT_7, {MEM2(IMM24,AN1), DM0}},
{ "movb",	0xf040,		0xffc0,		FMT_4, {MEM2(DI, AN1), DM0}},
{ "movb",	0xf4c40000,	0xfffc0000,	FMT_7, {MEM(IMM24_MEM), DN0}},
{ "movb",	0x10,		0xf0,		FMT_1, {DM0, MEM(AN1)}},
{ "movb",	0xf51000,	0xfff000,	FMT_5, {DM0, MEM2(SD8, AN1)}},
{ "movb",	0xf7900000,	0xfff00000,	FMT_6, {DM0, MEM2(SD16, AN1)}},
{ "movb",	0xf4200000,	0xfff00000,	FMT_7, {DM0, MEM2(IMM24, AN1)}},
{ "movb",	0xf0c0,		0xffc0,		FMT_4, {DM0, MEM2(DI, AN1)}},
{ "movb",	0xc40000,	0xfc0000,	FMT_3, {DN0, MEM(IMM16_MEM)}},
{ "movb",	0xf4440000,	0xfffc0000,	FMT_7, {DN0, MEM(IMM24_MEM)}},

{ "movbu",	0x30,		0xf0,		FMT_1, {MEM(AN1), DM0}},
{ "movbu",	0xf53000,	0xfff000,	FMT_5, {MEM2(SD8, AN1), DM0}},
{ "movbu",	0xf7500000,	0xfff00000,	FMT_6, {MEM2(SD16, AN1), DM0}},
{ "movbu",	0xf4900000,	0xfff00000,	FMT_7, {MEM2(IMM24,AN1), DM0}},
{ "movbu",	0xf080,		0xffc0,		FMT_4, {MEM2(DI, AN1), DM0}},
{ "movbu",	0xcc0000,	0xfc0000,	FMT_3, {MEM(IMM16_MEM), DN0}},
{ "movbu",	0xf4c80000,	0xfffc0000,	FMT_7, {MEM(IMM24_MEM), DN0}},

{ "ext",	0xf3c1,		0xfff3,		FMT_4, {DN1}},
{ "extx",	0xb0, 		0xfc,		FMT_1, {DN0}},
{ "extxu",	0xb4,		0xfc,		FMT_1, {DN0}},
{ "extxb",	0xb8,		0xfc,		FMT_1, {DN0}},
{ "extxbu",	0xbc,		0xfc,		FMT_1, {DN0}},

{ "add",	0x90,		0xf0,		FMT_1, {DN1, DM0}},
{ "add",	0xf200,		0xfff0,		FMT_4, {DM1, AN0}},
{ "add",	0xf2c0,		0xfff0,		FMT_4, {AN1, DM0}},
{ "add",	0xf240,		0xfff0,		FMT_4, {AN1, AM0}},
{ "add",	0xd400,		0xfc00,		FMT_2, {SIMM8, DN0}},
{ "add",	0xf7180000,	0xfffc0000,	FMT_6, {SIMM16, DN0}},
{ "add",	0xf4600000,	0xfffc0000,	FMT_7, {IMM24, DN0}},
{ "add",	0xd000,		0xfc00,		FMT_2, {SIMM8, AN0}},
{ "add",	0xf7080000,	0xfffc0000,	FMT_6, {SIMM16, AN0}},
{ "add",	0xf4640000,	0xfffc0000,	FMT_7, {IMM24, AN0}},
{ "addc",	0xf280,		0xfff0,		FMT_4, {DN1, DM0}},
{ "addnf",	0xf50c00,	0xfffc00,	FMT_5, {SIMM8, AN0}},

{ "sub",	0xa0,		0xf0,		FMT_1, {DN1, DM0}},
{ "sub",	0xf210,		0xfff0,		FMT_4, {DN1, AN0}},
{ "sub",	0xf2d0,		0xfff0,		FMT_4, {AN1, DM0}},
{ "sub",	0xf250,		0xfff0,		FMT_4, {AN1, AM0}},
{ "sub",	0xf71c0000,	0xfffc0000,	FMT_6, {IMM16, DN0}},
{ "sub",	0xf4680000,	0xfffc0000,	FMT_7, {IMM24, DN0}},
{ "sub",	0xf70c0000,	0xfffc0000,	FMT_6, {IMM16, AN0}},
{ "sub",	0xf46c0000,	0xfffc0000,	FMT_7, {IMM24, AN0}},
{ "subc",	0xf290,		0xfff0,		FMT_4, {DN1, DM0}},

{ "mul",	0xf340,		0xfff0,		FMT_4, {DN1, DM0}},
{ "mulu",	0xf350,		0xfff0,		FMT_4, {DN1, DM0}},

{ "divu",	0xf360,		0xfff0,		FMT_4, {DN1, DM0}},

{ "cmp",	0xf390,		0xfff0,		FMT_4, {DN1, DM0}},
{ "cmp",	0xf220,		0xfff0,		FMT_4, {DM1, AN0}},
{ "cmp",	0xf2e0,		0xfff0,		FMT_4, {AN1, DM0}},
{ "cmp",	0xf260,		0xfff0,		FMT_4, {AN1, AM0}},
{ "cmp",	0xd800,		0xfc00,		FMT_2, {SIMM8, DN0}},
{ "cmp",	0xf7480000,	0xfffc0000,	FMT_6, {SIMM16, DN0}},
{ "cmp",	0xf4780000,	0xfffc0000,	FMT_7, {IMM24, DN0}},
{ "cmp",	0xec0000,	0xfc0000,	FMT_3, {IMM16, AN0}},
{ "cmp",	0xf47c0000,	0xfffc0000,	FMT_7, {IMM24, AN0}},

{ "and",	0xf300,		0xfff0,		FMT_4, {DN1, DM0}},
{ "and",	0xf50000,	0xfffc00,	FMT_5, {IMM8, DN0}},
{ "and",	0xf7000000,	0xfffc0000,	FMT_6, {SIMM16N, DN0}},
{ "and",	0xf7100000,	0xffff0000,	FMT_6, {SIMM16N, PSW}},
{ "or",		0xf310,		0xfff0,		FMT_4, {DN1, DM0}},
{ "or",		0xf50800,	0xfffc00,	FMT_5, {IMM8, DN0}},
{ "or",		0xf7400000,	0xfffc0000,	FMT_6, {SIMM16N, DN0}},
{ "or",		0xf7140000,	0xffff0000,	FMT_6, {SIMM16N, PSW}},
{ "xor",	0xf320,		0xfff0,		FMT_4, {DN1, DM0}},
{ "xor",	0xf74c0000,	0xfffc0000,	FMT_6, {SIMM16N, DN0}},
{ "not",	0xf3e4,		0xfffc,		FMT_4, {DN0}},

{ "asr",	0xf338,		0xfffc,		FMT_4, {DN0}},
{ "lsr",	0xf33c,		0xfffc,		FMT_4, {DN0}},
{ "ror",	0xf334,		0xfffc,		FMT_4, {DN0}},
{ "rol",	0xf330,		0xfffc,		FMT_4, {DN0}},

{ "btst",	0xf50400,	0xfffc00,	FMT_5, {IMM8, DN0}},
{ "btst",	0xf7040000,	0xfffc0000,	FMT_6, {SIMM16N, DN0}},
{ "bset",	0xf020,		0xfff0,		FMT_4, {DM0, MEM(AN1)}},
{ "bclr",	0xf030,		0xfff0,		FMT_4, {DM0, MEM(AN1)}},

{ "beq",	0xe800,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bne",	0xe900,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "blt",	0xe000,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "ble",	0xe300,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bge",	0xe200,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bgt",	0xe100,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bcs",	0xe400,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bls",	0xe700,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bcc",	0xe600,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bhi",	0xe500,		0xff00,		FMT_2, {SD8N_PCREL}},
{ "bvc",	0xf5fc00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bvs",	0xf5fd00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bnc",	0xf5fe00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bns",	0xf5ff00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bra",	0xea00,		0xff00,		FMT_2, {SD8N_PCREL}},

{ "beqx",	0xf5e800,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bnex",	0xf5e900,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bltx",	0xf5e000,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "blex",	0xf5e300,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bgex",	0xf5e200,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bgtx",	0xf5e100,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bcsx",	0xf5e400,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "blsx",	0xf5e700,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bccx",	0xf5e600,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bhix",	0xf5e500,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bvcx",	0xf5ec00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bvsx",	0xf5ed00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bncx",	0xf5ee00,	0xffff00,	FMT_5, {SD8N_PCREL}},
{ "bnsx",	0xf5ef00,	0xffff00,	FMT_5, {SD8N_PCREL}},

{ "jmp",	0xfc0000,	0xff0000,	FMT_3, {IMM16_PCREL}},
{ "jmp",	0xf4e00000,	0xffff0000,	FMT_7, {IMM24_PCREL}},
{ "jmp",	0xf000,		0xfff3,		FMT_4, {PAREN,AN1,PAREN}},
{ "jsr",	0xfd0000,	0xff0000,	FMT_3, {IMM16_PCREL}},
{ "jsr",	0xf4e10000,	0xffff0000,	FMT_7, {IMM24_PCREL}},
{ "jsr",	0xf001,		0xfff3,		FMT_4, {PAREN,AN1,PAREN}},

{ "nop",	0xf6,		0xff,		FMT_1, {UNUSED}},

{ "rts",	0xfe,		0xff,		FMT_1, {UNUSED}},
{ "rti",	0xeb,		0xff,		FMT_1, {UNUSED}},

/* Extension.  We need some instruction to trigger "emulated syscalls"
   for our simulator.  */
{ "syscall",	0xf010,		0xffff,		FMT_4, {UNUSED}},

/* Extension.  When talking to the simulator, gdb requires some instruction
   that will trigger a "breakpoint" (really just an instruction that isn't
   otherwise used by the tools.  This instruction must be the same size
   as the smallest instruction on the target machine.  In the case of the
   mn10x00 the "break" instruction must be one byte.  0xff is available on
   both mn10x00 architectures.  */
{ "break",      0xff,           0xff,           FMT_1, {UNUSED}},

{ 0, 0, 0, 0, {0}},

} ;

const int mn10200_num_opcodes =
  sizeof (mn10200_opcodes) / sizeof (mn10200_opcodes[0]);


