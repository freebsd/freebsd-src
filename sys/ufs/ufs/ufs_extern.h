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
struct thread;
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

int	ufs_vnoperate(struct vop_generic_args *);
int	ufs_vnoperatefifo(struct vop_generic_args *);
int	ufs_vnoperatespec(struct vop_generic_args *);

int	 ufs_bmap(struct vop_bmap_args *);
int	 ufs_bmaparray(struct vnode *, ufs2_daddr_t, ufs2_daddr_t *,
	    struct buf *, int *, int *);
int	 ufs_fhtovp(struct mount *, struct ufid *, struct vnode **);
int	 ufs_checkpath(struct inode *, struct inode *, struct ucred *);
void	 ufs_dirbad(struct inode *, doff_t, char *);
int	 ufs_dirbadentry(struct vnode *, struct direct *, int);
int	 ufs_dirempty(struct inode *, ino_t, struct ucred *);
int	 ufs_extread(struct vop_read_args *);
int	 ufs_extwrite(struct vop_write_args *);
void	 ufs_makedirentry(struct inode *, struct componentname *,
	    struct direct *);
int	 ufs_direnter(struct vnode *, struct vnode *, struct direct *,
	    struct componentname *, struct buf *);
int	 ufs_dirremove(struct vnode *, struct inode *, int, int);
int	 ufs_dirrewrite(struct inode *, struct inode *, ino_t, int, int);
int	 ufs_getlbns(struct vnode *, ufs2_daddr_t, struct indir *, int *);
int	 ufs_ihashget(dev_t, ino_t, int, struct vnode **);
void	 ufs_ihashinit(void);
int	 ufs_ihashins(struct inode *, int, struct vnode **);
struct vnode *
	 ufs_ihashlookup(dev_t, ino_t);
void	 ufs_ihashrem(struct inode *);
void	 ufs_ihashuninit(void);
int	 ufs_inactive(struct vop_inactive_args *);
int	 ufs_init(struct vfsconf *);
void	 ufs_itimes(struct vnode *vp);
int	 ufs_lookup(struct vop_cachedlookup_args *);
int	 ufs_readdir(struct vop_readdir_args *);
int	 ufs_reclaim(struct vop_reclaim_args *);
void	 ffs_snapgone(struct inode *);
int	 ufs_root(struct mount *, struct vnode **);
int	 ufs_start(struct mount *, int, struct thread *);
int	 ufs_uninit(struct vfsconf *);
int	 ufs_vinit(struct mount *, vop_t **, vop_t **, struct vnode **);

/*
 * Soft update function prototypes.
 */
int	softdep_setup_directory_add(struct buf *, struct inode *, off_t,
	    ino_t, struct buf *, int);
void	softdep_change_directoryentry_offset(struct inode *, caddr_t,
	    caddr_t, caddr_t, int);
void	softdep_setup_remove(struct buf *,struct inode *, struct inode *, int);
void	softdep_setup_directory_change(struct buf *, struct inode *,
	    struct inode *, ino_t, int);
void	softdep_change_linkcnt(struct inode *);
void	softdep_releasefile(struct inode *);
int	softdep_slowdown(struct vnode *);

/*
 * Flags to low-level allocation routines.
 * The low 16-bits are reserved for IO_ flags from vnode.h.
 */
#define BA_CLRBUF	0x00010000	/* Request alloced buffer be cleared. */
#define BA_METAONLY	0x00020000	/* Return indirect block buffer. */
#define BA_NOWAIT	0x00040000	/* Do not sleep to await lock. */

#endif /* !_UFS_UFS_EXTERN_H_ */
