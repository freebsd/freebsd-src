/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
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

#endif	/* _LINUXKPI_LINUX_CLEANUP_H */
