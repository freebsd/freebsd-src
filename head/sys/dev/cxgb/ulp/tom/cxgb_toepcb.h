/*-
 * Copyright (c) 2007-2008, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
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
 */
#ifndef CXGB_TOEPCB_H_
#define CXGB_TOEPCB_H_
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/mbufq.h>

struct toepcb {
	struct toedev 		*tp_toedev;
	struct l2t_entry 	*tp_l2t;
	unsigned int 		tp_tid;
	int 			tp_wr_max;
	int 			tp_wr_avail;
	int 			tp_wr_unacked;
	int 			tp_delack_mode;
	int 			tp_mtu_idx;
	int 			tp_ulp_mode;
	int 			tp_qset_idx;
	int 			tp_mss_clamp;
	int 			tp_qset;
	int 			tp_flags;
	int 			tp_enqueued_bytes;
	int 			tp_page_count;
	int 			tp_state;

	tcp_seq 		tp_iss;
	tcp_seq 		tp_delack_seq;
	tcp_seq 		tp_rcv_wup;
	tcp_seq 		tp_copied_seq;
	uint64_t 		tp_write_seq;

	volatile int 		tp_refcount;
	vm_page_t 		*tp_pages;
	
	struct tcpcb 		*tp_tp;
	struct mbuf  		*tp_m_last;
	bus_dma_tag_t		tp_tx_dmat;
	bus_dma_tag_t		tp_rx_dmat;
	bus_dmamap_t		tp_dmamap;

	LIST_ENTRY(toepcb) 	synq_entry;
	struct mbuf_head 	wr_list;
	struct mbuf_head 	out_of_order_queue;
	struct ddp_state 	tp_ddp_state;
	struct cv		tp_cv;
			   
};

static inline void
reset_wr_list(struct toepcb *toep)
{

	mbufq_init(&toep->wr_list);
}

static inline void
purge_wr_queue(struct toepcb *toep)
{
	struct mbuf *m;
	
	while ((m = mbufq_dequeue(&toep->wr_list)) != NULL) 
		m_freem(m);
}

static inline void
enqueue_wr(struct toepcb *toep, struct mbuf *m)
{

	mbufq_tail(&toep->wr_list, m);
}

static inline struct mbuf *
peek_wr(const struct toepcb *toep)
{

	return (mbufq_peek(&toep->wr_list));
}

static inline struct mbuf *
dequeue_wr(struct toepcb *toep)
{

	return (mbufq_dequeue(&toep->wr_list));
}

#define wr_queue_walk(toep, m) \
	for (m = peek_wr(toep); m; m = m->m_nextpkt)



#endif

