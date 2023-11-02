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

#include <net/vnet.h>

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
static struct mtx	rpctls_server_lock;
static struct opaque_auth rpctls_null_verf;

KRPC_VNET_DECLARE(uint64_t, svc_vc_tls_handshake_success);
KRPC_VNET_DECLARE(uint64_t, svc_vc_tls_handshake_failed);

KRPC_VNET_DEFINE_STATIC(CLIENT **, rpctls_server_handle);
KRPC_VNET_DEFINE_STATIC(struct socket *, rpctls_server_so) = NULL;
KRPC_VNET_DEFINE_STATIC(SVCXPRT *, rpctls_server_xprt) = NULL;
KRPC_VNET_DEFINE_STATIC(bool, rpctls_srv_newdaemon) = false;
KRPC_VNET_DEFINE_STATIC(int, rpctls_srv_prevproc) = 0;
KRPC_VNET_DEFINE_STATIC(bool *, rpctls_server_busy);

static CLIENT		*rpctls_connect_client(void);
static CLIENT		*rpctls_server_client(int procpos);
static enum clnt_stat	rpctls_server(SVCXPRT *xprt, struct socket *so,
			    uint32_t *flags, uint64_t *sslp,
			    uid_t *uid, int *ngrps, gid_t **gids,
			    int *procposp);

static void
rpctls_vnetinit(const void *unused __unused)
{
	int i;

	KRPC_VNET(rpctls_server_handle) = malloc(sizeof(CLIENT *) *
	    RPCTLS_SRV_MAXNPROCS, M_RPC, M_WAITOK | M_ZERO);
	KRPC_VNET(rpctls_server_busy) = malloc(sizeof(bool) *
	    RPCTLS_SRV_MAXNPROCS, M_RPC, M_WAITOK | M_ZERO);
	for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++)
		KRPC_VNET(rpctls_server_busy)[i] = false;
}
VNET_SYSINIT(rpctls_vnetinit, SI_SUB_VNET_DONE, SI_ORDER_ANY,
    rpctls_vnetinit, NULL);

static void
rpctls_cleanup(void *unused __unused)
{

	free(KRPC_VNET(rpctls_server_handle), M_RPC);
	free(KRPC_VNET(rpctls_server_busy), M_RPC);
}
VNET_SYSUNINIT(rpctls_cleanup, SI_SUB_VNET_DONE, SI_ORDER_ANY,
    rpctls_cleanup, NULL);

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
	int fd = -1, error, i, try_count;
	CLIENT *cl, *oldcl[RPCTLS_SRV_MAXNPROCS], *concl;
	uint64_t ssl[3];
	struct timeval timeo;
#ifdef KERN_TLS
	u_int maxlen;
#endif
        
	error = priv_check(td, PRIV_NFS_DAEMON);
	if (error != 0)
		return (error);

	KRPC_CURVNET_SET(KRPC_TD_TO_VNET(td));
	switch (uap->op) {
	case RPCTLS_SYSC_SRVSTARTUP:
		if (jailed(curthread->td_ucred) &&
		    !prison_check_nfsd(curthread->td_ucred))
			error = EPERM;
		if (error == 0) {
			/* Get rid of all old CLIENTs. */
			mtx_lock(&rpctls_server_lock);
			for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
				oldcl[i] = KRPC_VNET(rpctls_server_handle)[i];
				KRPC_VNET(rpctls_server_handle)[i] = NULL;
				KRPC_VNET(rpctls_server_busy)[i] = false;
			}
			KRPC_VNET(rpctls_srv_newdaemon) = true;
			KRPC_VNET(rpctls_srv_prevproc) = 0;
			mtx_unlock(&rpctls_server_lock);
			for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
				if (oldcl[i] != NULL) {
					CLNT_CLOSE(oldcl[i]);
					CLNT_RELEASE(oldcl[i]);
				}
			}
		}
		break;
	case RPCTLS_SYSC_CLSETPATH:
		if (jailed(curthread->td_ucred))
			error = EPERM;
		if (error == 0)
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
		oldcl[0] = rpctls_connect_handle;
		rpctls_connect_handle = cl;
		mtx_unlock(&rpctls_connect_lock);
	
		if (oldcl[0] != NULL) {
			CLNT_CLOSE(oldcl[0]);
			CLNT_RELEASE(oldcl[0]);
		}
		break;
	case RPCTLS_SYSC_SRVSETPATH:
		if (jailed(curthread->td_ucred) &&
		    !prison_check_nfsd(curthread->td_ucred))
			error = EPERM;
		if (error == 0)
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
	
		for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++)
			oldcl[i] = NULL;
		mtx_lock(&rpctls_server_lock);
		if (KRPC_VNET(rpctls_srv_newdaemon)) {
			/*
			 * For a new daemon, the rpctls_srv_handles have
			 * already been cleaned up by RPCTLS_SYSC_SRVSTARTUP.
			 * Scan for an available array entry to use.
			 */
			for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
				if (KRPC_VNET(rpctls_server_handle)[i] == NULL)
					break;
			}
			if (i == RPCTLS_SRV_MAXNPROCS && error == 0)
				error = ENXIO;
		} else {
			/* For an old daemon, clear out old CLIENTs. */
			for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
				oldcl[i] = KRPC_VNET(rpctls_server_handle)[i];
				KRPC_VNET(rpctls_server_handle)[i] = NULL;
				KRPC_VNET(rpctls_server_busy)[i] = false;
			}
			i = 0;	/* Set to use rpctls_server_handle[0]. */
		}
		if (error == 0)
			KRPC_VNET(rpctls_server_handle)[i] = cl;
		mtx_unlock(&rpctls_server_lock);

		for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
			if (oldcl[i] != NULL) {
				CLNT_CLOSE(oldcl[i]);
				CLNT_RELEASE(oldcl[i]);
			}
		}
		break;
	case RPCTLS_SYSC_CLSHUTDOWN:
		mtx_lock(&rpctls_connect_lock);
		oldcl[0] = rpctls_connect_handle;
		rpctls_connect_handle = NULL;
		mtx_unlock(&rpctls_connect_lock);
	
		if (oldcl[0] != NULL) {
			CLNT_CLOSE(oldcl[0]);
			CLNT_RELEASE(oldcl[0]);
		}
		break;
	case RPCTLS_SYSC_SRVSHUTDOWN:
		mtx_lock(&rpctls_server_lock);
		for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
			oldcl[i] = KRPC_VNET(rpctls_server_handle)[i];
			KRPC_VNET(rpctls_server_handle)[i] = NULL;
		}
		KRPC_VNET(rpctls_srv_newdaemon) = false;
		mtx_unlock(&rpctls_server_lock);
	
		for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
			if (oldcl[i] != NULL) {
				CLNT_CLOSE(oldcl[i]);
				CLNT_RELEASE(oldcl[i]);
			}
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
		so = KRPC_VNET(rpctls_server_so);
		KRPC_VNET(rpctls_server_so) = NULL;
		xprt = KRPC_VNET(rpctls_server_xprt);
		KRPC_VNET(rpctls_server_xprt) = NULL;
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
	KRPC_CURVNET_RESTORE();

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
rpctls_server_client(int procpos)
{
	CLIENT *cl;

	KRPC_CURVNET_SET_QUIET(KRPC_TD_TO_VNET(curthread));
	mtx_lock(&rpctls_server_lock);
	cl = KRPC_VNET(rpctls_server_handle)[procpos];
	if (cl != NULL)
		CLNT_ACQUIRE(cl);
	mtx_unlock(&rpctls_server_lock);
	KRPC_CURVNET_RESTORE();
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
rpctls_srv_handlerecord(uint64_t sec, uint64_t usec, uint64_t ssl, int procpos,
    uint32_t *reterr)
{
	struct rpctlssd_handlerecord_arg arg;
	struct rpctlssd_handlerecord_res res;
	enum clnt_stat stat;
	CLIENT *cl;

	cl = rpctls_server_client(procpos);
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
rpctls_srv_disconnect(uint64_t sec, uint64_t usec, uint64_t ssl, int procpos,
    uint32_t *reterr)
{
	struct rpctlssd_disconnect_arg arg;
	struct rpctlssd_disconnect_res res;
	enum clnt_stat stat;
	CLIENT *cl;

	cl = rpctls_server_client(procpos);
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
    uid_t *uid, int *ngrps, gid_t **gids, int *procposp)
{
	enum clnt_stat stat;
	CLIENT *cl;
	struct rpctlssd_connect_res res;
	gid_t *gidp;
	uint32_t *gidv;
	int i, procpos;

	KRPC_CURVNET_SET_QUIET(KRPC_TD_TO_VNET(curthread));
	cl = NULL;
	procpos = -1;
	mtx_lock(&rpctls_server_lock);
	for (i = (KRPC_VNET(rpctls_srv_prevproc) + 1) % RPCTLS_SRV_MAXNPROCS;
	    i != KRPC_VNET(rpctls_srv_prevproc);
	    i = (i + 1) % RPCTLS_SRV_MAXNPROCS) {
		if (KRPC_VNET(rpctls_server_handle)[i] != NULL)
			break;
	}
	if (i == KRPC_VNET(rpctls_srv_prevproc)) {
		if (KRPC_VNET(rpctls_server_handle)[i] != NULL)
			procpos = i;
	} else
		KRPC_VNET(rpctls_srv_prevproc) = procpos = i;
	mtx_unlock(&rpctls_server_lock);
	if (procpos >= 0)
		cl = rpctls_server_client(procpos);
	if (cl == NULL) {
		KRPC_CURVNET_RESTORE();
		return (RPC_SYSTEMERROR);
	}

	/* Serialize the server upcalls. */
	mtx_lock(&rpctls_server_lock);
	while (KRPC_VNET(rpctls_server_busy)[procpos])
		msleep(&KRPC_VNET(rpctls_server_busy)[procpos],
		    &rpctls_server_lock, PVFS, "rtlssn", 0);
	KRPC_VNET(rpctls_server_busy)[procpos] = true;
	KRPC_VNET(rpctls_server_so) = so;
	KRPC_VNET(rpctls_server_xprt) = xprt;
	mtx_unlock(&rpctls_server_lock);

	/* Do the server upcall. */
	res.gid.gid_val = NULL;
	stat = rpctlssd_connect_1(NULL, &res, cl);
	if (stat == RPC_SUCCESS) {
		*flags = res.flags;
		*sslp++ = res.sec;
		*sslp++ = res.usec;
		*sslp = res.ssl;
		*procposp = procpos;
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
	KRPC_VNET(rpctls_server_so) = NULL;
	KRPC_VNET(rpctls_server_xprt) = NULL;
	KRPC_VNET(rpctls_server_busy)[procpos] = false;
	wakeup(&KRPC_VNET(rpctls_server_busy)[procpos]);
	mtx_unlock(&rpctls_server_lock);
	KRPC_CURVNET_RESTORE();

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
	int ngrps, procpos;
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
	stat = rpctls_server(xprt, xprt->xp_socket, &flags,
	    ssl, &uid, &ngrps, &gidp, &procpos);

	/* Re-enable reception on the socket within the krpc. */
	sx_xlock(&xprt->xp_lock);
	xprt->xp_dontrcv = FALSE;
	if (stat == RPC_SUCCESS) {
		xprt->xp_tls = flags;
		xprt->xp_sslsec = ssl[0];
		xprt->xp_sslusec = ssl[1];
		xprt->xp_sslrefno = ssl[2];
		xprt->xp_sslproc = procpos;
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
	if (rpctlscd_run && rpctls_connect_handle == NULL)
		return (false);
	KRPC_CURVNET_SET_QUIET(KRPC_TD_TO_VNET(curthread));
	if (rpctlssd_run && KRPC_VNET(rpctls_server_handle)[0] == NULL) {
		KRPC_CURVNET_RESTORE();
		return (false);
	}
	KRPC_CURVNET_RESTORE();
	*maxlenp = maxlen;
	return (enable);
}

