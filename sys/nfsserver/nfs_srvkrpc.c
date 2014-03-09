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
 *	@(#)nfs_syscalls.c	8.5 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet6.h"
#include "opt_kgssapi.h"

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/eventhandler.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef INET6
#include <net/if.h>
#include <net/if_var.h>			/* XXX: for in6_var.h */
#include <netinet6/in6_var.h>		/* XXX: for ip6_sprintf */
#endif

#include <rpc/rpc.h>
#include <rpc/rpcsec_gss.h>
#include <rpc/replay.h>

#include <nfs/xdr_subs.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs_fha.h>
#include <nfsserver/nfs.h>
#include <nfsserver/nfsm_subs.h>
#include <nfsserver/nfsrvcache.h>
#include <nfsserver/nfs_fha_old.h>

#include <security/mac/mac_framework.h>

static MALLOC_DEFINE(M_NFSSVC, "nfss_srvsock", "Nfs server structure");

MALLOC_DEFINE(M_NFSRVDESC, "nfss_srvdesc", "NFS server socket descriptor");
MALLOC_DEFINE(M_NFSD, "nfss_daemon", "Nfs server daemon structure");

#define	TRUE	1
#define	FALSE	0

SYSCTL_DECL(_vfs_nfsrv);

SVCPOOL		*nfsrv_pool;
int		nfsd_waiting = 0;
int		nfsrv_numnfsd = 0;
struct callout	nfsrv_callout;
static eventhandler_tag nfsrv_nmbclusters_tag;

static int	nfs_privport = 0;
SYSCTL_INT(_vfs_nfsrv, NFS_NFSPRIVPORT, nfs_privport, CTLFLAG_RW,
    &nfs_privport, 0,
    "Only allow clients using a privileged port");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, gatherdelay, CTLFLAG_RW,
    &nfsrvw_procrastinate, 0,
    "Delay value for write gathering");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, gatherdelay_v3, CTLFLAG_RW,
    &nfsrvw_procrastinate_v3, 0,
    "Delay in seconds for NFSv3 write gathering");

static int	nfssvc_addsock(struct file *, struct thread *);
static int	nfssvc_nfsd(struct thread *, struct nfsd_nfsd_args *);

extern u_long sb_max_adj;

int32_t (*nfsrv3_procs[NFS_NPROCS])(struct nfsrv_descript *nd,
    struct nfssvc_sock *slp, struct mbuf **mreqp) = {
	nfsrv_null,
	nfsrv_getattr,
	nfsrv_setattr,
	nfsrv_lookup,
	nfsrv3_access,
	nfsrv_readlink,
	nfsrv_read,
	nfsrv_write,
	nfsrv_create,
	nfsrv_mkdir,
	nfsrv_symlink,
	nfsrv_mknod,
	nfsrv_remove,
	nfsrv_rmdir,
	nfsrv_rename,
	nfsrv_link,
	nfsrv_readdir,
	nfsrv_readdirplus,
	nfsrv_statfs,
	nfsrv_fsinfo,
	nfsrv_pathconf,
	nfsrv_commit,
	nfsrv_noop
};

/*
 * NFS server system calls
 */
/*
 * This is now called from nfssvc() in nfs/nfs_nfssvc.c.
 */

/*
 * Nfs server psuedo system call for the nfsd's
 * Based on the flag value it either:
 * - adds a socket to the selection list
 * - remains in the kernel as an nfsd
 * - remains in the kernel as an nfsiod
 * For INET6 we suppose that nfsd provides only IN6P_IPV6_V6ONLY sockets
 * and that mountd provides
 *  - sockaddr with no IPv4-mapped addresses
 *  - mask for both INET and INET6 families if there is IPv4-mapped overlap
 */
int
nfssvc_nfsserver(struct thread *td, struct nfssvc_args *uap)
{
	struct file *fp;
	struct nfsd_addsock_args addsockarg;
	struct nfsd_nfsd_args nfsdarg;
	cap_rights_t rights;
	int error;

	if (uap->flag & NFSSVC_ADDSOCK) {
		error = copyin(uap->argp, (caddr_t)&addsockarg,
		    sizeof(addsockarg));
		if (error)
			return (error);
		error = fget(td, addsockarg.sock,
		    cap_rights_init(&rights, CAP_SOCK_SERVER), &fp);
		if (error)
			return (error);
		if (fp->f_type != DTYPE_SOCKET) {
			fdrop(fp, td);
			return (error);	/* XXXRW: Should be EINVAL? */
		}
		error = nfssvc_addsock(fp, td);
		fdrop(fp, td);
	} else if (uap->flag & NFSSVC_OLDNFSD)
		error = nfssvc_nfsd(td, NULL);
	else if (uap->flag & NFSSVC_NFSD) {
		if (!uap->argp)
			return (EINVAL);
		error = copyin(uap->argp, (caddr_t)&nfsdarg,
		    sizeof(nfsdarg));
		if (error)
			return (error);
		error = nfssvc_nfsd(td, &nfsdarg);
	} else
		error = ENXIO;
	return (error);
}

/*
 * Generate the rpc reply header
 * siz arg. is used to decide if adding a cluster is worthwhile
 */
struct mbuf *
nfs_rephead(int siz, struct nfsrv_descript *nd, int err,
    struct mbuf **mbp, caddr_t *bposp)
{
	u_int32_t *tl;
	struct mbuf *mreq;
	caddr_t bpos;
	struct mbuf *mb;

	if (err == EBADRPC)
		return (NULL);

	nd->nd_repstat = err;
	if (err && (nd->nd_flag & ND_NFSV3) == 0)	/* XXX recheck */
		siz = 0;

	MGET(mreq, M_WAITOK, MT_DATA);

	/*
	 * If this is a big reply, use a cluster
	 */
	mreq->m_len = 0;
	if (siz >= MINCLSIZE) {
		MCLGET(mreq, M_WAITOK);
	}
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	if (err != NFSERR_RETVOID) {
		tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
		if (err)
			*tl = txdr_unsigned(nfsrv_errmap(nd, err));
		else
			*tl = 0;
	}

	*mbp = mb;
	*bposp = bpos;
	if (err != 0 && err != NFSERR_RETVOID)
		nfsrvstats.srvrpc_errs++;

	return (mreq);
}

static void
nfssvc_program(struct svc_req *rqst, SVCXPRT *xprt)
{
	rpcproc_t procnum;
	int32_t (*proc)(struct nfsrv_descript *nd, struct nfssvc_sock *slp,
	    struct mbuf **mreqp);
	int flag;
	struct nfsrv_descript nd;
	struct mbuf *mreq, *mrep;
	int error;

	if (rqst->rq_vers == NFS_VER2) {
		if (rqst->rq_proc > NFSV2PROC_STATFS) {
			svcerr_noproc(rqst);
			svc_freereq(rqst);
			return;
		}
		procnum = nfsrv_nfsv3_procid[rqst->rq_proc];
		flag = 0;
	} else {
		if (rqst->rq_proc >= NFS_NPROCS) {
			svcerr_noproc(rqst);
			svc_freereq(rqst);
			return;
		}
		procnum = rqst->rq_proc;
		flag = ND_NFSV3;
	}
	proc = nfsrv3_procs[procnum];

	mreq = mrep = NULL;
	mreq = rqst->rq_args;
	rqst->rq_args = NULL;
	(void)nfs_realign(&mreq, M_WAITOK);

	/*
	 * Note: we want rq_addr, not svc_getrpccaller for nd_nam2 -
	 * NFS_SRVMAXDATA uses a NULL value for nd_nam2 to detect TCP
	 * mounts.
	 */
	memset(&nd, 0, sizeof(nd));
	nd.nd_md = nd.nd_mrep = mreq;
	nd.nd_dpos = mtod(mreq, caddr_t);
	nd.nd_nam = svc_getrpccaller(rqst);
	nd.nd_nam2 = rqst->rq_addr;
	nd.nd_procnum = procnum;
	nd.nd_cr = NULL;
	nd.nd_flag = flag;

	if (nfs_privport) {
		/* Check if source port is privileged */
		u_short port;
		struct sockaddr *nam = nd.nd_nam;
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)nam;
		/*
		 * INET/INET6 - same code:
		 *    sin_port and sin6_port are at same offset
		 */
		port = ntohs(sin->sin_port);
		if (port >= IPPORT_RESERVED &&
		    nd.nd_procnum != NFSPROC_NULL) {
#ifdef INET6
			char b6[INET6_ADDRSTRLEN];
#if defined(KLD_MODULE)
			/* Do not use ip6_sprintf: the nfs module should work without INET6. */
#define ip6_sprintf(buf, a)						\
			(sprintf((buf), "%x:%x:%x:%x:%x:%x:%x:%x",	\
			    (a)->s6_addr16[0], (a)->s6_addr16[1],	\
			    (a)->s6_addr16[2], (a)->s6_addr16[3],	\
			    (a)->s6_addr16[4], (a)->s6_addr16[5],	\
			    (a)->s6_addr16[6], (a)->s6_addr16[7]),	\
			    (buf))
#endif
#endif
			printf("NFS request from unprivileged port (%s:%d)\n",
#ifdef INET6
			    sin->sin_family == AF_INET6 ?
			    ip6_sprintf(b6, &satosin6(sin)->sin6_addr) :
#if defined(KLD_MODULE)
#undef ip6_sprintf
#endif
#endif
			    inet_ntoa(sin->sin_addr), port);
			m_freem(mreq);
			svcerr_weakauth(rqst);
			svc_freereq(rqst);
			return;
		}
	}

	if (proc != nfsrv_null) {
		if (!svc_getcred(rqst, &nd.nd_cr, &nd.nd_credflavor)) {
			m_freem(mreq);
			svcerr_weakauth(rqst);
			svc_freereq(rqst);
			return;
		}
#ifdef MAC
		mac_cred_associate_nfsd(nd.nd_cr);
#endif
	}
	nfsrvstats.srvrpccnt[nd.nd_procnum]++;

	error = proc(&nd, NULL, &mrep);

	if (nd.nd_cr)
		crfree(nd.nd_cr);

	if (mrep == NULL) {
		svcerr_decode(rqst);
		svc_freereq(rqst);
		return;
	}
	if (error && error != NFSERR_RETVOID) {
		svcerr_systemerr(rqst);
		svc_freereq(rqst);
		return;
	}
	if (nd.nd_repstat & NFSERR_AUTHERR) {
		svcerr_auth(rqst, nd.nd_repstat & ~NFSERR_AUTHERR);
		m_freem(mrep);
	} else {
		if (!svc_sendreply_mbuf(rqst, mrep))
			svcerr_systemerr(rqst);
	}
	svc_freereq(rqst);
}

/*
 * Adds a socket to the list for servicing by nfsds.
 */
static int
nfssvc_addsock(struct file *fp, struct thread *td)
{
	int siz;
	struct socket *so;
	int error;
	SVCXPRT *xprt;

	so = fp->f_data;

	siz = sb_max_adj;
	error = soreserve(so, siz, siz);
	if (error)
		return (error);

	/*
	 * Steal the socket from userland so that it doesn't close
	 * unexpectedly.
	 */
	if (so->so_type == SOCK_DGRAM)
		xprt = svc_dg_create(nfsrv_pool, so, 0, 0);
	else
		xprt = svc_vc_create(nfsrv_pool, so, 0, 0);
	if (xprt) {
		fp->f_ops = &badfileops;
		fp->f_data = NULL;
		svc_reg(xprt, NFS_PROG, NFS_VER2, nfssvc_program, NULL);
		svc_reg(xprt, NFS_PROG, NFS_VER3, nfssvc_program, NULL);
		SVC_RELEASE(xprt);
	}

	return (0);
}

/*
 * Called by nfssvc() for nfsds.  Just loops around servicing rpc requests
 * until it is killed by a signal.
 */
static int
nfssvc_nfsd(struct thread *td, struct nfsd_nfsd_args *args)
{
	char principal[128];
	int error;

	if (args) {
		error = copyinstr(args->principal, principal,
		    sizeof(principal), NULL);
		if (error)
			return (error);
	} else {
		memcpy(principal, "nfs@", 4);
		getcredhostname(td->td_ucred, principal + 4,
		    sizeof(principal) - 4);
	}

	/*
	 * Only the first nfsd actually does any work.  The RPC code
	 * adds threads to it as needed.  Any extra processes offered
	 * by nfsd just exit.  If nfsd is new enough, it will call us
	 * once with a structure that specifies how many threads to
	 * use.
	 */
	NFSD_LOCK();
	if (nfsrv_numnfsd == 0) {
		nfsrv_numnfsd++;

		NFSD_UNLOCK();

		rpc_gss_set_svc_name_call(principal, "kerberosv5",
		    GSS_C_INDEFINITE, NFS_PROG, NFS_VER2);
		rpc_gss_set_svc_name_call(principal, "kerberosv5",
		    GSS_C_INDEFINITE, NFS_PROG, NFS_VER3);

		if (args) {
			nfsrv_pool->sp_minthreads = args->minthreads;
			nfsrv_pool->sp_maxthreads = args->maxthreads;
		} else {
			nfsrv_pool->sp_minthreads = 4;
			nfsrv_pool->sp_maxthreads = 4;
		}

		svc_run(nfsrv_pool);

		rpc_gss_clear_svc_name_call(NFS_PROG, NFS_VER2);
		rpc_gss_clear_svc_name_call(NFS_PROG, NFS_VER3);

		NFSD_LOCK();
		nfsrv_numnfsd--;
		nfsrv_init(TRUE);
	}
	NFSD_UNLOCK();

	return (0);
}

/*
 * Size the NFS server's duplicate request cache at 1/2 the
 * nmbclusters, floating within a (64, 2048) range.  This is to
 * prevent all mbuf clusters being tied up in the NFS dupreq
 * cache for small values of nmbclusters.
 */
static size_t
nfsrv_replay_size(void)
{
	size_t replaysiz;

	replaysiz = nmbclusters / 2;
	if (replaysiz > NFSRVCACHE_MAX_SIZE)
		replaysiz = NFSRVCACHE_MAX_SIZE;
	if (replaysiz < NFSRVCACHE_MIN_SIZE)
		replaysiz = NFSRVCACHE_MIN_SIZE;
	replaysiz *= MCLBYTES;

	return (replaysiz);
}

/*
 * Called when nmbclusters changes - we resize the replay cache
 * accordingly.
 */
static void
nfsrv_nmbclusters_change(void *tag)
{

	if (nfsrv_pool)
		replay_setsize(nfsrv_pool->sp_rcache, nfsrv_replay_size());
}

/*
 * Initialize the data structures for the server.
 * Handshake with any new nfsds starting up to avoid any chance of
 * corruption.
 */
void
nfsrv_init(int terminating)
{

	NFSD_LOCK_ASSERT();

	if (terminating) {
		NFSD_UNLOCK();
		EVENTHANDLER_DEREGISTER(nmbclusters_change,
		    nfsrv_nmbclusters_tag);
		svcpool_destroy(nfsrv_pool);
		nfsrv_pool = NULL;
		NFSD_LOCK();
	} else
		nfs_pub.np_valid = 0;

	NFSD_UNLOCK();

	nfsrv_pool = svcpool_create("nfsd", SYSCTL_STATIC_CHILDREN(_vfs_nfsrv));
	nfsrv_pool->sp_rcache = replay_newcache(nfsrv_replay_size());
	nfsrv_pool->sp_assign = fhaold_assign;
	nfsrv_pool->sp_done = fha_nd_complete;
	nfsrv_nmbclusters_tag = EVENTHANDLER_REGISTER(nmbclusters_change,
	    nfsrv_nmbclusters_change, NULL, EVENTHANDLER_PRI_FIRST);

	NFSD_LOCK();
}
