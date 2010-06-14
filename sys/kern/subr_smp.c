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

int smp_topology = 0;	/* Which topology we're using. */
SYSCTL_INT(_kern_smp, OID_AUTO, topology, CTLFLAG_RD, &smp_topology, 0,
    "Topology override setting; 0 is default provided by hardware.");
TUNABLE_INT("kern.smp.topology", &smp_topology);

#ifdef SMP
/* Enable forwarding of a signal to a process running on a different CPU */
static int forward_signal_enabled = 1;
SYSCTL_INT(_kern_smp, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0,
	   "Forwarding of a signal to a process on a different CPU");

/* Variables needed for SMP rendezvous. */
static volatile int smp_rv_ncpus;
static void (*volatile smp_rv_setup_func)(void *arg);
static void (*volatile smp_rv_action_func)(void *arg);
static void (*volatile smp_rv_teardown_func)(void *arg);
static void *volatile smp_rv_func_arg;
static volatile int smp_rv_waiters[3];

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

	mtx_init(&smp_ipi_mtx, "smp rendezvous", NULL, MTX_SPIN);

	/* Probe for MP hardware. */
	if (smp_disabled != 0 || cpu_mp_probe() == 0) {
		mp_ncpus = 1;
		all_cpus = PCPU_GET(cpumask);
		return;
	}

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
static int
generic_stop_cpus(cpumask_t map, u_int type)
{
	int i;

	KASSERT(type == IPI_STOP || type == IPI_STOP_HARD,
	    ("%s: invalid stop type", __func__));

	if (!smp_started)
		return 0;

	CTR2(KTR_SMP, "stop_cpus(%x) with %u type", map, type);

	/* send the stop IPI to all CPUs in map */
	ipi_selected(map, type);

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

int
stop_cpus(cpumask_t map)
{

	return (generic_stop_cpus(map, IPI_STOP));
}

int
stop_cpus_hard(cpumask_t map)
{

	return (generic_stop_cpus(map, IPI_STOP_HARD));
}

#if defined(__amd64__)
/*
 * When called the executing CPU will send an IPI to all other CPUs
 *  requesting that they halt execution.
 *
 * Usually (but not necessarily) called with 'other_cpus' as its arg.
 *
 *  - Signals all CPUs in map to suspend.
 *  - Waits for each to suspend.
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
suspend_cpus(cpumask_t map)
{
	int i;

	if (!smp_started)
		return (0);

	CTR1(KTR_SMP, "suspend_cpus(%x)", map);

	/* send the suspend IPI to all CPUs in map */
	ipi_selected(map, IPI_SUSPEND);

	i = 0;
	while ((stopped_cpus & map) != map) {
		/* spin */
		cpu_spinwait();
		i++;
#ifdef DIAGNOSTIC
		if (i == 100000) {
			printf("timeout suspending cpus\n");
			break;
		}
#endif
	}

	return (1);
}
#endif

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
	void* local_func_arg = smp_rv_func_arg;
	void (*local_setup_func)(void*)   = smp_rv_setup_func;
	void (*local_action_func)(void*)   = smp_rv_action_func;
	void (*local_teardown_func)(void*) = smp_rv_teardown_func;

	/* Ensure we have up-to-date values. */
	atomic_add_acq_int(&smp_rv_waiters[0], 1);
	while (smp_rv_waiters[0] < smp_rv_ncpus)
		cpu_spinwait();

	/* setup function */
	if (local_setup_func != smp_no_rendevous_barrier) {
		if (smp_rv_setup_func != NULL)
			smp_rv_setup_func(smp_rv_func_arg);

		/* spin on entry rendezvous */
		atomic_add_int(&smp_rv_waiters[1], 1);
		while (smp_rv_waiters[1] < smp_rv_ncpus)
                	cpu_spinwait();
	}

	/* action function */
	if (local_action_func != NULL)
		local_action_func(local_func_arg);

	/* spin on exit rendezvous */
	atomic_add_int(&smp_rv_waiters[2], 1);
	if (local_teardown_func == smp_no_rendevous_barrier)
                return;
	while (smp_rv_waiters[2] < smp_rv_ncpus)
		cpu_spinwait();

	/* teardown function */
	if (local_teardown_func != NULL)
		local_teardown_func(local_func_arg);
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

	CPU_FOREACH(i) {
		if (((1 << i) & map) != 0)
			ncpus++;
	}
	if (ncpus == 0)
		panic("ncpus is 0 with map=0x%x", map);

	/* obtain rendezvous lock */
	mtx_lock_spin(&smp_ipi_mtx);

	/* set static function pointers */
	smp_rv_ncpus = ncpus;
	smp_rv_setup_func = setup_func;
	smp_rv_action_func = action_func;
	smp_rv_teardown_func = teardown_func;
	smp_rv_func_arg = arg;
	smp_rv_waiters[1] = 0;
	smp_rv_waiters[2] = 0;
	atomic_store_rel_int(&smp_rv_waiters[0], 0);

	/* signal other processors, which will enter the IPI with interrupts off */
	ipi_selected(map & ~(1 << curcpu), IPI_RENDEZVOUS);

	/* Check if the current CPU is in the map */
	if ((map & (1 << curcpu)) != 0)
		smp_rendezvous_action();

	if (teardown_func == smp_no_rendevous_barrier)
		while (atomic_load_acq_int(&smp_rv_waiters[2]) < ncpus)
			cpu_spinwait();

	/* release lock */
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

static struct cpu_group group[MAXCPU];

struct cpu_group *
smp_topo(void)
{
	struct cpu_group *top;

	/*
	 * Check for a fake topology request for debugging purposes.
	 */
	switch (smp_topology) {
	case 1:
		/* Dual core with no sharing.  */
		top = smp_topo_1level(CG_SHARE_NONE, 2, 0);
		break;
	case 2:
		/* No topology, all cpus are equal. */
		top = smp_topo_none();
		break;
	case 3:
		/* Dual core with shared L2.  */
		top = smp_topo_1level(CG_SHARE_L2, 2, 0);
		break;
	case 4:
		/* quad core, shared l3 among each package, private l2.  */
		top = smp_topo_1level(CG_SHARE_L3, 4, 0);
		break;
	case 5:
		/* quad core,  2 dualcore parts on each package share l2.  */
		top = smp_topo_2level(CG_SHARE_NONE, 2, CG_SHARE_L2, 2, 0);
		break;
	case 6:
		/* Single-core 2xHTT */
		top = smp_topo_1level(CG_SHARE_L1, 2, CG_FLAG_HTT);
		break;
	case 7:
		/* quad core with a shared l3, 8 threads sharing L2.  */
		top = smp_topo_2level(CG_SHARE_L3, 4, CG_SHARE_L2, 8,
		    CG_FLAG_SMT);
		break;
	default:
		/* Default, ask the system what it wants. */
		top = cpu_topo();
		break;
	}
	/*
	 * Verify the returned topology.
	 */
	if (top->cg_count != mp_ncpus)
		panic("Built bad topology at %p.  CPU count %d != %d",
		    top, top->cg_count, mp_ncpus);
	if (top->cg_mask != all_cpus)
		panic("Built bad topology at %p.  CPU mask 0x%X != 0x%X",
		    top, top->cg_mask, all_cpus);
	return (top);
}

struct cpu_group *
smp_topo_none(void)
{
	struct cpu_group *top;

	top = &group[0];
	top->cg_parent = NULL;
	top->cg_child = NULL;
	if (mp_ncpus == sizeof(top->cg_mask) * 8)
		top->cg_mask = -1;
	else
		top->cg_mask = (1 << mp_ncpus) - 1;
	top->cg_count = mp_ncpus;
	top->cg_children = 0;
	top->cg_level = CG_SHARE_NONE;
	top->cg_flags = 0;
	
	return (top);
}

static int
smp_topo_addleaf(struct cpu_group *parent, struct cpu_group *child, int share,
    int count, int flags, int start)
{
	cpumask_t mask;
	int i;

	for (mask = 0, i = 0; i < count; i++, start++)
		mask |= (1 << start);
	child->cg_parent = parent;
	child->cg_child = NULL;
	child->cg_children = 0;
	child->cg_level = share;
	child->cg_count = count;
	child->cg_flags = flags;
	child->cg_mask = mask;
	parent->cg_children++;
	for (; parent != NULL; parent = parent->cg_parent) {
		if ((parent->cg_mask & child->cg_mask) != 0)
			panic("Duplicate children in %p.  mask 0x%X child 0x%X",
			    parent, parent->cg_mask, child->cg_mask);
		parent->cg_mask |= child->cg_mask;
		parent->cg_count += child->cg_count;
	}

	return (start);
}

struct cpu_group *
smp_topo_1level(int share, int count, int flags)
{
	struct cpu_group *child;
	struct cpu_group *top;
	int packages;
	int cpu;
	int i;

	cpu = 0;
	top = &group[0];
	packages = mp_ncpus / count;
	top->cg_child = child = &group[1];
	top->cg_level = CG_SHARE_NONE;
	for (i = 0; i < packages; i++, child++)
		cpu = smp_topo_addleaf(top, child, share, count, flags, cpu);
	return (top);
}

struct cpu_group *
smp_topo_2level(int l2share, int l2count, int l1share, int l1count,
    int l1flags)
{
	struct cpu_group *top;
	struct cpu_group *l1g;
	struct cpu_group *l2g;
	int cpu;
	int i;
	int j;

	cpu = 0;
	top = &group[0];
	l2g = &group[1];
	top->cg_child = l2g;
	top->cg_level = CG_SHARE_NONE;
	top->cg_children = mp_ncpus / (l2count * l1count);
	l1g = l2g + top->cg_children;
	for (i = 0; i < top->cg_children; i++, l2g++) {
		l2g->cg_parent = top;
		l2g->cg_child = l1g;
		l2g->cg_level = l2share;
		for (j = 0; j < l2count; j++, l1g++)
			cpu = smp_topo_addleaf(l2g, l1g, l1share, l1count,
			    l1flags, cpu);
	}
	return (top);
}


struct cpu_group *
smp_topo_find(struct cpu_group *top, int cpu)
{
	struct cpu_group *cg;
	cpumask_t mask;
	int children;
	int i;

	mask = (1 << cpu);
	cg = top;
	for (;;) {
		if ((cg->cg_mask & mask) == 0)
			return (NULL);
		if (cg->cg_children == 0)
			return (cg);
		children = cg->cg_children;
		for (i = 0, cg = cg->cg_child; i < children; cg++, i++)
			if ((cg->cg_mask & mask) != 0)
				break;
	}
	return (NULL);
}
#else /* !SMP */

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
#endif /* SMP */

void
smp_no_rendevous_barrier(void *dummy)
{
#ifdef SMP
	KASSERT((!smp_started),("smp_no_rendevous called and smp is started"));
#endif
}
