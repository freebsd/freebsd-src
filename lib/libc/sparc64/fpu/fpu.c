/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)fpu.c	8.1 (Berkeley) 6/11/93
 *	$NetBSD: fpu.c,v 1.11 2000/12/06 01:47:50 mrg Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include "namespace.h"
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "un-namespace.h"
#include "libc_private.h"

#include <machine/emul.h>
#include <machine/fp.h>
#include <machine/frame.h>
#include <machine/fsr.h>
#include <machine/instr.h>
#include <machine/pcb.h>
#include <machine/tstate.h>

#include "../sys/__sparc_utrap_private.h"
#include "fpu_emu.h"
#include "fpu_extern.h"

/*
 * Translate current exceptions into `first' exception.  The
 * bits go the wrong way for ffs() (0x10 is most important, etc).
 * There are only 5, so do it the obvious way.
 */
#define	X1(x) x
#define	X2(x) x,x
#define	X4(x) x,x,x,x
#define	X8(x) X4(x),X4(x)
#define	X16(x) X8(x),X8(x)

static char cx_to_trapx[] = {
	X1(FSR_NX),
	X2(FSR_DZ),
	X4(FSR_UF),
	X8(FSR_OF),
	X16(FSR_NV)
};

#ifdef FPU_DEBUG
#ifdef FPU_DEBUG_MASK
int __fpe_debug = FPU_DEBUG_MASK;
#else
int __fpe_debug = 0;
#endif
#endif	/* FPU_DEBUG */

static int __fpu_execute(struct utrapframe *, struct fpemu *, u_int32_t, u_long);
static void utrap_write(char *);
static void utrap_kill_self(int);

/*
 * System call wrappers usable in an utrap environment.
 */
static void
utrap_write(char *str)
{
	int berrno;

	berrno = errno;
	__sys_write(STDERR_FILENO, str, strlen(str));
	errno = berrno;
}

static void
utrap_kill_self(sig)
{
	int berrno;

	berrno = errno;
	__sys_kill(__sys_getpid(), sig);
	errno = berrno;
}

void
__fpu_panic(char *msg)
{

	utrap_write(msg);
	utrap_write("\n");
	utrap_kill_self(SIGKILL);
}

/*
 * Need to use an fpstate on the stack; we could switch, so we cannot safely
 * modify the pcb one, it might get overwritten.
 */
void
__fpu_exception(struct utrapframe *uf)
{
	struct fpemu fe;
	u_long fsr, tstate;
	u_int insn;
	int rv;

	fsr = uf->uf_fsr;

	switch (FSR_GET_FTT(fsr)) {
	case FSR_FTT_NONE:
		utrap_write("lost FPU trap type\n");
		return;
	case FSR_FTT_IEEE:
		goto fatal;
	case FSR_FTT_SEQERR:
		utrap_write("FPU sequence error\n");
		goto fatal;
	case FSR_FTT_HWERR:
		utrap_write("FPU hardware error\n");
		goto fatal;
	case FSR_FTT_UNFIN:
	case FSR_FTT_UNIMP:
		break;
	default:
		utrap_write("unknown FPU error\n");
		goto fatal;
	}

	fe.fe_fsr = fsr & ~FSR_FTT_MASK;
	insn = *(u_int32_t *)uf->uf_pc;
	if (IF_OP(insn) != IOP_MISC || (IF_F3_OP3(insn) != INS2_FPop1 &&
	    IF_F3_OP3(insn) != INS2_FPop2))
		__fpu_panic("bogus FP fault");
	tstate = uf->uf_state;
	rv = __fpu_execute(uf, &fe, insn, tstate);
	if (rv != 0)
		utrap_kill_self(rv);
	__asm __volatile("ldx %0, %%fsr" : : "m" (fe.fe_fsr));
	return;
fatal:
	utrap_kill_self(SIGFPE);
	return;
}

#ifdef FPU_DEBUG
/*
 * Dump a `fpn' structure.
 */
void
__fpu_dumpfpn(struct fpn *fp)
{
	static char *class[] = {
		"SNAN", "QNAN", "ZERO", "NUM", "INF"
	};

	printf("%s %c.%x %x %x %xE%d", class[fp->fp_class + 2],
		fp->fp_sign ? '-' : ' ',
		fp->fp_mant[0],	fp->fp_mant[1],
		fp->fp_mant[2], fp->fp_mant[3],
		fp->fp_exp);
}
#endif

static u_long
fetch_reg(struct utrapframe *uf, int reg)
{
	u_long offs;
	struct frame *frm;

	if (reg == IREG_G0)
		return (0);
	else if (reg < IREG_O0)	/* global */
		return (uf->uf_global[reg]);
	else if (reg < IREG_L0)	/* out */
		return (uf->uf_out[reg - IREG_O0]);
	else {			/* local, in */
		/*
		 * The in registers are immediately after the locals in
		 * the frame.
		 */
		frm = (struct frame *)(uf->uf_out[6] + SPOFF);
		return (frm->f_local[reg - IREG_L0]);
	}
	__fpu_panic("fetch_reg: bogus register");
}

static void
__fpu_mov(struct fpemu *fe, int type, int rd, int rs1, int rs2)
{
	int i;

	i = 1 << type;
	__fpu_setreg(rd++, rs1);
	while (--i)
		__fpu_setreg(rd++, __fpu_getreg(++rs2));
}

static __inline void
__fpu_ccmov(struct fpemu *fe, int type, int rd, int rs1, int rs2,
    u_int32_t insn, int fcc)
{

	if (IF_F4_COND(insn) == fcc)
		__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
}

static int
__fpu_cmpck(struct fpemu *fe)
{
	u_long fsr;
	int cx;

	/*
	 * The only possible exception here is NV; catch it
	 * early and get out, as there is no result register.
	 */
	cx = fe->fe_cx;
	fsr = fe->fe_fsr | (cx << FSR_CEXC_SHIFT);
	if (cx != 0) {
		if (fsr & (FSR_NV << FSR_TEM_SHIFT)) {
			fe->fe_fsr = (fsr & ~FSR_FTT_MASK) |
			    FSR_FTT(FSR_FTT_IEEE);
			return (SIGFPE);
		}
		fsr |= FSR_NV << FSR_AEXC_SHIFT;
	}
	fe->fe_fsr = fsr;
	return (0);
}

static int opmask[] = {0, 0, 1, 3};

/*
 * Helper for forming the below case statements. Build only the op3 and opf
 * field of the instruction, these are the only that need to match.
 */
#define	FOP(op3, opf) \
	((op3) << IF_F3_OP3_SHIFT | (opf) << IF_F3_OPF_SHIFT)

/*
 * Execute an FPU instruction (one that runs entirely in the FPU; not
 * FBfcc or STF, for instance).  On return, fe->fe_fs->fs_fsr will be
 * modified to reflect the setting the hardware would have left.
 *
 * Note that we do not catch all illegal opcodes, so you can, for instance,
 * multiply two integers this way.
 */
static int
__fpu_execute(struct utrapframe *uf, struct fpemu *fe, u_int32_t insn, u_long tstate)
{
	struct fpn *fp;
	int opf, rs1, rs2, rd, type, mask, cx, cond;
	u_long reg, fsr;
	u_int space[4];

	/*
	 * `Decode' and execute instruction.  Start with no exceptions.
	 * The type of any opf opcode is in the bottom two bits, so we
	 * squish them out here.
	 */
	opf = insn & (IF_MASK(IF_F3_OP3_SHIFT, IF_F3_OP3_BITS) |
	    IF_MASK(IF_F3_OPF_SHIFT + 2, IF_F3_OPF_BITS - 2));
	type = IF_F3_OPF(insn) & 3;
	mask = opmask[type];
	rs1 = IF_F3_RS1(insn) & ~mask;
	rs2 = IF_F3_RS2(insn) & ~mask;
	rd = IF_F3_RD(insn) & ~mask;
	cond = 0;
#ifdef notdef
	if ((rs1 | rs2 | rd) & mask)
		return (SIGILL);
#endif
	fsr = fe->fe_fsr;
	fe->fe_fsr &= ~FSR_CEXC_MASK;
	fe->fe_cx = 0;
	switch (opf) {
	case FOP(INS2_FPop2, INSFP2_FMOV_CC(IFCC_FCC(0))):
		__fpu_ccmov(fe, type, rd, __fpu_getreg(rs2), rs2, insn,
		    FSR_GET_FCC0(fsr));
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_CC(IFCC_FCC(1))):
		__fpu_ccmov(fe, type, rd, __fpu_getreg(rs2), rs2, insn,
		    FSR_GET_FCC1(fsr));
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_CC(IFCC_FCC(2))):
		__fpu_ccmov(fe, type, rd, __fpu_getreg(rs2), rs2, insn,
		    FSR_GET_FCC2(fsr));
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_CC(IFCC_FCC(3))):
		__fpu_ccmov(fe, type, rd, __fpu_getreg(rs2), rs2, insn,
		    FSR_GET_FCC3(fsr));
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_CC(IFCC_ICC)):
		__fpu_ccmov(fe, type, rd, __fpu_getreg(rs2), rs2, insn,
		    (tstate & TSTATE_ICC_MASK) >> TSTATE_ICC_SHIFT);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_CC(IFCC_XCC)):
		__fpu_ccmov(fe, type, rd, __fpu_getreg(rs2), rs2, insn,
		    (tstate & TSTATE_XCC_MASK) >> (TSTATE_XCC_SHIFT));
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_RC(IRCOND_Z)):
		reg = fetch_reg(uf, IF_F4_RS1(insn));
		if (reg == 0)
			__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_RC(IRCOND_LEZ)):
		reg = fetch_reg(uf, IF_F4_RS1(insn));
		if (reg <= 0)
			__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_RC(IRCOND_LZ)):
		reg = fetch_reg(uf, IF_F4_RS1(insn));
		if (reg < 0)
			__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_RC(IRCOND_NZ)):
		reg = fetch_reg(uf, IF_F4_RS1(insn));
		if (reg != 0)
			__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_RC(IRCOND_GZ)):
		reg = fetch_reg(uf, IF_F4_RS1(insn));
		if (reg > 0)
			__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FMOV_RC(IRCOND_GEZ)):
		reg = fetch_reg(uf, IF_F4_RS1(insn));
		if (reg >= 0)
			__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop2, INSFP2_FCMP):
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		__fpu_compare(fe, 0, IF_F3_CC(insn));
		return (__fpu_cmpck(fe));
	case FOP(INS2_FPop2, INSFP2_FCMPE):
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		__fpu_compare(fe, 1, IF_F3_CC(insn));
		return (__fpu_cmpck(fe));
	case FOP(INS2_FPop1, INSFP1_FMOV):	/* these should all be pretty obvious */
		__fpu_mov(fe, type, rd, __fpu_getreg(rs2), rs2);
		return (0);
	case FOP(INS2_FPop1, INSFP1_FNEG):
		__fpu_mov(fe, type, rd, __fpu_getreg(rs2) ^ (1 << 31), rs2);
		return (0);
	case FOP(INS2_FPop1, INSFP1_FABS):
		__fpu_mov(fe, type, rd, __fpu_getreg(rs2) & ~(1 << 31), rs2);
		return (0);
	case FOP(INS2_FPop1, INSFP1_FSQRT):
		__fpu_explode(fe, &fe->fe_f1, type, rs2);
		fp = __fpu_sqrt(fe);
		break;
	case FOP(INS2_FPop1, INSFP1_FADD):
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		fp = __fpu_add(fe);
		break;
	case FOP(INS2_FPop1, INSFP1_FSUB):
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		fp = __fpu_sub(fe);
		break;
	case FOP(INS2_FPop1, INSFP1_FMUL):
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		fp = __fpu_mul(fe);
		break;
	case FOP(INS2_FPop1, INSFP1_FDIV):
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		fp = __fpu_div(fe);
		break;
	case FOP(INS2_FPop1, INSFP1_FsMULd):
	case FOP(INS2_FPop1, INSFP1_FdMULq):
		if (type == FTYPE_EXT)
			return (SIGILL);
		__fpu_explode(fe, &fe->fe_f1, type, rs1);
		__fpu_explode(fe, &fe->fe_f2, type, rs2);
		type++;	/* single to double, or double to quad */
		/*
		 * Recalculate rd (the old type applied for the source regs
		 * only, the target one has a different size).
		 */
		mask = opmask[type];
		rd = IF_F3_RD(insn) & ~mask;
		fp = __fpu_mul(fe);
		break;
	case FOP(INS2_FPop1, INSFP1_FxTOs):
	case FOP(INS2_FPop1, INSFP1_FxTOd):
	case FOP(INS2_FPop1, INSFP1_FxTOq):
		type = FTYPE_LNG;
		__fpu_explode(fe, fp = &fe->fe_f1, type, rs2);
		/* sneaky; depends on instruction encoding */
		type = (IF_F3_OPF(insn) >> 2) & 3;
		mask = opmask[type];
		rd = IF_F3_RD(insn) & ~mask;
		break;
	case FOP(INS2_FPop1, INSFP1_FTOx):
		__fpu_explode(fe, fp = &fe->fe_f1, type, rs2);
		type = FTYPE_LNG;
		mask = 1;	/* needs 2 registers */
		rd = IF_F3_RD(insn) & ~mask;
		break;
	case FOP(INS2_FPop1, INSFP1_FTOs):
	case FOP(INS2_FPop1, INSFP1_FTOd):
	case FOP(INS2_FPop1, INSFP1_FTOq):
	case FOP(INS2_FPop1, INSFP1_FTOi):
		__fpu_explode(fe, fp = &fe->fe_f1, type, rs2);
		/* sneaky; depends on instruction encoding */
		type = (IF_F3_OPF(insn) >> 2) & 3;
		mask = opmask[type];
		rd = IF_F3_RD(insn) & ~mask;
		break;
	default:
		return (SIGILL);
	}

	/*
	 * ALU operation is complete.  Collapse the result and then check
	 * for exceptions.  If we got any, and they are enabled, do not
	 * alter the destination register, just stop with an exception.
	 * Otherwise set new current exceptions and accrue.
	 */
	__fpu_implode(fe, fp, type, space);
	cx = fe->fe_cx;
	if (cx != 0) {
		mask = (fsr >> FSR_TEM_SHIFT) & FSR_TEM_MASK;
		if (cx & mask) {
			/* not accrued??? */
			fsr = (fsr & ~FSR_FTT_MASK) |
			    FSR_FTT(FSR_FTT_IEEE) |
			    FSR_CEXC(cx_to_trapx[(cx & mask) - 1]);
			return (SIGFPE);
		}
		fsr |= (cx << FSR_CEXC_SHIFT) | (cx << FSR_AEXC_SHIFT);
	}
	fe->fe_fsr = fsr;
	__fpu_setreg(rd, space[0]);
	if (type >= FTYPE_DBL || type == FTYPE_LNG) {
		__fpu_setreg(rd + 1, space[1]);
		if (type > FTYPE_DBL) {
			__fpu_setreg(rd + 2, space[2]);
			__fpu_setreg(rd + 3, space[3]);
		}
	}
	return (0);	/* success */
}
