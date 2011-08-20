/*-
 * Copyright (c) 2001, John Baldwin <jhb@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
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
 * This module holds the global variables and machine independent functions
 * used for the kernel SMP support.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/smp.h>

#include "opt_sched.h"

#ifdef SMP
volatile cpumask_t stopped_cpus;
volatile cpumask_t started_cpus;
cpumask_t idle_cpus_mask;
cpumask_t hlt_cpus_mask;
cpumask_t logical_cpus_mask;

void (*cpustop_restartfunc)(void);
#endif
/* This is used in modules that need to work in both SMP and UP. */
cpumask_t all_cpus;

int mp_ncpus;
/* export this for libkvm consumers. */
int mp_maxcpus = MAXCPU;

struct cpu_top *smp_topology;
volatile int smp_started;
u_int mp_maxid;

SYSCTL_NODE(_kern, OID_AUTO, smp, CTLFLAG_RD, NULL, "Kernel SMP");

SYSCTL_INT(_kern_smp, OID_AUTO, maxid, CTLFLAG_RD, &mp_maxid, 0,
    "Max CPU ID.");

SYSCTL_INT(_kern_smp, OID_AUTO, maxcpus, CTLFLAG_RD, &mp_maxcpus, 0,
    "Max number of CPUs that the system was compiled for.");

int smp_active = 0;	/* are the APs allowed to run? */
SYSCTL_INT(_kern_smp, OID_AUTO, active, CTLFLAG_RW, &smp_active, 0,
    "Number of Auxillary Processors (APs) that were successfully started");

int smp_disabled = 0;	/* has smp been disabled? */
SYSCTL_INT(_kern_smp, OID_AUTO, disabled, CTLFLAG_RDTUN, &smp_disabled, 0,
    "SMP has been disabled from the loader");
TUNABLE_INT("kern.smp.disabled", &smp_disabled);

int smp_cpus = 1;	/* how many cpu's running */
SYSCTL_INT(_kern_smp, OID_AUTO, cpus, CTLFLAG_RD, &smp_cpus, 0,
    "Number of CPUs online");

#ifdef SMP
/* Enable forwarding of a signal to a process running on a different CPU */
static int forward_signal_enabled = 1;
SYSCTL_INT(_kern_smp, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0,
	   "Forwarding of a signal to a process on a different CPU");

/* Enable forwarding of roundrobin to all other cpus */
static int forward_roundrobin_enabled = 1;
SYSCTL_INT(_kern_smp, OID_AUTO, forward_roundrobin_enabled, CTLFLAG_RW,
	   &forward_roundrobin_enabled, 0,
	   "Forwarding of roundrobin to all other CPUs");

/* Variables needed for SMP rendezvous. */
static volatile int smp_rv_ncpus;
static void (*volatile smp_rv_setup_func)(void *arg);
static void (*volatile smp_rv_action_func)(void *arg);
static void (*volatile smp_rv_teardown_func)(void *arg);
static void * volatile smp_rv_func_arg;
static volatile int smp_rv_waiters[4];

/* 
 * Shared mutex to restrict busywaits between smp_rendezvous() and
 * smp(_targeted)_tlb_shootdown().  A deadlock occurs if both of these
 * functions trigger at once and cause multiple CPUs to busywait with
 * interrupts disabled. 
 */
struct mtx smp_ipi_mtx;

/*
 * Let the MD SMP code initialize mp_maxid very early if it can.
 */
static void
mp_setmaxid(void *dummy)
{
	cpu_mp_setmaxid();
}
SYSINIT(cpu_mp_setmaxid, SI_SUB_TUNABLES, SI_ORDER_FIRST, mp_setmaxid, NULL);

/*
 * Call the MD SMP initialization code.
 */
static void
mp_start(void *dummy)
{

	/* Probe for MP hardware. */
	if (smp_disabled != 0 || cpu_mp_probe() == 0) {
		mp_ncpus = 1;
		all_cpus = PCPU_GET(cpumask);
		return;
	}

	mtx_init(&smp_ipi_mtx, "smp rendezvous", NULL, MTX_SPIN);
	cpu_mp_start();
	printf("FreeBSD/SMP: Multiprocessor System Detected: %d CPUs\n",
	    mp_ncpus);
	cpu_mp_announce();
}
SYSINIT(cpu_mp, SI_SUB_CPU, SI_ORDER_THIRD, mp_start, NULL);

void
forward_signal(struct thread *td)
{
	int id;

	/*
	 * signotify() has already set TDF_ASTPENDING and TDF_NEEDSIGCHECK on
	 * this thread, so all we need to do is poke it if it is currently
	 * executing so that it executes ast().
	 */
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(TD_IS_RUNNING(td),
	    ("forward_signal: thread is not TDS_RUNNING"));

	CTR1(KTR_SMP, "forward_signal(%p)", td->td_proc);

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_signal_enabled)
		return;

	/* No need to IPI ourself. */
	if (td == curthread)
		return;

	id = td->td_oncpu;
	if (id == NOCPU)
		return;
	ipi_selected(1 << id, IPI_AST);
}

void
forward_roundrobin(void)
{
	struct pcpu *pc;
	struct thread *td;
	cpumask_t id, map, me;

	CTR0(KTR_SMP, "forward_roundrobin()");

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_roundrobin_enabled)
		return;
	map = 0;
	me = PCPU_GET(cpumask);
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		td = pc->pc_curthread;
		id = pc->pc_cpumask;
		if (id != me && (id & stopped_cpus) == 0 &&
		    !TD_IS_IDLETHREAD(td)) {
			td->td_flags |= TDF_NEEDRESCHED;
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
stop_cpus(cpumask_t map)
{
	int i;

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "stop_cpus(%x)", map);

	/* send the stop IPI to all CPUs in map */
	ipi_selected(map, IPI_STOP);

	i = 0;
	while ((stopped_cpus & map) != map) {
		/* spin */
		cpu_spinwait();
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
restart_cpus(cpumask_t map)
{

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "restart_cpus(%x)", map);

	/* signal other cpus to restart */
	atomic_store_rel_int(&started_cpus, map);

	/* wait for each to clear its bit */
	while ((stopped_cpus & map) != 0)
		cpu_spinwait();

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
	struct thread *td;
	void *local_func_arg;
	void (*local_setup_func)(void*);
	void (*local_action_func)(void*);
	void (*local_teardown_func)(void*);
#ifdef INVARIANTS
	int owepreempt;
#endif

	/* Ensure we have up-to-date values. */
	atomic_add_acq_int(&smp_rv_waiters[0], 1);
	while (smp_rv_waiters[0] < smp_rv_ncpus)
		cpu_spinwait();

	/* Fetch rendezvous parameters after acquire barrier. */
	local_func_arg = smp_rv_func_arg;
	local_setup_func = smp_rv_setup_func;
	local_action_func = smp_rv_action_func;
	local_teardown_func = smp_rv_teardown_func;

	/*
	 * Use a nested critical section to prevent any preemptions
	 * from occurring during a rendezvous action routine.
	 * Specifically, if a rendezvous handler is invoked via an IPI
	 * and the interrupted thread was in the critical_exit()
	 * function after setting td_critnest to 0 but before
	 * performing a deferred preemption, this routine can be
	 * invoked with td_critnest set to 0 and td_owepreempt true.
	 * In that case, a critical_exit() during the rendezvous
	 * action would trigger a preemption which is not permitted in
	 * a rendezvous action.  To fix this, wrap all of the
	 * rendezvous action handlers in a critical section.  We
	 * cannot use a regular critical section however as having
	 * critical_exit() preempt from this routine would also be
	 * problematic (the preemption must not occur before the IPI
	 * has been acknowleged via an EOI).  Instead, we
	 * intentionally ignore td_owepreempt when leaving the
	 * critical setion.  This should be harmless because we do not
	 * permit rendezvous action routines to schedule threads, and
	 * thus td_owepreempt should never transition from 0 to 1
	 * during this routine.
	 */
	td = curthread;
	td->td_critnest++;
#ifdef INVARIANTS
	owepreempt = td->td_owepreempt;
#endif
	
	/*
	 * If requested, run a setup function before the main action
	 * function.  Ensure all CPUs have completed the setup
	 * function before moving on to the action function.
	 */
	if (local_setup_func != smp_no_rendevous_barrier) {
		if (smp_rv_setup_func != NULL)
			smp_rv_setup_func(smp_rv_func_arg);
		atomic_add_int(&smp_rv_waiters[1], 1);
		while (smp_rv_waiters[1] < smp_rv_ncpus)
                	cpu_spinwait();
	}

	if (local_action_func != NULL)
		local_action_func(local_func_arg);

	if (local_teardown_func != smp_no_rendevous_barrier) {
		/*
		 * Signal that the main action has been completed.  If a
		 * full exit rendezvous is requested, then all CPUs will
		 * wait here until all CPUs have finished the main action.
		 */
		atomic_add_int(&smp_rv_waiters[2], 1);
		while (smp_rv_waiters[2] < smp_rv_ncpus)
			cpu_spinwait();

		if (local_teardown_func != NULL)
			local_teardown_func(local_func_arg);
	}

	/*
	 * Signal that the rendezvous is fully completed by this CPU.
	 * This means that no member of smp_rv_* pseudo-structure will be
	 * accessed by this target CPU after this point; in particular,
	 * memory pointed by smp_rv_func_arg.
	 */
	atomic_add_int(&smp_rv_waiters[3], 1);

	td->td_critnest--;
	KASSERT(owepreempt == td->td_owepreempt,
	    ("rendezvous action changed td_owepreempt"));
}

void
smp_rendezvous_cpus(cpumask_t map,
	void (* setup_func)(void *), 
	void (* action_func)(void *),
	void (* teardown_func)(void *),
	void *arg)
{
	int i, ncpus = 0;

	if (!smp_started) {
		if (setup_func != NULL)
			setup_func(arg);
		if (action_func != NULL)
			action_func(arg);
		if (teardown_func != NULL)
			teardown_func(arg);
		return;
	}

	for (i = 0; i <= mp_maxid; i++)
		if (((1 << i) & map) != 0 && !CPU_ABSENT(i))
			ncpus++;
	if (ncpus == 0)
		panic("ncpus is 0 with map=0x%x", map);

	mtx_lock_spin(&smp_ipi_mtx);

	/* Pass rendezvous parameters via global variables. */
	smp_rv_ncpus = ncpus;
	smp_rv_setup_func = setup_func;
	smp_rv_action_func = action_func;
	smp_rv_teardown_func = teardown_func;
	smp_rv_func_arg = arg;
	smp_rv_waiters[1] = 0;
	smp_rv_waiters[2] = 0;
	smp_rv_waiters[3] = 0;
	atomic_store_rel_int(&smp_rv_waiters[0], 0);

	/*
	 * Signal other processors, which will enter the IPI with
	 * interrupts off.
	 */
	ipi_selected(map & ~(1 << curcpu), IPI_RENDEZVOUS);

	/* Check if the current CPU is in the map */
	if ((map & (1 << curcpu)) != 0)
		smp_rendezvous_action();

	/*
	 * Ensure that the master CPU waits for all the other
	 * CPUs to finish the rendezvous, so that smp_rv_*
	 * pseudo-structure and the arg are guaranteed to not
	 * be in use.
	 */
	while (atomic_load_acq_int(&smp_rv_waiters[3]) < ncpus)
		cpu_spinwait();

	mtx_unlock_spin(&smp_ipi_mtx);
}

void
smp_rendezvous(void (* setup_func)(void *), 
	       void (* action_func)(void *),
	       void (* teardown_func)(void *),
	       void *arg)
{
	smp_rendezvous_cpus(all_cpus, setup_func, action_func, teardown_func, arg);
}
#else /* !SMP */

/*
 * Provide dummy SMP support for UP kernels.  Modules that need to use SMP
 * APIs will still work using this dummy support.
 */
static void
mp_setvariables_for_up(void *dummy)
{
	mp_ncpus = 1;
	mp_maxid = PCPU_GET(cpuid);
	all_cpus = PCPU_GET(cpumask);
	KASSERT(PCPU_GET(cpuid) == 0, ("UP must have a CPU ID of zero"));
}
SYSINIT(cpu_mp_setvariables, SI_SUB_TUNABLES, SI_ORDER_FIRST,
    mp_setvariables_for_up, NULL);

void
smp_rendezvous_cpus(cpumask_t map,
	void (*setup_func)(void *), 
	void (*action_func)(void *),
	void (*teardown_func)(void *),
	void *arg)
{
	if (setup_func != NULL)
		setup_func(arg);
	if (action_func != NULL)
		action_func(arg);
	if (teardown_func != NULL)
		teardown_func(arg);
}

void
smp_rendezvous(void (*setup_func)(void *), 
	       void (*action_func)(void *),
	       void (*teardown_func)(void *),
	       void *arg)
{

	if (setup_func != NULL)
		setup_func(arg);
	if (action_func != NULL)
		action_func(arg);
	if (teardown_func != NULL)
		teardown_func(arg);
}
#endif /* SMP */

void
smp_no_rendevous_barrier(void *dummy)
{
#ifdef SMP
	KASSERT((!smp_started),("smp_no_rendevous called and smp is started"));
#endif
}
