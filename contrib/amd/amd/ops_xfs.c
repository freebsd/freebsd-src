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
 * $Id: ops_xfs.c,v 1.2 1999/01/10 21:53:52 ezk Exp $
 *
 */

/*
 * Irix UN*X file system: XFS (Extended File System)
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/* forward declarations */
static char * xfs_match(am_opts *fo);
static int xfs_fmount(mntfs *mf);
static int xfs_fumount(mntfs *mf);

/*
 * Ops structure
 */
am_ops xfs_ops =
{
  "xfs",
  xfs_match,
  0,				/* xfs_init */
  amfs_auto_fmount,
  xfs_fmount,
  amfs_auto_fumount,
  xfs_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* xfs_readlink */
  0,				/* xfs_mounted */
  0,				/* xfs_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO
};


/*
 * XFS needs local filesystem and device.
 */
static char *
xfs_match(am_opts *fo)
{

  if (!fo->opt_dev) {
    plog(XLOG_USER, "xfs: no device specified");
    return 0;
  }

#ifdef DEBUG
  dlog("XFS: mounting device \"%s\" on \"%s\"", fo->opt_dev, fo->opt_fs);
#endif /* DEBUG */

  /*
   * Determine magic cookie to put in mtab
   */
  return strdup(fo->opt_dev);
}


static int
mount_xfs(char *dir, char *fs_name, char *opts)
{
  xfs_args_t xfs_args;
  mntent_t mnt;
  int flags;

  /*
   * Figure out the name of the file system type.
   */
  MTYPE_TYPE type = MOUNT_TYPE_XFS;

  memset((voidp) &xfs_args, 0, sizeof(xfs_args)); /* Paranoid */

  /*
   * Fill in the mount structure
   */
  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = fs_name;
  mnt.mnt_type = MNTTAB_TYPE_XFS;
  mnt.mnt_opts = opts;

  flags = compute_mount_flags(&mnt);

#ifdef HAVE_FIELD_XFS_ARGS_T_FLAGS
  xfs_args.flags = 0;		/* XXX: fix this to correct flags */
#endif /* HAVE_FIELD_XFS_ARGS_T_FLAGS */
#ifdef HAVE_FIELD_XFS_ARGS_T_FSPEC
  xfs_args.fspec = fs_name;
#endif /* HAVE_FIELD_XFS_ARGS_T_FSPEC */

  /*
   * Call generic mount routine
   */
  return mount_fs(&mnt, flags, (caddr_t) &xfs_args, 0, type, 0, NULL, mnttab_file_name);
}


static int
xfs_fmount(mntfs *mf)
{
  int error;

  error = mount_xfs(mf->mf_mount, mf->mf_info, mf->mf_mopts);
  if (error) {
    errno = error;
    plog(XLOG_ERROR, "mount_xfs: %m");
    return error;
  }

  return 0;
}


static int
xfs_fumount(mntfs *mf)
{
  return UMOUNT_FS(mf->mf_mount, mnttab_file_name);
}
