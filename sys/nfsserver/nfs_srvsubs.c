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
 *	@(#)nfs_subs.c  8.8 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <rpc/rpc.h>

#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfsserver/nfsm_subs.h>

#include <netinet/in.h>

/*
 * Data items converted to xdr at startup, since they are constant
 * This is kinda hokey, but may save a little time doing byte swaps
 */
u_int32_t nfsrv_nfs_xdrneg1;
u_int32_t nfsrv_nfs_true, nfsrv_nfs_false;

/* And other global data */
static const nfstype nfsv2_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR,
				       NFLNK, NFNON, NFCHR, NFNON };
#define vtonfsv2_type(a)	txdr_unsigned(nfsv2_type[((int32_t)(a))])
#define vtonfsv3_mode(m)	txdr_unsigned((m) & ALLPERMS)

int nfsrv_ticks;

struct mtx nfsd_mtx;

/*
 * Mapping of old NFS Version 2 RPC numbers to generic numbers.
 */
const int nfsrv_nfsv3_procid[NFS_NPROCS] = {
	NFSPROC_NULL,
	NFSPROC_GETATTR,
	NFSPROC_SETATTR,
	NFSPROC_NOOP,
	NFSPROC_LOOKUP,
	NFSPROC_READLINK,
	NFSPROC_READ,
	NFSPROC_NOOP,
	NFSPROC_WRITE,
	NFSPROC_CREATE,
	NFSPROC_REMOVE,
	NFSPROC_RENAME,
	NFSPROC_LINK,
	NFSPROC_SYMLINK,
	NFSPROC_MKDIR,
	NFSPROC_RMDIR,
	NFSPROC_READDIR,
	NFSPROC_FSSTAT,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
};

/*
 * and the reverse mapping from generic to Version 2 procedure numbers
 */
const int nfsrvv2_procid[NFS_NPROCS] = {
	NFSV2PROC_NULL,
	NFSV2PROC_GETATTR,
	NFSV2PROC_SETATTR,
	NFSV2PROC_LOOKUP,
	NFSV2PROC_NOOP,
	NFSV2PROC_READLINK,
	NFSV2PROC_READ,
	NFSV2PROC_WRITE,
	NFSV2PROC_CREATE,
	NFSV2PROC_MKDIR,
	NFSV2PROC_SYMLINK,
	NFSV2PROC_CREATE,
	NFSV2PROC_REMOVE,
	NFSV2PROC_RMDIR,
	NFSV2PROC_RENAME,
	NFSV2PROC_LINK,
	NFSV2PROC_READDIR,
	NFSV2PROC_NOOP,
	NFSV2PROC_STATFS,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
};

/*
 * Maps errno values to nfs error numbers.
 * Use 0 (which gets converted to NFSERR_IO) as the catch all for ones not
 * specifically defined in RFC 1094.
 */
static const u_char nfsrv_v2errmap[ELAST] = {
  NFSERR_PERM,	NFSERR_NOENT,	0,		0,		0,	
  NFSERR_NXIO,	0,		0,		0,		0,	
  0,		0,		NFSERR_ACCES,	0,		0,	
  0,		NFSERR_EXIST,	0,		NFSERR_NODEV,	NFSERR_NOTDIR,
  NFSERR_ISDIR,	0,		0,		0,		0,	
  0,		NFSERR_FBIG,	NFSERR_NOSPC,	0,		NFSERR_ROFS,
  0,		0,		0,		0,		0,	
  0,		0,		0,		0,		0,	
  0,		0,		0,		0,		0,	
  0,		0,		0,		0,		0,	
  0,		0,		0,		0,		0,	
  0,		0,		0,		0,		0,	
  0,		0,		NFSERR_NAMETOL,	0,		0,	
  NFSERR_NOTEMPTY, 0,		0,		NFSERR_DQUOT,	NFSERR_STALE,
  0
};

/*
 * Maps errno values to nfs error numbers.
 * Although it is not obvious whether or not NFS clients really care if
 * a returned error value is in the specified list for the procedure, the
 * safest thing to do is filter them appropriately. For Version 2, the
 * X/Open XNFS document is the only specification that defines error values
 * for each RPC (The RFC simply lists all possible error values for all RPCs),
 * so I have decided to not do this for Version 2.
 * The first entry is the default error return and the rest are the valid
 * errors for that RPC in increasing numeric order.
 */
static const short nfsv3err_null[] = {
	0,
	0,
};

static const short nfsv3err_getattr[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_setattr[] = {
	NFSERR_IO,
	NFSERR_PERM,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOT_SYNC,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_lookup[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_NAMETOL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_access[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_readlink[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_read[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_NXIO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_write[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_FBIG,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_create[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_mkdir[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_symlink[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_mknod[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_BADTYPE,
	0,
};

static const short nfsv3err_remove[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_rmdir[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_INVAL,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_NOTEMPTY,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_rename[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_XDEV,
	NFSERR_NOTDIR,
	NFSERR_ISDIR,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_MLINK,
	NFSERR_NAMETOL,
	NFSERR_NOTEMPTY,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_link[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_XDEV,
	NFSERR_NOTDIR,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_MLINK,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_readdir[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_TOOSMALL,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_readdirplus[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_NOTSUPP,
	NFSERR_TOOSMALL,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_fsstat[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_fsinfo[] = {
	NFSERR_STALE,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_pathconf[] = {
	NFSERR_STALE,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short nfsv3err_commit[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static const short *nfsrv_v3errmap[] = {
	nfsv3err_null,
	nfsv3err_getattr,
	nfsv3err_setattr,
	nfsv3err_lookup,
	nfsv3err_access,
	nfsv3err_readlink,
	nfsv3err_read,
	nfsv3err_write,
	nfsv3err_create,
	nfsv3err_mkdir,
	nfsv3err_symlink,
	nfsv3err_mknod,
	nfsv3err_remove,
	nfsv3err_rmdir,
	nfsv3err_rename,
	nfsv3err_link,
	nfsv3err_readdir,
	nfsv3err_readdirplus,
	nfsv3err_fsstat,
	nfsv3err_fsinfo,
	nfsv3err_pathconf,
	nfsv3err_commit,
};

extern int (*nfsd_call_nfsserver)(struct thread *, struct nfssvc_args *);

/*
 * Called once to initialize data structures...
 */
static int
nfsrv_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		mtx_init(&nfsd_mtx, "nfsd_mtx", NULL, MTX_DEF);
		nfsrv_nfs_true = txdr_unsigned(TRUE);
		nfsrv_nfs_false = txdr_unsigned(FALSE);
		nfsrv_nfs_xdrneg1 = txdr_unsigned(-1);
		nfsrv_ticks = (hz * NFS_TICKINTVL + 500) / 1000;
		if (nfsrv_ticks < 1)
			nfsrv_ticks = 1;

		NFSD_LOCK();
		nfsrv_init(0);		/* Init server data structures */
		NFSD_UNLOCK();

		nfsd_call_nfsserver = nfssvc_nfsserver;
		break;

	case MOD_UNLOAD:
		if (nfsrv_numnfsd != 0) {
			error = EBUSY;
			break;
		}

		nfsd_call_nfsserver = NULL;
		callout_drain(&nfsrv_callout);
		mtx_destroy(&nfsd_mtx);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return error;
}
static moduledata_t nfsserver_mod = {
	"nfsserver",
	nfsrv_modevent,
	NULL,
};
DECLARE_MODULE(nfsserver, nfsserver_mod, SI_SUB_VFS, SI_ORDER_ANY);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(nfsserver, 1);
MODULE_DEPEND(nfsserver, nfssvc, 1, 1, 1);
MODULE_DEPEND(nfsserver, krpc, 1, 1, 1);
MODULE_DEPEND(nfsserver, nfs_common, 1, 1, 1);

/*
 * Set up nameidata for a lookup() call and do it.
 *
 * If pubflag is set, this call is done for a lookup operation on the
 * public filehandle. In that case we allow crossing mountpoints and
 * absolute pathnames. However, the caller is expected to check that
 * the lookup result is within the public fs, and deny access if
 * it is not.
 *
 * nfs_namei() clears out garbage fields that namei() might leave garbage.
 * This is mainly ni_vp and ni_dvp when an error occurs, and ni_dvp when no
 * error occurs but the parent was not requested.
 *
 * dirp may be set whether an error is returned or not, and must be
 * released by the caller.
 */
int
nfs_namei(struct nameidata *ndp, struct nfsrv_descript *nfsd,
    fhandle_t *fhp, int len, struct nfssvc_sock *slp,
    struct sockaddr *nam, struct mbuf **mdp,
    caddr_t *dposp, struct vnode **retdirp, int v3, struct vattr *retdirattrp,
    int *retdirattr_retp, int pubflag)
{
	int i, rem;
	struct mbuf *md;
	char *fromcp, *tocp, *cp;
	struct iovec aiov;
	struct uio auio;
	struct vnode *dp;
	int error, rdonly, linklen;
	struct componentname *cnp = &ndp->ni_cnd;
	int lockleaf = (cnp->cn_flags & LOCKLEAF) != 0;
	int dvfslocked;
	int vfslocked;

	vfslocked = 0;
	dvfslocked = 0;
	*retdirp = NULL;
	cnp->cn_flags |= NOMACCHECK;
	cnp->cn_pnbuf = uma_zalloc(namei_zone, M_WAITOK);

	/*
	 * Copy the name from the mbuf list to ndp->ni_pnbuf
	 * and set the various ndp fields appropriately.
	 */
	fromcp = *dposp;
	tocp = cnp->cn_pnbuf;
	md = *mdp;
	rem = mtod(md, caddr_t) + md->m_len - fromcp;
	for (i = 0; i < len; i++) {
		while (rem == 0) {
			md = md->m_next;
			if (md == NULL) {
				error = EBADRPC;
				goto out;
			}
			fromcp = mtod(md, caddr_t);
			rem = md->m_len;
		}
		if (*fromcp == '\0' || (!pubflag && *fromcp == '/')) {
			error = EACCES;
			goto out;
		}
		*tocp++ = *fromcp++;
		rem--;
	}
	*tocp = '\0';
	*mdp = md;
	*dposp = fromcp;
	len = nfsm_rndup(len)-len;
	if (len > 0) {
		if (rem >= len)
			*dposp += len;
		else if ((error = nfs_adv(mdp, dposp, len, rem)) != 0)
			goto out;
	}

	if (!pubflag && nfs_ispublicfh(fhp))
		return (ESTALE);

	/*
	 * Extract and set starting directory.
	 */
	error = nfsrv_fhtovp(fhp, 0, &dp, &dvfslocked, nfsd, slp, nam, &rdonly);
	if (error)
		goto out;
	vfslocked = VFS_LOCK_GIANT(dp->v_mount);
	if (dp->v_type != VDIR) {
		vput(dp);
		error = ENOTDIR;
		goto out;
	}

	if (rdonly)
		cnp->cn_flags |= RDONLY;

	/*
	 * Set return directory.  Reference to dp is implicitly transfered
	 * to the returned pointer
	 */
	*retdirp = dp;
	if (v3) {
		*retdirattr_retp = VOP_GETATTR(dp, retdirattrp,
			ndp->ni_cnd.cn_cred);
	}

	VOP_UNLOCK(dp, 0);

	if (pubflag) {
		/*
		 * Oh joy. For WebNFS, handle those pesky '%' escapes,
		 * and the 'native path' indicator.
		 */
		cp = uma_zalloc(namei_zone, M_WAITOK);
		fromcp = cnp->cn_pnbuf;
		tocp = cp;
		if ((unsigned char)*fromcp >= WEBNFS_SPECCHAR_START) {
			switch ((unsigned char)*fromcp) {
			case WEBNFS_NATIVE_CHAR:
				/*
				 * 'Native' path for us is the same
				 * as a path according to the NFS spec,
				 * just skip the escape char.
				 */
				fromcp++;
				break;
			/*
			 * More may be added in the future, range 0x80-0xff
			 */
			default:
				error = EIO;
				uma_zfree(namei_zone, cp);
				goto out;
			}
		}
		/*
		 * Translate the '%' escapes, URL-style.
		 */
		while (*fromcp != '\0') {
			if (*fromcp == WEBNFS_ESC_CHAR) {
				if (fromcp[1] != '\0' && fromcp[2] != '\0') {
					fromcp++;
					*tocp++ = HEXSTRTOI(fromcp);
					fromcp += 2;
					continue;
				} else {
					error = ENOENT;
					uma_zfree(namei_zone, cp);
					goto out;
				}
			} else
				*tocp++ = *fromcp++;
		}
		*tocp = '\0';
		uma_zfree(namei_zone, cnp->cn_pnbuf);
		cnp->cn_pnbuf = cp;
	}

	ndp->ni_pathlen = (tocp - cnp->cn_pnbuf) + 1;
	ndp->ni_segflg = UIO_SYSSPACE;

	if (pubflag) {
		ndp->ni_rootdir = rootvnode;
		ndp->ni_loopcnt = 0;
		if (cnp->cn_pnbuf[0] == '/') {
			int tvfslocked;

			tvfslocked = VFS_LOCK_GIANT(rootvnode->v_mount);
			VFS_UNLOCK_GIANT(vfslocked);
			dp = rootvnode;
			vfslocked = tvfslocked;
		}
	} else {
		cnp->cn_flags |= NOCROSSMOUNT;
	}

	/*
	 * Initialize for scan, set ni_startdir and bump ref on dp again
	 * because lookup() will dereference ni_startdir.
	 */

	cnp->cn_thread = curthread;
	VREF(dp);
	ndp->ni_startdir = dp;

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
		if (vfslocked)
			ndp->ni_cnd.cn_flags |= GIANTHELD;
		error = lookup(ndp);
		vfslocked = (ndp->ni_cnd.cn_flags & GIANTHELD) != 0;
		ndp->ni_cnd.cn_flags &= ~GIANTHELD;
		if (error)
			break;

		/*
		 * Check for encountering a symbolic link.  Trivial
		 * termination occurs if no symlink encountered.
		 * Note: zfree is safe because error is 0, so we will
		 * not zfree it again when we break.
		 */
		if ((cnp->cn_flags & ISSYMLINK) == 0) {
			if (cnp->cn_flags & (SAVENAME | SAVESTART))
				cnp->cn_flags |= HASBUF;
			else
				uma_zfree(namei_zone, cnp->cn_pnbuf);
			if (ndp->ni_vp && !lockleaf)
				VOP_UNLOCK(ndp->ni_vp, 0);
			break;
		}

		/*
		 * Validate symlink
		 */
		if ((cnp->cn_flags & LOCKPARENT) && ndp->ni_pathlen == 1)
			VOP_UNLOCK(ndp->ni_dvp, 0);
		if (!pubflag) {
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
			vput(ndp->ni_vp);
			vrele(ndp->ni_dvp);
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
			bcopy(ndp->ni_next, cp + linklen, ndp->ni_pathlen);
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
	if (cnp->cn_flags & GIANTHELD) {
		mtx_unlock(&Giant);
		cnp->cn_flags &= ~GIANTHELD;
	}

	/*
	 * nfs_namei() guarentees that fields will not contain garbage
	 * whether an error occurs or not.  This allows the caller to track
	 * cleanup state trivially.
	 */
out:
	if (error) {
		uma_zfree(namei_zone, cnp->cn_pnbuf);
		ndp->ni_vp = NULL;
		ndp->ni_dvp = NULL;
		ndp->ni_startdir = NULL;
		cnp->cn_flags &= ~HASBUF;
		VFS_UNLOCK_GIANT(vfslocked);
		vfslocked = 0;
	} else if ((ndp->ni_cnd.cn_flags & (WANTPARENT|LOCKPARENT)) == 0) {
		ndp->ni_dvp = NULL;
	}
	/*
	 * This differs from normal namei() in that even on failure we may
	 * return with Giant held due to the dirp return.  Make sure we only
	 * have not recursed however.  The calling code only expects to drop
	 * one acquire.
	 */
	if (vfslocked || dvfslocked)
		ndp->ni_cnd.cn_flags |= GIANTHELD;
	if (vfslocked && dvfslocked)
		VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * A fiddled version of m_adj() that ensures null fill to a long
 * boundary and only trims off the back end
 */
void
nfsm_adj(struct mbuf *mp, int len, int nul)
{
	struct mbuf *m;
	int count, i;
	char *cp;

	/*
	 * Trim from tail.  Scan the mbuf chain,
	 * calculating its length and finding the last mbuf.
	 * If the adjustment only affects this mbuf, then just
	 * adjust and return.  Otherwise, rescan and truncate
	 * after the remaining size.
	 */
	count = 0;
	m = mp;
	for (;;) {
		count += m->m_len;
		if (m->m_next == NULL)
			break;
		m = m->m_next;
	}
	if (m->m_len > len) {
		m->m_len -= len;
		if (nul > 0) {
			cp = mtod(m, caddr_t)+m->m_len-nul;
			for (i = 0; i < nul; i++)
				*cp++ = '\0';
		}
		return;
	}
	count -= len;
	if (count < 0)
		count = 0;
	/*
	 * Correct length for chain is "count".
	 * Find the mbuf with last data, adjust its length,
	 * and toss data from remaining mbufs on chain.
	 */
	for (m = mp; m; m = m->m_next) {
		if (m->m_len >= count) {
			m->m_len = count;
			if (nul > 0) {
				cp = mtod(m, caddr_t)+m->m_len-nul;
				for (i = 0; i < nul; i++)
					*cp++ = '\0';
			}
			if (m->m_next != NULL) {
				m_freem(m->m_next);
				m->m_next = NULL;
			}
			break;
		}
		count -= m->m_len;
	}
}

/*
 * Make these functions instead of macros, so that the kernel text size
 * doesn't get too big...
 */
void
nfsm_srvwcc(struct nfsrv_descript *nfsd, int before_ret,
    struct vattr *before_vap, int after_ret, struct vattr *after_vap,
    struct mbuf **mbp, char **bposp)
{
	struct mbuf *mb = *mbp;
	char *bpos = *bposp;
	u_int32_t *tl;

	if (before_ret) {
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
		*tl = nfsrv_nfs_false;
	} else {
		tl = nfsm_build(u_int32_t *, 7 * NFSX_UNSIGNED);
		*tl++ = nfsrv_nfs_true;
		txdr_hyper(before_vap->va_size, tl);
		tl += 2;
		txdr_nfsv3time(&(before_vap->va_mtime), tl);
		tl += 2;
		txdr_nfsv3time(&(before_vap->va_ctime), tl);
	}
	*bposp = bpos;
	*mbp = mb;
	nfsm_srvpostopattr(nfsd, after_ret, after_vap, mbp, bposp);
}

void
nfsm_srvpostopattr(struct nfsrv_descript *nfsd, int after_ret,
    struct vattr *after_vap, struct mbuf **mbp, char **bposp)
{
	struct mbuf *mb = *mbp;
	char *bpos = *bposp;
	u_int32_t *tl;
	struct nfs_fattr *fp;

	if (after_ret) {
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
		*tl = nfsrv_nfs_false;
	} else {
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED + NFSX_V3FATTR);
		*tl++ = nfsrv_nfs_true;
		fp = (struct nfs_fattr *)tl;
		nfsm_srvfattr(nfsd, after_vap, fp);
	}
	*mbp = mb;
	*bposp = bpos;
}

void
nfsm_srvfattr(struct nfsrv_descript *nfsd, struct vattr *vap,
    struct nfs_fattr *fp)
{

	fp->fa_nlink = txdr_unsigned(vap->va_nlink);
	fp->fa_uid = txdr_unsigned(vap->va_uid);
	fp->fa_gid = txdr_unsigned(vap->va_gid);
	if (nfsd->nd_flag & ND_NFSV3) {
		fp->fa_type = vtonfsv3_type(vap->va_type);
		fp->fa_mode = vtonfsv3_mode(vap->va_mode);
		txdr_hyper(vap->va_size, &fp->fa3_size);
		txdr_hyper(vap->va_bytes, &fp->fa3_used);
		fp->fa3_rdev.specdata1 = txdr_unsigned(major(vap->va_rdev));
		fp->fa3_rdev.specdata2 = txdr_unsigned(minor(vap->va_rdev));
		fp->fa3_fsid.nfsuquad[0] = 0;
		fp->fa3_fsid.nfsuquad[1] = txdr_unsigned(vap->va_fsid);
		fp->fa3_fileid.nfsuquad[0] = 0;
		fp->fa3_fileid.nfsuquad[1] = txdr_unsigned(vap->va_fileid);
		txdr_nfsv3time(&vap->va_atime, &fp->fa3_atime);
		txdr_nfsv3time(&vap->va_mtime, &fp->fa3_mtime);
		txdr_nfsv3time(&vap->va_ctime, &fp->fa3_ctime);
	} else {
		fp->fa_type = vtonfsv2_type(vap->va_type);
		fp->fa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		fp->fa2_size = txdr_unsigned(vap->va_size);
		fp->fa2_blocksize = txdr_unsigned(vap->va_blocksize);
		if (vap->va_type == VFIFO)
			fp->fa2_rdev = 0xffffffff;
		else
			fp->fa2_rdev = txdr_unsigned(vap->va_rdev);
		fp->fa2_blocks = txdr_unsigned(vap->va_bytes / NFS_FABLKSIZE);
		fp->fa2_fsid = txdr_unsigned(vap->va_fsid);
		fp->fa2_fileid = txdr_unsigned(vap->va_fileid);
		txdr_nfsv2time(&vap->va_atime, &fp->fa2_atime);
		txdr_nfsv2time(&vap->va_mtime, &fp->fa2_mtime);
		txdr_nfsv2time(&vap->va_ctime, &fp->fa2_ctime);
	}
}

/*
 * nfsrv_fhtovp() - convert a fh to a vnode ptr (optionally locked)
 * 	- look up fsid in mount list (if not found ret error)
 *	- get vp and export rights by calling VFS_FHTOVP()
 *	- if cred->cr_uid == 0 or MNT_EXPORTANON set it to credanon
 */
int
nfsrv_fhtovp(fhandle_t *fhp, int flags, struct vnode **vpp, int *vfslockedp,
    struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct sockaddr *nam, int *rdonlyp)
{
	struct mount *mp;
	int i;
	struct ucred *cred, *credanon;
	int error, exflags;
#ifdef MNT_EXNORESPORT		/* XXX needs mountd and /etc/exports help yet */
	struct sockaddr_int *saddr;
#endif
	int credflavor;
	int vfslocked;
	int numsecflavors, *secflavors;
	int authsys;
	int v3 = nfsd->nd_flag & ND_NFSV3;
	int mountreq;

	*vfslockedp = 0;
	*vpp = NULL;

	if (nfs_ispublicfh(fhp)) {
		if (!nfs_pub.np_valid)
			return (ESTALE);
		fhp = &nfs_pub.np_handle;
	}

	mp = vfs_busyfs(&fhp->fh_fsid);
	if (!mp)
		return (ESTALE);
	vfslocked = VFS_LOCK_GIANT(mp);
	error = VFS_CHECKEXP(mp, nam, &exflags, &credanon,
	    &numsecflavors, &secflavors);
	if (error) {
		vfs_unbusy(mp);
		goto out;
	}
	if (numsecflavors == 0) {
		/*
		 * This can happen if the system is running with an
		 * old mountd that doesn't pass in a secflavor list.
		 */
		numsecflavors = 1;
		authsys = AUTH_SYS;
		secflavors = &authsys;
	}
	credflavor = nfsd->nd_credflavor;
	for (i = 0; i < numsecflavors; i++) {
		if (secflavors[i] == credflavor)
			break;
	}
	if (i == numsecflavors) {
		/*
		 * RFC 2623 section 2.3.2 - allow certain procedures
		 * used at NFS client mount time even if they have
		 * weak authentication.
		 */
		mountreq = FALSE;
		if (v3) {
			if (nfsd->nd_procnum == NFSPROC_FSINFO
			    || nfsd->nd_procnum == NFSPROC_GETATTR)
				mountreq = TRUE;
		} else {
			if (nfsd->nd_procnum == NFSPROC_FSSTAT
			    || nfsd->nd_procnum == NFSPROC_GETATTR)
				mountreq = TRUE;
		}
		if (!mountreq) {
			error = NFSERR_AUTHERR | AUTH_TOOWEAK;
			vfs_unbusy(mp);
			goto out;
		}
	}
	error = VFS_FHTOVP(mp, &fhp->fh_fid, vpp);
	if (error) {
		/* Make sure the server replies ESTALE to the client. */
		error = ESTALE;
		vfs_unbusy(mp);
		goto out;
	}
#ifdef MNT_EXNORESPORT
	if (!(exflags & (MNT_EXNORESPORT|MNT_EXPUBLIC))) {
		saddr = (struct sockaddr_in *)nam;
		if ((saddr->sin_family == AF_INET ||
		     saddr->sin_family == AF_INET6) &&
	/* same code for INET and INET6: sin*_port at same offet */
		    ntohs(saddr->sin_port) >= IPPORT_RESERVED) {
			vput(*vpp);
			*vpp = NULL;
			error = NFSERR_AUTHERR | AUTH_TOOWEAK;
			vfs_unbusy(mp);
			goto out;
		}
	}
#endif
	/*
	 * Check/setup credentials.
	 */
	cred = nfsd->nd_cr;
	if (cred->cr_uid == 0 || (exflags & MNT_EXPORTANON)) {
		cred->cr_uid = credanon->cr_uid;
		crsetgroups(cred, credanon->cr_ngroups, credanon->cr_groups);
	}
	if (exflags & MNT_EXRDONLY)
		*rdonlyp = 1;
	else
		*rdonlyp = 0;

	if (!(flags & NFSRV_FLAG_BUSY))
		vfs_unbusy(mp);
out:
	if (credanon != NULL)
		crfree(credanon);

	if (error) {
		VFS_UNLOCK_GIANT(vfslocked);
	} else
		*vfslockedp = vfslocked;
	return (error);
}


/*
 * WebNFS: check if a filehandle is a public filehandle. For v3, this
 * means a length of 0, for v2 it means all zeroes. nfsm_srvmtofh has
 * transformed this to all zeroes in both cases, so check for it.
 */
int
nfs_ispublicfh(fhandle_t *fhp)
{
	char *cp = (char *)fhp;
	int i;

	NFSD_LOCK_DONTCARE();

	for (i = 0; i < NFSX_V3FH; i++)
		if (*cp++ != 0)
			return (FALSE);
	return (TRUE);
}

/*
 * Map errnos to NFS error numbers. For Version 3 also filter out error
 * numbers not specified for the associated procedure.
 */
int
nfsrv_errmap(struct nfsrv_descript *nd, int err)
{
	const short *defaulterrp, *errp;
	int e;


	if (nd->nd_flag & ND_NFSV3) {
	    if (nd->nd_procnum <= NFSPROC_COMMIT) {
		errp = defaulterrp = nfsrv_v3errmap[nd->nd_procnum];
		while (*++errp) {
			if (*errp == err)
				return (err);
			else if (*errp > err)
				break;
		}
		return ((int)*defaulterrp);
	    } else
		return (err & 0xffff);
	}
	e = 0;
	if (err <= ELAST)
		e = nfsrv_v2errmap[err - 1];
	if (e != 0)
		return (e);
	return (NFSERR_IO);
}

/*
 * Sort the group list in increasing numerical order.
 * (Insertion sort by Chris Torek, who was grossed out by the bubble sort
 *  that used to be here.)
 */
void
nfsrvw_sort(gid_t *list, int num)
{
	int i, j;
	gid_t v;

	/* Insertion sort. */
	for (i = 1; i < num; i++) {
		v = list[i];
		/* find correct slot for value v, moving others up */
		for (j = i; --j >= 0 && v < list[j];)
			list[j + 1] = list[j];
		list[j + 1] = v;
	}
}

/*
 * Helper functions for macros.
 */

void
nfsm_srvfhtom_xx(fhandle_t *f, int v3, struct mbuf **mb, caddr_t *bpos)
{
	u_int32_t *tl;

	if (v3) {
		tl = nfsm_build_xx(NFSX_UNSIGNED + NFSX_V3FH, mb, bpos);
		*tl++ = txdr_unsigned(NFSX_V3FH);
		bcopy(f, tl, NFSX_V3FH);
	} else {
		tl = nfsm_build_xx(NFSX_V2FH, mb, bpos);
		bcopy(f, tl, NFSX_V2FH);
	}
}

void
nfsm_srvpostop_fh_xx(fhandle_t *f, struct mbuf **mb, caddr_t *bpos)
{
	u_int32_t *tl;

	tl = nfsm_build_xx(2 * NFSX_UNSIGNED + NFSX_V3FH, mb, bpos);
	*tl++ = nfsrv_nfs_true;
	*tl++ = txdr_unsigned(NFSX_V3FH);
	bcopy(f, tl, NFSX_V3FH);
}

int
nfsm_srvstrsiz_xx(int *s, int m, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;

	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	*s = fxdr_unsigned(int32_t, *tl);
	if (*s > m || *s <= 0)
		return EBADRPC;
	return 0;
}

int
nfsm_srvnamesiz_xx(int *s, int m, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;

	NFSD_LOCK_DONTCARE();

	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	*s = fxdr_unsigned(int32_t, *tl);
	if (*s > m)
		return NFSERR_NAMETOL;
	if (*s <= 0)
		return EBADRPC;
	return 0;
}

int
nfsm_srvnamesiz0_xx(int *s, int m, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;

	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	*s = fxdr_unsigned(int32_t, *tl);
	if (*s > m)
		return NFSERR_NAMETOL;
	if (*s < 0)
		return EBADRPC;
	return 0;
}

void
nfsm_clget_xx(u_int32_t **tl, struct mbuf *mb, struct mbuf **mp,
    char **bp, char **be, caddr_t bpos)
{
	struct mbuf *nmp;

	NFSD_UNLOCK_ASSERT();

	if (*bp >= *be) {
		if (*mp == mb)
			(*mp)->m_len += *bp - bpos;
		MGET(nmp, M_WAIT, MT_DATA);
		MCLGET(nmp, M_WAIT);
		nmp->m_len = NFSMSIZ(nmp);
		(*mp)->m_next = nmp;
		*mp = nmp;
		*bp = mtod(*mp, caddr_t);
		*be = *bp + (*mp)->m_len;
	}
	*tl = (u_int32_t *)*bp;
}

int
nfsm_srvmtofh_xx(fhandle_t *f, int v3, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;
	int fhlen;

	if (v3) {
		tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		fhlen = fxdr_unsigned(int, *tl);
		if (fhlen != 0 && fhlen != NFSX_V3FH)
			return EBADRPC;
	} else {
		fhlen = NFSX_V2FH;
	}
	if (fhlen != 0) {
		tl = nfsm_dissect_xx_nonblock(fhlen, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		bcopy((caddr_t)tl, (caddr_t)(f), fhlen);
	} else {
		bzero((caddr_t)(f), NFSX_V3FH);
	}
	return 0;
}

int
nfsm_srvsattr_xx(struct vattr *a, struct mbuf **md, caddr_t *dpos)
{
	u_int32_t *tl;
	int toclient = 0;

	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	if (*tl == nfsrv_nfs_true) {
		tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		(a)->va_mode = nfstov_mode(*tl);
	}
	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	if (*tl == nfsrv_nfs_true) {
		tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		(a)->va_uid = fxdr_unsigned(uid_t, *tl);
	}
	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	if (*tl == nfsrv_nfs_true) {
		tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		(a)->va_gid = fxdr_unsigned(gid_t, *tl);
	}
	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	if (*tl == nfsrv_nfs_true) {
		tl = nfsm_dissect_xx_nonblock(2 * NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		(a)->va_size = fxdr_hyper(tl);
	}
	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	switch (fxdr_unsigned(int, *tl)) {
	case NFSV3SATTRTIME_TOCLIENT:
		tl = nfsm_dissect_xx_nonblock(2 * NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		fxdr_nfsv3time(tl, &(a)->va_atime);
		toclient = 1;
		break;
	case NFSV3SATTRTIME_TOSERVER:
		getnanotime(&(a)->va_atime);
		a->va_vaflags |= VA_UTIMES_NULL;
		break;
	}
	tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
	if (tl == NULL)
		return EBADRPC;
	switch (fxdr_unsigned(int, *tl)) {
	case NFSV3SATTRTIME_TOCLIENT:
		tl = nfsm_dissect_xx_nonblock(2 * NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			return EBADRPC;
		fxdr_nfsv3time(tl, &(a)->va_mtime);
		a->va_vaflags &= ~VA_UTIMES_NULL;
		break;
	case NFSV3SATTRTIME_TOSERVER:
		getnanotime(&(a)->va_mtime);
		if (toclient == 0)
			a->va_vaflags |= VA_UTIMES_NULL;
		break;
	}
	return 0;
}
