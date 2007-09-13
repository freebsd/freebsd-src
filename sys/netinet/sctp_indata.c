/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_indata.c,v 1.36 2005/03/06 16:04:17 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_input.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_timer.h>


/*
 * NOTES: On the outbound side of things I need to check the sack timer to
 * see if I should generate a sack into the chunk queue (if I have data to
 * send that is and will be sending it .. for bundling.
 *
 * The callback in sctp_usrreq.c will get called when the socket is read from.
 * This will cause sctp_service_queues() to get called on the top entry in
 * the list.
 */

void
sctp_set_rwnd(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	uint32_t calc, calc_save;

	/*
	 * This is really set wrong with respect to a 1-2-m socket. Since
	 * the sb_cc is the count that everyone as put up. When we re-write
	 * sctp_soreceive then we will fix this so that ONLY this
	 * associations data is taken into account.
	 */
	if (stcb->sctp_socket == NULL)
		return;

	if (stcb->asoc.sb_cc == 0 &&
	    asoc->size_on_reasm_queue == 0 &&
	    asoc->size_on_all_streams == 0) {
		/* Full rwnd granted */
		asoc->my_rwnd = max(SCTP_SB_LIMIT_RCV(stcb->sctp_socket),
		    SCTP_MINIMAL_RWND);
		return;
	}
	/* get actual space */
	calc = (uint32_t) sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv);

	/*
	 * take out what has NOT been put on socket queue and we yet hold
	 * for putting up.
	 */
	calc = sctp_sbspace_sub(calc, (uint32_t) asoc->size_on_reasm_queue);
	calc = sctp_sbspace_sub(calc, (uint32_t) asoc->size_on_all_streams);

	if (calc == 0) {
		/* out of space */
		asoc->my_rwnd = 0;
		return;
	}
	/* what is the overhead of all these rwnd's */

	calc = sctp_sbspace_sub(calc, stcb->asoc.my_rwnd_control_len);
	calc_save = calc;

	asoc->my_rwnd = calc;
	if ((asoc->my_rwnd == 0) &&
	    (calc < stcb->asoc.my_rwnd_control_len)) {
		/*-
		 * If our rwnd == 0 && the overhead is greater than the
 		 * data onqueue, we clamp the rwnd to 1. This lets us
 		 * still accept inbound segments, but hopefully will shut
 		 * the sender down when he finally gets the message. This
		 * hopefully will gracefully avoid discarding packets.
 		 */
		asoc->my_rwnd = 1;
	}
	if (asoc->my_rwnd &&
	    (asoc->my_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_receiver)) {
		/* SWS engaged, tell peer none left */
		asoc->my_rwnd = 1;
	}
}

/* Calculate what the rwnd would be */
uint32_t
sctp_calc_rwnd(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	uint32_t calc = 0, calc_save = 0, result = 0;

	/*
	 * This is really set wrong with respect to a 1-2-m socket. Since
	 * the sb_cc is the count that everyone as put up. When we re-write
	 * sctp_soreceive then we will fix this so that ONLY this
	 * associations data is taken into account.
	 */
	if (stcb->sctp_socket == NULL)
		return (calc);

	if (stcb->asoc.sb_cc == 0 &&
	    asoc->size_on_reasm_queue == 0 &&
	    asoc->size_on_all_streams == 0) {
		/* Full rwnd granted */
		calc = max(SCTP_SB_LIMIT_RCV(stcb->sctp_socket),
		    SCTP_MINIMAL_RWND);
		return (calc);
	}
	/* get actual space */
	calc = (uint32_t) sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv);

	/*
	 * take out what has NOT been put on socket queue and we yet hold
	 * for putting up.
	 */
	calc = sctp_sbspace_sub(calc, (uint32_t) asoc->size_on_reasm_queue);
	calc = sctp_sbspace_sub(calc, (uint32_t) asoc->size_on_all_streams);

	if (calc == 0) {
		/* out of space */
		return (calc);
	}
	/* what is the overhead of all these rwnd's */
	calc = sctp_sbspace_sub(calc, stcb->asoc.my_rwnd_control_len);
	calc_save = calc;

	result = calc;
	if ((result == 0) &&
	    (calc < stcb->asoc.my_rwnd_control_len)) {
		/*-
		 * If our rwnd == 0 && the overhead is greater than the
 		 * data onqueue, we clamp the rwnd to 1. This lets us
 		 * still accept inbound segments, but hopefully will shut
 		 * the sender down when he finally gets the message. This
		 * hopefully will gracefully avoid discarding packets.
 		 */
		result = 1;
	}
	if (asoc->my_rwnd &&
	    (asoc->my_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_receiver)) {
		/* SWS engaged, tell peer none left */
		result = 1;
	}
	return (result);
}



/*
 * Build out our readq entry based on the incoming packet.
 */
struct sctp_queued_to_read *
sctp_build_readq_entry(struct sctp_tcb *stcb,
    struct sctp_nets *net,
    uint32_t tsn, uint32_t ppid,
    uint32_t context, uint16_t stream_no,
    uint16_t stream_seq, uint8_t flags,
    struct mbuf *dm)
{
	struct sctp_queued_to_read *read_queue_e = NULL;

	sctp_alloc_a_readq(stcb, read_queue_e);
	if (read_queue_e == NULL) {
		goto failed_build;
	}
	read_queue_e->sinfo_stream = stream_no;
	read_queue_e->sinfo_ssn = stream_seq;
	read_queue_e->sinfo_flags = (flags << 8);
	read_queue_e->sinfo_ppid = ppid;
	read_queue_e->sinfo_context = stcb->asoc.context;
	read_queue_e->sinfo_timetolive = 0;
	read_queue_e->sinfo_tsn = tsn;
	read_queue_e->sinfo_cumtsn = tsn;
	read_queue_e->sinfo_assoc_id = sctp_get_associd(stcb);
	read_queue_e->whoFrom = net;
	read_queue_e->length = 0;
	atomic_add_int(&net->ref_count, 1);
	read_queue_e->data = dm;
	read_queue_e->spec_flags = 0;
	read_queue_e->tail_mbuf = NULL;
	read_queue_e->aux_data = NULL;
	read_queue_e->stcb = stcb;
	read_queue_e->port_from = stcb->rport;
	read_queue_e->do_not_ref_stcb = 0;
	read_queue_e->end_added = 0;
	read_queue_e->some_taken = 0;
	read_queue_e->pdapi_aborted = 0;
failed_build:
	return (read_queue_e);
}


/*
 * Build out our readq entry based on the incoming packet.
 */
static struct sctp_queued_to_read *
sctp_build_readq_entry_chk(struct sctp_tcb *stcb,
    struct sctp_tmit_chunk *chk)
{
	struct sctp_queued_to_read *read_queue_e = NULL;

	sctp_alloc_a_readq(stcb, read_queue_e);
	if (read_queue_e == NULL) {
		goto failed_build;
	}
	read_queue_e->sinfo_stream = chk->rec.data.stream_number;
	read_queue_e->sinfo_ssn = chk->rec.data.stream_seq;
	read_queue_e->sinfo_flags = (chk->rec.data.rcv_flags << 8);
	read_queue_e->sinfo_ppid = chk->rec.data.payloadtype;
	read_queue_e->sinfo_context = stcb->asoc.context;
	read_queue_e->sinfo_timetolive = 0;
	read_queue_e->sinfo_tsn = chk->rec.data.TSN_seq;
	read_queue_e->sinfo_cumtsn = chk->rec.data.TSN_seq;
	read_queue_e->sinfo_assoc_id = sctp_get_associd(stcb);
	read_queue_e->whoFrom = chk->whoTo;
	read_queue_e->aux_data = NULL;
	read_queue_e->length = 0;
	atomic_add_int(&chk->whoTo->ref_count, 1);
	read_queue_e->data = chk->data;
	read_queue_e->tail_mbuf = NULL;
	read_queue_e->stcb = stcb;
	read_queue_e->port_from = stcb->rport;
	read_queue_e->spec_flags = 0;
	read_queue_e->do_not_ref_stcb = 0;
	read_queue_e->end_added = 0;
	read_queue_e->some_taken = 0;
	read_queue_e->pdapi_aborted = 0;
failed_build:
	return (read_queue_e);
}


struct mbuf *
sctp_build_ctl_nchunk(struct sctp_inpcb *inp,
    struct sctp_sndrcvinfo *sinfo)
{
	struct sctp_sndrcvinfo *outinfo;
	struct cmsghdr *cmh;
	struct mbuf *ret;
	int len;
	int use_extended = 0;

	if (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT)) {
		/* user does not want the sndrcv ctl */
		return (NULL);
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXT_RCVINFO)) {
		use_extended = 1;
		len = CMSG_LEN(sizeof(struct sctp_extrcvinfo));
	} else {
		len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
	}


	ret = sctp_get_mbuf_for_msg(len,
	    0, M_DONTWAIT, 1, MT_DATA);

	if (ret == NULL) {
		/* No space */
		return (ret);
	}
	/* We need a CMSG header followed by the struct  */
	cmh = mtod(ret, struct cmsghdr *);
	outinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmh);
	cmh->cmsg_level = IPPROTO_SCTP;
	if (use_extended) {
		cmh->cmsg_type = SCTP_EXTRCV;
		cmh->cmsg_len = len;
		memcpy(outinfo, sinfo, len);
	} else {
		cmh->cmsg_type = SCTP_SNDRCV;
		cmh->cmsg_len = len;
		*outinfo = *sinfo;
	}
	SCTP_BUF_LEN(ret) = cmh->cmsg_len;
	return (ret);
}


char *
sctp_build_ctl_cchunk(struct sctp_inpcb *inp,
    int *control_len,
    struct sctp_sndrcvinfo *sinfo)
{
	struct sctp_sndrcvinfo *outinfo;
	struct cmsghdr *cmh;
	char *buf;
	int len;
	int use_extended = 0;

	if (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT)) {
		/* user does not want the sndrcv ctl */
		return (NULL);
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXT_RCVINFO)) {
		use_extended = 1;
		len = CMSG_LEN(sizeof(struct sctp_extrcvinfo));
	} else {
		len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
	}
	SCTP_MALLOC(buf, char *, len, SCTP_M_CMSG);
	if (buf == NULL) {
		/* No space */
		return (buf);
	}
	/* We need a CMSG header followed by the struct  */
	cmh = (struct cmsghdr *)buf;
	outinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmh);
	cmh->cmsg_level = IPPROTO_SCTP;
	if (use_extended) {
		cmh->cmsg_type = SCTP_EXTRCV;
		cmh->cmsg_len = len;
		memcpy(outinfo, sinfo, len);
	} else {
		cmh->cmsg_type = SCTP_SNDRCV;
		cmh->cmsg_len = len;
		*outinfo = *sinfo;
	}
	*control_len = len;
	return (buf);
}


/*
 * We are delivering currently from the reassembly queue. We must continue to
 * deliver until we either: 1) run out of space. 2) run out of sequential
 * TSN's 3) hit the SCTP_DATA_LAST_FRAG flag.
 */
static void
sctp_service_reassembly(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	uint16_t nxt_todel;
	uint16_t stream_no;
	int end = 0;
	int cntDel;
	struct sctp_queued_to_read *control, *ctl, *ctlat;

	if (stcb == NULL)
		return;

	cntDel = stream_no = 0;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) ||
	    (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) ||
	    (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET)) {
		/* socket above is long gone or going.. */
abandon:
		asoc->fragmented_delivery_inprogress = 0;
		chk = TAILQ_FIRST(&asoc->reasmqueue);
		while (chk) {
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			asoc->size_on_reasm_queue -= chk->send_size;
			sctp_ucount_decr(asoc->cnt_on_reasm_queue);
			/*
			 * Lose the data pointer, since its in the socket
			 * buffer
			 */
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			/* Now free the address and data */
			sctp_free_a_chunk(stcb, chk);
			/* sa_ignore FREED_MEMORY */
			chk = TAILQ_FIRST(&asoc->reasmqueue);
		}
		return;
	}
	SCTP_TCB_LOCK_ASSERT(stcb);
	do {
		chk = TAILQ_FIRST(&asoc->reasmqueue);
		if (chk == NULL) {
			return;
		}
		if (chk->rec.data.TSN_seq != (asoc->tsn_last_delivered + 1)) {
			/* Can't deliver more :< */
			return;
		}
		stream_no = chk->rec.data.stream_number;
		nxt_todel = asoc->strmin[stream_no].last_sequence_delivered + 1;
		if (nxt_todel != chk->rec.data.stream_seq &&
		    (chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) == 0) {
			/*
			 * Not the next sequence to deliver in its stream OR
			 * unordered
			 */
			return;
		}
		if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {

			control = sctp_build_readq_entry_chk(stcb, chk);
			if (control == NULL) {
				/* out of memory? */
				return;
			}
			/* save it off for our future deliveries */
			stcb->asoc.control_pdapi = control;
			if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG)
				end = 1;
			else
				end = 0;
			sctp_add_to_readq(stcb->sctp_ep,
			    stcb, control, &stcb->sctp_socket->so_rcv, end, SCTP_SO_NOT_LOCKED);
			cntDel++;
		} else {
			if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG)
				end = 1;
			else
				end = 0;
			if (sctp_append_to_readq(stcb->sctp_ep, stcb,
			    stcb->asoc.control_pdapi,
			    chk->data, end, chk->rec.data.TSN_seq,
			    &stcb->sctp_socket->so_rcv)) {
				/*
				 * something is very wrong, either
				 * control_pdapi is NULL, or the tail_mbuf
				 * is corrupt, or there is a EOM already on
				 * the mbuf chain.
				 */
				if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
					goto abandon;
				} else {
					if ((stcb->asoc.control_pdapi == NULL) || (stcb->asoc.control_pdapi->tail_mbuf == NULL)) {
						panic("This should not happen control_pdapi NULL?");
					}
					/* if we did not panic, it was a EOM */
					panic("Bad chunking ??");
					return;
				}
			}
			cntDel++;
		}
		/* pull it we did it */
		TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
		if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {
			asoc->fragmented_delivery_inprogress = 0;
			if ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) == 0) {
				asoc->strmin[stream_no].last_sequence_delivered++;
			}
			if ((chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) == 0) {
				SCTP_STAT_INCR_COUNTER64(sctps_reasmusrmsgs);
			}
		} else if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {
			/*
			 * turn the flag back on since we just  delivered
			 * yet another one.
			 */
			asoc->fragmented_delivery_inprogress = 1;
		}
		asoc->tsn_of_pdapi_last_delivered = chk->rec.data.TSN_seq;
		asoc->last_flags_delivered = chk->rec.data.rcv_flags;
		asoc->last_strm_seq_delivered = chk->rec.data.stream_seq;
		asoc->last_strm_no_delivered = chk->rec.data.stream_number;

		asoc->tsn_last_delivered = chk->rec.data.TSN_seq;
		asoc->size_on_reasm_queue -= chk->send_size;
		sctp_ucount_decr(asoc->cnt_on_reasm_queue);
		/* free up the chk */
		chk->data = NULL;
		sctp_free_a_chunk(stcb, chk);

		if (asoc->fragmented_delivery_inprogress == 0) {
			/*
			 * Now lets see if we can deliver the next one on
			 * the stream
			 */
			struct sctp_stream_in *strm;

			strm = &asoc->strmin[stream_no];
			nxt_todel = strm->last_sequence_delivered + 1;
			ctl = TAILQ_FIRST(&strm->inqueue);
			if (ctl && (nxt_todel == ctl->sinfo_ssn)) {
				while (ctl != NULL) {
					/* Deliver more if we can. */
					if (nxt_todel == ctl->sinfo_ssn) {
						ctlat = TAILQ_NEXT(ctl, next);
						TAILQ_REMOVE(&strm->inqueue, ctl, next);
						asoc->size_on_all_streams -= ctl->length;
						sctp_ucount_decr(asoc->cnt_on_all_streams);
						strm->last_sequence_delivered++;
						sctp_add_to_readq(stcb->sctp_ep, stcb,
						    ctl,
						    &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
						ctl = ctlat;
					} else {
						break;
					}
					nxt_todel = strm->last_sequence_delivered + 1;
				}
			}
			break;
		}
		/* sa_ignore FREED_MEMORY */
		chk = TAILQ_FIRST(&asoc->reasmqueue);
	} while (chk);
}

/*
 * Queue the chunk either right into the socket buffer if it is the next one
 * to go OR put it in the correct place in the delivery queue.  If we do
 * append to the so_buf, keep doing so until we are out of order. One big
 * question still remains, what to do when the socket buffer is FULL??
 */
static void
sctp_queue_data_to_stream(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_queued_to_read *control, int *abort_flag)
{
	/*
	 * FIX-ME maybe? What happens when the ssn wraps? If we are getting
	 * all the data in one stream this could happen quite rapidly. One
	 * could use the TSN to keep track of things, but this scheme breaks
	 * down in the other type of stream useage that could occur. Send a
	 * single msg to stream 0, send 4Billion messages to stream 1, now
	 * send a message to stream 0. You have a situation where the TSN
	 * has wrapped but not in the stream. Is this worth worrying about
	 * or should we just change our queue sort at the bottom to be by
	 * TSN.
	 * 
	 * Could it also be legal for a peer to send ssn 1 with TSN 2 and ssn 2
	 * with TSN 1? If the peer is doing some sort of funky TSN/SSN
	 * assignment this could happen... and I don't see how this would be
	 * a violation. So for now I am undecided an will leave the sort by
	 * SSN alone. Maybe a hybred approach is the answer
	 * 
	 */
	struct sctp_stream_in *strm;
	struct sctp_queued_to_read *at;
	int queue_needed;
	uint16_t nxt_todel;
	struct mbuf *oper;

	queue_needed = 1;
	asoc->size_on_all_streams += control->length;
	sctp_ucount_incr(asoc->cnt_on_all_streams);
	strm = &asoc->strmin[control->sinfo_stream];
	nxt_todel = strm->last_sequence_delivered + 1;
	if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
		sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_INTO_STRD);
	}
	SCTPDBG(SCTP_DEBUG_INDATA1,
	    "queue to stream called for ssn:%u lastdel:%u nxt:%u\n",
	    (uint32_t) control->sinfo_stream,
	    (uint32_t) strm->last_sequence_delivered,
	    (uint32_t) nxt_todel);
	if (compare_with_wrap(strm->last_sequence_delivered,
	    control->sinfo_ssn, MAX_SEQ) ||
	    (strm->last_sequence_delivered == control->sinfo_ssn)) {
		/* The incoming sseq is behind where we last delivered? */
		SCTPDBG(SCTP_DEBUG_INDATA1, "Duplicate S-SEQ:%d delivered:%d from peer, Abort  association\n",
		    control->sinfo_ssn, strm->last_sequence_delivered);
		/*
		 * throw it in the stream so it gets cleaned up in
		 * association destruction
		 */
		TAILQ_INSERT_HEAD(&strm->inqueue, control, next);
		oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
		    0, M_DONTWAIT, 1, MT_DATA);
		if (oper) {
			struct sctp_paramhdr *ph;
			uint32_t *ippp;

			SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
			    (sizeof(uint32_t) * 3);
			ph = mtod(oper, struct sctp_paramhdr *);
			ph->param_type = htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
			ph->param_length = htons(SCTP_BUF_LEN(oper));
			ippp = (uint32_t *) (ph + 1);
			*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_1);
			ippp++;
			*ippp = control->sinfo_tsn;
			ippp++;
			*ippp = ((control->sinfo_stream << 16) | control->sinfo_ssn);
		}
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_1;
		sctp_abort_an_association(stcb->sctp_ep, stcb,
		    SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

		*abort_flag = 1;
		return;

	}
	if (nxt_todel == control->sinfo_ssn) {
		/* can be delivered right away? */
		if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
			sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_IMMED_DEL);
		}
		queue_needed = 0;
		asoc->size_on_all_streams -= control->length;
		sctp_ucount_decr(asoc->cnt_on_all_streams);
		strm->last_sequence_delivered++;
		sctp_add_to_readq(stcb->sctp_ep, stcb,
		    control,
		    &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
		control = TAILQ_FIRST(&strm->inqueue);
		while (control != NULL) {
			/* all delivered */
			nxt_todel = strm->last_sequence_delivered + 1;
			if (nxt_todel == control->sinfo_ssn) {
				at = TAILQ_NEXT(control, next);
				TAILQ_REMOVE(&strm->inqueue, control, next);
				asoc->size_on_all_streams -= control->length;
				sctp_ucount_decr(asoc->cnt_on_all_streams);
				strm->last_sequence_delivered++;
				/*
				 * We ignore the return of deliver_data here
				 * since we always can hold the chunk on the
				 * d-queue. And we have a finite number that
				 * can be delivered from the strq.
				 */
				if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
					sctp_log_strm_del(control, NULL,
					    SCTP_STR_LOG_FROM_IMMED_DEL);
				}
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				    control,
				    &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
				control = at;
				continue;
			}
			break;
		}
	}
	if (queue_needed) {
		/*
		 * Ok, we did not deliver this guy, find the correct place
		 * to put it on the queue.
		 */
		if (TAILQ_EMPTY(&strm->inqueue)) {
			/* Empty queue */
			if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
				sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_INSERT_HD);
			}
			TAILQ_INSERT_HEAD(&strm->inqueue, control, next);
		} else {
			TAILQ_FOREACH(at, &strm->inqueue, next) {
				if (compare_with_wrap(at->sinfo_ssn,
				    control->sinfo_ssn, MAX_SEQ)) {
					/*
					 * one in queue is bigger than the
					 * new one, insert before this one
					 */
					if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
						sctp_log_strm_del(control, at,
						    SCTP_STR_LOG_FROM_INSERT_MD);
					}
					TAILQ_INSERT_BEFORE(at, control, next);
					break;
				} else if (at->sinfo_ssn == control->sinfo_ssn) {
					/*
					 * Gak, He sent me a duplicate str
					 * seq number
					 */
					/*
					 * foo bar, I guess I will just free
					 * this new guy, should we abort
					 * too? FIX ME MAYBE? Or it COULD be
					 * that the SSN's have wrapped.
					 * Maybe I should compare to TSN
					 * somehow... sigh for now just blow
					 * away the chunk!
					 */

					if (control->data)
						sctp_m_freem(control->data);
					control->data = NULL;
					asoc->size_on_all_streams -= control->length;
					sctp_ucount_decr(asoc->cnt_on_all_streams);
					if (control->whoFrom)
						sctp_free_remote_addr(control->whoFrom);
					control->whoFrom = NULL;
					sctp_free_a_readq(stcb, control);
					return;
				} else {
					if (TAILQ_NEXT(at, next) == NULL) {
						/*
						 * We are at the end, insert
						 * it after this one
						 */
						if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
							sctp_log_strm_del(control, at,
							    SCTP_STR_LOG_FROM_INSERT_TL);
						}
						TAILQ_INSERT_AFTER(&strm->inqueue,
						    at, control, next);
						break;
					}
				}
			}
		}
	}
}

/*
 * Returns two things: You get the total size of the deliverable parts of the
 * first fragmented message on the reassembly queue. And you get a 1 back if
 * all of the message is ready or a 0 back if the message is still incomplete
 */
static int
sctp_is_all_msg_on_reasm(struct sctp_association *asoc, uint32_t * t_size)
{
	struct sctp_tmit_chunk *chk;
	uint32_t tsn;

	*t_size = 0;
	chk = TAILQ_FIRST(&asoc->reasmqueue);
	if (chk == NULL) {
		/* nothing on the queue */
		return (0);
	}
	if ((chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) == 0) {
		/* Not a first on the queue */
		return (0);
	}
	tsn = chk->rec.data.TSN_seq;
	while (chk) {
		if (tsn != chk->rec.data.TSN_seq) {
			return (0);
		}
		*t_size += chk->send_size;
		if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {
			return (1);
		}
		tsn++;
		chk = TAILQ_NEXT(chk, sctp_next);
	}
	return (0);
}

static void
sctp_deliver_reasm_check(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	uint16_t nxt_todel;
	uint32_t tsize;

doit_again:
	chk = TAILQ_FIRST(&asoc->reasmqueue);
	if (chk == NULL) {
		/* Huh? */
		asoc->size_on_reasm_queue = 0;
		asoc->cnt_on_reasm_queue = 0;
		return;
	}
	if (asoc->fragmented_delivery_inprogress == 0) {
		nxt_todel =
		    asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered + 1;
		if ((chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) &&
		    (nxt_todel == chk->rec.data.stream_seq ||
		    (chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED))) {
			/*
			 * Yep the first one is here and its ok to deliver
			 * but should we?
			 */
			if ((sctp_is_all_msg_on_reasm(asoc, &tsize) ||
			    (tsize >= stcb->sctp_ep->partial_delivery_point))) {

				/*
				 * Yes, we setup to start reception, by
				 * backing down the TSN just in case we
				 * can't deliver. If we
				 */
				asoc->fragmented_delivery_inprogress = 1;
				asoc->tsn_last_delivered =
				    chk->rec.data.TSN_seq - 1;
				asoc->str_of_pdapi =
				    chk->rec.data.stream_number;
				asoc->ssn_of_pdapi = chk->rec.data.stream_seq;
				asoc->pdapi_ppid = chk->rec.data.payloadtype;
				asoc->fragment_flags = chk->rec.data.rcv_flags;
				sctp_service_reassembly(stcb, asoc);
			}
		}
	} else {
		/*
		 * Service re-assembly will deliver stream data queued at
		 * the end of fragmented delivery.. but it wont know to go
		 * back and call itself again... we do that here with the
		 * got doit_again
		 */
		sctp_service_reassembly(stcb, asoc);
		if (asoc->fragmented_delivery_inprogress == 0) {
			/*
			 * finished our Fragmented delivery, could be more
			 * waiting?
			 */
			goto doit_again;
		}
	}
}

/*
 * Dump onto the re-assembly queue, in its proper place. After dumping on the
 * queue, see if anthing can be delivered. If so pull it off (or as much as
 * we can. If we run out of space then we must dump what we can and set the
 * appropriate flag to say we queued what we could.
 */
static void
sctp_queue_data_for_reasm(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_tmit_chunk *chk, int *abort_flag)
{
	struct mbuf *oper;
	uint32_t cum_ackp1, last_tsn, prev_tsn, post_tsn;
	u_char last_flags;
	struct sctp_tmit_chunk *at, *prev, *next;

	prev = next = NULL;
	cum_ackp1 = asoc->tsn_last_delivered + 1;
	if (TAILQ_EMPTY(&asoc->reasmqueue)) {
		/* This is the first one on the queue */
		TAILQ_INSERT_HEAD(&asoc->reasmqueue, chk, sctp_next);
		/*
		 * we do not check for delivery of anything when only one
		 * fragment is here
		 */
		asoc->size_on_reasm_queue = chk->send_size;
		sctp_ucount_incr(asoc->cnt_on_reasm_queue);
		if (chk->rec.data.TSN_seq == cum_ackp1) {
			if (asoc->fragmented_delivery_inprogress == 0 &&
			    (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) !=
			    SCTP_DATA_FIRST_FRAG) {
				/*
				 * An empty queue, no delivery inprogress,
				 * we hit the next one and it does NOT have
				 * a FIRST fragment mark.
				 */
				SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, its not first, no fragmented delivery in progress\n");
				oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);

				if (oper) {
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(oper) =
					    sizeof(struct sctp_paramhdr) +
					    (sizeof(uint32_t) * 3);
					ph = mtod(oper, struct sctp_paramhdr *);
					ph->param_type =
					    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
					ph->param_length = htons(SCTP_BUF_LEN(oper));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_2);
					ippp++;
					*ippp = chk->rec.data.TSN_seq;
					ippp++;
					*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);

				}
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_2;
				sctp_abort_an_association(stcb->sctp_ep, stcb,
				    SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
				*abort_flag = 1;
			} else if (asoc->fragmented_delivery_inprogress &&
			    (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) == SCTP_DATA_FIRST_FRAG) {
				/*
				 * We are doing a partial delivery and the
				 * NEXT chunk MUST be either the LAST or
				 * MIDDLE fragment NOT a FIRST
				 */
				SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, it IS a first and fragmented delivery in progress\n");
				oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);
				if (oper) {
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(oper) =
					    sizeof(struct sctp_paramhdr) +
					    (3 * sizeof(uint32_t));
					ph = mtod(oper, struct sctp_paramhdr *);
					ph->param_type =
					    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
					ph->param_length = htons(SCTP_BUF_LEN(oper));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_3);
					ippp++;
					*ippp = chk->rec.data.TSN_seq;
					ippp++;
					*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);
				}
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_3;
				sctp_abort_an_association(stcb->sctp_ep, stcb,
				    SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
				*abort_flag = 1;
			} else if (asoc->fragmented_delivery_inprogress) {
				/*
				 * Here we are ok with a MIDDLE or LAST
				 * piece
				 */
				if (chk->rec.data.stream_number !=
				    asoc->str_of_pdapi) {
					/* Got to be the right STR No */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, it IS not same stream number %d vs %d\n",
					    chk->rec.data.stream_number,
					    asoc->str_of_pdapi);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (sizeof(uint32_t) * 3);
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_4);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_4;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
				} else if ((asoc->fragment_flags & SCTP_DATA_UNORDERED) !=
					    SCTP_DATA_UNORDERED &&
					    chk->rec.data.stream_seq !=
				    asoc->ssn_of_pdapi) {
					/* Got to be the right STR Seq */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, it IS not same stream seq %d vs %d\n",
					    chk->rec.data.stream_seq,
					    asoc->ssn_of_pdapi);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_5);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);

					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_5;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
				}
			}
		}
		return;
	}
	/* Find its place */
	TAILQ_FOREACH(at, &asoc->reasmqueue, sctp_next) {
		if (compare_with_wrap(at->rec.data.TSN_seq,
		    chk->rec.data.TSN_seq, MAX_TSN)) {
			/*
			 * one in queue is bigger than the new one, insert
			 * before this one
			 */
			/* A check */
			asoc->size_on_reasm_queue += chk->send_size;
			sctp_ucount_incr(asoc->cnt_on_reasm_queue);
			next = at;
			TAILQ_INSERT_BEFORE(at, chk, sctp_next);
			break;
		} else if (at->rec.data.TSN_seq == chk->rec.data.TSN_seq) {
			/* Gak, He sent me a duplicate str seq number */
			/*
			 * foo bar, I guess I will just free this new guy,
			 * should we abort too? FIX ME MAYBE? Or it COULD be
			 * that the SSN's have wrapped. Maybe I should
			 * compare to TSN somehow... sigh for now just blow
			 * away the chunk!
			 */
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			sctp_free_a_chunk(stcb, chk);
			return;
		} else {
			last_flags = at->rec.data.rcv_flags;
			last_tsn = at->rec.data.TSN_seq;
			prev = at;
			if (TAILQ_NEXT(at, sctp_next) == NULL) {
				/*
				 * We are at the end, insert it after this
				 * one
				 */
				/* check it first */
				asoc->size_on_reasm_queue += chk->send_size;
				sctp_ucount_incr(asoc->cnt_on_reasm_queue);
				TAILQ_INSERT_AFTER(&asoc->reasmqueue, at, chk, sctp_next);
				break;
			}
		}
	}
	/* Now the audits */
	if (prev) {
		prev_tsn = chk->rec.data.TSN_seq - 1;
		if (prev_tsn == prev->rec.data.TSN_seq) {
			/*
			 * Ok the one I am dropping onto the end is the
			 * NEXT. A bit of valdiation here.
			 */
			if ((prev->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
			    SCTP_DATA_FIRST_FRAG ||
			    (prev->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
			    SCTP_DATA_MIDDLE_FRAG) {
				/*
				 * Insert chk MUST be a MIDDLE or LAST
				 * fragment
				 */
				if ((chk->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
				    SCTP_DATA_FIRST_FRAG) {
					SCTPDBG(SCTP_DEBUG_INDATA1, "Prev check - It can be a midlle or last but not a first\n");
					SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, it's a FIRST!\n");
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_6);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);

					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_6;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					return;
				}
				if (chk->rec.data.stream_number !=
				    prev->rec.data.stream_number) {
					/*
					 * Huh, need the correct STR here,
					 * they must be the same.
					 */
					SCTP_PRINTF("Prev check - Gak, Evil plot, ssn:%d not the same as at:%d\n",
					    chk->rec.data.stream_number,
					    prev->rec.data.stream_number);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_7);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_7;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
				if ((prev->rec.data.rcv_flags & SCTP_DATA_UNORDERED) == 0 &&
				    chk->rec.data.stream_seq !=
				    prev->rec.data.stream_seq) {
					/*
					 * Huh, need the correct STR here,
					 * they must be the same.
					 */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Prev check - Gak, Evil plot, sseq:%d not the same as at:%d\n",
					    chk->rec.data.stream_seq,
					    prev->rec.data.stream_seq);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_8);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_8;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
			} else if ((prev->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
			    SCTP_DATA_LAST_FRAG) {
				/* Insert chk MUST be a FIRST */
				if ((chk->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) !=
				    SCTP_DATA_FIRST_FRAG) {
					SCTPDBG(SCTP_DEBUG_INDATA1, "Prev check - Gak, evil plot, its not FIRST and it must be!\n");
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_9);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);

					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_9;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
			}
		}
	}
	if (next) {
		post_tsn = chk->rec.data.TSN_seq + 1;
		if (post_tsn == next->rec.data.TSN_seq) {
			/*
			 * Ok the one I am inserting ahead of is my NEXT
			 * one. A bit of valdiation here.
			 */
			if (next->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {
				/* Insert chk MUST be a last fragment */
				if ((chk->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK)
				    != SCTP_DATA_LAST_FRAG) {
					SCTPDBG(SCTP_DEBUG_INDATA1, "Next chk - Next is FIRST, we must be LAST\n");
					SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, its not a last!\n");
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_10);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_10;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
			} else if ((next->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
				    SCTP_DATA_MIDDLE_FRAG ||
				    (next->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
			    SCTP_DATA_LAST_FRAG) {
				/*
				 * Insert chk CAN be MIDDLE or FIRST NOT
				 * LAST
				 */
				if ((chk->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
				    SCTP_DATA_LAST_FRAG) {
					SCTPDBG(SCTP_DEBUG_INDATA1, "Next chk - Next is a MIDDLE/LAST\n");
					SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, new prev chunk is a LAST\n");
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_11);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);

					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_11;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
				if (chk->rec.data.stream_number !=
				    next->rec.data.stream_number) {
					/*
					 * Huh, need the correct STR here,
					 * they must be the same.
					 */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Next chk - Gak, Evil plot, ssn:%d not the same as at:%d\n",
					    chk->rec.data.stream_number,
					    next->rec.data.stream_number);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_12);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);

					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_12;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
				if ((next->rec.data.rcv_flags & SCTP_DATA_UNORDERED) == 0 &&
				    chk->rec.data.stream_seq !=
				    next->rec.data.stream_seq) {
					/*
					 * Huh, need the correct STR here,
					 * they must be the same.
					 */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Next chk - Gak, Evil plot, sseq:%d not the same as at:%d\n",
					    chk->rec.data.stream_seq,
					    next->rec.data.stream_seq);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_13);
						ippp++;
						*ippp = chk->rec.data.TSN_seq;
						ippp++;
						*ippp = ((chk->rec.data.stream_number << 16) | chk->rec.data.stream_seq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_13;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return;
				}
			}
		}
	}
	/* Do we need to do some delivery? check */
	sctp_deliver_reasm_check(stcb, asoc);
}

/*
 * This is an unfortunate routine. It checks to make sure a evil guy is not
 * stuffing us full of bad packet fragments. A broken peer could also do this
 * but this is doubtful. It is to bad I must worry about evil crackers sigh
 * :< more cycles.
 */
static int
sctp_does_tsn_belong_to_reasm(struct sctp_association *asoc,
    uint32_t TSN_seq)
{
	struct sctp_tmit_chunk *at;
	uint32_t tsn_est;

	TAILQ_FOREACH(at, &asoc->reasmqueue, sctp_next) {
		if (compare_with_wrap(TSN_seq,
		    at->rec.data.TSN_seq, MAX_TSN)) {
			/* is it one bigger? */
			tsn_est = at->rec.data.TSN_seq + 1;
			if (tsn_est == TSN_seq) {
				/* yep. It better be a last then */
				if ((at->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) !=
				    SCTP_DATA_LAST_FRAG) {
					/*
					 * Ok this guy belongs next to a guy
					 * that is NOT last, it should be a
					 * middle/last, not a complete
					 * chunk.
					 */
					return (1);
				} else {
					/*
					 * This guy is ok since its a LAST
					 * and the new chunk is a fully
					 * self- contained one.
					 */
					return (0);
				}
			}
		} else if (TSN_seq == at->rec.data.TSN_seq) {
			/* Software error since I have a dup? */
			return (1);
		} else {
			/*
			 * Ok, 'at' is larger than new chunk but does it
			 * need to be right before it.
			 */
			tsn_est = TSN_seq + 1;
			if (tsn_est == at->rec.data.TSN_seq) {
				/* Yep, It better be a first */
				if ((at->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) !=
				    SCTP_DATA_FIRST_FRAG) {
					return (1);
				} else {
					return (0);
				}
			}
		}
	}
	return (0);
}


static int
sctp_process_a_data_chunk(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct mbuf **m, int offset, struct sctp_data_chunk *ch, int chk_length,
    struct sctp_nets *net, uint32_t * high_tsn, int *abort_flag,
    int *break_flag, int last_chunk)
{
	/* Process a data chunk */
	/* struct sctp_tmit_chunk *chk; */
	struct sctp_tmit_chunk *chk;
	uint32_t tsn, gap;
	struct mbuf *dmbuf;
	int indx, the_len;
	int need_reasm_check = 0;
	uint16_t strmno, strmseq;
	struct mbuf *oper;
	struct sctp_queued_to_read *control;
	int ordered;
	uint32_t protocol_id;
	uint8_t chunk_flags;
	struct sctp_stream_reset_list *liste;

	chk = NULL;
	tsn = ntohl(ch->dp.tsn);
	chunk_flags = ch->ch.chunk_flags;
	protocol_id = ch->dp.protocol_id;
	ordered = ((ch->ch.chunk_flags & SCTP_DATA_UNORDERED) == 0);
	if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
		sctp_log_map(tsn, asoc->cumulative_tsn, asoc->highest_tsn_inside_map, SCTP_MAP_TSN_ENTERS);
	}
	if (stcb == NULL) {
		return (0);
	}
	SCTP_LTRACE_CHK(stcb->sctp_ep, stcb, ch->ch.chunk_type, tsn);
	if (compare_with_wrap(asoc->cumulative_tsn, tsn, MAX_TSN) ||
	    asoc->cumulative_tsn == tsn) {
		/* It is a duplicate */
		SCTP_STAT_INCR(sctps_recvdupdata);
		if (asoc->numduptsns < SCTP_MAX_DUP_TSNS) {
			/* Record a dup for the next outbound sack */
			asoc->dup_tsns[asoc->numduptsns] = tsn;
			asoc->numduptsns++;
		}
		return (0);
	}
	/* Calculate the number of TSN's between the base and this TSN */
	if (tsn >= asoc->mapping_array_base_tsn) {
		gap = tsn - asoc->mapping_array_base_tsn;
	} else {
		gap = (MAX_TSN - asoc->mapping_array_base_tsn) + tsn + 1;
	}
	if (gap >= (SCTP_MAPPING_ARRAY << 3)) {
		/* Can't hold the bit in the mapping at max array, toss it */
		return (0);
	}
	if (gap >= (uint32_t) (asoc->mapping_array_size << 3)) {
		SCTP_TCB_LOCK_ASSERT(stcb);
		if (sctp_expand_mapping_array(asoc, gap)) {
			/* Can't expand, drop it */
			return (0);
		}
	}
	if (compare_with_wrap(tsn, *high_tsn, MAX_TSN)) {
		*high_tsn = tsn;
	}
	/* See if we have received this one already */
	if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap)) {
		SCTP_STAT_INCR(sctps_recvdupdata);
		if (asoc->numduptsns < SCTP_MAX_DUP_TSNS) {
			/* Record a dup for the next outbound sack */
			asoc->dup_tsns[asoc->numduptsns] = tsn;
			asoc->numduptsns++;
		}
		asoc->send_sack = 1;
		return (0);
	}
	/*
	 * Check to see about the GONE flag, duplicates would cause a sack
	 * to be sent up above
	 */
	if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) ||
	    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET))
	    ) {
		/*
		 * wait a minute, this guy is gone, there is no longer a
		 * receiver. Send peer an ABORT!
		 */
		struct mbuf *op_err;

		op_err = sctp_generate_invmanparam(SCTP_CAUSE_OUT_OF_RESC);
		sctp_abort_an_association(stcb->sctp_ep, stcb, 0, op_err, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return (0);
	}
	/*
	 * Now before going further we see if there is room. If NOT then we
	 * MAY let one through only IF this TSN is the one we are waiting
	 * for on a partial delivery API.
	 */

	/* now do the tests */
	if (((asoc->cnt_on_all_streams +
	    asoc->cnt_on_reasm_queue +
	    asoc->cnt_msg_on_sb) > sctp_max_chunks_on_queue) ||
	    (((int)asoc->my_rwnd) <= 0)) {
		/*
		 * When we have NO room in the rwnd we check to make sure
		 * the reader is doing its job...
		 */
		if (stcb->sctp_socket->so_rcv.sb_cc) {
			/* some to read, wake-up */
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
			struct socket *so;

			so = SCTP_INP_SO(stcb->sctp_ep);
			atomic_add_int(&stcb->asoc.refcnt, 1);
			SCTP_TCB_UNLOCK(stcb);
			SCTP_SOCKET_LOCK(so, 1);
			SCTP_TCB_LOCK(stcb);
			atomic_subtract_int(&stcb->asoc.refcnt, 1);
			if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {
				/* assoc was freed while we were unlocked */
				SCTP_SOCKET_UNLOCK(so, 1);
				return (0);
			}
#endif
			sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
			SCTP_SOCKET_UNLOCK(so, 1);
#endif
		}
		/* now is it in the mapping array of what we have accepted? */
		if (compare_with_wrap(tsn,
		    asoc->highest_tsn_inside_map, MAX_TSN)) {

			/* Nope not in the valid range dump it */
			SCTPDBG(SCTP_DEBUG_INDATA1, "My rwnd overrun1:tsn:%lx rwnd %lu sbspace:%ld\n",
			    (u_long)tsn, (u_long)asoc->my_rwnd,
			    sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv));
			sctp_set_rwnd(stcb, asoc);
			if ((asoc->cnt_on_all_streams +
			    asoc->cnt_on_reasm_queue +
			    asoc->cnt_msg_on_sb) > sctp_max_chunks_on_queue) {
				SCTP_STAT_INCR(sctps_datadropchklmt);
			} else {
				SCTP_STAT_INCR(sctps_datadroprwnd);
			}
			indx = *break_flag;
			*break_flag = 1;
			return (0);
		}
	}
	strmno = ntohs(ch->dp.stream_id);
	if (strmno >= asoc->streamincnt) {
		struct sctp_paramhdr *phdr;
		struct mbuf *mb;

		mb = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) * 2),
		    0, M_DONTWAIT, 1, MT_DATA);
		if (mb != NULL) {
			/* add some space up front so prepend will work well */
			SCTP_BUF_RESV_UF(mb, sizeof(struct sctp_chunkhdr));
			phdr = mtod(mb, struct sctp_paramhdr *);
			/*
			 * Error causes are just param's and this one has
			 * two back to back phdr, one with the error type
			 * and size, the other with the streamid and a rsvd
			 */
			SCTP_BUF_LEN(mb) = (sizeof(struct sctp_paramhdr) * 2);
			phdr->param_type = htons(SCTP_CAUSE_INVALID_STREAM);
			phdr->param_length =
			    htons(sizeof(struct sctp_paramhdr) * 2);
			phdr++;
			/* We insert the stream in the type field */
			phdr->param_type = ch->dp.stream_id;
			/* And set the length to 0 for the rsvd field */
			phdr->param_length = 0;
			sctp_queue_op_err(stcb, mb);
		}
		SCTP_STAT_INCR(sctps_badsid);
		SCTP_TCB_LOCK_ASSERT(stcb);
		SCTP_SET_TSN_PRESENT(asoc->mapping_array, gap);
		if (compare_with_wrap(tsn, asoc->highest_tsn_inside_map, MAX_TSN)) {
			/* we have a new high score */
			asoc->highest_tsn_inside_map = tsn;
			if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map(0, 2, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
			}
		}
		if (tsn == (asoc->cumulative_tsn + 1)) {
			/* Update cum-ack */
			asoc->cumulative_tsn = tsn;
		}
		return (0);
	}
	/*
	 * Before we continue lets validate that we are not being fooled by
	 * an evil attacker. We can only have 4k chunks based on our TSN
	 * spread allowed by the mapping array 512 * 8 bits, so there is no
	 * way our stream sequence numbers could have wrapped. We of course
	 * only validate the FIRST fragment so the bit must be set.
	 */
	strmseq = ntohs(ch->dp.stream_sequence);
#ifdef SCTP_ASOCLOG_OF_TSNS
	SCTP_TCB_LOCK_ASSERT(stcb);
	if (asoc->tsn_in_at >= SCTP_TSN_LOG_SIZE) {
		asoc->tsn_in_at = 0;
		asoc->tsn_in_wrapped = 1;
	}
	asoc->in_tsnlog[asoc->tsn_in_at].tsn = tsn;
	asoc->in_tsnlog[asoc->tsn_in_at].strm = strmno;
	asoc->in_tsnlog[asoc->tsn_in_at].seq = strmseq;
	asoc->in_tsnlog[asoc->tsn_in_at].sz = chk_length;
	asoc->in_tsnlog[asoc->tsn_in_at].flgs = chunk_flags;
	asoc->in_tsnlog[asoc->tsn_in_at].stcb = (void *)stcb;
	asoc->in_tsnlog[asoc->tsn_in_at].in_pos = asoc->tsn_in_at;
	asoc->in_tsnlog[asoc->tsn_in_at].in_out = 1;
	asoc->tsn_in_at++;
#endif
	if ((chunk_flags & SCTP_DATA_FIRST_FRAG) &&
	    (TAILQ_EMPTY(&asoc->resetHead)) &&
	    (chunk_flags & SCTP_DATA_UNORDERED) == 0 &&
	    (compare_with_wrap(asoc->strmin[strmno].last_sequence_delivered,
	    strmseq, MAX_SEQ) ||
	    asoc->strmin[strmno].last_sequence_delivered == strmseq)) {
		/* The incoming sseq is behind where we last delivered? */
		SCTPDBG(SCTP_DEBUG_INDATA1, "EVIL/Broken-Dup S-SEQ:%d delivered:%d from peer, Abort!\n",
		    strmseq, asoc->strmin[strmno].last_sequence_delivered);
		oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
		    0, M_DONTWAIT, 1, MT_DATA);
		if (oper) {
			struct sctp_paramhdr *ph;
			uint32_t *ippp;

			SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
			    (3 * sizeof(uint32_t));
			ph = mtod(oper, struct sctp_paramhdr *);
			ph->param_type = htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
			ph->param_length = htons(SCTP_BUF_LEN(oper));
			ippp = (uint32_t *) (ph + 1);
			*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_14);
			ippp++;
			*ippp = tsn;
			ippp++;
			*ippp = ((strmno << 16) | strmseq);

		}
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_14;
		sctp_abort_an_association(stcb->sctp_ep, stcb,
		    SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return (0);
	}
	/************************************
	 * From here down we may find ch-> invalid
	 * so its a good idea NOT to use it.
	 *************************************/

	the_len = (chk_length - sizeof(struct sctp_data_chunk));
	if (last_chunk == 0) {
		dmbuf = SCTP_M_COPYM(*m,
		    (offset + sizeof(struct sctp_data_chunk)),
		    the_len, M_DONTWAIT);
#ifdef SCTP_MBUF_LOGGING
		if (sctp_logging_level & SCTP_MBUF_LOGGING_ENABLE) {
			struct mbuf *mat;

			mat = dmbuf;
			while (mat) {
				if (SCTP_BUF_IS_EXTENDED(mat)) {
					sctp_log_mb(mat, SCTP_MBUF_ICOPY);
				}
				mat = SCTP_BUF_NEXT(mat);
			}
		}
#endif
	} else {
		/* We can steal the last chunk */
		int l_len;

		dmbuf = *m;
		/* lop off the top part */
		m_adj(dmbuf, (offset + sizeof(struct sctp_data_chunk)));
		if (SCTP_BUF_NEXT(dmbuf) == NULL) {
			l_len = SCTP_BUF_LEN(dmbuf);
		} else {
			/*
			 * need to count up the size hopefully does not hit
			 * this to often :-0
			 */
			struct mbuf *lat;

			l_len = 0;
			lat = dmbuf;
			while (lat) {
				l_len += SCTP_BUF_LEN(lat);
				lat = SCTP_BUF_NEXT(lat);
			}
		}
		if (l_len > the_len) {
			/* Trim the end round bytes off  too */
			m_adj(dmbuf, -(l_len - the_len));
		}
	}
	if (dmbuf == NULL) {
		SCTP_STAT_INCR(sctps_nomem);
		return (0);
	}
	if ((chunk_flags & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG &&
	    asoc->fragmented_delivery_inprogress == 0 &&
	    TAILQ_EMPTY(&asoc->resetHead) &&
	    ((ordered == 0) ||
	    ((asoc->strmin[strmno].last_sequence_delivered + 1) == strmseq &&
	    TAILQ_EMPTY(&asoc->strmin[strmno].inqueue)))) {
		/* Candidate for express delivery */
		/*
		 * Its not fragmented, No PD-API is up, Nothing in the
		 * delivery queue, Its un-ordered OR ordered and the next to
		 * deliver AND nothing else is stuck on the stream queue,
		 * And there is room for it in the socket buffer. Lets just
		 * stuff it up the buffer....
		 */

		/* It would be nice to avoid this copy if we could :< */
		sctp_alloc_a_readq(stcb, control);
		sctp_build_readq_entry_mac(control, stcb, asoc->context, net, tsn,
		    protocol_id,
		    stcb->asoc.context,
		    strmno, strmseq,
		    chunk_flags,
		    dmbuf);
		if (control == NULL) {
			goto failed_express_del;
		}
		sctp_add_to_readq(stcb->sctp_ep, stcb, control, &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
		if ((chunk_flags & SCTP_DATA_UNORDERED) == 0) {
			/* for ordered, bump what we delivered */
			asoc->strmin[strmno].last_sequence_delivered++;
		}
		SCTP_STAT_INCR(sctps_recvexpress);
		if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
			sctp_log_strm_del_alt(stcb, tsn, strmseq, strmno,
			    SCTP_STR_LOG_FROM_EXPRS_DEL);
		}
		control = NULL;
		goto finish_express_del;
	}
failed_express_del:
	/* If we reach here this is a new chunk */
	chk = NULL;
	control = NULL;
	/* Express for fragmented delivery? */
	if ((asoc->fragmented_delivery_inprogress) &&
	    (stcb->asoc.control_pdapi) &&
	    (asoc->str_of_pdapi == strmno) &&
	    (asoc->ssn_of_pdapi == strmseq)
	    ) {
		control = stcb->asoc.control_pdapi;
		if ((chunk_flags & SCTP_DATA_FIRST_FRAG) == SCTP_DATA_FIRST_FRAG) {
			/* Can't be another first? */
			goto failed_pdapi_express_del;
		}
		if (tsn == (control->sinfo_tsn + 1)) {
			/* Yep, we can add it on */
			int end = 0;
			uint32_t cumack;

			if (chunk_flags & SCTP_DATA_LAST_FRAG) {
				end = 1;
			}
			cumack = asoc->cumulative_tsn;
			if ((cumack + 1) == tsn)
				cumack = tsn;

			if (sctp_append_to_readq(stcb->sctp_ep, stcb, control, dmbuf, end,
			    tsn,
			    &stcb->sctp_socket->so_rcv)) {
				SCTP_PRINTF("Append fails end:%d\n", end);
				goto failed_pdapi_express_del;
			}
			SCTP_STAT_INCR(sctps_recvexpressm);
			control->sinfo_tsn = tsn;
			asoc->tsn_last_delivered = tsn;
			asoc->fragment_flags = chunk_flags;
			asoc->tsn_of_pdapi_last_delivered = tsn;
			asoc->last_flags_delivered = chunk_flags;
			asoc->last_strm_seq_delivered = strmseq;
			asoc->last_strm_no_delivered = strmno;
			if (end) {
				/* clean up the flags and such */
				asoc->fragmented_delivery_inprogress = 0;
				if ((chunk_flags & SCTP_DATA_UNORDERED) == 0) {
					asoc->strmin[strmno].last_sequence_delivered++;
				}
				stcb->asoc.control_pdapi = NULL;
				if (TAILQ_EMPTY(&asoc->reasmqueue) == 0) {
					/*
					 * There could be another message
					 * ready
					 */
					need_reasm_check = 1;
				}
			}
			control = NULL;
			goto finish_express_del;
		}
	}
failed_pdapi_express_del:
	control = NULL;
	if ((chunk_flags & SCTP_DATA_NOT_FRAG) != SCTP_DATA_NOT_FRAG) {
		sctp_alloc_a_chunk(stcb, chk);
		if (chk == NULL) {
			/* No memory so we drop the chunk */
			SCTP_STAT_INCR(sctps_nomem);
			if (last_chunk == 0) {
				/* we copied it, free the copy */
				sctp_m_freem(dmbuf);
			}
			return (0);
		}
		chk->rec.data.TSN_seq = tsn;
		chk->no_fr_allowed = 0;
		chk->rec.data.stream_seq = strmseq;
		chk->rec.data.stream_number = strmno;
		chk->rec.data.payloadtype = protocol_id;
		chk->rec.data.context = stcb->asoc.context;
		chk->rec.data.doing_fast_retransmit = 0;
		chk->rec.data.rcv_flags = chunk_flags;
		chk->asoc = asoc;
		chk->send_size = the_len;
		chk->whoTo = net;
		atomic_add_int(&net->ref_count, 1);
		chk->data = dmbuf;
	} else {
		sctp_alloc_a_readq(stcb, control);
		sctp_build_readq_entry_mac(control, stcb, asoc->context, net, tsn,
		    protocol_id,
		    stcb->asoc.context,
		    strmno, strmseq,
		    chunk_flags,
		    dmbuf);
		if (control == NULL) {
			/* No memory so we drop the chunk */
			SCTP_STAT_INCR(sctps_nomem);
			if (last_chunk == 0) {
				/* we copied it, free the copy */
				sctp_m_freem(dmbuf);
			}
			return (0);
		}
		control->length = the_len;
	}

	/* Mark it as received */
	/* Now queue it where it belongs */
	if (control != NULL) {
		/* First a sanity check */
		if (asoc->fragmented_delivery_inprogress) {
			/*
			 * Ok, we have a fragmented delivery in progress if
			 * this chunk is next to deliver OR belongs in our
			 * view to the reassembly, the peer is evil or
			 * broken.
			 */
			uint32_t estimate_tsn;

			estimate_tsn = asoc->tsn_last_delivered + 1;
			if (TAILQ_EMPTY(&asoc->reasmqueue) &&
			    (estimate_tsn == control->sinfo_tsn)) {
				/* Evil/Broke peer */
				sctp_m_freem(control->data);
				control->data = NULL;
				if (control->whoFrom) {
					sctp_free_remote_addr(control->whoFrom);
					control->whoFrom = NULL;
				}
				sctp_free_a_readq(stcb, control);
				oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);
				if (oper) {
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(oper) =
					    sizeof(struct sctp_paramhdr) +
					    (3 * sizeof(uint32_t));
					ph = mtod(oper, struct sctp_paramhdr *);
					ph->param_type =
					    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
					ph->param_length = htons(SCTP_BUF_LEN(oper));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_15);
					ippp++;
					*ippp = tsn;
					ippp++;
					*ippp = ((strmno << 16) | strmseq);
				}
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_15;
				sctp_abort_an_association(stcb->sctp_ep, stcb,
				    SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

				*abort_flag = 1;
				return (0);
			} else {
				if (sctp_does_tsn_belong_to_reasm(asoc, control->sinfo_tsn)) {
					sctp_m_freem(control->data);
					control->data = NULL;
					if (control->whoFrom) {
						sctp_free_remote_addr(control->whoFrom);
						control->whoFrom = NULL;
					}
					sctp_free_a_readq(stcb, control);

					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_16);
						ippp++;
						*ippp = tsn;
						ippp++;
						*ippp = ((strmno << 16) | strmseq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_16;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return (0);
				}
			}
		} else {
			/* No PDAPI running */
			if (!TAILQ_EMPTY(&asoc->reasmqueue)) {
				/*
				 * Reassembly queue is NOT empty validate
				 * that this tsn does not need to be in
				 * reasembly queue. If it does then our peer
				 * is broken or evil.
				 */
				if (sctp_does_tsn_belong_to_reasm(asoc, control->sinfo_tsn)) {
					sctp_m_freem(control->data);
					control->data = NULL;
					if (control->whoFrom) {
						sctp_free_remote_addr(control->whoFrom);
						control->whoFrom = NULL;
					}
					sctp_free_a_readq(stcb, control);
					oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (oper) {
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(oper) =
						    sizeof(struct sctp_paramhdr) +
						    (3 * sizeof(uint32_t));
						ph = mtod(oper,
						    struct sctp_paramhdr *);
						ph->param_type =
						    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
						ph->param_length =
						    htons(SCTP_BUF_LEN(oper));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_17);
						ippp++;
						*ippp = tsn;
						ippp++;
						*ippp = ((strmno << 16) | strmseq);
					}
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_17;
					sctp_abort_an_association(stcb->sctp_ep,
					    stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);

					*abort_flag = 1;
					return (0);
				}
			}
		}
		/* ok, if we reach here we have passed the sanity checks */
		if (chunk_flags & SCTP_DATA_UNORDERED) {
			/* queue directly into socket buffer */
			sctp_add_to_readq(stcb->sctp_ep, stcb,
			    control,
			    &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
		} else {
			/*
			 * Special check for when streams are resetting. We
			 * could be more smart about this and check the
			 * actual stream to see if it is not being reset..
			 * that way we would not create a HOLB when amongst
			 * streams being reset and those not being reset.
			 * 
			 * We take complete messages that have a stream reset
			 * intervening (aka the TSN is after where our
			 * cum-ack needs to be) off and put them on a
			 * pending_reply_queue. The reassembly ones we do
			 * not have to worry about since they are all sorted
			 * and proceessed by TSN order. It is only the
			 * singletons I must worry about.
			 */
			if (((liste = TAILQ_FIRST(&asoc->resetHead)) != NULL) &&
			    ((compare_with_wrap(tsn, liste->tsn, MAX_TSN)))
			    ) {
				/*
				 * yep its past where we need to reset... go
				 * ahead and queue it.
				 */
				if (TAILQ_EMPTY(&asoc->pending_reply_queue)) {
					/* first one on */
					TAILQ_INSERT_TAIL(&asoc->pending_reply_queue, control, next);
				} else {
					struct sctp_queued_to_read *ctlOn;
					unsigned char inserted = 0;

					ctlOn = TAILQ_FIRST(&asoc->pending_reply_queue);
					while (ctlOn) {
						if (compare_with_wrap(control->sinfo_tsn,
						    ctlOn->sinfo_tsn, MAX_TSN)) {
							ctlOn = TAILQ_NEXT(ctlOn, next);
						} else {
							/* found it */
							TAILQ_INSERT_BEFORE(ctlOn, control, next);
							inserted = 1;
							break;
						}
					}
					if (inserted == 0) {
						/*
						 * must be put at end, use
						 * prevP (all setup from
						 * loop) to setup nextP.
						 */
						TAILQ_INSERT_TAIL(&asoc->pending_reply_queue, control, next);
					}
				}
			} else {
				sctp_queue_data_to_stream(stcb, asoc, control, abort_flag);
				if (*abort_flag) {
					return (0);
				}
			}
		}
	} else {
		/* Into the re-assembly queue */
		sctp_queue_data_for_reasm(stcb, asoc, chk, abort_flag);
		if (*abort_flag) {
			/*
			 * the assoc is now gone and chk was put onto the
			 * reasm queue, which has all been freed.
			 */
			*m = NULL;
			return (0);
		}
	}
finish_express_del:
	if (compare_with_wrap(tsn, asoc->highest_tsn_inside_map, MAX_TSN)) {
		/* we have a new high score */
		asoc->highest_tsn_inside_map = tsn;
		if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 2, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
	}
	if (tsn == (asoc->cumulative_tsn + 1)) {
		/* Update cum-ack */
		asoc->cumulative_tsn = tsn;
	}
	if (last_chunk) {
		*m = NULL;
	}
	if (ordered) {
		SCTP_STAT_INCR_COUNTER64(sctps_inorderchunks);
	} else {
		SCTP_STAT_INCR_COUNTER64(sctps_inunorderchunks);
	}
	SCTP_STAT_INCR(sctps_recvdata);
	/* Set it present please */
	if (sctp_logging_level & SCTP_STR_LOGGING_ENABLE) {
		sctp_log_strm_del_alt(stcb, tsn, strmseq, strmno, SCTP_STR_LOG_FROM_MARK_TSN);
	}
	if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
		sctp_log_map(asoc->mapping_array_base_tsn, asoc->cumulative_tsn,
		    asoc->highest_tsn_inside_map, SCTP_MAP_PREPARE_SLIDE);
	}
	SCTP_TCB_LOCK_ASSERT(stcb);
	SCTP_SET_TSN_PRESENT(asoc->mapping_array, gap);
	/* check the special flag for stream resets */
	if (((liste = TAILQ_FIRST(&asoc->resetHead)) != NULL) &&
	    ((compare_with_wrap(asoc->cumulative_tsn, liste->tsn, MAX_TSN)) ||
	    (asoc->cumulative_tsn == liste->tsn))
	    ) {
		/*
		 * we have finished working through the backlogged TSN's now
		 * time to reset streams. 1: call reset function. 2: free
		 * pending_reply space 3: distribute any chunks in
		 * pending_reply_queue.
		 */
		struct sctp_queued_to_read *ctl;

		sctp_reset_in_stream(stcb, liste->number_entries, liste->req.list_of_streams);
		TAILQ_REMOVE(&asoc->resetHead, liste, next_resp);
		SCTP_FREE(liste, SCTP_M_STRESET);
		/* sa_ignore FREED_MEMORY */
		liste = TAILQ_FIRST(&asoc->resetHead);
		ctl = TAILQ_FIRST(&asoc->pending_reply_queue);
		if (ctl && (liste == NULL)) {
			/* All can be removed */
			while (ctl) {
				TAILQ_REMOVE(&asoc->pending_reply_queue, ctl, next);
				sctp_queue_data_to_stream(stcb, asoc, ctl, abort_flag);
				if (*abort_flag) {
					return (0);
				}
				ctl = TAILQ_FIRST(&asoc->pending_reply_queue);
			}
		} else if (ctl) {
			/* more than one in queue */
			while (!compare_with_wrap(ctl->sinfo_tsn, liste->tsn, MAX_TSN)) {
				/*
				 * if ctl->sinfo_tsn is <= liste->tsn we can
				 * process it which is the NOT of
				 * ctl->sinfo_tsn > liste->tsn
				 */
				TAILQ_REMOVE(&asoc->pending_reply_queue, ctl, next);
				sctp_queue_data_to_stream(stcb, asoc, ctl, abort_flag);
				if (*abort_flag) {
					return (0);
				}
				ctl = TAILQ_FIRST(&asoc->pending_reply_queue);
			}
		}
		/*
		 * Now service re-assembly to pick up anything that has been
		 * held on reassembly queue?
		 */
		sctp_deliver_reasm_check(stcb, asoc);
		need_reasm_check = 0;
	}
	if (need_reasm_check) {
		/* Another one waits ? */
		sctp_deliver_reasm_check(stcb, asoc);
	}
	return (1);
}

int8_t sctp_map_lookup_tab[256] = {
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 4,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 5,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 4,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 6,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 4,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 5,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 4,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 3,
	-1, 0, -1, 1, -1, 0, -1, 2,
	-1, 0, -1, 1, -1, 0, -1, 7,
};


void
sctp_sack_check(struct sctp_tcb *stcb, int ok_to_sack, int was_a_gap, int *abort_flag)
{
	/*
	 * Now we also need to check the mapping array in a couple of ways.
	 * 1) Did we move the cum-ack point?
	 */
	struct sctp_association *asoc;
	int i, at;
	int last_all_ones = 0;
	int slide_from, slide_end, lgap, distance;
	uint32_t old_cumack, old_base, old_highest;
	unsigned char aux_array[64];


	asoc = &stcb->asoc;
	at = 0;

	old_cumack = asoc->cumulative_tsn;
	old_base = asoc->mapping_array_base_tsn;
	old_highest = asoc->highest_tsn_inside_map;
	if (asoc->mapping_array_size < 64)
		memcpy(aux_array, asoc->mapping_array,
		    asoc->mapping_array_size);
	else
		memcpy(aux_array, asoc->mapping_array, 64);

	/*
	 * We could probably improve this a small bit by calculating the
	 * offset of the current cum-ack as the starting point.
	 */
	at = 0;
	for (i = 0; i < stcb->asoc.mapping_array_size; i++) {

		if (asoc->mapping_array[i] == 0xff) {
			at += 8;
			last_all_ones = 1;
		} else {
			/* there is a 0 bit */
			at += sctp_map_lookup_tab[asoc->mapping_array[i]];
			last_all_ones = 0;
			break;
		}
	}
	asoc->cumulative_tsn = asoc->mapping_array_base_tsn + (at - last_all_ones);
	/* at is one off, since in the table a embedded -1 is present */
	at++;

	if (compare_with_wrap(asoc->cumulative_tsn,
	    asoc->highest_tsn_inside_map,
	    MAX_TSN)) {
#ifdef INVARIANTS
		panic("huh, cumack 0x%x greater than high-tsn 0x%x in map",
		    asoc->cumulative_tsn, asoc->highest_tsn_inside_map);
#else
		SCTP_PRINTF("huh, cumack 0x%x greater than high-tsn 0x%x in map - should panic?\n",
		    asoc->cumulative_tsn, asoc->highest_tsn_inside_map);
		asoc->highest_tsn_inside_map = asoc->cumulative_tsn;
#endif
	}
	if ((asoc->cumulative_tsn == asoc->highest_tsn_inside_map) && (at >= 8)) {
		/* The complete array was completed by a single FR */
		/* higest becomes the cum-ack */
		int clr;

		asoc->cumulative_tsn = asoc->highest_tsn_inside_map;
		/* clear the array */
		clr = (at >> 3) + 1;
		if (clr > asoc->mapping_array_size) {
			clr = asoc->mapping_array_size;
		}
		memset(asoc->mapping_array, 0, clr);
		/* base becomes one ahead of the cum-ack */
		asoc->mapping_array_base_tsn = asoc->cumulative_tsn + 1;
		if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(old_base, old_cumack, old_highest,
			    SCTP_MAP_PREPARE_SLIDE);
			sctp_log_map(asoc->mapping_array_base_tsn, asoc->cumulative_tsn,
			    asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_CLEARED);
		}
	} else if (at >= 8) {
		/* we can slide the mapping array down */
		/* Calculate the new byte postion we can move down */
		slide_from = at >> 3;
		/*
		 * now calculate the ceiling of the move using our highest
		 * TSN value
		 */
		if (asoc->highest_tsn_inside_map >= asoc->mapping_array_base_tsn) {
			lgap = asoc->highest_tsn_inside_map -
			    asoc->mapping_array_base_tsn;
		} else {
			lgap = (MAX_TSN - asoc->mapping_array_base_tsn) +
			    asoc->highest_tsn_inside_map + 1;
		}
		slide_end = lgap >> 3;
		if (slide_end < slide_from) {
			panic("impossible slide");
		}
		distance = (slide_end - slide_from) + 1;
		if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(old_base, old_cumack, old_highest,
			    SCTP_MAP_PREPARE_SLIDE);
			sctp_log_map((uint32_t) slide_from, (uint32_t) slide_end,
			    (uint32_t) lgap, SCTP_MAP_SLIDE_FROM);
		}
		if (distance + slide_from > asoc->mapping_array_size ||
		    distance < 0) {
			/*
			 * Here we do NOT slide forward the array so that
			 * hopefully when more data comes in to fill it up
			 * we will be able to slide it forward. Really I
			 * don't think this should happen :-0
			 */

			if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map((uint32_t) distance, (uint32_t) slide_from,
				    (uint32_t) asoc->mapping_array_size,
				    SCTP_MAP_SLIDE_NONE);
			}
		} else {
			int ii;

			for (ii = 0; ii < distance; ii++) {
				asoc->mapping_array[ii] =
				    asoc->mapping_array[slide_from + ii];
			}
			for (ii = distance; ii <= slide_end; ii++) {
				asoc->mapping_array[ii] = 0;
			}
			asoc->mapping_array_base_tsn += (slide_from << 3);
			if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map(asoc->mapping_array_base_tsn,
				    asoc->cumulative_tsn, asoc->highest_tsn_inside_map,
				    SCTP_MAP_SLIDE_RESULT);
			}
		}
	}
	/*
	 * Now we need to see if we need to queue a sack or just start the
	 * timer (if allowed).
	 */
	if (ok_to_sack) {
		if (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_SENT) {
			/*
			 * Ok special case, in SHUTDOWN-SENT case. here we
			 * maker sure SACK timer is off and instead send a
			 * SHUTDOWN and a SACK
			 */
			if (SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_RECV,
				    stcb->sctp_ep, stcb, NULL, SCTP_FROM_SCTP_INDATA + SCTP_LOC_18);
			}
			sctp_send_shutdown(stcb, stcb->asoc.primary_destination);
			sctp_send_sack(stcb);
		} else {
			int is_a_gap;

			/* is there a gap now ? */
			is_a_gap = compare_with_wrap(stcb->asoc.highest_tsn_inside_map,
			    stcb->asoc.cumulative_tsn, MAX_TSN);

			/*
			 * CMT DAC algorithm: increase number of packets
			 * received since last ack
			 */
			stcb->asoc.cmt_dac_pkts_rcvd++;

			if ((stcb->asoc.send_sack == 1) ||	/* We need to send a
								 * SACK */
			    ((was_a_gap) && (is_a_gap == 0)) ||	/* was a gap, but no
								 * longer is one */
			    (stcb->asoc.numduptsns) ||	/* we have dup's */
			    (is_a_gap) ||	/* is still a gap */
			    (stcb->asoc.delayed_ack == 0) ||	/* Delayed sack disabled */
			    (stcb->asoc.data_pkts_seen >= stcb->asoc.sack_freq)	/* hit limit of pkts */
			    ) {

				if ((sctp_cmt_on_off) && (sctp_cmt_use_dac) &&
				    (stcb->asoc.send_sack == 0) &&
				    (stcb->asoc.numduptsns == 0) &&
				    (stcb->asoc.delayed_ack) &&
				    (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer))) {

					/*
					 * CMT DAC algorithm: With CMT,
					 * delay acks even in the face of
					 * 
					 * reordering. Therefore, if acks that
					 * do not have to be sent because of
					 * the above reasons, will be
					 * delayed. That is, acks that would
					 * have been sent due to gap reports
					 * will be delayed with DAC. Start
					 * the delayed ack timer.
					 */
					sctp_timer_start(SCTP_TIMER_TYPE_RECV,
					    stcb->sctp_ep, stcb, NULL);
				} else {
					/*
					 * Ok we must build a SACK since the
					 * timer is pending, we got our
					 * first packet OR there are gaps or
					 * duplicates.
					 */
					(void)SCTP_OS_TIMER_STOP(&stcb->asoc.dack_timer.timer);
					sctp_send_sack(stcb);
				}
			} else {
				if (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
					sctp_timer_start(SCTP_TIMER_TYPE_RECV,
					    stcb->sctp_ep, stcb, NULL);
				}
			}
		}
	}
}

void
sctp_service_queues(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	uint32_t tsize;
	uint16_t nxt_todel;

	if (asoc->fragmented_delivery_inprogress) {
		sctp_service_reassembly(stcb, asoc);
	}
	/* Can we proceed further, i.e. the PD-API is complete */
	if (asoc->fragmented_delivery_inprogress) {
		/* no */
		return;
	}
	/*
	 * Now is there some other chunk I can deliver from the reassembly
	 * queue.
	 */
doit_again:
	chk = TAILQ_FIRST(&asoc->reasmqueue);
	if (chk == NULL) {
		asoc->size_on_reasm_queue = 0;
		asoc->cnt_on_reasm_queue = 0;
		return;
	}
	nxt_todel = asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered + 1;
	if ((chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) &&
	    ((nxt_todel == chk->rec.data.stream_seq) ||
	    (chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED))) {
		/*
		 * Yep the first one is here. We setup to start reception,
		 * by backing down the TSN just in case we can't deliver.
		 */

		/*
		 * Before we start though either all of the message should
		 * be here or 1/4 the socket buffer max or nothing on the
		 * delivery queue and something can be delivered.
		 */
		if ((sctp_is_all_msg_on_reasm(asoc, &tsize) ||
		    (tsize >= stcb->sctp_ep->partial_delivery_point))) {
			asoc->fragmented_delivery_inprogress = 1;
			asoc->tsn_last_delivered = chk->rec.data.TSN_seq - 1;
			asoc->str_of_pdapi = chk->rec.data.stream_number;
			asoc->ssn_of_pdapi = chk->rec.data.stream_seq;
			asoc->pdapi_ppid = chk->rec.data.payloadtype;
			asoc->fragment_flags = chk->rec.data.rcv_flags;
			sctp_service_reassembly(stcb, asoc);
			if (asoc->fragmented_delivery_inprogress == 0) {
				goto doit_again;
			}
		}
	}
}

int
sctp_process_data(struct mbuf **mm, int iphlen, int *offset, int length,
    struct sctphdr *sh, struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net, uint32_t * high_tsn)
{
	struct sctp_data_chunk *ch, chunk_buf;
	struct sctp_association *asoc;
	int num_chunks = 0;	/* number of control chunks processed */
	int stop_proc = 0;
	int chk_length, break_flag, last_chunk;
	int abort_flag = 0, was_a_gap = 0;
	struct mbuf *m;

	/* set the rwnd */
	sctp_set_rwnd(stcb, &stcb->asoc);

	m = *mm;
	SCTP_TCB_LOCK_ASSERT(stcb);
	asoc = &stcb->asoc;
	if (compare_with_wrap(stcb->asoc.highest_tsn_inside_map,
	    stcb->asoc.cumulative_tsn, MAX_TSN)) {
		/* there was a gap before this data was processed */
		was_a_gap = 1;
	}
	/*
	 * setup where we got the last DATA packet from for any SACK that
	 * may need to go out. Don't bump the net. This is done ONLY when a
	 * chunk is assigned.
	 */
	asoc->last_data_chunk_from = net;

	/*-
	 * Now before we proceed we must figure out if this is a wasted
	 * cluster... i.e. it is a small packet sent in and yet the driver
	 * underneath allocated a full cluster for it. If so we must copy it
	 * to a smaller mbuf and free up the cluster mbuf. This will help
	 * with cluster starvation. Note for __Panda__ we don't do this
	 * since it has clusters all the way down to 64 bytes.
	 */
	if (SCTP_BUF_LEN(m) < (long)MLEN && SCTP_BUF_NEXT(m) == NULL) {
		/* we only handle mbufs that are singletons.. not chains */
		m = sctp_get_mbuf_for_msg(SCTP_BUF_LEN(m), 0, M_DONTWAIT, 1, MT_DATA);
		if (m) {
			/* ok lets see if we can copy the data up */
			caddr_t *from, *to;

			/* get the pointers and copy */
			to = mtod(m, caddr_t *);
			from = mtod((*mm), caddr_t *);
			memcpy(to, from, SCTP_BUF_LEN((*mm)));
			/* copy the length and free up the old */
			SCTP_BUF_LEN(m) = SCTP_BUF_LEN((*mm));
			sctp_m_freem(*mm);
			/* sucess, back copy */
			*mm = m;
		} else {
			/* We are in trouble in the mbuf world .. yikes */
			m = *mm;
		}
	}
	/* get pointer to the first chunk header */
	ch = (struct sctp_data_chunk *)sctp_m_getptr(m, *offset,
	    sizeof(struct sctp_data_chunk), (uint8_t *) & chunk_buf);
	if (ch == NULL) {
		return (1);
	}
	/*
	 * process all DATA chunks...
	 */
	*high_tsn = asoc->cumulative_tsn;
	break_flag = 0;
	asoc->data_pkts_seen++;
	while (stop_proc == 0) {
		/* validate chunk length */
		chk_length = ntohs(ch->ch.chunk_length);
		if (length - *offset < chk_length) {
			/* all done, mutulated chunk */
			stop_proc = 1;
			break;
		}
		if (ch->ch.chunk_type == SCTP_DATA) {
			if ((size_t)chk_length < sizeof(struct sctp_data_chunk) + 1) {
				/*
				 * Need to send an abort since we had a
				 * invalid data chunk.
				 */
				struct mbuf *op_err;

				op_err = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 2 * sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);

				if (op_err) {
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(op_err) = sizeof(struct sctp_paramhdr) +
					    (2 * sizeof(uint32_t));
					ph = mtod(op_err, struct sctp_paramhdr *);
					ph->param_type =
					    htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
					ph->param_length = htons(SCTP_BUF_LEN(op_err));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_19);
					ippp++;
					*ippp = asoc->cumulative_tsn;

				}
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_19;
				sctp_abort_association(inp, stcb, m, iphlen, sh,
				    op_err, 0);
				return (2);
			}
#ifdef SCTP_AUDITING_ENABLED
			sctp_audit_log(0xB1, 0);
#endif
			if (SCTP_SIZE32(chk_length) == (length - *offset)) {
				last_chunk = 1;
			} else {
				last_chunk = 0;
			}
			if (sctp_process_a_data_chunk(stcb, asoc, mm, *offset, ch,
			    chk_length, net, high_tsn, &abort_flag, &break_flag,
			    last_chunk)) {
				num_chunks++;
			}
			if (abort_flag)
				return (2);

			if (break_flag) {
				/*
				 * Set because of out of rwnd space and no
				 * drop rep space left.
				 */
				stop_proc = 1;
				break;
			}
		} else {
			/* not a data chunk in the data region */
			switch (ch->ch.chunk_type) {
			case SCTP_INITIATION:
			case SCTP_INITIATION_ACK:
			case SCTP_SELECTIVE_ACK:
			case SCTP_HEARTBEAT_REQUEST:
			case SCTP_HEARTBEAT_ACK:
			case SCTP_ABORT_ASSOCIATION:
			case SCTP_SHUTDOWN:
			case SCTP_SHUTDOWN_ACK:
			case SCTP_OPERATION_ERROR:
			case SCTP_COOKIE_ECHO:
			case SCTP_COOKIE_ACK:
			case SCTP_ECN_ECHO:
			case SCTP_ECN_CWR:
			case SCTP_SHUTDOWN_COMPLETE:
			case SCTP_AUTHENTICATION:
			case SCTP_ASCONF_ACK:
			case SCTP_PACKET_DROPPED:
			case SCTP_STREAM_RESET:
			case SCTP_FORWARD_CUM_TSN:
			case SCTP_ASCONF:
				/*
				 * Now, what do we do with KNOWN chunks that
				 * are NOT in the right place?
				 * 
				 * For now, I do nothing but ignore them. We
				 * may later want to add sysctl stuff to
				 * switch out and do either an ABORT() or
				 * possibly process them.
				 */
				if (sctp_strict_data_order) {
					struct mbuf *op_err;

					op_err = sctp_generate_invmanparam(SCTP_CAUSE_PROTOCOL_VIOLATION);
					sctp_abort_association(inp, stcb, m, iphlen, sh, op_err, 0);
					return (2);
				}
				break;
			default:
				/* unknown chunk type, use bit rules */
				if (ch->ch.chunk_type & 0x40) {
					/* Add a error report to the queue */
					struct mbuf *merr;
					struct sctp_paramhdr *phd;

					merr = sctp_get_mbuf_for_msg(sizeof(*phd), 0, M_DONTWAIT, 1, MT_DATA);
					if (merr) {
						phd = mtod(merr, struct sctp_paramhdr *);
						/*
						 * We cheat and use param
						 * type since we did not
						 * bother to define a error
						 * cause struct. They are
						 * the same basic format
						 * with different names.
						 */
						phd->param_type =
						    htons(SCTP_CAUSE_UNRECOG_CHUNK);
						phd->param_length =
						    htons(chk_length + sizeof(*phd));
						SCTP_BUF_LEN(merr) = sizeof(*phd);
						SCTP_BUF_NEXT(merr) = SCTP_M_COPYM(m, *offset,
						    SCTP_SIZE32(chk_length),
						    M_DONTWAIT);
						if (SCTP_BUF_NEXT(merr)) {
							sctp_queue_op_err(stcb, merr);
						} else {
							sctp_m_freem(merr);
						}
					}
				}
				if ((ch->ch.chunk_type & 0x80) == 0) {
					/* discard the rest of this packet */
					stop_proc = 1;
				}	/* else skip this bad chunk and
					 * continue... */
				break;
			};	/* switch of chunk type */
		}
		*offset += SCTP_SIZE32(chk_length);
		if ((*offset >= length) || stop_proc) {
			/* no more data left in the mbuf chain */
			stop_proc = 1;
			continue;
		}
		ch = (struct sctp_data_chunk *)sctp_m_getptr(m, *offset,
		    sizeof(struct sctp_data_chunk), (uint8_t *) & chunk_buf);
		if (ch == NULL) {
			*offset = length;
			stop_proc = 1;
			break;

		}
	}			/* while */
	if (break_flag) {
		/*
		 * we need to report rwnd overrun drops.
		 */
		sctp_send_packet_dropped(stcb, net, *mm, iphlen, 0);
	}
	if (num_chunks) {
		/*
		 * Did we get data, if so update the time for auto-close and
		 * give peer credit for being alive.
		 */
		SCTP_STAT_INCR(sctps_recvpktwithdata);
		if (sctp_logging_level & SCTP_THRESHOLD_LOGGING) {
			sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
			    stcb->asoc.overall_error_count,
			    0,
			    SCTP_FROM_SCTP_INDATA,
			    __LINE__);
		}
		stcb->asoc.overall_error_count = 0;
		(void)SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_last_rcvd);
	}
	/* now service all of the reassm queue if needed */
	if (!(TAILQ_EMPTY(&asoc->reasmqueue)))
		sctp_service_queues(stcb, asoc);

	if (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_SENT) {
		/* Assure that we ack right away */
		stcb->asoc.send_sack = 1;
	}
	/* Start a sack timer or QUEUE a SACK for sending */
	if ((stcb->asoc.cumulative_tsn == stcb->asoc.highest_tsn_inside_map) &&
	    (stcb->asoc.mapping_array[0] != 0xff)) {
		if ((stcb->asoc.data_pkts_seen >= stcb->asoc.sack_freq) ||
		    (stcb->asoc.delayed_ack == 0) ||
		    (stcb->asoc.send_sack == 1)) {
			if (SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
				(void)SCTP_OS_TIMER_STOP(&stcb->asoc.dack_timer.timer);
			}
			sctp_send_sack(stcb);
		} else {
			if (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
				sctp_timer_start(SCTP_TIMER_TYPE_RECV,
				    stcb->sctp_ep, stcb, NULL);
			}
		}
	} else {
		sctp_sack_check(stcb, 1, was_a_gap, &abort_flag);
	}
	if (abort_flag)
		return (2);

	return (0);
}

static void
sctp_handle_segments(struct mbuf *m, int *offset, struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_sack_chunk *ch, uint32_t last_tsn, uint32_t * biggest_tsn_acked,
    uint32_t * biggest_newly_acked_tsn, uint32_t * this_sack_lowest_newack,
    int num_seg, int *ecn_seg_sums)
{
	/************************************************/
	/* process fragments and update sendqueue        */
	/************************************************/
	struct sctp_sack *sack;
	struct sctp_gap_ack_block *frag, block;
	struct sctp_tmit_chunk *tp1;
	int i;
	unsigned int j;
	int num_frs = 0;

	uint16_t frag_strt, frag_end, primary_flag_set;
	u_long last_frag_high;

	/*
	 * @@@ JRI : TODO: This flag is not used anywhere .. remove?
	 */
	if (asoc->primary_destination->dest_state & SCTP_ADDR_SWITCH_PRIMARY) {
		primary_flag_set = 1;
	} else {
		primary_flag_set = 0;
	}
	sack = &ch->sack;

	frag = (struct sctp_gap_ack_block *)sctp_m_getptr(m, *offset,
	    sizeof(struct sctp_gap_ack_block), (uint8_t *) & block);
	*offset += sizeof(block);
	if (frag == NULL) {
		return;
	}
	tp1 = NULL;
	last_frag_high = 0;
	for (i = 0; i < num_seg; i++) {
		frag_strt = ntohs(frag->start);
		frag_end = ntohs(frag->end);
		/* some sanity checks on the fargment offsets */
		if (frag_strt > frag_end) {
			/* this one is malformed, skip */
			frag++;
			continue;
		}
		if (compare_with_wrap((frag_end + last_tsn), *biggest_tsn_acked,
		    MAX_TSN))
			*biggest_tsn_acked = frag_end + last_tsn;

		/* mark acked dgs and find out the highestTSN being acked */
		if (tp1 == NULL) {
			tp1 = TAILQ_FIRST(&asoc->sent_queue);

			/* save the locations of the last frags */
			last_frag_high = frag_end + last_tsn;
		} else {
			/*
			 * now lets see if we need to reset the queue due to
			 * a out-of-order SACK fragment
			 */
			if (compare_with_wrap(frag_strt + last_tsn,
			    last_frag_high, MAX_TSN)) {
				/*
				 * if the new frag starts after the last TSN
				 * frag covered, we are ok and this one is
				 * beyond the last one
				 */
				;
			} else {
				/*
				 * ok, they have reset us, so we need to
				 * reset the queue this will cause extra
				 * hunting but hey, they chose the
				 * performance hit when they failed to order
				 * there gaps..
				 */
				tp1 = TAILQ_FIRST(&asoc->sent_queue);
			}
			last_frag_high = frag_end + last_tsn;
		}
		for (j = frag_strt + last_tsn; j <= frag_end + last_tsn; j++) {
			while (tp1) {
				if (tp1->rec.data.doing_fast_retransmit)
					num_frs++;

				/*
				 * CMT: CUCv2 algorithm. For each TSN being
				 * processed from the sent queue, track the
				 * next expected pseudo-cumack, or
				 * rtx_pseudo_cumack, if required. Separate
				 * cumack trackers for first transmissions,
				 * and retransmissions.
				 */
				if ((tp1->whoTo->find_pseudo_cumack == 1) && (tp1->sent < SCTP_DATAGRAM_RESEND) &&
				    (tp1->snd_count == 1)) {
					tp1->whoTo->pseudo_cumack = tp1->rec.data.TSN_seq;
					tp1->whoTo->find_pseudo_cumack = 0;
				}
				if ((tp1->whoTo->find_rtx_pseudo_cumack == 1) && (tp1->sent < SCTP_DATAGRAM_RESEND) &&
				    (tp1->snd_count > 1)) {
					tp1->whoTo->rtx_pseudo_cumack = tp1->rec.data.TSN_seq;
					tp1->whoTo->find_rtx_pseudo_cumack = 0;
				}
				if (tp1->rec.data.TSN_seq == j) {
					if (tp1->sent != SCTP_DATAGRAM_UNSENT) {
						/*
						 * must be held until
						 * cum-ack passes
						 */
						/*
						 * ECN Nonce: Add the nonce
						 * value to the sender's
						 * nonce sum
						 */
						if (tp1->sent < SCTP_DATAGRAM_RESEND) {
							/*-
							 * If it is less than RESEND, it is
							 * now no-longer in flight.
							 * Higher values may already be set
							 * via previous Gap Ack Blocks...
							 * i.e. ACKED or RESEND.
							 */
							if (compare_with_wrap(tp1->rec.data.TSN_seq,
							    *biggest_newly_acked_tsn, MAX_TSN)) {
								*biggest_newly_acked_tsn = tp1->rec.data.TSN_seq;
							}
							/*
							 * CMT: SFR algo
							 * (and HTNA) - set
							 * saw_newack to 1
							 * for dest being
							 * newly acked.
							 * update
							 * this_sack_highest_
							 * newack if
							 * appropriate.
							 */
							if (tp1->rec.data.chunk_was_revoked == 0)
								tp1->whoTo->saw_newack = 1;

							if (compare_with_wrap(tp1->rec.data.TSN_seq,
							    tp1->whoTo->this_sack_highest_newack,
							    MAX_TSN)) {
								tp1->whoTo->this_sack_highest_newack =
								    tp1->rec.data.TSN_seq;
							}
							/*
							 * CMT DAC algo:
							 * also update
							 * this_sack_lowest_n
							 * ewack
							 */
							if (*this_sack_lowest_newack == 0) {
								if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
									sctp_log_sack(*this_sack_lowest_newack,
									    last_tsn,
									    tp1->rec.data.TSN_seq,
									    0,
									    0,
									    SCTP_LOG_TSN_ACKED);
								}
								*this_sack_lowest_newack = tp1->rec.data.TSN_seq;
							}
							/*
							 * CMT: CUCv2
							 * algorithm. If
							 * (rtx-)pseudo-cumac
							 * k for corresp
							 * dest is being
							 * acked, then we
							 * have a new
							 * (rtx-)pseudo-cumac
							 * k. Set
							 * new_(rtx_)pseudo_c
							 * umack to TRUE so
							 * that the cwnd for
							 * this dest can be
							 * updated. Also
							 * trigger search
							 * for the next
							 * expected
							 * (rtx-)pseudo-cumac
							 * k. Separate
							 * pseudo_cumack
							 * trackers for
							 * first
							 * transmissions and
							 * retransmissions.
							 */
							if (tp1->rec.data.TSN_seq == tp1->whoTo->pseudo_cumack) {
								if (tp1->rec.data.chunk_was_revoked == 0) {
									tp1->whoTo->new_pseudo_cumack = 1;
								}
								tp1->whoTo->find_pseudo_cumack = 1;
							}
							if (sctp_logging_level & SCTP_CWND_LOGGING_ENABLE) {
								sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.TSN_seq, SCTP_CWND_LOG_FROM_SACK);
							}
							if (tp1->rec.data.TSN_seq == tp1->whoTo->rtx_pseudo_cumack) {
								if (tp1->rec.data.chunk_was_revoked == 0) {
									tp1->whoTo->new_pseudo_cumack = 1;
								}
								tp1->whoTo->find_rtx_pseudo_cumack = 1;
							}
							if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
								sctp_log_sack(*biggest_newly_acked_tsn,
								    last_tsn,
								    tp1->rec.data.TSN_seq,
								    frag_strt,
								    frag_end,
								    SCTP_LOG_TSN_ACKED);
							}
							if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
								sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_GAP,
								    tp1->whoTo->flight_size,
								    tp1->book_size,
								    (uintptr_t) tp1->whoTo,
								    tp1->rec.data.TSN_seq);
							}
							sctp_flight_size_decrease(tp1);
							sctp_total_flight_decrease(stcb, tp1);

							tp1->whoTo->net_ack += tp1->send_size;
							if (tp1->snd_count < 2) {
								/*
								 * True
								 * non-retran
								 * smited
								 * chunk */
								tp1->whoTo->net_ack2 += tp1->send_size;

								/*
								 * update RTO
								 * too ? */
								if (tp1->do_rtt) {
									tp1->whoTo->RTO =
									    sctp_calculate_rto(stcb,
									    asoc,
									    tp1->whoTo,
									    &tp1->sent_rcv_time,
									    sctp_align_safe_nocopy);
									tp1->do_rtt = 0;
								}
							}
						}
						if (tp1->sent <= SCTP_DATAGRAM_RESEND) {
							(*ecn_seg_sums) += tp1->rec.data.ect_nonce;
							(*ecn_seg_sums) &= SCTP_SACK_NONCE_SUM;
							if (compare_with_wrap(tp1->rec.data.TSN_seq,
							    asoc->this_sack_highest_gap,
							    MAX_TSN)) {
								asoc->this_sack_highest_gap =
								    tp1->rec.data.TSN_seq;
							}
							if (tp1->sent == SCTP_DATAGRAM_RESEND) {
								sctp_ucount_decr(asoc->sent_queue_retran_cnt);
#ifdef SCTP_AUDITING_ENABLED
								sctp_audit_log(0xB2,
								    (asoc->sent_queue_retran_cnt & 0x000000ff));
#endif
							}
						}
						/*
						 * All chunks NOT UNSENT
						 * fall through here and are
						 * marked
						 */
						tp1->sent = SCTP_DATAGRAM_MARKED;
						if (tp1->rec.data.chunk_was_revoked) {
							/* deflate the cwnd */
							tp1->whoTo->cwnd -= tp1->book_size;
							tp1->rec.data.chunk_was_revoked = 0;
						}
					}
					break;
				}	/* if (tp1->TSN_seq == j) */
				if (compare_with_wrap(tp1->rec.data.TSN_seq, j,
				    MAX_TSN))
					break;

				tp1 = TAILQ_NEXT(tp1, sctp_next);
			}	/* end while (tp1) */
		}		/* end for (j = fragStart */
		frag = (struct sctp_gap_ack_block *)sctp_m_getptr(m, *offset,
		    sizeof(struct sctp_gap_ack_block), (uint8_t *) & block);
		*offset += sizeof(block);
		if (frag == NULL) {
			break;
		}
	}
	if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
		if (num_frs)
			sctp_log_fr(*biggest_tsn_acked,
			    *biggest_newly_acked_tsn,
			    last_tsn, SCTP_FR_LOG_BIGGEST_TSNS);
	}
}

static void
sctp_check_for_revoked(struct sctp_tcb *stcb,
    struct sctp_association *asoc, uint32_t cumack,
    u_long biggest_tsn_acked)
{
	struct sctp_tmit_chunk *tp1;
	int tot_revoked = 0;

	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	while (tp1) {
		if (compare_with_wrap(tp1->rec.data.TSN_seq, cumack,
		    MAX_TSN)) {
			/*
			 * ok this guy is either ACK or MARKED. If it is
			 * ACKED it has been previously acked but not this
			 * time i.e. revoked.  If it is MARKED it was ACK'ed
			 * again.
			 */
			if (compare_with_wrap(tp1->rec.data.TSN_seq, biggest_tsn_acked,
			    MAX_TSN))
				break;


			if (tp1->sent == SCTP_DATAGRAM_ACKED) {
				/* it has been revoked */
				tp1->sent = SCTP_DATAGRAM_SENT;
				tp1->rec.data.chunk_was_revoked = 1;
				/*
				 * We must add this stuff back in to assure
				 * timers and such get started.
				 */
				if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
					sctp_misc_ints(SCTP_FLIGHT_LOG_UP_REVOKE,
					    tp1->whoTo->flight_size,
					    tp1->book_size,
					    (uintptr_t) tp1->whoTo,
					    tp1->rec.data.TSN_seq);
				}
				sctp_flight_size_increase(tp1);
				sctp_total_flight_increase(stcb, tp1);
				/*
				 * We inflate the cwnd to compensate for our
				 * artificial inflation of the flight_size.
				 */
				tp1->whoTo->cwnd += tp1->book_size;
				tot_revoked++;
				if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
					sctp_log_sack(asoc->last_acked_seq,
					    cumack,
					    tp1->rec.data.TSN_seq,
					    0,
					    0,
					    SCTP_LOG_TSN_REVOKED);
				}
			} else if (tp1->sent == SCTP_DATAGRAM_MARKED) {
				/* it has been re-acked in this SACK */
				tp1->sent = SCTP_DATAGRAM_ACKED;
			}
		}
		if (tp1->sent == SCTP_DATAGRAM_UNSENT)
			break;
		tp1 = TAILQ_NEXT(tp1, sctp_next);
	}
	if (tot_revoked > 0) {
		/*
		 * Setup the ecn nonce re-sync point. We do this since once
		 * data is revoked we begin to retransmit things, which do
		 * NOT have the ECN bits set. This means we are now out of
		 * sync and must wait until we get back in sync with the
		 * peer to check ECN bits.
		 */
		tp1 = TAILQ_FIRST(&asoc->send_queue);
		if (tp1 == NULL) {
			asoc->nonce_resync_tsn = asoc->sending_seq;
		} else {
			asoc->nonce_resync_tsn = tp1->rec.data.TSN_seq;
		}
		asoc->nonce_wait_for_ecne = 0;
		asoc->nonce_sum_check = 0;
	}
}

static void
sctp_strike_gap_ack_chunks(struct sctp_tcb *stcb, struct sctp_association *asoc,
    u_long biggest_tsn_acked, u_long biggest_tsn_newly_acked, u_long this_sack_lowest_newack, int accum_moved)
{
	struct sctp_tmit_chunk *tp1;
	int strike_flag = 0;
	struct timeval now;
	int tot_retrans = 0;
	uint32_t sending_seq;
	struct sctp_nets *net;
	int num_dests_sacked = 0;

	/*
	 * select the sending_seq, this is either the next thing ready to be
	 * sent but not transmitted, OR, the next seq we assign.
	 */
	tp1 = TAILQ_FIRST(&stcb->asoc.send_queue);
	if (tp1 == NULL) {
		sending_seq = asoc->sending_seq;
	} else {
		sending_seq = tp1->rec.data.TSN_seq;
	}

	/* CMT DAC algo: finding out if SACK is a mixed SACK */
	if (sctp_cmt_on_off && sctp_cmt_use_dac) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->saw_newack)
				num_dests_sacked++;
		}
	}
	if (stcb->asoc.peer_supports_prsctp) {
		(void)SCTP_GETTIME_TIMEVAL(&now);
	}
	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	while (tp1) {
		strike_flag = 0;
		if (tp1->no_fr_allowed) {
			/* this one had a timeout or something */
			tp1 = TAILQ_NEXT(tp1, sctp_next);
			continue;
		}
		if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
			if (tp1->sent < SCTP_DATAGRAM_RESEND)
				sctp_log_fr(biggest_tsn_newly_acked,
				    tp1->rec.data.TSN_seq,
				    tp1->sent,
				    SCTP_FR_LOG_CHECK_STRIKE);
		}
		if (compare_with_wrap(tp1->rec.data.TSN_seq, biggest_tsn_acked,
		    MAX_TSN) ||
		    tp1->sent == SCTP_DATAGRAM_UNSENT) {
			/* done */
			break;
		}
		if (stcb->asoc.peer_supports_prsctp) {
			if ((PR_SCTP_TTL_ENABLED(tp1->flags)) && tp1->sent < SCTP_DATAGRAM_ACKED) {
				/* Is it expired? */
				if (
				    (timevalcmp(&now, &tp1->rec.data.timetodrop, >))
				    ) {
					/* Yes so drop it */
					if (tp1->data != NULL) {
						(void)sctp_release_pr_sctp_chunk(stcb, tp1,
						    (SCTP_RESPONSE_TO_USER_REQ | SCTP_NOTIFY_DATAGRAM_SENT),
						    &asoc->sent_queue, SCTP_SO_NOT_LOCKED);
					}
					tp1 = TAILQ_NEXT(tp1, sctp_next);
					continue;
				}
			}
			if ((PR_SCTP_RTX_ENABLED(tp1->flags)) && tp1->sent < SCTP_DATAGRAM_ACKED) {
				/* Has it been retransmitted tv_sec times? */
				if (tp1->snd_count > tp1->rec.data.timetodrop.tv_sec) {
					/* Yes, so drop it */
					if (tp1->data != NULL) {
						(void)sctp_release_pr_sctp_chunk(stcb, tp1,
						    (SCTP_RESPONSE_TO_USER_REQ | SCTP_NOTIFY_DATAGRAM_SENT),
						    &asoc->sent_queue, SCTP_SO_NOT_LOCKED);
					}
					tp1 = TAILQ_NEXT(tp1, sctp_next);
					continue;
				}
			}
		}
		if (compare_with_wrap(tp1->rec.data.TSN_seq,
		    asoc->this_sack_highest_gap, MAX_TSN)) {
			/* we are beyond the tsn in the sack  */
			break;
		}
		if (tp1->sent >= SCTP_DATAGRAM_RESEND) {
			/* either a RESEND, ACKED, or MARKED */
			/* skip */
			tp1 = TAILQ_NEXT(tp1, sctp_next);
			continue;
		}
		/*
		 * CMT : SFR algo (covers part of DAC and HTNA as well)
		 */
		if (tp1->whoTo && tp1->whoTo->saw_newack == 0) {
			/*
			 * No new acks were receieved for data sent to this
			 * dest. Therefore, according to the SFR algo for
			 * CMT, no data sent to this dest can be marked for
			 * FR using this SACK.
			 */
			tp1 = TAILQ_NEXT(tp1, sctp_next);
			continue;
		} else if (tp1->whoTo && compare_with_wrap(tp1->rec.data.TSN_seq,
		    tp1->whoTo->this_sack_highest_newack, MAX_TSN)) {
			/*
			 * CMT: New acks were receieved for data sent to
			 * this dest. But no new acks were seen for data
			 * sent after tp1. Therefore, according to the SFR
			 * algo for CMT, tp1 cannot be marked for FR using
			 * this SACK. This step covers part of the DAC algo
			 * and the HTNA algo as well.
			 */
			tp1 = TAILQ_NEXT(tp1, sctp_next);
			continue;
		}
		/*
		 * Here we check to see if we were have already done a FR
		 * and if so we see if the biggest TSN we saw in the sack is
		 * smaller than the recovery point. If so we don't strike
		 * the tsn... otherwise we CAN strike the TSN.
		 */
		/*
		 * @@@ JRI: Check for CMT if (accum_moved &&
		 * asoc->fast_retran_loss_recovery && (sctp_cmt_on_off ==
		 * 0)) {
		 */
		if (accum_moved && asoc->fast_retran_loss_recovery) {
			/*
			 * Strike the TSN if in fast-recovery and cum-ack
			 * moved.
			 */
			if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(biggest_tsn_newly_acked,
				    tp1->rec.data.TSN_seq,
				    tp1->sent,
				    SCTP_FR_LOG_STRIKE_CHUNK);
			}
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				tp1->sent++;
			}
			if (sctp_cmt_on_off && sctp_cmt_use_dac) {
				/*
				 * CMT DAC algorithm: If SACK flag is set to
				 * 0, then lowest_newack test will not pass
				 * because it would have been set to the
				 * cumack earlier. If not already to be
				 * rtx'd, If not a mixed sack and if tp1 is
				 * not between two sacked TSNs, then mark by
				 * one more. NOTE that we are marking by one
				 * additional time since the SACK DAC flag
				 * indicates that two packets have been
				 * received after this missing TSN.
				 */
				if ((tp1->sent < SCTP_DATAGRAM_RESEND) && (num_dests_sacked == 1) &&
				    compare_with_wrap(this_sack_lowest_newack, tp1->rec.data.TSN_seq, MAX_TSN)) {
					if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(16 + num_dests_sacked,
						    tp1->rec.data.TSN_seq,
						    tp1->sent,
						    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					tp1->sent++;
				}
			}
		} else if (tp1->rec.data.doing_fast_retransmit) {
			/*
			 * For those that have done a FR we must take
			 * special consideration if we strike. I.e the
			 * biggest_newly_acked must be higher than the
			 * sending_seq at the time we did the FR.
			 */
			if (
#ifdef SCTP_FR_TO_ALTERNATE
			/*
			 * If FR's go to new networks, then we must only do
			 * this for singly homed asoc's. However if the FR's
			 * go to the same network (Armando's work) then its
			 * ok to FR multiple times.
			 */
			    (asoc->numnets < 2)
#else
			    (1)
#endif
			    ) {

				if ((compare_with_wrap(biggest_tsn_newly_acked,
				    tp1->rec.data.fast_retran_tsn, MAX_TSN)) ||
				    (biggest_tsn_newly_acked ==
				    tp1->rec.data.fast_retran_tsn)) {
					/*
					 * Strike the TSN, since this ack is
					 * beyond where things were when we
					 * did a FR.
					 */
					if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(biggest_tsn_newly_acked,
						    tp1->rec.data.TSN_seq,
						    tp1->sent,
						    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						tp1->sent++;
					}
					strike_flag = 1;
					if (sctp_cmt_on_off && sctp_cmt_use_dac) {
						/*
						 * CMT DAC algorithm: If
						 * SACK flag is set to 0,
						 * then lowest_newack test
						 * will not pass because it
						 * would have been set to
						 * the cumack earlier. If
						 * not already to be rtx'd,
						 * If not a mixed sack and
						 * if tp1 is not between two
						 * sacked TSNs, then mark by
						 * one more. NOTE that we
						 * are marking by one
						 * additional time since the
						 * SACK DAC flag indicates
						 * that two packets have
						 * been received after this
						 * missing TSN.
						 */
						if ((tp1->sent < SCTP_DATAGRAM_RESEND) &&
						    (num_dests_sacked == 1) &&
						    compare_with_wrap(this_sack_lowest_newack,
						    tp1->rec.data.TSN_seq, MAX_TSN)) {
							if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
								sctp_log_fr(32 + num_dests_sacked,
								    tp1->rec.data.TSN_seq,
								    tp1->sent,
								    SCTP_FR_LOG_STRIKE_CHUNK);
							}
							if (tp1->sent < SCTP_DATAGRAM_RESEND) {
								tp1->sent++;
							}
						}
					}
				}
			}
			/*
			 * JRI: TODO: remove code for HTNA algo. CMT's SFR
			 * algo covers HTNA.
			 */
		} else if (compare_with_wrap(tp1->rec.data.TSN_seq,
		    biggest_tsn_newly_acked, MAX_TSN)) {
			/*
			 * We don't strike these: This is the  HTNA
			 * algorithm i.e. we don't strike If our TSN is
			 * larger than the Highest TSN Newly Acked.
			 */
			;
		} else {
			/* Strike the TSN */
			if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(biggest_tsn_newly_acked,
				    tp1->rec.data.TSN_seq,
				    tp1->sent,
				    SCTP_FR_LOG_STRIKE_CHUNK);
			}
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				tp1->sent++;
			}
			if (sctp_cmt_on_off && sctp_cmt_use_dac) {
				/*
				 * CMT DAC algorithm: If SACK flag is set to
				 * 0, then lowest_newack test will not pass
				 * because it would have been set to the
				 * cumack earlier. If not already to be
				 * rtx'd, If not a mixed sack and if tp1 is
				 * not between two sacked TSNs, then mark by
				 * one more. NOTE that we are marking by one
				 * additional time since the SACK DAC flag
				 * indicates that two packets have been
				 * received after this missing TSN.
				 */
				if ((tp1->sent < SCTP_DATAGRAM_RESEND) && (num_dests_sacked == 1) &&
				    compare_with_wrap(this_sack_lowest_newack, tp1->rec.data.TSN_seq, MAX_TSN)) {
					if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(48 + num_dests_sacked,
						    tp1->rec.data.TSN_seq,
						    tp1->sent,
						    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					tp1->sent++;
				}
			}
		}
		if (tp1->sent == SCTP_DATAGRAM_RESEND) {
			/* Increment the count to resend */
			struct sctp_nets *alt;

			/* printf("OK, we are now ready to FR this guy\n"); */
			if (sctp_logging_level & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(tp1->rec.data.TSN_seq, tp1->snd_count,
				    0, SCTP_FR_MARKED);
			}
			if (strike_flag) {
				/* This is a subsequent FR */
				SCTP_STAT_INCR(sctps_sendmultfastretrans);
			}
			sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			if (sctp_cmt_on_off) {
				/*
				 * CMT: Using RTX_SSTHRESH policy for CMT.
				 * If CMT is being used, then pick dest with
				 * largest ssthresh for any retransmission.
				 */
				tp1->no_fr_allowed = 1;
				alt = tp1->whoTo;
				/* sa_ignore NO_NULL_CHK */
				if (sctp_cmt_on_off && sctp_cmt_pf) {
					/*
					 * JRS 5/18/07 - If CMT PF is on,
					 * use the PF version of
					 * find_alt_net()
					 */
					alt = sctp_find_alternate_net(stcb, alt, 2);
				} else {
					/*
					 * JRS 5/18/07 - If only CMT is on,
					 * use the CMT version of
					 * find_alt_net()
					 */
					/* sa_ignore NO_NULL_CHK */
					alt = sctp_find_alternate_net(stcb, alt, 1);
				}
				if (alt == NULL) {
					alt = tp1->whoTo;
				}
				/*
				 * CUCv2: If a different dest is picked for
				 * the retransmission, then new
				 * (rtx-)pseudo_cumack needs to be tracked
				 * for orig dest. Let CUCv2 track new (rtx-)
				 * pseudo-cumack always.
				 */
				if (tp1->whoTo) {
					tp1->whoTo->find_pseudo_cumack = 1;
					tp1->whoTo->find_rtx_pseudo_cumack = 1;
				}
			} else {/* CMT is OFF */

#ifdef SCTP_FR_TO_ALTERNATE
				/* Can we find an alternate? */
				alt = sctp_find_alternate_net(stcb, tp1->whoTo, 0);
#else
				/*
				 * default behavior is to NOT retransmit
				 * FR's to an alternate. Armando Caro's
				 * paper details why.
				 */
				alt = tp1->whoTo;
#endif
			}

			tp1->rec.data.doing_fast_retransmit = 1;
			tot_retrans++;
			/* mark the sending seq for possible subsequent FR's */
			/*
			 * printf("Marking TSN for FR new value %x\n",
			 * (uint32_t)tpi->rec.data.TSN_seq);
			 */
			if (TAILQ_EMPTY(&asoc->send_queue)) {
				/*
				 * If the queue of send is empty then its
				 * the next sequence number that will be
				 * assigned so we subtract one from this to
				 * get the one we last sent.
				 */
				tp1->rec.data.fast_retran_tsn = sending_seq;
			} else {
				/*
				 * If there are chunks on the send queue
				 * (unsent data that has made it from the
				 * stream queues but not out the door, we
				 * take the first one (which will have the
				 * lowest TSN) and subtract one to get the
				 * one we last sent.
				 */
				struct sctp_tmit_chunk *ttt;

				ttt = TAILQ_FIRST(&asoc->send_queue);
				tp1->rec.data.fast_retran_tsn =
				    ttt->rec.data.TSN_seq;
			}

			if (tp1->do_rtt) {
				/*
				 * this guy had a RTO calculation pending on
				 * it, cancel it
				 */
				tp1->do_rtt = 0;
			}
			/* fix counts and things */
			if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
				sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_RSND,
				    (tp1->whoTo ? (tp1->whoTo->flight_size) : 0),
				    tp1->book_size,
				    (uintptr_t) tp1->whoTo,
				    tp1->rec.data.TSN_seq);
			}
			if (tp1->whoTo) {
				tp1->whoTo->net_ack++;
				sctp_flight_size_decrease(tp1);
			}
			if (sctp_logging_level & SCTP_LOG_RWND_ENABLE) {
				sctp_log_rwnd(SCTP_INCREASE_PEER_RWND,
				    asoc->peers_rwnd, tp1->send_size, sctp_peer_chunk_oh);
			}
			/* add back to the rwnd */
			asoc->peers_rwnd += (tp1->send_size + sctp_peer_chunk_oh);

			/* remove from the total flight */
			sctp_total_flight_decrease(stcb, tp1);
			if (alt != tp1->whoTo) {
				/* yes, there is an alternate. */
				sctp_free_remote_addr(tp1->whoTo);
				/* sa_ignore FREED_MEMORY */
				tp1->whoTo = alt;
				atomic_add_int(&alt->ref_count, 1);
			}
		}
		tp1 = TAILQ_NEXT(tp1, sctp_next);
	}			/* while (tp1) */

	if (tot_retrans > 0) {
		/*
		 * Setup the ecn nonce re-sync point. We do this since once
		 * we go to FR something we introduce a Karn's rule scenario
		 * and won't know the totals for the ECN bits.
		 */
		asoc->nonce_resync_tsn = sending_seq;
		asoc->nonce_wait_for_ecne = 0;
		asoc->nonce_sum_check = 0;
	}
}

struct sctp_tmit_chunk *
sctp_try_advance_peer_ack_point(struct sctp_tcb *stcb,
    struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *tp1, *tp2, *a_adv = NULL;
	struct timeval now;
	int now_filled = 0;

	if (asoc->peer_supports_prsctp == 0) {
		return (NULL);
	}
	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	while (tp1) {
		if (tp1->sent != SCTP_FORWARD_TSN_SKIP &&
		    tp1->sent != SCTP_DATAGRAM_RESEND) {
			/* no chance to advance, out of here */
			break;
		}
		if (!PR_SCTP_ENABLED(tp1->flags)) {
			/*
			 * We can't fwd-tsn past any that are reliable aka
			 * retransmitted until the asoc fails.
			 */
			break;
		}
		if (!now_filled) {
			(void)SCTP_GETTIME_TIMEVAL(&now);
			now_filled = 1;
		}
		tp2 = TAILQ_NEXT(tp1, sctp_next);
		/*
		 * now we got a chunk which is marked for another
		 * retransmission to a PR-stream but has run out its chances
		 * already maybe OR has been marked to skip now. Can we skip
		 * it if its a resend?
		 */
		if (tp1->sent == SCTP_DATAGRAM_RESEND &&
		    (PR_SCTP_TTL_ENABLED(tp1->flags))) {
			/*
			 * Now is this one marked for resend and its time is
			 * now up?
			 */
			if (timevalcmp(&now, &tp1->rec.data.timetodrop, >)) {
				/* Yes so drop it */
				if (tp1->data) {
					(void)sctp_release_pr_sctp_chunk(stcb, tp1,
					    (SCTP_RESPONSE_TO_USER_REQ | SCTP_NOTIFY_DATAGRAM_SENT),
					    &asoc->sent_queue, SCTP_SO_NOT_LOCKED);
				}
			} else {
				/*
				 * No, we are done when hit one for resend
				 * whos time as not expired.
				 */
				break;
			}
		}
		/*
		 * Ok now if this chunk is marked to drop it we can clean up
		 * the chunk, advance our peer ack point and we can check
		 * the next chunk.
		 */
		if (tp1->sent == SCTP_FORWARD_TSN_SKIP) {
			/* advance PeerAckPoint goes forward */
			asoc->advanced_peer_ack_point = tp1->rec.data.TSN_seq;
			a_adv = tp1;
			/*
			 * we don't want to de-queue it here. Just wait for
			 * the next peer SACK to come with a new cumTSN and
			 * then the chunk will be droped in the normal
			 * fashion.
			 */
			if (tp1->data) {
				sctp_free_bufspace(stcb, asoc, tp1, 1);
				/*
				 * Maybe there should be another
				 * notification type
				 */
				sctp_ulp_notify(SCTP_NOTIFY_DG_FAIL, stcb,
				    (SCTP_RESPONSE_TO_USER_REQ | SCTP_NOTIFY_DATAGRAM_SENT),
				    tp1, SCTP_SO_NOT_LOCKED);
				sctp_m_freem(tp1->data);
				tp1->data = NULL;
				if (stcb->sctp_socket) {
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
					struct socket *so;

					so = SCTP_INP_SO(stcb->sctp_ep);
					atomic_add_int(&stcb->asoc.refcnt, 1);
					SCTP_TCB_UNLOCK(stcb);
					SCTP_SOCKET_LOCK(so, 1);
					SCTP_TCB_LOCK(stcb);
					atomic_subtract_int(&stcb->asoc.refcnt, 1);
					if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {
						/*
						 * assoc was freed while we
						 * were unlocked
						 */
						SCTP_SOCKET_UNLOCK(so, 1);
						return (NULL);
					}
#endif
					sctp_sowwakeup(stcb->sctp_ep, stcb->sctp_socket);
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
					SCTP_SOCKET_UNLOCK(so, 1);
#endif
					if (sctp_logging_level & SCTP_WAKE_LOGGING_ENABLE) {
						sctp_wakeup_log(stcb, tp1->rec.data.TSN_seq, 1, SCTP_WAKESND_FROM_FWDTSN);
					}
				}
			}
		} else {
			/*
			 * If it is still in RESEND we can advance no
			 * further
			 */
			break;
		}
		/*
		 * If we hit here we just dumped tp1, move to next tsn on
		 * sent queue.
		 */
		tp1 = tp2;
	}
	return (a_adv);
}

static void
sctp_fs_audit(struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	int inflight = 0, resend = 0, inbetween = 0, acked = 0, above = 0;

	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		if (chk->sent < SCTP_DATAGRAM_RESEND) {
			inflight++;
		} else if (chk->sent == SCTP_DATAGRAM_RESEND) {
			resend++;
		} else if (chk->sent < SCTP_DATAGRAM_ACKED) {
			inbetween++;
		} else if (chk->sent > SCTP_DATAGRAM_ACKED) {
			above++;
		} else {
			acked++;
		}
	}

	if ((inflight > 0) || (inbetween > 0)) {
#ifdef INVARIANTS
		panic("Flight size-express incorrect? \n");
#else
		SCTP_PRINTF("Flight size-express incorrect inflight:%d inbetween:%d\n",
		    inflight, inbetween);
#endif
	}
}


static void
sctp_window_probe_recovery(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    struct sctp_nets *net,
    struct sctp_tmit_chunk *tp1)
{
	struct sctp_tmit_chunk *chk;

	/* First setup this one and get it moved back */
	tp1->sent = SCTP_DATAGRAM_UNSENT;
	if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_WP,
		    tp1->whoTo->flight_size,
		    tp1->book_size,
		    (uintptr_t) tp1->whoTo,
		    tp1->rec.data.TSN_seq);
	}
	sctp_flight_size_decrease(tp1);
	sctp_total_flight_decrease(stcb, tp1);
	TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
	TAILQ_INSERT_HEAD(&asoc->send_queue, tp1, sctp_next);
	asoc->sent_queue_cnt--;
	asoc->send_queue_cnt++;
	/*
	 * Now all guys marked for RESEND on the sent_queue must be moved
	 * back too.
	 */
	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			/* Another chunk to move */
			chk->sent = SCTP_DATAGRAM_UNSENT;
			/* It should not be in flight */
			TAILQ_REMOVE(&asoc->sent_queue, chk, sctp_next);
			TAILQ_INSERT_AFTER(&asoc->send_queue, tp1, chk, sctp_next);
			asoc->sent_queue_cnt--;
			asoc->send_queue_cnt++;
			sctp_ucount_decr(asoc->sent_queue_retran_cnt);
		}
	}
}

void
sctp_express_handle_sack(struct sctp_tcb *stcb, uint32_t cumack,
    uint32_t rwnd, int nonce_sum_flag, int *abort_now)
{
	struct sctp_nets *net;
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *tp1, *tp2;
	uint32_t old_rwnd;
	int win_probe_recovery = 0;
	int win_probe_recovered = 0;
	int j, done_once = 0;

	if (sctp_logging_level & SCTP_LOG_SACK_ARRIVALS_ENABLE) {
		sctp_misc_ints(SCTP_SACK_LOG_EXPRESS, cumack,
		    rwnd, stcb->asoc.last_acked_seq, stcb->asoc.peers_rwnd);
	}
	SCTP_TCB_LOCK_ASSERT(stcb);
#ifdef SCTP_ASOCLOG_OF_TSNS
	stcb->asoc.cumack_log[stcb->asoc.cumack_log_at] = cumack;
	stcb->asoc.cumack_log_at++;
	if (stcb->asoc.cumack_log_at > SCTP_TSN_LOG_SIZE) {
		stcb->asoc.cumack_log_at = 0;
	}
#endif
	asoc = &stcb->asoc;
	old_rwnd = asoc->peers_rwnd;
	if (compare_with_wrap(asoc->last_acked_seq, cumack, MAX_TSN)) {
		/* old ack */
		return;
	} else if (asoc->last_acked_seq == cumack) {
		/* Window update sack */
		asoc->peers_rwnd = sctp_sbspace_sub(rwnd,
		    (uint32_t) (asoc->total_flight + (asoc->sent_queue_cnt * sctp_peer_chunk_oh)));
		if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {
			/* SWS sender side engages */
			asoc->peers_rwnd = 0;
		}
		if (asoc->peers_rwnd > old_rwnd) {
			goto again;
		}
		return;
	}
	/* First setup for CC stuff */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		net->prev_cwnd = net->cwnd;
		net->net_ack = 0;
		net->net_ack2 = 0;

		/*
		 * CMT: Reset CUC and Fast recovery algo variables before
		 * SACK processing
		 */
		net->new_pseudo_cumack = 0;
		net->will_exit_fast_recovery = 0;
	}
	if (sctp_strict_sacks) {
		uint32_t send_s;

		if (!TAILQ_EMPTY(&asoc->sent_queue)) {
			tp1 = TAILQ_LAST(&asoc->sent_queue,
			    sctpchunk_listhead);
			send_s = tp1->rec.data.TSN_seq + 1;
		} else {
			send_s = asoc->sending_seq;
		}
		if ((cumack == send_s) ||
		    compare_with_wrap(cumack, send_s, MAX_TSN)) {
#ifndef INVARIANTS
			struct mbuf *oper;

#endif
#ifdef INVARIANTS
			panic("Impossible sack 1");
#else
			*abort_now = 1;
			/* XXX */
			oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
			    0, M_DONTWAIT, 1, MT_DATA);
			if (oper) {
				struct sctp_paramhdr *ph;
				uint32_t *ippp;

				SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
				    sizeof(uint32_t);
				ph = mtod(oper, struct sctp_paramhdr *);
				ph->param_type = htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
				ph->param_length = htons(SCTP_BUF_LEN(oper));
				ippp = (uint32_t *) (ph + 1);
				*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_25);
			}
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_25;
			sctp_abort_an_association(stcb->sctp_ep, stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
			return;
#endif
		}
	}
	asoc->this_sack_highest_gap = cumack;
	if (sctp_logging_level & SCTP_THRESHOLD_LOGGING) {
		sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
		    stcb->asoc.overall_error_count,
		    0,
		    SCTP_FROM_SCTP_INDATA,
		    __LINE__);
	}
	stcb->asoc.overall_error_count = 0;
	if (compare_with_wrap(cumack, asoc->last_acked_seq, MAX_TSN)) {
		/* process the new consecutive TSN first */
		tp1 = TAILQ_FIRST(&asoc->sent_queue);
		while (tp1) {
			tp2 = TAILQ_NEXT(tp1, sctp_next);
			if (compare_with_wrap(cumack, tp1->rec.data.TSN_seq,
			    MAX_TSN) ||
			    cumack == tp1->rec.data.TSN_seq) {
				if (tp1->sent == SCTP_DATAGRAM_UNSENT) {
					printf("Warning, an unsent is now acked?\n");
				}
				/*
				 * ECN Nonce: Add the nonce to the sender's
				 * nonce sum
				 */
				asoc->nonce_sum_expect_base += tp1->rec.data.ect_nonce;
				if (tp1->sent < SCTP_DATAGRAM_ACKED) {
					/*
					 * If it is less than ACKED, it is
					 * now no-longer in flight. Higher
					 * values may occur during marking
					 */
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_CA,
							    tp1->whoTo->flight_size,
							    tp1->book_size,
							    (uintptr_t) tp1->whoTo,
							    tp1->rec.data.TSN_seq);
						}
						sctp_flight_size_decrease(tp1);
						/* sa_ignore NO_NULL_CHK */
						sctp_total_flight_decrease(stcb, tp1);
					}
					tp1->whoTo->net_ack += tp1->send_size;
					if (tp1->snd_count < 2) {
						/*
						 * True non-retransmited
						 * chunk
						 */
						tp1->whoTo->net_ack2 +=
						    tp1->send_size;

						/* update RTO too? */
						if (tp1->do_rtt) {
							tp1->whoTo->RTO =
							/*
							 * sa_ignore
							 * NO_NULL_CHK
							 */
							    sctp_calculate_rto(stcb,
							    asoc, tp1->whoTo,
							    &tp1->sent_rcv_time,
							    sctp_align_safe_nocopy);
							tp1->do_rtt = 0;
						}
					}
					/*
					 * CMT: CUCv2 algorithm. From the
					 * cumack'd TSNs, for each TSN being
					 * acked for the first time, set the
					 * following variables for the
					 * corresp destination.
					 * new_pseudo_cumack will trigger a
					 * cwnd update.
					 * find_(rtx_)pseudo_cumack will
					 * trigger search for the next
					 * expected (rtx-)pseudo-cumack.
					 */
					tp1->whoTo->new_pseudo_cumack = 1;
					tp1->whoTo->find_pseudo_cumack = 1;
					tp1->whoTo->find_rtx_pseudo_cumack = 1;

					if (sctp_logging_level & SCTP_CWND_LOGGING_ENABLE) {
						/* sa_ignore NO_NULL_CHK */
						sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.TSN_seq, SCTP_CWND_LOG_FROM_SACK);
					}
				}
				if (tp1->sent == SCTP_DATAGRAM_RESEND) {
					sctp_ucount_decr(asoc->sent_queue_retran_cnt);
				}
				if (tp1->rec.data.chunk_was_revoked) {
					/* deflate the cwnd */
					tp1->whoTo->cwnd -= tp1->book_size;
					tp1->rec.data.chunk_was_revoked = 0;
				}
				tp1->sent = SCTP_DATAGRAM_ACKED;
				TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
				if (tp1->data) {
					/* sa_ignore NO_NULL_CHK */
					sctp_free_bufspace(stcb, asoc, tp1, 1);
					sctp_m_freem(tp1->data);
				}
				if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
					sctp_log_sack(asoc->last_acked_seq,
					    cumack,
					    tp1->rec.data.TSN_seq,
					    0,
					    0,
					    SCTP_LOG_FREE_SENT);
				}
				tp1->data = NULL;
				asoc->sent_queue_cnt--;
				sctp_free_a_chunk(stcb, tp1);
				tp1 = tp2;
			} else {
				break;
			}
		}

	}
	/* sa_ignore NO_NULL_CHK */
	if (stcb->sctp_socket) {
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		struct socket *so;

#endif

		SOCKBUF_LOCK(&stcb->sctp_socket->so_snd);
		if (sctp_logging_level & SCTP_WAKE_LOGGING_ENABLE) {
			/* sa_ignore NO_NULL_CHK */
			sctp_wakeup_log(stcb, cumack, 1, SCTP_WAKESND_FROM_SACK);
		}
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		so = SCTP_INP_SO(stcb->sctp_ep);
		atomic_add_int(&stcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_SOCKET_LOCK(so, 1);
		SCTP_TCB_LOCK(stcb);
		atomic_subtract_int(&stcb->asoc.refcnt, 1);
		if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {
			/* assoc was freed while we were unlocked */
			SCTP_SOCKET_UNLOCK(so, 1);
			return;
		}
#endif
		sctp_sowwakeup_locked(stcb->sctp_ep, stcb->sctp_socket);
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	} else {
		if (sctp_logging_level & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, cumack, 1, SCTP_NOWAKE_FROM_SACK);
		}
	}

	/* JRS - Use the congestion control given in the CC module */
	if (asoc->last_acked_seq != cumack)
		asoc->cc_functions.sctp_cwnd_update_after_sack(stcb, asoc, 1, 0, 0);

	asoc->last_acked_seq = cumack;

	if (TAILQ_EMPTY(&asoc->sent_queue)) {
		/* nothing left in-flight */
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			net->flight_size = 0;
			net->partial_bytes_acked = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
	}
	/* Fix up the a-p-a-p for future PR-SCTP sends */
	if (compare_with_wrap(cumack, asoc->advanced_peer_ack_point, MAX_TSN)) {
		asoc->advanced_peer_ack_point = cumack;
	}
	/* ECN Nonce updates */
	if (asoc->ecn_nonce_allowed) {
		if (asoc->nonce_sum_check) {
			if (nonce_sum_flag != ((asoc->nonce_sum_expect_base) & SCTP_SACK_NONCE_SUM)) {
				if (asoc->nonce_wait_for_ecne == 0) {
					struct sctp_tmit_chunk *lchk;

					lchk = TAILQ_FIRST(&asoc->send_queue);
					asoc->nonce_wait_for_ecne = 1;
					if (lchk) {
						asoc->nonce_wait_tsn = lchk->rec.data.TSN_seq;
					} else {
						asoc->nonce_wait_tsn = asoc->sending_seq;
					}
				} else {
					if (compare_with_wrap(asoc->last_acked_seq, asoc->nonce_wait_tsn, MAX_TSN) ||
					    (asoc->last_acked_seq == asoc->nonce_wait_tsn)) {
						/*
						 * Misbehaving peer. We need
						 * to react to this guy
						 */
						asoc->ecn_allowed = 0;
						asoc->ecn_nonce_allowed = 0;
					}
				}
			}
		} else {
			/* See if Resynchronization Possible */
			if (compare_with_wrap(asoc->last_acked_seq, asoc->nonce_resync_tsn, MAX_TSN)) {
				asoc->nonce_sum_check = 1;
				/*
				 * now we must calculate what the base is.
				 * We do this based on two things, we know
				 * the total's for all the segments
				 * gap-acked in the SACK (none), We also
				 * know the SACK's nonce sum, its in
				 * nonce_sum_flag. So we can build a truth
				 * table to back-calculate the new value of
				 * asoc->nonce_sum_expect_base:
				 * 
				 * SACK-flag-Value         Seg-Sums Base 0 0 0
				 * 1                    0 1 0 1 1 1
				 * 1 0
				 */
				asoc->nonce_sum_expect_base = (0 ^ nonce_sum_flag) & SCTP_SACK_NONCE_SUM;
			}
		}
	}
	/* RWND update */
	asoc->peers_rwnd = sctp_sbspace_sub(rwnd,
	    (uint32_t) (asoc->total_flight + (asoc->sent_queue_cnt * sctp_peer_chunk_oh)));
	if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {
		/* SWS sender side engages */
		asoc->peers_rwnd = 0;
	}
	if (asoc->peers_rwnd > old_rwnd) {
		win_probe_recovery = 1;
	}
	/* Now assure a timer where data is queued at */
again:
	j = 0;
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (win_probe_recovery && (net->window_probe)) {
			net->window_probe = 0;
			win_probe_recovered = 1;
			/*
			 * Find first chunk that was used with window probe
			 * and clear the sent
			 */
			/* sa_ignore FREED_MEMORY */
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->window_probe) {
					/* move back to data send queue */
					sctp_window_probe_recovery(stcb, asoc, net, tp1);
					break;
				}
			}
		}
		if (net->flight_size) {
			int to_ticks;

			if (net->RTO == 0) {
				to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
			} else {
				to_ticks = MSEC_TO_TICKS(net->RTO);
			}
			j++;
			(void)SCTP_OS_TIMER_START(&net->rxt_timer.timer, to_ticks,
			    sctp_timeout_handler, &net->rxt_timer);
		} else {
			if (SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net,
				    SCTP_FROM_SCTP_INDATA + SCTP_LOC_22);
			}
			if (sctp_early_fr) {
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck4);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_23);
				}
			}
		}
	}
	if ((j == 0) &&
	    (!TAILQ_EMPTY(&asoc->sent_queue)) &&
	    (asoc->sent_queue_retran_cnt == 0) &&
	    (win_probe_recovered == 0) &&
	    (done_once == 0)) {
		/* huh, this should not happen */
		sctp_fs_audit(asoc);
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			net->flight_size = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
		asoc->sent_queue_retran_cnt = 0;
		TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				sctp_flight_size_increase(tp1);
				sctp_total_flight_increase(stcb, tp1);
			} else if (tp1->sent == SCTP_DATAGRAM_RESEND) {
				asoc->sent_queue_retran_cnt++;
			}
		}
		done_once = 1;
		goto again;
	}
	/**********************************/
	/* Now what about shutdown issues */
	/**********************************/
	if (TAILQ_EMPTY(&asoc->send_queue) && TAILQ_EMPTY(&asoc->sent_queue)) {
		/* nothing left on sendqueue.. consider done */
		/* clean up */
		if ((asoc->stream_queue_cnt == 1) &&
		    ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) ||
		    (asoc->state & SCTP_STATE_SHUTDOWN_RECEIVED)) &&
		    (asoc->locked_on_sending)
		    ) {
			struct sctp_stream_queue_pending *sp;

			/*
			 * I may be in a state where we got all across.. but
			 * cannot write more due to a shutdown... we abort
			 * since the user did not indicate EOR in this case.
			 * The sp will be cleaned during free of the asoc.
			 */
			sp = TAILQ_LAST(&((asoc->locked_on_sending)->outqueue),
			    sctp_streamhead);
			if ((sp) && (sp->length == 0)) {
				/* Let cleanup code purge it */
				if (sp->msg_is_complete) {
					asoc->stream_queue_cnt--;
				} else {
					asoc->state |= SCTP_STATE_PARTIAL_MSG_LEFT;
					asoc->locked_on_sending = NULL;
					asoc->stream_queue_cnt--;
				}
			}
		}
		if ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) &&
		    (asoc->stream_queue_cnt == 0)) {
			if (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT) {
				/* Need to abort here */
				struct mbuf *oper;

		abort_out_now:
				*abort_now = 1;
				/* XXX */
				oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);
				if (oper) {
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
					    sizeof(uint32_t);
					ph = mtod(oper, struct sctp_paramhdr *);
					ph->param_type = htons(SCTP_CAUSE_USER_INITIATED_ABT);
					ph->param_length = htons(SCTP_BUF_LEN(oper));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_24);
				}
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_24;
				sctp_abort_an_association(stcb->sctp_ep, stcb, SCTP_RESPONSE_TO_USER_REQ, oper, SCTP_SO_NOT_LOCKED);
			} else {
				if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
				sctp_stop_timers_for_shutdown(stcb);
				sctp_send_shutdown(stcb,
				    stcb->asoc.primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
				    stcb->sctp_ep, stcb, asoc->primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb, asoc->primary_destination);
			}
		} else if ((SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED) &&
		    (asoc->stream_queue_cnt == 0)) {
			if (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT) {
				goto abort_out_now;
			}
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_ACK_SENT);
			sctp_send_shutdown_ack(stcb,
			    stcb->asoc.primary_destination);

			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK,
			    stcb->sctp_ep, stcb, asoc->primary_destination);
		}
	}
	if (sctp_logging_level & SCTP_SACK_RWND_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_SACK_RWND_UPDATE,
		    rwnd,
		    stcb->asoc.peers_rwnd,
		    stcb->asoc.total_flight,
		    stcb->asoc.total_output_queue_size);
	}
}

void
sctp_handle_sack(struct mbuf *m, int offset,
    struct sctp_sack_chunk *ch, struct sctp_tcb *stcb,
    struct sctp_nets *net_from, int *abort_now, int sack_len, uint32_t rwnd)
{
	struct sctp_association *asoc;
	struct sctp_sack *sack;
	struct sctp_tmit_chunk *tp1, *tp2;
	uint32_t cum_ack, last_tsn, biggest_tsn_acked, biggest_tsn_newly_acked,
	         this_sack_lowest_newack;
	uint32_t sav_cum_ack;
	uint16_t num_seg, num_dup;
	uint16_t wake_him = 0;
	unsigned int sack_length;
	uint32_t send_s = 0;
	long j;
	int accum_moved = 0;
	int will_exit_fast_recovery = 0;
	uint32_t a_rwnd, old_rwnd;
	int win_probe_recovery = 0;
	int win_probe_recovered = 0;
	struct sctp_nets *net = NULL;
	int nonce_sum_flag, ecn_seg_sums = 0;
	int done_once;
	uint8_t reneged_all = 0;
	uint8_t cmt_dac_flag;

	/*
	 * we take any chance we can to service our queues since we cannot
	 * get awoken when the socket is read from :<
	 */
	/*
	 * Now perform the actual SACK handling: 1) Verify that it is not an
	 * old sack, if so discard. 2) If there is nothing left in the send
	 * queue (cum-ack is equal to last acked) then you have a duplicate
	 * too, update any rwnd change and verify no timers are running.
	 * then return. 3) Process any new consequtive data i.e. cum-ack
	 * moved process these first and note that it moved. 4) Process any
	 * sack blocks. 5) Drop any acked from the queue. 6) Check for any
	 * revoked blocks and mark. 7) Update the cwnd. 8) Nothing left,
	 * sync up flightsizes and things, stop all timers and also check
	 * for shutdown_pending state. If so then go ahead and send off the
	 * shutdown. If in shutdown recv, send off the shutdown-ack and
	 * start that timer, Ret. 9) Strike any non-acked things and do FR
	 * procedure if needed being sure to set the FR flag. 10) Do pr-sctp
	 * procedures. 11) Apply any FR penalties. 12) Assure we will SACK
	 * if in shutdown_recv state.
	 */
	SCTP_TCB_LOCK_ASSERT(stcb);
	sack = &ch->sack;
	/* CMT DAC algo */
	this_sack_lowest_newack = 0;
	j = 0;
	sack_length = (unsigned int)sack_len;
	/* ECN Nonce */
	SCTP_STAT_INCR(sctps_slowpath_sack);
	nonce_sum_flag = ch->ch.chunk_flags & SCTP_SACK_NONCE_SUM;
	cum_ack = last_tsn = ntohl(sack->cum_tsn_ack);
#ifdef SCTP_ASOCLOG_OF_TSNS
	stcb->asoc.cumack_log[stcb->asoc.cumack_log_at] = cum_ack;
	stcb->asoc.cumack_log_at++;
	if (stcb->asoc.cumack_log_at > SCTP_TSN_LOG_SIZE) {
		stcb->asoc.cumack_log_at = 0;
	}
#endif
	num_seg = ntohs(sack->num_gap_ack_blks);
	a_rwnd = rwnd;

	if (sctp_logging_level & SCTP_LOG_SACK_ARRIVALS_ENABLE) {
		sctp_misc_ints(SCTP_SACK_LOG_NORMAL, cum_ack,
		    rwnd, stcb->asoc.last_acked_seq, stcb->asoc.peers_rwnd);
	}
	/* CMT DAC algo */
	cmt_dac_flag = ch->ch.chunk_flags & SCTP_SACK_CMT_DAC;
	num_dup = ntohs(sack->num_dup_tsns);

	old_rwnd = stcb->asoc.peers_rwnd;
	if (sctp_logging_level & SCTP_THRESHOLD_LOGGING) {
		sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
		    stcb->asoc.overall_error_count,
		    0,
		    SCTP_FROM_SCTP_INDATA,
		    __LINE__);
	}
	stcb->asoc.overall_error_count = 0;
	asoc = &stcb->asoc;
	if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
		sctp_log_sack(asoc->last_acked_seq,
		    cum_ack,
		    0,
		    num_seg,
		    num_dup,
		    SCTP_LOG_NEW_SACK);
	}
	if ((num_dup) && (sctp_logging_level & (SCTP_FR_LOGGING_ENABLE | SCTP_EARLYFR_LOGGING_ENABLE))) {
		int off_to_dup, iii;
		uint32_t *dupdata, dblock;

		off_to_dup = (num_seg * sizeof(struct sctp_gap_ack_block)) + sizeof(struct sctp_sack_chunk);
		if ((off_to_dup + (num_dup * sizeof(uint32_t))) <= sack_length) {
			dupdata = (uint32_t *) sctp_m_getptr(m, off_to_dup,
			    sizeof(uint32_t), (uint8_t *) & dblock);
			off_to_dup += sizeof(uint32_t);
			if (dupdata) {
				for (iii = 0; iii < num_dup; iii++) {
					sctp_log_fr(*dupdata, 0, 0, SCTP_FR_DUPED);
					dupdata = (uint32_t *) sctp_m_getptr(m, off_to_dup,
					    sizeof(uint32_t), (uint8_t *) & dblock);
					if (dupdata == NULL)
						break;
					off_to_dup += sizeof(uint32_t);
				}
			}
		} else {
			SCTP_PRINTF("Size invalid offset to dups:%d number dups:%d sack_len:%d num gaps:%d\n",
			    off_to_dup, num_dup, sack_length, num_seg);
		}
	}
	if (sctp_strict_sacks) {
		/* reality check */
		if (!TAILQ_EMPTY(&asoc->sent_queue)) {
			tp1 = TAILQ_LAST(&asoc->sent_queue,
			    sctpchunk_listhead);
			send_s = tp1->rec.data.TSN_seq + 1;
		} else {
			send_s = asoc->sending_seq;
		}
		if (cum_ack == send_s ||
		    compare_with_wrap(cum_ack, send_s, MAX_TSN)) {
#ifndef INVARIANTS
			struct mbuf *oper;

#endif
#ifdef INVARIANTS
	hopeless_peer:
			panic("Impossible sack 1");
#else


			/*
			 * no way, we have not even sent this TSN out yet.
			 * Peer is hopelessly messed up with us.
			 */
	hopeless_peer:
			*abort_now = 1;
			/* XXX */
			oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
			    0, M_DONTWAIT, 1, MT_DATA);
			if (oper) {
				struct sctp_paramhdr *ph;
				uint32_t *ippp;

				SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
				    sizeof(uint32_t);
				ph = mtod(oper, struct sctp_paramhdr *);
				ph->param_type = htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
				ph->param_length = htons(SCTP_BUF_LEN(oper));
				ippp = (uint32_t *) (ph + 1);
				*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_25);
			}
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_25;
			sctp_abort_an_association(stcb->sctp_ep, stcb, SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
			return;
#endif
		}
	}
	/**********************/
	/* 1) check the range */
	/**********************/
	if (compare_with_wrap(asoc->last_acked_seq, last_tsn, MAX_TSN)) {
		/* acking something behind */
		return;
	}
	sav_cum_ack = asoc->last_acked_seq;

	/* update the Rwnd of the peer */
	if (TAILQ_EMPTY(&asoc->sent_queue) &&
	    TAILQ_EMPTY(&asoc->send_queue) &&
	    (asoc->stream_queue_cnt == 0)
	    ) {
		/* nothing left on send/sent and strmq */
		if (sctp_logging_level & SCTP_LOG_RWND_ENABLE) {
			sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
			    asoc->peers_rwnd, 0, 0, a_rwnd);
		}
		asoc->peers_rwnd = a_rwnd;
		if (asoc->sent_queue_retran_cnt) {
			asoc->sent_queue_retran_cnt = 0;
		}
		if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {
			/* SWS sender side engages */
			asoc->peers_rwnd = 0;
		}
		/* stop any timers */
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
			    stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_26);
			if (sctp_early_fr) {
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck1);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_26);
				}
			}
			net->partial_bytes_acked = 0;
			net->flight_size = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
		return;
	}
	/*
	 * We init netAckSz and netAckSz2 to 0. These are used to track 2
	 * things. The total byte count acked is tracked in netAckSz AND
	 * netAck2 is used to track the total bytes acked that are un-
	 * amibguious and were never retransmitted. We track these on a per
	 * destination address basis.
	 */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		net->prev_cwnd = net->cwnd;
		net->net_ack = 0;
		net->net_ack2 = 0;

		/*
		 * CMT: Reset CUC and Fast recovery algo variables before
		 * SACK processing
		 */
		net->new_pseudo_cumack = 0;
		net->will_exit_fast_recovery = 0;
	}
	/* process the new consecutive TSN first */
	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	while (tp1) {
		if (compare_with_wrap(last_tsn, tp1->rec.data.TSN_seq,
		    MAX_TSN) ||
		    last_tsn == tp1->rec.data.TSN_seq) {
			if (tp1->sent != SCTP_DATAGRAM_UNSENT) {
				/*
				 * ECN Nonce: Add the nonce to the sender's
				 * nonce sum
				 */
				asoc->nonce_sum_expect_base += tp1->rec.data.ect_nonce;
				accum_moved = 1;
				if (tp1->sent < SCTP_DATAGRAM_ACKED) {
					/*
					 * If it is less than ACKED, it is
					 * now no-longer in flight. Higher
					 * values may occur during marking
					 */
					if ((tp1->whoTo->dest_state &
					    SCTP_ADDR_UNCONFIRMED) &&
					    (tp1->snd_count < 2)) {
						/*
						 * If there was no retran
						 * and the address is
						 * un-confirmed and we sent
						 * there and are now
						 * sacked.. its confirmed,
						 * mark it so.
						 */
						tp1->whoTo->dest_state &=
						    ~SCTP_ADDR_UNCONFIRMED;
					}
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_CA,
							    tp1->whoTo->flight_size,
							    tp1->book_size,
							    (uintptr_t) tp1->whoTo,
							    tp1->rec.data.TSN_seq);
						}
						sctp_flight_size_decrease(tp1);
						sctp_total_flight_decrease(stcb, tp1);
					}
					tp1->whoTo->net_ack += tp1->send_size;

					/* CMT SFR and DAC algos */
					this_sack_lowest_newack = tp1->rec.data.TSN_seq;
					tp1->whoTo->saw_newack = 1;

					if (tp1->snd_count < 2) {
						/*
						 * True non-retransmited
						 * chunk
						 */
						tp1->whoTo->net_ack2 +=
						    tp1->send_size;

						/* update RTO too? */
						if (tp1->do_rtt) {
							tp1->whoTo->RTO =
							    sctp_calculate_rto(stcb,
							    asoc, tp1->whoTo,
							    &tp1->sent_rcv_time,
							    sctp_align_safe_nocopy);
							tp1->do_rtt = 0;
						}
					}
					/*
					 * CMT: CUCv2 algorithm. From the
					 * cumack'd TSNs, for each TSN being
					 * acked for the first time, set the
					 * following variables for the
					 * corresp destination.
					 * new_pseudo_cumack will trigger a
					 * cwnd update.
					 * find_(rtx_)pseudo_cumack will
					 * trigger search for the next
					 * expected (rtx-)pseudo-cumack.
					 */
					tp1->whoTo->new_pseudo_cumack = 1;
					tp1->whoTo->find_pseudo_cumack = 1;
					tp1->whoTo->find_rtx_pseudo_cumack = 1;


					if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
						sctp_log_sack(asoc->last_acked_seq,
						    cum_ack,
						    tp1->rec.data.TSN_seq,
						    0,
						    0,
						    SCTP_LOG_TSN_ACKED);
					}
					if (sctp_logging_level & SCTP_CWND_LOGGING_ENABLE) {
						sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.TSN_seq, SCTP_CWND_LOG_FROM_SACK);
					}
				}
				if (tp1->sent == SCTP_DATAGRAM_RESEND) {
					sctp_ucount_decr(asoc->sent_queue_retran_cnt);
#ifdef SCTP_AUDITING_ENABLED
					sctp_audit_log(0xB3,
					    (asoc->sent_queue_retran_cnt & 0x000000ff));
#endif
				}
				if (tp1->rec.data.chunk_was_revoked) {
					/* deflate the cwnd */
					tp1->whoTo->cwnd -= tp1->book_size;
					tp1->rec.data.chunk_was_revoked = 0;
				}
				tp1->sent = SCTP_DATAGRAM_ACKED;
			}
		} else {
			break;
		}
		tp1 = TAILQ_NEXT(tp1, sctp_next);
	}
	biggest_tsn_newly_acked = biggest_tsn_acked = last_tsn;
	/* always set this up to cum-ack */
	asoc->this_sack_highest_gap = last_tsn;

	/* Move offset up to point to gaps/dups */
	offset += sizeof(struct sctp_sack_chunk);
	if (((num_seg * (sizeof(struct sctp_gap_ack_block))) + sizeof(struct sctp_sack_chunk)) > sack_length) {

		/* skip corrupt segments */
		goto skip_segments;
	}
	if (num_seg > 0) {

		/*
		 * CMT: SFR algo (and HTNA) - this_sack_highest_newack has
		 * to be greater than the cumack. Also reset saw_newack to 0
		 * for all dests.
		 */
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			net->saw_newack = 0;
			net->this_sack_highest_newack = last_tsn;
		}

		/*
		 * thisSackHighestGap will increase while handling NEW
		 * segments this_sack_highest_newack will increase while
		 * handling NEWLY ACKED chunks. this_sack_lowest_newack is
		 * used for CMT DAC algo. saw_newack will also change.
		 */
		sctp_handle_segments(m, &offset, stcb, asoc, ch, last_tsn,
		    &biggest_tsn_acked, &biggest_tsn_newly_acked, &this_sack_lowest_newack,
		    num_seg, &ecn_seg_sums);

		if (sctp_strict_sacks) {
			/*
			 * validate the biggest_tsn_acked in the gap acks if
			 * strict adherence is wanted.
			 */
			if ((biggest_tsn_acked == send_s) ||
			    (compare_with_wrap(biggest_tsn_acked, send_s, MAX_TSN))) {
				/*
				 * peer is either confused or we are under
				 * attack. We must abort.
				 */
				goto hopeless_peer;
			}
		}
	}
skip_segments:
	/*******************************************/
	/* cancel ALL T3-send timer if accum moved */
	/*******************************************/
	if (sctp_cmt_on_off) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->new_pseudo_cumack)
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net,
				    SCTP_FROM_SCTP_INDATA + SCTP_LOC_27);

		}
	} else {
		if (accum_moved) {
			TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_28);
			}
		}
	}
	/********************************************/
	/* drop the acked chunks from the sendqueue */
	/********************************************/
	asoc->last_acked_seq = cum_ack;

	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	if (tp1 == NULL)
		goto done_with_it;
	do {
		if (compare_with_wrap(tp1->rec.data.TSN_seq, cum_ack,
		    MAX_TSN)) {
			break;
		}
		if (tp1->sent == SCTP_DATAGRAM_UNSENT) {
			/* no more sent on list */
			printf("Warning, tp1->sent == %d and its now acked?\n",
			    tp1->sent);
		}
		tp2 = TAILQ_NEXT(tp1, sctp_next);
		TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
		if (tp1->pr_sctp_on) {
			if (asoc->pr_sctp_cnt != 0)
				asoc->pr_sctp_cnt--;
		}
		if ((TAILQ_FIRST(&asoc->sent_queue) == NULL) &&
		    (asoc->total_flight > 0)) {
#ifdef INVARIANTS
			panic("Warning flight size is postive and should be 0");
#else
			SCTP_PRINTF("Warning flight size incorrect should be 0 is %d\n",
			    asoc->total_flight);
#endif
			asoc->total_flight = 0;
		}
		if (tp1->data) {
			/* sa_ignore NO_NULL_CHK */
			sctp_free_bufspace(stcb, asoc, tp1, 1);
			sctp_m_freem(tp1->data);
			if (PR_SCTP_BUF_ENABLED(tp1->flags)) {
				asoc->sent_queue_cnt_removeable--;
			}
		}
		if (sctp_logging_level & SCTP_SACK_LOGGING_ENABLE) {
			sctp_log_sack(asoc->last_acked_seq,
			    cum_ack,
			    tp1->rec.data.TSN_seq,
			    0,
			    0,
			    SCTP_LOG_FREE_SENT);
		}
		tp1->data = NULL;
		asoc->sent_queue_cnt--;
		sctp_free_a_chunk(stcb, tp1);
		wake_him++;
		tp1 = tp2;
	} while (tp1 != NULL);

done_with_it:
	/* sa_ignore NO_NULL_CHK */
	if ((wake_him) && (stcb->sctp_socket)) {
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		struct socket *so;

#endif
		SOCKBUF_LOCK(&stcb->sctp_socket->so_snd);
		if (sctp_logging_level & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, cum_ack, wake_him, SCTP_WAKESND_FROM_SACK);
		}
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		so = SCTP_INP_SO(stcb->sctp_ep);
		atomic_add_int(&stcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_SOCKET_LOCK(so, 1);
		SCTP_TCB_LOCK(stcb);
		atomic_subtract_int(&stcb->asoc.refcnt, 1);
		if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {
			/* assoc was freed while we were unlocked */
			SCTP_SOCKET_UNLOCK(so, 1);
			return;
		}
#endif
		sctp_sowwakeup_locked(stcb->sctp_ep, stcb->sctp_socket);
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	} else {
		if (sctp_logging_level & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, cum_ack, wake_him, SCTP_NOWAKE_FROM_SACK);
		}
	}

	if (asoc->fast_retran_loss_recovery && accum_moved) {
		if (compare_with_wrap(asoc->last_acked_seq,
		    asoc->fast_recovery_tsn, MAX_TSN) ||
		    asoc->last_acked_seq == asoc->fast_recovery_tsn) {
			/* Setup so we will exit RFC2582 fast recovery */
			will_exit_fast_recovery = 1;
		}
	}
	/*
	 * Check for revoked fragments:
	 * 
	 * if Previous sack - Had no frags then we can't have any revoked if
	 * Previous sack - Had frag's then - If we now have frags aka
	 * num_seg > 0 call sctp_check_for_revoked() to tell if peer revoked
	 * some of them. else - The peer revoked all ACKED fragments, since
	 * we had some before and now we have NONE.
	 */

	if (num_seg)
		sctp_check_for_revoked(stcb, asoc, cum_ack, biggest_tsn_acked);
	else if (asoc->saw_sack_with_frags) {
		int cnt_revoked = 0;

		tp1 = TAILQ_FIRST(&asoc->sent_queue);
		if (tp1 != NULL) {
			/* Peer revoked all dg's marked or acked */
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if ((tp1->sent > SCTP_DATAGRAM_RESEND) &&
				    (tp1->sent < SCTP_FORWARD_TSN_SKIP)) {
					tp1->sent = SCTP_DATAGRAM_SENT;
					if (sctp_logging_level & SCTP_FLIGHT_LOGGING_ENABLE) {
						sctp_misc_ints(SCTP_FLIGHT_LOG_UP_REVOKE,
						    tp1->whoTo->flight_size,
						    tp1->book_size,
						    (uintptr_t) tp1->whoTo,
						    tp1->rec.data.TSN_seq);
					}
					sctp_flight_size_increase(tp1);
					sctp_total_flight_increase(stcb, tp1);
					tp1->rec.data.chunk_was_revoked = 1;
					/*
					 * To ensure that this increase in
					 * flightsize, which is artificial,
					 * does not throttle the sender, we
					 * also increase the cwnd
					 * artificially.
					 */
					tp1->whoTo->cwnd += tp1->book_size;
					cnt_revoked++;
				}
			}
			if (cnt_revoked) {
				reneged_all = 1;
			}
		}
		asoc->saw_sack_with_frags = 0;
	}
	if (num_seg)
		asoc->saw_sack_with_frags = 1;
	else
		asoc->saw_sack_with_frags = 0;

	/* JRS - Use the congestion control given in the CC module */
	asoc->cc_functions.sctp_cwnd_update_after_sack(stcb, asoc, accum_moved, reneged_all, will_exit_fast_recovery);

	if (TAILQ_EMPTY(&asoc->sent_queue)) {
		/* nothing left in-flight */
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			/* stop all timers */
			if (sctp_early_fr) {
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck4);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_29);
				}
			}
			sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
			    stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_30);
			net->flight_size = 0;
			net->partial_bytes_acked = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
	}
	/**********************************/
	/* Now what about shutdown issues */
	/**********************************/
	if (TAILQ_EMPTY(&asoc->send_queue) && TAILQ_EMPTY(&asoc->sent_queue)) {
		/* nothing left on sendqueue.. consider done */
		if (sctp_logging_level & SCTP_LOG_RWND_ENABLE) {
			sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
			    asoc->peers_rwnd, 0, 0, a_rwnd);
		}
		asoc->peers_rwnd = a_rwnd;
		if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {
			/* SWS sender side engages */
			asoc->peers_rwnd = 0;
		}
		/* clean up */
		if ((asoc->stream_queue_cnt == 1) &&
		    ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) ||
		    (asoc->state & SCTP_STATE_SHUTDOWN_RECEIVED)) &&
		    (asoc->locked_on_sending)
		    ) {
			struct sctp_stream_queue_pending *sp;

			/*
			 * I may be in a state where we got all across.. but
			 * cannot write more due to a shutdown... we abort
			 * since the user did not indicate EOR in this case.
			 */
			sp = TAILQ_LAST(&((asoc->locked_on_sending)->outqueue),
			    sctp_streamhead);
			if ((sp) && (sp->length == 0)) {
				asoc->locked_on_sending = NULL;
				if (sp->msg_is_complete) {
					asoc->stream_queue_cnt--;
				} else {
					asoc->state |= SCTP_STATE_PARTIAL_MSG_LEFT;
					asoc->stream_queue_cnt--;
				}
			}
		}
		if ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) &&
		    (asoc->stream_queue_cnt == 0)) {
			if (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT) {
				/* Need to abort here */
				struct mbuf *oper;

		abort_out_now:
				*abort_now = 1;
				/* XXX */
				oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);
				if (oper) {
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
					    sizeof(uint32_t);
					ph = mtod(oper, struct sctp_paramhdr *);
					ph->param_type = htons(SCTP_CAUSE_USER_INITIATED_ABT);
					ph->param_length = htons(SCTP_BUF_LEN(oper));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_31);
				}
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_31;
				sctp_abort_an_association(stcb->sctp_ep, stcb, SCTP_RESPONSE_TO_USER_REQ, oper, SCTP_SO_NOT_LOCKED);
				return;
			} else {
				if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
				sctp_stop_timers_for_shutdown(stcb);
				sctp_send_shutdown(stcb,
				    stcb->asoc.primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
				    stcb->sctp_ep, stcb, asoc->primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb, asoc->primary_destination);
			}
			return;
		} else if ((SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED) &&
		    (asoc->stream_queue_cnt == 0)) {
			if (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT) {
				goto abort_out_now;
			}
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_ACK_SENT);
			sctp_send_shutdown_ack(stcb,
			    stcb->asoc.primary_destination);

			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK,
			    stcb->sctp_ep, stcb, asoc->primary_destination);
			return;
		}
	}
	/*
	 * Now here we are going to recycle net_ack for a different use...
	 * HEADS UP.
	 */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		net->net_ack = 0;
	}

	/*
	 * CMT DAC algorithm: If SACK DAC flag was 0, then no extra marking
	 * to be done. Setting this_sack_lowest_newack to the cum_ack will
	 * automatically ensure that.
	 */
	if (sctp_cmt_on_off && sctp_cmt_use_dac && (cmt_dac_flag == 0)) {
		this_sack_lowest_newack = cum_ack;
	}
	if (num_seg > 0) {
		sctp_strike_gap_ack_chunks(stcb, asoc, biggest_tsn_acked,
		    biggest_tsn_newly_acked, this_sack_lowest_newack, accum_moved);
	}
	/*********************************************/
	/* Here we perform PR-SCTP procedures        */
	/* (section 4.2)                             */
	/*********************************************/
	/* C1. update advancedPeerAckPoint */
	if (compare_with_wrap(cum_ack, asoc->advanced_peer_ack_point, MAX_TSN)) {
		asoc->advanced_peer_ack_point = cum_ack;
	}
	/* C2. try to further move advancedPeerAckPoint ahead */
	if ((asoc->peer_supports_prsctp) && (asoc->pr_sctp_cnt > 0)) {
		struct sctp_tmit_chunk *lchk;

		lchk = sctp_try_advance_peer_ack_point(stcb, asoc);
		/* C3. See if we need to send a Fwd-TSN */
		if (compare_with_wrap(asoc->advanced_peer_ack_point, cum_ack,
		    MAX_TSN)) {
			/*
			 * ISSUE with ECN, see FWD-TSN processing for notes
			 * on issues that will occur when the ECN NONCE
			 * stuff is put into SCTP for cross checking.
			 */
			send_forward_tsn(stcb, asoc);

			/*
			 * ECN Nonce: Disable Nonce Sum check when FWD TSN
			 * is sent and store resync tsn
			 */
			asoc->nonce_sum_check = 0;
			asoc->nonce_resync_tsn = asoc->advanced_peer_ack_point;
			if (lchk) {
				/* Assure a timer is up */
				sctp_timer_start(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, lchk->whoTo);
			}
		}
	}
	/* JRS - Use the congestion control given in the CC module */
	asoc->cc_functions.sctp_cwnd_update_after_fr(stcb, asoc);

	/******************************************************************
	 *  Here we do the stuff with ECN Nonce checking.
	 *  We basically check to see if the nonce sum flag was incorrect
	 *  or if resynchronization needs to be done. Also if we catch a
	 *  misbehaving receiver we give him the kick.
	 ******************************************************************/

	if (asoc->ecn_nonce_allowed) {
		if (asoc->nonce_sum_check) {
			if (nonce_sum_flag != ((asoc->nonce_sum_expect_base + ecn_seg_sums) & SCTP_SACK_NONCE_SUM)) {
				if (asoc->nonce_wait_for_ecne == 0) {
					struct sctp_tmit_chunk *lchk;

					lchk = TAILQ_FIRST(&asoc->send_queue);
					asoc->nonce_wait_for_ecne = 1;
					if (lchk) {
						asoc->nonce_wait_tsn = lchk->rec.data.TSN_seq;
					} else {
						asoc->nonce_wait_tsn = asoc->sending_seq;
					}
				} else {
					if (compare_with_wrap(asoc->last_acked_seq, asoc->nonce_wait_tsn, MAX_TSN) ||
					    (asoc->last_acked_seq == asoc->nonce_wait_tsn)) {
						/*
						 * Misbehaving peer. We need
						 * to react to this guy
						 */
						asoc->ecn_allowed = 0;
						asoc->ecn_nonce_allowed = 0;
					}
				}
			}
		} else {
			/* See if Resynchronization Possible */
			if (compare_with_wrap(asoc->last_acked_seq, asoc->nonce_resync_tsn, MAX_TSN)) {
				asoc->nonce_sum_check = 1;
				/*
				 * now we must calculate what the base is.
				 * We do this based on two things, we know
				 * the total's for all the segments
				 * gap-acked in the SACK, its stored in
				 * ecn_seg_sums. We also know the SACK's
				 * nonce sum, its in nonce_sum_flag. So we
				 * can build a truth table to back-calculate
				 * the new value of
				 * asoc->nonce_sum_expect_base:
				 * 
				 * SACK-flag-Value         Seg-Sums Base 0 0 0
				 * 1                    0 1 0 1 1 1
				 * 1 0
				 */
				asoc->nonce_sum_expect_base = (ecn_seg_sums ^ nonce_sum_flag) & SCTP_SACK_NONCE_SUM;
			}
		}
	}
	/* Now are we exiting loss recovery ? */
	if (will_exit_fast_recovery) {
		/* Ok, we must exit fast recovery */
		asoc->fast_retran_loss_recovery = 0;
	}
	if ((asoc->sat_t3_loss_recovery) &&
	    ((compare_with_wrap(asoc->last_acked_seq, asoc->sat_t3_recovery_tsn,
	    MAX_TSN) ||
	    (asoc->last_acked_seq == asoc->sat_t3_recovery_tsn)))) {
		/* end satellite t3 loss recovery */
		asoc->sat_t3_loss_recovery = 0;
	}
	/*
	 * CMT Fast recovery
	 */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (net->will_exit_fast_recovery) {
			/* Ok, we must exit fast recovery */
			net->fast_retran_loss_recovery = 0;
		}
	}

	/* Adjust and set the new rwnd value */
	if (sctp_logging_level & SCTP_LOG_RWND_ENABLE) {
		sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
		    asoc->peers_rwnd, asoc->total_flight, (asoc->sent_queue_cnt * sctp_peer_chunk_oh), a_rwnd);
	}
	asoc->peers_rwnd = sctp_sbspace_sub(a_rwnd,
	    (uint32_t) (asoc->total_flight + (asoc->sent_queue_cnt * sctp_peer_chunk_oh)));
	if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {
		/* SWS sender side engages */
		asoc->peers_rwnd = 0;
	}
	if (asoc->peers_rwnd > old_rwnd) {
		win_probe_recovery = 1;
	}
	/*
	 * Now we must setup so we have a timer up for anyone with
	 * outstanding data.
	 */
	done_once = 0;
again:
	j = 0;
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (win_probe_recovery && (net->window_probe)) {
			net->window_probe = 0;
			win_probe_recovered = 1;
			/*-
			 * Find first chunk that was used with
			 * window probe and clear the event. Put
			 * it back into the send queue as if has
			 * not been sent.
			 */
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->window_probe) {
					sctp_window_probe_recovery(stcb, asoc, net, tp1);
					break;
				}
			}
		}
		if (net->flight_size) {
			j++;
			sctp_timer_start(SCTP_TIMER_TYPE_SEND,
			    stcb->sctp_ep, stcb, net);
		} else {
			if (SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net,
				    SCTP_FROM_SCTP_INDATA + SCTP_LOC_22);
			}
			if (sctp_early_fr) {
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck4);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_23);
				}
			}
		}
	}
	if ((j == 0) &&
	    (!TAILQ_EMPTY(&asoc->sent_queue)) &&
	    (asoc->sent_queue_retran_cnt == 0) &&
	    (win_probe_recovered == 0) &&
	    (done_once == 0)) {
		/* huh, this should not happen */
		sctp_fs_audit(asoc);
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			net->flight_size = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
		asoc->sent_queue_retran_cnt = 0;
		TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				sctp_flight_size_increase(tp1);
				sctp_total_flight_increase(stcb, tp1);
			} else if (tp1->sent == SCTP_DATAGRAM_RESEND) {
				asoc->sent_queue_retran_cnt++;
			}
		}
		done_once = 1;
		goto again;
	}
	if (sctp_logging_level & SCTP_SACK_RWND_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_SACK_RWND_UPDATE,
		    a_rwnd,
		    stcb->asoc.peers_rwnd,
		    stcb->asoc.total_flight,
		    stcb->asoc.total_output_queue_size);
	}
}

void
sctp_update_acked(struct sctp_tcb *stcb, struct sctp_shutdown_chunk *cp,
    struct sctp_nets *netp, int *abort_flag)
{
	/* Copy cum-ack */
	uint32_t cum_ack, a_rwnd;

	cum_ack = ntohl(cp->cumulative_tsn_ack);
	/* Arrange so a_rwnd does NOT change */
	a_rwnd = stcb->asoc.peers_rwnd + stcb->asoc.total_flight;

	/* Now call the express sack handling */
	sctp_express_handle_sack(stcb, cum_ack, a_rwnd, 0, abort_flag);
}

static void
sctp_kick_prsctp_reorder_queue(struct sctp_tcb *stcb,
    struct sctp_stream_in *strmin)
{
	struct sctp_queued_to_read *ctl, *nctl;
	struct sctp_association *asoc;
	int tt;

	asoc = &stcb->asoc;
	tt = strmin->last_sequence_delivered;
	/*
	 * First deliver anything prior to and including the stream no that
	 * came in
	 */
	ctl = TAILQ_FIRST(&strmin->inqueue);
	while (ctl) {
		nctl = TAILQ_NEXT(ctl, next);
		if (compare_with_wrap(tt, ctl->sinfo_ssn, MAX_SEQ) ||
		    (tt == ctl->sinfo_ssn)) {
			/* this is deliverable now */
			TAILQ_REMOVE(&strmin->inqueue, ctl, next);
			/* subtract pending on streams */
			asoc->size_on_all_streams -= ctl->length;
			sctp_ucount_decr(asoc->cnt_on_all_streams);
			/* deliver it to at least the delivery-q */
			if (stcb->sctp_socket) {
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				    ctl,
				    &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
			}
		} else {
			/* no more delivery now. */
			break;
		}
		ctl = nctl;
	}
	/*
	 * now we must deliver things in queue the normal way  if any are
	 * now ready.
	 */
	tt = strmin->last_sequence_delivered + 1;
	ctl = TAILQ_FIRST(&strmin->inqueue);
	while (ctl) {
		nctl = TAILQ_NEXT(ctl, next);
		if (tt == ctl->sinfo_ssn) {
			/* this is deliverable now */
			TAILQ_REMOVE(&strmin->inqueue, ctl, next);
			/* subtract pending on streams */
			asoc->size_on_all_streams -= ctl->length;
			sctp_ucount_decr(asoc->cnt_on_all_streams);
			/* deliver it to at least the delivery-q */
			strmin->last_sequence_delivered = ctl->sinfo_ssn;
			if (stcb->sctp_socket) {
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				    ctl,
				    &stcb->sctp_socket->so_rcv, 1, SCTP_SO_NOT_LOCKED);
			}
			tt = strmin->last_sequence_delivered + 1;
		} else {
			break;
		}
		ctl = nctl;
	}
}

void
sctp_handle_forward_tsn(struct sctp_tcb *stcb,
    struct sctp_forward_tsn_chunk *fwd, int *abort_flag, struct mbuf *m, int offset)
{
	/*
	 * ISSUES that MUST be fixed for ECN! When we are the sender of the
	 * forward TSN, when the SACK comes back that acknowledges the
	 * FWD-TSN we must reset the NONCE sum to match correctly. This will
	 * get quite tricky since we may have sent more data interveneing
	 * and must carefully account for what the SACK says on the nonce
	 * and any gaps that are reported. This work will NOT be done here,
	 * but I note it here since it is really related to PR-SCTP and
	 * FWD-TSN's
	 */

	/* The pr-sctp fwd tsn */
	/*
	 * here we will perform all the data receiver side steps for
	 * processing FwdTSN, as required in by pr-sctp draft:
	 * 
	 * Assume we get FwdTSN(x):
	 * 
	 * 1) update local cumTSN to x 2) try to further advance cumTSN to x +
	 * others we have 3) examine and update re-ordering queue on
	 * pr-in-streams 4) clean up re-assembly queue 5) Send a sack to
	 * report where we are.
	 */
	struct sctp_association *asoc;
	uint32_t new_cum_tsn, gap;
	unsigned int i, cnt_gone, fwd_sz, cumack_set_flag, m_size;
	struct sctp_stream_in *strm;
	struct sctp_tmit_chunk *chk, *at;

	cumack_set_flag = 0;
	asoc = &stcb->asoc;
	cnt_gone = 0;
	if ((fwd_sz = ntohs(fwd->ch.chunk_length)) < sizeof(struct sctp_forward_tsn_chunk)) {
		SCTPDBG(SCTP_DEBUG_INDATA1,
		    "Bad size too small/big fwd-tsn\n");
		return;
	}
	m_size = (stcb->asoc.mapping_array_size << 3);
	/*************************************************************/
	/* 1. Here we update local cumTSN and shift the bitmap array */
	/*************************************************************/
	new_cum_tsn = ntohl(fwd->new_cumulative_tsn);

	if (compare_with_wrap(asoc->cumulative_tsn, new_cum_tsn, MAX_TSN) ||
	    asoc->cumulative_tsn == new_cum_tsn) {
		/* Already got there ... */
		return;
	}
	if (compare_with_wrap(new_cum_tsn, asoc->highest_tsn_inside_map,
	    MAX_TSN)) {
		asoc->highest_tsn_inside_map = new_cum_tsn;
		if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 0, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
	}
	/*
	 * now we know the new TSN is more advanced, let's find the actual
	 * gap
	 */
	if ((compare_with_wrap(new_cum_tsn, asoc->mapping_array_base_tsn,
	    MAX_TSN)) ||
	    (new_cum_tsn == asoc->mapping_array_base_tsn)) {
		gap = new_cum_tsn - asoc->mapping_array_base_tsn;
	} else {
		/* try to prevent underflow here */
		gap = new_cum_tsn + (MAX_TSN - asoc->mapping_array_base_tsn) + 1;
	}

	if (gap >= m_size) {
		if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 0, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
		if ((long)gap > sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv)) {
			struct mbuf *oper;

			/*
			 * out of range (of single byte chunks in the rwnd I
			 * give out). This must be an attacker.
			 */
			*abort_flag = 1;
			oper = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + 3 * sizeof(uint32_t)),
			    0, M_DONTWAIT, 1, MT_DATA);
			if (oper) {
				struct sctp_paramhdr *ph;
				uint32_t *ippp;

				SCTP_BUF_LEN(oper) = sizeof(struct sctp_paramhdr) +
				    (sizeof(uint32_t) * 3);
				ph = mtod(oper, struct sctp_paramhdr *);
				ph->param_type = htons(SCTP_CAUSE_PROTOCOL_VIOLATION);
				ph->param_length = htons(SCTP_BUF_LEN(oper));
				ippp = (uint32_t *) (ph + 1);
				*ippp = htonl(SCTP_FROM_SCTP_INDATA + SCTP_LOC_33);
				ippp++;
				*ippp = asoc->highest_tsn_inside_map;
				ippp++;
				*ippp = new_cum_tsn;
			}
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_33;
			sctp_abort_an_association(stcb->sctp_ep, stcb,
			    SCTP_PEER_FAULTY, oper, SCTP_SO_NOT_LOCKED);
			return;
		}
		SCTP_STAT_INCR(sctps_fwdtsn_map_over);
slide_out:
		memset(stcb->asoc.mapping_array, 0, stcb->asoc.mapping_array_size);
		cumack_set_flag = 1;
		asoc->mapping_array_base_tsn = new_cum_tsn + 1;
		asoc->cumulative_tsn = asoc->highest_tsn_inside_map = new_cum_tsn;

		if (sctp_logging_level & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 3, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
		asoc->last_echo_tsn = asoc->highest_tsn_inside_map;
	} else {
		SCTP_TCB_LOCK_ASSERT(stcb);
		if ((compare_with_wrap(((uint32_t) asoc->cumulative_tsn + gap), asoc->highest_tsn_inside_map, MAX_TSN)) ||
		    (((uint32_t) asoc->cumulative_tsn + gap) == asoc->highest_tsn_inside_map)) {
			goto slide_out;
		} else {
			for (i = 0; i <= gap; i++) {
				SCTP_SET_TSN_PRESENT(asoc->mapping_array, i);
			}
		}
		/*
		 * Now after marking all, slide thing forward but no sack
		 * please.
		 */
		sctp_sack_check(stcb, 0, 0, abort_flag);
		if (*abort_flag)
			return;
	}

	/*************************************************************/
	/* 2. Clear up re-assembly queue                             */
	/*************************************************************/
	/*
	 * First service it if pd-api is up, just in case we can progress it
	 * forward
	 */
	if (asoc->fragmented_delivery_inprogress) {
		sctp_service_reassembly(stcb, asoc);
	}
	if (!TAILQ_EMPTY(&asoc->reasmqueue)) {
		/* For each one on here see if we need to toss it */
		/*
		 * For now large messages held on the reasmqueue that are
		 * complete will be tossed too. We could in theory do more
		 * work to spin through and stop after dumping one msg aka
		 * seeing the start of a new msg at the head, and call the
		 * delivery function... to see if it can be delivered... But
		 * for now we just dump everything on the queue.
		 */
		chk = TAILQ_FIRST(&asoc->reasmqueue);
		while (chk) {
			at = TAILQ_NEXT(chk, sctp_next);
			if (compare_with_wrap(asoc->cumulative_tsn,
			    chk->rec.data.TSN_seq, MAX_TSN) ||
			    asoc->cumulative_tsn == chk->rec.data.TSN_seq) {
				/* It needs to be tossed */
				TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
				if (compare_with_wrap(chk->rec.data.TSN_seq,
				    asoc->tsn_last_delivered, MAX_TSN)) {
					asoc->tsn_last_delivered =
					    chk->rec.data.TSN_seq;
					asoc->str_of_pdapi =
					    chk->rec.data.stream_number;
					asoc->ssn_of_pdapi =
					    chk->rec.data.stream_seq;
					asoc->fragment_flags =
					    chk->rec.data.rcv_flags;
				}
				asoc->size_on_reasm_queue -= chk->send_size;
				sctp_ucount_decr(asoc->cnt_on_reasm_queue);
				cnt_gone++;

				/* Clear up any stream problem */
				if ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) !=
				    SCTP_DATA_UNORDERED &&
				    (compare_with_wrap(chk->rec.data.stream_seq,
				    asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered,
				    MAX_SEQ))) {
					/*
					 * We must dump forward this streams
					 * sequence number if the chunk is
					 * not unordered that is being
					 * skipped. There is a chance that
					 * if the peer does not include the
					 * last fragment in its FWD-TSN we
					 * WILL have a problem here since
					 * you would have a partial chunk in
					 * queue that may not be
					 * deliverable. Also if a Partial
					 * delivery API as started the user
					 * may get a partial chunk. The next
					 * read returning a new chunk...
					 * really ugly but I see no way
					 * around it! Maybe a notify??
					 */
					asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered =
					    chk->rec.data.stream_seq;
				}
				if (chk->data) {
					sctp_m_freem(chk->data);
					chk->data = NULL;
				}
				sctp_free_a_chunk(stcb, chk);
			} else {
				/*
				 * Ok we have gone beyond the end of the
				 * fwd-tsn's mark. Some checks...
				 */
				if ((asoc->fragmented_delivery_inprogress) &&
				    (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG)) {
					uint32_t str_seq;

					/*
					 * Special case PD-API is up and
					 * what we fwd-tsn' over includes
					 * one that had the LAST_FRAG. We no
					 * longer need to do the PD-API.
					 */
					asoc->fragmented_delivery_inprogress = 0;

					str_seq = (asoc->str_of_pdapi << 16) | asoc->ssn_of_pdapi;
					sctp_ulp_notify(SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION,
					    stcb, SCTP_PARTIAL_DELIVERY_ABORTED, (void *)&str_seq, SCTP_SO_NOT_LOCKED);

				}
				break;
			}
			chk = at;
		}
	}
	if (asoc->fragmented_delivery_inprogress) {
		/*
		 * Ok we removed cnt_gone chunks in the PD-API queue that
		 * were being delivered. So now we must turn off the flag.
		 */
		uint32_t str_seq;

		str_seq = (asoc->str_of_pdapi << 16) | asoc->ssn_of_pdapi;
		sctp_ulp_notify(SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION,
		    stcb, SCTP_PARTIAL_DELIVERY_ABORTED, (void *)&str_seq, SCTP_SO_NOT_LOCKED);
		asoc->fragmented_delivery_inprogress = 0;
	}
	/*************************************************************/
	/* 3. Update the PR-stream re-ordering queues                */
	/*************************************************************/
	fwd_sz -= sizeof(*fwd);
	if (m && fwd_sz) {
		/* New method. */
		unsigned int num_str;
		struct sctp_strseq *stseq, strseqbuf;

		offset += sizeof(*fwd);

		num_str = fwd_sz / sizeof(struct sctp_strseq);
		for (i = 0; i < num_str; i++) {
			uint16_t st;

			stseq = (struct sctp_strseq *)sctp_m_getptr(m, offset,
			    sizeof(struct sctp_strseq),
			    (uint8_t *) & strseqbuf);
			offset += sizeof(struct sctp_strseq);
			if (stseq == NULL) {
				break;
			}
			/* Convert */
			st = ntohs(stseq->stream);
			stseq->stream = st;
			st = ntohs(stseq->sequence);
			stseq->sequence = st;
			/* now process */
			if (stseq->stream >= asoc->streamincnt) {
				/* screwed up streams, stop!  */
				break;
			}
			strm = &asoc->strmin[stseq->stream];
			if (compare_with_wrap(stseq->sequence,
			    strm->last_sequence_delivered, MAX_SEQ)) {
				/* Update the sequence number */
				strm->last_sequence_delivered =
				    stseq->sequence;
			}
			/* now kick the stream the new way */
			/* sa_ignore NO_NULL_CHK */
			sctp_kick_prsctp_reorder_queue(stcb, strm);
		}
	}
	if (TAILQ_FIRST(&asoc->reasmqueue)) {
		/* now lets kick out and check for more fragmented delivery */
		/* sa_ignore NO_NULL_CHK */
		sctp_deliver_reasm_check(stcb, &stcb->asoc);
	}
}
