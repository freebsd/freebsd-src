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
 * Copyright 2010 Martin Matuska <mm@FreeBSD.org>. All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/dmu.h>
#include <sys/zio.h>
#include <sys/nvpair.h>
#include <sys/dsl_deleg.h>
#include <sys/zfs_ioctl.h>
#include "zfs_ioctl_compat.h"

/*
 * FreeBSD zfs_cmd compatibility with v15 and older binaries
 * appropriately remap/extend the zfs_cmd_t structure
 */
void
zfs_cmd_compat_get(zfs_cmd_t *zc, caddr_t addr, const int cflag)
{
	zfs_cmd_v15_t *zc_c;

	if (cflag == ZFS_CMD_COMPAT_V15) {
		zc_c = (void *)addr;

		/* zc */
		strlcpy(zc->zc_name,zc_c->zc_name,MAXPATHLEN);
		strlcpy(zc->zc_value,zc_c->zc_value,MAXPATHLEN);
		strlcpy(zc->zc_string,zc_c->zc_string,MAXPATHLEN);
		zc->zc_guid = zc_c->zc_guid;
		zc->zc_nvlist_conf = zc_c->zc_nvlist_conf;
		zc->zc_nvlist_conf_size = zc_c->zc_nvlist_conf_size;
		zc->zc_nvlist_src = zc_c->zc_nvlist_src;
		zc->zc_nvlist_src_size = zc_c->zc_nvlist_src_size;
		zc->zc_nvlist_dst = zc_c->zc_nvlist_dst;
		zc->zc_nvlist_dst_size = zc_c->zc_nvlist_dst_size;
		zc->zc_cookie = zc_c->zc_cookie;
		zc->zc_objset_type = zc_c->zc_objset_type;
		zc->zc_perm_action = zc_c->zc_perm_action;
		zc->zc_history = zc_c->zc_history;
		zc->zc_history_len = zc_c->zc_history_len;
		zc->zc_history_offset = zc_c->zc_history_offset;
		zc->zc_obj = zc_c->zc_obj;
		zc->zc_share = zc_c->zc_share;
		zc->zc_jailid = zc_c->zc_jailid;
		zc->zc_objset_stats = zc_c->zc_objset_stats;
		zc->zc_begin_record = zc_c->zc_begin_record;

		/* zc->zc_inject_record */
		zc->zc_inject_record.zi_objset =
		    zc_c->zc_inject_record.zi_objset;
		zc->zc_inject_record.zi_object =
		    zc_c->zc_inject_record.zi_object;
		zc->zc_inject_record.zi_start =
		    zc_c->zc_inject_record.zi_start;
		zc->zc_inject_record.zi_end =
		    zc_c->zc_inject_record.zi_end;
		zc->zc_inject_record.zi_guid =
		    zc_c->zc_inject_record.zi_guid;
		zc->zc_inject_record.zi_level =
		    zc_c->zc_inject_record.zi_level;
		zc->zc_inject_record.zi_error =
		    zc_c->zc_inject_record.zi_error;
		zc->zc_inject_record.zi_type =
		    zc_c->zc_inject_record.zi_type;
		zc->zc_inject_record.zi_freq =
		    zc_c->zc_inject_record.zi_freq;
		zc->zc_inject_record.zi_failfast =
		    zc_c->zc_inject_record.zi_failfast;
	}
}

void
zfs_cmd_compat_put(zfs_cmd_t *zc, caddr_t addr, const int cflag)
{
	zfs_cmd_v15_t *zc_c;

	switch (cflag) {
	case ZFS_CMD_COMPAT_V15:
		zc_c = (void *)addr;

		/* zc */
		strlcpy(zc_c->zc_name,zc->zc_name,MAXPATHLEN);
		strlcpy(zc_c->zc_value,zc->zc_value,MAXPATHLEN);
		strlcpy(zc_c->zc_string,zc->zc_string,MAXPATHLEN);
		zc_c->zc_guid = zc->zc_guid;
		zc_c->zc_nvlist_conf = zc->zc_nvlist_conf;
		zc_c->zc_nvlist_conf_size = zc->zc_nvlist_conf_size;
		zc_c->zc_nvlist_src = zc->zc_nvlist_src;
		zc_c->zc_nvlist_src_size = zc->zc_nvlist_src_size;
		zc_c->zc_nvlist_dst = zc->zc_nvlist_dst;
		zc_c->zc_nvlist_dst_size = zc->zc_nvlist_dst_size;
		zc_c->zc_cookie = zc->zc_cookie;
		zc_c->zc_objset_type = zc->zc_objset_type;
		zc_c->zc_perm_action = zc->zc_perm_action;
		zc_c->zc_history = zc->zc_history;
		zc_c->zc_history_len = zc->zc_history_len;
		zc_c->zc_history_offset = zc->zc_history_offset;
		zc_c->zc_obj = zc->zc_obj;
		zc_c->zc_share = zc->zc_share;
		zc_c->zc_jailid = zc->zc_jailid;
		zc_c->zc_objset_stats = zc->zc_objset_stats;
		zc_c->zc_begin_record = zc->zc_begin_record;

		/* zc_inject_record */
		zc_c->zc_inject_record.zi_objset =
		    zc->zc_inject_record.zi_objset;
		zc_c->zc_inject_record.zi_object =
		    zc->zc_inject_record.zi_object;
		zc_c->zc_inject_record.zi_start =
		    zc->zc_inject_record.zi_start;
		zc_c->zc_inject_record.zi_end =
		    zc->zc_inject_record.zi_end;
		zc_c->zc_inject_record.zi_guid =
		    zc->zc_inject_record.zi_guid;
		zc_c->zc_inject_record.zi_level =
		    zc->zc_inject_record.zi_level;
		zc_c->zc_inject_record.zi_error =
		    zc->zc_inject_record.zi_error;
		zc_c->zc_inject_record.zi_type =
		    zc->zc_inject_record.zi_type;
		zc_c->zc_inject_record.zi_freq =
		    zc->zc_inject_record.zi_freq;
		zc_c->zc_inject_record.zi_failfast =
		    zc->zc_inject_record.zi_failfast;

		break;
	}
}

static int
zfs_ioctl_compat_write_nvlist_dst(zfs_cmd_t *zc, nvlist_t *nvl, size_t nvsize)
{
	char *packed = (void *)(uintptr_t)zc->zc_nvlist_dst;
	int err;

	err = nvlist_pack(nvl, &packed, &nvsize,
	    NV_ENCODE_NATIVE, 0);
	if (err == 0)
		zc->zc_nvlist_dst_size = nvsize;

	return (err);
}

static void
zfs_ioctl_compat_fix_stats_nvlist(nvlist_t *nvl)
{
	nvlist_t **child;
	nvlist_t *nvroot = NULL;
	vdev_stat_t *vs;
	uint_t c, children, nelem;

	if (nvlist_lookup_nvlist_array(nvl, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++) {
			zfs_ioctl_compat_fix_stats_nvlist(child[c]);
		}
	}

	if (nvlist_lookup_nvlist(nvl, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0)
		zfs_ioctl_compat_fix_stats_nvlist(nvroot);
#ifdef _KERNEL
	if ((nvlist_lookup_uint64_array(nvl, ZPOOL_CONFIG_VDEV_STATS,
#else
	if ((nvlist_lookup_uint64_array(nvl, "stats",
#endif

	    (uint64_t **)&vs, &nelem) == 0)) {
		nvlist_add_uint64_array(nvl,
#ifdef _KERNEL
		    "stats",
#else
		    ZPOOL_CONFIG_VDEV_STATS,
#endif
		    (uint64_t *)vs, nelem);
#ifdef _KERNEL
		nvlist_remove(nvl, ZPOOL_CONFIG_VDEV_STATS,
#else
		nvlist_remove(nvl, "stats",
#endif
		    DATA_TYPE_UINT64_ARRAY);
	}
}

static void
zfs_ioctl_compat_fix_stats(zfs_cmd_t *zc, const int cflag)
{
	nvlist_t *nv, *nvp = NULL;
	nvpair_t *elem;
	size_t nvsize;
	char *packed;

	if (nvlist_unpack((void *)(uintptr_t)zc->zc_nvlist_dst,
	    zc->zc_nvlist_dst_size, &nv, 0) != 0)
		return;

	if (cflag == 5) { /* ZFS_IOC_POOL_STATS */
		elem = NULL;
		while ((elem = nvlist_next_nvpair(nv, elem)) != NULL) {
			if (nvpair_value_nvlist(elem, &nvp) == 0)
				zfs_ioctl_compat_fix_stats_nvlist(nvp);
		}
		elem = NULL;
	} else
		zfs_ioctl_compat_fix_stats_nvlist(nv);

	VERIFY(nvlist_size(nv, &nvsize, NV_ENCODE_NATIVE) == 0);
	zfs_ioctl_compat_write_nvlist_dst(zc, nv, nvsize);

	nvlist_free(nv);
}

static void
zfs_ioctl_compat_pool_get_props(zfs_cmd_t *zc)
{
	nvlist_t *nv, *nva = NULL;
	size_t nvsize;

	if (nvlist_unpack((void *)(uintptr_t)zc->zc_nvlist_dst,
	    zc->zc_nvlist_dst_size, &nv, 0) != 0)
		return;

#ifdef _KERNEL
	if (nvlist_lookup_nvlist(nv, "allocated", &nva) == 0) {
		nvlist_add_nvlist(nv, "used", nva);
		nvlist_remove(nv, "allocated", DATA_TYPE_NVLIST);
	}

	if (nvlist_lookup_nvlist(nv, "free", &nva) == 0) {
		nvlist_add_nvlist(nv, "available", nva);
		nvlist_remove(nv, "free", DATA_TYPE_NVLIST);
	}
#else
	if (nvlist_lookup_nvlist(nv, "used", &nva) == 0) {
		nvlist_add_nvlist(nv, "allocated", nva);
		nvlist_remove(nv, "used", DATA_TYPE_NVLIST);
	}

	if (nvlist_lookup_nvlist(nv, "available", &nva) == 0) {
		nvlist_add_nvlist(nv, "free", nva);
		nvlist_remove(nv, "available", DATA_TYPE_NVLIST);
	}
#endif

	VERIFY(nvlist_size(nv, &nvsize, NV_ENCODE_NATIVE) == 0);
	zfs_ioctl_compat_write_nvlist_dst(zc, nv, nvsize);

	nvlist_free(nv);
}

#ifndef _KERNEL
int
zcmd_ioctl_compat(int fd, unsigned long cmd, zfs_cmd_t *zc, const int cflag)
{
	int nc, ret;
	void *zc_c;
	unsigned long ncmd;

	if (cflag == ZFS_CMD_COMPAT_NONE) {
		ret = ioctl(fd, cmd, zc);
		return (ret);
	}

	if (cflag == ZFS_CMD_COMPAT_V15) {
		nc = zfs_ioctl_v28_to_v15[ZFS_IOC(cmd)];
		zc_c = malloc(sizeof(zfs_cmd_v15_t));
		ncmd = _IOWR('Z', nc, struct zfs_cmd_v15);
	} else
		return (EINVAL);

	if (ZFS_IOC(ncmd) == ZFS_IOC_COMPAT_FAIL)
		return (ENOTSUP);

	zfs_cmd_compat_put(zc, (caddr_t)zc_c, cflag);
	ret = ioctl(fd, ncmd, zc_c);
	if (cflag == ZFS_CMD_COMPAT_V15 &&
	    nc == 2 /* ZFS_IOC_POOL_IMPORT */)
		ret = ioctl(fd, _IOWR('Z', 4 /* ZFS_IOC_POOL_CONFIGS */,
		    struct zfs_cmd_v15), zc_c);
	zfs_cmd_compat_get(zc, (caddr_t)zc_c, cflag);
	free(zc_c);

	switch (nc) {
	case 2:	/* ZFS_IOC_POOL_IMPORT */
	case 4: /* ZFS_IOC_POOL_CONFIGS */
	case 5: /* ZFS_IOC_POOL_STATS */
	case 6: /* ZFS_IOC_POOL_TRYIMPORT */
		zfs_ioctl_compat_fix_stats(zc, nc);
		break;
	case 41: /* ZFS_IOC_POOL_GET_PROPS (v15) */
		zfs_ioctl_compat_pool_get_props(zc);
		break;
	}

	return (ret);
}
#else /* _KERNEL */
void
zfs_ioctl_compat_pre(zfs_cmd_t *zc, int *vec, const int cflag)
{
	if (cflag == ZFS_CMD_COMPAT_V15)
		switch (*vec) {

		case 7: /* ZFS_IOC_POOL_SCRUB (v15) */
			zc->zc_cookie = POOL_SCAN_SCRUB;
			break;
		}
}

void
zfs_ioctl_compat_post(zfs_cmd_t *zc, int vec, const int cflag)
{
	if (cflag == ZFS_CMD_COMPAT_V15) {
		switch (vec) {
		case 4:	/* ZFS_IOC_POOL_CONFIGS */
		case 5:	/* ZFS_IOC_POOL_STATS */
		case 6:	/* ZFS_IOC_POOL_TRYIMPORT */
			zfs_ioctl_compat_fix_stats(zc, vec);
			break;
		case 41: /* ZFS_IOC_POOL_GET_PROPS (v15) */
			zfs_ioctl_compat_pool_get_props(zc);
			break;
		}
	}
}
#endif /* KERNEL */
