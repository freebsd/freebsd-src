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
 * $Id: ops_cdfs.c,v 1.3 1999/03/30 17:22:46 ezk Exp $
 *
 */

/*
 * High Sierra (CD-ROM) file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/* forward declarations */
static char *cdfs_match(am_opts *fo);
static int cdfs_fmount(mntfs *mf);
static int cdfs_fumount(mntfs *mf);

/*
 * Ops structure
 */
am_ops cdfs_ops =
{
  "cdfs",
  cdfs_match,
  0,				/* cdfs_init */
  amfs_auto_fmount,
  cdfs_fmount,
  amfs_auto_fumount,
  cdfs_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* cdfs_readlink */
  0,				/* cdfs_mounted */
  0,				/* cdfs_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_UBACKGROUND | FS_AMQINFO
};


/*
 * CDFS needs remote filesystem.
 */
static char *
cdfs_match(am_opts *fo)
{
  if (!fo->opt_dev) {
    plog(XLOG_USER, "cdfs: no source device specified");
    return 0;
  }
#ifdef DEBUG
  dlog("CDFS: mounting device \"%s\" on \"%s\"",
       fo->opt_dev, fo->opt_fs);
#endif /* DEBUG */

  /*
   * Determine magic cookie to put in mtab
   */
  return strdup(fo->opt_dev);
}


static int
mount_cdfs(char *dir, char *fs_name, char *opts)
{
  cdfs_args_t cdfs_args;
  mntent_t mnt;
  int genflags, cdfs_flags;

  /*
   * Figure out the name of the file system type.
   */
  MTYPE_TYPE type = MOUNT_TYPE_CDFS;

  memset((voidp) &cdfs_args, 0, sizeof(cdfs_args)); /* Paranoid */
  cdfs_flags = 0;

  /*
   * Fill in the mount structure
   */
  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = fs_name;
  mnt.mnt_type = MNTTAB_TYPE_CDFS;
  mnt.mnt_opts = opts;

#if defined(MNT2_CDFS_OPT_DEFPERM) && defined(MNTTAB_OPT_DEFPERM)
  if (hasmntopt(&mnt, MNTTAB_OPT_DEFPERM))
# ifdef MNT2_CDFS_OPT_DEFPERM
    cdfs_flags |= MNT2_CDFS_OPT_DEFPERM;
# else /* not MNT2_CDFS_OPT_DEFPERM */
    cdfs_flags &= ~MNT2_CDFS_OPT_NODEFPERM;
# endif /* not MNT2_CDFS_OPT_DEFPERM */
#endif /* defined(MNT2_CDFS_OPT_DEFPERM) && defined(MNTTAB_OPT_DEFPERM) */

#if defined(MNT2_CDFS_OPT_NODEFPERM) && defined(MNTTAB_OPT_NODEFPERM)
  if (hasmntopt(&mnt, MNTTAB_OPT_NODEFPERM))
    cdfs_flags |= MNT2_CDFS_OPT_NODEFPERM;
#endif /* MNTTAB_OPT_NODEFPERM */

#if defined(MNT2_CDFS_OPT_NOVERSION) && defined(MNTTAB_OPT_NOVERSION)
  if (hasmntopt(&mnt, MNTTAB_OPT_NOVERSION))
    cdfs_flags |= MNT2_CDFS_OPT_NOVERSION;
#endif /* defined(MNT2_CDFS_OPT_NOVERSION) && defined(MNTTAB_OPT_NOVERSION) */

#if defined(MNT2_CDFS_OPT_RRIP) && defined(MNTTAB_OPT_RRIP)
  if (hasmntopt(&mnt, MNTTAB_OPT_RRIP))
    cdfs_flags |= MNT2_CDFS_OPT_RRIP;
#endif /* defined(MNT2_CDFS_OPT_RRIP) && defined(MNTTAB_OPT_RRIP) */
#if defined(MNT2_CDFS_OPT_NORRIP) && defined(MNTTAB_OPT_NORRIP)
  if (hasmntopt(&mnt, MNTTAB_OPT_NORRIP))
    cdfs_flags |= MNT2_CDFS_OPT_NORRIP;
#endif /* defined(MNT2_CDFS_OPT_NORRIP) && defined(MNTTAB_OPT_NORRIP) */

#if defined(MNT2_CDFS_OPT_GENS) && defined(MNTTAB_OPT_GENS)
  if (hasmntopt(&mnt, MNTTAB_OPT_GENS))
    cdfs_flags |= MNT2_CDFS_OPT_GENS;
#endif /* defined(MNT2_CDFS_OPT_GENS) && defined(MNTTAB_OPT_GENS) */
#if defined(MNT2_CDFS_OPT_EXTATT) && defined(MNTTAB_OPT_EXTATT)
  if (hasmntopt(&mnt, MNTTAB_OPT_EXTATT))
    cdfs_flags |= MNT2_CDFS_OPT_EXTATT;
#endif /* defined(MNT2_CDFS_OPT_EXTATT) && defined(MNTTAB_OPT_EXTATT) */

  genflags = compute_mount_flags(&mnt);

#ifdef HAVE_FIELD_CDFS_ARGS_T_FLAGS
  cdfs_args.flags = cdfs_flags;
#endif /* HAVE_FIELD_CDFS_ARGS_T_FLAGS */

#ifdef HAVE_FIELD_CDFS_ARGS_T_ISO_FLAGS
  cdfs_args.iso_flags = genflags | cdfs_flags;
#endif /* HAVE_FIELD_CDFS_ARGS_T_ISO_FLAGS */

#ifdef HAVE_FIELD_CDFS_ARGS_T_ISO_PGTHRESH
  cdfs_args.iso_pgthresh = hasmntval(&mnt, MNTTAB_OPT_PGTHRESH);
#endif /* HAVE_FIELD_CDFS_ARGS_T_ISO_PGTHRESH */

#ifdef HAVE_FIELD_CDFS_ARGS_T_FSPEC
  cdfs_args.fspec = fs_name;
#endif /* HAVE_FIELD_CDFS_ARGS_T_FSPEC */

#ifdef HAVE_FIELD_CDFS_ARGS_T_NORRIP
  /* XXX: need to provide norrip mount opt */
  cdfs_args.norrip = 0;		/* use Rock-Ridge Protocol extensions */
#endif /* HAVE_FIELD_CDFS_ARGS_T_NORRIP */

#ifdef HAVE_FIELD_CDFS_ARGS_T_SSECTOR
  /* XXX: need to provide ssector mount option */
  cdfs_args.ssector = 0;	/* use 1st session on disk */
#endif /* HAVE_FIELD_CDFS_ARGS_T_SSECTOR */

  /*
   * Call generic mount routine
   */
  return mount_fs(&mnt, genflags, (caddr_t) &cdfs_args, 0, type, 0, NULL, mnttab_file_name);
}


static int
cdfs_fmount(mntfs *mf)
{
  int error;

  error = mount_cdfs(mf->mf_mount, mf->mf_info, mf->mf_mopts);
  if (error) {
    errno = error;
    plog(XLOG_ERROR, "mount_cdfs: %m");
    return error;
  }
  return 0;
}


static int
cdfs_fumount(mntfs *mf)
{
  return UMOUNT_FS(mf->mf_mount, mnttab_file_name);
}
