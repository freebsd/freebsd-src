/*
 * Copyright (c) 1992, 1993, 1994, 1995 Jan-Simon Pendry.
 * Copyright (c) 1992, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)union_vnops.c	8.32 (Berkeley) 6/23/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <fs/unionfs/union.h>

#include <vm/vm.h>
#include <vm/vnode_pager.h>

#include <vm/vm_page.h>
#include <vm/vm_object.h>

int uniondebug = 0;

#if UDEBUG_ENABLED
SYSCTL_INT(_vfs, OID_AUTO, uniondebug, CTLFLAG_RW, &uniondebug, 0, "");
#else
SYSCTL_INT(_vfs, OID_AUTO, uniondebug, CTLFLAG_RD, &uniondebug, 0, "");
#endif

static int	union_access(struct vop_access_args *ap);
static int	union_advlock(struct vop_advlock_args *ap);
static int	union_close(struct vop_close_args *ap);
static int	union_create(struct vop_create_args *ap);
static int	union_createvobject(struct vop_createvobject_args *ap);
static int	union_destroyvobject(struct vop_destroyvobject_args *ap);
static int	union_fsync(struct vop_fsync_args *ap);
static int	union_getattr(struct vop_getattr_args *ap);
static int	union_getvobject(struct vop_getvobject_args *ap);
static int	union_inactive(struct vop_inactive_args *ap);
static int	union_ioctl(struct vop_ioctl_args *ap);
static int	union_lease(struct vop_lease_args *ap);
static int	union_link(struct vop_link_args *ap);
static int	union_lookup(struct vop_lookup_args *ap);
static int	union_lookup1(struct vnode *udvp, struct vnode **dvp,
				   struct vnode **vpp,
				   struct componentname *cnp);
static int	union_mkdir(struct vop_mkdir_args *ap);
static int	union_mknod(struct vop_mknod_args *ap);
static int	union_open(struct vop_open_args *ap);
static int	union_pathconf(struct vop_pathconf_args *ap);
static int	union_print(struct vop_print_args *ap);
static int	union_read(struct vop_read_args *ap);
static int	union_readdir(struct vop_readdir_args *ap);
static int	union_readlink(struct vop_readlink_args *ap);
static int	union_getwritemount(struct vop_getwritemount_args *ap);
static int	union_reclaim(struct vop_reclaim_args *ap);
static int	union_remove(struct vop_remove_args *ap);
static int	union_rename(struct vop_rename_args *ap);
static int	union_revoke(struct vop_revoke_args *ap);
static int	union_rmdir(struct vop_rmdir_args *ap);
static int	union_poll(struct vop_poll_args *ap);
static int	union_setattr(struct vop_setattr_args *ap);
static int	union_strategy(struct vop_strategy_args *ap);
static int	union_symlink(struct vop_symlink_args *ap);
static int	union_whiteout(struct vop_whiteout_args *ap);
static int	union_write(struct vop_read_args *ap);

static __inline
struct vnode *
union_lock_upper(struct union_node *un, struct thread *td)
{
	struct vnode *uppervp;

	if ((uppervp = un->un_uppervp) != NULL) {
		VREF(uppervp);
		vn_lock(uppervp, LK_EXCLUSIVE | LK_CANRECURSE | LK_RETRY, td);
	}
	KASSERT((uppervp == NULL || vrefcnt(uppervp) > 0), ("uppervp usecount is 0"));
	return(uppervp);
}

static __inline
void
union_unlock_upper(struct vnode *uppervp, struct thread *td)
{
	vput(uppervp);
}

static __inline
struct vnode *
union_lock_other(struct union_node *un, struct thread *td)
{
	struct vnode *vp;

	if (un->un_uppervp != NULL) {
		vp = union_lock_upper(un, td);
	} else if ((vp = un->un_lowervp) != NULL) {
		VREF(vp);
		vn_lock(vp, LK_EXCLUSIVE | LK_CANRECURSE | LK_RETRY, td);
	}
	return(vp);
}

static __inline
void
union_unlock_other(struct vnode *vp, struct thread *td)
{
	vput(vp);
}

/*
 *	union_lookup:
 *
 *	udvp	must be exclusively locked on call and will remain 
 *		exclusively locked on return.  This is the mount point 
 *		for our filesystem.
 *
 *	dvp	Our base directory, locked and referenced.
 *		The passed dvp will be dereferenced and unlocked on return
 *		and a new dvp will be returned which is locked and 
 *		referenced in the same variable.
 *
 *	vpp	is filled in with the result if no error occured,
 *		locked and ref'd.
 *
 *		If an error is returned, *vpp is set to NULLVP.  If no
 *		error occurs, *vpp is returned with a reference and an
 *		exclusive lock.
 */

static int
union_lookup1(udvp, pdvp, vpp, cnp)
	struct vnode *udvp;
	struct vnode **pdvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	int error;
	struct thread *td = cnp->cn_thread;
	struct vnode *dvp = *pdvp;
	struct vnode *tdvp;
	struct mount *mp;

	/*
	 * If stepping up the directory tree, check for going
	 * back across the mount point, in which case do what
	 * lookup would do by stepping back down the mount
	 * hierarchy.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		while ((dvp != udvp) && (dvp->v_vflag & VV_ROOT)) {
			/*
			 * Don't do the NOCROSSMOUNT check
			 * at this level.  By definition,
			 * union fs deals with namespaces, not
			 * filesystems.
			 */
			tdvp = dvp;
			dvp = dvp->v_mount->mnt_vnodecovered;
			VREF(dvp);
			vput(tdvp);
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
		}
	}

	/*
	 * Set return dvp to be the upperdvp 'parent directory.
	 */
	*pdvp = dvp;

	/*
	 * If the VOP_LOOKUP() call generates an error, tdvp is invalid and
	 * no changes will have been made to dvp, so we are set to return.
	 */

        error = VOP_LOOKUP(dvp, &tdvp, cnp);
	if (error) {
		UDEBUG(("dvp %p error %d flags %lx\n", dvp, error, cnp->cn_flags));
		*vpp = NULL;
		return (error);
	}

	/*
	 * The parent directory will have been unlocked, unless lookup
	 * found the last component or if dvp == tdvp (tdvp must be locked).
	 *
	 * We want our dvp to remain locked and ref'd.  We also want tdvp
	 * to remain locked and ref'd.
	 */
	UDEBUG(("parentdir %p result %p flag %lx\n", dvp, tdvp, cnp->cn_flags));

	if (dvp != tdvp && (cnp->cn_flags & ISLASTCN) == 0)
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);

	/*
	 * Lastly check if the current node is a mount point in
	 * which case walk up the mount hierarchy making sure not to
	 * bump into the root of the mount tree (ie. dvp != udvp).
	 *
	 * We use dvp as a temporary variable here, it is no longer related
	 * to the dvp above.  However, we have to ensure that both *pdvp and
	 * tdvp are locked on return.
	 */

	dvp = tdvp;
	while (
	    dvp != udvp && 
	    (dvp->v_type == VDIR) &&
	    (mp = dvp->v_mountedhere)
	) {
		int relock_pdvp = 0;

		if (vfs_busy(mp, 0, 0, td))
			continue;

		if (dvp == *pdvp)
			relock_pdvp = 1;
		vput(dvp);
		dvp = NULL;
		error = VFS_ROOT(mp, &dvp);

		vfs_unbusy(mp, td);

		if (relock_pdvp)
			vn_lock(*pdvp, LK_EXCLUSIVE | LK_RETRY, td);

		if (error) {
			*vpp = NULL;
			return (error);
		}
	}
	*vpp = dvp;
	return (0);
}

static int
union_lookup(ap)
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	int uerror, lerror;
	struct vnode *uppervp, *lowervp;
	struct vnode *upperdvp, *lowerdvp;
	struct vnode *dvp = ap->a_dvp;		/* starting dir */
	struct union_node *dun = VTOUNION(dvp);	/* associated union node */
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	int lockparent = cnp->cn_flags & LOCKPARENT;
	struct union_mount *um = MOUNTTOUNIONMOUNT(dvp->v_mount);
	struct ucred *saved_cred = NULL;
	int iswhiteout;
	struct vattr va;

	*ap->a_vpp = NULLVP;

	/*
	 * Disallow write attempts to the filesystem mounted read-only.
	 */
	if ((cnp->cn_flags & ISLASTCN) && 
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		return (EROFS);
	}

	/*
	 * For any lookups we do, always return with the parent locked.
	 */
	cnp->cn_flags |= LOCKPARENT;

	lowerdvp = dun->un_lowervp;
	uppervp = NULLVP;
	lowervp = NULLVP;
	iswhiteout = 0;

	uerror = ENOENT;
	lerror = ENOENT;

	/*
	 * Get a private lock on uppervp and a reference, effectively 
	 * taking it out of the union_node's control.
	 *
	 * We must lock upperdvp while holding our lock on dvp
	 * to avoid a deadlock.
	 */
	upperdvp = union_lock_upper(dun, td);

	/*
	 * Do the lookup in the upper level.
	 * If that level consumes additional pathnames,
	 * then assume that something special is going
	 * on and just return that vnode.
	 */
	if (upperdvp != NULLVP) {
		/*
		 * We do not have to worry about the DOTDOT case, we've
		 * already unlocked dvp.
		 */
		UDEBUG(("A %p\n", upperdvp));

		/*
		 * Do the lookup.   We must supply a locked and referenced
		 * upperdvp to the function and will get a new locked and
		 * referenced upperdvp back, with the old having been 
		 * dereferenced.
		 *
		 * If an error is returned, uppervp will be NULLVP.  If no
		 * error occurs, uppervp will be the locked and referenced.
		 * Return vnode, or possibly NULL, depending on what is being
		 * requested.  It is possible that the returned uppervp
		 * will be the same as upperdvp.
		 */
		uerror = union_lookup1(um->um_uppervp, &upperdvp, &uppervp, cnp);
		UDEBUG((
		    "uerror %d upperdvp %p %d/%d, uppervp %p ref=%d/lck=%d\n",
		    uerror,
		    upperdvp,
		    vrefcnt(upperdvp),
		    VOP_ISLOCKED(upperdvp, NULL),
		    uppervp,
		    (uppervp ? vrefcnt(uppervp) : -99),
		    (uppervp ? VOP_ISLOCKED(uppervp, NULL) : -99)
		));

		/*
		 * Disallow write attempts to the filesystem mounted read-only.
		 */
		if (uerror == EJUSTRETURN && (cnp->cn_flags & ISLASTCN) &&
		    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
		    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)) {
			error = EROFS;
			goto out;
		}

		/*
		 * Special case: If cn_consume != 0 then skip out.  The result
		 * of the lookup is transfered to our return variable.  If
		 * an error occured we have to throw away the results.
		 */

		if (cnp->cn_consume != 0) {
			if ((error = uerror) == 0) {
				*ap->a_vpp = uppervp;
				uppervp = NULL;
			}
			goto out;
		}

		/*
		 * Calculate whiteout, fall through.
		 */

		if (uerror == ENOENT || uerror == EJUSTRETURN) {
			if (cnp->cn_flags & ISWHITEOUT) {
				iswhiteout = 1;
			} else if (lowerdvp != NULLVP) {
				int terror;

				terror = VOP_GETATTR(upperdvp, &va,
					cnp->cn_cred, cnp->cn_thread);
				if (terror == 0 && (va.va_flags & OPAQUE))
					iswhiteout = 1;
			}
		}
	}

	/*
	 * In a similar way to the upper layer, do the lookup
	 * in the lower layer.   This time, if there is some
	 * component magic going on, then vput whatever we got
	 * back from the upper layer and return the lower vnode
	 * instead.
	 */

	if (lowerdvp != NULLVP && !iswhiteout) {
		int nameiop;

		UDEBUG(("B %p\n", lowerdvp));

		/*
		 * Force only LOOKUPs on the lower node, since
		 * we won't be making changes to it anyway.
		 */
		nameiop = cnp->cn_nameiop;
		cnp->cn_nameiop = LOOKUP;
		if (um->um_op == UNMNT_BELOW) {
			saved_cred = cnp->cn_cred;
			cnp->cn_cred = um->um_cred;
		}

		/*
		 * We shouldn't have to worry about locking interactions
		 * between the lower layer and our union layer (w.r.t.
		 * `..' processing) because we don't futz with lowervp
		 * locks in the union-node instantiation code path.
		 *
		 * union_lookup1() requires lowervp to be locked on entry,
		 * and it will be unlocked on return.  The ref count will
		 * not change.  On return lowervp doesn't represent anything
		 * to us so we NULL it out.
		 */
		VREF(lowerdvp);
		vn_lock(lowerdvp, LK_EXCLUSIVE | LK_RETRY, td);
		lerror = union_lookup1(um->um_lowervp, &lowerdvp, &lowervp, cnp);
		if (lowerdvp == lowervp)
			vrele(lowerdvp);
		else
			vput(lowerdvp);
		lowerdvp = NULL;	/* lowerdvp invalid after vput */

		if (um->um_op == UNMNT_BELOW)
			cnp->cn_cred = saved_cred;
		cnp->cn_nameiop = nameiop;

		if (cnp->cn_consume != 0 || lerror == EACCES) {
			if ((error = lerror) == 0) {
				*ap->a_vpp = lowervp;
				lowervp = NULL;
			}
			goto out;
		}
	} else {
		UDEBUG(("C %p\n", lowerdvp));
		if ((cnp->cn_flags & ISDOTDOT) && dun->un_pvp != NULLVP) {
			if ((lowervp = LOWERVP(dun->un_pvp)) != NULL) {
				VREF(lowervp);
				vn_lock(lowervp, LK_EXCLUSIVE | LK_RETRY, td);
				lerror = 0;
			}
		}
	}

	/*
	 * Ok.  Now we have uerror, uppervp, upperdvp, lerror, and lowervp.
	 *
	 * 1. If both layers returned an error, select the upper layer.
	 *
	 * 2. If the upper layer failed and the bottom layer succeeded,
	 *    two subcases occur:
	 *
	 *	a.  The bottom vnode is not a directory, in which case
	 *	    just return a new union vnode referencing an
	 *	    empty top layer and the existing bottom layer.
	 *
	 *	b.  The bottom vnode is a directory, in which case
	 *	    create a new directory in the top layer and
	 *	    and fall through to case 3.
	 *
	 * 3. If the top layer succeeded, then return a new union
	 *    vnode referencing whatever the new top layer and
	 *    whatever the bottom layer returned.
	 */

	/* case 1. */
	if ((uerror != 0) && (lerror != 0)) {
		error = uerror;
		goto out;
	}

	/* case 2. */
	if (uerror != 0 /* && (lerror == 0) */ ) {
		if (lowervp->v_type == VDIR) { /* case 2b. */
			KASSERT(uppervp == NULL, ("uppervp unexpectedly non-NULL"));
			/*
			 * Oops, uppervp has a problem, we may have to shadow.
			 */
			uerror = union_mkshadow(um, upperdvp, cnp, &uppervp);
			if (uerror) {
				error = uerror;
				goto out;
			}
		}
	}

	/*
	 * Must call union_allocvp() with both the upper and lower vnodes
	 * referenced and the upper vnode locked.   ap->a_vpp is returned 
	 * referenced and locked.  lowervp, uppervp, and upperdvp are 
	 * absorbed by union_allocvp() whether it succeeds or fails.
	 *
	 * upperdvp is the parent directory of uppervp which may be
	 * different, depending on the path, from dvp->un_uppervp.  That's
	 * why it is a separate argument.  Note that it must be unlocked.
	 *
	 * dvp must be locked on entry to the call and will be locked on
	 * return.
	 */

	if (uppervp && uppervp != upperdvp)
		VOP_UNLOCK(uppervp, 0, td);
	if (lowervp)
		VOP_UNLOCK(lowervp, 0, td);
	if (upperdvp)
		VOP_UNLOCK(upperdvp, 0, td);

	error = union_allocvp(ap->a_vpp, dvp->v_mount, dvp, upperdvp, cnp,
			      uppervp, lowervp, 1);

	UDEBUG(("Create %p = %p %p refs=%d\n", *ap->a_vpp, uppervp, lowervp, (*ap->a_vpp) ? vrefcnt(*ap->a_vpp) : -99));

	uppervp = NULL;
	upperdvp = NULL;
	lowervp = NULL;

	/* 
	 *	Termination Code
	 *
	 *	- put away any extra junk laying around.  Note that lowervp
	 *	  (if not NULL) will never be the same as *ap->a_vp and 
	 *	  neither will uppervp, because when we set that state we 
	 *	  NULL-out lowervp or uppervp.  On the otherhand, upperdvp
	 *	  may match uppervp or *ap->a_vpp.
	 *
	 *	- relock/unlock dvp if appropriate.
	 */

out:
	if (upperdvp) {
		if (upperdvp == uppervp || upperdvp == *ap->a_vpp)
			vrele(upperdvp);
		else
			vput(upperdvp);
	}

	if (uppervp)
		vput(uppervp);

	if (lowervp)
		vput(lowervp);

	/*
	 * Restore LOCKPARENT state
	 */

	if (!lockparent)
		cnp->cn_flags &= ~LOCKPARENT;

	UDEBUG(("Out %d vpp %p/%d lower %p upper %p\n", error, *ap->a_vpp,
		((*ap->a_vpp) ? vrefcnt(*ap->a_vpp) : -99),
		lowervp, uppervp));

	if (error == 0 || error == EJUSTRETURN) {
		/*
		 * dvp lock state, determine whether to relock dvp.
		 * We are expected to unlock dvp unless:
		 *
		 *	- there was an error (other than EJUSTRETURN), or
		 *	- we hit the last component and lockparent is true
		 */
		if (*ap->a_vpp != dvp) {
			if (!lockparent || (cnp->cn_flags & ISLASTCN) == 0)
				VOP_UNLOCK(dvp, 0, td);
		}

		if (cnp->cn_namelen == 1 &&
		    cnp->cn_nameptr[0] == '.' &&
		    *ap->a_vpp != dvp) {
#ifdef	DIAGNOSTIC
			vprint("union_lookup: vp", *ap->a_vpp);
			vprint("union_lookup: dvp", dvp);
#endif
			panic("union_lookup returning . (%p) != startdir (%p)",
			    *ap->a_vpp, dvp);
		}
	}

	return (error);
}

/*
 * 	union_create:
 *
 * a_dvp is locked on entry and remains locked on return.  a_vpp is returned
 * locked if no error occurs, otherwise it is garbage.
 */

static int
union_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	struct vnode *dvp;
	int error = EROFS;

	if ((dvp = union_lock_upper(dun, td)) != NULL) {
		struct vnode *vp;
		struct mount *mp;

		error = VOP_CREATE(dvp, &vp, cnp, ap->a_vap);
		if (error == 0) {
			mp = ap->a_dvp->v_mount;
			VOP_UNLOCK(vp, 0, td);
			UDEBUG(("ALLOCVP-1 FROM %p REFS %d\n", vp, vrefcnt(vp)));
			error = union_allocvp(ap->a_vpp, mp, NULLVP, NULLVP,
				cnp, vp, NULLVP, 1);
			UDEBUG(("ALLOCVP-2B FROM %p REFS %d\n", *ap->a_vpp, vrefcnt(vp)));
		}
		union_unlock_upper(dvp, td);
	}
	return (error);
}

static int
union_whiteout(ap)
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;
	struct vnode *uppervp;
	int error;

	switch (ap->a_flags) {
	case CREATE:
	case DELETE:
		uppervp = union_lock_upper(un, cnp->cn_thread);
		if (uppervp != NULLVP) {
			error = VOP_WHITEOUT(un->un_uppervp, cnp, ap->a_flags);
			union_unlock_upper(uppervp, cnp->cn_thread);
		} else
			error = EOPNOTSUPP;
		break;
	case LOOKUP:
		error = EOPNOTSUPP;
		break;
	default:
		panic("union_whiteout: unknown op");
	}
	return (error);
}

/*
 * 	union_mknod:
 *
 *	a_dvp is locked on entry and should remain locked on return.
 *	a_vpp is garbagre whether an error occurs or not.
 */

static int
union_mknod(ap)
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp;
	int error = EROFS;

	if ((dvp = union_lock_upper(dun, cnp->cn_thread)) != NULL) {
		error = VOP_MKNOD(dvp, ap->a_vpp, cnp, ap->a_vap);
		union_unlock_upper(dvp, cnp->cn_thread);
	}
	return (error);
}

/*
 *	union_open:
 *
 *	run open VOP.  When opening the underlying vnode we have to mimic
 *	vn_open().  What we *really* need to do to avoid screwups if the
 *	open semantics change is to call vn_open().  For example, ufs blows
 *	up if you open a file but do not vmio it prior to writing.
 */

static int
union_open(ap)
	struct vop_open_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *tvp;
	int mode = ap->a_mode;
	struct ucred *cred = ap->a_cred;
	struct thread *td = ap->a_td;
	int error = 0;
	int tvpisupper = 1;

	/*
	 * If there is an existing upper vp then simply open that.
	 * The upper vp takes precedence over the lower vp.  When opening
	 * a lower vp for writing copy it to the uppervp and then open the
	 * uppervp.
	 *
	 * At the end of this section tvp will be left locked.
	 */
	if ((tvp = union_lock_upper(un, td)) == NULLVP) {
		/*
		 * If the lower vnode is being opened for writing, then
		 * copy the file contents to the upper vnode and open that,
		 * otherwise can simply open the lower vnode.
		 */
		tvp = un->un_lowervp;
		if ((ap->a_mode & FWRITE) && (tvp->v_type == VREG)) {
			int docopy = !(mode & O_TRUNC);
			error = union_copyup(un, docopy, cred, td);
			tvp = union_lock_upper(un, td);
		} else {
			un->un_openl++;
			VREF(tvp);
			vn_lock(tvp, LK_EXCLUSIVE | LK_RETRY, td);
			tvpisupper = 0;
		}
	}

	/*
	 * We are holding the correct vnode, open it.
	 */

	if (error == 0)
		error = VOP_OPEN(tvp, mode, cred, td, -1);

	/*
	 * This is absolutely necessary or UFS will blow up.
	 */
        if (error == 0 && vn_canvmio(tvp) == TRUE) {
                error = vfs_object_create(tvp, td, cred);
        }

	/*
	 * Release any locks held.
	 */
	if (tvpisupper) {
		if (tvp)
			union_unlock_upper(tvp, td);
	} else {
		vput(tvp);
	}
	return (error);
}

/*
 *	union_close:
 *
 *	It is unclear whether a_vp is passed locked or unlocked.  Whatever
 *	the case we do not change it.
 */

static int
union_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp;

	if ((vp = un->un_uppervp) == NULLVP) {
#ifdef UNION_DIAGNOSTIC
		if (un->un_openl <= 0)
			panic("union: un_openl cnt");
#endif
		--un->un_openl;
		vp = un->un_lowervp;
	}
	ap->a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_close), ap));
}

/*
 * Check access permission on the union vnode.
 * The access check being enforced is to check
 * against both the underlying vnode, and any
 * copied vnode.  This ensures that no additional
 * file permissions are given away simply because
 * the user caused an implicit file copy.
 */
static int
union_access(ap)
	struct vop_access_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct thread *td = ap->a_td;
	int error = EACCES;
	struct vnode *vp;

	/*
	 * Disallow write attempts on filesystems mounted read-only.
	 */
	if ((ap->a_mode & VWRITE) && 
	    (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (ap->a_vp->v_type) {
		case VREG: 
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}

	if ((vp = union_lock_upper(un, td)) != NULLVP) {
		ap->a_vp = vp;
		error = VCALL(vp, VOFFSET(vop_access), ap);
		union_unlock_upper(vp, td);
		return(error);
	}

	if ((vp = un->un_lowervp) != NULLVP) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		ap->a_vp = vp;

		/*
		 * Remove VWRITE from a_mode if our mount point is RW, because
		 * we want to allow writes and lowervp may be read-only.
		 */
		if ((un->un_vnode->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ap->a_mode &= ~VWRITE;

		error = VCALL(vp, VOFFSET(vop_access), ap);
		if (error == 0) {
			struct union_mount *um;

			um = MOUNTTOUNIONMOUNT(un->un_vnode->v_mount);

			if (um->um_op == UNMNT_BELOW) {
				ap->a_cred = um->um_cred;
				error = VCALL(vp, VOFFSET(vop_access), ap);
			}
		}
		VOP_UNLOCK(vp, 0, td);
	}
	return(error);
}

/*
 * We handle getattr only to change the fsid and
 * track object sizes
 *
 * It's not clear whether VOP_GETATTR is to be
 * called with the vnode locked or not.  stat() calls
 * it with (vp) locked, and fstat() calls it with
 * (vp) unlocked. 
 *
 * Because of this we cannot use our normal locking functions
 * if we do not intend to lock the main a_vp node.  At the moment
 * we are running without any specific locking at all, but beware
 * to any programmer that care must be taken if locking is added
 * to this function.
 */

static int
union_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	int error;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp;
	struct vattr *vap;
	struct vattr va;

	/*
	 * Some programs walk the filesystem hierarchy by counting
	 * links to directories to avoid stat'ing all the time.
	 * This means the link count on directories needs to be "correct".
	 * The only way to do that is to call getattr on both layers
	 * and fix up the link count.  The link count will not necessarily
	 * be accurate but will be large enough to defeat the tree walkers.
	 */

	vap = ap->a_vap;

	if ((vp = un->un_uppervp) != NULLVP) {
		error = VOP_GETATTR(vp, vap, ap->a_cred, ap->a_td);
		if (error)
			return (error);
		/* XXX isn't this dangerous without a lock? */
		union_newsize(ap->a_vp, vap->va_size, VNOVAL);
	}

	if (vp == NULLVP) {
		vp = un->un_lowervp;
	} else if (vp->v_type == VDIR && un->un_lowervp != NULLVP) {
		vp = un->un_lowervp;
		vap = &va;
	} else {
		vp = NULLVP;
	}

	if (vp != NULLVP) {
		error = VOP_GETATTR(vp, vap, ap->a_cred, ap->a_td);
		if (error)
			return (error);
		/* XXX isn't this dangerous without a lock? */
		union_newsize(ap->a_vp, VNOVAL, vap->va_size);
	}

	if ((vap != ap->a_vap) && (vap->va_type == VDIR))
		ap->a_vap->va_nlink += vap->va_nlink;
	return (0);
}

static int
union_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct thread *td = ap->a_td;
	struct vattr *vap = ap->a_vap;
	struct vnode *uppervp;
	int error;

	/*
	 * Disallow write attempts on filesystems mounted read-only.
	 */
	if ((ap->a_vp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	     vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	     vap->va_mtime.tv_sec != VNOVAL || 
	     vap->va_mode != (mode_t)VNOVAL)) {
		return (EROFS);
	}

	/*
	 * Handle case of truncating lower object to zero size
	 * by creating a zero length upper object.  This is to
	 * handle the case of open with O_TRUNC and O_CREAT.
	 */
	if (un->un_uppervp == NULLVP && (un->un_lowervp->v_type == VREG)) {
		error = union_copyup(un, (ap->a_vap->va_size != 0),
			    ap->a_cred, ap->a_td);
		if (error)
			return (error);
	}

	/*
	 * Try to set attributes in upper layer,
	 * otherwise return read-only filesystem error.
	 */
	error = EROFS;
	if ((uppervp = union_lock_upper(un, td)) != NULLVP) {
		error = VOP_SETATTR(un->un_uppervp, ap->a_vap,
					ap->a_cred, ap->a_td);
		if ((error == 0) && (ap->a_vap->va_size != VNOVAL))
			union_newsize(ap->a_vp, ap->a_vap->va_size, VNOVAL);
		union_unlock_upper(uppervp, td);
	}
	return (error);
}

static int
union_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct thread *td = ap->a_uio->uio_td;
	struct vnode *uvp;
	int error;

	uvp = union_lock_other(un, td);
	KASSERT(uvp != NULL, ("union_read: backing vnode missing!"));

	error = VOP_READ(uvp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	union_unlock_other(uvp, td);

	/*
	 * XXX
	 * Perhaps the size of the underlying object has changed under
	 * our feet.  Take advantage of the offset information present
	 * in the uio structure.
	 */
	if (error == 0) {
		struct union_node *un = VTOUNION(ap->a_vp);
		off_t cur = ap->a_uio->uio_offset;

		if (uvp == un->un_uppervp) {
			if (cur > un->un_uppersz)
				union_newsize(ap->a_vp, cur, VNOVAL);
		} else {
			if (cur > un->un_lowersz)
				union_newsize(ap->a_vp, VNOVAL, cur);
		}
	}
	return (error);
}

static int
union_write(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct thread *td = ap->a_uio->uio_td;
	struct vnode *uppervp;
	int error;

	if ((uppervp = union_lock_upper(un, td)) == NULLVP)
		panic("union: missing upper layer in write");

	error = VOP_WRITE(uppervp, ap->a_uio, ap->a_ioflag, ap->a_cred);

	/*
	 * The size of the underlying object may be changed by the
	 * write.
	 */
	if (error == 0) {
		off_t cur = ap->a_uio->uio_offset;

		if (cur > un->un_uppersz)
			union_newsize(ap->a_vp, cur, VNOVAL);
	}
	union_unlock_upper(uppervp, td);
	return (error);
}

static int
union_lease(ap)
	struct vop_lease_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
		struct ucred *a_cred;
		int a_flag;
	} */ *ap;
{
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_lease), ap));
}

static int
union_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_ioctl), ap));
}

static int
union_poll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_poll), ap));
}

static int
union_revoke(ap)
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (UPPERVP(vp))
		VOP_REVOKE(UPPERVP(vp), ap->a_flags);
	if (LOWERVP(vp))
		VOP_REVOKE(LOWERVP(vp), ap->a_flags);
	vgone(vp);
	return (0);
}

static int
union_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	int error = 0;
	struct thread *td = ap->a_td;
	struct vnode *targetvp;
	struct union_node *un = VTOUNION(ap->a_vp);

	if ((targetvp = union_lock_other(un, td)) != NULLVP) {
		error = VOP_FSYNC(targetvp, ap->a_cred, ap->a_waitfor, td);
		union_unlock_other(targetvp, td);
	}

	return (error);
}

/*
 *	union_remove:
 *
 *	Remove the specified cnp.  The dvp and vp are passed to us locked
 *	and must remain locked on return.
 */

static int
union_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_vp);
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	struct vnode *uppervp;
	struct vnode *upperdvp;
	int error;

	if ((upperdvp = union_lock_upper(dun, td)) == NULLVP)
		panic("union remove: null upper vnode");

	if ((uppervp = union_lock_upper(un, td)) != NULLVP) {
		if (union_dowhiteout(un, cnp->cn_cred, td))
			cnp->cn_flags |= DOWHITEOUT;
		error = VOP_REMOVE(upperdvp, uppervp, cnp);
#if 0
		/* XXX */
		if (!error)
			union_removed_upper(un);
#endif
		union_unlock_upper(uppervp, td);
	} else {
		error = union_mkwhiteout(
			    MOUNTTOUNIONMOUNT(ap->a_dvp->v_mount),
			    upperdvp, ap->a_cnp, un->un_path);
	}
	union_unlock_upper(upperdvp, td);
	return (error);
}

/*
 *	union_link:
 *
 *	tdvp and vp will be locked on entry.
 *	tdvp and vp should remain locked on return.
 */

static int
union_link(ap)
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	struct union_node *dun = VTOUNION(ap->a_tdvp);
	struct vnode *vp;
	struct vnode *tdvp;
	int error = 0;

	if (ap->a_tdvp->v_op != ap->a_vp->v_op) {
		vp = ap->a_vp;
	} else {
		struct union_node *tun = VTOUNION(ap->a_vp);

		if (tun->un_uppervp == NULLVP) {
#if 0
			if (dun->un_uppervp == tun->un_dirvp) {
				if (dun->un_flags & UN_ULOCK) {
					dun->un_flags &= ~UN_ULOCK;
					VOP_UNLOCK(dun->un_uppervp, 0, td);
				}
			}
#endif
			error = union_copyup(tun, 1, cnp->cn_cred, td);
#if 0
			if (dun->un_uppervp == tun->un_dirvp) {
				vn_lock(dun->un_uppervp,
					    LK_EXCLUSIVE | LK_RETRY, td);
				dun->un_flags |= UN_ULOCK;
			}
#endif
			if (error)
				return (error);
		}
		vp = tun->un_uppervp;
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	}

	/*
	 * Make sure upper is locked, then unlock the union directory we were 
	 * called with to avoid a deadlock while we are calling VOP_LINK() on 
	 * the upper (with tdvp locked and vp not locked).  Our ap->a_tdvp
	 * is expected to be locked on return.
	 */

	if ((tdvp = union_lock_upper(dun, td)) == NULLVP)
		return (EROFS);

	VOP_UNLOCK(ap->a_tdvp, 0, td);		/* unlock calling node */
	error = VOP_LINK(tdvp, vp, cnp);	/* call link on upper */

	/*
	 * Unlock tun->un_uppervp if we locked it above.
	 */
	if (ap->a_tdvp->v_op == ap->a_vp->v_op)
		VOP_UNLOCK(vp, 0, td);
	/*
	 * We have to unlock tdvp prior to relocking our calling node in
	 * order to avoid a deadlock.  We also have to unlock ap->a_vp
	 * before relocking the directory, but then we have to relock
	 * ap->a_vp as our caller expects.
	 */
	VOP_UNLOCK(ap->a_vp, 0, td);
	union_unlock_upper(tdvp, td);
	vn_lock(ap->a_tdvp, LK_EXCLUSIVE | LK_RETRY, td);
	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY, td);
	return (error);
}

static int
union_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	int error;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *tvp = ap->a_tvp;

	/*
	 * Figure out what fdvp to pass to our upper or lower vnode.  If we
	 * replace the fdvp, release the original one and ref the new one.
	 */

	if (fdvp->v_op == union_vnodeop_p) {	/* always true */
		struct union_node *un = VTOUNION(fdvp);
		if (un->un_uppervp == NULLVP) {
			/*
			 * this should never happen in normal
			 * operation but might if there was
			 * a problem creating the top-level shadow
			 * directory.
			 */
			error = EXDEV;
			goto bad;
		}
		fdvp = un->un_uppervp;
		VREF(fdvp);
		vrele(ap->a_fdvp);
	}

	/*
	 * Figure out what fvp to pass to our upper or lower vnode.  If we
	 * replace the fvp, release the original one and ref the new one.
	 */

	if (fvp->v_op == union_vnodeop_p) {	/* always true */
		struct union_node *un = VTOUNION(fvp);
#if 0
		struct union_mount *um = MOUNTTOUNIONMOUNT(fvp->v_mount);
#endif

		if (un->un_uppervp == NULLVP) {
			switch(fvp->v_type) {
			case VREG:
				vn_lock(un->un_vnode, LK_EXCLUSIVE | LK_RETRY, ap->a_fcnp->cn_thread);
				error = union_copyup(un, 1, ap->a_fcnp->cn_cred, ap->a_fcnp->cn_thread);
				VOP_UNLOCK(un->un_vnode, 0, ap->a_fcnp->cn_thread);
				if (error)
					goto bad;
				break;
			case VDIR:
				/*
				 * XXX not yet.
				 *
				 * There is only one way to rename a directory
				 * based in the lowervp, and that is to copy
				 * the entire directory hierarchy.  Otherwise
				 * it would not last across a reboot.
				 */
#if 0
				vrele(fvp);
				fvp = NULL;
				vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY, ap->a_fcnp->cn_thread);
				error = union_mkshadow(um, fdvp, 
					    ap->a_fcnp, &un->un_uppervp);
				VOP_UNLOCK(fdvp, 0, ap->a_fcnp->cn_thread);
				if (un->un_uppervp)
					VOP_UNLOCK(un->un_uppervp, 0, ap->a_fcnp->cn_thread);
				if (error)
					goto bad;
				break;
#endif
			default:
				error = EXDEV;
				goto bad;
			}
		}

		if (un->un_lowervp != NULLVP)
			ap->a_fcnp->cn_flags |= DOWHITEOUT;
		fvp = un->un_uppervp;
		VREF(fvp);
		vrele(ap->a_fvp);
	}

	/*
	 * Figure out what tdvp (destination directory) to pass to the
	 * lower level.  If we replace it with uppervp, we need to vput the 
	 * old one.  The exclusive lock is transfered to what we will pass
	 * down in the VOP_RENAME() and we replace uppervp with a simple
	 * reference.
	 */

	if (tdvp->v_op == union_vnodeop_p) {
		struct union_node *un = VTOUNION(tdvp);

		if (un->un_uppervp == NULLVP) {
			/*
			 * This should never happen in normal
			 * operation but might if there was
			 * a problem creating the top-level shadow
			 * directory.
			 */
			error = EXDEV;
			goto bad;
		}

		/*
		 * New tdvp is a lock and reference on uppervp.
		 * Put away the old tdvp.
		 */
		tdvp = union_lock_upper(un, ap->a_tcnp->cn_thread);
		vput(ap->a_tdvp);
	}

	/*
	 * Figure out what tvp (destination file) to pass to the
	 * lower level.
	 *
	 * If the uppervp file does not exist, put away the (wrong)
	 * file and change tvp to NULL.
	 */

	if (tvp != NULLVP && tvp->v_op == union_vnodeop_p) {
		struct union_node *un = VTOUNION(tvp);

		tvp = union_lock_upper(un, ap->a_tcnp->cn_thread);
		vput(ap->a_tvp);
		/* note: tvp may be NULL */
	}

	/*
	 * VOP_RENAME() releases/vputs prior to returning, so we have no
	 * cleanup to do.
	 */

	return (VOP_RENAME(fdvp, fvp, ap->a_fcnp, tdvp, tvp, ap->a_tcnp));

	/*
	 * Error.  We still have to release / vput the various elements.
	 */

bad:
	vrele(fdvp);
	if (fvp)
		vrele(fvp);
	vput(tdvp);
	if (tvp != NULLVP) {
		if (tvp != tdvp)
			vput(tvp);
		else
			vrele(tvp);
	}
	return (error);
}

static int
union_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	struct vnode *upperdvp;
	int error = EROFS;

	if ((upperdvp = union_lock_upper(dun, td)) != NULLVP) {
		struct vnode *vp;

		error = VOP_MKDIR(upperdvp, &vp, cnp, ap->a_vap);
		union_unlock_upper(upperdvp, td);

		if (error == 0) {
			VOP_UNLOCK(vp, 0, td);
			UDEBUG(("ALLOCVP-2 FROM %p REFS %d\n", vp, vrefcnt(vp)));
			error = union_allocvp(ap->a_vpp, ap->a_dvp->v_mount,
				ap->a_dvp, NULLVP, cnp, vp, NULLVP, 1);
			UDEBUG(("ALLOCVP-2B FROM %p REFS %d\n", *ap->a_vpp, vrefcnt(vp)));
		}
	}
	return (error);
}

static int
union_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_vp);
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	struct vnode *upperdvp;
	struct vnode *uppervp;
	int error;

	if ((upperdvp = union_lock_upper(dun, td)) == NULLVP)
		panic("union rmdir: null upper vnode");

	if ((uppervp = union_lock_upper(un, td)) != NULLVP) {
		if (union_dowhiteout(un, cnp->cn_cred, td))
			cnp->cn_flags |= DOWHITEOUT;
		error = VOP_RMDIR(upperdvp, uppervp, ap->a_cnp);
		union_unlock_upper(uppervp, td);
	} else {
		error = union_mkwhiteout(
			    MOUNTTOUNIONMOUNT(ap->a_dvp->v_mount),
			    dun->un_uppervp, ap->a_cnp, un->un_path);
	}
	union_unlock_upper(upperdvp, td);
	return (error);
}

/*
 *	union_symlink:
 *
 *	dvp is locked on entry and remains locked on return.  a_vpp is garbage
 *	(unused).
 */

static int
union_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;
	struct thread *td = cnp->cn_thread;
	struct vnode *dvp;
	int error = EROFS;

	if ((dvp = union_lock_upper(dun, td)) != NULLVP) {
		error = VOP_SYMLINK(dvp, ap->a_vpp, cnp, ap->a_vap,
			    ap->a_target);
		union_unlock_upper(dvp, td);
	}
	return (error);
}

/*
 * union_readdir ()works in concert with getdirentries() and
 * readdir(3) to provide a list of entries in the unioned
 * directories.  getdirentries()  is responsible for walking
 * down the union stack.  readdir(3) is responsible for
 * eliminating duplicate names from the returned data stream.
 */
static int
union_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct thread *td = ap->a_uio->uio_td;
	struct vnode *uvp;
	int error = 0;

	if ((uvp = union_lock_upper(un, td)) != NULLVP) {
		ap->a_vp = uvp;
		error = VCALL(uvp, VOFFSET(vop_readdir), ap);
		union_unlock_upper(uvp, td);
	}
	return(error);
}

static int
union_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	int error;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct uio *uio = ap->a_uio;
	struct thread *td = uio->uio_td;
	struct vnode *vp;

	vp = union_lock_other(un, td);
	KASSERT(vp != NULL, ("union_readlink: backing vnode missing!"));

	ap->a_vp = vp;
	error = VCALL(vp, VOFFSET(vop_readlink), ap);
	union_unlock_other(vp, td);

	return (error);
}

static int
union_getwritemount(ap)
	struct vop_getwritemount_args /* {
		struct vnode *a_vp;
		struct mount **a_mpp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *uvp = UPPERVP(vp);

	if (uvp == NULL) {
		VI_LOCK(vp);
		if (vp->v_iflag & VI_FREE) {
			VI_UNLOCK(vp);
			return (EOPNOTSUPP);
		}
		VI_UNLOCK(vp);
		return (EACCES);
	}
	return(VOP_GETWRITEMOUNT(uvp, ap->a_mpp));
}

/*
 *	union_inactive:
 *
 *	Called with the vnode locked.  We are expected to unlock the vnode.
 */

static int
union_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct thread *td = ap->a_td;
	struct union_node *un = VTOUNION(vp);

	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our union_node is in the
	 * cache and reusable.
	 *
	 */

	if (un->un_dircache != NULL)
		union_dircache_free(un);

#if 0
	if ((un->un_flags & UN_ULOCK) && un->un_uppervp) {
		un->un_flags &= ~UN_ULOCK;
		VOP_UNLOCK(un->un_uppervp, 0, td);
	}
#endif

	VOP_UNLOCK(vp, 0, td);

	if ((un->un_flags & UN_CACHED) == 0)
		vgone(vp);

	return (0);
}

static int
union_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	union_freevp(ap->a_vp);

	return (0);
}

/*
 * unionvp do not hold a VM object and there is no need to create one for
 * upper or lower vp because it is done in the union_open()
 */
static int
union_createvobject(ap)
	struct vop_createvobject_args /* {
		struct vnode *vp;
		struct ucred *cred;
		struct thread *td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	vp->v_vflag |= VV_OBJBUF;
	return (0);
}

/*
 * We have nothing to destroy and this operation shouldn't be bypassed.
 */
static int
union_destroyvobject(ap)
	struct vop_destroyvobject_args /* {
		struct vnode *vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	vp->v_vflag &= ~VV_OBJBUF;
	return (0);
}

/*
 * Get VM object from the upper or lower vp
 */
static int
union_getvobject(ap)
	struct vop_getvobject_args /* {
		struct vnode *vp;
		struct vm_object **objpp;
	} */ *ap;
{
	struct vnode *ovp = OTHERVP(ap->a_vp);

	if (ovp == NULL)
		return EINVAL;
	return (VOP_GETVOBJECT(ovp, ap->a_objpp));
}

static int
union_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	printf("\tvp=%p, uppervp=%p, lowervp=%p\n",
	       vp, UPPERVP(vp), LOWERVP(vp));
	if (UPPERVP(vp) != NULLVP)
		vprint("union: upper", UPPERVP(vp));
	if (LOWERVP(vp) != NULLVP)
		vprint("union: lower", LOWERVP(vp));

	return (0);
}

static int
union_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{
	int error;
	struct thread *td = curthread;		/* XXX */
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp;

	vp = union_lock_other(un, td);
	KASSERT(vp != NULL, ("union_pathconf: backing vnode missing!"));

	ap->a_vp = vp;
	error = VCALL(vp, VOFFSET(vop_pathconf), ap);
	union_unlock_other(vp, td);

	return (error);
}

static int
union_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{
	register struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_advlock), ap));
}


/*
 * XXX - vop_strategy must be hand coded because it has no
 * YYY - and it is not coherent with anything
 *
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
static int
union_strategy(ap)
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	struct vnode *othervp = OTHERVP(bp->b_vp);

	KASSERT(ap->a_vp == ap->a_bp->b_vp, ("%s(%p != %p)",
	    __func__, ap->a_vp, ap->a_bp->b_vp));
#ifdef DIAGNOSTIC
	if (othervp == NULLVP)
		panic("union_strategy: nil vp");
	if ((bp->b_iocmd == BIO_WRITE) &&
	    (othervp == LOWERVP(bp->b_vp)))
		panic("union_strategy: writing to lowervp");
#endif
	return (VOP_STRATEGY(othervp, bp));
}

/*
 * Global vfs data structures
 */
vop_t **union_vnodeop_p;
static struct vnodeopv_entry_desc union_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) union_access },
	{ &vop_advlock_desc,		(vop_t *) union_advlock },
	{ &vop_bmap_desc,		(vop_t *) vop_eopnotsupp },
	{ &vop_close_desc,		(vop_t *) union_close },
	{ &vop_create_desc,		(vop_t *) union_create },
	{ &vop_createvobject_desc,	(vop_t *) union_createvobject },
	{ &vop_destroyvobject_desc,	(vop_t *) union_destroyvobject },
	{ &vop_fsync_desc,		(vop_t *) union_fsync },
	{ &vop_getattr_desc,		(vop_t *) union_getattr },
	{ &vop_getvobject_desc,		(vop_t *) union_getvobject },
	{ &vop_inactive_desc,		(vop_t *) union_inactive },
	{ &vop_ioctl_desc,		(vop_t *) union_ioctl },
	{ &vop_lease_desc,		(vop_t *) union_lease },
	{ &vop_link_desc,		(vop_t *) union_link },
	{ &vop_lookup_desc,		(vop_t *) union_lookup },
	{ &vop_mkdir_desc,		(vop_t *) union_mkdir },
	{ &vop_mknod_desc,		(vop_t *) union_mknod },
	{ &vop_open_desc,		(vop_t *) union_open },
	{ &vop_pathconf_desc,		(vop_t *) union_pathconf },
	{ &vop_poll_desc,		(vop_t *) union_poll },
	{ &vop_print_desc,		(vop_t *) union_print },
	{ &vop_read_desc,		(vop_t *) union_read },
	{ &vop_readdir_desc,		(vop_t *) union_readdir },
	{ &vop_readlink_desc,		(vop_t *) union_readlink },
	{ &vop_getwritemount_desc,	(vop_t *) union_getwritemount },
	{ &vop_reclaim_desc,		(vop_t *) union_reclaim },
	{ &vop_remove_desc,		(vop_t *) union_remove },
	{ &vop_rename_desc,		(vop_t *) union_rename },
	{ &vop_revoke_desc,		(vop_t *) union_revoke },
	{ &vop_rmdir_desc,		(vop_t *) union_rmdir },
	{ &vop_setattr_desc,		(vop_t *) union_setattr },
	{ &vop_strategy_desc,		(vop_t *) union_strategy },
	{ &vop_symlink_desc,		(vop_t *) union_symlink },
	{ &vop_whiteout_desc,		(vop_t *) union_whiteout },
	{ &vop_write_desc,		(vop_t *) union_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc union_vnodeop_opv_desc =
	{ &union_vnodeop_p, union_vnodeop_entries };

VNODEOP_SET(union_vnodeop_opv_desc);
