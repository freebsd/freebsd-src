/*-
 * Copyright (c) 1998 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: mutex_witness.c,v 1.1.2.20 2000/04/27 03:10:27 cp Exp $
 *	and BSDI $Id: synch_machdep.c,v 2.3.2.39 2000/04/27 03:10:25 cp Exp $
 * $FreeBSD$
 */

/*
 * Machine independent bits of mutex implementation and implementation of
 * `witness' structure & related debugging routines.
 */

/*
 *	Main Entry: witness
 *	Pronunciation: 'wit-n&s
 *	Function: noun
 *	Etymology: Middle English witnesse, from Old English witnes knowledge,
 *	    testimony, witness, from 2wit
 *	Date: before 12th century
 *	1 : attestation of a fact or event : TESTIMONY
 *	2 : one that gives evidence; specifically : one who testifies in
 *	    a cause or before a judicial tribunal
 *	3 : one asked to be present at a transaction so as to be able to
 *	    testify to its having taken place
 *	4 : one who has personal knowledge of something
 *	5 a : something serving as evidence or proof : SIGN
 *	  b : public affirmation by word or example of usually
 *	      religious faith or conviction <the heroic witness to divine
 *	      life -- Pilot>
 *	6 capitalized : a member of the Jehovah's Witnesses 
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>
#include <sys/ktr.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

/*
 * Internal utility macros.
 */
#define mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)

#define mtx_owner(m)	(mtx_unowned((m)) ? NULL \
	: (struct proc *)((m)->mtx_lock & MTX_FLAGMASK))

#define SET_PRIO(p, pri)	(p)->p_pri.pri_level = (pri)

/*
 * Lock classes for sleep and spin mutexes.
 */
struct lock_class lock_class_mtx_sleep = {
	"sleep mutex",
	LC_SLEEPLOCK | LC_RECURSABLE
};
struct lock_class lock_class_mtx_spin = {
	"spin mutex",
	LC_SPINLOCK | LC_RECURSABLE
};

/*
 * Prototypes for non-exported routines.
 */
static void	propagate_priority(struct proc *);

static void
propagate_priority(struct proc *p)
{
	int pri = p->p_pri.pri_level;
	struct mtx *m = p->p_blocked;

	mtx_assert(&sched_lock, MA_OWNED);
	for (;;) {
		struct proc *p1;

		p = mtx_owner(m);

		if (p == NULL) {
			/*
			 * This really isn't quite right. Really
			 * ought to bump priority of process that
			 * next acquires the mutex.
			 */
			MPASS(m->mtx_lock == MTX_CONTESTED);
			return;
		}

		MPASS(p->p_magic == P_MAGIC);
		KASSERT(p->p_stat != SSLEEP, ("sleeping process owns a mutex"));
		if (p->p_pri.pri_level <= pri)
			return;

		/*
		 * Bump this process' priority.
		 */
		SET_PRIO(p, pri);

		/*
		 * If lock holder is actually running, just bump priority.
		 */
		if (p->p_oncpu != NOCPU) {
			MPASS(p->p_stat == SRUN || p->p_stat == SZOMB || p->p_stat == SSTOP);
			return;
		}

#ifndef SMP
		/*
		 * For UP, we check to see if p is curproc (this shouldn't
		 * ever happen however as it would mean we are in a deadlock.)
		 */
		KASSERT(p != curproc, ("Deadlock detected"));
#endif

		/*
		 * If on run queue move to new run queue, and
		 * quit.
		 */
		if (p->p_stat == SRUN) {
			MPASS(p->p_blocked == NULL);
			remrunqueue(p);
			setrunqueue(p);
			return;
		}

		/*
		 * If we aren't blocked on a mutex, we should be.
		 */
		KASSERT(p->p_stat == SMTX, (
		    "process %d(%s):%d holds %s but isn't blocked on a mutex\n",
		    p->p_pid, p->p_comm, p->p_stat,
		    m->mtx_object.lo_name));

		/*
		 * Pick up the mutex that p is blocked on.
		 */
		m = p->p_blocked;
		MPASS(m != NULL);

		/*
		 * Check if the proc needs to be moved up on
		 * the blocked chain
		 */
		if (p == TAILQ_FIRST(&m->mtx_blocked)) {
			continue;
		}

		p1 = TAILQ_PREV(p, procqueue, p_procq);
		if (p1->p_pri.pri_level <= pri) {
			continue;
		}

		/*
		 * Remove proc from blocked chain and determine where
		 * it should be moved up to.  Since we know that p1 has
		 * a lower priority than p, we know that at least one
		 * process in the chain has a lower priority and that
		 * p1 will thus not be NULL after the loop.
		 */
		TAILQ_REMOVE(&m->mtx_blocked, p, p_procq);
		TAILQ_FOREACH(p1, &m->mtx_blocked, p_procq) {
			MPASS(p1->p_magic == P_MAGIC);
			if (p1->p_pri.pri_level > pri)
				break;
		}

		MPASS(p1 != NULL);
		TAILQ_INSERT_BEFORE(p1, p, p_procq);
		CTR4(KTR_LOCK,
		    "propagate_priority: p %p moved before %p on [%p] %s",
		    p, p1, m, m->mtx_object.lo_name);
	}
}

/*
 * Function versions of the inlined __mtx_* macros.  These are used by
 * modules and can also be called from assembly language if needed.
 */
void
_mtx_lock_flags(struct mtx *m, int opts, const char *file, int line)
{

	__mtx_lock_flags(m, opts, file, line);
}

void
_mtx_unlock_flags(struct mtx *m, int opts, const char *file, int line)
{

	__mtx_unlock_flags(m, opts, file, line);
}

void
_mtx_lock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{

	__mtx_lock_spin_flags(m, opts, file, line);
}

void
_mtx_unlock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{

	__mtx_unlock_spin_flags(m, opts, file, line);
}

/*
 * The important part of mtx_trylock{,_flags}()
 * Tries to acquire lock `m.' We do NOT handle recursion here; we assume that
 * if we're called, it's because we know we don't already own this lock.
 */
int
_mtx_trylock(struct mtx *m, int opts, const char *file, int line)
{
	int rval;

	MPASS(curproc != NULL);

	/*
	 * _mtx_trylock does not accept MTX_NOSWITCH option.
	 */
	KASSERT((opts & MTX_NOSWITCH) == 0,
	    ("mtx_trylock() called with invalid option flag(s) %d", opts));

	rval = _obtain_lock(m, curproc);

	LOCK_LOG_TRY("LOCK", &m->mtx_object, opts, rval, file, line);
	if (rval) {
		/*
		 * We do not handle recursion in _mtx_trylock; see the
		 * note at the top of the routine.
		 */
		KASSERT(!mtx_recursed(m),
		    ("mtx_trylock() called on a recursed mutex"));
		mtx_update_flags(m, 1);
		WITNESS_LOCK(&m->mtx_object, opts | LOP_TRYLOCK, file, line);
	}

	return (rval);
}

/*
 * _mtx_lock_sleep: the tougher part of acquiring an MTX_DEF lock.
 *
 * We call this if the lock is either contested (i.e. we need to go to
 * sleep waiting for it), or if we need to recurse on it.
 */
void
_mtx_lock_sleep(struct mtx *m, int opts, const char *file, int line)
{
	struct proc *p = curproc;

	if ((m->mtx_lock & MTX_FLAGMASK) == (uintptr_t)p) {
		m->mtx_recurse++;
		atomic_set_ptr(&m->mtx_lock, MTX_RECURSED);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_lock_sleep: %p recursing", m);
		return;
	}

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR4(KTR_LOCK,
		    "_mtx_lock_sleep: %s contested (lock=%p) at %s:%d",
		    m->mtx_object.lo_name, (void *)m->mtx_lock, file, line);

	while (!_obtain_lock(m, p)) {
		uintptr_t v;
		struct proc *p1;

		mtx_lock_spin(&sched_lock);
		/*
		 * Check if the lock has been released while spinning for
		 * the sched_lock.
		 */
		if ((v = m->mtx_lock) == MTX_UNOWNED) {
			mtx_unlock_spin(&sched_lock);
			continue;
		}

		/*
		 * The mutex was marked contested on release. This means that
		 * there are processes blocked on it.
		 */
		if (v == MTX_CONTESTED) {
			p1 = TAILQ_FIRST(&m->mtx_blocked);
			MPASS(p1 != NULL);
			m->mtx_lock = (uintptr_t)p | MTX_CONTESTED;

			if (p1->p_pri.pri_level < p->p_pri.pri_level)
				SET_PRIO(p, p1->p_pri.pri_level); 
			mtx_unlock_spin(&sched_lock);
			return;
		}

		/*
		 * If the mutex isn't already contested and a failure occurs
		 * setting the contested bit, the mutex was either released
		 * or the state of the MTX_RECURSED bit changed.
		 */
		if ((v & MTX_CONTESTED) == 0 &&
		    !atomic_cmpset_ptr(&m->mtx_lock, (void *)v,
			(void *)(v | MTX_CONTESTED))) {
			mtx_unlock_spin(&sched_lock);
			continue;
		}

		/*
		 * We deffinately must sleep for this lock.
		 */
		mtx_assert(m, MA_NOTOWNED);

#ifdef notyet
		/*
		 * If we're borrowing an interrupted thread's VM context, we
		 * must clean up before going to sleep.
		 */
		if (p->p_ithd != NULL) {
			struct ithd *it = p->p_ithd;

			if (it->it_interrupted) {
				if (LOCK_LOG_TEST(&m->mtx_object, opts))
					CTR2(KTR_LOCK,
				    "_mtx_lock_sleep: %p interrupted %p",
					    it, it->it_interrupted);
				intr_thd_fixup(it);
			}
		}
#endif

		/*
		 * Put us on the list of threads blocked on this mutex.
		 */
		if (TAILQ_EMPTY(&m->mtx_blocked)) {
			p1 = (struct proc *)(m->mtx_lock & MTX_FLAGMASK);
			LIST_INSERT_HEAD(&p1->p_contested, m, mtx_contested);
			TAILQ_INSERT_TAIL(&m->mtx_blocked, p, p_procq);
		} else {
			TAILQ_FOREACH(p1, &m->mtx_blocked, p_procq)
				if (p1->p_pri.pri_level > p->p_pri.pri_level)
					break;
			if (p1)
				TAILQ_INSERT_BEFORE(p1, p, p_procq);
			else
				TAILQ_INSERT_TAIL(&m->mtx_blocked, p, p_procq);
		}

		/*
		 * Save who we're blocked on.
		 */
		p->p_blocked = m;
		p->p_mtxname = m->mtx_object.lo_name;
		p->p_stat = SMTX;
		propagate_priority(p);

		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR3(KTR_LOCK,
			    "_mtx_lock_sleep: p %p blocked on [%p] %s", p, m,
			    m->mtx_object.lo_name);

		mi_switch();

		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR3(KTR_LOCK,
			  "_mtx_lock_sleep: p %p free from blocked on [%p] %s",
			  p, m, m->mtx_object.lo_name);

		mtx_unlock_spin(&sched_lock);
	}

	return;
}

/*
 * _mtx_lock_spin: the tougher part of acquiring an MTX_SPIN lock.
 *
 * This is only called if we need to actually spin for the lock. Recursion
 * is handled inline.
 */
void
_mtx_lock_spin(struct mtx *m, int opts, critical_t mtx_crit, const char *file,
	       int line)
{
	int i = 0;

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spinning", m);

	for (;;) {
		if (_obtain_lock(m, curproc))
			break;

		/* Give interrupts a chance while we spin. */
		critical_exit(mtx_crit);
		while (m->mtx_lock != MTX_UNOWNED) {
			if (i++ < 1000000)
				continue;
			if (i++ < 6000000)
				DELAY(1);
#ifdef DDB
			else if (!db_active)
#else
			else
#endif
			panic("spin lock %s held by %p for > 5 seconds",
			    m->mtx_object.lo_name, (void *)m->mtx_lock);
		}
		mtx_crit = critical_enter();
	}

	m->mtx_savecrit = mtx_crit;
	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spin done", m);

	return;
}

/*
 * _mtx_unlock_sleep: the tougher part of releasing an MTX_DEF lock.
 *
 * We are only called here if the lock is recursed or contested (i.e. we
 * need to wake up a blocked thread).
 */
void
_mtx_unlock_sleep(struct mtx *m, int opts, const char *file, int line)
{
	struct proc *p, *p1;
	struct mtx *m1;
	int pri;

	p = curproc;

	if (mtx_recursed(m)) {
		if (--(m->mtx_recurse) == 0)
			atomic_clear_ptr(&m->mtx_lock, MTX_RECURSED);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p unrecurse", m);
		return;
	}

	mtx_lock_spin(&sched_lock);
	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p contested", m);

	p1 = TAILQ_FIRST(&m->mtx_blocked);
	MPASS(p->p_magic == P_MAGIC);
	MPASS(p1->p_magic == P_MAGIC);

	TAILQ_REMOVE(&m->mtx_blocked, p1, p_procq);

	if (TAILQ_EMPTY(&m->mtx_blocked)) {
		LIST_REMOVE(m, mtx_contested);
		_release_lock_quick(m);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p not held", m);
	} else
		atomic_store_rel_ptr(&m->mtx_lock, (void *)MTX_CONTESTED);

	pri = PRI_MAX;
	LIST_FOREACH(m1, &p->p_contested, mtx_contested) {
		int cp = TAILQ_FIRST(&m1->mtx_blocked)->p_pri.pri_level;
		if (cp < pri)
			pri = cp;
	}

	if (pri > p->p_pri.pri_native)
		pri = p->p_pri.pri_native;
	SET_PRIO(p, pri);

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR2(KTR_LOCK, "_mtx_unlock_sleep: %p contested setrunqueue %p",
		    m, p1);

	p1->p_blocked = NULL;
	p1->p_stat = SRUN;
	setrunqueue(p1);

	if ((opts & MTX_NOSWITCH) == 0 && p1->p_pri.pri_level < pri) {
#ifdef notyet
		if (p->p_ithd != NULL) {
			struct ithd *it = p->p_ithd;

			if (it->it_interrupted) {
				if (LOCK_LOG_TEST(&m->mtx_object, opts))
					CTR2(KTR_LOCK,
				    "_mtx_unlock_sleep: %p interrupted %p",
					    it, it->it_interrupted);
				intr_thd_fixup(it);
			}
		}
#endif
		setrunqueue(p);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR2(KTR_LOCK,
			    "_mtx_unlock_sleep: %p switching out lock=%p", m,
			    (void *)m->mtx_lock);

		mi_switch();
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR2(KTR_LOCK, "_mtx_unlock_sleep: %p resuming lock=%p",
			    m, (void *)m->mtx_lock);
	}

	mtx_unlock_spin(&sched_lock);

	return;
}

/*
 * All the unlocking of MTX_SPIN locks is done inline.
 * See the _rel_spin_lock() macro for the details. 
 */

#ifdef WITNESS
/*
 * Update the lock object flags before calling witness.  Note that when we
 * lock a mutex, this is called after getting the lock, but when unlocking
 * a mutex, this function is called before releasing the lock.
 */
void
_mtx_update_flags(struct mtx *m, int locking)
{

	mtx_assert(m, MA_OWNED);
	if (locking) {
		m->mtx_object.lo_flags |= LO_LOCKED;
		if (mtx_recursed(m))
			m->mtx_object.lo_flags |= LO_RECURSED;
		else
			/* XXX: we shouldn't need this in theory. */
			m->mtx_object.lo_flags &= ~LO_RECURSED;
	} else {
		switch (m->mtx_recurse) {
		case 0:
			/* XXX: we shouldn't need the LO_RECURSED in theory. */
			m->mtx_object.lo_flags &= ~(LO_LOCKED | LO_RECURSED);
			break;
		case 1:
			m->mtx_object.lo_flags &= ~(LO_RECURSED);
			break;
		default:
			break;
		}
	}
}
#endif

/*
 * The backing function for the INVARIANTS-enabled mtx_assert()
 */
#ifdef INVARIANT_SUPPORT
void
_mtx_assert(struct mtx *m, int what, const char *file, int line)
{
	switch (what) {
	case MA_OWNED:
	case MA_OWNED | MA_RECURSED:
	case MA_OWNED | MA_NOTRECURSED:
		if (!mtx_owned(m))
			panic("mutex %s not owned at %s:%d",
			    m->mtx_object.lo_name, file, line);
		if (mtx_recursed(m)) {
			if ((what & MA_NOTRECURSED) != 0)
				panic("mutex %s recursed at %s:%d",
				    m->mtx_object.lo_name, file, line);
		} else if ((what & MA_RECURSED) != 0) {
			panic("mutex %s unrecursed at %s:%d",
			    m->mtx_object.lo_name, file, line);
		}
		break;
	case MA_NOTOWNED:
		if (mtx_owned(m))
			panic("mutex %s owned at %s:%d",
			    m->mtx_object.lo_name, file, line);
		break;
	default:
		panic("unknown mtx_assert at %s:%d", file, line);
	}
}
#endif

/*
 * The MUTEX_DEBUG-enabled mtx_validate()
 *
 * Most of these checks have been moved off into the LO_INITIALIZED flag
 * maintained by the witness code.
 */
#ifdef MUTEX_DEBUG

void	mtx_validate __P((struct mtx *));

void
mtx_validate(struct mtx *m)
{

/*
 * XXX - When kernacc() is fixed on the alpha to handle K0_SEG memory properly
 * we can re-enable the kernacc() checks.
 */
#ifndef __alpha__
	if (!kernacc((caddr_t)m, sizeof(m), VM_PROT_READ | VM_PROT_WRITE))
		panic("Can't read and write to mutex %p", m);
#endif
}
#endif

/*
 * Mutex initialization routine; initialize lock `m' of type contained in
 * `opts' with options contained in `opts' and description `description.'
 */ 
void
mtx_init(struct mtx *m, const char *description, int opts)
{
	struct lock_object *lock;

	MPASS((opts & ~(MTX_SPIN | MTX_QUIET | MTX_RECURSE |
	    MTX_SLEEPABLE | MTX_NOWITNESS)) == 0);

#ifdef MUTEX_DEBUG
	/* Diagnostic and error correction */
	mtx_validate(m);
#endif

	bzero(m, sizeof(*m));
	lock = &m->mtx_object;
	if (opts & MTX_SPIN)
		lock->lo_class = &lock_class_mtx_spin;
	else
		lock->lo_class = &lock_class_mtx_sleep;
	lock->lo_name = description;
	if (opts & MTX_QUIET)
		lock->lo_flags = LO_QUIET;
	if (opts & MTX_RECURSE)
		lock->lo_flags |= LO_RECURSABLE;
	if (opts & MTX_SLEEPABLE)
		lock->lo_flags |= LO_SLEEPABLE;
	if ((opts & MTX_NOWITNESS) == 0)
		lock->lo_flags |= LO_WITNESS;

	m->mtx_lock = MTX_UNOWNED;
	TAILQ_INIT(&m->mtx_blocked);

	LOCK_LOG_INIT(lock, opts);

	WITNESS_INIT(lock);
}

/*
 * Remove lock `m' from all_mtx queue.  We don't allow MTX_QUIET to be
 * passed in as a flag here because if the corresponding mtx_init() was
 * called with MTX_QUIET set, then it will already be set in the mutex's
 * flags.
 */
void
mtx_destroy(struct mtx *m)
{

	LOCK_LOG_DESTROY(&m->mtx_object, 0);

	if (!mtx_owned(m))
		MPASS(mtx_unowned(m));
	else {
		MPASS((m->mtx_lock & (MTX_RECURSED|MTX_CONTESTED)) == 0);

		/* Tell witness this isn't locked to make it happy. */
		m->mtx_object.lo_flags &= ~LO_LOCKED;
		WITNESS_UNLOCK(&m->mtx_object, MTX_NOSWITCH, __FILE__,
		    __LINE__);
	}

	WITNESS_DESTROY(&m->mtx_object);
}
