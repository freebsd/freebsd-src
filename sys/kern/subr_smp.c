/*
 * Copyright (c) 2001
 *	John Baldwin <jhb@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BALDWIN AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JOHN BALDWIN OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This module holds the global variables and machine independent functions
 * used for the kernel SMP support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

volatile u_int stopped_cpus;
volatile u_int started_cpus;

void (*cpustop_restartfunc) __P((void));
int mp_ncpus;

u_int all_cpus;

SYSCTL_NODE(_kern, OID_AUTO, smp, CTLFLAG_RD, NULL, "Kernel SMP");

int smp_active = 0;	/* are the APs allowed to run? */
SYSCTL_INT(_kern_smp, OID_AUTO, active, CTLFLAG_RW, &smp_active, 0, "");

int smp_cpus = 1;	/* how many cpu's running */
SYSCTL_INT(_kern_smp, OID_AUTO, cpus, CTLFLAG_RD, &smp_cpus, 0, "");

/* Enable forwarding of a signal to a process running on a different CPU */
static int forward_signal_enabled = 1;
SYSCTL_INT(_kern_smp, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0, "");

/* Enable forwarding of roundrobin to all other cpus */
static int forward_roundrobin_enabled = 1;
SYSCTL_INT(_kern_smp, OID_AUTO, forward_roundrobin_enabled, CTLFLAG_RW,
	   &forward_roundrobin_enabled, 0, "");

/* Variables needed for SMP rendezvous. */
static void (*smp_rv_setup_func)(void *arg);
static void (*smp_rv_action_func)(void *arg);
static void (*smp_rv_teardown_func)(void *arg);
static void *smp_rv_func_arg;
static volatile int smp_rv_waiters[2];
static struct mtx smp_rv_mtx;

/*
 * Initialize MI SMP variables and call the MD SMP initialization code.
 */
static void
mp_start(void *dummy)
{

	/* Probe for MP hardware. */
	if (cpu_mp_probe() == 0)
		return;

	mtx_init(&smp_rv_mtx, "smp rendezvous", MTX_SPIN);
	cpu_mp_start();
	printf("FreeBSD/SMP: Multiprocessor System Detected: %d CPUs\n",
	    mp_ncpus);
	cpu_mp_announce();
}
SYSINIT(cpu_mp, SI_SUB_CPU, SI_ORDER_SECOND, mp_start, NULL)

void
forward_signal(struct thread *td)
{
	int id;

	/*
	 * signotify() has already set PS_ASTPENDING on this process so all
	 * we need to do is poke it if it is currently executing so that it
	 * executes ast().
	 */
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(td->td_proc->p_stat == SRUN, ("forward_signal: process is not SRUN"));

	CTR1(KTR_SMP, "forward_signal(%p)", td->td_proc);

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_signal_enabled)
		return;

	/* No need to IPI ourself. */
	if (td == curthread)
		return;

	id = td->td_kse->ke_oncpu;
	if (id == NOCPU)
		return;
	ipi_selected(1 << id, IPI_AST);
}

void
forward_roundrobin(void)
{
	struct globaldata *gd;
	struct thread *td;
	u_int id, map;

	mtx_assert(&sched_lock, MA_OWNED);

	CTR0(KTR_SMP, "forward_roundrobin()");

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_roundrobin_enabled)
		return;
	map = 0;
	SLIST_FOREACH(gd, &cpuhead, gd_allcpu) {
		td = gd->gd_curthread;
		id = gd->gd_cpuid;
		if (id != PCPU_GET(cpuid) && (id & stopped_cpus) == 0 &&
		    td != gd->gd_idlethread) {
			td->td_kse->ke_flags |= KEF_NEEDRESCHED;
			map |= id;
		}
	}
	ipi_selected(map, IPI_AST);
}

/*
 * When called the executing CPU will send an IPI to all other CPUs
 *  requesting that they halt execution.
 *
 * Usually (but not necessarily) called with 'other_cpus' as its arg.
 *
 *  - Signals all CPUs in map to stop.
 *  - Waits for each to stop.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 *
 * XXX FIXME: this is not MP-safe, needs a lock to prevent multiple CPUs
 *            from executing at same time.
 */
int
stop_cpus(u_int map)
{
	int i;

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "stop_cpus(%x)", map);

	/* send the stop IPI to all CPUs in map */
	ipi_selected(map, IPI_STOP);
	
	i = 0;
	while ((atomic_load_acq_int(&stopped_cpus) & map) != map) {
		/* spin */
		i++;
#ifdef DIAGNOSTIC
		if (i == 100000) {
			printf("timeout stopping cpus\n");
			break;
		}
#endif
	}

	return 1;
}


/*
 * Called by a CPU to restart stopped CPUs. 
 *
 * Usually (but not necessarily) called with 'stopped_cpus' as its arg.
 *
 *  - Signals all CPUs in map to restart.
 *  - Waits for each to restart.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 */
int
restart_cpus(u_int map)
{

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "restart_cpus(%x)", map);

	/* signal other cpus to restart */
	atomic_store_rel_int(&started_cpus, map);

	/* wait for each to clear its bit */
	while ((atomic_load_acq_int(&stopped_cpus) & map) != 0)
		;	/* nothing */

	return 1;
}

/*
 * All-CPU rendezvous.  CPUs are signalled, all execute the setup function 
 * (if specified), rendezvous, execute the action function (if specified),
 * rendezvous again, execute the teardown function (if specified), and then
 * resume.
 *
 * Note that the supplied external functions _must_ be reentrant and aware
 * that they are running in parallel and in an unknown lock context.
 */
void
smp_rendezvous_action(void)
{

	/* setup function */
	if (smp_rv_setup_func != NULL)
		smp_rv_setup_func(smp_rv_func_arg);
	/* spin on entry rendezvous */
	atomic_add_int(&smp_rv_waiters[0], 1);
	while (atomic_load_acq_int(&smp_rv_waiters[0]) < mp_ncpus)
		;	/* nothing */
	/* action function */
	if (smp_rv_action_func != NULL)
		smp_rv_action_func(smp_rv_func_arg);
	/* spin on exit rendezvous */
	atomic_add_int(&smp_rv_waiters[1], 1);
	while (atomic_load_acq_int(&smp_rv_waiters[1]) < mp_ncpus)
		;	/* nothing */
	/* teardown function */
	if (smp_rv_teardown_func != NULL)
		smp_rv_teardown_func(smp_rv_func_arg);
}

void
smp_rendezvous(void (* setup_func)(void *), 
	       void (* action_func)(void *),
	       void (* teardown_func)(void *),
	       void *arg)
{

	if (!smp_started) {
		if (setup_func != NULL)
			setup_func(arg);
		if (action_func != NULL)
			action_func(arg);
		if (teardown_func != NULL)
			teardown_func(arg);
		return;
	}
		
	/* obtain rendezvous lock */
	mtx_lock_spin(&smp_rv_mtx);

	/* set static function pointers */
	smp_rv_setup_func = setup_func;
	smp_rv_action_func = action_func;
	smp_rv_teardown_func = teardown_func;
	smp_rv_func_arg = arg;
	smp_rv_waiters[0] = 0;
	smp_rv_waiters[1] = 0;

	/* signal other processors, which will enter the IPI with interrupts off */
	ipi_all_but_self(IPI_RENDEZVOUS);

	/* call executor function */
	smp_rendezvous_action();

	/* release lock */
	mtx_unlock_spin(&smp_rv_mtx);
}
