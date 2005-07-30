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
 */

/*
 * Machine independent bits of mutex implementation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_adaptive_mutexes.h"
#include "opt_ddb.h"
#include "opt_mprof.h"
#include "opt_mutex_wake_all.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/turnstile.h>
#include <sys/vmmeter.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

/* 
 * Force MUTEX_WAKE_ALL for now.
 * single thread wakeup needs fixes to avoid race conditions with 
 * priority inheritance.
 */
#ifndef MUTEX_WAKE_ALL
#define MUTEX_WAKE_ALL
#endif

/*
 * Internal utility macros.
 */
#define mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)

#define mtx_owner(m)	(mtx_unowned((m)) ? NULL \
	: (struct thread *)((m)->mtx_lock & MTX_FLAGMASK))

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
	uintmax_t	cnt_max;
	uintmax_t	cnt_tot;
	uintmax_t	cnt_cur;
	uintmax_t	cnt_contest_holding;
	uintmax_t	cnt_contest_locking;
	struct mutex_prof *next;
};

/*
 * mprof_buf is a static pool of profiling records to avoid possible
 * reentrance of the memory allocation functions.
 *
 * Note: NUM_MPROF_BUFFERS must be smaller than MPROF_HASH_SIZE.
 */
#ifdef MPROF_BUFFERS
#define NUM_MPROF_BUFFERS	MPROF_BUFFERS
#else
#define	NUM_MPROF_BUFFERS	1000
#endif
static struct mutex_prof mprof_buf[NUM_MPROF_BUFFERS];
static int first_free_mprof_buf;
#ifndef MPROF_HASH_SIZE
#define	MPROF_HASH_SIZE		1009
#endif
#if NUM_MPROF_BUFFERS >= MPROF_HASH_SIZE
#error MPROF_BUFFERS must be larger than MPROF_HASH_SIZE
#endif
static struct mutex_prof *mprof_hash[MPROF_HASH_SIZE];
/* SWAG: sbuf size = avg stat. line size * number of locks */
#define MPROF_SBUF_SIZE		256 * 400

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
	static int multiplier = 1;

	if (first_free_mprof_buf == 0)
		return (SYSCTL_OUT(req, "No locking recorded",
		    sizeof("No locking recorded")));

retry_sbufops:
	sb = sbuf_new(NULL, NULL, MPROF_SBUF_SIZE * multiplier, SBUF_FIXEDLEN);
	sbuf_printf(sb, "\n%6s %12s %11s %5s %12s %12s %s\n",
	    "max", "total", "count", "avg", "cnt_hold", "cnt_lock", "name");
	/*
	 * XXX this spinlock seems to be by far the largest perpetrator
	 * of spinlock latency (1.6 msec on an Athlon1600 was recorded
	 * even before I pessimized it further by moving the average
	 * computation here).
	 */
	mtx_lock_spin(&mprof_mtx);
	for (i = 0; i < first_free_mprof_buf; ++i) {
		sbuf_printf(sb, "%6ju %12ju %11ju %5ju %12ju %12ju %s:%d (%s)\n",
		    mprof_buf[i].cnt_max / 1000,
		    mprof_buf[i].cnt_tot / 1000,
		    mprof_buf[i].cnt_cur,
		    mprof_buf[i].cnt_cur == 0 ? (uintmax_t)0 :
			mprof_buf[i].cnt_tot / (mprof_buf[i].cnt_cur * 1000),
		    mprof_buf[i].cnt_contest_holding,
		    mprof_buf[i].cnt_contest_locking,
		    mprof_buf[i].file, mprof_buf[i].line, mprof_buf[i].name);
		if (sbuf_overflowed(sb)) {
			mtx_unlock_spin(&mprof_mtx);
			sbuf_delete(sb);
			multiplier++;
			goto retry_sbufops;
		}
	}
	mtx_unlock_spin(&mprof_mtx);
	sbuf_finish(sb);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (error);
}
SYSCTL_PROC(_debug_mutex_prof, OID_AUTO, stats, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, dump_mutex_prof_stats, "A", "Mutex profiling statistics");

static int
reset_mutex_prof_stats(SYSCTL_HANDLER_ARGS)
{
	int error, v;

	if (first_free_mprof_buf == 0)
		return (0);

	v = 0;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error)
		return (error);
	if (req->newptr == NULL)
		return (error);
	if (v == 0)
		return (0);

	mtx_lock_spin(&mprof_mtx);
	bzero(mprof_buf, sizeof(*mprof_buf) * first_free_mprof_buf);
	bzero(mprof_hash, sizeof(struct mtx *) * MPROF_HASH_SIZE);
	first_free_mprof_buf = 0;
	mtx_unlock_spin(&mprof_mtx);
	return (0);
}
SYSCTL_PROC(_debug_mutex_prof, OID_AUTO, reset, CTLTYPE_INT | CTLFLAG_RW,
    NULL, 0, reset_mutex_prof_stats, "I", "Reset mutex profiling statistics");
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
	WITNESS_CHECKORDER(&m->mtx_object, opts | LOP_NEWORDER | LOP_EXCLUSIVE,
	    file, line);
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
		for (p = m->mtx_filename;
		    p != NULL && strncmp(p, "../", 3) == 0; p += 3)
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
		if (now - acqtime > mpp->cnt_max)
			mpp->cnt_max = now - acqtime;
		mpp->cnt_tot += now - acqtime;
		mpp->cnt_cur++;
		/*
		 * There's a small race, really we should cmpxchg
		 * 0 with the current value, but that would bill
		 * the contention to the wrong lock instance if
		 * it followed this also.
		 */
		mpp->cnt_contest_holding += m->mtx_contest_holding;
		m->mtx_contest_holding = 0;
		mpp->cnt_contest_locking += m->mtx_contest_locking;
		m->mtx_contest_locking = 0;
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
	WITNESS_CHECKORDER(&m->mtx_object, opts | LOP_NEWORDER | LOP_EXCLUSIVE,
	    file, line);
	_get_spin_lock(m, curthread, opts, file, line);
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
	_rel_spin_lock(m);
}

/*
 * The important part of mtx_trylock{,_flags}()
 * Tries to acquire lock `m.'  If this function is called on a mutex that
 * is already owned, it will recursively acquire the lock.
 */
int
_mtx_trylock(struct mtx *m, int opts, const char *file, int line)
{
	int rval;

	MPASS(curthread != NULL);

	if (mtx_owned(m) && (m->mtx_object.lo_flags & LO_RECURSABLE) != 0) {
		m->mtx_recurse++;
		atomic_set_ptr(&m->mtx_lock, MTX_RECURSED);
		rval = 1;
	} else
		rval = _obtain_lock(m, (uintptr_t)curthread);

	LOCK_LOG_TRY("LOCK", &m->mtx_object, opts, rval, file, line);
	if (rval)
		WITNESS_LOCK(&m->mtx_object, opts | LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);

	return (rval);
}

/*
 * _mtx_lock_sleep: the tougher part of acquiring an MTX_DEF lock.
 *
 * We call this if the lock is either contested (i.e. we need to go to
 * sleep waiting for it), or if we need to recurse on it.
 */
void
_mtx_lock_sleep(struct mtx *m, uintptr_t tid, int opts, const char *file,
    int line)
{
#if defined(SMP) && !defined(NO_ADAPTIVE_MUTEXES)
	struct thread *owner;
#endif
	uintptr_t v;
#ifdef KTR
	int cont_logged = 0;
#endif
#ifdef MUTEX_PROFILING
	int contested;
#endif

	if (mtx_owned(m)) {
		KASSERT((m->mtx_object.lo_flags & LO_RECURSABLE) != 0,
	    ("_mtx_lock_sleep: recursed on non-recursive mutex %s @ %s:%d\n",
		    m->mtx_object.lo_name, file, line));
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

#ifdef MUTEX_PROFILING
	contested = 0;
#endif
	while (!_obtain_lock(m, tid)) {
#ifdef MUTEX_PROFILING
		contested = 1;
		atomic_add_int(&m->mtx_contest_holding, 1);
#endif
		turnstile_lock(&m->mtx_object);
		v = m->mtx_lock;

		/*
		 * Check if the lock has been released while spinning for
		 * the turnstile chain lock.
		 */
		if (v == MTX_UNOWNED) {
			turnstile_release(&m->mtx_object);
			cpu_spinwait();
			continue;
		}

#ifdef MUTEX_WAKE_ALL
		MPASS(v != MTX_CONTESTED);
#else
		/*
		 * The mutex was marked contested on release. This means that
		 * there are other threads blocked on it.  Grab ownership of
		 * it and propagate its priority to the current thread if
		 * necessary.
		 */
		if (v == MTX_CONTESTED) {
			m->mtx_lock = tid | MTX_CONTESTED;
			turnstile_claim(&m->mtx_object);
			break;
		}
#endif

		/*
		 * If the mutex isn't already contested and a failure occurs
		 * setting the contested bit, the mutex was either released
		 * or the state of the MTX_RECURSED bit changed.
		 */
		if ((v & MTX_CONTESTED) == 0 &&
		    !atomic_cmpset_ptr(&m->mtx_lock, v, v | MTX_CONTESTED)) {
			turnstile_release(&m->mtx_object);
			cpu_spinwait();
			continue;
		}

#if defined(SMP) && !defined(NO_ADAPTIVE_MUTEXES)
		/*
		 * If the current owner of the lock is executing on another
		 * CPU, spin instead of blocking.
		 */
		owner = (struct thread *)(v & MTX_FLAGMASK);
#ifdef ADAPTIVE_GIANT
		if (TD_IS_RUNNING(owner)) {
#else
		if (m != &Giant && TD_IS_RUNNING(owner)) {
#endif
			turnstile_release(&m->mtx_object);
			while (mtx_owner(m) == owner && TD_IS_RUNNING(owner)) {
				cpu_spinwait();
			}
			continue;
		}
#endif	/* SMP && !NO_ADAPTIVE_MUTEXES */

		/*
		 * We definitely must sleep for this lock.
		 */
		mtx_assert(m, MA_NOTOWNED);

#ifdef KTR
		if (!cont_logged) {
			CTR6(KTR_CONTENTION,
			    "contention: %p at %s:%d wants %s, taken by %s:%d",
			    (void *)tid, file, line, m->mtx_object.lo_name,
			    WITNESS_FILE(&m->mtx_object),
			    WITNESS_LINE(&m->mtx_object));
			cont_logged = 1;
		}
#endif

		/*
		 * Block on the turnstile.
		 */
		turnstile_wait(&m->mtx_object, mtx_owner(m));
	}

#ifdef KTR
	if (cont_logged) {
		CTR4(KTR_CONTENTION,
		    "contention end: %s acquired by %p at %s:%d",
		    m->mtx_object.lo_name, (void *)tid, file, line);
	}
#endif
#ifdef MUTEX_PROFILING
	if (contested)
		m->mtx_contest_locking++;
	m->mtx_contest_holding = 0;
#endif
	return;
}

#ifdef SMP
/*
 * _mtx_lock_spin: the tougher part of acquiring an MTX_SPIN lock.
 *
 * This is only called if we need to actually spin for the lock. Recursion
 * is handled inline.
 */
void
_mtx_lock_spin(struct mtx *m, uintptr_t tid, int opts, const char *file,
    int line)
{
	int i = 0;

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spinning", m);

	for (;;) {
		if (_obtain_lock(m, tid))
			break;

		/* Give interrupts a chance while we spin. */
		spinlock_exit();
		while (m->mtx_lock != MTX_UNOWNED) {
			if (i++ < 10000000) {
				cpu_spinwait();
				continue;
			}
			if (i < 60000000)
				DELAY(1);
			else if (!kdb_active) {
				printf("spin lock %s held by %p for > 5 seconds\n",
				    m->mtx_object.lo_name, (void *)m->mtx_lock);
#ifdef WITNESS
				witness_display_spinlock(&m->mtx_object,
				    mtx_owner(m));
#endif
				panic("spin lock held too long");
			}
			cpu_spinwait();
		}
		spinlock_enter();
	}

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spin done", m);

	return;
}
#endif /* SMP */

/*
 * _mtx_unlock_sleep: the tougher part of releasing an MTX_DEF lock.
 *
 * We are only called here if the lock is recursed or contested (i.e. we
 * need to wake up a blocked thread).
 */
void
_mtx_unlock_sleep(struct mtx *m, int opts, const char *file, int line)
{
	struct turnstile *ts;
#ifndef PREEMPTION
	struct thread *td, *td1;
#endif

	if (mtx_recursed(m)) {
		if (--(m->mtx_recurse) == 0)
			atomic_clear_ptr(&m->mtx_lock, MTX_RECURSED);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p unrecurse", m);
		return;
	}

	turnstile_lock(&m->mtx_object);
	ts = turnstile_lookup(&m->mtx_object);
	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p contested", m);

#if defined(SMP) && !defined(NO_ADAPTIVE_MUTEXES)
	if (ts == NULL) {
		_release_lock_quick(m);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p no sleepers", m);
		turnstile_release(&m->mtx_object);
		return;
	}
#else
	MPASS(ts != NULL);
#endif
#ifndef PREEMPTION
	/* XXX */
	td1 = turnstile_head(ts);
#endif
#ifdef MUTEX_WAKE_ALL
	turnstile_broadcast(ts);
	_release_lock_quick(m);
#else
	if (turnstile_signal(ts)) {
		_release_lock_quick(m);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p not held", m);
	} else {
		m->mtx_lock = MTX_CONTESTED;
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR1(KTR_LOCK, "_mtx_unlock_sleep: %p still contested",
			    m);
	}
#endif
	turnstile_unpend(ts);

#ifndef PREEMPTION
	/*
	 * XXX: This is just a hack until preemption is done.  However,
	 * once preemption is done we need to either wrap the
	 * turnstile_signal() and release of the actual lock in an
	 * extra critical section or change the preemption code to
	 * always just set a flag and never do instant-preempts.
	 */
	td = curthread;
	if (td->td_critnest > 0 || td1->td_priority >= td->td_priority)
		return;
	mtx_lock_spin(&sched_lock);
	if (!TD_IS_RUNNING(td1)) {
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

		mi_switch(SW_INVOL, NULL);
		if (LOCK_LOG_TEST(&m->mtx_object, opts))
			CTR2(KTR_LOCK, "_mtx_unlock_sleep: %p resuming lock=%p",
			    m, (void *)m->mtx_lock);
	}
	mtx_unlock_spin(&sched_lock);
#endif

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

	if (panicstr != NULL || dumping)
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
 * XXX: When kernacc() does not require Giant we can reenable this check
 */
#ifdef notyet
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
	    MTX_NOWITNESS | MTX_DUPOK)) == 0);

#ifdef MUTEX_DEBUG
	/* Diagnostic and error correction */
	mtx_validate(m);
#endif

	lock = &m->mtx_object;
	KASSERT((lock->lo_flags & LO_INITIALIZED) == 0,
	    ("mutex \"%s\" %p already initialized", name, m));
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
	if ((opts & MTX_NOWITNESS) == 0)
		lock->lo_flags |= LO_WITNESS;
	if (opts & MTX_DUPOK)
		lock->lo_flags |= LO_DUPOK;

	m->mtx_lock = MTX_UNOWNED;

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

	/* Setup turnstiles so that sleep mutexes work. */
	init_turnstiles();

	/*
	 * Initialize mutexes.
	 */
	mtx_init(&Giant, "Giant", NULL, MTX_DEF | MTX_RECURSE);
	mtx_init(&sched_lock, "sched lock", NULL, MTX_SPIN | MTX_RECURSE);
	mtx_init(&proc0.p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK);
	mtx_lock(&Giant);
}
