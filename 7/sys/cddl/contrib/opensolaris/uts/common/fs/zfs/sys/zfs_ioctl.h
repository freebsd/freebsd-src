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

#ifndef	_SYS_ZFS_IOCTL_H
#define	_SYS_ZFS_IOCTL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/cred.h>
#include <sys/dmu.h>
#include <sys/zio.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Property values for snapdir
 */
#define	ZFS_SNAPDIR_HIDDEN		0
#define	ZFS_SNAPDIR_VISIBLE		1

#define	DMU_BACKUP_VERSION (1ULL)
#define	DMU_BACKUP_MAGIC 0x2F5bacbacULL

/*
 * zfs ioctl command structure
 */
typedef struct dmu_replay_record {
	enum {
		DRR_BEGIN, DRR_OBJECT, DRR_FREEOBJECTS,
		DRR_WRITE, DRR_FREE, DRR_END,
	} drr_type;
	uint32_t drr_pad;
	union {
		struct drr_begin {
			uint64_t drr_magic;
			uint64_t drr_version;
			uint64_t drr_creation_time;
			dmu_objset_type_t drr_type;
			uint32_t drr_pad;
			uint64_t drr_toguid;
			uint64_t drr_fromguid;
			char drr_toname[MAXNAMELEN];
		} drr_begin;
		struct drr_end {
			zio_cksum_t drr_checksum;
		} drr_end;
		struct drr_object {
			uint64_t drr_object;
			dmu_object_type_t drr_type;
			dmu_object_type_t drr_bonustype;
			uint32_t drr_blksz;
			uint32_t drr_bonuslen;
			uint8_t drr_checksum;
			uint8_t drr_compress;
			uint8_t drr_pad[6];
			/* bonus content follows */
		} drr_object;
		struct drr_freeobjects {
			uint64_t drr_firstobj;
			uint64_t drr_numobjs;
		} drr_freeobjects;
		struct drr_write {
			uint64_t drr_object;
			dmu_object_type_t drr_type;
			uint32_t drr_pad;
			uint64_t drr_offset;
			uint64_t drr_length;
			/* content follows */
		} drr_write;
		struct drr_free {
			uint64_t drr_object;
			uint64_t drr_offset;
			uint64_t drr_length;
		} drr_free;
	} drr_u;
} dmu_replay_record_t;

typedef struct zinject_record {
	uint64_t	zi_objset;
	uint64_t	zi_object;
	uint64_t	zi_start;
	uint64_t	zi_end;
	uint64_t	zi_guid;
	uint32_t	zi_level;
	uint32_t	zi_error;
	uint64_t	zi_type;
	uint32_t	zi_freq;
} zinject_record_t;

#define	ZINJECT_NULL		0x1
#define	ZINJECT_FLUSH_ARC	0x2
#define	ZINJECT_UNLOAD_SPA	0x4

typedef struct zfs_cmd {
	char		zc_name[MAXPATHLEN];
	char		zc_value[MAXPATHLEN * 2];
	uint64_t	zc_guid;
	uint64_t	zc_nvlist_src;	/* really (char *) */
	uint64_t	zc_nvlist_src_size;
	uint64_t	zc_nvlist_dst;	/* really (char *) */
	uint64_t	zc_nvlist_dst_size;
	uint64_t	zc_cookie;
	uint64_t	zc_cred;
	uint64_t	zc_dev;
	uint64_t	zc_objset_type;
	uint64_t	zc_history;	/* really (char *) */
	uint64_t	zc_history_len;
	uint64_t	zc_history_offset;
	uint64_t	zc_obj;
	uint64_t	zc_jailid;
	dmu_objset_stats_t zc_objset_stats;
	struct drr_begin zc_begin_record;
	zinject_record_t zc_inject_record;
} zfs_cmd_t;

#ifdef _KERNEL
typedef struct zfs_create_data {
	cred_t		*zc_cred;
	dev_t		zc_dev;
	nvlist_t	*zc_props;
} zfs_create_data_t;
#endif

#define	ZVOL_MAX_MINOR	(1 << 16)
#define	ZFS_MIN_MINOR	(ZVOL_MAX_MINOR + 1)

#ifdef _KERNEL

extern int zfs_secpolicy_write(const char *dataset, cred_t *cr);
extern int zfs_busy(void);
extern int zfs_unmount_snap(char *, void *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ZFS_IOCTL_H */
