/* ia64-opc-b.c -- IA-64 `B' opcode table.
   Copyright 1998, 1999, 2000 Free Software Foundation, Inc.
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

#define B0	IA64_TYPE_B, 0
#define B	IA64_TYPE_B, 1

/* instruction bit fields: */
#define bBtype(x)	(((ia64_insn) ((x) & 0x7)) << 6)
#define bD(x)		(((ia64_insn) ((x) & 0x1)) << 35)
#define bIh(x)		(((ia64_insn) ((x) & 0x1)) << 35)
#define bPa(x)		(((ia64_insn) ((x) & 0x1)) << 12)
#define bPr(x)		(((ia64_insn) ((x) & 0x3f)) << 0)
#define bWha(x)		(((ia64_insn) ((x) & 0x3)) << 33)
#define bWhb(x)		(((ia64_insn) ((x) & 0x3)) << 3)
#define bX6(x)		(((ia64_insn) ((x) & 0x3f)) << 27)

#define mBtype		bBtype (-1)
#define mD		bD (-1)
#define mIh		bIh (-1)
#define mPa		bPa (-1)
#define mPr		bPr (-1)
#define mWha		bWha (-1)
#define mWhb		bWhb (-1)
#define mX6		bX6 (-1)

#define OpX6(a,b) 	(bOp (a) | bX6 (b)), (mOp | mX6)
#define OpPaWhaD(a,b,c,d) \
	(bOp (a) | bPa (b) | bWha (c) | bD (d)), (mOp | mPa | mWha | mD)
#define OpBtypePaWhaD(a,b,c,d,e) \
	(bOp (a) | bBtype (b) | bPa (c) | bWha (d) | bD (e)), \
	(mOp | mBtype | mPa | mWha | mD)
#define OpBtypePaWhaDPr(a,b,c,d,e,f) \
	(bOp (a) | bBtype (b) | bPa (c) | bWha (d) | bD (e) | bPr (f)), \
	(mOp | mBtype | mPa | mWha | mD | mPr)
#define OpX6BtypePaWhaD(a,b,c,d,e,f) \
	(bOp (a) | bX6 (b) | bBtype (c) | bPa (d) | bWha (e) | bD (f)), \
	(mOp | mX6 | mBtype | mPa | mWha | mD)
#define OpX6BtypePaWhaDPr(a,b,c,d,e,f,g) \
   (bOp (a) | bX6 (b) | bBtype (c) | bPa (d) | bWha (e) | bD (f) | bPr (g)), \
	(mOp | mX6 | mBtype | mPa | mWha | mD | mPr)
#define OpIhWhb(a,b,c) \
	(bOp (a) | bIh (b) | bWhb (c)), \
	(mOp | mIh | mWhb)
#define OpX6IhWhb(a,b,c,d) \
	(bOp (a) | bX6 (b) | bIh (c) | bWhb (d)), \
	(mOp | mX6 | mIh | mWhb)

struct ia64_opcode ia64_opcodes_b[] =
  {
    /* B-type instruction encodings (sorted according to major opcode) */

#define BR(a,b) \
      B0, OpX6BtypePaWhaDPr (0, 0x20, 0, a, 0, b, 0), {B2}, PSEUDO
    {"br.few",		BR (0, 0)},
    {"br",		BR (0, 0)},
    {"br.few.clr",	BR (0, 1)},
    {"br.clr",		BR (0, 1)},
    {"br.many",		BR (1, 0)},
    {"br.many.clr",	BR (1, 1)},
#undef BR

#define BR(a,b,c,d,e)	B0, OpX6BtypePaWhaD (0, a, b, c, d, e), {B2}
    {"br.cond.sptk.few",	BR (0x20, 0, 0, 0, 0)},
    {"br.cond.sptk",		BR (0x20, 0, 0, 0, 0), PSEUDO},
    {"br.cond.sptk.few.clr",	BR (0x20, 0, 0, 0, 1)},
    {"br.cond.sptk.clr",	BR (0x20, 0, 0, 0, 1), PSEUDO},
    {"br.cond.spnt.few",	BR (0x20, 0, 0, 1, 0)},
    {"br.cond.spnt",		BR (0x20, 0, 0, 1, 0), PSEUDO},
    {"br.cond.spnt.few.clr",	BR (0x20, 0, 0, 1, 1)},
    {"br.cond.spnt.clr",	BR (0x20, 0, 0, 1, 1), PSEUDO},
    {"br.cond.dptk.few",	BR (0x20, 0, 0, 2, 0)},
    {"br.cond.dptk",		BR (0x20, 0, 0, 2, 0), PSEUDO},
    {"br.cond.dptk.few.clr",	BR (0x20, 0, 0, 2, 1)},
    {"br.cond.dptk.clr",	BR (0x20, 0, 0, 2, 1), PSEUDO},
    {"br.cond.dpnt.few",	BR (0x20, 0, 0, 3, 0)},
    {"br.cond.dpnt",		BR (0x20, 0, 0, 3, 0), PSEUDO},
    {"br.cond.dpnt.few.clr",	BR (0x20, 0, 0, 3, 1)},
    {"br.cond.dpnt.clr",	BR (0x20, 0, 0, 3, 1), PSEUDO},
    {"br.cond.sptk.many",	BR (0x20, 0, 1, 0, 0)},
    {"br.cond.sptk.many.clr",	BR (0x20, 0, 1, 0, 1)},
    {"br.cond.spnt.many",	BR (0x20, 0, 1, 1, 0)},
    {"br.cond.spnt.many.clr",	BR (0x20, 0, 1, 1, 1)},
    {"br.cond.dptk.many",	BR (0x20, 0, 1, 2, 0)},
    {"br.cond.dptk.many.clr",	BR (0x20, 0, 1, 2, 1)},
    {"br.cond.dpnt.many",	BR (0x20, 0, 1, 3, 0)},
    {"br.cond.dpnt.many.clr",	BR (0x20, 0, 1, 3, 1)},
    {"br.sptk.few",		BR (0x20, 0, 0, 0, 0)},
    {"br.sptk",			BR (0x20, 0, 0, 0, 0), PSEUDO},
    {"br.sptk.few.clr",		BR (0x20, 0, 0, 0, 1)},
    {"br.sptk.clr",		BR (0x20, 0, 0, 0, 1), PSEUDO},
    {"br.spnt.few",		BR (0x20, 0, 0, 1, 0)},
    {"br.spnt",			BR (0x20, 0, 0, 1, 0), PSEUDO},
    {"br.spnt.few.clr",		BR (0x20, 0, 0, 1, 1)},
    {"br.spnt.clr",		BR (0x20, 0, 0, 1, 1), PSEUDO},
    {"br.dptk.few",		BR (0x20, 0, 0, 2, 0)},
    {"br.dptk",			BR (0x20, 0, 0, 2, 0), PSEUDO},
    {"br.dptk.few.clr",		BR (0x20, 0, 0, 2, 1)},
    {"br.dptk.clr",		BR (0x20, 0, 0, 2, 1), PSEUDO},
    {"br.dpnt.few",		BR (0x20, 0, 0, 3, 0)},
    {"br.dpnt",			BR (0x20, 0, 0, 3, 0), PSEUDO},
    {"br.dpnt.few.clr",		BR (0x20, 0, 0, 3, 1)},
    {"br.dpnt.clr",		BR (0x20, 0, 0, 3, 1), PSEUDO},
    {"br.sptk.many",		BR (0x20, 0, 1, 0, 0)},
    {"br.sptk.many.clr",	BR (0x20, 0, 1, 0, 1)},
    {"br.spnt.many",		BR (0x20, 0, 1, 1, 0)},
    {"br.spnt.many.clr",	BR (0x20, 0, 1, 1, 1)},
    {"br.dptk.many",		BR (0x20, 0, 1, 2, 0)},
    {"br.dptk.many.clr",	BR (0x20, 0, 1, 2, 1)},
    {"br.dpnt.many",		BR (0x20, 0, 1, 3, 0)},
    {"br.dpnt.many.clr",	BR (0x20, 0, 1, 3, 1)},
    {"br.ia.sptk.few",		BR (0x20, 1, 0, 0, 0)},
    {"br.ia.sptk",		BR (0x20, 1, 0, 0, 0), PSEUDO},
    {"br.ia.sptk.few.clr",	BR (0x20, 1, 0, 0, 1)},
    {"br.ia.sptk.clr",		BR (0x20, 1, 0, 0, 1), PSEUDO},
    {"br.ia.spnt.few",		BR (0x20, 1, 0, 1, 0)},
    {"br.ia.spnt",		BR (0x20, 1, 0, 1, 0), PSEUDO},
    {"br.ia.spnt.few.clr",	BR (0x20, 1, 0, 1, 1)},
    {"br.ia.spnt.clr",		BR (0x20, 1, 0, 1, 1), PSEUDO},
    {"br.ia.dptk.few",		BR (0x20, 1, 0, 2, 0)},
    {"br.ia.dptk",		BR (0x20, 1, 0, 2, 0), PSEUDO},
    {"br.ia.dptk.few.clr",	BR (0x20, 1, 0, 2, 1)},
    {"br.ia.dptk.clr",		BR (0x20, 1, 0, 2, 1), PSEUDO},
    {"br.ia.dpnt.few",		BR (0x20, 1, 0, 3, 0)},
    {"br.ia.dpnt",		BR (0x20, 1, 0, 3, 0), PSEUDO},
    {"br.ia.dpnt.few.clr",	BR (0x20, 1, 0, 3, 1)},
    {"br.ia.dpnt.clr",		BR (0x20, 1, 0, 3, 1), PSEUDO},
    {"br.ia.sptk.many",		BR (0x20, 1, 1, 0, 0)},
    {"br.ia.sptk.many.clr",	BR (0x20, 1, 1, 0, 1)},
    {"br.ia.spnt.many",		BR (0x20, 1, 1, 1, 0)},
    {"br.ia.spnt.many.clr",	BR (0x20, 1, 1, 1, 1)},
    {"br.ia.dptk.many",		BR (0x20, 1, 1, 2, 0)},
    {"br.ia.dptk.many.clr",	BR (0x20, 1, 1, 2, 1)},
    {"br.ia.dpnt.many",		BR (0x20, 1, 1, 3, 0)},
    {"br.ia.dpnt.many.clr",	BR (0x20, 1, 1, 3, 1)},
    {"br.ret.sptk.few",		BR (0x21, 4, 0, 0, 0), MOD_RRBS},
    {"br.ret.sptk",		BR (0x21, 4, 0, 0, 0), PSEUDO | MOD_RRBS},
    {"br.ret.sptk.few.clr",	BR (0x21, 4, 0, 0, 1), MOD_RRBS},
    {"br.ret.sptk.clr",		BR (0x21, 4, 0, 0, 1), PSEUDO | MOD_RRBS},
    {"br.ret.spnt.few",		BR (0x21, 4, 0, 1, 0), MOD_RRBS},
    {"br.ret.spnt",		BR (0x21, 4, 0, 1, 0), PSEUDO | MOD_RRBS},
    {"br.ret.spnt.few.clr",	BR (0x21, 4, 0, 1, 1), MOD_RRBS},
    {"br.ret.spnt.clr",		BR (0x21, 4, 0, 1, 1), PSEUDO | MOD_RRBS},
    {"br.ret.dptk.few",		BR (0x21, 4, 0, 2, 0), MOD_RRBS},
    {"br.ret.dptk",		BR (0x21, 4, 0, 2, 0), PSEUDO | MOD_RRBS},
    {"br.ret.dptk.few.clr",	BR (0x21, 4, 0, 2, 1), MOD_RRBS},
    {"br.ret.dptk.clr",		BR (0x21, 4, 0, 2, 1), PSEUDO | MOD_RRBS},
    {"br.ret.dpnt.few",		BR (0x21, 4, 0, 3, 0), MOD_RRBS},
    {"br.ret.dpnt",		BR (0x21, 4, 0, 3, 0), PSEUDO | MOD_RRBS},
    {"br.ret.dpnt.few.clr",	BR (0x21, 4, 0, 3, 1), MOD_RRBS},
    {"br.ret.dpnt.clr",		BR (0x21, 4, 0, 3, 1), PSEUDO | MOD_RRBS},
    {"br.ret.sptk.many",	BR (0x21, 4, 1, 0, 0), MOD_RRBS},
    {"br.ret.sptk.many.clr",	BR (0x21, 4, 1, 0, 1), MOD_RRBS},
    {"br.ret.spnt.many",	BR (0x21, 4, 1, 1, 0), MOD_RRBS},
    {"br.ret.spnt.many.clr",	BR (0x21, 4, 1, 1, 1), MOD_RRBS},
    {"br.ret.dptk.many",	BR (0x21, 4, 1, 2, 0), MOD_RRBS},
    {"br.ret.dptk.many.clr",	BR (0x21, 4, 1, 2, 1), MOD_RRBS},
    {"br.ret.dpnt.many",	BR (0x21, 4, 1, 3, 0), MOD_RRBS},
    {"br.ret.dpnt.many.clr",	BR (0x21, 4, 1, 3, 1), MOD_RRBS},
#undef BR

    {"cover",		B0, OpX6 (0, 0x02), {0, }, NO_PRED | LAST | MOD_RRBS},
    {"clrrrb",		B0, OpX6 (0, 0x04), {0, }, NO_PRED | LAST | MOD_RRBS},
    {"clrrrb.pr",	B0, OpX6 (0, 0x05), {0, }, NO_PRED | LAST | MOD_RRBS},
    {"rfi",		B0, OpX6 (0, 0x08), {0, }, NO_PRED | LAST | PRIV | MOD_RRBS},
    {"bsw.0",		B0, OpX6 (0, 0x0c), {0, }, NO_PRED | LAST | PRIV},
    {"bsw.1",		B0, OpX6 (0, 0x0d), {0, }, NO_PRED | LAST | PRIV},
    {"epc",		B0, OpX6 (0, 0x10), {0, }, NO_PRED},

    {"break.b",		B0, OpX6 (0, 0x00), {IMMU21}},

    {"br.call.sptk.few",	B, OpPaWhaD (1, 0, 0, 0), {B1, B2}},
    {"br.call.sptk",		B, OpPaWhaD (1, 0, 0, 0), {B1, B2}, PSEUDO},
    {"br.call.sptk.few.clr",	B, OpPaWhaD (1, 0, 0, 1), {B1, B2}},
    {"br.call.sptk.clr",	B, OpPaWhaD (1, 0, 0, 1), {B1, B2}, PSEUDO},
    {"br.call.spnt.few",	B, OpPaWhaD (1, 0, 1, 0), {B1, B2}},
    {"br.call.spnt",		B, OpPaWhaD (1, 0, 1, 0), {B1, B2}, PSEUDO},
    {"br.call.spnt.few.clr",	B, OpPaWhaD (1, 0, 1, 1), {B1, B2}},
    {"br.call.spnt.clr",	B, OpPaWhaD (1, 0, 1, 1), {B1, B2}, PSEUDO},
    {"br.call.dptk.few",	B, OpPaWhaD (1, 0, 2, 0), {B1, B2}},
    {"br.call.dptk",		B, OpPaWhaD (1, 0, 2, 0), {B1, B2}, PSEUDO},
    {"br.call.dptk.few.clr",	B, OpPaWhaD (1, 0, 2, 1), {B1, B2}},
    {"br.call.dptk.clr",	B, OpPaWhaD (1, 0, 2, 1), {B1, B2}, PSEUDO},
    {"br.call.dpnt.few",	B, OpPaWhaD (1, 0, 3, 0), {B1, B2}},
    {"br.call.dpnt",		B, OpPaWhaD (1, 0, 3, 0), {B1, B2}, PSEUDO},
    {"br.call.dpnt.few.clr",	B, OpPaWhaD (1, 0, 3, 1), {B1, B2}},
    {"br.call.dpnt.clr",	B, OpPaWhaD (1, 0, 3, 1), {B1, B2}, PSEUDO},
    {"br.call.sptk.many",	B, OpPaWhaD (1, 1, 0, 0), {B1, B2}},
    {"br.call.sptk.many.clr",	B, OpPaWhaD (1, 1, 0, 1), {B1, B2}},
    {"br.call.spnt.many",	B, OpPaWhaD (1, 1, 1, 0), {B1, B2}},
    {"br.call.spnt.many.clr",	B, OpPaWhaD (1, 1, 1, 1), {B1, B2}},
    {"br.call.dptk.many",	B, OpPaWhaD (1, 1, 2, 0), {B1, B2}},
    {"br.call.dptk.many.clr",	B, OpPaWhaD (1, 1, 2, 1), {B1, B2}},
    {"br.call.dpnt.many",	B, OpPaWhaD (1, 1, 3, 0), {B1, B2}},
    {"br.call.dpnt.many.clr",	B, OpPaWhaD (1, 1, 3, 1), {B1, B2}},

#define BRP(a,b,c) \
      B0, OpX6IhWhb (2, a, b, c), {B2, TAG13}, NO_PRED
    {"brp.sptk",		BRP (0x10, 0, 0)},
    {"brp.dptk",		BRP (0x10, 0, 2)},
    {"brp.sptk.imp",		BRP (0x10, 1, 0)},
    {"brp.dptk.imp",		BRP (0x10, 1, 2)},
    {"brp.ret.sptk",		BRP (0x11, 0, 0)},
    {"brp.ret.dptk",		BRP (0x11, 0, 2)},
    {"brp.ret.sptk.imp",	BRP (0x11, 1, 0)},
    {"brp.ret.dptk.imp",	BRP (0x11, 1, 2)},
#undef BRP

    {"nop.b",		B0, OpX6 (2, 0x00), {IMMU21}},

#define BR(a,b) \
      B0, OpBtypePaWhaDPr (4, 0, a, 0, b, 0), {TGT25c}, PSEUDO
    {"br.few",		BR (0, 0)},
    {"br",		BR (0, 0)},
    {"br.few.clr",	BR (0, 1)},
    {"br.clr",		BR (0, 1)},
    {"br.many",		BR (1, 0)},
    {"br.many.clr",	BR (1, 1)},
#undef BR

#define BR(a,b,c) \
      B0, OpBtypePaWhaD (4, 0, a, b, c), {TGT25c}
    {"br.cond.sptk.few",	BR (0, 0, 0)},
    {"br.cond.sptk",		BR (0, 0, 0), PSEUDO},
    {"br.cond.sptk.few.clr",	BR (0, 0, 1)},
    {"br.cond.sptk.clr",	BR (0, 0, 1), PSEUDO},
    {"br.cond.spnt.few",	BR (0, 1, 0)},
    {"br.cond.spnt",		BR (0, 1, 0), PSEUDO},
    {"br.cond.spnt.few.clr",	BR (0, 1, 1)},
    {"br.cond.spnt.clr",	BR (0, 1, 1), PSEUDO},
    {"br.cond.dptk.few",	BR (0, 2, 0)},
    {"br.cond.dptk",		BR (0, 2, 0), PSEUDO},
    {"br.cond.dptk.few.clr",	BR (0, 2, 1)},
    {"br.cond.dptk.clr",	BR (0, 2, 1), PSEUDO},
    {"br.cond.dpnt.few",	BR (0, 3, 0)},
    {"br.cond.dpnt",		BR (0, 3, 0), PSEUDO},
    {"br.cond.dpnt.few.clr",	BR (0, 3, 1)},
    {"br.cond.dpnt.clr",	BR (0, 3, 1), PSEUDO},
    {"br.cond.sptk.many",	BR (1, 0, 0)},
    {"br.cond.sptk.many.clr",	BR (1, 0, 1)},
    {"br.cond.spnt.many",	BR (1, 1, 0)},
    {"br.cond.spnt.many.clr",	BR (1, 1, 1)},
    {"br.cond.dptk.many",	BR (1, 2, 0)},
    {"br.cond.dptk.many.clr",	BR (1, 2, 1)},
    {"br.cond.dpnt.many",	BR (1, 3, 0)},
    {"br.cond.dpnt.many.clr",	BR (1, 3, 1)},
    {"br.sptk.few",		BR (0, 0, 0)},
    {"br.sptk",			BR (0, 0, 0), PSEUDO},
    {"br.sptk.few.clr",		BR (0, 0, 1)},
    {"br.sptk.clr",		BR (0, 0, 1), PSEUDO},
    {"br.spnt.few",		BR (0, 1, 0)},
    {"br.spnt",			BR (0, 1, 0), PSEUDO},
    {"br.spnt.few.clr",		BR (0, 1, 1)},
    {"br.spnt.clr",		BR (0, 1, 1), PSEUDO},
    {"br.dptk.few",		BR (0, 2, 0)},
    {"br.dptk",			BR (0, 2, 0), PSEUDO},
    {"br.dptk.few.clr",		BR (0, 2, 1)},
    {"br.dptk.clr",		BR (0, 2, 1), PSEUDO},
    {"br.dpnt.few",		BR (0, 3, 0)},
    {"br.dpnt",			BR (0, 3, 0), PSEUDO},
    {"br.dpnt.few.clr",		BR (0, 3, 1)},
    {"br.dpnt.clr",		BR (0, 3, 1), PSEUDO},
    {"br.sptk.many",		BR (1, 0, 0)},
    {"br.sptk.many.clr",	BR (1, 0, 1)},
    {"br.spnt.many",		BR (1, 1, 0)},
    {"br.spnt.many.clr",	BR (1, 1, 1)},
    {"br.dptk.many",		BR (1, 2, 0)},
    {"br.dptk.many.clr",	BR (1, 2, 1)},
    {"br.dpnt.many",		BR (1, 3, 0)},
    {"br.dpnt.many.clr",	BR (1, 3, 1)},
#undef BR

#define BR(a,b,c,d) \
	B0, OpBtypePaWhaD (4, a, b, c, d), {TGT25c}, SLOT2
    {"br.wexit.sptk.few",	BR (2, 0, 0, 0) | MOD_RRBS},
    {"br.wexit.sptk",		BR (2, 0, 0, 0) | PSEUDO | MOD_RRBS},
    {"br.wexit.sptk.few.clr",	BR (2, 0, 0, 1) | MOD_RRBS},
    {"br.wexit.sptk.clr",	BR (2, 0, 0, 1) | PSEUDO | MOD_RRBS},
    {"br.wexit.spnt.few",	BR (2, 0, 1, 0) | MOD_RRBS},
    {"br.wexit.spnt",		BR (2, 0, 1, 0) | PSEUDO | MOD_RRBS},
    {"br.wexit.spnt.few.clr",	BR (2, 0, 1, 1) | MOD_RRBS},
    {"br.wexit.spnt.clr",	BR (2, 0, 1, 1) | PSEUDO | MOD_RRBS},
    {"br.wexit.dptk.few",	BR (2, 0, 2, 0) | MOD_RRBS},
    {"br.wexit.dptk",		BR (2, 0, 2, 0) | PSEUDO | MOD_RRBS},
    {"br.wexit.dptk.few.clr",	BR (2, 0, 2, 1) | MOD_RRBS},
    {"br.wexit.dptk.clr",	BR (2, 0, 2, 1) | PSEUDO | MOD_RRBS},
    {"br.wexit.dpnt.few",	BR (2, 0, 3, 0) | MOD_RRBS},
    {"br.wexit.dpnt",		BR (2, 0, 3, 0) | PSEUDO | MOD_RRBS},
    {"br.wexit.dpnt.few.clr",	BR (2, 0, 3, 1) | MOD_RRBS},
    {"br.wexit.dpnt.clr",	BR (2, 0, 3, 1) | PSEUDO | MOD_RRBS},
    {"br.wexit.sptk.many",	BR (2, 1, 0, 0) | MOD_RRBS},
    {"br.wexit.sptk.many.clr",	BR (2, 1, 0, 1) | MOD_RRBS},
    {"br.wexit.spnt.many",	BR (2, 1, 1, 0) | MOD_RRBS},
    {"br.wexit.spnt.many.clr",	BR (2, 1, 1, 1) | MOD_RRBS},
    {"br.wexit.dptk.many",	BR (2, 1, 2, 0) | MOD_RRBS},
    {"br.wexit.dptk.many.clr",	BR (2, 1, 2, 1) | MOD_RRBS},
    {"br.wexit.dpnt.many",	BR (2, 1, 3, 0) | MOD_RRBS},
    {"br.wexit.dpnt.many.clr",	BR (2, 1, 3, 1) | MOD_RRBS},
    {"br.wtop.sptk.few",	BR (3, 0, 0, 0) | MOD_RRBS},
    {"br.wtop.sptk",		BR (3, 0, 0, 0) | PSEUDO | MOD_RRBS},
    {"br.wtop.sptk.few.clr",	BR (3, 0, 0, 1) | MOD_RRBS},
    {"br.wtop.sptk.clr",	BR (3, 0, 0, 1) | PSEUDO | MOD_RRBS},
    {"br.wtop.spnt.few",	BR (3, 0, 1, 0) | MOD_RRBS},
    {"br.wtop.spnt",		BR (3, 0, 1, 0) | PSEUDO | MOD_RRBS},
    {"br.wtop.spnt.few.clr",	BR (3, 0, 1, 1) | MOD_RRBS},
    {"br.wtop.spnt.clr",	BR (3, 0, 1, 1) | PSEUDO | MOD_RRBS},
    {"br.wtop.dptk.few",	BR (3, 0, 2, 0) | MOD_RRBS},
    {"br.wtop.dptk",		BR (3, 0, 2, 0) | PSEUDO | MOD_RRBS},
    {"br.wtop.dptk.few.clr",	BR (3, 0, 2, 1) | MOD_RRBS},
    {"br.wtop.dptk.clr",	BR (3, 0, 2, 1) | PSEUDO | MOD_RRBS},
    {"br.wtop.dpnt.few",	BR (3, 0, 3, 0) | MOD_RRBS},
    {"br.wtop.dpnt",		BR (3, 0, 3, 0) | PSEUDO | MOD_RRBS},
    {"br.wtop.dpnt.few.clr",	BR (3, 0, 3, 1) | MOD_RRBS},
    {"br.wtop.dpnt.clr",	BR (3, 0, 3, 1) | PSEUDO | MOD_RRBS},
    {"br.wtop.sptk.many",	BR (3, 1, 0, 0) | MOD_RRBS},
    {"br.wtop.sptk.many.clr",	BR (3, 1, 0, 1) | MOD_RRBS},
    {"br.wtop.spnt.many",	BR (3, 1, 1, 0) | MOD_RRBS},
    {"br.wtop.spnt.many.clr",	BR (3, 1, 1, 1) | MOD_RRBS},
    {"br.wtop.dptk.many",	BR (3, 1, 2, 0) | MOD_RRBS},
    {"br.wtop.dptk.many.clr",	BR (3, 1, 2, 1) | MOD_RRBS},
    {"br.wtop.dpnt.many",	BR (3, 1, 3, 0) | MOD_RRBS},
    {"br.wtop.dpnt.many.clr",	BR (3, 1, 3, 1) | MOD_RRBS},

#undef BR
#define BR(a,b,c,d) \
	B0, OpBtypePaWhaD (4, a, b, c, d), {TGT25c}, SLOT2 | NO_PRED
    {"br.cloop.sptk.few",	BR (5, 0, 0, 0)},
    {"br.cloop.sptk",		BR (5, 0, 0, 0) | PSEUDO},
    {"br.cloop.sptk.few.clr",	BR (5, 0, 0, 1)},
    {"br.cloop.sptk.clr",	BR (5, 0, 0, 1) | PSEUDO},
    {"br.cloop.spnt.few",	BR (5, 0, 1, 0)},
    {"br.cloop.spnt",		BR (5, 0, 1, 0) | PSEUDO},
    {"br.cloop.spnt.few.clr",	BR (5, 0, 1, 1)},
    {"br.cloop.spnt.clr",	BR (5, 0, 1, 1) | PSEUDO},
    {"br.cloop.dptk.few",	BR (5, 0, 2, 0)},
    {"br.cloop.dptk",		BR (5, 0, 2, 0) | PSEUDO},
    {"br.cloop.dptk.few.clr",	BR (5, 0, 2, 1)},
    {"br.cloop.dptk.clr",	BR (5, 0, 2, 1) | PSEUDO},
    {"br.cloop.dpnt.few",	BR (5, 0, 3, 0)},
    {"br.cloop.dpnt",		BR (5, 0, 3, 0) | PSEUDO},
    {"br.cloop.dpnt.few.clr",	BR (5, 0, 3, 1)},
    {"br.cloop.dpnt.clr",	BR (5, 0, 3, 1) | PSEUDO},
    {"br.cloop.sptk.many",	BR (5, 1, 0, 0)},
    {"br.cloop.sptk.many.clr",	BR (5, 1, 0, 1)},
    {"br.cloop.spnt.many",	BR (5, 1, 1, 0)},
    {"br.cloop.spnt.many.clr",	BR (5, 1, 1, 1)},
    {"br.cloop.dptk.many",	BR (5, 1, 2, 0)},
    {"br.cloop.dptk.many.clr",	BR (5, 1, 2, 1)},
    {"br.cloop.dpnt.many",	BR (5, 1, 3, 0)},
    {"br.cloop.dpnt.many.clr",	BR (5, 1, 3, 1)},
    {"br.cexit.sptk.few",	BR (6, 0, 0, 0) | MOD_RRBS},
    {"br.cexit.sptk",		BR (6, 0, 0, 0) | PSEUDO | MOD_RRBS},
    {"br.cexit.sptk.few.clr",	BR (6, 0, 0, 1) | MOD_RRBS},
    {"br.cexit.sptk.clr",	BR (6, 0, 0, 1) | PSEUDO | MOD_RRBS},
    {"br.cexit.spnt.few",	BR (6, 0, 1, 0) | MOD_RRBS},
    {"br.cexit.spnt",		BR (6, 0, 1, 0) | PSEUDO | MOD_RRBS},
    {"br.cexit.spnt.few.clr",	BR (6, 0, 1, 1) | MOD_RRBS},
    {"br.cexit.spnt.clr",	BR (6, 0, 1, 1) | PSEUDO | MOD_RRBS},
    {"br.cexit.dptk.few",	BR (6, 0, 2, 0) | MOD_RRBS},
    {"br.cexit.dptk",		BR (6, 0, 2, 0) | PSEUDO | MOD_RRBS},
    {"br.cexit.dptk.few.clr",	BR (6, 0, 2, 1) | MOD_RRBS},
    {"br.cexit.dptk.clr",	BR (6, 0, 2, 1) | PSEUDO | MOD_RRBS},
    {"br.cexit.dpnt.few",	BR (6, 0, 3, 0) | MOD_RRBS},
    {"br.cexit.dpnt",		BR (6, 0, 3, 0) | PSEUDO | MOD_RRBS},
    {"br.cexit.dpnt.few.clr",	BR (6, 0, 3, 1) | MOD_RRBS},
    {"br.cexit.dpnt.clr",	BR (6, 0, 3, 1) | PSEUDO | MOD_RRBS},
    {"br.cexit.sptk.many",	BR (6, 1, 0, 0) | MOD_RRBS},
    {"br.cexit.sptk.many.clr",	BR (6, 1, 0, 1) | MOD_RRBS},
    {"br.cexit.spnt.many",	BR (6, 1, 1, 0) | MOD_RRBS},
    {"br.cexit.spnt.many.clr",	BR (6, 1, 1, 1) | MOD_RRBS},
    {"br.cexit.dptk.many",	BR (6, 1, 2, 0) | MOD_RRBS},
    {"br.cexit.dptk.many.clr",	BR (6, 1, 2, 1) | MOD_RRBS},
    {"br.cexit.dpnt.many",	BR (6, 1, 3, 0) | MOD_RRBS},
    {"br.cexit.dpnt.many.clr",	BR (6, 1, 3, 1) | MOD_RRBS},
    {"br.ctop.sptk.few",	BR (7, 0, 0, 0) | MOD_RRBS},
    {"br.ctop.sptk",		BR (7, 0, 0, 0) | PSEUDO | MOD_RRBS},
    {"br.ctop.sptk.few.clr",	BR (7, 0, 0, 1) | MOD_RRBS},
    {"br.ctop.sptk.clr",	BR (7, 0, 0, 1) | PSEUDO | MOD_RRBS},
    {"br.ctop.spnt.few",	BR (7, 0, 1, 0) | MOD_RRBS},
    {"br.ctop.spnt",		BR (7, 0, 1, 0) | PSEUDO | MOD_RRBS},
    {"br.ctop.spnt.few.clr",	BR (7, 0, 1, 1) | MOD_RRBS},
    {"br.ctop.spnt.clr",	BR (7, 0, 1, 1) | PSEUDO | MOD_RRBS},
    {"br.ctop.dptk.few",	BR (7, 0, 2, 0) | MOD_RRBS},
    {"br.ctop.dptk",		BR (7, 0, 2, 0) | PSEUDO | MOD_RRBS},
    {"br.ctop.dptk.few.clr",	BR (7, 0, 2, 1) | MOD_RRBS},
    {"br.ctop.dptk.clr",	BR (7, 0, 2, 1) | PSEUDO | MOD_RRBS},
    {"br.ctop.dpnt.few",	BR (7, 0, 3, 0) | MOD_RRBS},
    {"br.ctop.dpnt",		BR (7, 0, 3, 0) | PSEUDO | MOD_RRBS},
    {"br.ctop.dpnt.few.clr",	BR (7, 0, 3, 1) | MOD_RRBS},
    {"br.ctop.dpnt.clr",	BR (7, 0, 3, 1) | PSEUDO | MOD_RRBS},
    {"br.ctop.sptk.many",	BR (7, 1, 0, 0) | MOD_RRBS},
    {"br.ctop.sptk.many.clr",	BR (7, 1, 0, 1) | MOD_RRBS},
    {"br.ctop.spnt.many",	BR (7, 1, 1, 0) | MOD_RRBS},
    {"br.ctop.spnt.many.clr",	BR (7, 1, 1, 1) | MOD_RRBS},
    {"br.ctop.dptk.many",	BR (7, 1, 2, 0) | MOD_RRBS},
    {"br.ctop.dptk.many.clr",	BR (7, 1, 2, 1) | MOD_RRBS},
    {"br.ctop.dpnt.many",	BR (7, 1, 3, 0) | MOD_RRBS},
    {"br.ctop.dpnt.many.clr",	BR (7, 1, 3, 1) | MOD_RRBS},

#undef BR
#define BR(a,b,c,d) \
	B0, OpBtypePaWhaD (4, a, b, c, d), {TGT25c}, SLOT2
    {"br.call.sptk.few",	B, OpPaWhaD (5, 0, 0, 0), {B1, TGT25c}},
    {"br.call.sptk",		B, OpPaWhaD (5, 0, 0, 0), {B1, TGT25c}, PSEUDO},
    {"br.call.sptk.few.clr",	B, OpPaWhaD (5, 0, 0, 1), {B1, TGT25c}},
    {"br.call.sptk.clr",	B, OpPaWhaD (5, 0, 0, 1), {B1, TGT25c}, PSEUDO},
    {"br.call.spnt.few",	B, OpPaWhaD (5, 0, 1, 0), {B1, TGT25c}},
    {"br.call.spnt",		B, OpPaWhaD (5, 0, 1, 0), {B1, TGT25c}, PSEUDO},
    {"br.call.spnt.few.clr",	B, OpPaWhaD (5, 0, 1, 1), {B1, TGT25c}},
    {"br.call.spnt.clr",	B, OpPaWhaD (5, 0, 1, 1), {B1, TGT25c}, PSEUDO},
    {"br.call.dptk.few",	B, OpPaWhaD (5, 0, 2, 0), {B1, TGT25c}},
    {"br.call.dptk",		B, OpPaWhaD (5, 0, 2, 0), {B1, TGT25c}, PSEUDO},
    {"br.call.dptk.few.clr",	B, OpPaWhaD (5, 0, 2, 1), {B1, TGT25c}},
    {"br.call.dptk.clr",	B, OpPaWhaD (5, 0, 2, 1), {B1, TGT25c}, PSEUDO},
    {"br.call.dpnt.few",	B, OpPaWhaD (5, 0, 3, 0), {B1, TGT25c}},
    {"br.call.dpnt",		B, OpPaWhaD (5, 0, 3, 0), {B1, TGT25c}, PSEUDO},
    {"br.call.dpnt.few.clr",	B, OpPaWhaD (5, 0, 3, 1), {B1, TGT25c}},
    {"br.call.dpnt.clr",	B, OpPaWhaD (5, 0, 3, 1), {B1, TGT25c}, PSEUDO},
    {"br.call.sptk.many",	B, OpPaWhaD (5, 1, 0, 0), {B1, TGT25c}},
    {"br.call.sptk.many.clr",	B, OpPaWhaD (5, 1, 0, 1), {B1, TGT25c}},
    {"br.call.spnt.many",	B, OpPaWhaD (5, 1, 1, 0), {B1, TGT25c}},
    {"br.call.spnt.many.clr",	B, OpPaWhaD (5, 1, 1, 1), {B1, TGT25c}},
    {"br.call.dptk.many",	B, OpPaWhaD (5, 1, 2, 0), {B1, TGT25c}},
    {"br.call.dptk.many.clr",	B, OpPaWhaD (5, 1, 2, 1), {B1, TGT25c}},
    {"br.call.dpnt.many",	B, OpPaWhaD (5, 1, 3, 0), {B1, TGT25c}},
    {"br.call.dpnt.many.clr",	B, OpPaWhaD (5, 1, 3, 1), {B1, TGT25c}},
#undef BR

    /* branch predict */
#define BRP(a,b) \
      B0, OpIhWhb (7, a, b), {TGT25c, TAG13}, NO_PRED
    {"brp.sptk",		BRP (0, 0)},
    {"brp.loop",		BRP (0, 1)},
    {"brp.dptk",		BRP (0, 2)},
    {"brp.exit",		BRP (0, 3)},
    {"brp.sptk.imp",		BRP (1, 0)},
    {"brp.loop.imp",		BRP (1, 1)},
    {"brp.dptk.imp",		BRP (1, 2)},
    {"brp.exit.imp",		BRP (1, 3)},
#undef BRP

    {0}
  };

#undef B0
#undef B
#undef bBtype
#undef bD
#undef bIh
#undef bPa
#undef bPr
#undef bWha
#undef bWhb
#undef bX6
#undef mBtype
#undef mD
#undef mIh
#undef mPa
#undef mPr
#undef mWha
#undef mWhb
#undef mX6
#undef OpX6
#undef OpPaWhaD
#undef OpBtypePaWhaD
#undef OpBtypePaWhaDPr
#undef OpX6BtypePaWhaD
#undef OpX6BtypePaWhaDPr
#undef OpIhWhb
#undef OpX6IhWhb
