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

#ifndef _UFS_FFS_EXTERN_H
#define	_UFS_FFS_EXTERN_H

struct buf;
struct cg;
struct fid;
struct fs;
struct inode;
struct malloc_type;
struct mount;
struct thread;
struct sockaddr;
struct statfs;
struct ucred;
struct vnode;
struct vop_balloc_args;
struct vop_fsync_args;
struct vop_reallocblks_args;
struct vop_copyonwrite_args;

int	ffs_alloc __P((struct inode *,
	    ufs_daddr_t, ufs_daddr_t, int, struct ucred *, ufs_daddr_t *));
int	ffs_balloc __P((struct vnode *a_vp, off_t a_startoffset, int a_size,
            struct ucred *a_cred, int a_flags, struct buf **a_bpp));
int	ffs_blkatoff __P((struct vnode *, off_t, char **, struct buf **));
void	ffs_blkfree __P((struct fs *, struct vnode *, ufs_daddr_t, long,
	    ino_t));
ufs_daddr_t ffs_blkpref __P((struct inode *, ufs_daddr_t, int, ufs_daddr_t *));
void	ffs_clrblock __P((struct fs *, u_char *, ufs_daddr_t));
void	ffs_clusteracct	__P((struct fs *, struct cg *, ufs_daddr_t, int));
int	ffs_fhtovp __P((struct mount *, struct fid *, struct vnode **));
int	ffs_flushfiles __P((struct mount *, int, struct thread *));
void	ffs_fragacct __P((struct fs *, int, int32_t [], int));
int	ffs_freefile __P((struct fs *, struct vnode *, ino_t, int ));
int	ffs_isblock __P((struct fs *, u_char *, ufs_daddr_t));
int	ffs_isfreeblock __P((struct fs *, unsigned char *, ufs_daddr_t));
int	ffs_mountfs __P((struct vnode *, struct mount *, struct thread *,
	     struct malloc_type *));
int	ffs_mountroot __P((void));
int	ffs_mount __P((struct mount *, char *, caddr_t, struct nameidata *,
	    struct thread *));
int	ffs_reallocblks __P((struct vop_reallocblks_args *));
int	ffs_realloccg __P((struct inode *,
	    ufs_daddr_t, ufs_daddr_t, int, int, struct ucred *, struct buf **));
void	ffs_setblock __P((struct fs *, u_char *, ufs_daddr_t));
int	ffs_snapblkfree __P((struct fs *, struct vnode *, ufs_daddr_t,
	    long, ino_t));
void	ffs_snapremove __P((struct vnode *vp));
int	ffs_snapshot __P((struct mount *mp, char *snapfile));
void	ffs_snapshot_mount __P((struct mount *mp));
void	ffs_snapshot_unmount __P((struct mount *mp));
int	ffs_statfs __P((struct mount *, struct statfs *, struct thread *));
int	ffs_sync __P((struct mount *, int, struct ucred *, struct thread *));
int	ffs_truncate __P((struct vnode *, off_t, int, struct ucred *, struct thread *));
int	ffs_unmount __P((struct mount *, int, struct thread *));
int	ffs_update __P((struct vnode *, int));
int	ffs_valloc __P((struct vnode *, int, struct ucred *, struct vnode **));

int	ffs_vfree __P((struct vnode *, ino_t, int));
int	ffs_vget __P((struct mount *, ino_t, struct vnode **));
int	ffs_vptofh __P((struct vnode *, struct fid *));

extern vop_t **ffs_vnodeop_p;
extern vop_t **ffs_specop_p;
extern vop_t **ffs_fifoop_p;

/*
 * Soft update function prototypes.
 */
void	softdep_initialize __P((void));
int	softdep_mount __P((struct vnode *, struct mount *, struct fs *,
	    struct ucred *));
int	softdep_flushworklist __P((struct mount *, int *, struct thread *));
int	softdep_flushfiles __P((struct mount *, int, struct thread *));
void	softdep_update_inodeblock __P((struct inode *, struct buf *, int));
void	softdep_load_inodeblock __P((struct inode *));
void	softdep_freefile __P((struct vnode *, ino_t, int));
int	softdep_request_cleanup __P((struct fs *, struct vnode *));
void	softdep_setup_freeblocks __P((struct inode *, off_t));
void	softdep_setup_inomapdep __P((struct buf *, struct inode *, ino_t));
void	softdep_setup_blkmapdep __P((struct buf *, struct fs *, ufs_daddr_t));
void	softdep_setup_allocdirect __P((struct inode *, ufs_lbn_t, ufs_daddr_t,
	    ufs_daddr_t, long, long, struct buf *));
void	softdep_setup_allocindir_meta __P((struct buf *, struct inode *,
	    struct buf *, int, ufs_daddr_t));
void	softdep_setup_allocindir_page __P((struct inode *, ufs_lbn_t,
	    struct buf *, int, ufs_daddr_t, ufs_daddr_t, struct buf *));
void	softdep_fsync_mountdev __P((struct vnode *));
int	softdep_sync_metadata __P((struct vop_fsync_args *));
/* XXX incorrectly moved to mount.h - should be indirect function */
#if 0
int	softdep_fsync __P((struct vnode *vp));
#endif

#endif /* !_UFS_FFS_EXTERN_H */
