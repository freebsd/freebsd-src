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

#ifndef APPLEKEXT
#include <fs/nfs/nfsport.h>

struct nfsrv_stablefirst nfsrv_stablefirst;
int nfsrv_issuedelegs = 0;
int nfsrv_dolocallocks = 0;
struct nfsv4lock nfsv4rootfs_lock;

extern int newnfs_numnfsd;
extern struct nfsstats newnfsstats;
extern int nfsrv_lease;
extern struct timeval nfsboottime;
extern u_int32_t newnfs_true, newnfs_false;
NFSV4ROOTLOCKMUTEX;
NFSSTATESPINLOCK;

/*
 * Hash lists for nfs V4.
 * (Some would put them in the .h file, but I don't like declaring storage
 *  in a .h)
 */
struct nfsclienthashhead nfsclienthash[NFSCLIENTHASHSIZE];
struct nfslockhashhead nfslockhash[NFSLOCKHASHSIZE];
#endif	/* !APPLEKEXT */

static u_int32_t nfsrv_openpluslock = 0, nfsrv_delegatecnt = 0;
static time_t nfsrvboottime;
static int nfsrv_writedelegifpos = 1;
static int nfsrv_returnoldstateid = 0, nfsrv_clients = 0;
static int nfsrv_clienthighwater = NFSRV_CLIENTHIGHWATER;
static int nfsrv_nogsscallback = 0;

/* local functions */
static void nfsrv_dumpaclient(struct nfsclient *clp,
    struct nfsd_dumpclients *dumpp);
static void nfsrv_freeopenowner(struct nfsstate *stp, int cansleep,
    NFSPROC_T *p);
static int nfsrv_freeopen(struct nfsstate *stp, vnode_t vp, int cansleep,
    NFSPROC_T *p);
static void nfsrv_freelockowner(struct nfsstate *stp, vnode_t vp, int cansleep,
    NFSPROC_T *p);
static void nfsrv_freeallnfslocks(struct nfsstate *stp, vnode_t vp,
    int cansleep, NFSPROC_T *p);
static void nfsrv_freenfslock(struct nfslock *lop);
static void nfsrv_freenfslockfile(struct nfslockfile *lfp);
static void nfsrv_freedeleg(struct nfsstate *);
static int nfsrv_getstate(struct nfsclient *clp, nfsv4stateid_t *stateidp, 
    u_int32_t flags, struct nfsstate **stpp);
static void nfsrv_getowner(struct nfsstatehead *hp, struct nfsstate *new_stp,
    struct nfsstate **stpp);
static int nfsrv_getlockfh(vnode_t vp, u_short flags,
    struct nfslockfile **new_lfpp, fhandle_t *nfhp, NFSPROC_T *p);
static int nfsrv_getlockfile(u_short flags, struct nfslockfile **new_lfpp,
    struct nfslockfile **lfpp, fhandle_t *nfhp, int lockit);
static void nfsrv_insertlock(struct nfslock *new_lop,
    struct nfslock *insert_lop, struct nfsstate *stp, struct nfslockfile *lfp);
static void nfsrv_updatelock(struct nfsstate *stp, struct nfslock **new_lopp,
    struct nfslock **other_lopp, struct nfslockfile *lfp);
static int nfsrv_getipnumber(u_char *cp);
static int nfsrv_checkrestart(nfsquad_t clientid, u_int32_t flags,
    nfsv4stateid_t *stateidp, int specialid);
static int nfsrv_checkgrace(u_int32_t flags);
static int nfsrv_docallback(struct nfsclient *clp, int procnum,
    nfsv4stateid_t *stateidp, int trunc, fhandle_t *fhp,
    struct nfsvattr *nap, nfsattrbit_t *attrbitp, NFSPROC_T *p);
static u_int32_t nfsrv_nextclientindex(void);
static u_int32_t nfsrv_nextstateindex(struct nfsclient *clp);
static void nfsrv_markstable(struct nfsclient *clp);
static int nfsrv_checkstable(struct nfsclient *clp);
static int nfsrv_clientconflict(struct nfsclient *clp, int *haslockp, struct 
    vnode *vp, NFSPROC_T *p);
static int nfsrv_delegconflict(struct nfsstate *stp, int *haslockp,
    NFSPROC_T *p, vnode_t vp);
static int nfsrv_cleandeleg(vnode_t vp, struct nfslockfile *lfp,
    struct nfsclient *clp, int *haslockp, NFSPROC_T *p);
static int nfsrv_notsamecredname(struct nfsrv_descript *nd,
    struct nfsclient *clp);
static time_t nfsrv_leaseexpiry(void);
static void nfsrv_delaydelegtimeout(struct nfsstate *stp);
static int nfsrv_checkseqid(struct nfsrv_descript *nd, u_int32_t seqid,
    struct nfsstate *stp, struct nfsrvcache *op);
static int nfsrv_nootherstate(struct nfsstate *stp);
static int nfsrv_locallock(vnode_t vp, struct nfslockfile *lfp, int flags,
    uint64_t first, uint64_t end, struct nfslockconflict *cfp, NFSPROC_T *p);
static void nfsrv_localunlock(vnode_t vp, struct nfslockfile *lfp,
    uint64_t init_first, uint64_t init_end, NFSPROC_T *p);
static int nfsrv_dolocal(vnode_t vp, struct nfslockfile *lfp, int flags,
    int oldflags, uint64_t first, uint64_t end, struct nfslockconflict *cfp,
    NFSPROC_T *p);
static void nfsrv_locallock_rollback(vnode_t vp, struct nfslockfile *lfp,
    NFSPROC_T *p);
static void nfsrv_locallock_commit(struct nfslockfile *lfp, int flags,
    uint64_t first, uint64_t end);
static void nfsrv_locklf(struct nfslockfile *lfp);
static void nfsrv_unlocklf(struct nfslockfile *lfp);

/*
 * Scan the client list for a match and either return the current one,
 * create a new entry or return an error.
 * If returning a non-error, the clp structure must either be linked into
 * the client list or free'd.
 */
APPLESTATIC int
nfsrv_setclient(struct nfsrv_descript *nd, struct nfsclient **new_clpp,
    nfsquad_t *clientidp, nfsquad_t *confirmp, NFSPROC_T *p)
{
	struct nfsclient *clp = NULL, *new_clp = *new_clpp;
	int i;
	struct nfsstate *stp, *tstp;
	struct sockaddr_in *sad, *rad;
	int zapit = 0, gotit, hasstate = 0, igotlock;
	static u_int64_t confirm_index = 0;

	/*
	 * Check for state resource limit exceeded.
	 */
	if (nfsrv_openpluslock > NFSRV_V4STATELIMIT)
		return (NFSERR_RESOURCE);

	if (nfsrv_issuedelegs == 0 ||
	    ((nd->nd_flag & ND_GSS) != 0 && nfsrv_nogsscallback != 0))
		/*
		 * Don't do callbacks when delegations are disabled or
		 * for AUTH_GSS unless enabled via nfsrv_nogsscallback.
		 * If establishing a callback connection is attempted
		 * when a firewall is blocking the callback path, the
		 * server may wait too long for the connect attempt to
		 * succeed during the Open. Some clients, such as Linux,
		 * may timeout and give up on the Open before the server
		 * replies. Also, since AUTH_GSS callbacks are not
		 * yet interoperability tested, they might cause the
		 * server to crap out, if they get past the Init call to
		 * the client.
		 */
		new_clp->lc_program = 0;

	/* Lock out other nfsd threads */
	NFSLOCKV4ROOTMUTEX();
	nfsv4_relref(&nfsv4rootfs_lock);
	do {
		igotlock = nfsv4_lock(&nfsv4rootfs_lock, 1, NULL,
		    NFSV4ROOTLOCKMUTEXPTR, NULL);
	} while (!igotlock);
	NFSUNLOCKV4ROOTMUTEX();

	/*
	 * Search for a match in the client list.
	 */
	gotit = i = 0;
	while (i < NFSCLIENTHASHSIZE && !gotit) {
	    LIST_FOREACH(clp, &nfsclienthash[i], lc_hash) {
		if (new_clp->lc_idlen == clp->lc_idlen &&
		    !NFSBCMP(new_clp->lc_id, clp->lc_id, clp->lc_idlen)) {
			gotit = 1;
			break;
		}
	    }
	    i++;
	}
	if (!gotit ||
	    (clp->lc_flags & (LCL_NEEDSCONFIRM | LCL_ADMINREVOKED))) {
		/*
		 * Get rid of the old one.
		 */
		if (i != NFSCLIENTHASHSIZE) {
			LIST_REMOVE(clp, lc_hash);
			nfsrv_cleanclient(clp, p);
			nfsrv_freedeleglist(&clp->lc_deleg);
			nfsrv_freedeleglist(&clp->lc_olddeleg);
			zapit = 1;
		}
		/*
		 * Add it after assigning a client id to it.
		 */
		new_clp->lc_flags |= LCL_NEEDSCONFIRM;
		confirmp->qval = new_clp->lc_confirm.qval = ++confirm_index;
		clientidp->lval[0] = new_clp->lc_clientid.lval[0] =
		    (u_int32_t)nfsrvboottime;
		clientidp->lval[1] = new_clp->lc_clientid.lval[1] =
		    nfsrv_nextclientindex();
		new_clp->lc_stateindex = 0;
		new_clp->lc_statemaxindex = 0;
		new_clp->lc_cbref = 0;
		new_clp->lc_expiry = nfsrv_leaseexpiry();
		LIST_INIT(&new_clp->lc_open);
		LIST_INIT(&new_clp->lc_deleg);
		LIST_INIT(&new_clp->lc_olddeleg);
		for (i = 0; i < NFSSTATEHASHSIZE; i++)
			LIST_INIT(&new_clp->lc_stateid[i]);
		LIST_INSERT_HEAD(NFSCLIENTHASH(new_clp->lc_clientid), new_clp,
		    lc_hash);
		newnfsstats.srvclients++;
		nfsrv_openpluslock++;
		nfsrv_clients++;
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
		if (zapit)
			nfsrv_zapclient(clp, p);
		*new_clpp = NULL;
		return (0);
	}

	/*
	 * Now, handle the cases where the id is already issued.
	 */
	if (nfsrv_notsamecredname(nd, clp)) {
	    /*
	     * Check to see if there is expired state that should go away.
	     */
	    if (clp->lc_expiry < NFSD_MONOSEC &&
	        (!LIST_EMPTY(&clp->lc_open) || !LIST_EMPTY(&clp->lc_deleg))) {
		nfsrv_cleanclient(clp, p);
		nfsrv_freedeleglist(&clp->lc_deleg);
	    }

	    /*
	     * If there is outstanding state, then reply NFSERR_CLIDINUSE per
	     * RFC3530 Sec. 8.1.2 last para.
	     */
	    if (!LIST_EMPTY(&clp->lc_deleg)) {
		hasstate = 1;
	    } else if (LIST_EMPTY(&clp->lc_open)) {
		hasstate = 0;
	    } else {
		hasstate = 0;
		/* Look for an Open on the OpenOwner */
		LIST_FOREACH(stp, &clp->lc_open, ls_list) {
		    if (!LIST_EMPTY(&stp->ls_open)) {
			hasstate = 1;
			break;
		    }
		}
	    }
	    if (hasstate) {
		/*
		 * If the uid doesn't match, return NFSERR_CLIDINUSE after
		 * filling out the correct ipaddr and portnum.
		 */
		sad = NFSSOCKADDR(new_clp->lc_req.nr_nam, struct sockaddr_in *);
		rad = NFSSOCKADDR(clp->lc_req.nr_nam, struct sockaddr_in *);
		sad->sin_addr.s_addr = rad->sin_addr.s_addr;
		sad->sin_port = rad->sin_port;
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
		return (NFSERR_CLIDINUSE);
	    }
	}

	if (NFSBCMP(new_clp->lc_verf, clp->lc_verf, NFSX_VERF)) {
		/*
		 * If the verifier has changed, the client has rebooted
		 * and a new client id is issued. The old state info
		 * can be thrown away once the SETCLIENTID_CONFIRM occurs.
		 */
		LIST_REMOVE(clp, lc_hash);
		new_clp->lc_flags |= LCL_NEEDSCONFIRM;
		confirmp->qval = new_clp->lc_confirm.qval = ++confirm_index;
		clientidp->lval[0] = new_clp->lc_clientid.lval[0] =
		    nfsrvboottime;
		clientidp->lval[1] = new_clp->lc_clientid.lval[1] =
		    nfsrv_nextclientindex();
		new_clp->lc_stateindex = 0;
		new_clp->lc_statemaxindex = 0;
		new_clp->lc_cbref = 0;
		new_clp->lc_expiry = nfsrv_leaseexpiry();

		/*
		 * Save the state until confirmed.
		 */
		LIST_NEWHEAD(&new_clp->lc_open, &clp->lc_open, ls_list);
		LIST_FOREACH(tstp, &new_clp->lc_open, ls_list)
			tstp->ls_clp = new_clp;
		LIST_NEWHEAD(&new_clp->lc_deleg, &clp->lc_deleg, ls_list);
		LIST_FOREACH(tstp, &new_clp->lc_deleg, ls_list)
			tstp->ls_clp = new_clp;
		LIST_NEWHEAD(&new_clp->lc_olddeleg, &clp->lc_olddeleg,
		    ls_list);
		LIST_FOREACH(tstp, &new_clp->lc_olddeleg, ls_list)
			tstp->ls_clp = new_clp;
		for (i = 0; i < NFSSTATEHASHSIZE; i++) {
			LIST_NEWHEAD(&new_clp->lc_stateid[i],
			    &clp->lc_stateid[i], ls_hash);
			LIST_FOREACH(tstp, &new_clp->lc_stateid[i], ls_list)
				tstp->ls_clp = new_clp;
		}
		LIST_INSERT_HEAD(NFSCLIENTHASH(new_clp->lc_clientid), new_clp,
		    lc_hash);
		newnfsstats.srvclients++;
		nfsrv_openpluslock++;
		nfsrv_clients++;
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();

		/*
		 * Must wait until any outstanding callback on the old clp
		 * completes.
		 */
		while (clp->lc_cbref) {
			clp->lc_flags |= LCL_WAKEUPWANTED;
			(void) tsleep((caddr_t)clp, PZERO - 1,
			    "nfsd clp", 10 * hz);
		}
		nfsrv_zapclient(clp, p);
		*new_clpp = NULL;
		return (0);
	}
	/*
	 * id and verifier match, so update the net address info
	 * and get rid of any existing callback authentication
	 * handle, so a new one will be acquired.
	 */
	LIST_REMOVE(clp, lc_hash);
	new_clp->lc_flags |= (LCL_NEEDSCONFIRM | LCL_DONTCLEAN);
	new_clp->lc_expiry = nfsrv_leaseexpiry();
	confirmp->qval = new_clp->lc_confirm.qval = ++confirm_index;
	clientidp->lval[0] = new_clp->lc_clientid.lval[0] =
	    clp->lc_clientid.lval[0];
	clientidp->lval[1] = new_clp->lc_clientid.lval[1] =
	    clp->lc_clientid.lval[1];
	new_clp->lc_delegtime = clp->lc_delegtime;
	new_clp->lc_stateindex = clp->lc_stateindex;
	new_clp->lc_statemaxindex = clp->lc_statemaxindex;
	new_clp->lc_cbref = 0;
	LIST_NEWHEAD(&new_clp->lc_open, &clp->lc_open, ls_list);
	LIST_FOREACH(tstp, &new_clp->lc_open, ls_list)
		tstp->ls_clp = new_clp;
	LIST_NEWHEAD(&new_clp->lc_deleg, &clp->lc_deleg, ls_list);
	LIST_FOREACH(tstp, &new_clp->lc_deleg, ls_list)
		tstp->ls_clp = new_clp;
	LIST_NEWHEAD(&new_clp->lc_olddeleg, &clp->lc_olddeleg, ls_list);
	LIST_FOREACH(tstp, &new_clp->lc_olddeleg, ls_list)
		tstp->ls_clp = new_clp;
	for (i = 0; i < NFSSTATEHASHSIZE; i++) {
		LIST_NEWHEAD(&new_clp->lc_stateid[i], &clp->lc_stateid[i],
		    ls_hash);
		LIST_FOREACH(tstp, &new_clp->lc_stateid[i], ls_list)
			tstp->ls_clp = new_clp;
	}
	LIST_INSERT_HEAD(NFSCLIENTHASH(new_clp->lc_clientid), new_clp,
	    lc_hash);
	newnfsstats.srvclients++;
	nfsrv_openpluslock++;
	nfsrv_clients++;
	NFSLOCKV4ROOTMUTEX();
	nfsv4_unlock(&nfsv4rootfs_lock, 1);
	NFSUNLOCKV4ROOTMUTEX();

	/*
	 * Must wait until any outstanding callback on the old clp
	 * completes.
	 */
	while (clp->lc_cbref) {
		clp->lc_flags |= LCL_WAKEUPWANTED;
		(void) tsleep((caddr_t)clp, PZERO - 1, "nfsd clp", 10 * hz);
	}
	nfsrv_zapclient(clp, p);
	*new_clpp = NULL;
	return (0);
}

/*
 * Check to see if the client id exists and optionally confirm it.
 */
APPLESTATIC int
nfsrv_getclient(nfsquad_t clientid, int opflags, struct nfsclient **clpp,
    nfsquad_t confirm, struct nfsrv_descript *nd, NFSPROC_T *p)
{
	struct nfsclient *clp;
	struct nfsstate *stp;
	int i;
	struct nfsclienthashhead *hp;
	int error = 0, igotlock, doneok;

	if (clpp)
		*clpp = NULL;
	if (nfsrvboottime != clientid.lval[0])
		return (NFSERR_STALECLIENTID);

	/*
	 * If called with opflags == CLOPS_RENEW, the State Lock is
	 * already held. Otherwise, we need to get either that or,
	 * for the case of Confirm, lock out the nfsd threads.
	 */
	if (opflags & CLOPS_CONFIRM) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_relref(&nfsv4rootfs_lock);
		do {
			igotlock = nfsv4_lock(&nfsv4rootfs_lock, 1, NULL,
			    NFSV4ROOTLOCKMUTEXPTR, NULL);
		} while (!igotlock);
		NFSUNLOCKV4ROOTMUTEX();
	} else if (opflags != CLOPS_RENEW) {
		NFSLOCKSTATE();
	}

	hp = NFSCLIENTHASH(clientid);
	LIST_FOREACH(clp, hp, lc_hash) {
		if (clp->lc_clientid.lval[1] == clientid.lval[1])
			break;
	}
	if (clp == LIST_END(hp)) {
		if (opflags & CLOPS_CONFIRM)
			error = NFSERR_STALECLIENTID;
		else
			error = NFSERR_EXPIRED;
	} else if (clp->lc_flags & LCL_ADMINREVOKED) {
		/*
		 * If marked admin revoked, just return the error.
		 */
		error = NFSERR_ADMINREVOKED;
	}
	if (error) {
		if (opflags & CLOPS_CONFIRM) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		} else if (opflags != CLOPS_RENEW) {
			NFSUNLOCKSTATE();
		}
		return (error);
	}

	/*
	 * Perform any operations specified by the opflags.
	 */
	if (opflags & CLOPS_CONFIRM) {
		if (clp->lc_confirm.qval != confirm.qval)
			error = NFSERR_STALECLIENTID;
		else if (nfsrv_notsamecredname(nd, clp))
			error = NFSERR_CLIDINUSE;

		if (!error) {
		    if ((clp->lc_flags & (LCL_NEEDSCONFIRM | LCL_DONTCLEAN)) ==
			LCL_NEEDSCONFIRM) {
			/*
			 * Hang onto the delegations (as old delegations)
			 * for an Open with CLAIM_DELEGATE_PREV unless in
			 * grace, but get rid of the rest of the state.
			 */
			nfsrv_cleanclient(clp, p);
			nfsrv_freedeleglist(&clp->lc_olddeleg);
			if (nfsrv_checkgrace(0)) {
			    /* In grace, so just delete delegations */
			    nfsrv_freedeleglist(&clp->lc_deleg);
			} else {
			    LIST_FOREACH(stp, &clp->lc_deleg, ls_list)
				stp->ls_flags |= NFSLCK_OLDDELEG;
			    clp->lc_delegtime = NFSD_MONOSEC +
				nfsrv_lease + NFSRV_LEASEDELTA;
			    LIST_NEWHEAD(&clp->lc_olddeleg, &clp->lc_deleg,
				ls_list);
			}
		    }
		    clp->lc_flags &= ~(LCL_NEEDSCONFIRM | LCL_DONTCLEAN);
		    if (clp->lc_program)
			clp->lc_flags |= LCL_NEEDSCBNULL;
		}
	} else if (clp->lc_flags & LCL_NEEDSCONFIRM) {
		error = NFSERR_EXPIRED;
	}

	/*
	 * If called by the Renew Op, we must check the principal.
	 */
	if (!error && (opflags & CLOPS_RENEWOP)) {
	    if (nfsrv_notsamecredname(nd, clp)) {
		doneok = 0;
		for (i = 0; i < NFSSTATEHASHSIZE && doneok == 0; i++) {
		    LIST_FOREACH(stp, &clp->lc_stateid[i], ls_hash) {
			if ((stp->ls_flags & NFSLCK_OPEN) &&
			    stp->ls_uid == nd->nd_cred->cr_uid) {
				doneok = 1;
				break;
			}
		    }
		}
		if (!doneok)
			error = NFSERR_ACCES;
	    }
	    if (!error && (clp->lc_flags & LCL_CBDOWN))
		error = NFSERR_CBPATHDOWN;
	}
	if ((!error || error == NFSERR_CBPATHDOWN) &&
	     (opflags & CLOPS_RENEW)) {
		clp->lc_expiry = nfsrv_leaseexpiry();
	}
	if (opflags & CLOPS_CONFIRM) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
	} else if (opflags != CLOPS_RENEW) {
		NFSUNLOCKSTATE();
	}
	if (clpp)
		*clpp = clp;
	return (error);
}

/*
 * Called from the new nfssvc syscall to admin revoke a clientid.
 * Returns 0 for success, error otherwise.
 */
APPLESTATIC int
nfsrv_adminrevoke(struct nfsd_clid *revokep, NFSPROC_T *p)
{
	struct nfsclient *clp = NULL;
	int i;
	int gotit, igotlock;

	/*
	 * First, lock out the nfsd so that state won't change while the
	 * revocation record is being written to the stable storage restart
	 * file.
	 */
	NFSLOCKV4ROOTMUTEX();
	do {
		igotlock = nfsv4_lock(&nfsv4rootfs_lock, 1, NULL,
		    NFSV4ROOTLOCKMUTEXPTR, NULL);
	} while (!igotlock);
	NFSUNLOCKV4ROOTMUTEX();

	/*
	 * Search for a match in the client list.
	 */
	gotit = i = 0;
	while (i < NFSCLIENTHASHSIZE && !gotit) {
	    LIST_FOREACH(clp, &nfsclienthash[i], lc_hash) {
		if (revokep->nclid_idlen == clp->lc_idlen &&
		    !NFSBCMP(revokep->nclid_id, clp->lc_id, clp->lc_idlen)) {
			gotit = 1;
			break;
		}
	    }
	    i++;
	}
	if (!gotit) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 0);
		NFSUNLOCKV4ROOTMUTEX();
		return (EPERM);
	}

	/*
	 * Now, write out the revocation record
	 */
	nfsrv_writestable(clp->lc_id, clp->lc_idlen, NFSNST_REVOKE, p);
	nfsrv_backupstable();

	/*
	 * and clear out the state, marking the clientid revoked.
	 */
	clp->lc_flags &= ~LCL_CALLBACKSON;
	clp->lc_flags |= LCL_ADMINREVOKED;
	nfsrv_cleanclient(clp, p);
	nfsrv_freedeleglist(&clp->lc_deleg);
	nfsrv_freedeleglist(&clp->lc_olddeleg);
	NFSLOCKV4ROOTMUTEX();
	nfsv4_unlock(&nfsv4rootfs_lock, 0);
	NFSUNLOCKV4ROOTMUTEX();
	return (0);
}

/*
 * Dump out stats for all clients. Called from nfssvc(2), that is used
 * newnfsstats.
 */
APPLESTATIC void
nfsrv_dumpclients(struct nfsd_dumpclients *dumpp, int maxcnt)
{
	struct nfsclient *clp;
	int i = 0, cnt = 0;

	/*
	 * First, get a reference on the nfsv4rootfs_lock so that an
	 * exclusive lock cannot be acquired while dumping the clients.
	 */
	NFSLOCKV4ROOTMUTEX();
	nfsv4_getref(&nfsv4rootfs_lock, NULL, NFSV4ROOTLOCKMUTEXPTR, NULL);
	NFSUNLOCKV4ROOTMUTEX();
	NFSLOCKSTATE();
	/*
	 * Rattle through the client lists until done.
	 */
	while (i < NFSCLIENTHASHSIZE && cnt < maxcnt) {
	    clp = LIST_FIRST(&nfsclienthash[i]);
	    while (clp != LIST_END(&nfsclienthash[i]) && cnt < maxcnt) {
		nfsrv_dumpaclient(clp, &dumpp[cnt]);
		cnt++;
		clp = LIST_NEXT(clp, lc_hash);
	    }
	    i++;
	}
	if (cnt < maxcnt)
	    dumpp[cnt].ndcl_clid.nclid_idlen = 0;
	NFSUNLOCKSTATE();
	NFSLOCKV4ROOTMUTEX();
	nfsv4_relref(&nfsv4rootfs_lock);
	NFSUNLOCKV4ROOTMUTEX();
}

/*
 * Dump stats for a client. Must be called with the NFSSTATELOCK and spl'd.
 */
static void
nfsrv_dumpaclient(struct nfsclient *clp, struct nfsd_dumpclients *dumpp)
{
	struct nfsstate *stp, *openstp, *lckownstp;
	struct nfslock *lop;
	struct sockaddr *sad;
	struct sockaddr_in *rad;
	struct sockaddr_in6 *rad6;

	dumpp->ndcl_nopenowners = dumpp->ndcl_nlockowners = 0;
	dumpp->ndcl_nopens = dumpp->ndcl_nlocks = 0;
	dumpp->ndcl_ndelegs = dumpp->ndcl_nolddelegs = 0;
	dumpp->ndcl_flags = clp->lc_flags;
	dumpp->ndcl_clid.nclid_idlen = clp->lc_idlen;
	NFSBCOPY(clp->lc_id, dumpp->ndcl_clid.nclid_id, clp->lc_idlen);
	sad = NFSSOCKADDR(clp->lc_req.nr_nam, struct sockaddr *);
	dumpp->ndcl_addrfam = sad->sa_family;
	if (sad->sa_family == AF_INET) {
		rad = (struct sockaddr_in *)sad;
		dumpp->ndcl_cbaddr.sin_addr = rad->sin_addr;
	} else {
		rad6 = (struct sockaddr_in6 *)sad;
		dumpp->ndcl_cbaddr.sin6_addr = rad6->sin6_addr;
	}

	/*
	 * Now, scan the state lists and total up the opens and locks.
	 */
	LIST_FOREACH(stp, &clp->lc_open, ls_list) {
	    dumpp->ndcl_nopenowners++;
	    LIST_FOREACH(openstp, &stp->ls_open, ls_list) {
		dumpp->ndcl_nopens++;
		LIST_FOREACH(lckownstp, &openstp->ls_open, ls_list) {
		    dumpp->ndcl_nlockowners++;
		    LIST_FOREACH(lop, &lckownstp->ls_lock, lo_lckowner) {
			dumpp->ndcl_nlocks++;
		    }
		}
	    }
	}

	/*
	 * and the delegation lists.
	 */
	LIST_FOREACH(stp, &clp->lc_deleg, ls_list) {
	    dumpp->ndcl_ndelegs++;
	}
	LIST_FOREACH(stp, &clp->lc_olddeleg, ls_list) {
	    dumpp->ndcl_nolddelegs++;
	}
}

/*
 * Dump out lock stats for a file.
 */
APPLESTATIC void
nfsrv_dumplocks(vnode_t vp, struct nfsd_dumplocks *ldumpp, int maxcnt,
    NFSPROC_T *p)
{
	struct nfsstate *stp;
	struct nfslock *lop;
	int cnt = 0;
	struct nfslockfile *lfp;
	struct sockaddr *sad;
	struct sockaddr_in *rad;
	struct sockaddr_in6 *rad6;
	int ret;
	fhandle_t nfh;

	ret = nfsrv_getlockfh(vp, 0, NULL, &nfh, p);
	/*
	 * First, get a reference on the nfsv4rootfs_lock so that an
	 * exclusive lock on it cannot be acquired while dumping the locks.
	 */
	NFSLOCKV4ROOTMUTEX();
	nfsv4_getref(&nfsv4rootfs_lock, NULL, NFSV4ROOTLOCKMUTEXPTR, NULL);
	NFSUNLOCKV4ROOTMUTEX();
	NFSLOCKSTATE();
	if (!ret)
		ret = nfsrv_getlockfile(0, NULL, &lfp, &nfh, 0);
	if (ret) {
		ldumpp[0].ndlck_clid.nclid_idlen = 0;
		NFSUNLOCKSTATE();
		NFSLOCKV4ROOTMUTEX();
		nfsv4_relref(&nfsv4rootfs_lock);
		NFSUNLOCKV4ROOTMUTEX();
		return;
	}

	/*
	 * For each open share on file, dump it out.
	 */
	stp = LIST_FIRST(&lfp->lf_open);
	while (stp != LIST_END(&lfp->lf_open) && cnt < maxcnt) {
		ldumpp[cnt].ndlck_flags = stp->ls_flags;
		ldumpp[cnt].ndlck_stateid.seqid = stp->ls_stateid.seqid;
		ldumpp[cnt].ndlck_stateid.other[0] = stp->ls_stateid.other[0];
		ldumpp[cnt].ndlck_stateid.other[1] = stp->ls_stateid.other[1];
		ldumpp[cnt].ndlck_stateid.other[2] = stp->ls_stateid.other[2];
		ldumpp[cnt].ndlck_owner.nclid_idlen =
		    stp->ls_openowner->ls_ownerlen;
		NFSBCOPY(stp->ls_openowner->ls_owner,
		    ldumpp[cnt].ndlck_owner.nclid_id,
		    stp->ls_openowner->ls_ownerlen);
		ldumpp[cnt].ndlck_clid.nclid_idlen = stp->ls_clp->lc_idlen;
		NFSBCOPY(stp->ls_clp->lc_id, ldumpp[cnt].ndlck_clid.nclid_id,
		    stp->ls_clp->lc_idlen);
		sad=NFSSOCKADDR(stp->ls_clp->lc_req.nr_nam, struct sockaddr *);
		ldumpp[cnt].ndlck_addrfam = sad->sa_family;
		if (sad->sa_family == AF_INET) {
			rad = (struct sockaddr_in *)sad;
			ldumpp[cnt].ndlck_cbaddr.sin_addr = rad->sin_addr;
		} else {
			rad6 = (struct sockaddr_in6 *)sad;
			ldumpp[cnt].ndlck_cbaddr.sin6_addr = rad6->sin6_addr;
		}
		stp = LIST_NEXT(stp, ls_file);
		cnt++;
	}

	/*
	 * and all locks.
	 */
	lop = LIST_FIRST(&lfp->lf_lock);
	while (lop != LIST_END(&lfp->lf_lock) && cnt < maxcnt) {
		stp = lop->lo_stp;
		ldumpp[cnt].ndlck_flags = lop->lo_flags;
		ldumpp[cnt].ndlck_first = lop->lo_first;
		ldumpp[cnt].ndlck_end = lop->lo_end;
		ldumpp[cnt].ndlck_stateid.seqid = stp->ls_stateid.seqid;
		ldumpp[cnt].ndlck_stateid.other[0] = stp->ls_stateid.other[0];
		ldumpp[cnt].ndlck_stateid.other[1] = stp->ls_stateid.other[1];
		ldumpp[cnt].ndlck_stateid.other[2] = stp->ls_stateid.other[2];
		ldumpp[cnt].ndlck_owner.nclid_idlen = stp->ls_ownerlen;
		NFSBCOPY(stp->ls_owner, ldumpp[cnt].ndlck_owner.nclid_id,
		    stp->ls_ownerlen);
		ldumpp[cnt].ndlck_clid.nclid_idlen = stp->ls_clp->lc_idlen;
		NFSBCOPY(stp->ls_clp->lc_id, ldumpp[cnt].ndlck_clid.nclid_id,
		    stp->ls_clp->lc_idlen);
		sad=NFSSOCKADDR(stp->ls_clp->lc_req.nr_nam, struct sockaddr *);
		ldumpp[cnt].ndlck_addrfam = sad->sa_family;
		if (sad->sa_family == AF_INET) {
			rad = (struct sockaddr_in *)sad;
			ldumpp[cnt].ndlck_cbaddr.sin_addr = rad->sin_addr;
		} else {
			rad6 = (struct sockaddr_in6 *)sad;
			ldumpp[cnt].ndlck_cbaddr.sin6_addr = rad6->sin6_addr;
		}
		lop = LIST_NEXT(lop, lo_lckfile);
		cnt++;
	}

	/*
	 * and the delegations.
	 */
	stp = LIST_FIRST(&lfp->lf_deleg);
	while (stp != LIST_END(&lfp->lf_deleg) && cnt < maxcnt) {
		ldumpp[cnt].ndlck_flags = stp->ls_flags;
		ldumpp[cnt].ndlck_stateid.seqid = stp->ls_stateid.seqid;
		ldumpp[cnt].ndlck_stateid.other[0] = stp->ls_stateid.other[0];
		ldumpp[cnt].ndlck_stateid.other[1] = stp->ls_stateid.other[1];
		ldumpp[cnt].ndlck_stateid.other[2] = stp->ls_stateid.other[2];
		ldumpp[cnt].ndlck_owner.nclid_idlen = 0;
		ldumpp[cnt].ndlck_clid.nclid_idlen = stp->ls_clp->lc_idlen;
		NFSBCOPY(stp->ls_clp->lc_id, ldumpp[cnt].ndlck_clid.nclid_id,
		    stp->ls_clp->lc_idlen);
		sad=NFSSOCKADDR(stp->ls_clp->lc_req.nr_nam, struct sockaddr *);
		ldumpp[cnt].ndlck_addrfam = sad->sa_family;
		if (sad->sa_family == AF_INET) {
			rad = (struct sockaddr_in *)sad;
			ldumpp[cnt].ndlck_cbaddr.sin_addr = rad->sin_addr;
		} else {
			rad6 = (struct sockaddr_in6 *)sad;
			ldumpp[cnt].ndlck_cbaddr.sin6_addr = rad6->sin6_addr;
		}
		stp = LIST_NEXT(stp, ls_file);
		cnt++;
	}

	/*
	 * If list isn't full, mark end of list by setting the client name
	 * to zero length.
	 */
	if (cnt < maxcnt)
		ldumpp[cnt].ndlck_clid.nclid_idlen = 0;
	NFSUNLOCKSTATE();
	NFSLOCKV4ROOTMUTEX();
	nfsv4_relref(&nfsv4rootfs_lock);
	NFSUNLOCKV4ROOTMUTEX();
}

/*
 * Server timer routine. It can scan any linked list, so long
 * as it holds the spin/mutex lock and there is no exclusive lock on
 * nfsv4rootfs_lock.
 * (For OpenBSD, a kthread is ok. For FreeBSD, I think it is ok
 *  to do this from a callout, since the spin locks work. For
 *  Darwin, I'm not sure what will work correctly yet.)
 * Should be called once per second.
 */
APPLESTATIC void
nfsrv_servertimer(void)
{
	struct nfsclient *clp, *nclp;
	struct nfsstate *stp, *nstp;
	int got_ref, i;

	/*
	 * Make sure nfsboottime is set. This is used by V3 as well
	 * as V4. Note that nfsboottime is not nfsrvboottime, which is
	 * only used by the V4 server for leases.
	 */
	if (nfsboottime.tv_sec == 0)
		NFSSETBOOTTIME(nfsboottime);

	/*
	 * If server hasn't started yet, just return.
	 */
	NFSLOCKSTATE();
	if (nfsrv_stablefirst.nsf_eograce == 0) {
		NFSUNLOCKSTATE();
		return;
	}
	if (!(nfsrv_stablefirst.nsf_flags & NFSNSF_UPDATEDONE)) {
		if (!(nfsrv_stablefirst.nsf_flags & NFSNSF_GRACEOVER) &&
		    NFSD_MONOSEC > nfsrv_stablefirst.nsf_eograce)
			nfsrv_stablefirst.nsf_flags |=
			    (NFSNSF_GRACEOVER | NFSNSF_NEEDLOCK);
		NFSUNLOCKSTATE();
		return;
	}

	/*
	 * Try and get a reference count on the nfsv4rootfs_lock so that
	 * no nfsd thread can acquire an exclusive lock on it before this
	 * call is done. If it is already exclusively locked, just return.
	 */
	NFSLOCKV4ROOTMUTEX();
	got_ref = nfsv4_getref_nonblock(&nfsv4rootfs_lock);
	NFSUNLOCKV4ROOTMUTEX();
	if (got_ref == 0) {
		NFSUNLOCKSTATE();
		return;
	}

	/*
	 * For each client...
	 */
	for (i = 0; i < NFSCLIENTHASHSIZE; i++) {
	    clp = LIST_FIRST(&nfsclienthash[i]);
	    while (clp != LIST_END(&nfsclienthash[i])) {
		nclp = LIST_NEXT(clp, lc_hash);
		if (!(clp->lc_flags & LCL_EXPIREIT)) {
		    if (((clp->lc_expiry + NFSRV_STALELEASE) < NFSD_MONOSEC
			 && ((LIST_EMPTY(&clp->lc_deleg)
			      && LIST_EMPTY(&clp->lc_open)) ||
			     nfsrv_clients > nfsrv_clienthighwater)) ||
			(clp->lc_expiry + NFSRV_MOULDYLEASE) < NFSD_MONOSEC ||
			(clp->lc_expiry < NFSD_MONOSEC &&
			 (nfsrv_openpluslock * 10 / 9) > NFSRV_V4STATELIMIT)) {
			/*
			 * Lease has expired several nfsrv_lease times ago:
			 * PLUS
			 *    - no state is associated with it
			 *    OR
			 *    - above high water mark for number of clients
			 *      (nfsrv_clienthighwater should be large enough
			 *       that this only occurs when clients fail to
			 *       use the same nfs_client_id4.id. Maybe somewhat
			 *       higher that the maximum number of clients that
			 *       will mount this server?)
			 * OR
			 * Lease has expired a very long time ago
			 * OR
			 * Lease has expired PLUS the number of opens + locks
			 * has exceeded 90% of capacity
			 *
			 * --> Mark for expiry. The actual expiry will be done
			 *     by an nfsd sometime soon.
			 */
			clp->lc_flags |= LCL_EXPIREIT;
			nfsrv_stablefirst.nsf_flags |=
			    (NFSNSF_NEEDLOCK | NFSNSF_EXPIREDCLIENT);
		    } else {
			/*
			 * If there are no opens, increment no open tick cnt
			 * If time exceeds NFSNOOPEN, mark it to be thrown away
			 * otherwise, if there is an open, reset no open time
			 * Hopefully, this will avoid excessive re-creation
			 * of open owners and subsequent open confirms.
			 */
			stp = LIST_FIRST(&clp->lc_open);
			while (stp != LIST_END(&clp->lc_open)) {
				nstp = LIST_NEXT(stp, ls_list);
				if (LIST_EMPTY(&stp->ls_open)) {
					stp->ls_noopens++;
					if (stp->ls_noopens > NFSNOOPEN ||
					    (nfsrv_openpluslock * 2) >
					    NFSRV_V4STATELIMIT)
						nfsrv_stablefirst.nsf_flags |=
							NFSNSF_NOOPENS;
				} else {
					stp->ls_noopens = 0;
				}
				stp = nstp;
			}
		    }
		}
		clp = nclp;
	    }
	}
	NFSUNLOCKSTATE();
	NFSLOCKV4ROOTMUTEX();
	nfsv4_relref(&nfsv4rootfs_lock);
	NFSUNLOCKV4ROOTMUTEX();
}

/*
 * The following set of functions free up the various data structures.
 */
/*
 * Clear out all open/lock state related to this nfsclient.
 * Caller must hold an exclusive lock on nfsv4rootfs_lock, so that
 * there are no other active nfsd threads.
 */
APPLESTATIC void
nfsrv_cleanclient(struct nfsclient *clp, NFSPROC_T *p)
{
	struct nfsstate *stp, *nstp;

	LIST_FOREACH_SAFE(stp, &clp->lc_open, ls_list, nstp)
		nfsrv_freeopenowner(stp, 1, p);
}

/*
 * Free a client that has been cleaned. It should also already have been
 * removed from the lists.
 * (Just to be safe w.r.t. newnfs_disconnect(), call this function when
 *  softclock interrupts are enabled.)
 */
APPLESTATIC void
nfsrv_zapclient(struct nfsclient *clp, NFSPROC_T *p)
{

#ifdef notyet
	if ((clp->lc_flags & (LCL_GSS | LCL_CALLBACKSON)) ==
	     (LCL_GSS | LCL_CALLBACKSON) &&
	    (clp->lc_hand.nfsh_flag & NFSG_COMPLETE) &&
	    clp->lc_handlelen > 0) {
		clp->lc_hand.nfsh_flag &= ~NFSG_COMPLETE;
		clp->lc_hand.nfsh_flag |= NFSG_DESTROYED;
		(void) nfsrv_docallback(clp, NFSV4PROC_CBNULL,
			NULL, 0, NULL, NULL, NULL, p);
	}
#endif
	newnfs_disconnect(&clp->lc_req);
	NFSSOCKADDRFREE(clp->lc_req.nr_nam);
	NFSFREEMUTEX(&clp->lc_req.nr_mtx);
	free((caddr_t)clp, M_NFSDCLIENT);
	NFSLOCKSTATE();
	newnfsstats.srvclients--;
	nfsrv_openpluslock--;
	nfsrv_clients--;
	NFSUNLOCKSTATE();
}

/*
 * Free a list of delegation state structures.
 * (This function will also free all nfslockfile structures that no
 *  longer have associated state.)
 */
APPLESTATIC void
nfsrv_freedeleglist(struct nfsstatehead *sthp)
{
	struct nfsstate *stp, *nstp;

	LIST_FOREACH_SAFE(stp, sthp, ls_list, nstp) {
		nfsrv_freedeleg(stp);
	}
	LIST_INIT(sthp);
}

/*
 * Free up a delegation.
 */
static void
nfsrv_freedeleg(struct nfsstate *stp)
{
	struct nfslockfile *lfp;

	LIST_REMOVE(stp, ls_hash);
	LIST_REMOVE(stp, ls_list);
	LIST_REMOVE(stp, ls_file);
	lfp = stp->ls_lfp;
	if (LIST_EMPTY(&lfp->lf_open) &&
	    LIST_EMPTY(&lfp->lf_lock) && LIST_EMPTY(&lfp->lf_deleg) &&
	    LIST_EMPTY(&lfp->lf_locallock) && LIST_EMPTY(&lfp->lf_rollback) &&
	    lfp->lf_usecount == 0 &&
	    nfsv4_testlock(&lfp->lf_locallock_lck) == 0)
		nfsrv_freenfslockfile(lfp);
	FREE((caddr_t)stp, M_NFSDSTATE);
	newnfsstats.srvdelegates--;
	nfsrv_openpluslock--;
	nfsrv_delegatecnt--;
}

/*
 * This function frees an open owner and all associated opens.
 */
static void
nfsrv_freeopenowner(struct nfsstate *stp, int cansleep, NFSPROC_T *p)
{
	struct nfsstate *nstp, *tstp;

	LIST_REMOVE(stp, ls_list);
	/*
	 * Now, free all associated opens.
	 */
	nstp = LIST_FIRST(&stp->ls_open);
	while (nstp != LIST_END(&stp->ls_open)) {
		tstp = nstp;
		nstp = LIST_NEXT(nstp, ls_list);
		(void) nfsrv_freeopen(tstp, NULL, cansleep, p);
	}
	if (stp->ls_op)
		nfsrvd_derefcache(stp->ls_op);
	FREE((caddr_t)stp, M_NFSDSTATE);
	newnfsstats.srvopenowners--;
	nfsrv_openpluslock--;
}

/*
 * This function frees an open (nfsstate open structure) with all associated
 * lock_owners and locks. It also frees the nfslockfile structure iff there
 * are no other opens on the file.
 * Returns 1 if it free'd the nfslockfile, 0 otherwise.
 */
static int
nfsrv_freeopen(struct nfsstate *stp, vnode_t vp, int cansleep, NFSPROC_T *p)
{
	struct nfsstate *nstp, *tstp;
	struct nfslockfile *lfp;
	int ret;

	LIST_REMOVE(stp, ls_hash);
	LIST_REMOVE(stp, ls_list);
	LIST_REMOVE(stp, ls_file);

	lfp = stp->ls_lfp;
	/*
	 * Now, free all lockowners associated with this open.
	 */
	LIST_FOREACH_SAFE(tstp, &stp->ls_open, ls_list, nstp)
		nfsrv_freelockowner(tstp, vp, cansleep, p);

	/*
	 * The nfslockfile is freed here if there are no locks
	 * associated with the open.
	 * If there are locks associated with the open, the
	 * nfslockfile structure can be freed via nfsrv_freelockowner().
	 * Acquire the state mutex to avoid races with calls to
	 * nfsrv_getlockfile().
	 */
	if (cansleep != 0)
		NFSLOCKSTATE();
	if (lfp != NULL && LIST_EMPTY(&lfp->lf_open) &&
	    LIST_EMPTY(&lfp->lf_deleg) && LIST_EMPTY(&lfp->lf_lock) &&
	    LIST_EMPTY(&lfp->lf_locallock) && LIST_EMPTY(&lfp->lf_rollback) &&
	    lfp->lf_usecount == 0 &&
	    (cansleep != 0 || nfsv4_testlock(&lfp->lf_locallock_lck) == 0)) {
		nfsrv_freenfslockfile(lfp);
		ret = 1;
	} else
		ret = 0;
	if (cansleep != 0)
		NFSUNLOCKSTATE();
	FREE((caddr_t)stp, M_NFSDSTATE);
	newnfsstats.srvopens--;
	nfsrv_openpluslock--;
	return (ret);
}

/*
 * Frees a lockowner and all associated locks.
 */
static void
nfsrv_freelockowner(struct nfsstate *stp, vnode_t vp, int cansleep,
    NFSPROC_T *p)
{

	LIST_REMOVE(stp, ls_hash);
	LIST_REMOVE(stp, ls_list);
	nfsrv_freeallnfslocks(stp, vp, cansleep, p);
	if (stp->ls_op)
		nfsrvd_derefcache(stp->ls_op);
	FREE((caddr_t)stp, M_NFSDSTATE);
	newnfsstats.srvlockowners--;
	nfsrv_openpluslock--;
}

/*
 * Free all the nfs locks on a lockowner.
 */
static void
nfsrv_freeallnfslocks(struct nfsstate *stp, vnode_t vp, int cansleep,
    NFSPROC_T *p)
{
	struct nfslock *lop, *nlop;
	struct nfsrollback *rlp, *nrlp;
	struct nfslockfile *lfp = NULL;
	int gottvp = 0;
	vnode_t tvp = NULL;
	uint64_t first, end;

	lop = LIST_FIRST(&stp->ls_lock);
	while (lop != LIST_END(&stp->ls_lock)) {
		nlop = LIST_NEXT(lop, lo_lckowner);
		/*
		 * Since all locks should be for the same file, lfp should
		 * not change.
		 */
		if (lfp == NULL)
			lfp = lop->lo_lfp;
		else if (lfp != lop->lo_lfp)
			panic("allnfslocks");
		/*
		 * If vp is NULL and cansleep != 0, a vnode must be acquired
		 * from the file handle. This only occurs when called from
		 * nfsrv_cleanclient().
		 */
		if (gottvp == 0) {
			if (nfsrv_dolocallocks == 0)
				tvp = NULL;
			else if (vp == NULL && cansleep != 0)
				tvp = nfsvno_getvp(&lfp->lf_fh);
			else
				tvp = vp;
			gottvp = 1;
		}

		if (tvp != NULL) {
			if (cansleep == 0)
				panic("allnfs2");
			first = lop->lo_first;
			end = lop->lo_end;
			nfsrv_freenfslock(lop);
			nfsrv_localunlock(tvp, lfp, first, end, p);
			LIST_FOREACH_SAFE(rlp, &lfp->lf_rollback, rlck_list,
			    nrlp)
				free(rlp, M_NFSDROLLBACK);
			LIST_INIT(&lfp->lf_rollback);
		} else
			nfsrv_freenfslock(lop);
		lop = nlop;
	}
	if (vp == NULL && tvp != NULL)
		vput(tvp);
}

/*
 * Free an nfslock structure.
 */
static void
nfsrv_freenfslock(struct nfslock *lop)
{

	if (lop->lo_lckfile.le_prev != NULL) {
		LIST_REMOVE(lop, lo_lckfile);
		newnfsstats.srvlocks--;
		nfsrv_openpluslock--;
	}
	LIST_REMOVE(lop, lo_lckowner);
	FREE((caddr_t)lop, M_NFSDLOCK);
}

/*
 * This function frees an nfslockfile structure.
 */
static void
nfsrv_freenfslockfile(struct nfslockfile *lfp)
{

	LIST_REMOVE(lfp, lf_hash);
	FREE((caddr_t)lfp, M_NFSDLOCKFILE);
}

/*
 * This function looks up an nfsstate structure via stateid.
 */
static int
nfsrv_getstate(struct nfsclient *clp, nfsv4stateid_t *stateidp, __unused u_int32_t flags,
    struct nfsstate **stpp)
{
	struct nfsstate *stp;
	struct nfsstatehead *hp;

	*stpp = NULL;
	hp = NFSSTATEHASH(clp, *stateidp);
	LIST_FOREACH(stp, hp, ls_hash) {
		if (!NFSBCMP(stp->ls_stateid.other, stateidp->other,
			NFSX_STATEIDOTHER))
			break;
	}

	/*
	 * If no state id in list, return NFSERR_BADSTATEID.
	 */
	if (stp == LIST_END(hp))
		return (NFSERR_BADSTATEID);
	*stpp = stp;
	return (0);
}

/*
 * This function gets an nfsstate structure via owner string.
 */
static void
nfsrv_getowner(struct nfsstatehead *hp, struct nfsstate *new_stp,
    struct nfsstate **stpp)
{
	struct nfsstate *stp;

	*stpp = NULL;
	LIST_FOREACH(stp, hp, ls_list) {
		if (new_stp->ls_ownerlen == stp->ls_ownerlen &&
		  !NFSBCMP(new_stp->ls_owner,stp->ls_owner,stp->ls_ownerlen)) {
			*stpp = stp;
			return;
		}
	}
}

/*
 * Lock control function called to update lock status.
 * Returns 0 upon success, -1 if there is no lock and the flags indicate
 * that one isn't to be created and an NFSERR_xxx for other errors.
 * The structures new_stp and new_lop are passed in as pointers that should
 * be set to NULL if the structure is used and shouldn't be free'd.
 * For the NFSLCK_TEST and NFSLCK_CHECK cases, the structures are
 * never used and can safely be allocated on the stack. For all other
 * cases, *new_stpp and *new_lopp should be malloc'd before the call,
 * in case they are used.
 */
APPLESTATIC int
nfsrv_lockctrl(vnode_t vp, struct nfsstate **new_stpp,
    struct nfslock **new_lopp, struct nfslockconflict *cfp,
    nfsquad_t clientid, nfsv4stateid_t *stateidp,
    __unused struct nfsexstuff *exp,
    struct nfsrv_descript *nd, NFSPROC_T *p)
{
	struct nfslock *lop;
	struct nfsstate *new_stp = *new_stpp;
	struct nfslock *new_lop = *new_lopp;
	struct nfsstate *tstp, *mystp, *nstp;
	int specialid = 0;
	struct nfslockfile *lfp;
	struct nfslock *other_lop = NULL;
	struct nfsstate *stp, *lckstp = NULL;
	struct nfsclient *clp = NULL;
	u_int32_t bits;
	int error = 0, haslock = 0, ret, reterr;
	int getlckret, delegation = 0, filestruct_locked;
	fhandle_t nfh;
	uint64_t first, end;
	uint32_t lock_flags;

	if (new_stp->ls_flags & (NFSLCK_CHECK | NFSLCK_SETATTR)) {
		/*
		 * Note the special cases of "all 1s" or "all 0s" stateids and
		 * let reads with all 1s go ahead.
		 */
		if (new_stp->ls_stateid.seqid == 0x0 &&
		    new_stp->ls_stateid.other[0] == 0x0 &&
		    new_stp->ls_stateid.other[1] == 0x0 &&
		    new_stp->ls_stateid.other[2] == 0x0)
			specialid = 1;
		else if (new_stp->ls_stateid.seqid == 0xffffffff &&
		    new_stp->ls_stateid.other[0] == 0xffffffff &&
		    new_stp->ls_stateid.other[1] == 0xffffffff &&
		    new_stp->ls_stateid.other[2] == 0xffffffff)
			specialid = 2;
	}

	/*
	 * Check for restart conditions (client and server).
	 */
	error = nfsrv_checkrestart(clientid, new_stp->ls_flags,
	    &new_stp->ls_stateid, specialid);
	if (error)
		return (error);

	/*
	 * Check for state resource limit exceeded.
	 */
	if ((new_stp->ls_flags & NFSLCK_LOCK) &&
	    nfsrv_openpluslock > NFSRV_V4STATELIMIT)
		return (NFSERR_RESOURCE);

	/*
	 * For the lock case, get another nfslock structure,
	 * just in case we need it.
	 * Malloc now, before we start sifting through the linked lists,
	 * in case we have to wait for memory.
	 */
tryagain:
	if (new_stp->ls_flags & NFSLCK_LOCK)
		MALLOC(other_lop, struct nfslock *, sizeof (struct nfslock),
		    M_NFSDLOCK, M_WAITOK);
	filestruct_locked = 0;
	reterr = 0;
	lfp = NULL;

	/*
	 * Get the lockfile structure for CFH now, so we can do a sanity
	 * check against the stateid, before incrementing the seqid#, since
	 * we want to return NFSERR_BADSTATEID on failure and the seqid#
	 * shouldn't be incremented for this case.
	 * If nfsrv_getlockfile() returns -1, it means "not found", which
	 * will be handled later.
	 * If we are doing Lock/LockU and local locking is enabled, sleep
	 * lock the nfslockfile structure.
	 */
	getlckret = nfsrv_getlockfh(vp, new_stp->ls_flags, NULL, &nfh, p);
	NFSLOCKSTATE();
	if (getlckret == 0) {
		if ((new_stp->ls_flags & (NFSLCK_LOCK | NFSLCK_UNLOCK)) != 0 &&
		    nfsrv_dolocallocks != 0 && nd->nd_repstat == 0) {
			getlckret = nfsrv_getlockfile(new_stp->ls_flags, NULL,
			    &lfp, &nfh, 1);
			if (getlckret == 0)
				filestruct_locked = 1;
		} else
			getlckret = nfsrv_getlockfile(new_stp->ls_flags, NULL,
			    &lfp, &nfh, 0);
	}
	if (getlckret != 0 && getlckret != -1)
		reterr = getlckret;

	if (filestruct_locked != 0) {
		LIST_INIT(&lfp->lf_rollback);
		if ((new_stp->ls_flags & NFSLCK_LOCK)) {
			/*
			 * For local locking, do the advisory locking now, so
			 * that any conflict can be detected. A failure later
			 * can be rolled back locally. If an error is returned,
			 * struct nfslockfile has been unlocked and any local
			 * locking rolled back.
			 */
			NFSUNLOCKSTATE();
			reterr = nfsrv_locallock(vp, lfp,
			    (new_lop->lo_flags & (NFSLCK_READ | NFSLCK_WRITE)),
			    new_lop->lo_first, new_lop->lo_end, cfp, p);
			NFSLOCKSTATE();
		}
	}

	if (specialid == 0) {
	    if (new_stp->ls_flags & NFSLCK_TEST) {
		/*
		 * RFC 3530 does not list LockT as an op that renews a
		 * lease, but the concensus seems to be that it is ok
		 * for a server to do so.
		 */
		error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
		    (nfsquad_t)((u_quad_t)0), NULL, p);

		/*
		 * Since NFSERR_EXPIRED, NFSERR_ADMINREVOKED are not valid
		 * error returns for LockT, just go ahead and test for a lock,
		 * since there are no locks for this client, but other locks
		 * can conflict. (ie. same client will always be false)
		 */
		if (error == NFSERR_EXPIRED || error == NFSERR_ADMINREVOKED)
		    error = 0;
		lckstp = new_stp;
	    } else {
	      error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
		(nfsquad_t)((u_quad_t)0), NULL, p);
	      if (error == 0)
		/*
		 * Look up the stateid
		 */
		error = nfsrv_getstate(clp, &new_stp->ls_stateid,
		  new_stp->ls_flags, &stp);
	      /*
	       * do some sanity checks for an unconfirmed open or a
	       * stateid that refers to the wrong file, for an open stateid
	       */
	      if (error == 0 && (stp->ls_flags & NFSLCK_OPEN) &&
		  ((stp->ls_openowner->ls_flags & NFSLCK_NEEDSCONFIRM) ||
		   (getlckret == 0 && stp->ls_lfp != lfp)))
			error = NFSERR_BADSTATEID;
	      if (error == 0 &&
		  (stp->ls_flags & (NFSLCK_DELEGREAD | NFSLCK_DELEGWRITE)) &&
		  getlckret == 0 && stp->ls_lfp != lfp)
			error = NFSERR_BADSTATEID;

	      /*
	       * If the lockowner stateid doesn't refer to the same file,
	       * I believe that is considered ok, since some clients will
	       * only create a single lockowner and use that for all locks
	       * on all files.
	       * For now, log it as a diagnostic, instead of considering it
	       * a BadStateid.
	       */
	      if (error == 0 && (stp->ls_flags &
		  (NFSLCK_OPEN | NFSLCK_DELEGREAD | NFSLCK_DELEGWRITE)) == 0 &&
		  getlckret == 0 && stp->ls_lfp != lfp) {
#ifdef DIAGNOSTIC
		  printf("Got a lock statid for different file open\n");
#endif
		  /*
		  error = NFSERR_BADSTATEID;
		  */
	      }

	      if (error == 0) {
		    if (new_stp->ls_flags & NFSLCK_OPENTOLOCK) {
			/*
			 * If haslock set, we've already checked the seqid.
			 */
			if (!haslock) {
			    if (stp->ls_flags & NFSLCK_OPEN)
				error = nfsrv_checkseqid(nd, new_stp->ls_seq,
				    stp->ls_openowner, new_stp->ls_op);
			    else
				error = NFSERR_BADSTATEID;
			}
			if (!error)
			    nfsrv_getowner(&stp->ls_open, new_stp, &lckstp);
			if (lckstp)
			    /*
			     * I believe this should be an error, but it
			     * isn't obvious what NFSERR_xxx would be
			     * appropriate, so I'll use NFSERR_INVAL for now.
			     */
			    error = NFSERR_INVAL;
			else
			    lckstp = new_stp;
		    } else if (new_stp->ls_flags&(NFSLCK_LOCK|NFSLCK_UNLOCK)) {
			/*
			 * If haslock set, ditto above.
			 */
			if (!haslock) {
			    if (stp->ls_flags & NFSLCK_OPEN)
				error = NFSERR_BADSTATEID;
			    else
				error = nfsrv_checkseqid(nd, new_stp->ls_seq,
				    stp, new_stp->ls_op);
			}
			lckstp = stp;
		    } else {
			lckstp = stp;
		    }
	      }
	      /*
	       * If the seqid part of the stateid isn't the same, return
	       * NFSERR_OLDSTATEID for cases other than I/O Ops.
	       * For I/O Ops, only return NFSERR_OLDSTATEID if
	       * nfsrv_returnoldstateid is set. (The concensus on the email
	       * list was that most clients would prefer to not receive
	       * NFSERR_OLDSTATEID for I/O Ops, but the RFC suggests that that
	       * is what will happen, so I use the nfsrv_returnoldstateid to
	       * allow for either server configuration.)
	       */
	      if (!error && stp->ls_stateid.seqid!=new_stp->ls_stateid.seqid &&
		  (!(new_stp->ls_flags & NFSLCK_CHECK) ||
		   nfsrv_returnoldstateid))
		    error = NFSERR_OLDSTATEID;
	    }
	}

	/*
	 * Now we can check for grace.
	 */
	if (!error)
		error = nfsrv_checkgrace(new_stp->ls_flags);
	if ((new_stp->ls_flags & NFSLCK_RECLAIM) && !error &&
		nfsrv_checkstable(clp))
		error = NFSERR_NOGRACE;
	/*
	 * If we successfully Reclaimed state, note that.
	 */
	if ((new_stp->ls_flags & NFSLCK_RECLAIM) && !error)
		nfsrv_markstable(clp);

	/*
	 * At this point, either error == NFSERR_BADSTATEID or the
	 * seqid# has been updated, so we can return any error.
	 * If error == 0, there may be an error in:
	 *    nd_repstat - Set by the calling function.
	 *    reterr - Set above, if getting the nfslockfile structure
	 *       or acquiring the local lock failed.
	 *    (If both of these are set, nd_repstat should probably be
	 *     returned, since that error was detected before this
	 *     function call.)
	 */
	if (error != 0 || nd->nd_repstat != 0 || reterr != 0) {
		if (error == 0) {
			if (nd->nd_repstat != 0)
				error = nd->nd_repstat;
			else
				error = reterr;
		}
		if (filestruct_locked != 0) {
			/* Roll back local locks. */
			NFSUNLOCKSTATE();
			nfsrv_locallock_rollback(vp, lfp, p);
			NFSLOCKSTATE();
			nfsrv_unlocklf(lfp);
		}
		NFSUNLOCKSTATE();
		if (other_lop)
			FREE((caddr_t)other_lop, M_NFSDLOCK);
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (error);
	}

	/*
	 * Check the nfsrv_getlockfile return.
	 * Returned -1 if no structure found.
	 */
	if (getlckret == -1) {
		error = NFSERR_EXPIRED;
		/*
		 * Called from lockt, so no lock is OK.
		 */
		if (new_stp->ls_flags & NFSLCK_TEST) {
			error = 0;
		} else if (new_stp->ls_flags &
		    (NFSLCK_CHECK | NFSLCK_SETATTR)) {
			/*
			 * Called to check for a lock, OK if the stateid is all
			 * 1s or all 0s, but there should be an nfsstate
			 * otherwise.
			 * (ie. If there is no open, I'll assume no share
			 *  deny bits.)
			 */
			if (specialid)
				error = 0;
			else
				error = NFSERR_BADSTATEID;
		}
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		/*
		 * Called to lock or unlock, so the lock has gone away.
		 */
		return (error);
	}

	/*
	 * For NFSLCK_CHECK and NFSLCK_LOCK, test for a share conflict.
	 * For NFSLCK_CHECK, allow a read if write access is granted,
	 * but check for a deny. For NFSLCK_LOCK, require correct access,
	 * which implies a conflicting deny can't exist.
	 */
	if (new_stp->ls_flags & (NFSLCK_CHECK | NFSLCK_LOCK)) {
	    /*
	     * Four kinds of state id:
	     * - specialid (all 0s or all 1s), only for NFSLCK_CHECK
	     * - stateid for an open
	     * - stateid for a delegation
	     * - stateid for a lock owner
	     */
	    if (!specialid) {
		if (stp->ls_flags & (NFSLCK_DELEGREAD | NFSLCK_DELEGWRITE)) {
		    delegation = 1;
		    mystp = stp;
		    nfsrv_delaydelegtimeout(stp);
	        } else if (stp->ls_flags & NFSLCK_OPEN) {
		    mystp = stp;
		} else {
		    mystp = stp->ls_openstp;
		}
		/*
		 * If locking or checking, require correct access
		 * bit set.
		 */
		if (((new_stp->ls_flags & NFSLCK_LOCK) &&
		     !((new_lop->lo_flags >> NFSLCK_LOCKSHIFT) &
		       mystp->ls_flags & NFSLCK_ACCESSBITS)) ||
		    ((new_stp->ls_flags & (NFSLCK_CHECK|NFSLCK_READACCESS)) ==
		      (NFSLCK_CHECK | NFSLCK_READACCESS) &&
		     !(mystp->ls_flags & NFSLCK_READACCESS)) ||
		    ((new_stp->ls_flags & (NFSLCK_CHECK|NFSLCK_WRITEACCESS)) ==
		      (NFSLCK_CHECK | NFSLCK_WRITEACCESS) &&
		     !(mystp->ls_flags & NFSLCK_WRITEACCESS))) {
			if (filestruct_locked != 0) {
				/* Roll back local locks. */
				NFSUNLOCKSTATE();
				nfsrv_locallock_rollback(vp, lfp, p);
				NFSLOCKSTATE();
				nfsrv_unlocklf(lfp);
			}
			NFSUNLOCKSTATE();
			if (other_lop)
				FREE((caddr_t)other_lop, M_NFSDLOCK);
			if (haslock) {
				NFSLOCKV4ROOTMUTEX();
				nfsv4_unlock(&nfsv4rootfs_lock, 1);
				NFSUNLOCKV4ROOTMUTEX();
			}
			return (NFSERR_OPENMODE);
		}
	    } else
		mystp = NULL;
	    if ((new_stp->ls_flags & NFSLCK_CHECK) && !delegation) {
		/*
		 * Check for a conflicting deny bit.
		 */
		LIST_FOREACH(tstp, &lfp->lf_open, ls_file) {
		    if (tstp != mystp) {
			bits = tstp->ls_flags;
			bits >>= NFSLCK_SHIFT;
			if (new_stp->ls_flags & bits & NFSLCK_ACCESSBITS) {
			    ret = nfsrv_clientconflict(tstp->ls_clp, &haslock,
				vp, p);
			    if (ret == 1) {
				/*
				* nfsrv_clientconflict unlocks state
				 * when it returns non-zero.
				 */
				lckstp = NULL;
				goto tryagain;
			    }
			    if (ret == 0)
				NFSUNLOCKSTATE();
			    if (haslock) {
				NFSLOCKV4ROOTMUTEX();
				nfsv4_unlock(&nfsv4rootfs_lock, 1);
				NFSUNLOCKV4ROOTMUTEX();
			    }
			    if (ret == 2)
				return (NFSERR_PERM);
			    else
				return (NFSERR_OPENMODE);
			}
		    }
		}

		/* We're outta here */
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (0);
	    }
	}

	/*
	 * For setattr, just get rid of all the Delegations for other clients.
	 */
	if (new_stp->ls_flags & NFSLCK_SETATTR) {
		ret = nfsrv_cleandeleg(vp, lfp, clp, &haslock, p);
		if (ret) {
			/*
			 * nfsrv_cleandeleg() unlocks state when it
			 * returns non-zero.
			 */
			if (ret == -1) {
				lckstp = NULL;
				goto tryagain;
			}
			return (ret);
		}
		if (!(new_stp->ls_flags & NFSLCK_CHECK) ||
		    (LIST_EMPTY(&lfp->lf_open) && LIST_EMPTY(&lfp->lf_lock) &&
		     LIST_EMPTY(&lfp->lf_deleg))) {
			NFSUNLOCKSTATE();
			if (haslock) {
				NFSLOCKV4ROOTMUTEX();
				nfsv4_unlock(&nfsv4rootfs_lock, 1);
				NFSUNLOCKV4ROOTMUTEX();
			}
			return (0);
		}
	}

	/*
	 * Check for a conflicting delegation. If one is found, call
	 * nfsrv_delegconflict() to handle it. If the v4root lock hasn't
	 * been set yet, it will get the lock. Otherwise, it will recall
	 * the delegation. Then, we try try again...
	 * I currently believe the conflict algorithm to be:
	 * For Lock Ops (Lock/LockT/LockU)
	 * - there is a conflict iff a different client has a write delegation
	 * For Reading (Read Op)
	 * - there is a conflict iff a different client has a write delegation
	 *   (the specialids are always a different client)
	 * For Writing (Write/Setattr of size)
	 * - there is a conflict if a different client has any delegation
	 * - there is a conflict if the same client has a read delegation
	 *   (I don't understand why this isn't allowed, but that seems to be
	 *    the current concensus?)
	 */
	tstp = LIST_FIRST(&lfp->lf_deleg);
	while (tstp != LIST_END(&lfp->lf_deleg)) {
	    nstp = LIST_NEXT(tstp, ls_file);
	    if ((((new_stp->ls_flags&(NFSLCK_LOCK|NFSLCK_UNLOCK|NFSLCK_TEST))||
		 ((new_stp->ls_flags & NFSLCK_CHECK) &&
		  (new_lop->lo_flags & NFSLCK_READ))) &&
		  clp != tstp->ls_clp &&
		 (tstp->ls_flags & NFSLCK_DELEGWRITE)) ||
		 ((new_stp->ls_flags & NFSLCK_CHECK) &&
		   (new_lop->lo_flags & NFSLCK_WRITE) &&
		  (clp != tstp->ls_clp ||
		   (tstp->ls_flags & NFSLCK_DELEGREAD)))) {
		if (filestruct_locked != 0) {
			/* Roll back local locks. */
			NFSUNLOCKSTATE();
			nfsrv_locallock_rollback(vp, lfp, p);
			NFSLOCKSTATE();
			nfsrv_unlocklf(lfp);
		}
		ret = nfsrv_delegconflict(tstp, &haslock, p, vp);
		if (ret) {
		    /*
		     * nfsrv_delegconflict unlocks state when it
		     * returns non-zero, which it always does.
		     */
		    if (other_lop) {
			FREE((caddr_t)other_lop, M_NFSDLOCK);
			other_lop = NULL;
		    }
		    if (ret == -1) {
			lckstp = NULL;
			goto tryagain;
		    }
		    return (ret);
		}
		/* Never gets here. */
	    }
	    tstp = nstp;
	}

	/*
	 * Handle the unlock case by calling nfsrv_updatelock().
	 * (Should I have done some access checking above for unlock? For now,
	 *  just let it happen.)
	 */
	if (new_stp->ls_flags & NFSLCK_UNLOCK) {
		first = new_lop->lo_first;
		end = new_lop->lo_end;
		nfsrv_updatelock(stp, new_lopp, &other_lop, lfp);
		stateidp->seqid = ++(stp->ls_stateid.seqid);
		stateidp->other[0] = stp->ls_stateid.other[0];
		stateidp->other[1] = stp->ls_stateid.other[1];
		stateidp->other[2] = stp->ls_stateid.other[2];
		if (filestruct_locked != 0) {
			NFSUNLOCKSTATE();
			/* Update the local locks. */
			nfsrv_localunlock(vp, lfp, first, end, p);
			NFSLOCKSTATE();
			nfsrv_unlocklf(lfp);
		}
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (0);
	}

	/*
	 * Search for a conflicting lock. A lock conflicts if:
	 * - the lock range overlaps and
	 * - at least one lock is a write lock and
	 * - it is not owned by the same lock owner
	 */
	if (!delegation) {
	  LIST_FOREACH(lop, &lfp->lf_lock, lo_lckfile) {
	    if (new_lop->lo_end > lop->lo_first &&
		new_lop->lo_first < lop->lo_end &&
		(new_lop->lo_flags == NFSLCK_WRITE ||
		 lop->lo_flags == NFSLCK_WRITE) &&
		lckstp != lop->lo_stp &&
		(clp != lop->lo_stp->ls_clp ||
		 lckstp->ls_ownerlen != lop->lo_stp->ls_ownerlen ||
		 NFSBCMP(lckstp->ls_owner, lop->lo_stp->ls_owner,
		    lckstp->ls_ownerlen))) {
		if (other_lop) {
		    FREE((caddr_t)other_lop, M_NFSDLOCK);
		    other_lop = NULL;
		}
		ret = nfsrv_clientconflict(lop->lo_stp->ls_clp,&haslock,vp,p);
		if (ret == 1) {
		    if (filestruct_locked != 0) {
			/* Roll back local locks. */
			nfsrv_locallock_rollback(vp, lfp, p);
			NFSLOCKSTATE();
			nfsrv_unlocklf(lfp);
			NFSUNLOCKSTATE();
		    }
		    /*
		     * nfsrv_clientconflict() unlocks state when it
		     * returns non-zero.
		     */
		    lckstp = NULL;
		    goto tryagain;
		}
		/*
		 * Found a conflicting lock, so record the conflict and
		 * return the error.
		 */
		if (cfp != NULL && ret == 0) {
		    cfp->cl_clientid.lval[0]=lop->lo_stp->ls_stateid.other[0];
		    cfp->cl_clientid.lval[1]=lop->lo_stp->ls_stateid.other[1];
		    cfp->cl_first = lop->lo_first;
		    cfp->cl_end = lop->lo_end;
		    cfp->cl_flags = lop->lo_flags;
		    cfp->cl_ownerlen = lop->lo_stp->ls_ownerlen;
		    NFSBCOPY(lop->lo_stp->ls_owner, cfp->cl_owner,
			cfp->cl_ownerlen);
		}
		if (ret == 2)
		    error = NFSERR_PERM;
		else if (new_stp->ls_flags & NFSLCK_RECLAIM)
		    error = NFSERR_RECLAIMCONFLICT;
		else if (new_stp->ls_flags & NFSLCK_CHECK)
		    error = NFSERR_LOCKED;
		else
		    error = NFSERR_DENIED;
		if (filestruct_locked != 0 && ret == 0) {
			/* Roll back local locks. */
			NFSUNLOCKSTATE();
			nfsrv_locallock_rollback(vp, lfp, p);
			NFSLOCKSTATE();
			nfsrv_unlocklf(lfp);
		}
		if (ret == 0)
			NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (error);
	    }
	  }
	}

	/*
	 * We only get here if there was no lock that conflicted.
	 */
	if (new_stp->ls_flags & (NFSLCK_TEST | NFSLCK_CHECK)) {
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (0);
	}

	/*
	 * We only get here when we are creating or modifying a lock.
	 * There are two variants:
	 * - exist_lock_owner where lock_owner exists
	 * - open_to_lock_owner with new lock_owner
	 */
	first = new_lop->lo_first;
	end = new_lop->lo_end;
	lock_flags = new_lop->lo_flags;
	if (!(new_stp->ls_flags & NFSLCK_OPENTOLOCK)) {
		nfsrv_updatelock(lckstp, new_lopp, &other_lop, lfp);
		stateidp->seqid = ++(lckstp->ls_stateid.seqid);
		stateidp->other[0] = lckstp->ls_stateid.other[0];
		stateidp->other[1] = lckstp->ls_stateid.other[1];
		stateidp->other[2] = lckstp->ls_stateid.other[2];
	} else {
		/*
		 * The new open_to_lock_owner case.
		 * Link the new nfsstate into the lists.
		 */
		new_stp->ls_seq = new_stp->ls_opentolockseq;
		nfsrvd_refcache(new_stp->ls_op);
		stateidp->seqid = new_stp->ls_stateid.seqid = 1;
		stateidp->other[0] = new_stp->ls_stateid.other[0] =
		    clp->lc_clientid.lval[0];
		stateidp->other[1] = new_stp->ls_stateid.other[1] =
		    clp->lc_clientid.lval[1];
		stateidp->other[2] = new_stp->ls_stateid.other[2] =
		    nfsrv_nextstateindex(clp);
		new_stp->ls_clp = clp;
		LIST_INIT(&new_stp->ls_lock);
		new_stp->ls_openstp = stp;
		new_stp->ls_lfp = lfp;
		nfsrv_insertlock(new_lop, (struct nfslock *)new_stp, new_stp,
		    lfp);
		LIST_INSERT_HEAD(NFSSTATEHASH(clp, new_stp->ls_stateid),
		    new_stp, ls_hash);
		LIST_INSERT_HEAD(&stp->ls_open, new_stp, ls_list);
		*new_lopp = NULL;
		*new_stpp = NULL;
		newnfsstats.srvlockowners++;
		nfsrv_openpluslock++;
	}
	if (filestruct_locked != 0) {
		NFSUNLOCKSTATE();
		nfsrv_locallock_commit(lfp, lock_flags, first, end);
		NFSLOCKSTATE();
		nfsrv_unlocklf(lfp);
	}
	NFSUNLOCKSTATE();
	if (haslock) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
	}
	if (other_lop)
		FREE((caddr_t)other_lop, M_NFSDLOCK);
	return (0);
}

/*
 * Check for state errors for Open.
 * repstat is passed back out as an error if more critical errors
 * are not detected.
 */
APPLESTATIC int
nfsrv_opencheck(nfsquad_t clientid, nfsv4stateid_t *stateidp,
    struct nfsstate *new_stp, vnode_t vp, struct nfsrv_descript *nd,
    NFSPROC_T *p, int repstat)
{
	struct nfsstate *stp, *nstp;
	struct nfsclient *clp;
	struct nfsstate *ownerstp;
	struct nfslockfile *lfp, *new_lfp;
	int error, haslock = 0, ret, readonly = 0, getfhret = 0;

	if ((new_stp->ls_flags & NFSLCK_SHAREBITS) == NFSLCK_READACCESS)
		readonly = 1;
	/*
	 * Check for restart conditions (client and server).
	 */
	error = nfsrv_checkrestart(clientid, new_stp->ls_flags,
		&new_stp->ls_stateid, 0);
	if (error)
		return (error);

	/*
	 * Check for state resource limit exceeded.
	 * Technically this should be SMP protected, but the worst
	 * case error is "out by one or two" on the count when it
	 * returns NFSERR_RESOURCE and the limit is just a rather
	 * arbitrary high water mark, so no harm is done.
	 */
	if (nfsrv_openpluslock > NFSRV_V4STATELIMIT)
		return (NFSERR_RESOURCE);

tryagain:
	MALLOC(new_lfp, struct nfslockfile *, sizeof (struct nfslockfile),
	    M_NFSDLOCKFILE, M_WAITOK);
	if (vp)
		getfhret = nfsrv_getlockfh(vp, new_stp->ls_flags, &new_lfp,
		    NULL, p);
	NFSLOCKSTATE();
	/*
	 * Get the nfsclient structure.
	 */
	error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
	    (nfsquad_t)((u_quad_t)0), NULL, p);

	/*
	 * Look up the open owner. See if it needs confirmation and
	 * check the seq#, as required.
	 */
	if (!error)
		nfsrv_getowner(&clp->lc_open, new_stp, &ownerstp);

	if (!error && ownerstp) {
		error = nfsrv_checkseqid(nd, new_stp->ls_seq, ownerstp,
		    new_stp->ls_op);
		/*
		 * If the OpenOwner hasn't been confirmed, assume the
		 * old one was a replay and this one is ok.
		 * See: RFC3530 Sec. 14.2.18.
		 */
		if (error == NFSERR_BADSEQID &&
		    (ownerstp->ls_flags & NFSLCK_NEEDSCONFIRM))
			error = 0;
	}

	/*
	 * Check for grace.
	 */
	if (!error)
		error = nfsrv_checkgrace(new_stp->ls_flags);
	if ((new_stp->ls_flags & NFSLCK_RECLAIM) && !error &&
		nfsrv_checkstable(clp))
		error = NFSERR_NOGRACE;

	/*
	 * If none of the above errors occurred, let repstat be
	 * returned.
	 */
	if (repstat && !error)
		error = repstat;
	if (error) {
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		free((caddr_t)new_lfp, M_NFSDLOCKFILE);
		return (error);
	}

	/*
	 * If vp == NULL, the file doesn't exist yet, so return ok.
	 * (This always happens on the first pass, so haslock must be 0.)
	 */
	if (vp == NULL) {
		NFSUNLOCKSTATE();
		FREE((caddr_t)new_lfp, M_NFSDLOCKFILE);
		return (0);
	}

	/*
	 * Get the structure for the underlying file.
	 */
	if (getfhret)
		error = getfhret;
	else
		error = nfsrv_getlockfile(new_stp->ls_flags, &new_lfp, &lfp,
		    NULL, 0);
	if (new_lfp)
		FREE((caddr_t)new_lfp, M_NFSDLOCKFILE);
	if (error) {
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (error);
	}

	/*
	 * Search for a conflicting open/share.
	 */
	if (new_stp->ls_flags & NFSLCK_DELEGCUR) {
	    /*
	     * For Delegate_Cur, search for the matching Delegation,
	     * which indicates no conflict.
	     * An old delegation should have been recovered by the
	     * client doing a Claim_DELEGATE_Prev, so I won't let
	     * it match and return NFSERR_EXPIRED. Should I let it
	     * match?
	     */
	    LIST_FOREACH(stp, &lfp->lf_deleg, ls_file) {
		if (!(stp->ls_flags & NFSLCK_OLDDELEG) &&
		    stateidp->seqid == stp->ls_stateid.seqid &&
		    !NFSBCMP(stateidp->other, stp->ls_stateid.other,
			  NFSX_STATEIDOTHER))
			break;
	    }
	    if (stp == LIST_END(&lfp->lf_deleg) ||
		((new_stp->ls_flags & NFSLCK_WRITEACCESS) &&
		 (stp->ls_flags & NFSLCK_DELEGREAD))) {
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (NFSERR_EXPIRED);
	    }
	}

	/*
	 * Check for access/deny bit conflicts. I check for the same
	 * owner as well, in case the client didn't bother.
	 */
	LIST_FOREACH(stp, &lfp->lf_open, ls_file) {
		if (!(new_stp->ls_flags & NFSLCK_DELEGCUR) &&
		    (((new_stp->ls_flags & NFSLCK_ACCESSBITS) &
		      ((stp->ls_flags>>NFSLCK_SHIFT) & NFSLCK_ACCESSBITS))||
		     ((stp->ls_flags & NFSLCK_ACCESSBITS) &
		      ((new_stp->ls_flags>>NFSLCK_SHIFT)&NFSLCK_ACCESSBITS)))){
			ret = nfsrv_clientconflict(stp->ls_clp,&haslock,vp,p);
			if (ret == 1) {
				/*
				 * nfsrv_clientconflict() unlocks
				 * state when it returns non-zero.
				 */
				goto tryagain;
			}
			if (ret == 2)
				error = NFSERR_PERM;
			else if (new_stp->ls_flags & NFSLCK_RECLAIM)
				error = NFSERR_RECLAIMCONFLICT;
			else
				error = NFSERR_SHAREDENIED;
			if (ret == 0)
				NFSUNLOCKSTATE();
			if (haslock) {
				NFSLOCKV4ROOTMUTEX();
				nfsv4_unlock(&nfsv4rootfs_lock, 1);
				NFSUNLOCKV4ROOTMUTEX();
			}
			return (error);
		}
	}

	/*
	 * Check for a conflicting delegation. If one is found, call
	 * nfsrv_delegconflict() to handle it. If the v4root lock hasn't
	 * been set yet, it will get the lock. Otherwise, it will recall
	 * the delegation. Then, we try try again...
	 * (If NFSLCK_DELEGCUR is set, it has a delegation, so there
	 *  isn't a conflict.)
	 * I currently believe the conflict algorithm to be:
	 * For Open with Read Access and Deny None
	 * - there is a conflict iff a different client has a write delegation
	 * For Open with other Write Access or any Deny except None
	 * - there is a conflict if a different client has any delegation
	 * - there is a conflict if the same client has a read delegation
	 *   (The current concensus is that this last case should be
	 *    considered a conflict since the client with a read delegation
	 *    could have done an Open with ReadAccess and WriteDeny
	 *    locally and then not have checked for the WriteDeny.)
	 * Don't check for a Reclaim, since that will be dealt with
	 * by nfsrv_openctrl().
	 */
	if (!(new_stp->ls_flags &
		(NFSLCK_DELEGPREV | NFSLCK_DELEGCUR | NFSLCK_RECLAIM))) {
	    stp = LIST_FIRST(&lfp->lf_deleg);
	    while (stp != LIST_END(&lfp->lf_deleg)) {
		nstp = LIST_NEXT(stp, ls_file);
		if ((readonly && stp->ls_clp != clp &&
		       (stp->ls_flags & NFSLCK_DELEGWRITE)) ||
		    (!readonly && (stp->ls_clp != clp ||
		         (stp->ls_flags & NFSLCK_DELEGREAD)))) {
			ret = nfsrv_delegconflict(stp, &haslock, p, vp);
			if (ret) {
			    /*
			     * nfsrv_delegconflict() unlocks state
			     * when it returns non-zero.
			     */
			    if (ret == -1)
				goto tryagain;
			    return (ret);
			}
		}
		stp = nstp;
	    }
	}
	NFSUNLOCKSTATE();
	if (haslock) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
	}
	return (0);
}

/*
 * Open control function to create/update open state for an open.
 */
APPLESTATIC int
nfsrv_openctrl(struct nfsrv_descript *nd, vnode_t vp,
    struct nfsstate **new_stpp, nfsquad_t clientid, nfsv4stateid_t *stateidp,
    nfsv4stateid_t *delegstateidp, u_int32_t *rflagsp, struct nfsexstuff *exp,
    NFSPROC_T *p, u_quad_t filerev)
{
	struct nfsstate *new_stp = *new_stpp;
	struct nfsstate *stp, *nstp;
	struct nfsstate *openstp = NULL, *new_open, *ownerstp, *new_deleg;
	struct nfslockfile *lfp, *new_lfp;
	struct nfsclient *clp;
	int error, haslock = 0, ret, delegate = 1, writedeleg = 1;
	int readonly = 0, cbret = 1, getfhret = 0;

	if ((new_stp->ls_flags & NFSLCK_SHAREBITS) == NFSLCK_READACCESS)
		readonly = 1;
	/*
	 * Check for restart conditions (client and server).
	 * (Paranoia, should have been detected by nfsrv_opencheck().)
	 * If an error does show up, return NFSERR_EXPIRED, since the
	 * the seqid# has already been incremented.
	 */
	error = nfsrv_checkrestart(clientid, new_stp->ls_flags,
	    &new_stp->ls_stateid, 0);
	if (error) {
		printf("Nfsd: openctrl unexpected restart err=%d\n",
		    error);
		return (NFSERR_EXPIRED);
	}

tryagain:
	MALLOC(new_lfp, struct nfslockfile *, sizeof (struct nfslockfile),
	    M_NFSDLOCKFILE, M_WAITOK);
	MALLOC(new_open, struct nfsstate *, sizeof (struct nfsstate),
	    M_NFSDSTATE, M_WAITOK);
	MALLOC(new_deleg, struct nfsstate *, sizeof (struct nfsstate),
	    M_NFSDSTATE, M_WAITOK);
	getfhret = nfsrv_getlockfh(vp, new_stp->ls_flags, &new_lfp,
	    NULL, p);
	NFSLOCKSTATE();
	/*
	 * Get the client structure. Since the linked lists could be changed
	 * by other nfsd processes if this process does a tsleep(), one of
	 * two things must be done.
	 * 1 - don't tsleep()
	 * or
	 * 2 - get the nfsv4_lock() { indicated by haslock == 1 }
	 *     before using the lists, since this lock stops the other
	 *     nfsd. This should only be used for rare cases, since it
	 *     essentially single threads the nfsd.
	 *     At this time, it is only done for cases where the stable
	 *     storage file must be written prior to completion of state
	 *     expiration.
	 */
	error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
	    (nfsquad_t)((u_quad_t)0), NULL, p);
	if (!error && (clp->lc_flags & LCL_NEEDSCBNULL) &&
	    clp->lc_program) {
		/*
		 * This happens on the first open for a client
		 * that supports callbacks.
		 */
		NFSUNLOCKSTATE();
		/*
		 * Although nfsrv_docallback() will sleep, clp won't
		 * go away, since they are only removed when the
		 * nfsv4_lock() has blocked the nfsd threads. The
		 * fields in clp can change, but having multiple
		 * threads do this Null callback RPC should be
		 * harmless.
		 */
		cbret = nfsrv_docallback(clp, NFSV4PROC_CBNULL,
		    NULL, 0, NULL, NULL, NULL, p);
		NFSLOCKSTATE();
		clp->lc_flags &= ~LCL_NEEDSCBNULL;
		if (!cbret)
			clp->lc_flags |= LCL_CALLBACKSON;
	}

	/*
	 * Look up the open owner. See if it needs confirmation and
	 * check the seq#, as required.
	 */
	if (!error)
		nfsrv_getowner(&clp->lc_open, new_stp, &ownerstp);

	if (error) {
		NFSUNLOCKSTATE();
		printf("Nfsd: openctrl unexpected state err=%d\n",
			error);
		free((caddr_t)new_lfp, M_NFSDLOCKFILE);
		free((caddr_t)new_open, M_NFSDSTATE);
		free((caddr_t)new_deleg, M_NFSDSTATE);
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (NFSERR_EXPIRED);
	}

	if (new_stp->ls_flags & NFSLCK_RECLAIM)
		nfsrv_markstable(clp);

	/*
	 * Get the structure for the underlying file.
	 */
	if (getfhret)
		error = getfhret;
	else
		error = nfsrv_getlockfile(new_stp->ls_flags, &new_lfp, &lfp,
		    NULL, 0);
	if (new_lfp)
		FREE((caddr_t)new_lfp, M_NFSDLOCKFILE);
	if (error) {
		NFSUNLOCKSTATE();
		printf("Nfsd openctrl unexpected getlockfile err=%d\n",
		    error);
		free((caddr_t)new_open, M_NFSDSTATE);
		free((caddr_t)new_deleg, M_NFSDSTATE);
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (error);
	}

	/*
	 * Search for a conflicting open/share.
	 */
	if (new_stp->ls_flags & NFSLCK_DELEGCUR) {
	    /*
	     * For Delegate_Cur, search for the matching Delegation,
	     * which indicates no conflict.
	     * An old delegation should have been recovered by the
	     * client doing a Claim_DELEGATE_Prev, so I won't let
	     * it match and return NFSERR_EXPIRED. Should I let it
	     * match?
	     */
	    LIST_FOREACH(stp, &lfp->lf_deleg, ls_file) {
		if (!(stp->ls_flags & NFSLCK_OLDDELEG) &&
		    stateidp->seqid == stp->ls_stateid.seqid &&
		    !NFSBCMP(stateidp->other, stp->ls_stateid.other,
			NFSX_STATEIDOTHER))
			break;
	    }
	    if (stp == LIST_END(&lfp->lf_deleg) ||
		((new_stp->ls_flags & NFSLCK_WRITEACCESS) &&
		 (stp->ls_flags & NFSLCK_DELEGREAD))) {
		NFSUNLOCKSTATE();
		printf("Nfsd openctrl unexpected expiry\n");
		free((caddr_t)new_open, M_NFSDSTATE);
		free((caddr_t)new_deleg, M_NFSDSTATE);
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (NFSERR_EXPIRED);
	    }

	    /*
	     * Don't issue a Delegation, since one already exists and
	     * delay delegation timeout, as required.
	     */
	    delegate = 0;
	    nfsrv_delaydelegtimeout(stp);
	}

	/*
	 * Check for access/deny bit conflicts. I also check for the
	 * same owner, since the client might not have bothered to check.
	 * Also, note an open for the same file and owner, if found,
	 * which is all we do here for Delegate_Cur, since conflict
	 * checking is already done.
	 */
	LIST_FOREACH(stp, &lfp->lf_open, ls_file) {
		if (ownerstp && stp->ls_openowner == ownerstp)
			openstp = stp;
		if (!(new_stp->ls_flags & NFSLCK_DELEGCUR)) {
		    /*
		     * If another client has the file open, the only
		     * delegation that can be issued is a Read delegation
		     * and only if it is a Read open with Deny none.
		     */
		    if (clp != stp->ls_clp) {
			if ((stp->ls_flags & NFSLCK_SHAREBITS) ==
			    NFSLCK_READACCESS)
			    writedeleg = 0;
			else
			    delegate = 0;
		    }
		    if(((new_stp->ls_flags & NFSLCK_ACCESSBITS) &
		        ((stp->ls_flags>>NFSLCK_SHIFT) & NFSLCK_ACCESSBITS))||
		       ((stp->ls_flags & NFSLCK_ACCESSBITS) &
		        ((new_stp->ls_flags>>NFSLCK_SHIFT)&NFSLCK_ACCESSBITS))){
			ret = nfsrv_clientconflict(stp->ls_clp,&haslock,vp,p);
			if (ret == 1) {
				/*
				 * nfsrv_clientconflict() unlocks state
				 * when it returns non-zero.
				 */
				free((caddr_t)new_open, M_NFSDSTATE);
				free((caddr_t)new_deleg, M_NFSDSTATE);
				openstp = NULL;
				goto tryagain;
			}
			if (ret == 2)
				error = NFSERR_PERM;
			else if (new_stp->ls_flags & NFSLCK_RECLAIM)
				error = NFSERR_RECLAIMCONFLICT;
			else
				error = NFSERR_SHAREDENIED;
			if (ret == 0)
				NFSUNLOCKSTATE();
			if (haslock) {
				NFSLOCKV4ROOTMUTEX();
				nfsv4_unlock(&nfsv4rootfs_lock, 1);
				NFSUNLOCKV4ROOTMUTEX();
			}
			free((caddr_t)new_open, M_NFSDSTATE);
			free((caddr_t)new_deleg, M_NFSDSTATE);
			printf("nfsd openctrl unexpected client cnfl\n");
			return (error);
		    }
		}
	}

	/*
	 * Check for a conflicting delegation. If one is found, call
	 * nfsrv_delegconflict() to handle it. If the v4root lock hasn't
	 * been set yet, it will get the lock. Otherwise, it will recall
	 * the delegation. Then, we try try again...
	 * (If NFSLCK_DELEGCUR is set, it has a delegation, so there
	 *  isn't a conflict.)
	 * I currently believe the conflict algorithm to be:
	 * For Open with Read Access and Deny None
	 * - there is a conflict iff a different client has a write delegation
	 * For Open with other Write Access or any Deny except None
	 * - there is a conflict if a different client has any delegation
	 * - there is a conflict if the same client has a read delegation
	 *   (The current concensus is that this last case should be
	 *    considered a conflict since the client with a read delegation
	 *    could have done an Open with ReadAccess and WriteDeny
	 *    locally and then not have checked for the WriteDeny.)
	 */
	if (!(new_stp->ls_flags & (NFSLCK_DELEGPREV | NFSLCK_DELEGCUR))) {
	    stp = LIST_FIRST(&lfp->lf_deleg);
	    while (stp != LIST_END(&lfp->lf_deleg)) {
		nstp = LIST_NEXT(stp, ls_file);
		if (stp->ls_clp != clp && (stp->ls_flags & NFSLCK_DELEGREAD))
			writedeleg = 0;
		else
			delegate = 0;
		if ((readonly && stp->ls_clp != clp &&
		       (stp->ls_flags & NFSLCK_DELEGWRITE)) ||
		    (!readonly && (stp->ls_clp != clp ||
		         (stp->ls_flags & NFSLCK_DELEGREAD)))) {
		    if (new_stp->ls_flags & NFSLCK_RECLAIM) {
			delegate = 2;
		    } else {
			ret = nfsrv_delegconflict(stp, &haslock, p, vp);
			if (ret) {
			    /*
			     * nfsrv_delegconflict() unlocks state
			     * when it returns non-zero.
			     */
			    printf("Nfsd openctrl unexpected deleg cnfl\n");
			    free((caddr_t)new_open, M_NFSDSTATE);
			    free((caddr_t)new_deleg, M_NFSDSTATE);
			    if (ret == -1) {
				openstp = NULL;
				goto tryagain;
			    }
			    return (ret);
			}
		    }
		}
		stp = nstp;
	    }
	}

	/*
	 * We only get here if there was no open that conflicted.
	 * If an open for the owner exists, or in the access/deny bits.
	 * Otherwise it is a new open. If the open_owner hasn't been
	 * confirmed, replace the open with the new one needing confirmation,
	 * otherwise add the open.
	 */
	if (new_stp->ls_flags & NFSLCK_DELEGPREV) {
	    /*
	     * Handle NFSLCK_DELEGPREV by searching the old delegations for
	     * a match. If found, just move the old delegation to the current
	     * delegation list and issue open. If not found, return
	     * NFSERR_EXPIRED.
	     */
	    LIST_FOREACH(stp, &clp->lc_olddeleg, ls_list) {
		if (stp->ls_lfp == lfp) {
		    /* Found it */
		    if (stp->ls_clp != clp)
			panic("olddeleg clp");
		    LIST_REMOVE(stp, ls_list);
		    LIST_REMOVE(stp, ls_hash);
		    stp->ls_flags &= ~NFSLCK_OLDDELEG;
		    stp->ls_stateid.seqid = delegstateidp->seqid = 0;
		    stp->ls_stateid.other[0] = delegstateidp->other[0] =
			clp->lc_clientid.lval[0];
		    stp->ls_stateid.other[1] = delegstateidp->other[1] =
			clp->lc_clientid.lval[1];
		    stp->ls_stateid.other[2] = delegstateidp->other[2] =
			nfsrv_nextstateindex(clp);
		    stp->ls_compref = nd->nd_compref;
		    LIST_INSERT_HEAD(&clp->lc_deleg, stp, ls_list);
		    LIST_INSERT_HEAD(NFSSTATEHASH(clp,
			stp->ls_stateid), stp, ls_hash);
		    if (stp->ls_flags & NFSLCK_DELEGWRITE)
			*rflagsp |= NFSV4OPEN_WRITEDELEGATE;
		    else
			*rflagsp |= NFSV4OPEN_READDELEGATE;
		    clp->lc_delegtime = NFSD_MONOSEC +
			nfsrv_lease + NFSRV_LEASEDELTA;

		    /*
		     * Now, do the associated open.
		     */
		    new_open->ls_stateid.seqid = 0;
		    new_open->ls_stateid.other[0] = clp->lc_clientid.lval[0];
		    new_open->ls_stateid.other[1] = clp->lc_clientid.lval[1];
		    new_open->ls_stateid.other[2] = nfsrv_nextstateindex(clp);
		    new_open->ls_flags = (new_stp->ls_flags&NFSLCK_DENYBITS)|
			NFSLCK_OPEN;
		    if (stp->ls_flags & NFSLCK_DELEGWRITE)
			new_open->ls_flags |= (NFSLCK_READACCESS |
			    NFSLCK_WRITEACCESS);
		    else
			new_open->ls_flags |= NFSLCK_READACCESS;
		    new_open->ls_uid = new_stp->ls_uid;
		    new_open->ls_lfp = lfp;
		    new_open->ls_clp = clp;
		    LIST_INIT(&new_open->ls_open);
		    LIST_INSERT_HEAD(&lfp->lf_open, new_open, ls_file);
		    LIST_INSERT_HEAD(NFSSTATEHASH(clp, new_open->ls_stateid),
			new_open, ls_hash);
		    /*
		     * and handle the open owner
		     */
		    if (ownerstp) {
			new_open->ls_openowner = ownerstp;
			LIST_INSERT_HEAD(&ownerstp->ls_open,new_open,ls_list);
		    } else {
			new_open->ls_openowner = new_stp;
			new_stp->ls_flags = 0;
			nfsrvd_refcache(new_stp->ls_op);
			new_stp->ls_noopens = 0;
			LIST_INIT(&new_stp->ls_open);
			LIST_INSERT_HEAD(&new_stp->ls_open, new_open, ls_list);
			LIST_INSERT_HEAD(&clp->lc_open, new_stp, ls_list);
			*new_stpp = NULL;
			newnfsstats.srvopenowners++;
			nfsrv_openpluslock++;
		    }
		    openstp = new_open;
		    new_open = NULL;
		    newnfsstats.srvopens++;
		    nfsrv_openpluslock++;
		    break;
		}
	    }
	    if (stp == LIST_END(&clp->lc_olddeleg))
		error = NFSERR_EXPIRED;
	} else if (new_stp->ls_flags & (NFSLCK_DELEGREAD | NFSLCK_DELEGWRITE)) {
	    /*
	     * Scan to see that no delegation for this client and file
	     * doesn't already exist.
	     * There also shouldn't yet be an Open for this file and
	     * openowner.
	     */
	    LIST_FOREACH(stp, &lfp->lf_deleg, ls_file) {
		if (stp->ls_clp == clp)
		    break;
	    }
	    if (stp == LIST_END(&lfp->lf_deleg) && openstp == NULL) {
		/*
		 * This is the Claim_Previous case with a delegation
		 * type != Delegate_None.
		 */
		/*
		 * First, add the delegation. (Although we must issue the
		 * delegation, we can also ask for an immediate return.)
		 */
		new_deleg->ls_stateid.seqid = delegstateidp->seqid = 0;
		new_deleg->ls_stateid.other[0] = delegstateidp->other[0] =
		    clp->lc_clientid.lval[0];
		new_deleg->ls_stateid.other[1] = delegstateidp->other[1] =
		    clp->lc_clientid.lval[1];
		new_deleg->ls_stateid.other[2] = delegstateidp->other[2] =
		    nfsrv_nextstateindex(clp);
		if (new_stp->ls_flags & NFSLCK_DELEGWRITE) {
		    new_deleg->ls_flags = (NFSLCK_DELEGWRITE |
			NFSLCK_READACCESS | NFSLCK_WRITEACCESS);
		    *rflagsp |= NFSV4OPEN_WRITEDELEGATE;
		} else {
		    new_deleg->ls_flags = (NFSLCK_DELEGREAD |
			NFSLCK_READACCESS);
		    *rflagsp |= NFSV4OPEN_READDELEGATE;
		}
		new_deleg->ls_uid = new_stp->ls_uid;
		new_deleg->ls_lfp = lfp;
		new_deleg->ls_clp = clp;
		new_deleg->ls_filerev = filerev;
		new_deleg->ls_compref = nd->nd_compref;
		LIST_INSERT_HEAD(&lfp->lf_deleg, new_deleg, ls_file);
		LIST_INSERT_HEAD(NFSSTATEHASH(clp,
		    new_deleg->ls_stateid), new_deleg, ls_hash);
		LIST_INSERT_HEAD(&clp->lc_deleg, new_deleg, ls_list);
		new_deleg = NULL;
		if (delegate == 2 || nfsrv_issuedelegs == 0 ||
		    (clp->lc_flags & (LCL_CALLBACKSON | LCL_CBDOWN)) !=
		     LCL_CALLBACKSON ||
		    NFSRV_V4DELEGLIMIT(nfsrv_delegatecnt) ||
		    !NFSVNO_DELEGOK(vp))
		    *rflagsp |= NFSV4OPEN_RECALL;
		newnfsstats.srvdelegates++;
		nfsrv_openpluslock++;
		nfsrv_delegatecnt++;

		/*
		 * Now, do the associated open.
		 */
		new_open->ls_stateid.seqid = 0;
		new_open->ls_stateid.other[0] = clp->lc_clientid.lval[0];
		new_open->ls_stateid.other[1] = clp->lc_clientid.lval[1];
		new_open->ls_stateid.other[2] = nfsrv_nextstateindex(clp);
		new_open->ls_flags = (new_stp->ls_flags & NFSLCK_DENYBITS) |
		    NFSLCK_OPEN;
		if (new_stp->ls_flags & NFSLCK_DELEGWRITE)
			new_open->ls_flags |= (NFSLCK_READACCESS |
			    NFSLCK_WRITEACCESS);
		else
			new_open->ls_flags |= NFSLCK_READACCESS;
		new_open->ls_uid = new_stp->ls_uid;
		new_open->ls_lfp = lfp;
		new_open->ls_clp = clp;
		LIST_INIT(&new_open->ls_open);
		LIST_INSERT_HEAD(&lfp->lf_open, new_open, ls_file);
		LIST_INSERT_HEAD(NFSSTATEHASH(clp, new_open->ls_stateid),
		   new_open, ls_hash);
		/*
		 * and handle the open owner
		 */
		if (ownerstp) {
		    new_open->ls_openowner = ownerstp;
		    LIST_INSERT_HEAD(&ownerstp->ls_open, new_open, ls_list);
		} else {
		    new_open->ls_openowner = new_stp;
		    new_stp->ls_flags = 0;
		    nfsrvd_refcache(new_stp->ls_op);
		    new_stp->ls_noopens = 0;
		    LIST_INIT(&new_stp->ls_open);
		    LIST_INSERT_HEAD(&new_stp->ls_open, new_open, ls_list);
		    LIST_INSERT_HEAD(&clp->lc_open, new_stp, ls_list);
		    *new_stpp = NULL;
		    newnfsstats.srvopenowners++;
		    nfsrv_openpluslock++;
		}
		openstp = new_open;
		new_open = NULL;
		newnfsstats.srvopens++;
		nfsrv_openpluslock++;
	    } else {
		error = NFSERR_RECLAIMCONFLICT;
	    }
	} else if (ownerstp) {
		if (ownerstp->ls_flags & NFSLCK_NEEDSCONFIRM) {
		    /* Replace the open */
		    if (ownerstp->ls_op)
			nfsrvd_derefcache(ownerstp->ls_op);
		    ownerstp->ls_op = new_stp->ls_op;
		    nfsrvd_refcache(ownerstp->ls_op);
		    ownerstp->ls_seq = new_stp->ls_seq;
		    *rflagsp |= NFSV4OPEN_RESULTCONFIRM;
		    stp = LIST_FIRST(&ownerstp->ls_open);
		    stp->ls_flags = (new_stp->ls_flags & NFSLCK_SHAREBITS) |
			NFSLCK_OPEN;
		    stp->ls_stateid.seqid = 0;
		    stp->ls_uid = new_stp->ls_uid;
		    if (lfp != stp->ls_lfp) {
			LIST_REMOVE(stp, ls_file);
			LIST_INSERT_HEAD(&lfp->lf_open, stp, ls_file);
			stp->ls_lfp = lfp;
		    }
		    openstp = stp;
		} else if (openstp) {
		    openstp->ls_flags |= (new_stp->ls_flags & NFSLCK_SHAREBITS);
		    openstp->ls_stateid.seqid++;

		    /*
		     * This is where we can choose to issue a delegation.
		     */
		    if (delegate && nfsrv_issuedelegs &&
			writedeleg && !NFSVNO_EXRDONLY(exp) &&
			(nfsrv_writedelegifpos || !readonly) &&
			(clp->lc_flags & (LCL_CALLBACKSON | LCL_CBDOWN)) ==
			 LCL_CALLBACKSON &&
			!NFSRV_V4DELEGLIMIT(nfsrv_delegatecnt) &&
			NFSVNO_DELEGOK(vp)) {
			new_deleg->ls_stateid.seqid = delegstateidp->seqid = 0;
			new_deleg->ls_stateid.other[0] = delegstateidp->other[0]
			    = clp->lc_clientid.lval[0];
			new_deleg->ls_stateid.other[1] = delegstateidp->other[1]
			    = clp->lc_clientid.lval[1];
			new_deleg->ls_stateid.other[2] = delegstateidp->other[2]
			    = nfsrv_nextstateindex(clp);
			new_deleg->ls_flags = (NFSLCK_DELEGWRITE |
			    NFSLCK_READACCESS | NFSLCK_WRITEACCESS);
			*rflagsp |= NFSV4OPEN_WRITEDELEGATE;
			new_deleg->ls_uid = new_stp->ls_uid;
			new_deleg->ls_lfp = lfp;
			new_deleg->ls_clp = clp;
			new_deleg->ls_filerev = filerev;
			new_deleg->ls_compref = nd->nd_compref;
			LIST_INSERT_HEAD(&lfp->lf_deleg, new_deleg, ls_file);
			LIST_INSERT_HEAD(NFSSTATEHASH(clp,
			    new_deleg->ls_stateid), new_deleg, ls_hash);
			LIST_INSERT_HEAD(&clp->lc_deleg, new_deleg, ls_list);
			new_deleg = NULL;
			newnfsstats.srvdelegates++;
			nfsrv_openpluslock++;
			nfsrv_delegatecnt++;
		    }
		} else {
		    new_open->ls_stateid.seqid = 0;
		    new_open->ls_stateid.other[0] = clp->lc_clientid.lval[0];
		    new_open->ls_stateid.other[1] = clp->lc_clientid.lval[1];
		    new_open->ls_stateid.other[2] = nfsrv_nextstateindex(clp);
		    new_open->ls_flags = (new_stp->ls_flags & NFSLCK_SHAREBITS)|
			NFSLCK_OPEN;
		    new_open->ls_uid = new_stp->ls_uid;
		    new_open->ls_openowner = ownerstp;
		    new_open->ls_lfp = lfp;
		    new_open->ls_clp = clp;
		    LIST_INIT(&new_open->ls_open);
		    LIST_INSERT_HEAD(&lfp->lf_open, new_open, ls_file);
		    LIST_INSERT_HEAD(&ownerstp->ls_open, new_open, ls_list);
		    LIST_INSERT_HEAD(NFSSTATEHASH(clp, new_open->ls_stateid),
			new_open, ls_hash);
		    openstp = new_open;
		    new_open = NULL;
		    newnfsstats.srvopens++;
		    nfsrv_openpluslock++;

		    /*
		     * This is where we can choose to issue a delegation.
		     */
		    if (delegate && nfsrv_issuedelegs &&
			(writedeleg || readonly) &&
			(clp->lc_flags & (LCL_CALLBACKSON | LCL_CBDOWN)) ==
			 LCL_CALLBACKSON &&
			!NFSRV_V4DELEGLIMIT(nfsrv_delegatecnt) &&
			NFSVNO_DELEGOK(vp)) {
			new_deleg->ls_stateid.seqid = delegstateidp->seqid = 0;
			new_deleg->ls_stateid.other[0] = delegstateidp->other[0]
			    = clp->lc_clientid.lval[0];
			new_deleg->ls_stateid.other[1] = delegstateidp->other[1]
			    = clp->lc_clientid.lval[1];
			new_deleg->ls_stateid.other[2] = delegstateidp->other[2]
			    = nfsrv_nextstateindex(clp);
			if (writedeleg && !NFSVNO_EXRDONLY(exp) &&
			    (nfsrv_writedelegifpos || !readonly)) {
			    new_deleg->ls_flags = (NFSLCK_DELEGWRITE |
				NFSLCK_READACCESS | NFSLCK_WRITEACCESS);
			    *rflagsp |= NFSV4OPEN_WRITEDELEGATE;
			} else {
			    new_deleg->ls_flags = (NFSLCK_DELEGREAD |
				NFSLCK_READACCESS);
			    *rflagsp |= NFSV4OPEN_READDELEGATE;
			}
			new_deleg->ls_uid = new_stp->ls_uid;
			new_deleg->ls_lfp = lfp;
			new_deleg->ls_clp = clp;
			new_deleg->ls_filerev = filerev;
			new_deleg->ls_compref = nd->nd_compref;
			LIST_INSERT_HEAD(&lfp->lf_deleg, new_deleg, ls_file);
			LIST_INSERT_HEAD(NFSSTATEHASH(clp,
			    new_deleg->ls_stateid), new_deleg, ls_hash);
			LIST_INSERT_HEAD(&clp->lc_deleg, new_deleg, ls_list);
			new_deleg = NULL;
			newnfsstats.srvdelegates++;
			nfsrv_openpluslock++;
			nfsrv_delegatecnt++;
		    }
		}
	} else {
		/*
		 * New owner case. Start the open_owner sequence with a
		 * Needs confirmation (unless a reclaim) and hang the
		 * new open off it.
		 */
		new_open->ls_stateid.seqid = 0;
		new_open->ls_stateid.other[0] = clp->lc_clientid.lval[0];
		new_open->ls_stateid.other[1] = clp->lc_clientid.lval[1];
		new_open->ls_stateid.other[2] = nfsrv_nextstateindex(clp);
		new_open->ls_flags = (new_stp->ls_flags & NFSLCK_SHAREBITS) |
		    NFSLCK_OPEN;
		new_open->ls_uid = new_stp->ls_uid;
		LIST_INIT(&new_open->ls_open);
		new_open->ls_openowner = new_stp;
		new_open->ls_lfp = lfp;
		new_open->ls_clp = clp;
		LIST_INSERT_HEAD(&lfp->lf_open, new_open, ls_file);
		if (new_stp->ls_flags & NFSLCK_RECLAIM) {
			new_stp->ls_flags = 0;
		} else {
			*rflagsp |= NFSV4OPEN_RESULTCONFIRM;
			new_stp->ls_flags = NFSLCK_NEEDSCONFIRM;
		}
		nfsrvd_refcache(new_stp->ls_op);
		new_stp->ls_noopens = 0;
		LIST_INIT(&new_stp->ls_open);
		LIST_INSERT_HEAD(&new_stp->ls_open, new_open, ls_list);
		LIST_INSERT_HEAD(&clp->lc_open, new_stp, ls_list);
		LIST_INSERT_HEAD(NFSSTATEHASH(clp, new_open->ls_stateid),
		    new_open, ls_hash);
		openstp = new_open;
		new_open = NULL;
		*new_stpp = NULL;
		newnfsstats.srvopens++;
		nfsrv_openpluslock++;
		newnfsstats.srvopenowners++;
		nfsrv_openpluslock++;
	}
	if (!error) {
		stateidp->seqid = openstp->ls_stateid.seqid;
		stateidp->other[0] = openstp->ls_stateid.other[0];
		stateidp->other[1] = openstp->ls_stateid.other[1];
		stateidp->other[2] = openstp->ls_stateid.other[2];
	}
	NFSUNLOCKSTATE();
	if (haslock) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
	}
	if (new_open)
		FREE((caddr_t)new_open, M_NFSDSTATE);
	if (new_deleg)
		FREE((caddr_t)new_deleg, M_NFSDSTATE);
	return (error);
}

/*
 * Open update. Does the confirm, downgrade and close.
 */
APPLESTATIC int
nfsrv_openupdate(vnode_t vp, struct nfsstate *new_stp, nfsquad_t clientid,
    nfsv4stateid_t *stateidp, struct nfsrv_descript *nd, NFSPROC_T *p)
{
	struct nfsstate *stp, *ownerstp;
	struct nfsclient *clp;
	struct nfslockfile *lfp;
	u_int32_t bits;
	int error, gotstate = 0, len = 0;
	u_char client[NFSV4_OPAQUELIMIT];

	/*
	 * Check for restart conditions (client and server).
	 */
	error = nfsrv_checkrestart(clientid, new_stp->ls_flags,
	    &new_stp->ls_stateid, 0);
	if (error)
		return (error);

	NFSLOCKSTATE();
	/*
	 * Get the open structure via clientid and stateid.
	 */
	error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
	    (nfsquad_t)((u_quad_t)0), NULL, p);
	if (!error)
		error = nfsrv_getstate(clp, &new_stp->ls_stateid,
		    new_stp->ls_flags, &stp);

	/*
	 * Sanity check the open.
	 */
	if (!error && (!(stp->ls_flags & NFSLCK_OPEN) ||
		(!(new_stp->ls_flags & NFSLCK_CONFIRM) &&
		 (stp->ls_openowner->ls_flags & NFSLCK_NEEDSCONFIRM)) ||
		((new_stp->ls_flags & NFSLCK_CONFIRM) &&
		 (!(stp->ls_openowner->ls_flags & NFSLCK_NEEDSCONFIRM)))))
		error = NFSERR_BADSTATEID;

	if (!error)
		error = nfsrv_checkseqid(nd, new_stp->ls_seq,
		    stp->ls_openowner, new_stp->ls_op);
	if (!error && stp->ls_stateid.seqid != new_stp->ls_stateid.seqid &&
	    !(new_stp->ls_flags & NFSLCK_CONFIRM))
		error = NFSERR_OLDSTATEID;
	if (!error && vnode_vtype(vp) != VREG) {
		if (vnode_vtype(vp) == VDIR)
			error = NFSERR_ISDIR;
		else
			error = NFSERR_INVAL;
	}

	if (error) {
		/*
		 * If a client tries to confirm an Open with a bad
		 * seqid# and there are no byte range locks or other Opens
		 * on the openowner, just throw it away, so the next use of the
		 * openowner will start a fresh seq#.
		 */
		if (error == NFSERR_BADSEQID &&
		    (new_stp->ls_flags & NFSLCK_CONFIRM) &&
		    nfsrv_nootherstate(stp))
			nfsrv_freeopenowner(stp->ls_openowner, 0, p);
		NFSUNLOCKSTATE();
		return (error);
	}

	/*
	 * Set the return stateid.
	 */
	stateidp->seqid = stp->ls_stateid.seqid + 1;
	stateidp->other[0] = stp->ls_stateid.other[0];
	stateidp->other[1] = stp->ls_stateid.other[1];
	stateidp->other[2] = stp->ls_stateid.other[2];
	/*
	 * Now, handle the three cases.
	 */
	if (new_stp->ls_flags & NFSLCK_CONFIRM) {
		/*
		 * If the open doesn't need confirmation, it seems to me that
		 * there is a client error, but I'll just log it and keep going?
		 */
		if (!(stp->ls_openowner->ls_flags & NFSLCK_NEEDSCONFIRM))
			printf("Nfsv4d: stray open confirm\n");
		stp->ls_openowner->ls_flags = 0;
		stp->ls_stateid.seqid++;
		if (!(clp->lc_flags & LCL_STAMPEDSTABLE)) {
			clp->lc_flags |= LCL_STAMPEDSTABLE;
			len = clp->lc_idlen;
			NFSBCOPY(clp->lc_id, client, len);
			gotstate = 1;
		}
		NFSUNLOCKSTATE();
	} else if (new_stp->ls_flags & NFSLCK_CLOSE) {
		ownerstp = stp->ls_openowner;
		lfp = stp->ls_lfp;
		if (nfsrv_dolocallocks != 0 && !LIST_EMPTY(&stp->ls_open)) {
			/* Get the lf lock */
			nfsrv_locklf(lfp);
			NFSUNLOCKSTATE();
			if (nfsrv_freeopen(stp, vp, 1, p) == 0) {
				NFSLOCKSTATE();
				nfsrv_unlocklf(lfp);
				NFSUNLOCKSTATE();
			}
		} else {
			(void) nfsrv_freeopen(stp, NULL, 0, p);
			NFSUNLOCKSTATE();
		}
	} else {
		/*
		 * Update the share bits, making sure that the new set are a
		 * subset of the old ones.
		 */
		bits = (new_stp->ls_flags & NFSLCK_SHAREBITS);
		if (~(stp->ls_flags) & bits) {
			NFSUNLOCKSTATE();
			return (NFSERR_INVAL);
		}
		stp->ls_flags = (bits | NFSLCK_OPEN);
		stp->ls_stateid.seqid++;
		NFSUNLOCKSTATE();
	}

	/*
	 * If the client just confirmed its first open, write a timestamp
	 * to the stable storage file.
	 */
	if (gotstate != 0) {
		nfsrv_writestable(client, len, NFSNST_NEWSTATE, p);
		nfsrv_backupstable();
	}
	return (error);
}

/*
 * Delegation update. Does the purge and return.
 */
APPLESTATIC int
nfsrv_delegupdate(nfsquad_t clientid, nfsv4stateid_t *stateidp,
    vnode_t vp, int op, struct ucred *cred, NFSPROC_T *p)
{
	struct nfsstate *stp;
	struct nfsclient *clp;
	int error;
	fhandle_t fh;

	/*
	 * Do a sanity check against the file handle for DelegReturn.
	 */
	if (vp) {
		error = nfsvno_getfh(vp, &fh, p);
		if (error)
			return (error);
	}
	/*
	 * Check for restart conditions (client and server).
	 */
	if (op == NFSV4OP_DELEGRETURN)
		error = nfsrv_checkrestart(clientid, NFSLCK_DELEGRETURN,
			stateidp, 0);
	else
		error = nfsrv_checkrestart(clientid, NFSLCK_DELEGPURGE,
			stateidp, 0);

	NFSLOCKSTATE();
	/*
	 * Get the open structure via clientid and stateid.
	 */
	if (!error)
	    error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
		(nfsquad_t)((u_quad_t)0), NULL, p);
	if (error) {
		if (error == NFSERR_CBPATHDOWN)
			error = 0;
		if (error == NFSERR_STALECLIENTID && op == NFSV4OP_DELEGRETURN)
			error = NFSERR_STALESTATEID;
	}
	if (!error && op == NFSV4OP_DELEGRETURN) {
	    error = nfsrv_getstate(clp, stateidp, NFSLCK_DELEGRETURN, &stp);
	    if (!error && stp->ls_stateid.seqid != stateidp->seqid)
		error = NFSERR_OLDSTATEID;
	}
	/*
	 * NFSERR_EXPIRED means that the state has gone away,
	 * so Delegations have been purged. Just return ok.
	 */
	if (error == NFSERR_EXPIRED && op == NFSV4OP_DELEGPURGE) {
		NFSUNLOCKSTATE();
		return (0);
	}
	if (error) {
		NFSUNLOCKSTATE();
		return (error);
	}

	if (op == NFSV4OP_DELEGRETURN) {
		if (NFSBCMP((caddr_t)&fh, (caddr_t)&stp->ls_lfp->lf_fh,
		    sizeof (fhandle_t))) {
			NFSUNLOCKSTATE();
			return (NFSERR_BADSTATEID);
		}
		nfsrv_freedeleg(stp);
	} else {
		nfsrv_freedeleglist(&clp->lc_olddeleg);
	}
	NFSUNLOCKSTATE();
	return (0);
}

/*
 * Release lock owner.
 */
APPLESTATIC int
nfsrv_releaselckown(struct nfsstate *new_stp, nfsquad_t clientid,
    NFSPROC_T *p)
{
	struct nfsstate *stp, *nstp, *openstp, *ownstp;
	struct nfsclient *clp;
	int error;

	/*
	 * Check for restart conditions (client and server).
	 */
	error = nfsrv_checkrestart(clientid, new_stp->ls_flags,
	    &new_stp->ls_stateid, 0);
	if (error)
		return (error);

	NFSLOCKSTATE();
	/*
	 * Get the lock owner by name.
	 */
	error = nfsrv_getclient(clientid, CLOPS_RENEW, &clp,
	    (nfsquad_t)((u_quad_t)0), NULL, p);
	if (error) {
		NFSUNLOCKSTATE();
		return (error);
	}
	LIST_FOREACH(ownstp, &clp->lc_open, ls_list) {
	    LIST_FOREACH(openstp, &ownstp->ls_open, ls_list) {
		stp = LIST_FIRST(&openstp->ls_open);
		while (stp != LIST_END(&openstp->ls_open)) {
		    nstp = LIST_NEXT(stp, ls_list);
		    /*
		     * If the owner matches, check for locks and
		     * then free or return an error.
		     */
		    if (stp->ls_ownerlen == new_stp->ls_ownerlen &&
			!NFSBCMP(stp->ls_owner, new_stp->ls_owner,
			 stp->ls_ownerlen)){
			if (LIST_EMPTY(&stp->ls_lock)) {
			    nfsrv_freelockowner(stp, NULL, 0, p);
			} else {
			    NFSUNLOCKSTATE();
			    return (NFSERR_LOCKSHELD);
			}
		    }
		    stp = nstp;
		}
	    }
	}
	NFSUNLOCKSTATE();
	return (0);
}

/*
 * Get the file handle for a lock structure.
 */
static int
nfsrv_getlockfh(vnode_t vp, u_short flags,
    struct nfslockfile **new_lfpp, fhandle_t *nfhp, NFSPROC_T *p)
{
	fhandle_t *fhp = NULL;
	struct nfslockfile *new_lfp;
	int error;

	/*
	 * For lock, use the new nfslock structure, otherwise just
	 * a fhandle_t on the stack.
	 */
	if (flags & NFSLCK_OPEN) {
		new_lfp = *new_lfpp;
		fhp = &new_lfp->lf_fh;
	} else if (nfhp) {
		fhp = nfhp;
	} else {
		panic("nfsrv_getlockfh");
	}
	error = nfsvno_getfh(vp, fhp, p);
	return (error);
}

/*
 * Get an nfs lock structure. Allocate one, as required, and return a
 * pointer to it.
 * Returns an NFSERR_xxx upon failure or -1 to indicate no current lock.
 */
static int
nfsrv_getlockfile(u_short flags, struct nfslockfile **new_lfpp,
    struct nfslockfile **lfpp, fhandle_t *nfhp, int lockit)
{
	struct nfslockfile *lfp;
	fhandle_t *fhp = NULL, *tfhp;
	struct nfslockhashhead *hp;
	struct nfslockfile *new_lfp = NULL;

	/*
	 * For lock, use the new nfslock structure, otherwise just
	 * a fhandle_t on the stack.
	 */
	if (flags & NFSLCK_OPEN) {
		new_lfp = *new_lfpp;
		fhp = &new_lfp->lf_fh;
	} else if (nfhp) {
		fhp = nfhp;
	} else {
		panic("nfsrv_getlockfile");
	}

	hp = NFSLOCKHASH(fhp);
	LIST_FOREACH(lfp, hp, lf_hash) {
		tfhp = &lfp->lf_fh;
		if (NFSVNO_CMPFH(fhp, tfhp)) {
			if (lockit)
				nfsrv_locklf(lfp);
			*lfpp = lfp;
			return (0);
		}
	}
	if (!(flags & NFSLCK_OPEN))
		return (-1);

	/*
	 * No match, so chain the new one into the list.
	 */
	LIST_INIT(&new_lfp->lf_open);
	LIST_INIT(&new_lfp->lf_lock);
	LIST_INIT(&new_lfp->lf_deleg);
	LIST_INIT(&new_lfp->lf_locallock);
	LIST_INIT(&new_lfp->lf_rollback);
	new_lfp->lf_locallock_lck.nfslock_usecnt = 0;
	new_lfp->lf_locallock_lck.nfslock_lock = 0;
	new_lfp->lf_usecount = 0;
	LIST_INSERT_HEAD(hp, new_lfp, lf_hash);
	*lfpp = new_lfp;
	*new_lfpp = NULL;
	return (0);
}

/*
 * This function adds a nfslock lock structure to the list for the associated
 * nfsstate and nfslockfile structures. It will be inserted after the
 * entry pointed at by insert_lop.
 */
static void
nfsrv_insertlock(struct nfslock *new_lop, struct nfslock *insert_lop,
    struct nfsstate *stp, struct nfslockfile *lfp)
{
	struct nfslock *lop, *nlop;

	new_lop->lo_stp = stp;
	new_lop->lo_lfp = lfp;

	if (stp != NULL) {
		/* Insert in increasing lo_first order */
		lop = LIST_FIRST(&lfp->lf_lock);
		if (lop == LIST_END(&lfp->lf_lock) ||
		    new_lop->lo_first <= lop->lo_first) {
			LIST_INSERT_HEAD(&lfp->lf_lock, new_lop, lo_lckfile);
		} else {
			nlop = LIST_NEXT(lop, lo_lckfile);
			while (nlop != LIST_END(&lfp->lf_lock) &&
			       nlop->lo_first < new_lop->lo_first) {
				lop = nlop;
				nlop = LIST_NEXT(lop, lo_lckfile);
			}
			LIST_INSERT_AFTER(lop, new_lop, lo_lckfile);
		}
	} else {
		new_lop->lo_lckfile.le_prev = NULL;	/* list not used */
	}

	/*
	 * Insert after insert_lop, which is overloaded as stp or lfp for
	 * an empty list.
	 */
	if (stp == NULL && (struct nfslockfile *)insert_lop == lfp)
		LIST_INSERT_HEAD(&lfp->lf_locallock, new_lop, lo_lckowner);
	else if ((struct nfsstate *)insert_lop == stp)
		LIST_INSERT_HEAD(&stp->ls_lock, new_lop, lo_lckowner);
	else
		LIST_INSERT_AFTER(insert_lop, new_lop, lo_lckowner);
	if (stp != NULL) {
		newnfsstats.srvlocks++;
		nfsrv_openpluslock++;
	}
}

/*
 * This function updates the locking for a lock owner and given file. It
 * maintains a list of lock ranges ordered on increasing file offset that
 * are NFSLCK_READ or NFSLCK_WRITE and non-overlapping (aka POSIX style).
 * It always adds new_lop to the list and sometimes uses the one pointed
 * at by other_lopp.
 */
static void
nfsrv_updatelock(struct nfsstate *stp, struct nfslock **new_lopp,
    struct nfslock **other_lopp, struct nfslockfile *lfp)
{
	struct nfslock *new_lop = *new_lopp;
	struct nfslock *lop, *tlop, *ilop;
	struct nfslock *other_lop = *other_lopp;
	int unlock = 0, myfile = 0;
	u_int64_t tmp;

	/*
	 * Work down the list until the lock is merged.
	 */
	if (new_lop->lo_flags & NFSLCK_UNLOCK)
		unlock = 1;
	if (stp != NULL) {
		ilop = (struct nfslock *)stp;
		lop = LIST_FIRST(&stp->ls_lock);
	} else {
		ilop = (struct nfslock *)lfp;
		lop = LIST_FIRST(&lfp->lf_locallock);
	}
	while (lop != NULL) {
	    /*
	     * Only check locks for this file that aren't before the start of
	     * new lock's range.
	     */
	    if (lop->lo_lfp == lfp) {
	      myfile = 1;
	      if (lop->lo_end >= new_lop->lo_first) {
		if (new_lop->lo_end < lop->lo_first) {
			/*
			 * If the new lock ends before the start of the
			 * current lock's range, no merge, just insert
			 * the new lock.
			 */
			break;
		}
		if (new_lop->lo_flags == lop->lo_flags ||
		    (new_lop->lo_first <= lop->lo_first &&
		     new_lop->lo_end >= lop->lo_end)) {
			/*
			 * This lock can be absorbed by the new lock/unlock.
			 * This happens when it covers the entire range
			 * of the old lock or is contiguous
			 * with the old lock and is of the same type or an
			 * unlock.
			 */
			if (lop->lo_first < new_lop->lo_first)
				new_lop->lo_first = lop->lo_first;
			if (lop->lo_end > new_lop->lo_end)
				new_lop->lo_end = lop->lo_end;
			tlop = lop;
			lop = LIST_NEXT(lop, lo_lckowner);
			nfsrv_freenfslock(tlop);
			continue;
		}

		/*
		 * All these cases are for contiguous locks that are not the
		 * same type, so they can't be merged.
		 */
		if (new_lop->lo_first <= lop->lo_first) {
			/*
			 * This case is where the new lock overlaps with the
			 * first part of the old lock. Move the start of the
			 * old lock to just past the end of the new lock. The
			 * new lock will be inserted in front of the old, since
			 * ilop hasn't been updated. (We are done now.)
			 */
			lop->lo_first = new_lop->lo_end;
			break;
		}
		if (new_lop->lo_end >= lop->lo_end) {
			/*
			 * This case is where the new lock overlaps with the
			 * end of the old lock's range. Move the old lock's
			 * end to just before the new lock's first and insert
			 * the new lock after the old lock.
			 * Might not be done yet, since the new lock could
			 * overlap further locks with higher ranges.
			 */
			lop->lo_end = new_lop->lo_first;
			ilop = lop;
			lop = LIST_NEXT(lop, lo_lckowner);
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
		 * new_lop->lo_flags any longer.
		 */
		tmp = new_lop->lo_first;
		if (other_lop == NULL) {
			if (!unlock)
				panic("nfsd srv update unlock");
			other_lop = new_lop;
			*new_lopp = NULL;
		}
		other_lop->lo_first = new_lop->lo_end;
		other_lop->lo_end = lop->lo_end;
		other_lop->lo_flags = lop->lo_flags;
		other_lop->lo_stp = stp;
		other_lop->lo_lfp = lfp;
		lop->lo_end = tmp;
		nfsrv_insertlock(other_lop, lop, stp, lfp);
		*other_lopp = NULL;
		ilop = lop;
		break;
	      }
	    }
	    ilop = lop;
	    lop = LIST_NEXT(lop, lo_lckowner);
	    if (myfile && (lop == NULL || lop->lo_lfp != lfp))
		break;
	}

	/*
	 * Insert the new lock in the list at the appropriate place.
	 */
	if (!unlock) {
		nfsrv_insertlock(new_lop, ilop, stp, lfp);
		*new_lopp = NULL;
	}
}

/*
 * This function handles sequencing of locks, etc.
 * It returns an error that indicates what the caller should do.
 */
static int
nfsrv_checkseqid(struct nfsrv_descript *nd, u_int32_t seqid,
    struct nfsstate *stp, struct nfsrvcache *op)
{

	if (op != nd->nd_rp)
		panic("nfsrvstate checkseqid");
	if (!(op->rc_flag & RC_INPROG))
		panic("nfsrvstate not inprog");
	if (stp->ls_op && stp->ls_op->rc_refcnt <= 0) {
		printf("refcnt=%d\n", stp->ls_op->rc_refcnt);
		panic("nfsrvstate op refcnt");
	}
	if ((stp->ls_seq + 1) == seqid) {
		if (stp->ls_op)
			nfsrvd_derefcache(stp->ls_op);
		stp->ls_op = op;
		nfsrvd_refcache(op);
		stp->ls_seq = seqid;
		return (0);
	} else if (stp->ls_seq == seqid && stp->ls_op &&
		op->rc_xid == stp->ls_op->rc_xid &&
		op->rc_refcnt == 0 &&
		op->rc_reqlen == stp->ls_op->rc_reqlen &&
		op->rc_cksum == stp->ls_op->rc_cksum) {
		if (stp->ls_op->rc_flag & RC_INPROG)
			return (NFSERR_DONTREPLY);
		nd->nd_rp = stp->ls_op;
		nd->nd_rp->rc_flag |= RC_INPROG;
		nfsrvd_delcache(op);
		return (NFSERR_REPLYFROMCACHE);
	}
	return (NFSERR_BADSEQID);
}

/*
 * Get the client ip address for callbacks. If the strings can't be parsed,
 * just set lc_program to 0 to indicate no callbacks are possible.
 * (For cases where the address can't be parsed or is 0.0.0.0.0.0, set
 *  the address to the client's transport address. This won't be used
 *  for callbacks, but can be printed out by newnfsstats for info.)
 * Return error if the xdr can't be parsed, 0 otherwise.
 */
APPLESTATIC int
nfsrv_getclientipaddr(struct nfsrv_descript *nd, struct nfsclient *clp)
{
	u_int32_t *tl;
	u_char *cp, *cp2;
	int i, j;
	struct sockaddr_in *rad, *sad;
	u_char protocol[5], addr[24];
	int error = 0, cantparse = 0;
	union {
		u_long ival;
		u_char cval[4];
	} ip;
	union {
		u_short sval;
		u_char cval[2];
	} port;

	rad = NFSSOCKADDR(clp->lc_req.nr_nam, struct sockaddr_in *);
	rad->sin_family = AF_INET;
	rad->sin_len = sizeof (struct sockaddr_in);
	rad->sin_addr.s_addr = 0;
	rad->sin_port = 0;
	clp->lc_req.nr_client = NULL;
	clp->lc_req.nr_lock = 0;
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	i = fxdr_unsigned(int, *tl);
	if (i >= 3 && i <= 4) {
		error = nfsrv_mtostr(nd, protocol, i);
		if (error)
			goto nfsmout;
		if (!strcmp(protocol, "tcp")) {
			clp->lc_flags |= LCL_TCPCALLBACK;
			clp->lc_req.nr_sotype = SOCK_STREAM;
			clp->lc_req.nr_soproto = IPPROTO_TCP;
		} else if (!strcmp(protocol, "udp")) {
			clp->lc_req.nr_sotype = SOCK_DGRAM;
			clp->lc_req.nr_soproto = IPPROTO_UDP;
		} else {
			cantparse = 1;
		}
	} else {
		cantparse = 1;
		if (i > 0) {
			error = nfsm_advance(nd, NFSM_RNDUP(i), -1);
			if (error)
				goto nfsmout;
		}
	}
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	i = fxdr_unsigned(int, *tl);
	if (i < 0) {
		error = NFSERR_BADXDR;
		goto nfsmout;
	} else if (i == 0) {
		cantparse = 1;
	} else if (!cantparse && i <= 23 && i >= 11) {
		error = nfsrv_mtostr(nd, addr, i);
		if (error)
			goto nfsmout;

		/*
		 * Parse out the address fields. We expect 6 decimal numbers
		 * separated by '.'s.
		 */
		cp = addr;
		i = 0;
		while (*cp && i < 6) {
			cp2 = cp;
			while (*cp2 && *cp2 != '.')
				cp2++;
			if (*cp2)
				*cp2++ = '\0';
			else if (i != 5) {
				cantparse = 1;
				break;
			}
			j = nfsrv_getipnumber(cp);
			if (j >= 0) {
				if (i < 4)
					ip.cval[3 - i] = j;
				else
					port.cval[5 - i] = j;
			} else {
				cantparse = 1;
				break;
			}
			cp = cp2;
			i++;
		}
		if (!cantparse) {
			if (ip.ival != 0x0) {
				rad->sin_addr.s_addr = htonl(ip.ival);
				rad->sin_port = htons(port.sval);
			} else {
				cantparse = 1;
			}
		}
	} else {
		cantparse = 1;
		if (i > 0) {
			error = nfsm_advance(nd, NFSM_RNDUP(i), -1);
			if (error)
				goto nfsmout;
		}
	}
	if (cantparse) {
		sad = NFSSOCKADDR(nd->nd_nam, struct sockaddr_in *);
		rad->sin_addr.s_addr = sad->sin_addr.s_addr;
		rad->sin_port = 0x0;
		clp->lc_program = 0;
	}
nfsmout:
	return (error);
}

/*
 * Turn a string of up to three decimal digits into a number. Return -1 upon
 * error.
 */
static int
nfsrv_getipnumber(u_char *cp)
{
	int i = 0, j = 0;

	while (*cp) {
		if (j > 2 || *cp < '0' || *cp > '9')
			return (-1);
		i *= 10;
		i += (*cp - '0');
		cp++;
		j++;
	}
	if (i < 256)
		return (i);
	return (-1);
}

/*
 * This function checks for restart conditions.
 */
static int
nfsrv_checkrestart(nfsquad_t clientid, u_int32_t flags,
    nfsv4stateid_t *stateidp, int specialid)
{
	int ret;

	/*
	 * First check for a server restart. Open, LockT, ReleaseLockOwner
	 * and DelegPurge have a clientid, the rest a stateid.
	 */
	if (flags &
	    (NFSLCK_OPEN | NFSLCK_TEST | NFSLCK_RELEASE | NFSLCK_DELEGPURGE)) {
		if (clientid.lval[0] != nfsrvboottime)
			return (NFSERR_STALECLIENTID);
	} else if (stateidp->other[0] != nfsrvboottime &&
		specialid == 0)
		return (NFSERR_STALESTATEID);

	/*
	 * Read, Write, Setattr and LockT can return NFSERR_GRACE and do
	 * not use a lock/open owner seqid#, so the check can be done now.
	 * (The others will be checked, as required, later.)
	 */
	if (!(flags & (NFSLCK_CHECK | NFSLCK_TEST)))
		return (0);

	NFSLOCKSTATE();
	ret = nfsrv_checkgrace(flags);
	NFSUNLOCKSTATE();
	return (ret);
}

/*
 * Check for grace.
 */
static int
nfsrv_checkgrace(u_int32_t flags)
{

	if (nfsrv_stablefirst.nsf_flags & NFSNSF_GRACEOVER) {
		if (flags & NFSLCK_RECLAIM)
			return (NFSERR_NOGRACE);
	} else {
		if (!(flags & NFSLCK_RECLAIM))
			return (NFSERR_GRACE);

		/*
		 * If grace is almost over and we are still getting Reclaims,
		 * extend grace a bit.
		 */
		if ((NFSD_MONOSEC + NFSRV_LEASEDELTA) >
		    nfsrv_stablefirst.nsf_eograce)
			nfsrv_stablefirst.nsf_eograce = NFSD_MONOSEC +
				NFSRV_LEASEDELTA;
	}
	return (0);
}

/*
 * Do a server callback.
 */
static int
nfsrv_docallback(struct nfsclient *clp, int procnum,
    nfsv4stateid_t *stateidp, int trunc, fhandle_t *fhp,
    struct nfsvattr *nap, nfsattrbit_t *attrbitp, NFSPROC_T *p)
{
	mbuf_t m;
	u_int32_t *tl;
	struct nfsrv_descript nfsd, *nd = &nfsd;
	struct ucred *cred;
	int error = 0;
	u_int32_t callback;

	cred = newnfs_getcred();
	NFSLOCKSTATE();	/* mostly for lc_cbref++ */
	if (clp->lc_flags & LCL_NEEDSCONFIRM) {
		NFSUNLOCKSTATE();
		panic("docallb");
	}
	clp->lc_cbref++;

	/*
	 * Fill the callback program# and version into the request
	 * structure for newnfs_connect() to use.
	 */
	clp->lc_req.nr_prog = clp->lc_program;
	clp->lc_req.nr_vers = NFSV4_CBVERS;

	/*
	 * First, fill in some of the fields of nd and cr.
	 */
	nd->nd_flag = ND_NFSV4;
	if (clp->lc_flags & LCL_GSS)
		nd->nd_flag |= ND_KERBV;
	nd->nd_repstat = 0;
	cred->cr_uid = clp->lc_uid;
	cred->cr_gid = clp->lc_gid;
	callback = clp->lc_callback;
	NFSUNLOCKSTATE();
	cred->cr_ngroups = 1;

	/*
	 * Get the first mbuf for the request.
	 */
	MGET(m, M_WAIT, MT_DATA);
	mbuf_setlen(m, 0);
	nd->nd_mreq = nd->nd_mb = m;
	nd->nd_bpos = NFSMTOD(m, caddr_t);
	
	/*
	 * and build the callback request.
	 */
	if (procnum == NFSV4OP_CBGETATTR) {
		nd->nd_procnum = NFSV4PROC_CBCOMPOUND;
		(void) nfsm_strtom(nd, "CB Getattr", 10);
		NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(NFSV4_MINORVERSION);
		*tl++ = txdr_unsigned(callback);
		*tl++ = txdr_unsigned(1);
		*tl = txdr_unsigned(NFSV4OP_CBGETATTR);
		(void) nfsm_fhtom(nd, (u_int8_t *)fhp, NFSX_MYFH, 0);
		(void) nfsrv_putattrbit(nd, attrbitp);
	} else if (procnum == NFSV4OP_CBRECALL) {
		nd->nd_procnum = NFSV4PROC_CBCOMPOUND;
		(void) nfsm_strtom(nd, "CB Recall", 9);
		NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED + NFSX_STATEID);
		*tl++ = txdr_unsigned(NFSV4_MINORVERSION);
		*tl++ = txdr_unsigned(callback);
		*tl++ = txdr_unsigned(1);
		*tl++ = txdr_unsigned(NFSV4OP_CBRECALL);
		*tl++ = txdr_unsigned(stateidp->seqid);
		NFSBCOPY((caddr_t)stateidp->other, (caddr_t)tl,
		    NFSX_STATEIDOTHER);
		tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
		if (trunc)
			*tl = newnfs_true;
		else
			*tl = newnfs_false;
		(void) nfsm_fhtom(nd, (u_int8_t *)fhp, NFSX_MYFH, 0);
	} else {
		nd->nd_procnum = NFSV4PROC_CBNULL;
	}

	/*
	 * Call newnfs_connect(), as required, and then newnfs_request().
	 */
	(void) newnfs_sndlock(&clp->lc_req.nr_lock);
	if (clp->lc_req.nr_client == NULL) {
		if (nd->nd_procnum == NFSV4PROC_CBNULL)
			error = newnfs_connect(NULL, &clp->lc_req, cred,
			    NULL, 1);
		else
			error = newnfs_connect(NULL, &clp->lc_req, cred,
			    NULL, 3);
	}
	newnfs_sndunlock(&clp->lc_req.nr_lock);
	if (!error) {
		error = newnfs_request(nd, NULL, clp, &clp->lc_req, NULL,
		    NULL, cred, clp->lc_program, NFSV4_CBVERS, NULL, 1, NULL);
	}
	NFSFREECRED(cred);

	/*
	 * If error is set here, the Callback path isn't working
	 * properly, so twiddle the appropriate LCL_ flags.
	 * (nd_repstat != 0 indicates the Callback path is working,
	 *  but the callback failed on the client.)
	 */
	if (error) {
		/*
		 * Mark the callback pathway down, which disabled issuing
		 * of delegations and gets Renew to return NFSERR_CBPATHDOWN.
		 */
		NFSLOCKSTATE();
		clp->lc_flags |= LCL_CBDOWN;
		NFSUNLOCKSTATE();
	} else {
		/*
		 * Callback worked. If the callback path was down, disable
		 * callbacks, so no more delegations will be issued. (This
		 * is done on the assumption that the callback pathway is
		 * flakey.)
		 */
		NFSLOCKSTATE();
		if (clp->lc_flags & LCL_CBDOWN)
			clp->lc_flags &= ~(LCL_CBDOWN | LCL_CALLBACKSON);
		NFSUNLOCKSTATE();
		if (nd->nd_repstat)
			error = nd->nd_repstat;
		else if (procnum == NFSV4OP_CBGETATTR)
			error = nfsv4_loadattr(nd, NULL, nap, NULL, NULL, 0,
			    NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL,
			    p, NULL);
		mbuf_freem(nd->nd_mrep);
	}
	NFSLOCKSTATE();
	clp->lc_cbref--;
	if ((clp->lc_flags & LCL_WAKEUPWANTED) && clp->lc_cbref == 0) {
		clp->lc_flags &= ~LCL_WAKEUPWANTED;
		NFSUNLOCKSTATE();
		wakeup((caddr_t)clp);
	} else {
		NFSUNLOCKSTATE();
	}
	return (error);
}

/*
 * Return the next index# for a clientid. Mostly just increment and return
 * the next one, but... if the 32bit unsigned does actually wrap around,
 * it should be rebooted.
 * At an average rate of one new client per second, it will wrap around in
 * approximately 136 years. (I think the server will have been shut
 * down or rebooted before then.)
 */
static u_int32_t
nfsrv_nextclientindex(void)
{
	static u_int32_t client_index = 0;

	client_index++;
	if (client_index != 0)
		return (client_index);

	printf("%s: out of clientids\n", __func__);
	return (client_index);
}

/*
 * Return the next index# for a stateid. Mostly just increment and return
 * the next one, but... if the 32bit unsigned does actually wrap around
 * (will a BSD server stay up that long?), find
 * new start and end values.
 */
static u_int32_t
nfsrv_nextstateindex(struct nfsclient *clp)
{
	struct nfsstate *stp;
	int i;
	u_int32_t canuse, min_index, max_index;

	if (!(clp->lc_flags & LCL_INDEXNOTOK)) {
		clp->lc_stateindex++;
		if (clp->lc_stateindex != clp->lc_statemaxindex)
			return (clp->lc_stateindex);
	}

	/*
	 * Yuck, we've hit the end.
	 * Look for a new min and max.
	 */
	min_index = 0;
	max_index = 0xffffffff;
	for (i = 0; i < NFSSTATEHASHSIZE; i++) {
	    LIST_FOREACH(stp, &clp->lc_stateid[i], ls_hash) {
		if (stp->ls_stateid.other[2] > 0x80000000) {
		    if (stp->ls_stateid.other[2] < max_index)
			max_index = stp->ls_stateid.other[2];
		} else {
		    if (stp->ls_stateid.other[2] > min_index)
			min_index = stp->ls_stateid.other[2];
		}
	    }
	}

	/*
	 * Yikes, highly unlikely, but I'll handle it anyhow.
	 */
	if (min_index == 0x80000000 && max_index == 0x80000001) {
	    canuse = 0;
	    /*
	     * Loop around until we find an unused entry. Return that
	     * and set LCL_INDEXNOTOK, so the search will continue next time.
	     * (This is one of those rare cases where a goto is the
	     *  cleanest way to code the loop.)
	     */
tryagain:
	    for (i = 0; i < NFSSTATEHASHSIZE; i++) {
		LIST_FOREACH(stp, &clp->lc_stateid[i], ls_hash) {
		    if (stp->ls_stateid.other[2] == canuse) {
			canuse++;
			goto tryagain;
		    }
		}
	    }
	    clp->lc_flags |= LCL_INDEXNOTOK;
	    return (canuse);
	}

	/*
	 * Ok to start again from min + 1.
	 */
	clp->lc_stateindex = min_index + 1;
	clp->lc_statemaxindex = max_index;
	clp->lc_flags &= ~LCL_INDEXNOTOK;
	return (clp->lc_stateindex);
}

/*
 * The following functions handle the stable storage file that deals with
 * the edge conditions described in RFC3530 Sec. 8.6.3.
 * The file is as follows:
 * - a single record at the beginning that has the lease time of the
 *   previous server instance (before the last reboot) and the nfsrvboottime
 *   values for the previous server boots.
 *   These previous boot times are used to ensure that the current
 *   nfsrvboottime does not, somehow, get set to a previous one.
 *   (This is important so that Stale ClientIDs and StateIDs can
 *    be recognized.)
 *   The number of previous nfsvrboottime values preceeds the list.
 * - followed by some number of appended records with:
 *   - client id string
 *   - flag that indicates it is a record revoking state via lease
 *     expiration or similar
 *     OR has successfully acquired state.
 * These structures vary in length, with the client string at the end, up
 * to NFSV4_OPAQUELIMIT in size.
 *
 * At the end of the grace period, the file is truncated, the first
 * record is rewritten with updated information and any acquired state
 * records for successful reclaims of state are written.
 *
 * Subsequent records are appended when the first state is issued to
 * a client and when state is revoked for a client.
 *
 * When reading the file in, state issued records that come later in
 * the file override older ones, since the append log is in cronological order.
 * If, for some reason, the file can't be read, the grace period is
 * immediately terminated and all reclaims get NFSERR_NOGRACE.
 */

/*
 * Read in the stable storage file. Called by nfssvc() before the nfsd
 * processes start servicing requests.
 */
APPLESTATIC void
nfsrv_setupstable(NFSPROC_T *p)
{
	struct nfsrv_stablefirst *sf = &nfsrv_stablefirst;
	struct nfsrv_stable *sp, *nsp;
	struct nfst_rec *tsp;
	int error, i, tryagain;
	off_t off = 0;
	int aresid, len;
	struct timeval curtime;

	/*
	 * If NFSNSF_UPDATEDONE is set, this is a restart of the nfsds without
	 * a reboot, so state has not been lost.
	 */
	if (sf->nsf_flags & NFSNSF_UPDATEDONE)
		return;
	/*
	 * Set Grace over just until the file reads successfully.
	 */
	NFSGETTIME(&curtime);
	nfsrvboottime = curtime.tv_sec;
	LIST_INIT(&sf->nsf_head);
	sf->nsf_flags = (NFSNSF_GRACEOVER | NFSNSF_NEEDLOCK);
	sf->nsf_eograce = NFSD_MONOSEC + NFSRV_LEASEDELTA;
	if (sf->nsf_fp == NULL)
		return;
	error = NFSD_RDWR(UIO_READ, NFSFPVNODE(sf->nsf_fp),
	    (caddr_t)&sf->nsf_rec, sizeof (struct nfsf_rec), off, UIO_SYSSPACE,
	    0, NFSFPCRED(sf->nsf_fp), &aresid, p);
	if (error || aresid || sf->nsf_numboots == 0 ||
		sf->nsf_numboots > NFSNSF_MAXNUMBOOTS)
		return;

	/*
	 * Now, read in the boottimes.
	 */
	sf->nsf_bootvals = (time_t *)malloc((sf->nsf_numboots + 1) *
		sizeof (time_t), M_TEMP, M_WAITOK);
	off = sizeof (struct nfsf_rec);
	error = NFSD_RDWR(UIO_READ, NFSFPVNODE(sf->nsf_fp),
	    (caddr_t)sf->nsf_bootvals, sf->nsf_numboots * sizeof (time_t), off,
	    UIO_SYSSPACE, 0, NFSFPCRED(sf->nsf_fp), &aresid, p);
	if (error || aresid) {
		free((caddr_t)sf->nsf_bootvals, M_TEMP);
		sf->nsf_bootvals = NULL;
		return;
	}

	/*
	 * Make sure this nfsrvboottime is different from all recorded
	 * previous ones.
	 */
	do {
		tryagain = 0;
		for (i = 0; i < sf->nsf_numboots; i++) {
			if (nfsrvboottime == sf->nsf_bootvals[i]) {
				nfsrvboottime++;
				tryagain = 1;
				break;
			}
		}
	} while (tryagain);

	sf->nsf_flags |= NFSNSF_OK;
	off += (sf->nsf_numboots * sizeof (time_t));

	/*
	 * Read through the file, building a list of records for grace
	 * checking.
	 * Each record is between sizeof (struct nfst_rec) and
	 * sizeof (struct nfst_rec) + NFSV4_OPAQUELIMIT - 1
	 * and is actually sizeof (struct nfst_rec) + nst_len - 1.
	 */
	tsp = (struct nfst_rec *)malloc(sizeof (struct nfst_rec) +
		NFSV4_OPAQUELIMIT - 1, M_TEMP, M_WAITOK);
	do {
	    error = NFSD_RDWR(UIO_READ, NFSFPVNODE(sf->nsf_fp),
	        (caddr_t)tsp, sizeof (struct nfst_rec) + NFSV4_OPAQUELIMIT - 1,
	        off, UIO_SYSSPACE, 0, NFSFPCRED(sf->nsf_fp), &aresid, p);
	    len = (sizeof (struct nfst_rec) + NFSV4_OPAQUELIMIT - 1) - aresid;
	    if (error || (len > 0 && (len < sizeof (struct nfst_rec) ||
		len < (sizeof (struct nfst_rec) + tsp->len - 1)))) {
		/*
		 * Yuck, the file has been corrupted, so just return
		 * after clearing out any restart state, so the grace period
		 * is over.
		 */
		LIST_FOREACH_SAFE(sp, &sf->nsf_head, nst_list, nsp) {
			LIST_REMOVE(sp, nst_list);
			free((caddr_t)sp, M_TEMP);
		}
		free((caddr_t)tsp, M_TEMP);
		sf->nsf_flags &= ~NFSNSF_OK;
		free((caddr_t)sf->nsf_bootvals, M_TEMP);
		sf->nsf_bootvals = NULL;
		return;
	    }
	    if (len > 0) {
		off += sizeof (struct nfst_rec) + tsp->len - 1;
		/*
		 * Search the list for a matching client.
		 */
		LIST_FOREACH(sp, &sf->nsf_head, nst_list) {
			if (tsp->len == sp->nst_len &&
			    !NFSBCMP(tsp->client, sp->nst_client, tsp->len))
				break;
		}
		if (sp == LIST_END(&sf->nsf_head)) {
			sp = (struct nfsrv_stable *)malloc(tsp->len +
				sizeof (struct nfsrv_stable) - 1, M_TEMP,
				M_WAITOK);
			NFSBCOPY((caddr_t)tsp, (caddr_t)&sp->nst_rec,
				sizeof (struct nfst_rec) + tsp->len - 1);
			LIST_INSERT_HEAD(&sf->nsf_head, sp, nst_list);
		} else {
			if (tsp->flag == NFSNST_REVOKE)
				sp->nst_flag |= NFSNST_REVOKE;
			else
				/*
				 * A subsequent timestamp indicates the client
				 * did a setclientid/confirm and any previous
				 * revoke is no longer relevant.
				 */
				sp->nst_flag &= ~NFSNST_REVOKE;
		}
	    }
	} while (len > 0);
	free((caddr_t)tsp, M_TEMP);
	sf->nsf_flags = NFSNSF_OK;
	sf->nsf_eograce = NFSD_MONOSEC + sf->nsf_lease +
		NFSRV_LEASEDELTA;
}

/*
 * Update the stable storage file, now that the grace period is over.
 */
APPLESTATIC void
nfsrv_updatestable(NFSPROC_T *p)
{
	struct nfsrv_stablefirst *sf = &nfsrv_stablefirst;
	struct nfsrv_stable *sp, *nsp;
	int i;
	struct nfsvattr nva;
	vnode_t vp;
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 500000)
	mount_t mp = NULL;
#endif
	int error;

	if (sf->nsf_fp == NULL || (sf->nsf_flags & NFSNSF_UPDATEDONE))
		return;
	sf->nsf_flags |= NFSNSF_UPDATEDONE;
	/*
	 * Ok, we need to rewrite the stable storage file.
	 * - truncate to 0 length
	 * - write the new first structure
	 * - loop through the data structures, writing out any that
	 *   have timestamps older than the old boot
	 */
	if (sf->nsf_bootvals) {
		sf->nsf_numboots++;
		for (i = sf->nsf_numboots - 2; i >= 0; i--)
			sf->nsf_bootvals[i + 1] = sf->nsf_bootvals[i];
	} else {
		sf->nsf_numboots = 1;
		sf->nsf_bootvals = (time_t *)malloc(sizeof (time_t),
			M_TEMP, M_WAITOK);
	}
	sf->nsf_bootvals[0] = nfsrvboottime;
	sf->nsf_lease = nfsrv_lease;
	NFSVNO_ATTRINIT(&nva);
	NFSVNO_SETATTRVAL(&nva, size, 0);
	vp = NFSFPVNODE(sf->nsf_fp);
	vn_start_write(vp, &mp, V_WAIT);
	if (vn_lock(vp, LK_EXCLUSIVE) == 0) {
		error = nfsvno_setattr(vp, &nva, NFSFPCRED(sf->nsf_fp), p,
		    NULL);
		VOP_UNLOCK(vp, 0);
	} else
		error = EPERM;
	vn_finished_write(mp);
	if (!error)
	    error = NFSD_RDWR(UIO_WRITE, vp,
		(caddr_t)&sf->nsf_rec, sizeof (struct nfsf_rec), (off_t)0,
		UIO_SYSSPACE, IO_SYNC, NFSFPCRED(sf->nsf_fp), NULL, p);
	if (!error)
	    error = NFSD_RDWR(UIO_WRITE, vp,
		(caddr_t)sf->nsf_bootvals,
		sf->nsf_numboots * sizeof (time_t),
		(off_t)(sizeof (struct nfsf_rec)),
		UIO_SYSSPACE, IO_SYNC, NFSFPCRED(sf->nsf_fp), NULL, p);
	free((caddr_t)sf->nsf_bootvals, M_TEMP);
	sf->nsf_bootvals = NULL;
	if (error) {
		sf->nsf_flags &= ~NFSNSF_OK;
		printf("EEK! Can't write NfsV4 stable storage file\n");
		return;
	}
	sf->nsf_flags |= NFSNSF_OK;

	/*
	 * Loop through the list and write out timestamp records for
	 * any clients that successfully reclaimed state.
	 */
	LIST_FOREACH_SAFE(sp, &sf->nsf_head, nst_list, nsp) {
		if (sp->nst_flag & NFSNST_GOTSTATE) {
			nfsrv_writestable(sp->nst_client, sp->nst_len,
				NFSNST_NEWSTATE, p);
			sp->nst_clp->lc_flags |= LCL_STAMPEDSTABLE;
		}
		LIST_REMOVE(sp, nst_list);
		free((caddr_t)sp, M_TEMP);
	}
	nfsrv_backupstable();
}

/*
 * Append a record to the stable storage file.
 */
APPLESTATIC void
nfsrv_writestable(u_char *client, int len, int flag, NFSPROC_T *p)
{
	struct nfsrv_stablefirst *sf = &nfsrv_stablefirst;
	struct nfst_rec *sp;
	int error;

	if (!(sf->nsf_flags & NFSNSF_OK) || sf->nsf_fp == NULL)
		return;
	sp = (struct nfst_rec *)malloc(sizeof (struct nfst_rec) +
		len - 1, M_TEMP, M_WAITOK);
	sp->len = len;
	NFSBCOPY(client, sp->client, len);
	sp->flag = flag;
	error = NFSD_RDWR(UIO_WRITE, NFSFPVNODE(sf->nsf_fp),
	    (caddr_t)sp, sizeof (struct nfst_rec) + len - 1, (off_t)0,
	    UIO_SYSSPACE, (IO_SYNC | IO_APPEND), NFSFPCRED(sf->nsf_fp), NULL, p);
	free((caddr_t)sp, M_TEMP);
	if (error) {
		sf->nsf_flags &= ~NFSNSF_OK;
		printf("EEK! Can't write NfsV4 stable storage file\n");
	}
}

/*
 * This function is called during the grace period to mark a client
 * that successfully reclaimed state.
 */
static void
nfsrv_markstable(struct nfsclient *clp)
{
	struct nfsrv_stable *sp;

	/*
	 * First find the client structure.
	 */
	LIST_FOREACH(sp, &nfsrv_stablefirst.nsf_head, nst_list) {
		if (sp->nst_len == clp->lc_idlen &&
		    !NFSBCMP(sp->nst_client, clp->lc_id, sp->nst_len))
			break;
	}
	if (sp == LIST_END(&nfsrv_stablefirst.nsf_head))
		return;

	/*
	 * Now, just mark it and set the nfsclient back pointer.
	 */
	sp->nst_flag |= NFSNST_GOTSTATE;
	sp->nst_clp = clp;
}

/*
 * This function is called for a reclaim, to see if it gets grace.
 * It returns 0 if a reclaim is allowed, 1 otherwise.
 */
static int
nfsrv_checkstable(struct nfsclient *clp)
{
	struct nfsrv_stable *sp;

	/*
	 * First, find the entry for the client.
	 */
	LIST_FOREACH(sp, &nfsrv_stablefirst.nsf_head, nst_list) {
		if (sp->nst_len == clp->lc_idlen &&
		    !NFSBCMP(sp->nst_client, clp->lc_id, sp->nst_len))
			break;
	}

	/*
	 * If not in the list, state was revoked or no state was issued
	 * since the previous reboot, a reclaim is denied.
	 */
	if (sp == LIST_END(&nfsrv_stablefirst.nsf_head) ||
	    (sp->nst_flag & NFSNST_REVOKE) ||
	    !(nfsrv_stablefirst.nsf_flags & NFSNSF_OK))
		return (1);
	return (0);
}

/*
 * Test for and try to clear out a conflicting client. This is called by
 * nfsrv_lockctrl() and nfsrv_openctrl() when conflicts with other clients
 * a found.
 * The trick here is that it can't revoke a conflicting client with an
 * expired lease unless it holds the v4root lock, so...
 * If no v4root lock, get the lock and return 1 to indicate "try again".
 * Return 0 to indicate the conflict can't be revoked and 1 to indicate
 * the revocation worked and the conflicting client is "bye, bye", so it
 * can be tried again.
 * Return 2 to indicate that the vnode is VI_DOOMED after vn_lock().
 * Unlocks State before a non-zero value is returned.
 */
static int
nfsrv_clientconflict(struct nfsclient *clp, int *haslockp, vnode_t vp,
    NFSPROC_T *p)
{
	int gotlock, lktype;

	/*
	 * If lease hasn't expired, we can't fix it.
	 */
	if (clp->lc_expiry >= NFSD_MONOSEC ||
	    !(nfsrv_stablefirst.nsf_flags & NFSNSF_UPDATEDONE))
		return (0);
	if (*haslockp == 0) {
		NFSUNLOCKSTATE();
		lktype = VOP_ISLOCKED(vp);
		VOP_UNLOCK(vp, 0);
		NFSLOCKV4ROOTMUTEX();
		nfsv4_relref(&nfsv4rootfs_lock);
		do {
			gotlock = nfsv4_lock(&nfsv4rootfs_lock, 1, NULL,
			    NFSV4ROOTLOCKMUTEXPTR, NULL);
		} while (!gotlock);
		NFSUNLOCKV4ROOTMUTEX();
		*haslockp = 1;
		vn_lock(vp, lktype | LK_RETRY);
		if ((vp->v_iflag & VI_DOOMED) != 0)
			return (2);
		else
			return (1);
	}
	NFSUNLOCKSTATE();

	/*
	 * Ok, we can expire the conflicting client.
	 */
	nfsrv_writestable(clp->lc_id, clp->lc_idlen, NFSNST_REVOKE, p);
	nfsrv_backupstable();
	nfsrv_cleanclient(clp, p);
	nfsrv_freedeleglist(&clp->lc_deleg);
	nfsrv_freedeleglist(&clp->lc_olddeleg);
	LIST_REMOVE(clp, lc_hash);
	nfsrv_zapclient(clp, p);
	return (1);
}

/*
 * Resolve a delegation conflict.
 * Returns 0 to indicate the conflict was resolved without sleeping.
 * Return -1 to indicate that the caller should check for conflicts again.
 * Return > 0 for an error that should be returned, normally NFSERR_DELAY.
 *
 * Also, manipulate the nfsv4root_lock, as required. It isn't changed
 * for a return of 0, since there was no sleep and it could be required
 * later. It is released for a return of NFSERR_DELAY, since the caller
 * will return that error. It is released when a sleep was done waiting
 * for the delegation to be returned or expire (so that other nfsds can
 * handle ops). Then, it must be acquired for the write to stable storage.
 * (This function is somewhat similar to nfsrv_clientconflict(), but
 *  the semantics differ in a couple of subtle ways. The return of 0
 *  indicates the conflict was resolved without sleeping here, not
 *  that the conflict can't be resolved and the handling of nfsv4root_lock
 *  differs, as noted above.)
 * Unlocks State before returning a non-zero value.
 */
static int
nfsrv_delegconflict(struct nfsstate *stp, int *haslockp, NFSPROC_T *p,
    vnode_t vp)
{
	struct nfsclient *clp = stp->ls_clp;
	int gotlock, error, lktype, retrycnt, zapped_clp;
	nfsv4stateid_t tstateid;
	fhandle_t tfh;

	/*
	 * If the conflict is with an old delegation...
	 */
	if (stp->ls_flags & NFSLCK_OLDDELEG) {
		/*
		 * You can delete it, if it has expired.
		 */
		if (clp->lc_delegtime < NFSD_MONOSEC) {
			nfsrv_freedeleg(stp);
			NFSUNLOCKSTATE();
			return (-1);
		}
		NFSUNLOCKSTATE();
		/*
		 * During this delay, the old delegation could expire or it
		 * could be recovered by the client via an Open with
		 * CLAIM_DELEGATE_PREV.
		 * Release the nfsv4root_lock, if held.
		 */
		if (*haslockp) {
			*haslockp = 0;
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (NFSERR_DELAY);
	}

	/*
	 * It's a current delegation, so:
	 * - check to see if the delegation has expired
	 *   - if so, get the v4root lock and then expire it
	 */
	if (!(stp->ls_flags & NFSLCK_DELEGRECALL)) {
		/*
		 * - do a recall callback, since not yet done
		 * For now, never allow truncate to be set. To use
		 * truncate safely, it must be guaranteed that the
		 * Remove, Rename or Setattr with size of 0 will
		 * succeed and that would require major changes to
		 * the VFS/Vnode OPs.
		 * Set the expiry time large enough so that it won't expire
		 * until after the callback, then set it correctly, once
		 * the callback is done. (The delegation will now time
		 * out whether or not the Recall worked ok. The timeout
		 * will be extended when ops are done on the delegation
		 * stateid, up to the timelimit.)
		 */
		stp->ls_delegtime = NFSD_MONOSEC + (2 * nfsrv_lease) +
		    NFSRV_LEASEDELTA;
		stp->ls_delegtimelimit = NFSD_MONOSEC + (6 * nfsrv_lease) +
		    NFSRV_LEASEDELTA;
		stp->ls_flags |= NFSLCK_DELEGRECALL;

		/*
		 * Loop NFSRV_CBRETRYCNT times while the CBRecall replies
		 * NFSERR_BADSTATEID or NFSERR_BADHANDLE. This is done
		 * in order to try and avoid a race that could happen
		 * when a CBRecall request passed the Open reply with
		 * the delegation in it when transitting the network.
		 * Since nfsrv_docallback will sleep, don't use stp after
		 * the call.
		 */
		NFSBCOPY((caddr_t)&stp->ls_stateid, (caddr_t)&tstateid,
		    sizeof (tstateid));
		NFSBCOPY((caddr_t)&stp->ls_lfp->lf_fh, (caddr_t)&tfh,
		    sizeof (tfh));
		NFSUNLOCKSTATE();
		if (*haslockp) {
			*haslockp = 0;
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		retrycnt = 0;
		do {
		    error = nfsrv_docallback(clp, NFSV4OP_CBRECALL,
			&tstateid, 0, &tfh, NULL, NULL, p);
		    retrycnt++;
		} while ((error == NFSERR_BADSTATEID ||
		    error == NFSERR_BADHANDLE) && retrycnt < NFSV4_CBRETRYCNT);
		return (NFSERR_DELAY);
	}

	if (clp->lc_expiry >= NFSD_MONOSEC &&
	    stp->ls_delegtime >= NFSD_MONOSEC) {
		NFSUNLOCKSTATE();
		/*
		 * A recall has been done, but it has not yet expired.
		 * So, RETURN_DELAY.
		 */
		if (*haslockp) {
			*haslockp = 0;
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (NFSERR_DELAY);
	}

	/*
	 * If we don't yet have the lock, just get it and then return,
	 * since we need that before deleting expired state, such as
	 * this delegation.
	 * When getting the lock, unlock the vnode, so other nfsds that
	 * are in progress, won't get stuck waiting for the vnode lock.
	 */
	if (*haslockp == 0) {
		NFSUNLOCKSTATE();
		lktype = VOP_ISLOCKED(vp);
		VOP_UNLOCK(vp, 0);
		NFSLOCKV4ROOTMUTEX();
		nfsv4_relref(&nfsv4rootfs_lock);
		do {
			gotlock = nfsv4_lock(&nfsv4rootfs_lock, 1, NULL,
			    NFSV4ROOTLOCKMUTEXPTR, NULL);
		} while (!gotlock);
		NFSUNLOCKV4ROOTMUTEX();
		*haslockp = 1;
		vn_lock(vp, lktype | LK_RETRY);
		if ((vp->v_iflag & VI_DOOMED) != 0) {
			*haslockp = 0;
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
			return (NFSERR_PERM);
		}
		return (-1);
	}

	NFSUNLOCKSTATE();
	/*
	 * Ok, we can delete the expired delegation.
	 * First, write the Revoke record to stable storage and then
	 * clear out the conflict.
	 * Since all other nfsd threads are now blocked, we can safely
	 * sleep without the state changing.
	 */
	nfsrv_writestable(clp->lc_id, clp->lc_idlen, NFSNST_REVOKE, p);
	nfsrv_backupstable();
	if (clp->lc_expiry < NFSD_MONOSEC) {
		nfsrv_cleanclient(clp, p);
		nfsrv_freedeleglist(&clp->lc_deleg);
		nfsrv_freedeleglist(&clp->lc_olddeleg);
		LIST_REMOVE(clp, lc_hash);
		zapped_clp = 1;
	} else {
		nfsrv_freedeleg(stp);
		zapped_clp = 0;
	}
	if (zapped_clp)
		nfsrv_zapclient(clp, p);
	return (-1);
}

/*
 * Check for a remove allowed, if remove is set to 1 and get rid of
 * delegations.
 */
APPLESTATIC int
nfsrv_checkremove(vnode_t vp, int remove, NFSPROC_T *p)
{
	struct nfsstate *stp;
	struct nfslockfile *lfp;
	int error, haslock = 0;
	fhandle_t nfh;

	/*
	 * First, get the lock file structure.
	 * (A return of -1 means no associated state, so remove ok.)
	 */
	error = nfsrv_getlockfh(vp, NFSLCK_CHECK, NULL, &nfh, p);
tryagain:
	NFSLOCKSTATE();
	if (!error)
		error = nfsrv_getlockfile(NFSLCK_CHECK, NULL, &lfp, &nfh, 0);
	if (error) {
		NFSUNLOCKSTATE();
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		if (error == -1)
			return (0);
		return (error);
	}

	/*
	 * Now, we must Recall any delegations.
	 */
	error = nfsrv_cleandeleg(vp, lfp, NULL, &haslock, p);
	if (error) {
		/*
		 * nfsrv_cleandeleg() unlocks state for non-zero
		 * return.
		 */
		if (error == -1)
			goto tryagain;
		if (haslock) {
			NFSLOCKV4ROOTMUTEX();
			nfsv4_unlock(&nfsv4rootfs_lock, 1);
			NFSUNLOCKV4ROOTMUTEX();
		}
		return (error);
	}

	/*
	 * Now, look for a conflicting open share.
	 */
	if (remove) {
		LIST_FOREACH(stp, &lfp->lf_open, ls_file) {
			if (stp->ls_flags & NFSLCK_WRITEDENY) {
				error = NFSERR_FILEOPEN;
				break;
			}
		}
	}

	NFSUNLOCKSTATE();
	if (haslock) {
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
	}
	return (error);
}

/*
 * Clear out all delegations for the file referred to by lfp.
 * May return NFSERR_DELAY, if there will be a delay waiting for
 * delegations to expire.
 * Returns -1 to indicate it slept while recalling a delegation.
 * This function has the side effect of deleting the nfslockfile structure,
 * if it no longer has associated state and didn't have to sleep.
 * Unlocks State before a non-zero value is returned.
 */
static int
nfsrv_cleandeleg(vnode_t vp, struct nfslockfile *lfp,
    struct nfsclient *clp, int *haslockp, NFSPROC_T *p)
{
	struct nfsstate *stp, *nstp;
	int ret;

	stp = LIST_FIRST(&lfp->lf_deleg);
	while (stp != LIST_END(&lfp->lf_deleg)) {
		nstp = LIST_NEXT(stp, ls_file);
		if (stp->ls_clp != clp) {
			ret = nfsrv_delegconflict(stp, haslockp, p, vp);
			if (ret) {
				/*
				 * nfsrv_delegconflict() unlocks state
				 * when it returns non-zero.
				 */
				return (ret);
			}
		}
		stp = nstp;
	}
	return (0);
}

/*
 * There are certain operations that, when being done outside of NFSv4,
 * require that any NFSv4 delegation for the file be recalled.
 * This function is to be called for those cases:
 * VOP_RENAME() - When a delegation is being recalled for any reason,
 *	the client may have to do Opens against the server, using the file's
 *	final component name. If the file has been renamed on the server,
 *	that component name will be incorrect and the Open will fail.
 * VOP_REMOVE() - Theoretically, a client could Open a file after it has
 *	been removed on the server, if there is a delegation issued to
 *	that client for the file. I say "theoretically" since clients
 *	normally do an Access Op before the Open and that Access Op will
 *	fail with ESTALE. Note that NFSv2 and 3 don't even do Opens, so
 *	they will detect the file's removal in the same manner. (There is
 *	one case where RFC3530 allows a client to do an Open without first
 *	doing an Access Op, which is passage of a check against the ACE
 *	returned with a Write delegation, but current practice is to ignore
 *	the ACE and always do an Access Op.)
 *	Since the functions can only be called with an unlocked vnode, this
 *	can't be done at this time.
 * VOP_ADVLOCK() - When a client holds a delegation, it can issue byte range
 *	locks locally in the client, which are not visible to the server. To
 *	deal with this, issuing of delegations for a vnode must be disabled
 *	and all delegations for the vnode recalled. This is done via the
 *	second function, using the VV_DISABLEDELEG vflag on the vnode.
 */
APPLESTATIC void
nfsd_recalldelegation(vnode_t vp, NFSPROC_T *p)
{
	struct timespec mytime;
	int32_t starttime;
	int error;

	/*
	 * First, check to see if the server is currently running and it has
	 * been called for a regular file when issuing delegations.
	 */
	if (newnfs_numnfsd == 0 || vp->v_type != VREG ||
	    nfsrv_issuedelegs == 0)
		return;

	KASSERT((VOP_ISLOCKED(vp) != LK_EXCLUSIVE), ("vp %p is locked", vp));
	/*
	 * First, get a reference on the nfsv4rootfs_lock so that an
	 * exclusive lock cannot be acquired by another thread.
	 */
	NFSLOCKV4ROOTMUTEX();
	nfsv4_getref(&nfsv4rootfs_lock, NULL, NFSV4ROOTLOCKMUTEXPTR, NULL);
	NFSUNLOCKV4ROOTMUTEX();

	/*
	 * Now, call nfsrv_checkremove() in a loop while it returns
	 * NFSERR_DELAY. Return upon any other error or when timed out.
	 */
	NFSGETNANOTIME(&mytime);
	starttime = (u_int32_t)mytime.tv_sec;
	do {
		if (vn_lock(vp, LK_EXCLUSIVE) == 0) {
			error = nfsrv_checkremove(vp, 0, p);
			VOP_UNLOCK(vp, 0);
		} else
			error = EPERM;
		if (error == NFSERR_DELAY) {
			NFSGETNANOTIME(&mytime);
			if (((u_int32_t)mytime.tv_sec - starttime) >
			    NFS_REMOVETIMEO &&
			    ((u_int32_t)mytime.tv_sec - starttime) <
			    100000)
				break;
			/* Sleep for a short period of time */
			(void) nfs_catnap(PZERO, 0, "nfsremove");
		}
	} while (error == NFSERR_DELAY);
	NFSLOCKV4ROOTMUTEX();
	nfsv4_relref(&nfsv4rootfs_lock);
	NFSUNLOCKV4ROOTMUTEX();
}

APPLESTATIC void
nfsd_disabledelegation(vnode_t vp, NFSPROC_T *p)
{

#ifdef VV_DISABLEDELEG
	/*
	 * First, flag issuance of delegations disabled.
	 */
	atomic_set_long(&vp->v_vflag, VV_DISABLEDELEG);
#endif

	/*
	 * Then call nfsd_recalldelegation() to get rid of all extant
	 * delegations.
	 */
	nfsd_recalldelegation(vp, p);
}

/*
 * Check for conflicting locks, etc. and then get rid of delegations.
 * (At one point I thought that I should get rid of delegations for any
 *  Setattr, since it could potentially disallow the I/O op (read or write)
 *  allowed by the delegation. However, Setattr Ops that aren't changing
 *  the size get a stateid of all 0s, so you can't tell if it is a delegation
 *  for the same client or a different one, so I decided to only get rid
 *  of delegations for other clients when the size is being changed.)
 * In general, a Setattr can disable NFS I/O Ops that are outstanding, such
 * as Write backs, even if there is no delegation, so it really isn't any
 * different?)
 */
APPLESTATIC int
nfsrv_checksetattr(vnode_t vp, struct nfsrv_descript *nd,
    nfsv4stateid_t *stateidp, struct nfsvattr *nvap, nfsattrbit_t *attrbitp,
    struct nfsexstuff *exp, NFSPROC_T *p)
{
	struct nfsstate st, *stp = &st;
	struct nfslock lo, *lop = &lo;
	int error = 0;
	nfsquad_t clientid;

	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_SIZE)) {
		stp->ls_flags = (NFSLCK_CHECK | NFSLCK_WRITEACCESS);
		lop->lo_first = nvap->na_size;
	} else {
		stp->ls_flags = 0;
		lop->lo_first = 0;
	}
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_OWNER) ||
	    NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_OWNERGROUP) ||
	    NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_MODE) ||
	    NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_ACL))
		stp->ls_flags |= NFSLCK_SETATTR;
	if (stp->ls_flags == 0)
		return (0);
	lop->lo_end = NFS64BITSSET;
	lop->lo_flags = NFSLCK_WRITE;
	stp->ls_ownerlen = 0;
	stp->ls_op = NULL;
	stp->ls_uid = nd->nd_cred->cr_uid;
	stp->ls_stateid.seqid = stateidp->seqid;
	clientid.lval[0] = stp->ls_stateid.other[0] = stateidp->other[0];
	clientid.lval[1] = stp->ls_stateid.other[1] = stateidp->other[1];
	stp->ls_stateid.other[2] = stateidp->other[2];
	error = nfsrv_lockctrl(vp, &stp, &lop, NULL, clientid,
	    stateidp, exp, nd, p);
	return (error);
}

/*
 * Check for a write delegation and do a CBGETATTR if there is one, updating
 * the attributes, as required.
 * Should I return an error if I can't get the attributes? (For now, I'll
 * just return ok.
 */
APPLESTATIC int
nfsrv_checkgetattr(struct nfsrv_descript *nd, vnode_t vp,
    struct nfsvattr *nvap, nfsattrbit_t *attrbitp, struct ucred *cred,
    NFSPROC_T *p)
{
	struct nfsstate *stp;
	struct nfslockfile *lfp;
	struct nfsclient *clp;
	struct nfsvattr nva;
	fhandle_t nfh;
	int error;
	nfsattrbit_t cbbits;
	u_quad_t delegfilerev;

	NFSCBGETATTR_ATTRBIT(attrbitp, &cbbits);
	if (!NFSNONZERO_ATTRBIT(&cbbits))
		return (0);

	/*
	 * Get the lock file structure.
	 * (A return of -1 means no associated state, so return ok.)
	 */
	error = nfsrv_getlockfh(vp, NFSLCK_CHECK, NULL, &nfh, p);
	NFSLOCKSTATE();
	if (!error)
		error = nfsrv_getlockfile(NFSLCK_CHECK, NULL, &lfp, &nfh, 0);
	if (error) {
		NFSUNLOCKSTATE();
		if (error == -1)
			return (0);
		return (error);
	}

	/*
	 * Now, look for a write delegation.
	 */
	LIST_FOREACH(stp, &lfp->lf_deleg, ls_file) {
		if (stp->ls_flags & NFSLCK_DELEGWRITE)
			break;
	}
	if (stp == LIST_END(&lfp->lf_deleg)) {
		NFSUNLOCKSTATE();
		return (0);
	}
	clp = stp->ls_clp;
	delegfilerev = stp->ls_filerev;

	/*
	 * If the Write delegation was issued as a part of this Compound RPC
	 * or if we have an Implied Clientid (used in a previous Op in this
	 * compound) and it is the client the delegation was issued to,
	 * just return ok.
	 * I also assume that it is from the same client iff the network
	 * host IP address is the same as the callback address. (Not
	 * exactly correct by the RFC, but avoids a lot of Getattr
	 * callbacks.)
	 */
	if (nd->nd_compref == stp->ls_compref ||
	    ((nd->nd_flag & ND_IMPLIEDCLID) &&
	     clp->lc_clientid.qval == nd->nd_clientid.qval) ||
	     nfsaddr2_match(clp->lc_req.nr_nam, nd->nd_nam)) {
		NFSUNLOCKSTATE();
		return (0);
	}

	/*
	 * We are now done with the delegation state structure,
	 * so the statelock can be released and we can now tsleep().
	 */

	/*
	 * Now, we must do the CB Getattr callback, to see if Change or Size
	 * has changed.
	 */
	if (clp->lc_expiry >= NFSD_MONOSEC) {
		NFSUNLOCKSTATE();
		NFSVNO_ATTRINIT(&nva);
		nva.na_filerev = NFS64BITSSET;
		error = nfsrv_docallback(clp, NFSV4OP_CBGETATTR, NULL,
		    0, &nfh, &nva, &cbbits, p);
		if (!error) {
			if ((nva.na_filerev != NFS64BITSSET &&
			    nva.na_filerev > delegfilerev) ||
			    (NFSVNO_ISSETSIZE(&nva) &&
			     nva.na_size != nvap->na_size)) {
				nfsvno_updfilerev(vp, nvap, cred, p);
				if (NFSVNO_ISSETSIZE(&nva))
					nvap->na_size = nva.na_size;
			}
		}
	} else {
		NFSUNLOCKSTATE();
	}
	return (0);
}

/*
 * This function looks for openowners that haven't had any opens for
 * a while and throws them away. Called by an nfsd when NFSNSF_NOOPENS
 * is set.
 */
APPLESTATIC void
nfsrv_throwawayopens(NFSPROC_T *p)
{
	struct nfsclient *clp, *nclp;
	struct nfsstate *stp, *nstp;
	int i;

	NFSLOCKSTATE();
	nfsrv_stablefirst.nsf_flags &= ~NFSNSF_NOOPENS;
	/*
	 * For each client...
	 */
	for (i = 0; i < NFSCLIENTHASHSIZE; i++) {
	    LIST_FOREACH_SAFE(clp, &nfsclienthash[i], lc_hash, nclp) {
		LIST_FOREACH_SAFE(stp, &clp->lc_open, ls_list, nstp) {
			if (LIST_EMPTY(&stp->ls_open) &&
			    (stp->ls_noopens > NFSNOOPEN ||
			     (nfsrv_openpluslock * 2) >
			     NFSRV_V4STATELIMIT))
				nfsrv_freeopenowner(stp, 0, p);
		}
	    }
	}
	NFSUNLOCKSTATE();
}

/*
 * This function checks to see if the credentials are the same.
 * Returns 1 for not same, 0 otherwise.
 */
static int
nfsrv_notsamecredname(struct nfsrv_descript *nd, struct nfsclient *clp)
{

	if (nd->nd_flag & ND_GSS) {
		if (!(clp->lc_flags & LCL_GSS))
			return (1);
		if (clp->lc_flags & LCL_NAME) {
			if (nd->nd_princlen != clp->lc_namelen ||
			    NFSBCMP(nd->nd_principal, clp->lc_name,
				clp->lc_namelen))
				return (1);
			else
				return (0);
		}
		if (nd->nd_cred->cr_uid == clp->lc_uid)
			return (0);
		else
			return (1);
	} else if (clp->lc_flags & LCL_GSS)
		return (1);
	/*
	 * For AUTH_SYS, allow the same uid or root. (This is underspecified
	 * in RFC3530, which talks about principals, but doesn't say anything
	 * about uids for AUTH_SYS.)
	 */
	if (nd->nd_cred->cr_uid == clp->lc_uid || nd->nd_cred->cr_uid == 0)
		return (0);
	else
		return (1);
}

/*
 * Calculate the lease expiry time.
 */
static time_t
nfsrv_leaseexpiry(void)
{
	struct timeval curtime;

	NFSGETTIME(&curtime);
	if (nfsrv_stablefirst.nsf_eograce > NFSD_MONOSEC)
		return (NFSD_MONOSEC + 2 * (nfsrv_lease + NFSRV_LEASEDELTA));
	return (NFSD_MONOSEC + nfsrv_lease + NFSRV_LEASEDELTA);
}

/*
 * Delay the delegation timeout as far as ls_delegtimelimit, as required.
 */
static void
nfsrv_delaydelegtimeout(struct nfsstate *stp)
{

	if ((stp->ls_flags & NFSLCK_DELEGRECALL) == 0)
		return;

	if ((stp->ls_delegtime + 15) > NFSD_MONOSEC &&
	    stp->ls_delegtime < stp->ls_delegtimelimit) {
		stp->ls_delegtime += nfsrv_lease;
		if (stp->ls_delegtime > stp->ls_delegtimelimit)
			stp->ls_delegtime = stp->ls_delegtimelimit;
	}
}

/*
 * This function checks to see if there is any other state associated
 * with the openowner for this Open.
 * It returns 1 if there is no other state, 0 otherwise.
 */
static int
nfsrv_nootherstate(struct nfsstate *stp)
{
	struct nfsstate *tstp;

	LIST_FOREACH(tstp, &stp->ls_openowner->ls_open, ls_list) {
		if (tstp != stp || !LIST_EMPTY(&tstp->ls_lock))
			return (0);
	}
	return (1);
}

/*
 * Create a list of lock deltas (changes to local byte range locking
 * that can be rolled back using the list) and apply the changes via
 * nfsvno_advlock(). Optionally, lock the list. It is expected that either
 * the rollback or update function will be called after this.
 * It returns an error (and rolls back, as required), if any nfsvno_advlock()
 * call fails. If it returns an error, it will unlock the list.
 */
static int
nfsrv_locallock(vnode_t vp, struct nfslockfile *lfp, int flags,
    uint64_t first, uint64_t end, struct nfslockconflict *cfp, NFSPROC_T *p)
{
	struct nfslock *lop, *nlop;
	int error = 0;

	/* Loop through the list of locks. */
	lop = LIST_FIRST(&lfp->lf_locallock);
	while (first < end && lop != NULL) {
		nlop = LIST_NEXT(lop, lo_lckowner);
		if (first >= lop->lo_end) {
			/* not there yet */
			lop = nlop;
		} else if (first < lop->lo_first) {
			/* new one starts before entry in list */
			if (end <= lop->lo_first) {
				/* no overlap between old and new */
				error = nfsrv_dolocal(vp, lfp, flags,
				    NFSLCK_UNLOCK, first, end, cfp, p);
				if (error != 0)
					break;
				first = end;
			} else {
				/* handle fragment overlapped with new one */
				error = nfsrv_dolocal(vp, lfp, flags,
				    NFSLCK_UNLOCK, first, lop->lo_first, cfp,
				    p);
				if (error != 0)
					break;
				first = lop->lo_first;
			}
		} else {
			/* new one overlaps this entry in list */
			if (end <= lop->lo_end) {
				/* overlaps all of new one */
				error = nfsrv_dolocal(vp, lfp, flags,
				    lop->lo_flags, first, end, cfp, p);
				if (error != 0)
					break;
				first = end;
			} else {
				/* handle fragment overlapped with new one */
				error = nfsrv_dolocal(vp, lfp, flags,
				    lop->lo_flags, first, lop->lo_end, cfp, p);
				if (error != 0)
					break;
				first = lop->lo_end;
				lop = nlop;
			}
		}
	}
	if (first < end && error == 0)
		/* handle fragment past end of list */
		error = nfsrv_dolocal(vp, lfp, flags, NFSLCK_UNLOCK, first,
		    end, cfp, p);
	return (error);
}

/*
 * Local lock unlock. Unlock all byte ranges that are no longer locked
 * by NFSv4. To do this, unlock any subranges of first-->end that
 * do not overlap with the byte ranges of any lock in the lfp->lf_lock
 * list. This list has all locks for the file held by other
 * <clientid, lockowner> tuples. The list is ordered by increasing
 * lo_first value, but may have entries that overlap each other, for
 * the case of read locks.
 */
static void
nfsrv_localunlock(vnode_t vp, struct nfslockfile *lfp, uint64_t init_first,
    uint64_t init_end, NFSPROC_T *p)
{
	struct nfslock *lop;
	uint64_t first, end, prevfirst;

	first = init_first;
	end = init_end;
	while (first < init_end) {
		/* Loop through all nfs locks, adjusting first and end */
		prevfirst = 0;
		LIST_FOREACH(lop, &lfp->lf_lock, lo_lckfile) {
			KASSERT(prevfirst <= lop->lo_first,
			    ("nfsv4 locks out of order"));
			KASSERT(lop->lo_first < lop->lo_end,
			    ("nfsv4 bogus lock"));
			prevfirst = lop->lo_first;
			if (first >= lop->lo_first &&
			    first < lop->lo_end)
				/*
				 * Overlaps with initial part, so trim
				 * off that initial part by moving first past
				 * it.
				 */
				first = lop->lo_end;
			else if (end > lop->lo_first &&
			    lop->lo_first > first) {
				/*
				 * This lock defines the end of the
				 * segment to unlock, so set end to the
				 * start of it and break out of the loop.
				 */
				end = lop->lo_first;
				break;
			}
			if (first >= end)
				/*
				 * There is no segment left to do, so
				 * break out of this loop and then exit
				 * the outer while() since first will be set
				 * to end, which must equal init_end here.
				 */
				break;
		}
		if (first < end) {
			/* Unlock this segment */
			(void) nfsrv_dolocal(vp, lfp, NFSLCK_UNLOCK,
			    NFSLCK_READ, first, end, NULL, p);
			nfsrv_locallock_commit(lfp, NFSLCK_UNLOCK,
			    first, end);
		}
		/*
		 * Now move past this segment and look for any further
		 * segment in the range, if there is one.
		 */
		first = end;
		end = init_end;
	}
}

/*
 * Do the local lock operation and update the rollback list, as required.
 * Perform the rollback and return the error if nfsvno_advlock() fails.
 */
static int
nfsrv_dolocal(vnode_t vp, struct nfslockfile *lfp, int flags, int oldflags,
    uint64_t first, uint64_t end, struct nfslockconflict *cfp, NFSPROC_T *p)
{
	struct nfsrollback *rlp;
	int error, ltype, oldltype;

	if (flags & NFSLCK_WRITE)
		ltype = F_WRLCK;
	else if (flags & NFSLCK_READ)
		ltype = F_RDLCK;
	else
		ltype = F_UNLCK;
	if (oldflags & NFSLCK_WRITE)
		oldltype = F_WRLCK;
	else if (oldflags & NFSLCK_READ)
		oldltype = F_RDLCK;
	else
		oldltype = F_UNLCK;
	if (ltype == oldltype || (oldltype == F_WRLCK && ltype == F_RDLCK))
		/* nothing to do */
		return (0);
	error = nfsvno_advlock(vp, ltype, first, end, p);
	if (error != 0) {
		if (cfp != NULL) {
			cfp->cl_clientid.lval[0] = 0;
			cfp->cl_clientid.lval[1] = 0;
			cfp->cl_first = 0;
			cfp->cl_end = NFS64BITSSET;
			cfp->cl_flags = NFSLCK_WRITE;
			cfp->cl_ownerlen = 5;
			NFSBCOPY("LOCAL", cfp->cl_owner, 5);
		}
		nfsrv_locallock_rollback(vp, lfp, p);
	} else if (ltype != F_UNLCK) {
		rlp = malloc(sizeof (struct nfsrollback), M_NFSDROLLBACK,
		    M_WAITOK);
		rlp->rlck_first = first;
		rlp->rlck_end = end;
		rlp->rlck_type = oldltype;
		LIST_INSERT_HEAD(&lfp->lf_rollback, rlp, rlck_list);
	}
	return (error);
}

/*
 * Roll back local lock changes and free up the rollback list.
 */
static void
nfsrv_locallock_rollback(vnode_t vp, struct nfslockfile *lfp, NFSPROC_T *p)
{
	struct nfsrollback *rlp, *nrlp;

	LIST_FOREACH_SAFE(rlp, &lfp->lf_rollback, rlck_list, nrlp) {
		(void) nfsvno_advlock(vp, rlp->rlck_type, rlp->rlck_first,
		    rlp->rlck_end, p);
		free(rlp, M_NFSDROLLBACK);
	}
	LIST_INIT(&lfp->lf_rollback);
}

/*
 * Update local lock list and delete rollback list (ie now committed to the
 * local locks). Most of the work is done by the internal function.
 */
static void
nfsrv_locallock_commit(struct nfslockfile *lfp, int flags, uint64_t first,
    uint64_t end)
{
	struct nfsrollback *rlp, *nrlp;
	struct nfslock *new_lop, *other_lop;

	new_lop = malloc(sizeof (struct nfslock), M_NFSDLOCK, M_WAITOK);
	if (flags & (NFSLCK_READ | NFSLCK_WRITE))
		other_lop = malloc(sizeof (struct nfslock), M_NFSDLOCK,
		    M_WAITOK);
	else
		other_lop = NULL;
	new_lop->lo_flags = flags;
	new_lop->lo_first = first;
	new_lop->lo_end = end;
	nfsrv_updatelock(NULL, &new_lop, &other_lop, lfp);
	if (new_lop != NULL)
		free(new_lop, M_NFSDLOCK);
	if (other_lop != NULL)
		free(other_lop, M_NFSDLOCK);

	/* and get rid of the rollback list */
	LIST_FOREACH_SAFE(rlp, &lfp->lf_rollback, rlck_list, nrlp)
		free(rlp, M_NFSDROLLBACK);
	LIST_INIT(&lfp->lf_rollback);
}

/*
 * Lock the struct nfslockfile for local lock updating.
 */
static void
nfsrv_locklf(struct nfslockfile *lfp)
{
	int gotlock;

	/* lf_usecount ensures *lfp won't be free'd */
	lfp->lf_usecount++;
	do {
		gotlock = nfsv4_lock(&lfp->lf_locallock_lck, 1, NULL,
		    NFSSTATEMUTEXPTR, NULL);
	} while (gotlock == 0);
	lfp->lf_usecount--;
}

/*
 * Unlock the struct nfslockfile after local lock updating.
 */
static void
nfsrv_unlocklf(struct nfslockfile *lfp)
{

	nfsv4_unlock(&lfp->lf_locallock_lck, 0);
}

/*
 * Clear out all state for the NFSv4 server.
 * Must be called by a thread that can sleep when no nfsds are running.
 */
void
nfsrv_throwawayallstate(NFSPROC_T *p)
{
	struct nfsclient *clp, *nclp;
	struct nfslockfile *lfp, *nlfp;
	int i;

	/*
	 * For each client, clean out the state and then free the structure.
	 */
	for (i = 0; i < NFSCLIENTHASHSIZE; i++) {
		LIST_FOREACH_SAFE(clp, &nfsclienthash[i], lc_hash, nclp) {
			nfsrv_cleanclient(clp, p);
			nfsrv_freedeleglist(&clp->lc_deleg);
			nfsrv_freedeleglist(&clp->lc_olddeleg);
			free(clp, M_NFSDCLIENT);
		}
	}

	/*
	 * Also, free up any remaining lock file structures.
	 */
	for (i = 0; i < NFSLOCKHASHSIZE; i++) {
		LIST_FOREACH_SAFE(lfp, &nfslockhash[i], lf_hash, nlfp) {
			printf("nfsd unload: fnd a lock file struct\n");
			nfsrv_freenfslockfile(lfp);
		}
	}
}

