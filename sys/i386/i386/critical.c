/*-
 * Copyright (c) 2001 Matthew Dillon.  This code is distributed under
 * the BSD copyright, /usr/src/COPYRIGHT.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/ucontext.h>

#ifdef SMP
#include <machine/privatespace.h>
#include <machine/smp.h>
#else
/*
 * XXX this mess to get sched_ithd() and call_fast_unpend()
 */
#include <sys/bus.h>
#include <machine/apic.h>
#include <machine/frame.h>
#include <i386/isa/icu.h>
#include <i386/isa/intr_machdep.h>
#endif

void unpend(void);	/* note: not static (called from assembly) */

/*
 * Instrument our ability to run critical sections with interrupts
 * enabled.  Default is 1 (enabled).  The value can be changed on the
 * fly, at any time.  If set to 0 the original interrupt disablement
 * will be used for critical sections.
 */
int critical_mode = 1;
SYSCTL_INT(_debug, OID_AUTO, critical_mode,
	CTLFLAG_RW, &critical_mode, 0, "");

/*
 *	cpu_critical_enter:
 *
 *	This routine is called from critical_enter() on the 0->1 transition
 *	of td_critnest, prior to it being incremented to 1.
 *
 *	If old-style critical section handling (critical_mode == 0), we
 *	disable interrupts. 
 *
 *	If new-style critical section handling (criticla_mode != 0), we
 *	do not have to do anything.  However, as a side effect any
 *	interrupts occuring while td_critnest is non-zero will be
 *	deferred.
 */
void
cpu_critical_enter(void)
{
	if (critical_mode == 0) {
		struct thread *td = curthread;
		td->td_md.md_savecrit = intr_disable();
	}
}

/*
 *	cpu_critical_exit:
 *
 *	This routine is called from critical_exit() on a 1->0 transition
 *	of td_critnest, after it has been decremented to 0.  We are
 *	exiting the last critical section.
 *
 *	If td_critnest is -1 this is the 'new' critical_enter()/exit()
 *	code (the default critical_mode=1) and we do not have to do 
 *	anything unless PCPU_GET(int_pending) is non-zero. 
 *
 *	Note that the td->critnest (1->0) transition interrupt race against
 *	our int_pending/unpend() check below is handled by the interrupt
 *	code for us, so we do not have to do anything fancy.
 *
 *	Otherwise td_critnest contains the saved hardware interrupt state
 *	and will be restored.  Since interrupts were hard-disabled there
 *	will be no pending interrupts to dispatch (the 'original' code).
 */
void
cpu_critical_exit(void)
{
	struct thread *td = curthread;

	if (td->td_md.md_savecrit != (register_t)-1) {
		intr_restore(td->td_md.md_savecrit);
		td->td_md.md_savecrit = (register_t)-1;
	} else {
		/*
		 * We may have to schedule pending interrupts.  Create
		 * conditions similar to an interrupt context and call
		 * unpend().
		 *
		 * note: we do this even if we are in an interrupt
		 * nesting level.  Deep nesting is protected by
		 * critical_*() and if we conditionalized it then we
		 * would have to check int_pending again whenever
		 * we decrement td_intr_nesting_level to 0.
		 */
		if (PCPU_GET(int_pending)) {
			register_t eflags;

			eflags = intr_disable();
			if (PCPU_GET(int_pending)) {
				++td->td_intr_nesting_level;
				unpend();
				--td->td_intr_nesting_level;
			}
			intr_restore(eflags);
		}
	}
}

/*
 * cpu_critical_fork_exit() - cleanup after fork
 *
 *	For i386 we do not have to do anything, td_critnest and
 *	td_savecrit are handled by the fork trampoline code.
 */
void
cpu_critical_fork_exit(void)
{
}

/*
 * cpu_thread_link() - thread linkup, initialize machine-dependant fields
 *
 *	(copy code originally in kern/kern_proc.c).  XXX we actually
 *	don't have to initialize this field but it's probably a good
 *	idea for the moment for debugging's sake.  The field is only
 *	valid when td_critnest is non-zero.
 */
void
cpu_thread_link(struct thread *td)
{
	td->td_md.md_savecrit = 0;
}

/*
 * Called from cpu_critical_exit() or called from the assembly vector code
 * to process any interrupts which may have occured while we were in
 * a critical section.
 *
 * 	- interrupts must be disabled
 *	- td_critnest must be 0
 *	- td_intr_nesting_level must be incremented by the caller
 */
void
unpend(void)
{
	KASSERT(curthread->td_critnest == 0, ("unpend critnest != 0"));
	KASSERT((read_eflags() & PSL_I) == 0, ("unpend interrupts enabled1"));
	curthread->td_critnest = 1;
	for (;;) {
		u_int32_t mask;

		/*
		 * Fast interrupts have priority
		 */
		if ((mask = PCPU_GET(fpending)) != 0) {
			int irq = bsfl(mask);
			PCPU_SET(fpending, mask & ~(1 << irq));
			call_fast_unpend(irq);
			KASSERT((read_eflags() & PSL_I) == 0, ("unpend interrupts enabled2 %d", irq));
			continue;
		}

		/*
		 * Threaded interrupts come next
		 */
		if ((mask = PCPU_GET(ipending)) != 0) {
			int irq = bsfl(mask);
			PCPU_SET(ipending, mask & ~(1 << irq));
			sched_ithd((void *)irq);
			KASSERT((read_eflags() & PSL_I) == 0, ("unpend interrupts enabled3 %d", irq));
			continue;
		}

		/*
		 * Software interrupts and delayed IPIs are last
		 *
		 * XXX give the bits #defined names.  see also
		 * isa/xxx_vector.s
		 */
		if ((mask = PCPU_GET(spending)) != 0) {
			int irq = bsfl(mask);
			PCPU_SET(spending, mask & ~(1 << irq));
			switch(irq) {
			case 0:		/* bit 0 - hardclock */
				mtx_lock_spin(&sched_lock);
				hardclock_process(curthread, 0);
				mtx_unlock_spin(&sched_lock);
				break;
			case 1:		/* bit 1 - statclock */
				mtx_lock_spin(&sched_lock);
				statclock_process(curthread->td_kse, (register_t)unpend, 0);
				mtx_unlock_spin(&sched_lock);
				break;
			}
			KASSERT((read_eflags() & PSL_I) == 0, ("unpend interrupts enabled4 %d", irq));
			continue;
		}
		break;
	}
	/*
	 * Interrupts are still disabled, we can safely clear int_pending 
	 * and td_critnest.
	 */
	KASSERT((read_eflags() & PSL_I) == 0, ("unpend interrupts enabled5"));
	PCPU_SET(int_pending, 0);
	curthread->td_critnest = 0;
}

