/* Mips opcde list for GDB, the GNU debugger.
   Copyright (C) 1989 Free Software Foundation, Inc.
   Contributed by Nobuyuki Hikichi(hikichi@sra.junet)
   Made to work for little-endian machines, and debugged
   by Per Bothner (bothner@cs.wisc.edu).
   Many fixes contributed by Frank Yellin (fy@lucid.com).

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if BITS_BIG_ENDIAN
#define BIT_FIELDS_2(a,b) a;b;
#define BIT_FIELDS_4(a,b,c,d) a;b;c;d;
#define BIT_FIELDS_6(a,b,c,d,e,f) a;b;c;d;e;f;
#else
#define BIT_FIELDS_2(a,b) b;a;
#define BIT_FIELDS_4(a,b,c,d) d;c;b;a;
#define BIT_FIELDS_6(a,b,c,d,e,f) f;e;d;c;b;a;
#endif

struct op_i_fmt
{
BIT_FIELDS_4(
  unsigned op : 6,
  unsigned rs : 5,
  unsigned rt : 5,
  unsigned immediate : 16)
};

struct op_j_fmt
{
BIT_FIELDS_2(
  unsigned op : 6,
  unsigned target : 26)
};

struct op_r_fmt
{
BIT_FIELDS_6(
  unsigned op : 6,
  unsigned rs : 5,
  unsigned rt : 5,
  unsigned rd : 5,
  unsigned shamt : 5,
  unsigned funct : 6)
};


struct fop_i_fmt
{
BIT_FIELDS_4(
  unsigned op : 6,
  unsigned rs : 5,
  unsigned rt : 5,
  unsigned immediate : 16)
};

struct op_b_fmt
{
BIT_FIELDS_4(
  unsigned op : 6,
  unsigned rs : 5,
  unsigned rt : 5,
  short delta : 16)
};

struct fop_r_fmt
{
BIT_FIELDS_6(
  unsigned op : 6,
  unsigned fmt : 5,
  unsigned ft : 5,
  unsigned fs : 5,
  unsigned fd : 5,
  unsigned funct : 6)
};

struct mips_opcode
{
  char *name;
  unsigned long opcode;
  unsigned long match;
  char *args;
  int bdelay; /* Nonzero if delayed branch.  */
};

/* args format;

   "s" rs: source register specifier
   "t" rt: target register
   "i" immediate
   "a" target address
   "c" branch condition
   "d" rd: destination register specifier
   "h" shamt: shift amount
   "f" funct: function field

  for fpu
   "S" fs source 1 register
   "T" ft source 2 register
   "D" distination register
*/

#define one(x) (x << 26)
#define op_func(x, y) ((x << 26) | y)
#define op_cond(x, y) ((x << 26) | (y << 16))
#define op_rs_func(x, y, z) ((x << 26) | (y << 21) | z)
#define op_rs_b11(x, y, z) ((x << 26) | (y << 21) | z)
#define op_o16(x, y) ((x << 26) | (y << 16))
#define op_bc(x, y, z) ((x << 26) | (y << 21) | (z << 16))

struct mips_opcode mips_opcodes[] =
{
/* These first opcodes are special cases of the ones in the comments */
  {"nop",	0,		0xffffffff,	     /*li*/	"", 0},
  {"li",	op_bc(9,0,0),	op_bc(0x3f,31,0),    /*addiu*/	"t,j", 0},
  {"b",		one(4),		0xffff0000,	     /*beq*/	"b", 1},
  {"move",	op_func(0, 33),	op_cond(0x3f,31)|0x7ff,/*addu*/	"d,s", 0},

  {"sll",	op_func(0, 0),	op_func(0x3f, 0x3f),		"d,t,h", 0},
  {"srl",	op_func(0, 2),	op_func(0x3f, 0x3f),		"d,t,h", 0},
  {"sra",	op_func(0, 3),	op_func(0x3f, 0x3f),		"d,t,h", 0},
  {"sllv",	op_func(0, 4),	op_func(0x3f, 0x7ff),		"d,t,s", 0},
  {"srlv",	op_func(0, 6),	op_func(0x3f, 0x7ff),		"d,t,s", 0},
  {"srav",	op_func(0, 7),	op_func(0x3f, 0x7ff),		"d,t,s", 0},
  {"jr",	op_func(0, 8),	op_func(0x3f, 0x1fffff),	"s", 1},
  {"jalr",	op_func(0, 9),	op_func(0x3f, 0x1f07ff),	"d,s", 1},
  {"syscall",	op_func(0, 12),	op_func(0x3f, 0x3f),		"", 0},
  {"break",	op_func(0, 13),	op_func(0x3f, 0x3f),		"", 0},
  {"mfhi",      op_func(0, 16), op_func(0x3f, 0x03ff07ff),      "d", 0},
  {"mthi",      op_func(0, 17), op_func(0x3f, 0x1fffff),        "s", 0},
  {"mflo",      op_func(0, 18), op_func(0x3f, 0x03ff07ff),      "d", 0},
  {"mtlo",      op_func(0, 19), op_func(0x3f, 0x1fffff),        "s", 0},
  {"mult",	op_func(0, 24),	op_func(0x3f, 0xffff),		"s,t", 0},
  {"multu",	op_func(0, 25),	op_func(0x3f, 0xffff),		"s,t", 0},
  {"div",	op_func(0, 26),	op_func(0x3f, 0xffff),		"s,t", 0},
  {"divu",	op_func(0, 27),	op_func(0x3f, 0xffff),		"s,t", 0},
  {"add",	op_func(0, 32),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"addu",	op_func(0, 33),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"sub",	op_func(0, 34),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"subu",	op_func(0, 35),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"and",	op_func(0, 36),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"or",	op_func(0, 37),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"xor",	op_func(0, 38),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"nor",	op_func(0, 39),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"slt",	op_func(0, 42),	op_func(0x3f, 0x7ff),		"d,s,t", 0},
  {"sltu",	op_func(0, 43),	op_func(0x3f, 0x7ff),		"d,s,t", 0},

  {"bltz",	op_cond (1, 0),	op_cond(0x3f, 0x1f),		"s,b", 1},
  {"bgez",	op_cond (1, 1),	op_cond(0x3f, 0x1f),		"s,b", 1},
  {"bltzal",	op_cond (1, 16),op_cond(0x3f, 0x1f),		"s,b", 1},
  {"bgezal",	op_cond (1, 17),op_cond(0x3f, 0x1f),		"s,b", 1},


  {"j",		one(2),		one(0x3f),			"a", 1},
  {"jal",	one(3),		one(0x3f),			"a", 1},
  {"beq",	one(4),		one(0x3f),			"s,t,b", 1},
  {"bne",	one(5),		one(0x3f),			"s,t,b", 1},
  {"blez",	one(6),		one(0x3f) | 0x1f0000,		"s,b", 1},
  {"bgtz",	one(7),		one(0x3f) | 0x1f0000,		"s,b", 1},
  {"addi",	one(8),		one(0x3f),			"t,s,j", 0},
  {"addiu",	one(9),		one(0x3f),			"t,s,j", 0},
  {"slti",	one(10),	one(0x3f),			"t,s,j", 0},
  {"sltiu",	one(11),	one(0x3f),			"t,s,j", 0},
  {"andi",	one(12),	one(0x3f),			"t,s,i", 0},
  {"ori",	one(13),	one(0x3f),			"t,s,i", 0},
  {"xori",	one(14),	one(0x3f),			"t,s,i", 0},
	/* rs field is don't care field? */
  {"lui",	one(15),	one(0x3f),			"t,i", 0},

/* co processor 0 instruction */
  {"mfc0",	op_rs_b11 (16, 0, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"cfc0",	op_rs_b11 (16, 2, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"mtc0",	op_rs_b11 (16, 4, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"ctc0",	op_rs_b11 (16, 6, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},

  {"bc0f",	op_o16(16, 0x100),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc0f",	op_o16(16, 0x180),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc0t",	op_o16(16, 0x101),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc0t",	op_o16(16, 0x181),	op_o16(0x3f, 0x3ff),	"b", 1},

  {"tlbr",	op_rs_func(16, 0x10, 1), ~0, "", 0},
  {"tlbwi",	op_rs_func(16, 0x10, 2), ~0, "", 0},
  {"tlbwr",	op_rs_func(16, 0x10, 6), ~0, "", 0},
  {"tlbp",	op_rs_func(16, 0x10, 8), ~0, "", 0},
  {"rfe",	op_rs_func(16, 0x10, 16), ~0, "", 0},

  {"mfc1",	op_rs_b11 (17, 0, 0),	op_rs_b11(0x3f, 0x1f, 0),"t,S", 0},
  {"cfc1",	op_rs_b11 (17, 2, 0),	op_rs_b11(0x3f, 0x1f, 0),"t,S", 0},
  {"mtc1",	op_rs_b11 (17, 4, 0),	op_rs_b11(0x3f, 0x1f, 0),"t,S", 0},
  {"ctc1",	op_rs_b11 (17, 6, 0),	op_rs_b11(0x3f, 0x1f, 0),"t,S", 0},

  {"bc1f",	op_o16(17, 0x100),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc1f",	op_o16(17, 0x180),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc1t",	op_o16(17, 0x101),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc1t",	op_o16(17, 0x181),	op_o16(0x3f, 0x3ff),	"b", 1},

/* fpu instruction */
  {"add.s",	op_rs_func(17, 0x10, 0),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"add.d",	op_rs_func(17, 0x11, 0),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"sub.s",	op_rs_func(17, 0x10, 1),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"sub.d",	op_rs_func(17, 0x11, 1),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"mul.s",	op_rs_func(17, 0x10, 2),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"mul.d",	op_rs_func(17, 0x11, 2),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"div.s",	op_rs_func(17, 0x10, 3),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"div.d",	op_rs_func(17, 0x11, 3),
			op_rs_func(0x3f, 0x1f, 0x3f),	"D,S,T", 0},
  {"abs.s",	op_rs_func(17, 0x10, 5),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"abs.d",	op_rs_func(17, 0x11, 5),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"mov.s",	op_rs_func(17, 0x10, 6),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"mov.d",	op_rs_func(17, 0x11, 6),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"neg.s",	op_rs_func(17, 0x10, 7),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"neg.d",	op_rs_func(17, 0x11, 7),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.s.s",	op_rs_func(17, 0x10, 32),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.s.d",	op_rs_func(17, 0x11, 32),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.s.w",	op_rs_func(17, 0x14, 32),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.d.s",	op_rs_func(17, 0x10, 33),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.d.d",	op_rs_func(17, 0x11, 33),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.d.w",	op_rs_func(17, 0x14, 33),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.w.s",	op_rs_func(17, 0x10, 36),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"cvt.w.d",	op_rs_func(17, 0x11, 36),
			op_rs_func(0x3f, 0x1f, 0x1f003f),	"D,S", 0},
  {"c.f.s",	op_rs_func(17, 0x10, 48),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.f.d",	op_rs_func(17, 0x11, 48),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.un.s",	op_rs_func(17, 0x10, 49),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.un.d",	op_rs_func(17, 0x11, 49),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.eq.s",	op_rs_func(17, 0x10, 50),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.eq.d",	op_rs_func(17, 0x11, 50),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ueq.s",	op_rs_func(17, 0x10, 51),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ueq.d",	op_rs_func(17, 0x11, 51),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.olt.s",	op_rs_func(17, 0x10, 52),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.olt.d",	op_rs_func(17, 0x11, 52),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ult.s",	op_rs_func(17, 0x10, 53),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ult.d",	op_rs_func(17, 0x11, 53),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ole.s",	op_rs_func(17, 0x10, 54),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ole.d",	op_rs_func(17, 0x11, 54),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ule.s",	op_rs_func(17, 0x10, 55),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ule.d",	op_rs_func(17, 0x11, 55),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.sf.s",	op_rs_func(17, 0x10, 56),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.sf.d",	op_rs_func(17, 0x11, 56),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ngle.s",	op_rs_func(17, 0x10, 57),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ngle.d",	op_rs_func(17, 0x11, 57),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.seq.s",	op_rs_func(17, 0x10, 58),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.seq.d",	op_rs_func(17, 0x11, 58),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ngl.s",	op_rs_func(17, 0x10, 59),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ngl.d",	op_rs_func(17, 0x11, 59),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.lt.s",	op_rs_func(17, 0x10, 60),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.lt.d",	op_rs_func(17, 0x11, 60),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.nge.s",	op_rs_func(17, 0x10, 61),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.nge.d",	op_rs_func(17, 0x11, 61),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.le.s",	op_rs_func(17, 0x10, 62),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.le.d",	op_rs_func(17, 0x11, 62),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ngt.s",	op_rs_func(17, 0x10, 63),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},
  {"c.ngt.d",	op_rs_func(17, 0x11, 63),
			op_rs_func(0x3f, 0x1f, 0x7ff),	"S,T", 0},

/* co processor 2 instruction */
  {"mfc2",	op_rs_b11 (18, 0, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"cfc2",	op_rs_b11 (18, 2, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"mtc2",	op_rs_b11 (18, 4, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"ctc2",	op_rs_b11 (18, 6, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"bc2f",	op_o16(18, 0x100),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc2f",	op_o16(18, 0x180),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc2f",	op_o16(18, 0x101),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc2t",	op_o16(18, 0x181),	op_o16(0x3f, 0x3ff),	"b", 1},

/* co processor 3 instruction */
  {"mtc3",	op_rs_b11 (19, 0, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"cfc3",	op_rs_b11 (19, 2, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"mtc3",	op_rs_b11 (19, 4, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"ctc3",	op_rs_b11 (19, 6, 0),	op_rs_b11(0x3f, 0x1f, 0x1ffff),	"t,d", 0},
  {"bc3f",	op_o16(19, 0x100),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc3f",	op_o16(19, 0x180),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc3t",	op_o16(19, 0x101),	op_o16(0x3f, 0x3ff),	"b", 1},
  {"bc3t",	op_o16(19, 0x181),	op_o16(0x3f, 0x3ff),	"b", 1},

  {"lb",	one(32),	one(0x3f),		"t,j(s)", 0},
  {"lh",	one(33),	one(0x3f),		"t,j(s)", 0},
  {"lwl",	one(34),	one(0x3f),		"t,j(s)", 0},
  {"lw",	one(35),	one(0x3f),		"t,j(s)", 0},
  {"lbu",	one(36),	one(0x3f),		"t,j(s)", 0},
  {"lhu",	one(37),	one(0x3f),		"t,j(s)", 0},
  {"lwr",	one(38),	one(0x3f),		"t,j(s)", 0},
  {"sb",	one(40),	one(0x3f),		"t,j(s)", 0},
  {"sh",	one(41),	one(0x3f),		"t,j(s)", 0},
  {"swl",	one(42),	one(0x3f),		"t,j(s)", 0},
  {"swr",       one(46),        one(0x3f),              "t,j(s)", 0},
  {"sw",	one(43),	one(0x3f),		"t,j(s)", 0},
  {"lwc0",	one(48),	one(0x3f),		"t,j(s)", 0},
/* for fpu */
  {"lwc1",	one(49),	one(0x3f),		"T,j(s)", 0},
  {"lwc2",	one(50),	one(0x3f),		"t,j(s)", 0},
  {"lwc3",	one(51),	one(0x3f),		"t,j(s)", 0},
  {"swc0",	one(56),	one(0x3f),		"t,j(s)", 0},
/* for fpu */
  {"swc1",	one(57),	one(0x3f),		"T,j(s)", 0},
  {"swc2",	one(58),	one(0x3f),		"t,j(s)", 0},
  {"swc3",	one(59),	one(0x3f),		"t,j(s)", 0},
};
