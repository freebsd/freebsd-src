/*
 * Copyright (c) 2006, David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD$
 *
 */

 /*
  * A lockless rwlock for rtld.
  */
#include <sys/cdefs.h>
#include <stdlib.h>

#include "rtld_lock.h"
#include "thr_private.h"

#define CACHE_LINE_SIZE		64
#define WAFLAG			0x1
#define RC_INCR			0x2

static int	_thr_rtld_clr_flag(int);
static void	*_thr_rtld_lock_create(void);
static void	_thr_rtld_lock_destroy(void *);
static void	_thr_rtld_lock_release(void *);
static void	_thr_rtld_rlock_acquire(void *);
static int	_thr_rtld_set_flag(int);
static void	_thr_rtld_wlock_acquire(void *);

struct rtld_lock {
	volatile int		lock;
	volatile int		rd_waiters;
	volatile int		wr_waiters;
	volatile long		rd_cv;
	volatile long		wr_cv;
	void			*base;
};

static void *
_thr_rtld_lock_create(void)
{
	void			*base;
	char			*p;
	uintptr_t		r;
	struct rtld_lock	*l;

	THR_ASSERT(sizeof(struct rtld_lock) <= CACHE_LINE_SIZE,
		"rtld_lock too large");
	base = calloc(1, CACHE_LINE_SIZE);
	p = (char *)base;
	if ((uintptr_t)p % CACHE_LINE_SIZE != 0) {
		free(base);
		base = calloc(1, 2 * CACHE_LINE_SIZE);
		p = (char *)base;
		if ((r = (uintptr_t)p % CACHE_LINE_SIZE) != 0)
			p += CACHE_LINE_SIZE - r;
	}
	l = (struct rtld_lock *)p;
	l->base = base;
	return (l);
}

static void
_thr_rtld_lock_destroy(void *lock)
{
	struct rtld_lock *l = (struct rtld_lock *)lock;
	free(l->base);
}

static void
_thr_rtld_rlock_acquire(void *lock)
{
	struct pthread		*curthread;
	struct rtld_lock	*l;
	long			v;

	curthread = _get_curthread();
	l = (struct rtld_lock *)lock;

	THR_CRITICAL_ENTER(curthread);
	atomic_add_acq_int(&l->lock, RC_INCR);
	if (!(l->lock & WAFLAG))
		return;
	v = l->rd_cv;
	atomic_add_int(&l->rd_waiters, 1);
	while (l->lock & WAFLAG) {
		_thr_umtx_wait(&l->rd_cv, v, NULL);
		v = l->rd_cv;
	}
	atomic_add_int(&l->rd_waiters, -1);
}

static void
_thr_rtld_wlock_acquire(void *lock)
{
	struct pthread		*curthread;
	struct rtld_lock	*l;
	long			v;

	curthread = _get_curthread();
	l = (struct rtld_lock *)lock;

	_thr_signal_block(curthread);
	for (;;) {
		if (atomic_cmpset_acq_int(&l->lock, 0, WAFLAG))
			return;
		v = l->wr_cv;
		atomic_add_int(&l->wr_waiters, 1);
		while (l->lock != 0) {
			_thr_umtx_wait(&l->wr_cv, v, NULL);
			v = l->wr_cv;
		}
		atomic_add_int(&l->wr_waiters, -1);
	}
}

static void
_thr_rtld_lock_release(void *lock)
{
	struct pthread		*curthread;
	struct rtld_lock	*l;

	curthread = _get_curthread();
	l = (struct rtld_lock *)lock;
	
	if ((l->lock & WAFLAG) == 0) {
		atomic_add_rel_int(&l->lock, -RC_INCR);
		if (l->lock == 0 && l->wr_waiters) {
			atomic_add_long(&l->wr_cv, 1);
			_thr_umtx_wake(&l->wr_cv, l->wr_waiters);
		}
		THR_CRITICAL_LEAVE(curthread);
	} else {
		atomic_add_rel_int(&l->lock, -WAFLAG);
		if (l->lock == 0 && l->wr_waiters) {
			atomic_add_long(&l->wr_cv, 1);
			_thr_umtx_wake(&l->wr_cv, l->wr_waiters);
		} else if (l->rd_waiters) {
			atomic_add_long(&l->rd_cv, 1);
			_thr_umtx_wake(&l->rd_cv, l->rd_waiters);
		}
		_thr_signal_unblock(curthread);
	}
}

static int
_thr_rtld_set_flag(int mask __unused)
{
	/*
	 * The caller's code in rtld-elf is broken, it is not signal safe,
	 * just return zero to fool it.
	 */
	return (0);
}

static int
_thr_rtld_clr_flag(int mask __unused)
{
	return (0);
}

void
_thr_rtld_init(void)
{
	struct RtldLockInfo	li;
	struct pthread		*curthread;
	long dummy;

	curthread = _get_curthread();

	/* force to resolve _umtx_op PLT */
	_umtx_op((struct umtx *)&dummy, UMTX_OP_WAKE, 1, 0, 0);

	li.lock_create  = _thr_rtld_lock_create;
	li.lock_destroy = _thr_rtld_lock_destroy;
	li.rlock_acquire = _thr_rtld_rlock_acquire;
	li.wlock_acquire = _thr_rtld_wlock_acquire;
	li.lock_release  = _thr_rtld_lock_release;
	li.thread_set_flag = _thr_rtld_set_flag;
	li.thread_clr_flag = _thr_rtld_clr_flag;
	li.at_fork = NULL;
	
	/* mask signals, also force to resolve __sys_sigprocmask PLT */
	_thr_signal_block(curthread);
	_rtld_thread_init(&li);
	_thr_signal_unblock(curthread);
}

void
_thr_rtld_fini(void)
{
	struct pthread	*curthread;

	curthread = _get_curthread();
	_thr_signal_block(curthread);
	_rtld_thread_init(NULL);
	_thr_signal_unblock(curthread);
}
