/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vfs_conf.c	7.3 (Berkeley) 6/28/90
 *	$Id: vfs_conf.c,v 1.2 1993/10/16 15:25:21 rgrimes Exp $
 */

#include "param.h"
#include "mount.h"

/*
 * This specifies the filesystem used to mount the root.
 * This specification should be done by /etc/config.
 */
extern int ufs_mountroot();
int (*mountroot)() = ufs_mountroot;

/*
 * These define the root filesystem and device.
 */
struct mount *rootfs;
struct vnode *rootdir;

/*
 * Set up the filesystem operations for vnodes.
 * The types are defined in mount.h.
 */
extern	struct vfsops ufs_vfsops;

#ifdef NFS
extern	struct vfsops nfs_vfsops;
#endif

#ifdef MFS
extern	struct vfsops mfs_vfsops;
#endif

#ifdef PCFS
extern	struct vfsops pcfs_vfsops;
#endif

#ifdef ISOFS
extern	struct vfsops isofs_vfsops;
#endif

struct vfsops *vfssw[] = {
	(struct vfsops *)0,	/* 0 = MOUNT_NONE */
	&ufs_vfsops,		/* 1 = MOUNT_UFS */
#ifdef NFS
	&nfs_vfsops,		/* 2 = MOUNT_NFS */
#else
	(struct vfsops *)0,
#endif
#ifdef MFS
	&mfs_vfsops,		/* 3 = MOUNT_MFS */
#else
	(struct vfsops *)0,
#endif
#ifdef PCFS
	&pcfs_vfsops,		/* 4 = MOUNT_MSDOS */
#else
	(struct vfsops *)0,
#endif
#ifdef ISOFS
	&isofs_vfsops,		/* 5 = MOUNT_ISOFS */
#else
	(struct vfsops *)0,
#endif
};
