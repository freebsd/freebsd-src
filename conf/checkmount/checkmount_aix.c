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
 * File: am-utils/conf/checkmount/checkmount_aix.c
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>


/*
 * These were missing external definitions from old AIX's headers.  They
 * appear to be available in <sys/vmount.h> on AIX 5.3, and possibly
 * earlier. Hence I commented this out.
 */
#ifndef HAVE_EXTERN_MNTCTL
extern int mntctl(int cmd, int size, voidp buf);
#endif /* not HAVE_EXTERN_MNTCTL */
extern int is_same_host(char *name1, char *name2, struct in_addr addr2);


int
fixmount_check_mount(char *host, struct in_addr hostaddr, char *path)
{
  int ret, i;
  char *mntinfo = NULL, *cp;
  char *short_hostname, *long_hostname, *mount_point;
  struct vmount *vp;

  /*
   * First figure out size of mount table and allocate space for a copy...
   * Then get mount table for real.
   */
  ret = mntctl(MCTL_QUERY, sizeof(i), (char *) &i);
  if (ret == 0) {
    mntinfo = xmalloc(i);
    ret = mntctl(MCTL_QUERY, i, mntinfo);
  }
  if (ret <= 0) {
    fprintf(stderr, "mntctl: %m");
    XFREE(mntinfo);
    exit(1);
  }

  /* iterate over each vmount structure */
  for (i = 0, cp = mntinfo; i < ret; i++, cp += vp->vmt_length) {
    vp = (struct vmount *) cp;
    mount_point = vmt2dataptr(vp, VMT_STUB);
    long_hostname = vmt2dataptr(vp, VMT_HOSTNAME);
    short_hostname = vmt2dataptr(vp, VMT_HOST);
    if (STREQ(path, mount_point) &&
	(is_same_host(long_hostname, host, hostaddr) ||
	 is_same_host(short_hostname, host, hostaddr)))
      return 1;
  }

  return 0;
}
