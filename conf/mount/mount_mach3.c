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
 * File: am-utils/conf/mount/mount_mach3.c
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


int
mount_mach3(char *type, char *mnt, int flags, caddr_t mnt_data)
{
  int err = 0;
  errno = 0;

  if (syscall(SYS_vfsmount, type, mnt->mnt_dir, flags, mnt_data)) {
    err = -1;
    if (errno == ENODEV) {
      /*
       * might be an old kernel, need to try again
       * with file type number instead of string
       */
      int typeno = 1;
      plog(XLOG_ERROR, "%s: 1SYS_vfsmount: %m", mnt->mnt_dir);

      if (STREQ(mnt->mnt_type, MOUNT_TYPE_UFS))
        typeno = 0;
      else if (STREQ(mnt->mnt_type, MOUNT_TYPE_NFS))
        typeno = 1;
      else
	plog(XLOG_ERROR, "%s: type defaults to nfs...", mnt->mnt_dir);

      plog(XLOG_ERROR, "%s: retry SYS_vfsmount %s %d", mnt->mnt_dir,
	   mnt->mnt_type, typeno);
      if (typeno >= 0) {
        if (syscall(SYS_vfsmount, typeno, mnt->mnt_dir, flags, mnt_data)) {
          plog(XLOG_ERROR, "%s: 2SYS_vfsmount: %m", mnt->mnt_dir);
	} else {
          err = 0;
	}
      }
    }
  }
  return err;
}
