/*-
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
 *	@(#)mfs_extern.h	8.4 (Berkeley) 3/30/95
 */

struct buf;
struct mount;
struct nameidata;
struct proc;
struct statfs;
struct ucred;
struct vnode;
struct vfsconf;

__BEGIN_DECLS
int	mfs_badop __P((void));
int	mfs_bmap __P((struct vop_bmap_args *));
int	mfs_close __P((struct vop_close_args *));
void	mfs_doio __P((struct buf *bp, caddr_t base));
int	mfs_inactive __P((struct vop_inactive_args *)); /* XXX */
int	mfs_reclaim __P((struct vop_reclaim_args *));
int	mfs_init __P((struct vfsconf *));
int	mfs_ioctl __P((struct vop_ioctl_args *));
int	mfs_mount __P((struct mount *mp,
	    char *path, caddr_t data, struct nameidata *ndp, struct proc *p));
int	mfs_open __P((struct vop_open_args *));
int	mfs_print __P((struct vop_print_args *)); /* XXX */
#define	mfs_revoke vop_revoke
int	mfs_start __P((struct mount *mp, int flags, struct proc *p));
int	mfs_statfs __P((struct mount *mp, struct statfs *sbp, struct proc *p));
int	mfs_strategy __P((struct vop_strategy_args *)); /* XXX */
__END_DECLS
