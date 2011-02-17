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
 *	@(#)nfs_socket.c	8.5 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Socket operations for use by nfs
 */

#include "opt_inet6.h"
#include "opt_kdtrace.h"
#include "opt_kgssapi.h"

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

#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsnode.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

dtrace_nfsclient_nfs23_start_probe_func_t
    dtrace_nfsclient_nfs23_start_probe;

dtrace_nfsclient_nfs23_done_probe_func_t
    dtrace_nfsclient_nfs23_done_probe;

/*
 * Registered probes by RPC type.
 */
uint32_t	nfsclient_nfs2_start_probes[NFS_NPROCS];
uint32_t	nfsclient_nfs2_done_probes[NFS_NPROCS];

uint32_t	nfsclient_nfs3_start_probes[NFS_NPROCS];
uint32_t	nfsclient_nfs3_done_probes[NFS_NPROCS];
#endif

static int	nfs_bufpackets = 4;
static int	nfs_reconnects;
static int	nfs3_jukebox_delay = 10;
static int	nfs_skip_wcc_data_onerr = 1;
static int	fake_wchan;

SYSCTL_DECL(_vfs_nfs);

SYSCTL_INT(_vfs_nfs, OID_AUTO, bufpackets, CTLFLAG_RW, &nfs_bufpackets, 0,
    "Buffer reservation size 2 < x < 64");
SYSCTL_INT(_vfs_nfs, OID_AUTO, reconnects, CTLFLAG_RD, &nfs_reconnects, 0,
    "Number of times the nfs client has had to reconnect");
SYSCTL_INT(_vfs_nfs, OID_AUTO, nfs3_jukebox_delay, CTLFLAG_RW,
    &nfs3_jukebox_delay, 0,
    "Number of seconds to delay a retry after receiving EJUKEBOX");
SYSCTL_INT(_vfs_nfs, OID_AUTO, skip_wcc_data_onerr, CTLFLAG_RW,
    &nfs_skip_wcc_data_onerr, 0,
    "Disable weak cache consistency checking when server returns an error");

static void	nfs_down(struct nfsmount *, struct thread *, const char *,
    int, int);
static void	nfs_up(struct nfsmount *, struct thread *, const char *,
    int, int);
static int	nfs_msg(struct thread *, const char *, const char *, int);

extern int nfsv2_procid[];

struct nfs_cached_auth {
	int		ca_refs; /* refcount, including 1 from the cache */
	uid_t		ca_uid;	 /* uid that corresponds to this auth */
	AUTH		*ca_auth; /* RPC auth handle */
};

/*
 * RTT estimator
 */

static enum nfs_rto_timer_t nfs_proct[NFS_NPROCS] = {
	NFS_DEFAULT_TIMER,	/* NULL */
	NFS_GETATTR_TIMER,	/* GETATTR */
	NFS_DEFAULT_TIMER,	/* SETATTR */
	NFS_LOOKUP_TIMER,	/* LOOKUP */
	NFS_GETATTR_TIMER,	/* ACCESS */
	NFS_READ_TIMER,		/* READLINK */
	NFS_READ_TIMER,		/* READ */
	NFS_WRITE_TIMER,	/* WRITE */
	NFS_DEFAULT_TIMER,	/* CREATE */
	NFS_DEFAULT_TIMER,	/* MKDIR */
	NFS_DEFAULT_TIMER,	/* SYMLINK */
	NFS_DEFAULT_TIMER,	/* MKNOD */
	NFS_DEFAULT_TIMER,	/* REMOVE */
	NFS_DEFAULT_TIMER,	/* RMDIR */
	NFS_DEFAULT_TIMER,	/* RENAME */
	NFS_DEFAULT_TIMER,	/* LINK */
	NFS_READ_TIMER,		/* READDIR */
	NFS_READ_TIMER,		/* READDIRPLUS */
	NFS_DEFAULT_TIMER,	/* FSSTAT */
	NFS_DEFAULT_TIMER,	/* FSINFO */
	NFS_DEFAULT_TIMER,	/* PATHCONF */
	NFS_DEFAULT_TIMER,	/* COMMIT */
	NFS_DEFAULT_TIMER,	/* NOOP */
};

/*
 * Choose the correct RTT timer for this NFS procedure.
 */
static inline enum nfs_rto_timer_t
nfs_rto_timer(u_int32_t procnum)
{

	return (nfs_proct[procnum]);
}

/*
 * Initialize the RTT estimator state for a new mount point.
 */
static void
nfs_init_rtt(struct nfsmount *nmp)
{
	int i;

	for (i = 0; i < NFS_MAX_TIMER; i++) {
		nmp->nm_timers[i].rt_srtt = hz;
		nmp->nm_timers[i].rt_deviate = 0;
		nmp->nm_timers[i].rt_rtxcur = hz;
	}
}

/*
 * Initialize sockets and congestion for a new NFS connection.
 * We do not free the sockaddr if error.
 */
int
nfs_connect(struct nfsmount *nmp)
{
	int rcvreserve, sndreserve;
	int pktscale;
	struct sockaddr *saddr;
	struct ucred *origcred;
	struct thread *td = curthread;
	CLIENT *client;
	struct netconfig *nconf;
	rpcvers_t vers;
	int one = 1, retries;

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
	td->td_ucred = nmp->nm_mountp->mnt_cred;
	saddr = nmp->nm_nam;

	vers = NFS_VER2;
	if (nmp->nm_flag & NFSMNT_NFSV3)
		vers = NFS_VER3;
	else if (nmp->nm_flag & NFSMNT_NFSV4)
		vers = NFS_VER4;
	if (saddr->sa_family == AF_INET)
		if (nmp->nm_sotype == SOCK_DGRAM)
			nconf = getnetconfigent("udp");
		else
			nconf = getnetconfigent("tcp");
	else
		if (nmp->nm_sotype == SOCK_DGRAM)
			nconf = getnetconfigent("udp6");
		else
			nconf = getnetconfigent("tcp6");

	/*
	 * Get buffer reservation size from sysctl, but impose reasonable
	 * limits.
	 */
	pktscale = nfs_bufpackets;
	if (pktscale < 2)
		pktscale = 2;
	if (pktscale > 64)
		pktscale = 64;
	mtx_lock(&nmp->nm_mtx);
	if (nmp->nm_sotype == SOCK_DGRAM) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * pktscale;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * pktscale;
	} else if (nmp->nm_sotype == SOCK_SEQPACKET) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * pktscale;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * pktscale;
	} else {
		if (nmp->nm_sotype != SOCK_STREAM)
			panic("nfscon sotype");
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * pktscale;
		rcvreserve = (nmp->nm_rsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * pktscale;
	}
	mtx_unlock(&nmp->nm_mtx);

	client = clnt_reconnect_create(nconf, saddr, NFS_PROG, vers,
	    sndreserve, rcvreserve);
	CLNT_CONTROL(client, CLSET_WAITCHAN, "nfsreq");
	if (nmp->nm_flag & NFSMNT_INT)
		CLNT_CONTROL(client, CLSET_INTERRUPTIBLE, &one);
	if (nmp->nm_flag & NFSMNT_RESVPORT)
		CLNT_CONTROL(client, CLSET_PRIVPORT, &one);
	if (nmp->nm_flag & NFSMNT_SOFT)
		retries = nmp->nm_retry;
	else
		retries = INT_MAX;
	CLNT_CONTROL(client, CLSET_RETRIES, &retries);

	mtx_lock(&nmp->nm_mtx);
	if (nmp->nm_client) {
		/*
		 * Someone else already connected.
		 */
		CLNT_RELEASE(client);
	} else
		nmp->nm_client = client;

	/*
	 * Protocols that do not require connections may be optionally left
	 * unconnected for servers that reply from a port other than NFS_PORT.
	 */
	if (!(nmp->nm_flag & NFSMNT_NOCONN)) {
		mtx_unlock(&nmp->nm_mtx);
		CLNT_CONTROL(client, CLSET_CONNECT, &one);
	} else
		mtx_unlock(&nmp->nm_mtx);

	/* Restore current thread's credentials. */
	td->td_ucred = origcred;

	mtx_lock(&nmp->nm_mtx);
	/* Initialize other non-zero congestion variables. */
	nfs_init_rtt(nmp);
	mtx_unlock(&nmp->nm_mtx);
	return (0);
}

/*
 * NFS disconnect.  Clean up and unlink.
 */
void
nfs_disconnect(struct nfsmount *nmp)
{
	CLIENT *client;

	mtx_lock(&nmp->nm_mtx);
	if (nmp->nm_client) {
		client = nmp->nm_client;
		nmp->nm_client = NULL;
		mtx_unlock(&nmp->nm_mtx);
#ifdef KGSSAPI
		rpc_gss_secpurge(client);
#endif
		CLNT_CLOSE(client);
		CLNT_RELEASE(client);
	} else
		mtx_unlock(&nmp->nm_mtx);
}

void
nfs_safedisconnect(struct nfsmount *nmp)
{

	nfs_disconnect(nmp);
}

static AUTH *
nfs_getauth(struct nfsmount *nmp, struct ucred *cred)
{
#ifdef KGSSAPI
	rpc_gss_service_t svc;
	AUTH *auth;
#endif

	switch (nmp->nm_secflavor) {
#ifdef KGSSAPI
	case RPCSEC_GSS_KRB5:
	case RPCSEC_GSS_KRB5I:
	case RPCSEC_GSS_KRB5P:
		if (!nmp->nm_mech_oid)
			if (!rpc_gss_mech_to_oid("kerberosv5",
			    &nmp->nm_mech_oid))
				return (NULL);
		if (nmp->nm_secflavor == RPCSEC_GSS_KRB5)
			svc = rpc_gss_svc_none;
		else if (nmp->nm_secflavor == RPCSEC_GSS_KRB5I)
			svc = rpc_gss_svc_integrity;
		else
			svc = rpc_gss_svc_privacy;
		auth = rpc_gss_secfind(nmp->nm_client, cred,
		    nmp->nm_principal, nmp->nm_mech_oid, svc);
		if (auth)
			return (auth);
		/* fallthrough */
#endif
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
 * nfs_request - goes something like this
 *	- fill in request struct
 *	- links it into list
 *	- calls nfs_send() for first transmit
 *	- calls nfs_receive() to get reply
 *	- break down rpc header and return with nfs reply pointed to
 *	  by mrep or error
 * nb: always frees up mreq mbuf list
 */
int
nfs_request(struct vnode *vp, struct mbuf *mreq, int procnum,
    struct thread *td, struct ucred *cred, struct mbuf **mrp,
    struct mbuf **mdp, caddr_t *dposp)
{
	struct mbuf *mrep;
	u_int32_t *tl;
	struct nfsmount *nmp;
	struct mbuf *md;
	time_t waituntil;
	caddr_t dpos;
	int error = 0;
	struct timeval now;
	AUTH *auth = NULL;
	enum nfs_rto_timer_t timer;
	struct nfs_feedback_arg nf;
	struct rpc_callextra ext;
	enum clnt_stat stat;
	struct timeval timo;

	/* Reject requests while attempting a forced unmount. */
	if (vp->v_mount->mnt_kern_flag & MNTK_UNMOUNTF) {
		m_freem(mreq);
		return (ESTALE);
	}
	nmp = VFSTONFS(vp->v_mount);
	bzero(&nf, sizeof(struct nfs_feedback_arg));
	nf.nf_mount = nmp;
	nf.nf_td = td;
	getmicrouptime(&now);
	nf.nf_lastmsg = now.tv_sec -
	    ((nmp->nm_tprintf_delay) - (nmp->nm_tprintf_initial_delay));

	/*
	 * XXX if not already connected call nfs_connect now.  Longer
	 * term, change nfs_mount to call nfs_connect unconditionally
	 * and let clnt_reconnect_create handle reconnects.
	 */
	if (!nmp->nm_client)
		nfs_connect(nmp);

	auth = nfs_getauth(nmp, cred);
	if (!auth) {
		m_freem(mreq);
		return (EACCES);
	}
	bzero(&ext, sizeof(ext));
	ext.rc_auth = auth;

	ext.rc_feedback = nfs_feedback;
	ext.rc_feedback_arg = &nf;

	/*
	 * Use a conservative timeout for RPCs other than getattr,
	 * lookup, read or write.  The justification for doing "other"
	 * this way is that these RPCs happen so infrequently that
	 * timer est. would probably be stale.  Also, since many of
	 * these RPCs are non-idempotent, a conservative timeout is
	 * desired.
	 */
	timer = nfs_rto_timer(procnum);
	if (timer != NFS_DEFAULT_TIMER)
		ext.rc_timers = &nmp->nm_timers[timer - 1];
	else
		ext.rc_timers = NULL;

#ifdef KDTRACE_HOOKS
	if (dtrace_nfsclient_nfs23_start_probe != NULL) {
		uint32_t probe_id;
		int probe_procnum;

		if (nmp->nm_flag & NFSMNT_NFSV3) {
			probe_id = nfsclient_nfs3_start_probes[procnum];
			probe_procnum = procnum;
		} else {
			probe_id = nfsclient_nfs2_start_probes[procnum];
			probe_procnum = nfsv2_procid[procnum];
		}
		if (probe_id != 0)
			(dtrace_nfsclient_nfs23_start_probe)(probe_id, vp,
			    mreq, cred, probe_procnum);
	}
#endif

	nfsstats.rpcrequests++;
tryagain:
	timo.tv_sec = nmp->nm_timeo / NFS_HZ;
	timo.tv_usec = (nmp->nm_timeo * 1000000) / NFS_HZ;
	mrep = NULL;
	stat = CLNT_CALL_MBUF(nmp->nm_client, &ext,
	    (nmp->nm_flag & NFSMNT_NFSV3) ? procnum : nfsv2_procid[procnum],
	    mreq, &mrep, timo);

	/*
	 * If there was a successful reply and a tprintf msg.
	 * tprintf a response.
	 */
	if (stat == RPC_SUCCESS)
		error = 0;
	else if (stat == RPC_TIMEDOUT)
		error = ETIMEDOUT;
	else if (stat == RPC_VERSMISMATCH)
		error = EOPNOTSUPP;
	else if (stat == RPC_PROGVERSMISMATCH)
		error = EPROTONOSUPPORT;
	else
		error = EACCES;
	if (error)
		goto nfsmout;

	KASSERT(mrep != NULL, ("mrep shouldn't be NULL if no error\n"));

	/*
	 * Search for any mbufs that are not a multiple of 4 bytes long
	 * or with m_data not longword aligned.
	 * These could cause pointer alignment problems, so copy them to
	 * well aligned mbufs.
	 */
	error = nfs_realign(&mrep, M_DONTWAIT);
	if (error == ENOMEM) {
		m_freem(mrep);
		AUTH_DESTROY(auth);
		return (error);
	}

	md = mrep;
	dpos = mtod(mrep, caddr_t);
	tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
	if (*tl != 0) {
		error = fxdr_unsigned(int, *tl);
		if ((nmp->nm_flag & NFSMNT_NFSV3) &&
		    error == NFSERR_TRYLATER) {
			m_freem(mrep);
			error = 0;
			waituntil = time_second + nfs3_jukebox_delay;
			while (time_second < waituntil)
				(void)tsleep(&fake_wchan, PSOCK, "nqnfstry",
				    hz);
			goto tryagain;
		}

		/*
		 * If the File Handle was stale, invalidate the lookup
		 * cache, just in case.
		 */
		if (error == ESTALE)
			nfs_purgecache(vp);
		/*
		 * Skip wcc data on NFS errors for now.  NetApp filers
		 * return corrupt postop attrs in the wcc data for NFS
		 * err EROFS.  Not sure if they could return corrupt
		 * postop attrs for others errors.
		 */
		if ((nmp->nm_flag & NFSMNT_NFSV3) &&
		    !nfs_skip_wcc_data_onerr) {
			*mrp = mrep;
			*mdp = md;
			*dposp = dpos;
			error |= NFSERR_RETERR;
		} else
			m_freem(mrep);
		goto nfsmout;
	}

#ifdef KDTRACE_HOOKS
	if (dtrace_nfsclient_nfs23_done_probe != NULL) {
		uint32_t probe_id;
		int probe_procnum;

		if (nmp->nm_flag & NFSMNT_NFSV3) {
			probe_id = nfsclient_nfs3_done_probes[procnum];
			probe_procnum = procnum;
		} else {
			probe_id = nfsclient_nfs2_done_probes[procnum];
			probe_procnum = (nmp->nm_flag & NFSMNT_NFSV3) ?
			    procnum : nfsv2_procid[procnum];
		}
		if (probe_id != 0)
			(dtrace_nfsclient_nfs23_done_probe)(probe_id, vp,
			    mreq, cred, probe_procnum, 0);
	}
#endif
	m_freem(mreq);
	*mrp = mrep;
	*mdp = md;
	*dposp = dpos;
	AUTH_DESTROY(auth);
	return (0);

nfsmout:
#ifdef KDTRACE_HOOKS
	if (dtrace_nfsclient_nfs23_done_probe != NULL) {
		uint32_t probe_id;
		int probe_procnum;

		if (nmp->nm_flag & NFSMNT_NFSV3) {
			probe_id = nfsclient_nfs3_done_probes[procnum];
			probe_procnum = procnum;
		} else {
			probe_id = nfsclient_nfs2_done_probes[procnum];
			probe_procnum = (nmp->nm_flag & NFSMNT_NFSV3) ?
			    procnum : nfsv2_procid[procnum];
		}
		if (probe_id != 0)
			(dtrace_nfsclient_nfs23_done_probe)(probe_id, vp,
			    mreq, cred, probe_procnum, error);
	}
#endif
	m_freem(mreq);
	if (auth)
		AUTH_DESTROY(auth);
	return (error);
}

/*
 * Mark all of an nfs mount's outstanding requests with R_SOFTTERM and
 * wait for all requests to complete.  This is used by forced unmounts
 * to terminate any outstanding RPCs.
 */
int
nfs_nmcancelreqs(struct nfsmount *nmp)
{

	if (nmp->nm_client)
		CLNT_CLOSE(nmp->nm_client);
	return (0);
}

/*
 * Any signal that can interrupt an NFS operation in an intr mount
 * should be added to this set.  SIGSTOP and SIGKILL cannot be masked.
 */
int nfs_sig_set[] = {
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

	for (i = 0 ; i < sizeof(nfs_sig_set)/sizeof(int) ; i++)
		if (SIGISMEMBER(set, nfs_sig_set[i]))
			return (1);
	return (0);
}

/*
 * The set/restore sigmask functions are used to (temporarily) overwrite
 * the process p_sigmask during an RPC call (for example).  These are also
 * used in other places in the NFS client that might tsleep().
 */
void
nfs_set_sigmask(struct thread *td, sigset_t *oldset)
{
	sigset_t newset;
	int i;
	struct proc *p;

	SIGFILLSET(newset);
	if (td == NULL)
		td = curthread; /* XXX */
	p = td->td_proc;
	/* Remove the NFS set of signals from newset. */
	PROC_LOCK(p);
	mtx_lock(&p->p_sigacts->ps_mtx);
	for (i = 0 ; i < sizeof(nfs_sig_set)/sizeof(int) ; i++) {
		/*
		 * But make sure we leave the ones already masked
		 * by the process, i.e. remove the signal from the
		 * temporary signalmask only if it wasn't already
		 * in p_sigmask.
		 */
		if (!SIGISMEMBER(td->td_sigmask, nfs_sig_set[i]) &&
		    !SIGISMEMBER(p->p_sigacts->ps_sigignore, nfs_sig_set[i]))
			SIGDELSET(newset, nfs_sig_set[i]);
	}
	mtx_unlock(&p->p_sigacts->ps_mtx);
	PROC_UNLOCK(p);
	kern_sigprocmask(td, SIG_SETMASK, &newset, oldset, 0);
}

void
nfs_restore_sigmask(struct thread *td, sigset_t *set)
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
nfs_msleep(struct thread *td, void *ident, struct mtx *mtx, int priority,
    char *wmesg, int timo)
{
	sigset_t oldset;
	int error;
	struct proc *p;

	if ((priority & PCATCH) == 0)
		return msleep(ident, mtx, priority, wmesg, timo);
	if (td == NULL)
		td = curthread; /* XXX */
	nfs_set_sigmask(td, &oldset);
	error = msleep(ident, mtx, priority, wmesg, timo);
	nfs_restore_sigmask(td, &oldset);
	p = td->td_proc;
	return (error);
}

/*
 * Test for a termination condition pending on the process.
 * This is used for NFSMNT_INT mounts.
 */
int
nfs_sigintr(struct nfsmount *nmp, struct thread *td)
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
	if (error)
		tprintf(p, LOG_INFO, "nfs server %s: %s, error %d\n", server,
		    msg, error);
	else
		tprintf(p, LOG_INFO, "nfs server %s: %s\n", server, msg);
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
	if ((flags & NFSSTA_LOCKTIMEO) &&
	    !(nmp->nm_state & NFSSTA_LOCKTIMEO)) {
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
	if (tprintfmsg)
		nfs_msg(td, nmp->nm_mountp->mnt_stat.f_mntfromname, msg, 0);

	mtx_lock(&nmp->nm_mtx);
	if ((flags & NFSSTA_TIMEO) && (nmp->nm_state & NFSSTA_TIMEO)) {
		nmp->nm_state &= ~NFSSTA_TIMEO;
		mtx_unlock(&nmp->nm_mtx);
		vfs_event_signal(&nmp->nm_mountp->mnt_stat.f_fsid,
		    VQ_NOTRESP, 1);
	} else
		mtx_unlock(&nmp->nm_mtx);

	mtx_lock(&nmp->nm_mtx);
	if ((flags & NFSSTA_LOCKTIMEO) &&
	    (nmp->nm_state & NFSSTA_LOCKTIMEO)) {
		nmp->nm_state &= ~NFSSTA_LOCKTIMEO;
		mtx_unlock(&nmp->nm_mtx);
		vfs_event_signal(&nmp->nm_mountp->mnt_stat.f_fsid,
		    VQ_NOTRESPLOCK, 1);
	} else
		mtx_unlock(&nmp->nm_mtx);
}
