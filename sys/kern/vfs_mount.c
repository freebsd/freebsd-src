/*
 * Copyright (c) 1989, 1993
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
 * $FreeBSD$
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
#include <sys/param.h>		/* dev_t (types.h)*/
#include <sys/systm.h>		/* rootvp*/
#include <sys/proc.h>		/* curproc*/
#include <sys/vnode.h>		/* NULLVP*/
#include <sys/mount.h>		/* struct mount*/
#include <sys/malloc.h>		/* M_MOUNT*/

/*
 * GLOBALS
 */
int (*mountroot) __P((void *));
struct vnode *rootvnode;
struct vfsops	*mountrootvfsops;


/*
 * Common root mount code shared by all filesystems
 */
#define ROOTDIR		"/"
#define ROOTNAME	"root_device"



/*
 * vfs_mountroot
 *
 * Common entry point for root mounts
 *
 * PARAMETERS:
 *		data	pointer to the vfs_ops for the FS type mounting
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
int
vfs_mountroot(data)
	void			*data;
{
	struct mount		*mp;
	u_int			size;
	int			err = 0;
	struct proc		*p = curproc;	/* XXX */
	struct vfsops		*mnt_op = (struct vfsops *)data;

	/*
	 *  New root mount structure
	 */
	mp = malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op		= mnt_op;
	mp->mnt_flag		= MNT_ROOTFS;
	mp->mnt_vnodecovered	= NULLVP;

	/*
	 * Lock mount point
	 */
	if( ( err = vfs_lock(mp)) != 0)
		goto error_1;

	/* Save "last mounted on" info for mount point (NULL pad)*/
	copystr(	ROOTDIR,			/* mount point*/
			mp->mnt_stat.f_mntonname,	/* save area*/
			MNAMELEN - 1,			/* max size*/
			&size);				/* real size*/
	bzero( mp->mnt_stat.f_mntonname + size, MNAMELEN - size);

	/* Save "mounted from" info for mount point (NULL pad)*/
	copystr(	ROOTNAME,			/* device name*/
			mp->mnt_stat.f_mntfromname,	/* save area*/
			MNAMELEN - 1,			/* max size*/
			&size);				/* real size*/
	bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

	/*
	 * Attempt the mount
	 */
	err = VFS_MOUNT( mp, NULL, NULL, NULL, p);
	if( err)
		goto error_2;

	/* Add fs to list of mounted file systems*/
	CIRCLEQ_INSERT_TAIL( &mountlist, mp, mnt_list);

	/* Unlock mount point*/
	vfs_unlock(mp);

	/* root mount, update system time from FS specific data*/
	inittodr( mp->mnt_time);

	goto success;


error_2:	/* mount error*/

	/* unlock before failing*/
	vfs_unlock( mp);

error_1:	/* lock error*/

	/* free mount struct before failing*/
	free( mp, M_MOUNT);

success:
	return( err);
}
