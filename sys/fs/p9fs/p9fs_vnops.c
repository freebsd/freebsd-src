/*
 * Copyright (c) 2017-2020 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
*	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* This file contains VFS file ops for the 9P protocol.
 * This makes the upper layer of the p9fs driver. These functions interact
 * with the VFS layer and lower layer of p9fs driver which is 9Pnet. All
 * the user file operations are handled here.
 */
#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/rwlock.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <fs/p9fs/p9_client.h>
#include <fs/p9fs/p9_debug.h>
#include <fs/p9fs/p9fs.h>
#include <fs/p9fs/p9fs_proto.h>

/* File permissions. */
#define IEXEC		0000100 /* Executable. */
#define IWRITE		0000200 /* Writeable. */
#define IREAD		0000400 /* Readable. */
#define ISVTX		0001000 /* Sticky bit. */
#define ISGID		0002000 /* Set-gid. */
#define ISUID		0004000 /* Set-uid. */

static MALLOC_DEFINE(M_P9UIOV, "uio", "UIOV structures for strategy in p9fs");
extern uma_zone_t p9fs_io_buffer_zone;
extern uma_zone_t p9fs_getattr_zone;
extern uma_zone_t p9fs_setattr_zone;
extern uma_zone_t p9fs_pbuf_zone;
/* For the root vnode's vnops. */
struct vop_vector p9fs_vnops;

static uint32_t p9fs_unix2p9_mode(uint32_t mode);

static void
p9fs_itimes(struct vnode *vp)
{
	struct p9fs_node *node;
	struct timespec ts;
	struct p9fs_inode *inode;

	node = P9FS_VTON(vp);
	inode = &node->inode;

	vfs_timestamp(&ts);
	inode->i_mtime = ts.tv_sec;
}

/*
 * Cleanup the p9fs node, the in memory representation of a vnode for p9fs.
 * The cleanup includes invalidating all cache entries for the vnode,
 * destroying the vobject, removing vnode from hashlist, removing p9fs node
 * from the list of session p9fs nodes, and disposing of the p9fs node.
 * Basically it is doing a reverse of what a create/vget does.
 */
void
p9fs_cleanup(struct p9fs_node *np)
{
	struct vnode *vp;
	struct p9fs_session *vses;

	if (np == NULL)
		return;

	vp = P9FS_NTOV(np);
	vses = np->p9fs_ses;

	/* Remove the vnode from hash list if vnode is not already deleted */
	if ((np->flags & P9FS_NODE_DELETED) == 0)
		vfs_hash_remove(vp);

	P9FS_LOCK(vses);
	if ((np->flags & P9FS_NODE_IN_SESSION) != 0) {
		np->flags &= ~P9FS_NODE_IN_SESSION;
		STAILQ_REMOVE(&vses->virt_node_list, np, p9fs_node, p9fs_node_next);
	} else {
		P9FS_UNLOCK(vses);
		return;
	}
	P9FS_UNLOCK(vses);

	/* Invalidate all entries to a particular vnode. */
	cache_purge(vp);

	/* Destroy the vm object and flush associated pages. */
	vnode_destroy_vobject(vp);

	/* Remove all the FID */
	p9fs_fid_remove_all(np, FALSE);

	/* Dispose all node knowledge.*/
	p9fs_destroy_node(&np);
}

/*
 * Reclaim VOP is defined to be called for every vnode. This starts off
 * the cleanup by clunking(remove the fid on the server) and calls
 * p9fs_cleanup to free all the resources allocated for p9fs node.
 */
static int
p9fs_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp;
	struct p9fs_node *np;

	vp = ap->a_vp;
	np = P9FS_VTON(vp);

	P9_DEBUG(VOPS, "%s: vp:%p node:%p\n", __func__, vp, np);
	p9fs_cleanup(np);

	return (0);
}

/*
 * recycle vnodes which are no longer referenced i.e, their usecount is zero
 */
static int
p9fs_inactive(struct vop_inactive_args *ap)
{
	struct vnode *vp;
	struct p9fs_node *np;

	vp = ap->a_vp;
	np = P9FS_VTON(vp);

	P9_DEBUG(VOPS, "%s: vp:%p node:%p file:%s\n", __func__, vp, np, np->inode.i_name);
	if (np->flags & P9FS_NODE_DELETED)
		vrecycle(vp);

	return (0);
}

struct p9fs_lookup_alloc_arg {
	struct componentname *cnp;
	struct p9fs_node *dnp;
	struct p9_fid *newfid;
};

/* Callback for vn_get_ino */
static int
p9fs_lookup_alloc(struct mount *mp, void *arg, int lkflags, struct vnode **vpp)
{
	struct p9fs_lookup_alloc_arg *p9aa = arg;

	return (p9fs_vget_common(mp, NULL, p9aa->cnp->cn_lkflags, p9aa->dnp,
		p9aa->newfid, vpp, p9aa->cnp->cn_nameptr));
}

/*
 * p9fs_lookup is called for every component name that is being searched for.
 *
 * I. If component is found on the server, we look for the in-memory
 *    repesentation(vnode) of this component in namecache.
 *    A. If the node is found in the namecache, we check is the vnode is still
 *	 valid.
 *	 1. If it is still valid, return vnode.
 *	 2. If it is not valid, we remove this vnode from the name cache and
 *	    create a new vnode for the component and return that vnode.
 *    B. If the vnode is not found in the namecache, we look for it in the
 *       hash list.
 *       1. If the vnode is in the hash list, we check if the vnode is still
 *	    valid.
 *	    a. If it is still valid, we add that vnode to the namecache for
 *	       future lookups and return the vnode.
 *	    b. If it is not valid, create a new vnode and p9fs node,
 *	       initialize them and return the vnode.
 *	 2. If the vnode is not found in the hash list, we create a new vnode
 *	    and p9fs node, initialize them and return the vnode.
 * II. If the component is not found on the server, an error code is returned.
 *     A. For the creation case, we return EJUSTRETURN so VFS can handle it.
 *     B. For all other cases, ENOENT is returned.
 */
static int
p9fs_lookup(struct vop_lookup_args *ap)
{
	struct vnode *dvp;
	struct vnode **vpp, *vp;
	struct componentname *cnp;
	struct p9fs_node *dnp; /*dir p9_node */
	struct p9fs_node *np;
	struct p9fs_session *vses;
	struct mount *mp; /* Get the mount point */
	struct p9_fid *dvfid, *newfid;
	uint64_t flags;
	int error;
	struct vattr vattr;
	char tmpchr;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;
	dnp = P9FS_VTON(dvp);
	error = 0;
	flags = cnp->cn_flags;
	*vpp = NULLVP;

	if (dnp == NULL)
		return (ENOENT);

	if (cnp->cn_nameptr[0] == '.' && cnp->cn_namelen == 1) {
		vref(dvp);
		*vpp = dvp;
		return (0);
	}

	vses = dnp->p9fs_ses;
	mp = vses->p9fs_mount;

	/* Do the cache part ourselves */
	if ((flags & ISLASTCN) && (mp->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, curthread);
	if (error)
		return (error);

	/* Do the directory walk on host to check if file exist */
	dvfid = p9fs_get_fid(vses->clnt, dnp, cnp->cn_cred, VFID, -1, &error);
	if (error)
		return (error);

	/*
	 * Save the character present at namelen in nameptr string and
	 * null terminate the character to get the search name for p9_dir_walk
	 * This is done to handle when lookup is for "a" and component
	 * name contains a/b/c
	 */
	tmpchr = cnp->cn_nameptr[cnp->cn_namelen];
	cnp->cn_nameptr[cnp->cn_namelen] = '\0';

	/*
	 * If the client_walk fails, it means the file looking for doesnt exist.
	 * Create the file is the flags are set or just return the error
	 */
	newfid = p9_client_walk(dvfid, 1, &cnp->cn_nameptr, 1, &error);

	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;

	if (error != 0 || newfid == NULL) {
		/* Clunk the newfid if it is not NULL */
		if (newfid != NULL)
			p9_client_clunk(newfid);

		if (error != ENOENT)
			return (error);

		/* The requested file was not found. */
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN)) {

			if (mp->mnt_flag & MNT_RDONLY)
				return (EROFS);

			error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred,
			    curthread);
			if (!error) {
				return (EJUSTRETURN);
			}
		}
		return (error);
	}

	/* Look for the entry in the component cache*/
	error = cache_lookup(dvp, vpp, cnp, NULL, NULL);
	if (error > 0 && error != ENOENT) {
		P9_DEBUG(VOPS, "%s: Cache lookup error %d \n", __func__, error);
		goto out;
	}

	if (error == -1) {
		vp = *vpp;
		/* Check if the entry in cache is stale or not */
		if ((p9fs_node_cmp(vp, &newfid->qid) == 0) &&
		    ((error = VOP_GETATTR(vp, &vattr, cnp->cn_cred)) == 0)) {
			goto out;
		}
		/*
		 * This case, we have an error coming from getattr,
		 * act accordingly.
		 */
		cache_purge(vp);
		if (dvp != vp)
			vput(vp);
		else
			vrele(vp);

		*vpp = NULLVP;
	} else if (error == ENOENT) {
		if (VN_IS_DOOMED(dvp))
			goto out;
		if (VOP_GETATTR(dvp, &vattr, cnp->cn_cred) == 0) {
			error = ENOENT;
			goto out;
		}
		cache_purge_negative(dvp);
	}
	/* Reset values */
	error = 0;
	vp = NULLVP;

	tmpchr = cnp->cn_nameptr[cnp->cn_namelen];
	cnp->cn_nameptr[cnp->cn_namelen] = '\0';

	/*
	 * Looks like we have found an entry. Now take care of all other cases.
	 */
	if (flags & ISDOTDOT) {
		struct p9fs_lookup_alloc_arg p9aa;
		p9aa.cnp = cnp;
		p9aa.dnp = dnp;
		p9aa.newfid = newfid;
		error = vn_vget_ino_gen(dvp, p9fs_lookup_alloc, &p9aa, 0, &vp);
		if (error)
			goto out;
		*vpp = vp;
	} else {
		/*
		 * client_walk is equivalent to searching a component name in a
		 * directory(fid) here. If new fid is returned, we have found an
		 * entry for this component name so, go and create the rest of
		 * the vnode infra(vget_common) for the returned newfid.
		 */
		if ((cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		    && (flags & ISLASTCN)) {
			error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred,
			    curthread);
			if (error)
				goto out;

			error = p9fs_vget_common(mp, NULL, cnp->cn_lkflags,
			    dnp, newfid, &vp, cnp->cn_nameptr);
			if (error)
				goto out;

			*vpp = vp;
			np = P9FS_VTON(vp);
			if ((dnp->inode.i_mode & ISVTX) &&
			    cnp->cn_cred->cr_uid != 0 &&
			    cnp->cn_cred->cr_uid != dnp->inode.n_uid &&
			    cnp->cn_cred->cr_uid != np->inode.n_uid) {
				vput(*vpp);
				*vpp = NULL;
				cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;
				return (EPERM);
			}
		} else {
			error = p9fs_vget_common(mp, NULL, cnp->cn_lkflags,
			    dnp, newfid, &vp, cnp->cn_nameptr);
			if (error)
				goto out;
			*vpp = vp;
		}
	}

	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;

	/* Store the result the cache if MAKEENTRY is specified in flags */
	if ((cnp->cn_flags & MAKEENTRY) != 0)
		cache_enter(dvp, *vpp, cnp);
	return (error);
out:
	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;
	p9_client_clunk(newfid);
	return (error);
}

/*
 * Common creation function for file/directory with respective flags. We first
 * open the parent directory in order to create the file under it. For this,
 * as 9P protocol suggests, we need to call client_walk to create the open fid.
 * Once we have the open fid, the file_create function creates the direntry with
 * the name and perm specified under the parent dir. If this succeeds (an entry
 * is created for the new file on the server), we create our metadata for this
 * file (vnode, p9fs node calling vget). Once we are done, we clunk the open
 * fid of the parent directory.
 */
static int
create_common(struct p9fs_node *dnp, struct componentname *cnp,
    char *extension, uint32_t perm, uint8_t mode, struct vnode **vpp)
{
	char tmpchr;
	struct p9_fid *dvfid, *ofid, *newfid;
	struct p9fs_session *vses;
	struct mount *mp;
	int error;

	P9_DEBUG(VOPS, "%s: name %s\n", __func__, cnp->cn_nameptr);

	vses = dnp->p9fs_ses;
	mp = vses->p9fs_mount;
	newfid = NULL;
	error = 0;

	dvfid = p9fs_get_fid(vses->clnt, dnp, cnp->cn_cred, VFID, -1, &error);
	if (error != 0)
		return (error);

	/* Clone the directory fid to create the new file */
	ofid = p9_client_walk(dvfid, 0, NULL, 1, &error);
	if (error != 0)
		return (error);

	/*
	 * Save the character present at namelen in nameptr string and
	 * null terminate the character to get the search name for p9_dir_walk
	 */
	tmpchr = cnp->cn_nameptr[cnp->cn_namelen];
	cnp->cn_nameptr[cnp->cn_namelen] = '\0';

	error = p9_client_file_create(ofid, cnp->cn_nameptr, perm, mode,
		    extension);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: p9_client_fcreate failed %d\n", __func__, error);
		goto out;
	}

	/* If its not hardlink only then do the walk, else we are done. */
	if (!(perm & P9PROTO_DMLINK)) {
		/*
		 * Do the lookup part and add the vnode, p9fs node. Note that vpp
		 * is filled in here.
		 */
		newfid = p9_client_walk(dvfid, 1, &cnp->cn_nameptr, 1, &error);
		if (newfid != NULL) {
			error = p9fs_vget_common(mp, NULL, cnp->cn_lkflags,
			    dnp, newfid, vpp, cnp->cn_nameptr);
			if (error != 0)
				goto out;
		} else {
			/* Not found return NOENTRY.*/
			goto out;
		}

		if ((cnp->cn_flags & MAKEENTRY) != 0)
			cache_enter(P9FS_NTOV(dnp), *vpp, cnp);
	}
	P9_DEBUG(VOPS, "%s: created file under vp %p node %p fid %ju\n",
	    __func__, *vpp, dnp, (uintmax_t)dvfid->fid);
	/* Clunk the open ofid. */
	if (ofid != NULL)
		(void)p9_client_clunk(ofid);

	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;
	return (0);
out:
	if (ofid != NULL)
		(void)p9_client_clunk(ofid);

	if (newfid != NULL)
		(void)p9_client_clunk(newfid);

	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;
	return (error);
}

/*
 * This is the main file creation VOP. Make the permissions of the new
 * file and call the create_common common code to complete the create.
 */
static int
p9fs_create(struct vop_create_args *ap)
{
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	uint32_t mode;
	struct p9fs_node *dnp;
	struct p9fs_inode *dinode;
	uint32_t perm;
	int ret;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;
	dnp = P9FS_VTON(dvp);
	dinode = &dnp->inode;
	mode = MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode);
	perm = p9fs_unix2p9_mode(mode);

	P9_DEBUG(VOPS, "%s: dvp %p\n", __func__, dvp);

	ret = create_common(dnp, cnp, NULL, perm, P9PROTO_ORDWR, vpp);
	if (ret == 0) {
		P9FS_INCR_LINKS(dinode);
	}

	return (ret);
}

/*
 * p9fs_mkdir is the main directory creation vop. Make the permissions of the new dir
 * and call the create_common common code to complete the create.
 */
static int
p9fs_mkdir(struct vop_mkdir_args *ap)
{
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	uint32_t mode;
	struct p9fs_node *dnp;
	struct p9fs_inode *dinode;
	uint32_t perm;
	int ret;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;
	dnp = P9FS_VTON(dvp);
	dinode = &dnp->inode;
	mode = MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode);
	perm = p9fs_unix2p9_mode(mode | S_IFDIR);

	P9_DEBUG(VOPS, "%s: dvp %p\n", __func__, dvp);

	ret = create_common(dnp, cnp, NULL, perm, P9PROTO_ORDWR, vpp);
	if (ret == 0)
		P9FS_INCR_LINKS(dinode);

	return (ret);
}

/*
 * p9fs_mknod is the main node creation vop. Make the permissions of the new node
 * and call the create_common common code to complete the create.
 */
static int
p9fs_mknod(struct vop_mknod_args *ap)
{
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	uint32_t mode;
	struct p9fs_node *dnp;
	struct p9fs_inode *dinode;
	uint32_t perm;
	int ret;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;
	dnp = P9FS_VTON(dvp);
	dinode = &dnp->inode;
	mode = MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode);
	perm = p9fs_unix2p9_mode(mode);

	P9_DEBUG(VOPS, "%s: dvp %p\n", __func__, dvp);

	ret = create_common(dnp, cnp, NULL, perm, P9PROTO_OREAD, vpp);
	if (ret == 0) {
		P9FS_INCR_LINKS(dinode);
	}

	return (ret);
}

/* Convert open mode permissions to P9 */
static int
p9fs_uflags_mode(int uflags, int extended)
{
	uint32_t ret;

	/* Convert first to O flags.*/
	uflags = OFLAGS(uflags);

	switch (uflags & 3) {

	case O_RDONLY:
	    ret = P9PROTO_OREAD;
	    break;

	case O_WRONLY:
	    ret = P9PROTO_OWRITE;
	    break;

	case O_RDWR:
	    ret = P9PROTO_ORDWR;
	    break;
	}

	if (extended) {
		if (uflags & O_EXCL)
			ret |= P9PROTO_OEXCL;

		if (uflags & O_APPEND)
			ret |= P9PROTO_OAPPEND;
	}

	return (ret);
}

/*
 * This is the main open VOP for every file open. If the file is already
 * open, then increment and return. If there is no open fid for this file,
 * there needs to be a client_walk which creates a new open fid for this file.
 * Once we have a open fid, call the open on this file with the mode creating
 * the vobject.
 */
static int
p9fs_open(struct vop_open_args *ap)
{
	int error;
	struct vnode *vp;
	struct p9fs_node *np;
	struct p9fs_session *vses;
	struct p9_fid *vofid, *vfid;
	size_t filesize;
	uint32_t mode;

	error = 0;
	vp = ap->a_vp;
	np = P9FS_VTON(vp);
	vses = np->p9fs_ses;

	P9_DEBUG(VOPS, "%s: vp %p\n", __func__, vp);

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK)
		return (EOPNOTSUPP);

	error = p9fs_reload_stats_dotl(vp, ap->a_cred);
	if (error != 0)
		return (error);

	ASSERT_VOP_LOCKED(vp, __func__);
	/*
	 * Invalidate the pages of the vm_object cache if the file is modified
	 * based on the flag set in reload stats
	 */
	if (vp->v_type == VREG && (np->flags & P9FS_NODE_MODIFIED) != 0) {
		error = vinvalbuf(vp, 0, 0, 0);
		if (error != 0)
			return (error);
		np->flags &= ~P9FS_NODE_MODIFIED;
	}

	vfid = p9fs_get_fid(vses->clnt, np, ap->a_cred, VFID, -1, &error);
	if (error != 0)
		return (error);

	/*
	 * Translate kernel fflags to 9p mode
	 */
	mode = p9fs_uflags_mode(ap->a_mode, 1);

	/*
	 * Search the fid in vofid_list for current user. If found increase the open
	 * count and return. If not found clone a new fid and open the file using
	 * that cloned fid.
	 */
	vofid = p9fs_get_fid(vses->clnt, np, ap->a_cred, VOFID, mode, &error);
	if (vofid != NULL) {
		vofid->v_opens++;
		return (0);
	} else {
		/*vofid is the open fid for this file.*/
		vofid = p9_client_walk(vfid, 0, NULL, 1, &error);
		if (error != 0)
			return (error);
	}

	error = p9_client_open(vofid, mode);
	if (error != 0)
		p9_client_clunk(vofid);
	else {
		vofid->v_opens = 1;
		filesize = np->inode.i_size;
		vnode_create_vobject(vp, filesize, ap->a_td);
		p9fs_fid_add(np, vofid, VOFID);
	}

	return (error);
}

/*
 * Close the open references. Just reduce the open count on vofid and return.
 * Let clunking of VOFID happen in p9fs_reclaim.
 */
static int
p9fs_close(struct vop_close_args *ap)
{
	struct vnode *vp;
	struct p9fs_node *np;
	struct p9fs_session *vses;
	struct p9_fid *vofid;
	int error;

	vp = ap->a_vp;
	np = P9FS_VTON(vp);

	if (np == NULL)
		return (0);

	vses = np->p9fs_ses;
	error = 0;

	P9_DEBUG(VOPS, "%s: file_name %s\n", __func__, np->inode.i_name);

	/*
	 * Translate kernel fflags to 9p mode
	 */
	vofid = p9fs_get_fid(vses->clnt, np, ap->a_cred, VOFID,
	    p9fs_uflags_mode(ap->a_fflag, 1), &error);
	if (vofid == NULL)
		return (0);

	vofid->v_opens--;

	return (0);
}

/* Helper routine for checking if fileops are possible on this file */
static int
p9fs_check_possible(struct vnode *vp, struct vattr *vap, mode_t mode)
{

	/* Check if we are allowed to write */
	switch (vap->va_type) {
	case VDIR:
	case VLNK:
	case VREG:
		/*
		 * Normal nodes: check if we're on a read-only mounted
		 * file system and bail out if we're trying to write.
		 */
		if ((mode & VMODIFY_PERMS) && (vp->v_mount->mnt_flag & MNT_RDONLY))
			return (EROFS);
		break;
	case VBLK:
	case VCHR:
	case VSOCK:
	case VFIFO:
		/*
		 * Special nodes: even on read-only mounted file systems
		 * these are allowed to be written to if permissions allow.
		 */
		break;
	default:
		/* No idea what this is */
		return (EINVAL);
	}

	return (0);
}

/* Check the access permissions of the file. */
static int
p9fs_access(struct vop_access_args *ap)
{
	struct vnode *vp;
	accmode_t accmode;
	struct ucred *cred;
	struct vattr vap;
	int error;

	vp = ap->a_vp;
	accmode = ap->a_accmode;
	cred = ap->a_cred;

	P9_DEBUG(VOPS, "%s: vp %p\n", __func__, vp);

	/* make sure getattr is working correctly and is defined.*/
	error = VOP_GETATTR(vp, &vap, cred);
	if (error != 0)
		return (error);

	error = p9fs_check_possible(vp, &vap, accmode);
	if (error != 0)
		return (error);

	/* Call the Generic Access check in VOPS*/
	error = vaccess(vp->v_type, vap.va_mode, vap.va_uid, vap.va_gid, accmode,
	    cred);


	return (error);
}

/*
 * Reload the file stats from the server and update the inode structure present
 * in p9fs node.
 */
int
p9fs_reload_stats_dotl(struct vnode *vp, struct ucred *cred)
{
	struct p9_stat_dotl *stat;
	int error;
	struct p9fs_node *node;
	struct p9fs_session *vses;
	struct p9_fid *vfid;

	error = 0;
	node = P9FS_VTON(vp);
	vses = node->p9fs_ses;

	vfid = p9fs_get_fid(vses->clnt, node, cred, VOFID, P9PROTO_OREAD, &error);
	if (vfid == NULL) {
		vfid = p9fs_get_fid(vses->clnt, node, cred, VFID, -1, &error);
		if (error)
			return (error);
	}

	stat = uma_zalloc(p9fs_getattr_zone, M_WAITOK | M_ZERO);

	error = p9_client_getattr(vfid, stat, P9PROTO_STATS_ALL);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: p9_client_getattr failed: %d\n", __func__, error);
		goto out;
	}

	/* Init the vnode with the disk info */
	p9fs_stat_vnode_dotl(stat, vp);
out:
	if (stat != NULL) {
		uma_zfree(p9fs_getattr_zone, stat);
	}

	return (error);
}

/*
 * Read the current inode values into the vap attr. We reload the stats from
 * the server.
 */
static int
p9fs_getattr_dotl(struct vop_getattr_args *ap)
{
	struct vnode *vp;
	struct vattr *vap;
	struct p9fs_node *node;
	struct p9fs_inode *inode;
	int error;

	vp = ap->a_vp;
	vap = ap->a_vap;
	node = P9FS_VTON(vp);

	if (node == NULL)
		return (ENOENT);

	inode = &node->inode;

	P9_DEBUG(VOPS, "%s: %u %u\n", __func__, inode->i_mode, IFTOVT(inode->i_mode));

	/* Reload our stats once to get the right values.*/
	error = p9fs_reload_stats_dotl(vp, ap->a_cred);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: failed: %d\n", __func__, error);
		return (error);
	}

	/* Basic info */
	VATTR_NULL(vap);

	vap->va_atime.tv_sec = inode->i_atime;
	vap->va_mtime.tv_sec = inode->i_mtime;
	vap->va_ctime.tv_sec = inode->i_ctime;
	vap->va_atime.tv_nsec = inode->i_atime_nsec;
	vap->va_mtime.tv_nsec = inode->i_mtime_nsec;
	vap->va_ctime.tv_nsec = inode->i_ctime_nsec;
	vap->va_type = IFTOVT(inode->i_mode);
	vap->va_mode = inode->i_mode;
	vap->va_uid = inode->n_uid;
	vap->va_gid = inode->n_gid;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = inode->i_size;
	vap->va_nlink = inode->i_links_count;
	vap->va_blocksize = inode->blksize;
	vap->va_fileid = inode->i_qid_path;
	vap->va_flags = inode->i_flags;
	vap->va_gen = inode->gen;
	vap->va_filerev = inode->data_version;
	vap->va_vaflags = 0;
	vap->va_bytes = inode->blocks * P9PROTO_TGETATTR_BLK;

	return (0);
}

/* Convert a standard FreeBSD permission to P9. */
static uint32_t
p9fs_unix2p9_mode(uint32_t mode)
{
	uint32_t res;

	res = mode & 0777;
	if (S_ISDIR(mode))
		res |= P9PROTO_DMDIR;
	if (S_ISSOCK(mode))
		res |= P9PROTO_DMSOCKET;
	if (S_ISLNK(mode))
		res |= P9PROTO_DMSYMLINK;
	if (S_ISFIFO(mode))
		res |= P9PROTO_DMNAMEDPIPE;
	if ((mode & S_ISUID) == S_ISUID)
		res |= P9PROTO_DMSETUID;
	if ((mode & S_ISGID) == S_ISGID)
		res |= P9PROTO_DMSETGID;
	if ((mode & S_ISVTX) == S_ISVTX)
		res |= P9PROTO_DMSETVTX;

	return (res);
}

/* Update inode with the stats read from server.(9P2000.L version) */
int
p9fs_stat_vnode_dotl(struct p9_stat_dotl *stat, struct vnode *vp)
{
	struct p9fs_node *np;
	struct p9fs_inode *inode;

	np = P9FS_VTON(vp);
	inode = &np->inode;

	ASSERT_VOP_LOCKED(vp, __func__);
	/* Update the pager size if file size changes on host */
	if (inode->i_size != stat->st_size) {
		inode->i_size = stat->st_size;
		if (vp->v_type == VREG)
			vnode_pager_setsize(vp, inode->i_size);
	}

	inode->i_mtime = stat->st_mtime_sec;
	inode->i_atime = stat->st_atime_sec;
	inode->i_ctime = stat->st_ctime_sec;
	inode->i_mtime_nsec = stat->st_mtime_nsec;
	inode->i_atime_nsec = stat->st_atime_nsec;
	inode->i_ctime_nsec = stat->st_ctime_nsec;
	inode->n_uid = stat->st_uid;
	inode->n_gid = stat->st_gid;
	inode->i_mode = stat->st_mode;
	vp->v_type = IFTOVT(inode->i_mode);
	inode->i_links_count = stat->st_nlink;
	inode->blksize = stat->st_blksize;
	inode->blocks = stat->st_blocks;
	inode->gen = stat->st_gen;
	inode->data_version = stat->st_data_version;

	ASSERT_VOP_LOCKED(vp, __func__);
	/* Setting a flag if file changes based on qid version */
	if (np->vqid.qid_version != stat->qid.version)
		np->flags |= P9FS_NODE_MODIFIED;
	memcpy(&np->vqid, &stat->qid, sizeof(stat->qid));

	return (0);
}

/*
 * Write the current in memory inode stats into persistent stats structure
 * to write to the server(for linux version).
 */
static int
p9fs_inode_to_iattr(struct p9fs_inode *inode, struct p9_iattr_dotl *p9attr)
{
	p9attr->size = inode->i_size;
	p9attr->mode = inode->i_mode;
	p9attr->uid = inode->n_uid;
	p9attr->gid = inode->n_gid;
	p9attr->atime_sec = inode->i_atime;
	p9attr->atime_nsec = inode->i_atime_nsec;
	p9attr->mtime_sec = inode->i_mtime;
	p9attr->mtime_nsec = inode->i_mtime_nsec;

	return (0);
}

/*
 * Modify the ownership of a file whenever the chown is called on the
 * file.
 */
static int
p9fs_chown(struct vnode *vp, uid_t uid, gid_t gid, struct ucred *cred,
    struct thread *td)
{
	struct p9fs_node *np;
	struct p9fs_inode *inode;
	uid_t ouid;
	gid_t ogid;
	int error;

	np = P9FS_VTON(vp);
	inode = &np->inode;

	if (uid == (uid_t)VNOVAL)
		uid = inode->n_uid;
	if (gid == (gid_t)VNOVAL)
		gid = inode->n_gid;
	/*
	 * To modify the ownership of a file, must possess VADMIN for that
	 * file.
	 */
	if ((error = VOP_ACCESSX(vp, VWRITE_OWNER, cred, td)))
		return (error);
	/*
	 * To change the owner of a file, or change the group of a file to a
	 * group of which we are not a member, the caller must have
	 * privilege.
	 */
	if (((uid != inode->n_uid && uid != cred->cr_uid) ||
	    (gid != inode->n_gid && !groupmember(gid, cred))) &&
	    (error = priv_check_cred(cred, PRIV_VFS_CHOWN)))
		return (error);

	ogid = inode->n_gid;
	ouid = inode->n_uid;

	inode->n_gid = gid;
	inode->n_uid = uid;

	if ((inode->i_mode & (ISUID | ISGID)) &&
	    (ouid != uid || ogid != gid)) {

		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID))
			inode->i_mode &= ~(ISUID | ISGID);
	}
	P9_DEBUG(VOPS, "%s: vp %p, cred %p, td %p - ret OK\n", __func__, vp, cred, td);

	return (0);
}

/*
 * Update the in memory inode with all chmod new permissions/mode. Typically a
 * setattr is called to update it to server.
 */
static int
p9fs_chmod(struct vnode *vp, uint32_t  mode, struct ucred *cred, struct thread *td)
{
	struct p9fs_node *np;
	struct p9fs_inode *inode;
	uint32_t nmode;
	int error;

	np = P9FS_VTON(vp);
	inode = &np->inode;

	P9_DEBUG(VOPS, "%s: vp %p, mode %x, cred %p, td %p\n",  __func__, vp, mode, cred, td);
	/*
	 * To modify the permissions on a file, must possess VADMIN
	 * for that file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);

	/*
	 * Privileged processes may set the sticky bit on non-directories,
	 * as well as set the setgid bit on a file with a group that the
	 * process is not a member of. Both of these are allowed in
	 * jail(8).
	 */
	if (vp->v_type != VDIR && (mode & S_ISTXT)) {
		if (priv_check_cred(cred, PRIV_VFS_STICKYFILE))
			return (EFTYPE);
	}
	if (!groupmember(inode->n_gid, cred) && (mode & ISGID)) {
		error = priv_check_cred(cred, PRIV_VFS_SETGID);
		if (error != 0)
			return (error);
	}

	/*
	 * Deny setting setuid if we are not the file owner.
	 */
	if ((mode & ISUID) && inode->n_uid != cred->cr_uid) {
		error = priv_check_cred(cred, PRIV_VFS_ADMIN);
		if (error != 0)
			return (error);
	}
	nmode = inode->i_mode;
	nmode &= ~ALLPERMS;
	nmode |= (mode & ALLPERMS);
	inode->i_mode = nmode;

	P9_DEBUG(VOPS, "%s: to mode %x  %d \n ", __func__, nmode, error);

	return (error);
}

/*
 * Set the attributes of a file referenced by fid. A valid bitmask is sent
 * in request selecting which fields to set
 */
static int
p9fs_setattr_dotl(struct vop_setattr_args *ap)
{
	struct vnode *vp;
	struct vattr *vap;
	struct p9fs_node *node;
	struct p9fs_inode *inode;
	struct ucred *cred;
	struct thread *td;
	struct p9_iattr_dotl *p9attr;
	struct p9fs_session *vses;
	struct p9_fid *vfid;
	uint64_t oldfilesize;
	int error;

	vp = ap->a_vp;
	vap = ap->a_vap;
	node = P9FS_VTON(vp);
	inode = &node->inode;
	cred = ap->a_cred;
	td = curthread;
	vses = node->p9fs_ses;
	error = 0;

	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    (vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		P9_DEBUG(ERROR, "%s: unsettable attribute\n", __func__);
		return (EINVAL);
	}
	/* Disallow write attempts on read only filesystem */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/* Setting of flags is not supported */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/* Allocate p9attr struct */
	p9attr = uma_zalloc(p9fs_setattr_zone, M_WAITOK | M_ZERO);
	if (p9attr == NULL)
		return (ENOMEM);

	/* Check if we need to change the ownership of the file*/
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		P9_DEBUG(VOPS, "%s: vp:%p td:%p uid/gid %x/%x\n", __func__,
		    vp, td, vap->va_uid, vap->va_gid);

		error = p9fs_chown(vp, vap->va_uid, vap->va_gid, cred, td);
		p9attr->valid |= P9PROTO_SETATTR_UID | P9PROTO_SETATTR_GID |
			P9PROTO_SETATTR_MODE;
		if (error)
			goto out;
	}

	/* Check for mode changes */
	if (vap->va_mode != (mode_t)VNOVAL) {
		P9_DEBUG(VOPS, "%s: vp:%p td:%p mode %x\n", __func__, vp, td,
		    vap->va_mode);

		error = p9fs_chmod(vp, (int)vap->va_mode, cred, td);
		p9attr->valid |= P9PROTO_SETATTR_MODE;
		if (error)
			goto out;
	}

	/* Update the size of the file and update mtime */
	if (vap->va_size != (uint64_t)VNOVAL) {
		P9_DEBUG(VOPS, "%s: vp:%p td:%p size:%jx\n", __func__,
		    vp, td, (uintmax_t)vap->va_size);
		switch (vp->v_type) {
			case VDIR:
				error = EISDIR;
				goto out;
			case VLNK:
			case VREG:
				/* Invalidate cached pages of vp */
				error = vinvalbuf(vp, 0, 0, 0);
				if (error)
					goto out;
				oldfilesize = inode->i_size;
				inode->i_size = vap->va_size;
				/* Update the p9fs_inode time */
				p9fs_itimes(vp);
				p9attr->valid |= P9PROTO_SETATTR_SIZE |
				    P9PROTO_SETATTR_ATIME |
				    P9PROTO_SETATTR_MTIME |
				    P9PROTO_SETATTR_ATIME_SET |
				    P9PROTO_SETATTR_MTIME_SET ;
				break;
			default:
				goto out;
		}
	} else if (vap->va_atime.tv_sec != VNOVAL ||
		    vap->va_mtime.tv_sec != VNOVAL) {
		P9_DEBUG(VOPS, "%s: vp:%p td:%p time a/m %jx/%jx/\n",
		    __func__, vp, td, (uintmax_t)vap->va_atime.tv_sec,
		    (uintmax_t)vap->va_mtime.tv_sec);
		/* Update the p9fs_inode times */
		p9fs_itimes(vp);
		p9attr->valid |= P9PROTO_SETATTR_ATIME |
			P9PROTO_SETATTR_MTIME | P9PROTO_SETATTR_ATIME_SET |
			P9PROTO_SETATTR_MTIME_SET;
	}

	vfid = p9fs_get_fid(vses->clnt, node, cred, VOFID, P9PROTO_OWRITE, &error);
	if (vfid == NULL) {
		vfid = p9fs_get_fid(vses->clnt, node, cred, VFID, -1, &error);
		if (error)
			goto out;
	}

	/* Write the inode structure values into p9attr */
	p9fs_inode_to_iattr(inode, p9attr);
	error = p9_client_setattr(vfid, p9attr);
	if (vap->va_size != (uint64_t)VNOVAL && vp->v_type == VREG) {
		if (error)
			inode->i_size = oldfilesize;
		else
			vnode_pager_setsize(vp, inode->i_size);
	}
out:
	if (p9attr) {
		uma_zfree(p9fs_setattr_zone, p9attr);
	}
	P9_DEBUG(VOPS, "%s: error: %d\n", __func__, error);
	return (error);
}

struct open_fid_state {
	struct p9_fid *vofid;
	int fflags;
	int opened;
};

/*
 * TODO: change this to take P9PROTO_* mode and avoid routing through
 * VOP_OPEN, factoring out implementation of p9fs_open.
 */
static int
p9fs_get_open_fid(struct vnode *vp, int fflags, struct ucred *cr, struct open_fid_state *statep)
{
	struct p9fs_node *np;
	struct p9fs_session *vses;
	struct p9_fid *vofid;
	int mode = p9fs_uflags_mode(fflags, TRUE);
	int error = 0;

	statep->opened = FALSE;

	np = P9FS_VTON(vp);
	vses = np->p9fs_ses;
	vofid = p9fs_get_fid(vses->clnt, np, cr, VOFID, mode, &error);
	if (vofid == NULL) {
		error = VOP_OPEN(vp, fflags, cr, curthread, NULL);
		if (error) {
			return (error);
		}
		vofid = p9fs_get_fid(vses->clnt, np, cr, VOFID, mode, &error);
		if (vofid == NULL) {
			return (EBADF);
		}
		statep->fflags = fflags;
		statep->opened = TRUE;
	}
	statep->vofid = vofid;
	return (0);
}

static void
p9fs_release_open_fid(struct vnode *vp, struct ucred *cr, struct open_fid_state *statep)
{
	if (statep->opened) {
		(void) VOP_CLOSE(vp, statep->fflags, cr, curthread);
	}
}

/*
 * An I/O buffer is used to to do any transfer. The uio is the vfs structure we
 * need to copy data into. As long as resid is greater than zero, we call
 * client_read to read data from offset(offset into the file) in the open fid
 * for the file into the I/O buffer. The data is read into the user data buffer.
 */
static int
p9fs_read(struct vop_read_args *ap)
{
	struct vnode *vp;
	struct uio *uio;
	struct p9fs_node *np;
	uint64_t offset;
	int64_t ret;
	uint64_t resid;
	uint32_t count;
	int error;
	char *io_buffer = NULL;
	uint64_t filesize;
	struct open_fid_state ostate;

	vp = ap->a_vp;
	uio = ap->a_uio;
	np = P9FS_VTON(vp);
	error = 0;

	if (VN_ISDEV(vp))
		return (EOPNOTSUPP);
	if (vp->v_type != VREG)
		return (EISDIR);
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);

	error = p9fs_get_open_fid(vp, FREAD, ap->a_cred, &ostate);
	if (error)
		return (error);

	/* where in the file are we to start reading */
	offset = uio->uio_offset;
	filesize = np->inode.i_size;
	if (uio->uio_offset >= filesize)
		goto out;

	P9_DEBUG(VOPS, "%s: called %jd at %ju\n",
	    __func__, (intmax_t)uio->uio_resid, (uintmax_t)uio->uio_offset);

	/* Work with a local buffer from the pool for this vop */

	io_buffer = uma_zalloc(p9fs_io_buffer_zone, M_WAITOK | M_ZERO);
	while ((resid = uio->uio_resid) > 0) {
		if (offset >= filesize)
			break;
		count = MIN(filesize - uio->uio_offset , resid);
		if (count == 0)
			break;

		/* Copy count bytes into the uio */
		ret = p9_client_read(ostate.vofid, offset, count, io_buffer);
		/*
		 * This is the only place in the entire p9fs where we check the
		 * error for < 0 as p9_client_read/write return the number of
		 * bytes instead of an error code. In this case if ret is < 0,
		 * it means there is an IO error.
		 */
		if (ret < 0) {
			error = -ret;
			goto out;
		}
		error = uiomove(io_buffer, ret, uio);
		if (error != 0)
			goto out;

		offset += ret;
	}
	uio->uio_offset = offset;
out:
	uma_zfree(p9fs_io_buffer_zone, io_buffer);
	p9fs_release_open_fid(vp, ap->a_cred, &ostate);

	return (error);
}

/*
 * The user buffer contains the data to be written. This data is copied first
 * from uio into I/O buffer. This I/O  buffer is used to do the client_write to
 * the fid of the file starting from the offset given upto count bytes. The
 * number of bytes written is returned to the caller.
 */
static int
p9fs_write(struct vop_write_args *ap)
{
	struct vnode *vp;
	struct uio *uio;
	struct p9fs_node *np;
	uint64_t off, offset;
	int64_t ret;
	uint64_t resid, bytes_written;
	uint32_t count;
	int error, ioflag;
	uint64_t file_size;
	char *io_buffer = NULL;
	struct open_fid_state ostate;

	vp = ap->a_vp;
	uio = ap->a_uio;
	np = P9FS_VTON(vp);
	error = 0;
	ioflag = ap->a_ioflag;

	error = p9fs_get_open_fid(vp, FWRITE, ap->a_cred, &ostate);
	if (error)
		return (error);

	P9_DEBUG(VOPS, "%s: %#zx at %#jx\n",
	    __func__, uio->uio_resid, (uintmax_t)uio->uio_offset);

	if (uio->uio_offset < 0) {
		error = EINVAL;
		goto out;
	}
	if (uio->uio_resid == 0)
		goto out;

	file_size = np->inode.i_size;

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = file_size;
		break;
	case VDIR:
		return (EISDIR);
	case VLNK:
		break;
	default:
		panic("%s: bad file type vp: %p", __func__, vp);
	}

	resid = uio->uio_resid;
	offset = uio->uio_offset;
	bytes_written = 0;
	error = 0;

	io_buffer = uma_zalloc(p9fs_io_buffer_zone, M_WAITOK | M_ZERO);
	while ((resid = uio->uio_resid) > 0) {
                off = 0;
		count = MIN(resid, P9FS_IOUNIT);
		error = uiomove(io_buffer, count, uio);

		if (error != 0) {
			P9_DEBUG(ERROR, "%s: uiomove failed: %d\n", __func__, error);
			goto out;
		}

		/* While count still exists, keep writing.*/
		while (count > 0) {
			/* Copy count bytes from the uio */
			ret = p9_client_write(ostate.vofid, offset, count,
                                io_buffer + off);
			if (ret < 0) {
				if (bytes_written == 0) {
					error = -ret;
					goto out;
				} else {
					break;
				}
			}
			P9_DEBUG(VOPS, "%s: write %#zx at %#jx\n",
			    __func__, uio->uio_resid, (uintmax_t)uio->uio_offset);

                        off += ret;
			offset += ret;
			bytes_written += ret;
			count -= ret;
		}
	}
	/* Update the fields in the node to reflect the change*/
	if (file_size < uio->uio_offset + uio->uio_resid) {
		np->inode.i_size = uio->uio_offset + uio->uio_resid;
		vnode_pager_setsize(vp, uio->uio_offset + uio->uio_resid);
	}
out:
	if (io_buffer)
		uma_zfree(p9fs_io_buffer_zone, io_buffer);
	p9fs_release_open_fid(vp, ap->a_cred, &ostate);

	return (error);
}

/*
 * Common handler of all removal-related VOPs (e.g. rmdir, rm). Perform the
 * client_remove op to send messages to remove the node's fid on the server.
 * After that, does a node metadata cleanup on client side.
 */
static int
remove_common(struct p9fs_node *dnp, struct p9fs_node *np, const char *name,
    struct ucred *cred)
{
	int error;
	struct p9fs_session *vses;
	struct vnode *vp;
	struct p9_fid *vfid;

	error = 0;
	vses = np->p9fs_ses;
	vp = P9FS_NTOV(np);

	vfid = p9fs_get_fid(vses->clnt, dnp, cred, VFID, -1, &error);
	if (error != 0)
		return (error);

	error = p9_client_unlink(vfid, name,
	    np->v_node->v_type == VDIR ? P9PROTO_UNLINKAT_REMOVEDIR : 0);
	if (error != 0)
		return (error);

	/* Remove all non-open fids associated with the vp */
	if (np->inode.i_links_count == 1)
		p9fs_fid_remove_all(np, TRUE);

	/* Invalidate all entries of vnode from name cache and hash list. */
	cache_purge(vp);
	vfs_hash_remove(vp);

	np->flags |= P9FS_NODE_DELETED;

	return (error);
}

/* Remove vop for all files. Call common code for remove and adjust links */
static int
p9fs_remove(struct vop_remove_args *ap)
{
	struct vnode *vp;
	struct p9fs_node *np;
	struct vnode *dvp;
	struct p9fs_node *dnp;
	struct p9fs_inode *dinode;
	struct componentname *cnp;
	int error;

	cnp = ap->a_cnp;
	vp = ap->a_vp;
	np = P9FS_VTON(vp);
	dvp = ap->a_dvp;
	dnp = P9FS_VTON(dvp);
	dinode = &dnp->inode;

	P9_DEBUG(VOPS, "%s: vp %p node %p \n", __func__, vp, np);

	if (vp->v_type == VDIR)
		return (EISDIR);

	error = remove_common(dnp, np, cnp->cn_nameptr, cnp->cn_cred);
	if (error == 0)
		P9FS_DECR_LINKS(dinode);

	return (error);
}

/* Remove vop for all directories. Call common code for remove and adjust links */
static int
p9fs_rmdir(struct vop_rmdir_args *ap)
{
	struct vnode *vp;
	struct p9fs_node *np;
	struct vnode *dvp;
	struct p9fs_node *dnp;
	struct p9fs_inode *dinode;
	struct componentname *cnp;
	int error;

	cnp = ap->a_cnp;
	vp = ap->a_vp;
	np = P9FS_VTON(vp);
	dvp = ap->a_dvp;
	dnp = P9FS_VTON(dvp);
	dinode = &dnp->inode;

	P9_DEBUG(VOPS, "%s: vp %p node %p \n", __func__, vp, np);

	error = remove_common(dnp, np, cnp->cn_nameptr, cnp->cn_cred);
	if (error == 0)
		P9FS_DECR_LINKS(dinode);

	return (error);
}

/*
 * Create symlinks. Make the permissions and call create_common code
 * for Soft links.
 */
static int
p9fs_symlink(struct vop_symlink_args *ap)
{
	struct vnode *dvp;
	struct vnode **vpp;
	struct vattr *vap;
	struct componentname *cnp;
	char *symtgt;
	struct p9fs_node *dnp;
	struct p9fs_session *vses;
	struct mount *mp;
	struct p9_fid *dvfid, *newfid;
	int error;
	char tmpchr;
	gid_t gid;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	vap = ap->a_vap;
	cnp = ap->a_cnp;
	symtgt = (char*)(uintptr_t) ap->a_target;
	dnp = P9FS_VTON(dvp);
	vses = dnp->p9fs_ses;
	mp = vses->p9fs_mount;
	newfid = NULL;
	error = 0;
	gid = vap->va_gid;

	P9_DEBUG(VOPS, "%s: dvp %p\n", __func__, dvp);

	/*
	 * Save the character present at namelen in nameptr string and
	 * null terminate the character to get the search name for p9_dir_walk
	 */
	tmpchr = cnp->cn_nameptr[cnp->cn_namelen];
	cnp->cn_nameptr[cnp->cn_namelen] = '\0';

	dvfid = p9fs_get_fid(vses->clnt, dnp, cnp->cn_cred, VFID, -1, &error);
	if (error != 0)
		goto out;

	error = p9_create_symlink(dvfid, cnp->cn_nameptr, symtgt, gid);
	if (error != 0)
		goto out;

	/*create vnode for symtgt */
	newfid = p9_client_walk(dvfid, 1, &cnp->cn_nameptr, 1, &error);
	if (newfid != NULL) {
		error = p9fs_vget_common(mp, NULL, cnp->cn_lkflags,
		    dnp, newfid, vpp, cnp->cn_nameptr);
		if (error != 0)
			goto out;
	} else
		goto out;

	if ((cnp->cn_flags & MAKEENTRY) != 0) {
		cache_enter(P9FS_NTOV(dnp), *vpp, cnp);
	}
	P9_DEBUG(VOPS, "%s: created file under vp %p node %p fid %ju\n",
	    __func__, *vpp, dnp, (uintmax_t)dvfid->fid);

	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;
	return (error);

out:
	if (newfid != NULL)
		p9_client_clunk(newfid);
	cnp->cn_nameptr[cnp->cn_namelen] = tmpchr;
	return (error);
}

/* Create hard link */
static int
p9fs_link(struct vop_link_args *ap)
{
	struct vnode *vp;
	struct vnode *tdvp;
	struct componentname *cnp;
	struct p9fs_node *dnp;
	struct p9fs_node *np;
	struct p9fs_inode *inode;
	struct p9fs_session *vses;
	struct p9_fid *dvfid, *oldvfid;
	int error;

	vp = ap->a_vp;
	tdvp = ap->a_tdvp;
	cnp = ap->a_cnp;
	dnp = P9FS_VTON(tdvp);
	np = P9FS_VTON(vp);
	inode = &np->inode;
	vses = np->p9fs_ses;
	error = 0;

	P9_DEBUG(VOPS, "%s: tdvp %p vp %p\n", __func__, tdvp, vp);

	dvfid = p9fs_get_fid(vses->clnt, dnp, cnp->cn_cred, VFID, -1, &error);
	if (error != 0)
		return (error);
	oldvfid = p9fs_get_fid(vses->clnt, np, cnp->cn_cred, VFID, -1, &error);
	if (error != 0)
		return (error);

	error = p9_create_hardlink(dvfid, oldvfid, cnp->cn_nameptr);
	if (error != 0)
		return (error);
	/* Increment ref count on the inode */
	P9FS_INCR_LINKS(inode);

	return (0);
}

/* Read contents of the symbolic link */
static int
p9fs_readlink(struct vop_readlink_args *ap)
{
	struct vnode *vp;
	struct uio *uio;
	struct p9fs_node *dnp;
	struct p9fs_session *vses;
	struct p9_fid *dvfid;
	int error, len;
	char *target;

	vp = ap->a_vp;
	uio = ap->a_uio;
	dnp = P9FS_VTON(vp);
	vses = dnp->p9fs_ses;
	error = 0;

	P9_DEBUG(VOPS, "%s: vp %p\n", __func__, vp);

	dvfid = p9fs_get_fid(vses->clnt, dnp, ap->a_cred, VFID, -1, &error);
	if (error != 0)
		return (error);

	error = p9_readlink(dvfid, &target);
	if (error != 0)
		return (error);

	len = strlen(target);
	error = uiomove(target, len, uio);

	return (0);
}

/*
 * Iterate through a directory. An entire 8k data is read into the I/O buffer.
 * This buffer is parsed to make dir entries and fed to the user buffer to
 * complete it to the VFS.
 */
static int
p9fs_readdir(struct vop_readdir_args *ap)
{
	struct uio *uio;
	struct vnode *vp;
	struct dirent cde;
	int64_t offset;
	uint64_t diroffset;
	struct p9fs_node *np;
	int error;
	int32_t count;
	struct p9_client *clnt;
	struct p9_dirent dent;
	char *io_buffer;
	struct p9_fid *vofid;

	uio = ap->a_uio;
	vp = ap->a_vp;
	np = P9FS_VTON(ap->a_vp);
	offset = 0;
	diroffset = 0;
	error = 0;
	count = 0;
	clnt = np->p9fs_ses->clnt;

	P9_DEBUG(VOPS, "%s: vp %p, offset %jd, resid %zd\n", __func__, vp, (intmax_t) uio->uio_offset, uio->uio_resid);

	if (ap->a_uio->uio_iov->iov_len <= 0)
		return (EINVAL);

	if (vp->v_type != VDIR)
		return (ENOTDIR);

	vofid = p9fs_get_fid(clnt, np, ap->a_cred, VOFID, P9PROTO_OREAD, &error);
	if (vofid == NULL) {
		P9_DEBUG(ERROR, "%s: NULL FID\n", __func__);
		return (EBADF);
	}

	if (ap->a_eofflag != NULL)
		*ap->a_eofflag = 0;

	io_buffer = uma_zalloc(p9fs_io_buffer_zone, M_WAITOK);

	/* We haven't reached the end yet. read more. */
	diroffset = uio->uio_offset;
	while (uio->uio_resid >= sizeof(struct dirent)) {
		/*
		 * We need to read more data as what is indicated by filesize because
		 * filesize is based on data stored in struct dirent structure but
		 * we read data in struct p9_dirent format which has different size.
		 * Hence we read max data(P9FS_IOUNIT) everytime from host, convert
		 * it into struct dirent structure and send it back.
		 */
		count = P9FS_IOUNIT;
		bzero(io_buffer, P9FS_MTU);
		count = p9_client_readdir(vofid, (char *)io_buffer,
		    diroffset, count);

		if (count == 0) {
			if (ap->a_eofflag != NULL)
				*ap->a_eofflag = 1;
			break;
		}

		if (count < 0) {
			error = EIO;
			goto out;
		}

		offset = 0;
		while (offset + QEMU_DIRENTRY_SZ <= count) {

			/*
			 * Read and make sense out of the buffer in one dirent
			 * This is part of 9p protocol read. This reads one p9_dirent,
			 * appends it to dirent(FREEBSD specifc) and continues to parse the buffer.
			 */
			bzero(&dent, sizeof(dent));
			offset = p9_dirent_read(clnt, io_buffer, offset, count,
				&dent);
			if (offset < 0 || offset > count) {
				error = EIO;
				goto out;
			}

			bzero(&cde, sizeof(cde));
			strncpy(cde.d_name, dent.d_name, dent.len);
			cde.d_fileno = dent.qid.path;
			cde.d_type = dent.d_type;
			cde.d_namlen = dent.len;
			cde.d_reclen = GENERIC_DIRSIZ(&cde);

                        /*
                         * If there isn't enough space in the uio to return a
                         * whole dirent, break off read
                         */
                        if (uio->uio_resid < GENERIC_DIRSIZ(&cde))
                                break;

			/* Transfer */
			error = uiomove(&cde, GENERIC_DIRSIZ(&cde), uio);
			if (error != 0) {
				error = EIO;
				goto out;
			}
			diroffset = dent.d_off;
		}
	}
	/* Pass on last transferred offset */
	uio->uio_offset = diroffset;

out:
	uma_zfree(p9fs_io_buffer_zone, io_buffer);

	return (error);
}

static void
p9fs_doio(struct vnode *vp, struct buf *bp, struct p9_fid *vofid, struct ucred *cr)
{
	struct uio *uiov;
	struct iovec io;
	int error;
	uint64_t off, offset;
	uint64_t filesize;
	uint64_t resid;
	uint32_t count;
	int64_t ret;
	struct p9fs_node *np;
	char *io_buffer;

	error = 0;
	np = P9FS_VTON(vp);

	filesize = np->inode.i_size;
	uiov = malloc(sizeof(struct uio), M_P9UIOV, M_WAITOK);
	uiov->uio_iov = &io;
	uiov->uio_iovcnt = 1;
	uiov->uio_segflg = UIO_SYSSPACE;
	io_buffer = uma_zalloc(p9fs_io_buffer_zone, M_WAITOK | M_ZERO);

	if (bp->b_iocmd == BIO_READ) {
		io.iov_len = uiov->uio_resid = bp->b_bcount;
		io.iov_base = bp->b_data;
		uiov->uio_rw = UIO_READ;

		switch (vp->v_type) {

		case VREG:
		{
			uiov->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE;

			if (uiov->uio_resid) {
				int left = uiov->uio_resid;
				int nread = bp->b_bcount - left;

				if (left > 0)
					bzero((char *)bp->b_data + nread, left);
			}
			/* where in the file are we to start reading */
			offset = uiov->uio_offset;
			if (uiov->uio_offset >= filesize)
				goto out;

			while ((resid = uiov->uio_resid) > 0) {
				if (offset >= filesize)
					break;
				count = min(filesize - uiov->uio_offset, resid);
				if (count == 0)
					break;

				P9_DEBUG(VOPS, "%s: read called %#zx at %#jx\n",
				    __func__, uiov->uio_resid, (uintmax_t)uiov->uio_offset);

				/* Copy count bytes into the uio */
				ret = p9_client_read(vofid, offset, count, io_buffer);
				error = uiomove(io_buffer, ret, uiov);

				if (error != 0)
					goto out;
				offset += ret;
			}
			break;
		}
		default:
			printf("vfs:  type %x unexpected\n", vp->v_type);
			break;
		}
	} else {
		if (bp->b_dirtyend > bp->b_dirtyoff) {
			io.iov_len = uiov->uio_resid = bp->b_dirtyend - bp->b_dirtyoff;
			uiov->uio_offset = ((off_t)bp->b_blkno) * PAGE_SIZE + bp->b_dirtyoff;
			io.iov_base = (char *)bp->b_data + bp->b_dirtyoff;
			uiov->uio_rw = UIO_WRITE;

			if (uiov->uio_offset < 0) {
				error = EINVAL;
				goto out;
			}

			if (uiov->uio_resid == 0)
				goto out;

			resid = uiov->uio_resid;
			offset = uiov->uio_offset;
			error = 0;

			while ((resid = uiov->uio_resid) > 0) {
                                off = 0;
				count = MIN(resid, P9FS_IOUNIT);
				error = uiomove(io_buffer, count, uiov);
				if (error != 0) {
					goto out;
				}

				while (count > 0) {
					/* Copy count bytes from the uio */
					ret = p9_client_write(vofid, offset, count,
                                                io_buffer + off);
					if (ret < 0)
						goto out;

					P9_DEBUG(VOPS, "%s: write called %#zx at %#jx\n",
					    __func__, uiov->uio_resid, (uintmax_t)uiov->uio_offset);
                                        off += ret;
					offset += ret;
					count -= ret;
				}
			}

			/* Update the fields in the node to reflect the change */
			if (filesize < uiov->uio_offset + uiov->uio_resid) {
				np->inode.i_size = uiov->uio_offset + uiov->uio_resid;
				vnode_pager_setsize(vp, uiov->uio_offset + uiov->uio_resid);
				/* update the modified timers. */
				p9fs_itimes(vp);
			}
		} else {
			 bp->b_resid = 0;
			 goto out1;
		}
	}
out:
	/* Set the error */
	if (error != 0) {
		bp->b_error = error;
		bp->b_ioflags |= BIO_ERROR;
	}
	bp->b_resid = uiov->uio_resid;
out1:
	bufdone(bp);
	uma_zfree(p9fs_io_buffer_zone, io_buffer);
	free(uiov, M_P9UIOV);
}

/*
 * The I/O buffer is mapped to a uio and a client_write/client_read is performed
 * the same way as p9fs_read and p9fs_write.
 */
static int
p9fs_strategy(struct vop_strategy_args *ap)
{
	struct vnode *vp;
	struct buf *bp;
	struct ucred *cr;
	int error;
	struct open_fid_state ostate;

	vp = ap->a_vp;
	bp = ap->a_bp;
	error = 0;

	P9_DEBUG(VOPS, "%s: vp %p, iocmd %d\n ", __func__, vp, bp->b_iocmd);

	if (bp->b_iocmd == BIO_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;

	error = p9fs_get_open_fid(vp, bp->b_iocmd == BIO_READ ? FREAD : FWRITE, cr, &ostate);
	if (error) {
		P9_DEBUG(ERROR, "%s: p9fs_get_open_fid failed: %d\n", __func__, error);
		bp->b_error = error;
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		return (0);
	}

	p9fs_doio(vp, bp, ostate.vofid, cr);
	p9fs_release_open_fid(vp, cr, &ostate);

	return (0);
}

/* Rename a file */
static int
p9fs_rename(struct vop_rename_args *ap)
{
	struct vnode *tvp;
	struct vnode *tdvp;
	struct vnode *fvp;
	struct vnode *fdvp;
	struct componentname *tcnp;
	struct componentname *fcnp;
	struct p9fs_node *tdnode;
	struct p9fs_node *fdnode;
	struct p9fs_inode *fdinode;
	struct p9fs_node *fnode;
	struct p9fs_inode *finode;
	struct p9fs_session *vses;
	struct p9fs_node *tnode;
	struct p9fs_inode *tinode;
	struct p9_fid *olddirvfid, *newdirvfid ;
	int error;

	tvp = ap->a_tvp;
	tdvp = ap->a_tdvp;
	fvp = ap->a_fvp;
	fdvp = ap->a_fdvp;
	tcnp = ap->a_tcnp;
	fcnp = ap->a_fcnp;
	tdnode = P9FS_VTON(tdvp);
	fdnode = P9FS_VTON(fdvp);
	fdinode = &fdnode->inode;
	fnode = P9FS_VTON(fvp);
	finode = &fnode->inode;
	vses = fnode->p9fs_ses;
	error = 0;

	P9_DEBUG(VOPS, "%s: tvp %p, tdvp %p, fvp %p, fdvp %p\n ", __func__, tvp, tdvp, fvp, fdvp);

	/* Check for cross mount operation */
	if (fvp->v_mount != tdvp->v_mount ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	/* warning  if you are renaming to the same name */
	if (fvp == tvp)
		error = 0;

	olddirvfid = p9fs_get_fid(vses->clnt, fdnode, fcnp->cn_cred, VFID, -1, &error);
	if (error != 0)
		goto out;
	newdirvfid = p9fs_get_fid(vses->clnt, tdnode, tcnp->cn_cred, VFID, -1, &error);
	if (error != 0)
		goto out;

	error = p9_client_renameat(olddirvfid, fcnp->cn_nameptr, newdirvfid, tcnp->cn_nameptr);
	if (error != 0)
		goto out;

	/*
	 * decrement the link count on the "from" file whose name is going
	 * to be changed if its a directory
	 */
	if (fvp->v_type == VDIR) {
		if (tvp && tvp->v_type == VDIR)
			cache_purge(tdvp);
		P9FS_DECR_LINKS(fdinode);
		cache_purge(fdvp);
	}

	/* Taking exclusive lock on the from node before decrementing the link count */
	if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
		goto out;
	P9FS_DECR_LINKS(finode);
	VOP_UNLOCK(fvp);

	if (tvp) {
		tnode = P9FS_VTON(tvp);
		tinode = &tnode->inode;
		P9FS_DECR_LINKS(tinode);
	}

out:
	if (tdvp == tvp)
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
 * Put VM pages, synchronously.
 * XXX: like smbfs, cannot use vop_stdputpages due to mapping requirement
 */
static int
p9fs_putpages(struct vop_putpages_args *ap)
{
	struct uio uio;
	struct iovec iov;
	int i, error, npages, count;
	off_t offset;
	int *rtvals;
	struct vnode *vp;
	struct thread *td;
	struct ucred *cred;
	struct p9fs_node *np;
	vm_page_t *pages;
	vm_offset_t kva;
	struct buf *bp;

	vp = ap->a_vp;
	np = P9FS_VTON(vp);
	td = curthread;
	cred = curthread->td_ucred;
	pages = ap->a_m;
	count = ap->a_count;
	rtvals = ap->a_rtvals;
	npages = btoc(count);
	offset = IDX_TO_OFF(pages[0]->pindex);

	/*
	 * When putting pages, do not extend file past EOF.
	 */
	if (offset + count > np->inode.i_size) {
		count = np->inode.i_size - offset;
		if (count < 0)
			count = 0;
	}

	for (i = 0; i < npages; i++)
		rtvals[i] = VM_PAGER_ERROR;

	bp = uma_zalloc(p9fs_pbuf_zone, M_WAITOK);
	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);

	VM_CNT_INC(v_vnodeout);
	VM_CNT_ADD(v_vnodepgsout, count);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;

	P9_DEBUG(VOPS, "of=%jd,resid=%zd\n", (intmax_t)uio.uio_offset, uio.uio_resid);

	error = VOP_WRITE(vp, &uio, vnode_pager_putpages_ioflags(ap->a_sync),
	    cred);

	pmap_qremove(kva, npages);
	uma_zfree(p9fs_pbuf_zone, bp);

	if (error == 0)
		vnode_pager_undirty_pages(pages, rtvals, count - uio.uio_resid,
		    np->inode.i_size - offset, npages * PAGE_SIZE);

	return (rtvals[0]);
}

struct vop_vector p9fs_vnops = {
	.vop_default =		&default_vnodeops,
	.vop_lookup =		p9fs_lookup,
	.vop_open =		p9fs_open,
	.vop_close =		p9fs_close,
	.vop_access =		p9fs_access,
	.vop_getattr =		p9fs_getattr_dotl,
	.vop_setattr =		p9fs_setattr_dotl,
	.vop_reclaim =		p9fs_reclaim,
	.vop_inactive =		p9fs_inactive,
	.vop_readdir =		p9fs_readdir,
	.vop_create =		p9fs_create,
	.vop_mknod =		p9fs_mknod,
	.vop_read =		p9fs_read,
	.vop_write =		p9fs_write,
	.vop_remove =		p9fs_remove,
	.vop_mkdir =		p9fs_mkdir,
	.vop_rmdir =		p9fs_rmdir,
	.vop_strategy =		p9fs_strategy,
	.vop_symlink =		p9fs_symlink,
	.vop_rename =           p9fs_rename,
	.vop_link =		p9fs_link,
	.vop_readlink =		p9fs_readlink,
	.vop_putpages =		p9fs_putpages,
};
VFS_VOP_VECTOR_REGISTER(p9fs_vnops);
