/*
 * Copyright (c) 2001 Alexander Kabaev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */
#include <sys/cdefs.h>
#include <stdlib.h>

#include "rtld_lock.h"
#include "thr_private.h"

static int	_thr_rtld_clr_flag(int);
static void	*_thr_rtld_lock_create(void);
static void	_thr_rtld_lock_destroy(void *);
static void	_thr_rtld_lock_release(void *);
static void	_thr_rtld_rlock_acquire(void *);
static int	_thr_rtld_set_flag(int);
static void	_thr_rtld_wlock_acquire(void *);

#ifdef NOTYET
static void *
_thr_rtld_lock_create(void)
{
	pthread_rwlock_t prwlock;
	if (_pthread_rwlock_init(&prwlock, NULL))
		return (NULL);
	return (prwlock);
}

static void
_thr_rtld_lock_destroy(void *lock)
{
	pthread_rwlock_t prwlock;

	prwlock = (pthread_rwlock_t)lock;
	if (prwlock != NULL)
		_pthread_rwlock_destroy(&prwlock);
}

static void
_thr_rtld_rlock_acquire(void *lock)
{
	pthread_rwlock_t prwlock;

	prwlock = (pthread_rwlock_t)lock;
	_thr_rwlock_rdlock(&prwlock);
}

static void
_thr_rtld_wlock_acquire(void *lock)
{
	pthread_rwlock_t prwlock;

	prwlock = (pthread_rwlock_t)lock;
	_thr_rwlock_wrlock(&prwlock);
}

static void
_thr_rtld_lock_release(void *lock)
{
	pthread_rwlock_t prwlock;

	prwlock = (pthread_rwlock_t)lock;
	_thr_rwlock_unlock(&prwlock);
}


static int
_thr_rtld_set_flag(int mask)
{
	struct pthread *curthread;
	int bits;

	curthread = _get_curthread();
	if (curthread != NULL) {
		bits = curthread->rtld_bits;
		curthread->rtld_bits |= mask;
	} else {
		bits = 0;
		PANIC("No current thread in rtld call");
	}

	return (bits);
}

static int
_thr_rtld_clr_flag(int mask)
{
	struct pthread *curthread;
	int bits;

	curthread = _get_curthread();
	if (curthread != NULL) {
		bits = curthread->rtld_bits;
		curthread->rtld_bits &= ~mask;
	} else {
		bits = 0;
		PANIC("No current thread in rtld call");
	}
	return (bits);
}

void
_thr_rtld_init(void)
{
	struct RtldLockInfo li;

	li.lock_create  = _thr_rtld_lock_create;
	li.lock_destroy = _thr_rtld_lock_destroy;
	li.rlock_acquire = _thr_rtld_rlock_acquire;
	li.wlock_acquire = _thr_rtld_wlock_acquire;
	li.lock_release  = _thr_rtld_lock_release;
	li.thread_set_flag = _thr_rtld_set_flag;
	li.thread_clr_flag = _thr_rtld_clr_flag;
	li.at_fork = NULL;
	_rtld_thread_init(&li);
}

void
_thr_rtld_fini(void)
{
	_rtld_thread_init(NULL);
}
#endif

struct rtld_kse_lock {
	struct lock	lck;
	struct kse	*owner;
	kse_critical_t	crit;
	int		count;
	int		write;
};

static void *
_thr_rtld_lock_create(void)
{
	struct rtld_kse_lock *l;

	if ((l = malloc(sizeof(struct rtld_kse_lock))) != NULL) {
		_lock_init(&l->lck, LCK_ADAPTIVE, _kse_lock_wait,
		    _kse_lock_wakeup, calloc);
		l->owner = NULL;
		l->count = 0;
		l->write = 0;
	}
	return (l);
}

static void
_thr_rtld_lock_destroy(void *lock __unused)
{
	/* XXX We really can not free memory after a fork() */
#if 0
	struct rtld_kse_lock *l;

	l = (struct rtld_kse_lock *)lock;
	_lock_destroy(&l->lck);
	free(l);
#endif
	return;
}

static void
_thr_rtld_rlock_acquire(void *lock)
{
	struct rtld_kse_lock *l;
	kse_critical_t crit;
	struct kse *curkse;

	l  = (struct rtld_kse_lock *)lock;
	crit = _kse_critical_enter();
	curkse = _get_curkse();
	if (l->owner == curkse) {
		l->count++;
		_kse_critical_leave(crit);	/* probably not necessary */
	} else {
		KSE_LOCK_ACQUIRE(curkse, &l->lck);
		l->crit = crit;
		l->owner = curkse;
		l->count = 1;
		l->write = 0;
	}
}

static void
_thr_rtld_wlock_acquire(void *lock)
{
	struct rtld_kse_lock *l;
	kse_critical_t crit;
	struct kse *curkse;

	l = (struct rtld_kse_lock *)lock;
	crit = _kse_critical_enter();
	curkse = _get_curkse();
	if (l->owner == curkse) {
		_kse_critical_leave(crit);
		PANIC("Recursive write lock attempt on rtld lock");
	} else {
		KSE_LOCK_ACQUIRE(curkse, &l->lck);
		l->crit = crit;
		l->owner = curkse;
		l->count = 1;
		l->write = 1;
	}
}

static void
_thr_rtld_lock_release(void *lock)
{
	struct rtld_kse_lock *l;
	kse_critical_t crit;
	struct kse *curkse;

	l = (struct rtld_kse_lock *)lock;
	crit = _kse_critical_enter();
	curkse = _get_curkse();
	if (l->owner != curkse) {
		/*
		 * We might want to forcibly unlock the rtld lock
		 * and/or disable threaded mode so there is better
		 * chance that the panic will work.  Otherwise,
		 * we could end up trying to take the rtld lock
		 * again.
		 */
		_kse_critical_leave(crit);
		PANIC("Attempt to unlock rtld lock when not owner.");
	} else {
		l->count--;
		if (l->count == 0) {
			/*
			 * If there ever is a count associated with
			 * _kse_critical_leave(), we'll need to add
			 * another call to it here with the crit
			 * value from above.
			 */
			crit  = l->crit;
			l->owner = NULL;
			l->write = 0;
			KSE_LOCK_RELEASE(curkse, &l->lck);
		}
		_kse_critical_leave(crit);
	}
}


static int
_thr_rtld_set_flag(int mask __unused)
{
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
	struct RtldLockInfo li;

	li.lock_create  = _thr_rtld_lock_create;
	li.lock_destroy = _thr_rtld_lock_destroy;
	li.rlock_acquire = _thr_rtld_rlock_acquire;
	li.wlock_acquire = _thr_rtld_wlock_acquire;
	li.lock_release  = _thr_rtld_lock_release;
	li.thread_set_flag = _thr_rtld_set_flag;
	li.thread_clr_flag = _thr_rtld_clr_flag;
	li.at_fork = NULL;
	_rtld_thread_init(&li);
}

void
_thr_rtld_fini(void)
{
	_rtld_thread_init(NULL);
}
