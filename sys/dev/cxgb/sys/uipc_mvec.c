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

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>



#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#include <sys/mvec.h>
#else
#include <dev/cxgb/cxgb_include.h>
#include <dev/cxgb/sys/mvec.h>
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

uma_zone_t zone_miovec;
static int mi_inited = 0;

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
#ifdef IFNET_MULTIQ		
		mi->mi_rss_hash = m->m_pkthdr.rss_hash;
#endif		
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
		mi->mi_mbuf = m;
	} else {
		mi->mi_base = (caddr_t)m;
		mi->mi_data = m->m_data;
		mi->mi_size = MSIZE;
		mi->mi_type = EXT_MBUF;
		mi->mi_refcnt = NULL;
	}
	KASSERT(mi->mi_len != 0, ("miov has len 0"));
	KASSERT(mi->mi_type > 0, ("mi_type is invalid"));

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

	KASSERT(n->m_pkthdr.len, ("packet has zero header len"));
		
	if (n->m_flags & M_PKTHDR && !SLIST_EMPTY(&n->m_pkthdr.tags)) 
		m_tag_delete_chain(n, NULL);

retry:
	seg_count = 0;
	if (n->m_next == NULL) {
		busdma_map_mbuf_fast(n, segs);
		*nsegs = 1;

		return (0);
	}

	if (n->m_pkthdr.len <= 104) {
		caddr_t data;

		if ((m0 = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL) 
			return (ENOMEM);

		data = m0->m_data;
		memcpy(m0, n, sizeof(struct m_hdr) + sizeof(struct pkthdr));
		m0->m_data = data;
		m0->m_len = n->m_pkthdr.len;
		m0->m_flags &= ~M_EXT;
		m0->m_next = NULL;
		m0->m_type = n->m_type;
		n->m_flags &= ~M_PKTHDR;
		while (n) {
			memcpy(data, n->m_data, n->m_len);
			data += n->m_len;
			n = n->m_next;
		}
		m_freem(*m);
		n = m0;
		*m = n;
		DPRINTF("collapsed into immediate - list:%d\n", !SLIST_EMPTY(&m0->m_pkthdr.tags));
		goto retry;
	}
	
	while (n && seg_count < TX_MAX_SEGS) {
		marray[seg_count] = n;

		/*
		 * firmware doesn't like empty segments
		 */
		if (__predict_true(n->m_len != 0)) 
			seg_count++;

		n = n->m_next;
	}
#if 0
	/*
	 * XXX needs more careful consideration
	 */
	if (__predict_false(seg_count == 1)) {
		n = marray[0];
		if (n != *m)
			
		/* XXX */
		goto retry;
	}
#endif	
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
		if (((n->m_flags & (M_EXT|M_NOFREE)) == M_EXT) &&
		    (n->m_len > 0) && (n->m_ext.ext_type != EXT_PACKET) )
			n->m_flags &= ~M_EXT; 
		else if ((n->m_len > 0) || (n->m_ext.ext_type == EXT_PACKET)) {
			n = n->m_next;
			continue;
		}
		mhead = n->m_next;
		m_free(n);
		n = mhead;
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
busdma_map_sg_vec(struct mbuf **m, struct mbuf **mret, bus_dma_segment_t *segs, int count)
{
	struct mbuf *m0, **mp;
	struct mbuf_iovec *mi;
	struct mbuf_vec *mv;
	int i;
	
	if (count > MAX_MIOVEC_IOV) {
		if ((m0 = uma_zalloc_arg(zone_clust, NULL, M_NOWAIT)) == NULL) 
			return (ENOMEM);
		m0->m_type = EXT_CLIOVEC;
	} else {
		if ((m0 = uma_zalloc_arg(zone_miovec, NULL, M_NOWAIT)) == NULL)
			return (ENOMEM);
		m0->m_type = EXT_IOVEC;
	}

	m0->m_flags = 0;
	m0->m_pkthdr.len = m0->m_len = (*m)->m_len; /* not the real length but needs to be non-zero */
	mv = mtomv(m0);
	mv->mv_count = count;
	mv->mv_first = 0;
	for (mp = m, i = 0, mi = mv->mv_vec; i < count; mp++, segs++, mi++, i++) {
		if ((*mp)->m_flags & M_PKTHDR && !SLIST_EMPTY(&(*mp)->m_pkthdr.tags)) 
			m_tag_delete_chain(*mp, NULL);
		busdma_map_mbuf_fast(*mp, segs);
		_mcl_collapse_mbuf(mi, *mp);
		KASSERT(mi->mi_len, ("empty packet"));
	}

	for (mp = m, i = 0; i < count; i++, mp++) {
		(*mp)->m_next = (*mp)->m_nextpkt = NULL;
		if (((*mp)->m_flags & (M_EXT|M_NOFREE)) == M_EXT) {
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
	u_int cnt;
	int dofree;
	caddr_t cl;
	
	/* Account for lazy ref count assign. */
	dofree = (mi->mi_refcnt == NULL);

	/*
	 * This is tricky.  We need to make sure to decrement the
	 * refcount in a safe way but to also clean up if we're the
	 * last reference.  This method seems to do it without race.
	 */
	while (dofree == 0) {
		cnt = *(mi->mi_refcnt);
		if (mi->mi_type == EXT_PACKET) {
			dofree = 1;
			break;
		}
		if (atomic_cmpset_int(mi->mi_refcnt, cnt, cnt - 1)) {
			if (cnt == 1)
				dofree = 1;
			break;
		}
	}
	if (dofree == 0)
		return;

	cl = mi->mi_base;
	switch (type) {
	case EXT_MBUF:
		m_free_fast((struct mbuf *)cl);
		break;
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
		(*(mi->mi_ext.ext_free))(mi->mi_ext.ext_buf,
		    mi->mi_ext.ext_args);
		break;
	case EXT_PACKET:
		uma_zfree(zone_pack, mi->mi_mbuf);
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
	

