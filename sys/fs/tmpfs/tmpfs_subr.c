/*	$NetBSD: tmpfs_subr.c,v 1.35 2007/07/09 21:10:50 ad Exp $	*/

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
 * Efficient memory file system supporting functions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/fnv_hash.h>
#include <sys/lock.h>
#include <sys/limits.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/smr.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>
#include <vm/uma.h>

#include <fs/tmpfs/tmpfs.h>
#include <fs/tmpfs/tmpfs_fifoops.h>
#include <fs/tmpfs/tmpfs_vnops.h>

SYSCTL_NODE(_vfs, OID_AUTO, tmpfs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "tmpfs file system");

static long tmpfs_pages_reserved = TMPFS_PAGES_MINRESERVED;
static long tmpfs_pages_avail_init;
static int tmpfs_mem_percent = TMPFS_MEM_PERCENT;
static void tmpfs_set_reserve_from_percent(void);

MALLOC_DEFINE(M_TMPFSDIR, "tmpfs dir", "tmpfs dirent structure");
static uma_zone_t tmpfs_node_pool;
VFS_SMR_DECLARE;

int tmpfs_pager_type = -1;

static vm_object_t
tmpfs_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t offset, struct ucred *cred)
{
	vm_object_t object;

	MPASS(handle == NULL);
	MPASS(offset == 0);
	object = vm_object_allocate_dyn(tmpfs_pager_type, size,
	    OBJ_COLORED | OBJ_SWAP);
	if (!swap_pager_init_object(object, NULL, NULL, size, 0)) {
		vm_object_deallocate(object);
		object = NULL;
	}
	return (object);
}

/*
 * Make sure tmpfs vnodes with writable mappings can be found on the lazy list.
 *
 * This allows for periodic mtime updates while only scanning vnodes which are
 * plausibly dirty, see tmpfs_update_mtime_lazy.
 */
static void
tmpfs_pager_writecount_recalc(vm_object_t object, vm_offset_t old,
    vm_offset_t new)
{
	struct vnode *vp;

	VM_OBJECT_ASSERT_WLOCKED(object);

	vp = VM_TO_TMPFS_VP(object);

	/*
	 * Forced unmount?
	 */
	if (vp == NULL || vp->v_object == NULL) {
		KASSERT((object->flags & OBJ_TMPFS_VREF) == 0,
		    ("object %p with OBJ_TMPFS_VREF but without vnode",
		    object));
		VM_OBJECT_WUNLOCK(object);
		return;
	}

	if (old == 0) {
		VNASSERT((object->flags & OBJ_TMPFS_VREF) == 0, vp,
		    ("object without writable mappings has a reference"));
		VNPASS(vp->v_usecount > 0, vp);
	} else {
		VNASSERT((object->flags & OBJ_TMPFS_VREF) != 0, vp,
		    ("object with writable mappings does not "
		    "have a reference"));
	}

	if (old == new) {
		VM_OBJECT_WUNLOCK(object);
		return;
	}

	if (new == 0) {
		vm_object_clear_flag(object, OBJ_TMPFS_VREF);
		VM_OBJECT_WUNLOCK(object);
		vrele(vp);
	} else {
		if ((object->flags & OBJ_TMPFS_VREF) == 0) {
			vref(vp);
			vlazy(vp);
			vm_object_set_flag(object, OBJ_TMPFS_VREF);
		}
		VM_OBJECT_WUNLOCK(object);
	}
}

static void
tmpfs_pager_update_writecount(vm_object_t object, vm_offset_t start,
    vm_offset_t end)
{
	vm_offset_t new, old;

	VM_OBJECT_WLOCK(object);
	KASSERT((object->flags & OBJ_ANON) == 0,
	    ("%s: object %p with OBJ_ANON", __func__, object));
	old = object->un_pager.swp.writemappings;
	object->un_pager.swp.writemappings += (vm_ooffset_t)end - start;
	new = object->un_pager.swp.writemappings;
	tmpfs_pager_writecount_recalc(object, old, new);
	VM_OBJECT_ASSERT_UNLOCKED(object);
}

static void
tmpfs_pager_release_writecount(vm_object_t object, vm_offset_t start,
    vm_offset_t end)
{
	vm_offset_t new, old;

	VM_OBJECT_WLOCK(object);
	KASSERT((object->flags & OBJ_ANON) == 0,
	    ("%s: object %p with OBJ_ANON", __func__, object));
	old = object->un_pager.swp.writemappings;
	KASSERT(old >= (vm_ooffset_t)end - start,
	    ("tmpfs obj %p writecount %jx dec %jx", object, (uintmax_t)old,
	    (uintmax_t)((vm_ooffset_t)end - start)));
	object->un_pager.swp.writemappings -= (vm_ooffset_t)end - start;
	new = object->un_pager.swp.writemappings;
	tmpfs_pager_writecount_recalc(object, old, new);
	VM_OBJECT_ASSERT_UNLOCKED(object);
}

static void
tmpfs_pager_getvp(vm_object_t object, struct vnode **vpp, bool *vp_heldp)
{
	struct vnode *vp;

	/*
	 * Tmpfs VREG node, which was reclaimed, has tmpfs_pager_type
	 * type.  In this case there is no v_writecount to adjust.
	 */
	if (vp_heldp != NULL)
		VM_OBJECT_RLOCK(object);
	else
		VM_OBJECT_ASSERT_LOCKED(object);
	if ((object->flags & OBJ_TMPFS) != 0) {
		vp = VM_TO_TMPFS_VP(object);
		if (vp != NULL) {
			*vpp = vp;
			if (vp_heldp != NULL) {
				vhold(vp);
				*vp_heldp = true;
			}
		}
	}
	if (vp_heldp != NULL)
		VM_OBJECT_RUNLOCK(object);
}

static void
tmpfs_pager_freespace(vm_object_t obj, vm_pindex_t start, vm_size_t size)
{
	struct tmpfs_node *node;
	struct tmpfs_mount *tm;
	vm_size_t c;

	swap_pager_freespace(obj, start, size, &c);
	if ((obj->flags & OBJ_TMPFS) == 0 || c == 0)
		return;

	node = obj->un_pager.swp.swp_priv;
	MPASS(node->tn_type == VREG);
	tm = node->tn_reg.tn_tmp;

	KASSERT(tm->tm_pages_used >= c,
	    ("tmpfs tm %p pages %jd free %jd", tm,
	    (uintmax_t)tm->tm_pages_used, (uintmax_t)c));
	atomic_add_long(&tm->tm_pages_used, -c);
	KASSERT(node->tn_reg.tn_pages >= c,
	    ("tmpfs node %p pages %jd free %jd", node,
	    (uintmax_t)node->tn_reg.tn_pages, (uintmax_t)c));
	node->tn_reg.tn_pages -= c;
}

static void
tmpfs_page_inserted(vm_object_t obj, vm_page_t m)
{
	struct tmpfs_node *node;
	struct tmpfs_mount *tm;

	if ((obj->flags & OBJ_TMPFS) == 0)
		return;

	node = obj->un_pager.swp.swp_priv;
	MPASS(node->tn_type == VREG);
	tm = node->tn_reg.tn_tmp;

	if (!vm_pager_has_page(obj, m->pindex, NULL, NULL)) {
		atomic_add_long(&tm->tm_pages_used, 1);
		node->tn_reg.tn_pages += 1;
	}
}

static void
tmpfs_page_removed(vm_object_t obj, vm_page_t m)
{
	struct tmpfs_node *node;
	struct tmpfs_mount *tm;

	if ((obj->flags & OBJ_TMPFS) == 0)
		return;

	node = obj->un_pager.swp.swp_priv;
	MPASS(node->tn_type == VREG);
	tm = node->tn_reg.tn_tmp;

	if (!vm_pager_has_page(obj, m->pindex, NULL, NULL)) {
		KASSERT(tm->tm_pages_used >= 1,
		    ("tmpfs tm %p pages %jd free 1", tm,
		    (uintmax_t)tm->tm_pages_used));
		atomic_add_long(&tm->tm_pages_used, -1);
		KASSERT(node->tn_reg.tn_pages >= 1,
		    ("tmpfs node %p pages %jd free 1", node,
		    (uintmax_t)node->tn_reg.tn_pages));
		node->tn_reg.tn_pages -= 1;
	}
}

static boolean_t
tmpfs_can_alloc_page(vm_object_t obj, vm_pindex_t pindex)
{
	struct tmpfs_mount *tm;

	tm = VM_TO_TMPFS_MP(obj);
	if (tm == NULL || vm_pager_has_page(obj, pindex, NULL, NULL) ||
	    tm->tm_pages_max == 0)
		return (true);
	if (tm->tm_pages_max == ULONG_MAX)
		return (tmpfs_mem_avail() >= 1);
	return (tm->tm_pages_max > atomic_load_long(&tm->tm_pages_used));
}

struct pagerops tmpfs_pager_ops = {
	.pgo_kvme_type = KVME_TYPE_VNODE,
	.pgo_alloc = tmpfs_pager_alloc,
	.pgo_set_writeable_dirty = vm_object_set_writeable_dirty_,
	.pgo_update_writecount = tmpfs_pager_update_writecount,
	.pgo_release_writecount = tmpfs_pager_release_writecount,
	.pgo_mightbedirty = vm_object_mightbedirty_,
	.pgo_getvp = tmpfs_pager_getvp,
	.pgo_freespace = tmpfs_pager_freespace,
	.pgo_page_inserted = tmpfs_page_inserted,
	.pgo_page_removed = tmpfs_page_removed,
	.pgo_can_alloc_page = tmpfs_can_alloc_page,
};

static int
tmpfs_node_ctor(void *mem, int size, void *arg, int flags)
{
	struct tmpfs_node *node;

	node = mem;
	node->tn_gen++;
	node->tn_size = 0;
	node->tn_status = 0;
	node->tn_accessed = false;
	node->tn_flags = 0;
	node->tn_links = 0;
	node->tn_vnode = NULL;
	node->tn_vpstate = 0;
	return (0);
}

static void
tmpfs_node_dtor(void *mem, int size, void *arg)
{
	struct tmpfs_node *node;

	node = mem;
	node->tn_type = VNON;
}

static int
tmpfs_node_init(void *mem, int size, int flags)
{
	struct tmpfs_node *node;

	node = mem;
	node->tn_id = 0;
	mtx_init(&node->tn_interlock, "tmpfsni", NULL, MTX_DEF | MTX_NEW);
	node->tn_gen = arc4random();
	return (0);
}

static void
tmpfs_node_fini(void *mem, int size)
{
	struct tmpfs_node *node;

	node = mem;
	mtx_destroy(&node->tn_interlock);
}

int
tmpfs_subr_init(void)
{
	tmpfs_pager_type = vm_pager_alloc_dyn_type(&tmpfs_pager_ops,
	    OBJT_SWAP);
	if (tmpfs_pager_type == -1)
		return (EINVAL);
	tmpfs_node_pool = uma_zcreate("TMPFS node",
	    sizeof(struct tmpfs_node), tmpfs_node_ctor, tmpfs_node_dtor,
	    tmpfs_node_init, tmpfs_node_fini, UMA_ALIGN_PTR, 0);
	VFS_SMR_ZONE_SET(tmpfs_node_pool);

	tmpfs_pages_avail_init = tmpfs_mem_avail();
	tmpfs_set_reserve_from_percent();
	return (0);
}

void
tmpfs_subr_uninit(void)
{
	if (tmpfs_pager_type != -1)
		vm_pager_free_dyn_type(tmpfs_pager_type);
	tmpfs_pager_type = -1;
	uma_zdestroy(tmpfs_node_pool);
}

static int
sysctl_mem_reserved(SYSCTL_HANDLER_ARGS)
{
	int error;
	long pages, bytes;

	pages = *(long *)arg1;
	bytes = pages * PAGE_SIZE;

	error = sysctl_handle_long(oidp, &bytes, 0, req);
	if (error || !req->newptr)
		return (error);

	pages = bytes / PAGE_SIZE;
	if (pages < TMPFS_PAGES_MINRESERVED)
		return (EINVAL);

	*(long *)arg1 = pages;
	return (0);
}

SYSCTL_PROC(_vfs_tmpfs, OID_AUTO, memory_reserved,
    CTLTYPE_LONG | CTLFLAG_MPSAFE | CTLFLAG_RW, &tmpfs_pages_reserved, 0,
    sysctl_mem_reserved, "L",
    "Amount of available memory and swap below which tmpfs growth stops");

static int
sysctl_mem_percent(SYSCTL_HANDLER_ARGS)
{
	int error, percent;

	percent = *(int *)arg1;
	error = sysctl_handle_int(oidp, &percent, 0, req);
	if (error || !req->newptr)
		return (error);

	if ((unsigned) percent > 100)
		return (EINVAL);

	*(int *)arg1 = percent;
	tmpfs_set_reserve_from_percent();
	return (0);
}

static void
tmpfs_set_reserve_from_percent(void)
{
	size_t reserved;

	reserved = tmpfs_pages_avail_init * (100 - tmpfs_mem_percent) / 100;
	tmpfs_pages_reserved = max(reserved, TMPFS_PAGES_MINRESERVED);
}

SYSCTL_PROC(_vfs_tmpfs, OID_AUTO, memory_percent,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RWTUN, &tmpfs_mem_percent, 0,
    sysctl_mem_percent, "I",
    "Percent of available memory that can be used if no size limit");

static __inline int tmpfs_dirtree_cmp(struct tmpfs_dirent *a,
    struct tmpfs_dirent *b);
RB_PROTOTYPE_STATIC(tmpfs_dir, tmpfs_dirent, uh.td_entries, tmpfs_dirtree_cmp);

size_t
tmpfs_mem_avail(void)
{
	size_t avail;
	long reserved;

	avail = swap_pager_avail + vm_free_count();
	reserved = atomic_load_long(&tmpfs_pages_reserved);
	if (__predict_false(avail < reserved))
		return (0);
	return (avail - reserved);
}

size_t
tmpfs_pages_used(struct tmpfs_mount *tmp)
{
	const size_t node_size = sizeof(struct tmpfs_node) +
	    sizeof(struct tmpfs_dirent);
	size_t meta_pages;

	meta_pages = howmany((uintmax_t)tmp->tm_nodes_inuse * node_size,
	    PAGE_SIZE);
	return (meta_pages + tmp->tm_pages_used);
}

bool
tmpfs_pages_check_avail(struct tmpfs_mount *tmp, size_t req_pages)
{
	if (tmpfs_mem_avail() < req_pages)
		return (false);

	if (tmp->tm_pages_max != ULONG_MAX &&
	    tmp->tm_pages_max < req_pages + tmpfs_pages_used(tmp))
		return (false);

	return (true);
}

static int
tmpfs_partial_page_invalidate(vm_object_t object, vm_pindex_t idx, int base,
    int end, boolean_t ignerr)
{
	int error;

	error = vm_page_grab_zero_partial(object, idx, base, end);
	if (ignerr)
		error = 0;
	return (error);
}

void
tmpfs_ref_node(struct tmpfs_node *node)
{
#ifdef INVARIANTS
	u_int old;

	old =
#endif
	refcount_acquire(&node->tn_refcount);
#ifdef INVARIANTS
	KASSERT(old > 0, ("node %p zero refcount", node));
#endif
}

/*
 * Allocates a new node of type 'type' inside the 'tmp' mount point, with
 * its owner set to 'uid', its group to 'gid' and its mode set to 'mode',
 * using the credentials of the process 'p'.
 *
 * If the node type is set to 'VDIR', then the parent parameter must point
 * to the parent directory of the node being created.  It may only be NULL
 * while allocating the root node.
 *
 * If the node type is set to 'VBLK' or 'VCHR', then the rdev parameter
 * specifies the device the node represents.
 *
 * If the node type is set to 'VLNK', then the parameter target specifies
 * the file name of the target file for the symbolic link that is being
 * created.
 *
 * Note that new nodes are retrieved from the available list if it has
 * items or, if it is empty, from the node pool as long as there is enough
 * space to create them.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_alloc_node(struct mount *mp, struct tmpfs_mount *tmp, __enum_uint8(vtype) type,
    uid_t uid, gid_t gid, mode_t mode, struct tmpfs_node *parent,
    const char *target, dev_t rdev, struct tmpfs_node **node)
{
	struct tmpfs_node *nnode;
	char *symlink;
	char symlink_smr;

	/* If the root directory of the 'tmp' file system is not yet
	 * allocated, this must be the request to do it. */
	MPASS(IMPLIES(tmp->tm_root == NULL, parent == NULL && type == VDIR));

	MPASS((type == VLNK) ^ (target == NULL));
	MPASS((type == VBLK || type == VCHR) ^ (rdev == VNOVAL));

	if (tmp->tm_nodes_inuse >= tmp->tm_nodes_max)
		return (ENOSPC);
	if (!tmpfs_pages_check_avail(tmp, 1))
		return (ENOSPC);

	if ((mp->mnt_kern_flag & MNTK_UNMOUNT) != 0) {
		/*
		 * When a new tmpfs node is created for fully
		 * constructed mount point, there must be a parent
		 * node, which vnode is locked exclusively.  As
		 * consequence, if the unmount is executing in
		 * parallel, vflush() cannot reclaim the parent vnode.
		 * Due to this, the check for MNTK_UNMOUNT flag is not
		 * racy: if we did not see MNTK_UNMOUNT flag, then tmp
		 * cannot be destroyed until node construction is
		 * finished and the parent vnode unlocked.
		 *
		 * Tmpfs does not need to instantiate new nodes during
		 * unmount.
		 */
		return (EBUSY);
	}
	if ((mp->mnt_kern_flag & MNT_RDONLY) != 0)
		return (EROFS);

	nnode = uma_zalloc_smr(tmpfs_node_pool, M_WAITOK);

	/* Generic initialization. */
	nnode->tn_type = type;
	vfs_timestamp(&nnode->tn_atime);
	nnode->tn_birthtime = nnode->tn_ctime = nnode->tn_mtime =
	    nnode->tn_atime;
	nnode->tn_uid = uid;
	nnode->tn_gid = gid;
	nnode->tn_mode = mode;
	nnode->tn_id = alloc_unr64(&tmp->tm_ino_unr);
	nnode->tn_refcount = 1;
	LIST_INIT(&nnode->tn_extattrs);

	/* Type-specific initialization. */
	switch (nnode->tn_type) {
	case VBLK:
	case VCHR:
		nnode->tn_rdev = rdev;
		break;

	case VDIR:
		RB_INIT(&nnode->tn_dir.tn_dirhead);
		LIST_INIT(&nnode->tn_dir.tn_dupindex);
		MPASS(parent != nnode);
		MPASS(IMPLIES(parent == NULL, tmp->tm_root == NULL));
		nnode->tn_dir.tn_parent = (parent == NULL) ? nnode : parent;
		nnode->tn_dir.tn_readdir_lastn = 0;
		nnode->tn_dir.tn_readdir_lastp = NULL;
		nnode->tn_dir.tn_wht_size = 0;
		nnode->tn_links++;
		TMPFS_NODE_LOCK(nnode->tn_dir.tn_parent);
		nnode->tn_dir.tn_parent->tn_links++;
		TMPFS_NODE_UNLOCK(nnode->tn_dir.tn_parent);
		break;

	case VFIFO:
		/* FALLTHROUGH */
	case VSOCK:
		break;

	case VLNK:
		MPASS(strlen(target) < MAXPATHLEN);
		nnode->tn_size = strlen(target);

		symlink = NULL;
		if (!tmp->tm_nonc) {
			symlink = cache_symlink_alloc(nnode->tn_size + 1,
			    M_WAITOK);
			symlink_smr = true;
		}
		if (symlink == NULL) {
			symlink = malloc(nnode->tn_size + 1, M_TMPFSNAME,
			    M_WAITOK);
			symlink_smr = false;
		}
		memcpy(symlink, target, nnode->tn_size + 1);

		/*
		 * Allow safe symlink resolving for lockless lookup.
		 * tmpfs_fplookup_symlink references this comment.
		 *
		 * 1. nnode is not yet visible to the world
		 * 2. both tn_link_target and tn_link_smr get populated
		 * 3. release fence publishes their content
		 * 4. tn_link_target content is immutable until node
		 *    destruction, where the pointer gets set to NULL
		 * 5. tn_link_smr is never changed once set
		 *
		 * As a result it is sufficient to issue load consume
		 * on the node pointer to also get the above content
		 * in a stable manner.  Worst case tn_link_smr flag
		 * may be set to true despite being stale, while the
		 * target buffer is already cleared out.
		 */
		atomic_store_ptr(&nnode->tn_link_target, symlink);
		atomic_store_char((char *)&nnode->tn_link_smr, symlink_smr);
		atomic_thread_fence_rel();
		break;

	case VREG:
		nnode->tn_reg.tn_aobj =
		    vm_pager_allocate(tmpfs_pager_type, NULL, 0,
		    VM_PROT_DEFAULT, 0,
		    NULL /* XXXKIB - tmpfs needs swap reservation */);
		nnode->tn_reg.tn_aobj->un_pager.swp.swp_priv = nnode;
		vm_object_set_flag(nnode->tn_reg.tn_aobj, OBJ_TMPFS);
		nnode->tn_reg.tn_tmp = tmp;
		nnode->tn_reg.tn_pages = 0;
		break;

	default:
		panic("tmpfs_alloc_node: type %p %d", nnode,
		    (int)nnode->tn_type);
	}

	TMPFS_LOCK(tmp);
	LIST_INSERT_HEAD(&tmp->tm_nodes_used, nnode, tn_entries);
	nnode->tn_attached = true;
	tmp->tm_nodes_inuse++;
	tmp->tm_refcount++;
	TMPFS_UNLOCK(tmp);

	*node = nnode;
	return (0);
}

/*
 * Destroys the node pointed to by node from the file system 'tmp'.
 * If the node references a directory, no entries are allowed.
 */
void
tmpfs_free_node(struct tmpfs_mount *tmp, struct tmpfs_node *node)
{
	if (refcount_release_if_not_last(&node->tn_refcount))
		return;

	TMPFS_LOCK(tmp);
	TMPFS_NODE_LOCK(node);
	if (!tmpfs_free_node_locked(tmp, node, false)) {
		TMPFS_NODE_UNLOCK(node);
		TMPFS_UNLOCK(tmp);
	}
}

bool
tmpfs_free_node_locked(struct tmpfs_mount *tmp, struct tmpfs_node *node,
    bool detach)
{
	struct tmpfs_extattr *ea;
	vm_object_t uobj;
	char *symlink;
	bool last;

	TMPFS_MP_ASSERT_LOCKED(tmp);
	TMPFS_NODE_ASSERT_LOCKED(node);

	last = refcount_release(&node->tn_refcount);
	if (node->tn_attached && (detach || last)) {
		MPASS(tmp->tm_nodes_inuse > 0);
		tmp->tm_nodes_inuse--;
		LIST_REMOVE(node, tn_entries);
		node->tn_attached = false;
	}
	if (!last)
		return (false);

	TMPFS_NODE_UNLOCK(node);

#ifdef INVARIANTS
	MPASS(node->tn_vnode == NULL);
	MPASS((node->tn_vpstate & TMPFS_VNODE_ALLOCATING) == 0);

	/*
	 * Make sure this is a node type we can deal with. Everything
	 * is explicitly enumerated without the 'default' clause so
	 * the compiler can throw an error in case a new type is
	 * added.
	 */
	switch (node->tn_type) {
	case VBLK:
	case VCHR:
	case VDIR:
	case VFIFO:
	case VSOCK:
	case VLNK:
	case VREG:
		break;
	case VNON:
	case VBAD:
	case VMARKER:
		panic("%s: bad type %d for node %p", __func__,
		    (int)node->tn_type, node);
	}
#endif

	while ((ea = LIST_FIRST(&node->tn_extattrs)) != NULL) {
		LIST_REMOVE(ea, ea_extattrs);
		tmpfs_extattr_free(ea);
	}

	switch (node->tn_type) {
	case VREG:
		uobj = node->tn_reg.tn_aobj;
		node->tn_reg.tn_aobj = NULL;
		if (uobj != NULL) {
			VM_OBJECT_WLOCK(uobj);
			KASSERT((uobj->flags & OBJ_TMPFS) != 0,
			    ("tmpfs node %p uobj %p not tmpfs", node, uobj));
			vm_object_clear_flag(uobj, OBJ_TMPFS);
			KASSERT(tmp->tm_pages_used >= node->tn_reg.tn_pages,
			    ("tmpfs tmp %p node %p pages %jd free %jd", tmp,
			    node, (uintmax_t)tmp->tm_pages_used,
			    (uintmax_t)node->tn_reg.tn_pages));
			atomic_add_long(&tmp->tm_pages_used,
			    -node->tn_reg.tn_pages);
			VM_OBJECT_WUNLOCK(uobj);
		}
		tmpfs_free_tmp(tmp);

		/*
		 * vm_object_deallocate() must not be called while
		 * owning tm_allnode_lock, because deallocate might
		 * sleep.  Call it after tmpfs_free_tmp() does the
		 * unlock.
		 */
		if (uobj != NULL)
			vm_object_deallocate(uobj);

		break;
	case VLNK:
		tmpfs_free_tmp(tmp);

		symlink = node->tn_link_target;
		atomic_store_ptr(&node->tn_link_target, NULL);
		if (atomic_load_char(&node->tn_link_smr)) {
			cache_symlink_free(symlink, node->tn_size + 1);
		} else {
			free(symlink, M_TMPFSNAME);
		}
		break;
	default:
		tmpfs_free_tmp(tmp);
		break;
	}

	uma_zfree_smr(tmpfs_node_pool, node);
	return (true);
}

static __inline uint32_t
tmpfs_dirent_hash(const char *name, u_int len)
{
	uint32_t hash;

	hash = fnv_32_buf(name, len, FNV1_32_INIT + len) & TMPFS_DIRCOOKIE_MASK;
#ifdef TMPFS_DEBUG_DIRCOOKIE_DUP
	hash &= 0xf;
#endif
	if (hash < TMPFS_DIRCOOKIE_MIN)
		hash += TMPFS_DIRCOOKIE_MIN;

	return (hash);
}

static __inline off_t
tmpfs_dirent_cookie(struct tmpfs_dirent *de)
{
	if (de == NULL)
		return (TMPFS_DIRCOOKIE_EOF);

	MPASS(de->td_cookie >= TMPFS_DIRCOOKIE_MIN);

	return (de->td_cookie);
}

static __inline boolean_t
tmpfs_dirent_dup(struct tmpfs_dirent *de)
{
	return ((de->td_cookie & TMPFS_DIRCOOKIE_DUP) != 0);
}

static __inline boolean_t
tmpfs_dirent_duphead(struct tmpfs_dirent *de)
{
	return ((de->td_cookie & TMPFS_DIRCOOKIE_DUPHEAD) != 0);
}

void
tmpfs_dirent_init(struct tmpfs_dirent *de, const char *name, u_int namelen)
{
	de->td_hash = de->td_cookie = tmpfs_dirent_hash(name, namelen);
	memcpy(de->ud.td_name, name, namelen);
	de->td_namelen = namelen;
}

/*
 * Allocates a new directory entry for the node node with a name of name.
 * The new directory entry is returned in *de.
 *
 * The link count of node is increased by one to reflect the new object
 * referencing it.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_alloc_dirent(struct tmpfs_mount *tmp, struct tmpfs_node *node,
    const char *name, u_int len, struct tmpfs_dirent **de)
{
	struct tmpfs_dirent *nde;

	nde = malloc(sizeof(*nde), M_TMPFSDIR, M_WAITOK);
	nde->td_node = node;
	if (name != NULL) {
		nde->ud.td_name = malloc(len, M_TMPFSNAME, M_WAITOK);
		tmpfs_dirent_init(nde, name, len);
	} else
		nde->td_namelen = 0;
	if (node != NULL)
		node->tn_links++;

	*de = nde;

	return (0);
}

/*
 * Frees a directory entry.  It is the caller's responsibility to destroy
 * the node referenced by it if needed.
 *
 * The link count of node is decreased by one to reflect the removal of an
 * object that referenced it.  This only happens if 'node_exists' is true;
 * otherwise the function will not access the node referred to by the
 * directory entry, as it may already have been released from the outside.
 */
void
tmpfs_free_dirent(struct tmpfs_mount *tmp, struct tmpfs_dirent *de)
{
	struct tmpfs_node *node;

	node = de->td_node;
	if (node != NULL) {
		MPASS(node->tn_links > 0);
		node->tn_links--;
	}
	if (!tmpfs_dirent_duphead(de) && de->ud.td_name != NULL)
		free(de->ud.td_name, M_TMPFSNAME);
	free(de, M_TMPFSDIR);
}

void
tmpfs_destroy_vobject(struct vnode *vp, vm_object_t obj)
{
	bool want_vrele;

	ASSERT_VOP_ELOCKED(vp, "tmpfs_destroy_vobject");
	if (vp->v_type != VREG || obj == NULL)
		return;

	VM_OBJECT_WLOCK(obj);
	VI_LOCK(vp);
	vp->v_object = NULL;

	/*
	 * May be going through forced unmount.
	 */
	want_vrele = false;
	if ((obj->flags & OBJ_TMPFS_VREF) != 0) {
		vm_object_clear_flag(obj, OBJ_TMPFS_VREF);
		want_vrele = true;
	}

	if (vp->v_writecount < 0)
		vp->v_writecount = 0;
	VI_UNLOCK(vp);
	VM_OBJECT_WUNLOCK(obj);
	if (want_vrele) {
		vrele(vp);
	}
}

/*
 * Allocates a new vnode for the node node or returns a new reference to
 * an existing one if the node had already a vnode referencing it.  The
 * resulting locked vnode is returned in *vpp.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_alloc_vp(struct mount *mp, struct tmpfs_node *node, int lkflag,
    struct vnode **vpp)
{
	struct vnode *vp;
	enum vgetstate vs;
	struct tmpfs_mount *tm;
	vm_object_t object;
	int error;

	error = 0;
	tm = VFS_TO_TMPFS(mp);
	TMPFS_NODE_LOCK(node);
	tmpfs_ref_node(node);
loop:
	TMPFS_NODE_ASSERT_LOCKED(node);
	if ((vp = node->tn_vnode) != NULL) {
		MPASS((node->tn_vpstate & TMPFS_VNODE_DOOMED) == 0);
		if ((node->tn_type == VDIR && node->tn_dir.tn_parent == NULL) ||
		    (VN_IS_DOOMED(vp) &&
		     (lkflag & LK_NOWAIT) != 0)) {
			TMPFS_NODE_UNLOCK(node);
			error = ENOENT;
			vp = NULL;
			goto out;
		}
		if (VN_IS_DOOMED(vp)) {
			node->tn_vpstate |= TMPFS_VNODE_WRECLAIM;
			while ((node->tn_vpstate & TMPFS_VNODE_WRECLAIM) != 0) {
				msleep(&node->tn_vnode, TMPFS_NODE_MTX(node),
				    0, "tmpfsE", 0);
			}
			goto loop;
		}
		vs = vget_prep(vp);
		TMPFS_NODE_UNLOCK(node);
		error = vget_finish(vp, lkflag, vs);
		if (error == ENOENT) {
			TMPFS_NODE_LOCK(node);
			goto loop;
		}
		if (error != 0) {
			vp = NULL;
			goto out;
		}

		/*
		 * Make sure the vnode is still there after
		 * getting the interlock to avoid racing a free.
		 */
		if (node->tn_vnode != vp) {
			vput(vp);
			TMPFS_NODE_LOCK(node);
			goto loop;
		}

		goto out;
	}

	if ((node->tn_vpstate & TMPFS_VNODE_DOOMED) ||
	    (node->tn_type == VDIR && node->tn_dir.tn_parent == NULL)) {
		TMPFS_NODE_UNLOCK(node);
		error = ENOENT;
		vp = NULL;
		goto out;
	}

	/*
	 * otherwise lock the vp list while we call getnewvnode
	 * since that can block.
	 */
	if (node->tn_vpstate & TMPFS_VNODE_ALLOCATING) {
		node->tn_vpstate |= TMPFS_VNODE_WANT;
		error = msleep((caddr_t) &node->tn_vpstate,
		    TMPFS_NODE_MTX(node), 0, "tmpfs_alloc_vp", 0);
		if (error != 0)
			goto out;
		goto loop;
	} else
		node->tn_vpstate |= TMPFS_VNODE_ALLOCATING;

	TMPFS_NODE_UNLOCK(node);

	/* Get a new vnode and associate it with our node. */
	error = getnewvnode("tmpfs", mp, VFS_TO_TMPFS(mp)->tm_nonc ?
	    &tmpfs_vnodeop_nonc_entries : &tmpfs_vnodeop_entries, &vp);
	if (error != 0)
		goto unlock;
	MPASS(vp != NULL);

	/* lkflag is ignored, the lock is exclusive */
	(void) vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	vp->v_data = node;
	vp->v_type = node->tn_type;

	/* Type-specific initialization. */
	switch (node->tn_type) {
	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VLNK:
		/* FALLTHROUGH */
	case VSOCK:
		break;
	case VFIFO:
		vp->v_op = &tmpfs_fifoop_entries;
		break;
	case VREG:
		object = node->tn_reg.tn_aobj;
		VM_OBJECT_WLOCK(object);
		KASSERT((object->flags & OBJ_TMPFS_VREF) == 0,
		    ("%s: object %p with OBJ_TMPFS_VREF but without vnode",
		    __func__, object));
		VI_LOCK(vp);
		KASSERT(vp->v_object == NULL, ("Not NULL v_object in tmpfs"));
		vp->v_object = object;
		vn_irflag_set_locked(vp, (tm->tm_pgread ? VIRF_PGREAD : 0) |
		    VIRF_TEXT_REF);
		VI_UNLOCK(vp);
		VNASSERT((object->flags & OBJ_TMPFS_VREF) == 0, vp,
		    ("leaked OBJ_TMPFS_VREF"));
		if (object->un_pager.swp.writemappings > 0) {
			vrefact(vp);
			vlazy(vp);
			vm_object_set_flag(object, OBJ_TMPFS_VREF);
		}
		VM_OBJECT_WUNLOCK(object);
		break;
	case VDIR:
		MPASS(node->tn_dir.tn_parent != NULL);
		if (node->tn_dir.tn_parent == node)
			vp->v_vflag |= VV_ROOT;
		break;

	default:
		panic("tmpfs_alloc_vp: type %p %d", node, (int)node->tn_type);
	}
	if (vp->v_type != VFIFO)
		VN_LOCK_ASHARE(vp);

	error = insmntque1(vp, mp);
	if (error != 0) {
		/* Need to clear v_object for insmntque failure. */
		tmpfs_destroy_vobject(vp, vp->v_object);
		vp->v_object = NULL;
		vp->v_data = NULL;
		vp->v_op = &dead_vnodeops;
		vgone(vp);
		vput(vp);
		vp = NULL;
	} else {
		vn_set_state(vp, VSTATE_CONSTRUCTED);
	}

unlock:
	TMPFS_NODE_LOCK(node);

	MPASS(node->tn_vpstate & TMPFS_VNODE_ALLOCATING);
	node->tn_vpstate &= ~TMPFS_VNODE_ALLOCATING;
	node->tn_vnode = vp;

	if (node->tn_vpstate & TMPFS_VNODE_WANT) {
		node->tn_vpstate &= ~TMPFS_VNODE_WANT;
		TMPFS_NODE_UNLOCK(node);
		wakeup((caddr_t) &node->tn_vpstate);
	} else
		TMPFS_NODE_UNLOCK(node);

out:
	if (error == 0) {
		*vpp = vp;

#ifdef INVARIANTS
		MPASS(*vpp != NULL);
		ASSERT_VOP_LOCKED(*vpp, __func__);
		TMPFS_NODE_LOCK(node);
		MPASS(*vpp == node->tn_vnode);
		TMPFS_NODE_UNLOCK(node);
#endif
	}
	tmpfs_free_node(tm, node);

	return (error);
}

/*
 * Destroys the association between the vnode vp and the node it
 * references.
 */
void
tmpfs_free_vp(struct vnode *vp)
{
	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	TMPFS_NODE_ASSERT_LOCKED(node);
	node->tn_vnode = NULL;
	if ((node->tn_vpstate & TMPFS_VNODE_WRECLAIM) != 0)
		wakeup(&node->tn_vnode);
	node->tn_vpstate &= ~TMPFS_VNODE_WRECLAIM;
	vp->v_data = NULL;
}

/*
 * Allocates a new file of type 'type' and adds it to the parent directory
 * 'dvp'; this addition is done using the component name given in 'cnp'.
 * The ownership of the new file is automatically assigned based on the
 * credentials of the caller (through 'cnp'), the group is set based on
 * the parent directory and the mode is determined from the 'vap' argument.
 * If successful, *vpp holds a vnode to the newly created file and zero
 * is returned.  Otherwise *vpp is NULL and the function returns an
 * appropriate error code.
 */
int
tmpfs_alloc_file(struct vnode *dvp, struct vnode **vpp, struct vattr *vap,
    struct componentname *cnp, const char *target)
{
	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *dnode;
	struct tmpfs_node *node;
	struct tmpfs_node *parent;

	ASSERT_VOP_ELOCKED(dvp, "tmpfs_alloc_file");

	tmp = VFS_TO_TMPFS(dvp->v_mount);
	dnode = VP_TO_TMPFS_DIR(dvp);
	*vpp = NULL;

	/* If the entry we are creating is a directory, we cannot overflow
	 * the number of links of its parent, because it will get a new
	 * link. */
	if (vap->va_type == VDIR) {
		/* Ensure that we do not overflow the maximum number of links
		 * imposed by the system. */
		MPASS(dnode->tn_links <= TMPFS_LINK_MAX);
		if (dnode->tn_links == TMPFS_LINK_MAX) {
			return (EMLINK);
		}

		parent = dnode;
		MPASS(parent != NULL);
	} else
		parent = NULL;

	/* Allocate a node that represents the new file. */
	error = tmpfs_alloc_node(dvp->v_mount, tmp, vap->va_type,
	    cnp->cn_cred->cr_uid, dnode->tn_gid, vap->va_mode, parent,
	    target, vap->va_rdev, &node);
	if (error != 0)
		return (error);

	/* Allocate a directory entry that points to the new file. */
	error = tmpfs_alloc_dirent(tmp, node, cnp->cn_nameptr, cnp->cn_namelen,
	    &de);
	if (error != 0) {
		tmpfs_free_node(tmp, node);
		return (error);
	}

	/* Allocate a vnode for the new file. */
	error = tmpfs_alloc_vp(dvp->v_mount, node, LK_EXCLUSIVE, vpp);
	if (error != 0) {
		tmpfs_free_dirent(tmp, de);
		tmpfs_free_node(tmp, node);
		return (error);
	}

	/* Now that all required items are allocated, we can proceed to
	 * insert the new node into the directory, an operation that
	 * cannot fail. */
	if (cnp->cn_flags & ISWHITEOUT)
		tmpfs_dir_whiteout_remove(dvp, cnp);
	tmpfs_dir_attach(dvp, de);
	return (0);
}

struct tmpfs_dirent *
tmpfs_dir_first(struct tmpfs_node *dnode, struct tmpfs_dir_cursor *dc)
{
	struct tmpfs_dirent *de;

	de = RB_MIN(tmpfs_dir, &dnode->tn_dir.tn_dirhead);
	dc->tdc_tree = de;
	if (de != NULL && tmpfs_dirent_duphead(de))
		de = LIST_FIRST(&de->ud.td_duphead);
	dc->tdc_current = de;

	return (dc->tdc_current);
}

struct tmpfs_dirent *
tmpfs_dir_next(struct tmpfs_node *dnode, struct tmpfs_dir_cursor *dc)
{
	struct tmpfs_dirent *de;

	MPASS(dc->tdc_tree != NULL);
	if (tmpfs_dirent_dup(dc->tdc_current)) {
		dc->tdc_current = LIST_NEXT(dc->tdc_current, uh.td_dup.entries);
		if (dc->tdc_current != NULL)
			return (dc->tdc_current);
	}
	dc->tdc_tree = dc->tdc_current = RB_NEXT(tmpfs_dir,
	    &dnode->tn_dir.tn_dirhead, dc->tdc_tree);
	if ((de = dc->tdc_current) != NULL && tmpfs_dirent_duphead(de)) {
		dc->tdc_current = LIST_FIRST(&de->ud.td_duphead);
		MPASS(dc->tdc_current != NULL);
	}

	return (dc->tdc_current);
}

/* Lookup directory entry in RB-Tree. Function may return duphead entry. */
static struct tmpfs_dirent *
tmpfs_dir_xlookup_hash(struct tmpfs_node *dnode, uint32_t hash)
{
	struct tmpfs_dirent *de, dekey;

	dekey.td_hash = hash;
	de = RB_FIND(tmpfs_dir, &dnode->tn_dir.tn_dirhead, &dekey);
	return (de);
}

/* Lookup directory entry by cookie, initialize directory cursor accordingly. */
static struct tmpfs_dirent *
tmpfs_dir_lookup_cookie(struct tmpfs_node *node, off_t cookie,
    struct tmpfs_dir_cursor *dc)
{
	struct tmpfs_dir *dirhead = &node->tn_dir.tn_dirhead;
	struct tmpfs_dirent *de, dekey;

	MPASS(cookie >= TMPFS_DIRCOOKIE_MIN);

	if (cookie == node->tn_dir.tn_readdir_lastn &&
	    (de = node->tn_dir.tn_readdir_lastp) != NULL) {
		/* Protect against possible race, tn_readdir_last[pn]
		 * may be updated with only shared vnode lock held. */
		if (cookie == tmpfs_dirent_cookie(de))
			goto out;
	}

	if ((cookie & TMPFS_DIRCOOKIE_DUP) != 0) {
		LIST_FOREACH(de, &node->tn_dir.tn_dupindex,
		    uh.td_dup.index_entries) {
			MPASS(tmpfs_dirent_dup(de));
			if (de->td_cookie == cookie)
				goto out;
			/* dupindex list is sorted. */
			if (de->td_cookie < cookie) {
				de = NULL;
				goto out;
			}
		}
		MPASS(de == NULL);
		goto out;
	}

	if ((cookie & TMPFS_DIRCOOKIE_MASK) != cookie) {
		de = NULL;
	} else {
		dekey.td_hash = cookie;
		/* Recover if direntry for cookie was removed */
		de = RB_NFIND(tmpfs_dir, dirhead, &dekey);
	}
	dc->tdc_tree = de;
	dc->tdc_current = de;
	if (de != NULL && tmpfs_dirent_duphead(de)) {
		dc->tdc_current = LIST_FIRST(&de->ud.td_duphead);
		MPASS(dc->tdc_current != NULL);
	}
	return (dc->tdc_current);

out:
	dc->tdc_tree = de;
	dc->tdc_current = de;
	if (de != NULL && tmpfs_dirent_dup(de))
		dc->tdc_tree = tmpfs_dir_xlookup_hash(node,
		    de->td_hash);
	return (dc->tdc_current);
}

/*
 * Looks for a directory entry in the directory represented by node.
 * 'cnp' describes the name of the entry to look for.  Note that the .
 * and .. components are not allowed as they do not physically exist
 * within directories.
 *
 * Returns a pointer to the entry when found, otherwise NULL.
 */
struct tmpfs_dirent *
tmpfs_dir_lookup(struct tmpfs_node *node, struct tmpfs_node *f,
    struct componentname *cnp)
{
	struct tmpfs_dir_duphead *duphead;
	struct tmpfs_dirent *de;
	uint32_t hash;

	MPASS(IMPLIES(cnp->cn_namelen == 1, cnp->cn_nameptr[0] != '.'));
	MPASS(IMPLIES(cnp->cn_namelen == 2, !(cnp->cn_nameptr[0] == '.' &&
	    cnp->cn_nameptr[1] == '.')));
	TMPFS_VALIDATE_DIR(node);

	hash = tmpfs_dirent_hash(cnp->cn_nameptr, cnp->cn_namelen);
	de = tmpfs_dir_xlookup_hash(node, hash);
	if (de != NULL && tmpfs_dirent_duphead(de)) {
		duphead = &de->ud.td_duphead;
		LIST_FOREACH(de, duphead, uh.td_dup.entries) {
			if (TMPFS_DIRENT_MATCHES(de, cnp->cn_nameptr,
			    cnp->cn_namelen))
				break;
		}
	} else if (de != NULL) {
		if (!TMPFS_DIRENT_MATCHES(de, cnp->cn_nameptr,
		    cnp->cn_namelen))
			de = NULL;
	}
	if (de != NULL && f != NULL && de->td_node != f)
		de = NULL;

	return (de);
}

/*
 * Attach duplicate-cookie directory entry nde to dnode and insert to dupindex
 * list, allocate new cookie value.
 */
static void
tmpfs_dir_attach_dup(struct tmpfs_node *dnode,
    struct tmpfs_dir_duphead *duphead, struct tmpfs_dirent *nde)
{
	struct tmpfs_dir_duphead *dupindex;
	struct tmpfs_dirent *de, *pde;

	dupindex = &dnode->tn_dir.tn_dupindex;
	de = LIST_FIRST(dupindex);
	if (de == NULL || de->td_cookie < TMPFS_DIRCOOKIE_DUP_MAX) {
		if (de == NULL)
			nde->td_cookie = TMPFS_DIRCOOKIE_DUP_MIN;
		else
			nde->td_cookie = de->td_cookie + 1;
		MPASS(tmpfs_dirent_dup(nde));
		LIST_INSERT_HEAD(dupindex, nde, uh.td_dup.index_entries);
		LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
		return;
	}

	/*
	 * Cookie numbers are near exhaustion. Scan dupindex list for unused
	 * numbers. dupindex list is sorted in descending order. Keep it so
	 * after inserting nde.
	 */
	while (1) {
		pde = de;
		de = LIST_NEXT(de, uh.td_dup.index_entries);
		if (de == NULL && pde->td_cookie != TMPFS_DIRCOOKIE_DUP_MIN) {
			/*
			 * Last element of the index doesn't have minimal cookie
			 * value, use it.
			 */
			nde->td_cookie = TMPFS_DIRCOOKIE_DUP_MIN;
			LIST_INSERT_AFTER(pde, nde, uh.td_dup.index_entries);
			LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
			return;
		} else if (de == NULL) {
			/*
			 * We are so lucky have 2^30 hash duplicates in single
			 * directory :) Return largest possible cookie value.
			 * It should be fine except possible issues with
			 * VOP_READDIR restart.
			 */
			nde->td_cookie = TMPFS_DIRCOOKIE_DUP_MAX;
			LIST_INSERT_HEAD(dupindex, nde,
			    uh.td_dup.index_entries);
			LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
			return;
		}
		if (de->td_cookie + 1 == pde->td_cookie ||
		    de->td_cookie >= TMPFS_DIRCOOKIE_DUP_MAX)
			continue;	/* No hole or invalid cookie. */
		nde->td_cookie = de->td_cookie + 1;
		MPASS(tmpfs_dirent_dup(nde));
		MPASS(pde->td_cookie > nde->td_cookie);
		MPASS(nde->td_cookie > de->td_cookie);
		LIST_INSERT_BEFORE(de, nde, uh.td_dup.index_entries);
		LIST_INSERT_HEAD(duphead, nde, uh.td_dup.entries);
		return;
	}
}

/*
 * Attaches the directory entry de to the directory represented by vp.
 * Note that this does not change the link count of the node pointed by
 * the directory entry, as this is done by tmpfs_alloc_dirent.
 */
void
tmpfs_dir_attach(struct vnode *vp, struct tmpfs_dirent *de)
{
	struct tmpfs_node *dnode;
	struct tmpfs_dirent *xde, *nde;

	ASSERT_VOP_ELOCKED(vp, __func__);
	MPASS(de->td_namelen > 0);
	MPASS(de->td_hash >= TMPFS_DIRCOOKIE_MIN);
	MPASS(de->td_cookie == de->td_hash);

	dnode = VP_TO_TMPFS_DIR(vp);
	dnode->tn_dir.tn_readdir_lastn = 0;
	dnode->tn_dir.tn_readdir_lastp = NULL;

	MPASS(!tmpfs_dirent_dup(de));
	xde = RB_INSERT(tmpfs_dir, &dnode->tn_dir.tn_dirhead, de);
	if (xde != NULL && tmpfs_dirent_duphead(xde))
		tmpfs_dir_attach_dup(dnode, &xde->ud.td_duphead, de);
	else if (xde != NULL) {
		/*
		 * Allocate new duphead. Swap xde with duphead to avoid
		 * adding/removing elements with the same hash.
		 */
		MPASS(!tmpfs_dirent_dup(xde));
		tmpfs_alloc_dirent(VFS_TO_TMPFS(vp->v_mount), NULL, NULL, 0,
		    &nde);
		/* *nde = *xde; XXX gcc 4.2.1 may generate invalid code. */
		memcpy(nde, xde, sizeof(*xde));
		xde->td_cookie |= TMPFS_DIRCOOKIE_DUPHEAD;
		LIST_INIT(&xde->ud.td_duphead);
		xde->td_namelen = 0;
		xde->td_node = NULL;
		tmpfs_dir_attach_dup(dnode, &xde->ud.td_duphead, nde);
		tmpfs_dir_attach_dup(dnode, &xde->ud.td_duphead, de);
	}
	dnode->tn_size += sizeof(struct tmpfs_dirent);
	dnode->tn_status |= TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	dnode->tn_accessed = true;
	tmpfs_update(vp);
}

/*
 * Detaches the directory entry de from the directory represented by vp.
 * Note that this does not change the link count of the node pointed by
 * the directory entry, as this is done by tmpfs_free_dirent.
 */
void
tmpfs_dir_detach(struct vnode *vp, struct tmpfs_dirent *de)
{
	struct tmpfs_mount *tmp;
	struct tmpfs_dir *head;
	struct tmpfs_node *dnode;
	struct tmpfs_dirent *xde;

	ASSERT_VOP_ELOCKED(vp, __func__);

	dnode = VP_TO_TMPFS_DIR(vp);
	head = &dnode->tn_dir.tn_dirhead;
	dnode->tn_dir.tn_readdir_lastn = 0;
	dnode->tn_dir.tn_readdir_lastp = NULL;

	if (tmpfs_dirent_dup(de)) {
		/* Remove duphead if de was last entry. */
		if (LIST_NEXT(de, uh.td_dup.entries) == NULL) {
			xde = tmpfs_dir_xlookup_hash(dnode, de->td_hash);
			MPASS(tmpfs_dirent_duphead(xde));
		} else
			xde = NULL;
		LIST_REMOVE(de, uh.td_dup.entries);
		LIST_REMOVE(de, uh.td_dup.index_entries);
		if (xde != NULL) {
			if (LIST_EMPTY(&xde->ud.td_duphead)) {
				RB_REMOVE(tmpfs_dir, head, xde);
				tmp = VFS_TO_TMPFS(vp->v_mount);
				MPASS(xde->td_node == NULL);
				tmpfs_free_dirent(tmp, xde);
			}
		}
		de->td_cookie = de->td_hash;
	} else
		RB_REMOVE(tmpfs_dir, head, de);

	dnode->tn_size -= sizeof(struct tmpfs_dirent);
	dnode->tn_status |= TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	dnode->tn_accessed = true;
	tmpfs_update(vp);
}

void
tmpfs_dir_destroy(struct tmpfs_mount *tmp, struct tmpfs_node *dnode)
{
	struct tmpfs_dirent *de, *dde, *nde;

	RB_FOREACH_SAFE(de, tmpfs_dir, &dnode->tn_dir.tn_dirhead, nde) {
		RB_REMOVE(tmpfs_dir, &dnode->tn_dir.tn_dirhead, de);
		/* Node may already be destroyed. */
		de->td_node = NULL;
		if (tmpfs_dirent_duphead(de)) {
			while ((dde = LIST_FIRST(&de->ud.td_duphead)) != NULL) {
				LIST_REMOVE(dde, uh.td_dup.entries);
				dde->td_node = NULL;
				tmpfs_free_dirent(tmp, dde);
			}
		}
		tmpfs_free_dirent(tmp, de);
	}
}

/*
 * Helper function for tmpfs_readdir.  Creates a '.' entry for the given
 * directory and returns it in the uio space.  The function returns 0
 * on success, -1 if there was not enough space in the uio structure to
 * hold the directory entry or an appropriate error code if another
 * error happens.
 */
static int
tmpfs_dir_getdotdent(struct tmpfs_mount *tm, struct tmpfs_node *node,
    struct uio *uio)
{
	int error;
	struct dirent dent;

	TMPFS_VALIDATE_DIR(node);
	MPASS(uio->uio_offset == TMPFS_DIRCOOKIE_DOT);

	dent.d_fileno = node->tn_id;
	dent.d_off = TMPFS_DIRCOOKIE_DOTDOT;
	dent.d_type = DT_DIR;
	dent.d_namlen = 1;
	dent.d_name[0] = '.';
	dent.d_reclen = GENERIC_DIRSIZ(&dent);
	dirent_terminate(&dent);

	if (dent.d_reclen > uio->uio_resid)
		error = EJUSTRETURN;
	else
		error = uiomove(&dent, dent.d_reclen, uio);

	tmpfs_set_accessed(tm, node);

	return (error);
}

/*
 * Helper function for tmpfs_readdir.  Creates a '..' entry for the given
 * directory and returns it in the uio space.  The function returns 0
 * on success, -1 if there was not enough space in the uio structure to
 * hold the directory entry or an appropriate error code if another
 * error happens.
 */
static int
tmpfs_dir_getdotdotdent(struct tmpfs_mount *tm, struct tmpfs_node *node,
    struct uio *uio, off_t next)
{
	struct tmpfs_node *parent;
	struct dirent dent;
	int error;

	TMPFS_VALIDATE_DIR(node);
	MPASS(uio->uio_offset == TMPFS_DIRCOOKIE_DOTDOT);

	/*
	 * Return ENOENT if the current node is already removed.
	 */
	TMPFS_ASSERT_LOCKED(node);
	parent = node->tn_dir.tn_parent;
	if (parent == NULL)
		return (ENOENT);

	dent.d_fileno = parent->tn_id;
	dent.d_off = next;
	dent.d_type = DT_DIR;
	dent.d_namlen = 2;
	dent.d_name[0] = '.';
	dent.d_name[1] = '.';
	dent.d_reclen = GENERIC_DIRSIZ(&dent);
	dirent_terminate(&dent);

	if (dent.d_reclen > uio->uio_resid)
		error = EJUSTRETURN;
	else
		error = uiomove(&dent, dent.d_reclen, uio);

	tmpfs_set_accessed(tm, node);

	return (error);
}

/*
 * Helper function for tmpfs_readdir.  Returns as much directory entries
 * as can fit in the uio space.  The read starts at uio->uio_offset.
 * The function returns 0 on success, -1 if there was not enough space
 * in the uio structure to hold the directory entry or an appropriate
 * error code if another error happens.
 */
int
tmpfs_dir_getdents(struct tmpfs_mount *tm, struct tmpfs_node *node,
    struct uio *uio, int maxcookies, uint64_t *cookies, int *ncookies)
{
	struct tmpfs_dir_cursor dc;
	struct tmpfs_dirent *de, *nde;
	off_t off;
	int error;

	TMPFS_VALIDATE_DIR(node);

	off = 0;

	/*
	 * Lookup the node from the current offset.  The starting offset of
	 * 0 will lookup both '.' and '..', and then the first real entry,
	 * or EOF if there are none.  Then find all entries for the dir that
	 * fit into the buffer.  Once no more entries are found (de == NULL),
	 * the offset is set to TMPFS_DIRCOOKIE_EOF, which will cause the next
	 * call to return 0.
	 */
	switch (uio->uio_offset) {
	case TMPFS_DIRCOOKIE_DOT:
		error = tmpfs_dir_getdotdent(tm, node, uio);
		if (error != 0)
			return (error);
		uio->uio_offset = off = TMPFS_DIRCOOKIE_DOTDOT;
		if (cookies != NULL)
			cookies[(*ncookies)++] = off;
		/* FALLTHROUGH */
	case TMPFS_DIRCOOKIE_DOTDOT:
		de = tmpfs_dir_first(node, &dc);
		off = tmpfs_dirent_cookie(de);
		error = tmpfs_dir_getdotdotdent(tm, node, uio, off);
		if (error != 0)
			return (error);
		uio->uio_offset = off;
		if (cookies != NULL)
			cookies[(*ncookies)++] = off;
		/* EOF. */
		if (de == NULL)
			return (0);
		break;
	case TMPFS_DIRCOOKIE_EOF:
		return (0);
	default:
		de = tmpfs_dir_lookup_cookie(node, uio->uio_offset, &dc);
		if (de == NULL)
			return (EINVAL);
		if (cookies != NULL)
			off = tmpfs_dirent_cookie(de);
	}

	/*
	 * Read as much entries as possible; i.e., until we reach the end of the
	 * directory or we exhaust uio space.
	 */
	do {
		struct dirent d;

		/*
		 * Create a dirent structure representing the current tmpfs_node
		 * and fill it.
		 */
		if (de->td_node == NULL) {
			d.d_fileno = 1;
			d.d_type = DT_WHT;
		} else {
			d.d_fileno = de->td_node->tn_id;
			switch (de->td_node->tn_type) {
			case VBLK:
				d.d_type = DT_BLK;
				break;

			case VCHR:
				d.d_type = DT_CHR;
				break;

			case VDIR:
				d.d_type = DT_DIR;
				break;

			case VFIFO:
				d.d_type = DT_FIFO;
				break;

			case VLNK:
				d.d_type = DT_LNK;
				break;

			case VREG:
				d.d_type = DT_REG;
				break;

			case VSOCK:
				d.d_type = DT_SOCK;
				break;

			default:
				panic("tmpfs_dir_getdents: type %p %d",
				    de->td_node, (int)de->td_node->tn_type);
			}
		}
		d.d_namlen = de->td_namelen;
		MPASS(de->td_namelen < sizeof(d.d_name));
		(void)memcpy(d.d_name, de->ud.td_name, de->td_namelen);
		d.d_reclen = GENERIC_DIRSIZ(&d);

		/*
		 * Stop reading if the directory entry we are treating is bigger
		 * than the amount of data that can be returned.
		 */
		if (d.d_reclen > uio->uio_resid) {
			error = EJUSTRETURN;
			break;
		}

		nde = tmpfs_dir_next(node, &dc);
		d.d_off = tmpfs_dirent_cookie(nde);
		dirent_terminate(&d);

		/*
		 * Copy the new dirent structure into the output buffer and
		 * advance pointers.
		 */
		error = uiomove(&d, d.d_reclen, uio);
		if (error == 0) {
			de = nde;
			if (cookies != NULL) {
				off = tmpfs_dirent_cookie(de);
				MPASS(*ncookies < maxcookies);
				cookies[(*ncookies)++] = off;
			}
		}
	} while (error == 0 && uio->uio_resid > 0 && de != NULL);

	/* Skip setting off when using cookies as it is already done above. */
	if (cookies == NULL)
		off = tmpfs_dirent_cookie(de);

	/* Update the offset and cache. */
	uio->uio_offset = off;
	node->tn_dir.tn_readdir_lastn = off;
	node->tn_dir.tn_readdir_lastp = de;

	tmpfs_set_accessed(tm, node);
	return (error);
}

int
tmpfs_dir_whiteout_add(struct vnode *dvp, struct componentname *cnp)
{
	struct tmpfs_dirent *de;
	struct tmpfs_node *dnode;
	int error;

	error = tmpfs_alloc_dirent(VFS_TO_TMPFS(dvp->v_mount), NULL,
	    cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error != 0)
		return (error);
	dnode = VP_TO_TMPFS_DIR(dvp);
	tmpfs_dir_attach(dvp, de);
	dnode->tn_dir.tn_wht_size += sizeof(*de);
	return (0);
}

void
tmpfs_dir_whiteout_remove(struct vnode *dvp, struct componentname *cnp)
{
	struct tmpfs_dirent *de;
	struct tmpfs_node *dnode;

	dnode = VP_TO_TMPFS_DIR(dvp);
	de = tmpfs_dir_lookup(dnode, NULL, cnp);
	MPASS(de != NULL && de->td_node == NULL);
	MPASS(dnode->tn_dir.tn_wht_size >= sizeof(*de));
	dnode->tn_dir.tn_wht_size -= sizeof(*de);
	tmpfs_dir_detach(dvp, de);
	tmpfs_free_dirent(VFS_TO_TMPFS(dvp->v_mount), de);
}

/*
 * Frees any dirents still associated with the directory represented
 * by dvp in preparation for the removal of the directory.  This is
 * required when removing a directory which contains only whiteout
 * entries.
 */
void
tmpfs_dir_clear_whiteouts(struct vnode *dvp)
{
	struct tmpfs_dir_cursor dc;
	struct tmpfs_dirent *de;
	struct tmpfs_node *dnode;

	dnode = VP_TO_TMPFS_DIR(dvp);

	while ((de = tmpfs_dir_first(dnode, &dc)) != NULL) {
		KASSERT(de->td_node == NULL, ("%s: non-whiteout dirent %p",
		    __func__, de));
		dnode->tn_dir.tn_wht_size -= sizeof(*de);
		tmpfs_dir_detach(dvp, de);
		tmpfs_free_dirent(VFS_TO_TMPFS(dvp->v_mount), de);
	}
	MPASS(dnode->tn_size == 0);
	MPASS(dnode->tn_dir.tn_wht_size == 0);
}

/*
 * Resizes the aobj associated with the regular file pointed to by 'vp' to the
 * size 'newsize'.  'vp' must point to a vnode that represents a regular file.
 * 'newsize' must be positive.
 *
 * Returns zero on success or an appropriate error code on failure.
 */
int
tmpfs_reg_resize(struct vnode *vp, off_t newsize, boolean_t ignerr)
{
	struct tmpfs_node *node;
	vm_object_t uobj;
	vm_pindex_t idx, newpages, oldpages;
	off_t oldsize;
	int base, error;

	MPASS(vp->v_type == VREG);
	MPASS(newsize >= 0);

	node = VP_TO_TMPFS_NODE(vp);
	uobj = node->tn_reg.tn_aobj;

	/*
	 * Convert the old and new sizes to the number of pages needed to
	 * store them.  It may happen that we do not need to do anything
	 * because the last allocated page can accommodate the change on
	 * its own.
	 */
	oldsize = node->tn_size;
	oldpages = OFF_TO_IDX(oldsize + PAGE_MASK);
	MPASS(oldpages == uobj->size);
	newpages = OFF_TO_IDX(newsize + PAGE_MASK);

	if (__predict_true(newpages == oldpages && newsize >= oldsize)) {
		node->tn_size = newsize;
		return (0);
	}

	VM_OBJECT_WLOCK(uobj);
	if (newsize < oldsize) {
		/*
		 * Zero the truncated part of the last page.
		 */
		base = newsize & PAGE_MASK;
		if (base != 0) {
			idx = OFF_TO_IDX(newsize);
			error = tmpfs_partial_page_invalidate(uobj, idx, base,
			    PAGE_SIZE, ignerr);
			if (error != 0) {
				VM_OBJECT_WUNLOCK(uobj);
				return (error);
			}
		}

		/*
		 * Release any swap space and free any whole pages.
		 */
		if (newpages < oldpages)
			vm_object_page_remove(uobj, newpages, 0, 0);
	}
	uobj->size = newpages;
	VM_OBJECT_WUNLOCK(uobj);

	node->tn_size = newsize;
	return (0);
}

/*
 * Punch hole in the aobj associated with the regular file pointed to by 'vp'.
 * Requests completely beyond the end-of-file are converted to no-op.
 *
 * Returns 0 on success or error code from tmpfs_partial_page_invalidate() on
 * failure.
 */
int
tmpfs_reg_punch_hole(struct vnode *vp, off_t *offset, off_t *length)
{
	struct tmpfs_node *node;
	vm_object_t object;
	vm_pindex_t pistart, pi, piend;
	int startofs, endofs, end;
	off_t off, len;
	int error;

	KASSERT(*length <= OFF_MAX - *offset, ("%s: offset + length overflows",
	    __func__));
	node = VP_TO_TMPFS_NODE(vp);
	KASSERT(node->tn_type == VREG, ("%s: node is not regular file",
	    __func__));
	object = node->tn_reg.tn_aobj;
	off = *offset;
	len = omin(node->tn_size - off, *length);
	startofs = off & PAGE_MASK;
	endofs = (off + len) & PAGE_MASK;
	pistart = OFF_TO_IDX(off);
	piend = OFF_TO_IDX(off + len);
	pi = OFF_TO_IDX((vm_ooffset_t)off + PAGE_MASK);
	error = 0;

	/* Handle the case when offset is on or beyond file size. */
	if (len <= 0) {
		*length = 0;
		return (0);
	}

	VM_OBJECT_WLOCK(object);

	/*
	 * If there is a partial page at the beginning of the hole-punching
	 * request, fill the partial page with zeroes.
	 */
	if (startofs != 0) {
		end = pistart != piend ? PAGE_SIZE : endofs;
		error = tmpfs_partial_page_invalidate(object, pistart, startofs,
		    end, FALSE);
		if (error != 0)
			goto out;
		off += end - startofs;
		len -= end - startofs;
	}

	/*
	 * Toss away the full pages in the affected area.
	 */
	if (pi < piend) {
		vm_object_page_remove(object, pi, piend, 0);
		off += IDX_TO_OFF(piend - pi);
		len -= IDX_TO_OFF(piend - pi);
	}

	/*
	 * If there is a partial page at the end of the hole-punching request,
	 * fill the partial page with zeroes.
	 */
	if (endofs != 0 && pistart != piend) {
		error = tmpfs_partial_page_invalidate(object, piend, 0, endofs,
		    FALSE);
		if (error != 0)
			goto out;
		off += endofs;
		len -= endofs;
	}

out:
	VM_OBJECT_WUNLOCK(object);
	*offset = off;
	*length = len;
	return (error);
}

void
tmpfs_check_mtime(struct vnode *vp)
{
	struct tmpfs_node *node;
	struct vm_object *obj;

	ASSERT_VOP_ELOCKED(vp, "check_mtime");
	if (vp->v_type != VREG)
		return;
	obj = vp->v_object;
	KASSERT(obj->type == tmpfs_pager_type &&
	    (obj->flags & (OBJ_SWAP | OBJ_TMPFS)) ==
	    (OBJ_SWAP | OBJ_TMPFS), ("non-tmpfs obj"));
	/* unlocked read */
	if (obj->generation != obj->cleangeneration) {
		VM_OBJECT_WLOCK(obj);
		if (obj->generation != obj->cleangeneration) {
			obj->cleangeneration = obj->generation;
			node = VP_TO_TMPFS_NODE(vp);
			node->tn_status |= TMPFS_NODE_MODIFIED |
			    TMPFS_NODE_CHANGED;
		}
		VM_OBJECT_WUNLOCK(obj);
	}
}

/*
 * Change flags of the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chflags(struct vnode *vp, u_long flags, struct ucred *cred,
    struct thread *td)
{
	int error;
	struct tmpfs_node *node;

	ASSERT_VOP_ELOCKED(vp, "chflags");

	node = VP_TO_TMPFS_NODE(vp);

	if ((flags & ~(SF_APPEND | SF_ARCHIVED | SF_IMMUTABLE | SF_NOUNLINK |
	    UF_APPEND | UF_ARCHIVE | UF_HIDDEN | UF_IMMUTABLE | UF_NODUMP |
	    UF_NOUNLINK | UF_OFFLINE | UF_OPAQUE | UF_READONLY | UF_REPARSE |
	    UF_SPARSE | UF_SYSTEM)) != 0)
		return (EOPNOTSUPP);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/*
	 * Callers may only modify the file flags on objects they
	 * have VADMIN rights for.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);
	/*
	 * Unprivileged processes are not permitted to unset system
	 * flags, or modify flags if any system flags are set.
	 */
	if (!priv_check_cred(cred, PRIV_VFS_SYSFLAGS)) {
		if (node->tn_flags &
		    (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND)) {
			error = securelevel_gt(cred, 0);
			if (error)
				return (error);
		}
	} else {
		if (node->tn_flags &
		    (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND) ||
		    ((flags ^ node->tn_flags) & SF_SETTABLE))
			return (EPERM);
	}
	node->tn_flags = flags;
	node->tn_status |= TMPFS_NODE_CHANGED;

	ASSERT_VOP_ELOCKED(vp, "chflags2");

	return (0);
}

/*
 * Change access mode on the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chmod(struct vnode *vp, mode_t mode, struct ucred *cred,
    struct thread *td)
{
	int error;
	struct tmpfs_node *node;
	mode_t newmode;

	ASSERT_VOP_ELOCKED(vp, "chmod");
	ASSERT_VOP_IN_SEQC(vp);

	node = VP_TO_TMPFS_NODE(vp);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return (EPERM);

	/*
	 * To modify the permissions on a file, must possess VADMIN
	 * for that file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);

	/*
	 * Privileged processes may set the sticky bit on non-directories,
	 * as well as set the setgid bit on a file with a group that the
	 * process is not a member of.
	 */
	if (vp->v_type != VDIR && (mode & S_ISTXT)) {
		if (priv_check_cred(cred, PRIV_VFS_STICKYFILE))
			return (EFTYPE);
	}
	if (!groupmember(node->tn_gid, cred) && (mode & S_ISGID)) {
		error = priv_check_cred(cred, PRIV_VFS_SETGID);
		if (error)
			return (error);
	}

	newmode = node->tn_mode & ~ALLPERMS;
	newmode |= mode & ALLPERMS;
	atomic_store_short(&node->tn_mode, newmode);

	node->tn_status |= TMPFS_NODE_CHANGED;

	ASSERT_VOP_ELOCKED(vp, "chmod2");

	return (0);
}

/*
 * Change ownership of the given vnode.  At least one of uid or gid must
 * be different than VNOVAL.  If one is set to that value, the attribute
 * is unchanged.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chown(struct vnode *vp, uid_t uid, gid_t gid, struct ucred *cred,
    struct thread *td)
{
	int error;
	struct tmpfs_node *node;
	uid_t ouid;
	gid_t ogid;
	mode_t newmode;

	ASSERT_VOP_ELOCKED(vp, "chown");
	ASSERT_VOP_IN_SEQC(vp);

	node = VP_TO_TMPFS_NODE(vp);

	/* Assign default values if they are unknown. */
	MPASS(uid != VNOVAL || gid != VNOVAL);
	if (uid == VNOVAL)
		uid = node->tn_uid;
	if (gid == VNOVAL)
		gid = node->tn_gid;
	MPASS(uid != VNOVAL && gid != VNOVAL);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return (EPERM);

	/*
	 * To modify the ownership of a file, must possess VADMIN for that
	 * file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);

	/*
	 * To change the owner of a file, or change the group of a file to a
	 * group of which we are not a member, the caller must have
	 * privilege.
	 */
	if ((uid != node->tn_uid ||
	    (gid != node->tn_gid && !groupmember(gid, cred))) &&
	    (error = priv_check_cred(cred, PRIV_VFS_CHOWN)))
		return (error);

	ogid = node->tn_gid;
	ouid = node->tn_uid;

	node->tn_uid = uid;
	node->tn_gid = gid;

	node->tn_status |= TMPFS_NODE_CHANGED;

	if ((node->tn_mode & (S_ISUID | S_ISGID)) != 0 &&
	    (ouid != uid || ogid != gid)) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID)) {
			newmode = node->tn_mode & ~(S_ISUID | S_ISGID);
			atomic_store_short(&node->tn_mode, newmode);
		}
	}

	ASSERT_VOP_ELOCKED(vp, "chown2");

	return (0);
}

/*
 * Change size of the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chsize(struct vnode *vp, u_quad_t size, struct ucred *cred,
    struct thread *td)
{
	int error;
	struct tmpfs_node *node;

	ASSERT_VOP_ELOCKED(vp, "chsize");

	node = VP_TO_TMPFS_NODE(vp);

	/* Decide whether this is a valid operation based on the file type. */
	error = 0;
	switch (vp->v_type) {
	case VDIR:
		return (EISDIR);

	case VREG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		break;

	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VFIFO:
		/*
		 * Allow modifications of special files even if in the file
		 * system is mounted read-only (we are not modifying the
		 * files themselves, but the objects they represent).
		 */
		return (0);

	default:
		/* Anything else is unsupported. */
		return (EOPNOTSUPP);
	}

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return (EPERM);

	error = vn_rlimit_trunc(size, td);
	if (error != 0)
		return (error);

	error = tmpfs_truncate(vp, size);
	/*
	 * tmpfs_truncate will raise the NOTE_EXTEND and NOTE_ATTRIB kevents
	 * for us, as will update tn_status; no need to do that here.
	 */

	ASSERT_VOP_ELOCKED(vp, "chsize2");

	return (error);
}

/*
 * Change access and modification times of the given vnode.
 * Caller should execute tmpfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
tmpfs_chtimes(struct vnode *vp, struct vattr *vap,
    struct ucred *cred, struct thread *td)
{
	int error;
	struct tmpfs_node *node;

	ASSERT_VOP_ELOCKED(vp, "chtimes");

	node = VP_TO_TMPFS_NODE(vp);

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return (EPERM);

	error = vn_utimes_perm(vp, vap, cred, td);
	if (error != 0)
		return (error);

	if (vap->va_atime.tv_sec != VNOVAL)
		node->tn_accessed = true;
	if (vap->va_mtime.tv_sec != VNOVAL)
		node->tn_status |= TMPFS_NODE_MODIFIED;
	if (vap->va_birthtime.tv_sec != VNOVAL)
		node->tn_status |= TMPFS_NODE_MODIFIED;
	tmpfs_itimes(vp, &vap->va_atime, &vap->va_mtime);
	if (vap->va_birthtime.tv_sec != VNOVAL)
		node->tn_birthtime = vap->va_birthtime;
	ASSERT_VOP_ELOCKED(vp, "chtimes2");

	return (0);
}

void
tmpfs_set_status(struct tmpfs_mount *tm, struct tmpfs_node *node, int status)
{

	if ((node->tn_status & status) == status || tm->tm_ronly)
		return;
	TMPFS_NODE_LOCK(node);
	node->tn_status |= status;
	TMPFS_NODE_UNLOCK(node);
}

void
tmpfs_set_accessed(struct tmpfs_mount *tm, struct tmpfs_node *node)
{
	if (node->tn_accessed || tm->tm_ronly)
		return;
	atomic_store_8(&node->tn_accessed, true);
}

/* Sync timestamps */
void
tmpfs_itimes(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod)
{
	struct tmpfs_node *node;
	struct timespec now;

	ASSERT_VOP_LOCKED(vp, "tmpfs_itimes");
	node = VP_TO_TMPFS_NODE(vp);

	if (!node->tn_accessed &&
	    (node->tn_status & (TMPFS_NODE_MODIFIED | TMPFS_NODE_CHANGED)) == 0)
		return;

	vfs_timestamp(&now);
	TMPFS_NODE_LOCK(node);
	if (node->tn_accessed) {
		if (acc == NULL)
			 acc = &now;
		node->tn_atime = *acc;
	}
	if (node->tn_status & TMPFS_NODE_MODIFIED) {
		if (mod == NULL)
			mod = &now;
		node->tn_mtime = *mod;
	}
	if (node->tn_status & TMPFS_NODE_CHANGED)
		node->tn_ctime = now;
	node->tn_status &= ~(TMPFS_NODE_MODIFIED | TMPFS_NODE_CHANGED);
	node->tn_accessed = false;
	TMPFS_NODE_UNLOCK(node);

	/* XXX: FIX? The entropy here is desirable, but the harvesting may be expensive */
	random_harvest_queue(node, sizeof(*node), RANDOM_FS_ATIME);
}

int
tmpfs_truncate(struct vnode *vp, off_t length)
{
	struct tmpfs_node *node;
	int error;

	if (length < 0)
		return (EINVAL);
	if (length > VFS_TO_TMPFS(vp->v_mount)->tm_maxfilesize)
		return (EFBIG);

	node = VP_TO_TMPFS_NODE(vp);
	error = node->tn_size == length ? 0 : tmpfs_reg_resize(vp, length,
	    FALSE);
	if (error == 0)
		node->tn_status |= TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	tmpfs_update(vp);

	return (error);
}

static __inline int
tmpfs_dirtree_cmp(struct tmpfs_dirent *a, struct tmpfs_dirent *b)
{
	if (a->td_hash > b->td_hash)
		return (1);
	else if (a->td_hash < b->td_hash)
		return (-1);
	return (0);
}

RB_GENERATE_STATIC(tmpfs_dir, tmpfs_dirent, uh.td_entries, tmpfs_dirtree_cmp);
