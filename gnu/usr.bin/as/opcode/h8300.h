/* Opcode table for the H8-300
   Copyright (C) 1991,1992 Free Software Foundation.
   Written by Steve Chamberlain, sac@cygnus.com.

This file is part of GDB, the GNU Debugger and GAS, the GNU Assembler.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

typedef int op_type;

#define Hex0	0
#define Hex1 	1
#define Hex2 	2
#define Hex3 	3
#define Hex4 	4
#define Hex5 	5
#define Hex6 	6
#define Hex7 	7
#define Hex8 	8
#define Hex9 	9
#define HexA 	10
#define HexB 	11
#define HexC 	12
#define HexD 	13
#define HexE 	14
#define HexF 	15
#define START  		0x20
#define KBIT 		0x21		/* K is #1, or #2, yielding 0x0 or 0x8 */
#define IMM3 		0x22		/* bit number */
#define RD8 		0x23		/* 8 bit reg as 2nd op  */
#define RD16 		0x24		/* 16 bit reg as 2nd op */
#define RS8 		0x25		/* 8 bit reg as 1st op */
#define RS16 		0x26		/* 16 bit reg 1st op */
#define IMM8 		0x27		/* constant which fits into 8 bits */
#define IMM16 		0x28		/* constant which fits into 16 bits */
#define CCR 		0x29		/* CCR reg */
#define ABS8SRC 	0x2a		/* abs 8 address mode */
#define ABS8DST		0x2b		/* abs 8 address mode */
#define DISP8 		0x2c		/* pc rel displacement */
#define ABS16SRC 	0x2d		/* abs 16 address mode */
#define ABS16OR8SRC 	0x2e		/* abs 16 address mode, but could be abs 8 */
#define ABS16DST 	0x2f		/* abs 16 address mode */
#define ABS16OR8DST 	0x30		/* abs 16 address mode */
#define DISPSRC 	0x31		/* @(r:16) address mode src */
#define DISPDST 	0x32		/* @(r:16) address mode dst*/
#define DISPREG 	0x33		/* register from DISP address mode */
#define RDDEC 		0x34		/* @-rn mode */
#define RSINC 		0x35		/* @rn+ mode */
#define RDIND 		0x36		/* @R mode dst */
#define RSIND 		0x37		/* @R mode src */
#define MEMIND 		0x38		/* @@abs8 mode */
#define ABS16ORREL8SRC	0x39          	/* abs 16bit or pcrel */
#define IGNORE 		0x3a
#define B30  		0x40 		/* bit 3 must be low */
#define B31  		0x80		/* bit 3 must be high */
#define E      		0x81		/* End of list */



struct code
{
  op_type nib[9];
} ;

struct arg
{
  op_type nib[3];
} ;

struct h8_opcode
{
    char *name;
    struct arg args;
    struct code data;
    char length;
    char noperands;
    char idx;
    char size;

};





#ifdef DEFINE_TABLE

#define BITOP(imm, name, op00, op01,op10,op11, op20,op21)\
{  name,	{imm,RD8,E},	{op00, op01, imm, RD8,E}},\
{  name,	{imm,RDIND,E},	{op10, op11, RDIND, 0, op00,op01, imm, 0,E}},\
{  name,	{imm,ABS8DST,E},{op20, op21, ABS8DST, IGNORE, op00,op01, imm, 0,E}}

#define EBITOP(imm, name, op00, op01,op10,op11, op20,op21)\
   BITOP(imm, name, op00+1, op01, op10,op11, op20,op21),\
   BITOP(RS8,  name, op00, op01, op10,op11, op20,op21)

#define WTWOP(name, op1, op2) \
{ name, {RS16, RD16, E}, { op1, op2, RS16, RD16, E}}

#define BRANCH(name, op) \
{ name,{DISP8,E}, { Hex4, op, DISP8,IGNORE,E }}

#define SOP(name) \
{  name
#define EOP   }


#define TWOOP(name, op1, op2,op3) \
{ name, {IMM8, RD8,E},	{ op1, RD8, IMM8,IGNORE,E}},\
{ name, {RS8, RD8, E},	{ op2, op3, RS8, RD8 ,E}}

#define UNOP(name, op1, op2) \
{ name, {RS8, E}, { op1, op2, 0, RS8, E}}

#define UNOP3(name, op1, op2, op3) \
{ name , {RS8, E}, {op1, op2, op3, RS8, E}}

struct h8_opcode h8_opcodes[]
=
{
  TWOOP("add.b", Hex8, Hex0,Hex8),
  WTWOP("add.w", Hex0, Hex9),
  SOP("adds"), {KBIT,RD16|B30, E}, {Hex0, HexB, KBIT, RD16|B30, E} EOP,
  TWOOP("addx", Hex9,Hex0,HexE),
  TWOOP("and", HexE,Hex1,Hex6),
  SOP("andc"),  {IMM8, CCR, E}, { Hex0, Hex6, IMM8,IGNORE, E} EOP,
  BITOP(IMM3|B30, "band", Hex7, Hex6, Hex7, HexC, Hex7, HexE),
  BRANCH("bra", Hex0),
  BRANCH("bt", Hex0),
  BRANCH("brn", Hex1),
  BRANCH("bf", Hex1),
  BRANCH("bhi", Hex2),
  BRANCH("bls", Hex3),
  BRANCH("bcc", Hex4),
  BRANCH("bhs", Hex4),
  BRANCH("bcs", Hex5),
  BRANCH("blo", Hex5),
  BRANCH("bne", Hex6),
  BRANCH("beq", Hex7),
  BRANCH("bvc", Hex8),
  BRANCH("bvs", Hex9),
  BRANCH("bpl", HexA),
  BRANCH("bmi", HexB),
  BRANCH("bge", HexC),
  BRANCH("blt", HexD),
  BRANCH("bgt", HexE),
  BRANCH("ble", HexF),
  EBITOP(IMM3|B30,"bclr", Hex6, Hex2, Hex7, HexD, Hex7, HexF),
  BITOP(IMM3|B31,"biand", Hex7, Hex6, Hex7, HexC, Hex7, HexE),
  BITOP(IMM3|B31, "bild", Hex7, Hex7,Hex7, HexC, Hex7, HexE),
  BITOP(IMM3|B31, "bior", Hex7, Hex4,Hex7, HexC, Hex7, HexE),
  BITOP(IMM3|B31, "bist", Hex6, Hex7,Hex7, HexD, Hex7, HexE),
  BITOP(IMM3|B31, "bixor", Hex7, Hex5,Hex7, HexC, Hex7, HexE),
  BITOP(IMM3|B30, "bld", Hex7, Hex7,Hex7, HexC, Hex7, HexE),
  EBITOP(IMM3|B30,"bnot", Hex6, Hex1, Hex7, HexD, Hex7, HexF),
  BITOP(IMM3|B30,"bor", Hex7, Hex4,Hex7, HexC, Hex7, HexE),
  EBITOP(IMM3|B30,"bset", Hex6, Hex0,Hex7, HexD, Hex7, HexF),
  SOP("bsr"),{DISP8, E},{ Hex5, Hex5, DISP8,IGNORE, E}, EOP,
  BITOP(IMM3|B30, "bst", Hex6, Hex7,Hex7, HexD, Hex7, HexF),
  EBITOP(IMM3|B30, "btst", Hex6, Hex3,Hex7, HexC, Hex7, HexE),
  BITOP(IMM3|B30, "bxor", Hex7,Hex5,Hex7, HexC, Hex7, HexE),
  TWOOP( "cmp.b",HexA, Hex1, HexC),
  WTWOP( "cmp.w",Hex1,HexD),
  UNOP( "daa",Hex0, HexF),
  UNOP( "das",Hex1, HexF),
  UNOP( "dec",Hex1, HexA),
  SOP("divxu"),{RS8, RD16|B30, E}, { Hex5, Hex1, RS8, RD16|B30, E} EOP,
  SOP("eepmov"),{ E}, {Hex7, HexB, Hex5, HexC, Hex5, Hex9, Hex8, HexF,E} EOP,
  UNOP( "inc", Hex0, HexA),
  SOP("jmp"),{RSIND|B30, E}, {Hex5, Hex9, RSIND|B30, Hex0, E} EOP,
  SOP("jmp"),{ABS16ORREL8SRC, E}, {Hex5, HexA, Hex0, Hex0, ABS16ORREL8SRC, IGNORE,IGNORE,IGNORE,E} EOP,
  SOP("jmp"),{MEMIND, E}, {Hex5, HexB, MEMIND,IGNORE, E} EOP,
  SOP("jsr"),{RSIND|B30, E}, {Hex5, HexD, RSIND|B30, Hex0, E} EOP,
  SOP("jsr"),{ABS16ORREL8SRC, E}, {Hex5, HexE, Hex0, Hex0,
			      ABS16ORREL8SRC,IGNORE,IGNORE,IGNORE, E} EOP,
  SOP("jsr"),{MEMIND, E}, {Hex5, HexF, MEMIND, IGNORE,E} EOP,
  SOP("ldc"),{IMM8, CCR, E}, { Hex0, Hex7, IMM8,IGNORE, E} EOP,
  SOP("ldc"),{RS8, CCR, E}, { Hex0, Hex3, Hex0, RS8, E} EOP,
  SOP("mov.b"),{RS8, RD8, E}, { Hex0, HexC, RS8, RD8, E} EOP,
  SOP("mov.b"),{IMM8, RD8, E}, { HexF, RD8, IMM8,IGNORE, E} EOP,
  SOP("mov.b"),{RSIND|B30,RD8, E}, { Hex6, Hex8, RSIND|B30, RD8, E} EOP,
  SOP("mov.b"),{DISPSRC,RD8, E}, { Hex6, HexE, DISPREG|B30, RD8,
				    DISPSRC, IGNORE, IGNORE, IGNORE, E} EOP,
  SOP("mov.b"),{RSINC|B30, RD8, E}, { Hex6, HexC, RSINC|B30, RD8, E} EOP,
  SOP("mov.b"),{ABS16OR8SRC, RD8, E}, { Hex6, HexA, Hex0, RD8,ABS16OR8SRC,
				      IGNORE,IGNORE,IGNORE,E} EOP,
  SOP("mov.b"),{ABS8SRC, RD8, E}, { Hex2, RD8, ABS8SRC,IGNORE, E} EOP,
  SOP("mov.b"),{RS8, RDIND|B30, E}, { Hex6, Hex8, RDIND|B31, RS8, E} EOP,
  SOP("mov.b"),{RS8, DISPDST, E}, { Hex6, HexE, DISPREG|B31,
				     RS8,DISPDST, IGNORE, IGNORE, IGNORE, E} EOP,
  SOP("mov.b"),{RS8, RDDEC|B31, E}, { Hex6, HexC, RDDEC|B31, RS8, E} EOP,
  SOP( "mov.b"),{RS8, ABS16OR8DST, E}, { Hex6, HexA, Hex8, RS8,
				       ABS16OR8DST,IGNORE,IGNORE,IGNORE, E} EOP,
  SOP( "mov.b"),{RS8, ABS8DST, E}, { Hex3, RS8, ABS8DST,IGNORE, E} EOP,
  SOP( "mov.w"),{RS16|B30, RD16|B30, E},{ Hex0, HexD, RS16|B30,
					   RD16|B30, E} EOP,
  SOP("mov.w"),{IMM16, RD16|B30, E}, { Hex7, Hex9, Hex0, RD16|B30,
					IMM16,IGNORE,IGNORE,IGNORE, E} EOP,
  SOP("mov.w"),{RSIND|B30,RD16|B30, E},{ Hex6, Hex9, RSIND|B30,
					  RD16|B30, E} EOP,
  SOP("mov.w"),{DISPSRC,RD16|B30, E}, { Hex6, HexF, DISPREG|B30,
					 RD16|B30, DISPSRC, IGNORE, IGNORE, IGNORE,E} EOP,
  SOP("mov.w"),{RSINC|B30, RD16|B30, E}, { Hex6, HexD, RSINC|B30,
 RD16|B30, E}EOP,
  SOP("mov.w"), {ABS16SRC, RD16|B30, E}, { Hex6, HexB, Hex0,
					  RD16|B30,ABS16SRC,IGNORE,IGNORE,IGNORE, E} EOP,
SOP("mov.w"),  {RS16|B30, RDIND|B30, E},{ Hex6, Hex9, RDIND|B31,
					   RS16|B30, E} EOP,
SOP("mov.w"), {RS16|B30, DISPDST, E}, { Hex6, HexF, DISPREG|B31,
					 RS16|B30,DISPDST, IGNORE,IGNORE,IGNORE,E} EOP,
SOP("mov.w"), {RS16|B30, RDDEC|B30, E},{ Hex6, HexD, RDDEC|B31,
					  RS16|B30, E} EOP,
SOP("mov.w"), {RS16|B30, ABS16DST, E}, { Hex6, HexB, Hex8, RS16|B30,
					  ABS16DST, IGNORE, IGNORE, IGNORE, E} EOP,
SOP("movfpe"), {ABS16SRC, RD8, E}, { Hex6, HexA, Hex4, RD8,
				      ABS16SRC,IGNORE,IGNORE,IGNORE, E} EOP,
SOP("movtpe"), {RS8, ABS16DST, E}, { Hex6, HexA, HexC, RS8,
				      ABS16DST,IGNORE,IGNORE,IGNORE,
				      E} EOP,
SOP("mulxu"), {RS8, RD16|B30, E}, { Hex5, Hex0, RS8, RD16|B30, E} EOP,
SOP( "neg"), {RS8, E},  { Hex1, Hex7, Hex8, RS8, E} EOP,
SOP( "nop"), {E},  { Hex0, Hex0, Hex0, Hex0,E} EOP,
SOP( "not"), {RS8,E},  { Hex1, Hex7, Hex0, RS8,E} EOP,
TWOOP("or", HexC, Hex1, Hex4),
SOP( "orc"), {IMM8, CCR,E}, { Hex0, Hex4, IMM8,IGNORE,E} EOP,
SOP( "pop"), {RS16|B30,E},  { Hex6, HexD, Hex7, RS16|B30,E} EOP,
SOP( "push"), {RS16|B30,E},  { Hex6, HexD, HexF, RS16|B30,E} EOP,
 UNOP3( "rotl",Hex1, Hex2,Hex8),
 UNOP3( "rotr",Hex1, Hex3, Hex8),
 UNOP3( "rotxl",Hex1, Hex2, Hex0),
 UNOP3( "rotxr",Hex1, Hex3, Hex0),
SOP("rte"), {E},  { Hex5, Hex6, Hex7, Hex0,E} EOP,
SOP("rts"), {E},  { Hex5, Hex4, Hex7, Hex0,E} EOP,
 UNOP3( "shal", Hex1, Hex0, Hex8),
 UNOP3( "shar", Hex1, Hex1, Hex8),
 UNOP3( "shll", Hex1, Hex0, Hex0),
 UNOP3( "shlr", Hex1, Hex1, Hex0),
SOP("sleep"), {E},  { Hex0, Hex1, Hex8, Hex0,E} EOP,
SOP("stc"), {CCR, RD8,E}, { Hex0, Hex2, Hex0, RD8,E} EOP,
SOP("sub.b"), {RS8,RD8,E}, { Hex1, Hex8, RS8, RD8,E} EOP,
SOP("sub.w"), {RS16|B30, RD16|B30,E},  {Hex1, Hex9, RS16|B30,RD16|B30,E} EOP,
SOP("subs"), {KBIT,RD16|B30,E}, { Hex1, HexB, KBIT, RD16|B30,E} EOP,
 TWOOP("subx",HexB, Hex1, HexE),
 TWOOP("xor", HexD, Hex1, Hex5),
SOP("xorc"), {IMM8, CCR,E}, { Hex0, Hex5, IMM8,IGNORE,E} EOP,
 0
};
#else
extern struct h8_opcode h8_opcodes[] ;
#endif




