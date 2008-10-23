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
 *	@(#)nfs_srvcache.c	8.3 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Reference: Chet Juszczak, "Improving the Performance and Correctness
 *		of an NFS Server", in Proc. Winter 1989 USENIX Conference,
 *		pages 53-63. San Diego, February 1989.
 */
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>	/* for sodupsockaddr */
#include <sys/eventhandler.h>

#include <netinet/in.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>
#include <nfsserver/nfsrvcache.h>

static long numnfsrvcache;
static long desirednfsrvcache;

#define	NFSRCHASH(xid) \
	(&nfsrvhashtbl[((xid) + ((xid) >> 24)) & nfsrvhash])
static LIST_HEAD(nfsrvhash, nfsrvcache) *nfsrvhashtbl;
static TAILQ_HEAD(nfsrvlru, nfsrvcache) nfsrvlruhead;
static u_long nfsrvhash;
static eventhandler_tag nfsrv_nmbclusters_tag;

#define TRUE	1
#define	FALSE	0

#define	NETFAMILY(rp) \
		(((rp)->rc_flag & RC_NAM) ? (rp)->rc_nam->sa_family : AF_INET)

/*
 * Static array that defines which nfs rpc's are nonidempotent
 */
static const int nonidempotent[NFS_NPROCS] = {
	FALSE,
	FALSE,
	TRUE,
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
static const int nfsv2_repstat[NFS_NPROCS] = {
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
};

/* 
 * Size the NFS server's duplicate request cache at 1/2 the nmbclsters, floating 
 * within a (64, 2048) range. This is to prevent all mbuf clusters being tied up 
 * in the NFS dupreq cache for small values of nmbclusters. 
 */
static void
nfsrvcache_size_change(void *tag)
{
	desirednfsrvcache = nmbclusters /2;
	if (desirednfsrvcache > NFSRVCACHE_MAX_SIZE)
		desirednfsrvcache = NFSRVCACHE_MAX_SIZE;
	if (desirednfsrvcache < NFSRVCACHE_MIN_SIZE)
		desirednfsrvcache = NFSRVCACHE_MIN_SIZE;	
}

/*
 * Initialize the server request cache list
 */
void
nfsrv_initcache(void)
{
	nfsrvcache_size_change(NULL);
	nfsrvhashtbl = hashinit(desirednfsrvcache, M_NFSD, &nfsrvhash);
	TAILQ_INIT(&nfsrvlruhead);
	nfsrv_nmbclusters_tag = EVENTHANDLER_REGISTER(nmbclusters_change,
	    nfsrvcache_size_change, NULL, EVENTHANDLER_PRI_FIRST);
}

/*
 * Teardown the server request cache list
 */
void
nfsrv_destroycache(void)
{
	KASSERT(TAILQ_EMPTY(&nfsrvlruhead), ("%s: pending requests", __func__));
	EVENTHANDLER_DEREGISTER(nmbclusters_change, nfsrv_nmbclusters_tag);
	hashdestroy(nfsrvhashtbl, M_NFSD, nfsrvhash);
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
int
nfsrv_getcache(struct nfsrv_descript *nd, struct mbuf **repp)
{
	struct nfsrvcache *rp;
	struct mbuf *mb;
	struct sockaddr_in *saddr;
	caddr_t bpos;
	int ret;

	NFSD_LOCK_ASSERT();

	/*
	 * Don't cache recent requests for reliable transport protocols.
	 * (Maybe we should for the case of a reconnect, but..)
	 */
	if (!nd->nd_nam2)
		return (RC_DOIT);
loop:
	LIST_FOREACH(rp, NFSRCHASH(nd->nd_retxid), rc_hash) {
	    if (nd->nd_retxid == rp->rc_xid && nd->nd_procnum == rp->rc_proc &&
		netaddr_match(NETFAMILY(rp), &rp->rc_haddr, nd->nd_nam)) {
		        NFS_DPF(RC, ("H%03x", rp->rc_xid & 0xfff));
			if ((rp->rc_flag & RC_LOCKED) != 0) {
				rp->rc_flag |= RC_WANTED;
				(void) msleep(rp, &nfsd_mtx, PZERO-1,
				    "nfsrc", 0);
				goto loop;
			}
			rp->rc_flag |= RC_LOCKED;
			/* If not at end of LRU chain, move it there */
			if (TAILQ_NEXT(rp, rc_lru)) {
				TAILQ_REMOVE(&nfsrvlruhead, rp, rc_lru);
				TAILQ_INSERT_TAIL(&nfsrvlruhead, rp, rc_lru);
			}
			if (rp->rc_state == RC_UNUSED)
				panic("nfsrv cache");
			if (rp->rc_state == RC_INPROG) {
				nfsrvstats.srvcache_inproghits++;
				ret = RC_DROPIT;
			} else if (rp->rc_flag & RC_REPSTATUS) {
				nfsrvstats.srvcache_nonidemdonehits++;
				NFSD_UNLOCK();
				*repp = nfs_rephead(0, nd, rp->rc_status,
				    &mb, &bpos);
				ret = RC_REPLY;
				NFSD_LOCK();
			} else if (rp->rc_flag & RC_REPMBUF) {
				nfsrvstats.srvcache_nonidemdonehits++;
				NFSD_UNLOCK();
				*repp = m_copym(rp->rc_reply, 0, M_COPYALL,
						M_WAIT);
				NFSD_LOCK();
				ret = RC_REPLY;
			} else {
				nfsrvstats.srvcache_idemdonehits++;
				rp->rc_state = RC_INPROG;
				ret = RC_DOIT;
			}
			rp->rc_flag &= ~RC_LOCKED;
			if (rp->rc_flag & RC_WANTED) {
				rp->rc_flag &= ~RC_WANTED;
				wakeup(rp);
			}
			return (ret);
		}
	}
	nfsrvstats.srvcache_misses++;
	NFS_DPF(RC, ("M%03x", nd->nd_retxid & 0xfff));
	if (numnfsrvcache < desirednfsrvcache) {
		NFSD_UNLOCK();
		rp = (struct nfsrvcache *)malloc((u_long)sizeof *rp,
		    M_NFSD, M_WAITOK | M_ZERO);
		NFSD_LOCK();
		numnfsrvcache++;
		rp->rc_flag = RC_LOCKED;
	} else {
		rp = TAILQ_FIRST(&nfsrvlruhead);
		while ((rp->rc_flag & RC_LOCKED) != 0) {
			rp->rc_flag |= RC_WANTED;
			(void) msleep(rp, &nfsd_mtx, PZERO-1, "nfsrc", 0);
			rp = TAILQ_FIRST(&nfsrvlruhead);
		}
		rp->rc_flag |= RC_LOCKED;
		LIST_REMOVE(rp, rc_hash);
		TAILQ_REMOVE(&nfsrvlruhead, rp, rc_lru);
		if (rp->rc_flag & RC_REPMBUF)
			m_freem(rp->rc_reply);
		if (rp->rc_flag & RC_NAM)
			free(rp->rc_nam, M_SONAME);
		rp->rc_flag &= (RC_LOCKED | RC_WANTED);
	}
	TAILQ_INSERT_TAIL(&nfsrvlruhead, rp, rc_lru);
	rp->rc_state = RC_INPROG;
	rp->rc_xid = nd->nd_retxid;
	saddr = (struct sockaddr_in *)nd->nd_nam;
	switch (saddr->sin_family) {
	case AF_INET:
		rp->rc_flag |= RC_INETADDR;
		rp->rc_inetaddr = saddr->sin_addr.s_addr;
		break;
/*	case AF_INET6:	*/
/*	case AF_ISO:	*/
	default:
		/*
		 * XXXRW: Seems like we should only set RC_NAM if we
		 * actually manage to set rc_nam to something non-NULL.
		 */
		rp->rc_flag |= RC_NAM;
		rp->rc_nam = sodupsockaddr(nd->nd_nam, M_NOWAIT);
		break;
	};
	rp->rc_proc = nd->nd_procnum;
	LIST_INSERT_HEAD(NFSRCHASH(nd->nd_retxid), rp, rc_hash);
	rp->rc_flag &= ~RC_LOCKED;
	if (rp->rc_flag & RC_WANTED) {
		rp->rc_flag &= ~RC_WANTED;
		wakeup(rp);
	}
	return (RC_DOIT);
}

/*
 * Update a request cache entry after the rpc has been done
 */
void
nfsrv_updatecache(struct nfsrv_descript *nd, int repvalid, struct mbuf *repmbuf)
{
	struct nfsrvcache *rp;

	NFSD_LOCK_ASSERT();

	if (!nd->nd_nam2)
		return;
loop:
	LIST_FOREACH(rp, NFSRCHASH(nd->nd_retxid), rc_hash) {
	    if (nd->nd_retxid == rp->rc_xid && nd->nd_procnum == rp->rc_proc &&
		netaddr_match(NETFAMILY(rp), &rp->rc_haddr, nd->nd_nam)) {
			NFS_DPF(RC, ("U%03x", rp->rc_xid & 0xfff));
			if ((rp->rc_flag & RC_LOCKED) != 0) {
				rp->rc_flag |= RC_WANTED;
				(void) msleep(rp, &nfsd_mtx, PZERO-1,
				    "nfsrc", 0);
				goto loop;
			}
			rp->rc_flag |= RC_LOCKED;
			if (rp->rc_state == RC_DONE) {
				/*
				 * This can occur if the cache is too small.
				 * Retransmits of the same request aren't
				 * dropped so we may see the operation
				 * complete more then once.
				 */
				if (rp->rc_flag & RC_REPMBUF) {
					m_freem(rp->rc_reply);
					rp->rc_flag &= ~RC_REPMBUF;
				}
			}
			rp->rc_state = RC_DONE;
			/*
			 * If we have a valid reply update status and save
			 * the reply for non-idempotent rpc's.
			 */
			if (repvalid && nonidempotent[nd->nd_procnum]) {
				if ((nd->nd_flag & ND_NFSV3) == 0 &&
				    nfsv2_repstat[
				    nfsrvv2_procid[nd->nd_procnum]]) {
					rp->rc_status = nd->nd_repstat;
					rp->rc_flag |= RC_REPSTATUS;
				} else {
					NFSD_UNLOCK();
					rp->rc_reply = m_copym(repmbuf,
						0, M_COPYALL, M_WAIT);
					NFSD_LOCK();
					rp->rc_flag |= RC_REPMBUF;
				}
			}
			rp->rc_flag &= ~RC_LOCKED;
			if (rp->rc_flag & RC_WANTED) {
				rp->rc_flag &= ~RC_WANTED;
				wakeup(rp);
			}
			return;
		}
	}
	NFS_DPF(RC, ("L%03x", nd->nd_retxid & 0xfff));
}

/*
 * Clean out the cache. Called when the last nfsd terminates.
 */
void
nfsrv_cleancache(void)
{
	struct nfsrvcache *rp, *nextrp;

	NFSD_LOCK_ASSERT();

	TAILQ_FOREACH_SAFE(rp, &nfsrvlruhead, rc_lru, nextrp) {
		LIST_REMOVE(rp, rc_hash);
		TAILQ_REMOVE(&nfsrvlruhead, rp, rc_lru);
		if (rp->rc_flag & RC_REPMBUF)
			m_freem(rp->rc_reply);
		if (rp->rc_flag & RC_NAM)
			free(rp->rc_nam, M_SONAME);
		free(rp, M_NFSD);
	}
	numnfsrvcache = 0;
}
