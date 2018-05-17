/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018, Matthew Macy <mmacy@freebsd.org>
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
 *
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

static MALLOC_DEFINE(M_EPOCH, "epoch", "epoch based reclamation");

/* arbitrary --- needs benchmarking */
#define MAX_ADAPTIVE_SPIN 1000

#define EPOCH_EXITING 0x1
#ifdef __amd64__
#define EPOCH_ALIGN CACHE_LINE_SIZE*2
#else
#define EPOCH_ALIGN CACHE_LINE_SIZE
#endif

CTASSERT(sizeof(epoch_section_t) == sizeof(ck_epoch_section_t));
CTASSERT(sizeof(ck_epoch_entry_t) == sizeof(struct epoch_context));
SYSCTL_NODE(_kern, OID_AUTO, epoch, CTLFLAG_RW, 0, "epoch information");
SYSCTL_NODE(_kern_epoch, OID_AUTO, stats, CTLFLAG_RW, 0, "epoch stats");

static int poll_intvl;
SYSCTL_INT(_kern_epoch, OID_AUTO, poll_intvl, CTLFLAG_RWTUN,
		   &poll_intvl, 0, "# of ticks to wait between garbage collecting deferred frees");
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

TAILQ_HEAD(threadlist, thread);

CK_STACK_CONTAINER(struct ck_epoch_entry, stack_entry,
    ck_epoch_entry_container)

typedef struct epoch_record {
	ck_epoch_record_t er_record;
	volatile struct threadlist er_tdlist;
	volatile uint32_t er_gen;
	uint32_t er_cpuid;
} *epoch_record_t;

struct epoch_pcpu_state {
	struct epoch_record eps_record;
} __aligned(EPOCH_ALIGN);

struct epoch {
	struct ck_epoch e_epoch __aligned(EPOCH_ALIGN);
	struct grouptask e_gtask;
	struct callout e_timer;
	struct mtx e_lock;
	int e_flags;
	/* make sure that immutable data doesn't overlap with the gtask, callout, and mutex*/
	struct epoch_pcpu_state *e_pcpu_dom[MAXMEMDOM] __aligned(EPOCH_ALIGN);
	counter_u64_t e_frees;
	uint64_t e_free_last;
	struct epoch_pcpu_state *e_pcpu[0];
};

static __read_mostly int domcount[MAXMEMDOM];
static __read_mostly int domoffsets[MAXMEMDOM];
static __read_mostly int inited;
__read_mostly epoch_t global_epoch;

static void epoch_call_task(void *context);

#if defined(__powerpc64__) || defined(__powerpc__) || !defined(NUMA)
static bool usedomains = false;
#else
static bool usedomains = true;
#endif
static void
epoch_init(void *arg __unused)
{
	int domain, count;

	if (poll_intvl == 0)
		poll_intvl = hz;

	block_count = counter_u64_alloc(M_WAITOK);
	migrate_count = counter_u64_alloc(M_WAITOK);
	turnstile_count = counter_u64_alloc(M_WAITOK);
	switch_count = counter_u64_alloc(M_WAITOK);
	if (usedomains == false)
		goto done;
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
 done:
	inited = 1;
	global_epoch = epoch_alloc();
}
SYSINIT(epoch, SI_SUB_TASKQ + 1, SI_ORDER_FIRST, epoch_init, NULL);

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

static void
epoch_callout(void *arg)
{
	epoch_t epoch;
	uint64_t frees;

	epoch = arg;
	frees = counter_u64_fetch(epoch->e_frees);
	/* pick some better value */
	if (frees - epoch->e_free_last > 10) {
		GROUPTASK_ENQUEUE(&epoch->e_gtask);
		epoch->e_free_last = frees;
	}
	if ((epoch->e_flags & EPOCH_EXITING) == 0)
		callout_reset(&epoch->e_timer, poll_intvl, epoch_callout, epoch);
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
	epoch->e_frees = counter_u64_alloc(M_WAITOK);
	mtx_init(&epoch->e_lock, "epoch callout", NULL, MTX_DEF);
	callout_init_mtx(&epoch->e_timer, &epoch->e_lock, 0);
	taskqgroup_config_gtask_init(epoch, &epoch->e_gtask, epoch_call_task, "epoch call task");
	if (usedomains)
		epoch_init_numa(epoch);
	else
		epoch_init_legacy(epoch);
	callout_reset(&epoch->e_timer, poll_intvl, epoch_callout, epoch);
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
	mtx_lock(&epoch->e_lock);
	epoch->e_flags |= EPOCH_EXITING;
	mtx_unlock(&epoch->e_lock);
	/*
	 * Execute any lingering callbacks
	 */
	GROUPTASK_ENQUEUE(&epoch->e_gtask);
	gtaskqueue_drain(epoch->e_gtask.gt_taskqueue, &epoch->e_gtask.gt_task);
	callout_drain(&epoch->e_timer);
	mtx_destroy(&epoch->e_lock);
	counter_u64_free(epoch->e_frees);
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
epoch_enter_internal(epoch_t epoch, struct thread *td)
{
	struct epoch_pcpu_state *eps;

	INIT_CHECK(epoch);
	critical_enter();
	td->td_pre_epoch_prio = td->td_priority;
	eps = epoch->e_pcpu[curcpu];
#ifdef INVARIANTS
	MPASS(td->td_epochnest < UCHAR_MAX - 2);
	if (td->td_epochnest > 1) {
		struct thread *curtd;
		int found = 0;

		TAILQ_FOREACH(curtd, &eps->eps_record.er_tdlist, td_epochq)
			if (curtd == td)
				found = 1;
		KASSERT(found, ("recursing on a second epoch"));
		critical_exit();
		return;
	}
#endif
	TAILQ_INSERT_TAIL(&eps->eps_record.er_tdlist, td, td_epochq);
	sched_pin();
	ck_epoch_begin(&eps->eps_record.er_record, (ck_epoch_section_t*)&td->td_epoch_section);
	critical_exit();
}

void
epoch_exit_internal(epoch_t epoch, struct thread *td)
{
	struct epoch_pcpu_state *eps;

	td = curthread;
	MPASS(td->td_epochnest == 0);
	INIT_CHECK(epoch);
	critical_enter();
	eps = epoch->e_pcpu[curcpu];

	ck_epoch_end(&eps->eps_record.er_record, (ck_epoch_section_t*)&td->td_epoch_section);
	TAILQ_REMOVE(&eps->eps_record.er_tdlist, td, td_epochq);
	eps->eps_record.er_gen++;
	sched_unpin();
	if (__predict_false(td->td_pre_epoch_prio != td->td_priority)) {
		thread_lock(td);
		sched_prio(td, td->td_pre_epoch_prio);
		thread_unlock(td);
	}
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
	int spincount, gen;

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
			gen = record->er_gen;
			thread_unlock(td);
			do {
				cpu_spinwait();
			} while (tdwait == TAILQ_FIRST(&record->er_tdlist) &&
					 gen == record->er_gen && TD_IS_RUNNING(tdwait) &&
					 spincount++ < MAX_ADAPTIVE_SPIN);
			thread_lock(td);
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
	TAILQ_FOREACH(tdwait, &record->er_tdlist, td_epochq) {
		/*
		 * Propagate our priority to any other waiters to prevent us
		 * from starving them. They will have their original priority
		 * restore on exit from epoch_wait().
		 */
		if (!TD_IS_INHIBITED(tdwait) && tdwait->td_priority > td->td_priority) {
			critical_enter();
			thread_unlock(td);
			thread_lock(tdwait);
			sched_prio(tdwait, td->td_priority);
			thread_unlock(tdwait);
			thread_lock(td);
			critical_exit();
		}
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
	 * so we have nothing to do except context switch away.
	 */
	counter_u64_add(switch_count, 1);
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
#ifdef INVARIANTS
	int locks;

	locks = curthread->td_locks;
#endif
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
	KASSERT(td->td_locks == locks,
			("%d residual locks held", td->td_locks - locks));
}

void
epoch_call(epoch_t epoch, epoch_context_t ctx, void (*callback) (epoch_context_t))
{
	struct epoch_pcpu_state *eps;
	ck_epoch_entry_t *cb;

	cb = (void *)ctx;

	MPASS(callback);
	/* too early in boot to have epoch set up */
	if (__predict_false(epoch == NULL))
		goto boottime;

	counter_u64_add(epoch->e_frees, 1);

	critical_enter();
	eps = epoch->e_pcpu[curcpu];
	ck_epoch_call(&eps->eps_record.er_record, cb, (ck_epoch_cb_t*)callback);
	critical_exit();
	return;
 boottime:
	callback(ctx);
}

static void
epoch_call_task(void *context)
{
	struct epoch_pcpu_state *eps;
	epoch_t epoch;
	struct thread *td;
	ck_stack_entry_t *cursor;
	ck_stack_t deferred;
	int cpu;

	epoch = context;
	td = curthread;
	ck_stack_init(&deferred);
	thread_lock(td);
	CPU_FOREACH(cpu) {
		sched_bind(td, cpu);
		eps = epoch->e_pcpu[cpu];
		ck_epoch_poll_deferred(&eps->eps_record.er_record, &deferred);
	}
	sched_unbind(td);
	thread_unlock(td);
	while((cursor = ck_stack_pop_npsc(&deferred)) != NULL) {
		struct ck_epoch_entry *entry =
		    ck_epoch_entry_container(cursor);
		entry->function(entry);
	}
}

int
in_epoch(void)
{
	return (curthread->td_epochnest != 0);
}
