/*-
 * Copyright (c) 2002-2003, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>
#include <machine/smp.h>

#define KTR_ULE         KTR_NFS

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
/* XXX This is bogus compatability crap for ps */
static fixpt_t  ccpu = 0.95122942450071400909 * FSCALE; /* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

static void sched_setup(void *dummy);
SYSINIT(sched_setup, SI_SUB_RUN_QUEUE, SI_ORDER_FIRST, sched_setup, NULL)

static SYSCTL_NODE(_kern, OID_AUTO, sched, CTLFLAG_RW, 0, "SCHED");

static int sched_strict;
SYSCTL_INT(_kern_sched, OID_AUTO, strict, CTLFLAG_RD, &sched_strict, 0, "");

static int slice_min = 1;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_min, CTLFLAG_RW, &slice_min, 0, "");

static int slice_max = 10;
SYSCTL_INT(_kern_sched, OID_AUTO, slice_max, CTLFLAG_RW, &slice_max, 0, "");

int realstathz;
int tickincr = 1;

#ifdef SMP
/* Callout to handle load balancing SMP systems. */
static struct callout kseq_lb_callout;
#endif

/*
 * These datastructures are allocated within their parent datastructure but
 * are scheduler specific.
 */

struct ke_sched {
	int		ske_slice;
	struct runq	*ske_runq;
	/* The following variables are only used for pctcpu calculation */
	int		ske_ltick;	/* Last tick that we were running on */
	int		ske_ftick;	/* First tick that we were running on */
	int		ske_ticks;	/* Tick count */
	/* CPU that we have affinity for. */
	u_char		ske_cpu;
};
#define	ke_slice	ke_sched->ske_slice
#define	ke_runq		ke_sched->ske_runq
#define	ke_ltick	ke_sched->ske_ltick
#define	ke_ftick	ke_sched->ske_ftick
#define	ke_ticks	ke_sched->ske_ticks
#define	ke_cpu		ke_sched->ske_cpu
#define	ke_assign	ke_procq.tqe_next

#define	KEF_ASSIGNED	KEF_SCHED0	/* KSE is being migrated. */

struct kg_sched {
	int	skg_slptime;		/* Number of ticks we vol. slept */
	int	skg_runtime;		/* Number of ticks we were running */
};
#define	kg_slptime	kg_sched->skg_slptime
#define	kg_runtime	kg_sched->skg_runtime

struct td_sched {
	int	std_slptime;
};
#define	td_slptime	td_sched->std_slptime

struct td_sched td_sched;
struct ke_sched ke_sched;
struct kg_sched kg_sched;

struct ke_sched *kse0_sched = &ke_sched;
struct kg_sched *ksegrp0_sched = &kg_sched;
struct p_sched *proc0_sched = NULL;
struct td_sched *thread0_sched = &td_sched;

/*
 * The priority is primarily determined by the interactivity score.  Thus, we
 * give lower(better) priorities to kse groups that use less CPU.  The nice
 * value is then directly added to this to allow nice to have some effect
 * on latency.
 *
 * PRI_RANGE:	Total priority range for timeshare threads.
 * PRI_NRESV:	Number of nice values.
 * PRI_BASE:	The start of the dynamic range.
 */
#define	SCHED_PRI_RANGE		(PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define	SCHED_PRI_NRESV		PRIO_TOTAL
#define	SCHED_PRI_NHALF		(PRIO_TOTAL / 2)
#define	SCHED_PRI_NTHRESH	(SCHED_PRI_NHALF - 1)
#define	SCHED_PRI_BASE		(PRI_MIN_TIMESHARE)
#define	SCHED_PRI_INTERACT(score)					\
    ((score) * SCHED_PRI_RANGE / SCHED_INTERACT_MAX)

/*
 * These determine the interactivity of a process.
 *
 * SLP_RUN_MAX:	Maximum amount of sleep time + run time we'll accumulate
 *		before throttling back.
 * SLP_RUN_FORK:	Maximum slp+run time to inherit at fork time.
 * INTERACT_MAX:	Maximum interactivity value.  Smaller is better.
 * INTERACT_THRESH:	Threshhold for placement on the current runq.
 */
#define	SCHED_SLP_RUN_MAX	((hz * 5) << 10)
#define	SCHED_SLP_RUN_FORK	((hz / 2) << 10)
#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)
#define	SCHED_INTERACT_THRESH	(30)

/*
 * These parameters and macros determine the size of the time slice that is
 * granted to each thread.
 *
 * SLICE_MIN:	Minimum time slice granted, in units of ticks.
 * SLICE_MAX:	Maximum time slice granted.
 * SLICE_RANGE:	Range of available time slices scaled by hz.
 * SLICE_SCALE:	The number slices granted per val in the range of [0, max].
 * SLICE_NICE:  Determine the amount of slice granted to a scaled nice.
 */
#define	SCHED_SLICE_MIN			(slice_min)
#define	SCHED_SLICE_MAX			(slice_max)
#define	SCHED_SLICE_RANGE		(SCHED_SLICE_MAX - SCHED_SLICE_MIN + 1)
#define	SCHED_SLICE_SCALE(val, max)	(((val) * SCHED_SLICE_RANGE) / (max))
#define	SCHED_SLICE_NICE(nice)						\
    (SCHED_SLICE_MAX - SCHED_SLICE_SCALE((nice), SCHED_PRI_NTHRESH))

/*
 * This macro determines whether or not the kse belongs on the current or
 * next run queue.
 * 
 * XXX nice value should effect how interactive a kg is.
 */
#define	SCHED_INTERACTIVE(kg)						\
    (sched_interact_score(kg) < SCHED_INTERACT_THRESH)
#define	SCHED_CURR(kg, ke)						\
    (ke->ke_thread->td_priority != kg->kg_user_pri ||			\
    SCHED_INTERACTIVE(kg))

/*
 * Cpu percentage computation macros and defines.
 *
 * SCHED_CPU_TIME:	Number of seconds to average the cpu usage across.
 * SCHED_CPU_TICKS:	Number of hz ticks to average the cpu usage across.
 */

#define	SCHED_CPU_TIME	10
#define	SCHED_CPU_TICKS	(hz * SCHED_CPU_TIME)

/*
 * kseq - per processor runqs and statistics.
 */

#define	KSEQ_NCLASS	(PRI_IDLE + 1)	/* Number of run classes. */

struct kseq {
	struct runq	ksq_idle;		/* Queue of IDLE threads. */
	struct runq	ksq_timeshare[2];	/* Run queues for !IDLE. */
	struct runq	*ksq_next;		/* Next timeshare queue. */
	struct runq	*ksq_curr;		/* Current queue. */
	int		ksq_loads[KSEQ_NCLASS];	/* Load for each class */
	int		ksq_load;		/* Aggregate load. */
	short		ksq_nice[PRIO_TOTAL + 1]; /* KSEs in each nice bin. */
	short		ksq_nicemin;		/* Least nice. */
#ifdef SMP
	unsigned int	ksq_rslices;	/* Slices on run queue */
	int		ksq_cpus;	/* Count of CPUs in this kseq. */
	struct kse 	*ksq_assigned;	/* KSEs assigned by another CPU. */
#endif
};

/*
 * One kse queue per processor.
 */
#ifdef SMP
static int kseq_idle;
static struct kseq	kseq_cpu[MAXCPU];
static struct kseq	*kseq_idmap[MAXCPU];
#define	KSEQ_SELF()	(kseq_idmap[PCPU_GET(cpuid)])
#define	KSEQ_CPU(x)	(kseq_idmap[(x)])
#else
static struct kseq	kseq_cpu;
#define	KSEQ_SELF()	(&kseq_cpu)
#define	KSEQ_CPU(x)	(&kseq_cpu)
#endif

static void sched_slice(struct kse *ke);
static void sched_priority(struct ksegrp *kg);
static int sched_interact_score(struct ksegrp *kg);
static void sched_interact_update(struct ksegrp *kg);
static void sched_interact_fork(struct ksegrp *kg);
static void sched_pctcpu_update(struct kse *ke);

/* Operations on per processor queues */
static struct kse * kseq_choose(struct kseq *kseq);
static void kseq_setup(struct kseq *kseq);
static void kseq_add(struct kseq *kseq, struct kse *ke);
static void kseq_rem(struct kseq *kseq, struct kse *ke);
static void kseq_nice_add(struct kseq *kseq, int nice);
static void kseq_nice_rem(struct kseq *kseq, int nice);
void kseq_print(int cpu);
#ifdef SMP
#if 0
static int sched_pickcpu(void);
#endif
static struct kse *runq_steal(struct runq *rq);
static struct kseq *kseq_load_highest(void);
static void kseq_balance(void *arg);
static void kseq_move(struct kseq *from, int cpu);
static int kseq_find(void);
static void kseq_notify(struct kse *ke, int cpu);
static void kseq_assign(struct kseq *);
static struct kse *kseq_steal(struct kseq *kseq);
#endif

void
kseq_print(int cpu)
{
	struct kseq *kseq;
	int i;

	kseq = KSEQ_CPU(cpu);

	printf("kseq:\n");
	printf("\tload:           %d\n", kseq->ksq_load);
	printf("\tload ITHD:      %d\n", kseq->ksq_loads[PRI_ITHD]);
	printf("\tload REALTIME:  %d\n", kseq->ksq_loads[PRI_REALTIME]);
	printf("\tload TIMESHARE: %d\n", kseq->ksq_loads[PRI_TIMESHARE]);
	printf("\tload IDLE:      %d\n", kseq->ksq_loads[PRI_IDLE]);
	printf("\tnicemin:\t%d\n", kseq->ksq_nicemin);
	printf("\tnice counts:\n");
	for (i = 0; i < PRIO_TOTAL + 1; i++)
		if (kseq->ksq_nice[i])
			printf("\t\t%d = %d\n",
			    i - SCHED_PRI_NHALF, kseq->ksq_nice[i]);
}

static void
kseq_add(struct kseq *kseq, struct kse *ke)
{
	mtx_assert(&sched_lock, MA_OWNED);
	kseq->ksq_loads[PRI_BASE(ke->ke_ksegrp->kg_pri_class)]++;
	kseq->ksq_load++;
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
	CTR6(KTR_ULE, "Add kse %p to %p (slice: %d, pri: %d, nice: %d(%d))",
	    ke, ke->ke_runq, ke->ke_slice, ke->ke_thread->td_priority,
	    ke->ke_ksegrp->kg_nice, kseq->ksq_nicemin);
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
		kseq_nice_add(kseq, ke->ke_ksegrp->kg_nice);
#ifdef SMP
	kseq->ksq_rslices += ke->ke_slice;
#endif
}

static void
kseq_rem(struct kseq *kseq, struct kse *ke)
{
	mtx_assert(&sched_lock, MA_OWNED);
	kseq->ksq_loads[PRI_BASE(ke->ke_ksegrp->kg_pri_class)]--;
	kseq->ksq_load--;
	ke->ke_runq = NULL;
	if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE)
		kseq_nice_rem(kseq, ke->ke_ksegrp->kg_nice);
#ifdef SMP
	kseq->ksq_rslices -= ke->ke_slice;
#endif
}

static void
kseq_nice_add(struct kseq *kseq, int nice)
{
	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	kseq->ksq_nice[nice + SCHED_PRI_NHALF]++;
	if (nice < kseq->ksq_nicemin || kseq->ksq_loads[PRI_TIMESHARE] == 1)
		kseq->ksq_nicemin = nice;
}

static void
kseq_nice_rem(struct kseq *kseq, int nice) 
{
	int n;

	mtx_assert(&sched_lock, MA_OWNED);
	/* Normalize to zero. */
	n = nice + SCHED_PRI_NHALF;
	kseq->ksq_nice[n]--;
	KASSERT(kseq->ksq_nice[n] >= 0, ("Negative nice count."));

	/*
	 * If this wasn't the smallest nice value or there are more in
	 * this bucket we can just return.  Otherwise we have to recalculate
	 * the smallest nice.
	 */
	if (nice != kseq->ksq_nicemin ||
	    kseq->ksq_nice[n] != 0 ||
	    kseq->ksq_loads[PRI_TIMESHARE] == 0)
		return;

	for (; n < SCHED_PRI_NRESV + 1; n++)
		if (kseq->ksq_nice[n]) {
			kseq->ksq_nicemin = n - SCHED_PRI_NHALF;
			return;
		}
}

#ifdef SMP
/*
 * kseq_balance is a simple CPU load balancing algorithm.  It operates by
 * finding the least loaded and most loaded cpu and equalizing their load
 * by migrating some processes.
 *
 * Dealing only with two CPUs at a time has two advantages.  Firstly, most
 * installations will only have 2 cpus.  Secondly, load balancing too much at
 * once can have an unpleasant effect on the system.  The scheduler rarely has
 * enough information to make perfect decisions.  So this algorithm chooses
 * algorithm simplicity and more gradual effects on load in larger systems.
 *
 * It could be improved by considering the priorities and slices assigned to
 * each task prior to balancing them.  There are many pathological cases with
 * any approach and so the semi random algorithm below may work as well as any.
 *
 */
static void
kseq_balance(void *arg)
{
	struct kseq *kseq;
	int high_load;
	int low_load;
	int high_cpu;
	int low_cpu;
	int move;
	int diff;
	int i;

	high_cpu = 0;
	low_cpu = 0;
	high_load = 0;
	low_load = -1;

	mtx_lock_spin(&sched_lock);
	if (smp_started == 0)
		goto out;

	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i) || (i & stopped_cpus) != 0)
			continue;
		kseq = KSEQ_CPU(i);
		if (kseq->ksq_load > high_load) {
			high_load = kseq->ksq_load;
			high_cpu = i;
		}
		if (low_load == -1 || kseq->ksq_load < low_load) {
			low_load = kseq->ksq_load;
			low_cpu = i;
		}
	}

	kseq = KSEQ_CPU(high_cpu);

	high_load = kseq->ksq_loads[PRI_IDLE] + kseq->ksq_loads[PRI_TIMESHARE] +
	    kseq->ksq_loads[PRI_REALTIME];
	/*
	 * Nothing to do.
	 */
	if (high_load < kseq->ksq_cpus + 1)
		goto out;

	high_load -= kseq->ksq_cpus;

	if (low_load >= high_load)
		goto out;

	diff = high_load - low_load;
	move = diff / 2;
	if (diff & 0x1)
		move++;

	for (i = 0; i < move; i++)
		kseq_move(kseq, low_cpu);

out:
	mtx_unlock_spin(&sched_lock);
	callout_reset(&kseq_lb_callout, hz, kseq_balance, NULL);

	return;
}

static struct kseq *
kseq_load_highest(void)
{
	struct kseq *kseq;
	int load;
	int cpu;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);
	cpu = 0;
	load = 0;

	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i) || (i & stopped_cpus) != 0)
			continue;
		kseq = KSEQ_CPU(i);
		if (kseq->ksq_load > load) {
			load = kseq->ksq_load;
			cpu = i;
		}
	}
	kseq = KSEQ_CPU(cpu);

	if ((kseq->ksq_loads[PRI_IDLE] + kseq->ksq_loads[PRI_TIMESHARE] +
	    kseq->ksq_loads[PRI_REALTIME]) > kseq->ksq_cpus)
		return (kseq);

	return (NULL);
}

static void
kseq_move(struct kseq *from, int cpu)
{
	struct kse *ke;

	ke = kseq_steal(from);
	runq_remove(ke->ke_runq, ke);
	ke->ke_state = KES_THREAD;
	kseq_rem(from, ke);

	ke->ke_cpu = cpu;
	sched_add(ke->ke_thread);
}

static int
kseq_find(void)
{
	struct kseq *high;

	if (!smp_started)
		return (0);
	if (kseq_idle & PCPU_GET(cpumask))
		return (0);
	/*
	 * Find the cpu with the highest load and steal one proc.
	 */
	if ((high = kseq_load_highest()) == NULL ||
	    high == KSEQ_SELF()) {
		/*
		 * If we couldn't find one, set ourselves in the
		 * idle map.
		 */
		atomic_set_int(&kseq_idle, PCPU_GET(cpumask));
		return (0);
	}
	/*
	 * Remove this kse from this kseq and runq and then requeue
	 * on the current processor.  We now have a load of one!
	 */
	kseq_move(high, PCPU_GET(cpuid));

	return (1);
}

static void
kseq_assign(struct kseq *kseq)
{
	struct kse *nke;
	struct kse *ke;

	do {
		ke = kseq->ksq_assigned;
	} while(!atomic_cmpset_ptr(&kseq->ksq_assigned, ke, NULL));
	for (; ke != NULL; ke = nke) {
		nke = ke->ke_assign;
		ke->ke_flags &= ~KEF_ASSIGNED;
		sched_add(ke->ke_thread);
	}
}

static void
kseq_notify(struct kse *ke, int cpu)
{
	struct kseq *kseq;
	struct thread *td;
	struct pcpu *pcpu;

	ke->ke_flags |= KEF_ASSIGNED;

	kseq = KSEQ_CPU(cpu);

	/*
	 * Place a KSE on another cpu's queue and force a resched.
	 */
	do {
		ke->ke_assign = kseq->ksq_assigned;
	} while(!atomic_cmpset_ptr(&kseq->ksq_assigned, ke->ke_assign, ke));
	pcpu = pcpu_find(cpu);
	td = pcpu->pc_curthread;
	if (ke->ke_thread->td_priority < td->td_priority ||
	    td == pcpu->pc_idlethread) {
		td->td_flags |= TDF_NEEDRESCHED;
		ipi_selected(1 << cpu, IPI_AST);
	}
}

static struct kse *
runq_steal(struct runq *rq)
{
	struct rqhead *rqh;
	struct rqbits *rqb;
	struct kse *ke;
	int word;
	int bit;

	mtx_assert(&sched_lock, MA_OWNED);
	rqb = &rq->rq_status;
	for (word = 0; word < RQB_LEN; word++) {
		if (rqb->rqb_bits[word] == 0)
			continue;
		for (bit = 0; bit < RQB_BPW; bit++) {
			if ((rqb->rqb_bits[word] & (1 << bit)) == 0)
				continue;
			rqh = &rq->rq_queues[bit + (word << RQB_L2BPW)];
			TAILQ_FOREACH(ke, rqh, ke_procq) {
				if (PRI_BASE(ke->ke_ksegrp->kg_pri_class) !=
				    PRI_ITHD)
					return (ke);
			}
		}
	}
	return (NULL);
}

static struct kse *
kseq_steal(struct kseq *kseq)
{
	struct kse *ke;

	if ((ke = runq_steal(kseq->ksq_curr)) != NULL)
		return (ke);
	if ((ke = runq_steal(kseq->ksq_next)) != NULL)
		return (ke);
	return (runq_steal(&kseq->ksq_idle));
}
#endif	/* SMP */

/*
 * Pick the highest priority task we have and return it.
 */

static struct kse *
kseq_choose(struct kseq *kseq)
{
	struct kse *ke;
	struct runq *swap;

	mtx_assert(&sched_lock, MA_OWNED);
	swap = NULL;

	for (;;) {
		ke = runq_choose(kseq->ksq_curr);
		if (ke == NULL) {
			/*
			 * We already swaped once and didn't get anywhere.
			 */
			if (swap)
				break;
			swap = kseq->ksq_curr;
			kseq->ksq_curr = kseq->ksq_next;
			kseq->ksq_next = swap;
			continue;
		}
		/*
		 * If we encounter a slice of 0 the kse is in a
		 * TIMESHARE kse group and its nice was too far out
		 * of the range that receives slices. 
		 */
		if (ke->ke_slice == 0) {
			runq_remove(ke->ke_runq, ke);
			sched_slice(ke);
			ke->ke_runq = kseq->ksq_next;
			runq_add(ke->ke_runq, ke);
			continue;
		}
		return (ke);
	}

	return (runq_choose(&kseq->ksq_idle));
}

static void
kseq_setup(struct kseq *kseq)
{
	runq_init(&kseq->ksq_timeshare[0]);
	runq_init(&kseq->ksq_timeshare[1]);
	runq_init(&kseq->ksq_idle);

	kseq->ksq_curr = &kseq->ksq_timeshare[0];
	kseq->ksq_next = &kseq->ksq_timeshare[1];

	kseq->ksq_loads[PRI_ITHD] = 0;
	kseq->ksq_loads[PRI_REALTIME] = 0;
	kseq->ksq_loads[PRI_TIMESHARE] = 0;
	kseq->ksq_loads[PRI_IDLE] = 0;
	kseq->ksq_load = 0;
#ifdef SMP
	kseq->ksq_rslices = 0;
	kseq->ksq_assigned = NULL;
#endif
}

static void
sched_setup(void *dummy)
{
#ifdef SMP
	int i;
#endif

	slice_min = (hz/100);	/* 10ms */
	slice_max = (hz/7);	/* ~140ms */

#ifdef SMP
	/* init kseqs */
	/* Create the idmap. */
#ifdef ULE_HTT_EXPERIMENTAL
	if (smp_topology == NULL) {
#else
	if (1) {
#endif
		for (i = 0; i < MAXCPU; i++) {
			kseq_setup(&kseq_cpu[i]);
			kseq_idmap[i] = &kseq_cpu[i];
			kseq_cpu[i].ksq_cpus = 1;
		}
	} else {
		int j;

		for (i = 0; i < smp_topology->ct_count; i++) {
			struct cpu_group *cg;

			cg = &smp_topology->ct_group[i];
			kseq_setup(&kseq_cpu[i]);

			for (j = 0; j < MAXCPU; j++)
				if ((cg->cg_mask & (1 << j)) != 0)
					kseq_idmap[j] = &kseq_cpu[i];
			kseq_cpu[i].ksq_cpus = cg->cg_count;
		}
	}
	callout_init(&kseq_lb_callout, CALLOUT_MPSAFE);
	kseq_balance(NULL);
#else
	kseq_setup(KSEQ_SELF());
#endif
	mtx_lock_spin(&sched_lock);
	kseq_add(KSEQ_SELF(), &kse0);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Scale the scheduling priority according to the "interactivity" of this
 * process.
 */
static void
sched_priority(struct ksegrp *kg)
{
	int pri;

	if (kg->kg_pri_class != PRI_TIMESHARE)
		return;

	pri = SCHED_PRI_INTERACT(sched_interact_score(kg));
	pri += SCHED_PRI_BASE;
	pri += kg->kg_nice;

	if (pri > PRI_MAX_TIMESHARE)
		pri = PRI_MAX_TIMESHARE;
	else if (pri < PRI_MIN_TIMESHARE)
		pri = PRI_MIN_TIMESHARE;

	kg->kg_user_pri = pri;

	return;
}

/*
 * Calculate a time slice based on the properties of the kseg and the runq
 * that we're on.  This is only for PRI_TIMESHARE ksegrps.
 */
static void
sched_slice(struct kse *ke)
{
	struct kseq *kseq;
	struct ksegrp *kg;

	kg = ke->ke_ksegrp;
	kseq = KSEQ_CPU(ke->ke_cpu);

	/*
	 * Rationale:
	 * KSEs in interactive ksegs get the minimum slice so that we
	 * quickly notice if it abuses its advantage.
	 *
	 * KSEs in non-interactive ksegs are assigned a slice that is
	 * based on the ksegs nice value relative to the least nice kseg
	 * on the run queue for this cpu.
	 *
	 * If the KSE is less nice than all others it gets the maximum
	 * slice and other KSEs will adjust their slice relative to
	 * this when they first expire.
	 *
	 * There is 20 point window that starts relative to the least
	 * nice kse on the run queue.  Slice size is determined by
	 * the kse distance from the last nice ksegrp.
	 *
	 * If you are outside of the window you will get no slice and
	 * you will be reevaluated each time you are selected on the
	 * run queue.
	 *	
	 */

	if (!SCHED_INTERACTIVE(kg)) {
		int nice;

		nice = kg->kg_nice + (0 - kseq->ksq_nicemin);
		if (kseq->ksq_loads[PRI_TIMESHARE] == 0 ||
		    kg->kg_nice < kseq->ksq_nicemin)
			ke->ke_slice = SCHED_SLICE_MAX;
		else if (nice <= SCHED_PRI_NTHRESH)
			ke->ke_slice = SCHED_SLICE_NICE(nice);
		else
			ke->ke_slice = 0;
	} else
		ke->ke_slice = SCHED_SLICE_MIN;

	CTR6(KTR_ULE,
	    "Sliced %p(%d) (nice: %d, nicemin: %d, load: %d, interactive: %d)",
	    ke, ke->ke_slice, kg->kg_nice, kseq->ksq_nicemin,
	    kseq->ksq_loads[PRI_TIMESHARE], SCHED_INTERACTIVE(kg));

	return;
}

/*
 * This routine enforces a maximum limit on the amount of scheduling history
 * kept.  It is called after either the slptime or runtime is adjusted.
 * This routine will not operate correctly when slp or run times have been
 * adjusted to more than double their maximum.
 */
static void
sched_interact_update(struct ksegrp *kg)
{
	int sum;

	sum = kg->kg_runtime + kg->kg_slptime;
	if (sum < SCHED_SLP_RUN_MAX)
		return;
	/*
	 * If we have exceeded by more than 1/5th then the algorithm below
	 * will not bring us back into range.  Dividing by two here forces
	 * us into the range of [3/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
	 */
	if (sum > (SCHED_INTERACT_MAX / 5) * 6) {
		kg->kg_runtime /= 2;
		kg->kg_slptime /= 2;
		return;
	}
	kg->kg_runtime = (kg->kg_runtime / 5) * 4;
	kg->kg_slptime = (kg->kg_slptime / 5) * 4;
}

static void
sched_interact_fork(struct ksegrp *kg)
{
	int ratio;
	int sum;

	sum = kg->kg_runtime + kg->kg_slptime;
	if (sum > SCHED_SLP_RUN_FORK) {
		ratio = sum / SCHED_SLP_RUN_FORK;
		kg->kg_runtime /= ratio;
		kg->kg_slptime /= ratio;
	}
}

static int
sched_interact_score(struct ksegrp *kg)
{
	int div;

	if (kg->kg_runtime > kg->kg_slptime) {
		div = max(1, kg->kg_runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF +
		    (SCHED_INTERACT_HALF - (kg->kg_slptime / div)));
	} if (kg->kg_slptime > kg->kg_runtime) {
		div = max(1, kg->kg_slptime / SCHED_INTERACT_HALF);
		return (kg->kg_runtime / div);
	}

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

}

/*
 * This is only somewhat accurate since given many processes of the same
 * priority they will switch when their slices run out, which will be
 * at most SCHED_SLICE_MAX.
 */
int
sched_rr_interval(void)
{
	return (SCHED_SLICE_MAX);
}

static void
sched_pctcpu_update(struct kse *ke)
{
	/*
	 * Adjust counters and watermark for pctcpu calc.
	 */
	if (ke->ke_ltick > ticks - SCHED_CPU_TICKS) {
		/*
		 * Shift the tick count out so that the divide doesn't
		 * round away our results.
		 */
		ke->ke_ticks <<= 10;
		ke->ke_ticks = (ke->ke_ticks / (ticks - ke->ke_ftick)) *
			    SCHED_CPU_TICKS;
		ke->ke_ticks >>= 10;
	} else
		ke->ke_ticks = 0;
	ke->ke_ltick = ticks;
	ke->ke_ftick = ke->ke_ltick - SCHED_CPU_TICKS;
}

#if 0
/* XXX Should be changed to kseq_load_lowest() */
int
sched_pickcpu(void)
{
	struct kseq *kseq;
	int load;
	int cpu;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);
	if (!smp_started)
		return (0);

	load = 0;
	cpu = 0;

	for (i = 0; i < mp_maxid; i++) {
		if (CPU_ABSENT(i) || (i & stopped_cpus) != 0)
			continue;
		kseq = KSEQ_CPU(i);
		if (kseq->ksq_load < load) {
			cpu = i;
			load = kseq->ksq_load;
		}
	}

	CTR1(KTR_ULE, "sched_pickcpu: %d", cpu);
	return (cpu);
}
#endif

void
sched_prio(struct thread *td, u_char prio)
{
	struct kse *ke;

	ke = td->td_kse;
	mtx_assert(&sched_lock, MA_OWNED);
	if (TD_ON_RUNQ(td)) {
		/*
		 * If the priority has been elevated due to priority
		 * propagation, we may have to move ourselves to a new
		 * queue.  We still call adjustrunqueue below in case kse
		 * needs to fix things up.
		 */
		if (ke && (ke->ke_flags & KEF_ASSIGNED) == 0 &&
		    ke->ke_runq != KSEQ_CPU(ke->ke_cpu)->ksq_curr) {
			runq_remove(ke->ke_runq, ke);
			ke->ke_runq = KSEQ_CPU(ke->ke_cpu)->ksq_curr;
			runq_add(ke->ke_runq, ke);
		}
		adjustrunqueue(td, prio);
	} else
		td->td_priority = prio;
}

void
sched_switch(struct thread *td)
{
	struct thread *newtd;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);

	ke = td->td_kse;

	td->td_last_kse = ke;
        td->td_lastcpu = td->td_oncpu;
	td->td_oncpu = NOCPU;
        td->td_flags &= ~TDF_NEEDRESCHED;

	if (TD_IS_RUNNING(td)) {
		if (td->td_proc->p_flag & P_SA) {
			kseq_rem(KSEQ_CPU(ke->ke_cpu), ke);
			setrunqueue(td);
		} else {
			/*
			 * This queue is always correct except for idle threads
			 * which have a higher priority due to priority
			 * propagation.
			 */
			if (ke->ke_ksegrp->kg_pri_class == PRI_IDLE) {
				if (td->td_priority < PRI_MIN_IDLE)
					ke->ke_runq = KSEQ_SELF()->ksq_curr;
				else
					ke->ke_runq = &KSEQ_SELF()->ksq_idle;
			}
			runq_add(ke->ke_runq, ke);
			/* setrunqueue(td); */
		}
	} else {
		if (ke->ke_runq)
			kseq_rem(KSEQ_CPU(ke->ke_cpu), ke);
		/*
		 * We will not be on the run queue. So we must be
		 * sleeping or similar.
		 */
		if (td->td_proc->p_flag & P_SA)
			kse_reassign(ke);
	}
	newtd = choosethread();
	if (td != newtd)
		cpu_switch(td, newtd);
	sched_lock.mtx_lock = (uintptr_t)td;

	td->td_oncpu = PCPU_GET(cpuid);
}

void
sched_nice(struct ksegrp *kg, int nice)
{
	struct kse *ke;
	struct thread *td;
	struct kseq *kseq;

	PROC_LOCK_ASSERT(kg->kg_proc, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	/*
	 * We need to adjust the nice counts for running KSEs.
	 */
	if (kg->kg_pri_class == PRI_TIMESHARE)
		FOREACH_KSE_IN_GROUP(kg, ke) {
			if (ke->ke_runq == NULL)
				continue;
			kseq = KSEQ_CPU(ke->ke_cpu);
			kseq_nice_rem(kseq, kg->kg_nice);
			kseq_nice_add(kseq, nice);
		}
	kg->kg_nice = nice;
	sched_priority(kg);
	FOREACH_THREAD_IN_GROUP(kg, td)
		td->td_flags |= TDF_NEEDRESCHED;
}

void
sched_sleep(struct thread *td, u_char prio)
{
	mtx_assert(&sched_lock, MA_OWNED);

	td->td_slptime = ticks;
	td->td_priority = prio;

	CTR2(KTR_ULE, "sleep kse %p (tick: %d)",
	    td->td_kse, td->td_slptime);
}

void
sched_wakeup(struct thread *td)
{
	mtx_assert(&sched_lock, MA_OWNED);

	/*
	 * Let the kseg know how long we slept for.  This is because process
	 * interactivity behavior is modeled in the kseg.
	 */
	if (td->td_slptime) {
		struct ksegrp *kg;
		int hzticks;

		kg = td->td_ksegrp;
		hzticks = (ticks - td->td_slptime) << 10;
		if (hzticks >= SCHED_SLP_RUN_MAX) {
			kg->kg_slptime = SCHED_SLP_RUN_MAX;
			kg->kg_runtime = 1;
		} else {
			kg->kg_slptime += hzticks;
			sched_interact_update(kg);
		}
		sched_priority(kg);
		if (td->td_kse)
			sched_slice(td->td_kse);
		CTR2(KTR_ULE, "wakeup kse %p (%d ticks)",
		    td->td_kse, hzticks);
		td->td_slptime = 0;
	}
	setrunqueue(td);
}

/*
 * Penalize the parent for creating a new child and initialize the child's
 * priority.
 */
void
sched_fork(struct proc *p, struct proc *p1)
{

	mtx_assert(&sched_lock, MA_OWNED);

	sched_fork_ksegrp(FIRST_KSEGRP_IN_PROC(p), FIRST_KSEGRP_IN_PROC(p1));
	sched_fork_kse(FIRST_KSE_IN_PROC(p), FIRST_KSE_IN_PROC(p1));
	sched_fork_thread(FIRST_THREAD_IN_PROC(p), FIRST_THREAD_IN_PROC(p1));
}

void
sched_fork_kse(struct kse *ke, struct kse *child)
{

	child->ke_slice = 1;	/* Attempt to quickly learn interactivity. */
	child->ke_cpu = ke->ke_cpu; /* sched_pickcpu(); */
	child->ke_runq = NULL;

	/* Grab our parents cpu estimation information. */
	child->ke_ticks = ke->ke_ticks;
	child->ke_ltick = ke->ke_ltick;
	child->ke_ftick = ke->ke_ftick;
}

void
sched_fork_ksegrp(struct ksegrp *kg, struct ksegrp *child)
{
	PROC_LOCK_ASSERT(child->kg_proc, MA_OWNED);

	child->kg_slptime = kg->kg_slptime;
	child->kg_runtime = kg->kg_runtime;
	child->kg_user_pri = kg->kg_user_pri;
	child->kg_nice = kg->kg_nice;
	sched_interact_fork(child);
	kg->kg_runtime += tickincr << 10;
	sched_interact_update(kg);

	CTR6(KTR_ULE, "sched_fork_ksegrp: %d(%d, %d) - %d(%d, %d)",
	    kg->kg_proc->p_pid, kg->kg_slptime, kg->kg_runtime, 
	    child->kg_proc->p_pid, child->kg_slptime, child->kg_runtime);
}

void
sched_fork_thread(struct thread *td, struct thread *child)
{
}

void
sched_class(struct ksegrp *kg, int class)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	if (kg->kg_pri_class == class)
		return;

	FOREACH_KSE_IN_GROUP(kg, ke) {
		if (ke->ke_state != KES_ONRUNQ &&
		    ke->ke_state != KES_THREAD)
			continue;
		kseq = KSEQ_CPU(ke->ke_cpu);

		kseq->ksq_loads[PRI_BASE(kg->kg_pri_class)]--;
		kseq->ksq_loads[PRI_BASE(class)]++;

		if (kg->kg_pri_class == PRI_TIMESHARE)
			kseq_nice_rem(kseq, kg->kg_nice);
		else if (class == PRI_TIMESHARE)
			kseq_nice_add(kseq, kg->kg_nice);
	}

	kg->kg_pri_class = class;
}

/*
 * Return some of the child's priority and interactivity to the parent.
 */
void
sched_exit(struct proc *p, struct proc *child)
{
	mtx_assert(&sched_lock, MA_OWNED);
	sched_exit_kse(FIRST_KSE_IN_PROC(p), FIRST_KSE_IN_PROC(child));
	sched_exit_ksegrp(FIRST_KSEGRP_IN_PROC(p), FIRST_KSEGRP_IN_PROC(child));
}

void
sched_exit_kse(struct kse *ke, struct kse *child)
{
	kseq_rem(KSEQ_CPU(child->ke_cpu), child);
}

void
sched_exit_ksegrp(struct ksegrp *kg, struct ksegrp *child)
{
	/* kg->kg_slptime += child->kg_slptime; */
	kg->kg_runtime += child->kg_runtime;
	sched_interact_update(kg);
}

void
sched_exit_thread(struct thread *td, struct thread *child)
{
}

void
sched_clock(struct thread *td)
{
	struct kseq *kseq;
	struct ksegrp *kg;
	struct kse *ke;

	/*
	 * sched_setup() apparently happens prior to stathz being set.  We
	 * need to resolve the timers earlier in the boot so we can avoid
	 * calculating this here.
	 */
	if (realstathz == 0) {
		realstathz = stathz ? stathz : hz;
		tickincr = hz / realstathz;
		/*
		 * XXX This does not work for values of stathz that are much
		 * larger than hz.
		 */
		if (tickincr == 0)
			tickincr = 1;
	}

	ke = td->td_kse;
	kg = ke->ke_ksegrp;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((td != NULL), ("schedclock: null thread pointer"));

	/* Adjust ticks for pctcpu */
	ke->ke_ticks++;
	ke->ke_ltick = ticks;

	/* Go up to one second beyond our max and then trim back down */
	if (ke->ke_ftick + SCHED_CPU_TICKS + hz < ke->ke_ltick)
		sched_pctcpu_update(ke);

	if (td->td_flags & TDF_IDLETD)
		return;

	CTR4(KTR_ULE, "Tick kse %p (slice: %d, slptime: %d, runtime: %d)",
	    ke, ke->ke_slice, kg->kg_slptime >> 10, kg->kg_runtime >> 10);
	/*
	 * We only do slicing code for TIMESHARE ksegrps.
	 */
	if (kg->kg_pri_class != PRI_TIMESHARE)
		return;
	/*
	 * We used a tick charge it to the ksegrp so that we can compute our
	 * interactivity.
	 */
	kg->kg_runtime += tickincr << 10;
	sched_interact_update(kg);

	/*
	 * We used up one time slice.
	 */
	ke->ke_slice--;
	kseq = KSEQ_SELF();
#ifdef SMP
	kseq->ksq_rslices--;
#endif

	if (ke->ke_slice > 0)
		return;
	/*
	 * We're out of time, recompute priorities and requeue.
	 */
	kseq_rem(kseq, ke);
	sched_priority(kg);
	sched_slice(ke);
	if (SCHED_CURR(kg, ke))
		ke->ke_runq = kseq->ksq_curr;
	else
		ke->ke_runq = kseq->ksq_next;
	kseq_add(kseq, ke);
	td->td_flags |= TDF_NEEDRESCHED;
}

int
sched_runnable(void)
{
	struct kseq *kseq;
	int load;

	load = 1;

	mtx_lock_spin(&sched_lock);
	kseq = KSEQ_SELF();
#ifdef SMP
	if (kseq->ksq_assigned)
		kseq_assign(kseq);
#endif
	if ((curthread->td_flags & TDF_IDLETD) != 0) {
		if (kseq->ksq_load > 0)
			goto out;
	} else
		if (kseq->ksq_load - 1 > 0)
			goto out;
	load = 0;
out:
	mtx_unlock_spin(&sched_lock);
	return (load);
}

void
sched_userret(struct thread *td)
{
	struct ksegrp *kg;

	kg = td->td_ksegrp;
	
	if (td->td_priority != kg->kg_user_pri) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = kg->kg_user_pri;
		mtx_unlock_spin(&sched_lock);
	}
}

struct kse *
sched_choose(void)
{
	struct kseq *kseq;
	struct kse *ke;

	mtx_assert(&sched_lock, MA_OWNED);
	kseq = KSEQ_SELF();
#ifdef SMP
retry:
	if (kseq->ksq_assigned)
		kseq_assign(kseq);
#endif
	ke = kseq_choose(kseq);
	if (ke) {
#ifdef SMP
		if (ke->ke_ksegrp->kg_pri_class == PRI_IDLE)
			if (kseq_find())
				goto retry;
#endif
		runq_remove(ke->ke_runq, ke);
		ke->ke_state = KES_THREAD;

		if (ke->ke_ksegrp->kg_pri_class == PRI_TIMESHARE) {
			CTR4(KTR_ULE, "Run kse %p from %p (slice: %d, pri: %d)",
			    ke, ke->ke_runq, ke->ke_slice,
			    ke->ke_thread->td_priority);
		}
		return (ke);
	}
#ifdef SMP
	if (kseq_find())
		goto retry;
#endif

	return (NULL);
}

void
sched_add(struct thread *td)
{
	struct kseq *kseq;
	struct ksegrp *kg;
	struct kse *ke;
	int class;

	mtx_assert(&sched_lock, MA_OWNED);
	ke = td->td_kse;
	kg = td->td_ksegrp;
	if (ke->ke_flags & KEF_ASSIGNED)
		return;
	kseq = KSEQ_SELF();
	KASSERT((ke->ke_thread != NULL), ("sched_add: No thread on KSE"));
	KASSERT((ke->ke_thread->td_kse != NULL),
	    ("sched_add: No KSE on thread"));
	KASSERT(ke->ke_state != KES_ONRUNQ,
	    ("sched_add: kse %p (%s) already in run queue", ke,
	    ke->ke_proc->p_comm));
	KASSERT(ke->ke_proc->p_sflag & PS_INMEM,
	    ("sched_add: process swapped out"));
	KASSERT(ke->ke_runq == NULL,
	    ("sched_add: KSE %p is still assigned to a run queue", ke));

	class = PRI_BASE(kg->kg_pri_class);
	switch (class) {
	case PRI_ITHD:
	case PRI_REALTIME:
		ke->ke_runq = kseq->ksq_curr;
		ke->ke_slice = SCHED_SLICE_MAX;
		ke->ke_cpu = PCPU_GET(cpuid);
		break;
	case PRI_TIMESHARE:
#ifdef SMP
		if (ke->ke_cpu != PCPU_GET(cpuid)) {
			kseq_notify(ke, ke->ke_cpu);
			return;
		}
#endif
		if (SCHED_CURR(kg, ke))
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = kseq->ksq_next;
		break;
	case PRI_IDLE:
#ifdef SMP
		if (ke->ke_cpu != PCPU_GET(cpuid)) {
			kseq_notify(ke, ke->ke_cpu);
			return;
		}
#endif
		/*
		 * This is for priority prop.
		 */
		if (ke->ke_thread->td_priority < PRI_MIN_IDLE)
			ke->ke_runq = kseq->ksq_curr;
		else
			ke->ke_runq = &kseq->ksq_idle;
		ke->ke_slice = SCHED_SLICE_MIN;
		break;
	default:
		panic("Unknown pri class.");
		break;
	}
#ifdef SMP
	/*
	 * If there are any idle processors, give them our extra load.
	 */
	if (kseq_idle && class != PRI_ITHD &&
	    (kseq->ksq_loads[PRI_IDLE] + kseq->ksq_loads[PRI_TIMESHARE] +
	    kseq->ksq_loads[PRI_REALTIME]) >= kseq->ksq_cpus) {
		int cpu;

		/*
		 * Multiple cpus could find this bit simultaneously but the
		 * race shouldn't be terrible.
		 */
		cpu = ffs(kseq_idle);
		if (cpu) {
			cpu--;
			atomic_clear_int(&kseq_idle, 1 << cpu);
			ke->ke_cpu = cpu;
			ke->ke_runq = NULL;
			kseq_notify(ke, cpu);
			return;
		}
	}
	if (class == PRI_TIMESHARE || class == PRI_REALTIME)
		atomic_clear_int(&kseq_idle, PCPU_GET(cpumask));
#endif
        if (td->td_priority < curthread->td_priority)
                curthread->td_flags |= TDF_NEEDRESCHED;

	ke->ke_ksegrp->kg_runq_kses++;
	ke->ke_state = KES_ONRUNQ;

	runq_add(ke->ke_runq, ke);
	kseq_add(kseq, ke);
}

void
sched_rem(struct thread *td)
{
	struct kseq *kseq;
	struct kse *ke;

	ke = td->td_kse;
	/*
	 * It is safe to just return here because sched_rem() is only ever
	 * used in places where we're immediately going to add the
	 * kse back on again.  In that case it'll be added with the correct
	 * thread and priority when the caller drops the sched_lock.
	 */
	if (ke->ke_flags & KEF_ASSIGNED)
		return;
	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((ke->ke_state == KES_ONRUNQ), ("KSE not on run queue"));

	ke->ke_state = KES_THREAD;
	ke->ke_ksegrp->kg_runq_kses--;
	kseq = KSEQ_CPU(ke->ke_cpu);
	runq_remove(ke->ke_runq, ke);
	kseq_rem(kseq, ke);
}

fixpt_t
sched_pctcpu(struct thread *td)
{
	fixpt_t pctcpu;
	struct kse *ke;

	pctcpu = 0;
	ke = td->td_kse;
	if (ke == NULL)
		return (0);

	mtx_lock_spin(&sched_lock);
	if (ke->ke_ticks) {
		int rtick;

		/*
		 * Don't update more frequently than twice a second.  Allowing
		 * this causes the cpu usage to decay away too quickly due to
		 * rounding errors.
		 */
		if (ke->ke_ltick < (ticks - (hz / 2)))
			sched_pctcpu_update(ke);
		/* How many rtick per second ? */
		rtick = min(ke->ke_ticks / SCHED_CPU_TIME, SCHED_CPU_TICKS);
		pctcpu = (FSCALE * ((FSCALE * rtick)/realstathz)) >> FSHIFT;
	}

	ke->ke_proc->p_swtime = ke->ke_ltick - ke->ke_ftick;
	mtx_unlock_spin(&sched_lock);

	return (pctcpu);
}

int
sched_sizeof_kse(void)
{
	return (sizeof(struct kse) + sizeof(struct ke_sched));
}

int
sched_sizeof_ksegrp(void)
{
	return (sizeof(struct ksegrp) + sizeof(struct kg_sched));
}

int
sched_sizeof_proc(void)
{
	return (sizeof(struct proc));
}

int
sched_sizeof_thread(void)
{
	return (sizeof(struct thread) + sizeof(struct td_sched));
}
