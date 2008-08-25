/*-
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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
 */

#include "opt_inet6.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#if __FreeBSD_version >= 700000
#include <sys/priv.h>
#endif
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>

#include <nlm/nlm_prot.h>
#include <nlm/sm_inter.h>
#include <nlm/nlm.h>
#include <rpc/rpc_com.h>
#include <rpc/rpcb_prot.h>

MALLOC_DEFINE(M_NLM, "NLM", "Network Lock Manager");

/*
 * If a host is inactive (and holds no locks) for this amount of
 * seconds, we consider it idle and stop tracking it.
 */
#define NLM_IDLE_TIMEOUT	30

/*
 * We check the host list for idle every few seconds.
 */
#define NLM_IDLE_PERIOD		5

/*
 * Support for sysctl vfs.nlm.sysid
 */
SYSCTL_NODE(_vfs, OID_AUTO, nlm, CTLFLAG_RW, NULL, "Network Lock Manager");
SYSCTL_NODE(_vfs_nlm, OID_AUTO, sysid, CTLFLAG_RW, NULL, "");

/*
 * Syscall hooks
 */
static int nlm_syscall_offset = SYS_nlm_syscall;
static struct sysent nlm_syscall_prev_sysent;
#if __FreeBSD_version < 700000
static struct sysent nlm_syscall_sysent = {
	(sizeof(struct nlm_syscall_args) / sizeof(register_t)) | SYF_MPSAFE,
	(sy_call_t *) nlm_syscall
};
#else
MAKE_SYSENT(nlm_syscall);
#endif
static bool_t nlm_syscall_registered = FALSE;

/*
 * Debug level passed in from userland. We also support a sysctl hook
 * so that it can be changed on a live system.
 */
static int nlm_debug_level;
SYSCTL_INT(_debug, OID_AUTO, nlm_debug, CTLFLAG_RW, &nlm_debug_level, 0, "");

/*
 * Grace period handling. The value of nlm_grace_threshold is the
 * value of time_uptime after which we are serving requests normally.
 */
static time_t nlm_grace_threshold;

/*
 * We check for idle hosts if time_uptime is greater than
 * nlm_next_idle_check,
 */
static time_t nlm_next_idle_check;

/*
 * A socket to use for RPC - shared by all IPv4 RPC clients.
 */
static struct socket *nlm_socket;

#ifdef INET6

/*
 * A socket to use for RPC - shared by all IPv6 RPC clients.
 */
static struct socket *nlm_socket6;

#endif

/*
 * An RPC client handle that can be used to communicate with the local
 * NSM.
 */
static CLIENT *nlm_nsm;

/*
 * An AUTH handle for the server's creds.
 */
static AUTH *nlm_auth;

/*
 * A zero timeval for sending async RPC messages.
 */
struct timeval nlm_zero_tv = { 0, 0 };

/*
 * The local NSM state number
 */
int nlm_nsm_state;


/*
 * A lock to protect the host list and waiting lock list.
 */
static struct mtx nlm_global_lock;

/*
 * Locks:
 * (l)		locked by nh_lock
 * (s)		only accessed via server RPC which is single threaded
 * (g)		locked by nlm_global_lock
 * (c)		const until freeing
 * (a)		modified using atomic ops
 */

/*
 * A pending client-side lock request, stored on the nlm_waiting_locks
 * list.
 */
struct nlm_waiting_lock {
	TAILQ_ENTRY(nlm_waiting_lock) nw_link; /* (g) */
	bool_t		nw_waiting;	       /* (g) */
	nlm4_lock	nw_lock;	       /* (c) */
	union nfsfh	nw_fh;		       /* (c) */
	struct vnode	*nw_vp;		       /* (c) */
};
TAILQ_HEAD(nlm_waiting_lock_list, nlm_waiting_lock);

struct nlm_waiting_lock_list nlm_waiting_locks; /* (g) */

/*
 * A pending server-side asynchronous lock request, stored on the
 * nh_pending list of the NLM host.
 */
struct nlm_async_lock {
	TAILQ_ENTRY(nlm_async_lock) af_link; /* (l) host's list of locks */
	struct task	af_task;	/* (c) async callback details */
	void		*af_cookie;	/* (l) lock manager cancel token */
	struct vnode	*af_vp;		/* (l) vnode to lock */
	struct flock	af_fl;		/* (c) lock details */
	struct nlm_host *af_host;	/* (c) host which is locking */
	CLIENT		*af_rpc;	/* (c) rpc client to send message */
	nlm4_testargs	af_granted;	/* (c) notification details */
};
TAILQ_HEAD(nlm_async_lock_list, nlm_async_lock);

/*
 * NLM host.
 */
enum nlm_host_state {
	NLM_UNMONITORED,
	NLM_MONITORED,
	NLM_MONITOR_FAILED,
	NLM_RECOVERING
};
struct nlm_host {
	struct mtx	nh_lock;
	volatile u_int	nh_refs;       /* (a) reference count */
	TAILQ_ENTRY(nlm_host) nh_link; /* (g) global list of hosts */
	char		nh_caller_name[MAXNAMELEN]; /* (c) printable name of host */
	uint32_t	nh_sysid;	 /* (c) our allocaed system ID */
	char		nh_sysid_string[10]; /* (c) string rep. of sysid */
	struct sockaddr_storage	nh_addr; /* (s) remote address of host */
	CLIENT		*nh_rpc;	 /* (l) RPC handle to send to host */
	rpcvers_t	nh_vers;	 /* (s) NLM version of host */
	int		nh_state;	 /* (s) last seen NSM state of host */
	enum nlm_host_state nh_monstate; /* (l) local NSM monitoring state */
	time_t		nh_idle_timeout; /* (s) Time at which host is idle */
	time_t		nh_rpc_create_time; /* (s) Time we create RPC client */
	struct sysctl_ctx_list nh_sysctl; /* (c) vfs.nlm.sysid nodes */
	struct nlm_async_lock_list nh_pending; /* (l) pending async locks */
	struct nlm_async_lock_list nh_finished; /* (l) finished async locks */
};
TAILQ_HEAD(nlm_host_list, nlm_host);

static struct nlm_host_list nlm_hosts; /* (g) */
static uint32_t nlm_next_sysid = 1;    /* (g) */

static void	nlm_host_unmonitor(struct nlm_host *);

/**********************************************************************/

/*
 * Initialise NLM globals.
 */
static void
nlm_init(void *dummy)
{
	int error;

	mtx_init(&nlm_global_lock, "nlm_global_lock", NULL, MTX_DEF);
	TAILQ_INIT(&nlm_waiting_locks);
	TAILQ_INIT(&nlm_hosts);

	error = syscall_register(&nlm_syscall_offset, &nlm_syscall_sysent,
	    &nlm_syscall_prev_sysent);
	if (error)
		printf("Can't register NLM syscall\n");
	else
		nlm_syscall_registered = TRUE;
}
SYSINIT(nlm_init, SI_SUB_LOCK, SI_ORDER_FIRST, nlm_init, NULL);

static void
nlm_uninit(void *dummy)
{

	if (nlm_syscall_registered)
		syscall_deregister(&nlm_syscall_offset,
		    &nlm_syscall_prev_sysent);
}
SYSUNINIT(nlm_uninit, SI_SUB_LOCK, SI_ORDER_FIRST, nlm_uninit, NULL);

/*
 * Copy a struct netobj.
 */ 
void
nlm_copy_netobj(struct netobj *dst, struct netobj *src,
    struct malloc_type *type)
{

	dst->n_len = src->n_len;
	dst->n_bytes = malloc(src->n_len, type, M_WAITOK);
	memcpy(dst->n_bytes, src->n_bytes, src->n_len);
}

/*
 * Create an RPC client handle for the given (address,prog,vers)
 * triple using UDP.
 */
static CLIENT *
nlm_get_rpc(struct sockaddr *sa, rpcprog_t prog, rpcvers_t vers)
{
	const char *wchan = "nlmrcv";
	const char* protofmly;
	struct sockaddr_storage ss;
	struct socket *so;
	CLIENT *rpcb;
	struct timeval timo;
	RPCB parms;
	char *uaddr;
	enum clnt_stat stat = RPC_SUCCESS;
	int rpcvers = RPCBVERS4;
	bool_t do_tcp = FALSE;
	struct portmap mapping;
	u_short port = 0;

	/*
	 * First we need to contact the remote RPCBIND service to find
	 * the right port.
	 */
	memcpy(&ss, sa, sa->sa_len);
	switch (ss.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&ss)->sin_port = htons(111);
		protofmly = "inet";
		so = nlm_socket;
		break;
		
#ifdef INET6
	case AF_INET6:
		((struct sockaddr_in6 *)&ss)->sin6_port = htons(111);
		protofmly = "inet6";
		so = nlm_socket6;
		break;
#endif

	default:
		/*
		 * Unsupported address family - fail.
		 */
		return (NULL);
	}

	rpcb = clnt_dg_create(so, (struct sockaddr *)&ss,
	    RPCBPROG, rpcvers, 0, 0);
	if (!rpcb)
		return (NULL);

try_tcp:
	parms.r_prog = prog;
	parms.r_vers = vers;
	if (do_tcp)
		parms.r_netid = "tcp";
	else
		parms.r_netid = "udp";
	parms.r_addr = "";
	parms.r_owner = "";

	/*
	 * Use the default timeout.
	 */
	timo.tv_sec = 25;
	timo.tv_usec = 0;
again:
	switch (rpcvers) {
	case RPCBVERS4:
	case RPCBVERS:
		/*
		 * Try RPCBIND 4 then 3.
		 */
		uaddr = NULL;
		stat = CLNT_CALL(rpcb, (rpcprog_t) RPCBPROC_GETADDR,
		    (xdrproc_t) xdr_rpcb, &parms,
		    (xdrproc_t) xdr_wrapstring, &uaddr, timo);
		if (stat == RPC_PROGVERSMISMATCH) {
			if (rpcvers == RPCBVERS4)
				rpcvers = RPCBVERS;
			else if (rpcvers == RPCBVERS)
				rpcvers = PMAPVERS;
			CLNT_CONTROL(rpcb, CLSET_VERS, &rpcvers);
			goto again;
		} else if (stat == RPC_SUCCESS) {
			/*
			 * We have a reply from the remote RPCBIND - turn it
			 * into an appropriate address and make a new client
			 * that can talk to the remote NLM.
			 *
			 * XXX fixup IPv6 scope ID.
			 */
			struct netbuf *a;
			a = __rpc_uaddr2taddr_af(ss.ss_family, uaddr);
			if (!a) {
				CLNT_DESTROY(rpcb);
				return (NULL);
			}
			memcpy(&ss, a->buf, a->len);
			free(a->buf, M_RPC);
			free(a, M_RPC);
			xdr_free((xdrproc_t) xdr_wrapstring, &uaddr);
		}
		break;
	case PMAPVERS:
		/*
		 * Try portmap.
		 */
		mapping.pm_prog = parms.r_prog;
		mapping.pm_vers = parms.r_vers;
		mapping.pm_prot = do_tcp ? IPPROTO_TCP : IPPROTO_UDP;
		mapping.pm_port = 0;

		stat = CLNT_CALL(rpcb, (rpcprog_t) PMAPPROC_GETPORT,
		    (xdrproc_t) xdr_portmap, &mapping,
		    (xdrproc_t) xdr_u_short, &port, timo);

		if (stat == RPC_SUCCESS) {
			switch (ss.ss_family) {
			case AF_INET:
				((struct sockaddr_in *)&ss)->sin_port =
					htons(port);
				break;
		
#ifdef INET6
			case AF_INET6:
				((struct sockaddr_in6 *)&ss)->sin6_port =
					htons(port);
				break;
#endif
			}
		}
		break;
	default:
		panic("invalid rpcvers %d", rpcvers);
	}
	/*
	 * We may have a positive response from the portmapper, but the NLM
	 * service was not found. Make sure we received a valid port.
	 */
	switch (ss.ss_family) {
	case AF_INET:
		port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
#endif
	}
	if (stat != RPC_SUCCESS || !port) {
		/*
		 * If we were able to talk to rpcbind or portmap, but the udp
		 * variant wasn't available, ask about tcp.
		 *
		 * XXX - We could also check for a TCP portmapper, but
		 * if the host is running a portmapper at all, we should be able
		 * to hail it over UDP.
		 */
		if (stat == RPC_SUCCESS && !do_tcp) {
			do_tcp = TRUE;
			goto try_tcp;
		}

		/* Otherwise, bad news. */
		printf("NLM: failed to contact remote rpcbind, "
		    "stat = %d, port = %d\n",
		    (int) stat, port);
		CLNT_DESTROY(rpcb);
		return (NULL);
	}

	if (do_tcp) {
		/*
		 * Destroy the UDP client we used to speak to rpcbind and
		 * recreate as a TCP client.
		 */
		struct netconfig *nconf = NULL;

		CLNT_DESTROY(rpcb);

		switch (ss.ss_family) {
		case AF_INET:
			nconf = getnetconfigent("tcp");
			break;
#ifdef INET6
		case AF_INET6:
			nconf = getnetconfigent("tcp6");
			break;
#endif
		}

		rpcb = clnt_reconnect_create(nconf, (struct sockaddr *)&ss,
		    prog, vers, 0, 0);
		CLNT_CONTROL(rpcb, CLSET_WAITCHAN, &wchan);
		rpcb->cl_auth = nlm_auth;
		
	} else {
		/*
		 * Re-use the client we used to speak to rpcbind.
		 */
		CLNT_CONTROL(rpcb, CLSET_SVC_ADDR, &ss);
		CLNT_CONTROL(rpcb, CLSET_PROG, &prog);
		CLNT_CONTROL(rpcb, CLSET_VERS, &vers);
		CLNT_CONTROL(rpcb, CLSET_WAITCHAN, &wchan);
		rpcb->cl_auth = nlm_auth;
	}

	return (rpcb);
}

/*
 * This async callback after when an async lock request has been
 * granted. We notify the host which initiated the request.
 */
static void
nlm_lock_callback(void *arg, int pending)
{
	struct nlm_async_lock *af = (struct nlm_async_lock *) arg;
	struct rpc_callextra ext;

	if (nlm_debug_level >= 2)
		printf("NLM: async lock %p for %s (sysid %d) granted\n",
		    af, af->af_host->nh_caller_name,
		    af->af_host->nh_sysid);

	/*
	 * Send the results back to the host.
	 *
	 * Note: there is a possible race here with nlm_host_notify
	 * destroying the RPC client. To avoid problems, the first
	 * thing nlm_host_notify does is to cancel pending async lock
	 * requests.
	 */
	memset(&ext, 0, sizeof(ext));
	ext.rc_auth = nlm_auth;
	if (af->af_host->nh_vers == NLM_VERS4) {
		nlm4_granted_msg_4(&af->af_granted,
		    NULL, af->af_rpc, &ext, nlm_zero_tv);
	} else {
		/*
		 * Back-convert to legacy protocol
		 */
		nlm_testargs granted;
		granted.cookie = af->af_granted.cookie;
		granted.exclusive = af->af_granted.exclusive;
		granted.alock.caller_name =
			af->af_granted.alock.caller_name;
		granted.alock.fh = af->af_granted.alock.fh;
		granted.alock.oh = af->af_granted.alock.oh;
		granted.alock.svid = af->af_granted.alock.svid;
		granted.alock.l_offset =
			af->af_granted.alock.l_offset;
		granted.alock.l_len =
			af->af_granted.alock.l_len;

		nlm_granted_msg_1(&granted,
		    NULL, af->af_rpc, &ext, nlm_zero_tv);
	}

	/*
	 * Move this entry to the nh_finished list. Someone else will
	 * free it later - its too hard to do it here safely without
	 * racing with cancel.
	 *
	 * XXX possibly we should have a third "granted sent but not
	 * ack'ed" list so that we can re-send the granted message.
	 */
	mtx_lock(&af->af_host->nh_lock);
	TAILQ_REMOVE(&af->af_host->nh_pending, af, af_link);
	TAILQ_INSERT_TAIL(&af->af_host->nh_finished, af, af_link);
	mtx_unlock(&af->af_host->nh_lock);
}

/*
 * Free an async lock request. The request must have been removed from
 * any list.
 */
static void
nlm_free_async_lock(struct nlm_async_lock *af)
{
	/*
	 * Free an async lock.
	 */
	if (af->af_rpc)
		CLNT_RELEASE(af->af_rpc);
	xdr_free((xdrproc_t) xdr_nlm4_testargs, &af->af_granted);
	if (af->af_vp)
		vrele(af->af_vp);
	free(af, M_NLM);
}

/*
 * Cancel our async request - this must be called with
 * af->nh_host->nh_lock held. This is slightly complicated by a
 * potential race with our own callback. If we fail to cancel the
 * lock, it must already have been granted - we make sure our async
 * task has completed by calling taskqueue_drain in this case.
 */
static int
nlm_cancel_async_lock(struct nlm_async_lock *af)
{
	struct nlm_host *host = af->af_host;
	int error;

	mtx_assert(&host->nh_lock, MA_OWNED);

	mtx_unlock(&host->nh_lock);

	error = VOP_ADVLOCKASYNC(af->af_vp, NULL, F_CANCEL, &af->af_fl,
	    F_REMOTE, NULL, &af->af_cookie);

	if (error) {
		/*
		 * We failed to cancel - make sure our callback has
		 * completed before we continue.
		 */
		taskqueue_drain(taskqueue_thread, &af->af_task);
	}

	mtx_lock(&host->nh_lock);
	
	if (!error) {
		if (nlm_debug_level >= 2)
			printf("NLM: async lock %p for %s (sysid %d) "
			    "cancelled\n",
			    af, host->nh_caller_name, host->nh_sysid);

		/*
		 * Remove from the nh_pending list and free now that
		 * we are safe from the callback.
		 */
		TAILQ_REMOVE(&host->nh_pending, af, af_link);
		mtx_unlock(&host->nh_lock);
		nlm_free_async_lock(af);
		mtx_lock(&host->nh_lock);
	}

	return (error);
}

static void
nlm_free_finished_locks(struct nlm_host *host)
{
	struct nlm_async_lock *af;

	mtx_lock(&host->nh_lock);
	while ((af = TAILQ_FIRST(&host->nh_finished)) != NULL) {
		TAILQ_REMOVE(&host->nh_finished, af, af_link);
		mtx_unlock(&host->nh_lock);
		nlm_free_async_lock(af);
		mtx_lock(&host->nh_lock);
	}
	mtx_unlock(&host->nh_lock);
}

/*
 * Free resources used by a host. This is called after the reference
 * count has reached zero so it doesn't need to worry about locks.
 */
static void
nlm_host_destroy(struct nlm_host *host)
{

	mtx_lock(&nlm_global_lock);
	TAILQ_REMOVE(&nlm_hosts, host, nh_link);
	mtx_unlock(&nlm_global_lock);

	if (host->nh_rpc)
		CLNT_RELEASE(host->nh_rpc);
	mtx_destroy(&host->nh_lock);
	sysctl_ctx_free(&host->nh_sysctl);
	free(host, M_NLM);
}

/*
 * Thread start callback for client lock recovery
 */
static void
nlm_client_recovery_start(void *arg)
{
	struct nlm_host *host = (struct nlm_host *) arg;

	if (nlm_debug_level >= 1)
		printf("NLM: client lock recovery for %s started\n",
		    host->nh_caller_name);

	nlm_client_recovery(host);

	if (nlm_debug_level >= 1)
		printf("NLM: client lock recovery for %s completed\n",
		    host->nh_caller_name);

	host->nh_monstate = NLM_MONITORED;
	nlm_host_release(host);

	kthread_exit();
}

/*
 * This is called when we receive a host state change notification. We
 * unlock any active locks owned by the host. When rpc.lockd is
 * shutting down, this function is called with newstate set to zero
 * which allows us to cancel any pending async locks and clear the
 * locking state.
 */
static void
nlm_host_notify(struct nlm_host *host, int newstate)
{
	struct nlm_async_lock *af;

	if (newstate) {
		if (nlm_debug_level >= 1)
			printf("NLM: host %s (sysid %d) rebooted, new "
			    "state is %d\n",
			    host->nh_caller_name, host->nh_sysid, newstate);
	}

	/*
	 * Cancel any pending async locks for this host.
	 */
	mtx_lock(&host->nh_lock);
	while ((af = TAILQ_FIRST(&host->nh_pending)) != NULL) {
		/*
		 * nlm_cancel_async_lock will remove the entry from
		 * nh_pending and free it.
		 */
		nlm_cancel_async_lock(af);
	}
	mtx_unlock(&host->nh_lock);
	nlm_free_finished_locks(host);

	/*
	 * The host just rebooted - trash its locks.
	 */
	lf_clearremotesys(host->nh_sysid);
	host->nh_state = newstate;

	/*
	 * If we have any remote locks for this host (i.e. it
	 * represents a remote NFS server that our local NFS client
	 * has locks for), start a recovery thread.
	 */
	if (newstate != 0
	    && host->nh_monstate != NLM_RECOVERING
	    && lf_countlocks(NLM_SYSID_CLIENT | host->nh_sysid) > 0) {
		struct thread *td;
		host->nh_monstate = NLM_RECOVERING;
		refcount_acquire(&host->nh_refs);
		kthread_add(nlm_client_recovery_start, host, curproc, &td, 0, 0,
		    "NFS lock recovery for %s", host->nh_caller_name);
	}
}

/*
 * Sysctl handler to count the number of locks for a sysid.
 */
static int
nlm_host_lock_count_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct nlm_host *host;
	int count;

	host = oidp->oid_arg1;
	count = lf_countlocks(host->nh_sysid);
	return sysctl_handle_int(oidp, &count, 0, req);
}

/*
 * Sysctl handler to count the number of client locks for a sysid.
 */
static int
nlm_host_client_lock_count_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct nlm_host *host;
	int count;

	host = oidp->oid_arg1;
	count = lf_countlocks(NLM_SYSID_CLIENT | host->nh_sysid);
	return sysctl_handle_int(oidp, &count, 0, req);
}

/*
 * Create a new NLM host.
 */
static struct nlm_host *
nlm_create_host(const char* caller_name)
{
	struct nlm_host *host;
	struct sysctl_oid *oid;

	mtx_assert(&nlm_global_lock, MA_OWNED);

	if (nlm_debug_level >= 1)
		printf("NLM: new host %s (sysid %d)\n",
		    caller_name, nlm_next_sysid);
	host = malloc(sizeof(struct nlm_host), M_NLM, M_NOWAIT|M_ZERO);
	if (!host)
		return (NULL);
	mtx_init(&host->nh_lock, "nh_lock", NULL, MTX_DEF);
	host->nh_refs = 1;
	strlcpy(host->nh_caller_name, caller_name, MAXNAMELEN);
	host->nh_sysid = nlm_next_sysid++;
	snprintf(host->nh_sysid_string, sizeof(host->nh_sysid_string),
		"%d", host->nh_sysid);
	host->nh_rpc = NULL;
	host->nh_vers = 0;
	host->nh_state = 0;
	host->nh_monstate = NLM_UNMONITORED;
	TAILQ_INIT(&host->nh_pending);
	TAILQ_INIT(&host->nh_finished);
	TAILQ_INSERT_TAIL(&nlm_hosts, host, nh_link);

	mtx_unlock(&nlm_global_lock);

	sysctl_ctx_init(&host->nh_sysctl);
	oid = SYSCTL_ADD_NODE(&host->nh_sysctl,
	    SYSCTL_STATIC_CHILDREN(_vfs_nlm_sysid),
	    OID_AUTO, host->nh_sysid_string, CTLFLAG_RD, NULL, "");
	SYSCTL_ADD_STRING(&host->nh_sysctl, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "hostname", CTLFLAG_RD, host->nh_caller_name, 0, "");
	SYSCTL_ADD_INT(&host->nh_sysctl, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "version", CTLFLAG_RD, &host->nh_vers, 0, "");
	SYSCTL_ADD_INT(&host->nh_sysctl, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "monitored", CTLFLAG_RD, &host->nh_monstate, 0, "");
	SYSCTL_ADD_PROC(&host->nh_sysctl, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "lock_count", CTLTYPE_INT | CTLFLAG_RD, host, 0,
	    nlm_host_lock_count_sysctl, "I", "");
	SYSCTL_ADD_PROC(&host->nh_sysctl, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "client_lock_count", CTLTYPE_INT | CTLFLAG_RD, host, 0,
	    nlm_host_client_lock_count_sysctl, "I", "");

	mtx_lock(&nlm_global_lock);

	return (host);
}

/*
 * Return non-zero if the address parts of the two sockaddrs are the
 * same.
 */
static int
nlm_compare_addr(const struct sockaddr *a, const struct sockaddr *b)
{
	const struct sockaddr_in *a4, *b4;
#ifdef INET6
	const struct sockaddr_in6 *a6, *b6;
#endif

	if (a->sa_family != b->sa_family)
		return (FALSE);

	switch (a->sa_family) {
	case AF_INET:
		a4 = (const struct sockaddr_in *) a;
		b4 = (const struct sockaddr_in *) b;
		return !memcmp(&a4->sin_addr, &b4->sin_addr,
		    sizeof(a4->sin_addr));
#ifdef INET6
	case AF_INET6:
		a6 = (const struct sockaddr_in6 *) a;
		b6 = (const struct sockaddr_in6 *) b;
		return !memcmp(&a6->sin6_addr, &b6->sin6_addr,
		    sizeof(a6->sin6_addr));
#endif
	}

	return (0);
}

/*
 * Check for idle hosts and stop monitoring them. We could also free
 * the host structure here, possibly after a larger timeout but that
 * would require some care to avoid races with
 * e.g. nlm_host_lock_count_sysctl.
 */
static void
nlm_check_idle(void)
{
	struct nlm_host *host;

	mtx_assert(&nlm_global_lock, MA_OWNED);

	if (time_uptime <= nlm_next_idle_check)
		return;

	nlm_next_idle_check = time_uptime + NLM_IDLE_PERIOD;

	TAILQ_FOREACH(host, &nlm_hosts, nh_link) {
		if (host->nh_monstate == NLM_MONITORED
		    && time_uptime > host->nh_idle_timeout) {
			mtx_unlock(&nlm_global_lock);
			if (lf_countlocks(host->nh_sysid) > 0
			    || lf_countlocks(NLM_SYSID_CLIENT
				+ host->nh_sysid)) {
				host->nh_idle_timeout =
					time_uptime + NLM_IDLE_TIMEOUT;
				mtx_lock(&nlm_global_lock);
				continue;
			}
			nlm_host_unmonitor(host);
			mtx_lock(&nlm_global_lock);
		} 
	}
}

/*
 * Search for an existing NLM host that matches the given name
 * (typically the caller_name element of an nlm4_lock).  If none is
 * found, create a new host. If 'addr' is non-NULL, record the remote
 * address of the host so that we can call it back for async
 * responses. If 'vers' is greater than zero then record the NLM
 * program version to use to communicate with this client.
 */
struct nlm_host *
nlm_find_host_by_name(const char *name, const struct sockaddr *addr,
    rpcvers_t vers)
{
	struct nlm_host *host;

	mtx_lock(&nlm_global_lock);

	/*
	 * The remote host is determined by caller_name.
	 */
	TAILQ_FOREACH(host, &nlm_hosts, nh_link) {
		if (!strcmp(host->nh_caller_name, name))
			break;
	}

	if (!host) {
		host = nlm_create_host(name);
		if (!host) {
			mtx_unlock(&nlm_global_lock);
			return (NULL);
		}
	}
	refcount_acquire(&host->nh_refs);

	host->nh_idle_timeout = time_uptime + NLM_IDLE_TIMEOUT;

	/*
	 * If we have an address for the host, record it so that we
	 * can send async replies etc.
	 */
	if (addr) {
		
		KASSERT(addr->sa_len < sizeof(struct sockaddr_storage),
		    ("Strange remote transport address length"));

		/*
		 * If we have seen an address before and we currently
		 * have an RPC client handle, make sure the address is
		 * the same, otherwise discard the client handle.
		 */
		if (host->nh_addr.ss_len && host->nh_rpc) {
			if (!nlm_compare_addr(
				    (struct sockaddr *) &host->nh_addr,
				    addr)
			    || host->nh_vers != vers) {
				CLIENT *client;
				mtx_lock(&host->nh_lock);
				client = host->nh_rpc;
				host->nh_rpc = NULL;
				mtx_unlock(&host->nh_lock);
				if (client) {
					CLNT_RELEASE(client);
				}
			}
		}
		memcpy(&host->nh_addr, addr, addr->sa_len);
		host->nh_vers = vers;
	}

	nlm_check_idle();

	mtx_unlock(&nlm_global_lock);

	return (host);
}

/*
 * Search for an existing NLM host that matches the given remote
 * address. If none is found, create a new host with the requested
 * address and remember 'vers' as the NLM protocol version to use for
 * that host.
 */
struct nlm_host *
nlm_find_host_by_addr(const struct sockaddr *addr, int vers)
{
	/*
	 * Fake up a name using inet_ntop. This buffer is
	 * large enough for an IPv6 address.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	struct nlm_host *host;

	switch (addr->sa_family) {
	case AF_INET:
		__rpc_inet_ntop(AF_INET,
		    &((const struct sockaddr_in *) addr)->sin_addr,
		    tmp, sizeof tmp);
		break;
#ifdef INET6
	case AF_INET6:
		__rpc_inet_ntop(AF_INET6,
		    &((const struct sockaddr_in6 *) addr)->sin6_addr,
		    tmp, sizeof tmp);
		break;
#endif
	default:
		strcmp(tmp, "<unknown>");
	}


	mtx_lock(&nlm_global_lock);

	/*
	 * The remote host is determined by caller_name.
	 */
	TAILQ_FOREACH(host, &nlm_hosts, nh_link) {
		if (nlm_compare_addr(addr,
			(const struct sockaddr *) &host->nh_addr))
			break;
	}

	if (!host) {
		host = nlm_create_host(tmp);
		if (!host) {
			mtx_unlock(&nlm_global_lock);
			return (NULL);
		}
		memcpy(&host->nh_addr, addr, addr->sa_len);
		host->nh_vers = vers;
	}
	refcount_acquire(&host->nh_refs);

	host->nh_idle_timeout = time_uptime + NLM_IDLE_TIMEOUT;

	nlm_check_idle();

	mtx_unlock(&nlm_global_lock);

	return (host);
}

/*
 * Find the NLM host that matches the value of 'sysid'. If none
 * exists, return NULL.
 */
static struct nlm_host *
nlm_find_host_by_sysid(int sysid)
{
	struct nlm_host *host;

	TAILQ_FOREACH(host, &nlm_hosts, nh_link) {
		if (host->nh_sysid == sysid) {
			refcount_acquire(&host->nh_refs);
			return (host);
		}
	}

	return (NULL);
}

void nlm_host_release(struct nlm_host *host)
{
	if (refcount_release(&host->nh_refs)) {
		/*
		 * Free the host
		 */
		nlm_host_destroy(host);
	}
}

/*
 * Unregister this NLM host with the local NSM due to idleness.
 */
static void
nlm_host_unmonitor(struct nlm_host *host)
{
	mon_id smmonid;
	sm_stat_res smstat;
	struct timeval timo;
	enum clnt_stat stat;

	if (nlm_debug_level >= 1)
		printf("NLM: unmonitoring %s (sysid %d)\n",
		    host->nh_caller_name, host->nh_sysid);

	/*
	 * We put our assigned system ID value in the priv field to
	 * make it simpler to find the host if we are notified of a
	 * host restart.
	 */
	smmonid.mon_name = host->nh_caller_name;
	smmonid.my_id.my_name = "localhost";
	smmonid.my_id.my_prog = NLM_PROG;
	smmonid.my_id.my_vers = NLM_SM;
	smmonid.my_id.my_proc = NLM_SM_NOTIFY;

	timo.tv_sec = 25;
	timo.tv_usec = 0;
	stat = CLNT_CALL(nlm_nsm, SM_UNMON,
	    (xdrproc_t) xdr_mon, &smmonid,
	    (xdrproc_t) xdr_sm_stat, &smstat, timo);

	if (stat != RPC_SUCCESS) {
		printf("Failed to contact local NSM - rpc error %d\n", stat);
		return;
	}
	if (smstat.res_stat == stat_fail) {
		printf("Local NSM refuses to unmonitor %s\n",
		    host->nh_caller_name);
		return;
	}

	host->nh_monstate = NLM_UNMONITORED;
}

/*
 * Register this NLM host with the local NSM so that we can be
 * notified if it reboots.
 */
void
nlm_host_monitor(struct nlm_host *host, int state)
{
	mon smmon;
	sm_stat_res smstat;
	struct timeval timo;
	enum clnt_stat stat;

	if (state && !host->nh_state) {
		/*
		 * This is the first time we have seen an NSM state
		 * value for this host. We record it here to help
		 * detect host reboots.
		 */
		host->nh_state = state;
		if (nlm_debug_level >= 1)
			printf("NLM: host %s (sysid %d) has NSM state %d\n",
			    host->nh_caller_name, host->nh_sysid, state);
	}

	mtx_lock(&host->nh_lock);
	if (host->nh_monstate != NLM_UNMONITORED) {
		mtx_unlock(&host->nh_lock);
		return;
	}
	host->nh_monstate = NLM_MONITORED;
	mtx_unlock(&host->nh_lock);

	if (nlm_debug_level >= 1)
		printf("NLM: monitoring %s (sysid %d)\n",
		    host->nh_caller_name, host->nh_sysid);

	/*
	 * We put our assigned system ID value in the priv field to
	 * make it simpler to find the host if we are notified of a
	 * host restart.
	 */
	smmon.mon_id.mon_name = host->nh_caller_name;
	smmon.mon_id.my_id.my_name = "localhost";
	smmon.mon_id.my_id.my_prog = NLM_PROG;
	smmon.mon_id.my_id.my_vers = NLM_SM;
	smmon.mon_id.my_id.my_proc = NLM_SM_NOTIFY;
	memcpy(smmon.priv, &host->nh_sysid, sizeof(host->nh_sysid));

	timo.tv_sec = 25;
	timo.tv_usec = 0;
	stat = CLNT_CALL(nlm_nsm, SM_MON,
	    (xdrproc_t) xdr_mon, &smmon,
	    (xdrproc_t) xdr_sm_stat, &smstat, timo);

	if (stat != RPC_SUCCESS) {
		printf("Failed to contact local NSM - rpc error %d\n", stat);
		return;
	}
	if (smstat.res_stat == stat_fail) {
		printf("Local NSM refuses to monitor %s\n",
		    host->nh_caller_name);
		mtx_lock(&host->nh_lock);
		host->nh_monstate = NLM_MONITOR_FAILED;
		mtx_unlock(&host->nh_lock);
		return;
	}

	host->nh_monstate = NLM_MONITORED;
}

/*
 * Return an RPC client handle that can be used to talk to the NLM
 * running on the given host.
 */
CLIENT *
nlm_host_get_rpc(struct nlm_host *host)
{
	CLIENT *client;

	mtx_lock(&host->nh_lock);

	/*
	 * We can't hold onto RPC handles for too long - the async
	 * call/reply protocol used by some NLM clients makes it hard
	 * to tell when they change port numbers (e.g. after a
	 * reboot). Note that if a client reboots while it isn't
	 * holding any locks, it won't bother to notify us. We
	 * expire the RPC handles after two minutes.
	 */
	if (host->nh_rpc && time_uptime > host->nh_rpc_create_time + 2*60) {
		client = host->nh_rpc;
		host->nh_rpc = NULL;
		mtx_unlock(&host->nh_lock);
		CLNT_RELEASE(client);
		mtx_lock(&host->nh_lock);
	}

	if (!host->nh_rpc) {
		mtx_unlock(&host->nh_lock);
		client = nlm_get_rpc((struct sockaddr *)&host->nh_addr,
		    NLM_PROG, host->nh_vers);
		mtx_lock(&host->nh_lock);

		if (client) {
			if (host->nh_rpc) {
				mtx_unlock(&host->nh_lock);
				CLNT_DESTROY(client);
				mtx_lock(&host->nh_lock);
			} else {
				host->nh_rpc = client;
				host->nh_rpc_create_time = time_uptime;
			}
		}
	}

	client = host->nh_rpc;
	if (client)
		CLNT_ACQUIRE(client);
	mtx_unlock(&host->nh_lock);

	return (client);

}

int nlm_host_get_sysid(struct nlm_host *host)
{

	return (host->nh_sysid);
}

int
nlm_host_get_state(struct nlm_host *host)
{

	return (host->nh_state);
}

void *
nlm_register_wait_lock(struct nlm4_lock *lock, struct vnode *vp)
{
	struct nlm_waiting_lock *nw;

	nw = malloc(sizeof(struct nlm_waiting_lock), M_NLM, M_WAITOK);
	nw->nw_lock = *lock;
	memcpy(&nw->nw_fh.fh_bytes, nw->nw_lock.fh.n_bytes,
	    nw->nw_lock.fh.n_len);
	nw->nw_lock.fh.n_bytes = nw->nw_fh.fh_bytes;
	nw->nw_waiting = TRUE;
	nw->nw_vp = vp;
	mtx_lock(&nlm_global_lock);
	TAILQ_INSERT_TAIL(&nlm_waiting_locks, nw, nw_link);
	mtx_unlock(&nlm_global_lock);

	return nw;
}

void
nlm_deregister_wait_lock(void *handle)
{
	struct nlm_waiting_lock *nw = handle;

	mtx_lock(&nlm_global_lock);
	TAILQ_REMOVE(&nlm_waiting_locks, nw, nw_link);
	mtx_unlock(&nlm_global_lock);
	
	free(nw, M_NLM);
}

int
nlm_wait_lock(void *handle, int timo)
{
	struct nlm_waiting_lock *nw = handle;
	int error;

	/*
	 * If the granted message arrived before we got here,
	 * nw->nw_waiting will be FALSE - in that case, don't sleep.
	 */
	mtx_lock(&nlm_global_lock);
	error = 0;
	if (nw->nw_waiting)
		error = msleep(nw, &nlm_global_lock, PCATCH, "nlmlock", timo);
	TAILQ_REMOVE(&nlm_waiting_locks, nw, nw_link);
	if (error) {
		/*
		 * The granted message may arrive after the
		 * interrupt/timeout but before we manage to lock the
		 * mutex. Detect this by examining nw_lock.
		 */
		if (!nw->nw_waiting)
			error = 0;
	} else {
		/*
		 * If nlm_cancel_wait is called, then error will be
		 * zero but nw_waiting will still be TRUE. We
		 * translate this into EINTR.
		 */
		if (nw->nw_waiting)
			error = EINTR;
	}
	mtx_unlock(&nlm_global_lock);

	free(nw, M_NLM);

	return (error);
}

void
nlm_cancel_wait(struct vnode *vp)
{
	struct nlm_waiting_lock *nw;

	mtx_lock(&nlm_global_lock);
	TAILQ_FOREACH(nw, &nlm_waiting_locks, nw_link) {
		if (nw->nw_vp == vp) {
			wakeup(nw);
		}
	}
	mtx_unlock(&nlm_global_lock);
}


/**********************************************************************/

/*
 * Syscall interface with userland.
 */

extern void nlm_prog_0(struct svc_req *rqstp, SVCXPRT *transp);
extern void nlm_prog_1(struct svc_req *rqstp, SVCXPRT *transp);
extern void nlm_prog_3(struct svc_req *rqstp, SVCXPRT *transp);
extern void nlm_prog_4(struct svc_req *rqstp, SVCXPRT *transp);

static int
nlm_register_services(SVCPOOL *pool, int addr_count, char **addrs)
{
	static rpcvers_t versions[] = {
		NLM_SM, NLM_VERS, NLM_VERSX, NLM_VERS4
	};
	static void (*dispatchers[])(struct svc_req *, SVCXPRT *) = {
		nlm_prog_0, nlm_prog_1, nlm_prog_3, nlm_prog_4
	};
	static const int version_count = sizeof(versions) / sizeof(versions[0]);

	SVCXPRT **xprts;
	char netid[16];
	char uaddr[128];
	struct netconfig *nconf;
	int i, j, error;

	if (!addr_count) {
		printf("NLM: no service addresses given - can't start server");
		return (EINVAL);
	}

	xprts = malloc(addr_count * sizeof(SVCXPRT *), M_NLM, M_WAITOK);
	for (i = 0; i < version_count; i++) {
		for (j = 0; j < addr_count; j++) {
			/*
			 * Create transports for the first version and
			 * then just register everything else to the
			 * same transports.
			 */
			if (i == 0) {
				char *up;

				error = copyin(&addrs[2*j], &up,
				    sizeof(char*));
				if (error)
					goto out;
				error = copyinstr(up, netid, sizeof(netid),
				    NULL);
				if (error)
					goto out;
				error = copyin(&addrs[2*j+1], &up,
				    sizeof(char*));
				if (error)
					goto out;
				error = copyinstr(up, uaddr, sizeof(uaddr),
				    NULL);
				if (error)
					goto out;
				nconf = getnetconfigent(netid);
				if (!nconf) {
					printf("Can't lookup netid %s\n",
					    netid);
					error = EINVAL;
					goto out;
				}
				xprts[j] = svc_tp_create(pool, dispatchers[i],
				    NLM_PROG, versions[i], uaddr, nconf);
				if (!xprts[j]) {
					printf("NLM: unable to create "
					    "(NLM_PROG, %d).\n", versions[i]);
					error = EINVAL;
					goto out;
				}
				freenetconfigent(nconf);
			} else {
				nconf = getnetconfigent(xprts[j]->xp_netid);
				rpcb_unset(NLM_PROG, versions[i], nconf);
				if (!svc_reg(xprts[j], NLM_PROG, versions[i],
					dispatchers[i], nconf)) {
					printf("NLM: can't register "
					    "(NLM_PROG, %d)\n", versions[i]);
					error = EINVAL;
					goto out;
				}
			}
		}
	}
	error = 0;
out:
	free(xprts, M_NLM);
	return (error);
}

/*
 * Main server entry point. Contacts the local NSM to get its current
 * state and send SM_UNMON_ALL. Registers the NLM services and then
 * services requests. Does not return until the server is interrupted
 * by a signal.
 */
static int
nlm_server_main(int addr_count, char **addrs)
{
	struct thread *td = curthread;
	int error;
	SVCPOOL *pool = NULL;
	struct sockopt opt;
	int portlow;
#ifdef INET6
	struct sockaddr_in6 sin6;
#endif
	struct sockaddr_in sin;
	my_id id;
	sm_stat smstat;
	struct timeval timo;
	enum clnt_stat stat;
	struct nlm_host *host, *nhost;
	struct nlm_waiting_lock *nw;
	vop_advlock_t *old_nfs_advlock;
	vop_reclaim_t *old_nfs_reclaim;
	int v4_used;
#ifdef INET6
	int v6_used;
#endif

	if (nlm_socket) {
		printf("NLM: can't start server - it appears to be running already\n");
		return (EPERM);
	}

	memset(&opt, 0, sizeof(opt));

	nlm_socket = NULL;
	error = socreate(AF_INET, &nlm_socket, SOCK_DGRAM, 0,
	    td->td_ucred, td);
	if (error) {
		printf("NLM: can't create IPv4 socket - error %d\n", error);
		return (error);
	}
	opt.sopt_dir = SOPT_SET;
	opt.sopt_level = IPPROTO_IP;
	opt.sopt_name = IP_PORTRANGE;
	portlow = IP_PORTRANGE_LOW;
	opt.sopt_val = &portlow;
	opt.sopt_valsize = sizeof(portlow);
	sosetopt(nlm_socket, &opt);

#ifdef INET6
	nlm_socket6 = NULL;
	error = socreate(AF_INET6, &nlm_socket6, SOCK_DGRAM, 0,
	    td->td_ucred, td);
	if (error) {
		printf("NLM: can't create IPv6 socket - error %d\n", error);
		goto out;
		return (error);
	}
	opt.sopt_dir = SOPT_SET;
	opt.sopt_level = IPPROTO_IPV6;
	opt.sopt_name = IPV6_PORTRANGE;
	portlow = IPV6_PORTRANGE_LOW;
	opt.sopt_val = &portlow;
	opt.sopt_valsize = sizeof(portlow);
	sosetopt(nlm_socket6, &opt);
#endif

	nlm_auth = authunix_create(curthread->td_ucred);

#ifdef INET6
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_loopback;
	nlm_nsm = nlm_get_rpc((struct sockaddr *) &sin6, SM_PROG, SM_VERS);
	if (!nlm_nsm) {
#endif
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		nlm_nsm = nlm_get_rpc((struct sockaddr *) &sin, SM_PROG,
		    SM_VERS);
#ifdef INET6
	}
#endif

	if (!nlm_nsm) {
		printf("Can't start NLM - unable to contact NSM\n");
		error = EINVAL;
		goto out;
	}

	pool = svcpool_create();

	error = nlm_register_services(pool, addr_count, addrs);
	if (error)
		goto out;

	memset(&id, 0, sizeof(id));
	id.my_name = "NFS NLM";

	timo.tv_sec = 25;
	timo.tv_usec = 0;
	stat = CLNT_CALL(nlm_nsm, SM_UNMON_ALL,
	    (xdrproc_t) xdr_my_id, &id,
	    (xdrproc_t) xdr_sm_stat, &smstat, timo);

	if (stat != RPC_SUCCESS) {
		struct rpc_err err;

		CLNT_GETERR(nlm_nsm, &err);
		printf("NLM: unexpected error contacting NSM, stat=%d, errno=%d\n",
		    stat, err.re_errno);
		error = EINVAL;
		goto out;
	}

	if (nlm_debug_level >= 1)
		printf("NLM: local NSM state is %d\n", smstat.state);
	nlm_nsm_state = smstat.state;

	old_nfs_advlock = nfs_advlock_p;
	nfs_advlock_p = nlm_advlock;
	old_nfs_reclaim = nfs_reclaim_p;
	nfs_reclaim_p = nlm_reclaim;

	svc_run(pool);
	error = 0;

	nfs_advlock_p = old_nfs_advlock;
	nfs_reclaim_p = old_nfs_reclaim;

out:
	if (pool)
		svcpool_destroy(pool);

	/*
	 * We are finished communicating with the NSM.
	 */
	if (nlm_nsm) {
		CLNT_RELEASE(nlm_nsm);
		nlm_nsm = NULL;
	}

	/*
	 * Trash all the existing state so that if the server
	 * restarts, it gets a clean slate. This is complicated by the
	 * possibility that there may be other threads trying to make
	 * client locking requests.
	 *
	 * First we fake a client reboot notification which will
	 * cancel any pending async locks and purge remote lock state
	 * from the local lock manager. We release the reference from
	 * nlm_hosts to the host (which may remove it from the list
	 * and free it). After this phase, the only entries in the
	 * nlm_host list should be from other threads performing
	 * client lock requests. We arrange to defer closing the
	 * sockets until the last RPC client handle is released.
	 */
	v4_used = 0;
#ifdef INET6
	v6_used = 0;
#endif
	mtx_lock(&nlm_global_lock);
	TAILQ_FOREACH(nw, &nlm_waiting_locks, nw_link) {
		wakeup(nw);
	}
	TAILQ_FOREACH_SAFE(host, &nlm_hosts, nh_link, nhost) {
		mtx_unlock(&nlm_global_lock);
		nlm_host_notify(host, 0);
		nlm_host_release(host);
		mtx_lock(&nlm_global_lock);
	}
	TAILQ_FOREACH_SAFE(host, &nlm_hosts, nh_link, nhost) {
		mtx_lock(&host->nh_lock);
		if (host->nh_rpc) {
			if (host->nh_addr.ss_family == AF_INET)
				v4_used++;
#ifdef INET6
			if (host->nh_addr.ss_family == AF_INET6)
				v6_used++;
#endif
			/*
			 * Note that the rpc over udp code copes
			 * correctly with the fact that a socket may
			 * be used by many rpc handles.
			 */
			CLNT_CONTROL(host->nh_rpc, CLSET_FD_CLOSE, 0);
		}
		mtx_unlock(&host->nh_lock);
	}
	mtx_unlock(&nlm_global_lock);

	AUTH_DESTROY(nlm_auth);

	if (!v4_used)
		soclose(nlm_socket);
	nlm_socket = NULL;
#ifdef INET6
	if (!v6_used)
		soclose(nlm_socket6);
	nlm_socket6 = NULL;
#endif

	return (error);
}

int
nlm_syscall(struct thread *td, struct nlm_syscall_args *uap)
{
	int error;

#if __FreeBSD_version >= 700000
	error = priv_check(td, PRIV_NFS_LOCKD);
#else
	error = suser(td);
#endif
	if (error)
		return (error);

	nlm_debug_level = uap->debug_level;
	nlm_grace_threshold = time_uptime + uap->grace_period;
	nlm_next_idle_check = time_uptime + NLM_IDLE_PERIOD;

	return nlm_server_main(uap->addr_count, uap->addrs);
}

/**********************************************************************/

/*
 * NLM implementation details, called from the RPC stubs.
 */


void
nlm_sm_notify(struct nlm_sm_status *argp)
{
	uint32_t sysid;
	struct nlm_host *host;

	if (nlm_debug_level >= 3)
		printf("nlm_sm_notify(): mon_name = %s\n", argp->mon_name);
	memcpy(&sysid, &argp->priv, sizeof(sysid));
	host = nlm_find_host_by_sysid(sysid);
	if (host) {
		nlm_host_notify(host, argp->state);
		nlm_host_release(host);
	}
}

static void
nlm_convert_to_fhandle_t(fhandle_t *fhp, struct netobj *p)
{
	memcpy(fhp, p->n_bytes, sizeof(fhandle_t));
}

struct vfs_state {
	struct mount	*vs_mp;
	struct vnode	*vs_vp;
	int		vs_vfslocked;
	int		vs_vnlocked;
};

static int
nlm_get_vfs_state(struct nlm_host *host, struct svc_req *rqstp,
    fhandle_t *fhp, struct vfs_state *vs)
{
	int error, exflags, freecred;
	struct ucred *cred = NULL, *credanon;
	
	memset(vs, 0, sizeof(*vs));
	freecred = FALSE;

	vs->vs_mp = vfs_getvfs(&fhp->fh_fsid);
	if (!vs->vs_mp) {
		return (ESTALE);
	}
	vs->vs_vfslocked = VFS_LOCK_GIANT(vs->vs_mp);

	error = VFS_CHECKEXP(vs->vs_mp, (struct sockaddr *)&host->nh_addr,
	    &exflags, &credanon);
	if (error)
		goto out;

	if (exflags & MNT_EXRDONLY || (vs->vs_mp->mnt_flag & MNT_RDONLY)) {
		error = EROFS;
		goto out;
	}

	error = VFS_FHTOVP(vs->vs_mp, &fhp->fh_fid, &vs->vs_vp);
	if (error)
		goto out;
	vs->vs_vnlocked = TRUE;

	cred = crget();
	freecred = TRUE;
	if (!svc_getcred(rqstp, cred, NULL)) {
		error = EINVAL;
		goto out;
	}
	if (cred->cr_uid == 0 || (exflags & MNT_EXPORTANON)) {
		crfree(cred);
		cred = credanon;
		freecred = FALSE;
	}

	/*
	 * Check cred.
	 */
	error = VOP_ACCESS(vs->vs_vp, VWRITE, cred, curthread);
	if (error)
		goto out;

#if __FreeBSD_version < 800011
	VOP_UNLOCK(vs->vs_vp, 0, curthread);
#else
	VOP_UNLOCK(vs->vs_vp, 0);
#endif
	vs->vs_vnlocked = FALSE;

out:
	if (freecred)
		crfree(cred);

	return (error);
}

static void
nlm_release_vfs_state(struct vfs_state *vs)
{

	if (vs->vs_vp) {
		if (vs->vs_vnlocked)
			vput(vs->vs_vp);
		else
			vrele(vs->vs_vp);
	}
	if (vs->vs_mp)
		vfs_rel(vs->vs_mp);
	VFS_UNLOCK_GIANT(vs->vs_vfslocked);
}

static nlm4_stats
nlm_convert_error(int error)
{

	if (error == ESTALE)
		return nlm4_stale_fh;
	else if (error == EROFS)
		return nlm4_rofs;
	else
		return nlm4_failed;
}

int
nlm_do_test(nlm4_testargs *argp, nlm4_testres *result, struct svc_req *rqstp,
	CLIENT **rpcp)
{
	fhandle_t fh;
	struct vfs_state vs;
	struct nlm_host *host, *bhost;
	int error, sysid;
	struct flock fl;
	
	memset(result, 0, sizeof(*result));
	memset(&vs, 0, sizeof(vs));

	host = nlm_find_host_by_name(argp->alock.caller_name,
	    (struct sockaddr *) rqstp->rq_xprt->xp_rtaddr.buf, rqstp->rq_vers);
	if (!host) {
		result->stat.stat = nlm4_denied_nolocks;
		return (ENOMEM);
	}

	if (nlm_debug_level >= 3)
		printf("nlm_do_test(): caller_name = %s (sysid = %d)\n",
		    host->nh_caller_name, host->nh_sysid);

	nlm_free_finished_locks(host);
	sysid = host->nh_sysid;

	nlm_convert_to_fhandle_t(&fh, &argp->alock.fh);
	nlm_copy_netobj(&result->cookie, &argp->cookie, M_RPC);

	if (time_uptime < nlm_grace_threshold) {
		result->stat.stat = nlm4_denied_grace_period;
		goto out;
	}

	error = nlm_get_vfs_state(host, rqstp, &fh, &vs);
	if (error) {
		result->stat.stat = nlm_convert_error(error);
		goto out;
	}

	fl.l_start = argp->alock.l_offset;
	fl.l_len = argp->alock.l_len;
	fl.l_pid = argp->alock.svid;
	fl.l_sysid = sysid;
	fl.l_whence = SEEK_SET;
	if (argp->exclusive)
		fl.l_type = F_WRLCK;
	else
		fl.l_type = F_RDLCK;
	error = VOP_ADVLOCK(vs.vs_vp, NULL, F_GETLK, &fl, F_REMOTE);
	if (error) {
		result->stat.stat = nlm4_failed;
		goto out;
	}

	if (fl.l_type == F_UNLCK) {
		result->stat.stat = nlm4_granted;
	} else {
		result->stat.stat = nlm4_denied;
		result->stat.nlm4_testrply_u.holder.exclusive =
			(fl.l_type == F_WRLCK);
		result->stat.nlm4_testrply_u.holder.svid = fl.l_pid;
		bhost = nlm_find_host_by_sysid(fl.l_sysid);
		if (bhost) {
			/*
			 * We don't have any useful way of recording
			 * the value of oh used in the original lock
			 * request. Ideally, the test reply would have
			 * a space for the owning host's name allowing
			 * our caller's NLM to keep track.
			 *
			 * As far as I can see, Solaris uses an eight
			 * byte structure for oh which contains a four
			 * byte pid encoded in local byte order and
			 * the first four bytes of the host
			 * name. Linux uses a variable length string
			 * 'pid@hostname' in ascii but doesn't even
			 * return that in test replies.
			 *
			 * For the moment, return nothing in oh
			 * (already zero'ed above).
			 */
			nlm_host_release(bhost);
		}
		result->stat.nlm4_testrply_u.holder.l_offset = fl.l_start;
		result->stat.nlm4_testrply_u.holder.l_len = fl.l_len;
	}

out:
	nlm_release_vfs_state(&vs);
	if (rpcp)
		*rpcp = nlm_host_get_rpc(host);
	nlm_host_release(host);
	return (0);
}

int
nlm_do_lock(nlm4_lockargs *argp, nlm4_res *result, struct svc_req *rqstp,
    bool_t monitor, CLIENT **rpcp)
{
	fhandle_t fh;
	struct vfs_state vs;
	struct nlm_host *host;
	int error, sysid;
	struct flock fl;
	
	memset(result, 0, sizeof(*result));
	memset(&vs, 0, sizeof(vs));

	host = nlm_find_host_by_name(argp->alock.caller_name,
	    (struct sockaddr *) rqstp->rq_xprt->xp_rtaddr.buf, rqstp->rq_vers);
	if (!host) {
		result->stat.stat = nlm4_denied_nolocks;
		return (ENOMEM);
	}

	if (nlm_debug_level >= 3)
		printf("nlm_do_lock(): caller_name = %s (sysid = %d)\n",
		    host->nh_caller_name, host->nh_sysid);

	if (monitor && host->nh_state && argp->state
	    && host->nh_state != argp->state) {
		/*
		 * The host rebooted without telling us. Trash its
		 * locks.
		 */
		nlm_host_notify(host, argp->state);
	}

	nlm_free_finished_locks(host);
	sysid = host->nh_sysid;

	nlm_convert_to_fhandle_t(&fh, &argp->alock.fh);
	nlm_copy_netobj(&result->cookie, &argp->cookie, M_RPC);

	if (time_uptime < nlm_grace_threshold && !argp->reclaim) {
		result->stat.stat = nlm4_denied_grace_period;
		goto out;
	}

	error = nlm_get_vfs_state(host, rqstp, &fh, &vs);
	if (error) {
		result->stat.stat = nlm_convert_error(error);
		goto out;
	}

	fl.l_start = argp->alock.l_offset;
	fl.l_len = argp->alock.l_len;
	fl.l_pid = argp->alock.svid;
	fl.l_sysid = sysid;
	fl.l_whence = SEEK_SET;
	if (argp->exclusive)
		fl.l_type = F_WRLCK;
	else
		fl.l_type = F_RDLCK;
	if (argp->block) {
		struct nlm_async_lock *af;
		CLIENT *client;

		/*
		 * First, make sure we can contact the host's NLM.
		 */
		client = nlm_host_get_rpc(host);
		if (!client) {
			result->stat.stat = nlm4_failed;
			goto out;
		}

		/*
		 * First we need to check and see if there is an
		 * existing blocked lock that matches. This could be a
		 * badly behaved client or an RPC re-send. If we find
		 * one, just return nlm4_blocked.
		 */
		mtx_lock(&host->nh_lock);
		TAILQ_FOREACH(af, &host->nh_pending, af_link) {
			if (af->af_fl.l_start == fl.l_start
			    && af->af_fl.l_len == fl.l_len
			    && af->af_fl.l_pid == fl.l_pid
			    && af->af_fl.l_type == fl.l_type) {
				break;
			}
		}
		mtx_unlock(&host->nh_lock);
		if (af) {
			CLNT_RELEASE(client);
			result->stat.stat = nlm4_blocked;
			goto out;
		}

		af = malloc(sizeof(struct nlm_async_lock), M_NLM,
		    M_WAITOK|M_ZERO);
		TASK_INIT(&af->af_task, 0, nlm_lock_callback, af);
		af->af_vp = vs.vs_vp;
		af->af_fl = fl;
		af->af_host = host;
		af->af_rpc = client;
		/*
		 * We use M_RPC here so that we can xdr_free the thing
		 * later.
		 */
		af->af_granted.exclusive = argp->exclusive;
		af->af_granted.alock.caller_name =
			strdup(argp->alock.caller_name, M_RPC);
		nlm_copy_netobj(&af->af_granted.alock.fh,
		    &argp->alock.fh, M_RPC);
		nlm_copy_netobj(&af->af_granted.alock.oh,
		    &argp->alock.oh, M_RPC);
		af->af_granted.alock.svid = argp->alock.svid;
		af->af_granted.alock.l_offset = argp->alock.l_offset;
		af->af_granted.alock.l_len = argp->alock.l_len;

		/*
		 * Put the entry on the pending list before calling
		 * VOP_ADVLOCKASYNC. We do this in case the lock
		 * request was blocked (returning EINPROGRESS) but
		 * then granted before we manage to run again. The
		 * client may receive the granted message before we
		 * send our blocked reply but thats their problem.
		 */
		mtx_lock(&host->nh_lock);
		TAILQ_INSERT_TAIL(&host->nh_pending, af, af_link);
		mtx_unlock(&host->nh_lock);

		error = VOP_ADVLOCKASYNC(vs.vs_vp, NULL, F_SETLK, &fl, F_REMOTE,
		    &af->af_task, &af->af_cookie);

		/*
		 * If the lock completed synchronously, just free the
		 * tracking structure now.
		 */
		if (error != EINPROGRESS) {
			CLNT_RELEASE(af->af_rpc);
			mtx_lock(&host->nh_lock);
			TAILQ_REMOVE(&host->nh_pending, af, af_link);
			mtx_unlock(&host->nh_lock);
			xdr_free((xdrproc_t) xdr_nlm4_testargs,
			    &af->af_granted);
			free(af, M_NLM);
		} else {
			if (nlm_debug_level >= 2)
				printf("NLM: pending async lock %p for %s "
				    "(sysid %d)\n",
				    af, host->nh_caller_name, sysid);
			/*
			 * Don't vrele the vnode just yet - this must
			 * wait until either the async callback
			 * happens or the lock is cancelled.
			 */
			vs.vs_vp = NULL;
		}
	} else {
		error = VOP_ADVLOCK(vs.vs_vp, NULL, F_SETLK, &fl, F_REMOTE);
	}

	if (error) {
		if (error == EINPROGRESS) {
			result->stat.stat = nlm4_blocked;
		} else if (error == EDEADLK) {
			result->stat.stat = nlm4_deadlck;
		} else if (error == EAGAIN) {
			result->stat.stat = nlm4_denied;
		} else {
			result->stat.stat = nlm4_failed;
		}
	} else {
		if (monitor)
			nlm_host_monitor(host, argp->state);
		result->stat.stat = nlm4_granted;
	}       

out:
	nlm_release_vfs_state(&vs);
	if (rpcp)
		*rpcp = nlm_host_get_rpc(host);
	nlm_host_release(host);
	return (0);
}

int
nlm_do_cancel(nlm4_cancargs *argp, nlm4_res *result, struct svc_req *rqstp,
    CLIENT **rpcp)
{
	fhandle_t fh;
	struct vfs_state vs;
	struct nlm_host *host;
	int error, sysid;
	struct flock fl;
	struct nlm_async_lock *af;
	
	memset(result, 0, sizeof(*result));
	memset(&vs, 0, sizeof(vs));

	host = nlm_find_host_by_name(argp->alock.caller_name,
	    (struct sockaddr *) rqstp->rq_xprt->xp_rtaddr.buf, rqstp->rq_vers);
	if (!host) {
		result->stat.stat = nlm4_denied_nolocks;
		return (ENOMEM);
	}

	if (nlm_debug_level >= 3)
		printf("nlm_do_cancel(): caller_name = %s (sysid = %d)\n",
		    host->nh_caller_name, host->nh_sysid);

	nlm_free_finished_locks(host);
	sysid = host->nh_sysid;

	nlm_convert_to_fhandle_t(&fh, &argp->alock.fh);
	nlm_copy_netobj(&result->cookie, &argp->cookie, M_RPC);

	if (time_uptime < nlm_grace_threshold) {
		result->stat.stat = nlm4_denied_grace_period;
		goto out;
	}

	error = nlm_get_vfs_state(host, rqstp, &fh, &vs);
	if (error) {
		result->stat.stat = nlm_convert_error(error);
		goto out;
	}

	fl.l_start = argp->alock.l_offset;
	fl.l_len = argp->alock.l_len;
	fl.l_pid = argp->alock.svid;
	fl.l_sysid = sysid;
	fl.l_whence = SEEK_SET;
	if (argp->exclusive)
		fl.l_type = F_WRLCK;
	else
		fl.l_type = F_RDLCK;

	/*
	 * First we need to try and find the async lock request - if
	 * there isn't one, we give up and return nlm4_denied.
	 */
	mtx_lock(&host->nh_lock);

	TAILQ_FOREACH(af, &host->nh_pending, af_link) {
		if (af->af_fl.l_start == fl.l_start
		    && af->af_fl.l_len == fl.l_len
		    && af->af_fl.l_pid == fl.l_pid
		    && af->af_fl.l_type == fl.l_type) {
			break;
		}
	}

	if (!af) {
		mtx_unlock(&host->nh_lock);
		result->stat.stat = nlm4_denied;
		goto out;
	}

	error = nlm_cancel_async_lock(af);

	if (error) {
		result->stat.stat = nlm4_denied;
	} else {
		result->stat.stat = nlm4_granted;
	}

	mtx_unlock(&host->nh_lock);

out:
	nlm_release_vfs_state(&vs);
	if (rpcp)
		*rpcp = nlm_host_get_rpc(host);
	nlm_host_release(host);
	return (0);
}

int
nlm_do_unlock(nlm4_unlockargs *argp, nlm4_res *result, struct svc_req *rqstp,
    CLIENT **rpcp)
{
	fhandle_t fh;
	struct vfs_state vs;
	struct nlm_host *host;
	int error, sysid;
	struct flock fl;
	
	memset(result, 0, sizeof(*result));
	memset(&vs, 0, sizeof(vs));

	host = nlm_find_host_by_name(argp->alock.caller_name,
	    (struct sockaddr *) rqstp->rq_xprt->xp_rtaddr.buf, rqstp->rq_vers);
	if (!host) {
		result->stat.stat = nlm4_denied_nolocks;
		return (ENOMEM);
	}

	if (nlm_debug_level >= 3)
		printf("nlm_do_unlock(): caller_name = %s (sysid = %d)\n",
		    host->nh_caller_name, host->nh_sysid);

	nlm_free_finished_locks(host);
	sysid = host->nh_sysid;

	nlm_convert_to_fhandle_t(&fh, &argp->alock.fh);
	nlm_copy_netobj(&result->cookie, &argp->cookie, M_RPC);

	if (time_uptime < nlm_grace_threshold) {
		result->stat.stat = nlm4_denied_grace_period;
		goto out;
	}

	error = nlm_get_vfs_state(host, rqstp, &fh, &vs);
	if (error) {
		result->stat.stat = nlm_convert_error(error);
		goto out;
	}

	fl.l_start = argp->alock.l_offset;
	fl.l_len = argp->alock.l_len;
	fl.l_pid = argp->alock.svid;
	fl.l_sysid = sysid;
	fl.l_whence = SEEK_SET;
	fl.l_type = F_UNLCK;
	error = VOP_ADVLOCK(vs.vs_vp, NULL, F_UNLCK, &fl, F_REMOTE);

	/*
	 * Ignore the error - there is no result code for failure,
	 * only for grace period.
	 */
	result->stat.stat = nlm4_granted;

out:
	nlm_release_vfs_state(&vs);
	if (rpcp)
		*rpcp = nlm_host_get_rpc(host);
	nlm_host_release(host);
	return (0);
}

int
nlm_do_granted(nlm4_testargs *argp, nlm4_res *result, struct svc_req *rqstp,

    CLIENT **rpcp)
{
	struct nlm_host *host;
	struct nlm_waiting_lock *nw;
	
	memset(result, 0, sizeof(*result));

	host = nlm_find_host_by_addr(
		(struct sockaddr *) rqstp->rq_xprt->xp_rtaddr.buf,
		rqstp->rq_vers);
	if (!host) {
		result->stat.stat = nlm4_denied_nolocks;
		return (ENOMEM);
	}

	nlm_copy_netobj(&result->cookie, &argp->cookie, M_RPC);
	result->stat.stat = nlm4_denied;

	mtx_lock(&nlm_global_lock);
	TAILQ_FOREACH(nw, &nlm_waiting_locks, nw_link) {
		if (!nw->nw_waiting)
			continue;
		if (argp->alock.svid == nw->nw_lock.svid
		    && argp->alock.l_offset == nw->nw_lock.l_offset
		    && argp->alock.l_len == nw->nw_lock.l_len
		    && argp->alock.fh.n_len == nw->nw_lock.fh.n_len
		    && !memcmp(argp->alock.fh.n_bytes, nw->nw_lock.fh.n_bytes,
			nw->nw_lock.fh.n_len)) {
			nw->nw_waiting = FALSE;
			wakeup(nw);
			result->stat.stat = nlm4_granted;
			break;
		}
	}
	mtx_unlock(&nlm_global_lock);
	if (rpcp)
		*rpcp = nlm_host_get_rpc(host);
	nlm_host_release(host);
	return (0);
}

void
nlm_do_free_all(nlm4_notify *argp)
{
	struct nlm_host *host, *thost;

	TAILQ_FOREACH_SAFE(host, &nlm_hosts, nh_link, thost) {
		if (!strcmp(host->nh_caller_name, argp->name))
			nlm_host_notify(host, argp->state);
	}
}

/*
 * Kernel module glue
 */
static int
nfslockd_modevent(module_t mod, int type, void *data)
{

	return (0);
}
static moduledata_t nfslockd_mod = {
	"nfslockd",
	nfslockd_modevent,
	NULL,
};
DECLARE_MODULE(nfslockd, nfslockd_mod, SI_SUB_VFS, SI_ORDER_ANY);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_DEPEND(nfslockd, krpc, 1, 1, 1);
MODULE_DEPEND(nfslockd, nfs, 1, 1, 1);
MODULE_VERSION(nfslockd, 1);
