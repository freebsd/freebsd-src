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
#include <sys/pcpu.h>
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
#define MAX_EPOCHS 64

CTASSERT(sizeof(ck_epoch_entry_t) == sizeof(struct epoch_context));
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
static counter_u64_t epoch_call_count;

SYSCTL_COUNTER_U64(_kern_epoch_stats, OID_AUTO, epoch_calls, CTLFLAG_RW,
    &epoch_call_count, "# of times a callback was deferred");
static counter_u64_t epoch_call_task_count;

SYSCTL_COUNTER_U64(_kern_epoch_stats, OID_AUTO, epoch_call_tasks, CTLFLAG_RW,
    &epoch_call_task_count, "# of times a callback task was run");

TAILQ_HEAD (threadlist, thread);

CK_STACK_CONTAINER(struct ck_epoch_entry, stack_entry,
    ck_epoch_entry_container)

	epoch_t	allepochs[MAX_EPOCHS];

DPCPU_DEFINE(struct grouptask, epoch_cb_task);
DPCPU_DEFINE(int, epoch_cb_count);

static __read_mostly int domcount[MAXMEMDOM];
static __read_mostly int domoffsets[MAXMEMDOM];
static __read_mostly int inited;
static __read_mostly int epoch_count;
__read_mostly epoch_t global_epoch;
__read_mostly epoch_t global_epoch_preempt;

static void epoch_call_task(void *context __unused);

#if defined(__powerpc64__) || defined(__powerpc__) || !defined(NUMA)
static bool usedomains = false;
#else
static bool usedomains = true;
#endif
static void
epoch_init(void *arg __unused)
{
	int domain, cpu;

	block_count = counter_u64_alloc(M_WAITOK);
	migrate_count = counter_u64_alloc(M_WAITOK);
	turnstile_count = counter_u64_alloc(M_WAITOK);
	switch_count = counter_u64_alloc(M_WAITOK);
	epoch_call_count = counter_u64_alloc(M_WAITOK);
	epoch_call_task_count = counter_u64_alloc(M_WAITOK);
	if (usedomains == false)
		goto done;
	domain = 0;
	domoffsets[0] = 0;
	for (domain = 0; domain < vm_ndomains; domain++) {
		domcount[domain] = CPU_COUNT(&cpuset_domain[domain]);
		if (bootverbose)
			printf("domcount[%d] %d\n", domain, domcount[domain]);
	}
	for (domain = 1; domain < vm_ndomains; domain++)
		domoffsets[domain] = domoffsets[domain - 1] + domcount[domain - 1];

	for (domain = 0; domain < vm_ndomains; domain++) {
		if (domcount[domain] == 0) {
			usedomains = false;
			break;
		}
	}
done:
	CPU_FOREACH(cpu) {
		GROUPTASK_INIT(DPCPU_ID_PTR(cpu, epoch_cb_task), 0, epoch_call_task, NULL);
		taskqgroup_attach_cpu(qgroup_softirq, DPCPU_ID_PTR(cpu, epoch_cb_task), NULL, cpu, -1, "epoch call task");
	}
	inited = 1;
	global_epoch = epoch_alloc(0);
	global_epoch_preempt = epoch_alloc(EPOCH_PREEMPT);
}
SYSINIT(epoch, SI_SUB_TASKQ + 1, SI_ORDER_FIRST, epoch_init, NULL);

#if !defined(EARLY_AP_STARTUP)
static void
epoch_init_smp(void *dummy __unused)
{
	inited = 2;
}
SYSINIT(epoch_smp, SI_SUB_SMP + 1, SI_ORDER_FIRST, epoch_init_smp, NULL);
#endif


static void
epoch_init_numa(epoch_t epoch)
{
	int domain, cpu_offset;
	epoch_record_t er;

	for (domain = 0; domain < vm_ndomains; domain++) {
		er = malloc_domain(sizeof(*er) * domcount[domain], M_EPOCH,
		    domain, M_ZERO | M_WAITOK);
		epoch->e_pcpu_dom[domain] = er;
		cpu_offset = domoffsets[domain];
		for (int i = 0; i < domcount[domain]; i++, er++) {
			epoch->e_pcpu[cpu_offset + i] = er;
			ck_epoch_register(&epoch->e_epoch, &er->er_record, NULL);
			TAILQ_INIT((struct threadlist *)(uintptr_t)&er->er_tdlist);
			er->er_cpuid = cpu_offset + i;
		}
	}
}

static void
epoch_init_legacy(epoch_t epoch)
{
	epoch_record_t er;

	er = malloc(sizeof(*er) * mp_ncpus, M_EPOCH, M_ZERO | M_WAITOK);
	epoch->e_pcpu_dom[0] = er;
	for (int i = 0; i < mp_ncpus; i++, er++) {
		epoch->e_pcpu[i] = er;
		ck_epoch_register(&epoch->e_epoch, &er->er_record, NULL);
		TAILQ_INIT((struct threadlist *)(uintptr_t)&er->er_tdlist);
		er->er_cpuid = i;
	}
}

epoch_t
epoch_alloc(int flags)
{
	epoch_t epoch;

	if (__predict_false(!inited))
		panic("%s called too early in boot", __func__);
	epoch = malloc(sizeof(struct epoch) + mp_ncpus * sizeof(void *),
	    M_EPOCH, M_ZERO | M_WAITOK);
	ck_epoch_init(&epoch->e_epoch);
	if (usedomains)
		epoch_init_numa(epoch);
	else
		epoch_init_legacy(epoch);
	MPASS(epoch_count < MAX_EPOCHS - 2);
	epoch->e_flags = flags;
	epoch->e_idx = epoch_count;
	allepochs[epoch_count++] = epoch;
	return (epoch);
}

void
epoch_free(epoch_t epoch)
{
	int domain;
#ifdef INVARIANTS
	struct epoch_record *er;
	int cpu;

	CPU_FOREACH(cpu) {
		er = epoch->e_pcpu[cpu];
		MPASS(TAILQ_EMPTY(&er->er_tdlist));
	}
#endif
	allepochs[epoch->e_idx] = NULL;
	epoch_wait(global_epoch);
	if (usedomains)
		for (domain = 0; domain < vm_ndomains; domain++)
			free_domain(epoch->e_pcpu_dom[domain], M_EPOCH);
	else
		free(epoch->e_pcpu_dom[0], M_EPOCH);
	free(epoch, M_EPOCH);
}

void
epoch_enter_preempt_KBI(epoch_t epoch, epoch_tracker_t et)
{

	epoch_enter_preempt(epoch, et);
}

void
epoch_exit_preempt_KBI(epoch_t epoch, epoch_tracker_t et)
{

	epoch_exit_preempt(epoch, et);
}

void
epoch_enter_KBI(epoch_t epoch)
{

	epoch_enter(epoch);
}

void
epoch_exit_KBI(epoch_t epoch)
{

	epoch_exit(epoch);
}

/*
 * epoch_block_handler_preempt is a callback from the ck code when another thread is
 * currently in an epoch section.
 */
static void
epoch_block_handler_preempt(struct ck_epoch *global __unused, ck_epoch_record_t *cr,
    void *arg __unused)
{
	epoch_record_t record;
	struct thread *td, *owner, *curwaittd;
	struct epoch_thread *tdwait;
	struct turnstile *ts;
	struct lock_object *lock;
	int spincount, gen;
	int locksheld __unused;

	record = __containerof(cr, struct epoch_record, er_record);
	td = curthread;
	locksheld = td->td_locks;
	spincount = 0;
	counter_u64_add(block_count, 1);
	if (record->er_cpuid != curcpu) {
		/*
		 * If the head of the list is running, we can wait for it
		 * to remove itself from the list and thus save us the
		 * overhead of a migration
		 */
		if ((tdwait = TAILQ_FIRST(&record->er_tdlist)) != NULL &&
		    TD_IS_RUNNING(tdwait->et_td)) {
			gen = record->er_gen;
			thread_unlock(td);
			do {
				cpu_spinwait();
			} while (tdwait == TAILQ_FIRST(&record->er_tdlist) &&
			    gen == record->er_gen && TD_IS_RUNNING(tdwait->et_td) &&
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
	TAILQ_FOREACH(tdwait, &record->er_tdlist, et_link) {
		/*
		 * Propagate our priority to any other waiters to prevent us
		 * from starving them. They will have their original priority
		 * restore on exit from epoch_wait().
		 */
		curwaittd = tdwait->et_td;
		if (!TD_IS_INHIBITED(curwaittd) && curwaittd->td_priority > td->td_priority) {
			critical_enter();
			thread_unlock(td);
			thread_lock(curwaittd);
			sched_prio(curwaittd, td->td_priority);
			thread_unlock(curwaittd);
			thread_lock(td);
			critical_exit();
		}
		if (TD_IS_INHIBITED(curwaittd) && TD_ON_LOCK(curwaittd) &&
		    ((ts = curwaittd->td_blocked) != NULL)) {
			/*
			 * We unlock td to allow turnstile_wait to reacquire the
			 * the thread lock. Before unlocking it we enter a critical
			 * section to prevent preemption after we reenable interrupts
			 * by dropping the thread lock in order to prevent curwaittd
			 * from getting to run.
			 */
			critical_enter();
			thread_unlock(td);
			owner = turnstile_lock(ts, &lock);
			/*
			 * The owner pointer indicates that the lock succeeded. Only
			 * in case we hold the lock and the turnstile we locked is still
			 * the one that curwaittd is blocked on can we continue. Otherwise
			 * The turnstile pointer has been changed out from underneath
			 * us, as in the case where the lock holder has signalled curwaittd,
			 * and we need to continue.
			 */
			if (owner != NULL && ts == curwaittd->td_blocked) {
				MPASS(TD_IS_INHIBITED(curwaittd) && TD_ON_LOCK(curwaittd));
				critical_exit();
				turnstile_wait(ts, owner, curwaittd->td_tsqueue);
				counter_u64_add(turnstile_count, 1);
				thread_lock(td);
				return;
			} else if (owner != NULL)
				turnstile_unlock(ts, lock);
			thread_lock(td);
			critical_exit();
			KASSERT(td->td_locks == locksheld,
			    ("%d extra locks held", td->td_locks - locksheld));
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
epoch_wait_preempt(epoch_t epoch)
{
	struct thread *td;
	int was_bound;
	int old_cpu;
	int old_pinned;
	u_char old_prio;
	int locks __unused;

	MPASS(cold || epoch != NULL);
	INIT_CHECK(epoch);
	td = curthread;
#ifdef INVARIANTS
	locks = curthread->td_locks;
	MPASS(epoch->e_flags & EPOCH_PREEMPT);
	if ((epoch->e_flags & EPOCH_LOCKED) == 0)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "epoch_wait() can be long running");
	KASSERT(!in_epoch(epoch),
			("epoch_wait_preempt() called in the middle "
			 "of an epoch section of the same epoch"));
#endif
	thread_lock(td);
	DROP_GIANT();

	old_cpu = PCPU_GET(cpuid);
	old_pinned = td->td_pinned;
	old_prio = td->td_priority;
	was_bound = sched_is_bound(td);
	sched_unbind(td);
	td->td_pinned = 0;
	sched_bind(td, old_cpu);

	ck_epoch_synchronize_wait(&epoch->e_epoch, epoch_block_handler_preempt, NULL);

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

static void
epoch_block_handler(struct ck_epoch *g __unused, ck_epoch_record_t *c __unused,
    void *arg __unused)
{
	cpu_spinwait();
}

void
epoch_wait(epoch_t epoch)
{

	MPASS(cold || epoch != NULL);
	INIT_CHECK(epoch);
	MPASS(epoch->e_flags == 0);
	critical_enter();
	ck_epoch_synchronize_wait(&epoch->e_epoch, epoch_block_handler, NULL);
	critical_exit();
}

void
epoch_call(epoch_t epoch, epoch_context_t ctx, void (*callback) (epoch_context_t))
{
	epoch_record_t er;
	ck_epoch_entry_t *cb;

	cb = (void *)ctx;

	MPASS(callback);
	/* too early in boot to have epoch set up */
	if (__predict_false(epoch == NULL))
		goto boottime;
#if !defined(EARLY_AP_STARTUP)
	if (__predict_false(inited < 2))
		goto boottime;
#endif

	critical_enter();
	*DPCPU_PTR(epoch_cb_count) += 1;
	er = epoch->e_pcpu[curcpu];
	ck_epoch_call(&er->er_record, cb, (ck_epoch_cb_t *)callback);
	critical_exit();
	return;
boottime:
	callback(ctx);
}

static void
epoch_call_task(void *arg __unused)
{
	ck_stack_entry_t *cursor, *head, *next;
	ck_epoch_record_t *record;
	epoch_t epoch;
	ck_stack_t cb_stack;
	int i, npending, total;

	ck_stack_init(&cb_stack);
	critical_enter();
	epoch_enter(global_epoch);
	for (total = i = 0; i < epoch_count; i++) {
		if (__predict_false((epoch = allepochs[i]) == NULL))
			continue;
		record = &epoch->e_pcpu[curcpu]->er_record;
		if ((npending = record->n_pending) == 0)
			continue;
		ck_epoch_poll_deferred(record, &cb_stack);
		total += npending - record->n_pending;
	}
	epoch_exit(global_epoch);
	*DPCPU_PTR(epoch_cb_count) -= total;
	critical_exit();

	counter_u64_add(epoch_call_count, total);
	counter_u64_add(epoch_call_task_count, 1);

	head = ck_stack_batch_pop_npsc(&cb_stack);
	for (cursor = head; cursor != NULL; cursor = next) {
		struct ck_epoch_entry *entry =
		ck_epoch_entry_container(cursor);

		next = CK_STACK_NEXT(cursor);
		entry->function(entry);
	}
}

int
in_epoch_verbose(epoch_t epoch, int dump_onfail)
{
	struct epoch_thread *tdwait;
	struct thread *td;
	epoch_record_t er;

	td = curthread;
	if (td->td_epochnest == 0)
		return (0);
	if (__predict_false((epoch) == NULL))
		return (0);
	critical_enter();
	er = epoch->e_pcpu[curcpu];
	TAILQ_FOREACH(tdwait, &er->er_tdlist, et_link)
		if (tdwait->et_td == td) {
			critical_exit();
			return (1);
		}
#ifdef INVARIANTS
	if (dump_onfail) {
		MPASS(td->td_pinned);
		printf("cpu: %d id: %d\n", curcpu, td->td_tid);
		TAILQ_FOREACH(tdwait, &er->er_tdlist, et_link)
			printf("td_tid: %d ", tdwait->et_td->td_tid);
		printf("\n");
	}
#endif
	critical_exit();
	return (0);
}

int
in_epoch(epoch_t epoch)
{
	return (in_epoch_verbose(epoch, 0));
}

void
epoch_adjust_prio(struct thread *td, u_char prio)
{
	thread_lock(td);
	sched_prio(td, prio);
	thread_unlock(td);
}
