/*
 * Copyright (c) 1989, 1993
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
 *	@(#)dead_vnops.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/poll.h>

static int	chkvnlock __P((struct vnode *));
/*
 * Prototypes for dead operations on vnodes.
 */
static int	dead_badop __P((void));
static int	dead_bmap __P((struct vop_bmap_args *));
static int	dead_ioctl __P((struct vop_ioctl_args *));
static int	dead_lock __P((struct vop_lock_args *));
static int	dead_lookup __P((struct vop_lookup_args *));
static int	dead_open __P((struct vop_open_args *));
static int	dead_poll __P((struct vop_poll_args *));
static int	dead_print __P((struct vop_print_args *));
static int	dead_read __P((struct vop_read_args *));
static int	dead_write __P((struct vop_write_args *));

vop_t **dead_vnodeop_p;
static struct vnodeopv_entry_desc dead_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) vop_ebadf },
	{ &vop_advlock_desc,		(vop_t *) vop_ebadf },
	{ &vop_bmap_desc,		(vop_t *) dead_bmap },
	{ &vop_create_desc,		(vop_t *) dead_badop },
	{ &vop_getattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_inactive_desc,		(vop_t *) vop_null },
	{ &vop_ioctl_desc,		(vop_t *) dead_ioctl },
	{ &vop_link_desc,		(vop_t *) dead_badop },
	{ &vop_lock_desc,		(vop_t *) dead_lock },
	{ &vop_lookup_desc,		(vop_t *) dead_lookup },
	{ &vop_mkdir_desc,		(vop_t *) dead_badop },
	{ &vop_mknod_desc,		(vop_t *) dead_badop },
	{ &vop_mmap_desc,		(vop_t *) dead_badop },
	{ &vop_open_desc,		(vop_t *) dead_open },
	{ &vop_pathconf_desc,		(vop_t *) vop_ebadf },	/* per pathconf(2) */
	{ &vop_poll_desc,		(vop_t *) dead_poll },
	{ &vop_print_desc,		(vop_t *) dead_print },
	{ &vop_read_desc,		(vop_t *) dead_read },
	{ &vop_readdir_desc,		(vop_t *) vop_ebadf },
	{ &vop_readlink_desc,		(vop_t *) vop_ebadf },
	{ &vop_reclaim_desc,		(vop_t *) vop_null },
	{ &vop_remove_desc,		(vop_t *) dead_badop },
	{ &vop_rename_desc,		(vop_t *) dead_badop },
	{ &vop_rmdir_desc,		(vop_t *) dead_badop },
	{ &vop_setattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_symlink_desc,		(vop_t *) dead_badop },
	{ &vop_write_desc,		(vop_t *) dead_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc dead_vnodeop_opv_desc =
	{ &dead_vnodeop_p, dead_vnodeop_entries };

VNODEOP_SET(dead_vnodeop_opv_desc);

/*
 * Trivial lookup routine that always fails.
 */
/* ARGSUSED */
static int
dead_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open always fails as if device did not exist.
 */
/* ARGSUSED */
static int
dead_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (ENXIO);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
static int
dead_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{

	if (chkvnlock(ap->a_vp))
		panic("dead_read: lock");
	/*
	 * Return EOF for tty devices, EIO for others
	 */
	if ((ap->a_vp->v_flag & VISTTY) == 0)
		return (EIO);
	return (0);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
static int
dead_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{

	if (chkvnlock(ap->a_vp))
		panic("dead_write: lock");
	return (EIO);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
static int
dead_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	if (!chkvnlock(ap->a_vp))
		return (ENOTTY);
	return (VCALL(ap->a_vp, VOFFSET(vop_ioctl), ap));
}


/*
 * Wait until the vnode has finished changing state.
 */
static int
dead_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK) {
		simple_unlock(&vp->v_interlock);
		ap->a_flags &= ~LK_INTERLOCK;
	}
	if (!chkvnlock(vp))
		return (0);
	return (VCALL(vp, VOFFSET(vop_lock), ap));
}

/*
 * Wait until the vnode has finished changing state.
 */
static int
dead_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{

	if (!chkvnlock(ap->a_vp))
		return (EIO);
	return (VOP_BMAP(ap->a_vp, ap->a_bn, ap->a_vpp, ap->a_bnp, ap->a_runp, ap->a_runb));
}

/*
 * Print out the contents of a dead vnode.
 */
/* ARGSUSED */
static int
dead_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_NON, dead vnode\n");
	return (0);
}

/*
 * Empty vnode bad operation
 */
static int
dead_badop()
{

	panic("dead_badop called");
	/* NOTREACHED */
}

/*
 * We have to wait during times when the vnode is
 * in a state of change.
 */
int
chkvnlock(vp)
	register struct vnode *vp;
{
	int locked = 0;

	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		(void) tsleep((caddr_t)vp, PINOD, "ckvnlk", 0);
		locked = 1;
	}
	return (locked);
}

/*
 * Trivial poll routine that always returns POLLHUP.
 * This is necessary so that a process which is polling a file
 * gets notified when that file is revoke()d.
 */
static int
dead_poll(ap)
	struct vop_poll_args *ap;
{
	return (POLLHUP);
}
