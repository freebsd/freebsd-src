/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2001 Julian Elischer <julian@freebsd.org>.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "opt_witness.h"
#include "opt_hwpmc_hooks.h"

#include <sys/systm.h>
#include <sys/asan.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/bitstring.h>
#include <sys/epoch.h>
#include <sys/rangelock.h>
#include <sys/resourcevar.h>
#include <sys/sdt.h>
#include <sys/smp.h>
#include <sys/sched.h>
#include <sys/sleepqueue.h>
#include <sys/selinfo.h>
#include <sys/syscallsubr.h>
#include <sys/dtrace_bsd.h>
#include <sys/sysent.h>
#include <sys/turnstile.h>
#include <sys/taskqueue.h>
#include <sys/ktr.h>
#include <sys/rwlock.h>
#include <sys/umtxvar.h>
#include <sys/vmmeter.h>
#include <sys/cpuset.h>
#ifdef	HWPMC_HOOKS
#include <sys/pmckern.h>
#endif
#include <sys/priv.h>

#include <security/audit/audit.h>

#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/vm_phys.h>
#include <sys/eventhandler.h>

/*
 * Asserts below verify the stability of struct thread and struct proc
 * layout, as exposed by KBI to modules.  On head, the KBI is allowed
 * to drift, change to the structures must be accompanied by the
 * assert update.
 *
 * On the stable branches after KBI freeze, conditions must not be
 * violated.  Typically new fields are moved to the end of the
 * structures.
 */
#ifdef __amd64__
_Static_assert(offsetof(struct thread, td_flags) == 0x108,
    "struct thread KBI td_flags");
_Static_assert(offsetof(struct thread, td_pflags) == 0x114,
    "struct thread KBI td_pflags");
_Static_assert(offsetof(struct thread, td_frame) == 0x4e8,
    "struct thread KBI td_frame");
_Static_assert(offsetof(struct thread, td_emuldata) == 0x6f0,
    "struct thread KBI td_emuldata");
_Static_assert(offsetof(struct proc, p_flag) == 0xb8,
    "struct proc KBI p_flag");
_Static_assert(offsetof(struct proc, p_pid) == 0xc4,
    "struct proc KBI p_pid");
_Static_assert(offsetof(struct proc, p_filemon) == 0x3c8,
    "struct proc KBI p_filemon");
_Static_assert(offsetof(struct proc, p_comm) == 0x3e0,
    "struct proc KBI p_comm");
_Static_assert(offsetof(struct proc, p_emuldata) == 0x4d0,
    "struct proc KBI p_emuldata");
#endif
#ifdef __i386__
_Static_assert(offsetof(struct thread, td_flags) == 0x9c,
    "struct thread KBI td_flags");
_Static_assert(offsetof(struct thread, td_pflags) == 0xa8,
    "struct thread KBI td_pflags");
_Static_assert(offsetof(struct thread, td_frame) == 0x33c,
    "struct thread KBI td_frame");
_Static_assert(offsetof(struct thread, td_emuldata) == 0x380,
    "struct thread KBI td_emuldata");
_Static_assert(offsetof(struct proc, p_flag) == 0x6c,
    "struct proc KBI p_flag");
_Static_assert(offsetof(struct proc, p_pid) == 0x78,
    "struct proc KBI p_pid");
_Static_assert(offsetof(struct proc, p_filemon) == 0x270,
    "struct proc KBI p_filemon");
_Static_assert(offsetof(struct proc, p_comm) == 0x284,
    "struct proc KBI p_comm");
_Static_assert(offsetof(struct proc, p_emuldata) == 0x318,
    "struct proc KBI p_emuldata");
#endif

SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE(proc, , , lwp__exit);

/*
 * thread related storage.
 */
static uma_zone_t thread_zone;

struct thread_domain_data {
	struct thread	*tdd_zombies;
	int		tdd_reapticks;
} __aligned(CACHE_LINE_SIZE);

static struct thread_domain_data thread_domain_data[MAXMEMDOM];

static struct task	thread_reap_task;
static struct callout  	thread_reap_callout;

static void thread_zombie(struct thread *);
static void thread_reap(void);
static void thread_reap_all(void);
static void thread_reap_task_cb(void *, int);
static void thread_reap_callout_cb(void *);
static void thread_unsuspend_one(struct thread *td, struct proc *p,
    bool boundary);
static void thread_free_batched(struct thread *td);

static __exclusive_cache_line struct mtx tid_lock;
static bitstr_t *tid_bitmap;

static MALLOC_DEFINE(M_TIDHASH, "tidhash", "thread hash");

static int maxthread;
SYSCTL_INT(_kern, OID_AUTO, maxthread, CTLFLAG_RDTUN,
    &maxthread, 0, "Maximum number of threads");

static __exclusive_cache_line int nthreads;

static LIST_HEAD(tidhashhead, thread) *tidhashtbl;
static u_long	tidhash;
static u_long	tidhashlock;
static struct	rwlock *tidhashtbl_lock;
#define	TIDHASH(tid)		(&tidhashtbl[(tid) & tidhash])
#define	TIDHASHLOCK(tid)	(&tidhashtbl_lock[(tid) & tidhashlock])

EVENTHANDLER_LIST_DEFINE(thread_ctor);
EVENTHANDLER_LIST_DEFINE(thread_dtor);
EVENTHANDLER_LIST_DEFINE(thread_init);
EVENTHANDLER_LIST_DEFINE(thread_fini);

static bool
thread_count_inc_try(void)
{
	int nthreads_new;

	nthreads_new = atomic_fetchadd_int(&nthreads, 1) + 1;
	if (nthreads_new >= maxthread - 100) {
		if (priv_check_cred(curthread->td_ucred, PRIV_MAXPROC) != 0 ||
		    nthreads_new >= maxthread) {
			atomic_subtract_int(&nthreads, 1);
			return (false);
		}
	}
	return (true);
}

static bool
thread_count_inc(void)
{
	static struct timeval lastfail;
	static int curfail;

	thread_reap();
	if (thread_count_inc_try()) {
		return (true);
	}

	thread_reap_all();
	if (thread_count_inc_try()) {
		return (true);
	}

	if (ppsratecheck(&lastfail, &curfail, 1)) {
		printf("maxthread limit exceeded by uid %u "
		    "(pid %d); consider increasing kern.maxthread\n",
		    curthread->td_ucred->cr_ruid, curproc->p_pid);
	}
	return (false);
}

static void
thread_count_sub(int n)
{

	atomic_subtract_int(&nthreads, n);
}

static void
thread_count_dec(void)
{

	thread_count_sub(1);
}

static lwpid_t
tid_alloc(void)
{
	static lwpid_t trytid;
	lwpid_t tid;

	mtx_lock(&tid_lock);
	/*
	 * It is an invariant that the bitmap is big enough to hold maxthread
	 * IDs. If we got to this point there has to be at least one free.
	 */
	if (trytid >= maxthread)
		trytid = 0;
	bit_ffc_at(tid_bitmap, trytid, maxthread, &tid);
	if (tid == -1) {
		KASSERT(trytid != 0, ("unexpectedly ran out of IDs"));
		trytid = 0;
		bit_ffc_at(tid_bitmap, trytid, maxthread, &tid);
		KASSERT(tid != -1, ("unexpectedly ran out of IDs"));
	}
	bit_set(tid_bitmap, tid);
	trytid = tid + 1;
	mtx_unlock(&tid_lock);
	return (tid + NO_PID);
}

static void
tid_free_locked(lwpid_t rtid)
{
	lwpid_t tid;

	mtx_assert(&tid_lock, MA_OWNED);
	KASSERT(rtid >= NO_PID,
	    ("%s: invalid tid %d\n", __func__, rtid));
	tid = rtid - NO_PID;
	KASSERT(bit_test(tid_bitmap, tid) != 0,
	    ("thread ID %d not allocated\n", rtid));
	bit_clear(tid_bitmap, tid);
}

static void
tid_free(lwpid_t rtid)
{

	mtx_lock(&tid_lock);
	tid_free_locked(rtid);
	mtx_unlock(&tid_lock);
}

static void
tid_free_batch(lwpid_t *batch, int n)
{
	int i;

	mtx_lock(&tid_lock);
	for (i = 0; i < n; i++) {
		tid_free_locked(batch[i]);
	}
	mtx_unlock(&tid_lock);
}

/*
 * Batching for thread reapping.
 */
struct tidbatch {
	lwpid_t tab[16];
	int n;
};

static void
tidbatch_prep(struct tidbatch *tb)
{

	tb->n = 0;
}

static void
tidbatch_add(struct tidbatch *tb, struct thread *td)
{

	KASSERT(tb->n < nitems(tb->tab),
	    ("%s: count too high %d", __func__, tb->n));
	tb->tab[tb->n] = td->td_tid;
	tb->n++;
}

static void
tidbatch_process(struct tidbatch *tb)
{

	KASSERT(tb->n <= nitems(tb->tab),
	    ("%s: count too high %d", __func__, tb->n));
	if (tb->n == nitems(tb->tab)) {
		tid_free_batch(tb->tab, tb->n);
		tb->n = 0;
	}
}

static void
tidbatch_final(struct tidbatch *tb)
{

	KASSERT(tb->n <= nitems(tb->tab),
	    ("%s: count too high %d", __func__, tb->n));
	if (tb->n != 0) {
		tid_free_batch(tb->tab, tb->n);
	}
}

/*
 * Batching thread count free, for consistency
 */
struct tdcountbatch {
	int n;
};

static void
tdcountbatch_prep(struct tdcountbatch *tb)
{

	tb->n = 0;
}

static void
tdcountbatch_add(struct tdcountbatch *tb, struct thread *td __unused)
{

	tb->n++;
}

static void
tdcountbatch_process(struct tdcountbatch *tb)
{

	if (tb->n == 32) {
		thread_count_sub(tb->n);
		tb->n = 0;
	}
}

static void
tdcountbatch_final(struct tdcountbatch *tb)
{

	if (tb->n != 0) {
		thread_count_sub(tb->n);
	}
}

/*
 * Prepare a thread for use.
 */
static int
thread_ctor(void *mem, int size, void *arg, int flags)
{
	struct thread	*td;

	td = (struct thread *)mem;
	TD_SET_STATE(td, TDS_INACTIVE);
	td->td_lastcpu = td->td_oncpu = NOCPU;

	/*
	 * Note that td_critnest begins life as 1 because the thread is not
	 * running and is thereby implicitly waiting to be on the receiving
	 * end of a context switch.
	 */
	td->td_critnest = 1;
	td->td_lend_user_pri = PRI_MAX;
#ifdef AUDIT
	audit_thread_alloc(td);
#endif
#ifdef KDTRACE_HOOKS
	kdtrace_thread_ctor(td);
#endif
	umtx_thread_alloc(td);
	MPASS(td->td_sel == NULL);
	return (0);
}

/*
 * Reclaim a thread after use.
 */
static void
thread_dtor(void *mem, int size, void *arg)
{
	struct thread *td;

	td = (struct thread *)mem;

#ifdef INVARIANTS
	/* Verify that this thread is in a safe state to free. */
	switch (TD_GET_STATE(td)) {
	case TDS_INHIBITED:
	case TDS_RUNNING:
	case TDS_CAN_RUN:
	case TDS_RUNQ:
		/*
		 * We must never unlink a thread that is in one of
		 * these states, because it is currently active.
		 */
		panic("bad state for thread unlinking");
		/* NOTREACHED */
	case TDS_INACTIVE:
		break;
	default:
		panic("bad thread state");
		/* NOTREACHED */
	}
#endif
#ifdef AUDIT
	audit_thread_free(td);
#endif
#ifdef KDTRACE_HOOKS
	kdtrace_thread_dtor(td);
#endif
	/* Free all OSD associated to this thread. */
	osd_thread_exit(td);
	ast_kclear(td);
	seltdfini(td);
}

/*
 * Initialize type-stable parts of a thread (when newly created).
 */
static int
thread_init(void *mem, int size, int flags)
{
	struct thread *td;

	td = (struct thread *)mem;

	td->td_allocdomain = vm_phys_domain(vtophys(td));
	td->td_sleepqueue = sleepq_alloc();
	td->td_turnstile = turnstile_alloc();
	EVENTHANDLER_DIRECT_INVOKE(thread_init, td);
	umtx_thread_init(td);
	td->td_kstack = 0;
	td->td_sel = NULL;
	return (0);
}

/*
 * Tear down type-stable parts of a thread (just before being discarded).
 */
static void
thread_fini(void *mem, int size)
{
	struct thread *td;

	td = (struct thread *)mem;
	EVENTHANDLER_DIRECT_INVOKE(thread_fini, td);
	turnstile_free(td->td_turnstile);
	sleepq_free(td->td_sleepqueue);
	umtx_thread_fini(td);
	MPASS(td->td_sel == NULL);
}

/*
 * For a newly created process,
 * link up all the structures and its initial threads etc.
 * called from:
 * {arch}/{arch}/machdep.c   {arch}_init(), init386() etc.
 * proc_dtor() (should go away)
 * proc_init()
 */
void
proc_linkup0(struct proc *p, struct thread *td)
{
	TAILQ_INIT(&p->p_threads);	     /* all threads in proc */
	proc_linkup(p, td);
}

void
proc_linkup(struct proc *p, struct thread *td)
{

	sigqueue_init(&p->p_sigqueue, p);
	p->p_ksi = ksiginfo_alloc(M_WAITOK);
	if (p->p_ksi != NULL) {
		/* XXX p_ksi may be null if ksiginfo zone is not ready */
		p->p_ksi->ksi_flags = KSI_EXT | KSI_INS;
	}
	LIST_INIT(&p->p_mqnotifier);
	p->p_numthreads = 0;
	thread_link(td, p);
}

static void
ast_suspend(struct thread *td, int tda __unused)
{
	struct proc *p;

	p = td->td_proc;
	/*
	 * We need to check to see if we have to exit or wait due to a
	 * single threading requirement or some other STOP condition.
	 */
	PROC_LOCK(p);
	thread_suspend_check(0);
	PROC_UNLOCK(p);
}

extern int max_threads_per_proc;

/*
 * Initialize global thread allocation resources.
 */
void
threadinit(void)
{
	u_long i;
	lwpid_t tid0;

	/*
	 * Place an upper limit on threads which can be allocated.
	 *
	 * Note that other factors may make the de facto limit much lower.
	 *
	 * Platform limits are somewhat arbitrary but deemed "more than good
	 * enough" for the foreseable future.
	 */
	if (maxthread == 0) {
#ifdef _LP64
		maxthread = MIN(maxproc * max_threads_per_proc, 1000000);
#else
		maxthread = MIN(maxproc * max_threads_per_proc, 100000);
#endif
	}

	mtx_init(&tid_lock, "TID lock", NULL, MTX_DEF);
	tid_bitmap = bit_alloc(maxthread, M_TIDHASH, M_WAITOK);
	/*
	 * Handle thread0.
	 */
	thread_count_inc();
	tid0 = tid_alloc();
	if (tid0 != THREAD0_TID)
		panic("tid0 %d != %d\n", tid0, THREAD0_TID);

	/*
	 * Thread structures are specially aligned so that (at least) the
	 * 5 lower bits of a pointer to 'struct thead' must be 0.  These bits
	 * are used by synchronization primitives to store flags in pointers to
	 * such structures.
	 */
	thread_zone = uma_zcreate("THREAD", sched_sizeof_thread(),
	    thread_ctor, thread_dtor, thread_init, thread_fini,
	    UMA_ALIGN_CACHE_AND_MASK(32 - 1), UMA_ZONE_NOFREE);
	tidhashtbl = hashinit(maxproc / 2, M_TIDHASH, &tidhash);
	tidhashlock = (tidhash + 1) / 64;
	if (tidhashlock > 0)
		tidhashlock--;
	tidhashtbl_lock = malloc(sizeof(*tidhashtbl_lock) * (tidhashlock + 1),
	    M_TIDHASH, M_WAITOK | M_ZERO);
	for (i = 0; i < tidhashlock + 1; i++)
		rw_init(&tidhashtbl_lock[i], "tidhash");

	TASK_INIT(&thread_reap_task, 0, thread_reap_task_cb, NULL);
	callout_init(&thread_reap_callout, 1);
	callout_reset(&thread_reap_callout, 5 * hz,
	    thread_reap_callout_cb, NULL);
	ast_register(TDA_SUSPEND, ASTR_ASTF_REQUIRED, 0, ast_suspend);
}

/*
 * Place an unused thread on the zombie list.
 */
void
thread_zombie(struct thread *td)
{
	struct thread_domain_data *tdd;
	struct thread *ztd;

	tdd = &thread_domain_data[td->td_allocdomain];
	ztd = atomic_load_ptr(&tdd->tdd_zombies);
	for (;;) {
		td->td_zombie = ztd;
		if (atomic_fcmpset_rel_ptr((uintptr_t *)&tdd->tdd_zombies,
		    (uintptr_t *)&ztd, (uintptr_t)td))
			break;
		continue;
	}
}

/*
 * Release a thread that has exited after cpu_throw().
 */
void
thread_stash(struct thread *td)
{
	atomic_subtract_rel_int(&td->td_proc->p_exitthreads, 1);
	thread_zombie(td);
}

/*
 * Reap zombies from passed domain.
 */
static void
thread_reap_domain(struct thread_domain_data *tdd)
{
	struct thread *itd, *ntd;
	struct tidbatch tidbatch;
	struct credbatch credbatch;
	struct limbatch limbatch;
	struct tdcountbatch tdcountbatch;

	/*
	 * Reading upfront is pessimal if followed by concurrent atomic_swap,
	 * but most of the time the list is empty.
	 */
	if (tdd->tdd_zombies == NULL)
		return;

	itd = (struct thread *)atomic_swap_ptr((uintptr_t *)&tdd->tdd_zombies,
	    (uintptr_t)NULL);
	if (itd == NULL)
		return;

	/*
	 * Multiple CPUs can get here, the race is fine as ticks is only
	 * advisory.
	 */
	tdd->tdd_reapticks = ticks;

	tidbatch_prep(&tidbatch);
	credbatch_prep(&credbatch);
	limbatch_prep(&limbatch);
	tdcountbatch_prep(&tdcountbatch);

	while (itd != NULL) {
		ntd = itd->td_zombie;
		EVENTHANDLER_DIRECT_INVOKE(thread_dtor, itd);

		tidbatch_add(&tidbatch, itd);
		credbatch_add(&credbatch, itd);
		limbatch_add(&limbatch, itd);
		tdcountbatch_add(&tdcountbatch, itd);

		thread_free_batched(itd);

		tidbatch_process(&tidbatch);
		credbatch_process(&credbatch);
		limbatch_process(&limbatch);
		tdcountbatch_process(&tdcountbatch);

		itd = ntd;
	}

	tidbatch_final(&tidbatch);
	credbatch_final(&credbatch);
	limbatch_final(&limbatch);
	tdcountbatch_final(&tdcountbatch);
}

/*
 * Reap zombies from all domains.
 */
static void
thread_reap_all(void)
{
	struct thread_domain_data *tdd;
	int i, domain;

	domain = PCPU_GET(domain);
	for (i = 0; i < vm_ndomains; i++) {
		tdd = &thread_domain_data[(i + domain) % vm_ndomains];
		thread_reap_domain(tdd);
	}
}

/*
 * Reap zombies from local domain.
 */
static void
thread_reap(void)
{
	struct thread_domain_data *tdd;
	int domain;

	domain = PCPU_GET(domain);
	tdd = &thread_domain_data[domain];

	thread_reap_domain(tdd);
}

static void
thread_reap_task_cb(void *arg __unused, int pending __unused)
{

	thread_reap_all();
}

static void
thread_reap_callout_cb(void *arg __unused)
{
	struct thread_domain_data *tdd;
	int i, cticks, lticks;
	bool wantreap;

	wantreap = false;
	cticks = atomic_load_int(&ticks);
	for (i = 0; i < vm_ndomains; i++) {
		tdd = &thread_domain_data[i];
		lticks = tdd->tdd_reapticks;
		if (tdd->tdd_zombies != NULL &&
		    (u_int)(cticks - lticks) > 5 * hz) {
			wantreap = true;
			break;
		}
	}

	if (wantreap)
		taskqueue_enqueue(taskqueue_thread, &thread_reap_task);
	callout_reset(&thread_reap_callout, 5 * hz,
	    thread_reap_callout_cb, NULL);
}

/*
 * Calling this function guarantees that any thread that exited before
 * the call is reaped when the function returns.  By 'exited' we mean
 * a thread removed from the process linkage with thread_unlink().
 * Practically this means that caller must lock/unlock corresponding
 * process lock before the call, to synchronize with thread_exit().
 */
void
thread_reap_barrier(void)
{
	struct task *t;

	/*
	 * First do context switches to each CPU to ensure that all
	 * PCPU pc_deadthreads are moved to zombie list.
	 */
	quiesce_all_cpus("", PDROP);

	/*
	 * Second, fire the task in the same thread as normal
	 * thread_reap() is done, to serialize reaping.
	 */
	t = malloc(sizeof(*t), M_TEMP, M_WAITOK);
	TASK_INIT(t, 0, thread_reap_task_cb, t);
	taskqueue_enqueue(taskqueue_thread, t);
	taskqueue_drain(taskqueue_thread, t);
	free(t, M_TEMP);
}

/*
 * Allocate a thread.
 */
struct thread *
thread_alloc(int pages)
{
	struct thread *td;
	lwpid_t tid;

	if (!thread_count_inc()) {
		return (NULL);
	}

	tid = tid_alloc();
	td = uma_zalloc(thread_zone, M_WAITOK);
	KASSERT(td->td_kstack == 0, ("thread_alloc got thread with kstack"));
	if (!vm_thread_new(td, pages)) {
		uma_zfree(thread_zone, td);
		tid_free(tid);
		thread_count_dec();
		return (NULL);
	}
	td->td_tid = tid;
	bzero(&td->td_sa.args, sizeof(td->td_sa.args));
	kasan_thread_alloc(td);
	kmsan_thread_alloc(td);
	cpu_thread_alloc(td);
	EVENTHANDLER_DIRECT_INVOKE(thread_ctor, td);
	return (td);
}

int
thread_recycle(struct thread *td, int pages)
{
	if (td->td_kstack == 0 || td->td_kstack_pages != pages) {
		if (td->td_kstack != 0)
			vm_thread_dispose(td);
		if (!vm_thread_new(td, pages))
			return (ENOMEM);
		cpu_thread_alloc(td);
	}
	kasan_thread_alloc(td);
	kmsan_thread_alloc(td);
	return (0);
}

/*
 * Deallocate a thread.
 */
static void
thread_free_batched(struct thread *td)
{

	lock_profile_thread_exit(td);
	if (td->td_cpuset)
		cpuset_rel(td->td_cpuset);
	td->td_cpuset = NULL;
	cpu_thread_free(td);
	if (td->td_kstack != 0)
		vm_thread_dispose(td);
	callout_drain(&td->td_slpcallout);
	/*
	 * Freeing handled by the caller.
	 */
	td->td_tid = -1;
	kmsan_thread_free(td);
	uma_zfree(thread_zone, td);
}

void
thread_free(struct thread *td)
{
	lwpid_t tid;

	EVENTHANDLER_DIRECT_INVOKE(thread_dtor, td);
	tid = td->td_tid;
	thread_free_batched(td);
	tid_free(tid);
	thread_count_dec();
}

void
thread_cow_get_proc(struct thread *newtd, struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	newtd->td_realucred = crcowget(p->p_ucred);
	newtd->td_ucred = newtd->td_realucred;
	newtd->td_limit = lim_hold(p->p_limit);
	newtd->td_cowgen = p->p_cowgen;
}

void
thread_cow_get(struct thread *newtd, struct thread *td)
{

	MPASS(td->td_realucred == td->td_ucred);
	newtd->td_realucred = crcowget(td->td_realucred);
	newtd->td_ucred = newtd->td_realucred;
	newtd->td_limit = lim_hold(td->td_limit);
	newtd->td_cowgen = td->td_cowgen;
}

void
thread_cow_free(struct thread *td)
{

	if (td->td_realucred != NULL)
		crcowfree(td);
	if (td->td_limit != NULL)
		lim_free(td->td_limit);
}

void
thread_cow_update(struct thread *td)
{
	struct proc *p;
	struct ucred *oldcred;
	struct plimit *oldlimit;

	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = crcowsync();
	oldlimit = lim_cowsync();
	td->td_cowgen = p->p_cowgen;
	PROC_UNLOCK(p);
	if (oldcred != NULL)
		crfree(oldcred);
	if (oldlimit != NULL)
		lim_free(oldlimit);
}

void
thread_cow_synced(struct thread *td)
{
	struct proc *p;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS(td->td_cowgen != p->p_cowgen);
	MPASS(td->td_ucred == p->p_ucred);
	MPASS(td->td_limit == p->p_limit);
	td->td_cowgen = p->p_cowgen;
}

/*
 * Discard the current thread and exit from its context.
 * Always called with scheduler locked.
 *
 * Because we can't free a thread while we're operating under its context,
 * push the current thread into our CPU's deadthread holder. This means
 * we needn't worry about someone else grabbing our context before we
 * do a cpu_throw().
 */
void
thread_exit(void)
{
	uint64_t runtime, new_switchtime;
	struct thread *td;
	struct thread *td2;
	struct proc *p;

	td = curthread;
	p = td->td_proc;

	PROC_SLOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&Giant, MA_NOTOWNED);

	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(p != NULL, ("thread exiting without a process"));
	CTR3(KTR_PROC, "thread_exit: thread %p (pid %ld, %s)", td,
	    (long)p->p_pid, td->td_name);
	SDT_PROBE0(proc, , , lwp__exit);
	KASSERT(TAILQ_EMPTY(&td->td_sigqueue.sq_list), ("signal pending"));
	MPASS(td->td_realucred == td->td_ucred);

	/*
	 * drop FPU & debug register state storage, or any other
	 * architecture specific resources that
	 * would not be on a new untouched process.
	 */
	cpu_thread_exit(td);

	/*
	 * The last thread is left attached to the process
	 * So that the whole bundle gets recycled. Skip
	 * all this stuff if we never had threads.
	 * EXIT clears all sign of other threads when
	 * it goes to single threading, so the last thread always
	 * takes the short path.
	 */
	if (p->p_flag & P_HADTHREADS) {
		if (p->p_numthreads > 1) {
			atomic_add_int(&td->td_proc->p_exitthreads, 1);
			thread_unlink(td);
			td2 = FIRST_THREAD_IN_PROC(p);
			sched_exit_thread(td2, td);

			/*
			 * The test below is NOT true if we are the
			 * sole exiting thread. P_STOPPED_SINGLE is unset
			 * in exit1() after it is the only survivor.
			 */
			if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
				if (p->p_numthreads == p->p_suspcount) {
					thread_lock(p->p_singlethread);
					thread_unsuspend_one(p->p_singlethread,
					    p, false);
				}
			}

			PCPU_SET(deadthread, td);
		} else {
			/*
			 * The last thread is exiting.. but not through exit()
			 */
			panic ("thread_exit: Last thread exiting on its own");
		}
	} 
#ifdef	HWPMC_HOOKS
	/*
	 * If this thread is part of a process that is being tracked by hwpmc(4),
	 * inform the module of the thread's impending exit.
	 */
	if (PMC_PROC_IS_USING_PMCS(td->td_proc)) {
		PMC_SWITCH_CONTEXT(td, PMC_FN_CSW_OUT);
		PMC_CALL_HOOK_UNLOCKED(td, PMC_FN_THR_EXIT, NULL);
	} else if (PMC_SYSTEM_SAMPLING_ACTIVE())
		PMC_CALL_HOOK_UNLOCKED(td, PMC_FN_THR_EXIT_LOG, NULL);
#endif
	PROC_UNLOCK(p);
	PROC_STATLOCK(p);
	thread_lock(td);
	PROC_SUNLOCK(p);

	/* Do the same timestamp bookkeeping that mi_switch() would do. */
	new_switchtime = cpu_ticks();
	runtime = new_switchtime - PCPU_GET(switchtime);
	td->td_runtime += runtime;
	td->td_incruntime += runtime;
	PCPU_SET(switchtime, new_switchtime);
	PCPU_SET(switchticks, ticks);
	VM_CNT_INC(v_swtch);

	/* Save our resource usage in our process. */
	td->td_ru.ru_nvcsw++;
	ruxagg_locked(p, td);
	rucollect(&p->p_ru, &td->td_ru);
	PROC_STATUNLOCK(p);

	TD_SET_STATE(td, TDS_INACTIVE);
#ifdef WITNESS
	witness_thread_exit(td);
#endif
	CTR1(KTR_PROC, "thread_exit: cpu_throw() thread %p", td);
	sched_throw(td);
	panic("I'm a teapot!");
	/* NOTREACHED */
}

/*
 * Do any thread specific cleanups that may be needed in wait()
 * called with Giant, proc and schedlock not held.
 */
void
thread_wait(struct proc *p)
{
	struct thread *td;

	mtx_assert(&Giant, MA_NOTOWNED);
	KASSERT(p->p_numthreads == 1, ("multiple threads in thread_wait()"));
	KASSERT(p->p_exitthreads == 0, ("p_exitthreads leaking"));
	td = FIRST_THREAD_IN_PROC(p);
	/* Lock the last thread so we spin until it exits cpu_throw(). */
	thread_lock(td);
	thread_unlock(td);
	lock_profile_thread_exit(td);
	cpuset_rel(td->td_cpuset);
	td->td_cpuset = NULL;
	cpu_thread_clean(td);
	thread_cow_free(td);
	callout_drain(&td->td_slpcallout);
	thread_reap();	/* check for zombie threads etc. */
}

/*
 * Link a thread to a process.
 * set up anything that needs to be initialized for it to
 * be used by the process.
 */
void
thread_link(struct thread *td, struct proc *p)
{

	/*
	 * XXX This can't be enabled because it's called for proc0 before
	 * its lock has been created.
	 * PROC_LOCK_ASSERT(p, MA_OWNED);
	 */
	TD_SET_STATE(td, TDS_INACTIVE);
	td->td_proc     = p;
	td->td_flags    = TDF_INMEM;

	LIST_INIT(&td->td_contested);
	LIST_INIT(&td->td_lprof[0]);
	LIST_INIT(&td->td_lprof[1]);
#ifdef EPOCH_TRACE
	SLIST_INIT(&td->td_epochs);
#endif
	sigqueue_init(&td->td_sigqueue, p);
	callout_init(&td->td_slpcallout, 1);
	TAILQ_INSERT_TAIL(&p->p_threads, td, td_plist);
	p->p_numthreads++;
}

/*
 * Called from:
 *  thread_exit()
 */
void
thread_unlink(struct thread *td)
{
	struct proc *p = td->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);
#ifdef EPOCH_TRACE
	MPASS(SLIST_EMPTY(&td->td_epochs));
#endif

	TAILQ_REMOVE(&p->p_threads, td, td_plist);
	p->p_numthreads--;
	/* could clear a few other things here */
	/* Must  NOT clear links to proc! */
}

static int
calc_remaining(struct proc *p, int mode)
{
	int remaining;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	if (mode == SINGLE_EXIT)
		remaining = p->p_numthreads;
	else if (mode == SINGLE_BOUNDARY)
		remaining = p->p_numthreads - p->p_boundary_count;
	else if (mode == SINGLE_NO_EXIT || mode == SINGLE_ALLPROC)
		remaining = p->p_numthreads - p->p_suspcount;
	else
		panic("calc_remaining: wrong mode %d", mode);
	return (remaining);
}

static int
remain_for_mode(int mode)
{

	return (mode == SINGLE_ALLPROC ? 0 : 1);
}

static void
weed_inhib(int mode, struct thread *td2, struct proc *p)
{
	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	THREAD_LOCK_ASSERT(td2, MA_OWNED);

	/*
	 * Since the thread lock is dropped by the scheduler we have
	 * to retry to check for races.
	 */
restart:
	switch (mode) {
	case SINGLE_EXIT:
		if (TD_IS_SUSPENDED(td2)) {
			thread_unsuspend_one(td2, p, true);
			thread_lock(td2);
			goto restart;
		}
		if (TD_CAN_ABORT(td2)) {
			sleepq_abort(td2, EINTR);
			return;
		}
		break;
	case SINGLE_BOUNDARY:
	case SINGLE_NO_EXIT:
		if (TD_IS_SUSPENDED(td2) &&
		    (td2->td_flags & TDF_BOUNDARY) == 0) {
			thread_unsuspend_one(td2, p, false);
			thread_lock(td2);
			goto restart;
		}
		if (TD_CAN_ABORT(td2)) {
			sleepq_abort(td2, ERESTART);
			return;
		}
		break;
	case SINGLE_ALLPROC:
		/*
		 * ALLPROC suspend tries to avoid spurious EINTR for
		 * threads sleeping interruptable, by suspending the
		 * thread directly, similarly to sig_suspend_threads().
		 * Since such sleep is not neccessary performed at the user
		 * boundary, TDF_ALLPROCSUSP is used to avoid immediate
		 * un-suspend.
		 */
		if (TD_IS_SUSPENDED(td2) &&
		    (td2->td_flags & TDF_ALLPROCSUSP) == 0) {
			thread_unsuspend_one(td2, p, false);
			thread_lock(td2);
			goto restart;
		}
		if (TD_CAN_ABORT(td2)) {
			td2->td_flags |= TDF_ALLPROCSUSP;
			sleepq_abort(td2, ERESTART);
			return;
		}
		break;
	default:
		break;
	}
	thread_unlock(td2);
}

/*
 * Enforce single-threading.
 *
 * Returns 1 if the caller must abort (another thread is waiting to
 * exit the process or similar). Process is locked!
 * Returns 0 when you are successfully the only thread running.
 * A process has successfully single threaded in the suspend mode when
 * There are no threads in user mode. Threads in the kernel must be
 * allowed to continue until they get to the user boundary. They may even
 * copy out their return values and data before suspending. They may however be
 * accelerated in reaching the user boundary as we will wake up
 * any sleeping threads that are interruptable. (PCATCH).
 */
int
thread_single(struct proc *p, int mode)
{
	struct thread *td;
	struct thread *td2;
	int remaining;

	td = curthread;
	KASSERT(mode == SINGLE_EXIT || mode == SINGLE_BOUNDARY ||
	    mode == SINGLE_ALLPROC || mode == SINGLE_NO_EXIT,
	    ("invalid mode %d", mode));
	/*
	 * If allowing non-ALLPROC singlethreading for non-curproc
	 * callers, calc_remaining() and remain_for_mode() should be
	 * adjusted to also account for td->td_proc != p.  For now
	 * this is not implemented because it is not used.
	 */
	KASSERT((mode == SINGLE_ALLPROC && td->td_proc != p) ||
	    (mode != SINGLE_ALLPROC && td->td_proc == p),
	    ("mode %d proc %p curproc %p", mode, p, td->td_proc));
	mtx_assert(&Giant, MA_NOTOWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * Is someone already single threading?
	 * Or may be singlethreading is not needed at all.
	 */
	if (mode == SINGLE_ALLPROC) {
		while ((p->p_flag & P_STOPPED_SINGLE) != 0) {
			if ((p->p_flag2 & P2_WEXIT) != 0)
				return (1);
			msleep(&p->p_flag, &p->p_mtx, PCATCH, "thrsgl", 0);
		}
		if ((p->p_flag & (P_STOPPED_SIG | P_TRACED)) != 0 ||
		    (p->p_flag2 & P2_WEXIT) != 0)
			return (1);
	} else if ((p->p_flag & P_HADTHREADS) == 0)
		return (0);
	if (p->p_singlethread != NULL && p->p_singlethread != td)
		return (1);

	if (mode == SINGLE_EXIT) {
		p->p_flag |= P_SINGLE_EXIT;
		p->p_flag &= ~P_SINGLE_BOUNDARY;
	} else {
		p->p_flag &= ~P_SINGLE_EXIT;
		if (mode == SINGLE_BOUNDARY)
			p->p_flag |= P_SINGLE_BOUNDARY;
		else
			p->p_flag &= ~P_SINGLE_BOUNDARY;
	}
	if (mode == SINGLE_ALLPROC)
		p->p_flag |= P_TOTAL_STOP;
	p->p_flag |= P_STOPPED_SINGLE;
	PROC_SLOCK(p);
	p->p_singlethread = td;
	remaining = calc_remaining(p, mode);
	while (remaining != remain_for_mode(mode)) {
		if (P_SHOULDSTOP(p) != P_STOPPED_SINGLE)
			goto stopme;
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2 == td)
				continue;
			thread_lock(td2);
			ast_sched_locked(td2, TDA_SUSPEND);
			if (TD_IS_INHIBITED(td2)) {
				weed_inhib(mode, td2, p);
#ifdef SMP
			} else if (TD_IS_RUNNING(td2)) {
				forward_signal(td2);
				thread_unlock(td2);
#endif
			} else
				thread_unlock(td2);
		}
		remaining = calc_remaining(p, mode);

		/*
		 * Maybe we suspended some threads.. was it enough?
		 */
		if (remaining == remain_for_mode(mode))
			break;

stopme:
		/*
		 * Wake us up when everyone else has suspended.
		 * In the mean time we suspend as well.
		 */
		thread_suspend_switch(td, p);
		remaining = calc_remaining(p, mode);
	}
	if (mode == SINGLE_EXIT) {
		/*
		 * Convert the process to an unthreaded process.  The
		 * SINGLE_EXIT is called by exit1() or execve(), in
		 * both cases other threads must be retired.
		 */
		KASSERT(p->p_numthreads == 1, ("Unthreading with >1 threads"));
		p->p_singlethread = NULL;
		p->p_flag &= ~(P_STOPPED_SINGLE | P_SINGLE_EXIT | P_HADTHREADS);

		/*
		 * Wait for any remaining threads to exit cpu_throw().
		 */
		while (p->p_exitthreads != 0) {
			PROC_SUNLOCK(p);
			PROC_UNLOCK(p);
			sched_relinquish(td);
			PROC_LOCK(p);
			PROC_SLOCK(p);
		}
	} else if (mode == SINGLE_BOUNDARY) {
		/*
		 * Wait until all suspended threads are removed from
		 * the processors.  The thread_suspend_check()
		 * increments p_boundary_count while it is still
		 * running, which makes it possible for the execve()
		 * to destroy vmspace while our other threads are
		 * still using the address space.
		 *
		 * We lock the thread, which is only allowed to
		 * succeed after context switch code finished using
		 * the address space.
		 */
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2 == td)
				continue;
			thread_lock(td2);
			KASSERT((td2->td_flags & TDF_BOUNDARY) != 0,
			    ("td %p not on boundary", td2));
			KASSERT(TD_IS_SUSPENDED(td2),
			    ("td %p is not suspended", td2));
			thread_unlock(td2);
		}
	}
	PROC_SUNLOCK(p);
	return (0);
}

bool
thread_suspend_check_needed(void)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	return (P_SHOULDSTOP(p) || ((p->p_flag & P_TRACED) != 0 &&
	    (td->td_dbgflags & TDB_SUSPEND) != 0));
}

/*
 * Called in from locations that can safely check to see
 * whether we have to suspend or at least throttle for a
 * single-thread event (e.g. fork).
 *
 * Such locations include userret().
 * If the "return_instead" argument is non zero, the thread must be able to
 * accept 0 (caller may continue), or 1 (caller must abort) as a result.
 *
 * The 'return_instead' argument tells the function if it may do a
 * thread_exit() or suspend, or whether the caller must abort and back
 * out instead.
 *
 * If the thread that set the single_threading request has set the
 * P_SINGLE_EXIT bit in the process flags then this call will never return
 * if 'return_instead' is false, but will exit.
 *
 * P_SINGLE_EXIT | return_instead == 0| return_instead != 0
 *---------------+--------------------+---------------------
 *       0       | returns 0          |   returns 0 or 1
 *               | when ST ends       |   immediately
 *---------------+--------------------+---------------------
 *       1       | thread exits       |   returns 1
 *               |                    |  immediately
 * 0 = thread_exit() or suspension ok,
 * other = return error instead of stopping the thread.
 *
 * While a full suspension is under effect, even a single threading
 * thread would be suspended if it made this call (but it shouldn't).
 * This call should only be made from places where
 * thread_exit() would be safe as that may be the outcome unless
 * return_instead is set.
 */
int
thread_suspend_check(int return_instead)
{
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	mtx_assert(&Giant, MA_NOTOWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	while (thread_suspend_check_needed()) {
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			KASSERT(p->p_singlethread != NULL,
			    ("singlethread not set"));
			/*
			 * The only suspension in action is a
			 * single-threading. Single threader need not stop.
			 * It is safe to access p->p_singlethread unlocked
			 * because it can only be set to our address by us.
			 */
			if (p->p_singlethread == td)
				return (0);	/* Exempt from stopping. */
		}
		if ((p->p_flag & P_SINGLE_EXIT) && return_instead)
			return (EINTR);

		/* Should we goto user boundary if we didn't come from there? */
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE &&
		    (p->p_flag & P_SINGLE_BOUNDARY) && return_instead)
			return (ERESTART);

		/*
		 * Ignore suspend requests if they are deferred.
		 */
		if ((td->td_flags & TDF_SBDRY) != 0) {
			KASSERT(return_instead,
			    ("TDF_SBDRY set for unsafe thread_suspend_check"));
			KASSERT((td->td_flags & (TDF_SEINTR | TDF_SERESTART)) !=
			    (TDF_SEINTR | TDF_SERESTART),
			    ("both TDF_SEINTR and TDF_SERESTART"));
			return (TD_SBDRY_INTR(td) ? TD_SBDRY_ERRNO(td) : 0);
		}

		/*
		 * If the process is waiting for us to exit,
		 * this thread should just suicide.
		 * Assumes that P_SINGLE_EXIT implies P_STOPPED_SINGLE.
		 */
		if ((p->p_flag & P_SINGLE_EXIT) && (p->p_singlethread != td)) {
			PROC_UNLOCK(p);

			/*
			 * Allow Linux emulation layer to do some work
			 * before thread suicide.
			 */
			if (__predict_false(p->p_sysent->sv_thread_detach != NULL))
				(p->p_sysent->sv_thread_detach)(td);
			umtx_thread_exit(td);
			kern_thr_exit(td);
			panic("stopped thread did not exit");
		}

		PROC_SLOCK(p);
		thread_stopped(p);
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			if (p->p_numthreads == p->p_suspcount + 1) {
				thread_lock(p->p_singlethread);
				thread_unsuspend_one(p->p_singlethread, p,
				    false);
			}
		}
		PROC_UNLOCK(p);
		thread_lock(td);
		/*
		 * When a thread suspends, it just
		 * gets taken off all queues.
		 */
		thread_suspend_one(td);
		if (return_instead == 0) {
			p->p_boundary_count++;
			td->td_flags |= TDF_BOUNDARY;
		}
		PROC_SUNLOCK(p);
		mi_switch(SW_INVOL | SWT_SUSPEND);
		PROC_LOCK(p);
	}
	return (0);
}

/*
 * Check for possible stops and suspensions while executing a
 * casueword or similar transiently failing operation.
 *
 * The sleep argument controls whether the function can handle a stop
 * request itself or it should return ERESTART and the request is
 * proceed at the kernel/user boundary in ast.
 *
 * Typically, when retrying due to casueword(9) failure (rv == 1), we
 * should handle the stop requests there, with exception of cases when
 * the thread owns a kernel resource, for instance busied the umtx
 * key, or when functions return immediately if thread_check_susp()
 * returned non-zero.  On the other hand, retrying the whole lock
 * operation, we better not stop there but delegate the handling to
 * ast.
 *
 * If the request is for thread termination P_SINGLE_EXIT, we cannot
 * handle it at all, and simply return EINTR.
 */
int
thread_check_susp(struct thread *td, bool sleep)
{
	struct proc *p;
	int error;

	/*
	 * The check for TDA_SUSPEND is racy, but it is enough to
	 * eventually break the lockstep loop.
	 */
	if (!td_ast_pending(td, TDA_SUSPEND))
		return (0);
	error = 0;
	p = td->td_proc;
	PROC_LOCK(p);
	if (p->p_flag & P_SINGLE_EXIT)
		error = EINTR;
	else if (P_SHOULDSTOP(p) ||
	    ((p->p_flag & P_TRACED) && (td->td_dbgflags & TDB_SUSPEND)))
		error = sleep ? thread_suspend_check(0) : ERESTART;
	PROC_UNLOCK(p);
	return (error);
}

void
thread_suspend_switch(struct thread *td, struct proc *p)
{

	KASSERT(!TD_IS_SUSPENDED(td), ("already suspended"));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	/*
	 * We implement thread_suspend_one in stages here to avoid
	 * dropping the proc lock while the thread lock is owned.
	 */
	if (p == td->td_proc) {
		thread_stopped(p);
		p->p_suspcount++;
	}
	PROC_UNLOCK(p);
	thread_lock(td);
	ast_unsched_locked(td, TDA_SUSPEND);
	TD_SET_SUSPENDED(td);
	sched_sleep(td, 0);
	PROC_SUNLOCK(p);
	DROP_GIANT();
	mi_switch(SW_VOL | SWT_SUSPEND);
	PICKUP_GIANT();
	PROC_LOCK(p);
	PROC_SLOCK(p);
}

void
thread_suspend_one(struct thread *td)
{
	struct proc *p;

	p = td->td_proc;
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(!TD_IS_SUSPENDED(td), ("already suspended"));
	p->p_suspcount++;
	ast_unsched_locked(td, TDA_SUSPEND);
	TD_SET_SUSPENDED(td);
	sched_sleep(td, 0);
}

static void
thread_unsuspend_one(struct thread *td, struct proc *p, bool boundary)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(TD_IS_SUSPENDED(td), ("Thread not suspended"));
	TD_CLR_SUSPENDED(td);
	td->td_flags &= ~TDF_ALLPROCSUSP;
	if (td->td_proc == p) {
		PROC_SLOCK_ASSERT(p, MA_OWNED);
		p->p_suspcount--;
		if (boundary && (td->td_flags & TDF_BOUNDARY) != 0) {
			td->td_flags &= ~TDF_BOUNDARY;
			p->p_boundary_count--;
		}
	}
	setrunnable(td, 0);
}

void
thread_run_flash(struct thread *td)
{
	struct proc *p;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (TD_ON_SLEEPQ(td))
		sleepq_remove_nested(td);
	else
		thread_lock(td);

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(TD_IS_SUSPENDED(td), ("Thread not suspended"));

	TD_CLR_SUSPENDED(td);
	PROC_SLOCK(p);
	MPASS(p->p_suspcount > 0);
	p->p_suspcount--;
	PROC_SUNLOCK(p);
	setrunnable(td, 0);
}

/*
 * Allow all threads blocked by single threading to continue running.
 */
void
thread_unsuspend(struct proc *p)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	if (!P_SHOULDSTOP(p)) {
                FOREACH_THREAD_IN_PROC(p, td) {
			thread_lock(td);
			if (TD_IS_SUSPENDED(td))
				thread_unsuspend_one(td, p, true);
			else
				thread_unlock(td);
		}
	} else if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE &&
	    p->p_numthreads == p->p_suspcount) {
		/*
		 * Stopping everything also did the job for the single
		 * threading request. Now we've downgraded to single-threaded,
		 * let it continue.
		 */
		if (p->p_singlethread->td_proc == p) {
			thread_lock(p->p_singlethread);
			thread_unsuspend_one(p->p_singlethread, p, false);
		}
	}
}

/*
 * End the single threading mode..
 */
void
thread_single_end(struct proc *p, int mode)
{
	struct thread *td;

	KASSERT(mode == SINGLE_EXIT || mode == SINGLE_BOUNDARY ||
	    mode == SINGLE_ALLPROC || mode == SINGLE_NO_EXIT,
	    ("invalid mode %d", mode));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT((mode == SINGLE_ALLPROC && (p->p_flag & P_TOTAL_STOP) != 0) ||
	    (mode != SINGLE_ALLPROC && (p->p_flag & P_TOTAL_STOP) == 0),
	    ("mode %d does not match P_TOTAL_STOP", mode));
	KASSERT(mode == SINGLE_ALLPROC || p->p_singlethread == curthread,
	    ("thread_single_end from other thread %p %p",
	    curthread, p->p_singlethread));
	KASSERT(mode != SINGLE_BOUNDARY ||
	    (p->p_flag & P_SINGLE_BOUNDARY) != 0,
	    ("mis-matched SINGLE_BOUNDARY flags %x", p->p_flag));
	p->p_flag &= ~(P_STOPPED_SINGLE | P_SINGLE_EXIT | P_SINGLE_BOUNDARY |
	    P_TOTAL_STOP);
	PROC_SLOCK(p);
	p->p_singlethread = NULL;

	/*
	 * If there are other threads they may now run,
	 * unless of course there is a blanket 'stop order'
	 * on the process. The single threader must be allowed
	 * to continue however as this is a bad place to stop.
	 */
	if (p->p_numthreads != remain_for_mode(mode) && !P_SHOULDSTOP(p)) {
                FOREACH_THREAD_IN_PROC(p, td) {
			thread_lock(td);
			if (TD_IS_SUSPENDED(td))
				thread_unsuspend_one(td, p, true);
			else
				thread_unlock(td);
		}
	}
	KASSERT(mode != SINGLE_BOUNDARY || p->p_boundary_count == 0,
	    ("inconsistent boundary count %d", p->p_boundary_count));
	PROC_SUNLOCK(p);
	wakeup(&p->p_flag);
}

/*
 * Locate a thread by number and return with proc lock held.
 *
 * thread exit establishes proc -> tidhash lock ordering, but lookup
 * takes tidhash first and needs to return locked proc.
 *
 * The problem is worked around by relying on type-safety of both
 * structures and doing the work in 2 steps:
 * - tidhash-locked lookup which saves both thread and proc pointers
 * - proc-locked verification that the found thread still matches
 */
static bool
tdfind_hash(lwpid_t tid, pid_t pid, struct proc **pp, struct thread **tdp)
{
#define RUN_THRESH	16
	struct proc *p;
	struct thread *td;
	int run;
	bool locked;

	run = 0;
	rw_rlock(TIDHASHLOCK(tid));
	locked = true;
	LIST_FOREACH(td, TIDHASH(tid), td_hash) {
		if (td->td_tid != tid) {
			run++;
			continue;
		}
		p = td->td_proc;
		if (pid != -1 && p->p_pid != pid) {
			td = NULL;
			break;
		}
		if (run > RUN_THRESH) {
			if (rw_try_upgrade(TIDHASHLOCK(tid))) {
				LIST_REMOVE(td, td_hash);
				LIST_INSERT_HEAD(TIDHASH(td->td_tid),
					td, td_hash);
				rw_wunlock(TIDHASHLOCK(tid));
				locked = false;
				break;
			}
		}
		break;
	}
	if (locked)
		rw_runlock(TIDHASHLOCK(tid));
	if (td == NULL)
		return (false);
	*pp = p;
	*tdp = td;
	return (true);
}

struct thread *
tdfind(lwpid_t tid, pid_t pid)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	if (td->td_tid == tid) {
		if (pid != -1 && td->td_proc->p_pid != pid)
			return (NULL);
		PROC_LOCK(td->td_proc);
		return (td);
	}

	for (;;) {
		if (!tdfind_hash(tid, pid, &p, &td))
			return (NULL);
		PROC_LOCK(p);
		if (td->td_tid != tid) {
			PROC_UNLOCK(p);
			continue;
		}
		if (td->td_proc != p) {
			PROC_UNLOCK(p);
			continue;
		}
		if (p->p_state == PRS_NEW) {
			PROC_UNLOCK(p);
			return (NULL);
		}
		return (td);
	}
}

void
tidhash_add(struct thread *td)
{
	rw_wlock(TIDHASHLOCK(td->td_tid));
	LIST_INSERT_HEAD(TIDHASH(td->td_tid), td, td_hash);
	rw_wunlock(TIDHASHLOCK(td->td_tid));
}

void
tidhash_remove(struct thread *td)
{

	rw_wlock(TIDHASHLOCK(td->td_tid));
	LIST_REMOVE(td, td_hash);
	rw_wunlock(TIDHASHLOCK(td->td_tid));
}
