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

#define MCOUNT __asm ("							\n\
	.globl	_mcount							\n\
	.proc	_mcount							\n\
_mcount:								\n\
	alloc	loc0=ar.pfs,8,7,2,0	// space to save r8-r11,rp,b7	\n\
	add	sp=-8*16,sp		// space to save f8-f15		\n\
	mov	loc1=rp			// caller's return address	\n\
	mov	loc2=b7			// our return back to caller	\n\
	;;								\n\
	add	r17=16,sp		// leave 16 bytes for mcount	\n\
	add	r18=32,sp						\n\
	;;								\n\
	mov	loc3=r8			// structure return address	\n\
	mov	loc4=r9			// language specific		\n\
	mov	loc5=r10		// language specific		\n\
	mov	loc6=r11		// language specific		\n\
	;;								\n\
	stf.spill [r17]=f8,32		// save float arguments		\n\
	stf.spill [r18]=f9,32						\n\
	mov	out0=rp			// frompc			\n\
	;;								\n\
	stf.spill [r17]=f10,32						\n\
	stf.spill [r18]=f11,32						\n\
	mov	out1=b7			// selfpc			\n\
	;;								\n\
	stf.spill [r17]=f12,32						\n\
	stf.spill [r18]=f13,32						\n\
	;;								\n\
	stf.spill [r17]=f14,32						\n\
	stf.spill [r18]=f15,32						\n\
	;;								\n\
	br.call.sptk.many rp=mcount					\n\
	;;								\n\
	add	r17=16,sp						\n\
	add	r18=32,sp						\n\
	;;								\n\
	ldf.fill f8=[r17],32						\n\
	ldf.fill f9=[r18],32						\n\
	mov	r8=loc3			// restore structure pointer	\n\
	;;								\n\
	ldf.fill f10=[r17],32		// restore float arguments	\n\
	ldf.fill f11=[r18],32						\n\
	mov	r9=loc4							\n\
	;;								\n\
	ldf.fill f12=[r17],32		// etc.				\n\
	ldf.fill f13=[r18],32						\n\
	mov	r10=loc5						\n\
	;;								\n\
	ldf.fill f14=[r17],32						\n\
	ldf.fill f15=[r18],32						\n\
	mov	r11=loc6						\n\
	;;								\n\
	mov	b7=loc2			// clean up			\n\
	mov	rp=loc1							\n\
	mov	ar.pfs=loc0						\n\
	;;								\n\
	alloc	r14=ar.pfs,0,0,8,0	// drop our register frame	\n\
	br.sptk.many b7			// back to caller		\n\
									\n\
	.end	_mcount");

#ifdef _KERNEL
/*
 * The following two macros do splhigh and splx respectively.
 */
#define MCOUNT_ENTER(s) \n\
	_c = cpu_critical_enter()
#define MCOUNT_EXIT(s) \n\
	cpu_critical_exit(_c)
#define	MCOUNT_DECL(s)	critical_t c;
#ifdef GUPROF
struct gmonparam;

void	nullfunc_loop_profiled(void);
void	nullfunc_profiled(void);
void	startguprof(struct gmonparam *p);
void	stopguprof(struct gmonparam *p);
#else
#define startguprof(p)
#define stopguprof(p)
#endif /* GUPROF */

#else /* !_KERNEL */
typedef u_long	uintfptr_t;
#endif
