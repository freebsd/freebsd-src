/*
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1995 Artisoft, Inc.  All Rights Reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)vfs_conf.c	8.8 (Berkeley) 3/31/94
 * $Id: vfs_conf.c,v 1.24 1998/04/20 03:57:30 julian Exp $
 */

/*
 * PURPOSE:	This file abstracts the root mounting interface from
 *		the per file system semantics for handling mounts,
 *		the overall intent of which is to move the BSD
 *		internals dependence out of the FS code, both to
 *		make the FS code more portable and to free up some
 *		of the BSD internals so that they may more easily
 *		be changed.
 *
 * NOTE1:	Code is single entry/single exit to aid debugging
 *		and conversion for kernel multithreading.
 *
 * NOTE2:	Code notes lock state in headers on entry and exit
 *		as an aid to conversion for kernel multithreading
 *		on SMP reentrancy
 */
#include "opt_devfs.h" /* for SLICE */
#include "opt_bootp.h"

#include <sys/param.h>		/* dev_t (types.h)*/
#include <sys/kernel.h>
#include <sys/systm.h>		/* rootvp*/
#include <sys/proc.h>		/* curproc*/
#include <sys/vnode.h>		/* NULLVP*/
#include <sys/mount.h>		/* struct mount*/
#include <sys/malloc.h>		/* M_MOUNT*/

/*
 * GLOBALS
 */

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount struct");

/*
 *  These define the root filesystem, device, and root filesystem type.
 */
dev_t rootdevs[] = { NODEV, NODEV };
char *rootdevnames[2];
struct vnode *rootvnode;
char *mountrootfsname;
#ifdef SLICE
char	rootdevice[32];
#endif
#ifdef BOOTP
extern void bootpc_init __P((void));
#endif

/*
 * vfs_init() will set maxvfsconf
 * to the highest defined type number.
 */
int maxvfsconf;
struct vfsconf *vfsconf;

/*
 * Common root mount code shared by all filesystems
 */
#define ROOTNAME	"root_device"

/*
 * vfs_mountrootfs
 *
 * Common entry point for root mounts
 *
 * PARAMETERS:
 * 		NONE
 *
 * RETURNS:	0	Success
 *		!0	error number (errno.h)
 *
 * LOCK STATE:
 *		ENTRY
 *			<no locks held>
 *		EXIT
 *			<no locks held>
 *
 * NOTES:
 *		This code is currently supported only for use for
 *		the FFS file system type.  This is a matter of
 *		fixing the other file systems, not this code!
 */
static void
vfs_mountrootfs(void *unused)
{
	struct mount		*mp;
	int			i, err;
	struct proc		*p = curproc;	/* XXX */
	dev_t			orootdev;

#ifdef BOOTP
	bootpc_init();
#endif
	/*
	 *  New root mount structure
	 */
	if ((err = vfs_rootmountalloc(mountrootfsname, ROOTNAME, &mp))) {
		printf("error %d: ", err);
		panic("cannot mount root\n");
		return ;
	}
	mp->mnt_flag		|= MNT_ROOTFS;

	/*
	 * Attempt the mount
	 */
	err = ENXIO;
	orootdev = rootdev;
	if (rootdevs[0] == NODEV)
		rootdevs[0] = rootdev;
	for (i = 0; i < sizeof(rootdevs) / sizeof(rootdevs[0]); i++) {
		if (rootdevs[i] == NODEV)
			break;
		rootdev = rootdevs[i];
		if (rootdev != orootdev) {
			printf("changing root device to %s\n", rootdevnames[i]);
			orootdev = rootdev;
		}
		strncpy(mp->mnt_stat.f_mntfromname,
		    rootdevnames[i] ? rootdevnames[i] : ROOTNAME, MNAMELEN - 1);
		err = VFS_MOUNT(mp, NULL, NULL, NULL, p);
		if (err != ENXIO)
			break;
	}
	if (err) {
		/*
		 * XXX should ask the user for the name in some cases.
		 * Why do we call vfs_unbusy() here and not after ENXIO
		 * is returned above?
		 */
		vfs_unbusy(mp, p);
		/*
		 * free mount struct before failing
		 * (hardly worthwhile with the PANIC eh?)
		 */
		free( mp, M_MOUNT);
		printf("error %d: ", err);
		panic("cannot mount root (2)\n");
		return;
	}

	simple_lock(&mountlist_slock);

	/*
	 * Add fs to list of mounted file systems
	 */
	CIRCLEQ_INSERT_HEAD(&mountlist, mp, mnt_list);

	simple_unlock(&mountlist_slock);
	vfs_unbusy(mp, p);

	/* root mount, update system time from FS specific data*/
	inittodr(mp->mnt_time);
	return;
}

SYSINIT(mountroot, SI_SUB_MOUNT_ROOT, SI_ORDER_FIRST, vfs_mountrootfs, NULL)

