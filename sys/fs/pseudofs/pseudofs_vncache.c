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
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/pseudofs/pseudofs_internal.h>

static MALLOC_DEFINE(M_PFSVNCACHE, "pfs_vncache", "pseudofs vnode cache");

static struct mtx pfs_vncache_mutex;

struct pfs_vnode {
	struct vnode		*pv_vnode;
	struct pfs_vnode	*pv_next;
} *pfs_vncache;

SYSCTL_NODE(_vfs_pfs, OID_AUTO, vncache, CTLFLAG_RW, 0,
    "pseudofs vnode cache");

static int pfs_vncache_hits;
SYSCTL_INT(_vfs_pfs_vncache, OID_AUTO, hits, CTLFLAG_RD, &pfs_vncache_hits, 0,
    "number of cache hits since initialization");

static int pfs_vncache_misses;
SYSCTL_INT(_vfs_pfs_vncache, OID_AUTO, misses, CTLFLAG_RD, &pfs_vncache_misses, 0,
    "number of cache misses since initialization");

extern vop_t **pfs_vnodeop_p;

/*
 * Initialize vnode cache
 */
void
pfs_vncache_load(void)
{
	mtx_init(&pfs_vncache_mutex, "pseudofs_vncache", MTX_DEF);
}

/*
 * Tear down vnode cache
 */
void
pfs_vncache_unload(void)
{
	mtx_destroy(&pfs_vncache_mutex);
}

/*
 * Allocate a vnode
 */
int
pfs_vncache_alloc(struct mount *mp, struct vnode **vpp,
		  struct pfs_node *pn, pid_t pid)
{
	struct pfs_vnode *pv;
	struct pfs_vdata *pvd;
	int error;
	
	/* see if the vnode is in the cache */
	mtx_lock(&pfs_vncache_mutex);
	for (pv = pfs_vncache; pv; pv = pv->pv_next) {
		pvd = (struct pfs_vdata *)pv->pv_vnode->v_data;
		if (pvd->pvd_pn == pn && pvd->pvd_pid == pid) {
			if (vget(pv->pv_vnode, 0, curproc) == 0) {
				++pfs_vncache_hits;
				*vpp = pv->pv_vnode;
				mtx_unlock(&pfs_vncache_mutex);
				return (0);
			}
			/* XXX if this can happen, we're in trouble */
			break;
		}
	}
	mtx_unlock(&pfs_vncache_mutex);
	++pfs_vncache_misses;

	/* nope, get a new one */
	MALLOC(pv, struct pfs_vnode *, sizeof *pv, M_PFSVNCACHE, M_WAITOK);
	MALLOC(pvd, struct pfs_vdata *, sizeof *pvd, M_PFSVNCACHE, M_WAITOK);
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
	pv->pv_vnode = *vpp;
	mtx_lock(&pfs_vncache_mutex);
	pv->pv_next = pfs_vncache;
	pfs_vncache = pv;
	mtx_unlock(&pfs_vncache_mutex);
	return (0);
}

/*
 * Free a vnode
 */
int
pfs_vncache_free(struct vnode *vp)
{
	struct pfs_vnode *prev, *pv;
	struct pfs_vdata *pvd;
	
	mtx_lock(&pfs_vncache_mutex);
	for (prev = NULL, pv = pfs_vncache; pv; prev = pv, pv = pv->pv_next)
		if (pv->pv_vnode == vp)
			break;
	KASSERT(pv != NULL, ("pfs_vncache_free(): not in cache\n"));
	if (prev)
		prev->pv_next = pv->pv_next;
	else
		pfs_vncache = pv->pv_next;
	mtx_unlock(&pfs_vncache_mutex);
	
	pvd = (struct pfs_vdata *)vp->v_data;
	FREE(pvd, M_PFSVNCACHE);
	vp->v_data = NULL;
	FREE(pv, M_PFSVNCACHE);
	return (0);
}
