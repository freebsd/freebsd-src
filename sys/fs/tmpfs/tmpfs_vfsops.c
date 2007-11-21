/*	$NetBSD: tmpfs_vfsops.c,v 1.10 2005/12/11 12:24:29 christos Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Efficient memory file system.
 *
 * tmpfs is a file system that uses NetBSD's virtual memory sub-system
 * (the well-known UVM) to store file data and metadata in an efficient
 * way.  This means that it does not follow the structure of an on-disk
 * file system because it simply does not need to.  Instead, it uses
 * memory-specific data structures and algorithms to automatically
 * allocate and release resources.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_param.h>

#include <fs/tmpfs/tmpfs.h>

/*
 * Default permission for root node
 */
#define TMPFS_DEFAULT_ROOT_MODE	(S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

MALLOC_DEFINE(M_TMPFSMNT, "tmpfs mount", "tmpfs mount structures");
MALLOC_DEFINE(M_TMPFSNAME, "tmpfs name", "tmpfs file names");

/* --------------------------------------------------------------------- */

static int	tmpfs_mount(struct mount *, struct thread *);
static int	tmpfs_unmount(struct mount *, int, struct thread *);
static int	tmpfs_root(struct mount *, int flags, struct vnode **,
		    struct thread *);
static int	tmpfs_fhtovp(struct mount *, struct fid *, struct vnode **);
static int	tmpfs_statfs(struct mount *, struct statfs *, struct thread *);

/* --------------------------------------------------------------------- */

static const char *tmpfs_opts[] = {
	"from", "size", "inodes", "uid", "gid", "mode", "export",
	NULL
};

/* --------------------------------------------------------------------- */

#define SWI_MAXMIB	3

static u_int
get_swpgtotal(void)
{
	struct xswdev xsd;
	char *sname = "vm.swap_info";
	int soid[SWI_MAXMIB], oid[2];
	u_int unswdev, total, dmmax, nswapdev;
	size_t mibi, len;

	total = 0;

	len = sizeof(dmmax);
	if (kernel_sysctlbyname(curthread, "vm.dmmax", &dmmax, &len,
				NULL, 0, NULL, 0) != 0)
		return total;

	len = sizeof(nswapdev);
	if (kernel_sysctlbyname(curthread, "vm.nswapdev",
				&nswapdev, &len,
				NULL, 0, NULL, 0) != 0)
		return total;

	mibi = (SWI_MAXMIB - 1) * sizeof(int);
	oid[0] = 0;
	oid[1] = 3;

	if (kernel_sysctl(curthread, oid, 2,
			soid, &mibi, (void *)sname, strlen(sname),
			NULL, 0) != 0)
		return total;

	mibi = (SWI_MAXMIB - 1);
	for (unswdev = 0; unswdev < nswapdev; ++unswdev) {
		soid[mibi] = unswdev;
		len = sizeof(struct xswdev);
		if (kernel_sysctl(curthread,
				soid, mibi + 1, &xsd, &len, NULL, 0,
				NULL, 0) != 0)
			return total;
		if (len == sizeof(struct xswdev))
			total += (xsd.xsw_nblks - dmmax);
	}

	/* Not Reached */
	return total;
}

/* --------------------------------------------------------------------- */
static int
tmpfs_node_ctor(void *mem, int size, void *arg, int flags)
{
	struct tmpfs_node *node = (struct tmpfs_node *)mem;

	node->tn_gen++;
	node->tn_size = 0;
	node->tn_status = 0;
	node->tn_flags = 0;
	node->tn_links = 0;
	node->tn_lockf = NULL;
	node->tn_vnode = NULL;
	node->tn_vpstate = 0;
	node->tn_lookup_dirent = NULL;

	return (0);
}

static void
tmpfs_node_dtor(void *mem, int size, void *arg)
{
	struct tmpfs_node *node = (struct tmpfs_node *)mem;
	node->tn_type = VNON;
}

static int
tmpfs_node_init(void *mem, int size, int flags)
{
	struct tmpfs_node *node = (struct tmpfs_node *)mem;
	node->tn_id = 0;

	mtx_init(&node->tn_interlock, "tmpfs node interlock", NULL, MTX_DEF);
	node->tn_gen = arc4random();

	return (0);
}

static void
tmpfs_node_fini(void *mem, int size)
{
	struct tmpfs_node *node = (struct tmpfs_node *)mem;

	mtx_destroy(&node->tn_interlock);
}

static int
tmpfs_mount(struct mount *mp, struct thread *td)
{
	struct tmpfs_mount *tmp;
	struct tmpfs_node *root;
	size_t pages, mem_size;
	ino_t nodes;
	int error;
	/* Size counters. */
	ino_t	nodes_max;
	off_t	size_max;

	/* Root node attributes. */
	uid_t	root_uid;
	gid_t	root_gid;
	mode_t	root_mode;

	struct vattr	va;

	if (vfs_filteropt(mp->mnt_optnew, tmpfs_opts))
		return (EINVAL);

	if (mp->mnt_flag & MNT_UPDATE) {
		/* XXX: There is no support yet to update file system
		 * settings.  Should be added. */

		return EOPNOTSUPP;
	}

	printf("WARNING: TMPFS is considered to be a highly experimental "
		"feature in FreeBSD.\n");

	vn_lock(mp->mnt_vnodecovered, LK_SHARED | LK_RETRY, td);
	error = VOP_GETATTR(mp->mnt_vnodecovered, &va, mp->mnt_cred, td);
	VOP_UNLOCK(mp->mnt_vnodecovered, 0, td);
	if (error)
		return (error);

	if (mp->mnt_cred->cr_ruid != 0 ||
	    vfs_scanopt(mp->mnt_optnew, "gid", "%d", &root_gid) != 1)
		root_gid = va.va_gid;
	if (mp->mnt_cred->cr_ruid != 0 ||
	    vfs_scanopt(mp->mnt_optnew, "uid", "%d", &root_uid) != 1)
		root_uid = va.va_uid;
	if (mp->mnt_cred->cr_ruid != 0 ||
	    vfs_scanopt(mp->mnt_optnew, "mode", "%ho", &root_mode) != 1)
		root_mode = va.va_mode;
	if(vfs_scanopt(mp->mnt_optnew, "inodes", "%d", &nodes_max) != 1)
		nodes_max = 0;

	if(vfs_scanopt(mp->mnt_optnew,
			"size",
			"%qu", &size_max) != 1)
		size_max = 0;

	/* Do not allow mounts if we do not have enough memory to preserve
	 * the minimum reserved pages. */
	mem_size = cnt.v_free_count + cnt.v_inactive_count + get_swpgtotal();
	mem_size -= mem_size > cnt.v_wire_count ? cnt.v_wire_count : mem_size;
	if (mem_size < TMPFS_PAGES_RESERVED)
		return ENOSPC;

	/* Get the maximum number of memory pages this file system is
	 * allowed to use, based on the maximum size the user passed in
	 * the mount structure.  A value of zero is treated as if the
	 * maximum available space was requested. */
	if (size_max < PAGE_SIZE || size_max >= SIZE_MAX)
		pages = SIZE_MAX;
	else
		pages = howmany(size_max, PAGE_SIZE);
	MPASS(pages > 0);

	if (nodes_max <= 3)
		nodes = 3 + pages * PAGE_SIZE / 1024;
	else
		nodes = nodes_max;
	MPASS(nodes >= 3);

	/* Allocate the tmpfs mount structure and fill it. */
	tmp = (struct tmpfs_mount *)malloc(sizeof(struct tmpfs_mount),
	    M_TMPFSMNT, M_WAITOK | M_ZERO);

	mtx_init(&tmp->allnode_lock, "tmpfs allnode lock", NULL, MTX_DEF);
	tmp->tm_nodes_max = nodes;
	tmp->tm_nodes_inuse = 0;
	tmp->tm_maxfilesize = (u_int64_t)(cnt.v_page_count + get_swpgtotal()) * PAGE_SIZE;
	LIST_INIT(&tmp->tm_nodes_used);

	tmp->tm_pages_max = pages;
	tmp->tm_pages_used = 0;
	tmp->tm_ino_unr = new_unrhdr(2, INT_MAX, &tmp->allnode_lock);
	tmp->tm_dirent_pool = uma_zcreate(
					"TMPFS dirent",
					sizeof(struct tmpfs_dirent),
					NULL, NULL, NULL, NULL,
					UMA_ALIGN_PTR,
					0);
	tmp->tm_node_pool = uma_zcreate(
					"TMPFS node",
					sizeof(struct tmpfs_node),
					tmpfs_node_ctor, tmpfs_node_dtor,
					tmpfs_node_init, tmpfs_node_fini,
					UMA_ALIGN_PTR,
					0);

	/* Allocate the root node. */
	error = tmpfs_alloc_node(tmp, VDIR, root_uid,
	    root_gid, root_mode & ALLPERMS, NULL, NULL,
	    VNOVAL, td, &root);

	if (error != 0 || root == NULL) {
	    uma_zdestroy(tmp->tm_node_pool);
	    uma_zdestroy(tmp->tm_dirent_pool);
	    delete_unrhdr(tmp->tm_ino_unr);
	    free(tmp, M_TMPFSMNT);
	    return error;
	}
	KASSERT(root->tn_id == 2, ("tmpfs root with invalid ino: %d", root->tn_id));
	tmp->tm_root = root;

	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_MPSAFE;
	MNT_IUNLOCK(mp);

	mp->mnt_data = tmp;
	mp->mnt_stat.f_namemax = MAXNAMLEN;
	vfs_getnewfsid(mp);
	vfs_mountedfrom(mp, "tmpfs");

	return 0;
}

/* --------------------------------------------------------------------- */

/* ARGSUSED2 */
static int
tmpfs_unmount(struct mount *mp, int mntflags, struct thread *l)
{
	int error;
	int flags = 0;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;

	/* Handle forced unmounts. */
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* Finalize all pending I/O. */
	error = vflush(mp, 0, flags, l);
	if (error != 0)
		return error;

	tmp = VFS_TO_TMPFS(mp);

	/* Free all associated data.  The loop iterates over the linked list
	 * we have containing all used nodes.  For each of them that is
	 * a directory, we free all its directory entries.  Note that after
	 * freeing a node, it will automatically go to the available list,
	 * so we will later have to iterate over it to release its items. */
	node = LIST_FIRST(&tmp->tm_nodes_used);
	while (node != NULL) {
		struct tmpfs_node *next;

		if (node->tn_type == VDIR) {
			struct tmpfs_dirent *de;

			de = TAILQ_FIRST(&node->tn_dir.tn_dirhead);
			while (de != NULL) {
				struct tmpfs_dirent *nde;

				nde = TAILQ_NEXT(de, td_entries);
				tmpfs_free_dirent(tmp, de, FALSE);
				de = nde;
				node->tn_size -= sizeof(struct tmpfs_dirent);
			}
		}

		next = LIST_NEXT(node, tn_entries);
		tmpfs_free_node(tmp, node);
		node = next;
	}

	uma_zdestroy(tmp->tm_dirent_pool);
	uma_zdestroy(tmp->tm_node_pool);
	delete_unrhdr(tmp->tm_ino_unr);

	mtx_destroy(&tmp->allnode_lock);
	MPASS(tmp->tm_pages_used == 0);
	MPASS(tmp->tm_nodes_inuse == 0);

	/* Throw away the tmpfs_mount structure. */
	free(mp->mnt_data, M_TMPFSMNT);
	mp->mnt_data = NULL;

	MNT_ILOCK(mp);
	mp->mnt_flag &= ~MNT_LOCAL;
	MNT_IUNLOCK(mp);
	return 0;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
{
	int error;
	error = tmpfs_alloc_vp(mp, VFS_TO_TMPFS(mp)->tm_root, flags, vpp, td);

	if (!error)
		(*vpp)->v_vflag |= VV_ROOT;

	return error;
}

/* --------------------------------------------------------------------- */

static int
tmpfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	boolean_t found;
	struct tmpfs_fid *tfhp;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;

	tmp = VFS_TO_TMPFS(mp);

	tfhp = (struct tmpfs_fid *)fhp;
	if (tfhp->tf_len != sizeof(struct tmpfs_fid))
		return EINVAL;

	if (tfhp->tf_id >= tmp->tm_nodes_max)
		return EINVAL;

	found = FALSE;

	TMPFS_LOCK(tmp);
	LIST_FOREACH(node, &tmp->tm_nodes_used, tn_entries) {
		if (node->tn_id == tfhp->tf_id &&
		    node->tn_gen == tfhp->tf_gen) {
			found = TRUE;
			break;
		}
	}
	TMPFS_UNLOCK(tmp);

	if (found)
		return (tmpfs_alloc_vp(mp, node, LK_EXCLUSIVE, vpp, curthread));

	return (EINVAL);
}

/* --------------------------------------------------------------------- */

/* ARGSUSED2 */
static int
tmpfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *l)
{
	fsfilcnt_t freenodes;
	struct tmpfs_mount *tmp;

	tmp = VFS_TO_TMPFS(mp);

	sbp->f_iosize = PAGE_SIZE;
	sbp->f_bsize = PAGE_SIZE;

	sbp->f_blocks = TMPFS_PAGES_MAX(tmp);
	sbp->f_bavail = sbp->f_bfree = TMPFS_PAGES_AVAIL(tmp);

	freenodes = MIN(tmp->tm_nodes_max - tmp->tm_nodes_inuse,
	    TMPFS_PAGES_AVAIL(tmp) * PAGE_SIZE / sizeof(struct tmpfs_node));

	sbp->f_files = freenodes + tmp->tm_nodes_inuse;
	sbp->f_ffree = freenodes;
	/* sbp->f_owner = tmp->tn_uid; */

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * tmpfs vfs operations.
 */

struct vfsops tmpfs_vfsops = {
	.vfs_mount =			tmpfs_mount,
	.vfs_unmount =			tmpfs_unmount,
	.vfs_root =			tmpfs_root,
	.vfs_statfs =			tmpfs_statfs,
	.vfs_fhtovp =			tmpfs_fhtovp,
};
VFS_SET(tmpfs_vfsops, tmpfs, 0);
