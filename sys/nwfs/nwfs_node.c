/*
 * Copyright (c) 1999, Boris Popov
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
 * $FreeBSD: src/sys/nwfs/nwfs_node.c,v 1.3.2.2 2000/08/03 00:09:37 ps Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <sys/queue.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>

#include <nwfs/nwfs.h>
#include <nwfs/nwfs_mount.h>
#include <nwfs/nwfs_node.h>
#include <nwfs/nwfs_subr.h>

#define	NWNOHASH(fhsum) (&nwhashtbl[(fhsum.f_id) & nwnodehash])

extern vop_t **nwfs_vnodeop_p;

static LIST_HEAD(nwnode_hash_head,nwnode) *nwhashtbl;
static u_long nwnodehash;
static int nwhashlock = 0;

MALLOC_DEFINE(M_NWNODE, "NWFS node", "NWFS vnode private part");
MALLOC_DEFINE(M_NWFSHASH, "NWFS hash", "NWFS has table");

static int nwfs_sysctl_vnprint(SYSCTL_HANDLER_ARGS);

extern struct linker_set sysctl_vfs_nwfs;

SYSCTL_DECL(_vfs_nwfs);

SYSCTL_PROC(_vfs_nwfs, OID_AUTO, vnprint, CTLFLAG_WR|CTLTYPE_OPAQUE,
	    NULL, 0, nwfs_sysctl_vnprint, "S,vnlist", "vnode hash");

void
nwfs_hash_init(void) {
	nwhashtbl = hashinit(desiredvnodes, M_NWFSHASH, &nwnodehash);
}

void
nwfs_hash_free(void) {
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
		for (np = nhpp->lh_first; np != 0; np = np->n_hash.le_next) {
			vp = NWTOV(np);
			printf("%s:%d:%d:%d:%d\n",np->n_name,vp->v_usecount,vp->v_holdcnt,
			    np->n_fid.f_id, np->n_fid.f_parent);
		}
	}
	return 0;
}

/*
 * Allocate new nwfsnode/vnode from given nwnode. 
 * Vnode referenced and not locked.
 */
int
nwfs_allocvp(struct mount *mp, ncpfid fid, struct vnode **vpp) {
	struct proc *p = curproc;	/* XXX */
	struct nwnode *np, *np2;
	struct nwnode_hash_head *nhpp;
	struct vnode *vp;
	int error;

retry:
	nhpp = NWNOHASH(fid);
loop:
	for (np = nhpp->lh_first; np != 0; np = np->n_hash.le_next) {
		vp = NWTOV(np);
		if (mp != vp->v_mount || !NWCMPF(&fid, &np->n_fid))
			continue;
		if (vget(vp, LK_EXCLUSIVE, p))
			goto loop;
		*vpp = vp;
		return(0);
	}

	/* lock list, or waiting in malloc can cause problems */
	if (nwhashlock) {
		while(nwhashlock) {
			nwhashlock = -1;
			tsleep((caddr_t) &nwhashlock, PVM, "nwfsvp", 0);
		}
		goto loop;
	}
	nwhashlock = 1;
	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(np, struct nwnode *, sizeof *np, M_NWNODE, M_WAITOK);
	error = getnewvnode(VT_NWFS, mp, nwfs_vnodeop_p, &vp);
	if (error) {
		if (nwhashlock < 0)
			wakeup(&nwhashlock);
		nwhashlock = 0;
		*vpp = 0;
		FREE(np, M_NWNODE);
		return (error);
	}
	*vpp = vp;
	bzero(np,sizeof(*np));
	vp->v_data = np;
	np->n_vnode = vp;
	np->n_mount = VFSTONWFS(mp);
	np->n_fid = fid;
	for (np2 = nhpp->lh_first; np2 != 0; np2 = np->n_hash.le_next) {
		if (mp != NWTOV(np2)->v_mount || !NWCMPF(&fid, &np2->n_fid))
			continue;
		vrele(vp);
		FREE(np, M_NWNODE);
		if (nwhashlock < 0)
			wakeup(&nwhashlock);
		nwhashlock = 0;
		goto retry;
	}
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	if (nwhashlock < 0)
		wakeup(&nwhashlock);
	nwhashlock = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	np->n_flag |= NNEW;
	return (error);
}

int
nwfs_lookupnp(struct nwmount *nmp, ncpfid fid, struct nwnode **npp) {
	struct nwnode *np;
	struct nwnode_hash_head *nhpp;

	nhpp = NWNOHASH(fid);
	for (np = nhpp->lh_first; np != 0; np = np->n_hash.le_next) {
		if (nmp != np->n_mount || !NWCMPF(&fid, &np->n_fid))
			continue;
		*npp = np;
		return(0);
	}
	return ENOENT;
}

/*
 * Free nwnode, and give vnode back to system
 */
int
nwfs_reclaim(ap)                     
        struct vop_reclaim_args /* {
        struct vnode *a_vp;
        } */ *ap;
{
	struct vnode *dvp = NULL, *vp = ap->a_vp;
	struct nwnode *dnp, *np = VTONW(vp);
	struct nwmount *nmp=VTONWFS(vp);
	
	NCPVNDEBUG("%s,%d\n", np->n_name, vp->v_usecount);
	if (np->n_refparent) {
		np->n_refparent = 0;
		if (nwfs_lookupnp(nmp, np->n_parent, &dnp) == 0) {
			dvp = dnp->n_vnode;
		} else {
			NCPVNDEBUG("%s: has no parent ?\n",np->n_name);
		}
	}
	LIST_REMOVE(np, n_hash);
	cache_purge(vp);
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
		struct proc *a_p;
	} */ *ap;
{
	struct proc *p = ap->a_p;
	struct ucred *cred = p->p_ucred;
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	int error;

	NCPVNDEBUG("%s: %d\n", VTONW(vp)->n_name, vp->v_usecount);
	if (np->opened) {
		error = nwfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
		error = ncp_close_file(NWFSTOCONN(VTONWFS(vp)), &np->n_fh, p, cred);
		np->opened = 0;
	}
	VOP_UNLOCK(vp, 0, p);
	return (0);
}
/*
 * routines to maintain vnode attributes cache
 * nwfs_attr_cacheenter: unpack np.i to va structure
 */
void
nwfs_attr_cacheenter(struct vnode *vp, struct nw_entry_info *fi) {
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	register struct vattr *va = &np->n_vattr;

	va->va_type = vp->v_type;		/* vnode type (for create) */
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
	return;
}

int
nwfs_attr_cachelookup(struct vnode *vp, struct vattr *va) {
	struct nwnode *np = VTONW(vp);
	int diff;

	diff = time_second - np->n_atime;
	if (diff > 2) {	/* XXX should be configurable */
		return ENOENT;
	}
	*va = np->n_vattr;
	return 0;
}
