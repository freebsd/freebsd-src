/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ffs_extern.h	8.6 (Berkeley) 3/30/95
 * $FreeBSD$
 */

#ifndef	_UFS_IFS_EXTERN_H
#define	_UFS_IFS_EXTERN_H

/*
 * Sysctl values for the fast filesystem.
 */
#define	IFS_REALLOCBLKS		3	/* block reallocation enabled */
#define	IFS_ASYNCFREE		4	/* asynchronous block freeing enabled */
#define	IFS_MAXID		5	/* number of valid ffs ids */


/* Return vals from ifs_isinodealloc */
#define	IFS_INODE_ISALLOC	1
#define	IFS_INODE_NOALLOC	0
#define	IFS_INODE_EMPTYCG	-1

#define	IFS_NAMES { \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "doreallocblks", CTLTYPE_INT }, \
	{ "doasyncfree", CTLTYPE_INT }, \
}

struct buf;
struct fid;
struct fs;
struct inode;
struct malloc_type;
struct mount;
struct proc;
struct sockaddr;
struct statfs;
struct ucred;
struct vnode;
struct vop_balloc_args;
struct vop_bmap_args;
struct vop_fsync_args;
struct vop_reallocblks_args;

extern vop_t **ifs_vnodeop_p;
extern vop_t **ifs_specop_p;
extern vop_t **ifs_fifoop_p;

int	ifs_lookup(struct vop_lookup_args *);
int	ifs_isinodealloc(struct inode *, ufs_daddr_t);

#endif /* !_UFS_IFS_EXTERN_H */
