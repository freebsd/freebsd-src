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
 * $FreeBSD: src/sys/dev/cxgb/sys/mvec.h,v 1.6 2007/07/17 06:50:34 kmacy Exp $
 *
 ***************************************************************************/

#ifndef _MVEC_H_
#define _MVEC_H_

#define mtomv(m)          ((struct mbuf_vec *)((m)->m_pktdat))

#define M_IOVEC               0x100000 /* mbuf immediate data area is used for cluster ptrs */
#define MBUF_IOV_TYPE_MASK   ((1<<3)-1) 
#define mbuf_vec_set_type(mv, i, type) \
	(mv)->mv_vec[(i)].mi_flags = (((mv)->mv_vec[(i)].mi_flags \
		& ~MBUF_IOV_TYPE_MASK) | type)
 
#define mbuf_vec_get_type(mv, i) \
	((mv)->mv_vec[(i)].mi_flags & MBUF_IOV_TYPE_MASK)


struct mbuf_iovec {
	uint16_t mi_flags;     /* per-cluster flags          */ 
	uint16_t mi_len;       /* length of cluster          */ 
	uint32_t mi_offset;    /* data offsets into cluster  */ 
	uint8_t  *mi_base;     /* pointers to cluster        */
	volatile uint32_t *mi_refcnt;   /* refcnt for cluster*/
#ifdef __i386__
	void     *mi_args;      /* for sf_buf                 */
#endif	
};

#define MAX_MBUF_IOV          ((MHLEN-8)/sizeof(struct mbuf_iovec))
struct mbuf_vec {
	uint16_t mv_first;     /* first valid cluster        */
	uint16_t mv_count;     /* # of clusters              */
	uint32_t mv_flags;     /* flags for iovec            */
	struct mbuf_iovec mv_vec[MAX_MBUF_IOV];
};

int _m_explode(struct mbuf *);
int _m_collapse(struct mbuf *, int maxbufs, struct mbuf **);
void mb_free_vec(struct mbuf *m);

static __inline void 
m_iovinit(struct mbuf *m) 
{ 
	struct mbuf_vec *mv = mtomv(m); 

	mv->mv_first = mv->mv_count = 0;
	m->m_pkthdr.len = m->m_len = 0;
	m->m_flags |= M_IOVEC; 
} 
 
static __inline void 
m_iovappend(struct mbuf *m, uint8_t *cl, int size, int len, int offset)
{ 
	struct mbuf_vec *mv = mtomv(m);
	struct mbuf_iovec *iov;
	int idx = mv->mv_first + mv->mv_count; 

        KASSERT(idx <= MAX_MBUF_IOV, ("tried to append too many clusters to mbuf iovec")); 
	if ((m->m_flags & M_EXT) != 0) 
		panic("invalid flags in %s", __func__); 

        if (mv->mv_count == 0)
		m->m_data = cl + offset; 

        iov = &mv->mv_vec[idx];
	iov->mi_flags = m_gettype(size); 
        iov->mi_base = cl; 
        iov->mi_len = len; 
        iov->mi_offset = offset;
        m->m_pkthdr.len += len;
        m->m_len += len;
        mv->mv_count++;
} 

static __inline int
m_explode(struct mbuf *m)
{
	if ((m->m_flags & M_IOVEC) == 0)
		return (0);

	return _m_explode(m); 
} 
 
static __inline int
m_collapse(struct mbuf *m, int maxbufs, struct mbuf **mnew) 
{
#if (!defined(__sparc64__) && !defined(__sun4v__)) 	
	if (m->m_next == NULL)
#endif		
	{
		*mnew = m;
		return (0);
	}
	return _m_collapse(m, maxbufs, mnew);
} 

static __inline struct mbuf *
m_free_vec(struct mbuf *m)
{
	struct mbuf *n = m->m_next;

	if (m->m_flags & M_IOVEC)
		mb_free_vec(m);
	else if (m->m_flags & M_EXT)
		mb_free_ext(m);
	else
		uma_zfree(zone_mbuf, m);

	if (n)
		n->m_flags &= ~M_PKTHDR;
	
	return (n);
}

static __inline void 
m_freem_vec(struct mbuf *m)
{
	
	while (m != NULL) 
		m = m_free_vec(m);
}

static __inline uma_zone_t
m_getzonefromtype(int type)
{
	uma_zone_t zone;
	
	switch (type) {
	case EXT_MBUF:
		zone = zone_mbuf;
		break;
	case EXT_CLUSTER:
		zone = zone_clust;
		break;
#if MJUMPAGESIZE != MCLBYTES
	case EXT_JUMBOP:
		zone = zone_jumbop;
		break;
#endif
	case EXT_JUMBO9:
		zone = zone_jumbo9;
		break;
	case EXT_JUMBO16:
		zone = zone_jumbo16;
		break;
#ifndef PACKET_ZONE_DISABLED
	case EXT_PACKET:
		zone = zone_pack;
		break;
#endif		
	default:
		panic("%s: invalid cluster type %d", __func__, type);
	}
	return (zone);
}

#if (!defined(__sparc64__) && !defined(__sun4v__))
int
bus_dmamap_load_mvec_sg(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
                        bus_dma_segment_t *segs, int *nsegs, int flags);

#else
#define bus_dmamap_load_mvec_sg bus_dmamap_load_mbuf_sg
#endif

#endif
