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
#include <machine/critical.h>

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

void i386_unpend(void);		/* NOTE: not static, called from assembly */

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
 * cpu_unpend() -	called from critical_exit() inline after quick
 *			interrupt-pending check.
 */
void
cpu_unpend(void)
{
	register_t eflags;
	struct thread *td;

	td = curthread;
	eflags = intr_disable();
	if (PCPU_GET(int_pending)) {
		++td->td_intr_nesting_level;
		i386_unpend();
		--td->td_intr_nesting_level;
	}
	intr_restore(eflags);
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
 * Called from cpu_unpend or called from the assembly vector code
 * to process any interrupts which may have occured while we were in
 * a critical section.
 *
 * 	- interrupts must be disabled
 *	- td_critnest must be 0
 *	- td_intr_nesting_level must be incremented by the caller
 *
 * NOT STATIC (called from assembly)
 */
void
i386_unpend(void)
{
	KASSERT(curthread->td_critnest == 0, ("unpend critnest != 0"));
	KASSERT((read_eflags() & PSL_I) == 0, ("unpend interrupts enabled1"));
	curthread->td_critnest = 1;
	for (;;) {
		u_int32_t mask;
		int irq;

		/*
		 * Fast interrupts have priority
		 */
		if ((mask = PCPU_GET(fpending)) != 0) {
			irq = bsfl(mask);
			PCPU_SET(fpending, mask & ~(1 << irq));
			call_fast_unpend(irq);
			KASSERT((read_eflags() & PSL_I) == 0,
			    ("unpend interrupts enabled2 %d", irq));
			continue;
		}

		/*
		 * Threaded interrupts come next
		 */
		if ((mask = PCPU_GET(ipending)) != 0) {
			irq = bsfl(mask);
			PCPU_SET(ipending, mask & ~(1 << irq));
			sched_ithd((void *)irq);
			KASSERT((read_eflags() & PSL_I) == 0,
			    ("unpend interrupts enabled3 %d", irq));
			continue;
		}

		/*
		 * Software interrupts and delayed IPIs are last
		 *
		 * XXX give the bits #defined names.  see also
		 * isa/xxx_vector.s
		 */
		if ((mask = PCPU_GET(spending)) != 0) {
			irq = bsfl(mask);
			PCPU_SET(spending, mask & ~(1 << irq));
			switch(irq) {
			case 0:		/* bit 0 - hardclock */
				mtx_lock_spin(&sched_lock);
				hardclock_process(curthread, 0);
				mtx_unlock_spin(&sched_lock);
				break;
			case 1:		/* bit 1 - statclock */
				mtx_lock_spin(&sched_lock);
				statclock_process(curthread->td_kse,
				    (register_t)i386_unpend, 0);
				mtx_unlock_spin(&sched_lock);
				break;
			}
			KASSERT((read_eflags() & PSL_I) == 0,
			    ("unpend interrupts enabled4 %d", irq));
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
