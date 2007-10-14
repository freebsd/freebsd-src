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
 * SPA-wide ENA.
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
 * will use the same ENA.  If a physical I/O is issued (by passing the
 * ZIO_FLAG_NOBOOKMARK flag), then this pointer is reset, guaranteeing that a
 * unique ENA will be generated.  For an aggregate I/O, this pointer is set to
 * NULL, and no ereport will be generated (since it doesn't actually correspond
 * to any particular device or piece of data).
 */
void
zfs_ereport_post(const char *subclass, spa_t *spa, vdev_t *vd, zio_t *zio,
    uint64_t stateoroffset, uint64_t size)
{
#ifdef _KERNEL
	char buf[1024];
	struct sbuf sb;
	struct timespec ts;

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

	/*
	 * Ignore any errors from I/Os that we are going to retry anyway - we
	 * only generate errors from the final failure.
	 */
	if (zio && zio_should_retry(zio))
		return;

	/*
	 * If this is not a read or write zio, ignore the error.  This can occur
	 * if the DKIOCFLUSHWRITECACHE ioctl fails.
	 */
	if (zio && zio->io_type != ZIO_TYPE_READ &&
	    zio->io_type != ZIO_TYPE_WRITE)
		return;

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
	 *
	 * The direct reference to spa_name is used rather than spa_name()
	 * because of the asynchronous nature of the zio pipeline.  spa_name()
	 * asserts that the config lock is held in some form.  This is always
	 * the case in I/O context, but because the check for RW_WRITER compares
	 * against 'curthread', we may be in an asynchronous context and blow
	 * this assert.  Rather than loosen this assert, we acknowledge that all
	 * contexts in which this function is called (pool open, I/O) are safe,
	 * and dereference the name directly.
	 */
	sbuf_printf(&sb, " %s=%s", FM_EREPORT_PAYLOAD_ZFS_POOL, spa->spa_name);
	sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_POOL_GUID,
	    spa_guid(spa));
	sbuf_printf(&sb, " %s=%u", FM_EREPORT_PAYLOAD_ZFS_POOL_CONTEXT,
	    spa->spa_load_state);

	if (vd != NULL) {
		vdev_t *pvd = vd->vdev_parent;

		sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_VDEV_GUID,
		    vd->vdev_guid);
		sbuf_printf(&sb, " %s=%s", FM_EREPORT_PAYLOAD_ZFS_VDEV_TYPE,
		    vd->vdev_ops->vdev_op_type);
		if (vd->vdev_path)
			sbuf_printf(&sb, " %s=%s",
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_PATH, vd->vdev_path);
		if (vd->vdev_devid)
			sbuf_printf(&sb, " %s=%s",
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_DEVID, vd->vdev_devid);

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

	sbuf_finish(&sb);
	ZFS_LOG(1, "%s\n", sbuf_data(&sb));
	devctl_notify("ZFS", spa->spa_name, subclass, sbuf_data(&sb));
	if (sbuf_overflowed(&sb))
		printf("ZFS WARNING: sbuf overflowed\n");
	sbuf_delete(&sb);
#endif
}

/*
 * The 'resource.fs.zfs.ok' event is an internal signal that the associated
 * resource (pool or disk) has been identified by ZFS as healthy.  This will
 * then trigger the DE to close the associated case, if any.
 */
void
zfs_post_ok(spa_t *spa, vdev_t *vd)
{
#ifdef _KERNEL
	char buf[1024];
	char class[64];
	struct sbuf sb;
	struct timespec ts;

	nanotime(&ts);

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_printf(&sb, "time=%ju.%ld", (uintmax_t)ts.tv_sec, ts.tv_nsec);

	snprintf(class, sizeof(class), "%s.%s.%s", FM_RSRC_RESOURCE,
	    ZFS_ERROR_CLASS, FM_RESOURCE_OK);
	sbuf_printf(&sb, " %s=%hhu", FM_VERSION, FM_RSRC_VERSION);
	sbuf_printf(&sb, " %s=%s", FM_CLASS, class);
	sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_POOL_GUID,
	    spa_guid(spa));
	if (vd)
		sbuf_printf(&sb, " %s=%ju", FM_EREPORT_PAYLOAD_ZFS_VDEV_GUID,
		    vd->vdev_guid);
	sbuf_finish(&sb);
	devctl_notify("ZFS", spa->spa_name, class, sbuf_data(&sb));
	if (sbuf_overflowed(&sb))
		printf("ZFS WARNING: sbuf overflowed\n");
	sbuf_delete(&sb);
#endif
}
