/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vfs_cache.c	7.8 (Berkeley) 2/28/91
 *	$Id: vfs_cache.c,v 1.2 1993/10/16 15:25:19 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "time.h"
#include "mount.h"
#include "vnode.h"
#include "namei.h"
#include "errno.h"
#include "malloc.h"

/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where vp refers to the directory
 * containing name.
 *
 * For simplicity (and economy of storage), names longer than
 * a maximum length of NCHNAMLEN are not cached; they occur
 * infrequently in any case, and are almost never of interest.
 *
 * Upon reaching the last segment of a path, if the reference
 * is for DELETE, or NOCACHE is set (rewrite), and the
 * name is located in the cache, it will be dropped.
 */

/*
 * Structures associated with name cacheing.
 */
union nchash {
	union	nchash *nch_head[2];
	struct	namecache *nch_chain[2];
} *nchashtbl;
#define	nch_forw	nch_chain[0]
#define	nch_back	nch_chain[1]

u_long	nchash;				/* size of hash table - 1 */
long	numcache;			/* number of cache entries allocated */
struct	namecache *nchhead, **nchtail;	/* LRU chain pointers */
struct	nchstats nchstats;		/* cache effectiveness statistics */

int doingcache = 1;			/* 1 => enable the cache */

/*
 * Look for a the name in the cache. We don't do this
 * if the segment name is long, simply so the cache can avoid
 * holding long names (which would either waste space, or
 * add greatly to the complexity).
 *
 * Lookup is called with ni_dvp pointing to the directory to search,
 * ni_ptr pointing to the name of the entry being sought, ni_namelen
 * tells the length of the name, and ni_hash contains a hash of
 * the name. If the lookup succeeds, the vnode is returned in ni_vp
 * and a status of -1 is returned. If the lookup determines that
 * the name does not exist (negative cacheing), a status of ENOENT
 * is returned. If the lookup fails, a status of zero is returned.
 */
cache_lookup(ndp)
	register struct nameidata *ndp;
{
	register struct vnode *dvp;
	register struct namecache *ncp;
	union nchash *nhp;

	if (!doingcache)
		return (0);
	if (ndp->ni_namelen > NCHNAMLEN) {
		nchstats.ncs_long++;
		ndp->ni_makeentry = 0;
		return (0);
	}
	dvp = ndp->ni_dvp;
	nhp = &nchashtbl[ndp->ni_hash & nchash];
	for (ncp = nhp->nch_forw; ncp != (struct namecache *)nhp;
	    ncp = ncp->nc_forw) {
		if (ncp->nc_dvp == dvp &&
		    ncp->nc_dvpid == dvp->v_id &&
		    ncp->nc_nlen == ndp->ni_namelen &&
		    !bcmp(ncp->nc_name, ndp->ni_ptr, (unsigned)ncp->nc_nlen))
			break;
	}
	if (ncp == (struct namecache *)nhp) {
		nchstats.ncs_miss++;
		return (0);
	}
	if (!ndp->ni_makeentry) {
		nchstats.ncs_badhits++;
	} else if (ncp->nc_vp == NULL) {
		if ((ndp->ni_nameiop & OPMASK) != CREATE) {
			nchstats.ncs_neghits++;
			/*
			 * Move this slot to end of LRU chain,
			 * if not already there.
			 */
			if (ncp->nc_nxt) {
				/* remove from LRU chain */
				*ncp->nc_prev = ncp->nc_nxt;
				ncp->nc_nxt->nc_prev = ncp->nc_prev;
				/* and replace at end of it */
				ncp->nc_nxt = NULL;
				ncp->nc_prev = nchtail;
				*nchtail = ncp;
				nchtail = &ncp->nc_nxt;
			}
			return (ENOENT);
		}
	} else if (ncp->nc_vpid != ncp->nc_vp->v_id) {
		nchstats.ncs_falsehits++;
	} else {
		nchstats.ncs_goodhits++;
		/*
		 * move this slot to end of LRU chain, if not already there
		 */
		if (ncp->nc_nxt) {
			/* remove from LRU chain */
			*ncp->nc_prev = ncp->nc_nxt;
			ncp->nc_nxt->nc_prev = ncp->nc_prev;
			/* and replace at end of it */
			ncp->nc_nxt = NULL;
			ncp->nc_prev = nchtail;
			*nchtail = ncp;
			nchtail = &ncp->nc_nxt;
		}
		ndp->ni_vp = ncp->nc_vp;
		return (-1);
	}

	/*
	 * Last component and we are renaming or deleting,
	 * the cache entry is invalid, or otherwise don't
	 * want cache entry to exist.
	 */
	/* remove from LRU chain */
	*ncp->nc_prev = ncp->nc_nxt;
	if (ncp->nc_nxt)
		ncp->nc_nxt->nc_prev = ncp->nc_prev;
	else
		nchtail = ncp->nc_prev;
	/* remove from hash chain */
	remque(ncp);
	/* insert at head of LRU list (first to grab) */
	ncp->nc_nxt = nchhead;
	ncp->nc_prev = &nchhead;
	nchhead->nc_prev = &ncp->nc_nxt;
	nchhead = ncp;
	/* and make a dummy hash chain */
	ncp->nc_forw = ncp;
	ncp->nc_back = ncp;
	return (0);
}

/*
 * Add an entry to the cache
 */
cache_enter(ndp)
	register struct nameidata *ndp;
{
	register struct namecache *ncp;
	union nchash *nhp;

	if (!doingcache)
		return;
	/*
	 * Free the cache slot at head of lru chain.
	 */
	if (numcache < desiredvnodes) {
		ncp = (struct namecache *)
			malloc((u_long)sizeof *ncp, M_CACHE, M_WAITOK);
		bzero((char *)ncp, sizeof *ncp);
		numcache++;
	} else if (ncp = nchhead) {
		/* remove from lru chain */
		*ncp->nc_prev = ncp->nc_nxt;
		if (ncp->nc_nxt)
			ncp->nc_nxt->nc_prev = ncp->nc_prev;
		else
			nchtail = ncp->nc_prev;
		/* remove from old hash chain */
		remque(ncp);
	} else
		return;
	/* grab the vnode we just found */
	ncp->nc_vp = ndp->ni_vp;
	if (ndp->ni_vp)
		ncp->nc_vpid = ndp->ni_vp->v_id;
	else
		ncp->nc_vpid = 0;
	/* fill in cache info */
	ncp->nc_dvp = ndp->ni_dvp;
	ncp->nc_dvpid = ndp->ni_dvp->v_id;
	ncp->nc_nlen = ndp->ni_namelen;
	bcopy(ndp->ni_ptr, ncp->nc_name, (unsigned)ncp->nc_nlen);
	/* link at end of lru chain */
	ncp->nc_nxt = NULL;
	ncp->nc_prev = nchtail;
	*nchtail = ncp;
	nchtail = &ncp->nc_nxt;
	/* and insert on hash chain */
	nhp = &nchashtbl[ndp->ni_hash & nchash];
	insque(ncp, nhp);
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
nchinit()
{
	register union nchash *nchp;
	long nchashsize;

	nchhead = 0;
	nchtail = &nchhead;
	nchashsize = roundup((desiredvnodes + 1) * sizeof *nchp / 2,
		NBPG * CLSIZE);
	nchashtbl = (union nchash *)malloc((u_long)nchashsize,
	    M_CACHE, M_WAITOK);
	for (nchash = 1; nchash <= nchashsize / sizeof *nchp; nchash <<= 1)
		/* void */;
	nchash = (nchash >> 1) - 1;
	for (nchp = &nchashtbl[nchash]; nchp >= nchashtbl; nchp--) {
		nchp->nch_head[0] = nchp;
		nchp->nch_head[1] = nchp;
	}
}

/*
 * Cache flush, a particular vnode; called when a vnode is renamed to
 * hide entries that would now be invalid
 */
cache_purge(vp)
	struct vnode *vp;
{
	union nchash *nhp;
	struct namecache *ncp;

	vp->v_id = ++nextvnodeid;
	if (nextvnodeid != 0)
		return;
	for (nhp = &nchashtbl[nchash]; nhp >= nchashtbl; nhp--) {
		for (ncp = nhp->nch_forw; ncp != (struct namecache *)nhp;
		    ncp = ncp->nc_forw) {
			ncp->nc_vpid = 0;
			ncp->nc_dvpid = 0;
		}
	}
	vp->v_id = ++nextvnodeid;
}

/*
 * Cache flush, a whole filesystem; called when filesys is umounted to
 * remove entries that would now be invalid
 *
 * The line "nxtcp = nchhead" near the end is to avoid potential problems
 * if the cache lru chain is modified while we are dumping the
 * inode.  This makes the algorithm O(n^2), but do you think I care?
 */
cache_purgevfs(mp)
	struct mount *mp;
{
	register struct namecache *ncp, *nxtcp;

	for (ncp = nchhead; ncp; ncp = nxtcp) {
		nxtcp = ncp->nc_nxt;
		if (ncp->nc_dvp == NULL || ncp->nc_dvp->v_mount != mp)
			continue;
		/* free the resources we had */
		ncp->nc_vp = NULL;
		ncp->nc_dvp = NULL;
		remque(ncp);		/* remove entry from its hash chain */
		ncp->nc_forw = ncp;	/* and make a dummy one */
		ncp->nc_back = ncp;
		/* delete this entry from LRU chain */
		*ncp->nc_prev = nxtcp;
		if (nxtcp)
			nxtcp->nc_prev = ncp->nc_prev;
		else
			nchtail = ncp->nc_prev;
		/* cause rescan of list, it may have altered */
		nxtcp = nchhead;
		/* put the now-free entry at head of LRU */
		ncp->nc_nxt = nxtcp;
		ncp->nc_prev = &nchhead;
		nxtcp->nc_prev = &ncp->nc_nxt;
		nchhead = ncp;
	}
}
