/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2022 Marko Zec
 * Copyright (c) 2005, 2018 University of Zagreb
 * Copyright (c) 2005 International Computer Science Institute
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * An implementation of DXR, a simple IPv4 LPM scheme with compact lookup
 * structures and a trivial search procedure.  More significant bits of
 * the search key are used to directly index a two-stage trie, while the
 * remaining bits are used for finding the next hop in a sorted array.
 * More details in:
 *
 * M. Zec, L. Rizzo, M. Mikuc, DXR: towards a billion routing lookups per
 * second in software, ACM SIGCOMM Computer Communication Review, September
 * 2012
 *
 * M. Zec, M. Mikuc, Pushing the envelope: beyond two billion IP routing
 * lookups per second on commodity CPUs, IEEE SoftCOM, September 2017, Split
 */

#include <sys/cdefs.h>
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ctype.h>
#include <sys/epoch.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <vm/uma.h>

#include <netinet/in.h>
#include <netinet/in_fib.h>

#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/fib_algo.h>

#define	DXR_TRIE_BITS		20

CTASSERT(DXR_TRIE_BITS >= 16 && DXR_TRIE_BITS <= 24);

/* DXR2: two-stage primary trie, instead of a single direct lookup table */
#define	DXR2

#if DXR_TRIE_BITS > 16
#define	DXR_D			16
#else
#define	DXR_D			(DXR_TRIE_BITS - 1)
#endif

#define	D_TBL_SIZE		(1 << DXR_D)
#define	DIRECT_TBL_SIZE		(1 << DXR_TRIE_BITS)
#define	DXR_RANGE_MASK		(0xffffffffU >> DXR_TRIE_BITS)
#define	DXR_RANGE_SHIFT		(32 - DXR_TRIE_BITS)

#define	DESC_BASE_BITS		22
#define	DESC_FRAGMENTS_BITS	(32 - DESC_BASE_BITS)
#define	BASE_MAX		((1 << DESC_BASE_BITS) - 1)
#define	RTBL_SIZE_INCR		(BASE_MAX / 64)

#if DXR_TRIE_BITS < 24
#define	FRAGS_MASK_SHORT	((1 << (23 - DXR_TRIE_BITS)) - 1)
#else
#define	FRAGS_MASK_SHORT	0
#endif
#define	FRAGS_PREF_SHORT	(((1 << DESC_FRAGMENTS_BITS) - 1) & \
				 ~FRAGS_MASK_SHORT)
#define	FRAGS_MARK_XL		(FRAGS_PREF_SHORT - 1)
#define	FRAGS_MARK_HIT		(FRAGS_PREF_SHORT - 2)

#define	IS_SHORT_FORMAT(x)	((x & FRAGS_PREF_SHORT) == FRAGS_PREF_SHORT)
#define	IS_LONG_FORMAT(x)	((x & FRAGS_PREF_SHORT) != FRAGS_PREF_SHORT)
#define	IS_XL_FORMAT(x)		(x == FRAGS_MARK_XL)

#define	RE_SHORT_MAX_NH		((1 << (DXR_TRIE_BITS - 8)) - 1)

#define	CHUNK_HASH_BITS		16
#define	CHUNK_HASH_SIZE		(1 << CHUNK_HASH_BITS)
#define	CHUNK_HASH_MASK		(CHUNK_HASH_SIZE - 1)

#define	TRIE_HASH_BITS		16
#define	TRIE_HASH_SIZE		(1 << TRIE_HASH_BITS)
#define	TRIE_HASH_MASK		(TRIE_HASH_SIZE - 1)

#define	XTBL_SIZE_INCR		(DIRECT_TBL_SIZE / 16)

#define	UNUSED_BUCKETS		8

/* Lookup structure elements */

struct direct_entry {
	uint32_t		fragments: DESC_FRAGMENTS_BITS,
				base: DESC_BASE_BITS;
};

struct range_entry_long {
	uint32_t		start: DXR_RANGE_SHIFT,
				nexthop: DXR_TRIE_BITS;
};

#if DXR_TRIE_BITS < 24
struct range_entry_short {
	uint16_t		start: DXR_RANGE_SHIFT - 8,
				nexthop: DXR_TRIE_BITS - 8;
};
#endif

/* Auxiliary structures */

struct heap_entry {
	uint32_t		start;
	uint32_t		end;
	uint32_t		preflen;
	uint32_t		nexthop;
};

struct chunk_desc {
	LIST_ENTRY(chunk_desc)	cd_all_le;
	LIST_ENTRY(chunk_desc)	cd_hash_le;
	uint32_t		cd_hash;
	uint32_t		cd_refcnt;
	uint32_t		cd_base;
	uint32_t		cd_cur_size;
	uint32_t		cd_max_size;
};

struct trie_desc {
	LIST_ENTRY(trie_desc)	td_all_le;
	LIST_ENTRY(trie_desc)	td_hash_le;
	uint32_t		td_hash;
	uint32_t		td_index;
	uint32_t		td_refcnt;
};

struct dxr_aux {
	/* Glue to external state */
	struct fib_data		*fd;
	uint32_t		fibnum;
	int			refcnt;

	/* Auxiliary build-time tables */
	struct direct_entry	direct_tbl[DIRECT_TBL_SIZE];
	uint16_t		d_tbl[D_TBL_SIZE];
	struct direct_entry	*x_tbl;
	union {
		struct range_entry_long	re;
		uint32_t	fragments;
	}			*range_tbl;

	/* Auxiliary internal state */
	uint32_t		updates_mask[DIRECT_TBL_SIZE / 32];
	struct trie_desc	*trietbl[D_TBL_SIZE];
	LIST_HEAD(, chunk_desc)	chunk_hashtbl[CHUNK_HASH_SIZE];
	LIST_HEAD(, chunk_desc)	all_chunks;
	LIST_HEAD(, chunk_desc) unused_chunks[UNUSED_BUCKETS];
	LIST_HEAD(, trie_desc)	trie_hashtbl[TRIE_HASH_SIZE];
	LIST_HEAD(, trie_desc)	all_trie;
	LIST_HEAD(, trie_desc)	unused_trie; /* abuses hash link entry */
	struct sockaddr_in	dst;
	struct sockaddr_in	mask;
	struct heap_entry	heap[33];
	uint32_t		prefixes;
	uint32_t		updates_low;
	uint32_t		updates_high;
	uint32_t		unused_chunks_size;
	uint32_t		xtbl_size;
	uint32_t		all_trie_cnt;
	uint32_t		unused_trie_cnt;
	uint32_t		trie_rebuilt_prefixes;
	uint32_t		heap_index;
	uint32_t		d_bits;
	uint32_t		rtbl_size;
	uint32_t		rtbl_top;
	uint32_t		rtbl_work_frags;
	uint32_t		work_chunk;
};

/* Main lookup structure container */

struct dxr {
	/* Lookup tables */
	void			*d;
	void			*x;
	void			*r;
	struct nhop_object	**nh_tbl;

	/* Glue to external state */
	struct dxr_aux		*aux;
	struct fib_data		*fd;
	struct epoch_context	epoch_ctx;
	uint32_t		fibnum;
	uint16_t		d_shift;
	uint16_t		x_shift;
	uint32_t		x_mask;
};

static MALLOC_DEFINE(M_DXRLPM, "dxr", "DXR LPM");
static MALLOC_DEFINE(M_DXRAUX, "dxr aux", "DXR auxiliary");

uma_zone_t chunk_zone;
uma_zone_t trie_zone;

VNET_DEFINE_STATIC(int, frag_limit) = 100;
#define	V_frag_limit	VNET(frag_limit)

/* Binary search for a matching address range */
#define	DXR_LOOKUP_STAGE					\
	if (masked_dst < range[middle].start) {			\
		upperbound = middle;				\
		middle = (middle + lowerbound) / 2;		\
	} else if (masked_dst < range[middle + 1].start)	\
		return (range[middle].nexthop);			\
	else {							\
		lowerbound = middle + 1;			\
		middle = (upperbound + middle + 1) / 2;		\
	}							\
	if (upperbound == lowerbound)				\
		return (range[lowerbound].nexthop);

static int
range_lookup(struct range_entry_long *rt, struct direct_entry de, uint32_t dst)
{
	uint32_t base;
	uint32_t upperbound;
	uint32_t middle;
	uint32_t lowerbound;
	uint32_t masked_dst;

	base = de.base;
	lowerbound = 0;
	masked_dst = dst & DXR_RANGE_MASK;

#if DXR_TRIE_BITS < 24
	if (__predict_true(IS_SHORT_FORMAT(de.fragments))) {
		upperbound = de.fragments & FRAGS_MASK_SHORT;
		struct range_entry_short *range =
		    (struct range_entry_short *) &rt[base];

		masked_dst >>= 8;
		middle = upperbound;
		upperbound = upperbound * 2 + 1;

		for (;;) {
			DXR_LOOKUP_STAGE
			DXR_LOOKUP_STAGE
		}
	}
#endif

	upperbound = de.fragments;
	middle = upperbound / 2;
	struct range_entry_long *range = &rt[base];
	if (__predict_false(IS_XL_FORMAT(de.fragments))) {
		upperbound = *((uint32_t *) range);
		range++;
		middle = upperbound / 2;
	}

	for (;;) {
		DXR_LOOKUP_STAGE
		DXR_LOOKUP_STAGE
	}
}

#define DXR_LOOKUP_DEFINE(D)						\
	static int inline						\
	dxr_lookup_##D(struct dxr *dxr, uint32_t dst)			\
	{								\
		struct direct_entry de;					\
		uint16_t *dt = dxr->d;					\
		struct direct_entry *xt = dxr->x;			\
									\
		de = xt[(dt[dst >> (32 - (D))] << (DXR_TRIE_BITS - (D))) \
		    + ((dst >> DXR_RANGE_SHIFT) &			\
		    (0xffffffffU >> (32 - DXR_TRIE_BITS + (D))))];	\
		if (__predict_true(de.fragments == FRAGS_MARK_HIT))	\
			return (de.base);				\
		return (range_lookup(dxr->r, de, dst));			\
	}								\
									\
	static struct nhop_object *					\
	dxr_fib_lookup_##D(void *algo_data,				\
	    const struct flm_lookup_key key, uint32_t scopeid __unused)	\
	{								\
		struct dxr *dxr = algo_data;				\
									\
		return (dxr->nh_tbl[dxr_lookup_##D(dxr,			\
		    ntohl(key.addr4.s_addr))]);				\
	}

#ifdef DXR2
#if DXR_TRIE_BITS > 16
DXR_LOOKUP_DEFINE(16)
#endif
DXR_LOOKUP_DEFINE(15)
DXR_LOOKUP_DEFINE(14)
DXR_LOOKUP_DEFINE(13)
DXR_LOOKUP_DEFINE(12)
DXR_LOOKUP_DEFINE(11)
DXR_LOOKUP_DEFINE(10)
DXR_LOOKUP_DEFINE(9)
#endif /* DXR2 */

static int inline
dxr_lookup(struct dxr *dxr, uint32_t dst)
{
	struct direct_entry de;
#ifdef DXR2
	uint16_t *dt = dxr->d;
	struct direct_entry *xt = dxr->x;

	de = xt[(dt[dst >> dxr->d_shift] << dxr->x_shift) +
	    ((dst >> DXR_RANGE_SHIFT) & dxr->x_mask)];
#else /* !DXR2 */
	struct direct_entry *dt = dxr->d;

	de = dt[dst >> DXR_RANGE_SHIFT];
#endif /* !DXR2 */
	if (__predict_true(de.fragments == FRAGS_MARK_HIT))
		return (de.base);
	return (range_lookup(dxr->r, de, dst));
}

static void
initheap(struct dxr_aux *da, uint32_t dst_u32, uint32_t chunk)
{
	struct heap_entry *fhp = &da->heap[0];
	struct rtentry *rt;
	struct route_nhop_data rnd;
 
	da->heap_index = 0;
	da->dst.sin_addr.s_addr = htonl(dst_u32);
	rt = fib4_lookup_rt(da->fibnum, da->dst.sin_addr, 0, NHR_UNLOCKED,
	    &rnd);
	if (rt != NULL) {
		struct in_addr addr;
		uint32_t scopeid;

		rt_get_inet_prefix_plen(rt, &addr, &fhp->preflen, &scopeid);
		fhp->start = ntohl(addr.s_addr);
		fhp->end = fhp->start;
		if (fhp->preflen < 32)
			fhp->end |= (0xffffffffU >> fhp->preflen);
		fhp->nexthop = fib_get_nhop_idx(da->fd, rnd.rnd_nhop);
	} else {
		fhp->preflen = fhp->nexthop = fhp->start = 0;
		fhp->end = 0xffffffffU;
	}
}

static uint32_t
chunk_size(struct dxr_aux *da, struct direct_entry *fdesc)
{

	if (IS_SHORT_FORMAT(fdesc->fragments))
		return ((fdesc->fragments & FRAGS_MASK_SHORT) + 1);
	else if (IS_XL_FORMAT(fdesc->fragments))
		return (da->range_tbl[fdesc->base].fragments + 2);
	else /* if (IS_LONG_FORMAT(fdesc->fragments)) */
		return (fdesc->fragments + 1);
}

static uint32_t
chunk_hash(struct dxr_aux *da, struct direct_entry *fdesc)
{
	uint32_t size = chunk_size(da, fdesc);
	uint32_t *p = (uint32_t *) &da->range_tbl[fdesc->base];
	uint32_t *l = (uint32_t *) &da->range_tbl[fdesc->base + size];
	uint32_t hash = fdesc->fragments;

	for (; p < l; p++)
		hash = (hash << 7) + (hash >> 13) + *p;

	return (hash + (hash >> 16));
}

static int
chunk_ref(struct dxr_aux *da, uint32_t chunk)
{
	struct direct_entry *fdesc = &da->direct_tbl[chunk];
	struct chunk_desc *cdp, *empty_cdp;
	uint32_t base = fdesc->base;
	uint32_t size = chunk_size(da, fdesc);
	uint32_t hash = chunk_hash(da, fdesc);
	int i;

	/* Find an existing descriptor */
	LIST_FOREACH(cdp, &da->chunk_hashtbl[hash & CHUNK_HASH_MASK],
	    cd_hash_le) {
		if (cdp->cd_hash != hash || cdp->cd_cur_size != size ||
		    memcmp(&da->range_tbl[base], &da->range_tbl[cdp->cd_base],
		    sizeof(struct range_entry_long) * size))
			continue;
		da->rtbl_top = fdesc->base;
		fdesc->base = cdp->cd_base;
		cdp->cd_refcnt++;
		return (0);
	}

	/* No matching chunks found. Find an empty one to recycle. */
	for (cdp = NULL, i = size; cdp == NULL && i < UNUSED_BUCKETS; i++)
		cdp = LIST_FIRST(&da->unused_chunks[i]);

	if (cdp == NULL)
		LIST_FOREACH(empty_cdp, &da->unused_chunks[0], cd_hash_le)
			if (empty_cdp->cd_max_size >= size && (cdp == NULL ||
			    empty_cdp->cd_max_size < cdp->cd_max_size)) {
				cdp = empty_cdp;
				if (empty_cdp->cd_max_size == size)
					break;
			}

	if (cdp != NULL) {
		/* Copy from heap into the recycled chunk */
		bcopy(&da->range_tbl[fdesc->base], &da->range_tbl[cdp->cd_base],
		    size * sizeof(struct range_entry_long));
		fdesc->base = cdp->cd_base;
		da->rtbl_top -= size;
		da->unused_chunks_size -= cdp->cd_max_size;
		if (cdp->cd_max_size > size) {
			/* Split the range in two, need a new descriptor */
			empty_cdp = uma_zalloc(chunk_zone, M_NOWAIT);
			if (empty_cdp == NULL)
				return (1);
			LIST_INSERT_BEFORE(cdp, empty_cdp, cd_all_le);
			empty_cdp->cd_base = cdp->cd_base + size;
			empty_cdp->cd_cur_size = 0;
			empty_cdp->cd_max_size = cdp->cd_max_size - size;

			i = empty_cdp->cd_max_size;
			if (i >= UNUSED_BUCKETS)
				i = 0;
			LIST_INSERT_HEAD(&da->unused_chunks[i], empty_cdp,
			    cd_hash_le);

			da->unused_chunks_size += empty_cdp->cd_max_size;
			cdp->cd_max_size = size;
		}
		LIST_REMOVE(cdp, cd_hash_le);
	} else {
		/* Alloc a new descriptor at the top of the heap*/
		cdp = uma_zalloc(chunk_zone, M_NOWAIT);
		if (cdp == NULL)
			return (1);
		cdp->cd_max_size = size;
		cdp->cd_base = fdesc->base;
		LIST_INSERT_HEAD(&da->all_chunks, cdp, cd_all_le);
		KASSERT(cdp->cd_base + cdp->cd_max_size == da->rtbl_top,
		    ("dxr: %s %d", __FUNCTION__, __LINE__));
	}

	cdp->cd_hash = hash;
	cdp->cd_refcnt = 1;
	cdp->cd_cur_size = size;
	LIST_INSERT_HEAD(&da->chunk_hashtbl[hash & CHUNK_HASH_MASK], cdp,
	    cd_hash_le);
	if (da->rtbl_top >= da->rtbl_size) {
		if (da->rtbl_top >= BASE_MAX) {
			FIB_PRINTF(LOG_ERR, da->fd,
			    "structural limit exceeded at %d "
			    "range table elements", da->rtbl_top);
			return (1);
		}
		da->rtbl_size += RTBL_SIZE_INCR;
		i = (BASE_MAX - da->rtbl_top) * LOG_DEBUG / BASE_MAX;
		FIB_PRINTF(i, da->fd, "range table at %d%% structural limit",
		    da->rtbl_top * 100 / BASE_MAX);
		da->range_tbl = realloc(da->range_tbl,
		    sizeof(*da->range_tbl) * da->rtbl_size + FRAGS_PREF_SHORT,
		    M_DXRAUX, M_NOWAIT);
		if (da->range_tbl == NULL)
			return (1);
	}

	return (0);
}

static void
chunk_unref(struct dxr_aux *da, uint32_t chunk)
{
	struct direct_entry *fdesc = &da->direct_tbl[chunk];
	struct chunk_desc *cdp, *cdp2;
	uint32_t base = fdesc->base;
	uint32_t size = chunk_size(da, fdesc);
	uint32_t hash = chunk_hash(da, fdesc);
	int i;

	/* Find the corresponding descriptor */
	LIST_FOREACH(cdp, &da->chunk_hashtbl[hash & CHUNK_HASH_MASK],
	    cd_hash_le)
		if (cdp->cd_hash == hash && cdp->cd_cur_size == size &&
		    memcmp(&da->range_tbl[base], &da->range_tbl[cdp->cd_base],
		    sizeof(struct range_entry_long) * size) == 0)
			break;

	KASSERT(cdp != NULL, ("dxr: dangling chunk"));
	if (--cdp->cd_refcnt > 0)
		return;

	LIST_REMOVE(cdp, cd_hash_le);
	da->unused_chunks_size += cdp->cd_max_size;
	cdp->cd_cur_size = 0;

	/* Attempt to merge with the preceding chunk, if empty */
	cdp2 = LIST_NEXT(cdp, cd_all_le);
	if (cdp2 != NULL && cdp2->cd_cur_size == 0) {
		KASSERT(cdp2->cd_base + cdp2->cd_max_size == cdp->cd_base,
		    ("dxr: %s %d", __FUNCTION__, __LINE__));
		LIST_REMOVE(cdp, cd_all_le);
		LIST_REMOVE(cdp2, cd_hash_le);
		cdp2->cd_max_size += cdp->cd_max_size;
		uma_zfree(chunk_zone, cdp);
		cdp = cdp2;
	}

	/* Attempt to merge with the subsequent chunk, if empty */
	cdp2 = LIST_PREV(cdp, &da->all_chunks, chunk_desc, cd_all_le);
	if (cdp2 != NULL && cdp2->cd_cur_size == 0) {
		KASSERT(cdp->cd_base + cdp->cd_max_size == cdp2->cd_base,
		    ("dxr: %s %d", __FUNCTION__, __LINE__));
		LIST_REMOVE(cdp, cd_all_le);
		LIST_REMOVE(cdp2, cd_hash_le);
		cdp2->cd_max_size += cdp->cd_max_size;
		cdp2->cd_base = cdp->cd_base;
		uma_zfree(chunk_zone, cdp);
		cdp = cdp2;
	}

	if (cdp->cd_base + cdp->cd_max_size == da->rtbl_top) {
		/* Free the chunk on the top of the range heap, trim the heap */
		KASSERT(cdp == LIST_FIRST(&da->all_chunks),
		    ("dxr: %s %d", __FUNCTION__, __LINE__));
		da->rtbl_top -= cdp->cd_max_size;
		da->unused_chunks_size -= cdp->cd_max_size;
		LIST_REMOVE(cdp, cd_all_le);
		uma_zfree(chunk_zone, cdp);
		return;
	}

	i = cdp->cd_max_size;
	if (i >= UNUSED_BUCKETS)
		i = 0;
	LIST_INSERT_HEAD(&da->unused_chunks[i], cdp, cd_hash_le);
}

#ifdef DXR2
static uint32_t
trie_hash(struct dxr_aux *da, uint32_t dxr_x, uint32_t index)
{
	uint32_t i, *val;
	uint32_t hash = 0;

	for (i = 0; i < (1 << dxr_x); i++) {
		hash = (hash << 3) ^ (hash >> 3);
		val = (uint32_t *)
		    (void *) &da->direct_tbl[(index << dxr_x) + i];
		hash += (*val << 5);
		hash += (*val >> 5);
	}

	return (hash + (hash >> 16));
}

static int
trie_ref(struct dxr_aux *da, uint32_t index)
{
	struct trie_desc *tp;
	uint32_t dxr_d = da->d_bits;
	uint32_t dxr_x = DXR_TRIE_BITS - dxr_d;
	uint32_t hash = trie_hash(da, dxr_x, index);

	/* Find an existing descriptor */
	LIST_FOREACH(tp, &da->trie_hashtbl[hash & TRIE_HASH_MASK], td_hash_le)
		if (tp->td_hash == hash &&
		    memcmp(&da->direct_tbl[index << dxr_x],
		    &da->x_tbl[tp->td_index << dxr_x],
		    sizeof(*da->x_tbl) << dxr_x) == 0) {
			tp->td_refcnt++;
			da->trietbl[index] = tp;
			return(tp->td_index);
		}

	tp = LIST_FIRST(&da->unused_trie);
	if (tp != NULL) {
		LIST_REMOVE(tp, td_hash_le);
		da->unused_trie_cnt--;
	} else {
		tp = uma_zalloc(trie_zone, M_NOWAIT);
		if (tp == NULL)
			return (-1);
		LIST_INSERT_HEAD(&da->all_trie, tp, td_all_le);
		tp->td_index = da->all_trie_cnt++;
	}

	tp->td_hash = hash;
	tp->td_refcnt = 1;
	LIST_INSERT_HEAD(&da->trie_hashtbl[hash & TRIE_HASH_MASK], tp,
	   td_hash_le);
	memcpy(&da->x_tbl[tp->td_index << dxr_x],
	    &da->direct_tbl[index << dxr_x], sizeof(*da->x_tbl) << dxr_x);
	da->trietbl[index] = tp;
	if (da->all_trie_cnt >= da->xtbl_size >> dxr_x) {
		da->xtbl_size += XTBL_SIZE_INCR;
		da->x_tbl = realloc(da->x_tbl,
		    sizeof(*da->x_tbl) * da->xtbl_size, M_DXRAUX, M_NOWAIT);
		if (da->x_tbl == NULL)
			return (-1);
	}
	return(tp->td_index);
}

static void
trie_unref(struct dxr_aux *da, uint32_t index)
{
	struct trie_desc *tp = da->trietbl[index];

	if (tp == NULL)
		return;
	da->trietbl[index] = NULL;
	if (--tp->td_refcnt > 0)
		return;

	LIST_REMOVE(tp, td_hash_le);
	da->unused_trie_cnt++;
	if (tp->td_index != da->all_trie_cnt - 1) {
		LIST_INSERT_HEAD(&da->unused_trie, tp, td_hash_le);
		return;
	}

	do {
		da->all_trie_cnt--;
		da->unused_trie_cnt--;
		LIST_REMOVE(tp, td_all_le);
		uma_zfree(trie_zone, tp);
		LIST_FOREACH(tp, &da->unused_trie, td_hash_le)
			if (tp->td_index == da->all_trie_cnt - 1) {
				LIST_REMOVE(tp, td_hash_le);
				break;
			}
	} while (tp != NULL);
}
#endif

static void
heap_inject(struct dxr_aux *da, uint32_t start, uint32_t end, uint32_t preflen,
    uint32_t nh)
{
	struct heap_entry *fhp;
	int i;

	for (i = da->heap_index; i >= 0; i--) {
		if (preflen > da->heap[i].preflen)
			break;
		else if (preflen < da->heap[i].preflen)
			da->heap[i + 1] = da->heap[i];
		else
			return;
	}

	fhp = &da->heap[i + 1];
	fhp->preflen = preflen;
	fhp->start = start;
	fhp->end = end;
	fhp->nexthop = nh;
	da->heap_index++;
}

static int
dxr_walk(struct rtentry *rt, void *arg)
{
	struct dxr_aux *da = arg;
	uint32_t chunk = da->work_chunk;
	uint32_t first = chunk << DXR_RANGE_SHIFT;
	uint32_t last = first | DXR_RANGE_MASK;
	struct range_entry_long *fp =
	    &da->range_tbl[da->rtbl_top + da->rtbl_work_frags].re;
	struct heap_entry *fhp = &da->heap[da->heap_index];
	uint32_t preflen, nh, start, end, scopeid;
	struct in_addr addr;

	rt_get_inet_prefix_plen(rt, &addr, &preflen, &scopeid);
	start = ntohl(addr.s_addr);
	if (start > last)
		return (-1);	/* Beyond chunk boundaries, we are done */
	if (start < first)
		return (0);	/* Skip this route */

	end = start;
	if (preflen < 32)
		end |= (0xffffffffU >> preflen);
	nh = fib_get_nhop_idx(da->fd, rt_get_raw_nhop(rt));

	if (start == fhp->start)
		heap_inject(da, start, end, preflen, nh);
	else {
		/* start > fhp->start */
		while (start > fhp->end) {
			uint32_t oend = fhp->end;

			if (da->heap_index > 0) {
				fhp--;
				da->heap_index--;
			} else
				initheap(da, fhp->end + 1, chunk);
			if (fhp->end > oend && fhp->nexthop != fp->nexthop) {
				fp++;
				da->rtbl_work_frags++;
				fp->start = (oend + 1) & DXR_RANGE_MASK;
				fp->nexthop = fhp->nexthop;
			}
		}
		if (start > ((chunk << DXR_RANGE_SHIFT) | fp->start) &&
		    nh != fp->nexthop) {
			fp++;
			da->rtbl_work_frags++;
			fp->start = start & DXR_RANGE_MASK;
		} else if (da->rtbl_work_frags) {
			if ((--fp)->nexthop == nh)
				da->rtbl_work_frags--;
			else
				fp++;
		}
		fp->nexthop = nh;
		heap_inject(da, start, end, preflen, nh);
	}

	return (0);
}

static int
update_chunk(struct dxr_aux *da, uint32_t chunk)
{
	struct range_entry_long *fp;
#if DXR_TRIE_BITS < 24
	struct range_entry_short *fps;
	uint32_t start, nh, i;
#endif
	struct heap_entry *fhp;
	uint32_t first = chunk << DXR_RANGE_SHIFT;
	uint32_t last = first | DXR_RANGE_MASK;

	if (da->direct_tbl[chunk].fragments != FRAGS_MARK_HIT)
		chunk_unref(da, chunk);

	initheap(da, first, chunk);

	fp = &da->range_tbl[da->rtbl_top].re;
	da->rtbl_work_frags = 0;
	fp->start = first & DXR_RANGE_MASK;
	fp->nexthop = da->heap[0].nexthop;

	da->dst.sin_addr.s_addr = htonl(first);
	da->mask.sin_addr.s_addr = htonl(~DXR_RANGE_MASK);

	da->work_chunk = chunk;
	rib_walk_from(da->fibnum, AF_INET, RIB_FLAG_LOCKED,
	    (struct sockaddr *) &da->dst, (struct sockaddr *) &da->mask,
	    dxr_walk, da);

	/* Flush any remaining objects on the heap */
	fp = &da->range_tbl[da->rtbl_top + da->rtbl_work_frags].re;
	fhp = &da->heap[da->heap_index];
	while (fhp->preflen > DXR_TRIE_BITS) {
		uint32_t oend = fhp->end;

		if (da->heap_index > 0) {
			fhp--;
			da->heap_index--;
		} else
			initheap(da, fhp->end + 1, chunk);
		if (fhp->end > oend && fhp->nexthop != fp->nexthop) {
			/* Have we crossed the upper chunk boundary? */
			if (oend >= last)
				break;
			fp++;
			da->rtbl_work_frags++;
			fp->start = (oend + 1) & DXR_RANGE_MASK;
			fp->nexthop = fhp->nexthop;
		}
	}

	/* Direct hit if the chunk contains only a single fragment */
	if (da->rtbl_work_frags == 0) {
		da->direct_tbl[chunk].base = fp->nexthop;
		da->direct_tbl[chunk].fragments = FRAGS_MARK_HIT;
		return (0);
	}

	da->direct_tbl[chunk].base = da->rtbl_top;
	da->direct_tbl[chunk].fragments = da->rtbl_work_frags;

#if DXR_TRIE_BITS < 24
	/* Check whether the chunk can be more compactly encoded */
	fp = &da->range_tbl[da->rtbl_top].re;
	for (i = 0; i <= da->rtbl_work_frags; i++, fp++)
		if ((fp->start & 0xff) != 0 || fp->nexthop > RE_SHORT_MAX_NH)
			break;
	if (i == da->rtbl_work_frags + 1) {
		fp = &da->range_tbl[da->rtbl_top].re;
		fps = (void *) fp;
		for (i = 0; i <= da->rtbl_work_frags; i++, fp++, fps++) {
			start = fp->start;
			nh = fp->nexthop;
			fps->start = start >> 8;
			fps->nexthop = nh;
		}
		fps->start = start >> 8;
		fps->nexthop = nh;
		da->rtbl_work_frags >>= 1;
		da->direct_tbl[chunk].fragments =
		    da->rtbl_work_frags | FRAGS_PREF_SHORT;
	} else
#endif
	if (da->rtbl_work_frags >= FRAGS_MARK_HIT) {
		da->direct_tbl[chunk].fragments = FRAGS_MARK_XL;
		memmove(&da->range_tbl[da->rtbl_top + 1],
		   &da->range_tbl[da->rtbl_top],
		   (da->rtbl_work_frags + 1) * sizeof(*da->range_tbl));
		da->range_tbl[da->rtbl_top].fragments = da->rtbl_work_frags;
		da->rtbl_work_frags++;
	}
	da->rtbl_top += (da->rtbl_work_frags + 1);
	return (chunk_ref(da, chunk));
}

static void
dxr_build(struct dxr *dxr)
{
	struct dxr_aux *da = dxr->aux;
	struct chunk_desc *cdp;
	struct rib_rtable_info rinfo;
	struct timeval t0, t1, t2, t3;
	uint32_t r_size, dxr_tot_size;
	uint32_t i, m, range_rebuild = 0;
	uint32_t range_frag;
#ifdef DXR2
	struct trie_desc *tp;
	uint32_t d_tbl_size, dxr_x, d_size, x_size;
	uint32_t ti, trie_rebuild = 0, prev_size = 0;
	uint32_t trie_frag;
#endif

	KASSERT(dxr->d == NULL, ("dxr: d not free"));

	if (da == NULL) {
		da = malloc(sizeof(*dxr->aux), M_DXRAUX, M_NOWAIT);
		if (da == NULL)
			return;
		dxr->aux = da;
		da->fibnum = dxr->fibnum;
		da->refcnt = 1;
		LIST_INIT(&da->all_chunks);
		LIST_INIT(&da->all_trie);
		da->rtbl_size = RTBL_SIZE_INCR;
		da->range_tbl = NULL;
		da->xtbl_size = XTBL_SIZE_INCR;
		da->x_tbl = NULL;
		bzero(&da->dst, sizeof(da->dst));
		bzero(&da->mask, sizeof(da->mask));
		da->dst.sin_len = sizeof(da->dst);
		da->mask.sin_len = sizeof(da->mask);
		da->dst.sin_family = AF_INET;
		da->mask.sin_family = AF_INET;
	}
	if (da->range_tbl == NULL) {
		da->range_tbl = malloc(sizeof(*da->range_tbl) * da->rtbl_size
		    + FRAGS_PREF_SHORT, M_DXRAUX, M_NOWAIT);
		if (da->range_tbl == NULL)
			return;
		range_rebuild = 1;
	}
#ifdef DXR2
	if (da->x_tbl == NULL) {
		da->x_tbl = malloc(sizeof(*da->x_tbl) * da->xtbl_size,
		    M_DXRAUX, M_NOWAIT);
		if (da->x_tbl == NULL)
			return;
		trie_rebuild = 1;
	}
#endif
	da->fd = dxr->fd;

	microuptime(&t0);

	dxr->nh_tbl = fib_get_nhop_array(da->fd);
	fib_get_rtable_info(fib_get_rh(da->fd), &rinfo);

	if (da->updates_low > da->updates_high)
		range_rebuild = 1;

range_build:
	if (range_rebuild) {
		/* Bulk cleanup */
		bzero(da->chunk_hashtbl, sizeof(da->chunk_hashtbl));
		while ((cdp = LIST_FIRST(&da->all_chunks)) != NULL) {
			LIST_REMOVE(cdp, cd_all_le);
			uma_zfree(chunk_zone, cdp);
		}
		for (i = 0; i < UNUSED_BUCKETS; i++)
			LIST_INIT(&da->unused_chunks[i]);
		da->unused_chunks_size = 0;
		da->rtbl_top = 0;
		da->updates_low = 0;
		da->updates_high = DIRECT_TBL_SIZE - 1;
		memset(da->updates_mask, 0xff, sizeof(da->updates_mask));
		for (i = 0; i < DIRECT_TBL_SIZE; i++) {
			da->direct_tbl[i].fragments = FRAGS_MARK_HIT;
			da->direct_tbl[i].base = 0;
		}
	}
	da->prefixes = rinfo.num_prefixes;

	/* DXR: construct direct & range table */
	for (i = da->updates_low; i <= da->updates_high; i++) {
		m = da->updates_mask[i >> 5] >> (i & 0x1f);
		if (m == 0)
			i |= 0x1f;
		else if (m & 1 && update_chunk(da, i) != 0)
			return;
	}

	range_frag = 0;
	if (da->rtbl_top)
		range_frag = da->unused_chunks_size * 10000ULL / da->rtbl_top;
	if (range_frag > V_frag_limit) {
		range_rebuild = 1;
		goto range_build;
	}

	r_size = sizeof(*da->range_tbl) * da->rtbl_top;
	microuptime(&t1);

#ifdef DXR2
	if (range_rebuild ||
	    abs(fls(da->prefixes) - fls(da->trie_rebuilt_prefixes)) > 1)
		trie_rebuild = 1;

trie_build:
	if (trie_rebuild) {
		da->trie_rebuilt_prefixes = da->prefixes;
		da->d_bits = DXR_D;
		da->updates_low = 0;
		da->updates_high = DIRECT_TBL_SIZE - 1;
		if (!range_rebuild)
			memset(da->updates_mask, 0xff,
			    sizeof(da->updates_mask));
	}

dxr2_try_squeeze:
	if (trie_rebuild) {
		/* Bulk cleanup */
		bzero(da->trietbl, sizeof(da->trietbl));
		bzero(da->trie_hashtbl, sizeof(da->trie_hashtbl));
		while ((tp = LIST_FIRST(&da->all_trie)) != NULL) {
			LIST_REMOVE(tp, td_all_le);
			uma_zfree(trie_zone, tp);
		}
		LIST_INIT(&da->unused_trie);
		da->all_trie_cnt = da->unused_trie_cnt = 0;
	}

	/* Populate d_tbl, x_tbl */
	dxr_x = DXR_TRIE_BITS - da->d_bits;
	d_tbl_size = (1 << da->d_bits);

	for (i = da->updates_low >> dxr_x; i <= da->updates_high >> dxr_x;
	    i++) {
		if (!trie_rebuild) {
			m = 0;
			for (int j = 0; j < (1 << dxr_x); j += 32)
				m |= da->updates_mask[((i << dxr_x) + j) >> 5];
			if (m == 0)
				continue;
			trie_unref(da, i);
		}
		ti = trie_ref(da, i);
		if (ti < 0)
			return;
		da->d_tbl[i] = ti;
	}

	trie_frag = 0;
	if (da->all_trie_cnt)
		trie_frag = da->unused_trie_cnt * 10000ULL / da->all_trie_cnt;
	if (trie_frag > V_frag_limit) {
		trie_rebuild = 1;
		goto trie_build;
	}

	d_size = sizeof(*da->d_tbl) * d_tbl_size;
	x_size = sizeof(*da->x_tbl) * DIRECT_TBL_SIZE / d_tbl_size
	    * da->all_trie_cnt;
	dxr_tot_size = d_size + x_size + r_size;

	if (trie_rebuild == 1) {
		/* Try to find a more compact D/X split */
		if (prev_size == 0 || dxr_tot_size <= prev_size)
			da->d_bits--;
		else {
			da->d_bits++;
			trie_rebuild = 2;
		}
		prev_size = dxr_tot_size;
		goto dxr2_try_squeeze;
	}
	microuptime(&t2);
#else /* !DXR2 */
	dxr_tot_size = sizeof(da->direct_tbl) + r_size;
	t2 = t1;
#endif

	dxr->d = malloc(dxr_tot_size, M_DXRLPM, M_NOWAIT);
	if (dxr->d == NULL)
		return;
#ifdef DXR2
	memcpy(dxr->d, da->d_tbl, d_size);
	dxr->x = ((char *) dxr->d) + d_size;
	memcpy(dxr->x, da->x_tbl, x_size);
	dxr->r = ((char *) dxr->x) + x_size;
	dxr->d_shift = 32 - da->d_bits;
	dxr->x_shift = dxr_x;
	dxr->x_mask = 0xffffffffU >> (32 - dxr_x);
#else /* !DXR2 */
	memcpy(dxr->d, da->direct_tbl, sizeof(da->direct_tbl));
	dxr->r = ((char *) dxr->d) + sizeof(da->direct_tbl);
#endif
	memcpy(dxr->r, da->range_tbl, r_size);

	if (da->updates_low <= da->updates_high)
		bzero(&da->updates_mask[da->updates_low / 32],
		    (da->updates_high - da->updates_low) / 8 + 1);
	da->updates_low = DIRECT_TBL_SIZE - 1;
	da->updates_high = 0;
	microuptime(&t3);

#ifdef DXR2
	FIB_PRINTF(LOG_INFO, da->fd, "D%dX%dR, %d prefixes, %d nhops (max)",
	    da->d_bits, dxr_x, rinfo.num_prefixes, rinfo.num_nhops);
#else
	FIB_PRINTF(LOG_INFO, da->fd, "D%dR, %d prefixes, %d nhops (max)",
	    DXR_D, rinfo.num_prefixes, rinfo.num_nhops);
#endif
	i = dxr_tot_size * 100;
	if (rinfo.num_prefixes)
		i /= rinfo.num_prefixes;
	FIB_PRINTF(LOG_INFO, da->fd, "%d.%02d KBytes, %d.%02d Bytes/prefix",
	    dxr_tot_size / 1024, dxr_tot_size * 100 / 1024 % 100,
	    i / 100, i % 100);
#ifdef DXR2
	FIB_PRINTF(LOG_INFO, da->fd,
	    "%d.%02d%% trie, %d.%02d%% range fragmentation",
	    trie_frag / 100, trie_frag % 100,
	    range_frag / 100, range_frag % 100);
#else
	FIB_PRINTF(LOG_INFO, da->fd, "%d.%01d%% range fragmentation",
	    range_frag / 100, range_frag % 100);
#endif
	i = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
	FIB_PRINTF(LOG_INFO, da->fd, "range table %s in %u.%03u ms",
	    range_rebuild ? "rebuilt" : "updated", i / 1000, i % 1000);
#ifdef DXR2
	i = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
	FIB_PRINTF(LOG_INFO, da->fd, "trie %s in %u.%03u ms",
	    trie_rebuild ? "rebuilt" : "updated", i / 1000, i % 1000);
#endif
	i = (t3.tv_sec - t2.tv_sec) * 1000000 + t3.tv_usec - t2.tv_usec;
	FIB_PRINTF(LOG_INFO, da->fd, "snapshot forked in %u.%03u ms",
	    i / 1000, i % 1000);
}

/*
 * Glue functions for attaching to FreeBSD 13 fib_algo infrastructure.
 */

static struct nhop_object *
dxr_fib_lookup(void *algo_data, const struct flm_lookup_key key,
    uint32_t scopeid)
{
	struct dxr *dxr = algo_data;

	return (dxr->nh_tbl[dxr_lookup(dxr, ntohl(key.addr4.s_addr))]);
}

static enum flm_op_result
dxr_init(uint32_t fibnum, struct fib_data *fd, void *old_data, void **data)
{
	struct dxr *old_dxr = old_data;
	struct dxr_aux *da = NULL;
	struct dxr *dxr;

	dxr = malloc(sizeof(*dxr), M_DXRAUX, M_NOWAIT);
	if (dxr == NULL)
		return (FLM_REBUILD);

	/* Check whether we may reuse the old auxiliary structures */
	if (old_dxr != NULL && old_dxr->aux != NULL) {
		da = old_dxr->aux;
		atomic_add_int(&da->refcnt, 1);
	}

	dxr->aux = da;
	dxr->d = NULL;
	dxr->fd = fd;
	dxr->fibnum = fibnum;
	*data = dxr;

	return (FLM_SUCCESS);
}

static void
dxr_destroy(void *data)
{
	struct dxr *dxr = data;
	struct dxr_aux *da;
	struct chunk_desc *cdp;
	struct trie_desc *tp;

	if (dxr->d != NULL)
		free(dxr->d, M_DXRLPM);

	da = dxr->aux;
	free(dxr, M_DXRAUX);

	if (da == NULL || atomic_fetchadd_int(&da->refcnt, -1) > 1)
		return;

	/* Release auxiliary structures */
	while ((cdp = LIST_FIRST(&da->all_chunks)) != NULL) {
		LIST_REMOVE(cdp, cd_all_le);
		uma_zfree(chunk_zone, cdp);
	}
	while ((tp = LIST_FIRST(&da->all_trie)) != NULL) {
		LIST_REMOVE(tp, td_all_le);
		uma_zfree(trie_zone, tp);
	}
	free(da->range_tbl, M_DXRAUX);
	free(da->x_tbl, M_DXRAUX);
	free(da, M_DXRAUX);
}

static void 
epoch_dxr_destroy(epoch_context_t ctx)
{
	struct dxr *dxr = __containerof(ctx, struct dxr, epoch_ctx);

	dxr_destroy(dxr);
}

static void *
choose_lookup_fn(struct dxr_aux *da)
{

#ifdef DXR2
	switch (da->d_bits) {
#if DXR_TRIE_BITS > 16
	case 16:
		return (dxr_fib_lookup_16);
#endif
	case 15:
		return (dxr_fib_lookup_15);
	case 14:
		return (dxr_fib_lookup_14);
	case 13:
		return (dxr_fib_lookup_13);
	case 12:
		return (dxr_fib_lookup_12);
	case 11:
		return (dxr_fib_lookup_11);
	case 10:
		return (dxr_fib_lookup_10);
	case 9:
		return (dxr_fib_lookup_9);
	}
#endif /* DXR2 */
	return (dxr_fib_lookup);
}

static enum flm_op_result
dxr_dump_end(void *data, struct fib_dp *dp)
{
	struct dxr *dxr = data;
	struct dxr_aux *da;

	dxr_build(dxr);

	da = dxr->aux;
	if (da == NULL)
		return (FLM_REBUILD);

	/* Structural limit exceeded, hard error */
	if (da->rtbl_top >= BASE_MAX)
		return (FLM_ERROR);

	/* A malloc(,, M_NOWAIT) failed somewhere, retry later */
	if (dxr->d == NULL)
		return (FLM_REBUILD);

	dp->f = choose_lookup_fn(da);
	dp->arg = dxr;

	return (FLM_SUCCESS);
}

static enum flm_op_result
dxr_dump_rib_item(struct rtentry *rt, void *data)
{
	
	return (FLM_SUCCESS);
}

static enum flm_op_result
dxr_change_rib_item(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *data)
{

	return (FLM_BATCH);
}

static enum flm_op_result
dxr_change_rib_batch(struct rib_head *rnh, struct fib_change_queue *q,
    void *data)
{
	struct dxr *dxr = data;
	struct dxr *new_dxr;
	struct dxr_aux *da;
	struct fib_dp new_dp;
	enum flm_op_result res;
	uint32_t ip, plen, hmask, start, end, i, ui;
#ifdef INVARIANTS
	struct rib_rtable_info rinfo;
	int update_delta = 0;
#endif

	KASSERT(data != NULL, ("%s: NULL data", __FUNCTION__));
	KASSERT(q != NULL, ("%s: NULL q", __FUNCTION__));
	KASSERT(q->count < q->size, ("%s: q->count %d q->size %d",
	    __FUNCTION__, q->count, q->size));

	da = dxr->aux;
	KASSERT(da != NULL, ("%s: NULL dxr->aux", __FUNCTION__));

	FIB_PRINTF(LOG_INFO, da->fd, "processing %d update(s)", q->count);
	for (ui = 0; ui < q->count; ui++) {
#ifdef INVARIANTS
		if (q->entries[ui].nh_new != NULL)
			update_delta++;
		if (q->entries[ui].nh_old != NULL)
			update_delta--;
#endif
		plen = q->entries[ui].plen;
		ip = ntohl(q->entries[ui].addr4.s_addr);
		if (plen < 32)
			hmask = 0xffffffffU >> plen;
		else
			hmask = 0;
		start = (ip & ~hmask) >> DXR_RANGE_SHIFT;
		end = (ip | hmask) >> DXR_RANGE_SHIFT;

		if ((start & 0x1f) == 0 && (end & 0x1f) == 0x1f)
			for (i = start >> 5; i <= end >> 5; i++)
				da->updates_mask[i] = 0xffffffffU;
		else
			for (i = start; i <= end; i++)
				da->updates_mask[i >> 5] |= (1 << (i & 0x1f));
		if (start < da->updates_low)
			da->updates_low = start;
		if (end > da->updates_high)
			da->updates_high = end;
	}

#ifdef INVARIANTS
	fib_get_rtable_info(fib_get_rh(da->fd), &rinfo);
	KASSERT(da->prefixes + update_delta == rinfo.num_prefixes,
	    ("%s: update count mismatch", __FUNCTION__));
#endif

	res = dxr_init(0, dxr->fd, data, (void **) &new_dxr);
	if (res != FLM_SUCCESS)
		return (res);

	dxr_build(new_dxr);

	/* Structural limit exceeded, hard error */
	if (da->rtbl_top >= BASE_MAX) {
		dxr_destroy(new_dxr);
		return (FLM_ERROR);
	}

	/* A malloc(,, M_NOWAIT) failed somewhere, retry later */
	if (new_dxr->d == NULL) {
		dxr_destroy(new_dxr);
		return (FLM_REBUILD);
	}

	new_dp.f = choose_lookup_fn(da);
	new_dp.arg = new_dxr;
	if (fib_set_datapath_ptr(dxr->fd, &new_dp)) {
		fib_set_algo_ptr(dxr->fd, new_dxr);
		fib_epoch_call(epoch_dxr_destroy, &dxr->epoch_ctx);
		return (FLM_SUCCESS);
	}

	dxr_destroy(new_dxr);
	return (FLM_REBUILD);
}

static uint8_t
dxr_get_pref(const struct rib_rtable_info *rinfo)
{

	/* Below bsearch4 up to 10 prefixes. Always supersedes dpdk_lpm4. */
	return (251);
}

SYSCTL_DECL(_net_route_algo);
SYSCTL_NODE(_net_route_algo, OID_AUTO, dxr, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "DXR tunables");

static int
sysctl_dxr_frag_limit(SYSCTL_HANDLER_ARGS)
{
	char buf[8];
	int error, new, i;

	snprintf(buf, sizeof(buf), "%d.%02d%%", V_frag_limit / 100,
	    V_frag_limit % 100);
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (!isdigit(*buf) && *buf != '.')
		return (EINVAL);
	for (i = 0, new = 0; isdigit(buf[i]) && i < sizeof(buf); i++)
		new = new * 10 + buf[i] - '0';
	new *= 100;
	if (buf[i++] == '.') {
		if (!isdigit(buf[i]))
			return (EINVAL);
		new += (buf[i++] - '0') * 10;
		if (isdigit(buf[i]))
			new += buf[i++] - '0';
	}
	if (new > 1000)
		return (EINVAL);
	V_frag_limit = new;
	snprintf(buf, sizeof(buf), "%d.%02d%%", V_frag_limit / 100,
	    V_frag_limit % 100);
	return (0);
}

SYSCTL_PROC(_net_route_algo_dxr, OID_AUTO, frag_limit,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_VNET,
    0, 0, sysctl_dxr_frag_limit, "A",
    "Fragmentation threshold to full rebuild");

static struct fib_lookup_module fib_dxr_mod = {
	.flm_name = "dxr",
	.flm_family = AF_INET,
	.flm_init_cb = dxr_init,
	.flm_destroy_cb = dxr_destroy,
	.flm_dump_rib_item_cb = dxr_dump_rib_item,
	.flm_dump_end_cb = dxr_dump_end,
	.flm_change_rib_item_cb = dxr_change_rib_item,
	.flm_change_rib_items_cb = dxr_change_rib_batch,
	.flm_get_pref = dxr_get_pref,
};

static int
dxr_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		chunk_zone = uma_zcreate("dxr chunk", sizeof(struct chunk_desc),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		trie_zone = uma_zcreate("dxr trie", sizeof(struct trie_desc),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		fib_module_register(&fib_dxr_mod);
		return(0);
	case MOD_UNLOAD:
		error = fib_module_unregister(&fib_dxr_mod);
		if (error)
			return (error);
		uma_zdestroy(chunk_zone);
		uma_zdestroy(trie_zone);
		return(0);
	default:
		return(EOPNOTSUPP);
	}
}

static moduledata_t dxr_mod = {"fib_dxr", dxr_modevent, 0};

DECLARE_MODULE(fib_dxr, dxr_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(fib_dxr, 1);
