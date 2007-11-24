/*
 * Copyright (c) 2001,3 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/* #include <machine/frame.h> */
#define	FRAME_V0		0
#define	FRAME_T0		1
#define	FRAME_T1		2
#define	FRAME_T2		3
#define	FRAME_T3		4
#define	FRAME_T4		5
#define	FRAME_T5		6
#define	FRAME_T6		7
#define	FRAME_T7		8
#define	FRAME_S0		9
#define	FRAME_S1		10
#define	FRAME_S2		11
#define	FRAME_S3		12
#define	FRAME_S4		13
#define	FRAME_S5		14
#define	FRAME_S6		15
#define	FRAME_A3		16
#define	FRAME_A4		17
#define	FRAME_A5		18
#define	FRAME_RA		23
#define	FRAME_T12		24
#define	FRAME_AT		25
#define	FRAME_SP		26
#define FRAME_TRAPARG_A0	28
#define FRAME_TRAPARG_A1	29
#define FRAME_TRAPARG_A2	30
#define	FRAME_PC		(FRAME_TRAPARG_A2 + 1 + 1)

/* #include <machine/reg.h> */
#define	R_V0	0
#define	R_T0	1
#define	R_T1	2
#define	R_T2	3
#define	R_T3	4
#define	R_T4	5
#define	R_T5	6
#define	R_T6	7
#define	R_T7	8
#define	R_S0	9
#define	R_S1	10
#define	R_S2	11
#define	R_S3	12
#define	R_S4	13
#define	R_S5	14
#define	R_S6	15
#define	R_A0	16
#define	R_A1	17
#define	R_A2	18
#define	R_A3	19
#define	R_A4	20
#define	R_A5	21
#define	R_T8	22
#define	R_T9	23
#define	R_T10	24
#define	R_T11	25
#define	R_RA	26
#define	R_T12	27
#define	R_SP	30
#define	R_ZERO	31

/*
 * XXX - The rev id's are defined in <machine/ucontext.h>
 */
#define	MC_FMT_OFFSET	73*8	/* offset to format from mcontext */
#define	REV0_SIGFRAME	0x0001	/* rev R0 sigcontext format */
#define	REV0_TRAPFRAME	0x0002	/* rev R0 trapframe format */

/*
 * int _alpha_restore_context(const mcontext_t *mcp,
 *          intptr_t val, intptr_t *loc);
 *
 *   The format of the context is verified at the beginning.
 *   Returns -1 if invalid format.
 */
	.set	noreorder
LEAF(_alpha_restore_context, 3)
	LDGP(pv)
	bne	a0, Lsc1			/* argument null? */
Lscbad:	ldiq	v0, -1				/* return -1 */
	br	Lscend
Lsc1:	ldq	t1, MC_FMT_OFFSET(a0)		/* is mcontext valid format? */
	ldiq	t0, REV0_TRAPFRAME
	cmpeq	t0, t1, t0			/* is it trapframe format? */
	bne	t0, Lsc_fp			/* if so, check fp state */
	ldiq	t0, REV0_SIGFRAME
	cmpeq	t0, t1, t0			/* is it sigcontext format? */
	beq	t0, Lscbad
	/* supposedly sigcontext format, check magic number */
	ldiq	t0, 0xACEDBADE			/* check magic number */
	ldq	t1, ((R_ZERO + 1) * 8)(a0)	/* magic in mc_regs[R_ZERO] */
	cmpeq	t0, t1, t0
	beq	t0, Lscbad
	/* restore floating point regs first */
Lsc_fp:	ldq	t0, ((71 + 1) * 8)(a0)		/* if FP regs not saved, */
	beq	t0, Lsc2			/*   skip setting FP regs */
	ldt	$f0,  ((37 + 1) * 8)(a0)	/* restore FP regs using */
	ldt	$f1,  ((38 + 1) * 8)(a0)	/*   hw name */
	ldt	$f2,  ((39 + 1) * 8)(a0)
	ldt	$f3,  ((40 + 1) * 8)(a0)
	ldt	$f4,  ((41 + 1) * 8)(a0)
	ldt	$f5,  ((42 + 1) * 8)(a0)
	ldt	$f6,  ((43 + 1) * 8)(a0)
	ldt	$f7,  ((44 + 1) * 8)(a0)
	ldt	$f8,  ((45 + 1) * 8)(a0)
	ldt	$f9,  ((46 + 1) * 8)(a0)
	ldt	$f10, ((47 + 1) * 8)(a0)
	ldt	$f11, ((48 + 1) * 8)(a0)
	ldt	$f12, ((49 + 1) * 8)(a0)
	ldt	$f13, ((50 + 1) * 8)(a0)
	ldt	$f14, ((51 + 1) * 8)(a0)
	ldt	$f15, ((52 + 1) * 8)(a0)
	ldt	$f16, ((53 + 1) * 8)(a0)
	ldt	$f17, ((54 + 1) * 8)(a0)
	ldt	$f18, ((55 + 1) * 8)(a0)
	ldt	$f19, ((56 + 1) * 8)(a0)
	ldt	$f20, ((57 + 1) * 8)(a0)
	ldt	$f21, ((58 + 1) * 8)(a0)
	ldt	$f22, ((59 + 1) * 8)(a0)
	ldt	$f23, ((60 + 1) * 8)(a0)
	ldt	$f24, ((61 + 1) * 8)(a0)
	ldt	$f25, ((62 + 1) * 8)(a0)
	ldt	$f26, ((63 + 1) * 8)(a0)
	ldt	$f27, ((64 + 1) * 8)(a0)
	.set noat
	ldt	$f28, ((65 + 1) * 8)(a0)
	.set at
	ldt	$f29, ((66 + 1) * 8)(a0)
	ldt	$f30, ((67 + 1) * 8)(a0)
	/* $f31 is hardwired zero */
	ldt	ft0, ((69 + 1) * 8)(a0)		/* restore FP control reg */
	mt_fpcr ft0
Lsc2:	ldiq	t0, REV0_SIGFRAME		/* check the context format */
	ldq	t1, MC_FMT_OFFSET(a0)		/*   again. */
	cmpeq	t0, t1, t0			/* is it sigcontext format? */
	bne	t0, Lsc_sc
	/* trapframe format */
	ldq	v0, ((FRAME_V0 + 1) * 8)(a0)	/* restore v0 */
	ldq	t0, ((FRAME_T0 + 1) * 8)(a0)	/* restore t0-t7 */
	ldq	t1, ((FRAME_T1 + 1) * 8)(a0)
	ldq	t2, ((FRAME_T2 + 1) * 8)(a0)
	ldq	t3, ((FRAME_T3 + 1) * 8)(a0)
	ldq	t4, ((FRAME_T4 + 1) * 8)(a0)
	ldq	t5, ((FRAME_T5 + 1) * 8)(a0)
	ldq	t6, ((FRAME_T6 + 1) * 8)(a0)
	ldq	t7, ((FRAME_T7 + 1) * 8)(a0)
	ldq	s0, ((FRAME_S0 + 1) * 8)(a0)	/* restore s0-s6 */
	ldq	s1, ((FRAME_S1 + 1) * 8)(a0)
	ldq	s2, ((FRAME_S2 + 1) * 8)(a0)
	ldq	s3, ((FRAME_S3 + 1) * 8)(a0)
	ldq	s4, ((FRAME_S4 + 1) * 8)(a0)
	ldq	s5, ((FRAME_S5 + 1) * 8)(a0)
	ldq	s6, ((FRAME_S6 + 1) * 8)(a0)
	ldq	a4, ((FRAME_A4 + 1) * 8)(a0)	/* restore a4, a5 */
	ldq	a5, ((FRAME_A5 + 1) * 8)(a0)
	ldq	ra, ((FRAME_RA + 1) * 8)(a0)
	ldq	sp, ((FRAME_SP + 1) * 8)(a0)
	subq	sp, 16, sp			/* save room on stack */
	ldq	a3, ((FRAME_TRAPARG_A1 + 1) * 8)(a0)
	stq	a3, 0(a0)			/* save a1 on stack */
	ldq	a3, ((FRAME_TRAPARG_A2 + 1) * 8)(a0)
	stq	a3, 8(a0)			/* save a2 on stack */
	.set noat
	ldq	at_reg, ((FRAME_PC + 1) * 8)(a0) /* PC at time of trap? */
	.set at
	ldq	a3, ((FRAME_A3 + 1) * 8)(a0)	/* restore a3 */
	ldq	a0, ((FRAME_TRAPARG_A0 + 1) * 8)(a0)	/* restore a0 */
	br	Lsc3
Lsc_sc:	/* sigcontext format */
	ldq	v0, ((R_V0 + 1) * 8)(a0)	/* restore v0 */
	ldq	t0, ((R_T0 + 1) * 8)(a0)	/* restore t0-t7 */
	ldq	t1, ((R_T1 + 1) * 8)(a0)
	ldq	t2, ((R_T2 + 1) * 8)(a0)
	ldq	t3, ((R_T3 + 1) * 8)(a0)
	ldq	t4, ((R_T4 + 1) * 8)(a0)
	ldq	t5, ((R_T5 + 1) * 8)(a0)
	ldq	t6, ((R_T6 + 1) * 8)(a0)
	ldq	t7, ((R_T7 + 1) * 8)(a0)
	ldq	s0, ((R_S0 + 1) * 8)(a0)	/* restore s0-s6 */
	ldq	s1, ((R_S1 + 1) * 8)(a0)
	ldq	s2, ((R_S2 + 1) * 8)(a0)
	ldq	s3, ((R_S3 + 1) * 8)(a0)
	ldq	s4, ((R_S4 + 1) * 8)(a0)
	ldq	s5, ((R_S5 + 1) * 8)(a0)
	ldq	s6, ((R_S6 + 1) * 8)(a0)
	ldq	a4, ((R_A4 + 1) * 8)(a0)	/* restore a4, a5 */
	ldq	a5, ((R_A5 + 1) * 8)(a0)
	ldq	ra, ((R_RA + 1) * 8)(a0)
	ldq	sp, ((R_SP + 1) * 8)(a0)
	subq	sp, 16, sp			/* save room on stack */
	ldq	a3, ((R_A1 + 1) * 8)(a0)	/* get a1 */
	stq	a3, 0(a0)			/* save a1 on stack */
	ldq	a3, ((R_A2 + 1) * 8)(a0)	/* get a2 */
	stq	a3, 8(a0)			/* save a2 on stack */
	ldq	a3, ((R_A3 + 1) * 8)(a0)	/* restore a3 */
	ldq	a0, ((R_A0 + 1) * 8)(a0)	/* restore a0 */
Lsc3:	beq	a2, Lsc4
	stq	a1, 0(a2)
Lsc4:	ldq	a1, 0(sp)		/* restore a1, a2 */
	ldq	a2, 8(sp)
	addq	sp, 16, sp		/* restore stack */
Lscend:	RET
END(_alpha_restore_context)


/*
 * int _alpha_save_context(mcontext_t *);
 *
 *   Always save in trapframe format.  Floating point registers are
 *   saved but may be optimized away later (see comments below).
 */
LEAF(_alpha_save_context, 1)
	LDGP(pv)
	bne	a0, Lgc1			/* argument null? */
	ldiq	v0, -1				/* return -1 */
	br	Lgcend
Lgc1:	ldiq	v0, 1				/* save_context returns 1, */
	stq	v0, ((FRAME_V0 + 1) * 8)(a0)	/*   so save 1 in v0 */
	stq	t0, ((FRAME_T0 + 1) * 8)(a0)	/* save t0-t7 */
	stq	t1, ((FRAME_T1 + 1) * 8)(a0)
	stq	t2, ((FRAME_T2 + 1) * 8)(a0)
	stq	t3, ((FRAME_T3 + 1) * 8)(a0)
	stq	t4, ((FRAME_T4 + 1) * 8)(a0)
	stq	t5, ((FRAME_T5 + 1) * 8)(a0)
	stq	t6, ((FRAME_T6 + 1) * 8)(a0)
	stq	t7, ((FRAME_T7 + 1) * 8)(a0)
	stq	s0, ((FRAME_S0 + 1) * 8)(a0)	/* save s0-s6 */
	stq	s1, ((FRAME_S1 + 1) * 8)(a0)
	stq	s2, ((FRAME_S2 + 1) * 8)(a0)
	stq	s3, ((FRAME_S3 + 1) * 8)(a0)
	stq	s4, ((FRAME_S4 + 1) * 8)(a0)
	stq	s5, ((FRAME_S5 + 1) * 8)(a0)
	stq	s6, ((FRAME_S6 + 1) * 8)(a0)
	stq	a0, ((FRAME_TRAPARG_A0 + 1) * 8)(a0)	/* save a0-a5 */
	stq	a1, ((FRAME_TRAPARG_A1 + 1) * 8)(a0)
	stq	a2, ((FRAME_TRAPARG_A2 + 1) * 8)(a0)
	stq	a3, ((FRAME_A3 + 1) * 8)(a0)
	stq	a4, ((FRAME_A4 + 1) * 8)(a0)
	stq	a5, ((FRAME_A5 + 1) * 8)(a0)
	stq	ra, ((FRAME_RA + 1) * 8)(a0)
	stq	sp, ((FRAME_SP + 1) * 8)(a0)
	ldiq	t0, REV0_TRAPFRAME		/* store trapframe format in */
	stq	t0, MC_FMT_OFFSET(a0)		/*   ucp->uc-rev */
	/*
	 * XXX - Do we really need to save floating point registers?
	 *
	 * This is an explicit call to get the current context, so
	 * shouldn't the caller be done with the floating point registers?
	 * Contexts formed by involuntary switches, such as signal delivery,
	 * should have floating point registers saved by the kernel.
	 */
#if 1
	stq	zero, ((71 + 1) * 8)(a0)	/* FP regs are not saved */
#else
	ldiq	t0, 1				/* say we've used FP,  */
	stq	t0,   ((71 + 1) * 8)(a0)	/*   mc_ownedfp = 1  */
	stt	$f0,  ((37 + 1) * 8)(a0)	/* save first register, using */
	stt	$f1,  ((38 + 1) * 8)(a0)	/*   hw name etc. */
	stt	$f2,  ((39 + 1) * 8)(a0)
	stt	$f3,  ((40 + 1) * 8)(a0)
	stt	$f4,  ((41 + 1) * 8)(a0)
	stt	$f5,  ((42 + 1) * 8)(a0)
	stt	$f6,  ((43 + 1) * 8)(a0)
	stt	$f7,  ((44 + 1) * 8)(a0)
	stt	$f8,  ((45 + 1) * 8)(a0)
	stt	$f9,  ((46 + 1) * 8)(a0)
	stt	$f10, ((47 + 1) * 8)(a0)
	stt	$f11, ((48 + 1) * 8)(a0)
	stt	$f12, ((49 + 1) * 8)(a0)
	stt	$f13, ((50 + 1) * 8)(a0)
	stt	$f14, ((51 + 1) * 8)(a0)
	stt	$f15, ((52 + 1) * 8)(a0)
	stt	$f16, ((53 + 1) * 8)(a0)
	stt	$f17, ((54 + 1) * 8)(a0)
	stt	$f18, ((55 + 1) * 8)(a0)
	stt	$f19, ((56 + 1) * 8)(a0)
	stt	$f20, ((57 + 1) * 8)(a0)
	stt	$f21, ((58 + 1) * 8)(a0)
	stt	$f22, ((59 + 1) * 8)(a0)
	stt	$f23, ((60 + 1) * 8)(a0)
	stt	$f24, ((61 + 1) * 8)(a0)
	stt	$f25, ((62 + 1) * 8)(a0)
	stt	$f26, ((63 + 1) * 8)(a0)
	stt	$f27, ((64 + 1) * 8)(a0)
	.set noat
	stt	$f28, ((65 + 1) * 8)(a0)
	.set at
	stt	$f29, ((66 + 1) * 8)(a0)
	stt	$f30, ((67 + 1) * 8)(a0)
	/* $f31 is hardwired zero */
#endif
	mf_fpcr ft0				/* get FP control reg */
	stt	ft0, ((69 + 1) * 8)(a0)		/* and store it in mc_fpcr */
	stq	zero, ((70 + 1) * 8)(a0)	/* FP software control XXX */
	mov	zero, v0			/* return zero */
Lgcend:	RET
END(_alpha_save_context)
