/*
 * Copyright (c) 1990, 1993
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
 *	@(#)specdev.h	8.6 (Berkeley) 5/21/95
 * $Id: specdev.h,v 1.11 1997/02/22 09:40:35 peter Exp $
 */

/*
 * This structure defines the information maintained about
 * special devices. It is allocated in checkalias and freed
 * in vgone.
 */
struct specinfo {
	struct	vnode **si_hashchain;
	struct	vnode *si_specnext;
	long	si_flags;
	dev_t	si_rdev;
};
/*
 * Exported shorthand
 */
#define v_rdev v_specinfo->si_rdev
#define v_hashchain v_specinfo->si_hashchain
#define v_specnext v_specinfo->si_specnext
#define v_specflags v_specinfo->si_flags

/*
 * Flags for specinfo
 */
#define	SI_MOUNTEDON	0x0001	/* block special device is mounted on */

/*
 * Special device management
 */
#define	SPECHSZ	64
#if	((SPECHSZ&(SPECHSZ-1)) == 0)
#define	SPECHASH(rdev)	(((rdev>>5)+(rdev))&(SPECHSZ-1))
#else
#define	SPECHASH(rdev)	(((unsigned)((rdev>>5)+(rdev)))%SPECHSZ)
#endif

extern	struct vnode *speclisth[SPECHSZ];

/*
 * Prototypes for special file operations on vnodes.
 */
extern	vop_t **spec_vnodeop_p;
struct	nameidata;
struct	componentname;
struct	ucred;
struct	flock;
struct	buf;
struct	uio;

int	spec_badop __P((void));
int	spec_lookup __P((struct vop_lookup_args *));
#define spec_create ((int (*) __P((struct  vop_create_args *)))spec_badop)
#define spec_mknod ((int (*) __P((struct  vop_mknod_args *)))spec_badop)
int	spec_open __P((struct vop_open_args *));
int	spec_close __P((struct vop_close_args *));
#define spec_access ((int (*) __P((struct  vop_access_args *)))spec_ebadf)
#define spec_setattr ((int (*) __P((struct  vop_setattr_args *)))spec_ebadf)
int	spec_read __P((struct vop_read_args *));
int	spec_write __P((struct vop_write_args *));
#define	spec_lease_check ((int (*) __P((struct  vop_lease_args *)))nullop)
int	spec_ioctl __P((struct vop_ioctl_args *));
int	spec_poll __P((struct vop_poll_args *));
#define	spec_revoke vop_revoke
#define spec_mmap ((int (*) __P((struct  vop_mmap_args *)))spec_badop)
int	spec_fsync __P((struct  vop_fsync_args *));
#define spec_seek ((int (*) __P((struct  vop_seek_args *)))spec_badop)
#define spec_remove ((int (*) __P((struct  vop_remove_args *)))spec_badop)
#define spec_link ((int (*) __P((struct  vop_link_args *)))spec_badop)
#define spec_rename ((int (*) __P((struct  vop_rename_args *)))spec_badop)
#define spec_mkdir ((int (*) __P((struct  vop_mkdir_args *)))spec_badop)
#define spec_rmdir ((int (*) __P((struct  vop_rmdir_args *)))spec_badop)
#define spec_symlink ((int (*) __P((struct  vop_symlink_args *)))spec_badop)
#define spec_readdir ((int (*) __P((struct  vop_readdir_args *)))spec_badop)
#define spec_readlink ((int (*) __P((struct  vop_readlink_args *)))spec_badop)
#define spec_abortop ((int (*) __P((struct  vop_abortop_args *)))spec_badop)
int	spec_inactive __P((struct  vop_inactive_args *));
#define spec_reclaim ((int (*) __P((struct  vop_reclaim_args *)))nullop)
#define spec_lock ((int (*) __P((struct  vop_lock_args *)))vop_nolock)
#define spec_unlock ((int (*) __P((struct  vop_unlock_args *)))vop_nounlock)
int	spec_bmap __P((struct vop_bmap_args *));
int	spec_strategy __P((struct vop_strategy_args *));
int	spec_print __P((struct vop_print_args *));
#define spec_islocked ((int(*) __P((struct vop_islocked_args *)))vop_noislocked)
int	spec_pathconf __P((struct vop_pathconf_args *));
int	spec_advlock __P((struct vop_advlock_args *));
int	spec_getpages __P((struct vop_getpages_args *));
#define spec_blkatoff ((int (*) __P((struct  vop_blkatoff_args *)))spec_badop)
#define spec_valloc ((int (*) __P((struct  vop_valloc_args *)))spec_badop)
#define spec_reallocblks \
	((int (*) __P((struct  vop_reallocblks_args *)))spec_badop)
#define spec_vfree ((int (*) __P((struct  vop_vfree_args *)))spec_badop)
#define spec_truncate ((int (*) __P((struct  vop_truncate_args *)))nullop)
#define spec_update ((int (*) __P((struct  vop_update_args *)))nullop)
#define spec_bwrite ((int (*) __P((struct  vop_bwrite_args *)))nullop)
