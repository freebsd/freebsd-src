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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/vdev_disk.h>
#include <sys/vdev_impl.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>
#include <sys/sunldi.h>

/*
 * Virtual device vector for disks.
 */

extern ldi_ident_t zfs_li;

typedef struct vdev_disk_buf {
	buf_t	vdb_buf;
	zio_t	*vdb_io;
} vdev_disk_buf_t;

static int
vdev_disk_open(vdev_t *vd, uint64_t *psize, uint64_t *ashift)
{
	vdev_disk_t *dvd;
	struct dk_minfo dkm;
	int error;

	/*
	 * We must have a pathname, and it must be absolute.
	 */
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/') {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}

	dvd = vd->vdev_tsd = kmem_zalloc(sizeof (vdev_disk_t), KM_SLEEP);

	/*
	 * When opening a disk device, we want to preserve the user's original
	 * intent.  We always want to open the device by the path the user gave
	 * us, even if it is one of multiple paths to the save device.  But we
	 * also want to be able to survive disks being removed/recabled.
	 * Therefore the sequence of opening devices is:
	 *
	 * 1. Try opening the device by path.  For legacy pools without the
	 *    'whole_disk' property, attempt to fix the path by appending 's0'.
	 *
	 * 2. If the devid of the device matches the stored value, return
	 *    success.
	 *
	 * 3. Otherwise, the device may have moved.  Try opening the device
	 *    by the devid instead.
	 *
	 */
	if (vd->vdev_devid != NULL) {
		if (ddi_devid_str_decode(vd->vdev_devid, &dvd->vd_devid,
		    &dvd->vd_minor) != 0) {
			vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
			return (EINVAL);
		}
	}

	error = EINVAL;		/* presume failure */

	if (vd->vdev_path != NULL) {
		ddi_devid_t devid;

		if (vd->vdev_wholedisk == -1ULL) {
			size_t len = strlen(vd->vdev_path) + 3;
			char *buf = kmem_alloc(len, KM_SLEEP);
			ldi_handle_t lh;

			(void) snprintf(buf, len, "%ss0", vd->vdev_path);

			if (ldi_open_by_name(buf, spa_mode, kcred,
			    &lh, zfs_li) == 0) {
				spa_strfree(vd->vdev_path);
				vd->vdev_path = buf;
				vd->vdev_wholedisk = 1ULL;
				(void) ldi_close(lh, spa_mode, kcred);
			} else {
				kmem_free(buf, len);
			}
		}

		error = ldi_open_by_name(vd->vdev_path, spa_mode, kcred,
		    &dvd->vd_lh, zfs_li);

		/*
		 * Compare the devid to the stored value.
		 */
		if (error == 0 && vd->vdev_devid != NULL &&
		    ldi_get_devid(dvd->vd_lh, &devid) == 0) {
			if (ddi_devid_compare(devid, dvd->vd_devid) != 0) {
				error = EINVAL;
				(void) ldi_close(dvd->vd_lh, spa_mode, kcred);
				dvd->vd_lh = NULL;
			}
			ddi_devid_free(devid);
		}

		/*
		 * If we succeeded in opening the device, but 'vdev_wholedisk'
		 * is not yet set, then this must be a slice.
		 */
		if (error == 0 && vd->vdev_wholedisk == -1ULL)
			vd->vdev_wholedisk = 0;
	}

	/*
	 * If we were unable to open by path, or the devid check fails, open by
	 * devid instead.
	 */
	if (error != 0 && vd->vdev_devid != NULL)
		error = ldi_open_by_devid(dvd->vd_devid, dvd->vd_minor,
		    spa_mode, kcred, &dvd->vd_lh, zfs_li);

	if (error) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (error);
	}

	/*
	 * Determine the actual size of the device.
	 */
	if (ldi_get_size(dvd->vd_lh, psize) != 0) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (EINVAL);
	}

	/*
	 * If we own the whole disk, try to enable disk write caching.
	 * We ignore errors because it's OK if we can't do it.
	 */
	if (vd->vdev_wholedisk == 1) {
		int wce = 1;
		(void) ldi_ioctl(dvd->vd_lh, DKIOCSETWCE, (intptr_t)&wce,
		    FKIOCTL, kcred, NULL);
	}

	/*
	 * Determine the device's minimum transfer size.
	 * If the ioctl isn't supported, assume DEV_BSIZE.
	 */
	if (ldi_ioctl(dvd->vd_lh, DKIOCGMEDIAINFO, (intptr_t)&dkm,
	    FKIOCTL, kcred, NULL) != 0)
		dkm.dki_lbsize = DEV_BSIZE;

	*ashift = highbit(MAX(dkm.dki_lbsize, SPA_MINBLOCKSIZE)) - 1;

	/*
	 * Clear the nowritecache bit, so that on a vdev_reopen() we will
	 * try again.
	 */
	vd->vdev_nowritecache = B_FALSE;

	return (0);
}

static void
vdev_disk_close(vdev_t *vd)
{
	vdev_disk_t *dvd = vd->vdev_tsd;

	if (dvd == NULL)
		return;

	dprintf("removing disk %s, devid %s\n",
	    vd->vdev_path ? vd->vdev_path : "<none>",
	    vd->vdev_devid ? vd->vdev_devid : "<none>");

	if (dvd->vd_minor != NULL)
		ddi_devid_str_free(dvd->vd_minor);

	if (dvd->vd_devid != NULL)
		ddi_devid_free(dvd->vd_devid);

	if (dvd->vd_lh != NULL)
		(void) ldi_close(dvd->vd_lh, spa_mode, kcred);

	kmem_free(dvd, sizeof (vdev_disk_t));
	vd->vdev_tsd = NULL;
}

static void
vdev_disk_io_intr(buf_t *bp)
{
	vdev_disk_buf_t *vdb = (vdev_disk_buf_t *)bp;
	zio_t *zio = vdb->vdb_io;

	if ((zio->io_error = geterror(bp)) == 0 && bp->b_resid != 0)
		zio->io_error = EIO;

	kmem_free(vdb, sizeof (vdev_disk_buf_t));

	zio_next_stage_async(zio);
}

static void
vdev_disk_ioctl_done(void *zio_arg, int error)
{
	zio_t *zio = zio_arg;

	zio->io_error = error;

	zio_next_stage_async(zio);
}

static void
vdev_disk_io_start(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	vdev_disk_t *dvd = vd->vdev_tsd;
	vdev_disk_buf_t *vdb;
	buf_t *bp;
	int flags, error;

	if (zio->io_type == ZIO_TYPE_IOCTL) {
		zio_vdev_io_bypass(zio);

		/* XXPOLICY */
		if (vdev_is_dead(vd)) {
			zio->io_error = ENXIO;
			zio_next_stage_async(zio);
			return;
		}

		switch (zio->io_cmd) {

		case DKIOCFLUSHWRITECACHE:

			if (zfs_nocacheflush)
				break;

			if (vd->vdev_nowritecache) {
				zio->io_error = ENOTSUP;
				break;
			}

			zio->io_dk_callback.dkc_callback = vdev_disk_ioctl_done;
			zio->io_dk_callback.dkc_cookie = zio;

			error = ldi_ioctl(dvd->vd_lh, zio->io_cmd,
			    (uintptr_t)&zio->io_dk_callback,
			    FKIOCTL, kcred, NULL);

			if (error == 0) {
				/*
				 * The ioctl will be done asychronously,
				 * and will call vdev_disk_ioctl_done()
				 * upon completion.
				 */
				return;
			} else if (error == ENOTSUP) {
				/*
				 * If we get ENOTSUP, we know that no future
				 * attempts will ever succeed.  In this case we
				 * set a persistent bit so that we don't bother
				 * with the ioctl in the future.
				 */
				vd->vdev_nowritecache = B_TRUE;
			}
			zio->io_error = error;

			break;

		default:
			zio->io_error = ENOTSUP;
		}

		zio_next_stage_async(zio);
		return;
	}

	if (zio->io_type == ZIO_TYPE_READ && vdev_cache_read(zio) == 0)
		return;

	if ((zio = vdev_queue_io(zio)) == NULL)
		return;

	flags = (zio->io_type == ZIO_TYPE_READ ? B_READ : B_WRITE);
	flags |= B_BUSY | B_NOCACHE;
	if (zio->io_flags & ZIO_FLAG_FAILFAST)
		flags |= B_FAILFAST;

	vdb = kmem_alloc(sizeof (vdev_disk_buf_t), KM_SLEEP);

	vdb->vdb_io = zio;
	bp = &vdb->vdb_buf;

	bioinit(bp);
	bp->b_flags = flags;
	bp->b_bcount = zio->io_size;
	bp->b_un.b_addr = zio->io_data;
	bp->b_lblkno = lbtodb(zio->io_offset);
	bp->b_bufsize = zio->io_size;
	bp->b_iodone = (int (*)())vdev_disk_io_intr;

	/* XXPOLICY */
	error = vdev_is_dead(vd) ? ENXIO : vdev_error_inject(vd, zio);
	if (error) {
		zio->io_error = error;
		bioerror(bp, error);
		bp->b_resid = bp->b_bcount;
		bp->b_iodone(bp);
		return;
	}

	error = ldi_strategy(dvd->vd_lh, bp);
	/* ldi_strategy() will return non-zero only on programming errors */
	ASSERT(error == 0);
}

static void
vdev_disk_io_done(zio_t *zio)
{
	vdev_queue_io_done(zio);

	if (zio->io_type == ZIO_TYPE_WRITE)
		vdev_cache_write(zio);

	if (zio_injection_enabled && zio->io_error == 0)
		zio->io_error = zio_handle_device_injection(zio->io_vd, EIO);

	zio_next_stage(zio);
}

vdev_ops_t vdev_disk_ops = {
	vdev_disk_open,
	vdev_disk_close,
	vdev_default_asize,
	vdev_disk_io_start,
	vdev_disk_io_done,
	NULL,
	VDEV_TYPE_DISK,		/* name of this vdev type */
	B_TRUE			/* leaf vdev */
};
