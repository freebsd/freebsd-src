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

#ifndef	_LIBFS_IMPL_H
#define	_LIBFS_IMPL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/dmu.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_acl.h>
#include <sys/nvpair.h>

#include <libuutil.h>
#include <libzfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct libzfs_handle {
	int libzfs_error;
	int libzfs_fd;
	FILE *libzfs_mnttab;
	FILE *libzfs_sharetab;
	uu_avl_pool_t *libzfs_ns_avlpool;
	uu_avl_t *libzfs_ns_avl;
	uint64_t libzfs_ns_gen;
	int libzfs_desc_active;
	char libzfs_action[1024];
	char libzfs_desc[1024];
	int libzfs_printerr;
};

struct zfs_handle {
	libzfs_handle_t *zfs_hdl;
	char zfs_name[ZFS_MAXNAMELEN];
	zfs_type_t zfs_type; /* type including snapshot */
	zfs_type_t zfs_head_type; /* type excluding snapshot */
	dmu_objset_stats_t zfs_dmustats;
	nvlist_t *zfs_props;
	nvlist_t *zfs_user_props;
	boolean_t zfs_mntcheck;
	char *zfs_mntopts;
	char zfs_root[MAXPATHLEN];
};

/*
 * This is different from checking zfs_type, because it will also catch
 * snapshots of volumes.
 */
#define	ZFS_IS_VOLUME(zhp) ((zhp)->zfs_head_type == ZFS_TYPE_VOLUME)

struct zpool_handle {
	libzfs_handle_t *zpool_hdl;
	char zpool_name[ZPOOL_MAXNAMELEN];
	int zpool_state;
	size_t zpool_config_size;
	nvlist_t *zpool_config;
	nvlist_t *zpool_old_config;
	nvlist_t *zpool_props;
};

int zfs_error(libzfs_handle_t *, int, const char *);
int zfs_error_fmt(libzfs_handle_t *, int, const char *, ...);
void zfs_error_aux(libzfs_handle_t *, const char *, ...);
void *zfs_alloc(libzfs_handle_t *, size_t);
void *zfs_realloc(libzfs_handle_t *, void *, size_t, size_t);
char *zfs_strdup(libzfs_handle_t *, const char *);
int no_memory(libzfs_handle_t *);

int zfs_standard_error(libzfs_handle_t *, int, const char *);
int zfs_standard_error_fmt(libzfs_handle_t *, int, const char *, ...);
int zpool_standard_error(libzfs_handle_t *, int, const char *);
int zpool_standard_error_fmt(libzfs_handle_t *, int, const char *, ...);

int get_dependents(libzfs_handle_t *, boolean_t, const char *, char ***,
    size_t *);

int zfs_expand_proplist_common(libzfs_handle_t *, zfs_proplist_t **,
    zfs_type_t);
int zfs_get_proplist_common(libzfs_handle_t *, char *, zfs_proplist_t **,
    zfs_type_t);
zfs_prop_t zfs_prop_iter_common(zfs_prop_f, void *, zfs_type_t, boolean_t);
zfs_prop_t zfs_name_to_prop_common(const char *, zfs_type_t);

nvlist_t *zfs_validate_properties(libzfs_handle_t *, zfs_type_t, char *,
	nvlist_t *, uint64_t, zfs_handle_t *zhp, const char *errbuf);

typedef struct prop_changelist prop_changelist_t;

int zcmd_alloc_dst_nvlist(libzfs_handle_t *, zfs_cmd_t *, size_t);
int zcmd_write_src_nvlist(libzfs_handle_t *, zfs_cmd_t *, nvlist_t *, size_t *);
int zcmd_expand_dst_nvlist(libzfs_handle_t *, zfs_cmd_t *);
int zcmd_read_dst_nvlist(libzfs_handle_t *, zfs_cmd_t *, nvlist_t **);
void zcmd_free_nvlists(zfs_cmd_t *);

int changelist_prefix(prop_changelist_t *);
int changelist_postfix(prop_changelist_t *);
void changelist_rename(prop_changelist_t *, const char *, const char *);
void changelist_remove(zfs_handle_t *, prop_changelist_t *);
void changelist_free(prop_changelist_t *);
prop_changelist_t *changelist_gather(zfs_handle_t *, zfs_prop_t, int);
int changelist_unshare(prop_changelist_t *);
int changelist_haszonedchild(prop_changelist_t *);

void remove_mountpoint(zfs_handle_t *);

zfs_handle_t *make_dataset_handle(libzfs_handle_t *, const char *);

int zpool_open_silent(libzfs_handle_t *, const char *, zpool_handle_t **);

int zvol_create_link(libzfs_handle_t *, const char *);
int zvol_remove_link(libzfs_handle_t *, const char *);
int zpool_iter_zvol(zpool_handle_t *, int (*)(const char *, void *), void *);

void namespace_clear(libzfs_handle_t *);

#ifdef	__FreeBSD__
/*
 * This is FreeBSD version of ioctl, because Solaris' ioctl() updates
 * zc_nvlist_dst_size even if an error is returned, on FreeBSD if an
 * error is returned zc_nvlist_dst_size won't be updated.
 */
static __inline int
zcmd_ioctl(int fd, unsigned long cmd, zfs_cmd_t *zc)
{
	size_t oldsize;
	int ret;

	oldsize = zc->zc_nvlist_dst_size;
	ret = ioctl(fd, cmd, zc);
	if (ret == 0 && oldsize < zc->zc_nvlist_dst_size) {
		ret = -1;
		errno = ENOMEM;
	}

	return (ret);
}
#define	ioctl(fd, cmd, zc)	zcmd_ioctl((fd), (cmd), (zc))
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBFS_IMPL_H */
