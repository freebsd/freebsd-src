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
 *	@(#)mtab_aix.c	8.1 (Berkeley) 6/6/93
 *
 * $Id: mtab_aix.c,v 5.2.2.1 1992/02/09 15:10:07 jsp beta $
 *
 */

#include "am.h"

#ifdef READ_MTAB_AIX3_STYLE

#include <sys/mntctl.h>
#include <sys/vmount.h>

static struct mntent *mnt_dup(mp)
struct vmount *mp;
{
	struct mntent *new_mp = ALLOC(mntent);

	char *ty;
	new_mp->mnt_fsname = strdup(vmt2dataptr(mp, VMT_OBJECT));
	new_mp->mnt_dir = strdup(vmt2dataptr(mp, VMT_STUB));
	new_mp->mnt_opts = strdup(vmt2dataptr(mp, VMT_ARGS));
	switch (mp->vmt_gfstype) {
	case MNT_JFS:  ty = MTAB_TYPE_UFS; break;
	case MNT_NFS:
		ty = MTAB_TYPE_NFS;
		new_mp->mnt_fsname = str3cat(new_mp->mnt_fsname,
				vmt2dataptr(mp, VMT_HOSTNAME),
				":", new_mp->mnt_fsname);
		break;
	default:  ty = "unknown"; break;
	}
	new_mp->mnt_type = strdup(ty);
	new_mp->mnt_passno = mp->vmt_vfsnumber;
	new_mp->mnt_freq = 0;

	return new_mp;
}

/*
 * Read a mount table into memory
 */
mntlist *read_mtab(fs)
char *fs;
{
	mntlist **mpp, *mhp;

	int i;
	char *mntinfo = 0, *cp;
	struct vmount *vp;
	int ret;

	/*
	 * First figure out size of mount table
	 * and allocate space for a copy...
	 * Then get mount table for real.
	 */
	ret = mntctl(MCTL_QUERY, sizeof(i), &i);
	if (ret == 0) {
		mntinfo = xmalloc(i);
		ret = mntctl(MCTL_QUERY, i, mntinfo);
	}

	if (ret <= 0) {
		plog(XLOG_ERROR, "mntctl: %m");
		goto out;
	}
#ifdef DEBUG
	/*dlog("mntctl returns %d structures", ret);*/
#endif /* DEBUG */

	mpp = &mhp;
	for (i = 0, cp = mntinfo; i < ret; i++, cp += vp->vmt_length) {
		vp = (struct vmount *) cp;

		/*
		 * Allocate a new slot
		 */
		*mpp = ALLOC(mntlist);

		/*
		 * Copy the data returned by mntctl
		 */
		(*mpp)->mnt = mnt_dup(vp);

		/*
		 * Move to next pointer
		 */
		mpp = &(*mpp)->mnext;
	}

	*mpp = 0;

out:
	if (mntinfo)
		free(mntinfo);
	return mhp;
}

#endif /* READ_MTAB_AIX3_STYLE */
