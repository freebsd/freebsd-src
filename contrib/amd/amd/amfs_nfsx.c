/*
 * Copyright (c) 1997-2003 Erez Zadok
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
 * $Id: amfs_nfsx.c,v 1.3.2.4 2002/12/27 22:44:32 ezk Exp $
 *
 */

/*
 * NFS hierarchical mounts
 *
 * TODO: Re-implement.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * The rfs field contains a list of mounts to be done from
 * the remote host.
 */
typedef struct amfs_nfsx_mnt {
  mntfs *n_mnt;
  int n_error;
} amfs_nfsx_mnt;

struct amfs_nfsx {
  int nx_c;			/* Number of elements in nx_v */
  amfs_nfsx_mnt *nx_v;		/* Underlying mounts */
  amfs_nfsx_mnt *nx_try;
};

/* forward definitions */
static char *amfs_nfsx_match(am_opts *fo);
static int amfs_nfsx_fmount (mntfs *);
static int amfs_nfsx_fmount(mntfs *mf);
static int amfs_nfsx_fumount(mntfs *mf);
static int amfs_nfsx_init(mntfs *mf);

/*
 * Ops structure
 */
am_ops amfs_nfsx_ops =
{
  "nfsx",
  amfs_nfsx_match,
  amfs_nfsx_init,
  amfs_auto_fmount,
  amfs_nfsx_fmount,
  amfs_auto_fumount,
  amfs_nfsx_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* amfs_nfsx_readlink */
  0,				/* amfs_nfsx_mounted */
  0,				/* amfs_nfsx_umounted */
  find_nfs_srvr,		/* XXX */
	/* FS_UBACKGROUND| */ FS_AMQINFO
};


static char *
amfs_nfsx_match(am_opts *fo)
{
  char *xmtab;
  char *ptr;
  int len;

  if (!fo->opt_rfs) {
    plog(XLOG_USER, "amfs_nfsx: no remote filesystem specified");
    return FALSE;
  }

  if (!fo->opt_rhost) {
    plog(XLOG_USER, "amfs_nfsx: no remote host specified");
    return FALSE;
  }

  /* set default sublink */
  if (fo->opt_sublink == 0) {
    ptr = strchr(fo->opt_rfs, ',');
    if (ptr && ptr != (fo->opt_rfs + 1))
      fo->opt_sublink = strnsave(fo->opt_rfs + 1, ptr - fo->opt_rfs - 1);
  }

  /*
   * Remove trailing ",..." from ${fs}
   * After deslashifying, overwrite the end of ${fs} with "/"
   * to make sure it is unique.
   */
  if ((ptr = strchr(fo->opt_fs, ',')))
    *ptr = '\0';
  deslashify(fo->opt_fs);

  /*
   * Bump string length to allow trailing /
   */
  len = strlen(fo->opt_fs);
  fo->opt_fs = xrealloc(fo->opt_fs, len + 1 + 1);
  ptr = fo->opt_fs + len;

  /*
   * Make unique...
   */
  *ptr++ = '/';
  *ptr = '\0';

  /*
   * Determine magic cookie to put in mtab
   */
  xmtab = str3cat((char *) 0, fo->opt_rhost, ":", fo->opt_rfs);
#ifdef DEBUG
  dlog("NFSX: mounting remote server \"%s\", remote fs \"%s\" on \"%s\"",
       fo->opt_rhost, fo->opt_rfs, fo->opt_fs);
#endif /* DEBUG */

  return xmtab;
}


static void
amfs_nfsx_prfree(voidp vp)
{
  struct amfs_nfsx *nx = (struct amfs_nfsx *) vp;
  int i;

  for (i = 0; i < nx->nx_c; i++) {
    mntfs *m = nx->nx_v[i].n_mnt;
    if (m)
      free_mntfs(m);
  }

  XFREE(nx->nx_v);
  XFREE(nx);
}


static int
amfs_nfsx_init(mntfs *mf)
{
  /*
   * mf_info has the form:
   *   host:/prefix/path,sub,sub,sub
   */
  int i;
  int glob_error;
  struct amfs_nfsx *nx;
  int asked_for_wakeup = 0;

  nx = (struct amfs_nfsx *) mf->mf_private;

  if (nx == 0) {
    char **ivec;
    char *info = 0;
    char *host;
    char *pref;
    int error = 0;

    info = strdup(mf->mf_info);
    host = strchr(info, ':');
    if (!host) {
      error = EINVAL;
      goto errexit;
    }
    pref = host +1;
    host = info;

    /*
     * Split the prefix off from the suffices
     */
    ivec = strsplit(pref, ',', '\'');

    /*
     * Count array size
     */
    for (i = 0; ivec[i]; i++) ;

    nx = ALLOC(struct amfs_nfsx);
    mf->mf_private = (voidp) nx;
    mf->mf_prfree = amfs_nfsx_prfree;

    nx->nx_c = i - 1;		/* i-1 because we don't want the prefix */
    nx->nx_v = (amfs_nfsx_mnt *) xmalloc(nx->nx_c * sizeof(amfs_nfsx_mnt));
    {
      char *mp = 0;
      char *xinfo = 0;
      char *fs = mf->mf_fo->opt_fs;
      char *rfs = 0;
      for (i = 0; i < nx->nx_c; i++) {
	char *path = ivec[i + 1];
	rfs = str3cat(rfs, pref, "/", path);
	/*
	 * Determine the mount point.
	 * If this is the root, then don't remove
	 * the trailing slash to avoid mntfs name clashes.
	 */
	mp = str3cat(mp, fs, "/", rfs);
	normalize_slash(mp);
	deslashify(mp);
	/*
	 * Determine the mount info
	 */
	xinfo = str3cat(xinfo, host, *path == '/' ? "" : "/", path);
	normalize_slash(xinfo);
	if (pref[1] != '\0')
	  deslashify(xinfo);
#ifdef DEBUG
	dlog("amfs_nfsx: init mount for %s on %s", xinfo, mp);
#endif /* DEBUG */
	nx->nx_v[i].n_error = -1;
	nx->nx_v[i].n_mnt = find_mntfs(&nfs_ops, mf->mf_fo, mp, xinfo, "", mf->mf_mopts, mf->mf_remopts);
      }
      if (rfs)
	XFREE(rfs);
      if (mp)
	XFREE(mp);
      if (xinfo)
	XFREE(xinfo);
    }

    XFREE(ivec);
  errexit:
    if (info)
      XFREE(info);
    if (error)
      return error;
  }

  /*
   * Iterate through the mntfs's and call
   * the underlying init routine on each
   */
  glob_error = 0;

  for (i = 0; i < nx->nx_c; i++) {
    amfs_nfsx_mnt *n = &nx->nx_v[i];
    mntfs *m = n->n_mnt;
    int error = (*m->mf_ops->fs_init) (m);
    /*
     * if you just "return error" here, you will have made a failure
     * in any submounts to fail the whole group.  There was old unused code
     * here before.
     */
    if (error > 0)
      n->n_error = error;

    else if (error < 0) {
      glob_error = -1;
      if (!asked_for_wakeup) {
	asked_for_wakeup = 1;
	sched_task(wakeup_task, (voidp) mf, (voidp) m);
      }
    }
  }

  return glob_error;
}


static void
amfs_nfsx_cont(int rc, int term, voidp closure)
{
  mntfs *mf = (mntfs *) closure;
  struct amfs_nfsx *nx = (struct amfs_nfsx *) mf->mf_private;
  amfs_nfsx_mnt *n = nx->nx_try;

  n->n_mnt->mf_flags &= ~(MFF_ERROR | MFF_MOUNTING);
  mf->mf_flags &= ~MFF_ERROR;

  /*
   * Wakeup anything waiting for this mount
   */
  wakeup((voidp) n->n_mnt);

  if (rc || term) {
    if (term) {
      /*
       * Not sure what to do for an error code.
       */
      plog(XLOG_ERROR, "mount for %s got signal %d", n->n_mnt->mf_mount, term);
      n->n_error = EIO;
    } else {
      /*
       * Check for exit status
       */
      errno = rc;		/* XXX */
      plog(XLOG_ERROR, "%s: mount (amfs_nfsx_cont): %m", n->n_mnt->mf_mount);
      n->n_error = rc;
    }
    free_mntfs(n->n_mnt);
    n->n_mnt = new_mntfs();
    n->n_mnt->mf_error = n->n_error;
    n->n_mnt->mf_flags |= MFF_ERROR;
  } else {
    /*
     * The mount worked.
     */
    mf_mounted(n->n_mnt);
    n->n_error = 0;
  }

  /*
   * Do the remaining bits
   */
  if (amfs_nfsx_fmount(mf) >= 0) {
    wakeup((voidp) mf);
    mf->mf_flags &= ~MFF_MOUNTING;
    mf_mounted(mf);
  }
}


static int
try_amfs_nfsx_mount(voidp mv)
{
  mntfs *mf = (mntfs *) mv;
  int error;

  mf->mf_flags |= MFF_MOUNTING;
  error = (*mf->mf_ops->fmount_fs) (mf);
  mf->mf_flags &= ~MFF_MOUNTING;

  return error;
}


static int
amfs_nfsx_remount(mntfs *mf, int fg)
{
  struct amfs_nfsx *nx = (struct amfs_nfsx *) mf->mf_private;
  amfs_nfsx_mnt *n;
  int glob_error = -1;

  for (n = nx->nx_v; n < nx->nx_v + nx->nx_c; n++) {
    mntfs *m = n->n_mnt;
    if (n->n_error < 0) {
      if (!(m->mf_flags & MFF_MKMNT) && m->mf_ops->fs_flags & FS_MKMNT) {
	int error = mkdirs(m->mf_mount, 0555);
	if (!error)
	  m->mf_flags |= MFF_MKMNT;
      }
    }
  }

  /*
   * Iterate through the mntfs's and mount each filesystem
   * which is not yet mounted.
   */
  for (n = nx->nx_v; n < nx->nx_v + nx->nx_c; n++) {
    mntfs *m = n->n_mnt;
    if (n->n_error < 0) {
      /*
       * Check fmount entry pt. exists
       * and then mount...
       */
      if (!m->mf_ops->fmount_fs) {
	n->n_error = EINVAL;
      } else {
#ifdef DEBUG
	dlog("calling underlying fmount on %s", m->mf_mount);
#endif /* DEBUG */
	if (!fg && foreground && (m->mf_ops->fs_flags & FS_MBACKGROUND)) {
	  m->mf_flags |= MFF_MOUNTING;	/* XXX */
#ifdef DEBUG
	  dlog("backgrounding mount of \"%s\"", m->mf_info);
#endif /* DEBUG */
	  nx->nx_try = n;
	  run_task(try_amfs_nfsx_mount, (voidp) m, amfs_nfsx_cont, (voidp) mf);
	  n->n_error = -1;
	  return -1;
	} else {
#ifdef DEBUG
	  dlog("foreground mount of \"%s\" ...", mf->mf_info);
#endif /* DEBUG */
	  n->n_error = (*m->mf_ops->fmount_fs) (m);
	}
      }

#ifdef DEBUG
      if (n->n_error > 0) {
	errno = n->n_error;	/* XXX */
	dlog("underlying fmount of %s failed: %m", m->mf_mount);
      }
#endif /* DEBUG */

      if (n->n_error == 0) {
	glob_error = 0;
      } else if (glob_error < 0) {
	glob_error = n->n_error;
      }
    }
  }

  return glob_error < 0 ? 0 : glob_error;
}


static int
amfs_nfsx_fmount(mntfs *mf)
{
  return amfs_nfsx_remount(mf, FALSE);
}


/*
 * Unmount an NFS hierarchy.
 * Note that this is called in the foreground
 * and so may hang under extremely rare conditions.
 */
static int
amfs_nfsx_fumount(mntfs *mf)
{
  struct amfs_nfsx *nx = (struct amfs_nfsx *) mf->mf_private;
  amfs_nfsx_mnt *n;
  int glob_error = 0;

  /*
   * Iterate in reverse through the mntfs's and unmount each filesystem
   * which is mounted.
   */
  for (n = nx->nx_v + nx->nx_c - 1; n >= nx->nx_v; --n) {
    mntfs *m = n->n_mnt;
    /*
     * If this node has not been messed with
     * and there has been no error so far
     * then try and unmount.
     * If an error had occurred then zero
     * the error code so that the remount
     * only tries to unmount those nodes
     * which had been successfully unmounted.
     */
    if (n->n_error == 0) {
#ifdef DEBUG
      dlog("calling underlying fumount on %s", m->mf_mount);
#endif /* DEBUG */
      n->n_error = (*m->mf_ops->fumount_fs) (m);
      if (n->n_error) {
	glob_error = n->n_error;
	n->n_error = 0;
      } else {
	/*
	 * Make sure remount gets this node
	 */
	n->n_error = -1;
      }
    }
  }

  /*
   * If any unmounts failed then remount the
   * whole lot...
   */
  if (glob_error) {
    glob_error = amfs_nfsx_remount(mf, TRUE);
    if (glob_error) {
      errno = glob_error;	/* XXX */
      plog(XLOG_USER, "amfs_nfsx: remount of %s failed: %m", mf->mf_mount);
    }
    glob_error = EBUSY;
  } else {
    /*
     * Remove all the mount points
     */
    for (n = nx->nx_v; n < nx->nx_v + nx->nx_c; n++) {
      mntfs *m = n->n_mnt;
      am_node am;

      /*
       * XXX: all the umounted handler needs is a
       * mntfs pointer, so pass an am_node with the right
       * pointer in it.
       */
      memset((voidp) &am, 0, sizeof(am));
      am.am_mnt = m;
#ifdef DEBUG
      dlog("calling underlying umounted on %s", m->mf_mount);
#endif /* DEBUG */
      (*m->mf_ops->umounted) (&am);

      if (n->n_error < 0) {
	if (m->mf_ops->fs_flags & FS_MKMNT) {
	  (void) rmdirs(m->mf_mount);
	  m->mf_flags &= ~MFF_MKMNT;
	}
      }
      free_mntfs(m);
      n->n_mnt = 0;
      n->n_error = -1;
    }
  }

  return glob_error;
}
