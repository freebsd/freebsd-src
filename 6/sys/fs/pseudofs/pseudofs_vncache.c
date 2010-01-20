/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
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
__FBSDID("$FreeBSD$");

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
static struct pfs_vdata *pfs_vncache;
static eventhandler_tag pfs_exit_tag;
static void pfs_exit(void *arg, struct proc *p);

SYSCTL_NODE(_vfs_pfs, OID_AUTO, vncache, CTLFLAG_RW, 0,
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

/*
 * Initialize vnode cache
 */
void
pfs_vncache_load(void)
{

	mtx_assert(&Giant, MA_OWNED);
	mtx_init(&pfs_vncache_mutex, "pfs_vncache", NULL, MTX_DEF);
	pfs_exit_tag = EVENTHANDLER_REGISTER(process_exit, pfs_exit, NULL,
	    EVENTHANDLER_PRI_ANY);
}

/*
 * Tear down vnode cache
 */
void
pfs_vncache_unload(void)
{

	mtx_assert(&Giant, MA_OWNED);
	EVENTHANDLER_DEREGISTER(process_exit, pfs_exit_tag);
	KASSERT(pfs_vncache_entries == 0,
	    ("%d vncache entries remaining", pfs_vncache_entries));
	mtx_destroy(&pfs_vncache_mutex);
}

/*
 * Allocate a vnode
 */
int
pfs_vncache_alloc(struct mount *mp, struct vnode **vpp,
		  struct pfs_node *pn, pid_t pid)
{
	struct pfs_vdata *pvd;
	struct vnode *vp;
	int error;

	/*
	 * See if the vnode is in the cache.
	 * XXX linear search is not very efficient.
	 */
retry:
	mtx_lock(&pfs_vncache_mutex);
	for (pvd = pfs_vncache; pvd; pvd = pvd->pvd_next) {
		if (pvd->pvd_pn == pn && pvd->pvd_pid == pid &&
		    pvd->pvd_vnode->v_mount == mp) {
			vp = pvd->pvd_vnode;
			VI_LOCK(vp);
			mtx_unlock(&pfs_vncache_mutex);
			if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, curthread) == 0) {
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
	++pfs_vncache_misses;

	/* nope, get a new one */
	MALLOC(pvd, struct pfs_vdata *, sizeof *pvd, M_PFSVNCACHE, M_WAITOK);
	if (++pfs_vncache_entries > pfs_vncache_maxentries)
		pfs_vncache_maxentries = pfs_vncache_entries;
	error = getnewvnode("pseudofs", mp, &pfs_vnodeops, vpp);
	if (error) {
		FREE(pvd, M_PFSVNCACHE);
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
	mtx_lock(&pfs_vncache_mutex);
	pvd->pvd_prev = NULL;
	pvd->pvd_next = pfs_vncache;
	if (pvd->pvd_next)
		pvd->pvd_next->pvd_prev = pvd;
	pfs_vncache = pvd;
	mtx_unlock(&pfs_vncache_mutex);
        (*vpp)->v_vnlock->lk_flags |= LK_CANRECURSE;
	vn_lock(*vpp, LK_RETRY | LK_EXCLUSIVE, curthread);
	return (0);
}

/*
 * Free a vnode
 */
int
pfs_vncache_free(struct vnode *vp)
{
	struct pfs_vdata *pvd;

	mtx_lock(&pfs_vncache_mutex);
	pvd = (struct pfs_vdata *)vp->v_data;
	KASSERT(pvd != NULL, ("pfs_vncache_free(): no vnode data\n"));
	if (pvd->pvd_next)
		pvd->pvd_next->pvd_prev = pvd->pvd_prev;
	if (pvd->pvd_prev)
		pvd->pvd_prev->pvd_next = pvd->pvd_next;
	else
		pfs_vncache = pvd->pvd_next;
	mtx_unlock(&pfs_vncache_mutex);

	--pfs_vncache_entries;
	FREE(pvd, M_PFSVNCACHE);
	vp->v_data = NULL;
	return (0);
}

/*
 * Purge the cache of dead / disabled entries
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
void
pfs_purge(struct pfs_node *pn)
{
	struct pfs_vdata *pvd;
	struct vnode *vnp;

	mtx_lock(&pfs_vncache_mutex);
	pvd = pfs_vncache;
	while (pvd != NULL) {
		if (pvd->pvd_dead || (pn != NULL && pvd->pvd_pn == pn)) {
			vnp = pvd->pvd_vnode;
			vhold(vnp);
			mtx_unlock(&pfs_vncache_mutex);
			VOP_LOCK(vnp, LK_EXCLUSIVE, curthread);
			vgone(vnp);
			VOP_UNLOCK(vnp, 0, curthread);
			vdrop(vnp);
			mtx_lock(&pfs_vncache_mutex);
			pvd = pfs_vncache;
		} else {
			pvd = pvd->pvd_next;
		}
	}
	mtx_unlock(&pfs_vncache_mutex);
}

/*
 * Free all vnodes associated with a defunct process
 *
 * XXXRW: It is unfortunate that pfs_exit() always acquires and releases two
 * mutexes (one of which is Giant) for every process exit, even if procfs
 * isn't mounted.
 */
static void
pfs_exit(void *arg, struct proc *p)
{
	struct pfs_vdata *pvd;
	int dead;

	if (pfs_vncache == NULL)
		return;
	mtx_lock(&Giant);
	mtx_lock(&pfs_vncache_mutex);
	for (pvd = pfs_vncache, dead = 0; pvd != NULL; pvd = pvd->pvd_next)
		if (pvd->pvd_pid == p->p_pid)
			dead = pvd->pvd_dead = 1;
	mtx_unlock(&pfs_vncache_mutex);
	if (dead)
		pfs_purge(NULL);
	mtx_unlock(&Giant);
}

/*
 * Disable a pseudofs node, and free all vnodes associated with it
 */
int
pfs_disable(struct pfs_node *pn)
{
	if (pn->pn_flags & PFS_DISABLED)
		return (0);
	pn->pn_flags |= PFS_DISABLED;
	pfs_purge(pn);
	return (0);
}

/*
 * Re-enable a disabled pseudofs node
 */
int
pfs_enable(struct pfs_node *pn)
{
	pn->pn_flags &= ~PFS_DISABLED;
	return (0);
}
