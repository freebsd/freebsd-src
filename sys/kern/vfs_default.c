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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <machine/limits.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_zone.h>

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
	{ &vop_advlock_desc,		(vop_t *) vop_einval },
	{ &vop_bwrite_desc,		(vop_t *) vop_stdbwrite },
	{ &vop_close_desc,		(vop_t *) vop_null },
	{ &vop_createvobject_desc,	(vop_t *) vop_stdcreatevobject },
	{ &vop_destroyvobject_desc,	(vop_t *) vop_stddestroyvobject },
	{ &vop_fsync_desc,		(vop_t *) vop_null },
	{ &vop_getvobject_desc,		(vop_t *) vop_stdgetvobject },
	{ &vop_inactive_desc,		(vop_t *) vop_stdinactive },
	{ &vop_ioctl_desc,		(vop_t *) vop_enotty },
	{ &vop_islocked_desc,		(vop_t *) vop_noislocked },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_lock_desc,		(vop_t *) vop_nolock },
	{ &vop_mmap_desc,		(vop_t *) vop_einval },
	{ &vop_open_desc,		(vop_t *) vop_null },
	{ &vop_pathconf_desc,		(vop_t *) vop_einval },
	{ &vop_poll_desc,		(vop_t *) vop_nopoll },
	{ &vop_readlink_desc,		(vop_t *) vop_einval },
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

int
vop_panic(struct vop_generic_args *ap)
{

	printf("vop_panic[%s]\n", ap->a_desc->vdesc_name);
	panic("Filesystem goof");
	return (0);
}

/*
 *	vop_nostrategy:
 *
 *	Strategy routine for VFS devices that have none.
 *
 *	BIO_ERROR and B_INVAL must be cleared prior to calling any strategy
 *	routine.  Typically this is done for a BIO_READ strategy call.
 *	Typically B_INVAL is assumed to already be clear prior to a write 
 *	and should not be cleared manually unless you just made the buffer
 *	invalid.  BIO_ERROR should be cleared either way.
 */

static int
vop_nostrategy (struct vop_strategy_args *ap)
{
	printf("No strategy for buffer at %p\n", ap->a_bp);
	vprint("", ap->a_vp);
	vprint("", ap->a_bp->b_vp);
	ap->a_bp->b_ioflags |= BIO_ERROR;
	ap->a_bp->b_error = EOPNOTSUPP;
	bufdone(ap->a_bp);
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
	struct lock *l;

	if ((l = (struct lock *)ap->a_vp->v_data) == NULL) {
		if (ap->a_flags & LK_INTERLOCK)
			simple_unlock(&ap->a_vp->v_interlock);
		return 0;
	}

#ifndef	DEBUG_LOCKS
	return (lockmgr(l, ap->a_flags, &ap->a_vp->v_interlock, ap->a_p));
#else
	return (debuglockmgr(l, ap->a_flags, &ap->a_vp->v_interlock, ap->a_p,
	    "vop_stdlock", ap->a_vp->filename, ap->a_vp->line));
#endif
}

int
vop_stdunlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct lock *l;

	if ((l = (struct lock *)ap->a_vp->v_data) == NULL) {
		if (ap->a_flags & LK_INTERLOCK)
			simple_unlock(&ap->a_vp->v_interlock);
		return 0;
	}

	return (lockmgr(l, ap->a_flags | LK_RELEASE, &ap->a_vp->v_interlock, 
	    ap->a_p));
}

int
vop_stdislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct lock *l;

	if ((l = (struct lock *)ap->a_vp->v_data) == NULL)
		return 0;

	return (lockstatus(l, ap->a_p));
}

int
vop_stdinactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Return true for select/poll.
 */
int
vop_nopoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	/*
	 * Return true for read/write.  If the user asked for something
	 * special, return POLLNVAL, so that clients have a way of
	 * determining reliably whether or not the extended
	 * functionality is present without hard-coding knowledge
	 * of specific filesystem implementations.
	 */
	if (ap->a_events & ~POLLSTANDARD)
		return (POLLNVAL);

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Implement poll for local filesystems that support it.
 */
int
vop_stdpoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	if ((ap->a_events & ~POLLSTANDARD) == 0)
		return (ap->a_events & (POLLRDNORM|POLLWRNORM));
	return (vn_pollrecord(ap->a_vp, ap->a_p, ap->a_events));
}

int
vop_stdbwrite(ap)
	struct vop_bwrite_args *ap;
{
	return (bwrite(ap->a_bp));
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 * A minimal shared lock is necessary to ensure that the underlying object
 * is not revoked while an operation is in progress. So, an active shared
 * count is maintained in an auxillary vnode lock structure.
 */
int
vop_sharedlock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	/*
	 * This code cannot be used until all the non-locking filesystems
	 * (notably NFS) are converted to properly lock and release nodes.
	 * Also, certain vnode operations change the locking state within
	 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
	 * and symlink). Ideally these operations should not change the
	 * lock state, but should be changed to let the caller of the
	 * function unlock them. Otherwise all intermediate vnode layers
	 * (such as union, umapfs, etc) must catch these functions to do
	 * the necessary locking at their layer. Note that the inactive
	 * and lookup operations also change their lock state, but this 
	 * cannot be avoided, so these two operations will always need
	 * to be handled in intermediate layers.
	 */
	struct vnode *vp = ap->a_vp;
	int vnflags, flags = ap->a_flags;

	if (vp->v_vnlock == NULL) {
		if ((flags & LK_TYPE_MASK) == LK_DRAIN)
			return (0);
		MALLOC(vp->v_vnlock, struct lock *, sizeof(struct lock),
		    M_VNODE, M_WAITOK);
		lockinit(vp->v_vnlock, PVFS, "vnlock", 0, LK_NOPAUSE);
	}
	switch (flags & LK_TYPE_MASK) {
	case LK_DRAIN:
		vnflags = LK_DRAIN;
		break;
	case LK_EXCLUSIVE:
#ifdef DEBUG_VFS_LOCKS
		/*
		 * Normally, we use shared locks here, but that confuses
		 * the locking assertions.
		 */
		vnflags = LK_EXCLUSIVE;
		break;
#endif
	case LK_SHARED:
		vnflags = LK_SHARED;
		break;
	case LK_UPGRADE:
	case LK_EXCLUPGRADE:
	case LK_DOWNGRADE:
		return (0);
	case LK_RELEASE:
	default:
		panic("vop_sharedlock: bad operation %d", flags & LK_TYPE_MASK);
	}
	if (flags & LK_INTERLOCK)
		vnflags |= LK_INTERLOCK;
#ifndef	DEBUG_LOCKS
	return (lockmgr(vp->v_vnlock, vnflags, &vp->v_interlock, ap->a_p));
#else
	return (debuglockmgr(vp->v_vnlock, vnflags, &vp->v_interlock, ap->a_p,
	    "vop_sharedlock", vp->filename, vp->line));
#endif
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 * A minimal shared lock is necessary to ensure that the underlying object
 * is not revoked while an operation is in progress. So, an active shared
 * count is maintained in an auxillary vnode lock structure.
 */
int
vop_nolock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
#ifdef notyet
	/*
	 * This code cannot be used until all the non-locking filesystems
	 * (notably NFS) are converted to properly lock and release nodes.
	 * Also, certain vnode operations change the locking state within
	 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
	 * and symlink). Ideally these operations should not change the
	 * lock state, but should be changed to let the caller of the
	 * function unlock them. Otherwise all intermediate vnode layers
	 * (such as union, umapfs, etc) must catch these functions to do
	 * the necessary locking at their layer. Note that the inactive
	 * and lookup operations also change their lock state, but this 
	 * cannot be avoided, so these two operations will always need
	 * to be handled in intermediate layers.
	 */
	struct vnode *vp = ap->a_vp;
	int vnflags, flags = ap->a_flags;

	if (vp->v_vnlock == NULL) {
		if ((flags & LK_TYPE_MASK) == LK_DRAIN)
			return (0);
		MALLOC(vp->v_vnlock, struct lock *, sizeof(struct lock),
		    M_VNODE, M_WAITOK);
		lockinit(vp->v_vnlock, PVFS, "vnlock", 0, LK_NOPAUSE);
	}
	switch (flags & LK_TYPE_MASK) {
	case LK_DRAIN:
		vnflags = LK_DRAIN;
		break;
	case LK_EXCLUSIVE:
	case LK_SHARED:
		vnflags = LK_SHARED;
		break;
	case LK_UPGRADE:
	case LK_EXCLUPGRADE:
	case LK_DOWNGRADE:
		return (0);
	case LK_RELEASE:
	default:
		panic("vop_nolock: bad operation %d", flags & LK_TYPE_MASK);
	}
	if (flags & LK_INTERLOCK)
		vnflags |= LK_INTERLOCK;
	return(lockmgr(vp->v_vnlock, vnflags, &vp->v_interlock, ap->a_p));
#else /* for now */
	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK)
		simple_unlock(&ap->a_vp->v_interlock);
	return (0);
#endif
}

/*
 * Do the inverse of vop_nolock, handling the interlock in a compatible way.
 */
int
vop_nounlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (vp->v_vnlock == NULL) {
		if (ap->a_flags & LK_INTERLOCK)
			simple_unlock(&ap->a_vp->v_interlock);
		return (0);
	}
	return (lockmgr(vp->v_vnlock, LK_RELEASE | ap->a_flags,
		&ap->a_vp->v_interlock, ap->a_p));
}

/*
 * Return whether or not the node is in use.
 */
int
vop_noislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (vp->v_vnlock == NULL)
		return (0);
	return (lockstatus(vp->v_vnlock, ap->a_p));
}

/*
 * Return our mount point, as we will take charge of the writes.
 */
int
vop_stdgetwritemount(ap)
	struct vop_getwritemount_args /* {
		struct vnode *a_vp;
		struct mount **a_mpp;
	} */ *ap;
{

	*(ap->a_mpp) = ap->a_vp->v_mount;
	return (0);
}

int
vop_stdcreatevobject(ap)
	struct vop_createvobject_args /* {
		struct vnode *vp;
		struct ucred *cred;
		struct proc *p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ucred *cred = ap->a_cred;
	struct proc *p = ap->a_p;
	struct vattr vat;
	vm_object_t object;
	int error = 0;

	if (!vn_isdisk(vp, NULL) && vn_canvmio(vp) == FALSE)
		return (0);

retry:
	if ((object = vp->v_object) == NULL) {
		if (vp->v_type == VREG || vp->v_type == VDIR) {
			if ((error = VOP_GETATTR(vp, &vat, cred, p)) != 0)
				goto retn;
			object = vnode_pager_alloc(vp, vat.va_size, 0, 0);
		} else if (devsw(vp->v_rdev) != NULL) {
			/*
			 * This simply allocates the biggest object possible
			 * for a disk vnode.  This should be fixed, but doesn't
			 * cause any problems (yet).
			 */
			object = vnode_pager_alloc(vp, IDX_TO_OFF(INT_MAX), 0, 0);
		} else {
			goto retn;
		}
		/*
		 * Dereference the reference we just created.  This assumes
		 * that the object is associated with the vp.
		 */
		object->ref_count--;
		vp->v_usecount--;
	} else {
		if (object->flags & OBJ_DEAD) {
			VOP_UNLOCK(vp, 0, p);
			tsleep(object, PVM, "vodead", 0);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			goto retry;
		}
	}

	KASSERT(vp->v_object != NULL, ("vfs_object_create: NULL object"));
	vp->v_flag |= VOBJBUF;

retn:
	return (error);
}

int
vop_stddestroyvobject(ap)
	struct vop_destroyvobject_args /* {
		struct vnode *vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	vm_object_t obj = vp->v_object;

	if (vp->v_object == NULL)
		return (0);

	if (obj->ref_count == 0) {
		/*
		 * vclean() may be called twice. The first time
		 * removes the primary reference to the object,
		 * the second time goes one further and is a
		 * special-case to terminate the object.
		 */
		vm_object_terminate(obj);
	} else {
		/*
		 * Woe to the process that tries to page now :-).
		 */
		vm_pager_deallocate(obj);
	}
	return (0);
}

int
vop_stdgetvobject(ap)
	struct vop_getvobject_args /* {
		struct vnode *vp;
		struct vm_object **objpp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vm_object **objpp = ap->a_objpp;

	if (objpp)
		*objpp = vp->v_object;
	return (vp->v_object ? 0 : EINVAL);
}

/* 
 * vfs default ops
 * used to fill the vfs fucntion table to get reasonable default return values.
 */
int 
vfs_stdmount (mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data; 
	struct nameidata *ndp;
	struct proc *p;
{
	return (0);
}

int	
vfs_stdunmount (mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	return (0);
}

int	
vfs_stdroot (mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

int	
vfs_stdstatfs (mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

int
vfs_stdvptofh (vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return (EOPNOTSUPP);
}

int	
vfs_stdstart (mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
}

int	
vfs_stdquotactl (mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

int	
vfs_stdsync (mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred; 
	struct proc *p;
{
	return (0);
}

int	
vfs_stdvget (mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

int	
vfs_stdfhtovp (mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

int 
vfs_stdcheckexp (mp, nam, extflagsp, credanonp)
	struct mount *mp;
	struct sockaddr *nam;
	int *extflagsp;
	struct ucred **credanonp;
{
	return (EOPNOTSUPP);
}

int
vfs_stdinit (vfsp) 
	struct vfsconf *vfsp;
{
	return (0);
}

int
vfs_stduninit (vfsp)
	struct vfsconf *vfsp;
{
	return(0);
}

int
vfs_stdextattrctl(mp, cmd, attrname, arg, p)
	struct mount *mp;
	int cmd;
	const char *attrname;
	caddr_t arg;
	struct proc *p;
{
	return(EOPNOTSUPP);
}

/* end of vfs default ops */
