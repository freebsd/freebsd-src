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
#include <sys/refcount.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vnode.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfsserver/nfsm_subs.h>

#define	TRUE	1
#define	FALSE	0

static int nfs_realign_test;
static int nfs_realign_count;

SYSCTL_DECL(_vfs_nfsrv);

SYSCTL_INT(_vfs_nfsrv, OID_AUTO, realign_test, CTLFLAG_RW, &nfs_realign_test, 0, "");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, realign_count, CTLFLAG_RW, &nfs_realign_count, 0, "");


/*
 * There is a congestion window for outstanding rpcs maintained per mount
 * point. The cwnd size is adjusted in roughly the way that:
 * Van Jacobson, Congestion avoidance and Control, In "Proceedings of
 * SIGCOMM '88". ACM, August 1988.
 * describes for TCP. The cwnd size is chopped in half on a retransmit timeout
 * and incremented by 1/cwnd when each rpc reply is received and a full cwnd
 * of rpcs is in progress.
 * (The sent count and cwnd are scaled for integer arith.)
 * Variants of "slow start" were tried and were found to be too much of a
 * performance hit (ave. rtt 3 times larger),
 * I suspect due to the large rtt that nfs rpcs have.
 */
#define	NFS_CWNDSCALE	256
#define	NFS_MAXCWND	(NFS_CWNDSCALE * 32)
struct callout	nfsrv_callout;

static void	nfs_realign(struct mbuf **pm, int hsiz);	/* XXX SHARED */
static int	nfsrv_getstream(struct nfssvc_sock *, int);

int32_t (*nfsrv3_procs[NFS_NPROCS])(struct nfsrv_descript *nd,
				struct nfssvc_sock *slp,
				struct thread *td,
				struct mbuf **mreqp) = {
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

	/* XXXRW: not 100% clear the lock is needed here. */
	NFSD_LOCK_ASSERT();

	nd->nd_repstat = err;
	if (err && (nd->nd_flag & ND_NFSV3) == 0)	/* XXX recheck */
		siz = 0;
	NFSD_UNLOCK();
	MGETHDR(mreq, M_TRYWAIT, MT_DATA);
	mb = mreq;
	/*
	 * If this is a big reply, use a cluster else
	 * try and leave leading space for the lower level headers.
	 */
	mreq->m_len = 6 * NFSX_UNSIGNED;
	siz += RPC_REPLYSIZ;
	if ((max_hdr + siz) >= MINCLSIZE) {
		MCLGET(mreq, M_TRYWAIT);
	} else
		mreq->m_data += min(max_hdr, M_TRAILINGSPACE(mreq));
	NFSD_LOCK();
	tl = mtod(mreq, u_int32_t *);
	bpos = ((caddr_t)tl) + mreq->m_len;
	*tl++ = txdr_unsigned(nd->nd_retxid);
	*tl++ = nfsrv_rpc_reply;
	if (err == ERPCMISMATCH || (err & NFSERR_AUTHERR)) {
		*tl++ = nfsrv_rpc_msgdenied;
		if (err & NFSERR_AUTHERR) {
			*tl++ = nfsrv_rpc_autherr;
			*tl = txdr_unsigned(err & ~NFSERR_AUTHERR);
			mreq->m_len -= NFSX_UNSIGNED;
			bpos -= NFSX_UNSIGNED;
		} else {
			*tl++ = nfsrv_rpc_mismatch;
			*tl++ = txdr_unsigned(RPC_VER2);
			*tl = txdr_unsigned(RPC_VER2);
		}
	} else {
		*tl++ = nfsrv_rpc_msgaccepted;
		/*
		 * Send a RPCAUTH_NULL verifier - no Kerberos.
		 */
		*tl++ = 0;
		*tl++ = 0;
		switch (err) {
		case EPROGUNAVAIL:
			*tl = txdr_unsigned(RPC_PROGUNAVAIL);
			break;
		case EPROGMISMATCH:
			*tl = txdr_unsigned(RPC_PROGMISMATCH);
			tl = nfsm_build(u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(2);
			*tl = txdr_unsigned(3);
			break;
		case EPROCUNAVAIL:
			*tl = txdr_unsigned(RPC_PROCUNAVAIL);
			break;
		case EBADRPC:
			*tl = txdr_unsigned(RPC_GARBAGE);
			break;
		default:
			*tl = 0;
			if (err != NFSERR_RETVOID) {
				tl = nfsm_build(u_int32_t *, NFSX_UNSIGNED);
				if (err)
				    *tl = txdr_unsigned(nfsrv_errmap(nd, err));
				else
				    *tl = 0;
			}
			break;
		}
	}
	*mbp = mb;
	*bposp = bpos;
	if (err != 0 && err != NFSERR_RETVOID)
		nfsrvstats.srvrpc_errs++;
	return mreq;
}


/*
 *	nfs_realign:
 *
 *	Check for badly aligned mbuf data and realign by copying the unaligned
 *	portion of the data into a new mbuf chain and freeing the portions
 *	of the old chain that were replaced.
 *
 *	We cannot simply realign the data within the existing mbuf chain
 *	because the underlying buffers may contain other rpc commands and
 *	we cannot afford to overwrite them.
 *
 *	We would prefer to avoid this situation entirely.  The situation does
 *	not occur with NFS/UDP and is supposed to only occassionally occur
 *	with TCP.  Use vfs.nfs.realign_count and realign_test to check this.
 */
static void
nfs_realign(struct mbuf **pm, int hsiz)	/* XXX COMMON */
{
	struct mbuf *m;
	struct mbuf *n = NULL;
	int off = 0;

	/* XXXRW: may not need lock? */
	NFSD_LOCK_ASSERT();

	++nfs_realign_test;
	while ((m = *pm) != NULL) {
		if ((m->m_len & 0x3) || (mtod(m, intptr_t) & 0x3)) {
			NFSD_UNLOCK();
			MGET(n, M_TRYWAIT, MT_DATA);
			if (m->m_len >= MINCLSIZE) {
				MCLGET(n, M_TRYWAIT);
			}
			NFSD_LOCK();
			n->m_len = 0;
			break;
		}
		pm = &m->m_next;
	}

	/*
	 * If n is non-NULL, loop on m copying data, then replace the
	 * portion of the chain that had to be realigned.
	 */
	if (n != NULL) {
		++nfs_realign_count;
		while (m) {
			m_copyback(n, off, m->m_len, mtod(m, caddr_t));
			off += m->m_len;
			m = m->m_next;
		}
		m_freem(*pm);
		*pm = n;
	}
}


/*
 * Parse an RPC request
 * - verify it
 * - fill in the cred struct.
 */
int
nfs_getreq(struct nfsrv_descript *nd, struct nfsd *nfsd, int has_header)
{
	int len, i;
	u_int32_t *tl;
	caddr_t dpos;
	u_int32_t nfsvers, auth_type;
	int error = 0;
	struct mbuf *mrep, *md;

	NFSD_LOCK_ASSERT();

	mrep = nd->nd_mrep;
	md = nd->nd_md;
	dpos = nd->nd_dpos;
	if (has_header) {
		tl = nfsm_dissect_nonblock(u_int32_t *, 10 * NFSX_UNSIGNED);
		nd->nd_retxid = fxdr_unsigned(u_int32_t, *tl++);
		if (*tl++ != nfsrv_rpc_call) {
			m_freem(mrep);
			return (EBADRPC);
		}
	} else
		tl = nfsm_dissect_nonblock(u_int32_t *, 8 * NFSX_UNSIGNED);
	nd->nd_repstat = 0;
	nd->nd_flag = 0;
	if (*tl++ != nfsrv_rpc_vers) {
		nd->nd_repstat = ERPCMISMATCH;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if (*tl != nfsrv_nfs_prog) {
		nd->nd_repstat = EPROGUNAVAIL;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	tl++;
	nfsvers = fxdr_unsigned(u_int32_t, *tl++);
	if (nfsvers < NFS_VER2 || nfsvers > NFS_VER3) {
		nd->nd_repstat = EPROGMISMATCH;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	nd->nd_procnum = fxdr_unsigned(u_int32_t, *tl++);
	if (nd->nd_procnum == NFSPROC_NULL)
		return (0);
	if (nfsvers == NFS_VER3) {
		nd->nd_flag = ND_NFSV3;
		if (nd->nd_procnum >= NFS_NPROCS) {
			nd->nd_repstat = EPROCUNAVAIL;
			nd->nd_procnum = NFSPROC_NOOP;
			return (0);
		}
	} else {
		if (nd->nd_procnum > NFSV2PROC_STATFS) {
			nd->nd_repstat = EPROCUNAVAIL;
			nd->nd_procnum = NFSPROC_NOOP;
			return (0);
		}
		/* Map the v2 procedure numbers into v3 ones */
		nd->nd_procnum = nfsrv_nfsv3_procid[nd->nd_procnum];
	}
	auth_type = *tl++;
	len = fxdr_unsigned(int, *tl++);
	if (len < 0 || len > RPCAUTH_MAXSIZ) {
		m_freem(mrep);
		return (EBADRPC);
	}

	/*
	 * Handle auth_unix;
	 */
	if (auth_type == nfsrv_rpc_auth_unix) {
		len = fxdr_unsigned(int, *++tl);
		if (len < 0 || len > NFS_MAXNAMLEN) {
			m_freem(mrep);
			return (EBADRPC);
		}
		nfsm_adv(nfsm_rndup(len));
		tl = nfsm_dissect_nonblock(u_int32_t *, 3 * NFSX_UNSIGNED);
		/*
		 * XXX: This credential should be managed using crget(9)
		 * and related calls.  Right now, this tramples on any
		 * extensible data in the ucred, and worse.  This wasn't
		 * fixed before FreeBSD 5.3-RELEASE.
		 */
		bzero((caddr_t)&nd->nd_cr, sizeof (struct ucred));
		refcount_init(&nd->nd_cr.cr_ref, 1);
		nd->nd_cr.cr_uid = fxdr_unsigned(uid_t, *tl++);
		nd->nd_cr.cr_gid = fxdr_unsigned(gid_t, *tl++);
		len = fxdr_unsigned(int, *tl);
		if (len < 0 || len > RPCAUTH_UNIXGIDS) {
			m_freem(mrep);
			return (EBADRPC);
		}
		tl = nfsm_dissect_nonblock(u_int32_t *, (len + 2) * NFSX_UNSIGNED);
		for (i = 1; i <= len; i++)
		    if (i < NGROUPS)
			nd->nd_cr.cr_groups[i] = fxdr_unsigned(gid_t, *tl++);
		    else
			tl++;
		nd->nd_cr.cr_ngroups = (len >= NGROUPS) ? NGROUPS : (len + 1);
		if (nd->nd_cr.cr_ngroups > 1)
		    nfsrvw_sort(nd->nd_cr.cr_groups, nd->nd_cr.cr_ngroups);
		len = fxdr_unsigned(int, *++tl);
		if (len < 0 || len > RPCAUTH_MAXSIZ) {
			m_freem(mrep);
			return (EBADRPC);
		}
		if (len > 0)
			nfsm_adv(nfsm_rndup(len));
	} else {
		nd->nd_repstat = (NFSERR_AUTHERR | AUTH_REJECTCRED);
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}

	nd->nd_md = md;
	nd->nd_dpos = dpos;
	return (0);
nfsmout:
	return (error);
}

/*
 * Socket upcall routine for the nfsd sockets.
 * The caddr_t arg is a pointer to the "struct nfssvc_sock".
 * Essentially do as much as possible non-blocking, else punt and it will
 * be called with M_TRYWAIT from an nfsd.
 */
void
nfsrv_rcv(struct socket *so, void *arg, int waitflag)
{
	struct nfssvc_sock *slp = (struct nfssvc_sock *)arg;
	struct mbuf *m;
	struct mbuf *mp;
	struct sockaddr *nam;
	struct uio auio;
	int flags, error;

	/*
	 * XXXRW: For now, assert Giant here since the NFS server upcall
	 * will perform socket operations requiring Giant in a non-mpsafe
	 * kernel.
	 */
	NET_ASSERT_GIANT();
	NFSD_UNLOCK_ASSERT();

	/* XXXRW: Unlocked read. */
	if ((slp->ns_flag & SLP_VALID) == 0)
		return;

	/*
	 * We can't do this in the context of a socket callback
	 * because we're called with locks held.
	 * XXX: SMP
	 */
	if (waitflag == M_DONTWAIT) {
		NFSD_LOCK();
		slp->ns_flag |= SLP_NEEDQ;
		goto dorecs;
	}


	NFSD_LOCK();
	auio.uio_td = NULL;
	if (so->so_type == SOCK_STREAM) {
		/*
		 * If there are already records on the queue, defer soreceive()
		 * to an nfsd so that there is feedback to the TCP layer that
		 * the nfs servers are heavily loaded.
		 */
		if (STAILQ_FIRST(&slp->ns_rec) != NULL &&
		    waitflag == M_DONTWAIT) {
			slp->ns_flag |= SLP_NEEDQ;
			goto dorecs;
		}

		/*
		 * Do soreceive().
		 */
		auio.uio_resid = 1000000000;
		flags = MSG_DONTWAIT;
		NFSD_UNLOCK();
		error = so->so_proto->pr_usrreqs->pru_soreceive
			(so, &nam, &auio, &mp, NULL, &flags);
		NFSD_LOCK();
		if (error || mp == NULL) {
			if (error == EWOULDBLOCK)
				slp->ns_flag |= SLP_NEEDQ;
			else
				slp->ns_flag |= SLP_DISCONN;
			goto dorecs;
		}
		m = mp;
		if (slp->ns_rawend) {
			slp->ns_rawend->m_next = m;
			slp->ns_cc += 1000000000 - auio.uio_resid;
		} else {
			slp->ns_raw = m;
			slp->ns_cc = 1000000000 - auio.uio_resid;
		}
		while (m->m_next)
			m = m->m_next;
		slp->ns_rawend = m;

		/*
		 * Now try and parse record(s) out of the raw stream data.
		 */
		error = nfsrv_getstream(slp, waitflag);
		if (error) {
			if (error == EPERM)
				slp->ns_flag |= SLP_DISCONN;
			else
				slp->ns_flag |= SLP_NEEDQ;
		}
	} else {
		do {
			auio.uio_resid = 1000000000;
			flags = MSG_DONTWAIT;
			NFSD_UNLOCK();
			error = so->so_proto->pr_usrreqs->pru_soreceive
				(so, &nam, &auio, &mp, NULL, &flags);
			if (mp) {
				struct nfsrv_rec *rec;
				rec = malloc(sizeof(struct nfsrv_rec),
			            M_NFSRVDESC, 
				    waitflag == M_DONTWAIT ? M_NOWAIT : M_WAITOK);
				if (!rec) {
					if (nam)
						FREE(nam, M_SONAME);
					m_freem(mp);
					NFSD_LOCK();
					continue;
				}
				NFSD_LOCK();
				nfs_realign(&mp, 10 * NFSX_UNSIGNED);
				rec->nr_address = nam;
				rec->nr_packet = mp;
				STAILQ_INSERT_TAIL(&slp->ns_rec, rec, nr_link);
			} else
				NFSD_LOCK();
			if (error) {
				if ((so->so_proto->pr_flags & PR_CONNREQUIRED)
					&& error != EWOULDBLOCK) {
					slp->ns_flag |= SLP_DISCONN;
					goto dorecs;
				}
			}
		} while (mp);
	}

	/*
	 * Now try and process the request records, non-blocking.
	 */
dorecs:
	if (waitflag == M_DONTWAIT &&
		(STAILQ_FIRST(&slp->ns_rec) != NULL ||
		 (slp->ns_flag & (SLP_NEEDQ | SLP_DISCONN))))
		nfsrv_wakenfsd(slp);
	NFSD_UNLOCK();
}

/*
 * Try and extract an RPC request from the mbuf data list received on a
 * stream socket. The "waitflag" argument indicates whether or not it
 * can sleep.
 */
static int
nfsrv_getstream(struct nfssvc_sock *slp, int waitflag)
{
	struct mbuf *m, **mpp;
	char *cp1, *cp2;
	int len;
	struct mbuf *om, *m2, *recm;
	u_int32_t recmark;

	NFSD_LOCK_ASSERT();

	if (slp->ns_flag & SLP_GETSTREAM)
		panic("nfs getstream");
	slp->ns_flag |= SLP_GETSTREAM;
	for (;;) {
	    if (slp->ns_reclen == 0) {
		if (slp->ns_cc < NFSX_UNSIGNED) {
			slp->ns_flag &= ~SLP_GETSTREAM;
			return (0);
		}
		m = slp->ns_raw;
		if (m->m_len >= NFSX_UNSIGNED) {
			bcopy(mtod(m, caddr_t), (caddr_t)&recmark, NFSX_UNSIGNED);
			m->m_data += NFSX_UNSIGNED;
			m->m_len -= NFSX_UNSIGNED;
		} else {
			cp1 = (caddr_t)&recmark;
			cp2 = mtod(m, caddr_t);
			while (cp1 < ((caddr_t)&recmark) + NFSX_UNSIGNED) {
				while (m->m_len == 0) {
					m = m->m_next;
					cp2 = mtod(m, caddr_t);
				}
				*cp1++ = *cp2++;
				m->m_data++;
				m->m_len--;
			}
		}
		slp->ns_cc -= NFSX_UNSIGNED;
		recmark = ntohl(recmark);
		slp->ns_reclen = recmark & ~0x80000000;
		if (recmark & 0x80000000)
			slp->ns_flag |= SLP_LASTFRAG;
		else
			slp->ns_flag &= ~SLP_LASTFRAG;
		if (slp->ns_reclen > NFS_MAXPACKET) {
			slp->ns_flag &= ~SLP_GETSTREAM;
			return (EPERM);
		}
	    }

	    /*
	     * Now get the record part.
	     *
	     * Note that slp->ns_reclen may be 0.  Linux sometimes
	     * generates 0-length RPCs.
	     */
	    recm = NULL;
	    if (slp->ns_cc == slp->ns_reclen) {
		recm = slp->ns_raw;
		slp->ns_raw = slp->ns_rawend = NULL;
		slp->ns_cc = slp->ns_reclen = 0;
	    } else if (slp->ns_cc > slp->ns_reclen) {
		len = 0;
		m = slp->ns_raw;
		om = NULL;

		while (len < slp->ns_reclen) {
			if ((len + m->m_len) > slp->ns_reclen) {
				NFSD_UNLOCK();
				m2 = m_copym(m, 0, slp->ns_reclen - len,
					waitflag);
				NFSD_LOCK();
				if (m2) {
					if (om) {
						om->m_next = m2;
						recm = slp->ns_raw;
					} else
						recm = m2;
					m->m_data += slp->ns_reclen - len;
					m->m_len -= slp->ns_reclen - len;
					len = slp->ns_reclen;
				} else {
					slp->ns_flag &= ~SLP_GETSTREAM;
					return (EWOULDBLOCK);
				}
			} else if ((len + m->m_len) == slp->ns_reclen) {
				om = m;
				len += m->m_len;
				m = m->m_next;
				recm = slp->ns_raw;
				om->m_next = NULL;
			} else {
				om = m;
				len += m->m_len;
				m = m->m_next;
			}
		}
		slp->ns_raw = m;
		slp->ns_cc -= len;
		slp->ns_reclen = 0;
	    } else {
		slp->ns_flag &= ~SLP_GETSTREAM;
		return (0);
	    }

	    /*
	     * Accumulate the fragments into a record.
	     */
	    mpp = &slp->ns_frag;
	    while (*mpp)
		mpp = &((*mpp)->m_next);
	    *mpp = recm;
	    if (slp->ns_flag & SLP_LASTFRAG) {
		struct nfsrv_rec *rec;
		NFSD_UNLOCK();
		rec = malloc(sizeof(struct nfsrv_rec), M_NFSRVDESC,
	            waitflag == M_DONTWAIT ? M_NOWAIT : M_WAITOK);
		NFSD_LOCK();
		if (!rec) {
		    m_freem(slp->ns_frag);
		} else {
		    nfs_realign(&slp->ns_frag, 10 * NFSX_UNSIGNED);
		    rec->nr_address = NULL;
		    rec->nr_packet = slp->ns_frag;
		    STAILQ_INSERT_TAIL(&slp->ns_rec, rec, nr_link);
		}
		slp->ns_frag = NULL;
	    }
	}
}

/*
 * Parse an RPC header.
 */
int
nfsrv_dorec(struct nfssvc_sock *slp, struct nfsd *nfsd,
    struct nfsrv_descript **ndp)
{
	struct nfsrv_rec *rec;
	struct mbuf *m;
	struct sockaddr *nam;
	struct nfsrv_descript *nd;
	int error;

	NFSD_LOCK_ASSERT();

	*ndp = NULL;
	if ((slp->ns_flag & SLP_VALID) == 0 ||
	    STAILQ_FIRST(&slp->ns_rec) == NULL)
		return (ENOBUFS);
	rec = STAILQ_FIRST(&slp->ns_rec);
	STAILQ_REMOVE_HEAD(&slp->ns_rec, nr_link);
	nam = rec->nr_address;
	m = rec->nr_packet;
	free(rec, M_NFSRVDESC);
	NFSD_UNLOCK();
	MALLOC(nd, struct nfsrv_descript *, sizeof (struct nfsrv_descript),
		M_NFSRVDESC, M_WAITOK);
	NFSD_LOCK();
	nd->nd_md = nd->nd_mrep = m;
	nd->nd_nam2 = nam;
	nd->nd_dpos = mtod(m, caddr_t);
	error = nfs_getreq(nd, nfsd, TRUE);
	if (error) {
		if (nam) {
			FREE(nam, M_SONAME);
		}
		free((caddr_t)nd, M_NFSRVDESC);
		return (error);
	}
	*ndp = nd;
	nfsd->nfsd_nd = nd;
	return (0);
}

/*
 * Search for a sleeping nfsd and wake it up.
 * SIDE EFFECT: If none found, set NFSD_CHECKSLP flag, so that one of the
 * running nfsds will go look for the work in the nfssvc_sock list.
 */
void
nfsrv_wakenfsd(struct nfssvc_sock *slp)
{
	struct nfsd *nd;

	NFSD_LOCK_ASSERT();

	if ((slp->ns_flag & SLP_VALID) == 0)
		return;
	TAILQ_FOREACH(nd, &nfsd_head, nfsd_chain) {
		if (nd->nfsd_flag & NFSD_WAITING) {
			nd->nfsd_flag &= ~NFSD_WAITING;
			if (nd->nfsd_slp)
				panic("nfsd wakeup");
			slp->ns_sref++;
			nd->nfsd_slp = slp;
			wakeup(nd);
			return;
		}
	}
	slp->ns_flag |= SLP_DOREC;
	nfsd_head_flag |= NFSD_CHECKSLP;
}

/*
 * This is the nfs send routine.
 * For the server side:
 * - return EINTR or ERESTART if interrupted by a signal
 * - return EPIPE if a connection is lost for connection based sockets (TCP...)
 * - do any cleanup required by recoverable socket errors (?)
 */
int
nfsrv_send(struct socket *so, struct sockaddr *nam, struct mbuf *top)
{
	struct sockaddr *sendnam;
	int error, soflags, flags;

	NET_ASSERT_GIANT();
	NFSD_UNLOCK_ASSERT();

	soflags = so->so_proto->pr_flags;
	if ((soflags & PR_CONNREQUIRED) || (so->so_state & SS_ISCONNECTED))
		sendnam = NULL;
	else
		sendnam = nam;
	if (so->so_type == SOCK_SEQPACKET)
		flags = MSG_EOR;
	else
		flags = 0;

	error = so->so_proto->pr_usrreqs->pru_sosend(so, sendnam, 0, top, 0,
						     flags, curthread/*XXX*/);
	if (error == ENOBUFS && so->so_type == SOCK_DGRAM)
		error = 0;

	if (error) {
		log(LOG_INFO, "nfsd send error %d\n", error);

		/*
		 * Handle any recoverable (soft) socket errors here. (?)
		 */
		if (error != EINTR && error != ERESTART &&
		    error != EWOULDBLOCK && error != EPIPE)
			error = 0;
	}
	return (error);
}

/*
 * NFS server timer routine.
 */
void
nfsrv_timer(void *arg)
{
	struct nfssvc_sock *slp;
	u_quad_t cur_usec;

	NFSD_LOCK();
	/*
	 * Scan the write gathering queues for writes that need to be
	 * completed now.
	 */
	cur_usec = nfs_curusec();
	TAILQ_FOREACH(slp, &nfssvc_sockhead, ns_chain) {
		if (LIST_FIRST(&slp->ns_tq) &&
		    LIST_FIRST(&slp->ns_tq)->nd_time <= cur_usec)
			nfsrv_wakenfsd(slp);
	}
	NFSD_UNLOCK();
	callout_reset(&nfsrv_callout, nfsrv_ticks, nfsrv_timer, NULL);
}
