/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vfs_lookup.c	7.32 (Berkeley) 5/21/91
 *	$Id: vfs_lookup.c,v 1.2 1993/10/16 15:25:23 rgrimes Exp $
 */

#include "param.h"
#include "syslimits.h"
#include "time.h"
#include "namei.h"
#include "vnode.h"
#include "mount.h"
#include "errno.h"
#include "malloc.h"
#include "filedesc.h"
#include "proc.h"

#ifdef KTRACE
#include "ktrace.h"
#endif

/*
 * Convert a pathname into a pointer to a locked inode.
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
namei(ndp, p)
	register struct nameidata *ndp;
	struct proc *p;
{
	register struct filedesc *fdp;	/* pointer to file descriptor state */
	register char *cp;		/* pointer into pathname argument */
	register struct vnode *dp;	/* the directory we are searching */
	struct iovec aiov;		/* uio for reading symbolic links */
	struct uio auio;
	int error, linklen;

	ndp->ni_cred = p->p_ucred;
	fdp = p->p_fd;

	/*
	 * Get a buffer for the name to be translated, and copy the
	 * name into the buffer.
	 */
	if ((ndp->ni_nameiop & HASBUF) == 0)
		MALLOC(ndp->ni_pnbuf, caddr_t, MAXPATHLEN, M_NAMEI, M_WAITOK);
	if (ndp->ni_segflg == UIO_SYSSPACE)
		error = copystr(ndp->ni_dirp, ndp->ni_pnbuf,
			    MAXPATHLEN, &ndp->ni_pathlen);
	else
		error = copyinstr(ndp->ni_dirp, ndp->ni_pnbuf,
			    MAXPATHLEN, &ndp->ni_pathlen);
	if (error) {
		free(ndp->ni_pnbuf, M_NAMEI);
		ndp->ni_vp = NULL;
		return (error);
	}
	ndp->ni_loopcnt = 0;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_NAMEI))
		ktrnamei(p->p_tracep, ndp->ni_pnbuf);
#endif

	/*
	 * Get starting point for the translation.
	 */
	if ((ndp->ni_rootdir = fdp->fd_rdir) == NULL)
		ndp->ni_rootdir = rootdir;
	dp = fdp->fd_cdir;
	VREF(dp);
	for (;;) {
		/*
		 * Check if root directory should replace current directory.
		 * Done at start of translation and after symbolic link.
		 */
		ndp->ni_ptr = ndp->ni_pnbuf;
		if (*ndp->ni_ptr == '/') {
			vrele(dp);
			while (*ndp->ni_ptr == '/') {
				ndp->ni_ptr++;
				ndp->ni_pathlen--;
			}
			dp = ndp->ni_rootdir;
			VREF(dp);
		}
		ndp->ni_startdir = dp;
		if (error = lookup(ndp, p)) {
			FREE(ndp->ni_pnbuf, M_NAMEI);
			return (error);
		}
		/*
		 * Check for symbolic link
		 */
		if (ndp->ni_more == 0) {
			if ((ndp->ni_nameiop & (SAVENAME | SAVESTART)) == 0)
				FREE(ndp->ni_pnbuf, M_NAMEI);
			else
				ndp->ni_nameiop |= HASBUF;
			return (0);
		}
		if ((ndp->ni_nameiop & LOCKPARENT) && ndp->ni_pathlen == 1)
			VOP_UNLOCK(ndp->ni_dvp);
		if (ndp->ni_loopcnt++ >= MAXSYMLINKS) {
			error = ELOOP;
			break;
		}
		if (ndp->ni_pathlen > 1)
			MALLOC(cp, char *, MAXPATHLEN, M_NAMEI, M_WAITOK);
		else
			cp = ndp->ni_pnbuf;
		aiov.iov_base = cp;
		aiov.iov_len = MAXPATHLEN;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_procp = (struct proc *)0;
		auio.uio_resid = MAXPATHLEN;
		if (error = VOP_READLINK(ndp->ni_vp, &auio, p->p_ucred)) {
			if (ndp->ni_pathlen > 1)
				free(cp, M_NAMEI);
			break;
		}
		linklen = MAXPATHLEN - auio.uio_resid;
		if (linklen + ndp->ni_pathlen >= MAXPATHLEN) {
			if (ndp->ni_pathlen > 1)
				free(cp, M_NAMEI);
			error = ENAMETOOLONG;
			break;
		}
		if (ndp->ni_pathlen > 1) {
			bcopy(ndp->ni_next, cp + linklen, ndp->ni_pathlen);
			FREE(ndp->ni_pnbuf, M_NAMEI);
			ndp->ni_pnbuf = cp;
		} else
			ndp->ni_pnbuf[linklen] = '\0';
		ndp->ni_pathlen += linklen;
		vput(ndp->ni_vp);
		dp = ndp->ni_dvp;
	}
	FREE(ndp->ni_pnbuf, M_NAMEI);
	vrele(ndp->ni_dvp);
	vput(ndp->ni_vp);
	ndp->ni_vp = NULL;
	return (error);
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
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent vnode unlocked.
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
lookup(ndp, p)
	register struct nameidata *ndp;
	struct proc *p;
{
	register char *cp;		/* pointer into pathname argument */
	register struct vnode *dp = 0;	/* the directory we are searching */
	struct vnode *tdp;		/* saved dp */
	struct mount *mp;		/* mount table entry */
	int docache;			/* == 0 do not cache last component */
	int flag;			/* LOOKUP, CREATE, RENAME or DELETE */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int rdonly;			/* mounted read-only flag bit(s) */
	int error = 0;

	/*
	 * Setup: break out flag bits into variables.
	 */
	flag = ndp->ni_nameiop & OPMASK;
	wantparent = ndp->ni_nameiop & (LOCKPARENT|WANTPARENT);
	docache = (ndp->ni_nameiop & NOCACHE) ^ NOCACHE;
	if (flag == DELETE || (wantparent && flag != CREATE))
		docache = 0;
	rdonly = MNT_RDONLY;
	if (ndp->ni_nameiop & REMOTE)
		rdonly |= MNT_EXRDONLY;
	ndp->ni_dvp = NULL;
	ndp->ni_more = 0;
	dp = ndp->ni_startdir;
	ndp->ni_startdir = NULLVP;
	VOP_LOCK(dp);

dirloop:
	/*
	 * Search a new directory.
	 *
	 * The ni_hash value is for use by vfs_cache.
	 * The last component of the filename is left accessible via
	 * ndp->ptr for callers that need the name. Callers needing
	 * the name set the SAVENAME flag. When done, they assume
	 * responsibility for freeing the pathname buffer.
	 */
	ndp->ni_hash = 0;
	for (cp = ndp->ni_ptr; *cp != 0 && *cp != '/'; cp++)
		ndp->ni_hash += (unsigned char)*cp;
	ndp->ni_namelen = cp - ndp->ni_ptr;
	if (ndp->ni_namelen >= NAME_MAX) {
		error = ENAMETOOLONG;
		goto bad;
	}
#ifdef NAMEI_DIAGNOSTIC
	{ char c = *cp;
	*cp = '\0';
	printf("{%s}: ", ndp->ni_ptr);
	*cp = c; }
#endif
	ndp->ni_pathlen -= ndp->ni_namelen;
	ndp->ni_next = cp;
	ndp->ni_makeentry = 1;
	if (*cp == '\0' && docache == 0)
		ndp->ni_makeentry = 0;
	ndp->ni_isdotdot = (ndp->ni_namelen == 2 &&
		ndp->ni_ptr[1] == '.' && ndp->ni_ptr[0] == '.');

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. like "/." or ".".
	 */
	if (ndp->ni_ptr[0] == '\0') {
		if (flag != LOOKUP || wantparent) {
			error = EISDIR;
			goto bad;
		}
		if (dp->v_type != VDIR) {
			error = ENOTDIR;
			goto bad;
		}
		if (!(ndp->ni_nameiop & LOCKLEAF))
			VOP_UNLOCK(dp);
		ndp->ni_vp = dp;
		if (ndp->ni_nameiop & SAVESTART)
			panic("lookup: SAVESTART");
		return (0);
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If at root directory (e.g. after chroot)
	 *    then ignore it so can't get out.
	 * 2. If this vnode is the root of a mounted
	 *    filesystem, then replace it with the
	 *    vnode which was mounted on so we take the
	 *    .. in the other file system.
	 */
	if (ndp->ni_isdotdot) {
		for (;;) {
/* 17 Aug 92*/		if ((dp == ndp->ni_rootdir) || (dp == rootdir)) {
				ndp->ni_dvp = dp;
				ndp->ni_vp = dp;
				VREF(dp);
				goto nextname;
			}
			if ((dp->v_flag & VROOT) == 0 ||
			    (ndp->ni_nameiop & NOCROSSMOUNT))
				break;
			tdp = dp;
			dp = dp->v_mount->mnt_vnodecovered;
			vput(tdp);
			VREF(dp);
			VOP_LOCK(dp);
		}
	}

	/*
	 * We now have a segment name to search for, and a directory to search.
	 */
	if (error = VOP_LOOKUP(dp, ndp, p)) {
#ifdef DIAGNOSTIC
		if (ndp->ni_vp != NULL)
			panic("leaf should be empty");
#endif
#ifdef NAMEI_DIAGNOSTIC
		printf("not found\n");
#endif
		if (flag == LOOKUP || flag == DELETE ||
		    error != ENOENT || *cp != 0)
			goto bad;
		/*
		 * If creating and at end of pathname, then can consider
		 * allowing file to be created.
		 */
		if (ndp->ni_dvp->v_mount->mnt_flag & rdonly) {
			error = EROFS;
			goto bad;
		}
		/*
		 * We return with ni_vp NULL to indicate that the entry
		 * doesn't currently exist, leaving a pointer to the
		 * (possibly locked) directory inode in ndp->ni_dvp.
		 */
		if (ndp->ni_nameiop & SAVESTART) {
			ndp->ni_startdir = ndp->ni_dvp;
			VREF(ndp->ni_startdir);
		}
		return (0);
	}
#ifdef NAMEI_DIAGNOSTIC
	printf("found\n");
#endif

	dp = ndp->ni_vp;
	/*
	 * Check for symbolic link
	 */
	if ((dp->v_type == VLNK) &&
	    ((ndp->ni_nameiop & FOLLOW) || *ndp->ni_next == '/')) {
		ndp->ni_more = 1;
		return (0);
	}

	/*
	 * Check to see if the vnode has been mounted on;
	 * if so find the root of the mounted file system.
	 */
mntloop:
	while (dp->v_type == VDIR && (mp = dp->v_mountedhere) &&
	       (ndp->ni_nameiop & NOCROSSMOUNT) == 0) {
		while(mp->mnt_flag & MNT_MLOCK) {
			mp->mnt_flag |= MNT_MWAIT;
			sleep((caddr_t)mp, PVFS);
			goto mntloop;
		}
		if (error = VFS_ROOT(dp->v_mountedhere, &tdp))
			goto bad2;
		vput(dp);
		ndp->ni_vp = dp = tdp;
	}

nextname:
	/*
	 * Not a symbolic link.  If more pathname,
	 * continue at next component, else return.
	 */
	if (*ndp->ni_next == '/') {
		ndp->ni_ptr = ndp->ni_next;
		while (*ndp->ni_ptr == '/') {
			ndp->ni_ptr++;
			ndp->ni_pathlen--;
		}
		vrele(ndp->ni_dvp);
		goto dirloop;
	}
	/*
	 * Check for read-only file systems.
	 */
	if (flag == DELETE || flag == RENAME) {
		/*
		 * Disallow directory write attempts on read-only
		 * file systems.
		 */
		if ((dp->v_mount->mnt_flag & rdonly) ||
		    (wantparent && (ndp->ni_dvp->v_mount->mnt_flag & rdonly))) {
			error = EROFS;
			goto bad2;
		}
	}
	if (ndp->ni_nameiop & SAVESTART) {
		ndp->ni_startdir = ndp->ni_dvp;
		VREF(ndp->ni_startdir);
	}
	if (!wantparent)
		vrele(ndp->ni_dvp);
	if ((ndp->ni_nameiop & LOCKLEAF) == 0)
		VOP_UNLOCK(dp);
	return (0);

bad2:
	if ((ndp->ni_nameiop & LOCKPARENT) && *ndp->ni_next == '\0')
		VOP_UNLOCK(ndp->ni_dvp);
	vrele(ndp->ni_dvp);
bad:
	vput(dp);
	ndp->ni_vp = NULL;
	return (error);
}
