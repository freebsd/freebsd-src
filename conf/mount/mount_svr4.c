/*
 * Copyright (c) 1997-2014 Erez Zadok
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 * File: am-utils/conf/mount/mount_svr4.c
 *
 */

/*
 * SVR4:
 * Solaris 2.x (SunOS 5.x) and HPUX-11 Mount helper.
 *      -Erez Zadok <ezk@cs.columbia.edu>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


/*
 * On Solaris 8 with in-kernel mount table, pass mount options to kernel to
 * have them evaluated.  They will also show up in /etc/mnttab.
 */
#if defined(MNT2_GEN_OPT_OPTIONSTR) && defined(MAX_MNTOPT_STR)
# define sys_mount(fsname, dir, flags, type, data, datasize) \
	mount((fsname), (dir), (MNT2_GEN_OPT_OPTIONSTR | flags), (type), \
	      (data), (datasize), mountopts, sizeof(mountopts))
#else /* not defined(MNT2_GEN_OPT_OPTIONSTR) && defined(MAX_MNTOPT_STR) */
# define sys_mount(fsname, dir, flags, type, data, datasize) \
	mount((fsname), (dir), (flags), (type), (data), (datasize))
#endif /* not defined(MNT2_GEN_OPT_OPTIONSTR) && defined(MAX_MNTOPT_STR) */


/*
 * Map from conventional mount arguments
 * to Solaris 2.x (SunOS 5.x) style arguments.
 */
int
mount_svr4(char *fsname, char *dir, int flags, MTYPE_TYPE type, caddr_t data, const char *optstr)
{
#if defined(MNT2_GEN_OPT_OPTIONSTR) && defined(MAX_MNTOPT_STR)
  char mountopts[MAX_MNTOPT_STR];

  /*
   * Save a copy of the mount options.  The kernel will overwrite them with
   * those it recognizes.
   */
  xstrlcpy(mountopts, optstr, MAX_MNTOPT_STR);
#endif /* defined(MNT2_GEN_OPT_OPTIONSTR) && defined(MAX_MNTOPT_STR) */

#if defined(MOUNT_TYPE_NFS3) && defined(MNTTAB_TYPE_NFS3)
  if (STREQ(type, MOUNT_TYPE_NFS3)) {
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(nfs_args_t));
  }
#endif /* defined(MOUNT_TYPE_NFS3) && defined(MNTTAB_TYPE_NFS3) */

#if defined(MOUNT_TYPE_NFS) && defined(MNTTAB_TYPE_NFS)
  if (STREQ(type, MOUNT_TYPE_NFS)) {
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(nfs_args_t));
  }
#endif /* defined(MOUNT_TYPE_NFS) && defined(MNTTAB_TYPE_NFS) */

#if defined(MOUNT_TYPE_AUTOFS) && defined(MNTTAB_TYPE_AUTOFS)
  if (STREQ(type, MOUNT_TYPE_AUTOFS)) {
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(autofs_args_t));
  }
#endif /* defined(MOUNT_TYPE_AUTOFS) && defined(MNTTAB_TYPE_AUTOFS) */

#if defined(MOUNT_TYPE_UFS) && defined(MNTTAB_TYPE_UFS)
  if (STREQ(type, MOUNT_TYPE_UFS))
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(ufs_args_t));
#endif /* defined(MOUNT_TYPE_UFS) && defined(MNTTAB_TYPE_UFS) */

#if defined(MOUNT_TYPE_PCFS) && defined(MNTTAB_TYPE_PCFS)
  if (STREQ(type, MOUNT_TYPE_PCFS))
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(pcfs_args_t));
#endif /* defined(MOUNT_TYPE_PCFS) && defined(MNTTAB_TYPE_PCFS) */

#if defined(MOUNT_TYPE_CDFS) && defined(MNTTAB_TYPE_CDFS)
  /*
   * HSFS on Solaris allows for 3 HSFSMNT_* flags to be passed
   * as arguments to the mount().  These flags are bit fields in an
   * integer, and that integer is passed as the "data" of this system
   * call.  The flags are described in <sys/fs/hsfs_rrip.h>.  However,
   * Solaris does not have an interface to these.  It does not define
   * a structure hsfs_args or anything that one can figure out what
   * arguments to pass to mount(2) for this type of filesystem.
   * Therefore, until Sun does, no arguments are passed to this mount
   * below.
   * -Erez Zadok <ezk@cs.columbia.edu>.
   */
  if (STREQ(type, MOUNT_TYPE_CDFS))
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_FSS | flags),
		     type, (char *) NULL, 0);
#endif /* defined(MOUNT_TYPE_CDFS) && defined(MNTTAB_TYPE_CDFS) */

#if defined(MOUNT_TYPE_LOFS) && defined(MNTTAB_TYPE_LOFS)
  if (STREQ(type, MOUNT_TYPE_LOFS))
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_FSS | flags),
		     type, (char *) NULL, 0);
#endif /* defined(MOUNT_TYPE_LOFS) && defined(MNTTAB_TYPE_LOFS) */

#ifdef HAVE_FS_CACHEFS
# if defined(MOUNT_TYPE_CACHEFS) && defined(MNTTAB_TYPE_CACHEFS)
  if (STREQ(type, MOUNT_TYPE_CACHEFS))
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(cachefs_args_t));
# endif /* defined(MOUNT_TYPE_CACHEFS) && defined(MNTTAB_TYPE_CACHEFS) */
#endif /*HAVE_FS_CACHEFS */

#ifdef HAVE_FS_AUTOFS
# if defined(MOUNT_TYPE_AUTOFS) && defined(MNTTAB_TYPE_AUTOFS)
  if (STREQ(type, MOUNT_TYPE_AUTOFS))
    return sys_mount(fsname, dir, (MNT2_GEN_OPT_DATA | flags),
		     type, (char *) data, sizeof(autofs_args_t));
# endif /* defined(MOUNT_TYPE_AUTOFS) && defined(MNTTAB_TYPE_AUTOFS) */
#endif /* HAVE_FS_AUTOFS */

  return EINVAL;
}
