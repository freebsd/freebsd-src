/*
 * Copyright (c) 1991, 1993
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
 *	@(#)fifo.h	8.2 (Berkeley) 2/2/94
 * $Id: fifo.h,v 1.6 1995/11/09 08:15:25 bde Exp $
 */

extern vop_t **fifo_vnodeop_p;

/*
 * Prototypes for fifo operations on vnodes.
 */
int	fifo_badop __P((void));
int	fifo_ebadf __P((void));
int	fifo_printinfo __P((struct vnode *));
int	fifo_lookup __P((struct vop_lookup_args *));
#define fifo_create ((int (*) __P((struct  vop_create_args *)))fifo_badop)
#define fifo_mknod ((int (*) __P((struct  vop_mknod_args *)))fifo_badop)
int	fifo_open __P((struct vop_open_args *));
int	fifo_close __P((struct vop_close_args *));
#define fifo_access ((int (*) __P((struct  vop_access_args *)))fifo_ebadf)
#define fifo_getattr ((int (*) __P((struct  vop_getattr_args *)))fifo_ebadf)
#define fifo_setattr ((int (*) __P((struct  vop_setattr_args *)))fifo_ebadf)
int	fifo_read __P((struct vop_read_args *));
int	fifo_write __P((struct vop_write_args *));
int	fifo_ioctl __P((struct vop_ioctl_args *));
int	fifo_select __P((struct vop_select_args *));
#define fifo_mmap ((int (*) __P((struct  vop_mmap_args *)))fifo_badop)
#define fifo_fsync ((int (*) __P((struct  vop_fsync_args *)))nullop)
#define fifo_seek ((int (*) __P((struct  vop_seek_args *)))fifo_badop)
#define fifo_remove ((int (*) __P((struct  vop_remove_args *)))fifo_badop)
#define fifo_link ((int (*) __P((struct  vop_link_args *)))fifo_badop)
#define fifo_rename ((int (*) __P((struct  vop_rename_args *)))fifo_badop)
#define fifo_mkdir ((int (*) __P((struct  vop_mkdir_args *)))fifo_badop)
#define fifo_rmdir ((int (*) __P((struct  vop_rmdir_args *)))fifo_badop)
#define fifo_symlink ((int (*) __P((struct  vop_symlink_args *)))fifo_badop)
#define fifo_readdir ((int (*) __P((struct  vop_readdir_args *)))fifo_badop)
#define fifo_readlink ((int (*) __P((struct  vop_readlink_args *)))fifo_badop)
#define fifo_abortop ((int (*) __P((struct  vop_abortop_args *)))fifo_badop)
#define fifo_inactive ((int (*) __P((struct  vop_inactive_args *)))nullop)
#define fifo_reclaim ((int (*) __P((struct  vop_reclaim_args *)))nullop)
int	fifo_lock __P((struct vop_lock_args *));
int	fifo_unlock __P((struct vop_unlock_args *));
int	fifo_bmap __P((struct vop_bmap_args *));
#define fifo_strategy ((int (*) __P((struct  vop_strategy_args *)))fifo_badop)
int	fifo_print __P((struct vop_print_args *));
#define fifo_islocked ((int (*) __P((struct  vop_islocked_args *)))nullop)
int	fifo_pathconf __P((struct vop_pathconf_args *));
int	fifo_advlock __P((struct vop_advlock_args *));
#define fifo_blkatoff ((int (*) __P((struct  vop_blkatoff_args *)))fifo_badop)
#define fifo_valloc ((int (*) __P((struct  vop_valloc_args *)))fifo_badop)
#define fifo_reallocblks \
	((int (*) __P((struct  vop_reallocblks_args *)))fifo_badop)
#define fifo_vfree ((int (*) __P((struct  vop_vfree_args *)))fifo_badop)
#define fifo_truncate ((int (*) __P((struct  vop_truncate_args *)))nullop)
#define fifo_update ((int (*) __P((struct  vop_update_args *)))nullop)
#define fifo_bwrite ((int (*) __P((struct  vop_bwrite_args *)))nullop)
