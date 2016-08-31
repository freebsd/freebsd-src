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
 * File: am-utils/conf/mtab/mtab_svr4.c
 *
 * How to manage the mount table file.  Based on other SVR3 ports.
 *      -Erez Zadok <ezk@cs.columbia.edu>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

/*
 * file descriptor for lock file
 * values: -1  no file-descriptor was set yet (or mnttab unlocked, or error
 *             in locking).
 *         >=0 legal file-descriptor value (file lock succeeded)
 */
static int mntent_lock_fd = -1;


#ifdef MOUNT_TABLE_ON_FILE
static char mtlckname[] = "/etc/.mnttab.lock";
#endif /* MOUNT_TABLE_ON_FILE */


/****************************************************************************/
/*** Private functions                                                      */
/****************************************************************************/

static void
unlockmnttab(void)
{
#ifdef MOUNT_TABLE_ON_FILE
  if (mntent_lock_fd >= 0) {
    close(mntent_lock_fd);
    mntent_lock_fd = -1;
  }
#endif /* MOUNT_TABLE_ON_FILE */
}


#ifdef MOUNT_TABLE_ON_FILE
static int
lockfile(int fd, int type)
{
  struct flock lk;
  int ret;

  lk.l_type = type;
  lk.l_whence = 0;
  lk.l_start = 0;
  lk.l_len = 0;

  /*
   * F_SETLKW means to block until the read or write block is free to be
   * locked.
   */
  ret = fcntl(fd, F_SETLKW, &lk);
  return ret;
}
#endif /* MOUNT_TABLE_ON_FILE */


/* return 0 if locking succeeded, -1 if failed */
static int
lockmnttab(void)
{
#ifdef MOUNT_TABLE_ON_FILE
  /* if mnttab file is locked, all is well */
  if (mntent_lock_fd >= 0)
    return 0;

  /* need to lock mnttab file. first, open the file */
  mntent_lock_fd = open(mtlckname, O_RDWR | O_CREAT, 0600);
  if (mntent_lock_fd < 0) {
    plog(XLOG_ERROR, "Unable to open/creat %s: %m", mtlckname);
    return -1;
  }

  /* if succeeded in opening the file, try to lock it */
  if (lockfile(mntent_lock_fd, F_WRLCK) < 0) {
    close(mntent_lock_fd);
    mntent_lock_fd = -1;
#ifdef DEBUG
    dlog("lock %s failed: %m", mtlckname);
#endif /* DEBUG */
    return -1;
  }
#else /* not MOUNT_TABLE_ON_FILE */
  /* fake lock for in-kernel mount table */
#endif /* not MOUNT_TABLE_ON_FILE */

  /* finally, succeeded in also locking the file */
  return 0;
}


/*
 * Convert from solaris mnttab to Amd mntent.  Since am-utils uses
 * native "struct mnttab" if available, this really copies fields of
 * the same structure.
 */
static mntent_t *
mnt_dup(const mntent_t *mtp)
{
  mntent_t *mep = ALLOC(mntent_t);

  mep->mnt_fsname = xstrdup(mtp->mnt_fsname);
  mep->mnt_dir = xstrdup(mtp->mnt_dir);
  mep->mnt_type = xstrdup(mtp->mnt_type);
  mep->mnt_opts = xstrdup(mtp->mnt_opts);
  mep->mnt_time = xstrdup(mtp->mnt_time);

  return mep;
}


/*
 * Adjust arguments in mntent_t.
 */
#ifdef MOUNT_TABLE_ON_FILE
static mntent_t *
update_mnttab_fields(const mntent_t *mnt)
{
  static mntent_t mt;
  static char timestr[16];
  struct timeval tv;

  /* most fields don't change, only update mnt_time below */
  mt.mnt_fsname = mnt->mnt_fsname;
  mt.mnt_dir = mnt->mnt_dir;
  mt.mnt_type = mnt->mnt_type;
  mt.mnt_opts = mnt->mnt_opts;

  /*
   * Solaris 2.5 and newer take a second argument to gettimeofday().  If you
   * find a useful svr4-like OS that uses the old style, and this code here
   * fails, then create a new autoconf test that will determine the number
   * of arguments gettimeofday() takes.  -Erez.
   */
  if (gettimeofday(&tv, NULL) < 0)
    timestr[0] = '\0';
  else
    xsnprintf(timestr, sizeof(timestr), "%ld", tv.tv_sec);

  mt.mnt_time = timestr;

  return &mt;
}
#endif /* MOUNT_TABLE_ON_FILE */


static void
write_mntent_to_mtab(FILE *fp, const mntent_t *mnt)
{
#ifdef MOUNT_TABLE_ON_FILE
  putmntent(fp, update_mnttab_fields(mnt));
#endif /* MOUNT_TABLE_ON_FILE */
}


/****************************************************************************/
/*** Public functions                                                       */
/****************************************************************************/


void
unlock_mntlist(void)
{
  unlockmnttab();
}


/*
 * Read a mount table into memory
 */
mntlist *
read_mtab(char *fs, const char *mnttabname)
{
  mntlist **mpp, *mhp;
  FILE *fp;
  mntent_t mountbuf;
  int ret;

  if (lockmnttab() < 0)		/* failed locking */
    return NULL;

  fp = fopen(mnttabname, "r");
  if (fp == NULL) {
    plog(XLOG_ERROR, "Can't open %s: %m", mnttabname);
    return NULL;
  }
  mpp = &mhp;

  while ((ret = getmntent(fp, &mountbuf)) == 0) {
    /*
     * Allocate a new slot
     */
    *mpp = ALLOC(struct mntlist);

    /*
     * Copy the data returned by getmntent
     */
    (*mpp)->mnt = mnt_dup(&mountbuf);

    /*
     * Move to next pointer
     */
    mpp = &(*mpp)->mnext;
  }

  if (ret > 0) {
    plog(XLOG_ERROR, "read error on %s: %m", mnttabname);
    unlockmnttab();
    mhp = NULL;
  }
  *mpp = NULL;

  fclose(fp);
  return mhp;
}


void
rewrite_mtab(mntlist *mp, const char *mnttabname)
{
  FILE *fp;

  assert(mntent_lock_fd >= 0);	/* ensure lock fd is valid */

  fp = fopen(mnttabname, "r+");
  if (fp == NULL) {
    plog(XLOG_ERROR, "Can't open %s: %m", mnttabname);
    unlockmnttab();
    return;
  }
  while (mp) {
    if (mp->mnt)
      write_mntent_to_mtab(fp, mp->mnt);
    mp = mp->mnext;
  }

  ftruncate(fileno(fp), ftell(fp));
  fclose(fp);
  unlockmnttab();
}


void
write_mntent(mntent_t *mtp, const char *mnttabname)
{
  FILE *fp;

  if (lockmnttab() < 0)
    return;

  fp = fopen(mnttabname, "a");
  if (fp == NULL) {
    plog(XLOG_ERROR, "Unable to append %s: %m", mnttabname);
    return;
  }
  write_mntent_to_mtab(fp, mtp);

  fclose(fp);
  unlockmnttab();
}
