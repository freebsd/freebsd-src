/*
 * Copyright (c) 1997-1999 Erez Zadok
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      %W% (Berkeley) %G%
 *
 * $Id: ops_lofs.c,v 1.2 1999/01/10 21:53:49 ezk Exp $
 *
 */

/*
 * Loopback file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/* forward definitions */
static char * lofs_match(am_opts *fo);
static int lofs_fmount(mntfs *mf);
static int lofs_fumount(mntfs *mf);
static int mount_lofs(char *dir, char *fs_name, char *opts);


/*
 * Ops structure
 */
am_ops lofs_ops =
{
  "lofs",
  lofs_match,
  0,				/* lofs_init */
  amfs_auto_fmount,
  lofs_fmount,
  amfs_auto_fumount,
  lofs_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* lofs_readlink */
  0,				/* lofs_mounted */
  0,				/* lofs_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO
};


/*
 * LOFS needs remote filesystem.
 */
static char *
lofs_match(am_opts *fo)
{
  if (!fo->opt_rfs) {
    plog(XLOG_USER, "lofs: no source filesystem specified");
    return 0;
  }
#ifdef DEBUG
  dlog("LOFS: mounting fs \"%s\" on \"%s\"",
       fo->opt_rfs, fo->opt_fs);
#endif /* DEBUG */

  /*
   * Determine magic cookie to put in mtab
   */
  return strdup(fo->opt_rfs);
}


static int
mount_lofs(char *dir, char *fs_name, char *opts)
{
  mntent_t mnt;
  int flags;

  /*
   * Figure out the name of the file system type.
   */
  MTYPE_TYPE type = MOUNT_TYPE_LOFS;

  /*
   * Fill in the mount structure
   */
  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = fs_name;
  mnt.mnt_type = MNTTAB_TYPE_LOFS;
  mnt.mnt_opts = opts;

  flags = compute_mount_flags(&mnt);

  /*
   * Call generic mount routine
   */
  return mount_fs(&mnt, flags, NULL, 0, type, 0, NULL, mnttab_file_name);
}


static int
lofs_fmount(mntfs *mf)
{
  int error;

  error = mount_lofs(mf->mf_mount, mf->mf_info, mf->mf_mopts);
  if (error) {
    errno = error;
    plog(XLOG_ERROR, "mount_lofs: %m");
    return error;
  }
  return 0;
}


static int
lofs_fumount(mntfs *mf)
{
  return UMOUNT_FS(mf->mf_mount, mnttab_file_name);
}
