/* Table of opcodes for the i860.
   Copyright (C) 1989 Free Software Foundation, Inc.

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
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if !defined(__STDC__) && !defined(const)
#define const
#endif

/*
 * Structure of an opcode table entry.
 */
struct i860_opcode
{
    const char *name;
    unsigned long match;	/* Bits that must be set.  */
    unsigned long lose;	/* Bits that must not be set.  */
    const char *args;
    /* Nonzero if this is a possible expand-instruction.  */
    char expand;
};

enum expand_type
{
    E_MOV = 1, E_ADDR, E_U32, E_AND, E_S32, E_DELAY
};

/*
   All i860 opcodes are 32 bits, except for the pseudoinstructions
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
   I    16 bit immediate, aligned.
   5    5 bit immediate.
   l    lbroff 26 bit PC relative immediate.
   r    sbroff 16 bit PC relative immediate.
   s	split 16 bit immediate.
   S	split 16 bit immediate, aligned.
   e    src1 floating point register.
   f    src2 floating point register.
   g    dest floating point register.

*/

/* The order of the opcodes in this table is significant:
   
   * The assembler requires that all instances of the same mnemonic must be
   consecutive.  If they aren't, the assembler will bomb at runtime.

   * The disassembler should not care about the order of the opcodes.  */

static struct i860_opcode i860_opcodes[] =
{

/* REG-Format Instructions */
{ "ld.c",	0x30000000, 0xcc000000, "c,d", 0 },	/* ld.c csrc2,idest */
{ "ld.b",	0x00000000, 0xfc000000, "1(2),d", 0 },	/* ld.b isrc1(isrc2),idest */
{ "ld.b",	0x04000000, 0xf8000000, "I(2),d", E_ADDR },	/* ld.b #const(isrc2),idest */
{ "ld.s",	0x10000000, 0xec000001, "1(2),d", 0 },	/* ld.s isrc1(isrc2),idest */
{ "ld.s",	0x14000001, 0xe8000000, "I(2),d", E_ADDR },	/* ld.s #const(isrc2),idest */
{ "ld.l",	0x10000001, 0xec000000, "1(2),d", 0 },	/* ld.l isrc1(isrc2),idest */
{ "ld.l",	0x14000001, 0xe8000000, "I(2),d", E_ADDR },	/* ld.l #const(isrc2),idest */

{ "st.c",	0x38000000, 0xc4000000, "1,c", 0 },	/* st.c isrc1ni,csrc2 */
{ "st.b",	0x0c000000, 0xf0000000, "1,S(2)", E_ADDR },	/* st.b isrc1ni,#const(isrc2) */
{ "st.s",	0x1c000000, 0xe0000000, "1,S(2)", E_ADDR },	/* st.s isrc1ni,#const(isrc2) */
{ "st.l",	0x1c000001, 0xe0000000, "1,S(2)", E_ADDR },	/* st.l isrc1ni,#const(isrc2) */

{ "ixfr",	0x08000000, 0xf4000000, "1,g", 0 },	/* ixfr isrc1ni,fdest */

{ "fld.l",	0x20000002, 0xdc000001, "1(2),g", 0 },	/* fld.l isrc1(isrc2),fdest */
{ "fld.l",	0x24000002, 0xd8000001, "i(2),g", E_ADDR },	/* fld.l #const(isrc2),fdest */
{ "fld.l",	0x20000003, 0xdc000000, "1(2)++,g", 0 },	/* fld.l isrc1(isrc2)++,fdest */
{ "fld.l",	0x24000003, 0xd8000000, "i(2)++,g", E_ADDR },	/* fld.l #const(isrc2)++,fdest */
{ "fld.d",	0x20000000, 0xdc000007, "1(2),g", 0 },	/* fld.d isrc1(isrc2),fdest */
{ "fld.d",	0x24000000, 0xd8000007, "i(2),g", E_ADDR },	/* fld.d #const(isrc2),fdest */
{ "fld.d",	0x20000001, 0xdc000006, "1(2)++,g", 0 },	/* fld.d isrc1(isrc2)++,fdest */
{ "fld.d",	0x24000001, 0xd8000006, "i(2)++,g", E_ADDR },	/* fld.d #const(isrc2)++,fdest */
{ "fld.q",	0x20000004, 0xdc000003, "1(2),g", 0 },	/* fld.q isrc1(isrc2),fdest */
{ "fld.q",	0x24000004, 0xd8000003, "i(2),g", E_ADDR },	/* fld.q #const(isrc2),fdest */
{ "fld.q",	0x20000005, 0xdc000002, "1(2)++,g", 0 },	/* fld.q isrc1(isrc2)++,fdest */
{ "fld.q",	0x24000005, 0xd8000002, "i(2)++,g", E_ADDR },	/* fld.q #const(isrc2)++,fdest */

{ "pfld.l",	0x60000000, 0x9c000003, "1(2),g", 0 },	/* pfld.l isrc1(isrc2),fdest */
{ "pfld.l",	0x64000000, 0x98000003, "i(2),g", E_ADDR },	/* pfld.l #const(isrc2),fdest */
{ "pfld.l",	0x60000001, 0x9c000002, "1(2)++,g", 0 },	/* pfld.l isrc1(isrc2)++,fdest */
{ "pfld.l",	0x64000001, 0x98000002, "i(2)++,g", E_ADDR },	/* pfld.l #const(isrc2)++,fdest */
{ "pfld.d",	0x60000000, 0x9c000007, "1(2),g", 0 },	/* pfld.d isrc1(isrc2),fdest */
{ "pfld.d",	0x64000000, 0x98000007, "i(2),g", E_ADDR },	/* pfld.d #const(isrc2),fdest */
{ "pfld.d",	0x60000001, 0x9c000006, "1(2)++,g", 0 },	/* pfld.d isrc1(isrc2)++,fdest */
{ "pfld.d",	0x64000001, 0x98000006, "i(2)++,g", E_ADDR },	/* pfld.d #const(isrc2)++,fdest */

{ "fst.l",	0x28000002, 0xd4000001, "g,1(2)", 0 },	/* fst.l fdest,isrc1(isrc2) */
{ "fst.l",	0x2c000002, 0xd0000001, "g,i(2)", E_ADDR },	/* fst.l fdest,#const(isrc2) */
{ "fst.l",	0x28000003, 0xd4000000, "g,1(2)++", 0 },	/* fst.l fdest,isrc1(isrc2)++ */
{ "fst.l",	0x2c000003, 0xd0000000, "g,i(2)++", E_ADDR },	/* fst.l fdest,#const(isrc2)++ */
{ "fst.d",	0x28000000, 0xd4000007, "g,1(2)", 0 },	/* fst.d fdest,isrc1(isrc2) */
{ "fst.d",	0x2c000000, 0xd0000007, "g,i(2)", E_ADDR },	/* fst.d fdest,#const(isrc2) */
{ "fst.d",	0x28000001, 0xd4000006, "g,1(2)++", 0 },	/* fst.d fdest,isrc1(isrc2)++ */
{ "fst.d",	0x2c000001, 0xd0000006, "g,i(2)++", E_ADDR },	/* fst.d fdest,#const(isrc2)++ */
{ "fst.q",	0x28000004, 0xd4000003, "g,1(2)", 0 },	/* fst.q fdest,isrc1(isrc2) */
{ "fst.q",	0x2c000004, 0xd0000003, "g,i(2)", E_ADDR },	/* fst.q fdest,#const(isrc2) */
{ "fst.q",	0x28000005, 0xd4000002, "g,1(2)++", 0 },	/* fst.q fdest,isrc1(isrc2)++ */
{ "fst.q",	0x2c000005, 0xd0000002, "g,i(2)++", E_ADDR },	/* fst.q fdest,#const(isrc2)++ */

{ "pst.d",	0x3c000000, 0xc0000007, "g,i(2)", E_ADDR },	/* pst.d fdest,#const(isrc2) */
{ "pst.d",	0x3c000001, 0xc0000006, "g,i(2)++", E_ADDR },	/* pst.d fdest,#const(isrc2)++ */

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

{ "flush",	0x34000000, 0xc81f0001, "i(2)", E_ADDR },	/* flush #const(isrc2) */
{ "flush",	0x34000001, 0xc81f0000, "i(2)++", E_ADDR },	/* flush #const(isrc2)++ */

{ "and",	0xc0000000, 0x3c000000, "1,2,d", 0 },	/* and isrc1,isrc2,idest */
{ "and",	0xc4000000, 0x38000000, "i,2,d", E_AND },	/* and #const,isrc2,idest */
{ "andh",	0xc8000000, 0x34000000, "1,2,d", 0 },	/* andh isrc1,isrc2,idest */
{ "andh",	0xcc000000, 0x30000000, "i,2,d", 0 },	/* andh #const,isrc2,idest */
{ "andnot",	0xd0000000, 0x2c000000, "1,2,d", 0 },	/* andnot isrc1,isrc2,idest */
{ "andnot",	0xd4000000, 0x28000000, "i,2,d", E_U32 },	/* andnot #const,isrc2,idest */
{ "andnoth",	0xd8000000, 0x24000000, "1,2,d", 0 },	/* andnoth isrc1,isrc2,idest */
{ "andnoth",	0xdc000000, 0x20000000, "i,2,d", 0 },	/* andnoth #const,isrc2,idest */
{ "or",		0xe0000000, 0x1c000000, "1,2,d", 0 },	/* or isrc1,isrc2,idest */
{ "or",		0xe4000000, 0x18000000, "i,2,d", E_U32 },	/* or #const,isrc2,idest */
{ "orh",	0xe8000000, 0x14000000, "1,2,d", 0 },	/* orh isrc1,isrc2,idest */
{ "orh",	0xec000000, 0x10000000, "i,2,d", 0 },	/* orh #const,isrc2,idest */
{ "xor",	0xf0000000, 0x0c000000, "1,2,d", 0 },	/* xor isrc1,isrc2,idest */
{ "xor",	0xf4000000, 0x08000000, "i,2,d", E_U32 },	/* xor #const,isrc2,idest */
{ "xorh",	0xf8000000, 0x04000000, "1,2,d", 0 },	/* xorh isrc1,isrc2,idest */
{ "xorh",	0xfc000000, 0x00000000, "i,2,d", 0 },	/* xorh #const,isrc2,idest */

{ "bte",	0x58000000, 0xa4000000, "1,2,s", 0 },	/* bte isrc1s,isrc2,sbroff */
{ "bte",	0x5c000000, 0xa0000000, "5,2,s", 0 },	/* bte #const5,isrc2,sbroff */
{ "btne",	0x50000000, 0xac000000, "1,2,s", 0 },	/* btne isrc1s,isrc2,sbroff */
{ "btne",	0x54000000, 0xa8000000, "5,2,s", 0 },	/* btne #const5,isrc2,sbroff */
{ "bla",	0xb4000000, 0x48000000, "1,2,s", E_DELAY },	/* bla isrc1s,isrc2,sbroff */
{ "bri",	0x40000000, 0xbc000000, "1", E_DELAY },	/* bri isrc1ni */

/* Core Escape Instruction Format */
{ "lock",	0x4c000001, 0xb000001e, "", 0 },	/* lock set BL in dirbase */
{ "calli",	0x4c000002, 0xb000001d, "1", E_DELAY },	/* calli isrc1ni */
{ "intovr",	0x4c000004, 0xb000001b, "", 0 },	/* intovr trap on integer overflow */
{ "unlock",	0x4c000007, 0xb0000018, "", 0 },	/* unlock clear BL in dirbase */

/* CTRL-Format Instructions */
{ "br",		0x68000000, 0x94000000, "l", E_DELAY },	/* br lbroff */
{ "call",	0x6c000000, 0x90000000, "l", E_DELAY },	/* call lbroff */
{ "bc",		0x70000000, 0x8c000000, "l", 0 },	/* bc lbroff */
{ "bc.t",	0x74000000, 0x88000000, "l", E_DELAY },	/* bc.t lbroff */
{ "bnc",	0x78000000, 0x84000000, "l", 0 },	/* bnc lbroff */
{ "bnc.t",	0x7c000000, 0x80000000, "l", E_DELAY },	/* bnc.t lbroff */

/* Floating Point Escape Instruction Format - pfam.p fsrc1,fsrc2,fdest */
{ "r2p1.ss",	0x48000400, 0xb40003ff, "e,f,g", 0 },
{ "r2p1.sd",	0x48000480, 0xb400037f, "e,f,g", 0 },
{ "r2p1.dd",	0x48000580, 0xb400027f, "e,f,g", 0 },
{ "r2pt.ss",	0x48000401, 0xb40003fe, "e,f,g", 0 },
{ "r2pt.sd",	0x48000481, 0xb400037e, "e,f,g", 0 },
{ "r2pt.dd",	0x48000581, 0xb400027e, "e,f,g", 0 },
{ "r2ap1.ss",	0x48000402, 0xb40003fd, "e,f,g", 0 },
{ "r2ap1.sd",	0x48000482, 0xb400037d, "e,f,g", 0 },
{ "r2ap1.dd",	0x48000582, 0xb400027d, "e,f,g", 0 },
{ "r2apt.ss",	0x48000403, 0xb40003fc, "e,f,g", 0 },
{ "r2apt.sd",	0x48000483, 0xb400037c, "e,f,g", 0 },
{ "r2apt.dd",	0x48000583, 0xb400027c, "e,f,g", 0 },
{ "i2p1.ss",	0x48000404, 0xb40003fb, "e,f,g", 0 },
{ "i2p1.sd",	0x48000484, 0xb400037b, "e,f,g", 0 },
{ "i2p1.dd",	0x48000584, 0xb400027b, "e,f,g", 0 },
{ "i2pt.ss",	0x48000405, 0xb40003fa, "e,f,g", 0 },
{ "i2pt.sd",	0x48000485, 0xb400037a, "e,f,g", 0 },
{ "i2pt.dd",	0x48000585, 0xb400027a, "e,f,g", 0 },
{ "i2ap1.ss",	0x48000406, 0xb40003f9, "e,f,g", 0 },
{ "i2ap1.sd",	0x48000486, 0xb4000379, "e,f,g", 0 },
{ "i2ap1.dd",	0x48000586, 0xb4000279, "e,f,g", 0 },
{ "i2apt.ss",	0x48000407, 0xb40003f8, "e,f,g", 0 },
{ "i2apt.sd",	0x48000487, 0xb4000378, "e,f,g", 0 },
{ "i2apt.dd",	0x48000587, 0xb4000278, "e,f,g", 0 },
{ "rat1p2.ss",	0x48000408, 0xb40003f7, "e,f,g", 0 },
{ "rat1p2.sd",	0x48000488, 0xb4000377, "e,f,g", 0 },
{ "rat1p2.dd",	0x48000588, 0xb4000277, "e,f,g", 0 },
{ "m12apm.ss",	0x48000409, 0xb40003f6, "e,f,g", 0 },
{ "m12apm.sd",	0x48000489, 0xb4000376, "e,f,g", 0 },
{ "m12apm.dd",	0x48000589, 0xb4000276, "e,f,g", 0 },
{ "ra1p2.ss",	0x4800040a, 0xb40003f5, "e,f,g", 0 },
{ "ra1p2.sd",	0x4800048a, 0xb4000375, "e,f,g", 0 },
{ "ra1p2.dd",	0x4800058a, 0xb4000275, "e,f,g", 0 },
{ "m12ttpa.ss",	0x4800040b, 0xb40003f4, "e,f,g", 0 },
{ "m12ttpa.sd",	0x4800048b, 0xb4000374, "e,f,g", 0 },
{ "m12ttpa.dd",	0x4800058b, 0xb4000274, "e,f,g", 0 },
{ "iat1p2.ss",	0x4800040c, 0xb40003f3, "e,f,g", 0 },
{ "iat1p2.sd",	0x4800048c, 0xb4000373, "e,f,g", 0 },
{ "iat1p2.dd",	0x4800058c, 0xb4000273, "e,f,g", 0 },
{ "m12tpm.ss",	0x4800040d, 0xb40003f2, "e,f,g", 0 },
{ "m12tpm.sd",	0x4800048d, 0xb4000372, "e,f,g", 0 },
{ "m12tpm.dd",	0x4800058d, 0xb4000272, "e,f,g", 0 },
{ "ia1p2.ss",	0x4800040e, 0xb40003f1, "e,f,g", 0 },
{ "ia1p2.sd",	0x4800048e, 0xb4000371, "e,f,g", 0 },
{ "ia1p2.dd",	0x4800058e, 0xb4000271, "e,f,g", 0 },
{ "m12tpa.ss",	0x4800040f, 0xb40003f0, "e,f,g", 0 },
{ "m12tpa.sd",	0x4800048f, 0xb4000370, "e,f,g", 0 },
{ "m12tpa.dd",	0x4800058f, 0xb4000270, "e,f,g", 0 },

/* Floating Point Escape Instruction Format - pfsm.p fsrc1,fsrc2,fdest */
{ "r2s1.ss",	0x48000410, 0xb40003ef, "e,f,g", 0 },
{ "r2s1.sd",	0x48000490, 0xb400036f, "e,f,g", 0 },
{ "r2s1.dd",	0x48000590, 0xb400026f, "e,f,g", 0 },
{ "r2st.ss",	0x48000411, 0xb40003ee, "e,f,g", 0 },
{ "r2st.sd",	0x48000491, 0xb400036e, "e,f,g", 0 },
{ "r2st.dd",	0x48000591, 0xb400026e, "e,f,g", 0 },
{ "r2as1.ss",	0x48000412, 0xb40003ed, "e,f,g", 0 },
{ "r2as1.sd",	0x48000492, 0xb400036d, "e,f,g", 0 },
{ "r2as1.dd",	0x48000592, 0xb400026d, "e,f,g", 0 },
{ "r2ast.ss",	0x48000413, 0xb40003ec, "e,f,g", 0 },
{ "r2ast.sd",	0x48000493, 0xb400036c, "e,f,g", 0 },
{ "r2ast.dd",	0x48000593, 0xb400026c, "e,f,g", 0 },
{ "i2s1.ss",	0x48000414, 0xb40003eb, "e,f,g", 0 },
{ "i2s1.sd",	0x48000494, 0xb400036b, "e,f,g", 0 },
{ "i2s1.dd",	0x48000594, 0xb400026b, "e,f,g", 0 },
{ "i2st.ss",	0x48000415, 0xb40003ea, "e,f,g", 0 },
{ "i2st.sd",	0x48000495, 0xb400036a, "e,f,g", 0 },
{ "i2st.dd",	0x48000595, 0xb400026a, "e,f,g", 0 },
{ "i2as1.ss",	0x48000416, 0xb40003e9, "e,f,g", 0 },
{ "i2as1.sd",	0x48000496, 0xb4000369, "e,f,g", 0 },
{ "i2as1.dd",	0x48000596, 0xb4000269, "e,f,g", 0 },
{ "i2ast.ss",	0x48000417, 0xb40003e8, "e,f,g", 0 },
{ "i2ast.sd",	0x48000497, 0xb4000368, "e,f,g", 0 },
{ "i2ast.dd",	0x48000597, 0xb4000268, "e,f,g", 0 },
{ "rat1s2.ss",	0x48000418, 0xb40003e7, "e,f,g", 0 },
{ "rat1s2.sd",	0x48000498, 0xb4000367, "e,f,g", 0 },
{ "rat1s2.dd",	0x48000598, 0xb4000267, "e,f,g", 0 },
{ "m12asm.ss",	0x48000419, 0xb40003e6, "e,f,g", 0 },
{ "m12asm.sd",	0x48000499, 0xb4000366, "e,f,g", 0 },
{ "m12asm.dd",	0x48000599, 0xb4000266, "e,f,g", 0 },
{ "ra1s2.ss",	0x4800041a, 0xb40003e5, "e,f,g", 0 },
{ "ra1s2.sd",	0x4800049a, 0xb4000365, "e,f,g", 0 },
{ "ra1s2.dd",	0x4800059a, 0xb4000265, "e,f,g", 0 },
{ "m12ttsa.ss",	0x4800041b, 0xb40003e4, "e,f,g", 0 },
{ "m12ttsa.sd",	0x4800049b, 0xb4000364, "e,f,g", 0 },
{ "m12ttsa.dd",	0x4800059b, 0xb4000264, "e,f,g", 0 },
{ "iat1s2.ss",	0x4800041c, 0xb40003e3, "e,f,g", 0 },
{ "iat1s2.sd",	0x4800049c, 0xb4000363, "e,f,g", 0 },
{ "iat1s2.dd",	0x4800059c, 0xb4000263, "e,f,g", 0 },
{ "m12tsm.ss",	0x4800041d, 0xb40003e2, "e,f,g", 0 },
{ "m12tsm.sd",	0x4800049d, 0xb4000362, "e,f,g", 0 },
{ "m12tsm.dd",	0x4800059d, 0xb4000262, "e,f,g", 0 },
{ "ia1s2.ss",	0x4800041e, 0xb40003e1, "e,f,g", 0 },
{ "ia1s2.sd",	0x4800049e, 0xb4000361, "e,f,g", 0 },
{ "ia1s2.dd",	0x4800059e, 0xb4000261, "e,f,g", 0 },
{ "m12tsa.ss",	0x4800041f, 0xb40003e0, "e,f,g", 0 },
{ "m12tsa.sd",	0x4800049f, 0xb4000360, "e,f,g", 0 },
{ "m12tsa.dd",	0x4800059f, 0xb4000260, "e,f,g", 0 },

/* Floating Point Escape Instruction Format - pfmam.p fsrc1,fsrc2,fdest */
{ "mr2p1.ss",	0x48000000, 0xb40007ff, "e,f,g", 0 },
{ "mr2p1.sd",	0x48000080, 0xb400077f, "e,f,g", 0 },
{ "mr2p1.dd",	0x48000180, 0xb400067f, "e,f,g", 0 },
{ "mr2pt.ss",	0x48000001, 0xb40007fe, "e,f,g", 0 },
{ "mr2pt.sd",	0x48000081, 0xb400077e, "e,f,g", 0 },
{ "mr2pt.dd",	0x48000181, 0xb400067e, "e,f,g", 0 },
{ "mr2mp1.ss",	0x48000002, 0xb40007fd, "e,f,g", 0 },
{ "mr2mp1.sd",	0x48000082, 0xb400077d, "e,f,g", 0 },
{ "mr2mp1.dd",	0x48000182, 0xb400067d, "e,f,g", 0 },
{ "mr2mpt.ss",	0x48000003, 0xb40007fc, "e,f,g", 0 },
{ "mr2mpt.sd",	0x48000083, 0xb400077c, "e,f,g", 0 },
{ "mr2mpt.dd",	0x48000183, 0xb400067c, "e,f,g", 0 },
{ "mi2p1.ss",	0x48000004, 0xb40007fb, "e,f,g", 0 },
{ "mi2p1.sd",	0x48000084, 0xb400077b, "e,f,g", 0 },
{ "mi2p1.dd",	0x48000184, 0xb400067b, "e,f,g", 0 },
{ "mi2pt.ss",	0x48000005, 0xb40007fa, "e,f,g", 0 },
{ "mi2pt.sd",	0x48000085, 0xb400077a, "e,f,g", 0 },
{ "mi2pt.dd",	0x48000185, 0xb400067a, "e,f,g", 0 },
{ "mi2mp1.ss",	0x48000006, 0xb40007f9, "e,f,g", 0 },
{ "mi2mp1.sd",	0x48000086, 0xb4000779, "e,f,g", 0 },
{ "mi2mp1.dd",	0x48000186, 0xb4000679, "e,f,g", 0 },
{ "mi2mpt.ss",	0x48000007, 0xb40007f8, "e,f,g", 0 },
{ "mi2mpt.sd",	0x48000087, 0xb4000778, "e,f,g", 0 },
{ "mi2mpt.dd",	0x48000187, 0xb4000678, "e,f,g", 0 },
{ "mrmt1p2.ss",	0x48000008, 0xb40007f7, "e,f,g", 0 },
{ "mrmt1p2.sd",	0x48000088, 0xb4000777, "e,f,g", 0 },
{ "mrmt1p2.dd",	0x48000188, 0xb4000677, "e,f,g", 0 },
{ "mm12mpm.ss",	0x48000009, 0xb40007f6, "e,f,g", 0 },
{ "mm12mpm.sd",	0x48000089, 0xb4000776, "e,f,g", 0 },
{ "mm12mpm.dd",	0x48000189, 0xb4000676, "e,f,g", 0 },
{ "mrm1p2.ss",	0x4800000a, 0xb40007f5, "e,f,g", 0 },
{ "mrm1p2.sd",	0x4800008a, 0xb4000775, "e,f,g", 0 },
{ "mrm1p2.dd",	0x4800018a, 0xb4000675, "e,f,g", 0 },
{ "mm12ttpm.ss",0x4800000b, 0xb40007f4, "e,f,g", 0 },
{ "mm12ttpm.sd",0x4800008b, 0xb4000774, "e,f,g", 0 },
{ "mm12ttpm.dd",0x4800018b, 0xb4000674, "e,f,g", 0 },
{ "mimt1p2.ss",	0x4800000c, 0xb40007f3, "e,f,g", 0 },
{ "mimt1p2.sd",	0x4800008c, 0xb4000773, "e,f,g", 0 },
{ "mimt1p2.dd",	0x4800018c, 0xb4000673, "e,f,g", 0 },
{ "mm12tpm.ss",	0x4800000d, 0xb40007f2, "e,f,g", 0 },
{ "mm12tpm.sd",	0x4800008d, 0xb4000772, "e,f,g", 0 },
{ "mm12tpm.dd",	0x4800018d, 0xb4000672, "e,f,g", 0 },
{ "mim1p2.ss",	0x4800000e, 0xb40007f1, "e,f,g", 0 },
{ "mim1p2.sd",	0x4800008e, 0xb4000771, "e,f,g", 0 },
{ "mim1p2.dd",	0x4800018e, 0xb4000671, "e,f,g", 0 },

/* Floating Point Escape Instruction Format - pfmsm.p fsrc1,fsrc2,fdest */
{ "mr2s1.ss",	0x48000010, 0xb40007ef, "e,f,g", 0 },
{ "mr2s1.sd",	0x48000090, 0xb400076f, "e,f,g", 0 },
{ "mr2s1.dd",	0x48000190, 0xb400066f, "e,f,g", 0 },
{ "mr2st.ss",	0x48000011, 0xb40007ee, "e,f,g", 0 },
{ "mr2st.sd",	0x48000091, 0xb400076e, "e,f,g", 0 },
{ "mr2st.dd",	0x48000191, 0xb400066e, "e,f,g", 0 },
{ "mr2ms1.ss",	0x48000012, 0xb40007ed, "e,f,g", 0 },
{ "mr2ms1.sd",	0x48000092, 0xb400076d, "e,f,g", 0 },
{ "mr2ms1.dd",	0x48000192, 0xb400066d, "e,f,g", 0 },
{ "mr2mst.ss",	0x48000013, 0xb40007ec, "e,f,g", 0 },
{ "mr2mst.sd",	0x48000093, 0xb400076c, "e,f,g", 0 },
{ "mr2mst.dd",	0x48000193, 0xb400066c, "e,f,g", 0 },
{ "mi2s1.ss",	0x48000014, 0xb40007eb, "e,f,g", 0 },
{ "mi2s1.sd",	0x48000094, 0xb400076b, "e,f,g", 0 },
{ "mi2s1.dd",	0x48000194, 0xb400066b, "e,f,g", 0 },
{ "mi2st.ss",	0x48000015, 0xb40007ea, "e,f,g", 0 },
{ "mi2st.sd",	0x48000095, 0xb400076a, "e,f,g", 0 },
{ "mi2st.dd",	0x48000195, 0xb400066a, "e,f,g", 0 },
{ "mi2ms1.ss",	0x48000016, 0xb40007e9, "e,f,g", 0 },
{ "mi2ms1.sd",	0x48000096, 0xb4000769, "e,f,g", 0 },
{ "mi2ms1.dd",	0x48000196, 0xb4000669, "e,f,g", 0 },
{ "mi2mst.ss",	0x48000017, 0xb40007e8, "e,f,g", 0 },
{ "mi2mst.sd",	0x48000097, 0xb4000768, "e,f,g", 0 },
{ "mi2mst.dd",	0x48000197, 0xb4000668, "e,f,g", 0 },
{ "mrmt1s2.ss",	0x48000018, 0xb40007e7, "e,f,g", 0 },
{ "mrmt1s2.sd",	0x48000098, 0xb4000767, "e,f,g", 0 },
{ "mrmt1s2.dd",	0x48000198, 0xb4000667, "e,f,g", 0 },
{ "mm12msm.ss",	0x48000019, 0xb40007e6, "e,f,g", 0 },
{ "mm12msm.sd",	0x48000099, 0xb4000766, "e,f,g", 0 },
{ "mm12msm.dd",	0x48000199, 0xb4000666, "e,f,g", 0 },
{ "mrm1s2.ss",	0x4800001a, 0xb40007e5, "e,f,g", 0 },
{ "mrm1s2.sd",	0x4800009a, 0xb4000765, "e,f,g", 0 },
{ "mrm1s2.dd",	0x4800019a, 0xb4000665, "e,f,g", 0 },
{ "mm12ttsm.ss",0x4800001b, 0xb40007e4, "e,f,g", 0 },
{ "mm12ttsm.sd",0x4800009b, 0xb4000764, "e,f,g", 0 },
{ "mm12ttsm.dd",0x4800019b, 0xb4000664, "e,f,g", 0 },
{ "mimt1s2.ss",	0x4800001c, 0xb40007e3, "e,f,g", 0 },
{ "mimt1s2.sd",	0x4800009c, 0xb4000763, "e,f,g", 0 },
{ "mimt1s2.dd",	0x4800019c, 0xb4000663, "e,f,g", 0 },
{ "mm12tsm.ss",	0x4800001d, 0xb40007e2, "e,f,g", 0 },
{ "mm12tsm.sd",	0x4800009d, 0xb4000762, "e,f,g", 0 },
{ "mm12tsm.dd",	0x4800019d, 0xb4000662, "e,f,g", 0 },
{ "mim1s2.ss",	0x4800001e, 0xb40007e1, "e,f,g", 0 },
{ "mim1s2.sd",	0x4800009e, 0xb4000761, "e,f,g", 0 },
{ "mim1s2.dd",	0x4800019e, 0xb4000661, "e,f,g", 0 },


{ "fmul.ss",	0x48000020, 0xb40007df, "e,f,g", 0 },	/* fmul.p fsrc1,fsrc2,fdest */
{ "fmul.sd",	0x480000a0, 0xb400075f, "e,f,g", 0 },	/* fmul.p fsrc1,fsrc2,fdest */
{ "fmul.dd",	0x480001a0, 0xb400065f, "e,f,g", 0 },	/* fmul.p fsrc1,fsrc2,fdest */
{ "pfmul.ss",	0x48000420, 0xb40003df, "e,f,g", 0 },	/* pfmul.p fsrc1,fsrc2,fdest */
{ "pfmul.sd",	0x480004a0, 0xb400035f, "e,f,g", 0 },	/* pfmul.p fsrc1,fsrc2,fdest */
{ "pfmul.dd",	0x480005a0, 0xb400025f, "e,f,g", 0 },	/* pfmul.p fsrc1,fsrc2,fdest */
{ "pfmul3.dd",	0x480005a4, 0xb400025b, "e,f,g", 0 },	/* pfmul3.p fsrc1,fsrc2,fdest */
{ "fmlow.dd",	0x480001a1, 0xb400065e, "e,f,g", 0 },	/* fmlow.dd fsrc1,fsrc2,fdest */
{ "frcp.ss",	0x48000022, 0xb40007dd, "f,g", 0 },	/* frcp.p fsrc2,fdest */
{ "frcp.sd",	0x480000a2, 0xb400075d, "f,g", 0 },	/* frcp.p fsrc2,fdest */
{ "frcp.dd",	0x480001a2, 0xb400065d, "f,g", 0 },	/* frcp.p fsrc2,fdest */
{ "frsqr.ss",	0x48000023, 0xb40007dc, "f,g", 0 },	/* frsqr.p fsrc2,fdest */
{ "frsqr.sd",	0x480000a3, 0xb400075c, "f,g", 0 },	/* frsqr.p fsrc2,fdest */
{ "frsqr.dd",	0x480001a3, 0xb400065c, "f,g", 0 },	/* frsqr.p fsrc2,fdest */
{ "fadd.ss",	0x48000030, 0xb40007cf, "e,f,g", 0 },	/* fadd.p fsrc1,fsrc2,fdest */
{ "fadd.sd",	0x480000b0, 0xb400074f, "e,f,g", 0 },	/* fadd.p fsrc1,fsrc2,fdest */
{ "fadd.dd",	0x480001b0, 0xb400064f, "e,f,g", 0 },	/* fadd.p fsrc1,fsrc2,fdest */
{ "pfadd.ss",	0x48000430, 0xb40003cf, "e,f,g", 0 },	/* pfadd.p fsrc1,fsrc2,fdest */
{ "pfadd.sd",	0x480004b0, 0xb400034f, "e,f,g", 0 },	/* pfadd.p fsrc1,fsrc2,fdest */
{ "pfadd.dd",	0x480005b0, 0xb400024f, "e,f,g", 0 },	/* pfadd.p fsrc1,fsrc2,fdest */
{ "fsub.ss",	0x48000031, 0xb40007ce, "e,f,g", 0 },	/* fsub.p fsrc1,fsrc2,fdest */
{ "fsub.sd",	0x480000b1, 0xb400074e, "e,f,g", 0 },	/* fsub.p fsrc1,fsrc2,fdest */
{ "fsub.dd",	0x480001b1, 0xb400064e, "e,f,g", 0 },	/* fsub.p fsrc1,fsrc2,fdest */
{ "pfsub.ss",	0x48000431, 0xb40003ce, "e,f,g", 0 },	/* pfsub.p fsrc1,fsrc2,fdest */
{ "pfsub.sd",	0x480004b1, 0xb400034e, "e,f,g", 0 },	/* pfsub.p fsrc1,fsrc2,fdest */
{ "pfsub.dd",	0x480005b1, 0xb400024e, "e,f,g", 0 },	/* pfsub.p fsrc1,fsrc2,fdest */
{ "fix.ss",	0x48000032, 0xb40007cd, "e,g", 0 },	/* fix.p fsrc1,fdest */
{ "fix.sd",	0x480000b2, 0xb400074d, "e,g", 0 },	/* fix.p fsrc1,fdest */
{ "fix.dd",	0x480001b2, 0xb400064d, "e,g", 0 },	/* fix.p fsrc1,fdest */
{ "pfix.ss",	0x48000432, 0xb40003cd, "e,g", 0 },	/* pfix.p fsrc1,fdest */
{ "pfix.sd",	0x480004b2, 0xb400034d, "e,g", 0 },	/* pfix.p fsrc1,fdest */
{ "pfix.dd",	0x480005b2, 0xb400024d, "e,g", 0 },	/* pfix.p fsrc1,fdest */
{ "famov.ss",	0x48000033, 0xb40007cc, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "famov.ds",	0x48000133, 0xb40006cc, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "famov.sd",	0x480000b3, 0xb400074c, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "famov.dd",	0x480001b3, 0xb400064c, "e,g", 0 },	/* famov.p fsrc1,fdest */
{ "pfamov.ss",	0x48000433, 0xb40003cc, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
{ "pfamov.ds",	0x48000533, 0xb40002cc, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
{ "pfamov.sd",	0x480004b3, 0xb400034c, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
{ "pfamov.dd",	0x480005b3, 0xb400024c, "e,g", 0 },	/* pfamov.p fsrc1,fdest */
/* pfgt has R bit cleared; pfle has R bit set */
{ "pfgt.ss",	0x48000434, 0xb40003cb, "e,f,g", 0 },	/* pfgt.p fsrc1,fsrc2,fdest */
{ "pfgt.sd",	0x48000434, 0xb40003cb, "e,f,g", 0 },	/* pfgt.p fsrc1,fsrc2,fdest */
{ "pfgt.dd",	0x48000534, 0xb40002cb, "e,f,g", 0 },	/* pfgt.p fsrc1,fsrc2,fdest */
/* pfgt has R bit cleared; pfle has R bit set */
{ "pfle.ss",	0x480004b4, 0xb400034b, "e,f,g", 0 },	/* pfle.p fsrc1,fsrc2,fdest */
{ "pfle.sd",	0x480004b4, 0xb400034b, "e,f,g", 0 },	/* pfle.p fsrc1,fsrc2,fdest */
{ "pfle.dd",	0x480005b4, 0xb400024b, "e,f,g", 0 },	/* pfle.p fsrc1,fsrc2,fdest */
{ "ftrunc.ss",	0x4800003a, 0xb40007c5, "e,g", 0 },	/* ftrunc.p fsrc1,fdest */
{ "ftrunc.sd",	0x480000ba, 0xb4000745, "e,g", 0 },	/* ftrunc.p fsrc1,fdest */
{ "ftrunc.dd",	0x480001ba, 0xb4000645, "e,g", 0 },	/* ftrunc.p fsrc1,fdest */
{ "pftrunc.ss",	0x4800043a, 0xb40003c5, "e,g", 0 },	/* pftrunc.p fsrc1,fdest */
{ "pftrunc.sd",	0x480004ba, 0xb4000345, "e,g", 0 },	/* pftrunc.p fsrc1,fdest */
{ "pftrunc.dd",	0x480005ba, 0xb4000245, "e,g", 0 },	/* pftrunc.p fsrc1,fdest */
{ "fxfr",	0x48000040, 0xb40007bf, "e,d", 0 },	/* fxfr fsrc1,idest */
{ "fiadd.ss",	0x48000049, 0xb40007b6, "e,f,g", 0 },	/* fiadd.w fsrc1,fsrc2,fdest */
{ "fiadd.dd",	0x480001c9, 0xb4000636, "e,f,g", 0 },	/* fiadd.w fsrc1,fsrc2,fdest */
{ "pfiadd.ss",	0x48000449, 0xb40003b6, "e,f,g", 0 },	/* pfiadd.w fsrc1,fsrc2,fdest */
{ "pfiadd.dd",	0x480005c9, 0xb4000236, "e,f,g", 0 },	/* pfiadd.w fsrc1,fsrc2,fdest */
{ "fisub.ss",	0x4800004d, 0xb40007b2, "e,f,g", 0 },	/* fisub.w fsrc1,fsrc2,fdest */
{ "fisub.dd",	0x480001cd, 0xb4000632, "e,f,g", 0 },	/* fisub.w fsrc1,fsrc2,fdest */
{ "pfisub.ss",	0x4800044d, 0xb40003b2, "e,f,g", 0 },	/* pfisub.w fsrc1,fsrc2,fdest */
{ "pfisub.dd",	0x480005cd, 0xb4000232, "e,f,g", 0 },	/* pfisub.w fsrc1,fsrc2,fdest */
{ "fzchkl",	0x48000057, 0xb40007a8, "e,f,g", 0 },	/* fzchkl fsrc1,fsrc2,fdest */
{ "pfzchkl",	0x48000457, 0xb40003a8, "e,f,g", 0 },	/* pfzchkl fsrc1,fsrc2,fdest */
{ "fzchks",	0x4800005f, 0xb40007a0, "e,f,g", 0 },	/* fzchks fsrc1,fsrc2,fdest */
{ "pfzchks",	0x4800045f, 0xb40003a0, "e,f,g", 0 },	/* pfzchks fsrc1,fsrc2,fdest */
{ "faddp",	0x48000050, 0xb40007af, "e,f,g", 0 },	/* faddp fsrc1,fsrc2,fdest */
{ "pfaddp",	0x48000450, 0xb40003af, "e,f,g", 0 },	/* pfaddp fsrc1,fsrc2,fdest */
{ "faddz",	0x48000051, 0xb40007ae, "e,f,g", 0 },	/* faddz fsrc1,fsrc2,fdest */
{ "pfaddz",	0x48000451, 0xb40003ae, "e,f,g", 0 },	/* pfaddz fsrc1,fsrc2,fdest */
{ "form",	0x4800005a, 0xb40007a5, "e,g", 0 },	/* form fsrc1,fdest */
{ "pform",	0x4800045a, 0xb40003a5, "e,g", 0 },	/* pform fsrc1,fdest */

/* Floating point pseudo-instructions */
{ "fmov.ss",	0x48000049, 0xb7e007b6, "e,g", 0 },	/* fiadd.ss fsrc1,f0,fdest */
{ "fmov.dd",	0x480001c9, 0xb7e00636, "e,g", 0 },	/* fiadd.dd fsrc1,f0,fdest */
{ "fmov.sd",	0x480000b0, 0xb7e0074f, "e,g", 0 },	/* fadd.sd fsrc1,f0,fdest */
{ "fmov.ds",	0x48000130, 0xb7e006cf, "e,g", 0 },	/* fadd.ds fsrc1,f0,fdest */
{ "pfmov.ds",	0x48000530, 0xb73002cf, "e,g", 0 },	/* pfadd.ds fsrc1,f0,fdest */
{ "pfmov.dd",	0x480005c9, 0xb7e00236, "e,g", 0 },	/* pfiadd.dd fsrc1,f0,fdest */


};

#define NUMOPCODES ((sizeof i860_opcodes)/(sizeof i860_opcodes[0]))


