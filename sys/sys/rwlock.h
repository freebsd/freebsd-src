/*-
 * Copyright (c) 2006 John Baldwin <jhb@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_RWLOCK_H_
#define _SYS_RWLOCK_H_

#include <sys/_lock.h>
#include <sys/_rwlock.h>
#include <sys/lock_profile.h>

#ifdef _KERNEL
#include <sys/pcpu.h>
#include <machine/atomic.h>
#endif

/*
 * The rw_lock field consists of several fields.  The low bit indicates
 * if the lock is locked with a read (shared) or write (exclusive) lock.
 * A value of 0 indicates a write lock, and a value of 1 indicates a read
 * lock.  Bit 1 is a boolean indicating if there are any threads waiting
 * for a read lock.  Bit 2 is a boolean indicating if there are any threads
 * waiting for a write lock.  The rest of the variable's definition is
 * dependent on the value of the first bit.  For a write lock, it is a
 * pointer to the thread holding the lock, similar to the mtx_lock field of
 * mutexes.  For read locks, it is a count of read locks that are held.
 *
 * When the lock is not locked by any thread, it is encoded as a read lock
 * with zero waiters.
 *
 * A note about memory barriers.  Write locks need to use the same memory
 * barriers as mutexes: _acq when acquiring a write lock and _rel when
 * releasing a write lock.  Read locks also need to use an _acq barrier when
 * acquiring a read lock.  However, since read locks do not update any
 * locked data (modulo bugs of course), no memory barrier is needed when
 * releasing a read lock.
 */

#define	RW_LOCK_READ		0x01
#define	RW_LOCK_READ_WAITERS	0x02
#define	RW_LOCK_WRITE_WAITERS	0x04
#define	RW_LOCK_RECURSED	0x08
#define	RW_LOCK_FLAGMASK						\
	(RW_LOCK_READ | RW_LOCK_READ_WAITERS | RW_LOCK_WRITE_WAITERS |	\
	RW_LOCK_RECURSED)

#define	RW_OWNER(x)		((x) & ~RW_LOCK_FLAGMASK)
#define	RW_READERS_SHIFT	4
#define	RW_READERS(x)		(RW_OWNER((x)) >> RW_READERS_SHIFT)
#define	RW_READERS_LOCK(x)	((x) << RW_READERS_SHIFT | RW_LOCK_READ)
#define	RW_ONE_READER		(1 << RW_READERS_SHIFT)

#define	RW_UNLOCKED		RW_READERS_LOCK(0)
#define	RW_DESTROYED		(RW_LOCK_READ_WAITERS | RW_LOCK_WRITE_WAITERS)

#ifdef _KERNEL

/* Very simple operations on rw_lock. */

/* Try to obtain a write lock once. */
#define	_rw_write_lock(rw, tid)						\
	atomic_cmpset_acq_ptr(&(rw)->rw_lock, RW_UNLOCKED, (tid))

/* Release a write lock quickly if there are no waiters. */
#define	_rw_write_unlock(rw, tid)					\
	atomic_cmpset_rel_ptr(&(rw)->rw_lock, (tid), RW_UNLOCKED)

/*
 * Full lock operations that are suitable to be inlined in non-debug
 * kernels.  If the lock cannot be acquired or released trivially then
 * the work is deferred to another function.
 */

/* Acquire a write lock. */
#define	__rw_wlock(rw, tid, file, line) do {				\
	uintptr_t _tid = (uintptr_t)(tid);				\
						                        \
	if (!_rw_write_lock((rw), _tid))				\
		_rw_wlock_hard((rw), _tid, (file), (line));		\
	else								\
		lock_profile_obtain_lock_success(&(rw)->lock_object, 0,	\
		    0, (file), (line));					\
} while (0)

/* Release a write lock. */
#define	__rw_wunlock(rw, tid, file, line) do {				\
	uintptr_t _tid = (uintptr_t)(tid);				\
									\
	if (!_rw_write_unlock((rw), _tid))				\
		_rw_wunlock_hard((rw), _tid, (file), (line));		\
} while (0)

/*
 * Function prototypes.  Routines that start with _ are not part of the
 * external API and should not be called directly.  Wrapper macros should
 * be used instead.
 */

#define	rw_init(rw, name)	rw_init_flags((rw), (name), 0)
void	rw_init_flags(struct rwlock *rw, const char *name, int opts);
void	rw_destroy(struct rwlock *rw);
void	rw_sysinit(void *arg);
int	rw_wowned(struct rwlock *rw);
void	_rw_wlock(struct rwlock *rw, const char *file, int line);
void	_rw_wunlock(struct rwlock *rw, const char *file, int line);
void	_rw_rlock(struct rwlock *rw, const char *file, int line);
void	_rw_runlock(struct rwlock *rw, const char *file, int line);
void	_rw_wlock_hard(struct rwlock *rw, uintptr_t tid, const char *file,
	    int line);
void	_rw_wunlock_hard(struct rwlock *rw, uintptr_t tid, const char *file,
	    int line);
int	_rw_try_upgrade(struct rwlock *rw, const char *file, int line);
void	_rw_downgrade(struct rwlock *rw, const char *file, int line);
#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
void	_rw_assert(struct rwlock *rw, int what, const char *file, int line);
#endif

/*
 * Public interface for lock operations.
 *
 * XXX: Missing try locks.
 */

#ifndef LOCK_DEBUG
#error LOCK_DEBUG not defined, include <sys/lock.h> before <sys/rwlock.h>
#endif
#if LOCK_DEBUG > 0 || defined(RWLOCK_NOINLINE)
#define	rw_wlock(rw)		_rw_wlock((rw), LOCK_FILE, LOCK_LINE)
#define	rw_wunlock(rw)		_rw_wunlock((rw), LOCK_FILE, LOCK_LINE)
#else
#define	rw_wlock(rw)							\
	__rw_wlock((rw), curthread, LOCK_FILE, LOCK_LINE)
#define	rw_wunlock(rw)							\
	__rw_wunlock((rw), curthread, LOCK_FILE, LOCK_LINE)
#endif
#define	rw_rlock(rw)		_rw_rlock((rw), LOCK_FILE, LOCK_LINE)
#define	rw_runlock(rw)		_rw_runlock((rw), LOCK_FILE, LOCK_LINE)
#define	rw_try_upgrade(rw)	_rw_try_upgrade((rw), LOCK_FILE, LOCK_LINE)
#define	rw_downgrade(rw)	_rw_downgrade((rw), LOCK_FILE, LOCK_LINE)
#define	rw_sleep(chan, rw, pri, wmesg, timo)				\
	_sleep((chan), &(rw)->lock_object, (pri), (wmesg), (timo))

#define	rw_initialized(rw)	lock_initalized(&(rw)->lock_object)

struct rw_args {
	struct rwlock	*ra_rw;
	const char 	*ra_desc;
};

#define	RW_SYSINIT(name, rw, desc)					\
	static struct rw_args name##_args = {				\
		(rw),							\
		(desc),							\
	};								\
	SYSINIT(name##_rw_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    rw_sysinit, &name##_args);					\
	SYSUNINIT(name##_rw_sysuninit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    rw_destroy, (rw))

/*
 * Options passed to rw_init_flags().
 */
#define	RW_DUPOK	0x01
#define	RW_NOPROFILE	0x02
#define	RW_NOWITNESS	0x04
#define	RW_QUIET	0x08
#define	RW_RECURSE	0x10

/*
 * The INVARIANTS-enabled rw_assert() functionality.
 *
 * The constants need to be defined for INVARIANT_SUPPORT infrastructure
 * support as _rw_assert() itself uses them and the latter implies that
 * _rw_assert() must build.
 */
#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
#define	RA_LOCKED		LA_LOCKED
#define	RA_RLOCKED		LA_SLOCKED
#define	RA_WLOCKED		LA_XLOCKED
#define	RA_UNLOCKED		LA_UNLOCKED
#define	RA_RECURSED		LA_RECURSED
#define	RA_NOTRECURSED		LA_NOTRECURSED
#endif

#ifdef INVARIANTS
#define	rw_assert(rw, what)	_rw_assert((rw), (what), LOCK_FILE, LOCK_LINE)
#else
#define	rw_assert(rw, what)
#endif

#endif /* _KERNEL */
#endif /* !_SYS_RWLOCK_H_ */
