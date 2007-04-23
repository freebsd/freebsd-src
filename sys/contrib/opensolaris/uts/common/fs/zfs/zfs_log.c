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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_dir.h>
#include <sys/zil.h>
#include <sys/byteorder.h>
#include <sys/policy.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/dmu.h>
#include <sys/spa.h>

/*
 * All the functions in this file are used to construct the log entries
 * to record transactions. They allocate * a intent log transaction
 * structure (itx_t) and save within it all the information necessary to
 * possibly replay the transaction. The itx is then assigned a sequence
 * number and inserted in the in-memory list anchored in the zilog.
 */

/*
 * zfs_log_create() is used to handle TX_CREATE, TX_MKDIR and TX_MKXATTR
 * transactions.
 */
void
zfs_log_create(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *dzp, znode_t *zp, char *name)
{
	itx_t *itx;
	uint64_t seq;
	lr_create_t *lr;
	size_t namesize = strlen(name) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize);
	lr = (lr_create_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	lr->lr_foid = zp->z_id;
	lr->lr_mode = zp->z_phys->zp_mode;
	lr->lr_uid = zp->z_phys->zp_uid;
	lr->lr_gid = zp->z_phys->zp_gid;
	lr->lr_gen = zp->z_phys->zp_gen;
	lr->lr_crtime[0] = zp->z_phys->zp_crtime[0];
	lr->lr_crtime[1] = zp->z_phys->zp_crtime[1];
	lr->lr_rdev = zp->z_phys->zp_rdev;
	bcopy(name, (char *)(lr + 1), namesize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
	zp->z_last_itx = seq;
}

/*
 * zfs_log_remove() handles both TX_REMOVE and TX_RMDIR transactions.
 */
void
zfs_log_remove(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *dzp, char *name)
{
	itx_t *itx;
	uint64_t seq;
	lr_remove_t *lr;
	size_t namesize = strlen(name) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize);
	lr = (lr_remove_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	bcopy(name, (char *)(lr + 1), namesize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
}

/*
 * zfs_log_link() handles TX_LINK transactions.
 */
void
zfs_log_link(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *dzp, znode_t *zp, char *name)
{
	itx_t *itx;
	uint64_t seq;
	lr_link_t *lr;
	size_t namesize = strlen(name) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize);
	lr = (lr_link_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	lr->lr_link_obj = zp->z_id;
	bcopy(name, (char *)(lr + 1), namesize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
	zp->z_last_itx = seq;
}

/*
 * zfs_log_symlink() handles TX_SYMLINK transactions.
 */
void
zfs_log_symlink(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *dzp, znode_t *zp, char *name, char *link)
{
	itx_t *itx;
	uint64_t seq;
	lr_create_t *lr;
	size_t namesize = strlen(name) + 1;
	size_t linksize = strlen(link) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + namesize + linksize);
	lr = (lr_create_t *)&itx->itx_lr;
	lr->lr_doid = dzp->z_id;
	lr->lr_foid = zp->z_id;
	lr->lr_mode = zp->z_phys->zp_mode;
	lr->lr_uid = zp->z_phys->zp_uid;
	lr->lr_gid = zp->z_phys->zp_gid;
	lr->lr_gen = zp->z_phys->zp_gen;
	lr->lr_crtime[0] = zp->z_phys->zp_crtime[0];
	lr->lr_crtime[1] = zp->z_phys->zp_crtime[1];
	bcopy(name, (char *)(lr + 1), namesize);
	bcopy(link, (char *)(lr + 1) + namesize, linksize);

	seq = zil_itx_assign(zilog, itx, tx);
	dzp->z_last_itx = seq;
	zp->z_last_itx = seq;
}

/*
 * zfs_log_rename() handles TX_RENAME transactions.
 */
void
zfs_log_rename(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *sdzp, char *sname, znode_t *tdzp, char *dname, znode_t *szp)
{
	itx_t *itx;
	uint64_t seq;
	lr_rename_t *lr;
	size_t snamesize = strlen(sname) + 1;
	size_t dnamesize = strlen(dname) + 1;

	if (zilog == NULL)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + snamesize + dnamesize);
	lr = (lr_rename_t *)&itx->itx_lr;
	lr->lr_sdoid = sdzp->z_id;
	lr->lr_tdoid = tdzp->z_id;
	bcopy(sname, (char *)(lr + 1), snamesize);
	bcopy(dname, (char *)(lr + 1) + snamesize, dnamesize);

	seq = zil_itx_assign(zilog, itx, tx);
	sdzp->z_last_itx = seq;
	tdzp->z_last_itx = seq;
	szp->z_last_itx = seq;
}

/*
 * zfs_log_write() handles TX_WRITE transactions.
 */
ssize_t zfs_immediate_write_sz = 32768;

void
zfs_log_write(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, offset_t off, ssize_t len, int ioflag)
{
	itx_t *itx;
	uint64_t seq;
	lr_write_t *lr;
	itx_wr_state_t write_state;
	int err;

	if (zilog == NULL || zp->z_unlinked)
		return;

	/*
	 * Writes are handled in three different ways:
	 *
	 * WR_INDIRECT:
	 *    If the write is greater than zfs_immediate_write_sz then
	 *    later *if* we need to log the write then dmu_sync() is used
	 *    to immediately write the block and it's block pointer is put
	 *    in the log record.
	 * WR_COPIED:
	 *    If we know we'll immediately be committing the
	 *    transaction (FDSYNC (O_DSYNC)), the we allocate a larger
	 *    log record here for the data and copy the data in.
	 * WR_NEED_COPY:
	 *    Otherwise we don't allocate a buffer, and *if* we need to
	 *    flush the write later then a buffer is allocated and
	 *    we retrieve the data using the dmu.
	 */
	if (len > zfs_immediate_write_sz)
		write_state = WR_INDIRECT;
	else if (ioflag & FDSYNC)
		write_state = WR_COPIED;
	else
		write_state = WR_NEED_COPY;

	itx = zil_itx_create(txtype, sizeof (*lr) +
	    (write_state == WR_COPIED ? len : 0));
	lr = (lr_write_t *)&itx->itx_lr;
	if (write_state == WR_COPIED) {
		err = dmu_read(zp->z_zfsvfs->z_os, zp->z_id, off, len, lr + 1);
		if (err) {
			kmem_free(itx, offsetof(itx_t, itx_lr) +
			    itx->itx_lr.lrc_reclen);
			itx = zil_itx_create(txtype, sizeof (*lr));
			lr = (lr_write_t *)&itx->itx_lr;
			write_state = WR_NEED_COPY;
		}
	}

	itx->itx_wr_state = write_state;
	lr->lr_foid = zp->z_id;
	lr->lr_offset = off;
	lr->lr_length = len;
	lr->lr_blkoff = 0;
	BP_ZERO(&lr->lr_blkptr);

	itx->itx_private = zp->z_zfsvfs;

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}

/*
 * zfs_log_truncate() handles TX_TRUNCATE transactions.
 */
void
zfs_log_truncate(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, uint64_t off, uint64_t len)
{
	itx_t *itx;
	uint64_t seq;
	lr_truncate_t *lr;

	if (zilog == NULL || zp->z_unlinked)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr));
	lr = (lr_truncate_t *)&itx->itx_lr;
	lr->lr_foid = zp->z_id;
	lr->lr_offset = off;
	lr->lr_length = len;

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}

/*
 * zfs_log_setattr() handles TX_SETATTR transactions.
 */
void
zfs_log_setattr(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, vattr_t *vap, uint_t mask_applied)
{
	itx_t *itx;
	uint64_t seq;
	lr_setattr_t *lr;

	if (zilog == NULL || zp->z_unlinked)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr));
	lr = (lr_setattr_t *)&itx->itx_lr;
	lr->lr_foid = zp->z_id;
	lr->lr_mask = (uint64_t)mask_applied;
	lr->lr_mode = (uint64_t)vap->va_mode;
	lr->lr_uid = (uint64_t)vap->va_uid;
	lr->lr_gid = (uint64_t)vap->va_gid;
	lr->lr_size = (uint64_t)vap->va_size;
	ZFS_TIME_ENCODE(&vap->va_atime, lr->lr_atime);
	ZFS_TIME_ENCODE(&vap->va_mtime, lr->lr_mtime);

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}

/*
 * zfs_log_acl() handles TX_ACL transactions.
 */
void
zfs_log_acl(zilog_t *zilog, dmu_tx_t *tx, int txtype,
	znode_t *zp, int aclcnt, ace_t *z_ace)
{
	itx_t *itx;
	uint64_t seq;
	lr_acl_t *lr;

	if (zilog == NULL || zp->z_unlinked)
		return;

	itx = zil_itx_create(txtype, sizeof (*lr) + aclcnt * sizeof (ace_t));
	lr = (lr_acl_t *)&itx->itx_lr;
	lr->lr_foid = zp->z_id;
	lr->lr_aclcnt = (uint64_t)aclcnt;
	bcopy(z_ace, (ace_t *)(lr + 1), aclcnt * sizeof (ace_t));

	itx->itx_sync = (zp->z_sync_cnt != 0);
	seq = zil_itx_assign(zilog, itx, tx);
	zp->z_last_itx = seq;
}
