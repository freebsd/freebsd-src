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
 * File: am-utils/conf/umount/umount_linux.c
 *
 */

/*
 * Linux method of unmounting filesystems: if umount(2) failed with EIO or
 * ESTALE, try umount2(2) with MNT_FORCE+MNT_DETACH.
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
#ifdef HAVE_LOOP_DEVICE
  char *opt, *xopts = NULL;
  char loopstr[] = "loop=";
  char *loopdev;
#endif /* HAVE_LOOP_DEVICE */
  unsigned int retries = 8;

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

  if (!mp_save) {
    plog(XLOG_ERROR, "Couldn't find how to unmount %s", mntdir);
    /* Assume it is already unmounted */
    error = 0;
    goto out;
  }

  plog(XLOG_ERROR, "Trying unmount %s, umount_flags 0x%x", mp_save->mnt->mnt_dir, unmount_flags);
  dlog("Trying unmount(%s)", mp_save->mnt->mnt_dir);

#ifdef MOUNT_TABLE_ON_FILE
  /*
   * This unmount may hang leaving this process with an exclusive lock on
   * /etc/mtab. Therefore it is necessary to unlock mtab, do the unmount,
   * then lock mtab (again) and reread it and finally update it.
   */
  unlock_mntlist();
#endif /* MOUNT_TABLE_ON_FILE */

again:
#if defined(HAVE_UMOUNT2) && defined(MNT2_GEN_OPT_DETACH)
  /*
   * If user asked to try forced unmounts, then do a quick check to see if
   * the mount point is hung badly.  If so, then try to detach it by
   * force; if the latter works, we're done.
   */
  if (unmount_flags & AMU_UMOUNT_DETACH) {
    /*
     * Note: we pass both DETACH and FORCE flags, because umount2_fs below
     * (on Linux), should try FORCE before DETACH (the latter always
     * succeeds).
     */
    error = umount2_fs(mp_save->mnt->mnt_dir,
		       unmount_flags & (AMU_UMOUNT_DETACH|AMU_UMOUNT_FORCE));
  } else
#endif /* defined(HAVE_UMOUNT2) && defined(MNT2_GEN_OPT_DETACH) */
    error = UNMOUNT_TRAP(mp_save->mnt);

  /* Linux kernel can be sluggish for some reason */
  if (error == EBUSY && retries--) {
    struct timespec tm = {0, 200000000};
    nanosleep(&tm, NULL);
    goto again;
  }

  if (error < 0) {
    plog(XLOG_WARNING, "unmount(%s) failed: %m", mp_save->mnt->mnt_dir);
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

#if defined(HAVE_UMOUNT2) && defined(MNT2_GEN_OPT_FORCE)
    case EBUSY:
      /*
       * Caller determines if forced unmounts should be used now (for
       * EBUSY).  If caller asked to force an unmount, *and* the above
       * "trivial" unmount attempt failed with EBUSY, then try to force
       * the unmount.
       */
      if (unmount_flags & AMU_UMOUNT_FORCE) {
	error = umount2_fs(mp_save->mnt->mnt_dir,
			   unmount_flags & AMU_UMOUNT_FORCE);
	if (error < 0) {
	  plog(XLOG_WARNING, "%s: unmount/force: %m",
	       mp_save->mnt->mnt_dir);
	  error = errno;
	}
      }
      break;
#endif /* defined(HAVE_UMOUNT2) && defined(MNT2_GEN_OPT_FORCE) */

    default:
      dlog("%s: unmount: %m", mp_save->mnt->mnt_dir);
      break;
    }
  } else {
    dlog("unmount(%s) succeeded", mp_save->mnt->mnt_dir);
  }
  dlog("Finished unmount(%s)", mp_save->mnt->mnt_dir);

  /*
   * If we are successful or there was an ENOENT, remove
   * the mount entry from the mtab file.
   */
  if (error && error != ENOENT)
    goto out;

#ifdef HAVE_LOOP_DEVICE
  /* look for loop=/dev/loopX in mnt_opts */
  xopts = xstrdup(mp_save->mnt->mnt_opts); /* b/c strtok is destructive */
  for (opt = strtok(xopts, ","); opt; opt = strtok(NULL, ","))
    if (NSTREQ(opt, loopstr, sizeof(loopstr) - 1)) {
      loopdev = opt + sizeof(loopstr) - 1;
      if (delete_loop_device(loopdev) < 0)
	plog(XLOG_WARNING, "unmount() failed to release loop device %s: %m", loopdev);
      else
	plog(XLOG_INFO, "unmount() released loop device %s OK", loopdev);
      break;
    }
  if (xopts)
    XFREE(xopts);
#endif /* HAVE_LOOP_DEVICE */

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

 out:
  free_mntlist(mlist);

  return error;
}


#if defined(HAVE_UMOUNT2) && (defined(MNT2_GEN_OPT_FORCE) || defined(MNT2_GEN_OPT_DETACH))
/*
 * Force unmount, no questions asked, without touching mnttab file.  Try
 * detach first because it is safer: will remove the hung mnt point without
 * affecting hung applications.  "Force" is more risky: it will cause the
 * kernel to return EIO to applications stuck on a stat(2) of Amd.
 */
int
umount2_fs(const char *mntdir, u_int unmount_flags)
{
  int error = 0;

#ifdef MNT2_GEN_OPT_DETACH
  if (unmount_flags & AMU_UMOUNT_DETACH) {
    error = umount2(mntdir, MNT2_GEN_OPT_DETACH);
    if (error < 0 && (errno == EINVAL || errno == ENOENT))
      error = 0;		/* ignore EINVAL/ENOENT */
    if (error < 0) {		/* don't try FORCE if detach succeeded */
      plog(XLOG_WARNING, "%s: unmount/detach: %m", mntdir);
      /* fall through to try "force" (if flag specified) */
    } else {
      dlog("%s: unmount/detach: OK", mntdir);
      return error;
    }
  }
#endif /* MNT2_GEN_OPT_DETACH */

#ifdef MNT2_GEN_OPT_FORCE
  if (unmount_flags & AMU_UMOUNT_FORCE) {
    plog(XLOG_INFO, "umount2_fs: trying unmount/forced on %s", mntdir);
    error = umount2(mntdir, MNT2_GEN_OPT_FORCE);
    if (error < 0 && (errno == EINVAL || errno == ENOENT))
      error = 0;		/* ignore EINVAL/ENOENT */
    if (error < 0)
      plog(XLOG_WARNING, "%s: unmount/force: %m", mntdir);
    else
      dlog("%s: unmount/force: OK", mntdir);
    /* fall through to return whatever error we got (if any) */
  }
#endif /* MNT2_GEN_OPT_FORCE */

  return error;
}
#endif /* HAVE_UMOUNT2 && (MNT2_GEN_OPT_FORCE || MNT2_GEN_OPT_DETACH) */
