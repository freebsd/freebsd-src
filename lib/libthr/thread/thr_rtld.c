/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

 /*
  * A lockless rwlock for rtld.
  */
#include <sys/mman.h>
#include <sys/syscall.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>

#include "libc_private.h"
#include "rtld_lock.h"
#include "thr_private.h"

extern int __libsys_errno;

static int	_thr_rtld_clr_flag(int);
static void	*_thr_rtld_lock_create(void);
static void	_thr_rtld_lock_destroy(void *);
static void	_thr_rtld_lock_release(void *);
static void	_thr_rtld_rlock_acquire(void *);
static int	_thr_rtld_set_flag(int);
static void	_thr_rtld_wlock_acquire(void *);

struct rtld_lock {
	struct urwlock lock;
	struct pthread *wowner;
	u_int rlocks;
	char _pad[CACHE_LINE_SIZE - sizeof(struct urwlock) -
	    sizeof(struct pthread *) - sizeof(u_int)];
};

static struct rtld_lock lock_place[MAX_RTLD_LOCKS] __aligned(CACHE_LINE_SIZE);
static int busy_places;

static void *
_thr_rtld_lock_create(void)
{
	int locki;
	struct rtld_lock *l;
	static const char fail[] = "_thr_rtld_lock_create failed\n";

	for (locki = 0; locki < MAX_RTLD_LOCKS; locki++) {
		if ((busy_places & (1 << locki)) == 0)
			break;
	}
	if (locki == MAX_RTLD_LOCKS) {
		write(2, fail, sizeof(fail) - 1);
		return (NULL);
	}
	busy_places |= (1 << locki);

	l = &lock_place[locki];
	l->lock.rw_flags = URWLOCK_PREFER_READER;
	return (l);
}

static void
_thr_rtld_lock_destroy(void *lock)
{
	int locki;
	size_t i;

	locki = (struct rtld_lock *)lock - &lock_place[0];
	for (i = 0; i < sizeof(struct rtld_lock); ++i)
		((char *)lock)[i] = 0;
	busy_places &= ~(1 << locki);
}

#define SAVE_ERRNO()	{			\
	if (curthread != _thr_initial)		\
		errsave = curthread->error;	\
	else					\
		errsave = __libsys_errno;	\
}

#define RESTORE_ERRNO()	{ 			\
	if (curthread != _thr_initial)  	\
		curthread->error = errsave;	\
	else					\
		__libsys_errno = errsave;	\
}

static void
_thr_rtld_rlock_acquire(void *lock)
{
	struct pthread		*curthread;
	struct rtld_lock	*l;
	int			errsave;

	curthread = _get_curthread();
	SAVE_ERRNO();
	l = (struct rtld_lock *)lock;

	if (l->wowner == curthread) {
		l->rlocks++;
	} else {
		THR_CRITICAL_ENTER(curthread);
		while (_thr_rwlock_rdlock(&l->lock, 0, NULL) != 0)
			;
	}
	curthread->rdlock_count++;
	RESTORE_ERRNO();
}

static void
_thr_rtld_wlock_acquire(void *lock)
{
	struct pthread		*curthread;
	struct rtld_lock	*l;
	int			errsave;

	curthread = _get_curthread();
	SAVE_ERRNO();
	l = (struct rtld_lock *)lock;

	THR_CRITICAL_ENTER(curthread);
	while (_thr_rwlock_wrlock(&l->lock, NULL) != 0)
		;
	l->wowner = curthread;
	RESTORE_ERRNO();
}

static void
_thr_rtld_lock_release(void *lock)
{
	struct pthread		*curthread;
	struct rtld_lock	*l;
	int32_t			state;
	int			errsave;

	curthread = _get_curthread();
	SAVE_ERRNO();
	l = (struct rtld_lock *)lock;
	
	state = l->lock.rw_state;
	if (__predict_false(_thr_after_fork)) {
		/*
		 * After fork, only this thread is running, there is no
		 * waiters.  Keeping waiters recorded in rwlock breaks
		 * wake logic.
		 */
		atomic_clear_int(&l->lock.rw_state,
		    URWLOCK_WRITE_WAITERS | URWLOCK_READ_WAITERS);
		l->lock.rw_blocked_readers = 0;
		l->lock.rw_blocked_writers = 0;
	}
	if ((state & URWLOCK_WRITE_OWNER) != 0) {
		if (l->rlocks > 0) {
			l->rlocks--;
			return;
		} else {
			l->wowner = NULL;
		}
	}
	if (_thr_rwlock_unlock(&l->lock) == 0) {
		if ((state & URWLOCK_WRITE_OWNER) == 0)
			curthread->rdlock_count--;
		THR_CRITICAL_LEAVE(curthread);
	}
	RESTORE_ERRNO();
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

/*
 * ABI bug workaround: This symbol must be present for rtld to accept
 * RTLI_VERSION from RtldLockInfo
 */
extern char _pli_rtli_version;
char _pli_rtli_version;

static char *
_thr_dlerror_loc(void)
{
	struct pthread *curthread;

	curthread = _get_curthread();
	return (curthread->dlerror_msg);
}

static int *
_thr_dlerror_seen(void)
{
	struct pthread *curthread;

	curthread = _get_curthread();
	return (&curthread->dlerror_seen);
}

void
_thr_rtld_init(void)
{
	struct RtldLockInfo	li;
	struct pthread		*curthread;
	ucontext_t *uc;
	int uc_len;
	char dummy[2] = {};

	curthread = _get_curthread();

	/* force to resolve _umtx_op PLT */
	_umtx_op_err(&dummy, UMTX_OP_WAKE, 1, 0, 0);
	
	/* force to resolve errno() PLT */
	__error();

	/* force to resolve memcpy PLT */
	memcpy(&dummy[0], &dummy[1], 1);

	mprotect(NULL, 0, 0);
	_rtld_get_stack_prot();
	thr_wake(-1);

	li.rtli_version = RTLI_VERSION;
	li.lock_create  = _thr_rtld_lock_create;
	li.lock_destroy = _thr_rtld_lock_destroy;
	li.rlock_acquire = _thr_rtld_rlock_acquire;
	li.wlock_acquire = _thr_rtld_wlock_acquire;
	li.lock_release  = _thr_rtld_lock_release;
	li.thread_set_flag = _thr_rtld_set_flag;
	li.thread_clr_flag = _thr_rtld_clr_flag;
	li.at_fork = NULL;
	li.dlerror_loc = _thr_dlerror_loc;
	li.dlerror_loc_sz = sizeof(curthread->dlerror_msg);
	li.dlerror_seen = _thr_dlerror_seen;

	/*
	 * Preresolve the symbols needed for the fork interposer.  We
	 * call _rtld_atfork_pre() and _rtld_atfork_post() with NULL
	 * argument to indicate that no actual locking inside the
	 * functions should happen.  Neither rtld compat locks nor
	 * libthr rtld locks cannot work there:
	 * - compat locks do not handle the case of two locks taken
	 *   in write mode (the signal mask for the thread is corrupted);
	 * - libthr locks would work, but locked rtld_bind_lock prevents
	 *   symbol resolution for _rtld_atfork_post.
	 */
	_rtld_atfork_pre(NULL);
	_rtld_atfork_post(NULL);
	_malloc_prefork();
	_malloc_postfork();
	getpid();
	syscall(SYS_getpid);

	/* mask signals, also force to resolve __sys_sigprocmask PLT */
	_thr_signal_block(curthread);
	_rtld_thread_init(&li);
	_thr_signal_unblock(curthread);
	_thr_signal_block_check_fast();
	_thr_signal_block_setup(curthread);

	/* resolve machine depended functions, if any */
	_thr_resolve_machdep();

	uc_len = __getcontextx_size();
	uc = alloca(uc_len);
	getcontext(uc);
	__fillcontextx2((char *)uc);
}
