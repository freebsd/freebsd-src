/**************************************************************************
 *
 * Copyright (c) 2007, Kip Macy kmacy@freebsd.org
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. The name of Kip Macy nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 ***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/ktr.h>
#include <sys/sf_buf.h>

#include <machine/bus.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#else
#include <dev/cxgb/cxgb_include.h>
#endif

#include "opt_zero.h"

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#ifdef INVARIANTS
#define M_SANITY m_sanity
#else
#define M_SANITY(a, b)
#endif

#define MAX_BUFS 36
#define MAX_HVEC 8

extern uint32_t collapse_free;
extern uint32_t mb_free_vec_free;

struct mbuf_ext {
	struct mbuf    *me_m;
	caddr_t         me_base;
	volatile u_int *me_refcnt;
	int             me_flags;
	uint32_t        me_offset;
};

int
_m_explode(struct mbuf *m) 
{
        int i, offset, type, first, len;
        uint8_t *cl;
        struct mbuf *m0, *head = NULL;
        struct mbuf_vec *mv;

#ifdef INVARIANTS
	len = m->m_len;
	m0 = m->m_next;
	while (m0) {
		KASSERT((m0->m_flags & M_PKTHDR) == 0,
		    ("pkthdr set on intermediate mbuf - pre"));
		len += m0->m_len;
		m0 = m0->m_next;
		
	}
	if (len != m->m_pkthdr.len)
		panic("at start len=%d pktlen=%d", len, m->m_pkthdr.len);
#endif
        mv = mtomv(m);
	first = mv->mv_first;
        for (i = mv->mv_count + first - 1; i > first; i--) {
		type = mbuf_vec_get_type(mv, i);
                cl = mv->mv_vec[i].mi_base;
		offset = mv->mv_vec[i].mi_offset;
		len = mv->mv_vec[i].mi_len;
		if (__predict_false(type == EXT_MBUF)) {
			m0 = (struct mbuf *)cl;
			KASSERT((m0->m_flags & M_EXT) == 0, ("M_EXT set on mbuf"));
			m0->m_len = len;
			m0->m_data = cl + offset;
			goto skip_cluster;

		} else if ((m0 = m_get(M_NOWAIT, MT_DATA)) == NULL) {
			/*
			 * Check for extra memory leaks
			 */
			m_freem(head); 
			return (ENOMEM);
                } 
		m0->m_flags = 0;

		m_cljset(m0, (uint8_t *)cl, type);
		m0->m_len = mv->mv_vec[i].mi_len;
		if (offset)
			m_adj(m0, offset);
	skip_cluster:		
		m0->m_next = head;
		m->m_len -= m0->m_len;
		head = m0;
	}
	offset = mv->mv_vec[first].mi_offset;
	cl = mv->mv_vec[first].mi_base;
	type = mbuf_vec_get_type(mv, first);
	m->m_flags &= ~(M_IOVEC);
	m_cljset(m, cl, type);
	if (offset)
		m_adj(m, offset);
	m->m_next = head;
	head = m;
	M_SANITY(m, 0);

	return (0);
} 

static __inline int
m_vectorize(struct mbuf *m, int max, struct mbuf **vec, int *count)
{
	int i, error = 0;

	for (i = 0; i < max; i++) {
		if (m == NULL)
			break;
#ifndef MBUF_PACKET_ZONE_DISABLE
		if ((m->m_flags & M_EXT) && (m->m_ext.ext_type == EXT_PACKET))
			return (EINVAL);
#endif
#ifdef ZERO_COPY_SOCKETS
		if ((m->m_flags & M_EXT) && (m->m_ext.ext_type == EXT_SFBUF))		
			return (EINVAL);
#endif
		M_SANITY(m, 0);
		vec[i] = m;
		m = m->m_next;
	}
	if (m)
		error = EFBIG;

	*count = i;

	return (error);
}

static __inline int
m_findmbufs(struct mbuf **ivec, int maxbufs, struct mbuf_ext *ovec, int osize, int *ocount)
{
	int i, j, nhbufsneed, nhbufs;
	struct mbuf *m;
	
	nhbufsneed = min(((maxbufs - 1)/MAX_MBUF_IOV) + 1, osize);
	ovec[0].me_m = NULL;
	
	for (nhbufs = j = i = 0; i < maxbufs && nhbufs < nhbufsneed; i++) {
		if ((ivec[i]->m_flags & M_EXT) == 0)
			continue;
		m = ivec[i];
		ovec[nhbufs].me_m = m;
		ovec[nhbufs].me_base = m->m_ext.ext_buf;
		ovec[nhbufs].me_refcnt = m->m_ext.ref_cnt;
		ovec[nhbufs].me_offset = (m->m_data - m->m_ext.ext_buf);
		ovec[nhbufs].me_flags = m->m_ext.ext_type;
		nhbufs++;
	}
	if (nhbufs == 0) {
		if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL) 
			goto m_getfail;
		ovec[nhbufs].me_m = m;
		nhbufs = 1;
	}
	while (nhbufs < nhbufsneed) {
		if ((m = m_get(M_NOWAIT, MT_DATA)) == NULL) 
			goto m_getfail;
		ovec[nhbufs].me_m = m;
		nhbufs++;
	}
	/* 
	 * Copy over packet header to new head of chain
	 */
	if (ovec[0].me_m != ivec[0]) {
		ovec[0].me_m->m_flags |= M_PKTHDR;
		memcpy(&ovec[0].me_m->m_pkthdr, &ivec[0]->m_pkthdr, sizeof(struct pkthdr));
		SLIST_INIT(&ivec[0]->m_pkthdr.tags);
	}
	*ocount = nhbufs;
	return (0);
m_getfail:
	for (i = 0; i < nhbufs; i++)
		if ((ovec[i].me_m->m_flags & M_EXT) == 0)
			uma_zfree(zone_mbuf, ovec[i].me_m);
	return (ENOMEM);
	
}

static __inline void
m_setiovec(struct mbuf_iovec *mi, struct mbuf *m, struct mbuf_ext *extvec, int *me_index,
    int max_me_index)
{
	int idx = *me_index;
	
	mi->mi_len = m->m_len;
	if (idx < max_me_index && extvec[idx].me_m == m) {
		struct mbuf_ext *me = &extvec[idx];
		(*me_index)++;
		mi->mi_base = me->me_base;
		mi->mi_refcnt = me->me_refcnt;
		mi->mi_offset = me->me_offset;
		mi->mi_flags = me->me_flags;
	} else if (m->m_flags & M_EXT) {
		mi->mi_base = m->m_ext.ext_buf;
		mi->mi_refcnt = m->m_ext.ref_cnt;
		mi->mi_offset =
		    (m->m_data - m->m_ext.ext_buf);
		mi->mi_flags = m->m_ext.ext_type;
	} else {
		KASSERT(m->m_len < 256, ("mbuf too large len=%d",
			m->m_len));
		mi->mi_base = (uint8_t *)m;
		mi->mi_refcnt = NULL;
		mi->mi_offset =
		    (m->m_data - (caddr_t)m);
		mi->mi_flags = EXT_MBUF;
	}
	DPRINTF("type=%d len=%d refcnt=%p cl=%p offset=0x%x\n",
	    mi->mi_flags, mi->mi_len, mi->mi_refcnt, mi->mi_base,
	    mi->mi_offset);
}

int
_m_collapse(struct mbuf *m, int maxbufs, struct mbuf **mnew)
{
	struct mbuf *m0, *lmvec[MAX_BUFS];
	struct mbuf **mnext;
	struct mbuf **vec = lmvec;
	struct mbuf *mhead = NULL;
	struct mbuf_vec *mv;
	int err, i, j, max, len, nhbufs;
	struct mbuf_ext dvec[MAX_HVEC];
	int hidx = 0, dvecidx;

	M_SANITY(m, 0);
	if (maxbufs > MAX_BUFS) {
		if ((vec = malloc(maxbufs * sizeof(struct mbuf *),
			    M_DEVBUF, M_NOWAIT)) == NULL)
			return (ENOMEM);
	}

	if ((err = m_vectorize(m, maxbufs, vec, &max)) != 0)
		goto out;
	if ((err = m_findmbufs(vec, max, dvec, MAX_HVEC, &nhbufs)) != 0)
		goto out;

	KASSERT(max > 0, ("invalid mbuf count"));
	KASSERT(nhbufs > 0, ("invalid header mbuf count"));

	mhead = m0 = dvec[0].me_m;
	
	DPRINTF("nbufs=%d nhbufs=%d\n", max, nhbufs);
	for (hidx = dvecidx = i = 0, mnext = NULL; i < max; hidx++) {
		m0 = dvec[hidx].me_m;
		m0->m_flags &= ~M_EXT;
		m0->m_flags |= M_IOVEC;
		
		if (mnext) 
			*mnext = m0;
			
		mv = mtomv(m0);
		len = mv->mv_first = 0;
		for (j = 0; j < MAX_MBUF_IOV && i < max; j++, i++) {
			struct mbuf_iovec *mi = &mv->mv_vec[j];

			m_setiovec(mi, vec[i], dvec, &dvecidx, nhbufs);
			len += mi->mi_len;
		}
		m0->m_data = mv->mv_vec[0].mi_base + mv->mv_vec[0].mi_offset;
		mv->mv_count = j;
		m0->m_len = len;
		mnext = &m0->m_next;
		DPRINTF("count=%d len=%d\n", j, len);
	}
	
	/*
	 * Terminate chain
	 */
	m0->m_next = NULL;
	
	/*
	 * Free all mbufs not used by the mbuf iovec chain
	 */
	for (i = 0; i < max; i++) 
		if (vec[i]->m_flags & M_EXT) {
			vec[i]->m_flags &= ~M_EXT;
			collapse_free++;
			uma_zfree(zone_mbuf, vec[i]);
		}
	
	*mnew = mhead;
out:
	if (vec != lmvec)
		free(vec, M_DEVBUF);
	return (err);
}

void
mb_free_vec(struct mbuf *m)
{
	struct mbuf_vec *mv;
	int i;
	
	KASSERT((m->m_flags & (M_EXT|M_IOVEC)) == M_IOVEC,
	    ("%s: M_EXT set", __func__));

	mv = mtomv(m);
	KASSERT(mv->mv_count <= MAX_MBUF_IOV,
	    ("%s: mi_count too large %d", __func__, mv->mv_count));

	DPRINTF("count=%d len=%d\n", mv->mv_count, m->m_len);
	for (i = mv->mv_first; i < mv->mv_count; i++) {
		uma_zone_t zone = NULL;
		volatile int *refcnt = mv->mv_vec[i].mi_refcnt;
		int type = mbuf_vec_get_type(mv, i);
		void *cl = mv->mv_vec[i].mi_base;

		if ((type != EXT_MBUF) && *refcnt != 1 &&
		    atomic_fetchadd_int(refcnt, -1) != 1)
			continue;

		DPRINTF("freeing idx=%d refcnt=%p type=%d cl=%p\n", i, refcnt, type, cl);
		switch (type) {
		case EXT_MBUF:
			mb_free_vec_free++;
		case EXT_CLUSTER:
		case EXT_JUMBOP:
		case EXT_JUMBO9:  
		case EXT_JUMBO16:
			zone = m_getzonefromtype(type);
			uma_zfree(zone, cl);
			continue;
		case EXT_SFBUF:
			*refcnt = 0;
			uma_zfree(zone_ext_refcnt, __DEVOLATILE(u_int *,
				refcnt));
#ifdef __i386__
			sf_buf_mext(cl, mv->mv_vec[i].mi_args);
#else
			/*
			 * Every architecture other than i386 uses a vm_page
			 * for an sf_buf (well ... sparc64 does but shouldn't)
			 */
			sf_buf_mext(cl, PHYS_TO_VM_PAGE(vtophys(cl)));
#endif			
			continue;
		default:
			KASSERT(m->m_ext.ext_type == 0,
				("%s: unknown ext_type", __func__));
			break;
		}
	}
	/*
	 * Free this mbuf back to the mbuf zone with all iovec
	 * information purged.
	 */
	mb_free_vec_free++;
	uma_zfree(zone_mbuf, m);
}

#if (!defined(__sparc64__) && !defined(__sun4v__))
#include <sys/sysctl.h>

#define BUS_DMA_COULD_BOUNCE	BUS_DMA_BUS3
#define BUS_DMA_MIN_ALLOC_COMP	BUS_DMA_BUS4

struct bounce_zone {
	STAILQ_ENTRY(bounce_zone) links;
	STAILQ_HEAD(bp_list, bounce_page) bounce_page_list;
	int		total_bpages;
	int		free_bpages;
	int		reserved_bpages;
	int		active_bpages;
	int		total_bounced;
	int		total_deferred;
	bus_size_t	alignment;
	bus_size_t	boundary;
	bus_addr_t	lowaddr;
	char		zoneid[8];
	char		lowaddrid[20];
	struct sysctl_ctx_list sysctl_tree;
	struct sysctl_oid *sysctl_tree_top;
};
struct bus_dma_tag {
	bus_dma_tag_t	  parent;
	bus_size_t	  alignment;
	bus_size_t	  boundary;
	bus_addr_t	  lowaddr;
	bus_addr_t	  highaddr;
	bus_dma_filter_t *filter;
	void		 *filterarg;
	bus_size_t	  maxsize;
	u_int		  nsegments;
	bus_size_t	  maxsegsz;
	int		  flags;
	int		  ref_count;
	int		  map_count;
	bus_dma_lock_t	 *lockfunc;
	void		 *lockfuncarg;
	bus_dma_segment_t *segments;
	struct bounce_zone *bounce_zone;
};

struct bus_dmamap {
	struct bp_list	       bpages;
	int		       pagesneeded;
	int		       pagesreserved;
	bus_dma_tag_t	       dmat;
	void		      *buf;		/* unmapped buffer pointer */
	bus_size_t	       buflen;		/* unmapped buffer length */
	bus_dmamap_callback_t *callback;
	void		      *callback_arg;
	STAILQ_ENTRY(bus_dmamap) links;
};

static struct bus_dmamap nobounce_dmamap;

static __inline int
run_filter(bus_dma_tag_t dmat, bus_addr_t paddr)
{
	int retval;

	retval = 0;

	do {
		if (((paddr > dmat->lowaddr && paddr <= dmat->highaddr)
		 || ((paddr & (dmat->alignment - 1)) != 0))
		 && (dmat->filter == NULL
		  || (*dmat->filter)(dmat->filterarg, paddr) != 0))
			retval = 1;

		dmat = dmat->parent;		
	} while (retval == 0 && dmat != NULL);
	return (retval);
}

static __inline int
_bus_dmamap_load_buffer(bus_dma_tag_t dmat,
    			bus_dmamap_t map,
			void *buf, bus_size_t buflen,
			pmap_t pmap,
			int flags,
			bus_addr_t *lastaddrp,
			bus_dma_segment_t *segs,
			int *segp,
			int first)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vm_offset_t vaddr;
	int needbounce = 0;
	int seg;

	if (map == NULL)
		map = &nobounce_dmamap;

	/* Reserve Necessary Bounce Pages */
	if (map->pagesneeded != 0)
		panic("don't support bounce pages");

	vaddr = (vm_offset_t)buf;
	lastaddr = *lastaddrp;
	bmask = ~(dmat->boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap)
			curaddr = pmap_extract(pmap, vaddr);
		else
			curaddr = pmap_kextract(vaddr);


		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (dmat->boundary > 0) {
			baddr = (curaddr + dmat->boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		if (map->pagesneeded != 0 && run_filter(dmat, curaddr))
			panic("no bounce page support");
		
		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			segs[seg].ds_addr = curaddr;
			segs[seg].ds_len = sgsize;
			first = 0;
		} else {
			if (needbounce == 0 && curaddr == lastaddr &&
			    (segs[seg].ds_len + sgsize) <= dmat->maxsegsz &&
			    (dmat->boundary == 0 ||
			     (segs[seg].ds_addr & bmask) == (curaddr & bmask)))
				segs[seg].ds_len += sgsize;
			else {
				if (++seg >= dmat->nsegments)
					break;
				segs[seg].ds_addr = curaddr;
				segs[seg].ds_len = sgsize;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	return (buflen != 0 ? EFBIG : 0); /* XXX better return value here? */
}

int
bus_dmamap_load_mvec_sg(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
                        bus_dma_segment_t *segs, int *nsegs, int flags)
{
	int error, i;

	M_ASSERTPKTHDR(m0);
	
	if ((m0->m_flags & M_IOVEC) == 0)
		return (bus_dmamap_load_mbuf_sg(dmat, map, m0, segs, nsegs, flags));
	
	flags |= BUS_DMA_NOWAIT;
	*nsegs = 0;
	error = 0;
	if (m0->m_pkthdr.len <= dmat->maxsize) {
		int first = 1;
		bus_addr_t lastaddr = 0;
		struct mbuf *m;
		
		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			struct mbuf_vec *mv;
			int count, firstcl;
			if (!(m->m_len > 0))
				continue;
			
			mv = mtomv(m);
			count = mv->mv_count;
			firstcl = mv->mv_first;
			KASSERT(count <= MAX_MBUF_IOV, ("count=%d too large", count));
			for (i = firstcl; i < count && error == 0; i++) {
				void *data = mv->mv_vec[i].mi_base + mv->mv_vec[i].mi_offset;
				int len = mv->mv_vec[i].mi_len;

				if (len == 0)
					continue;
				DPRINTF("mapping data=%p len=%d\n", data, len); 
				error = _bus_dmamap_load_buffer(dmat, NULL, 
				    data, len, NULL, flags, &lastaddr,
				    segs, nsegs, first);
				DPRINTF("%d: addr=0x%jx len=%ju\n", i,
				    (uintmax_t)segs[i].ds_addr, (uintmax_t)segs[i].ds_len);
				first = 0;
			}
		}
	} else {
		error = EINVAL;
	}

	(*nsegs)++;

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat->flags, error, *nsegs);
	return (error);
}
#endif /* !__sparc64__  && !__sun4v__ */
