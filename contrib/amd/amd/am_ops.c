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
 * $Id: am_ops.c,v 1.6.2.7 2004/01/06 03:15:16 ezk Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>


/*
 * The order of these entries matters, since lookups in this table are done
 * on a first-match basis.  The entries below are a mixture of native
 * filesystems supported by the OS (HAVE_FS_FOO), and some meta-filesystems
 * supported by amd (HAVE_AMU_FS_FOO).  The order is set here in expected
 * match-hit such that more popular filesystems are listed first (nfs is the
 * most popular, followed by a symlink F/S)
 */
static am_ops *vops[] =
{
#ifdef HAVE_FS_NFS
  &nfs_ops,			/* network F/S (version 2) */
#endif /* HAVE_FS_NFS */
#ifdef HAVE_AMU_FS_LINK
  &amfs_link_ops,		/* symlink F/S */
#endif /* HAVE_AMU_FS_LINK */

  /*
   * Other amd-supported meta-filesystems.
   */
#ifdef HAVE_AMU_FS_NFSX
  &amfs_nfsx_ops,		/* multiple-nfs F/S */
#endif /* HAVE_AMU_FS_NFSX */
#ifdef HAVE_AMU_FS_NFSL
  &amfs_nfsl_ops,		/* NFS with local link existence check */
#endif /* HAVE_AMU_FS_NFSL */
#ifdef HAVE_AMU_FS_HOST
  &amfs_host_ops,		/* multiple exported nfs F/S */
#endif /* HAVE_AMU_FS_HOST */
#ifdef HAVE_AMU_FS_LINKX
  &amfs_linkx_ops,		/* symlink F/S with link target verify */
#endif /* HAVE_AMU_FS_LINKX */
#ifdef HAVE_AMU_FS_PROGRAM
  &amfs_program_ops,		/* program F/S */
#endif /* HAVE_AMU_FS_PROGRAM */
#ifdef HAVE_AMU_FS_UNION
  &amfs_union_ops,		/* union F/S */
#endif /* HAVE_AMU_FS_UNION */
#ifdef HAVE_AMU_FS_INHERIT
  &amfs_inherit_ops,		/* inheritance F/S */
#endif /* HAVE_AMU_FS_INHERIT */

  /*
   * A few more native filesystems.
   */
#ifdef HAVE_FS_UFS
  &ufs_ops,			/* Unix F/S */
#endif /* HAVE_FS_UFS */
#ifdef HAVE_FS_XFS
  &xfs_ops,			/* Unix (irix) F/S */
#endif /* HAVE_FS_XFS */
#ifdef HAVE_FS_EFS
  &efs_ops,			/* Unix (irix) F/S */
#endif /* HAVE_FS_EFS */
#ifdef HAVE_FS_LOFS
  &lofs_ops,			/* loopback F/S */
#endif /* HAVE_FS_LOFS */
#ifdef HAVE_FS_CDFS
  &cdfs_ops,			/* CDROM/HSFS/ISO9960 F/S */
#endif /* HAVE_FS_CDFS */
#ifdef HAVE_FS_PCFS
  &pcfs_ops,			/* Floppy/MSDOS F/S */
#endif /* HAVE_FS_PCFS */
#ifdef HAVE_FS_CACHEFS
  &cachefs_ops,			/* caching F/S */
#endif /* HAVE_FS_CACHEFS */
#ifdef HAVE_FS_NULLFS
/* FILL IN */			/* null (loopback) F/S */
#endif /* HAVE_FS_NULLFS */
#ifdef HAVE_FS_UNIONFS
/* FILL IN */			/* union (bsd44) F/S */
#endif /* HAVE_FS_UNIONFS */
#ifdef HAVE_FS_UMAPFS
/* FILL IN */			/* uid/gid mapping F/S */
#endif /* HAVE_FS_UMAPFS */

  /*
   * These 5 should be last, in the order:
   *	(1) amfs_auto
   *	(2) amfs_direct
   *	(3) amfs_toplvl
   *	(4) amfs_error
   */
#ifdef HAVE_AMU_FS_AUTO
  &amfs_auto_ops,		/* Automounter F/S */
#endif /* HAVE_AMU_FS_AUTO */
#ifdef HAVE_AMU_FS_DIRECT
  &amfs_direct_ops,		/* direct-mount F/S */
#endif /* HAVE_AMU_FS_DIRECT */
#ifdef HAVE_AMU_FS_TOPLVL
  &amfs_toplvl_ops,		/* top-level mount F/S */
#endif /* HAVE_AMU_FS_TOPLVL */
#ifdef HAVE_AMU_FS_ERROR
  &amfs_error_ops,		/* error F/S */
#endif /* HAVE_AMU_FS_ERROR */
  0
};


void
ops_showamfstypes(char *buf)
{
  struct am_ops **ap;
  int l = 0;

  buf[0] = '\0';
  for (ap = vops; *ap; ap++) {
    strcat(buf, (*ap)->fs_type);
    if (ap[1])
      strcat(buf, ", ");
    l += strlen((*ap)->fs_type) + 2;
    if (l > 62) {
      l = 0;
      strcat(buf, "\n      ");
    }
  }
}


static void
ops_show1(char *buf, int *lp, const char *name)
{
  strcat(buf, name);
  strcat(buf, ", ");
  *lp += strlen(name) + 2;
  if (*lp > 60) {
    strcat(buf, "\t\n");
    *lp = 0;
  }
}


void
ops_showfstypes(char *buf)
{
  int l = 0;

  buf[0] = '\0';

#ifdef MNTTAB_TYPE_CACHEFS
  ops_show1(buf, &l, MNTTAB_TYPE_CACHEFS);
#endif /* MNTTAB_TYPE_CACHEFS */

#ifdef MNTTAB_TYPE_CDFS
  ops_show1(buf, &l, MNTTAB_TYPE_CDFS);
#endif /* MNTTAB_TYPE_CDFS */

#ifdef MNTTAB_TYPE_CFS
  ops_show1(buf, &l, MNTTAB_TYPE_CFS);
#endif /* MNTTAB_TYPE_CFS */

#ifdef MNTTAB_TYPE_LOFS
  ops_show1(buf, &l, MNTTAB_TYPE_LOFS);
#endif /* MNTTAB_TYPE_LOFS */

#ifdef MNTTAB_TYPE_EFS
  ops_show1(buf, &l, MNTTAB_TYPE_EFS);
#endif /* MNTTAB_TYPE_EFS */

#ifdef MNTTAB_TYPE_MFS
  ops_show1(buf, &l, MNTTAB_TYPE_MFS);
#endif /* MNTTAB_TYPE_MFS */

#ifdef MNTTAB_TYPE_NFS
  ops_show1(buf, &l, MNTTAB_TYPE_NFS);
#endif /* MNTTAB_TYPE_NFS */

#ifdef MNTTAB_TYPE_NFS3
  ops_show1(buf, &l, "nfs3");	/* always hard-code as nfs3 */
#endif /* MNTTAB_TYPE_NFS3 */

#ifdef MNTTAB_TYPE_NULLFS
  ops_show1(buf, &l, MNTTAB_TYPE_NULLFS);
#endif /* MNTTAB_TYPE_NULLFS */

#ifdef MNTTAB_TYPE_PCFS
  ops_show1(buf, &l, MNTTAB_TYPE_PCFS);
#endif /* MNTTAB_TYPE_PCFS */

#ifdef MNTTAB_TYPE_TFS
  ops_show1(buf, &l, MNTTAB_TYPE_TFS);
#endif /* MNTTAB_TYPE_TFS */

#ifdef MNTTAB_TYPE_TMPFS
  ops_show1(buf, &l, MNTTAB_TYPE_TMPFS);
#endif /* MNTTAB_TYPE_TMPFS */

#ifdef MNTTAB_TYPE_UFS
  ops_show1(buf, &l, MNTTAB_TYPE_UFS);
#endif /* MNTTAB_TYPE_UFS */

#ifdef MNTTAB_TYPE_UMAPFS
  ops_show1(buf, &l, MNTTAB_TYPE_UMAPFS);
#endif /* MNTTAB_TYPE_UMAPFS */

#ifdef MNTTAB_TYPE_UNIONFS
  ops_show1(buf, &l, MNTTAB_TYPE_UNIONFS);
#endif /* MNTTAB_TYPE_UNIONFS */

#ifdef MNTTAB_TYPE_XFS
  ops_show1(buf, &l, MNTTAB_TYPE_XFS);
#endif /* MNTTAB_TYPE_XFS */

  /* terminate with a period, newline, and NULL */
  if (buf[strlen(buf)-1] == '\n')
    buf[strlen(buf) - 4] = '\0';
  else
    buf[strlen(buf) - 2] = '\0';
  strcat(buf, ".\n");
}


/*
 * return string option which is the reverse of opt.
 * nosuid -> suid
 * quota -> noquota
 * ro -> rw
 * etc.
 * may return pointer to static buffer or subpointer within opt.
 */
static char *
reverse_option(const char *opt)
{
  static char buf[80];

  /* sanity check */
  if (!opt)
    return NULL;

  /* check special cases */
  /* XXX: if this gets too long, rewrite the code more flexibly */
  if (STREQ(opt, "ro")) return "rw";
  if (STREQ(opt, "rw")) return "ro";
  if (STREQ(opt, "bg")) return "fg";
  if (STREQ(opt, "fg")) return "bg";
  if (STREQ(opt, "soft")) return "hard";
  if (STREQ(opt, "hard")) return "soft";

  /* check if string starts with 'no' and chop it */
  if (NSTREQ(opt, "no", 2)) {
    strcpy(buf, &opt[2]);
  } else {
    /* finally return a string prepended with 'no' */
    strcpy(buf, "no");
    strcat(buf, opt);
  }
  return buf;
}


/*
 * start with an empty string. for each opts1 option that is not
 * in opts2, add it to the string (make sure the reverse of it
 * isn't in either). finally add opts2. return new string.
 * Both opts1 and opts2 must not be null!
 * Caller must eventually free the string being returned.
 */
static char *
merge_opts(const char *opts1, const char *opts2)
{
  mntent_t mnt2;		/* place holder for opts2 */
  char *newstr;			/* new string to return (malloc'ed) */
  char *tmpstr;			/* temp */
  char *eq;			/* pointer to whatever follows '=' within temp */
  char oneopt[80];		/* one option w/o value if any */
  char *revoneopt;		/* reverse of oneopt */
  int len = strlen(opts1) + strlen(opts2) + 2; /* space for "," and NULL */
  char *s1 = strdup(opts1);	/* copy of opts1 to munge */

  /* initialization */
  mnt2.mnt_opts = (char *) opts2;
  newstr = xmalloc(len);
  newstr[0] = '\0';

  for (tmpstr = strtok(s1, ",");
       tmpstr;
       tmpstr = strtok(NULL, ",")) {
    /* copy option to temp buffer */
    strncpy(oneopt, tmpstr, 80);
    oneopt[79] = '\0';
    /* if option has a value such as rsize=1024, chop the value part */
    if ((eq = haseq(oneopt)))
      *eq = '\0';
    /* find reverse option of oneopt */
    revoneopt = reverse_option(oneopt);
    /* if option orits reverse exist in opts2, ignore it */
    if (hasmntopt(&mnt2, oneopt) || hasmntopt(&mnt2, revoneopt))
      continue;
    /* add option to returned string */
    if (newstr && newstr[0]) {
      strcat(newstr, ",");
      strcat(newstr, tmpstr);
    } else {
      strcpy(newstr, tmpstr);
    }
  }

  /* finally, append opts2 itself */
  if (newstr && newstr[0]) {
    strcat(newstr, ",");
    strcat(newstr, opts2);
  } else {
    strcpy(newstr, opts2);
  }

  XFREE(s1);
  return newstr;
}


am_ops *
ops_match(am_opts *fo, char *key, char *g_key, char *path, char *keym, char *map)
{
  am_ops **vp;
  am_ops *rop = 0;

  /*
   * First crack the global opts and the local opts
   */
  if (!eval_fs_opts(fo, key, g_key, path, keym, map)) {
    rop = &amfs_error_ops;
  } else if (fo->opt_type == 0) {
    plog(XLOG_USER, "No fs type specified (key = \"%s\", map = \"%s\")", keym, map);
    rop = &amfs_error_ops;
  } else {
    /*
     * Next find the correct filesystem type
     */
    for (vp = vops; (rop = *vp); vp++)
      if (STREQ(rop->fs_type, fo->opt_type))
	break;
    if (!rop) {
      plog(XLOG_USER, "fs type \"%s\" not recognized", fo->opt_type);
      rop = &amfs_error_ops;
    }
  }

  /*
   * Make sure we have a default mount option.
   * Otherwise skip past any leading '-'.
   */
  if (fo->opt_opts == 0)
    fo->opt_opts = strdup("rw,defaults");
  else if (*fo->opt_opts == '-') {
    /*
     * We cannot simply do fo->opt_opts++ here since the opts
     * module will try to free the pointer fo->opt_opts later.
     * So just reallocate the thing -- stolcke 11/11/94
     */
    char *old = fo->opt_opts;
    fo->opt_opts = strdup(old + 1);
    XFREE(old);
  }

  /*
   * If addopts option was used, then append it to the
   * current options and remote mount options.
   */
  if (fo->opt_addopts) {
    if (STREQ(fo->opt_opts, fo->opt_remopts)) {
      /* optimize things for the common case where opts==remopts */
      char *mergedstr;
      mergedstr = merge_opts(fo->opt_opts, fo->opt_addopts);
      plog(XLOG_INFO, "merge rem/opts \"%s\" add \"%s\" => \"%s\"",
	   fo->opt_opts, fo->opt_addopts, mergedstr);
      XFREE(fo->opt_opts);
      XFREE(fo->opt_remopts);
      fo->opt_opts = mergedstr;
      fo->opt_remopts = strdup(mergedstr);
    } else {
      char *mergedstr, *remmergedstr;
      mergedstr = merge_opts(fo->opt_opts, fo->opt_addopts);
      plog(XLOG_INFO, "merge opts \"%s\" add \"%s\" => \"%s\"",
	   fo->opt_opts, fo->opt_addopts, mergedstr);
      XFREE(fo->opt_opts);
      fo->opt_opts = mergedstr;
      remmergedstr = merge_opts(fo->opt_remopts, fo->opt_addopts);
      plog(XLOG_INFO, "merge remopts \"%s\" add \"%s\" => \"%s\"",
	   fo->opt_remopts, fo->opt_addopts, remmergedstr);
      XFREE(fo->opt_remopts);
      fo->opt_remopts = remmergedstr;
    }
  }

  /*
   * Check the filesystem is happy
   */
  if (fo->fs_mtab)
    XFREE(fo->fs_mtab);

  if ((fo->fs_mtab = (*rop->fs_match) (fo)))
    return rop;

  /*
   * Return error file system
   */
  fo->fs_mtab = (*amfs_error_ops.fs_match) (fo);
  return &amfs_error_ops;
}
