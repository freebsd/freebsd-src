/* $FreeBSD$ */
/* From: NetBSD: profile.h,v 1.9 1997/04/06 08:47:37 cgd Exp */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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
 */

#define	_MCOUNT_DECL	void mcount

#define FUNCTION_ALIGNMENT 32

typedef u_long	fptrdiff_t;

#if 0
/*
 * XXX The definition of MCOUNT below is really the following code, run
 * XXX through cpp, since the inline assembly isn't preprocessed.
 */
#define	OFFSET_AT	0
#define OFFSET_V0	8
#define OFFSET_T0	16
#define OFFSET_T1	24
#define OFFSET_T2	32
#define OFFSET_T3	40
#define OFFSET_T4	48
#define OFFSET_T5	56
#define OFFSET_T6	64
#define OFFSET_T7	72
#define OFFSET_S6	80
#define OFFSET_A0	88
#define OFFSET_A1	96
#define OFFSET_A2	104
#define OFFSET_A3	112
#define OFFSET_A4	120
#define OFFSET_A5	128
#define OFFSET_T8	136
#define OFFSET_T9	144
#define OFFSET_T10	152
#define OFFSET_T11	160
#define OFFSET_RA	168
#define OFFSET_T12	176
#define OFFSET_GP	184
#define	FRAME_SIZE	192

LEAF(_mcount,0)			/* XXX */
	.set noat
	.set noreorder

	lda	sp, -FRAME_SIZE(sp)

	stq	at_reg, OFFSET_AT(sp)
	stq	v0, OFFSET_V0(sp)
	stq	t0, OFFSET_T0(sp)
	stq	t1, OFFSET_T1(sp)
	stq	t2, OFFSET_T2(sp)
	stq	t3, OFFSET_T3(sp)
	stq	t4, OFFSET_T4(sp)
	stq	t5, OFFSET_T5(sp)
	stq	t6, OFFSET_T6(sp)
	stq	t7, OFFSET_T7(sp)
	stq	s6, OFFSET_S6(sp)	/* XXX because run _after_ prologue. */
	stq	a0, OFFSET_A0(sp)
	stq	a1, OFFSET_A1(sp)
	stq	a2, OFFSET_A2(sp)
	stq	a3, OFFSET_A3(sp)
	stq	a4, OFFSET_A4(sp)
	stq	a5, OFFSET_A5(sp)
	stq	t8, OFFSET_T8(sp)
	stq	t9, OFFSET_T9(sp)
	stq	t10, OFFSET_T10(sp)
	stq	t11, OFFSET_T11(sp)
	stq	ra, OFFSET_RA(sp)
	stq	t12, OFFSET_T12(sp)
	stq	gp, OFFSET_GP(sp)

	br	pv, LX99
LX99:	SETGP(pv)
	mov	ra, a0
	mov	at_reg, a1
	CALL(mcount)

	ldq	v0, OFFSET_V0(sp)
	ldq	t0, OFFSET_T0(sp)
	ldq	t1, OFFSET_T1(sp)
	ldq	t2, OFFSET_T2(sp)
	ldq	t3, OFFSET_T3(sp)
	ldq	t4, OFFSET_T4(sp)
	ldq	t5, OFFSET_T5(sp)
	ldq	t6, OFFSET_T6(sp)
	ldq	t7, OFFSET_T7(sp)
	ldq	s6, OFFSET_S6(sp)	/* XXX because run _after_ prologue. */
	ldq	a0, OFFSET_A0(sp)
	ldq	a1, OFFSET_A1(sp)
	ldq	a2, OFFSET_A2(sp)
	ldq	a3, OFFSET_A3(sp)
	ldq	a4, OFFSET_A4(sp)
	ldq	a5, OFFSET_A5(sp)
	ldq	t8, OFFSET_T8(sp)
	ldq	t9, OFFSET_T9(sp)
	ldq	t10, OFFSET_T10(sp)
	ldq	t11, OFFSET_T11(sp)
	ldq	ra, OFFSET_RA(sp)
	stq	t12, OFFSET_T12(sp)
	ldq	gp, OFFSET_GP(sp)

	ldq	at_reg, OFFSET_AT(sp)

	lda	sp, FRAME_SIZE(sp)
	ret	zero, (at_reg), 1

	END(_mcount)
#endif /* 0 */

#define MCOUNT __asm ("		\
	.globl	_mcount;	\
	.proc	_mcount;	\
_mcount:;			\
				\
	.end	_mcount");

#ifdef _KERNEL
/*
 * The following two macros do splhigh and splx respectively.
 * _alpha_pal_swpipl is a special version of alpha_pal_swpipl which
 * doesn't include profiling support.
 *
 * XXX These macros should probably use inline assembly.
 */
#define MCOUNT_ENTER(s) \
	s = _alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH)
#define MCOUNT_EXIT(s) \
	(void)_alpha_pal_swpipl(s);
#define	MCOUNT_DECL(s)	u_long s;
#ifdef GUPROF
struct gmonparam;

void	nullfunc_loop_profiled __P((void));
void	nullfunc_profiled __P((void));
void	startguprof __P((struct gmonparam *p));
void	stopguprof __P((struct gmonparam *p));
#else
#define startguprof(p)
#define stopguprof(p)
#endif /* GUPROF */

#else /* !_KERNEL */
typedef u_long	uintfptr_t;
#endif
