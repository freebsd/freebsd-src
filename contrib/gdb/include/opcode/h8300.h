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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Instructions are stored as a sequence of nibbles.
   If the nibble has value 15 or less then the representation is complete.
   Otherwise, we record what it contains with several flags.  */

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
#define SRC             0x40
#define DST             0x80
#define L_8		0x01
#define L_16		0x02
#define L_32		0x04
#define L_P             0x08
#define L_24		0x10

#define REG		0x100
#define IMM		0x1000
#define DISP		0x2000
#define IND		0x4000
#define INC		0x8000
#define DEC		0x10000
#define L_3		0x20000
#define KBIT 		0x40000
#define DBIT            0x80000
#define DISPREG         0x100000
#define IGNORE 		0x200000
#define E      		0x400000		/* FIXME: end of nibble sequence? */
#define L_2		0x800000
#define CCR		0x4000000
#define ABS             0x8000000
#define B30  		0x1000000 		/* bit 3 must be low */
#define B31  		0x2000000		/* bit 3 must be high */
#define ABSJMP		0x10000000  
#define ABSMOV          0x20000000	
#define PCREL           0x40000000
#define MEMIND          0x80000000

#define IMM3 		IMM|L_3
#define IMM2 		IMM|L_2

#define SIZE		(L_2|L_3|L_8|L_16|L_32|L_P|L_24)
#define MODE		(REG|IMM|DISP|IND|INC|DEC|CCR|ABS|MEMIND)

#define RD8 		(DST|L_8|REG)
#define RD16 		(DST|L_16|REG)
#define RD32 		(DST|L_32|REG)
#define RS8 		(SRC|L_8|REG)
#define RS16 		(SRC|L_16|REG)
#define RS32		(SRC|L_32|REG)

#define RSP 		(SRC|L_P|REG)
#define RDP 		(DST|L_P|REG)

#define IMM8 		(IMM|SRC|L_8)
#define IMM16 		(IMM|SRC|L_16)
#define IMM32 		(IMM|SRC|L_32)

#define ABS8SRC 	(SRC|ABS|L_8)
#define ABS8DST		(DST|ABS|L_8)

#define DISP8 		(PCREL|L_8)
#define DISP16 		(PCREL|L_16)

#define DISP8SRC	(DISP|L_8|SRC)
#define DISP16SRC	(DISP|L_16|SRC)

#define DISP8DST	(DISP|L_8|DST)
#define DISP16DST	(DISP|L_16|DST)

#define ABS16SRC 	(SRC|ABS|L_16)
#define ABS16DST 	(DST|ABS|L_16)
#define ABS24SRC 	(SRC|ABS|L_24)
#define ABS24DST 	(DST|ABS|L_24)

#define RDDEC 		(DST|DEC)
#define RSINC 		(SRC|INC)

#define RDIND 		(DST|IND)
#define RSIND 		(SRC|IND)

#if 1
#define OR8 RS8		/* ??? OR as in One Register? */
#define OR16 RS16
#define OR32 RS32
#else
#define OR8 RD8
#define OR16 RD16
#define OR32 RD32
#endif

struct code 
{
  op_type nib[30];
};

struct arg 
{
  op_type nib[3];
};

struct h8_opcode 
{
  int how;
  int inbase;
  int time;
  char *name;
  struct arg args;
  struct code data;
  int length;
  int noperands;
  int idx;
  int size;
};

#ifdef DEFINE_TABLE

#define BITOP(code, imm, name, op00, op01,op10,op11, op20,op21)\
{ code, 1, 2, name,	{imm,RD8,E},	{op00, op01, imm, RD8, E, 0, 0, 0, 0}, 0, 0, 0, 0},\
{ code, 1, 6, name,	{imm,RDIND,E},	{op10, op11, B30|RDIND, 0, op00,op01, imm, 0, E}, 0, 0, 0, 0},\
{ code, 1, 6, name,	{imm,ABS8DST,E},{op20, op21, ABS8DST, IGNORE, op00,op01, imm, 0,E}, 0, 0, 0, 0}

#define EBITOP(code, imm, name, op00, op01,op10,op11, op20,op21)\
   BITOP(code,imm, name, op00+1, op01, op10,op11, op20,op21),\
   BITOP(code,RS8,  name, op00, op01, op10,op11, op20,op21)

#define WTWOP(code,name, op1, op2) \
{ code, 1, 2, name, {RS16, RD16, E}, { op1, op2, RS16, RD16, E, 0, 0, 0, 0}, 0, 0, 0, 0}

#define BRANCH(code, name, op) \
{ code, 1, 4,name,{DISP8,E,0}, { 0x4, op, DISP8, IGNORE, E, 0, 0, 0, 0}, 0, 0, 0, 0}, \
{ code, 0, 6,name,{DISP16,E,0}, { 0x5, 0x8, op, 0x0, DISP16, IGNORE, IGNORE, IGNORE, E,0}, 0, 0, 0, 0} 

#define SOP(code, x,name) \
{code, 1, x,  name 

#define NEW_SOP(code, in,x,name) \
{code, in, x,  name 
#define EOP  ,0,0,0 }

#define TWOOP(code, name, op1, op2,op3) \
{ code,1, 2,name, {IMM8, RD8, E},	{ op1, RD8, IMM8, IGNORE, E, 0, 0, 0, 0}, 0, 0, 0, 0},\
{ code, 1, 2,name, {RS8, RD8, E},	{ op2, op3, RS8, RD8, E, 0, 0, 0, 0}, 0, 0, 0, 0} 

#define UNOP(code,name, op1, op2) \
{ code, 1, 2, name, {OR8, E, 0}, { op1, op2, 0, OR8, E, 0, 0, 0, 0}, 0, 0, 0, 0}

#define UNOP3(code, name, op1, op2, op3) \
{ O(code,SB), 1, 2, name, {OR8,  E, 0}, {op1, op2, op3+0, OR8,  E, 0, 0, 0, 0}, 0, 0, 0, 0}, \
{ O(code,SW), 0, 2, name, {OR16, E, 0}, {op1, op2, op3+1, OR16, E, 0, 0, 0, 0}, 0, 0, 0, 0}, \
{ O(code,SL), 0, 2, name, {OR32, E, 0}, {op1, op2, op3+3, OR32|B30, E, 0, 0, 0, 0}, 0, 0, 0, 0}

#define IMM32LIST IMM32,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE
#define IMM24LIST IMM24,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE
#define IMM16LIST IMM16,IGNORE,IGNORE,IGNORE
#define A16LIST L_16,IGNORE,IGNORE,IGNORE
#define DISP24LIST DISP|L_24,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE
#define ABS24LIST ABS|L_24,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE
#define A24LIST L_24,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE
#define PREFIX32 0x0,0x1,0x0,0x0
#define PREFIXLDC 0x0,0x1,0x4,0x0


#define O(op, size) (op*4+size)

#define O_RECOMPILE 0
#define O_ADD  1
#define O_ADDX 2
#define O_AND  3
#define O_BAND 4
#define O_BRA  5
#define O_BRN  6
#define O_BHI  7
#define O_BLS  8
#define O_BCC  9
#define O_BCS  10
#define O_BNE  11  
#define O_BVC  12
#define O_BVS  13
#define O_BPL  14
#define O_BMI  15
#define O_BGE  16
#define O_BLT  17
#define O_BGT  18
#define O_BLE  19
#define O_ANDC 20
#define O_BEQ 21
#define O_BCLR 22
#define O_BIAND 23
#define O_BILD 24
#define O_BIOR 25
#define O_BIXOR 26
#define O_BIST 27
#define O_BLD 28
#define O_BNOT 29
#define O_BSET 30
#define O_BSR 31
#define O_BXOR 32
#define O_CMP 33
#define O_DAA 34
#define O_DAS 35
#define O_DEC 36
#define O_DIVU 37
#define O_DIVS 38
#define O_INC 39
#define O_LDC 40
#define O_MOV_TO_MEM 41
#define O_OR 42
#define O_ROTL 43
#define O_ROTR 44
#define O_ROTXL 45
#define O_ROTXR 46
#define O_BPT 47
#define O_SHAL 48
#define O_SHAR 49
#define O_SHLL 50
#define O_SHLR 51
#define O_SUB  52
#define O_SUBS 53
#define O_TRAPA 54
#define O_XOR 55
#define O_XORC 56
#define O_BOR 57
#define O_BST 58
#define O_BTST 59
#define O_EEPMOV 60
#define O_EXTS 61
#define O_EXTU 62
#define O_JMP 63
#define O_JSR 64
#define O_MULU 65
#define O_MULS 66
#define O_NOP 67
#define O_NOT 68
#define O_ORC 69
#define O_RTE 70
#define O_STC 71
#define O_SUBX 72
#define O_NEG 73
#define O_RTS 74
#define O_SLEEP 75
#define O_ILL 76
#define O_ADDS 77
#define O_SYSCALL 78
#define O_MOV_TO_REG 79
#define O_LAST 80
#define SB 0
#define SW 1
#define SL 2


/* FIXME: Lots of insns have "E, 0, 0, 0, 0" in the nibble code sequences.
   Methinks the zeroes aren't necessary.  Once confirmed, nuke 'em.  */

struct h8_opcode h8_opcodes[] = 
{
  TWOOP(O(O_ADD,SB),"add.b", 0x8, 0x0,0x8),
  
  NEW_SOP(O(O_ADD,SW),1,2,"add.w"),{RS16,RD16,E },{0x0,0x9,RS16,RD16,E} EOP,
  NEW_SOP(O(O_ADD,SW),0,4,"add.w"),{IMM16,RD16,E },{0x7,0x9,0x1,RD16,IMM16,IGNORE,IGNORE,IGNORE,E} EOP,
  NEW_SOP(O(O_ADD,SL),0,2,"add.l"),{RS32,RD32,E }, {0x0,0xA,B31|RS32,B30|RD32,E} EOP,
  NEW_SOP(O(O_ADD,SL),0,6,"add.l"),{IMM32,RD32,E },{0x7,0xA,0x1,B30|RD32,IMM32LIST,E} EOP,
  NEW_SOP(O(O_ADDS,SL),1,2,"adds"), {KBIT,RDP,E},   {0x0,0xB,KBIT,RDP,E,0,0,0,0} EOP,

  TWOOP(O(O_ADDX,SB),"addx",0x9,0x0,0xE),
  TWOOP(O(O_AND,SB), "and.b",0xE,0x1,0x6),

  NEW_SOP(O(O_AND,SW),0,2,"and.w"),{RS16,RD16,E },{0x6,0x6,RS16,RD16,E} EOP,
  NEW_SOP(O(O_AND,SW),0,4,"and.w"),{IMM16,RD16,E },{0x7,0x9,0x6,RD16,IMM16,IGNORE,IGNORE,IGNORE,E} EOP,
  
  NEW_SOP(O(O_AND,SL),0,6,"and.l"),{IMM32,RD32,E },{0x7,0xA,0x6,B30|RD32,IMM32LIST,E} EOP,
  NEW_SOP(O(O_AND,SL),0,2,"and.l") ,{RS32,RD32,E },{0x0,0x1,0xF,0x0,0x6,0x6,B30|RS32,B30|RD32,E} EOP,

  NEW_SOP(O(O_ANDC,SB),1,2,"andc"), {IMM8,CCR,E},{ 0x0,0x6,IMM8,IGNORE,E,0,0,0,0} EOP,

  BITOP(O(O_BAND,SB), IMM3,"band",0x7,0x6,0x7,0xC,0x7,0xE),
  BRANCH(O(O_BRA,SB),"bra",0x0),
  BRANCH(O(O_BRA,SB),"bt",0x0),
  BRANCH(O(O_BRN,SB),"brn",0x1),
  BRANCH(O(O_BRN,SB),"bf",0x1),
  BRANCH(O(O_BHI,SB),"bhi",0x2),
  BRANCH(O(O_BLS,SB),"bls",0x3),
  BRANCH(O(O_BCC,SB),"bcc",0x4),
  BRANCH(O(O_BCC,SB),"bhs",0x4),
  BRANCH(O(O_BCS,SB),"bcs",0x5),
  BRANCH(O(O_BCS,SB),"blo",0x5),
  BRANCH(O(O_BNE,SB),"bne",0x6),
  BRANCH(O(O_BEQ,SB),"beq",0x7),
  BRANCH(O(O_BVC,SB),"bvc",0x8),
  BRANCH(O(O_BVS,SB),"bvs",0x9),
  BRANCH(O(O_BPL,SB),"bpl",0xA),
  BRANCH(O(O_BMI,SB),"bmi",0xB),
  BRANCH(O(O_BGE,SB),"bge",0xC),
  BRANCH(O(O_BLT,SB),"blt",0xD),
  BRANCH(O(O_BGT,SB),"bgt",0xE),
  BRANCH(O(O_BLE,SB),"ble",0xF),

  EBITOP(O(O_BCLR,SB),IMM3,    "bclr", 0x6,0x2,0x7,0xD,0x7,0xF),
  BITOP(O(O_BIAND,SB),IMM3|B31,"biand",0x7,0x6,0x7,0xC,0x7,0xE),
  BITOP(O(O_BILD,SB), IMM3|B31,"bild", 0x7,0x7,0x7,0xC,0x7,0xE),
  BITOP(O(O_BIOR,SB), IMM3|B31,"bior", 0x7,0x4,0x7,0xC,0x7,0xE),
  BITOP(O(O_BIST,SB), IMM3|B31,"bist", 0x6,0x7,0x7,0xD,0x7,0xF),
  BITOP(O(O_BIXOR,SB),IMM3|B31,"bixor",0x7,0x5,0x7,0xC,0x7,0xE),
  BITOP(O(O_BLD,SB),  IMM3|B30,"bld",  0x7,0x7,0x7,0xC,0x7,0xE),
  EBITOP(O(O_BNOT,SB),IMM3|B30,"bnot", 0x6,0x1,0x7,0xD,0x7,0xF),
  BITOP(O(O_BOR,SB),  IMM3|B30,"bor",  0x7,0x4,0x7,0xC,0x7,0xE),
  EBITOP(O(O_BSET,SB),IMM3|B30,"bset", 0x6,0x0,0x7,0xD,0x7,0xF),

  SOP(O(O_BSR,SB),6,"bsr"),{DISP8,E,0},{ 0x5,0x5,DISP8,IGNORE,E,0,0,0,0} EOP,
  SOP(O(O_BSR,SB),6,"bsr"),{DISP16,E,0},{ 0x5,0xC,0x0,0x0,DISP16,IGNORE,IGNORE,IGNORE,E,0,0,0,0} EOP,
  BITOP(O(O_BST,SB), IMM3|B30,"bst",0x6,0x7,0x7,0xD,0x7,0xF),
  EBITOP(O(O_BTST,SB), IMM3|B30,"btst",0x6,0x3,0x7,0xC,0x7,0xE),
  BITOP(O(O_BXOR,SB), IMM3|B30,"bxor",0x7,0x5,0x7,0xC,0x7,0xE),

  TWOOP(O(O_CMP,SB), "cmp.b",0xA,0x1,0xC),
  WTWOP(O(O_CMP,SW), "cmp.w",0x1,0xD),

  NEW_SOP(O(O_CMP,SW),1,2,"cmp.w"),{RS16,RD16,E },{0x1,0xD,RS16,RD16,E} EOP,
  NEW_SOP(O(O_CMP,SW),0,4,"cmp.w"),{IMM16,RD16,E },{0x7,0x9,0x2,RD16,IMM16,IGNORE,IGNORE,IGNORE,E} EOP,

  NEW_SOP(O(O_CMP,SL),0,6,"cmp.l"),{IMM32,RD32,E },{0x7,0xA,0x2,B30|RD32,IMM32LIST,E} EOP,
  NEW_SOP(O(O_CMP,SL),0,2,"cmp.l") ,{RS32,RD32,E },{0x1,0xF,B31|RS32,B30|RD32,E} EOP,

  UNOP(O(O_DAA,SB), "daa",0x0,0xF),
  UNOP(O(O_DAS,SB), "das",0x1,0xF),
  UNOP(O(O_DEC,SB), "dec.b",0x1,0xA),

  NEW_SOP(O(O_DEC, SW),0,2,"dec.w") ,{DBIT,RD16,E },{0x1,0xB,0x5|DBIT,RD16,E} EOP,
  NEW_SOP(O(O_DEC, SL),0,2,"dec.l") ,{DBIT,RD32,E },{0x1,0xB,0x7|DBIT,RD32|B30,E} EOP,

  NEW_SOP(O(O_DIVU,SB),1,6,"divxu.b"), {RS8,RD16,E}, {0x5,0x1,RS8,RD16,E,0,0,0,0}EOP,
  NEW_SOP(O(O_DIVU,SW),0,20,"divxu.w"),{RS16,RD32,E},{0x5,0x3,RS16,B30|RD32,E}EOP,
    
  NEW_SOP(O(O_DIVS,SB),0,20,"divxs.b") ,{RS8,RD16,E },{0x0,0x1,0xD,0x0,0x5,0x1,RS8,RD16,E} EOP,
  NEW_SOP(O(O_DIVS,SW),0,02,"divxs.w") ,{RS16,RD32,E },{0x0,0x1,0xD,0x0,0x5,0x3,RS16,B30|RD32,E} EOP,

  NEW_SOP(O(O_EEPMOV,SB),1,50,"eepmov"),{ E,0,0},{0x7,0xB,0x5,0xC,0x5,0x9,0x8,0xF,E}EOP,
  NEW_SOP(O(O_EEPMOV,SW),0,50,"eepmovw"),{E,0,0},{0x7,0xB,0xD,0x4,0x5,0x9,0x8,0xF,E} EOP,
    
  NEW_SOP(O(O_EXTS,SW),0,2,"exts.w"),{OR16,E,0},{0x1,0x7,0xD,OR16,E   }EOP,
  NEW_SOP(O(O_EXTS,SL),0,2,"exts.l"),{OR32,E,0},{0x1,0x7,0xF,OR32|B30,E   }EOP,

  NEW_SOP(O(O_EXTU,SW),0,2,"extu.w"),{OR16,E,0},{0x1,0x7,0x5,OR16,E   }EOP,
  NEW_SOP(O(O_EXTU,SL),0,2,"extu.l"),{OR32,E,0},{0x1,0x7,0x7,OR32|B30,E   }EOP,
    
  UNOP(O(O_INC,SB), "inc",0x0,0xA),

  NEW_SOP(O(O_INC,SW),0,2,"inc.w") ,{DBIT,RD16,E },{0x0,0xB,0x5|DBIT,RD16,E} EOP,
  NEW_SOP(O(O_INC,SL),0,2,"inc.l") ,{DBIT,RD32,E },{0x0,0xB,0x7|DBIT,RD32|B30,E} EOP,

  SOP(O(O_JMP,SB),4,"jmp"),{RSIND,E,0},{0x5,0x9,B30|RSIND,0x0,E,0,0,0,0}EOP,
  SOP(O(O_JMP,SB),6,"jmp"),{SRC|ABSJMP,E,0},{0x5,0xA,SRC|ABSJMP,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_JMP,SB),8,"jmp"),{SRC|MEMIND,E,0},{0x5,0xB,SRC|MEMIND,IGNORE,E,0,0,0,0}EOP,

  SOP(O(O_JSR,SB),6,"jsr"),{SRC|RSIND,E,0}, {0x5,0xD,B30|RSIND,0x0,E,0,0,0,0}EOP,
  SOP(O(O_JSR,SB),8,"jsr"),{SRC|ABSJMP,E,0},{0x5,0xE,SRC|ABSJMP,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_JSR,SB),8,"jsr"),{SRC|MEMIND,E,0},{0x5,0xF,SRC|MEMIND,IGNORE,E,0,0,0,0}EOP,

  NEW_SOP(O(O_LDC,SB),1,2,"ldc"),{IMM8,CCR,E},         { 0x0,0x7,IMM8,IGNORE,E,0,0,0,0}EOP,
  NEW_SOP(O(O_LDC,SB),1,2,"ldc"),{OR8,CCR,E},          { 0x0,0x3,0x0,OR8,E,0,0,0,0}EOP,
  NEW_SOP(O(O_LDC,SB),0,2,"ldc"),{ABS16SRC,CCR,E},     {PREFIXLDC,0x6,0xB,0x0,0x0,ABS16SRC,IGNORE,IGNORE,IGNORE,E}EOP,
  NEW_SOP(O(O_LDC,SB),0,2,"ldc"),{ABS24SRC,CCR,E},     {PREFIXLDC,0x6,0xB,0x2,0x0,0x0,0x0,SRC|ABS24LIST,E}EOP,
  NEW_SOP(O(O_LDC,SB),0,2,"ldc"),{DISP|SRC|L_16,CCR,E},{PREFIXLDC,0x6,0x9,B30|DISPREG,0,DISP|L_16,IGNORE,IGNORE,IGNORE,E}EOP,
  NEW_SOP(O(O_LDC,SB),0,2,"ldc"),{DISP|SRC|L_24,CCR,E},{PREFIXLDC,0x7,0x8,B30|DISPREG,0,0x6,0xB,0x2,0x0,0x0,0x0,SRC|DISP24LIST,E}EOP,
  NEW_SOP(O(O_LDC,SB),0,2,"ldc"),{RSINC,CCR,E},        {PREFIXLDC,0x6,0xD,B30|RSINC,0x0,E}EOP,
  NEW_SOP(O(O_LDC,SB),0,2,"ldc"),{RSIND,CCR,E},        {PREFIXLDC,0x6,0x9,B30|RDIND,0x0,E} EOP,


  SOP(O(O_MOV_TO_REG,SB),4,"mov.b"),{ABSMOV|ABS|SRC|L_16,RD8,E},  { 0x6,0xA,0x0,RD8,ABSMOV|SRC|ABS|A16LIST,E}EOP,
  SOP(O(O_MOV_TO_REG,SB),6,"mov.b"),{ABSMOV|ABS|SRC|L_24,RD8,E }, { 0x6,0xA,0x2,RD8,0x0,0x0,SRC|ABSMOV|ABS|A24LIST,E }EOP,
  SOP(O(O_MOV_TO_MEM,SB),4,"mov.b"),{RS8,ABSMOV|ABS|L_16|DST,E},     { 0x6,0xA,0x8,RS8,ABSMOV|DST|ABS|A16LIST,E}EOP,
  SOP(O(O_MOV_TO_MEM,SB),6,"mov.b"),{RS8,ABSMOV|ABS|DST|L_24,E },    { 0x6,0xA,0xA,RS8,0x0,0x0,DST|ABSMOV|ABS|A24LIST,E }EOP,
    
  SOP(O(O_MOV_TO_REG,SB),6,"mov.b"),{DISP|L_24|SRC,RD8,E},  { 0x7,0x8,B30|DISPREG,0x0,0x6,0xA,0x2,RD8,0x0,0x0,SRC|DISP24LIST,E}EOP,
  SOP(O(O_MOV_TO_MEM,SB),6,"mov.b"),{RS8,DISP|L_24|DST,E},  { 0x7,0x8,B30|DISPREG,0x0,0x6,0xA,0xA,RS8,0x0,0x0,DST|DISP24LIST,E}EOP,



  SOP(O(O_MOV_TO_REG,SB),2,"mov.b"),{RS8,RD8,E},	    { 0x0,0xC,RS8,RD8,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SB),2,"mov.b"),{IMM8,RD8,E},           { 0xF,RD8,IMM8,IGNORE,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SB),4,"mov.b"),{RSIND,RD8,E},          { 0x6,0x8,B30|RSIND,RD8,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SB),6,"mov.b"),{DISP16SRC,RD8,E},      { 0x6,0xE,B30|DISPREG,RD8,DISP16SRC,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_MOV_TO_REG,SB),6,"mov.b"),{RSINC,RD8,E},          { 0x6,0xC,B30|RSINC,RD8,E,0,0,0,0}EOP,

  SOP(O(O_MOV_TO_REG,SB),4,"mov.b"),{ABS8SRC,RD8,E},        { 0x2,RD8,ABS8SRC,IGNORE,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_MEM,SB),4,"mov.b"),{RS8,RDIND,E},          { 0x6,0x8,RDIND|B31,RS8,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_MEM,SB),6,"mov.b"),{RS8,DISP16DST,E},      { 0x6,0xE,DISPREG|B31,RS8,DISP16DST,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_MOV_TO_MEM,SB),6,"mov.b"),{RS8,RDDEC|B31,E},      { 0x6,0xC,RDDEC|B31,RS8,E,0,0,0,0}EOP,

  SOP(O(O_MOV_TO_MEM,SB),4,"mov.b"),{RS8,ABS8DST,E},        { 0x3,RS8,ABS8DST,IGNORE,E,0,0,0,0}EOP,

  SOP(O(O_MOV_TO_MEM,SW),6,"mov.w"),{RS16,RDIND,E},        { 0x6,0x9,RDIND|B31,RS16,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SW),6,"mov.w"),{DISP|L_24|SRC,RD16,E},{ 0x7,0x8,B30|DISPREG,0x0,0x6,0xB,0x2,RD16,0x0,0x0,SRC|DISP24LIST,E}EOP,
  SOP(O(O_MOV_TO_MEM,SW),6,"mov.w"),{RS16,DISP|L_24|DST,E},{ 0x7,0x8,B30|DISPREG,0x0,0x6,0xB,0xA,RS16,0x0,0x0,DST|DISP24LIST,E}EOP,
  SOP(O(O_MOV_TO_REG,SW),6,"mov.w"),{ABS|L_24|SRC,RD16,E },{ 0x6,0xB,0x2,RD16,0x0,0x0,SRC|ABS24LIST,E }EOP,
  SOP(O(O_MOV_TO_MEM,SW),6,"mov.w"),{RS16,ABS|L_24|DST,E },{ 0x6,0xB,0xA,RS16,0x0,0x0,DST|ABS24LIST,E }EOP,
  SOP(O(O_MOV_TO_REG,SW),2,"mov.w"),{RS16,RD16,E},         { 0x0,0xD,RS16, RD16,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SW),4,"mov.w"),{IMM16,RD16,E},        { 0x7,0x9,0x0,RD16,IMM16,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_MOV_TO_REG,SW),4,"mov.w"),{RSIND,RD16,E},        { 0x6,0x9,B30|RSIND,RD16,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SW),6,"mov.w"),{DISP16SRC,RD16,E},    { 0x6,0xF,B30|DISPREG,RD16,DISP16SRC,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_MOV_TO_REG,SW),6,"mov.w"),{RSINC,RD16,E},        { 0x6,0xD,B30|RSINC,RD16,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_REG,SW),6,"mov.w"),{ABS16SRC,RD16,E},     { 0x6,0xB,0x0,RD16,ABS16SRC,IGNORE,IGNORE,IGNORE,E}EOP,

  SOP(O(O_MOV_TO_MEM,SW),6,"mov.w"),{RS16,DISP16DST,E},    { 0x6,0xF,DISPREG|B31,RS16,DISP16DST,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_MOV_TO_MEM,SW),6,"mov.w"),{RS16,RDDEC,E},        { 0x6,0xD,RDDEC|B31,RS16,E,0,0,0,0}EOP,
  SOP(O(O_MOV_TO_MEM,SW),6,"mov.w"),{RS16,ABS16DST,E},     { 0x6,0xB,0x8,RS16,ABS16DST,IGNORE,IGNORE,IGNORE,E}EOP,

  SOP(O(O_MOV_TO_REG,SL),4,"mov.l"),{IMM32,RD32,E},        { 0x7,0xA,0x0,B30|RD32,IMM32LIST,E}EOP,
  SOP(O(O_MOV_TO_REG,SL),2,"mov.l"),{RS32,RD32,E},         { 0x0,0xF,B31|RS32,B30|RD32,E,0,0,0,0}EOP,

  SOP(O(O_MOV_TO_REG,SL),4,"mov.l"),{RSIND,RD32,E},        { PREFIX32,0x6,0x9,RSIND|B30,B30|RD32,E,0,0,0,0 }EOP,
  SOP(O(O_MOV_TO_REG,SL),6,"mov.l"),{DISP16SRC,RD32,E},    { PREFIX32,0x6,0xF,DISPREG|B30,B30|RD32,DISP16SRC,IGNORE,IGNORE,IGNORE,E }EOP,
  SOP(O(O_MOV_TO_REG,SL),6,"mov.l"),{DISP|L_24|SRC,RD32,E},{ PREFIX32,0x7,0x8,B30|DISPREG,0x0,0x6,0xB,0x2,B30|RD32,0x0,0x0,SRC|DISP24LIST,E }EOP,
  SOP(O(O_MOV_TO_REG,SL),6,"mov.l"),{RSINC,RD32,E},        { PREFIX32,0x6,0xD,B30|RSINC,B30|RD32,E,0,0,0,0 }EOP,
  SOP(O(O_MOV_TO_REG,SL),6,"mov.l"),{ABS16SRC,RD32,E},     { PREFIX32,0x6,0xB,0x0,B30|RD32,ABS16SRC,IGNORE,IGNORE,IGNORE,E }EOP,
  SOP(O(O_MOV_TO_REG,SL),6,"mov.l"),{ABS24SRC,RD32,E },    { PREFIX32,0x6,0xB,0x2,B30|RD32,0x0,0x0,SRC|ABS24LIST,E }EOP,
  SOP(O(O_MOV_TO_MEM,SL),6,"mov.l"),{RS32,RDIND,E},        { PREFIX32,0x6,0x9,RDIND|B31,B30|RS32,E,0,0,0,0 }EOP,
  SOP(O(O_MOV_TO_MEM,SL),6,"mov.l"),{RS32,DISP16DST,E},    { PREFIX32,0x6,0xF,DISPREG|B31,B30|RS32,DISP16DST,IGNORE,IGNORE,IGNORE,E }EOP,
  SOP(O(O_MOV_TO_MEM,SL),6,"mov.l"),{RS32,DISP|L_24|DST,E},{ PREFIX32,0x7,0x8,B31|DISPREG,0x0,0x6,0xB,0xA,B30|RS32,0x0,0x0,DST|DISP24LIST,E }EOP,
  SOP(O(O_MOV_TO_MEM,SL),6,"mov.l"),{RS32,RDDEC,E},        { PREFIX32,0x6,0xD,RDDEC|B31,B30|RS32,E,0,0,0,0 }EOP,
  SOP(O(O_MOV_TO_MEM,SL),6,"mov.l"),{RS32,ABS16DST,E},     { PREFIX32,0x6,0xB,0x8,B30|RS32,ABS16DST,IGNORE,IGNORE,IGNORE,E }EOP,
  SOP(O(O_MOV_TO_MEM,SL),6,"mov.l"),{RS32,ABS24DST,E },    { PREFIX32,0x6,0xB,0xA,B30|RS32,0x0,0x0,DST|ABS24LIST,E }EOP,

  SOP(O(O_MOV_TO_REG,SB),10,"movfpe"),{ABS16SRC,RD8,E},{ 0x6,0xA,0x4,RD8,ABS16SRC,IGNORE,IGNORE,IGNORE,E}EOP,
  SOP(O(O_MOV_TO_MEM,SB),10,"movtpe"),{RS8,ABS16DST,E},{ 0x6,0xA,0xC,RS8,ABS16DST,IGNORE,IGNORE,IGNORE,E}EOP,

  NEW_SOP(O(O_MULU,SB),1,14,"mulxu.b"),{RS8,RD16,E}, { 0x5,0x0,RS8,RD16,E,0,0,0,0}EOP,
  NEW_SOP(O(O_MULU,SW),0,14,"mulxu.w"),{RS16,RD32,E},{ 0x5,0x2,RS16,B30|RD32,E,0,0,0,0}EOP,

  NEW_SOP(O(O_MULS,SB),0,20,"mulxs.b"),{RS8,RD16,E}, { 0x0,0x1,0xc,0x0,0x5,0x0,RS8,RD16,E}EOP,
  NEW_SOP(O(O_MULS,SW),0,20,"mulxs.w"),{RS16,RD32,E},{ 0x0,0x1,0xc,0x0,0x5,0x2,RS16,B30|RD32,E}EOP,
  
  /* ??? This can use UNOP3.  */
  NEW_SOP(O(O_NEG,SB),1,2,"neg.b"),{ OR8,E, 0},{ 0x1,0x7,0x8,OR8,E,0,0,0,0}EOP,
  NEW_SOP(O(O_NEG,SW),0,2,"neg.w"),{ OR16,E,0},{ 0x1,0x7,0x9,OR16,E}EOP,
  NEW_SOP(O(O_NEG,SL),0,2,"neg.l"),{ OR32,E,0},{ 0x1,0x7,0xB,B30|OR32,E}EOP,
    
  NEW_SOP(O(O_NOP,SB),1,2,"nop"),{E,0,0},{ 0x0,0x0,0x0,0x0,E,0,0,0,0}EOP,

  /* ??? This can use UNOP3.  */
  NEW_SOP(O(O_NOT,SB),1,2,"not.b"),{ OR8,E, 0},{ 0x1,0x7,0x0,OR8,E,0,0,0,0}EOP,
  NEW_SOP(O(O_NOT,SW),0,2,"not.w"),{ OR16,E,0},{ 0x1,0x7,0x1,OR16,E}EOP,
  NEW_SOP(O(O_NOT,SL),0,2,"not.l"),{ OR32,E,0},{ 0x1,0x7,0x3,B30|OR32,E}EOP,

  TWOOP(O(O_OR, SB),"or.b",0xC,0x1,0x4),
  NEW_SOP(O(O_OR,SW),0,4,"or.w"),{IMM16,RD16,E },{0x7,0x9,0x4,RD16,IMM16,IGNORE,IGNORE,IGNORE,E} EOP,
  NEW_SOP(O(O_OR,SW),0,2,"or.w"),{RS16,RD16,E },{0x6,0x4,RS16,RD16,E} EOP,

  NEW_SOP(O(O_OR,SL),0,6,"or.l"),{IMM32,RD32,E },{0x7,0xA,0x4,B30|RD32,IMM32LIST,E} EOP,
  NEW_SOP(O(O_OR,SL),0,2,"or.l"),{RS32,RD32,E },{0x0,0x1,0xF,0x0,0x6,0x4,B30|RS32,B30|RD32,E} EOP,

  NEW_SOP(O(O_ORC,SB),1,2,"orc"),{IMM8,CCR,E},{ 0x0,0x4,IMM8,IGNORE,E,0,0,0,0}EOP,

  NEW_SOP(O(O_MOV_TO_REG,SW),1,6,"pop.w"),{OR16,E,0},{ 0x6,0xD,0x7,OR16,E,0,0,0,0}EOP,
  NEW_SOP(O(O_MOV_TO_REG,SL),0,6,"pop.l"),{OR32,E,0},{ PREFIX32,0x6,0xD,0x7,OR32|B30,E,0,0,0,0}EOP,
  NEW_SOP(O(O_MOV_TO_MEM,SW),1,6,"push.w"),{OR16,E,0},{ 0x6,0xD,0xF,OR16,E,0,0,0,0}EOP,
  NEW_SOP(O(O_MOV_TO_MEM,SL),0,6,"push.l"),{OR32,E,0},{ PREFIX32,0x6,0xD,0xF,OR32|B30,E,0,0,0,0}EOP,

  UNOP3(O_ROTL,  "rotl", 0x1,0x2,0x8),
  UNOP3(O_ROTR,  "rotr", 0x1,0x3,0x8),
  UNOP3(O_ROTXL, "rotxl",0x1,0x2,0x0),
  UNOP3(O_ROTXR, "rotxr",0x1,0x3,0x0),

  SOP(O(O_BPT,SB),  10,"bpt"),{E,0,0},{ 0x7,0xA,0xF,0xF,E,0,0,0,0}EOP,
  SOP(O(O_RTE,SB),  10,"rte"),{E,0,0},{ 0x5,0x6,0x7,0x0,E,0,0,0,0}EOP,
  SOP(O(O_RTS,SB),   8,"rts"),{E,0,0},{ 0x5,0x4,0x7,0x0,E,0,0,0,0}EOP,

  UNOP3(O_SHAL,  "shal",0x1,0x0,0x8),
  UNOP3(O_SHAR,  "shar",0x1,0x1,0x8),
  UNOP3(O_SHLL,  "shll",0x1,0x0,0x0),
  UNOP3(O_SHLR,  "shlr",0x1,0x1,0x0),

  SOP(O(O_SLEEP,SB),2,"sleep"),{E,0,0},{ 0x0,0x1,0x8,0x0,E,0,0,0,0} EOP,

  NEW_SOP(O(O_STC,SB), 1,2,"stc"),{CCR,RD8,E},{ 0x0,0x2,0x0,RD8,E,0,0,0,0} EOP,

  NEW_SOP(O(O_STC,SB),0,2,"stc"),{CCR,RSIND,E},        {PREFIXLDC,0x6,0x9,B31|RDIND,0x0,E} EOP,
  NEW_SOP(O(O_STC,SB),0,2,"stc"),{CCR,DISP|DST|L_16,E},{PREFIXLDC,0x6,0x9,B31|DISPREG,0,DST|DISP|L_16,IGNORE,IGNORE,IGNORE,E}EOP,
  NEW_SOP(O(O_STC,SB),0,2,"stc"),{CCR,DISP|DST|L_24,E},{PREFIXLDC,0x7,0x8,B31|DISPREG,0,0x6,0xB,0x2,0x0,0x0,0x0,DST|DISP24LIST,E}EOP,
  NEW_SOP(O(O_STC,SB),0,2,"stc"),{CCR,RDDEC,E},        {PREFIXLDC,0x6,0xD,B31|RDDEC,0x0,E}EOP,

  NEW_SOP(O(O_STC,SB),0,2,"stc"),{CCR,ABS16SRC,E},     {PREFIXLDC,0x6,0xB,0x8,0x0,ABS16DST,IGNORE,IGNORE,IGNORE,E}EOP,
  NEW_SOP(O(O_STC,SB),0,2,"stc"),{CCR,ABS24SRC,E},     {PREFIXLDC,0x6,0xB,0xA,0x0,0x0,0x0,DST|ABS24LIST,E}EOP,

  SOP(O(O_SUB,SB),2,"sub.b"),{RS8,RD8,E},{ 0x1,0x8,RS8,RD8,E,0,0,0,0}EOP,

  NEW_SOP(O(O_SUB,SW),1,2,"sub.w"),{RS16,RD16,E },  {0x1,0x9,RS16,RD16,E} EOP,
  NEW_SOP(O(O_SUB,SW),0,4,"sub.w"),{IMM16,RD16,E }, {0x7,0x9,0x3,RD16,IMM16,IGNORE,IGNORE,IGNORE,E} EOP,
  NEW_SOP(O(O_SUB,SL),0,2,"sub.l") ,{RS32,RD32,E }, {0x1,0xA,B31|RS32,B30|RD32,E} EOP,
  NEW_SOP(O(O_SUB,SL),0,6,"sub.l"), {IMM32,RD32,E },{0x7,0xA,0x3,B30|RD32,IMM32LIST,E} EOP,

  SOP(O(O_SUBS,SL),2,"subs"),{KBIT,RDP,E},{ 0x1,0xB,KBIT,RDP,E,0,0,0,0}EOP,
  TWOOP(O(O_SUBX,SB),"subx",0xB,0x1,0xE),

  NEW_SOP(O(O_TRAPA,SB),0,2,"trapa"),{ IMM2,E},  {0x5,0x7,IMM2,IGNORE,E  }EOP,

  TWOOP(O(O_XOR, SB),"xor",0xD,0x1,0x5),

  NEW_SOP(O(O_XOR,SW),0,4,"xor.w"),{IMM16,RD16,E },{0x7,0x9,0x5,RD16,IMM16,IGNORE,IGNORE,IGNORE,E} EOP,
  NEW_SOP(O(O_XOR,SW),0,2,"xor.w"),{RS16,RD16,E },{0x6,0x5,RS16,RD16,E} EOP,

  NEW_SOP(O(O_XOR,SL),0,6,"xor.l"),{IMM32,RD32,E },{0x7,0xA,0x5,B30|RD32,IMM32LIST,E} EOP,
  NEW_SOP(O(O_XOR,SL),0,2,"xor.l") ,{RS32,RD32,E },{0x0,0x1,0xF,0x0,0x6,0x5,B30|RS32,B30|RD32,E} EOP,

  SOP(O(O_XORC,SB),2,"xorc"),{IMM8,CCR,E},{ 0x0,0x5,IMM8,IGNORE,E,0,0,0,0}EOP,
  0
};
#else
extern struct h8_opcode h8_opcodes[] ;
#endif




