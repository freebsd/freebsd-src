/*-
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$OpenBSD: pf_norm.c,v 1.114 2009/01/29 14:11:45 henning Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_pf.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/vnet.h>
#include <net/pfvar.h>
#include <net/pf_mtag.h>
#include <net/if_pflog.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

struct pf_frent {
	LIST_ENTRY(pf_frent) fr_next;
	union {
		struct {
			struct ip *_fr_ip;
			struct mbuf *_fr_m;
		} _frag;
		struct {
			uint16_t _fr_off;
			uint16_t _fr_end;
		} _cache;
	} _u;
};
#define	fr_ip	_u._frag._fr_ip
#define	fr_m	_u._frag._fr_m
#define	fr_off	_u._cache._fr_off
#define	fr_end	_u._cache._fr_end

struct pf_fragment {
	RB_ENTRY(pf_fragment) fr_entry;
	TAILQ_ENTRY(pf_fragment) frag_next;
	struct in_addr	fr_src;
	struct in_addr	fr_dst;
	u_int8_t	fr_p;		/* protocol of this fragment */
	u_int8_t	fr_flags;	/* status flags */
#define PFFRAG_SEENLAST	0x0001		/* Seen the last fragment for this */
#define PFFRAG_NOBUFFER	0x0002		/* Non-buffering fragment cache */
#define PFFRAG_DROP	0x0004		/* Drop all fragments */
#define BUFFER_FRAGMENTS(fr)	(!((fr)->fr_flags & PFFRAG_NOBUFFER))
	u_int16_t	fr_id;		/* fragment id for reassemble */
	u_int16_t	fr_max;		/* fragment data max */
	u_int32_t	fr_timeout;
	LIST_HEAD(, pf_frent) fr_queue;
};

static struct mtx pf_frag_mtx;
#define PF_FRAG_LOCK()		mtx_lock(&pf_frag_mtx)
#define PF_FRAG_UNLOCK()	mtx_unlock(&pf_frag_mtx)
#define PF_FRAG_ASSERT()	mtx_assert(&pf_frag_mtx, MA_OWNED)

VNET_DEFINE(uma_zone_t, pf_state_scrub_z);	/* XXX: shared with pfsync */

static VNET_DEFINE(uma_zone_t, pf_frent_z);
#define	V_pf_frent_z	VNET(pf_frent_z)
static VNET_DEFINE(uma_zone_t, pf_frag_z);
#define	V_pf_frag_z	VNET(pf_frag_z)

TAILQ_HEAD(pf_fragqueue, pf_fragment);
TAILQ_HEAD(pf_cachequeue, pf_fragment);
static VNET_DEFINE(struct pf_fragqueue,	pf_fragqueue);
#define	V_pf_fragqueue			VNET(pf_fragqueue)
static VNET_DEFINE(struct pf_cachequeue,	pf_cachequeue);
#define	V_pf_cachequeue			VNET(pf_cachequeue)
RB_HEAD(pf_frag_tree, pf_fragment);
static VNET_DEFINE(struct pf_frag_tree,	pf_frag_tree);
#define	V_pf_frag_tree			VNET(pf_frag_tree)
static VNET_DEFINE(struct pf_frag_tree,	pf_cache_tree);
#define	V_pf_cache_tree			VNET(pf_cache_tree)
static int		 pf_frag_compare(struct pf_fragment *,
			    struct pf_fragment *);
static RB_PROTOTYPE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);
static RB_GENERATE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);

/* Private prototypes */
static void		 pf_free_fragment(struct pf_fragment *);
static void		 pf_remove_fragment(struct pf_fragment *);
static int		 pf_normalize_tcpopt(struct pf_rule *, struct mbuf *,
			    struct tcphdr *, int, sa_family_t);
#ifdef INET
static void		 pf_ip2key(struct pf_fragment *, struct ip *);
static void		 pf_scrub_ip(struct mbuf **, u_int32_t, u_int8_t,
			    u_int8_t);
static void		 pf_flush_fragments(void);
static struct pf_fragment *pf_find_fragment(struct ip *, struct pf_frag_tree *);
static struct mbuf	*pf_reassemble(struct mbuf **, struct pf_fragment **,
			    struct pf_frent *, int);
static struct mbuf	*pf_fragcache(struct mbuf **, struct ip*,
			    struct pf_fragment **, int, int, int *);
#endif /* INET */
#ifdef INET6
static void		 pf_scrub_ip6(struct mbuf **, u_int8_t);
#endif
#define	DPFPRINTF(x) do {				\
	if (V_pf_status.debug >= PF_DEBUG_MISC) {	\
		printf("%s: ", __func__);		\
		printf x ;				\
	}						\
} while(0)

void
pf_normalize_init(void)
{

	V_pf_frag_z = uma_zcreate("pf frags", sizeof(struct pf_fragment),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	V_pf_frent_z = uma_zcreate("pf frag entries", sizeof(struct pf_frent),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	V_pf_state_scrub_z = uma_zcreate("pf state scrubs",
	    sizeof(struct pf_state_scrub),  NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	V_pf_limits[PF_LIMIT_FRAGS].zone = V_pf_frent_z;
	V_pf_limits[PF_LIMIT_FRAGS].limit = PFFRAG_FRENT_HIWAT;
	uma_zone_set_max(V_pf_frent_z, PFFRAG_FRENT_HIWAT);
	uma_zone_set_warning(V_pf_frent_z, "PF frag entries limit reached");

	mtx_init(&pf_frag_mtx, "pf fragments", NULL, MTX_DEF);

	TAILQ_INIT(&V_pf_fragqueue);
	TAILQ_INIT(&V_pf_cachequeue);
}

void
pf_normalize_cleanup(void)
{

	uma_zdestroy(V_pf_state_scrub_z);
	uma_zdestroy(V_pf_frent_z);
	uma_zdestroy(V_pf_frag_z);

	mtx_destroy(&pf_frag_mtx);
}

static int
pf_frag_compare(struct pf_fragment *a, struct pf_fragment *b)
{
	int	diff;

	if ((diff = a->fr_id - b->fr_id))
		return (diff);
	else if ((diff = a->fr_p - b->fr_p))
		return (diff);
	else if (a->fr_src.s_addr < b->fr_src.s_addr)
		return (-1);
	else if (a->fr_src.s_addr > b->fr_src.s_addr)
		return (1);
	else if (a->fr_dst.s_addr < b->fr_dst.s_addr)
		return (-1);
	else if (a->fr_dst.s_addr > b->fr_dst.s_addr)
		return (1);
	return (0);
}

void
pf_purge_expired_fragments(void)
{
	struct pf_fragment	*frag;
	u_int32_t		 expire = time_uptime -
				    V_pf_default_rule.timeout[PFTM_FRAG];

	PF_FRAG_LOCK();
	while ((frag = TAILQ_LAST(&V_pf_fragqueue, pf_fragqueue)) != NULL) {
		KASSERT((BUFFER_FRAGMENTS(frag)),
		    ("BUFFER_FRAGMENTS(frag) == 0: %s", __FUNCTION__));
		if (frag->fr_timeout > expire)
			break;

		DPFPRINTF(("expiring %d(%p)\n", frag->fr_id, frag));
		pf_free_fragment(frag);
	}

	while ((frag = TAILQ_LAST(&V_pf_cachequeue, pf_cachequeue)) != NULL) {
		KASSERT((!BUFFER_FRAGMENTS(frag)),
		    ("BUFFER_FRAGMENTS(frag) != 0: %s", __FUNCTION__));
		if (frag->fr_timeout > expire)
			break;

		DPFPRINTF(("expiring %d(%p)\n", frag->fr_id, frag));
		pf_free_fragment(frag);
		KASSERT((TAILQ_EMPTY(&V_pf_cachequeue) ||
		    TAILQ_LAST(&V_pf_cachequeue, pf_cachequeue) != frag),
		    ("!(TAILQ_EMPTY() || TAILQ_LAST() == farg): %s",
		    __FUNCTION__));
	}
	PF_FRAG_UNLOCK();
}

#ifdef INET
/*
 * Try to flush old fragments to make space for new ones
 */
static void
pf_flush_fragments(void)
{
	struct pf_fragment	*frag, *cache;
	int			 goal;

	PF_FRAG_ASSERT();

	goal = uma_zone_get_cur(V_pf_frent_z) * 9 / 10;
	DPFPRINTF(("trying to free %d frag entriess\n", goal));
	while (goal < uma_zone_get_cur(V_pf_frent_z)) {
		frag = TAILQ_LAST(&V_pf_fragqueue, pf_fragqueue);
		if (frag)
			pf_free_fragment(frag);
		cache = TAILQ_LAST(&V_pf_cachequeue, pf_cachequeue);
		if (cache)
			pf_free_fragment(cache);
		if (frag == NULL && cache == NULL)
			break;
	}
}
#endif /* INET */

/* Frees the fragments and all associated entries */
static void
pf_free_fragment(struct pf_fragment *frag)
{
	struct pf_frent		*frent;

	PF_FRAG_ASSERT();

	/* Free all fragments */
	if (BUFFER_FRAGMENTS(frag)) {
		for (frent = LIST_FIRST(&frag->fr_queue); frent;
		    frent = LIST_FIRST(&frag->fr_queue)) {
			LIST_REMOVE(frent, fr_next);

			m_freem(frent->fr_m);
			uma_zfree(V_pf_frent_z, frent);
		}
	} else {
		for (frent = LIST_FIRST(&frag->fr_queue); frent;
		    frent = LIST_FIRST(&frag->fr_queue)) {
			LIST_REMOVE(frent, fr_next);

			KASSERT((LIST_EMPTY(&frag->fr_queue) ||
			    LIST_FIRST(&frag->fr_queue)->fr_off >
			    frent->fr_end),
			    ("! (LIST_EMPTY() || LIST_FIRST()->fr_off >"
			    " frent->fr_end): %s", __func__));

			uma_zfree(V_pf_frent_z, frent);
		}
	}

	pf_remove_fragment(frag);
}

#ifdef INET
static void
pf_ip2key(struct pf_fragment *key, struct ip *ip)
{
	key->fr_p = ip->ip_p;
	key->fr_id = ip->ip_id;
	key->fr_src.s_addr = ip->ip_src.s_addr;
	key->fr_dst.s_addr = ip->ip_dst.s_addr;
}

static struct pf_fragment *
pf_find_fragment(struct ip *ip, struct pf_frag_tree *tree)
{
	struct pf_fragment	 key;
	struct pf_fragment	*frag;

	PF_FRAG_ASSERT();

	pf_ip2key(&key, ip);

	frag = RB_FIND(pf_frag_tree, tree, &key);
	if (frag != NULL) {
		/* XXX Are we sure we want to update the timeout? */
		frag->fr_timeout = time_uptime;
		if (BUFFER_FRAGMENTS(frag)) {
			TAILQ_REMOVE(&V_pf_fragqueue, frag, frag_next);
			TAILQ_INSERT_HEAD(&V_pf_fragqueue, frag, frag_next);
		} else {
			TAILQ_REMOVE(&V_pf_cachequeue, frag, frag_next);
			TAILQ_INSERT_HEAD(&V_pf_cachequeue, frag, frag_next);
		}
	}

	return (frag);
}
#endif /* INET */

/* Removes a fragment from the fragment queue and frees the fragment */

static void
pf_remove_fragment(struct pf_fragment *frag)
{

	PF_FRAG_ASSERT();

	if (BUFFER_FRAGMENTS(frag)) {
		RB_REMOVE(pf_frag_tree, &V_pf_frag_tree, frag);
		TAILQ_REMOVE(&V_pf_fragqueue, frag, frag_next);
		uma_zfree(V_pf_frag_z, frag);
	} else {
		RB_REMOVE(pf_frag_tree, &V_pf_cache_tree, frag);
		TAILQ_REMOVE(&V_pf_cachequeue, frag, frag_next);
		uma_zfree(V_pf_frag_z, frag);
	}
}

#ifdef INET
#define FR_IP_OFF(fr)	((ntohs((fr)->fr_ip->ip_off) & IP_OFFMASK) << 3)
static struct mbuf *
pf_reassemble(struct mbuf **m0, struct pf_fragment **frag,
    struct pf_frent *frent, int mff)
{
	struct mbuf	*m = *m0, *m2;
	struct pf_frent	*frea, *next;
	struct pf_frent	*frep = NULL;
	struct ip	*ip = frent->fr_ip;
	int		 hlen = ip->ip_hl << 2;
	u_int16_t	 off = (ntohs(ip->ip_off) & IP_OFFMASK) << 3;
	u_int16_t	 ip_len = ntohs(ip->ip_len) - ip->ip_hl * 4;
	u_int16_t	 max = ip_len + off;

	PF_FRAG_ASSERT();
	KASSERT((*frag == NULL || BUFFER_FRAGMENTS(*frag)),
	    ("! (*frag == NULL || BUFFER_FRAGMENTS(*frag)): %s", __FUNCTION__));

	/* Strip off ip header */
	m->m_data += hlen;
	m->m_len -= hlen;

	/* Create a new reassembly queue for this packet */
	if (*frag == NULL) {
		*frag = uma_zalloc(V_pf_frag_z, M_NOWAIT);
		if (*frag == NULL) {
			pf_flush_fragments();
			*frag = uma_zalloc(V_pf_frag_z, M_NOWAIT);
			if (*frag == NULL)
				goto drop_fragment;
		}

		(*frag)->fr_flags = 0;
		(*frag)->fr_max = 0;
		(*frag)->fr_src = frent->fr_ip->ip_src;
		(*frag)->fr_dst = frent->fr_ip->ip_dst;
		(*frag)->fr_p = frent->fr_ip->ip_p;
		(*frag)->fr_id = frent->fr_ip->ip_id;
		(*frag)->fr_timeout = time_uptime;
		LIST_INIT(&(*frag)->fr_queue);

		RB_INSERT(pf_frag_tree, &V_pf_frag_tree, *frag);
		TAILQ_INSERT_HEAD(&V_pf_fragqueue, *frag, frag_next);

		/* We do not have a previous fragment */
		frep = NULL;
		goto insert;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	LIST_FOREACH(frea, &(*frag)->fr_queue, fr_next) {
		if (FR_IP_OFF(frea) > off)
			break;
		frep = frea;
	}

	KASSERT((frep != NULL || frea != NULL),
	    ("!(frep != NULL || frea != NULL): %s", __FUNCTION__));;

	if (frep != NULL &&
	    FR_IP_OFF(frep) + ntohs(frep->fr_ip->ip_len) - frep->fr_ip->ip_hl *
	    4 > off)
	{
		u_int16_t	precut;

		precut = FR_IP_OFF(frep) + ntohs(frep->fr_ip->ip_len) -
		    frep->fr_ip->ip_hl * 4 - off;
		if (precut >= ip_len)
			goto drop_fragment;
		m_adj(frent->fr_m, precut);
		DPFPRINTF(("overlap -%d\n", precut));
		/* Enforce 8 byte boundaries */
		ip->ip_off = htons(ntohs(ip->ip_off) + (precut >> 3));
		off = (ntohs(ip->ip_off) & IP_OFFMASK) << 3;
		ip_len -= precut;
		ip->ip_len = htons(ip_len);
	}

	for (; frea != NULL && ip_len + off > FR_IP_OFF(frea);
	    frea = next)
	{
		u_int16_t	aftercut;

		aftercut = ip_len + off - FR_IP_OFF(frea);
		DPFPRINTF(("adjust overlap %d\n", aftercut));
		if (aftercut < ntohs(frea->fr_ip->ip_len) - frea->fr_ip->ip_hl
		    * 4)
		{
			frea->fr_ip->ip_len =
			    htons(ntohs(frea->fr_ip->ip_len) - aftercut);
			frea->fr_ip->ip_off = htons(ntohs(frea->fr_ip->ip_off) +
			    (aftercut >> 3));
			m_adj(frea->fr_m, aftercut);
			break;
		}

		/* This fragment is completely overlapped, lose it */
		next = LIST_NEXT(frea, fr_next);
		m_freem(frea->fr_m);
		LIST_REMOVE(frea, fr_next);
		uma_zfree(V_pf_frent_z, frea);
	}

 insert:
	/* Update maximum data size */
	if ((*frag)->fr_max < max)
		(*frag)->fr_max = max;
	/* This is the last segment */
	if (!mff)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	if (frep == NULL)
		LIST_INSERT_HEAD(&(*frag)->fr_queue, frent, fr_next);
	else
		LIST_INSERT_AFTER(frep, frent, fr_next);

	/* Check if we are completely reassembled */
	if (!((*frag)->fr_flags & PFFRAG_SEENLAST))
		return (NULL);

	/* Check if we have all the data */
	off = 0;
	for (frep = LIST_FIRST(&(*frag)->fr_queue); frep; frep = next) {
		next = LIST_NEXT(frep, fr_next);

		off += ntohs(frep->fr_ip->ip_len) - frep->fr_ip->ip_hl * 4;
		if (off < (*frag)->fr_max &&
		    (next == NULL || FR_IP_OFF(next) != off))
		{
			DPFPRINTF(("missing fragment at %d, next %d, max %d\n",
			    off, next == NULL ? -1 : FR_IP_OFF(next),
			    (*frag)->fr_max));
			return (NULL);
		}
	}
	DPFPRINTF(("%d < %d?\n", off, (*frag)->fr_max));
	if (off < (*frag)->fr_max)
		return (NULL);

	/* We have all the data */
	frent = LIST_FIRST(&(*frag)->fr_queue);
	KASSERT((frent != NULL), ("frent == NULL: %s", __FUNCTION__));
	if ((frent->fr_ip->ip_hl << 2) + off > IP_MAXPACKET) {
		DPFPRINTF(("drop: too big: %d\n", off));
		pf_free_fragment(*frag);
		*frag = NULL;
		return (NULL);
	}
	next = LIST_NEXT(frent, fr_next);

	/* Magic from ip_input */
	ip = frent->fr_ip;
	m = frent->fr_m;
	m2 = m->m_next;
	m->m_next = NULL;
	m_cat(m, m2);
	uma_zfree(V_pf_frent_z, frent);
	for (frent = next; frent != NULL; frent = next) {
		next = LIST_NEXT(frent, fr_next);

		m2 = frent->fr_m;
		uma_zfree(V_pf_frent_z, frent);
		m->m_pkthdr.csum_flags &= m2->m_pkthdr.csum_flags;
		m->m_pkthdr.csum_data += m2->m_pkthdr.csum_data;
		m_cat(m, m2);
	}

	while (m->m_pkthdr.csum_data & 0xffff0000)
		m->m_pkthdr.csum_data = (m->m_pkthdr.csum_data & 0xffff) +
		    (m->m_pkthdr.csum_data >> 16);
	ip->ip_src = (*frag)->fr_src;
	ip->ip_dst = (*frag)->fr_dst;

	/* Remove from fragment queue */
	pf_remove_fragment(*frag);
	*frag = NULL;

	hlen = ip->ip_hl << 2;
	ip->ip_len = htons(off + hlen);
	m->m_len += hlen;
	m->m_data -= hlen;

	/* some debugging cruft by sklower, below, will go away soon */
	/* XXX this should be done elsewhere */
	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (m2 = m; m2; m2 = m2->m_next)
			plen += m2->m_len;
		m->m_pkthdr.len = plen;
	}

	DPFPRINTF(("complete: %p(%d)\n", m, ntohs(ip->ip_len)));
	return (m);

 drop_fragment:
	/* Oops - fail safe - drop packet */
	uma_zfree(V_pf_frent_z, frent);
	m_freem(m);
	return (NULL);
}

static struct mbuf *
pf_fragcache(struct mbuf **m0, struct ip *h, struct pf_fragment **frag, int mff,
    int drop, int *nomem)
{
	struct mbuf		*m = *m0;
	struct pf_frent		*frp, *fra, *cur = NULL;
	int			 ip_len = ntohs(h->ip_len) - (h->ip_hl << 2);
	u_int16_t		 off = ntohs(h->ip_off) << 3;
	u_int16_t		 max = ip_len + off;
	int			 hosed = 0;

	PF_FRAG_ASSERT();
	KASSERT((*frag == NULL || !BUFFER_FRAGMENTS(*frag)),
	    ("!(*frag == NULL || !BUFFER_FRAGMENTS(*frag)): %s", __FUNCTION__));

	/* Create a new range queue for this packet */
	if (*frag == NULL) {
		*frag = uma_zalloc(V_pf_frag_z, M_NOWAIT);
		if (*frag == NULL) {
			pf_flush_fragments();
			*frag = uma_zalloc(V_pf_frag_z, M_NOWAIT);
			if (*frag == NULL)
				goto no_mem;
		}

		/* Get an entry for the queue */
		cur = uma_zalloc(V_pf_frent_z, M_NOWAIT);
		if (cur == NULL) {
			uma_zfree(V_pf_frag_z, *frag);
			*frag = NULL;
			goto no_mem;
		}

		(*frag)->fr_flags = PFFRAG_NOBUFFER;
		(*frag)->fr_max = 0;
		(*frag)->fr_src = h->ip_src;
		(*frag)->fr_dst = h->ip_dst;
		(*frag)->fr_p = h->ip_p;
		(*frag)->fr_id = h->ip_id;
		(*frag)->fr_timeout = time_uptime;

		cur->fr_off = off;
		cur->fr_end = max;
		LIST_INIT(&(*frag)->fr_queue);
		LIST_INSERT_HEAD(&(*frag)->fr_queue, cur, fr_next);

		RB_INSERT(pf_frag_tree, &V_pf_cache_tree, *frag);
		TAILQ_INSERT_HEAD(&V_pf_cachequeue, *frag, frag_next);

		DPFPRINTF(("fragcache[%d]: new %d-%d\n", h->ip_id, off, max));

		goto pass;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	frp = NULL;
	LIST_FOREACH(fra, &(*frag)->fr_queue, fr_next) {
		if (fra->fr_off > off)
			break;
		frp = fra;
	}

	KASSERT((frp != NULL || fra != NULL),
	    ("!(frp != NULL || fra != NULL): %s", __FUNCTION__));

	if (frp != NULL) {
		int	precut;

		precut = frp->fr_end - off;
		if (precut >= ip_len) {
			/* Fragment is entirely a duplicate */
			DPFPRINTF(("fragcache[%d]: dead (%d-%d) %d-%d\n",
			    h->ip_id, frp->fr_off, frp->fr_end, off, max));
			goto drop_fragment;
		}
		if (precut == 0) {
			/* They are adjacent.  Fixup cache entry */
			DPFPRINTF(("fragcache[%d]: adjacent (%d-%d) %d-%d\n",
			    h->ip_id, frp->fr_off, frp->fr_end, off, max));
			frp->fr_end = max;
		} else if (precut > 0) {
			/* The first part of this payload overlaps with a
			 * fragment that has already been passed.
			 * Need to trim off the first part of the payload.
			 * But to do so easily, we need to create another
			 * mbuf to throw the original header into.
			 */

			DPFPRINTF(("fragcache[%d]: chop %d (%d-%d) %d-%d\n",
			    h->ip_id, precut, frp->fr_off, frp->fr_end, off,
			    max));

			off += precut;
			max -= precut;
			/* Update the previous frag to encompass this one */
			frp->fr_end = max;

			if (!drop) {
				/* XXX Optimization opportunity
				 * This is a very heavy way to trim the payload.
				 * we could do it much faster by diddling mbuf
				 * internals but that would be even less legible
				 * than this mbuf magic.  For my next trick,
				 * I'll pull a rabbit out of my laptop.
				 */
				*m0 = m_dup(m, M_NOWAIT);
				if (*m0 == NULL)
					goto no_mem;
				/* From KAME Project : We have missed this! */
				m_adj(*m0, (h->ip_hl << 2) -
				    (*m0)->m_pkthdr.len);

				KASSERT(((*m0)->m_next == NULL),
				    ("(*m0)->m_next != NULL: %s",
				    __FUNCTION__));
				m_adj(m, precut + (h->ip_hl << 2));
				m_cat(*m0, m);
				m = *m0;
				if (m->m_flags & M_PKTHDR) {
					int plen = 0;
					struct mbuf *t;
					for (t = m; t; t = t->m_next)
						plen += t->m_len;
					m->m_pkthdr.len = plen;
				}


				h = mtod(m, struct ip *);

				KASSERT(((int)m->m_len ==
				    ntohs(h->ip_len) - precut),
				    ("m->m_len != ntohs(h->ip_len) - precut: %s",
				    __FUNCTION__));
				h->ip_off = htons(ntohs(h->ip_off) +
				    (precut >> 3));
				h->ip_len = htons(ntohs(h->ip_len) - precut);
			} else {
				hosed++;
			}
		} else {
			/* There is a gap between fragments */

			DPFPRINTF(("fragcache[%d]: gap %d (%d-%d) %d-%d\n",
			    h->ip_id, -precut, frp->fr_off, frp->fr_end, off,
			    max));

			cur = uma_zalloc(V_pf_frent_z, M_NOWAIT);
			if (cur == NULL)
				goto no_mem;

			cur->fr_off = off;
			cur->fr_end = max;
			LIST_INSERT_AFTER(frp, cur, fr_next);
		}
	}

	if (fra != NULL) {
		int	aftercut;
		int	merge = 0;

		aftercut = max - fra->fr_off;
		if (aftercut == 0) {
			/* Adjacent fragments */
			DPFPRINTF(("fragcache[%d]: adjacent %d-%d (%d-%d)\n",
			    h->ip_id, off, max, fra->fr_off, fra->fr_end));
			fra->fr_off = off;
			merge = 1;
		} else if (aftercut > 0) {
			/* Need to chop off the tail of this fragment */
			DPFPRINTF(("fragcache[%d]: chop %d %d-%d (%d-%d)\n",
			    h->ip_id, aftercut, off, max, fra->fr_off,
			    fra->fr_end));
			fra->fr_off = off;
			max -= aftercut;

			merge = 1;

			if (!drop) {
				m_adj(m, -aftercut);
				if (m->m_flags & M_PKTHDR) {
					int plen = 0;
					struct mbuf *t;
					for (t = m; t; t = t->m_next)
						plen += t->m_len;
					m->m_pkthdr.len = plen;
				}
				h = mtod(m, struct ip *);
				KASSERT(((int)m->m_len == ntohs(h->ip_len) - aftercut),
				    ("m->m_len != ntohs(h->ip_len) - aftercut: %s",
				    __FUNCTION__));
				h->ip_len = htons(ntohs(h->ip_len) - aftercut);
			} else {
				hosed++;
			}
		} else if (frp == NULL) {
			/* There is a gap between fragments */
			DPFPRINTF(("fragcache[%d]: gap %d %d-%d (%d-%d)\n",
			    h->ip_id, -aftercut, off, max, fra->fr_off,
			    fra->fr_end));

			cur = uma_zalloc(V_pf_frent_z, M_NOWAIT);
			if (cur == NULL)
				goto no_mem;

			cur->fr_off = off;
			cur->fr_end = max;
			LIST_INSERT_BEFORE(fra, cur, fr_next);
		}


		/* Need to glue together two separate fragment descriptors */
		if (merge) {
			if (cur && fra->fr_off <= cur->fr_end) {
				/* Need to merge in a previous 'cur' */
				DPFPRINTF(("fragcache[%d]: adjacent(merge "
				    "%d-%d) %d-%d (%d-%d)\n",
				    h->ip_id, cur->fr_off, cur->fr_end, off,
				    max, fra->fr_off, fra->fr_end));
				fra->fr_off = cur->fr_off;
				LIST_REMOVE(cur, fr_next);
				uma_zfree(V_pf_frent_z, cur);
				cur = NULL;

			} else if (frp && fra->fr_off <= frp->fr_end) {
				/* Need to merge in a modified 'frp' */
				KASSERT((cur == NULL), ("cur != NULL: %s",
				    __FUNCTION__));
				DPFPRINTF(("fragcache[%d]: adjacent(merge "
				    "%d-%d) %d-%d (%d-%d)\n",
				    h->ip_id, frp->fr_off, frp->fr_end, off,
				    max, fra->fr_off, fra->fr_end));
				fra->fr_off = frp->fr_off;
				LIST_REMOVE(frp, fr_next);
				uma_zfree(V_pf_frent_z, frp);
				frp = NULL;

			}
		}
	}

	if (hosed) {
		/*
		 * We must keep tracking the overall fragment even when
		 * we're going to drop it anyway so that we know when to
		 * free the overall descriptor.  Thus we drop the frag late.
		 */
		goto drop_fragment;
	}


 pass:
	/* Update maximum data size */
	if ((*frag)->fr_max < max)
		(*frag)->fr_max = max;

	/* This is the last segment */
	if (!mff)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	/* Check if we are completely reassembled */
	if (((*frag)->fr_flags & PFFRAG_SEENLAST) &&
	    LIST_FIRST(&(*frag)->fr_queue)->fr_off == 0 &&
	    LIST_FIRST(&(*frag)->fr_queue)->fr_end == (*frag)->fr_max) {
		/* Remove from fragment queue */
		DPFPRINTF(("fragcache[%d]: done 0-%d\n", h->ip_id,
		    (*frag)->fr_max));
		pf_free_fragment(*frag);
		*frag = NULL;
	}

	return (m);

 no_mem:
	*nomem = 1;

	/* Still need to pay attention to !IP_MF */
	if (!mff && *frag != NULL)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	m_freem(m);
	return (NULL);

 drop_fragment:

	/* Still need to pay attention to !IP_MF */
	if (!mff && *frag != NULL)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	if (drop) {
		/* This fragment has been deemed bad.  Don't reass */
		if (((*frag)->fr_flags & PFFRAG_DROP) == 0)
			DPFPRINTF(("fragcache[%d]: dropping overall fragment\n",
			    h->ip_id));
		(*frag)->fr_flags |= PFFRAG_DROP;
	}

	m_freem(m);
	return (NULL);
}

int
pf_normalize_ip(struct mbuf **m0, int dir, struct pfi_kif *kif, u_short *reason,
    struct pf_pdesc *pd)
{
	struct mbuf		*m = *m0;
	struct pf_rule		*r;
	struct pf_frent		*frent;
	struct pf_fragment	*frag = NULL;
	struct ip		*h = mtod(m, struct ip *);
	int			 mff = (ntohs(h->ip_off) & IP_MF);
	int			 hlen = h->ip_hl << 2;
	u_int16_t		 fragoff = (ntohs(h->ip_off) & IP_OFFMASK) << 3;
	u_int16_t		 max;
	int			 ip_len;
	int			 ip_off;
	int			 tag = -1;

	PF_RULES_RASSERT();

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_SCRUB].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != dir)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != AF_INET)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != h->ip_p)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr,
		    (struct pf_addr *)&h->ip_src.s_addr, AF_INET,
		    r->src.neg, kif, M_GETFIB(m)))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr,
		    (struct pf_addr *)&h->ip_dst.s_addr, AF_INET,
		    r->dst.neg, NULL, M_GETFIB(m)))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->match_tag && !pf_match_tag(m, r, &tag,
		    pd->pf_mtag ? pd->pf_mtag->tag : 0))
			r = TAILQ_NEXT(r, entries);
		else
			break;
	}

	if (r == NULL || r->action == PF_NOSCRUB)
		return (PF_PASS);
	else {
		r->packets[dir == PF_OUT]++;
		r->bytes[dir == PF_OUT] += pd->tot_len;
	}

	/* Check for illegal packets */
	if (hlen < (int)sizeof(struct ip))
		goto drop;

	if (hlen > ntohs(h->ip_len))
		goto drop;

	/* Clear IP_DF if the rule uses the no-df option */
	if (r->rule_flag & PFRULE_NODF && h->ip_off & htons(IP_DF)) {
		u_int16_t ip_off = h->ip_off;

		h->ip_off &= htons(~IP_DF);
		h->ip_sum = pf_cksum_fixup(h->ip_sum, ip_off, h->ip_off, 0);
	}

	/* We will need other tests here */
	if (!fragoff && !mff)
		goto no_fragment;

	/* We're dealing with a fragment now. Don't allow fragments
	 * with IP_DF to enter the cache. If the flag was cleared by
	 * no-df above, fine. Otherwise drop it.
	 */
	if (h->ip_off & htons(IP_DF)) {
		DPFPRINTF(("IP_DF\n"));
		goto bad;
	}

	ip_len = ntohs(h->ip_len) - hlen;
	ip_off = (ntohs(h->ip_off) & IP_OFFMASK) << 3;

	/* All fragments are 8 byte aligned */
	if (mff && (ip_len & 0x7)) {
		DPFPRINTF(("mff and %d\n", ip_len));
		goto bad;
	}

	/* Respect maximum length */
	if (fragoff + ip_len > IP_MAXPACKET) {
		DPFPRINTF(("max packet %d\n", fragoff + ip_len));
		goto bad;
	}
	max = fragoff + ip_len;

	if ((r->rule_flag & (PFRULE_FRAGCROP|PFRULE_FRAGDROP)) == 0) {

		/* Fully buffer all of the fragments */
		PF_FRAG_LOCK();
		frag = pf_find_fragment(h, &V_pf_frag_tree);

		/* Check if we saw the last fragment already */
		if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
		    max > frag->fr_max)
			goto bad;

		/* Get an entry for the fragment queue */
		frent = uma_zalloc(V_pf_frent_z, M_NOWAIT);
		if (frent == NULL) {
			PF_FRAG_UNLOCK();
			REASON_SET(reason, PFRES_MEMORY);
			return (PF_DROP);
		}
		frent->fr_ip = h;
		frent->fr_m = m;

		/* Might return a completely reassembled mbuf, or NULL */
		DPFPRINTF(("reass frag %d @ %d-%d\n", h->ip_id, fragoff, max));
		*m0 = m = pf_reassemble(m0, &frag, frent, mff);
		PF_FRAG_UNLOCK();

		if (m == NULL)
			return (PF_DROP);

		/* use mtag from concatenated mbuf chain */
		pd->pf_mtag = pf_find_mtag(m);
#ifdef DIAGNOSTIC
		if (pd->pf_mtag == NULL) {
			printf("%s: pf_find_mtag returned NULL(1)\n", __func__);
			if ((pd->pf_mtag = pf_get_mtag(m)) == NULL) {
				m_freem(m);
				*m0 = NULL;
				goto no_mem;
			}
		}
#endif
		if (frag != NULL && (frag->fr_flags & PFFRAG_DROP))
			goto drop;

		h = mtod(m, struct ip *);
	} else {
		/* non-buffering fragment cache (drops or masks overlaps) */
		int	nomem = 0;

		if (dir == PF_OUT && pd->pf_mtag->flags & PF_TAG_FRAGCACHE) {
			/*
			 * Already passed the fragment cache in the
			 * input direction.  If we continued, it would
			 * appear to be a dup and would be dropped.
			 */
			goto fragment_pass;
		}

		PF_FRAG_LOCK();
		frag = pf_find_fragment(h, &V_pf_cache_tree);

		/* Check if we saw the last fragment already */
		if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
		    max > frag->fr_max) {
			if (r->rule_flag & PFRULE_FRAGDROP)
				frag->fr_flags |= PFFRAG_DROP;
			goto bad;
		}

		*m0 = m = pf_fragcache(m0, h, &frag, mff,
		    (r->rule_flag & PFRULE_FRAGDROP) ? 1 : 0, &nomem);
		PF_FRAG_UNLOCK();
		if (m == NULL) {
			if (nomem)
				goto no_mem;
			goto drop;
		}

		/* use mtag from copied and trimmed mbuf chain */
		pd->pf_mtag = pf_find_mtag(m);
#ifdef DIAGNOSTIC
		if (pd->pf_mtag == NULL) {
			printf("%s: pf_find_mtag returned NULL(2)\n", __func__);
			if ((pd->pf_mtag = pf_get_mtag(m)) == NULL) {
				m_freem(m);
				*m0 = NULL;
				goto no_mem;
			}
		}
#endif
		if (dir == PF_IN)
			pd->pf_mtag->flags |= PF_TAG_FRAGCACHE;

		if (frag != NULL && (frag->fr_flags & PFFRAG_DROP))
			goto drop;
		goto fragment_pass;
	}

 no_fragment:
	/* At this point, only IP_DF is allowed in ip_off */
	if (h->ip_off & ~htons(IP_DF)) {
		u_int16_t ip_off = h->ip_off;

		h->ip_off &= htons(IP_DF);
		h->ip_sum = pf_cksum_fixup(h->ip_sum, ip_off, h->ip_off, 0);
	}

	/* not missing a return here */

 fragment_pass:
	pf_scrub_ip(&m, r->rule_flag, r->min_ttl, r->set_tos);

	if ((r->rule_flag & (PFRULE_FRAGCROP|PFRULE_FRAGDROP)) == 0)
		pd->flags |= PFDESC_IP_REAS;
	return (PF_PASS);

 no_mem:
	REASON_SET(reason, PFRES_MEMORY);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET, dir, *reason, r, NULL, NULL, pd,
		    1);
	return (PF_DROP);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET, dir, *reason, r, NULL, NULL, pd,
		    1);
	return (PF_DROP);

 bad:
	DPFPRINTF(("dropping bad fragment\n"));

	/* Free associated fragments */
	if (frag != NULL) {
		pf_free_fragment(frag);
		PF_FRAG_UNLOCK();
	}

	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET, dir, *reason, r, NULL, NULL, pd,
		    1);

	return (PF_DROP);
}
#endif

#ifdef INET6
int
pf_normalize_ip6(struct mbuf **m0, int dir, struct pfi_kif *kif,
    u_short *reason, struct pf_pdesc *pd)
{
	struct mbuf		*m = *m0;
	struct pf_rule		*r;
	struct ip6_hdr		*h = mtod(m, struct ip6_hdr *);
	int			 off;
	struct ip6_ext		 ext;
	struct ip6_opt		 opt;
	struct ip6_opt_jumbo	 jumbo;
	struct ip6_frag		 frag;
	u_int32_t		 jumbolen = 0, plen;
	u_int16_t		 fragoff = 0;
	int			 optend;
	int			 ooff;
	u_int8_t		 proto;
	int			 terminal;

	PF_RULES_RASSERT();

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_SCRUB].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != dir)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != AF_INET6)
			r = r->skip[PF_SKIP_AF].ptr;
#if 0 /* header chain! */
		else if (r->proto && r->proto != h->ip6_nxt)
			r = r->skip[PF_SKIP_PROTO].ptr;
#endif
		else if (PF_MISMATCHAW(&r->src.addr,
		    (struct pf_addr *)&h->ip6_src, AF_INET6,
		    r->src.neg, kif, M_GETFIB(m)))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr,
		    (struct pf_addr *)&h->ip6_dst, AF_INET6,
		    r->dst.neg, NULL, M_GETFIB(m)))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else
			break;
	}

	if (r == NULL || r->action == PF_NOSCRUB)
		return (PF_PASS);
	else {
		r->packets[dir == PF_OUT]++;
		r->bytes[dir == PF_OUT] += pd->tot_len;
	}

	/* Check for illegal packets */
	if (sizeof(struct ip6_hdr) + IPV6_MAXPACKET < m->m_pkthdr.len)
		goto drop;

	off = sizeof(struct ip6_hdr);
	proto = h->ip6_nxt;
	terminal = 0;
	do {
		switch (proto) {
		case IPPROTO_FRAGMENT:
			goto fragment;
			break;
		case IPPROTO_AH:
		case IPPROTO_ROUTING:
		case IPPROTO_DSTOPTS:
			if (!pf_pull_hdr(m, off, &ext, sizeof(ext), NULL,
			    NULL, AF_INET6))
				goto shortpkt;
			if (proto == IPPROTO_AH)
				off += (ext.ip6e_len + 2) * 4;
			else
				off += (ext.ip6e_len + 1) * 8;
			proto = ext.ip6e_nxt;
			break;
		case IPPROTO_HOPOPTS:
			if (!pf_pull_hdr(m, off, &ext, sizeof(ext), NULL,
			    NULL, AF_INET6))
				goto shortpkt;
			optend = off + (ext.ip6e_len + 1) * 8;
			ooff = off + sizeof(ext);
			do {
				if (!pf_pull_hdr(m, ooff, &opt.ip6o_type,
				    sizeof(opt.ip6o_type), NULL, NULL,
				    AF_INET6))
					goto shortpkt;
				if (opt.ip6o_type == IP6OPT_PAD1) {
					ooff++;
					continue;
				}
				if (!pf_pull_hdr(m, ooff, &opt, sizeof(opt),
				    NULL, NULL, AF_INET6))
					goto shortpkt;
				if (ooff + sizeof(opt) + opt.ip6o_len > optend)
					goto drop;
				switch (opt.ip6o_type) {
				case IP6OPT_JUMBO:
					if (h->ip6_plen != 0)
						goto drop;
					if (!pf_pull_hdr(m, ooff, &jumbo,
					    sizeof(jumbo), NULL, NULL,
					    AF_INET6))
						goto shortpkt;
					memcpy(&jumbolen, jumbo.ip6oj_jumbo_len,
					    sizeof(jumbolen));
					jumbolen = ntohl(jumbolen);
					if (jumbolen <= IPV6_MAXPACKET)
						goto drop;
					if (sizeof(struct ip6_hdr) + jumbolen !=
					    m->m_pkthdr.len)
						goto drop;
					break;
				default:
					break;
				}
				ooff += sizeof(opt) + opt.ip6o_len;
			} while (ooff < optend);

			off = optend;
			proto = ext.ip6e_nxt;
			break;
		default:
			terminal = 1;
			break;
		}
	} while (!terminal);

	/* jumbo payload option must be present, or plen > 0 */
	if (ntohs(h->ip6_plen) == 0)
		plen = jumbolen;
	else
		plen = ntohs(h->ip6_plen);
	if (plen == 0)
		goto drop;
	if (sizeof(struct ip6_hdr) + plen > m->m_pkthdr.len)
		goto shortpkt;

	pf_scrub_ip6(&m, r->min_ttl);

	return (PF_PASS);

 fragment:
	if (ntohs(h->ip6_plen) == 0 || jumbolen)
		goto drop;
	plen = ntohs(h->ip6_plen);

	if (!pf_pull_hdr(m, off, &frag, sizeof(frag), NULL, NULL, AF_INET6))
		goto shortpkt;
	fragoff = ntohs(frag.ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff + (plen - off - sizeof(frag)) > IPV6_MAXPACKET)
		goto badfrag;

	/* do something about it */
	/* remember to set pd->flags |= PFDESC_IP_REAS */
	return (PF_PASS);

 shortpkt:
	REASON_SET(reason, PFRES_SHORT);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET6, dir, *reason, r, NULL, NULL, pd,
		    1);
	return (PF_DROP);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET6, dir, *reason, r, NULL, NULL, pd,
		    1);
	return (PF_DROP);

 badfrag:
	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET6, dir, *reason, r, NULL, NULL, pd,
		    1);
	return (PF_DROP);
}
#endif /* INET6 */

int
pf_normalize_tcp(int dir, struct pfi_kif *kif, struct mbuf *m, int ipoff,
    int off, void *h, struct pf_pdesc *pd)
{
	struct pf_rule	*r, *rm = NULL;
	struct tcphdr	*th = pd->hdr.tcp;
	int		 rewrite = 0;
	u_short		 reason;
	u_int8_t	 flags;
	sa_family_t	 af = pd->af;

	PF_RULES_RASSERT();

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_SCRUB].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != dir)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, pd->src, af,
		    r->src.neg, kif, M_GETFIB(m)))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
			    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, pd->dst, af,
		    r->dst.neg, NULL, M_GETFIB(m)))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
			    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		else if (r->os_fingerprint != PF_OSFP_ANY && !pf_osfp_match(
			    pf_osfp_fingerprint(pd, m, off, th),
			    r->os_fingerprint))
			r = TAILQ_NEXT(r, entries);
		else {
			rm = r;
			break;
		}
	}

	if (rm == NULL || rm->action == PF_NOSCRUB)
		return (PF_PASS);
	else {
		r->packets[dir == PF_OUT]++;
		r->bytes[dir == PF_OUT] += pd->tot_len;
	}

	if (rm->rule_flag & PFRULE_REASSEMBLE_TCP)
		pd->flags |= PFDESC_TCP_NORM;

	flags = th->th_flags;
	if (flags & TH_SYN) {
		/* Illegal packet */
		if (flags & TH_RST)
			goto tcp_drop;

		if (flags & TH_FIN)
			flags &= ~TH_FIN;
	} else {
		/* Illegal packet */
		if (!(flags & (TH_ACK|TH_RST)))
			goto tcp_drop;
	}

	if (!(flags & TH_ACK)) {
		/* These flags are only valid if ACK is set */
		if ((flags & TH_FIN) || (flags & TH_PUSH) || (flags & TH_URG))
			goto tcp_drop;
	}

	/* Check for illegal header length */
	if (th->th_off < (sizeof(struct tcphdr) >> 2))
		goto tcp_drop;

	/* If flags changed, or reserved data set, then adjust */
	if (flags != th->th_flags || th->th_x2 != 0) {
		u_int16_t	ov, nv;

		ov = *(u_int16_t *)(&th->th_ack + 1);
		th->th_flags = flags;
		th->th_x2 = 0;
		nv = *(u_int16_t *)(&th->th_ack + 1);

		th->th_sum = pf_cksum_fixup(th->th_sum, ov, nv, 0);
		rewrite = 1;
	}

	/* Remove urgent pointer, if TH_URG is not set */
	if (!(flags & TH_URG) && th->th_urp) {
		th->th_sum = pf_cksum_fixup(th->th_sum, th->th_urp, 0, 0);
		th->th_urp = 0;
		rewrite = 1;
	}

	/* Process options */
	if (r->max_mss && pf_normalize_tcpopt(r, m, th, off, pd->af))
		rewrite = 1;

	/* copy back packet headers if we sanitized */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);

 tcp_drop:
	REASON_SET(&reason, PFRES_NORM);
	if (rm != NULL && r->log)
		PFLOG_PACKET(kif, m, AF_INET, dir, reason, r, NULL, NULL, pd,
		    1);
	return (PF_DROP);
}

int
pf_normalize_tcp_init(struct mbuf *m, int off, struct pf_pdesc *pd,
    struct tcphdr *th, struct pf_state_peer *src, struct pf_state_peer *dst)
{
	u_int32_t tsval, tsecr;
	u_int8_t hdr[60];
	u_int8_t *opt;

	KASSERT((src->scrub == NULL),
	    ("pf_normalize_tcp_init: src->scrub != NULL"));

	src->scrub = uma_zalloc(V_pf_state_scrub_z, M_ZERO | M_NOWAIT);
	if (src->scrub == NULL)
		return (1);

	switch (pd->af) {
#ifdef INET
	case AF_INET: {
		struct ip *h = mtod(m, struct ip *);
		src->scrub->pfss_ttl = h->ip_ttl;
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *h = mtod(m, struct ip6_hdr *);
		src->scrub->pfss_ttl = h->ip6_hlim;
		break;
	}
#endif /* INET6 */
	}


	/*
	 * All normalizations below are only begun if we see the start of
	 * the connections.  They must all set an enabled bit in pfss_flags
	 */
	if ((th->th_flags & TH_SYN) == 0)
		return (0);


	if (th->th_off > (sizeof(struct tcphdr) >> 2) && src->scrub &&
	    pf_pull_hdr(m, off, hdr, th->th_off << 2, NULL, NULL, pd->af)) {
		/* Diddle with TCP options */
		int hlen;
		opt = hdr + sizeof(struct tcphdr);
		hlen = (th->th_off << 2) - sizeof(struct tcphdr);
		while (hlen >= TCPOLEN_TIMESTAMP) {
			switch (*opt) {
			case TCPOPT_EOL:	/* FALLTHROUGH */
			case TCPOPT_NOP:
				opt++;
				hlen--;
				break;
			case TCPOPT_TIMESTAMP:
				if (opt[1] >= TCPOLEN_TIMESTAMP) {
					src->scrub->pfss_flags |=
					    PFSS_TIMESTAMP;
					src->scrub->pfss_ts_mod =
					    htonl(arc4random());

					/* note PFSS_PAWS not set yet */
					memcpy(&tsval, &opt[2],
					    sizeof(u_int32_t));
					memcpy(&tsecr, &opt[6],
					    sizeof(u_int32_t));
					src->scrub->pfss_tsval0 = ntohl(tsval);
					src->scrub->pfss_tsval = ntohl(tsval);
					src->scrub->pfss_tsecr = ntohl(tsecr);
					getmicrouptime(&src->scrub->pfss_last);
				}
				/* FALLTHROUGH */
			default:
				hlen -= MAX(opt[1], 2);
				opt += MAX(opt[1], 2);
				break;
			}
		}
	}

	return (0);
}

void
pf_normalize_tcp_cleanup(struct pf_state *state)
{
	if (state->src.scrub)
		uma_zfree(V_pf_state_scrub_z, state->src.scrub);
	if (state->dst.scrub)
		uma_zfree(V_pf_state_scrub_z, state->dst.scrub);

	/* Someday... flush the TCP segment reassembly descriptors. */
}

int
pf_normalize_tcp_stateful(struct mbuf *m, int off, struct pf_pdesc *pd,
    u_short *reason, struct tcphdr *th, struct pf_state *state,
    struct pf_state_peer *src, struct pf_state_peer *dst, int *writeback)
{
	struct timeval uptime;
	u_int32_t tsval, tsecr;
	u_int tsval_from_last;
	u_int8_t hdr[60];
	u_int8_t *opt;
	int copyback = 0;
	int got_ts = 0;

	KASSERT((src->scrub || dst->scrub),
	    ("%s: src->scrub && dst->scrub!", __func__));

	/*
	 * Enforce the minimum TTL seen for this connection.  Negate a common
	 * technique to evade an intrusion detection system and confuse
	 * firewall state code.
	 */
	switch (pd->af) {
#ifdef INET
	case AF_INET: {
		if (src->scrub) {
			struct ip *h = mtod(m, struct ip *);
			if (h->ip_ttl > src->scrub->pfss_ttl)
				src->scrub->pfss_ttl = h->ip_ttl;
			h->ip_ttl = src->scrub->pfss_ttl;
		}
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		if (src->scrub) {
			struct ip6_hdr *h = mtod(m, struct ip6_hdr *);
			if (h->ip6_hlim > src->scrub->pfss_ttl)
				src->scrub->pfss_ttl = h->ip6_hlim;
			h->ip6_hlim = src->scrub->pfss_ttl;
		}
		break;
	}
#endif /* INET6 */
	}

	if (th->th_off > (sizeof(struct tcphdr) >> 2) &&
	    ((src->scrub && (src->scrub->pfss_flags & PFSS_TIMESTAMP)) ||
	    (dst->scrub && (dst->scrub->pfss_flags & PFSS_TIMESTAMP))) &&
	    pf_pull_hdr(m, off, hdr, th->th_off << 2, NULL, NULL, pd->af)) {
		/* Diddle with TCP options */
		int hlen;
		opt = hdr + sizeof(struct tcphdr);
		hlen = (th->th_off << 2) - sizeof(struct tcphdr);
		while (hlen >= TCPOLEN_TIMESTAMP) {
			switch (*opt) {
			case TCPOPT_EOL:	/* FALLTHROUGH */
			case TCPOPT_NOP:
				opt++;
				hlen--;
				break;
			case TCPOPT_TIMESTAMP:
				/* Modulate the timestamps.  Can be used for
				 * NAT detection, OS uptime determination or
				 * reboot detection.
				 */

				if (got_ts) {
					/* Huh?  Multiple timestamps!? */
					if (V_pf_status.debug >= PF_DEBUG_MISC) {
						DPFPRINTF(("multiple TS??"));
						pf_print_state(state);
						printf("\n");
					}
					REASON_SET(reason, PFRES_TS);
					return (PF_DROP);
				}
				if (opt[1] >= TCPOLEN_TIMESTAMP) {
					memcpy(&tsval, &opt[2],
					    sizeof(u_int32_t));
					if (tsval && src->scrub &&
					    (src->scrub->pfss_flags &
					    PFSS_TIMESTAMP)) {
						tsval = ntohl(tsval);
						pf_change_a(&opt[2],
						    &th->th_sum,
						    htonl(tsval +
						    src->scrub->pfss_ts_mod),
						    0);
						copyback = 1;
					}

					/* Modulate TS reply iff valid (!0) */
					memcpy(&tsecr, &opt[6],
					    sizeof(u_int32_t));
					if (tsecr && dst->scrub &&
					    (dst->scrub->pfss_flags &
					    PFSS_TIMESTAMP)) {
						tsecr = ntohl(tsecr)
						    - dst->scrub->pfss_ts_mod;
						pf_change_a(&opt[6],
						    &th->th_sum, htonl(tsecr),
						    0);
						copyback = 1;
					}
					got_ts = 1;
				}
				/* FALLTHROUGH */
			default:
				hlen -= MAX(opt[1], 2);
				opt += MAX(opt[1], 2);
				break;
			}
		}
		if (copyback) {
			/* Copyback the options, caller copys back header */
			*writeback = 1;
			m_copyback(m, off + sizeof(struct tcphdr),
			    (th->th_off << 2) - sizeof(struct tcphdr), hdr +
			    sizeof(struct tcphdr));
		}
	}


	/*
	 * Must invalidate PAWS checks on connections idle for too long.
	 * The fastest allowed timestamp clock is 1ms.  That turns out to
	 * be about 24 days before it wraps.  XXX Right now our lowerbound
	 * TS echo check only works for the first 12 days of a connection
	 * when the TS has exhausted half its 32bit space
	 */
#define TS_MAX_IDLE	(24*24*60*60)
#define TS_MAX_CONN	(12*24*60*60)	/* XXX remove when better tsecr check */

	getmicrouptime(&uptime);
	if (src->scrub && (src->scrub->pfss_flags & PFSS_PAWS) &&
	    (uptime.tv_sec - src->scrub->pfss_last.tv_sec > TS_MAX_IDLE ||
	    time_uptime - state->creation > TS_MAX_CONN))  {
		if (V_pf_status.debug >= PF_DEBUG_MISC) {
			DPFPRINTF(("src idled out of PAWS\n"));
			pf_print_state(state);
			printf("\n");
		}
		src->scrub->pfss_flags = (src->scrub->pfss_flags & ~PFSS_PAWS)
		    | PFSS_PAWS_IDLED;
	}
	if (dst->scrub && (dst->scrub->pfss_flags & PFSS_PAWS) &&
	    uptime.tv_sec - dst->scrub->pfss_last.tv_sec > TS_MAX_IDLE) {
		if (V_pf_status.debug >= PF_DEBUG_MISC) {
			DPFPRINTF(("dst idled out of PAWS\n"));
			pf_print_state(state);
			printf("\n");
		}
		dst->scrub->pfss_flags = (dst->scrub->pfss_flags & ~PFSS_PAWS)
		    | PFSS_PAWS_IDLED;
	}

	if (got_ts && src->scrub && dst->scrub &&
	    (src->scrub->pfss_flags & PFSS_PAWS) &&
	    (dst->scrub->pfss_flags & PFSS_PAWS)) {
		/* Validate that the timestamps are "in-window".
		 * RFC1323 describes TCP Timestamp options that allow
		 * measurement of RTT (round trip time) and PAWS
		 * (protection against wrapped sequence numbers).  PAWS
		 * gives us a set of rules for rejecting packets on
		 * long fat pipes (packets that were somehow delayed
		 * in transit longer than the time it took to send the
		 * full TCP sequence space of 4Gb).  We can use these
		 * rules and infer a few others that will let us treat
		 * the 32bit timestamp and the 32bit echoed timestamp
		 * as sequence numbers to prevent a blind attacker from
		 * inserting packets into a connection.
		 *
		 * RFC1323 tells us:
		 *  - The timestamp on this packet must be greater than
		 *    or equal to the last value echoed by the other
		 *    endpoint.  The RFC says those will be discarded
		 *    since it is a dup that has already been acked.
		 *    This gives us a lowerbound on the timestamp.
		 *        timestamp >= other last echoed timestamp
		 *  - The timestamp will be less than or equal to
		 *    the last timestamp plus the time between the
		 *    last packet and now.  The RFC defines the max
		 *    clock rate as 1ms.  We will allow clocks to be
		 *    up to 10% fast and will allow a total difference
		 *    or 30 seconds due to a route change.  And this
		 *    gives us an upperbound on the timestamp.
		 *        timestamp <= last timestamp + max ticks
		 *    We have to be careful here.  Windows will send an
		 *    initial timestamp of zero and then initialize it
		 *    to a random value after the 3whs; presumably to
		 *    avoid a DoS by having to call an expensive RNG
		 *    during a SYN flood.  Proof MS has at least one
		 *    good security geek.
		 *
		 *  - The TCP timestamp option must also echo the other
		 *    endpoints timestamp.  The timestamp echoed is the
		 *    one carried on the earliest unacknowledged segment
		 *    on the left edge of the sequence window.  The RFC
		 *    states that the host will reject any echoed
		 *    timestamps that were larger than any ever sent.
		 *    This gives us an upperbound on the TS echo.
		 *        tescr <= largest_tsval
		 *  - The lowerbound on the TS echo is a little more
		 *    tricky to determine.  The other endpoint's echoed
		 *    values will not decrease.  But there may be
		 *    network conditions that re-order packets and
		 *    cause our view of them to decrease.  For now the
		 *    only lowerbound we can safely determine is that
		 *    the TS echo will never be less than the original
		 *    TS.  XXX There is probably a better lowerbound.
		 *    Remove TS_MAX_CONN with better lowerbound check.
		 *        tescr >= other original TS
		 *
		 * It is also important to note that the fastest
		 * timestamp clock of 1ms will wrap its 32bit space in
		 * 24 days.  So we just disable TS checking after 24
		 * days of idle time.  We actually must use a 12d
		 * connection limit until we can come up with a better
		 * lowerbound to the TS echo check.
		 */
		struct timeval delta_ts;
		int ts_fudge;


		/*
		 * PFTM_TS_DIFF is how many seconds of leeway to allow
		 * a host's timestamp.  This can happen if the previous
		 * packet got delayed in transit for much longer than
		 * this packet.
		 */
		if ((ts_fudge = state->rule.ptr->timeout[PFTM_TS_DIFF]) == 0)
			ts_fudge = V_pf_default_rule.timeout[PFTM_TS_DIFF];

		/* Calculate max ticks since the last timestamp */
#define TS_MAXFREQ	1100		/* RFC max TS freq of 1Khz + 10% skew */
#define TS_MICROSECS	1000000		/* microseconds per second */
		delta_ts = uptime;
		timevalsub(&delta_ts, &src->scrub->pfss_last);
		tsval_from_last = (delta_ts.tv_sec + ts_fudge) * TS_MAXFREQ;
		tsval_from_last += delta_ts.tv_usec / (TS_MICROSECS/TS_MAXFREQ);

		if ((src->state >= TCPS_ESTABLISHED &&
		    dst->state >= TCPS_ESTABLISHED) &&
		    (SEQ_LT(tsval, dst->scrub->pfss_tsecr) ||
		    SEQ_GT(tsval, src->scrub->pfss_tsval + tsval_from_last) ||
		    (tsecr && (SEQ_GT(tsecr, dst->scrub->pfss_tsval) ||
		    SEQ_LT(tsecr, dst->scrub->pfss_tsval0))))) {
			/* Bad RFC1323 implementation or an insertion attack.
			 *
			 * - Solaris 2.6 and 2.7 are known to send another ACK
			 *   after the FIN,FIN|ACK,ACK closing that carries
			 *   an old timestamp.
			 */

			DPFPRINTF(("Timestamp failed %c%c%c%c\n",
			    SEQ_LT(tsval, dst->scrub->pfss_tsecr) ? '0' : ' ',
			    SEQ_GT(tsval, src->scrub->pfss_tsval +
			    tsval_from_last) ? '1' : ' ',
			    SEQ_GT(tsecr, dst->scrub->pfss_tsval) ? '2' : ' ',
			    SEQ_LT(tsecr, dst->scrub->pfss_tsval0)? '3' : ' '));
			DPFPRINTF((" tsval: %u  tsecr: %u  +ticks: %u  "
			    "idle: %jus %lums\n",
			    tsval, tsecr, tsval_from_last,
			    (uintmax_t)delta_ts.tv_sec,
			    delta_ts.tv_usec / 1000));
			DPFPRINTF((" src->tsval: %u  tsecr: %u\n",
			    src->scrub->pfss_tsval, src->scrub->pfss_tsecr));
			DPFPRINTF((" dst->tsval: %u  tsecr: %u  tsval0: %u"
			    "\n", dst->scrub->pfss_tsval,
			    dst->scrub->pfss_tsecr, dst->scrub->pfss_tsval0));
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				pf_print_state(state);
				pf_print_flags(th->th_flags);
				printf("\n");
			}
			REASON_SET(reason, PFRES_TS);
			return (PF_DROP);
		}

		/* XXX I'd really like to require tsecr but it's optional */

	} else if (!got_ts && (th->th_flags & TH_RST) == 0 &&
	    ((src->state == TCPS_ESTABLISHED && dst->state == TCPS_ESTABLISHED)
	    || pd->p_len > 0 || (th->th_flags & TH_SYN)) &&
	    src->scrub && dst->scrub &&
	    (src->scrub->pfss_flags & PFSS_PAWS) &&
	    (dst->scrub->pfss_flags & PFSS_PAWS)) {
		/* Didn't send a timestamp.  Timestamps aren't really useful
		 * when:
		 *  - connection opening or closing (often not even sent).
		 *    but we must not let an attacker to put a FIN on a
		 *    data packet to sneak it through our ESTABLISHED check.
		 *  - on a TCP reset.  RFC suggests not even looking at TS.
		 *  - on an empty ACK.  The TS will not be echoed so it will
		 *    probably not help keep the RTT calculation in sync and
		 *    there isn't as much danger when the sequence numbers
		 *    got wrapped.  So some stacks don't include TS on empty
		 *    ACKs :-(
		 *
		 * To minimize the disruption to mostly RFC1323 conformant
		 * stacks, we will only require timestamps on data packets.
		 *
		 * And what do ya know, we cannot require timestamps on data
		 * packets.  There appear to be devices that do legitimate
		 * TCP connection hijacking.  There are HTTP devices that allow
		 * a 3whs (with timestamps) and then buffer the HTTP request.
		 * If the intermediate device has the HTTP response cache, it
		 * will spoof the response but not bother timestamping its
		 * packets.  So we can look for the presence of a timestamp in
		 * the first data packet and if there, require it in all future
		 * packets.
		 */

		if (pd->p_len > 0 && (src->scrub->pfss_flags & PFSS_DATA_TS)) {
			/*
			 * Hey!  Someone tried to sneak a packet in.  Or the
			 * stack changed its RFC1323 behavior?!?!
			 */
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				DPFPRINTF(("Did not receive expected RFC1323 "
				    "timestamp\n"));
				pf_print_state(state);
				pf_print_flags(th->th_flags);
				printf("\n");
			}
			REASON_SET(reason, PFRES_TS);
			return (PF_DROP);
		}
	}


	/*
	 * We will note if a host sends his data packets with or without
	 * timestamps.  And require all data packets to contain a timestamp
	 * if the first does.  PAWS implicitly requires that all data packets be
	 * timestamped.  But I think there are middle-man devices that hijack
	 * TCP streams immediately after the 3whs and don't timestamp their
	 * packets (seen in a WWW accelerator or cache).
	 */
	if (pd->p_len > 0 && src->scrub && (src->scrub->pfss_flags &
	    (PFSS_TIMESTAMP|PFSS_DATA_TS|PFSS_DATA_NOTS)) == PFSS_TIMESTAMP) {
		if (got_ts)
			src->scrub->pfss_flags |= PFSS_DATA_TS;
		else {
			src->scrub->pfss_flags |= PFSS_DATA_NOTS;
			if (V_pf_status.debug >= PF_DEBUG_MISC && dst->scrub &&
			    (dst->scrub->pfss_flags & PFSS_TIMESTAMP)) {
				/* Don't warn if other host rejected RFC1323 */
				DPFPRINTF(("Broken RFC1323 stack did not "
				    "timestamp data packet. Disabled PAWS "
				    "security.\n"));
				pf_print_state(state);
				pf_print_flags(th->th_flags);
				printf("\n");
			}
		}
	}


	/*
	 * Update PAWS values
	 */
	if (got_ts && src->scrub && PFSS_TIMESTAMP == (src->scrub->pfss_flags &
	    (PFSS_PAWS_IDLED|PFSS_TIMESTAMP))) {
		getmicrouptime(&src->scrub->pfss_last);
		if (SEQ_GEQ(tsval, src->scrub->pfss_tsval) ||
		    (src->scrub->pfss_flags & PFSS_PAWS) == 0)
			src->scrub->pfss_tsval = tsval;

		if (tsecr) {
			if (SEQ_GEQ(tsecr, src->scrub->pfss_tsecr) ||
			    (src->scrub->pfss_flags & PFSS_PAWS) == 0)
				src->scrub->pfss_tsecr = tsecr;

			if ((src->scrub->pfss_flags & PFSS_PAWS) == 0 &&
			    (SEQ_LT(tsval, src->scrub->pfss_tsval0) ||
			    src->scrub->pfss_tsval0 == 0)) {
				/* tsval0 MUST be the lowest timestamp */
				src->scrub->pfss_tsval0 = tsval;
			}

			/* Only fully initialized after a TS gets echoed */
			if ((src->scrub->pfss_flags & PFSS_PAWS) == 0)
				src->scrub->pfss_flags |= PFSS_PAWS;
		}
	}

	/* I have a dream....  TCP segment reassembly.... */
	return (0);
}

static int
pf_normalize_tcpopt(struct pf_rule *r, struct mbuf *m, struct tcphdr *th,
    int off, sa_family_t af)
{
	u_int16_t	*mss;
	int		 thoff;
	int		 opt, cnt, optlen = 0;
	int		 rewrite = 0;
	u_char		 opts[TCP_MAXOLEN];
	u_char		*optp = opts;

	thoff = th->th_off << 2;
	cnt = thoff - sizeof(struct tcphdr);

	if (cnt > 0 && !pf_pull_hdr(m, off + sizeof(*th), opts, cnt,
	    NULL, NULL, af))
		return (rewrite);

	for (; cnt > 0; cnt -= optlen, optp += optlen) {
		opt = optp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = optp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {
		case TCPOPT_MAXSEG:
			mss = (u_int16_t *)(optp + 2);
			if ((ntohs(*mss)) > r->max_mss) {
				th->th_sum = pf_cksum_fixup(th->th_sum,
				    *mss, htons(r->max_mss), 0);
				*mss = htons(r->max_mss);
				rewrite = 1;
			}
			break;
		default:
			break;
		}
	}

	if (rewrite)
		m_copyback(m, off + sizeof(*th), thoff - sizeof(*th), opts);

	return (rewrite);
}

#ifdef INET
static void
pf_scrub_ip(struct mbuf **m0, u_int32_t flags, u_int8_t min_ttl, u_int8_t tos)
{
	struct mbuf		*m = *m0;
	struct ip		*h = mtod(m, struct ip *);

	/* Clear IP_DF if no-df was requested */
	if (flags & PFRULE_NODF && h->ip_off & htons(IP_DF)) {
		u_int16_t ip_off = h->ip_off;

		h->ip_off &= htons(~IP_DF);
		h->ip_sum = pf_cksum_fixup(h->ip_sum, ip_off, h->ip_off, 0);
	}

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (min_ttl && h->ip_ttl < min_ttl) {
		u_int16_t ip_ttl = h->ip_ttl;

		h->ip_ttl = min_ttl;
		h->ip_sum = pf_cksum_fixup(h->ip_sum, ip_ttl, h->ip_ttl, 0);
	}

	/* Enforce tos */
	if (flags & PFRULE_SET_TOS) {
		u_int16_t	ov, nv;

		ov = *(u_int16_t *)h;
		h->ip_tos = tos;
		nv = *(u_int16_t *)h;

		h->ip_sum = pf_cksum_fixup(h->ip_sum, ov, nv, 0);
	}

	/* random-id, but not for fragments */
	if (flags & PFRULE_RANDOMID && !(h->ip_off & ~htons(IP_DF))) {
		u_int16_t ip_id = h->ip_id;

		h->ip_id = ip_randomid();
		h->ip_sum = pf_cksum_fixup(h->ip_sum, ip_id, h->ip_id, 0);
	}
}
#endif /* INET */

#ifdef INET6
static void
pf_scrub_ip6(struct mbuf **m0, u_int8_t min_ttl)
{
	struct mbuf		*m = *m0;
	struct ip6_hdr		*h = mtod(m, struct ip6_hdr *);

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (min_ttl && h->ip6_hlim < min_ttl)
		h->ip6_hlim = min_ttl;
}
#endif
