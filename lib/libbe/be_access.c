/*
 * be_access.c
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

#include "be.h"
#include "be_impl.h"

/*
 * usage
 */
int
be_mount(libbe_handle_t *lbh, char *bootenv, char *mountpoint, int flags,
    char *result_loc)
{
	char be[BE_MAXPATHLEN];
	char mnt_temp[BE_MAXPATHLEN];
	zfs_handle_t *zfs_hdl;
	char *path;
	int mntflags;
	int err;

	if (err = be_root_concat(lbh, bootenv, be))
		return (set_error(lbh, err));

	if (!be_exists(lbh, bootenv))
		return (set_error(lbh, BE_ERR_NOENT));

	if (is_mounted(lbh->lzh, be, &path))
		return (set_error(lbh, BE_ERR_MOUNTED));

	mntflags = (flags & BE_MNT_FORCE) ? MNT_FORCE : 0;

	/* Create mountpoint if it is not specified */
	if (mountpoint == NULL) {
		strcpy(mnt_temp, "/tmp/be_mount.XXXX");
		if (mkdtemp(mnt_temp) == NULL)
			/* XXX TODO: create error for this */
			return (set_error(lbh, BE_ERR_UNKNOWN));
	}

	char opt = '\0';
	if (err = zmount(be, (mountpoint == NULL) ? mnt_temp : mountpoint,
	    mntflags, MNTTYPE_ZFS, NULL, 0, &opt, 1))
		/*
		 * XXX TODO: zmount returns the nmount error, look into what
		 * kind of errors we can report from that
		 */
		return (set_error(lbh, BE_ERR_UNKNOWN));

	if (result_loc != NULL)
		strcpy(result_loc, mountpoint == NULL ? mnt_temp : mountpoint);

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

	if (err = be_root_concat(lbh, bootenv, be))
		return (set_error(lbh, err));

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
		/* XXX TODO correct error */
		return (set_error(lbh, BE_ERR_NOMOUNT));

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

	if (err = unmount(mntpath, mntflags))
		/* XXX TODO correct error */
		return (set_error(lbh, BE_ERR_NOMOUNT));

	return (set_error(lbh, BE_ERR_SUCCESS));
}
