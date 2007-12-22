/*
 * Copyright (c) 1997-2004 Erez Zadok
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989 The Regents of the University of California.
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
 * $Id: amfs_inherit.c,v 1.3.2.4 2004/01/06 03:15:16 ezk Exp $
 *
 */

/*
 * Inheritance file system.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * This implements a filesystem restart.
 *
 * This is a *gross* hack - it knows far too
 * much about the way other parts of the
 * system work.  See restart.c too.
 */

static char *amfs_inherit_match(am_opts *fo);
static int amfs_inherit_fmount(mntfs *mf);
static int amfs_inherit_fumount(mntfs *mf);
static int amfs_inherit_init(mntfs *mf);
static int amfs_inherit_mount(am_node *mp);


/*
 * Ops structure
 */
am_ops amfs_inherit_ops =
{
  "inherit",
  amfs_inherit_match,
  amfs_inherit_init,
  amfs_inherit_mount,
  amfs_inherit_fmount,
  amfs_auto_fumount,
  amfs_inherit_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* amfs_inherit_readlink */
  0,				/* amfs_inherit_mounted */
  0,				/* amfs_inherit_umounted */
  find_amfs_auto_srvr,
  FS_DISCARD
};


/*
 * This should never be called.
 */
static char *
amfs_inherit_match(am_opts *fo)
{
  plog(XLOG_FATAL, "amfs_inherit_match called!");
  return 0;
}


static int
amfs_inherit_init(mntfs *mf)
{
  mntfs *mf_link = (mntfs *) mf->mf_private;

  if (mf_link == 0) {
    plog(XLOG_ERROR, "Remount collision on %s?", mf->mf_mount);
    plog(XLOG_FATAL, "Attempting to init not-a-filesystem");
    return EINVAL;
  }

  if (mf_link->mf_ops->fs_init)
    return (*mf_link->mf_ops->fs_init) (mf_link);
  return 0;
}


/*
 * Take the linked mount point and
 * propagate.
 */
static mntfs *
amfs_inherit_inherit(mntfs *mf)
{
  mntfs *mf_link = (mntfs *) mf->mf_private;

  if (mf_link == 0) {
    plog(XLOG_FATAL, "Attempting to inherit not-a-filesystem");
    return 0;			/* XXX */
  }
  mf_link->mf_fo = mf->mf_fo;

  /*
   * Discard the old map.
   * Don't call am_unmounted since this
   * node was never really mounted in the
   * first place.
   */
  mf->mf_private = 0;
  free_mntfs(mf);

  /*
   * Free the dangling reference
   * to the mount link.
   */
  free_mntfs(mf_link);

  /*
   * Get a hold of the other entry
   */
  mf_link->mf_flags &= ~MFF_RESTART;

  /* Say what happened */
  plog(XLOG_INFO, "restarting %s on %s", mf_link->mf_info, mf_link->mf_mount);

  return mf_link;
}


static int
amfs_inherit_mount(am_node *mp)
{
  mntfs *newmf = amfs_inherit_inherit(mp->am_mnt);

  if (newmf) {
    mp->am_mnt = newmf;
    /*
     * XXX - must do the am_mounted call here
     */
    if (newmf->mf_ops->fs_flags & FS_MBACKGROUND)
      am_mounted(mp);

    new_ttl(mp);
    return 0;
  }
  return EINVAL;
}


static int
amfs_inherit_fmount(mntfs *mf)
{
  am_node *mp = find_mf(mf);

  if (mp)
    return amfs_inherit_mount(mp);
  return amfs_inherit_inherit(mf) ? 0 : EINVAL;
}


static int
amfs_inherit_fumount(mntfs *mf)
{
  /*
   * Always succeed
   */
  return 0;
}
