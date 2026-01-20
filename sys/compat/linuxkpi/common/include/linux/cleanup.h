/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2026 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 */

#ifndef	_LINUXKPI_LINUX_CLEANUP_H
#define	_LINUXKPI_LINUX_CLEANUP_H

#include <linux/err.h>

#define	CLEANUP_NAME(_n, _s)	__CONCAT(__CONCAT(cleanup_, _n), _s)

#define	__cleanup(_f)		__attribute__((__cleanup__(_f)))

#define	DECLARE(_n, _x)							\
    CLEANUP_NAME(_n, _t) _x __cleanup(CLEANUP_NAME(_n, _destroy)) =	\
	CLEANUP_NAME(_n, _create)

/*
 * Note: "_T" are special as they are exposed into common code for
 * statements.  Extra care should be taken when changing the code.
 */
#define	DEFINE_GUARD(_n, _dt, _lock, _unlock)				\
									\
    typedef _dt CLEANUP_NAME(_n, _t);					\
									\
    static inline _dt							\
    CLEANUP_NAME(_n, _create)( _dt _T)					\
    {									\
	_dt c;								\
									\
	c = ({ _lock; _T; });						\
	return (c);							\
    }									\
									\
    static inline void							\
    CLEANUP_NAME(_n, _destroy)(_dt *t)					\
    {									\
	_dt _T;								\
									\
	_T = *t;							\
	if (_T) { _unlock; };						\
    }

/* We need to keep these calls unique. */
#define	_guard(_n, _x)							\
    DECLARE(_n, _x)
#define	guard(_n)							\
    _guard(_n, guard_ ## _n ## _ ## __COUNTER__)

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
 * Our initial version go broken up.  Some simplifications like using
 * "bool" for the lock had to be changed to a more general type.
 * _T is still special and, like other bits, may not always be used,
 * so tag with __unused (or better the LinuxKPI __maybe_unused).
 */
#define	_DEFINE_LOCK_GUARD_0(_n, _lock)					\
    static inline CLEANUP_NAME(_n, _t)					\
    CLEANUP_NAME(_n, _create)(void)					\
    {									\
	CLEANUP_NAME(_n, _t) _tmp;					\
	CLEANUP_NAME(_n, _t) *_T __maybe_unused;			\
									\
	_tmp.lock = (void *)1;						\
	_T = &_tmp;							\
	_lock;								\
	return (_tmp);							\
    }

#define	_DEFINE_LOCK_GUARD_1(_n, _type, _lock)				\
    static inline CLEANUP_NAME(_n, _t)					\
    CLEANUP_NAME(_n, _create)(_type *l)					\
    {									\
	CLEANUP_NAME(_n, _t) _tmp;					\
	CLEANUP_NAME(_n, _t) *_T __maybe_unused;			\
									\
	_tmp.lock = l;							\
	_T = &_tmp;							\
	_lock;								\
	return (_tmp);							\
    }

#define	_GUARD_IS_ERR(_v)						\
    ({									\
	uintptr_t x = (uintptr_t)(void *)(_v);				\
	IS_ERR_VALUE(x);						\
    })

#define	__is_cond_ptr(_n)						\
    CLEANUP_NAME(_n, _is_cond)
#define	__guard_ptr(_n)							\
    CLEANUP_NAME(_n, _ptr)

#define	_DEFINE_CLEANUP_IS_CONDITIONAL(_n, _b)				\
    static const bool CLEANUP_NAME(_n, _is_cond) __maybe_unused = _b

#define	_DEFINE_GUARD_LOCK_PTR(_n, _lp)					\
    static inline void *						\
    CLEANUP_NAME(_n, _lock_ptr)(CLEANUP_NAME(_n, _t) *_T)		\
    {									\
	void *_p;							\
									\
	_p = (void *)(uintptr_t)*(_lp);					\
	if (IS_ERR(_p))							\
		_p = NULL;						\
	return (_p);							\
    }

#define	_DEFINE_UNLOCK_GUARD(_n, _type, _unlock, ...)			\
    typedef struct {							\
	_type *lock;							\
	__VA_ARGS__;							\
    } CLEANUP_NAME(_n, _t);	    					\
									\
    static inline void							\
    CLEANUP_NAME(_n, _destroy)(CLEANUP_NAME(_n, _t) *_T)		\
    {									\
	if (!_GUARD_IS_ERR(_T->lock)) {					\
	    _unlock;							\
	}								\
    }									\
									\
    _DEFINE_GUARD_LOCK_PTR(_n, &_T->lock)

#define	DEFINE_LOCK_GUARD_0(_n, _lock, _unlock, ...)			\
    _DEFINE_CLEANUP_IS_CONDITIONAL(_n, false);				\
    _DEFINE_UNLOCK_GUARD(_n, void, _unlock, __VA_ARGS__)		\
    _DEFINE_LOCK_GUARD_0(_n, _lock)

/* This allows the type to be set. */
#define	DEFINE_LOCK_GUARD_1(_n, _t, _lock, _unlock, ...)		\
    _DEFINE_CLEANUP_IS_CONDITIONAL(_n, false);				\
    _DEFINE_UNLOCK_GUARD(_n, _t, _unlock, __VA_ARGS__)			\
    _DEFINE_LOCK_GUARD_1(_n, _t, _lock)

#define	_scoped_guard(_n, _l, ...)					\
    for (DECLARE(_n, _scoped)(__VA_ARGS__);				\
	1 /*__guard_ptr(_n)(&_scoped) || !__is_cond_ptr(_n) */;		\
	({ goto _l; }))							\
		if (0) {						\
_l:									\
			break;						\
		} else

#define	scoped_guard(_n, ...)						\
    _scoped_guard(_n, ___label_ ## __COUNTER__, ##__VA_ARGS__)

#endif	/* _LINUXKPI_LINUX_CLEANUP_H */
