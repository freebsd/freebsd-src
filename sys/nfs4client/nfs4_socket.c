/* $FreeBSD$ */
/* $Id: nfs_socket.c,v 1.12 2003/11/05 14:59:01 rees Exp $ */

/*
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/vnode.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfs4client/nfs4.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsmount.h>

#ifdef NFS4_USE_RPCCLNT
#include <rpc/rpcclnt.h>
#include <rpc/rpcm_subs.h>
#endif

#ifdef NFS4_USE_RPCCLNT
static struct rpc_program nfs_program = {
  NFS_PROG, NFS_VER4, "NFSv4"
};
#endif


int
nfs4_connect(struct nfsmount *nmp)
{
	struct rpcclnt * rpc = &nmp->nm_rpcclnt;
	struct rpc_auth * auth;
	int flag = 0;
	int error;

	/* XXX hack! */
#ifdef __OpenBSD__
	struct proc * td = curproc;
#else
	struct thread * td = curthread;
#endif

	MALLOC(auth, struct rpc_auth *, sizeof(struct rpc_auth), M_TEMP, M_WAITOK);
	auth->auth_type  = RPCAUTH_UNIX;

	/* translate nfs flags -> rpcclnt flags */
	if (nmp->nm_flag & NFSMNT_SOFT)
		flag |= RPCCLNT_SOFT;

	if (nmp->nm_flag & NFSMNT_INT)
		flag |= RPCCLNT_INT;

	if (nmp->nm_flag & NFSMNT_NOCONN)
		flag |= RPCCLNT_NOCONN;

	if (nmp->nm_flag & NFSMNT_DUMBTIMR)
		flag |= RPCCLNT_DUMBTIMR;

	/* rpc->rc_servername = nmp->nm_mountp->mnt_stat.f_mntfromname; */
				  
	error = rpcclnt_setup(rpc, &nfs_program, nmp->nm_nam, nmp->nm_sotype,
			      nmp->nm_soproto, auth, 
			      /* XXX: check nmp->nm_flag to make sure these are set */
			      (nmp->nm_rsize > nmp->nm_readdirsize) ? nmp->nm_rsize : nmp->nm_readdirsize,
			      nmp->nm_wsize, flag);

	/* set deadthresh, timeo, retry */
	rpc->rc_deadthresh = nmp->nm_deadthresh;
	rpc->rc_timeo = nmp->nm_timeo;
	rpc->rc_retry = nmp->nm_retry;


	if (error)
		return error;

	return rpcclnt_connect(rpc, td);
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
nfs4_disconnect(struct nfsmount *nmp)
{
	rpcclnt_disconnect(&nmp->nm_rpcclnt);
}

void
nfs4_safedisconnect(struct nfsmount *nmp)
{
	rpcclnt_safedisconnect(&nmp->nm_rpcclnt);
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
/* XXX overloaded before */
#define	NQ_TRYLATERDEL	15	/* Initial try later delay (sec) */

int
nfs4_request(struct vnode *vp, struct mbuf *mrest, int procnum,
    struct thread *td, struct ucred *cred, struct mbuf **mrp,
    struct mbuf **mdp, caddr_t *dposp)
{
	int error;
	u_int32_t *tl;
	struct nfsmount * nmp = VFSTONFS(vp->v_mount);
	struct rpcclnt * clnt = &nmp->nm_rpcclnt;
	struct mbuf *md, *mrep;
	caddr_t dpos;
	struct rpc_reply reply;

	if ((error = rpcclnt_request(clnt, mrest, procnum, td, cred,
	    &reply)) != 0) {
		goto out;
	}

	/* XXX: don't free mrest if an error occured, to allow caller to retry*/
	m_freem(mrest);
	mrep = reply.mrep;
	md = reply.result_md;
	dpos = reply.result_dpos;

	tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
	if (*tl != 0) {
		error = fxdr_unsigned(int, *tl);

		#if 0
		if ((nmp->nm_flag & NFSMNT_NFSV3) &&
		    error == NFSERR_TRYLATER) {
			m_freem(mrep);
			error = 0;
			waituntil = time_second + trylater_delay;
			while (time_second < waituntil)
				(void) tsleep(&lbolt, PSOCK, "nqnfstry", 0);
			trylater_delay *= nfs_backoff[trylater_cnt];
			if (trylater_cnt < NFS_NBACKOFF - 1)
				trylater_cnt++;
			goto tryagain;
		}
		#endif

		/*
 		** If the File Handle was stale, invalidate the
 		** lookup cache, just in case.
 		**/
		if (error == ESTALE)
			cache_purge(vp);
		goto out;
	}

	*mrp = mrep;
	*mdp = md;
	*dposp = dpos;
	return (0);
nfsmout:
out:
	m_freem(reply.mrep);
	*mrp = NULL;
	*mdp = NULL;
	return (error);
}


int
nfs4_request_mnt(struct nfsmount *nmp, struct mbuf *mrest, int procnum,
    struct thread *td, struct ucred *cred, struct mbuf **mrp,
    struct mbuf **mdp, caddr_t *dposp)
{
	int error;
	u_int32_t *tl;
	struct rpcclnt * clnt = &nmp->nm_rpcclnt;
	struct mbuf *md, *mrep;
	caddr_t dpos;
	struct rpc_reply reply;

	if ((error = rpcclnt_request(clnt, mrest, procnum, td, cred,
	    &reply)) != 0) {
		goto out;
	}

	/* XXX: don't free mrest if an error occured, to allow caller to retry*/
	m_freem(mrest);
	mrep = reply.mrep;
	md = reply.result_md;
	dpos = reply.result_dpos;

	tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
	if (*tl != 0) {
		error = fxdr_unsigned(int, *tl);
#if 0
		if ((nmp->nm_flag & NFSMNT_NFSV3) &&
		    error == NFSERR_TRYLATER) {
			m_freem(mrep);
			error = 0;
			waituntil = time_second + trylater_delay;
			while (time_second < waituntil)
				(void) tsleep(&lbolt, PSOCK, "nqnfstry", 0);
			trylater_delay *= nfs_backoff[trylater_cnt];
			if (trylater_cnt < NFS_NBACKOFF - 1)
				trylater_cnt++;
			goto tryagain;
		}
#endif
		goto out;
	}

	*mrp = mrep;
	*mdp = md;
	*dposp = dpos;
	return (0);
nfsmout:
out:
	m_freem(reply.mrep);
	*mrp = NULL;
	*mdp = NULL;
	return (error);
}


/*
 * Mark all of an nfs mount's outstanding requests with R_SOFTTERM and
 * wait for all requests to complete. This is used by forced unmounts
 * to terminate any outstanding RPCs.
 */
int
nfs4_nmcancelreqs(nmp)
	struct nfsmount *nmp;
{
	return rpcclnt_cancelreqs(&nmp->nm_rpcclnt);
}

/*
 * Test for a termination condition pending on the process.
 * This is used for NFSMNT_INT mounts.
 */
int
nfs4_sigintr(struct nfsmount *nmp, struct nfsreq *rep, struct thread *td)
{
	if (rep != NULL) {
		printf("nfs_sigintr: attempting to use nfsreq != NULL\n");
		return EINTR;
	}
	return rpcclnt_sigintr(&nmp->nm_rpcclnt, NULL, td);
}
