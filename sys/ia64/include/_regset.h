/*-
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
union _ia64_fpreg {
	unsigned char	fpr_bits[16];
	long double	fpr_flt;
};

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
	union _ia64_fpreg	fr32;
	union _ia64_fpreg	fr33;
	union _ia64_fpreg	fr34;
	union _ia64_fpreg	fr35;
	union _ia64_fpreg	fr36;
	union _ia64_fpreg	fr37;
	union _ia64_fpreg	fr38;
	union _ia64_fpreg	fr39;
	union _ia64_fpreg	fr40;
	union _ia64_fpreg	fr41;
	union _ia64_fpreg	fr42;
	union _ia64_fpreg	fr43;
	union _ia64_fpreg	fr44;
	union _ia64_fpreg	fr45;
	union _ia64_fpreg	fr46;
	union _ia64_fpreg	fr47;
	union _ia64_fpreg	fr48;
	union _ia64_fpreg	fr49;
	union _ia64_fpreg	fr50;
	union _ia64_fpreg	fr51;
	union _ia64_fpreg	fr52;
	union _ia64_fpreg	fr53;
	union _ia64_fpreg	fr54;
	union _ia64_fpreg	fr55;
	union _ia64_fpreg	fr56;
	union _ia64_fpreg	fr57;
	union _ia64_fpreg	fr58;
	union _ia64_fpreg	fr59;
	union _ia64_fpreg	fr60;
	union _ia64_fpreg	fr61;
	union _ia64_fpreg	fr62;
	union _ia64_fpreg	fr63;
	union _ia64_fpreg	fr64;
	union _ia64_fpreg	fr65;
	union _ia64_fpreg	fr66;
	union _ia64_fpreg	fr67;
	union _ia64_fpreg	fr68;
	union _ia64_fpreg	fr69;
	union _ia64_fpreg	fr70;
	union _ia64_fpreg	fr71;
	union _ia64_fpreg	fr72;
	union _ia64_fpreg	fr73;
	union _ia64_fpreg	fr74;
	union _ia64_fpreg	fr75;
	union _ia64_fpreg	fr76;
	union _ia64_fpreg	fr77;
	union _ia64_fpreg	fr78;
	union _ia64_fpreg	fr79;
	union _ia64_fpreg	fr80;
	union _ia64_fpreg	fr81;
	union _ia64_fpreg	fr82;
	union _ia64_fpreg	fr83;
	union _ia64_fpreg	fr84;
	union _ia64_fpreg	fr85;
	union _ia64_fpreg	fr86;
	union _ia64_fpreg	fr87;
	union _ia64_fpreg	fr88;
	union _ia64_fpreg	fr89;
	union _ia64_fpreg	fr90;
	union _ia64_fpreg	fr91;
	union _ia64_fpreg	fr92;
	union _ia64_fpreg	fr93;
	union _ia64_fpreg	fr94;
	union _ia64_fpreg	fr95;
	union _ia64_fpreg	fr96;
	union _ia64_fpreg	fr97;
	union _ia64_fpreg	fr98;
	union _ia64_fpreg	fr99;
	union _ia64_fpreg	fr100;
	union _ia64_fpreg	fr101;
	union _ia64_fpreg	fr102;
	union _ia64_fpreg	fr103;
	union _ia64_fpreg	fr104;
	union _ia64_fpreg	fr105;
	union _ia64_fpreg	fr106;
	union _ia64_fpreg	fr107;
	union _ia64_fpreg	fr108;
	union _ia64_fpreg	fr109;
	union _ia64_fpreg	fr110;
	union _ia64_fpreg	fr111;
	union _ia64_fpreg	fr112;
	union _ia64_fpreg	fr113;
	union _ia64_fpreg	fr114;
	union _ia64_fpreg	fr115;
	union _ia64_fpreg	fr116;
	union _ia64_fpreg	fr117;
	union _ia64_fpreg	fr118;
	union _ia64_fpreg	fr119;
	union _ia64_fpreg	fr120;
	union _ia64_fpreg	fr121;
	union _ia64_fpreg	fr122;
	union _ia64_fpreg	fr123;
	union _ia64_fpreg	fr124;
	union _ia64_fpreg	fr125;
	union _ia64_fpreg	fr126;
	union _ia64_fpreg	fr127;
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
	union _ia64_fpreg	fr2;
	union _ia64_fpreg	fr3;
	union _ia64_fpreg	fr4;
	union _ia64_fpreg	fr5;
	union _ia64_fpreg	fr16;
	union _ia64_fpreg	fr17;
	union _ia64_fpreg	fr18;
	union _ia64_fpreg	fr19;
	union _ia64_fpreg	fr20;
	union _ia64_fpreg	fr21;
	union _ia64_fpreg	fr22;
	union _ia64_fpreg	fr23;
	union _ia64_fpreg	fr24;
	union _ia64_fpreg	fr25;
	union _ia64_fpreg	fr26;
	union _ia64_fpreg	fr27;
	union _ia64_fpreg	fr28;
	union _ia64_fpreg	fr29;
	union _ia64_fpreg	fr30;
	union _ia64_fpreg	fr31;
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
	union _ia64_fpreg	fr6;
	union _ia64_fpreg	fr7;
	union _ia64_fpreg	fr8;
	union _ia64_fpreg	fr9;
	union _ia64_fpreg	fr10;
	union _ia64_fpreg	fr11;
	union _ia64_fpreg	fr12;
	union _ia64_fpreg	fr13;
	union _ia64_fpreg	fr14;
	union _ia64_fpreg	fr15;
};

#ifdef _KERNEL
void	restore_callee_saved(const struct _callee_saved *);
void	restore_callee_saved_fp(const struct _callee_saved_fp *);
void	restore_high_fp(const struct _high_fp *);
void	save_callee_saved(struct _callee_saved *);
void	save_callee_saved_fp(struct _callee_saved_fp *);
void	save_high_fp(struct _high_fp *);
#endif

#endif	/* _MACHINE_REGSET_H_ */
