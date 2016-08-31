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
 * File: am-utils/conf/mtab/mtab_aix.c
 *
 */

/*
 * AIX systems don't write their mount tables on a file.  Instead, they
 * use a (better) system where the kernel keeps this state, and you access
 * the mount tables via a known interface.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

/*
 * These were missing external definitions from old AIX's headers.  They
 * appear to be available in <sys/vmount.h> on AIX 5.3, and possibly
 * earlier. Hence I commented this out.
 */
#ifndef HAVE_EXTERN_MNTCTL
extern int mntctl(int cmd, int size, voidp buf);
#endif /* not HAVE_EXTERN_MNTCTL */


static mntent_t *
mnt_dup(struct vmount *mp)
{
  mntent_t *new_mp = ALLOC(mntent_t);
  char *ty;
  char *fsname = xstrdup(vmt2dataptr(mp, VMT_OBJECT));

  new_mp->mnt_dir = strdup(vmt2dataptr(mp, VMT_STUB));
  new_mp->mnt_opts = strdup(vmt2dataptr(mp, VMT_ARGS));

  switch (mp->vmt_gfstype) {

  case MOUNT_TYPE_UFS:
    ty = MNTTAB_TYPE_UFS;
    new_mp->mnt_fsname = xstrdup(fsname);
    break;

  case MOUNT_TYPE_NFS:
    ty = MNTTAB_TYPE_NFS;
    new_mp->mnt_fsname = str3cat((char *) NULL,
				 vmt2dataptr(mp, VMT_HOSTNAME), ":",
				 fsname);
    break;

#ifdef HAVE_FS_NFS3
  case MOUNT_TYPE_NFS3:
    ty = MNTTAB_TYPE_NFS3;
    new_mp->mnt_fsname = str3cat((char *) NULL,
				 vmt2dataptr(mp, VMT_HOSTNAME), ":",
				 fsname);
    break;
#endif /* HAVE_FS_NFS3 */

  default:
    ty = "unknown";
    new_mp->mnt_fsname = xstrdup(fsname);
    break;

  }

  new_mp->mnt_type = xstrdup(ty);
  /* store the VFS ID for uvmount() */
  new_mp->mnt_passno = mp->vmt_vfsnumber;
  new_mp->mnt_freq = 0;

  XFREE(fsname);

  return new_mp;
}


/*
 * Read a mount table into memory
 */
mntlist *
read_mtab(char *fs, const char *mnttabname)
{
  mntlist **mpp, *mhp;
  int i;
  char *mntinfo = NULL, *cp;
  struct vmount *vp;
  int ret;
  int maxtry = 10;		/* maximum number of times to try mntctl */

  /*
   * Figure out size of mount table and allocate space for a copy.  Then get
   * mount table for real.  We repeat this loop at most 10 times to minimze
   * the chance of a race condition (something gets un/mounted in between
   * calls to mntctl()
   */
  i = sizeof(int);
  do {
    if (mntinfo)
      XFREE(mntinfo);
    mntinfo = xmalloc(i);
    ret = mntctl(MCTL_QUERY, i, mntinfo);
    if (ret == 0)
      i = *(int*) mntinfo;
    if (--maxtry <= 0) {
      plog(XLOG_ERROR, "mntctl: could not get a stable result");
      ret = -1;
      errno = EINVAL;
      break;
    }
  } while (ret == 0);
  if (ret < 0) {
    plog(XLOG_ERROR, "mntctl: %m");
    goto out;
  }

  mpp = &mhp;
  for (i = 0, cp = mntinfo; i < ret; i++, cp += vp->vmt_length) {
    vp = (struct vmount *) cp;

    /*
     * Allocate a new slot
     */
    *mpp = ALLOC(struct mntlist);

    /*
     * Copy the data returned by mntctl
     */
    (*mpp)->mnt = mnt_dup(vp);

    /*
     * Move to next pointer
     */
    mpp = &(*mpp)->mnext;
  }

  *mpp = NULL;

out:
  if (mntinfo)
    XFREE(mntinfo);
  return mhp;
}
