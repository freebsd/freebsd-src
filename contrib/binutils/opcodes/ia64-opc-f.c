/* ia64-opc-f.c -- IA-64 `F' opcode table.
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

#define f0	IA64_TYPE_F, 0
#define f	IA64_TYPE_F, 1
#define f2	IA64_TYPE_F, 2

#define bF2(x)	(((ia64_insn) ((x) & 0x7f)) << 13)
#define bF4(x)	(((ia64_insn) ((x) & 0x7f)) << 27)
#define bQ(x)	(((ia64_insn) ((x) & 0x1)) << 36)
#define bRa(x)	(((ia64_insn) ((x) & 0x1)) << 33)
#define bRb(x)	(((ia64_insn) ((x) & 0x1)) << 36)
#define bSf(x)	(((ia64_insn) ((x) & 0x3)) << 34)
#define bTa(x)	(((ia64_insn) ((x) & 0x1)) << 12)
#define bXa(x)	(((ia64_insn) ((x) & 0x1)) << 36)
#define bXb(x)	(((ia64_insn) ((x) & 0x1)) << 33)
#define bX2(x)	(((ia64_insn) ((x) & 0x3)) << 34)
#define bX6(x)	(((ia64_insn) ((x) & 0x3f)) << 27)

#define mF2	bF2 (-1)
#define mF4	bF4 (-1)
#define mQ	bQ (-1)
#define mRa	bRa (-1)
#define mRb	bRb (-1)
#define mSf	bSf (-1)
#define mTa	bTa (-1)
#define mXa	bXa (-1)
#define mXb	bXb (-1)
#define mX2	bX2 (-1)
#define mX6	bX6 (-1)

#define OpXa(a,b)	(bOp (a) | bXa (b)), (mOp | mXa)
#define OpXaSf(a,b,c)	(bOp (a) | bXa (b) | bSf (c)), (mOp | mXa | mSf)
#define OpXaSfF2(a,b,c,d) \
	(bOp (a) | bXa (b) | bSf (c) | bF2 (d)), (mOp | mXa | mSf | mF2)
#define OpXaSfF4(a,b,c,d) \
	(bOp (a) | bXa (b) | bSf (c) | bF4 (d)), (mOp | mXa | mSf | mF4)
#define OpXaSfF2F4(a,b,c,d,e) \
	(bOp (a) | bXa (b) | bSf (c) | bF2 (d) | bF4 (e)), \
	(mOp | mXa | mSf | mF2 | mF4)
#define OpXaX2(a,b,c)	(bOp (a) | bXa (b) | bX2 (c)), (mOp | mXa | mX2)
#define OpXaX2F2(a,b,c,d) \
	(bOp (a) | bXa (b) | bX2 (c) | bF2 (d)), (mOp | mXa | mX2 | mF2)
#define OpRaRbTaSf(a,b,c,d,e) \
	(bOp (a) | bRa (b) | bRb (c) | bTa (d) | bSf (e)), \
	(mOp | mRa | mRb | mTa | mSf)
#define OpTa(a,b)	(bOp (a) | bTa (b)), (mOp | mTa)
#define OpXbQSf(a,b,c,d) \
	(bOp (a) | bXb (b) | bQ (c) | bSf (d)), (mOp | mXb | mQ | mSf)
#define OpXbX6(a,b,c) \
	(bOp (a) | bXb (b) | bX6 (c)), (mOp | mXb | mX6)
#define OpXbX6F2(a,b,c,d) \
	(bOp (a) | bXb (b) | bX6 (c) | bF2 (d)), (mOp | mXb | mX6 | mF2)
#define OpXbX6Sf(a,b,c,d) \
	(bOp (a) | bXb (b) | bX6 (c) | bSf (d)), (mOp | mXb | mX6 | mSf)

struct ia64_opcode ia64_opcodes_f[] =
  {
    /* F-type instruction encodings (sorted according to major opcode) */

    {"frcpa.s0",	f2, OpXbQSf (0, 1, 0, 0), {F1, P2, F2, F3}},
    {"frcpa",		f2, OpXbQSf (0, 1, 0, 0), {F1, P2, F2, F3}, PSEUDO},
    {"frcpa.s1",	f2, OpXbQSf (0, 1, 0, 1), {F1, P2, F2, F3}},
    {"frcpa.s2",	f2, OpXbQSf (0, 1, 0, 2), {F1, P2, F2, F3}},
    {"frcpa.s3",	f2, OpXbQSf (0, 1, 0, 3), {F1, P2, F2, F3}},

    {"frsqrta.s0",	f2, OpXbQSf (0, 1, 1, 0), {F1, P2, F3}},
    {"frsqrta",		f2, OpXbQSf (0, 1, 1, 0), {F1, P2, F3}, PSEUDO},
    {"frsqrta.s1",	f2, OpXbQSf (0, 1, 1, 1), {F1, P2, F3}},
    {"frsqrta.s2",	f2, OpXbQSf (0, 1, 1, 2), {F1, P2, F3}},
    {"frsqrta.s3",	f2, OpXbQSf (0, 1, 1, 3), {F1, P2, F3}},

    {"fmin.s0",		f, OpXbX6Sf (0, 0, 0x14, 0), {F1, F2, F3}},
    {"fmin",		f, OpXbX6Sf (0, 0, 0x14, 0), {F1, F2, F3}, PSEUDO},
    {"fmin.s1",		f, OpXbX6Sf (0, 0, 0x14, 1), {F1, F2, F3}},
    {"fmin.s2",		f, OpXbX6Sf (0, 0, 0x14, 2), {F1, F2, F3}},
    {"fmin.s3",		f, OpXbX6Sf (0, 0, 0x14, 3), {F1, F2, F3}},
    {"fmax.s0",		f, OpXbX6Sf (0, 0, 0x15, 0), {F1, F2, F3}},
    {"fmax",		f, OpXbX6Sf (0, 0, 0x15, 0), {F1, F2, F3}, PSEUDO},
    {"fmax.s1",		f, OpXbX6Sf (0, 0, 0x15, 1), {F1, F2, F3}},
    {"fmax.s2",		f, OpXbX6Sf (0, 0, 0x15, 2), {F1, F2, F3}},
    {"fmax.s3",		f, OpXbX6Sf (0, 0, 0x15, 3), {F1, F2, F3}},
    {"famin.s0",	f, OpXbX6Sf (0, 0, 0x16, 0), {F1, F2, F3}},
    {"famin",		f, OpXbX6Sf (0, 0, 0x16, 0), {F1, F2, F3}, PSEUDO},
    {"famin.s1",	f, OpXbX6Sf (0, 0, 0x16, 1), {F1, F2, F3}},
    {"famin.s2",	f, OpXbX6Sf (0, 0, 0x16, 2), {F1, F2, F3}},
    {"famin.s3",	f, OpXbX6Sf (0, 0, 0x16, 3), {F1, F2, F3}},
    {"famax.s0",	f, OpXbX6Sf (0, 0, 0x17, 0), {F1, F2, F3}},
    {"famax",		f, OpXbX6Sf (0, 0, 0x17, 0), {F1, F2, F3}, PSEUDO},
    {"famax.s1",	f, OpXbX6Sf (0, 0, 0x17, 1), {F1, F2, F3}},
    {"famax.s2",	f, OpXbX6Sf (0, 0, 0x17, 2), {F1, F2, F3}},
    {"famax.s3",	f, OpXbX6Sf (0, 0, 0x17, 3), {F1, F2, F3}},

    {"mov",		f, OpXbX6 (0, 0, 0x10), {F1, F3}, PSEUDO | F2_EQ_F3},
    {"fabs",		f, OpXbX6F2 (0, 0, 0x10, 0), {F1, F3}, PSEUDO},
    {"fneg",		f, OpXbX6   (0, 0, 0x11), {F1, F3}, PSEUDO | F2_EQ_F3},
    {"fnegabs",		f, OpXbX6F2 (0, 0, 0x11, 0), {F1, F3}, PSEUDO},
    {"fmerge.s",	f, OpXbX6   (0, 0, 0x10), {F1, F2, F3}},
    {"fmerge.ns",	f, OpXbX6   (0, 0, 0x11), {F1, F2, F3}},

    {"fmerge.se",	f, OpXbX6 (0, 0, 0x12), {F1, F2, F3}},
    {"fmix.lr",		f, OpXbX6 (0, 0, 0x39), {F1, F2, F3}},
    {"fmix.r",		f, OpXbX6 (0, 0, 0x3a), {F1, F2, F3}},
    {"fmix.l",		f, OpXbX6 (0, 0, 0x3b), {F1, F2, F3}},
    {"fsxt.r",		f, OpXbX6 (0, 0, 0x3c), {F1, F2, F3}},
    {"fsxt.l",		f, OpXbX6 (0, 0, 0x3d), {F1, F2, F3}},
    {"fpack",		f, OpXbX6 (0, 0, 0x28), {F1, F2, F3}},
    {"fswap",		f, OpXbX6 (0, 0, 0x34), {F1, F2, F3}},
    {"fswap.nl",	f, OpXbX6 (0, 0, 0x35), {F1, F2, F3}},
    {"fswap.nr",	f, OpXbX6 (0, 0, 0x36), {F1, F2, F3}},
    {"fand",		f, OpXbX6 (0, 0, 0x2c), {F1, F2, F3}},
    {"fandcm",		f, OpXbX6 (0, 0, 0x2d), {F1, F2, F3}},
    {"for",		f, OpXbX6 (0, 0, 0x2e), {F1, F2, F3}},
    {"fxor",		f, OpXbX6 (0, 0, 0x2f), {F1, F2, F3}},

    {"fcvt.fx.s0",		f, OpXbX6Sf (0, 0, 0x18, 0), {F1, F2}},
    {"fcvt.fx",			f, OpXbX6Sf (0, 0, 0x18, 0), {F1, F2}, PSEUDO},
    {"fcvt.fx.s1",		f, OpXbX6Sf (0, 0, 0x18, 1), {F1, F2}},
    {"fcvt.fx.s2",		f, OpXbX6Sf (0, 0, 0x18, 2), {F1, F2}},
    {"fcvt.fx.s3",		f, OpXbX6Sf (0, 0, 0x18, 3), {F1, F2}},
    {"fcvt.fxu.s0",		f, OpXbX6Sf (0, 0, 0x19, 0), {F1, F2}},
    {"fcvt.fxu",		f, OpXbX6Sf (0, 0, 0x19, 0), {F1, F2}, PSEUDO},
    {"fcvt.fxu.s1",		f, OpXbX6Sf (0, 0, 0x19, 1), {F1, F2}},
    {"fcvt.fxu.s2",		f, OpXbX6Sf (0, 0, 0x19, 2), {F1, F2}},
    {"fcvt.fxu.s3",		f, OpXbX6Sf (0, 0, 0x19, 3), {F1, F2}},
    {"fcvt.fx.trunc.s0",	f, OpXbX6Sf (0, 0, 0x1a, 0), {F1, F2}},
    {"fcvt.fx.trunc",		f, OpXbX6Sf (0, 0, 0x1a, 0), {F1, F2}, PSEUDO},
    {"fcvt.fx.trunc.s1",	f, OpXbX6Sf (0, 0, 0x1a, 1), {F1, F2}},
    {"fcvt.fx.trunc.s2",	f, OpXbX6Sf (0, 0, 0x1a, 2), {F1, F2}},
    {"fcvt.fx.trunc.s3",	f, OpXbX6Sf (0, 0, 0x1a, 3), {F1, F2}},
    {"fcvt.fxu.trunc.s0",	f, OpXbX6Sf (0, 0, 0x1b, 0), {F1, F2}},
    {"fcvt.fxu.trunc",		f, OpXbX6Sf (0, 0, 0x1b, 0), {F1, F2}, PSEUDO},
    {"fcvt.fxu.trunc.s1",	f, OpXbX6Sf (0, 0, 0x1b, 1), {F1, F2}},
    {"fcvt.fxu.trunc.s2",	f, OpXbX6Sf (0, 0, 0x1b, 2), {F1, F2}},
    {"fcvt.fxu.trunc.s3",	f, OpXbX6Sf (0, 0, 0x1b, 3), {F1, F2}},

    {"fcvt.xf",		f, OpXbX6 (0, 0, 0x1c), {F1, F2}},

    {"fsetc.s0",	f0, OpXbX6Sf (0, 0, 0x04, 0), {IMMU7a, IMMU7b}},
    {"fsetc",		f0, OpXbX6Sf (0, 0, 0x04, 0), {IMMU7a, IMMU7b}, PSEUDO},
    {"fsetc.s1",	f0, OpXbX6Sf (0, 0, 0x04, 1), {IMMU7a, IMMU7b}},
    {"fsetc.s2",	f0, OpXbX6Sf (0, 0, 0x04, 2), {IMMU7a, IMMU7b}},
    {"fsetc.s3",	f0, OpXbX6Sf (0, 0, 0x04, 3), {IMMU7a, IMMU7b}},
    {"fclrf.s0",	f0, OpXbX6Sf (0, 0, 0x05, 0)},
    {"fclrf",		f0, OpXbX6Sf (0, 0, 0x05, 0), {0}, PSEUDO},
    {"fclrf.s1",	f0, OpXbX6Sf (0, 0, 0x05, 1)},
    {"fclrf.s2",	f0, OpXbX6Sf (0, 0, 0x05, 2)},
    {"fclrf.s3",	f0, OpXbX6Sf (0, 0, 0x05, 3)},
    {"fchkf.s0",	f0, OpXbX6Sf (0, 0, 0x08, 0), {TGT25}},
    {"fchkf",		f0, OpXbX6Sf (0, 0, 0x08, 0), {TGT25}, PSEUDO},
    {"fchkf.s1",	f0, OpXbX6Sf (0, 0, 0x08, 1), {TGT25}},
    {"fchkf.s2",	f0, OpXbX6Sf (0, 0, 0x08, 2), {TGT25}},
    {"fchkf.s3",	f0, OpXbX6Sf (0, 0, 0x08, 3), {TGT25}},

    {"break.f",		f0, OpXbX6 (0, 0, 0x00), {IMMU21}},
    {"nop.f",		f0, OpXbX6 (0, 0, 0x01), {IMMU21}},

    {"fprcpa.s0",	f2, OpXbQSf (1, 1, 0, 0), {F1, P2, F2, F3}},
    {"fprcpa",		f2, OpXbQSf (1, 1, 0, 0), {F1, P2, F2, F3}, PSEUDO},
    {"fprcpa.s1",	f2, OpXbQSf (1, 1, 0, 1), {F1, P2, F2, F3}},
    {"fprcpa.s2",	f2, OpXbQSf (1, 1, 0, 2), {F1, P2, F2, F3}},
    {"fprcpa.s3",	f2, OpXbQSf (1, 1, 0, 3), {F1, P2, F2, F3}},

    {"fprsqrta.s0",	f2, OpXbQSf (1, 1, 1, 0), {F1, P2, F3}},
    {"fprsqrta",	f2, OpXbQSf (1, 1, 1, 0), {F1, P2, F3}, PSEUDO},
    {"fprsqrta.s1",	f2, OpXbQSf (1, 1, 1, 1), {F1, P2, F3}},
    {"fprsqrta.s2",	f2, OpXbQSf (1, 1, 1, 2), {F1, P2, F3}},
    {"fprsqrta.s3",	f2, OpXbQSf (1, 1, 1, 3), {F1, P2, F3}},

    {"fpmin.s0",	f, OpXbX6Sf (1, 0, 0x14, 0), {F1, F2, F3}},
    {"fpmin",		f, OpXbX6Sf (1, 0, 0x14, 0), {F1, F2, F3}, PSEUDO},
    {"fpmin.s1",	f, OpXbX6Sf (1, 0, 0x14, 1), {F1, F2, F3}},
    {"fpmin.s2",	f, OpXbX6Sf (1, 0, 0x14, 2), {F1, F2, F3}},
    {"fpmin.s3",	f, OpXbX6Sf (1, 0, 0x14, 3), {F1, F2, F3}},
    {"fpmax.s0",	f, OpXbX6Sf (1, 0, 0x15, 0), {F1, F2, F3}},
    {"fpmax",		f, OpXbX6Sf (1, 0, 0x15, 0), {F1, F2, F3}, PSEUDO},
    {"fpmax.s1",	f, OpXbX6Sf (1, 0, 0x15, 1), {F1, F2, F3}},
    {"fpmax.s2",	f, OpXbX6Sf (1, 0, 0x15, 2), {F1, F2, F3}},
    {"fpmax.s3",	f, OpXbX6Sf (1, 0, 0x15, 3), {F1, F2, F3}},
    {"fpamin.s0",	f, OpXbX6Sf (1, 0, 0x16, 0), {F1, F2, F3}},
    {"fpamin",		f, OpXbX6Sf (1, 0, 0x16, 0), {F1, F2, F3}, PSEUDO},
    {"fpamin.s1",	f, OpXbX6Sf (1, 0, 0x16, 1), {F1, F2, F3}},
    {"fpamin.s2",	f, OpXbX6Sf (1, 0, 0x16, 2), {F1, F2, F3}},
    {"fpamin.s3",	f, OpXbX6Sf (1, 0, 0x16, 3), {F1, F2, F3}},
    {"fpamax.s0",	f, OpXbX6Sf (1, 0, 0x17, 0), {F1, F2, F3}},
    {"fpamax",		f, OpXbX6Sf (1, 0, 0x17, 0), {F1, F2, F3}, PSEUDO},
    {"fpamax.s1",	f, OpXbX6Sf (1, 0, 0x17, 1), {F1, F2, F3}},
    {"fpamax.s2",	f, OpXbX6Sf (1, 0, 0x17, 2), {F1, F2, F3}},
    {"fpamax.s3",	f, OpXbX6Sf (1, 0, 0x17, 3), {F1, F2, F3}},

    {"fpcmp.eq.s0",	f, OpXbX6Sf (1, 0, 0x30, 0), {F1, F2, F3}},
    {"fpcmp.eq",	f, OpXbX6Sf (1, 0, 0x30, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.eq.s1",	f, OpXbX6Sf (1, 0, 0x30, 1), {F1, F2, F3}},
    {"fpcmp.eq.s2",	f, OpXbX6Sf (1, 0, 0x30, 2), {F1, F2, F3}},
    {"fpcmp.eq.s3",	f, OpXbX6Sf (1, 0, 0x30, 3), {F1, F2, F3}},
    {"fpcmp.lt.s0",	f, OpXbX6Sf (1, 0, 0x31, 0), {F1, F2, F3}},
    {"fpcmp.lt",	f, OpXbX6Sf (1, 0, 0x31, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.lt.s1",	f, OpXbX6Sf (1, 0, 0x31, 1), {F1, F2, F3}},
    {"fpcmp.lt.s2",	f, OpXbX6Sf (1, 0, 0x31, 2), {F1, F2, F3}},
    {"fpcmp.lt.s3",	f, OpXbX6Sf (1, 0, 0x31, 3), {F1, F2, F3}},
    {"fpcmp.le.s0",	f, OpXbX6Sf (1, 0, 0x32, 0), {F1, F2, F3}},
    {"fpcmp.le",	f, OpXbX6Sf (1, 0, 0x32, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.le.s1",	f, OpXbX6Sf (1, 0, 0x32, 1), {F1, F2, F3}},
    {"fpcmp.le.s2",	f, OpXbX6Sf (1, 0, 0x32, 2), {F1, F2, F3}},
    {"fpcmp.le.s3",	f, OpXbX6Sf (1, 0, 0x32, 3), {F1, F2, F3}},
    {"fpcmp.gt.s0",	f, OpXbX6Sf (1, 0, 0x31, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.gt",	f, OpXbX6Sf (1, 0, 0x31, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.gt.s1",	f, OpXbX6Sf (1, 0, 0x31, 1), {F1, F3, F2}, PSEUDO},
    {"fpcmp.gt.s2",	f, OpXbX6Sf (1, 0, 0x31, 2), {F1, F3, F2}, PSEUDO},
    {"fpcmp.gt.s3",	f, OpXbX6Sf (1, 0, 0x31, 3), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ge.s0",	f, OpXbX6Sf (1, 0, 0x32, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ge",	f, OpXbX6Sf (1, 0, 0x32, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ge.s1",	f, OpXbX6Sf (1, 0, 0x32, 1), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ge.s2",	f, OpXbX6Sf (1, 0, 0x32, 2), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ge.s3",	f, OpXbX6Sf (1, 0, 0x32, 3), {F1, F3, F2}, PSEUDO},
    {"fpcmp.unord.s0",	f, OpXbX6Sf (1, 0, 0x33, 0), {F1, F2, F3}},
    {"fpcmp.unord",	f, OpXbX6Sf (1, 0, 0x33, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.unord.s1",	f, OpXbX6Sf (1, 0, 0x33, 1), {F1, F2, F3}},
    {"fpcmp.unord.s2",	f, OpXbX6Sf (1, 0, 0x33, 2), {F1, F2, F3}},
    {"fpcmp.unord.s3",	f, OpXbX6Sf (1, 0, 0x33, 3), {F1, F2, F3}},
    {"fpcmp.neq.s0",	f, OpXbX6Sf (1, 0, 0x34, 0), {F1, F2, F3}},
    {"fpcmp.neq",	f, OpXbX6Sf (1, 0, 0x34, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.neq.s1",	f, OpXbX6Sf (1, 0, 0x34, 1), {F1, F2, F3}},
    {"fpcmp.neq.s2",	f, OpXbX6Sf (1, 0, 0x34, 2), {F1, F2, F3}},
    {"fpcmp.neq.s3",	f, OpXbX6Sf (1, 0, 0x34, 3), {F1, F2, F3}},
    {"fpcmp.nlt.s0",	f, OpXbX6Sf (1, 0, 0x35, 0), {F1, F2, F3}},
    {"fpcmp.nlt",	f, OpXbX6Sf (1, 0, 0x35, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.nlt.s1",	f, OpXbX6Sf (1, 0, 0x35, 1), {F1, F2, F3}},
    {"fpcmp.nlt.s2",	f, OpXbX6Sf (1, 0, 0x35, 2), {F1, F2, F3}},
    {"fpcmp.nlt.s3",	f, OpXbX6Sf (1, 0, 0x35, 3), {F1, F2, F3}},
    {"fpcmp.nle.s0",	f, OpXbX6Sf (1, 0, 0x36, 0), {F1, F2, F3}},
    {"fpcmp.nle",	f, OpXbX6Sf (1, 0, 0x36, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.nle.s1",	f, OpXbX6Sf (1, 0, 0x36, 1), {F1, F2, F3}},
    {"fpcmp.nle.s2",	f, OpXbX6Sf (1, 0, 0x36, 2), {F1, F2, F3}},
    {"fpcmp.nle.s3",	f, OpXbX6Sf (1, 0, 0x36, 3), {F1, F2, F3}},
    {"fpcmp.ngt.s0",	f, OpXbX6Sf (1, 0, 0x35, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ngt",	f, OpXbX6Sf (1, 0, 0x35, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ngt.s1",	f, OpXbX6Sf (1, 0, 0x35, 1), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ngt.s2",	f, OpXbX6Sf (1, 0, 0x35, 2), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ngt.s3",	f, OpXbX6Sf (1, 0, 0x35, 3), {F1, F3, F2}, PSEUDO},
    {"fpcmp.nge.s0",	f, OpXbX6Sf (1, 0, 0x36, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.nge",	f, OpXbX6Sf (1, 0, 0x36, 0), {F1, F3, F2}, PSEUDO},
    {"fpcmp.nge.s1",	f, OpXbX6Sf (1, 0, 0x36, 1), {F1, F3, F2}, PSEUDO},
    {"fpcmp.nge.s2",	f, OpXbX6Sf (1, 0, 0x36, 2), {F1, F3, F2}, PSEUDO},
    {"fpcmp.nge.s3",	f, OpXbX6Sf (1, 0, 0x36, 3), {F1, F3, F2}, PSEUDO},
    {"fpcmp.ord.s0",	f, OpXbX6Sf (1, 0, 0x37, 0), {F1, F2, F3}},
    {"fpcmp.ord",	f, OpXbX6Sf (1, 0, 0x37, 0), {F1, F2, F3}, PSEUDO},
    {"fpcmp.ord.s1",	f, OpXbX6Sf (1, 0, 0x37, 1), {F1, F2, F3}},
    {"fpcmp.ord.s2",	f, OpXbX6Sf (1, 0, 0x37, 2), {F1, F2, F3}},
    {"fpcmp.ord.s3",	f, OpXbX6Sf (1, 0, 0x37, 3), {F1, F2, F3}},

    {"fpabs",		f, OpXbX6F2 (1, 0, 0x10, 0), {F1, F3}, PSEUDO},
    {"fpneg",		f, OpXbX6   (1, 0, 0x11), {F1, F3}, PSEUDO | F2_EQ_F3},
    {"fpnegabs",	f, OpXbX6F2 (1, 0, 0x11, 0), {F1, F3}, PSEUDO},
    {"fpmerge.s",	f, OpXbX6   (1, 0, 0x10), {F1, F2, F3}},
    {"fpmerge.ns",	f, OpXbX6   (1, 0, 0x11), {F1, F2, F3}},
    {"fpmerge.se",	f, OpXbX6 (1, 0, 0x12), {F1, F2, F3}},

    {"fpcvt.fx.s0",		f, OpXbX6Sf (1, 0, 0x18, 0), {F1, F2}},
    {"fpcvt.fx",		f, OpXbX6Sf (1, 0, 0x18, 0), {F1, F2}, PSEUDO},
    {"fpcvt.fx.s1",		f, OpXbX6Sf (1, 0, 0x18, 1), {F1, F2}},
    {"fpcvt.fx.s2",		f, OpXbX6Sf (1, 0, 0x18, 2), {F1, F2}},
    {"fpcvt.fx.s3",		f, OpXbX6Sf (1, 0, 0x18, 3), {F1, F2}},
    {"fpcvt.fxu.s0",		f, OpXbX6Sf (1, 0, 0x19, 0), {F1, F2}},
    {"fpcvt.fxu",		f, OpXbX6Sf (1, 0, 0x19, 0), {F1, F2}, PSEUDO},
    {"fpcvt.fxu.s1",		f, OpXbX6Sf (1, 0, 0x19, 1), {F1, F2}},
    {"fpcvt.fxu.s2",		f, OpXbX6Sf (1, 0, 0x19, 2), {F1, F2}},
    {"fpcvt.fxu.s3",		f, OpXbX6Sf (1, 0, 0x19, 3), {F1, F2}},
    {"fpcvt.fx.trunc.s0",	f, OpXbX6Sf (1, 0, 0x1a, 0), {F1, F2}},
    {"fpcvt.fx.trunc",		f, OpXbX6Sf (1, 0, 0x1a, 0), {F1, F2}, PSEUDO},
    {"fpcvt.fx.trunc.s1",	f, OpXbX6Sf (1, 0, 0x1a, 1), {F1, F2}},
    {"fpcvt.fx.trunc.s2",	f, OpXbX6Sf (1, 0, 0x1a, 2), {F1, F2}},
    {"fpcvt.fx.trunc.s3",	f, OpXbX6Sf (1, 0, 0x1a, 3), {F1, F2}},
    {"fpcvt.fxu.trunc.s0",	f, OpXbX6Sf (1, 0, 0x1b, 0), {F1, F2}},
    {"fpcvt.fxu.trunc",		f, OpXbX6Sf (1, 0, 0x1b, 0), {F1, F2}, PSEUDO},
    {"fpcvt.fxu.trunc.s1",	f, OpXbX6Sf (1, 0, 0x1b, 1), {F1, F2}},
    {"fpcvt.fxu.trunc.s2",	f, OpXbX6Sf (1, 0, 0x1b, 2), {F1, F2}},
    {"fpcvt.fxu.trunc.s3",	f, OpXbX6Sf (1, 0, 0x1b, 3), {F1, F2}},

    {"fcmp.eq.s0",	  f2, OpRaRbTaSf (4, 0, 0, 0, 0), {P1, P2, F2, F3}},
    {"fcmp.eq",		  f2, OpRaRbTaSf (4, 0, 0, 0, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.eq.s1",	  f2, OpRaRbTaSf (4, 0, 0, 0, 1), {P1, P2, F2, F3}},
    {"fcmp.eq.s2",	  f2, OpRaRbTaSf (4, 0, 0, 0, 2), {P1, P2, F2, F3}},
    {"fcmp.eq.s3",	  f2, OpRaRbTaSf (4, 0, 0, 0, 3), {P1, P2, F2, F3}},
    {"fcmp.lt.s0",	  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P1, P2, F2, F3}},
    {"fcmp.lt",		  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.lt.s1",	  f2, OpRaRbTaSf (4, 0, 1, 0, 1), {P1, P2, F2, F3}},
    {"fcmp.lt.s2",	  f2, OpRaRbTaSf (4, 0, 1, 0, 2), {P1, P2, F2, F3}},
    {"fcmp.lt.s3",	  f2, OpRaRbTaSf (4, 0, 1, 0, 3), {P1, P2, F2, F3}},
    {"fcmp.le.s0",	  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P1, P2, F2, F3}},
    {"fcmp.le",		  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.le.s1",	  f2, OpRaRbTaSf (4, 1, 0, 0, 1), {P1, P2, F2, F3}},
    {"fcmp.le.s2",	  f2, OpRaRbTaSf (4, 1, 0, 0, 2), {P1, P2, F2, F3}},
    {"fcmp.le.s3",	  f2, OpRaRbTaSf (4, 1, 0, 0, 3), {P1, P2, F2, F3}},
    {"fcmp.unord.s0",	  f2, OpRaRbTaSf (4, 1, 1, 0, 0), {P1, P2, F2, F3}},
    {"fcmp.unord",	  f2, OpRaRbTaSf (4, 1, 1, 0, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.unord.s1",	  f2, OpRaRbTaSf (4, 1, 1, 0, 1), {P1, P2, F2, F3}},
    {"fcmp.unord.s2",	  f2, OpRaRbTaSf (4, 1, 1, 0, 2), {P1, P2, F2, F3}},
    {"fcmp.unord.s3",	  f2, OpRaRbTaSf (4, 1, 1, 0, 3), {P1, P2, F2, F3}},
    {"fcmp.eq.unc.s0",	  f2, OpRaRbTaSf (4, 0, 0, 1, 0), {P1, P2, F2, F3}},
    {"fcmp.eq.unc",	  f2, OpRaRbTaSf (4, 0, 0, 1, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.eq.unc.s1",	  f2, OpRaRbTaSf (4, 0, 0, 1, 1), {P1, P2, F2, F3}},
    {"fcmp.eq.unc.s2",	  f2, OpRaRbTaSf (4, 0, 0, 1, 2), {P1, P2, F2, F3}},
    {"fcmp.eq.unc.s3",	  f2, OpRaRbTaSf (4, 0, 0, 1, 3), {P1, P2, F2, F3}},
    {"fcmp.lt.unc.s0",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P1, P2, F2, F3}},
    {"fcmp.lt.unc",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.lt.unc.s1",	  f2, OpRaRbTaSf (4, 0, 1, 1, 1), {P1, P2, F2, F3}},
    {"fcmp.lt.unc.s2",	  f2, OpRaRbTaSf (4, 0, 1, 1, 2), {P1, P2, F2, F3}},
    {"fcmp.lt.unc.s3",	  f2, OpRaRbTaSf (4, 0, 1, 1, 3), {P1, P2, F2, F3}},
    {"fcmp.le.unc.s0",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P1, P2, F2, F3}},
    {"fcmp.le.unc",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.le.unc.s1",	  f2, OpRaRbTaSf (4, 1, 0, 1, 1), {P1, P2, F2, F3}},
    {"fcmp.le.unc.s2",	  f2, OpRaRbTaSf (4, 1, 0, 1, 2), {P1, P2, F2, F3}},
    {"fcmp.le.unc.s3",	  f2, OpRaRbTaSf (4, 1, 0, 1, 3), {P1, P2, F2, F3}},
    {"fcmp.unord.unc.s0", f2, OpRaRbTaSf (4, 1, 1, 1, 0), {P1, P2, F2, F3}},
    {"fcmp.unord.unc",    f2, OpRaRbTaSf (4, 1, 1, 1, 0), {P1, P2, F2, F3}, PSEUDO},
    {"fcmp.unord.unc.s1", f2, OpRaRbTaSf (4, 1, 1, 1, 1), {P1, P2, F2, F3}},
    {"fcmp.unord.unc.s2", f2, OpRaRbTaSf (4, 1, 1, 1, 2), {P1, P2, F2, F3}},
    {"fcmp.unord.unc.s3", f2, OpRaRbTaSf (4, 1, 1, 1, 3), {P1, P2, F2, F3}},

    /* pseudo-ops of the above */
    {"fcmp.gt.s0",	  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P1, P2, F3, F2}},
    {"fcmp.gt",		  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P1, P2, F3, F2}, PSEUDO},
    {"fcmp.gt.s1",	  f2, OpRaRbTaSf (4, 0, 1, 0, 1), {P1, P2, F3, F2}},
    {"fcmp.gt.s2",	  f2, OpRaRbTaSf (4, 0, 1, 0, 2), {P1, P2, F3, F2}},
    {"fcmp.gt.s3",	  f2, OpRaRbTaSf (4, 0, 1, 0, 3), {P1, P2, F3, F2}},
    {"fcmp.ge.s0",	  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P1, P2, F3, F2}},
    {"fcmp.ge",		  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P1, P2, F3, F2}, PSEUDO},
    {"fcmp.ge.s1",	  f2, OpRaRbTaSf (4, 1, 0, 0, 1), {P1, P2, F3, F2}},
    {"fcmp.ge.s2",	  f2, OpRaRbTaSf (4, 1, 0, 0, 2), {P1, P2, F3, F2}},
    {"fcmp.ge.s3",	  f2, OpRaRbTaSf (4, 1, 0, 0, 3), {P1, P2, F3, F2}},
    {"fcmp.neq.s0",	  f2, OpRaRbTaSf (4, 0, 0, 0, 0), {P2, P1, F2, F3}},
    {"fcmp.neq",	  f2, OpRaRbTaSf (4, 0, 0, 0, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.neq.s1",	  f2, OpRaRbTaSf (4, 0, 0, 0, 1), {P2, P1, F2, F3}},
    {"fcmp.neq.s2",	  f2, OpRaRbTaSf (4, 0, 0, 0, 2), {P2, P1, F2, F3}},
    {"fcmp.neq.s3",	  f2, OpRaRbTaSf (4, 0, 0, 0, 3), {P2, P1, F2, F3}},
    {"fcmp.nlt.s0",	  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P2, P1, F2, F3}},
    {"fcmp.nlt",	  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.nlt.s1",	  f2, OpRaRbTaSf (4, 0, 1, 0, 1), {P2, P1, F2, F3}},
    {"fcmp.nlt.s2",	  f2, OpRaRbTaSf (4, 0, 1, 0, 2), {P2, P1, F2, F3}},
    {"fcmp.nlt.s3",	  f2, OpRaRbTaSf (4, 0, 1, 0, 3), {P2, P1, F2, F3}},
    {"fcmp.nle.s0",	  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P2, P1, F2, F3}},
    {"fcmp.nle",	  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.nle.s1",	  f2, OpRaRbTaSf (4, 1, 0, 0, 1), {P2, P1, F2, F3}},
    {"fcmp.nle.s2",	  f2, OpRaRbTaSf (4, 1, 0, 0, 2), {P2, P1, F2, F3}},
    {"fcmp.nle.s3",	  f2, OpRaRbTaSf (4, 1, 0, 0, 3), {P2, P1, F2, F3}},
    {"fcmp.ngt.s0",	  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P2, P1, F3, F2}},
    {"fcmp.ngt",	  f2, OpRaRbTaSf (4, 0, 1, 0, 0), {P2, P1, F3, F2}, PSEUDO},
    {"fcmp.ngt.s1",	  f2, OpRaRbTaSf (4, 0, 1, 0, 1), {P2, P1, F3, F2}},
    {"fcmp.ngt.s2",	  f2, OpRaRbTaSf (4, 0, 1, 0, 2), {P2, P1, F3, F2}},
    {"fcmp.ngt.s3",	  f2, OpRaRbTaSf (4, 0, 1, 0, 3), {P2, P1, F3, F2}},
    {"fcmp.nge.s0",	  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P2, P1, F3, F2}},
    {"fcmp.nge",	  f2, OpRaRbTaSf (4, 1, 0, 0, 0), {P2, P1, F3, F2}, PSEUDO},
    {"fcmp.nge.s1",	  f2, OpRaRbTaSf (4, 1, 0, 0, 1), {P2, P1, F3, F2}},
    {"fcmp.nge.s2",	  f2, OpRaRbTaSf (4, 1, 0, 0, 2), {P2, P1, F3, F2}},
    {"fcmp.nge.s3",	  f2, OpRaRbTaSf (4, 1, 0, 0, 3), {P2, P1, F3, F2}},
    {"fcmp.ord.s0",	  f2, OpRaRbTaSf (4, 1, 1, 0, 0), {P2, P1, F2, F3}},
    {"fcmp.ord",	  f2, OpRaRbTaSf (4, 1, 1, 0, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.ord.s1",	  f2, OpRaRbTaSf (4, 1, 1, 0, 1), {P2, P1, F2, F3}},
    {"fcmp.ord.s2",	  f2, OpRaRbTaSf (4, 1, 1, 0, 2), {P2, P1, F2, F3}},
    {"fcmp.ord.s3",	  f2, OpRaRbTaSf (4, 1, 1, 0, 3), {P2, P1, F2, F3}},
    {"fcmp.gt.unc.s0",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P1, P2, F3, F2}},
    {"fcmp.gt.unc",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P1, P2, F3, F2}, PSEUDO},
    {"fcmp.gt.unc.s1",	  f2, OpRaRbTaSf (4, 0, 1, 1, 1), {P1, P2, F3, F2}},
    {"fcmp.gt.unc.s2",	  f2, OpRaRbTaSf (4, 0, 1, 1, 2), {P1, P2, F3, F2}},
    {"fcmp.gt.unc.s3",	  f2, OpRaRbTaSf (4, 0, 1, 1, 3), {P1, P2, F3, F2}},
    {"fcmp.ge.unc.s0",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P1, P2, F3, F2}},
    {"fcmp.ge.unc",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P1, P2, F3, F2}, PSEUDO},
    {"fcmp.ge.unc.s1",	  f2, OpRaRbTaSf (4, 1, 0, 1, 1), {P1, P2, F3, F2}},
    {"fcmp.ge.unc.s2",	  f2, OpRaRbTaSf (4, 1, 0, 1, 2), {P1, P2, F3, F2}},
    {"fcmp.ge.unc.s3",	  f2, OpRaRbTaSf (4, 1, 0, 1, 3), {P1, P2, F3, F2}},
    {"fcmp.neq.unc.s0",	  f2, OpRaRbTaSf (4, 0, 0, 1, 0), {P2, P1, F2, F3}},
    {"fcmp.neq.unc",	  f2, OpRaRbTaSf (4, 0, 0, 1, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.neq.unc.s1",	  f2, OpRaRbTaSf (4, 0, 0, 1, 1), {P2, P1, F2, F3}},
    {"fcmp.neq.unc.s2",	  f2, OpRaRbTaSf (4, 0, 0, 1, 2), {P2, P1, F2, F3}},
    {"fcmp.neq.unc.s3",	  f2, OpRaRbTaSf (4, 0, 0, 1, 3), {P2, P1, F2, F3}},
    {"fcmp.nlt.unc.s0",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P2, P1, F2, F3}},
    {"fcmp.nlt.unc",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.nlt.unc.s1",	  f2, OpRaRbTaSf (4, 0, 1, 1, 1), {P2, P1, F2, F3}},
    {"fcmp.nlt.unc.s2",	  f2, OpRaRbTaSf (4, 0, 1, 1, 2), {P2, P1, F2, F3}},
    {"fcmp.nlt.unc.s3",	  f2, OpRaRbTaSf (4, 0, 1, 1, 3), {P2, P1, F2, F3}},
    {"fcmp.nle.unc.s0",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P2, P1, F2, F3}},
    {"fcmp.nle.unc",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.nle.unc.s1",	  f2, OpRaRbTaSf (4, 1, 0, 1, 1), {P2, P1, F2, F3}},
    {"fcmp.nle.unc.s2",	  f2, OpRaRbTaSf (4, 1, 0, 1, 2), {P2, P1, F2, F3}},
    {"fcmp.nle.unc.s3",	  f2, OpRaRbTaSf (4, 1, 0, 1, 3), {P2, P1, F2, F3}},
    {"fcmp.ngt.unc.s0",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P2, P1, F3, F2}},
    {"fcmp.ngt.unc",	  f2, OpRaRbTaSf (4, 0, 1, 1, 0), {P2, P1, F3, F2}, PSEUDO},
    {"fcmp.ngt.unc.s1",	  f2, OpRaRbTaSf (4, 0, 1, 1, 1), {P2, P1, F3, F2}},
    {"fcmp.ngt.unc.s2",	  f2, OpRaRbTaSf (4, 0, 1, 1, 2), {P2, P1, F3, F2}},
    {"fcmp.ngt.unc.s3",	  f2, OpRaRbTaSf (4, 0, 1, 1, 3), {P2, P1, F3, F2}},
    {"fcmp.nge.unc.s0",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P2, P1, F3, F2}},
    {"fcmp.nge.unc",	  f2, OpRaRbTaSf (4, 1, 0, 1, 0), {P2, P1, F3, F2}, PSEUDO},
    {"fcmp.nge.unc.s1",	  f2, OpRaRbTaSf (4, 1, 0, 1, 1), {P2, P1, F3, F2}},
    {"fcmp.nge.unc.s2",	  f2, OpRaRbTaSf (4, 1, 0, 1, 2), {P2, P1, F3, F2}},
    {"fcmp.nge.unc.s3",	  f2, OpRaRbTaSf (4, 1, 0, 1, 3), {P2, P1, F3, F2}},
    {"fcmp.ord.unc.s0",	  f2, OpRaRbTaSf (4, 1, 1, 1, 0), {P2, P1, F2, F3}},
    {"fcmp.ord.unc",	  f2, OpRaRbTaSf (4, 1, 1, 1, 0), {P2, P1, F2, F3}, PSEUDO},
    {"fcmp.ord.unc.s1",   f2, OpRaRbTaSf (4, 1, 1, 1, 1), {P2, P1, F2, F3}},
    {"fcmp.ord.unc.s2",   f2, OpRaRbTaSf (4, 1, 1, 1, 2), {P2, P1, F2, F3}},
    {"fcmp.ord.unc.s3",   f2, OpRaRbTaSf (4, 1, 1, 1, 3), {P2, P1, F2, F3}},

    {"fclass.m",	f2, OpTa (5, 0), {P1, P2, F2, IMMU9}},
    {"fclass.nm",	f2, OpTa (5, 0), {P2, P1, F2, IMMU9}, PSEUDO},
    {"fclass.m.unc",	f2, OpTa (5, 1), {P1, P2, F2, IMMU9}},
    {"fclass.nm.unc",	f2, OpTa (5, 1), {P2, P1, F2, IMMU9}, PSEUDO},

    /* note: fnorm and fcvt.xuf have identical encodings! */
    {"fnorm.s0",	f, OpXaSfF2F4 (0x8, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm",		f, OpXaSfF2F4 (0x8, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s1",	f, OpXaSfF2F4 (0x8, 0, 1, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s2",	f, OpXaSfF2F4 (0x8, 0, 2, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s3",	f, OpXaSfF2F4 (0x8, 0, 3, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s.s0",	f, OpXaSfF2F4 (0x8, 1, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s",		f, OpXaSfF2F4 (0x8, 1, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s.s1",	f, OpXaSfF2F4 (0x8, 1, 1, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s.s2",	f, OpXaSfF2F4 (0x8, 1, 2, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.s.s3",	f, OpXaSfF2F4 (0x8, 1, 3, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s0",	f, OpXaSfF2F4 (0x8, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf",	f, OpXaSfF2F4 (0x8, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s1",	f, OpXaSfF2F4 (0x8, 0, 1, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s2",	f, OpXaSfF2F4 (0x8, 0, 2, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s3",	f, OpXaSfF2F4 (0x8, 0, 3, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s.s0",	f, OpXaSfF2F4 (0x8, 1, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s",	f, OpXaSfF2F4 (0x8, 1, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s.s1",	f, OpXaSfF2F4 (0x8, 1, 1, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s.s2",	f, OpXaSfF2F4 (0x8, 1, 2, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.s.s3",	f, OpXaSfF2F4 (0x8, 1, 3, 0, 1), {F1, F3}, PSEUDO},
    {"fadd.s0",		f, OpXaSfF4 (0x8, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fadd",		f, OpXaSfF4 (0x8, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s1",		f, OpXaSfF4 (0x8, 0, 1, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s2",		f, OpXaSfF4 (0x8, 0, 2, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s3",		f, OpXaSfF4 (0x8, 0, 3, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s.s0",	f, OpXaSfF4 (0x8, 1, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s",		f, OpXaSfF4 (0x8, 1, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s.s1",	f, OpXaSfF4 (0x8, 1, 1, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s.s2",	f, OpXaSfF4 (0x8, 1, 2, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.s.s3",	f, OpXaSfF4 (0x8, 1, 3, 1), {F1, F3, F2}, PSEUDO},
    {"fmpy.s0",		f, OpXaSfF2 (0x8, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy",		f, OpXaSfF2 (0x8, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s1",		f, OpXaSfF2 (0x8, 0, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s2",		f, OpXaSfF2 (0x8, 0, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s3",		f, OpXaSfF2 (0x8, 0, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s.s0",	f, OpXaSfF2 (0x8, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s",		f, OpXaSfF2 (0x8, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s.s1",	f, OpXaSfF2 (0x8, 1, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s.s2",	f, OpXaSfF2 (0x8, 1, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.s.s3",	f, OpXaSfF2 (0x8, 1, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fma.s0",		f, OpXaSf (0x8, 0, 0), {F1, F3, F4, F2}},
    {"fma",		f, OpXaSf (0x8, 0, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fma.s1",		f, OpXaSf (0x8, 0, 1), {F1, F3, F4, F2}},
    {"fma.s2",		f, OpXaSf (0x8, 0, 2), {F1, F3, F4, F2}},
    {"fma.s3",		f, OpXaSf (0x8, 0, 3), {F1, F3, F4, F2}},
    {"fma.s.s0",	f, OpXaSf (0x8, 1, 0), {F1, F3, F4, F2}},
    {"fma.s",		f, OpXaSf (0x8, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fma.s.s1",	f, OpXaSf (0x8, 1, 1), {F1, F3, F4, F2}},
    {"fma.s.s2",	f, OpXaSf (0x8, 1, 2), {F1, F3, F4, F2}},
    {"fma.s.s3",	f, OpXaSf (0x8, 1, 3), {F1, F3, F4, F2}},

    {"fnorm.d.s0",	f, OpXaSfF2F4 (0x9, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.d",		f, OpXaSfF2F4 (0x9, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.d.s1",	f, OpXaSfF2F4 (0x9, 0, 1, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.d.s2",	f, OpXaSfF2F4 (0x9, 0, 2, 0, 1), {F1, F3}, PSEUDO},
    {"fnorm.d.s3",	f, OpXaSfF2F4 (0x9, 0, 3, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.d.s0",	f, OpXaSfF2F4 (0x9, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.d",	f, OpXaSfF2F4 (0x9, 0, 0, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.d.s1",	f, OpXaSfF2F4 (0x9, 0, 1, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.d.s2",	f, OpXaSfF2F4 (0x9, 0, 2, 0, 1), {F1, F3}, PSEUDO},
    {"fcvt.xuf.d.s3",	f, OpXaSfF2F4 (0x9, 0, 3, 0, 1), {F1, F3}, PSEUDO},
    {"fadd.d.s0",	f, OpXaSfF4 (0x9, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.d",		f, OpXaSfF4 (0x9, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.d.s1",	f, OpXaSfF4 (0x9, 0, 1, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.d.s2",	f, OpXaSfF4 (0x9, 0, 2, 1), {F1, F3, F2}, PSEUDO},
    {"fadd.d.s3",	f, OpXaSfF4 (0x9, 0, 3, 1), {F1, F3, F2}, PSEUDO},
    {"fmpy.d.s0",	f, OpXaSfF2 (0x9, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.d",		f, OpXaSfF2 (0x9, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.d.s1",	f, OpXaSfF2 (0x9, 0, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.d.s2",	f, OpXaSfF2 (0x9, 0, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fmpy.d.s3",	f, OpXaSfF2 (0x9, 0, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fma.d.s0",	f, OpXaSf (0x9, 0, 0), {F1, F3, F4, F2}},
    {"fma.d",		f, OpXaSf (0x9, 0, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fma.d.s1",	f, OpXaSf (0x9, 0, 1), {F1, F3, F4, F2}},
    {"fma.d.s2",	f, OpXaSf (0x9, 0, 2), {F1, F3, F4, F2}},
    {"fma.d.s3",	f, OpXaSf (0x9, 0, 3), {F1, F3, F4, F2}},

    {"fpmpy.s0",	f, OpXaSfF2 (0x9, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fpmpy",		f, OpXaSfF2 (0x9, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fpmpy.s1",	f, OpXaSfF2 (0x9, 1, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fpmpy.s2",	f, OpXaSfF2 (0x9, 1, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fpmpy.s3",	f, OpXaSfF2 (0x9, 1, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fpma.s0",		f, OpXaSf   (0x9, 1, 0), {F1, F3, F4, F2}},
    {"fpma",		f, OpXaSf   (0x9, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fpma.s1",		f, OpXaSf   (0x9, 1, 1), {F1, F3, F4, F2}},
    {"fpma.s2",		f, OpXaSf   (0x9, 1, 2), {F1, F3, F4, F2}},
    {"fpma.s3",		f, OpXaSf   (0x9, 1, 3), {F1, F3, F4, F2}},

    {"fsub.s0",		f, OpXaSfF4 (0xa, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fsub",		f, OpXaSfF4 (0xa, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s1",		f, OpXaSfF4 (0xa, 0, 1, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s2",		f, OpXaSfF4 (0xa, 0, 2, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s3",		f, OpXaSfF4 (0xa, 0, 3, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s.s0",	f, OpXaSfF4 (0xa, 1, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s",		f, OpXaSfF4 (0xa, 1, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s.s1",	f, OpXaSfF4 (0xa, 1, 1, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s.s2",	f, OpXaSfF4 (0xa, 1, 2, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.s.s3",	f, OpXaSfF4 (0xa, 1, 3, 1), {F1, F3, F2}, PSEUDO},
    {"fms.s0",		f, OpXaSf   (0xa, 0, 0), {F1, F3, F4, F2}},
    {"fms",		f, OpXaSf   (0xa, 0, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fms.s1",		f, OpXaSf   (0xa, 0, 1), {F1, F3, F4, F2}},
    {"fms.s2",		f, OpXaSf   (0xa, 0, 2), {F1, F3, F4, F2}},
    {"fms.s3",		f, OpXaSf   (0xa, 0, 3), {F1, F3, F4, F2}},
    {"fms.s.s0",	f, OpXaSf   (0xa, 1, 0), {F1, F3, F4, F2}},
    {"fms.s",		f, OpXaSf   (0xa, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fms.s.s1",	f, OpXaSf   (0xa, 1, 1), {F1, F3, F4, F2}},
    {"fms.s.s2",	f, OpXaSf   (0xa, 1, 2), {F1, F3, F4, F2}},
    {"fms.s.s3",	f, OpXaSf   (0xa, 1, 3), {F1, F3, F4, F2}},
    {"fsub.d.s0",	f, OpXaSfF4 (0xb, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.d",		f, OpXaSfF4 (0xb, 0, 0, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.d.s1",	f, OpXaSfF4 (0xb, 0, 1, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.d.s2",	f, OpXaSfF4 (0xb, 0, 2, 1), {F1, F3, F2}, PSEUDO},
    {"fsub.d.s3",	f, OpXaSfF4 (0xb, 0, 3, 1), {F1, F3, F2}, PSEUDO},
    {"fms.d.s0",	f, OpXaSf   (0xb, 0, 0), {F1, F3, F4, F2}},
    {"fms.d",		f, OpXaSf   (0xb, 0, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fms.d.s1",	f, OpXaSf   (0xb, 0, 1), {F1, F3, F4, F2}},
    {"fms.d.s2",	f, OpXaSf   (0xb, 0, 2), {F1, F3, F4, F2}},
    {"fms.d.s3",	f, OpXaSf   (0xb, 0, 3), {F1, F3, F4, F2}},

    {"fpms.s0",		f, OpXaSf (0xb, 1, 0), {F1, F3, F4, F2}},
    {"fpms",		f, OpXaSf (0xb, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fpms.s1",		f, OpXaSf (0xb, 1, 1), {F1, F3, F4, F2}},
    {"fpms.s2",		f, OpXaSf (0xb, 1, 2), {F1, F3, F4, F2}},
    {"fpms.s3",		f, OpXaSf (0xb, 1, 3), {F1, F3, F4, F2}},

    {"fnmpy.s0",	f, OpXaSfF2 (0xc, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy",		f, OpXaSfF2 (0xc, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s1",	f, OpXaSfF2 (0xc, 0, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s2",	f, OpXaSfF2 (0xc, 0, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s3",	f, OpXaSfF2 (0xc, 0, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s.s0",	f, OpXaSfF2 (0xc, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s",		f, OpXaSfF2 (0xc, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s.s1",	f, OpXaSfF2 (0xc, 1, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s.s2",	f, OpXaSfF2 (0xc, 1, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.s.s3",	f, OpXaSfF2 (0xc, 1, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fnma.s0",		f, OpXaSf (0xc, 0, 0), {F1, F3, F4, F2}},
    {"fnma",		f, OpXaSf (0xc, 0, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fnma.s1",		f, OpXaSf (0xc, 0, 1), {F1, F3, F4, F2}},
    {"fnma.s2",		f, OpXaSf (0xc, 0, 2), {F1, F3, F4, F2}},
    {"fnma.s3",		f, OpXaSf (0xc, 0, 3), {F1, F3, F4, F2}},
    {"fnma.s.s0",	f, OpXaSf (0xc, 1, 0), {F1, F3, F4, F2}},
    {"fnma.s",		f, OpXaSf (0xc, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fnma.s.s1",	f, OpXaSf (0xc, 1, 1), {F1, F3, F4, F2}},
    {"fnma.s.s2",	f, OpXaSf (0xc, 1, 2), {F1, F3, F4, F2}},
    {"fnma.s.s3",	f, OpXaSf (0xc, 1, 3), {F1, F3, F4, F2}},
    {"fnmpy.d.s0",	f, OpXaSfF2 (0xd, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.d",		f, OpXaSfF2 (0xd, 0, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.d.s1",	f, OpXaSfF2 (0xd, 0, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.d.s2",	f, OpXaSfF2 (0xd, 0, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fnmpy.d.s3",	f, OpXaSfF2 (0xd, 0, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fnma.d.s0",	f, OpXaSf (0xd, 0, 0), {F1, F3, F4, F2}},
    {"fnma.d",		f, OpXaSf (0xd, 0, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fnma.d.s1",	f, OpXaSf (0xd, 0, 1), {F1, F3, F4, F2}},
    {"fnma.d.s2",	f, OpXaSf (0xd, 0, 2), {F1, F3, F4, F2}},
    {"fnma.d.s3",	f, OpXaSf (0xd, 0, 3), {F1, F3, F4, F2}},

    {"fpnmpy.s0",	f, OpXaSfF2 (0xd, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fpnmpy",		f, OpXaSfF2 (0xd, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"fpnmpy.s1",	f, OpXaSfF2 (0xd, 1, 1, 0), {F1, F3, F4}, PSEUDO},
    {"fpnmpy.s2",	f, OpXaSfF2 (0xd, 1, 2, 0), {F1, F3, F4}, PSEUDO},
    {"fpnmpy.s3",	f, OpXaSfF2 (0xd, 1, 3, 0), {F1, F3, F4}, PSEUDO},
    {"fpnma.s0",	f, OpXaSf   (0xd, 1, 0), {F1, F3, F4, F2}},
    {"fpnma",		f, OpXaSf   (0xd, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"fpnma.s1",	f, OpXaSf   (0xd, 1, 1), {F1, F3, F4, F2}},
    {"fpnma.s2",	f, OpXaSf   (0xd, 1, 2), {F1, F3, F4, F2}},
    {"fpnma.s3",	f, OpXaSf   (0xd, 1, 3), {F1, F3, F4, F2}},

    {"xmpy.l",		f, OpXaX2F2 (0xe, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"xmpy.lu",		f, OpXaX2F2 (0xe, 1, 0, 0), {F1, F3, F4}, PSEUDO},
    {"xmpy.h",		f, OpXaX2F2 (0xe, 1, 3, 0), {F1, F3, F4}, PSEUDO},
    {"xmpy.hu",		f, OpXaX2F2 (0xe, 1, 2, 0), {F1, F3, F4}, PSEUDO},
    {"xma.l",		f, OpXaX2 (0xe, 1, 0), {F1, F3, F4, F2}},
    {"xma.lu",		f, OpXaX2 (0xe, 1, 0), {F1, F3, F4, F2}, PSEUDO},
    {"xma.h",		f, OpXaX2 (0xe, 1, 3), {F1, F3, F4, F2}},
    {"xma.hu",		f, OpXaX2 (0xe, 1, 2), {F1, F3, F4, F2}},

    {"fselect",		f, OpXa (0xe, 0), {F1, F3, F4, F2}},

    {0}
  };

#undef f0
#undef f
#undef f2
#undef bF2
#undef bF4
#undef bQ
#undef bRa
#undef bRb
#undef bSf
#undef bTa
#undef bXa
#undef bXb
#undef bX2
#undef bX6
#undef mF2
#undef mF4
#undef mQ
#undef mRa
#undef mRb
#undef mSf
#undef mTa
#undef mXa
#undef mXb
#undef mX2
#undef mX6
#undef OpXa
#undef OpXaSf
#undef OpXaSfF2
#undef OpXaSfF4
#undef OpXaSfF2F4
#undef OpXaX2
#undef OpRaRbTaSf
#undef OpTa
#undef OpXbQSf
#undef OpXbX6
#undef OpXbX6F2
#undef OpXbX6Sf
