/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Functions that perform the vfs operations required by the routines in
 * nfsd_serv.c. It is hoped that this change will make the server more
 * portable.
 */

#include <fs/nfs/nfsport.h>
#include <sys/hash.h>
#include <sys/sysctl.h>
#include <nlm/nlm_prot.h>
#include <nlm/nlm.h>

extern u_int32_t newnfs_true, newnfs_false, newnfs_xdrneg1;
extern int nfsrv_useacl;
extern int newnfs_numnfsd;
extern struct mount nfsv4root_mnt;
extern struct nfsrv_stablefirst nfsrv_stablefirst;
extern void (*nfsd_call_servertimer)(void);
struct vfsoptlist nfsv4root_opt, nfsv4root_newopt;
NFSDLOCKMUTEX;
struct mtx nfs_cache_mutex;
struct mtx nfs_v4root_mutex;
struct nfsrvfh nfs_rootfh, nfs_pubfh;
int nfs_pubfhset = 0, nfs_rootfhset = 0;
static uint32_t nfsv4_sysid = 0;

static int nfssvc_srvcall(struct thread *, struct nfssvc_args *,
    struct ucred *);

static int enable_crossmntpt = 1;
static int nfs_commit_blks;
static int nfs_commit_miss;
extern int nfsrv_issuedelegs;
extern int nfsrv_dolocallocks;

SYSCTL_DECL(_vfs_newnfs);
SYSCTL_INT(_vfs_newnfs, OID_AUTO, mirrormnt, CTLFLAG_RW, &enable_crossmntpt,
    0, "Enable nfsd to cross mount points");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, commit_blks, CTLFLAG_RW, &nfs_commit_blks,
    0, "");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, commit_miss, CTLFLAG_RW, &nfs_commit_miss,
    0, "");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, issue_delegations, CTLFLAG_RW,
    &nfsrv_issuedelegs, 0, "Enable nfsd to issue delegations");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, enable_locallocks, CTLFLAG_RW,
    &nfsrv_dolocallocks, 0, "Enable nfsd to acquire local locks on files");

#define	NUM_HEURISTIC		1017
#define	NHUSE_INIT		64
#define	NHUSE_INC		16
#define	NHUSE_MAX		2048

static struct nfsheur {
	struct vnode *nh_vp;	/* vp to match (unreferenced pointer) */
	off_t nh_nextr;		/* next offset for sequential detection */
	int nh_use;		/* use count for selection */
	int nh_seqcount;	/* heuristic */
} nfsheur[NUM_HEURISTIC];


/*
 * Get attributes into nfsvattr structure.
 */
int
nfsvno_getattr(struct vnode *vp, struct nfsvattr *nvap, struct ucred *cred,
    struct thread *p, int vpislocked)
{
	int error, lockedit = 0;

	if (vpislocked == 0) {
		/*
		 * When vpislocked == 0, the vnode is either exclusively
		 * locked by this thread or not locked by this thread.
		 * As such, shared lock it, if not exclusively locked.
		 */
		if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE) {
			lockedit = 1;
			vn_lock(vp, LK_SHARED | LK_RETRY);
		}
	}
	error = VOP_GETATTR(vp, &nvap->na_vattr, cred);
	if (lockedit != 0)
		VOP_UNLOCK(vp, 0);
	return (error);
}

/*
 * Get a file handle for a vnode.
 */
int
nfsvno_getfh(struct vnode *vp, fhandle_t *fhp, struct thread *p)
{
	int error;

	NFSBZERO((caddr_t)fhp, sizeof(fhandle_t));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VOP_VPTOFH(vp, &fhp->fh_fid);
	return (error);
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client. You cannot just use
 * vn_writechk() and VOP_ACCESSX() for two reasons.
 * 1 - You must check for exported rdonly as well as MNT_RDONLY for the write
 *     case.
 * 2 - The owner is to be given access irrespective of mode bits for some
 *     operations, so that processes that chmod after opening a file don't
 *     break.
 */
int
nfsvno_accchk(struct vnode *vp, accmode_t accmode, struct ucred *cred,
    struct nfsexstuff *exp, struct thread *p, int override, int vpislocked,
    u_int32_t *supportedtypep)
{
	struct vattr vattr;
	int error = 0, getret = 0;

	if (vpislocked == 0) {
		if (vn_lock(vp, LK_SHARED) != 0)
			return (EPERM);
	}
	if (accmode & VWRITE) {
		/* Just vn_writechk() changed to check rdonly */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (NFSVNO_EXRDONLY(exp) ||
		    (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			switch (vp->v_type) {
			case VREG:
			case VDIR:
			case VLNK:
				error = EROFS;
			default:
				break;
			}
		}
		/*
		 * If there's shared text associated with
		 * the inode, try to free it up once.  If
		 * we fail, we can't allow writing.
		 */
		if ((vp->v_vflag & VV_TEXT) != 0 && error == 0)
			error = ETXTBSY;
	}
	if (error != 0) {
		if (vpislocked == 0)
			VOP_UNLOCK(vp, 0);
		return (error);
	}

	/*
	 * Should the override still be applied when ACLs are enabled?
	 */
	error = VOP_ACCESSX(vp, accmode, cred, p);
	if (error != 0 && (accmode & (VDELETE | VDELETE_CHILD))) {
		/*
		 * Try again with VEXPLICIT_DENY, to see if the test for
		 * deletion is supported.
		 */
		error = VOP_ACCESSX(vp, accmode | VEXPLICIT_DENY, cred, p);
		if (error == 0) {
			if (vp->v_type == VDIR) {
				accmode &= ~(VDELETE | VDELETE_CHILD);
				accmode |= VWRITE;
				error = VOP_ACCESSX(vp, accmode, cred, p);
			} else if (supportedtypep != NULL) {
				*supportedtypep &= ~NFSACCESS_DELETE;
			}
		}
	}

	/*
	 * Allow certain operations for the owner (reads and writes
	 * on files that are already open).
	 */
	if (override != NFSACCCHK_NOOVERRIDE &&
	    (error == EPERM || error == EACCES)) {
		if (cred->cr_uid == 0 && (override & NFSACCCHK_ALLOWROOT))
			error = 0;
		else if (override & NFSACCCHK_ALLOWOWNER) {
			getret = VOP_GETATTR(vp, &vattr, cred);
			if (getret == 0 && cred->cr_uid == vattr.va_uid)
				error = 0;
		}
	}
	if (vpislocked == 0)
		VOP_UNLOCK(vp, 0);
	return (error);
}

/*
 * Set attribute(s) vnop.
 */
int
nfsvno_setattr(struct vnode *vp, struct nfsvattr *nvap, struct ucred *cred,
    struct thread *p, struct nfsexstuff *exp)
{
	int error;

	error = VOP_SETATTR(vp, &nvap->na_vattr, cred);
	return (error);
}

/*
 * Set up nameidata for a lookup() call and do it
 * For the cases where we are crossing mount points
 * (looking up the public fh path or the v4 root path when
 *  not using a pseudo-root fs), set/release the Giant lock,
 * as required.
 */
int
nfsvno_namei(struct nfsrv_descript *nd, struct nameidata *ndp,
    struct vnode *dp, int islocked, struct nfsexstuff *exp, struct thread *p,
    struct vnode **retdirp)
{
	struct componentname *cnp = &ndp->ni_cnd;
	int i;
	struct iovec aiov;
	struct uio auio;
	int lockleaf = (cnp->cn_flags & LOCKLEAF) != 0, linklen;
	int error = 0, crossmnt;
	char *cp;

	*retdirp = NULL;
	cnp->cn_nameptr = cnp->cn_pnbuf;
	/*
	 * Extract and set starting directory.
	 */
	if (dp->v_type != VDIR) {
		if (islocked)
			vput(dp);
		else
			vrele(dp);
		nfsvno_relpathbuf(ndp);
		return (ENOTDIR);
	}
	if (islocked)
		NFSVOPUNLOCK(dp, 0, p);
	VREF(dp);
	*retdirp = dp;
	if (NFSVNO_EXRDONLY(exp))
		cnp->cn_flags |= RDONLY;
	ndp->ni_segflg = UIO_SYSSPACE;
	crossmnt = 1;

	if (nd->nd_flag & ND_PUBLOOKUP) {
		ndp->ni_loopcnt = 0;
		if (cnp->cn_pnbuf[0] == '/') {
			vrele(dp);
			/*
			 * Check for degenerate pathnames here, since lookup()
			 * panics on them.
			 */
			for (i = 1; i < ndp->ni_pathlen; i++)
				if (cnp->cn_pnbuf[i] != '/')
					break;
			if (i == ndp->ni_pathlen) {
				error = NFSERR_ACCES;
				goto out;
			}
			dp = rootvnode;
			VREF(dp);
		}
	} else if ((enable_crossmntpt == 0 && NFSVNO_EXPORTED(exp)) ||
	    (nd->nd_flag & ND_NFSV4) == 0) {
		/*
		 * Only cross mount points for NFSv4 when doing a
		 * mount while traversing the file system above
		 * the mount point, unless enable_crossmntpt is set.
		 */
		cnp->cn_flags |= NOCROSSMOUNT;
		crossmnt = 0;
	}

	/*
	 * Initialize for scan, set ni_startdir and bump ref on dp again
	 * becuase lookup() will dereference ni_startdir.
	 */

	cnp->cn_thread = p;
	ndp->ni_startdir = dp;
	ndp->ni_rootdir = rootvnode;

	if (!lockleaf)
		cnp->cn_flags |= LOCKLEAF;
	for (;;) {
		cnp->cn_nameptr = cnp->cn_pnbuf;
		/*
		 * Call lookup() to do the real work.  If an error occurs,
		 * ndp->ni_vp and ni_dvp are left uninitialized or NULL and
		 * we do not have to dereference anything before returning.
		 * In either case ni_startdir will be dereferenced and NULLed
		 * out.
		 */
		if (exp->nes_vfslocked)
			ndp->ni_cnd.cn_flags |= GIANTHELD;
		error = lookup(ndp);
		/*
		 * The Giant lock should only change when
		 * crossing mount points.
		 */
		if (crossmnt) {
			exp->nes_vfslocked =
			    (ndp->ni_cnd.cn_flags & GIANTHELD) != 0;
			ndp->ni_cnd.cn_flags &= ~GIANTHELD;
		}
		if (error)
			break;

		/*
		 * Check for encountering a symbolic link.  Trivial
		 * termination occurs if no symlink encountered.
		 */
		if ((cnp->cn_flags & ISSYMLINK) == 0) {
			if ((cnp->cn_flags & (SAVENAME | SAVESTART)) == 0)
				nfsvno_relpathbuf(ndp);
			if (ndp->ni_vp && !lockleaf)
				NFSVOPUNLOCK(ndp->ni_vp, 0, p);
			break;
		}

		/*
		 * Validate symlink
		 */
		if ((cnp->cn_flags & LOCKPARENT) && ndp->ni_pathlen == 1)
			NFSVOPUNLOCK(ndp->ni_dvp, 0, p);
		if (!(nd->nd_flag & ND_PUBLOOKUP)) {
			error = EINVAL;
			goto badlink2;
		}

		if (ndp->ni_loopcnt++ >= MAXSYMLINKS) {
			error = ELOOP;
			goto badlink2;
		}
		if (ndp->ni_pathlen > 1)
			cp = uma_zalloc(namei_zone, M_WAITOK);
		else
			cp = cnp->cn_pnbuf;
		aiov.iov_base = cp;
		aiov.iov_len = MAXPATHLEN;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_td = NULL;
		auio.uio_resid = MAXPATHLEN;
		error = VOP_READLINK(ndp->ni_vp, &auio, cnp->cn_cred);
		if (error) {
		badlink1:
			if (ndp->ni_pathlen > 1)
				uma_zfree(namei_zone, cp);
		badlink2:
			vrele(ndp->ni_dvp);
			vput(ndp->ni_vp);
			break;
		}
		linklen = MAXPATHLEN - auio.uio_resid;
		if (linklen == 0) {
			error = ENOENT;
			goto badlink1;
		}
		if (linklen + ndp->ni_pathlen >= MAXPATHLEN) {
			error = ENAMETOOLONG;
			goto badlink1;
		}

		/*
		 * Adjust or replace path
		 */
		if (ndp->ni_pathlen > 1) {
			NFSBCOPY(ndp->ni_next, cp + linklen, ndp->ni_pathlen);
			uma_zfree(namei_zone, cnp->cn_pnbuf);
			cnp->cn_pnbuf = cp;
		} else
			cnp->cn_pnbuf[linklen] = '\0';
		ndp->ni_pathlen += linklen;

		/*
		 * Cleanup refs for next loop and check if root directory
		 * should replace current directory.  Normally ni_dvp
		 * becomes the new base directory and is cleaned up when
		 * we loop.  Explicitly null pointers after invalidation
		 * to clarify operation.
		 */
		vput(ndp->ni_vp);
		ndp->ni_vp = NULL;

		if (cnp->cn_pnbuf[0] == '/') {
			vrele(ndp->ni_dvp);
			ndp->ni_dvp = ndp->ni_rootdir;
			VREF(ndp->ni_dvp);
		}
		ndp->ni_startdir = ndp->ni_dvp;
		ndp->ni_dvp = NULL;
	}
	if (!lockleaf)
		cnp->cn_flags &= ~LOCKLEAF;

out:
	if (error) {
		uma_zfree(namei_zone, cnp->cn_pnbuf);
		ndp->ni_vp = NULL;
		ndp->ni_dvp = NULL;
		ndp->ni_startdir = NULL;
		cnp->cn_flags &= ~HASBUF;
	} else if ((ndp->ni_cnd.cn_flags & (WANTPARENT|LOCKPARENT)) == 0) {
		ndp->ni_dvp = NULL;
	}
	return (error);
}

/*
 * Set up a pathname buffer and return a pointer to it and, optionally
 * set a hash pointer.
 */
void
nfsvno_setpathbuf(struct nameidata *ndp, char **bufpp, u_long **hashpp)
{
	struct componentname *cnp = &ndp->ni_cnd;

	cnp->cn_flags |= (NOMACCHECK | HASBUF);
	cnp->cn_pnbuf = uma_zalloc(namei_zone, M_WAITOK);
	if (hashpp != NULL)
		*hashpp = NULL;
	*bufpp = cnp->cn_pnbuf;
}

/*
 * Release the above path buffer, if not released by nfsvno_namei().
 */
void
nfsvno_relpathbuf(struct nameidata *ndp)
{

	if ((ndp->ni_cnd.cn_flags & HASBUF) == 0)
		panic("nfsrelpath");
	uma_zfree(namei_zone, ndp->ni_cnd.cn_pnbuf);
	ndp->ni_cnd.cn_flags &= ~HASBUF;
}

/*
 * Readlink vnode op into an mbuf list.
 */
int
nfsvno_readlink(struct vnode *vp, struct ucred *cred, struct thread *p,
    struct mbuf **mpp, struct mbuf **mpendp, int *lenp)
{
	struct iovec iv[(NFS_MAXPATHLEN+MLEN-1)/MLEN];
	struct iovec *ivp = iv;
	struct uio io, *uiop = &io;
	struct mbuf *mp, *mp2 = NULL, *mp3 = NULL;
	int i, len, tlen, error;

	len = 0;
	i = 0;
	while (len < NFS_MAXPATHLEN) {
		NFSMGET(mp);
		MCLGET(mp, M_WAIT);
		mp->m_len = NFSMSIZ(mp);
		if (len == 0) {
			mp3 = mp2 = mp;
		} else {
			mp2->m_next = mp;
			mp2 = mp;
		}
		if ((len + mp->m_len) > NFS_MAXPATHLEN) {
			mp->m_len = NFS_MAXPATHLEN - len;
			len = NFS_MAXPATHLEN;
		} else {
			len += mp->m_len;
		}
		ivp->iov_base = mtod(mp, caddr_t);
		ivp->iov_len = mp->m_len;
		i++;
		ivp++;
	}
	uiop->uio_iov = iv;
	uiop->uio_iovcnt = i;
	uiop->uio_offset = 0;
	uiop->uio_resid = len;
	uiop->uio_rw = UIO_READ;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_td = NULL;
	error = VOP_READLINK(vp, uiop, cred);
	if (error) {
		m_freem(mp3);
		*lenp = 0;
		return (error);
	}
	if (uiop->uio_resid > 0) {
		len -= uiop->uio_resid;
		tlen = NFSM_RNDUP(len);
		nfsrv_adj(mp3, NFS_MAXPATHLEN - tlen, tlen - len);
	}
	*lenp = len;
	*mpp = mp3;
	*mpendp = mp;
	return (0);
}

/*
 * Read vnode op call into mbuf list.
 */
int
nfsvno_read(struct vnode *vp, off_t off, int cnt, struct ucred *cred,
    struct thread *p, struct mbuf **mpp, struct mbuf **mpendp)
{
	struct mbuf *m;
	int i;
	struct iovec *iv;
	struct iovec *iv2;
	int error = 0, len, left, siz, tlen, ioflag = 0, hi, try = 32;
	struct mbuf *m2 = NULL, *m3;
	struct uio io, *uiop = &io;
	struct nfsheur *nh;

	/*
	 * Calculate seqcount for heuristic
	 */
	/*
	 * Locate best candidate
	 */

	hi = ((int)(vm_offset_t)vp / sizeof(struct vnode)) % NUM_HEURISTIC;
	nh = &nfsheur[hi];

	while (try--) {
		if (nfsheur[hi].nh_vp == vp) {
			nh = &nfsheur[hi];
			break;
		}
		if (nfsheur[hi].nh_use > 0)
			--nfsheur[hi].nh_use;
		hi = (hi + 1) % NUM_HEURISTIC;
		if (nfsheur[hi].nh_use < nh->nh_use)
			nh = &nfsheur[hi];
	}

	if (nh->nh_vp != vp) {
		nh->nh_vp = vp;
		nh->nh_nextr = off;
		nh->nh_use = NHUSE_INIT;
		if (off == 0)
			nh->nh_seqcount = 4;
		else
			nh->nh_seqcount = 1;
	}

	/*
	 * Calculate heuristic
	 */

	if ((off == 0 && nh->nh_seqcount > 0) || off == nh->nh_nextr) {
		if (++nh->nh_seqcount > IO_SEQMAX)
			nh->nh_seqcount = IO_SEQMAX;
	} else if (nh->nh_seqcount > 1) {
		nh->nh_seqcount = 1;
	} else {
		nh->nh_seqcount = 0;
	}
	nh->nh_use += NHUSE_INC;
	if (nh->nh_use > NHUSE_MAX)
		nh->nh_use = NHUSE_MAX;
	ioflag |= nh->nh_seqcount << IO_SEQSHIFT;

	len = left = NFSM_RNDUP(cnt);
	m3 = NULL;
	/*
	 * Generate the mbuf list with the uio_iov ref. to it.
	 */
	i = 0;
	while (left > 0) {
		NFSMGET(m);
		MCLGET(m, M_WAIT);
		m->m_len = 0;
		siz = min(M_TRAILINGSPACE(m), left);
		left -= siz;
		i++;
		if (m3)
			m2->m_next = m;
		else
			m3 = m;
		m2 = m;
	}
	MALLOC(iv, struct iovec *, i * sizeof (struct iovec),
	    M_TEMP, M_WAITOK);
	uiop->uio_iov = iv2 = iv;
	m = m3;
	left = len;
	i = 0;
	while (left > 0) {
		if (m == NULL)
			panic("nfsvno_read iov");
		siz = min(M_TRAILINGSPACE(m), left);
		if (siz > 0) {
			iv->iov_base = mtod(m, caddr_t) + m->m_len;
			iv->iov_len = siz;
			m->m_len += siz;
			left -= siz;
			iv++;
			i++;
		}
		m = m->m_next;
	}
	uiop->uio_iovcnt = i;
	uiop->uio_offset = off;
	uiop->uio_resid = len;
	uiop->uio_rw = UIO_READ;
	uiop->uio_segflg = UIO_SYSSPACE;
	error = VOP_READ(vp, uiop, IO_NODELOCKED | ioflag, cred);
	FREE((caddr_t)iv2, M_TEMP);
	if (error) {
		m_freem(m3);
		*mpp = NULL;
		return (error);
	}
	tlen = len - uiop->uio_resid;
	cnt = cnt < tlen ? cnt : tlen;
	tlen = NFSM_RNDUP(cnt);
	if (tlen == 0) {
		m_freem(m3);
		m3 = NULL;
	} else if (len != tlen || tlen != cnt)
		nfsrv_adj(m3, len - tlen, tlen - cnt);
	*mpp = m3;
	*mpendp = m2;
	return (0);
}

/*
 * Write vnode op from an mbuf list.
 */
int
nfsvno_write(struct vnode *vp, off_t off, int retlen, int cnt, int stable,
    struct mbuf *mp, char *cp, struct ucred *cred, struct thread *p)
{
	struct iovec *ivp;
	int i, len;
	struct iovec *iv;
	int ioflags, error;
	struct uio io, *uiop = &io;

	MALLOC(ivp, struct iovec *, cnt * sizeof (struct iovec), M_TEMP,
	    M_WAITOK);
	uiop->uio_iov = iv = ivp;
	uiop->uio_iovcnt = cnt;
	i = mtod(mp, caddr_t) + mp->m_len - cp;
	len = retlen;
	while (len > 0) {
		if (mp == NULL)
			panic("nfsvno_write");
		if (i > 0) {
			i = min(i, len);
			ivp->iov_base = cp;
			ivp->iov_len = i;
			ivp++;
			len -= i;
		}
		mp = mp->m_next;
		if (mp) {
			i = mp->m_len;
			cp = mtod(mp, caddr_t);
		}
	}

	if (stable == NFSWRITE_UNSTABLE)
		ioflags = IO_NODELOCKED;
	else
		ioflags = (IO_SYNC | IO_NODELOCKED);
	uiop->uio_resid = retlen;
	uiop->uio_rw = UIO_WRITE;
	uiop->uio_segflg = UIO_SYSSPACE;
	NFSUIOPROC(uiop, p);
	uiop->uio_offset = off;
	error = VOP_WRITE(vp, uiop, ioflags, cred);
	FREE((caddr_t)iv, M_TEMP);
	return (error);
}

/*
 * Common code for creating a regular file (plus special files for V2).
 */
int
nfsvno_createsub(struct nfsrv_descript *nd, struct nameidata *ndp,
    struct vnode **vpp, struct nfsvattr *nvap, int *exclusive_flagp,
    int32_t *cverf, NFSDEV_T rdev, struct thread *p, struct nfsexstuff *exp)
{
	u_quad_t tempsize;
	int error;

	error = nd->nd_repstat;
	if (!error && ndp->ni_vp == NULL) {
		if (nvap->na_type == VREG || nvap->na_type == VSOCK) {
			vrele(ndp->ni_startdir);
			error = VOP_CREATE(ndp->ni_dvp,
			    &ndp->ni_vp, &ndp->ni_cnd, &nvap->na_vattr);
			vput(ndp->ni_dvp);
			nfsvno_relpathbuf(ndp);
			if (!error) {
				if (*exclusive_flagp) {
					*exclusive_flagp = 0;
					NFSVNO_ATTRINIT(nvap);
					nvap->na_atime.tv_sec = cverf[0];
					nvap->na_atime.tv_nsec = cverf[1];
					error = VOP_SETATTR(ndp->ni_vp,
					    &nvap->na_vattr, nd->nd_cred);
				}
			}
		/*
		 * NFS V2 Only. nfsrvd_mknod() does this for V3.
		 * (This implies, just get out on an error.)
		 */
		} else if (nvap->na_type == VCHR || nvap->na_type == VBLK ||
			nvap->na_type == VFIFO) {
			if (nvap->na_type == VCHR && rdev == 0xffffffff)
				nvap->na_type = VFIFO;
                        if (nvap->na_type != VFIFO &&
			    (error = priv_check_cred(nd->nd_cred,
			     PRIV_VFS_MKNOD_DEV, 0))) {
				vrele(ndp->ni_startdir);
				nfsvno_relpathbuf(ndp);
				vput(ndp->ni_dvp);
				return (error);
			}
			nvap->na_rdev = rdev;
			error = VOP_MKNOD(ndp->ni_dvp, &ndp->ni_vp,
			    &ndp->ni_cnd, &nvap->na_vattr);
			vput(ndp->ni_dvp);
			nfsvno_relpathbuf(ndp);
			if (error) {
				vrele(ndp->ni_startdir);
				return (error);
			}
		} else {
			vrele(ndp->ni_startdir);
			nfsvno_relpathbuf(ndp);
			vput(ndp->ni_dvp);
			return (ENXIO);
		}
		*vpp = ndp->ni_vp;
	} else {
		/*
		 * Handle cases where error is already set and/or
		 * the file exists.
		 * 1 - clean up the lookup
		 * 2 - iff !error and na_size set, truncate it
		 */
		vrele(ndp->ni_startdir);
		nfsvno_relpathbuf(ndp);
		*vpp = ndp->ni_vp;
		if (ndp->ni_dvp == *vpp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		if (!error && nvap->na_size != VNOVAL) {
			error = nfsvno_accchk(*vpp, VWRITE,
			    nd->nd_cred, exp, p, NFSACCCHK_NOOVERRIDE,
			    NFSACCCHK_VPISLOCKED, NULL);
			if (!error) {
				tempsize = nvap->na_size;
				NFSVNO_ATTRINIT(nvap);
				nvap->na_size = tempsize;
				error = VOP_SETATTR(*vpp,
				    &nvap->na_vattr, nd->nd_cred);
			}
		}
		if (error)
			vput(*vpp);
	}
	return (error);
}

/*
 * Do a mknod vnode op.
 */
int
nfsvno_mknod(struct nameidata *ndp, struct nfsvattr *nvap, struct ucred *cred,
    struct thread *p)
{
	int error = 0;
	enum vtype vtyp;

	vtyp = nvap->na_type;
	/*
	 * Iff doesn't exist, create it.
	 */
	if (ndp->ni_vp) {
		vrele(ndp->ni_startdir);
		nfsvno_relpathbuf(ndp);
		vput(ndp->ni_dvp);
		vrele(ndp->ni_vp);
		return (EEXIST);
	}
	if (vtyp != VCHR && vtyp != VBLK && vtyp != VSOCK && vtyp != VFIFO) {
		vrele(ndp->ni_startdir);
		nfsvno_relpathbuf(ndp);
		vput(ndp->ni_dvp);
		return (NFSERR_BADTYPE);
	}
	if (vtyp == VSOCK) {
		vrele(ndp->ni_startdir);
		error = VOP_CREATE(ndp->ni_dvp, &ndp->ni_vp,
		    &ndp->ni_cnd, &nvap->na_vattr);
		vput(ndp->ni_dvp);
		nfsvno_relpathbuf(ndp);
	} else {
		if (nvap->na_type != VFIFO &&
		    (error = priv_check_cred(cred, PRIV_VFS_MKNOD_DEV, 0))) {
			vrele(ndp->ni_startdir);
			nfsvno_relpathbuf(ndp);
			vput(ndp->ni_dvp);
			return (error);
		}
		error = VOP_MKNOD(ndp->ni_dvp, &ndp->ni_vp,
		    &ndp->ni_cnd, &nvap->na_vattr);
		vput(ndp->ni_dvp);
		nfsvno_relpathbuf(ndp);
		if (error)
			vrele(ndp->ni_startdir);
		/*
		 * Since VOP_MKNOD returns the ni_vp, I can't
		 * see any reason to do the lookup.
		 */
	}
	return (error);
}

/*
 * Mkdir vnode op.
 */
int
nfsvno_mkdir(struct nameidata *ndp, struct nfsvattr *nvap, uid_t saved_uid,
    struct ucred *cred, struct thread *p, struct nfsexstuff *exp)
{
	int error = 0;

	if (ndp->ni_vp != NULL) {
		if (ndp->ni_dvp == ndp->ni_vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vrele(ndp->ni_vp);
		nfsvno_relpathbuf(ndp);
		return (EEXIST);
	}
	error = VOP_MKDIR(ndp->ni_dvp, &ndp->ni_vp, &ndp->ni_cnd,
	    &nvap->na_vattr);
	vput(ndp->ni_dvp);
	nfsvno_relpathbuf(ndp);
	return (error);
}

/*
 * symlink vnode op.
 */
int
nfsvno_symlink(struct nameidata *ndp, struct nfsvattr *nvap, char *pathcp,
    int pathlen, int not_v2, uid_t saved_uid, struct ucred *cred, struct thread *p,
    struct nfsexstuff *exp)
{
	int error = 0;

	if (ndp->ni_vp) {
		vrele(ndp->ni_startdir);
		nfsvno_relpathbuf(ndp);
		if (ndp->ni_dvp == ndp->ni_vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vrele(ndp->ni_vp);
		return (EEXIST);
	}

	error = VOP_SYMLINK(ndp->ni_dvp, &ndp->ni_vp, &ndp->ni_cnd,
	    &nvap->na_vattr, pathcp);
	vput(ndp->ni_dvp);
	vrele(ndp->ni_startdir);
	nfsvno_relpathbuf(ndp);
	/*
	 * Although FreeBSD still had the lookup code in
	 * it for 7/current, there doesn't seem to be any
	 * point, since VOP_SYMLINK() returns the ni_vp.
	 * Just vput it for v2.
	 */
	if (!not_v2 && !error)
		vput(ndp->ni_vp);
	return (error);
}

/*
 * Parse symbolic link arguments.
 * This function has an ugly side effect. It will MALLOC() an area for
 * the symlink and set iov_base to point to it, only if it succeeds.
 * So, if it returns with uiop->uio_iov->iov_base != NULL, that must
 * be FREE'd later.
 */
int
nfsvno_getsymlink(struct nfsrv_descript *nd, struct nfsvattr *nvap,
    struct thread *p, char **pathcpp, int *lenp)
{
	u_int32_t *tl;
	char *pathcp = NULL;
	int error = 0, len;
	struct nfsv2_sattr *sp;

	*pathcpp = NULL;
	*lenp = 0;
	if ((nd->nd_flag & ND_NFSV3) &&
	    (error = nfsrv_sattr(nd, nvap, NULL, NULL, p)))
		goto nfsmout;
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	len = fxdr_unsigned(int, *tl);
	if (len > NFS_MAXPATHLEN || len <= 0) {
		error = EBADRPC;
		goto nfsmout;
	}
	MALLOC(pathcp, caddr_t, len + 1, M_TEMP, M_WAITOK);
	error = nfsrv_mtostr(nd, pathcp, len);
	if (error)
		goto nfsmout;
	if (nd->nd_flag & ND_NFSV2) {
		NFSM_DISSECT(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		nvap->na_mode = fxdr_unsigned(u_int16_t, sp->sa_mode);
	}
	*pathcpp = pathcp;
	*lenp = len;
	return (0);
nfsmout:
	if (pathcp)
		free(pathcp, M_TEMP);
	return (error);
}

/*
 * Remove a non-directory object.
 */
int
nfsvno_removesub(struct nameidata *ndp, int is_v4, struct ucred *cred,
    struct thread *p, struct nfsexstuff *exp)
{
	struct vnode *vp;
	int error = 0;

	vp = ndp->ni_vp;
	if (vp->v_type == VDIR)
		error = NFSERR_ISDIR;
	else if (is_v4)
		error = nfsrv_checkremove(vp, 1, p);
	if (!error)
		error = VOP_REMOVE(ndp->ni_dvp, vp, &ndp->ni_cnd);
	if (ndp->ni_dvp == vp)
		vrele(ndp->ni_dvp);
	else
		vput(ndp->ni_dvp);
	vput(vp);
	return (error);
}

/*
 * Remove a directory.
 */
int
nfsvno_rmdirsub(struct nameidata *ndp, int is_v4, struct ucred *cred,
    struct thread *p, struct nfsexstuff *exp)
{
	struct vnode *vp;
	int error = 0;

	vp = ndp->ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (ndp->ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_vflag & VV_ROOT)
		error = EBUSY;
out:
	if (!error)
		error = VOP_RMDIR(ndp->ni_dvp, vp, &ndp->ni_cnd);
	if (ndp->ni_dvp == vp)
		vrele(ndp->ni_dvp);
	else
		vput(ndp->ni_dvp);
	vput(vp);
	return (error);
}

/*
 * Rename vnode op.
 */
int
nfsvno_rename(struct nameidata *fromndp, struct nameidata *tondp,
    u_int32_t ndstat, u_int32_t ndflag, struct ucred *cred, struct thread *p)
{
	struct vnode *fvp, *tvp, *tdvp;
	int error = 0;

	fvp = fromndp->ni_vp;
	if (ndstat) {
		vrele(fromndp->ni_dvp);
		vrele(fvp);
		error = ndstat;
		goto out1;
	}
	tdvp = tondp->ni_dvp;
	tvp = tondp->ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = (ndflag & ND_NFSV2) ? EISDIR : EEXIST;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = (ndflag & ND_NFSV2) ? ENOTDIR : EEXIST;
			goto out;
		}
		if (tvp->v_type == VDIR && tvp->v_mountedhere) {
			error = (ndflag & ND_NFSV2) ? ENOTEMPTY : EXDEV;
			goto out;
		}

		/*
		 * A rename to '.' or '..' results in a prematurely
		 * unlocked vnode on FreeBSD5, so I'm just going to fail that
		 * here.
		 */
		if ((tondp->ni_cnd.cn_namelen == 1 &&
		     tondp->ni_cnd.cn_nameptr[0] == '.') ||
		    (tondp->ni_cnd.cn_namelen == 2 &&
		     tondp->ni_cnd.cn_nameptr[0] == '.' &&
		     tondp->ni_cnd.cn_nameptr[1] == '.')) {
			error = EINVAL;
			goto out;
		}
	}
	if (fvp->v_type == VDIR && fvp->v_mountedhere) {
		error = (ndflag & ND_NFSV2) ? ENOTEMPTY : EXDEV;
		goto out;
	}
	if (fvp->v_mount != tdvp->v_mount) {
		error = (ndflag & ND_NFSV2) ? ENOTEMPTY : EXDEV;
		goto out;
	}
	if (fvp == tdvp) {
		error = (ndflag & ND_NFSV2) ? ENOTEMPTY : EINVAL;
		goto out;
	}
	if (fvp == tvp) {
		/*
		 * If source and destination are the same, there is nothing to
		 * do. Set error to -1 to indicate this.
		 */
		error = -1;
		goto out;
	}
	if (ndflag & ND_NFSV4) {
		if (vn_lock(fvp, LK_EXCLUSIVE) == 0) {
			error = nfsrv_checkremove(fvp, 0, p);
			VOP_UNLOCK(fvp, 0);
		} else
			error = EPERM;
		if (tvp && !error)
			error = nfsrv_checkremove(tvp, 1, p);
	} else {
		/*
		 * For NFSv2 and NFSv3, try to get rid of the delegation, so
		 * that the NFSv4 client won't be confused by the rename.
		 * Since nfsd_recalldelegation() can only be called on an
		 * unlocked vnode at this point and fvp is the file that will
		 * still exist after the rename, just do fvp.
		 */
		nfsd_recalldelegation(fvp, p);
	}
out:
	if (!error) {
		error = VOP_RENAME(fromndp->ni_dvp, fromndp->ni_vp,
		    &fromndp->ni_cnd, tondp->ni_dvp, tondp->ni_vp,
		    &tondp->ni_cnd);
	} else {
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		vrele(fromndp->ni_dvp);
		vrele(fvp);
		if (error == -1)
			error = 0;
	}
	vrele(tondp->ni_startdir);
	nfsvno_relpathbuf(tondp);
out1:
	vrele(fromndp->ni_startdir);
	nfsvno_relpathbuf(fromndp);
	return (error);
}

/*
 * Link vnode op.
 */
int
nfsvno_link(struct nameidata *ndp, struct vnode *vp, struct ucred *cred,
    struct thread *p, struct nfsexstuff *exp)
{
	struct vnode *xp;
	int error = 0;

	xp = ndp->ni_vp;
	if (xp != NULL) {
		error = EEXIST;
	} else {
		xp = ndp->ni_dvp;
		if (vp->v_mount != xp->v_mount)
			error = EXDEV;
	}
	if (!error) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		if ((vp->v_iflag & VI_DOOMED) == 0)
			error = VOP_LINK(ndp->ni_dvp, vp, &ndp->ni_cnd);
		else
			error = EPERM;
		if (ndp->ni_dvp == vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		VOP_UNLOCK(vp, 0);
	} else {
		if (ndp->ni_dvp == ndp->ni_vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		if (ndp->ni_vp)
			vrele(ndp->ni_vp);
	}
	nfsvno_relpathbuf(ndp);
	return (error);
}

/*
 * Do the fsync() appropriate for the commit.
 */
int
nfsvno_fsync(struct vnode *vp, u_int64_t off, int cnt, struct ucred *cred,
    struct thread *td)
{
	int error = 0;

	if (cnt > MAX_COMMIT_COUNT) {
		/*
		 * Give up and do the whole thing
		 */
		if (vp->v_object &&
		   (vp->v_object->flags & OBJ_MIGHTBEDIRTY)) {
			VM_OBJECT_LOCK(vp->v_object);
			vm_object_page_clean(vp->v_object, 0, 0, OBJPC_SYNC);
			VM_OBJECT_UNLOCK(vp->v_object);
		}
		error = VOP_FSYNC(vp, MNT_WAIT, td);
	} else {
		/*
		 * Locate and synchronously write any buffers that fall
		 * into the requested range.  Note:  we are assuming that
		 * f_iosize is a power of 2.
		 */
		int iosize = vp->v_mount->mnt_stat.f_iosize;
		int iomask = iosize - 1;
		struct bufobj *bo;
		daddr_t lblkno;

		/*
		 * Align to iosize boundry, super-align to page boundry.
		 */
		if (off & iomask) {
			cnt += off & iomask;
			off &= ~(u_quad_t)iomask;
		}
		if (off & PAGE_MASK) {
			cnt += off & PAGE_MASK;
			off &= ~(u_quad_t)PAGE_MASK;
		}
		lblkno = off / iosize;

		if (vp->v_object &&
		   (vp->v_object->flags & OBJ_MIGHTBEDIRTY)) {
			VM_OBJECT_LOCK(vp->v_object);
			vm_object_page_clean(vp->v_object, off / PAGE_SIZE, (cnt + PAGE_MASK) / PAGE_SIZE, OBJPC_SYNC);
			VM_OBJECT_UNLOCK(vp->v_object);
		}

		bo = &vp->v_bufobj;
		BO_LOCK(bo);
		while (cnt > 0) {
			struct buf *bp;

			/*
			 * If we have a buffer and it is marked B_DELWRI we
			 * have to lock and write it.  Otherwise the prior
			 * write is assumed to have already been committed.
			 *
			 * gbincore() can return invalid buffers now so we
			 * have to check that bit as well (though B_DELWRI
			 * should not be set if B_INVAL is set there could be
			 * a race here since we haven't locked the buffer).
			 */
			if ((bp = gbincore(&vp->v_bufobj, lblkno)) != NULL) {
				if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL |
				    LK_INTERLOCK, BO_MTX(bo)) == ENOLCK) {
					BO_LOCK(bo);
					continue; /* retry */
				}
			    	if ((bp->b_flags & (B_DELWRI|B_INVAL)) ==
				    B_DELWRI) {
					bremfree(bp);
					bp->b_flags &= ~B_ASYNC;
					bwrite(bp);
					++nfs_commit_miss;
				} else
					BUF_UNLOCK(bp);
				BO_LOCK(bo);
			}
			++nfs_commit_blks;
			if (cnt < iosize)
				break;
			cnt -= iosize;
			++lblkno;
		}
		BO_UNLOCK(bo);
	}
	return (error);
}

/*
 * Statfs vnode op.
 */
int
nfsvno_statfs(struct vnode *vp, struct statfs *sf)
{

	return (VFS_STATFS(vp->v_mount, sf));
}

/*
 * Do the vnode op stuff for Open. Similar to nfsvno_createsub(), but
 * must handle nfsrv_opencheck() calls after any other access checks.
 */
void
nfsvno_open(struct nfsrv_descript *nd, struct nameidata *ndp,
    nfsquad_t clientid, nfsv4stateid_t *stateidp, struct nfsstate *stp,
    int *exclusive_flagp, struct nfsvattr *nvap, int32_t *cverf, int create,
    NFSACL_T *aclp, nfsattrbit_t *attrbitp, struct ucred *cred, struct thread *p,
    struct nfsexstuff *exp, struct vnode **vpp)
{
	struct vnode *vp = NULL;
	u_quad_t tempsize;
	struct nfsexstuff nes;

	if (ndp->ni_vp == NULL)
		nd->nd_repstat = nfsrv_opencheck(clientid,
		    stateidp, stp, NULL, nd, p, nd->nd_repstat);
	if (!nd->nd_repstat) {
		if (ndp->ni_vp == NULL) {
			vrele(ndp->ni_startdir);
			nd->nd_repstat = VOP_CREATE(ndp->ni_dvp,
			    &ndp->ni_vp, &ndp->ni_cnd, &nvap->na_vattr);
			vput(ndp->ni_dvp);
			nfsvno_relpathbuf(ndp);
			if (!nd->nd_repstat) {
				if (*exclusive_flagp) {
					*exclusive_flagp = 0;
					NFSVNO_ATTRINIT(nvap);
					nvap->na_atime.tv_sec = cverf[0];
					nvap->na_atime.tv_nsec = cverf[1];
					nd->nd_repstat = VOP_SETATTR(ndp->ni_vp,
					    &nvap->na_vattr, cred);
				} else {
					nfsrv_fixattr(nd, ndp->ni_vp, nvap,
					    aclp, p, attrbitp, exp);
				}
			}
			vp = ndp->ni_vp;
		} else {
			if (ndp->ni_startdir)
				vrele(ndp->ni_startdir);
			nfsvno_relpathbuf(ndp);
			vp = ndp->ni_vp;
			if (create == NFSV4OPEN_CREATE) {
				if (ndp->ni_dvp == vp)
					vrele(ndp->ni_dvp);
				else
					vput(ndp->ni_dvp);
			}
			if (NFSVNO_ISSETSIZE(nvap) && vp->v_type == VREG) {
				if (ndp->ni_cnd.cn_flags & RDONLY)
					NFSVNO_SETEXRDONLY(&nes);
				else
					NFSVNO_EXINIT(&nes);
				nd->nd_repstat = nfsvno_accchk(vp, 
				    VWRITE, cred, &nes, p,
				    NFSACCCHK_NOOVERRIDE,
				    NFSACCCHK_VPISLOCKED, NULL);
				nd->nd_repstat = nfsrv_opencheck(clientid,
				    stateidp, stp, vp, nd, p, nd->nd_repstat);
				if (!nd->nd_repstat) {
					tempsize = nvap->na_size;
					NFSVNO_ATTRINIT(nvap);
					nvap->na_size = tempsize;
					nd->nd_repstat = VOP_SETATTR(vp,
					    &nvap->na_vattr, cred);
				}
			} else if (vp->v_type == VREG) {
				nd->nd_repstat = nfsrv_opencheck(clientid,
				    stateidp, stp, vp, nd, p, nd->nd_repstat);
			}
		}
	} else {
		if (ndp->ni_cnd.cn_flags & HASBUF)
			nfsvno_relpathbuf(ndp);
		if (ndp->ni_startdir && create == NFSV4OPEN_CREATE) {
			vrele(ndp->ni_startdir);
			if (ndp->ni_dvp == ndp->ni_vp)
				vrele(ndp->ni_dvp);
			else
				vput(ndp->ni_dvp);
			if (ndp->ni_vp)
				vput(ndp->ni_vp);
		}
	}
	*vpp = vp;
}

/*
 * Updates the file rev and sets the mtime and ctime
 * to the current clock time, returning the va_filerev and va_Xtime
 * values.
 */
void
nfsvno_updfilerev(struct vnode *vp, struct nfsvattr *nvap,
    struct ucred *cred, struct thread *p)
{
	struct vattr va;

	VATTR_NULL(&va);
	getnanotime(&va.va_mtime);
	(void) VOP_SETATTR(vp, &va, cred);
	(void) nfsvno_getattr(vp, nvap, cred, p, 1);
}

/*
 * Glue routine to nfsv4_fillattr().
 */
int
nfsvno_fillattr(struct nfsrv_descript *nd, struct vnode *vp,
    struct nfsvattr *nvap, fhandle_t *fhp, int rderror, nfsattrbit_t *attrbitp,
    struct ucred *cred, struct thread *p, int isdgram, int reterr)
{
	int error;

	error = nfsv4_fillattr(nd, vp, NULL, &nvap->na_vattr, fhp, rderror,
	    attrbitp, cred, p, isdgram, reterr);
	return (error);
}

/* Since the Readdir vnode ops vary, put the entire functions in here. */
/*
 * nfs readdir service
 * - mallocs what it thinks is enough to read
 *	count rounded up to a multiple of DIRBLKSIZ <= NFS_MAXREADDIR
 * - calls VOP_READDIR()
 * - loops around building the reply
 *	if the output generated exceeds count break out of loop
 *	The NFSM_CLGET macro is used here so that the reply will be packed
 *	tightly in mbuf clusters.
 * - it trims out records with d_fileno == 0
 *	this doesn't matter for Unix clients, but they might confuse clients
 *	for other os'.
 * - it trims out records with d_type == DT_WHT
 *	these cannot be seen through NFS (unless we extend the protocol)
 *     The alternate call nfsrvd_readdirplus() does lookups as well.
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */
int
nfsrvd_readdir(struct nfsrv_descript *nd, int isdgram,
    struct vnode *vp, struct thread *p, struct nfsexstuff *exp)
{
	struct dirent *dp;
	u_int32_t *tl;
	int dirlen;
	char *cpos, *cend, *rbuf;
	struct nfsvattr at;
	int nlen, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, ncookies;
	u_int64_t off, toff, verf;
	u_long *cookies = NULL, *cookiep;
	struct uio io;
	struct iovec iv;
	int not_zfs;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &at);
		return (0);
	}
	if (nd->nd_flag & ND_NFSV2) {
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		off = fxdr_unsigned(u_quad_t, *tl++);
	} else {
		NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 2;
		verf = fxdr_hyper(tl);
		tl += 2;
	}
	toff = off;
	cnt = fxdr_unsigned(int, *tl);
	if (cnt > NFS_SRVMAXDATA(nd) || cnt < 0)
		cnt = NFS_SRVMAXDATA(nd);
	siz = ((cnt + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	fullsiz = siz;
	if (nd->nd_flag & ND_NFSV3) {
		nd->nd_repstat = getret = nfsvno_getattr(vp, &at, nd->nd_cred,
		    p, 1);
#if 0
		/*
		 * va_filerev is not sufficient as a cookie verifier,
		 * since it is not supposed to change when entries are
		 * removed/added unless that offset cookies returned to
		 * the client are no longer valid.
		 */
		if (!nd->nd_repstat && toff && verf != at.na_filerev)
			nd->nd_repstat = NFSERR_BAD_COOKIE;
#endif
	}
	if (nd->nd_repstat == 0 && cnt == 0) {
		if (nd->nd_flag & ND_NFSV2)
			/* NFSv2 does not have NFSERR_TOOSMALL */
			nd->nd_repstat = EPERM;
		else
			nd->nd_repstat = NFSERR_TOOSMALL;
	}
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_accchk(vp, VEXEC,
		    nd->nd_cred, exp, p, NFSACCCHK_NOOVERRIDE,
		    NFSACCCHK_VPISLOCKED, NULL);
	if (nd->nd_repstat) {
		vput(vp);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, getret, &at);
		return (0);
	}
	not_zfs = strcmp(vp->v_mount->mnt_vfc->vfc_name, "zfs");
	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
again:
	eofflag = 0;
	if (cookies) {
		free((caddr_t)cookies, M_TEMP);
		cookies = NULL;
	}

	iv.iov_base = rbuf;
	iv.iov_len = siz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = siz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_td = NULL;
	nd->nd_repstat = VOP_READDIR(vp, &io, nd->nd_cred, &eofflag, &ncookies,
	    &cookies);
	off = (u_int64_t)io.uio_offset;
	if (io.uio_resid)
		siz -= io.uio_resid;

	if (!cookies && !nd->nd_repstat)
		nd->nd_repstat = NFSERR_PERM;
	if (nd->nd_flag & ND_NFSV3) {
		getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 1);
		if (!nd->nd_repstat)
			nd->nd_repstat = getret;
	}

	/*
	 * Handles the failed cases. nd->nd_repstat == 0 past here.
	 */
	if (nd->nd_repstat) {
		vput(vp);
		free((caddr_t)rbuf, M_TEMP);
		if (cookies)
			free((caddr_t)cookies, M_TEMP);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, getret, &at);
		return (0);
	}
	/*
	 * If nothing read, return eof
	 * rpc reply
	 */
	if (siz == 0) {
		vput(vp);
		if (nd->nd_flag & ND_NFSV2) {
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		} else {
			nfsrv_postopattr(nd, getret, &at);
			NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			txdr_hyper(at.na_filerev, tl);
			tl += 2;
		}
		*tl++ = newnfs_false;
		*tl = newnfs_true;
		FREE((caddr_t)rbuf, M_TEMP);
		FREE((caddr_t)cookies, M_TEMP);
		return (0);
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;

	/*
	 * For some reason FreeBSD's ufs_readdir() chooses to back the
	 * directory offset up to a block boundary, so it is necessary to
	 * skip over the records that precede the requested offset. This
	 * requires the assumption that file offset cookies monotonically
	 * increase.
	 * Since the offset cookies don't monotonically increase for ZFS,
	 * this is not done when ZFS is the file system.
	 */
	while (cpos < cend && ncookies > 0 &&
	    (dp->d_fileno == 0 || dp->d_type == DT_WHT ||
	     (not_zfs != 0 && ((u_quad_t)(*cookiep)) <= toff))) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		siz = fullsiz;
		toff = off;
		goto again;
	}
	vput(vp);

	/*
	 * dirlen is the size of the reply, including all XDR and must
	 * not exceed cnt. For NFSv2, RFC1094 didn't clearly indicate
	 * if the XDR should be included in "count", but to be safe, we do.
	 * (Include the two booleans at the end of the reply in dirlen now.)
	 */
	if (nd->nd_flag & ND_NFSV3) {
		nfsrv_postopattr(nd, getret, &at);
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		txdr_hyper(at.na_filerev, tl);
		dirlen = NFSX_V3POSTOPATTR + NFSX_VERF + 2 * NFSX_UNSIGNED;
	} else {
		dirlen = 2 * NFSX_UNSIGNED;
	}

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		nlen = dp->d_namlen;
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT &&
			nlen <= NFS_MAXNAMLEN) {
			if (nd->nd_flag & ND_NFSV3)
				dirlen += (6*NFSX_UNSIGNED + NFSM_RNDUP(nlen));
			else
				dirlen += (4*NFSX_UNSIGNED + NFSM_RNDUP(nlen));
			if (dirlen > cnt) {
				eofflag = 0;
				break;
			}

			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			if (nd->nd_flag & ND_NFSV3) {
				NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
				*tl++ = newnfs_true;
				*tl++ = 0;
			} else {
				NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
				*tl++ = newnfs_true;
			}
			*tl = txdr_unsigned(dp->d_fileno);
			(void) nfsm_strtom(nd, dp->d_name, nlen);
			if (nd->nd_flag & ND_NFSV3) {
				NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
				*tl++ = 0;
			} else
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(*cookiep);
		}
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos < cend)
		eofflag = 0;
	NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = newnfs_false;
	if (eofflag)
		*tl = newnfs_true;
	else
		*tl = newnfs_false;
	FREE((caddr_t)rbuf, M_TEMP);
	FREE((caddr_t)cookies, M_TEMP);
	return (0);
nfsmout:
	vput(vp);
	return (error);
}

/*
 * Readdirplus for V3 and Readdir for V4.
 */
int
nfsrvd_readdirplus(struct nfsrv_descript *nd, int isdgram,
    struct vnode *vp, struct thread *p, struct nfsexstuff *exp)
{
	struct dirent *dp;
	u_int32_t *tl;
	int dirlen;
	char *cpos, *cend, *rbuf;
	struct vnode *nvp;
	fhandle_t nfh;
	struct nfsvattr nva, at, *nvap = &nva;
	struct mbuf *mb0, *mb1;
	struct nfsreferral *refp;
	int nlen, r, error = 0, getret = 1, usevget = 1;
	int siz, cnt, fullsiz, eofflag, ncookies, entrycnt;
	caddr_t bpos0, bpos1;
	u_int64_t off, toff, verf;
	u_long *cookies = NULL, *cookiep;
	nfsattrbit_t attrbits, rderrbits, savbits;
	struct uio io;
	struct iovec iv;
	struct componentname cn;
	int not_zfs;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &at);
		return (0);
	}
	NFSM_DISSECT(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
	off = fxdr_hyper(tl);
	toff = off;
	tl += 2;
	verf = fxdr_hyper(tl);
	tl += 2;
	siz = fxdr_unsigned(int, *tl++);
	cnt = fxdr_unsigned(int, *tl);

	/*
	 * Use the server's maximum data transfer size as the upper bound
	 * on reply datalen.
	 */
	if (cnt > NFS_SRVMAXDATA(nd) || cnt < 0)
		cnt = NFS_SRVMAXDATA(nd);

	/*
	 * siz is a "hint" of how much directory information (name, fileid,
	 * cookie) should be in the reply. At least one client "hints" 0,
	 * so I set it to cnt for that case. I also round it up to the
	 * next multiple of DIRBLKSIZ.
	 */
	if (siz <= 0)
		siz = cnt;
	siz = ((siz + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));

	if (nd->nd_flag & ND_NFSV4) {
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error)
			goto nfsmout;
		NFSSET_ATTRBIT(&savbits, &attrbits);
		NFSCLRNOTFILLABLE_ATTRBIT(&attrbits);
		NFSZERO_ATTRBIT(&rderrbits);
		NFSSETBIT_ATTRBIT(&rderrbits, NFSATTRBIT_RDATTRERROR);
	} else {
		NFSZERO_ATTRBIT(&attrbits);
	}
	fullsiz = siz;
	nd->nd_repstat = getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 1);
	if (!nd->nd_repstat) {
	    if (off && verf != at.na_filerev) {
		/*
		 * va_filerev is not sufficient as a cookie verifier,
		 * since it is not supposed to change when entries are
		 * removed/added unless that offset cookies returned to
		 * the client are no longer valid.
		 */
#if 0
		if (nd->nd_flag & ND_NFSV4) {
			nd->nd_repstat = NFSERR_NOTSAME;
		} else {
			nd->nd_repstat = NFSERR_BAD_COOKIE;
		}
#endif
	    } else if ((nd->nd_flag & ND_NFSV4) && off == 0 && verf != 0) {
		nd->nd_repstat = NFSERR_BAD_COOKIE;
	    }
	}
	if (!nd->nd_repstat && vp->v_type != VDIR)
		nd->nd_repstat = NFSERR_NOTDIR;
	if (!nd->nd_repstat && cnt == 0)
		nd->nd_repstat = NFSERR_TOOSMALL;
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_accchk(vp, VEXEC,
		    nd->nd_cred, exp, p, NFSACCCHK_NOOVERRIDE,
		    NFSACCCHK_VPISLOCKED, NULL);
	if (nd->nd_repstat) {
		vput(vp);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, getret, &at);
		return (0);
	}
	not_zfs = strcmp(vp->v_mount->mnt_vfc->vfc_name, "zfs");

	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
again:
	eofflag = 0;
	if (cookies) {
		free((caddr_t)cookies, M_TEMP);
		cookies = NULL;
	}

	iv.iov_base = rbuf;
	iv.iov_len = siz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = siz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_td = NULL;
	nd->nd_repstat = VOP_READDIR(vp, &io, nd->nd_cred, &eofflag, &ncookies,
	    &cookies);
	off = (u_int64_t)io.uio_offset;
	if (io.uio_resid)
		siz -= io.uio_resid;

	getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 1);

	if (!cookies && !nd->nd_repstat)
		nd->nd_repstat = NFSERR_PERM;
	if (!nd->nd_repstat)
		nd->nd_repstat = getret;
	if (nd->nd_repstat) {
		vput(vp);
		if (cookies)
			free((caddr_t)cookies, M_TEMP);
		free((caddr_t)rbuf, M_TEMP);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, getret, &at);
		return (0);
	}
	/*
	 * If nothing read, return eof
	 * rpc reply
	 */
	if (siz == 0) {
		vput(vp);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, getret, &at);
		NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		txdr_hyper(at.na_filerev, tl);
		tl += 2;
		*tl++ = newnfs_false;
		*tl = newnfs_true;
		free((caddr_t)cookies, M_TEMP);
		free((caddr_t)rbuf, M_TEMP);
		return (0);
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;

	/*
	 * For some reason FreeBSD's ufs_readdir() chooses to back the
	 * directory offset up to a block boundary, so it is necessary to
	 * skip over the records that precede the requested offset. This
	 * requires the assumption that file offset cookies monotonically
	 * increase.
	 * Since the offset cookies don't monotonically increase for ZFS,
	 * this is not done when ZFS is the file system.
	 */
	while (cpos < cend && ncookies > 0 &&
	  (dp->d_fileno == 0 || dp->d_type == DT_WHT ||
	   (not_zfs != 0 && ((u_quad_t)(*cookiep)) <= toff) ||
	   ((nd->nd_flag & ND_NFSV4) &&
	    ((dp->d_namlen == 1 && dp->d_name[0] == '.') ||
	     (dp->d_namlen==2 && dp->d_name[0]=='.' && dp->d_name[1]=='.'))))) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		siz = fullsiz;
		toff = off;
		goto again;
	}
	VOP_UNLOCK(vp, 0);

	/*
	 * Save this position, in case there is an error before one entry
	 * is created.
	 */
	mb0 = nd->nd_mb;
	bpos0 = nd->nd_bpos;

	/*
	 * Fill in the first part of the reply.
	 * dirlen is the reply length in bytes and cannot exceed cnt.
	 * (Include the two booleans at the end of the reply in dirlen now,
	 *  so we recognize when we have exceeded cnt.)
	 */
	if (nd->nd_flag & ND_NFSV3) {
		dirlen = NFSX_V3POSTOPATTR + NFSX_VERF + 2 * NFSX_UNSIGNED;
		nfsrv_postopattr(nd, getret, &at);
	} else {
		dirlen = NFSX_VERF + 2 * NFSX_UNSIGNED;
	}
	NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
	txdr_hyper(at.na_filerev, tl);

	/*
	 * Save this position, in case there is an empty reply needed.
	 */
	mb1 = nd->nd_mb;
	bpos1 = nd->nd_bpos;

	/* Loop through the records and build reply */
	entrycnt = 0;
	while (cpos < cend && ncookies > 0 && dirlen < cnt) {
		nlen = dp->d_namlen;
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT &&
		    nlen <= NFS_MAXNAMLEN &&
		    ((nd->nd_flag & ND_NFSV3) || nlen > 2 ||
		     (nlen==2 && (dp->d_name[0]!='.' || dp->d_name[1]!='.'))
		      || (nlen == 1 && dp->d_name[0] != '.'))) {
			/*
			 * Save the current position in the reply, in case
			 * this entry exceeds cnt.
			 */
			mb1 = nd->nd_mb;
			bpos1 = nd->nd_bpos;
	
			/*
			 * For readdir_and_lookup get the vnode using
			 * the file number.
			 */
			nvp = NULL;
			refp = NULL;
			r = 0;
			if ((nd->nd_flag & ND_NFSV3) ||
			    NFSNONZERO_ATTRBIT(&savbits)) {
				if (nd->nd_flag & ND_NFSV4)
					refp = nfsv4root_getreferral(NULL,
					    vp, dp->d_fileno);
				if (refp == NULL) {
					if (usevget)
						r = VFS_VGET(vp->v_mount,
						    dp->d_fileno, LK_SHARED,
						    &nvp);
					else
						r = EOPNOTSUPP;
					if (r == EOPNOTSUPP) {
						if (usevget) {
							usevget = 0;
							cn.cn_nameiop = LOOKUP;
							cn.cn_lkflags =
							    LK_SHARED |
							    LK_RETRY;
							cn.cn_cred =
							    nd->nd_cred;
							cn.cn_thread = p;
						}
						cn.cn_nameptr = dp->d_name;
						cn.cn_namelen = nlen;
						cn.cn_flags = ISLASTCN |
						    NOFOLLOW | LOCKLEAF |
						    MPSAFE;
						if (nlen == 2 &&
						    dp->d_name[0] == '.' &&
						    dp->d_name[1] == '.')
							cn.cn_flags |=
							    ISDOTDOT;
						if (vn_lock(vp, LK_SHARED)
						    != 0) {
							nd->nd_repstat = EPERM;
							break;
						}
						if ((vp->v_vflag & VV_ROOT) != 0
						    && (cn.cn_flags & ISDOTDOT)
						    != 0) {
							vref(vp);
							nvp = vp;
							r = 0;
						} else
							r = VOP_LOOKUP(vp, &nvp,
							    &cn);
					}
				}
				if (!r) {
				    if (refp == NULL &&
					((nd->nd_flag & ND_NFSV3) ||
					 NFSNONZERO_ATTRBIT(&attrbits))) {
					r = nfsvno_getfh(nvp, &nfh, p);
					if (!r)
					    r = nfsvno_getattr(nvp, nvap,
						nd->nd_cred, p, 1);
				    }
				} else {
				    nvp = NULL;
				}
				if (r) {
					if (!NFSISSET_ATTRBIT(&attrbits,
					    NFSATTRBIT_RDATTRERROR)) {
						if (nvp != NULL)
							vput(nvp);
						nd->nd_repstat = r;
						break;
					}
				}
			}

			/*
			 * Build the directory record xdr
			 */
			if (nd->nd_flag & ND_NFSV3) {
				NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
				*tl++ = newnfs_true;
				*tl++ = 0;
				*tl = txdr_unsigned(dp->d_fileno);
				dirlen += nfsm_strtom(nd, dp->d_name, nlen);
				NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
				*tl++ = 0;
				*tl = txdr_unsigned(*cookiep);
				nfsrv_postopattr(nd, 0, nvap);
				dirlen += nfsm_fhtom(nd,(u_int8_t *)&nfh,0,1);
				dirlen += (5*NFSX_UNSIGNED+NFSX_V3POSTOPATTR);
				if (nvp != NULL)
					vput(nvp);
			} else {
				NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
				*tl++ = newnfs_true;
				*tl++ = 0;
				*tl = txdr_unsigned(*cookiep);
				dirlen += nfsm_strtom(nd, dp->d_name, nlen);
				if (nvp != NULL)
					VOP_UNLOCK(nvp, 0);
				if (refp != NULL) {
					dirlen += nfsrv_putreferralattr(nd,
					    &savbits, refp, 0,
					    &nd->nd_repstat);
					if (nd->nd_repstat) {
						if (nvp != NULL)
							vrele(nvp);
						break;
					}
				} else if (r) {
					dirlen += nfsvno_fillattr(nd, nvp, nvap,
					    &nfh, r, &rderrbits, nd->nd_cred,
					    p, isdgram, 0);
				} else {
					dirlen += nfsvno_fillattr(nd, nvp, nvap,
					    &nfh, r, &attrbits, nd->nd_cred,
					    p, isdgram, 0);
				}
				if (nvp != NULL)
					vrele(nvp);
				dirlen += (3 * NFSX_UNSIGNED);
			}
			if (dirlen <= cnt)
				entrycnt++;
		}
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);

	/*
	 * If dirlen > cnt, we must strip off the last entry. If that
	 * results in an empty reply, report NFSERR_TOOSMALL.
	 */
	if (dirlen > cnt || nd->nd_repstat) {
		if (!nd->nd_repstat && entrycnt == 0)
			nd->nd_repstat = NFSERR_TOOSMALL;
		if (nd->nd_repstat)
			newnfs_trimtrailing(nd, mb0, bpos0);
		else
			newnfs_trimtrailing(nd, mb1, bpos1);
		eofflag = 0;
	} else if (cpos < cend)
		eofflag = 0;
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = newnfs_false;
		if (eofflag)
			*tl = newnfs_true;
		else
			*tl = newnfs_false;
	}
	FREE((caddr_t)cookies, M_TEMP);
	FREE((caddr_t)rbuf, M_TEMP);
	return (0);
nfsmout:
	vput(vp);
	return (error);
}

/*
 * Get the settable attributes out of the mbuf list.
 * (Return 0 or EBADRPC)
 */
int
nfsrv_sattr(struct nfsrv_descript *nd, struct nfsvattr *nvap,
    nfsattrbit_t *attrbitp, NFSACL_T *aclp, struct thread *p)
{
	u_int32_t *tl;
	struct nfsv2_sattr *sp;
	struct timeval curtime;
	int error = 0, toclient = 0;

	switch (nd->nd_flag & (ND_NFSV2 | ND_NFSV3 | ND_NFSV4)) {
	case ND_NFSV2:
		NFSM_DISSECT(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		/*
		 * Some old clients didn't fill in the high order 16bits.
		 * --> check the low order 2 bytes for 0xffff
		 */
		if ((fxdr_unsigned(int, sp->sa_mode) & 0xffff) != 0xffff)
			nvap->na_mode = nfstov_mode(sp->sa_mode);
		if (sp->sa_uid != newnfs_xdrneg1)
			nvap->na_uid = fxdr_unsigned(uid_t, sp->sa_uid);
		if (sp->sa_gid != newnfs_xdrneg1)
			nvap->na_gid = fxdr_unsigned(gid_t, sp->sa_gid);
		if (sp->sa_size != newnfs_xdrneg1)
			nvap->na_size = fxdr_unsigned(u_quad_t, sp->sa_size);
		if (sp->sa_atime.nfsv2_sec != newnfs_xdrneg1) {
#ifdef notyet
			fxdr_nfsv2time(&sp->sa_atime, &nvap->na_atime);
#else
			nvap->na_atime.tv_sec =
				fxdr_unsigned(u_int32_t,sp->sa_atime.nfsv2_sec);
			nvap->na_atime.tv_nsec = 0;
#endif
		}
		if (sp->sa_mtime.nfsv2_sec != newnfs_xdrneg1)
			fxdr_nfsv2time(&sp->sa_mtime, &nvap->na_mtime);
		break;
	case ND_NFSV3:
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		if (*tl == newnfs_true) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			nvap->na_mode = nfstov_mode(*tl);
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		if (*tl == newnfs_true) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			nvap->na_uid = fxdr_unsigned(uid_t, *tl);
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		if (*tl == newnfs_true) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			nvap->na_gid = fxdr_unsigned(gid_t, *tl);
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		if (*tl == newnfs_true) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			nvap->na_size = fxdr_hyper(tl);
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		switch (fxdr_unsigned(int, *tl)) {
		case NFSV3SATTRTIME_TOCLIENT:
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &nvap->na_atime);
			toclient = 1;
			break;
		case NFSV3SATTRTIME_TOSERVER:
			NFSGETTIME(&curtime);
			nvap->na_atime.tv_sec = curtime.tv_sec;
			nvap->na_atime.tv_nsec = curtime.tv_usec * 1000;
			nvap->na_vaflags |= VA_UTIMES_NULL;
			break;
		};
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		switch (fxdr_unsigned(int, *tl)) {
		case NFSV3SATTRTIME_TOCLIENT:
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &nvap->na_mtime);
			nvap->na_vaflags &= ~VA_UTIMES_NULL;
			break;
		case NFSV3SATTRTIME_TOSERVER:
			NFSGETTIME(&curtime);
			nvap->na_mtime.tv_sec = curtime.tv_sec;
			nvap->na_mtime.tv_nsec = curtime.tv_usec * 1000;
			if (!toclient)
				nvap->na_vaflags |= VA_UTIMES_NULL;
			break;
		};
		break;
	case ND_NFSV4:
		error = nfsv4_sattr(nd, nvap, attrbitp, aclp, p);
	};
nfsmout:
	return (error);
}

/*
 * Handle the setable attributes for V4.
 * Returns NFSERR_BADXDR if it can't be parsed, 0 otherwise.
 */
int
nfsv4_sattr(struct nfsrv_descript *nd, struct nfsvattr *nvap,
    nfsattrbit_t *attrbitp, NFSACL_T *aclp, struct thread *p)
{
	u_int32_t *tl;
	int attrsum = 0;
	int i, j;
	int error, attrsize, bitpos, aclsize, aceerr, retnotsup = 0;
	int toclient = 0;
	u_char *cp, namestr[NFSV4_SMALLSTR + 1];
	uid_t uid;
	gid_t gid;
	struct timeval curtime;

	error = nfsrv_getattrbits(nd, attrbitp, NULL, &retnotsup);
	if (error)
		return (error);
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	attrsize = fxdr_unsigned(int, *tl);

	/*
	 * Loop around getting the setable attributes. If an unsupported
	 * one is found, set nd_repstat == NFSERR_ATTRNOTSUPP and return.
	 */
	if (retnotsup) {
		nd->nd_repstat = NFSERR_ATTRNOTSUPP;
		bitpos = NFSATTRBIT_MAX;
	} else {
		bitpos = 0;
	}
	for (; bitpos < NFSATTRBIT_MAX; bitpos++) {
	    if (attrsum > attrsize) {
		error = NFSERR_BADXDR;
		goto nfsmout;
	    }
	    if (NFSISSET_ATTRBIT(attrbitp, bitpos))
		switch (bitpos) {
		case NFSATTRBIT_SIZE:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_HYPER);
			nvap->na_size = fxdr_hyper(tl);
			attrsum += NFSX_HYPER;
			break;
		case NFSATTRBIT_ACL:
			error = nfsrv_dissectacl(nd, aclp, &aceerr, &aclsize,
			    p);
			if (error)
				goto nfsmout;
			if (aceerr && !nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += aclsize;
			break;
		case NFSATTRBIT_ARCHIVE:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			if (!nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += NFSX_UNSIGNED;
			break;
		case NFSATTRBIT_HIDDEN:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			if (!nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += NFSX_UNSIGNED;
			break;
		case NFSATTRBIT_MIMETYPE:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			i = fxdr_unsigned(int, *tl);
			error = nfsm_advance(nd, NFSM_RNDUP(i), -1);
			if (error)
				goto nfsmout;
			if (!nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += (NFSX_UNSIGNED + NFSM_RNDUP(i));
			break;
		case NFSATTRBIT_MODE:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			nvap->na_mode = nfstov_mode(*tl);
			attrsum += NFSX_UNSIGNED;
			break;
		case NFSATTRBIT_OWNER:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			j = fxdr_unsigned(int, *tl);
			if (j < 0)
				return (NFSERR_BADXDR);
			if (j > NFSV4_SMALLSTR)
				cp = malloc(j + 1, M_NFSSTRING, M_WAITOK);
			else
				cp = namestr;
			error = nfsrv_mtostr(nd, cp, j);
			if (error) {
				if (j > NFSV4_SMALLSTR)
					free(cp, M_NFSSTRING);
				return (error);
			}
			if (!nd->nd_repstat) {
				nd->nd_repstat = nfsv4_strtouid(cp,j,&uid,p);
				if (!nd->nd_repstat)
					nvap->na_uid = uid;
			}
			if (j > NFSV4_SMALLSTR)
				free(cp, M_NFSSTRING);
			attrsum += (NFSX_UNSIGNED + NFSM_RNDUP(j));
			break;
		case NFSATTRBIT_OWNERGROUP:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			j = fxdr_unsigned(int, *tl);
			if (j < 0)
				return (NFSERR_BADXDR);
			if (j > NFSV4_SMALLSTR)
				cp = malloc(j + 1, M_NFSSTRING, M_WAITOK);
			else
				cp = namestr;
			error = nfsrv_mtostr(nd, cp, j);
			if (error) {
				if (j > NFSV4_SMALLSTR)
					free(cp, M_NFSSTRING);
				return (error);
			}
			if (!nd->nd_repstat) {
				nd->nd_repstat = nfsv4_strtogid(cp,j,&gid,p);
				if (!nd->nd_repstat)
					nvap->na_gid = gid;
			}
			if (j > NFSV4_SMALLSTR)
				free(cp, M_NFSSTRING);
			attrsum += (NFSX_UNSIGNED + NFSM_RNDUP(j));
			break;
		case NFSATTRBIT_SYSTEM:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			if (!nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += NFSX_UNSIGNED;
			break;
		case NFSATTRBIT_TIMEACCESSSET:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			attrsum += NFSX_UNSIGNED;
			if (fxdr_unsigned(int, *tl)==NFSV4SATTRTIME_TOCLIENT) {
			    NFSM_DISSECT(tl, u_int32_t *, NFSX_V4TIME);
			    fxdr_nfsv4time(tl, &nvap->na_atime);
			    toclient = 1;
			    attrsum += NFSX_V4TIME;
			} else {
			    NFSGETTIME(&curtime);
			    nvap->na_atime.tv_sec = curtime.tv_sec;
			    nvap->na_atime.tv_nsec = curtime.tv_usec * 1000;
			    nvap->na_vaflags |= VA_UTIMES_NULL;
			}
			break;
		case NFSATTRBIT_TIMEBACKUP:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_V4TIME);
			if (!nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += NFSX_V4TIME;
			break;
		case NFSATTRBIT_TIMECREATE:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_V4TIME);
			if (!nd->nd_repstat)
				nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			attrsum += NFSX_V4TIME;
			break;
		case NFSATTRBIT_TIMEMODIFYSET:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			attrsum += NFSX_UNSIGNED;
			if (fxdr_unsigned(int, *tl)==NFSV4SATTRTIME_TOCLIENT) {
			    NFSM_DISSECT(tl, u_int32_t *, NFSX_V4TIME);
			    fxdr_nfsv4time(tl, &nvap->na_mtime);
			    nvap->na_vaflags &= ~VA_UTIMES_NULL;
			    attrsum += NFSX_V4TIME;
			} else {
			    NFSGETTIME(&curtime);
			    nvap->na_mtime.tv_sec = curtime.tv_sec;
			    nvap->na_mtime.tv_nsec = curtime.tv_usec * 1000;
			    if (!toclient)
				nvap->na_vaflags |= VA_UTIMES_NULL;
			}
			break;
		default:
			nd->nd_repstat = NFSERR_ATTRNOTSUPP;
			/*
			 * set bitpos so we drop out of the loop.
			 */
			bitpos = NFSATTRBIT_MAX;
			break;
		};
	}

	/*
	 * some clients pad the attrlist, so we need to skip over the
	 * padding.
	 */
	if (attrsum > attrsize) {
		error = NFSERR_BADXDR;
	} else {
		attrsize = NFSM_RNDUP(attrsize);
		if (attrsum < attrsize)
			error = nfsm_advance(nd, attrsize - attrsum, -1);
	}
nfsmout:
	return (error);
}

/*
 * Check/setup export credentials.
 */
int
nfsd_excred(struct nfsrv_descript *nd, struct nfsexstuff *exp,
    struct ucred *credanon)
{
	int error = 0;

	/*
	 * Check/setup credentials.
	 */
	if (nd->nd_flag & ND_GSS)
		exp->nes_exflag &= ~MNT_EXPORTANON;

	/*
	 * Check to see if the operation is allowed for this security flavor.
	 * RFC2623 suggests that the NFSv3 Fsinfo RPC be allowed to
	 * AUTH_NONE or AUTH_SYS for file systems requiring RPCSEC_GSS.
	 * Also, allow Secinfo, so that it can acquire the correct flavor(s).
	 */
	if (nfsvno_testexp(nd, exp) &&
	    nd->nd_procnum != NFSV4OP_SECINFO &&
	    nd->nd_procnum != NFSPROC_FSINFO) {
		if (nd->nd_flag & ND_NFSV4)
			error = NFSERR_WRONGSEC;
		else
			error = (NFSERR_AUTHERR | AUTH_TOOWEAK);
		return (error);
	}

	/*
	 * Check to see if the file system is exported V4 only.
	 */
	if (NFSVNO_EXV4ONLY(exp) && !(nd->nd_flag & ND_NFSV4))
		return (NFSERR_PROGNOTV4);

	/*
	 * Now, map the user credentials.
	 * (Note that ND_AUTHNONE will only be set for an NFSv3
	 *  Fsinfo RPC. If set for anything else, this code might need
	 *  to change.)
	 */
	if (NFSVNO_EXPORTED(exp) &&
	    ((!(nd->nd_flag & ND_GSS) && nd->nd_cred->cr_uid == 0) ||
	     NFSVNO_EXPORTANON(exp) ||
	     (nd->nd_flag & ND_AUTHNONE))) {
		nd->nd_cred->cr_uid = credanon->cr_uid;
		nd->nd_cred->cr_gid = credanon->cr_gid;
		crsetgroups(nd->nd_cred, credanon->cr_ngroups,
		    credanon->cr_groups);
	}
	return (0);
}

/*
 * Check exports.
 */
int
nfsvno_checkexp(struct mount *mp, struct sockaddr *nam, struct nfsexstuff *exp,
    struct ucred **credp)
{
	int i, error, *secflavors;

	error = VFS_CHECKEXP(mp, nam, &exp->nes_exflag, credp,
	    &exp->nes_numsecflavor, &secflavors);
	if (error) {
		if (nfs_rootfhset) {
			exp->nes_exflag = 0;
			exp->nes_numsecflavor = 0;
			error = 0;
		}
	} else {
		/* Copy the security flavors. */
		for (i = 0; i < exp->nes_numsecflavor; i++)
			exp->nes_secflavors[i] = secflavors[i];
	}
	return (error);
}

/*
 * Get a vnode for a file handle and export stuff.
 */
int
nfsvno_fhtovp(struct mount *mp, fhandle_t *fhp, struct sockaddr *nam,
    int lktype, struct vnode **vpp, struct nfsexstuff *exp,
    struct ucred **credp)
{
	int i, error, *secflavors;

	*credp = NULL;
	exp->nes_numsecflavor = 0;
	error = VFS_FHTOVP(mp, &fhp->fh_fid, vpp);
	if (error != 0)
		/* Make sure the server replies ESTALE to the client. */
		error = ESTALE;
	if (nam && !error) {
		error = VFS_CHECKEXP(mp, nam, &exp->nes_exflag, credp,
		    &exp->nes_numsecflavor, &secflavors);
		if (error) {
			if (nfs_rootfhset) {
				exp->nes_exflag = 0;
				exp->nes_numsecflavor = 0;
				error = 0;
			} else {
				vput(*vpp);
			}
		} else {
			/* Copy the security flavors. */
			for (i = 0; i < exp->nes_numsecflavor; i++)
				exp->nes_secflavors[i] = secflavors[i];
		}
	}
	if (error == 0 && lktype == LK_SHARED)
		/*
		 * It would be much better to pass lktype to VFS_FHTOVP(),
		 * but this will have to do until VFS_FHTOVP() has a lock
		 * type argument like VFS_VGET().
		 */
		vn_lock(*vpp, LK_DOWNGRADE | LK_RETRY);
	return (error);
}

/*
 * Do the pathconf vnode op.
 */
int
nfsvno_pathconf(struct vnode *vp, int flag, register_t *retf,
    struct ucred *cred, struct thread *p)
{
	int error;

	error = VOP_PATHCONF(vp, flag, retf);
	return (error);
}

/*
 * nfsd_fhtovp() - convert a fh to a vnode ptr
 * 	- look up fsid in mount list (if not found ret error)
 *	- get vp and export rights by calling nfsvno_fhtovp()
 *	- if cred->cr_uid == 0 or MNT_EXPORTANON set it to credanon
 *	  for AUTH_SYS
 * Also handle getting the Giant lock for the file system,
 * as required:
 * - if same mount point as *mpp
 *       do nothing
 *   else if *mpp == NULL
 *       if already locked
 *           leave it locked
 *       else
 *           call VFS_LOCK_GIANT()
 *   else
 *       if already locked
 *            unlock Giant
 *       call VFS_LOCK_GIANT()
 */
void
nfsd_fhtovp(struct nfsrv_descript *nd, struct nfsrvfh *nfp, int lktype,
    struct vnode **vpp, struct nfsexstuff *exp,
    struct mount **mpp, int startwrite, struct thread *p)
{
	struct mount *mp;
	struct ucred *credanon;
	fhandle_t *fhp;

	fhp = (fhandle_t *)nfp->nfsrvfh_data;
	/*
	 * Check for the special case of the nfsv4root_fh.
	 */
	mp = vfs_getvfs(&fhp->fh_fsid);
	if (!mp) {
		*vpp = NULL;
		nd->nd_repstat = ESTALE;
		if (*mpp && exp->nes_vfslocked)
			VFS_UNLOCK_GIANT(*mpp);
		*mpp = NULL;
		exp->nes_vfslocked = 0;
		return;
	}

	/*
	 * Now, handle Giant for the file system.
	 */
	if (*mpp != NULL && *mpp != mp && exp->nes_vfslocked) {
		VFS_UNLOCK_GIANT(*mpp);
		exp->nes_vfslocked = 0;
	}
	if (!exp->nes_vfslocked && *mpp != mp)
		exp->nes_vfslocked = VFS_LOCK_GIANT(mp);

	*mpp = mp;
	if (startwrite)
		vn_start_write(NULL, mpp, V_WAIT);

	nd->nd_repstat = nfsvno_fhtovp(mp, fhp, nd->nd_nam, lktype, vpp, exp,
	    &credanon);

	/*
	 * For NFSv4 without a pseudo root fs, unexported file handles
	 * can be returned, so that Lookup works everywhere.
	 */
	if (!nd->nd_repstat && exp->nes_exflag == 0 &&
	    !(nd->nd_flag & ND_NFSV4)) {
		vput(*vpp);
		nd->nd_repstat = EACCES;
	}

	/*
	 * Personally, I've never seen any point in requiring a
	 * reserved port#, since only in the rare case where the
	 * clients are all boxes with secure system priviledges,
	 * does it provide any enhanced security, but... some people
	 * believe it to be useful and keep putting this code back in.
	 * (There is also some "security checker" out there that
	 *  complains if the nfs server doesn't enforce this.)
	 * However, note the following:
	 * RFC3530 (NFSv4) specifies that a reserved port# not be
	 *	required.
	 * RFC2623 recommends that, if a reserved port# is checked for,
	 *	that there be a way to turn that off--> ifdef'd.
	 */
#ifdef NFS_REQRSVPORT
	if (!nd->nd_repstat) {
		struct sockaddr_in *saddr;
		struct sockaddr_in6 *saddr6;

		saddr = NFSSOCKADDR(nd->nd_nam, struct sockaddr_in *);
		saddr6 = NFSSOCKADDR(nd->nd_nam, struct sockaddr_in6 *);
		if (!(nd->nd_flag & ND_NFSV4) &&
		    ((saddr->sin_family == AF_INET &&
		      ntohs(saddr->sin_port) >= IPPORT_RESERVED) ||
		     (saddr6->sin6_family == AF_INET6 &&
		      ntohs(saddr6->sin6_port) >= IPPORT_RESERVED))) {
			vput(*vpp);
			nd->nd_repstat = (NFSERR_AUTHERR | AUTH_TOOWEAK);
		}
	}
#endif	/* NFS_REQRSVPORT */

	/*
	 * Check/setup credentials.
	 */
	if (!nd->nd_repstat) {
		nd->nd_saveduid = nd->nd_cred->cr_uid;
		nd->nd_repstat = nfsd_excred(nd, exp, credanon);
		if (nd->nd_repstat)
			vput(*vpp);
	}
	if (credanon != NULL)
		crfree(credanon);
	if (nd->nd_repstat) {
		if (startwrite)
			vn_finished_write(mp);
		if (exp->nes_vfslocked) {
			VFS_UNLOCK_GIANT(mp);
			exp->nes_vfslocked = 0;
		}
		vfs_rel(mp);
		*vpp = NULL;
		*mpp = NULL;
	} else {
		vfs_rel(mp);
	}
}

/*
 * glue for fp.
 */
int
fp_getfvp(struct thread *p, int fd, struct file **fpp, struct vnode **vpp)
{
	struct filedesc *fdp;
	struct file *fp;

	fdp = p->td_proc->p_fd;
	if (fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	*fpp = fp;
	return (0);
}

/*
 * Called from nfssvc() to update the exports list. Just call
 * vfs_export(). This has to be done, since the v4 root fake fs isn't
 * in the mount list.
 */
int
nfsrv_v4rootexport(void *argp, struct ucred *cred, struct thread *p)
{
	struct nfsex_args *nfsexargp = (struct nfsex_args *)argp;
	int error;
	struct nameidata nd;
	fhandle_t fh;

	error = vfs_export(&nfsv4root_mnt, &nfsexargp->export);
	if ((nfsexargp->export.ex_flags & MNT_DELEXPORT) != 0)
		nfs_rootfhset = 0;
	else if (error == 0) {
		if (nfsexargp->fspec == NULL)
			return (EPERM);
		/*
		 * If fspec != NULL, this is the v4root path.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE, UIO_USERSPACE,
		    nfsexargp->fspec, p);
		if ((error = namei(&nd)) != 0)
			return (error);
		error = nfsvno_getfh(nd.ni_vp, &fh, p);
		vrele(nd.ni_vp);
		if (!error) {
			nfs_rootfh.nfsrvfh_len = NFSX_MYFH;
			NFSBCOPY((caddr_t)&fh,
			    nfs_rootfh.nfsrvfh_data,
			    sizeof (fhandle_t));
			nfs_rootfhset = 1;
		}
	}
	return (error);
}

/*
 * Get the tcp socket sequence numbers we need.
 * (Maybe this should be moved to the tcp sources?)
 */
int
nfsrv_getsocksndseq(struct socket *so, tcp_seq *maxp, tcp_seq *unap)
{
	struct inpcb *inp;
	struct tcpcb *tp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("nfsrv_getsocksndseq: inp == NULL"));
	INP_RLOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		INP_RUNLOCK(inp);
		return (EPIPE);
	}
	tp = intotcpcb(inp);
	if (tp->t_state != TCPS_ESTABLISHED) {
		INP_RUNLOCK(inp);
		return (EPIPE);
	}
	*maxp = tp->snd_max;
	*unap = tp->snd_una;
	INP_RUNLOCK(inp);
	return (0);
}

/*
 * This function needs to test to see if the system is near its limit
 * for memory allocation via malloc() or mget() and return True iff
 * either of these resources are near their limit.
 * XXX (For now, this is just a stub.)
 */
int nfsrv_testmalloclimit = 0;
int
nfsrv_mallocmget_limit(void)
{
	static int printmesg = 0;
	static int testval = 1;

	if (nfsrv_testmalloclimit && (testval++ % 1000) == 0) {
		if ((printmesg++ % 100) == 0)
			printf("nfsd: malloc/mget near limit\n");
		return (1);
	}
	return (0);
}

/*
 * BSD specific initialization of a mount point.
 */
void
nfsd_mntinit(void)
{
	static int inited = 0;

	if (inited)
		return;
	inited = 1;
	nfsv4root_mnt.mnt_flag = (MNT_RDONLY | MNT_EXPORTED);
	TAILQ_INIT(&nfsv4root_mnt.mnt_nvnodelist);
	nfsv4root_mnt.mnt_export = NULL;
	TAILQ_INIT(&nfsv4root_opt);
	TAILQ_INIT(&nfsv4root_newopt);
	nfsv4root_mnt.mnt_opt = &nfsv4root_opt;
	nfsv4root_mnt.mnt_optnew = &nfsv4root_newopt;
	nfsv4root_mnt.mnt_nvnodelistsize = 0;
}

/*
 * Get a vnode for a file handle, without checking exports, etc.
 */
struct vnode *
nfsvno_getvp(fhandle_t *fhp)
{
	struct mount *mp;
	struct vnode *vp;
	int error;

	mp = vfs_getvfs(&fhp->fh_fsid);
	if (mp == NULL)
		return (NULL);
	error = VFS_FHTOVP(mp, &fhp->fh_fid, &vp);
	if (error)
		return (NULL);
	return (vp);
}

/*
 * Do a local VOP_ADVLOCK().
 */
int
nfsvno_advlock(struct vnode *vp, int ftype, u_int64_t first,
    u_int64_t end, struct thread *td)
{
	int error;
	struct flock fl;
	u_int64_t tlen;

	if (nfsrv_dolocallocks == 0)
		return (0);

	/* Check for VI_DOOMED here, so that VOP_ADVLOCK() isn't performed. */
	if ((vp->v_iflag & VI_DOOMED) != 0)
		return (EPERM);

	fl.l_whence = SEEK_SET;
	fl.l_type = ftype;
	fl.l_start = (off_t)first;
	if (end == NFS64BITSSET) {
		fl.l_len = 0;
	} else {
		tlen = end - first;
		fl.l_len = (off_t)tlen;
	}
	/*
	 * For FreeBSD8, the l_pid and l_sysid must be set to the same
	 * values for all calls, so that all locks will be held by the
	 * nfsd server. (The nfsd server handles conflicts between the
	 * various clients.)
	 * Since an NFSv4 lockowner is a ClientID plus an array of up to 1024
	 * bytes, so it can't be put in l_sysid.
	 */
	if (nfsv4_sysid == 0)
		nfsv4_sysid = nlm_acquire_next_sysid();
	fl.l_pid = (pid_t)0;
	fl.l_sysid = (int)nfsv4_sysid;

	NFSVOPUNLOCK(vp, 0, td);
	if (ftype == F_UNLCK)
		error = VOP_ADVLOCK(vp, (caddr_t)td->td_proc, F_UNLCK, &fl,
		    (F_POSIX | F_REMOTE));
	else
		error = VOP_ADVLOCK(vp, (caddr_t)td->td_proc, F_SETLK, &fl,
		    (F_POSIX | F_REMOTE));
	NFSVOPLOCK(vp, LK_EXCLUSIVE | LK_RETRY, td);
	return (error);
}

/*
 * Unlock an underlying local file system.
 */
void
nfsvno_unlockvfs(struct mount *mp)
{

	VFS_UNLOCK_GIANT(mp);
}

/*
 * Lock an underlying file system, as required, and return
 * whether or not it is locked.
 */
int
nfsvno_lockvfs(struct mount *mp)
{
	int ret;

	ret = VFS_LOCK_GIANT(mp);
	return (ret);
}

/*
 * Check the nfsv4 root exports.
 */
int
nfsvno_v4rootexport(struct nfsrv_descript *nd)
{
	struct ucred *credanon;
	int exflags, error, numsecflavor, *secflavors, i;

	error = vfs_stdcheckexp(&nfsv4root_mnt, nd->nd_nam, &exflags,
	    &credanon, &numsecflavor, &secflavors);
	if (error)
		return (NFSERR_PROGUNAVAIL);
	if (credanon != NULL)
		crfree(credanon);
	for (i = 0; i < numsecflavor; i++) {
		if (secflavors[i] == AUTH_SYS)
			nd->nd_flag |= ND_EXAUTHSYS;
		else if (secflavors[i] == RPCSEC_GSS_KRB5)
			nd->nd_flag |= ND_EXGSS;
		else if (secflavors[i] == RPCSEC_GSS_KRB5I)
			nd->nd_flag |= ND_EXGSSINTEGRITY;
		else if (secflavors[i] == RPCSEC_GSS_KRB5P)
			nd->nd_flag |= ND_EXGSSPRIVACY;
	}
	return (0);
}

/*
 * Nfs server psuedo system call for the nfsd's
 */
/*
 * MPSAFE
 */
static int
nfssvc_nfsd(struct thread *td, struct nfssvc_args *uap)
{
	struct file *fp;
	struct nfsd_addsock_args sockarg;
	struct nfsd_nfsd_args nfsdarg;
	int error;

	if (uap->flag & NFSSVC_NFSDADDSOCK) {
		error = copyin(uap->argp, (caddr_t)&sockarg, sizeof (sockarg));
		if (error)
			return (error);
		if ((error = fget(td, sockarg.sock, &fp)) != 0) {
			return (error);
		}
		if (fp->f_type != DTYPE_SOCKET) {
			fdrop(fp, td);
			return (EPERM);
		}
		error = nfsrvd_addsock(fp);
		fdrop(fp, td);
	} else if (uap->flag & NFSSVC_NFSDNFSD) {
		if (uap->argp == NULL) 
			return (EINVAL);
		error = copyin(uap->argp, (caddr_t)&nfsdarg,
		    sizeof (nfsdarg));
		if (error)
			return (error);
		error = nfsrvd_nfsd(td, &nfsdarg);
	} else {
		error = nfssvc_srvcall(td, uap, td->td_ucred);
	}
	return (error);
}

static int
nfssvc_srvcall(struct thread *p, struct nfssvc_args *uap, struct ucred *cred)
{
	struct nfsex_args export;
	struct file *fp = NULL;
	int stablefd, len;
	struct nfsd_clid adminrevoke;
	struct nfsd_dumplist dumplist;
	struct nfsd_dumpclients *dumpclients;
	struct nfsd_dumplocklist dumplocklist;
	struct nfsd_dumplocks *dumplocks;
	struct nameidata nd;
	vnode_t vp;
	int error = EINVAL;

	if (uap->flag & NFSSVC_PUBLICFH) {
		NFSBZERO((caddr_t)&nfs_pubfh.nfsrvfh_data,
		    sizeof (fhandle_t));
		error = copyin(uap->argp,
		    &nfs_pubfh.nfsrvfh_data, sizeof (fhandle_t));
		if (!error)
			nfs_pubfhset = 1;
	} else if (uap->flag & NFSSVC_V4ROOTEXPORT) {
		error = copyin(uap->argp,(caddr_t)&export,
		    sizeof (struct nfsex_args));
		if (!error)
			error = nfsrv_v4rootexport(&export, cred, p);
	} else if (uap->flag & NFSSVC_NOPUBLICFH) {
		nfs_pubfhset = 0;
		error = 0;
	} else if (uap->flag & NFSSVC_STABLERESTART) {
		error = copyin(uap->argp, (caddr_t)&stablefd,
		    sizeof (int));
		if (!error)
			error = fp_getfvp(p, stablefd, &fp, &vp);
		if (!error && (NFSFPFLAG(fp) & (FREAD | FWRITE)) != (FREAD | FWRITE))
			error = EBADF;
		if (!error && newnfs_numnfsd != 0)
			error = EPERM;
		if (!error) {
			nfsrv_stablefirst.nsf_fp = fp;
			nfsrv_setupstable(p);
		}
	} else if (uap->flag & NFSSVC_ADMINREVOKE) {
		error = copyin(uap->argp, (caddr_t)&adminrevoke,
		    sizeof (struct nfsd_clid));
		if (!error)
			error = nfsrv_adminrevoke(&adminrevoke, p);
	} else if (uap->flag & NFSSVC_DUMPCLIENTS) {
		error = copyin(uap->argp, (caddr_t)&dumplist,
		    sizeof (struct nfsd_dumplist));
		if (!error && (dumplist.ndl_size < 1 ||
			dumplist.ndl_size > NFSRV_MAXDUMPLIST))
			error = EPERM;
		if (!error) {
		    len = sizeof (struct nfsd_dumpclients) * dumplist.ndl_size;
		    dumpclients = (struct nfsd_dumpclients *)malloc(len,
			M_TEMP, M_WAITOK);
		    nfsrv_dumpclients(dumpclients, dumplist.ndl_size);
		    error = copyout(dumpclients,
			CAST_USER_ADDR_T(dumplist.ndl_list), len);
		    free((caddr_t)dumpclients, M_TEMP);
		}
	} else if (uap->flag & NFSSVC_DUMPLOCKS) {
		error = copyin(uap->argp, (caddr_t)&dumplocklist,
		    sizeof (struct nfsd_dumplocklist));
		if (!error && (dumplocklist.ndllck_size < 1 ||
			dumplocklist.ndllck_size > NFSRV_MAXDUMPLIST))
			error = EPERM;
		if (!error)
			error = nfsrv_lookupfilename(&nd,
				dumplocklist.ndllck_fname, p);
		if (!error) {
			len = sizeof (struct nfsd_dumplocks) *
				dumplocklist.ndllck_size;
			dumplocks = (struct nfsd_dumplocks *)malloc(len,
				M_TEMP, M_WAITOK);
			nfsrv_dumplocks(nd.ni_vp, dumplocks,
			    dumplocklist.ndllck_size, p);
			vput(nd.ni_vp);
			error = copyout(dumplocks,
			    CAST_USER_ADDR_T(dumplocklist.ndllck_list), len);
			free((caddr_t)dumplocks, M_TEMP);
		}
	}
	return (error);
}

/*
 * Check exports.
 * Returns 0 if ok, 1 otherwise.
 */
int
nfsvno_testexp(struct nfsrv_descript *nd, struct nfsexstuff *exp)
{
	int i;

	/*
	 * This seems odd, but allow the case where the security flavor
	 * list is empty. This happens when NFSv4 is traversing non-exported
	 * file systems. Exported file systems should always have a non-empty
	 * security flavor list.
	 */
	if (exp->nes_numsecflavor == 0)
		return (0);

	for (i = 0; i < exp->nes_numsecflavor; i++) {
		/*
		 * The tests for privacy and integrity must be first,
		 * since ND_GSS is set for everything but AUTH_SYS.
		 */
		if (exp->nes_secflavors[i] == RPCSEC_GSS_KRB5P &&
		    (nd->nd_flag & ND_GSSPRIVACY))
			return (0);
		if (exp->nes_secflavors[i] == RPCSEC_GSS_KRB5I &&
		    (nd->nd_flag & ND_GSSINTEGRITY))
			return (0);
		if (exp->nes_secflavors[i] == RPCSEC_GSS_KRB5 &&
		    (nd->nd_flag & ND_GSS))
			return (0);
		if (exp->nes_secflavors[i] == AUTH_SYS &&
		    (nd->nd_flag & ND_GSS) == 0)
			return (0);
	}
	return (1);
}

/*
 * Calculate a hash value for the fid in a file handle.
 */
uint32_t
nfsrv_hashfh(fhandle_t *fhp)
{
	uint32_t hashval;

	hashval = hash32_buf(&fhp->fh_fid, sizeof(struct fid), 0);
	return (hashval);
}

extern int (*nfsd_call_nfsd)(struct thread *, struct nfssvc_args *);

/*
 * Called once to initialize data structures...
 */
static int
nfsd_modevent(module_t mod, int type, void *data)
{
	int error = 0;
	static int loaded = 0;

	switch (type) {
	case MOD_LOAD:
		if (loaded)
			return (0);
		newnfs_portinit();
		mtx_init(&nfs_cache_mutex, "nfs_cache_mutex", NULL, MTX_DEF);
		mtx_init(&nfs_v4root_mutex, "nfs_v4root_mutex", NULL, MTX_DEF);
		mtx_init(&nfsv4root_mnt.mnt_mtx, "struct mount mtx", NULL,
		    MTX_DEF);
		lockinit(&nfsv4root_mnt.mnt_explock, PVFS, "explock", 0, 0);
		nfsrvd_initcache();
		nfsd_init();
		NFSD_LOCK();
		nfsrvd_init(0);
		NFSD_UNLOCK();
		nfsd_mntinit();
#ifdef VV_DISABLEDELEG
		vn_deleg_ops.vndeleg_recall = nfsd_recalldelegation;
		vn_deleg_ops.vndeleg_disable = nfsd_disabledelegation;
#endif
		nfsd_call_servertimer = nfsrv_servertimer;
		nfsd_call_nfsd = nfssvc_nfsd;
		loaded = 1;
		break;

	case MOD_UNLOAD:
		if (newnfs_numnfsd != 0) {
			error = EBUSY;
			break;
		}

#ifdef VV_DISABLEDELEG
		vn_deleg_ops.vndeleg_recall = NULL;
		vn_deleg_ops.vndeleg_disable = NULL;
#endif
		nfsd_call_servertimer = NULL;
		nfsd_call_nfsd = NULL;
		/* and get rid of the locks */
		mtx_destroy(&nfs_cache_mutex);
		mtx_destroy(&nfs_v4root_mutex);
		mtx_destroy(&nfsv4root_mnt.mnt_mtx);
		lockdestroy(&nfsv4root_mnt.mnt_explock);
		loaded = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return error;
}
static moduledata_t nfsd_mod = {
	"nfsd",
	nfsd_modevent,
	NULL,
};
DECLARE_MODULE(nfsd, nfsd_mod, SI_SUB_VFS, SI_ORDER_ANY);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(nfsd, 1);
MODULE_DEPEND(nfsd, nfscommon, 1, 1, 1);
MODULE_DEPEND(nfsd, nfslockd, 1, 1, 1);
MODULE_DEPEND(nfsd, krpc, 1, 1, 1);
MODULE_DEPEND(nfsd, nfssvc, 1, 1, 1);

