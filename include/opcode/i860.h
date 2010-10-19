/* Table of opcodes for the i860.
   Copyright 1989, 1991, 2000, 2002, 2003 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler, and GDB, the GNU disassembler.

GAS/GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS/GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS or GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street - Fifth Floor,
Boston, MA 02110-1301, USA.  */


/* Structure of an opcode table entry.  */
struct i860_opcode
{
    /* The opcode name.  */
    const char *name;

    /* Bits that must be set.  */
    unsigned long match;

    /* Bits that must not be set.  */
    unsigned long lose;

    const char *args;

    /* Nonzero if this is a possible expand-instruction.  */
    char expand;
};


enum expand_type
{
    E_MOV = 1, E_ADDR, E_U32, E_AND, E_S32, E_DELAY, XP_ONLY
};


/* All i860 opcodes are 32 bits, except for the pseudo-instructions
   and the operations utilizing a 32-bit address expression, an
   unsigned 32-bit constant, or a signed 32-bit constant.
   These opcodes are expanded into a two-instruction sequence for
   any situation where the immediate operand does not fit in 32 bits.
   In the case of the add and subtract operations the expansion is
   to a three-instruction sequence (ex: orh, or, adds).  In cases
   where the address is to be relocated, the instruction is
   expanded to handle the worse case, this could be optimized at
   the final link if the actual address were known.

   The pseudoinstructions are:  mov, fmov, pmov, nop, and fnop.
   These instructions are implemented as a one or two instruction
   sequence of other operations.

   The match component is a mask saying which bits must match a
   particular opcode in order for an instruction to be an instance
   of that opcode.

   The args component is a string containing one character
   for each operand of the instruction.

Kinds of operands:
   #    Number used by optimizer.  It is ignored.
   1    src1 integer register.
   2    src2 integer register.
   d    dest register.
   c	ctrlreg control register.
   i    16 bit immediate.
   I    16 bit immediate, aligned 2^0. (ld.b)
   J    16 bit immediate, aligned 2^1. (ld.s)
   K    16 bit immediate, aligned 2^2. (ld.l, {p}fld.l, fst.l)
   L    16 bit immediate, aligned 2^3. ({p}fld.d, fst.d)
   M    16 bit immediate, aligned 2^4. ({p}fld.q, fst.q)
   5    5 bit immediate.
   l    lbroff 26 bit PC relative immediate.
   r    sbroff 16 bit PC relative immediate.
   s	split 16 bit immediate.
   S	split 16 bit immediate, aligned 2^0. (st.b)
   T	split 16 bit immediate, aligned 2^1. (st.s)
   U	split 16 bit immediate, aligned 2^2. (st.l)
   e    src1 floating point register.
   f    src2 floating point register.
   g    dest floating point register.  */


/* The order of the opcodes in this table is significant. The assembler
   requires that all instances of the same mnemonic must be consecutive.
   If they aren't, the assembler will not function properly.
 
   The order of opcodes does not affect the disassembler.  */

static const struct i860_opcode i860_opcodes[] =
{
/* REG-Format Instructions.  */
{ "ld.c",	0x30000000, 0xcc000000, "c,d", 0 },	/* ld.c csrc2,idest */
{ "ld.b",	0x00000000, 0xfc000000, "1(2),d", 0 },	/* ld.b isrc1(isrc2),idest */
{ "ld.b",	0x04000000, 0xf8000000, "I(2),d", E_ADDR },	/* ld.b #const(isrc2),idest */
{ "ld.s",	0x10000000, 0xec000001, "1(2),d", 0 },	/* ld.s isrc1(isrc2),idest */
{ "ld.s",	0x14000000, 0xe8000001, "J(2),d", E_ADDR },	/* ld.s #const(isrc2),idest */
{ "ld.l",	0x10000001, 0xec000000, "1(2),d", 0 },	/* ld.l isrc1(isrc2),idest */
{ "ld.l",	0x14000001, 0xe8000000, "K(2),d", E_ADDR },	/* ld.l #const(isrc2),idest */

{ "st.c",	0x38000000, 0xc4000000, "1,c", 0 },	/* st.c isrc1ni,csrc2 */
{ "st.b",	0x0c000000, 0xf0000000, "1,S(2)", E_ADDR },	/* st.b isrc1ni,#const(isrc2) */
{ "st.s",	0x1c000000, 0xe0000001, "1,T(2)", E_ADDR },	/* st.s isrc1ni,#const(isrc2) */
{ "st.l",	0x1c000001, 0xe0000000, "1,U(2)", E_ADDR },	/* st.l isrc1ni,#const(isrc2) */

{ "ixfr",	0x08000000, 0xf4000000, "1,g", 0 },	/* ixfr isrc1ni,fdest */

{ "fld.l",	0x20000002, 0xdc000001, "1(2),g", 0 },	/* fld.l isrc1(isrc2),fdest */
{ "fld.l",	0x24000002, 0xd8000001, "K(2),g", E_ADDR },	/* fld.l #const(isrc2),fdest */
{ "fld.l",	0x20000003, 0xdc000000, "1(2)++,g", 0 },	/* fld.l isrc1(isrc2)++,fdest */
{ "fld.l",	0x24000003, 0xd8000000, "K(2)++,g", E_ADDR },	/* fld.l #const(isrc2)++,fdest */
{ "fld.d",	0x20000000, 0xdc000007, "1(2),g", 0 },	/* fld.d isrc1(isrc2),fdest */
{ "fld.d",	0x24000000, 0xd8000007, "L(2),g", E_ADDR },	/* fld.d #const(isrc2),fdest */
{ "fld.d",	0x20000001, 0xdc000006, "1(2)++,g", 0 },	/* fld.d isrc1(isrc2)++,fdest */
{ "fld.d",	0x24000001, 0xd8000006, "L(2)++,g", E_ADDR },	/* fld.d #const(isrc2)++,fdest */
{ "fld.q",	0x20000004, 0xdc000003, "1(2),g", 0 },	/* fld.q isrc1(isrc2),fdest */
{ "fld.q",	0x24000004, 0xd8000003, "M(2),g", E_ADDR },	/* fld.q #const(isrc2),fdest */
{ "fld.q",	0x20000005, 0xdc000002, "1(2)++,g", 0 },	/* fld.q isrc1(isrc2)++,fdest */
{ "fld.q",	0x24000005, 0xd8000002, "M(2)++,g", E_ADDR },	/* fld.q #const(isrc2)++,fdest */

{ "pfld.l",	0x60000002, 0x9c000001, "1(2),g", 0 },	/* pfld.l isrc1(isrc2),fdest */
{ "pfld.l",	0x64000002, 0x98000001, "K(2),g", E_ADDR },	/* pfld.l #const(isrc2),fdest */
{ "pfld.l",	0x60000003, 0x9c000000, "1(2)++,g", 0 },	/* pfld.l isrc1(isrc2)++,fdest */
{ "pfld.l",	0x64000003, 0x98000000, "K(2)++,g", E_ADDR },	/* pfld.l #const(isrc2)++,fdest */
{ "pfld.d",	0x60000000, 0x9c000007, "1(2),g", 0 },	/* pfld.d isrc1(isrc2),fdest */
{ "pfld.d",	0x64000000, 0x98000007, "L(2),g", E_ADDR },	/* pfld.d #const(isrc2),fdest */
{ "pfld.d",	0x60000001, 0x9c000006, "1(2)++,g", 0 },	/* pfld.d isrc1(isrc2)++,fdest */
{ "pfld.d",	0x64000001, 0x98000006, "L(2)++,g", E_ADDR },	/* pfld.d #const(isrc2)++,fdest */
{ "pfld.q",	0x60000004, 0x9c000003, "1(2),g", XP_ONLY },	/* pfld.q isrc1(isrc2),fdest */
{ "pfld.q",	0x64000004, 0x98000003, "L(2),g", XP_ONLY },	/* pfld.q #const(isrc2),fdest */
{ "pfld.q",	0x60000005, 0x9c000002, "1(2)++,g", XP_ONLY },	/* pfld.q isrc1(isrc2)++,fdest */
{ "pfld.q",	0x64000005, 0x98000002, "L(2)++,g", XP_ONLY },	/* pfld.q #const(isrc2)++,fdest */

{ "fst.l",	0x28000002, 0xd4000001, "g,1(2)", 0 },	/* fst.l fdest,isrc1(isrc2) */
{ "fst.l",	0x2c000002, 0xd0000001, "g,K(2)", E_ADDR },	/* fst.l fdest,#const(isrc2) */
{ "fst.l",	0x28000003, 0xd4000000, "g,1(2)++", 0 },	/* fst.l fdest,isrc1(isrc2)++ */
{ "fst.l",	0x2c000003, 0xd0000000, "g,K(2)++", E_ADDR },	/* fst.l fdest,#const(isrc2)++ */
{ "fst.d",	0x28000000, 0xd4000007, "g,1(2)", 0 },	/* fst.d fdest,isrc1(isrc2) */
{ "fst.d",	0x2c000000, 0xd0000007, "g,L(2)", E_ADDR },	/* fst.d fdest,#const(isrc2) */
{ "fst.d",	0x28000001, 0xd4000006, "g,1(2)++", 0 },	/* fst.d fdest,isrc1(isrc2)++ */
{ "fst.d",	0x2c000001, 0xd0000006, "g,L(2)++", E_ADDR },	/* fst.d fdest,#const(isrc2)++ */
{ "fst.q",	0x28000004, 0xd4000003, "g,1(2)", 0 },	/* fst.d fdest,isrc1(isrc2) */
{ "fst.q",	0x2c000004, 0xd0000003, "g,M(2)", E_ADDR },	/* fst.d fdest,#const(isrc2) */
{ "fst.q",	0x28000005, 0xd4000002, "g,1(2)++", 0 },	/* fst.d fdest,isrc1(isrc2)++ */
{ "fst.q",	0x2c000005, 0xd0000002, "g,M(2)++", E_ADDR },	/* fst.d fdest,#const(isrc2)++ */

{ "pst.d",	0x3c000000, 0xc0000007, "g,L(2)", E_ADDR },	/* pst.d fdest,#const(isrc2) */
{ "pst.d",	0x3c000001, 0xc0000006, "g,L(2)++", E_ADDR },	/* pst.d fdest,#const(isrc2)++ */

{ "addu",	0x80000000, 0x7c000000, "1,2,d", 0 },	/* addu isrc1,isrc2,idest */
{ "addu",	0x84000000, 0x78000000, "i,2,d", E_S32 },	/* addu #const,isrc2,idest */
{ "adds",	0x90000000, 0x6c000000, "1,2,d", 0 },	/* adds isrc1,isrc2,idest */
{ "adds",	0x94000000, 0x68000000, "i,2,d", E_S32 },	/* adds #const,isrc2,idest */
{ "subu",	0x88000000, 0x74000000, "1,2,d", 0 },	/* subu isrc1,isrc2,idest */
{ "subu",	0x8c000000, 0x70000000, "i,2,d", E_S32 },	/* subu #const,isrc2,idest */
{ "subs",	0x98000000, 0x64000000, "1,2,d", 0 },	/* subs isrc1,isrc2,idest */
{ "subs",	0x9c000000, 0x60000000, "i,2,d", E_S32 },	/* subs #const,isrc2,idest */

{ "shl",	0xa0000000, 0x5c000000, "1,2,d", 0 },	/* shl isrc1,isrc2,idest */
{ "shl",	0xa4000000, 0x58000000, "i,2,d", 0 },	/* shl #const,isrc2,idest */
{ "shr",	0xa8000000, 0x54000000, "1,2,d", 0 },	/* shr isrc1,isrc2,idest */
{ "shr",	0xac000000, 0x50000000, "i,2,d", 0 },	/* shr #const,isrc2,idest */
{ "shrd",	0xb0000000, 0x4c000000, "1,2,d", 0 },	/* shrd isrc1,isrc2,idest */
{ "shra",	0xb8000000, 0x44000000, "1,2,d", 0 },	/* shra isrc1,isrc2,idest */
{ "shra",	0xbc000000, 0x40000000, "i,2,d", 0 },	/* shra #const,isrc2,idest */

{ "mov",	0xa0000000, 0x5c00f800, "2,d", 0 },	/* shl r0,isrc2,idest */
{ "mov",	0x94000000, 0x69e00000, "i,d", E_MOV },	/* adds #const,r0,idest */
{ "nop",	0xa0000000, 0x5ffff800, "", 0 },	/* shl r0,r0,r0 */
{ "fnop",	0xb0000000, 0x4ffff800, "", 0 },	/* shrd r0,r0,r0 */

{ "trap",	0x44000000, 0xb8000000, "1,2,d", 0 },	/* trap isrc1ni,isrc2,idest */

{ "flush",	0x34000004, 0xc81f0003, "L(2)", E_ADDR },	/* flush #const(isrc2) */
{ "flush",	0x34000005, 0xc81f0002, "L(2)++", E_ADDR },	/* flush #const(isrc2)++ */

{ "and",	0xc0000000, 0x3c000000, "1,2,d", 0 },	/* and isrc1,isrc2,idest */
{ "and",	0xc4000000, 0x38000000, "i,2,d", E_AND },	/* and #const,isrc2,idest */
{ "andh",	0xcc000000, 0x30000000, "i,2,d", 0 },	/* andh #const,isrc2,idest */
{ "andnot",	0xd0000000, 0x2c000000, "1,2,d", 0 },	/* andnot isrc1,isrc2,idest */
{ "andnot",	0xd4000000, 0x28000000, "i,2,d", E_U32 },	/* andnot #const,isrc2,idest */
{ "andnoth",	0xdc000000, 0x20000000, "i,2,d", 0 },	/* andnoth #const,isrc2,idest */
{ "or",		0xe0000000, 0x1c000000, "1,2,d", 0 },	/* or isrc1,isrc2,idest */
{ "or",		0xe4000000, 0x18000000, "i,2,d", E_U32 },	/* or #const,isrc2,idest */
{ "orh",	0xec000000, 0x10000000, "i,2,d", 0 },	/* orh #const,isrc2,idest */
{ "xor",	0xf0000000, 0x0c000000, "1,2,d", 0 },	/* xor isrc1,isrc2,idest */
{ "xor",	0xf4000000, 0x08000000, "i,2,d", E_U32 },	/* xor #const,isrc2,idest */
{ "xorh",	0xfc000000, 0x00000000, "i,2,d", 0 },	/* xorh #const,isrc2,idest */

{ "bte",	0x58000000, 0xa4000000, "1,2,r", 0 },	/* bte isrc1s,isrc2,sbroff */
{ "bte",	0x5c000000, 0xa0000000, "5,2,r", 0 },	/* bte #const5,isrc2,sbroff */
{ "btne",	0x50000000, 0xac000000, "1,2,r", 0 },	/* btne isrc1s,isrc2,sbroff */
{ "btne",	0x54000000, 0xa8000000, "5,2,r", 0 },	/* btne #const5,isrc2,sbroff */
{ "bla",	0xb4000000, 0x48000000, "1,2,r", E_DELAY },	/* bla isrc1s,isrc2,sbroff */
{ "bri",	0x40000000, 0xbc000000, "1", E_DELAY },	/* bri isrc1ni */

/* Core Escape Instruction Format */
{ "lock",	0x4c000001, 0xb000001e, "", 0 },	/* lock set BL in dirbase */
{ "calli",	0x4c000002, 0xb000001d, "1", E_DELAY },	/* calli isrc1ni */
{ "intovr",	0x4c000004, 0xb000001b, "", 0 },	/* intovr trap on integer overflow */
{ "unlock",	0x4c000007, 0xb0000018, "", 0 },	/* unlock clear BL in dirbase */
{ "ldio.l",	0x4c000408, 0xb00003f7, "2,d", XP_ONLY },	/* ldio.l isrc2,idest */
{ "ldio.s",	0x4c000208, 0xb00005f7, "2,d", XP_ONLY },	/* ldio.s isrc2,idest */
{ "ldio.b",	0x4c000008, 0xb00007f7, "2,d", XP_ONLY },	/* ldio.b isrc2,idest */
{ "stio.l",	0x4c000409, 0xb00003f6, "1,2", XP_ONLY },	/* stio.l isrc1ni,isrc2 */
{ "stio.s",	0x4c000209, 0xb00005f6, "1,2", XP_ONLY },	/* stio.s isrc1ni,isrc2 */
{ "stio.b",	0x4c000009, 0xb00007f6, "1,2", XP_ONLY },	/* stio.b isrc1ni,isrc2 */
{ "ldint.l",	0x4c00040a, 0xb00003f5, "2,d", XP_ONLY },	/* ldint.l isrc2,idest */
{ "ldint.s",	0x4c00020a, 0xb00005f5, "2,d", XP_ONLY },	/* ldint.s isrc2,idest */
{ "ldint.b",	0x4c00000a, 0xb00007f5, "2,d", XP_ONLY },	/* ldint.b isrc2,idest */
{ "scyc.b",	0x4c00000b, 0xb00007f4, "2", XP_ONLY },		/* scyc.b isrc2 */

/* CTRL-Format Instructions */
{ "br",		0x68000000, 0x94000000, "l", E_DELAY },	/* br lbroff */
{ "call",	0x6c000000, 0x90000000, "l", E_DELAY },	/* call lbroff */
{ "bc",		0x70000000, 0x8c000000, "l", 0 },	/* bc lbroff */
{ "bc.t",	0x74000000, 0x88000000, "l", E_DELAY },	/* bc.t lbroff */
{ "bnc",	0x78000000, 0x84000000, "l", 0 },	/* bnc lbroff */
{ "bnc.t",	0x7c000000, 0x80000000, "l", E_DELAY },	/* bnc.t lbroff */

/* Floating Point Escape Instruction Format - pfam.p fsrc1,fsrc2,fdest.  */
{ "r2p1.ss",	0x48000400, 0xb40001ff, "e,f,g", 0 },
{ "r2p1.sd",	0x48000480, 0xb400017f, "e,f,g", 0 },
{ "r2p1.dd",	0x48000580, 0xb400007f, "e,f,g", 0 },
{ "r2pt.ss",	0x48000401, 0xb40001fe, "e,f,g", 0 },
{ "r2pt.sd",	0x48000481, 0xb400017e, "e,f,g", 0 },
{ "r2pt.dd",	0x48000581, 0xb400007e, "e,f,g", 0 },
{ "r2ap1.ss",	0x48000402, 0xb40001fd, "e,f,g", 0 },
{ "r2ap1.sd",	0x48000482, 0xb400017d, "e,f,g", 0 },
{ "r2ap1.dd",	0x48000582, 0xb400007d, "e,f,g", 0 },
{ "r2apt.ss",	0x48000403, 0xb40001fc, "e,f,g", 0 },
{ "r2apt.sd",	0x48000483, 0xb400017c, "e,f,g", 0 },
{ "r2apt.dd",	0x48000583, 0xb400007c, "e,f,g", 0 },
{ "i2p1.ss",	0x48000404, 0xb40001fb, "e,f,g", 0 },
{ "i2p1.sd",	0x48000484, 0xb400017b, "e,f,g", 0 },
{ "i2p1.dd",	0x48000584, 0xb400007b, "e,f,g", 0 },
{ "i2pt.ss",	0x48000405, 0xb40001fa, "e,f,g", 0 },
{ "i2pt.sd",	0x48000485, 0xb400017a, "e,f,g", 0 },
{ "i2pt.dd",	0x48000585, 0xb400007a, "e,f,g", 0 },
{ "i2ap1.ss",	0x48000406, 0xb40001f9, "e,f,g", 0 },
{ "i2ap1.sd",	0x48000486, 0xb4000179, "e,f,g", 0 },
{ "i2ap1.dd",	0x48000586, 0xb4000079, "e,f,g", 0 },
{ "i2apt.ss",	0x48000407, 0xb40001f8, "e,f,g", 0 },
{ "i2apt.sd",	0x48000487, 0xb4000178, "e,f,g", 0 },
{ "i2apt.dd",	0x48000587, 0xb4000078, "e,f,g", 0 },
{ "rat1p2.ss",	0x48000408, 0xb40001f7, "e,f,g", 0 },
{ "rat1p2.sd",	0x48000488, 0xb4000177, "e,f,g", 0 },
{ "rat1p2.dd",	0x48000588, 0xb4000077, "e,f,g", 0 },
{ "m12apm.ss",	0x48000409, 0xb40001f6, "e,f,g", 0 },
{ "m12apm.sd",	0x48000489, 0xb4000176, "e,f,g", 0 },
{ "m12apm.dd",	0x48000589, 0xb4000076, "e,f,g", 0 },
{ "ra1p2.ss",	0x4800040a, 0xb40001f5, "e,f,g", 0 },
{ "ra1p2.sd",	0x4800048a, 0xb4000175, "e,f,g", 0 },
{ "ra1p2.dd",	0x4800058a, 0xb4000075, "e,f,g", 0 },
{ "m12ttpa.ss",	0x4800040b, 0xb40001f4, "e,f,g", 0 },
{ "m12ttpa.sd",	0x4800048b, 0xb4000174, "e,f,g", 0 },
{ "m12ttpa.dd",	0x4800058b, 0xb4000074, "e,f,g", 0 },
{ "iat1p2.ss",	0x4800040c, 0xb40001f3, "e,f,g", 0 },
{ "iat1p2.sd",	0x4800048c, 0xb4000173, "e,f,g", 0 },
{ "iat1p2.dd",	0x4800058c, 0xb4000073, "e,f,g", 0 },
{ "m12tpm.ss",	0x4800040d, 0xb40001f2, "e,f,g", 0 },
{ "m12tpm.sd",	0x4800048d, 0xb4000172, "e,f,g", 0 },
{ "m12tpm.dd",	0x4800058d, 0xb4000072, "e,f,g", 0 },
{ "ia1p2.ss",	0x4800040e, 0xb40001f1, "e,f,g", 0 },
{ "ia1p2.sd",	0x4800048e, 0xb4000171, "e,f,g", 0 },
{ "ia1p2.dd",	0x4800058e, 0xb4000071, "e,f,g", 0 },
{ "m12tpa.ss",	0x4800040f, 0xb40001f0, "e,f,g", 0 },
{ "m12tpa.sd",	0x4800048f, 0xb4000170, "e,f,g", 0 },
{ "m12tpa.dd",	0x4800058f, 0xb4000070, "e,f,g", 0 },

/* Floating Point Escape Instruction Format - pfsm.p fsrc1,fsrc2,fdest.  */
{ "r2s1.ss",	0x48000410, 0xb40001ef, "e,f,g", 0 },
{ "r2s1.sd",	0x48000490, 0xb400016f, "e,f,g", 0 },
{ "r2s1.dd",	0x48000590, 0xb400006f, "e,f,g", 0 },
{ "r2st.ss",	0x48000411, 0xb40001ee, "e,f,g", 0 },
{ "r2st.sd",	0x48000491, 0xb400016e, "e,f,g", 0 },
{ "r2st.dd",	0x48000591, 0xb400006e, "e,f,g", 0 },
{ "r2as1.ss",	0x48000412, 0xb40001ed, "e,f,g", 0 },
{ "r2as1.sd",	0x48000492, 0xb400016d, "e,f,g", 0 },
{ "r2as1.dd",	0x48000592, 0xb400006d, "e,f,g", 0 },
{ "r2ast.ss",	0x48000413, 0xb40001ec, "e,f,g", 0 },
{ "r2ast.sd",	0x48000493, 0xb400016c, "e,f,g", 0 },
{ "r2ast.dd",	0x48000593, 0xb400006c, "e,f,g", 0 },
{ "i2s1.ss",	0x48000414, 0xb40001eb, "e,f,g", 0 },
{ "i2s1.sd",	0x48000494, 0xb400016b, "e,f,g", 0 },
{ "i2s1.dd",	0x48000594, 0xb400006b, "e,f,g", 0 },
{ "i2st.ss",	0x48000415, 0xb40001ea, "e,f,g", 0 },
{ "i2st.sd",	0x48000495, 0xb400016a, "e,f,g", 0 },
{ "i2st.dd",	0x48000595, 0xb400006a, "e,f,g", 0 },
{ "i2as1.ss",	0x48000416, 0xb40001e9, "e,f,g", 0 },
{ "i2as1.sd",	0x48000496, 0xb4000169, "e,f,g", 0 },
{ "i2as1.dd",	0x48000596, 0xb4000069, "e,f,g", 0 },
{ "i2ast.ss",	0x48000417, 0xb40001e8, "e,f,g", 0 },
{ "i2ast.sd",	0x48000497, 0xb4000168, "e,f,g", 0 },
{ "i2ast.dd",	0x48000597, 0xb4000068, "e,f,g", 0 },
{ "rat1s2.ss",	0x48000418, 0xb40001e7, "e,f,g", 0 },
{ "rat1s2.sd",	0x48000498, 0xb4000167, "e,f,g", 0 },
{ "rat1s2.dd",	0x48000598, 0xb4000067, "e,f,g", 0 },
{ "m12asm.ss",	0x48000419, 0xb40001e6, "e,f,g", 0 },
{ "m12asm.sd",	0x48000499, 0xb4000166, "e,f,g", 0 },
{ "m12asm.dd",	0x48000599, 0xb4000066, "e,f,g", 0 },
{ "ra1s2.ss",	0x4800041a, 0xb40001e5, "e,f,g", 0 },
{ "ra1s2.sd",	0x4800049a, 0xb4000165, "e,f,g", 0 },
{ "ra1s2.dd",	0x4800059a, 0xb4000065, "e,f,g", 0 },
{ "m12ttsa.ss",	0x4800041b, 0xb40001e4, "e,f,g", 0 },
{ "m12ttsa.sd",	0x4800049b, 0xb4000164, "e,f,g", 0 },
{ "m12ttsa.dd",	0x4800059b, 0xb4000064, "e,f,g", 0 },
{ "iat1s2.ss",	0x4800041c, 0xb40001e3, "e,f,g", 0 },
{ "iat1s2.sd",	0x4800049c, 0xb4000163, "e,f,g", 0 },
{ "iat1s2.dd",	0x4800059c, 0xb4000063, "e,f,g", 0 },
{ "m12tsm.ss",	0x4800041d, 0xb40001e2, "e,f,g", 0 },
{ "m12tsm.sd",	0x4800049d, 0xb4000162, "e,f,g", 0 },
{ "m12tsm.dd",	0x4800059d, 0xb4000062, "e,f,g", 0 },
{ "ia1s2.ss",	0x4800041e, 0xb40001e1, "e,f,g", 0 },
{ "ia1s2.sd",	0x4800049e, 0xb4000161, "e,f,g", 0 },
{ "ia1s2.dd",	0x4800059e, 0xb4000061, "e,f,g", 0 },
{ "m12tsa.ss",	0x4800041f, 0xb40001e0, "e,f,g", 0 },
{ "m12tsa.sd",	0x4800049f, 0xb4000160, "e,f,g", 0 },
{ "m12tsa.dd",	0x4800059f, 0xb4000060, "e,f,g", 0 },

/* Floating Point Escape Instruction Format - pfmam.p fsrc1,fsrc2,fdest.  */
{ "mr2p1.ss",	0x48000000, 0xb40005ff, "e,f,g", 0 },
{ "mr2p1.sd",	0x48000080, 0xb400057f, "e,f,g", 0 },
{ "mr2p1.dd",	0x48000180, 0xb400047f, "e,f,g", 0 },
{ "mr2pt.ss",	0x48000001, 0xb40005fe, "e,f,g", 0 },
{ "mr2pt.sd",	0x48000081, 0xb400057e, "e,f,g", 0 },
{ "mr2pt.dd",	0x48000181, 0xb400047e, "e,f,g", 0 },
{ "mr2mp1.ss",	0x48000002, 0xb40005fd, "e,f,g", 0 },
{ "mr2mp1.sd",	0x48000082, 0xb400057d, "e,f,g", 0 },
{ "mr2mp1.dd",	0x48000182, 0xb400047d, "e,f,g", 0 },
{ "mr2mpt.ss",	0x48000003, 0xb40005fc, "e,f,g", 0 },
{ "mr2mpt.sd",	0x48000083, 0xb400057c, "e,f,g", 0 },
{ "mr2mpt.dd",	0x48000183, 0xb400047c, "e,f,g", 0 },
{ "mi2p1.ss",	0x48000004, 0xb40005fb, "e,f,g", 0 },
{ "mi2p1.sd",	0x48000084, 0xb400057b, "e,f,g", 0 },
{ "mi2p1.dd",	0x48000184, 0xb400047b, "e,f,g", 0 },
{ "mi2pt.ss",	0x48000005, 0xb40005fa, "e,f,g", 0 },
{ "mi2pt.sd",	0x48000085, 0xb400057a, "e,f,g", 0 },
{ "mi2pt.dd",	0x48000185, 0xb400047a, "e,f,g", 0 },
{ "mi2mp1.ss",	0x48000006, 0xb40005f9, "e,f,g", 0 },
{ "mi2mp1.sd",	0x48000086, 0xb4000579, "e,f,g", 0 },
{ "mi2mp1.dd",	0x48000186, 0xb4000479, "e,f,g", 0 },
{ "mi2mpt.ss",	0x48000007, 0xb40005f8, "e,f,g", 0 },
{ "mi2mpt.sd",	0x48000087, 0xb4000578, "e,f,g", 0 },
{ "mi2mpt.dd",	0x48000187, 0xb4000478, "e,f,g", 0 },
{ "mrmt1p2.ss",	0x48000008, 0xb40005f7, "e,f,g", 0 },
{ "mrmt1p2.sd",	0x48000088, 0xb4000577, "e,f,g", 0 },
{ "mrmt1p2.dd",	0x48000188, 0xb4000477, "e,f,g", 0 },
{ "mm12mpm.ss",	0x48000009, 0xb40005f6, "e,f,g", 0 },
{ "mm12mpm.sd",	0x48000089, 0xb4000576, "e,f,g", 0 },
{ "mm12mpm.dd",	0x48000189, 0xb4000476, "e,f,g", 0 },
{ "mrm1p2.ss",	0x4800000a, 0xb40005f5, "e,f,g", 0 },
{ "mrm1p2.sd",	0x4800008a, 0xb4000575, "e,f,g", 0 },
{ "mrm1p2.dd",	0x4800018a, 0xb4000475, "e,f,g", 0 },
{ "mm12ttpm.ss",0x4800000b, 0xb40005f4, "e,f,g", 0 },
{ "mm12ttpm.sd",0x4800008b, 0xb4000574, "e,f,g", 0 },
{ "mm12ttpm.dd",0x4800018b, 0xb4000474, "e,f,g", 0 },
{ "mimt1p2.ss",	0x4800000c, 0xb40005f3, "e,f,g", 0 },
{ "mimt1p2.sd",	0x4800008c, 0xb4000573, "e,f,g", 0 },
{ "mimt1p2.dd",	0x4800018c, 0xb4000473, "e,f,g", 0 },
{ "mm12tpm.ss",	0x4800000d, 0xb40005f2, "e,f,g", 0 },
{ "mm12tpm.sd",	0x4800008d, 0xb4000572, "e,f,g", 0 },
{ "mm12tpm.dd",	0x4800018d, 0xb4000472, "e,f,g", 0 },
{ "mim1p2.ss",	0x4800000e, 0xb40005f1, "e,f,g", 0 },
{ "mim1p2.sd",	0x4800008e, 0xb4000571, "e,f,g", 0 },
{ "mim1p2.dd",	0x4800018e, 0xb4000471, "e,f,g", 0 },

/* Floating Point Escape Instruction Format - pfmsm.p fsrc1,fsrc2,fdest.  */
{ "mr2s1.ss",	0x48000010, 0xb40005ef, "e,f,g", 0 },
{ "mr2s1.sd",	0x48000090, 0xb400056f, "e,f,g", 0 },
{ "mr2s1.dd",	0x48000190, 0xb400046f, "e,f,g", 0 },
{ "mr2st.ss",	0x48000011, 0xb40005ee, "e,f,g", 0 },
{ "mr2st.sd",	0x48000091, 0xb400056e, "e,f,g", 0 },
{ "mr2st.dd",	0x48000191, 0xb400046e, "e,f,g", 0 },
{ "mr2ms1.ss",	0x48000012, 0xb40005ed, "e,f,g", 0 },
{ "mr2ms1.sd",	0x48000092, 0xb400056d, "e,f,g", 0 },
{ "mr2ms1.dd",	0x48000192, 0xb400046d, "e,f,g", 0 },
{ "mr2mst.ss",	0x48000013, 0xb40005ec, "e,f,g", 0 },
{ "mr2mst.sd",	0x48000093, 0xb400056c, "e,f,g", 0 },
{ "mr2mst.dd",	0x48000193, 0xb400046c, "e,f,g", 0 },
{ "mi2s1.ss",	0x48000014, 0xb40005eb, "e,f,g", 0 },
{ "mi2s1.sd",	0x48000094, 0xb400056b, "e,f,g", 0 },
{ "mi2s1.dd",	0x48000194, 0xb400046b, "e,f,g", 0 },
{ "mi2st.ss",	0x48000015, 0xb40005ea, "e,f,g", 0 },
{ "mi2st.sd",	0x48000095, 0xb400056a, "e,f,g", 0 },
{ "mi2st.dd",	0x48000195, 0xb400046a, "e,f,g", 0 },
{ "mi2ms1.ss",	0x48000016, 0xb40005e9, "e,f,g", 0 },
{ "mi2ms1.sd",	0x48000096, 0xb4000569, "e,f,g", 0 },
{ "mi2ms1.dd",	0x48000196, 0xb4000469, "e,f,g", 0 },
{ "mi2mst.ss",	0x48000017, 0xb40005e8, "e,f,g", 0 },
{ "mi2mst.sd",	0x48000097, 0xb4000568, "e,f,g", 0 },
{ "mi2mst.dd",	0x48000197, 0xb4000468, "e,f,g", 0 },
{ "mrmt1s2.ss",	0x48000018, 0xb40005e7, "e,f,g", 0 },
{ "mrmt1s2.sd",	0x48000098, 0xb4000567, "e,f,g", 0 },
{ "mrmt1s2.dd",	0x48000198, 0xb4000467, "e,f,g", 0 },
{ "mm12msm.ss",	0x48000019, 0xb40005e6, "e,f,g", 0 },
{ "mm12msm.sd",	0x48000099, 0xb4000566, "e,f,g", 0 },
{ "mm12msm.dd",	0x48000199, 0xb4000466, "e,f,g", 0 },
{ "mrm1s2.ss",	0x4800001a, 0xb40005e5, "e,f,g", 0 },
{ "mrm1s2.sd",	0x4800009a, 0xb4000565, "e,f,g", 0 },
{ "mrm1s2.dd",	0x4800019a, 0xb4000465, "e,f,g", 0 },
{ "mm12ttsm.ss",0x4800001b, 0xb40005e4, "e,f,g", 0 },
{ "mm12ttsm.sd",0x4800009b, 0xb4000564, "e,f,g", 0 },
{ "mm12ttsm.dd",0x4800019b, 0xb4000464, "e,f,g", 0 },
{ "mimt1s2.ss",	0x4800001c, 0xb40005e3, "e,f,g", 0 },
{ "mimt1s2.sd",	0x4800009c, 0xb4000563, "e,f,g", 0 },
{ "mimt1s2.dd",	0x4800019c, 0xb4000463, "e,f,g", 0 },
{ "mm12tsm.ss",	0x4800001d, 0xb40005e2, "e,f,g", 0 },
{ "mm12tsm.sd",	0x4800009d, 0xb4000562, "e,f,g", 0 },
{ "mm12tsm.dd",	0x4800019d, 0xb4000462, "e,f,g", 0 },
{ "mim1s2.ss",	0x4800001e, 0xb40005e1, "e,f,g", 0 },
{ "mim1s2.sd",	0x4800009e, 0xb4000561, "e,f,g", 0 },
{ "mim1s2.dd",	0x4800019e, 0xb4000461, "e,f,g", 0 },

{ "fmul.ss",	0x48000020, 0xb40005df, "e,f,g", 0 },	/* fmul.p fsrc1,fsrc2,fdest */
{ "fmul.sd",	0x480000a0, 0xb400055f, "e,f,g", 0 },	/* fmul.p fsrc1,fsrc2,fdest */
{ "fmul.dd",	0x480001a0, 0xb400045f, "e,f,g", 0 },	/* fmul.p fsrc1,fsrc2,fdest */
{ "pfmul.ss",	0x48000420, 0xb40001df, "e,f,g", 0 },	/* pfmul.p fsrc1,fsrc2,fdest */
{ "pfmul.sd",	0x480004a0, 0xb400015f, "e,f,g", 0 },	/* pfmul.p fsrc1,fsrc2,fdest */
{ "pfmul.dd",	0x480005a0, 0xb400005f, "e,f,g", 0 },	/* pfmul.p fsrc1,fsrc2,fdest */
{ "pfmul3.dd",	0x480005a4, 0xb400005b, "e,f,g", 0 },	/* pfmul3.p fsrc1,fsrc2,fdest */
{ "fmlow.dd",	0x480001a1, 0xb400045e, "e,f,g", 0 },	/* fmlow.dd fsrc1,fsrc2,fdest */
{ "frcp.ss",	0x48000022, 0xb40005dd, "f,g", 0 },	/* frcp.p fsrc2,fdest */
{ "frcp.sd",	0x480000a2, 0xb400055d, "f,g", 0 },	/* frcp.p fsrc2,fdest */
{ "frcp.dd",	0x480001a2, 0xb400045d, "f,g", 0 },	/* frcp.p fsrc2,fdest */
{ "frsqr.ss",	0x48000023, 0xb40005dc, "f,g", 0 },	/* frsqr.p fsrc2,fdest */
{ "frsqr.sd",	0x480000a3, 0xb400055c, "f,g", 0 },	/* frsqr.p fsrc2,fdest */
{ "frsqr.dd",	0x480001a3, 0xb400045c, "f,g", 0 },	/* frsqr.p fsrc2,fdest */
{ "fadd.ss",	0x48000030, 0xb40005cf, "e,f,g", 0 },	/* fadd.p fsrc1,fsrc2,fdest */
{ "fadd.sd",	0x480000b0, 0xb400054f, "e,f,g", 0 },	/* fadd.p fsrc1,fsrc2,fdest */
{ "fadd.dd",	0x480001b0, 0xb400044f, "e,f,g", 0 },	/* fadd.p fsrc1,fsrc2,fdest */
{ "pfadd.ss",	0x48000430, 0xb40001cf, "e,f,g", 0 },	/* pfadd.p fsrc1,fsrc2,fdest */
{ "pfadd.sd",	0x480004b0, 0xb400014f, "e,f,g", 0 },	/* pfadd.p fsrc1,fsrc2,fdest */
{ "pfadd.dd",	0x480005b0, 0xb400004f, "e,f,g", 0 },	/* pfadd.p fsrc1,fsrc2,fdest */
{ "fsub.ss",	0x48000031, 0xb40005ce, "e,f,g", 0 },	/* fsub.p fsrc1,fsrc2,fdest */
{ "fsub.sd",	0x480000b1, 0xb400054e, "e,f,g", 0 },	/* fsub.p fsrc1,fsrc2,fdest */
{ "fsub.dd",	0x480001b1, 0xb400044e, "e,f,g", 0 },	/* fsub.p fsrc1,fsrc2,fdest */
{ "pfsub.ss",	0x48000431, 0xb40001ce, "e,f,g", 0 },	/* pfsub.p fsrc1,fsrc2,fdest */
{ "pfsub.sd",	0x480004b1, 0xb400014e, "e,f,g", 0 },	/* pfsub.p fsrc1,fsrc2,fdest */
{ "pfsub.dd",	0x480005b1, 0xb400004e, "e,f,g", 0 },	/* pfsub.p fsrc1,fsrc2,fdest */
{ "fix.sd",	0x480000b2, 0xb400054d, "e,g", 0 },	/* fix.p fsrc1,fdest */
{ "fix.dd",	0x480001b2, 0xb400044d, "e,g", 0 },	/* fix.p fsrc1,fdest */
{ "pfix.sd",	0x480004b2, 0xb400014d, "e,g", 0 },	/* pfix.p fsrc1,fdest */
{ "pfix.dd",	0x480005b2, 0xb400004d, "e,g", 0 },	/* pfix.p fsrc1,fdest */
{ "famov.ss",	0x48000033, 0xb40005cc, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "famov.ds",	0x48000133, 0xb40004cc, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "famov.sd",	0x480000b3, 0xb400054c, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "famov.dd",	0x480001b3, 0xb400044c, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "pfamov.ss",	0x48000433, 0xb40001cc, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
{ "pfamov.ds",	0x48000533, 0xb40000cc, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
{ "pfamov.sd",	0x480004b3, 0xb400014c, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
{ "pfamov.dd",	0x480005b3, 0xb400004c, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
/* Opcode pfgt has R bit cleared; pfle has R bit set.  */
{ "pfgt.ss",	0x48000434, 0xb40001cb, "e,f,g", 0 },	/* pfgt.p fsrc1,fsrc2,fdest */
{ "pfgt.dd",	0x48000534, 0xb40000cb, "e,f,g", 0 },	/* pfgt.p fsrc1,fsrc2,fdest */
/* Opcode pfgt has R bit cleared; pfle has R bit set.  */
{ "pfle.ss",	0x480004b4, 0xb400014b, "e,f,g", 0 },	/* pfle.p fsrc1,fsrc2,fdest */
{ "pfle.dd",	0x480005b4, 0xb400004b, "e,f,g", 0 },	/* pfle.p fsrc1,fsrc2,fdest */
{ "pfeq.ss",	0x48000435, 0xb40001ca, "e,f,g", 0 },	/* pfeq.p fsrc1,fsrc2,fdest */
{ "pfeq.dd",	0x48000535, 0xb40000ca, "e,f,g", 0 },	/* pfeq.p fsrc1,fsrc2,fdest */
{ "ftrunc.sd",	0x480000ba, 0xb4000545, "e,g", 0 },	/* ftrunc.p fsrc1,fdest */
{ "ftrunc.dd",	0x480001ba, 0xb4000445, "e,g", 0 },	/* ftrunc.p fsrc1,fdest */
{ "pftrunc.sd",	0x480004ba, 0xb4000145, "e,g", 0 },	/* pftrunc.p fsrc1,fdest */
{ "pftrunc.dd",	0x480005ba, 0xb4000045, "e,g", 0 },	/* pftrunc.p fsrc1,fdest */
{ "fxfr",	0x48000040, 0xb40005bf, "e,d", 0 },	/* fxfr fsrc1,idest */
{ "fiadd.ss",	0x48000049, 0xb40005b6, "e,f,g", 0 },	/* fiadd.w fsrc1,fsrc2,fdest */
{ "fiadd.dd",	0x480001c9, 0xb4000436, "e,f,g", 0 },	/* fiadd.w fsrc1,fsrc2,fdest */
{ "pfiadd.ss",	0x48000449, 0xb40001b6, "e,f,g", 0 },	/* pfiadd.w fsrc1,fsrc2,fdest */
{ "pfiadd.dd",	0x480005c9, 0xb4000036, "e,f,g", 0 },	/* pfiadd.w fsrc1,fsrc2,fdest */
{ "fisub.ss",	0x4800004d, 0xb40005b2, "e,f,g", 0 },	/* fisub.w fsrc1,fsrc2,fdest */
{ "fisub.dd",	0x480001cd, 0xb4000432, "e,f,g", 0 },	/* fisub.w fsrc1,fsrc2,fdest */
{ "pfisub.ss",	0x4800044d, 0xb40001b2, "e,f,g", 0 },	/* pfisub.w fsrc1,fsrc2,fdest */
{ "pfisub.dd",	0x480005cd, 0xb4000032, "e,f,g", 0 },	/* pfisub.w fsrc1,fsrc2,fdest */
{ "fzchkl",	0x480001d7, 0xb4000428, "e,f,g", 0 },	/* fzchkl fsrc1,fsrc2,fdest */
{ "pfzchkl",	0x480005d7, 0xb4000028, "e,f,g", 0 },	/* pfzchkl fsrc1,fsrc2,fdest */
{ "fzchks",	0x480001df, 0xb4000420, "e,f,g", 0 },	/* fzchks fsrc1,fsrc2,fdest */
{ "pfzchks",	0x480005df, 0xb4000020, "e,f,g", 0 },	/* pfzchks fsrc1,fsrc2,fdest */
{ "faddp",	0x480001d0, 0xb400042f, "e,f,g", 0 },	/* faddp fsrc1,fsrc2,fdest */
{ "pfaddp",	0x480005d0, 0xb400002f, "e,f,g", 0 },	/* pfaddp fsrc1,fsrc2,fdest */
{ "faddz",	0x480001d1, 0xb400042e, "e,f,g", 0 },	/* faddz fsrc1,fsrc2,fdest */
{ "pfaddz",	0x480005d1, 0xb400002e, "e,f,g", 0 },	/* pfaddz fsrc1,fsrc2,fdest */
{ "form",	0x480001da, 0xb4000425, "e,g", 0 },	/* form fsrc1,fdest */
{ "pform",	0x480005da, 0xb4000025, "e,g", 0 },	/* pform fsrc1,fdest */

/* Floating point pseudo-instructions.  */
{ "fmov.ss",	0x48000049, 0xb7e005b6, "e,g", 0 },	/* fiadd.ss fsrc1,f0,fdest */
{ "fmov.dd",	0x480001c9, 0xb7e00436, "e,g", 0 },	/* fiadd.dd fsrc1,f0,fdest */
{ "fmov.sd",	0x480000b3, 0xb400054c, "e,g", 0 },	/* famov.sd fsrc1,fdest */
{ "fmov.ds",	0x48000133, 0xb40004cc, "e,g", 0 },	/* famov.ds fsrc1,fdest */
{ "pfmov.ds",	0x48000533, 0xb40000cc, "e,g", 0 },	/* pfamov.ds fsrc1,fdest */
{ "pfmov.dd",	0x480005c9, 0xb7e00036, "e,g", 0 },	/* pfiadd.dd fsrc1,f0,fdest */
{ 0, 0, 0, 0, 0 },

};

#define NUMOPCODES ((sizeof i860_opcodes)/(sizeof i860_opcodes[0]))


