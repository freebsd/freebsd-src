/*
 * Copyright (c) 1996, Peter Wemm <peter@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: init_smp.c,v 1.11 1997/07/15 02:46:37 fsmp Exp $
 */

#include "opt_smp.h"

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/sysent.h>
#include <sys/reboot.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <sys/lock.h>

#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/smptests.h>	/** IGNORE_IDLEPROCS, TEST_TEST1 */
#include <machine/specialreg.h>
#ifndef CR0_EM
#define	CR0_EM	0x00000004	/* EMulate non-NPX coproc. (trap ESC only) */
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>

#if defined(TEST_TEST1)
void	ipi_test1(void);
#endif  /** TEST_TEST1 */

int smp_active = 0;	/* are the APs allowed to run? */

static int
sysctl_smp_active SYSCTL_HANDLER_ARGS
{
	int error = 0;
	int new_val;

	error = SYSCTL_OUT(req, &smp_active, sizeof(int));

	if (error || !req->newptr)
		return (error);

	error = SYSCTL_IN(req, &new_val, sizeof(int));
	if (error)
		return (error);
	if (new_val < 1)
		return (EBUSY);
	if (new_val > mp_ncpus)
		return (EINVAL);
	smp_active = new_val;
	return (0);
}

SYSCTL_PROC(_kern, OID_AUTO, smp_active, CTLTYPE_INT|CTLFLAG_RW,
		0, 0, &sysctl_smp_active, "I", "");

int smp_cpus = 0;	/* how many cpu's running */
SYSCTL_INT(_kern, OID_AUTO, smp_cpus, CTLFLAG_RD, &smp_cpus, 0, "");

int idle_debug = 0;
SYSCTL_INT(_kern, OID_AUTO, idle_debug, CTLFLAG_RW, &idle_debug, 0, "");

int invltlb_ok = 0;	/* throttle smp_invltlb() till safe */

#if defined(IGNORE_IDLEPROCS)
int ignore_idleprocs = 1;
#else
int ignore_idleprocs = 0;
#endif
SYSCTL_INT(_kern, OID_AUTO, ignore_idleprocs, CTLFLAG_RW, &ignore_idleprocs,
	   0, "");

static void smp_kickoff __P((void *dummy));
SYSINIT(smpkick, SI_SUB_SMP, SI_ORDER_FIRST, smp_kickoff, NULL)

static void smp_idleloop __P((void *));

void	secondary_main __P((void));

static int idle_loops = 0;
void	boot_unlock __P((void));
 
struct proc *SMPidleproc[NCPU];
static int cpu_starting = -1;

static void
smp_kickoff(dummy)
	void *dummy;
{
	int		rval[2];	/* return from fork */
	struct proc	*p;
	int i;

	/*
	 * Create the appropriate number of cpu-idle-eaters
	 */
	for (i = 0; i < mp_ncpus; i++) {
		/* kernel thread*/
		if (fork(&proc0, NULL, rval))
			panic("cannot fork idle process");
		p = pfind(rval[0]);
		cpu_set_fork_handler(p, smp_idleloop, NULL);
		SMPidleproc[i] = p;
		p->p_flag |= P_INMEM | P_SYSTEM | P_IDLEPROC;
		sprintf(p->p_comm, "cpuidle%d", i);

		/*
		 * PRIO_IDLE is the last scheduled of the three
		 * classes and we choose the lowest priority possible
		 * for there.
		 */
		p->p_rtprio.type = RTP_PRIO_IDLE;
		p->p_rtprio.prio = RTP_PRIO_MAX;
	}

}


#define MSG_CPU_MADEIT \
	printf("SMP: TADA! CPU #%d made it into the scheduler!.\n", \
	       cpuid)
#define MSG_NEXT_CPU \
	printf("SMP: %d of %d CPU's online. Unlocking next CPU..\n", \
	       smp_cpus, mp_ncpus)
#define MSG_FINAL_CPU \
	printf("SMP: All %d CPU's are online!\n", \
	       smp_cpus)
#define MSG_TOOMANY_CPU \
	printf("SMP: Hey! Too many cpu's started, %d of %d running!\n", \
	       smp_cpus, mp_ncpus)

/*
 * This is run by the secondary processor to kick things off.
 * It basically drops into the switch routine to pick the first
 * available process to run, which is probably an idle process.
 */

void
secondary_main()
{
	u_int   temp;

	get_mplock();

	/*
	 * Record our ID so we know when we've released the mp_stk.
	 * We must remain single threaded through this.
	 */
	cpu_starting = cpuid;
	smp_cpus++;

	/* Build our map of 'other' CPUs. */
	other_cpus = all_cpus & ~(1 << cpuid);

	printf("SMP: AP CPU #%d LAUNCHED!!  Starting Scheduling...\n",
		cpuid);

#if defined(TEST_TEST1)
/* XXX this would be dangerous for > 2 CPUs! */
	if (cpuid == IPI_TARGET_TEST1) {
		lapic.tpr = 0xff;
		ipi_test1();
	}
#endif  /** TEST_TEST1 */

        /* Setup the FPU. */
	temp = rcr0();
	temp &= ~(CR0_EM);
	temp |= (CR0_MP | CR0_NE | CR0_TS);
	load_cr0(temp);

	curproc = NULL;			/* ensure no context to save */
	cpu_switch(curproc);		/* start first process */

	panic("switch returned!");
}


/*
 * The main program loop for the idle process
 */

static void
smp_idleloop(dummy)
void *dummy;
{
	int dcnt = 0;
	int apic_id;

	/*
	 * This code is executed only on startup of the idleprocs
	 * The fact that this is executed is an indication that the
	 * idle procs are online and it's safe to kick off the first
	 * AP cpu.
	 */
	if ( ++idle_loops == mp_ncpus ) {
		printf("SMP: All idle procs online.\n");

#ifndef NO_AUTOSTART
		printf("SMP: *** AUTO *** starting 1st AP!\n");
		smp_cpus = 1;
		smp_active = mp_ncpus;	/* XXX */
		boot_unlock();
#else
		printf("You can now activate SMP processing, use: sysctl -w kern.smp_active=%d\n", mp_ncpus);
#endif
	}

	spl0();
	rel_mplock();

	while (1) {
		/*
		 * make the optimiser assume nothing about the
		 * which*qs variables
		 */
		__asm __volatile("" : : : "memory");
 
#ifdef NO_AUTOSTART
		if (smp_cpus == 0 && smp_active != 0) {
			get_mplock();
			printf("SMP: Starting 1st AP!\n");
			smp_cpus = 1;
			smp_active = mp_ncpus;  /* XXX */
			boot_unlock();
			rel_mplock();
		}
#endif

		/*
		 * If smp_active is set to (say) 1, we want cpu id's
		 * 1,2,etc to freeze here.
		 */
		if (smp_active && smp_active <= cpuid) {
			get_mplock();
			printf("SMP: cpu#%d freezing\n", cpuid);
			wakeup((caddr_t)&smp_active);
			rel_mplock();

			while (smp_active <= cpuid) {
				__asm __volatile("" : : : "memory");
			}
			get_mplock();
			printf("SMP: cpu#%d waking up!\n", cpuid);
			rel_mplock();
		}

/* XXX DEBUG */
		apic_id = (apic_id_to_logical[(lapic.id & 0x0f000000) >> 24]);
		if (cpuid != apic_id) {
			printf("SMP: cpuid = %d\n", cpuid);
			printf("SMP: apic_id = %d\n", apic_id);
			printf("PTD[MPPTDI] = %08x\n", PTD[MPPTDI]);
			panic("cpuid mismatch! boom!!");
		}
/* XXX END DEBUG */
			
		if (whichqs || whichrtqs || (!ignore_idleprocs && whichidqs)) {
			/* grab lock for kernel "entry" */
			get_mplock();

			/* We need to retest due to the spin lock */
			__asm __volatile("" : : : "memory");

			if (whichqs || whichrtqs ||
					(!ignore_idleprocs && whichidqs)) {
				splhigh();
				if (curproc)
					setrunqueue(curproc);
				cnt.v_swtch++;
				cpu_switch(curproc);
				microtime(&runtime);

				if (cpu_starting != -1 &&
				    cpu_starting == cpuid) {
					/*
					 * TADA! we have arrived! unlock the
					 * next cpu now that we have released
					 * the single mp_stk.
					 */
					MSG_CPU_MADEIT;
					cpu_starting = -1;

					/* Init local apic for irq's */
					apic_initialize();

					if (smp_cpus < mp_ncpus) {
						MSG_NEXT_CPU;
						boot_unlock();
					} else if (smp_cpus > mp_ncpus) {
						MSG_TOOMANY_CPU;
						panic("too many cpus");
					} else {
						MSG_FINAL_CPU;
						/*
						 * It's safe to send IPI's now
						 * that all CPUs are online.
						 */
						invltlb_ok = 1;
					}
				}

				(void)spl0();
			}
			rel_mplock();
		} else {
			dcnt++;
			if (idle_debug && (dcnt % idle_debug) == 0) {
				get_mplock();
				printf("idleproc pid#%d on cpu#%d, lock %08x\n",
					curproc->p_pid, cpuid, mp_lock);
				rel_mplock();
			}
		}
	}
}
