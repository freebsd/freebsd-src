/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/emul.h>
#include <machine/frame.h>
#include <machine/instr.h>

/*
 * Alpha-compatible sysctls to control the alignment fixup.
 */
static int unaligned_print = 0;		/* warn about unaligned accesses */
static int unaligned_fix = 1;		/* fix up unaligned accesses */
static int unaligned_sigbus = 0;	/* don't SIGBUS on fixed-up accesses */

SYSCTL_INT(_machdep, OID_AUTO, unaligned_print, CTLFLAG_RW,
    &unaligned_print, 0, "");

SYSCTL_INT(_machdep, OID_AUTO, unaligned_fix, CTLFLAG_RW,
    &unaligned_fix, 0, "");

SYSCTL_INT(_machdep, OID_AUTO, unaligned_sigbus, CTLFLAG_RW,
    &unaligned_sigbus, 0, "");

int
emul_fetch_reg(struct trapframe *tf, int reg, u_long *val)
{
	u_long offs;

	CTR1(KTR_TRAP, "emul_fetch_reg: register %d", reg);
	if (reg == IREG_G0)
		*val = 0;
	else if (reg < IREG_O0)	/* global */
		*val = tf->tf_global[reg];
	else if (reg < IREG_L0)	/* out */
		*val = tf->tf_out[reg - IREG_O0];
	else {			/* local, in */
		/*
		 * The in registers are immediately after the locals in
		 * the frame.
		 */
		offs = offsetof(struct frame, fr_local[reg - IREG_L0]);
		return (copyin((void *)(tf->tf_sp + SPOFF + offs), val,
		    sizeof(*val)));
	}
	return (0);
}

int
emul_store_reg(struct trapframe *tf, int reg, u_long val)
{
	u_long offs;

	CTR1(KTR_TRAP, "emul_store_reg: register %d", reg);
	if (reg == IREG_G0)
		return (0);
	if (reg < IREG_O0)	/* global */
		tf->tf_global[reg] = val;
	else if (reg < IREG_L0)	/* out */
		tf->tf_out[reg - IREG_O0] = val;
	else {			/* local, in */
		/*
		 * The in registers are immediately after the locals in
		 * the frame.
		 */
		offs = offsetof(struct frame, fr_local[reg - IREG_L0]);
		return (copyout(&val, (void *)(tf->tf_sp + SPOFF + offs),
		    sizeof(val)));
	}
	return (0);
}

/* Retrieve rs2 or use the immediate value from the instruction */
static int
f3_op2(struct trapframe *tf, u_int insn, u_long *op2)
{
	int error;

	if (IF_F3_I(insn) != 0)
		*op2 = IF_SIMM(insn, 13);
	else {
		if ((error = emul_fetch_reg(tf, IF_F3_RS2(insn), op2)) != 0)
			return (error);
	}
	return (0);
}

/*
 * XXX: should the addr from the sfar be used instead of decoding the
 * instruction?
 */
static int
f3_memop_addr(struct trapframe *tf, u_int insn, u_long *addr)
{
	u_long addr1;
	int error;

	if ((error = f3_op2(tf, insn, &addr1)) != 0)
		return (error);
	CTR2(KTR_TRAP, "f3_memop_addr: addr1: %#lx (imm %d)", addr1, IF_F3_I(insn));
	error = emul_fetch_reg(tf, IF_F3_RS1(insn), addr);
	*addr += addr1;
	return (error);
}

static int
fixup_st(struct trapframe *tf, u_int insn, int size)
{
	u_long addr, reg;
	int error;

	if ((error = f3_memop_addr(tf, insn, &addr)) != 0)
		return (error);
	if ((error = emul_fetch_reg(tf, IF_F3_RD(insn), &reg)) != 0)
		return (error);
	reg <<= 8 * (8 - size);
	CTR1(KTR_TRAP, "fixup_st: writing to %#lx", addr);
	return (copyout(&reg, (void *)addr, size));
}

static int
fixup_ld(struct trapframe *tf, u_int insn, int size, int sign)
{
	u_long addr, reg;
	int error;

	if ((error = f3_memop_addr(tf, insn, &addr)) != 0)
		return (error);
	reg = 0;
	CTR1(KTR_TRAP, "fixup_ld: reading from %#lx", addr);
	if ((error = copyin((void *)addr, &reg, size)) != 0)
		return (error);
	reg >>= 8 * (8 - size);
	if (sign && size < 8) {
		/* Need to sign-extend. */
		reg = IF_SEXT(reg, size * 8);
	}
	return (emul_store_reg(tf, IF_F3_RD(insn), reg));
}

/*
 * NOTE: fixed up loads and stores are not atomical any more (in some cases,
 * they could be made, though, but that is not implemented yet). This means
 * that in some sorts of programs, this emulation could cause bugs.
 * XXX: this is still very incomplete!
 */
int
unaligned_fixup(struct thread *td, struct trapframe *tf)
{
	struct proc *p;
	u_int insn;
	int fixed, error;

	p = td->td_proc;

	if (rwindow_save(td) != 0) {
		/*
		 * The process will need to be killed without sending a
		 * signal; let the signal code do that.
		 */
		return (SIGBUS);
	}
	if (copyin((void *)tf->tf_tpc, &insn, sizeof(insn)) != 0)
		return (SIGBUS);

	CTR1(KTR_TRAP, "unaligned_fixup: insn %x", insn);
	fixed = 0;
	if (unaligned_fix) {
		error = 0;
		if (IF_OP(insn) == IOP_LDST) {
			fixed = 1;
			switch (IF_F3_OP3(insn)) {
			case INS3_LDUH:
				error = fixup_ld(tf, insn, 2, 0);
				break;
			case INS3_LDUW:
				error = fixup_ld(tf, insn, 4, 0);
				break;
			case INS3_LDX:
				error = fixup_ld(tf, insn, 8, 0);
				break;
			case INS3_LDSH:
				error = fixup_ld(tf, insn, 2, 1);
				break;
			case INS3_LDSW:
				error = fixup_ld(tf, insn, 4, 1);
				break;
			case INS3_STH:
				error = fixup_st(tf, insn, 2);
				break;
			case INS3_STW:
				error = fixup_st(tf, insn, 4);
				break;
			case INS3_STX:
				error = fixup_st(tf, insn, 8);
				break;
			default:
				fixed = 0;
			}
		}
		if (error != 0)
			return (SIGBUS);
	}

	CTR5(KTR_TRAP, "unaligned_fixup: pid %d, va=%#lx pc=%#lx "
	    "npc=%#lx, fixed=%d", p->p_pid, tf->tf_sfar, tf->tf_tpc,
	    tf->tf_tnpc, fixed);
	if (unaligned_print || !fixed) {
		uprintf("pid %d (%s): unaligned access: va=%#lx pc=%#lx "
		    "npc=%#lx %s\n", p->p_pid, p->p_comm, tf->tf_sfar,
		    tf->tf_tpc, tf->tf_tnpc, fixed ? "(fixed)" : "(unfixable)");
	}
	return (fixed && !unaligned_sigbus ? 0 : SIGBUS);
}

static int
emul_popc(struct trapframe *tf, u_int insn)
{
	u_long reg, res;
	int i;

	if (IF_F3_RS1(insn) != 0)
		return (SIGILL);
	if (f3_op2(tf, insn, &reg) != 0)
		return (SIGBUS);
	res = 0;
	for (i = 0; i < 64; i++)
		res += (reg >> i) & 1;
	if (emul_store_reg(tf, IF_F3_RD(insn), res) != 0)
		return (SIGBUS);
	return (0);
}

/*
 * Emulate unimplemented instructions, if applicable.
 * Only handles popc right now.
 */
int
emul_insn(struct thread *td, struct trapframe *tf)
{
	u_int insn;

	if (rwindow_save(td) != 0) {
		/*
		 * The process will need to be killed without sending a
		 * signal; let the signal code do that.
		 */
		return (SIGBUS);
	}
	if (copyin((void *)tf->tf_tpc, &insn, sizeof(insn)) != 0)
		return (SIGBUS);

	CTR1(KTR_TRAP, "emulate_insn: insn %x", insn);
	switch (IF_OP(insn)) {
	case IOP_MISC:
		switch (IF_F3_OP3(insn)) {
		case INS2_POPC:
			return (emul_popc(tf, insn));
		}
		break;
	}
	return (SIGILL);
}
