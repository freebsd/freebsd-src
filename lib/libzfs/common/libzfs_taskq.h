/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
 * Copyright (c) 2012, 2017 by Delphix. All rights reserved.
 * Copyright (c) 2012, Joyent, Inc. All rights reserved.
 */

#ifndef	_ZFS_TASKQ_H
#define	_ZFS_TASKQ_H

#include <stdint.h>
#include <umem.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct zfs_taskq zfs_taskq_t;
typedef uintptr_t zfs_taskqid_t;
typedef void (ztask_func_t)(void *);

#define	ZFS_TQENT_FLAG_PREALLOC		0x1	/* taskq_dispatch_ent used */

#define	ZFS_TASKQ_PREPOPULATE		0x0001

#define	ZFS_TQ_SLEEP	UMEM_NOFAIL	/* Can block for memory */
#define	ZFS_TQ_NOSLEEP	UMEM_DEFAULT	/* cannot block for memory; may fail */
#define	ZFS_TQ_FRONT	0x08		/* Queue in front */

extern zfs_taskq_t	*zfs_taskq_create(const char *, int, pri_t, int,
	int, uint_t);
extern void		zfs_taskq_destroy(zfs_taskq_t *);

extern zfs_taskqid_t	zfs_taskq_dispatch(zfs_taskq_t *, ztask_func_t,
	void *, uint_t);

extern void		zfs_taskq_wait(zfs_taskq_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _ZFS_TASKQ_H */
