/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
 * Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
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

#include "be.h"
#include "be_impl.h"

struct be_mountcheck_info {
	const char *path;
	char *name;
};

static int
be_mountcheck_cb(zfs_handle_t *zfs_hdl, void *data)
{
	struct be_mountcheck_info *info;
	char *mountpoint;

	if (data == NULL)
		return (1);
	info = (struct be_mountcheck_info *)data;
	if (!zfs_is_mounted(zfs_hdl, &mountpoint))
		return (0);
	if (strcmp(mountpoint, info->path) == 0) {
		info->name = strdup(zfs_get_name(zfs_hdl));
		free(mountpoint);
		return (1);
	}
	free(mountpoint);
	return (0);
}

/*
 * usage
 */
int
be_mounted_at(libbe_handle_t *lbh, const char *path, nvlist_t *details)
{
	char be[BE_MAXPATHLEN];
	zfs_handle_t *root_hdl;
	struct be_mountcheck_info info;
	prop_data_t propinfo;

	bzero(&be, BE_MAXPATHLEN);
	if ((root_hdl = zfs_open(lbh->lzh, lbh->root,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (BE_ERR_ZFSOPEN);

	info.path = path;
	info.name = NULL;
	zfs_iter_filesystems(root_hdl, be_mountcheck_cb, &info);
	zfs_close(root_hdl);

	if (info.name != NULL) {
		if (details != NULL) {
			if ((root_hdl = zfs_open(lbh->lzh, lbh->root,
			    ZFS_TYPE_FILESYSTEM)) == NULL) {
				free(info.name);
				return (BE_ERR_ZFSOPEN);
			}

			propinfo.lbh = lbh;
			propinfo.list = details;
			propinfo.single_object = false;
			prop_list_builder_cb(root_hdl, &propinfo);
			zfs_close(root_hdl);
		}
		free(info.name);
		return (0);
	}
	return (1);
}

/*
 * usage
 */
int
be_mount(libbe_handle_t *lbh, char *bootenv, char *mountpoint, int flags,
    char *result_loc)
{
	char be[BE_MAXPATHLEN];
	char mnt_temp[BE_MAXPATHLEN];
	int mntflags;
	int err;

	if ((err = be_root_concat(lbh, bootenv, be)) != 0)
		return (set_error(lbh, err));

	if ((err = be_exists(lbh, bootenv)) != 0)
		return (set_error(lbh, err));

	if (is_mounted(lbh->lzh, be, NULL))
		return (set_error(lbh, BE_ERR_MOUNTED));

	mntflags = (flags & BE_MNT_FORCE) ? MNT_FORCE : 0;

	/* Create mountpoint if it is not specified */
	if (mountpoint == NULL) {
		strlcpy(mnt_temp, "/tmp/be_mount.XXXX", sizeof(mnt_temp));
		if (mkdtemp(mnt_temp) == NULL)
			return (set_error(lbh, BE_ERR_IO));
	}

	char opt = '\0';
	if ((err = zmount(be, (mountpoint == NULL) ? mnt_temp : mountpoint,
	    mntflags, __DECONST(char *, MNTTYPE_ZFS), NULL, 0, &opt, 1)) != 0) {
		switch (errno) {
		case ENAMETOOLONG:
			return (set_error(lbh, BE_ERR_PATHLEN));
		case ELOOP:
		case ENOENT:
		case ENOTDIR:
			return (set_error(lbh, BE_ERR_BADPATH));
		case EPERM:
			return (set_error(lbh, BE_ERR_PERMS));
		case EBUSY:
			return (set_error(lbh, BE_ERR_PATHBUSY));
		default:
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	if (result_loc != NULL)
		strlcpy(result_loc, mountpoint == NULL ? mnt_temp : mountpoint,
		    BE_MAXPATHLEN);

	return (BE_ERR_SUCCESS);
}


/*
 * usage
 */
int
be_unmount(libbe_handle_t *lbh, char *bootenv, int flags)
{
	int err, mntflags;
	char be[BE_MAXPATHLEN];
	struct statfs *mntbuf;
	int mntsize;
	char *mntpath;

	if ((err = be_root_concat(lbh, bootenv, be)) != 0)
		return (set_error(lbh, err));

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		if (errno == EIO)
			return (set_error(lbh, BE_ERR_IO));
		return (set_error(lbh, BE_ERR_NOMOUNT));
	}

	mntpath = NULL;
	for (int i = 0; i < mntsize; ++i) {
		/* 0x000000de is the type number of zfs */
		if (mntbuf[i].f_type != 0x000000de)
			continue;

		if (strcmp(mntbuf[i].f_mntfromname, be) == 0) {
			mntpath = mntbuf[i].f_mntonname;
			break;
		}
	}

	if (mntpath == NULL)
		return (set_error(lbh, BE_ERR_NOMOUNT));

	mntflags = (flags & BE_MNT_FORCE) ? MNT_FORCE : 0;

	if ((err = unmount(mntpath, mntflags)) != 0) {
		switch (errno) {
		case ENAMETOOLONG:
			return (set_error(lbh, BE_ERR_PATHLEN));
		case ELOOP:
		case ENOENT:
		case ENOTDIR:
			return (set_error(lbh, BE_ERR_BADPATH));
		case EPERM:
			return (set_error(lbh, BE_ERR_PERMS));
		case EBUSY:
			return (set_error(lbh, BE_ERR_PATHBUSY));
		default:
			return (set_error(lbh, BE_ERR_UNKNOWN));
		}
	}

	return (set_error(lbh, BE_ERR_SUCCESS));
}
