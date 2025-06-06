// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2007-2010 Lawrence Livermore National Security, LLC.
 *  Copyright (C) 2007 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Brian Behlendorf <behlendorf1@llnl.gov>.
 *  UCRL-CODE-235197
 *
 *  This file is part of the SPL, Solaris Porting Layer.
 *
 *  The SPL is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  The SPL is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the SPL.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SPL_TYPES_H
#define	_SPL_TYPES_H

#include <linux/types.h>

typedef enum {
	B_FALSE = 0,
	B_TRUE = 1
} boolean_t;

typedef unsigned char		uchar_t;
typedef unsigned short		ushort_t;
typedef unsigned int		uint_t;
typedef unsigned long		ulong_t;
typedef unsigned long long	u_longlong_t;
typedef long long		longlong_t;

#ifndef HAVE_KERNEL_INTPTR_T
typedef long			intptr_t;
#endif
typedef unsigned long long	rlim64_t;

typedef struct task_struct	kthread_t;
typedef struct task_struct	proc_t;

typedef int			id_t;
typedef short			pri_t;
typedef short			index_t;
typedef longlong_t		offset_t;
typedef u_longlong_t		u_offset_t;
typedef ulong_t			pgcnt_t;

typedef int			major_t;
typedef int			minor_t;

struct user_namespace;
#ifdef HAVE_IOPS_CREATE_IDMAP
#include <linux/refcount.h>
#ifdef HAVE_IDMAP_NO_USERNS
#include <linux/user_namespace.h>
struct mnt_idmap {
	struct uid_gid_map uid_map;
	struct uid_gid_map gid_map;
	refcount_t count;
};
typedef struct mnt_idmap	zidmap_t;
#define	idmap_owner(p)	(NULL)
#else
struct mnt_idmap {
	struct user_namespace *owner;
	refcount_t count;
};
typedef struct mnt_idmap	zidmap_t;
#define	idmap_owner(p)	(((struct mnt_idmap *)p)->owner)
#endif
#else
typedef struct user_namespace	zidmap_t;
#define	idmap_owner(p)	((struct user_namespace *)p)
#endif

extern zidmap_t *zfs_init_idmap;

#ifdef HAVE_1ARG_ASSIGN_STR
#define	__assign_str_impl(a, b)		__assign_str(a)
#else
#define	__assign_str_impl(a, b)		__assign_str(a, b)
#endif

#endif	/* _SPL_TYPES_H */
