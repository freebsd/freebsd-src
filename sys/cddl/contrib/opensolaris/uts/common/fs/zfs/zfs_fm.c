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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/zio.h>

#include <sys/fm/fs/zfs.h>
#include <sys/fm/protocol.h>
#include <sys/fm/util.h>

#ifdef _KERNEL
/* Including sys/bus.h is just too hard, so I declare what I need here. */
extern void devctl_notify(const char *__system, const char *__subsystem,
    const char *__type, const char *__data);
#endif

/*
 * This general routine is responsible for generating all the different ZFS
 * ereports.  The payload is dependent on the class, and which arguments are
 * supplied to the function:
 *
 * 	EREPORT			POOL	VDEV	IO
 * 	block			X	X	X
 * 	data			X		X
 * 	device			X	X
 * 	pool			X
 *
 * If we are in a loading state, all errors are chained together by the same
 * SPA-wide ENA (Error Numeric Association).
 *
 * For isolated I/O requests, we get the ENA from the zio_t. The propagation
 * gets very complicated due to RAID-Z, gang blocks, and vdev caching.  We want
 * to chain together all ereports associated with a logical piece of data.  For
 * read I/Os, there  are basically three 'types' of I/O, which form a roughly
 * layered diagram:
 *
 *      +---------------+
 * 	| Aggregate I/O |	No associated logical data or device
 * 	+---------------+
 *              |
 *              V
 * 	+---------------+	Reads associated with a piece of logical data.
 * 	|   Read I/O    |	This includes reads on behalf of RAID-Z,
 * 	+---------------+       mirrors, gang blocks, retries, etc.
 *              |
 *              V
 * 	+---------------+	Reads associated with a particular device, but
 * 	| Physical I/O  |	no logical data.  Issued as part of vdev caching
 * 	+---------------+	and I/O aggregation.
 *
 * Note that 'physical I/O' here is not the same terminology as used in the rest
 * of ZIO.  Typically, 'physical I/O' simply means that there is no attached
 * blockpointer.  But I/O with no associated block pointer can still be related
 * to a logical piece of data (i.e. RAID-Z requests).
 *
 * Purely physical I/O always have unique ENAs.  They are not related to a
 * particular piece of logical data, and therefore cannot be chained together.
 * We still generate an ereport, but the DE doesn't correlate it with any
 * logical piece of data.  When such an I/O fails, the delegated I/O requests
 * will issue a retry, which will trigger the 'real' ereport with the correct
 * ENA.
 *
 * We keep track of the ENA for a ZIO chain through the 'io_logical' member.
 * When a new logical I/O is issued, we set this to point to itself.  Child I/Os
 * then inherit this pointer, so that when it is first set subsequent failures
 * will use the same ENA.  For vdev cache fill and queue aggregation I/O,
 * this pointer is set to NULL, and no ereport will be generated (since it
 * doesn't actually correspond to any particular device or piece of data,
 * and the caller will always retry without caching or queueing anyway).
 */
void
zfs_ereport_post(const char *subclass, spa_t *spa, vdev_t *vd, zio_t *zio,
    uint64_t stateoroffset, uint64_t size)
{
#ifdef _KERNEL
	char buf[1024];
	struct sbuf sb;
	struct timespec ts;
	int error;

	/*
	 * If we are doing a spa_tryimport(), ignore errors.
	 */
	if (spa->spa_load_state == SPA_LOAD_TRYIMPORT)
		return;

	/*
	 * If we are in the middle of opening a pool, and the previous attempt
	 * failed, don't bother logging any new ereports - we're just going to
	 * get the same diagnosis anyway.
	 */
	if (spa->spa_load_state != SPA_LOAD_NONE &&
	    spa->spa_last_open_failed)
		return;

	if (zio != NULL) {
		/*
		 * If this is not a read or write zio, ignore the error.  This
		 * can occur if the DKIOCFLUSHWRITECACHE ioctl fails.
		 */
		if (zio->io_type != ZIO_TYPE_READ &&
		    zio->io_type != ZIO_TYPE_WRITE)
			return;

		/*
		 * Ignore any errors from speculative I/Os, as failure is an
		 * expected result.
		 */
		if (zio->io_flags & ZIO_FLAG_SPECULATIVE)
			return;

		/*
		 * If this I/O is not a retry I/O, don't post an ereport.
		 * Otherwise, we risk making bad diagnoses based on B_FAILFAST
		 * I/Os.
		 */
		if (zio->io_error == EIO &&
		    !(zio->io_flags & ZIO_FLAG_IO_RETRY))
			return;

		if (vd != NULL) {
			/*
			 * If the vdev has already been marked as failing due
			 * to a failed probe, then ignore any subsequent I/O
			 * errors, as the DE will automatically fault the vdev
			 * on the first such failure.  This also catches cases
			 * where vdev_remove_wanted is set and the device has
			 * not yet been asynchronously placed into the REMOVED
			 * state.
			 */
			if (zio->io_vd == vd &&
			    !vdev_accessible(vd, zio) &&
			    strcmp(subclass, FM_EREPORT_ZFS_PROBE_FAILURE) != 0)
				return;

			/*
			 * Ignore checksum errors for reads from DTL regions of
			 * leaf vdevs.
			 */
			if (zio->io_type == ZIO_TYPE_READ &&
			    zio->io_error == ECKSUM &&
			    vd->vdev_ops->vdev_op_leaf &&
			    vdev_dtl_contains(vd, DTL_MISSING, zio->io_txg, 1))
				return;
		}
	}
	nanotime(&ts);

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_printf(&sb, "time=%ju.%ld", (uintmax_t)ts.tv_sec, ts.tv_nsec);

	/*
	 * Serialize ereport generation
	 */
	mutex_enter(&spa->spa_errlist_lock);

#if 0
	/*
	 * Determine the ENA to use for this event.  If we are in a loading
	 * state, use a SPA-wide ENA.  Otherwise, if we are in an I/O state, use
	 * a root zio-wide ENA.  Otherwise, simply use a unique ENA.
	 */
	if (spa->spa_load_state != SPA_LOAD_NONE) {
#if 0
		if (spa->spa_ena == 0)
			spa->spa_ena = fm_ena_generate(0, FM_ENA_FMT1);
#endif
		ena = spa->spa_ena;
	} else if (zio != NULL && zio->io_logical != NULL) {
#if 0
		if (zio->io_logical->io_ena == 0)
			zio->io_logical->io_ena =
			    fm_ena_generate(0, FM_ENA_FMT1);
#endif
		ena = zio->io_logical->io_ena;
	} else {
#if 0
		ena = fm_ena_generate(0, FM_ENA_FMT1);
#else
		ena = 0;
#endif
	}
#endif

	/*
	 * Construct the full class, detector, and other standard FMA fields.
	 */
	sbuf_printf(&sb, " ereport_version=%u", FM_EREPORT_VERSION);
	sbuf_printf(&sb, " class=%s.%s", ZFS_ERROR_CLASS, subclass);

	sbuf_printf(&sb, " zfs_scheme_version=%u", FM_ZFS_SCHEME_VERSION);

	/*
	 * Construct the per-ereport payload, depending on which parameters are
	 * passed in.
	 */

	/*
	 * Generic payload members common to all ereports.
	 */
	sbuf_printf(&sb, " %s=%s", FM_EREPORT_PAYLOAD_ZFS_POOL, spa_name(spa));
	sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_POOL_GUID,
	    spa_guid(spa));
	sbuf_printf(&sb, " %s=%d", FM_EREPORT_PAYLOAD_ZFS_POOL_CONTEXT,
	    spa->spa_load_state);

	if (spa != NULL) {
		sbuf_printf(&sb, " %s=%s", FM_EREPORT_PAYLOAD_ZFS_POOL_FAILMODE,
		    spa_get_failmode(spa) == ZIO_FAILURE_MODE_WAIT ?
		    FM_EREPORT_FAILMODE_WAIT :
		    spa_get_failmode(spa) == ZIO_FAILURE_MODE_CONTINUE ?
		    FM_EREPORT_FAILMODE_CONTINUE : FM_EREPORT_FAILMODE_PANIC);
	}

	if (vd != NULL) {
		vdev_t *pvd = vd->vdev_parent;

		sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_VDEV_GUID,
		    vd->vdev_guid);
		sbuf_printf(&sb, " %s=%s", FM_EREPORT_PAYLOAD_ZFS_VDEV_TYPE,
		    vd->vdev_ops->vdev_op_type);
		if (vd->vdev_path != NULL)
			sbuf_printf(&sb, " %s=%s",
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_PATH, vd->vdev_path);
		if (vd->vdev_devid != NULL)
			sbuf_printf(&sb, " %s=%s",
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_DEVID, vd->vdev_devid);
		if (vd->vdev_fru != NULL)
			sbuf_printf(&sb, " %s=%s",
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_FRU, vd->vdev_fru);

		if (pvd != NULL) {
			sbuf_printf(&sb, " %s=%ju",
			    FM_EREPORT_PAYLOAD_ZFS_PARENT_GUID, pvd->vdev_guid);
			sbuf_printf(&sb, " %s=%s",
			    FM_EREPORT_PAYLOAD_ZFS_PARENT_TYPE,
			    pvd->vdev_ops->vdev_op_type);
			if (pvd->vdev_path)
				sbuf_printf(&sb, " %s=%s",
				    FM_EREPORT_PAYLOAD_ZFS_PARENT_PATH,
				    pvd->vdev_path);
			if (pvd->vdev_devid)
				sbuf_printf(&sb, " %s=%s",
				    FM_EREPORT_PAYLOAD_ZFS_PARENT_DEVID,
				    pvd->vdev_devid);
		}
	}

	if (zio != NULL) {
		/*
		 * Payload common to all I/Os.
		 */
		sbuf_printf(&sb, " %s=%u", FM_EREPORT_PAYLOAD_ZFS_ZIO_ERR,
		    zio->io_error);

		/*
		 * If the 'size' parameter is non-zero, it indicates this is a
		 * RAID-Z or other I/O where the physical offset and length are
		 * provided for us, instead of within the zio_t.
		 */
		if (vd != NULL) {
			if (size) {
				sbuf_printf(&sb, " %s=%ju",
				    FM_EREPORT_PAYLOAD_ZFS_ZIO_OFFSET,
				    stateoroffset);
				sbuf_printf(&sb, " %s=%ju",
				    FM_EREPORT_PAYLOAD_ZFS_ZIO_SIZE, size);
			} else {
				sbuf_printf(&sb, " %s=%ju",
				    FM_EREPORT_PAYLOAD_ZFS_ZIO_OFFSET,
				    zio->io_offset);
				sbuf_printf(&sb, " %s=%ju",
				    FM_EREPORT_PAYLOAD_ZFS_ZIO_SIZE,
				    zio->io_size);
			}
		}

		/*
		 * Payload for I/Os with corresponding logical information.
		 */
		if (zio->io_logical != NULL) {
			sbuf_printf(&sb, " %s=%ju",
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_OBJECT,
			    zio->io_logical->io_bookmark.zb_object);
			sbuf_printf(&sb, " %s=%ju",
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_LEVEL,
			    zio->io_logical->io_bookmark.zb_level);
			sbuf_printf(&sb, " %s=%ju",
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_BLKID,
			    zio->io_logical->io_bookmark.zb_blkid);
		}
	} else if (vd != NULL) {
		/*
		 * If we have a vdev but no zio, this is a device fault, and the
		 * 'stateoroffset' parameter indicates the previous state of the
		 * vdev.
		 */
		sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_PREV_STATE,
		    stateoroffset);
	}
	mutex_exit(&spa->spa_errlist_lock);

	error = sbuf_finish(&sb);
	devctl_notify("ZFS", spa->spa_name, subclass, sbuf_data(&sb));
	if (error != 0)
		printf("ZFS WARNING: sbuf overflowed\n");
	sbuf_delete(&sb);
#endif
}

static void
zfs_post_common(spa_t *spa, vdev_t *vd, const char *name)
{
#ifdef _KERNEL
	char buf[1024];
	char class[64];
	struct sbuf sb;
	struct timespec ts;
	int error;

	nanotime(&ts);

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_printf(&sb, "time=%ju.%ld", (uintmax_t)ts.tv_sec, ts.tv_nsec);

	snprintf(class, sizeof(class), "%s.%s.%s", FM_RSRC_RESOURCE,
	    ZFS_ERROR_CLASS, name);
	sbuf_printf(&sb, " %s=%d", FM_VERSION, FM_RSRC_VERSION);
	sbuf_printf(&sb, " %s=%s", FM_CLASS, class);
	sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_POOL_GUID,
	    spa_guid(spa));
	if (vd)
		sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_VDEV_GUID,
		    vd->vdev_guid);
	error = sbuf_finish(&sb);
	ZFS_LOG(1, "%s", sbuf_data(&sb));
	devctl_notify("ZFS", spa->spa_name, class, sbuf_data(&sb));
	if (error != 0)
		printf("ZFS WARNING: sbuf overflowed\n");
	sbuf_delete(&sb);
#endif
}

/*
 * The 'resource.fs.zfs.removed' event is an internal signal that the given vdev
 * has been removed from the system.  This will cause the DE to ignore any
 * recent I/O errors, inferring that they are due to the asynchronous device
 * removal.
 */
void
zfs_post_remove(spa_t *spa, vdev_t *vd)
{
	zfs_post_common(spa, vd, FM_RESOURCE_REMOVED);
}

/*
 * The 'resource.fs.zfs.autoreplace' event is an internal signal that the pool
 * has the 'autoreplace' property set, and therefore any broken vdevs will be
 * handled by higher level logic, and no vdev fault should be generated.
 */
void
zfs_post_autoreplace(spa_t *spa, vdev_t *vd)
{
	zfs_post_common(spa, vd, FM_RESOURCE_AUTOREPLACE);
}
