/* SCTP kernel reference Implementation
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001-2003 Intel Corp.
 * Copyright (c) 2001-2003 International Business Machines Corp.
 *
 * This file is part of the SCTP kernel reference Implementation
 *
 * These functions implement the sctp_outq class.   The outqueue handles
 * bundling and queueing of outgoing SCTP chunks.
 *
 * The SCTP reference implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * The SCTP reference implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Perry Melange         <pmelange@null.cc.uic.edu>
 *    Xingang Guo           <xingang.guo@intel.com>
 *    Hui Huang 	    <hui.huang@nokia.com>
 *    Sridhar Samudrala     <sri@us.ibm.com>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#include <linux/list.h>   /* For struct list_head */
#include <linux/socket.h>
#include <linux/ip.h>
#include <net/sock.h>	  /* For skb_set_owner_w */

#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

/* Declare internal functions here.  */
static int sctp_acked(struct sctp_sackhdr *sack, __u32 tsn);
static void sctp_check_transmitted(struct sctp_outq *q,
				   struct list_head *transmitted_queue,
				   struct sctp_transport *transport,
				   struct sctp_sackhdr *sack,
				   __u32 highest_new_tsn);

static void sctp_mark_missing(struct sctp_outq *q,
			      struct list_head *transmitted_queue,
			      struct sctp_transport *transport,
			      __u32 highest_new_tsn,
			      int count_of_newacks);

/* Add data to the front of the queue. */
static inline void sctp_outq_head_data(struct sctp_outq *q,
					struct sctp_chunk *ch)
{
	__skb_queue_head(&q->out, (struct sk_buff *)ch);
	q->out_qlen += ch->skb->len;
	return;
}

/* Take data from the front of the queue. */
static inline struct sctp_chunk *sctp_outq_dequeue_data(struct sctp_outq *q)
{
	struct sctp_chunk *ch;
	ch = (struct sctp_chunk *)__skb_dequeue(&q->out);
	if (ch)
		q->out_qlen -= ch->skb->len;
	return ch;
}
/* Add data chunk to the end of the queue. */
static inline void sctp_outq_tail_data(struct sctp_outq *q,
				       struct sctp_chunk *ch)
{
	__skb_queue_tail(&q->out, (struct sk_buff *)ch);
	q->out_qlen += ch->skb->len;
	return;
}

/* Insert a chunk behind chunk 'pos'. */
static inline void sctp_outq_insert_data(struct sctp_outq *q,
					 struct sctp_chunk *ch,
					 struct sctp_chunk *pos)
{
	__skb_insert((struct sk_buff *)ch, (struct sk_buff *)pos->prev,
		     (struct sk_buff *)pos, pos->list);
	q->out_qlen += ch->skb->len;
}

/*
 * SFR-CACC algorithm:
 * D) If count_of_newacks is greater than or equal to 2
 * and t was not sent to the current primary then the
 * sender MUST NOT increment missing report count for t.
 */
static inline int sctp_cacc_skip_3_1_d(struct sctp_transport *primary,
				       struct sctp_transport *transport,
				       int count_of_newacks)
{
	if (count_of_newacks >=2 && transport != primary)
		return 1;
	return 0;
}

/*
 * SFR-CACC algorithm:
 * F) If count_of_newacks is less than 2, let d be the
 * destination to which t was sent. If cacc_saw_newack
 * is 0 for destination d, then the sender MUST NOT
 * increment missing report count for t.
 */
static inline int sctp_cacc_skip_3_1_f(struct sctp_transport *transport,
				       int count_of_newacks)
{
	if (count_of_newacks < 2 && !transport->cacc.cacc_saw_newack)
		return 1;
	return 0;
}

/*
 * SFR-CACC algorithm:
 * 3.1) If CYCLING_CHANGEOVER is 0, the sender SHOULD
 * execute steps C, D, F.
 *
 * C has been implemented in sctp_outq_sack
 */
static inline int sctp_cacc_skip_3_1(struct sctp_transport *primary,
				     struct sctp_transport *transport,
				     int count_of_newacks)
{
	if (!primary->cacc.cycling_changeover) {
		if (sctp_cacc_skip_3_1_d(primary, transport, count_of_newacks))
			return 1;
		if (sctp_cacc_skip_3_1_f(transport, count_of_newacks))
			return 1;
		return 0;
	}
	return 0;
}

/*
 * SFR-CACC algorithm:
 * 3.2) Else if CYCLING_CHANGEOVER is 1, and t is less
 * than next_tsn_at_change of the current primary, then
 * the sender MUST NOT increment missing report count
 * for t.
 */
static inline int sctp_cacc_skip_3_2(struct sctp_transport *primary, __u32 tsn)
{
	if (primary->cacc.cycling_changeover &&
	    TSN_lt(tsn, primary->cacc.next_tsn_at_change))
		return 1;
	return 0;
}

/*
 * SFR-CACC algorithm:
 * 3) If the missing report count for TSN t is to be
 * incremented according to [RFC2960] and
 * [SCTP_STEWART-2002], and CHANGEOVER_ACTIVE is set,
 * then the sender MUST futher execute steps 3.1 and
 * 3.2 to determine if the missing report count for
 * TSN t SHOULD NOT be incremented.
 *
 * 3.3) If 3.1 and 3.2 do not dictate that the missing
 * report count for t should not be incremented, then
 * the sender SOULD increment missing report count for
 * t (according to [RFC2960] and [SCTP_STEWART_2002]).
 */
static inline int sctp_cacc_skip(struct sctp_transport *primary,
				 struct sctp_transport *transport,
				 int count_of_newacks,
				 __u32 tsn)
{
	if (primary->cacc.changeover_active &&
	    (sctp_cacc_skip_3_1(primary, transport, count_of_newacks)
	     || sctp_cacc_skip_3_2(primary, tsn)))
		return 1;
	return 0;
}

/* Generate a new outqueue.  */
struct sctp_outq *sctp_outq_new(struct sctp_association *asoc)
{
	struct sctp_outq *q;

	q = t_new(struct sctp_outq, GFP_KERNEL);
	if (q) {
		sctp_outq_init(asoc, q);
		q->malloced = 1;
	}
	return q;
}

/* Initialize an existing sctp_outq.  This does the boring stuff.
 * You still need to define handlers if you really want to DO
 * something with this structure...
 */
void sctp_outq_init(struct sctp_association *asoc, struct sctp_outq *q)
{
	q->asoc = asoc;
	skb_queue_head_init(&q->out);
	skb_queue_head_init(&q->control);
	INIT_LIST_HEAD(&q->retransmit);
	INIT_LIST_HEAD(&q->sacked);

	q->init_output = NULL;
	q->config_output = NULL;
	q->append_output = NULL;
	q->build_output = NULL;
	q->force_output = NULL;

	q->outstanding_bytes = 0;
	q->empty = 1;
	q->cork  = 0;

	q->malloced = 0;
	q->out_qlen = 0;
}

/* Free the outqueue structure and any related pending chunks.
 */
void sctp_outq_teardown(struct sctp_outq *q)
{
	struct sctp_transport *transport;
	struct list_head *lchunk, *pos, *temp;
	struct sctp_chunk *chunk;

	/* Throw away unacknowledged chunks. */
	list_for_each(pos, &q->asoc->peer.transport_addr_list) {
		transport = list_entry(pos, struct sctp_transport, transports);
		while ((lchunk = sctp_list_dequeue(&transport->transmitted))) {
			chunk = list_entry(lchunk, struct sctp_chunk,
					   transmitted_list);
			/* Mark as part of a failed message. */
			sctp_datamsg_fail(chunk, q->error);
			sctp_chunk_free(chunk);
		}
	}

	/* Throw away chunks that have been gap ACKed.  */
	list_for_each_safe(lchunk, temp, &q->sacked) {
		list_del_init(lchunk);
		chunk = list_entry(lchunk, struct sctp_chunk,
				   transmitted_list);
		sctp_datamsg_fail(chunk, q->error);
		sctp_chunk_free(chunk);
	}

	/* Throw away any chunks in the retransmit queue. */
	list_for_each_safe(lchunk, temp, &q->retransmit) {
		list_del_init(lchunk);
		chunk = list_entry(lchunk, struct sctp_chunk,
				   transmitted_list);
		sctp_datamsg_fail(chunk, q->error);
		sctp_chunk_free(chunk);
	}

	/* Throw away any leftover data chunks. */
	while ((chunk = sctp_outq_dequeue_data(q))) {

		/* Mark as send failure. */
		sctp_datamsg_fail(chunk, q->error);
		sctp_chunk_free(chunk);
	}

	q->error = 0;

	/* Throw away any leftover control chunks. */
	while ((chunk = (struct sctp_chunk *) skb_dequeue(&q->control)))
		sctp_chunk_free(chunk);
}

/* Free the outqueue structure and any related pending chunks.  */
void sctp_outq_free(struct sctp_outq *q)
{
	/* Throw away leftover chunks. */
	sctp_outq_teardown(q);

	/* If we were kmalloc()'d, free the memory.  */
	if (q->malloced)
		kfree(q);
}

/* Put a new chunk in an sctp_outq.  */
int sctp_outq_tail(struct sctp_outq *q, struct sctp_chunk *chunk)
{
	int error = 0;

	SCTP_DEBUG_PRINTK("sctp_outq_tail(%p, %p[%s])\n",
			  q, chunk, chunk && chunk->chunk_hdr ?
			  sctp_cname(SCTP_ST_CHUNK(chunk->chunk_hdr->type))
			  : "Illegal Chunk");

	/* If it is data, queue it up, otherwise, send it
	 * immediately.
	 */
	if (SCTP_CID_DATA == chunk->chunk_hdr->type) {
		/* Is it OK to queue data chunks?  */
		/* From 9. Termination of Association
		 *
		 * When either endpoint performs a shutdown, the
		 * association on each peer will stop accepting new
		 * data from its user and only deliver data in queue
		 * at the time of sending or receiving the SHUTDOWN
		 * chunk.
		 */
		switch (q->asoc->state) {
		case SCTP_STATE_EMPTY:
		case SCTP_STATE_CLOSED:
		case SCTP_STATE_SHUTDOWN_PENDING:
		case SCTP_STATE_SHUTDOWN_SENT:
		case SCTP_STATE_SHUTDOWN_RECEIVED:
		case SCTP_STATE_SHUTDOWN_ACK_SENT:
			/* Cannot send after transport endpoint shutdown */
			error = -ESHUTDOWN;
			break;

		default:
			SCTP_DEBUG_PRINTK("outqueueing (%p, %p[%s])\n",
			  q, chunk, chunk && chunk->chunk_hdr ?
			  sctp_cname(SCTP_ST_CHUNK(chunk->chunk_hdr->type))
			  : "Illegal Chunk");

			sctp_outq_tail_data(q, chunk);
			if (chunk->chunk_hdr->flags & SCTP_DATA_UNORDERED)
				SCTP_INC_STATS(SctpOutUnorderChunks);
			else
				SCTP_INC_STATS(SctpOutOrderChunks);
			q->empty = 0;
			break;
		};
	} else {
		__skb_queue_tail(&q->control, (struct sk_buff *) chunk);
		SCTP_INC_STATS(SctpOutCtrlChunks);
	}

	if (error < 0)
		return error;

	if (!q->cork)
		error = sctp_outq_flush(q, 0);

	return error;
}

/* Insert a chunk into the retransmit queue.  Chunks on the retransmit
 * queue are kept in order, based on the TSNs.
 */
void sctp_retransmit_insert(struct list_head *tlchunk, struct sctp_outq *q)
{
	struct list_head *rlchunk;
	struct sctp_chunk *tchunk, *rchunk;
	__u32 ttsn, rtsn;
	int done = 0;

	tchunk = list_entry(tlchunk, struct sctp_chunk, transmitted_list);
	ttsn = ntohl(tchunk->subh.data_hdr->tsn);

	list_for_each(rlchunk, &q->retransmit) {
		rchunk = list_entry(rlchunk, struct sctp_chunk,
				    transmitted_list);
		rtsn = ntohl(rchunk->subh.data_hdr->tsn);
		if (TSN_lt(ttsn, rtsn)) {
			list_add(tlchunk, rlchunk->prev);
			done = 1;
			break;
		}
	}
	if (!done) {
		list_add_tail(tlchunk, &q->retransmit);
	}
}

/* Mark all the eligible packets on a transport for retransmission.  */
void sctp_retransmit_mark(struct sctp_outq *q,
			  struct sctp_transport *transport,
			  __u8 fast_retransmit)
{
	struct list_head *lchunk, *ltemp;
	struct sctp_chunk *chunk;

	/* Walk through the specified transmitted queue.  */
	list_for_each_safe(lchunk, ltemp, &transport->transmitted) {
		chunk = list_entry(lchunk, struct sctp_chunk,
				   transmitted_list);

		/* If we are doing retransmission due to a fast retransmit,
		 * only the chunk's that are marked for fast retransmit
		 * should be added to the retransmit queue.  If we are doing
		 * retransmission due to a timeout or pmtu discovery, only the
		 * chunks that are not yet acked should be added to the
		 * retransmit queue.
		 */
		if ((fast_retransmit && chunk->fast_retransmit) ||
		   (!fast_retransmit && !chunk->tsn_gap_acked)) {
			/* RFC 2960 6.2.1 Processing a Received SACK
			 *
			 * C) Any time a DATA chunk is marked for
			 * retransmission (via either T3-rtx timer expiration
			 * (Section 6.3.3) or via fast retransmit
			 * (Section 7.2.4)), add the data size of those
			 * chunks to the rwnd.
			 */
			q->asoc->peer.rwnd += sctp_data_size(chunk);
			q->outstanding_bytes -= sctp_data_size(chunk);
			transport->flight_size -= sctp_data_size(chunk);

			/* sctpimpguide-05 Section 2.8.2
			 * M5) If a T3-rtx timer expires, the
			 * 'TSN.Missing.Report' of all affected TSNs is set
			 * to 0.
			 */
			chunk->tsn_missing_report = 0;

			/* If a chunk that is being used for RTT measurement
			 * has to be retransmitted, we cannot use this chunk
			 * anymore for RTT measurements. Reset rto_pending so
			 * that a new RTT measurement is started when a new
			 * data chunk is sent.
			 */
			if (chunk->rtt_in_progress) {
				chunk->rtt_in_progress = 0;
				transport->rto_pending = 0;
			}

			/* Move the chunk to the retransmit queue. The chunks
			 * on the retransmit queue is always kept in order.
			 */
			list_del_init(lchunk);
			sctp_retransmit_insert(lchunk, q);
		}
	}

	SCTP_DEBUG_PRINTK("%s: transport: %p, fast_retransmit: %d, "
			  "cwnd: %d, ssthresh: %d, flight_size: %d, "
			  "pba: %d\n", __FUNCTION__,
			  transport, fast_retransmit,
			  transport->cwnd, transport->ssthresh,
			  transport->flight_size,
			  transport->partial_bytes_acked);

}

/* Mark all the eligible packets on a transport for retransmission and force
 * one packet out.
 */
void sctp_retransmit(struct sctp_outq *q, struct sctp_transport *transport,
		     sctp_retransmit_reason_t reason)
{
	int error = 0;
	__u8 fast_retransmit = 0;

	switch(reason) {
	case SCTP_RTXR_T3_RTX:
		sctp_transport_lower_cwnd(transport, SCTP_LOWER_CWND_T3_RTX);
		/* Update the retran path if the T3-rtx timer has expired for
		 * the current retran path.
		 */
		if (transport == transport->asoc->peer.retran_path)
			sctp_assoc_update_retran_path(transport->asoc);
		break;
	case SCTP_RTXR_FAST_RTX:
		sctp_transport_lower_cwnd(transport, SCTP_LOWER_CWND_FAST_RTX);
		fast_retransmit = 1;
		break;
	case SCTP_RTXR_PMTUD:
	default:
		break;
	}

	sctp_retransmit_mark(q, transport, fast_retransmit);

	error = sctp_outq_flush(q, /* rtx_timeout */ 1);

	if (error)
		q->asoc->base.sk->sk_err = -error;
}

/*
 * Transmit DATA chunks on the retransmit queue.  Upon return from
 * sctp_outq_flush_rtx() the packet 'pkt' may contain chunks which
 * need to be transmitted by the caller.
 * We assume that pkt->transport has already been set.
 *
 * The return value is a normal kernel error return value.
 */
static int sctp_outq_flush_rtx(struct sctp_outq *q, struct sctp_packet *pkt,
			       int rtx_timeout, int *start_timer)
{
	struct list_head *lqueue;
	struct list_head *lchunk;
	struct sctp_transport *transport = pkt->transport;
	sctp_xmit_t status;
	struct sctp_chunk *chunk;
	struct sctp_association *asoc;
	int error = 0;

	asoc = q->asoc;
	lqueue = &q->retransmit;

	/* RFC 2960 6.3.3 Handle T3-rtx Expiration
	 *
	 * E3) Determine how many of the earliest (i.e., lowest TSN)
	 * outstanding DATA chunks for the address for which the
	 * T3-rtx has expired will fit into a single packet, subject
	 * to the MTU constraint for the path corresponding to the
	 * destination transport address to which the retransmission
	 * is being sent (this may be different from the address for
	 * which the timer expires [see Section 6.4]). Call this value
	 * K. Bundle and retransmit those K DATA chunks in a single
	 * packet to the destination endpoint.
	 *
	 * [Just to be painfully clear, if we are retransmitting
	 * because a timeout just happened, we should send only ONE
	 * packet of retransmitted data.]
	 */
	lchunk = sctp_list_dequeue(lqueue);

	while (lchunk) {
		chunk = list_entry(lchunk, struct sctp_chunk,
				   transmitted_list);

		/* Make sure that Gap Acked TSNs are not retransmitted.  A
		 * simple approach is just to move such TSNs out of the
		 * way and into a 'transmitted' queue and skip to the
		 * next chunk.
		 */
		if (chunk->tsn_gap_acked) {
			list_add_tail(lchunk, &transport->transmitted);
			lchunk = sctp_list_dequeue(lqueue);
			continue;
		}

		/* Attempt to append this chunk to the packet. */
		status = (*q->append_output)(pkt, chunk);

		switch (status) {
		case SCTP_XMIT_PMTU_FULL:
			/* Send this packet.  */
			if ((error = (*q->force_output)(pkt)) == 0)
				*start_timer = 1;

			/* If we are retransmitting, we should only
			 * send a single packet.
			 */
			if (rtx_timeout) {
				list_add(lchunk, lqueue);
				lchunk = NULL;
			}

			/* Bundle lchunk in the next round.  */
			break;

		case SCTP_XMIT_RWND_FULL:
		        /* Send this packet. */
			if ((error = (*q->force_output)(pkt)) == 0)
				*start_timer = 1;

			/* Stop sending DATA as there is no more room
			 * at the receiver.
			 */
			list_add(lchunk, lqueue);
			lchunk = NULL;
			break;

		default:
			/* The append was successful, so add this chunk to
			 * the transmitted list.
			 */
			list_add_tail(lchunk, &transport->transmitted);
			*start_timer = 1;
			q->empty = 0;

			/* Retrieve a new chunk to bundle. */
			lchunk = sctp_list_dequeue(lqueue);
			break;
		};
	}

	return error;
}

/* Cork the outqueue so queued chunks are really queued. */
int sctp_outq_uncork(struct sctp_outq *q)
{
	int error = 0;
	if (q->cork) {
		q->cork = 0;
		error = sctp_outq_flush(q, 0);
	}
	return error;
}

/*
 * Try to flush an outqueue.
 *
 * Description: Send everything in q which we legally can, subject to
 * congestion limitations.
 * * Note: This function can be called from multiple contexts so appropriate
 * locking concerns must be made.  Today we use the sock lock to protect
 * this function.
 */
int sctp_outq_flush(struct sctp_outq *q, int rtx_timeout)
{
	struct sctp_packet *packet;
	struct sctp_packet singleton;
	struct sctp_association *asoc = q->asoc;
	int ecn_capable = asoc->peer.ecn_capable;
	__u16 sport = asoc->base.bind_addr.port;
	__u16 dport = asoc->peer.port;
	__u32 vtag = asoc->peer.i.init_tag;
	/* This is the ECNE handler for singleton packets.  */
	sctp_packet_phandler_t *s_ecne_handler = NULL;
	sctp_packet_phandler_t *ecne_handler = NULL;
	struct sk_buff_head *queue;
	struct sctp_transport *transport = NULL;
	struct sctp_transport *new_transport;
	struct sctp_chunk *chunk;
	sctp_xmit_t status;
	int error = 0;
	int start_timer = 0;

	/* These transports have chunks to send. */
	struct list_head transport_list;
	struct list_head *ltransport;

	INIT_LIST_HEAD(&transport_list);
	packet = NULL;

	/*
	 * 6.10 Bundling
	 *   ...
	 *   When bundling control chunks with DATA chunks, an
	 *   endpoint MUST place control chunks first in the outbound
	 *   SCTP packet.  The transmitter MUST transmit DATA chunks
	 *   within a SCTP packet in increasing order of TSN.
	 *   ...
	 */
	if (ecn_capable) {
		s_ecne_handler = &sctp_get_no_prepend;
		ecne_handler = &sctp_get_ecne_prepend;
	}

	queue = &q->control;
	while ((chunk = (struct sctp_chunk *)skb_dequeue(queue))) {
		/* Pick the right transport to use. */
		new_transport = chunk->transport;

		if (!new_transport) {
			new_transport = asoc->peer.active_path;
		} else if (!new_transport->active) {
			/* If the chunk is Heartbeat, send it to
			 * chunk->transport, even it's inactive.
			 */
			if (chunk->chunk_hdr->type != SCTP_CID_HEARTBEAT)
				new_transport = asoc->peer.active_path;
		}

		/* Are we switching transports?
		 * Take care of transport locks.
		 */
		if (new_transport != transport) {
			transport = new_transport;
			if (list_empty(&transport->send_ready)) {
				list_add_tail(&transport->send_ready,
					      &transport_list);
			}
			packet = &transport->packet;
			(*q->config_output)(packet, vtag,
					    ecn_capable, ecne_handler);
		}

		switch (chunk->chunk_hdr->type) {
		/*
		 * 6.10 Bundling
		 *   ...
		 *   An endpoint MUST NOT bundle INIT, INIT ACK or SHUTDOWN
		 *   COMPLETE with any other chunks.  [Send them immediately.]
		 */
		case SCTP_CID_INIT:
		case SCTP_CID_INIT_ACK:
		case SCTP_CID_SHUTDOWN_COMPLETE:
			(*q->init_output)(&singleton, transport, sport, dport);
			(*q->config_output)(&singleton, vtag, ecn_capable,
					    s_ecne_handler);
			(void) (*q->build_output)(&singleton, chunk);
			error = (*q->force_output)(&singleton);
			if (error < 0)
				return error;
			break;

		case SCTP_CID_ABORT:
		case SCTP_CID_SACK:
		case SCTP_CID_HEARTBEAT:
		case SCTP_CID_HEARTBEAT_ACK:
		case SCTP_CID_SHUTDOWN:
		case SCTP_CID_SHUTDOWN_ACK:
		case SCTP_CID_ERROR:
		case SCTP_CID_COOKIE_ECHO:
		case SCTP_CID_COOKIE_ACK:
		case SCTP_CID_ECN_ECNE:
		case SCTP_CID_ECN_CWR:
			(void) (*q->build_output)(packet, chunk);
			break;

		case SCTP_CID_ASCONF:
		case SCTP_CID_ASCONF_ACK:
			(void) (*q->build_output)(packet, chunk);
			break;

		default:
			/* We built a chunk with an illegal type! */
			BUG();
		};
	}

	/* Is it OK to send data chunks?  */
	switch (asoc->state) {
	case SCTP_STATE_COOKIE_ECHOED:
		/* Only allow bundling when this packet has a COOKIE-ECHO
		 * chunk.
		 */
		if (!packet || !packet->has_cookie_echo)
			break;

		/* fallthru */
	case SCTP_STATE_ESTABLISHED:
	case SCTP_STATE_SHUTDOWN_PENDING:
	case SCTP_STATE_SHUTDOWN_RECEIVED:
		/*
		 * RFC 2960 6.1  Transmission of DATA Chunks
		 *
		 * C) When the time comes for the sender to transmit,
		 * before sending new DATA chunks, the sender MUST
		 * first transmit any outstanding DATA chunks which
		 * are marked for retransmission (limited by the
		 * current cwnd).
		 */
		if (!list_empty(&q->retransmit)) {
			if (transport == asoc->peer.retran_path)
				goto retran;

			/* Switch transports & prepare the packet.  */

			transport = asoc->peer.retran_path;

			if (list_empty(&transport->send_ready)) {
				list_add_tail(&transport->send_ready,
					      &transport_list);
			}

			packet = &transport->packet;
			(*q->config_output)(packet, vtag,
					    ecn_capable, ecne_handler);
		retran:
			error = sctp_outq_flush_rtx(q, packet,
						    rtx_timeout, &start_timer);

			if (start_timer)
				sctp_transport_reset_timers(transport);

			/* This can happen on COOKIE-ECHO resend.  Only
			 * one chunk can get bundled with a COOKIE-ECHO.
			 */
			if (packet->has_cookie_echo)
				goto sctp_flush_out;

			/* Don't send new data if there is still data
			 * waiting to retransmit.
			 */
			if (!list_empty(&q->retransmit))
				goto sctp_flush_out;
		}

		/* Finally, transmit new packets.  */
		start_timer = 0;
		queue = &q->out;

		while ((chunk = sctp_outq_dequeue_data(q))) {
			/* RFC 2960 6.5 Every DATA chunk MUST carry a valid
			 * stream identifier.
			 */
			if (chunk->sinfo.sinfo_stream >=
			    asoc->c.sinit_num_ostreams) {

				/* Mark as s failed send. */
				sctp_datamsg_fail(chunk, SCTP_ERROR_INV_STRM);
				sctp_chunk_free(chunk);
				continue;
			}

			/* Has this chunk expired? */
			if (sctp_datamsg_expires(chunk)) {
				sctp_datamsg_fail(chunk, 0);
				sctp_chunk_free(chunk);
				continue;
			}

			/* If there is a specified transport, use it.
			 * Otherwise, we want to use the active path.
			 */
			new_transport = chunk->transport;
			if (!new_transport || !new_transport->active)
				new_transport = asoc->peer.active_path;

			/* Change packets if necessary.  */
			if (new_transport != transport) {
				transport = new_transport;

				/* Schedule to have this transport's
				 * packet flushed.
				 */
				if (list_empty(&transport->send_ready)) {
					list_add_tail(&transport->send_ready,
						      &transport_list);
				}

				packet = &transport->packet;
				(*q->config_output)(packet, vtag,
						    ecn_capable, ecne_handler);
			}

			SCTP_DEBUG_PRINTK("sctp_transmit_packet(%p, %p[%s]), ",
					  q, chunk,
					  chunk && chunk->chunk_hdr ?
					  sctp_cname(SCTP_ST_CHUNK(
						  chunk->chunk_hdr->type))
					  : "Illegal Chunk");

			SCTP_DEBUG_PRINTK("TX TSN 0x%x skb->head "
					"%p skb->users %d.\n",
					ntohl(chunk->subh.data_hdr->tsn),
					chunk->skb ?chunk->skb->head : 0,
					chunk->skb ?
					atomic_read(&chunk->skb->users) : -1);

			/* Add the chunk to the packet.  */
			status = (*q->build_output)(packet, chunk);

			switch (status) {
			case SCTP_XMIT_PMTU_FULL:
			case SCTP_XMIT_RWND_FULL:
			case SCTP_XMIT_NAGLE_DELAY:
				/* We could not append this chunk, so put
				 * the chunk back on the output queue.
				 */
				SCTP_DEBUG_PRINTK("sctp_outq_flush: could "
					"not transmit TSN: 0x%x, status: %d\n",
					ntohl(chunk->subh.data_hdr->tsn),
					status);
				sctp_outq_head_data(q, chunk);
				goto sctp_flush_out;
				break;

			case SCTP_XMIT_OK:
				break;

			default:
				BUG();
			}

			/* BUG: We assume that the (*q->force_output())
			 * call below will succeed all the time and add the
			 * chunk to the transmitted list and restart the
			 * timers.
			 * It is possible that the call can fail under OOM
			 * conditions.
			 *
			 * Is this really a problem?  Won't this behave
			 * like a lost TSN?
			 */
			list_add_tail(&chunk->transmitted_list,
				      &transport->transmitted);

			sctp_transport_reset_timers(transport);

			q->empty = 0;

			/* Only let one DATA chunk get bundled with a
			 * COOKIE-ECHO chunk.
			 */
			if (packet->has_cookie_echo)
				goto sctp_flush_out;
		}
		break;

	default:
		/* Do nothing.  */
		break;
	}

sctp_flush_out:

	/* Before returning, examine all the transports touched in
	 * this call.  Right now, we bluntly force clear all the
	 * transports.  Things might change after we implement Nagle.
	 * But such an examination is still required.
	 *
	 * --xguo
	 */
	while ((ltransport = sctp_list_dequeue(&transport_list)) != NULL ) {
		struct sctp_transport *t = list_entry(ltransport,
						      struct sctp_transport,
						      send_ready);
		if (t != transport)
			transport = t;

		packet = &transport->packet;
		if (packet->size != SCTP_IP_OVERHEAD)
			error = (*q->force_output)(packet);
	}

	return error;
}

/* Set the various output handling callbacks.  */
int sctp_outq_set_output_handlers(struct sctp_outq *q,
				      sctp_outq_ohandler_init_t init,
				      sctp_outq_ohandler_config_t config,
				      sctp_outq_ohandler_t append,
				      sctp_outq_ohandler_t build,
				      sctp_outq_ohandler_force_t force)
{
	q->init_output = init;
	q->config_output = config;
	q->append_output = append;
	q->build_output = build;
	q->force_output = force;
	return 0;
}

/* Update unack_data based on the incoming SACK chunk */
static void sctp_sack_update_unack_data(struct sctp_association *assoc,
					struct sctp_sackhdr *sack)
{
	sctp_sack_variable_t *frags;
	__u16 unack_data;
	int i;

	unack_data = assoc->next_tsn - assoc->ctsn_ack_point - 1;

	frags = sack->variable;
	for (i = 0; i < ntohs(sack->num_gap_ack_blocks); i++) {
		unack_data -= ((ntohs(frags[i].gab.end) -
				ntohs(frags[i].gab.start) + 1));
	}

	assoc->unack_data = unack_data;
}

/* Return the highest new tsn that is acknowledged by the given SACK chunk. */
static __u32 sctp_highest_new_tsn(struct sctp_sackhdr *sack,
				  struct sctp_association *asoc)
{
	struct list_head *ltransport, *lchunk;
	struct sctp_transport *transport;
	struct sctp_chunk *chunk;
	__u32 highest_new_tsn, tsn;
	struct list_head *transport_list = &asoc->peer.transport_addr_list;

	highest_new_tsn = ntohl(sack->cum_tsn_ack);

	list_for_each(ltransport, transport_list) {
		transport = list_entry(ltransport, struct sctp_transport,
				       transports);
		list_for_each(lchunk, &transport->transmitted) {
			chunk = list_entry(lchunk, struct sctp_chunk,
					   transmitted_list);
			tsn = ntohl(chunk->subh.data_hdr->tsn);

			if (!chunk->tsn_gap_acked &&
			    TSN_lt(highest_new_tsn, tsn) &&
			    sctp_acked(sack, tsn))
				highest_new_tsn = tsn;
		}
	}

	return highest_new_tsn;
}

/* This is where we REALLY process a SACK.
 *
 * Process the SACK against the outqueue.  Mostly, this just frees
 * things off the transmitted queue.
 */
int sctp_outq_sack(struct sctp_outq *q, struct sctp_sackhdr *sack)
{
	struct sctp_association *asoc = q->asoc;
	struct sctp_transport *transport;
	struct sctp_chunk *tchunk;
	struct list_head *lchunk, *transport_list, *pos, *temp;
	sctp_sack_variable_t *frags = sack->variable;
	__u32 sack_ctsn, ctsn, tsn;
	__u32 highest_tsn, highest_new_tsn;
	__u32 sack_a_rwnd;
	unsigned outstanding;
	struct sctp_transport *primary = asoc->peer.primary_path;
	int count_of_newacks = 0;

	/* Grab the association's destination address list. */
	transport_list = &asoc->peer.transport_addr_list;

	sack_ctsn = ntohl(sack->cum_tsn_ack);

	/*
	 * SFR-CACC algorithm:
	 * On receipt of a SACK the sender SHOULD execute the
	 * following statements.
	 *
	 * 1) If the cumulative ack in the SACK passes next tsn_at_change
	 * on the current primary, the CHANGEOVER_ACTIVE flag SHOULD be
	 * cleared. The CYCLING_CHANGEOVER flag SHOULD also be cleared for
	 * all destinations.
	 */
	if (TSN_lte(primary->cacc.next_tsn_at_change, sack_ctsn)) {
		primary->cacc.changeover_active = 0;
		list_for_each(pos, transport_list) {
			transport = list_entry(pos, struct sctp_transport,
					transports);
			transport->cacc.cycling_changeover = 0;
		}
	}

	/*
	 * SFR-CACC algorithm:
	 * 2) If the SACK contains gap acks and the flag CHANGEOVER_ACTIVE
	 * is set the receiver of the SACK MUST take the following actions:
	 *
	 * A) Initialize the cacc_saw_newack to 0 for all destination
	 * addresses.
	 */
	if (sack->num_gap_ack_blocks > 0 &&
	    primary->cacc.changeover_active) {
		list_for_each(pos, transport_list) {
			transport = list_entry(pos, struct sctp_transport,
					transports);
			transport->cacc.cacc_saw_newack = 0;
		}
	}

	/* Get the highest TSN in the sack. */
	highest_tsn = sack_ctsn +
		ntohs(frags[ntohs(sack->num_gap_ack_blocks) - 1].gab.end);

	if (TSN_lt(asoc->highest_sacked, highest_tsn)) {
		highest_new_tsn = highest_tsn;
		asoc->highest_sacked = highest_tsn;
	} else {
		highest_new_tsn = sctp_highest_new_tsn(sack, asoc);
	}

	/* Run through the retransmit queue.  Credit bytes received
	 * and free those chunks that we can.
	 */
	sctp_check_transmitted(q, &q->retransmit, NULL, sack, highest_new_tsn);
	sctp_mark_missing(q, &q->retransmit, NULL, highest_new_tsn, 0);

	/* Run through the transmitted queue.
	 * Credit bytes received and free those chunks which we can.
	 *
	 * This is a MASSIVE candidate for optimization.
	 */
	list_for_each(pos, transport_list) {
		transport  = list_entry(pos, struct sctp_transport,
					transports);
		sctp_check_transmitted(q, &transport->transmitted,
				       transport, sack, highest_new_tsn);
		/*
		 * SFR-CACC algorithm:
		 * C) Let count_of_newacks be the number of
		 * destinations for which cacc_saw_newack is set.
		 */
		if (transport->cacc.cacc_saw_newack)
			count_of_newacks ++;
	}

	list_for_each(pos, transport_list) {
		transport  = list_entry(pos, struct sctp_transport,
					transports);
		sctp_mark_missing(q, &transport->transmitted, transport,
				  highest_new_tsn, count_of_newacks);
	}

	/* Move the Cumulative TSN Ack Point if appropriate.  */
	if (TSN_lt(asoc->ctsn_ack_point, sack_ctsn))
		asoc->ctsn_ack_point = sack_ctsn;

	/* Update unack_data field in the assoc. */
	sctp_sack_update_unack_data(asoc, sack);

	ctsn = asoc->ctsn_ack_point;

	SCTP_DEBUG_PRINTK("%s: sack Cumulative TSN Ack is 0x%x.\n",
			  __FUNCTION__, sack_ctsn);
	SCTP_DEBUG_PRINTK("%s: Cumulative TSN Ack of association "
			  "%p is 0x%x.\n", __FUNCTION__, asoc, ctsn);

	/* Throw away stuff rotting on the sack queue.  */
	list_for_each_safe(lchunk, temp, &q->sacked) {
		tchunk = list_entry(lchunk, struct sctp_chunk,
				    transmitted_list);
		tsn = ntohl(tchunk->subh.data_hdr->tsn);
		if (TSN_lte(tsn, ctsn))
			sctp_chunk_free(tchunk);
	}

	/* ii) Set rwnd equal to the newly received a_rwnd minus the
	 *     number of bytes still outstanding after processing the
	 *     Cumulative TSN Ack and the Gap Ack Blocks.
	 */

	sack_a_rwnd = ntohl(sack->a_rwnd);
	outstanding = q->outstanding_bytes;

	if (outstanding < sack_a_rwnd)
		sack_a_rwnd -= outstanding;
	else
		sack_a_rwnd = 0;

	asoc->peer.rwnd = sack_a_rwnd;

	/* See if all chunks are acked.
	 * Make sure the empty queue handler will get run later.
	 */
	q->empty = skb_queue_empty(&q->out) && list_empty(&q->retransmit);
	if (!q->empty)
		goto finish;

	list_for_each(pos, transport_list) {
		transport  = list_entry(pos, struct sctp_transport,
					transports);
		q->empty = q->empty && list_empty(&transport->transmitted);
		if (!q->empty)
			goto finish;
	}

	SCTP_DEBUG_PRINTK("sack queue is empty.\n");
finish:
	return q->empty;
}

/* Is the outqueue empty?  */
int sctp_outq_is_empty(const struct sctp_outq *q)
{
	return q->empty;
}

/********************************************************************
 * 2nd Level Abstractions
 ********************************************************************/

/* Go through a transport's transmitted list or the association's retransmit
 * list and move chunks that are acked by the Cumulative TSN Ack to q->sacked.
 * The retransmit list will not have an associated transport.
 *
 * I added coherent debug information output.	--xguo
 *
 * Instead of printing 'sacked' or 'kept' for each TSN on the
 * transmitted_queue, we print a range: SACKED: TSN1-TSN2, TSN3, TSN4-TSN5.
 * KEPT TSN6-TSN7, etc.
 */
static void sctp_check_transmitted(struct sctp_outq *q,
				   struct list_head *transmitted_queue,
				   struct sctp_transport *transport,
				   struct sctp_sackhdr *sack,
				   __u32 highest_new_tsn_in_sack)
{
	struct list_head *lchunk;
	struct sctp_chunk *tchunk;
	struct list_head tlist;
	__u32 tsn;
	__u32 sack_ctsn;
	__u32 rtt;
	__u8 restart_timer = 0;
	int bytes_acked = 0;

	/* These state variables are for coherent debug output. --xguo */

#if SCTP_DEBUG
	__u32 dbg_ack_tsn = 0;	/* An ACKed TSN range starts here... */
	__u32 dbg_last_ack_tsn = 0;  /* ...and finishes here.	     */
	__u32 dbg_kept_tsn = 0;	/* An un-ACKed range starts here...  */
	__u32 dbg_last_kept_tsn = 0; /* ...and finishes here.	     */

	/* 0 : The last TSN was ACKed.
	 * 1 : The last TSN was NOT ACKed (i.e. KEPT).
	 * -1: We need to initialize.
	 */
	int dbg_prt_state = -1;
#endif /* SCTP_DEBUG */

	sack_ctsn = ntohl(sack->cum_tsn_ack);

	INIT_LIST_HEAD(&tlist);

	/* The while loop will skip empty transmitted queues. */
	while (NULL != (lchunk = sctp_list_dequeue(transmitted_queue))) {
		tchunk = list_entry(lchunk, struct sctp_chunk,
				    transmitted_list);

		tsn = ntohl(tchunk->subh.data_hdr->tsn);
		if (sctp_acked(sack, tsn)) {
			/* If this queue is the retransmit queue, the
			 * retransmit timer has already reclaimed
			 * the outstanding bytes for this chunk, so only
			 * count bytes associated with a transport.
			 */
			if (transport) {
				/* If this chunk is being used for RTT
				 * measurement, calculate the RTT and update
				 * the RTO using this value.
				 *
				 * 6.3.1 C5) Karn's algorithm: RTT measurements
				 * MUST NOT be made using packets that were
				 * retransmitted (and thus for which it is
				 * ambiguous whether the reply was for the
				 * first instance of the packet or a later
				 * instance).
				 */
			   	if (!tchunk->tsn_gap_acked &&
				    !tchunk->resent &&
				    tchunk->rtt_in_progress) {
					rtt = jiffies - tchunk->sent_at;
					sctp_transport_update_rto(transport,
								  rtt);
				}
			}
                        if (TSN_lte(tsn, sack_ctsn)) {
				/* RFC 2960  6.3.2 Retransmission Timer Rules
				 *
				 * R3) Whenever a SACK is received
				 * that acknowledges the DATA chunk
				 * with the earliest outstanding TSN
				 * for that address, restart T3-rtx
				 * timer for that address with its
				 * current RTO.
				 */
				restart_timer = 1;

				if (!tchunk->tsn_gap_acked) {
					tchunk->tsn_gap_acked = 1;
					bytes_acked += sctp_data_size(tchunk);
					/*
					 * SFR-CACC algorithm:
					 * 2) If the SACK contains gap acks
					 * and the flag CHANGEOVER_ACTIVE is
					 * set the receiver of the SACK MUST
					 * take the following action:
					 *
					 * B) For each TSN t being acked that
					 * has not been acked in any SACK so
					 * far, set cacc_saw_newack to 1 for
					 * the destination that the TSN was
					 * sent to.
					 */
					if (transport &&
					    sack->num_gap_ack_blocks &&
					    q->asoc->peer.primary_path->cacc.
					    changeover_active)
						transport->cacc.cacc_saw_newack
							= 1;
				}

				list_add_tail(&tchunk->transmitted_list,
					      &q->sacked);
			} else {
				/* RFC2960 7.2.4, sctpimpguide-05 2.8.2
				 * M2) Each time a SACK arrives reporting
				 * 'Stray DATA chunk(s)' record the highest TSN
				 * reported as newly acknowledged, call this
				 * value 'HighestTSNinSack'. A newly
				 * acknowledged DATA chunk is one not
				 * previously acknowledged in a SACK.
				 *
				 * When the SCTP sender of data receives a SACK
				 * chunk that acknowledges, for the first time,
				 * the receipt of a DATA chunk, all the still
				 * unacknowledged DATA chunks whose TSN is
				 * older than that newly acknowledged DATA
				 * chunk, are qualified as 'Stray DATA chunks'.
				 */
				if (!tchunk->tsn_gap_acked) {
					tchunk->tsn_gap_acked = 1;
					bytes_acked += sctp_data_size(tchunk);
				}
				list_add_tail(lchunk, &tlist);
			}

#if SCTP_DEBUG
			switch (dbg_prt_state) {
			case 0:	/* last TSN was ACKed */
				if (dbg_last_ack_tsn + 1 == tsn) {
					/* This TSN belongs to the
					 * current ACK range.
					 */
					break;
				}

				if (dbg_last_ack_tsn != dbg_ack_tsn) {
					/* Display the end of the
					 * current range.
					 */
					SCTP_DEBUG_PRINTK("-%08x",
							  dbg_last_ack_tsn);
				}

				/* Start a new range.  */
				SCTP_DEBUG_PRINTK(",%08x", tsn);
				dbg_ack_tsn = tsn;
				break;

			case 1:	/* The last TSN was NOT ACKed. */
				if (dbg_last_kept_tsn != dbg_kept_tsn) {
					/* Display the end of current range. */
					SCTP_DEBUG_PRINTK("-%08x",
							  dbg_last_kept_tsn);
				}

				SCTP_DEBUG_PRINTK("\n");

				/* FALL THROUGH... */
			default:
				/* This is the first-ever TSN we examined.  */
				/* Start a new range of ACK-ed TSNs.  */
				SCTP_DEBUG_PRINTK("ACKed: %08x", tsn);
				dbg_prt_state = 0;
				dbg_ack_tsn = tsn;
			};

			dbg_last_ack_tsn = tsn;
#endif /* SCTP_DEBUG */

		} else {
			if (tchunk->tsn_gap_acked) {
				SCTP_DEBUG_PRINTK("%s: Receiver reneged on "
						  "data TSN: 0x%x\n",
						  __FUNCTION__,
						  tsn);
				tchunk->tsn_gap_acked = 0;

				bytes_acked -= sctp_data_size(tchunk);

				/* RFC 2960 6.3.2 Retransmission Timer Rules
				 *
				 * R4) Whenever a SACK is received missing a
				 * TSN that was previously acknowledged via a
				 * Gap Ack Block, start T3-rtx for the
				 * destination address to which the DATA
				 * chunk was originally
				 * transmitted if it is not already running.
				 */
				restart_timer = 1;
			}

			list_add_tail(lchunk, &tlist);

#if SCTP_DEBUG
			/* See the above comments on ACK-ed TSNs. */
			switch (dbg_prt_state) {
			case 1:
				if (dbg_last_kept_tsn + 1 == tsn)
					break;

				if (dbg_last_kept_tsn != dbg_kept_tsn)
					SCTP_DEBUG_PRINTK("-%08x",
							  dbg_last_kept_tsn);

				SCTP_DEBUG_PRINTK(",%08x", tsn);
				dbg_kept_tsn = tsn;
				break;

			case 0:
				if (dbg_last_ack_tsn != dbg_ack_tsn)
					SCTP_DEBUG_PRINTK("-%08x",
							  dbg_last_ack_tsn);
				SCTP_DEBUG_PRINTK("\n");

				/* FALL THROUGH... */
			default:
				SCTP_DEBUG_PRINTK("KEPT: %08x",tsn);
				dbg_prt_state = 1;
				dbg_kept_tsn = tsn;
			};

			dbg_last_kept_tsn = tsn;
#endif /* SCTP_DEBUG */
		}
	}

#if SCTP_DEBUG
	/* Finish off the last range, displaying its ending TSN.  */
	switch (dbg_prt_state) {
	case 0:
		if (dbg_last_ack_tsn != dbg_ack_tsn) {
			SCTP_DEBUG_PRINTK("-%08x\n", dbg_last_ack_tsn);
		} else {
			SCTP_DEBUG_PRINTK("\n");
		}
	break;

	case 1:
		if (dbg_last_kept_tsn != dbg_kept_tsn) {
			SCTP_DEBUG_PRINTK("-%08x\n", dbg_last_kept_tsn);
		} else {
			SCTP_DEBUG_PRINTK("\n");
		}
	};
#endif /* SCTP_DEBUG */
	if (transport) {
		if (bytes_acked) {
			/* 8.2. When an outstanding TSN is acknowledged,
			 * the endpoint shall clear the error counter of
			 * the destination transport address to which the
			 * DATA chunk was last sent.
			 * The association's overall error counter is
			 * also cleared.
			 */
			transport->error_count = 0;
			transport->asoc->overall_error_count = 0;

			/* Mark the destination transport address as
			 * active if it is not so marked.
			 */
			if (!transport->active) {
				sctp_assoc_control_transport(
					transport->asoc,
					transport,
					SCTP_TRANSPORT_UP,
					SCTP_RECEIVED_SACK);
			}

			sctp_transport_raise_cwnd(transport, sack_ctsn,
						  bytes_acked);

			transport->flight_size -= bytes_acked;
			q->outstanding_bytes -= bytes_acked;
		} else {
			/* RFC 2960 6.1, sctpimpguide-06 2.15.2
			 * When a sender is doing zero window probing, it
			 * should not timeout the association if it continues
			 * to receive new packets from the receiver. The
			 * reason is that the receiver MAY keep its window
			 * closed for an indefinite time.
			 * A sender is doing zero window probing when the
			 * receiver's advertised window is zero, and there is
			 * only one data chunk in flight to the receiver.
			 */
			if (!q->asoc->peer.rwnd &&
			    !list_empty(&tlist) &&
			    (sack_ctsn+2 == q->asoc->next_tsn)) {
				SCTP_DEBUG_PRINTK("%s: SACK received for zero "
						  "window probe: %u\n",
						  __FUNCTION__, sack_ctsn);
				q->asoc->overall_error_count = 0;
				transport->error_count = 0;
			}
		}

		/* RFC 2960 6.3.2 Retransmission Timer Rules
		 *
		 * R2) Whenever all outstanding data sent to an address have
		 * been acknowledged, turn off the T3-rtx timer of that
		 * address.
		 */
		if (!transport->flight_size) {
			if (timer_pending(&transport->T3_rtx_timer) &&
			    del_timer(&transport->T3_rtx_timer)) {
				sctp_transport_put(transport);
			}
		} else if (restart_timer) {
			if (!mod_timer(&transport->T3_rtx_timer,
				       jiffies + transport->rto))
				sctp_transport_hold(transport);
		}
	}

	list_splice(&tlist, transmitted_queue);
}

/* Mark chunks as missing and consequently may get retransmitted. */
static void sctp_mark_missing(struct sctp_outq *q,
			      struct list_head *transmitted_queue,
			      struct sctp_transport *transport,
			      __u32 highest_new_tsn_in_sack,
			      int count_of_newacks)
{
	struct sctp_chunk *chunk;
	struct list_head *pos;
	__u32 tsn;
	char do_fast_retransmit = 0;
	struct sctp_transport *primary = q->asoc->peer.primary_path;

	list_for_each(pos, transmitted_queue) {

		chunk = list_entry(pos, struct sctp_chunk, transmitted_list);
		tsn = ntohl(chunk->subh.data_hdr->tsn);

		/* RFC 2960 7.2.4, sctpimpguide-05 2.8.2 M3) Examine all
		 * 'Unacknowledged TSN's', if the TSN number of an
		 * 'Unacknowledged TSN' is smaller than the 'HighestTSNinSack'
		 * value, increment the 'TSN.Missing.Report' count on that
		 * chunk if it has NOT been fast retransmitted or marked for
		 * fast retransmit already.
		 */
		if (!chunk->fast_retransmit &&
		    !chunk->tsn_gap_acked &&
		    TSN_lt(tsn, highest_new_tsn_in_sack)) {

			/* SFR-CACC may require us to skip marking
			 * this chunk as missing.
			 */
			if (!transport || !sctp_cacc_skip(primary, transport,
					    count_of_newacks, tsn)) {
				chunk->tsn_missing_report++;

				SCTP_DEBUG_PRINTK(
					"%s: TSN 0x%x missing counter: %d\n",
					__FUNCTION__, tsn,
					chunk->tsn_missing_report);
			}
		}
		/*
		 * M4) If any DATA chunk is found to have a
		 * 'TSN.Missing.Report'
		 * value larger than or equal to 4, mark that chunk for
		 * retransmission and start the fast retransmit procedure.
		 */

		if (chunk->tsn_missing_report >= 4) {
			chunk->fast_retransmit = 1;
			do_fast_retransmit = 1;
		}
	}

	if (transport) {
		if (do_fast_retransmit)
			sctp_retransmit(q, transport, SCTP_RTXR_FAST_RTX);

		SCTP_DEBUG_PRINTK("%s: transport: %p, cwnd: %d, "
				  "ssthresh: %d, flight_size: %d, pba: %d\n",
				  __FUNCTION__, transport, transport->cwnd,
			  	  transport->ssthresh, transport->flight_size,
				  transport->partial_bytes_acked);
	}
}

/* Is the given TSN acked by this packet?  */
static int sctp_acked(struct sctp_sackhdr *sack, __u32 tsn)
{
	int i;
	sctp_sack_variable_t *frags;
	__u16 gap;
	__u32 ctsn = ntohl(sack->cum_tsn_ack);

        if (TSN_lte(tsn, ctsn))
		goto pass;

	/* 3.3.4 Selective Acknowledgement (SACK) (3):
	 *
	 * Gap Ack Blocks:
	 *  These fields contain the Gap Ack Blocks. They are repeated
	 *  for each Gap Ack Block up to the number of Gap Ack Blocks
	 *  defined in the Number of Gap Ack Blocks field. All DATA
	 *  chunks with TSNs greater than or equal to (Cumulative TSN
	 *  Ack + Gap Ack Block Start) and less than or equal to
	 *  (Cumulative TSN Ack + Gap Ack Block End) of each Gap Ack
	 *  Block are assumed to have been received correctly.
	 */

	frags = sack->variable;
	gap = tsn - ctsn;
	for (i = 0; i < ntohs(sack->num_gap_ack_blocks); ++i) {
		if (TSN_lte(ntohs(frags[i].gab.start), gap) &&
		    TSN_lte(gap, ntohs(frags[i].gab.end)))
			goto pass;
	}

	return 0;
pass:
	return 1;
}
