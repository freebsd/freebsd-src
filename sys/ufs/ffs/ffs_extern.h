/*	$NetBSD: ffs_extern.h,v 1.40 2004/06/04 07:43:56 he Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef _UFS_FFS_FFS_EXTERN_H_
#define _UFS_FFS_FFS_EXTERN_H_

/*
 * Sysctl values for the fast filesystem.
 */
#define FFS_CLUSTERREAD		1	/* cluster reading enabled */
#define FFS_CLUSTERWRITE	2	/* cluster writing enabled */
#define FFS_REALLOCBLKS		3	/* block reallocation enabled */
#define FFS_ASYNCFREE		4	/* asynchronous block freeing enabled */
#define FFS_LOG_CHANGEOPT	5	/* log optimalization strategy change */
#define FFS_MAXID		6	/* number of valid ffs ids */

#define FFS_NAMES { \
	{ 0, 0 }, \
	{ "doclusterread", CTLTYPE_INT }, \
	{ "doclusterwrite", CTLTYPE_INT }, \
	{ "doreallocblks", CTLTYPE_INT }, \
	{ "doasyncfree", CTLTYPE_INT }, \
	{ "log_changeopt", CTLTYPE_INT }, \
}

struct buf;
struct fid;
struct fs;
struct inode;
struct ufs1_dinode;
struct ufs2_dinode;
struct mount;
struct nameidata;
struct proc;
struct statvfs;
struct timeval;
struct timespec;
struct ucred;
struct ufsmount;
struct uio;
struct vnode;
struct mbuf;
struct cg;

extern struct pool ffs_inode_pool;	/* memory pool for inodes */
extern struct pool ffs_dinode1_pool;	/* memory pool for UFS1 dinodes */
extern struct pool ffs_dinode2_pool;	/* memory pool for UFS2 dinodes */

__BEGIN_DECLS

/* ffs_alloc.c */
int ffs_alloc __P((struct inode *, daddr_t, daddr_t , int, struct ucred *,
		   daddr_t *));
int ffs_realloccg __P((struct inode *, daddr_t, daddr_t, int, int ,
		       struct ucred *, struct buf **, daddr_t *));
int ffs_reallocblks __P((void *));
int ffs_valloc __P((void *));
daddr_t ffs_blkpref_ufs1 __P((struct inode *, daddr_t, int, int32_t *));
daddr_t ffs_blkpref_ufs2 __P((struct inode *, daddr_t, int, int64_t *));
void ffs_blkfree __P((struct fs *, struct vnode *, daddr_t, long, ino_t));
int ffs_vfree __P((void *));
void ffs_clusteracct __P((struct fs *, struct cg *, int32_t, int));
int ffs_checkfreefile __P((struct fs *, struct vnode *, ino_t));

/* ffs_balloc.c */
int ffs_balloc __P((void *));

/* ffs_bswap.c */
void ffs_sb_swap __P((struct fs*, struct fs *));
void ffs_dinode1_swap __P((struct ufs1_dinode *, struct ufs1_dinode *));
void ffs_dinode2_swap __P((struct ufs2_dinode *, struct ufs2_dinode *));
void ffs_csum_swap __P((struct csum *, struct csum *, int));
void ffs_csumtotal_swap __P((struct csum_total *, struct csum_total *));
void ffs_cg_swap __P((struct cg *, struct cg *, struct fs *));

/* ffs_inode.c */
int ffs_update __P((void *));
int ffs_truncate __P((void *));

/* ffs_subr.c */
void ffs_load_inode __P((struct buf *, struct inode *, struct fs *, ino_t));
int ffs_blkatoff __P((void *));
int ffs_freefile __P((void *));
void ffs_fragacct __P((struct fs *, int, int32_t[], int, int));
#ifdef DIAGNOSTIC
void	ffs_checkoverlap __P((struct buf *, struct inode *));
#endif
int ffs_isblock __P((struct fs *, u_char *, int32_t));
int ffs_isfreeblock __P((struct fs *, u_char *, int32_t));
void ffs_clrblock __P((struct fs *, u_char *, int32_t));
void ffs_setblock __P((struct fs *, u_char *, int32_t));

/* ffs_vfsops.c */
void ffs_init __P((void));
void ffs_reinit __P((void));
void ffs_done __P((void));
int ffs_mountroot __P((void));
int ffs_mount __P((struct mount *, const char *, void *, struct nameidata *,
		   struct proc *));
int ffs_reload __P((struct mount *, struct ucred *, struct proc *));
int ffs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int ffs_unmount __P((struct mount *, int, struct proc *));
int ffs_flushfiles __P((struct mount *, int, struct proc *));
int ffs_statvfs __P((struct mount *, struct statvfs *, struct proc *));
int ffs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int ffs_vget __P((struct mount *, ino_t, struct vnode **));
int ffs_fhtovp __P((struct mount *, struct fid *, struct vnode **));
int ffs_vptofh __P((struct vnode *, struct fid *));
int ffs_sbupdate __P((struct ufsmount *, int));
int ffs_cgupdate __P((struct ufsmount *, int));

/* ffs_appleufs.c */
u_int16_t ffs_appleufs_cksum __P((const struct appleufslabel *));
int ffs_appleufs_validate __P((const char*,const struct appleufslabel *,struct appleufslabel *));
void ffs_appleufs_set __P((struct appleufslabel *, const char *, time_t, uint64_t));


/* ffs_vnops.c */
int ffs_read __P((void *));
int ffs_write __P((void *));
int ffs_fsync __P((void *));
int ffs_reclaim __P((void *));
int ffs_getpages __P((void *));
int ffs_putpages __P((void *));
void ffs_gop_size __P((struct vnode *, off_t, off_t *, int));

#ifdef SYSCTL_SETUP_PROTO
SYSCTL_SETUP_PROTO(sysctl_vfs_ffs_setup);
#endif /* SYSCTL_SETUP_PROTO */

__END_DECLS

 
/*
 * Snapshot function prototypes.
 */
int	ffs_snapblkfree(struct fs *, struct vnode *, daddr_t, long, ino_t);
void	ffs_snapremove(struct vnode *);
int	ffs_snapshot(struct mount *, struct vnode *, struct timespec *);
void	ffs_snapshot_mount(struct mount *);
void	ffs_snapshot_unmount(struct mount *);
void	ffs_snapgone(struct inode *);

/*
 * Soft dependency function prototypes.
 */
void	softdep_initialize __P((void));
void	softdep_reinitialize __P((void));
int	softdep_mount __P((struct vnode *, struct mount *, struct fs *,
	    struct ucred *));
int	softdep_flushworklist __P((struct mount *, int *, struct proc *));
int	softdep_flushfiles __P((struct mount *, int, struct proc *));
void	softdep_update_inodeblock __P((struct inode *, struct buf *, int));
void	softdep_load_inodeblock __P((struct inode *));
void	softdep_freefile __P((void *));
void	softdep_setup_freeblocks __P((struct inode *, off_t, int));
void	softdep_setup_inomapdep __P((struct buf *, struct inode *, ino_t));
void	softdep_setup_blkmapdep __P((struct buf *, struct fs *, daddr_t));
void	softdep_setup_allocdirect __P((struct inode *, daddr_t, daddr_t,
	    daddr_t, long, long, struct buf *));
void	softdep_setup_allocindir_meta __P((struct buf *, struct inode *,
	    struct buf *, int, daddr_t));
void	softdep_setup_allocindir_page __P((struct inode *, daddr_t,
	    struct buf *, int, daddr_t, daddr_t, struct buf *));
void	softdep_fsync_mountdev __P((struct vnode *));
int	softdep_sync_metadata __P((void *));

extern int (**ffs_vnodeop_p) __P((void *));
extern int (**ffs_specop_p) __P((void *));
extern int (**ffs_fifoop_p) __P((void *));

#endif /* !_UFS_FFS_FFS_EXTERN_H_ */
