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

/*
 * These functions implement the client side state handling for NFSv4.
 * NFSv4 state handling:
 * - A lockowner is used to determine lock contention, so it
 *   corresponds directly to a Posix pid. (1 to 1 mapping)
 * - The correct granularity of an OpenOwner is not nearly so
 *   obvious. An OpenOwner does the following:
 *   - provides a serial sequencing of Open/Close/Lock-with-new-lockowner
 *   - is used to check for Open/Share contention (not applicable to
 *     this client, since all Opens are Deny_None)
 *   As such, I considered both extreme.
 *   1 OpenOwner per ClientID - Simple to manage, but fully serializes
 *   all Open, Close and Lock (with a new lockowner) Ops.
 *   1 OpenOwner for each Open - This one results in an OpenConfirm for
 *   every Open, for most servers.
 *   So, I chose to use the same mapping as I did for LockOwnwers.
 *   The main concern here is that you can end up with multiple Opens
 *   for the same File Handle, but on different OpenOwners (opens
 *   inherited from parents, grandparents...) and you do not know
 *   which of these the vnodeop close applies to. This is handled by
 *   delaying the Close Op(s) until all of the Opens have been closed.
 *   (It is not yet obvious if this is the correct granularity.)
 * - How the code handles serialization:
 *   - For the ClientId, it uses an exclusive lock while getting its
 *     SetClientId and during recovery. Otherwise, it uses a shared
 *     lock via a reference count.
 *   - For the rest of the data structures, it uses an SMP mutex
 *     (once the nfs client is SMP safe) and doesn't sleep while
 *     manipulating the linked lists.
 *   - The serialization of Open/Close/Lock/LockU falls out in the
 *     "wash", since OpenOwners and LockOwners are both mapped from
 *     Posix pid. In other words, there is only one Posix pid using
 *     any given owner, so that owner is serialized. (If you change
 *     the granularity of the OpenOwner, then code must be added to
 *     serialize Ops on the OpenOwner.)
 * - When to get rid of OpenOwners and LockOwners.
 *   - When a process exits, it calls nfscl_cleanup(), which goes
 *     through the client list looking for all Open and Lock Owners.
 *     When one is found, it is marked "defunct" or in the case of
 *     an OpenOwner without any Opens, freed.
 *     The renew thread scans for defunct Owners and gets rid of them,
 *     if it can. The LockOwners will also be deleted when the
 *     associated Open is closed.
 *   - If the LockU or Close Op(s) fail during close in a way
 *     that could be recovered upon retry, they are relinked to the
 *     ClientId's defunct open list and retried by the renew thread
 *     until they succeed or an unmount/recovery occurs.
 *     (Since we are done with them, they do not need to be recovered.)
 */

#ifndef APPLEKEXT
#include <fs/nfs/nfsport.h>

/*
 * Global variables
 */
extern struct nfsstats newnfsstats;
extern struct nfsreqhead nfsd_reqq;
NFSREQSPINLOCK;
NFSCLSTATEMUTEX;
int nfscl_inited = 0;
struct nfsclhead nfsclhead;	/* Head of clientid list */
int nfscl_deleghighwater = NFSCLDELEGHIGHWATER;
#endif	/* !APPLEKEXT */

static int nfscl_delegcnt = 0;
static int nfscl_getopen(struct nfsclownerhead *, u_int8_t *, int, u_int8_t *,
    NFSPROC_T *, u_int32_t, struct nfsclowner **, struct nfsclopen **);
static void nfscl_clrelease(struct nfsclclient *);
static void nfscl_cleanclient(struct nfsclclient *);
static void nfscl_expireclient(struct nfsclclient *, struct nfsmount *,
    struct ucred *, NFSPROC_T *);
static int nfscl_expireopen(struct nfsclclient *, struct nfsclopen *,
    struct nfsmount *, struct ucred *, NFSPROC_T *);
static void nfscl_recover(struct nfsclclient *, struct ucred *, NFSPROC_T *);
static void nfscl_insertlock(struct nfscllockowner *, struct nfscllock *,
    struct nfscllock *, int);
static int nfscl_updatelock(struct nfscllockowner *, struct nfscllock **,
    struct nfscllock **, int);
static void nfscl_delegreturnall(struct nfsclclient *, NFSPROC_T *);
static u_int32_t nfscl_nextcbident(void);
static mount_t nfscl_getmnt(u_int32_t);
static struct nfscldeleg *nfscl_finddeleg(struct nfsclclient *, u_int8_t *,
    int);
static int nfscl_checkconflict(struct nfscllockownerhead *, struct nfscllock *,
    u_int8_t *, struct nfscllock **);
static void nfscl_freelockowner(struct nfscllockowner *, int);
static void nfscl_freealllocks(struct nfscllockownerhead *, int);
static int nfscl_localconflict(struct nfsclclient *, u_int8_t *, int,
    struct nfscllock *, u_int8_t *, struct nfscldeleg *, struct nfscllock **);
static void nfscl_newopen(struct nfsclclient *, struct nfscldeleg *,
    struct nfsclowner **, struct nfsclowner **, struct nfsclopen **,
    struct nfsclopen **, u_int8_t *, u_int8_t *, int, int *);
static int nfscl_moveopen(vnode_t , struct nfsclclient *,
    struct nfsmount *, struct nfsclopen *, struct nfsclowner *,
    struct nfscldeleg *, struct ucred *, NFSPROC_T *);
static void nfscl_totalrecall(struct nfsclclient *);
static int nfscl_relock(vnode_t , struct nfsclclient *, struct nfsmount *,
    struct nfscllockowner *, struct nfscllock *, struct ucred *, NFSPROC_T *);
static int nfscl_tryopen(struct nfsmount *, vnode_t , u_int8_t *, int,
    u_int8_t *, int, u_int32_t, struct nfsclopen *, u_int8_t *, int,
    struct nfscldeleg **, int, u_int32_t, struct ucred *, NFSPROC_T *);
static int nfscl_trylock(struct nfsmount *, vnode_t , u_int8_t *,
    int, struct nfscllockowner *, int, int, u_int64_t, u_int64_t, short,
    struct ucred *, NFSPROC_T *);
static int nfsrpc_reopen(struct nfsmount *, u_int8_t *, int, u_int32_t,
    struct nfsclopen *, struct nfscldeleg **, struct ucred *, NFSPROC_T *);
static void nfscl_freedeleg(struct nfscldeleghead *, struct nfscldeleg *);
static int nfscl_errmap(struct nfsrv_descript *);
static void nfscl_cleanup_common(struct nfsclclient *, u_int8_t *);
static int nfscl_recalldeleg(struct nfsclclient *, struct nfsmount *,
    struct nfscldeleg *, vnode_t, struct ucred *, NFSPROC_T *, int);
static void nfscl_freeopenowner(struct nfsclowner *, int);
static void nfscl_cleandeleg(struct nfscldeleg *);
static int nfscl_trydelegreturn(struct nfscldeleg *, struct ucred *,
    struct nfsmount *, NFSPROC_T *);

static short nfscberr_null[] = {
	0,
	0,
};

static short nfscberr_getattr[] = {
	NFSERR_RESOURCE,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfscberr_recall[] = {
	NFSERR_RESOURCE,
	NFSERR_BADHANDLE,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	0,
};

static short *nfscl_cberrmap[] = {
	nfscberr_null,
	nfscberr_null,
	nfscberr_null,
	nfscberr_getattr,
	nfscberr_recall
};

#define	NETFAMILY(clp) \
		(((clp)->nfsc_flags & NFSCLFLAGS_AFINET6) ? AF_INET6 : AF_INET)

/*
 * Called for an open operation.
 * If the nfhp argument is NULL, just get an openowner.
 */
APPLESTATIC int
nfscl_open(vnode_t vp, u_int8_t *nfhp, int fhlen, u_int32_t amode, int usedeleg,
    struct ucred *cred, NFSPROC_T *p, struct nfsclowner **owpp,
    struct nfsclopen **opp, int *newonep, int *retp, int lockit)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op = NULL, *nop = NULL;
	struct nfscldeleg *dp;
	struct nfsclownerhead *ohp;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];
	int ret;

	if (newonep != NULL)
		*newonep = 0;
	if (opp != NULL)
		*opp = NULL;
	if (owpp != NULL)
		*owpp = NULL;

	/*
	 * Might need one or both of these, so MALLOC them now, to
	 * avoid a tsleep() in MALLOC later.
	 */
	MALLOC(nowp, struct nfsclowner *, sizeof (struct nfsclowner),
	    M_NFSCLOWNER, M_WAITOK);
	if (nfhp != NULL)
	    MALLOC(nop, struct nfsclopen *, sizeof (struct nfsclopen) +
		fhlen - 1, M_NFSCLOPEN, M_WAITOK);
	ret = nfscl_getcl(vp, cred, p, &clp);
	if (ret != 0) {
		FREE((caddr_t)nowp, M_NFSCLOWNER);
		if (nop != NULL)
			FREE((caddr_t)nop, M_NFSCLOPEN);
		return (ret);
	}

	/*
	 * Get the Open iff it already exists.
	 * If none found, add the new one or return error, depending upon
	 * "create".
	 */
	nfscl_filllockowner(p, own);
	NFSLOCKCLSTATE();
	dp = NULL;
	/* First check the delegation list */
	if (nfhp != NULL && usedeleg) {
		LIST_FOREACH(dp, NFSCLDELEGHASH(clp, nfhp, fhlen), nfsdl_hash) {
			if (dp->nfsdl_fhlen == fhlen &&
			    !NFSBCMP(nfhp, dp->nfsdl_fh, fhlen)) {
				if (!(amode & NFSV4OPEN_ACCESSWRITE) ||
				    (dp->nfsdl_flags & NFSCLDL_WRITE))
					break;
				dp = NULL;
				break;
			}
		}
	}

	if (dp != NULL)
		ohp = &dp->nfsdl_owner;
	else
		ohp = &clp->nfsc_owner;
	/* Now, search for an openowner */
	LIST_FOREACH(owp, ohp, nfsow_list) {
		if (!NFSBCMP(owp->nfsow_owner, own, NFSV4CL_LOCKNAMELEN))
			break;
	}

	/*
	 * Create a new open, as required.
	 */
	nfscl_newopen(clp, dp, &owp, &nowp, &op, &nop, own, nfhp, fhlen,
	    newonep);

	/*
	 * Serialize modifications to the open owner for multiple threads
	 * within the same process using a read/write sleep lock.
	 */
	if (lockit)
		nfscl_lockexcl(&owp->nfsow_rwlock, NFSCLSTATEMUTEXPTR);
	NFSUNLOCKCLSTATE();
	if (nowp != NULL)
		FREE((caddr_t)nowp, M_NFSCLOWNER);
	if (nop != NULL)
		FREE((caddr_t)nop, M_NFSCLOPEN);
	if (owpp != NULL)
		*owpp = owp;
	if (opp != NULL)
		*opp = op;
	if (retp != NULL) {
		if (nfhp != NULL && dp != NULL && nop == NULL)
			/* new local open on delegation */
			*retp = NFSCLOPEN_SETCRED;
		else
			*retp = NFSCLOPEN_OK;
	}

	/*
	 * Now, check the mode on the open and return the appropriate
	 * value.
	 */
	if (op != NULL && (amode & ~(op->nfso_mode))) {
		op->nfso_mode |= amode;
		if (retp != NULL && dp == NULL)
			*retp = NFSCLOPEN_DOOPEN;
	}
	return (0);
}

/*
 * Create a new open, as required.
 */
static void
nfscl_newopen(struct nfsclclient *clp, struct nfscldeleg *dp,
    struct nfsclowner **owpp, struct nfsclowner **nowpp, struct nfsclopen **opp,
    struct nfsclopen **nopp, u_int8_t *own, u_int8_t *fhp, int fhlen,
    int *newonep)
{
	struct nfsclowner *owp = *owpp, *nowp;
	struct nfsclopen *op, *nop;

	if (nowpp != NULL)
		nowp = *nowpp;
	else
		nowp = NULL;
	if (nopp != NULL)
		nop = *nopp;
	else
		nop = NULL;
	if (owp == NULL && nowp != NULL) {
		NFSBCOPY(own, nowp->nfsow_owner, NFSV4CL_LOCKNAMELEN);
		LIST_INIT(&nowp->nfsow_open);
		nowp->nfsow_clp = clp;
		nowp->nfsow_seqid = 0;
		nowp->nfsow_defunct = 0;
		nfscl_lockinit(&nowp->nfsow_rwlock);
		if (dp != NULL) {
			newnfsstats.cllocalopenowners++;
			LIST_INSERT_HEAD(&dp->nfsdl_owner, nowp, nfsow_list);
		} else {
			newnfsstats.clopenowners++;
			LIST_INSERT_HEAD(&clp->nfsc_owner, nowp, nfsow_list);
		}
		owp = *owpp = nowp;
		*nowpp = NULL;
		if (newonep != NULL)
			*newonep = 1;
	}

	 /* If an fhp has been specified, create an Open as well. */
	if (fhp != NULL) {
		/* and look for the correct open, based upon FH */
		LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
			if (op->nfso_fhlen == fhlen &&
			    !NFSBCMP(op->nfso_fh, fhp, fhlen))
				break;
		}
		if (op == NULL && nop != NULL) {
			nop->nfso_own = owp;
			nop->nfso_mode = 0;
			nop->nfso_opencnt = 0;
			nop->nfso_posixlock = 1;
			nop->nfso_fhlen = fhlen;
			NFSBCOPY(fhp, nop->nfso_fh, fhlen);
			LIST_INIT(&nop->nfso_lock);
			nop->nfso_stateid.seqid = 0;
			nop->nfso_stateid.other[0] = 0;
			nop->nfso_stateid.other[1] = 0;
			nop->nfso_stateid.other[2] = 0;
			if (dp != NULL) {
				TAILQ_REMOVE(&clp->nfsc_deleg, dp, nfsdl_list);
				TAILQ_INSERT_HEAD(&clp->nfsc_deleg, dp,
				    nfsdl_list);
				dp->nfsdl_timestamp = NFSD_MONOSEC + 120;
				newnfsstats.cllocalopens++;
			} else {
				newnfsstats.clopens++;
			}
			LIST_INSERT_HEAD(&owp->nfsow_open, nop, nfso_list);
			*opp = nop;
			*nopp = NULL;
			if (newonep != NULL)
				*newonep = 1;
		} else {
			*opp = op;
		}
	}
}

/*
 * Called to find/add a delegation to a client.
 */
APPLESTATIC int
nfscl_deleg(mount_t mp, struct nfsclclient *clp, u_int8_t *nfhp,
    int fhlen, struct ucred *cred, NFSPROC_T *p, struct nfscldeleg **dpp)
{
	struct nfscldeleg *dp = *dpp, *tdp;

	/*
	 * First, if we have received a Read delegation for a file on a
	 * read/write file system, just return it, because they aren't
	 * useful, imho.
	 */
	if (mp != NULL && dp != NULL && !NFSMNT_RDONLY(mp) &&
	    (dp->nfsdl_flags & NFSCLDL_READ)) {
		(void) nfscl_trydelegreturn(dp, cred, VFSTONFS(mp), p);
		FREE((caddr_t)dp, M_NFSCLDELEG);
		*dpp = NULL;
		return (0);
	}

	/* Look for the correct deleg, based upon FH */
	NFSLOCKCLSTATE();
	tdp = nfscl_finddeleg(clp, nfhp, fhlen);
	if (tdp == NULL) {
		if (dp == NULL) {
			NFSUNLOCKCLSTATE();
			return (NFSERR_BADSTATEID);
		}
		*dpp = NULL;
		TAILQ_INSERT_HEAD(&clp->nfsc_deleg, dp, nfsdl_list);
		LIST_INSERT_HEAD(NFSCLDELEGHASH(clp, nfhp, fhlen), dp,
		    nfsdl_hash);
		dp->nfsdl_timestamp = NFSD_MONOSEC + 120;
		newnfsstats.cldelegates++;
		nfscl_delegcnt++;
	} else {
		/*
		 * Delegation already exists, what do we do if a new one??
		 */
		if (dp != NULL) {
			printf("Deleg already exists!\n");
			FREE((caddr_t)dp, M_NFSCLDELEG);
			*dpp = NULL;
		} else {
			*dpp = tdp;
		}
	}
	NFSUNLOCKCLSTATE();
	return (0);
}

/*
 * Find a delegation for this file handle. Return NULL upon failure.
 */
static struct nfscldeleg *
nfscl_finddeleg(struct nfsclclient *clp, u_int8_t *fhp, int fhlen)
{
	struct nfscldeleg *dp;

	LIST_FOREACH(dp, NFSCLDELEGHASH(clp, fhp, fhlen), nfsdl_hash) {
	    if (dp->nfsdl_fhlen == fhlen &&
		!NFSBCMP(dp->nfsdl_fh, fhp, fhlen))
		break;
	}
	return (dp);
}

/*
 * Get a stateid for an I/O operation. First, look for an open and iff
 * found, return either a lockowner stateid or the open stateid.
 * If no Open is found, just return error and the special stateid of all zeros.
 */
APPLESTATIC int
nfscl_getstateid(vnode_t vp, u_int8_t *nfhp, int fhlen, u_int32_t mode,
    struct ucred *cred, NFSPROC_T *p, nfsv4stateid_t *stateidp,
    void **lckpp)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp;
	struct nfsclopen *op = NULL;
	struct nfscllockowner *lp;
	struct nfscldeleg *dp;
	struct nfsnode *np;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];
	int error, done;

	*lckpp = NULL;
	/*
	 * Initially, just set the special stateid of all zeros.
	 */
	stateidp->seqid = 0;
	stateidp->other[0] = 0;
	stateidp->other[1] = 0;
	stateidp->other[2] = 0;
	if (vnode_vtype(vp) != VREG)
		return (EISDIR);
	np = VTONFS(vp);
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(VFSTONFS(vnode_mount(vp)));
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return (EACCES);
	}

	/*
	 * Wait for recovery to complete.
	 */
	while ((clp->nfsc_flags & NFSCLFLAGS_RECVRINPROG))
		(void) nfsmsleep(&clp->nfsc_flags, NFSCLSTATEMUTEXPTR,
		    PZERO, "nfsrecvr", NULL);

	/*
	 * First, look for a delegation.
	 */
	LIST_FOREACH(dp, NFSCLDELEGHASH(clp, nfhp, fhlen), nfsdl_hash) {
		if (dp->nfsdl_fhlen == fhlen &&
		    !NFSBCMP(nfhp, dp->nfsdl_fh, fhlen)) {
			if (!(mode & NFSV4OPEN_ACCESSWRITE) ||
			    (dp->nfsdl_flags & NFSCLDL_WRITE)) {
				stateidp->seqid = dp->nfsdl_stateid.seqid;
				stateidp->other[0] = dp->nfsdl_stateid.other[0];
				stateidp->other[1] = dp->nfsdl_stateid.other[1];
				stateidp->other[2] = dp->nfsdl_stateid.other[2];
				if (!(np->n_flag & NDELEGRECALL)) {
					TAILQ_REMOVE(&clp->nfsc_deleg, dp,
					    nfsdl_list);
					TAILQ_INSERT_HEAD(&clp->nfsc_deleg, dp,
					    nfsdl_list);
					dp->nfsdl_timestamp = NFSD_MONOSEC +
					    120;
					dp->nfsdl_rwlock.nfslock_usecnt++;
					*lckpp = (void *)&dp->nfsdl_rwlock;
				}
				NFSUNLOCKCLSTATE();
				return (0);
			}
			break;
		}
	}

	if (p != NULL) {
		/*
		 * If p != NULL, we want to search the parentage tree
		 * for a matching OpenOwner and use that.
		 */
		nfscl_filllockowner(p, own);
		error = nfscl_getopen(&clp->nfsc_owner, nfhp, fhlen, NULL, p,
		    mode, NULL, &op);
		if (error == 0) {
			/* now look for a lockowner */
			LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
				if (!NFSBCMP(lp->nfsl_owner, own,
				    NFSV4CL_LOCKNAMELEN)) {
					stateidp->seqid =
					    lp->nfsl_stateid.seqid;
					stateidp->other[0] =
					    lp->nfsl_stateid.other[0];
					stateidp->other[1] =
					    lp->nfsl_stateid.other[1];
					stateidp->other[2] =
					    lp->nfsl_stateid.other[2];
					NFSUNLOCKCLSTATE();
					return (0);
				}
			}
		}
	}
	if (op == NULL) {
		/* If not found, just look for any OpenOwner that will work. */
		done = 0;
		owp = LIST_FIRST(&clp->nfsc_owner);
		while (!done && owp != NULL) {
			LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
				if (op->nfso_fhlen == fhlen &&
				    !NFSBCMP(op->nfso_fh, nfhp, fhlen) &&
				    (mode & op->nfso_mode) == mode) {
					done = 1;
					break;
				}
			}
			if (!done)
				owp = LIST_NEXT(owp, nfsow_list);
		}
		if (!done) {
			NFSUNLOCKCLSTATE();
			return (ENOENT);
		}
		/* for read aheads or write behinds, use the open cred */
		newnfs_copycred(&op->nfso_cred, cred);
	}

	/*
	 * No lock stateid, so return the open stateid.
	 */
	stateidp->seqid = op->nfso_stateid.seqid;
	stateidp->other[0] = op->nfso_stateid.other[0];
	stateidp->other[1] = op->nfso_stateid.other[1];
	stateidp->other[2] = op->nfso_stateid.other[2];
	NFSUNLOCKCLSTATE();
	return (0);
}

/*
 * Get an existing open. Search up the parentage tree for a match and
 * return with the first one found.
 */
static int
nfscl_getopen(struct nfsclownerhead *ohp, u_int8_t *nfhp, int fhlen,
    u_int8_t *rown, NFSPROC_T *p, u_int32_t mode, struct nfsclowner **owpp,
    struct nfsclopen **opp)
{
	struct nfsclowner *owp = NULL;
	struct nfsclopen *op;
	NFSPROC_T *nproc;
	u_int8_t own[NFSV4CL_LOCKNAMELEN], *ownp;

	nproc = p;
	op = NULL;
	while (op == NULL && (nproc != NULL || rown != NULL)) {
		if (nproc != NULL) {
			nfscl_filllockowner(nproc, own);
			ownp = own;
		} else {
			ownp = rown;
		}
		/* Search the client list */
		LIST_FOREACH(owp, ohp, nfsow_list) {
			if (!NFSBCMP(owp->nfsow_owner, ownp,
			    NFSV4CL_LOCKNAMELEN))
				break;
		}
		if (owp != NULL) {
			/* and look for the correct open */
			LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
				if (op->nfso_fhlen == fhlen &&
				    !NFSBCMP(op->nfso_fh, nfhp, fhlen)
				    && (op->nfso_mode & mode) == mode) {
					break;
				}
			}
		}
		if (rown != NULL)
			break;
		if (op == NULL)
			nproc = nfscl_getparent(nproc);
	}
	if (op == NULL) {
		return (EBADF);
	}
	if (owpp)
		*owpp = owp;
	*opp = op;
	return (0);
}

/*
 * Release use of an open owner. Called when open operations are done
 * with the open owner.
 */
APPLESTATIC void
nfscl_ownerrelease(struct nfsclowner *owp, __unused int error,
    __unused int candelete, int unlocked)
{

	if (owp == NULL)
		return;
	NFSLOCKCLSTATE();
	if (!unlocked)
		nfscl_lockunlock(&owp->nfsow_rwlock);
	nfscl_clrelease(owp->nfsow_clp);
	NFSUNLOCKCLSTATE();
}

/*
 * Release use of an open structure under an open owner.
 */
APPLESTATIC void
nfscl_openrelease(struct nfsclopen *op, int error, int candelete)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp;

	if (op == NULL)
		return;
	NFSLOCKCLSTATE();
	owp = op->nfso_own;
	nfscl_lockunlock(&owp->nfsow_rwlock);
	clp = owp->nfsow_clp;
	if (error && candelete && op->nfso_opencnt == 0)
		nfscl_freeopen(op, 0);
	nfscl_clrelease(clp);
	NFSUNLOCKCLSTATE();
}

/*
 * Called to get a clientid structure. It will optionally lock the
 * client data structures to do the SetClientId/SetClientId_confirm,
 * but will release that lock and return the clientid with a refernce
 * count on it.
 * If the "cred" argument is NULL, a new clientid should not be created.
 * If the "p" argument is NULL, a SetClientID/SetClientIDConfirm cannot
 * be done.
 * It always clpp with a reference count on it, unless returning an error.
 */
APPLESTATIC int
nfscl_getcl(vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    struct nfsclclient **clpp)
{
	struct nfsclclient *clp;
	struct nfsclclient *newclp = NULL;
	struct nfscllockowner *lp, *nlp;
	struct nfsmount *nmp = VFSTONFS(vnode_mount(vp));
	char uuid[HOSTUUIDLEN];
	int igotlock = 0, error, trystalecnt, clidinusedelay, i;
	u_int16_t idlen = 0;

	if (cred != NULL) {
		getcredhostuuid(cred, uuid, sizeof uuid);
		idlen = strlen(uuid);
		if (idlen > 0)
			idlen += sizeof (u_int64_t);
		else
			idlen += sizeof (u_int64_t) + 16; /* 16 random bytes */
		MALLOC(newclp, struct nfsclclient *,
		    sizeof (struct nfsclclient) + idlen - 1, M_NFSCLCLIENT,
		    M_WAITOK);
	}
	NFSLOCKCLSTATE();
	clp = nmp->nm_clp;
	if (clp == NULL) {
		if (newclp == NULL) {
			NFSUNLOCKCLSTATE();
			return (EACCES);
		}
		clp = newclp;
		NFSBZERO((caddr_t)clp, sizeof(struct nfsclclient) + idlen - 1);
		clp->nfsc_idlen = idlen;
		LIST_INIT(&clp->nfsc_owner);
		TAILQ_INIT(&clp->nfsc_deleg);
		for (i = 0; i < NFSCLDELEGHASHSIZE; i++)
			LIST_INIT(&clp->nfsc_deleghash[i]);
		LIST_INIT(&clp->nfsc_defunctlockowner);
		clp->nfsc_flags = NFSCLFLAGS_INITED;
		clp->nfsc_clientidrev = 1;
		clp->nfsc_cbident = nfscl_nextcbident();
		nfscl_fillclid(nmp->nm_clval, uuid, clp->nfsc_id,
		    clp->nfsc_idlen);
		LIST_INSERT_HEAD(&nfsclhead, clp, nfsc_list);
		nmp->nm_clp = clp;
		clp->nfsc_nmp = nmp;
		NFSUNLOCKCLSTATE();
		nfscl_start_renewthread(clp);
	} else {
		NFSUNLOCKCLSTATE();
		if (newclp != NULL)
			FREE((caddr_t)newclp, M_NFSCLCLIENT);
	}
	NFSLOCKCLSTATE();
	while ((clp->nfsc_flags & NFSCLFLAGS_HASCLIENTID) == 0 && !igotlock)
		igotlock = nfsv4_lock(&clp->nfsc_lock, 1, NULL,
		    NFSCLSTATEMUTEXPTR);
	if (!igotlock)
		nfsv4_getref(&clp->nfsc_lock, NULL, NFSCLSTATEMUTEXPTR);
	NFSUNLOCKCLSTATE();

	/*
	 * If it needs a clientid, do the setclientid now.
	 */
	if ((clp->nfsc_flags & NFSCLFLAGS_HASCLIENTID) == 0) {
		if (!igotlock)
			panic("nfscl_clget");
		if (p == NULL || cred == NULL) {
			NFSLOCKCLSTATE();
			nfsv4_unlock(&clp->nfsc_lock, 0);
			NFSUNLOCKCLSTATE();
			return (EACCES);
		}
		/* get rid of defunct lockowners */
		LIST_FOREACH_SAFE(lp, &clp->nfsc_defunctlockowner, nfsl_list,
		    nlp) {
			nfscl_freelockowner(lp, 0);
		}
		/*
		 * If RFC3530 Sec. 14.2.33 is taken literally,
		 * NFSERR_CLIDINUSE will be returned persistently for the
		 * case where a new mount of the same file system is using
		 * a different principal. In practice, NFSERR_CLIDINUSE is
		 * only returned when there is outstanding unexpired state
		 * on the clientid. As such, try for twice the lease
		 * interval, if we know what that is. Otherwise, make a
		 * wild ass guess.
		 * The case of returning NFSERR_STALECLIENTID is far less
		 * likely, but might occur if there is a significant delay
		 * between doing the SetClientID and SetClientIDConfirm Ops,
		 * such that the server throws away the clientid before
		 * receiving the SetClientIDConfirm.
		 */
		if (clp->nfsc_renew > 0)
			clidinusedelay = NFSCL_LEASE(clp->nfsc_renew) * 2;
		else
			clidinusedelay = 120;
		trystalecnt = 3;
		do {
			error = nfsrpc_setclient(VFSTONFS(vnode_mount(vp)),
			    clp, cred, p);
			if (error == NFSERR_STALECLIENTID ||
			    error == NFSERR_STALEDONTRECOVER ||
			    error == NFSERR_CLIDINUSE) {
				(void) nfs_catnap(PZERO, error, "nfs_setcl");
			}
		} while (((error == NFSERR_STALECLIENTID ||
		     error == NFSERR_STALEDONTRECOVER) && --trystalecnt > 0) ||
		    (error == NFSERR_CLIDINUSE && --clidinusedelay > 0));
		if (error) {
			NFSLOCKCLSTATE();
			nfsv4_unlock(&clp->nfsc_lock, 0);
			NFSUNLOCKCLSTATE();
			return (error);
		}
		clp->nfsc_flags |= NFSCLFLAGS_HASCLIENTID;
	}
	if (igotlock) {
		NFSLOCKCLSTATE();
		nfsv4_unlock(&clp->nfsc_lock, 1);
		NFSUNLOCKCLSTATE();
	}

	*clpp = clp;
	return (0);
}

/*
 * Get a reference to a clientid and return it, if valid.
 */
APPLESTATIC struct nfsclclient *
nfscl_findcl(struct nfsmount *nmp)
{
	struct nfsclclient *clp;

	clp = nmp->nm_clp;
	if (clp == NULL || !(clp->nfsc_flags & NFSCLFLAGS_HASCLIENTID))
		return (NULL);
	return (clp);
}

/*
 * Release the clientid structure. It may be locked or reference counted.
 */
static void
nfscl_clrelease(struct nfsclclient *clp)
{

	if (clp->nfsc_lock.nfslock_lock & NFSV4LOCK_LOCK)
		nfsv4_unlock(&clp->nfsc_lock, 0);
	else
		nfsv4_relref(&clp->nfsc_lock);
}

/*
 * External call for nfscl_clrelease.
 */
APPLESTATIC void
nfscl_clientrelease(struct nfsclclient *clp)
{

	NFSLOCKCLSTATE();
	if (clp->nfsc_lock.nfslock_lock & NFSV4LOCK_LOCK)
		nfsv4_unlock(&clp->nfsc_lock, 0);
	else
		nfsv4_relref(&clp->nfsc_lock);
	NFSUNLOCKCLSTATE();
}

/*
 * Called when wanting to lock a byte region.
 */
APPLESTATIC int
nfscl_getbytelock(vnode_t vp, u_int64_t off, u_int64_t len,
    short type, struct ucred *cred, NFSPROC_T *p, struct nfsclclient *rclp,
    int recovery, u_int8_t *rownp, u_int8_t *ropenownp,
    struct nfscllockowner **lpp, int *newonep, int *donelocallyp)
{
	struct nfscllockowner *lp;
	struct nfsclopen *op;
	struct nfsclclient *clp;
	struct nfscllockowner *nlp;
	struct nfscllock *nlop, *otherlop;
	struct nfscldeleg *dp = NULL, *ldp = NULL;
	struct nfscllockownerhead *lhp = NULL;
	struct nfsnode *np;
	u_int8_t own[NFSV4CL_LOCKNAMELEN], *ownp;
	int error = 0, ret, donelocally = 0;
	u_int32_t mode;

	if (type == F_WRLCK)
		mode = NFSV4OPEN_ACCESSWRITE;
	else
		mode = NFSV4OPEN_ACCESSREAD;
	np = VTONFS(vp);
	*lpp = NULL;
	*newonep = 0;
	*donelocallyp = 0;

	/*
	 * Might need these, so MALLOC them now, to
	 * avoid a tsleep() in MALLOC later.
	 */
	MALLOC(nlp, struct nfscllockowner *,
	    sizeof (struct nfscllockowner), M_NFSCLLOCKOWNER, M_WAITOK);
	MALLOC(otherlop, struct nfscllock *,
	    sizeof (struct nfscllock), M_NFSCLLOCK, M_WAITOK);
	MALLOC(nlop, struct nfscllock *,
	    sizeof (struct nfscllock), M_NFSCLLOCK, M_WAITOK);
	nlop->nfslo_type = type;
	nlop->nfslo_first = off;
	if (len == NFS64BITSSET) {
		nlop->nfslo_end = NFS64BITSSET;
	} else {
		nlop->nfslo_end = off + len;
		if (nlop->nfslo_end <= nlop->nfslo_first)
			error = NFSERR_INVAL;
	}

	if (!error) {
		if (recovery)
			clp = rclp;
		else
			error = nfscl_getcl(vp, cred, p, &clp);
	}
	if (error) {
		FREE((caddr_t)nlp, M_NFSCLLOCKOWNER);
		FREE((caddr_t)otherlop, M_NFSCLLOCK);
		FREE((caddr_t)nlop, M_NFSCLLOCK);
		return (error);
	}

	op = NULL;
	if (recovery) {
		ownp = rownp;
	} else {
		nfscl_filllockowner(p, own);
		ownp = own;
	}
	if (!recovery) {
		NFSLOCKCLSTATE();
		/*
		 * First, search for a delegation. If one exists for this file,
		 * the lock can be done locally against it, so long as there
		 * isn't a local lock conflict.
		 */
		ldp = dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh,
		    np->n_fhp->nfh_len);
		/* Just sanity check for correct type of delegation */
		if (dp != NULL && ((dp->nfsdl_flags & NFSCLDL_RECALL) ||
		    (type == F_WRLCK && !(dp->nfsdl_flags & NFSCLDL_WRITE))))
			dp = NULL;
	}
	if (dp != NULL) {
		/* Now, find the associated open to get the correct openowner */
		ret = nfscl_getopen(&dp->nfsdl_owner, np->n_fhp->nfh_fh,
		    np->n_fhp->nfh_len, NULL, p, mode, NULL, &op);
		if (ret)
			ret = nfscl_getopen(&clp->nfsc_owner,
			    np->n_fhp->nfh_fh, np->n_fhp->nfh_len, NULL, p,
			    mode, NULL, &op);
		if (!ret) {
			lhp = &dp->nfsdl_lock;
			TAILQ_REMOVE(&clp->nfsc_deleg, dp, nfsdl_list);
			TAILQ_INSERT_HEAD(&clp->nfsc_deleg, dp, nfsdl_list);
			dp->nfsdl_timestamp = NFSD_MONOSEC + 120;
			donelocally = 1;
		} else {
			dp = NULL;
		}
	}
	if (!donelocally) {
		/*
		 * Get the related Open.
		 */
		if (recovery)
			error = nfscl_getopen(&clp->nfsc_owner,
			    np->n_fhp->nfh_fh, np->n_fhp->nfh_len, ropenownp,
			    NULL, mode, NULL, &op);
		else
			error = nfscl_getopen(&clp->nfsc_owner,
			    np->n_fhp->nfh_fh, np->n_fhp->nfh_len, NULL, p,
			    mode, NULL, &op);
		if (!error)
			lhp = &op->nfso_lock;
	}
	if (!error && !recovery)
		error = nfscl_localconflict(clp, np->n_fhp->nfh_fh,
		    np->n_fhp->nfh_len, nlop, ownp, ldp, NULL);
	if (error) {
		if (!recovery) {
			nfscl_clrelease(clp);
			NFSUNLOCKCLSTATE();
		}
		FREE((caddr_t)nlp, M_NFSCLLOCKOWNER);
		FREE((caddr_t)otherlop, M_NFSCLLOCK);
		FREE((caddr_t)nlop, M_NFSCLLOCK);
		return (error);
	}

	/*
	 * Ok, see if a lockowner exists and create one, as required.
	 */
	LIST_FOREACH(lp, lhp, nfsl_list) {
		if (!NFSBCMP(lp->nfsl_owner, ownp, NFSV4CL_LOCKNAMELEN))
			break;
	}
	if (lp == NULL) {
		NFSBCOPY(ownp, nlp->nfsl_owner, NFSV4CL_LOCKNAMELEN);
		if (recovery)
			NFSBCOPY(ropenownp, nlp->nfsl_openowner,
			    NFSV4CL_LOCKNAMELEN);
		else
			NFSBCOPY(op->nfso_own->nfsow_owner, nlp->nfsl_openowner,
			    NFSV4CL_LOCKNAMELEN);
		nlp->nfsl_seqid = 0;
		nlp->nfsl_defunct = 0;
		nlp->nfsl_inprog = NULL;
		nfscl_lockinit(&nlp->nfsl_rwlock);
		LIST_INIT(&nlp->nfsl_lock);
		if (donelocally) {
			nlp->nfsl_open = NULL;
			newnfsstats.cllocallockowners++;
		} else {
			nlp->nfsl_open = op;
			newnfsstats.cllockowners++;
		}
		LIST_INSERT_HEAD(lhp, nlp, nfsl_list);
		lp = nlp;
		nlp = NULL;
		*newonep = 1;
	}

	/*
	 * Now, update the byte ranges for locks.
	 */
	ret = nfscl_updatelock(lp, &nlop, &otherlop, donelocally);
	if (!ret)
		donelocally = 1;
	if (donelocally) {
		*donelocallyp = 1;
		if (!recovery)
			nfscl_clrelease(clp);
	} else {
		/*
		 * Serial modifications on the lock owner for multiple threads
		 * for the same process using a read/write lock.
		 */
		if (!recovery)
			nfscl_lockexcl(&lp->nfsl_rwlock, NFSCLSTATEMUTEXPTR);
	}
	if (!recovery)
		NFSUNLOCKCLSTATE();

	if (nlp)
		FREE((caddr_t)nlp, M_NFSCLLOCKOWNER);
	if (nlop)
		FREE((caddr_t)nlop, M_NFSCLLOCK);
	if (otherlop)
		FREE((caddr_t)otherlop, M_NFSCLLOCK);

	*lpp = lp;
	return (0);
}

/*
 * Called to unlock a byte range, for LockU.
 */
APPLESTATIC int
nfscl_relbytelock(vnode_t vp, u_int64_t off, u_int64_t len,
    __unused struct ucred *cred, NFSPROC_T *p, int callcnt,
    struct nfsclclient *clp, struct nfscllockowner **lpp, int *dorpcp)
{
	struct nfscllockowner *lp;
	struct nfsclowner *owp;
	struct nfsclopen *op;
	struct nfscllock *nlop, *other_lop = NULL;
	struct nfscldeleg *dp;
	struct nfsnode *np;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];
	int ret = 0, fnd;

	np = VTONFS(vp);
	*lpp = NULL;
	*dorpcp = 0;

	/*
	 * Might need these, so MALLOC them now, to
	 * avoid a tsleep() in MALLOC later.
	 */
	MALLOC(nlop, struct nfscllock *,
	    sizeof (struct nfscllock), M_NFSCLLOCK, M_WAITOK);
	nlop->nfslo_type = F_UNLCK;
	nlop->nfslo_first = off;
	if (len == NFS64BITSSET) {
		nlop->nfslo_end = NFS64BITSSET;
	} else {
		nlop->nfslo_end = off + len;
		if (nlop->nfslo_end <= nlop->nfslo_first) {
			FREE((caddr_t)nlop, M_NFSCLLOCK);
			return (NFSERR_INVAL);
		}
	}
	if (callcnt == 0) {
		MALLOC(other_lop, struct nfscllock *,
		    sizeof (struct nfscllock), M_NFSCLLOCK, M_WAITOK);
		*other_lop = *nlop;
	}
	nfscl_filllockowner(p, own);
	dp = NULL;
	NFSLOCKCLSTATE();
	if (callcnt == 0)
		dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh,
		    np->n_fhp->nfh_len);

	/*
	 * First, unlock any local regions on a delegation.
	 */
	if (dp != NULL) {
		/* Look for this lockowner. */
		LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
			if (!NFSBCMP(lp->nfsl_owner, own,
			    NFSV4CL_LOCKNAMELEN))
				break;
		}
		if (lp != NULL)
			/* Use other_lop, so nlop is still available */
			(void)nfscl_updatelock(lp, &other_lop, NULL, 1);
	}

	/*
	 * Now, find a matching open/lockowner that hasn't already been done,
	 * as marked by nfsl_inprog.
	 */
	lp = NULL;
	fnd = 0;
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
	    LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
		if (op->nfso_fhlen == np->n_fhp->nfh_len &&
		    !NFSBCMP(op->nfso_fh, np->n_fhp->nfh_fh, op->nfso_fhlen)) {
		    LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
			if (lp->nfsl_inprog == NULL &&
			    !NFSBCMP(lp->nfsl_owner, own,
			     NFSV4CL_LOCKNAMELEN)) {
				fnd = 1;
				break;
			}
		    }
		    if (fnd)
			break;
		}
	    }
	    if (fnd)
		break;
	}

	if (lp != NULL) {
		ret = nfscl_updatelock(lp, &nlop, NULL, 0);
		if (ret)
			*dorpcp = 1;
		/*
		 * Serial modifications on the lock owner for multiple
		 * threads for the same process using a read/write lock.
		 */
		lp->nfsl_inprog = p;
		nfscl_lockexcl(&lp->nfsl_rwlock, NFSCLSTATEMUTEXPTR);
		*lpp = lp;
	}
	NFSUNLOCKCLSTATE();
	if (nlop)
		FREE((caddr_t)nlop, M_NFSCLLOCK);
	if (other_lop)
		FREE((caddr_t)other_lop, M_NFSCLLOCK);
	return (0);
}

/*
 * Release all lockowners marked in progess for this process and file.
 */
APPLESTATIC void
nfscl_releasealllocks(struct nfsclclient *clp, vnode_t vp, NFSPROC_T *p)
{
	struct nfsclowner *owp;
	struct nfsclopen *op;
	struct nfscllockowner *lp;
	struct nfsnode *np;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];

	np = VTONFS(vp);
	nfscl_filllockowner(p, own);
	NFSLOCKCLSTATE();
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
	    LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
		if (op->nfso_fhlen == np->n_fhp->nfh_len &&
		    !NFSBCMP(op->nfso_fh, np->n_fhp->nfh_fh, op->nfso_fhlen)) {
		    LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
			if (lp->nfsl_inprog == p &&
			    !NFSBCMP(lp->nfsl_owner, own,
			    NFSV4CL_LOCKNAMELEN)) {
			    lp->nfsl_inprog = NULL;
			    nfscl_lockunlock(&lp->nfsl_rwlock);
			}
		    }
		}
	    }
	}
	nfscl_clrelease(clp);
	NFSUNLOCKCLSTATE();
}

/*
 * Called to find out if any bytes within the byte range specified are
 * write locked by the calling process. Used to determine if flushing
 * is required before a LockU.
 * If in doubt, return 1, so the flush will occur.
 */
APPLESTATIC int
nfscl_checkwritelocked(vnode_t vp, struct flock *fl,
    struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclowner *owp;
	struct nfscllockowner *lp;
	struct nfsclopen *op;
	struct nfsclclient *clp;
	struct nfscllock *lop;
	struct nfscldeleg *dp;
	struct nfsnode *np;
	u_int64_t off, end;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];
	int error = 0;

	np = VTONFS(vp);
	switch (fl->l_whence) {
	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		off = fl->l_start;
		break;
	case SEEK_END:
		off = np->n_size + fl->l_start;
		break;
	default:
		return (1);
	};
	if (fl->l_len != 0) {
		end = off + fl->l_len;
		if (end < off)
			return (1);
	} else {
		end = NFS64BITSSET;
	}

	error = nfscl_getcl(vp, cred, p, &clp);
	if (error)
		return (1);
	nfscl_filllockowner(p, own);
	NFSLOCKCLSTATE();

	/*
	 * First check the delegation locks.
	 */
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL) {
		LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
			if (!NFSBCMP(lp->nfsl_owner, own,
			    NFSV4CL_LOCKNAMELEN))
				break;
		}
		if (lp != NULL) {
			LIST_FOREACH(lop, &lp->nfsl_lock, nfslo_list) {
				if (lop->nfslo_first >= end)
					break;
				if (lop->nfslo_end <= off)
					continue;
				if (lop->nfslo_type == F_WRLCK) {
					nfscl_clrelease(clp);
					NFSUNLOCKCLSTATE();
					return (1);
				}
			}
		}
	}

	/*
	 * Now, check state against the server.
	 */
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
	    LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
		if (op->nfso_fhlen == np->n_fhp->nfh_len &&
		    !NFSBCMP(op->nfso_fh, np->n_fhp->nfh_fh, op->nfso_fhlen)) {
		    LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
			if (!NFSBCMP(lp->nfsl_owner, own,
			    NFSV4CL_LOCKNAMELEN))
			    break;
		    }
		    if (lp != NULL) {
			LIST_FOREACH(lop, &lp->nfsl_lock, nfslo_list) {
			    if (lop->nfslo_first >= end)
				break;
			    if (lop->nfslo_end <= off)
				continue;
			    if (lop->nfslo_type == F_WRLCK) {
				nfscl_clrelease(clp);
				NFSUNLOCKCLSTATE();
				return (1);
			    }
			}
		    }
		}
	    }
	}
	nfscl_clrelease(clp);
	NFSUNLOCKCLSTATE();
	return (0);
}

/*
 * Release a byte range lock owner structure.
 */
APPLESTATIC void
nfscl_lockrelease(struct nfscllockowner *lp, int error, int candelete)
{
	struct nfsclclient *clp;

	if (lp == NULL)
		return;
	NFSLOCKCLSTATE();
	clp = lp->nfsl_open->nfso_own->nfsow_clp;
	if (error != 0 && candelete &&
	    (lp->nfsl_rwlock.nfslock_lock & NFSV4LOCK_WANTED) == 0)
		nfscl_freelockowner(lp, 0);
	else
		nfscl_lockunlock(&lp->nfsl_rwlock);
	nfscl_clrelease(clp);
	NFSUNLOCKCLSTATE();
}

/*
 * Free up an open structure and any associated byte range lock structures.
 */
APPLESTATIC void
nfscl_freeopen(struct nfsclopen *op, int local)
{

	LIST_REMOVE(op, nfso_list);
	nfscl_freealllocks(&op->nfso_lock, local);
	FREE((caddr_t)op, M_NFSCLOPEN);
	if (local)
		newnfsstats.cllocalopens--;
	else
		newnfsstats.clopens--;
}

/*
 * Free up all lock owners and associated locks.
 */
static void
nfscl_freealllocks(struct nfscllockownerhead *lhp, int local)
{
	struct nfscllockowner *lp, *nlp;

	LIST_FOREACH_SAFE(lp, lhp, nfsl_list, nlp) {
		if ((lp->nfsl_rwlock.nfslock_lock & NFSV4LOCK_WANTED))
			panic("nfscllckw");
		nfscl_freelockowner(lp, local);
	}
}

/*
 * Called for an Open when NFSERR_EXPIRED is received from the server.
 * If there are no byte range locks nor a Share Deny lost, try to do a
 * fresh Open. Otherwise, free the open.
 */
static int
nfscl_expireopen(struct nfsclclient *clp, struct nfsclopen *op,
    struct nfsmount *nmp, struct ucred *cred, NFSPROC_T *p)
{
	struct nfscllockowner *lp;
	struct nfscldeleg *dp;
	int mustdelete = 0, error;

	/*
	 * Look for any byte range lock(s).
	 */
	LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
		if (!LIST_EMPTY(&lp->nfsl_lock)) {
			mustdelete = 1;
			break;
		}
	}

	/*
	 * If no byte range lock(s) nor a Share deny, try to re-open.
	 */
	if (!mustdelete && (op->nfso_mode & NFSLCK_DENYBITS) == 0) {
		newnfs_copycred(&op->nfso_cred, cred);
		dp = NULL;
		error = nfsrpc_reopen(nmp, op->nfso_fh,
		    op->nfso_fhlen, op->nfso_mode, op, &dp, cred, p);
		if (error) {
			mustdelete = 1;
			if (dp != NULL) {
				FREE((caddr_t)dp, M_NFSCLDELEG);
				dp = NULL;
			}
		}
		if (dp != NULL)
			nfscl_deleg(nmp->nm_mountp, clp, op->nfso_fh,
			    op->nfso_fhlen, cred, p, &dp);
	}

	/*
	 * If a byte range lock or Share deny or couldn't re-open, free it.
	 */
	if (mustdelete)
		nfscl_freeopen(op, 0);
	return (mustdelete);
}

/*
 * Free up an open owner structure.
 */
static void
nfscl_freeopenowner(struct nfsclowner *owp, int local)
{

	LIST_REMOVE(owp, nfsow_list);
	FREE((caddr_t)owp, M_NFSCLOWNER);
	if (local)
		newnfsstats.cllocalopenowners--;
	else
		newnfsstats.clopenowners--;
}

/*
 * Free up a byte range lock owner structure.
 */
static void
nfscl_freelockowner(struct nfscllockowner *lp, int local)
{
	struct nfscllock *lop, *nlop;

	LIST_REMOVE(lp, nfsl_list);
	LIST_FOREACH_SAFE(lop, &lp->nfsl_lock, nfslo_list, nlop) {
		nfscl_freelock(lop, local);
	}
	FREE((caddr_t)lp, M_NFSCLLOCKOWNER);
	if (local)
		newnfsstats.cllocallockowners--;
	else
		newnfsstats.cllockowners--;
}

/*
 * Free up a byte range lock structure.
 */
APPLESTATIC void
nfscl_freelock(struct nfscllock *lop, int local)
{

	LIST_REMOVE(lop, nfslo_list);
	FREE((caddr_t)lop, M_NFSCLLOCK);
	if (local)
		newnfsstats.cllocallocks--;
	else
		newnfsstats.cllocks--;
}

/*
 * Clean out the state related to a delegation.
 */
static void
nfscl_cleandeleg(struct nfscldeleg *dp)
{
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op;

	LIST_FOREACH_SAFE(owp, &dp->nfsdl_owner, nfsow_list, nowp) {
		op = LIST_FIRST(&owp->nfsow_open);
		if (op != NULL) {
			if (LIST_NEXT(op, nfso_list) != NULL)
				panic("nfscleandel");
			nfscl_freeopen(op, 1);
		}
		nfscl_freeopenowner(owp, 1);
	}
	nfscl_freealllocks(&dp->nfsdl_lock, 1);
}

/*
 * Free a delegation.
 */
static void
nfscl_freedeleg(struct nfscldeleghead *hdp, struct nfscldeleg *dp)
{

	TAILQ_REMOVE(hdp, dp, nfsdl_list);
	LIST_REMOVE(dp, nfsdl_hash);
	FREE((caddr_t)dp, M_NFSCLDELEG);
	newnfsstats.cldelegates--;
	nfscl_delegcnt--;
}

/*
 * Free up all state related to this client structure.
 */
static void
nfscl_cleanclient(struct nfsclclient *clp)
{
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op, *nop;
	struct nfscllockowner *lp, *nlp;


	/* get rid of defunct lockowners */
	LIST_FOREACH_SAFE(lp, &clp->nfsc_defunctlockowner, nfsl_list, nlp) {
		nfscl_freelockowner(lp, 0);
	}

	/* Now, all the OpenOwners, etc. */
	LIST_FOREACH_SAFE(owp, &clp->nfsc_owner, nfsow_list, nowp) {
		LIST_FOREACH_SAFE(op, &owp->nfsow_open, nfso_list, nop) {
			nfscl_freeopen(op, 0);
		}
		nfscl_freeopenowner(owp, 0);
	}
}

/*
 * Called when an NFSERR_EXPIRED is received from the server.
 */
static void
nfscl_expireclient(struct nfsclclient *clp, struct nfsmount *nmp,
    struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclowner *owp, *nowp, *towp;
	struct nfsclopen *op, *nop, *top;
	struct nfscldeleg *dp, *ndp;
	int ret, printed = 0;

	/*
	 * First, merge locally issued Opens into the list for the server.
	 */
	dp = TAILQ_FIRST(&clp->nfsc_deleg);
	while (dp != NULL) {
	    ndp = TAILQ_NEXT(dp, nfsdl_list);
	    owp = LIST_FIRST(&dp->nfsdl_owner);
	    while (owp != NULL) {
		nowp = LIST_NEXT(owp, nfsow_list);
		op = LIST_FIRST(&owp->nfsow_open);
		if (op != NULL) {
		    if (LIST_NEXT(op, nfso_list) != NULL)
			panic("nfsclexp");
		    LIST_FOREACH(towp, &clp->nfsc_owner, nfsow_list) {
			if (!NFSBCMP(towp->nfsow_owner, owp->nfsow_owner,
			    NFSV4CL_LOCKNAMELEN))
			    break;
		    }
		    if (towp != NULL) {
			/* Merge opens in */
			LIST_FOREACH(top, &towp->nfsow_open, nfso_list) {
			    if (top->nfso_fhlen == op->nfso_fhlen &&
				!NFSBCMP(top->nfso_fh, op->nfso_fh,
				 op->nfso_fhlen)) {
				top->nfso_mode |= op->nfso_mode;
				top->nfso_opencnt += op->nfso_opencnt;
				break;
			    }
			}
			if (top == NULL) {
			    /* Just add the open to the owner list */
			    LIST_REMOVE(op, nfso_list);
			    op->nfso_own = towp;
			    LIST_INSERT_HEAD(&towp->nfsow_open, op, nfso_list);
			    newnfsstats.cllocalopens--;
			    newnfsstats.clopens++;
			}
		    } else {
			/* Just add the openowner to the client list */
			LIST_REMOVE(owp, nfsow_list);
			owp->nfsow_clp = clp;
			LIST_INSERT_HEAD(&clp->nfsc_owner, owp, nfsow_list);
			newnfsstats.cllocalopenowners--;
			newnfsstats.clopenowners++;
			newnfsstats.cllocalopens--;
			newnfsstats.clopens++;
		    }
		}
		owp = nowp;
	    }
	    if (!printed && !LIST_EMPTY(&dp->nfsdl_lock)) {
		printed = 1;
		printf("nfsv4 expired locks lost\n");
	    }
	    nfscl_cleandeleg(dp);
	    nfscl_freedeleg(&clp->nfsc_deleg, dp);
	    dp = ndp;
	}
	if (!TAILQ_EMPTY(&clp->nfsc_deleg))
	    panic("nfsclexp");

	/*
	 * Now, try and reopen against the server.
	 */
	LIST_FOREACH_SAFE(owp, &clp->nfsc_owner, nfsow_list, nowp) {
		owp->nfsow_seqid = 0;
		LIST_FOREACH_SAFE(op, &owp->nfsow_open, nfso_list, nop) {
			ret = nfscl_expireopen(clp, op, nmp, cred, p);
			if (ret && !printed) {
				printed = 1;
				printf("nfsv4 expired locks lost\n");
			}
		}
		if (LIST_EMPTY(&owp->nfsow_open))
			nfscl_freeopenowner(owp, 0);
	}
}

#ifndef	__FreeBSD__
/*
 * Called from exit() upon process termination.
 */
APPLESTATIC void
nfscl_cleanup(NFSPROC_T *p)
{
	struct nfsclclient *clp;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];

	if (!nfscl_inited)
		return;
	nfscl_filllockowner(p, own);

	NFSLOCKCLSTATE();
	/*
	 * Loop through all the clientids, looking for the OpenOwners.
	 */
	LIST_FOREACH(clp, &nfsclhead, nfsc_list)
		nfscl_cleanup_common(clp, own);
	NFSUNLOCKCLSTATE();
}
#endif	/* !__FreeBSD__ */

/*
 * Common code used by nfscl_cleanup() and nfscl_cleanupkext().
 * Must be called with CLSTATE lock held.
 */
static void
nfscl_cleanup_common(struct nfsclclient *clp, u_int8_t *own)
{
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op;
	struct nfscllockowner *lp, *nlp;
	struct nfscldeleg *dp;

	/* First, get rid of local locks on delegations. */
	TAILQ_FOREACH(dp, &clp->nfsc_deleg, nfsdl_list) {
		LIST_FOREACH_SAFE(lp, &dp->nfsdl_lock, nfsl_list, nlp) {
		    if (!NFSBCMP(lp->nfsl_owner, own, NFSV4CL_LOCKNAMELEN)) {
			if ((lp->nfsl_rwlock.nfslock_lock & NFSV4LOCK_WANTED))
			    panic("nfscllckw");
			nfscl_freelockowner(lp, 1);
		    }
		}
	}
	owp = LIST_FIRST(&clp->nfsc_owner);
	while (owp != NULL) {
		nowp = LIST_NEXT(owp, nfsow_list);
		if (!NFSBCMP(owp->nfsow_owner, own,
		    NFSV4CL_LOCKNAMELEN)) {
			/*
			 * If there are children that haven't closed the
			 * file descriptors yet, the opens will still be
			 * here. For that case, let the renew thread clear
			 * out the OpenOwner later.
			 */
			if (LIST_EMPTY(&owp->nfsow_open))
				nfscl_freeopenowner(owp, 0);
			else
				owp->nfsow_defunct = 1;
		} else {
			/* look for lockowners on other opens */
			LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
				LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
					if (!NFSBCMP(lp->nfsl_owner, own,
					    NFSV4CL_LOCKNAMELEN))
						lp->nfsl_defunct = 1;
				}
			}
		}
		owp = nowp;
	}

	/* and check the defunct list */
	LIST_FOREACH(lp, &clp->nfsc_defunctlockowner, nfsl_list) {
		if (!NFSBCMP(lp->nfsl_owner, own, NFSV4CL_LOCKNAMELEN))
		    lp->nfsl_defunct = 1;
	}
}

#if defined(APPLEKEXT) || defined(__FreeBSD__)
/*
 * Simulate the call nfscl_cleanup() by looking for open owners associated
 * with processes that no longer exist, since a call to nfscl_cleanup()
 * can't be patched into exit().
 */
static void
nfscl_cleanupkext(struct nfsclclient *clp)
{
	struct nfsclowner *owp, *nowp;
	struct nfscllockowner *lp;

	NFSPROCLISTLOCK();
	NFSLOCKCLSTATE();
	LIST_FOREACH_SAFE(owp, &clp->nfsc_owner, nfsow_list, nowp) {
		if (nfscl_procdoesntexist(owp->nfsow_owner))
			nfscl_cleanup_common(clp, owp->nfsow_owner);
	}

	/* and check the defunct list */
	LIST_FOREACH(lp, &clp->nfsc_defunctlockowner, nfsl_list) {
		if (nfscl_procdoesntexist(lp->nfsl_owner))
			lp->nfsl_defunct = 1;
	}
	NFSUNLOCKCLSTATE();
	NFSPROCLISTUNLOCK();
}
#endif	/* APPLEKEXT || __FreeBSD__ */

/*
 * Called from nfs umount to free up the clientid.
 */
APPLESTATIC void
nfscl_umount(struct nfsmount *nmp, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	struct ucred *cred;
	int igotlock;

	clp = nmp->nm_clp;
	if (clp != NULL) {
		if ((clp->nfsc_flags & NFSCLFLAGS_INITED) == 0)
			panic("nfscl umount");
	
		/*
		 * First, handshake with the nfscl renew thread, to terminate
		 * it.
		 */
		clp->nfsc_flags |= NFSCLFLAGS_UMOUNT;
		while (clp->nfsc_flags & NFSCLFLAGS_HASTHREAD)
			(void) tsleep((caddr_t)clp, PWAIT, "nfsclumnt", hz);
	
		NFSLOCKCLSTATE();
		do {
			igotlock = nfsv4_lock(&clp->nfsc_lock, 1, NULL,
			    NFSCLSTATEMUTEXPTR);
		} while (!igotlock);
		NFSUNLOCKCLSTATE();
	
		/*
		 * Free up all the state. It will expire on the server, but
		 * maybe we should do a SetClientId/SetClientIdConfirm so
		 * the server throws it away?
		 */
		LIST_REMOVE(clp, nfsc_list);
		nfscl_delegreturnall(clp, p);
		cred = newnfs_getcred();
		(void) nfsrpc_setclient(nmp, clp, cred, p);
		nfscl_cleanclient(clp);
		nmp->nm_clp = NULL;
		NFSFREECRED(cred);
		FREE((caddr_t)clp, M_NFSCLCLIENT);
	}

}

/*
 * This function is called when a server replies with NFSERR_STALECLIENTID
 * or NFSERR_STALESTATEID. It traverses the clientid lists, doing Opens
 * and Locks with reclaim. If these fail, it deletes the corresponding state.
 */
static void
nfscl_recover(struct nfsclclient *clp, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op, *nop;
	struct nfscllockowner *lp, *nlp;
	struct nfscllock *lop, *nlop;
	struct nfscldeleg *dp, *ndp, *tdp;
	struct nfsmount *nmp;
	struct ucred *tcred;
	struct nfsclopenhead extra_open;
	struct nfscldeleghead extra_deleg;
	struct nfsreq *rep;
	u_int64_t len;
	u_int32_t delegtype = NFSV4OPEN_DELEGATEWRITE, mode;
	int igotlock = 0, error, trycnt, firstlock, s;

	/*
	 * First, lock the client structure, so everyone else will
	 * block when trying to use state.
	 */
	NFSLOCKCLSTATE();
	clp->nfsc_flags |= NFSCLFLAGS_RECVRINPROG;
	do {
		igotlock = nfsv4_lock(&clp->nfsc_lock, 1, NULL,
		    NFSCLSTATEMUTEXPTR);
	} while (!igotlock);
	NFSUNLOCKCLSTATE();

	nmp = clp->nfsc_nmp;
	if (nmp == NULL)
		panic("nfscl recover");
	trycnt = 5;
	do {
		error = nfsrpc_setclient(nmp, clp, cred, p);
	} while ((error == NFSERR_STALECLIENTID ||
	     error == NFSERR_STALEDONTRECOVER) && --trycnt > 0);
	if (error) {
		nfscl_cleanclient(clp);
		NFSLOCKCLSTATE();
		clp->nfsc_flags &= ~(NFSCLFLAGS_HASCLIENTID |
		    NFSCLFLAGS_RECOVER | NFSCLFLAGS_RECVRINPROG);
		wakeup(&clp->nfsc_flags);
		nfsv4_unlock(&clp->nfsc_lock, 0);
		NFSUNLOCKCLSTATE();
		return;
	}
	clp->nfsc_flags |= NFSCLFLAGS_HASCLIENTID;
	clp->nfsc_flags &= ~NFSCLFLAGS_RECOVER;

	/*
	 * Mark requests already queued on the server, so that they don't
	 * initiate another recovery cycle. Any requests already in the
	 * queue that handle state information will have the old stale
	 * clientid/stateid and will get a NFSERR_STALESTATEID or
	 * NFSERR_STALECLIENTID reply from the server. This will be
	 * translated to NFSERR_STALEDONTRECOVER when R_DONTRECOVER is set.
	 */
	s = splsoftclock();
	NFSLOCKREQ();
	TAILQ_FOREACH(rep, &nfsd_reqq, r_chain) {
		if (rep->r_nmp == nmp)
			rep->r_flags |= R_DONTRECOVER;
	}
	NFSUNLOCKREQ();
	splx(s);

	/* get rid of defunct lockowners */
	LIST_FOREACH_SAFE(lp, &clp->nfsc_defunctlockowner, nfsl_list, nlp) {
		nfscl_freelockowner(lp, 0);
	}

	/*
	 * Now, mark all delegations "need reclaim".
	 */
	TAILQ_FOREACH(dp, &clp->nfsc_deleg, nfsdl_list)
		dp->nfsdl_flags |= NFSCLDL_NEEDRECLAIM;

	TAILQ_INIT(&extra_deleg);
	LIST_INIT(&extra_open);
	/*
	 * Now traverse the state lists, doing Open and Lock Reclaims.
	 */
	tcred = newnfs_getcred();
	owp = LIST_FIRST(&clp->nfsc_owner);
	while (owp != NULL) {
	    nowp = LIST_NEXT(owp, nfsow_list);
	    owp->nfsow_seqid = 0;
	    op = LIST_FIRST(&owp->nfsow_open);
	    while (op != NULL) {
		nop = LIST_NEXT(op, nfso_list);
		if (error != NFSERR_NOGRACE) {
		    /* Search for a delegation to reclaim with the open */
		    TAILQ_FOREACH(dp, &clp->nfsc_deleg, nfsdl_list) {
			if (!(dp->nfsdl_flags & NFSCLDL_NEEDRECLAIM))
			    continue;
			if ((dp->nfsdl_flags & NFSCLDL_WRITE)) {
			    mode = NFSV4OPEN_ACCESSWRITE;
			    delegtype = NFSV4OPEN_DELEGATEWRITE;
			} else {
			    mode = NFSV4OPEN_ACCESSREAD;
			    delegtype = NFSV4OPEN_DELEGATEREAD;
			}
			if ((op->nfso_mode & mode) == mode &&
			    op->nfso_fhlen == dp->nfsdl_fhlen &&
			    !NFSBCMP(op->nfso_fh, dp->nfsdl_fh, op->nfso_fhlen))
			    break;
		    }
		    ndp = dp;
		    if (dp == NULL)
			delegtype = NFSV4OPEN_DELEGATENONE;
		    newnfs_copycred(&op->nfso_cred, tcred);
		    error = nfscl_tryopen(nmp, NULL, op->nfso_fh,
			op->nfso_fhlen, op->nfso_fh, op->nfso_fhlen,
			op->nfso_mode, op, NULL, 0, &ndp, 1, delegtype,
			tcred, p);
		    if (!error) {
			/* Handle any replied delegation */
			if (ndp != NULL && ((ndp->nfsdl_flags & NFSCLDL_WRITE)
			    || NFSMNT_RDONLY(nmp->nm_mountp))) {
			    if ((ndp->nfsdl_flags & NFSCLDL_WRITE))
				mode = NFSV4OPEN_ACCESSWRITE;
			    else
				mode = NFSV4OPEN_ACCESSREAD;
			    TAILQ_FOREACH(dp, &clp->nfsc_deleg, nfsdl_list) {
				if (!(dp->nfsdl_flags & NFSCLDL_NEEDRECLAIM))
				    continue;
				if ((op->nfso_mode & mode) == mode &&
				    op->nfso_fhlen == dp->nfsdl_fhlen &&
				    !NFSBCMP(op->nfso_fh, dp->nfsdl_fh,
				    op->nfso_fhlen)) {
				    dp->nfsdl_stateid = ndp->nfsdl_stateid;
				    dp->nfsdl_sizelimit = ndp->nfsdl_sizelimit;
				    dp->nfsdl_ace = ndp->nfsdl_ace;
				    dp->nfsdl_change = ndp->nfsdl_change;
				    dp->nfsdl_flags &= ~NFSCLDL_NEEDRECLAIM;
				    if ((ndp->nfsdl_flags & NFSCLDL_RECALL))
					dp->nfsdl_flags |= NFSCLDL_RECALL;
				    FREE((caddr_t)ndp, M_NFSCLDELEG);
				    ndp = NULL;
				    break;
				}
			    }
			}
			if (ndp != NULL)
			    TAILQ_INSERT_HEAD(&extra_deleg, ndp, nfsdl_list);

			/* and reclaim all byte range locks */
			lp = LIST_FIRST(&op->nfso_lock);
			while (lp != NULL) {
			    nlp = LIST_NEXT(lp, nfsl_list);
			    lp->nfsl_seqid = 0;
			    firstlock = 1;
			    lop = LIST_FIRST(&lp->nfsl_lock);
			    while (lop != NULL) {
				nlop = LIST_NEXT(lop, nfslo_list);
				if (lop->nfslo_end == NFS64BITSSET)
				    len = NFS64BITSSET;
				else
				    len = lop->nfslo_end - lop->nfslo_first;
				if (error != NFSERR_NOGRACE)
				    error = nfscl_trylock(nmp, NULL,
					op->nfso_fh, op->nfso_fhlen, lp,
					firstlock, 1, lop->nfslo_first, len,
					lop->nfslo_type, tcred, p);
				if (error != 0)
				    nfscl_freelock(lop, 0);
				else
				    firstlock = 0;
				lop = nlop;
			    }
			    /* If no locks, but a lockowner, just delete it. */
			    if (LIST_EMPTY(&lp->nfsl_lock))
				nfscl_freelockowner(lp, 0);
			    lp = nlp;
			}
		    } else {
			nfscl_freeopen(op, 0);
		    }
		}
		op = nop;
	    }
	    owp = nowp;
	}

	/*
	 * Now, try and get any delegations not yet reclaimed by cobbling
	 * to-gether an appropriate open.
	 */
	nowp = NULL;
	dp = TAILQ_FIRST(&clp->nfsc_deleg);
	while (dp != NULL) {
	    ndp = TAILQ_NEXT(dp, nfsdl_list);
	    if ((dp->nfsdl_flags & NFSCLDL_NEEDRECLAIM)) {
		if (nowp == NULL) {
		    MALLOC(nowp, struct nfsclowner *,
			sizeof (struct nfsclowner), M_NFSCLOWNER, M_WAITOK);
		    /*
		     * Name must be as long an largest possible
		     * NFSV4CL_LOCKNAMELEN. 12 for now.
		     */
		    NFSBCOPY("RECLAIMDELEG", nowp->nfsow_owner,
			NFSV4CL_LOCKNAMELEN);
		    LIST_INIT(&nowp->nfsow_open);
		    nowp->nfsow_clp = clp;
		    nowp->nfsow_seqid = 0;
		    nowp->nfsow_defunct = 0;
		    nfscl_lockinit(&nowp->nfsow_rwlock);
		}
		nop = NULL;
		if (error != NFSERR_NOGRACE) {
		    MALLOC(nop, struct nfsclopen *, sizeof (struct nfsclopen) +
			dp->nfsdl_fhlen - 1, M_NFSCLOPEN, M_WAITOK);
		    nop->nfso_own = nowp;
		    if ((dp->nfsdl_flags & NFSCLDL_WRITE)) {
			nop->nfso_mode = NFSV4OPEN_ACCESSWRITE;
			delegtype = NFSV4OPEN_DELEGATEWRITE;
		    } else {
			nop->nfso_mode = NFSV4OPEN_ACCESSREAD;
			delegtype = NFSV4OPEN_DELEGATEREAD;
		    }
		    nop->nfso_opencnt = 0;
		    nop->nfso_posixlock = 1;
		    nop->nfso_fhlen = dp->nfsdl_fhlen;
		    NFSBCOPY(dp->nfsdl_fh, nop->nfso_fh, dp->nfsdl_fhlen);
		    LIST_INIT(&nop->nfso_lock);
		    nop->nfso_stateid.seqid = 0;
		    nop->nfso_stateid.other[0] = 0;
		    nop->nfso_stateid.other[1] = 0;
		    nop->nfso_stateid.other[2] = 0;
		    newnfs_copycred(&dp->nfsdl_cred, tcred);
		    newnfs_copyincred(tcred, &nop->nfso_cred);
		    tdp = NULL;
		    error = nfscl_tryopen(nmp, NULL, nop->nfso_fh,
			nop->nfso_fhlen, nop->nfso_fh, nop->nfso_fhlen,
			nop->nfso_mode, nop, NULL, 0, &tdp, 1,
			delegtype, tcred, p);
		    if (tdp != NULL) {
			if ((tdp->nfsdl_flags & NFSCLDL_WRITE))
			    mode = NFSV4OPEN_ACCESSWRITE;
			else
			    mode = NFSV4OPEN_ACCESSREAD;
			if ((nop->nfso_mode & mode) == mode &&
			    nop->nfso_fhlen == tdp->nfsdl_fhlen &&
			    !NFSBCMP(nop->nfso_fh, tdp->nfsdl_fh,
			    nop->nfso_fhlen)) {
			    dp->nfsdl_stateid = tdp->nfsdl_stateid;
			    dp->nfsdl_sizelimit = tdp->nfsdl_sizelimit;
			    dp->nfsdl_ace = tdp->nfsdl_ace;
			    dp->nfsdl_change = tdp->nfsdl_change;
			    dp->nfsdl_flags &= ~NFSCLDL_NEEDRECLAIM;
			    if ((tdp->nfsdl_flags & NFSCLDL_RECALL))
				dp->nfsdl_flags |= NFSCLDL_RECALL;
			    FREE((caddr_t)tdp, M_NFSCLDELEG);
			} else {
			    TAILQ_INSERT_HEAD(&extra_deleg, tdp, nfsdl_list);
			}
		    }
		}
		if (error) {
		    if (nop != NULL)
			FREE((caddr_t)nop, M_NFSCLOPEN);
		    /*
		     * Couldn't reclaim it, so throw the state
		     * away. Ouch!!
		     */
		    nfscl_cleandeleg(dp);
		    nfscl_freedeleg(&clp->nfsc_deleg, dp);
		} else {
		    LIST_INSERT_HEAD(&extra_open, nop, nfso_list);
		}
	    }
	    dp = ndp;
	}

	/*
	 * Now, get rid of extra Opens and Delegations.
	 */
	LIST_FOREACH_SAFE(op, &extra_open, nfso_list, nop) {
		do {
			newnfs_copycred(&op->nfso_cred, tcred);
			error = nfscl_tryclose(op, tcred, nmp, p);
			if (error == NFSERR_GRACE)
				(void) nfs_catnap(PZERO, error, "nfsexcls");
		} while (error == NFSERR_GRACE);
		LIST_REMOVE(op, nfso_list);
		FREE((caddr_t)op, M_NFSCLOPEN);
	}
	if (nowp != NULL)
		FREE((caddr_t)nowp, M_NFSCLOWNER);

	TAILQ_FOREACH_SAFE(dp, &extra_deleg, nfsdl_list, ndp) {
		do {
			newnfs_copycred(&dp->nfsdl_cred, tcred);
			error = nfscl_trydelegreturn(dp, tcred, nmp, p);
			if (error == NFSERR_GRACE)
				(void) nfs_catnap(PZERO, error, "nfsexdlg");
		} while (error == NFSERR_GRACE);
		TAILQ_REMOVE(&extra_deleg, dp, nfsdl_list);
		FREE((caddr_t)dp, M_NFSCLDELEG);
	}

	NFSLOCKCLSTATE();
	clp->nfsc_flags &= ~NFSCLFLAGS_RECVRINPROG;
	wakeup(&clp->nfsc_flags);
	nfsv4_unlock(&clp->nfsc_lock, 0);
	NFSUNLOCKCLSTATE();
	NFSFREECRED(tcred);
}

/*
 * This function is called when a server replies with NFSERR_EXPIRED.
 * It deletes all state for the client and does a fresh SetClientId/confirm.
 * XXX Someday it should post a signal to the process(es) that hold the
 * state, so they know that lock state has been lost.
 */
APPLESTATIC int
nfscl_hasexpired(struct nfsclclient *clp, u_int32_t clidrev, NFSPROC_T *p)
{
	struct nfscllockowner *lp, *nlp;
	struct nfsmount *nmp;
	struct ucred *cred;
	int igotlock = 0, error, trycnt;

	/*
	 * If the clientid has gone away or a new SetClientid has already
	 * been done, just return ok.
	 */
	if (clp == NULL || clidrev != clp->nfsc_clientidrev)
		return (0);

	/*
	 * First, lock the client structure, so everyone else will
	 * block when trying to use state. Also, use NFSCLFLAGS_EXPIREIT so
	 * that only one thread does the work.
	 */
	NFSLOCKCLSTATE();
	clp->nfsc_flags |= NFSCLFLAGS_EXPIREIT;
	do {
		igotlock = nfsv4_lock(&clp->nfsc_lock, 1, NULL,
		    NFSCLSTATEMUTEXPTR);
	} while (!igotlock && (clp->nfsc_flags & NFSCLFLAGS_EXPIREIT));
	if ((clp->nfsc_flags & NFSCLFLAGS_EXPIREIT) == 0) {
		if (igotlock)
			nfsv4_unlock(&clp->nfsc_lock, 0);
		NFSUNLOCKCLSTATE();
		return (0);
	}
	clp->nfsc_flags |= NFSCLFLAGS_RECVRINPROG;
	NFSUNLOCKCLSTATE();

	nmp = clp->nfsc_nmp;
	if (nmp == NULL)
		panic("nfscl expired");
	cred = newnfs_getcred();
	trycnt = 5;
	do {
		error = nfsrpc_setclient(nmp, clp, cred, p);
	} while ((error == NFSERR_STALECLIENTID ||
	     error == NFSERR_STALEDONTRECOVER) && --trycnt > 0);
	if (error) {
		/*
		 * Clear out any state.
		 */
		nfscl_cleanclient(clp);
		NFSLOCKCLSTATE();
		clp->nfsc_flags &= ~(NFSCLFLAGS_HASCLIENTID |
		    NFSCLFLAGS_RECOVER);
	} else {
		/* get rid of defunct lockowners */
		LIST_FOREACH_SAFE(lp, &clp->nfsc_defunctlockowner, nfsl_list,
		    nlp) {
			nfscl_freelockowner(lp, 0);
		}

		/*
		 * Expire the state for the client.
		 */
		nfscl_expireclient(clp, nmp, cred, p);
		NFSLOCKCLSTATE();
		clp->nfsc_flags |= NFSCLFLAGS_HASCLIENTID;
		clp->nfsc_flags &= ~NFSCLFLAGS_RECOVER;
	}
	clp->nfsc_flags &= ~(NFSCLFLAGS_EXPIREIT | NFSCLFLAGS_RECVRINPROG);
	wakeup(&clp->nfsc_flags);
	nfsv4_unlock(&clp->nfsc_lock, 0);
	NFSUNLOCKCLSTATE();
	NFSFREECRED(cred);
	return (error);
}

/*
 * This function inserts a lock in the list after insert_lop.
 */
static void
nfscl_insertlock(struct nfscllockowner *lp, struct nfscllock *new_lop,
    struct nfscllock *insert_lop, int local)
{

	if ((struct nfscllockowner *)insert_lop == lp)
		LIST_INSERT_HEAD(&lp->nfsl_lock, new_lop, nfslo_list);
	else
		LIST_INSERT_AFTER(insert_lop, new_lop, nfslo_list);
	if (local)
		newnfsstats.cllocallocks++;
	else
		newnfsstats.cllocks++;
}

/*
 * This function updates the locking for a lock owner and given file. It
 * maintains a list of lock ranges ordered on increasing file offset that
 * are NFSCLLOCK_READ or NFSCLLOCK_WRITE and non-overlapping (aka POSIX style).
 * It always adds new_lop to the list and sometimes uses the one pointed
 * at by other_lopp.
 * Returns 1 if the locks were modified, 0 otherwise.
 */
static int
nfscl_updatelock(struct nfscllockowner *lp, struct nfscllock **new_lopp,
    struct nfscllock **other_lopp, int local)
{
	struct nfscllock *new_lop = *new_lopp;
	struct nfscllock *lop, *tlop, *ilop;
	struct nfscllock *other_lop;
	int unlock = 0, modified = 0;
	u_int64_t tmp;

	/*
	 * Work down the list until the lock is merged.
	 */
	if (new_lop->nfslo_type == F_UNLCK)
		unlock = 1;
	ilop = (struct nfscllock *)lp;
	lop = LIST_FIRST(&lp->nfsl_lock);
	while (lop != NULL) {
	    /*
	     * Only check locks for this file that aren't before the start of
	     * new lock's range.
	     */
	    if (lop->nfslo_end >= new_lop->nfslo_first) {
		if (new_lop->nfslo_end < lop->nfslo_first) {
		    /*
		     * If the new lock ends before the start of the
		     * current lock's range, no merge, just insert
		     * the new lock.
		     */
		    break;
		}
		if (new_lop->nfslo_type == lop->nfslo_type ||
		    (new_lop->nfslo_first <= lop->nfslo_first &&
		     new_lop->nfslo_end >= lop->nfslo_end)) {
		    /*
		     * This lock can be absorbed by the new lock/unlock.
		     * This happens when it covers the entire range
		     * of the old lock or is contiguous
		     * with the old lock and is of the same type or an
		     * unlock.
		     */
		    if (new_lop->nfslo_type != lop->nfslo_type ||
			new_lop->nfslo_first != lop->nfslo_first ||
			new_lop->nfslo_end != lop->nfslo_end)
			modified = 1;
		    if (lop->nfslo_first < new_lop->nfslo_first)
			new_lop->nfslo_first = lop->nfslo_first;
		    if (lop->nfslo_end > new_lop->nfslo_end)
			new_lop->nfslo_end = lop->nfslo_end;
		    tlop = lop;
		    lop = LIST_NEXT(lop, nfslo_list);
		    nfscl_freelock(tlop, local);
		    continue;
		}

		/*
		 * All these cases are for contiguous locks that are not the
		 * same type, so they can't be merged.
		 */
		if (new_lop->nfslo_first <= lop->nfslo_first) {
		    /*
		     * This case is where the new lock overlaps with the
		     * first part of the old lock. Move the start of the
		     * old lock to just past the end of the new lock. The
		     * new lock will be inserted in front of the old, since
		     * ilop hasn't been updated. (We are done now.)
		     */
		    if (lop->nfslo_first != new_lop->nfslo_end) {
			lop->nfslo_first = new_lop->nfslo_end;
			modified = 1;
		    }
		    break;
		}
		if (new_lop->nfslo_end >= lop->nfslo_end) {
		    /*
		     * This case is where the new lock overlaps with the
		     * end of the old lock's range. Move the old lock's
		     * end to just before the new lock's first and insert
		     * the new lock after the old lock.
		     * Might not be done yet, since the new lock could
		     * overlap further locks with higher ranges.
		     */
		    if (lop->nfslo_end != new_lop->nfslo_first) {
			lop->nfslo_end = new_lop->nfslo_first;
			modified = 1;
		    }
		    ilop = lop;
		    lop = LIST_NEXT(lop, nfslo_list);
		    continue;
		}
		/*
		 * The final case is where the new lock's range is in the
		 * middle of the current lock's and splits the current lock
		 * up. Use *other_lopp to handle the second part of the
		 * split old lock range. (We are done now.)
		 * For unlock, we use new_lop as other_lop and tmp, since
		 * other_lop and new_lop are the same for this case.
		 * We noted the unlock case above, so we don't need
		 * new_lop->nfslo_type any longer.
		 */
		tmp = new_lop->nfslo_first;
		if (unlock) {
		    other_lop = new_lop;
		    *new_lopp = NULL;
		} else {
		    other_lop = *other_lopp;
		    *other_lopp = NULL;
		}
		other_lop->nfslo_first = new_lop->nfslo_end;
		other_lop->nfslo_end = lop->nfslo_end;
		other_lop->nfslo_type = lop->nfslo_type;
		lop->nfslo_end = tmp;
		nfscl_insertlock(lp, other_lop, lop, local);
		ilop = lop;
		modified = 1;
		break;
	    }
	    ilop = lop;
	    lop = LIST_NEXT(lop, nfslo_list);
	    if (lop == NULL)
		break;
	}

	/*
	 * Insert the new lock in the list at the appropriate place.
	 */
	if (!unlock) {
		nfscl_insertlock(lp, new_lop, ilop, local);
		*new_lopp = NULL;
		modified = 1;
	}
	return (modified);
}

/*
 * This function must be run as a kernel thread.
 * It does Renew Ops and recovery, when required.
 */
APPLESTATIC void
nfscl_renewthread(struct nfsclclient *clp, NFSPROC_T *p)
{
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op;
	struct nfscllockowner *lp, *nlp, *olp;
	struct nfscldeleghead dh;
	struct nfscllockownerhead lh;
	struct nfscldeleg *dp, *ndp;
	struct ucred *cred;
	u_int32_t clidrev;
	int error, cbpathdown, islept, igotlock, ret, clearok;
	uint32_t recover_done_time = 0;

	cred = newnfs_getcred();
	NFSLOCKCLSTATE();
	clp->nfsc_flags |= NFSCLFLAGS_HASTHREAD;
	NFSUNLOCKCLSTATE();
	for(;;) {
		newnfs_setroot(cred);
		cbpathdown = 0;
		if (clp->nfsc_flags & NFSCLFLAGS_RECOVER) {
			/*
			 * Only allow one recover within 1/2 of the lease
			 * duration (nfsc_renew).
			 */
			if (recover_done_time < NFSD_MONOSEC) {
				recover_done_time = NFSD_MONOSEC +
				    clp->nfsc_renew;
				nfscl_recover(clp, cred, p);
			} else {
				NFSLOCKCLSTATE();
				clp->nfsc_flags &= ~NFSCLFLAGS_RECOVER;
				NFSUNLOCKCLSTATE();
			}
		}
		if (clp->nfsc_expire <= NFSD_MONOSEC &&
		    (clp->nfsc_flags & NFSCLFLAGS_HASCLIENTID)) {
			clp->nfsc_expire = NFSD_MONOSEC + clp->nfsc_renew;
			clidrev = clp->nfsc_clientidrev;
			error = nfsrpc_renew(clp, cred, p);
			if (error == NFSERR_CBPATHDOWN)
			    cbpathdown = 1;
			else if (error == NFSERR_STALECLIENTID) {
			    NFSLOCKCLSTATE();
			    clp->nfsc_flags |= NFSCLFLAGS_RECOVER;
			    NFSUNLOCKCLSTATE();
			} else if (error == NFSERR_EXPIRED)
			    (void) nfscl_hasexpired(clp, clidrev, p);
		}

		LIST_INIT(&lh);
		TAILQ_INIT(&dh);
		NFSLOCKCLSTATE();
		if (cbpathdown)
			/* It's a Total Recall! */
			nfscl_totalrecall(clp);

		/*
		 * Now, handle defunct owners.
		 */
		owp = LIST_FIRST(&clp->nfsc_owner);
		while (owp != NULL) {
		    nowp = LIST_NEXT(owp, nfsow_list);
		    if (LIST_EMPTY(&owp->nfsow_open)) {
			if (owp->nfsow_defunct)
			    nfscl_freeopenowner(owp, 0);
		    } else {
			LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
			    lp = LIST_FIRST(&op->nfso_lock);
			    while (lp != NULL) {
				nlp = LIST_NEXT(lp, nfsl_list);
				if (lp->nfsl_defunct &&
				    LIST_EMPTY(&lp->nfsl_lock)) {
				    LIST_FOREACH(olp, &lh, nfsl_list) {
					if (!NFSBCMP(olp->nfsl_owner,
					    lp->nfsl_owner,NFSV4CL_LOCKNAMELEN))
					    break;
				    }
				    if (olp == NULL) {
					LIST_REMOVE(lp, nfsl_list);
					LIST_INSERT_HEAD(&lh, lp, nfsl_list);
				    } else {
					nfscl_freelockowner(lp, 0);
				    }
				}
				lp = nlp;
			    }
			}
		    }
		    owp = nowp;
		}

		/* also search the defunct list */
		lp = LIST_FIRST(&clp->nfsc_defunctlockowner);
		while (lp != NULL) {
		    nlp = LIST_NEXT(lp, nfsl_list);
		    if (lp->nfsl_defunct) {
			LIST_FOREACH(olp, &lh, nfsl_list) {
			    if (!NFSBCMP(olp->nfsl_owner, lp->nfsl_owner,
				NFSV4CL_LOCKNAMELEN))
				break;
			}
			if (olp == NULL) {
			    LIST_REMOVE(lp, nfsl_list);
			    LIST_INSERT_HEAD(&lh, lp, nfsl_list);
			} else {
			    nfscl_freelockowner(lp, 0);
			}
		    }
		    lp = nlp;
		}
		/* and release defunct lock owners */
		LIST_FOREACH_SAFE(lp, &lh, nfsl_list, nlp) {
		    nfscl_freelockowner(lp, 0);
		}

		/*
		 * Do the recall on any delegations. To avoid trouble, always
		 * come back up here after having slept.
		 */
		igotlock = 0;
tryagain:
		dp = TAILQ_FIRST(&clp->nfsc_deleg);
		while (dp != NULL) {
			ndp = TAILQ_NEXT(dp, nfsdl_list);
			if ((dp->nfsdl_flags & NFSCLDL_RECALL)) {
				/*
				 * Wait for outstanding I/O ops to be done.
				 */
				if (dp->nfsdl_rwlock.nfslock_usecnt > 0) {
				    if (igotlock) {
					nfsv4_unlock(&clp->nfsc_lock, 0);
					igotlock = 0;
				    }
				    dp->nfsdl_rwlock.nfslock_lock |=
					NFSV4LOCK_WANTED;
				    (void) nfsmsleep(&dp->nfsdl_rwlock,
					NFSCLSTATEMUTEXPTR, PZERO, "nfscld",
					NULL);
				    goto tryagain;
				}
				while (!igotlock) {
				    igotlock = nfsv4_lock(&clp->nfsc_lock, 1,
					&islept, NFSCLSTATEMUTEXPTR);
				    if (islept)
					goto tryagain;
				}
				NFSUNLOCKCLSTATE();
				newnfs_copycred(&dp->nfsdl_cred, cred);
				ret = nfscl_recalldeleg(clp, clp->nfsc_nmp, dp,
				    NULL, cred, p, 1);
				if (!ret) {
				    nfscl_cleandeleg(dp);
				    TAILQ_REMOVE(&clp->nfsc_deleg, dp,
					nfsdl_list);
				    LIST_REMOVE(dp, nfsdl_hash);
				    TAILQ_INSERT_HEAD(&dh, dp, nfsdl_list);
				    nfscl_delegcnt--;
				    newnfsstats.cldelegates--;
				}
				NFSLOCKCLSTATE();
			}
			dp = ndp;
		}

		/*
		 * Clear out old delegations, if we are above the high water
		 * mark. Only clear out ones with no state related to them.
		 * The tailq list is in LRU order.
		 */
		dp = TAILQ_LAST(&clp->nfsc_deleg, nfscldeleghead);
		while (nfscl_delegcnt > nfscl_deleghighwater && dp != NULL) {
		    ndp = TAILQ_PREV(dp, nfscldeleghead, nfsdl_list);
		    if (dp->nfsdl_rwlock.nfslock_usecnt == 0 &&
			dp->nfsdl_rwlock.nfslock_lock == 0 &&
			dp->nfsdl_timestamp < NFSD_MONOSEC &&
			!(dp->nfsdl_flags & (NFSCLDL_RECALL | NFSCLDL_ZAPPED |
			  NFSCLDL_NEEDRECLAIM))) {
			clearok = 1;
			LIST_FOREACH(owp, &dp->nfsdl_owner, nfsow_list) {
			    op = LIST_FIRST(&owp->nfsow_open);
			    if (op != NULL) {
				clearok = 0;
				break;
			    }
			}
			if (clearok) {
			    LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
				if (!LIST_EMPTY(&lp->nfsl_lock)) {
				    clearok = 0;
				    break;
				}
			    }
			}
			if (clearok) {
			    TAILQ_REMOVE(&clp->nfsc_deleg, dp, nfsdl_list);
			    LIST_REMOVE(dp, nfsdl_hash);
			    TAILQ_INSERT_HEAD(&dh, dp, nfsdl_list);
			    nfscl_delegcnt--;
			    newnfsstats.cldelegates--;
			}
		    }
		    dp = ndp;
		}
		if (igotlock)
			nfsv4_unlock(&clp->nfsc_lock, 0);
		NFSUNLOCKCLSTATE();

		/*
		 * Delegreturn any delegations cleaned out or recalled.
		 */
		TAILQ_FOREACH_SAFE(dp, &dh, nfsdl_list, ndp) {
			newnfs_copycred(&dp->nfsdl_cred, cred);
			(void) nfscl_trydelegreturn(dp, cred, clp->nfsc_nmp, p);
			TAILQ_REMOVE(&dh, dp, nfsdl_list);
			FREE((caddr_t)dp, M_NFSCLDELEG);
		}

#if defined(APPLEKEXT) || defined(__FreeBSD__)
		/*
		 * Simulate the calls to nfscl_cleanup() when a process
		 * exits, since the call can't be patched into exit().
		 */
		{
			struct timespec mytime;
			static time_t prevsec = 0;

			NFSGETNANOTIME(&mytime);
			if (prevsec != mytime.tv_sec) {
				prevsec = mytime.tv_sec;
				nfscl_cleanupkext(clp);
			}
		}
#endif	/* APPLEKEXT || __FreeBSD__ */

		if ((clp->nfsc_flags & NFSCLFLAGS_RECOVER) == 0)
		    (void) tsleep((caddr_t)clp, PWAIT, "nfscl", hz);
		if (clp->nfsc_flags & NFSCLFLAGS_UMOUNT) {
			NFSFREECRED(cred);
			clp->nfsc_flags &= ~NFSCLFLAGS_HASTHREAD;
			wakeup((caddr_t)clp);
			return;
		}
	}
}

/*
 * Initiate state recovery. Called when NFSERR_STALECLIENTID or
 * NFSERR_STALESTATEID is received.
 */
APPLESTATIC void
nfscl_initiate_recovery(struct nfsclclient *clp)
{

	if (clp == NULL)
		return;
	NFSLOCKCLSTATE();
	clp->nfsc_flags |= NFSCLFLAGS_RECOVER;
	NFSUNLOCKCLSTATE();
	wakeup((caddr_t)clp);
}

/*
 * Dump out the state stuff for debugging.
 */
APPLESTATIC void
nfscl_dumpstate(struct nfsmount *nmp, int openowner, int opens,
    int lockowner, int locks)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp;
	struct nfsclopen *op;
	struct nfscllockowner *lp;
	struct nfscllock *lop;
	struct nfscldeleg *dp;

	clp = nmp->nm_clp;
	if (clp == NULL) {
		printf("nfscl dumpstate NULL clp\n");
		return;
	}
	NFSLOCKCLSTATE();
	TAILQ_FOREACH(dp, &clp->nfsc_deleg, nfsdl_list) {
	  LIST_FOREACH(owp, &dp->nfsdl_owner, nfsow_list) {
	    if (openowner && !LIST_EMPTY(&owp->nfsow_open))
		printf("owner=0x%x 0x%x 0x%x 0x%x seqid=%d\n",
		    owp->nfsow_owner[0], owp->nfsow_owner[1],
		    owp->nfsow_owner[2], owp->nfsow_owner[3],
		    owp->nfsow_seqid);
	    LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
		if (opens)
		    printf("open st=0x%x 0x%x 0x%x cnt=%d fh12=0x%x\n",
			op->nfso_stateid.other[0], op->nfso_stateid.other[1],
			op->nfso_stateid.other[2], op->nfso_opencnt,
			op->nfso_fh[12]);
		LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
		    if (lockowner)
			printf("lckown=0x%x 0x%x 0x%x 0x%x seqid=%d st=0x%x 0x%x 0x%x\n",
			    lp->nfsl_owner[0], lp->nfsl_owner[1],
			    lp->nfsl_owner[2], lp->nfsl_owner[3],
			    lp->nfsl_seqid,
			    lp->nfsl_stateid.other[0], lp->nfsl_stateid.other[1],
			    lp->nfsl_stateid.other[2]);
		    LIST_FOREACH(lop, &lp->nfsl_lock, nfslo_list) {
			if (locks)
#ifdef __FreeBSD__
			    printf("lck typ=%d fst=%ju end=%ju\n",
				lop->nfslo_type, (intmax_t)lop->nfslo_first,
				(intmax_t)lop->nfslo_end);
#else
			    printf("lck typ=%d fst=%qd end=%qd\n",
				lop->nfslo_type, lop->nfslo_first,
				lop->nfslo_end);
#endif
		    }
		}
	    }
	  }
	}
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
	    if (openowner && !LIST_EMPTY(&owp->nfsow_open))
		printf("owner=0x%x 0x%x 0x%x 0x%x seqid=%d\n",
		    owp->nfsow_owner[0], owp->nfsow_owner[1],
		    owp->nfsow_owner[2], owp->nfsow_owner[3],
		    owp->nfsow_seqid);
	    LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
		if (opens)
		    printf("open st=0x%x 0x%x 0x%x cnt=%d fh12=0x%x\n",
			op->nfso_stateid.other[0], op->nfso_stateid.other[1],
			op->nfso_stateid.other[2], op->nfso_opencnt,
			op->nfso_fh[12]);
		LIST_FOREACH(lp, &op->nfso_lock, nfsl_list) {
		    if (lockowner)
			printf("lckown=0x%x 0x%x 0x%x 0x%x seqid=%d st=0x%x 0x%x 0x%x\n",
			    lp->nfsl_owner[0], lp->nfsl_owner[1],
			    lp->nfsl_owner[2], lp->nfsl_owner[3],
			    lp->nfsl_seqid,
			    lp->nfsl_stateid.other[0], lp->nfsl_stateid.other[1],
			    lp->nfsl_stateid.other[2]);
		    LIST_FOREACH(lop, &lp->nfsl_lock, nfslo_list) {
			if (locks)
#ifdef __FreeBSD__
			    printf("lck typ=%d fst=%ju end=%ju\n",
				lop->nfslo_type, (intmax_t)lop->nfslo_first,
				(intmax_t)lop->nfslo_end);
#else
			    printf("lck typ=%d fst=%qd end=%qd\n",
				lop->nfslo_type, lop->nfslo_first,
				lop->nfslo_end);
#endif
		    }
		}
	    }
	}
	NFSUNLOCKCLSTATE();
}

/*
 * Check for duplicate open owners and opens.
 * (Only used as a diagnostic aid.)
 */
APPLESTATIC void
nfscl_dupopen(vnode_t vp, int dupopens)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp, *owp2;
	struct nfsclopen *op, *op2;
	struct nfsfh *nfhp;

	clp = VFSTONFS(vnode_mount(vp))->nm_clp;
	if (clp == NULL) {
		printf("nfscl dupopen NULL clp\n");
		return;
	}
	nfhp = VTONFS(vp)->n_fhp;
	NFSLOCKCLSTATE();

	/*
	 * First, search for duplicate owners.
	 * These should never happen!
	 */
	LIST_FOREACH(owp2, &clp->nfsc_owner, nfsow_list) {
	    LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
		if (owp != owp2 &&
		    !NFSBCMP(owp->nfsow_owner, owp2->nfsow_owner,
		    NFSV4CL_LOCKNAMELEN)) {
			NFSUNLOCKCLSTATE();
			printf("DUP OWNER\n");
			nfscl_dumpstate(VFSTONFS(vnode_mount(vp)), 1, 1, 0, 0);
			return;
		}
	    }
	}

	/*
	 * Now, search for duplicate stateids.
	 * These shouldn't happen, either.
	 */
	LIST_FOREACH(owp2, &clp->nfsc_owner, nfsow_list) {
	    LIST_FOREACH(op2, &owp2->nfsow_open, nfso_list) {
		LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
		    LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
			if (op != op2 &&
			    (op->nfso_stateid.other[0] != 0 ||
			     op->nfso_stateid.other[1] != 0 ||
			     op->nfso_stateid.other[2] != 0) &&
			    op->nfso_stateid.other[0] == op2->nfso_stateid.other[0] &&
			    op->nfso_stateid.other[1] == op2->nfso_stateid.other[1] &&
			    op->nfso_stateid.other[2] == op2->nfso_stateid.other[2]) {
			    NFSUNLOCKCLSTATE();
			    printf("DUP STATEID\n");
			    nfscl_dumpstate(VFSTONFS(vnode_mount(vp)), 1, 1, 0,
				0);
			    return;
			}
		    }
		}
	    }
	}

	/*
	 * Now search for duplicate opens.
	 * Duplicate opens for the same owner
	 * should never occur. Other duplicates are
	 * possible and are checked for if "dupopens"
	 * is true.
	 */
	LIST_FOREACH(owp2, &clp->nfsc_owner, nfsow_list) {
	    LIST_FOREACH(op2, &owp2->nfsow_open, nfso_list) {
		if (nfhp->nfh_len == op2->nfso_fhlen &&
		    !NFSBCMP(nfhp->nfh_fh, op2->nfso_fh, nfhp->nfh_len)) {
		    LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
			LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
			    if (op != op2 && nfhp->nfh_len == op->nfso_fhlen &&
				!NFSBCMP(nfhp->nfh_fh, op->nfso_fh, nfhp->nfh_len) &&
				(!NFSBCMP(op->nfso_own->nfsow_owner,
				 op2->nfso_own->nfsow_owner, NFSV4CL_LOCKNAMELEN) ||
				 dupopens)) {
				if (!NFSBCMP(op->nfso_own->nfsow_owner,
				    op2->nfso_own->nfsow_owner, NFSV4CL_LOCKNAMELEN)) {
				    NFSUNLOCKCLSTATE();
				    printf("BADDUP OPEN\n");
				} else {
				    NFSUNLOCKCLSTATE();
				    printf("DUP OPEN\n");
				}
				nfscl_dumpstate(VFSTONFS(vnode_mount(vp)), 1, 1,
				    0, 0);
				return;
			    }
			}
		    }
		}
	    }
	}
	NFSUNLOCKCLSTATE();
}

/*
 * During close, find an open that needs to be dereferenced and
 * dereference it. If there are no more opens for this file,
 * log a message to that effect.
 * Opens aren't actually Close'd until VOP_INACTIVE() is performed
 * on the file's vnode.
 * This is the safe way, since it is difficult to identify
 * which open the close is for and I/O can be performed after the
 * close(2) system call when a file is mmap'd.
 * If it returns 0 for success, there will be a referenced
 * clp returned via clpp.
 */
APPLESTATIC int
nfscl_getclose(vnode_t vp, struct nfsclclient **clpp)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp;
	struct nfsclopen *op;
	struct nfscldeleg *dp;
	struct nfsfh *nfhp;
	int error, notdecr;

	error = nfscl_getcl(vp, NULL, NULL, &clp);
	if (error)
		return (error);
	*clpp = clp;

	nfhp = VTONFS(vp)->n_fhp;
	notdecr = 1;
	NFSLOCKCLSTATE();
	/*
	 * First, look for one under a delegation that was locally issued
	 * and just decrement the opencnt for it. Since all my Opens against
	 * the server are DENY_NONE, I don't see a problem with hanging
	 * onto them. (It is much easier to use one of the extant Opens
	 * that I already have on the server when a Delegation is recalled
	 * than to do fresh Opens.) Someday, I might need to rethink this, but.
	 */
	dp = nfscl_finddeleg(clp, nfhp->nfh_fh, nfhp->nfh_len);
	if (dp != NULL) {
		LIST_FOREACH(owp, &dp->nfsdl_owner, nfsow_list) {
			op = LIST_FIRST(&owp->nfsow_open);
			if (op != NULL) {
				/*
				 * Since a delegation is for a file, there
				 * should never be more than one open for
				 * each openowner.
				 */
				if (LIST_NEXT(op, nfso_list) != NULL)
					panic("nfscdeleg opens");
				if (notdecr && op->nfso_opencnt > 0) {
					notdecr = 0;
					op->nfso_opencnt--;
					break;
				}
			}
		}
	}

	/* Now process the opens against the server. */
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
		LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
			if (op->nfso_fhlen == nfhp->nfh_len &&
			    !NFSBCMP(op->nfso_fh, nfhp->nfh_fh,
			    nfhp->nfh_len)) {
				/* Found an open, decrement cnt if possible */
				if (notdecr && op->nfso_opencnt > 0) {
					notdecr = 0;
					op->nfso_opencnt--;
				}
				/*
				 * There are more opens, so just return.
				 */
				if (op->nfso_opencnt > 0) {
					NFSUNLOCKCLSTATE();
					return (0);
				}
			}
		}
	}
	NFSUNLOCKCLSTATE();
	if (notdecr)
		printf("nfscl: never fnd open\n");
	return (0);
}

APPLESTATIC int
nfscl_doclose(vnode_t vp, struct nfsclclient **clpp, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	struct nfsclowner *owp, *nowp;
	struct nfsclopen *op;
	struct nfscldeleg *dp;
	struct nfsfh *nfhp;
	int error;

	error = nfscl_getcl(vp, NULL, NULL, &clp);
	if (error)
		return (error);
	*clpp = clp;

	nfhp = VTONFS(vp)->n_fhp;
	NFSLOCKCLSTATE();
	/*
	 * First get rid of the local Open structures, which should be no
	 * longer in use.
	 */
	dp = nfscl_finddeleg(clp, nfhp->nfh_fh, nfhp->nfh_len);
	if (dp != NULL) {
		LIST_FOREACH_SAFE(owp, &dp->nfsdl_owner, nfsow_list, nowp) {
			op = LIST_FIRST(&owp->nfsow_open);
			if (op != NULL) {
				KASSERT((op->nfso_opencnt == 0),
				    ("nfscl: bad open cnt on deleg"));
				nfscl_freeopen(op, 1);
			}
			nfscl_freeopenowner(owp, 1);
		}
	}

	/* Now process the opens against the server. */
lookformore:
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
		op = LIST_FIRST(&owp->nfsow_open);
		while (op != NULL) {
			if (op->nfso_fhlen == nfhp->nfh_len &&
			    !NFSBCMP(op->nfso_fh, nfhp->nfh_fh,
			    nfhp->nfh_len)) {
				/* Found an open, close it. */
				KASSERT((op->nfso_opencnt == 0),
				    ("nfscl: bad open cnt on server"));
				NFSUNLOCKCLSTATE();
				nfsrpc_doclose(VFSTONFS(vnode_mount(vp)), op,
				    p);
				NFSLOCKCLSTATE();
				goto lookformore;
			}
			op = LIST_NEXT(op, nfso_list);
		}
	}
	NFSUNLOCKCLSTATE();
	return (0);
}

/*
 * Return all delegations on this client.
 * (Must be called with client sleep lock.)
 */
static void
nfscl_delegreturnall(struct nfsclclient *clp, NFSPROC_T *p)
{
	struct nfscldeleg *dp, *ndp;
	struct ucred *cred;

	cred = newnfs_getcred();
	TAILQ_FOREACH_SAFE(dp, &clp->nfsc_deleg, nfsdl_list, ndp) {
		nfscl_cleandeleg(dp);
		(void) nfscl_trydelegreturn(dp, cred, clp->nfsc_nmp, p);
		nfscl_freedeleg(&clp->nfsc_deleg, dp);
	}
	NFSFREECRED(cred);
}

/*
 * Do a callback RPC.
 */
APPLESTATIC void
nfscl_docb(struct nfsrv_descript *nd, NFSPROC_T *p)
{
	int i, op;
	u_int32_t *tl;
	struct nfsclclient *clp;
	struct nfscldeleg *dp = NULL;
	int numops, taglen = -1, error = 0, trunc, ret = 0;
	u_int32_t minorvers, retops = 0, *retopsp = NULL, *repp, cbident;
	u_char tag[NFSV4_SMALLSTR + 1], *tagstr;
	vnode_t vp = NULL;
	struct nfsnode *np;
	struct vattr va;
	struct nfsfh *nfhp;
	mount_t mp;
	nfsattrbit_t attrbits, rattrbits;
	nfsv4stateid_t stateid;

	nfsrvd_rephead(nd);
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	taglen = fxdr_unsigned(int, *tl);
	if (taglen < 0) {
		error = EBADRPC;
		goto nfsmout;
	}
	if (taglen <= NFSV4_SMALLSTR)
		tagstr = tag;
	else
		tagstr = malloc(taglen + 1, M_TEMP, M_WAITOK);
	error = nfsrv_mtostr(nd, tagstr, taglen);
	if (error) {
		if (taglen > NFSV4_SMALLSTR)
			free(tagstr, M_TEMP);
		taglen = -1;
		goto nfsmout;
	}
	(void) nfsm_strtom(nd, tag, taglen);
	if (taglen > NFSV4_SMALLSTR) {
		free(tagstr, M_TEMP);
	}
	NFSM_BUILD(retopsp, u_int32_t *, NFSX_UNSIGNED);
	NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	minorvers = fxdr_unsigned(u_int32_t, *tl++);
	if (minorvers != NFSV4_MINORVERSION)
		nd->nd_repstat = NFSERR_MINORVERMISMATCH;
	cbident = fxdr_unsigned(u_int32_t, *tl++);
	if (nd->nd_repstat)
		numops = 0;
	else
		numops = fxdr_unsigned(int, *tl);
	/*
	 * Loop around doing the sub ops.
	 */
	for (i = 0; i < numops; i++) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		NFSM_BUILD(repp, u_int32_t *, 2 * NFSX_UNSIGNED);
		*repp++ = *tl;
		op = fxdr_unsigned(int, *tl);
		if (op < NFSV4OP_CBGETATTR || op > NFSV4OP_CBRECALL) {
		    nd->nd_repstat = NFSERR_OPILLEGAL;
		    *repp = nfscl_errmap(nd);
		    retops++;
		    break;
		}
		nd->nd_procnum = op;
		newnfsstats.cbrpccnt[nd->nd_procnum]++;
		switch (op) {
		case NFSV4OP_CBGETATTR:
			clp = NULL;
			error = nfsm_getfh(nd, &nfhp);
			if (!error)
				error = nfsrv_getattrbits(nd, &attrbits,
				    NULL, NULL);
			if (!error) {
				mp = nfscl_getmnt(cbident);
				if (mp == NULL)
					error = NFSERR_SERVERFAULT;
			}
			if (!error) {
				dp = NULL;
				NFSLOCKCLSTATE();
				clp = nfscl_findcl(VFSTONFS(mp));
				if (clp != NULL)
					dp = nfscl_finddeleg(clp, nfhp->nfh_fh,
					    nfhp->nfh_len);
				NFSUNLOCKCLSTATE();
				if (dp == NULL)
					error = NFSERR_SERVERFAULT;
			}
			if (!error) {
				ret = nfscl_ngetreopen(mp, nfhp->nfh_fh,
				    nfhp->nfh_len, p, &np);
				if (!ret)
					vp = NFSTOV(np);
			}
			if (nfhp != NULL)
				FREE((caddr_t)nfhp, M_NFSFH);
			if (!error) {
				NFSZERO_ATTRBIT(&rattrbits);
				if (NFSISSET_ATTRBIT(&attrbits,
				    NFSATTRBIT_SIZE)) {
					if (!ret)
						va.va_size = np->n_size;
					else
						va.va_size = dp->nfsdl_size;
					NFSSETBIT_ATTRBIT(&rattrbits,
					    NFSATTRBIT_SIZE);
				}
				if (NFSISSET_ATTRBIT(&attrbits,
				    NFSATTRBIT_CHANGE)) {
					va.va_filerev = dp->nfsdl_change;
					if (ret || (np->n_flag & NDELEGMOD))
						va.va_filerev++;
					NFSSETBIT_ATTRBIT(&rattrbits,
					    NFSATTRBIT_CHANGE);
				}
				(void) nfsv4_fillattr(nd, NULL, NULL, &va,
				    NULL, 0, &rattrbits, NULL, NULL, 0, 0);
				if (!ret)
					vrele(vp);
			}
			break;
		case NFSV4OP_CBRECALL:
			clp = NULL;
			NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID +
			    NFSX_UNSIGNED);
			stateid.seqid = *tl++;
			NFSBCOPY((caddr_t)tl, (caddr_t)stateid.other,
			    NFSX_STATEIDOTHER);
			tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
			trunc = fxdr_unsigned(int, *tl);
			error = nfsm_getfh(nd, &nfhp);
			if (!error) {
				mp = nfscl_getmnt(cbident);
				if (mp == NULL)
					error = NFSERR_SERVERFAULT;
			}
			if (!error) {
				NFSLOCKCLSTATE();
				clp = nfscl_findcl(VFSTONFS(mp));
				if (clp != NULL) {
					dp = nfscl_finddeleg(clp, nfhp->nfh_fh,
					    nfhp->nfh_len);
					if (dp != NULL) {
						dp->nfsdl_flags |=
						    NFSCLDL_RECALL;
						wakeup((caddr_t)clp);
					}
				} else {
					error = NFSERR_SERVERFAULT;
				}
				NFSUNLOCKCLSTATE();
			}
			if (nfhp != NULL)
				FREE((caddr_t)nfhp, M_NFSFH);
			break;
		};
		if (error) {
			if (error == EBADRPC || error == NFSERR_BADXDR) {
				nd->nd_repstat = NFSERR_BADXDR;
			} else {
				nd->nd_repstat = error;
			}
			error = 0;
		}
		retops++;
		if (nd->nd_repstat) {
			*repp = nfscl_errmap(nd);
			break;
		} else
			*repp = 0;	/* NFS4_OK */
	}
nfsmout:
	if (error) {
		if (error == EBADRPC || error == NFSERR_BADXDR)
			nd->nd_repstat = NFSERR_BADXDR;
		else
			printf("nfsv4 comperr1=%d\n", error);
	}
	if (taglen == -1) {
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = 0;
		*tl = 0;
	} else {
		*retopsp = txdr_unsigned(retops);
	}
	*nd->nd_errp = nfscl_errmap(nd);
}

/*
 * Generate the next cbident value. Basically just increment a static value
 * and then check that it isn't already in the list, if it has wrapped around.
 */
static u_int32_t
nfscl_nextcbident(void)
{
	struct nfsclclient *clp;
	int matched;
	static u_int32_t nextcbident = 0;
	static int haswrapped = 0;

	nextcbident++;
	if (nextcbident == 0)
		haswrapped = 1;
	if (haswrapped) {
		/*
		 * Search the clientid list for one already using this cbident.
		 */
		do {
			matched = 0;
			NFSLOCKCLSTATE();
			LIST_FOREACH(clp, &nfsclhead, nfsc_list) {
				if (clp->nfsc_cbident == nextcbident) {
					matched = 1;
					break;
				}
			}
			NFSUNLOCKCLSTATE();
			if (matched == 1)
				nextcbident++;
		} while (matched);
	}
	return (nextcbident);
}

/*
 * Get the mount point related to a given cbident.
 */
static mount_t
nfscl_getmnt(u_int32_t cbident)
{
	struct nfsclclient *clp;
	struct nfsmount *nmp;

	NFSLOCKCLSTATE();
	LIST_FOREACH(clp, &nfsclhead, nfsc_list) {
		if (clp->nfsc_cbident == cbident)
			break;
	}
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return (NULL);
	}
	nmp = clp->nfsc_nmp;
	NFSUNLOCKCLSTATE();
	return (nmp->nm_mountp);
}

/*
 * Search for a lock conflict locally on the client. A conflict occurs if
 * - not same owner and overlapping byte range and at least one of them is
 *   a write lock or this is an unlock.
 */
static int
nfscl_localconflict(struct nfsclclient *clp, u_int8_t *fhp, int fhlen,
    struct nfscllock *nlop, u_int8_t *own, struct nfscldeleg *dp,
    struct nfscllock **lopp)
{
	struct nfsclowner *owp;
	struct nfsclopen *op;
	int ret;

	if (dp != NULL) {
		ret = nfscl_checkconflict(&dp->nfsdl_lock, nlop, own, lopp);
		if (ret)
			return (ret);
	}
	LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
		LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
			if (op->nfso_fhlen == fhlen &&
			    !NFSBCMP(op->nfso_fh, fhp, fhlen)) {
				ret = nfscl_checkconflict(&op->nfso_lock, nlop,
				    own, lopp);
				if (ret)
					return (ret);
			}
		}
	}
	return (0);
}

static int
nfscl_checkconflict(struct nfscllockownerhead *lhp, struct nfscllock *nlop,
    u_int8_t *own, struct nfscllock **lopp)
{
	struct nfscllockowner *lp;
	struct nfscllock *lop;

	LIST_FOREACH(lp, lhp, nfsl_list) {
		if (NFSBCMP(lp->nfsl_owner, own, NFSV4CL_LOCKNAMELEN)) {
			LIST_FOREACH(lop, &lp->nfsl_lock, nfslo_list) {
				if (lop->nfslo_first >= nlop->nfslo_end)
					break;
				if (lop->nfslo_end <= nlop->nfslo_first)
					continue;
				if (lop->nfslo_type == F_WRLCK ||
				    nlop->nfslo_type == F_WRLCK ||
				    nlop->nfslo_type == F_UNLCK) {
					if (lopp != NULL)
						*lopp = lop;
					return (NFSERR_DENIED);
				}
			}
		}
	}
	return (0);
}

/*
 * Check for a local conflicting lock.
 */
APPLESTATIC int
nfscl_lockt(vnode_t vp, struct nfsclclient *clp, u_int64_t off,
    u_int64_t len, struct flock *fl, NFSPROC_T *p)
{
	struct nfscllock *lop, nlck;
	struct nfscldeleg *dp;
	struct nfsnode *np;
	u_int8_t own[NFSV4CL_LOCKNAMELEN];
	int error;

	nlck.nfslo_type = fl->l_type;
	nlck.nfslo_first = off;
	if (len == NFS64BITSSET) {
		nlck.nfslo_end = NFS64BITSSET;
	} else {
		nlck.nfslo_end = off + len;
		if (nlck.nfslo_end <= nlck.nfslo_first)
			return (NFSERR_INVAL);
	}
	np = VTONFS(vp);
	nfscl_filllockowner(p, own);
	NFSLOCKCLSTATE();
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	error = nfscl_localconflict(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len,
	    &nlck, own, dp, &lop);
	if (error != 0) {
		fl->l_whence = SEEK_SET;
		fl->l_start = lop->nfslo_first;
		if (lop->nfslo_end == NFS64BITSSET)
			fl->l_len = 0;
		else
			fl->l_len = lop->nfslo_end - lop->nfslo_first;
		fl->l_pid = (pid_t)0;
		fl->l_type = lop->nfslo_type;
		error = -1;			/* no RPC required */
	} else if (dp != NULL && ((dp->nfsdl_flags & NFSCLDL_WRITE) ||
	    fl->l_type == F_RDLCK)) {
		/*
		 * The delegation ensures that there isn't a conflicting
		 * lock on the server, so return -1 to indicate an RPC
		 * isn't required.
		 */
		fl->l_type = F_UNLCK;
		error = -1;
	}
	NFSUNLOCKCLSTATE();
	return (error);
}

/*
 * Handle Recall of a delegation.
 * The clp must be exclusive locked when this is called.
 */
static int
nfscl_recalldeleg(struct nfsclclient *clp, struct nfsmount *nmp,
    struct nfscldeleg *dp, vnode_t vp, struct ucred *cred, NFSPROC_T *p,
    int called_from_renewthread)
{
	struct nfsclowner *owp, *lowp, *nowp;
	struct nfsclopen *op, *lop;
	struct nfscllockowner *lp;
	struct nfscllock *lckp;
	struct nfsnode *np;
	int error = 0, ret, gotvp = 0;

	if (vp == NULL) {
		/*
		 * First, get a vnode for the file. This is needed to do RPCs.
		 */
		ret = nfscl_ngetreopen(nmp->nm_mountp, dp->nfsdl_fh,
		    dp->nfsdl_fhlen, p, &np);
		if (ret) {
			/*
			 * File isn't open, so nothing to move over to the
			 * server.
			 */
			return (0);
		}
		vp = NFSTOV(np);
		gotvp = 1;
	} else {
		np = VTONFS(vp);
	}
	dp->nfsdl_flags &= ~NFSCLDL_MODTIMESET;
	NFSINVALATTRCACHE(np);

	/*
	 * Ok, if it's a write delegation, flush data to the server, so
	 * that close/open consistency is retained.
	 */
	ret = 0;
	NFSLOCKNODE(np);
	if ((dp->nfsdl_flags & NFSCLDL_WRITE) && (np->n_flag & NMODIFIED)) {
#ifdef APPLE
		OSBitOrAtomic((u_int32_t)NDELEGRECALL, (UInt32 *)&np->n_flag);
#else
		np->n_flag |= NDELEGRECALL;
#endif
		NFSUNLOCKNODE(np);
		ret = ncl_flush(vp, MNT_WAIT, cred, p, 1,
		    called_from_renewthread);
		NFSLOCKNODE(np);
#ifdef APPLE
		OSBitAndAtomic((int32_t)~(NMODIFIED | NDELEGRECALL), (UInt32 *)&np->n_flag);
#else
		np->n_flag &= ~(NMODIFIED | NDELEGRECALL);
#endif
	}
	NFSUNLOCKNODE(np);
	if (ret == EIO && called_from_renewthread != 0) {
		/*
		 * If the flush failed with EIO for the renew thread,
		 * return now, so that the dirty buffer will be flushed
		 * later.
		 */
		if (gotvp != 0)
			vrele(vp);
		return (ret);
	}

	/*
	 * Now, for each openowner with opens issued locally, move them
	 * over to state against the server.
	 */
	LIST_FOREACH(lowp, &dp->nfsdl_owner, nfsow_list) {
		lop = LIST_FIRST(&lowp->nfsow_open);
		if (lop != NULL) {
			if (LIST_NEXT(lop, nfso_list) != NULL)
				panic("nfsdlg mult opens");
			/*
			 * Look for the same openowner against the server.
			 */
			LIST_FOREACH(owp, &clp->nfsc_owner, nfsow_list) {
				if (!NFSBCMP(lowp->nfsow_owner,
				    owp->nfsow_owner, NFSV4CL_LOCKNAMELEN)) {
					newnfs_copycred(&dp->nfsdl_cred, cred);
					ret = nfscl_moveopen(vp, clp, nmp, lop,
					    owp, dp, cred, p);
					if (ret == NFSERR_STALECLIENTID ||
					    ret == NFSERR_STALEDONTRECOVER) {
						if (gotvp)
							vrele(vp);
						return (ret);
					}
					if (ret) {
						nfscl_freeopen(lop, 1);
						if (!error)
							error = ret;
					}
					break;
				}
			}

			/*
			 * If no openowner found, create one and get an open
			 * for it.
			 */
			if (owp == NULL) {
				MALLOC(nowp, struct nfsclowner *,
				    sizeof (struct nfsclowner), M_NFSCLOWNER,
				    M_WAITOK);
				nfscl_newopen(clp, NULL, &owp, &nowp, &op, 
				    NULL, lowp->nfsow_owner, dp->nfsdl_fh,
				    dp->nfsdl_fhlen, NULL);
				newnfs_copycred(&dp->nfsdl_cred, cred);
				ret = nfscl_moveopen(vp, clp, nmp, lop,
				    owp, dp, cred, p);
				if (ret) {
					nfscl_freeopenowner(owp, 0);
					if (ret == NFSERR_STALECLIENTID ||
					    ret == NFSERR_STALEDONTRECOVER) {
						if (gotvp)
							vrele(vp);
						return (ret);
					}
					if (ret) {
						nfscl_freeopen(lop, 1);
						if (!error)
							error = ret;
					}
				}
			}
		}
	}

	/*
	 * Now, get byte range locks for any locks done locally.
	 */
	LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
		LIST_FOREACH(lckp, &lp->nfsl_lock, nfslo_list) {
			newnfs_copycred(&dp->nfsdl_cred, cred);
			ret = nfscl_relock(vp, clp, nmp, lp, lckp, cred, p);
			if (ret == NFSERR_STALESTATEID ||
			    ret == NFSERR_STALEDONTRECOVER ||
			    ret == NFSERR_STALECLIENTID) {
				if (gotvp)
					vrele(vp);
				return (ret);
			}
			if (ret && !error)
				error = ret;
		}
	}
	if (gotvp)
		vrele(vp);
	return (error);
}

/*
 * Move a locally issued open over to an owner on the state list.
 * SIDE EFFECT: If it needs to sleep (do an rpc), it unlocks clstate and
 * returns with it unlocked.
 */
static int
nfscl_moveopen(vnode_t vp, struct nfsclclient *clp, struct nfsmount *nmp,
    struct nfsclopen *lop, struct nfsclowner *owp, struct nfscldeleg *dp,
    struct ucred *cred, NFSPROC_T *p)
{
	struct nfsclopen *op, *nop;
	struct nfscldeleg *ndp;
	struct nfsnode *np;
	int error = 0, newone;

	/*
	 * First, look for an appropriate open, If found, just increment the
	 * opencnt in it.
	 */
	LIST_FOREACH(op, &owp->nfsow_open, nfso_list) {
		if ((op->nfso_mode & lop->nfso_mode) == lop->nfso_mode &&
		    op->nfso_fhlen == lop->nfso_fhlen &&
		    !NFSBCMP(op->nfso_fh, lop->nfso_fh, op->nfso_fhlen)) {
			op->nfso_opencnt += lop->nfso_opencnt;
			nfscl_freeopen(lop, 1);
			return (0);
		}
	}

	/* No appropriate open, so we have to do one against the server. */
	np = VTONFS(vp);
	MALLOC(nop, struct nfsclopen *, sizeof (struct nfsclopen) +
	    lop->nfso_fhlen - 1, M_NFSCLOPEN, M_WAITOK);
	newone = 0;
	nfscl_newopen(clp, NULL, &owp, NULL, &op, &nop, owp->nfsow_owner,
	    lop->nfso_fh, lop->nfso_fhlen, &newone);
	ndp = dp;
	error = nfscl_tryopen(nmp, vp, np->n_v4->n4_data, np->n_v4->n4_fhlen,
	    lop->nfso_fh, lop->nfso_fhlen, lop->nfso_mode, op,
	    NFS4NODENAME(np->n_v4), np->n_v4->n4_namelen, &ndp, 0, 0, cred, p);
	if (error) {
		if (newone)
			nfscl_freeopen(op, 0);
	} else {
		if (newone)
			newnfs_copyincred(cred, &op->nfso_cred);
		op->nfso_mode |= lop->nfso_mode;
		op->nfso_opencnt += lop->nfso_opencnt;
		nfscl_freeopen(lop, 1);
	}
	if (nop != NULL)
		FREE((caddr_t)nop, M_NFSCLOPEN);
	if (ndp != NULL) {
		/*
		 * What should I do with the returned delegation, since the
		 * delegation is being recalled? For now, just printf and
		 * through it away.
		 */
		printf("Moveopen returned deleg\n");
		FREE((caddr_t)ndp, M_NFSCLDELEG);
	}
	return (error);
}

/*
 * Recall all delegations on this client.
 */
static void
nfscl_totalrecall(struct nfsclclient *clp)
{
	struct nfscldeleg *dp;

	TAILQ_FOREACH(dp, &clp->nfsc_deleg, nfsdl_list)
		dp->nfsdl_flags |= NFSCLDL_RECALL;
}

/*
 * Relock byte ranges. Called for delegation recall and state expiry.
 */
static int
nfscl_relock(vnode_t vp, struct nfsclclient *clp, struct nfsmount *nmp,
    struct nfscllockowner *lp, struct nfscllock *lop, struct ucred *cred,
    NFSPROC_T *p)
{
	struct nfscllockowner *nlp;
	struct nfsfh *nfhp;
	u_int64_t off, len;
	u_int32_t clidrev = 0;
	int error, newone, donelocally;

	off = lop->nfslo_first;
	len = lop->nfslo_end - lop->nfslo_first;
	error = nfscl_getbytelock(vp, off, len, lop->nfslo_type, cred, p,
	    clp, 1, lp->nfsl_owner, lp->nfsl_openowner, &nlp, &newone,
	    &donelocally);
	if (error || donelocally)
		return (error);
	if (nmp->nm_clp != NULL)
		clidrev = nmp->nm_clp->nfsc_clientidrev;
	else
		clidrev = 0;
	nfhp = VTONFS(vp)->n_fhp;
	error = nfscl_trylock(nmp, vp, nfhp->nfh_fh,
	    nfhp->nfh_len, nlp, newone, 0, off,
	    len, lop->nfslo_type, cred, p);
	if (error)
		nfscl_freelockowner(nlp, 0);
	return (error);
}

/*
 * Called to re-open a file. Basically get a vnode for the file handle
 * and then call nfsrpc_openrpc() to do the rest.
 */
static int
nfsrpc_reopen(struct nfsmount *nmp, u_int8_t *fhp, int fhlen,
    u_int32_t mode, struct nfsclopen *op, struct nfscldeleg **dpp,
    struct ucred *cred, NFSPROC_T *p)
{
	struct nfsnode *np;
	vnode_t vp;
	int error;

	error = nfscl_ngetreopen(nmp->nm_mountp, fhp, fhlen, p, &np);
	if (error)
		return (error);
	vp = NFSTOV(np);
	if (np->n_v4 != NULL) {
		error = nfscl_tryopen(nmp, vp, np->n_v4->n4_data,
		    np->n_v4->n4_fhlen, fhp, fhlen, mode, op,
		    NFS4NODENAME(np->n_v4), np->n_v4->n4_namelen, dpp, 0, 0,
		    cred, p);
	} else {
		error = EINVAL;
	}
	vrele(vp);
	return (error);
}

/*
 * Try an open against the server. Just call nfsrpc_openrpc(), retrying while
 * NFSERR_DELAY. Also, try system credentials, if the passed in credentials
 * fail.
 */
static int
nfscl_tryopen(struct nfsmount *nmp, vnode_t vp, u_int8_t *fhp, int fhlen,
    u_int8_t *newfhp, int newfhlen, u_int32_t mode, struct nfsclopen *op,
    u_int8_t *name, int namelen, struct nfscldeleg **ndpp,
    int reclaim, u_int32_t delegtype, struct ucred *cred, NFSPROC_T *p)
{
	int error;

	do {
		error = nfsrpc_openrpc(nmp, vp, fhp, fhlen, newfhp, newfhlen,
		    mode, op, name, namelen, ndpp, reclaim, delegtype, cred, p,
		    0, 0);
		if (error == NFSERR_DELAY)
			(void) nfs_catnap(PZERO, error, "nfstryop");
	} while (error == NFSERR_DELAY);
	if (error == EAUTH || error == EACCES) {
		/* Try again using system credentials */
		newnfs_setroot(cred);
		do {
		    error = nfsrpc_openrpc(nmp, vp, fhp, fhlen, newfhp,
			newfhlen, mode, op, name, namelen, ndpp, reclaim,
			delegtype, cred, p, 1, 0);
		    if (error == NFSERR_DELAY)
			(void) nfs_catnap(PZERO, error, "nfstryop");
		} while (error == NFSERR_DELAY);
	}
	return (error);
}

/*
 * Try a byte range lock. Just loop on nfsrpc_lock() while it returns
 * NFSERR_DELAY. Also, retry with system credentials, if the provided
 * cred don't work.
 */
static int
nfscl_trylock(struct nfsmount *nmp, vnode_t vp, u_int8_t *fhp,
    int fhlen, struct nfscllockowner *nlp, int newone, int reclaim,
    u_int64_t off, u_int64_t len, short type, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	do {
		error = nfsrpc_lock(nd, nmp, vp, fhp, fhlen, nlp, newone,
		    reclaim, off, len, type, cred, p, 0);
		if (!error && nd->nd_repstat == NFSERR_DELAY)
			(void) nfs_catnap(PZERO, (int)nd->nd_repstat,
			    "nfstrylck");
	} while (!error && nd->nd_repstat == NFSERR_DELAY);
	if (!error)
		error = nd->nd_repstat;
	if (error == EAUTH || error == EACCES) {
		/* Try again using root credentials */
		newnfs_setroot(cred);
		do {
			error = nfsrpc_lock(nd, nmp, vp, fhp, fhlen, nlp,
			    newone, reclaim, off, len, type, cred, p, 1);
			if (!error && nd->nd_repstat == NFSERR_DELAY)
				(void) nfs_catnap(PZERO, (int)nd->nd_repstat,
				    "nfstrylck");
		} while (!error && nd->nd_repstat == NFSERR_DELAY);
		if (!error)
			error = nd->nd_repstat;
	}
	return (error);
}

/*
 * Try a delegreturn against the server. Just call nfsrpc_delegreturn(),
 * retrying while NFSERR_DELAY. Also, try system credentials, if the passed in
 * credentials fail.
 */
static int
nfscl_trydelegreturn(struct nfscldeleg *dp, struct ucred *cred,
    struct nfsmount *nmp, NFSPROC_T *p)
{
	int error;

	do {
		error = nfsrpc_delegreturn(dp, cred, nmp, p, 0);
		if (error == NFSERR_DELAY)
			(void) nfs_catnap(PZERO, error, "nfstrydp");
	} while (error == NFSERR_DELAY);
	if (error == EAUTH || error == EACCES) {
		/* Try again using system credentials */
		newnfs_setroot(cred);
		do {
			error = nfsrpc_delegreturn(dp, cred, nmp, p, 1);
			if (error == NFSERR_DELAY)
				(void) nfs_catnap(PZERO, error, "nfstrydp");
		} while (error == NFSERR_DELAY);
	}
	return (error);
}

/*
 * Try a close against the server. Just call nfsrpc_closerpc(),
 * retrying while NFSERR_DELAY. Also, try system credentials, if the passed in
 * credentials fail.
 */
APPLESTATIC int
nfscl_tryclose(struct nfsclopen *op, struct ucred *cred,
    struct nfsmount *nmp, NFSPROC_T *p)
{
	struct nfsrv_descript nfsd, *nd = &nfsd;
	int error;

	do {
		error = nfsrpc_closerpc(nd, nmp, op, cred, p, 0);
		if (error == NFSERR_DELAY)
			(void) nfs_catnap(PZERO, error, "nfstrycl");
	} while (error == NFSERR_DELAY);
	if (error == EAUTH || error == EACCES) {
		/* Try again using system credentials */
		newnfs_setroot(cred);
		do {
			error = nfsrpc_closerpc(nd, nmp, op, cred, p, 1);
			if (error == NFSERR_DELAY)
				(void) nfs_catnap(PZERO, error, "nfstrycl");
		} while (error == NFSERR_DELAY);
	}
	return (error);
}

/*
 * Decide if a delegation on a file permits close without flushing writes
 * to the server. This might be a big performance win in some environments.
 * (Not useful until the client does caching on local stable storage.)
 */
APPLESTATIC int
nfscl_mustflush(vnode_t vp)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np;
	struct nfsmount *nmp;

	np = VTONFS(vp);
	nmp = VFSTONFS(vnode_mount(vp));
	if (!NFSHASNFSV4(nmp))
		return (1);
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return (1);
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL && (dp->nfsdl_flags & (NFSCLDL_WRITE | NFSCLDL_RECALL))
	     == NFSCLDL_WRITE &&
	    (dp->nfsdl_sizelimit >= np->n_size ||
	     !NFSHASSTRICT3530(nmp))) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	NFSUNLOCKCLSTATE();
	return (1);
}

/*
 * See if a (write) delegation exists for this file.
 */
APPLESTATIC int
nfscl_nodeleg(vnode_t vp, int writedeleg)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np;
	struct nfsmount *nmp;

	np = VTONFS(vp);
	nmp = VFSTONFS(vnode_mount(vp));
	if (!NFSHASNFSV4(nmp))
		return (1);
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return (1);
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL && (dp->nfsdl_flags & NFSCLDL_RECALL) == 0 &&
	    (writedeleg == 0 || (dp->nfsdl_flags & NFSCLDL_WRITE)
	     == NFSCLDL_WRITE)) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	NFSUNLOCKCLSTATE();
	return (1);
}

/*
 * Look for an associated delegation that should be DelegReturned.
 */
APPLESTATIC int
nfscl_removedeleg(vnode_t vp, NFSPROC_T *p, nfsv4stateid_t *stp)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsclowner *owp;
	struct nfscllockowner *lp;
	struct nfsmount *nmp;
	struct ucred *cred;
	struct nfsnode *np;
	int igotlock = 0, triedrecall = 0, needsrecall, retcnt = 0, islept;

	nmp = VFSTONFS(vnode_mount(vp));
	np = VTONFS(vp);
	NFSLOCKCLSTATE();
	/*
	 * Loop around waiting for:
	 * - outstanding I/O operations on delegations to complete
	 * - for a delegation on vp that has state, lock the client and
	 *   do a recall
	 * - return delegation with no state
	 */
	while (1) {
		clp = nfscl_findcl(nmp);
		if (clp == NULL) {
			NFSUNLOCKCLSTATE();
			return (retcnt);
		}
		dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh,
		    np->n_fhp->nfh_len);
		if (dp != NULL) {
		    /*
		     * Wait for outstanding I/O ops to be done.
		     */
		    if (dp->nfsdl_rwlock.nfslock_usecnt > 0) {
			if (igotlock) {
			    nfsv4_unlock(&clp->nfsc_lock, 0);
			    igotlock = 0;
			}
			dp->nfsdl_rwlock.nfslock_lock |= NFSV4LOCK_WANTED;
			(void) nfsmsleep(&dp->nfsdl_rwlock,
			    NFSCLSTATEMUTEXPTR, PZERO, "nfscld", NULL);
			continue;
		    }
		    needsrecall = 0;
		    LIST_FOREACH(owp, &dp->nfsdl_owner, nfsow_list) {
			if (!LIST_EMPTY(&owp->nfsow_open)) {
			    needsrecall = 1;
			    break;
			}
		    }
		    if (!needsrecall) {
			LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
			    if (!LIST_EMPTY(&lp->nfsl_lock)) {
				needsrecall = 1;
				break;
			    }
			}
		    }
		    if (needsrecall && !triedrecall) {
			islept = 0;
			while (!igotlock) {
			    igotlock = nfsv4_lock(&clp->nfsc_lock, 1,
				&islept, NFSCLSTATEMUTEXPTR);
			    if (islept)
				break;
			}
			if (islept)
			    continue;
			NFSUNLOCKCLSTATE();
			cred = newnfs_getcred();
			newnfs_copycred(&dp->nfsdl_cred, cred);
			(void) nfscl_recalldeleg(clp, nmp, dp, vp, cred, p, 0);
			NFSFREECRED(cred);
			triedrecall = 1;
			NFSLOCKCLSTATE();
			nfsv4_unlock(&clp->nfsc_lock, 0);
			igotlock = 0;
			continue;
		    }
		    *stp = dp->nfsdl_stateid;
		    retcnt = 1;
		    nfscl_cleandeleg(dp);
		    nfscl_freedeleg(&clp->nfsc_deleg, dp);
		}
		if (igotlock)
		    nfsv4_unlock(&clp->nfsc_lock, 0);
		NFSUNLOCKCLSTATE();
		return (retcnt);
	}
}

/*
 * Look for associated delegation(s) that should be DelegReturned.
 */
APPLESTATIC int
nfscl_renamedeleg(vnode_t fvp, nfsv4stateid_t *fstp, int *gotfdp, vnode_t tvp,
    nfsv4stateid_t *tstp, int *gottdp, NFSPROC_T *p)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsclowner *owp;
	struct nfscllockowner *lp;
	struct nfsmount *nmp;
	struct ucred *cred;
	struct nfsnode *np;
	int igotlock = 0, triedrecall = 0, needsrecall, retcnt = 0, islept;

	nmp = VFSTONFS(vnode_mount(fvp));
	*gotfdp = 0;
	*gottdp = 0;
	NFSLOCKCLSTATE();
	/*
	 * Loop around waiting for:
	 * - outstanding I/O operations on delegations to complete
	 * - for a delegation on fvp that has state, lock the client and
	 *   do a recall
	 * - return delegation(s) with no state.
	 */
	while (1) {
		clp = nfscl_findcl(nmp);
		if (clp == NULL) {
			NFSUNLOCKCLSTATE();
			return (retcnt);
		}
		np = VTONFS(fvp);
		dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh,
		    np->n_fhp->nfh_len);
		if (dp != NULL && *gotfdp == 0) {
		    /*
		     * Wait for outstanding I/O ops to be done.
		     */
		    if (dp->nfsdl_rwlock.nfslock_usecnt > 0) {
			if (igotlock) {
			    nfsv4_unlock(&clp->nfsc_lock, 0);
			    igotlock = 0;
			}
			dp->nfsdl_rwlock.nfslock_lock |= NFSV4LOCK_WANTED;
			(void) nfsmsleep(&dp->nfsdl_rwlock,
			    NFSCLSTATEMUTEXPTR, PZERO, "nfscld", NULL);
			continue;
		    }
		    needsrecall = 0;
		    LIST_FOREACH(owp, &dp->nfsdl_owner, nfsow_list) {
			if (!LIST_EMPTY(&owp->nfsow_open)) {
			    needsrecall = 1;
			    break;
			}
		    }
		    if (!needsrecall) {
			LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
			    if (!LIST_EMPTY(&lp->nfsl_lock)) {
				needsrecall = 1;
				break;
			    }
			}
		    }
		    if (needsrecall && !triedrecall) {
			islept = 0;
			while (!igotlock) {
			    igotlock = nfsv4_lock(&clp->nfsc_lock, 1,
				&islept, NFSCLSTATEMUTEXPTR);
			    if (islept)
				break;
			}
			if (islept)
			    continue;
			NFSUNLOCKCLSTATE();
			cred = newnfs_getcred();
			newnfs_copycred(&dp->nfsdl_cred, cred);
			(void) nfscl_recalldeleg(clp, nmp, dp, fvp, cred, p, 0);
			NFSFREECRED(cred);
			triedrecall = 1;
			NFSLOCKCLSTATE();
			nfsv4_unlock(&clp->nfsc_lock, 0);
			igotlock = 0;
			continue;
		    }
		    *fstp = dp->nfsdl_stateid;
		    retcnt++;
		    *gotfdp = 1;
		    nfscl_cleandeleg(dp);
		    nfscl_freedeleg(&clp->nfsc_deleg, dp);
		}
		if (igotlock) {
		    nfsv4_unlock(&clp->nfsc_lock, 0);
		    igotlock = 0;
		}
		if (tvp != NULL) {
		    np = VTONFS(tvp);
		    dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh,
			np->n_fhp->nfh_len);
		    if (dp != NULL && *gottdp == 0) {
			/*
			 * Wait for outstanding I/O ops to be done.
			 */
			if (dp->nfsdl_rwlock.nfslock_usecnt > 0) {
			    dp->nfsdl_rwlock.nfslock_lock |= NFSV4LOCK_WANTED;
			    (void) nfsmsleep(&dp->nfsdl_rwlock,
				NFSCLSTATEMUTEXPTR, PZERO, "nfscld", NULL);
			    continue;
			}
			LIST_FOREACH(owp, &dp->nfsdl_owner, nfsow_list) {
			    if (!LIST_EMPTY(&owp->nfsow_open)) {
				NFSUNLOCKCLSTATE();
				return (retcnt);
			    }
			}
			LIST_FOREACH(lp, &dp->nfsdl_lock, nfsl_list) {
			    if (!LIST_EMPTY(&lp->nfsl_lock)) {
				NFSUNLOCKCLSTATE();
				return (retcnt);
			    }
			}
			*tstp = dp->nfsdl_stateid;
			retcnt++;
			*gottdp = 1;
			nfscl_cleandeleg(dp);
			nfscl_freedeleg(&clp->nfsc_deleg, dp);
		    }
		}
		NFSUNLOCKCLSTATE();
		return (retcnt);
	}
}

/*
 * Get a reference on the clientid associated with the mount point.
 * Return 1 if success, 0 otherwise.
 */
APPLESTATIC int
nfscl_getref(struct nfsmount *nmp)
{
	struct nfsclclient *clp;

	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return (0);
	}
	nfsv4_getref(&clp->nfsc_lock, NULL, NFSCLSTATEMUTEXPTR);
	NFSUNLOCKCLSTATE();
	return (1);
}

/*
 * Release a reference on a clientid acquired with the above call.
 */
APPLESTATIC void
nfscl_relref(struct nfsmount *nmp)
{
	struct nfsclclient *clp;

	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	nfsv4_relref(&clp->nfsc_lock);
	NFSUNLOCKCLSTATE();
}

/*
 * Save the size attribute in the delegation, since the nfsnode
 * is going away.
 */
APPLESTATIC void
nfscl_reclaimnode(vnode_t vp)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;

	nmp = VFSTONFS(vnode_mount(vp));
	if (!NFSHASNFSV4(nmp))
		return;
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL && (dp->nfsdl_flags & NFSCLDL_WRITE))
		dp->nfsdl_size = np->n_size;
	NFSUNLOCKCLSTATE();
}

/*
 * Get the saved size attribute in the delegation, since it is a
 * newly allocated nfsnode.
 */
APPLESTATIC void
nfscl_newnode(vnode_t vp)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;

	nmp = VFSTONFS(vnode_mount(vp));
	if (!NFSHASNFSV4(nmp))
		return;
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL && (dp->nfsdl_flags & NFSCLDL_WRITE))
		np->n_size = dp->nfsdl_size;
	NFSUNLOCKCLSTATE();
}

/*
 * If there is a valid write delegation for this file, set the modtime
 * to the local clock time.
 */
APPLESTATIC void
nfscl_delegmodtime(vnode_t vp)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;

	nmp = VFSTONFS(vnode_mount(vp));
	if (!NFSHASNFSV4(nmp))
		return;
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL && (dp->nfsdl_flags & NFSCLDL_WRITE)) {
		NFSGETNANOTIME(&dp->nfsdl_modtime);
		dp->nfsdl_flags |= NFSCLDL_MODTIMESET;
	}
	NFSUNLOCKCLSTATE();
}

/*
 * If there is a valid write delegation for this file with a modtime set,
 * put that modtime in mtime.
 */
APPLESTATIC void
nfscl_deleggetmodtime(vnode_t vp, struct timespec *mtime)
{
	struct nfsclclient *clp;
	struct nfscldeleg *dp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp;

	nmp = VFSTONFS(vnode_mount(vp));
	if (!NFSHASNFSV4(nmp))
		return;
	NFSLOCKCLSTATE();
	clp = nfscl_findcl(nmp);
	if (clp == NULL) {
		NFSUNLOCKCLSTATE();
		return;
	}
	dp = nfscl_finddeleg(clp, np->n_fhp->nfh_fh, np->n_fhp->nfh_len);
	if (dp != NULL &&
	    (dp->nfsdl_flags & (NFSCLDL_WRITE | NFSCLDL_MODTIMESET)) ==
	    (NFSCLDL_WRITE | NFSCLDL_MODTIMESET))
		*mtime = dp->nfsdl_modtime;
	NFSUNLOCKCLSTATE();
}

static int
nfscl_errmap(struct nfsrv_descript *nd)
{
	short *defaulterrp, *errp;

	if (!nd->nd_repstat)
		return (0);
	if (nd->nd_procnum == NFSPROC_NOOP)
		return (txdr_unsigned(nd->nd_repstat & 0xffff));
	if (nd->nd_repstat == EBADRPC)
		return (txdr_unsigned(NFSERR_BADXDR));
	if (nd->nd_repstat == NFSERR_MINORVERMISMATCH ||
	    nd->nd_repstat == NFSERR_OPILLEGAL)
		return (txdr_unsigned(nd->nd_repstat));
	errp = defaulterrp = nfscl_cberrmap[nd->nd_procnum];
	while (*++errp)
		if (*errp == (short)nd->nd_repstat)
			return (txdr_unsigned(nd->nd_repstat));
	return (txdr_unsigned(*defaulterrp));
}

