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
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/condvar.h>	/* XXX */

struct sx {
	struct lock_object sx_object;	/* Common lock properties. */
	struct mtx	sx_lock;	/* General protection lock. */
	int		sx_cnt;		/* -1: xlock, > 0: slock count. */
	struct cv	sx_shrd_cv;	/* slock waiters. */
	int		sx_shrd_wcnt;	/* Number of slock waiters. */
	struct cv	sx_excl_cv;	/* xlock waiters. */
	int		sx_excl_wcnt;	/* Number of xlock waiters. */
	struct thread	*sx_xholder;	/* Thread presently holding xlock. */
};

#ifdef _KERNEL
void	sx_init(struct sx *sx, const char *description);
void	sx_destroy(struct sx *sx);
void	_sx_slock(struct sx *sx, const char *file, int line);
void	_sx_xlock(struct sx *sx, const char *file, int line);
int	_sx_try_slock(struct sx *sx, const char *file, int line);
int	_sx_try_xlock(struct sx *sx, const char *file, int line);
void	_sx_sunlock(struct sx *sx, const char *file, int line);
void	_sx_xunlock(struct sx *sx, const char *file, int line);
int	_sx_try_upgrade(struct sx *sx, const char *file, int line);
void	_sx_downgrade(struct sx *sx, const char *file, int line);
#ifdef INVARIANT_SUPPORT
void	_sx_assert(struct sx *sx, int what, const char *file, int line);
#endif

#define	sx_slock(sx)		_sx_slock((sx), LOCK_FILE, LOCK_LINE)
#define	sx_xlock(sx)		_sx_xlock((sx), LOCK_FILE, LOCK_LINE)
#define	sx_try_slock(sx)	_sx_try_slock((sx), LOCK_FILE, LOCK_LINE)
#define	sx_try_xlock(sx)	_sx_try_xlock((sx), LOCK_FILE, LOCK_LINE)
#define	sx_sunlock(sx)		_sx_sunlock((sx), LOCK_FILE, LOCK_LINE)
#define	sx_xunlock(sx)		_sx_xunlock((sx), LOCK_FILE, LOCK_LINE)
#define	sx_try_upgrade(sx)	_sx_try_upgrade((sx), LOCK_FILE, LOCK_LINE)
#define	sx_downgrade(sx)	_sx_downgrade((sx), LOCK_FILE, LOCK_LINE)

#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
#define	SX_LOCKED		LA_LOCKED
#define	SX_SLOCKED		LA_SLOCKED
#define	SX_XLOCKED		LA_XLOCKED
#endif	/* INVARIANTS || INVARIANT_SUPPORT */

#ifdef INVARIANTS
#define	sx_assert(sx, what)	_sx_assert((sx), (what), LOCK_FILE, LOCK_LINE)
#else	/* INVARIANTS */
#define	sx_assert(sx, what)
#endif	/* INVARIANTS */

#endif	/* _KERNEL */
#endif	/* !LOCORE */
#endif	/* _SYS_SX_H_ */
