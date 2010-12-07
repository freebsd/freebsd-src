/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD: user/dfr/xenhvm/7/sys/dev/xen/netfront/mbufq.h 181643 2008-08-12 20:01:57Z kmacy $

***************************************************************************/

#ifndef CXGB_MBUFQ_H_
#define CXGB_MBUFQ_H_

struct mbuf_head {
	struct mbuf *head;
	struct mbuf *tail;
	uint32_t     qlen;
	uint32_t     qsize;
	struct mtx   lock;
};

static __inline void
mbufq_init(struct mbuf_head *l)
{
	l->head = l->tail = NULL;
	l->qlen = l->qsize = 0;
}

static __inline int
mbufq_empty(struct mbuf_head *l)
{
	return (l->head == NULL);
}

static __inline int
mbufq_len(struct mbuf_head *l)
{
	return (l->qlen);
}

static __inline int
mbufq_size(struct mbuf_head *l)
{
	return (l->qsize);
}

static __inline int
mbufq_head_size(struct mbuf_head *l)
{
	return (l->head ? l->head->m_pkthdr.len : 0);
}

static __inline void
mbufq_tail(struct mbuf_head *l, struct mbuf *m)
{
	l->qlen++;
	if (l->head == NULL)
		l->head = m;
	else
		l->tail->m_nextpkt = m;
	l->tail = m;
	l->qsize += m->m_pkthdr.len;
}

static __inline struct mbuf *
mbufq_dequeue(struct mbuf_head *l)
{
	struct mbuf *m;

	m = l->head;
	if (m) {
		if (m == l->tail) 
			l->head = l->tail = NULL;
		else
			l->head = m->m_nextpkt;
		m->m_nextpkt = NULL;
		l->qlen--;
		l->qsize -= m->m_pkthdr.len;
	}

	return (m);
}

static __inline struct mbuf *
mbufq_peek(struct mbuf_head *l)
{
	return (l->head);
}

static __inline void
mbufq_append(struct mbuf_head *a, struct mbuf_head *b)
{
	if (a->tail) 
		a->tail->m_nextpkt = b->head;
	if (b->tail)
		a->tail = b->tail;
	a->qlen += b->qlen;
	a->qsize += b->qsize;
	
	
}
#endif  /* CXGB_MBUFQ_H_ */
