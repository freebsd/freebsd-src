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
 *	@(#)ufs_extern.h	8.10 (Berkeley) 5/14/95
 * $FreeBSD$
 */

#ifndef _UFS_UFS_EXTERN_H_
#define	_UFS_UFS_EXTERN_H_

struct componentname;
struct direct;
struct indir;
struct inode;
struct mount;
struct netcred;
struct proc;
struct sockaddr;
struct ucred;
struct ufid;
struct vfsconf;
struct vnode;
struct vop_bmap_args;
struct vop_cachedlookup_args;
struct vop_generic_args;
struct vop_inactive_args;
struct vop_reclaim_args;

int	ufs_vnoperate __P((struct vop_generic_args *));
int	ufs_vnoperatefifo __P((struct vop_generic_args *));
int	ufs_vnoperatespec __P((struct vop_generic_args *));

int	 ufs_bmap __P((struct vop_bmap_args *));
int	 ufs_bmaparray __P((struct vnode *, daddr_t, daddr_t *,
		int *, int *));
int	 ufs_fhtovp(register struct mount *, struct ufid *, struct vnode **);
int	 ufs_checkpath __P((struct inode *, struct inode *, struct ucred *));
void	 ufs_dirbad __P((struct inode *, doff_t, char *));
int	 ufs_dirbadentry __P((struct vnode *, struct direct *, int));
int	 ufs_dirempty __P((struct inode *, ino_t, struct ucred *));
void	 ufs_makedirentry __P((struct inode *, struct componentname *,
	    struct direct *));
int	 ufs_direnter __P((struct vnode *, struct vnode *, struct direct *,
	    struct componentname *, struct buf *));
int	 ufs_dirremove __P((struct vnode *, struct inode *, int, int));
int	 ufs_dirrewrite __P((struct inode *, struct inode *, ino_t, int, int));
int	 ufs_getlbns __P((struct vnode *, ufs_daddr_t, struct indir *, int *));
struct vnode *
	 ufs_ihashget __P((dev_t, ino_t));
void	 ufs_ihashinit __P((void));
void	 ufs_ihashins __P((struct inode *));
struct vnode *
	 ufs_ihashlookup __P((dev_t, ino_t));
void	 ufs_ihashrem __P((struct inode *));
int	 ufs_inactive __P((struct vop_inactive_args *));
int	 ufs_init __P((struct vfsconf *));
void	 ufs_itimes __P((struct vnode *vp));
int	 ufs_lookup __P((struct vop_cachedlookup_args *));
int	 ufs_readdir __P((struct vop_readdir_args *));
int	 ufs_reclaim __P((struct vop_reclaim_args *));
void	 ffs_snapgone __P((struct inode *));
int	 ufs_root __P((struct mount *, struct vnode **));
int	 ufs_start __P((struct mount *, int, struct proc *));
int	 ufs_vinit __P((struct mount *, vop_t **, vop_t **, struct vnode **));

/*
 * Soft update function prototypes.
 */
void	softdep_setup_directory_add __P((struct buf *, struct inode *, off_t,
	    long, struct buf *));
void	softdep_change_directoryentry_offset __P((struct inode *, caddr_t,
	    caddr_t, caddr_t, int));
void	softdep_setup_remove __P((struct buf *,struct inode *, struct inode *,
	    int));
void	softdep_setup_directory_change __P((struct buf *, struct inode *,
	    struct inode *, long, int));
void	softdep_change_linkcnt __P((struct inode *));
int	softdep_slowdown __P((struct vnode *));

#endif /* !_UFS_UFS_EXTERN_H_ */
