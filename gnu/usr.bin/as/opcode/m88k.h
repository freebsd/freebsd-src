/* m88k-opcode.h -- Instruction information for the Motorola 88000
   Contributed by Devon Bowen of Buffalo University
   and Torbjorn Granlund of the Swedish Institute of Computer Science.
   Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if !defined(__STDC__) && !defined(const)
#define const
#endif

/*
  Character codes for op_spec field below.
  Reserved for direct matching: x , [ ]

  d = GRF Destination register (21:5)
  1 = Source register 1 (16:5)
  2 = Source register 2 (0:5)
  3 = Both source registers (same value) (0:5 and 16:5)
  I = IMM16 (0:16)
  b = bit field spec. (0:10)
  p = 16 bit pc displ. (0:16)
  P = 26 bit pc displ. (0:26)
  B = bb0/bb1 condition (21:5)
  M = bcnd condition (21:5)
  f = fcr (5:6)
  c = cr (5:6)
  V = VEC9 (0:9)
  ? = Give warning for this insn/operand combination
 */

/* instruction descriptor structure */

struct m88k_opcode
{
  unsigned int	opcode;
  char		*name;
  char		*op_spec;
};

/* and introducing... the Motorola 88100 instruction sets... */

/* These macros may seem silly, but they are in preparation
   for future versions of the 88000 family.  */

#define _MC88100(OPCODE,MNEM,OP_SPEC) {OPCODE,MNEM,OP_SPEC},
#define _MC88xxx(OPCODE,MNEM,OP_SPEC) {OPCODE,MNEM,OP_SPEC},

/* Equal mnemonics must be adjacent.
   More specific operand specification must go before more general.
   For example, "d,1,2" must go before "d,1,I" as a register for s2
   would otherwise be considered a variable name.  */

static struct m88k_opcode m88k_opcodes[] =
{
  /*  Opcode     Mnemonic         Opspec */

  _MC88xxx(0xf4007000, "add",		"d,1,2")
  _MC88xxx(0x70000000, "add",		"d,1,I")
  _MC88xxx(0xf4007200, "add.ci",	"d,1,2")
  _MC88xxx(0xf4007300, "add.cio",	"d,1,2")
  _MC88xxx(0xf4007100, "add.co",	"d,1,2")
  _MC88xxx(0xf4006000, "addu",		"d,1,2")
  _MC88xxx(0x60000000, "addu",		"d,1,I")
  _MC88xxx(0xf4006200, "addu.ci",	"d,1,2")
  _MC88xxx(0xf4006300, "addu.cio",	"d,1,2")
  _MC88xxx(0xf4006100, "addu.co",	"d,1,2")
  _MC88xxx(0xf4004000, "and",		"d,1,2")
  _MC88xxx(0x40000000, "and",		"d,1,I")
  _MC88xxx(0xf4004400, "and.c",		"d,1,2")
  _MC88xxx(0x44000000, "and.u",		"d,1,I")
  _MC88xxx(0xd0000000, "bb0",		"B,1,p")
  _MC88xxx(0xd4000000, "bb0.n",		"B,1,p")
  _MC88xxx(0xd8000000, "bb1",		"B,1,p")
  _MC88xxx(0xdc000000, "bb1.n",		"B,1,p")
  _MC88xxx(0xe8000000, "bcnd",		"M,1,p")
  _MC88xxx(0xec000000, "bcnd.n",	"M,1,p")
  _MC88xxx(0xc0000000, "br",		"P")
  _MC88xxx(0xc4000000, "br.n",		"P")
  _MC88xxx(0xc8000000, "bsr",		"P")
  _MC88xxx(0xcc000000, "bsr.n",		"P")
  _MC88xxx(0xf4008000, "clr",		"d,1,2")
  _MC88xxx(0xf0008000, "clr",		"d,1,b")
  _MC88xxx(0xf4007c00, "cmp",		"d,1,2")
  _MC88xxx(0x7c000000, "cmp",		"d,1,I")
  _MC88xxx(0xf4007800, "div",		"d,1,2")
  _MC88xxx(0x78000000, "div",		"d,1,I")
  _MC88xxx(0xf4007800, "divs",		"d,1,2")
  _MC88xxx(0x78000000, "divs",		"d,1,I")
  _MC88xxx(0xf4006800, "divu",		"d,1,2")
  _MC88xxx(0x68000000, "divu",		"d,1,I")
  _MC88xxx(0xf4009000, "ext",		"d,1,2")
  _MC88xxx(0xf0009000, "ext",		"d,1,b")
  _MC88xxx(0xf4009800, "extu",		"d,1,2")
  _MC88xxx(0xf0009800, "extu",		"d,1,b")
  _MC88xxx(0x84002800, "fadd.sss",	"d,1,2")
  _MC88xxx(0x84002880, "fadd.ssd",	"d,1,2")
  _MC88xxx(0x84002a00, "fadd.sds",	"d,1,2")
  _MC88xxx(0x84002a80, "fadd.sdd",	"d,1,2")
  _MC88xxx(0x84002820, "fadd.dss",	"d,1,2")
  _MC88xxx(0x840028a0, "fadd.dsd",	"d,1,2")
  _MC88xxx(0x84002a20, "fadd.dds",	"d,1,2")
  _MC88xxx(0x84002aa0, "fadd.ddd",	"d,1,2")
  _MC88xxx(0x84003a80, "fcmp.sdd",	"d,1,2")
  _MC88xxx(0x84003a00, "fcmp.sds",	"d,1,2")
  _MC88xxx(0x84003880, "fcmp.ssd",	"d,1,2")
  _MC88xxx(0x84003800, "fcmp.sss",	"d,1,2")
  _MC88xxx(0x84007000, "fdiv.sss",	"d,1,2")
  _MC88xxx(0x84007080, "fdiv.ssd",	"d,1,2")
  _MC88xxx(0x84007200, "fdiv.sds",	"d,1,2")
  _MC88xxx(0x84007280, "fdiv.sdd",	"d,1,2")
  _MC88xxx(0x84007020, "fdiv.dss",	"d,1,2")
  _MC88xxx(0x840070a0, "fdiv.dsd",	"d,1,2")
  _MC88xxx(0x84007220, "fdiv.dds",	"d,1,2")
  _MC88xxx(0x840072a0, "fdiv.ddd",	"d,1,2")
  _MC88xxx(0xf400ec00, "ff0",		"d,2")
  _MC88xxx(0xf400e800, "ff1",		"d,2")
  _MC88xxx(0x80004800, "fldcr",		"d,f")
  _MC88xxx(0x84002020, "flt.ds",	"d,2")
  _MC88xxx(0x84002000, "flt.ss",	"d,2")
  _MC88xxx(0x84000000, "fmul.sss",	"d,1,2")
  _MC88xxx(0x84000080, "fmul.ssd",	"d,1,2")
  _MC88xxx(0x84000200, "fmul.sds",	"d,1,2")
  _MC88xxx(0x84000280, "fmul.sdd",	"d,1,2")
  _MC88xxx(0x84000020, "fmul.dss",	"d,1,2")
  _MC88xxx(0x840000a0, "fmul.dsd",	"d,1,2")
  _MC88xxx(0x84000220, "fmul.dds",	"d,1,2")
  _MC88xxx(0x840002a0, "fmul.ddd",	"d,1,2")
  _MC88xxx(0x80008800, "fstcr",		"3,f")
  _MC88xxx(0x84003000, "fsub.sss",	"d,1,2")
  _MC88xxx(0x84003080, "fsub.ssd",	"d,1,2")
  _MC88xxx(0x84003200, "fsub.sds",	"d,1,2")
  _MC88xxx(0x84003280, "fsub.sdd",	"d,1,2")
  _MC88xxx(0x84003020, "fsub.dss",	"d,1,2")
  _MC88xxx(0x840030a0, "fsub.dsd",	"d,1,2")
  _MC88xxx(0x84003220, "fsub.dds",	"d,1,2")
  _MC88xxx(0x840032a0, "fsub.ddd",	"d,1,2")
  _MC88xxx(0x8000c800, "fxcr",		"d,3,f")
  _MC88xxx(0x8400fc01, "illop1",	"")
  _MC88xxx(0x8400fc02, "illop2",	"")
  _MC88xxx(0x8400fc03, "illop3",	"")
  _MC88xxx(0x84004880, "int.sd",	"d,2")
  _MC88xxx(0x84004800, "int.ss",	"d,2")
  _MC88xxx(0xf400c000, "jmp",		"2")
  _MC88xxx(0xf400c400, "jmp.n",		"2")
  _MC88xxx(0xf400c800, "jsr",		"2")
  _MC88xxx(0xf400cc00, "jsr.n",		"2")
  _MC88xxx(0xf4001400, "ld",		"d,1,2")
  _MC88xxx(0xf4001600, "ld",		"d,1[2]")
  _MC88xxx(0x14000000, "ld",		"d,1,I")
  _MC88xxx(0xf4001e00, "ld.b",		"d,1[2]")
  _MC88xxx(0xf4001c00, "ld.b",		"d,1,2")
  _MC88xxx(0x1c000000, "ld.b",		"d,1,I")
  _MC88xxx(0xf4001d00, "ld.b.usr",	"d,1,2")
  _MC88xxx(0xf4001f00, "ld.b.usr",	"d,1[2]")
  _MC88xxx(0xf4000e00, "ld.bu",		"d,1[2]")
  _MC88xxx(0xf4000c00, "ld.bu",		"d,1,2")
  _MC88xxx(0x0c000000, "ld.bu",		"d,1,I")
  _MC88xxx(0xf4000d00, "ld.bu.usr",	"d,1,2")
  _MC88xxx(0xf4000f00, "ld.bu.usr",	"d,1[2]")
  _MC88xxx(0xf4001200, "ld.d",		"d,1[2]")
  _MC88xxx(0xf4001000, "ld.d",		"d,1,2")
  _MC88xxx(0x10000000, "ld.d",		"d,1,I")
  _MC88xxx(0xf4001100, "ld.d.usr",	"d,1,2")
  _MC88xxx(0xf4001300, "ld.d.usr",	"d,1[2]")
  _MC88xxx(0xf4001a00, "ld.h",		"d,1[2]")
  _MC88xxx(0xf4001800, "ld.h",		"d,1,2")
  _MC88xxx(0x18000000, "ld.h",		"d,1,I")
  _MC88xxx(0xf4001900, "ld.h.usr",	"d,1,2")
  _MC88xxx(0xf4001b00, "ld.h.usr",	"d,1[2]")
  _MC88xxx(0xf4000a00, "ld.hu",		"d,1[2]")
  _MC88xxx(0xf4000800, "ld.hu",		"d,1,2")
  _MC88xxx(0x08000000, "ld.hu",		"d,1,I")
  _MC88xxx(0xf4000900, "ld.hu.usr",	"d,1,2")
  _MC88xxx(0xf4000b00, "ld.hu.usr",	"d,1[2]")
  _MC88xxx(0xf4001500, "ld.usr",	"d,1,2")
  _MC88xxx(0xf4001700, "ld.usr",	"d,1[2]")
  _MC88xxx(0xf4003600, "lda",		"d,1[2]")
  _MC88xxx(0xf4006000, "lda",		"?d,1,2")	/* Output addu */
  _MC88xxx(0x60000000, "lda",		"?d,1,I")	/* Output addu */
  _MC88xxx(0xf4006000, "lda.b",		"?d,1[2]")	/* Output addu */
  _MC88xxx(0xf4006000, "lda.b",		"?d,1,2")	/* Output addu */
  _MC88xxx(0x60000000, "lda.b",		"?d,1,I")	/* Output addu */
  _MC88xxx(0xf4003200, "lda.d",		"d,1[2]")
  _MC88xxx(0xf4006000, "lda.d",		"?d,1,2")	/* Output addu */
  _MC88xxx(0x60000000, "lda.d",		"?d,1,I")	/* Output addu */
  _MC88xxx(0xf4003a00, "lda.h",		"d,1[2]")
  _MC88xxx(0xf4006000, "lda.h",		"?d,1,2")	/* Output addu */
  _MC88xxx(0x60000000, "lda.h",		"?d,1,I")	/* Output addu */
  _MC88xxx(0x80004000, "ldcr",		"d,c")
  _MC88xxx(0xf400a000, "mak",		"d,1,2")
  _MC88xxx(0xf000a000, "mak",		"d,1,b")
  _MC88xxx(0x48000000, "mask",		"d,1,I")
  _MC88xxx(0x4c000000, "mask.u",	"d,1,I")
  _MC88xxx(0xf4006c00, "mul",		"d,1,2")
  _MC88xxx(0x6c000000, "mul",		"d,1,I")
  _MC88xxx(0xf4006c00, "mulu",		"d,1,2")	/* synonym for mul */
  _MC88xxx(0x6c000000, "mulu",		"d,1,I") 	/* synonym for mul */
  _MC88xxx(0x84005080, "nint.sd",	"d,2")
  _MC88xxx(0x84005000, "nint.ss",	"d,2")
  _MC88xxx(0xf4005800, "or",		"d,1,2")
  _MC88xxx(0x58000000, "or",		"d,1,I")
  _MC88xxx(0xf4005c00, "or.c",		"d,1,2")
  _MC88xxx(0x5c000000, "or.u",		"d,1,I")
  _MC88xxx(0xf000a800, "rot",		"d,1,b")
  _MC88xxx(0xf400a800, "rot",		"d,1,2")
  _MC88xxx(0xf400fc00, "rte",		"")
  _MC88xxx(0xf4008800, "set",		"d,1,2")
  _MC88xxx(0xf0008800, "set",		"d,1,b")
  _MC88xxx(0xf4002600, "st",		"d,1[2]")
  _MC88xxx(0xf4002400, "st",		"d,1,2")
  _MC88xxx(0x24000000, "st",		"d,1,I")
  _MC88xxx(0xf4002e00, "st.b",		"d,1[2]")
  _MC88xxx(0xf4002c00, "st.b",		"d,1,2")
  _MC88xxx(0x2c000000, "st.b",		"d,1,I")
  _MC88xxx(0xf4002d00, "st.b.usr",	"d,1,2")
  _MC88xxx(0xf4002f00, "st.b.usr",	"d,1[2]")
  _MC88xxx(0xf4002200, "st.d",		"d,1[2]")
  _MC88xxx(0xf4002000, "st.d",		"d,1,2")
  _MC88xxx(0x20000000, "st.d",		"d,1,I")
  _MC88xxx(0xf4002100, "st.d.usr",	"d,1,2")
  _MC88xxx(0xf4002300, "st.d.usr",	"d,1[2]")
  _MC88xxx(0xf4002a00, "st.h",		"d,1[2]")
  _MC88xxx(0xf4002800, "st.h",		"d,1,2")
  _MC88xxx(0x28000000, "st.h",		"d,1,I")
  _MC88xxx(0xf4002900, "st.h.usr",	"d,1,2")
  _MC88xxx(0xf4002b00, "st.h.usr",	"d,1[2]")
  _MC88xxx(0xf4002500, "st.usr",	"d,1,2")
  _MC88xxx(0xf4002700, "st.usr",	"d,1[2]")
  _MC88xxx(0x80008000, "stcr",		"3,c")
  _MC88xxx(0xf4007400, "sub",		"d,1,2")
  _MC88xxx(0x74000000, "sub",		"d,1,I")
  _MC88xxx(0xf4007600, "sub.ci",	"d,1,2")
  _MC88xxx(0xf4007700, "sub.cio",	"d,1,2")
  _MC88xxx(0xf4007500, "sub.co",	"d,1,2")
  _MC88xxx(0xf4006400, "subu",		"d,1,2")
  _MC88xxx(0x64000000, "subu",		"d,1,I")
  _MC88xxx(0xf4006600, "subu.ci",	"d,1,2")
  _MC88xxx(0xf4006700, "subu.cio",	"d,1,2")
  _MC88xxx(0xf4006500, "subu.co",	"d,1,2")
  _MC88xxx(0xf000d000, "tb0",		"B,1,V")
  _MC88xxx(0xf000d800, "tb1",		"B,1,V")
  _MC88xxx(0xf400f800, "tbnd",		"1,2")
  _MC88xxx(0xf8000000, "tbnd",		"1,I")
  _MC88xxx(0xf000e800, "tcnd",		"M,1,V")
  _MC88xxx(0x84005880, "trnc.sd",	"d,2")
  _MC88xxx(0x84005800, "trnc.ss",	"d,2")
  _MC88xxx(0x8000c000, "xcr",		"d,1,c")
  _MC88xxx(0xf4000600, "xmem",		"d,1[2]")
  _MC88xxx(0xf4000400, "xmem",		"d,1,2")
  _MC88100(0x04000000, "xmem",		"?d,1,I")
  _MC88xxx(0xf4000200, "xmem.bu",	"d,1[2]")
  _MC88xxx(0xf4000000, "xmem.bu",	"d,1,2")
  _MC88100(0x00000000, "xmem.bu",	"?d,1,I")
  _MC88xxx(0xf4000300, "xmem.bu.usr",	"d,1[2]")
  _MC88xxx(0xf4000100, "xmem.bu.usr",	"d,1,2")
  _MC88100(0x00000100, "xmem.bu.usr",	"?d,1,I")
  _MC88xxx(0xf4000700, "xmem.usr",	"d,1[2]")
  _MC88xxx(0xf4000500, "xmem.usr",	"d,1,2")
  _MC88100(0x04000100, "xmem.usr",	"?d,1,I")
  _MC88xxx(0xf4005000, "xor",		"d,1,2")
  _MC88xxx(0x50000000, "xor",		"d,1,I")
  _MC88xxx(0xf4005400, "xor.c",		"d,1,2")
  _MC88xxx(0x54000000, "xor.u",		"d,1,I")
  _MC88xxx(0x00000000, "",	    0)
};

#define NUMOPCODES ((sizeof m88k_opcodes)/(sizeof m88k_opcodes[0]))
