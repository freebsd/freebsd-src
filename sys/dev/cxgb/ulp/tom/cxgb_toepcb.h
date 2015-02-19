/*-
 * Copyright (c) 2007-2009, Chelsio Inc.
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
#include <sys/limits.h>

#define TP_DATASENT         	(1 << 0)
#define TP_TX_WAIT_IDLE      	(1 << 1)
#define TP_FIN_SENT          	(1 << 2)
#define TP_ABORT_RPL_PENDING 	(1 << 3)
#define TP_ABORT_SHUTDOWN    	(1 << 4)
#define TP_ABORT_RPL_RCVD    	(1 << 5)
#define TP_ABORT_REQ_RCVD    	(1 << 6)
#define TP_ATTACHED	    	(1 << 7)
#define TP_CPL_DONE		(1 << 8)
#define TP_IS_A_SYNQ_ENTRY	(1 << 9)
#define TP_ABORT_RPL_SENT	(1 << 10)
#define TP_SEND_FIN          	(1 << 11)
#define TP_SYNQE_EXPANDED	(1 << 12)

struct toepcb {
	TAILQ_ENTRY(toepcb) link; /* toep_list */
	int 			tp_flags;
	struct toedev 		*tp_tod;
	struct l2t_entry 	*tp_l2t;
	int			tp_tid;
	int 			tp_wr_max;
	int 			tp_wr_avail;
	int 			tp_wr_unacked;
	int 			tp_delack_mode;
	int 			tp_ulp_mode;
	int 			tp_qset;
	int 			tp_enqueued;
	int 			tp_rx_credits;

	struct inpcb 		*tp_inp;
	struct mbuf		*tp_m_last;

	struct mbufq 		wr_list;
	struct mbufq 		out_of_order_queue;
};

static inline void
reset_wr_list(struct toepcb *toep)
{
	mbufq_init(&toep->wr_list, INT_MAX);	/* XXX: sane limit needed */
}

static inline void
enqueue_wr(struct toepcb *toep, struct mbuf *m)
{
	(void )mbufq_enqueue(&toep->wr_list, m);
}

static inline struct mbuf *
peek_wr(const struct toepcb *toep)
{
	return (mbufq_first(&toep->wr_list));
}

static inline struct mbuf *
dequeue_wr(struct toepcb *toep)
{
	return (mbufq_dequeue(&toep->wr_list));
}

#endif
