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
 * $Id: amfs_direct.c,v 1.2 1999/01/10 21:53:41 ezk Exp $
 *
 */

/*
 * Direct file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/****************************************************************************
 *** FORWARD DEFINITIONS                                                  ***
 ****************************************************************************/
static am_node *amfs_direct_readlink(am_node *mp, int *error_return);

/****************************************************************************
 *** OPS STRUCTURES                                                       ***
 ****************************************************************************/
am_ops amfs_direct_ops =
{
  "direct",
  amfs_auto_match,
  0,				/* amfs_direct_init */
  amfs_toplvl_mount,
  0,
  amfs_toplvl_umount,
  0,
  amfs_error_lookuppn,
  amfs_error_readdir,
  amfs_direct_readlink,
  amfs_toplvl_mounted,
  0,				/* amfs_auto_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO
};


/****************************************************************************
 *** FUNCTIONS                                                             ***
 ****************************************************************************/

static am_node *
amfs_direct_readlink(am_node *mp, int *error_return)
{
  am_node *xp;
  int rc = 0;

  xp = next_nonerror_node(mp->am_child);
  if (!xp) {
    if (!mp->am_mnt->mf_private)
      amfs_auto_mkcacheref(mp->am_mnt);	/* XXX */
    xp = amfs_auto_lookuppn(mp, mp->am_path + 1, &rc, VLOOK_CREATE);
  }
  if (xp) {
    new_ttl(xp);		/* (7/12/89) from Rein Tollevik */
    return xp;
  }
  if (amd_state == Finishing)
    rc = ENOENT;
  *error_return = rc;
  return 0;
}
