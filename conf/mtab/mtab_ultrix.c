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
 * File: am-utils/conf/mtab/mtab_ultrix.c
 *
 */

/*
 * Include before config.h to force single definition of gt_names[] here.
 * This can be done unconditionally since this file is Ultrix specific
 * anyway and <sys/fs_types.h> is properly protected from multiple inclusion.
 * - Rainer Orth <ro@TechFak.Uni-Bielefeld.DE>
 * Hack hack hack.  Sigh. -Erez.
 */
#include <sys/fs_types.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

#ifndef NMOUNT
# define NMOUNT 20
#endif /* NMOUNT */


static mntent_t *
mnt_dup(struct fs_data *mp)
{
  mntent_t *new_mp = ALLOC(mntent_t);

  new_mp->mnt_fsname = xstrdup(mp->fd_devname);
  new_mp->mnt_dir = xstrdup(mp->fd_path);
  if (mp->fd_fstype >= GT_NUMTYPES)
    mp->fd_fstype = GT_UNKWN;
  else if (gt_names[mp->fd_fstype] == 0)
    mp->fd_fstype = GT_UNKWN;
  new_mp->mnt_type = xstrdup(gt_names[mp->fd_fstype]);
  new_mp->mnt_opts = xstrdup("unset");

  new_mp->mnt_freq = 0;
  new_mp->mnt_passno = mp->fd_dev;

  return new_mp;
}


/*
 * Read a mount table into memory
 */
mntlist *
read_mtab(char *fs, const char *mnttabname)
{
  mntlist **mpp, *mhp;
  /* From: Piete Brooks <pb@cl.cam.ac.uk> */
  int loc = 0;
  struct fs_data mountbuffer[NMOUNT], *fs_data;
  int ret;

  mpp = &mhp;
  while ((ret = getmountent(&loc, mountbuffer, NMOUNT)) > 0) {
    for (fs_data = mountbuffer; fs_data < &mountbuffer[ret]; fs_data++) {
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
    plog(XLOG_ERROR, "getmountent: %m");
    return 0;
  }
  *mpp = NULL;

  return mhp;
}
