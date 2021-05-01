/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");

#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/file.h>
#include <sys/filedesc.h>
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

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
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

static CLIENT		*rpctls_connect_handle;
static struct mtx	rpctls_connect_lock;
static struct socket	*rpctls_connect_so = NULL;
static CLIENT		*rpctls_connect_cl = NULL;
static CLIENT		*rpctls_server_handle;
static struct mtx	rpctls_server_lock;
static struct socket	*rpctls_server_so = NULL;
static SVCXPRT		*rpctls_server_xprt = NULL;
static struct opaque_auth rpctls_null_verf;

static CLIENT		*rpctls_connect_client(void);
static CLIENT		*rpctls_server_client(void);
static enum clnt_stat	rpctls_server(SVCXPRT *xprt, struct socket *so,
			    uint32_t *flags, uint64_t *sslp,
			    uid_t *uid, int *ngrps, gid_t **gids);

int
rpctls_init(void)
{
	int error;

	error = syscall_helper_register(rpctls_syscalls, SY_THR_STATIC_KLD);
	if (error != 0) {
		printf("rpctls_init: cannot register syscall\n");
		return (error);
	}
	mtx_init(&rpctls_connect_lock, "rpctls_connect_lock", NULL,
	    MTX_DEF);
	mtx_init(&rpctls_server_lock, "rpctls_server_lock", NULL,
	    MTX_DEF);
	rpctls_null_verf.oa_flavor = AUTH_NULL;
	rpctls_null_verf.oa_base = RPCTLS_START_STRING;
	rpctls_null_verf.oa_length = strlen(RPCTLS_START_STRING);
	return (0);
}

int
sys_rpctls_syscall(struct thread *td, struct rpctls_syscall_args *uap)
{
        struct sockaddr_un sun;
        struct netconfig *nconf;
	struct file *fp;
	struct socket *so;
	SVCXPRT *xprt;
	char path[MAXPATHLEN];
	int fd = -1, error, try_count;
	CLIENT *cl, *oldcl, *concl;
	uint64_t ssl[3];
	struct timeval timeo;
#ifdef KERN_TLS
	u_int maxlen;
#endif
        
	error = priv_check(td, PRIV_NFS_DAEMON);
	if (error != 0)
		return (error);

	switch (uap->op) {
	case RPCTLS_SYSC_CLSETPATH:
		error = copyinstr(uap->path, path, sizeof(path), NULL);
		if (error == 0) {
			error = ENXIO;
#ifdef KERN_TLS
			if (rpctls_getinfo(&maxlen, false, false))
				error = 0;
#endif
		}
		if (error == 0 && (strlen(path) + 1 > sizeof(sun.sun_path) ||
		    strlen(path) == 0))
			error = EINVAL;
	
		cl = NULL;
		if (error == 0) {
			sun.sun_family = AF_LOCAL;
			strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
			sun.sun_len = SUN_LEN(&sun);
			
			nconf = getnetconfigent("local");
			cl = clnt_reconnect_create(nconf,
			    (struct sockaddr *)&sun, RPCTLSCD, RPCTLSCDVERS,
			    RPC_MAXDATASIZE, RPC_MAXDATASIZE);
			/*
			 * The number of retries defaults to INT_MAX, which
			 * effectively means an infinite, uninterruptable loop. 
			 * Set the try_count to 1 so that no retries of the
			 * RPC occur.  Since it is an upcall to a local daemon,
			 * requests should not be lost and doing one of these
			 * RPCs multiple times is not correct.
			 * If the server is not working correctly, the
			 * daemon can get stuck in SSL_connect() trying
			 * to read data from the socket during the upcall.
			 * Set a timeout (currently 15sec) and assume the
			 * daemon is hung when the timeout occurs.
			 */
			if (cl != NULL) {
				try_count = 1;
				CLNT_CONTROL(cl, CLSET_RETRIES, &try_count);
				timeo.tv_sec = 15;
				timeo.tv_usec = 0;
				CLNT_CONTROL(cl, CLSET_TIMEOUT, &timeo);
			} else
				error = EINVAL;
		}
	
		mtx_lock(&rpctls_connect_lock);
		oldcl = rpctls_connect_handle;
		rpctls_connect_handle = cl;
		mtx_unlock(&rpctls_connect_lock);
	
		if (oldcl != NULL) {
			CLNT_CLOSE(oldcl);
			CLNT_RELEASE(oldcl);
		}
		break;
	case RPCTLS_SYSC_SRVSETPATH:
		error = copyinstr(uap->path, path, sizeof(path), NULL);
		if (error == 0) {
			error = ENXIO;
#ifdef KERN_TLS
			if (rpctls_getinfo(&maxlen, false, false))
				error = 0;
#endif
		}
		if (error == 0 && (strlen(path) + 1 > sizeof(sun.sun_path) ||
		    strlen(path) == 0))
			error = EINVAL;
	
		cl = NULL;
		if (error == 0) {
			sun.sun_family = AF_LOCAL;
			strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
			sun.sun_len = SUN_LEN(&sun);
			
			nconf = getnetconfigent("local");
			cl = clnt_reconnect_create(nconf,
			    (struct sockaddr *)&sun, RPCTLSSD, RPCTLSSDVERS,
			    RPC_MAXDATASIZE, RPC_MAXDATASIZE);
			/*
			 * The number of retries defaults to INT_MAX, which
			 * effectively means an infinite, uninterruptable loop. 
			 * Set the try_count to 1 so that no retries of the
			 * RPC occur.  Since it is an upcall to a local daemon,
			 * requests should not be lost and doing one of these
			 * RPCs multiple times is not correct.
			 * Set a timeout (currently 15sec) and assume that
			 * the daemon is hung if a timeout occurs.
			 */
			if (cl != NULL) {
				try_count = 1;
				CLNT_CONTROL(cl, CLSET_RETRIES, &try_count);
				timeo.tv_sec = 15;
				timeo.tv_usec = 0;
				CLNT_CONTROL(cl, CLSET_TIMEOUT, &timeo);
			} else
				error = EINVAL;
		}
	
		mtx_lock(&rpctls_server_lock);
		oldcl = rpctls_server_handle;
		rpctls_server_handle = cl;
		mtx_unlock(&rpctls_server_lock);
	
		if (oldcl != NULL) {
			CLNT_CLOSE(oldcl);
			CLNT_RELEASE(oldcl);
		}
		break;
	case RPCTLS_SYSC_CLSHUTDOWN:
		mtx_lock(&rpctls_connect_lock);
		oldcl = rpctls_connect_handle;
		rpctls_connect_handle = NULL;
		mtx_unlock(&rpctls_connect_lock);
	
		if (oldcl != NULL) {
			CLNT_CLOSE(oldcl);
			CLNT_RELEASE(oldcl);
		}
		break;
	case RPCTLS_SYSC_SRVSHUTDOWN:
		mtx_lock(&rpctls_server_lock);
		oldcl = rpctls_server_handle;
		rpctls_server_handle = NULL;
		mtx_unlock(&rpctls_server_lock);
	
		if (oldcl != NULL) {
			CLNT_CLOSE(oldcl);
			CLNT_RELEASE(oldcl);
		}
		break;
	case RPCTLS_SYSC_CLSOCKET:
		mtx_lock(&rpctls_connect_lock);
		so = rpctls_connect_so;
		rpctls_connect_so = NULL;
		concl = rpctls_connect_cl;
		rpctls_connect_cl = NULL;
		mtx_unlock(&rpctls_connect_lock);
		if (so != NULL) {
			error = falloc(td, &fp, &fd, 0);
			if (error == 0) {
				/*
				 * Set ssl refno so that clnt_vc_destroy() will
				 * not close the socket and will leave that for
				 * the daemon to do.
				 */
				soref(so);
				ssl[0] = ssl[1] = 0;
				ssl[2] = RPCTLS_REFNO_HANDSHAKE;
				CLNT_CONTROL(concl, CLSET_TLS, ssl);
				finit(fp, FREAD | FWRITE, DTYPE_SOCKET, so,
				    &socketops);
				fdrop(fp, td);	/* Drop fp reference. */
				td->td_retval[0] = fd;
			}
		} else
			error = EPERM;
		break;
	case RPCTLS_SYSC_SRVSOCKET:
		mtx_lock(&rpctls_server_lock);
		so = rpctls_server_so;
		rpctls_server_so = NULL;
		xprt = rpctls_server_xprt;
		rpctls_server_xprt = NULL;
		mtx_unlock(&rpctls_server_lock);
		if (so != NULL) {
			error = falloc(td, &fp, &fd, 0);
			if (error == 0) {
				/*
				 * Once this file descriptor is associated
				 * with the socket, it cannot be closed by
				 * the server side krpc code (svc_vc.c).
				 */
				soref(so);
				sx_xlock(&xprt->xp_lock);
				xprt->xp_tls = RPCTLS_FLAGS_HANDSHFAIL;
				sx_xunlock(&xprt->xp_lock);
				finit(fp, FREAD | FWRITE, DTYPE_SOCKET, so,
				    &socketops);
				fdrop(fp, td);	/* Drop fp reference. */
				td->td_retval[0] = fd;
			}
		} else
			error = EPERM;
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

/*
 * Acquire the rpctls_connect_handle and return it with a reference count,
 * if it is available.
 */
static CLIENT *
rpctls_connect_client(void)
{
	CLIENT *cl;

	mtx_lock(&rpctls_connect_lock);
	cl = rpctls_connect_handle;
	if (cl != NULL)
		CLNT_ACQUIRE(cl);
	mtx_unlock(&rpctls_connect_lock);
	return (cl);
}

/*
 * Acquire the rpctls_server_handle and return it with a reference count,
 * if it is available.
 */
static CLIENT *
rpctls_server_client(void)
{
	CLIENT *cl;

	mtx_lock(&rpctls_server_lock);
	cl = rpctls_server_handle;
	if (cl != NULL)
		CLNT_ACQUIRE(cl);
	mtx_unlock(&rpctls_server_lock);
	return (cl);
}

/* Do an upcall for a new socket connect using TLS. */
enum clnt_stat
rpctls_connect(CLIENT *newclient, char *certname, struct socket *so,
    uint64_t *sslp, uint32_t *reterr)
{
	struct rpctlscd_connect_arg arg;
	struct rpctlscd_connect_res res;
	struct rpc_callextra ext;
	struct timeval utimeout;
	enum clnt_stat stat;
	CLIENT *cl;
	int val;
	static bool rpctls_connect_busy = false;

	cl = rpctls_connect_client();
	if (cl == NULL)
		return (RPC_AUTHERROR);

	/* First, do the AUTH_TLS NULL RPC. */
	memset(&ext, 0, sizeof(ext));
	utimeout.tv_sec = 30;
	utimeout.tv_usec = 0;
	ext.rc_auth = authtls_create();
	stat = clnt_call_private(newclient, &ext, NULLPROC, (xdrproc_t)xdr_void,
	    NULL, (xdrproc_t)xdr_void, NULL, utimeout);
	AUTH_DESTROY(ext.rc_auth);
	if (stat == RPC_AUTHERROR)
		return (stat);
	if (stat != RPC_SUCCESS)
		return (RPC_SYSTEMERROR);

	/* Serialize the connect upcalls. */
	mtx_lock(&rpctls_connect_lock);
	while (rpctls_connect_busy)
		msleep(&rpctls_connect_busy, &rpctls_connect_lock, PVFS,
		    "rtlscn", 0);
	rpctls_connect_busy = true;
	rpctls_connect_so = so;
	rpctls_connect_cl = newclient;
	mtx_unlock(&rpctls_connect_lock);

	/* Temporarily block reception during the handshake upcall. */
	val = 1;
	CLNT_CONTROL(newclient, CLSET_BLOCKRCV, &val);

	/* Do the connect handshake upcall. */
	if (certname != NULL) {
		arg.certname.certname_len = strlen(certname);
		arg.certname.certname_val = certname;
	} else
		arg.certname.certname_len = 0;
	stat = rpctlscd_connect_1(&arg, &res, cl);
	if (stat == RPC_SUCCESS) {
		*reterr = res.reterr;
		if (res.reterr == 0) {
			*sslp++ = res.sec;
			*sslp++ = res.usec;
			*sslp = res.ssl;
		}
	} else if (stat == RPC_TIMEDOUT) {
		/*
		 * Do a shutdown on the socket, since the daemon is probably
		 * stuck in SSL_connect() trying to read the socket.
		 * Do not soclose() the socket, since the daemon will close()
		 * the socket after SSL_connect() returns an error.
		 */
		soshutdown(so, SHUT_RD);
	}
	CLNT_RELEASE(cl);

	/* Unblock reception. */
	val = 0;
	CLNT_CONTROL(newclient, CLSET_BLOCKRCV, &val);

	/* Once the upcall is done, the daemon is done with the fp and so. */
	mtx_lock(&rpctls_connect_lock);
	rpctls_connect_so = NULL;
	rpctls_connect_cl = NULL;
	rpctls_connect_busy = false;
	wakeup(&rpctls_connect_busy);
	mtx_unlock(&rpctls_connect_lock);

	return (stat);
}

/* Do an upcall to handle an non-application data record using TLS. */
enum clnt_stat
rpctls_cl_handlerecord(uint64_t sec, uint64_t usec, uint64_t ssl,
    uint32_t *reterr)
{
	struct rpctlscd_handlerecord_arg arg;
	struct rpctlscd_handlerecord_res res;
	enum clnt_stat stat;
	CLIENT *cl;

	cl = rpctls_connect_client();
	if (cl == NULL) {
		*reterr = RPCTLSERR_NOSSL;
		return (RPC_SUCCESS);
	}

	/* Do the handlerecord upcall. */
	arg.sec = sec;
	arg.usec = usec;
	arg.ssl = ssl;
	stat = rpctlscd_handlerecord_1(&arg, &res, cl);
	CLNT_RELEASE(cl);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

enum clnt_stat
rpctls_srv_handlerecord(uint64_t sec, uint64_t usec, uint64_t ssl,
    uint32_t *reterr)
{
	struct rpctlssd_handlerecord_arg arg;
	struct rpctlssd_handlerecord_res res;
	enum clnt_stat stat;
	CLIENT *cl;

	cl = rpctls_server_client();
	if (cl == NULL) {
		*reterr = RPCTLSERR_NOSSL;
		return (RPC_SUCCESS);
	}

	/* Do the handlerecord upcall. */
	arg.sec = sec;
	arg.usec = usec;
	arg.ssl = ssl;
	stat = rpctlssd_handlerecord_1(&arg, &res, cl);
	CLNT_RELEASE(cl);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

/* Do an upcall to shut down a socket using TLS. */
enum clnt_stat
rpctls_cl_disconnect(uint64_t sec, uint64_t usec, uint64_t ssl,
    uint32_t *reterr)
{
	struct rpctlscd_disconnect_arg arg;
	struct rpctlscd_disconnect_res res;
	enum clnt_stat stat;
	CLIENT *cl;

	cl = rpctls_connect_client();
	if (cl == NULL) {
		*reterr = RPCTLSERR_NOSSL;
		return (RPC_SUCCESS);
	}

	/* Do the disconnect upcall. */
	arg.sec = sec;
	arg.usec = usec;
	arg.ssl = ssl;
	stat = rpctlscd_disconnect_1(&arg, &res, cl);
	CLNT_RELEASE(cl);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

enum clnt_stat
rpctls_srv_disconnect(uint64_t sec, uint64_t usec, uint64_t ssl,
    uint32_t *reterr)
{
	struct rpctlssd_disconnect_arg arg;
	struct rpctlssd_disconnect_res res;
	enum clnt_stat stat;
	CLIENT *cl;

	cl = rpctls_server_client();
	if (cl == NULL) {
		*reterr = RPCTLSERR_NOSSL;
		return (RPC_SUCCESS);
	}

	/* Do the disconnect upcall. */
	arg.sec = sec;
	arg.usec = usec;
	arg.ssl = ssl;
	stat = rpctlssd_disconnect_1(&arg, &res, cl);
	CLNT_RELEASE(cl);
	if (stat == RPC_SUCCESS)
		*reterr = res.reterr;
	return (stat);
}

/* Do an upcall for a new server socket using TLS. */
static enum clnt_stat
rpctls_server(SVCXPRT *xprt, struct socket *so, uint32_t *flags, uint64_t *sslp,
    uid_t *uid, int *ngrps, gid_t **gids)
{
	enum clnt_stat stat;
	CLIENT *cl;
	struct rpctlssd_connect_res res;
	gid_t *gidp;
	uint32_t *gidv;
	int i;
	static bool rpctls_server_busy = false;

	cl = rpctls_server_client();
	if (cl == NULL)
		return (RPC_SYSTEMERROR);

	/* Serialize the server upcalls. */
	mtx_lock(&rpctls_server_lock);
	while (rpctls_server_busy)
		msleep(&rpctls_server_busy, &rpctls_server_lock, PVFS,
		    "rtlssn", 0);
	rpctls_server_busy = true;
	rpctls_server_so = so;
	rpctls_server_xprt = xprt;
	mtx_unlock(&rpctls_server_lock);

	/* Do the server upcall. */
	res.gid.gid_val = NULL;
	stat = rpctlssd_connect_1(NULL, &res, cl);
	if (stat == RPC_SUCCESS) {
		*flags = res.flags;
		*sslp++ = res.sec;
		*sslp++ = res.usec;
		*sslp = res.ssl;
		if ((*flags & (RPCTLS_FLAGS_CERTUSER |
		    RPCTLS_FLAGS_DISABLED)) == RPCTLS_FLAGS_CERTUSER) {
			*ngrps = res.gid.gid_len;
			*uid = res.uid;
			*gids = gidp = mem_alloc(*ngrps * sizeof(gid_t));
			gidv = res.gid.gid_val;
			for (i = 0; i < *ngrps; i++)
				*gidp++ = *gidv++;
		}
	} else if (stat == RPC_TIMEDOUT) {
		/*
		 * Do a shutdown on the socket, since the daemon is probably
		 * stuck in SSL_accept() trying to read the socket.
		 * Do not soclose() the socket, since the daemon will close()
		 * the socket after SSL_accept() returns an error.
		 */
		soshutdown(so, SHUT_RD);
	}
	CLNT_RELEASE(cl);
	mem_free(res.gid.gid_val, 0);

	/* Once the upcall is done, the daemon is done with the fp and so. */
	mtx_lock(&rpctls_server_lock);
	rpctls_server_so = NULL;
	rpctls_server_xprt = NULL;
	rpctls_server_busy = false;
	wakeup(&rpctls_server_busy);
	mtx_unlock(&rpctls_server_lock);

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
	uint64_t ssl[3];
	int ngrps;
	uid_t uid;
	gid_t *gidp;
#ifdef KERN_TLS
	u_int maxlen;
#endif
	
	/* Initialize reply. */
	rqst->rq_verf = rpctls_null_verf;

	/* Check client credentials. */
	if (rqst->rq_cred.oa_length != 0 ||
	    msg->rm_call.cb_verf.oa_length != 0 ||
	    msg->rm_call.cb_verf.oa_flavor != AUTH_NULL)
		return (AUTH_BADCRED);
	
	if (rqst->rq_proc != NULLPROC)
		return (AUTH_REJECTEDCRED);

	call_stat = FALSE;
#ifdef KERN_TLS
	if (rpctls_getinfo(&maxlen, false, true))
		call_stat = TRUE;
#endif
	if (!call_stat)
		return (AUTH_REJECTEDCRED);

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
		return (AUTH_REJECTEDCRED);
	}

	/* Do an upcall to do the TLS handshake. */
	stat = rpctls_server(xprt, xprt->xp_socket, &flags,
	    ssl, &uid, &ngrps, &gidp);

	/* Re-enable reception on the socket within the krpc. */
	sx_xlock(&xprt->xp_lock);
	xprt->xp_dontrcv = FALSE;
	if (stat == RPC_SUCCESS) {
		xprt->xp_tls = flags;
		xprt->xp_sslsec = ssl[0];
		xprt->xp_sslusec = ssl[1];
		xprt->xp_sslrefno = ssl[2];
		if ((flags & (RPCTLS_FLAGS_CERTUSER |
		    RPCTLS_FLAGS_DISABLED)) == RPCTLS_FLAGS_CERTUSER) {
			xprt->xp_ngrps = ngrps;
			xprt->xp_uid = uid;
			xprt->xp_gidp = gidp;
		}
	}
	sx_xunlock(&xprt->xp_lock);
	xprt_active(xprt);		/* Harmless if already active. */

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

	if (PMAP_HAS_DMAP == 0 || !mb_use_ext_pgs)
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
	if (rpctlscd_run && rpctls_connect_handle == NULL)
		return (false);
	if (rpctlssd_run && rpctls_server_handle == NULL)
		return (false);
	*maxlenp = maxlen;
	return (enable);
}

