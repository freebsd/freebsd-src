/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
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
 *	@(#)os-irix4.h	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 * IRIX 4.0.X definitions for Amd (automounter)
 * Contributed by Scott R. Presnell <srp@cgl.ucsf.edu>
 */

/*
 * Does the compiler grok void *
 */
#define VOIDP

/*
 * Which version of the Sun RPC library we are using
 * This is the implementation release number, not
 * the protocol revision number.
 */
#define RPC_3

/*
 * Which version of the NFS interface are we using.
 * This is the implementation release number, not
 * the protocol revision number.
 */
#define NFS_3

/*
 * Byte ordering
 */
#undef ARCH_ENDIAN
#define ARCH_ENDIAN	"big"

/*
 * Has support for syslog()
 */
#define HAS_SYSLOG

#define M_RDONLY	MS_RDONLY
#define M_GRPID		MS_GRPID
#define M_NOSUID	MS_NOSUID
#define M_NONDEV	MS_NODEV

/*
 * Support for ndbm
 */
#define OS_HAS_NDBM

#define UPDATE_MTAB

#undef	MTAB_TYPE_NFS
#define MTAB_TYPE_NFS	"nfs"

#undef	MTAB_TYPE_UFS
#define MTAB_TYPE_UFS	"efs"

#define NMOUNT	40	/* The std sun value */
/*
 * Name of filesystem types
 */
#define MOUNT_TYPE_UFS	sysfs(GETFSIND, FSID_EFS)
#define MOUNT_TYPE_NFS	sysfs(GETFSIND, FSID_NFS)

#define SYS5_SIGNALS

/*
 * Use <fcntl.h> rather than <sys/file.h>
 */
/*#define USE_FCNTL*/

/*
 * Use fcntl() rather than flock()
 */
/*#define LOCK_FCNTL*/

#ifdef __GNUC__
#define alloca(sz) __builtin_alloca(sz)
#endif

#define bzero(ptr, len) memset(ptr, 0, len)
#define bcopy(from, to, len) memcpy(to, from, len)

#undef MOUNT_TRAP
#define MOUNT_TRAP(type, mnt, flags, mnt_data) \
	irix_mount(mnt->mnt_fsname, mnt->mnt_dir,flags, type, mnt_data)
#undef UNMOUNT_TRAP
#define UNMOUNT_TRAP(mnt)	umount(mnt->mnt_dir)
#define NFDS	30	/* conservative */

#define NFS_HDR "misc-irix.h"
#define UFS_HDR "misc-irix.h"

/* not included in sys/param.h */
#include <sys/types.h>

#define MOUNT_HELPER_SOURCE "mount_irix.c"

/*
 * Under 4.0.X this information is in /usr/include/mntent.h
 * Below is what is used to be for Irix 3.3.X.
 */
/*#define	MNTINFO_DEV	"fsid"*/
/*#define	MNTINFO_PREF	"0x"*/

#define	MNTINFO_PREF	""

/*
 * Under Irix, mount type "auto" is probed by statfs() in df.  A statfs() of
 * a direct mount causes that mount to fire.  So change the  mount type in
 * /etc/mtab to "ignore" to stop that (this is what SGI does for their
 * automounter.  Use the old FASCIST define for this.
 */
#define FASCIST_DF_COMMAND MNTTYPE_IGNORE
