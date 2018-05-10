/*-
 * Copyright (c) 2018, Matthew Macy <mmacy@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Neither the name of Matthew Macy nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/epoch.h>
#include <sys/gtaskqueue.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/turnstile.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <ck_epoch.h>

MALLOC_DEFINE(M_EPOCH, "epoch", "epoch based reclamation");

/* arbitrary --- needs benchmarking */
#define MAX_ADAPTIVE_SPIN 5000

SYSCTL_NODE(_kern, OID_AUTO, epoch, CTLFLAG_RW, 0, "epoch information");
SYSCTL_NODE(_kern_epoch, OID_AUTO, stats, CTLFLAG_RW, 0, "epoch stats");

/* Stats. */
static counter_u64_t block_count;
SYSCTL_COUNTER_U64(_kern_epoch_stats, OID_AUTO, nblocked, CTLFLAG_RW,
				   &block_count, "# of times a thread was in an epoch when epoch_wait was called");
static counter_u64_t migrate_count;
SYSCTL_COUNTER_U64(_kern_epoch_stats, OID_AUTO, migrations, CTLFLAG_RW,
				   &migrate_count, "# of times thread was migrated to another CPU in epoch_wait");
static counter_u64_t turnstile_count;
SYSCTL_COUNTER_U64(_kern_epoch_stats, OID_AUTO, ncontended, CTLFLAG_RW,
				   &turnstile_count, "# of times a thread was blocked on a lock in an epoch during an epoch_wait");
static counter_u64_t switch_count;
SYSCTL_COUNTER_U64(_kern_epoch_stats, OID_AUTO, switches, CTLFLAG_RW,
				   &switch_count, "# of times a thread voluntarily context switched in epoch_wait");

typedef struct epoch_cb {
	void (*ec_callback)(epoch_context_t);
	STAILQ_ENTRY(epoch_cb) ec_link;
} *epoch_cb_t;

TAILQ_HEAD(threadlist, thread);

typedef struct epoch_record {
	ck_epoch_record_t er_record;
	volatile struct threadlist er_tdlist;
	uint32_t er_cpuid;
} *epoch_record_t;

struct epoch_pcpu_state {
	struct epoch_record eps_record;
	volatile int eps_waiters;
} __aligned(CACHE_LINE_SIZE);

struct epoch {
	struct ck_epoch e_epoch;
	struct mtx e_lock;
	struct grouptask e_gtask;
	STAILQ_HEAD(, epoch_cb) e_cblist;
	struct epoch_pcpu_state *e_pcpu_dom[MAXMEMDOM];
	struct epoch_pcpu_state *e_pcpu[0];
};

static __read_mostly int domcount[MAXMEMDOM];
static __read_mostly int domoffsets[MAXMEMDOM];
static __read_mostly int inited;

static void epoch_call_task(void *context);
static bool usedomains = true;

static void
epoch_init(void *arg __unused)
{
	int domain, count;

	count = domain = 0;
	domoffsets[0] = 0;
	for (domain = 0; domain < vm_ndomains; domain++) {
		domcount[domain] = CPU_COUNT(&cpuset_domain[domain]);
		if (bootverbose)
			printf("domcount[%d] %d\n", domain, domcount[domain]);
	}
	for (domain = 1; domain < vm_ndomains; domain++)
		domoffsets[domain] = domoffsets[domain-1] + domcount[domain-1];

	for (domain = 0; domain < vm_ndomains; domain++) {
		if (domcount[domain] == 0) {
			usedomains = false;
			break;
		}
	}

	block_count = counter_u64_alloc(M_WAITOK);
	migrate_count = counter_u64_alloc(M_WAITOK);
	turnstile_count = counter_u64_alloc(M_WAITOK);
	switch_count = counter_u64_alloc(M_WAITOK);
	inited = 1;
}
SYSINIT(epoch, SI_SUB_CPU + 1, SI_ORDER_FIRST, epoch_init, NULL);

static void
epoch_init_numa(epoch_t epoch)
{
	int domain, cpu_offset;
	struct epoch_pcpu_state *eps;
	epoch_record_t er;

	for (domain = 0; domain < vm_ndomains; domain++) {
		eps = malloc_domain(sizeof(*eps)*domcount[domain], M_EPOCH,
							domain, M_ZERO|M_WAITOK);
		epoch->e_pcpu_dom[domain] = eps;
		cpu_offset = domoffsets[domain];
		for (int i = 0; i < domcount[domain]; i++, eps++) {
			epoch->e_pcpu[cpu_offset + i] = eps;
			er = &eps->eps_record;
			ck_epoch_register(&epoch->e_epoch, &er->er_record, NULL);
			TAILQ_INIT((struct threadlist *)(uintptr_t)&er->er_tdlist);
			er->er_cpuid = cpu_offset + i;
		}
	}
}

static void
epoch_init_legacy(epoch_t epoch)
{
	struct epoch_pcpu_state *eps;
	epoch_record_t er;

	eps = malloc(sizeof(*eps)*mp_ncpus, M_EPOCH, M_ZERO|M_WAITOK);
	epoch->e_pcpu_dom[0] = eps;
	for (int i = 0; i < mp_ncpus; i++, eps++) {
		epoch->e_pcpu[i] = eps;
		er = &eps->eps_record;
		ck_epoch_register(&epoch->e_epoch, &er->er_record, NULL);
		TAILQ_INIT((struct threadlist *)(uintptr_t)&er->er_tdlist);
		er->er_cpuid = i;
	}
}

epoch_t
epoch_alloc(void)
{
	epoch_t epoch;

	if (__predict_false(!inited))
		panic("%s called too early in boot", __func__);
	epoch = malloc(sizeof(struct epoch) + mp_ncpus*sizeof(void*),
				   M_EPOCH, M_ZERO|M_WAITOK);
	ck_epoch_init(&epoch->e_epoch);
	mtx_init(&epoch->e_lock, "epoch cblist", NULL, MTX_DEF);
	STAILQ_INIT(&epoch->e_cblist);
	taskqgroup_config_gtask_init(epoch, &epoch->e_gtask, epoch_call_task, "epoch call task");
	if (usedomains)
		epoch_init_numa(epoch);
	else
		epoch_init_legacy(epoch);
	return (epoch);
}

void
epoch_free(epoch_t epoch)
{
	int domain;
#ifdef INVARIANTS
	struct epoch_pcpu_state *eps;
	int cpu;

	CPU_FOREACH(cpu) {
		eps = epoch->e_pcpu[cpu];
		MPASS(TAILQ_EMPTY(&eps->eps_record.er_tdlist));
	}
#endif
	mtx_destroy(&epoch->e_lock);
	taskqgroup_config_gtask_deinit(&epoch->e_gtask);
	if (usedomains)
		for (domain = 0; domain < vm_ndomains; domain++)
			free_domain(epoch->e_pcpu_dom[domain], M_EPOCH);
	else
		free(epoch->e_pcpu_dom[0], M_EPOCH);
	free(epoch, M_EPOCH);
}

#define INIT_CHECK(epoch)								\
	do {											\
		if (__predict_false((epoch) == NULL))		\
			return;									\
	} while (0)

void
epoch_enter(epoch_t epoch)
{
	struct epoch_pcpu_state *eps;
	struct thread *td;

	INIT_CHECK(epoch);

	td = curthread;
	critical_enter();
	eps = epoch->e_pcpu[curcpu];
	td->td_epochnest++;
	MPASS(td->td_epochnest < UCHAR_MAX - 2);
	if (td->td_epochnest == 1)
		TAILQ_INSERT_TAIL(&eps->eps_record.er_tdlist, td, td_epochq);
#ifdef INVARIANTS
	if (td->td_epochnest > 1) {
		struct thread *curtd;
		int found = 0;

		TAILQ_FOREACH(curtd, &eps->eps_record.er_tdlist, td_epochq)
			if (curtd == td)
				found = 1;
		KASSERT(found, ("recursing on a second epoch"));
	}
#endif
	sched_pin();
	ck_epoch_begin(&eps->eps_record.er_record, NULL);
	critical_exit();
}

void
epoch_enter_nopreempt(epoch_t epoch)
{
	struct epoch_pcpu_state *eps;

	INIT_CHECK(epoch);
	critical_enter();
	eps = epoch->e_pcpu[curcpu];
	curthread->td_epochnest++;
	MPASS(curthread->td_epochnest < UCHAR_MAX - 2);
	ck_epoch_begin(&eps->eps_record.er_record, NULL);
}

void
epoch_exit(epoch_t epoch)
{
	struct epoch_pcpu_state *eps;
	struct thread *td;

	td = curthread;
	INIT_CHECK(epoch);
	critical_enter();
	eps = epoch->e_pcpu[curcpu];
	sched_unpin();
	ck_epoch_end(&eps->eps_record.er_record, NULL);
	td->td_epochnest--;
	if (td->td_epochnest == 0)
		TAILQ_REMOVE(&eps->eps_record.er_tdlist, td, td_epochq);
	critical_exit();
}

void
epoch_exit_nopreempt(epoch_t epoch)
{
	struct epoch_pcpu_state *eps;

	INIT_CHECK(epoch);
	MPASS(curthread->td_critnest);
	eps = epoch->e_pcpu[curcpu];
	ck_epoch_end(&eps->eps_record.er_record, NULL);
	curthread->td_epochnest--;
	critical_exit();
}

/*
 * epoch_block_handler is a callback from the ck code when another thread is
 * currently in an epoch section.
 */
static void
epoch_block_handler(struct ck_epoch *global __unused, ck_epoch_record_t *cr,
					void *arg __unused)
{
	epoch_record_t record;
	struct epoch_pcpu_state *eps;
	struct thread *td, *tdwait, *owner;
	struct turnstile *ts;
	struct lock_object *lock;
	u_char prio;
	int spincount;

	eps = arg;
	record = __containerof(cr, struct epoch_record, er_record);
	td = curthread;
	spincount = 0;
	counter_u64_add(block_count, 1);
	if (record->er_cpuid != curcpu) {
		/*
		 * If the head of the list is running, we can wait for it
		 * to remove itself from the list and thus save us the
		 * overhead of a migration
		 */
		if ((tdwait = TAILQ_FIRST(&record->er_tdlist)) != NULL &&
			TD_IS_RUNNING(tdwait)) {
			while (tdwait == TAILQ_FIRST(&record->er_tdlist) &&
				   TD_IS_RUNNING(tdwait) && spincount++ < MAX_ADAPTIVE_SPIN) {
				cpu_spinwait();
			}
			return;
		}

		/*
		 * Being on the same CPU as that of the record on which
		 * we need to wait allows us access to the thread
		 * list associated with that CPU. We can then examine the
		 * oldest thread in the queue and wait on its turnstile
		 * until it resumes and so on until a grace period
		 * elapses.
		 *
		 */
		counter_u64_add(migrate_count, 1);
		sched_bind(td, record->er_cpuid);
		/*
		 * At this point we need to return to the ck code
		 * to scan to see if a grace period has elapsed.
		 * We can't move on to check the thread list, because
		 * in the meantime new threads may have arrived that
		 * in fact belong to a different epoch.
		 */
		return;
	}
	/*
	 * Try to find a thread in an epoch section on this CPU 
	 * waiting on a turnstile. Otherwise find the lowest
	 * priority thread (highest prio value) and drop our priority
	 * to match to allow it to run.
	 */
	prio = 0;
	TAILQ_FOREACH(tdwait, &record->er_tdlist, td_epochq) {
		if (td->td_priority > prio)
			prio = td->td_priority;
		if (TD_IS_INHIBITED(tdwait) && TD_ON_LOCK(tdwait) &&
			((ts = tdwait->td_blocked) != NULL)) {
			/*
			 * We unlock td to allow turnstile_wait to reacquire the
			 * the thread lock. Before unlocking it we enter a critical
			 * section to prevent preemption after we reenable interrupts
			 * by dropping the thread lock in order to prevent tdwait
			 * from getting to run.
			 */
			critical_enter();
			thread_unlock(td);
			owner = turnstile_lock(ts, &lock);
			/*
			 * The owner pointer indicates that the lock succeeded. Only
			 * in case we hold the lock and the turnstile we locked is still
			 * the one that tdwait is blocked on can we continue. Otherwise
			 * The turnstile pointer has been changed out from underneath
			 * us, as in the case where the lock holder has signalled tdwait,
			 * and we need to continue.
			 */
			if (owner != NULL && ts == tdwait->td_blocked) {
				MPASS(TD_IS_INHIBITED(tdwait) && TD_ON_LOCK(tdwait));
				critical_exit();
				turnstile_wait(ts, owner, tdwait->td_tsqueue);
				counter_u64_add(turnstile_count, 1);
				thread_lock(td);
				return;
			} else if (owner != NULL)
				turnstile_unlock(ts, lock);
			thread_lock(td);
			critical_exit();
			KASSERT(td->td_locks == 0,
					("%d locks held", td->td_locks));
		}
	}
	/*
	 * We didn't find any threads actually blocked on a lock
	 * we have nothing to do except set our priority to match
	 * that of the lowest value on the queue and context switch
	 * away.
	 */
	counter_u64_add(switch_count, 1);
	sched_prio(td, prio);
	mi_switch(SW_VOL | SWT_RELINQUISH, NULL);

	/*
	 * Release the thread lock while yielding to
	 * allow other threads to acquire the lock
	 * pointed to by TDQ_LOCKPTR(td). Else a
	 * deadlock like situation might happen. (HPS)
	 */
	thread_unlock(td);
	thread_lock(td);
}

void
epoch_wait(epoch_t epoch)
{
	struct thread *td;
	int was_bound;
	int old_cpu;
	int old_pinned;
	u_char old_prio;

	INIT_CHECK(epoch);

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "epoch_wait() can sleep");

	td = curthread;
	KASSERT(td->td_epochnest == 0, ("epoch_wait() in the middle of an epoch section"));
	thread_lock(td);

	DROP_GIANT();

	old_cpu = PCPU_GET(cpuid);
	old_pinned = td->td_pinned;
	old_prio = td->td_priority;
	was_bound = sched_is_bound(td);
	sched_unbind(td);
	td->td_pinned = 0;
	sched_bind(td, old_cpu);

	ck_epoch_synchronize_wait(&epoch->e_epoch, epoch_block_handler, NULL);

	/* restore CPU binding, if any */
	if (was_bound != 0) {
		sched_bind(td, old_cpu);
	} else {
		/* get thread back to initial CPU, if any */
		if (old_pinned != 0)
			sched_bind(td, old_cpu);
		sched_unbind(td);
	}
	/* restore pinned after bind */
	td->td_pinned = old_pinned;

	/* restore thread priority */
	sched_prio(td, old_prio);
	thread_unlock(td);

	PICKUP_GIANT();
}

void
epoch_call(epoch_t epoch, epoch_context_t ctx, void (*callback) (epoch_context_t))
{
	epoch_cb_t cb;

	cb = (void *)ctx;
	cb->ec_callback = callback;
	mtx_lock(&epoch->e_lock);
	STAILQ_INSERT_TAIL(&epoch->e_cblist, cb, ec_link);
	GROUPTASK_ENQUEUE(&epoch->e_gtask);
	mtx_unlock(&epoch->e_lock);
}

static void
epoch_call_task(void *context)
{
	epoch_t epoch;
	epoch_cb_t cb;
	STAILQ_HEAD(, epoch_cb) tmp_head;

	epoch = context;
	STAILQ_INIT(&tmp_head);

	mtx_lock(&epoch->e_lock);
	STAILQ_CONCAT(&tmp_head, &epoch->e_cblist);
	mtx_unlock(&epoch->e_lock);

	epoch_wait(epoch);

	while ((cb = STAILQ_FIRST(&tmp_head)) != NULL)
		cb->ec_callback((void*)cb);
}

int
in_epoch(void)
{
	return (curthread->td_epochnest != 0);
}
