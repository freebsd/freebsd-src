/*-
 * Copyright (c) 2017-2020 Juniper Networks, Inc.
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

/*
 * This file consists of all the VFS interactions of VFS ops which include
 * mount, unmount, initilaize etc. for p9fs.
 */

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/fnv_hash.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <vm/uma.h>

#include <fs/p9fs/p9fs_proto.h>
#include <fs/p9fs/p9_client.h>
#include <fs/p9fs/p9_debug.h>
#include <fs/p9fs/p9fs.h>

SYSCTL_NODE(_vfs, OID_AUTO, p9fs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Plan 9 filesystem");

/* This count is static now. Can be made tunable later */
#define P9FS_FLUSH_RETRIES 10

static MALLOC_DEFINE(M_P9MNT, "p9fs_mount", "Mount structures for p9fs");
static uma_zone_t p9fs_node_zone;
uma_zone_t p9fs_io_buffer_zone;
uma_zone_t p9fs_getattr_zone;
uma_zone_t p9fs_setattr_zone;
uma_zone_t p9fs_pbuf_zone;
extern struct vop_vector p9fs_vnops;

/* option parsing */
static const char *p9fs_opts[] = {
        "from", "trans", "access", NULL
};

/* Dispose p9fs node, freeing it to the UMA zone */
void
p9fs_dispose_node(struct p9fs_node **npp)
{
	struct p9fs_node *node;
	struct vnode *vp;

	node = *npp;

	if (node == NULL)
		return;

	if (node->parent && node->parent != node) {
		vrele(P9FS_NTOV(node->parent));
	}

	P9_DEBUG(VOPS, "%s: node: %p\n", __func__, *npp);

	vp = P9FS_NTOV(node);
	vp->v_data = NULL;

	/* Free our associated memory */
	if (!(vp->v_vflag & VV_ROOT)) {
		free(node->inode.i_name, M_TEMP);
		uma_zfree(p9fs_node_zone, node);
	}

	*npp = NULL;
}

/* Initialize memory allocation */
static int
p9fs_init(struct vfsconf *vfsp)
{

	p9fs_node_zone = uma_zcreate("p9fs node zone",
	    sizeof(struct p9fs_node), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Create the getattr_dotl zone */
	p9fs_getattr_zone = uma_zcreate("p9fs getattr zone",
	    sizeof(struct p9_stat_dotl), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Create the setattr_dotl zone */
	p9fs_setattr_zone = uma_zcreate("p9fs setattr zone",
	    sizeof(struct p9_iattr_dotl), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Create the putpages zone */
	p9fs_pbuf_zone = pbuf_zsecond_create("p9fs pbuf zone", nswbuf / 2);

	/*
	 * Create the io_buffer zone pool to keep things simpler in case of
	 * multiple threads. Each thread works with its own so there is no
	 * contention.
	 */
	p9fs_io_buffer_zone = uma_zcreate("p9fs io_buffer zone",
	    P9FS_MTU, NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	return (0);
}

/* Destroy all the allocated memory */
static int
p9fs_uninit(struct vfsconf *vfsp)
{

	uma_zdestroy(p9fs_node_zone);
	uma_zdestroy(p9fs_io_buffer_zone);
	uma_zdestroy(p9fs_getattr_zone);
	uma_zdestroy(p9fs_setattr_zone);
	uma_zdestroy(p9fs_pbuf_zone);

	return (0);
}

/* Function to umount p9fs */
static int
p9fs_unmount(struct mount *mp, int mntflags)
{
	struct p9fs_mount *vmp;
	struct p9fs_session *vses;
	int error, flags, i;

	error = 0;
	flags = 0;
	vmp = VFSTOP9(mp);
	if (vmp == NULL)
		return (0);

	vses = &vmp->p9fs_session;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	p9fs_prepare_to_close(mp);
	for (i = 0; i < P9FS_FLUSH_RETRIES; i++) {

		/* Flush everything on this mount point.*/
		error = vflush(mp, 1, flags, curthread);

		if (error == 0 || (mntflags & MNT_FORCE) == 0)
			break;
		/* Sleep until interrupted or 1 tick expires. */
		error = tsleep(&error, PSOCK, "p9unmnt", 1);
		if (error == EINTR)
			break;
		error = EBUSY;
	}

	if (error != 0)
		goto out;
	p9fs_close_session(mp);
	/* Cleanup the mount structure. */
	free(vmp, M_P9MNT);
	mp->mnt_data = NULL;
	return (error);
out:
	/* Restore the flag in case of error */
	vses->clnt->trans_status = P9FS_CONNECT;
	return (error);
}

/*
 * Compare qid stored in p9fs node
 * Return 1 if does not match otherwise return 0
 */
int
p9fs_node_cmp(struct vnode *vp, void *arg)
{
	struct p9fs_node *np;
	struct p9_qid *qid;

	np = vp->v_data;
	qid = (struct p9_qid *)arg;

	if (np == NULL)
		return (1);

	if (np->vqid.qid_path == qid->path) {
		if (vp->v_vflag & VV_ROOT)
			return (0);
		else if (np->vqid.qid_mode == qid->type &&
			    np->vqid.qid_version == qid->version)
			return (0);
	}

	return (1);
}

/*
 * Cleanup p9fs node
 *  - Destroy the FID LIST locks
 *  - Dispose all node knowledge
 */
void
p9fs_destroy_node(struct p9fs_node **npp)
{
	struct p9fs_node *np;

	np = *npp;

	if (np == NULL)
		return;

	/* Destroy the FID LIST locks */
	P9FS_VFID_LOCK_DESTROY(np);
	P9FS_VOFID_LOCK_DESTROY(np);

	/* Dispose all node knowledge.*/
	p9fs_dispose_node(&np);
}

/*
 * Common code used across p9fs to return vnode for the file represented
 * by the fid.
 * Lookup for the vnode in hash_list. This lookup is based on the qid path
 * which is unique to a file. p9fs_node_cmp is called in this lookup process.
 * I. If the vnode we are looking for is found in the hash list
 *    1. Check if the vnode is a valid vnode by reloading its stats
 *       a. if the reloading of the vnode stats returns error then remove the
 *          vnode from hash list and return
 *       b. If reloading of vnode stats returns without any error then, clunk the
 *          new fid which was created for the vnode as we know that the vnode
 *          already has a fid associated with it and return the vnode.
 *          This is to avoid fid leaks
 * II. If vnode is not found in the hash list then, create new vnode, p9fs
 *     node and return the vnode
 */
int
p9fs_vget_common(struct mount *mp, struct p9fs_node *np, int flags,
    struct p9fs_node *parent, struct p9_fid *fid, struct vnode **vpp,
    char *name)
{
	struct p9fs_mount *vmp;
	struct p9fs_session *vses;
	struct vnode *vp;
	struct p9fs_node *node;
	struct thread *td;
	uint32_t hash;
	int error, error_reload = 0;
	struct p9fs_inode *inode;

	td = curthread;
	vmp = VFSTOP9(mp);
	vses = &vmp->p9fs_session;

	/* Look for vp in the hash_list */
	hash = fnv_32_buf(&fid->qid.path, sizeof(uint64_t), FNV1_32_INIT);
	error = vfs_hash_get(mp, hash, flags, td, &vp, p9fs_node_cmp,
	    &fid->qid);
	if (error != 0)
		return (error);
	else if (vp != NULL) {
		if (vp->v_vflag & VV_ROOT) {
			if (np == NULL)
				p9_client_clunk(fid);
			*vpp = vp;
			return (0);
		}
		error = p9fs_reload_stats_dotl(vp, curthread->td_ucred);
		if (error != 0) {
			node = vp->v_data;
			/* Remove stale vnode from hash list */
			vfs_hash_remove(vp);
			node->flags |= P9FS_NODE_DELETED;

			vput(vp);
			*vpp = NULLVP;
			vp = NULL;
		} else {
			*vpp = vp;
			/* Clunk the new fid if not root */
			p9_client_clunk(fid);
			return (0);
		}
	}

	/*
	 * We must promote to an exclusive lock for vnode creation.  This
	 * can happen if lookup is passed LOCKSHARED.
	 */
	if ((flags & LK_TYPE_MASK) == LK_SHARED) {
		flags &= ~LK_TYPE_MASK;
		flags |= LK_EXCLUSIVE;
	}

	/* Allocate a new vnode. */
	if ((error = getnewvnode("p9fs", mp, &p9fs_vnops, &vp)) != 0) {
		*vpp = NULLVP;
		P9_DEBUG(ERROR, "%s: getnewvnode failed: %d\n", __func__, error);
		return (error);
	}

	/* If we dont have it, create one. */
	if (np == NULL) {
		np =  uma_zalloc(p9fs_node_zone, M_WAITOK | M_ZERO);
		/* Initialize the VFID list */
		P9FS_VFID_LOCK_INIT(np);
		STAILQ_INIT(&np->vfid_list);
		p9fs_fid_add(np, fid, VFID);

		/* Initialize the VOFID list */
		P9FS_VOFID_LOCK_INIT(np);
		STAILQ_INIT(&np->vofid_list);

		vref(P9FS_NTOV(parent));
		np->parent = parent;
		np->p9fs_ses = vses; /* Map the current session */
		inode = &np->inode;
		/*Fill the name of the file in inode */
		inode->i_name = malloc(strlen(name)+1, M_TEMP, M_NOWAIT | M_ZERO);
		strlcpy(inode->i_name, name, strlen(name)+1);
	} else {
		vp->v_type = VDIR; /* root vp is a directory */
		vp->v_vflag |= VV_ROOT;
		vref(vp); /* Increment a reference on root vnode during mount */
	}

	vp->v_data = np;
	np->v_node = vp;
	inode = &np->inode;
	inode->i_qid_path = fid->qid.path;
	P9FS_SET_LINKS(inode);

	lockmgr(vp->v_vnlock, LK_EXCLUSIVE, NULL);
	if (vp->v_type != VFIFO)
		VN_LOCK_ASHARE(vp);
	error = insmntque(vp, mp);
	if (error != 0) {
		/*
		 * vput(vp) is already called from insmntque_stddtr().
		 * Just goto 'out' to dispose the node.
		 */
		goto out;
	}

	/* Init the vnode with the disk info*/
	error = p9fs_reload_stats_dotl(vp, curthread->td_ucred);
	if (error != 0) {
		error_reload = 1;
		goto out;
	}

	error = vfs_hash_insert(vp, hash, flags, td, vpp,
	    p9fs_node_cmp, &fid->qid);
	if (error != 0) {
		goto out;
	}

	if (*vpp == NULL) {
		P9FS_LOCK(vses);
		STAILQ_INSERT_TAIL(&vses->virt_node_list, np, p9fs_node_next);
		np->flags |= P9FS_NODE_IN_SESSION;
		P9FS_UNLOCK(vses);
		vn_set_state(vp, VSTATE_CONSTRUCTED);
		*vpp = vp;
	} else {
		/*
		 * Returning matching vp found in hashlist.
		 * So cleanup the np allocated above in this context.
		 */
		if (!IS_ROOT(np)) {
			p9fs_destroy_node(&np);
		}
	}

	return (0);
out:
	/* Something went wrong, dispose the node */
	if (!IS_ROOT(np)) {
		p9fs_destroy_node(&np);
	}

	if (error_reload) {
		vput(vp);
	}

	*vpp = NULLVP;
	return (error);
}

/* Main mount function for 9pfs */
static int
p9_mount(struct mount *mp)
{
	struct p9_fid *fid;
	struct p9fs_mount *vmp;
	struct p9fs_session *vses;
	struct p9fs_node *p9fs_root;
	int error;
	char *from;
	int len;

	/* Verify the validity of mount options */
	if (vfs_filteropt(mp->mnt_optnew, p9fs_opts))
		return (EINVAL);

	/* Extract NULL terminated mount tag from mount options */
	error = vfs_getopt(mp->mnt_optnew, "from", (void **)&from, &len);
	if (error != 0 || from[len - 1] != '\0')
		return (EINVAL);

	/* Allocate and initialize the private mount structure. */
	vmp = malloc(sizeof (struct p9fs_mount), M_P9MNT, M_WAITOK | M_ZERO);
	mp->mnt_data = vmp;
	vmp->p9fs_mountp = mp;
	vmp->mount_tag = from;
	vmp->mount_tag_len = len;
	vses = &vmp->p9fs_session;
	vses->p9fs_mount = mp;
	p9fs_root = &vses->rnp;
	/* Hardware iosize from the Qemu */
	mp->mnt_iosize_max = PAGE_SIZE;
	/*
	 * Init the session for the p9fs root. This creates a new root fid and
	 * attaches the client and server.
	 */
	fid = p9fs_init_session(mp, &error);
	if (fid == NULL) {
		goto out;
	}

	P9FS_VFID_LOCK_INIT(p9fs_root);
	STAILQ_INIT(&p9fs_root->vfid_list);
	p9fs_fid_add(p9fs_root, fid, VFID);
	P9FS_VOFID_LOCK_INIT(p9fs_root);
	STAILQ_INIT(&p9fs_root->vofid_list);
	p9fs_root->parent = p9fs_root;
	p9fs_root->flags |= P9FS_ROOT;
	p9fs_root->p9fs_ses = vses;
	vfs_getnewfsid(mp);
	strlcpy(mp->mnt_stat.f_mntfromname, from,
	    sizeof(mp->mnt_stat.f_mntfromname));
	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_LOOKUP_SHARED | MNTK_EXTENDED_SHARED;
	MNT_IUNLOCK(mp);
	P9_DEBUG(VOPS, "%s: Mount successful\n", __func__);
	/* Mount structures created. */

	return (0);
out:
	P9_DEBUG(ERROR, "%s: Mount Failed \n", __func__);
	if (vmp != NULL) {
		free(vmp, M_P9MNT);
		mp->mnt_data = NULL;
	}
	return (error);
}

/* Mount entry point */
static int
p9fs_mount(struct mount *mp)
{
	int error;

	/*
	 * Minimal support for MNT_UPDATE - allow changing from
	 * readonly.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		if ((mp->mnt_flag & MNT_RDONLY) && !vfs_flagopt(mp->mnt_optnew, "ro", NULL, 0)) {
			mp->mnt_flag &= ~MNT_RDONLY;
		}
		return (0);
	}

	error = p9_mount(mp);
	if (error != 0)
		(void) p9fs_unmount(mp, MNT_FORCE);

	return (error);
}

/*
 * Retrieve the root vnode of this mount. After filesystem is mounted, the root
 * vnode is created for the first time. Subsequent calls to p9fs root will
 * return the same vnode created during mount.
 */
static int
p9fs_root(struct mount *mp, int lkflags, struct vnode **vpp)
{
	struct p9fs_mount *vmp;
	struct p9fs_node *np;
	struct p9_client *clnt;
	struct p9_fid *vfid;
	int error;

	vmp = VFSTOP9(mp);
	np = &vmp->p9fs_session.rnp;
	clnt = vmp->p9fs_session.clnt;
	error = 0;

	P9_DEBUG(VOPS, "%s: node=%p name=%s\n",__func__, np, np->inode.i_name);

	vfid = p9fs_get_fid(clnt, np, curthread->td_ucred, VFID, -1, &error);

	if (error != 0) {
		/* for root use the nobody user's fid as vfid.
		 * This is used while unmounting as root when non-root
		 * user has mounted p9fs
		 */
		if (vfid == NULL && clnt->trans_status == P9FS_BEGIN_DISCONNECT)
			vfid = vmp->p9fs_session.mnt_fid;
		else {
			*vpp = NULLVP;
			return (error);
		}
	}

	error = p9fs_vget_common(mp, np, lkflags, np, vfid, vpp, NULL);
	if (error != 0) {
		*vpp = NULLVP;
		return (error);
	}
	np->v_node = *vpp;
	return (error);
}

/* Retrieve the file system statistics */
static int
p9fs_statfs(struct mount *mp __unused, struct statfs *buf)
{
	struct p9fs_mount *vmp;
	struct p9fs_node *np;
	struct p9_client *clnt;
	struct p9_fid *vfid;
	struct p9_statfs statfs;
	int res, error;

	vmp = VFSTOP9(mp);
	np = &vmp->p9fs_session.rnp;
	clnt = vmp->p9fs_session.clnt;
	error = 0;

	vfid = p9fs_get_fid(clnt, np, curthread->td_ucred, VFID, -1, &error);
	if (error != 0) {
		return (error);
	}

	res = p9_client_statfs(vfid, &statfs);

	if (res == 0) {
		buf->f_type = statfs.type;
		/*
		 * We have a limit of 4k irrespective of what the
		 * Qemu server can do.
		 */
		if (statfs.bsize > PAGE_SIZE)
			buf->f_bsize = PAGE_SIZE;
		else
			buf->f_bsize = statfs.bsize;

		buf->f_iosize = buf->f_bsize;
		buf->f_blocks = statfs.blocks;
		buf->f_bfree = statfs.bfree;
		buf->f_bavail = statfs.bavail;
		buf->f_files = statfs.files;
		buf->f_ffree = statfs.ffree;
	}
	else {
		/* Atleast set these if stat fail */
		buf->f_bsize = PAGE_SIZE;
		buf->f_iosize = buf->f_bsize;   /* XXX */
	}

	return (0);
}

static int
p9fs_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{

	return (EINVAL);
}

struct vfsops p9fs_vfsops = {
	.vfs_init  =	p9fs_init,
	.vfs_uninit =	p9fs_uninit,
	.vfs_mount =	p9fs_mount,
	.vfs_unmount =	p9fs_unmount,
	.vfs_root =	p9fs_root,
	.vfs_statfs =	p9fs_statfs,
	.vfs_fhtovp =	p9fs_fhtovp,
};

VFS_SET(p9fs_vfsops, p9fs, VFCF_JAIL);
MODULE_VERSION(p9fs, 1);
