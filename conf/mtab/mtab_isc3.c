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
 * File: am-utils/conf/mtab/mtab_isc3.c
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

/* fd for /etc/.mnt.lock (also act as flag for: is_locked) */
static int mtlckf = 0;
static char mtlckname[] = "/etc/.mnt.lock";
static char mnttabname[] = "/etc/mnttab";


static void
unlockmnttab(void)
{
  if (mtlckf) {
    close(mtlckf);
    mtlckf = 0;
  }
}


static
lockfile(int fd, int type)
{
  struct flock lk;

  lk.l_type = type;
  lk.l_whence = 0;
  lk.l_start = 0;
  lk.l_len = 0;

  return fcntl(fd, F_SETLKW, &lk);
}


static
lockmnttab(void)
{
  if (mtlckf == 0) {		/* need lock on /etc/.mnt.lock */
    mtlckf = open(mtlckname, O_RDWR);
    if (mtlckf >= 0) {
      if (lockfile(mtlckf, F_WRLCK) < 0) {
	close(mtlckf);
	mtlckf = 0;
#ifdef DEBUG
	dlog("lock failed %m");
#endif /* DEBUG */
      } else {
	return 0;
      }
    }
  }
  plog(XLOG_ERROR, "Unable to lock %s: %m", mtlckname);
  return -1;
}


void
unlock_mntlist(void)
{
  dlog("unlock_mntlist: releasing");
  unlockmnttab();
}


/* convert from ix386 mnttab to amd mntent */
static mntent_t *
mnt_dup(mntent_t *mp)
{
  /* note: may not be null terminated */
  mntent_t *new_mp = ALLOC(mntent_t);
  char nullcpy[128];

  xstrlcpy(nullcpy, mp->mt_dev, 32);
  new_mp->mnt_fsname = xstrdup(nullcpy);

  xstrlcpy(nullcpy, mp->mt_filsys, 32);
  new_mp->mnt_dir = xstrdup(nullcpy);

  xstrlcpy(nullcpy, mp->mt_fstyp, 16);
  new_mp->mnt_type = xstrdup(nullcpy);

  xstrlcpy(nullcpy, mp->mt_mntopts, 64);
  new_mp->mnt_opts = xstrdup(nullcpy);

  new_mp->mnt_freq = 0;
  new_mp->mnt_passno = 0;

  new_mp->mnt_time = mp->mt_time;
  new_mp->mnt_ro = mp->mt_ro_flg;

  return new_mp;
}


/* convert back (static alloc) */
static mntent_t *
mtab_of(mntent_t *mnt)
{
  static mntent_t mt;

  xstrlcpy(mt.mt_dev, mnt->mnt_fsname, 32);
  xstrlcpy(mt.mt_filsys, mnt->mnt_dir, 32);

  mt.mt_ro_flg = mnt->mnt_ro;
  mt.mt_time = mnt->mnt_time;

  xstrlcpy(mt.mt_fstyp, mnt->mnt_type, 16);
  xstrlcpy(mt.mt_mntopts, mnt->mnt_opts, 64);

  return &mt;
}


/*
 * Read a mount table into memory
 */
mntlist *
read_mtab(char *fs, const char *mnttabname)
{
  mntlist **mpp, *mhp;
  /* From: Piete Brooks <pb@cl.cam.ac.uk> */
  int fd;
  mntent_t mountbuffer[NMOUNT], *fs_data;
  int ret;
  int nmts;

  if (lockmnttab() != 0)
    return (mntlist *) NULL;

  fd = open(mnttabname, O_RDONLY);
  if (fd < 0) {
    plog(XLOG_ERROR, "Can't open %s: %m", mnttabname);
    return (mntlist *) NULL;
  }
  mpp = &mhp;
  while ((ret = read(fd, (char *) mountbuffer, NMOUNT * sizeof(mntent_t))) > 0) {
    nmts = ret / sizeof(mntent_t);
    for (fs_data = mountbuffer; fs_data < &mountbuffer[nmts]; fs_data++) {
      /*
       * Allocate a new slot
       */
      *mpp = ALLOC(struct mntlist);

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
    plog(XLOG_ERROR, "read error on %s: %m", mnttabname);
    unlockmnttab();
    mhp = (mntlist *) NULL;
  }
  *mpp = NULL;

  close(fd);
  return mhp;
}


static
write_mntent_to_mtab(int fd, mntent_t *mnt)
{
  int wr;

eagain:
  wr = write(fd, (char *) mtab_of(mnt), sizeof(mntent_t));
  if (wr < 0) {
    switch (wr) {
    case EAGAIN:
      goto eagain;
    default:
      return -1;
    }
  }
  if (wr != sizeof(mntent_t))
      plog(XLOG_ERROR, "Can't write entry to %s: %m", mnttabname);
  return 0;
}


void
rewrite_mtab(mntlist *mp, const char *mnttabname)
{
  int fd;

  assert(mtlckf != 0);

  fd = open(mnttabname, O_RDWR | O_TRUNC);
  if (fd < 0) {
    plog(XLOG_ERROR, "Can't open %s: %m", mnttabname);
    unlockmnttab();
  }
  while (mp) {
    if (mp->mnt)
      write_mntent_to_mtab(fd, mp->mnt);
    mp = mp->mnext;
  }

  close(fd);
  unlockmnttab();
}


void
write_mntent(mntent_t *mp, const char *mnttabname)
{
  int fd;

  if (lockmnttab() == -1)
    return;

  fd = open(mnttabname, O_RDWR | O_APPEND);
  if (fd < 0) {
    plog(XLOG_ERROR, "Unable to append %s: %m", mnttabname);
    return;
  }
  write_mntent_to_mtab(fd, mp);

  close(fd);
  unlockmnttab();
}
