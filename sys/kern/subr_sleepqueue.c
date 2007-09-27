/*-
 * Copyright (c) 2004 John Baldwin <jhb@FreeBSD.org>
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
 * Implementation of sleep queues used to hold queue of threads blocked on
 * a wait channel.  Sleep queues different from turnstiles in that wait
 * channels are not owned by anyone, so there is no priority propagation.
 * Sleep queues can also provide a timeout and can also be interrupted by
 * signals.  That said, there are several similarities between the turnstile
 * and sleep queue implementations.  (Note: turnstiles were implemented
 * first.)  For example, both use a hash table of the same size where each
 * bucket is referred to as a "chain" that contains both a spin lock and
 * a linked list of queues.  An individual queue is located by using a hash
 * to pick a chain, locking the chain, and then walking the chain searching
 * for the queue.  This means that a wait channel object does not need to
 * embed it's queue head just as locks do not embed their turnstile queue
 * head.  Threads also carry around a sleep queue that they lend to the
 * wait channel when blocking.  Just as in turnstiles, the queue includes
 * a free list of the sleep queues of other threads blocked on the same
 * wait channel in the case of multiple waiters.
 *
 * Some additional functionality provided by sleep queues include the
 * ability to set a timeout.  The timeout is managed using a per-thread
 * callout that resumes a thread if it is asleep.  A thread may also
 * catch signals while it is asleep (aka an interruptible sleep).  The
 * signal code uses sleepq_abort() to interrupt a sleeping thread.  Finally,
 * sleep queues also provide some extra assertions.  One is not allowed to
 * mix the sleep/wakeup and cv APIs for a given wait channel.  Also, one
 * must consistently use the same lock to synchronize with a wait channel,
 * though this check is currently only a warning for sleep/wakeup due to
 * pre-existing abuse of that API.  The same lock must also be held when
 * awakening threads, though that is currently only enforced for condition
 * variables.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_sleepqueue_profiling.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/sysctl.h>

#include <vm/uma.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

/*
 * Constants for the hash table of sleep queue chains.  These constants are
 * the same ones that 4BSD (and possibly earlier versions of BSD) used.
 * Basically, we ignore the lower 8 bits of the address since most wait
 * channel pointers are aligned and only look at the next 7 bits for the
 * hash.  SC_TABLESIZE must be a power of two for SC_MASK to work properly.
 */
#define	SC_TABLESIZE	128			/* Must be power of 2. */
#define	SC_MASK		(SC_TABLESIZE - 1)
#define	SC_SHIFT	8
#define	SC_HASH(wc)	(((uintptr_t)(wc) >> SC_SHIFT) & SC_MASK)
#define	SC_LOOKUP(wc)	&sleepq_chains[SC_HASH(wc)]
#define NR_SLEEPQS      2
/*
 * There two different lists of sleep queues.  Both lists are connected
 * via the sq_hash entries.  The first list is the sleep queue chain list
 * that a sleep queue is on when it is attached to a wait channel.  The
 * second list is the free list hung off of a sleep queue that is attached
 * to a wait channel.
 *
 * Each sleep queue also contains the wait channel it is attached to, the
 * list of threads blocked on that wait channel, flags specific to the
 * wait channel, and the lock used to synchronize with a wait channel.
 * The flags are used to catch mismatches between the various consumers
 * of the sleep queue API (e.g. sleep/wakeup and condition variables).
 * The lock pointer is only used when invariants are enabled for various
 * debugging checks.
 *
 * Locking key:
 *  c - sleep queue chain lock
 */
struct sleepqueue {
	TAILQ_HEAD(, thread) sq_blocked[NR_SLEEPQS];	/* (c) Blocked threads. */
	LIST_ENTRY(sleepqueue) sq_hash;		/* (c) Chain and free list. */
	LIST_HEAD(, sleepqueue) sq_free;	/* (c) Free queues. */
	void	*sq_wchan;			/* (c) Wait channel. */
#ifdef INVARIANTS
	int	sq_type;			/* (c) Queue type. */
	struct mtx *sq_lock;		/* (c) Associated lock. */
#endif
};

struct sleepqueue_chain {
	LIST_HEAD(, sleepqueue) sc_queues;	/* List of sleep queues. */
	struct mtx sc_lock;			/* Spin lock for this chain. */
#ifdef SLEEPQUEUE_PROFILING
	u_int	sc_depth;			/* Length of sc_queues. */
	u_int	sc_max_depth;			/* Max length of sc_queues. */
#endif
};

#ifdef SLEEPQUEUE_PROFILING
u_int sleepq_max_depth;
SYSCTL_NODE(_debug, OID_AUTO, sleepq, CTLFLAG_RD, 0, "sleepq profiling");
SYSCTL_NODE(_debug_sleepq, OID_AUTO, chains, CTLFLAG_RD, 0,
    "sleepq chain stats");
SYSCTL_UINT(_debug_sleepq, OID_AUTO, max_depth, CTLFLAG_RD, &sleepq_max_depth,
    0, "maxmimum depth achieved of a single chain");
#endif
static struct sleepqueue_chain sleepq_chains[SC_TABLESIZE];
static uma_zone_t sleepq_zone;

/*
 * Prototypes for non-exported routines.
 */
static int	sleepq_catch_signals(void *wchan);
static int	sleepq_check_signals(void);
static int	sleepq_check_timeout(void);
#ifdef INVARIANTS
static void	sleepq_dtor(void *mem, int size, void *arg);
#endif
static int	sleepq_init(void *mem, int size, int flags);
static void	sleepq_resume_thread(struct sleepqueue *sq, struct thread *td,
		    int pri);
static void	sleepq_switch(void *wchan);
static void	sleepq_timeout(void *arg);

/*
 * Early initialization of sleep queues that is called from the sleepinit()
 * SYSINIT.
 */
void
init_sleepqueues(void)
{
#ifdef SLEEPQUEUE_PROFILING
	struct sysctl_oid *chain_oid;
	char chain_name[10];
#endif
	int i;

	for (i = 0; i < SC_TABLESIZE; i++) {
		LIST_INIT(&sleepq_chains[i].sc_queues);
		mtx_init(&sleepq_chains[i].sc_lock, "sleepq chain", NULL,
		    MTX_SPIN);
#ifdef SLEEPQUEUE_PROFILING
		snprintf(chain_name, sizeof(chain_name), "%d", i);
		chain_oid = SYSCTL_ADD_NODE(NULL, 
		    SYSCTL_STATIC_CHILDREN(_debug_sleepq_chains), OID_AUTO,
		    chain_name, CTLFLAG_RD, NULL, "sleepq chain stats");
		SYSCTL_ADD_UINT(NULL, SYSCTL_CHILDREN(chain_oid), OID_AUTO,
		    "depth", CTLFLAG_RD, &sleepq_chains[i].sc_depth, 0, NULL);
		SYSCTL_ADD_UINT(NULL, SYSCTL_CHILDREN(chain_oid), OID_AUTO,
		    "max_depth", CTLFLAG_RD, &sleepq_chains[i].sc_max_depth, 0,
		    NULL);
#endif
	}
	sleepq_zone = uma_zcreate("SLEEPQUEUE", sizeof(struct sleepqueue),
#ifdef INVARIANTS
	    NULL, sleepq_dtor, sleepq_init, NULL, UMA_ALIGN_CACHE, 0);
#else
	    NULL, NULL, sleepq_init, NULL, UMA_ALIGN_CACHE, 0);
#endif
	
	thread0.td_sleepqueue = sleepq_alloc();
}

/*
 * Get a sleep queue for a new thread.
 */
struct sleepqueue *
sleepq_alloc(void)
{

	return (uma_zalloc(sleepq_zone, M_WAITOK));
}

/*
 * Free a sleep queue when a thread is destroyed.
 */
void
sleepq_free(struct sleepqueue *sq)
{

	uma_zfree(sleepq_zone, sq);
}

/*
 * Lock the sleep queue chain associated with the specified wait channel.
 */
void
sleepq_lock(void *wchan)
{
	struct sleepqueue_chain *sc;

	sc = SC_LOOKUP(wchan);
	mtx_lock_spin(&sc->sc_lock);
}

/*
 * Look up the sleep queue associated with a given wait channel in the hash
 * table locking the associated sleep queue chain.  If no queue is found in
 * the table, NULL is returned.
 */
struct sleepqueue *
sleepq_lookup(void *wchan)
{
	struct sleepqueue_chain *sc;
	struct sleepqueue *sq;

	KASSERT(wchan != NULL, ("%s: invalid NULL wait channel", __func__));
	sc = SC_LOOKUP(wchan);
	mtx_assert(&sc->sc_lock, MA_OWNED);
	LIST_FOREACH(sq, &sc->sc_queues, sq_hash)
		if (sq->sq_wchan == wchan)
			return (sq);
	return (NULL);
}

/*
 * Unlock the sleep queue chain associated with a given wait channel.
 */
void
sleepq_release(void *wchan)
{
	struct sleepqueue_chain *sc;

	sc = SC_LOOKUP(wchan);
	mtx_unlock_spin(&sc->sc_lock);
}

/*
 * Places the current thread on the sleep queue for the specified wait
 * channel.  If INVARIANTS is enabled, then it associates the passed in
 * lock with the sleepq to make sure it is held when that sleep queue is
 * woken up.
 */
void
sleepq_add_queue(void *wchan, struct mtx *lock, const char *wmesg, int flags,
    int queue)
{
	struct sleepqueue_chain *sc;
	struct sleepqueue *sq;
	struct thread *td;

	td = curthread;
	sc = SC_LOOKUP(wchan);
	mtx_assert(&sc->sc_lock, MA_OWNED);
	MPASS(td->td_sleepqueue != NULL);
	MPASS(wchan != NULL);
	MPASS((queue >= 0) && (queue < NR_SLEEPQS));

	/* If this thread is not allowed to sleep, die a horrible death. */
	KASSERT(!(td->td_pflags & TDP_NOSLEEPING),
	    ("Trying sleep, but thread marked as sleeping prohibited"));

	/* Look up the sleep queue associated with the wait channel 'wchan'. */
	sq = sleepq_lookup(wchan);

	/*
	 * If the wait channel does not already have a sleep queue, use
	 * this thread's sleep queue.  Otherwise, insert the current thread
	 * into the sleep queue already in use by this wait channel.
	 */
	if (sq == NULL) {
#ifdef INVARIANTS
		int i;

		sq = td->td_sleepqueue;
		for (i = 0; i < NR_SLEEPQS; i++)
			KASSERT(TAILQ_EMPTY(&sq->sq_blocked[i]),
				("thread's sleep queue %d is not empty", i));
		KASSERT(LIST_EMPTY(&sq->sq_free),
		    ("thread's sleep queue has a non-empty free list"));
		KASSERT(sq->sq_wchan == NULL, ("stale sq_wchan pointer"));
		sq->sq_lock = lock;
		sq->sq_type = flags & SLEEPQ_TYPE;
#endif
#ifdef SLEEPQUEUE_PROFILING
		sc->sc_depth++;
		if (sc->sc_depth > sc->sc_max_depth) {
			sc->sc_max_depth = sc->sc_depth;
			if (sc->sc_max_depth > sleepq_max_depth)
				sleepq_max_depth = sc->sc_max_depth;
		}
#endif
		sq = td->td_sleepqueue;
		LIST_INSERT_HEAD(&sc->sc_queues, sq, sq_hash);
		sq->sq_wchan = wchan;
	} else {
		MPASS(wchan == sq->sq_wchan);
		MPASS(lock == sq->sq_lock);
		MPASS((flags & SLEEPQ_TYPE) == sq->sq_type);
		LIST_INSERT_HEAD(&sq->sq_free, td->td_sleepqueue, sq_hash);
	}
	TAILQ_INSERT_TAIL(&sq->sq_blocked[queue], td, td_slpq);
	td->td_sleepqueue = NULL;
	mtx_lock_spin(&sched_lock);
	td->td_sqqueue = queue;
	td->td_wchan = wchan;
	td->td_wmesg = wmesg;
	if (flags & SLEEPQ_INTERRUPTIBLE) {
		td->td_flags |= TDF_SINTR;
		td->td_flags &= ~TDF_SLEEPABORT;
	}
	mtx_unlock_spin(&sched_lock);
}

/*
 * Sets a timeout that will remove the current thread from the specified
 * sleep queue after timo ticks if the thread has not already been awakened.
 */
void
sleepq_set_timeout(void *wchan, int timo)
{
	struct sleepqueue_chain *sc;
	struct thread *td;

	td = curthread;
	sc = SC_LOOKUP(wchan);
	mtx_assert(&sc->sc_lock, MA_OWNED);
	MPASS(TD_ON_SLEEPQ(td));
	MPASS(td->td_sleepqueue == NULL);
	MPASS(wchan != NULL);
	callout_reset(&td->td_slpcallout, timo, sleepq_timeout, td);
}

/*
 * Marks the pending sleep of the current thread as interruptible and
 * makes an initial check for pending signals before putting a thread
 * to sleep. Return with sleep queue and scheduler lock held.
 */
static int
sleepq_catch_signals(void *wchan)
{
	struct sleepqueue_chain *sc;
	struct sleepqueue *sq;
	struct thread *td;
	struct proc *p;
	struct sigacts *ps;
	int sig, ret;

	td = curthread;
	p = curproc;
	sc = SC_LOOKUP(wchan);
	mtx_assert(&sc->sc_lock, MA_OWNED);
	MPASS(wchan != NULL);
	CTR3(KTR_PROC, "sleepq catching signals: thread %p (pid %ld, %s)",
		(void *)td, (long)p->p_pid, p->p_comm);

	MPASS(td->td_flags & TDF_SINTR);
	mtx_unlock_spin(&sc->sc_lock);

	/* See if there are any pending signals for this thread. */
	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	sig = cursig(td);
	if (sig == 0) {
		mtx_unlock(&ps->ps_mtx);
		ret = thread_suspend_check(1);
		MPASS(ret == 0 || ret == EINTR || ret == ERESTART);
	} else {
		if (SIGISMEMBER(ps->ps_sigintr, sig))
			ret = EINTR;
		else
			ret = ERESTART;
		mtx_unlock(&ps->ps_mtx);
	}

	if (ret == 0) {
		mtx_lock_spin(&sc->sc_lock);
		/*
		 * Lock sched_lock before unlocking proc lock,
		 * without this, we could lose a race.
		 */
		mtx_lock_spin(&sched_lock);
		PROC_UNLOCK(p);
		if (!(td->td_flags & TDF_INTERRUPT))
			return (0);
		/* KSE threads tried unblocking us. */
		ret = td->td_intrval;
		mtx_unlock_spin(&sched_lock);
		MPASS(ret == EINTR || ret == ERESTART);
	} else {
		PROC_UNLOCK(p);
		mtx_lock_spin(&sc->sc_lock);
	}
	/*
	 * There were pending signals and this thread is still
	 * on the sleep queue, remove it from the sleep queue.
	 */
	sq = sleepq_lookup(wchan);
	mtx_lock_spin(&sched_lock);
	if (TD_ON_SLEEPQ(td))
		sleepq_resume_thread(sq, td, -1);
	return (ret);
}

/*
 * Switches to another thread if we are still asleep on a sleep queue and
 * drop the lock on the sleep queue chain.  Returns with sched_lock held.
 */
static void
sleepq_switch(void *wchan)
{
	struct sleepqueue_chain *sc;
	struct thread *td;

	td = curthread;
	sc = SC_LOOKUP(wchan);
	mtx_assert(&sc->sc_lock, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);

	/* 
	 * If we have a sleep queue, then we've already been woken up, so
	 * just return.
	 */
	if (td->td_sleepqueue != NULL) {
		MPASS(!TD_ON_SLEEPQ(td));
		mtx_unlock_spin(&sc->sc_lock);
		return;
	}

	/*
	 * Otherwise, actually go to sleep.
	 */
	mtx_unlock_spin(&sc->sc_lock);
	sched_sleep(td);
	TD_SET_SLEEPING(td);
	mi_switch(SW_VOL, NULL);
	KASSERT(TD_IS_RUNNING(td), ("running but not TDS_RUNNING"));
	CTR3(KTR_PROC, "sleepq resume: thread %p (pid %ld, %s)",
	    (void *)td, (long)td->td_proc->p_pid, (void *)td->td_proc->p_comm);
}

/*
 * Check to see if we timed out.
 */
static int
sleepq_check_timeout(void)
{
	struct thread *td;

	mtx_assert(&sched_lock, MA_OWNED);
	td = curthread;

	/*
	 * If TDF_TIMEOUT is set, we timed out.
	 */
	if (td->td_flags & TDF_TIMEOUT) {
		td->td_flags &= ~TDF_TIMEOUT;
		return (EWOULDBLOCK);
	}

	/*
	 * If TDF_TIMOFAIL is set, the timeout ran after we had
	 * already been woken up.
	 */
	if (td->td_flags & TDF_TIMOFAIL)
		td->td_flags &= ~TDF_TIMOFAIL;

	/*
	 * If callout_stop() fails, then the timeout is running on
	 * another CPU, so synchronize with it to avoid having it
	 * accidentally wake up a subsequent sleep.
	 */
	else if (callout_stop(&td->td_slpcallout) == 0) {
		td->td_flags |= TDF_TIMEOUT;
		TD_SET_SLEEPING(td);
		mi_switch(SW_INVOL, NULL);
	}
	return (0);
}

/*
 * Check to see if we were awoken by a signal.
 */
static int
sleepq_check_signals(void)
{
	struct thread *td;

	mtx_assert(&sched_lock, MA_OWNED);
	td = curthread;

	/* We are no longer in an interruptible sleep. */
	if (td->td_flags & TDF_SINTR)
		td->td_flags &= ~TDF_SINTR;

	if (td->td_flags & TDF_SLEEPABORT) {
		td->td_flags &= ~TDF_SLEEPABORT;
		return (td->td_intrval);
	}

	if (td->td_flags & TDF_INTERRUPT)
		return (td->td_intrval);

	return (0);
}

/*
 * Block the current thread until it is awakened from its sleep queue.
 */
void
sleepq_wait(void *wchan)
{

	MPASS(!(curthread->td_flags & TDF_SINTR));
	mtx_lock_spin(&sched_lock);
	sleepq_switch(wchan);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Block the current thread until it is awakened from its sleep queue
 * or it is interrupted by a signal.
 */
int
sleepq_wait_sig(void *wchan)
{
	int rcatch;
	int rval;

	rcatch = sleepq_catch_signals(wchan);
	if (rcatch == 0)
		sleepq_switch(wchan);
	else
		sleepq_release(wchan);
	rval = sleepq_check_signals();
	mtx_unlock_spin(&sched_lock); 
	if (rcatch)
		return (rcatch);
	return (rval);
}

/*
 * Block the current thread until it is awakened from its sleep queue
 * or it times out while waiting.
 */
int
sleepq_timedwait(void *wchan)
{
	int rval;

	MPASS(!(curthread->td_flags & TDF_SINTR));
	mtx_lock_spin(&sched_lock);
	sleepq_switch(wchan);
	rval = sleepq_check_timeout();
	mtx_unlock_spin(&sched_lock);
	return (rval);
}

/*
 * Block the current thread until it is awakened from its sleep queue,
 * it is interrupted by a signal, or it times out waiting to be awakened.
 */
int
sleepq_timedwait_sig(void *wchan)
{
	int rcatch, rvalt, rvals;

	rcatch = sleepq_catch_signals(wchan);
	if (rcatch == 0)
		sleepq_switch(wchan);
	else
		sleepq_release(wchan);
	rvalt = sleepq_check_timeout();
	rvals = sleepq_check_signals();
	mtx_unlock_spin(&sched_lock);
	if (rcatch)
		return (rcatch);
	if (rvals)
		return (rvals);
	return (rvalt);
}

/*
 * Removes a thread from a sleep queue and makes it
 * runnable.
 */
static void
sleepq_resume_thread(struct sleepqueue *sq, struct thread *td, int pri)
{
	struct sleepqueue_chain *sc;

	MPASS(td != NULL);
	MPASS(sq->sq_wchan != NULL);
	MPASS(td->td_wchan == sq->sq_wchan);
	MPASS(td->td_sqqueue < NR_SLEEPQS && td->td_sqqueue >= 0);
	sc = SC_LOOKUP(sq->sq_wchan);
	mtx_assert(&sc->sc_lock, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);

	/* Remove the thread from the queue. */
	TAILQ_REMOVE(&sq->sq_blocked[(int)td->td_sqqueue], td, td_slpq);

	/*
	 * Get a sleep queue for this thread.  If this is the last waiter,
	 * use the queue itself and take it out of the chain, otherwise,
	 * remove a queue from the free list.
	 */
	if (LIST_EMPTY(&sq->sq_free)) {
		td->td_sleepqueue = sq;
#ifdef INVARIANTS
		sq->sq_wchan = NULL;
#endif
#ifdef SLEEPQUEUE_PROFILING
		sc->sc_depth--;
#endif
	} else
		td->td_sleepqueue = LIST_FIRST(&sq->sq_free);
	LIST_REMOVE(td->td_sleepqueue, sq_hash);

	td->td_wmesg = NULL;
	td->td_wchan = NULL;
	td->td_flags &= ~TDF_SINTR;

	/*
	 * Note that thread td might not be sleeping if it is running
	 * sleepq_catch_signals() on another CPU or is blocked on
	 * its proc lock to check signals.  It doesn't hurt to clear
	 * the sleeping flag if it isn't set though, so we just always
	 * do it.  However, we can't assert that it is set.
	 */
	CTR3(KTR_PROC, "sleepq_wakeup: thread %p (pid %ld, %s)",
	    (void *)td, (long)td->td_proc->p_pid, td->td_proc->p_comm);
	TD_CLR_SLEEPING(td);

	/* Adjust priority if requested. */
	MPASS(pri == -1 || (pri >= PRI_MIN && pri <= PRI_MAX));
	if (pri != -1 && td->td_priority > pri)
		sched_prio(td, pri);
	setrunnable(td);
}

#ifdef INVARIANTS
/*
 * UMA zone item deallocator.
 */
static void
sleepq_dtor(void *mem, int size, void *arg)
{
	struct sleepqueue *sq;
	int i;

	sq = mem;
	for (i = 0; i < NR_SLEEPQS; i++)
		MPASS(TAILQ_EMPTY(&sq->sq_blocked[i]));
}
#endif

/*
 * UMA zone item initializer.
 */
static int
sleepq_init(void *mem, int size, int flags)
{
	struct sleepqueue *sq;
	int i;

	bzero(mem, size);
	sq = mem;
	for (i = 0; i < NR_SLEEPQS; i++)
		TAILQ_INIT(&sq->sq_blocked[i]);
	LIST_INIT(&sq->sq_free);
	return (0);
}

/*
 * Find the highest priority thread sleeping on a wait channel and resume it.
 */
void
sleepq_signal_queue(void *wchan, int flags, int pri, int queue)
{
	struct sleepqueue *sq;
	struct thread *td, *besttd;

	CTR2(KTR_PROC, "sleepq_signal(%p, %d)", wchan, flags);
	KASSERT(wchan != NULL, ("%s: invalid NULL wait channel", __func__));
	MPASS((queue >= 0) && (queue < NR_SLEEPQS));
	sq = sleepq_lookup(wchan);
	if (sq == NULL) {
		sleepq_release(wchan);
		return;
	}
	KASSERT(sq->sq_type == (flags & SLEEPQ_TYPE),
	    ("%s: mismatch between sleep/wakeup and cv_*", __func__));

	/*
	 * Find the highest priority thread on the queue.  If there is a
	 * tie, use the thread that first appears in the queue as it has
	 * been sleeping the longest since threads are always added to
	 * the tail of sleep queues.
	 */
	besttd = NULL;
	TAILQ_FOREACH(td, &sq->sq_blocked[queue], td_slpq) {
		if (besttd == NULL || td->td_priority < besttd->td_priority)
			besttd = td;
	}
	MPASS(besttd != NULL);
	mtx_lock_spin(&sched_lock);
	sleepq_resume_thread(sq, besttd, pri);
	mtx_unlock_spin(&sched_lock);
	sleepq_release(wchan);
}

/*
 * Resume all threads sleeping on a specified wait channel.
 */
void
sleepq_broadcast_queue(void *wchan, int flags, int pri, int queue)
{
	struct sleepqueue *sq;

	CTR2(KTR_PROC, "sleepq_broadcast(%p, %d)", wchan, flags);
	KASSERT(wchan != NULL, ("%s: invalid NULL wait channel", __func__));
	MPASS((queue >= 0) && (queue < NR_SLEEPQS));
	sq = sleepq_lookup(wchan);
	if (sq == NULL) {
		sleepq_release(wchan);
		return;
	}
	KASSERT(sq->sq_type == (flags & SLEEPQ_TYPE),
	    ("%s: mismatch between sleep/wakeup and cv_*", __func__));

	/* Resume all blocked threads on the sleep queue. */
	mtx_lock_spin(&sched_lock);
	while (!TAILQ_EMPTY(&sq->sq_blocked[queue]))
		sleepq_resume_thread(sq, TAILQ_FIRST(&sq->sq_blocked[queue]),
		    pri);
	mtx_unlock_spin(&sched_lock);
	sleepq_release(wchan);
}

/*
 * Time sleeping threads out.  When the timeout expires, the thread is
 * removed from the sleep queue and made runnable if it is still asleep.
 */
static void
sleepq_timeout(void *arg)
{
	struct sleepqueue *sq;
	struct thread *td;
	void *wchan;

	td = arg;
	CTR3(KTR_PROC, "sleepq_timeout: thread %p (pid %ld, %s)",
	    (void *)td, (long)td->td_proc->p_pid, (void *)td->td_proc->p_comm);

	/*
	 * First, see if the thread is asleep and get the wait channel if
	 * it is.
	 */
	mtx_lock_spin(&sched_lock);
	if (TD_ON_SLEEPQ(td)) {
		wchan = td->td_wchan;
		mtx_unlock_spin(&sched_lock);
		sleepq_lock(wchan);
		sq = sleepq_lookup(wchan);
		mtx_lock_spin(&sched_lock);
	} else {
		wchan = NULL;
		sq = NULL;
	}

	/*
	 * At this point, if the thread is still on the sleep queue,
	 * we have that sleep queue locked as it cannot migrate sleep
	 * queues while we dropped sched_lock.  If it had resumed and
	 * was on another CPU while the lock was dropped, it would have
	 * seen that TDF_TIMEOUT and TDF_TIMOFAIL are clear and the
	 * call to callout_stop() to stop this routine would have failed
	 * meaning that it would have already set TDF_TIMEOUT to
	 * synchronize with this function.
	 */
	if (TD_ON_SLEEPQ(td)) {
		MPASS(td->td_wchan == wchan);
		MPASS(sq != NULL);
		td->td_flags |= TDF_TIMEOUT;
		sleepq_resume_thread(sq, td, -1);
		mtx_unlock_spin(&sched_lock);
		sleepq_release(wchan);
		return;
	} else if (wchan != NULL)
		sleepq_release(wchan);

	/*
	 * Now check for the edge cases.  First, if TDF_TIMEOUT is set,
	 * then the other thread has already yielded to us, so clear
	 * the flag and resume it.  If TDF_TIMEOUT is not set, then the
	 * we know that the other thread is not on a sleep queue, but it
	 * hasn't resumed execution yet.  In that case, set TDF_TIMOFAIL
	 * to let it know that the timeout has already run and doesn't
	 * need to be canceled.
	 */
	if (td->td_flags & TDF_TIMEOUT) {
		MPASS(TD_IS_SLEEPING(td));
		td->td_flags &= ~TDF_TIMEOUT;
		TD_CLR_SLEEPING(td);
		setrunnable(td);
	} else
		td->td_flags |= TDF_TIMOFAIL;
	mtx_unlock_spin(&sched_lock);
}

/*
 * Resumes a specific thread from the sleep queue associated with a specific
 * wait channel if it is on that queue.
 */
void
sleepq_remove(struct thread *td, void *wchan)
{
	struct sleepqueue *sq;

	/*
	 * Look up the sleep queue for this wait channel, then re-check
	 * that the thread is asleep on that channel, if it is not, then
	 * bail.
	 */
	MPASS(wchan != NULL);
	sleepq_lock(wchan);
	sq = sleepq_lookup(wchan);
	mtx_lock_spin(&sched_lock);
	if (!TD_ON_SLEEPQ(td) || td->td_wchan != wchan) {
		mtx_unlock_spin(&sched_lock);
		sleepq_release(wchan);
		return;
	}
	MPASS(sq != NULL);

	/* Thread is asleep on sleep queue sq, so wake it up. */
	sleepq_resume_thread(sq, td, -1);
	sleepq_release(wchan);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Abort a thread as if an interrupt had occurred.  Only abort
 * interruptible waits (unfortunately it isn't safe to abort others).
 *
 * XXX: What in the world does the comment below mean?
 * Also, whatever the signal code does...
 */
void
sleepq_abort(struct thread *td, int intrval)
{
	void *wchan;

	mtx_assert(&sched_lock, MA_OWNED);
	MPASS(TD_ON_SLEEPQ(td));
	MPASS(td->td_flags & TDF_SINTR);
	MPASS(intrval == EINTR || intrval == ERESTART);

	/*
	 * If the TDF_TIMEOUT flag is set, just leave. A
	 * timeout is scheduled anyhow.
	 */
	if (td->td_flags & TDF_TIMEOUT)
		return;

	CTR3(KTR_PROC, "sleepq_abort: thread %p (pid %ld, %s)",
	    (void *)td, (long)td->td_proc->p_pid, (void *)td->td_proc->p_comm);
	wchan = td->td_wchan;
	if (wchan != NULL) {
		td->td_intrval = intrval;
		td->td_flags |= TDF_SLEEPABORT;
	}
	mtx_unlock_spin(&sched_lock);
	sleepq_remove(td, wchan);
	mtx_lock_spin(&sched_lock);
}

#ifdef DDB
DB_SHOW_COMMAND(sleepq, db_show_sleepqueue)
{
	struct sleepqueue_chain *sc;
	struct sleepqueue *sq;
#ifdef INVARIANTS
	struct lock_object *lock;
#endif
	struct thread *td;
	void *wchan;
	int i;

	if (!have_addr)
		return;

	/*
	 * First, see if there is an active sleep queue for the wait channel
	 * indicated by the address.
	 */
	wchan = (void *)addr;
	sc = SC_LOOKUP(wchan);
	LIST_FOREACH(sq, &sc->sc_queues, sq_hash)
		if (sq->sq_wchan == wchan)
			goto found;

	/*
	 * Second, see if there is an active sleep queue at the address
	 * indicated.
	 */
	for (i = 0; i < SC_TABLESIZE; i++)
		LIST_FOREACH(sq, &sleepq_chains[i].sc_queues, sq_hash) {
			if (sq == (struct sleepqueue *)addr)
				goto found;
		}

	db_printf("Unable to locate a sleep queue via %p\n", (void *)addr);
	return;
found:
	db_printf("Wait channel: %p\n", sq->sq_wchan);
#ifdef INVARIANTS
	db_printf("Queue type: %d\n", sq->sq_type);
	if (sq->sq_lock) {
		lock = &sq->sq_lock->mtx_object;
		db_printf("Associated Interlock: %p - (%s) %s\n", lock,
		    LOCK_CLASS(lock)->lc_name, lock->lo_name);
	}
#endif
	db_printf("Blocked threads:\n");
	for (i = 0; i < NR_SLEEPQS; i++) {
		db_printf("\nQueue[%d]:\n", i);
		if (TAILQ_EMPTY(&sq->sq_blocked[i]))
			db_printf("\tempty\n");
		else
			TAILQ_FOREACH(td, &sq->sq_blocked[0],
				      td_slpq) {
				db_printf("\t%p (tid %d, pid %d, \"%s\")\n", td,
					  td->td_tid, td->td_proc->p_pid,
					  td->td_proc->p_comm);
			}
	}
}
#endif
