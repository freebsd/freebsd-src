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
 *	@(#)mtab_bsd.c	8.1 (Berkeley) 6/6/93
 *
 * $Id: mtab_bsd.c,v 5.2.2.1 1992/02/09 15:10:13 jsp beta $
 *
 */

#include "am.h"

#ifdef READ_MTAB_BSD_STYLE

#include <sys/mount.h>

static struct mntent *mnt_dup(mp)
struct statfs *mp;
{
	struct mntent *new_mp = ALLOC(mntent);
	char *ty;

	new_mp->mnt_fsname = strdup(mp->f_mntfromname);
	new_mp->mnt_dir = strdup(mp->f_mntonname);
	switch (mp->f_type) {
	case MOUNT_UFS:  ty = MTAB_TYPE_UFS; break;
	case MOUNT_NFS:  ty = MTAB_TYPE_NFS; break;
	case MOUNT_MFS:  ty = MTAB_TYPE_MFS; break;
	default:  ty = "unknown"; break;
	}
	new_mp->mnt_type = strdup(ty);
	new_mp->mnt_opts = strdup("unset");
	new_mp->mnt_freq = 0;
	new_mp->mnt_passno = 0;

	return new_mp;
}

/*
 * Read a mount table into memory
 */
mntlist *read_mtab(fs)
char *fs;
{
	mntlist **mpp, *mhp;
	struct statfs *mntbufp, *mntp;

	int nloc = getmntinfo(&mntbufp, MNT_NOWAIT);

	if (nloc == 0) {
		plog(XLOG_ERROR, "Can't read mount table");
		return 0;
	}

	mpp = &mhp;
	for (mntp = mntbufp; mntp < mntbufp + nloc; mntp++) {
		/*
		 * Allocate a new slot
		 */
		*mpp = ALLOC(mntlist);

		/*
		 * Copy the data returned by getmntent
		 */
		(*mpp)->mnt = mnt_dup(mntp);

		/*
		 * Move to next pointer
		 */
		mpp = &(*mpp)->mnext;
	}

	/*
	 * Terminate the list
	 */
	*mpp = 0;

	return mhp;
}

#endif /* READ_MTAB_BSD_STYLE */
