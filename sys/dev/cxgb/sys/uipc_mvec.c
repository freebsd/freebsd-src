/**************************************************************************
 *
 * Copyright (c) 2007-2008, Kip Macy kmacy@freebsd.org
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

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>

#include <cxgb_include.h>
#include <sys/mvec.h>

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

uma_zone_t zone_miovec;
static int mi_inited = 0;
int cxgb_mbufs_outstanding = 0;
int cxgb_pack_outstanding = 0;

void
mi_init(void)
{
	if (mi_inited > 0)
		return;
	else
		mi_inited++;
	zone_miovec = uma_zcreate("MBUF IOVEC", MIOVBYTES,
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_MAXBUCKET);
}

void
mi_deinit(void)
{
	mi_inited--;
	if (mi_inited == 0)
		uma_zdestroy(zone_miovec);
}
	
void
dump_mi(struct mbuf_iovec *mi)
{
	int i;
	struct mbuf_vec *mv;
	
	printf("mi_flags=0x%08x mi_base=%p mi_data=%p mi_len=%d mi_type=%d\n",
	    mi->mi_flags, mi->mi_base, mi->mi_data, mi->mi_len, mi->mi_type);

	if (mi->mi_type == EXT_CLIOVEC ||
	    mi->mi_type == EXT_IOVEC) {
		mv = mtomv((struct mbuf *)mi->mi_base);
		mi = mv->mv_vec;
		for (i = 0; i < mv->mv_count; i++, mi++) 
			dump_mi(mi);

	}
}

static __inline struct mbuf *
_mcl_collapse_mbuf(struct mbuf_iovec *mi, struct mbuf *m)
{
	struct mbuf *n = m->m_next;

	prefetch(n);

	mi->mi_flags = m->m_flags;
	mi->mi_len = m->m_len;
	mi->mi_mbuf = NULL;
	
	if (m->m_flags & M_PKTHDR) {
		mi->mi_ether_vtag = m->m_pkthdr.ether_vtag;
		mi->mi_tso_segsz = m->m_pkthdr.tso_segsz;
#ifdef IFNET_MULTIQUEUE		
		mi->mi_rss_hash = m->m_pkthdr.rss_hash;
#endif		
		if(!SLIST_EMPTY(&m->m_pkthdr.tags)) 
			m_tag_delete_chain(m, NULL);
	}
	if (m->m_type != MT_DATA) {
		mi->mi_data = NULL;
		mi->mi_base = (caddr_t)m;
		/*
		 * XXX JMPIOVEC
		 */
		mi->mi_size = (m->m_type == EXT_CLIOVEC) ? MCLBYTES : MIOVBYTES;
		mi->mi_type = m->m_type;
		mi->mi_len = m->m_pkthdr.len;
		KASSERT(mi->mi_len, ("empty packet"));
		mi->mi_refcnt = NULL;
	} else if (m->m_flags & M_EXT) {
		memcpy(&mi->mi_ext, &m->m_ext, sizeof(struct m_ext_));
		mi->mi_data = m->m_data;
		mi->mi_base = m->m_ext.ext_buf;
		mi->mi_type = m->m_ext.ext_type;
		mi->mi_size = m->m_ext.ext_size;
		mi->mi_refcnt = m->m_ext.ref_cnt;
		if (m->m_ext.ext_type == EXT_PACKET) {
			mi->mi_mbuf = m;
#ifdef INVARIANTS
			cxgb_pack_outstanding++;
#endif
		}
	} else {
		mi->mi_base = (caddr_t)m;
		mi->mi_data = m->m_data;
		mi->mi_size = MSIZE;
		mi->mi_type = EXT_MBUF;
		mi->mi_refcnt = NULL;
#ifdef INVARIANTS
		cxgb_mbufs_outstanding++;
#endif		
	}
	KASSERT(mi->mi_len != 0, ("miov has len 0"));
	KASSERT(mi->mi_type > 0, ("mi_type is invalid"));
	KASSERT(mi->mi_base, ("mi_base is invalid"));
	return (n);
}

struct mbuf *
mi_collapse_mbuf(struct mbuf_iovec *mi, struct mbuf *m)
{
	return _mcl_collapse_mbuf(mi, m);
}
	
void *
mcl_alloc(int seg_count, int *type) 
{
	uma_zone_t zone;
	
	if (seg_count > MAX_CL_IOV) {
		zone = zone_jumbop;
		*type = EXT_JMPIOVEC;
	} else if (seg_count > MAX_MIOVEC_IOV) {
		zone = zone_clust;
		*type = EXT_CLIOVEC;
	} else {
		*type = EXT_IOVEC;
		zone = zone_miovec;
	}
	return uma_zalloc_arg(zone, NULL, M_NOWAIT);
}

int
busdma_map_sg_collapse(struct mbuf **m, bus_dma_segment_t *segs, int *nsegs)
{
	struct mbuf *m0, *mhead, *n = *m;
	struct mbuf_iovec *mi;
	struct mbuf *marray[TX_MAX_SEGS];
	int i, type, seg_count, defragged = 0, err = 0;
	struct mbuf_vec *mv;
	int skipped, freed;
	
	KASSERT(n->m_pkthdr.len, ("packet has zero header len"));
	if (n->m_pkthdr.len <= PIO_LEN)
		return (0);
retry:
	seg_count = 0;
	if (n->m_next == NULL) {
		busdma_map_mbuf_fast(n, segs);
		*nsegs = 1;
		return (0);
	}
	skipped = freed = 0;
	while (n && seg_count < TX_MAX_SEGS) {
		marray[seg_count] = n;
		
		/*
		 * firmware doesn't like empty segments
		 */
		if (__predict_true(n->m_len != 0))  
			seg_count++;
		else
			skipped++;

		n = n->m_next;
	}
	if (seg_count == 0) {
		if (cxgb_debug)
			printf("empty segment chain\n");
		err = EFBIG;
		goto err_out;
	}  else if (seg_count >= TX_MAX_SEGS) {
		if (cxgb_debug)
			printf("mbuf chain too long: %d max allowed %d\n",
			    seg_count, TX_MAX_SEGS);
		if (!defragged) {
			n = m_defrag(*m, M_DONTWAIT);
			if (n == NULL) {
				err = ENOBUFS;
				goto err_out;
			}
			*m = n;
			defragged = 1;
			goto retry;
		}
		err = EFBIG;
		goto err_out;
	}

	if ((m0 = mcl_alloc(seg_count, &type)) == NULL) {
		err = ENOMEM;
		goto err_out;
	}
	
	memcpy(m0, *m, sizeof(struct m_hdr) + sizeof(struct pkthdr));
	m0->m_type = type;
	KASSERT(m0->m_pkthdr.len, ("empty packet being marshalled"));
	mv = mtomv(m0);
	mv->mv_count = seg_count;
	mv->mv_first = 0;
	for (i = 0, mi = mv->mv_vec; i < seg_count; mi++, segs++, i++) {
		n = marray[i];
		busdma_map_mbuf_fast(n, segs);
		_mcl_collapse_mbuf(mi, n);
	}
	n = *m;
	while (n) {
		if (n->m_len == 0)
			/* do nothing - free if mbuf or cluster */; 
		else if ((n->m_flags & M_EXT) == 0) {
			goto skip;
		} else if ((n->m_flags & M_EXT) &&
		    (n->m_ext.ext_type == EXT_PACKET)) {
			goto skip;
		} else if (n->m_flags & M_NOFREE) 
			goto skip; 
		else if ((n->m_flags & (M_EXT|M_NOFREE)) == M_EXT)
			n->m_flags &= ~M_EXT; 
		mhead = n->m_next;
		m_free(n);
		n = mhead;
		freed++;
		continue;
	skip:
		/*
		 * is an immediate mbuf or is from the packet zone
		 */
		n = n->m_next;
	}
	*nsegs = seg_count;
	*m = m0;
	DPRINTF("pktlen=%d m0=%p *m=%p m=%p\n", m0->m_pkthdr.len, m0, *m, m);
	return (0);
err_out:
	m_freem(*m);
	*m = NULL;	
	return (err);
}

int 
busdma_map_sg_vec(struct mbuf **m, struct mbuf **mret,
    bus_dma_segment_t *segs, int pkt_count)
{
	struct mbuf *m0, **mp;
	struct mbuf_iovec *mi;
	struct mbuf_vec *mv;
	int i, type;

	if ((m0 = mcl_alloc(pkt_count, &type)) == NULL)
		return (ENOMEM);

	memcpy(m0, *m, sizeof(struct m_hdr) +
	    sizeof(struct pkthdr));
	m0->m_type = type;
	mv = mtomv(m0);
	mv->mv_count = pkt_count;
	mv->mv_first = 0;
	for (mp = m, i = 0, mi = mv->mv_vec; i < pkt_count;
	     mp++, segs++, mi++, i++) {
		busdma_map_mbuf_fast(*mp, segs);
		_mcl_collapse_mbuf(mi, *mp);
		KASSERT(mi->mi_len, ("empty packet"));
	}

	for (mp = m, i = 0; i < pkt_count; i++, mp++) {
		if ((((*mp)->m_flags & (M_EXT|M_NOFREE)) == M_EXT)
		    && ((*mp)->m_ext.ext_type != EXT_PACKET)) {
			(*mp)->m_flags &= ~M_EXT;
			m_free(*mp);
		}
	}

	*mret = m0;
	return (0);
}

void
mb_free_ext_fast(struct mbuf_iovec *mi, int type, int idx)
{
	int dofree;
	caddr_t cl;
	
	cl = mi->mi_base;
	switch (type) {
	case EXT_PACKET:
#ifdef INVARIANTS
		cxgb_pack_outstanding--;
#endif
		m_free(mi->mi_mbuf);
		return;
	case EXT_MBUF:
		KASSERT((mi->mi_flags & M_NOFREE) == 0, ("no free set on mbuf"));
#ifdef INVARIANTS
		cxgb_mbufs_outstanding--;
#endif
		m_free_fast((struct mbuf *)cl);
		return;
	default:
		break;
	}

	/* Account for lazy ref count assign. */
	dofree = (mi->mi_refcnt == NULL);
	if (dofree == 0) {
		    if (*(mi->mi_refcnt) == 1 ||
		    atomic_fetchadd_int(mi->mi_refcnt, -1) == 1)
			    dofree = 1;
	}
	if (dofree == 0)
		return;

	switch (type) {
	case EXT_CLUSTER:
		cxgb_cache_put(zone_clust, cl);
		break;		
	case EXT_JUMBOP:
		cxgb_cache_put(zone_jumbop, cl);
		break;		
	case EXT_JUMBO9:
		cxgb_cache_put(zone_jumbo9, cl);
		break;		
	case EXT_JUMBO16:
		cxgb_cache_put(zone_jumbo16, cl);
		break;
	case EXT_SFBUF:
	case EXT_NET_DRV:
	case EXT_MOD_TYPE:
	case EXT_DISPOSABLE:
		*(mi->mi_refcnt) = 0;
		uma_zfree(zone_ext_refcnt, __DEVOLATILE(u_int *,
			mi->mi_ext.ref_cnt));
		/* FALLTHROUGH */
	case EXT_EXTREF:
		KASSERT(mi->mi_ext.ext_free != NULL,
		    ("%s: ext_free not set", __func__));
#if __FreeBSD_version >= 800016
		(*(mi->mi_ext.ext_free))(mi->mi_ext.ext_arg1,
		    mi->mi_ext.ext_arg2);
#else
		(*(mi->mi_ext.ext_free))(mi->mi_ext.ext_buf,
		    mi->mi_ext.ext_args);
#endif
		break;
	default:
		dump_mi(mi);
		panic("unknown mv type in m_free_vec type=%d idx=%d", type, idx);
		break;
	}
}

int
_m_explode(struct mbuf *m)
{
	panic("IMPLEMENT ME!!!");
}
	

