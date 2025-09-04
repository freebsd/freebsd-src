/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2025 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 */

#ifndef	_LINUXKPI_LINUX_CLEANUP_H
#define	_LINUXKPI_LINUX_CLEANUP_H

#define	__cleanup(_f)		__attribute__((__cleanup__(_f)))

/*
 * Note: "_T" are special as they are exposed into common code for
 * statements.  Extra care should be taken when changing the code.
 */
#define	DEFINE_GUARD(_n, _dt, _lock, _unlock)				\
									\
    typedef _dt guard_ ## _n ## _t;					\
									\
    static inline _dt							\
    guard_ ## _n ## _create( _dt _T)					\
    {									\
	_dt c;								\
									\
	c = ({ _lock; _T; });						\
	return (c);							\
    }									\
									\
    static inline void							\
    guard_ ## _n ## _destroy(_dt *t)					\
    {									\
	_dt _T;								\
									\
	_T = *t;							\
	if (_T) { _unlock; };						\
    }

/* We need to keep these calls unique. */
#define	guard(_n)							\
    guard_ ## _n ## _t guard_ ## _n ## _ ## __COUNTER__			\
	__cleanup(guard_ ## _n ## _destroy) = guard_ ## _n ## _create

#define	DEFINE_FREE(_n, _t, _f)						\
    static inline void							\
    __free_ ## _n(void *p)						\
    {									\
	_t _T;								\
									\
	_T = *(_t *)p;							\
	_f;								\
    }

#define	__free(_n)		__cleanup(__free_##_n)

/*
 * Given this is a _0 version it should likely be broken up into parts.
 * But we have no idead what a _1, _2, ... version would do different
 * until we see a call.
 * This is used for a not-real-type (rcu).   We use a bool to "simulate"
 * the lock held.  Also _T still special, may not always be used, so tag
 * with __unused (or better the LinuxKPI __maybe_unused).
 */
#define	DEFINE_LOCK_GUARD_0(_n, _lock, _unlock, ...)			\
									\
    typedef struct {							\
	bool lock;							\
	__VA_ARGS__;							\
    } guard_ ## _n ## _t;	    					\
									\
    static inline void							\
    guard_ ## _n ## _destroy(guard_ ## _n ## _t *_T)			\
    {									\
	if (_T->lock) {							\
	    _unlock;							\
	}								\
    }									\
									\
    static inline guard_ ## _n ## _t					\
    guard_ ## _n ## _create(void)					\
    {									\
	guard_ ## _n ## _t _tmp;					\
	guard_ ## _n ## _t *_T __maybe_unused;				\
									\
	_tmp.lock = true;						\
	_T = &_tmp;							\
	_lock;								\
	return (_tmp);							\
    }

#endif	/* _LINUXKPI_LINUX_CLEANUP_H */
