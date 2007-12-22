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
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <devid.h>
#include <dirent.h>
#include <fcntl.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/zfs_ioctl.h>
#include <sys/zio.h>
#include <strings.h>
#include <umem.h>

#include "zfs_namecheck.h"
#include "zfs_prop.h"
#include "libzfs_impl.h"

/*
 * Validate the given pool name, optionally putting an extended error message in
 * 'buf'.
 */
static boolean_t
zpool_name_valid(libzfs_handle_t *hdl, boolean_t isopen, const char *pool)
{
	namecheck_err_t why;
	char what;
	int ret;

	ret = pool_namecheck(pool, &why, &what);

	/*
	 * The rules for reserved pool names were extended at a later point.
	 * But we need to support users with existing pools that may now be
	 * invalid.  So we only check for this expanded set of names during a
	 * create (or import), and only in userland.
	 */
	if (ret == 0 && !isopen &&
	    (strncmp(pool, "mirror", 6) == 0 ||
	    strncmp(pool, "raidz", 5) == 0 ||
	    strncmp(pool, "spare", 5) == 0)) {
		zfs_error_aux(hdl,
		    dgettext(TEXT_DOMAIN, "name is reserved"));
		return (B_FALSE);
	}


	if (ret != 0) {
		if (hdl != NULL) {
			switch (why) {
			case NAME_ERR_TOOLONG:
				zfs_error_aux(hdl,
				    dgettext(TEXT_DOMAIN, "name is too long"));
				break;

			case NAME_ERR_INVALCHAR:
				zfs_error_aux(hdl,
				    dgettext(TEXT_DOMAIN, "invalid character "
				    "'%c' in pool name"), what);
				break;

			case NAME_ERR_NOLETTER:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "name must begin with a letter"));
				break;

			case NAME_ERR_RESERVED:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "name is reserved"));
				break;

			case NAME_ERR_DISKLIKE:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "pool name is reserved"));
				break;

			case NAME_ERR_LEADING_SLASH:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "leading slash in name"));
				break;

			case NAME_ERR_EMPTY_COMPONENT:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "empty component in name"));
				break;

			case NAME_ERR_TRAILING_SLASH:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "trailing slash in name"));
				break;

			case NAME_ERR_MULTIPLE_AT:
				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "multiple '@' delimiters in name"));
				break;

			}
		}
		return (B_FALSE);
	}

	return (B_TRUE);
}

static int
zpool_get_all_props(zpool_handle_t *zhp)
{
	zfs_cmd_t zc = { 0 };
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));

	if (zcmd_alloc_dst_nvlist(hdl, &zc, 0) != 0)
		return (-1);

	while (ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_GET_PROPS, &zc) != 0) {
		if (errno == ENOMEM) {
			if (zcmd_expand_dst_nvlist(hdl, &zc) != 0) {
				zcmd_free_nvlists(&zc);
				return (-1);
			}
		} else {
			zcmd_free_nvlists(&zc);
			return (-1);
		}
	}

	if (zcmd_read_dst_nvlist(hdl, &zc, &zhp->zpool_props) != 0) {
		zcmd_free_nvlists(&zc);
		return (-1);
	}

	zcmd_free_nvlists(&zc);

	return (0);
}

/*
 * Open a handle to the given pool, even if the pool is currently in the FAULTED
 * state.
 */
zpool_handle_t *
zpool_open_canfail(libzfs_handle_t *hdl, const char *pool)
{
	zpool_handle_t *zhp;
	boolean_t missing;

	/*
	 * Make sure the pool name is valid.
	 */
	if (!zpool_name_valid(hdl, B_TRUE, pool)) {
		(void) zfs_error_fmt(hdl, EZFS_INVALIDNAME,
		    dgettext(TEXT_DOMAIN, "cannot open '%s'"),
		    pool);
		return (NULL);
	}

	if ((zhp = zfs_alloc(hdl, sizeof (zpool_handle_t))) == NULL)
		return (NULL);

	zhp->zpool_hdl = hdl;
	(void) strlcpy(zhp->zpool_name, pool, sizeof (zhp->zpool_name));

	if (zpool_refresh_stats(zhp, &missing) != 0) {
		zpool_close(zhp);
		return (NULL);
	}

	if (missing) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "no such pool"));
		(void) zfs_error_fmt(hdl, EZFS_NOENT,
		    dgettext(TEXT_DOMAIN, "cannot open '%s'"),
		    pool);
		zpool_close(zhp);
		return (NULL);
	}

	return (zhp);
}

/*
 * Like the above, but silent on error.  Used when iterating over pools (because
 * the configuration cache may be out of date).
 */
int
zpool_open_silent(libzfs_handle_t *hdl, const char *pool, zpool_handle_t **ret)
{
	zpool_handle_t *zhp;
	boolean_t missing;

	if ((zhp = zfs_alloc(hdl, sizeof (zpool_handle_t))) == NULL)
		return (-1);

	zhp->zpool_hdl = hdl;
	(void) strlcpy(zhp->zpool_name, pool, sizeof (zhp->zpool_name));

	if (zpool_refresh_stats(zhp, &missing) != 0) {
		zpool_close(zhp);
		return (-1);
	}

	if (missing) {
		zpool_close(zhp);
		*ret = NULL;
		return (0);
	}

	*ret = zhp;
	return (0);
}

/*
 * Similar to zpool_open_canfail(), but refuses to open pools in the faulted
 * state.
 */
zpool_handle_t *
zpool_open(libzfs_handle_t *hdl, const char *pool)
{
	zpool_handle_t *zhp;

	if ((zhp = zpool_open_canfail(hdl, pool)) == NULL)
		return (NULL);

	if (zhp->zpool_state == POOL_STATE_UNAVAIL) {
		(void) zfs_error_fmt(hdl, EZFS_POOLUNAVAIL,
		    dgettext(TEXT_DOMAIN, "cannot open '%s'"), zhp->zpool_name);
		zpool_close(zhp);
		return (NULL);
	}

	return (zhp);
}

/*
 * Close the handle.  Simply frees the memory associated with the handle.
 */
void
zpool_close(zpool_handle_t *zhp)
{
	if (zhp->zpool_config)
		nvlist_free(zhp->zpool_config);
	if (zhp->zpool_old_config)
		nvlist_free(zhp->zpool_old_config);
	if (zhp->zpool_props)
		nvlist_free(zhp->zpool_props);
	free(zhp);
}

/*
 * Return the name of the pool.
 */
const char *
zpool_get_name(zpool_handle_t *zhp)
{
	return (zhp->zpool_name);
}

/*
 * Return the GUID of the pool.
 */
uint64_t
zpool_get_guid(zpool_handle_t *zhp)
{
	uint64_t guid;

	verify(nvlist_lookup_uint64(zhp->zpool_config, ZPOOL_CONFIG_POOL_GUID,
	    &guid) == 0);
	return (guid);
}

/*
 * Return the version of the pool.
 */
uint64_t
zpool_get_version(zpool_handle_t *zhp)
{
	uint64_t version;

	verify(nvlist_lookup_uint64(zhp->zpool_config, ZPOOL_CONFIG_VERSION,
	    &version) == 0);

	return (version);
}

/*
 * Return the amount of space currently consumed by the pool.
 */
uint64_t
zpool_get_space_used(zpool_handle_t *zhp)
{
	nvlist_t *nvroot;
	vdev_stat_t *vs;
	uint_t vsc;

	verify(nvlist_lookup_nvlist(zhp->zpool_config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);
	verify(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_STATS,
	    (uint64_t **)&vs, &vsc) == 0);

	return (vs->vs_alloc);
}

/*
 * Return the total space in the pool.
 */
uint64_t
zpool_get_space_total(zpool_handle_t *zhp)
{
	nvlist_t *nvroot;
	vdev_stat_t *vs;
	uint_t vsc;

	verify(nvlist_lookup_nvlist(zhp->zpool_config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);
	verify(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_STATS,
	    (uint64_t **)&vs, &vsc) == 0);

	return (vs->vs_space);
}

/*
 * Return the alternate root for this pool, if any.
 */
int
zpool_get_root(zpool_handle_t *zhp, char *buf, size_t buflen)
{
	zfs_cmd_t zc = { 0 };

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_OBJSET_STATS, &zc) != 0 ||
	    zc.zc_value[0] == '\0')
		return (-1);

	(void) strlcpy(buf, zc.zc_value, buflen);

	return (0);
}

/*
 * Return the state of the pool (ACTIVE or UNAVAILABLE)
 */
int
zpool_get_state(zpool_handle_t *zhp)
{
	return (zhp->zpool_state);
}

/*
 * Create the named pool, using the provided vdev list.  It is assumed
 * that the consumer has already validated the contents of the nvlist, so we
 * don't have to worry about error semantics.
 */
int
zpool_create(libzfs_handle_t *hdl, const char *pool, nvlist_t *nvroot,
    const char *altroot)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];

	(void) snprintf(msg, sizeof (msg), dgettext(TEXT_DOMAIN,
	    "cannot create '%s'"), pool);

	if (!zpool_name_valid(hdl, B_FALSE, pool))
		return (zfs_error(hdl, EZFS_INVALIDNAME, msg));

	if (altroot != NULL && altroot[0] != '/')
		return (zfs_error_fmt(hdl, EZFS_BADPATH,
		    dgettext(TEXT_DOMAIN, "bad alternate root '%s'"), altroot));

	if (zcmd_write_src_nvlist(hdl, &zc, nvroot, NULL) != 0)
		return (-1);

	(void) strlcpy(zc.zc_name, pool, sizeof (zc.zc_name));

	if (altroot != NULL)
		(void) strlcpy(zc.zc_value, altroot, sizeof (zc.zc_value));

	if (ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_CREATE, &zc) != 0) {
		zcmd_free_nvlists(&zc);

		switch (errno) {
		case EBUSY:
			/*
			 * This can happen if the user has specified the same
			 * device multiple times.  We can't reliably detect this
			 * until we try to add it and see we already have a
			 * label.
			 */
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "one or more vdevs refer to the same device"));
			return (zfs_error(hdl, EZFS_BADDEV, msg));

		case EOVERFLOW:
			/*
			 * This occurs when one of the devices is below
			 * SPA_MINDEVSIZE.  Unfortunately, we can't detect which
			 * device was the problem device since there's no
			 * reliable way to determine device size from userland.
			 */
			{
				char buf[64];

				zfs_nicenum(SPA_MINDEVSIZE, buf, sizeof (buf));

				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "one or more devices is less than the "
				    "minimum size (%s)"), buf);
			}
			return (zfs_error(hdl, EZFS_BADDEV, msg));

		case ENOSPC:
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "one or more devices is out of space"));
			return (zfs_error(hdl, EZFS_BADDEV, msg));

		default:
			return (zpool_standard_error(hdl, errno, msg));
		}
	}

	zcmd_free_nvlists(&zc);

	/*
	 * If this is an alternate root pool, then we automatically set the
	 * mountpoint of the root dataset to be '/'.
	 */
	if (altroot != NULL) {
		zfs_handle_t *zhp;

		verify((zhp = zfs_open(hdl, pool, ZFS_TYPE_ANY)) != NULL);
		verify(zfs_prop_set(zhp, zfs_prop_to_name(ZFS_PROP_MOUNTPOINT),
		    "/") == 0);

		zfs_close(zhp);
	}

	return (0);
}

/*
 * Destroy the given pool.  It is up to the caller to ensure that there are no
 * datasets left in the pool.
 */
int
zpool_destroy(zpool_handle_t *zhp)
{
	zfs_cmd_t zc = { 0 };
	zfs_handle_t *zfp = NULL;
	libzfs_handle_t *hdl = zhp->zpool_hdl;
	char msg[1024];

	if (zhp->zpool_state == POOL_STATE_ACTIVE &&
	    (zfp = zfs_open(zhp->zpool_hdl, zhp->zpool_name,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (-1);

	if (zpool_remove_zvol_links(zhp) != 0)
		return (-1);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));

	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_POOL_DESTROY, &zc) != 0) {
		(void) snprintf(msg, sizeof (msg), dgettext(TEXT_DOMAIN,
		    "cannot destroy '%s'"), zhp->zpool_name);

		if (errno == EROFS) {
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "one or more devices is read only"));
			(void) zfs_error(hdl, EZFS_BADDEV, msg);
		} else {
			(void) zpool_standard_error(hdl, errno, msg);
		}

		if (zfp)
			zfs_close(zfp);
		return (-1);
	}

	if (zfp) {
		remove_mountpoint(zfp);
		zfs_close(zfp);
	}

	return (0);
}

/*
 * Add the given vdevs to the pool.  The caller must have already performed the
 * necessary verification to ensure that the vdev specification is well-formed.
 */
int
zpool_add(zpool_handle_t *zhp, nvlist_t *nvroot)
{
	zfs_cmd_t zc = { 0 };
	int ret;
	libzfs_handle_t *hdl = zhp->zpool_hdl;
	char msg[1024];
	nvlist_t **spares;
	uint_t nspares;

	(void) snprintf(msg, sizeof (msg), dgettext(TEXT_DOMAIN,
	    "cannot add to '%s'"), zhp->zpool_name);

	if (zpool_get_version(zhp) < ZFS_VERSION_SPARES &&
	    nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "pool must be "
		    "upgraded to add hot spares"));
		return (zfs_error(hdl, EZFS_BADVERSION, msg));
	}

	if (zcmd_write_src_nvlist(hdl, &zc, nvroot, NULL) != 0)
		return (-1);
	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));

	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_VDEV_ADD, &zc) != 0) {
		switch (errno) {
		case EBUSY:
			/*
			 * This can happen if the user has specified the same
			 * device multiple times.  We can't reliably detect this
			 * until we try to add it and see we already have a
			 * label.
			 */
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "one or more vdevs refer to the same device"));
			(void) zfs_error(hdl, EZFS_BADDEV, msg);
			break;

		case EOVERFLOW:
			/*
			 * This occurrs when one of the devices is below
			 * SPA_MINDEVSIZE.  Unfortunately, we can't detect which
			 * device was the problem device since there's no
			 * reliable way to determine device size from userland.
			 */
			{
				char buf[64];

				zfs_nicenum(SPA_MINDEVSIZE, buf, sizeof (buf));

				zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
				    "device is less than the minimum "
				    "size (%s)"), buf);
			}
			(void) zfs_error(hdl, EZFS_BADDEV, msg);
			break;

		case ENOTSUP:
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "pool must be upgraded to add raidz2 vdevs"));
			(void) zfs_error(hdl, EZFS_BADVERSION, msg);
			break;

		case EDOM:
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "root pool can not have concatenated devices"));
			(void) zfs_error(hdl, EZFS_POOL_NOTSUP, msg);
			break;

		default:
			(void) zpool_standard_error(hdl, errno, msg);
		}

		ret = -1;
	} else {
		ret = 0;
	}

	zcmd_free_nvlists(&zc);

	return (ret);
}

/*
 * Exports the pool from the system.  The caller must ensure that there are no
 * mounted datasets in the pool.
 */
int
zpool_export(zpool_handle_t *zhp)
{
	zfs_cmd_t zc = { 0 };

	if (zpool_remove_zvol_links(zhp) != 0)
		return (-1);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));

	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_POOL_EXPORT, &zc) != 0)
		return (zpool_standard_error_fmt(zhp->zpool_hdl, errno,
		    dgettext(TEXT_DOMAIN, "cannot export '%s'"),
		    zhp->zpool_name));
	return (0);
}

/*
 * Import the given pool using the known configuration.  The configuration
 * should have come from zpool_find_import().  The 'newname' and 'altroot'
 * parameters control whether the pool is imported with a different name or with
 * an alternate root, respectively.
 */
int
zpool_import(libzfs_handle_t *hdl, nvlist_t *config, const char *newname,
    const char *altroot)
{
	zfs_cmd_t zc = { 0 };
	char *thename;
	char *origname;
	int ret;

	verify(nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME,
	    &origname) == 0);

	if (newname != NULL) {
		if (!zpool_name_valid(hdl, B_FALSE, newname))
			return (zfs_error_fmt(hdl, EZFS_INVALIDNAME,
			    dgettext(TEXT_DOMAIN, "cannot import '%s'"),
			    newname));
		thename = (char *)newname;
	} else {
		thename = origname;
	}

	if (altroot != NULL && altroot[0] != '/')
		return (zfs_error_fmt(hdl, EZFS_BADPATH,
		    dgettext(TEXT_DOMAIN, "bad alternate root '%s'"),
		    altroot));

	(void) strlcpy(zc.zc_name, thename, sizeof (zc.zc_name));

	if (altroot != NULL)
		(void) strlcpy(zc.zc_value, altroot, sizeof (zc.zc_value));
	else
		zc.zc_value[0] = '\0';

	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
	    &zc.zc_guid) == 0);

	if (zcmd_write_src_nvlist(hdl, &zc, config, NULL) != 0)
		return (-1);

	ret = 0;
	if (ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_IMPORT, &zc) != 0) {
		char desc[1024];
		if (newname == NULL)
			(void) snprintf(desc, sizeof (desc),
			    dgettext(TEXT_DOMAIN, "cannot import '%s'"),
			    thename);
		else
			(void) snprintf(desc, sizeof (desc),
			    dgettext(TEXT_DOMAIN, "cannot import '%s' as '%s'"),
			    origname, thename);

		switch (errno) {
		case ENOTSUP:
			/*
			 * Unsupported version.
			 */
			(void) zfs_error(hdl, EZFS_BADVERSION, desc);
			break;

		case EINVAL:
			(void) zfs_error(hdl, EZFS_INVALCONFIG, desc);
			break;

		default:
			(void) zpool_standard_error(hdl, errno, desc);
		}

		ret = -1;
	} else {
		zpool_handle_t *zhp;
		/*
		 * This should never fail, but play it safe anyway.
		 */
		if (zpool_open_silent(hdl, thename, &zhp) != 0) {
			ret = -1;
		} else if (zhp != NULL) {
			ret = zpool_create_zvol_links(zhp);
			zpool_close(zhp);
		}
	}

	zcmd_free_nvlists(&zc);
	return (ret);
}

/*
 * Scrub the pool.
 */
int
zpool_scrub(zpool_handle_t *zhp, pool_scrub_type_t type)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	zc.zc_cookie = type;

	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_POOL_SCRUB, &zc) == 0)
		return (0);

	(void) snprintf(msg, sizeof (msg),
	    dgettext(TEXT_DOMAIN, "cannot scrub %s"), zc.zc_name);

	if (errno == EBUSY)
		return (zfs_error(hdl, EZFS_RESILVERING, msg));
	else
		return (zpool_standard_error(hdl, errno, msg));
}

/*
 * 'avail_spare' is set to TRUE if the provided guid refers to an AVAIL
 * spare; but FALSE if its an INUSE spare.
 */
static nvlist_t *
vdev_to_nvlist_iter(nvlist_t *nv, const char *search, uint64_t guid,
    boolean_t *avail_spare)
{
	uint_t c, children;
	nvlist_t **child;
	uint64_t theguid, present;
	char *path;
	uint64_t wholedisk = 0;
	nvlist_t *ret;

	verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &theguid) == 0);

	if (search == NULL &&
	    nvlist_lookup_uint64(nv, ZPOOL_CONFIG_NOT_PRESENT, &present) == 0) {
		/*
		 * If the device has never been present since import, the only
		 * reliable way to match the vdev is by GUID.
		 */
		if (theguid == guid)
			return (nv);
	} else if (search != NULL &&
	    nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) == 0) {
		(void) nvlist_lookup_uint64(nv, ZPOOL_CONFIG_WHOLE_DISK,
		    &wholedisk);
		if (wholedisk) {
			/*
			 * For whole disks, the internal path has 's0', but the
			 * path passed in by the user doesn't.
			 */
			if (strlen(search) == strlen(path) - 2 &&
			    strncmp(search, path, strlen(search)) == 0)
				return (nv);
		} else if (strcmp(search, path) == 0) {
			return (nv);
		}
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		return (NULL);

	for (c = 0; c < children; c++)
		if ((ret = vdev_to_nvlist_iter(child[c], search, guid,
		    avail_spare)) != NULL)
			return (ret);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++) {
			if ((ret = vdev_to_nvlist_iter(child[c], search, guid,
			    avail_spare)) != NULL) {
				*avail_spare = B_TRUE;
				return (ret);
			}
		}
	}

	return (NULL);
}

nvlist_t *
zpool_find_vdev(zpool_handle_t *zhp, const char *path, boolean_t *avail_spare)
{
	char buf[MAXPATHLEN];
	const char *search;
	char *end;
	nvlist_t *nvroot;
	uint64_t guid;

	guid = strtoull(path, &end, 10);
	if (guid != 0 && *end == '\0') {
		search = NULL;
	} else if (path[0] != '/') {
		(void) snprintf(buf, sizeof (buf), "%s%s", _PATH_DEV, path);
		search = buf;
	} else {
		search = path;
	}

	verify(nvlist_lookup_nvlist(zhp->zpool_config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);

	*avail_spare = B_FALSE;
	return (vdev_to_nvlist_iter(nvroot, search, guid, avail_spare));
}

/*
 * Returns TRUE if the given guid corresponds to a spare (INUSE or not).
 */
static boolean_t
is_spare(zpool_handle_t *zhp, uint64_t guid)
{
	uint64_t spare_guid;
	nvlist_t *nvroot;
	nvlist_t **spares;
	uint_t nspares;
	int i;

	verify(nvlist_lookup_nvlist(zhp->zpool_config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);
	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		for (i = 0; i < nspares; i++) {
			verify(nvlist_lookup_uint64(spares[i],
			    ZPOOL_CONFIG_GUID, &spare_guid) == 0);
			if (guid == spare_guid)
				return (B_TRUE);
		}
	}

	return (B_FALSE);
}

/*
 * Bring the specified vdev online
 */
int
zpool_vdev_online(zpool_handle_t *zhp, const char *path)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	nvlist_t *tgt;
	boolean_t avail_spare;
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) snprintf(msg, sizeof (msg),
	    dgettext(TEXT_DOMAIN, "cannot online %s"), path);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if ((tgt = zpool_find_vdev(zhp, path, &avail_spare)) == NULL)
		return (zfs_error(hdl, EZFS_NODEVICE, msg));

	verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID, &zc.zc_guid) == 0);

	if (avail_spare || is_spare(zhp, zc.zc_guid) == B_TRUE)
		return (zfs_error(hdl, EZFS_ISSPARE, msg));

	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_VDEV_ONLINE, &zc) == 0)
		return (0);

	return (zpool_standard_error(hdl, errno, msg));
}

/*
 * Take the specified vdev offline
 */
int
zpool_vdev_offline(zpool_handle_t *zhp, const char *path, int istmp)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	nvlist_t *tgt;
	boolean_t avail_spare;
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) snprintf(msg, sizeof (msg),
	    dgettext(TEXT_DOMAIN, "cannot offline %s"), path);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if ((tgt = zpool_find_vdev(zhp, path, &avail_spare)) == NULL)
		return (zfs_error(hdl, EZFS_NODEVICE, msg));

	verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID, &zc.zc_guid) == 0);

	if (avail_spare || is_spare(zhp, zc.zc_guid) == B_TRUE)
		return (zfs_error(hdl, EZFS_ISSPARE, msg));

	zc.zc_cookie = istmp;

	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_VDEV_OFFLINE, &zc) == 0)
		return (0);

	switch (errno) {
	case EBUSY:

		/*
		 * There are no other replicas of this device.
		 */
		return (zfs_error(hdl, EZFS_NOREPLICAS, msg));

	default:
		return (zpool_standard_error(hdl, errno, msg));
	}
}

/*
 * Returns TRUE if the given nvlist is a vdev that was originally swapped in as
 * a hot spare.
 */
static boolean_t
is_replacing_spare(nvlist_t *search, nvlist_t *tgt, int which)
{
	nvlist_t **child;
	uint_t c, children;
	char *type;

	if (nvlist_lookup_nvlist_array(search, ZPOOL_CONFIG_CHILDREN, &child,
	    &children) == 0) {
		verify(nvlist_lookup_string(search, ZPOOL_CONFIG_TYPE,
		    &type) == 0);

		if (strcmp(type, VDEV_TYPE_SPARE) == 0 &&
		    children == 2 && child[which] == tgt)
			return (B_TRUE);

		for (c = 0; c < children; c++)
			if (is_replacing_spare(child[c], tgt, which))
				return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Attach new_disk (fully described by nvroot) to old_disk.
 * If 'replacing' is specified, tne new disk will replace the old one.
 */
int
zpool_vdev_attach(zpool_handle_t *zhp,
    const char *old_disk, const char *new_disk, nvlist_t *nvroot, int replacing)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	int ret;
	nvlist_t *tgt;
	boolean_t avail_spare;
	uint64_t val;
	char *path;
	nvlist_t **child;
	uint_t children;
	nvlist_t *config_root;
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	if (replacing)
		(void) snprintf(msg, sizeof (msg), dgettext(TEXT_DOMAIN,
		    "cannot replace %s with %s"), old_disk, new_disk);
	else
		(void) snprintf(msg, sizeof (msg), dgettext(TEXT_DOMAIN,
		    "cannot attach %s to %s"), new_disk, old_disk);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if ((tgt = zpool_find_vdev(zhp, old_disk, &avail_spare)) == 0)
		return (zfs_error(hdl, EZFS_NODEVICE, msg));

	if (avail_spare)
		return (zfs_error(hdl, EZFS_ISSPARE, msg));

	verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID, &zc.zc_guid) == 0);
	zc.zc_cookie = replacing;

	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0 || children != 1) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "new device must be a single disk"));
		return (zfs_error(hdl, EZFS_INVALCONFIG, msg));
	}

	verify(nvlist_lookup_nvlist(zpool_get_config(zhp, NULL),
	    ZPOOL_CONFIG_VDEV_TREE, &config_root) == 0);

	/*
	 * If the target is a hot spare that has been swapped in, we can only
	 * replace it with another hot spare.
	 */
	if (replacing &&
	    nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_IS_SPARE, &val) == 0 &&
	    nvlist_lookup_string(child[0], ZPOOL_CONFIG_PATH, &path) == 0 &&
	    (zpool_find_vdev(zhp, path, &avail_spare) == NULL ||
	    !avail_spare) && is_replacing_spare(config_root, tgt, 1)) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "can only be replaced by another hot spare"));
		return (zfs_error(hdl, EZFS_BADTARGET, msg));
	}

	/*
	 * If we are attempting to replace a spare, it canot be applied to an
	 * already spared device.
	 */
	if (replacing &&
	    nvlist_lookup_string(child[0], ZPOOL_CONFIG_PATH, &path) == 0 &&
	    zpool_find_vdev(zhp, path, &avail_spare) != NULL && avail_spare &&
	    is_replacing_spare(config_root, tgt, 0)) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "device has already been replaced with a spare"));
		return (zfs_error(hdl, EZFS_BADTARGET, msg));
	}

	if (zcmd_write_src_nvlist(hdl, &zc, nvroot, NULL) != 0)
		return (-1);

	ret = ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_VDEV_ATTACH, &zc);

	zcmd_free_nvlists(&zc);

	if (ret == 0)
		return (0);

	switch (errno) {
	case ENOTSUP:
		/*
		 * Can't attach to or replace this type of vdev.
		 */
		if (replacing)
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "cannot replace a replacing device"));
		else
			zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
			    "can only attach to mirrors and top-level "
			    "disks"));
		(void) zfs_error(hdl, EZFS_BADTARGET, msg);
		break;

	case EINVAL:
		/*
		 * The new device must be a single disk.
		 */
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "new device must be a single disk"));
		(void) zfs_error(hdl, EZFS_INVALCONFIG, msg);
		break;

	case EBUSY:
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "%s is busy"),
		    new_disk);
		(void) zfs_error(hdl, EZFS_BADDEV, msg);
		break;

	case EOVERFLOW:
		/*
		 * The new device is too small.
		 */
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "device is too small"));
		(void) zfs_error(hdl, EZFS_BADDEV, msg);
		break;

	case EDOM:
		/*
		 * The new device has a different alignment requirement.
		 */
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "devices have different sector alignment"));
		(void) zfs_error(hdl, EZFS_BADDEV, msg);
		break;

	case ENAMETOOLONG:
		/*
		 * The resulting top-level vdev spec won't fit in the label.
		 */
		(void) zfs_error(hdl, EZFS_DEVOVERFLOW, msg);
		break;

	default:
		(void) zpool_standard_error(hdl, errno, msg);
	}

	return (-1);
}

/*
 * Detach the specified device.
 */
int
zpool_vdev_detach(zpool_handle_t *zhp, const char *path)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	nvlist_t *tgt;
	boolean_t avail_spare;
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) snprintf(msg, sizeof (msg),
	    dgettext(TEXT_DOMAIN, "cannot detach %s"), path);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if ((tgt = zpool_find_vdev(zhp, path, &avail_spare)) == 0)
		return (zfs_error(hdl, EZFS_NODEVICE, msg));

	if (avail_spare)
		return (zfs_error(hdl, EZFS_ISSPARE, msg));

	verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID, &zc.zc_guid) == 0);

	if (ioctl(hdl->libzfs_fd, ZFS_IOC_VDEV_DETACH, &zc) == 0)
		return (0);

	switch (errno) {

	case ENOTSUP:
		/*
		 * Can't detach from this type of vdev.
		 */
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN, "only "
		    "applicable to mirror and replacing vdevs"));
		(void) zfs_error(zhp->zpool_hdl, EZFS_BADTARGET, msg);
		break;

	case EBUSY:
		/*
		 * There are no other replicas of this device.
		 */
		(void) zfs_error(hdl, EZFS_NOREPLICAS, msg);
		break;

	default:
		(void) zpool_standard_error(hdl, errno, msg);
	}

	return (-1);
}

/*
 * Remove the given device.  Currently, this is supported only for hot spares.
 */
int
zpool_vdev_remove(zpool_handle_t *zhp, const char *path)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	nvlist_t *tgt;
	boolean_t avail_spare;
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) snprintf(msg, sizeof (msg),
	    dgettext(TEXT_DOMAIN, "cannot remove %s"), path);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if ((tgt = zpool_find_vdev(zhp, path, &avail_spare)) == 0)
		return (zfs_error(hdl, EZFS_NODEVICE, msg));

	if (!avail_spare) {
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "only inactive hot spares can be removed"));
		return (zfs_error(hdl, EZFS_NODEVICE, msg));
	}

	verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID, &zc.zc_guid) == 0);

	if (ioctl(hdl->libzfs_fd, ZFS_IOC_VDEV_REMOVE, &zc) == 0)
		return (0);

	return (zpool_standard_error(hdl, errno, msg));
}

/*
 * Clear the errors for the pool, or the particular device if specified.
 */
int
zpool_clear(zpool_handle_t *zhp, const char *path)
{
	zfs_cmd_t zc = { 0 };
	char msg[1024];
	nvlist_t *tgt;
	boolean_t avail_spare;
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	if (path)
		(void) snprintf(msg, sizeof (msg),
		    dgettext(TEXT_DOMAIN, "cannot clear errors for %s"),
		    path);
	else
		(void) snprintf(msg, sizeof (msg),
		    dgettext(TEXT_DOMAIN, "cannot clear errors for %s"),
		    zhp->zpool_name);

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	if (path) {
		if ((tgt = zpool_find_vdev(zhp, path, &avail_spare)) == 0)
			return (zfs_error(hdl, EZFS_NODEVICE, msg));

		if (avail_spare)
			return (zfs_error(hdl, EZFS_ISSPARE, msg));

		verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID,
		    &zc.zc_guid) == 0);
	}

	if (ioctl(hdl->libzfs_fd, ZFS_IOC_CLEAR, &zc) == 0)
		return (0);

	return (zpool_standard_error(hdl, errno, msg));
}

/*
 * Iterate over all zvols in a given pool by walking the /dev/zvol/dsk/<pool>
 * hierarchy.
 */
int
zpool_iter_zvol(zpool_handle_t *zhp, int (*cb)(const char *, void *),
    void *data)
{
	libzfs_handle_t *hdl = zhp->zpool_hdl;
	char (*paths)[MAXPATHLEN];
	char path[MAXPATHLEN];
	size_t size = 4;
	int curr, fd, base, ret = 0;
	DIR *dirp;
	struct dirent *dp;
	struct stat st;

	if ((base = open(ZVOL_FULL_DEV_DIR, O_RDONLY)) < 0)
		return (errno == ENOENT ? 0 : -1);

	snprintf(path, sizeof(path), "%s/%s", ZVOL_FULL_DEV_DIR,
	    zhp->zpool_name);
	if (stat(path, &st) != 0) {
		int err = errno;
		(void) close(base);
		return (err == ENOENT ? 0 : -1);
	}

	/*
	 * Oddly this wasn't a directory -- ignore that failure since we
	 * know there are no links lower in the (non-existant) hierarchy.
	 */
	if (!S_ISDIR(st.st_mode)) {
		(void) close(base);
		return (0);
	}

	if ((paths = zfs_alloc(hdl, size * sizeof (paths[0]))) == NULL) {
		(void) close(base);
		return (-1);
	}

	(void) strlcpy(paths[0], zhp->zpool_name, sizeof (paths[0]));
	curr = 0;

	while (curr >= 0) {
		snprintf(path, sizeof(path), "%s/%s", ZVOL_FULL_DEV_DIR,
		    paths[curr]);
		if (lstat(path, &st) != 0)
			goto err;

		if (S_ISDIR(st.st_mode)) {
			if ((dirp = opendir(path)) == NULL) {
				goto err;
			}

			while ((dp = readdir(dirp)) != NULL) {
				if (dp->d_name[0] == '.')
					continue;

				if (curr + 1 == size) {
					paths = zfs_realloc(hdl, paths,
					    size * sizeof (paths[0]),
					    size * 2 * sizeof (paths[0]));
					if (paths == NULL) {
						(void) closedir(dirp);
						goto err;
					}

					size *= 2;
				}

				(void) strlcpy(paths[curr + 1], paths[curr],
				    sizeof (paths[curr + 1]));
				(void) strlcat(paths[curr], "/",
				    sizeof (paths[curr]));
				(void) strlcat(paths[curr], dp->d_name,
				    sizeof (paths[curr]));
				curr++;
			}

			(void) closedir(dirp);

		} else {
			if ((ret = cb(paths[curr], data)) != 0)
				break;
		}

		curr--;
	}

	free(paths);
	(void) close(base);

	return (ret);

err:
	free(paths);
	(void) close(base);
	return (-1);
}

typedef struct zvol_cb {
	zpool_handle_t *zcb_pool;
	boolean_t zcb_create;
} zvol_cb_t;

/*ARGSUSED*/
static int
do_zvol_create(zfs_handle_t *zhp, void *data)
{
	int ret;

	if (ZFS_IS_VOLUME(zhp))
		(void) zvol_create_link(zhp->zfs_hdl, zhp->zfs_name);

	ret = zfs_iter_children(zhp, do_zvol_create, NULL);

	zfs_close(zhp);

	return (ret);
}

/*
 * Iterate over all zvols in the pool and make any necessary minor nodes.
 */
int
zpool_create_zvol_links(zpool_handle_t *zhp)
{
	zfs_handle_t *zfp;
	int ret;

	/*
	 * If the pool is unavailable, just return success.
	 */
	if ((zfp = make_dataset_handle(zhp->zpool_hdl,
	    zhp->zpool_name)) == NULL)
		return (0);

	ret = zfs_iter_children(zfp, do_zvol_create, NULL);

	zfs_close(zfp);
	return (ret);
}

static int
do_zvol_remove(const char *dataset, void *data)
{
	zpool_handle_t *zhp = data;

	return (zvol_remove_link(zhp->zpool_hdl, dataset));
}

/*
 * Iterate over all zvols in the pool and remove any minor nodes.  We iterate
 * by examining the /dev links so that a corrupted pool doesn't impede this
 * operation.
 */
int
zpool_remove_zvol_links(zpool_handle_t *zhp)
{
	return (zpool_iter_zvol(zhp, do_zvol_remove, zhp));
}

/*
 * Convert from a devid string to a path.
 */
static char *
devid_to_path(char *devid_str)
{
	ddi_devid_t devid;
	char *minor;
	char *path;
	devid_nmlist_t *list = NULL;
	int ret;

	if (devid_str_decode(devid_str, &devid, &minor) != 0)
		return (NULL);

	ret = devid_deviceid_to_nmlist("/dev", devid, minor, &list);

	devid_str_free(minor);
	devid_free(devid);

	if (ret != 0)
		return (NULL);

	if ((path = strdup(list[0].devname)) == NULL)
		return (NULL);

	devid_free_nmlist(list);

	return (path);
}

/*
 * Convert from a path to a devid string.
 */
static char *
path_to_devid(const char *path)
{
	int fd;
	ddi_devid_t devid;
	char *minor, *ret;

	if ((fd = open(path, O_RDONLY)) < 0)
		return (NULL);

	minor = NULL;
	ret = NULL;
	if (devid_get(fd, &devid) == 0) {
		if (devid_get_minor_name(fd, &minor) == 0)
			ret = devid_str_encode(devid, minor);
		if (minor != NULL)
			devid_str_free(minor);
		devid_free(devid);
	}
	(void) close(fd);

	return (ret);
}

/*
 * Issue the necessary ioctl() to update the stored path value for the vdev.  We
 * ignore any failure here, since a common case is for an unprivileged user to
 * type 'zpool status', and we'll display the correct information anyway.
 */
static void
set_path(zpool_handle_t *zhp, nvlist_t *nv, const char *path)
{
	zfs_cmd_t zc = { 0 };

	(void) strncpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	(void) strncpy(zc.zc_value, path, sizeof (zc.zc_value));
	verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID,
	    &zc.zc_guid) == 0);

	(void) ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_VDEV_SETPATH, &zc);
}

/*
 * Given a vdev, return the name to display in iostat.  If the vdev has a path,
 * we use that, stripping off any leading "/dev/dsk/"; if not, we use the type.
 * We also check if this is a whole disk, in which case we strip off the
 * trailing 's0' slice name.
 *
 * This routine is also responsible for identifying when disks have been
 * reconfigured in a new location.  The kernel will have opened the device by
 * devid, but the path will still refer to the old location.  To catch this, we
 * first do a path -> devid translation (which is fast for the common case).  If
 * the devid matches, we're done.  If not, we do a reverse devid -> path
 * translation and issue the appropriate ioctl() to update the path of the vdev.
 * If 'zhp' is NULL, then this is an exported pool, and we don't need to do any
 * of these checks.
 */
char *
zpool_vdev_name(libzfs_handle_t *hdl, zpool_handle_t *zhp, nvlist_t *nv)
{
	char *path, *devid;
	uint64_t value;
	char buf[64];

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_NOT_PRESENT,
	    &value) == 0) {
		verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID,
		    &value) == 0);
		(void) snprintf(buf, sizeof (buf), "%llu",
		    (u_longlong_t)value);
		path = buf;
	} else if (nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) == 0) {

		if (zhp != NULL &&
		    nvlist_lookup_string(nv, ZPOOL_CONFIG_DEVID, &devid) == 0) {
			/*
			 * Determine if the current path is correct.
			 */
			char *newdevid = path_to_devid(path);

			if (newdevid == NULL ||
			    strcmp(devid, newdevid) != 0) {
				char *newpath;

				if ((newpath = devid_to_path(devid)) != NULL) {
					/*
					 * Update the path appropriately.
					 */
					set_path(zhp, nv, newpath);
					if (nvlist_add_string(nv,
					    ZPOOL_CONFIG_PATH, newpath) == 0)
						verify(nvlist_lookup_string(nv,
						    ZPOOL_CONFIG_PATH,
						    &path) == 0);
					free(newpath);
				}
			}

			if (newdevid)
				devid_str_free(newdevid);
		}

		if (strncmp(path, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
			path += sizeof(_PATH_DEV) - 1;

		if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_WHOLE_DISK,
		    &value) == 0 && value) {
			char *tmp = zfs_strdup(hdl, path);
			if (tmp == NULL)
				return (NULL);
			tmp[strlen(path) - 2] = '\0';
			return (tmp);
		}
	} else {
		verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &path) == 0);

		/*
		 * If it's a raidz device, we need to stick in the parity level.
		 */
		if (strcmp(path, VDEV_TYPE_RAIDZ) == 0) {
			verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_NPARITY,
			    &value) == 0);
			(void) snprintf(buf, sizeof (buf), "%s%llu", path,
			    (u_longlong_t)value);
			path = buf;
		}
	}

	return (zfs_strdup(hdl, path));
}

static int
zbookmark_compare(const void *a, const void *b)
{
	return (memcmp(a, b, sizeof (zbookmark_t)));
}

/*
 * Retrieve the persistent error log, uniquify the members, and return to the
 * caller.
 */
int
zpool_get_errlog(zpool_handle_t *zhp, nvlist_t **nverrlistp)
{
	zfs_cmd_t zc = { 0 };
	uint64_t count;
	zbookmark_t *zb = NULL;
	int i;

	/*
	 * Retrieve the raw error list from the kernel.  If the number of errors
	 * has increased, allocate more space and continue until we get the
	 * entire list.
	 */
	verify(nvlist_lookup_uint64(zhp->zpool_config, ZPOOL_CONFIG_ERRCOUNT,
	    &count) == 0);
	if ((zc.zc_nvlist_dst = (uintptr_t)zfs_alloc(zhp->zpool_hdl,
	    count * sizeof (zbookmark_t))) == (uintptr_t)NULL)
		return (-1);
	zc.zc_nvlist_dst_size = count;
	(void) strcpy(zc.zc_name, zhp->zpool_name);
	for (;;) {
		if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_ERROR_LOG,
		    &zc) != 0) {
			free((void *)(uintptr_t)zc.zc_nvlist_dst);
			if (errno == ENOMEM) {
				count = zc.zc_nvlist_dst_size;
				if ((zc.zc_nvlist_dst = (uintptr_t)
				    zfs_alloc(zhp->zpool_hdl, count *
				    sizeof (zbookmark_t))) == (uintptr_t)NULL)
					return (-1);
			} else {
				return (-1);
			}
		} else {
			break;
		}
	}

	/*
	 * Sort the resulting bookmarks.  This is a little confusing due to the
	 * implementation of ZFS_IOC_ERROR_LOG.  The bookmarks are copied last
	 * to first, and 'zc_nvlist_dst_size' indicates the number of boomarks
	 * _not_ copied as part of the process.  So we point the start of our
	 * array appropriate and decrement the total number of elements.
	 */
	zb = ((zbookmark_t *)(uintptr_t)zc.zc_nvlist_dst) +
	    zc.zc_nvlist_dst_size;
	count -= zc.zc_nvlist_dst_size;

	qsort(zb, count, sizeof (zbookmark_t), zbookmark_compare);

	verify(nvlist_alloc(nverrlistp, 0, KM_SLEEP) == 0);

	/*
	 * Fill in the nverrlistp with nvlist's of dataset and object numbers.
	 */
	for (i = 0; i < count; i++) {
		nvlist_t *nv;

		/* ignoring zb_blkid and zb_level for now */
		if (i > 0 && zb[i-1].zb_objset == zb[i].zb_objset &&
		    zb[i-1].zb_object == zb[i].zb_object)
			continue;

		if (nvlist_alloc(&nv, NV_UNIQUE_NAME, KM_SLEEP) != 0)
			goto nomem;
		if (nvlist_add_uint64(nv, ZPOOL_ERR_DATASET,
		    zb[i].zb_objset) != 0) {
			nvlist_free(nv);
			goto nomem;
		}
		if (nvlist_add_uint64(nv, ZPOOL_ERR_OBJECT,
		    zb[i].zb_object) != 0) {
			nvlist_free(nv);
			goto nomem;
		}
		if (nvlist_add_nvlist(*nverrlistp, "ejk", nv) != 0) {
			nvlist_free(nv);
			goto nomem;
		}
		nvlist_free(nv);
	}

	free((void *)(uintptr_t)zc.zc_nvlist_dst);
	return (0);

nomem:
	free((void *)(uintptr_t)zc.zc_nvlist_dst);
	return (no_memory(zhp->zpool_hdl));
}

/*
 * Upgrade a ZFS pool to the latest on-disk version.
 */
int
zpool_upgrade(zpool_handle_t *zhp)
{
	zfs_cmd_t zc = { 0 };
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) strcpy(zc.zc_name, zhp->zpool_name);
	if (ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_UPGRADE, &zc) != 0)
		return (zpool_standard_error_fmt(hdl, errno,
		    dgettext(TEXT_DOMAIN, "cannot upgrade '%s'"),
		    zhp->zpool_name));

	return (0);
}

/*
 * Log command history.
 *
 * 'pool' is B_TRUE if we are logging a command for 'zpool'; B_FALSE
 * otherwise ('zfs').  'pool_create' is B_TRUE if we are logging the creation
 * of the pool; B_FALSE otherwise.  'path' is the pathanme containing the
 * poolname.  'argc' and 'argv' are used to construct the command string.
 */
void
zpool_log_history(libzfs_handle_t *hdl, int argc, char **argv, const char *path,
	boolean_t pool, boolean_t pool_create)
{
	char cmd_buf[HIS_MAX_RECORD_LEN];
	char *dspath;
	zfs_cmd_t zc = { 0 };
	int i;

	/* construct the command string */
	(void) strcpy(cmd_buf, pool ? "zpool" : "zfs");
	for (i = 0; i < argc; i++) {
		if (strlen(cmd_buf) + 1 + strlen(argv[i]) > HIS_MAX_RECORD_LEN)
			break;
		(void) strcat(cmd_buf, " ");
		(void) strcat(cmd_buf, argv[i]);
	}

	/* figure out the poolname */
	dspath = strpbrk(path, "/@");
	if (dspath == NULL) {
		(void) strcpy(zc.zc_name, path);
	} else {
		(void) strncpy(zc.zc_name, path, dspath - path);
		zc.zc_name[dspath-path] = '\0';
	}

	zc.zc_history = (uint64_t)(uintptr_t)cmd_buf;
	zc.zc_history_len = strlen(cmd_buf);

	/* overloading zc_history_offset */
	zc.zc_history_offset = pool_create;

	(void) ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_LOG_HISTORY, &zc);
}

/*
 * Perform ioctl to get some command history of a pool.
 *
 * 'buf' is the buffer to fill up to 'len' bytes.  'off' is the
 * logical offset of the history buffer to start reading from.
 *
 * Upon return, 'off' is the next logical offset to read from and
 * 'len' is the actual amount of bytes read into 'buf'.
 */
static int
get_history(zpool_handle_t *zhp, char *buf, uint64_t *off, uint64_t *len)
{
	zfs_cmd_t zc = { 0 };
	libzfs_handle_t *hdl = zhp->zpool_hdl;

	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));

	zc.zc_history = (uint64_t)(uintptr_t)buf;
	zc.zc_history_len = *len;
	zc.zc_history_offset = *off;

	if (ioctl(hdl->libzfs_fd, ZFS_IOC_POOL_GET_HISTORY, &zc) != 0) {
		switch (errno) {
		case EPERM:
			return (zfs_error_fmt(hdl, EZFS_PERM,
			    dgettext(TEXT_DOMAIN,
			    "cannot show history for pool '%s'"),
			    zhp->zpool_name));
		case ENOENT:
			return (zfs_error_fmt(hdl, EZFS_NOHISTORY,
			    dgettext(TEXT_DOMAIN, "cannot get history for pool "
			    "'%s'"), zhp->zpool_name));
		case ENOTSUP:
			return (zfs_error_fmt(hdl, EZFS_BADVERSION,
			    dgettext(TEXT_DOMAIN, "cannot get history for pool "
			    "'%s', pool must be upgraded"), zhp->zpool_name));
		default:
			return (zpool_standard_error_fmt(hdl, errno,
			    dgettext(TEXT_DOMAIN,
			    "cannot get history for '%s'"), zhp->zpool_name));
		}
	}

	*len = zc.zc_history_len;
	*off = zc.zc_history_offset;

	return (0);
}

/*
 * Process the buffer of nvlists, unpacking and storing each nvlist record
 * into 'records'.  'leftover' is set to the number of bytes that weren't
 * processed as there wasn't a complete record.
 */
static int
zpool_history_unpack(char *buf, uint64_t bytes_read, uint64_t *leftover,
    nvlist_t ***records, uint_t *numrecords)
{
	uint64_t reclen;
	nvlist_t *nv;
	int i;

	while (bytes_read > sizeof (reclen)) {

		/* get length of packed record (stored as little endian) */
		for (i = 0, reclen = 0; i < sizeof (reclen); i++)
			reclen += (uint64_t)(((uchar_t *)buf)[i]) << (8*i);

		if (bytes_read < sizeof (reclen) + reclen)
			break;

		/* unpack record */
		if (nvlist_unpack(buf + sizeof (reclen), reclen, &nv, 0) != 0)
			return (ENOMEM);
		bytes_read -= sizeof (reclen) + reclen;
		buf += sizeof (reclen) + reclen;

		/* add record to nvlist array */
		(*numrecords)++;
		if (ISP2(*numrecords + 1)) {
			*records = realloc(*records,
			    *numrecords * 2 * sizeof (nvlist_t *));
		}
		(*records)[*numrecords - 1] = nv;
	}

	*leftover = bytes_read;
	return (0);
}

#define	HIS_BUF_LEN	(128*1024)

/*
 * Retrieve the command history of a pool.
 */
int
zpool_get_history(zpool_handle_t *zhp, nvlist_t **nvhisp)
{
	char buf[HIS_BUF_LEN];
	uint64_t off = 0;
	nvlist_t **records = NULL;
	uint_t numrecords = 0;
	int err, i;

	do {
		uint64_t bytes_read = sizeof (buf);
		uint64_t leftover;

		if ((err = get_history(zhp, buf, &off, &bytes_read)) != 0)
			break;

		/* if nothing else was read in, we're at EOF, just return */
		if (!bytes_read)
			break;

		if ((err = zpool_history_unpack(buf, bytes_read,
		    &leftover, &records, &numrecords)) != 0)
			break;
		off -= leftover;

		/* CONSTCOND */
	} while (1);

	if (!err) {
		verify(nvlist_alloc(nvhisp, NV_UNIQUE_NAME, 0) == 0);
		verify(nvlist_add_nvlist_array(*nvhisp, ZPOOL_HIST_RECORD,
		    records, numrecords) == 0);
	}
	for (i = 0; i < numrecords; i++)
		nvlist_free(records[i]);
	free(records);

	return (err);
}

void
zpool_obj_to_path(zpool_handle_t *zhp, uint64_t dsobj, uint64_t obj,
    char *pathname, size_t len)
{
	zfs_cmd_t zc = { 0 };
	boolean_t mounted = B_FALSE;
	char *mntpnt = NULL;
	char dsname[MAXNAMELEN];

	if (dsobj == 0) {
		/* special case for the MOS */
		(void) snprintf(pathname, len, "<metadata>:<0x%llx>", obj);
		return;
	}

	/* get the dataset's name */
	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));
	zc.zc_obj = dsobj;
	if (ioctl(zhp->zpool_hdl->libzfs_fd,
	    ZFS_IOC_DSOBJ_TO_DSNAME, &zc) != 0) {
		/* just write out a path of two object numbers */
		(void) snprintf(pathname, len, "<0x%llx>:<0x%llx>",
		    dsobj, obj);
		return;
	}
	(void) strlcpy(dsname, zc.zc_value, sizeof (dsname));

	/* find out if the dataset is mounted */
	mounted = is_mounted(zhp->zpool_hdl, dsname, &mntpnt);

	/* get the corrupted object's path */
	(void) strlcpy(zc.zc_name, dsname, sizeof (zc.zc_name));
	zc.zc_obj = obj;
	if (ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_OBJ_TO_PATH,
	    &zc) == 0) {
		if (mounted) {
			(void) snprintf(pathname, len, "%s%s", mntpnt,
			    zc.zc_value);
		} else {
			(void) snprintf(pathname, len, "%s:%s",
			    dsname, zc.zc_value);
		}
	} else {
		(void) snprintf(pathname, len, "%s:<0x%llx>", dsname, obj);
	}
	free(mntpnt);
}

int
zpool_set_prop(zpool_handle_t *zhp, const char *propname, const char *propval)
{
	zfs_cmd_t zc = { 0 };
	int ret = -1;
	char errbuf[1024];
	nvlist_t *nvl = NULL;
	nvlist_t *realprops;

	(void) snprintf(errbuf, sizeof (errbuf),
	    dgettext(TEXT_DOMAIN, "cannot set property for '%s'"),
	    zhp->zpool_name);

	if (zpool_get_version(zhp) < ZFS_VERSION_BOOTFS) {
		zfs_error_aux(zhp->zpool_hdl,
		    dgettext(TEXT_DOMAIN, "pool must be "
		    "upgraded to support pool properties"));
		return (zfs_error(zhp->zpool_hdl, EZFS_BADVERSION, errbuf));
	}

	if (zhp->zpool_props == NULL && zpool_get_all_props(zhp))
		return (zfs_error(zhp->zpool_hdl, EZFS_POOLPROPS, errbuf));

	if (nvlist_alloc(&nvl, NV_UNIQUE_NAME, 0) != 0 ||
	    nvlist_add_string(nvl, propname, propval) != 0) {
		return (no_memory(zhp->zpool_hdl));
	}

	if ((realprops = zfs_validate_properties(zhp->zpool_hdl, ZFS_TYPE_POOL,
	    zhp->zpool_name, nvl, 0, NULL, errbuf)) == NULL) {
		nvlist_free(nvl);
		return (-1);
	}

	nvlist_free(nvl);
	nvl = realprops;

	/*
	 * Execute the corresponding ioctl() to set this property.
	 */
	(void) strlcpy(zc.zc_name, zhp->zpool_name, sizeof (zc.zc_name));

	if (zcmd_write_src_nvlist(zhp->zpool_hdl, &zc, nvl, NULL) != 0)
		return (-1);

	ret = ioctl(zhp->zpool_hdl->libzfs_fd, ZFS_IOC_POOL_SET_PROPS, &zc);
	zcmd_free_nvlists(&zc);

	if (ret)
		(void) zpool_standard_error(zhp->zpool_hdl, errno, errbuf);

	return (ret);
}

int
zpool_get_prop(zpool_handle_t *zhp, zpool_prop_t prop, char *propbuf,
    size_t proplen, zfs_source_t *srctype)
{
	uint64_t value;
	char msg[1024], *strvalue;
	nvlist_t *nvp;
	zfs_source_t src = ZFS_SRC_NONE;

	(void) snprintf(msg, sizeof (msg), dgettext(TEXT_DOMAIN,
	    "cannot get property '%s'"), zpool_prop_to_name(prop));

	if (zpool_get_version(zhp) < ZFS_VERSION_BOOTFS) {
		zfs_error_aux(zhp->zpool_hdl,
		    dgettext(TEXT_DOMAIN, "pool must be "
		    "upgraded to support pool properties"));
		return (zfs_error(zhp->zpool_hdl, EZFS_BADVERSION, msg));
	}

	if (zhp->zpool_props == NULL && zpool_get_all_props(zhp))
		return (zfs_error(zhp->zpool_hdl, EZFS_POOLPROPS, msg));

	/*
	 * the "name" property is special cased
	 */
	if (!zfs_prop_valid_for_type(prop, ZFS_TYPE_POOL) &&
	    prop != ZFS_PROP_NAME)
		return (-1);

	switch (prop) {
	case ZFS_PROP_NAME:
		(void) strlcpy(propbuf, zhp->zpool_name, proplen);
		break;

	case ZFS_PROP_BOOTFS:
		if (nvlist_lookup_nvlist(zhp->zpool_props,
		    zpool_prop_to_name(prop), &nvp) != 0) {
			strvalue = (char *)zfs_prop_default_string(prop);
			if (strvalue == NULL)
				strvalue = "-";
			src = ZFS_SRC_DEFAULT;
		} else {
			VERIFY(nvlist_lookup_uint64(nvp,
			    ZFS_PROP_SOURCE, &value) == 0);
			src = value;
			VERIFY(nvlist_lookup_string(nvp, ZFS_PROP_VALUE,
			    &strvalue) == 0);
			if (strlen(strvalue) >= proplen)
				return (-1);
		}
		(void) strcpy(propbuf, strvalue);
		break;

	default:
		return (-1);
	}
	if (srctype)
		*srctype = src;
	return (0);
}

int
zpool_get_proplist(libzfs_handle_t *hdl, char *fields, zpool_proplist_t **listp)
{
	return (zfs_get_proplist_common(hdl, fields, listp, ZFS_TYPE_POOL));
}


int
zpool_expand_proplist(zpool_handle_t *zhp, zpool_proplist_t **plp)
{
	libzfs_handle_t *hdl = zhp->zpool_hdl;
	zpool_proplist_t *entry;
	char buf[ZFS_MAXPROPLEN];

	if (zfs_expand_proplist_common(hdl, plp, ZFS_TYPE_POOL) != 0)
		return (-1);

	for (entry = *plp; entry != NULL; entry = entry->pl_next) {

		if (entry->pl_fixed)
			continue;

		if (entry->pl_prop != ZFS_PROP_INVAL &&
		    zpool_get_prop(zhp, entry->pl_prop, buf, sizeof (buf),
		    NULL) == 0) {
			if (strlen(buf) > entry->pl_width)
				entry->pl_width = strlen(buf);
		}
	}

	return (0);
}
