/* ns32k-opcode.h -- Opcode table for National Semi 32k processor
   Copyright 1987, 1991, 1994, 2002 Free Software Foundation, Inc.

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
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifdef SEQUENT_COMPATABILITY
#define DEF_MODEC 20
#define DEF_MODEL 21
#endif

#ifndef DEF_MODEC
#define DEF_MODEC 20
#endif

#ifndef DEF_MODEL
#define DEF_MODEL 20
#endif
/*
   After deciding the instruction entry (via hash.c) the instruction parser
   will try to match the operands after the instruction to the required set
   given in the entry operandfield. Every operand will result in a change in
   the opcode or the addition of data to the opcode.
   The operands in the source instruction are checked for inconsistent
   semantics.

	F : 32 bit float	general form
	L : 64 bit float	    "
	B : byte		    "
	W : word		    "
	D : double-word		    "
	A : double-word		gen-address-form ie no regs, no immediate
	I : integer writeable   gen int except immediate (A + reg)
	Z : floating writeable	gen float except immediate (Z + freg)
	d : displacement
	b : displacement - pc relative addressing  acb
	p : displacement - pc relative addressing  br bcond bsr cxp
	q : quick
	i : immediate (8 bits)
	    This is not a standard ns32k operandtype, it is used to build
	    instructions like    svc arg1,arg2
	    Svc is the instruction SuperVisorCall and is sometimes used to
	    call OS-routines from usermode. Some args might be handy!
	r : register number (3 bits)
	O : setcfg instruction optionslist
	C : cinv instruction optionslist
	S : stringinstruction optionslist
	U : registerlist	save,enter
	u : registerlist	restore,exit
	M : mmu register
	P : cpu register
	g : 3:rd operand of inss or exts instruction
	G : 4:th operand of inss or exts instruction
	    Those operands are encoded in the same byte.
	    This byte is placed last in the instruction.
	f : operand of sfsr
	H : sequent-hack for bsr (Warning)

column	1 	instructions
	2 	number of bits in opcode.
	3 	number of bits in opcode explicitly
		determined by the instruction type.
	4 	opcodeseed, the number we build our opcode
		from.
	5 	operandtypes, used by operandparser.
	6 	size in bytes of immediate
*/
struct ns32k_opcode {
  const char *name;
  unsigned char opcode_id_size; /* not used by the assembler */
  unsigned char opcode_size;
  unsigned long opcode_seed;
  const char *operands;
  unsigned char im_size;	/* not used by dissassembler */
  const char *default_args;	/* default to those args when none given */
  char default_modec;		/* default to this addr-mode when ambigous
				   ie when the argument of a general addr-mode
				   is a plain constant */
  char default_model;		/* is a plain label */
};

#ifdef comment
/* This section was from the gdb version of this file. */

#ifndef ns32k_opcodeT
#define ns32k_opcodeT int
#endif /* no ns32k_opcodeT */

struct not_wot			/* ns32k opcode table: wot to do with this */
				/* particular opcode */
{
  int obits;			/* number of opcode bits */
  int ibits;			/* number of instruction bits */
  ns32k_opcodeT code;		/* op-code (may be > 8 bits!) */
  const char *args;		/* how to compile said opcode */
};

struct not			/* ns32k opcode text */
{
  const char *name;		/* opcode name: lowercase string  [key]  */
  struct not_wot detail;	/* rest of opcode table          [datum] */
};

/* Instructions look like this:

   basic instruction--1, 2, or 3 bytes
   index byte for operand A, if operand A is indexed--1 byte
   index byte for operand B, if operand B is indexed--1 byte
   addressing extension for operand A
   addressing extension for operand B
   implied operands

   Operand A is the operand listed first in the following opcode table.
   Operand B is the operand listed second in the following opcode table.
   All instructions have at most 2 general operands, so this is enough.
   The implied operands are associated with operands other than A and B.

   Each operand has a digit and a letter.

   The digit gives the position in the assembly language.  The letter,
   one of the following, tells us what kind of operand it is.  */

/* F : 32 bit float
 * L : 64 bit float
 * B : byte
 * W : word
 * D : double-word
 * I : integer not immediate
 * Z : floating not immediate
 * d : displacement
 * q : quick
 * i : immediate (8 bits)
 * r : register number (3 bits)
 * p : displacement - pc relative addressing
*/


#endif /* comment */

static const struct ns32k_opcode ns32k_opcodes[]=
{
  { "absf",	14,24,	0x35be,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "absl",	14,24,	0x34be,	"1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "absb",	14,24,	0x304e, "1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "absw",	14,24,	0x314e, "1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "absd",	14,24,	0x334e, "1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "acbb",	 7,16,	0x4c,	"2I1q3p",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "acbw",	 7,16,	0x4d,	"2I1q3p",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "acbd",	 7,16,	0x4f,	"2I1q3p",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "addf",	14,24,	0x01be,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "addl",	14,24,	0x00be, "1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "addb",	 6,16,	0x00,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "addw",	 6,16,	0x01,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "addd",	 6,16,	0x03,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "addcb",	 6,16,	0x10,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "addcw",	 6,16,	0x11,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "addcd",	 6,16,	0x13,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "addpb",	14,24,	0x3c4e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "addpw",	14,24,	0x3d4e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "addpd",	14,24,	0x3f4e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "addqb",	 7,16,	0x0c,	"2I1q",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "addqw",	 7,16,	0x0d,	"2I1q",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "addqd",	 7,16,	0x0f,	"2I1q",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "addr",	 6,16,	0x27,	"1A2I",		4,	"",	21,21	},
  { "adjspb",	11,16,	0x057c,	"1B",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "adjspw",	11,16,	0x057d,	"1W", 		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "adjspd",	11,16,	0x057f,	"1D", 		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "andb",	 6,16,	0x28,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "andw",	 6,16,	0x29,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "andd",	 6,16,	0x2b,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "ashb",	14,24,	0x044e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "ashw",	14,24,	0x054e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "ashd",	14,24,	0x074e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "beq",	 8,8,	0x0a,	"1p",		0,	"",	21,21	},
  { "bne",	 8,8,	0x1a,	"1p",		0,	"",	21,21	},
  { "bcs",	 8,8,	0x2a,	"1p",		0,	"",	21,21	},
  { "bcc",	 8,8,	0x3a,	"1p",		0,	"",	21,21	},
  { "bhi",	 8,8,	0x4a,	"1p",		0,	"",	21,21	},
  { "bls",	 8,8,	0x5a,	"1p",		0,	"",	21,21	},
  { "bgt",	 8,8,	0x6a,	"1p",		0,	"",	21,21	},
  { "ble",	 8,8,	0x7a,	"1p",		0,	"",	21,21	},
  { "bfs",	 8,8,	0x8a,	"1p",		0,	"",	21,21	},
  { "bfc",	 8,8,	0x9a,	"1p",		0,	"",	21,21	},
  { "blo",	 8,8,	0xaa,	"1p",		0,	"",	21,21	},
  { "bhs",	 8,8,	0xba,	"1p",		0,	"",	21,21	},
  { "blt",	 8,8,	0xca,	"1p",		0,	"",	21,21	},
  { "bge",	 8,8,	0xda,	"1p",		0,	"",	21,21	},
  { "but",	 8,8,	0xea,	"1p",		0,	"",	21,21	},
  { "buf",	 8,8,	0xfa,	"1p",		0,	"",	21,21	},
  { "bicb",	 6,16,	0x08,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "bicw",	 6,16,	0x09,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "bicd",	 6,16,	0x0b,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "bicpsrb",	11,16,	0x17c,	"1B",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "bicpsrw",	11,16,	0x17d,	"1W",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "bispsrb",	11,16,	0x37c,	"1B",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "bispsrw",	11,16,	0x37d,	"1W",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "bpt",	 8,8,	0xf2,	"",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "br",	 8,8,	0xea,	"1p",		0,	"",	21,21	},
#ifdef SEQUENT_COMPATABILITY
  { "bsr",	 8,8,	0x02,	"1H",		0,	"",	21,21	},
#else
  { "bsr",	 8,8,	0x02,	"1p",		0,	"",	21,21	},
#endif
  { "caseb",	11,16,	0x77c,	"1B",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "casew",	11,16,	0x77d,	"1W",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "cased",	11,16,	0x77f,	"1D",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cbitb",	14,24,	0x084e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "cbitw",	14,24,	0x094e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "cbitd",	14,24,	0x0b4e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cbitib",	14,24,	0x0c4e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "cbitiw",	14,24,	0x0d4e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "cbitid",	14,24,	0x0f4e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "checkb",	11,24,	0x0ee,	"2A3B1r",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "checkw",	11,24,	0x1ee,	"2A3W1r",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "checkd",	11,24,	0x3ee,	"2A3D1r",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cinv",	14,24,	0x271e,	"2D1C",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpf",	14,24,	0x09be,	"1F2F",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpl",	14,24,	0x08be,	"1L2L",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpb",	 6,16,	0x04,	"1B2B",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpw",	 6,16,	0x05,	"1W2W",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpd",	 6,16,	0x07,	"1D2D",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpmb",	14,24,	0x04ce,	"1A2A3b",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpmw",	14,24,	0x05ce,	"1A2A3b",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpmd",	14,24,	0x07ce,	"1A2A3b",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpqb",	 7,16,	0x1c,	"2B1q",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpqw",	 7,16,	0x1d,	"2W1q",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpqd",	 7,16,	0x1f,	"2D1q",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cmpsb",	16,24,	0x040e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "cmpsw",	16,24,	0x050e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "cmpsd",	16,24,	0x070e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "cmpst",	16,24,	0x840e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "comb",	14,24,	0x344e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "comw",	14,24,	0x354e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "comd",	14,24,	0x374e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cvtp",	11,24,	0x036e,	"2A3D1r",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "cxp",	 8,8,	0x22,	"1p",		0,	"",	21,21	},
  { "cxpd",	11,16,	0x07f,	"1A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "deib",	14,24,	0x2cce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "deiw",	14,24,	0x2dce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "deid",	14,24,	0x2fce,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "dia",	 8,8,	0xc2,	"",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "divf",	14,24,	0x21be,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "divl",	14,24,	0x20be,	"1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "divb",	14,24,	0x3cce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "divw",	14,24,	0x3dce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "divd",	14,24,	0x3fce,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "enter",	 8,8,	0x82,	"1U2d",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "exit",	 8,8,	0x92,	"1u",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "extb",	11,24,	0x02e,	"2I3B1r4d",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "extw",	11,24,	0x12e,	"2I3W1r4d",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "extd",	11,24,	0x32e,	"2I3D1r4d",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "extsb",	14,24,	0x0cce,	"1I2I4G3g",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "extsw",	14,24,	0x0dce,	"1I2I4G3g",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "extsd",	14,24,	0x0fce,	"1I2I4G3g",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "ffsb",	14,24,	0x046e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "ffsw",	14,24,	0x056e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "ffsd",	14,24,	0x076e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "flag",	 8,8,	0xd2,	"",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "floorfb",	14,24,	0x3c3e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "floorfw",	14,24,	0x3d3e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "floorfd",	14,24,	0x3f3e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "floorlb",	14,24,	0x383e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "floorlw",	14,24,	0x393e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "floorld",	14,24,	0x3b3e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "ibitb",	14,24,	0x384e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "ibitw",	14,24,	0x394e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "ibitd",	14,24,	0x3b4e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "indexb",	11,24,	0x42e,	"2B3B1r",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "indexw",	11,24,	0x52e,	"2W3W1r",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "indexd",	11,24,	0x72e,	"2D3D1r",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "insb",	11,24,	0x0ae,	"2B3I1r4d",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "insw",	11,24,	0x1ae,	"2W3I1r4d",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "insd",	11,24,	0x3ae,	"2D3I1r4d",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "inssb",	14,24,	0x08ce,	"1B2I4G3g",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "inssw",	14,24,	0x09ce,	"1W2I4G3g",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "inssd",	14,24,	0x0bce,	"1D2I4G3g",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "jsr",	11,16,	0x67f,	"1A",		4,	"",	21,21	},
  { "jump",	11,16,	0x27f,	"1A",		4,	"",	21,21	},
  { "lfsr",	19,24,	0x00f3e,"1D",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "lmr",	15,24,	0x0b1e,	"2D1M",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "lprb",	 7,16,	0x6c,	"2B1P",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "lprw",	 7,16,	0x6d,	"2W1P",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "lprd",	 7,16,	0x6f,	"2D1P",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "lshb",	14,24,	0x144e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "lshw",	14,24,	0x154e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "lshd",	14,24,	0x174e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "meib",	14,24,	0x24ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "meiw",	14,24,	0x25ce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "meid",	14,24,	0x27ce,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "modb",	14,24,	0x38ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "modw",	14,24,	0x39ce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "modd",	14,24,	0x3bce,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movf",	14,24,	0x05be,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movl",	14,24,	0x04be,	"1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "movb",	 6,16,	0x14,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movw",	 6,16,	0x15,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movd",	 6,16,	0x17,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movbf",	14,24,	0x043e,	"1B2Z",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movwf",	14,24,	0x053e,	"1W2Z",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movdf",	14,24,	0x073e,	"1D2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movbl",	14,24,	0x003e,	"1B2Z",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movwl",	14,24,	0x013e,	"1W2Z",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movdl",	14,24,	0x033e,	"1D2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movfl",	14,24,	0x1b3e,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movlf",	14,24,	0x163e,	"1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "movmb",	14,24,	0x00ce,	"1A2A3b",	1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movmw",	14,24,	0x01ce,	"1A2A3b",	2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movmd",	14,24,	0x03ce,	"1A2A3b",	4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movqb",	 7,16,	0x5c,	"2I1q",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movqw",	 7,16,	0x5d,	"2I1q",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movqd",	 7,16,	0x5f,	"2I1q",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movsb",	16,24,	0x000e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "movsw",	16,24,	0x010e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "movsd",	16,24,	0x030e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "movst",	16,24,	0x800e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "movsub",	14,24,	0x0cae,	"1A2A",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movsuw",	14,24,	0x0dae,	"1A2A",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movsud",	14,24,	0x0fae,	"1A2A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movusb",	14,24,	0x1cae,	"1A2A",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movusw",	14,24,	0x1dae,	"1A2A",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movusd",	14,24,	0x1fae,	"1A2A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "movxbd",	14,24,	0x1cce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movxwd",	14,24,	0x1dce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movxbw",	14,24,	0x10ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movzbd",	14,24,	0x18ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "movzwd",	14,24,	0x19ce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "movzbw",	14,24,	0x14ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "mulf",	14,24,	0x31be,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "mull",	14,24,	0x30be,	"1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "mulb",	14,24,	0x20ce, "1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "mulw",	14,24,	0x21ce, "1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "muld",	14,24,	0x23ce, "1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "negf",	14,24,	0x15be, "1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "negl",	14,24,	0x14be, "1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "negb",	14,24,	0x204e, "1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "negw",	14,24,	0x214e, "1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "negd",	14,24,	0x234e, "1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "nop",	 8,8,	0xa2,	"",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "notb",	14,24,	0x244e, "1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "notw",	14,24,	0x254e, "1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "notd",	14,24,	0x274e, "1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "orb",	 6,16,	0x18,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "orw",	 6,16,	0x19,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "ord",	 6,16,	0x1b,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "quob",	14,24,	0x30ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "quow",	14,24,	0x31ce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "quod",	14,24,	0x33ce,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "rdval",	19,24,	0x0031e,"1A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "remb",	14,24,	0x34ce,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "remw",	14,24,	0x35ce,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "remd",	14,24,	0x37ce,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "restore",	 8,8,	0x72,	"1u",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "ret",	 8,8,	0x12,	"1d",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "reti",	 8,8,	0x52,	"",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "rett",	 8,8,	0x42,	"1d",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "rotb",	14,24,	0x004e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "rotw",	14,24,	0x014e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "rotd",	14,24,	0x034e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "roundfb",	14,24,	0x243e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "roundfw",	14,24,	0x253e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "roundfd",	14,24,	0x273e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "roundlb",	14,24,	0x203e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "roundlw",	14,24,	0x213e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "roundld",	14,24,	0x233e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "rxp",	 8,8,	0x32,	"1d",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "seqb",	11,16,	0x3c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "seqw",	11,16,	0x3d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "seqd",	11,16,	0x3f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sneb",	11,16,	0xbc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "snew",	11,16,	0xbd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sned",	11,16,	0xbf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "scsb",	11,16,	0x13c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "scsw",	11,16,	0x13d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "scsd",	11,16,	0x13f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sccb",	11,16,	0x1bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sccw",	11,16,	0x1bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sccd",	11,16,	0x1bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "shib",	11,16,	0x23c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "shiw",	11,16,	0x23d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "shid",	11,16,	0x23f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slsb",	11,16,	0x2bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slsw",	11,16,	0x2bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slsd",	11,16,	0x2bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sgtb",	11,16,	0x33c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sgtw",	11,16,	0x33d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sgtd",	11,16,	0x33f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sleb",	11,16,	0x3bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slew",	11,16,	0x3bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sled",	11,16,	0x3bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfsb",	11,16,	0x43c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfsw",	11,16,	0x43d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfsd",	11,16,	0x43f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfcb",	11,16,	0x4bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfcw",	11,16,	0x4bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfcd",	11,16,	0x4bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slob",	11,16,	0x53c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slow",	11,16,	0x53d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "slod",	11,16,	0x53f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "shsb",	11,16,	0x5bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "shsw",	11,16,	0x5bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "shsd",	11,16,	0x5bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sltb",	11,16,	0x63c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sltw",	11,16,	0x63d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sltd",	11,16,	0x63f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sgeb",	11,16,	0x6bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sgew",	11,16,	0x6bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sged",	11,16,	0x6bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sutb",	11,16,	0x73c,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sutw",	11,16,	0x73d,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sutd",	11,16,	0x73f,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sufb",	11,16,	0x7bc,	"1B",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sufw",	11,16,	0x7bd,	"1W",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sufd",	11,16,	0x7bf,	"1D",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "save",	 8,8,	0x62,	"1U",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sbitb",    14,24,	0x184e,	"1B2A",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "sbitw",	14,24,	0x194e,	"1W2A",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "sbitd",	14,24,	0x1b4e,	"1D2A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "sbitib",	14,24,	0x1c4e,	"1B2A",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "sbitiw",	14,24,	0x1d4e,	"1W2A",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "sbitid",	14,24,	0x1f4e,	"1D2A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "setcfg",	15,24,	0x0b0e,	"1O",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "sfsr",	14,24,	0x373e,	"1f",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "skpsb",	16,24,	0x0c0e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "skpsw",	16,24,	0x0d0e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "skpsd",	16,24,	0x0f0e, "1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "skpst",	16,24,	0x8c0e,	"1S",		0,	"[]",	DEF_MODEC,DEF_MODEL	},
  { "smr",	15,24,	0x0f1e,	"2I1M",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "sprb",	 7,16,	0x2c,	"2I1P",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "sprw",	 7,16,	0x2d,	"2I1P",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "sprd",	 7,16,	0x2f,	"2I1P",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "subf",	14,24,	0x11be,	"1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "subl",	14,24,	0x10be,	"1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "subb",	 6,16,	0x20,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "subw",	 6,16,	0x21,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "subd",	 6,16,	0x23,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "subcb",	 6,16,	0x30,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "subcw",	 6,16,	0x31,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "subcd",	 6,16,	0x33,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "subpb",	14,24,	0x2c4e,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "subpw",	14,24,	0x2d4e,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "subpd",	14,24,	0x2f4e,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
#ifdef NS32K_SVC_IMMED_OPERANDS
  { "svc",	 8,8,	0xe2,	"2i1i",		1,	"",	DEF_MODEC,DEF_MODEL	}, /* not really, but some unix uses it */
#else
  { "svc",	 8,8,	0xe2,	"",		0,	"",	DEF_MODEC,DEF_MODEL	},
#endif
  { "tbitb",	 6,16,	0x34,	"1B2A",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "tbitw",	 6,16,	0x35,	"1W2A",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "tbitd",	 6,16,	0x37,	"1D2A",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "truncfb",	14,24,	0x2c3e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "truncfw",	14,24,	0x2d3e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "truncfd",	14,24,	0x2f3e,	"1F2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "trunclb",	14,24,	0x283e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "trunclw",	14,24,	0x293e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "truncld",	14,24,	0x2b3e,	"1L2I",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "wait",	 8,8,	0xb2,	"",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "wrval",	19,24,	0x0071e,"1A",		0,	"",	DEF_MODEC,DEF_MODEL	},
  { "xorb",	 6,16,	0x38,	"1B2I",		1,	"",	DEF_MODEC,DEF_MODEL	},
  { "xorw",	 6,16,	0x39,	"1W2I",		2,	"",	DEF_MODEC,DEF_MODEL	},
  { "xord",	 6,16,	0x3b,	"1D2I",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "dotf",	14,24,  0x0dfe, "1F2F",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "dotl",	14,24,  0x0cfe, "1L2L",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "logbf",	14,24,  0x15fe, "1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "logbl",	14,24,  0x14fe, "1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "polyf",	14,24,  0x09fe, "1F2F",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "polyl",	14,24,  0x08fe, "1L2L",		8,	"",	DEF_MODEC,DEF_MODEL	},
  { "scalbf",	14,24,  0x11fe, "1F2Z",		4,	"",	DEF_MODEC,DEF_MODEL	},
  { "scalbl",	14,24,  0x10fe, "1L2Z",		8,	"",	DEF_MODEC,DEF_MODEL	},
};

#define MAX_ARGS 4
#define ARG_LEN 50

