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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_FS_ZFS_VFSOPS_H
#define	_SYS_FS_ZFS_VFSOPS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/list.h>
#include <sys/vfs.h>
#include <sys/zil.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct zfsvfs zfsvfs_t;

struct zfsvfs {
	vfs_t		*z_vfs;		/* generic fs struct */
	zfsvfs_t	*z_parent;	/* parent fs */
	objset_t	*z_os;		/* objset reference */
	uint64_t	z_root;		/* id of root znode */
	uint64_t	z_unlinkedobj;	/* id of unlinked zapobj */
	uint64_t	z_max_blksz;	/* maximum block size for files */
	uint64_t	z_assign;	/* TXG_NOWAIT or set by zil_replay() */
	zilog_t		*z_log;		/* intent log pointer */
	uint_t		z_acl_mode;	/* acl chmod/mode behavior */
	uint_t		z_acl_inherit;	/* acl inheritance behavior */
	boolean_t	z_atime;	/* enable atimes mount option */
	boolean_t	z_unmounted1;	/* unmounted phase 1 */
	boolean_t	z_unmounted2;	/* unmounted phase 2 */
	uint32_t	z_op_cnt;	/* vnode/vfs operations ref count */
	krwlock_t	z_um_lock;	/* rw lock for umount phase 2 */
	list_t		z_all_znodes;	/* all vnodes in the fs */
	kmutex_t	z_znodes_lock;	/* lock for z_all_znodes */
	vnode_t		*z_ctldir;	/* .zfs directory pointer */
	boolean_t	z_show_ctldir;	/* expose .zfs in the root dir */
	boolean_t	z_issnap;	/* true if this is a snapshot */
#define	ZFS_OBJ_MTX_SZ	64
	kmutex_t	z_hold_mtx[ZFS_OBJ_MTX_SZ];	/* znode hold locks */
};

/*
 * The total file ID size is limited to 12 bytes (including the length
 * field) in the NFSv2 protocol.  For historical reasons, this same limit
 * is currently being imposed by the Solaris NFSv3 implementation...
 * although the protocol actually permits a maximum of 64 bytes.  It will
 * not be possible to expand beyond 12 bytes without abandoning support
 * of NFSv2 and making some changes to the Solaris NFSv3 implementation.
 *
 * For the time being, we will partition up the available space as follows:
 *	2 bytes		fid length (required)
 *	6 bytes		object number (48 bits)
 *	4 bytes		generation number (32 bits)
 * We reserve only 48 bits for the object number, as this is the limit
 * currently defined and imposed by the DMU.
 */
typedef struct zfid_short {
	uint16_t	zf_len;
	uint8_t		zf_object[6];		/* obj[i] = obj >> (8 * i) */
	uint8_t		zf_gen[4];		/* gen[i] = gen >> (8 * i) */
} zfid_short_t;

typedef struct zfid_long {
	zfid_short_t	z_fid;
	uint8_t		zf_setid[6];		/* obj[i] = obj >> (8 * i) */
	uint8_t		zf_setgen[2];		/* gen[i] = gen >> (8 * i) */
} zfid_long_t;

#define	SHORT_FID_LEN	(sizeof (zfid_short_t) - sizeof (uint16_t))
#define	LONG_FID_LEN	(sizeof (zfid_long_t) - sizeof (uint16_t))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_ZFS_VFSOPS_H */
