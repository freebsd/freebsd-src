/*
 * Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_SYS_SX_H_
#define	_SYS_SX_H_

#ifndef	LOCORE
#include <sys/lock.h>		/* XXX */
#include <sys/mutex.h>		/* XXX */
#include <sys/condvar.h>	/* XXX */

struct lock_object;

struct sx {
	struct lock_object sx_object;	/* Common lock properties. */
	struct mtx	sx_lock;	/* General protection lock. */
	int		sx_cnt;		/* -1: xlock, > 0: slock count. */
	struct cv	sx_shrd_cv;	/* slock waiters. */
	int		sx_shrd_wcnt;	/* Number of slock waiters. */
	struct cv	sx_excl_cv;	/* xlock waiters. */
	int		sx_excl_wcnt;	/* Number of xlock waiters. */
	struct proc	*sx_xholder;	/* Thread presently holding xlock. */
};

#ifdef _KERNEL
void	sx_init(struct sx *sx, const char *description);
void	sx_destroy(struct sx *sx);
void	_sx_slock(struct sx *sx, const char *file, int line);
void	_sx_xlock(struct sx *sx, const char *file, int line);
void	_sx_sunlock(struct sx *sx, const char *file, int line);
void	_sx_xunlock(struct sx *sx, const char *file, int line);

#define	sx_slock(sx)	_sx_slock((sx), __FILE__, __LINE__)
#define	sx_xlock(sx)	_sx_xlock((sx), __FILE__, __LINE__)
#define	sx_sunlock(sx)	_sx_sunlock((sx), __FILE__, __LINE__)
#define	sx_xunlock(sx)	_sx_xunlock((sx), __FILE__, __LINE__)

#ifdef INVARIANTS
/*
 * SX_ASSERT_SLOCKED() can only detect that at least *some* thread owns an
 * slock, but it cannot guarantee that *this* thread owns an slock.
 */
#define	SX_ASSERT_SLOCKED(sx) do {					\
	mtx_lock(&(sx)->sx_lock);					\
	_SX_ASSERT_SLOCKED((sx));					\
	mtx_unlock(&(sx)->sx_lock);					\
} while (0)
#define	_SX_ASSERT_SLOCKED(sx) do {					\
	KASSERT(((sx)->sx_cnt > 0), ("%s: lacking slock %s\n",		\
	    __FUNCTION__, (sx)->sx_object.lo_name));			\
} while (0)

/*
 * SX_ASSERT_XLOCKED() detects and guarantees that *we* own the xlock.
 */
#define	SX_ASSERT_XLOCKED(sx) do {					\
	mtx_lock(&(sx)->sx_lock);					\
	_SX_ASSERT_XLOCKED((sx));					\
	mtx_unlock(&(sx)->sx_lock);					\
} while (0)
#define	_SX_ASSERT_XLOCKED(sx) do {					\
	KASSERT(((sx)->sx_xholder == curproc),				\
	    ("%s: thread %p lacking xlock %s\n", __FUNCTION__,		\
	    curproc, (sx)->sx_object.lo_name));				\
} while (0)

#else	/* INVARIANTS */
#define	SX_ASSERT_SLOCKED(sx)
#define	SX_ASSERT_XLOCKED(sx)
#define	_SX_ASSERT_SLOCKED(sx)
#define	_SX_ASSERT_XLOCKED(sx)
#endif	/* INVARIANTS */

#endif	/* _KERNEL */
#endif	/* !LOCORE */
#endif	/* _SYS_SX_H_ */
