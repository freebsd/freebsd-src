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
 *	$Id: devfs_vnops.c,v 1.56 1998/06/10 06:34:54 peter Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/lock.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>/* definitions of spec functions we use */
#include <sys/dirent.h>
#include <miscfs/devfs/devfsdefs.h>
#include <sys/vmmeter.h>                                                        

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>


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
	if (error = VOP_ACCESS(dir_vnode, VEXEC, cnp->cn_cred, p))
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
		if (error = VOP_ACCESS(dir_vnode, VWRITE,
				cnp->cn_cred, p))
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
		if (error = VOP_ACCESS(dir_vnode, VWRITE,
				cnp->cn_cred, p))
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
		if (error = VOP_ACCESS(dir_vnode, VWRITE,
				cnp->cn_cred, p))
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
	dn_p	file_node;
	int	error;
	gid_t	*gp;
	int 	i;

DBPRINT(("access\n"));
	if (error = devfs_vntodn(vp,&file_node))
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
	if (cred->cr_uid != file_node->uid)
	{
		/* failing that.. try groups */
		mode >>= 3;
		gp = cred->cr_groups;
		for (i = 0; i < cred->cr_ngroups; i++, gp++)
		{
			if (file_node->gid == *gp)
			{
				goto found;
			}
		}
		/* failing that.. try general access */
		mode >>= 3;
found:
		;
	}
	if ((file_node->mode & mode) == mode)
		return (0);
	/*
	 *  Root gets to do anything.
	 * but only use suser prives as a last resort
	 * (Use of super powers is recorded in ap->a_p->p_acflag)
	 */
	if( suser(cred, &ap->a_p->p_acflag) == 0) /* XXX what if no proc? */
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
	dn_p	file_node;
	int	error;

DBPRINT(("getattr\n"));
	if (error = devfs_vntodn(vp,&file_node))
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}
	vap->va_rdev = 0;/* default value only */
	vap->va_mode = file_node->mode;
	switch (file_node->type)
	{
	case 	DEV_DIR:
		vap->va_rdev = (dev_t)file_node->dvm;
		vap->va_mode |= (S_IFDIR);
		break;
	case	DEV_CDEV:
		vap->va_rdev = file_node->by.Cdev.dev;
		vap->va_mode |= (S_IFCHR);
		break;
	case	DEV_BDEV:
		vap->va_rdev = file_node->by.Bdev.dev;
		vap->va_mode |= (S_IFBLK);
		break;
	case	DEV_SLNK:
		break;
	}
	vap->va_type = vp->v_type;
	vap->va_nlink = file_node->links;
	vap->va_uid = file_node->uid;
	vap->va_gid = file_node->gid;
	vap->va_fsid = (long)file_node->dvm;
	vap->va_fileid = (long)file_node;
	vap->va_size = file_node->len; /* now a u_quad_t */
	vap->va_blocksize = 512;
	/*
	 * XXX If the node times are in  Jan 1, 1970, then
	 * update them to the boot time.
	 * When we made the node, the date/time was not yet known.
	 */
	if(file_node->ctime.tv_sec < (24 * 3600))
	{
		TIMEVAL_TO_TIMESPEC(&boottime,&(file_node->ctime));
		TIMEVAL_TO_TIMESPEC(&boottime,&(file_node->mtime));
		TIMEVAL_TO_TIMESPEC(&boottime,&(file_node->atime));
	}
	vap->va_ctime = file_node->ctime;
	vap->va_mtime = file_node->mtime;
	vap->va_atime = file_node->atime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_bytes = file_node->len;		/* u_quad_t */
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
	dn_p	file_node;

	if (vap->va_flags != VNOVAL)	/* XXX needs to be implemented */
		return (EOPNOTSUPP);

	if (error = devfs_vntodn(vp,&file_node))
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
		    (cred->cr_uid != file_node->uid)  &&
		    suser(cred, &p->p_acflag))
			return (EPERM);
		    if(VOP_ACCESS(vp, VWRITE, cred, p))
			return (EACCES);
		file_node->atime = vap->va_atime;
		file_node->mtime = vap->va_mtime;
		nanotime(&file_node->ctime);
		return (0);
	}

	/*
	 * Change the permissions.. must be root or owner to do this.
	 */
	if (vap->va_mode != (u_short)VNOVAL) {
		if ((cred->cr_uid != file_node->uid)
		 && suser(cred, &p->p_acflag))
			return (EPERM);
		/* set drwxwxrwx stuff */
		file_node->mode &= ~07777;
		file_node->mode |= vap->va_mode & 07777;
	}

	/*
	 * Change the owner.. must be root to do this.
	 */
	if (vap->va_uid != (uid_t)VNOVAL) {
		if (suser(cred, &p->p_acflag))
			return (EPERM);
		file_node->uid = vap->va_uid;
	}

	/*
	 * Change the group.. must be root or owner to do this.
	 * If we are the owner, we must be in the target group too.
	 * don't use suser() unless you have to as it reports
	 * whether you needed suser powers or not.
	 */
	if (vap->va_gid != (gid_t)VNOVAL) {
		if (cred->cr_uid == file_node->uid){
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
	 	if( suser(cred, &p->p_acflag))
			return (EPERM);
cando:
		file_node->gid = vap->va_gid;
	}
#if 0
	/*
 	 * Copied from somewhere else
	 * but only kept as a marker and reminder of the fact that
	 * flags should be handled some day
	 */
	if (vap->va_flags != VNOVAL) {
		if (error = suser(cred, &p->p_acflag))
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
	dn_p	file_node;

DBPRINT(("read\n"));
	if (error = devfs_vntodn(ap->a_vp,&file_node))
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}


	switch (ap->a_vp->v_type) {
	case VREG:
		return(EINVAL);
	case VDIR:
		return VOP_READDIR(ap->a_vp,ap->a_uio,ap->a_cred,
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
	dn_p	file_node;
	int	error;

	switch (ap->a_vp->v_type) {
	case VREG:
		return(EINVAL);
	case VDIR:
		return(EISDIR);
	case VCHR:
	case VBLK:
		panic("devfs:  vnode methods");
	default:
		panic("devfs_write(): bad file type");
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
	if (error = devfs_vntodn(dvp, &tdp)) {
abortit:
		VOP_ABORTOP(dvp, cnp); 
		return (error);
	}
	if (error = devfs_vntodn(vp, &tp)) goto abortit;
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
	if ( error = devfs_vntodn(tdvp,&tdp)) goto abortit;
	if ( error = devfs_vntodn(vp,&fp)) goto abortit;
	
	/*
	 * trying to move it out of devfs? (v_tag == VT_DEVFS)
	 */
	if ( (vp->v_tag != VT_DEVFS)
	 || (vp->v_tag != tdvp->v_tag) ) {
		error = EXDEV;
abortit:
		VOP_ABORTOP(tdvp, cnp); 
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
	if ( error = devfs_vntodn(tdvp,&tdp)) goto abortit;
	if ( error = devfs_vntodn(fdvp,&fdp)) goto abortit;
	if ( error = devfs_vntodn(fvp,&fp)) goto abortit;
	fnp = fp->last_lookup;
	if (tvp) {
		if ( error = devfs_vntodn(tvp,&tp)) goto abortit;
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
		VOP_ABORTOP(tdvp, tcnp); 
		if (tdvp == tvp) /* eh? */
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
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
		VOP_ABORTOP(tdvp, tcnp);
		vput(tdvp);
		vput(tvp);

		/* Delete source. */
		VOP_ABORTOP(fdvp, fcnp); /*XXX*/
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
	struct vnode *vp;
	int error;
	dn_p dnp;
	union typeinfo by;
	devnm_p nm_p;

DBPRINT(("symlink\n"));
	if(error = devfs_vntodn(ap->a_dvp, &dnp)) {
		return (error);
	}
		
	by.Slnk.name = ap->a_target;
	by.Slnk.namelen = strlen(ap->a_target);
	dev_add_entry(ap->a_cnp->cn_nameptr, dnp, DEV_SLNK, &by,
		NULL, NULL, &nm_p);
	if(error = devfs_dntovn(nm_p->dnp, &vp)) {
		return (error);
	}
	VOP_SETATTR(vp, ap->a_vap, ap->a_cnp->cn_cred, ap->a_cnp->cn_proc);
	*ap->a_vpp = NULL;
	vput(vp);
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
	if (error = devfs_vntodn(vp,&dir_node))
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
			dirent.d_fileno = (unsigned long int)dir_node;
			name = ".";
			dirent.d_namlen = 1;
			dirent.d_type = DT_DIR;
			break;
		case	1:
			if(dir_node->by.Dir.parent)
				dirent.d_fileno
				 = (unsigned long int)dir_node->by.Dir.parent;
			else
				dirent.d_fileno = (unsigned long int)dir_node;
			name = "..";
			dirent.d_namlen = 2;
			dirent.d_type = DT_DIR;
			break;
		default:
			dirent.d_fileno =
				(unsigned long int)name_node->dnp;
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
			if (error = uiomove ((caddr_t)&dirent,
					dirent.d_reclen, uio))
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
	if (error = devfs_vntodn(vp,&lnk_node))
		return error;
	if(lnk_node->type != DEV_SLNK)
		return(EINVAL);
	if (error = VOP_ACCESS(vp, VREAD, ap->a_cred, NULL)) { /* XXX */
		return error;
	}
	error = uiomove(lnk_node->by.Slnk.name, lnk_node->by.Slnk.namelen, uio);
	return error;
}

#ifdef notyet
static int
devfs_abortop(struct vop_abortop_args *ap)
        /*struct vop_abortop_args {
                struct vnode *a_dvp;
                struct componentname *a_cnp;
        } */
{
DBPRINT(("abortop\n"));
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
	return 0;
}
#endif /* notyet */


static int
devfs_reclaim(struct vop_reclaim_args *ap)
        /*struct vop_reclaim_args {
		struct vnode *a_vp;
        } */
{
	dn_p	file_node = NULL;
	int	error;

DBPRINT(("reclaim\n"));
	if (error = devfs_vntodn(ap->a_vp,&file_node))
	{
		printf("devfs_vntodn returned %d ",error);
		return error;
	}

	ap->a_vp->v_data = NULL;
	if (file_node) {
		file_node->vn = 0;
		file_node->vn_id = 0;
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

/*
 * /devfs vnode unsupported operation
 */
static int
devfs_enotsupp(void *junk)
{

	return (EOPNOTSUPP);
}

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
	struct vnode *bvp, *vp = ap->a_vp;
	dev_t bdev, dev = (dev_t)vp->v_rdev;
	int error;
	dn_p	dnp;

	if (error = devfs_vntodn(vp,&dnp))
		return error;

	switch (vp->v_type) {
	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*dnp->by.Cdev.cdevsw->d_open)(
					dnp->by.Cdev.dev,
					ap->a_mode,
					S_IFCHR,
					p);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		error = (*dnp->by.Bdev.bdevsw->d_open)(
					dnp->by.Bdev.dev,
					ap->a_mode,
					S_IFBLK,
					p);
	}
	return (error);
}

/*
 * Vnode op for read
	struct vop_read_args {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} 

 */
/* ARGSUSED */
static int
devfs_read( struct vop_read_args *ap)
{
	register struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
 	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn, nextbn;
	long bsize, bscale;
	struct partinfo dpart;
	int n, on, majordev;
	d_ioctl_t *ioctl;
	int error = 0;
	dev_t dev;
	dn_p	dnp;

	if (error = devfs_vntodn(vp,&dnp))
		return error;


#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("devfs_read mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("devfs_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*dnp->by.Cdev.cdevsw->d_read)
			(dnp->by.Cdev.dev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		dev = dnp->by.Bdev.dev;
		/*
		 * This is a hack!
		 */
		if ( (ioctl = dnp->by.Bdev.bdevsw->d_ioctl) != NULL &&
		    (*ioctl)(dev, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0 &&
		    dpart.part->p_fstype == FS_BSDFFS &&
		    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
			bsize = dpart.part->p_frag * dpart.part->p_fsize;
		bscale = btodb(bsize);
		/* 
		 * Get buffers with this data from the buffer cache.
		 * If it's not there the strategy() entrypoint will be called.
		 * We may do this in several chunks.
		 */
		do {
			bn = btodb(uio->uio_offset) & ~(bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (vp->v_lastr + bscale == bn) {
				nextbn = bn + bscale;
				error = breadn(vp, bn, (int)bsize, &nextbn,
					(int *)&bsize, 1, NOCRED, &bp);
			} else
				error = bread(vp, bn, (int)bsize, NOCRED, &bp);
			vp->v_lastr = bn;
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			/* 
			 * Copy it to the user's space
			 */
			error = uiomove((char *)bp->b_data + on, n, uio);
			brelse(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("devfs_read type");
	}
	/* NOTREACHED */
}

/*
 * Vnode op for write
	struct vop_write_args  {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	}
 */
/* ARGSUSED */
static int
devfs_write( struct vop_write_args *ap)
{
	register struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	int bsize, blkmask;
	struct partinfo dpart;
	register int n, on;
	int error = 0;
	dn_p	dnp;

	if (error = devfs_vntodn(vp,&dnp))
		return error;


#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("devfs_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("devfs_write proc");
#endif

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*dnp->by.Cdev.cdevsw->d_write)
			(dnp->by.Cdev.dev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if (uio->uio_resid == 0)
			return (0);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if ((dnp->by.Bdev.bdevsw->d_ioctl != NULL)
		&& ((*dnp->by.Bdev.bdevsw->d_ioctl)(dnp->by.Bdev.dev, DIOCGPART,
					(caddr_t)&dpart, FREAD, p) == 0)
		&& (dpart.part->p_fstype == FS_BSDFFS)
		&& (dpart.part->p_frag != 0)
		&& (dpart.part->p_fsize != 0)) {
			bsize = dpart.part->p_frag * dpart.part->p_fsize;
		}
		blkmask = btodb(bsize) - 1;
		do {
			bn = btodb(uio->uio_offset) & ~blkmask;
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (n == bsize)
				bp = getblk(vp, bn, bsize, 0, 0);
			else
				error = bread(vp, bn, bsize, NOCRED, &bp);
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove((char *)bp->b_data + on, n, uio);
			if (n + on == bsize)
				bawrite(bp);
			else
				bdwrite(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("devfs_write type");
	}
	/* NOTREACHED */
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

	if (error = devfs_vntodn(ap->a_vp,&dnp))
		return error;


	switch (ap->a_vp->v_type) {

	case VCHR:
		return ((*dnp->by.Cdev.cdevsw->d_ioctl)(dnp->by.Cdev.dev,
					ap->a_command,
					ap->a_data,
					ap->a_fflag,
					ap->a_p));
	case VBLK:
		return ((*dnp->by.Bdev.bdevsw->d_ioctl)(dnp->by.Bdev.dev,
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

	if (error = devfs_vntodn(ap->a_vp,&dnp))
		return error;


	switch (ap->a_vp->v_type) {

	case VCHR:
		return (*dnp->by.Cdev.cdevsw->d_poll)(dnp->by.Cdev.dev,
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
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	struct buf *nbp;
	int s;
	dn_p	dnp;
	int	error;

	if (error = devfs_vntodn(vp,&dnp))
		return error;


	if (vp->v_type == VCHR)
		return (0);
	/*
	 * Flush all dirty buffers associated with a block device.
	 */
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("devfs_fsync: not dirty");
		if ((vp->v_flag & VOBJBUF) && (bp->b_flags & B_CLUSTEROK)) {
			vfs_bio_awrite(bp);
			splx(s);
		} else {
			bremfree(bp);
			bp->b_flags |= B_BUSY;
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
		if (vp->v_dirtyblkhd.lh_first) {
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
 *
 *	struct vop_inactive_args {
 *		struct vnode *a_vp;
 *		struct proc *a_p;
 *	} 
 */

static int
devfs_inactive(struct vop_inactive_args *ap)
{

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Just call the device strategy routine
	struct vop_strategy_args {
		struct vnode *a_vp;
		struct buf *a_bp;
	}
 */
static int
devfs_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;
	dn_p	dnp;
	int	error;

	if ((ap->a_vp->v_type != VCHR)
	&&  (ap->a_vp->v_type != VBLK))
		panic ("devfs_strat:badvnode type");
	if (error = devfs_vntodn(ap->a_vp,&dnp))
		return error;


	if (((bp->b_flags & B_READ) == 0) &&
		(LIST_FIRST(&bp->b_dep)) != NULL && bioops.io_start)
		(*bioops.io_start)(bp);
	switch (ap->a_vp->v_type) {
	case VCHR:
		(*dnp->by.Cdev.cdevsw->d_strategy)(bp);
	case VBLK:
		(*dnp->by.Bdev.bdevsw->d_strategy)(bp);
	}
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
	register struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	dev_t dev = vp->v_rdev;
	d_close_t *devclose;
	int mode, error;
	dn_p	dnp;

	if (error = devfs_vntodn(vp,&dnp))
		return error;


	switch (vp->v_type) {

	case VCHR:
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
		/*
		 * If the vnode is locked, then we are in the midst
		 * of forcably closing the device, otherwise we only
		 * close on last reference.
		 */
		if (vcount(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);
		return ((*dnp->by.Cdev.cdevsw->d_close)(dnp->by.Cdev.dev,
						ap->a_fflag,
						S_IFCHR,
						ap->a_p));
		break;

	case VBLK:
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

		/*
		 * We do not want to really close the device if it
		 * is still in use unless we are trying to close it
		 * forcibly. Since every use (buffer, vnode, swap, cmap)
		 * holds a reference to the vnode, and because we mark
		 * any other vnodes that alias this device, when the
		 * sum of the reference counts on all the aliased
		 * vnodes descends to one, we are on last close.
		 */
		if ((vcount(vp) > 1) && (vp->v_flag & VXLOCK) == 0)
			return (0);

		return ((*dnp->by.Bdev.bdevsw->d_close)(dnp->by.Bdev.dev,
						ap->a_fflag,
						S_IFBLK,
						ap->a_p));

	default:
		panic("devfs_close: not special");
	}
}

/*
 * Print out the contents of a special device vnode.
	struct vop_print_args {
		struct vnode *a_vp;
	}
 */

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
	 * Calculate the offset of the transfer.
	 */
	offset = IDX_TO_OFF(ap->a_m[0]->pindex) + ap->a_offset;

	/* XXX sanity check before we go into details. */
	/* XXX limits should be defined elsewhere. */
#define	DADDR_T_BIT	32
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
	 * Round up physical size for real devices, use the
	 * fundamental blocksize of the fs if possible.
	 */
	if (vp && vp->v_mount) {
		if (vp->v_type != VBLK) {
			vprint("Non VBLK", vp);
		}
		blksiz = vp->v_mount->mnt_stat.f_bsize;
		if (blksiz < DEV_BSIZE) {
			blksiz = DEV_BSIZE;
		}
	}
	else
		blksiz = DEV_BSIZE;
	size = (ap->a_count + blksiz - 1) & ~(blksiz - 1);

	bp = getpbuf();
	kva = (vm_offset_t)bp->b_data;

	/*
	 * Map the pages to be read into the kva.
	 */
	pmap_qenter(kva, ap->a_m, pcount);

	/* Build a minimal buffer header. */
	bp->b_flags = B_BUSY | B_READ | B_CALL;
	bp->b_iodone = devfs_getpages_iodone;

	/* B_PHYS is not set, but it is nice to fill this in. */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = blkno;
	bp->b_lblkno = blkno;
	pbgetvp(ap->a_vp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;
	bp->b_resid = 0;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += pcount;

	/* Do the input. */
	VOP_STRATEGY(bp->b_vp, bp);

	s = splbio();

	/* We definitely need to be at splbio here. */
	while ((bp->b_flags & B_DONE) == 0)
		tsleep(bp, PVM, "spread", 0);

	splx(s);

	if ((bp->b_flags & B_ERROR) != 0) {
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
			m->dirty = 0;
		} else if (toff < nread) {
			int nvalid = ((nread + DEV_BSIZE - 1) - toff) & ~(DEV_BSIZE - 1);
			vm_page_set_validclean(m, 0, nvalid);
		} else {
			m->valid = 0;
			m->dirty = 0;
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
					PAGE_WAKEUP(m);
				} else {
					vm_page_free(m);
				}
			} else {
				vm_page_free(m);
			}
		} else if (m->valid) {
			gotreqpage = 1;
		}
	}
	if (!gotreqpage) {
		m = ap->a_m[ap->a_reqpage];
#ifndef MAX_PERF
		printf("devfs_getpages: I/O read failure: (error code=%d)\n", error);
		printf("               size: %d, resid: %d, a_count: %d, valid: 0x%x\n",
				size, bp->b_resid, ap->a_count, m->valid);
		printf("               nread: %d, reqpage: %d, pindex: %d, pcount: %d\n",
				nread, ap->a_reqpage, m->pindex, pcount);
#endif
		/*
		 * Free the buffer header back to the swap buffer pool.
		 */
		relpbuf(bp);
		return VM_PAGER_ERROR;
	}
	/*
	 * Free the buffer header back to the swap buffer pool.
	 */
	relpbuf(bp);
	return VM_PAGER_OK;
}



/* These are the operations used by directories etc in a devfs */

vop_t **devfs_vnodeop_p;
static struct vnodeopv_entry_desc devfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_bmap_desc,		(vop_t *) devfs_badop },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_inactive_desc,		(vop_t *) devfs_inactive },
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
	{ &vop_fsync_desc,		(vop_t *) devfs_fsync },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_getpages_desc,		(vop_t *) devfs_getpages },
	{ &vop_inactive_desc,		(vop_t *) devfs_inactive },
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

