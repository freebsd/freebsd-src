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
#include "opt_global.h"
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
#include <sys/lock_profile.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>

#include <fs/devfs/devfs_int.h>

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

#define	mtx_owner(m)	((struct thread *)((m)->mtx_lock & ~MTX_FLAGMASK))

#ifdef DDB
static void	db_show_mtx(struct lock_object *lock);
#endif

/*
 * Lock classes for sleep and spin mutexes.
 */
struct lock_class lock_class_mtx_sleep = {
	"sleep mutex",
	LC_SLEEPLOCK | LC_RECURSABLE,
#ifdef DDB
	db_show_mtx
#endif
};
struct lock_class lock_class_mtx_spin = {
	"spin mutex",
	LC_SPINLOCK | LC_RECURSABLE,
#ifdef DDB
	db_show_mtx
#endif
};

/*
 * System-wide mutexes
 */
struct mtx sched_lock;
struct mtx Giant;

#ifdef LOCK_PROFILING
static inline void lock_profile_init(void)
{
        int i;
        /* Initialize the mutex profiling locks */
        for (i = 0; i < LPROF_LOCK_SIZE; i++) {
                mtx_init(&lprof_locks[i], "mprof lock",
                    NULL, MTX_SPIN|MTX_QUIET|MTX_NOPROFILE);
        }
}
#else
static inline void lock_profile_init(void) {;}
#endif

/*
 * Function versions of the inlined __mtx_* macros.  These are used by
 * modules and can also be called from assembly language if needed.
 */
void
_mtx_lock_flags(struct mtx *m, int opts, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_lock() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->mtx_object) == &lock_class_mtx_sleep,
	    ("mtx_lock() of spin mutex %s @ %s:%d", m->mtx_object.lo_name,
	    file, line));
	WITNESS_CHECKORDER(&m->mtx_object, opts | LOP_NEWORDER | LOP_EXCLUSIVE,
	    file, line);

	_get_sleep_lock(m, curthread, opts, file, line);
	LOCK_LOG_LOCK("LOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	WITNESS_LOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
	curthread->td_locks++;
}

void
_mtx_unlock_flags(struct mtx *m, int opts, const char *file, int line)
{
	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_unlock() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->mtx_object) == &lock_class_mtx_sleep,
	    ("mtx_unlock() of spin mutex %s @ %s:%d", m->mtx_object.lo_name,
	    file, line));
	curthread->td_locks--;
	WITNESS_UNLOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("UNLOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	mtx_assert(m, MA_OWNED);

	lock_profile_release_lock(&m->mtx_object);
	_rel_sleep_lock(m, curthread, opts, file, line);
}

void
_mtx_lock_spin_flags(struct mtx *m, int opts, const char *file, int line)
{
	
	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_lock_spin() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->mtx_object) == &lock_class_mtx_spin,
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
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_unlock_spin() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->mtx_object) == &lock_class_mtx_spin,
	    ("mtx_unlock_spin() of sleep mutex %s @ %s:%d",
	    m->mtx_object.lo_name, file, line));
	WITNESS_UNLOCK(&m->mtx_object, opts | LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("UNLOCK", &m->mtx_object, opts, m->mtx_recurse, file,
	    line);
	mtx_assert(m, MA_OWNED);

	lock_profile_release_lock(&m->mtx_object);
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
	int rval, contested = 0;
	uint64_t waittime = 0;
	
	MPASS(curthread != NULL);
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_trylock() of destroyed mutex @ %s:%d", file, line));
	KASSERT(LOCK_CLASS(&m->mtx_object) == &lock_class_mtx_sleep,
	    ("mtx_trylock() of spin mutex %s @ %s:%d", m->mtx_object.lo_name,
	    file, line));

	if (mtx_owned(m) && (m->mtx_object.lo_flags & LO_RECURSABLE) != 0) {
		m->mtx_recurse++;
		atomic_set_ptr(&m->mtx_lock, MTX_RECURSED);
		rval = 1;
	} else
		rval = _obtain_lock(m, (uintptr_t)curthread);

	LOCK_LOG_TRY("LOCK", &m->mtx_object, opts, rval, file, line);
	if (rval) {
		WITNESS_LOCK(&m->mtx_object, opts | LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
		curthread->td_locks++;
		if (m->mtx_recurse == 0)
			lock_profile_obtain_lock_success(&m->mtx_object, contested,
			    waittime, file, line);

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
_mtx_lock_sleep(struct mtx *m, uintptr_t tid, int opts, const char *file,
    int line)
{
#if defined(SMP) && !defined(NO_ADAPTIVE_MUTEXES)
	volatile struct thread *owner;
#endif
#ifdef KTR
	int cont_logged = 0;
#endif
	uintptr_t v;
	
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

	while (!_obtain_lock(m, tid)) { 
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
		owner = (struct thread *)(v & ~MTX_FLAGMASK);
#ifdef ADAPTIVE_GIANT
		if (TD_IS_RUNNING(owner)) 
#else
		if (m != &Giant && TD_IS_RUNNING(owner)) 
#endif
		{
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
		turnstile_wait(&m->mtx_object, mtx_owner(m),
		    TS_EXCLUSIVE_QUEUE);
	}
#ifdef KTR
	if (cont_logged) {
		CTR4(KTR_CONTENTION,
		    "contention end: %s acquired by %p at %s:%d",
		    m->mtx_object.lo_name, (void *)tid, file, line);
	}
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
	struct thread *td;

	if (LOCK_LOG_TEST(&m->mtx_object, opts))
		CTR1(KTR_LOCK, "_mtx_lock_spin: %p spinning", m);

	while (!_obtain_lock(m, tid)) {

		/* Give interrupts a chance while we spin. */
		spinlock_exit();
		while (m->mtx_lock != MTX_UNOWNED) {
			if (i++ < 10000000) {
				cpu_spinwait();
				continue;
			}
			if (i < 60000000 || kdb_active || panicstr != NULL)
				DELAY(1);
			else {
				td = mtx_owner(m);

				/* If the mutex is unlocked, try again. */
				if (td == NULL)
					continue;
				printf(
			"spin lock %p (%s) held by %p (tid %d) too long\n",
				    m, m->mtx_object.lo_name, td, td->td_tid);
#ifdef WITNESS
				witness_display_spinlock(&m->mtx_object, td);
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
	td1 = turnstile_head(ts, TS_EXCLUSIVE_QUEUE);
#endif
#ifdef MUTEX_WAKE_ALL
	turnstile_broadcast(ts, TS_EXCLUSIVE_QUEUE);
	_release_lock_quick(m);
#else
	if (turnstile_signal(ts, TS_EXCLUSIVE_QUEUE)) {
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
	turnstile_unpend(ts, TS_EXCLUSIVE_LOCK);

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
	struct lock_class *class;
	int flags;

	MPASS((opts & ~(MTX_SPIN | MTX_QUIET | MTX_RECURSE |
		MTX_NOWITNESS | MTX_DUPOK | MTX_NOPROFILE)) == 0);

#ifdef MUTEX_DEBUG
	/* Diagnostic and error correction */
	mtx_validate(m);
#endif

	/* Determine lock class and lock flags. */
	if (opts & MTX_SPIN)
		class = &lock_class_mtx_spin;
	else
		class = &lock_class_mtx_sleep;
	flags = 0;
	if (opts & MTX_QUIET)
		flags |= LO_QUIET;
	if (opts & MTX_RECURSE)
		flags |= LO_RECURSABLE;
	if ((opts & MTX_NOWITNESS) == 0)
		flags |= LO_WITNESS;
	if (opts & MTX_DUPOK)
		flags |= LO_DUPOK;
	if (opts & MTX_NOPROFILE)
		flags |= LO_NOPROFILE;

	/* Initialize mutex. */
	m->mtx_lock = MTX_UNOWNED;
	m->mtx_recurse = 0;

	lock_profile_object_init(&m->mtx_object, class, name);
	lock_init(&m->mtx_object, class, name, type, flags);
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

	if (!mtx_owned(m))
		MPASS(mtx_unowned(m));
	else {
		MPASS((m->mtx_lock & (MTX_RECURSED|MTX_CONTESTED)) == 0);

		/* Perform the non-mtx related part of mtx_unlock_spin(). */
		if (LOCK_CLASS(&m->mtx_object) == &lock_class_mtx_spin)
			spinlock_exit();
		else
			curthread->td_locks--;

		/* Tell witness this isn't locked to make it happy. */
		WITNESS_UNLOCK(&m->mtx_object, LOP_EXCLUSIVE, __FILE__,
		    __LINE__);
	}

	m->mtx_lock = MTX_DESTROYED;
	lock_profile_object_destroy(&m->mtx_object);
	lock_destroy(&m->mtx_object);
}

/*
 * Intialize the mutex code and system mutexes.  This is called from the MD
 * startup code prior to mi_startup().  The per-CPU data space needs to be
 * setup before this is called.
 */
void
mutex_init(void)
{

	/* Setup turnstiles so that sleep mutexes work. */
	init_turnstiles();

	/*
	 * Initialize mutexes.
	 */
	mtx_init(&Giant, "Giant", NULL, MTX_DEF | MTX_RECURSE);
	mtx_init(&sched_lock, "sched lock", NULL, MTX_SPIN | MTX_RECURSE);
	mtx_init(&proc0.p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK);
	mtx_init(&devmtx, "cdev", NULL, MTX_DEF);
	mtx_lock(&Giant);
	
	lock_profile_init();
}

#ifdef DDB
void
db_show_mtx(struct lock_object *lock)
{
	struct thread *td;
	struct mtx *m;

	m = (struct mtx *)lock;

	db_printf(" flags: {");
	if (LOCK_CLASS(lock) == &lock_class_mtx_spin)
		db_printf("SPIN");
	else
		db_printf("DEF");
	if (m->mtx_object.lo_flags & LO_RECURSABLE)
		db_printf(", RECURSE");
	if (m->mtx_object.lo_flags & LO_DUPOK)
		db_printf(", DUPOK");
	db_printf("}\n");
	db_printf(" state: {");
	if (mtx_unowned(m))
		db_printf("UNOWNED");
	else {
		db_printf("OWNED");
		if (m->mtx_lock & MTX_CONTESTED)
			db_printf(", CONTESTED");
		if (m->mtx_lock & MTX_RECURSED)
			db_printf(", RECURSED");
	}
	db_printf("}\n");
	if (!mtx_unowned(m)) {
		td = mtx_owner(m);
		db_printf(" owner: %p (tid %d, pid %d, \"%s\")\n", td,
		    td->td_tid, td->td_proc->p_pid, td->td_proc->p_comm);
		if (mtx_recursed(m))
			db_printf(" recursed: %d\n", m->mtx_recurse);
	}
}
#endif
