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
 * $Id: ops_cachefs.c,v 1.2 1999/01/10 21:53:49 ezk Exp $
 *
 */

/*
 * Caching filesystem (Solaris 2.x)
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/* forward declarations */
static char *cachefs_match(am_opts *fo);
static int cachefs_init(mntfs *mf);
static int cachefs_fmount(mntfs *mf);
static int cachefs_fumount(mntfs *mf);


/*
 * Ops structure
 */
am_ops cachefs_ops =
{
  "cachefs",
  cachefs_match,
  cachefs_init,
  amfs_auto_fmount,
  cachefs_fmount,
  amfs_auto_fumount,
  cachefs_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* cachefs_readlink */
  0,				/* post-mount actions */
  0,				/* post-umount actions */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_UBACKGROUND | FS_AMQINFO
};


/*
 * Check that f/s has all needed fields.
 * Returns: matched string if found, NULL otherwise.
 */
static char *
cachefs_match(am_opts *fo)
{
  /* sanity check */
  if (!fo->opt_rfs || !fo->opt_fs || !fo->opt_cachedir) {
    plog(XLOG_USER, "cachefs: must specify cachedir, rfs, and fs");
    return NULL;
  }

#ifdef DEBUG
  dlog("CACHEFS: using cache directory \"%s\"", fo->opt_cachedir);
#endif /* DEBUG */

  /* determine magic cookie to put in mtab */
  return strdup(fo->opt_cachedir);
}


/*
 * Initialize.
 * Returns: 0 if OK, non-zero (errno) if failed.
 */
static int
cachefs_init(mntfs *mf)
{
  /*
   * Save cache directory name
   */
  if (mf->mf_refc == 1) {
    mf->mf_private = (voidp) strdup(mf->mf_fo->opt_cachedir);
    mf->mf_prfree = (void (*)(voidp)) free;
  }

  return 0;
}


/*
 * mntpt is the mount point ($fs) [XXX: was 'dir']
 * backdir is the mounted pathname ($rfs) [XXX: was 'fs_name']
 * cachedir is the cache directory ($cachedir)
 */
static int
mount_cachefs(char *mntpt, char *backdir, char *cachedir, char *opts)
{
  cachefs_args_t ca;
  mntent_t mnt;
  int flags;
  char *cp;
  MTYPE_TYPE type = MOUNT_TYPE_CACHEFS;	/* F/S mount type */

  memset((voidp) &ca, 0, sizeof(ca)); /* Paranoid */

  /*
   * Fill in the mount structure
   */
  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = mntpt;
  mnt.mnt_fsname = backdir;
  mnt.mnt_type = MNTTAB_TYPE_CACHEFS;
  mnt.mnt_opts = opts;

  flags = compute_mount_flags(&mnt);

  /* Fill in cachefs mount arguments */

  /*
   * XXX: Caveats
   * (1) cache directory is NOT checked for sanity beforehand, nor is it
   * purged.  Maybe it should be purged first?
   * (2) cache directory is NOT locked.  Should we?
   */

  /* mount flags */
  ca.cfs_options.opt_flags = CFS_WRITE_AROUND | CFS_ACCESS_BACKFS;
  /* cache population size */
  ca.cfs_options.opt_popsize = DEF_POP_SIZE; /* default: 64K */
  /* filegrp size */
  ca.cfs_options.opt_fgsize = DEF_FILEGRP_SIZE; /* default: 256 */

  /* CFS ID for file system (must be unique) */
  ca.cfs_fsid = cachedir;

  /* CFS fscdir name */
  memset(ca.cfs_cacheid, 0, sizeof(ca.cfs_cacheid));
  /* append cacheid and mountpoint */
  sprintf(ca.cfs_cacheid, "%s:%s", ca.cfs_fsid, mntpt);
  /* convert '/' to '_' (Solaris does that...) */
  cp = ca.cfs_cacheid;
  while ((cp = strpbrk(cp, "/")) != NULL)
    *cp = '_';

  /* path for this cache dir */
  ca.cfs_cachedir = cachedir;

  /* back filesystem dir */
  ca.cfs_backfs = backdir;

  /* same as nfs values (XXX: need to handle these options) */
  ca.cfs_acregmin = 0;
  ca.cfs_acregmax = 0;
  ca.cfs_acdirmin = 0;
  ca.cfs_acdirmax = 0;

  /*
   * Call generic mount routine
   */
  return mount_fs(&mnt, flags, (caddr_t) &ca, 0, type, 0, NULL, mnttab_file_name);
}


static int
cachefs_fmount(mntfs *mf)
{
  int error;

  error = mount_cachefs(mf->mf_mount,
			mf->mf_fo->opt_rfs,
			mf->mf_fo->opt_cachedir,
			mf->mf_mopts);
  if (error) {
    errno = error;
    /* according to Solaris, if errno==ESRCH, "options to not match" */
    if (error == ESRCH)
      plog(XLOG_ERROR, "mount_cachefs: options to no match: %m");
    else
      plog(XLOG_ERROR, "mount_cachefs: %m");
    return error;
  }

  return 0;
}


static int
cachefs_fumount(mntfs *mf)
{
  int error;

  error = UMOUNT_FS(mf->mf_mount, mnttab_file_name);

  /*
   * In the case of cachefs, we must fsck the cache directory.  Otherwise,
   * it will remain inconsistent, and the next cachefs mount will fail
   * with the error "no space left on device" (ENOSPC).
   *
   * XXX: this is hacky! use fork/exec/wait instead...
   */
  if (!error) {
    char *cachedir = NULL;
    char cmd[128];

    cachedir = (char *) mf->mf_private;
    plog(XLOG_INFO, "running fsck on cache directory \"%s\"", cachedir);
    sprintf(cmd, "fsck -F cachefs %s", cachedir);
    system(cmd);
  }

  return error;
}
