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
struct vop_fsync_args;
struct vop_reallocblks_args;

int	ffs_alloc(struct inode *,
	    ufs2_daddr_t, ufs2_daddr_t, int, struct ucred *, ufs2_daddr_t *);
int	ffs_balloc_ufs1(struct vnode *a_vp, off_t a_startoffset, int a_size,
            struct ucred *a_cred, int a_flags, struct buf **a_bpp);
int	ffs_balloc_ufs2(struct vnode *a_vp, off_t a_startoffset, int a_size,
            struct ucred *a_cred, int a_flags, struct buf **a_bpp);
int	ffs_blkatoff(struct vnode *, off_t, char **, struct buf **);
void	ffs_blkfree(struct ufsmount *, struct fs *, struct vnode *,
	    ufs2_daddr_t, long, ino_t);
ufs2_daddr_t ffs_blkpref_ufs1(struct inode *, ufs_lbn_t, int, ufs1_daddr_t *);
ufs2_daddr_t ffs_blkpref_ufs2(struct inode *, ufs_lbn_t, int, ufs2_daddr_t *);
int	ffs_checkfreefile(struct fs *, struct vnode *, ino_t);
void	ffs_clrblock(struct fs *, u_char *, ufs1_daddr_t);
int	ffs_copyonwrite(struct vnode *, struct buf *);
vfs_fhtovp_t ffs_fhtovp;
int	ffs_flushfiles(struct mount *, int, struct thread *);
void	ffs_fragacct(struct fs *, int, int32_t [], int);
int	ffs_freefile(struct ufsmount *, struct fs *, struct vnode *, ino_t,
	    int);
int	ffs_isblock(struct fs *, u_char *, ufs1_daddr_t);
void	ffs_load_inode(struct buf *, struct inode *, struct fs *, ino_t);
int	ffs_mountroot(void);
int	ffs_reallocblks(struct vop_reallocblks_args *);
int	ffs_realloccg(struct inode *, ufs2_daddr_t, ufs2_daddr_t,
	    ufs2_daddr_t, int, int, struct ucred *, struct buf **);
void	ffs_setblock(struct fs *, u_char *, ufs1_daddr_t);
int	ffs_snapblkfree(struct fs *, struct vnode *, ufs2_daddr_t, long, ino_t);
void	ffs_snapremove(struct vnode *vp);
int	ffs_snapshot(struct mount *mp, char *snapfile);
void	ffs_snapshot_mount(struct mount *mp);
void	ffs_snapshot_unmount(struct mount *mp);
vfs_statfs_t ffs_statfs;
vfs_sync_t ffs_sync;
int	ffs_truncate(struct vnode *, off_t, int, struct ucred *, struct thread *);
vfs_unmount_t ffs_unmount;
int	ffs_update(struct vnode *, int);
int	ffs_valloc(struct vnode *, int, struct ucred *, struct vnode **);

int	ffs_vfree(struct vnode *, ino_t, int);
vfs_vget_t ffs_vget;
vfs_vptofh_t ffs_vptofh;

extern struct vop_vector ffs_vnodeops;
extern struct vop_vector ffs_fifoops;

/*
 * Soft update function prototypes.
 */
void	softdep_initialize(void);
void	softdep_uninitialize(void);
int	softdep_mount(struct vnode *, struct mount *, struct fs *,
	    struct ucred *);
int	softdep_flushworklist(struct mount *, int *, struct thread *);
int	softdep_flushfiles(struct mount *, int, struct thread *);
void	softdep_update_inodeblock(struct inode *, struct buf *, int);
void	softdep_load_inodeblock(struct inode *);
void	softdep_freefile(struct vnode *, ino_t, int);
int	softdep_request_cleanup(struct fs *, struct vnode *);
void	softdep_setup_freeblocks(struct inode *, off_t, int);
void	softdep_setup_inomapdep(struct buf *, struct inode *, ino_t);
void	softdep_setup_blkmapdep(struct buf *, struct fs *, ufs2_daddr_t);
void	softdep_setup_allocdirect(struct inode *, ufs_lbn_t, ufs2_daddr_t,
	    ufs2_daddr_t, long, long, struct buf *);
void	softdep_setup_allocext(struct inode *, ufs_lbn_t, ufs2_daddr_t,
	    ufs2_daddr_t, long, long, struct buf *);
void	softdep_setup_allocindir_meta(struct buf *, struct inode *,
	    struct buf *, int, ufs2_daddr_t);
void	softdep_setup_allocindir_page(struct inode *, ufs_lbn_t,
	    struct buf *, int, ufs2_daddr_t, ufs2_daddr_t, struct buf *);
void	softdep_fsync_mountdev(struct vnode *);
int	softdep_sync_metadata(struct vop_fsync_args *);
int	softdep_disk_prewrite(struct buf *bp);
/* XXX incorrectly moved to mount.h - should be indirect function */
#if 0
int	softdep_fsync(struct vnode *vp);
#endif

#endif /* !_UFS_FFS_EXTERN_H */
