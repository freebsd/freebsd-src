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
 * $FreeBSD$
 *
 ***************************************************************************/

#ifndef _MVEC_H_
#define _MVEC_H_
#include <machine/bus.h>

int cxgb_cache_init(void);

void cxgb_cache_flush(void);

caddr_t cxgb_cache_get(uma_zone_t zone);

void cxgb_cache_put(uma_zone_t zone, void *cl);

void cxgb_cache_refill(void);

extern int cxgb_cached_allocations;
extern int cxgb_cached;
extern int cxgb_ext_freed;
extern int cxgb_mbufs_outstanding;
extern int cxgb_pack_outstanding;

#define	mtomv(m)          ((struct mbuf_vec *)((m)->m_pktdat))
#define	M_IOVEC		0x100000	/* mbuf immediate data area is used for cluster ptrs */
#define	M_DDP		0x200000	/* direct data placement mbuf */
#define	EXT_PHYS	10		/* physical/bus address  */

/*
 * duplication from mbuf.h - can't use directly because
 * m_ext is a define
 */
struct m_ext_ {
	caddr_t		 ext_buf;	/* start of buffer */
	void		(*ext_free)	/* free routine if not the usual */
			    (void *, void *);
#if __FreeBSD_version >= 800016
	void		*ext_arg1;	/* optional argument pointer */
	void		*ext_arg2;	/* optional argument pointer */
#else
	void		*ext_args;	/* optional argument pointer */
#endif
	u_int		 ext_size;	/* size of buffer, for ext_free */
	volatile u_int	*ref_cnt;	/* pointer to ref count info */
	int		 ext_type;	/* type of external storage */
};

#define MT_IOVEC        9
#define MT_CLIOVEC      10

#define EXT_IOVEC       8
#define EXT_CLIOVEC     9
#define EXT_JMPIOVEC    10

#define m_cur_offset	m_ext.ext_size		/* override to provide ddp offset */
#define m_seq		m_pkthdr.csum_data	/* stored sequence */
#define m_ddp_gl	m_ext.ext_buf		/* ddp list	*/
#define m_ddp_flags	m_pkthdr.csum_flags	/* ddp flags	*/
#define m_ulp_mode	m_pkthdr.tso_segsz	/* upper level protocol	*/

extern uma_zone_t zone_miovec;

struct mbuf_iovec {
	struct m_ext_ mi_ext;
	uint32_t      mi_flags;
	uint32_t      mi_len;
	caddr_t       mi_data;
	uint16_t      mi_tso_segsz;
	uint16_t      mi_ether_vtag;
	uint16_t      mi_rss_hash;   /* this can be shrunk down if something comes
				      * along that needs 1 byte
				      */
	uint16_t     mi_pad;
	struct mbuf *mi_mbuf; 	/* need to be able to handle the @#$@@#%$ing packet zone */
#define mi_size      mi_ext.ext_size
#define mi_base      mi_ext.ext_buf
#define mi_args      mi_ext.ext_args
#define mi_size      mi_ext.ext_size
#define mi_size      mi_ext.ext_size
#define mi_refcnt    mi_ext.ref_cnt
#define mi_ext_free  mi_ext.ext_free
#define mi_ext_flags mi_ext.ext_flags
#define mi_type      mi_ext.ext_type
};

#define MIOVBYTES           512
#define MAX_MBUF_IOV        ((MHLEN-8)/sizeof(struct mbuf_iovec))
#define MAX_MIOVEC_IOV      ((MIOVBYTES-sizeof(struct m_hdr)-sizeof(struct pkthdr)-8)/sizeof(struct mbuf_iovec))
#define MAX_CL_IOV          ((MCLBYTES-sizeof(struct m_hdr)-sizeof(struct pkthdr)-8)/sizeof(struct mbuf_iovec))
#define MAX_PAGE_IOV        ((MJUMPAGESIZE-sizeof(struct m_hdr)-sizeof(struct pkthdr)-8)/sizeof(struct mbuf_iovec))

struct mbuf_vec {
	uint16_t mv_first;     /* first valid cluster        */
	uint16_t mv_count;     /* # of clusters              */
	uint32_t mv_flags;     /* flags for iovec            */
	struct mbuf_iovec mv_vec[0]; /* depends on whether or not this is in a cluster or an mbuf */
};
void mi_init(void);
void mi_deinit(void);

int _m_explode(struct mbuf *);
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
m_iovappend(struct mbuf *m, uint8_t *cl, int size, int len, caddr_t data, volatile uint32_t *ref)
{ 
	struct mbuf_vec *mv = mtomv(m);
	struct mbuf_iovec *iov;
	int idx = mv->mv_first + mv->mv_count; 

        KASSERT(idx <= MAX_MBUF_IOV, ("tried to append too many clusters to mbuf iovec")); 
	if ((m->m_flags & M_EXT) != 0) 
		panic("invalid flags in %s", __func__); 

        if (mv->mv_count == 0)
		m->m_data = data;

        iov = &mv->mv_vec[idx];
	iov->mi_type = m_gettype(size); 
        iov->mi_base = cl; 
        iov->mi_len = len; 
        iov->mi_data = data;
	iov->mi_refcnt = ref;
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

static __inline void
busdma_map_mbuf_fast(struct mbuf *m, bus_dma_segment_t *seg)
{
	seg->ds_addr = pmap_kextract(mtod(m, vm_offset_t));
	seg->ds_len = m->m_len;
}

int busdma_map_sg_collapse(struct mbuf **m, bus_dma_segment_t *segs, int *nsegs);
int busdma_map_sg_vec(struct mbuf **m, struct mbuf **mp, bus_dma_segment_t *segs, int count);
static __inline int busdma_map_sgl(bus_dma_segment_t *vsegs, bus_dma_segment_t *segs, int count) 
{
	while (count--) {
		segs->ds_addr = pmap_kextract((vm_offset_t)vsegs->ds_addr);
		segs->ds_len = vsegs->ds_len;
		segs++;
		vsegs++;
	}
	return (0);
}

struct mbuf *mi_collapse_mbuf(struct mbuf_iovec *mi, struct mbuf *m);
void *mcl_alloc(int seg_count, int *type);

void mb_free_ext_fast(struct mbuf_iovec *mi, int type, int idx);

static __inline void
mi_collapse_sge(struct mbuf_iovec *mi, bus_dma_segment_t *seg)
{
	mi->mi_flags = 0;
	mi->mi_base = (caddr_t)seg->ds_addr;
	mi->mi_len = seg->ds_len;
	mi->mi_size = 0;
	mi->mi_type = EXT_PHYS;
	mi->mi_refcnt = NULL;
}

static __inline void
m_free_iovec(struct mbuf *m, int type)
{
	int i;
	struct mbuf_vec *mv;
	struct mbuf_iovec *mi;
  
	mv = mtomv(m);
	mi = mv->mv_vec;
	for (i = 0; i < mv->mv_count; i++, mi++) {
		DPRINTF("freeing buf=%d of %d\n", i, mv->mv_count);
		mb_free_ext_fast(mi, mi->mi_type, i);
	}
	switch (type) {
	case EXT_IOVEC:
		uma_zfree(zone_miovec, m);
		break;
	case EXT_CLIOVEC:
		cxgb_cache_put(zone_clust, m);
		break;
	case EXT_JMPIOVEC:
		cxgb_cache_put(zone_jumbop, m);
		break;		
	default:
		panic("unexpected type %d\n", type);
	}
}

static __inline void
m_freem_iovec(struct mbuf_iovec *mi)
{
	struct mbuf *m = (struct mbuf *)mi->mi_base;

	switch (mi->mi_type) {
	case EXT_MBUF:
#ifdef PIO_LEN		
		KASSERT(m->m_pkthdr.len > PIO_LEN, ("freeing PIO buf"));
#endif
		KASSERT((mi->mi_flags & M_NOFREE) == 0, ("no free set on mbuf"));
		KASSERT(m->m_next == NULL, ("freeing chain"));
		cxgb_mbufs_outstanding--;
		m_free_fast(m);
		break;
	case EXT_PACKET:
		cxgb_pack_outstanding--;
		m_free(mi->mi_mbuf);
		break;
	case EXT_IOVEC:
	case EXT_CLIOVEC:
	case EXT_JMPIOVEC:
		m = (struct mbuf *)mi->mi_base;
		m_free_iovec(m, mi->mi_type);
		break;
	case EXT_CLUSTER:
	case EXT_JUMBOP:
	case EXT_JUMBO9:
	case EXT_JUMBO16:
	case EXT_SFBUF:
	case EXT_NET_DRV:
	case EXT_MOD_TYPE:
	case EXT_DISPOSABLE:
	case EXT_EXTREF:
		mb_free_ext_fast(mi, mi->mi_type, -1);
		break;
	default:
		panic("unknown miov type: %d\n", mi->mi_type);
		break;
	}
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
#ifdef PACKET_ZONE		
	case EXT_PACKET:
		zone = zone_pack;
		break;
#endif		
	default:
		panic("%s: invalid cluster type %d", __func__, type);
	}
	return (zone);
}

static __inline int
m_getsizefromtype(int type)
{
	int size;
	
	switch (type) {
	case EXT_MBUF:
		size = MSIZE;
		break;
	case EXT_CLUSTER:
	case EXT_PACKET:		
		size = MCLBYTES;
		break;
#if MJUMPAGESIZE != MCLBYTES
	case EXT_JUMBOP:
		size = MJUMPAGESIZE;
		break;
#endif
	case EXT_JUMBO9:
		size = MJUM9BYTES;
		break;
	case EXT_JUMBO16:
		size = MJUM16BYTES;
		break;
	default:
		panic("%s: unrecognized cluster type %d", __func__, type);
	}
	return (size);
}

void dump_mi(struct mbuf_iovec *mi);

#endif /* _MVEC_H_ */
