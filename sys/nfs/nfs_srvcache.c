/*
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
 *	@(#)nfs_srvcache.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Reference: Chet Juszczak, "Improving the Performance and Correctness
 *		of an NFS Server", in Proc. Winter 1989 USENIX Conference,
 *		pages 53-63. San Diego, February 1989.
 */
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netinet/in.h>
#ifdef ISO
#include <netiso/iso.h>
#endif
#include <nfs/nfsm_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsrvcache.h>
#include <nfs/nqnfs.h>

long numnfsrvcache, desirednfsrvcache = NFSRVCACHESIZ;

#define	NFSRCHASH(xid)		(((xid) + ((xid) >> 24)) & rheadhash)
static struct nfsrvcache *nfsrvlruhead, **nfsrvlrutail = &nfsrvlruhead;
static struct nfsrvcache **rheadhtbl;
static u_long rheadhash;

#define TRUE	1
#define	FALSE	0

#define	NETFAMILY(rp) \
		(((rp)->rc_flag & RC_INETADDR) ? AF_INET : AF_ISO)

/*
 * Static array that defines which nfs rpc's are nonidempotent
 */
int nonidempotent[NFS_NPROCS] = {
	FALSE,
	FALSE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
};

/* True iff the rpc reply is an nfs status ONLY! */
static int repliesstatus[NFS_NPROCS] = {
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
};

/*
 * Initialize the server request cache list
 */
nfsrv_initcache()
{

	rheadhtbl = hashinit(desirednfsrvcache, M_NFSD, &rheadhash);
}

/*
 * Look for the request in the cache
 * If found then
 *    return action and optionally reply
 * else
 *    insert it in the cache
 *
 * The rules are as follows:
 * - if in progress, return DROP request
 * - if completed within DELAY of the current time, return DROP it
 * - if completed a longer time ago return REPLY if the reply was cached or
 *   return DOIT
 * Update/add new request at end of lru list
 */
nfsrv_getcache(nam, nd, repp)
	struct mbuf *nam;
	register struct nfsd *nd;
	struct mbuf **repp;
{
	register struct nfsrvcache *rp, *rq, **rpp;
	struct mbuf *mb;
	struct sockaddr_in *saddr;
	caddr_t bpos;
	int ret;

	if (nd->nd_nqlflag != NQL_NOVAL)
		return (RC_DOIT);
	rpp = &rheadhtbl[NFSRCHASH(nd->nd_retxid)];
loop:
	for (rp = *rpp; rp; rp = rp->rc_forw) {
	    if (nd->nd_retxid == rp->rc_xid && nd->nd_procnum == rp->rc_proc &&
		netaddr_match(NETFAMILY(rp), &rp->rc_haddr, nam)) {
			if ((rp->rc_flag & RC_LOCKED) != 0) {
				rp->rc_flag |= RC_WANTED;
				(void) tsleep((caddr_t)rp, PZERO-1, "nfsrc", 0);
				goto loop;
			}
			rp->rc_flag |= RC_LOCKED;
			/* If not at end of LRU chain, move it there */
			if (rp->rc_next) {
				/* remove from LRU chain */
				*rp->rc_prev = rp->rc_next;
				rp->rc_next->rc_prev = rp->rc_prev;
				/* and replace at end of it */
				rp->rc_next = NULL;
				rp->rc_prev = nfsrvlrutail;
				*nfsrvlrutail = rp;
				nfsrvlrutail = &rp->rc_next;
			}
			if (rp->rc_state == RC_UNUSED)
				panic("nfsrv cache");
			if (rp->rc_state == RC_INPROG) {
				nfsstats.srvcache_inproghits++;
				ret = RC_DROPIT;
			} else if (rp->rc_flag & RC_REPSTATUS) {
				nfsstats.srvcache_nonidemdonehits++;
				nfs_rephead(0, nd, rp->rc_status,
				   0, (u_quad_t *)0, repp, &mb, &bpos);
				ret = RC_REPLY;
			} else if (rp->rc_flag & RC_REPMBUF) {
				nfsstats.srvcache_nonidemdonehits++;
				*repp = m_copym(rp->rc_reply, 0, M_COPYALL,
						M_WAIT);
				ret = RC_REPLY;
			} else {
				nfsstats.srvcache_idemdonehits++;
				rp->rc_state = RC_INPROG;
				ret = RC_DOIT;
			}
			rp->rc_flag &= ~RC_LOCKED;
			if (rp->rc_flag & RC_WANTED) {
				rp->rc_flag &= ~RC_WANTED;
				wakeup((caddr_t)rp);
			}
			return (ret);
		}
	}
	nfsstats.srvcache_misses++;
	if (numnfsrvcache < desirednfsrvcache) {
		rp = (struct nfsrvcache *)malloc((u_long)sizeof *rp,
		    M_NFSD, M_WAITOK);
		bzero((char *)rp, sizeof *rp);
		numnfsrvcache++;
		rp->rc_flag = RC_LOCKED;
	} else {
		rp = nfsrvlruhead;
		while ((rp->rc_flag & RC_LOCKED) != 0) {
			rp->rc_flag |= RC_WANTED;
			(void) tsleep((caddr_t)rp, PZERO-1, "nfsrc", 0);
			rp = nfsrvlruhead;
		}
		rp->rc_flag |= RC_LOCKED;
		/* remove from hash chain */
		if (rq = rp->rc_forw)
			rq->rc_back = rp->rc_back;
		*rp->rc_back = rq;
		/* remove from LRU chain */
		*rp->rc_prev = rp->rc_next;
		rp->rc_next->rc_prev = rp->rc_prev;
		if (rp->rc_flag & RC_REPMBUF)
			m_freem(rp->rc_reply);
		if (rp->rc_flag & RC_NAM)
			MFREE(rp->rc_nam, mb);
		rp->rc_flag &= (RC_LOCKED | RC_WANTED);
	}
	/* place at end of LRU list */
	rp->rc_next = NULL;
	rp->rc_prev = nfsrvlrutail;
	*nfsrvlrutail = rp;
	nfsrvlrutail = &rp->rc_next;
	rp->rc_state = RC_INPROG;
	rp->rc_xid = nd->nd_retxid;
	saddr = mtod(nam, struct sockaddr_in *);
	switch (saddr->sin_family) {
	case AF_INET:
		rp->rc_flag |= RC_INETADDR;
		rp->rc_inetaddr = saddr->sin_addr.s_addr;
		break;
	case AF_ISO:
	default:
		rp->rc_flag |= RC_NAM;
		rp->rc_nam = m_copym(nam, 0, M_COPYALL, M_WAIT);
		break;
	};
	rp->rc_proc = nd->nd_procnum;
	/* insert into hash chain */
	if (rq = *rpp)
		rq->rc_back = &rp->rc_forw;
	rp->rc_forw = rq;
	rp->rc_back = rpp;
	*rpp = rp;
	rp->rc_flag &= ~RC_LOCKED;
	if (rp->rc_flag & RC_WANTED) {
		rp->rc_flag &= ~RC_WANTED;
		wakeup((caddr_t)rp);
	}
	return (RC_DOIT);
}

/*
 * Update a request cache entry after the rpc has been done
 */
void
nfsrv_updatecache(nam, nd, repvalid, repmbuf)
	struct mbuf *nam;
	register struct nfsd *nd;
	int repvalid;
	struct mbuf *repmbuf;
{
	register struct nfsrvcache *rp;

	if (nd->nd_nqlflag != NQL_NOVAL)
		return;
loop:
	for (rp = rheadhtbl[NFSRCHASH(nd->nd_retxid)]; rp; rp = rp->rc_forw) {
	    if (nd->nd_retxid == rp->rc_xid && nd->nd_procnum == rp->rc_proc &&
		netaddr_match(NETFAMILY(rp), &rp->rc_haddr, nam)) {
			if ((rp->rc_flag & RC_LOCKED) != 0) {
				rp->rc_flag |= RC_WANTED;
				(void) tsleep((caddr_t)rp, PZERO-1, "nfsrc", 0);
				goto loop;
			}
			rp->rc_flag |= RC_LOCKED;
			rp->rc_state = RC_DONE;
			/*
			 * If we have a valid reply update status and save
			 * the reply for non-idempotent rpc's.
			 */
			if (repvalid && nonidempotent[nd->nd_procnum]) {
				if (repliesstatus[nd->nd_procnum]) {
					rp->rc_status = nd->nd_repstat;
					rp->rc_flag |= RC_REPSTATUS;
				} else {
					rp->rc_reply = m_copym(repmbuf,
						0, M_COPYALL, M_WAIT);
					rp->rc_flag |= RC_REPMBUF;
				}
			}
			rp->rc_flag &= ~RC_LOCKED;
			if (rp->rc_flag & RC_WANTED) {
				rp->rc_flag &= ~RC_WANTED;
				wakeup((caddr_t)rp);
			}
			return;
		}
	}
}

/*
 * Clean out the cache. Called when the last nfsd terminates.
 */
void
nfsrv_cleancache()
{
	register struct nfsrvcache *rp, *nextrp;

	for (rp = nfsrvlruhead; rp; rp = nextrp) {
		nextrp = rp->rc_next;
		free(rp, M_NFSD);
	}
	bzero((char *)rheadhtbl, (rheadhash + 1) * sizeof(void *));
	nfsrvlruhead = NULL;
	nfsrvlrutail = &nfsrvlruhead;
	numnfsrvcache = 0;
}
