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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/zfs_context.h>
#include <sys/vfs.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_acl.h>

void
zfs_ace_byteswap(ace_t *ace, int ace_cnt)
{
	int i;

	for (i = 0; i != ace_cnt; i++, ace++) {
		ace->a_who = BSWAP_32(ace->a_who);
		ace->a_access_mask = BSWAP_32(ace->a_access_mask);
		ace->a_flags = BSWAP_16(ace->a_flags);
		ace->a_type = BSWAP_16(ace->a_type);
	}
}

/* ARGSUSED */
void
zfs_acl_byteswap(void *buf, size_t size)
{
	int cnt;

	/*
	 * Arggh, since we don't know how many ACEs are in
	 * the array, we have to swap the entire block
	 */

	cnt = size / sizeof (ace_t);

	zfs_ace_byteswap((ace_t *)buf, cnt);
}

void
zfs_znode_byteswap(void *buf, size_t size)
{
	znode_phys_t *zp = buf;

	ASSERT(size >= sizeof (znode_phys_t));

	zp->zp_crtime[0] = BSWAP_64(zp->zp_crtime[0]);
	zp->zp_crtime[1] = BSWAP_64(zp->zp_crtime[1]);
	zp->zp_atime[0] = BSWAP_64(zp->zp_atime[0]);
	zp->zp_atime[1] = BSWAP_64(zp->zp_atime[1]);
	zp->zp_mtime[0] = BSWAP_64(zp->zp_mtime[0]);
	zp->zp_mtime[1] = BSWAP_64(zp->zp_mtime[1]);
	zp->zp_ctime[0] = BSWAP_64(zp->zp_ctime[0]);
	zp->zp_ctime[1] = BSWAP_64(zp->zp_ctime[1]);
	zp->zp_gen = BSWAP_64(zp->zp_gen);
	zp->zp_mode = BSWAP_64(zp->zp_mode);
	zp->zp_size = BSWAP_64(zp->zp_size);
	zp->zp_parent = BSWAP_64(zp->zp_parent);
	zp->zp_links = BSWAP_64(zp->zp_links);
	zp->zp_xattr = BSWAP_64(zp->zp_xattr);
	zp->zp_rdev = BSWAP_64(zp->zp_rdev);
	zp->zp_flags = BSWAP_64(zp->zp_flags);
	zp->zp_uid = BSWAP_64(zp->zp_uid);
	zp->zp_gid = BSWAP_64(zp->zp_gid);
	zp->zp_pad[0] = BSWAP_64(zp->zp_pad[0]);
	zp->zp_pad[1] = BSWAP_64(zp->zp_pad[1]);
	zp->zp_pad[2] = BSWAP_64(zp->zp_pad[2]);
	zp->zp_pad[3] = BSWAP_64(zp->zp_pad[3]);

	zp->zp_acl.z_acl_extern_obj = BSWAP_64(zp->zp_acl.z_acl_extern_obj);
	zp->zp_acl.z_acl_count = BSWAP_32(zp->zp_acl.z_acl_count);
	zp->zp_acl.z_acl_version = BSWAP_16(zp->zp_acl.z_acl_version);
	zp->zp_acl.z_acl_pad = BSWAP_16(zp->zp_acl.z_acl_pad);
	zfs_ace_byteswap(&zp->zp_acl.z_ace_data[0], ACE_SLOT_CNT);
}
