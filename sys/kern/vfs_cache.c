/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1995
 *	Poul-Henning Kamp.  All rights reserved.
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
 *	@(#)vfs_cache.c	8.3 (Berkeley) 8/22/94
 * $Id: vfs_cache.c,v 1.17 1995/10/29 15:31:18 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/errno.h>
#include <sys/malloc.h>

/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where vp refers to the directory
 * containing name.
 *
 * If it is a "negative" entry, (that we know a name to >not< exist)
 * we point out entry at our own "nchENOENT", to avoid too much special
 * casing in the inner loops of lookup.
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
static LIST_HEAD(nchashhead, namecache) *nchashtbl;	/* Hash Table */
static TAILQ_HEAD(, namecache) nclruhead;	/* LRU chain */
static u_long	nchash;			/* size of hash table */
struct nchstats nchstats;		/* cache effectiveness statistics */
static struct vnode nchENOENT;		/* our own "novnode" */
static int doingcache = 1;		/* 1 => enable the cache */
SYSCTL_INT(_debug, OID_AUTO, vfscache, CTLFLAG_RW, &doingcache, 0, "");
u_long	nextvnodeid;
static u_long	numcache;
u_long	numvnodes;

#ifdef NCH_STATISTICS
u_long	nchnbr;
#define NCHNBR(ncp) (ncp)->nc_nbr = ++nchnbr;
#define NCHHIT(ncp) (ncp)->nc_hits++
#else
#define NCHNBR(ncp)
#define NCHHIT(ncp)
#endif

#define PURGE(ncp)  {						\
	LIST_REMOVE(ncp, nc_hash);				\
	ncp->nc_hash.le_prev = 0;				\
	TAILQ_REMOVE(&nclruhead, ncp, nc_lru);			\
	TAILQ_INSERT_HEAD(&nclruhead, ncp, nc_lru); }

#define TOUCH(ncp)  {						\
	if (ncp->nc_lru.tqe_next == 0) { } else {		\
		TAILQ_REMOVE(&nclruhead, ncp, nc_lru);		\
		TAILQ_INSERT_TAIL(&nclruhead, ncp, nc_lru);	\
		NCHNBR(ncp); } }

/*
 * Lookup an entry in the cache
 *
 * We don't do this if the segment name is long, simply so the cache
 * can avoid holding long names (which would either waste space, or
 * add greatly to the complexity).
 *
 * Lookup is called with dvp pointing to the directory to search,
 * cnp pointing to the name of the entry being sought.
 * If the lookup succeeds, the vnode is returned in *vpp, and a status
 * of -1 is returned.
 * If the lookup determines that the name does not exist (negative cacheing),
 * a status of ENOENT is returned.
 * If the lookup fails, a status of zero is returned.
 */

int
cache_lookup(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	register struct namecache *ncp,*nnp;
	register struct nchashhead *ncpp;

	if (!doingcache) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}

	if (cnp->cn_namelen > NCHNAMLEN) {
		nchstats.ncs_long++;
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}

	ncpp = &nchashtbl[(dvp->v_id + cnp->cn_hash) % nchash];
	for (ncp = ncpp->lh_first; ncp != 0; ncp = nnp) {
		nnp = ncp->nc_hash.le_next;
		/* If one of the vp's went stale, don't bother anymore. */
		if ((ncp->nc_dvpid != ncp->nc_dvp->v_id) ||
		    (ncp->nc_vpid  != ncp->nc_vp->v_id)) {
			nchstats.ncs_falsehits++;
			PURGE(ncp);
			continue;
		}
		/* Now that we know the vp's to be valid, is it ours ? */
		if (ncp->nc_dvp == dvp &&
		    ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(ncp->nc_name, cnp->cn_nameptr, (u_int)ncp->nc_nlen))
			goto found;	/* Fanatism considered bad. */
	}
	nchstats.ncs_miss++;
	return (0);

    found:
	NCHHIT(ncp);

	/* We don't want to have an entry, so dump it */
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		nchstats.ncs_badhits++;
		PURGE(ncp);
		return (0);
	}

	/* We found a "positive" match, return the vnode */
        if (ncp->nc_vp != &nchENOENT) {
		nchstats.ncs_goodhits++;
		TOUCH(ncp);
		*vpp = ncp->nc_vp;
		return (-1);
	}

	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE) {
		nchstats.ncs_badhits++;
		PURGE(ncp);
		return (0);
	}

	/* The name does not exists */
	nchstats.ncs_neghits++;
	TOUCH(ncp);
	return (ENOENT);
}

/*
 * Add an entry to the cache.
 */

void
cache_enter(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	register struct namecache *ncp;
	register struct nchashhead *ncpp;

	if (!doingcache)
		return;

	if (cnp->cn_namelen > NCHNAMLEN) {
		printf("cache_enter: name too long");
		return;
	}

	if (numcache < numvnodes) {
		/* Add one more entry */
		ncp = (struct namecache *)
			malloc((u_long)sizeof *ncp, M_CACHE, M_WAITOK);
		bzero((char *)ncp, sizeof *ncp);
		numcache++;
	} else if (ncp = nclruhead.tqh_first) {
		/* reuse an old entry */
		TAILQ_REMOVE(&nclruhead, ncp, nc_lru);
		if (ncp->nc_hash.le_prev != 0) {
			LIST_REMOVE(ncp, nc_hash);
			ncp->nc_hash.le_prev = 0;
		}
	} else {
		/* give up */
		return;
	}

	/* If vp is NULL this is a "negative" cache entry */
	if (!vp)
		vp = &nchENOENT;

	/* fill in cache info */
	ncp->nc_vp = vp;
	ncp->nc_vpid = vp->v_id;
	ncp->nc_dvp = dvp;
	ncp->nc_dvpid = dvp->v_id;
	ncp->nc_nlen = cnp->cn_namelen;
	bcopy(cnp->cn_nameptr, ncp->nc_name, (unsigned)ncp->nc_nlen);
	TAILQ_INSERT_TAIL(&nclruhead, ncp, nc_lru);
	ncpp = &nchashtbl[(dvp->v_id + cnp->cn_hash) % nchash];
	LIST_INSERT_HEAD(ncpp, ncp, nc_hash);
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */

void
nchinit()
{

	TAILQ_INIT(&nclruhead);
	nchashtbl = phashinit(desiredvnodes, M_CACHE, &nchash);
	nchENOENT.v_id = 1;
}

/*
 * Invalidate a all entries to particular vnode.
 *
 * We actually just increment the v_id, that will do it.  The entries will
 * be purged by lookup as they get found.
 * If the v_id wraps around, we need to ditch the entire cache, to avoid
 * confusion.
 * No valid vnode will ever have (v_id == 0).
 */

void
cache_purge(vp)
	struct vnode *vp;
{
	struct nchashhead *ncpp;

	vp->v_id = ++nextvnodeid;
	if (nextvnodeid != 0)
		return;
	for (ncpp = &nchashtbl[nchash - 1]; ncpp >= nchashtbl; ncpp--) {
		while(ncpp->lh_first)
			PURGE(ncpp->lh_first);
	}
	vp->v_id = ++nextvnodeid;
}

/*
 * Flush all entries referencing a particular filesystem.
 *
 * Since we need to check it anyway, we will flush all the invalid
 * entriess at the same time.
 *
 * If we purge anything, we scan the hash-bucket again.  There is only
 * a handful of entries, so it cheap and simple.
 */

void
cache_purgevfs(mp)
	struct mount *mp;
{
	struct nchashhead *ncpp;
	struct namecache *ncp;

	/* Scan hash tables for applicable entries */
	for (ncpp = &nchashtbl[nchash - 1]; ncpp >= nchashtbl; ncpp--) {
		ncp = ncpp->lh_first;
		while(ncp) {
			if (ncp->nc_dvpid != ncp->nc_dvp->v_id ||
			    ncp->nc_vpid != ncp->nc_vp->v_id ||
			    ncp->nc_dvp->v_mount == mp) {
				PURGE(ncp);
				ncp = ncpp->lh_first;
			} else {
				ncp = ncp->nc_hash.le_next;
			}
		}
	}
}
