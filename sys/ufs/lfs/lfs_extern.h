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
 *	@(#)lfs_extern.h	8.6 (Berkeley) 5/8/95
 * $Id: lfs_extern.h,v 1.16 1997/10/10 18:17:20 phk Exp $
 */

#ifndef _UFS_LFS_LFS_EXTERN_H_
#define	_UFS_LFS_LFS_EXTERN_H_

#ifdef KERNEL

MALLOC_DECLARE(M_LFSNODE);
MALLOC_DECLARE(M_SEGMENT); /* XXX should be M_LFSSEGMENT ?? */

struct inode;
struct mount;
struct nameidata;

int	 lfs_balloc __P((struct vnode *, int, u_long, ufs_daddr_t, struct buf **));
int	 lfs_blkatoff __P((struct vop_blkatoff_args *));
int	 lfs_bwrite __P((struct vop_bwrite_args *));
int	 lfs_check __P((struct vnode *, ufs_daddr_t));
void	 lfs_free_buffer __P((caddr_t, int));
int	 lfs_gatherblock __P((struct segment *, struct buf *, int *));
struct dinode *
	 lfs_ifind __P((struct lfs *, ino_t, struct dinode *));
int	 lfs_init __P((struct vfsconf *));
int	 lfs_initseg __P((struct lfs *));
int	 lfs_makeinode __P((int, struct nameidata *, struct inode **));
int	 lfs_mountroot __P((void));
struct buf *
	 lfs_newbuf __P((struct vnode *, ufs_daddr_t, size_t));
void	 lfs_seglock __P((struct lfs *, unsigned long flags));
void	 lfs_segunlock __P((struct lfs *));
int	 lfs_segwrite __P((struct mount *, int));
#define	 lfs_sysctl ((int (*) __P((int *, u_int, void *, size_t *, void *, \
                                    size_t, struct proc *)))eopnotsupp)
int	 lfs_truncate __P((struct vop_truncate_args *));
int	 lfs_update __P((struct vop_update_args *));
void	 lfs_updatemeta __P((struct segment *));
int	 lfs_valloc __P((struct vop_valloc_args *));
int	 lfs_vcreate __P((struct mount *, ino_t, struct vnode **));
int	 lfs_vfree __P((struct vop_vfree_args *));
int	 lfs_vflush __P((struct vnode *));
int	 lfs_vref __P((struct vnode *));
void	 lfs_vunref __P((struct vnode *));
int	 lfs_writeinode __P((struct lfs *, struct segment *, struct inode *));
int	 lfs_writeseg __P((struct lfs *, struct segment *));
void	 lfs_writesuper __P((struct lfs *));

extern int lfs_allclean_wakeup;
extern int lfs_mount_type;
extern int locked_queue_count;
extern vop_t **lfs_vnodeop_p;
extern vop_t **lfs_specop_p;
extern vop_t **lfs_fifoop_p;
#define LFS_FIFOOPS lfs_fifoop_p

#endif /* KERNEL */

__BEGIN_DECLS
u_long	 cksum __P((void *, size_t));	/* XXX */
__END_DECLS

#endif /* !_UFS_LFS_LFS_EXTERN_H_ */
