/*
 * Copyright (c) 1992, 1993
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
 *	@(#)nfs_nqlease.c	8.3 (Berkeley) 1/4/94
 */

/*
 * References:
 *	Cary G. Gray and David R. Cheriton, "Leases: An Efficient Fault-Tolerant
 *		Mechanism for Distributed File Cache Consistency",
 *		In Proc. of the Twelfth ACM Symposium on Operating Systems
 *		Principals, pg. 202-210, Litchfield Park, AZ, Dec. 1989.
 *	Michael N. Nelson, Brent B. Welch and John K. Ousterhout, "Caching
 *		in the Sprite Network File System", ACM TOCS 6(1),
 *		pages 134-154, February 1988.
 *	V. Srinivasan and Jeffrey C. Mogul, "Spritely NFS: Implementation and
 *		Performance of Cache-Consistency Protocols", Digital
 *		Equipment Corporation WRL Research Report 89/5, May 1989.
 */
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/protosw.h>

#include <netinet/in.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nqnfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>

/*
 * List head for the lease queue and other global data.
 * At any time a lease is linked into a list ordered by increasing expiry time.
 */
#define	NQFHHASH(f)	((*((u_long *)(f)))&nqfheadhash)

union nqsrvthead nqthead;
struct nqlease **nqfhead;
u_long nqfheadhash;
time_t nqnfsstarttime = (time_t)0;
u_long nqnfs_prog, nqnfs_vers;
int nqsrv_clockskew = NQ_CLOCKSKEW;
int nqsrv_writeslack = NQ_WRITESLACK;
int nqsrv_maxlease = NQ_MAXLEASE;
int nqsrv_maxnumlease = NQ_MAXNUMLEASE;
void nqsrv_instimeq(), nqsrv_send_eviction(), nfs_sndunlock();
void nqsrv_unlocklease(), nqsrv_waitfor_expiry(), nfsrv_slpderef();
void nqsrv_addhost(), nqsrv_locklease(), nqnfs_serverd();
void nqnfs_clientlease();
struct mbuf *nfsm_rpchead();

/*
 * Signifies which rpcs can have piggybacked lease requests
 */
int nqnfs_piggy[NFS_NPROCS] = {
	0,
	NQL_READ,
	NQL_WRITE,
	0,
	NQL_READ,
	NQL_READ,
	NQL_READ,
	0,
	NQL_WRITE,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	NQL_READ,
	0,
	NQL_READ,
	0,
	0,
	0,
	0,
};

int nnnnnn = sizeof (struct nqlease);
int oooooo = sizeof (struct nfsnode);
extern nfstype nfs_type[9];
extern struct nfssvc_sock *nfs_udpsock, *nfs_cltpsock;
extern struct nfsd nfsd_head;
extern int nfsd_waiting;
extern struct nfsreq nfsreqh;

#define TRUE	1
#define	FALSE	0

/*
 * Get or check for a lease for "vp", based on NQL_CHECK flag.
 * The rules are as follows:
 * - if a current non-caching lease, reply non-caching
 * - if a current lease for same host only, extend lease
 * - if a read cachable lease and a read lease request
 *	add host to list any reply cachable
 * - else { set non-cachable for read-write sharing }
 *	send eviction notice messages to all other hosts that have lease
 *	wait for lease termination { either by receiving vacated messages
 *					from all the other hosts or expiry
 *					via. timeout }
 *	modify lease to non-cachable
 * - else if no current lease, issue new one
 * - reply
 * - return boolean TRUE iff nam should be m_freem()'d
 * NB: Since nqnfs_serverd() is called from a timer, any potential tsleep()
 *     in here must be framed by nqsrv_locklease() and nqsrv_unlocklease().
 *     nqsrv_locklease() is coded such that at least one of LC_LOCKED and
 *     LC_WANTED is set whenever a process is tsleeping in it. The exception
 *     is when a new lease is being allocated, since it is not in the timer
 *     queue yet. (Ditto for the splsoftclock() and splx(s) calls)
 */
nqsrv_getlease(vp, duration, flags, nd, nam, cachablep, frev, cred)
	struct vnode *vp;
	u_long *duration;
	int flags;
	struct nfsd *nd;
	struct mbuf *nam;
	int *cachablep;
	u_quad_t *frev;
	struct ucred *cred;
{
	register struct nqlease *lp, *lq, **lpp;
	register struct nqhost *lph;
	struct nqlease *tlp;
	struct nqm **lphp;
	struct vattr vattr;
	fhandle_t fh;
	int i, ok, error, s;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK)
		return (0);
	if (*duration > nqsrv_maxlease)
		*duration = nqsrv_maxlease;
	if (error = VOP_GETATTR(vp, &vattr, cred, nd->nd_procp))
		return (error);
	*frev = vattr.va_filerev;
	s = splsoftclock();
	tlp = vp->v_lease;
	if ((flags & NQL_CHECK) == 0)
		nfsstats.srvnqnfs_getleases++;
	if (tlp == (struct nqlease *)0) {

		/*
		 * Find the lease by searching the hash list.
		 */
		fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		if (error = VFS_VPTOFH(vp, &fh.fh_fid)) {
			splx(s);
			return (error);
		}
		lpp = &nqfhead[NQFHHASH(fh.fh_fid.fid_data)];
		for (lp = *lpp; lp; lp = lp->lc_fhnext)
			if (fh.fh_fsid.val[0] == lp->lc_fsid.val[0] &&
			    fh.fh_fsid.val[1] == lp->lc_fsid.val[1] &&
			    !bcmp(fh.fh_fid.fid_data, lp->lc_fiddata,
				  fh.fh_fid.fid_len - sizeof (long))) {
				/* Found it */
				lp->lc_vp = vp;
				vp->v_lease = lp;
				tlp = lp;
				break;
			}
	}
	lp = tlp;
	if (lp) {
		if ((lp->lc_flag & LC_NONCACHABLE) ||
		    (lp->lc_morehosts == (struct nqm *)0 &&
		     nqsrv_cmpnam(nd->nd_slp, nam, &lp->lc_host)))
			goto doreply;
		if ((flags & NQL_READ) && (lp->lc_flag & LC_WRITE)==0) {
			if (flags & NQL_CHECK)
				goto doreply;
			if (nqsrv_cmpnam(nd->nd_slp, nam, &lp->lc_host))
				goto doreply;
			i = 0;
			if (lp->lc_morehosts) {
				lph = lp->lc_morehosts->lpm_hosts;
				lphp = &lp->lc_morehosts->lpm_next;
				ok = 1;
			} else {
				lphp = &lp->lc_morehosts;
				ok = 0;
			}
			while (ok && (lph->lph_flag & LC_VALID)) {
				if (nqsrv_cmpnam(nd->nd_slp, nam, lph))
					goto doreply;
				if (++i == LC_MOREHOSTSIZ) {
					i = 0;
					if (*lphp) {
						lph = (*lphp)->lpm_hosts;
						lphp = &((*lphp)->lpm_next);
					} else
						ok = 0;
				} else
					lph++;
			}
			nqsrv_locklease(lp);
			if (!ok) {
				*lphp = (struct nqm *)
					malloc(sizeof (struct nqm),
						M_NQMHOST, M_WAITOK);
				bzero((caddr_t)*lphp, sizeof (struct nqm));
				lph = (*lphp)->lpm_hosts;
			}
			nqsrv_addhost(lph, nd->nd_slp, nam);
			nqsrv_unlocklease(lp);
		} else {
			lp->lc_flag |= LC_NONCACHABLE;
			nqsrv_locklease(lp);
			nqsrv_send_eviction(vp, lp, nd->nd_slp, nam, cred);
			nqsrv_waitfor_expiry(lp);
			nqsrv_unlocklease(lp);
		}
doreply:
		/*
		 * Update the lease and return
		 */
		if ((flags & NQL_CHECK) == 0)
			nqsrv_instimeq(lp, *duration);
		if (lp->lc_flag & LC_NONCACHABLE)
			*cachablep = 0;
		else {
			*cachablep = 1;
			if (flags & NQL_WRITE)
				lp->lc_flag |= LC_WRITTEN;
		}
		splx(s);
		return (0);
	}
	splx(s);
	if (flags & NQL_CHECK)
		return (0);

	/*
	 * Allocate new lease
	 * The value of nqsrv_maxnumlease should be set generously, so that
	 * the following "printf" happens infrequently.
	 */
	if (nfsstats.srvnqnfs_leases > nqsrv_maxnumlease) {
		printf("Nqnfs server, too many leases\n");
		do {
			(void) tsleep((caddr_t)&lbolt, PSOCK,
					"nqsrvnuml", 0);
		} while (nfsstats.srvnqnfs_leases > nqsrv_maxnumlease);
	}
	MALLOC(lp, struct nqlease *, sizeof (struct nqlease), M_NQLEASE, M_WAITOK);
	bzero((caddr_t)lp, sizeof (struct nqlease));
	if (flags & NQL_WRITE)
		lp->lc_flag |= (LC_WRITE | LC_WRITTEN);
	nqsrv_addhost(&lp->lc_host, nd->nd_slp, nam);
	lp->lc_vp = vp;
	lp->lc_fsid = fh.fh_fsid;
	bcopy(fh.fh_fid.fid_data, lp->lc_fiddata, fh.fh_fid.fid_len - sizeof (long));
	if (lq = *lpp)
		lq->lc_fhprev = &lp->lc_fhnext;
	lp->lc_fhnext = lq;
	lp->lc_fhprev = lpp;
	*lpp = lp;
	vp->v_lease = lp;
	s = splsoftclock();
	nqsrv_instimeq(lp, *duration);
	splx(s);
	*cachablep = 1;
	if (++nfsstats.srvnqnfs_leases > nfsstats.srvnqnfs_maxleases)
		nfsstats.srvnqnfs_maxleases = nfsstats.srvnqnfs_leases;
	return (0);
}

/*
 * Local lease check for server syscalls.
 * Just set up args and let nqsrv_getlease() do the rest.
 */
void
lease_check(vp, p, cred, flag)
	struct vnode *vp;
	struct proc *p;
	struct ucred *cred;
	int flag;
{
	int duration = 0, cache;
	struct nfsd nfsd;
	u_quad_t frev;

	nfsd.nd_slp = NQLOCALSLP;
	nfsd.nd_procp = p;
	(void) nqsrv_getlease(vp, &duration, NQL_CHECK | flag, &nfsd,
		(struct mbuf *)0, &cache, &frev, cred);
}

/*
 * Add a host to an nqhost structure for a lease.
 */
void
nqsrv_addhost(lph, slp, nam)
	register struct nqhost *lph;
	struct nfssvc_sock *slp;
	struct mbuf *nam;
{
	register struct sockaddr_in *saddr;

	if (slp == NQLOCALSLP)
		lph->lph_flag |= (LC_VALID | LC_LOCAL);
	else if (slp == nfs_udpsock) {
		saddr = mtod(nam, struct sockaddr_in *);
		lph->lph_flag |= (LC_VALID | LC_UDP);
		lph->lph_inetaddr = saddr->sin_addr.s_addr;
		lph->lph_port = saddr->sin_port;
	} else if (slp == nfs_cltpsock) {
		lph->lph_nam = m_copym(nam, 0, M_COPYALL, M_WAIT);
		lph->lph_flag |= (LC_VALID | LC_CLTP);
	} else {
		lph->lph_flag |= (LC_VALID | LC_SREF);
		lph->lph_slp = slp;
		slp->ns_sref++;
	}
}

/*
 * Update the lease expiry time and position it in the timer queue correctly.
 */
void
nqsrv_instimeq(lp, duration)
	register struct nqlease *lp;
	u_long duration;
{
	register struct nqlease *tlp;
	time_t newexpiry;

	newexpiry = time.tv_sec + duration + nqsrv_clockskew;
	if (lp->lc_expiry == newexpiry)
		return;
	if (lp->lc_chain1[0])
		remque(lp);
	lp->lc_expiry = newexpiry;

	/*
	 * Find where in the queue it should be.
	 */
	tlp = nqthead.th_chain[1];
	while (tlp->lc_expiry > newexpiry && tlp != (struct nqlease *)&nqthead)
		tlp = tlp->lc_chain1[1];
	if (tlp == nqthead.th_chain[1])
		NQSTORENOVRAM(newexpiry);
	insque(lp, tlp);
}

/*
 * Compare the requesting host address with the lph entry in the lease.
 * Return true iff it is the same.
 * This is somewhat messy due to the union in the nqhost structure.
 * The local host is indicated by the special value of NQLOCALSLP for slp.
 */
nqsrv_cmpnam(slp, nam, lph)
	register struct nfssvc_sock *slp;
	struct mbuf *nam;
	register struct nqhost *lph;
{
	register struct sockaddr_in *saddr;
	struct mbuf *addr;
	union nethostaddr lhaddr;
	int ret;

	if (slp == NQLOCALSLP) {
		if (lph->lph_flag & LC_LOCAL)
			return (1);
		else
			return (0);
	}
	if (slp == nfs_udpsock || slp == nfs_cltpsock)
		addr = nam;
	else
		addr = slp->ns_nam;
	if (lph->lph_flag & LC_UDP)
		ret = netaddr_match(AF_INET, &lph->lph_haddr, addr);
	else if (lph->lph_flag & LC_CLTP)
		ret = netaddr_match(AF_ISO, &lph->lph_claddr, addr);
	else {
		if ((lph->lph_slp->ns_flag & SLP_VALID) == 0)
			return (0);
		saddr = mtod(lph->lph_slp->ns_nam, struct sockaddr_in *);
		if (saddr->sin_family == AF_INET)
			lhaddr.had_inetaddr = saddr->sin_addr.s_addr;
		else
			lhaddr.had_nam = lph->lph_slp->ns_nam;
		ret = netaddr_match(saddr->sin_family, &lhaddr, addr);
	}
	return (ret);
}

/*
 * Send out eviction notice messages to all other hosts for the lease.
 */
void
nqsrv_send_eviction(vp, lp, slp, nam, cred)
	struct vnode *vp;
	register struct nqlease *lp;
	struct nfssvc_sock *slp;
	struct mbuf *nam;
	struct ucred *cred;
{
	register struct nqhost *lph = &lp->lc_host;
	register struct mbuf *m;
	register int siz;
	struct nqm *lphnext = lp->lc_morehosts;
	struct mbuf *mreq, *mb, *mb2, *nam2, *mheadend;
	struct socket *so;
	struct sockaddr_in *saddr;
	fhandle_t *fhp;
	caddr_t bpos, cp;
	u_long xid;
	int len = 1, ok = 1, i = 0;
	int sotype, *solockp;

	while (ok && (lph->lph_flag & LC_VALID)) {
		if (nqsrv_cmpnam(slp, nam, lph))
			lph->lph_flag |= LC_VACATED;
		else if ((lph->lph_flag & (LC_LOCAL | LC_VACATED)) == 0) {
			if (lph->lph_flag & LC_UDP) {
				MGET(nam2, M_WAIT, MT_SONAME);
				saddr = mtod(nam2, struct sockaddr_in *);
				nam2->m_len = saddr->sin_len =
					sizeof (struct sockaddr_in);
				saddr->sin_family = AF_INET;
				saddr->sin_addr.s_addr = lph->lph_inetaddr;
				saddr->sin_port = lph->lph_port;
				so = nfs_udpsock->ns_so;
			} else if (lph->lph_flag & LC_CLTP) {
				nam2 = lph->lph_nam;
				so = nfs_cltpsock->ns_so;
			} else if (lph->lph_slp->ns_flag & SLP_VALID) {
				nam2 = (struct mbuf *)0;
				so = lph->lph_slp->ns_so;
			} else
				goto nextone;
			sotype = so->so_type;
			if (so->so_proto->pr_flags & PR_CONNREQUIRED)
				solockp = &lph->lph_slp->ns_solock;
			else
				solockp = (int *)0;
			nfsm_reqhead((struct vnode *)0, NQNFSPROC_EVICTED,
				NFSX_FH);
			nfsm_build(cp, caddr_t, NFSX_FH);
			bzero(cp, NFSX_FH);
			fhp = (fhandle_t *)cp;
			fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
			VFS_VPTOFH(vp, &fhp->fh_fid);
			m = mreq;
			siz = 0;
			while (m) {
				siz += m->m_len;
				m = m->m_next;
			}
			if (siz <= 0 || siz > NFS_MAXPACKET) {
				printf("mbuf siz=%d\n",siz);
				panic("Bad nfs svc reply");
			}
			m = nfsm_rpchead(cred, TRUE, NQNFSPROC_EVICTED,
				RPCAUTH_UNIX, 5*NFSX_UNSIGNED, (char *)0,
				mreq, siz, &mheadend, &xid);
			/*
			 * For stream protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (sotype == SOCK_STREAM) {
				M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
				*mtod(m, u_long *) = htonl(0x80000000 |
					(m->m_pkthdr.len - NFSX_UNSIGNED));
			}
			if (((lph->lph_flag & (LC_UDP | LC_CLTP)) == 0 &&
			    (lph->lph_slp->ns_flag & SLP_VALID) == 0) ||
			    (solockp && (*solockp & NFSMNT_SNDLOCK)))
				m_freem(m);
			else {
				if (solockp)
					*solockp |= NFSMNT_SNDLOCK;
				(void) nfs_send(so, nam2, m,
						(struct nfsreq *)0);
				if (solockp)
					nfs_sndunlock(solockp);
			}
			if (lph->lph_flag & LC_UDP)
				MFREE(nam2, m);
		}
nextone:
		if (++i == len) {
			if (lphnext) {
				i = 0;
				len = LC_MOREHOSTSIZ;
				lph = lphnext->lpm_hosts;
				lphnext = lphnext->lpm_next;
			} else
				ok = 0;
		} else
			lph++;
	}
}

/*
 * Wait for the lease to expire.
 * This will occur when all clients have sent "vacated" messages to
 * this server OR when it expires do to timeout.
 */
void
nqsrv_waitfor_expiry(lp)
	register struct nqlease *lp;
{
	register struct nqhost *lph;
	register int i;
	struct nqm *lphnext;
	int len, ok;

tryagain:
	if (time.tv_sec > lp->lc_expiry)
		return;
	lph = &lp->lc_host;
	lphnext = lp->lc_morehosts;
	len = 1;
	i = 0;
	ok = 1;
	while (ok && (lph->lph_flag & LC_VALID)) {
		if ((lph->lph_flag & (LC_LOCAL | LC_VACATED)) == 0) {
			lp->lc_flag |= LC_EXPIREDWANTED;
			(void) tsleep((caddr_t)&lp->lc_flag, PSOCK,
					"nqexp", 0);
			goto tryagain;
		}
		if (++i == len) {
			if (lphnext) {
				i = 0;
				len = LC_MOREHOSTSIZ;
				lph = lphnext->lpm_hosts;
				lphnext = lphnext->lpm_next;
			} else
				ok = 0;
		} else
			lph++;
	}
}

/*
 * Nqnfs server timer that maintains the server lease queue.
 * Scan the lease queue for expired entries:
 * - when one is found, wakeup anyone waiting for it
 *   else dequeue and free
 */
void
nqnfs_serverd()
{
	register struct nqlease *lp, *lq;
	register struct nqhost *lph;
	struct nqlease *nextlp;
	struct nqm *lphnext, *olphnext;
	struct mbuf *n;
	int i, len, ok;

	lp = nqthead.th_chain[0];
	while (lp != (struct nqlease *)&nqthead) {
		if (lp->lc_expiry >= time.tv_sec)
			break;
		nextlp = lp->lc_chain1[0];
		if (lp->lc_flag & LC_EXPIREDWANTED) {
			lp->lc_flag &= ~LC_EXPIREDWANTED;
			wakeup((caddr_t)&lp->lc_flag);
		} else if ((lp->lc_flag & (LC_LOCKED | LC_WANTED)) == 0) {
		    /*
		     * Make a best effort at keeping a write caching lease long
		     * enough by not deleting it until it has been explicitly
		     * vacated or there have been no writes in the previous
		     * write_slack seconds since expiry and the nfsds are not
		     * all busy. The assumption is that if the nfsds are not
		     * all busy now (no queue of nfs requests), then the client
		     * would have been able to do at least one write to the
		     * file during the last write_slack seconds if it was still
		     * trying to push writes to the server.
		     */
		    if ((lp->lc_flag & (LC_WRITE | LC_VACATED)) == LC_WRITE &&
			((lp->lc_flag & LC_WRITTEN) || nfsd_waiting == 0)) {
			lp->lc_flag &= ~LC_WRITTEN;
			nqsrv_instimeq(lp, nqsrv_writeslack);
		    } else {
			remque(lp);
			if (lq = lp->lc_fhnext)
				lq->lc_fhprev = lp->lc_fhprev;
			*lp->lc_fhprev = lq;
			/*
			 * This soft reference may no longer be valid, but
			 * no harm done. The worst case is if the vnode was
			 * recycled and has another valid lease reference,
			 * which is dereferenced prematurely.
			 */
			lp->lc_vp->v_lease = (struct nqlease *)0;
			lph = &lp->lc_host;
			lphnext = lp->lc_morehosts;
			olphnext = (struct nqm *)0;
			len = 1;
			i = 0;
			ok = 1;
			while (ok && (lph->lph_flag & LC_VALID)) {
				if (lph->lph_flag & LC_CLTP)
					MFREE(lph->lph_nam, n);
				if (lph->lph_flag & LC_SREF)
					nfsrv_slpderef(lph->lph_slp);
				if (++i == len) {
					if (olphnext) {
						free((caddr_t)olphnext, M_NQMHOST);
						olphnext = (struct nqm *)0;
					}
					if (lphnext) {
						olphnext = lphnext;
						i = 0;
						len = LC_MOREHOSTSIZ;
						lph = lphnext->lpm_hosts;
						lphnext = lphnext->lpm_next;
					} else
						ok = 0;
				} else
					lph++;
			}
			FREE((caddr_t)lp, M_NQLEASE);
			if (olphnext)
				free((caddr_t)olphnext, M_NQMHOST);
			nfsstats.srvnqnfs_leases--;
		    }
		}
		lp = nextlp;
	}
}

/*
 * Called from nfssvc_nfsd() for a getlease rpc request.
 * Do the from/to xdr translation and call nqsrv_getlease() to
 * do the real work.
 */
nqnfsrv_getlease(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct nfsv2_fattr *fp;
	struct vattr va;
	register struct vattr *vap = &va;
	struct vnode *vp;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	register u_long *tl;
	register long t1;
	u_quad_t frev;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	int flags, rdonly, cache;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_long *, 2*NFSX_UNSIGNED);
	flags = fxdr_unsigned(int, *tl++);
	nfsd->nd_duration = fxdr_unsigned(int, *tl);
	if (error = nfsrv_fhtovp(fhp,
	    TRUE, &vp, cred, nfsd->nd_slp, nam, &rdonly))
		nfsm_reply(0);
	if (rdonly && flags == NQL_WRITE) {
		error = EROFS;
		nfsm_reply(0);
	}
	(void) nqsrv_getlease(vp, &nfsd->nd_duration, flags, nfsd,
		nam, &cache, &frev, cred);
	error = VOP_GETATTR(vp, vap, cred, nfsd->nd_procp);
	vput(vp);
	nfsm_reply(NFSX_NQFATTR + 4*NFSX_UNSIGNED);
	nfsm_build(tl, u_long *, 4*NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(cache);
	*tl++ = txdr_unsigned(nfsd->nd_duration);
	txdr_hyper(&frev, tl);
	nfsm_build(fp, struct nfsv2_fattr *, NFSX_NQFATTR);
	nfsm_srvfillattr;
	nfsm_srvdone;
}

/*
 * Called from nfssvc_nfsd() when a "vacated" message is received from a
 * client. Find the entry and expire it.
 */
nqnfsrv_vacated(nfsd, mrep, md, dpos, cred, nam, mrq)
	struct nfsd *nfsd;
	struct mbuf *mrep, *md;
	caddr_t dpos;
	struct ucred *cred;
	struct mbuf *nam, **mrq;
{
	register struct nqlease *lp;
	register struct nqhost *lph;
	struct nqlease *tlp = (struct nqlease *)0;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	register u_long *tl;
	register long t1;
	struct nqm *lphnext;
	int error = 0, i, len, ok, gotit = 0;
	char *cp2;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	m_freem(mrep);
	/*
	 * Find the lease by searching the hash list.
	 */
	for (lp = nqfhead[NQFHHASH(fhp->fh_fid.fid_data)]; lp;
	     lp = lp->lc_fhnext)
		if (fhp->fh_fsid.val[0] == lp->lc_fsid.val[0] &&
		    fhp->fh_fsid.val[1] == lp->lc_fsid.val[1] &&
		    !bcmp(fhp->fh_fid.fid_data, lp->lc_fiddata,
			  MAXFIDSZ)) {
			/* Found it */
			tlp = lp;
			break;
		}
	if (tlp) {
		lp = tlp;
		len = 1;
		i = 0;
		lph = &lp->lc_host;
		lphnext = lp->lc_morehosts;
		ok = 1;
		while (ok && (lph->lph_flag & LC_VALID)) {
			if (nqsrv_cmpnam(nfsd->nd_slp, nam, lph)) {
				lph->lph_flag |= LC_VACATED;
				gotit++;
				break;
			}
			if (++i == len) {
				if (lphnext) {
					len = LC_MOREHOSTSIZ;
					i = 0;
					lph = lphnext->lpm_hosts;
					lphnext = lphnext->lpm_next;
				} else
					ok = 0;
			} else
				lph++;
		}
		if ((lp->lc_flag & LC_EXPIREDWANTED) && gotit) {
			lp->lc_flag &= ~LC_EXPIREDWANTED;
			wakeup((caddr_t)&lp->lc_flag);
		}
nfsmout:
		return (EPERM);
	}
	return (EPERM);
}

/*
 * Client get lease rpc function.
 */
nqnfs_getlease(vp, rwflag, cred, p)
	register struct vnode *vp;
	int rwflag;
	struct ucred *cred;
	struct proc *p;
{
	register u_long *tl;
	register caddr_t cp;
	register long t1;
	register struct nfsnode *np;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t bpos, dpos, cp2;
	time_t reqtime;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int cachable;
	u_quad_t frev;
	
	nfsstats.rpccnt[NQNFSPROC_GETLEASE]++;
	mb = mreq = nfsm_reqh(vp, NQNFSPROC_GETLEASE, NFSX_FH+2*NFSX_UNSIGNED,
		 &bpos);
	nfsm_fhtom(vp);
	nfsm_build(tl, u_long *, 2*NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(rwflag);
	*tl = txdr_unsigned(nmp->nm_leaseterm);
	reqtime = time.tv_sec;
	nfsm_request(vp, NQNFSPROC_GETLEASE, p, cred);
	np = VTONFS(vp);
	nfsm_dissect(tl, u_long *, 4*NFSX_UNSIGNED);
	cachable = fxdr_unsigned(int, *tl++);
	reqtime += fxdr_unsigned(int, *tl++);
	if (reqtime > time.tv_sec) {
		fxdr_hyper(tl, &frev);
		nqnfs_clientlease(nmp, np, rwflag, cachable, reqtime, frev);
		nfsm_loadattr(vp, (struct vattr *)0);
	} else
		error = NQNFS_EXPIRED;
	nfsm_reqdone;
	return (error);
}

/*
 * Client vacated message function.
 */
nqnfs_vacated(vp, cred)
	register struct vnode *vp;
	struct ucred *cred;
{
	register caddr_t cp;
	register struct mbuf *m;
	register int i;
	caddr_t bpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mb, *mb2, *mheadend;
	struct nfsmount *nmp;
	struct nfsreq myrep;
	
	nmp = VFSTONFS(vp->v_mount);
	nfsstats.rpccnt[NQNFSPROC_VACATED]++;
	nfsm_reqhead(vp, NQNFSPROC_VACATED, NFSX_FH);
	nfsm_fhtom(vp);
	m = mreq;
	i = 0;
	while (m) {
		i += m->m_len;
		m = m->m_next;
	}
	m = nfsm_rpchead(cred, TRUE, NQNFSPROC_VACATED,
		RPCAUTH_UNIX, 5*NFSX_UNSIGNED, (char *)0,
		mreq, i, &mheadend, &xid);
	if (nmp->nm_sotype == SOCK_STREAM) {
		M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
		*mtod(m, u_long *) = htonl(0x80000000 | (m->m_pkthdr.len -
			NFSX_UNSIGNED));
	}
	myrep.r_flags = 0;
	myrep.r_nmp = nmp;
	if (nmp->nm_soflags & PR_CONNREQUIRED)
		(void) nfs_sndlock(&nmp->nm_flag, (struct nfsreq *)0);
	(void) nfs_send(nmp->nm_so, nmp->nm_nam, m, &myrep);
	if (nmp->nm_soflags & PR_CONNREQUIRED)
		nfs_sndunlock(&nmp->nm_flag);
	return (error);
}

/*
 * Called for client side callbacks
 */
nqnfs_callback(nmp, mrep, md, dpos)
	struct nfsmount *nmp;
	struct mbuf *mrep, *md;
	caddr_t dpos;
{
	register struct vnode *vp;
	register u_long *tl;
	register long t1;
	nfsv2fh_t nfh;
	fhandle_t *fhp;
	struct nfsnode *np;
	struct nfsd nd;
	int error;
	char *cp2;

	nd.nd_mrep = mrep;
	nd.nd_md = md;
	nd.nd_dpos = dpos;
	if (error = nfs_getreq(&nd, FALSE))
		return (error);
	md = nd.nd_md;
	dpos = nd.nd_dpos;
	if (nd.nd_procnum != NQNFSPROC_EVICTED) {
		m_freem(mrep);
		return (EPERM);
	}
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	m_freem(mrep);
	if (error = nfs_nget(nmp->nm_mountp, fhp, &np))
		return (error);
	vp = NFSTOV(np);
	if (np->n_tnext) {
		np->n_expiry = 0;
		np->n_flag |= NQNFSEVICTED;
		if (np->n_tprev != (struct nfsnode *)nmp) {
			if (np->n_tnext == (struct nfsnode *)nmp)
				nmp->nm_tprev = np->n_tprev;
			else
				np->n_tnext->n_tprev = np->n_tprev;
			np->n_tprev->n_tnext = np->n_tnext;
			np->n_tnext = nmp->nm_tnext;
			nmp->nm_tnext = np;
			np->n_tprev = (struct nfsnode *)nmp;
			if (np->n_tnext == (struct nfsnode *)nmp)
				nmp->nm_tprev = np;
			else
				np->n_tnext->n_tprev = np;
		}
	}
	vrele(vp);
	nfsm_srvdone;
}

/*
 * Nqnfs client helper daemon. Runs once a second to expire leases.
 * It also get authorization strings for "kerb" mounts.
 * It must start at the beginning of the list again after any potential
 * "sleep" since nfs_reclaim() called from vclean() can pull a node off
 * the list asynchronously.
 */
nqnfs_clientd(nmp, cred, ncd, flag, argp, p)
	register struct nfsmount *nmp;
	struct ucred *cred;
	struct nfsd_cargs *ncd;
	int flag;
	caddr_t argp;
	struct proc *p;
{
	register struct nfsnode *np;
	struct vnode *vp;
	struct nfsreq myrep;
	int error, vpid;

	/*
	 * First initialize some variables
	 */
	nqnfs_prog = txdr_unsigned(NQNFS_PROG);
	nqnfs_vers = txdr_unsigned(NQNFS_VER1);

	/*
	 * If an authorization string is being passed in, get it.
	 */
	if ((flag & NFSSVC_GOTAUTH) &&
		(nmp->nm_flag & (NFSMNT_WAITAUTH | NFSMNT_DISMNT)) == 0) {
		if (nmp->nm_flag & NFSMNT_HASAUTH)
			panic("cld kerb");
		if ((flag & NFSSVC_AUTHINFAIL) == 0) {
			if (ncd->ncd_authlen <= RPCAUTH_MAXSIZ &&
				copyin(ncd->ncd_authstr, nmp->nm_authstr,
				ncd->ncd_authlen) == 0) {
				nmp->nm_authtype = ncd->ncd_authtype;
				nmp->nm_authlen = ncd->ncd_authlen;
			} else
				nmp->nm_flag |= NFSMNT_AUTHERR;
		} else
			nmp->nm_flag |= NFSMNT_AUTHERR;
		nmp->nm_flag |= NFSMNT_HASAUTH;
		wakeup((caddr_t)&nmp->nm_authlen);
	} else
		nmp->nm_flag |= NFSMNT_WAITAUTH;

	/*
	 * Loop every second updating queue until there is a termination sig.
	 */
	while ((nmp->nm_flag & NFSMNT_DISMNT) == 0) {
	    if (nmp->nm_flag & NFSMNT_NQNFS) {
		/*
		 * If there are no outstanding requests (and therefore no
		 * processes in nfs_reply) and there is data in the receive
		 * queue, poke for callbacks.
		 */
		if (nfsreqh.r_next == &nfsreqh && nmp->nm_so &&
		    nmp->nm_so->so_rcv.sb_cc > 0) {
		    myrep.r_flags = R_GETONEREP;
		    myrep.r_nmp = nmp;
		    myrep.r_mrep = (struct mbuf *)0;
		    myrep.r_procp = (struct proc *)0;
		    (void) nfs_reply(&myrep);
		}

		/*
		 * Loop through the leases, updating as required.
		 */
		np = nmp->nm_tnext;
		while (np != (struct nfsnode *)nmp &&
		       (nmp->nm_flag & NFSMNT_DISMINPROG) == 0) {
			vp = NFSTOV(np);
if (vp->v_mount->mnt_stat.f_fsid.val[1] != MOUNT_NFS) panic("trash2");
			vpid = vp->v_id;
			if (np->n_expiry < time.tv_sec) {
			   if (vget(vp, 1) == 0) {
			     nmp->nm_inprog = vp;
			     if (vpid == vp->v_id) {
if (vp->v_mount->mnt_stat.f_fsid.val[1] != MOUNT_NFS) panic("trash3");
				if (np->n_tnext == (struct nfsnode *)nmp)
					nmp->nm_tprev = np->n_tprev;
				else
					np->n_tnext->n_tprev = np->n_tprev;
				if (np->n_tprev == (struct nfsnode *)nmp)
					nmp->nm_tnext = np->n_tnext;
				else
					np->n_tprev->n_tnext = np->n_tnext;
				np->n_tnext = (struct nfsnode *)0;
				if ((np->n_flag & (NMODIFIED | NQNFSEVICTED))
				    && vp->v_type == VREG) {
					if (np->n_flag & NQNFSEVICTED) {
						(void) nfs_vinvalbuf(vp,
						       V_SAVE, cred, p, 0);
						np->n_flag &= ~NQNFSEVICTED;
						(void) nqnfs_vacated(vp, cred);
					} else {
						(void) VOP_FSYNC(vp, cred,
						    MNT_WAIT, p);
						np->n_flag &= ~NMODIFIED;
					}
				}
			      }
			      vrele(vp);
			      nmp->nm_inprog = NULLVP;
			    }
			    if (np != nmp->nm_tnext)
				np = nmp->nm_tnext;
			    else
				break;
			} else if ((np->n_expiry - NQ_RENEWAL) < time.tv_sec) {
			    if ((np->n_flag & (NQNFSWRITE | NQNFSNONCACHE))
				 == NQNFSWRITE && vp->v_dirtyblkhd.lh_first &&
				 vget(vp, 1) == 0) {
				 nmp->nm_inprog = vp;
if (vp->v_mount->mnt_stat.f_fsid.val[1] != MOUNT_NFS) panic("trash4");
				 if (vpid == vp->v_id &&
				     nqnfs_getlease(vp, NQL_WRITE, cred, p)==0)
					np->n_brev = np->n_lrev;
				 vrele(vp);
				 nmp->nm_inprog = NULLVP;
			    }
			    if (np != nmp->nm_tnext)
				np = nmp->nm_tnext;
			    else
				break;
			} else
				break;
		}
	    }

	    /*
	     * Get an authorization string, if required.
	     */
	    if ((nmp->nm_flag & (NFSMNT_WAITAUTH | NFSMNT_DISMNT | NFSMNT_HASAUTH)) == 0) {
		ncd->ncd_authuid = nmp->nm_authuid;
		if (copyout((caddr_t)ncd, argp, sizeof (struct nfsd_cargs)))
			nmp->nm_flag |= NFSMNT_WAITAUTH;
		else
			return (ENEEDAUTH);
	    }

	    /*
	     * Wait a bit (no pun) and do it again.
	     */
	    if ((nmp->nm_flag & NFSMNT_DISMNT) == 0 &&
		(nmp->nm_flag & (NFSMNT_WAITAUTH | NFSMNT_HASAUTH))) {
		    error = tsleep((caddr_t)&nmp->nm_authstr, PSOCK | PCATCH,
			"nqnfstimr", hz / 3);
		    if (error == EINTR || error == ERESTART)
			(void) dounmount(nmp->nm_mountp, 0, p);
	    }
	}
	free((caddr_t)nmp, M_NFSMNT);
	if (error == EWOULDBLOCK)
		error = 0;
	return (error);
}

/*
 * Adjust all timer queue expiry times when the time of day clock is changed.
 * Called from the settimeofday() syscall.
 */
void
lease_updatetime(deltat)
	register int deltat;
{
	register struct nqlease *lp;
	register struct nfsnode *np;
	struct mount *mp;
	struct nfsmount *nmp;
	int s;

	if (nqnfsstarttime != 0)
		nqnfsstarttime += deltat;
	s = splsoftclock();
	lp = nqthead.th_chain[0];
	while (lp != (struct nqlease *)&nqthead) {
		lp->lc_expiry += deltat;
		lp = lp->lc_chain1[0];
	}
	splx(s);

	/*
	 * Search the mount list for all nqnfs mounts and do their timer
	 * queues.
	 */
	for (mp = mountlist.tqh_first; mp != NULL; mp = mp->mnt_list.tqe_next) {
		if (mp->mnt_stat.f_fsid.val[1] == MOUNT_NFS) {
			nmp = VFSTONFS(mp);
			if (nmp->nm_flag & NFSMNT_NQNFS) {
				np = nmp->nm_tnext;
				while (np != (struct nfsnode *)nmp) {
					np->n_expiry += deltat;
					np = np->n_tnext;
				}
			}
		}
	}
}

/*
 * Lock a server lease.
 */
void
nqsrv_locklease(lp)
	struct nqlease *lp;
{

	while (lp->lc_flag & LC_LOCKED) {
		lp->lc_flag |= LC_WANTED;
		(void) tsleep((caddr_t)lp, PSOCK, "nqlc", 0);
	}
	lp->lc_flag |= LC_LOCKED;
	lp->lc_flag &= ~LC_WANTED;
}

/*
 * Unlock a server lease.
 */
void
nqsrv_unlocklease(lp)
	struct nqlease *lp;
{

	lp->lc_flag &= ~LC_LOCKED;
	if (lp->lc_flag & LC_WANTED)
		wakeup((caddr_t)lp);
}

/*
 * Update a client lease.
 */
void
nqnfs_clientlease(nmp, np, rwflag, cachable, expiry, frev)
	register struct nfsmount *nmp;
	register struct nfsnode *np;
	int rwflag, cachable;
	time_t expiry;
	u_quad_t frev;
{
	register struct nfsnode *tp;

	if (np->n_tnext) {
		if (np->n_tnext == (struct nfsnode *)nmp)
			nmp->nm_tprev = np->n_tprev;
		else
			np->n_tnext->n_tprev = np->n_tprev;
		if (np->n_tprev == (struct nfsnode *)nmp)
			nmp->nm_tnext = np->n_tnext;
		else
			np->n_tprev->n_tnext = np->n_tnext;
		if (rwflag == NQL_WRITE)
			np->n_flag |= NQNFSWRITE;
	} else if (rwflag == NQL_READ)
		np->n_flag &= ~NQNFSWRITE;
	else
		np->n_flag |= NQNFSWRITE;
	if (cachable)
		np->n_flag &= ~NQNFSNONCACHE;
	else
		np->n_flag |= NQNFSNONCACHE;
	np->n_expiry = expiry;
	np->n_lrev = frev;
	tp = nmp->nm_tprev;
	while (tp != (struct nfsnode *)nmp && tp->n_expiry > np->n_expiry)
		tp = tp->n_tprev;
	if (tp == (struct nfsnode *)nmp) {
		np->n_tnext = nmp->nm_tnext;
		nmp->nm_tnext = np;
	} else {
		np->n_tnext = tp->n_tnext;
		tp->n_tnext = np;
	}
	np->n_tprev = tp;
	if (np->n_tnext == (struct nfsnode *)nmp)
		nmp->nm_tprev = np;
	else
		np->n_tnext->n_tprev = np;
}
