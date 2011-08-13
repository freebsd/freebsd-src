/*-
 * Copyright (c) 1989, 1991, 1993, 1995
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
 * Socket operations for use by nfs
 */

#include "opt_inet6.h"
#include "opt_kgssapi.h"
#include "opt_nfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vnode.h>

#include <rpc/rpc.h>

#include <kgssapi/krb5/kcrypto.h>

#include <fs/nfs/nfsport.h>

NFSSTATESPINLOCK;
NFSREQSPINLOCK;
extern struct nfsstats newnfsstats;
extern struct nfsreqhead nfsd_reqq;
extern int nfscl_ticks;
extern void (*ncl_call_invalcaches)(struct vnode *);

static int	nfsrv_gsscallbackson = 0;
static int	nfs_bufpackets = 4;
static int	nfs_reconnects;
static int	nfs3_jukebox_delay = 10;
static int	nfs_skip_wcc_data_onerr = 1;
static int	nfs_keytab_enctype = ETYPE_DES_CBC_CRC;

SYSCTL_DECL(_vfs_newnfs);

SYSCTL_INT(_vfs_newnfs, OID_AUTO, bufpackets, CTLFLAG_RW, &nfs_bufpackets, 0,
    "Buffer reservation size 2 < x < 64");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, reconnects, CTLFLAG_RD, &nfs_reconnects, 0,
    "Number of times the nfs client has had to reconnect");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, nfs3_jukebox_delay, CTLFLAG_RW, &nfs3_jukebox_delay, 0,
    "Number of seconds to delay a retry after receiving EJUKEBOX");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, skip_wcc_data_onerr, CTLFLAG_RW, &nfs_skip_wcc_data_onerr, 0,
    "Disable weak cache consistency checking when server returns an error");
SYSCTL_INT(_vfs_newnfs, OID_AUTO, keytab_enctype, CTLFLAG_RW, &nfs_keytab_enctype, 0,
    "Encryption type for the keytab entry used by nfs");

static void	nfs_down(struct nfsmount *, struct thread *, const char *,
    int, int);
static void	nfs_up(struct nfsmount *, struct thread *, const char *,
    int, int);
static int	nfs_msg(struct thread *, const char *, const char *, int);

struct nfs_cached_auth {
	int		ca_refs; /* refcount, including 1 from the cache */
	uid_t		ca_uid;	 /* uid that corresponds to this auth */
	AUTH		*ca_auth; /* RPC auth handle */
};

static int nfsv2_procid[NFS_V3NPROCS] = {
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
};

/*
 * Initialize sockets and congestion for a new NFS connection.
 * We do not free the sockaddr if error.
 */
int
newnfs_connect(struct nfsmount *nmp, struct nfssockreq *nrp,
    struct ucred *cred, NFSPROC_T *p, int callback_retry_mult)
{
	int rcvreserve, sndreserve;
	int pktscale;
	struct sockaddr *saddr;
	struct ucred *origcred;
	CLIENT *client;
	struct netconfig *nconf;
	struct socket *so;
	int one = 1, retries, error = 0;
	struct thread *td = curthread;

	/*
	 * We need to establish the socket using the credentials of
	 * the mountpoint.  Some parts of this process (such as
	 * sobind() and soconnect()) will use the curent thread's
	 * credential instead of the socket credential.  To work
	 * around this, temporarily change the current thread's
	 * credential to that of the mountpoint.
	 *
	 * XXX: It would be better to explicitly pass the correct
	 * credential to sobind() and soconnect().
	 */
	origcred = td->td_ucred;

	/*
	 * Use the credential in nr_cred, if not NULL.
	 */
	if (nrp->nr_cred != NULL)
		td->td_ucred = nrp->nr_cred;
	else
		td->td_ucred = cred;
	saddr = nrp->nr_nam;

	if (saddr->sa_family == AF_INET)
		if (nrp->nr_sotype == SOCK_DGRAM)
			nconf = getnetconfigent("udp");
		else
			nconf = getnetconfigent("tcp");
	else
		if (nrp->nr_sotype == SOCK_DGRAM)
			nconf = getnetconfigent("udp6");
		else
			nconf = getnetconfigent("tcp6");
			
	pktscale = nfs_bufpackets;
	if (pktscale < 2)
		pktscale = 2;
	if (pktscale > 64)
		pktscale = 64;
	/*
	 * soreserve() can fail if sb_max is too small, so shrink pktscale
	 * and try again if there is an error.
	 * Print a log message suggesting increasing sb_max.
	 * Creating a socket and doing this is necessary since, if the
	 * reservation sizes are too large and will make soreserve() fail,
	 * the connection will work until a large send is attempted and
	 * then it will loop in the krpc code.
	 */
	so = NULL;
	saddr = NFSSOCKADDR(nrp->nr_nam, struct sockaddr *);
	error = socreate(saddr->sa_family, &so, nrp->nr_sotype, 
	    nrp->nr_soproto, td->td_ucred, td);
	if (error) {
		td->td_ucred = origcred;
		goto out;
	}
	do {
	    if (error != 0 && pktscale > 2)
		pktscale--;
	    if (nrp->nr_sotype == SOCK_DGRAM) {
		if (nmp != NULL) {
			sndreserve = (NFS_MAXDGRAMDATA + NFS_MAXPKTHDR) *
			    pktscale;
			rcvreserve = (NFS_MAXDGRAMDATA + NFS_MAXPKTHDR) *
			    pktscale;
		} else {
			sndreserve = rcvreserve = 1024 * pktscale;
		}
	    } else {
		if (nrp->nr_sotype != SOCK_STREAM)
			panic("nfscon sotype");
		if (nmp != NULL) {
			sndreserve = (NFS_MAXBSIZE + NFS_MAXPKTHDR +
			    sizeof (u_int32_t)) * pktscale;
			rcvreserve = (NFS_MAXBSIZE + NFS_MAXPKTHDR +
			    sizeof (u_int32_t)) * pktscale;
		} else {
			sndreserve = rcvreserve = 1024 * pktscale;
		}
	    }
	    error = soreserve(so, sndreserve, rcvreserve);
	} while (error != 0 && pktscale > 2);
	soclose(so);
	if (error) {
		td->td_ucred = origcred;
		goto out;
	}

	client = clnt_reconnect_create(nconf, saddr, nrp->nr_prog,
	    nrp->nr_vers, sndreserve, rcvreserve);
	CLNT_CONTROL(client, CLSET_WAITCHAN, "newnfsreq");
	if (nmp != NULL) {
		if ((nmp->nm_flag & NFSMNT_INT))
			CLNT_CONTROL(client, CLSET_INTERRUPTIBLE, &one);
		if ((nmp->nm_flag & NFSMNT_RESVPORT))
			CLNT_CONTROL(client, CLSET_PRIVPORT, &one);
		if (NFSHASSOFT(nmp))
			retries = nmp->nm_retry;
		else
			retries = INT_MAX;
	} else {
		/*
		 * Three cases:
		 * - Null RPC callback to client
		 * - Non-Null RPC callback to client, wait a little longer
		 * - upcalls to nfsuserd and gssd (clp == NULL)
		 */
		if (callback_retry_mult == 0) {
			retries = NFSV4_UPCALLRETRY;
			CLNT_CONTROL(client, CLSET_PRIVPORT, &one);
		} else {
			retries = NFSV4_CALLBACKRETRY * callback_retry_mult;
		}
	}
	CLNT_CONTROL(client, CLSET_RETRIES, &retries);

	mtx_lock(&nrp->nr_mtx);
	if (nrp->nr_client != NULL) {
		/*
		 * Someone else already connected.
		 */
		CLNT_RELEASE(client);
	} else {
		nrp->nr_client = client;
	}

	/*
	 * Protocols that do not require connections may be optionally left
	 * unconnected for servers that reply from a port other than NFS_PORT.
	 */
	if (nmp == NULL || (nmp->nm_flag & NFSMNT_NOCONN) == 0) {
		mtx_unlock(&nrp->nr_mtx);
		CLNT_CONTROL(client, CLSET_CONNECT, &one);
	} else {
		mtx_unlock(&nrp->nr_mtx);
	}

	/* Restore current thread's credentials. */
	td->td_ucred = origcred;

out:
	NFSEXITCODE(error);
	return (error);
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
newnfs_disconnect(struct nfssockreq *nrp)
{
	CLIENT *client;

	mtx_lock(&nrp->nr_mtx);
	if (nrp->nr_client != NULL) {
		client = nrp->nr_client;
		nrp->nr_client = NULL;
		mtx_unlock(&nrp->nr_mtx);
		rpc_gss_secpurge_call(client);
		CLNT_CLOSE(client);
		CLNT_RELEASE(client);
	} else {
		mtx_unlock(&nrp->nr_mtx);
	}
}

static AUTH *
nfs_getauth(struct nfssockreq *nrp, int secflavour, char *clnt_principal,
    char *srv_principal, gss_OID mech_oid, struct ucred *cred)
{
	rpc_gss_service_t svc;
	AUTH *auth;
#ifdef notyet
	rpc_gss_options_req_t req_options;
#endif

	switch (secflavour) {
	case RPCSEC_GSS_KRB5:
	case RPCSEC_GSS_KRB5I:
	case RPCSEC_GSS_KRB5P:
		if (!mech_oid) {
			if (!rpc_gss_mech_to_oid_call("kerberosv5", &mech_oid))
				return (NULL);
		}
		if (secflavour == RPCSEC_GSS_KRB5)
			svc = rpc_gss_svc_none;
		else if (secflavour == RPCSEC_GSS_KRB5I)
			svc = rpc_gss_svc_integrity;
		else
			svc = rpc_gss_svc_privacy;
#ifdef notyet
		req_options.req_flags = GSS_C_MUTUAL_FLAG;
		req_options.time_req = 0;
		req_options.my_cred = GSS_C_NO_CREDENTIAL;
		req_options.input_channel_bindings = NULL;
		req_options.enc_type = nfs_keytab_enctype;

		auth = rpc_gss_secfind_call(nrp->nr_client, cred,
		    clnt_principal, srv_principal, mech_oid, svc,
		    &req_options);
#else
		/*
		 * Until changes to the rpcsec_gss code are committed,
		 * there is no support for host based initiator
		 * principals. As such, that case cannot yet be handled.
		 */
		if (clnt_principal == NULL)
			auth = rpc_gss_secfind_call(nrp->nr_client, cred,
			    srv_principal, mech_oid, svc);
		else
			auth = NULL;
#endif
		if (auth != NULL)
			return (auth);
		/* fallthrough */
	case AUTH_SYS:
	default:
		return (authunix_create(cred));

	}
}

/*
 * Callback from the RPC code to generate up/down notifications.
 */

struct nfs_feedback_arg {
	struct nfsmount *nf_mount;
	int		nf_lastmsg;	/* last tprintf */
	int		nf_tprintfmsg;
	struct thread	*nf_td;
};

static void
nfs_feedback(int type, int proc, void *arg)
{
	struct nfs_feedback_arg *nf = (struct nfs_feedback_arg *) arg;
	struct nfsmount *nmp = nf->nf_mount;
	struct timeval now;

	getmicrouptime(&now);

	switch (type) {
	case FEEDBACK_REXMIT2:
	case FEEDBACK_RECONNECT:
		if (nf->nf_lastmsg + nmp->nm_tprintf_delay < now.tv_sec) {
			nfs_down(nmp, nf->nf_td,
			    "not responding", 0, NFSSTA_TIMEO);
			nf->nf_tprintfmsg = TRUE;
			nf->nf_lastmsg = now.tv_sec;
		}
		break;

	case FEEDBACK_OK:
		nfs_up(nf->nf_mount, nf->nf_td,
		    "is alive again", NFSSTA_TIMEO, nf->nf_tprintfmsg);
		break;
	}
}

/*
 * newnfs_request - goes something like this
 *	- does the rpc by calling the krpc layer
 *	- break down rpc header and return with nfs reply
 * nb: always frees up nd_mreq mbuf list
 */
int
newnfs_request(struct nfsrv_descript *nd, struct nfsmount *nmp,
    struct nfsclient *clp, struct nfssockreq *nrp, vnode_t vp,
    struct thread *td, struct ucred *cred, u_int32_t prog, u_int32_t vers,
    u_char *retsum, int toplevel, u_int64_t *xidp)
{
	u_int32_t *tl;
	time_t waituntil;
	int i, j, set_uid = 0, set_sigset = 0;
	int trycnt, error = 0, usegssname = 0, secflavour = AUTH_SYS;
	u_int16_t procnum;
	u_int trylater_delay = 1;
	struct nfs_feedback_arg nf;
	struct timeval timo, now;
	AUTH *auth;
	struct rpc_callextra ext;
	enum clnt_stat stat;
	struct nfsreq *rep = NULL;
	char *srv_principal = NULL;
	uid_t saved_uid = (uid_t)-1;
	sigset_t oldset;

	if (xidp != NULL)
		*xidp = 0;
	/* Reject requests while attempting a forced unmount. */
	if (nmp != NULL && (nmp->nm_mountp->mnt_kern_flag & MNTK_UNMOUNTF)) {
		m_freem(nd->nd_mreq);
		return (ESTALE);
	}

	/* For client side interruptible mounts, mask off the signals. */
	if (nmp != NULL && td != NULL && NFSHASINT(nmp)) {
		newnfs_set_sigmask(td, &oldset);
		set_sigset = 1;
	}

	/*
	 * XXX if not already connected call nfs_connect now. Longer
	 * term, change nfs_mount to call nfs_connect unconditionally
	 * and let clnt_reconnect_create handle reconnects.
	 */
	if (nrp->nr_client == NULL)
		newnfs_connect(nmp, nrp, cred, td, 0);

	/*
	 * For a client side mount, nmp is != NULL and clp == NULL. For
	 * server calls (callbacks or upcalls), nmp == NULL.
	 */
	if (clp != NULL) {
		NFSLOCKSTATE();
		if ((clp->lc_flags & LCL_GSS) && nfsrv_gsscallbackson) {
			secflavour = RPCSEC_GSS_KRB5;
			if (nd->nd_procnum != NFSPROC_NULL) {
				if (clp->lc_flags & LCL_GSSINTEGRITY)
					secflavour = RPCSEC_GSS_KRB5I;
				else if (clp->lc_flags & LCL_GSSPRIVACY)
					secflavour = RPCSEC_GSS_KRB5P;
			}
		}
		NFSUNLOCKSTATE();
	} else if (nmp != NULL && NFSHASKERB(nmp) &&
	     nd->nd_procnum != NFSPROC_NULL) {
		if (NFSHASALLGSSNAME(nmp) && nmp->nm_krbnamelen > 0)
			nd->nd_flag |= ND_USEGSSNAME;
		if ((nd->nd_flag & ND_USEGSSNAME) != 0) {
			/*
			 * If there is a client side host based credential,
			 * use that, otherwise use the system uid, if set.
			 */
			if (nmp->nm_krbnamelen > 0) {
				usegssname = 1;
			} else if (nmp->nm_uid != (uid_t)-1) {
				saved_uid = cred->cr_uid;
				cred->cr_uid = nmp->nm_uid;
				set_uid = 1;
			}
		} else if (nmp->nm_krbnamelen == 0 &&
		    nmp->nm_uid != (uid_t)-1 && cred->cr_uid == (uid_t)0) {
			/*
			 * If there is no host based principal name and
			 * the system uid is set and this is root, use the
			 * system uid, since root won't have user
			 * credentials in a credentials cache file.
			 */
			saved_uid = cred->cr_uid;
			cred->cr_uid = nmp->nm_uid;
			set_uid = 1;
		}
		if (NFSHASINTEGRITY(nmp))
			secflavour = RPCSEC_GSS_KRB5I;
		else if (NFSHASPRIVACY(nmp))
			secflavour = RPCSEC_GSS_KRB5P;
		else
			secflavour = RPCSEC_GSS_KRB5;
		srv_principal = NFSMNT_SRVKRBNAME(nmp);
	} else if (nmp != NULL && !NFSHASKERB(nmp) &&
	    nd->nd_procnum != NFSPROC_NULL &&
	    (nd->nd_flag & ND_USEGSSNAME) != 0) {
		/*
		 * Use the uid that did the mount when the RPC is doing
		 * NFSv4 system operations, as indicated by the
		 * ND_USEGSSNAME flag, for the AUTH_SYS case.
		 */
		saved_uid = cred->cr_uid;
		if (nmp->nm_uid != (uid_t)-1)
			cred->cr_uid = nmp->nm_uid;
		else
			cred->cr_uid = 0;
		set_uid = 1;
	}

	if (nmp != NULL) {
		bzero(&nf, sizeof(struct nfs_feedback_arg));
		nf.nf_mount = nmp;
		nf.nf_td = td;
		getmicrouptime(&now);
		nf.nf_lastmsg = now.tv_sec -
		    ((nmp->nm_tprintf_delay)-(nmp->nm_tprintf_initial_delay));
	}

	if (nd->nd_procnum == NFSPROC_NULL)
		auth = authnone_create();
	else if (usegssname)
		auth = nfs_getauth(nrp, secflavour, nmp->nm_krbname,
		    srv_principal, NULL, cred);
	else
		auth = nfs_getauth(nrp, secflavour, NULL,
		    srv_principal, NULL, cred);
	if (set_uid)
		cred->cr_uid = saved_uid;
	if (auth == NULL) {
		m_freem(nd->nd_mreq);
		if (set_sigset)
			newnfs_restore_sigmask(td, &oldset);
		return (EACCES);
	}
	bzero(&ext, sizeof(ext));
	ext.rc_auth = auth;
	if (nmp != NULL) {
		ext.rc_feedback = nfs_feedback;
		ext.rc_feedback_arg = &nf;
	}

	procnum = nd->nd_procnum;
	if ((nd->nd_flag & ND_NFSV4) &&
	    nd->nd_procnum != NFSPROC_NULL &&
	    nd->nd_procnum != NFSV4PROC_CBCOMPOUND)
		procnum = NFSV4PROC_COMPOUND;

	if (nmp != NULL) {
		NFSINCRGLOBAL(newnfsstats.rpcrequests);

		/* Map the procnum to the old NFSv2 one, as required. */
		if ((nd->nd_flag & ND_NFSV2) != 0) {
			if (nd->nd_procnum < NFS_V3NPROCS)
				procnum = nfsv2_procid[nd->nd_procnum];
			else
				procnum = NFSV2PROC_NOOP;
		}

		/*
		 * Now only used for the R_DONTRECOVER case, but until that is
		 * supported within the krpc code, I need to keep a queue of
		 * outstanding RPCs for nfsv4 client requests.
		 */
		if ((nd->nd_flag & ND_NFSV4) && procnum == NFSV4PROC_COMPOUND)
			MALLOC(rep, struct nfsreq *, sizeof(struct nfsreq),
			    M_NFSDREQ, M_WAITOK);
	}
	trycnt = 0;
tryagain:
	if (nmp == NULL) {
		timo.tv_usec = 0;
		if (clp == NULL)
			timo.tv_sec = NFSV4_UPCALLTIMEO;
		else
			timo.tv_sec = NFSV4_CALLBACKTIMEO;
	} else {
		if (nrp->nr_sotype != SOCK_DGRAM) {
			timo.tv_usec = 0;
			if ((nmp->nm_flag & NFSMNT_NFSV4))
				timo.tv_sec = INT_MAX;
			else
				timo.tv_sec = NFS_TCPTIMEO;
		} else {
			timo.tv_sec = nmp->nm_timeo / NFS_HZ;
			timo.tv_usec = (nmp->nm_timeo * 1000000) / NFS_HZ;
		}

		if (rep != NULL) {
			rep->r_flags = 0;
			rep->r_nmp = nmp;
			/*
			 * Chain request into list of outstanding requests.
			 */
			NFSLOCKREQ();
			TAILQ_INSERT_TAIL(&nfsd_reqq, rep, r_chain);
			NFSUNLOCKREQ();
		}
	}

	nd->nd_mrep = NULL;
	stat = CLNT_CALL_MBUF(nrp->nr_client, &ext, procnum, nd->nd_mreq,
	    &nd->nd_mrep, timo);

	if (rep != NULL) {
		/*
		 * RPC done, unlink the request.
		 */
		NFSLOCKREQ();
		TAILQ_REMOVE(&nfsd_reqq, rep, r_chain);
		NFSUNLOCKREQ();
	}

	/*
	 * If there was a successful reply and a tprintf msg.
	 * tprintf a response.
	 */
	if (stat == RPC_SUCCESS) {
		error = 0;
	} else if (stat == RPC_TIMEDOUT) {
		error = ETIMEDOUT;
	} else if (stat == RPC_VERSMISMATCH) {
		error = EOPNOTSUPP;
	} else if (stat == RPC_PROGVERSMISMATCH) {
		error = EPROTONOSUPPORT;
	} else {
		error = EACCES;
	}
	if (error) {
		m_freem(nd->nd_mreq);
		AUTH_DESTROY(auth);
		if (rep != NULL)
			FREE((caddr_t)rep, M_NFSDREQ);
		if (set_sigset)
			newnfs_restore_sigmask(td, &oldset);
		return (error);
	}

	KASSERT(nd->nd_mrep != NULL, ("mrep shouldn't be NULL if no error\n"));

	/*
	 * Search for any mbufs that are not a multiple of 4 bytes long
	 * or with m_data not longword aligned.
	 * These could cause pointer alignment problems, so copy them to
	 * well aligned mbufs.
	 */
	newnfs_realign(&nd->nd_mrep);
	nd->nd_md = nd->nd_mrep;
	nd->nd_dpos = NFSMTOD(nd->nd_md, caddr_t);
	nd->nd_repstat = 0;
	if (nd->nd_procnum != NFSPROC_NULL) {
		/*
		 * and now the actual NFS xdr.
		 */
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		nd->nd_repstat = fxdr_unsigned(u_int32_t, *tl);
		if (nd->nd_repstat != 0) {
			if (((nd->nd_repstat == NFSERR_DELAY ||
			      nd->nd_repstat == NFSERR_GRACE) &&
			     (nd->nd_flag & ND_NFSV4) &&
			     nd->nd_procnum != NFSPROC_DELEGRETURN &&
			     nd->nd_procnum != NFSPROC_SETATTR &&
			     nd->nd_procnum != NFSPROC_READ &&
			     nd->nd_procnum != NFSPROC_WRITE &&
			     nd->nd_procnum != NFSPROC_OPEN &&
			     nd->nd_procnum != NFSPROC_CREATE &&
			     nd->nd_procnum != NFSPROC_OPENCONFIRM &&
			     nd->nd_procnum != NFSPROC_OPENDOWNGRADE &&
			     nd->nd_procnum != NFSPROC_CLOSE &&
			     nd->nd_procnum != NFSPROC_LOCK &&
			     nd->nd_procnum != NFSPROC_LOCKU) ||
			    (nd->nd_repstat == NFSERR_DELAY &&
			     (nd->nd_flag & ND_NFSV4) == 0) ||
			    nd->nd_repstat == NFSERR_RESOURCE) {
				if (trylater_delay > NFS_TRYLATERDEL)
					trylater_delay = NFS_TRYLATERDEL;
				waituntil = NFSD_MONOSEC + trylater_delay;
				while (NFSD_MONOSEC < waituntil)
					(void) nfs_catnap(PZERO, 0, "nfstry");
				trylater_delay *= 2;
				m_freem(nd->nd_mrep);
				nd->nd_mrep = NULL;
				goto tryagain;
			}

			/*
			 * If the File Handle was stale, invalidate the
			 * lookup cache, just in case.
			 * (vp != NULL implies a client side call)
			 */
			if (nd->nd_repstat == ESTALE && vp != NULL) {
				cache_purge(vp);
				if (ncl_call_invalcaches != NULL)
					(*ncl_call_invalcaches)(vp);
			}
		}

		/*
		 * Get rid of the tag, return count, and PUTFH result for V4.
		 */
		if (nd->nd_flag & ND_NFSV4) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			i = fxdr_unsigned(int, *tl);
			error = nfsm_advance(nd, NFSM_RNDUP(i), -1);
			if (error)
				goto nfsmout;
			NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			i = fxdr_unsigned(int, *++tl);

			/*
			 * If the first op's status is non-zero, mark that
			 * there is no more data to process.
			 */
			if (*++tl)
				nd->nd_flag |= ND_NOMOREDATA;

			/*
			 * If the first op is Putfh, throw its results away
			 * and toss the op# and status for the first op.
			 */
			if (nmp != NULL && i == NFSV4OP_PUTFH && *tl == 0) {
				NFSM_DISSECT(tl,u_int32_t *,2 * NFSX_UNSIGNED);
				i = fxdr_unsigned(int, *tl++);
				j = fxdr_unsigned(int, *tl);
				/*
				 * All Compounds that do an Op that must
				 * be in sequence consist of NFSV4OP_PUTFH
				 * followed by one of these. As such, we
				 * can determine if the seqid# should be
				 * incremented, here.
				 */
				if ((i == NFSV4OP_OPEN ||
				     i == NFSV4OP_OPENCONFIRM ||
				     i == NFSV4OP_OPENDOWNGRADE ||
				     i == NFSV4OP_CLOSE ||
				     i == NFSV4OP_LOCK ||
				     i == NFSV4OP_LOCKU) &&
				    (j == 0 ||
				     (j != NFSERR_STALECLIENTID &&
				      j != NFSERR_STALESTATEID &&
				      j != NFSERR_BADSTATEID &&
				      j != NFSERR_BADSEQID &&
				      j != NFSERR_BADXDR &&	 
				      j != NFSERR_RESOURCE &&
				      j != NFSERR_NOFILEHANDLE)))		 
					nd->nd_flag |= ND_INCRSEQID;
				/*
				 * If the first op's status is non-zero, mark
				 * that there is no more data to process.
				 */
				if (j)
					nd->nd_flag |= ND_NOMOREDATA;
			}

			/*
			 * If R_DONTRECOVER is set, replace the stale error
			 * reply, so that recovery isn't initiated.
			 */
			if ((nd->nd_repstat == NFSERR_STALECLIENTID ||
			     nd->nd_repstat == NFSERR_STALESTATEID) &&
			    rep != NULL && (rep->r_flags & R_DONTRECOVER))
				nd->nd_repstat = NFSERR_STALEDONTRECOVER;
		}
	}

	m_freem(nd->nd_mreq);
	AUTH_DESTROY(auth);
	if (rep != NULL)
		FREE((caddr_t)rep, M_NFSDREQ);
	if (set_sigset)
		newnfs_restore_sigmask(td, &oldset);
	return (0);
nfsmout:
	mbuf_freem(nd->nd_mrep);
	mbuf_freem(nd->nd_mreq);
	AUTH_DESTROY(auth);
	if (rep != NULL)
		FREE((caddr_t)rep, M_NFSDREQ);
	if (set_sigset)
		newnfs_restore_sigmask(td, &oldset);
	return (error);
}

/*
 * Mark all of an nfs mount's outstanding requests with R_SOFTTERM and
 * wait for all requests to complete. This is used by forced unmounts
 * to terminate any outstanding RPCs.
 */
int
newnfs_nmcancelreqs(struct nfsmount *nmp)
{

	if (nmp->nm_sockreq.nr_client != NULL)
		CLNT_CLOSE(nmp->nm_sockreq.nr_client);
	return (0);
}

/*
 * Any signal that can interrupt an NFS operation in an intr mount
 * should be added to this set. SIGSTOP and SIGKILL cannot be masked.
 */
int newnfs_sig_set[] = {
	SIGINT,
	SIGTERM,
	SIGHUP,
	SIGKILL,
	SIGSTOP,
	SIGQUIT
};

/*
 * Check to see if one of the signals in our subset is pending on
 * the process (in an intr mount).
 */
static int
nfs_sig_pending(sigset_t set)
{
	int i;
	
	for (i = 0 ; i < sizeof(newnfs_sig_set)/sizeof(int) ; i++)
		if (SIGISMEMBER(set, newnfs_sig_set[i]))
			return (1);
	return (0);
}
 
/*
 * The set/restore sigmask functions are used to (temporarily) overwrite
 * the process p_sigmask during an RPC call (for example). These are also
 * used in other places in the NFS client that might tsleep().
 */
void
newnfs_set_sigmask(struct thread *td, sigset_t *oldset)
{
	sigset_t newset;
	int i;
	struct proc *p;
	
	SIGFILLSET(newset);
	if (td == NULL)
		td = curthread; /* XXX */
	p = td->td_proc;
	/* Remove the NFS set of signals from newset */
	PROC_LOCK(p);
	mtx_lock(&p->p_sigacts->ps_mtx);
	for (i = 0 ; i < sizeof(newnfs_sig_set)/sizeof(int) ; i++) {
		/*
		 * But make sure we leave the ones already masked
		 * by the process, ie. remove the signal from the
		 * temporary signalmask only if it wasn't already
		 * in p_sigmask.
		 */
		if (!SIGISMEMBER(td->td_sigmask, newnfs_sig_set[i]) &&
		    !SIGISMEMBER(p->p_sigacts->ps_sigignore, newnfs_sig_set[i]))
			SIGDELSET(newset, newnfs_sig_set[i]);
	}
	mtx_unlock(&p->p_sigacts->ps_mtx);
	PROC_UNLOCK(p);
	kern_sigprocmask(td, SIG_SETMASK, &newset, oldset, 0);
}

void
newnfs_restore_sigmask(struct thread *td, sigset_t *set)
{
	if (td == NULL)
		td = curthread; /* XXX */
	kern_sigprocmask(td, SIG_SETMASK, set, NULL, 0);
}

/*
 * NFS wrapper to msleep(), that shoves a new p_sigmask and restores the
 * old one after msleep() returns.
 */
int
newnfs_msleep(struct thread *td, void *ident, struct mtx *mtx, int priority, char *wmesg, int timo)
{
	sigset_t oldset;
	int error;
	struct proc *p;
	
	if ((priority & PCATCH) == 0)
		return msleep(ident, mtx, priority, wmesg, timo);
	if (td == NULL)
		td = curthread; /* XXX */
	newnfs_set_sigmask(td, &oldset);
	error = msleep(ident, mtx, priority, wmesg, timo);
	newnfs_restore_sigmask(td, &oldset);
	p = td->td_proc;
	return (error);
}

/*
 * Test for a termination condition pending on the process.
 * This is used for NFSMNT_INT mounts.
 */
int
newnfs_sigintr(struct nfsmount *nmp, struct thread *td)
{
	struct proc *p;
	sigset_t tmpset;
	
	/* Terminate all requests while attempting a forced unmount. */
	if (nmp->nm_mountp->mnt_kern_flag & MNTK_UNMOUNTF)
		return (EIO);
	if (!(nmp->nm_flag & NFSMNT_INT))
		return (0);
	if (td == NULL)
		return (0);
	p = td->td_proc;
	PROC_LOCK(p);
	tmpset = p->p_siglist;
	SIGSETOR(tmpset, td->td_siglist);
	SIGSETNAND(tmpset, td->td_sigmask);
	mtx_lock(&p->p_sigacts->ps_mtx);
	SIGSETNAND(tmpset, p->p_sigacts->ps_sigignore);
	mtx_unlock(&p->p_sigacts->ps_mtx);
	if ((SIGNOTEMPTY(p->p_siglist) || SIGNOTEMPTY(td->td_siglist))
	    && nfs_sig_pending(tmpset)) {
		PROC_UNLOCK(p);
		return (EINTR);
	}
	PROC_UNLOCK(p);
	return (0);
}

static int
nfs_msg(struct thread *td, const char *server, const char *msg, int error)
{
	struct proc *p;

	p = td ? td->td_proc : NULL;
	if (error) {
		tprintf(p, LOG_INFO, "newnfs server %s: %s, error %d\n",
		    server, msg, error);
	} else {
		tprintf(p, LOG_INFO, "newnfs server %s: %s\n", server, msg);
	}
	return (0);
}

static void
nfs_down(struct nfsmount *nmp, struct thread *td, const char *msg,
    int error, int flags)
{
	if (nmp == NULL)
		return;
	mtx_lock(&nmp->nm_mtx);
	if ((flags & NFSSTA_TIMEO) && !(nmp->nm_state & NFSSTA_TIMEO)) {
		nmp->nm_state |= NFSSTA_TIMEO;
		mtx_unlock(&nmp->nm_mtx);
		vfs_event_signal(&nmp->nm_mountp->mnt_stat.f_fsid,
		    VQ_NOTRESP, 0);
	} else
		mtx_unlock(&nmp->nm_mtx);
	mtx_lock(&nmp->nm_mtx);
	if ((flags & NFSSTA_LOCKTIMEO) && !(nmp->nm_state & NFSSTA_LOCKTIMEO)) {
		nmp->nm_state |= NFSSTA_LOCKTIMEO;
		mtx_unlock(&nmp->nm_mtx);
		vfs_event_signal(&nmp->nm_mountp->mnt_stat.f_fsid,
		    VQ_NOTRESPLOCK, 0);
	} else
		mtx_unlock(&nmp->nm_mtx);
	nfs_msg(td, nmp->nm_mountp->mnt_stat.f_mntfromname, msg, error);
}

static void
nfs_up(struct nfsmount *nmp, struct thread *td, const char *msg,
    int flags, int tprintfmsg)
{
	if (nmp == NULL)
		return;
	if (tprintfmsg) {
		nfs_msg(td, nmp->nm_mountp->mnt_stat.f_mntfromname, msg, 0);
	}

	mtx_lock(&nmp->nm_mtx);
	if ((flags & NFSSTA_TIMEO) && (nmp->nm_state & NFSSTA_TIMEO)) {
		nmp->nm_state &= ~NFSSTA_TIMEO;
		mtx_unlock(&nmp->nm_mtx);
		vfs_event_signal(&nmp->nm_mountp->mnt_stat.f_fsid,
		    VQ_NOTRESP, 1);
	} else
		mtx_unlock(&nmp->nm_mtx);
	
	mtx_lock(&nmp->nm_mtx);
	if ((flags & NFSSTA_LOCKTIMEO) && (nmp->nm_state & NFSSTA_LOCKTIMEO)) {
		nmp->nm_state &= ~NFSSTA_LOCKTIMEO;
		mtx_unlock(&nmp->nm_mtx);
		vfs_event_signal(&nmp->nm_mountp->mnt_stat.f_fsid,
		    VQ_NOTRESPLOCK, 1);
	} else
		mtx_unlock(&nmp->nm_mtx);
}

