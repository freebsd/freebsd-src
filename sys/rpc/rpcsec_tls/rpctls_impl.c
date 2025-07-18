/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

/* Modified from the kernel GSSAPI code for RPC-over-TLS. */

#include <sys/cdefs.h>
#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/tree.h>

#include <net/vnet.h>

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/krpc.h>
#include <rpc/rpcsec_tls.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include "rpctlscd.h"
#include "rpctlssd.h"

/*
 * Syscall hooks
 */
static struct syscall_helper_data rpctls_syscalls[] = {
	SYSCALL_INIT_HELPER(rpctls_syscall),
	SYSCALL_INIT_LAST
};

static struct opaque_auth rpctls_null_verf;

KRPC_VNET_DECLARE(uint64_t, svc_vc_tls_handshake_success);
KRPC_VNET_DECLARE(uint64_t, svc_vc_tls_handshake_failed);

static CLIENT *rpctls_connect_handle;
static CLIENT *rpctls_server_handle;

struct upsock {
	RB_ENTRY(upsock) tree;
	struct socket *so;
	union {
		CLIENT *cl;
		SVCXPRT *xp;
	};
	bool server;
};

static RB_HEAD(upsock_t, upsock) upcall_sockets;
static intptr_t
upsock_compare(const struct upsock *a, const struct upsock *b)
{
	return ((intptr_t)((uintptr_t)a->so/2 - (uintptr_t)b->so/2));
}
RB_GENERATE_STATIC(upsock_t, upsock, tree, upsock_compare);
static struct mtx rpctls_lock;

static enum clnt_stat	rpctls_server(SVCXPRT *xprt, uint32_t *flags,
    uid_t *uid, int *ngrps, gid_t **gids);

static CLIENT *
rpctls_client_nl_create(const char *group, const rpcprog_t program,
    const rpcvers_t version)
{
	CLIENT *cl;

	cl = client_nl_create(group, program, version);
	KASSERT(cl, ("%s: netlink client already exist", __func__));
	/*
	 * Set the try_count to 1 so that no retries of the RPC occur.  Since
	 * it is an upcall to a local daemon, requests should not be lost and
	 * doing one of these RPCs multiple times is not correct.  If the
	 * server is not working correctly, the daemon can get stuck in
	 * SSL_connect() trying to read data from the socket during the upcall.
	 * Set a timeout (currently 15sec) and assume the daemon is hung when
	 *  the timeout occurs.
	 */
	clnt_control(cl, CLSET_RETRIES, &(int){1});
	clnt_control(cl, CLSET_TIMEOUT, &(struct timeval){.tv_sec = 15});
	clnt_control(cl, CLSET_WAITCHAN, __DECONST(char *, group));

	return (cl);
}

int
rpctls_init(void)
{
	int error;

	error = syscall_helper_register(rpctls_syscalls, SY_THR_STATIC_KLD);
	if (error != 0) {
		printf("rpctls_init: cannot register syscall\n");
		return (error);
	}
	mtx_init(&rpctls_lock, "rpctls lock", NULL, MTX_DEF);
	rpctls_null_verf.oa_flavor = AUTH_NULL;
	rpctls_null_verf.oa_base = RPCTLS_START_STRING;
	rpctls_null_verf.oa_length = strlen(RPCTLS_START_STRING);
	rpctls_connect_handle = rpctls_client_nl_create("tlsclnt",
	    RPCTLSCD, RPCTLSCDVERS);
	rpctls_server_handle = rpctls_client_nl_create("tlsserv",
	    RPCTLSSD, RPCTLSSDVERS);
	return (0);
}

int
sys_rpctls_syscall(struct thread *td, struct rpctls_syscall_args *uap)
{
	struct file *fp;
	struct upsock *upsp, ups;
	int fd = -1, error;
        
	error = priv_check(td, PRIV_NFS_DAEMON);
	if (error != 0)
		return (error);

	KRPC_CURVNET_SET(KRPC_TD_TO_VNET(td));
	mtx_lock(&rpctls_lock);
	upsp = RB_FIND(upsock_t, &upcall_sockets,
	    &(struct upsock){
	    .so = __DECONST(struct socket *, uap->socookie) });
	if (__predict_true(upsp != NULL)) {
		RB_REMOVE(upsock_t, &upcall_sockets, upsp);
		/*
		 * The upsp points to stack of NFS mounting thread.  Even
		 * though we removed it from the tree, we still don't own it.
		 * Make a copy before releasing the lock.  The mounting thread
		 * may timeout the RPC and unroll its stack.
		 */
		ups = *upsp;
	}
	mtx_unlock(&rpctls_lock);
	if (upsp == NULL) {
		KRPC_CURVNET_RESTORE();
		printf("%s: socket lookup failed\n", __func__);
		return (EPERM);
	}
	if ((error = falloc(td, &fp, &fd, 0)) != 0) {
		/*
		 * The socket will not be acquired by the daemon,
		 * but has been removed from the upcall socket RB.
		 * As such, it needs to be closed here.
		 */
		soclose(ups.so);
		KRPC_CURVNET_RESTORE();
		return (error);
	}
	soref(ups.so);
	if (ups.server) {
		/*
		 * Once this file descriptor is associated
		 * with the socket, it cannot be closed by
		 * the server side krpc code (svc_vc.c).
		 */
		sx_xlock(&ups.xp->xp_lock);
		ups.xp->xp_tls = RPCTLS_FLAGS_HANDSHFAIL;
		sx_xunlock(&ups.xp->xp_lock);
	} else {
		/*
		 * Initialize TLS state so that clnt_vc_destroy() will
		 * not close the socket and will leave that for the
		 * daemon to do.
		 */
		CLNT_CONTROL(ups.cl, CLSET_TLS, &(int){RPCTLS_INHANDSHAKE});
	}
	finit(fp, FREAD | FWRITE, DTYPE_SOCKET, ups.so, &socketops);
	fdrop(fp, td);	/* Drop fp reference. */
	td->td_retval[0] = fd;
	KRPC_CURVNET_RESTORE();

	return (error);
}

/* Error handling for both client and server failed RPC upcalls. */
static void
rpctls_rpc_failed(struct upsock *ups, struct socket *so)
{

	mtx_lock(&rpctls_lock);
	if (RB_FIND(upsock_t, &upcall_sockets, ups)) {
		struct upsock *removed __diagused;

		removed = RB_REMOVE(upsock_t, &upcall_sockets, ups);
		mtx_unlock(&rpctls_lock);
		MPASS(removed == ups);
		/*
		 * Since the socket was still in the RB tree when
		 * this function was called, the daemon will not
		 * close it.  As such, it needs to be closed here.
		 */
		soclose(so);
	} else {
		/*
		 * The daemon has taken the socket from the tree, but
		 * failed to do the handshake.
		 */
		mtx_unlock(&rpctls_lock);
		/*
		 * Do a shutdown on the socket, since the daemon is
		 * probably stuck in SSL_accept() or SSL_connect() trying to
		 * read the socket.  Do not soclose() the socket, since the
		 * daemon will close() the socket after SSL_accept()
		 * returns an error.
		 */
		soshutdown(so, SHUT_RD);
	}
}

/* Do an upcall for a new socket connect using TLS. */
enum clnt_stat
rpctls_connect(CLIENT *newclient, char *certname, struct socket *so,
    uint32_t *reterr)
{
	struct rpctlscd_connect_arg arg;
	struct rpctlscd_connect_res res;
	struct rpc_callextra ext;
	enum clnt_stat stat;
	struct upsock ups = {
		.so = so,
		.cl = newclient,
		.server = false,
	};

	/* First, do the AUTH_TLS NULL RPC. */
	memset(&ext, 0, sizeof(ext));
	ext.rc_auth = authtls_create();
	stat = clnt_call_private(newclient, &ext, NULLPROC, (xdrproc_t)xdr_void,
	    NULL, (xdrproc_t)xdr_void, NULL, (struct timeval){ .tv_sec = 30 });
	AUTH_DESTROY(ext.rc_auth);
	if (stat == RPC_AUTHERROR)
		return (stat);
	if (stat != RPC_SUCCESS)
		return (RPC_SYSTEMERROR);

	mtx_lock(&rpctls_lock);
	RB_INSERT(upsock_t, &upcall_sockets, &ups);
	mtx_unlock(&rpctls_lock);

	/* Temporarily block reception during the handshake upcall. */
	CLNT_CONTROL(newclient, CLSET_BLOCKRCV, &(int){1});

	/* Do the connect handshake upcall. */
	if (certname != NULL) {
		arg.certname.certname_len = strlen(certname);
		arg.certname.certname_val = certname;
	} else
		arg.certname.certname_len = 0;
	arg.socookie = (uint64_t)so;
	stat = rpctlscd_connect_2(&arg, &res, rpctls_connect_handle);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	else
		rpctls_rpc_failed(&ups, so);

	/* Unblock reception. */
	CLNT_CONTROL(newclient, CLSET_BLOCKRCV, &(int){0});

#ifdef INVARIANTS
	mtx_lock(&rpctls_lock);
	MPASS((RB_FIND(upsock_t, &upcall_sockets, &ups) == NULL));
	mtx_unlock(&rpctls_lock);
#endif

	return (stat);
}

/* Do an upcall to handle an non-application data record using TLS. */
enum clnt_stat
rpctls_cl_handlerecord(void *socookie, uint32_t *reterr)
{
	struct rpctlscd_handlerecord_arg arg;
	struct rpctlscd_handlerecord_res res;
	enum clnt_stat stat;

	/* Do the handlerecord upcall. */
	arg.socookie = (uint64_t)socookie;
	stat = rpctlscd_handlerecord_2(&arg, &res, rpctls_connect_handle);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

enum clnt_stat
rpctls_srv_handlerecord(void *socookie, uint32_t *reterr)
{
	struct rpctlssd_handlerecord_arg arg;
	struct rpctlssd_handlerecord_res res;
	enum clnt_stat stat;

	/* Do the handlerecord upcall. */
	arg.socookie = (uint64_t)socookie;
	stat = rpctlssd_handlerecord_2(&arg, &res, rpctls_server_handle);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

/* Do an upcall to shut down a socket using TLS. */
enum clnt_stat
rpctls_cl_disconnect(void *socookie, uint32_t *reterr)
{
	struct rpctlscd_disconnect_arg arg;
	struct rpctlscd_disconnect_res res;
	enum clnt_stat stat;

	/* Do the disconnect upcall. */
	arg.socookie = (uint64_t)socookie;
	stat = rpctlscd_disconnect_2(&arg, &res, rpctls_connect_handle);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

enum clnt_stat
rpctls_srv_disconnect(void *socookie, uint32_t *reterr)
{
	struct rpctlssd_disconnect_arg arg;
	struct rpctlssd_disconnect_res res;
	enum clnt_stat stat;

	/* Do the disconnect upcall. */
	arg.socookie = (uint64_t)socookie;
	stat = rpctlssd_disconnect_2(&arg, &res, rpctls_server_handle);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

/* Do an upcall for a new server socket using TLS. */
static enum clnt_stat
rpctls_server(SVCXPRT *xprt, uint32_t *flags, uid_t *uid, int *ngrps,
    gid_t **gids)
{
	enum clnt_stat stat;
	struct upsock ups = {
		.so = xprt->xp_socket,
		.xp = xprt,
		.server = true,
	};
	struct rpctlssd_connect_arg arg;
	struct rpctlssd_connect_res res;
	gid_t *gidp;
	uint32_t *gidv;
	int i;

	mtx_lock(&rpctls_lock);
	RB_INSERT(upsock_t, &upcall_sockets, &ups);
	mtx_unlock(&rpctls_lock);

	/* Do the server upcall. */
	res.gid.gid_val = NULL;
	arg.socookie = (uint64_t)xprt->xp_socket;
	stat = rpctlssd_connect_2(&arg, &res, rpctls_server_handle);
	if (stat == RPC_SUCCESS) {
		*flags = res.flags;
		if ((*flags & (RPCTLS_FLAGS_CERTUSER |
		    RPCTLS_FLAGS_DISABLED)) == RPCTLS_FLAGS_CERTUSER) {
			*ngrps = res.gid.gid_len;
			*uid = res.uid;
			*gids = gidp = mem_alloc(*ngrps * sizeof(gid_t));
			gidv = res.gid.gid_val;
			for (i = 0; i < *ngrps; i++)
				*gidp++ = *gidv++;
		}
	} else
		rpctls_rpc_failed(&ups, xprt->xp_socket);

	mem_free(res.gid.gid_val, 0);

#ifdef INVARIANTS
	mtx_lock(&rpctls_lock);
	MPASS((RB_FIND(upsock_t, &upcall_sockets, &ups) == NULL));
	mtx_unlock(&rpctls_lock);
#endif

	return (stat);
}

/*
 * Handle the NULL RPC with authentication flavor of AUTH_TLS.
 * This is a STARTTLS command, so do the upcall to the rpctlssd daemon,
 * which will do the TLS handshake.
 */
enum auth_stat
_svcauth_rpcsec_tls(struct svc_req *rqst, struct rpc_msg *msg)

{
	bool_t call_stat;
	enum clnt_stat stat;
	SVCXPRT *xprt;
	uint32_t flags;
	int ngrps;
	uid_t uid;
	gid_t *gidp;
#ifdef KERN_TLS
	u_int maxlen;
#endif
	
	KRPC_CURVNET_SET_QUIET(KRPC_TD_TO_VNET(curthread));
	KRPC_VNET(svc_vc_tls_handshake_failed)++;
	/* Initialize reply. */
	rqst->rq_verf = rpctls_null_verf;

	/* Check client credentials. */
	if (rqst->rq_cred.oa_length != 0 ||
	    msg->rm_call.cb_verf.oa_length != 0 ||
	    msg->rm_call.cb_verf.oa_flavor != AUTH_NULL) {
		KRPC_CURVNET_RESTORE();
		return (AUTH_BADCRED);
	}
	
	if (rqst->rq_proc != NULLPROC) {
		KRPC_CURVNET_RESTORE();
		return (AUTH_REJECTEDCRED);
	}

	call_stat = FALSE;
#ifdef KERN_TLS
	if (rpctls_getinfo(&maxlen, false, true))
		call_stat = TRUE;
#endif
	if (!call_stat) {
		KRPC_CURVNET_RESTORE();
		return (AUTH_REJECTEDCRED);
	}

	/*
	 * Disable reception for the krpc so that the TLS handshake can
	 * be done on the socket in the rpctlssd daemon.
	 */
	xprt = rqst->rq_xprt;
	sx_xlock(&xprt->xp_lock);
	xprt->xp_dontrcv = TRUE;
	sx_xunlock(&xprt->xp_lock);

	/*
	 * Send the reply to the NULL RPC with AUTH_TLS, which is the
	 * STARTTLS command for Sun RPC.
	 */
	call_stat = svc_sendreply(rqst, (xdrproc_t)xdr_void, NULL);
	if (!call_stat) {
		sx_xlock(&xprt->xp_lock);
		xprt->xp_dontrcv = FALSE;
		sx_xunlock(&xprt->xp_lock);
		xprt_active(xprt);	/* Harmless if already active. */
		KRPC_CURVNET_RESTORE();
		return (AUTH_REJECTEDCRED);
	}

	/* Do an upcall to do the TLS handshake. */
	stat = rpctls_server(xprt, &flags, &uid, &ngrps, &gidp);

	/* Re-enable reception on the socket within the krpc. */
	sx_xlock(&xprt->xp_lock);
	xprt->xp_dontrcv = FALSE;
	if (stat == RPC_SUCCESS) {
		xprt->xp_tls = flags;
		if ((flags & (RPCTLS_FLAGS_CERTUSER |
		    RPCTLS_FLAGS_DISABLED)) == RPCTLS_FLAGS_CERTUSER) {
			xprt->xp_ngrps = ngrps;
			xprt->xp_uid = uid;
			xprt->xp_gidp = gidp;
		}
		KRPC_VNET(svc_vc_tls_handshake_failed)--;
		KRPC_VNET(svc_vc_tls_handshake_success)++;
	}
	sx_xunlock(&xprt->xp_lock);
	xprt_active(xprt);		/* Harmless if already active. */
	KRPC_CURVNET_RESTORE();

	return (RPCSEC_GSS_NODISPATCH);
}

/*
 * Get kern.ipc.tls.enable and kern.ipc.tls.maxlen.
 */
bool
rpctls_getinfo(u_int *maxlenp, bool rpctlscd_run, bool rpctlssd_run)
{
	u_int maxlen;
	bool enable;
	int error;
	size_t siz;

	if (!mb_use_ext_pgs)
		return (false);
	siz = sizeof(enable);
	error = kernel_sysctlbyname(curthread, "kern.ipc.tls.enable",
	    &enable, &siz, NULL, 0, NULL, 0);
	if (error != 0)
		return (false);
	siz = sizeof(maxlen);
	error = kernel_sysctlbyname(curthread, "kern.ipc.tls.maxlen",
	    &maxlen, &siz, NULL, 0, NULL, 0);
	if (error != 0)
		return (false);
	*maxlenp = maxlen;
	return (enable);
}
