/*-
 * Copyright (c) 2001 Alexander Kabaev
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/gnu/fs/xfs/FreeBSD/xfs_mountops.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */
#ifndef	_XFS_XFS_H_
#define	_XFS_XFS_H_

#define	XFSFS_VMAJOR   0
#define	XFS_VMINOR     1
#define	XFS_VERSION    ((XFS_VMAJOR << 16) | XFS_VMINOR)
#define XFS_NAME       "xfs"

#ifdef _KERNEL

struct xfsmount {
	struct xfs_mount_args	m_args;		/* Mount parameters */
	struct mount *		m_mp;		/* Back pointer */
	xfs_vfs_t		m_vfs;		/* SHOULD BE FIRST */
};

#define XFSTOMNT(xmp)	((xmp)->m_mp)
#define XFSTOVFS(xmp)	(&(xmp)->m_vfs)

#define	MNTTOXFS(mp)	((struct xfsmount *)((mp)->mnt_data))
#define	MNTTOVFS(mp)	XFSTOVFS(MNTTOXFS(mp))

#define VFSTOMNT(vfsp)	(vfsp)->vfs_mp
#define VFSTOXFS(vfsp)	MNTTOXFS(VFSTOMNT(vfsp))

struct  xfsmount *xfsmount_allocate(struct mount *mp);
void	xfsmount_deallocate(struct xfsmount *xmp);

#endif	/* _KERNEL */

#endif	/* _XFS_XFS_H*/

