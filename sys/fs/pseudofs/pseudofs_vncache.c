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
 *
 *      $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
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
struct pfs_vdata *pfs_vncache;
static void pfs_exit(struct proc *p);

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

extern vop_t **pfs_vnodeop_p;

/*
 * Initialize vnode cache
 */
void
pfs_vncache_load(void)
{
	mtx_init(&pfs_vncache_mutex, "pseudofs_vncache", MTX_DEF|MTX_RECURSE);
	/* XXX at_exit() can fail with ENOMEN */
	at_exit(pfs_exit);
}

/*
 * Tear down vnode cache
 */
void
pfs_vncache_unload(void)
{
	rm_at_exit(pfs_exit);
	if (pfs_vncache_entries != 0)
		printf("pfs_vncache_unload(): %d entries remaining\n",
		    pfs_vncache_entries);
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
	int error;

	/*
	 * See if the vnode is in the cache.  
	 * XXX linear search is not very efficient.
	 */
	mtx_lock(&pfs_vncache_mutex);
	for (pvd = pfs_vncache; pvd; pvd = pvd->pvd_next) {
		if (pvd->pvd_pn == pn && pvd->pvd_pid == pid) {
			if (vget(pvd->pvd_vnode, 0, curthread) == 0) {
				++pfs_vncache_hits;
				*vpp = pvd->pvd_vnode;
				mtx_unlock(&pfs_vncache_mutex);
				/* XXX see comment at top of pfs_lookup() */
				cache_purge(*vpp);
				return (0);
			}
			/* XXX if this can happen, we're in trouble */
			break;
		}
	}
	mtx_unlock(&pfs_vncache_mutex);
	++pfs_vncache_misses;

	/* nope, get a new one */
	MALLOC(pvd, struct pfs_vdata *, sizeof *pvd, M_PFSVNCACHE, M_WAITOK);
	if (++pfs_vncache_entries > pfs_vncache_maxentries)
		pfs_vncache_maxentries = pfs_vncache_entries;
	error = getnewvnode(VT_PSEUDOFS, mp, pfs_vnodeop_p, vpp);
	if (error)
		return (error);
	pvd->pvd_pn = pn;
	pvd->pvd_pid = pid;
	(*vpp)->v_data = pvd;
	switch (pn->pn_type) {
	case pfstype_root:
		(*vpp)->v_flag = VROOT;
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
	pvd->pvd_vnode = *vpp;
	mtx_lock(&pfs_vncache_mutex);
	pvd->pvd_prev = NULL;
	pvd->pvd_next = pfs_vncache;
	if (pvd->pvd_next)
		pvd->pvd_next->pvd_prev = pvd;
	pfs_vncache = pvd;
	mtx_unlock(&pfs_vncache_mutex);
	return (0);
}

/*
 * Free a vnode
 */
int
pfs_vncache_free(struct vnode *vp)
{
	struct pfs_vdata *pvd;

	cache_purge(vp);
	
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
 * Free all vnodes associated with a defunct process
 */
static void
pfs_exit(struct proc *p)
{
	struct pfs_vdata *pvd, *prev;

	mtx_lock(&pfs_vncache_mutex);
	/*
	 * The double loop is necessary because vgone() indirectly
	 * calls pfs_vncache_free() which frees pvd, so we have to
	 * backtrace one step every time we free a vnode.
	 */
	/* XXX linear search... not very efficient */
	for (pvd = pfs_vncache; pvd != NULL; pvd = pvd->pvd_next) {
		while (pvd != NULL && pvd->pvd_pid == p->p_pid) {
			prev = pvd->pvd_prev;
			vgone(pvd->pvd_vnode);
			pvd = prev ? prev->pvd_next : pfs_vncache;
		}
	}
	mtx_unlock(&pfs_vncache_mutex);
}

/*
 * Disable a pseudofs node, and free all vnodes associated with it
 */
int
pfs_disable(struct pfs_node *pn)
{
	struct pfs_vdata *pvd, *prev;
	
	if (pn->pn_flags & PFS_DISABLED)
		return (0);
	mtx_lock(&pfs_vncache_mutex);
	pn->pn_flags |= PFS_DISABLED;
	/* see the comment about the double loop in pfs_exit() */
	/* XXX linear search... not very efficient */
	for (pvd = pfs_vncache; pvd != NULL; pvd = pvd->pvd_next) {
		while (pvd != NULL && pvd->pvd_pn == pn) {
			prev = pvd->pvd_prev;
			vgone(pvd->pvd_vnode);
			pvd = prev ? prev->pvd_next : pfs_vncache;
		}
	}
	mtx_unlock(&pfs_vncache_mutex);
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
