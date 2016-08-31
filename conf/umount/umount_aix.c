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
 * File: am-utils/conf/umount/umount_aix.c
 *
 */

/*
 * AIX method of unmounting filesystems.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


int
umount_fs(char *mntdir, const char *mnttabname, u_int unmount_flags)
{
  mntlist *mlist, *mp, *mp_save = NULL;
  int error = 0;

  mp = mlist = read_mtab(mntdir, mnttabname);

  /*
   * Search the mount table looking for
   * the correct (ie last) matching entry
   */
  while (mp) {
    if (STREQ(mp->mnt->mnt_dir, mntdir))
      mp_save = mp;
    mp = mp->mnext;
  }

  if (mp_save) {
    dlog("Trying unmount(%s)", mp_save->mnt->mnt_dir);

#ifdef MOUNT_TABLE_ON_FILE
    /*
     * This unmount may hang leaving this process with an exclusive lock on
     * /etc/mtab. Therefore it is necessary to unlock mtab, do the unmount,
     * then lock mtab (again) and reread it and finally update it.
     */
    unlock_mntlist();
#endif /* MOUNT_TABLE_ON_FILE */

#ifdef NEED_AUTOFS_SPACE_HACK
    if (unmount_flags & AMU_UMOUNT_AUTOFS) {
      char *mnt_dir_save = mp_save->mnt->mnt_dir;
      mp_save->mnt->mnt_dir = autofs_strdup_space_hack(mnt_dir_save);
      error = UNMOUNT_TRAP(mp_save->mnt);
      XFREE(mp_save->mnt->mnt_dir);
      mp_save->mnt->mnt_dir = mnt_dir_save;
    } else
#endif /* NEED_AUTOFS_SPACE_HACK */
      error = UNMOUNT_TRAP(mp_save->mnt);
    if (error < 0) {
      switch ((error = errno)) {
      case EINVAL:
      case ENOTBLK:
	plog(XLOG_WARNING, "unmount: %s is not mounted", mp_save->mnt->mnt_dir);
	error = 0;		/* Not really an error */
	break;

      case ENOENT:
	/*
	 * This could happen if the kernel insists on following symlinks
	 * when we try to unmount a direct mountpoint. We need to propagate
	 * the error up so that the top layers know it failed and don't
	 * try to rmdir() the mountpoint or other silly things.
	 */
	plog(XLOG_ERROR, "mount point %s: %m", mp_save->mnt->mnt_dir);
	break;

#if defined(MNT2_GEN_OPT_FORCE) && defined(HAVE_UVMOUNT)
      case EBUSY:
      case EIO:
      case ESTALE:
	/* caller determines if forced unmounts should be used */
	if (unmount_flags & AMU_UMOUNT_FORCE) {
	  error = umount2_fs(mntdir, unmount_flags);
	  if (error < 0)
	    error = errno;
	  else
	    break;		/* all is OK */
	}
	/* fallthrough */
#endif /* MNT2_GEN_OPT_FORCE && HAVE_UVMOUNT */
      default:
	dlog("%s: unmount: %m", mp_save->mnt->mnt_dir);
	break;
      }
    }
    dlog("Finished unmount(%s)", mp_save->mnt->mnt_dir);

    if (!error) {
#ifdef MOUNT_TABLE_ON_FILE
      free_mntlist(mlist);
      mp = mlist = read_mtab(mntdir, mnttabname);

      /*
       * Search the mount table looking for
       * the correct (ie last) matching entry
       */
      mp_save = NULL;
      while (mp) {
	if (STREQ(mp->mnt->mnt_dir, mntdir))
	  mp_save = mp;
	mp = mp->mnext;
      }

      if (mp_save) {
	mnt_free(mp_save->mnt);
	mp_save->mnt = NULL;
	rewrite_mtab(mlist, mnttabname);
      }
#endif /* MOUNT_TABLE_ON_FILE */
    }

  } else {

    plog(XLOG_ERROR, "Couldn't find how to unmount %s", mntdir);
    /*
     * Assume it is already unmounted
     */
    error = 0;
  } /* end of "if (mp_save)" statement */

  free_mntlist(mlist);

  return error;
}


#if defined(MNT2_GEN_OPT_FORCE) && defined(HAVE_UVMOUNT)
/* force unmount, no questions asked, without touching mnttab file */
int
umount2_fs(const char *mntdir, u_int unmount_flags)
{
  int error = 0;
#if 0
    u_int vfs_id = 0;
#endif /* 0 */

  if (unmount_flags & AMU_UMOUNT_FORCE) {
    plog(XLOG_INFO, "**UNIMPLEMENTED**: umount2_fs: trying unmount/forced on %s", mntdir);
#if 0
    /*
     * XXX: need to implement.  Call read_mtab and search mntlist for VFS ID
     * of mntdir, then pass that ID to uvmount(), then call free_mntlist().
     */
    error = uvmount(vfs_id, MNT2_GEN_OPT_FORCE); /* AIX */
    if (error < 0 && (errno == EINVAL || errno == ENOENT))
      error = 0;		/* ignore EINVAL/ENOENT */
    if (error < 0)
      plog(XLOG_WARNING, "%u: unmount/force: %m", vfs_id);
    else
      dlog("%s: unmount/force: OK", mntdir);
#endif /* 0 */
  }
  return error;
}
#endif /* MNT2_GEN_OPT_FORCE && HAVE_UVMOUNT */
