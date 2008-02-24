/*-
 * Copyright (c) 1999, 2000 Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/nwfs/nwfs_node.c,v 1.39 2007/03/13 01:50:23 tegge Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>

#include <fs/nwfs/nwfs.h>
#include <fs/nwfs/nwfs_mount.h>
#include <fs/nwfs/nwfs_node.h>
#include <fs/nwfs/nwfs_subr.h>

#define	NWNOHASH(fhsum) (&nwhashtbl[(fhsum.f_id) & nwnodehash])

static LIST_HEAD(nwnode_hash_head,nwnode) *nwhashtbl;
static u_long nwnodehash;
static struct lock nwhashlock;

static MALLOC_DEFINE(M_NWNODE, "nwfs_node", "NWFS vnode private part");
static MALLOC_DEFINE(M_NWFSHASH, "nwfs_hash", "NWFS has table");

static int nwfs_sysctl_vnprint(SYSCTL_HANDLER_ARGS);

SYSCTL_DECL(_vfs_nwfs);

SYSCTL_PROC(_vfs_nwfs, OID_AUTO, vnprint, CTLFLAG_WR|CTLTYPE_OPAQUE,
	    NULL, 0, nwfs_sysctl_vnprint, "S,vnlist", "vnode hash");

void
nwfs_hash_init(void) {
	nwhashtbl = hashinit(desiredvnodes, M_NWFSHASH, &nwnodehash);
	lockinit(&nwhashlock, PVFS, "nwfshl", 0, 0);
}

void
nwfs_hash_free(void) {
	lockdestroy(&nwhashlock);
	free(nwhashtbl, M_NWFSHASH);
}

int
nwfs_sysctl_vnprint(SYSCTL_HANDLER_ARGS) {
	struct nwnode *np;
	struct nwnode_hash_head *nhpp;
	struct vnode *vp;
	int i;

	if (nwfs_debuglevel == 0)
		return 0;
	printf("Name:uc:hc:fid:pfid\n");
	for(i = 0; i <= nwnodehash; i++) {
		nhpp = &nwhashtbl[i];
		LIST_FOREACH(np, nhpp, n_hash) {
			vp = NWTOV(np);
			vprint("", vp);
			printf("%s:%d:%d:%d:%d\n",np->n_name,vrefcnt(vp),
			    vp->v_holdcnt,np->n_fid.f_id, np->n_fid.f_parent);
		}
	}
	return 0;
}

/*
 * Search nwnode with given fid.
 * Hash list should be locked by caller.
 */
static int
nwfs_hashlookup(struct nwmount *nmp, ncpfid fid, struct nwnode **npp)
{
	struct nwnode *np;
	struct nwnode_hash_head *nhpp;

	nhpp = NWNOHASH(fid);
	LIST_FOREACH(np, nhpp, n_hash) {
		if (nmp != np->n_mount || !NWCMPF(&fid, &np->n_fid))
			continue;
		if (npp)
			*npp = np;
		return 0;
	}
	return ENOENT;
}

/*
 * Allocate new nwfsnode/vnode from given nwnode. 
 * Vnode referenced and not locked.
 */
static int
nwfs_allocvp(struct mount *mp, ncpfid fid, struct nw_entry_info *fap,
	struct vnode *dvp, struct vnode **vpp)
{
	struct thread *td = curthread;	/* XXX */
	struct nwnode *np;
	struct nwnode_hash_head *nhpp;
	struct nwmount *nmp = VFSTONWFS(mp);
	struct vnode *vp;
	int error;

loop:
	lockmgr(&nwhashlock, LK_EXCLUSIVE, NULL, td);
rescan:
	if (nwfs_hashlookup(nmp, fid, &np) == 0) {
		vp = NWTOV(np);
		mtx_lock(&vp->v_interlock);
		lockmgr(&nwhashlock, LK_RELEASE, NULL, td);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td))
			goto loop;
		if (fap)
			np->n_attr = fap->attributes;
		*vpp = vp;
		return(0);
	}
	lockmgr(&nwhashlock, LK_RELEASE, NULL, td);

	if (fap == NULL || ((fap->attributes & aDIR) == 0 && dvp == NULL))
		panic("nwfs_allocvp: fap = %p, dvp = %p\n", fap, dvp);
	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(np, struct nwnode *, sizeof *np, M_NWNODE, M_WAITOK | M_ZERO);
	error = getnewvnode("nwfs", mp, &nwfs_vnodeops, &vp);
	if (error) {
		*vpp = NULL;
		FREE(np, M_NWNODE);
		return (error);
	}
	error = insmntque(vp, mp);	/* XXX: Too early for mpsafe fs */
	if (error != 0) {
		FREE(np, M_NWNODE);
		*vpp = NULL;
		return (error);
	}
	vp->v_data = np;
	np->n_vnode = vp;
	np->n_mount = nmp;
	np->n_attr = fap->attributes;
	vp->v_type = np->n_attr & aDIR ? VDIR : VREG;
	np->n_fid = fid;
	if (dvp) {
		np->n_parent = VTONW(dvp)->n_fid;
	}
	vp->v_vnlock->lk_flags |= LK_CANRECURSE;
	lockmgr(&nwhashlock, LK_EXCLUSIVE, NULL, td);
	/*
	 * Another process can create vnode while we blocked in malloc() or
	 * getnewvnode(). Rescan list again.
	 */
	if (nwfs_hashlookup(nmp, fid, NULL) == 0) {
		vp->v_data = NULL;
		np->n_vnode = NULL;
		vrele(vp);
		FREE(np, M_NWNODE);
		goto rescan;
	}
	*vpp = vp;
	nhpp = NWNOHASH(fid);
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	lockmgr(&nwhashlock, LK_RELEASE, NULL, td);
	
	ASSERT_VOP_LOCKED(dvp, "nwfs_allocvp");
	if (vp->v_type == VDIR && dvp && (dvp->v_vflag & VV_ROOT) == 0) {
		np->n_flag |= NREFPARENT;
		vref(dvp);
	}
	return 0;
}

int
nwfs_nget(struct mount *mp, ncpfid fid, struct nw_entry_info *fap,
	  struct vnode *dvp, struct vnode **vpp)
{
	struct vnode *vp;
	int error;

	*vpp = NULL;
	error = nwfs_allocvp(mp, fid, fap, dvp, &vp);
	if (error)
		return error;
	if (fap)
		nwfs_attr_cacheenter(vp, fap);
	*vpp = vp;
	return 0;
}

int
nwfs_lookupnp(struct nwmount *nmp, ncpfid fid, struct thread *td,
	struct nwnode **npp)
{
	int error;

	lockmgr(&nwhashlock, LK_EXCLUSIVE, NULL, td);
	error = nwfs_hashlookup(nmp, fid, npp);
	lockmgr(&nwhashlock, LK_RELEASE, NULL, td);
	return error;
}

/*
 * Free nwnode, and give vnode back to system
 */
int
nwfs_reclaim(ap)                     
        struct vop_reclaim_args /* {
    		struct vnode *a_vp;
		struct thread *a_td;
        } */ *ap;
{
	struct vnode *dvp = NULL, *vp = ap->a_vp;
	struct nwnode *dnp, *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	struct thread *td = ap->a_td;
	
	NCPVNDEBUG("%s,%d\n", np->n_name, vrefcnt(vp));
	/*
	 * Destroy the vm object and flush associated pages.
	 */
	vnode_destroy_vobject(vp);

	if (np->n_flag & NREFPARENT) {
		np->n_flag &= ~NREFPARENT;
		if (nwfs_lookupnp(nmp, np->n_parent, td, &dnp) == 0) {
			dvp = dnp->n_vnode;
		} else {
			NCPVNDEBUG("%s: has no parent ?\n",np->n_name);
		}
	}
	lockmgr(&nwhashlock, LK_EXCLUSIVE, NULL, td);
	LIST_REMOVE(np, n_hash);
	lockmgr(&nwhashlock, LK_RELEASE, NULL, td);
	if (nmp->n_root == np) {
		nmp->n_root = NULL;
	}
	vp->v_data = NULL;
	FREE(np, M_NWNODE);
	if (dvp) {
		vrele(dvp);
	}
	return (0);
}

int
nwfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	struct thread *td = ap->a_td;
	struct ucred *cred = td->td_ucred;
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	int error;

	NCPVNDEBUG("%s: %d\n", VTONW(vp)->n_name, vrefcnt(vp));
	if (np->opened) {
		error = nwfs_vinvalbuf(vp, td);
		error = ncp_close_file(NWFSTOCONN(VTONWFS(vp)), &np->n_fh, td, cred);
		np->opened = 0;
	}
	if (np->n_flag & NSHOULDFREE) {
		cache_purge(vp);
		vgone(vp);
	}
	return (0);
}
/*
 * routines to maintain vnode attributes cache
 * nwfs_attr_cacheenter: unpack np.i to va structure
 */
void
nwfs_attr_cacheenter(struct vnode *vp, struct nw_entry_info *fi)
{
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	struct vattr *va = &np->n_vattr;

	va->va_type = vp->v_type;		/* vnode type (for create) */
	np->n_nmlen = fi->nameLen;
	bcopy(fi->entryName, np->n_name, np->n_nmlen);
	np->n_name[fi->nameLen] = 0;
	if (vp->v_type == VREG) {
		if (va->va_size != fi->dataStreamSize) {
			va->va_size = fi->dataStreamSize;
			vnode_pager_setsize(vp, va->va_size);
		}
		va->va_mode = nmp->m.file_mode;	/* files access mode and type */
	} else if (vp->v_type == VDIR) {
		va->va_size = 16384; 		/* should be a better way ... */
		va->va_mode = nmp->m.dir_mode;	/* files access mode and type */
	} else
		return;
	np->n_size = va->va_size;
	va->va_nlink = 1;		/* number of references to file */
	va->va_uid = nmp->m.uid;	/* owner user id */
	va->va_gid = nmp->m.gid;	/* owner group id */
	va->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	va->va_fileid = np->n_fid.f_id;	/* file id */
	if (va->va_fileid == 0)
		va->va_fileid = NWFS_ROOT_INO;
	va->va_blocksize=nmp->connh->nh_conn->buffer_size;/* blocksize preferred for i/o */
	/* time of last modification */
	ncp_dos2unixtime(fi->modifyDate, fi->modifyTime, 0, nmp->m.tz, &va->va_mtime);
	/* time of last access */
	ncp_dos2unixtime(fi->lastAccessDate, 0, 0, nmp->m.tz, &va->va_atime);
	va->va_ctime = va->va_mtime;	/* time file changed */
	va->va_gen = VNOVAL;		/* generation number of file */
	va->va_flags = 0;		/* flags defined for file */
	va->va_rdev = VNOVAL;		/* device the special file represents */
	va->va_bytes = va->va_size;	/* bytes of disk space held by file */
	va->va_filerev = 0;		/* file modification number */
	va->va_vaflags = 0;		/* operations flags */
	np->n_vattr = *va;
	if (np->n_mtime == 0) {
		np->n_mtime = va->va_mtime.tv_sec;
	}
	np->n_atime = time_second;
	np->n_dosfid = fi->DosDirNum;
	return;
}

int
nwfs_attr_cachelookup(struct vnode *vp, struct vattr *va)
{
	struct nwnode *np = VTONW(vp);
	int diff;

	diff = time_second - np->n_atime;
	if (diff > 2) {	/* XXX should be configurable */
		return ENOENT;
	}
	*va = np->n_vattr;
	return 0;
}
