/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)mtab_ultrix.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

#include "am.h"

#ifdef READ_MTAB_ULTRIX_STYLE

#include <sys/mount.h>
#include <sys/fs_types.h>

static struct mntent *mnt_dup(mp)
struct fs_data *mp;
{
	struct mntent *new_mp = ALLOC(mntent);

	new_mp->mnt_fsname = strdup(mp->fd_devname);
	new_mp->mnt_dir = strdup(mp->fd_path);
        if (mp->fd_fstype >= GT_NUMTYPES)
                mp->fd_fstype = GT_UNKWN;
        else if (gt_names[mp->fd_fstype] == 0)
                mp->fd_fstype = GT_UNKWN;
        new_mp->mnt_type = strdup(gt_names[mp->fd_fstype]);
	new_mp->mnt_opts = strdup("unset");

	new_mp->mnt_freq = 0;
	new_mp->mnt_passno = mp->fd_dev;

	return new_mp;
}

/*
 * Read a mount table into memory
 */
mntlist *read_mtab(fs)
char *fs;
{
	mntlist **mpp, *mhp;

/* From: Piete Brooks <pb@cl.cam.ac.uk> */

	int loc=0;
#undef	NMOUNT
#define	NMOUNT	20
	struct fs_data mountbuffer[NMOUNT], *fs_data;
	int ret;

	mpp = &mhp;
	while ((ret = getmountent(&loc, mountbuffer, NMOUNT)) > 0) {
	        for (fs_data = mountbuffer; fs_data < &mountbuffer[ret]; fs_data++) {
			/*
			 * Allocate a new slot
			 */
			*mpp = ALLOC(mntlist);

			/*
			 * Copy the data returned by getmntent
			 */
			(*mpp)->mnt = mnt_dup(fs_data);

			/*
			 * Move to next pointer
			 */
			mpp = &(*mpp)->mnext;
		}
	}
	if (ret < 0) {
		plog(XLOG_ERROR, "getmountent: %m");
		return 0;
	}
	*mpp = 0;

	return mhp;
}

#endif /* READ_MTAB_ULTRIX_STYLE */
