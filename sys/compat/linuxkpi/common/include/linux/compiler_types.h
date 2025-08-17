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

#ifndef	_LINUXKPI_LINUX_COMPILER_TYPES_H_
#define	_LINUXKPI_LINUX_COMPILER_TYPES_H_

#include <sys/cdefs.h>

#include <compat/linuxkpi/common/include/linux/compiler_attributes.h>

#define	__kernel
#define	__user
#define	__iomem
#define	__percpu
#define	__rcu
#define	__chk_user_ptr(x)		((void)0)
#define	__chk_io_ptr(x)			((void)0)
#define	__acquires(x)
#define	__releases(x)
#define	__acquire(x)			do { } while (0)
#define	__release(x)			do { } while (0)
#define	__cond_lock(x,c)		(c)
#define	__force
#define	__nocast
#define	__safe
#define	__builtin_warning(x, y...)	(1)

#define	___PASTE(a,b) a##b
#define	__PASTE(a,b) ___PASTE(a,b)

#define	__diag_push()
#define	__diag_pop()
#define	__diag_ignore_all(...)

#define	__same_type(a, b)	__builtin_types_compatible_p(typeof(a), typeof(b))

#endif
