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
 * Machine independent bits of mutex implementation.
 */

#include "opt_adaptive_mutexes.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sbuf.h>
#include <sys/stdint.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

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
	: (struct thread *)((m)->mtx_lock & MTX_FLAGMASK))

/* XXXKSE This test will change. */
#define	thread_running(td)						\
	((td)->td_kse != NULL && (td)->td_kse->ke_oncpu != NOCPU)
    
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
 * System-wide mutexes
 */
struct mtx sched_lock;
struct mtx Giant;

/*
 * Prototypes for non-exported routines.
 */
static void	propagate_priority(struct thread *);

static void
propagate_priority(struct thread *td)
{
	int pri = td->td_priority;
	struct mtx *m = td->td_blocked;

	mtx_assert(&sched_lock, MA_OWNED);
	for (;;) {
		struct thread *td1;

		td = mtx_owner(m);

		if (td == NULL) {
			/*
			 * This really isn't quite right. Really
			 * ought to bump priority of thread that
			 * next acquires the mutex.
			 */
			MPASS(m->mtx_lock == MTX_CONTESTED);
			return;
		}

		MPASS(td->td_proc != NULL);
		MPASS(td->td_proc->p_magic == P_MAGIC);
		KASSERT(!TD_IS_SLEEPING(td), ("sleeping thread owns a mutex"));
		if (td->td_priority <= pri) /* lower is higher priority */
			return;


		/*
		 * If lock holder is actually running, just bump priority.
		 */
		if (TD_IS_RUNNING(td)) {
			td->td_priority = pri;
			return;
		}

#ifndef SMP
		/*
		 * For UP, we check to see if td is curthread (this shouldn't
		 * ever happen however as it would mean we are in a deadlock.)
		 */
		KASSERT(td != curthread, ("Deadlock detected"));
#endif

		/*
		 * If on run queue move to new run queue, and quit.
		 * XXXKSE this gets a lot more complicated under threads
		 * but try anyhow.
		 */
		if (TD_ON_RUNQ(td)) {
			MPASS(td->td_blocked == NULL);
			sched_prio(td, pri);
			return;
		}
		/*
		 * Adjust for any other cases.
		 */
		td->td_priority = pri;

		/*
		 * If we aren't blocked on a mutex, we should be.
		 */
		KASSERT(TD_ON_LOCK(td), (
		    "process %d(%s):%d holds %s but isn't blocked on a mutex\n",
		    td->td_proc->p_pid, td->td_proc->p_comm, td->td_state,
		    m->mtx_object.lo_name));

		/*
		 * Pick up the mutex that td is blocked on.
		 */
		m = td->td_blocked;
		MPASS(m != NULL);

		/*
		 * Check if the thread needs to be moved up on
		 * the blocked chain
		 */
		if (td == TAILQ_FIRST(&m->mtx_blocked)) {
			continue;
		}

		td1 = TAILQ_PREV(td, threadqueue, td_lockq);
		if (td1->td_priority <= pri) {
			continue;
		}

		/*
		 * Remove thread from blocked chain and determine where
		 * it should be moved up to.  Since we know that td1 has
		 * a lower priority than td, we know that at least one
		 * thread in the chain has a lower priority and that
		 * td1 will thus not be NULL after the loop.
		 */
		TAILQ_REMOVE(&m->mtx_blocked, td, td_lockq);
		TAILQ_FOREACH(td1, &m->mtx_blocked, td_lockq) {
			MPASS(td1->td_proc->p_magic == P_MAGIC);
			if (td1->td_priority > pri)
				break;
		}

		MPASS(td1 != NULL);
		TAILQ_INSERT_BEFORE(td1, td, td_lockq);
		CTR4(KTR_LOCK,
		    "propagate_priority: p %p moved before %p on [%p] %s",
		    td, td1, m, m->mtx_object.lo_name);
	}
}

#ifdef MUTEX_PROFILING
SYSCTL_NODE(_debug, OID_AUTO, mutex, CTLFLAG_RD, NULL, "mutex debugging");
SYSCTL_NODE(_debug_mutex, OID_AUTO, prof, CTLFLAG_RD, NULL, "mutex profiling");
static int mutex_prof_enable = 0;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, enable, CTLFLAG_RW,
    &mutex_prof_enable, 0, "Enable tracing of mutex holdtime");

struct mutex_prof {
	const char	*name;
	const char	*file;
	int		line;
	struct {
		uintmax_t	max;
		uintmax_t	tot;
		uintmax_t	cur;
	} cnt;
	struct mutex_prof *next;
};

/*
 * mprof_buf is a static pool of profiling records to avoid possible
 * reentrance of the memory allocation functions.
 *
 * Note: NUM_MPROF_BUFFERS must be smaller than MPROF_HASH_SIZE.
 */
#define	NUM_MPROF_BUFFERS	1000
static struct mutex_prof mprof_buf[NUM_MPROF_BUFFERS];
static int first_free_mprof_buf;
#define	MPROF_HASH_SIZE		1009
static struct mutex_prof *mprof_hash[MPROF_HASH_SIZE];

static int mutex_prof_acquisitions;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, acquisitions, CTLFLAG_RD,
    &mutex_prof_acquisitions, 0, "Number of mutex acquistions recorded");
static int mutex_prof_records;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, records, CTLFLAG_RD,
    &mutex_prof_records, 0, "Number of profiling records");
static int mutex_prof_maxrecords = NUM_MPROF_BUFFERS;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, maxrecords, CTLFLAG_RD,
    &mutex_prof_maxrecords, 0, "Maximum number of profiling records");
static int mutex_prof_rejected;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, rejected, CTLFLAG_RD,
    &mutex_prof_rejected, 0, "Number of rejected profiling records");
static int mutex_prof_hashsize = MPROF_HASH_SIZE;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, hashsize, CTLFLAG_RD,
    &mutex_prof_hashsize, 0, "Hash size");
static int mutex_prof_collisions = 0;
SYSCTL_INT(_debug_mutex_prof, OID_AUTO, collisions, CTLFLAG_RD,
    &mutex_prof_collisions, 0, "Number of hash collisions");

/*
 * mprof_mtx protects the profiling buffers and the hash.
 */
static struct mtx mprof_mtx;
MTX_SYSINIT(mprof, &mprof_mtx, "mutex profiling lock", MTX_SPIN | MTX_QUIET);

static u_int64_t
nanoseconds(void)
{
	struct timespec tv;

	nanotime(&tv);
	return (tv.tv_sec * (u_int64_t)1000000000 + tv.tv_nsec);
}

static int
dump_mutex_prof_stats(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *sb;
	int error, i;

	if (first_free_mprof_buf == 0)
		return (SYSCTL_OUT(req, "No locking recorded",
		    sizeof("No locking recorded")));

	sb = sbuf_new(NULL, NULL, 1024, SBUF_AUTOEXTEND);
	sbuf_printf(sb, "%6s %12s %11s %5s %s\n",
	    "max", "total", "count", "avg", "name");
	/*
	 * XXX this spinlock seems to be by far the largest perpetrator
	 * of spinlock latency (1.6 msec on an Athlon1600 was recorded
	 * even before I pessimized it further by moving the average
	 * computation here).
	 */
	mtx_lock_spin(&mprof_mtx);
	for (i = 0; i < first_free_mprof_buf; ++i)
		sbuf_printf(sb, "%6ju %12ju %11ju %5ju %s:%d (%s)\n",
		    mprof_buf[i].cnt.max / 1000,
		    mprof_buf[i].cnt.tot / 1000,
		    mprof_buf[i].cnt.cur,
		    mprof_buf[i].cnt.cur == 0 ? (uintmax_t)0 :
			mprof_buf[i].cnt.tot / (mprof_buf[i].cnt.cur * 1000),
		    mprof_buf[i].file, mprof_buf[i].line, mprof_buf[i].name);
	mtx_unlock_spin(&mprof_mtx);
	sbuf_finish(sb);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (error);
}
SYSCTL_PROC(_debug_mutex_prof, OID_AUTO, stats, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, dump_mutex_prof_stats, "A", "Mutex profiling statistics");
#endif

/*
 * Function versions of the inlined __mtx_* macros.  These are used by
 * modules and can also be called from assembly language if needed.
 */
void
_mtx_lock_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_object.lo_class == &lock_class_mtx_sleep,
	    ("mtx_lock() of spin mutex %s @ %s:%d", m->mtx_object.lo_name,
	    file, line));
	_get_sleep_lock(m, curthread, opts, file, line);
	LOCK_LOG_LOCK("LOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	WITNESS_LOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
#ifdef MUTEX_PROFILING
	/* don't reset the timer when/if recursing */
	if (m->mtx_acqtime == 0) {
		m->mtx_filename = file;
		m->mtx_lineno = line;
		m->mtx_acqtime = mutex_prof_enable ? nanoseconds() : 0;
		++mutex_prof_acquisitions;
	}
#endif
}

void
_mtx_unlock_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_object.lo_class == &lock_class_mtx_sleep,
	    ("mtx_unlock() of spin mutex %s @ %s:%d", m->mtx_object.lo_name,
	    file, line));
 	WITNESS_UNLOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("UNLOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	mtx_assert(m, MA_OWNED);
#ifdef MUTEX_PROFILING
	if (m->mtx_acqtime != 0) {
		static const char *unknown = "(unknown)";
		struct mutex_prof *mpp;
		u_int64_t acqtime, now;
		const char *p, *q;
		volatile u_int hash;

		now = nanoseconds();
		acqtime = m->mtx_acqtime;
		m->mtx_acqtime = 0;
		if (now <= acqtime)
			goto out;
		for (p = m->mtx_filename; strncmp(p, "../", 3) == 0; p += 3)
			/* nothing */ ;
		if (p == NULL || *p == '\0')
			p = unknown;
		for (hash = m->mtx_lineno, q = p; *q != '\0'; ++q)
			hash = (hash * 2 + *q) % MPROF_HASH_SIZE;
		mtx_lock_spin(&mprof_mtx);
		for (mpp = mprof_hash[hash]; mpp != NULL; mpp = mpp->next)
			if (mpp->line == m->mtx_lineno &&
			    strcmp(mpp->file, p) == 0)
				break;
		if (mpp == NULL) {
			/* Just exit if we cannot get a trace buffer */
			if (first_free_mprof_buf >= NUM_MPROF_BUFFERS) {
				++mutex_prof_rejected;
				goto unlock;
			}
			mpp = &mprof_buf[first_free_mprof_buf++];
			mpp->name = mtx_name(m);
			mpp->file = p;
			mpp->line = m->mtx_lineno;
			mpp->next = mprof_hash[hash];
			if (mprof_hash[hash] != NULL)
				++mutex_prof_collisions;
			mprof_hash[hash] = mpp;	
			++mutex_prof_records;
		}
		/*
		 * Record if the mutex has been held longer now than ever
		 * before.
		 */
		if (now - acqtime > mpp->cnt.max)
			mpp->cnt.max = now - acqtime;
		mpp->cnt.tot += now - acqtime;
		mpp->cnt.cur++;
unlock:
		mtx_unlock_spin(&mprof_mtx);
	}
out:
#endif
	_rel_sleep_lock(m, curthread, opts, file, line);
}

void
_mtx_lock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_object.lo_class == &lock_class_mtx_spin,
	    ("mtx_lock_spin() of sleep mutex %s @ %s:%d",
	    m->mtx_object.lo_name, file, line));
#if defined(SMP) || LOCK_DEBUG > 0 || 1
	_get_spin_lock(m, curthread, opts, file, line);
#else
	critical_enter();
#endif
	LOCK_LOG_LOCK("LOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	WITNESS_LOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
}

void
_mtx_unlock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_object.lo_class == &lock_class_mtx_spin,
	    ("mtx_unlock_spin() of sleep mutex %s @ %s:%d",
	    m->mtx_object.lo_name, file, line));
 	WITNESS_UNLOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("UNLOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	mtx_assert(m, MA_OWNED);
#if defined(SMP) || LOCK_DEBUG > 0 || 1
	_rel_spin_lock(m);
#else
	critical_exit();
#endif
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

	MPASS(curthread != NULL);

	rval = _obtain_lock(m, curthread);

	LOCK_LOG_TRY("LOCK", &m->mtx_object, opts, rval, file, line);
	if (rval) {
		/*
		 * We do not handle recursion in _mtx_trylock; see the
		 * note at the top of the routine.
		 */
		KASSERT(!mtx_recursed(m),
		    ("mtx_trylock() called on a recursed mutex"));
		WITNESS_LOCK(&m->mtx_object, opts | LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
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
	struct thread *td = curthread;
#if defined(SMP) && defined(ADAPTIVE_MUTEXES)
	struct thread *owner;
#endif
#ifdef KTR
	int cont_logged = 0;
#endif

	if ((m->mtx_lock & MTX_FLAGMASK) == (uintptr_t)td) {
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

	while (!_obtain_lock(m, td)) {
		uintptr_t v;
		struct thread *td1;

		mtx_lock_spin(&sched_lock);
		/*
		 * Check if the lock has been released while spinning for
		 * the sched_lock.
		 */
		if ((v = m->mtx_lock) == MTX_UNOWNED) {
			mtx_unlock_spin(&sched_lock);
#ifdef __i386__
			ia32_pause();
#endif
			continue;
		}

		/*
		 * The mutex was marked contested on release. This means that
		 * there are threads blocked on it.
		 */
		if (v == MTX_CONTESTED) {
			td1 = TAILQ_FIRST(&m->mtx_blocked);
			MPASS(td1 != NULL);
			m->mtx_lock = (uintptr_t)td | MTX_CONTESTED;

			if (td1->td_priority < td->td_priority)
				td->td_priority = td1->td_priority; 
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
#ifdef __i386__
			ia32_pause();
#endif
			continue;
		}

#if defined(SMP) && defined(ADAPTIVE_MUTEXES)
		/*
		 * If the current owner of the lock is executing on another
		 * CPU, spin instead of blocking.
		 */
		owner = (struct thread *)(v & MTX_FLAGMASK);
		if (m != &Giant && thread_running(owner)) {
			mtx_unlock_spin(&sched_lock);
			while (mtx_owner(m) == owner && thread_running(owner)) {
#ifdef __i386__
				ia32_pause();
#endif
			}
			continue;
		}
#endif	/* SMP && ADAPTIVE_MUTEXES */

		/*
		 * We definitely must sleep for this lock.
		 */
		mtx_assert(m, MA_NOTOWNED);

#ifdef notyet
		/*
		 * If we're borrowing an interrupted thread's VM context, we
		 * must clean up before going to sleep.
		 */
		if (td->td_ithd != NULL) {
			struct ithd *it = td->td_ithd;

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
			td1 = mtx_owner(m);
			LIST_INSERT_HEAD(&td1->td_contested, m, mtx_contested);
			TAILQ_INSERT_TAIL(&m->mtx_blocked, td, td_lockq);
		} else {
			TAILQ_FOREACH(td1, &m->mtx_blocked, td_lockq)
				if (td1->td_priority > td->td_priority)
					break;
			if (td1)
				TAILQ_INSERT_BEFORE(td1, td, td_lockq);
			else
				TAILQ_INSERT_TAIL(&m->mtx_blocked, td, td_lockq);
		}
#ifdef KTR
		if (!cont_logged) {
			CTR6(KTR_CONTENTION,
			    "contention: %p at %s:%d wants %s, taken by %s:%d",
			    td, file, line, m->mtx_object.lo_name,
			    WITNESS_FILE(&m->mtx_object),
			    WITNESS_LINE(&m->mtx_object));
			cont_logged = 1;
		}
#endif

		/*
		 * Save who we're blocked on.
		 */
		td->td_blocked = m;
		td->td_lockname = m->mtx_object.lo_name;
		TD_SET_LOCK(td);
		propagate_priority(td);

		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR3(KTR_LOCK,
			    "_mtx_lock_sleep: p %p blocked on [%p] %s", td, m,
			    m->mtx_object.lo_name);

		td->td_proc->p_stats->p_ru.ru_nvcsw++;
		mi_switch();

		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR3(KTR_LOCK,
			  "_mtx_lock_sleep: p %p free from blocked on [%p] %s",
			  td, m, m->mtx_object.lo_name);

		mtx_unlock_spin(&sched_lock);
	}

#ifdef KTR
	if (cont_logged) {
		CTR4(KTR_CONTENTION,
		    "contention end: %s acquired by %p at %s:%d",
		    m->mtx_object.lo_name, td, file, line);
	}
#endif
	return;
}

/*
 * _mtx_lock_spin: the tougher part of acquiring an MTX_SPIN lock.
 *
 * This is only called if we need to actually spin for the lock. Recursion
 * is handled inline.
 */
void
_mtx_lock_spin(struct mtx *m, int opts, const char *file, int line)
{
	int i = 0;

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spinning", m);

	for (;;) {
		if (_obtain_lock(m, curthread))
			break;

		/* Give interrupts a chance while we spin. */
		critical_exit();
		while (m->mtx_lock != MTX_UNOWNED) {
			if (i++ < 10000000) {
#ifdef __i386__
				ia32_pause();
#endif
				continue;
			}
			if (i < 60000000)
				DELAY(1);
#ifdef DDB
			else if (!db_active)
#else
			else
#endif
				panic("spin lock %s held by %p for > 5 seconds",
				    m->mtx_object.lo_name, (void *)m->mtx_lock);
#ifdef __i386__
			ia32_pause();
#endif
		}
		critical_enter();
	}

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
	struct thread *td, *td1;
	struct mtx *m1;
	int pri;

	td = curthread;

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

	td1 = TAILQ_FIRST(&m->mtx_blocked);
#if defined(SMP) && defined(ADAPTIVE_MUTEXES)
	if (td1 == NULL) {
		_release_lock_quick(m);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p no sleepers", m);
		mtx_unlock_spin(&sched_lock);
		return;
	}
#endif
	MPASS(td->td_proc->p_magic == P_MAGIC);
	MPASS(td1->td_proc->p_magic == P_MAGIC);

	TAILQ_REMOVE(&m->mtx_blocked, td1, td_lockq);

	if (TAILQ_EMPTY(&m->mtx_blocked)) {
		LIST_REMOVE(m, mtx_contested);
		_release_lock_quick(m);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p not held", m);
	} else
		atomic_store_rel_ptr(&m->mtx_lock, (void *)MTX_CONTESTED);

	pri = PRI_MAX;
	LIST_FOREACH(m1, &td->td_contested, mtx_contested) {
		int cp = TAILQ_FIRST(&m1->mtx_blocked)->td_priority;
		if (cp < pri)
			pri = cp;
	}

	if (pri > td->td_base_pri)
		pri = td->td_base_pri;
	td->td_priority = pri;

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR2(KTR_LOCK, "_mtx_unlock_sleep: %p contested setrunqueue %p",
		    m, td1);

	td1->td_blocked = NULL;
	TD_CLR_LOCK(td1);
	if (!TD_CAN_RUN(td1)) {
		mtx_unlock_spin(&sched_lock);
		return;
	}
	setrunqueue(td1);

	if (td->td_critnest == 1 && td1->td_priority < pri) {
#ifdef notyet
		if (td->td_ithd != NULL) {
			struct ithd *it = td->td_ithd;

			if (it->it_interrupted) {
				if (LOCK_LOG_TEST(&m->mtx_object, opts))
					CTR2(KTR_LOCK,
				    "_mtx_unlock_sleep: %p interrupted %p",
					    it, it->it_interrupted);
				intr_thd_fixup(it);
			}
		}
#endif
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR2(KTR_LOCK,
			    "_mtx_unlock_sleep: %p switching out lock=%p", m,
			    (void *)m->mtx_lock);

		td->td_proc->p_stats->p_ru.ru_nivcsw++;
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

/*
 * The backing function for the INVARIANTS-enabled mtx_assert()
 */
#ifdef INVARIANT_SUPPORT
void
_mtx_assert(struct mtx *m, int what, const char *file, int line)
{

	if (panicstr != NULL)
		return;
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

void	mtx_validate(struct mtx *);

void
mtx_validate(struct mtx *m)
{

/*
 * XXX - When kernacc() is fixed on the alpha to handle K0_SEG memory properly
 * we can re-enable the kernacc() checks.
 */
#ifndef __alpha__
	/*
	 * Can't call kernacc() from early init386(), especially when
	 * initializing Giant mutex, because some stuff in kernacc()
	 * requires Giant itself.
	 */ 
	if (!cold)
		if (!kernacc((caddr_t)m, sizeof(m),
		    VM_PROT_READ | VM_PROT_WRITE))
			panic("Can't read and write to mutex %p", m);
#endif
}
#endif

/*
 * General init routine used by the MTX_SYSINIT() macro.
 */
void
mtx_sysinit(void *arg)
{
	struct mtx_args *margs = arg;

	mtx_init(margs->ma_mtx, margs->ma_desc, NULL, margs->ma_opts);
}

/*
 * Mutex initialization routine; initialize lock `m' of type contained in
 * `opts' with options contained in `opts' and name `name.'  The optional
 * lock type `type' is used as a general lock category name for use with
 * witness.
 */ 
void
mtx_init(struct mtx *m, const char *name, const char *type, int opts)
{
	struct lock_object *lock;

	MPASS((opts & ~(MTX_SPIN | MTX_QUIET | MTX_RECURSE |
	    MTX_SLEEPABLE | MTX_NOWITNESS | MTX_DUPOK)) == 0);

#ifdef MUTEX_DEBUG
	/* Diagnostic and error correction */
	mtx_validate(m);
#endif

	lock = &m->mtx_object;
	KASSERT((lock->lo_flags & LO_INITIALIZED) == 0,
	    ("mutex %s %p already initialized", name, m));
	bzero(m, sizeof(*m));
	if (opts & MTX_SPIN)
		lock->lo_class = &lock_class_mtx_spin;
	else
		lock->lo_class = &lock_class_mtx_sleep;
	lock->lo_name = name;
	lock->lo_type = type != NULL ? type : name;
	if (opts & MTX_QUIET)
		lock->lo_flags = LO_QUIET;
	if (opts & MTX_RECURSE)
		lock->lo_flags |= LO_RECURSABLE;
	if (opts & MTX_SLEEPABLE)
		lock->lo_flags |= LO_SLEEPABLE;
	if ((opts & MTX_NOWITNESS) == 0)
		lock->lo_flags |= LO_WITNESS;
	if (opts & MTX_DUPOK)
		lock->lo_flags |= LO_DUPOK;

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
		WITNESS_UNLOCK(&m->mtx_object, LOP_EXCLUSIVE, __FILE__,
		    __LINE__);
	}

	WITNESS_DESTROY(&m->mtx_object);
}

/*
 * Intialize the mutex code and system mutexes.  This is called from the MD
 * startup code prior to mi_startup().  The per-CPU data space needs to be
 * setup before this is called.
 */
void
mutex_init(void)
{

	/* Setup thread0 so that mutexes work. */
	LIST_INIT(&thread0.td_contested);

	/*
	 * Initialize mutexes.
	 */
	mtx_init(&Giant, "Giant", NULL, MTX_DEF | MTX_RECURSE);
	mtx_init(&sched_lock, "sched lock", NULL, MTX_SPIN | MTX_RECURSE);
	mtx_init(&proc0.p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK);
	mtx_lock(&Giant);
}

/*
 * Encapsulated Giant mutex routines.  These routines provide encapsulation
 * control for the Giant mutex, allowing sysctls to be used to turn on and
 * off Giant around certain subsystems.  The default value for the sysctls
 * are set to what developers believe is stable and working in regards to
 * the Giant pushdown.  Developers should not turn off Giant via these
 * sysctls unless they know what they are doing.
 *
 * Callers of mtx_lock_giant() are expected to pass the return value to an
 * accompanying mtx_unlock_giant() later on.  If multiple subsystems are 
 * effected by a Giant wrap, all related sysctl variables must be zero for
 * the subsystem call to operate without Giant (as determined by the caller).
 */

SYSCTL_NODE(_kern, OID_AUTO, giant, CTLFLAG_RD, NULL, "Giant mutex manipulation");

static int kern_giant_all = 0;
SYSCTL_INT(_kern_giant, OID_AUTO, all, CTLFLAG_RW, &kern_giant_all, 0, "");

int kern_giant_proc = 1;	/* Giant around PROC locks */
int kern_giant_file = 1;	/* Giant around struct file & filedesc */
int kern_giant_ucred = 1;	/* Giant around ucred */
SYSCTL_INT(_kern_giant, OID_AUTO, proc, CTLFLAG_RW, &kern_giant_proc, 0, "");
SYSCTL_INT(_kern_giant, OID_AUTO, file, CTLFLAG_RW, &kern_giant_file, 0, "");
SYSCTL_INT(_kern_giant, OID_AUTO, ucred, CTLFLAG_RW, &kern_giant_ucred, 0, "");

int
mtx_lock_giant(int sysctlvar)
{
	if (sysctlvar || kern_giant_all) {
		mtx_lock(&Giant);
		return(1);
	}
	return(0);
}

void
mtx_unlock_giant(int s)
{
	if (s)
		mtx_unlock(&Giant);
}

