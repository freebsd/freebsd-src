/* ia64-opc-a.c -- IA-64 `A' opcode table.
   Copyright 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

   This file is part of GDB, GAS, and the GNU binutils.

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public
   License as published by the Free Software Foundation; either version
   2, or (at your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they
   will be useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this file; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "ia64-opc.h"

#define A	IA64_TYPE_A, 1
#define A2	IA64_TYPE_A, 2

/* instruction bit fields: */
#define bC(x)		(((ia64_insn) ((x) & 0x1)) << 12)
#define bImm14(x)	((((ia64_insn) (((x) >>  0) & 0x7f)) << 13) | \
			 (((ia64_insn) (((x) >>  7) & 0x3f)) << 27) | \
			 (((ia64_insn) (((x) >> 13) & 0x01)) << 36))
#define bR3a(x)		(((ia64_insn) ((x) & 0x7f)) << 20)
#define bR3b(x)		(((ia64_insn) ((x) & 0x3)) << 20)
#define bTa(x)		(((ia64_insn) ((x) & 0x1)) << 33)
#define bTb(x)		(((ia64_insn) ((x) & 0x1)) << 36)
#define bVe(x)		(((ia64_insn) ((x) & 0x1)) << 33)
#define bX(x)		(((ia64_insn) ((x) & 0x1)) << 33)
#define bX2(x)		(((ia64_insn) ((x) & 0x3)) << 34)
#define bX2a(x)		(((ia64_insn) ((x) & 0x3)) << 34)
#define bX2b(x)		(((ia64_insn) ((x) & 0x3)) << 27)
#define bX4(x)		(((ia64_insn) ((x) & 0xf)) << 29)
#define bZa(x)		(((ia64_insn) ((x) & 0x1)) << 36)
#define bZb(x)		(((ia64_insn) ((x) & 0x1)) << 33)

/* instruction bit masks: */
#define mC	bC (-1)
#define mImm14	bImm14 (-1)
#define mR3a	bR3a (-1)
#define mR3b	bR3b (-1)
#define mTa	bTa (-1)
#define mTb	bTb (-1)
#define mVe	bVe (-1)
#define mX	bX (-1)
#define mX2	bX2 (-1)
#define mX2a	bX2a (-1)
#define mX2b	bX2b (-1)
#define mX4	bX4 (-1)
#define mZa	bZa (-1)
#define mZb	bZb (-1)

#define OpR3b(a,b)		(bOp (a) | bR3b (b)), (mOp | mR3b)
#define OpX2aVe(a,b,c)		(bOp (a) | bX2a (b) | bVe (c)), \
				(mOp | mX2a | mVe)
#define OpX2aVeR3a(a,b,c,d)	(bOp (a) | bX2a (b) | bVe (c) | bR3a (d)), \
				(mOp | mX2a | mVe | mR3a)
#define OpX2aVeImm14(a,b,c,d)	(bOp (a) | bX2a (b) | bVe (c) | bImm14 (d)), \
				(mOp | mX2a | mVe | mImm14)
#define OpX2aVeX4(a,b,c,d)	(bOp (a) | bX2a (b) | bVe (c) | bX4 (d)), \
				(mOp | mX2a | mVe | mX4)
#define OpX2aVeX4X2b(a,b,c,d,e)	\
	(bOp (a) | bX2a (b) | bVe (c) | bX4 (d) | bX2b (e)), \
	(mOp | mX2a | mVe | mX4 | mX2b)
#define OpX2TbTaC(a,b,c,d,e) \
	(bOp (a) | bX2 (b) | bTb (c) | bTa (d) | bC (e)), \
	(mOp | mX2 | mTb | mTa | mC)
#define OpX2TaC(a,b,c,d)	(bOp (a) | bX2 (b) | bTa (c) | bC (d)), \
				(mOp | mX2 | mTa | mC)
#define OpX2aZaZbX4(a,b,c,d,e) \
	(bOp (a) | bX2a (b) | bZa (c) | bZb (d) | bX4 (e)), \
	(mOp | mX2a | mZa | mZb | mX4)
#define OpX2aZaZbX4X2b(a,b,c,d,e,f) \
	(bOp (a) | bX2a (b) | bZa (c) | bZb (d) | bX4 (e) | bX2b (f)), \
	(mOp | mX2a | mZa | mZb | mX4 | mX2b)

struct ia64_opcode ia64_opcodes_a[] =
  {
    /* A-type instruction encodings (sorted according to major opcode) */

    {"add",	 A, OpX2aVeX4X2b (8, 0, 0, 0, 0), {R1, R2, R3}},
    {"add",	 A, OpX2aVeX4X2b (8, 0, 0, 0, 1), {R1, R2, R3, C1}},
    {"sub",	 A, OpX2aVeX4X2b (8, 0, 0, 1, 1), {R1, R2, R3}},
    {"sub",	 A, OpX2aVeX4X2b (8, 0, 0, 1, 0), {R1, R2, R3, C1}},
    {"addp4",	 A, OpX2aVeX4X2b (8, 0, 0, 2, 0), {R1, R2, R3}},
    {"and",	 A, OpX2aVeX4X2b (8, 0, 0, 3, 0), {R1, R2, R3}},
    {"andcm",	 A, OpX2aVeX4X2b (8, 0, 0, 3, 1), {R1, R2, R3}},
    {"or",	 A, OpX2aVeX4X2b (8, 0, 0, 3, 2), {R1, R2, R3}},
    {"xor",	 A, OpX2aVeX4X2b (8, 0, 0, 3, 3), {R1, R2, R3}},
    {"shladd",	 A, OpX2aVeX4 (8, 0, 0, 4), {R1, R2, CNT2a, R3}},
    {"shladdp4", A, OpX2aVeX4 (8, 0, 0, 6), {R1, R2, CNT2a, R3}},
    {"sub",	 A, OpX2aVeX4X2b (8, 0, 0, 9, 1), {R1, IMM8, R3}},
    {"and",	 A, OpX2aVeX4X2b (8, 0, 0, 0xb, 0), {R1, IMM8, R3}},
    {"andcm",	 A, OpX2aVeX4X2b (8, 0, 0, 0xb, 1), {R1, IMM8, R3}},
    {"or",	 A, OpX2aVeX4X2b (8, 0, 0, 0xb, 2), {R1, IMM8, R3}},
    {"xor",	 A, OpX2aVeX4X2b (8, 0, 0, 0xb, 3), {R1, IMM8, R3}},
    {"mov",	 A, OpX2aVeImm14 (8, 2, 0, 0), {R1, R3}},
    {"mov",	 A, OpX2aVeR3a (8, 2, 0, 0), {R1, IMM14}, PSEUDO},
    {"adds",	 A, OpX2aVe (8, 2, 0), {R1, IMM14, R3}},
    {"addp4",	 A, OpX2aVe (8, 3, 0), {R1, IMM14, R3}},
    {"padd1",		 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 0, 0), {R1, R2, R3}},
    {"padd2",		 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 0, 0), {R1, R2, R3}},
    {"padd4",		 A, OpX2aZaZbX4X2b (8, 1, 1, 0, 0, 0), {R1, R2, R3}},
    {"padd1.sss",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 0, 1), {R1, R2, R3}},
    {"padd2.sss",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 0, 1), {R1, R2, R3}},
    {"padd1.uuu",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 0, 2), {R1, R2, R3}},
    {"padd2.uuu",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 0, 2), {R1, R2, R3}},
    {"padd1.uus",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 0, 3), {R1, R2, R3}},
    {"padd2.uus",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 0, 3), {R1, R2, R3}},
    {"psub1",		 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 1, 0), {R1, R2, R3}},
    {"psub2",		 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 1, 0), {R1, R2, R3}},
    {"psub4",		 A, OpX2aZaZbX4X2b (8, 1, 1, 0, 1, 0), {R1, R2, R3}},
    {"psub1.sss",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 1, 1), {R1, R2, R3}},
    {"psub2.sss",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 1, 1), {R1, R2, R3}},
    {"psub1.uuu",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 1, 2), {R1, R2, R3}},
    {"psub2.uuu",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 1, 2), {R1, R2, R3}},
    {"psub1.uus",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 1, 3), {R1, R2, R3}},
    {"psub2.uus",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 1, 3), {R1, R2, R3}},
    {"pavg1",		 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 2, 2), {R1, R2, R3}},
    {"pavg2",		 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 2, 2), {R1, R2, R3}},
    {"pavg1.raz",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 2, 3), {R1, R2, R3}},
    {"pavg2.raz",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 2, 3), {R1, R2, R3}},
    {"pavgsub1",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 3, 2), {R1, R2, R3}},
    {"pavgsub2",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 3, 2), {R1, R2, R3}},
    {"pcmp1.eq",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 9, 0), {R1, R2, R3}},
    {"pcmp2.eq",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 9, 0), {R1, R2, R3}},
    {"pcmp4.eq",	 A, OpX2aZaZbX4X2b (8, 1, 1, 0, 9, 0), {R1, R2, R3}},
    {"pcmp1.gt",	 A, OpX2aZaZbX4X2b (8, 1, 0, 0, 9, 1), {R1, R2, R3}},
    {"pcmp2.gt",	 A, OpX2aZaZbX4X2b (8, 1, 0, 1, 9, 1), {R1, R2, R3}},
    {"pcmp4.gt",	 A, OpX2aZaZbX4X2b (8, 1, 1, 0, 9, 1), {R1, R2, R3}},
    {"pshladd2",	 A, OpX2aZaZbX4 (8, 1, 0, 1, 4), {R1, R2, CNT2b, R3}},
    {"pshradd2",	 A, OpX2aZaZbX4 (8, 1, 0, 1, 6), {R1, R2, CNT2b, R3}},

    {"mov",		 A, OpR3b (9, 0), {R1, IMM22}, PSEUDO},
    {"addl",		 A, Op    (9),	  {R1, IMM22, R3_2}},

    {"cmp.lt",		 A2, OpX2TbTaC (0xc, 0, 0, 0, 0), {P1, P2, R2, R3}},
    {"cmp.le",		 A2, OpX2TbTaC (0xc, 0, 0, 0, 0), {P2, P1, R3, R2}},
    {"cmp.gt",		 A2, OpX2TbTaC (0xc, 0, 0, 0, 0), {P1, P2, R3, R2}},
    {"cmp.ge",		 A2, OpX2TbTaC (0xc, 0, 0, 0, 0), {P2, P1, R2, R3}},
    {"cmp.lt.unc",	 A2, OpX2TbTaC (0xc, 0, 0, 0, 1), {P1, P2, R2, R3}},
    {"cmp.le.unc",	 A2, OpX2TbTaC (0xc, 0, 0, 0, 1), {P2, P1, R3, R2}},
    {"cmp.gt.unc",	 A2, OpX2TbTaC (0xc, 0, 0, 0, 1), {P1, P2, R3, R2}},
    {"cmp.ge.unc",	 A2, OpX2TbTaC (0xc, 0, 0, 0, 1), {P2, P1, R2, R3}},
    {"cmp.eq.and",	 A2, OpX2TbTaC (0xc, 0, 0, 1, 0), {P1, P2, R2, R3}},
    {"cmp.ne.andcm",	 A2, OpX2TbTaC (0xc, 0, 0, 1, 0), {P1, P2, R2, R3}, PSEUDO},
    {"cmp.ne.and",	 A2, OpX2TbTaC (0xc, 0, 0, 1, 1), {P1, P2, R2, R3}},
    {"cmp.eq.andcm",	 A2, OpX2TbTaC (0xc, 0, 0, 1, 1), {P1, P2, R2, R3}, PSEUDO},
    {"cmp4.lt",		 A2, OpX2TbTaC (0xc, 1, 0, 0, 0), {P1, P2, R2, R3}},
    {"cmp4.le",		 A2, OpX2TbTaC (0xc, 1, 0, 0, 0), {P2, P1, R3, R2}},
    {"cmp4.gt",		 A2, OpX2TbTaC (0xc, 1, 0, 0, 0), {P1, P2, R3, R2}},
    {"cmp4.ge",		 A2, OpX2TbTaC (0xc, 1, 0, 0, 0), {P2, P1, R2, R3}},
    {"cmp4.lt.unc",	 A2, OpX2TbTaC (0xc, 1, 0, 0, 1), {P1, P2, R2, R3}},
    {"cmp4.le.unc",	 A2, OpX2TbTaC (0xc, 1, 0, 0, 1), {P2, P1, R3, R2}},
    {"cmp4.gt.unc",	 A2, OpX2TbTaC (0xc, 1, 0, 0, 1), {P1, P2, R3, R2}},
    {"cmp4.ge.unc",	 A2, OpX2TbTaC (0xc, 1, 0, 0, 1), {P2, P1, R2, R3}},
    {"cmp4.eq.and",	 A2, OpX2TbTaC (0xc, 1, 0, 1, 0), {P1, P2, R2, R3}},
    {"cmp4.ne.andcm",	 A2, OpX2TbTaC (0xc, 1, 0, 1, 0), {P1, P2, R2, R3}, PSEUDO},
    {"cmp4.ne.and",	 A2, OpX2TbTaC (0xc, 1, 0, 1, 1), {P1, P2, R2, R3}},
    {"cmp4.eq.andcm",	 A2, OpX2TbTaC (0xc, 1, 0, 1, 1), {P1, P2, R2, R3}, PSEUDO},
    {"cmp.gt.and",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 0), {P1, P2, GR0, R3}},
    {"cmp.lt.and",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.le.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.ge.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.le.and",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 1), {P1, P2, GR0, R3}},
    {"cmp.ge.and",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.gt.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.lt.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.ge.and",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 0), {P1, P2, GR0, R3}},
    {"cmp.le.and",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.lt.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.gt.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.lt.and",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 1), {P1, P2, GR0, R3}},
    {"cmp.gt.and",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.ge.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.le.andcm",	 A2, OpX2TbTaC (0xc, 0, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.gt.and",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 0), {P1, P2, GR0, R3}},
    {"cmp4.lt.and",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.le.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.ge.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.le.and",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 1), {P1, P2, GR0, R3}},
    {"cmp4.ge.and",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.gt.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.lt.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.ge.and",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 0), {P1, P2, GR0, R3}},
    {"cmp4.le.and",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.lt.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.gt.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.lt.and",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 1), {P1, P2, GR0, R3}},
    {"cmp4.gt.and",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.ge.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.le.andcm",	 A2, OpX2TbTaC (0xc, 1, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.lt",		 A2, OpX2TaC   (0xc, 2, 0, 0), {P1, P2, IMM8, R3}},
    {"cmp.le",		 A2, OpX2TaC   (0xc, 2, 0, 0), {P1, P2, IMM8M1, R3}},
    {"cmp.gt",		 A2, OpX2TaC   (0xc, 2, 0, 0), {P2, P1, IMM8M1, R3}},
    {"cmp.ge",		 A2, OpX2TaC   (0xc, 2, 0, 0), {P2, P1, IMM8, R3}},
    {"cmp.lt.unc",	 A2, OpX2TaC   (0xc, 2, 0, 1), {P1, P2, IMM8, R3}},
    {"cmp.le.unc",	 A2, OpX2TaC   (0xc, 2, 0, 1), {P1, P2, IMM8M1, R3}},
    {"cmp.gt.unc",	 A2, OpX2TaC   (0xc, 2, 0, 1), {P2, P1, IMM8M1, R3}},
    {"cmp.ge.unc",	 A2, OpX2TaC   (0xc, 2, 0, 1), {P2, P1, IMM8, R3}},
    {"cmp.eq.and",	 A2, OpX2TaC   (0xc, 2, 1, 0), {P1, P2, IMM8, R3}},
    {"cmp.ne.andcm",	 A2, OpX2TaC   (0xc, 2, 1, 0), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp.ne.and",	 A2, OpX2TaC   (0xc, 2, 1, 1), {P1, P2, IMM8, R3}},
    {"cmp.eq.andcm",	 A2, OpX2TaC   (0xc, 2, 1, 1), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp4.lt",		 A2, OpX2TaC   (0xc, 3, 0, 0), {P1, P2, IMM8, R3}},
    {"cmp4.le",		 A2, OpX2TaC   (0xc, 3, 0, 0), {P1, P2, IMM8M1, R3}},
    {"cmp4.gt",		 A2, OpX2TaC   (0xc, 3, 0, 0), {P2, P1, IMM8M1, R3}},
    {"cmp4.ge",		 A2, OpX2TaC   (0xc, 3, 0, 0), {P2, P1, IMM8, R3}},
    {"cmp4.lt.unc",	 A2, OpX2TaC   (0xc, 3, 0, 1), {P1, P2, IMM8, R3}},
    {"cmp4.le.unc",	 A2, OpX2TaC   (0xc, 3, 0, 1), {P1, P2, IMM8M1, R3}},
    {"cmp4.gt.unc",	 A2, OpX2TaC   (0xc, 3, 0, 1), {P2, P1, IMM8M1, R3}},
    {"cmp4.ge.unc",	 A2, OpX2TaC   (0xc, 3, 0, 1), {P2, P1, IMM8, R3}},
    {"cmp4.eq.and",	 A2, OpX2TaC   (0xc, 3, 1, 0), {P1, P2, IMM8, R3}},
    {"cmp4.ne.andcm",	 A2, OpX2TaC   (0xc, 3, 1, 0), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp4.ne.and",	 A2, OpX2TaC   (0xc, 3, 1, 1), {P1, P2, IMM8, R3}},
    {"cmp4.eq.andcm",	 A2, OpX2TaC   (0xc, 3, 1, 1), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp.ltu",		 A2, OpX2TbTaC (0xd, 0, 0, 0, 0), {P1, P2, R2, R3}},
    {"cmp.leu",		 A2, OpX2TbTaC (0xd, 0, 0, 0, 0), {P2, P1, R3, R2}},
    {"cmp.gtu",		 A2, OpX2TbTaC (0xd, 0, 0, 0, 0), {P1, P2, R3, R2}},
    {"cmp.geu",		 A2, OpX2TbTaC (0xd, 0, 0, 0, 0), {P2, P1, R2, R3}},
    {"cmp.ltu.unc",	 A2, OpX2TbTaC (0xd, 0, 0, 0, 1), {P1, P2, R2, R3}},
    {"cmp.leu.unc",	 A2, OpX2TbTaC (0xd, 0, 0, 0, 1), {P2, P1, R3, R2}},
    {"cmp.gtu.unc",	 A2, OpX2TbTaC (0xd, 0, 0, 0, 1), {P1, P2, R3, R2}},
    {"cmp.geu.unc",	 A2, OpX2TbTaC (0xd, 0, 0, 0, 1), {P2, P1, R2, R3}},
    {"cmp.eq.or",	 A2, OpX2TbTaC (0xd, 0, 0, 1, 0), {P1, P2, R2, R3}},
    {"cmp.ne.orcm",	 A2, OpX2TbTaC (0xd, 0, 0, 1, 0), {P1, P2, R2, R3}, PSEUDO},
    {"cmp.ne.or",	 A2, OpX2TbTaC (0xd, 0, 0, 1, 1), {P1, P2, R2, R3}},
    {"cmp.eq.orcm",	 A2, OpX2TbTaC (0xd, 0, 0, 1, 1), {P1, P2, R2, R3}, PSEUDO},
    {"cmp4.ltu",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 0), {P1, P2, R2, R3}},
    {"cmp4.leu",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 0), {P2, P1, R3, R2}},
    {"cmp4.gtu",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 0), {P1, P2, R3, R2}},
    {"cmp4.geu",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 0), {P2, P1, R2, R3}},
    {"cmp4.ltu.unc",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 1), {P1, P2, R2, R3}},
    {"cmp4.leu.unc",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 1), {P2, P1, R3, R2}},
    {"cmp4.gtu.unc",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 1), {P1, P2, R3, R2}},
    {"cmp4.geu.unc",	 A2, OpX2TbTaC (0xd, 1, 0, 0, 1), {P2, P1, R2, R3}},
    {"cmp4.eq.or",	 A2, OpX2TbTaC (0xd, 1, 0, 1, 0), {P1, P2, R2, R3}},
    {"cmp4.ne.orcm",	 A2, OpX2TbTaC (0xd, 1, 0, 1, 0), {P1, P2, R2, R3}, PSEUDO},
    {"cmp4.ne.or",	 A2, OpX2TbTaC (0xd, 1, 0, 1, 1), {P1, P2, R2, R3}},
    {"cmp4.eq.orcm",	 A2, OpX2TbTaC (0xd, 1, 0, 1, 1), {P1, P2, R2, R3}, PSEUDO},
    {"cmp.gt.or",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 0), {P1, P2, GR0, R3}},
    {"cmp.lt.or",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.le.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.ge.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.le.or",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 1), {P1, P2, GR0, R3}},
    {"cmp.ge.or",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.gt.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.lt.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.ge.or",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 0), {P1, P2, GR0, R3}},
    {"cmp.le.or",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.lt.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.gt.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.lt.or",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 1), {P1, P2, GR0, R3}},
    {"cmp.gt.or",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.ge.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp.le.orcm",	 A2, OpX2TbTaC (0xd, 0, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.gt.or",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 0), {P1, P2, GR0, R3}},
    {"cmp4.lt.or",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.le.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.ge.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.le.or",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 1), {P1, P2, GR0, R3}},
    {"cmp4.ge.or",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.gt.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.lt.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.ge.or",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 0), {P1, P2, GR0, R3}},
    {"cmp4.le.or",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.lt.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 0), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.gt.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.lt.or",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 1), {P1, P2, GR0, R3}},
    {"cmp4.gt.or",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.ge.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 1), {P1, P2, GR0, R3}, PSEUDO},
    {"cmp4.le.orcm",	 A2, OpX2TbTaC (0xd, 1, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.ltu",		 A2, OpX2TaC   (0xd, 2, 0, 0), {P1, P2, IMM8, R3}},
    {"cmp.leu",		 A2, OpX2TaC   (0xd, 2, 0, 0), {P1, P2, IMM8M1U8, R3}},
    {"cmp.gtu",		 A2, OpX2TaC   (0xd, 2, 0, 0), {P2, P1, IMM8M1U8, R3}},
    {"cmp.geu",		 A2, OpX2TaC   (0xd, 2, 0, 0), {P2, P1, IMM8, R3}},
    {"cmp.ltu.unc",	 A2, OpX2TaC   (0xd, 2, 0, 1), {P1, P2, IMM8, R3}},
    {"cmp.leu.unc",	 A2, OpX2TaC   (0xd, 2, 0, 1), {P1, P2, IMM8M1U8, R3}},
    {"cmp.gtu.unc",	 A2, OpX2TaC   (0xd, 2, 0, 1), {P2, P1, IMM8M1U8, R3}},
    {"cmp.geu.unc",	 A2, OpX2TaC   (0xd, 2, 0, 1), {P2, P1, IMM8, R3}},
    {"cmp.eq.or",	 A2, OpX2TaC   (0xd, 2, 1, 0), {P1, P2, IMM8, R3}},
    {"cmp.ne.orcm",	 A2, OpX2TaC   (0xd, 2, 1, 0), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp.ne.or",	 A2, OpX2TaC   (0xd, 2, 1, 1), {P1, P2, IMM8, R3}},
    {"cmp.eq.orcm",	 A2, OpX2TaC   (0xd, 2, 1, 1), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp4.ltu",	 A2, OpX2TaC   (0xd, 3, 0, 0), {P1, P2, IMM8U4, R3}},
    {"cmp4.leu",	 A2, OpX2TaC   (0xd, 3, 0, 0), {P1, P2, IMM8M1U4, R3}},
    {"cmp4.gtu",	 A2, OpX2TaC   (0xd, 3, 0, 0), {P2, P1, IMM8M1U4, R3}},
    {"cmp4.geu",	 A2, OpX2TaC   (0xd, 3, 0, 0), {P2, P1, IMM8U4, R3}},
    {"cmp4.ltu.unc",	 A2, OpX2TaC   (0xd, 3, 0, 1), {P1, P2, IMM8U4, R3}},
    {"cmp4.leu.unc",	 A2, OpX2TaC   (0xd, 3, 0, 1), {P1, P2, IMM8M1U4, R3}},
    {"cmp4.gtu.unc",	 A2, OpX2TaC   (0xd, 3, 0, 1), {P2, P1, IMM8M1U4, R3}},
    {"cmp4.geu.unc",	 A2, OpX2TaC   (0xd, 3, 0, 1), {P2, P1, IMM8U4, R3}},
    {"cmp4.eq.or",	 A2, OpX2TaC   (0xd, 3, 1, 0), {P1, P2, IMM8, R3}},
    {"cmp4.ne.orcm",	 A2, OpX2TaC   (0xd, 3, 1, 0), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp4.ne.or",	 A2, OpX2TaC   (0xd, 3, 1, 1), {P1, P2, IMM8, R3}},
    {"cmp4.eq.orcm",	 A2, OpX2TaC   (0xd, 3, 1, 1), {P1, P2, IMM8, R3}, PSEUDO},
    {"cmp.eq",		 A2, OpX2TbTaC (0xe, 0, 0, 0, 0), {P1, P2, R2, R3}},
    {"cmp.ne",		 A2, OpX2TbTaC (0xe, 0, 0, 0, 0), {P2, P1, R2, R3}},
    {"cmp.eq.unc",	 A2, OpX2TbTaC (0xe, 0, 0, 0, 1), {P1, P2, R2, R3}},
    {"cmp.ne.unc",	 A2, OpX2TbTaC (0xe, 0, 0, 0, 1), {P2, P1, R2, R3}},
    {"cmp.eq.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 0, 1, 0), {P1, P2, R2, R3}},
    {"cmp.ne.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 0, 1, 0), {P2, P1, R2, R3}, PSEUDO},
    {"cmp.ne.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 0, 1, 1), {P1, P2, R2, R3}},
    {"cmp.eq.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 0, 1, 1), {P2, P1, R2, R3}, PSEUDO},
    {"cmp4.eq",		 A2, OpX2TbTaC (0xe, 1, 0, 0, 0), {P1, P2, R2, R3}},
    {"cmp4.ne",		 A2, OpX2TbTaC (0xe, 1, 0, 0, 0), {P2, P1, R2, R3}},
    {"cmp4.eq.unc",	 A2, OpX2TbTaC (0xe, 1, 0, 0, 1), {P1, P2, R2, R3}},
    {"cmp4.ne.unc",	 A2, OpX2TbTaC (0xe, 1, 0, 0, 1), {P2, P1, R2, R3}},
    {"cmp4.eq.or.andcm", A2, OpX2TbTaC (0xe, 1, 0, 1, 0), {P1, P2, R2, R3}},
    {"cmp4.ne.and.orcm", A2, OpX2TbTaC (0xe, 1, 0, 1, 0), {P2, P1, R2, R3}, PSEUDO},
    {"cmp4.ne.or.andcm", A2, OpX2TbTaC (0xe, 1, 0, 1, 1), {P1, P2, R2, R3}},
    {"cmp4.eq.and.orcm", A2, OpX2TbTaC (0xe, 1, 0, 1, 1), {P2, P1, R2, R3}, PSEUDO},
    {"cmp.gt.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 0), {P1, P2, GR0, R3}},
    {"cmp.lt.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.le.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 0), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp.ge.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 0), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp.le.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 1), {P1, P2, GR0, R3}},
    {"cmp.ge.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.gt.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 1), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp.lt.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 0, 1), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp.ge.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 0), {P1, P2, GR0, R3}},
    {"cmp.le.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.lt.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 0), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp.gt.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 0), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp.lt.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 1), {P1, P2, GR0, R3}},
    {"cmp.gt.or.andcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp.ge.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 1), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp.le.and.orcm",	 A2, OpX2TbTaC (0xe, 0, 1, 1, 1), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp4.gt.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 0), {P1, P2, GR0, R3}},
    {"cmp4.lt.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.le.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 0), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp4.ge.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 0), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp4.le.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 1), {P1, P2, GR0, R3}},
    {"cmp4.ge.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.gt.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 1), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp4.lt.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 0, 1), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp4.ge.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 0), {P1, P2, GR0, R3}},
    {"cmp4.le.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 0), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.lt.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 0), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp4.gt.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 0), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp4.lt.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 1), {P1, P2, GR0, R3}},
    {"cmp4.gt.or.andcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 1), {P1, P2, R3, GR0}, PSEUDO},
    {"cmp4.ge.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 1), {P2, P1, GR0, R3}, PSEUDO},
    {"cmp4.le.and.orcm", A2, OpX2TbTaC (0xe, 1, 1, 1, 1), {P2, P1, R3, GR0}, PSEUDO},
    {"cmp.eq",		 A2, OpX2TaC   (0xe, 2, 0, 0), {P1, P2, IMM8, R3}},
    {"cmp.ne",		 A2, OpX2TaC   (0xe, 2, 0, 0), {P2, P1, IMM8, R3}},
    {"cmp.eq.unc",	 A2, OpX2TaC   (0xe, 2, 0, 1), {P1, P2, IMM8, R3}},
    {"cmp.ne.unc",	 A2, OpX2TaC   (0xe, 2, 0, 1), {P2, P1, IMM8, R3}},
    {"cmp.eq.or.andcm",	 A2, OpX2TaC   (0xe, 2, 1, 0), {P1, P2, IMM8, R3}},
    {"cmp.ne.and.orcm",	 A2, OpX2TaC   (0xe, 2, 1, 0), {P2, P1, IMM8, R3}, PSEUDO},
    {"cmp.ne.or.andcm",	 A2, OpX2TaC   (0xe, 2, 1, 1), {P1, P2, IMM8, R3}},
    {"cmp.eq.and.orcm",	 A2, OpX2TaC   (0xe, 2, 1, 1), {P2, P1, IMM8, R3}, PSEUDO},
    {"cmp4.eq",		 A2, OpX2TaC   (0xe, 3, 0, 0), {P1, P2, IMM8, R3}},
    {"cmp4.ne",		 A2, OpX2TaC   (0xe, 3, 0, 0), {P2, P1, IMM8, R3}},
    {"cmp4.eq.unc",	 A2, OpX2TaC   (0xe, 3, 0, 1), {P1, P2, IMM8, R3}},
    {"cmp4.ne.unc",	 A2, OpX2TaC   (0xe, 3, 0, 1), {P2, P1, IMM8, R3}},
    {"cmp4.eq.or.andcm", A2, OpX2TaC   (0xe, 3, 1, 0), {P1, P2, IMM8, R3}},
    {"cmp4.ne.and.orcm", A2, OpX2TaC   (0xe, 3, 1, 0), {P2, P1, IMM8, R3}, PSEUDO},
    {"cmp4.ne.or.andcm", A2, OpX2TaC   (0xe, 3, 1, 1), {P1, P2, IMM8, R3}},
    {"cmp4.eq.and.orcm", A2, OpX2TaC   (0xe, 3, 1, 1), {P2, P1, IMM8, R3}, PSEUDO},

    {0}
  };

#undef A
#undef A2
#undef bC
#undef bImm14
#undef bR3a
#undef bR3b
#undef bTa
#undef bTb
#undef bVe
#undef bX
#undef bX2
#undef bX2a
#undef bX2b
#undef bX4
#undef bZa
#undef bZb
#undef mC
#undef mImm14
#undef mR3a
#undef mR3b
#undef mTa
#undef mTb
#undef mVe
#undef mX
#undef mX2
#undef mX2a
#undef mX2b
#undef mX4
#undef mZa
#undef mZb
#undef OpR3a
#undef OpR3b
#undef OpX2aVe
#undef OpX2aVeImm14
#undef OpX2aVeX4
#undef OpX2aVeX4X2b
#undef OpX2TbTaC
#undef OpX2TaC
#undef OpX2aZaZbX4
#undef OpX2aZaZbX4X2b
