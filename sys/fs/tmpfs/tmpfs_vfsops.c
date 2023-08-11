/*	$NetBSD: tmpfs_vfsops.c,v 1.10 2005/12/11 12:24:29 christos Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 * tmpfs is a file system that uses FreeBSD's virtual memory
 * sub-system to store file data and metadata in an efficient way.
 * This means that it does not follow the structure of an on-disk file
 * system because it simply does not need to.  Instead, it uses
 * memory-specific data structures and algorithms to automatically
 * allocate and release resources.
 */

#include "opt_ddb.h"
#include "opt_tmpfs.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_param.h>

#include <fs/tmpfs/tmpfs.h>

/*
 * Default permission for root node
 */
#define TMPFS_DEFAULT_ROOT_MODE	(S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

static MALLOC_DEFINE(M_TMPFSMNT, "tmpfs mount", "tmpfs mount structures");
MALLOC_DEFINE(M_TMPFSNAME, "tmpfs name", "tmpfs file names");

static int	tmpfs_mount(struct mount *);
static int	tmpfs_unmount(struct mount *, int);
static int	tmpfs_root(struct mount *, int flags, struct vnode **);
static int	tmpfs_fhtovp(struct mount *, struct fid *, int,
		    struct vnode **);
static int	tmpfs_statfs(struct mount *, struct statfs *);

static const char *tmpfs_opts[] = {
	"from", "easize", "size", "maxfilesize", "inodes", "uid", "gid", "mode",
	"export", "union", "nonc", "nomtime", "nosymfollow", "pgread", NULL
};

static const char *tmpfs_updateopts[] = {
	"from", "easize", "export", "nomtime", "size", "nosymfollow", NULL
};

static int
tmpfs_update_mtime_lazy_filter(struct vnode *vp, void *arg)
{
	struct vm_object *obj;

	if (vp->v_type != VREG)
		return (0);

	obj = atomic_load_ptr(&vp->v_object);
	if (obj == NULL)
		return (0);

	return (vm_object_mightbedirty_(obj));
}

static void
tmpfs_update_mtime_lazy(struct mount *mp)
{
	struct vnode *vp, *mvp;

	MNT_VNODE_FOREACH_LAZY(vp, mp, mvp, tmpfs_update_mtime_lazy_filter, NULL) {
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK) != 0)
			continue;
		tmpfs_check_mtime(vp);
		vput(vp);
	}
}

static void
tmpfs_update_mtime_all(struct mount *mp)
{
	struct vnode *vp, *mvp;

	if (VFS_TO_TMPFS(mp)->tm_nomtime)
		return;
	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		if (vp->v_type != VREG) {
			VI_UNLOCK(vp);
			continue;
		}
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK) != 0)
			continue;
		tmpfs_check_mtime(vp);
		tmpfs_update(vp);
		vput(vp);
	}
}

struct tmpfs_check_rw_maps_arg {
	bool found;
};

static bool
tmpfs_check_rw_maps_cb(struct mount *mp __unused, vm_map_t map __unused,
    vm_map_entry_t entry __unused, void *arg)
{
	struct tmpfs_check_rw_maps_arg *a;

	a = arg;
	a->found = true;
	return (true);
}

/*
 * Revoke write permissions from all mappings of regular files
 * belonging to the specified tmpfs mount.
 */
static bool
tmpfs_revoke_rw_maps_cb(struct mount *mp __unused, vm_map_t map,
    vm_map_entry_t entry, void *arg __unused)
{

	/*
	 * XXXKIB: might be invalidate the mapping
	 * instead ?  The process is not going to be
	 * happy in any case.
	 */
	entry->max_protection &= ~VM_PROT_WRITE;
	if ((entry->protection & VM_PROT_WRITE) != 0) {
		entry->protection &= ~VM_PROT_WRITE;
		pmap_protect(map->pmap, entry->start, entry->end,
		    entry->protection);
	}
	return (false);
}

static void
tmpfs_all_rw_maps(struct mount *mp, bool (*cb)(struct mount *mp, vm_map_t,
    vm_map_entry_t, void *), void *cb_arg)
{
	struct proc *p;
	struct vmspace *vm;
	vm_map_t map;
	vm_map_entry_t entry;
	vm_object_t object;
	struct vnode *vp;
	int gen;
	bool terminate;

	terminate = false;
	sx_slock(&allproc_lock);
again:
	gen = allproc_gen;
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (p->p_state != PRS_NORMAL || (p->p_flag & (P_INEXEC |
		    P_SYSTEM | P_WEXIT)) != 0) {
			PROC_UNLOCK(p);
			continue;
		}
		vm = vmspace_acquire_ref(p);
		_PHOLD_LITE(p);
		PROC_UNLOCK(p);
		if (vm == NULL) {
			PRELE(p);
			continue;
		}
		sx_sunlock(&allproc_lock);
		map = &vm->vm_map;

		vm_map_lock(map);
		if (map->busy)
			vm_map_wait_busy(map);
		VM_MAP_ENTRY_FOREACH(entry, map) {
			if ((entry->eflags & (MAP_ENTRY_GUARD |
			    MAP_ENTRY_IS_SUB_MAP | MAP_ENTRY_COW)) != 0 ||
			    (entry->max_protection & VM_PROT_WRITE) == 0)
				continue;
			object = entry->object.vm_object;
			if (object == NULL || object->type != tmpfs_pager_type)
				continue;
			/*
			 * No need to dig into shadow chain, mapping
			 * of the object not at top is readonly.
			 */

			VM_OBJECT_RLOCK(object);
			if (object->type == OBJT_DEAD) {
				VM_OBJECT_RUNLOCK(object);
				continue;
			}
			MPASS(object->ref_count > 1);
			if ((object->flags & OBJ_TMPFS) == 0) {
				VM_OBJECT_RUNLOCK(object);
				continue;
			}
			vp = VM_TO_TMPFS_VP(object);
			if (vp->v_mount != mp) {
				VM_OBJECT_RUNLOCK(object);
				continue;
			}

			terminate = cb(mp, map, entry, cb_arg);
			VM_OBJECT_RUNLOCK(object);
			if (terminate)
				break;
		}
		vm_map_unlock(map);

		vmspace_free(vm);
		sx_slock(&allproc_lock);
		PRELE(p);
		if (terminate)
			break;
	}
	if (!terminate && gen != allproc_gen)
		goto again;
	sx_sunlock(&allproc_lock);
}

static bool
tmpfs_check_rw_maps(struct mount *mp)
{
	struct tmpfs_check_rw_maps_arg ca;

	ca.found = false;
	tmpfs_all_rw_maps(mp, tmpfs_check_rw_maps_cb, &ca);
	return (ca.found);
}

static int
tmpfs_rw_to_ro(struct mount *mp)
{
	int error, flags;
	bool forced;

	forced = (mp->mnt_flag & MNT_FORCE) != 0;
	flags = WRITECLOSE | (forced ? FORCECLOSE : 0);

	if ((error = vn_start_write(NULL, &mp, V_WAIT)) != 0)
		return (error);
	error = vfs_write_suspend_umnt(mp);
	if (error != 0)
		return (error);
	if (!forced && tmpfs_check_rw_maps(mp)) {
		error = EBUSY;
		goto out;
	}
	VFS_TO_TMPFS(mp)->tm_ronly = 1;
	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_RDONLY;
	MNT_IUNLOCK(mp);
	for (;;) {
		tmpfs_all_rw_maps(mp, tmpfs_revoke_rw_maps_cb, NULL);
		tmpfs_update_mtime_all(mp);
		error = vflush(mp, 0, flags, curthread);
		if (error != 0) {
			VFS_TO_TMPFS(mp)->tm_ronly = 0;
			MNT_ILOCK(mp);
			mp->mnt_flag &= ~MNT_RDONLY;
			MNT_IUNLOCK(mp);
			goto out;
		}
		if (!tmpfs_check_rw_maps(mp))
			break;
	}
out:
	vfs_write_resume(mp, 0);
	return (error);
}

static int
tmpfs_mount(struct mount *mp)
{
	const size_t nodes_per_page = howmany(PAGE_SIZE,
	    sizeof(struct tmpfs_dirent) + sizeof(struct tmpfs_node));
	struct tmpfs_mount *tmp;
	struct tmpfs_node *root;
	int error;
	bool nomtime, nonc, pgread;
	/* Size counters. */
	u_quad_t pages;
	off_t nodes_max, size_max, maxfilesize, ea_max_size;

	/* Root node attributes. */
	uid_t root_uid;
	gid_t root_gid;
	mode_t root_mode;

	struct vattr va;

	if (vfs_filteropt(mp->mnt_optnew, tmpfs_opts))
		return (EINVAL);

	if (mp->mnt_flag & MNT_UPDATE) {
		/* Only support update mounts for certain options. */
		if (vfs_filteropt(mp->mnt_optnew, tmpfs_updateopts) != 0)
			return (EOPNOTSUPP);
		tmp = VFS_TO_TMPFS(mp);
		if (vfs_getopt_size(mp->mnt_optnew, "size", &size_max) == 0) {
			/*
			 * On-the-fly resizing is not supported (yet). We still
			 * need to have "size" listed as "supported", otherwise
			 * trying to update fs that is listed in fstab with size
			 * parameter, say trying to change rw to ro or vice
			 * versa, would cause vfs_filteropt() to bail.
			 */
			if (size_max != tmp->tm_size_max)
				return (EOPNOTSUPP);
		}
		if (vfs_getopt_size(mp->mnt_optnew, "easize", &ea_max_size) == 0) {
			tmp->tm_ea_memory_max = ea_max_size;
		}
		if (vfs_flagopt(mp->mnt_optnew, "ro", NULL, 0) &&
		    !tmp->tm_ronly) {
			/* RW -> RO */
			return (tmpfs_rw_to_ro(mp));
		} else if (!vfs_flagopt(mp->mnt_optnew, "ro", NULL, 0) &&
		    tmp->tm_ronly) {
			/* RO -> RW */
			tmp->tm_ronly = 0;
			MNT_ILOCK(mp);
			mp->mnt_flag &= ~MNT_RDONLY;
			MNT_IUNLOCK(mp);
		}
		tmp->tm_nomtime = vfs_getopt(mp->mnt_optnew, "nomtime", NULL,
		    0) == 0;
		MNT_ILOCK(mp);
		if ((mp->mnt_flag & MNT_UNION) == 0) {
			mp->mnt_kern_flag |= MNTK_FPLOOKUP;
		} else {
			mp->mnt_kern_flag &= ~MNTK_FPLOOKUP;
		}
		MNT_IUNLOCK(mp);
		return (0);
	}

	vn_lock(mp->mnt_vnodecovered, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(mp->mnt_vnodecovered, &va, mp->mnt_cred);
	VOP_UNLOCK(mp->mnt_vnodecovered);
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
	if (vfs_getopt_size(mp->mnt_optnew, "inodes", &nodes_max) != 0)
		nodes_max = 0;
	if (vfs_getopt_size(mp->mnt_optnew, "size", &size_max) != 0)
		size_max = 0;
	if (vfs_getopt_size(mp->mnt_optnew, "maxfilesize", &maxfilesize) != 0)
		maxfilesize = 0;
	if (vfs_getopt_size(mp->mnt_optnew, "easize", &ea_max_size) != 0)
		ea_max_size = 0;
	nonc = vfs_getopt(mp->mnt_optnew, "nonc", NULL, NULL) == 0;
	nomtime = vfs_getopt(mp->mnt_optnew, "nomtime", NULL, NULL) == 0;
	pgread = vfs_getopt(mp->mnt_optnew, "pgread", NULL, NULL) == 0;

	/* Do not allow mounts if we do not have enough memory to preserve
	 * the minimum reserved pages. */
	if (tmpfs_mem_avail() < TMPFS_PAGES_MINRESERVED)
		return (ENOSPC);

	/* Get the maximum number of memory pages this file system is
	 * allowed to use, based on the maximum size the user passed in
	 * the mount structure.  A value of zero is treated as if the
	 * maximum available space was requested. */
	if (size_max == 0 || size_max > OFF_MAX - PAGE_SIZE ||
	    (SIZE_MAX < OFF_MAX && size_max / PAGE_SIZE >= SIZE_MAX))
		pages = SIZE_MAX;
	else {
		size_max = roundup(size_max, PAGE_SIZE);
		pages = howmany(size_max, PAGE_SIZE);
	}
	MPASS(pages > 0);

	if (nodes_max <= 3) {
		if (pages < INT_MAX / nodes_per_page)
			nodes_max = pages * nodes_per_page;
		else
			nodes_max = INT_MAX;
	}
	if (nodes_max > INT_MAX)
		nodes_max = INT_MAX;
	MPASS(nodes_max >= 3);

	/* Allocate the tmpfs mount structure and fill it. */
	tmp = (struct tmpfs_mount *)malloc(sizeof(struct tmpfs_mount),
	    M_TMPFSMNT, M_WAITOK | M_ZERO);

	mtx_init(&tmp->tm_allnode_lock, "tmpfs allnode lock", NULL, MTX_DEF);
	tmp->tm_nodes_max = nodes_max;
	tmp->tm_nodes_inuse = 0;
	tmp->tm_ea_memory_inuse = 0;
	tmp->tm_refcount = 1;
	tmp->tm_maxfilesize = maxfilesize > 0 ? maxfilesize : OFF_MAX;
	tmp->tm_ea_memory_max = ea_max_size > 0 ?
	    ea_max_size : TMPFS_EA_MEMORY_RESERVED;
	LIST_INIT(&tmp->tm_nodes_used);

	tmp->tm_size_max = size_max;
	tmp->tm_pages_max = pages;
	tmp->tm_pages_used = 0;
	new_unrhdr64(&tmp->tm_ino_unr, 2);
	tmp->tm_ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	tmp->tm_nonc = nonc;
	tmp->tm_nomtime = nomtime;
	tmp->tm_pgread = pgread;

	/* Allocate the root node. */
	error = tmpfs_alloc_node(mp, tmp, VDIR, root_uid, root_gid,
	    root_mode & ALLPERMS, NULL, NULL, VNOVAL, &root);

	if (error != 0 || root == NULL) {
		free(tmp, M_TMPFSMNT);
		return (error);
	}
	KASSERT(root->tn_id == 2,
	    ("tmpfs root with invalid ino: %ju", (uintmax_t)root->tn_id));
	tmp->tm_root = root;

	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_LOOKUP_SHARED | MNTK_EXTENDED_SHARED |
	    MNTK_NOMSYNC;
	if (!nonc && (mp->mnt_flag & MNT_UNION) == 0)
		mp->mnt_kern_flag |= MNTK_FPLOOKUP;
	MNT_IUNLOCK(mp);

	mp->mnt_data = tmp;
	mp->mnt_stat.f_namemax = MAXNAMLEN;
	vfs_getnewfsid(mp);
	vfs_mountedfrom(mp, "tmpfs");

	return (0);
}

/* ARGSUSED2 */
static int
tmpfs_unmount(struct mount *mp, int mntflags)
{
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;
	int error, flags;

	flags = (mntflags & MNT_FORCE) != 0 ? FORCECLOSE : 0;
	tmp = VFS_TO_TMPFS(mp);

	/* Stop writers */
	error = vfs_write_suspend_umnt(mp);
	if (error != 0)
		return (error);
	/*
	 * At this point, nodes cannot be destroyed by any other
	 * thread because write suspension is started.
	 */

	for (;;) {
		error = vflush(mp, 0, flags, curthread);
		if (error != 0) {
			vfs_write_resume(mp, VR_START_WRITE);
			return (error);
		}
		MNT_ILOCK(mp);
		if (mp->mnt_nvnodelistsize == 0) {
			MNT_IUNLOCK(mp);
			break;
		}
		MNT_IUNLOCK(mp);
		if ((mntflags & MNT_FORCE) == 0) {
			vfs_write_resume(mp, VR_START_WRITE);
			return (EBUSY);
		}
	}

	TMPFS_LOCK(tmp);
	while ((node = LIST_FIRST(&tmp->tm_nodes_used)) != NULL) {
		TMPFS_NODE_LOCK(node);
		if (node->tn_type == VDIR)
			tmpfs_dir_destroy(tmp, node);
		if (tmpfs_free_node_locked(tmp, node, true))
			TMPFS_LOCK(tmp);
		else
			TMPFS_NODE_UNLOCK(node);
	}

	mp->mnt_data = NULL;
	tmpfs_free_tmp(tmp);
	vfs_write_resume(mp, VR_START_WRITE);

	return (0);
}

void
tmpfs_free_tmp(struct tmpfs_mount *tmp)
{
	TMPFS_MP_ASSERT_LOCKED(tmp);
	MPASS(tmp->tm_refcount > 0);

	tmp->tm_refcount--;
	if (tmp->tm_refcount > 0) {
		TMPFS_UNLOCK(tmp);
		return;
	}
	TMPFS_UNLOCK(tmp);

	mtx_destroy(&tmp->tm_allnode_lock);
	/*
	 * We cannot assert that tmp->tm_pages_used == 0 there,
	 * because tmpfs vm_objects might be still mapped by some
	 * process and outlive the mount due to reference counting.
	 */
	MPASS(tmp->tm_nodes_inuse == 0);

	free(tmp, M_TMPFSMNT);
}

static int
tmpfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	int error;

	error = tmpfs_alloc_vp(mp, VFS_TO_TMPFS(mp)->tm_root, flags, vpp);
	if (error == 0)
		(*vpp)->v_vflag |= VV_ROOT;
	return (error);
}

static int
tmpfs_fhtovp(struct mount *mp, struct fid *fhp, int flags,
    struct vnode **vpp)
{
	struct tmpfs_fid_data tfd;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;
	int error;

	if (fhp->fid_len != sizeof(tfd))
		return (EINVAL);

	/*
	 * Copy from fid_data onto the stack to avoid unaligned pointer use.
	 * See the comment in sys/mount.h on struct fid for details.
	 */
	memcpy(&tfd, fhp->fid_data, fhp->fid_len);

	tmp = VFS_TO_TMPFS(mp);

	if (tfd.tfd_id >= tmp->tm_nodes_max)
		return (EINVAL);

	TMPFS_LOCK(tmp);
	LIST_FOREACH(node, &tmp->tm_nodes_used, tn_entries) {
		if (node->tn_id == tfd.tfd_id &&
		    node->tn_gen == tfd.tfd_gen) {
			tmpfs_ref_node(node);
			break;
		}
	}
	TMPFS_UNLOCK(tmp);

	if (node != NULL) {
		error = tmpfs_alloc_vp(mp, node, LK_EXCLUSIVE, vpp);
		tmpfs_free_node(tmp, node);
	} else
		error = EINVAL;
	return (error);
}

/* ARGSUSED2 */
static int
tmpfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct tmpfs_mount *tmp;
	size_t used;

	tmp = VFS_TO_TMPFS(mp);

	sbp->f_iosize = PAGE_SIZE;
	sbp->f_bsize = PAGE_SIZE;

	used = tmpfs_pages_used(tmp);
	if (tmp->tm_pages_max != ULONG_MAX)
		 sbp->f_blocks = tmp->tm_pages_max;
	else
		 sbp->f_blocks = used + tmpfs_mem_avail();
	if (sbp->f_blocks <= used)
		sbp->f_bavail = 0;
	else
		sbp->f_bavail = sbp->f_blocks - used;
	sbp->f_bfree = sbp->f_bavail;
	used = tmp->tm_nodes_inuse;
	sbp->f_files = tmp->tm_nodes_max;
	if (sbp->f_files <= used)
		sbp->f_ffree = 0;
	else
		sbp->f_ffree = sbp->f_files - used;
	/* sbp->f_owner = tmp->tn_uid; */

	return (0);
}

static int
tmpfs_sync(struct mount *mp, int waitfor)
{

	if (waitfor == MNT_SUSPEND) {
		MNT_ILOCK(mp);
		mp->mnt_kern_flag |= MNTK_SUSPEND2 | MNTK_SUSPENDED;
		MNT_IUNLOCK(mp);
	} else if (waitfor == MNT_LAZY) {
		tmpfs_update_mtime_lazy(mp);
	}
	return (0);
}

static int
tmpfs_init(struct vfsconf *conf)
{
	int res;

	res = tmpfs_subr_init();
	if (res != 0)
		return (res);
	memcpy(&tmpfs_fnops, &vnops, sizeof(struct fileops));
	tmpfs_fnops.fo_close = tmpfs_fo_close;
	return (0);
}

static int
tmpfs_uninit(struct vfsconf *conf)
{
	tmpfs_subr_uninit();
	return (0);
}

/*
 * tmpfs vfs operations.
 */
struct vfsops tmpfs_vfsops = {
	.vfs_mount =			tmpfs_mount,
	.vfs_unmount =			tmpfs_unmount,
	.vfs_root =			vfs_cache_root,
	.vfs_cachedroot =		tmpfs_root,
	.vfs_statfs =			tmpfs_statfs,
	.vfs_fhtovp =			tmpfs_fhtovp,
	.vfs_sync =			tmpfs_sync,
	.vfs_init =			tmpfs_init,
	.vfs_uninit =			tmpfs_uninit,
};
VFS_SET(tmpfs_vfsops, tmpfs, VFCF_JAIL);

#ifdef DDB
#include <ddb/ddb.h>

static void
db_print_tmpfs(struct mount *mp, struct tmpfs_mount *tmp)
{
	db_printf("mp %p (%s) tmp %p\n", mp,
	    mp->mnt_stat.f_mntonname, tmp);
	db_printf(
	    "\tsize max %ju pages max %lu pages used %lu\n"
	    "\tinodes max %ju inodes inuse %ju ea inuse %ju refcount %ju\n"
	    "\tmaxfilesize %ju r%c %snamecache %smtime\n",
	    (uintmax_t)tmp->tm_size_max, tmp->tm_pages_max, tmp->tm_pages_used,
	    (uintmax_t)tmp->tm_nodes_max, (uintmax_t)tmp->tm_nodes_inuse,
	    (uintmax_t)tmp->tm_ea_memory_inuse, (uintmax_t)tmp->tm_refcount,
	    (uintmax_t)tmp->tm_maxfilesize,
	    tmp->tm_ronly ? 'o' : 'w', tmp->tm_nonc ? "no" : "",
	    tmp->tm_nomtime ? "no" : "");
}

DB_SHOW_COMMAND(tmpfs, db_show_tmpfs)
{
	struct mount *mp;
	struct tmpfs_mount *tmp;

	if (have_addr) {
		mp = (struct mount *)addr;
		tmp = VFS_TO_TMPFS(mp);
		db_print_tmpfs(mp, tmp);
		return;
	}

	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (strcmp(mp->mnt_stat.f_fstypename, tmpfs_vfsconf.vfc_name) ==
		    0) {
			tmp = VFS_TO_TMPFS(mp);
			db_print_tmpfs(mp, tmp);
		}
	}
}
#endif	/* DDB */
