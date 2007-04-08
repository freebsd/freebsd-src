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
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <dev/cxgb/sys/mvec.h>

int
_m_explode(struct mbuf *m) 
{
        int i, offset, type;
        void *cl;
        struct mbuf *m0, *head = NULL;
        struct mbuf_vec *mv;
         
        mv = mtomv(m);
        for (i = mv->mv_count + mv->mv_first - 1;
             i > mv->mv_first; i--) {
                cl = mv->mv_vec[i].mi_base;
                if ((m0 = m_get(M_NOWAIT, MT_DATA)) == NULL) {
                        m_freem(head); 
                        return (ENOMEM); 
                } 
		m0->m_flags = 0;
		type = mbuf_vec_get_type(mv, i);
		m_cljset(m0, (uint8_t *)cl, type);
		m0->m_len = mv->mv_vec[i].mi_size;

		offset = mv->mv_vec[i].mi_offset;

		if (offset)
			m_adj(m, offset);

		m0->m_next = head;
		m->m_len -= m0->m_len;
		head = m0;
	}
	offset = mv->mv_vec[0].mi_offset;
	cl = mv->mv_vec[0].mi_base;
	type = mbuf_vec_get_type(mv, 0);
	m->m_flags &= ~(M_IOVEC); 
	m_cljset(m, cl, type);
	if (offset)
		m_adj(m, offset);
	m->m_next = head;
	
	return (0);
} 

#define MAX_BUFS 36

struct mbuf *
_m_collapse(struct mbuf *m, int maxbufs)
{
	struct mbuf *m0, *lvec[MAX_BUFS];
	struct mbuf **mnext, **vec = &lvec[0];
	struct mbuf *mhead = NULL;
	struct mbuf_vec *mv;
	int i, j, max;

	if (maxbufs > MAX_BUFS)
		if ((vec = malloc(maxbufs * sizeof(struct mbuf *),
			    M_DEVBUF, M_NOWAIT)) == NULL)
			return (NULL);

	m0 = m;
	for (i = 0; i < maxbufs; i++) {
		if (m0 == NULL)
			goto batch;
		vec[i] = m0;
		m0 = m0->m_next;
	}

	if (i == maxbufs) 
		return (NULL);	
batch:
	max = i;
	i = 0;
	m0 = NULL;
	mnext = NULL;
	while (i < max) {
		if ((vec[i]->m_flags & M_EXT) == 0) {
			m0 = m_get(M_NOWAIT, MT_DATA);
		} else {
			m0 = vec[i];
			m0->m_flags = (vec[i]->m_flags & ~M_EXT);
		}
		m0->m_flags |= M_IOVEC;
		if (m0 == NULL)
			goto m_getfail;
		if (i == 0)
			mhead = m0;
		if (mnext)
			*mnext = m0;
		mv = mtomv(m0);
		mv->mv_count = mv->mv_first = 0;
		for (j = 0; j < MAX_MBUF_IOV; j++, i++) {
			if (vec[i]->m_flags & M_EXT) {
				mv->mv_vec[j].mi_base = vec[i]->m_ext.ext_buf;
				mv->mv_vec[j].mi_offset =
				    (vec[i]->m_ext.ext_buf - vec[i]->m_data);
				mv->mv_vec[j].mi_size = vec[i]->m_ext.ext_size;
				mv->mv_vec[j].mi_flags = vec[i]->m_ext.ext_type;
			} else {
				mv->mv_vec[j].mi_base = (caddr_t)vec[i];
				mv->mv_vec[j].mi_offset =
				    ((caddr_t)vec[i] - vec[i]->m_data);
				mv->mv_vec[j].mi_size = MSIZE;
				mv->mv_vec[j].mi_flags = EXT_MBUF;
			}
		}
		mnext = &m0->m_next;
	}

	mhead->m_flags |= (m0->m_flags & M_PKTHDR);
	return (mhead);

m_getfail:
	m0 = mhead;
	while (mhead) {
		mhead = m0->m_next;
		uma_zfree(zone_mbuf, m0);
	}
	return (NULL);
}

void
mb_free_vec(struct mbuf *m)
{
	struct mbuf_vec *mv;
	int i;
	
	KASSERT((m->m_flags & (M_EXT|M_IOVEC)) == M_IOVEC,
	    ("%s: M_IOVEC not set", __func__));

	mv = mtomv(m);
	KASSERT(mv->mv_count <= MAX_MBUF_IOV,
	    ("%s: mi_count too large %d", __func__, mv->mv_count));

	for (i = mv->mv_first; i < mv->mv_count; i++) {
		uma_zone_t zone = NULL;
		int *refcnt;
		int type = mbuf_vec_get_type(mv, i);
		void *cl = mv->mv_vec[i].mi_base;
		int size = mv->mv_vec[i].mi_size;
		
		zone = m_getzone(size);
		refcnt = uma_find_refcnt(zone, cl);
		if (*refcnt != 1 && atomic_fetchadd_int(refcnt, -1) != 1)
			continue;
	
		switch (type) {
		case EXT_PACKET:	/* The packet zone is special. */
			if (*refcnt == 0)
				*refcnt = 1;
			uma_zfree(zone_pack, m);
			return;		/* Job done. */
		case EXT_CLUSTER:
		case EXT_JUMBOP:
		case EXT_JUMBO9:  
		case EXT_JUMBO16:
			uma_zfree(zone, cl);
			continue;
		case EXT_SFBUF:
			*refcnt = 0;
			uma_zfree(zone_ext_refcnt, __DEVOLATILE(u_int *,
				refcnt));
			/* FALLTHROUGH */
		case EXT_EXTREF:
#ifdef notyet			
			KASSERT(m->m_ext.ext_free != NULL,
				("%s: ext_free not set", __func__));
			(*(m->m_ext.ext_free))(m->m_ext.ext_buf,
			    m->m_ext.ext_args);
#endif
			/*
			 * XXX 
			 */
			panic("unsupported mbuf_vec type: %d\n", type);
			break;
		default:
			KASSERT(m->m_ext.ext_type == 0,
				("%s: unknown ext_type", __func__));
			
		}
	}
	/*
	 * Free this mbuf back to the mbuf zone with all m_ext
	 * information purged.
	 */
	m->m_flags &= ~M_IOVEC;
	uma_zfree(zone_mbuf, m);
}
