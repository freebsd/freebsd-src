/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
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
 *	@(#)ffs_extern.h	8.3 (Berkeley) 4/16/94
 */

struct buf;
struct fid;
struct fs;
struct inode;
struct mount;
struct nameidata;
struct proc;
struct statfs;
struct timeval;
struct ucred;
struct uio;
struct vnode;
struct mbuf;
struct dinode;
struct ext2_group_desc;
struct ext2_inode;

__BEGIN_DECLS
int	ext2_alloc __P((struct inode *,
	    daddr_t, daddr_t, int, struct ucred *, daddr_t *));
int	ext2_balloc __P((struct inode *,
	    daddr_t, int, struct ucred *, struct buf **, int));
int	ext2_blkatoff __P((struct vop_blkatoff_args *));
void	ext2_blkfree __P((struct inode *, daddr_t, long));
daddr_t	ext2_blkpref __P((struct inode *, daddr_t, int, daddr_t *, daddr_t));
int	ext2_bmap __P((struct vop_bmap_args *));
int	ext2_fhtovp __P((struct mount *, struct fid *, struct mbuf *,
	    struct vnode **, int *, struct ucred **));
int	ext2_fsync __P((struct vop_fsync_args *));
int	ext2_init __P((void));
int	ext2_mount __P((struct mount *,
	    char *, caddr_t, struct nameidata *, struct proc *));
int	ext2_mountfs __P((struct vnode *, struct mount *, struct proc *));
int	ext2_mountroot __P((void));
int	ext2_read __P((struct vop_read_args *));
int	ext2_reallocblks __P((struct vop_reallocblks_args *));
int	ext2_reclaim __P((struct vop_reclaim_args *));
void	ext2_setblock __P((struct ext2_sb_info *, u_char *, daddr_t));
int	ext2_statfs __P((struct mount *, struct statfs *, struct proc *));
int	ext2_sync __P((struct mount *, int, struct ucred *, struct proc *));
int	ext2_truncate __P((struct vop_truncate_args *));
int	ext2_unmount __P((struct mount *, int, struct proc *));
int	ext2_update __P((struct vop_update_args *));
int	ext2_valloc __P((struct vop_valloc_args *));
int	ext2_vfree __P((struct vop_vfree_args *));
int	ext2_vget __P((struct mount *, ino_t, struct vnode **));
int	ext2_vptofh __P((struct vnode *, struct fid *));
int	ext2_write __P((struct vop_write_args *));
int 	ext2_lookup __P((struct vop_lookup_args *));
int 	ext2_readdir __P((struct vop_readdir_args *));
void	ext2_print_dinode __P((struct dinode *));
void	ext2_print_inode __P((struct inode *));
int	ext2_direnter __P((struct inode *, 
		struct vnode *, struct componentname *));
int	ext2_dirremove __P((struct vnode *, struct componentname *));
int	ext2_dirrewrite __P((struct inode *,
		struct inode *, struct componentname *));
int	ext2_dirempty __P((struct inode *, ino_t, struct ucred *));
int	ext2_checkpath __P((struct inode *, struct inode *, struct ucred *));
struct  ext2_group_desc * get_group_desc __P((struct mount * , 
		unsigned int , struct buf ** ));
void	ext2_discard_prealloc __P((struct inode *));
int	ext2_inactive __P((struct vop_inactive_args *));
int 	ll_w_block __P((struct buf *, int ));
int	ext2_new_block __P ((struct mount * mp, unsigned long goal,
			    long * prealloc_count,
			    long * prealloc_block));
ino_t	ext2_new_inode __P ((const struct inode * dir, int mode));
void	ext2_free_blocks (struct mount * mp, unsigned long block,
		       unsigned long count);
void	ext2_free_inode (struct inode * inode);
int	ext2_flushfiles __P((struct mount *mp, int flags, struct proc *p));
int	ext2_reload __P((struct mount *mountp, struct ucred *cred,
			struct proc *p));
void	ext2_ei2di __P((struct ext2_inode *ei, struct dinode *di));
void	ext2_di2ei __P((struct dinode *di, struct ext2_inode *ei));
void	mark_buffer_dirty __P((struct buf *bh));

#if !defined(__FreeBSD__)
int	bwrite();		/* FFS needs a bwrite routine.  XXX */
#endif

/* this macros allows some of the ufs code to distinguish between
 * an EXT2 and a non-ext2(FFS/LFS) vnode.
 */
#define  IS_EXT2_VNODE(vp) (vp->v_mount->mnt_stat.f_type == MOUNT_EXT2FS)

#ifdef DIAGNOSTIC
void	ext2_checkoverlap __P((struct buf *, struct inode *));
#endif
__END_DECLS

extern vop_t **ext2_vnodeop_p;
extern vop_t **ext2_specop_p;
#ifdef FIFO
extern vop_t **ext2_fifoop_p;
#define EXT2_FIFOOPS ext2_fifoop_p
#else
#define EXT2_FIFOOPS NULL
#endif
