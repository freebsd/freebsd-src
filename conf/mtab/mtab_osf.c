/*
 * Copyright (c) 1997-2014 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 * File: am-utils/conf/mtab/mtab_osf.c
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


static mntent_t *
mnt_dup(struct statfs *mp)
{
  mntent_t *new_mp = ALLOC(mntent_t);
  char *ty;
  char *at;
  char mntfrombuf[MNAMELEN];
  char *mntfromptr;

  /*
   * Under DEC OSF/1 T1.2-2 the f_mntfromname fields of
   * the statfs structure can be in the format <fs>@<host>
   * instead of <host>:<fs>.  Here we check for this and
   * reformat it if necessary.
   */
  mntfromptr = mp->f_mntfromname;

  switch (mp->f_type) {
  case MOUNT_TYPE_NFS:
#ifdef HAVE_FS_NFS3
  case MOUNT_TYPE_NFS3:
#endif /* HAVE_FS_NFS3 */
    at = strchr(mp->f_mntfromname, '@');
    if (at != '\0') {
      xstrlcpy(mntfrombuf, (at + 1), sizeof(mntfrombuf));
      xstrlcat(mntfrombuf, ":", sizeof(mntfrombuf));
      strncat(mntfrombuf, mp->f_mntfromname, (at - mp->f_mntfromname));
      mntfromptr = mntfrombuf;
    }
  }
  new_mp->mnt_fsname = xstrdup(mntfromptr);

  new_mp->mnt_dir = xstrdup(mp->f_mntonname);
  switch (mp->f_type) {
  case MOUNT_TYPE_UFS:
    ty = MNTTAB_TYPE_UFS;
    break;
#ifdef HAVE_FS_NFS3
  case MOUNT_TYPE_NFS3:
    ty = MNTTAB_TYPE_NFS3;
    break;
#endif /* HAVE_FS_NFS3 */
  case MOUNT_TYPE_NFS:
    ty = MNTTAB_TYPE_NFS;
    break;
  case MOUNT_TYPE_MFS:
    ty = MNTTAB_TYPE_MFS;
    break;
  default:
    ty = "unknown";
    break;
  }

  new_mp->mnt_type = xstrdup(ty);
  new_mp->mnt_opts = xstrdup("unset");
  new_mp->mnt_freq = 0;
  new_mp->mnt_passno = 0;

  return new_mp;
}


/*
 * Read a mount table into memory
 */
mntlist *
read_mtab(char *fs, const char *mnttabname)
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
    *mpp = ALLOC(struct mntlist);

    /*
     * Copy the data returned by getmntent
     */
    (*mpp)->mnt = mnt_dup(mntp);

    /*
     * Move to next pointer
     */
    mpp = &(*mpp)->mnext;
  }

  /* terminate the linked list */
  *mpp = NULL;

  return mhp;
}
