/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_ZFS_CONTEXT_H
#define	_SYS_ZFS_CONTEXT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/note.h>
#include <sys/kernel.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/bitmap.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/taskq.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/random.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/list.h>
#include <sys/uio.h>
#include <sys/dirent.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/string.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/sdt.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>
#include <sys/priv.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/stack.h>
#include <sys/lockf.h>
#include <sys/policy.h>
#include <sys/zone.h>
#include <sys/eventhandler.h>
#include <sys/misc.h>
#include <sys/zfs_debug.h>

#include <machine/stdarg.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
/* There is clash. vm_map.h defines the two below and vdev_cache.c use them. */
#ifdef min_offset
#undef min_offset
#endif
#ifdef max_offset
#undef max_offset
#endif
#include <vm/vm_extern.h>
#include <vm/vnode_pager.h>

#define	CPU_SEQID	(curcpu)

#ifdef	__cplusplus
}
#endif

extern int zfs_debug_level;
extern struct mtx zfs_debug_mtx;
#define	ZFS_LOG(lvl, ...)	do {					\
	if (((lvl) & 0xff) <= zfs_debug_level) {			\
		mtx_lock(&zfs_debug_mtx);				\
		printf("%s:%u[%d]: ", __func__, __LINE__, (lvl));	\
		printf(__VA_ARGS__);					\
		printf("\n");						\
		if ((lvl) & 0x100)					\
			kdb_backtrace();				\
		mtx_unlock(&zfs_debug_mtx);				\
	}								\
} while (0)

#endif	/* _SYS_ZFS_CONTEXT_H */
