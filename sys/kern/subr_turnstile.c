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
#include "opt_witness.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
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

#include <sys/mutex.h>

/*
 * The WITNESS-enabled mutex debug structure.
 */
#ifdef WITNESS
struct mtx_debug {
	struct witness	*mtxd_witness;
	LIST_ENTRY(mtx)	mtxd_held;
	const char	*mtxd_file;
	int		mtxd_line;
};

#define mtx_held	mtx_debug->mtxd_held
#define	mtx_file	mtx_debug->mtxd_file
#define	mtx_line	mtx_debug->mtxd_line
#define	mtx_witness	mtx_debug->mtxd_witness
#endif	/* WITNESS */

/*
 * Internal utility macros.
 */
#define mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)

#define mtx_owner(m)	(mtx_unowned((m)) ? NULL \
	: (struct proc *)((m)->mtx_lock & MTX_FLAGMASK))

#define SET_PRIO(p, pri)	(p)->p_pri.pri_level = (pri)

/*
 * Early WITNESS-enabled declarations.
 */
#ifdef WITNESS

/*
 * Internal WITNESS routines which must be prototyped early.
 *
 * XXX: When/if witness code is cleaned up, it would be wise to place all
 *	witness prototyping early in this file.
 */ 
static void witness_init(struct mtx *, int flag);
static void witness_destroy(struct mtx *);
static void witness_display(void(*)(const char *fmt, ...));

MALLOC_DEFINE(M_WITNESS, "witness", "witness mtx_debug structure");

/* All mutexes in system (used for debug/panic) */
static struct mtx_debug all_mtx_debug = { NULL, {NULL, NULL}, NULL, 0 };

/*
 * This global is set to 0 once it becomes safe to use the witness code.
 */
static int witness_cold = 1;

#else	/* WITNESS */

/* XXX XXX XXX
 * flag++ is sleazoid way of shuting up warning
 */
#define witness_init(m, flag) flag++
#define witness_destroy(m)
#define witness_try_enter(m, t, f, l)
#endif	/* WITNESS */

/*
 * All mutex locks in system are kept on the all_mtx list.
 */
static struct mtx all_mtx = { MTX_UNOWNED, 0, 0, 0, "All mutexes queue head",
	TAILQ_HEAD_INITIALIZER(all_mtx.mtx_blocked),
	{ NULL, NULL }, &all_mtx, &all_mtx,
#ifdef WITNESS
	&all_mtx_debug
#else
	NULL
#endif
	 };

/*
 * Global variables for book keeping.
 */
static int	mtx_cur_cnt;
static int	mtx_max_cnt;

/*
 * Couple of strings for KTR_LOCK tracing in order to avoid duplicates.
 */
char	STR_mtx_lock_slp[] = "GOT (sleep) %s [%p] r=%d at %s:%d";
char	STR_mtx_unlock_slp[] = "REL (sleep) %s [%p] r=%d at %s:%d";
char	STR_mtx_lock_spn[] = "GOT (spin) %s [%p] r=%d at %s:%d";
char	STR_mtx_unlock_spn[] = "REL (spin) %s [%p] r=%d at %s:%d";

/*
 * Prototypes for non-exported routines.
 *
 * NOTE: Prototypes for witness routines are placed at the bottom of the file. 
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
			MPASS(p->p_stat == SRUN || p->p_stat == SZOMB);
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
		    m->mtx_description));

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
		    p, p1, m, m->mtx_description);
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

#ifdef WITNESS
	if (rval && m->mtx_witness != NULL) {
		/*
		 * We do not handle recursion in _mtx_trylock; see the
		 * note at the top of the routine.
		 */
		KASSERT(!mtx_recursed(m),
		    ("mtx_trylock() called on a recursed mutex"));
		witness_try_enter(m, (opts | m->mtx_flags), file, line);
	}
#endif	/* WITNESS */

	if ((opts & MTX_QUIET) == 0)
		CTR5(KTR_LOCK, "TRY_LOCK %s [%p] result=%d at %s:%d",
		    m->mtx_description, m, rval, file, line);

	return rval;
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
		if ((opts & MTX_QUIET) == 0)
			CTR1(KTR_LOCK, "_mtx_lock_sleep: %p recursing", m);
		return;
	}

	if ((opts & MTX_QUIET) == 0)
		CTR4(KTR_LOCK,
		    "_mtx_lock_sleep: %s contested (lock=%p) at %s:%d",
		    m->mtx_description, (void *)m->mtx_lock, file, line);

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
				if ((opts & MTX_QUIET) == 0)
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
		p->p_mtxname = m->mtx_description;
		p->p_stat = SMTX;
		propagate_priority(p);

		if ((opts & MTX_QUIET) == 0)
			CTR3(KTR_LOCK,
			    "_mtx_lock_sleep: p %p blocked on [%p] %s", p, m,
			    m->mtx_description);

		mi_switch();

		if ((opts & MTX_QUIET) == 0)
			CTR3(KTR_LOCK,
			  "_mtx_lock_sleep: p %p free from blocked on [%p] %s",
			  p, m, m->mtx_description);

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

	if ((opts & MTX_QUIET) == 0)
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spinning", m);

	for (;;) {
		if (_obtain_lock(m, curproc))
			break;

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
			    m->mtx_description, (void *)m->mtx_lock);
		}
	}

	m->mtx_savecrit = mtx_crit;
	if ((opts & MTX_QUIET) == 0)
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
		if ((opts & MTX_QUIET) == 0)
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p unrecurse", m);
		return;
	}

	mtx_lock_spin(&sched_lock);
	if ((opts & MTX_QUIET) == 0)
		CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p contested", m);

	p1 = TAILQ_FIRST(&m->mtx_blocked);
	MPASS(p->p_magic == P_MAGIC);
	MPASS(p1->p_magic == P_MAGIC);

	TAILQ_REMOVE(&m->mtx_blocked, p1, p_procq);

	if (TAILQ_EMPTY(&m->mtx_blocked)) {
		LIST_REMOVE(m, mtx_contested);
		_release_lock_quick(m);
		if ((opts & MTX_QUIET) == 0)
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

	if ((opts & MTX_QUIET) == 0)
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
				if ((opts & MTX_QUIET) == 0)
					CTR2(KTR_LOCK,
				    "_mtx_unlock_sleep: %p interrupted %p",
					    it, it->it_interrupted);
				intr_thd_fixup(it);
			}
		}
#endif
		setrunqueue(p);
		if ((opts & MTX_QUIET) == 0)
			CTR2(KTR_LOCK,
			    "_mtx_unlock_sleep: %p switching out lock=%p", m,
			    (void *)m->mtx_lock);

		mi_switch();
		if ((opts & MTX_QUIET) == 0)
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
			    m->mtx_description, file, line);
		if (mtx_recursed(m)) {
			if ((what & MA_NOTRECURSED) != 0)
				panic("mutex %s recursed at %s:%d",
				    m->mtx_description, file, line);
		} else if ((what & MA_RECURSED) != 0) {
			panic("mutex %s unrecursed at %s:%d",
			    m->mtx_description, file, line);
		}
		break;
	case MA_NOTOWNED:
		if (mtx_owned(m))
			panic("mutex %s owned at %s:%d",
			    m->mtx_description, file, line);
		break;
	default:
		panic("unknown mtx_assert at %s:%d", file, line);
	}
}
#endif

/*
 * The MUTEX_DEBUG-enabled mtx_validate()
 */
#define MV_DESTROY	0	/* validate before destory */
#define MV_INIT		1	/* validate before init */

#ifdef MUTEX_DEBUG

int mtx_validate __P((struct mtx *, int));

int
mtx_validate(struct mtx *m, int when)
{
	struct mtx *mp;
	int i;
	int retval = 0;

#ifdef WITNESS
	if (witness_cold)
		return 0;
#endif
	if (m == &all_mtx || cold)
		return 0;

	mtx_lock(&all_mtx);
/*
 * XXX - When kernacc() is fixed on the alpha to handle K0_SEG memory properly
 * we can re-enable the kernacc() checks.
 */
#ifndef __alpha__
	MPASS(kernacc((caddr_t)all_mtx.mtx_next, sizeof(uintptr_t),
	    VM_PROT_READ) == 1);
#endif
	MPASS(all_mtx.mtx_next->mtx_prev == &all_mtx);
	for (i = 0, mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next) {
#ifndef __alpha__
		if (kernacc((caddr_t)mp->mtx_next, sizeof(uintptr_t),
		    VM_PROT_READ) != 1) {
			panic("mtx_validate: mp=%p mp->mtx_next=%p",
			    mp, mp->mtx_next);
		}
#endif
		i++;
		if (i > mtx_cur_cnt) {
			panic("mtx_validate: too many in chain, known=%d\n",
			    mtx_cur_cnt);
		}
	}
	MPASS(i == mtx_cur_cnt); 
	switch (when) {
	case MV_DESTROY:
		for (mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next)
			if (mp == m)
				break;
		MPASS(mp == m);
		break;
	case MV_INIT:
		for (mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next)
		if (mp == m) {
			/*
			 * Not good. This mutex already exists.
			 */
			printf("re-initing existing mutex %s\n",
			    m->mtx_description);
			MPASS(m->mtx_lock == MTX_UNOWNED);
			retval = 1;
		}
	}
	mtx_unlock(&all_mtx);
	return (retval);
}
#endif

/*
 * Mutex initialization routine; initialize lock `m' of type contained in
 * `opts' with options contained in `opts' and description `description.'
 * Place on "all_mtx" queue.
 */ 
void
mtx_init(struct mtx *m, const char *description, int opts)
{

	if ((opts & MTX_QUIET) == 0)
		CTR2(KTR_LOCK, "mtx_init %p (%s)", m, description);

#ifdef MUTEX_DEBUG
	/* Diagnostic and error correction */
	if (mtx_validate(m, MV_INIT))
		return;
#endif

	bzero((void *)m, sizeof *m);
	TAILQ_INIT(&m->mtx_blocked);

#ifdef WITNESS
	if (!witness_cold) {
		m->mtx_debug = malloc(sizeof(struct mtx_debug),
		    M_WITNESS, M_NOWAIT | M_ZERO);
		MPASS(m->mtx_debug != NULL);
	}
#endif

	m->mtx_description = description;
	m->mtx_flags = opts;
	m->mtx_lock = MTX_UNOWNED;

	/* Put on all mutex queue */
	mtx_lock(&all_mtx);
	m->mtx_next = &all_mtx;
	m->mtx_prev = all_mtx.mtx_prev;
	m->mtx_prev->mtx_next = m;
	all_mtx.mtx_prev = m;
	if (++mtx_cur_cnt > mtx_max_cnt)
		mtx_max_cnt = mtx_cur_cnt;
	mtx_unlock(&all_mtx);

#ifdef WITNESS
	if (!witness_cold)
		witness_init(m, opts);
#endif
}

/*
 * Remove lock `m' from all_mtx queue.
 */
void
mtx_destroy(struct mtx *m)
{

#ifdef WITNESS
	KASSERT(!witness_cold, ("%s: Cannot destroy while still cold\n",
	    __FUNCTION__));
#endif

	CTR2(KTR_LOCK, "mtx_destroy %p (%s)", m, m->mtx_description);

#ifdef MUTEX_DEBUG
	if (m->mtx_next == NULL)
		panic("mtx_destroy: %p (%s) already destroyed",
		    m, m->mtx_description);

	if (!mtx_owned(m)) {
		MPASS(m->mtx_lock == MTX_UNOWNED);
	} else {
		MPASS((m->mtx_lock & (MTX_RECURSED|MTX_CONTESTED)) == 0);
	}

	/* diagnostic */
	mtx_validate(m, MV_DESTROY);
#endif

#ifdef WITNESS
	if (m->mtx_witness)
		witness_destroy(m);
#endif /* WITNESS */

	/* Remove from the all mutex queue */
	mtx_lock(&all_mtx);
	m->mtx_next->mtx_prev = m->mtx_prev;
	m->mtx_prev->mtx_next = m->mtx_next;

#ifdef MUTEX_DEBUG
	m->mtx_next = m->mtx_prev = NULL;
#endif

#ifdef WITNESS
	free(m->mtx_debug, M_WITNESS);
	m->mtx_debug = NULL;
#endif

	mtx_cur_cnt--;
	mtx_unlock(&all_mtx);
}


/*
 * The WITNESS-enabled diagnostic code.
 */
#ifdef WITNESS
static void
witness_fixup(void *dummy __unused)
{
	struct mtx *mp;

	/*
	 * We have to release Giant before initializing its witness
	 * structure so that WITNESS doesn't get confused.
	 */
	mtx_unlock(&Giant);
	mtx_assert(&Giant, MA_NOTOWNED);

	mtx_lock(&all_mtx);

	/* Iterate through all mutexes and finish up mutex initialization. */
	for (mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next) {

		mp->mtx_debug = malloc(sizeof(struct mtx_debug),
		    M_WITNESS, M_NOWAIT | M_ZERO);
		MPASS(mp->mtx_debug != NULL);

		witness_init(mp, mp->mtx_flags);
	}
	mtx_unlock(&all_mtx);

	/* Mark the witness code as being ready for use. */
	atomic_store_rel_int(&witness_cold, 0);

	mtx_lock(&Giant);
}
SYSINIT(wtnsfxup, SI_SUB_MUTEX, SI_ORDER_FIRST, witness_fixup, NULL)

#define WITNESS_COUNT 200
#define	WITNESS_NCHILDREN 2

int witness_watch = 1;

struct witness {
	struct witness	*w_next;
	const char	*w_description;
	const char	*w_file;
	int		 w_line;
	struct witness	*w_morechildren;
	u_char		 w_childcnt;
	u_char		 w_Giant_squawked:1;
	u_char		 w_other_squawked:1;
	u_char		 w_same_squawked:1;
	u_char		 w_spin:1;	/* MTX_SPIN type mutex. */
	u_int		 w_level;
	struct witness	*w_children[WITNESS_NCHILDREN];
};

struct witness_blessed {
	char 	*b_lock1;
	char	*b_lock2;
};

#ifdef DDB
/*
 * When DDB is enabled and witness_ddb is set to 1, it will cause the system to
 * drop into kdebug() when:
 *	- a lock heirarchy violation occurs
 *	- locks are held when going to sleep.
 */
int	witness_ddb;
#ifdef WITNESS_DDB
TUNABLE_INT_DECL("debug.witness_ddb", 1, witness_ddb);
#else
TUNABLE_INT_DECL("debug.witness_ddb", 0, witness_ddb);
#endif
SYSCTL_INT(_debug, OID_AUTO, witness_ddb, CTLFLAG_RW, &witness_ddb, 0, "");
#endif /* DDB */

int	witness_skipspin;
#ifdef WITNESS_SKIPSPIN
TUNABLE_INT_DECL("debug.witness_skipspin", 1, witness_skipspin);
#else
TUNABLE_INT_DECL("debug.witness_skipspin", 0, witness_skipspin);
#endif
SYSCTL_INT(_debug, OID_AUTO, witness_skipspin, CTLFLAG_RD, &witness_skipspin, 0,
    "");

/*
 * Witness-enabled globals
 */
static struct mtx	w_mtx;
static struct witness	*w_free;
static struct witness	*w_all;
static int		 w_inited;
static int		 witness_dead;	/* fatal error, probably no memory */

static struct witness	 w_data[WITNESS_COUNT];

/*
 * Internal witness routine prototypes
 */
static struct witness *enroll(const char *description, int flag);
static int itismychild(struct witness *parent, struct witness *child);
static void removechild(struct witness *parent, struct witness *child);
static int isitmychild(struct witness *parent, struct witness *child);
static int isitmydescendant(struct witness *parent, struct witness *child);
static int dup_ok(struct witness *);
static int blessed(struct witness *, struct witness *);
static void
    witness_displaydescendants(void(*)(const char *fmt, ...), struct witness *);
static void witness_leveldescendents(struct witness *parent, int level);
static void witness_levelall(void);
static struct witness * witness_get(void);
static void witness_free(struct witness *m);

static char *ignore_list[] = {
	"witness lock",
	NULL
};

static char *spin_order_list[] = {
#if defined(__i386__) && defined (SMP)
	"com",
#endif
	"sio",
#ifdef __i386__
	"cy",
#endif
	"ng_node",
	"ng_worklist",
	"ithread table lock",
	"ithread list lock",
	"sched lock",
#ifdef __i386__
	"clk",
#endif
	"callout",
	/*
	 * leaf locks
	 */
#ifdef SMP
#ifdef __i386__
	"ap boot",
	"imen",
#endif
	"smp rendezvous",
#endif
	NULL
};

static char *order_list[] = {
	"Giant", "proctree", "allproc", "process lock", "uidinfo hash",
	    "uidinfo struct", NULL,
	NULL
};

static char *dup_list[] = {
	"process lock",
	NULL
};

static char *sleep_list[] = {
	"Giant",
	NULL
};

/*
 * Pairs of locks which have been blessed
 * Don't complain about order problems with blessed locks
 */
static struct witness_blessed blessed_list[] = {
};
static int blessed_count =
	sizeof(blessed_list) / sizeof(struct witness_blessed);

static void
witness_init(struct mtx *m, int flag)
{
	m->mtx_witness = enroll(m->mtx_description, flag);
}

static void
witness_destroy(struct mtx *m)
{
	struct mtx *m1;
	struct proc *p;
	p = curproc;
	LIST_FOREACH(m1, &p->p_heldmtx, mtx_held) {
		if (m1 == m) {
			LIST_REMOVE(m, mtx_held);
			break;
		}
	}
	return;

}

static void
witness_display(void(*prnt)(const char *fmt, ...))
{
	struct witness *w, *w1;
	int level, found;

	KASSERT(!witness_cold, ("%s: witness_cold\n", __FUNCTION__));
	witness_levelall();

	/*
	 * First, handle sleep mutexes which have been acquired at least
	 * once.
	 */
	prnt("Sleep mutexes:\n");
	for (w = w_all; w; w = w->w_next) {
		if (w->w_file == NULL || w->w_spin)
			continue;
		for (w1 = w_all; w1; w1 = w1->w_next) {
			if (isitmychild(w1, w))
				break;
		}
		if (w1 != NULL)
			continue;
		/*
		 * This lock has no anscestors, display its descendants. 
		 */
		witness_displaydescendants(prnt, w);
	}
	
	/*
	 * Now do spin mutexes which have been acquired at least once.
	 */
	prnt("\nSpin mutexes:\n");
	level = 0;
	while (level < sizeof(spin_order_list) / sizeof(char *)) {
		found = 0;
		for (w = w_all; w; w = w->w_next) {
			if (w->w_file == NULL || !w->w_spin)
				continue;
			if (w->w_level == 1 << level) {
				witness_displaydescendants(prnt, w);
				level++;
				found = 1;
			}
		}
		if (found == 0)
			level++;
	}
	
	/*
	 * Finally, any mutexes which have not been acquired yet.
	 */
	prnt("\nMutexes which were never acquired:\n");
	for (w = w_all; w; w = w->w_next) {
		if (w->w_file != NULL)
			continue;
		prnt("%s\n", w->w_description);
	}
}

void
witness_enter(struct mtx *m, int flags, const char *file, int line)
{
	struct witness *w, *w1;
	struct mtx *m1;
	struct proc *p;
	int i;
#ifdef DDB
	int go_into_ddb = 0;
#endif /* DDB */

	if (witness_cold || m->mtx_witness == NULL || panicstr)
		return;
	w = m->mtx_witness;
	p = curproc;

	if (flags & MTX_SPIN) {
		if ((m->mtx_flags & MTX_SPIN) == 0)
			panic("mutex_enter: MTX_SPIN on MTX_DEF mutex %s @"
			    " %s:%d", m->mtx_description, file, line);
		if (mtx_recursed(m)) {
			if ((m->mtx_flags & MTX_RECURSE) == 0)
				panic("mutex_enter: recursion on non-recursive"
				    " mutex %s @ %s:%d", m->mtx_description,
				    file, line);
			return;
		}
		mtx_lock_spin_flags(&w_mtx, MTX_QUIET);
		i = PCPU_GET(witness_spin_check);
		if (i != 0 && w->w_level < i) {
			mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
			panic("mutex_enter(%s:%x, MTX_SPIN) out of order @"
			    " %s:%d already holding %s:%x",
			    m->mtx_description, w->w_level, file, line,
			    spin_order_list[ffs(i)-1], i);
		}
		PCPU_SET(witness_spin_check, i | w->w_level);
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
		p->p_spinlocks++;
		MPASS(p->p_spinlocks > 0);
		w->w_file = file;
		w->w_line = line;
		m->mtx_line = line;
		m->mtx_file = file;
		return;
	}
	if ((m->mtx_flags & MTX_SPIN) != 0)
		panic("mutex_enter: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (mtx_recursed(m)) {
		if ((m->mtx_flags & MTX_RECURSE) == 0)
			panic("mutex_enter: recursion on non-recursive"
			    " mutex %s @ %s:%d", m->mtx_description,
			    file, line);
		return;
	}
	if (witness_dead)
		goto out;
	if (cold)
		goto out;

	if (p->p_spinlocks != 0)
		panic("blockable mtx_lock() of %s when not legal @ %s:%d",
			    m->mtx_description, file, line);
	/*
	 * Is this the first mutex acquired 
	 */
	if ((m1 = LIST_FIRST(&p->p_heldmtx)) == NULL)
		goto out;

	if ((w1 = m1->mtx_witness) == w) {
		if (w->w_same_squawked || dup_ok(w))
			goto out;
		w->w_same_squawked = 1;
		printf("acquring duplicate lock of same type: \"%s\"\n", 
			m->mtx_description);
		printf(" 1st @ %s:%d\n", w->w_file, w->w_line);
		printf(" 2nd @ %s:%d\n", file, line);
#ifdef DDB
		go_into_ddb = 1;
#endif /* DDB */
		goto out;
	}
	MPASS(!mtx_owned(&w_mtx));
	mtx_lock_spin_flags(&w_mtx, MTX_QUIET);
	/*
	 * If we have a known higher number just say ok
	 */
	if (witness_watch > 1 && w->w_level > w1->w_level) {
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
		goto out;
	}
	if (isitmydescendant(m1->mtx_witness, w)) {
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
		goto out;
	}
	for (i = 0; m1 != NULL; m1 = LIST_NEXT(m1, mtx_held), i++) {

		MPASS(i < 200);
		w1 = m1->mtx_witness;
		if (isitmydescendant(w, w1)) {
			mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
			if (blessed(w, w1))
				goto out;
			if (m1 == &Giant) {
				if (w1->w_Giant_squawked)
					goto out;
				else
					w1->w_Giant_squawked = 1;
			} else {
				if (w1->w_other_squawked)
					goto out;
				else
					w1->w_other_squawked = 1;
			}
			printf("lock order reversal\n");
			printf(" 1st %s last acquired @ %s:%d\n",
			    w->w_description, w->w_file, w->w_line);
			printf(" 2nd %p %s @ %s:%d\n",
			    m1, w1->w_description, w1->w_file, w1->w_line);
			printf(" 3rd %p %s @ %s:%d\n",
			    m, w->w_description, file, line);
#ifdef DDB
			go_into_ddb = 1;
#endif /* DDB */
			goto out;
		}
	}
	m1 = LIST_FIRST(&p->p_heldmtx);
	if (!itismychild(m1->mtx_witness, w))
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);

out:
#ifdef DDB
	if (witness_ddb && go_into_ddb)
		Debugger("witness_enter");
#endif /* DDB */
	w->w_file = file;
	w->w_line = line;
	m->mtx_line = line;
	m->mtx_file = file;

	/*
	 * If this pays off it likely means that a mutex being witnessed
	 * is acquired in hardclock. Put it in the ignore list. It is
	 * likely not the mutex this assert fails on.
	 */
	MPASS(m->mtx_held.le_prev == NULL);
	LIST_INSERT_HEAD(&p->p_heldmtx, (struct mtx*)m, mtx_held);
}

void
witness_try_enter(struct mtx *m, int flags, const char *file, int line)
{
	struct proc *p;
	struct witness *w = m->mtx_witness;

	if (witness_cold)
		return;
	if (panicstr)
		return;
	if (flags & MTX_SPIN) {
		if ((m->mtx_flags & MTX_SPIN) == 0)
			panic("mutex_try_enter: "
			    "MTX_SPIN on MTX_DEF mutex %s @ %s:%d",
			    m->mtx_description, file, line);
		if (mtx_recursed(m)) {
			if ((m->mtx_flags & MTX_RECURSE) == 0)
				panic("mutex_try_enter: recursion on"
				    " non-recursive mutex %s @ %s:%d",
				    m->mtx_description, file, line);
			return;
		}
		mtx_lock_spin_flags(&w_mtx, MTX_QUIET);
		PCPU_SET(witness_spin_check,
		    PCPU_GET(witness_spin_check) | w->w_level);
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
		w->w_file = file;
		w->w_line = line;
		m->mtx_line = line;
		m->mtx_file = file;
		return;
	}

	if ((m->mtx_flags & MTX_SPIN) != 0)
		panic("mutex_try_enter: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (mtx_recursed(m)) {
		if ((m->mtx_flags & MTX_RECURSE) == 0)
			panic("mutex_try_enter: recursion on non-recursive"
			    " mutex %s @ %s:%d", m->mtx_description, file,
			    line);
		return;
	}
	w->w_file = file;
	w->w_line = line;
	m->mtx_line = line;
	m->mtx_file = file;
	p = curproc;
	MPASS(m->mtx_held.le_prev == NULL);
	LIST_INSERT_HEAD(&p->p_heldmtx, (struct mtx*)m, mtx_held);
}

void
witness_exit(struct mtx *m, int flags, const char *file, int line)
{
	struct witness *w;
	struct proc *p;

	if (witness_cold || m->mtx_witness == NULL || panicstr)
		return;
	w = m->mtx_witness;
	p = curproc;

	if (flags & MTX_SPIN) {
		if ((m->mtx_flags & MTX_SPIN) == 0)
			panic("mutex_exit: MTX_SPIN on MTX_DEF mutex %s @"
			    " %s:%d", m->mtx_description, file, line);
		if (mtx_recursed(m)) {
			if ((m->mtx_flags & MTX_RECURSE) == 0)
				panic("mutex_exit: recursion on non-recursive"
				    " mutex %s @ %s:%d", m->mtx_description,
				    file, line); 
			return;
		}
		mtx_lock_spin_flags(&w_mtx, MTX_QUIET);
		PCPU_SET(witness_spin_check,
		    PCPU_GET(witness_spin_check) & ~w->w_level);
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
		MPASS(p->p_spinlocks > 0);
		p->p_spinlocks--;
		return;
	}
	if ((m->mtx_flags & MTX_SPIN) != 0)
		panic("mutex_exit: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (mtx_recursed(m)) {
		if ((m->mtx_flags & MTX_RECURSE) == 0)
			panic("mutex_exit: recursion on non-recursive"
			    " mutex %s @ %s:%d", m->mtx_description,
			    file, line); 
		return;
	}

	if ((flags & MTX_NOSWITCH) == 0 && p->p_spinlocks != 0 && !cold)
		panic("switchable mtx_unlock() of %s when not legal @ %s:%d",
			    m->mtx_description, file, line);
	LIST_REMOVE(m, mtx_held);
	m->mtx_held.le_prev = NULL;
}

int
witness_sleep(int check_only, struct mtx *mtx, const char *file, int line)
{
	struct mtx *m;
	struct proc *p;
	char **sleep;
	int n = 0;

	KASSERT(!witness_cold, ("%s: witness_cold\n", __FUNCTION__));
	p = curproc;
	LIST_FOREACH(m, &p->p_heldmtx, mtx_held) {
		if (m == mtx)
			continue;
		for (sleep = sleep_list; *sleep!= NULL; sleep++)
			if (strcmp(m->mtx_description, *sleep) == 0)
				goto next;
		if (n == 0)
			printf("Whee!\n");
		printf("%s:%d: %s with \"%s\" locked from %s:%d\n",
			file, line, check_only ? "could sleep" : "sleeping",
			m->mtx_description,
			m->mtx_witness->w_file, m->mtx_witness->w_line);
		n++;
	next:
	}
#ifdef DDB
	if (witness_ddb && n)
		Debugger("witness_sleep");
#endif /* DDB */
	return (n);
}

static struct witness *
enroll(const char *description, int flag)
{
	int i;
	struct witness *w, *w1;
	char **ignore;
	char **order;

	if (!witness_watch)
		return (NULL);
	for (ignore = ignore_list; *ignore != NULL; ignore++)
		if (strcmp(description, *ignore) == 0)
			return (NULL);

	if (w_inited == 0) {
		mtx_init(&w_mtx, "witness lock", MTX_SPIN);
		for (i = 0; i < WITNESS_COUNT; i++) {
			w = &w_data[i];
			witness_free(w);
		}
		w_inited = 1;
		for (order = order_list; *order != NULL; order++) {
			w = enroll(*order, MTX_DEF);
			w->w_file = "order list";
			for (order++; *order != NULL; order++) {
				w1 = enroll(*order, MTX_DEF);
				w1->w_file = "order list";
				itismychild(w, w1);
				w = w1;
    	    	    	}
		}
	}
	if ((flag & MTX_SPIN) && witness_skipspin)
		return (NULL);
	mtx_lock_spin_flags(&w_mtx, MTX_QUIET);
	for (w = w_all; w; w = w->w_next) {
		if (strcmp(description, w->w_description) == 0) {
			mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
			return (w);
		}
	}
	if ((w = witness_get()) == NULL)
		return (NULL);
	w->w_next = w_all;
	w_all = w;
	w->w_description = description;
	mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
	if (flag & MTX_SPIN) {
		w->w_spin = 1;
	
		i = 1;
		for (order = spin_order_list; *order != NULL; order++) {
			if (strcmp(description, *order) == 0)
				break;
			i <<= 1;
		}
		if (*order == NULL)
			panic("spin lock %s not in order list", description);
		w->w_level = i; 
	}

	return (w);
}

static int
itismychild(struct witness *parent, struct witness *child)
{
	static int recursed;

	/*
	 * Insert "child" after "parent"
	 */
	while (parent->w_morechildren)
		parent = parent->w_morechildren;

	if (parent->w_childcnt == WITNESS_NCHILDREN) {
		if ((parent->w_morechildren = witness_get()) == NULL)
			return (1);
		parent = parent->w_morechildren;
	}
	MPASS(child != NULL);
	parent->w_children[parent->w_childcnt++] = child;
	/*
	 * now prune whole tree
	 */
	if (recursed)
		return (0);
	recursed = 1;
	for (child = w_all; child != NULL; child = child->w_next) {
		for (parent = w_all; parent != NULL;
		    parent = parent->w_next) {
			if (!isitmychild(parent, child))
				continue;
			removechild(parent, child);
			if (isitmydescendant(parent, child))
				continue;
			itismychild(parent, child);
		}
	}
	recursed = 0;
	witness_levelall();
	return (0);
}

static void
removechild(struct witness *parent, struct witness *child)
{
	struct witness *w, *w1;
	int i;

	for (w = parent; w != NULL; w = w->w_morechildren)
		for (i = 0; i < w->w_childcnt; i++)
			if (w->w_children[i] == child)
				goto found;
	return;
found:
	for (w1 = w; w1->w_morechildren != NULL; w1 = w1->w_morechildren)
		continue;
	w->w_children[i] = w1->w_children[--w1->w_childcnt];
	MPASS(w->w_children[i] != NULL);

	if (w1->w_childcnt != 0)
		return;

	if (w1 == parent)
		return;
	for (w = parent; w->w_morechildren != w1; w = w->w_morechildren)
		continue;
	w->w_morechildren = 0;
	witness_free(w1);
}

static int
isitmychild(struct witness *parent, struct witness *child)
{
	struct witness *w;
	int i;

	for (w = parent; w != NULL; w = w->w_morechildren) {
		for (i = 0; i < w->w_childcnt; i++) {
			if (w->w_children[i] == child)
				return (1);
		}
	}
	return (0);
}

static int
isitmydescendant(struct witness *parent, struct witness *child)
{
	struct witness *w;
	int i;
	int j;

	for (j = 0, w = parent; w != NULL; w = w->w_morechildren, j++) {
		MPASS(j < 1000);
		for (i = 0; i < w->w_childcnt; i++) {
			if (w->w_children[i] == child)
				return (1);
		}
		for (i = 0; i < w->w_childcnt; i++) {
			if (isitmydescendant(w->w_children[i], child))
				return (1);
		}
	}
	return (0);
}

void
witness_levelall (void)
{
	struct witness *w, *w1;

	for (w = w_all; w; w = w->w_next)
		if (!(w->w_spin))
			w->w_level = 0;
	for (w = w_all; w; w = w->w_next) {
		if (w->w_spin)
			continue;
		for (w1 = w_all; w1; w1 = w1->w_next) {
			if (isitmychild(w1, w))
				break;
		}
		if (w1 != NULL)
			continue;
		witness_leveldescendents(w, 0);
	}
}

static void
witness_leveldescendents(struct witness *parent, int level)
{
	int i;
	struct witness *w;

	if (parent->w_level < level)
		parent->w_level = level;
	level++;
	for (w = parent; w != NULL; w = w->w_morechildren)
		for (i = 0; i < w->w_childcnt; i++)
			witness_leveldescendents(w->w_children[i], level);
}

static void
witness_displaydescendants(void(*prnt)(const char *fmt, ...),
			   struct witness *parent)
{
	struct witness *w;
	int i;
	int level;

	level = parent->w_spin ? ffs(parent->w_level) : parent->w_level;

	prnt("%d", level);
	if (level < 10)
		prnt(" ");
	for (i = 0; i < level; i++)
		prnt(" ");
	prnt("%s", parent->w_description);
	if (parent->w_file != NULL)
		prnt(" -- last acquired @ %s:%d\n", parent->w_file,
		    parent->w_line);

	for (w = parent; w != NULL; w = w->w_morechildren)
		for (i = 0; i < w->w_childcnt; i++)
			    witness_displaydescendants(prnt, w->w_children[i]);
    }

static int
dup_ok(struct witness *w)
{
	char **dup;
	
	for (dup = dup_list; *dup!= NULL; dup++)
		if (strcmp(w->w_description, *dup) == 0)
			return (1);
	return (0);
}

static int
blessed(struct witness *w1, struct witness *w2)
{
	int i;
	struct witness_blessed *b;

	for (i = 0; i < blessed_count; i++) {
		b = &blessed_list[i];
		if (strcmp(w1->w_description, b->b_lock1) == 0) {
			if (strcmp(w2->w_description, b->b_lock2) == 0)
				return (1);
			continue;
		}
		if (strcmp(w1->w_description, b->b_lock2) == 0)
			if (strcmp(w2->w_description, b->b_lock1) == 0)
				return (1);
	}
	return (0);
}

static struct witness *
witness_get()
{
	struct witness *w;

	if ((w = w_free) == NULL) {
		witness_dead = 1;
		mtx_unlock_spin_flags(&w_mtx, MTX_QUIET);
		printf("witness exhausted\n");
		return (NULL);
	}
	w_free = w->w_next;
	bzero(w, sizeof(*w));
	return (w);
}

static void
witness_free(struct witness *w)
{
	w->w_next = w_free;
	w_free = w;
}

int
witness_list(struct proc *p)
{
	struct mtx *m;
	int nheld;

	KASSERT(!witness_cold, ("%s: witness_cold\n", __FUNCTION__));
	nheld = 0;
	LIST_FOREACH(m, &p->p_heldmtx, mtx_held) {
		printf("\t\"%s\" (%p) locked at %s:%d\n",
		    m->mtx_description, m,
		    m->mtx_witness->w_file, m->mtx_witness->w_line);
		nheld++;
	}

	return (nheld);
}

#ifdef DDB

DB_SHOW_COMMAND(mutexes, db_witness_list)
{

	witness_list(curproc);
}

DB_SHOW_COMMAND(witness, db_witness_display)
{

	witness_display(db_printf);
}
#endif

void
witness_save(struct mtx *m, const char **filep, int *linep)
{

	KASSERT(!witness_cold, ("%s: witness_cold\n", __FUNCTION__));
	if (m->mtx_witness == NULL)
		return;

	*filep = m->mtx_witness->w_file;
	*linep = m->mtx_witness->w_line;
}

void
witness_restore(struct mtx *m, const char *file, int line)
{

	KASSERT(!witness_cold, ("%s: witness_cold\n", __FUNCTION__));
	if (m->mtx_witness == NULL)
		return;

	m->mtx_witness->w_file = file;
	m->mtx_witness->w_line = line;
}

#endif	/* WITNESS */
