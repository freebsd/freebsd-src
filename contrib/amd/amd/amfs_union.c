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
 * $Id: amfs_union.c,v 1.3.2.4 2002/12/27 22:44:33 ezk Exp $
 *
 */

/*
 * Union automounter file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/****************************************************************************
 *** FORWARD DEFINITIONS                                                  ***
 ****************************************************************************/
static void amfs_union_mounted(mntfs *mf);


/****************************************************************************
 *** OPS STRUCTURES                                                       ***
 ****************************************************************************/
am_ops amfs_union_ops =
{
  "union",
  amfs_auto_match,
  0,				/* amfs_auto_init */
  amfs_toplvl_mount,
  0,
  amfs_toplvl_umount,
  0,
  amfs_auto_lookuppn,
  amfs_auto_readdir,
  0,				/* amfs_toplvl_readlink */
  amfs_union_mounted,
  0,				/* amfs_toplvl_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO | FS_DIRECTORY
};


/*
 * Create a reference to a union'ed entry
 * XXX: this function may not be used anywhere...
 */
static int
create_amfs_union_node(char *dir, voidp arg)
{
  if (!STREQ(dir, "/defaults")) {
    int error = 0;
    (void) amfs_toplvl_ops.lookuppn(arg, dir, &error, VLOOK_CREATE);
    if (error > 0) {
      errno = error;		/* XXX */
      plog(XLOG_ERROR, "unionfs: could not mount %s: %m", dir);
    }
    return error;
  }
  return 0;
}


static void
amfs_union_mounted(mntfs *mf)
{
  int i;

  amfs_auto_mkcacheref(mf);

  /*
   * Having made the union mount point,
   * populate all the entries...
   */
  for (i = 0; i <= last_used_map; i++) {
    am_node *mp = exported_ap[i];
    if (mp && mp->am_mnt == mf) {
      /* return value from create_amfs_union_node is ignored by mapc_keyiter */
      (void) mapc_keyiter((mnt_map *) mp->am_mnt->mf_private,
			  (void (*)(char *, voidp)) create_amfs_union_node,
			  mp);
      break;
    }
  }
}
