/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
 * Copyright (c) 2015 François Tigeot
 * All rights reserved.
 */

#ifndef	_LINUXKPI_LINUX_COMPILER_ATTRIBUTES_H_
#define	_LINUXKPI_LINUX_COMPILER_ATTRIBUTES_H_

#include <sys/cdefs.h>

#define	__attribute_const__		__attribute__((__const__))

#ifndef	__deprecated
#define	__deprecated
#endif

#define	fallthrough			/* FALLTHROUGH */ do { } while(0)

#undef	__always_inline
#define	__always_inline			inline

#define	__printf(a,b)			__printflike(a,b)

#define	__malloc

#define	noinline			__noinline

#if __has_attribute(__nonstring__)
#define	__nonstring			__attribute__((__nonstring__))
#else
#define	__nonstring
#endif

#define	noinline_for_stack		__noinline

#define	__maybe_unused			__unused
#define	__always_unused			__unused

#define	__must_check			__result_use_check

#define	__weak __weak_symbol

#endif
