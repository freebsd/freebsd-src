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
 *	@(#)ufs_extern.h	8.3 (Berkeley) 4/16/94
 * $FreeBSD$
 */

#ifndef _UFS_UFS_EXTERN_H_
#define	_UFS_UFS_EXTERN_H_

struct buf;
struct direct;
struct fid;
struct flock;
struct inode;
struct mbuf;
struct mount;
struct nameidata;
struct proc;
struct ucred;
struct uio;
struct vattr;
struct vnode;
struct ufs_args;

int	 ufs_abortop __P((struct vop_abortop_args *));
int	 ufs_access __P((struct vop_access_args *));
int	 ufs_advlock __P((struct vop_advlock_args *));
int	 ufs_bmap __P((struct vop_bmap_args *));
int	 ufs_bmaparray __P((struct vnode *, daddr_t, daddr_t *, struct indir *,
		int *, int *, int *));
int	 ufs_check_export __P((struct mount *, struct ufid *, struct mbuf *,
		struct vnode **, int *exflagsp, struct ucred **));
int	 ufs_checkpath __P((struct inode *, struct inode *, struct ucred *));
int	 ufs_close __P((struct vop_close_args *));
int	 ufs_create __P((struct vop_create_args *));
void	 ufs_dirbad __P((struct inode *, doff_t, char *));
int	 ufs_dirbadentry __P((struct vnode *, struct direct *, int));
int	 ufs_dirempty __P((struct inode *, ino_t, struct ucred *));
int	 ufs_direnter __P((struct inode *, struct vnode *,struct componentname *));
int	 ufs_dirremove __P((struct vnode *, struct componentname*));
int	 ufs_dirrewrite
	    __P((struct inode *, struct inode *, struct componentname *));
int	 ufs_getattr __P((struct vop_getattr_args *));
int	 ufs_getlbns __P((struct vnode *, daddr_t, struct indir *, int *));
struct vnode *
	 ufs_ihashget __P((dev_t, ino_t));
void	 ufs_ihashinit __P((void));
void	 ufs_ihashins __P((struct inode *));
struct vnode *
	 ufs_ihashlookup __P((dev_t, ino_t));
void	 ufs_ihashrem __P((struct inode *));
int	 ufs_inactive __P((struct vop_inactive_args *));
int	 ufs_init __P((void));
int	 ufs_ioctl __P((struct vop_ioctl_args *));
int	 ufs_islocked __P((struct vop_islocked_args *));
int	 ufs_link __P((struct vop_link_args *));
int	 ufs_lock __P((struct vop_lock_args *));
int	 ufs_lookup __P((struct vop_lookup_args *));
int	 ufs_makeinode __P((int mode, struct vnode *, struct vnode **, struct componentname *));
int	 ufs_mkdir __P((struct vop_mkdir_args *));
int	 ufs_mknod __P((struct vop_mknod_args *));
int	 ufs_mmap __P((struct vop_mmap_args *));
int	 ufs_open __P((struct vop_open_args *));
int	 ufs_pathconf __P((struct vop_pathconf_args *));
int	 ufs_print __P((struct vop_print_args *));
int	 ufs_readdir __P((struct vop_readdir_args *));
int	 ufs_readlink __P((struct vop_readlink_args *));
int	 ufs_reclaim __P((struct vop_reclaim_args *));
int	 ufs_remove __P((struct vop_remove_args *));
int	 ufs_rename __P((struct vop_rename_args *));
int	 ufs_rmdir __P((struct vop_rmdir_args *));
int	 ufs_root __P((struct mount *, struct vnode **));
int	 ufs_seek __P((struct vop_seek_args *));
int	 ufs_select __P((struct vop_select_args *));
int	 ufs_setattr __P((struct vop_setattr_args *));
int	 ufs_start __P((struct mount *, int, struct proc *));
int	 ufs_strategy __P((struct vop_strategy_args *));
int	 ufs_symlink __P((struct vop_symlink_args *));
int	 ufs_unlock __P((struct vop_unlock_args *));
int	 ufs_vinit __P((struct mount *, vop_t **, vop_t **, struct vnode **));
int	 ufsspec_close __P((struct vop_close_args *));
int	 ufsspec_read __P((struct vop_read_args *));
int	 ufsspec_write __P((struct vop_write_args *));

int	ufsfifo_read __P((struct vop_read_args *));
int	ufsfifo_write __P((struct vop_write_args *));
int	ufsfifo_close __P((struct vop_close_args *));

#endif /* !_UFS_UFS_EXTERN_H_ */
