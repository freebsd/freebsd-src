/*
 * Copyright 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <miscfs/devfs/devfsdefs.h>
#include <sys/vmmeter.h>                                                        

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>


/*
 * Insert description here
 */


/*
 * Convert a component of a pathname into a pointer to a locked node.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and DNUNLOCK
 * instead of two DNUNLOCKs.
 *
 * Overall outline of devfs_lookup:
 *
 *	check accessibility of directory
 *	null terminate the component (lookup leaves the whole string alone)
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory,
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  node and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 * On return to lookup, remove the null termination we put in at the start.
 *
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent node unlocked.
 */
static int
devfs_lookup(struct vop_lookup_args *ap)
        /*struct vop_lookup_args {
                struct vnode * a_dvp; directory vnode ptr
                struct vnode ** a_vpp; where to put the result
                struct componentname * a_cnp; the name we want
        };*/
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dir_vnode = ap->a_dvp;
	struct vnode **result_vnode = ap->a_vpp;
        dn_p   dir_node;       /* the directory we are searching */
        dn_p   new_node;       /* the node we are searching for */
	devnm_p new_nodename;
	int flags = cnp->cn_flags;
        int op = cnp->cn_nameiop;       /* LOOKUP, CREATE, RENAME, or DELETE */
        int lockparent = flags & LOCKPARENT;
        int wantparent = flags & (LOCKPARENT|WANTPARENT);
        int error = 0;
	struct proc *p = cnp->cn_proc;
	char	heldchar;	/* the char at the end of the name componet */

	*result_vnode = NULL; /* safe not sorry */ /*XXX*/

DBPRINT(("lookup\n"));

	if (dir_vnode->v_usecount == 0)
	    printf("dir had no refs ");
	if (devfs_vntodn(dir_vnode,&dir_node))
	{
		printf("vnode has changed?\n");
		vprint("=",dir_vnode);
		return(EINVAL);
	}

	/*
	 * Check accessiblity of directory.
	 */
	if (dir_node->type != DEV_DIR) /* XXX or symlink? */
	{
		return (ENOTDIR);
	}
	if ((error = VOP_ACCESS(dir_vnode, VEXEC, cnp->cn_cred, p)) != 0)
	{
		return (error);
	}

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 */

/***********************************************************************\
* SEARCH FOR NAME							*
* while making sure the component is null terminated for the strcmp 	*
\***********************************************************************/

	heldchar = cnp->cn_nameptr[cnp->cn_namelen];
	cnp->cn_nameptr[cnp->cn_namelen] = '\0';
	new_nodename = dev_findname(dir_node,cnp->cn_nameptr);
	cnp->cn_nameptr[cnp->cn_namelen] = heldchar;
	if(!new_nodename) {
		/*******************************************************\
		* Failed to find it.. (That may be good)		*
		\*******************************************************/
		new_node = NULL; /* to be safe */
		/*
		 * If creating, and at end of pathname
		 * then can consider
		 * allowing file to be created.
		 */
        	if (!(flags & ISLASTCN) || !(op == CREATE || op == RENAME)) {
			return ENOENT;
		}
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		if ((error = VOP_ACCESS(dir_vnode, VWRITE,
				cnp->cn_cred, p)) != 0)
		{
DBPRINT(("MKACCESS "));
			return (error);
		}
		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to add a new entry.
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory vnode in namei_data->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		cnp->cn_flags |= SAVENAME; /*XXX why? */
		if (!lockparent)
			VOP_UNLOCK(dir_vnode, 0, p);
		return (EJUSTRETURN);
	}

	/***************************************************************\
	* Found it.. this is not always a good thing..			*
	\***************************************************************/
	new_node = new_nodename->dnp;
	new_node->last_lookup = new_nodename; /* for unlink */
	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in namei_data->ni_dvp), otherwise we go
	 * on and lock the node, being careful with ".".
	 */
	if (op == DELETE && (flags & ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		if ((error = VOP_ACCESS(dir_vnode, VWRITE,
				cnp->cn_cred, p)) != 0)
			return (error);
		/*
		 * we are trying to delete '.'.  What does this mean? XXX
		 */
		if (dir_node == new_node) {
			VREF(dir_vnode);
			*result_vnode = dir_vnode;
			return (0);
		}
		/*
		 * If directory is "sticky", then user must own
		 * the directory, or the file in it, else she
		 * may not delete it (unless she's root). This
		 * implements append-only directories.
		 */
		devfs_dntovn(new_node,result_vnode);
#ifdef NOTYET
		if ((dir_node->mode & ISVTX) &&
		    cnp->cn_cred->cr_uid != 0 &&
		    cnp->cn_cred->cr_uid != dir_node->uid &&
		    cnp->cn_cred->cr_uid != new_node->uid) {
			VOP_UNLOCK(*result_vnode, 0, p);
			return (EPERM);
		}
#endif
		if (!lockparent)
			VOP_UNLOCK(dir_vnode, 0, p);
		return (0);
	}

	/*
	 * If rewriting (RENAME), return the vnode and the
	 * information required to rewrite the present directory
	 * Must get node of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (op == RENAME && wantparent && (flags & ISLASTCN)) {
		/*
		 * Are we allowed to change the holding directory?
		 */
		if ((error = VOP_ACCESS(dir_vnode, VWRITE,
				cnp->cn_cred, p)) != 0)
			return (error);
		/*
		 * Careful about locking second node.
		 * This can only occur if the target is ".".
		 */
		if (dir_node == new_node)
			return (EISDIR);
		devfs_dntovn(new_node,result_vnode);
		/* hmm save the 'from' name (we need to delete it) */
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(dir_vnode, 0, p);
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not unlock the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "saved_dir_node" XXX.  We must get the target
	 * node before unlocking
	 * the directory to insure that the node will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * nodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the lock for the
	 * node associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(dir_vnode, 0, p);	/* race to get the node */
		devfs_dntovn(new_node,result_vnode);
		if (lockparent && (flags & ISLASTCN))
			vn_lock(dir_vnode, LK_EXCLUSIVE | LK_RETRY, p);
	} else if (dir_node == new_node) {
		VREF(dir_vnode);	/* we want ourself, ie "." */
		*result_vnode = dir_vnode;
	} else {
		devfs_dntovn(new_node,result_vnode);
		if (!lockparent || (flags & ISLASTCN))
			VOP_UNLOCK(dir_vnode, 0, p);
	}

DBPRINT(("GOT\n"));
	return (0);
}

/*
 */

static int
devfs_access(struct vop_access_args *ap)
        /*struct vop_access_args  {
                struct vnode *a_vp;
                int  a_mode;
                struct ucred *a_cred;
                struct proc *a_p;
        } */ 
{
	/*
 	 *  mode is filled with a combination of VREAD, VWRITE,
 	 *  and/or VEXEC bits turned on.  In an octal number these
 	 *  are the Y in 0Y00.
 	 */
	struct vnode *vp = ap->a_vp;
	int mode = ap->a_mode;
	struct ucred *cred = ap->a_cred;
	dn_p	dnp;
	int	error;
	gid_t	*gp;
	int 	i;

DBPRINT(("access\n"));
	if ((error = devfs_vntodn(vp,&dnp)) != 0)
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}

	/* 
	 * if we are not running as a process, we are in the 
	 * kernel and we DO have permission
	 */
	if (ap->a_p == NULL)
		return 0;

	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group. If not a member of the
	 * group, then check public access.
	 */
	if (cred->cr_uid != dnp->uid)
	{
		/* failing that.. try groups */
		mode >>= 3;
		gp = cred->cr_groups;
		for (i = 0; i < cred->cr_ngroups; i++, gp++)
		{
			if (dnp->gid == *gp)
			{
				goto found;
			}
		}
		/* failing that.. try general access */
		mode >>= 3;
found:
		;
	}
	if ((dnp->mode & mode) == mode)
		return (0);
	/*
	 *  Root gets to do anything.
	 * but only use suser_xxx prives as a last resort
	 * (Use of super powers is recorded in ap->a_p->p_acflag)
	 */
	if( suser_xxx(cred, ap->a_p, 0) == 0) /* XXX what if no proc? */
		return 0;
	return (EACCES);
}

static int
devfs_getattr(struct vop_getattr_args *ap)
        /*struct vop_getattr_args {
                struct vnode *a_vp;
                struct vattr *a_vap;
                struct ucred *a_cred;
                struct proc *a_p;
        } */ 
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	dn_p	dnp;
	int	error;

DBPRINT(("getattr\n"));
	if ((error = devfs_vntodn(vp,&dnp)) != 0)
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}
	vap->va_rdev = 0;/* default value only */
	vap->va_mode = dnp->mode;
	switch (dnp->type)
	{
	case 	DEV_DIR:
		vap->va_rdev = (udev_t)dnp->dvm;
		vap->va_mode |= (S_IFDIR);
		break;
	case	DEV_CDEV:
		vap->va_rdev = dev2udev(vp->v_rdev);
		vap->va_mode |= (S_IFCHR);
		break;
#if nolonger
	case	DEV_BDEV:
		vap->va_rdev = dev2budev(vp->v_rdev);
		vap->va_mode |= (S_IFBLK);
		break;
#endif
	case	DEV_SLNK:
		break;
	}
	vap->va_type = vp->v_type;
	vap->va_nlink = dnp->links;
	vap->va_uid = dnp->uid;
	vap->va_gid = dnp->gid;
	vap->va_fsid = (intptr_t)(void *)dnp->dvm;
	vap->va_fileid = (intptr_t)(void *)dnp;
	vap->va_size = dnp->len; /* now a u_quad_t */
	vap->va_blocksize = 512;
	/*
	 * XXX If the node times are in  Jan 1, 1970, then
	 * update them to the boot time.
	 * When we made the node, the date/time was not yet known.
	 */
	if(dnp->ctime.tv_sec < (24 * 3600))
	{
		TIMEVAL_TO_TIMESPEC(&boottime,&(dnp->ctime));
		TIMEVAL_TO_TIMESPEC(&boottime,&(dnp->mtime));
		TIMEVAL_TO_TIMESPEC(&boottime,&(dnp->atime));
	}
	if (dnp->flags & IN_ACCESS) {
		nanotime(&dnp->atime);
		dnp->flags &= ~IN_ACCESS;
	}
	vap->va_ctime = dnp->ctime;
	vap->va_mtime = dnp->mtime;
	vap->va_atime = dnp->atime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_bytes = dnp->len;		/* u_quad_t */
	vap->va_filerev = 0; /* XXX */		/* u_quad_t */
	vap->va_vaflags = 0; /* XXX */
	return 0;
}

static int
devfs_setattr(struct vop_setattr_args *ap)
        /*struct vop_setattr_args  {
                struct vnode *a_vp;
                struct vattr *a_vap;
                struct ucred *a_cred;
                struct proc *a_p;
        } */ 
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct ucred *cred = ap->a_cred;
	struct proc *p = ap->a_p;
	int error = 0;
	gid_t *gp;
	int i;
	dn_p	dnp;

	if (vap->va_flags != VNOVAL)	/* XXX needs to be implemented */
		return (EOPNOTSUPP);

	if ((error = devfs_vntodn(vp,&dnp)) != 0)
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}
DBPRINT(("setattr\n"));
	if ((vap->va_type != VNON)  ||
	    (vap->va_nlink != VNOVAL)  ||
	    (vap->va_fsid != VNOVAL)  ||
	    (vap->va_fileid != VNOVAL)  ||
	    (vap->va_blocksize != VNOVAL)  ||
	    (vap->va_rdev != VNOVAL)  ||
	    (vap->va_bytes != VNOVAL)  ||
	    (vap->va_gen != VNOVAL ))
	{
		return EINVAL;
	}


	/* 
	 * Anyone can touch the files in such a way that the times are set
	 * to NOW (e.g. run 'touch') if they have write permissions
	 * however only the owner or root can set "un-natural times.
	 * They also don't need write permissions.
	 */
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
#if 0		/*
		 * This next test is pointless under devfs for now..
		 * as there is only one devfs hiding under potentially many
		 * mountpoints and actual device node are really 'mounted' under
		 * a FAKE mountpoint inside the kernel only, no matter where it
		 * APPEARS they are mounted to the outside world..
		 * A readonly devfs doesn't exist anyway.
		 */
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
#endif
		if (((vap->va_vaflags & VA_UTIMES_NULL) == 0) &&
		    (cred->cr_uid != dnp->uid)  &&
		    suser_xxx(cred, p, 0))
			return (EPERM);
		    if(VOP_ACCESS(vp, VWRITE, cred, p))
			return (EACCES);
		dnp->atime = vap->va_atime;
		dnp->mtime = vap->va_mtime;
		nanotime(&dnp->ctime);
		return (0);
	}

	/*
	 * Change the permissions.. must be root or owner to do this.
	 */
	if (vap->va_mode != (u_short)VNOVAL) {
		if ((cred->cr_uid != dnp->uid)
		 && suser_xxx(cred, p, 0))
			return (EPERM);
		/* set drwxwxrwx stuff */
		dnp->mode &= ~07777;
		dnp->mode |= vap->va_mode & 07777;
	}

	/*
	 * Change the owner.. must be root to do this.
	 */
	if (vap->va_uid != (uid_t)VNOVAL) {
		if (suser_xxx(cred, p, 0))
			return (EPERM);
		dnp->uid = vap->va_uid;
	}

	/*
	 * Change the group.. must be root or owner to do this.
	 * If we are the owner, we must be in the target group too.
	 * don't use suser_xxx() unless you have to as it reports
	 * whether you needed suser_xxx powers or not.
	 */
	if (vap->va_gid != (gid_t)VNOVAL) {
		if (cred->cr_uid == dnp->uid){
			gp = cred->cr_groups;
			for (i = 0; i < cred->cr_ngroups; i++, gp++) {
				if (vap->va_gid == *gp)
					goto cando; 
			}
		}
		/*
		 * we can't do it with normal privs,
		 * do we have an ace up our sleeve?
		 */
	 	if( suser_xxx(cred, p, 0))
			return (EPERM);
cando:
		dnp->gid = vap->va_gid;
	}
#if 0
	/*
 	 * Copied from somewhere else
	 * but only kept as a marker and reminder of the fact that
	 * flags should be handled some day
	 */
	if (vap->va_flags != VNOVAL) {
		if (error = suser_xxx(cred, p, 0))
			return error;
		if (cred->cr_uid == 0)
		;
		else {
		}
	}
#endif
	return error;
}


static int
devfs_xread(struct vop_read_args *ap)
        /*struct vop_read_args {
                struct vnode *a_vp;
                struct uio *a_uio;
                int  a_ioflag;
                struct ucred *a_cred;
        } */
{
	int	error = 0;
	dn_p	dnp;
	struct vnode *vp = ap->a_vp;

DBPRINT(("read\n"));
	if ((error = devfs_vntodn(vp,&dnp)) != 0)
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}


	switch (vp->v_type) {
	case VREG:
		return(EINVAL);
	case VDIR:
		return VOP_READDIR(vp,ap->a_uio,ap->a_cred,
					NULL,NULL,NULL);
	case VCHR:
	case VBLK:
		panic("devfs:  vnode methods");

	default:
		panic("devfs_read(): bad file type");
		break;
	}
}

/*
 *  Write data to a file or directory.
 */
static int
devfs_xwrite(struct vop_write_args *ap)
        /*struct vop_write_args  {
                struct vnode *a_vp;
                struct uio *a_uio;
                int  a_ioflag;
                struct ucred *a_cred;
        } */
{
	struct vnode *vp = ap->a_vp;

	switch (vp->v_type) {
	case VREG:
		return(EINVAL);
	case VDIR:
		return(EISDIR);
	case VCHR:
	case VBLK:
		panic("devfs:  vnode methods");
	default:
		panic("devfs_xwrite(): bad file type");
	}
}


static int
devfs_remove(struct vop_remove_args *ap)
        /*struct vop_remove_args  {
                struct vnode *a_dvp;
                struct vnode *a_vp;
                struct componentname *a_cnp;
        } */ 
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	dn_p  tp, tdp;
	devnm_p tnp;
	int doingdirectory = 0;
	int error = 0;
	uid_t ouruid = cnp->cn_cred->cr_uid;


DBPRINT(("remove\n"));
	/*
	 * Lock our directories and get our name pointers
	 * assume that the names are null terminated as they
	 * are the end of the path. Get pointers to all our
	 * devfs structures.
	 */
	if ((error = devfs_vntodn(dvp, &tdp)) != 0) {
abortit:
		return (error);
	}
	if ((error = devfs_vntodn(vp, &tp)) != 0) goto abortit;
	/*
	 * Assuming we are atomic, dev_lookup left this for us
	 */
	tnp = tp->last_lookup;
	

	/*
	 * Check we are doing legal things WRT the new flags
	 */
	if ((tp->flags & (IMMUTABLE | APPEND))
	  || (tdp->flags & APPEND) /*XXX eh?*/ ) {
		error = EPERM;
		goto abortit;
	}

	/*
	 * Make sure that we don't try do something stupid
	 */
	if ((tp->type) == DEV_DIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ( (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') 
		    || (cnp->cn_flags&ISDOTDOT) ) {
			error = EINVAL;
			goto abortit;
		}
		doingdirectory++;
	}

	/***********************************
	 * Start actually doing things.... *
	 ***********************************/
	getnanotime(&(tdp->mtime));


	/*
	 * own the parent directory, or the destination of the rename,
	 * otherwise the destination may not be changed (except by
	 * root). This implements append-only directories.
	 * XXX shoudn't this be in generic code? 
	 */
	if ((tdp->mode & S_ISTXT)
	  && ouruid != 0
	  && ouruid != tdp->uid
	  && ouruid != tp->uid ) {
		error = EPERM;
		goto abortit;
	}
	/*
	 * Target must be empty if a directory and have no links
	 * to it. Also, ensure source and target are compatible
	 * (both directories, or both not directories).
	 */
	if (( doingdirectory) && (tp->links > 2)) {
			printf("nlink = %d\n",tp->links); /*XXX*/
			error = ENOTEMPTY;
			goto abortit;
	}
	dev_free_name(tnp);
	tp = NULL;
	return (error);
}

/*
 */
static int
devfs_link(struct vop_link_args *ap)
        /*struct vop_link_args  {
                struct vnode *a_tdvp;
                struct vnode *a_vp;
                struct componentname *a_cnp;
        } */ 
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	dn_p  fp, tdp;
	devnm_p tnp;
	int error = 0;

DBPRINT(("link\n"));
	/*
	 * First catch an arbitrary restriction for this FS
	 */
	if(cnp->cn_namelen > DEVMAXNAMESIZE) {
		error = ENAMETOOLONG;
		goto abortit;
	}

	/*
	 * Lock our directories and get our name pointers
	 * assume that the names are null terminated as they
	 * are the end of the path. Get pointers to all our
	 * devfs structures.
	 */
	if ((error = devfs_vntodn(tdvp,&tdp)) != 0) goto abortit;
	if ((error = devfs_vntodn(vp,&fp)) != 0) goto abortit;
	
	/*
	 * trying to move it out of devfs? (v_tag == VT_DEVFS)
	 */
	if ( (vp->v_tag != VT_DEVFS)
	 || (vp->v_tag != tdvp->v_tag) ) {
		error = EXDEV;
abortit:
		goto out;
	}

	/*
	 * Check we are doing legal things WRT the new flags
	 */
	if (fp->flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto abortit;
	}

	/***********************************
	 * Start actually doing things.... *
	 ***********************************/
	getnanotime(&(tdp->atime));
	error = dev_add_name(cnp->cn_nameptr,
			tdp,
			NULL,
			fp,
			&tnp);
out:
	return (error);

}

/*
 * Rename system call. Seems overly complicated to me...
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.
 *
 * When the target exists, both the directory
 * and target vnodes are locked.
 * the source and source-parent vnodes are referenced
 *
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to node if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */
static int
devfs_rename(struct vop_rename_args *ap)
        /*struct vop_rename_args  {
                struct vnode *a_fdvp;
                struct vnode *a_fvp;
                struct componentname *a_fcnp;
                struct vnode *a_tdvp;
                struct vnode *a_tvp;
                struct componentname *a_tcnp;
        } */
{
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct proc *p = fcnp->cn_proc;
	dn_p fp, fdp, tp, tdp;
	devnm_p fnp,tnp;
	int doingdirectory = 0;
	int error = 0;

	/*
	 * First catch an arbitrary restriction for this FS
	 */
	if(tcnp->cn_namelen > DEVMAXNAMESIZE) {
		error = ENAMETOOLONG;
		goto abortit;
	}

	/*
	 * Lock our directories and get our name pointers
	 * assume that the names are null terminated as they
	 * are the end of the path. Get pointers to all our
	 * devfs structures.
	 */
	if ((error = devfs_vntodn(tdvp,&tdp)) != 0) goto abortit;
	if ((error = devfs_vntodn(fdvp,&fdp)) != 0) goto abortit;
	if ((error = devfs_vntodn(fvp,&fp)) != 0) goto abortit;
	fnp = fp->last_lookup;
	if (tvp) {
		if ((error = devfs_vntodn(tvp,&tp)) != 0) goto abortit;
		tnp = tp->last_lookup;
	} else {
		tp = NULL;
		tnp = NULL;
	}
	
	/*
	 * trying to move it out of devfs? (v_tag == VT_DEVFS)
         * if we move a dir across mnt points. we need to fix all
	 * the mountpoint pointers! XXX
	 * so for now keep dirs within the same mount
	 */
	if ( (fvp->v_tag != VT_DEVFS)
	 || (fvp->v_tag != tdvp->v_tag)
	 || (tvp && (fvp->v_tag != tvp->v_tag))
	 || ((fp->type == DEV_DIR) && (fp->dvm != tdp->dvm ))) {
		error = EXDEV;
abortit:
		if (tdvp == tvp) /* eh? */
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	/*
	 * Check we are doing legal things WRT the new flags
	 */
	if ((tp && (tp->flags & (IMMUTABLE | APPEND)))
	  || (fp->flags & (IMMUTABLE | APPEND))
	  || (fdp->flags & APPEND)) {
		error = EPERM;
		goto abortit;
	}

	/*
	 * Make sure that we don't try do something stupid
	 */
	if ((fp->type) == DEV_DIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') 
		    || (fcnp->cn_flags&ISDOTDOT) 
		    || (tcnp->cn_namelen == 1 && tcnp->cn_nameptr[0] == '.') 
		    || (tcnp->cn_flags&ISDOTDOT) 
		    || (tdp == fp )) {
			error = EINVAL;
			goto abortit;
		}
		doingdirectory++;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". 
	 */
	if (doingdirectory && (tdp != fdp)) {
		dn_p tmp,ntmp;
		error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
		tmp = tdp;
		do {
			if(tmp == fp) {
				/* XXX unlock stuff here probably */
				error = EINVAL;
				goto out;
			}
			ntmp = tmp;
		} while ((tmp = tmp->by.Dir.parent) != ntmp);
	}

	/***********************************
	 * Start actually doing things.... *
	 ***********************************/
	getnanotime(&(fp->atime));
	/*
	 * Check if just deleting a link name.
	 */
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			error = EINVAL;
			goto abortit;
		}

		/* Release destination completely. */
		vput(tdvp);
		vput(tvp);

		/* Delete source. */
		vrele(fdvp);
		vrele(fvp);
		dev_free_name(fnp);
		return 0;
	}


	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work,  too bad :)
	 */
	fp->links++;
	/*
	 * If the target exists zap it (unless it's a non-empty directory)
	 * We could do that as well but won't
 	 */
	if (tp) {
		int ouruid = tcnp->cn_cred->cr_uid;
		/*
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 * XXX shoudn't this be in generic code? 
		 */
		if ((tdp->mode & S_ISTXT)
		  && ouruid != 0
		  && ouruid != tdp->uid
		  && ouruid != tp->uid ) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if (( doingdirectory) && (tp->links > 2)) {
				printf("nlink = %d\n",tp->links); /*XXX*/
				error = ENOTEMPTY;
				goto bad;
		}
		dev_free_name(tnp);
		tp = NULL;
	}
	dev_add_name(tcnp->cn_nameptr,tdp,fnp->as.front.realthing,fp,&tnp);
	fnp->dnp = NULL;
	fp->links--; /* one less link to it.. */
	dev_free_name(fnp);
	fp->links--; /* we added one earlier*/
	if (tdp)
		vput(tdvp);
	if (tp)
		vput(fvp);
	vrele(ap->a_fvp);
	return (error);

bad:
	if (tp)
		vput(tvp);
	vput(tdvp);
out:
	if (vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY, p) == 0) {
		fp->links--; /* we added one earlier*/
		vput(fvp);
	} else
		vrele(fvp);
	return (error);
}

static int
devfs_symlink(struct vop_symlink_args *ap)
        /*struct vop_symlink_args {
                struct vnode *a_dvp;
                struct vnode **a_vpp;
                struct componentname *a_cnp;
                struct vattr *a_vap;
                char *a_target;
        } */
{
	int error;
	dn_p dnp;
	union typeinfo by;
	devnm_p nm_p;

DBPRINT(("symlink\n"));
	if((error = devfs_vntodn(ap->a_dvp, &dnp)) != 0) {
		return (error);
	}
		
	by.Slnk.name = ap->a_target;
	by.Slnk.namelen = strlen(ap->a_target);
	dev_add_entry(ap->a_cnp->cn_nameptr, dnp, DEV_SLNK, &by,
		NULL, NULL, &nm_p);
	if((error = devfs_dntovn(nm_p->dnp, ap->a_vpp)) != 0) {
		return (error);
	}
	VOP_SETATTR(*ap->a_vpp, ap->a_vap, ap->a_cnp->cn_cred,
		ap->a_cnp->cn_proc);
	return 0;
}

/*
 * Vnode op for readdir
 */
static int
devfs_readdir(struct vop_readdir_args *ap)
        /*struct vop_readdir_args {
                struct vnode *a_vp;
                struct uio *a_uio;
                struct ucred *a_cred;
        	int *eofflag;
        	int *ncookies;
        	u_int **cookies;
        } */
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct dirent dirent;
	dn_p dir_node;
	devnm_p	name_node;
	char	*name;
	int error = 0;
	int reclen;
	int nodenumber;
	int	startpos,pos;

DBPRINT(("readdir\n"));

/*  set up refs to dir */
	if ((error = devfs_vntodn(vp,&dir_node)) != 0)
		return error;
	if(dir_node->type != DEV_DIR)
		return(ENOTDIR);

	pos = 0;
	startpos = uio->uio_offset;
	name_node = dir_node->by.Dir.dirlist;
	nodenumber = 0;
	getnanotime(&(dir_node->atime));

	while ((name_node || (nodenumber < 2)) && (uio->uio_resid > 0))
	{
		switch(nodenumber)
		{
		case	0:
			dirent.d_fileno = (uintptr_t)(void *)dir_node;
			name = ".";
			dirent.d_namlen = 1;
			dirent.d_type = DT_DIR;
			break;
		case	1:
			if(dir_node->by.Dir.parent)
				dirent.d_fileno
				 = (uintptr_t)(void *)dir_node->by.Dir.parent;
			else
				dirent.d_fileno = (uintptr_t)(void *)dir_node;
			name = "..";
			dirent.d_namlen = 2;
			dirent.d_type = DT_DIR;
			break;
		default:
			dirent.d_fileno = (uintptr_t)(void *)name_node->dnp;
			dirent.d_namlen = strlen(name_node->name);
			name = name_node->name;
			switch(name_node->dnp->type) {
			case DEV_BDEV:
				dirent.d_type = DT_BLK;
				break;
			case DEV_CDEV:
				dirent.d_type = DT_CHR;
				break;
			case DEV_DDEV:
				dirent.d_type = DT_SOCK; /*XXX*/
				break;
			case DEV_DIR:
				dirent.d_type = DT_DIR;
				break;
			case DEV_SLNK:
				dirent.d_type = DT_LNK;
				break;
			default:
				dirent.d_type = DT_UNKNOWN;
			}
		}

		reclen = dirent.d_reclen = GENERIC_DIRSIZ(&dirent);

		if(pos >= startpos)	/* made it to the offset yet? */
		{
			if (uio->uio_resid < reclen) /* will it fit? */
				break;
			strcpy( dirent.d_name,name);
			if ((error = uiomove ((caddr_t)&dirent,
					dirent.d_reclen, uio)) != 0)
				break;
		}
		pos += reclen;
		if((nodenumber >1) && name_node)
			name_node = name_node->next;
		nodenumber++;
	}
	uio->uio_offset = pos;

	return (error);
}


/*
 */
static int
devfs_readlink(struct vop_readlink_args *ap)
        /*struct vop_readlink_args {
                struct vnode *a_vp;
                struct uio *a_uio;
                struct ucred *a_cred;
        } */
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	dn_p lnk_node;
	int error = 0;


DBPRINT(("readlink\n"));
/*  set up refs to dir */
	if ((error = devfs_vntodn(vp,&lnk_node)) != 0)
		return error;
	if(lnk_node->type != DEV_SLNK)
		return(EINVAL);
	if ((error = VOP_ACCESS(vp, VREAD, ap->a_cred, NULL)) != 0) { /* XXX */
		return error;
	}
	error = uiomove(lnk_node->by.Slnk.name, lnk_node->by.Slnk.namelen, uio);
	return error;
}

static int
devfs_reclaim(struct vop_reclaim_args *ap)
        /*struct vop_reclaim_args {
		struct vnode *a_vp;
        } */
{
	dn_p	dnp = NULL;
	int	error;
	struct vnode *vp = ap->a_vp;

DBPRINT(("reclaim\n"));
	if ((error = devfs_vntodn(vp,&dnp)) != 0)
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}

	vp->v_data = NULL;
	if (dnp) {
		dnp->vn = 0;
		dnp->vn_id = 0;
	}
	return(0);
}

/*
 * Print out the contents of a /devfs vnode.
 */
static int
devfs_print(struct vop_print_args *ap)
	/*struct vop_print_args {
		struct vnode *a_vp;
	} */
{

	printf("tag VT_DEVFS, devfs vnode\n");
	return (0);
}

/**************************************************************************\
* pseudo ops *
\**************************************************************************/

/*proto*/
void
devfs_dropvnode(dn_p dnp)
{
	struct vnode *vn_p;

#ifdef PARANOID
	if(!dnp)
	{
		printf("devfs: dn count dropped too early\n");
	}
#endif
	vn_p = dnp->vn;
	/*
	 * check if we have a vnode.......
	 */
	if((vn_p) && ( dnp->vn_id == vn_p->v_id) && (dnp == (dn_p)vn_p->v_data))
	{
		VOP_REVOKE(vn_p, REVOKEALL);
	}
	dnp->vn = NULL; /* be pedantic about this */
}

/* struct vnode *speclisth[SPECHSZ];*/ /* till specfs goes away */

/*
 * Open a special file.
	struct vop_open_args {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} *ap;
 */
/* ARGSUSED */
static int
devfs_open( struct vop_open_args *ap)
{
	struct proc *p = ap->a_p;
	struct vnode *vp = ap->a_vp;
	int error;
	dn_p	dnp;
	struct cdevsw *dsw;
	dev_t dev = vp->v_rdev;

	if ((error = devfs_vntodn(vp,&dnp)) != 0)
		return error;

	switch (vp->v_type) {
	case VCHR:
		dsw = devsw(dev);
		if ( (dsw == NULL) || (dsw->d_open == NULL))
			return ENXIO;
		if (ap->a_cred != FSCRED && (ap->a_mode & FWRITE) && 
		    vn_isdisk(vp, NULL)) {
			/*
			 * When running in very secure mode, do not allow
			 * opens for writing of any disk devices.
			 */
			if (securelevel >= 2)
				return (EPERM);
			/*
			 * When running in secure mode, do not allow opens
			 * for writing if the device is mounted.
			 */
			if (securelevel >= 1 && vp->v_specmountpoint != NULL)
				return (EPERM);
		}
		if ((dsw->d_flags & D_TYPEMASK) == D_TTY)
			vp->v_flag |= VISTTY;
		VOP_UNLOCK(vp, 0, p);
		error = (*vp->v_rdev->si_devsw->d_open)(
					vp->v_rdev,
					ap->a_mode,
					S_IFCHR,
					p);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);
		/* NOT REACHED */
	case VBLK:
		dsw = devsw(dev);
		if ( (dsw == NULL) || (dsw->d_open == NULL))
			return ENXIO;
		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disk block devices.
		 */
		if (securelevel >= 2 && ap->a_cred != FSCRED &&
		    (ap->a_mode & FWRITE) &&
		    (dsw->d_flags & D_TYPEMASK) == D_DISK)
			return (EPERM);

		/*
		 * Do not allow opens of block devices that are
		 * currently mounted.
		 */
		error = vfs_mountedon(vp);
		if (error)
			return (error);
		error = (*vp->v_rdev->si_devsw->d_open)(
					vp->v_rdev,
					ap->a_mode,
					S_IFBLK,
					p);
		break;
	default:
		break;
	}
	return (error);
}

/* ARGSUSED */
static int
devfs_read( struct vop_read_args *ap)
{
	int error;

        error = VOCALL(spec_vnodeop_p, VOFFSET(vop_read), ap);
	return (error);
}

/* ARGSUSED */
static int
devfs_write( struct vop_write_args *ap)
{
	int error;

	error = VOCALL(spec_vnodeop_p, VOFFSET(vop_write), ap);
	return (error);
}

/*
 * Device ioctl operation.
	struct vop_ioctl_args {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	}
 */
/* ARGSUSED */
static int
devfs_ioctl(struct vop_ioctl_args *ap)
{
	dn_p	dnp;
	int	error;
	struct vnode *vp = ap->a_vp;

	if ((error = devfs_vntodn(vp,&dnp)) != 0)
		return error;


	switch (vp->v_type) {

	case VCHR:
		return ((*vp->v_rdev->si_devsw->d_ioctl)(vp->v_rdev,
					ap->a_command,
					ap->a_data,
					ap->a_fflag,
					ap->a_p));
	case VBLK:
		return ((*vp->v_rdev->si_devsw->d_ioctl)(vp->v_rdev,
					ap->a_command,
					ap->a_data,
					ap->a_fflag,
					ap->a_p));
	default:
		panic("devfs_ioctl");
		/* NOTREACHED */
	}
}

/*
	struct vop_poll_args {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct proc *a_p;
	} *ap;
*/
/* ARGSUSED */
static int
devfs_poll(struct vop_poll_args *ap)
{
	dn_p	dnp;
	int	error;
	struct vnode *vp = ap->a_vp;

	if ((error = devfs_vntodn(vp,&dnp)) != 0)
		return error;


	switch (vp->v_type) {

	case VCHR:
		return (*vp->v_rdev->si_devsw->d_poll)(vp->v_rdev,
					ap->a_events,
					ap->a_p);
	default:
		return (vop_defaultop((struct vop_generic_args *)ap));

	}
}
/*
 * Synch buffers associated with a block device
	struct vop_fsync_args {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_waitfor;
		struct proc *a_p;
	} 
 */
/* ARGSUSED */
static int
devfs_fsync(struct vop_fsync_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct buf *bp;
	struct buf *nbp;
	int s;
	dn_p	dnp;
	int	error;

	if ((error = devfs_vntodn(vp,&dnp)) != 0)
		return error;


	if (vp->v_type == VCHR)
		return (0);
	/*
	 * Flush all dirty buffers associated with a block device.
	 */
loop:
	s = splbio();
	for (bp = TAILQ_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = TAILQ_NEXT(bp, b_vnbufs);
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("devfs_fsync: not dirty");
		if ((vp->v_flag & VOBJBUF) && (bp->b_flags & B_CLUSTEROK)) {
			BUF_UNLOCK(bp);
			vfs_bio_awrite(bp);
			splx(s);
		} else {
			bremfree(bp);
			splx(s);
			bawrite(bp);
		}
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			(void) tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "spfsyn", 0);
		}
#ifdef DIAGNOSTIC
		if (!TAILQ_EMPTY(&vp->v_dirtyblkhd)) {
			vprint("devfs_fsync: dirty", vp);
			splx(s);
			goto loop;
		}
#endif
	}
	splx(s);
	return (0);
}

/*
 * Just call the device strategy routine
	struct vop_strategy_args {
		struct vnode *a_vp;
		struct bio *a_bp;
	}
 */
static int
devfs_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;
	dn_p	dnp;
	int	error;
	struct vnode *vp = ap->a_vp;

	if ((vp->v_type != VCHR)
	&&  (vp->v_type != VBLK))
		panic ("devfs_strat:badvnode type");
	if ((error = devfs_vntodn(vp,&dnp)) != 0)
		return error;


	if ((bp->b_iocmd == BIO_WRITE) && (LIST_FIRST(&bp->b_dep)) != NULL)
		buf_start(bp);
	switch (vp->v_type) {
	case VCHR:
		(*vp->v_rdev->si_devsw->d_strategy)(&bp->b_io);
		break;
	case VBLK:
		(*vp->v_rdev->si_devsw->d_strategy)(&bp->b_io);
		break;
	default:
		/* XXX set error code? */
		break;
	}
	return (0);
}

/*
 * I can't say I'm completely sure what this one is for.
 * it's copied from specfs.
	struct vop_freeblks_args {
		struct vnode *a_vp;
		daddr_t a_addr;
		daddr_t a_length;
	};
 */
static int
devfs_freeblks(struct vop_freeblks_args *ap)
{
	struct cdevsw *bsw;
	struct buf *bp;
	struct vnode *vp = ap->a_vp;

	bsw = devsw(vp->v_rdev);
	if ((bsw->d_flags & D_CANFREE) == 0)
		return (0);
	bp = geteblk(ap->a_length);
	bp->b_iocmd = BIO_DELETE;
	bp->b_dev = vp->v_rdev;
	bp->b_blkno = ap->a_addr;
	bp->b_offset = dbtob(ap->a_addr);
	bp->b_bcount = ap->a_length;
	DEV_STRATEGY(bp, 0);
	return (0);
}


/*
 * This is a noop, simply returning what one has been given.
	struct vop_bmap_args  {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	}
 */
static int
devfs_bmap(struct vop_bmap_args *ap)
{

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

/*
 * Device close routine
	struct vop_close_args {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	}
 */
/* ARGSUSED */
static int
devfs_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	dn_p dnp;
	struct cdevsw *devswp;
	dev_t dev;
	int mode, error;

	if ((error = devfs_vntodn(vp,&dnp)) != 0)
		return error;


	switch (vp->v_type) {

	case VCHR:
		devswp = vp->v_rdev->si_devsw;
		dev = vp->v_rdev;
		mode = S_IFCHR;
		/*
		 * Hack: a tty device that is a controlling terminal
		 * has a reference from the session structure.
		 * We cannot easily tell that a character device is
		 * a controlling terminal, unless it is the closing
		 * process' controlling terminal.  In that case,
		 * if the reference count is 2 (this last descriptor
		 * plus the session), release the reference from the session.
		 */
		if (vcount(vp) == 2 && ap->a_p &&
		    (vp->v_flag & VXLOCK) == 0 &&
		    vp == ap->a_p->p_session->s_ttyvp) {
			vrele(vp);
			ap->a_p->p_session->s_ttyvp = NULL;
		}
		if (vcount(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);

		break;

	case VBLK:
		devswp = vp->v_rdev->si_devsw;
		dev = vp->v_rdev;
		mode = S_IFBLK;
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks, so that
		 * we can, for instance, change floppy disks.
		 */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
		error = vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 0, 0);
		VOP_UNLOCK(vp, 0, ap->a_p);
		if (error)
			return (error);

		break;
	default:
		panic("devfs_close: not special");
	}
	/*
	 * If the vnode is locked, then we are in the midst
	 * of forcably closing the device, otherwise we would normally
	 * only close on last reference.
	 * We do not want to really close the device if it
	 * is still in use unless we are trying to close it
	 * forcibly. Since every use (buffer, vnode, swap, cmap)
	 * holds a reference to the vnode, and because we mark
	 * any other vnodes that alias this device, when the
	 * sum of the reference counts on all the aliased
	 * vnodes descends to one, we are on last close.
	 * defeat this however if the device wants to be told of every 
	 * close.
	 */
	if ((vp->v_flag & VXLOCK)
	|| (devswp->d_flags & D_TRACKCLOSE)
	|| (vcount(vp) <= 1)) {
		return ((*devswp->d_close)(dev, ap->a_fflag, mode, ap->a_p));
	}
	return (0);
}

/*
 * Special device advisory byte-level locks.
	struct vop_advlock_args {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	}
 */
/* ARGSUSED */
static int
devfs_advlock(struct vop_advlock_args *ap)
{

	return (ap->a_flags & F_FLOCK ? EOPNOTSUPP : EINVAL);
}

/*
 * Special device bad operation
 */
static int
devfs_badop(void)
{

	panic("devfs_badop called");
	/* NOTREACHED */
}

static void
devfs_getpages_iodone(struct buf *bp)
{

	bp->b_flags |= B_DONE;
	wakeup(bp);
}

static int
devfs_getpages(struct vop_getpages_args *ap)
{
	vm_offset_t kva;
	int error;
	int i, pcount, size, s;
	daddr_t blkno;
	struct buf *bp;
	vm_page_t m;
	vm_ooffset_t offset;
	int toff, nextoff, nread;
	struct vnode *vp = ap->a_vp;
	int blksiz;
	int gotreqpage;

	error = 0;
	pcount = round_page(ap->a_count) / PAGE_SIZE;

	/*
	 * Calculate the offset of the transfer and do sanity check.
	 * FreeBSD currently only supports an 8 TB range due to b_blkno
	 * being in DEV_BSIZE ( usually 512 ) byte chunks on call to
	 * VOP_STRATEGY.  XXX
	 */
	offset = IDX_TO_OFF(ap->a_m[0]->pindex) + ap->a_offset;

#define	DADDR_T_BIT	(sizeof(daddr_t)*8)
#define	OFFSET_MAX	((1LL << (DADDR_T_BIT + DEV_BSHIFT)) - 1)

	if (offset < 0 || offset > OFFSET_MAX) {
		/* XXX still no %q in kernel. */
		printf("devfs_getpages: preposterous offset 0x%x%08x\n",
		       (u_int)((u_quad_t)offset >> 32),
		       (u_int)(offset & 0xffffffff));
		return (VM_PAGER_ERROR);
	}

	blkno = btodb(offset);

	/*
	 * Round up physical size for real devices.  We cannot round using
	 * v_mount's block size data because v_mount has nothing to do with
	 * the device.  i.e. it's usually '/dev'.  We need the physical block
	 * size for the device itself.
	 *
	 * We can't use v_specmountpoint because it only exists when the
	 * block device is mounted.  However, we can use v_rdev.
	 */

	if (vp->v_type == VBLK)
		blksiz = vp->v_rdev->si_bsize_phys;
	else
		blksiz = DEV_BSIZE;

	size = (ap->a_count + blksiz - 1) & ~(blksiz - 1);

	bp = getpbuf(NULL);
	kva = (vm_offset_t)bp->b_data;

	/*
	 * Map the pages to be read into the kva.
	 */
	pmap_qenter(kva, ap->a_m, pcount);

	/* Build a minimal buffer header. */
	bp->b_iocmd = BIO_READ;
	bp->b_iodone = devfs_getpages_iodone;

	/* B_PHYS is not set, but it is nice to fill this in. */
	bp->b_rcred = bp->b_wcred = curproc->p_ucred;
	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = blkno;
	bp->b_lblkno = blkno;
	pbgetvp(vp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;
	bp->b_resid = 0;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += pcount;

	/* Do the input. */
	BUF_STRATEGY(bp);

	s = splbio();

	/* We definitely need to be at splbio here. */
	while ((bp->b_flags & B_DONE) == 0)
		tsleep(bp, PVM, "spread", 0);

	splx(s);

	if ((bp->b_ioflags & BIO_ERROR) != 0) {
		if (bp->b_error)
			error = bp->b_error;
		else
			error = EIO;
	}

	nread = size - bp->b_resid;

	if (nread < ap->a_count) {
		bzero((caddr_t)kva + nread,
			ap->a_count - nread);
	}
	pmap_qremove(kva, pcount);


	gotreqpage = 0;
	for (i = 0, toff = 0; i < pcount; i++, toff = nextoff) {
		nextoff = toff + PAGE_SIZE;
		m = ap->a_m[i];

		m->flags &= ~PG_ZERO;

		if (nextoff <= nread) {
			m->valid = VM_PAGE_BITS_ALL;
			vm_page_undirty(m);
		} else if (toff < nread) {
			/*
			 * Since this is a VM request, we have to supply the
			 * unaligned offset to allow vm_page_set_validclean()
			 * to zero sub-DEV_BSIZE'd portions of the page.
			 */
			vm_page_set_validclean(m, 0, nread - toff);
		} else {
			m->valid = 0;
			vm_page_undirty(m);
		}

		if (i != ap->a_reqpage) {
			/*
			 * Just in case someone was asking for this page we
			 * now tell them that it is ok to use.
			 */
			if (!error || (m->valid == VM_PAGE_BITS_ALL)) {
				if (m->valid) {
					if (m->flags & PG_WANTED) {
						vm_page_activate(m);
					} else {
						vm_page_deactivate(m);
					}
					vm_page_wakeup(m);
				} else {
					vm_page_free(m);
				}
			} else {
				vm_page_free(m);
			}
		} else if (m->valid) {
			gotreqpage = 1;
			/*
			 * Since this is a VM request, we need to make the
			 * entire page presentable by zeroing invalid sections.
			 */
			if (m->valid != VM_PAGE_BITS_ALL)
				vm_page_zero_invalid(m, FALSE);
		}
	}
	if (!gotreqpage) {
		m = ap->a_m[ap->a_reqpage];
		printf("devfs_getpages: I/O read failure: (error code=%d)\n",
								error);
		printf("               size: %d, resid:"
			" %ld, a_count: %d, valid: 0x%x\n",
				size, bp->b_resid, ap->a_count, m->valid);
		printf("               nread: %d, reqpage:"
			" %d, pindex: %d, pcount: %d\n",
				nread, ap->a_reqpage, m->pindex, pcount);
		/*
		 * Free the buffer header back to the swap buffer pool.
		 */
		relpbuf(bp, NULL);
		return VM_PAGER_ERROR;
	}
	/*
	 * Free the buffer header back to the swap buffer pool.
	 */
	relpbuf(bp, NULL);
	return VM_PAGER_OK;
}



/* These are the operations used by directories etc in a devfs */

vop_t **devfs_vnodeop_p;
static struct vnodeopv_entry_desc devfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_bmap_desc,		(vop_t *) devfs_badop },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_link_desc,		(vop_t *) devfs_link },
	{ &vop_lookup_desc,		(vop_t *) devfs_lookup },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_print_desc,		(vop_t *) devfs_print },
	{ &vop_read_desc,		(vop_t *) devfs_xread },
	{ &vop_readdir_desc,		(vop_t *) devfs_readdir },
	{ &vop_readlink_desc,		(vop_t *) devfs_readlink },
	{ &vop_reclaim_desc,		(vop_t *) devfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) devfs_remove },
	{ &vop_rename_desc,		(vop_t *) devfs_rename },
	{ &vop_setattr_desc,		(vop_t *) devfs_setattr },
	{ &vop_symlink_desc,		(vop_t *) devfs_symlink },
	{ &vop_write_desc,		(vop_t *) devfs_xwrite },
	{ NULL, NULL }
};
static struct vnodeopv_desc devfs_vnodeop_opv_desc =
	{ &devfs_vnodeop_p, devfs_vnodeop_entries };

VNODEOP_SET(devfs_vnodeop_opv_desc);



vop_t **devfs_spec_vnodeop_p;
static struct vnodeopv_entry_desc devfs_spec_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_advlock_desc,		(vop_t *) devfs_advlock },
	{ &vop_bmap_desc,		(vop_t *) devfs_bmap },
	{ &vop_close_desc,		(vop_t *) devfs_close },
	{ &vop_create_desc,		(vop_t *) devfs_badop },
	{ &vop_freeblks_desc,		(vop_t *) devfs_freeblks },
	{ &vop_fsync_desc,		(vop_t *) devfs_fsync },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_getpages_desc,		(vop_t *) devfs_getpages },
	{ &vop_ioctl_desc,		(vop_t *) devfs_ioctl },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_link_desc,		(vop_t *) devfs_badop },
	{ &vop_lookup_desc,		(vop_t *) devfs_lookup },
	{ &vop_mkdir_desc,		(vop_t *) devfs_badop },
	{ &vop_mknod_desc,		(vop_t *) devfs_badop },
	{ &vop_open_desc,		(vop_t *) devfs_open },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_poll_desc,		(vop_t *) devfs_poll },
	{ &vop_print_desc,		(vop_t *) devfs_print },
	{ &vop_read_desc,		(vop_t *) devfs_read },
	{ &vop_readdir_desc,		(vop_t *) devfs_badop },
	{ &vop_readlink_desc,		(vop_t *) devfs_badop },
	{ &vop_reallocblks_desc,	(vop_t *) devfs_badop },
	{ &vop_reclaim_desc,		(vop_t *) devfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) devfs_badop },
	{ &vop_rename_desc,		(vop_t *) devfs_badop },
	{ &vop_rmdir_desc,		(vop_t *) devfs_badop },
	{ &vop_setattr_desc,		(vop_t *) devfs_setattr },
	{ &vop_strategy_desc,		(vop_t *) devfs_strategy },
	{ &vop_symlink_desc,		(vop_t *) devfs_symlink },
	{ &vop_write_desc,		(vop_t *) devfs_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc devfs_spec_vnodeop_opv_desc =
	{ &devfs_spec_vnodeop_p, devfs_spec_vnodeop_entries };

VNODEOP_SET(devfs_spec_vnodeop_opv_desc);

