/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 1999, 2000 John D. Polstra.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: FreeBSD: src/libexec/rtld-elf/sparc64/lockdflt.c,v 1.3 2002/10/09
 * $FreeBSD$
 */

/*
 * Thread locking implementation for the dynamic linker.
 *
 * We use the "simple, non-scalable reader-preference lock" from:
 *
 *   J. M. Mellor-Crummey and M. L. Scott. "Scalable Reader-Writer
 *   Synchronization for Shared-Memory Multiprocessors." 3rd ACM Symp. on
 *   Principles and Practice of Parallel Programming, April 1991.
 *
 * In this algorithm the lock is a single word.  Its low-order bit is
 * set when a writer holds the lock.  The remaining high-order bits
 * contain a count of readers desiring the lock.  The algorithm requires
 * atomic "compare_and_store" and "add" operations, which we take
 * from machine/atomic.h.
 */

#include <sys/param.h>
#include <sys/signalvar.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#include "debug.h"
#include "rtld.h"
#include "rtld_machdep.h"
#include "rtld_libc.h"

void _rtld_thread_init(struct RtldLockInfo *) __exported;
void _rtld_atfork_pre(int *) __exported;
void _rtld_atfork_post(int *) __exported;

static char def_dlerror_msg[512];
static int def_dlerror_seen_val = 1;

static char *
def_dlerror_loc(void)
{
	return (def_dlerror_msg);
}

static int *
def_dlerror_seen(void)
{
	return (&def_dlerror_seen_val);
}

#define WAFLAG		0x1	/* A writer holds the lock */
#define RC_INCR		0x2	/* Adjusts count of readers desiring lock */

typedef struct Struct_Lock {
	volatile u_int lock;
	void *base;
} Lock;

static sigset_t fullsigmask, oldsigmask;
static int thread_flag, wnested;
static uint32_t fsigblock;

static void *
def_lock_create(void)
{
	void *base;
	char *p;
	uintptr_t r;
	Lock *l;

	/*
	 * Arrange for the lock to occupy its own cache line.  First, we
	 * optimistically allocate just a cache line, hoping that malloc
	 * will give us a well-aligned block of memory.  If that doesn't
	 * work, we allocate a larger block and take a well-aligned cache
	 * line from it.
	 */
	base = xmalloc(CACHE_LINE_SIZE);
	p = base;
	if ((uintptr_t)p % CACHE_LINE_SIZE != 0) {
		free(base);
		base = xmalloc(2 * CACHE_LINE_SIZE);
		p = base;
		if ((r = (uintptr_t)p % CACHE_LINE_SIZE) != 0)
			p += CACHE_LINE_SIZE - r;
	}
	l = (Lock *)p;
	l->base = base;
	l->lock = 0;
	return (l);
}

static void
def_lock_destroy(void *lock)
{
	Lock *l = lock;

	free(l->base);
}

static void
sig_fastunblock(void)
{
	uint32_t oldval;

	assert((fsigblock & ~SIGFASTBLOCK_FLAGS) >= SIGFASTBLOCK_INC);
	oldval = atomic_fetchadd_32(&fsigblock, -SIGFASTBLOCK_INC);
	if (oldval == (SIGFASTBLOCK_PEND | SIGFASTBLOCK_INC))
		__sys_sigfastblock(SIGFASTBLOCK_UNBLOCK, NULL);
}

static bool
def_lock_acquire_set(Lock *l, bool wlock)
{
	if (wlock) {
		if (atomic_cmpset_acq_int(&l->lock, 0, WAFLAG))
			return (true);
	} else {
		atomic_add_acq_int(&l->lock, RC_INCR);
		if ((l->lock & WAFLAG) == 0)
			return (true);
		atomic_add_int(&l->lock, -RC_INCR);
	}
	return (false);
}

static void
def_lock_acquire(Lock *l, bool wlock)
{
	sigset_t tmp_oldsigmask;

	if (ld_fast_sigblock) {
		for (;;) {
			atomic_add_32(&fsigblock, SIGFASTBLOCK_INC);
			if (def_lock_acquire_set(l, wlock))
				break;
			sig_fastunblock();
		}
	} else {
		for (;;) {
			sigprocmask(SIG_BLOCK, &fullsigmask, &tmp_oldsigmask);
			if (def_lock_acquire_set(l, wlock))
				break;
			sigprocmask(SIG_SETMASK, &tmp_oldsigmask, NULL);
		}
		if (atomic_fetchadd_int(&wnested, 1) == 0)
			oldsigmask = tmp_oldsigmask;
	}
}

static void
def_rlock_acquire(void *lock)
{
	def_lock_acquire(lock, false);
}

static void
def_wlock_acquire(void *lock)
{
	def_lock_acquire(lock, true);
}

static void
def_lock_release(void *lock)
{
	Lock *l = lock;

	atomic_add_rel_int(&l->lock, -((l->lock & WAFLAG) == 0 ?
	    RC_INCR : WAFLAG));
	if (ld_fast_sigblock)
		sig_fastunblock();
	else if (atomic_fetchadd_int(&wnested, -1) == 1)
		sigprocmask(SIG_SETMASK, &oldsigmask, NULL);
}

static int
def_thread_set_flag(int mask)
{
	int old_val = thread_flag;

	thread_flag |= mask;
	return (old_val);
}

static int
def_thread_clr_flag(int mask)
{
	int old_val = thread_flag;

	thread_flag &= ~mask;
	return (old_val);
}

/*
 * Public interface exposed to the rest of the dynamic linker.
 */
struct RtldLockInfo lockinfo;
static struct RtldLockInfo deflockinfo;

static __inline int
thread_mask_set(int mask)
{
	return (lockinfo.thread_set_flag(mask));
}

static __inline void
thread_mask_clear(int mask)
{
	lockinfo.thread_clr_flag(mask);
}

#define	RTLD_LOCK_CNT	3
static struct rtld_lock {
	void	*handle;
	int	 mask;
} rtld_locks[RTLD_LOCK_CNT];

rtld_lock_t	rtld_bind_lock = &rtld_locks[0];
rtld_lock_t	rtld_libc_lock = &rtld_locks[1];
rtld_lock_t	rtld_phdr_lock = &rtld_locks[2];

void
rlock_acquire(rtld_lock_t lock, RtldLockState *lockstate)
{

	if (lockstate == NULL)
		return;

	if (thread_mask_set(lock->mask) & lock->mask) {
		dbg("rlock_acquire: recursed");
		lockstate->lockstate = RTLD_LOCK_UNLOCKED;
		return;
	}
	lockinfo.rlock_acquire(lock->handle);
	lockstate->lockstate = RTLD_LOCK_RLOCKED;
}

void
wlock_acquire(rtld_lock_t lock, RtldLockState *lockstate)
{

	if (lockstate == NULL)
		return;

	if (thread_mask_set(lock->mask) & lock->mask) {
		dbg("wlock_acquire: recursed");
		lockstate->lockstate = RTLD_LOCK_UNLOCKED;
		return;
	}
	lockinfo.wlock_acquire(lock->handle);
	lockstate->lockstate = RTLD_LOCK_WLOCKED;
}

void
lock_release(rtld_lock_t lock, RtldLockState *lockstate)
{

	if (lockstate == NULL)
		return;

	switch (lockstate->lockstate) {
	case RTLD_LOCK_UNLOCKED:
		break;
	case RTLD_LOCK_RLOCKED:
	case RTLD_LOCK_WLOCKED:
		thread_mask_clear(lock->mask);
		lockinfo.lock_release(lock->handle);
		break;
	default:
		assert(0);
	}
}

void
lock_upgrade(rtld_lock_t lock, RtldLockState *lockstate)
{

	if (lockstate == NULL)
		return;

	lock_release(lock, lockstate);
	wlock_acquire(lock, lockstate);
}

void
lock_restart_for_upgrade(RtldLockState *lockstate)
{

	if (lockstate == NULL)
		return;

	switch (lockstate->lockstate) {
	case RTLD_LOCK_UNLOCKED:
	case RTLD_LOCK_WLOCKED:
		break;
	case RTLD_LOCK_RLOCKED:
		siglongjmp(lockstate->env, 1);
		break;
	default:
		assert(0);
	}
}

void
dlerror_dflt_init(void)
{
	lockinfo.dlerror_loc = def_dlerror_loc;
	lockinfo.dlerror_loc_sz = sizeof(def_dlerror_msg);
	lockinfo.dlerror_seen = def_dlerror_seen;
}

void
lockdflt_init(void)
{
	int i;

	deflockinfo.rtli_version = RTLI_VERSION;
	deflockinfo.lock_create = def_lock_create;
	deflockinfo.lock_destroy = def_lock_destroy;
	deflockinfo.rlock_acquire = def_rlock_acquire;
	deflockinfo.wlock_acquire = def_wlock_acquire;
	deflockinfo.lock_release = def_lock_release;
	deflockinfo.thread_set_flag = def_thread_set_flag;
	deflockinfo.thread_clr_flag = def_thread_clr_flag;
	deflockinfo.at_fork = NULL;
	deflockinfo.dlerror_loc = def_dlerror_loc;
	deflockinfo.dlerror_loc_sz = sizeof(def_dlerror_msg);
	deflockinfo.dlerror_seen = def_dlerror_seen;

	for (i = 0; i < RTLD_LOCK_CNT; i++) {
		rtld_locks[i].mask   = (1 << i);
		rtld_locks[i].handle = NULL;
	}

	memcpy(&lockinfo, &deflockinfo, sizeof(lockinfo));
	_rtld_thread_init(NULL);
	if (ld_fast_sigblock) {
		__sys_sigfastblock(SIGFASTBLOCK_SETPTR, &fsigblock);
	} else {
		/*
		 * Construct a mask to block all signals.  Note that
		 * blocked traps mean that the process is terminated
		 * if trap occurs while we are in locked section, with
		 * the default settings for kern.forcesigexit.
		 */
		sigfillset(&fullsigmask);
	}
}

/*
 * Callback function to allow threads implementation to
 * register their own locking primitives if the default
 * one is not suitable.
 * The current context should be the only context
 * executing at the invocation time.
 */
void
_rtld_thread_init(struct RtldLockInfo *pli)
{
	const Obj_Entry *obj;
	SymLook req;
	void *locks[RTLD_LOCK_CNT];
	int flags, i, res;

	if (pli == NULL) {
		lockinfo.rtli_version = RTLI_VERSION;
	} else {
		lockinfo.rtli_version = RTLI_VERSION_ONE;
		obj = obj_from_addr(pli->lock_create);
		if (obj != NULL) {
			symlook_init(&req, "_pli_rtli_version");
			res = symlook_obj(&req, obj);
			if (res == 0)
				lockinfo.rtli_version = pli->rtli_version;
		}
	}

	/* disable all locking while this function is running */
	flags =	thread_mask_set(~0);

	if (pli == NULL)
		pli = &deflockinfo;
	else if (ld_fast_sigblock) {
		fsigblock = 0;
		__sys_sigfastblock(SIGFASTBLOCK_UNSETPTR, NULL);
	}

	for (i = 0; i < RTLD_LOCK_CNT; i++)
		if ((locks[i] = pli->lock_create()) == NULL)
			break;

	if (i < RTLD_LOCK_CNT) {
		while (--i >= 0)
			pli->lock_destroy(locks[i]);
		abort();
	}

	for (i = 0; i < RTLD_LOCK_CNT; i++) {
		if (rtld_locks[i].handle == NULL)
			continue;
		if (flags & rtld_locks[i].mask)
			lockinfo.lock_release(rtld_locks[i].handle);
		lockinfo.lock_destroy(rtld_locks[i].handle);
	}

	for (i = 0; i < RTLD_LOCK_CNT; i++) {
		rtld_locks[i].handle = locks[i];
		if (flags & rtld_locks[i].mask)
			pli->wlock_acquire(rtld_locks[i].handle);
	}

	lockinfo.lock_create = pli->lock_create;
	lockinfo.lock_destroy = pli->lock_destroy;
	lockinfo.rlock_acquire = pli->rlock_acquire;
	lockinfo.wlock_acquire = pli->wlock_acquire;
	lockinfo.lock_release  = pli->lock_release;
	lockinfo.thread_set_flag = pli->thread_set_flag;
	lockinfo.thread_clr_flag = pli->thread_clr_flag;
	lockinfo.at_fork = pli->at_fork;
	if (lockinfo.rtli_version > RTLI_VERSION_ONE && pli != NULL) {
		strlcpy(pli->dlerror_loc(), lockinfo.dlerror_loc(),
		    lockinfo.dlerror_loc_sz);
		lockinfo.dlerror_loc = pli->dlerror_loc;
		lockinfo.dlerror_loc_sz = pli->dlerror_loc_sz;
		lockinfo.dlerror_seen = pli->dlerror_seen;
	}

	/* restore thread locking state, this time with new locks */
	thread_mask_clear(~0);
	thread_mask_set(flags);
	dbg("_rtld_thread_init: done");
}

void
_rtld_atfork_pre(int *locks)
{
	RtldLockState ls[2];

	if (locks == NULL)
		return;

	/*
	 * Warning: this did not worked well with the rtld compat
	 * locks above, when the thread signal mask was corrupted (set
	 * to all signals blocked) if two locks were taken
	 * simultaneously in the write mode.  The caller of the
	 * _rtld_atfork_pre() must provide the working implementation
	 * of the locks anyway, and libthr locks are fine.
	 */
	wlock_acquire(rtld_phdr_lock, &ls[0]);
	wlock_acquire(rtld_bind_lock, &ls[1]);

	/* XXXKIB: I am really sorry for this. */
	locks[0] = ls[1].lockstate;
	locks[2] = ls[0].lockstate;
}

void
_rtld_atfork_post(int *locks)
{
	RtldLockState ls[2];

	if (locks == NULL)
		return;

	bzero(ls, sizeof(ls));
	ls[0].lockstate = locks[2];
	ls[1].lockstate = locks[0];
	lock_release(rtld_bind_lock, &ls[1]);
	lock_release(rtld_phdr_lock, &ls[0]);
}
