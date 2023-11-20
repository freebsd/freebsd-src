/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2001 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
#include "opt_pseudofs.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/pseudofs/pseudofs_internal.h>

static MALLOC_DEFINE(M_PFSVNCACHE, "pfs_vncache", "pseudofs vnode cache");

static struct mtx pfs_vncache_mutex;
static eventhandler_tag pfs_exit_tag;
static void pfs_exit(void *arg, struct proc *p);
static void pfs_purge_all(void);

static SYSCTL_NODE(_vfs_pfs, OID_AUTO, vncache, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "pseudofs vnode cache");

static int pfs_vncache_entries;
SYSCTL_INT(_vfs_pfs_vncache, OID_AUTO, entries, CTLFLAG_RD,
    &pfs_vncache_entries, 0,
    "number of entries in the vnode cache");

static int pfs_vncache_maxentries;
SYSCTL_INT(_vfs_pfs_vncache, OID_AUTO, maxentries, CTLFLAG_RD,
    &pfs_vncache_maxentries, 0,
    "highest number of entries in the vnode cache");

static int pfs_vncache_hits;
SYSCTL_INT(_vfs_pfs_vncache, OID_AUTO, hits, CTLFLAG_RD,
    &pfs_vncache_hits, 0,
    "number of cache hits since initialization");

static int pfs_vncache_misses;
SYSCTL_INT(_vfs_pfs_vncache, OID_AUTO, misses, CTLFLAG_RD,
    &pfs_vncache_misses, 0,
    "number of cache misses since initialization");

extern struct vop_vector pfs_vnodeops;	/* XXX -> .h file */

static SLIST_HEAD(pfs_vncache_head, pfs_vdata) *pfs_vncache_hashtbl;
static u_long pfs_vncache_hash;
#define PFS_VNCACHE_HASH(pid)	(&pfs_vncache_hashtbl[(pid) & pfs_vncache_hash])

/*
 * Initialize vnode cache
 */
void
pfs_vncache_load(void)
{

	mtx_init(&pfs_vncache_mutex, "pfs_vncache", NULL, MTX_DEF);
	pfs_vncache_hashtbl = hashinit(maxproc / 4, M_PFSVNCACHE, &pfs_vncache_hash);
	pfs_exit_tag = EVENTHANDLER_REGISTER(process_exit, pfs_exit, NULL,
	    EVENTHANDLER_PRI_ANY);
}

/*
 * Tear down vnode cache
 */
void
pfs_vncache_unload(void)
{

	EVENTHANDLER_DEREGISTER(process_exit, pfs_exit_tag);
	pfs_purge_all();
	KASSERT(pfs_vncache_entries == 0,
	    ("%d vncache entries remaining", pfs_vncache_entries));
	mtx_destroy(&pfs_vncache_mutex);
	hashdestroy(pfs_vncache_hashtbl, M_PFSVNCACHE, pfs_vncache_hash);
}

/*
 * Allocate a vnode
 */
int
pfs_vncache_alloc(struct mount *mp, struct vnode **vpp,
		  struct pfs_node *pn, pid_t pid)
{
	struct pfs_vncache_head *hash;
	struct pfs_vdata *pvd, *pvd2;
	struct vnode *vp;
	enum vgetstate vs;
	int error;

	/*
	 * See if the vnode is in the cache.
	 */
	hash = PFS_VNCACHE_HASH(pid);
	if (SLIST_EMPTY(hash))
		goto alloc;
retry:
	mtx_lock(&pfs_vncache_mutex);
	SLIST_FOREACH(pvd, hash, pvd_hash) {
		if (pvd->pvd_pn == pn && pvd->pvd_pid == pid &&
		    pvd->pvd_vnode->v_mount == mp) {
			vp = pvd->pvd_vnode;
			vs = vget_prep(vp);
			mtx_unlock(&pfs_vncache_mutex);
			if (vget_finish(vp, LK_EXCLUSIVE, vs) == 0) {
				++pfs_vncache_hits;
				*vpp = vp;
				/*
				 * Some callers cache_enter(vp) later, so
				 * we have to make sure it's not in the
				 * VFS cache so it doesn't get entered
				 * twice.  A better solution would be to
				 * make pfs_vncache_alloc() responsible
				 * for entering the vnode in the VFS
				 * cache.
				 */
				cache_purge(vp);
				return (0);
			}
			goto retry;
		}
	}
	mtx_unlock(&pfs_vncache_mutex);
alloc:
	/* nope, get a new one */
	pvd = malloc(sizeof *pvd, M_PFSVNCACHE, M_WAITOK);
	error = getnewvnode("pseudofs", mp, &pfs_vnodeops, vpp);
	if (error) {
		free(pvd, M_PFSVNCACHE);
		return (error);
	}
	pvd->pvd_pn = pn;
	pvd->pvd_pid = pid;
	(*vpp)->v_data = pvd;
	switch (pn->pn_type) {
	case pfstype_root:
		(*vpp)->v_vflag = VV_ROOT;
#if 0
		printf("root vnode allocated\n");
#endif
		/* fall through */
	case pfstype_dir:
	case pfstype_this:
	case pfstype_parent:
	case pfstype_procdir:
		(*vpp)->v_type = VDIR;
		break;
	case pfstype_file:
		(*vpp)->v_type = VREG;
		break;
	case pfstype_symlink:
		(*vpp)->v_type = VLNK;
		break;
	case pfstype_none:
		KASSERT(0, ("pfs_vncache_alloc called for null node\n"));
	default:
		panic("%s has unexpected type: %d", pn->pn_name, pn->pn_type);
	}
	/*
	 * Propagate flag through to vnode so users know it can change
	 * if the process changes (i.e. execve)
	 */
	if ((pn->pn_flags & PFS_PROCDEP) != 0)
		(*vpp)->v_vflag |= VV_PROCDEP;
	pvd->pvd_vnode = *vpp;
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	VN_LOCK_AREC(*vpp);
	error = insmntque(*vpp, mp);
	if (error != 0) {
		free(pvd, M_PFSVNCACHE);
		*vpp = NULLVP;
		return (error);
	}
	vn_set_state(*vpp, VSTATE_CONSTRUCTED);
retry2:
	mtx_lock(&pfs_vncache_mutex);
	/*
	 * Other thread may race with us, creating the entry we are
	 * going to insert into the cache. Recheck after
	 * pfs_vncache_mutex is reacquired.
	 */
	SLIST_FOREACH(pvd2, hash, pvd_hash) {
		if (pvd2->pvd_pn == pn && pvd2->pvd_pid == pid &&
		    pvd2->pvd_vnode->v_mount == mp) {
			vp = pvd2->pvd_vnode;
			vs = vget_prep(vp);
			mtx_unlock(&pfs_vncache_mutex);
			if (vget_finish(vp, LK_EXCLUSIVE, vs) == 0) {
				++pfs_vncache_hits;
				vgone(*vpp);
				vput(*vpp);
				*vpp = vp;
				cache_purge(vp);
				return (0);
			}
			goto retry2;
		}
	}
	++pfs_vncache_misses;
	if (++pfs_vncache_entries > pfs_vncache_maxentries)
		pfs_vncache_maxentries = pfs_vncache_entries;
	SLIST_INSERT_HEAD(hash, pvd, pvd_hash);
	mtx_unlock(&pfs_vncache_mutex);
	return (0);
}

/*
 * Free a vnode
 */
int
pfs_vncache_free(struct vnode *vp)
{
	struct pfs_vdata *pvd, *pvd2;

	mtx_lock(&pfs_vncache_mutex);
	pvd = (struct pfs_vdata *)vp->v_data;
	KASSERT(pvd != NULL, ("pfs_vncache_free(): no vnode data\n"));
	SLIST_FOREACH(pvd2, PFS_VNCACHE_HASH(pvd->pvd_pid), pvd_hash) {
		if (pvd2 != pvd)
			continue;
		SLIST_REMOVE(PFS_VNCACHE_HASH(pvd->pvd_pid), pvd, pfs_vdata, pvd_hash);
		--pfs_vncache_entries;
		break;
	}
	mtx_unlock(&pfs_vncache_mutex);

	free(pvd, M_PFSVNCACHE);
	vp->v_data = NULL;
	return (0);
}

/*
 * Purge the cache of dead entries
 *
 * The code is not very efficient and this perhaps can be addressed without
 * a complete rewrite. Previous iteration was walking a linked list from
 * scratch every time. This code only walks the relevant hash chain (if pid
 * is provided), but still resorts to scanning the entire cache at least twice
 * if a specific component is to be removed which is slower. This can be
 * augmented with resizing the hash.
 *
 * Explanation of the previous state:
 *
 * This is extremely inefficient due to the fact that vgone() not only
 * indirectly modifies the vnode cache, but may also sleep.  We can
 * neither hold pfs_vncache_mutex across a vgone() call, nor make any
 * assumptions about the state of the cache after vgone() returns.  In
 * consequence, we must start over after every vgone() call, and keep
 * trying until we manage to traverse the entire cache.
 *
 * The only way to improve this situation is to change the data structure
 * used to implement the cache.
 */

static void
pfs_purge_one(struct vnode *vnp)
{

	VOP_LOCK(vnp, LK_EXCLUSIVE);
	vgone(vnp);
	VOP_UNLOCK(vnp);
	vdrop(vnp);
}

void
pfs_purge(struct pfs_node *pn)
{
	struct pfs_vdata *pvd;
	struct vnode *vnp;
	u_long i, removed;

	mtx_lock(&pfs_vncache_mutex);
restart:
	removed = 0;
	for (i = 0; i <= pfs_vncache_hash; i++) {
restart_chain:
		SLIST_FOREACH(pvd, &pfs_vncache_hashtbl[i], pvd_hash) {
			if (pn != NULL && pvd->pvd_pn != pn)
				continue;
			vnp = pvd->pvd_vnode;
			vhold(vnp);
			mtx_unlock(&pfs_vncache_mutex);
			pfs_purge_one(vnp);
			removed++;
			mtx_lock(&pfs_vncache_mutex);
			goto restart_chain;
		}
	}
	if (removed > 0)
		goto restart;
	mtx_unlock(&pfs_vncache_mutex);
}

static void
pfs_purge_all(void)
{

	pfs_purge(NULL);
}

/*
 * Free all vnodes associated with a defunct process
 */
static void
pfs_exit(void *arg, struct proc *p)
{
	struct pfs_vncache_head *hash;
	struct pfs_vdata *pvd;
	struct vnode *vnp;
	int pid;

	pid = p->p_pid;
	hash = PFS_VNCACHE_HASH(pid);
	if (SLIST_EMPTY(hash))
		return;
restart:
	mtx_lock(&pfs_vncache_mutex);
	SLIST_FOREACH(pvd, hash, pvd_hash) {
		if (pvd->pvd_pid != pid)
			continue;
		vnp = pvd->pvd_vnode;
		vhold(vnp);
		mtx_unlock(&pfs_vncache_mutex);
		pfs_purge_one(vnp);
		goto restart;
	}
	mtx_unlock(&pfs_vncache_mutex);
}
