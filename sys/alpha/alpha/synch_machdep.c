/*-
 * Copyright (c) 1997, 1998 Berkeley Software Design, Inc. All rights reserved.
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
 *	from BSDI $Id: synch_machdep.c,v 2.3.2.39 2000/04/27 03:10:25 cp Exp $
 * $FreeBSD$
 */

#define MTX_STRS		/* define common strings */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/mutex.h>

/* All mutii in system (used for debug/panic) */
mtx_t all_mtx = { MTX_UNOWNED, 0, 0, "All muti queue head",
	TAILQ_HEAD_INITIALIZER(all_mtx.mtx_blocked),
	{ NULL, NULL }, &all_mtx, &all_mtx
#ifdef SMP_DEBUG
	, NULL, { NULL, NULL }, NULL, 0
#endif
};

int	mtx_cur_cnt;
int	mtx_max_cnt;

extern void _mtx_enter_giant_def(void);
extern void _mtx_exit_giant_def(void);

static void propagate_priority(struct proc *) __unused;

#define	mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)
#define	mtx_owner(m)	(mtx_unowned(m) ? NULL \
			    : (struct proc *)((m)->mtx_lock & MTX_FLAGMASK))

#define RETIP(x)		*(((u_int64_t *)(&x)) - 1)
#define	SET_PRIO(p, pri)	(p)->p_priority = (pri)

/*
 * XXX Temporary, for use from assembly language
 */

void
_mtx_enter_giant_def(void)
{

	mtx_enter(&Giant, MTX_DEF);
}

void
_mtx_exit_giant_def(void)
{

	mtx_exit(&Giant, MTX_DEF);
}

static void
propagate_priority(struct proc *p)
{
	int pri = p->p_priority;
	mtx_t *m = p->p_blocked;

	for (;;) {
		struct proc *p1;

		p = mtx_owner(m);

		if (p == NULL) {
			/*
			 * This really isn't quite right. Really
			 * ought to bump priority of process that
			 * next axcquires the mutex.
			 */
			MPASS(m->mtx_lock == MTX_CONTESTED);
			return;
		}
		MPASS(p->p_magic == P_MAGIC);
		if (p->p_priority <= pri)
			return;
		/*
		 * If lock holder is actually running  just bump priority.
		 */
		if (TAILQ_NEXT(p, p_procq) == NULL) {
			SET_PRIO(p, pri);
			return;
		}
		/*
		 * If on run queue move to new run queue, and
		 * quit. Otherwise pick up mutex p is blocked on
		 */
		if ((m = p->p_blocked) == NULL) {
			remrunqueue(p);
			SET_PRIO(p, pri);
			setrunqueue(p);
			return;
		}
		/*
		 * Check if the proc needs to be moved up on
		 * the blocked chain
		 */
		if ((p1 = TAILQ_PREV(p, rq, p_procq)) == NULL ||
		    p1->p_priority <= pri)
			continue;

		/*
		 * Remove proc from blocked chain
		 */
		TAILQ_REMOVE(&m->mtx_blocked, p, p_procq);
		TAILQ_FOREACH(p1, &m->mtx_blocked, p_procq) {
			MPASS(p1->p_magic == P_MAGIC);
			if (p1->p_priority > pri)
				break;
		}
		if (p1)
			TAILQ_INSERT_BEFORE(p1, p, p_procq);
		else
			TAILQ_INSERT_TAIL(&m->mtx_blocked, p, p_procq);
		CTR4(KTR_LOCK,
		    "propagate priority: p %p moved before %p on [%p] %s",
		    p, p1, m, m->mtx_description);
	}
}

void
mtx_enter_hard(mtx_t *m, int type, int ipl)
{
	struct proc *p = CURPROC;

	switch (type) {
	case MTX_DEF:
		if ((m->mtx_lock & MTX_FLAGMASK) == (u_int64_t)p) {
			m->mtx_recurse++;
			atomic_set_64(&m->mtx_lock, MTX_RECURSE);
			CTR1(KTR_LOCK, "mtx_enter: %p recurse", m);
			return;
		}
		CTR3(KTR_LOCK, "mtx_enter: %p contested (lock=%lx) [0x%lx]",
		    m, m->mtx_lock, RETIP(m));
		while (!atomic_cmpset_64(&m->mtx_lock, MTX_UNOWNED,
					 (u_int64_t)p)) {
			int v;
			struct timeval tv;
			struct proc *p1;

			mtx_enter(&sched_lock, MTX_SPIN | MTX_RLIKELY);
			/*
			 * check if the lock has been released while
			 * waiting for the schedlock.
			 */
			if ((v = m->mtx_lock) == MTX_UNOWNED) {
				mtx_exit(&sched_lock, MTX_SPIN);
				continue;
			}
			/*
			 * The mutex was marked contested on release. This
			 * means that there are processes blocked on it.
			 */
			if (v == MTX_CONTESTED) {
				p1 = TAILQ_FIRST(&m->mtx_blocked);
				m->mtx_lock = (u_int64_t)p | MTX_CONTESTED;
				if (p1->p_priority < p->p_priority) {
					SET_PRIO(p, p1->p_priority);
				}
				mtx_exit(&sched_lock, MTX_SPIN);
				return;
			}
			/*
			 * If the mutex isn't already contested and
			 * a failure occurs setting the contested bit the
			 * mutex was either release or the
			 * state of the RECURSION bit changed.
			 */
			if ((v & MTX_CONTESTED) == 0 &&
			    !atomic_cmpset_64(&m->mtx_lock, v,
				               v | MTX_CONTESTED)) {
				mtx_exit(&sched_lock, MTX_SPIN);
				continue;
			}

			/* We definitely have to sleep for this lock */
			mtx_assert(m, MA_NOTOWNED);

			printf("m->mtx_lock=%lx\n", m->mtx_lock);

#ifdef notyet
			/*
			 * If we're borrowing an interrupted thread's VM
			 * context must clean up before going to sleep.
			 */
			if (p->p_flag & (P_ITHD | P_SITHD)) {
				ithd_t *it = (ithd_t *)p;

				if (it->it_interrupted) {
					CTR2(KTR_LOCK,
					    "mtx_enter: 0x%x interrupted 0x%x",
					    it, it->it_interrupted);
					intr_thd_fixup(it);
				}
			}
#endif

			/* Put us on the list of procs blocked on this mutex */
			if (TAILQ_EMPTY(&m->mtx_blocked)) {
				p1 = (struct proc *)(m->mtx_lock &
						     MTX_FLAGMASK);
				LIST_INSERT_HEAD(&p1->p_contested, m,
						 mtx_contested);
				TAILQ_INSERT_TAIL(&m->mtx_blocked, p, p_procq);
			} else {
				TAILQ_FOREACH(p1, &m->mtx_blocked, p_procq)
					if (p1->p_priority > p->p_priority)
						break;
				if (p1)
					TAILQ_INSERT_BEFORE(p1, p, p_procq);
				else
					TAILQ_INSERT_TAIL(&m->mtx_blocked, p,
							  p_procq);
			}

			p->p_blocked = m;	/* Who we're blocked on */
#ifdef notyet
			propagate_priority(p);
#endif
			CTR3(KTR_LOCK, "mtx_enter: p %p blocked on [%p] %s",
			    p, m, m->mtx_description);
			/*
			 * cloaned from mi_switch
			 */
			microtime(&tv);
			p->p_runtime += (tv.tv_usec -
					 PCPU_GET(switchtime.tv_usec)) +
					(tv.tv_sec -
					 PCPU_GET(switchtime.tv_sec)) *
					(int64_t)1000000;
			PCPU_SET(switchtime.tv_usec, tv.tv_usec);
			PCPU_SET(switchtime.tv_sec, tv.tv_sec);
			cpu_switch();
			if (PCPU_GET(switchtime.tv_sec) == 0)
				microtime(&GLOBALP->gd_switchtime);
			PCPU_SET(switchticks, ticks);
			CTR3(KTR_LOCK,
			    "mtx_enter: p %p free from blocked on [%p] %s",
			    p, m, m->mtx_description);
			mtx_exit(&sched_lock, MTX_SPIN);
		}
		alpha_mb();
		return;
	case MTX_SPIN:
	case MTX_SPIN | MTX_FIRST:
	case MTX_SPIN | MTX_TOPHALF:
	    {
		int i = 0;

		if (m->mtx_lock == (u_int64_t)p) {
			m->mtx_recurse++;
			return;
		}
		CTR1(KTR_LOCK, "mtx_enter: %p spinning", m);
		for (;;) {
			if (atomic_cmpset_64(&m->mtx_lock, MTX_UNOWNED,
			    (u_int64_t)p)) {
				alpha_mb();
				break;
			}
			while (m->mtx_lock != MTX_UNOWNED) {
				if (i++ < 1000000)
					continue;
				if (i++ < 6000000)
					DELAY (1);
				else
					panic("spin lock > 5 seconds");
			}
		}
			
#ifdef SMP_DEBUG
		if (type != MTX_SPIN)
			m->mtx_saveipl = 0xbeefface;
		else
#endif
			m->mtx_saveipl = ipl;
		CTR1(KTR_LOCK, "mtx_enter: %p spin done", m);
		return;
	    }
	}
}

void
mtx_exit_hard(mtx_t *m, int type)
{
	struct proc *p, *p1;
	mtx_t *m1;
	int pri;

	switch (type) {
	case MTX_DEF:
	case MTX_DEF | MTX_NOSWITCH:
		if (m->mtx_recurse != 0) {
			if (--(m->mtx_recurse) == 0)
				atomic_clear_64(&m->mtx_lock, MTX_RECURSE);
			CTR1(KTR_LOCK, "mtx_exit: %p unrecurse", m);
			return;
		}
		mtx_enter(&sched_lock, MTX_SPIN);
		CTR1(KTR_LOCK, "mtx_exit: %p contested", m);
		p = CURPROC;
		p1 = TAILQ_FIRST(&m->mtx_blocked);
		MPASS(p->p_magic == P_MAGIC);
		MPASS(p1->p_magic == P_MAGIC);
		TAILQ_REMOVE(&m->mtx_blocked, p1, p_procq);
		if (TAILQ_EMPTY(&m->mtx_blocked)) {
			LIST_REMOVE(m, mtx_contested);
			atomic_cmpset_64(&m->mtx_lock, m->mtx_lock,
					  MTX_UNOWNED);
			CTR1(KTR_LOCK, "mtx_exit: %p not held", m);
		} else
			m->mtx_lock = MTX_CONTESTED;
		pri = MAXPRI;
		LIST_FOREACH(m1, &p->p_contested, mtx_contested) {
			int cp = TAILQ_FIRST(&m1->mtx_blocked)->p_priority;
			if (cp < pri)
				pri = cp;
		}
		if (pri > p->p_nativepri)
			pri = p->p_nativepri;
		SET_PRIO(p, pri);
		CTR2(KTR_LOCK, "mtx_exit: %p contested setrunqueue %p",
		    m, p1);
		p1->p_blocked = NULL;
		setrunqueue(p1);
		if ((type & MTX_NOSWITCH) == 0 && p1->p_priority < pri) {
#ifdef notyet
			if (p->p_flag & (P_ITHD | P_SITHD)) {
				ithd_t *it = (ithd_t *)p;

				if (it->it_interrupted) {
					CTR2(KTR_LOCK,
					    "mtx_exit: 0x%x interruped 0x%x",
					    it, it->it_interrupted);
					intr_thd_fixup(it);
				}
			}
#endif
			setrunqueue(p);
			CTR2(KTR_LOCK, "mtx_exit: %p switching out lock=0x%lx",
			    m, m->mtx_lock);
			cpu_switch();
			CTR2(KTR_LOCK, "mtx_exit: %p resuming lock=0x%lx",
			    m, m->mtx_lock);
		}
		mtx_exit(&sched_lock, MTX_SPIN);
		return;
	case MTX_SPIN:
	case MTX_SPIN | MTX_FIRST:
		if (m->mtx_recurse != 0) {
			m->mtx_recurse--;
			return;
		}
		alpha_mb();
		if (atomic_cmpset_64(&m->mtx_lock, CURTHD, MTX_UNOWNED)) {
			MPASS(m->mtx_saveipl != 0xbeefface);
			alpha_pal_swpipl(m->mtx_saveipl);
			return;
		}
		panic("unsucuessful release of spin lock");
	case MTX_SPIN | MTX_TOPHALF:
		if (m->mtx_recurse != 0) {
			m->mtx_recurse--;
			return;
		}
		alpha_mb();
		if (atomic_cmpset_64(&m->mtx_lock, CURTHD, MTX_UNOWNED))
			return;
		panic("unsucuessful release of spin lock");
	default:
		panic("mtx_exit_hard: unsupported type 0x%x\n", type);
	}
}

#define MV_DESTROY	0	/* validate before destory */
#define MV_INIT		1	/* validate before init */

#ifdef SMP_DEBUG

int mtx_validate __P((mtx_t *, int));

int
mtx_validate(mtx_t *m, int when)
{
	mtx_t *mp;
	int i;
	int retval = 0;

	if (m == &all_mtx || cold)
		return 0;

	mtx_enter(&all_mtx, MTX_DEF);
	ASS(kernacc((caddr_t)all_mtx.mtx_next, 4, 1) == 1);
	ASS(all_mtx.mtx_next->mtx_prev == &all_mtx);
	for (i = 0, mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next) {
		if (kernacc((caddr_t)mp->mtx_next, 4, 1) != 1) {
			panic("mtx_validate: mp=%p mp->mtx_next=%p",
			    mp, mp->mtx_next);
		}
		i++;
		if (i > mtx_cur_cnt) {
			panic("mtx_validate: too many in chain, known=%d\n",
			    mtx_cur_cnt);
		}
	}
	ASS(i == mtx_cur_cnt); 
	switch (when) {
	case MV_DESTROY:
		for (mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next)
			if (mp == m)
				break;
		ASS(mp == m);
		break;
	case MV_INIT:
		for (mp = all_mtx.mtx_next; mp != &all_mtx; mp = mp->mtx_next)
		if (mp == m) {
			/*
			 * Not good. This mutex already exits
			 */
			retval = 1;
#if 1
			printf("re-initing existing mutex %s\n",
			    m->mtx_description);
			ASS(m->mtx_lock == MTX_UNOWNED);
			retval = 1;
#else
			panic("re-initing existing mutex %s",
			    m->mtx_description);
#endif
		}
	}
	mtx_exit(&all_mtx, MTX_DEF);
	return (retval);
}
#endif

void
mtx_init(mtx_t *m, char *t, int flag)
{

	CTR2(KTR_LOCK, "mtx_init %p (%s)", m, t);
#ifdef SMP_DEBUG
	if (mtx_validate(m, MV_INIT))	/* diagnostic and error correction */
		return;
#endif
	bzero((void *)m, sizeof *m);
	TAILQ_INIT(&m->mtx_blocked);
	m->mtx_description = t;
	m->mtx_lock = MTX_UNOWNED;
	/* Put on all mutex queue */
	mtx_enter(&all_mtx, MTX_DEF);
	m->mtx_next = &all_mtx;
	m->mtx_prev = all_mtx.mtx_prev;
	m->mtx_prev->mtx_next = m;
	all_mtx.mtx_prev = m;
	if (++mtx_cur_cnt > mtx_max_cnt)
		mtx_max_cnt = mtx_cur_cnt;
	mtx_exit(&all_mtx, MTX_DEF);
	witness_init(m, flag);
}

void
mtx_destroy(mtx_t *m)
{

	CTR2(KTR_LOCK, "mtx_destroy %p (%s)", m, m->mtx_description);
#ifdef SMP_DEBUG
	if (m->mtx_next == NULL)
		panic("mtx_destroy: %p (%s) already destroyed",
		    m, m->mtx_description);

	if (!mtx_owned(m)) {
		ASS(m->mtx_lock == MTX_UNOWNED);
	} else {
		ASS((m->mtx_lock & (MTX_RECURSE|MTX_CONTESTED)) == 0);
	}
	mtx_validate(m, MV_DESTROY);		/* diagnostic */
#endif

#ifdef WITNESS
	if (m->mtx_witness)
		witness_destroy(m);
#endif /* WITNESS */

	/* Remove from the all mutex queue */
	mtx_enter(&all_mtx, MTX_DEF);
	m->mtx_next->mtx_prev = m->mtx_prev;
	m->mtx_prev->mtx_next = m->mtx_next;
#ifdef SMP_DEBUG
	m->mtx_next = m->mtx_prev = NULL;
#endif
	mtx_cur_cnt--;
	mtx_exit(&all_mtx, MTX_DEF);
}
