/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	From:	@(#)nfsnode.h	7.12 (Berkeley) 4/16/91
 *	$Id: nfsnode.h,v 1.3 1993/10/20 07:31:16 davidg Exp $
 */

#ifndef __h_nfsnode
#define __h_nfsnode 1

/*
 * The nfsnode is the nfs equivalent to ufs's inode. Any similarity
 * is purely coincidental.
 * There is a unique nfsnode allocated for each active file,
 * each current directory, each mounted-on file, text file, and the root.
 * An nfsnode is 'named' by its file handle. (nget/nfs_node.c)
 */

struct nfsnode {
	struct	nfsnode *n_chain[2];	/* must be first */
	nfsv2fh_t n_fh;			/* NFS File Handle */
	long	n_flag;			/* Flag for locking.. */
	struct	vnode *n_vnode;	/* vnode associated with this nfsnode */
	time_t	n_attrstamp;	/* Time stamp (sec) for attributes */
	struct	vattr n_vattr;	/* Vnode attribute cache */
	struct	sillyrename *n_sillyrename;	/* Ptr to silly rename struct */
	u_long	n_size;		/* Current size of file */
	struct lockf *n_lockf; 	/* Locking record of file */
	time_t	n_mtime;	/* Prev modify time to maintain data cache consistency*/
	time_t	n_ctime;	/* Prev create time for name cache consistency*/
	int	n_error;	/* Save write error value */
	pid_t	n_lockholder;	/* holder of nfsnode lock */
	pid_t	n_lockwaiter;	/* most recent waiter for nfsnode lock */
	u_long	n_direofoffset;	/* Dir. EOF offset cache */
};

#define	n_forw		n_chain[0]
#define	n_back		n_chain[1]

#ifdef KERNEL
/*
 * Convert between nfsnode pointers and vnode pointers
 */
#define VTONFS(vp)	((struct nfsnode *)(vp)->v_data)
#define NFSTOV(np)	((struct vnode *)(np)->n_vnode)
#endif
/*
 * Flags for n_flag
 */
#define	NLOCKED		0x1	/* Lock the node for other local accesses */
#define	NWANT		0x2	/* Want above lock */
#define	NMODIFIED	0x4	/* Might have a modified buffer in bio */
#define	NWRITEERR	0x8	/* Flag write errors so close will know */

/*
 * Prototypes for NFS vnode operations
 */
int	nfs_lookup __P((
		struct vnode *vp,
		struct nameidata *ndp,
		struct proc *p));
int	nfs_create __P((
		struct nameidata *ndp,
		struct vattr *vap,
		struct proc *p));
int	nfs_mknod __P((
		struct nameidata *ndp,
		struct vattr *vap,
		struct ucred *cred,
		struct proc *p));
int	nfs_open __P((
		struct vnode *vp,
		int mode,
		struct ucred *cred,
		struct proc *p));
int	nfs_close __P((
		struct vnode *vp,
		int fflag,
		struct ucred *cred,
		struct proc *p));
int	nfs_access __P((
		struct vnode *vp,
		int mode,
		struct ucred *cred,
		struct proc *p));
int	nfs_getattr __P((
		struct vnode *vp,
		struct vattr *vap,
		struct ucred *cred,
		struct proc *p));
int	nfs_setattr __P((
		struct vnode *vp,
		struct vattr *vap,
		struct ucred *cred,
		struct proc *p));
int	nfs_read __P((
		struct vnode *vp,
		struct uio *uio,
		int ioflag,
		struct ucred *cred));
int	nfs_write __P((
		struct vnode *vp,
		struct uio *uio,
		int ioflag,
		struct ucred *cred));
#define nfs_ioctl ((int (*) __P(( \
		struct vnode *vp, \
		int command, \
		caddr_t data, \
		int fflag, \
		struct ucred *cred, \
		struct proc *p))) enoioctl)
#define nfs_select ((int (*) __P(( \
		struct vnode *vp, \
		int which, \
		int fflags, \
		struct ucred *cred, \
		struct proc *p))) seltrue)
int	nfs_mmap __P((
		struct vnode *vp,
		int fflags,
		struct ucred *cred,
		struct proc *p));
int	nfs_fsync __P((
		struct vnode *vp,
		int fflags,
		struct ucred *cred,
		int waitfor,
		struct proc *p));
#define nfs_seek ((int (*) __P(( \
		struct vnode *vp, \
		off_t oldoff, \
		off_t newoff, \
		struct ucred *cred))) nullop)
int	nfs_remove __P((
		struct nameidata *ndp,
		struct proc *p));
int	nfs_link __P((
		struct vnode *vp,
		struct nameidata *ndp,
		struct proc *p));
int	nfs_rename __P((
		struct nameidata *fndp,
		struct nameidata *tdnp,
		struct proc *p));
int	nfs_mkdir __P((
		struct nameidata *ndp,
		struct vattr *vap,
		struct proc *p));
int	nfs_rmdir __P((
		struct nameidata *ndp,
		struct proc *p));
int	nfs_symlink __P((
		struct nameidata *ndp,
		struct vattr *vap,
		char *target,
		struct proc *p));
int	nfs_readdir __P((
		struct vnode *vp,
		struct uio *uio,
		struct ucred *cred,
		int *eofflagp));
int	nfs_readlink __P((
		struct vnode *vp,
		struct uio *uio,
		struct ucred *cred));
int	nfs_abortop __P((
		struct nameidata *ndp));
int	nfs_inactive __P((
		struct vnode *vp,
		struct proc *p));
int	nfs_reclaim __P((
		struct vnode *vp));
int	nfs_lock __P((
		struct vnode *vp));
int	nfs_unlock __P((
		struct vnode *vp));
int	nfs_bmap __P((
		struct vnode *vp,
		daddr_t bn,
		struct vnode **vpp,
		daddr_t *bnp));
int	nfs_strategy __P((
		struct buf *bp));
int	nfs_print __P((
		struct vnode *vp));
int	nfs_islocked __P((
		struct vnode *vp));
int	nfs_advlock __P((
		struct vnode *vp,
		caddr_t id,
		int op,
		struct flock *fl,
		int flags));

void	nfs_nput __P((struct vnode *));
#endif /* __h_nfsnode */
