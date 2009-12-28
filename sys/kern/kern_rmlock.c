/*-
 * Copyright (c) 2007 Stephan Uphoff <ups@FreeBSD.org>
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
 * Machine independent bits of reader/writer lock implementation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rmlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/turnstile.h>
#include <sys/lock_profile.h>
#include <machine/cpu.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#define RMPF_ONQUEUE	1
#define RMPF_SIGNAL	2

/*
 * To support usage of rmlock in CVs and msleep yet another list for the
 * priority tracker would be needed.  Using this lock for cv and msleep also
 * does not seem very useful
 */

static __inline void compiler_memory_barrier(void) {
	__asm __volatile("":::"memory");
}

static void	assert_rm(struct lock_object *lock, int what);
static void	lock_rm(struct lock_object *lock, int how);
#ifdef KDTRACE_HOOKS
static int	owner_rm(struct lock_object *lock, struct thread **owner);
#endif
static int	unlock_rm(struct lock_object *lock);

struct lock_class lock_class_rm = {
	.lc_name = "rm",
	.lc_flags = LC_SLEEPLOCK | LC_RECURSABLE,
	.lc_assert = assert_rm,
#if 0
#ifdef DDB
	.lc_ddb_show = db_show_rwlock,
#endif
#endif
	.lc_lock = lock_rm,
	.lc_unlock = unlock_rm,
#ifdef KDTRACE_HOOKS
	.lc_owner = owner_rm,
#endif
};

static void
assert_rm(struct lock_object *lock, int what)
{

	panic("assert_rm called");
}

static void
lock_rm(struct lock_object *lock, int how)
{

	panic("lock_rm called");
}

static int
unlock_rm(struct lock_object *lock)
{

	panic("unlock_rm called");
}

#ifdef KDTRACE_HOOKS
static int
owner_rm(struct lock_object *lock, struct thread **owner)
{

	panic("owner_rm called");
}
#endif

static struct mtx rm_spinlock;

MTX_SYSINIT(rm_spinlock, &rm_spinlock, "rm_spinlock", MTX_SPIN);

/*
 * Add or remove tracker from per cpu list.
 *
 * The per cpu list can be traversed at any time in forward direction from an
 * interrupt on the *local* cpu.
 */
static void inline
rm_tracker_add(struct pcpu *pc, struct rm_priotracker *tracker)
{
	struct rm_queue *next;

	/* Initialize all tracker pointers */
	tracker->rmp_cpuQueue.rmq_prev = &pc->pc_rm_queue;
	next = pc->pc_rm_queue.rmq_next;
	tracker->rmp_cpuQueue.rmq_next = next;

	/* rmq_prev is not used during froward traversal. */
	next->rmq_prev = &tracker->rmp_cpuQueue;

	/* Update pointer to first element. */
	pc->pc_rm_queue.rmq_next = &tracker->rmp_cpuQueue;
}

static void inline
rm_tracker_remove(struct pcpu *pc, struct rm_priotracker *tracker)
{
	struct rm_queue *next, *prev;

	next = tracker->rmp_cpuQueue.rmq_next;
	prev = tracker->rmp_cpuQueue.rmq_prev;

	/* Not used during forward traversal. */
	next->rmq_prev = prev;

	/* Remove from list. */
	prev->rmq_next = next;
}

static void
rm_cleanIPI(void *arg)
{
	struct pcpu *pc;
	struct rmlock *rm = arg;
	struct rm_priotracker *tracker;
	struct rm_queue *queue;
	pc = pcpu_find(curcpu);

	for (queue = pc->pc_rm_queue.rmq_next; queue != &pc->pc_rm_queue;
	    queue = queue->rmq_next) {
		tracker = (struct rm_priotracker *)queue;
		if (tracker->rmp_rmlock == rm && tracker->rmp_flags == 0) {
			tracker->rmp_flags = RMPF_ONQUEUE;
			mtx_lock_spin(&rm_spinlock);
			LIST_INSERT_HEAD(&rm->rm_activeReaders, tracker,
			    rmp_qentry);
			mtx_unlock_spin(&rm_spinlock);
		}
	}
}

void
rm_init_flags(struct rmlock *rm, const char *name, int opts)
{
	int liflags;

	liflags = 0;
	if (!(opts & RM_NOWITNESS))
		liflags |= LO_WITNESS;
	if (opts & RM_RECURSE)
		liflags |= LO_RECURSABLE;
	rm->rm_noreadtoken = 1;
	LIST_INIT(&rm->rm_activeReaders);
	mtx_init(&rm->rm_lock, name, "rmlock_mtx", MTX_NOWITNESS);
	lock_init(&rm->lock_object, &lock_class_rm, name, NULL, liflags);
}

void
rm_init(struct rmlock *rm, const char *name)
{

	rm_init_flags(rm, name, 0);
}

void
rm_destroy(struct rmlock *rm)
{

	mtx_destroy(&rm->rm_lock);
	lock_destroy(&rm->lock_object);
}

int
rm_wowned(struct rmlock *rm)
{

	return (mtx_owned(&rm->rm_lock));
}

void
rm_sysinit(void *arg)
{
	struct rm_args *args = arg;

	rm_init(args->ra_rm, args->ra_desc);
}

void
rm_sysinit_flags(void *arg)
{
	struct rm_args_flags *args = arg;

	rm_init_flags(args->ra_rm, args->ra_desc, args->ra_opts);
}

static void
_rm_rlock_hard(struct rmlock *rm, struct rm_priotracker *tracker)
{
	struct pcpu *pc;
	struct rm_queue *queue;
	struct rm_priotracker *atracker;

	critical_enter();
	pc = pcpu_find(curcpu);

	/* Check if we just need to do a proper critical_exit. */
	if (0 == rm->rm_noreadtoken) {
		critical_exit();
		return;
	}

	/* Remove our tracker from the per cpu list. */
	rm_tracker_remove(pc, tracker);

	/* Check to see if the IPI granted us the lock after all. */
	if (tracker->rmp_flags) {
		/* Just add back tracker - we hold the lock. */
		rm_tracker_add(pc, tracker);
		critical_exit();
		return;
	}

	/*
	 * We allow readers to aquire a lock even if a writer is blocked if
	 * the lock is recursive and the reader already holds the lock.
	 */
	if ((rm->lock_object.lo_flags & LO_RECURSABLE) != 0) {
		/*
		 * Just grand the lock if this thread already have a tracker
		 * for this lock on the per cpu queue.
		 */
		for (queue = pc->pc_rm_queue.rmq_next;
		    queue != &pc->pc_rm_queue; queue = queue->rmq_next) {
			atracker = (struct rm_priotracker *)queue;
			if ((atracker->rmp_rmlock == rm) &&
			    (atracker->rmp_thread == tracker->rmp_thread)) {
				mtx_lock_spin(&rm_spinlock);
				LIST_INSERT_HEAD(&rm->rm_activeReaders,
				    tracker, rmp_qentry);
				tracker->rmp_flags = RMPF_ONQUEUE;
				mtx_unlock_spin(&rm_spinlock);
				rm_tracker_add(pc, tracker);
				critical_exit();
				return;
			}
		}
	}

	sched_unpin();
	critical_exit();

	mtx_lock(&rm->rm_lock);
	rm->rm_noreadtoken = 0;
	critical_enter();

	pc = pcpu_find(curcpu);
	rm_tracker_add(pc, tracker);
	sched_pin();
	critical_exit();

	mtx_unlock(&rm->rm_lock);
}

void
_rm_rlock(struct rmlock *rm, struct rm_priotracker *tracker)
{
	struct thread *td = curthread;
	struct pcpu *pc;

	tracker->rmp_flags  = 0;
	tracker->rmp_thread = td;
	tracker->rmp_rmlock = rm;

	td->td_critnest++;	/* critical_enter(); */

	compiler_memory_barrier();

	pc = cpuid_to_pcpu[td->td_oncpu]; /* pcpu_find(td->td_oncpu); */

	rm_tracker_add(pc, tracker);

	sched_pin();

	compiler_memory_barrier();

	td->td_critnest--;

	/*
	 * Fast path to combine two common conditions into a single
	 * conditional jump.
	 */
	if (0 == (td->td_owepreempt | rm->rm_noreadtoken))
		return;

	/* We do not have a read token and need to acquire one. */
	_rm_rlock_hard(rm, tracker);
}

static void
_rm_unlock_hard(struct thread *td,struct rm_priotracker *tracker)
{

	if (td->td_owepreempt) {
		td->td_critnest++;
		critical_exit();
	}

	if (!tracker->rmp_flags)
		return;

	mtx_lock_spin(&rm_spinlock);
	LIST_REMOVE(tracker, rmp_qentry);

	if (tracker->rmp_flags & RMPF_SIGNAL) {
		struct rmlock *rm;
		struct turnstile *ts;

		rm = tracker->rmp_rmlock;

		turnstile_chain_lock(&rm->lock_object);
		mtx_unlock_spin(&rm_spinlock);

		ts = turnstile_lookup(&rm->lock_object);

		turnstile_signal(ts, TS_EXCLUSIVE_QUEUE);
		turnstile_unpend(ts, TS_EXCLUSIVE_LOCK);
		turnstile_chain_unlock(&rm->lock_object);
	} else
		mtx_unlock_spin(&rm_spinlock);
}

void
_rm_runlock(struct rmlock *rm, struct rm_priotracker *tracker)
{
	struct pcpu *pc;
	struct thread *td = tracker->rmp_thread;

	td->td_critnest++;	/* critical_enter(); */
	pc = cpuid_to_pcpu[td->td_oncpu]; /* pcpu_find(td->td_oncpu); */
	rm_tracker_remove(pc, tracker);
	td->td_critnest--;
	sched_unpin();

	if (0 == (td->td_owepreempt | tracker->rmp_flags))
		return;

	_rm_unlock_hard(td, tracker);
}

void
_rm_wlock(struct rmlock *rm)
{
	struct rm_priotracker *prio;
	struct turnstile *ts;

	mtx_lock(&rm->rm_lock);

	if (rm->rm_noreadtoken == 0) {
		/* Get all read tokens back */

		rm->rm_noreadtoken = 1;

		/*
		 * Assumes rm->rm_noreadtoken update is visible on other CPUs
		 * before rm_cleanIPI is called.
		 */
#ifdef SMP
		smp_rendezvous(smp_no_rendevous_barrier,
		    rm_cleanIPI,
		    smp_no_rendevous_barrier,
		    rm);

#else
		rm_cleanIPI(rm);
#endif

		mtx_lock_spin(&rm_spinlock);
		while ((prio = LIST_FIRST(&rm->rm_activeReaders)) != NULL) {
			ts = turnstile_trywait(&rm->lock_object);
			prio->rmp_flags = RMPF_ONQUEUE | RMPF_SIGNAL;
			mtx_unlock_spin(&rm_spinlock);
			turnstile_wait(ts, prio->rmp_thread,
			    TS_EXCLUSIVE_QUEUE);
			mtx_lock_spin(&rm_spinlock);
		}
		mtx_unlock_spin(&rm_spinlock);
	}
}

void
_rm_wunlock(struct rmlock *rm)
{

	mtx_unlock(&rm->rm_lock);
}

#ifdef LOCK_DEBUG

void _rm_wlock_debug(struct rmlock *rm, const char *file, int line)
{

	WITNESS_CHECKORDER(&rm->lock_object, LOP_NEWORDER | LOP_EXCLUSIVE,
	    file, line, NULL);

	_rm_wlock(rm);

	LOCK_LOG_LOCK("RMWLOCK", &rm->lock_object, 0, 0, file, line);

	WITNESS_LOCK(&rm->lock_object, LOP_EXCLUSIVE, file, line);

	curthread->td_locks++;

}

void
_rm_wunlock_debug(struct rmlock *rm, const char *file, int line)
{

	curthread->td_locks--;
	WITNESS_UNLOCK(&rm->lock_object, LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("RMWUNLOCK", &rm->lock_object, 0, 0, file, line);
	_rm_wunlock(rm);
}

void
_rm_rlock_debug(struct rmlock *rm, struct rm_priotracker *tracker,
    const char *file, int line)
{

	WITNESS_CHECKORDER(&rm->lock_object, LOP_NEWORDER, file, line, NULL);

	_rm_rlock(rm, tracker);

	LOCK_LOG_LOCK("RMRLOCK", &rm->lock_object, 0, 0, file, line);

	WITNESS_LOCK(&rm->lock_object, 0, file, line);

	curthread->td_locks++;
}

void
_rm_runlock_debug(struct rmlock *rm, struct rm_priotracker *tracker,
    const char *file, int line)
{

	curthread->td_locks--;
	WITNESS_UNLOCK(&rm->lock_object, 0, file, line);
	LOCK_LOG_LOCK("RMRUNLOCK", &rm->lock_object, 0, 0, file, line);
	_rm_runlock(rm, tracker);
}

#else

/*
 * Just strip out file and line arguments if no lock debugging is enabled in
 * the kernel - we are called from a kernel module.
 */
void
_rm_wlock_debug(struct rmlock *rm, const char *file, int line)
{

	_rm_wlock(rm);
}

void
_rm_wunlock_debug(struct rmlock *rm, const char *file, int line)
{

	_rm_wunlock(rm);
}

void
_rm_rlock_debug(struct rmlock *rm, struct rm_priotracker *tracker,
    const char *file, int line)
{

	_rm_rlock(rm, tracker);
}

void
_rm_runlock_debug(struct rmlock *rm, struct rm_priotracker *tracker,
    const char *file, int line)
{

	_rm_runlock(rm, tracker);
}

#endif
