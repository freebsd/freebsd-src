/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <kenv.h>
#include <libgen.h>
#include <libzfs_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "be.h"
#include "be_impl.h"

#if SOON
static int be_create_child_noent(libbe_handle_t *lbh, const char *active,
    const char *child_path);
static int be_create_child_cloned(libbe_handle_t *lbh, const char *active);
#endif

/*
 * Iterator function for locating the rootfs amongst the children of the
 * zfs_be_root set by loader(8).  data is expected to be a libbe_handle_t *.
 */
static int
be_locate_rootfs(zfs_handle_t *chkds, void *data)
{
	libbe_handle_t *lbh;
	char *mntpoint;

	lbh = (libbe_handle_t *)data;
	if (lbh == NULL)
		return (1);

	mntpoint = NULL;
	if (zfs_is_mounted(chkds, &mntpoint) && strcmp(mntpoint, "/") == 0) {
		strlcpy(lbh->rootfs, zfs_get_name(chkds), sizeof(lbh->rootfs));
		free(mntpoint);
		return (1);
	} else if(mntpoint != NULL)
		free(mntpoint);

	return (0);
}

/*
 * Initializes the libbe context to operate in the root boot environment
 * dataset, for example, zroot/ROOT.
 */
libbe_handle_t *
libbe_init(void)
{
	struct stat sb;
	dev_t root_dev, boot_dev;
	libbe_handle_t *lbh;
	zfs_handle_t *rootds;
	char *poolname, *pos;
	int pnamelen;

	lbh = NULL;
	poolname = pos = NULL;
	rootds = NULL;

	/* Verify that /boot and / are mounted on the same filesystem */
	/* TODO: use errno here?? */
	if (stat("/", &sb) != 0)
		goto err;

	root_dev = sb.st_dev;

	if (stat("/boot", &sb) != 0)
		goto err;

	boot_dev = sb.st_dev;

	if (root_dev != boot_dev) {
		fprintf(stderr, "/ and /boot not on same device, quitting\n");
		goto err;
	}

	if ((lbh = calloc(1, sizeof(libbe_handle_t))) == NULL)
		goto err;

	if ((lbh->lzh = libzfs_init()) == NULL)
		goto err;

	/* Obtain path to boot environment root */
	if ((kenv(KENV_GET, "zfs_be_root", lbh->root,
	    sizeof(lbh->root))) == -1)
		goto err;

	/* Remove leading 'zfs:' if present, otherwise use value as-is */
	if (strcmp(lbh->root, "zfs:") == 0)
		strlcpy(lbh->root, strchr(lbh->root, ':') + sizeof(char),
		    sizeof(lbh->root));

	if ((pos = strchr(lbh->root, '/')) == NULL)
		goto err;

	pnamelen = pos - lbh->root;
	poolname = malloc(pnamelen + 1);
	if (poolname == NULL)
		goto err;

	strlcpy(poolname, lbh->root, pnamelen + 1);
	if ((lbh->active_phandle = zpool_open(lbh->lzh, poolname)) == NULL)
		goto err;
	free(poolname);
	poolname = NULL;

	if (zpool_get_prop(lbh->active_phandle, ZPOOL_PROP_BOOTFS, lbh->bootfs,
	    sizeof(lbh->bootfs), NULL, true) != 0)
		goto err;

	/* Obtain path to boot environment rootfs (currently booted) */
	/* XXX Get dataset mounted at / by kenv/GUID from mountroot? */
	if ((rootds = zfs_open(lbh->lzh, lbh->root, ZFS_TYPE_DATASET)) == NULL)
		goto err;

	zfs_iter_filesystems(rootds, be_locate_rootfs, lbh);
	zfs_close(rootds);
	rootds = NULL;
	if (*lbh->rootfs == '\0')
		goto err;

	return (lbh);
err:
	if (lbh != NULL) {
		if (lbh->active_phandle != NULL)
			zpool_close(lbh->active_phandle);
		if (lbh->lzh != NULL)
			libzfs_fini(lbh->lzh);
		free(lbh);
	}
	free(poolname);
	return (NULL);
}


/*
 * Free memory allocated by libbe_init()
 */
void
libbe_close(libbe_handle_t *lbh)
{

	if (lbh->active_phandle != NULL)
		zpool_close(lbh->active_phandle);
	libzfs_fini(lbh->lzh);
	free(lbh);
}

/*
 * Proxy through to libzfs for the moment.
 */
void
be_nicenum(uint64_t num, char *buf, size_t buflen)
{

	zfs_nicenum(num, buf, buflen);
}

static int
be_destroy_cb(zfs_handle_t *zfs_hdl, void *data)
{
	int err;

	if ((err = zfs_iter_children(zfs_hdl, be_destroy_cb, data)) != 0)
		return (err);
	if ((err = zfs_destroy(zfs_hdl, false)) != 0)
		return (err);
	return (0);
}

/*
 * Destroy the boot environment or snapshot specified by the name
 * parameter. Options are or'd together with the possible values:
 * BE_DESTROY_FORCE : forces operation on mounted datasets
 */
int
be_destroy(libbe_handle_t *lbh, const char *name, int options)
{
	zfs_handle_t *fs;
	char path[BE_MAXPATHLEN];
	char *p;
	int err, force, mounted;

	p = path;
	force = options & BE_DESTROY_FORCE;

	be_root_concat(lbh, name, path);

	if (strchr(name, '@') == NULL) {
		if (!zfs_dataset_exists(lbh->lzh, path, ZFS_TYPE_FILESYSTEM))
			return (set_error(lbh, BE_ERR_NOENT));

		if (strcmp(path, lbh->rootfs) == 0)
			return (set_error(lbh, BE_ERR_DESTROYACT));

		fs = zfs_open(lbh->lzh, p, ZFS_TYPE_FILESYSTEM);
	} else {

		if (!zfs_dataset_exists(lbh->lzh, path, ZFS_TYPE_SNAPSHOT))
			return (set_error(lbh, BE_ERR_NOENT));

		fs = zfs_open(lbh->lzh, p, ZFS_TYPE_SNAPSHOT);
	}

	if (fs == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	/* Check if mounted, unmount if force is specified */
	if ((mounted = zfs_is_mounted(fs, NULL)) != 0) {
		if (force)
			zfs_unmount(fs, NULL, 0);
		else
			return (set_error(lbh, BE_ERR_DESTROYMNT));
	}

	if ((err = be_destroy_cb(fs, NULL)) != 0) {
		/* Children are still present or the mount is referenced */
		if (err == EBUSY)
			return (set_error(lbh, BE_ERR_DESTROYMNT));
		return (set_error(lbh, BE_ERR_UNKNOWN));
	}

	return (0);
}


int
be_snapshot(libbe_handle_t *lbh, const char *source, const char *snap_name,
    bool recursive, char *result)
{
	char buf[BE_MAXPATHLEN];
	time_t rawtime;
	int len, err;

	be_root_concat(lbh, source, buf);

	if (!be_exists(lbh, buf))
		return (BE_ERR_NOENT);

	if (snap_name != NULL) {
		if (strlcat(buf, "@", sizeof(buf)) >= sizeof(buf))
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		if (strlcat(buf, snap_name, sizeof(buf)) >= sizeof(buf))
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		if (result != NULL)
			snprintf(result, BE_MAXPATHLEN, "%s@%s", source,
			    snap_name);
	} else {
		time(&rawtime);
		len = strlen(buf);
		strftime(buf + len, sizeof(buf) - len,
		    "@%F-%T", localtime(&rawtime));
		if (result != NULL && strlcpy(result, strrchr(buf, '/') + 1,
		    sizeof(buf)) >= sizeof(buf))
			return (set_error(lbh, BE_ERR_INVALIDNAME));
	}

	if ((err = zfs_snapshot(lbh->lzh, buf, recursive, NULL)) != 0) {
		switch (err) {
		case EZFS_INVALIDNAME:
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		default:
			/*
			 * The other errors that zfs_ioc_snapshot might return
			 * shouldn't happen if we've set things up properly, so
			 * we'll gloss over them and call it UNKNOWN as it will
			 * require further triage.
			 */
			if (errno == ENOTSUP)
				return (set_error(lbh, BE_ERR_NOPOOL));
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	return (BE_ERR_SUCCESS);
}


/*
 * Create the boot environment specified by the name parameter
 */
int
be_create(libbe_handle_t *lbh, const char *name)
{
	int err;

	err = be_create_from_existing(lbh, name, be_active_path(lbh));

	return (set_error(lbh, err));
}


static int
be_deep_clone_prop(int prop, void *cb)
{
	int err;
        struct libbe_dccb *dccb;
	zprop_source_t src;
	char pval[BE_MAXPATHLEN];
	char source[BE_MAXPATHLEN];

	dccb = cb;
	/* Skip some properties we don't want to touch */
	if (prop == ZFS_PROP_CANMOUNT)
		return (ZPROP_CONT);

	/* Don't copy readonly properties */
	if (zfs_prop_readonly(prop))
		return (ZPROP_CONT);

	if ((err = zfs_prop_get(dccb->zhp, prop, (char *)&pval,
	    sizeof(pval), &src, (char *)&source, sizeof(source), false)))
		/* Just continue if we fail to read a property */
		return (ZPROP_CONT);

	/* Only copy locally defined properties */
	if (src != ZPROP_SRC_LOCAL)
		return (ZPROP_CONT);

	nvlist_add_string(dccb->props, zfs_prop_to_name(prop), (char *)pval);

	return (ZPROP_CONT);
}

static int
be_deep_clone(zfs_handle_t *ds, void *data)
{
	int err;
	char be_path[BE_MAXPATHLEN];
	char snap_path[BE_MAXPATHLEN];
	const char *dspath;
	char *dsname;
	zfs_handle_t *snap_hdl;
	nvlist_t *props;
	struct libbe_deep_clone *isdc, sdc;
	struct libbe_dccb dccb;

	isdc = (struct libbe_deep_clone *)data;
	dspath = zfs_get_name(ds);
	if ((dsname = strrchr(dspath, '/')) == NULL)
		return (BE_ERR_UNKNOWN);
	dsname++;

	if (isdc->bename == NULL)
		snprintf(be_path, sizeof(be_path), "%s/%s", isdc->be_root, dsname);
	else
		snprintf(be_path, sizeof(be_path), "%s/%s", isdc->be_root, isdc->bename);

	snprintf(snap_path, sizeof(snap_path), "%s@%s", dspath, isdc->snapname);

	if (zfs_dataset_exists(isdc->lbh->lzh, be_path, ZFS_TYPE_DATASET))
		return (set_error(isdc->lbh, BE_ERR_EXISTS));

	if ((snap_hdl =
	    zfs_open(isdc->lbh->lzh, snap_path, ZFS_TYPE_SNAPSHOT)) == NULL)
		return (set_error(isdc->lbh, BE_ERR_ZFSOPEN));

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");

	dccb.zhp = ds;
	dccb.props = props;
	if (zprop_iter(be_deep_clone_prop, &dccb, B_FALSE, B_FALSE,
	    ZFS_TYPE_FILESYSTEM) == ZPROP_INVAL)
		return (-1);

	if ((err = zfs_clone(snap_hdl, be_path, props)) != 0)
		err = BE_ERR_ZFSCLONE;

	nvlist_free(props);
	zfs_close(snap_hdl);

	/* Failed to clone */
	if (err != BE_ERR_SUCCESS)
		return (set_error(isdc->lbh, err));

	sdc.lbh = isdc->lbh;
	sdc.bename = NULL;
	sdc.snapname = isdc->snapname;
	sdc.be_root = (char *)&be_path;

	err = zfs_iter_filesystems(ds, be_deep_clone, &sdc);

	return (err);
}

/*
 * Create the boot environment from pre-existing snapshot
 */
int
be_create_from_existing_snap(libbe_handle_t *lbh, const char *name,
    const char *snap)
{
	int err;
	char be_path[BE_MAXPATHLEN];
	char snap_path[BE_MAXPATHLEN];
	const char *bename;
	char *parentname, *snapname;
	zfs_handle_t *parent_hdl;
	struct libbe_deep_clone sdc;

	if ((err = be_validate_name(lbh, name)) != 0)
		return (set_error(lbh, err));
	if ((err = be_root_concat(lbh, snap, snap_path)) != 0)
		return (set_error(lbh, err));
	if ((err = be_validate_snap(lbh, snap_path)) != 0)
		return (set_error(lbh, err));

	if ((err = be_root_concat(lbh, name, be_path)) != 0)
		return (set_error(lbh, err));

	if ((bename = strrchr(name, '/')) == NULL)
		bename = name;
	else
		bename++;

	if ((parentname = strdup(snap_path)) == NULL)
		return (set_error(lbh, BE_ERR_UNKNOWN));

	snapname = strchr(parentname, '@');
	if (snapname == NULL) {
		free(parentname);
		return (set_error(lbh, BE_ERR_UNKNOWN));
	}
	*snapname = '\0';
	snapname++;

	sdc.lbh = lbh;
	sdc.bename = bename;
	sdc.snapname = snapname;
	sdc.be_root = lbh->root;

	parent_hdl = zfs_open(lbh->lzh, parentname, ZFS_TYPE_DATASET);
	err = be_deep_clone(parent_hdl, &sdc);

	free(parentname);
	return (set_error(lbh, err));
}


/*
 * Create a boot environment from an existing boot environment
 */
int
be_create_from_existing(libbe_handle_t *lbh, const char *name, const char *old)
{
	int err;
	char buf[BE_MAXPATHLEN];

	if ((err = be_snapshot(lbh, old, NULL, true, (char *)&buf)))
		return (set_error(lbh, err));

	err = be_create_from_existing_snap(lbh, name, (char *)buf);

	return (set_error(lbh, err));
}


/*
 * Verifies that a snapshot has a valid name, exists, and has a mountpoint of
 * '/'. Returns BE_ERR_SUCCESS (0), upon success, or the relevant BE_ERR_* upon
 * failure. Does not set the internal library error state.
 */
int
be_validate_snap(libbe_handle_t *lbh, const char *snap_name)
{
	zfs_handle_t *zfs_hdl;
	char buf[BE_MAXPATHLEN];
	char *delim_pos;
	int err = BE_ERR_SUCCESS;

	if (strlen(snap_name) >= BE_MAXPATHLEN)
		return (BE_ERR_PATHLEN);

	if (!zfs_dataset_exists(lbh->lzh, snap_name,
	    ZFS_TYPE_SNAPSHOT))
		return (BE_ERR_NOENT);

	strlcpy(buf, snap_name, sizeof(buf));

	/* Find the base filesystem of the snapshot */
	if ((delim_pos = strchr(buf, '@')) == NULL)
		return (BE_ERR_INVALIDNAME);
	*delim_pos = '\0';

	if ((zfs_hdl =
	    zfs_open(lbh->lzh, buf, ZFS_TYPE_DATASET)) == NULL)
		return (BE_ERR_NOORIGIN);

	if ((err = zfs_prop_get(zfs_hdl, ZFS_PROP_MOUNTPOINT, buf,
	    sizeof(buf), NULL, NULL, 0, 1)) != 0)
		err = BE_ERR_INVORIGIN;

	if ((err != 0) && (strncmp(buf, "/", sizeof(buf)) != 0))
		err = BE_ERR_INVORIGIN;

	zfs_close(zfs_hdl);

	return (err);
}


/*
 * Idempotently appends the name argument to the root boot environment path
 * and copies the resulting string into the result buffer (which is assumed
 * to be at least BE_MAXPATHLEN characters long. Returns BE_ERR_SUCCESS upon
 * success, BE_ERR_PATHLEN if the resulting path is longer than BE_MAXPATHLEN,
 * or BE_ERR_INVALIDNAME if the name is a path that does not begin with
 * zfs_be_root. Does not set internal library error state.
 */
int
be_root_concat(libbe_handle_t *lbh, const char *name, char *result)
{
	size_t name_len, root_len;

	name_len = strlen(name);
	root_len = strlen(lbh->root);

	/* Act idempotently; return be name if it is already a full path */
	if (strrchr(name, '/') != NULL) {
		if (strstr(name, lbh->root) != name)
			return (BE_ERR_INVALIDNAME);

		if (name_len >= BE_MAXPATHLEN)
			return (BE_ERR_PATHLEN);

		strlcpy(result, name, BE_MAXPATHLEN);
		return (BE_ERR_SUCCESS);
	} else if (name_len + root_len + 1 < BE_MAXPATHLEN) {
		snprintf(result, BE_MAXPATHLEN, "%s/%s", lbh->root,
		    name);
		return (BE_ERR_SUCCESS);
	}

	return (BE_ERR_PATHLEN);
}


/*
 * Verifies the validity of a boot environment name (A-Za-z0-9-_.). Returns
 * BE_ERR_SUCCESS (0) if name is valid, otherwise returns BE_ERR_INVALIDNAME.
 * Does not set internal library error state.
 */
int
be_validate_name(libbe_handle_t *lbh __unused, const char *name)
{
	for (int i = 0; *name; i++) {
		char c = *(name++);
		if (isalnum(c) || (c == '-') || (c == '_') || (c == '.'))
			continue;
		return (BE_ERR_INVALIDNAME);
	}

	return (BE_ERR_SUCCESS);
}


/*
 * usage
 */
int
be_rename(libbe_handle_t *lbh, const char *old, const char *new)
{
	char full_old[BE_MAXPATHLEN];
	char full_new[BE_MAXPATHLEN];
	zfs_handle_t *zfs_hdl;
	int err;

	if ((err = be_root_concat(lbh, old, full_old)) != 0)
		return (set_error(lbh, err));
	if ((err = be_root_concat(lbh, new, full_new)) != 0)
		return (set_error(lbh, err));

	if ((err = be_validate_name(lbh, new)) != 0)
		return (err);

	/* Check if old is active BE */
	if (strcmp(full_old, be_active_path(lbh)) == 0)
		return (set_error(lbh, BE_ERR_MOUNTED));

	if (!zfs_dataset_exists(lbh->lzh, full_old, ZFS_TYPE_DATASET))
		return (set_error(lbh, BE_ERR_NOENT));

	if (zfs_dataset_exists(lbh->lzh, full_new, ZFS_TYPE_DATASET))
		return (set_error(lbh, BE_ERR_EXISTS));

	if ((zfs_hdl = zfs_open(lbh->lzh, full_old,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	/* XXX TODO: Allow a force flag */
	if (zfs_is_mounted(zfs_hdl, NULL)) {
		zfs_close(zfs_hdl);
		return (set_error(lbh, BE_ERR_MOUNTED));
	}

	/* recurse, nounmount, forceunmount */
	struct renameflags flags = { 0, 0, 0 };

	err = zfs_rename(zfs_hdl, NULL, full_new, flags);

	zfs_close(zfs_hdl);

	return (set_error(lbh, err));
}


int
be_export(libbe_handle_t *lbh, const char *bootenv, int fd)
{
	char snap_name[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN];
	zfs_handle_t *zfs;
	int err;

	if ((err = be_snapshot(lbh, bootenv, NULL, true, snap_name)) != 0)
		/* Use the error set by be_snapshot */
		return (err);

	be_root_concat(lbh, snap_name, buf);

	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_DATASET)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	err = zfs_send_one(zfs, NULL, fd, 0);
	zfs_close(zfs);

	return (err);
}


int
be_import(libbe_handle_t *lbh, const char *bootenv, int fd)
{
	char buf[BE_MAXPATHLEN];
	time_t rawtime;
	nvlist_t *props;
	zfs_handle_t *zfs;
	int err, len;
	char nbuf[24];

	/*
	 * We don't need this to be incredibly random, just unique enough that
	 * it won't conflict with an existing dataset name.  Chopping time
	 * down to 32 bits is probably good enough for this.
	 */
	snprintf(nbuf, 24, "tmp%u",
	    (uint32_t)(time(NULL) & 0xFFFFFFFF));
	if ((err = be_root_concat(lbh, nbuf, buf)) != 0)
		/*
		 * Technically this is our problem, but we try to use short
		 * enough names that we won't run into problems except in
		 * worst-case BE root approaching MAXPATHLEN.
		 */
		return (set_error(lbh, BE_ERR_PATHLEN));

	time(&rawtime);
	len = strlen(buf);
	strftime(buf + len, sizeof(buf) - len, "@%F-%T", localtime(&rawtime));

	if ((err = lzc_receive(buf, NULL, NULL, false, fd)) != 0) {
		switch (err) {
		case EINVAL:
			return (set_error(lbh, BE_ERR_NOORIGIN));
		case ENOENT:
			return (set_error(lbh, BE_ERR_NOENT));
		case EIO:
			return (set_error(lbh, BE_ERR_IO));
		default:
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_SNAPSHOT)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");
	nvlist_add_string(props, "mountpoint", "/");

	be_root_concat(lbh, bootenv, buf);

	err = zfs_clone(zfs, buf, props);
	zfs_close(zfs);
	nvlist_free(props);

	if (err != 0)
		return (set_error(lbh, BE_ERR_UNKNOWN));

	/*
	 * Finally, we open up the dataset we just cloned the snapshot so that
	 * we may promote it.  This is necessary in order to clean up the ghost
	 * snapshot that doesn't need to be seen after the operation is
	 * complete.
	 */
	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_DATASET)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	err = zfs_promote(zfs);
	zfs_close(zfs);

	if (err != 0)
		return (set_error(lbh, BE_ERR_UNKNOWN));

	/* Clean up the temporary snapshot */
	return (be_destroy(lbh, nbuf, 0));
}

#if SOON
static int
be_create_child_noent(libbe_handle_t *lbh, const char *active,
    const char *child_path)
{
	nvlist_t *props;
	zfs_handle_t *zfs;
	int err;

	nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_add_string(props, "canmount", "noauto");
	nvlist_add_string(props, "mountpoint", child_path);

	/* Create */
	if ((err = zfs_create(lbh->lzh, active, ZFS_TYPE_DATASET,
	    props)) != 0) {
		switch (err) {
		case EZFS_EXISTS:
			return (set_error(lbh, BE_ERR_EXISTS));
		case EZFS_NOENT:
			return (set_error(lbh, BE_ERR_NOENT));
		case EZFS_BADTYPE:
		case EZFS_BADVERSION:
			return (set_error(lbh, BE_ERR_NOPOOL));
		case EZFS_BADPROP:
		default:
			/* We set something up wrong, probably... */
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}
	nvlist_free(props);

	if ((zfs = zfs_open(lbh->lzh, active, ZFS_TYPE_DATASET)) == NULL)
		return (set_error(lbh, BE_ERR_ZFSOPEN));

	/* Set props */
	if ((err = zfs_prop_set(zfs, "canmount", "noauto")) != 0) {
		zfs_close(zfs);
		/*
		 * Similar to other cases, this shouldn't fail unless we've
		 * done something wrong.  This is a new dataset that shouldn't
		 * have been mounted anywhere between creation and now.
		 */
		if (err == EZFS_NOMEM)
			return (set_error(lbh, BE_ERR_NOMEM));
		return (set_error(lbh, BE_ERR_UNKNOWN));
	}
	zfs_close(zfs);
	return (BE_ERR_SUCCESS);
}

static int
be_create_child_cloned(libbe_handle_t *lbh, const char *active)
{
	char buf[BE_MAXPATHLEN], tmp[BE_MAXPATHLEN];;
	zfs_handle_t *zfs;
	int err;

	/* XXX TODO ? */

	/*
	 * Establish if the existing path is a zfs dataset or just
	 * the subdirectory of one
	 */
	strlcpy(tmp, "tmp/be_snap.XXXXX", sizeof(tmp));
	if (mktemp(tmp) == NULL)
		return (set_error(lbh, BE_ERR_UNKNOWN));

	be_root_concat(lbh, tmp, buf);
	printf("Here %s?\n", buf);
	if ((err = zfs_snapshot(lbh->lzh, buf, false, NULL)) != 0) {
		switch (err) {
		case EZFS_INVALIDNAME:
			return (set_error(lbh, BE_ERR_INVALIDNAME));

		default:
			/*
			 * The other errors that zfs_ioc_snapshot might return
			 * shouldn't happen if we've set things up properly, so
			 * we'll gloss over them and call it UNKNOWN as it will
			 * require further triage.
			 */
			if (errno == ENOTSUP)
				return (set_error(lbh, BE_ERR_NOPOOL));
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	/* Clone */
	if ((zfs = zfs_open(lbh->lzh, buf, ZFS_TYPE_SNAPSHOT)) == NULL)
		return (BE_ERR_ZFSOPEN);

	if ((err = zfs_clone(zfs, active, NULL)) != 0)
		/* XXX TODO correct error */
		return (set_error(lbh, BE_ERR_UNKNOWN));

	/* set props */
	zfs_close(zfs);
	return (BE_ERR_SUCCESS);
}

int
be_add_child(libbe_handle_t *lbh, const char *child_path, bool cp_if_exists)
{
	struct stat sb;
	char active[BE_MAXPATHLEN], buf[BE_MAXPATHLEN];
	nvlist_t *props;
	const char *s;

	/* Require absolute paths */
	if (*child_path != '/')
		return (set_error(lbh, BE_ERR_BADPATH));

	strlcpy(active, be_active_path(lbh), BE_MAXPATHLEN);
	strcpy(buf, active);

	/* Create non-mountable parent dataset(s) */
	s = child_path;
	for (char *p; (p = strchr(s+1, '/')) != NULL; s = p) {
		size_t len = p - s;
		strncat(buf, s, len);

		nvlist_alloc(&props, NV_UNIQUE_NAME, KM_SLEEP);
		nvlist_add_string(props, "canmount", "off");
		nvlist_add_string(props, "mountpoint", "none");
		zfs_create(lbh->lzh, buf, ZFS_TYPE_DATASET, props);
		nvlist_free(props);
	}

	/* Path does not exist as a descendent of / yet */
	if (strlcat(active, child_path, BE_MAXPATHLEN) >= BE_MAXPATHLEN)
		return (set_error(lbh, BE_ERR_PATHLEN));

	if (stat(child_path, &sb) != 0) {
		/* Verify that error is ENOENT */
		if (errno != ENOENT)
			return (set_error(lbh, BE_ERR_UNKNOWN));
		return (be_create_child_noent(lbh, active, child_path));
	} else if (cp_if_exists)
		/* Path is already a descendent of / and should be copied */
		return (be_create_child_cloned(lbh, active));
	return (set_error(lbh, BE_ERR_EXISTS));
}
#endif	/* SOON */

static int
be_set_nextboot(libbe_handle_t *lbh, nvlist_t *config, uint64_t pool_guid,
    const char *zfsdev)
{
	nvlist_t **child;
	uint64_t vdev_guid;
	int c, children;

	if (nvlist_lookup_nvlist_array(config, ZPOOL_CONFIG_CHILDREN, &child,
	    &children) == 0) {
		for (c = 0; c < children; ++c)
			if (be_set_nextboot(lbh, child[c], pool_guid, zfsdev) != 0)
				return (1);
		return (0);
	}

	if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_GUID,
	    &vdev_guid) != 0) {
		return (1);
	}

	if (zpool_nextboot(lbh->lzh, pool_guid, vdev_guid, zfsdev) != 0) {
		perror("ZFS_IOC_NEXTBOOT failed");
		return (1);
	}

	return (0);
}


int
be_activate(libbe_handle_t *lbh, const char *bootenv, bool temporary)
{
	char be_path[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN];
	uint64_t pool_guid;
	nvlist_t *config, *vdevs;
	int err;

	be_root_concat(lbh, bootenv, be_path);

	/* Note: be_exists fails if mountpoint is not / */
	if (!be_exists(lbh, be_path))
		return (BE_ERR_NOENT);

	if (temporary) {
		config = zpool_get_config(lbh->active_phandle, NULL);
		if (config == NULL)
			/* config should be fetchable... */
			return (set_error(lbh, BE_ERR_UNKNOWN));

		if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
		    &pool_guid) != 0)
			/* Similarly, it shouldn't be possible */
			return (set_error(lbh, BE_ERR_UNKNOWN));

		/* Expected format according to zfsbootcfg(8) man */
		snprintf(buf, sizeof(buf), "zfs:%s:", be_path);

		/* We have no config tree */
		if (nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &vdevs) != 0)
			return (set_error(lbh, BE_ERR_NOPOOL));

		return (be_set_nextboot(lbh, vdevs, pool_guid, buf));
	} else {
		/* Obtain bootenv zpool */
		err = zpool_set_prop(lbh->active_phandle, "bootfs", be_path);

		switch (err) {
		case 0:
			return (BE_ERR_SUCCESS);

		default:
			/* XXX TODO correct errors */
			return (-1);
		}
	}
}
