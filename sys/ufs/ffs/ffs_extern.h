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
 * $Id: ffs_extern.h,v 1.7 1995/09/06 05:41:16 dyson Exp $
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

__BEGIN_DECLS
int	ffs_alloc __P((struct inode *,
	    daddr_t, daddr_t, int, struct ucred *, daddr_t *));
int	ffs_balloc __P((struct inode *,
	    daddr_t, int, struct ucred *, struct buf **, int));
int	ffs_blkatoff __P((struct vop_blkatoff_args *));
void	ffs_blkfree __P((struct inode *, daddr_t, long));
daddr_t	ffs_blkpref __P((struct inode *, daddr_t, int, daddr_t *));
int	ffs_bmap __P((struct vop_bmap_args *));
void	ffs_clrblock __P((struct fs *, u_char *, daddr_t));
int	ffs_fhtovp __P((struct mount *, struct fid *, struct mbuf *,
	    struct vnode **, int *, struct ucred **));
int	ffs_flushfiles __P((struct mount *, int, struct proc *));
void	ffs_fragacct __P((struct fs *, int, long [], int));
int	ffs_fsync __P((struct vop_fsync_args *));
int	ffs_init __P((void));
int	ffs_isblock __P((struct fs *, u_char *, daddr_t));
int	ffs_mount __P((struct mount *,
	    char *, caddr_t, struct nameidata *, struct proc *));
int	ffs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int	ffs_mountroot __P((void));
int	ffs_read __P((struct vop_read_args *));
int	ffs_reallocblks __P((struct vop_reallocblks_args *));
int	ffs_realloccg __P((struct inode *,
	    daddr_t, daddr_t, int, int, struct ucred *, struct buf **));
int	ffs_reclaim __P((struct vop_reclaim_args *));
void	ffs_setblock __P((struct fs *, u_char *, daddr_t));
int	ffs_statfs __P((struct mount *, struct statfs *, struct proc *));
int	ffs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int	ffs_truncate __P((struct vop_truncate_args *));
int	ffs_unmount __P((struct mount *, int, struct proc *));
int	ffs_update __P((struct vop_update_args *));
int	ffs_valloc __P((struct vop_valloc_args *));
int	ffs_vfree __P((struct vop_vfree_args *));
int	ffs_vget __P((struct mount *, ino_t, struct vnode **));
int	ffs_vptofh __P((struct vnode *, struct fid *));
int	ffs_write __P((struct vop_write_args *));
int	ffs_getpages __P((struct vop_getpages_args *));

#ifdef DIAGNOSTIC
void	ffs_checkoverlap __P((struct buf *, struct inode *));
#endif
__END_DECLS

extern vop_t **ffs_vnodeop_p;
extern vop_t **ffs_specop_p;
extern vop_t **ffs_fifoop_p;
#define FFS_FIFOOPS ffs_fifoop_p
