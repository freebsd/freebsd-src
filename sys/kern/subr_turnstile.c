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

#include <vm/vm.h>
#include <vm/vm_extern.h>

#define _KERN_MUTEX_C_		/* Cause non-inlined mtx_*() to be compiled. */
#include <sys/mutex.h>

/*
 * Machine independent bits of the mutex implementation
 */
/* All mutexes in system (used for debug/panic) */
#ifdef MUTEX_DEBUG
static struct mtx_debug all_mtx_debug = { NULL, {NULL, NULL}, NULL, 0,
	"All mutexes queue head" };
static struct mtx all_mtx = { MTX_UNOWNED, 0, 0, &all_mtx_debug,
	TAILQ_HEAD_INITIALIZER(all_mtx.mtx_blocked),
	{ NULL, NULL }, &all_mtx, &all_mtx };
#else	/* MUTEX_DEBUG */
static struct mtx all_mtx = { MTX_UNOWNED, 0, 0, "All mutexes queue head",
	TAILQ_HEAD_INITIALIZER(all_mtx.mtx_blocked),
	{ NULL, NULL }, &all_mtx, &all_mtx };
#endif	/* MUTEX_DEBUG */

static int	mtx_cur_cnt;
static int	mtx_max_cnt;

void	_mtx_enter_giant_def(void);
void	_mtx_exit_giant_def(void);
static void propagate_priority(struct proc *) __unused;

#define	mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)
#define	mtx_owner(m)	(mtx_unowned(m) ? NULL \
			    : (struct proc *)((m)->mtx_lock & MTX_FLAGMASK))

#define RETIP(x)		*(((uintptr_t *)(&x)) - 1)
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
	struct mtx *m = p->p_blocked;

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
		if (p->p_priority <= pri)
			return;
		/*
		 * If lock holder is actually running, just bump priority.
		 */
		if (TAILQ_NEXT(p, p_procq) == NULL) {
			MPASS(p->p_stat == SRUN || p->p_stat == SZOMB);
			SET_PRIO(p, pri);
			return;
		}
		/*
		 * If on run queue move to new run queue, and
		 * quit.
		 */
		if (p->p_stat == SRUN) {
			MPASS(p->p_blocked == NULL);
			remrunqueue(p);
			SET_PRIO(p, pri);
			setrunqueue(p);
			return;
		}

		/*
		 * If we aren't blocked on a mutex, give up and quit.
		 */
		if (p->p_stat != SMTX) {
			printf(
	"XXX: process %d(%s):%d holds %s but isn't blocked on a mutex\n",
			    p->p_pid, p->p_comm, p->p_stat, m->mtx_description);
			return;
		}

		/*
		 * Pick up the mutex that p is blocked on.
		 */
		m = p->p_blocked;
		MPASS(m != NULL);

		printf("XXX: process %d(%s) is blocked on %s\n", p->p_pid,
		    p->p_comm, m->mtx_description);
		/*
		 * Check if the proc needs to be moved up on
		 * the blocked chain
		 */
		if ((p1 = TAILQ_PREV(p, rq, p_procq)) == NULL ||
		    p1->p_priority <= pri) {
			if (p1)
				printf(
	"XXX: previous process %d(%s) has higher priority\n",
				    p->p_pid, p->p_comm);
			else
				printf("XXX: process at head of run queue\n");
			continue;
		}

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
		    "propagate priority: p 0x%p moved before 0x%p on [0x%p] %s",
		    p, p1, m, m->mtx_description);
	}
}

void
mtx_enter_hard(struct mtx *m, int type, int saveintr)
{
	struct proc *p = CURPROC;
	struct timeval new_switchtime;

	KASSERT(p != NULL, ("curproc is NULL in mutex"));

	switch (type) {
	case MTX_DEF:
		if ((m->mtx_lock & MTX_FLAGMASK) == (uintptr_t)p) {
			m->mtx_recurse++;
			atomic_set_ptr(&m->mtx_lock, MTX_RECURSE);
			CTR1(KTR_LOCK, "mtx_enter: 0x%p recurse", m);
			return;
		}
		CTR3(KTR_LOCK, "mtx_enter: 0x%p contested (lock=%p) [0x%p]",
		    m, (void *)m->mtx_lock, (void *)RETIP(m));
		while (!_obtain_lock(m, p)) {
			uintptr_t v;
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
				KASSERT(p1 != NULL, ("contested mutex has no contesters"));
				KASSERT(p != NULL, ("curproc is NULL for contested mutex"));
				m->mtx_lock = (uintptr_t)p | MTX_CONTESTED;
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
			    !atomic_cmpset_ptr(&m->mtx_lock, (void *)v,
				               (void *)(v | MTX_CONTESTED))) {
				mtx_exit(&sched_lock, MTX_SPIN);
				continue;
			}

			/* We definitely have to sleep for this lock */
			mtx_assert(m, MA_NOTOWNED);

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
			p->p_stat = SMTX;
#if 0
			propagate_priority(p);
#endif
			CTR3(KTR_LOCK, "mtx_enter: p 0x%p blocked on [0x%p] %s",
			    p, m, m->mtx_description);
			/*
			 * Blatantly copied from mi_switch nearly verbatim.
			 * When Giant goes away and we stop dinking with it
			 * in mi_switch, we can go back to calling mi_switch
			 * directly here.
			 */
	
			/*
			 * Compute the amount of time during which the current
			 * process was running, and add that to its total so
			 * far.
			 */
			microuptime(&new_switchtime);
			if (timevalcmp(&new_switchtime, &switchtime, <)) {
				printf(
		    "microuptime() went backwards (%ld.%06ld -> %ld.%06ld)\n",
		    		    switchtime.tv_sec, switchtime.tv_usec,
		    		    new_switchtime.tv_sec,
		    		    new_switchtime.tv_usec);
				new_switchtime = switchtime;
			} else {
				p->p_runtime += (new_switchtime.tv_usec -
				    switchtime.tv_usec) +
				    (new_switchtime.tv_sec - switchtime.tv_sec) *
				    (int64_t)1000000;
			}

			/*
			 * Pick a new current process and record its start time.
			 */
			cnt.v_swtch++;
			switchtime = new_switchtime;
			cpu_switch();
			if (switchtime.tv_sec == 0)
				microuptime(&switchtime);
			switchticks = ticks;
			CTR3(KTR_LOCK,
			    "mtx_enter: p 0x%p free from blocked on [0x%p] %s",
			    p, m, m->mtx_description);
			mtx_exit(&sched_lock, MTX_SPIN);
		}
		return;
	case MTX_SPIN:
	case MTX_SPIN | MTX_FIRST:
	case MTX_SPIN | MTX_TOPHALF:
	    {
		int i = 0;

		if (m->mtx_lock == (uintptr_t)p) {
			m->mtx_recurse++;
			return;
		}
		CTR1(KTR_LOCK, "mtx_enter: %p spinning", m);
		for (;;) {
			if (_obtain_lock(m, p))
				break;
			while (m->mtx_lock != MTX_UNOWNED) {
				if (i++ < 1000000)
					continue;
				if (i++ < 6000000)
					DELAY (1);
#ifdef DDB
				else if (!db_active)
#else
				else
#endif
					panic(
				"spin lock %s held by 0x%p for > 5 seconds",
					    m->mtx_description,
					    (void *)m->mtx_lock);
			}
		}
			
#ifdef MUTEX_DEBUG
		if (type != MTX_SPIN)
			m->mtx_saveintr = 0xbeefface;
		else
#endif
			m->mtx_saveintr = saveintr;
		CTR1(KTR_LOCK, "mtx_enter: 0x%p spin done", m);
		return;
	    }
	}
}

void
mtx_exit_hard(struct mtx *m, int type)
{
	struct proc *p, *p1;
	struct mtx *m1;
	int pri;

	p = CURPROC;
	switch (type) {
	case MTX_DEF:
	case MTX_DEF | MTX_NOSWITCH:
		if (m->mtx_recurse != 0) {
			if (--(m->mtx_recurse) == 0)
				atomic_clear_ptr(&m->mtx_lock, MTX_RECURSE);
			CTR1(KTR_LOCK, "mtx_exit: 0x%p unrecurse", m);
			return;
		}
		mtx_enter(&sched_lock, MTX_SPIN);
		CTR1(KTR_LOCK, "mtx_exit: 0x%p contested", m);
		p1 = TAILQ_FIRST(&m->mtx_blocked);
		MPASS(p->p_magic == P_MAGIC);
		MPASS(p1->p_magic == P_MAGIC);
		TAILQ_REMOVE(&m->mtx_blocked, p1, p_procq);
		if (TAILQ_EMPTY(&m->mtx_blocked)) {
			LIST_REMOVE(m, mtx_contested);
			_release_lock_quick(m);
			CTR1(KTR_LOCK, "mtx_exit: 0x%p not held", m);
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
		CTR2(KTR_LOCK, "mtx_exit: 0x%p contested setrunqueue 0x%p",
		    m, p1);
		p1->p_blocked = NULL;
		p1->p_stat = SRUN;
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
			CTR2(KTR_LOCK, "mtx_exit: 0x%p switching out lock=0x%p",
			    m, (void *)m->mtx_lock);
			mi_switch();
			CTR2(KTR_LOCK, "mtx_exit: 0x%p resuming lock=0x%p",
			    m, (void *)m->mtx_lock);
		}
		mtx_exit(&sched_lock, MTX_SPIN);
		break;
	case MTX_SPIN:
	case MTX_SPIN | MTX_FIRST:
		if (m->mtx_recurse != 0) {
			m->mtx_recurse--;
			return;
		}
		MPASS(mtx_owned(m));
		_release_lock_quick(m);
		if (type & MTX_FIRST)
			enable_intr();	/* XXX is this kosher? */
		else {
			MPASS(m->mtx_saveintr != 0xbeefface);
			restore_intr(m->mtx_saveintr);
		}
		break;
	case MTX_SPIN | MTX_TOPHALF:
		if (m->mtx_recurse != 0) {
			m->mtx_recurse--;
			return;
		}
		MPASS(mtx_owned(m));
		_release_lock_quick(m);
		break;
	default:
		panic("mtx_exit_hard: unsupported type 0x%x\n", type);
	}
}

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

	if (m == &all_mtx || cold)
		return 0;

	mtx_enter(&all_mtx, MTX_DEF);
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
	mtx_exit(&all_mtx, MTX_DEF);
	return (retval);
}
#endif

void
mtx_init(struct mtx *m, const char *t, int flag)
{
#ifdef MUTEX_DEBUG
	struct mtx_debug *debug;
#endif

	CTR2(KTR_LOCK, "mtx_init 0x%p (%s)", m, t);
#ifdef MUTEX_DEBUG
	if (mtx_validate(m, MV_INIT))	/* diagnostic and error correction */
		return;
	if (flag & MTX_COLD)
		debug = m->mtx_debug;
	else
		debug = NULL;
	if (debug == NULL) {
#ifdef DIAGNOSTIC
		if(cold && bootverbose)
			printf("malloc'ing mtx_debug while cold for %s\n", t);
#endif

		/* XXX - should not use DEVBUF */
		debug = malloc(sizeof(struct mtx_debug), M_DEVBUF, M_NOWAIT);
		MPASS(debug != NULL);
		bzero(debug, sizeof(struct mtx_debug));
	}
#endif
	bzero((void *)m, sizeof *m);
	TAILQ_INIT(&m->mtx_blocked);
#ifdef MUTEX_DEBUG
	m->mtx_debug = debug;
#endif
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
mtx_destroy(struct mtx *m)
{

	CTR2(KTR_LOCK, "mtx_destroy 0x%p (%s)", m, m->mtx_description);
#ifdef MUTEX_DEBUG
	if (m->mtx_next == NULL)
		panic("mtx_destroy: %p (%s) already destroyed",
		    m, m->mtx_description);

	if (!mtx_owned(m)) {
		MPASS(m->mtx_lock == MTX_UNOWNED);
	} else {
		MPASS((m->mtx_lock & (MTX_RECURSE|MTX_CONTESTED)) == 0);
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
#ifdef MUTEX_DEBUG
	m->mtx_next = m->mtx_prev = NULL;
	free(m->mtx_debug, M_DEVBUF);
	m->mtx_debug = NULL;
#endif
	mtx_cur_cnt--;
	mtx_exit(&all_mtx, MTX_DEF);
}

/*
 * The non-inlined versions of the mtx_*() functions are always built (above),
 * but the witness code depends on the MUTEX_DEBUG and WITNESS kernel options
 * being specified.
 */
#if (defined(MUTEX_DEBUG) && defined(WITNESS))

#define WITNESS_COUNT 200
#define	WITNESS_NCHILDREN 2

#ifndef SMP
extern int witness_spin_check;
#endif

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
	u_char		 w_sleep:1;
	u_char		 w_spin:1;	/* this is a spin mutex */
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
#ifdef WITNESS_DDB
int	witness_ddb = 1;
#else
int	witness_ddb = 0;
#endif
SYSCTL_INT(_debug, OID_AUTO, witness_ddb, CTLFLAG_RW, &witness_ddb, 0, "");
#endif /* DDB */

#ifdef WITNESS_SKIPSPIN
int	witness_skipspin = 1;
#else
int	witness_skipspin = 0;
#endif
SYSCTL_INT(_debug, OID_AUTO, witness_skipspin, CTLFLAG_RD, &witness_skipspin, 0,
    "");

MUTEX_DECLARE(static,w_mtx);
static struct witness	*w_free;
static struct witness	*w_all;
static int		 w_inited;
static int		 witness_dead;	/* fatal error, probably no memory */

static struct witness	 w_data[WITNESS_COUNT];

static struct witness	 *enroll __P((const char *description, int flag));
static int itismychild __P((struct witness *parent, struct witness *child));
static void removechild __P((struct witness *parent, struct witness *child));
static int isitmychild __P((struct witness *parent, struct witness *child));
static int isitmydescendant __P((struct witness *parent, struct witness *child));
static int dup_ok __P((struct witness *));
static int blessed __P((struct witness *, struct witness *));
static void witness_displaydescendants
    __P((void(*)(const char *fmt, ...), struct witness *));
static void witness_leveldescendents __P((struct witness *parent, int level));
static void witness_levelall __P((void));
static struct witness * witness_get __P((void));
static void witness_free __P((struct witness *m));


static char *ignore_list[] = {
	"witness lock",
	NULL
};

static char *spin_order_list[] = {
	"sched lock",
	"clk",
	"sio",
	/*
	 * leaf locks
	 */
	NULL
};

static char *order_list[] = {
	NULL
};

static char *dup_list[] = {
	NULL
};

static char *sleep_list[] = {
	"Giant lock",
	NULL
};

/*
 * Pairs of locks which have been blessed
 * Don't complain about order problems with blessed locks
 */
static struct witness_blessed blessed_list[] = {
};
static int blessed_count = sizeof(blessed_list) / sizeof(struct witness_blessed);

void
witness_init(struct mtx *m, int flag)
{
	m->mtx_witness = enroll(m->mtx_description, flag);
}

void
witness_destroy(struct mtx *m)
{
	struct mtx *m1;
	struct proc *p;
	p = CURPROC;
	for ((m1 = LIST_FIRST(&p->p_heldmtx)); m1 != NULL;
		m1 = LIST_NEXT(m1, mtx_held)) {
		if (m1 == m) {
			LIST_REMOVE(m, mtx_held);
			break;
		}
	}
	return;

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

	w = m->mtx_witness;
	p = CURPROC;

	if (flags & MTX_SPIN) {
		if (!w->w_spin)
			panic("mutex_enter: MTX_SPIN on MTX_DEF mutex %s @"
			    " %s:%d", m->mtx_description, file, line);
		if (m->mtx_recurse != 0)
			return;
		mtx_enter(&w_mtx, MTX_SPIN);
		i = witness_spin_check;
		if (i != 0 && w->w_level < i) {
			mtx_exit(&w_mtx, MTX_SPIN);
			panic("mutex_enter(%s:%x, MTX_SPIN) out of order @"
			    " %s:%d already holding %s:%x",
			    m->mtx_description, w->w_level, file, line,
			    spin_order_list[ffs(i)-1], i);
		}
		PCPU_SET(witness_spin_check, i | w->w_level);
		mtx_exit(&w_mtx, MTX_SPIN);
		return;
	}
	if (w->w_spin)
		panic("mutex_enter: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (m->mtx_recurse != 0)
		return;
	if (witness_dead)
		goto out;
	if (cold)
		goto out;

	if (!mtx_legal2block())
		panic("blockable mtx_enter() of %s when not legal @ %s:%d",
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
	mtx_enter(&w_mtx, MTX_SPIN);
	/*
	 * If we have a known higher number just say ok
	 */
	if (witness_watch > 1 && w->w_level > w1->w_level) {
		mtx_exit(&w_mtx, MTX_SPIN);
		goto out;
	}
	if (isitmydescendant(m1->mtx_witness, w)) {
		mtx_exit(&w_mtx, MTX_SPIN);
		goto out;
	}
	for (i = 0; m1 != NULL; m1 = LIST_NEXT(m1, mtx_held), i++) {

		MPASS(i < 200);
		w1 = m1->mtx_witness;
		if (isitmydescendant(w, w1)) {
			mtx_exit(&w_mtx, MTX_SPIN);
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
		mtx_exit(&w_mtx, MTX_SPIN);

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
	 * If this pays off it likely means that a mutex  being witnessed
	 * is acquired in hardclock. Put it in the ignore list. It is
	 * likely not the mutex this assert fails on.
	 */
	MPASS(m->mtx_held.le_prev == NULL);
	LIST_INSERT_HEAD(&p->p_heldmtx, (struct mtx*)m, mtx_held);
}

void
witness_exit(struct mtx *m, int flags, const char *file, int line)
{
	struct witness *w;

	w = m->mtx_witness;

	if (flags & MTX_SPIN) {
		if (!w->w_spin)
			panic("mutex_exit: MTX_SPIN on MTX_DEF mutex %s @"
			    " %s:%d", m->mtx_description, file, line);
		if (m->mtx_recurse != 0)
			return;
		mtx_enter(&w_mtx, MTX_SPIN);
		PCPU_SET(witness_spin_check, witness_spin_check & ~w->w_level);
		mtx_exit(&w_mtx, MTX_SPIN);
		return;
	}
	if (w->w_spin)
		panic("mutex_exit: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (m->mtx_recurse != 0)
		return;

	if ((flags & MTX_NOSWITCH) == 0 && !mtx_legal2block() && !cold)
		panic("switchable mtx_exit() of %s when not legal @ %s:%d",
			    m->mtx_description, file, line);
	LIST_REMOVE(m, mtx_held);
	m->mtx_held.le_prev = NULL;
}

void
witness_try_enter(struct mtx *m, int flags, const char *file, int line)
{
	struct proc *p;
	struct witness *w = m->mtx_witness;

	if (flags & MTX_SPIN) {
		if (!w->w_spin)
			panic("mutex_try_enter: "
			    "MTX_SPIN on MTX_DEF mutex %s @ %s:%d",
			    m->mtx_description, file, line);
		if (m->mtx_recurse != 0)
			return;
		mtx_enter(&w_mtx, MTX_SPIN);
		PCPU_SET(witness_spin_check, witness_spin_check | w->w_level);
		mtx_exit(&w_mtx, MTX_SPIN);
		return;
	}

	if (w->w_spin)
		panic("mutex_try_enter: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (m->mtx_recurse != 0)
		return;

	w->w_file = file;
	w->w_line = line;
	m->mtx_line = line;
	m->mtx_file = file;
	p = CURPROC;
	MPASS(m->mtx_held.le_prev == NULL);
	LIST_INSERT_HEAD(&p->p_heldmtx, (struct mtx*)m, mtx_held);
}

void
witness_display(void(*prnt)(const char *fmt, ...))
{
	struct witness *w, *w1;

	witness_levelall();

	for (w = w_all; w; w = w->w_next) {
		if (w->w_file == NULL)
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
	prnt("\nMutex which were never acquired\n");
	for (w = w_all; w; w = w->w_next) {
		if (w->w_file != NULL)
			continue;
		prnt("%s\n", w->w_description);
	}
}

int
witness_sleep(int check_only, struct mtx *mtx, const char *file, int line)
{
	struct mtx *m;
	struct proc *p;
	char **sleep;
	int n = 0;

	p = CURPROC;
	for ((m = LIST_FIRST(&p->p_heldmtx)); m != NULL;
	    m = LIST_NEXT(m, mtx_held)) {
		if (m == mtx)
			continue;
		for (sleep = sleep_list; *sleep!= NULL; sleep++)
			if (strcmp(m->mtx_description, *sleep) == 0)
				goto next;
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
		mtx_init(&w_mtx, "witness lock", MTX_COLD | MTX_DEF);
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
	mtx_enter(&w_mtx, MTX_SPIN);
	for (w = w_all; w; w = w->w_next) {
		if (strcmp(description, w->w_description) == 0) {
			mtx_exit(&w_mtx, MTX_SPIN);
			return (w);
		}
	}
	if ((w = witness_get()) == NULL)
		return (NULL);
	w->w_next = w_all;
	w_all = w;
	w->w_description = description;
	mtx_exit(&w_mtx, MTX_SPIN);
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
		if (!w->w_spin)
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
	int level = parent->w_level;

	prnt("%d", level);
	if (level < 10)
		prnt(" ");
	for (i = 0; i < level; i++)
		prnt(" ");
	prnt("%s", parent->w_description);
	if (parent->w_file != NULL) {
		prnt(" -- last acquired @ %s", parent->w_file);
#ifndef W_USE_WHERE
		prnt(":%d", parent->w_line);
#endif
		prnt("\n");
	}

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
		mtx_exit(&w_mtx, MTX_SPIN);
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

void
witness_list(struct proc *p)
{
	struct mtx *m;

	for ((m = LIST_FIRST(&p->p_heldmtx)); m != NULL;
	    m = LIST_NEXT(m, mtx_held)) {
		printf("\t\"%s\" (%p) locked at %s:%d\n",
		    m->mtx_description, m,
		    m->mtx_witness->w_file, m->mtx_witness->w_line);
	}
}

void
witness_save(struct mtx *m, const char **filep, int *linep)
{
	*filep = m->mtx_witness->w_file;
	*linep = m->mtx_witness->w_line;
}

void
witness_restore(struct mtx *m, const char *file, int line)
{
	m->mtx_witness->w_file = file;
	m->mtx_witness->w_line = line;
}

#endif	/* (defined(MUTEX_DEBUG) && defined(WITNESS)) */
