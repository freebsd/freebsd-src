/*
 * Copyright (c) 2002, 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_REGSET_H_
#define	_MACHINE_REGSET_H_

/*
 * Create register sets, based on the runtime specification. This allows
 * us to better reuse code and to copy sets around more efficiently.
 * Contexts are defined in terms of these sets. These include trapframe,
 * sigframe, pcb, mcontext, reg and fpreg. Other candidates are unwind
 * and coredump related contexts.
 *
 * Notes:
 * o  Constant registers (r0, f0 and f1) are not accounted for,
 * o  The stacked registers (r32-r127) are not accounted for,
 * o  Predicates are not split across sets.
 */

/* A single FP register. */
struct _ia64_fpreg {
	unsigned char	fpr_bits[16];
} __aligned(16);

/*
 * Special registers.
 */
struct _special {
	unsigned long		sp;
	unsigned long		unat;		/* NaT before spilling */
	unsigned long		rp;
	unsigned long		pr;
	unsigned long		pfs;
	unsigned long		bspstore;
	unsigned long		rnat;
	unsigned long		__spare;
	/* Userland context and syscalls */
	unsigned long		tp;
	unsigned long		rsc;
	unsigned long		fpsr;
	unsigned long		psr;
	/* ASYNC: Interrupt specific */
	unsigned long		gp;
	unsigned long		ndirty;
	unsigned long		cfm;
	unsigned long		iip;
	unsigned long		ifa;
	unsigned long		isr;
};

struct _high_fp {
	struct _ia64_fpreg	fr[96];		/* High FP register set. */
/* Can't be bothered to name them seperately. They are fr32-fr127. */
};

/*
 * Preserved registers.
 */
struct _callee_saved {
	unsigned long		unat;		/* NaT after spilling. */
	unsigned long		gr4;
	unsigned long		gr5;
	unsigned long		gr6;
	unsigned long		gr7;
	unsigned long		br1;
	unsigned long		br2;
	unsigned long		br3;
	unsigned long		br4;
	unsigned long		br5;
	unsigned long		lc;
	unsigned long		__spare;
};

struct _callee_saved_fp {
	struct _ia64_fpreg	fr2;
	struct _ia64_fpreg	fr3;
	struct _ia64_fpreg	fr4;
	struct _ia64_fpreg	fr5;
	struct _ia64_fpreg	fr16;
	struct _ia64_fpreg	fr17;
	struct _ia64_fpreg	fr18;
	struct _ia64_fpreg	fr19;
	struct _ia64_fpreg	fr20;
	struct _ia64_fpreg	fr21;
	struct _ia64_fpreg	fr22;
	struct _ia64_fpreg	fr23;
	struct _ia64_fpreg	fr24;
	struct _ia64_fpreg	fr25;
	struct _ia64_fpreg	fr26;
	struct _ia64_fpreg	fr27;
	struct _ia64_fpreg	fr28;
	struct _ia64_fpreg	fr29;
	struct _ia64_fpreg	fr30;
	struct _ia64_fpreg	fr31;
};

/*
 * Scratch registers.
 */
struct _caller_saved {
	unsigned long		unat;		/* NaT after spilling. */
	unsigned long		gr2;
	unsigned long		gr3;
	unsigned long		gr8;
	unsigned long		gr9;
	unsigned long		gr10;
	unsigned long		gr11;
	unsigned long		gr14;
	unsigned long		gr15;
	unsigned long		gr16;
	unsigned long		gr17;
	unsigned long		gr18;
	unsigned long		gr19;
	unsigned long		gr20;
	unsigned long		gr21;
	unsigned long		gr22;
	unsigned long		gr23;
	unsigned long		gr24;
	unsigned long		gr25;
	unsigned long		gr26;
	unsigned long		gr27;
	unsigned long		gr28;
	unsigned long		gr29;
	unsigned long		gr30;
	unsigned long		gr31;
	unsigned long		br6;
	unsigned long		br7;
	unsigned long		ccv;
	unsigned long		csd;
	unsigned long		ssd;
};

struct _caller_saved_fp {
	struct _ia64_fpreg	fr6;
	struct _ia64_fpreg	fr7;
	struct _ia64_fpreg	fr8;
	struct _ia64_fpreg	fr9;
	struct _ia64_fpreg	fr10;
	struct _ia64_fpreg	fr11;
	struct _ia64_fpreg	fr12;
	struct _ia64_fpreg	fr13;
	struct _ia64_fpreg	fr14;
	struct _ia64_fpreg	fr15;
};

#ifdef _KERNEL
void	restore_callee_saved(struct _callee_saved *);
void	restore_callee_saved_fp(struct _callee_saved_fp *);
void	restore_high_fp(struct _high_fp *);
void	save_callee_saved(struct _callee_saved *);
void	save_callee_saved_fp(struct _callee_saved_fp *);
void	save_high_fp(struct _high_fp *);
#endif

#endif	/* _MACHINE_REGSET_H_ */
