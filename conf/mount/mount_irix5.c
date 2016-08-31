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
 * File: am-utils/conf/mount/mount_irix5.c
 *
 */

/*
 * IRIX Mount helper
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


/*
 * Map from conventional mount arguments
 * to IRIX style arguments.
 *
 * NOTE: We have to use struct nfs_args (not nfs_args_t) below because
 * while IRIX5.3 needs the fh_len field added to struct nfs_args for NFS3
 * mounts to succeed, the the mount syscall fails if the argument size
 * includes fh_len! Talk about a broken interface ...  -- stolcke 7/4/97
 */
int
mount_irix(char *fsname, char *dir, int flags, MTYPE_TYPE type, voidp data)
{

#ifdef DEBUG
  dlog("mount_irix: fsname %s, dir %s, type %d", fsname, dir, type);
#endif /* DEBUG */

#ifdef HAVE_FS_NFS3
  if (STREQ(type, MOUNT_TYPE_NFS3))
    return mount(fsname, dir, (MNT2_GEN_OPT_FSS | MNT2_GEN_OPT_DATA | flags),
		 type, (struct nfs_args *) data, sizeof(struct nfs_args));
#endif /* HAVE_FS_NFS3 */

#ifdef HAVE_FS_NFS
  if (STREQ(type, MOUNT_TYPE_NFS))
    return mount(fsname, dir, (MNT2_GEN_OPT_FSS | MNT2_GEN_OPT_DATA | flags),
		 type, (struct nfs_args *) data, sizeof(struct nfs_args));
#endif /* HAVE_FS_NFS */

  /* XXX: do I need to pass {u,x,e}fs_args ? */

#ifdef HAVE_FS_UFS
  if (STREQ(type, MOUNT_TYPE_UFS))
    return mount(fsname, dir, (MNT2_GEN_OPT_FSS | flags), type);
#endif /* HAVE_FS_UFS */

#ifdef HAVE_FS_EFS
  if (STREQ(type, MOUNT_TYPE_EFS))
    return mount(fsname, dir, (MNT2_GEN_OPT_FSS | flags), type);
#endif /* HAVE_FS_EFS */

#ifdef HAVE_FS_XFS
  if (STREQ(type, MOUNT_TYPE_XFS))
    return mount(fsname, dir, (MNT2_GEN_OPT_FSS | flags), type);
#endif /* HAVE_FS_XFS */

  return EINVAL;
}
