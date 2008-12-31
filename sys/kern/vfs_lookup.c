/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)vfs_lookup.c	8.4 (Berkeley) 2/16/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/vfs_lookup.c,v 1.102.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_ktrace.h"
#include "opt_mac.h"
#include "opt_vfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/uma.h>

#define	NAMEI_DIAGNOSTIC 1
#undef NAMEI_DIAGNOSTIC

/*
 * Allocation zone for namei
 */
uma_zone_t namei_zone;
/*
 * Placeholder vnode for mp traversal
 */
static struct vnode *vp_crossmp;

static void
nameiinit(void *dummy __unused)
{
	int error;

	namei_zone = uma_zcreate("NAMEI", MAXPATHLEN, NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	error = getnewvnode("crossmp", NULL, &dead_vnodeops, &vp_crossmp);
	if (error != 0)
		panic("nameiinit: getnewvnode");
	vp_crossmp->v_vnlock->lk_flags &= ~LK_NOSHARE;
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_SECOND, nameiinit, NULL);

#ifdef LOOKUP_SHARED
static int lookup_shared = 1;
#else
static int lookup_shared = 0;
#endif
SYSCTL_INT(_vfs, OID_AUTO, lookup_shared, CTLFLAG_RW, &lookup_shared, 0,
    "Enables/Disables shared locks for path name translation");

/*
 * Convert a pathname into a pointer to a locked vnode.
 *
 * The FOLLOW flag is set when symbolic links are to be followed
 * when they occur at the end of the name translation process.
 * Symbolic links are always followed for all other pathname
 * components other than the last.
 *
 * The segflg defines whether the name is to be copied from user
 * space or kernel space.
 *
 * Overall outline of namei:
 *
 *	copy in name
 *	get starting directory
 *	while (!done && !error) {
 *		call lookup to search path.
 *		if symbolic link, massage name in buffer and continue
 *	}
 */
int
namei(struct nameidata *ndp)
{
	struct filedesc *fdp;	/* pointer to file descriptor state */
	char *cp;		/* pointer into pathname argument */
	struct vnode *dp;	/* the directory we are searching */
	struct iovec aiov;		/* uio for reading symbolic links */
	struct uio auio;
	int error, linklen;
	struct componentname *cnp = &ndp->ni_cnd;
	struct thread *td = cnp->cn_thread;
	struct proc *p = td->td_proc;
	int vfslocked;

	KASSERT((cnp->cn_flags & MPSAFE) != 0 || mtx_owned(&Giant) != 0,
	    ("NOT MPSAFE and Giant not held"));
	ndp->ni_cnd.cn_cred = ndp->ni_cnd.cn_thread->td_ucred;
	KASSERT(cnp->cn_cred && p, ("namei: bad cred/proc"));
	KASSERT((cnp->cn_nameiop & (~OPMASK)) == 0,
	    ("namei: nameiop contaminated with flags"));
	KASSERT((cnp->cn_flags & OPMASK) == 0,
	    ("namei: flags contaminated with nameiops"));
	if (!lookup_shared)
		cnp->cn_flags &= ~LOCKSHARED;
	fdp = p->p_fd;

	/*
	 * Get a buffer for the name to be translated, and copy the
	 * name into the buffer.
	 */
	if ((cnp->cn_flags & HASBUF) == 0)
		cnp->cn_pnbuf = uma_zalloc(namei_zone, M_WAITOK);
	if (ndp->ni_segflg == UIO_SYSSPACE)
		error = copystr(ndp->ni_dirp, cnp->cn_pnbuf,
			    MAXPATHLEN, (size_t *)&ndp->ni_pathlen);
	else
		error = copyinstr(ndp->ni_dirp, cnp->cn_pnbuf,
			    MAXPATHLEN, (size_t *)&ndp->ni_pathlen);

	/* If we are auditing the kernel pathname, save the user pathname. */
	if (cnp->cn_flags & AUDITVNODE1)
		AUDIT_ARG(upath, td, cnp->cn_pnbuf, ARG_UPATH1);
	if (cnp->cn_flags & AUDITVNODE2)
		AUDIT_ARG(upath, td, cnp->cn_pnbuf, ARG_UPATH2);

	/*
	 * Don't allow empty pathnames.
	 */
	if (!error && *cnp->cn_pnbuf == '\0')
		error = ENOENT;

	if (error) {
		uma_zfree(namei_zone, cnp->cn_pnbuf);
#ifdef DIAGNOSTIC
		cnp->cn_pnbuf = NULL;
		cnp->cn_nameptr = NULL;
#endif
		ndp->ni_vp = NULL;
		return (error);
	}
	ndp->ni_loopcnt = 0;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_NAMEI)) {
		KASSERT(cnp->cn_thread == curthread,
		    ("namei not using curthread"));
		ktrnamei(cnp->cn_pnbuf);
	}
#endif

	/*
	 * Get starting point for the translation.
	 */
	FILEDESC_SLOCK(fdp);
	ndp->ni_rootdir = fdp->fd_rdir;
	ndp->ni_topdir = fdp->fd_jdir;

	dp = fdp->fd_cdir;
	vfslocked = VFS_LOCK_GIANT(dp->v_mount);
	VREF(dp);
	FILEDESC_SUNLOCK(fdp);
	for (;;) {
		/*
		 * Check if root directory should replace current directory.
		 * Done at start of translation and after symbolic link.
		 */
		cnp->cn_nameptr = cnp->cn_pnbuf;
		if (*(cnp->cn_nameptr) == '/') {
			vrele(dp);
			VFS_UNLOCK_GIANT(vfslocked);
			while (*(cnp->cn_nameptr) == '/') {
				cnp->cn_nameptr++;
				ndp->ni_pathlen--;
			}
			dp = ndp->ni_rootdir;
			vfslocked = VFS_LOCK_GIANT(dp->v_mount);
			VREF(dp);
		}
		if (vfslocked)
			ndp->ni_cnd.cn_flags |= GIANTHELD;
		ndp->ni_startdir = dp;
		error = lookup(ndp);
		if (error) {
			uma_zfree(namei_zone, cnp->cn_pnbuf);
#ifdef DIAGNOSTIC
			cnp->cn_pnbuf = NULL;
			cnp->cn_nameptr = NULL;
#endif
			return (error);
		}
		vfslocked = (ndp->ni_cnd.cn_flags & GIANTHELD) != 0;
		ndp->ni_cnd.cn_flags &= ~GIANTHELD;
		/*
		 * Check for symbolic link
		 */
		if ((cnp->cn_flags & ISSYMLINK) == 0) {
			if ((cnp->cn_flags & (SAVENAME | SAVESTART)) == 0) {
				uma_zfree(namei_zone, cnp->cn_pnbuf);
#ifdef DIAGNOSTIC
				cnp->cn_pnbuf = NULL;
				cnp->cn_nameptr = NULL;
#endif
			} else
				cnp->cn_flags |= HASBUF;

			if ((cnp->cn_flags & MPSAFE) == 0) {
				VFS_UNLOCK_GIANT(vfslocked);
			} else if (vfslocked)
				ndp->ni_cnd.cn_flags |= GIANTHELD;
			return (0);
		}
		if (ndp->ni_loopcnt++ >= MAXSYMLINKS) {
			error = ELOOP;
			break;
		}
#ifdef MAC
		if ((cnp->cn_flags & NOMACCHECK) == 0) {
			error = mac_check_vnode_readlink(td->td_ucred,
			    ndp->ni_vp);
			if (error)
				break;
		}
#endif
		if (ndp->ni_pathlen > 1)
			cp = uma_zalloc(namei_zone, M_WAITOK);
		else
			cp = cnp->cn_pnbuf;
		aiov.iov_base = cp;
		aiov.iov_len = MAXPATHLEN;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_td = (struct thread *)0;
		auio.uio_resid = MAXPATHLEN;
		error = VOP_READLINK(ndp->ni_vp, &auio, cnp->cn_cred);
		if (error) {
			if (ndp->ni_pathlen > 1)
				uma_zfree(namei_zone, cp);
			break;
		}
		linklen = MAXPATHLEN - auio.uio_resid;
		if (linklen == 0) {
			if (ndp->ni_pathlen > 1)
				uma_zfree(namei_zone, cp);
			error = ENOENT;
			break;
		}
		if (linklen + ndp->ni_pathlen >= MAXPATHLEN) {
			if (ndp->ni_pathlen > 1)
				uma_zfree(namei_zone, cp);
			error = ENAMETOOLONG;
			break;
		}
		if (ndp->ni_pathlen > 1) {
			bcopy(ndp->ni_next, cp + linklen, ndp->ni_pathlen);
			uma_zfree(namei_zone, cnp->cn_pnbuf);
			cnp->cn_pnbuf = cp;
		} else
			cnp->cn_pnbuf[linklen] = '\0';
		ndp->ni_pathlen += linklen;
		vput(ndp->ni_vp);
		dp = ndp->ni_dvp;
	}
	uma_zfree(namei_zone, cnp->cn_pnbuf);
#ifdef DIAGNOSTIC
	cnp->cn_pnbuf = NULL;
	cnp->cn_nameptr = NULL;
#endif
	vput(ndp->ni_vp);
	ndp->ni_vp = NULL;
	vrele(ndp->ni_dvp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

static int
compute_cn_lkflags(struct mount *mp, int lkflags)
{
	if (mp == NULL || 
	    ((lkflags & LK_SHARED) && !(mp->mnt_kern_flag & MNTK_LOOKUP_SHARED))) {
		lkflags &= ~LK_SHARED;
		lkflags |= LK_EXCLUSIVE;
	}
	return lkflags;
}

/*
 * Search a pathname.
 * This is a very central and rather complicated routine.
 *
 * The pathname is pointed to by ni_ptr and is of length ni_pathlen.
 * The starting directory is taken from ni_startdir. The pathname is
 * descended until done, or a symbolic link is encountered. The variable
 * ni_more is clear if the path is completed; it is set to one if a
 * symbolic link needing interpretation is encountered.
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it, the parent directory is returned
 * locked. If flag has WANTPARENT or'ed into it, the parent directory is
 * returned unlocked. Otherwise the parent directory is not returned. If
 * the target of the pathname exists and LOCKLEAF is or'ed into the flag
 * the target is returned locked, otherwise it is returned unlocked.
 * When creating or renaming and LOCKPARENT is specified, the target may not
 * be ".".  When deleting and LOCKPARENT is specified, the target may be ".".
 *
 * Overall outline of lookup:
 *
 * dirloop:
 *	identify next component of name at ndp->ni_ptr
 *	handle degenerate case where name is null string
 *	if .. and crossing mount points and on mounted filesys, find parent
 *	call VOP_LOOKUP routine for next component name
 *	    directory vnode returned in ni_dvp, unlocked unless LOCKPARENT set
 *	    component vnode returned in ni_vp (if it exists), locked.
 *	if result vnode is mounted on and crossing mount points,
 *	    find mounted on vnode
 *	if more components of name, do next level at dirloop
 *	return the answer in ni_vp, locked if LOCKLEAF set
 *	    if LOCKPARENT set, return locked parent in ni_dvp
 *	    if WANTPARENT set, return unlocked parent in ni_dvp
 */
int
lookup(struct nameidata *ndp)
{
	char *cp;		/* pointer into pathname argument */
	struct vnode *dp = 0;	/* the directory we are searching */
	struct vnode *tdp;		/* saved dp */
	struct mount *mp;		/* mount table entry */
	int docache;			/* == 0 do not cache last component */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int rdonly;			/* lookup read-only flag bit */
	int trailing_slash;
	int error = 0;
	int dpunlocked = 0;		/* dp has already been unlocked */
	struct componentname *cnp = &ndp->ni_cnd;
	struct thread *td = cnp->cn_thread;
	int vfslocked;			/* VFS Giant state for child */
	int dvfslocked;			/* VFS Giant state for parent */
	int tvfslocked;
	int lkflags_save;
	
	/*
	 * Setup: break out flag bits into variables.
	 */
	dvfslocked = (ndp->ni_cnd.cn_flags & GIANTHELD) != 0;
	vfslocked = 0;
	ndp->ni_cnd.cn_flags &= ~GIANTHELD;
	wantparent = cnp->cn_flags & (LOCKPARENT | WANTPARENT);
	KASSERT(cnp->cn_nameiop == LOOKUP || wantparent,
	    ("CREATE, DELETE, RENAME require LOCKPARENT or WANTPARENT."));
	docache = (cnp->cn_flags & NOCACHE) ^ NOCACHE;
	if (cnp->cn_nameiop == DELETE ||
	    (wantparent && cnp->cn_nameiop != CREATE &&
	     cnp->cn_nameiop != LOOKUP))
		docache = 0;
	rdonly = cnp->cn_flags & RDONLY;
	cnp->cn_flags &= ~ISSYMLINK;
	ndp->ni_dvp = NULL;
	/*
	 * We use shared locks until we hit the parent of the last cn then
	 * we adjust based on the requesting flags.
	 */
	if (lookup_shared)
		cnp->cn_lkflags = LK_SHARED;
	else
		cnp->cn_lkflags = LK_EXCLUSIVE;
	dp = ndp->ni_startdir;
	ndp->ni_startdir = NULLVP;
	vn_lock(dp, compute_cn_lkflags(dp->v_mount, cnp->cn_lkflags | LK_RETRY), td);

dirloop:
	/*
	 * Search a new directory.
	 *
	 * The last component of the filename is left accessible via
	 * cnp->cn_nameptr for callers that need the name. Callers needing
	 * the name set the SAVENAME flag. When done, they assume
	 * responsibility for freeing the pathname buffer.
	 */
	cnp->cn_consume = 0;
	for (cp = cnp->cn_nameptr; *cp != 0 && *cp != '/'; cp++)
		continue;
	cnp->cn_namelen = cp - cnp->cn_nameptr;
	if (cnp->cn_namelen > NAME_MAX) {
		error = ENAMETOOLONG;
		goto bad;
	}
#ifdef NAMEI_DIAGNOSTIC
	{ char c = *cp;
	*cp = '\0';
	printf("{%s}: ", cnp->cn_nameptr);
	*cp = c; }
#endif
	ndp->ni_pathlen -= cnp->cn_namelen;
	ndp->ni_next = cp;

	/*
	 * Replace multiple slashes by a single slash and trailing slashes
	 * by a null.  This must be done before VOP_LOOKUP() because some
	 * fs's don't know about trailing slashes.  Remember if there were
	 * trailing slashes to handle symlinks, existing non-directories
	 * and non-existing files that won't be directories specially later.
	 */
	trailing_slash = 0;
	while (*cp == '/' && (cp[1] == '/' || cp[1] == '\0')) {
		cp++;
		ndp->ni_pathlen--;
		if (*cp == '\0') {
			trailing_slash = 1;
			*ndp->ni_next = '\0';	/* XXX for direnter() ... */
		}
	}
	ndp->ni_next = cp;

	cnp->cn_flags |= MAKEENTRY;
	if (*cp == '\0' && docache == 0)
		cnp->cn_flags &= ~MAKEENTRY;
	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[1] == '.' && cnp->cn_nameptr[0] == '.')
		cnp->cn_flags |= ISDOTDOT;
	else
		cnp->cn_flags &= ~ISDOTDOT;
	if (*ndp->ni_next == 0)
		cnp->cn_flags |= ISLASTCN;
	else
		cnp->cn_flags &= ~ISLASTCN;


	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. like "/." or ".".
	 */
	if (cnp->cn_nameptr[0] == '\0') {
		if (dp->v_type != VDIR) {
			error = ENOTDIR;
			goto bad;
		}
		if (cnp->cn_nameiop != LOOKUP) {
			error = EISDIR;
			goto bad;
		}
		if (wantparent) {
			ndp->ni_dvp = dp;
			VREF(dp);
		}
		ndp->ni_vp = dp;

		if (cnp->cn_flags & AUDITVNODE1)
			AUDIT_ARG(vnode, dp, ARG_VNODE1);
		else if (cnp->cn_flags & AUDITVNODE2)
			AUDIT_ARG(vnode, dp, ARG_VNODE2);

		if (!(cnp->cn_flags & (LOCKPARENT | LOCKLEAF)))
			VOP_UNLOCK(dp, 0, td);
		/* XXX This should probably move to the top of function. */
		if (cnp->cn_flags & SAVESTART)
			panic("lookup: SAVESTART");
		goto success;
	}

	/*
	 * Handle "..": four special cases.
	 * 1. Return an error if this is the last component of
	 *    the name and the operation is DELETE or RENAME.
	 * 2. If at root directory (e.g. after chroot)
	 *    or at absolute root directory
	 *    then ignore it so can't get out.
	 * 3. If this vnode is the root of a mounted
	 *    filesystem, then replace it with the
	 *    vnode which was mounted on so we take the
	 *    .. in the other filesystem.
	 * 4. If the vnode is the top directory of
	 *    the jail or chroot, don't let them out.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		if ((cnp->cn_flags & ISLASTCN) != 0 &&
		    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
			error = EINVAL;
			goto bad;
		}
		for (;;) {
			if (dp == ndp->ni_rootdir || 
			    dp == ndp->ni_topdir || 
			    dp == rootvnode ||
			    ((dp->v_vflag & VV_ROOT) != 0 &&
			     (cnp->cn_flags & NOCROSSMOUNT) != 0)) {
				ndp->ni_dvp = dp;
				ndp->ni_vp = dp;
				vfslocked = VFS_LOCK_GIANT(dp->v_mount);
				VREF(dp);
				goto nextname;
			}
			if ((dp->v_vflag & VV_ROOT) == 0)
				break;
			if (dp->v_iflag & VI_DOOMED) {	/* forced unmount */
				error = EBADF;
				goto bad;
			}
			tdp = dp;
			dp = dp->v_mount->mnt_vnodecovered;
			tvfslocked = dvfslocked;
			dvfslocked = VFS_LOCK_GIANT(dp->v_mount);
			VREF(dp);
			vput(tdp);
			VFS_UNLOCK_GIANT(tvfslocked);
			vn_lock(dp, compute_cn_lkflags(dp->v_mount, cnp->cn_lkflags | LK_RETRY), td);
		}
	}

	/*
	 * We now have a segment name to search for, and a directory to search.
	 */
unionlookup:
#ifdef MAC
	if ((cnp->cn_flags & NOMACCHECK) == 0) {
		error = mac_check_vnode_lookup(td->td_ucred, dp, cnp);
		if (error)
			goto bad;
	}
#endif
	ndp->ni_dvp = dp;
	ndp->ni_vp = NULL;
	ASSERT_VOP_LOCKED(dp, "lookup");
	VNASSERT(vfslocked == 0, dp, ("lookup: vfslocked %d", vfslocked));
	/*
	 * If we have a shared lock we may need to upgrade the lock for the
	 * last operation.
	 */
	if (dp != vp_crossmp &&
	    VOP_ISLOCKED(dp, td) == LK_SHARED &&
	    (cnp->cn_flags & ISLASTCN) && (cnp->cn_flags & LOCKPARENT))
		vn_lock(dp, LK_UPGRADE|LK_RETRY, td);
	/*
	 * If we're looking up the last component and we need an exclusive
	 * lock, adjust our lkflags.
	 */
	if ((cnp->cn_flags & (ISLASTCN|LOCKSHARED|LOCKLEAF)) ==
	    (ISLASTCN|LOCKLEAF))
		cnp->cn_lkflags = LK_EXCLUSIVE;
#ifdef NAMEI_DIAGNOSTIC
	vprint("lookup in", dp);
#endif
	lkflags_save = cnp->cn_lkflags;
	cnp->cn_lkflags = compute_cn_lkflags(dp->v_mount, cnp->cn_lkflags);
	if ((error = VOP_LOOKUP(dp, &ndp->ni_vp, cnp)) != 0) {
		cnp->cn_lkflags = lkflags_save;
		KASSERT(ndp->ni_vp == NULL, ("leaf should be empty"));
#ifdef NAMEI_DIAGNOSTIC
		printf("not found\n");
#endif
		if ((error == ENOENT) &&
		    (dp->v_vflag & VV_ROOT) && (dp->v_mount != NULL) &&
		    (dp->v_mount->mnt_flag & MNT_UNION)) {
			tdp = dp;
			dp = dp->v_mount->mnt_vnodecovered;
			tvfslocked = dvfslocked;
			dvfslocked = VFS_LOCK_GIANT(dp->v_mount);
			VREF(dp);
			vput(tdp);
			VFS_UNLOCK_GIANT(tvfslocked);
			vn_lock(dp, compute_cn_lkflags(dp->v_mount, cnp->cn_lkflags | LK_RETRY), td);
			goto unionlookup;
		}

		if (error != EJUSTRETURN)
			goto bad;
		/*
		 * If creating and at end of pathname, then can consider
		 * allowing file to be created.
		 */
		if (rdonly) {
			error = EROFS;
			goto bad;
		}
		if (*cp == '\0' && trailing_slash &&
		     !(cnp->cn_flags & WILLBEDIR)) {
			error = ENOENT;
			goto bad;
		}
		if ((cnp->cn_flags & LOCKPARENT) == 0)
			VOP_UNLOCK(dp, 0, td);
		/*
		 * This is a temporary assert to make sure I know what the
		 * behavior here was.
		 */
		KASSERT((cnp->cn_flags & (WANTPARENT|LOCKPARENT)) != 0,
		   ("lookup: Unhandled case."));
		/*
		 * We return with ni_vp NULL to indicate that the entry
		 * doesn't currently exist, leaving a pointer to the
		 * (possibly locked) directory vnode in ndp->ni_dvp.
		 */
		if (cnp->cn_flags & SAVESTART) {
			ndp->ni_startdir = ndp->ni_dvp;
			VREF(ndp->ni_startdir);
		}
		goto success;
	} else
		cnp->cn_lkflags = lkflags_save;
#ifdef NAMEI_DIAGNOSTIC
	printf("found\n");
#endif
	/*
	 * Take into account any additional components consumed by
	 * the underlying filesystem.
	 */
	if (cnp->cn_consume > 0) {
		cnp->cn_nameptr += cnp->cn_consume;
		ndp->ni_next += cnp->cn_consume;
		ndp->ni_pathlen -= cnp->cn_consume;
		cnp->cn_consume = 0;
	}

	dp = ndp->ni_vp;
	vfslocked = VFS_LOCK_GIANT(dp->v_mount);

	/*
	 * Check to see if the vnode has been mounted on;
	 * if so find the root of the mounted filesystem.
	 */
	while (dp->v_type == VDIR && (mp = dp->v_mountedhere) &&
	       (cnp->cn_flags & NOCROSSMOUNT) == 0) {
		if (vfs_busy(mp, 0, 0, td))
			continue;
		vput(dp);
		VFS_UNLOCK_GIANT(vfslocked);
		vfslocked = VFS_LOCK_GIANT(mp);
		if (dp != ndp->ni_dvp)
			vput(ndp->ni_dvp);
		else
			vrele(ndp->ni_dvp);
		VFS_UNLOCK_GIANT(dvfslocked);
		dvfslocked = 0;
		vref(vp_crossmp);
		ndp->ni_dvp = vp_crossmp;
		error = VFS_ROOT(mp, compute_cn_lkflags(mp, cnp->cn_lkflags), &tdp, td);
		vfs_unbusy(mp, td);
		if (vn_lock(vp_crossmp, LK_SHARED | LK_NOWAIT, td))
			panic("vp_crossmp exclusively locked or reclaimed");
		if (error) {
			dpunlocked = 1;
			goto bad2;
		}
		ndp->ni_vp = dp = tdp;
	}

	/*
	 * Check for symbolic link
	 */
	if ((dp->v_type == VLNK) &&
	    ((cnp->cn_flags & FOLLOW) || trailing_slash ||
	     *ndp->ni_next == '/')) {
		cnp->cn_flags |= ISSYMLINK;
		if (dp->v_iflag & VI_DOOMED) {
			/* We can't know whether the directory was mounted with
			 * NOSYMFOLLOW, so we can't follow safely. */
			error = EBADF;
			goto bad2;
		}
		if (dp->v_mount->mnt_flag & MNT_NOSYMFOLLOW) {
			error = EACCES;
			goto bad2;
		}
		/*
		 * Symlink code always expects an unlocked dvp.
		 */
		if (ndp->ni_dvp != ndp->ni_vp)
			VOP_UNLOCK(ndp->ni_dvp, 0, td);
		goto success;
	}

	/*
	 * Check for bogus trailing slashes.
	 */
	if (trailing_slash && dp->v_type != VDIR) {
		error = ENOTDIR;
		goto bad2;
	}

nextname:
	/*
	 * Not a symbolic link.  If more pathname,
	 * continue at next component, else return.
	 */
	KASSERT((cnp->cn_flags & ISLASTCN) || *ndp->ni_next == '/',
	    ("lookup: invalid path state."));
	if (*ndp->ni_next == '/') {
		cnp->cn_nameptr = ndp->ni_next;
		while (*cnp->cn_nameptr == '/') {
			cnp->cn_nameptr++;
			ndp->ni_pathlen--;
		}
		if (ndp->ni_dvp != dp)
			vput(ndp->ni_dvp);
		else
			vrele(ndp->ni_dvp);
		VFS_UNLOCK_GIANT(dvfslocked);
		dvfslocked = vfslocked;	/* dp becomes dvp in dirloop */
		vfslocked = 0;
		goto dirloop;
	}
	/*
	 * Disallow directory write attempts on read-only filesystems.
	 */
	if (rdonly &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		error = EROFS;
		goto bad2;
	}
	if (cnp->cn_flags & SAVESTART) {
		ndp->ni_startdir = ndp->ni_dvp;
		VREF(ndp->ni_startdir);
	}
	if (!wantparent) {
		if (ndp->ni_dvp != dp)
			vput(ndp->ni_dvp);
		else
			vrele(ndp->ni_dvp);
		VFS_UNLOCK_GIANT(dvfslocked);
		dvfslocked = 0;
	} else if ((cnp->cn_flags & LOCKPARENT) == 0 && ndp->ni_dvp != dp)
		VOP_UNLOCK(ndp->ni_dvp, 0, td);

	if (cnp->cn_flags & AUDITVNODE1)
		AUDIT_ARG(vnode, dp, ARG_VNODE1);
	else if (cnp->cn_flags & AUDITVNODE2)
		AUDIT_ARG(vnode, dp, ARG_VNODE2);

	if ((cnp->cn_flags & LOCKLEAF) == 0)
		VOP_UNLOCK(dp, 0, td);
success:
	/*
	 * Because of lookup_shared we may have the vnode shared locked, but
	 * the caller may want it to be exclusively locked.
	 */
	if ((cnp->cn_flags & (ISLASTCN | LOCKSHARED | LOCKLEAF)) ==
	    (ISLASTCN | LOCKLEAF) && VOP_ISLOCKED(dp, td) != LK_EXCLUSIVE) {
		vn_lock(dp, LK_UPGRADE | LK_RETRY, td);
	}
	if (vfslocked && dvfslocked)
		VFS_UNLOCK_GIANT(dvfslocked);	/* Only need one */
	if (vfslocked || dvfslocked)
		ndp->ni_cnd.cn_flags |= GIANTHELD;
	return (0);

bad2:
	if (dp != ndp->ni_dvp)
		vput(ndp->ni_dvp);
	else
		vrele(ndp->ni_dvp);
bad:
	if (!dpunlocked)
		vput(dp);
	VFS_UNLOCK_GIANT(vfslocked);
	VFS_UNLOCK_GIANT(dvfslocked);
	ndp->ni_cnd.cn_flags &= ~GIANTHELD;
	ndp->ni_vp = NULL;
	return (error);
}

/*
 * relookup - lookup a path name component
 *    Used by lookup to re-acquire things.
 */
int
relookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{
	struct thread *td = cnp->cn_thread;
	struct vnode *dp = 0;		/* the directory we are searching */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int rdonly;			/* lookup read-only flag bit */
	int error = 0;

	KASSERT(cnp->cn_flags & ISLASTCN,
	    ("relookup: Not given last component."));
	/*
	 * Setup: break out flag bits into variables.
	 */
	wantparent = cnp->cn_flags & (LOCKPARENT|WANTPARENT);
	KASSERT(wantparent, ("relookup: parent not wanted."));
	rdonly = cnp->cn_flags & RDONLY;
	cnp->cn_flags &= ~ISSYMLINK;
	dp = dvp;
	cnp->cn_lkflags = LK_EXCLUSIVE;
	vn_lock(dp, LK_EXCLUSIVE | LK_RETRY, td);

	/*
	 * Search a new directory.
	 *
	 * The last component of the filename is left accessible via
	 * cnp->cn_nameptr for callers that need the name. Callers needing
	 * the name set the SAVENAME flag. When done, they assume
	 * responsibility for freeing the pathname buffer.
	 */
#ifdef NAMEI_DIAGNOSTIC
	printf("{%s}: ", cnp->cn_nameptr);
#endif

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. like "/." or ".".
	 */
	if (cnp->cn_nameptr[0] == '\0') {
		if (cnp->cn_nameiop != LOOKUP || wantparent) {
			error = EISDIR;
			goto bad;
		}
		if (dp->v_type != VDIR) {
			error = ENOTDIR;
			goto bad;
		}
		if (!(cnp->cn_flags & LOCKLEAF))
			VOP_UNLOCK(dp, 0, td);
		*vpp = dp;
		/* XXX This should probably move to the top of function. */
		if (cnp->cn_flags & SAVESTART)
			panic("lookup: SAVESTART");
		return (0);
	}

	if (cnp->cn_flags & ISDOTDOT)
		panic ("relookup: lookup on dot-dot");

	/*
	 * We now have a segment name to search for, and a directory to search.
	 */
#ifdef NAMEI_DIAGNOSTIC
	vprint("search in:", dp);
#endif
	if ((error = VOP_LOOKUP(dp, vpp, cnp)) != 0) {
		KASSERT(*vpp == NULL, ("leaf should be empty"));
		if (error != EJUSTRETURN)
			goto bad;
		/*
		 * If creating and at end of pathname, then can consider
		 * allowing file to be created.
		 */
		if (rdonly) {
			error = EROFS;
			goto bad;
		}
		/* ASSERT(dvp == ndp->ni_startdir) */
		if (cnp->cn_flags & SAVESTART)
			VREF(dvp);
		if ((cnp->cn_flags & LOCKPARENT) == 0)
			VOP_UNLOCK(dp, 0, td);
		/*
		 * This is a temporary assert to make sure I know what the
		 * behavior here was.
		 */
		KASSERT((cnp->cn_flags & (WANTPARENT|LOCKPARENT)) != 0,
		   ("relookup: Unhandled case."));
		/*
		 * We return with ni_vp NULL to indicate that the entry
		 * doesn't currently exist, leaving a pointer to the
		 * (possibly locked) directory vnode in ndp->ni_dvp.
		 */
		return (0);
	}

	dp = *vpp;

	/*
	 * Disallow directory write attempts on read-only filesystems.
	 */
	if (rdonly &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		if (dvp == dp)
			vrele(dvp);
		else
			vput(dvp);
		error = EROFS;
		goto bad;
	}
	/*
	 * Set the parent lock/ref state to the requested state.
	 */
	if ((cnp->cn_flags & LOCKPARENT) == 0 && dvp != dp) {
		if (wantparent)
			VOP_UNLOCK(dvp, 0, td);
		else
			vput(dvp);
	} else if (!wantparent)
		vrele(dvp);
	/*
	 * Check for symbolic link
	 */
	KASSERT(dp->v_type != VLNK || !(cnp->cn_flags & FOLLOW),
	    ("relookup: symlink found.\n"));

	/* ASSERT(dvp == ndp->ni_startdir) */
	if (cnp->cn_flags & SAVESTART)
		VREF(dvp);
	
	if ((cnp->cn_flags & LOCKLEAF) == 0)
		VOP_UNLOCK(dp, 0, td);
	return (0);
bad:
	vput(dp);
	*vpp = NULL;
	return (error);
}

/*
 * Free data allocated by namei(); see namei(9) for details.
 */
void
NDFREE(struct nameidata *ndp, const u_int flags)
{
	int unlock_dvp;
	int unlock_vp;

	unlock_dvp = 0;
	unlock_vp = 0;

	if (!(flags & NDF_NO_FREE_PNBUF) &&
	    (ndp->ni_cnd.cn_flags & HASBUF)) {
		uma_zfree(namei_zone, ndp->ni_cnd.cn_pnbuf);
		ndp->ni_cnd.cn_flags &= ~HASBUF;
	}
	if (!(flags & NDF_NO_VP_UNLOCK) &&
	    (ndp->ni_cnd.cn_flags & LOCKLEAF) && ndp->ni_vp)
		unlock_vp = 1;
	if (!(flags & NDF_NO_VP_RELE) && ndp->ni_vp) {
		if (unlock_vp) {
			vput(ndp->ni_vp);
			unlock_vp = 0;
		} else
			vrele(ndp->ni_vp);
		ndp->ni_vp = NULL;
	}
	if (unlock_vp)
		VOP_UNLOCK(ndp->ni_vp, 0, ndp->ni_cnd.cn_thread);
	if (!(flags & NDF_NO_DVP_UNLOCK) &&
	    (ndp->ni_cnd.cn_flags & LOCKPARENT) &&
	    ndp->ni_dvp != ndp->ni_vp)
		unlock_dvp = 1;
	if (!(flags & NDF_NO_DVP_RELE) &&
	    (ndp->ni_cnd.cn_flags & (LOCKPARENT|WANTPARENT))) {
		if (unlock_dvp) {
			vput(ndp->ni_dvp);
			unlock_dvp = 0;
		} else
			vrele(ndp->ni_dvp);
		ndp->ni_dvp = NULL;
	}
	if (unlock_dvp)
		VOP_UNLOCK(ndp->ni_dvp, 0, ndp->ni_cnd.cn_thread);
	if (!(flags & NDF_NO_STARTDIR_RELE) &&
	    (ndp->ni_cnd.cn_flags & SAVESTART)) {
		vrele(ndp->ni_startdir);
		ndp->ni_startdir = NULL;
	}
}

/*
 * Determine if there is a suitable alternate filename under the specified
 * prefix for the specified path.  If the create flag is set, then the
 * alternate prefix will be used so long as the parent directory exists.
 * This is used by the various compatiblity ABIs so that Linux binaries prefer
 * files under /compat/linux for example.  The chosen path (whether under
 * the prefix or under /) is returned in a kernel malloc'd buffer pointed
 * to by pathbuf.  The caller is responsible for free'ing the buffer from
 * the M_TEMP bucket if one is returned.
 */
int
kern_alternate_path(struct thread *td, const char *prefix, char *path,
    enum uio_seg pathseg, char **pathbuf, int create)
{
	struct nameidata nd, ndroot;
	char *ptr, *buf, *cp;
	size_t len, sz;
	int error;

	buf = (char *) malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	*pathbuf = buf;

	/* Copy the prefix into the new pathname as a starting point. */
	len = strlcpy(buf, prefix, MAXPATHLEN);
	if (len >= MAXPATHLEN) {
		*pathbuf = NULL;
		free(buf, M_TEMP);
		return (EINVAL);
	}
	sz = MAXPATHLEN - len;
	ptr = buf + len;

	/* Append the filename to the prefix. */
	if (pathseg == UIO_SYSSPACE)
		error = copystr(path, ptr, sz, &len);
	else
		error = copyinstr(path, ptr, sz, &len);

	if (error) {
		*pathbuf = NULL;
		free(buf, M_TEMP);
		return (error);
	}

	/* Only use a prefix with absolute pathnames. */
	if (*ptr != '/') {
		error = EINVAL;
		goto keeporig;
	}

	/*
	 * We know that there is a / somewhere in this pathname.
	 * Search backwards for it, to find the file's parent dir
	 * to see if it exists in the alternate tree. If it does,
	 * and we want to create a file (cflag is set). We don't
	 * need to worry about the root comparison in this case.
	 */

	if (create) {
		for (cp = &ptr[len] - 1; *cp != '/'; cp--);
		*cp = '\0';

		NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE, UIO_SYSSPACE, buf, td);
		error = namei(&nd);
		*cp = '/';
		if (error != 0)
			goto keeporig;
	} else {
		NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE, UIO_SYSSPACE, buf, td);

		error = namei(&nd);
		if (error != 0)
			goto keeporig;

		/*
		 * We now compare the vnode of the prefix to the one
		 * vnode asked. If they resolve to be the same, then we
		 * ignore the match so that the real root gets used.
		 * This avoids the problem of traversing "../.." to find the
		 * root directory and never finding it, because "/" resolves
		 * to the emulation root directory. This is expensive :-(
		 */
		NDINIT(&ndroot, LOOKUP, FOLLOW | MPSAFE, UIO_SYSSPACE, prefix,
		    td);

		/* We shouldn't ever get an error from this namei(). */
		error = namei(&ndroot);
		if (error == 0) {
			if (nd.ni_vp == ndroot.ni_vp)
				error = ENOENT;

			NDFREE(&ndroot, NDF_ONLY_PNBUF);
			vrele(ndroot.ni_vp);
			VFS_UNLOCK_GIANT(NDHASGIANT(&ndroot));
		}
	}

	NDFREE(&nd, NDF_ONLY_PNBUF);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(NDHASGIANT(&nd));

keeporig:
	/* If there was an error, use the original path name. */
	if (error)
		bcopy(ptr, buf, len);
	return (error);
}
