/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>

#include <machine/limits.h>

#include "lomacfs.h"
#include "kernel_mediate.h"
#include "kernel_monitor.h"

#if defined(LOMAC_DEBUG_LOOKUPSTATS)
static unsigned int lomacfs_successful_lookups, lomacfs_failed_lookups,
    lomacfs_successful_cachedlookups, lomacfs_failed_cachedlookups,
    lomacfs_node_alloc_clashes, lomacfs_node_alloc_failures;

SYSCTL_NODE(_vfs, OID_AUTO, lomacfs, CTLFLAG_RW, 0, "LOMACFS filesystem");
SYSCTL_NODE(_vfs_lomacfs, OID_AUTO, debug, CTLFLAG_RW, 0, "debug stats");
SYSCTL_UINT(_vfs_lomacfs_debug, OID_AUTO, successful_lookups,
    CTLFLAG_RW, &lomacfs_successful_lookups, 0, "");
SYSCTL_UINT(_vfs_lomacfs_debug, OID_AUTO, failed_lookups,
    CTLFLAG_RW, &lomacfs_failed_lookups, 0, "");
SYSCTL_UINT(_vfs_lomacfs_debug, OID_AUTO, successful_cachedlookups,
    CTLFLAG_RW, &lomacfs_successful_cachedlookups, 0, "");
SYSCTL_UINT(_vfs_lomacfs_debug, OID_AUTO, failed_cachedlookups,
    CTLFLAG_RW, &lomacfs_failed_cachedlookups, 0, "");
SYSCTL_UINT(_vfs_lomacfs_debug, OID_AUTO, node_alloc_clashes,
    CTLFLAG_RW, &lomacfs_node_alloc_clashes, 0, "");
SYSCTL_UINT(_vfs_lomacfs_debug, OID_AUTO, node_alloc_failures,
    CTLFLAG_RW, &lomacfs_node_alloc_failures, 0, "");
#endif

static int
lomacfs_defaultop(
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
	} */ *ap
) {

	printf("lomacfs: %s unsupported\n", ap->a_desc->vdesc_name);
	return (EOPNOTSUPP);
}

static int
lomacfs_inactive(
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;
	struct vnode *lvp = VTOLVP(vp);
	struct thread *td = ap->a_td;

	KASSERT(lvp != NULL, ("inactive with NULL lowervp"));
	VOP_UNLOCK(ap->a_vp, 0, td);
	/*
	 * Temporarily drop our reference to the lower vnode, while keeping
	 * it held, to possibly call VOP_INACTIVE() on the lower layer.
	 */
	vrele(lvp);
#if defined(LOMAC_DEBUG_INACTIVE)
	do {
#if defined(LOMAC_DEBUG_INCNAME)
		const char *name = VTOLOMAC(vp)->ln_name;
#else
		const char *name = "[unknown]";
#endif
		printf("lomacfs: inactive(%p \"%s\"), lvp usecount down to %u\n",
		    vp, name, lvp->v_usecount);
	} while (0);
#endif
	/*
	 * Since the lower fs may actually remove the vnode on last
	 * release, destroy ourselves mostly here if that occurs.
	 *
	 * Additionally, devices should be totally freed
	 * on last close, not lazily.
	 */
	if (lvp->v_usecount == 0 &&
	    (lvp->v_type != VREG && lvp->v_type != VDIR)) {
		vdrop(lvp);
		VTOLVP(vp) = NULL;
		cache_purge(vp);
	} else
		vref(lvp);
	return (0);
}

static int
lomacfs_reclaim(
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;
	struct lomac_node *ln = VTOLOMAC(vp);
	struct vnode *lvp = VTOLVP(vp);

	if (lvp != NULL)
		vrele(lvp);
#if defined(LOMAC_DEBUG_RECLAIM)
	if (lvp != NULL) {
#if defined(LOMAC_DEBUG_INCNAME)
		const char *name = ln->ln_name;
#else
		const char *name = "[unknown]";
#endif
		printf("lomacfs: reclaim(%p \"%s\"), lvp usecount down to %u\n",
		    vp, name, lvp->v_usecount);
	}
#endif
	if (lvp != NULL)
		vdrop(lvp);
	vp->v_data = NULL;
	vp->v_rdev = NULL;
	free(ln, M_LOMACFS);

	return (0);
}

static int
lomacfs_print(
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;

	printf ("\ttag %s, vp=%p, lowervp=%p\n", vp->v_tag, vp,
	    VTOLVP(vp));
	return (0);
}

static int
lomacfs_lock(
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	struct thread *td = ap->a_td;
	struct vnode *lvp;
	int lflags = flags & ~(LK_INTERLOCK | LK_THISLAYER);
	int error;

	/*
	 * To prevent race conditions involving doing a lookup
	 * on "..", we have to lock the lower node, then lock our
	 * node. Most of the time it won't matter that we lock our
	 * node (as any locking would need the lower one locked
	 * first). But we can LK_DRAIN the upper lock as a step
	 * towards decomissioning it.
	 */
	lvp = VTOLVP(vp);
	if (lvp == NULL || flags & LK_THISLAYER)
		return (lockmgr(&vp->v_lock, flags, &vp->v_interlock, td));
	if (flags & LK_INTERLOCK) {
		mtx_unlock(&vp->v_interlock);
		flags &= ~LK_INTERLOCK;
	}
	if ((flags & LK_TYPE_MASK) == LK_DRAIN) {
		error = vn_lock(lvp,
			(lflags & ~LK_TYPE_MASK) | LK_EXCLUSIVE | LK_CANRECURSE,
			td);
	} else
		error = vn_lock(lvp, lflags | LK_CANRECURSE, td);
	if (error)
		return (error);	
	error = lockmgr(&vp->v_lock, flags, &vp->v_interlock, td);
	if (error)
		VOP_UNLOCK(lvp, 0, td);
	return (error);
}

/*
 * We need to process our own vnode unlock and then clear the
 * interlock flag as it applies only to our vnode, not the
 * vnodes below us on the stack.
 */
static int
lomacfs_unlock(
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	int lflags = (ap->a_flags | LK_RELEASE) &
	    ~(LK_THISLAYER | LK_INTERLOCK);
	struct thread *td = ap->a_td;
	struct vnode *lvp = VTOLVP(vp);
	int error;

	error = lockmgr(&vp->v_lock, flags | LK_RELEASE, &vp->v_interlock, td);
	if (lvp == NULL || flags & LK_THISLAYER || error)
		return (error);
	/*
	 * Hmm... in a vput(), this means we'll grab the lomacfs interlock,
	 * then the lower interlock.  I don't think this matters, though,
	 * since both won't be held at the same time.
	 */
	if (lvp != NULL)
		error = VOP_UNLOCK(lvp, lflags, td);
	return (error);
}

static int
lomacfs_islocked(
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap
) {

	struct vnode *vp = ap->a_vp;
	struct thread *td = ap->a_td;

	return (lockstatus(&vp->v_lock, td));
}

static int
lomacfs_lookup(
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap
) {
	int error;

	error = vfs_cache_lookup(ap);
#if defined(LOMAC_DEBUG_LOOKUPSTATS)
	if (error == 0)
		lomacfs_successful_lookups++;
	else
		lomacfs_failed_lookups++;
#endif
#if defined(LOMAC_DEBUG_LOOKUP)
	if (error == 0 && (*ap->a_vpp)->v_mount == dvp->v_mount) {
		struct vnode *vp = *ap->a_vpp;
#if defined(LOMAC_DEBUG_INCNAME)
		const char *name = VTOLOMAC(vp)->ln_name;
#else
		const char *name = "[unknown]";
#endif
		printf("lomacfs: lookup(%p \"%s\"), lvp usecount up to %u\n",
		    vp, name, VTOLVP(vp)->v_usecount);
	}
#endif
	return (error);
}

static int
lomacfs_cachedlookup(
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap
) {
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *ldvp = VTOLVP(dvp);
	struct vnode *lvp;
	int makeentry;
	int error;

	if (cnp->cn_flags & ISLASTCN && cnp->cn_nameiop != LOOKUP &&
	    cnp->cn_nameiop != CREATE) {
		lomac_object_t lobj = { LO_TYPE_LVNODE, { dvp } };
		const char *op;

		if (cnp->cn_nameiop == DELETE)
			op = "delete";
		else
			op = "rename";

		if (!mediate_subject_object(op, curthread->td_proc, &lobj))
			return (EPERM);
	}
	makeentry = cnp->cn_flags & MAKEENTRY;
	cnp->cn_flags &= ~makeentry;
	error = VOP_LOOKUP(ldvp, &lvp, cnp);
	cnp->cn_flags |= makeentry;
	if ((error == 0 || error == EJUSTRETURN) &&
	    cnp->cn_flags != (cnp->cn_flags | LOCKPARENT | ISLASTCN))
		(void)VOP_UNLOCK(dvp, LK_THISLAYER, curthread);
	if (error == 0 && lvp->v_type != VSOCK) {
		struct mount *mp;

		/*
		 * Check to see if the vnode has been mounted on;
		 * if so find the root of the mounted filesystem.
		 */
		if (lvp->v_type == VDIR && (mp = lvp->v_mountedhere) &&
		    (cnp->cn_flags & NOCROSSMOUNT) == 0) {
			struct vnode *tdp;

			if (vfs_busy(mp, 0, 0, curthread))
				goto forget_it;
			VOP_UNLOCK(lvp, 0, curthread);
			error = VFS_ROOT(mp, &tdp);
			vfs_unbusy(mp, curthread);
			if (error) {
				vrele(lvp);
				return (error);
			}
			vrele(lvp);
			lvp = tdp;
		}
forget_it:
		/*
		 * For a create or for devices (dynamic things, aren't they),
		 * don't enter the vnode into the cache.
		 */
		if (cnp->cn_nameiop == CREATE || lvp->v_type == VCHR)
			cnp->cn_flags &= ~makeentry;
		/*
		 * The top half of dvp is locked, but ldvp is unlocked.
		 * Additionally, lvp is locked already, and
		 * lomacfs_node_alloc() always returns it locked.
		 */
		error = lomacfs_node_alloc(dvp->v_mount, cnp,
		    dvp, lvp, ap->a_vpp);
		if (cnp->cn_nameiop == CREATE)
			cnp->cn_flags |= makeentry;
#if defined(LOMAC_DEBUG_LOOKUPSTATS)
		if (error) {
			if (error != EEXIST) {
				lomacfs_node_alloc_failures++;
			} else {
				lomacfs_node_alloc_clashes++;
				error = 0;
			}
		}
#else
		if (error == EEXIST)
			error = 0;
#endif
	} else if (error == 0) {
		/*
		 * For sockets, just return the "real" thing
		 * after entering it into the cache.
		 */
		*ap->a_vpp = lvp;
		if (cnp->cn_nameiop != CREATE && cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, lvp, cnp);
	}
		
#if defined(LOMAC_DEBUG_LOOKUPSTATS)
	if (error == 0)
		lomacfs_successful_cachedlookups++;
	else
		lomacfs_failed_cachedlookups++;
#endif
	return (error);
}

static int
lomacfs_getattr(
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	*/ *ap
) {
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error;

	error = VOP_GETATTR(VTOLVP(vp), vap, ap->a_cred, ap->a_td);
	if (error == 0 && vap->va_fsid == VNOVAL)
		vap->va_fsid = VTOLVP(vp)->v_mount->mnt_stat.f_fsid.val[0];
	return (error);
}

static int
lomacfs_setattr(
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	*/ *ap
) {
	lomac_object_t lobj = { LO_TYPE_LVNODE, { ap->a_vp } };
	int error;

	if (mediate_subject_object(ap->a_desc->vdesc_name, curthread->td_proc,
	    &lobj))
		error = VOP_SETATTR(VTOLVP(ap->a_vp), ap->a_vap, ap->a_cred,
		    ap->a_td);
	else
		error = EPERM;
	return (error);
}

static int
lomacfs_readdir(
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap
) {

	return (VOP_READDIR(VTOLVP(ap->a_vp), ap->a_uio, ap->a_cred,
	    ap->a_eofflag, ap->a_ncookies, ap->a_cookies));
}

static int
lomacfs_open(
	struct vop_open_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {
	lomac_object_t lobj;
	int error;

	lobj.lo_type = LO_TYPE_LVNODE;
	lobj.lo_object.vnode = ap->a_vp;
	if (!mediate_subject_object_open(ap->a_td->td_proc, &lobj))
		error = EPERM;
	else
		error = VOP_OPEN(VTOLVP(ap->a_vp), ap->a_mode, ap->a_cred,
		    ap->a_td);
	return (error);
}

static int
lomacfs_close(
	struct vop_close_args /* {
		struct vnode *a_vp;
		int a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;
	struct vnode *lvp = VTOLVP(vp);
	int error;

	/*
	 * XXX
	 * Try to cope with the horrible semantics introduced here...
	 */
	vref(lvp);
	error = VOP_CLOSE(lvp, ap->a_fflag, ap->a_cred, ap->a_td);
	if (error == EAGAIN)
		error = 0;
	else
		vrele(lvp);
	return (error);
}

static int
lomacfs_access(
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {

	return (VOP_ACCESS(VTOLVP(ap->a_vp), ap->a_mode, ap->a_cred, ap->a_td));
}

static int
lomacfs_readlink(
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap
) {
	struct vnode *lvp = VTOLVP(ap->a_vp);

	if (lvp == NULL)
		return (EPERM);
	return (VOP_READLINK(lvp, ap->a_uio, ap->a_cred));
}

static int
lomacfs_lease(
	struct vop_lease_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
		struct ucred *a_cred;
		int a_flag;
	} */ *ap
) {
	struct vnode *lvp = VTOLVP(ap->a_vp);

	return (VOP_LEASE(lvp, ap->a_td, ap->a_cred, ap->a_flag));
}

static int
lomacfs_read(
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap
) {
	struct vnode *lvp = VTOLVP(ap->a_vp);
	lomac_object_t lobj = { LO_TYPE_LVNODE, { ap->a_vp } };
	int error;

	error = monitor_read_object(curthread->td_proc, &lobj);
	if (error == 0)
		error = VOP_READ(lvp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	return (error);
}

static int
lomacfs_write(
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap
) {
	struct vnode *lvp = VTOLVP(ap->a_vp);
	lomac_object_t lobj = { LO_TYPE_LVNODE, { ap->a_vp } };
	int error;

	if (mediate_subject_object(ap->a_desc->vdesc_name, curthread->td_proc,
	    &lobj))
		error = VOP_WRITE(lvp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	else
		error = EIO;
	return (error);
}

static int
lomacfs_ioctl(
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t a_data;
		int a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {
	struct vnode *lvp = VTOLVP(ap->a_vp);

	return (VOP_IOCTL(lvp, ap->a_command, ap->a_data, ap->a_fflag,
	    ap->a_cred, ap->a_td));
}

static int
lomacfs_muxcreate(
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap
) {
	struct vnode *dvp = ap->a_dvp;
	struct vnode *ldvp = VTOLVP(dvp);
	struct componentname *cnp = ap->a_cnp;
	struct vattr *vap = ap->a_vap;
	int makeentry = cnp->cn_flags & MAKEENTRY;
	lomac_object_t lobj = { LO_TYPE_LVNODE, { dvp } };
	struct thread *td = curthread;
	int error;

	if (!mediate_subject_object(ap->a_desc->vdesc_name, td->td_proc,
	    &lobj) || (vap->va_type == VCHR &&
	    !mediate_subject_at_level("mknod", curthread->td_proc,
	    LOMAC_HIGHEST_LEVEL)))
		return (EPERM);
	ap->a_dvp = ldvp;
	cnp->cn_flags &= ~makeentry;
	error = VCALL(ldvp, ap->a_desc->vdesc_offset, ap);
	if (error == 0) {
		struct vnode *vp;
		int issock;

		issock = vap->va_type == VSOCK;
		vp = *ap->a_vpp;
		*ap->a_vpp = NULL;
		if (!issock)
			cnp->cn_flags |= makeentry;
		error = lomacfs_node_alloc(dvp->v_mount, cnp, dvp, vp,
		    ap->a_vpp);
		if (error)
			vput(vp);
		else if (issock) {
			/*
			 * I should really find a nicer way to do this.
			 */
			vref(vp);
			vput(*ap->a_vpp);
			*ap->a_vpp = vp;
			(void)VOP_LOCK(vp, LK_EXCLUSIVE | LK_RETRY, td);
		}
	}
	return (error);
}

static int
lomacfs_muxremove(
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap
) {
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	int error;

	ap->a_dvp = VTOLVP(dvp);
	if (VISLOMAC(vp))
		ap->a_vp = VTOLVP(vp);
	error = VCALL(ap->a_dvp, ap->a_desc->vdesc_offset, ap);
	if (error == 0)
		cache_purge(vp);
	return (error);
}

static int
lomacfs_fsync(
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap
) {

	return (VOP_FSYNC(VTOLVP(ap->a_vp), ap->a_cred, ap->a_waitfor,
	    ap->a_td));
}

static int
lomacfs_advlock(
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap
) {

	return (VOP_ADVLOCK(VTOLVP(ap->a_vp), ap->a_id, ap->a_op, ap->a_fl,
	    ap->a_flags));
}

static int
lomacfs_whiteout(
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap
) {

	return (VOP_WHITEOUT(VTOLVP(ap->a_dvp), ap->a_cnp, ap->a_flags));
}

static int
lomacfs_poll(
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {

	return (VOP_POLL(VTOLVP(ap->a_vp), ap->a_events, ap->a_cred, ap->a_td));
}

static int
lomacfs_revoke(
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap
) {

	return (VOP_REVOKE(VTOLVP(ap->a_vp), ap->a_flags));
}

static int
lomacfs_link(
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap
) {
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *vp = ap->a_vp;
	struct vnode *lvp = VISLOMAC(vp) ? VTOLVP(vp) : vp;
	struct componentname *cnp = ap->a_cnp;
	int error;

	error = VOP_LINK(VTOLVP(tdvp), lvp, cnp);
	if (error == 0 && vp->v_type == VSOCK) {
		cache_enter(tdvp, vp, cnp);
#if defined(LOMAC_DEBUG_LINK)
		do {
			struct vnode *nvp;
			int nerror;

			nerror = cache_lookup(tdvp, &nvp, cnp);
			printf("lomacfs: link(%p), cache_lookup() = %d (%p)\n",
			    vp, nerror, nvp);
		} while (0);
#endif
	}
	return (error);
}

static int
lomacfs_rename(
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap
) {
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *fvp = ap->a_fvp;
	struct componentname *fcnp = ap->a_fcnp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *tvp = ap->a_tvp;
	struct componentname *tcnp = ap->a_tcnp;
	int fvp_is_lomac = VISLOMAC(fvp);
	int error;

	vref(VTOLVP(fdvp));
	/*
	 * Handle the case when LOMAC returns a real vnode for
	 * VSOCK, rather than the LOMAC covering vnode.
	 */
	if (fvp_is_lomac)
		vref(VTOLVP(fvp));
	vref(VTOLVP(tdvp));
	if (tvp != NULL)
		vref(VTOLVP(tvp));
	error = VOP_RENAME(VTOLVP(fdvp), fvp_is_lomac ? VTOLVP(fvp) : fvp, fcnp,
	    VTOLVP(tdvp), tvp != NULL ? VTOLVP(tvp) : NULL, tcnp);
        if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
	cache_purge(fvp);
	if (tvp != NULL)
		cache_purge(tvp);
	(void)VOP_UNLOCK(tdvp, LK_THISLAYER, curthread);
	vrele(fdvp);
	if (fvp_is_lomac)
		vrele(fvp);
	vrele(tdvp);
	if (tvp != NULL) {
		(void)VOP_UNLOCK(tvp, LK_THISLAYER, curthread);
		vrele(tvp);
	} else if (tcnp->cn_nameiop == RENAME /* NOCACHE unsets MAKEENTRY */
	    && fvp->v_type == VSOCK)
		cache_enter(tdvp, fvp, tcnp);
	return (error);
}

static int
lomacfs_strategy(
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap
) {

	return (VOP_STRATEGY(VTOLVP(ap->a_vp), ap->a_bp));
}

/*
 * Let an underlying filesystem do the work of creating the "actual"
 * vm_object_t, and we will reference it.
 */
static int
lomacfs_createvobject(
	struct vop_createvobject_args /* {
		struct vnode *vp;
		struct ucred *cred;
		struct proc *p;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;
	struct vnode *lowervp = VTOLOMAC(vp) != NULL ? VTOLVP(vp) : NULL;
	int error;

	if (vp->v_type == VNON || lowervp == NULL)
		return (EINVAL);
	error = VOP_CREATEVOBJECT(lowervp, ap->a_cred, ap->a_td);
	if (error)
		return (error);
	vp->v_vflag |= VV_OBJBUF;
	return (error);
}

/*
 * We need to destroy the lower vnode object only if we created it.
 * XXX - I am very unsure about all of this.
 */
static int
lomacfs_destroyvobject(
	struct vop_destroyvobject_args /* {
		struct vnode *vp;
	} */ *ap
) {
	struct vnode *vp = ap->a_vp;

	vp->v_vflag &= ~VV_OBJBUF;
	return (0);
}

static int
lomacfs_bmap(
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap
) {

	return (VOP_BMAP(VTOLVP(ap->a_vp), ap->a_bn, ap->a_vpp, ap->a_bnp,
	   ap->a_runp, ap->a_runb));
}

static int
lomacfs_getpages(
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_reqpage;
		vm_ooffset_t a_offset;
	} */ *ap
) {

	return (VOP_GETPAGES(VTOLVP(ap->a_vp), ap->a_m, ap->a_count,
	    ap->a_reqpage, ap->a_offset));
}

static int
lomacfs_putpages(
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_sync;
		int *a_rtvals;
		vm_ooffset_t a_offset;
	} */ *ap
) {

	return (VOP_PUTPAGES(VTOLVP(ap->a_vp), ap->a_m, ap->a_count,
	    ap->a_sync, ap->a_rtvals, ap->a_offset));
}

static int
lomacfs_getvobject(
	struct vop_getvobject_args /* {
		struct vnode *a_vp;
		struct vm_object **a_objpp;
	} */ *ap
) {
	struct vnode *lvp = VTOLVP(ap->a_vp);

	if (lvp == NULL)
		return EINVAL;
	return (VOP_GETVOBJECT(lvp, ap->a_objpp));
}

static int
lomacfs_kqfilter(
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap
) {

	return (VOP_KQFILTER(VTOLVP(ap->a_vp), ap->a_kn));
}

static int
lomacfs_pathconf(
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap
) {

	return (VOP_PATHCONF(VTOLVP(ap->a_vp), ap->a_name, ap->a_retval));
}	

static int
lomacfs_reallocblks(
	struct vop_reallocblks_args /* {
		struct vnode *a_vp;
		struct cluster_save *a_buflist;
	} */ *ap
) {

	return (VOP_REALLOCBLKS(VTOLVP(ap->a_vp), ap->a_buflist));
}

static int
lomacfs_freeblks(
	struct vop_freeblks_args /* {
		struct vnode *a_vp;
		daddr_t a_addr;
		daddr_t a_length;
	} */ *ap
) {

	return (VOP_FREEBLKS(VTOLVP(ap->a_vp), ap->a_addr, ap->a_length));
}

static int
lomacfs_getacl(
	struct vop_getacl_args /* {
		struct vnode *a_vp;
		acl_type_t a_type;
		struct acl *a_aclp;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {

	return (VOP_GETACL(VTOLVP(ap->a_vp), ap->a_type, ap->a_aclp, ap->a_cred,
	    ap->a_td));
}

static int
lomacfs_setacl(
	struct vop_setacl_args /* {
		struct vnode *a_vp;
		acl_type_t a_type;
		struct acl *a_aclp;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {
	lomac_object_t lobj;

	lobj.lo_type = LO_TYPE_LVNODE;
	lobj.lo_object.vnode = ap->a_vp;
	if (!mediate_subject_object("setacl", ap->a_td->td_proc, &lobj))
		return (EPERM);
	else
		return (VOP_SETACL(VTOLVP(ap->a_vp), ap->a_type, ap->a_aclp,
		    ap->a_cred, ap->a_td));
}

static int
lomacfs_aclcheck(
	struct vop_aclcheck_args /* {
		struct vnode *a_vp;
		acl_type_t a_type;
		struct acl *a_aclp;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {

	return (VOP_ACLCHECK(VTOLVP(ap->a_vp), ap->a_type, ap->a_aclp,
	    ap->a_cred, ap->a_td));
}

static int
lomacfs_getextattr(
	struct vop_getextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		size_t *a_size;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {
	lomac_object_t lobj;

	lobj.lo_type = LO_TYPE_LVNODE;
	lobj.lo_object.vnode = ap->a_vp;
	if (monitor_read_object(ap->a_td->td_proc, &lobj))
		return (EPERM);
	else
		return (VOP_GETEXTATTR(VTOLVP(ap->a_vp), ap->a_attrnamespace,
		    ap->a_name, ap->a_uio, ap->a_size, ap->a_cred, ap->a_td));
}

static int
lomacfs_setextattr(
	struct vop_setextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap
) {
	lomac_object_t lobj;

	lobj.lo_type = LO_TYPE_LVNODE;
	lobj.lo_object.vnode = ap->a_vp;
	if (!mediate_subject_object("setextattr", ap->a_td->td_proc, &lobj))
		return (EPERM);
	else
		return (VOP_SETEXTATTR(VTOLVP(ap->a_vp), ap->a_attrnamespace,
		    ap->a_name, ap->a_uio, ap->a_cred, ap->a_td));
}

vop_t **lomacfs_vnodeop_p;
static struct vnodeopv_entry_desc lomacfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *)lomacfs_defaultop },
	{ &vop_inactive_desc,		(vop_t *)lomacfs_inactive },
	{ &vop_reclaim_desc,		(vop_t *)lomacfs_reclaim },
	{ &vop_print_desc,		(vop_t *)lomacfs_print },
	{ &vop_lock_desc,		(vop_t *)lomacfs_lock },
	{ &vop_unlock_desc,		(vop_t *)lomacfs_unlock },
	{ &vop_islocked_desc,		(vop_t *)lomacfs_islocked },
	{ &vop_lookup_desc,		(vop_t *)lomacfs_lookup },
	{ &vop_setattr_desc,		(vop_t *)lomacfs_setattr },
	{ &vop_getattr_desc,		(vop_t *)lomacfs_getattr },
	{ &vop_readdir_desc,		(vop_t *)lomacfs_readdir },
	{ &vop_open_desc,		(vop_t *)lomacfs_open },
	{ &vop_close_desc,		(vop_t *)lomacfs_close },
	{ &vop_access_desc,		(vop_t *)lomacfs_access },
	{ &vop_readlink_desc,		(vop_t *)lomacfs_readlink },
	{ &vop_lease_desc,		(vop_t *)lomacfs_lease },
	{ &vop_read_desc,		(vop_t *)lomacfs_read },
	{ &vop_write_desc,		(vop_t *)lomacfs_write },
	{ &vop_ioctl_desc,		(vop_t *)lomacfs_ioctl },
	{ &vop_create_desc,		(vop_t *)lomacfs_muxcreate },
	{ &vop_mkdir_desc,		(vop_t *)lomacfs_muxcreate },
	{ &vop_mknod_desc,		(vop_t *)lomacfs_muxcreate },
	{ &vop_symlink_desc,		(vop_t *)lomacfs_muxcreate },
	{ &vop_remove_desc,		(vop_t *)lomacfs_muxremove },
	{ &vop_rmdir_desc,		(vop_t *)lomacfs_muxremove },
	{ &vop_fsync_desc,		(vop_t *)lomacfs_fsync },
	{ &vop_advlock_desc,		(vop_t *)lomacfs_advlock },
	{ &vop_whiteout_desc,		(vop_t *)lomacfs_whiteout },
	{ &vop_poll_desc,		(vop_t *)lomacfs_poll },
	{ &vop_link_desc,		(vop_t *)lomacfs_link },
	{ &vop_rename_desc,		(vop_t *)lomacfs_rename },
	{ &vop_revoke_desc,		(vop_t *)lomacfs_revoke },
	{ &vop_cachedlookup_desc,	(vop_t *)lomacfs_cachedlookup },
	{ &vop_lookup_desc,		(vop_t *)lomacfs_lookup },
	{ &vop_bmap_desc,		(vop_t *)lomacfs_bmap },
	{ &vop_getpages_desc,		(vop_t *)lomacfs_getpages },
	{ &vop_putpages_desc,		(vop_t *)lomacfs_putpages },
	{ &vop_strategy_desc,		(vop_t *)lomacfs_strategy },
	{ &vop_createvobject_desc,	(vop_t *)lomacfs_createvobject },
	{ &vop_destroyvobject_desc,	(vop_t *)lomacfs_destroyvobject },
	{ &vop_getvobject_desc,		(vop_t *)lomacfs_getvobject },
	{ &vop_getwritemount_desc,	(vop_t *)vop_stdgetwritemount },
	{ &vop_kqfilter_desc,		(vop_t *)lomacfs_kqfilter },
	{ &vop_pathconf_desc,		(vop_t *)lomacfs_pathconf },
	{ &vop_reallocblks_desc,	(vop_t *)lomacfs_reallocblks },
	{ &vop_freeblks_desc,		(vop_t *)lomacfs_freeblks },
	{ &vop_getacl_desc,		(vop_t *)lomacfs_getacl },
	{ &vop_setacl_desc,		(vop_t *)lomacfs_setacl },
	{ &vop_aclcheck_desc,		(vop_t *)lomacfs_aclcheck },
	{ &vop_getextattr_desc,		(vop_t *)lomacfs_getextattr },
	{ &vop_setextattr_desc,		(vop_t *)lomacfs_setextattr },
	{ NULL,				NULL }
};
static struct vnodeopv_desc lomacfs_vnodeopv_opv_desc =
	{ &lomacfs_vnodeop_p, lomacfs_vnodeop_entries };
VNODEOP_SET(lomacfs_vnodeopv_opv_desc);
