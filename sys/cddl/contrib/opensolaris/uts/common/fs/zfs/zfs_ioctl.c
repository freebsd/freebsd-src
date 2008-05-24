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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/zfs_ioctl.h>
#include <sys/zap.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/dmu.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_prop.h>
#include <sys/sunddi.h>
#include <sys/policy.h>
#include <sys/zone.h>
#include <sys/nvpair.h>
#include <sys/mount.h>
#include <sys/taskqueue.h>
#include <sys/sdt.h>
#include <sys/varargs.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_ctldir.h>
#include <sys/zvol.h>

#include "zfs_namecheck.h"
#include "zfs_prop.h"

CTASSERT(sizeof(zfs_cmd_t) <= PAGE_SIZE);

static struct cdev *zfsdev;

extern void zfs_init(void);
extern void zfs_fini(void);

typedef int zfs_ioc_func_t(zfs_cmd_t *);
typedef int zfs_secpolicy_func_t(const char *, cred_t *);

typedef struct zfs_ioc_vec {
	zfs_ioc_func_t		*zvec_func;
	zfs_secpolicy_func_t	*zvec_secpolicy;
	enum {
		no_name,
		pool_name,
		dataset_name
	}			zvec_namecheck;
} zfs_ioc_vec_t;

/* _NOTE(PRINTFLIKE(4)) - this is printf-like, but lint is too whiney */
void
__dprintf(const char *file, const char *func, int line, const char *fmt, ...)
{
	const char *newfile;
	char buf[256];
	va_list adx;

	/*
	 * Get rid of annoying "../common/" prefix to filename.
	 */
	newfile = strrchr(file, '/');
	if (newfile != NULL) {
		newfile = newfile + 1; /* Get rid of leading / */
	} else {
		newfile = file;
	}

	va_start(adx, fmt);
	(void) vsnprintf(buf, sizeof (buf), fmt, adx);
	va_end(adx);

	/*
	 * To get this data, use the zfs-dprintf probe as so:
	 * dtrace -q -n 'zfs-dprintf \
	 *	/stringof(arg0) == "dbuf.c"/ \
	 *	{printf("%s: %s", stringof(arg1), stringof(arg3))}'
	 * arg0 = file name
	 * arg1 = function name
	 * arg2 = line number
	 * arg3 = message
	 */
	DTRACE_PROBE4(zfs__dprintf,
	    char *, newfile, char *, func, int, line, char *, buf);
}

/*
 * Policy for top-level read operations (list pools).  Requires no privileges,
 * and can be used in the local zone, as there is no associated dataset.
 */
/* ARGSUSED */
static int
zfs_secpolicy_none(const char *unused1, cred_t *cr)
{
	return (0);
}

/*
 * Policy for dataset read operations (list children, get statistics).  Requires
 * no privileges, but must be visible in the local zone.
 */
/* ARGSUSED */
static int
zfs_secpolicy_read(const char *dataset, cred_t *cr)
{
	if (INGLOBALZONE(curproc) ||
	    zone_dataset_visible(dataset, NULL))
		return (0);

	return (ENOENT);
}

static int
zfs_dozonecheck(const char *dataset, cred_t *cr)
{
	uint64_t zoned;
	int writable = 1;

	/*
	 * The dataset must be visible by this zone -- check this first
	 * so they don't see EPERM on something they shouldn't know about.
	 */
	if (!INGLOBALZONE(curproc) &&
	    !zone_dataset_visible(dataset, &writable))
		return (ENOENT);

	if (dsl_prop_get_integer(dataset, "jailed", &zoned, NULL))
		return (ENOENT);

	if (INGLOBALZONE(curproc)) {
		/*
		 * If the fs is zoned, only root can access it from the
		 * global zone.
		 */
		if (secpolicy_zfs(cr) && zoned)
			return (EPERM);
	} else {
		/*
		 * If we are in a local zone, the 'zoned' property must be set.
		 */
		if (!zoned)
			return (EPERM);

		/* must be writable by this zone */
		if (!writable)
			return (EPERM);
	}
	return (0);
}

/*
 * Policy for dataset write operations (create children, set properties, etc).
 * Requires SYS_MOUNT privilege, and must be writable in the local zone.
 */
int
zfs_secpolicy_write(const char *dataset, cred_t *cr)
{
	int error;

	if (error = zfs_dozonecheck(dataset, cr))
		return (error);

	return (secpolicy_zfs(cr));
}

/*
 * Policy for operations that want to write a dataset's parent:
 * create, destroy, snapshot, clone, restore.
 */
static int
zfs_secpolicy_parent(const char *dataset, cred_t *cr)
{
	char parentname[MAXNAMELEN];
	char *cp;

	/*
	 * Remove the @bla or /bla from the end of the name to get the parent.
	 */
	(void) strncpy(parentname, dataset, sizeof (parentname));
	cp = strrchr(parentname, '@');
	if (cp != NULL) {
		cp[0] = '\0';
	} else {
		cp = strrchr(parentname, '/');
		if (cp == NULL)
			return (ENOENT);
		cp[0] = '\0';

	}

	return (zfs_secpolicy_write(parentname, cr));
}

/*
 * Policy for pool operations - create/destroy pools, add vdevs, etc.  Requires
 * SYS_CONFIG privilege, which is not available in a local zone.
 */
/* ARGSUSED */
static int
zfs_secpolicy_config(const char *unused, cred_t *cr)
{
	if (secpolicy_sys_config(cr, B_FALSE) != 0)
		return (EPERM);

	return (0);
}

/*
 * Policy for fault injection.  Requires all privileges.
 */
/* ARGSUSED */
static int
zfs_secpolicy_inject(const char *unused, cred_t *cr)
{
	return (secpolicy_zinject(cr));
}

/*
 * Policy for dataset backup operations (sendbackup).
 * Requires SYS_MOUNT privilege, and must be writable in the local zone.
 */
static int
zfs_secpolicy_operator(const char *dataset, cred_t *cr)
{
	int writable = 1;

	if (!INGLOBALZONE(curproc) && !zone_dataset_visible(dataset, &writable))
		return (ENOENT);
	if (secpolicy_zfs(cr) != 0 && !groupmember(GID_OPERATOR, cr))
		return (EPERM);
	return (0);
}

/*
 * Returns the nvlist as specified by the user in the zfs_cmd_t.
 */
static int
get_nvlist(zfs_cmd_t *zc, nvlist_t **nvp)
{
	char *packed;
	size_t size;
	int error;
	nvlist_t *config = NULL;

	/*
	 * Read in and unpack the user-supplied nvlist.
	 */
	if ((size = zc->zc_nvlist_src_size) == 0)
		return (EINVAL);

	packed = kmem_alloc(size, KM_SLEEP);

	if ((error = xcopyin((void *)(uintptr_t)zc->zc_nvlist_src, packed,
	    size)) != 0) {
		kmem_free(packed, size);
		return (error);
	}

	if ((error = nvlist_unpack(packed, size, &config, 0)) != 0) {
		kmem_free(packed, size);
		return (error);
	}

	kmem_free(packed, size);

	*nvp = config;
	return (0);
}

static int
put_nvlist(zfs_cmd_t *zc, nvlist_t *nvl)
{
	char *packed = NULL;
	size_t size;
	int error;

	VERIFY(nvlist_size(nvl, &size, NV_ENCODE_NATIVE) == 0);

	if (size > zc->zc_nvlist_dst_size) {
		/*
		 * Solaris returns ENOMEM here, because even if an error is
		 * returned from an ioctl(2), new zc_nvlist_dst_size will be
		 * passed to the userland. This is not the case for FreeBSD.
		 * We need to return 0, so the kernel will copy the
		 * zc_nvlist_dst_size back and the userland can discover that a
		 * bigger buffer is needed.
		 */
		error = 0;
	} else {
		VERIFY(nvlist_pack(nvl, &packed, &size, NV_ENCODE_NATIVE,
		    KM_SLEEP) == 0);
		error = xcopyout(packed, (void *)(uintptr_t)zc->zc_nvlist_dst,
		    size);
		kmem_free(packed, size);
	}

	zc->zc_nvlist_dst_size = size;
	return (error);
}

static int
zfs_ioc_pool_create(zfs_cmd_t *zc)
{
	int error;
	nvlist_t *config;

	if ((error = get_nvlist(zc, &config)) != 0)
		return (error);

	error = spa_create(zc->zc_name, config, zc->zc_value[0] == '\0' ?
	    NULL : zc->zc_value);

	nvlist_free(config);

	return (error);
}

static int
zfs_ioc_pool_destroy(zfs_cmd_t *zc)
{
	return (spa_destroy(zc->zc_name));
}

static int
zfs_ioc_pool_import(zfs_cmd_t *zc)
{
	int error;
	nvlist_t *config;
	uint64_t guid;

	if ((error = get_nvlist(zc, &config)) != 0)
		return (error);

	if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID, &guid) != 0 ||
	    guid != zc->zc_guid)
		error = EINVAL;
	else
		error = spa_import(zc->zc_name, config,
		    zc->zc_value[0] == '\0' ? NULL : zc->zc_value);

	nvlist_free(config);

	return (error);
}

static int
zfs_ioc_pool_export(zfs_cmd_t *zc)
{
	return (spa_export(zc->zc_name, NULL));
}

static int
zfs_ioc_pool_configs(zfs_cmd_t *zc)
{
	nvlist_t *configs;
	int error;

	if ((configs = spa_all_configs(&zc->zc_cookie)) == NULL)
		return (EEXIST);

	error = put_nvlist(zc, configs);

	nvlist_free(configs);

	return (error);
}

static int
zfs_ioc_pool_stats(zfs_cmd_t *zc)
{
	nvlist_t *config;
	int error;
	int ret = 0;

	error = spa_get_stats(zc->zc_name, &config, zc->zc_value,
	    sizeof (zc->zc_value));

	if (config != NULL) {
		ret = put_nvlist(zc, config);
		nvlist_free(config);

		/*
		 * The config may be present even if 'error' is non-zero.
		 * In this case we return success, and preserve the real errno
		 * in 'zc_cookie'.
		 */
		zc->zc_cookie = error;
	} else {
		ret = error;
	}

	return (ret);
}

/*
 * Try to import the given pool, returning pool stats as appropriate so that
 * user land knows which devices are available and overall pool health.
 */
static int
zfs_ioc_pool_tryimport(zfs_cmd_t *zc)
{
	nvlist_t *tryconfig, *config;
	int error;

	if ((error = get_nvlist(zc, &tryconfig)) != 0)
		return (error);

	config = spa_tryimport(tryconfig);

	nvlist_free(tryconfig);

	if (config == NULL)
		return (EINVAL);

	error = put_nvlist(zc, config);
	nvlist_free(config);

	return (error);
}

static int
zfs_ioc_pool_scrub(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	error = spa_scrub(spa, zc->zc_cookie, B_FALSE);

	spa_close(spa, FTAG);

	return (error);
}

static int
zfs_ioc_pool_freeze(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;

	error = spa_open(zc->zc_name, &spa, FTAG);
	if (error == 0) {
		spa_freeze(spa);
		spa_close(spa, FTAG);
	}
	return (error);
}

static int
zfs_ioc_pool_upgrade(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	spa_upgrade(spa);

	spa_close(spa, FTAG);

	return (error);
}

static int
zfs_ioc_pool_get_history(zfs_cmd_t *zc)
{
	spa_t *spa;
	char *hist_buf;
	uint64_t size;
	int error;

	if ((size = zc->zc_history_len) == 0)
		return (EINVAL);

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	if (spa_version(spa) < ZFS_VERSION_ZPOOL_HISTORY) {
		spa_close(spa, FTAG);
		return (ENOTSUP);
	}

	hist_buf = kmem_alloc(size, KM_SLEEP);
	if ((error = spa_history_get(spa, &zc->zc_history_offset,
	    &zc->zc_history_len, hist_buf)) == 0) {
		error = xcopyout(hist_buf, (char *)(uintptr_t)zc->zc_history,
		    zc->zc_history_len);
	}

	spa_close(spa, FTAG);
	kmem_free(hist_buf, size);
	return (error);
}

static int
zfs_ioc_pool_log_history(zfs_cmd_t *zc)
{
	spa_t *spa;
	char *history_str = NULL;
	size_t size;
	int error;

	size = zc->zc_history_len;
	if (size == 0 || size > HIS_MAX_RECORD_LEN)
		return (EINVAL);

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	if (spa_version(spa) < ZFS_VERSION_ZPOOL_HISTORY) {
		spa_close(spa, FTAG);
		return (ENOTSUP);
	}

	/* add one for the NULL delimiter */
	size++;
	history_str = kmem_alloc(size, KM_SLEEP);
	if ((error = xcopyin((void *)(uintptr_t)zc->zc_history, history_str,
	    size)) != 0) {
		spa_close(spa, FTAG);
		kmem_free(history_str, size);
		return (error);
	}
	history_str[size - 1] = '\0';

	error = spa_history_log(spa, history_str, zc->zc_history_offset);

	spa_close(spa, FTAG);
	kmem_free(history_str, size);

	return (error);
}

static int
zfs_ioc_dsobj_to_dsname(zfs_cmd_t *zc)
{
	int error;

	if (error = dsl_dsobj_to_dsname(zc->zc_name, zc->zc_obj, zc->zc_value))
		return (error);

	return (0);
}

static int
zfs_ioc_obj_to_path(zfs_cmd_t *zc)
{
	objset_t *osp;
	int error;

	if ((error = dmu_objset_open(zc->zc_name, DMU_OST_ZFS,
	    DS_MODE_NONE | DS_MODE_READONLY, &osp)) != 0)
		return (error);

	error = zfs_obj_to_path(osp, zc->zc_obj, zc->zc_value,
	    sizeof (zc->zc_value));
	dmu_objset_close(osp);

	return (error);
}

static int
zfs_ioc_vdev_add(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;
	nvlist_t *config;

	error = spa_open(zc->zc_name, &spa, FTAG);
	if (error != 0)
		return (error);

	/*
	 * A root pool with concatenated devices is not supported.
	 * Thus, can not add a device to a root pool with one device.
	 */
	if (spa->spa_root_vdev->vdev_children == 1 && spa->spa_bootfs != 0) {
		spa_close(spa, FTAG);
		return (EDOM);
	}

	if ((error = get_nvlist(zc, &config)) == 0) {
		error = spa_vdev_add(spa, config);
		nvlist_free(config);
	}

	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_vdev_remove(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;

	error = spa_open(zc->zc_name, &spa, FTAG);
	if (error != 0)
		return (error);
	error = spa_vdev_remove(spa, zc->zc_guid, B_FALSE);
	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_vdev_online(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);
	error = vdev_online(spa, zc->zc_guid);
	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_vdev_offline(zfs_cmd_t *zc)
{
	spa_t *spa;
	int istmp = zc->zc_cookie;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);
	error = vdev_offline(spa, zc->zc_guid, istmp);
	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_vdev_attach(zfs_cmd_t *zc)
{
	spa_t *spa;
	int replacing = zc->zc_cookie;
	nvlist_t *config;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	if ((error = get_nvlist(zc, &config)) == 0) {
		error = spa_vdev_attach(spa, zc->zc_guid, config, replacing);
		nvlist_free(config);
	}

	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_vdev_detach(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	error = spa_vdev_detach(spa, zc->zc_guid, B_FALSE);

	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_vdev_setpath(zfs_cmd_t *zc)
{
	spa_t *spa;
	char *path = zc->zc_value;
	uint64_t guid = zc->zc_guid;
	int error;

	error = spa_open(zc->zc_name, &spa, FTAG);
	if (error != 0)
		return (error);

	error = spa_vdev_setpath(spa, guid, path);
	spa_close(spa, FTAG);
	return (error);
}

static int
zfs_ioc_objset_stats(zfs_cmd_t *zc)
{
	objset_t *os = NULL;
	int error;
	nvlist_t *nv;

retry:
	error = dmu_objset_open(zc->zc_name, DMU_OST_ANY,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &os);
	if (error != 0) {
		/*
		 * This is ugly: dmu_objset_open() can return EBUSY if
		 * the objset is held exclusively. Fortunately this hold is
		 * only for a short while, so we retry here.
		 * This avoids user code having to handle EBUSY,
		 * for example for a "zfs list".
		 */
		if (error == EBUSY) {
			delay(1);
			goto retry;
		}
		return (error);
	}

	dmu_objset_fast_stat(os, &zc->zc_objset_stats);

	if (zc->zc_nvlist_dst != 0 &&
	    (error = dsl_prop_get_all(os, &nv)) == 0) {
		dmu_objset_stats(os, nv);
		/*
		 * NB: zvol_get_stats() will read the objset contents,
		 * which we aren't supposed to do with a
		 * DS_MODE_STANDARD open, because it could be
		 * inconsistent.  So this is a bit of a workaround...
		 */
		if (!zc->zc_objset_stats.dds_inconsistent &&
		    dmu_objset_type(os) == DMU_OST_ZVOL)
			VERIFY(zvol_get_stats(os, nv) == 0);
		error = put_nvlist(zc, nv);
		nvlist_free(nv);
	}

	spa_altroot(dmu_objset_spa(os), zc->zc_value, sizeof (zc->zc_value));

	dmu_objset_close(os);
	if (error == ENOMEM)
		error = 0;
	return (error);
}

static int
zfs_ioc_dataset_list_next(zfs_cmd_t *zc)
{
	objset_t *os;
	int error;
	char *p;

retry:
	error = dmu_objset_open(zc->zc_name, DMU_OST_ANY,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &os);
	if (error != 0) {
		/*
		 * This is ugly: dmu_objset_open() can return EBUSY if
		 * the objset is held exclusively. Fortunately this hold is
		 * only for a short while, so we retry here.
		 * This avoids user code having to handle EBUSY,
		 * for example for a "zfs list".
		 */
		if (error == EBUSY) {
			delay(1);
			goto retry;
		}
		if (error == ENOENT)
			error = ESRCH;
		return (error);
	}

	p = strrchr(zc->zc_name, '/');
	if (p == NULL || p[1] != '\0')
		(void) strlcat(zc->zc_name, "/", sizeof (zc->zc_name));
	p = zc->zc_name + strlen(zc->zc_name);

	do {
		error = dmu_dir_list_next(os,
		    sizeof (zc->zc_name) - (p - zc->zc_name), p,
		    NULL, &zc->zc_cookie);
		if (error == ENOENT)
			error = ESRCH;
	} while (error == 0 && !INGLOBALZONE(curproc) &&
	    !zone_dataset_visible(zc->zc_name, NULL));

	/*
	 * If it's a hidden dataset (ie. with a '$' in its name), don't
	 * try to get stats for it.  Userland will skip over it.
	 */
	if (error == 0 && strchr(zc->zc_name, '$') == NULL)
		error = zfs_ioc_objset_stats(zc); /* fill in the stats */

	dmu_objset_close(os);
	return (error);
}

static int
zfs_ioc_snapshot_list_next(zfs_cmd_t *zc)
{
	objset_t *os;
	int error;

retry:
	error = dmu_objset_open(zc->zc_name, DMU_OST_ANY,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &os);
	if (error != 0) {
		/*
		 * This is ugly: dmu_objset_open() can return EBUSY if
		 * the objset is held exclusively. Fortunately this hold is
		 * only for a short while, so we retry here.
		 * This avoids user code having to handle EBUSY,
		 * for example for a "zfs list".
		 */
		if (error == EBUSY) {
			delay(1);
			goto retry;
		}
		if (error == ENOENT)
			error = ESRCH;
		return (error);
	}

	/*
	 * A dataset name of maximum length cannot have any snapshots,
	 * so exit immediately.
	 */
	if (strlcat(zc->zc_name, "@", sizeof (zc->zc_name)) >= MAXNAMELEN) {
		dmu_objset_close(os);
		return (ESRCH);
	}

	error = dmu_snapshot_list_next(os,
	    sizeof (zc->zc_name) - strlen(zc->zc_name),
	    zc->zc_name + strlen(zc->zc_name), NULL, &zc->zc_cookie);
	if (error == ENOENT)
		error = ESRCH;

	if (error == 0)
		error = zfs_ioc_objset_stats(zc); /* fill in the stats */

	dmu_objset_close(os);
	return (error);
}

static int
zfs_set_prop_nvlist(const char *name, dev_t dev, cred_t *cr, nvlist_t *nvl)
{
	nvpair_t *elem;
	int error;
	const char *propname;
	zfs_prop_t prop;
	uint64_t intval;
	char *strval;
	char buf[MAXNAMELEN];
	const char *p;
	spa_t *spa;

	elem = NULL;
	while ((elem = nvlist_next_nvpair(nvl, elem)) != NULL) {
		propname = nvpair_name(elem);

		if ((prop = zfs_name_to_prop(propname)) ==
		    ZFS_PROP_INVAL) {
			/*
			 * If this is a user-defined property, it must be a
			 * string, and there is no further validation to do.
			 */
			if (!zfs_prop_user(propname) ||
			    nvpair_type(elem) != DATA_TYPE_STRING)
				return (EINVAL);

			VERIFY(nvpair_value_string(elem, &strval) == 0);
			error = dsl_prop_set(name, propname, 1,
			    strlen(strval) + 1, strval);
			if (error == 0)
				continue;
			else
				return (error);
		}

		/*
		 * Check permissions for special properties.
		 */
		switch (prop) {
		case ZFS_PROP_ZONED:
			/*
			 * Disallow setting of 'zoned' from within a local zone.
			 */
			if (!INGLOBALZONE(curproc))
				return (EPERM);
			break;

		case ZFS_PROP_QUOTA:
			if (error = zfs_dozonecheck(name, cr))
				return (error);

			if (!INGLOBALZONE(curproc)) {
				uint64_t zoned;
				char setpoint[MAXNAMELEN];
				int dslen;
				/*
				 * Unprivileged users are allowed to modify the
				 * quota on things *under* (ie. contained by)
				 * the thing they own.
				 */
				if (dsl_prop_get_integer(name, "jailed", &zoned,
				    setpoint))
					return (EPERM);
				if (!zoned) /* this shouldn't happen */
					return (EPERM);
				dslen = strlen(name);
				if (dslen <= strlen(setpoint))
					return (EPERM);
			}
			break;

		case ZFS_PROP_COMPRESSION:
			/*
			 * If the user specified gzip compression, make sure
			 * the SPA supports it. We ignore any errors here since
			 * we'll catch them later.
			 */
			if (nvpair_type(elem) == DATA_TYPE_UINT64 &&
			    nvpair_value_uint64(elem, &intval) == 0 &&
			    intval >= ZIO_COMPRESS_GZIP_1 &&
			    intval <= ZIO_COMPRESS_GZIP_9) {
				if ((p = strchr(name, '/')) == NULL) {
					p = name;
				} else {
					bcopy(name, buf, p - name);
					buf[p - name] = '\0';
					p = buf;
				}

				if (spa_open(p, &spa, FTAG) == 0) {
					if (spa_version(spa) <
					    ZFS_VERSION_GZIP_COMPRESSION) {
						spa_close(spa, FTAG);
						return (ENOTSUP);
					}

					spa_close(spa, FTAG);
				}
			}
			break;
		}

		switch (prop) {
		case ZFS_PROP_QUOTA:
			if ((error = nvpair_value_uint64(elem, &intval)) != 0 ||
			    (error = dsl_dir_set_quota(name,
			    intval)) != 0)
				return (error);
			break;

		case ZFS_PROP_RESERVATION:
			if ((error = nvpair_value_uint64(elem, &intval)) != 0 ||
			    (error = dsl_dir_set_reservation(name,
			    intval)) != 0)
				return (error);
			break;

		case ZFS_PROP_VOLSIZE:
			if ((error = nvpair_value_uint64(elem, &intval)) != 0 ||
			    (error = zvol_set_volsize(name, dev,
			    intval)) != 0)
				return (error);
			break;

		case ZFS_PROP_VOLBLOCKSIZE:
			if ((error = nvpair_value_uint64(elem, &intval)) != 0 ||
			    (error = zvol_set_volblocksize(name,
			    intval)) != 0)
				return (error);
			break;

		default:
			if (nvpair_type(elem) == DATA_TYPE_STRING) {
				if (zfs_prop_get_type(prop) !=
				    prop_type_string)
					return (EINVAL);
				VERIFY(nvpair_value_string(elem, &strval) == 0);
				if ((error = dsl_prop_set(name,
				    nvpair_name(elem), 1, strlen(strval) + 1,
				    strval)) != 0)
					return (error);
			} else if (nvpair_type(elem) == DATA_TYPE_UINT64) {
				const char *unused;

				VERIFY(nvpair_value_uint64(elem, &intval) == 0);

				switch (zfs_prop_get_type(prop)) {
				case prop_type_number:
					break;
				case prop_type_boolean:
					if (intval > 1)
						return (EINVAL);
					break;
				case prop_type_string:
					return (EINVAL);
				case prop_type_index:
					if (zfs_prop_index_to_string(prop,
					    intval, &unused) != 0)
						return (EINVAL);
					break;
				default:
					cmn_err(CE_PANIC, "unknown property "
					    "type");
					break;
				}

				if ((error = dsl_prop_set(name, propname,
				    8, 1, &intval)) != 0)
					return (error);
			} else {
				return (EINVAL);
			}
			break;
		}
	}

	return (0);
}

static int
zfs_ioc_set_prop(zfs_cmd_t *zc)
{
	nvlist_t *nvl;
	int error;
	zfs_prop_t prop;

	/*
	 * If zc_value is set, then this is an attempt to inherit a value.
	 * Otherwise, zc_nvlist refers to a list of properties to set.
	 */
	if (zc->zc_value[0] != '\0') {
		if (!zfs_prop_user(zc->zc_value) &&
		    ((prop = zfs_name_to_prop(zc->zc_value)) ==
		    ZFS_PROP_INVAL ||
		    !zfs_prop_inheritable(prop)))
			return (EINVAL);

		return (dsl_prop_set(zc->zc_name, zc->zc_value, 0, 0, NULL));
	}

	if ((error = get_nvlist(zc, &nvl)) != 0)
		return (error);

	error = zfs_set_prop_nvlist(zc->zc_name, zc->zc_dev,
	    (cred_t *)(uintptr_t)zc->zc_cred, nvl);
	nvlist_free(nvl);
	return (error);
}

static int
zfs_ioc_pool_set_props(zfs_cmd_t *zc)
{
	nvlist_t *nvl;
	int error, reset_bootfs = 0;
	uint64_t objnum;
	zpool_prop_t prop;
	nvpair_t *elem;
	char *propname, *strval;
	spa_t *spa;
	vdev_t *rvdev;
	char *vdev_type;
	objset_t *os;

	if ((error = get_nvlist(zc, &nvl)) != 0)
		return (error);

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0) {
		nvlist_free(nvl);
		return (error);
	}

	if (spa_version(spa) < ZFS_VERSION_BOOTFS) {
		nvlist_free(nvl);
		spa_close(spa, FTAG);
		return (ENOTSUP);
	}

	elem = NULL;
	while ((elem = nvlist_next_nvpair(nvl, elem)) != NULL) {

		propname = nvpair_name(elem);

		if ((prop = zpool_name_to_prop(propname)) ==
		    ZFS_PROP_INVAL) {
			nvlist_free(nvl);
			spa_close(spa, FTAG);
			return (EINVAL);
		}

		switch (prop) {
		case ZFS_PROP_BOOTFS:
			/*
			 * A bootable filesystem can not be on a RAIDZ pool
			 * nor a striped pool with more than 1 device.
			 */
			rvdev = spa->spa_root_vdev;
			vdev_type =
			    rvdev->vdev_child[0]->vdev_ops->vdev_op_type;
			if (strcmp(vdev_type, VDEV_TYPE_RAIDZ) == 0 ||
			    (strcmp(vdev_type, VDEV_TYPE_MIRROR) != 0 &&
			    rvdev->vdev_children > 1)) {
				error = ENOTSUP;
				break;
			}

			reset_bootfs = 1;

			VERIFY(nvpair_value_string(elem, &strval) == 0);
			if (strval == NULL || strval[0] == '\0') {
				objnum =
				    zfs_prop_default_numeric(ZFS_PROP_BOOTFS);
				break;
			}

			if (error = dmu_objset_open(strval, DMU_OST_ZFS,
			    DS_MODE_STANDARD | DS_MODE_READONLY, &os))
				break;
			objnum = dmu_objset_id(os);
			dmu_objset_close(os);
			break;

		default:
			error = EINVAL;
		}

		if (error)
			break;
	}
	if (error == 0) {
		if (reset_bootfs) {
			VERIFY(nvlist_remove(nvl,
			    zpool_prop_to_name(ZFS_PROP_BOOTFS),
			    DATA_TYPE_STRING) == 0);
			VERIFY(nvlist_add_uint64(nvl,
			    zpool_prop_to_name(ZFS_PROP_BOOTFS), objnum) == 0);
		}
		error = spa_set_props(spa, nvl);
	}

	nvlist_free(nvl);
	spa_close(spa, FTAG);

	return (error);
}

static int
zfs_ioc_pool_get_props(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;
	nvlist_t *nvp = NULL;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	error = spa_get_props(spa, &nvp);

	if (error == 0 && zc->zc_nvlist_dst != 0)
		error = put_nvlist(zc, nvp);
	else
		error = EFAULT;

	spa_close(spa, FTAG);

	if (nvp)
		nvlist_free(nvp);
	return (error);
}

static int
zfs_ioc_create_minor(zfs_cmd_t *zc)
{
	return (zvol_create_minor(zc->zc_name, zc->zc_dev));
}

static int
zfs_ioc_remove_minor(zfs_cmd_t *zc)
{
	return (zvol_remove_minor(zc->zc_name));
}

/*
 * Search the vfs list for a specified resource.  Returns a pointer to it
 * or NULL if no suitable entry is found. The caller of this routine
 * is responsible for releasing the returned vfs pointer.
 */
static vfs_t *
zfs_get_vfs(const char *resource)
{
	vfs_t *vfsp;

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(vfsp, &mountlist, mnt_list) {
		if (strcmp(vfsp->mnt_stat.f_mntfromname, resource) == 0) {
			VFS_HOLD(vfsp);
			break;
		}
	}
	mtx_unlock(&mountlist_mtx);
	return (vfsp);
}

static void
zfs_create_cb(objset_t *os, void *arg, dmu_tx_t *tx)
{
	zfs_create_data_t *zc = arg;

	zfs_create_fs(os, (cred_t *)(uintptr_t)zc->zc_cred, tx);
}

static int
zfs_ioc_create(zfs_cmd_t *zc)
{
	objset_t *clone;
	int error = 0;
	zfs_create_data_t cbdata = { 0 };
	void (*cbfunc)(objset_t *os, void *arg, dmu_tx_t *tx);
	dmu_objset_type_t type = zc->zc_objset_type;

	switch (type) {

	case DMU_OST_ZFS:
		cbfunc = zfs_create_cb;
		break;

	case DMU_OST_ZVOL:
		cbfunc = zvol_create_cb;
		break;

	default:
		cbfunc = NULL;
	}
	if (strchr(zc->zc_name, '@'))
		return (EINVAL);

	if (zc->zc_nvlist_src != 0 &&
	    (error = get_nvlist(zc, &cbdata.zc_props)) != 0)
		return (error);

	cbdata.zc_cred = (cred_t *)(uintptr_t)zc->zc_cred;
	cbdata.zc_dev = (dev_t)zc->zc_dev;

	if (zc->zc_value[0] != '\0') {
		/*
		 * We're creating a clone of an existing snapshot.
		 */
		zc->zc_value[sizeof (zc->zc_value) - 1] = '\0';
		if (dataset_namecheck(zc->zc_value, NULL, NULL) != 0) {
			nvlist_free(cbdata.zc_props);
			return (EINVAL);
		}

		error = dmu_objset_open(zc->zc_value, type,
		    DS_MODE_STANDARD | DS_MODE_READONLY, &clone);
		if (error) {
			nvlist_free(cbdata.zc_props);
			return (error);
		}
		error = dmu_objset_create(zc->zc_name, type, clone, NULL, NULL);
		dmu_objset_close(clone);
	} else {
		if (cbfunc == NULL) {
			nvlist_free(cbdata.zc_props);
			return (EINVAL);
		}

		if (type == DMU_OST_ZVOL) {
			uint64_t volsize, volblocksize;

			if (cbdata.zc_props == NULL ||
			    nvlist_lookup_uint64(cbdata.zc_props,
			    zfs_prop_to_name(ZFS_PROP_VOLSIZE),
			    &volsize) != 0) {
				nvlist_free(cbdata.zc_props);
				return (EINVAL);
			}

			if ((error = nvlist_lookup_uint64(cbdata.zc_props,
			    zfs_prop_to_name(ZFS_PROP_VOLBLOCKSIZE),
			    &volblocksize)) != 0 && error != ENOENT) {
				nvlist_free(cbdata.zc_props);
				return (EINVAL);
			}

			if (error != 0)
				volblocksize = zfs_prop_default_numeric(
				    ZFS_PROP_VOLBLOCKSIZE);

			if ((error = zvol_check_volblocksize(
			    volblocksize)) != 0 ||
			    (error = zvol_check_volsize(volsize,
			    volblocksize)) != 0) {
				nvlist_free(cbdata.zc_props);
				return (error);
			}
		}

		error = dmu_objset_create(zc->zc_name, type, NULL, cbfunc,
		    &cbdata);
	}

	/*
	 * It would be nice to do this atomically.
	 */
	if (error == 0) {
		if ((error = zfs_set_prop_nvlist(zc->zc_name,
		    zc->zc_dev, (cred_t *)(uintptr_t)zc->zc_cred,
		    cbdata.zc_props)) != 0)
			(void) dmu_objset_destroy(zc->zc_name);
	}

	nvlist_free(cbdata.zc_props);
	return (error);
}

static int
zfs_ioc_snapshot(zfs_cmd_t *zc)
{
	if (snapshot_namecheck(zc->zc_value, NULL, NULL) != 0)
		return (EINVAL);
	return (dmu_objset_snapshot(zc->zc_name,
	    zc->zc_value, zc->zc_cookie));
}

int
zfs_unmount_snap(char *name, void *arg)
{
	char *snapname = arg;
	char *cp;
	vfs_t *vfsp = NULL;

	/*
	 * Snapshots (which are under .zfs control) must be unmounted
	 * before they can be destroyed.
	 */

	if (snapname) {
		(void) strcat(name, "@");
		(void) strcat(name, snapname);
		vfsp = zfs_get_vfs(name);
		cp = strchr(name, '@');
		*cp = '\0';
	} else if (strchr(name, '@')) {
		vfsp = zfs_get_vfs(name);
	}

	if (vfsp) {
		/*
		 * Always force the unmount for snapshots.
		 */
		int flag = MS_FORCE;
		int err;

		if ((err = vn_vfswlock(vfsp->vfs_vnodecovered)) != 0) {
			VFS_RELE(vfsp);
			return (err);
		}
		VFS_RELE(vfsp);
		mtx_lock(&Giant);	/* dounmount() */
		dounmount(vfsp, flag, curthread);
		mtx_unlock(&Giant);	/* dounmount() */
	}
	return (0);
}

static int
zfs_ioc_destroy_snaps(zfs_cmd_t *zc)
{
	int err;

	if (snapshot_namecheck(zc->zc_value, NULL, NULL) != 0)
		return (EINVAL);
	err = dmu_objset_find(zc->zc_name,
	    zfs_unmount_snap, zc->zc_value, DS_FIND_CHILDREN);
	if (err)
		return (err);
	return (dmu_snapshots_destroy(zc->zc_name, zc->zc_value));
}

static int
zfs_ioc_destroy(zfs_cmd_t *zc)
{
	if (strchr(zc->zc_name, '@') && zc->zc_objset_type == DMU_OST_ZFS) {
		int err = zfs_unmount_snap(zc->zc_name, NULL);
		if (err)
			return (err);
	}

	return (dmu_objset_destroy(zc->zc_name));
}

static int
zfs_ioc_rollback(zfs_cmd_t *zc)
{
	return (dmu_objset_rollback(zc->zc_name));
}

static int
zfs_ioc_rename(zfs_cmd_t *zc)
{
	int recursive = zc->zc_cookie & 1;

	zc->zc_value[sizeof (zc->zc_value) - 1] = '\0';
	if (dataset_namecheck(zc->zc_value, NULL, NULL) != 0)
		return (EINVAL);

	/*
	 * Unmount snapshot unless we're doing a recursive rename,
	 * in which case the dataset code figures out which snapshots
	 * to unmount.
	 */
	if (!recursive && strchr(zc->zc_name, '@') != NULL &&
	    zc->zc_objset_type == DMU_OST_ZFS) {
		int err = zfs_unmount_snap(zc->zc_name, NULL);
		if (err)
			return (err);
	}

	return (dmu_objset_rename(zc->zc_name, zc->zc_value, recursive));
}

static int
zfs_ioc_recvbackup(zfs_cmd_t *zc)
{
	kthread_t *td = curthread;
	struct file *fp;
	int error;
	offset_t new_off;

	if (dataset_namecheck(zc->zc_value, NULL, NULL) != 0 ||
	    strchr(zc->zc_value, '@') == NULL)
		return (EINVAL);

	error = fget_read(td, zc->zc_cookie, &fp);
	if (error)
		return (error);

	error = dmu_recvbackup(zc->zc_value, &zc->zc_begin_record,
	    &zc->zc_cookie, (boolean_t)zc->zc_guid, fp,
	    fp->f_offset);

	new_off = fp->f_offset + zc->zc_cookie;
	fp->f_offset = new_off;

	fdrop(fp, td);
	return (error);
}

static int
zfs_ioc_sendbackup(zfs_cmd_t *zc)
{
	kthread_t *td = curthread;
	struct file *fp;
	objset_t *fromsnap = NULL;
	objset_t *tosnap;
	int error, fd;

	error = dmu_objset_open(zc->zc_name, DMU_OST_ANY,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &tosnap);
	if (error)
		return (error);

	if (zc->zc_value[0] != '\0') {
		char buf[MAXPATHLEN];
		char *cp;

		(void) strncpy(buf, zc->zc_name, sizeof (buf));
		cp = strchr(buf, '@');
		if (cp)
			*(cp+1) = 0;
		(void) strlcat(buf, zc->zc_value, sizeof (buf));
		error = dmu_objset_open(buf, DMU_OST_ANY,
		    DS_MODE_STANDARD | DS_MODE_READONLY, &fromsnap);
		if (error) {
			dmu_objset_close(tosnap);
			return (error);
		}
	}

	fd = zc->zc_cookie;
	error = fget_write(td, fd, &fp);
	if (error) {
		dmu_objset_close(tosnap);
		if (fromsnap)
			dmu_objset_close(fromsnap);
		return (error);
	}

	error = dmu_sendbackup(tosnap, fromsnap, fp);

	fdrop(fp, td);
	if (fromsnap)
		dmu_objset_close(fromsnap);
	dmu_objset_close(tosnap);
	return (error);
}

static int
zfs_ioc_inject_fault(zfs_cmd_t *zc)
{
	int id, error;

	error = zio_inject_fault(zc->zc_name, (int)zc->zc_guid, &id,
	    &zc->zc_inject_record);

	if (error == 0)
		zc->zc_guid = (uint64_t)id;

	return (error);
}

static int
zfs_ioc_clear_fault(zfs_cmd_t *zc)
{
	return (zio_clear_fault((int)zc->zc_guid));
}

static int
zfs_ioc_inject_list_next(zfs_cmd_t *zc)
{
	int id = (int)zc->zc_guid;
	int error;

	error = zio_inject_list_next(&id, zc->zc_name, sizeof (zc->zc_name),
	    &zc->zc_inject_record);

	zc->zc_guid = id;

	return (error);
}

static int
zfs_ioc_error_log(zfs_cmd_t *zc)
{
	spa_t *spa;
	int error;
	size_t count = (size_t)zc->zc_nvlist_dst_size;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	error = spa_get_errlog(spa, (void *)(uintptr_t)zc->zc_nvlist_dst,
	    &count);
	if (error == 0)
		zc->zc_nvlist_dst_size = count;
	else
		zc->zc_nvlist_dst_size = spa_get_errlog_size(spa);

	spa_close(spa, FTAG);

	return (error);
}

static int
zfs_ioc_clear(zfs_cmd_t *zc)
{
	spa_t *spa;
	vdev_t *vd;
	int error;

	if ((error = spa_open(zc->zc_name, &spa, FTAG)) != 0)
		return (error);

	spa_config_enter(spa, RW_WRITER, FTAG);

	if (zc->zc_guid == 0) {
		vd = NULL;
	} else if ((vd = spa_lookup_by_guid(spa, zc->zc_guid)) == NULL) {
		spa_config_exit(spa, FTAG);
		spa_close(spa, FTAG);
		return (ENODEV);
	}

	vdev_clear(spa, vd);

	spa_config_exit(spa, FTAG);

	spa_close(spa, FTAG);

	return (0);
}

static int
zfs_ioc_promote(zfs_cmd_t *zc)
{
	char *cp;

	/*
	 * We don't need to unmount *all* the origin fs's snapshots, but
	 * it's easier.
	 */
	cp = strchr(zc->zc_value, '@');
	if (cp)
		*cp = '\0';
	(void) dmu_objset_find(zc->zc_value,
	    zfs_unmount_snap, NULL, DS_FIND_SNAPSHOTS);
	return (dsl_dataset_promote(zc->zc_name));
}

static int
zfs_ioc_jail(zfs_cmd_t *zc)
{

	return (zone_dataset_attach((cred_t *)(uintptr_t)zc->zc_cred,
	    zc->zc_name, (int)zc->zc_jailid));
}

static int
zfs_ioc_unjail(zfs_cmd_t *zc)
{

	return (zone_dataset_detach((cred_t *)(uintptr_t)zc->zc_cred,
	    zc->zc_name, (int)zc->zc_jailid));
}

static zfs_ioc_vec_t zfs_ioc_vec[] = {
	{ zfs_ioc_pool_create,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_destroy,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_import,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_export,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_configs,		zfs_secpolicy_none,	no_name },
	{ zfs_ioc_pool_stats,		zfs_secpolicy_read,	pool_name },
	{ zfs_ioc_pool_tryimport,	zfs_secpolicy_config,	no_name },
	{ zfs_ioc_pool_scrub,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_freeze,		zfs_secpolicy_config,	no_name },
	{ zfs_ioc_pool_upgrade,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_get_history,	zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_log_history,	zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_add,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_remove,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_online,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_offline,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_attach,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_detach,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_vdev_setpath,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_objset_stats,		zfs_secpolicy_read,	dataset_name },
	{ zfs_ioc_dataset_list_next,	zfs_secpolicy_read,	dataset_name },
	{ zfs_ioc_snapshot_list_next,	zfs_secpolicy_read,	dataset_name },
	{ zfs_ioc_set_prop,		zfs_secpolicy_write,	dataset_name },
	{ zfs_ioc_create_minor,		zfs_secpolicy_config,	dataset_name },
	{ zfs_ioc_remove_minor,		zfs_secpolicy_config,	dataset_name },
	{ zfs_ioc_create,		zfs_secpolicy_parent,	dataset_name },
	{ zfs_ioc_destroy,		zfs_secpolicy_parent,	dataset_name },
	{ zfs_ioc_rollback,		zfs_secpolicy_write,	dataset_name },
	{ zfs_ioc_rename,		zfs_secpolicy_write,	dataset_name },
	{ zfs_ioc_recvbackup,		zfs_secpolicy_write,	dataset_name },
	{ zfs_ioc_sendbackup,		zfs_secpolicy_operator,	dataset_name },
	{ zfs_ioc_inject_fault,		zfs_secpolicy_inject,	no_name },
	{ zfs_ioc_clear_fault,		zfs_secpolicy_inject,	no_name },
	{ zfs_ioc_inject_list_next,	zfs_secpolicy_inject,	no_name },
	{ zfs_ioc_error_log,		zfs_secpolicy_inject,	pool_name },
	{ zfs_ioc_clear,		zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_promote,		zfs_secpolicy_write,	dataset_name },
	{ zfs_ioc_destroy_snaps,	zfs_secpolicy_write,	dataset_name },
	{ zfs_ioc_snapshot,		zfs_secpolicy_operator,	dataset_name },
	{ zfs_ioc_dsobj_to_dsname,	zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_obj_to_path,		zfs_secpolicy_config,	no_name },
	{ zfs_ioc_pool_set_props,	zfs_secpolicy_config,	pool_name },
	{ zfs_ioc_pool_get_props,	zfs_secpolicy_read,	pool_name },
	{ zfs_ioc_jail,			zfs_secpolicy_config,	dataset_name },
	{ zfs_ioc_unjail,		zfs_secpolicy_config,	dataset_name }
};

static int
zfsdev_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td)
{
	zfs_cmd_t *zc = (void *)addr;
	uint_t vec;
	int error;

	vec = ZFS_IOC(cmd);

	if (vec >= sizeof (zfs_ioc_vec) / sizeof (zfs_ioc_vec[0]))
		return (EINVAL);

	zc->zc_cred = (uintptr_t)td->td_ucred;
	zc->zc_dev = (uintptr_t)dev;
	error = zfs_ioc_vec[vec].zvec_secpolicy(zc->zc_name, td->td_ucred);

	/*
	 * Ensure that all pool/dataset names are valid before we pass down to
	 * the lower layers.
	 */
	if (error == 0) {
		zc->zc_name[sizeof (zc->zc_name) - 1] = '\0';
		switch (zfs_ioc_vec[vec].zvec_namecheck) {
		case pool_name:
			if (pool_namecheck(zc->zc_name, NULL, NULL) != 0)
				error = EINVAL;
			break;

		case dataset_name:
			if (dataset_namecheck(zc->zc_name, NULL, NULL) != 0)
				error = EINVAL;
			break;

		case no_name:
			break;
		}
	}

	if (error == 0)
		error = zfs_ioc_vec[vec].zvec_func(zc);

	return (error);
}

/*
 * OK, so this is a little weird.
 *
 * /dev/zfs is the control node, i.e. minor 0.
 * /dev/zvol/[r]dsk/pool/dataset are the zvols, minor > 0.
 *
 * /dev/zfs has basically nothing to do except serve up ioctls,
 * so most of the standard driver entry points are in zvol.c.
 */
static struct cdevsw zfs_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	zfsdev_ioctl,
	.d_name =	ZFS_DEV_NAME
};

static void
zfsdev_init(void)
{
	zfsdev = make_dev(&zfs_cdevsw, 0x0, UID_ROOT, GID_OPERATOR, 0660,
	    ZFS_DEV_NAME);
}

static void
zfsdev_fini(void)
{
	if (zfsdev != NULL)
		destroy_dev(zfsdev);
}

static struct task zfs_start_task;
static struct root_hold_token *zfs_root_token;

static void
zfs_start(void *context __unused, int pending __unused)
{

	zfsdev_init();
	spa_init(FREAD | FWRITE);
	zfs_init();
	zvol_init();
	printf("ZFS storage pool version " ZFS_VERSION_STRING "\n");
	root_mount_rel(zfs_root_token);
}

static int
zfs_modevent(module_t mod, int type, void *unused __unused)
{
	int error;

	error = EOPNOTSUPP;
	switch (type) {
	case MOD_LOAD:
		zfs_root_token = root_mount_hold("ZFS");
		printf("WARNING: ZFS is considered to be an experimental "
		    "feature in FreeBSD.\n");
		TASK_INIT(&zfs_start_task, 0, zfs_start, NULL);
		taskqueue_enqueue(taskqueue_thread, &zfs_start_task);
		error = 0;
		break;
	case MOD_UNLOAD:
		if (spa_busy() || zfs_busy() || zvol_busy() ||
		    zio_injection_enabled) {
			error = EBUSY;
			break;
		}
		zvol_fini();
		zfs_fini();
		spa_fini();
		zfsdev_fini();
		error = 0;
		break;
	}
	return (error);
}

static moduledata_t zfs_mod = {
	"zfsctrl",
	zfs_modevent,
	0
};
DECLARE_MODULE(zfsctrl, zfs_mod, SI_SUB_VFS, SI_ORDER_ANY);
MODULE_DEPEND(zfsctrl, opensolaris, 1, 1, 1);
