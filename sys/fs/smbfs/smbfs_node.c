/*
 * Copyright (c) 2000-2001 Boris Popov
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
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
/*#include <vm/vm_page.h>
#include <vm/vm_object.h>*/

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#define	SMBFS_NOHASH(smp, hval)	(&(smp)->sm_hash[(hval) & (smp)->sm_hashlen])
#define	smbfs_hash_lock(smp, td)	lockmgr(&smp->sm_hashlock, LK_EXCLUSIVE, NULL, td)
#define	smbfs_hash_unlock(smp, td)	lockmgr(&smp->sm_hashlock, LK_RELEASE, NULL, td)


extern vop_t **smbfs_vnodeop_p;

MALLOC_DEFINE(M_SMBNODE, "SMBFS node", "SMBFS vnode private part");
static MALLOC_DEFINE(M_SMBNODENAME, "SMBFS nname", "SMBFS node name");

int smbfs_hashprint(struct mount *mp);

#if 0
#ifdef SYSCTL_DECL
SYSCTL_DECL(_vfs_smbfs);
#endif
SYSCTL_PROC(_vfs_smbfs, OID_AUTO, vnprint, CTLFLAG_WR|CTLTYPE_OPAQUE,
	    NULL, 0, smbfs_hashprint, "S,vnlist", "vnode hash");
#endif

#define	FNV_32_PRIME ((u_int32_t) 0x01000193UL)
#define	FNV1_32_INIT ((u_int32_t) 33554467UL)

u_int32_t
smbfs_hash(const u_char *name, int nmlen)
{
	u_int32_t v;

	for (v = FNV1_32_INIT; nmlen; name++, nmlen--) {
		v *= FNV_32_PRIME;
		v ^= (u_int32_t)*name;
	}
	return v;
}

int
smbfs_hashprint(struct mount *mp)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode_hashhead *nhpp;
	struct smbnode *np;
	int i;

	for(i = 0; i <= smp->sm_hashlen; i++) {
		nhpp = &smp->sm_hash[i];
		LIST_FOREACH(np, nhpp, n_hash)
			vprint(NULL, SMBTOV(np));
	}
	return 0;
}

static char *
smbfs_name_alloc(const u_char *name, int nmlen)
{
	u_char *cp;

	nmlen++;
#ifdef SMBFS_NAME_DEBUG
	cp = malloc(nmlen + 2 + sizeof(int), M_SMBNODENAME, M_WAITOK);
	*(int*)cp = nmlen;
	cp += sizeof(int);
	cp[0] = 0xfc;
	cp++;
	bcopy(name, cp, nmlen - 1);
	cp[nmlen] = 0xfe;
#else
	cp = malloc(nmlen, M_SMBNODENAME, M_WAITOK);
	bcopy(name, cp, nmlen - 1);
#endif
	cp[nmlen - 1] = 0;
	return cp;
}

static void
smbfs_name_free(u_char *name)
{
#ifdef SMBFS_NAME_DEBUG
	int nmlen, slen;
	u_char *cp;

	cp = name;
	cp--;
	if (*cp != 0xfc) {
		printf("First byte of name entry '%s' corrupted\n", name);
		Debugger("ditto");
	}
	cp -= sizeof(int);
	nmlen = *(int*)cp;
	slen = strlen(name) + 1;
	if (nmlen != slen) {
		printf("Name length mismatch: was %d, now %d name '%s'\n",
		    nmlen, slen, name);
		Debugger("ditto");
	}
	if (name[nmlen] != 0xfe) {
		printf("Last byte of name entry '%s' corrupted\n", name);
		Debugger("ditto");
	}
	free(cp, M_SMBNODENAME);
#else
	free(name, M_SMBNODENAME);
#endif
}

static int
smbfs_node_alloc(struct mount *mp, struct vnode *dvp,
	const char *name, int nmlen, struct smbfattr *fap, struct vnode **vpp)
{
	struct thread *td = curthread;	/* XXX */
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode_hashhead *nhpp;
	struct smbnode *np, *np2, *dnp;
	struct vnode *vp;
	u_long hashval;
	int error;

	*vpp = NULL;
	if (smp->sm_root != NULL && dvp == NULL) {
		SMBERROR("do not allocate root vnode twice!\n");
		return EINVAL;
	}
	if (nmlen == 2 && bcmp(name, "..", 2) == 0) {
		if (dvp == NULL)
			return EINVAL;
		vp = VTOSMB(dvp)->n_parent->n_vnode;
		error = vget(vp, LK_EXCLUSIVE, td);
		if (error == 0)
			*vpp = vp;
		return error;
	} else if (nmlen == 1 && name[0] == '.') {
		SMBERROR("do not call me with dot!\n");
		return EINVAL;
	}
	dnp = dvp ? VTOSMB(dvp) : NULL;
	if (dnp == NULL && dvp != NULL) {
		vprint("smbfs_node_alloc: dead parent vnode", dvp);
		return EINVAL;
	}
	hashval = smbfs_hash(name, nmlen);
retry:
	smbfs_hash_lock(smp, td);
loop:
	nhpp = SMBFS_NOHASH(smp, hashval);
	LIST_FOREACH(np, nhpp, n_hash) {
		vp = SMBTOV(np);
		if (np->n_parent != dnp ||
		    np->n_nmlen != nmlen || bcmp(name, np->n_name, nmlen) != 0)
			continue;
		VI_LOCK(vp);
		smbfs_hash_unlock(smp, td);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td) != 0)
			goto retry;
		*vpp = vp;
		return 0;
	}
	smbfs_hash_unlock(smp, td);
	/*
	 * If we don't have node attributes, then it is an explicit lookup
	 * for an existing vnode.
	 */
	if (fap == NULL)
		return ENOENT;

	MALLOC(np, struct smbnode *, sizeof *np, M_SMBNODE, M_WAITOK);
	error = getnewvnode(VT_SMBFS, mp, smbfs_vnodeop_p, &vp);
	if (error) {
		FREE(np, M_SMBNODE);
		return error;
	}
	vp->v_type = fap->fa_attr & SMB_FA_DIR ? VDIR : VREG;
	bzero(np, sizeof(*np));
	vp->v_data = np;
	np->n_vnode = vp;
	np->n_mount = VFSTOSMBFS(mp);
	np->n_nmlen = nmlen;
	np->n_name = smbfs_name_alloc(name, nmlen);
	np->n_ino = fap->fa_ino;

	if (dvp) {
		np->n_parent = dnp;
		if (/*vp->v_type == VDIR &&*/ (dvp->v_flag & VROOT) == 0) {
			vref(dvp);
			np->n_flag |= NREFPARENT;
		}
	} else if (vp->v_type == VREG)
		SMBERROR("new vnode '%s' born without parent ?\n", np->n_name);

	lockinit(&vp->v_lock, PINOD, "smbnode", VLKTIMEOUT, LK_CANRECURSE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	smbfs_hash_lock(smp, td);
	LIST_FOREACH(np2, nhpp, n_hash) {
		if (np2->n_parent != dnp ||
		    np2->n_nmlen != nmlen || bcmp(name, np2->n_name, nmlen) != 0)
			continue;
		vput(vp);
/*		smb_name_free(np->n_name);
		FREE(np, M_SMBNODE);*/
		goto loop;
	}
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	smbfs_hash_unlock(smp, td);
	*vpp = vp;
	return 0;
}

int
smbfs_nget(struct mount *mp, struct vnode *dvp, const char *name, int nmlen,
	struct smbfattr *fap, struct vnode **vpp)
{
	struct smbnode *np;
	struct vnode *vp;
	int error;

	*vpp = NULL;
	error = smbfs_node_alloc(mp, dvp, name, nmlen, fap, &vp);
	if (error)
		return error;
	np = VTOSMB(vp);
	if (fap)
		smbfs_attr_cacheenter(vp, fap);
	*vpp = vp;
	return 0;
}

/*
 * Free smbnode, and give vnode back to system
 */
int
smbfs_reclaim(ap)                     
        struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct thread *a_p;
        } */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct thread *td = ap->a_td;
	struct vnode *dvp;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	
	SMBVDEBUG("%s,%d\n", np->n_name, vp->v_usecount);

	smbfs_hash_lock(smp, td);

	dvp = (np->n_parent && (np->n_flag & NREFPARENT)) ?
	    np->n_parent->n_vnode : NULL;

	if (np->n_hash.le_prev)
		LIST_REMOVE(np, n_hash);
	cache_purge(vp);
	if (smp->sm_root == np) {
		SMBVDEBUG("root vnode\n");
		smp->sm_root = NULL;
	}
	vp->v_data = NULL;
	smbfs_hash_unlock(smp, td);
	if (np->n_name)
		smbfs_name_free(np->n_name);
	FREE(np, M_SMBNODE);
	if (dvp) {
		VI_LOCK(dvp);
		if (dvp->v_usecount >= 1) {
			VI_UNLOCK(dvp);
			vrele(dvp);
		} else {
			VI_UNLOCK(dvp);
			SMBERROR("BUG: negative use count for parent!\n");
		}
	}
	return 0;
}

int
smbfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	struct thread *td = ap->a_td;
	struct ucred *cred = td->td_ucred;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	SMBVDEBUG("%s: %d\n", VTOSMB(vp)->n_name, vp->v_usecount);
	if (np->n_opencount) {
		error = smbfs_vinvalbuf(vp, V_SAVE, cred, td, 1);
		smb_makescred(&scred, td, cred);
		error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
		   &np->n_mtime, &scred);
		np->n_opencount = 0;
	}
	VOP_UNLOCK(vp, 0, td);
	return (0);
}
/*
 * routines to maintain vnode attributes cache
 * smbfs_attr_cacheenter: unpack np.i to vattr structure
 */
void
smbfs_attr_cacheenter(struct vnode *vp, struct smbfattr *fap)
{
	struct smbnode *np = VTOSMB(vp);

	if (vp->v_type == VREG) {
		if (np->n_size != fap->fa_size) {
			np->n_size = fap->fa_size;
			vnode_pager_setsize(vp, np->n_size);
		}
	} else if (vp->v_type == VDIR) {
		np->n_size = 16384; 		/* should be a better way ... */
	} else
		return;
	np->n_mtime = fap->fa_mtime;
	np->n_dosattr = fap->fa_attr;
	np->n_attrage = time_second;
	return;
}

int
smbfs_attr_cachelookup(struct vnode *vp, struct vattr *va)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	int diff;

	diff = time_second - np->n_attrage;
	if (diff > 2)	/* XXX should be configurable */
		return ENOENT;
	va->va_type = vp->v_type;		/* vnode type (for create) */
	if (vp->v_type == VREG) {
		va->va_mode = smp->sm_args.file_mode;	/* files access mode and type */
	} else if (vp->v_type == VDIR) {
		va->va_mode = smp->sm_args.dir_mode;	/* files access mode and type */
	} else
		return EINVAL;
	va->va_size = np->n_size;
	va->va_nlink = 1;		/* number of references to file */
	va->va_uid = smp->sm_args.uid;	/* owner user id */
	va->va_gid = smp->sm_args.gid;	/* owner group id */
	va->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	va->va_fileid = np->n_ino;	/* file id */
	if (va->va_fileid == 0)
		va->va_fileid = 2;
	va->va_blocksize = SSTOVC(smp->sm_share)->vc_txmax;
	va->va_mtime = np->n_mtime;
	va->va_atime = va->va_ctime = va->va_mtime;	/* time file changed */
	va->va_gen = VNOVAL;		/* generation number of file */
	va->va_flags = 0;		/* flags defined for file */
	va->va_rdev = VNOVAL;		/* device the special file represents */
	va->va_bytes = va->va_size;	/* bytes of disk space held by file */
	va->va_filerev = 0;		/* file modification number */
	va->va_vaflags = 0;		/* operations flags */
	return 0;
}
