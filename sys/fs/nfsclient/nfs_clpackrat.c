/*-
 * Copyright (c) 2009 Rick Macklem, University of Guelph
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <fs/nfs/nfsport.h>
#include <fs/nfsclient/nfs.h>

/*
 * These functions provide the packrat support for the NFSv4 client. That
 * is, they implement agressive client side whole file caching to a directory
 * on stable storage, such as a disk.
 */
#ifndef APPLEKEXT
NFSCLSTATEMUTEX;
static char nfscl_packratpath[MAXPATHLEN + 1];
static int nfscl_packratpathlen = 0;
static uint64_t nfscl_packratmaxsize = 10000000;
#endif	/* !APPLEKEXT */

#define	NFS_PACKRATIOSIZE	NFS_MAXBSIZE

static vnode_t nfscl_openfile(char *, boolean_t, struct ucred *, NFSPROC_T *);
static void nfscl_closefile(vnode_t, char *, boolean_t, struct ucred *,
    NFSPROC_T *);
static void start_packrat(void *);
static void nfscl_start_packratthread(struct nfscldeleg *);
static void nfscl_packratthread(struct nfscldeleg *, NFSPROC_T *);
static int nfsrpc_readdp(struct nfscldeleg *, struct uio *, struct ucred *,
    NFSPROC_T *);
static int nfsrpc_readrpcdp(struct nfscldeleg *, struct uio *, struct ucred *,
    NFSPROC_T *);
static void nfscl_fhtofilename(u_int8_t *, u_int16_t, char *);
static void nfscl_packratbreakdown(struct nfscldeleg *, struct nfsmount *,
    struct ucred *, NFSPROC_T *);
static void nfscl_updatewrite(struct nfscldeleg *, struct nfsldirty *);
static int nfscl_localwrite(struct nfscldeleg *, uint64_t, int,
    struct ucred *, struct ucred *, NFSPROC_T *, int);
static int nfsrpc_writedp(struct nfscldeleg *, uint8_t *, off_t, int,
    struct ucred *, NFSPROC_T *, int);
static int nfsrpc_writerpcdp(struct nfscldeleg *, uint8_t *, off_t, int,
    struct ucred *, NFSPROC_T *);
static void nfscl_truncdirty(struct nfscldeleg *, uint64_t);
static int nfscl_packrathostaddr(struct nfsmount *, char *, int);
static void nfscl_packratgetvp(struct nfscldeleg *, struct nfsmount *, char *,
    int, struct ucred *, NFSPROC_T *);

/*
 * This function opens/creates a file for reading and writing, returning the
 * vnode pointer for it.
 */
static vnode_t
nfscl_openfile(char *path, boolean_t createit, struct ucred *cred, NFSPROC_T *p)
{
	struct nameidata nd;
	int error, fmode;

	NDINIT_AT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE,
	    path, AT_FDCWD, p);
	if (createit)
		fmode = O_CREAT | O_TRUNC | O_RDWR;
	else
		fmode = O_RDWR;
	error = vn_open_cred(&nd, &fmode, 0600, VN_OPEN_NOAUDIT, cred, NULL);
	if (error == 0)
		NDFREE(&nd, NDF_ONLY_PNBUF);
	else if (nd.ni_vp != NULL)
		panic("nfsclopenf");
	return (nd.ni_vp);
}

/*
 * This function closes and removes a file that was previously created/opened
 * by nfscl_openfile().
 */
static void
nfscl_closefile(vnode_t vp, char *path, boolean_t unlinkit, struct ucred *cred,
    NFSPROC_T *p)
{

	(void) vn_close(vp, FREAD | FWRITE, cred, p);
	if (unlinkit)
		(void) kern_unlinkat(p, AT_FDCWD, path, UIO_SYSSPACE, 0);
}

/*
 * Start up a packrat thread to copy the file to local disk.
 */
static void
start_packrat(void *arg)
{
	struct nfscldeleg *dp;
	struct thread *td;

	dp = (struct nfscldeleg *)arg;
	td = TAILQ_FIRST(&dp->nfsdl_packratthread->p_threads);
	nfscl_packratthread(dp, td);
	kproc_exit(0);
}

static void
nfscl_start_packratthread(struct nfscldeleg *dp)
{

	kproc_create(start_packrat, (void *)dp, &dp->nfsdl_packratthread, 0, 0,
	    "nfspackrat");
}

/*
 * This is the body of the packrat copy thread. Just copy the file to the
 * local one on disk.
 */
static void
nfscl_packratthread(struct nfscldeleg *dp, NFSPROC_T *td)
{
	struct uio uio;
	struct iovec io;
	struct ucred *incred, *outcred;
	char *iobuf;
	int error;
	off_t off;
	ssize_t resid;

	incred = newnfs_getcred();
	outcred = newnfs_getcred();
	newnfs_copycred(&dp->nfsdl_cred, incred);
	iobuf = (char *)malloc(NFS_PACKRATIOSIZE, M_TEMP, M_WAITOK);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = 0;

	/* Loop around until eof */
	do {
		off = uio.uio_offset;
		uio.uio_resid = NFS_PACKRATIOSIZE;
		io.iov_len = NFS_PACKRATIOSIZE;
		io.iov_base = iobuf;
		uio.uio_iovcnt = 1;
		uio.uio_iov = &io;
		uio.uio_td = td;
		error = nfsrpc_readdp(dp, &uio, incred, td);

		/* and then write to the disk file */
		if (error == 0 && uio.uio_resid < NFS_PACKRATIOSIZE) {
			error = vn_rdwr(UIO_WRITE, dp->nfsdl_filevp,
			    iobuf, NFS_PACKRATIOSIZE - uio.uio_resid,
			    off, UIO_SYSSPACE, IO_NOMACCHECK, outcred,
			    NOCRED, &resid, td);
			if (error == 0) {
				dp->nfsdl_localsize = uio.uio_offset;
				wakeup(&dp->nfsdl_localsize);
			}
		}
	} while (error == 0 && uio.uio_resid < NFS_PACKRATIOSIZE);
	free(iobuf, M_TEMP);

	if (error == 0) {
		vnode_pager_setsize(dp->nfsdl_filevp, dp->nfsdl_localsize);
		NFSLOCKCLSTATE();
		dp->nfsdl_flags &= ~NFSCLDL_COPYINPROG;
		dp->nfsdl_flags |= NFSCLDL_HASCOPY;
		wakeup(&dp->nfsdl_flags);
		wakeup(&dp->nfsdl_localsize);
		NFSUNLOCKCLSTATE();
	} else
		nfscl_packratbreakdown(dp, dp->nfsdl_clp->nfsc_nmp, outcred,
		    td);
	NFSFREECRED(incred);
	NFSFREECRED(outcred);
}

/*
 * Read operation against an NFSv4 delegation.
 */
static int
nfsrpc_readdp(struct nfscldeleg *dp, struct uio *uiop, struct ucred *cred,
    NFSPROC_T *p)
{
	int error, expireret = 0, retrycnt;
	u_int32_t clidrev = 0;

	clidrev = dp->nfsdl_clp->nfsc_clientidrev;
	retrycnt = 0;
	do {
		error = nfsrpc_readrpcdp(dp, uiop, cred, p);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(dp->nfsdl_clp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID) {
			(void) nfs_catnap(PZERO, error, "nfs_readdp");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(dp->nfsdl_clp, clidrev, p);
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
	    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error && retrycnt >= 4)
		error = EIO;
	return (error);
}

/*
 * The actual read RPC for the above.
 */
static int
nfsrpc_readrpcdp(struct nfscldeleg *dp, struct uio *uiop, struct ucred *cred,
    NFSPROC_T *p)
{
	u_int32_t *tl;
	int error = 0, len, retlen, tsiz, eof = 0;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfsmount *nmp;

	nmp = dp->nfsdl_clp->nfsc_nmp;
	if (nmp == NULL)
		return (0);
	tsiz = uio_uio_resid(uiop);
	nd->nd_mrep = NULL;
	while (tsiz > 0) {
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		nfscl_reqstart(nd, NFSPROC_READ, nmp, dp->nfsdl_fh,
		    dp->nfsdl_fhlen, NULL, NULL);
		nfsm_stateidtom(nd, &dp->nfsdl_stateid, NFSSTATEID_PUTSTATEID);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED * 3);
		txdr_hyper(uiop->uio_offset, tl);
		*(tl + 2) = txdr_unsigned(len);
		error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL,
		    p, cred, NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
		if (error != 0)
			return (error);
		if (nd->nd_repstat != 0 || error != 0) {
			if (error == 0)
				error = nd->nd_repstat;
			goto nfsmout;
		}
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		eof = fxdr_unsigned(int, *tl);
		NFSM_STRSIZ(retlen, nmp->nm_rsize);
		error = nfsm_mbufuio(nd, uiop, retlen);
		if (error)
			goto nfsmout;
		mbuf_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
		tsiz -= retlen;
		if (eof || retlen == 0)
			tsiz = 0;
	}
	return (0);
nfsmout:
	if (nd->nd_mrep != NULL)
		mbuf_freem(nd->nd_mrep);
	return (error);
}

static u_int8_t *not64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$%";
static u_int8_t *hexval = "0123456789abcdef";
/*
 * This function generates a file name for the file handle passed in as an
 * argument. Since the file handle can be up to 128 bytes and a file name is
 * limited to 255 in length, a simple hexadecimal representation doesn't
 * work.
 * The name looks like this:
 * - 2 hex digits for the file handle length
 * - fhlen/3 sequences of four characters, where each ascii character
 *   represents 6 bits (4 digits representing the 3 bytes)
 * - 0 to 2 bytes represented as hexadecimal
 * As such, a 128 byte fh creates a 174 byte file name.
 * The name argument must point to sufficient storage.
 */
static void
nfscl_fhtofilename(u_int8_t *fh, u_int16_t fhlen, char *name)
{

	*name++ = hexval[fhlen & 0xf];
	*name++ = hexval[fhlen >> 4];

	/*
	 * Now loop around for the fh bytes until < 3 left.
	 */
	while (fhlen >= 3) {
		*name++ = not64[*fh & 0x3f];
		*name++ = not64[((*fh & 0xc0) >> 2) | (*(fh + 1) & 0xf)];
		*name++ = not64[(*(fh + 1) >> 4) | ((*(fh + 2) & 0xc0) >> 2)];
		*name++ = not64[*(fh + 2) & 0x3f];
		fh += 3;
		fhlen -= 3;
	}

	/*
	 * and then do the last 0 to 2 bytes as hexadecimal.
	 */
	while (fhlen > 0) {
		*name++ = hexval[*fh & 0xf];
		*name++ = hexval[*fh >> 4];
		fh++;
		fhlen--;
	}
	*name = '\0';
}

/*
 * Set up a new delegation for packrat support.
 */
void
nfscl_packratsetup(struct nfscldeleg *dp, struct nfsmount *nmp,
    struct ucred *cred, NFSPROC_T *p)
{
	vnode_t delegvp, filevp;
	char fname[MAXPATHLEN + 1];
	int pathlen, pathlen2;

	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0 ||
	    dp->nfsdl_opensize > nfscl_packratmaxsize) {
		dp->nfsdl_flags &= ~NFSCLDL_COPYINPROG;
		wakeup(&dp->nfsdl_flags);
		NFSUNLOCKCLSTATE();
		return;
	}
	pathlen = nfscl_packratpathlen;
	bcopy(nfscl_packratpath, &fname[0], pathlen);
	NFSUNLOCKCLSTATE();

	/*
	 * The appropriate fields of the delegation structure are already
	 * initialized, so create the files and, if that is successful,
	 * fire up a packrat thread for it.
	 */
	pathlen2 = pathlen;
	fname[pathlen++] = 'D';
	pathlen = nfscl_packrathostaddr(nmp, &fname[0], pathlen);
	if (pathlen == 0) {
		NFSLOCKCLSTATE();
		dp->nfsdl_flags &= ~NFSCLDL_COPYINPROG;
		wakeup(&dp->nfsdl_flags);
		NFSUNLOCKCLSTATE();
		return;
	}
	nfscl_fhtofilename(dp->nfsdl_fh, dp->nfsdl_fhlen, &fname[pathlen]);
	delegvp = nfscl_openfile(fname, TRUE, cred, p);
	if (delegvp == NULL) {
		NFSLOCKCLSTATE();
		dp->nfsdl_flags &= ~NFSCLDL_COPYINPROG;
		wakeup(&dp->nfsdl_flags);
		NFSUNLOCKCLSTATE();
		return;
	}
	VOP_UNLOCK(delegvp, 0);
	fname[pathlen2] = 'F';
	filevp = nfscl_openfile(fname, TRUE, cred, p);
	if (filevp == NULL) {
		nfscl_closefile(delegvp, fname, TRUE, cred, p);
		NFSLOCKCLSTATE();
		dp->nfsdl_flags &= ~NFSCLDL_COPYINPROG;
		wakeup(&dp->nfsdl_flags);
		NFSUNLOCKCLSTATE();
		return;
	}
	VOP_UNLOCK(filevp, 0);

	NFSLOCKCLSTATE();
	dp->nfsdl_delegvp = delegvp;
	dp->nfsdl_filevp = filevp;
	dp->nfsdl_localsize = 0;
	NFSUNLOCKCLSTATE();

	nfscl_start_packratthread(dp);
}

/*
 * Reopen the locally cached copy of the file, created by nfscl_packratsetup().
 */
void
nfscl_packratopen(vnode_t vp, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;
	struct ucred *cred;
	char fname[MAXPATHLEN + 1];
	int pathlen;

	nmp = VFSTONFS(vnode_mount(vp));
	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0 || (nmp->nm_flag & NFSMNT_NFSV4) == 0) {
		NFSUNLOCKCLSTATE();
		return;
	}
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp == NULL || (dp->nfsdl_flags &
	    (NFSCLDL_RECALL | NFSCLDL_WRITE | NFSCLDL_HASCOPY)) !=
	    (NFSCLDL_WRITE | NFSCLDL_HASCOPY) ||
	    dp->nfsdl_filevp != NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}

	/* Now, try and reopen them. */
	pathlen = nfscl_packratpathlen;
	bcopy(nfscl_packratpath, &fname[0], pathlen);
	NFSUNLOCKCLSTATE();

	cred = newnfs_getcred();
	nfscl_packratgetvp(dp, nmp, fname, pathlen, cred, p);
	NFSFREECRED(cred);
}

/*
 * Do the actual reopens to get the vnodes.
 */
static void
nfscl_packratgetvp(struct nfscldeleg *dp, struct nfsmount *nmp, char *fname,
    int pathlen, struct ucred *cred, NFSPROC_T *p)
{
	vnode_t delegvp, filevp;
	boolean_t closethem;
	int pathlen2;

	/*
	 * The appropriate fields of the delegation structure are already
	 * initialized, so create the files and, if that is successful,
	 * fire up a packrat thread for it.
	 */
	pathlen2 = pathlen;
	fname[pathlen++] = 'D';
	pathlen = nfscl_packrathostaddr(nmp, &fname[0], pathlen);
	if (pathlen == 0)
		return;
	nfscl_fhtofilename(dp->nfsdl_fh, dp->nfsdl_fhlen, &fname[pathlen]);
	delegvp = nfscl_openfile(fname, FALSE, cred, p);
	if (delegvp == NULL)
		return;
	VOP_UNLOCK(delegvp, 0);
	fname[pathlen2] = 'F';
	filevp = nfscl_openfile(fname, FALSE, cred, p);
	if (filevp == NULL) {
		nfscl_closefile(delegvp, fname, FALSE, cred, p);
		return;
	}
	VOP_UNLOCK(filevp, 0);

	NFSLOCKCLSTATE();
	if (dp->nfsdl_delegvp == NULL) {
		dp->nfsdl_delegvp = delegvp;
		dp->nfsdl_filevp = filevp;
		closethem = FALSE;
	} else
		closethem = TRUE;
	NFSUNLOCKCLSTATE();

	if (closethem) {
		fname[pathlen2] = 'D';
		nfscl_closefile(delegvp, fname, FALSE, cred, p);
		fname[pathlen2] = 'F';
		nfscl_closefile(filevp, fname, FALSE, cred, p);
	}
}

/*
 * Get rid of the local files related to a delegation.
 */
static void
nfscl_packratbreakdown(struct nfscldeleg *dp, struct nfsmount *nmp,
    struct ucred *cred, NFSPROC_T *p)
{
	vnode_t delegvp, filevp;
	char fname[MAXPATHLEN + 1];
	int pathlen, pathlen2;

	/*
	 * Get the vnode pointers out of the delegation structure and
	 * null them out.
	 */
	NFSLOCKCLSTATE();
	pathlen = nfscl_packratpathlen;
	if (pathlen > 0)
		bcopy(nfscl_packratpath, &fname[0], pathlen);
	delegvp = dp->nfsdl_delegvp;
	filevp = dp->nfsdl_filevp;
	dp->nfsdl_delegvp = NULL;
	dp->nfsdl_filevp = NULL;
	pathlen2 = pathlen;
	if (pathlen > 0) {
		fname[pathlen++] = 'D';
		pathlen = nfscl_packrathostaddr(nmp, &fname[0], pathlen);
	}
	if (pathlen > 0)
		nfscl_fhtofilename(dp->nfsdl_fh, dp->nfsdl_fhlen,
		    &fname[pathlen]);
	while (dp->nfsdl_localiocnt > 0)
		(void) nfsmsleep(&dp->nfsdl_localiocnt, NFSCLSTATEMUTEXPTR,
		    PZERO, "nfspckbr", NULL);
	if ((dp->nfsdl_flags & NFSCLDL_COPYINPROG) != 0) {
		dp->nfsdl_flags &= ~NFSCLDL_COPYINPROG;
		wakeup(&dp->nfsdl_flags);
	}
	NFSUNLOCKCLSTATE();
	if (pathlen == 0)
		return;

	/*
	 * Close/remove the 2 files.
	 */
	if (delegvp != NULL)
		nfscl_closefile(delegvp, fname, TRUE, cred, p);
	fname[pathlen2] = 'F';
	if (filevp != NULL)
		nfscl_closefile(filevp, fname, TRUE, cred, p);
}

/*
 * Close the local files related to a delegation in order to release the
 * vnodes.
 */
void
nfscl_packratclose(vnode_t vp, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;
	struct ucred *cred;
	vnode_t delegvp, filevp;
	char fname[MAXPATHLEN + 1];
	int pathlen, pathlen2;

	nmp = VFSTONFS(vnode_mount(vp));
	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0 || (nmp->nm_flag & NFSMNT_NFSV4) == 0 ||
	    vp->v_usecount > 1) {
		NFSUNLOCKCLSTATE();
		return;
	}
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp == NULL || (dp->nfsdl_flags &
	    (NFSCLDL_RECALL | NFSCLDL_WRITE | NFSCLDL_HASCOPY)) !=
	    (NFSCLDL_WRITE | NFSCLDL_HASCOPY) ||
	    dp->nfsdl_filevp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	pathlen = nfscl_packratpathlen;
	bcopy(nfscl_packratpath, &fname[0], pathlen);
	delegvp = dp->nfsdl_delegvp;
	filevp = dp->nfsdl_filevp;
	dp->nfsdl_delegvp = NULL;
	dp->nfsdl_filevp = NULL;
	while (dp->nfsdl_localiocnt > 0)
		(void) nfsmsleep(&dp->nfsdl_localiocnt, NFSCLSTATEMUTEXPTR,
		    PZERO, "nfspckbr", NULL);
	NFSUNLOCKCLSTATE();
	pathlen2 = pathlen;
	fname[pathlen++] = 'D';
	pathlen = nfscl_packrathostaddr(nmp, &fname[0], pathlen);
	if (pathlen == 0)
		return;
	nfscl_fhtofilename(np->n_fhp->nfh_fh, np->n_fhp->nfh_len,
	    &fname[pathlen]);

	/*
	 * Close the 2 files.
	 */
	cred = newnfs_getcred();
	nfscl_closefile(delegvp, fname, FALSE, cred, p);
	fname[pathlen2] = 'F';
	nfscl_closefile(filevp, fname, FALSE, cred, p);
	NFSFREECRED(cred);
}

/*
 * Set up the packrat directory.
 */
int
nfscbd_packrat(char *pathbuf)
{
	int pathlen;

	pathlen = strlen(pathbuf);
	if (pathlen < 1 || pathbuf[pathlen - 1] != '/')
		return (EINVAL);
	NFSLOCKCLSTATE();
	strcpy(nfscl_packratpath, pathbuf);
	nfscl_packratpathlen = pathlen;
	NFSUNLOCKCLSTATE();
	return (0);
}

/*
 * Do a read via a local cached copy, if possible.
 * Return whether or not the local read was possible via "didread".
 */
int
nfscl_packratread(vnode_t vp, struct uio *uio, int ioflag, struct ucred *cred,
    int *didread)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;
	vnode_t filevp;
	int error;
	ssize_t tresid;

	*didread = 0;
	nmp = VFSTONFS(vnode_mount(vp));
	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0 || (nmp->nm_flag & NFSMNT_NFSV4) == 0) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp == NULL || (dp->nfsdl_flags &
	    (NFSCLDL_RECALL | NFSCLDL_WRITE)) != NFSCLDL_WRITE ||
	    dp->nfsdl_filevp == NULL ||
	    ((dp->nfsdl_flags & NFSCLDL_COPYINPROG) != 0 &&
	     uio->uio_offset + uio->uio_resid > dp->nfsdl_localsize)) {
		if (dp != NULL && (dp->nfsdl_flags & NFSCLDL_RECALL) != 0) {
			dp->nfsdl_flags |= NFSCLDL_WAITRECALL;
			(void) nfsmsleep(&dp->nfsdl_ldirty, NFSCLSTATEMUTEXPTR,
			    PZERO, "nfspkrc", NULL);
		}
		NFSUNLOCKCLSTATE();
		NFSLOCKNODE(np);
		NFSINVALATTRCACHE(np);
		np->n_flag &= ~NLOCALCACHE;
		NFSUNLOCKNODE(np);
		return (0);
	}

	*didread = 1;
	if (uio->uio_offset >= dp->nfsdl_localsize) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	if (uio->uio_offset + uio->uio_resid > dp->nfsdl_localsize) {
		tresid = uio->uio_resid;
		uio->uio_resid = dp->nfsdl_localsize - uio->uio_offset;
		tresid -= uio->uio_resid;
	} else
		tresid = 0;
	dp->nfsdl_localiocnt++;
	filevp = dp->nfsdl_filevp;
	NFSUNLOCKCLSTATE();
	error = 0;
	vn_lock(filevp, LK_SHARED | LK_RETRY);
	VI_LOCK(filevp);
	if ((filevp->v_iflag & VI_DOOMED) != 0)
		error = ENOENT;
	VI_UNLOCK(filevp);
	if (error == 0)
		error = VOP_READ(filevp, uio, ioflag, cred);
	VOP_UNLOCK(filevp, 0);
	uio->uio_resid += tresid;
	NFSLOCKCLSTATE();
	dp->nfsdl_localiocnt--;
	if (dp->nfsdl_localiocnt == 0)
		wakeup(&dp->nfsdl_localiocnt);
	NFSUNLOCKCLSTATE();
	return (error);
}

/*
 * Do a write into a local copy cached on disk, if possible.
 * Return whether or not the local write was possible via "didwrite".
 */
int
nfscl_packratwrite(vnode_t vp, struct uio *uio, int ioflag, struct ucred *cred,
    NFSPROC_T *p, int *didwrite)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;
	struct mount *mp;
	struct nfsldirty *wp;
	vnode_t filevp;
	uint64_t end;
	int error;
	off_t setsize;

	*didwrite = 0;
	nmp = VFSTONFS(vnode_mount(vp));
	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0 || (nmp->nm_flag & NFSMNT_NFSV4) == 0) {
		NFSUNLOCKCLSTATE();
		NFSLOCKNODE(np);
		np->n_flag &= ~NLOCALCACHE;
		NFSUNLOCKNODE(np);
		return (0);
	}
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		NFSLOCKNODE(np);
		np->n_flag &= ~NLOCALCACHE;
		NFSUNLOCKNODE(np);
		return (0);
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp == NULL || (dp->nfsdl_flags &
	    (NFSCLDL_RECALL | NFSCLDL_WRITE)) != NFSCLDL_WRITE ||
	    dp->nfsdl_filevp == NULL) {
		if (dp != NULL && (dp->nfsdl_flags & NFSCLDL_RECALL) != 0) {
			dp->nfsdl_flags |= NFSCLDL_WAITRECALL;
			(void) nfsmsleep(&dp->nfsdl_ldirty, NFSCLSTATEMUTEXPTR,
			    PZERO, "nfspkwc", NULL);
		}
		NFSUNLOCKCLSTATE();
		NFSLOCKNODE(np);
		np->n_flag &= ~NLOCALCACHE;
		NFSUNLOCKNODE(np);
		return (0);
	}

	/*
	 * We can now try and do the write. It cannot be done until the
	 * local copy has been read in to past the point at which we
	 * are writing, so we must loop until enough reading has been
	 * completed.
	 */
	end = (uint64_t)uio->uio_offset + uio->uio_resid;
	while ((dp->nfsdl_flags & NFSCLDL_COPYINPROG) != 0 &&
	    (end > dp->nfsdl_localsize || (ioflag & IO_APPEND) != 0))
		(void) nfsmsleep(&dp->nfsdl_localsize, NFSCLSTATEMUTEXPTR,
		    PZERO, "nfspckw", NULL);
	filevp = dp->nfsdl_filevp;
	if (filevp == NULL) {
		NFSUNLOCKCLSTATE();
		NFSLOCKNODE(np);
		np->n_flag &= ~NLOCALCACHE;
		NFSUNLOCKNODE(np);
		return (0);
	}
	if ((ioflag & IO_APPEND) != 0) {
		uio->uio_offset = dp->nfsdl_localsize;
		end = (uint64_t)uio->uio_offset + uio->uio_resid;
		ioflag &= ~IO_APPEND;
	}
	*didwrite = 1;
	if (uio->uio_offset < 0) {
		NFSUNLOCKCLSTATE();
		return (EINVAL);
	}
	if (end > nmp->nm_maxfilesize) {
		NFSUNLOCKCLSTATE();
		return (EFBIG);
	}
	if (uio->uio_resid == 0) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	dp->nfsdl_localiocnt++;
	NFSUNLOCKCLSTATE();
	NFSLOCKNODE(np);
	np->n_flag |= (NMODIFIED | NLOCALCACHE);
	NFSUNLOCKNODE(np);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (p != NULL && vn_rlimit_fsize(vp, uio, p))
		error = EFBIG;
	else
		error = 0;

	wp = (struct nfsldirty *)malloc(sizeof(struct nfsldirty),
	    M_NFSLDIRTY, M_WAITOK);
	wp->nfsw_first = (uint64_t)uio->uio_offset;
	wp->nfsw_end = end;
	mp = NULL;
	if (error == 0)
		error = vn_start_write(filevp, &mp, V_WAIT);
	if (error == 0) {
		if (MNT_SHARED_WRITES(mp) || ((mp == NULL) &&
		    MNT_SHARED_WRITES(filevp->v_mount)))
			vn_lock(filevp, LK_SHARED | LK_RETRY);
		else
			vn_lock(filevp, LK_EXCLUSIVE | LK_RETRY);
		VI_LOCK(filevp);
		if ((filevp->v_iflag & VI_DOOMED) != 0)
			error = ENOENT;
		VI_UNLOCK(filevp);
		if (error == 0)
			error = VOP_WRITE(filevp, uio, ioflag, cred);
		vn_finished_write(mp);
		VOP_UNLOCK(filevp, 0);
	}
	setsize = 0;
	NFSLOCKCLSTATE();
	if (error == 0) {
		if (end > dp->nfsdl_localsize) {
			dp->nfsdl_localsize = end;
			dp->nfsdl_flags |= NFSCLDL_LOCALSIZESET;
			setsize = end;
		}
		nfscl_updatewrite(dp, wp);
	} else
		free(wp, M_NFSLDIRTY);
	dp->nfsdl_localiocnt--;
	if (dp->nfsdl_localiocnt == 0)
		wakeup(&dp->nfsdl_localiocnt);
	NFSUNLOCKCLSTATE();
	if (setsize != 0)
		vnode_pager_setsize(vp, setsize);
	return (error);
}

/*
 * Add/merge this byte range to the list of modified byte ranges.
 */
static void
nfscl_updatewrite(struct nfscldeleg *dp, struct nfsldirty *newwp)
{
	struct nfsldirty *wp, *nwp, *owp, *onwp;

	/* Loop through the list of dirty byte ranges. */
	wp = LIST_FIRST(&dp->nfsdl_ldirty);
	owp = NULL;
	while (wp != NULL) {
		nwp = LIST_NEXT(wp, nfsw_list);
		if (newwp->nfsw_first > wp->nfsw_end) {
			/* Not there yet. */
			owp = wp;
			wp = nwp;
		} else if (newwp->nfsw_end < wp->nfsw_first) {
			/* No overlap between old and new, insert it. */
			LIST_INSERT_BEFORE(wp, newwp, nfsw_list);
			newwp = NULL;
			break;
		} else {
			/* Merge this range in. */
			if (newwp->nfsw_first < wp->nfsw_first)
				wp->nfsw_first = newwp->nfsw_first;
			if (newwp->nfsw_end > wp->nfsw_end)
				wp->nfsw_end = newwp->nfsw_end;
			free(newwp, M_NFSLDIRTY);
			newwp = NULL;

			/* Now, free any entries comsumed by this one. */
			while (nwp != NULL &&
			    nwp->nfsw_first <= wp->nfsw_end) {
				if (nwp->nfsw_end > wp->nfsw_end)
					/* Must be last overlapping range */
					wp->nfsw_end = nwp->nfsw_end;
				onwp = nwp;
				nwp = LIST_NEXT(nwp, nfsw_list);
				LIST_REMOVE(onwp, nfsw_list);
				free(onwp, M_NFSLDIRTY);
			}
			break;
		}
	}
	/* If not yet handled, it goes at the end of the list. */
	if (newwp != NULL) {
		if (owp != NULL)
			LIST_INSERT_AFTER(owp, newwp, nfsw_list);
		else
			LIST_INSERT_HEAD(&dp->nfsdl_ldirty, newwp, nfsw_list);
	}
}

/*
 * Flush any dirty byte ranges on a locally cached copy back to the server.
 * Return non-zero if the write back fails.
 */
int
nfscl_deleglocalflush(struct nfscldeleg *dp, struct nfsmount *nmp, NFSPROC_T *p,
    int called_from_renewthread, int called_from_remove)
{
	struct nfsldirty *wp, *nwp;
	struct ucred *incred, *outcred;
	char fname[MAXPATHLEN + 1];
	int error, pathlen;

	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0) {
		if ((dp->nfsdl_flags & NFSCLDL_WAITRECALL) != 0)
			wakeup(&dp->nfsdl_ldirty);
		NFSUNLOCKCLSTATE();
		return (0);
	}

	/* Wait for the packrat thread to complete. */
	while ((dp->nfsdl_flags & NFSCLDL_COPYINPROG) != 0)
		(void) nfsmsleep(&dp->nfsdl_flags, NFSCLSTATEMUTEXPTR,
		    PZERO, "nfspckth", NULL);

	if ((dp->nfsdl_flags & NFSCLDL_WRITE) == 0 ||
	    (dp->nfsdl_filevp == NULL &&
	     (dp->nfsdl_flags & NFSCLDL_HASCOPY) == 0)) {
		if ((dp->nfsdl_flags & NFSCLDL_WAITRECALL) != 0)
			wakeup(&dp->nfsdl_ldirty);
		NFSUNLOCKCLSTATE();
		return (0);
	}
	/* Now, try and reopen them. */
	pathlen = nfscl_packratpathlen;
	bcopy(nfscl_packratpath, &fname[0], pathlen);
	NFSUNLOCKCLSTATE();

	incred = newnfs_getcred();
	outcred = newnfs_getcred();
	newnfs_copycred(&dp->nfsdl_cred, outcred);

	/* Open the files, as required. */
	if (dp->nfsdl_filevp == NULL)
		nfscl_packratgetvp(dp, nmp, fname, pathlen, incred, p);

	/* Loop through the dirty byte range(s), writing them back. */
	error = 0;
	wp = LIST_FIRST(&dp->nfsdl_ldirty);
	while (wp != NULL && error == 0) {
		nwp = LIST_NEXT(wp, nfsw_list);
		if (called_from_remove == 0)
			error = nfscl_localwrite(dp, wp->nfsw_first,
			    (int)(wp->nfsw_end - wp->nfsw_first), incred,
			    outcred, p, called_from_renewthread);
		if (error == 0) {
			/* Get rid of the byte range. */
			LIST_REMOVE(wp, nfsw_list);
			free(wp, M_NFSLDIRTY);
		}
		wp = nwp;
	}

	if (error == 0) {
		NFSLOCKCLSTATE();
		if ((dp->nfsdl_flags & NFSCLDL_WAITRECALL) != 0)
			wakeup(&dp->nfsdl_ldirty);
		NFSUNLOCKCLSTATE();
		/* Now, the local files can be closed/deleted. */
		nfscl_packratbreakdown(dp, nmp, incred, p);
	}
	NFSFREECRED(incred);
	NFSFREECRED(outcred);
	return (error);
}

/*
 * Copy a byte range from the local cache file to the server.
 */
static int
nfscl_localwrite(struct nfscldeleg *dp, uint64_t start, int len,
    struct ucred *incred, struct ucred *outcred, NFSPROC_T *td,
    int called_from_renewthread)
{
	char *iobuf;
	int resid, error, xfer;
	off_t off;

	iobuf = (char *)malloc(NFS_PACKRATIOSIZE, M_TEMP, M_WAITOK);
	off = start;

	/* Loop around until done. */
	do {
		xfer = min(NFS_PACKRATIOSIZE, len);

		/* Read the bytes from the local file. */
		error = vn_rdwr(UIO_READ, dp->nfsdl_filevp,
		    iobuf, xfer, off, UIO_SYSSPACE, IO_NOMACCHECK, incred,
		    NOCRED, &resid, td);
		if (error == 0)
			error = nfsrpc_writedp(dp, iobuf, off, xfer, outcred,
			    td, called_from_renewthread);
		len -= xfer;
		off += xfer;
	} while (error == 0 && len > 0);
	free(iobuf, M_TEMP);
	return (error);
}

/*
 * Write operation against an NFSv4 delegation.
 */
static int
nfsrpc_writedp(struct nfscldeleg *dp, uint8_t *buf, off_t off, int len,
    struct ucred *cred, NFSPROC_T *p, int called_from_renewthread)
{
	int error, expireret = 0, retrycnt;
	u_int32_t clidrev = 0;

	clidrev = dp->nfsdl_clp->nfsc_clientidrev;
	retrycnt = 0;
	do {
		error = nfsrpc_writerpcdp(dp, buf, off, len, cred, p);
		if (error == NFSERR_STALESTATEID)
			nfscl_initiate_recovery(dp->nfsdl_clp);
		if (error == NFSERR_GRACE || error == NFSERR_STALESTATEID ||
		    error == NFSERR_STALEDONTRECOVER || error == NFSERR_DELAY ||
		    error == NFSERR_OLDSTATEID) {
			(void) nfs_catnap(PZERO, error, "nfs_writedp");
		} else if ((error == NFSERR_EXPIRED ||
		    error == NFSERR_BADSTATEID) && clidrev != 0) {
			expireret = nfscl_hasexpired(dp->nfsdl_clp, clidrev, p);
		}
		retrycnt++;
	} while (error == NFSERR_GRACE || error == NFSERR_DELAY ||
	    ((error == NFSERR_STALESTATEID ||
	      error == NFSERR_STALEDONTRECOVER) &&
	     called_from_renewthread == 0) ||
	    (error == NFSERR_OLDSTATEID && retrycnt < 20) ||
	    ((error == NFSERR_EXPIRED || error == NFSERR_BADSTATEID) &&
	     expireret == 0 && clidrev != 0 && retrycnt < 4));
	if (error != 0 && (retrycnt >= 4 ||
	    ((error == NFSERR_STALESTATEID ||
	      error == NFSERR_STALEDONTRECOVER) &&
	     called_from_renewthread != 0)))
		error = EIO;
	return (error);
}

/*
 * The actual write RPC for the above.
 */
static int
nfsrpc_writerpcdp(struct nfscldeleg *dp, uint8_t *buf, off_t off, int tlen,
    struct ucred *cred, NFSPROC_T *p)
{
	u_int32_t *tl;
	int error = 0, len, pos;
	struct nfsrv_descript nfsd;
	struct nfsrv_descript *nd = &nfsd;
	struct nfsmount *nmp;
	nfsattrbit_t attrbits;

	nmp = dp->nfsdl_clp->nfsc_nmp;
	if (nmp == NULL)
		return (0);
	NFSWRITEGETATTR_ATTRBIT(&attrbits);
	pos = 0;
	nd->nd_mrep = NULL;
	while (tlen > 0) {
		len = (tlen > nmp->nm_wsize) ? nmp->nm_wsize : tlen;
		nfscl_reqstart(nd, NFSPROC_WRITE, nmp, dp->nfsdl_fh,
		    dp->nfsdl_fhlen, NULL, NULL);
		nfsm_stateidtom(nd, &dp->nfsdl_stateid, NFSSTATEID_PUTSTATEID);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED * 3);
		txdr_hyper(off, tl);
		*(tl + 2) = txdr_unsigned(NFSWRITE_FILESYNC);
		(void) nfsm_strtom(nd, &buf[pos], len);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = txdr_unsigned(NFSV4OP_GETATTR);
		(void) nfsrv_putattrbit(nd, &attrbits);
		error = newnfs_request(nd, nmp, NULL, &nmp->nm_sockreq, NULL,
		    p, cred, NFS_PROG, NFS_VER4, NULL, 1, NULL, NULL);
		if (error != 0)
			return (error);
		if (nd->nd_repstat != 0 || error != 0) {
			if (error == 0)
				error = nd->nd_repstat;
			goto nfsmout;
		}
		mbuf_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
		tlen -= len;
		pos += len;
	}
	return (0);
nfsmout:
	if (nd->nd_mrep != NULL)
		mbuf_freem(nd->nd_mrep);
	return (error);
}

/*
 * Handle a Setattr of size for a write delegated file. Basically
 * update nfsdl_localsize plus get rid of any
 * dirty region(s) that no longer apply.
 */
void
nfscl_packratsetsize(vnode_t vp, uint64_t size)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;

	nmp = VFSTONFS(vnode_mount(vp));
	NFSLOCKCLSTATE();
	if (nfscl_packratpathlen == 0 || (nmp->nm_flag & NFSMNT_NFSV4) == 0) {
		NFSUNLOCKCLSTATE();
		return;
	}
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp == NULL || (dp->nfsdl_flags &
	    (NFSCLDL_RECALL | NFSCLDL_WRITE)) != NFSCLDL_WRITE ||
	    (dp->nfsdl_filevp == NULL &&
	     (dp->nfsdl_flags & NFSCLDL_HASCOPY) == 0) ||
	    dp->nfsdl_localsize == size) {
		NFSUNLOCKCLSTATE();
		return;
	}

	if (size < dp->nfsdl_localsize)
		/* Get rid of dirty region(s) that no longer apply. */
		nfscl_truncdirty(dp, size);
	/* Update the local size. */
	dp->nfsdl_localsize = size;
	dp->nfsdl_flags |= NFSCLDL_LOCALSIZESET;
	NFSUNLOCKCLSTATE();
	vnode_pager_setsize(vp, size);
}

/*
 * Get rid of any dirty region(s) that no longer apply after truncation.
 */
static void
nfscl_truncdirty(struct nfscldeleg *dp, uint64_t size)
{
	struct nfsldirty *wp, *nwp;

	wp = LIST_FIRST(&dp->nfsdl_ldirty);
	while (wp != NULL) {
		nwp = LIST_NEXT(wp, nfsw_list);
		if (wp->nfsw_first >= size) {
			/* Entire region is past end, so free it up. */
			LIST_REMOVE(wp, nfsw_list);
			free(wp, M_NFSLDIRTY);
		} else if (wp->nfsw_end > size)
			/* Region is being truncated. */
			wp->nfsw_end = size;
		wp = nwp;
	}
}

/*
 * Put the server's ip address in the file name.
 */
static int
nfscl_packrathostaddr(struct nfsmount *nmp, char *fname, int pathlen)
{
	struct sockaddr_in *saddr;
	struct sockaddr_in6 *saddr6;
	char iptype, *addr;
	int len, i;

	if (nmp->nm_sockreq.nr_nam->sa_family == AF_INET) {
		iptype = '4';
		saddr = (struct sockaddr_in *)nmp->nm_sockreq.nr_nam;
	} else if (nmp->nm_sockreq.nr_nam->sa_family == AF_INET6) {
		iptype = '6';
		saddr6 = (struct sockaddr_in6 *)nmp->nm_sockreq.nr_nam;
	} else
		return (0);

	fname[pathlen++] = iptype;
	if (iptype == '4') {
		addr = inet_ntoa(saddr->sin_addr);
		len = strlen(addr);
		bcopy(addr, &fname[pathlen], len);
		pathlen += len;
	} else {
		for (i = 0; i < 16; i++) {
			sprintf(&fname[pathlen], "%2x",
			    saddr6->sin6_addr.s6_addr[i]);
			pathlen += 2;
		}
	}
	return (pathlen);
}

