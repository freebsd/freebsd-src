/*
 * Copyright (c) 1997-2004 Erez Zadok
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
 * $Id: amfs_link.c,v 1.3.2.4 2004/01/06 03:15:16 ezk Exp $
 *
 */

/*
 * Symbol-link file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>


/*
 * Ops structures
 */
am_ops amfs_link_ops =
{
  "link",
  amfs_link_match,
  0,				/* amfs_link_init */
  amfs_auto_fmount,
  amfs_link_fmount,
  amfs_auto_fumount,
  amfs_link_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* amfs_link_readlink */
  0,				/* amfs_link_mounted */
  0,				/* amfs_link_umounted */
  find_amfs_auto_srvr,
  0
};


/*
 * SFS needs a link.
 */
char *
amfs_link_match(am_opts *fo)
{

  if (!fo->opt_fs) {
    plog(XLOG_USER, "link: no fs specified");
    return 0;
  }

  /*
   * Bug report (14/12/89) from Jay Plett <jay@princeton.edu>
   * If an automount point has the same name as an existing
   * link type mount Amd hits a race condition and either hangs
   * or causes a symlink loop.
   *
   * If fs begins with a '/' change the opt_fs & opt_sublink
   * fields so that the fs option doesn't end up pointing at
   * an existing symlink.
   *
   * If sublink is nil then set sublink to fs
   * else set sublink to fs / sublink
   *
   * Finally set fs to ".".
   */
  if (*fo->opt_fs == '/') {
    char *fullpath;
    char *link = fo->opt_sublink;
    if (link) {
      if (*link == '/')
	fullpath = strdup(link);
      else
	fullpath = str3cat((char *) 0, fo->opt_fs, "/", link);
    } else {
      fullpath = strdup(fo->opt_fs);
    }

    if (fo->opt_sublink)
      XFREE(fo->opt_sublink);
    fo->opt_sublink = fullpath;
    fo->opt_fs = str3cat(fo->opt_fs, ".", fullpath, "");
  }

  return strdup(fo->opt_fs);
}


int
amfs_link_fmount(mntfs *mf)
{
  /*
   * Wow - this is hard to implement! :-)
   */
  return 0;
}


int
amfs_link_fumount(mntfs *mf)
{
  return 0;
}
