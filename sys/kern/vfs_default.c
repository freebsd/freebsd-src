/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

static int vop_nostrategy __P((struct vop_strategy_args *));

/*
 * This vnode table stores what we want to do if the filesystem doesn't
 * implement a particular VOP.
 *
 * If there is no specific entry here, we will return EOPNOTSUPP.
 *
 */

vop_t **default_vnodeop_p;
static struct vnodeopv_entry_desc default_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_eopnotsupp },
	{ &vop_abortop_desc,		(vop_t *) nullop },
	{ &vop_advlock_desc,		(vop_t *) vop_einval },
	{ &vop_bwrite_desc,		(vop_t *) vn_bwrite },
	{ &vop_close_desc,		(vop_t *) vop_null },
	{ &vop_fsync_desc,		(vop_t *) vop_null },
	{ &vop_ioctl_desc,		(vop_t *) vop_enotty },
	{ &vop_islocked_desc,		(vop_t *) vop_noislocked },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_lock_desc,		(vop_t *) vop_nolock },
	{ &vop_mmap_desc,		(vop_t *) vop_einval },
	{ &vop_open_desc,		(vop_t *) vop_null },
	{ &vop_pathconf_desc,		(vop_t *) vop_einval },
	{ &vop_poll_desc,		(vop_t *) vop_nopoll },
	{ &vop_readlink_desc,		(vop_t *) vop_einval },
	{ &vop_reallocblks_desc,	(vop_t *) vop_eopnotsupp },
	{ &vop_revoke_desc,		(vop_t *) vop_revoke },
	{ &vop_strategy_desc,		(vop_t *) vop_nostrategy },
	{ &vop_unlock_desc,		(vop_t *) vop_nounlock },
	{ NULL, NULL }
};

static struct vnodeopv_desc default_vnodeop_opv_desc =
        { &default_vnodeop_p, default_vnodeop_entries };

VNODEOP_SET(default_vnodeop_opv_desc);

int
vop_eopnotsupp(struct vop_generic_args *ap)
{
	/*
	printf("vop_notsupp[%s]\n", ap->a_desc->vdesc_name);
	*/

	return (EOPNOTSUPP);
}

int
vop_ebadf(struct vop_generic_args *ap)
{

	return (EBADF);
}

int
vop_enotty(struct vop_generic_args *ap)
{

	return (ENOTTY);
}

int
vop_einval(struct vop_generic_args *ap)
{

	return (EINVAL);
}

int
vop_null(struct vop_generic_args *ap)
{

	return (0);
}

int
vop_defaultop(struct vop_generic_args *ap)
{

	return (VOCALL(default_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}

static int
vop_nostrategy (struct vop_strategy_args *ap)
{
	printf("No strategy for buffer at %p\n", ap->a_bp);
	vprint("", ap->a_bp->b_vp);
	ap->a_bp->b_flags |= B_ERROR;
	ap->a_bp->b_error = EOPNOTSUPP;
	biodone(ap->a_bp);
	return (EOPNOTSUPP);
}

int
vop_stdpathconf(ap)
	struct vop_pathconf_args /* {
	struct vnode *a_vp;
	int a_name;
	int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
		case _PC_LINK_MAX:
			*ap->a_retval = LINK_MAX;
			return (0);
		case _PC_MAX_CANON:
			*ap->a_retval = MAX_CANON;
			return (0);
		case _PC_MAX_INPUT:
			*ap->a_retval = MAX_INPUT;
			return (0);
		case _PC_PIPE_BUF:
			*ap->a_retval = PIPE_BUF;
			return (0);
		case _PC_CHOWN_RESTRICTED:
			*ap->a_retval = 1;
			return (0);
		case _PC_VDISABLE:
			*ap->a_retval = _POSIX_VDISABLE;
			return (0);
		default:
			return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Standard lock, unlock and islocked functions.
 *
 * These depend on the lock structure being the first element in the
 * inode, ie: vp->v_data points to the the lock!
 */
int
vop_stdlock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{               
	struct lock *l = (struct lock*)ap->a_vp->v_data;

	return (lockmgr(l, ap->a_flags, &ap->a_vp->v_interlock, ap->a_p));
}

int
vop_stdunlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct lock *l = (struct lock*)ap->a_vp->v_data;

	return (lockmgr(l, ap->a_flags | LK_RELEASE, &ap->a_vp->v_interlock, 
	    ap->a_p));
}

int
vop_stdislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct lock *l = (struct lock*)ap->a_vp->v_data;

	return (lockstatus(l));
}

