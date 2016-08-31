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
 * File: am-utils/conf/mount/mount_hpux.c
 *
 */

/*
 * HPUX:
 * HPUX 9.0 and 10.0 Mount helper.
 *      -Erez Zadok <ezk@cs.columbia.edu>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


/*
 * Map from conventional mount arguments
 * to HPUX style arguments.
 */
int
mount_hpux(MTYPE_TYPE type, const char *dir, int flags, caddr_t data)
{

#if defined(MOUNT_TYPE_NFS) && defined(MOUNT_NFS)
  if (STREQ(type, MOUNT_TYPE_NFS))
    return vfsmount(MOUNT_NFS, dir, flags, data);
#endif /* defined(MOUNT_TYPE_NFS) && defined(MOUNT_NFS) */

#if defined(MOUNT_TYPE_UFS) && defined(MOUNT_UFS)
  if (STREQ(type, MOUNT_TYPE_UFS))
    return vfsmount(MOUNT_UFS, dir, flags, data);
#endif /* defined(MOUNT_TYPE_UFS) && defined(MOUNT_UFS) */

#if defined(MOUNT_TYPE_PCFS) && defined(MOUNT_PC)
  /*
   * MOUNT_TYPE_PCFS gets redefined in conf/trap/trap_hpux.h because of
   * stupidities in HPUX 9.0's headers.
   */
  if (STREQ(type, MOUNT_TYPE_PCFS))
    return vfsmount(MOUNT_PC, dir, flags, data);
#endif /* defined(MOUNT_TYPE_PCFS) && defined(MOUNT_PC) */

#if defined(MOUNT_TYPE_CDFS) && defined(MOUNT_CDFS)
  if (STREQ(type, MOUNT_TYPE_CDFS))
    return vfsmount(MOUNT_CDFS, dir, flags, data);
#endif /* defined(MOUNT_TYPE_CDFS) && defined(MOUNT_CDFS) */

  return EINVAL;
}
