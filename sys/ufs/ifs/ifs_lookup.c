/*
 * Copyright (c) 1999, 2000
 *	Adrian Chadd <adrian@FreeBSD.org>
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
 *	@(#)ufs_lookup.c	8.15 (Berkeley) 6/16/95
 * $FreeBSD$
 */

#include <machine/limits.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ffs/fs.h>

#include <ufs/ifs/ifs_extern.h>

/* true if old FS format...*/
#define	OFSFMT(vp)	((vp)->v_mount->mnt_maxsymlinklen <= 0)

/* Define if you want my debug printfs inside ifs_lookup() */
#undef	DEBUG_IFS_LOOKUP


#ifdef DEBUG_IFS_LOOKUP
static char *
getnameiopstr(int nameiop)
{
	switch (nameiop) {
		case LOOKUP:
			return "LOOKUP";
			break;
		case CREATE:
			return "CREATE";
			break;
		case DELETE:
			return "DELETE";
			break;
		case RENAME:
			return "RENAME";
			break;
		default:
			return "unknown";
			break;
	}
}
#endif

/*
 * Convert a path to an inode.
 *
 * This is HIGHLY simplified - we just take the path fragment given to us,
 * attempt to convert it to an inode, and return the inode if we can.
 * No permission checking is done here, that is done further up in the
 * VFS call layers.
 */
int
ifs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vdp;		/* vnode for directory being searched */
	struct inode *dp;		/* inode for directory being searched */
	struct vnode *pdp;		/* saved dp during symlink work */
	struct vnode *tdp;		/* returned by VFS_VGET */
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	struct mount *mp = ap->a_dvp->v_mount;
	struct fs *fs = VFSTOUFS(mp)->um_fs;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	int error, lockparent, wantparent;
	struct thread *td = cnp->cn_thread;
	ufs_daddr_t inodenum;
	char *endp;

	*vpp = NULL;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	vdp = ap->a_dvp;
	dp = VTOI(vdp);
	pdp = vdp;
	/*
	 * Firstly, we are NOT dealing with RENAME, at all
	 */
	if (nameiop == RENAME) {
		*vpp = NULL;
#ifdef DEBUG_IFS_LOOKUP
		printf("ifs_lookup(): Denying RENAME nameiop\n");
#endif
		return (EPERM);
	}	
	/* Deal with the '.' directory */
	/* VOP_UNLOCK(vdp, 0, td); */
	if (cnp->cn_namelen == 1 && *(cnp->cn_nameptr) == '.') {
		/* We don't unlock the parent dir since the're the same */
		*vpp = vdp; 
		VREF(vdp);
		/* vn_lock(vdp, LK_SHARED | LK_RETRY, td); */
		return (0);
	}
	/* 
	 * 'newfile' is considered something special .. read below why
	 * we're returning NULL
	 */
	if ((cnp->cn_namelen) == 7 && (strncmp(cnp->cn_nameptr, "newfile", 7) == 0)) {
		if (nameiop == CREATE) {
			/* Check for write permissions in . */
			error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_thread);
			if (error)
				return (error);
			*vpp = NULL;
		        if (!lockparent || !(flags & ISLASTCN))
               			 VOP_UNLOCK(pdp, 0, td);
			cnp->cn_flags |= SAVENAME;
			return (EJUSTRETURN);
		} else {
			*vpp = NULL;
#ifdef DEBUG_IFS_LOOKUP
			printf("ifs_lookup(): Denying !CREATE on 'newfile'\n");
#endif
			return (EPERM);
		}
	}
	/* Grab the hex inode number */
	inodenum = strtouq(cnp->cn_nameptr, &endp, 0);
	/* Sanity Check */
	if (endp != (cnp->cn_nameptr + cnp->cn_namelen)) {
		*vpp = NULL;
		return (ENOENT);
	}
	/* 
	 * error check here - inodes 0-2 are considered 'special' even here
	 * so we will hide it from the user.
	 */
	if (inodenum <= 2) {
#ifdef DEBUG_IFS_LOOKUP
		printf("ifs_lookup(): Access to disk inode '%d' denied\n",
		    (int)inodenum);
#endif
		return EPERM;
	}
	/* Check we haven't overflowed the number of inodes in the fs */
	if (inodenum > (fs->fs_ncg * fs->fs_ipg)) {
#ifdef DEBUG_IFS_LOOKUP
		printf("ifs_lookup(): disk inode '%d' is outside the disk\n"),
		    (int)inodenum);
#endif
		return EINVAL;
	}
	/*
	 * The next bit of code grabs the inode, checks to see whether
	 * we're allowed to do our nameiop on it. The only one we need
	 * to check here is CREATE - only allow create on an inode
	 * that exists.
	 *
	 * Comment for VFS-newbies:
	 * read vn_open() - you'll learn that if you return a name here,
	 * it assumes you don't need to call VOP_CREATE. Bad juju as 
	 * you now have a vnode that isn't tagged with the right type,
	 * and it'll panic in the VOP_READ/VOP_WRITE routines..
	 */
	/*
	 * If we get here and its a CREATE, then return EPERM if the inode
	 * doesn't exist.
	 */
	if ((nameiop == CREATE) &&
	    (ifs_isinodealloc(VTOI(vdp), inodenum) != IFS_INODE_ISALLOC)) {
#ifdef DEBUG_IFS_LOOKUP
		printf("ifs_lookup(): CREATE on inode %d which doesn't exist\n",
		    (int)inodenum);
#endif
		return EPERM;
#ifdef DEBUG_IFS_LOOKUP
	} else if (nameiop == CREATE) {
		/* It does exist, allow CREATE */
		printf("ifs_lookup(): CREATE on inode %d which exists\n",
		    (int)inodenum);
	}
#else
	}
#endif
	/* 
	 * Make sure that the inode exists if we're trying to delete or
	 * modify
	 */
	if ((nameiop == LOOKUP || nameiop == DELETE) &&
	     ifs_isinodealloc(VTOI(vdp), inodenum) != IFS_INODE_ISALLOC) {
		/* it doesn't exist */
#ifdef DEBUG_IFS_LOOKUP
		printf("ifs_lookup(): Inode %d isn't allocated\n", inodenum);
#endif
		return ENOENT;
	}
	/* Now, we can get the vnode */
	error = VFS_VGET(vdp->v_mount, (long)inodenum, LK_EXCLUSIVE, &tdp);
	if (error)
		return (error);
	if (!lockparent || !(flags & ISLASTCN))
		VOP_UNLOCK(pdp, 0, td);
	*vpp = tdp;
	return (0);
}

