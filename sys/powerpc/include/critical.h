/*-
 * Copyright (c) 2002 Matthew Dillon.  This code is distributed under
 * the BSD copyright, /usr/src/COPYRIGHT.
 *
 * This file contains prototypes and high-level inlines related to
 * machine-level critical function support:
 *
 *	cpu_critical_enter()		- inlined
 *	cpu_critical_exit()		- inlined
 *	cpu_critical_fork_exit()	- prototyped
 *	related support functions residing
 *	in <arch>/<arch>/critical.c	- prototyped
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CRITICAL_H_
#define	_MACHINE_CRITICAL_H_

__BEGIN_DECLS

/*
 * Prototypes - see <arch>/<arch>/critical.c
 */
void cpu_critical_fork_exit(void);

#ifdef	__GNUC__

/*
 *	cpu_critical_enter:
 *
 *	This routine is called from critical_enter() on the 0->1 transition
 *	of td_critnest, prior to it being incremented to 1.
 */

static __inline void
cpu_critical_enter(void)
{
	u_int           msr;
	struct thread	*td = curthread;

	msr = mfmsr();
	td->td_md.md_savecrit = msr;
	msr &= ~PSL_EE;
	mtmsr(msr);
}

/*
 *	cpu_critical_exit:
 *
 *	This routine is called from critical_exit() on a 1->0 transition
 *	of td_critnest, after it has been decremented to 0.  We are
 *	exiting the last critical section.
 */
static __inline void
cpu_critical_exit(void)
{
	struct thread *td = curthread;

	mtmsr(td->td_md.md_savecrit);
}


#else /* !__GNUC__ */

void cpu_critical_enter(void)
void cpu_critical_exit(void)

#endif	/* __GNUC__ */

__END_DECLS

#endif /* !_MACHINE_CRITICAL_H_ */

