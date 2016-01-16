/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2008-2012, by Randall Stewart. All rights reserved.
 * Copyright (c) 2008-2012, by Michael Tuexen. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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
	asoc->my_rwnd = sctp_calc_rwnd(stcb, asoc);
}

/* Calculate what the rwnd would be */
uint32_t
sctp_calc_rwnd(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	uint32_t calc = 0;

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
		calc = max(SCTP_SB_LIMIT_RCV(stcb->sctp_socket), SCTP_MINIMAL_RWND);
		return (calc);
	}
	/* get actual space */
	calc = (uint32_t) sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv);

	/*
	 * take out what has NOT been put on socket queue and we yet hold
	 * for putting up.
	 */
	calc = sctp_sbspace_sub(calc, (uint32_t) (asoc->size_on_reasm_queue +
	    asoc->cnt_on_reasm_queue * MSIZE));
	calc = sctp_sbspace_sub(calc, (uint32_t) (asoc->size_on_all_streams +
	    asoc->cnt_on_all_streams * MSIZE));

	if (calc == 0) {
		/* out of space */
		return (calc);
	}
	/* what is the overhead of all these rwnd's */
	calc = sctp_sbspace_sub(calc, stcb->asoc.my_rwnd_control_len);
	/*
	 * If the window gets too small due to ctrl-stuff, reduce it to 1,
	 * even it is 0. SWS engaged
	 */
	if (calc < stcb->asoc.my_rwnd_control_len) {
		calc = 1;
	}
	return (calc);
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
	read_queue_e->sinfo_context = context;
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
sctp_build_ctl_nchunk(struct sctp_inpcb *inp, struct sctp_sndrcvinfo *sinfo)
{
	struct sctp_extrcvinfo *seinfo;
	struct sctp_sndrcvinfo *outinfo;
	struct sctp_rcvinfo *rcvinfo;
	struct sctp_nxtinfo *nxtinfo;
	struct cmsghdr *cmh;
	struct mbuf *ret;
	int len;
	int use_extended;
	int provide_nxt;

	if (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT) &&
	    sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVRCVINFO) &&
	    sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVNXTINFO)) {
		/* user does not want any ancillary data */
		return (NULL);
	}
	len = 0;
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVRCVINFO)) {
		len += CMSG_SPACE(sizeof(struct sctp_rcvinfo));
	}
	seinfo = (struct sctp_extrcvinfo *)sinfo;
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVNXTINFO) &&
	    (seinfo->sreinfo_next_flags & SCTP_NEXT_MSG_AVAIL)) {
		provide_nxt = 1;
		len += CMSG_SPACE(sizeof(struct sctp_rcvinfo));
	} else {
		provide_nxt = 0;
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT)) {
		if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXT_RCVINFO)) {
			use_extended = 1;
			len += CMSG_SPACE(sizeof(struct sctp_extrcvinfo));
		} else {
			use_extended = 0;
			len += CMSG_SPACE(sizeof(struct sctp_sndrcvinfo));
		}
	} else {
		use_extended = 0;
	}

	ret = sctp_get_mbuf_for_msg(len, 0, M_NOWAIT, 1, MT_DATA);
	if (ret == NULL) {
		/* No space */
		return (ret);
	}
	SCTP_BUF_LEN(ret) = 0;

	/* We need a CMSG header followed by the struct */
	cmh = mtod(ret, struct cmsghdr *);
	/*
	 * Make sure that there is no un-initialized padding between the
	 * cmsg header and cmsg data and after the cmsg data.
	 */
	memset(cmh, 0, len);
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVRCVINFO)) {
		cmh->cmsg_level = IPPROTO_SCTP;
		cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_rcvinfo));
		cmh->cmsg_type = SCTP_RCVINFO;
		rcvinfo = (struct sctp_rcvinfo *)CMSG_DATA(cmh);
		rcvinfo->rcv_sid = sinfo->sinfo_stream;
		rcvinfo->rcv_ssn = sinfo->sinfo_ssn;
		rcvinfo->rcv_flags = sinfo->sinfo_flags;
		rcvinfo->rcv_ppid = sinfo->sinfo_ppid;
		rcvinfo->rcv_tsn = sinfo->sinfo_tsn;
		rcvinfo->rcv_cumtsn = sinfo->sinfo_cumtsn;
		rcvinfo->rcv_context = sinfo->sinfo_context;
		rcvinfo->rcv_assoc_id = sinfo->sinfo_assoc_id;
		cmh = (struct cmsghdr *)((caddr_t)cmh + CMSG_SPACE(sizeof(struct sctp_rcvinfo)));
		SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_rcvinfo));
	}
	if (provide_nxt) {
		cmh->cmsg_level = IPPROTO_SCTP;
		cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_nxtinfo));
		cmh->cmsg_type = SCTP_NXTINFO;
		nxtinfo = (struct sctp_nxtinfo *)CMSG_DATA(cmh);
		nxtinfo->nxt_sid = seinfo->sreinfo_next_stream;
		nxtinfo->nxt_flags = 0;
		if (seinfo->sreinfo_next_flags & SCTP_NEXT_MSG_IS_UNORDERED) {
			nxtinfo->nxt_flags |= SCTP_UNORDERED;
		}
		if (seinfo->sreinfo_next_flags & SCTP_NEXT_MSG_IS_NOTIFICATION) {
			nxtinfo->nxt_flags |= SCTP_NOTIFICATION;
		}
		if (seinfo->sreinfo_next_flags & SCTP_NEXT_MSG_ISCOMPLETE) {
			nxtinfo->nxt_flags |= SCTP_COMPLETE;
		}
		nxtinfo->nxt_ppid = seinfo->sreinfo_next_ppid;
		nxtinfo->nxt_length = seinfo->sreinfo_next_length;
		nxtinfo->nxt_assoc_id = seinfo->sreinfo_next_aid;
		cmh = (struct cmsghdr *)((caddr_t)cmh + CMSG_SPACE(sizeof(struct sctp_nxtinfo)));
		SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_nxtinfo));
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT)) {
		cmh->cmsg_level = IPPROTO_SCTP;
		outinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmh);
		if (use_extended) {
			cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_extrcvinfo));
			cmh->cmsg_type = SCTP_EXTRCV;
			memcpy(outinfo, sinfo, sizeof(struct sctp_extrcvinfo));
			SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_extrcvinfo));
		} else {
			cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
			cmh->cmsg_type = SCTP_SNDRCV;
			*outinfo = *sinfo;
			SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_sndrcvinfo));
		}
	}
	return (ret);
}


static void
sctp_mark_non_revokable(struct sctp_association *asoc, uint32_t tsn)
{
	uint32_t gap, i, cumackp1;
	int fnd = 0;

	if (SCTP_BASE_SYSCTL(sctp_do_drain) == 0) {
		return;
	}
	cumackp1 = asoc->cumulative_tsn + 1;
	if (SCTP_TSN_GT(cumackp1, tsn)) {
		/*
		 * this tsn is behind the cum ack and thus we don't need to
		 * worry about it being moved from one to the other.
		 */
		return;
	}
	SCTP_CALC_TSN_TO_GAP(gap, tsn, asoc->mapping_array_base_tsn);
	if (!SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap)) {
		SCTP_PRINTF("gap:%x tsn:%x\n", gap, tsn);
		sctp_print_mapping_array(asoc);
#ifdef INVARIANTS
		panic("Things are really messed up now!!");
#endif
	}
	SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
	SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
	if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
		asoc->highest_tsn_inside_nr_map = tsn;
	}
	if (tsn == asoc->highest_tsn_inside_map) {
		/* We must back down to see what the new highest is */
		for (i = tsn - 1; SCTP_TSN_GE(i, asoc->mapping_array_base_tsn); i--) {
			SCTP_CALC_TSN_TO_GAP(gap, i, asoc->mapping_array_base_tsn);
			if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap)) {
				asoc->highest_tsn_inside_map = i;
				fnd = 1;
				break;
			}
		}
		if (!fnd) {
			asoc->highest_tsn_inside_map = asoc->mapping_array_base_tsn - 1;
		}
	}
}


/*
 * We are delivering currently from the reassembly queue. We must continue to
 * deliver until we either: 1) run out of space. 2) run out of sequential
 * TSN's 3) hit the SCTP_DATA_LAST_FRAG flag.
 */
static void
sctp_service_reassembly(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk, *nchk;
	uint16_t nxt_todel;
	uint16_t stream_no;
	int end = 0;
	int cntDel;
	struct sctp_queued_to_read *control, *ctl, *nctl;

	if (stcb == NULL)
		return;

	cntDel = stream_no = 0;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) ||
	    (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) ||
	    (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET)) {
		/* socket above is long gone or going.. */
abandon:
		asoc->fragmented_delivery_inprogress = 0;
		TAILQ_FOREACH_SAFE(chk, &asoc->reasmqueue, sctp_next, nchk) {
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
			sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
			/* sa_ignore FREED_MEMORY */
		}
		return;
	}
	SCTP_TCB_LOCK_ASSERT(stcb);
	TAILQ_FOREACH_SAFE(chk, &asoc->reasmqueue, sctp_next, nchk) {
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
			sctp_mark_non_revokable(asoc, chk->rec.data.TSN_seq);
			sctp_add_to_readq(stcb->sctp_ep,
			    stcb, control, &stcb->sctp_socket->so_rcv, end,
			    SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);
			cntDel++;
		} else {
			if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG)
				end = 1;
			else
				end = 0;
			sctp_mark_non_revokable(asoc, chk->rec.data.TSN_seq);
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
#ifdef INVARIANTS
					if ((stcb->asoc.control_pdapi == NULL) || (stcb->asoc.control_pdapi->tail_mbuf == NULL)) {
						panic("This should not happen control_pdapi NULL?");
					}
					/* if we did not panic, it was a EOM */
					panic("Bad chunking ??");
#else
					if ((stcb->asoc.control_pdapi == NULL) || (stcb->asoc.control_pdapi->tail_mbuf == NULL)) {
						SCTP_PRINTF("This should not happen control_pdapi NULL?\n");
					}
					SCTP_PRINTF("Bad chunking ??\n");
					SCTP_PRINTF("Dumping re-assembly queue this will probably hose the association\n");

#endif
					goto abandon;
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
		sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);

		if (asoc->fragmented_delivery_inprogress == 0) {
			/*
			 * Now lets see if we can deliver the next one on
			 * the stream
			 */
			struct sctp_stream_in *strm;

			strm = &asoc->strmin[stream_no];
			nxt_todel = strm->last_sequence_delivered + 1;
			TAILQ_FOREACH_SAFE(ctl, &strm->inqueue, next, nctl) {
				/* Deliver more if we can. */
				if (nxt_todel == ctl->sinfo_ssn) {
					TAILQ_REMOVE(&strm->inqueue, ctl, next);
					asoc->size_on_all_streams -= ctl->length;
					sctp_ucount_decr(asoc->cnt_on_all_streams);
					strm->last_sequence_delivered++;
					sctp_mark_non_revokable(asoc, ctl->sinfo_tsn);
					sctp_add_to_readq(stcb->sctp_ep, stcb,
					    ctl,
					    &stcb->sctp_socket->so_rcv, 1,
					    SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);
				} else {
					break;
				}
				nxt_todel = strm->last_sequence_delivered + 1;
			}
			break;
		}
	}
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
	struct mbuf *op_err;
	char msg[SCTP_DIAG_INFO_LEN];

	queue_needed = 1;
	asoc->size_on_all_streams += control->length;
	sctp_ucount_incr(asoc->cnt_on_all_streams);
	strm = &asoc->strmin[control->sinfo_stream];
	nxt_todel = strm->last_sequence_delivered + 1;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
		sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_INTO_STRD);
	}
	SCTPDBG(SCTP_DEBUG_INDATA1,
	    "queue to stream called for sid:%u ssn:%u tsn:%u lastdel:%u nxt:%u\n",
	    (uint32_t) control->sinfo_stream, (uint32_t) control->sinfo_ssn,
	    (uint32_t) control->sinfo_tsn,
	    (uint32_t) strm->last_sequence_delivered, (uint32_t) nxt_todel);
	if (SCTP_SSN_GE(strm->last_sequence_delivered, control->sinfo_ssn)) {
		/* The incoming sseq is behind where we last delivered? */
		SCTPDBG(SCTP_DEBUG_INDATA1, "Duplicate S-SEQ:%d delivered:%d from peer, Abort association\n",
		    control->sinfo_ssn, strm->last_sequence_delivered);
protocol_error:
		/*
		 * throw it in the stream so it gets cleaned up in
		 * association destruction
		 */
		TAILQ_INSERT_HEAD(&strm->inqueue, control, next);
		snprintf(msg, sizeof(msg), "Delivered SSN=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
		    strm->last_sequence_delivered, control->sinfo_tsn,
		    control->sinfo_stream, control->sinfo_ssn);
		op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_1;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return;

	}
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
	struct socket *so;

	so = SCTP_INP_SO(stcb->sctp_ep);
	atomic_add_int(&stcb->asoc.refcnt, 1);
	SCTP_TCB_UNLOCK(stcb);
	SCTP_SOCKET_LOCK(so, 1);
	SCTP_TCB_LOCK(stcb);
	atomic_subtract_int(&stcb->asoc.refcnt, 1);
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
		SCTP_SOCKET_UNLOCK(so, 1);
		return;
	}
#endif
	if (nxt_todel == control->sinfo_ssn) {
		/* can be delivered right away? */
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
			sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_IMMED_DEL);
		}
		/* EY it wont be queued if it could be delivered directly */
		queue_needed = 0;
		asoc->size_on_all_streams -= control->length;
		sctp_ucount_decr(asoc->cnt_on_all_streams);
		strm->last_sequence_delivered++;

		sctp_mark_non_revokable(asoc, control->sinfo_tsn);
		sctp_add_to_readq(stcb->sctp_ep, stcb,
		    control,
		    &stcb->sctp_socket->so_rcv, 1,
		    SCTP_READ_LOCK_NOT_HELD, SCTP_SO_LOCKED);
		TAILQ_FOREACH_SAFE(control, &strm->inqueue, next, at) {
			/* all delivered */
			nxt_todel = strm->last_sequence_delivered + 1;
			if (nxt_todel == control->sinfo_ssn) {
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
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
					sctp_log_strm_del(control, NULL,
					    SCTP_STR_LOG_FROM_IMMED_DEL);
				}
				sctp_mark_non_revokable(asoc, control->sinfo_tsn);
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				    control,
				    &stcb->sctp_socket->so_rcv, 1,
				    SCTP_READ_LOCK_NOT_HELD,
				    SCTP_SO_LOCKED);
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
		if (SCTP_TSN_GE(asoc->cumulative_tsn, control->sinfo_tsn)) {
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
			SCTP_SOCKET_UNLOCK(so, 1);
#endif
			goto protocol_error;
		}
		if (TAILQ_EMPTY(&strm->inqueue)) {
			/* Empty queue */
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
				sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_INSERT_HD);
			}
			TAILQ_INSERT_HEAD(&strm->inqueue, control, next);
		} else {
			TAILQ_FOREACH(at, &strm->inqueue, next) {
				if (SCTP_SSN_GT(at->sinfo_ssn, control->sinfo_ssn)) {
					/*
					 * one in queue is bigger than the
					 * new one, insert before this one
					 */
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
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
					if (control->whoFrom) {
						sctp_free_remote_addr(control->whoFrom);
						control->whoFrom = NULL;
					}
					sctp_free_a_readq(stcb, control);
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
					SCTP_SOCKET_UNLOCK(so, 1);
#endif
					return;
				} else {
					if (TAILQ_NEXT(at, next) == NULL) {
						/*
						 * We are at the end, insert
						 * it after this one
						 */
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
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
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
	SCTP_SOCKET_UNLOCK(so, 1);
#endif
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
	TAILQ_FOREACH(chk, &asoc->reasmqueue, sctp_next) {
		if (tsn != chk->rec.data.TSN_seq) {
			return (0);
		}
		*t_size += chk->send_size;
		if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {
			return (1);
		}
		tsn++;
	}
	return (0);
}

static void
sctp_deliver_reasm_check(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	uint16_t nxt_todel;
	uint32_t tsize, pd_point;

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
			if (stcb->sctp_socket) {
				pd_point = min(SCTP_SB_LIMIT_RCV(stcb->sctp_socket) >> SCTP_PARTIAL_DELIVERY_SHIFT,
				    stcb->sctp_ep->partial_delivery_point);
			} else {
				pd_point = stcb->sctp_ep->partial_delivery_point;
			}
			if (sctp_is_all_msg_on_reasm(asoc, &tsize) || (tsize >= pd_point)) {
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
	struct mbuf *op_err;
	char msg[SCTP_DIAG_INFO_LEN];
	uint32_t cum_ackp1, prev_tsn, post_tsn;
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
				snprintf(msg, sizeof(msg),
				    "Expected B-bit for TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
				    chk->rec.data.TSN_seq,
				    chk->rec.data.stream_number,
				    chk->rec.data.stream_seq);
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_2;
				sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
				*abort_flag = 1;
			} else if (asoc->fragmented_delivery_inprogress &&
			    (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) == SCTP_DATA_FIRST_FRAG) {
				/*
				 * We are doing a partial delivery and the
				 * NEXT chunk MUST be either the LAST or
				 * MIDDLE fragment NOT a FIRST
				 */
				SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, it IS a first and fragmented delivery in progress\n");
				snprintf(msg, sizeof(msg),
				    "Didn't expect B-bit for TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
				    chk->rec.data.TSN_seq,
				    chk->rec.data.stream_number,
				    chk->rec.data.stream_seq);
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_3;
				sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
					snprintf(msg, sizeof(msg),
					    "Expected SID=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    asoc->str_of_pdapi,
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_4;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
				} else if ((asoc->fragment_flags & SCTP_DATA_UNORDERED) !=
					    SCTP_DATA_UNORDERED &&
				    chk->rec.data.stream_seq != asoc->ssn_of_pdapi) {
					/* Got to be the right STR Seq */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Gak, Evil plot, it IS not same stream seq %d vs %d\n",
					    chk->rec.data.stream_seq,
					    asoc->ssn_of_pdapi);
					snprintf(msg, sizeof(msg),
					    "Expected SSN=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    asoc->ssn_of_pdapi,
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_5;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
				}
			}
		}
		return;
	}
	/* Find its place */
	TAILQ_FOREACH(at, &asoc->reasmqueue, sctp_next) {
		if (SCTP_TSN_GT(at->rec.data.TSN_seq, chk->rec.data.TSN_seq)) {
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
			sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
			return;
		} else {
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
					snprintf(msg, sizeof(msg),
					    "Can't handle B-bit, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_6;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					return;
				}
				if (chk->rec.data.stream_number !=
				    prev->rec.data.stream_number) {
					/*
					 * Huh, need the correct STR here,
					 * they must be the same.
					 */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Prev check - Gak, Evil plot, sid:%d not the same as at:%d\n",
					    chk->rec.data.stream_number,
					    prev->rec.data.stream_number);
					snprintf(msg, sizeof(msg),
					    "Expect SID=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    prev->rec.data.stream_number,
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_7;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					return;
				}
				if ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) !=
				    (prev->rec.data.rcv_flags & SCTP_DATA_UNORDERED)) {
					/*
					 * Huh, need the same ordering here,
					 * they must be the same.
					 */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Prev check - Gak, Evil plot, U-bit not constant\n");
					snprintf(msg, sizeof(msg),
					    "Expect U-bit=%d for TSN=%8.8x, got U-bit=%d",
					    (prev->rec.data.rcv_flags & SCTP_DATA_UNORDERED) ? 1 : 0,
					    chk->rec.data.TSN_seq,
					    (chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) ? 1 : 0);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_8;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
					snprintf(msg, sizeof(msg),
					    "Expect SSN=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    prev->rec.data.stream_seq,
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_9;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					return;
				}
			} else if ((prev->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) ==
			    SCTP_DATA_LAST_FRAG) {
				/* Insert chk MUST be a FIRST */
				if ((chk->rec.data.rcv_flags & SCTP_DATA_FRAG_MASK) !=
				    SCTP_DATA_FIRST_FRAG) {
					SCTPDBG(SCTP_DEBUG_INDATA1, "Prev check - Gak, evil plot, its not FIRST and it must be!\n");
					snprintf(msg, sizeof(msg),
					    "Expect B-bit, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_10;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
					snprintf(msg, sizeof(msg),
					    "Expect only E-bit, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_11;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
					snprintf(msg, sizeof(msg),
					    "Didn't expect E-bit, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_12;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
					snprintf(msg, sizeof(msg),
					    "Required SID %4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    next->rec.data.stream_number,
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_13;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					return;
				}
				if ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) !=
				    (next->rec.data.rcv_flags & SCTP_DATA_UNORDERED)) {
					/*
					 * Huh, need the same ordering here,
					 * they must be the same.
					 */
					SCTPDBG(SCTP_DEBUG_INDATA1, "Next check - Gak, Evil plot, U-bit not constant\n");
					snprintf(msg, sizeof(msg),
					    "Expect U-bit=%d for TSN=%8.8x, got U-bit=%d",
					    (next->rec.data.rcv_flags & SCTP_DATA_UNORDERED) ? 1 : 0,
					    chk->rec.data.TSN_seq,
					    (chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) ? 1 : 0);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_14;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
					snprintf(msg, sizeof(msg),
					    "Required SSN %4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    next->rec.data.stream_seq,
					    chk->rec.data.TSN_seq,
					    chk->rec.data.stream_number,
					    chk->rec.data.stream_seq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_15;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
		if (SCTP_TSN_GT(TSN_seq, at->rec.data.TSN_seq)) {
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
	int the_len;
	int need_reasm_check = 0;
	uint16_t strmno, strmseq;
	struct mbuf *op_err;
	char msg[SCTP_DIAG_INFO_LEN];
	struct sctp_queued_to_read *control;
	int ordered;
	uint32_t protocol_id;
	uint8_t chunk_flags;
	struct sctp_stream_reset_list *liste;

	chk = NULL;
	tsn = ntohl(ch->dp.tsn);
	chunk_flags = ch->ch.chunk_flags;
	if ((chunk_flags & SCTP_DATA_SACK_IMMEDIATELY) == SCTP_DATA_SACK_IMMEDIATELY) {
		asoc->send_sack = 1;
	}
	protocol_id = ch->dp.protocol_id;
	ordered = ((chunk_flags & SCTP_DATA_UNORDERED) == 0);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
		sctp_log_map(tsn, asoc->cumulative_tsn, asoc->highest_tsn_inside_map, SCTP_MAP_TSN_ENTERS);
	}
	if (stcb == NULL) {
		return (0);
	}
	SCTP_LTRACE_CHK(stcb->sctp_ep, stcb, ch->ch.chunk_type, tsn);
	if (SCTP_TSN_GE(asoc->cumulative_tsn, tsn)) {
		/* It is a duplicate */
		SCTP_STAT_INCR(sctps_recvdupdata);
		if (asoc->numduptsns < SCTP_MAX_DUP_TSNS) {
			/* Record a dup for the next outbound sack */
			asoc->dup_tsns[asoc->numduptsns] = tsn;
			asoc->numduptsns++;
		}
		asoc->send_sack = 1;
		return (0);
	}
	/* Calculate the number of TSN's between the base and this TSN */
	SCTP_CALC_TSN_TO_GAP(gap, tsn, asoc->mapping_array_base_tsn);
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
	if (SCTP_TSN_GT(tsn, *high_tsn)) {
		*high_tsn = tsn;
	}
	/* See if we have received this one already */
	if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap) ||
	    SCTP_IS_TSN_PRESENT(asoc->nr_mapping_array, gap)) {
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
	    (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET))) {
		/*
		 * wait a minute, this guy is gone, there is no longer a
		 * receiver. Send peer an ABORT!
		 */
		op_err = sctp_generate_cause(SCTP_CAUSE_OUT_OF_RESC, "");
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
	    asoc->cnt_msg_on_sb) >= SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue)) ||
	    (((int)asoc->my_rwnd) <= 0)) {
		/*
		 * When we have NO room in the rwnd we check to make sure
		 * the reader is doing its job...
		 */
		if (stcb->sctp_socket->so_rcv.sb_cc) {
			/* some to read, wake-up */
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
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
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
			SCTP_SOCKET_UNLOCK(so, 1);
#endif
		}
		/* now is it in the mapping array of what we have accepted? */
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_map) &&
		    SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			/* Nope not in the valid range dump it */
			sctp_set_rwnd(stcb, asoc);
			if ((asoc->cnt_on_all_streams +
			    asoc->cnt_on_reasm_queue +
			    asoc->cnt_msg_on_sb) >= SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue)) {
				SCTP_STAT_INCR(sctps_datadropchklmt);
			} else {
				SCTP_STAT_INCR(sctps_datadroprwnd);
			}
			*break_flag = 1;
			return (0);
		}
	}
	strmno = ntohs(ch->dp.stream_id);
	if (strmno >= asoc->streamincnt) {
		struct sctp_paramhdr *phdr;
		struct mbuf *mb;

		mb = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) * 2),
		    0, M_NOWAIT, 1, MT_DATA);
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
		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
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
	    SCTP_SSN_GE(asoc->strmin[strmno].last_sequence_delivered, strmseq)) {
		/* The incoming sseq is behind where we last delivered? */
		SCTPDBG(SCTP_DEBUG_INDATA1, "EVIL/Broken-Dup S-SEQ:%d delivered:%d from peer, Abort!\n",
		    strmseq, asoc->strmin[strmno].last_sequence_delivered);

		snprintf(msg, sizeof(msg), "Delivered SSN=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
		    asoc->strmin[strmno].last_sequence_delivered,
		    tsn, strmno, strmseq);
		op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_16;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
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
		    the_len, M_NOWAIT);
#ifdef SCTP_MBUF_LOGGING
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MBUF_LOGGING_ENABLE) {
			sctp_log_mbc(dmbuf, SCTP_MBUF_ICOPY);
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
			for (lat = dmbuf; lat; lat = SCTP_BUF_NEXT(lat)) {
				l_len += SCTP_BUF_LEN(lat);
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
	    ((uint16_t) (asoc->strmin[strmno].last_sequence_delivered + 1) == strmseq &&
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
		    strmno, strmseq,
		    chunk_flags,
		    dmbuf);
		if (control == NULL) {
			goto failed_express_del;
		}
		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
		}
		sctp_add_to_readq(stcb->sctp_ep, stcb,
		    control, &stcb->sctp_socket->so_rcv,
		    1, SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);

		if ((chunk_flags & SCTP_DATA_UNORDERED) == 0) {
			/* for ordered, bump what we delivered */
			asoc->strmin[strmno].last_sequence_delivered++;
		}
		SCTP_STAT_INCR(sctps_recvexpress);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
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

			if (chunk_flags & SCTP_DATA_LAST_FRAG) {
				end = 1;
			}
			if (sctp_append_to_readq(stcb->sctp_ep, stcb, control, dmbuf, end,
			    tsn,
			    &stcb->sctp_socket->so_rcv)) {
				SCTP_PRINTF("Append fails end:%d\n", end);
				goto failed_pdapi_express_del;
			}
			SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
			if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
				asoc->highest_tsn_inside_nr_map = tsn;
			}
			SCTP_STAT_INCR(sctps_recvexpressm);
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
	if (SCTP_BASE_SYSCTL(sctp_do_drain) == 0) {
		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
		}
	} else {
		SCTP_SET_TSN_PRESENT(asoc->mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_map)) {
			asoc->highest_tsn_inside_map = tsn;
		}
	}
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
				snprintf(msg, sizeof(msg), "Reas. queue emtpy, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
				    tsn, strmno, strmseq);
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_17;
				sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
				*abort_flag = 1;
				if (last_chunk) {
					*m = NULL;
				}
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
					snprintf(msg, sizeof(msg), "PD ongoing, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    tsn, strmno, strmseq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_18;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					if (last_chunk) {
						*m = NULL;
					}
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
					snprintf(msg, sizeof(msg), "No PD ongoing, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
					    tsn, strmno, strmseq);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_19;
					sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
					*abort_flag = 1;
					if (last_chunk) {
						*m = NULL;
					}
					return (0);
				}
			}
		}
		/* ok, if we reach here we have passed the sanity checks */
		if (chunk_flags & SCTP_DATA_UNORDERED) {
			/* queue directly into socket buffer */
			sctp_mark_non_revokable(asoc, control->sinfo_tsn);
			sctp_add_to_readq(stcb->sctp_ep, stcb,
			    control,
			    &stcb->sctp_socket->so_rcv, 1, SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);
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
			    SCTP_TSN_GT(tsn, liste->tsn)) {
				/*
				 * yep its past where we need to reset... go
				 * ahead and queue it.
				 */
				if (TAILQ_EMPTY(&asoc->pending_reply_queue)) {
					/* first one on */
					TAILQ_INSERT_TAIL(&asoc->pending_reply_queue, control, next);
				} else {
					struct sctp_queued_to_read *ctlOn,
					                   *nctlOn;
					unsigned char inserted = 0;

					TAILQ_FOREACH_SAFE(ctlOn, &asoc->pending_reply_queue, next, nctlOn) {
						if (SCTP_TSN_GT(control->sinfo_tsn, ctlOn->sinfo_tsn)) {
							continue;
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
					if (last_chunk) {
						*m = NULL;
					}
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
			if (last_chunk) {
				*m = NULL;
			}
			return (0);
		}
	}
finish_express_del:
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
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
		sctp_log_strm_del_alt(stcb, tsn, strmseq, strmno, SCTP_STR_LOG_FROM_MARK_TSN);
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
		sctp_log_map(asoc->mapping_array_base_tsn, asoc->cumulative_tsn,
		    asoc->highest_tsn_inside_map, SCTP_MAP_PREPARE_SLIDE);
	}
	/* check the special flag for stream resets */
	if (((liste = TAILQ_FIRST(&asoc->resetHead)) != NULL) &&
	    SCTP_TSN_GE(asoc->cumulative_tsn, liste->tsn)) {
		/*
		 * we have finished working through the backlogged TSN's now
		 * time to reset streams. 1: call reset function. 2: free
		 * pending_reply space 3: distribute any chunks in
		 * pending_reply_queue.
		 */
		struct sctp_queued_to_read *ctl, *nctl;

		sctp_reset_in_stream(stcb, liste->number_entries, liste->list_of_streams);
		TAILQ_REMOVE(&asoc->resetHead, liste, next_resp);
		sctp_send_deferred_reset_response(stcb, liste, SCTP_STREAM_RESET_RESULT_PERFORMED);
		SCTP_FREE(liste, SCTP_M_STRESET);
		/* sa_ignore FREED_MEMORY */
		liste = TAILQ_FIRST(&asoc->resetHead);
		if (TAILQ_EMPTY(&asoc->resetHead)) {
			/* All can be removed */
			TAILQ_FOREACH_SAFE(ctl, &asoc->pending_reply_queue, next, nctl) {
				TAILQ_REMOVE(&asoc->pending_reply_queue, ctl, next);
				sctp_queue_data_to_stream(stcb, asoc, ctl, abort_flag);
				if (*abort_flag) {
					return (0);
				}
			}
		} else {
			TAILQ_FOREACH_SAFE(ctl, &asoc->pending_reply_queue, next, nctl) {
				if (SCTP_TSN_GT(ctl->sinfo_tsn, liste->tsn)) {
					break;
				}
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
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 6,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 7,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 6,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3,
	0, 1, 0, 2, 0, 1, 0, 8
};


void
sctp_slide_mapping_arrays(struct sctp_tcb *stcb)
{
	/*
	 * Now we also need to check the mapping array in a couple of ways.
	 * 1) Did we move the cum-ack point?
	 * 
	 * When you first glance at this you might think that all entries that
	 * make up the postion of the cum-ack would be in the nr-mapping
	 * array only.. i.e. things up to the cum-ack are always
	 * deliverable. Thats true with one exception, when its a fragmented
	 * message we may not deliver the data until some threshold (or all
	 * of it) is in place. So we must OR the nr_mapping_array and
	 * mapping_array to get a true picture of the cum-ack.
	 */
	struct sctp_association *asoc;
	int at;
	uint8_t val;
	int slide_from, slide_end, lgap, distance;
	uint32_t old_cumack, old_base, old_highest, highest_tsn;

	asoc = &stcb->asoc;

	old_cumack = asoc->cumulative_tsn;
	old_base = asoc->mapping_array_base_tsn;
	old_highest = asoc->highest_tsn_inside_map;
	/*
	 * We could probably improve this a small bit by calculating the
	 * offset of the current cum-ack as the starting point.
	 */
	at = 0;
	for (slide_from = 0; slide_from < stcb->asoc.mapping_array_size; slide_from++) {
		val = asoc->nr_mapping_array[slide_from] | asoc->mapping_array[slide_from];
		if (val == 0xff) {
			at += 8;
		} else {
			/* there is a 0 bit */
			at += sctp_map_lookup_tab[val];
			break;
		}
	}
	asoc->cumulative_tsn = asoc->mapping_array_base_tsn + (at - 1);

	if (SCTP_TSN_GT(asoc->cumulative_tsn, asoc->highest_tsn_inside_map) &&
	    SCTP_TSN_GT(asoc->cumulative_tsn, asoc->highest_tsn_inside_nr_map)) {
#ifdef INVARIANTS
		panic("huh, cumack 0x%x greater than high-tsn 0x%x in map",
		    asoc->cumulative_tsn, asoc->highest_tsn_inside_map);
#else
		SCTP_PRINTF("huh, cumack 0x%x greater than high-tsn 0x%x in map - should panic?\n",
		    asoc->cumulative_tsn, asoc->highest_tsn_inside_map);
		sctp_print_mapping_array(asoc);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 6, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
		asoc->highest_tsn_inside_map = asoc->cumulative_tsn;
		asoc->highest_tsn_inside_nr_map = asoc->cumulative_tsn;
#endif
	}
	if (SCTP_TSN_GT(asoc->highest_tsn_inside_nr_map, asoc->highest_tsn_inside_map)) {
		highest_tsn = asoc->highest_tsn_inside_nr_map;
	} else {
		highest_tsn = asoc->highest_tsn_inside_map;
	}
	if ((asoc->cumulative_tsn == highest_tsn) && (at >= 8)) {
		/* The complete array was completed by a single FR */
		/* highest becomes the cum-ack */
		int clr;

#ifdef INVARIANTS
		unsigned int i;

#endif

		/* clear the array */
		clr = ((at + 7) >> 3);
		if (clr > asoc->mapping_array_size) {
			clr = asoc->mapping_array_size;
		}
		memset(asoc->mapping_array, 0, clr);
		memset(asoc->nr_mapping_array, 0, clr);
#ifdef INVARIANTS
		for (i = 0; i < asoc->mapping_array_size; i++) {
			if ((asoc->mapping_array[i]) || (asoc->nr_mapping_array[i])) {
				SCTP_PRINTF("Error Mapping array's not clean at clear\n");
				sctp_print_mapping_array(asoc);
			}
		}
#endif
		asoc->mapping_array_base_tsn = asoc->cumulative_tsn + 1;
		asoc->highest_tsn_inside_nr_map = asoc->highest_tsn_inside_map = asoc->cumulative_tsn;
	} else if (at >= 8) {
		/* we can slide the mapping array down */
		/* slide_from holds where we hit the first NON 0xff byte */

		/*
		 * now calculate the ceiling of the move using our highest
		 * TSN value
		 */
		SCTP_CALC_TSN_TO_GAP(lgap, highest_tsn, asoc->mapping_array_base_tsn);
		slide_end = (lgap >> 3);
		if (slide_end < slide_from) {
			sctp_print_mapping_array(asoc);
#ifdef INVARIANTS
			panic("impossible slide");
#else
			SCTP_PRINTF("impossible slide lgap:%x slide_end:%x slide_from:%x? at:%d\n",
			    lgap, slide_end, slide_from, at);
			return;
#endif
		}
		if (slide_end > asoc->mapping_array_size) {
#ifdef INVARIANTS
			panic("would overrun buffer");
#else
			SCTP_PRINTF("Gak, would have overrun map end:%d slide_end:%d\n",
			    asoc->mapping_array_size, slide_end);
			slide_end = asoc->mapping_array_size;
#endif
		}
		distance = (slide_end - slide_from) + 1;
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
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

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map((uint32_t) distance, (uint32_t) slide_from,
				    (uint32_t) asoc->mapping_array_size,
				    SCTP_MAP_SLIDE_NONE);
			}
		} else {
			int ii;

			for (ii = 0; ii < distance; ii++) {
				asoc->mapping_array[ii] = asoc->mapping_array[slide_from + ii];
				asoc->nr_mapping_array[ii] = asoc->nr_mapping_array[slide_from + ii];

			}
			for (ii = distance; ii < asoc->mapping_array_size; ii++) {
				asoc->mapping_array[ii] = 0;
				asoc->nr_mapping_array[ii] = 0;
			}
			if (asoc->highest_tsn_inside_map + 1 == asoc->mapping_array_base_tsn) {
				asoc->highest_tsn_inside_map += (slide_from << 3);
			}
			if (asoc->highest_tsn_inside_nr_map + 1 == asoc->mapping_array_base_tsn) {
				asoc->highest_tsn_inside_nr_map += (slide_from << 3);
			}
			asoc->mapping_array_base_tsn += (slide_from << 3);
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map(asoc->mapping_array_base_tsn,
				    asoc->cumulative_tsn, asoc->highest_tsn_inside_map,
				    SCTP_MAP_SLIDE_RESULT);
			}
		}
	}
}

void
sctp_sack_check(struct sctp_tcb *stcb, int was_a_gap)
{
	struct sctp_association *asoc;
	uint32_t highest_tsn;

	asoc = &stcb->asoc;
	if (SCTP_TSN_GT(asoc->highest_tsn_inside_nr_map, asoc->highest_tsn_inside_map)) {
		highest_tsn = asoc->highest_tsn_inside_nr_map;
	} else {
		highest_tsn = asoc->highest_tsn_inside_map;
	}

	/*
	 * Now we need to see if we need to queue a sack or just start the
	 * timer (if allowed).
	 */
	if (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_SENT) {
		/*
		 * Ok special case, in SHUTDOWN-SENT case. here we maker
		 * sure SACK timer is off and instead send a SHUTDOWN and a
		 * SACK
		 */
		if (SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
			sctp_timer_stop(SCTP_TIMER_TYPE_RECV,
			    stcb->sctp_ep, stcb, NULL,
			    SCTP_FROM_SCTP_INDATA + SCTP_LOC_20);
		}
		sctp_send_shutdown(stcb,
		    ((stcb->asoc.alternate) ? stcb->asoc.alternate : stcb->asoc.primary_destination));
		sctp_send_sack(stcb, SCTP_SO_NOT_LOCKED);
	} else {
		int is_a_gap;

		/* is there a gap now ? */
		is_a_gap = SCTP_TSN_GT(highest_tsn, stcb->asoc.cumulative_tsn);

		/*
		 * CMT DAC algorithm: increase number of packets received
		 * since last ack
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

			if ((stcb->asoc.sctp_cmt_on_off > 0) &&
			    (SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) &&
			    (stcb->asoc.send_sack == 0) &&
			    (stcb->asoc.numduptsns == 0) &&
			    (stcb->asoc.delayed_ack) &&
			    (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer))) {

				/*
				 * CMT DAC algorithm: With CMT, delay acks
				 * even in the face of
				 * 
				 * reordering. Therefore, if acks that do not
				 * have to be sent because of the above
				 * reasons, will be delayed. That is, acks
				 * that would have been sent due to gap
				 * reports will be delayed with DAC. Start
				 * the delayed ack timer.
				 */
				sctp_timer_start(SCTP_TIMER_TYPE_RECV,
				    stcb->sctp_ep, stcb, NULL);
			} else {
				/*
				 * Ok we must build a SACK since the timer
				 * is pending, we got our first packet OR
				 * there are gaps or duplicates.
				 */
				(void)SCTP_OS_TIMER_STOP(&stcb->asoc.dack_timer.timer);
				sctp_send_sack(stcb, SCTP_SO_NOT_LOCKED);
			}
		} else {
			if (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
				sctp_timer_start(SCTP_TIMER_TYPE_RECV,
				    stcb->sctp_ep, stcb, NULL);
			}
		}
	}
}

void
sctp_service_queues(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	uint32_t tsize, pd_point;
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
		 * be here or the socket buffer max or nothing on the
		 * delivery queue and something can be delivered.
		 */
		if (stcb->sctp_socket) {
			pd_point = min(SCTP_SB_LIMIT_RCV(stcb->sctp_socket) >> SCTP_PARTIAL_DELIVERY_SHIFT,
			    stcb->sctp_ep->partial_delivery_point);
		} else {
			pd_point = stcb->sctp_ep->partial_delivery_point;
		}
		if (sctp_is_all_msg_on_reasm(asoc, &tsize) || (tsize >= pd_point)) {
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
    struct sockaddr *src, struct sockaddr *dst,
    struct sctphdr *sh, struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct sctp_nets *net, uint32_t * high_tsn,
    uint8_t mflowtype, uint32_t mflowid,
    uint32_t vrf_id, uint16_t port)
{
	struct sctp_data_chunk *ch, chunk_buf;
	struct sctp_association *asoc;
	int num_chunks = 0;	/* number of control chunks processed */
	int stop_proc = 0;
	int chk_length, break_flag, last_chunk;
	int abort_flag = 0, was_a_gap;
	struct mbuf *m;
	uint32_t highest_tsn;

	/* set the rwnd */
	sctp_set_rwnd(stcb, &stcb->asoc);

	m = *mm;
	SCTP_TCB_LOCK_ASSERT(stcb);
	asoc = &stcb->asoc;
	if (SCTP_TSN_GT(asoc->highest_tsn_inside_nr_map, asoc->highest_tsn_inside_map)) {
		highest_tsn = asoc->highest_tsn_inside_nr_map;
	} else {
		highest_tsn = asoc->highest_tsn_inside_map;
	}
	was_a_gap = SCTP_TSN_GT(highest_tsn, stcb->asoc.cumulative_tsn);
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
		m = sctp_get_mbuf_for_msg(SCTP_BUF_LEN(m), 0, M_NOWAIT, 1, MT_DATA);
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
			continue;
		}
		if (ch->ch.chunk_type == SCTP_DATA) {
			if ((size_t)chk_length < sizeof(struct sctp_data_chunk)) {
				/*
				 * Need to send an abort since we had a
				 * invalid data chunk.
				 */
				struct mbuf *op_err;
				char msg[SCTP_DIAG_INFO_LEN];

				snprintf(msg, sizeof(msg), "DATA chunk of length %d",
				    chk_length);
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_21;
				sctp_abort_association(inp, stcb, m, iphlen,
				    src, dst, sh, op_err,
				    mflowtype, mflowid,
				    vrf_id, port);
				return (2);
			}
			if ((size_t)chk_length == sizeof(struct sctp_data_chunk)) {
				/*
				 * Need to send an abort since we had an
				 * empty data chunk.
				 */
				struct mbuf *op_err;

				op_err = sctp_generate_no_user_data_cause(ch->dp.tsn);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_22;
				sctp_abort_association(inp, stcb, m, iphlen,
				    src, dst, sh, op_err,
				    mflowtype, mflowid,
				    vrf_id, port);
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
				continue;
			}
		} else {
			/* not a data chunk in the data region */
			switch (ch->ch.chunk_type) {
			case SCTP_INITIATION:
			case SCTP_INITIATION_ACK:
			case SCTP_SELECTIVE_ACK:
			case SCTP_NR_SELECTIVE_ACK:
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
				if (SCTP_BASE_SYSCTL(sctp_strict_data_order)) {
					struct mbuf *op_err;
					char msg[SCTP_DIAG_INFO_LEN];

					snprintf(msg, sizeof(msg), "DATA chunk followwd by chunk of type %2.2x",
					    ch->ch.chunk_type);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					sctp_abort_association(inp, stcb,
					    m, iphlen,
					    src, dst,
					    sh, op_err,
					    mflowtype, mflowid,
					    vrf_id, port);
					return (2);
				}
				break;
			default:
				/* unknown chunk type, use bit rules */
				if (ch->ch.chunk_type & 0x40) {
					/* Add a error report to the queue */
					struct mbuf *merr;
					struct sctp_paramhdr *phd;

					merr = sctp_get_mbuf_for_msg(sizeof(*phd), 0, M_NOWAIT, 1, MT_DATA);
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
						SCTP_BUF_NEXT(merr) = SCTP_M_COPYM(m, *offset, chk_length, M_NOWAIT);
						if (SCTP_BUF_NEXT(merr)) {
							if (sctp_pad_lastmbuf(SCTP_BUF_NEXT(merr), SCTP_SIZE32(chk_length) - chk_length, NULL) == NULL) {
								sctp_m_freem(merr);
							} else {
								sctp_queue_op_err(stcb, merr);
							}
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
			}	/* switch of chunk type */
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
			continue;
		}
	}
	if (break_flag) {
		/*
		 * we need to report rwnd overrun drops.
		 */
		sctp_send_packet_dropped(stcb, net, *mm, length, iphlen, 0);
	}
	if (num_chunks) {
		/*
		 * Did we get data, if so update the time for auto-close and
		 * give peer credit for being alive.
		 */
		SCTP_STAT_INCR(sctps_recvpktwithdata);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
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
	sctp_sack_check(stcb, was_a_gap);
	return (0);
}

static int
sctp_process_segment_range(struct sctp_tcb *stcb, struct sctp_tmit_chunk **p_tp1, uint32_t last_tsn,
    uint16_t frag_strt, uint16_t frag_end, int nr_sacking,
    int *num_frs,
    uint32_t * biggest_newly_acked_tsn,
    uint32_t * this_sack_lowest_newack,
    int *rto_ok)
{
	struct sctp_tmit_chunk *tp1;
	unsigned int theTSN;
	int j, wake_him = 0, circled = 0;

	/* Recover the tp1 we last saw */
	tp1 = *p_tp1;
	if (tp1 == NULL) {
		tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
	}
	for (j = frag_strt; j <= frag_end; j++) {
		theTSN = j + last_tsn;
		while (tp1) {
			if (tp1->rec.data.doing_fast_retransmit)
				(*num_frs) += 1;

			/*-
			 * CMT: CUCv2 algorithm. For each TSN being
			 * processed from the sent queue, track the
			 * next expected pseudo-cumack, or
			 * rtx_pseudo_cumack, if required. Separate
			 * cumack trackers for first transmissions,
			 * and retransmissions.
			 */
			if ((tp1->sent < SCTP_DATAGRAM_RESEND) &&
			    (tp1->whoTo->find_pseudo_cumack == 1) &&
			    (tp1->snd_count == 1)) {
				tp1->whoTo->pseudo_cumack = tp1->rec.data.TSN_seq;
				tp1->whoTo->find_pseudo_cumack = 0;
			}
			if ((tp1->sent < SCTP_DATAGRAM_RESEND) &&
			    (tp1->whoTo->find_rtx_pseudo_cumack == 1) &&
			    (tp1->snd_count > 1)) {
				tp1->whoTo->rtx_pseudo_cumack = tp1->rec.data.TSN_seq;
				tp1->whoTo->find_rtx_pseudo_cumack = 0;
			}
			if (tp1->rec.data.TSN_seq == theTSN) {
				if (tp1->sent != SCTP_DATAGRAM_UNSENT) {
					/*-
					 * must be held until
					 * cum-ack passes
					 */
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						/*-
						 * If it is less than RESEND, it is
						 * now no-longer in flight.
						 * Higher values may already be set
						 * via previous Gap Ack Blocks...
						 * i.e. ACKED or RESEND.
						 */
						if (SCTP_TSN_GT(tp1->rec.data.TSN_seq,
						    *biggest_newly_acked_tsn)) {
							*biggest_newly_acked_tsn = tp1->rec.data.TSN_seq;
						}
						/*-
						 * CMT: SFR algo (and HTNA) - set
						 * saw_newack to 1 for dest being
						 * newly acked. update
						 * this_sack_highest_newack if
						 * appropriate.
						 */
						if (tp1->rec.data.chunk_was_revoked == 0)
							tp1->whoTo->saw_newack = 1;

						if (SCTP_TSN_GT(tp1->rec.data.TSN_seq,
						    tp1->whoTo->this_sack_highest_newack)) {
							tp1->whoTo->this_sack_highest_newack =
							    tp1->rec.data.TSN_seq;
						}
						/*-
						 * CMT DAC algo: also update
						 * this_sack_lowest_newack
						 */
						if (*this_sack_lowest_newack == 0) {
							if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
								sctp_log_sack(*this_sack_lowest_newack,
								    last_tsn,
								    tp1->rec.data.TSN_seq,
								    0,
								    0,
								    SCTP_LOG_TSN_ACKED);
							}
							*this_sack_lowest_newack = tp1->rec.data.TSN_seq;
						}
						/*-
						 * CMT: CUCv2 algorithm. If (rtx-)pseudo-cumack for corresp
						 * dest is being acked, then we have a new (rtx-)pseudo-cumack. Set
						 * new_(rtx_)pseudo_cumack to TRUE so that the cwnd for this dest can be
						 * updated. Also trigger search for the next expected (rtx-)pseudo-cumack.
						 * Separate pseudo_cumack trackers for first transmissions and
						 * retransmissions.
						 */
						if (tp1->rec.data.TSN_seq == tp1->whoTo->pseudo_cumack) {
							if (tp1->rec.data.chunk_was_revoked == 0) {
								tp1->whoTo->new_pseudo_cumack = 1;
							}
							tp1->whoTo->find_pseudo_cumack = 1;
						}
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
							sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.TSN_seq, SCTP_CWND_LOG_FROM_SACK);
						}
						if (tp1->rec.data.TSN_seq == tp1->whoTo->rtx_pseudo_cumack) {
							if (tp1->rec.data.chunk_was_revoked == 0) {
								tp1->whoTo->new_pseudo_cumack = 1;
							}
							tp1->whoTo->find_rtx_pseudo_cumack = 1;
						}
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
							sctp_log_sack(*biggest_newly_acked_tsn,
							    last_tsn,
							    tp1->rec.data.TSN_seq,
							    frag_strt,
							    frag_end,
							    SCTP_LOG_TSN_ACKED);
						}
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_GAP,
							    tp1->whoTo->flight_size,
							    tp1->book_size,
							    (uintptr_t) tp1->whoTo,
							    tp1->rec.data.TSN_seq);
						}
						sctp_flight_size_decrease(tp1);
						if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
							(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) (tp1->whoTo,
							    tp1);
						}
						sctp_total_flight_decrease(stcb, tp1);

						tp1->whoTo->net_ack += tp1->send_size;
						if (tp1->snd_count < 2) {
							/*-
							 * True non-retransmited chunk
							 */
							tp1->whoTo->net_ack2 += tp1->send_size;

							/*-
							 * update RTO too ?
							 */
							if (tp1->do_rtt) {
								if (*rto_ok) {
									tp1->whoTo->RTO =
									    sctp_calculate_rto(stcb,
									    &stcb->asoc,
									    tp1->whoTo,
									    &tp1->sent_rcv_time,
									    sctp_align_safe_nocopy,
									    SCTP_RTT_FROM_DATA);
									*rto_ok = 0;
								}
								if (tp1->whoTo->rto_needed == 0) {
									tp1->whoTo->rto_needed = 1;
								}
								tp1->do_rtt = 0;
							}
						}
					}
					if (tp1->sent <= SCTP_DATAGRAM_RESEND) {
						if (SCTP_TSN_GT(tp1->rec.data.TSN_seq,
						    stcb->asoc.this_sack_highest_gap)) {
							stcb->asoc.this_sack_highest_gap =
							    tp1->rec.data.TSN_seq;
						}
						if (tp1->sent == SCTP_DATAGRAM_RESEND) {
							sctp_ucount_decr(stcb->asoc.sent_queue_retran_cnt);
#ifdef SCTP_AUDITING_ENABLED
							sctp_audit_log(0xB2,
							    (stcb->asoc.sent_queue_retran_cnt & 0x000000ff));
#endif
						}
					}
					/*-
					 * All chunks NOT UNSENT fall through here and are marked
					 * (leave PR-SCTP ones that are to skip alone though)
					 */
					if ((tp1->sent != SCTP_FORWARD_TSN_SKIP) &&
					    (tp1->sent != SCTP_DATAGRAM_NR_ACKED)) {
						tp1->sent = SCTP_DATAGRAM_MARKED;
					}
					if (tp1->rec.data.chunk_was_revoked) {
						/* deflate the cwnd */
						tp1->whoTo->cwnd -= tp1->book_size;
						tp1->rec.data.chunk_was_revoked = 0;
					}
					/* NR Sack code here */
					if (nr_sacking &&
					    (tp1->sent != SCTP_DATAGRAM_NR_ACKED)) {
						if (stcb->asoc.strmout[tp1->rec.data.stream_number].chunks_on_queues > 0) {
							stcb->asoc.strmout[tp1->rec.data.stream_number].chunks_on_queues--;
#ifdef INVARIANTS
						} else {
							panic("No chunks on the queues for sid %u.", tp1->rec.data.stream_number);
#endif
						}
						tp1->sent = SCTP_DATAGRAM_NR_ACKED;
						if (tp1->data) {
							/*
							 * sa_ignore
							 * NO_NULL_CHK
							 */
							sctp_free_bufspace(stcb, &stcb->asoc, tp1, 1);
							sctp_m_freem(tp1->data);
							tp1->data = NULL;
						}
						wake_him++;
					}
				}
				break;
			}	/* if (tp1->TSN_seq == theTSN) */
			if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, theTSN)) {
				break;
			}
			tp1 = TAILQ_NEXT(tp1, sctp_next);
			if ((tp1 == NULL) && (circled == 0)) {
				circled++;
				tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
			}
		}		/* end while (tp1) */
		if (tp1 == NULL) {
			circled = 0;
			tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
		}
		/* In case the fragments were not in order we must reset */
	}			/* end for (j = fragStart */
	*p_tp1 = tp1;
	return (wake_him);	/* Return value only used for nr-sack */
}


static int
sctp_handle_segments(struct mbuf *m, int *offset, struct sctp_tcb *stcb, struct sctp_association *asoc,
    uint32_t last_tsn, uint32_t * biggest_tsn_acked,
    uint32_t * biggest_newly_acked_tsn, uint32_t * this_sack_lowest_newack,
    int num_seg, int num_nr_seg, int *rto_ok)
{
	struct sctp_gap_ack_block *frag, block;
	struct sctp_tmit_chunk *tp1;
	int i;
	int num_frs = 0;
	int chunk_freed;
	int non_revocable;
	uint16_t frag_strt, frag_end, prev_frag_end;

	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	prev_frag_end = 0;
	chunk_freed = 0;

	for (i = 0; i < (num_seg + num_nr_seg); i++) {
		if (i == num_seg) {
			prev_frag_end = 0;
			tp1 = TAILQ_FIRST(&asoc->sent_queue);
		}
		frag = (struct sctp_gap_ack_block *)sctp_m_getptr(m, *offset,
		    sizeof(struct sctp_gap_ack_block), (uint8_t *) & block);
		*offset += sizeof(block);
		if (frag == NULL) {
			return (chunk_freed);
		}
		frag_strt = ntohs(frag->start);
		frag_end = ntohs(frag->end);

		if (frag_strt > frag_end) {
			/* This gap report is malformed, skip it. */
			continue;
		}
		if (frag_strt <= prev_frag_end) {
			/* This gap report is not in order, so restart. */
			tp1 = TAILQ_FIRST(&asoc->sent_queue);
		}
		if (SCTP_TSN_GT((last_tsn + frag_end), *biggest_tsn_acked)) {
			*biggest_tsn_acked = last_tsn + frag_end;
		}
		if (i < num_seg) {
			non_revocable = 0;
		} else {
			non_revocable = 1;
		}
		if (sctp_process_segment_range(stcb, &tp1, last_tsn, frag_strt, frag_end,
		    non_revocable, &num_frs, biggest_newly_acked_tsn,
		    this_sack_lowest_newack, rto_ok)) {
			chunk_freed = 1;
		}
		prev_frag_end = frag_end;
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
		if (num_frs)
			sctp_log_fr(*biggest_tsn_acked,
			    *biggest_newly_acked_tsn,
			    last_tsn, SCTP_FR_LOG_BIGGEST_TSNS);
	}
	return (chunk_freed);
}

static void
sctp_check_for_revoked(struct sctp_tcb *stcb,
    struct sctp_association *asoc, uint32_t cumack,
    uint32_t biggest_tsn_acked)
{
	struct sctp_tmit_chunk *tp1;

	TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
		if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, cumack)) {
			/*
			 * ok this guy is either ACK or MARKED. If it is
			 * ACKED it has been previously acked but not this
			 * time i.e. revoked.  If it is MARKED it was ACK'ed
			 * again.
			 */
			if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, biggest_tsn_acked)) {
				break;
			}
			if (tp1->sent == SCTP_DATAGRAM_ACKED) {
				/* it has been revoked */
				tp1->sent = SCTP_DATAGRAM_SENT;
				tp1->rec.data.chunk_was_revoked = 1;
				/*
				 * We must add this stuff back in to assure
				 * timers and such get started.
				 */
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
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
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
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
	}
}


static void
sctp_strike_gap_ack_chunks(struct sctp_tcb *stcb, struct sctp_association *asoc,
    uint32_t biggest_tsn_acked, uint32_t biggest_tsn_newly_acked, uint32_t this_sack_lowest_newack, int accum_moved)
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
	if ((asoc->sctp_cmt_on_off > 0) &&
	    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->saw_newack)
				num_dests_sacked++;
		}
	}
	if (stcb->asoc.prsctp_supported) {
		(void)SCTP_GETTIME_TIMEVAL(&now);
	}
	TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
		strike_flag = 0;
		if (tp1->no_fr_allowed) {
			/* this one had a timeout or something */
			continue;
		}
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
			if (tp1->sent < SCTP_DATAGRAM_RESEND)
				sctp_log_fr(biggest_tsn_newly_acked,
				    tp1->rec.data.TSN_seq,
				    tp1->sent,
				    SCTP_FR_LOG_CHECK_STRIKE);
		}
		if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, biggest_tsn_acked) ||
		    tp1->sent == SCTP_DATAGRAM_UNSENT) {
			/* done */
			break;
		}
		if (stcb->asoc.prsctp_supported) {
			if ((PR_SCTP_TTL_ENABLED(tp1->flags)) && tp1->sent < SCTP_DATAGRAM_ACKED) {
				/* Is it expired? */
				if (timevalcmp(&now, &tp1->rec.data.timetodrop, >)) {
					/* Yes so drop it */
					if (tp1->data != NULL) {
						(void)sctp_release_pr_sctp_chunk(stcb, tp1, 1,
						    SCTP_SO_NOT_LOCKED);
					}
					continue;
				}
			}
		}
		if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, asoc->this_sack_highest_gap)) {
			/* we are beyond the tsn in the sack  */
			break;
		}
		if (tp1->sent >= SCTP_DATAGRAM_RESEND) {
			/* either a RESEND, ACKED, or MARKED */
			/* skip */
			if (tp1->sent == SCTP_FORWARD_TSN_SKIP) {
				/* Continue strikin FWD-TSN chunks */
				tp1->rec.data.fwd_tsn_cnt++;
			}
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
			continue;
		} else if (tp1->whoTo && SCTP_TSN_GT(tp1->rec.data.TSN_seq,
		    tp1->whoTo->this_sack_highest_newack)) {
			/*
			 * CMT: New acks were receieved for data sent to
			 * this dest. But no new acks were seen for data
			 * sent after tp1. Therefore, according to the SFR
			 * algo for CMT, tp1 cannot be marked for FR using
			 * this SACK. This step covers part of the DAC algo
			 * and the HTNA algo as well.
			 */
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
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(biggest_tsn_newly_acked,
				    tp1->rec.data.TSN_seq,
				    tp1->sent,
				    SCTP_FR_LOG_STRIKE_CHUNK);
			}
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				tp1->sent++;
			}
			if ((asoc->sctp_cmt_on_off > 0) &&
			    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {
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
				    SCTP_TSN_GT(this_sack_lowest_newack, tp1->rec.data.TSN_seq)) {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(16 + num_dests_sacked,
						    tp1->rec.data.TSN_seq,
						    tp1->sent,
						    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					tp1->sent++;
				}
			}
		} else if ((tp1->rec.data.doing_fast_retransmit) &&
		    (asoc->sctp_cmt_on_off == 0)) {
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

				if (SCTP_TSN_GE(biggest_tsn_newly_acked,
				    tp1->rec.data.fast_retran_tsn)) {
					/*
					 * Strike the TSN, since this ack is
					 * beyond where things were when we
					 * did a FR.
					 */
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(biggest_tsn_newly_acked,
						    tp1->rec.data.TSN_seq,
						    tp1->sent,
						    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						tp1->sent++;
					}
					strike_flag = 1;
					if ((asoc->sctp_cmt_on_off > 0) &&
					    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {
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
						    SCTP_TSN_GT(this_sack_lowest_newack,
						    tp1->rec.data.TSN_seq)) {
							if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
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
		} else if (SCTP_TSN_GT(tp1->rec.data.TSN_seq,
		    biggest_tsn_newly_acked)) {
			/*
			 * We don't strike these: This is the  HTNA
			 * algorithm i.e. we don't strike If our TSN is
			 * larger than the Highest TSN Newly Acked.
			 */
			;
		} else {
			/* Strike the TSN */
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(biggest_tsn_newly_acked,
				    tp1->rec.data.TSN_seq,
				    tp1->sent,
				    SCTP_FR_LOG_STRIKE_CHUNK);
			}
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				tp1->sent++;
			}
			if ((asoc->sctp_cmt_on_off > 0) &&
			    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {
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
				    SCTP_TSN_GT(this_sack_lowest_newack, tp1->rec.data.TSN_seq)) {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
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
			struct sctp_nets *alt;

			/* fix counts and things */
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
				sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_RSND,
				    (tp1->whoTo ? (tp1->whoTo->flight_size) : 0),
				    tp1->book_size,
				    (uintptr_t) tp1->whoTo,
				    tp1->rec.data.TSN_seq);
			}
			if (tp1->whoTo) {
				tp1->whoTo->net_ack++;
				sctp_flight_size_decrease(tp1);
				if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
					(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) (tp1->whoTo,
					    tp1);
				}
			}
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
				sctp_log_rwnd(SCTP_INCREASE_PEER_RWND,
				    asoc->peers_rwnd, tp1->send_size, SCTP_BASE_SYSCTL(sctp_peer_chunk_oh));
			}
			/* add back to the rwnd */
			asoc->peers_rwnd += (tp1->send_size + SCTP_BASE_SYSCTL(sctp_peer_chunk_oh));

			/* remove from the total flight */
			sctp_total_flight_decrease(stcb, tp1);

			if ((stcb->asoc.prsctp_supported) &&
			    (PR_SCTP_RTX_ENABLED(tp1->flags))) {
				/*
				 * Has it been retransmitted tv_sec times? -
				 * we store the retran count there.
				 */
				if (tp1->snd_count > tp1->rec.data.timetodrop.tv_sec) {
					/* Yes, so drop it */
					if (tp1->data != NULL) {
						(void)sctp_release_pr_sctp_chunk(stcb, tp1, 1,
						    SCTP_SO_NOT_LOCKED);
					}
					/* Make sure to flag we had a FR */
					tp1->whoTo->net_ack++;
					continue;
				}
			}
			/*
			 * SCTP_PRINTF("OK, we are now ready to FR this
			 * guy\n");
			 */
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(tp1->rec.data.TSN_seq, tp1->snd_count,
				    0, SCTP_FR_MARKED);
			}
			if (strike_flag) {
				/* This is a subsequent FR */
				SCTP_STAT_INCR(sctps_sendmultfastretrans);
			}
			sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			if (asoc->sctp_cmt_on_off > 0) {
				/*
				 * CMT: Using RTX_SSTHRESH policy for CMT.
				 * If CMT is being used, then pick dest with
				 * largest ssthresh for any retransmission.
				 */
				tp1->no_fr_allowed = 1;
				alt = tp1->whoTo;
				/* sa_ignore NO_NULL_CHK */
				if (asoc->sctp_cmt_pf > 0) {
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
			 * SCTP_PRINTF("Marking TSN for FR new value %x\n",
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
				if ((tp1->whoTo != NULL) &&
				    (tp1->whoTo->rto_needed == 0)) {
					tp1->whoTo->rto_needed = 1;
				}
				tp1->do_rtt = 0;
			}
			if (alt != tp1->whoTo) {
				/* yes, there is an alternate. */
				sctp_free_remote_addr(tp1->whoTo);
				/* sa_ignore FREED_MEMORY */
				tp1->whoTo = alt;
				atomic_add_int(&alt->ref_count, 1);
			}
		}
	}
}

struct sctp_tmit_chunk *
sctp_try_advance_peer_ack_point(struct sctp_tcb *stcb,
    struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *tp1, *tp2, *a_adv = NULL;
	struct timeval now;
	int now_filled = 0;

	if (asoc->prsctp_supported == 0) {
		return (NULL);
	}
	TAILQ_FOREACH_SAFE(tp1, &asoc->sent_queue, sctp_next, tp2) {
		if (tp1->sent != SCTP_FORWARD_TSN_SKIP &&
		    tp1->sent != SCTP_DATAGRAM_RESEND &&
		    tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
			/* no chance to advance, out of here */
			break;
		}
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_TRY_ADVANCE) {
			if ((tp1->sent == SCTP_FORWARD_TSN_SKIP) ||
			    (tp1->sent == SCTP_DATAGRAM_NR_ACKED)) {
				sctp_misc_ints(SCTP_FWD_TSN_CHECK,
				    asoc->advanced_peer_ack_point,
				    tp1->rec.data.TSN_seq, 0, 0);
			}
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
					    1, SCTP_SO_NOT_LOCKED);
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
		if ((tp1->sent == SCTP_FORWARD_TSN_SKIP) ||
		    (tp1->sent == SCTP_DATAGRAM_NR_ACKED)) {
			/* advance PeerAckPoint goes forward */
			if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, asoc->advanced_peer_ack_point)) {
				asoc->advanced_peer_ack_point = tp1->rec.data.TSN_seq;
				a_adv = tp1;
			} else if (tp1->rec.data.TSN_seq == asoc->advanced_peer_ack_point) {
				/* No update but we do save the chk */
				a_adv = tp1;
			}
		} else {
			/*
			 * If it is still in RESEND we can advance no
			 * further
			 */
			break;
		}
	}
	return (a_adv);
}

static int
sctp_fs_audit(struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	int inflight = 0, resend = 0, inbetween = 0, acked = 0, above = 0;
	int ret;

#ifndef INVARIANTS
	int entry_flight, entry_cnt;

#endif

	ret = 0;
#ifndef INVARIANTS
	entry_flight = asoc->total_flight;
	entry_cnt = asoc->total_flight_count;
#endif
	if (asoc->pr_sctp_cnt >= asoc->sent_queue_cnt)
		return (0);

	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		if (chk->sent < SCTP_DATAGRAM_RESEND) {
			SCTP_PRINTF("Chk TSN:%u size:%d inflight cnt:%d\n",
			    chk->rec.data.TSN_seq,
			    chk->send_size,
			    chk->snd_count);
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
		SCTP_PRINTF("asoc->total_flight:%d cnt:%d\n",
		    entry_flight, entry_cnt);

		SCTP_PRINTF("Flight size-express incorrect F:%d I:%d R:%d Ab:%d ACK:%d\n",
		    inflight, inbetween, resend, above, acked);
		ret = 1;
#endif
	}
	return (ret);
}


static void
sctp_window_probe_recovery(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    struct sctp_tmit_chunk *tp1)
{
	tp1->window_probe = 0;
	if ((tp1->sent >= SCTP_DATAGRAM_ACKED) || (tp1->data == NULL)) {
		/* TSN's skipped we do NOT move back. */
		sctp_misc_ints(SCTP_FLIGHT_LOG_DWN_WP_FWD,
		    tp1->whoTo ? tp1->whoTo->flight_size : 0,
		    tp1->book_size,
		    (uintptr_t) tp1->whoTo,
		    tp1->rec.data.TSN_seq);
		return;
	}
	/* First setup this by shrinking flight */
	if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
		(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) (tp1->whoTo,
		    tp1);
	}
	sctp_flight_size_decrease(tp1);
	sctp_total_flight_decrease(stcb, tp1);
	/* Now mark for resend */
	tp1->sent = SCTP_DATAGRAM_RESEND;
	sctp_ucount_incr(asoc->sent_queue_retran_cnt);

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_WP,
		    tp1->whoTo->flight_size,
		    tp1->book_size,
		    (uintptr_t) tp1->whoTo,
		    tp1->rec.data.TSN_seq);
	}
}

void
sctp_express_handle_sack(struct sctp_tcb *stcb, uint32_t cumack,
    uint32_t rwnd, int *abort_now, int ecne_seen)
{
	struct sctp_nets *net;
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *tp1, *tp2;
	uint32_t old_rwnd;
	int win_probe_recovery = 0;
	int win_probe_recovered = 0;
	int j, done_once = 0;
	int rto_ok = 1;

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_SACK_ARRIVALS_ENABLE) {
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
	if (SCTP_TSN_GT(asoc->last_acked_seq, cumack)) {
		/* old ack */
		return;
	} else if (asoc->last_acked_seq == cumack) {
		/* Window update sack */
		asoc->peers_rwnd = sctp_sbspace_sub(rwnd,
		    (uint32_t) (asoc->total_flight + (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh))));
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
		if (SCTP_TSN_GT(cumack, net->cwr_window_tsn)) {
			/* Drag along the window_tsn for cwr's */
			net->cwr_window_tsn = cumack;
		}
		net->prev_cwnd = net->cwnd;
		net->net_ack = 0;
		net->net_ack2 = 0;

		/*
		 * CMT: Reset CUC and Fast recovery algo variables before
		 * SACK processing
		 */
		net->new_pseudo_cumack = 0;
		net->will_exit_fast_recovery = 0;
		if (stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack) {
			(*stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack) (stcb, net);
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_strict_sacks)) {
		uint32_t send_s;

		if (!TAILQ_EMPTY(&asoc->sent_queue)) {
			tp1 = TAILQ_LAST(&asoc->sent_queue,
			    sctpchunk_listhead);
			send_s = tp1->rec.data.TSN_seq + 1;
		} else {
			send_s = asoc->sending_seq;
		}
		if (SCTP_TSN_GE(cumack, send_s)) {
			struct mbuf *op_err;
			char msg[SCTP_DIAG_INFO_LEN];

			*abort_now = 1;
			/* XXX */
			snprintf(msg, sizeof(msg), "Cum ack %8.8x greater or equal than TSN %8.8x",
			    cumack, send_s);
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_23;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
			return;
		}
	}
	asoc->this_sack_highest_gap = cumack;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
		sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
		    stcb->asoc.overall_error_count,
		    0,
		    SCTP_FROM_SCTP_INDATA,
		    __LINE__);
	}
	stcb->asoc.overall_error_count = 0;
	if (SCTP_TSN_GT(cumack, asoc->last_acked_seq)) {
		/* process the new consecutive TSN first */
		TAILQ_FOREACH_SAFE(tp1, &asoc->sent_queue, sctp_next, tp2) {
			if (SCTP_TSN_GE(cumack, tp1->rec.data.TSN_seq)) {
				if (tp1->sent == SCTP_DATAGRAM_UNSENT) {
					SCTP_PRINTF("Warning, an unsent is now acked?\n");
				}
				if (tp1->sent < SCTP_DATAGRAM_ACKED) {
					/*
					 * If it is less than ACKED, it is
					 * now no-longer in flight. Higher
					 * values may occur during marking
					 */
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_CA,
							    tp1->whoTo->flight_size,
							    tp1->book_size,
							    (uintptr_t) tp1->whoTo,
							    tp1->rec.data.TSN_seq);
						}
						sctp_flight_size_decrease(tp1);
						if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
							(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) (tp1->whoTo,
							    tp1);
						}
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
							if (rto_ok) {
								tp1->whoTo->RTO =
								/*
								 * sa_ignore
								 * NO_NULL_CH
								 * K
								 */
								    sctp_calculate_rto(stcb,
								    asoc, tp1->whoTo,
								    &tp1->sent_rcv_time,
								    sctp_align_safe_nocopy,
								    SCTP_RTT_FROM_DATA);
								rto_ok = 0;
							}
							if (tp1->whoTo->rto_needed == 0) {
								tp1->whoTo->rto_needed = 1;
							}
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

					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
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
				if (tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
					if (asoc->strmout[tp1->rec.data.stream_number].chunks_on_queues > 0) {
						asoc->strmout[tp1->rec.data.stream_number].chunks_on_queues--;
#ifdef INVARIANTS
					} else {
						panic("No chunks on the queues for sid %u.", tp1->rec.data.stream_number);
#endif
					}
				}
				TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
				if (tp1->data) {
					/* sa_ignore NO_NULL_CHK */
					sctp_free_bufspace(stcb, asoc, tp1, 1);
					sctp_m_freem(tp1->data);
					tp1->data = NULL;
				}
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
					sctp_log_sack(asoc->last_acked_seq,
					    cumack,
					    tp1->rec.data.TSN_seq,
					    0,
					    0,
					    SCTP_LOG_FREE_SENT);
				}
				asoc->sent_queue_cnt--;
				sctp_free_a_chunk(stcb, tp1, SCTP_SO_NOT_LOCKED);
			} else {
				break;
			}
		}

	}
	/* sa_ignore NO_NULL_CHK */
	if (stcb->sctp_socket) {
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		struct socket *so;

#endif
		SOCKBUF_LOCK(&stcb->sctp_socket->so_snd);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			/* sa_ignore NO_NULL_CHK */
			sctp_wakeup_log(stcb, 1, SCTP_WAKESND_FROM_SACK);
		}
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
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
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	} else {
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, 1, SCTP_NOWAKE_FROM_SACK);
		}
	}

	/* JRS - Use the congestion control given in the CC module */
	if ((asoc->last_acked_seq != cumack) && (ecne_seen == 0)) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->net_ack2 > 0) {
				/*
				 * Karn's rule applies to clearing error
				 * count, this is optional.
				 */
				net->error_count = 0;
				if (!(net->dest_state & SCTP_ADDR_REACHABLE)) {
					/* addr came good */
					net->dest_state |= SCTP_ADDR_REACHABLE;
					sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
					    0, (void *)net, SCTP_SO_NOT_LOCKED);
				}
				if (net == stcb->asoc.primary_destination) {
					if (stcb->asoc.alternate) {
						/*
						 * release the alternate,
						 * primary is good
						 */
						sctp_free_remote_addr(stcb->asoc.alternate);
						stcb->asoc.alternate = NULL;
					}
				}
				if (net->dest_state & SCTP_ADDR_PF) {
					net->dest_state &= ~SCTP_ADDR_PF;
					sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT,
					    stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_24);
					sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, stcb->sctp_ep, stcb, net);
					asoc->cc_functions.sctp_cwnd_update_exit_pf(stcb, net);
					/* Done with this net */
					net->net_ack = 0;
				}
				/* restore any doubled timers */
				net->RTO = (net->lastsa >> SCTP_RTT_SHIFT) + net->lastsv;
				if (net->RTO < stcb->asoc.minrto) {
					net->RTO = stcb->asoc.minrto;
				}
				if (net->RTO > stcb->asoc.maxrto) {
					net->RTO = stcb->asoc.maxrto;
				}
			}
		}
		asoc->cc_functions.sctp_cwnd_update_after_sack(stcb, asoc, 1, 0, 0);
	}
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
	/* RWND update */
	asoc->peers_rwnd = sctp_sbspace_sub(rwnd,
	    (uint32_t) (asoc->total_flight + (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh))));
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
		int to_ticks;

		if (win_probe_recovery && (net->window_probe)) {
			win_probe_recovered = 1;
			/*
			 * Find first chunk that was used with window probe
			 * and clear the sent
			 */
			/* sa_ignore FREED_MEMORY */
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->window_probe) {
					/* move back to data send queue */
					sctp_window_probe_recovery(stcb, asoc, tp1);
					break;
				}
			}
		}
		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		if (net->flight_size) {
			j++;
			(void)SCTP_OS_TIMER_START(&net->rxt_timer.timer, to_ticks,
			    sctp_timeout_handler, &net->rxt_timer);
			if (net->window_probe) {
				net->window_probe = 0;
			}
		} else {
			if (net->window_probe) {
				/*
				 * In window probes we must assure a timer
				 * is still running there
				 */
				net->window_probe = 0;
				if (!SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
					SCTP_OS_TIMER_START(&net->rxt_timer.timer, to_ticks,
					    sctp_timeout_handler, &net->rxt_timer);
				}
			} else if (SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net,
				    SCTP_FROM_SCTP_INDATA + SCTP_LOC_25);
			}
		}
	}
	if ((j == 0) &&
	    (!TAILQ_EMPTY(&asoc->sent_queue)) &&
	    (asoc->sent_queue_retran_cnt == 0) &&
	    (win_probe_recovered == 0) &&
	    (done_once == 0)) {
		/*
		 * huh, this should not happen unless all packets are
		 * PR-SCTP and marked to skip of course.
		 */
		if (sctp_fs_audit(asoc)) {
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
					sctp_ucount_incr(asoc->sent_queue_retran_cnt);
				}
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
				struct mbuf *op_err;

		abort_out_now:
				*abort_now = 1;
				/* XXX */
				op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_26;
				sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
			} else {
				struct sctp_nets *netp;

				if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
				SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
				sctp_stop_timers_for_shutdown(stcb);
				if (asoc->alternate) {
					netp = asoc->alternate;
				} else {
					netp = asoc->primary_destination;
				}
				sctp_send_shutdown(stcb, netp);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
				    stcb->sctp_ep, stcb, netp);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb, netp);
			}
		} else if ((SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED) &&
		    (asoc->stream_queue_cnt == 0)) {
			struct sctp_nets *netp;

			if (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT) {
				goto abort_out_now;
			}
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_ACK_SENT);
			SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
			sctp_stop_timers_for_shutdown(stcb);
			if (asoc->alternate) {
				netp = asoc->alternate;
			} else {
				netp = asoc->primary_destination;
			}
			sctp_send_shutdown_ack(stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK,
			    stcb->sctp_ep, stcb, netp);
		}
	}
	/*********************************************/
	/* Here we perform PR-SCTP procedures        */
	/* (section 4.2)                             */
	/*********************************************/
	/* C1. update advancedPeerAckPoint */
	if (SCTP_TSN_GT(cumack, asoc->advanced_peer_ack_point)) {
		asoc->advanced_peer_ack_point = cumack;
	}
	/* PR-Sctp issues need to be addressed too */
	if ((asoc->prsctp_supported) && (asoc->pr_sctp_cnt > 0)) {
		struct sctp_tmit_chunk *lchk;
		uint32_t old_adv_peer_ack_point;

		old_adv_peer_ack_point = asoc->advanced_peer_ack_point;
		lchk = sctp_try_advance_peer_ack_point(stcb, asoc);
		/* C3. See if we need to send a Fwd-TSN */
		if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, cumack)) {
			/*
			 * ISSUE with ECN, see FWD-TSN processing.
			 */
			if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, old_adv_peer_ack_point)) {
				send_forward_tsn(stcb, asoc);
			} else if (lchk) {
				/* try to FR fwd-tsn's that get lost too */
				if (lchk->rec.data.fwd_tsn_cnt >= 3) {
					send_forward_tsn(stcb, asoc);
				}
			}
		}
		if (lchk) {
			/* Assure a timer is up */
			sctp_timer_start(SCTP_TIMER_TYPE_SEND,
			    stcb->sctp_ep, stcb, lchk->whoTo);
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_RWND_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_SACK_RWND_UPDATE,
		    rwnd,
		    stcb->asoc.peers_rwnd,
		    stcb->asoc.total_flight,
		    stcb->asoc.total_output_queue_size);
	}
}

void
sctp_handle_sack(struct mbuf *m, int offset_seg, int offset_dup,
    struct sctp_tcb *stcb,
    uint16_t num_seg, uint16_t num_nr_seg, uint16_t num_dup,
    int *abort_now, uint8_t flags,
    uint32_t cum_ack, uint32_t rwnd, int ecne_seen)
{
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *tp1, *tp2;
	uint32_t last_tsn, biggest_tsn_acked, biggest_tsn_newly_acked, this_sack_lowest_newack;
	uint16_t wake_him = 0;
	uint32_t send_s = 0;
	long j;
	int accum_moved = 0;
	int will_exit_fast_recovery = 0;
	uint32_t a_rwnd, old_rwnd;
	int win_probe_recovery = 0;
	int win_probe_recovered = 0;
	struct sctp_nets *net = NULL;
	int done_once;
	int rto_ok = 1;
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
	/* CMT DAC algo */
	this_sack_lowest_newack = 0;
	SCTP_STAT_INCR(sctps_slowpath_sack);
	last_tsn = cum_ack;
	cmt_dac_flag = flags & SCTP_SACK_CMT_DAC;
#ifdef SCTP_ASOCLOG_OF_TSNS
	stcb->asoc.cumack_log[stcb->asoc.cumack_log_at] = cum_ack;
	stcb->asoc.cumack_log_at++;
	if (stcb->asoc.cumack_log_at > SCTP_TSN_LOG_SIZE) {
		stcb->asoc.cumack_log_at = 0;
	}
#endif
	a_rwnd = rwnd;

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_SACK_ARRIVALS_ENABLE) {
		sctp_misc_ints(SCTP_SACK_LOG_NORMAL, cum_ack,
		    rwnd, stcb->asoc.last_acked_seq, stcb->asoc.peers_rwnd);
	}
	old_rwnd = stcb->asoc.peers_rwnd;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
		sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
		    stcb->asoc.overall_error_count,
		    0,
		    SCTP_FROM_SCTP_INDATA,
		    __LINE__);
	}
	stcb->asoc.overall_error_count = 0;
	asoc = &stcb->asoc;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
		sctp_log_sack(asoc->last_acked_seq,
		    cum_ack,
		    0,
		    num_seg,
		    num_dup,
		    SCTP_LOG_NEW_SACK);
	}
	if ((num_dup) && (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE)) {
		uint16_t i;
		uint32_t *dupdata, dblock;

		for (i = 0; i < num_dup; i++) {
			dupdata = (uint32_t *) sctp_m_getptr(m, offset_dup + i * sizeof(uint32_t),
			    sizeof(uint32_t), (uint8_t *) & dblock);
			if (dupdata == NULL) {
				break;
			}
			sctp_log_fr(*dupdata, 0, 0, SCTP_FR_DUPED);
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_strict_sacks)) {
		/* reality check */
		if (!TAILQ_EMPTY(&asoc->sent_queue)) {
			tp1 = TAILQ_LAST(&asoc->sent_queue,
			    sctpchunk_listhead);
			send_s = tp1->rec.data.TSN_seq + 1;
		} else {
			tp1 = NULL;
			send_s = asoc->sending_seq;
		}
		if (SCTP_TSN_GE(cum_ack, send_s)) {
			struct mbuf *op_err;
			char msg[SCTP_DIAG_INFO_LEN];

			/*
			 * no way, we have not even sent this TSN out yet.
			 * Peer is hopelessly messed up with us.
			 */
			SCTP_PRINTF("NEW cum_ack:%x send_s:%x is smaller or equal\n",
			    cum_ack, send_s);
			if (tp1) {
				SCTP_PRINTF("Got send_s from tsn:%x + 1 of tp1:%p\n",
				    tp1->rec.data.TSN_seq, (void *)tp1);
			}
	hopeless_peer:
			*abort_now = 1;
			/* XXX */
			snprintf(msg, sizeof(msg), "Cum ack %8.8x greater or equal than TSN %8.8x",
			    cum_ack, send_s);
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_27;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
			return;
		}
	}
	/**********************/
	/* 1) check the range */
	/**********************/
	if (SCTP_TSN_GT(asoc->last_acked_seq, last_tsn)) {
		/* acking something behind */
		return;
	}
	/* update the Rwnd of the peer */
	if (TAILQ_EMPTY(&asoc->sent_queue) &&
	    TAILQ_EMPTY(&asoc->send_queue) &&
	    (asoc->stream_queue_cnt == 0)) {
		/* nothing left on send/sent and strmq */
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
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
			    stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_28);
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
		if (SCTP_TSN_GT(cum_ack, net->cwr_window_tsn)) {
			/* Drag along the window_tsn for cwr's */
			net->cwr_window_tsn = cum_ack;
		}
		net->prev_cwnd = net->cwnd;
		net->net_ack = 0;
		net->net_ack2 = 0;

		/*
		 * CMT: Reset CUC and Fast recovery algo variables before
		 * SACK processing
		 */
		net->new_pseudo_cumack = 0;
		net->will_exit_fast_recovery = 0;
		if (stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack) {
			(*stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack) (stcb, net);
		}
	}
	/* process the new consecutive TSN first */
	TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
		if (SCTP_TSN_GE(last_tsn, tp1->rec.data.TSN_seq)) {
			if (tp1->sent != SCTP_DATAGRAM_UNSENT) {
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
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_CA,
							    tp1->whoTo->flight_size,
							    tp1->book_size,
							    (uintptr_t) tp1->whoTo,
							    tp1->rec.data.TSN_seq);
						}
						sctp_flight_size_decrease(tp1);
						sctp_total_flight_decrease(stcb, tp1);
						if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
							(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) (tp1->whoTo,
							    tp1);
						}
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
							if (rto_ok) {
								tp1->whoTo->RTO =
								    sctp_calculate_rto(stcb,
								    asoc, tp1->whoTo,
								    &tp1->sent_rcv_time,
								    sctp_align_safe_nocopy,
								    SCTP_RTT_FROM_DATA);
								rto_ok = 0;
							}
							if (tp1->whoTo->rto_needed == 0) {
								tp1->whoTo->rto_needed = 1;
							}
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


					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
						sctp_log_sack(asoc->last_acked_seq,
						    cum_ack,
						    tp1->rec.data.TSN_seq,
						    0,
						    0,
						    SCTP_LOG_TSN_ACKED);
					}
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
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
				if (tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
					tp1->sent = SCTP_DATAGRAM_ACKED;
				}
			}
		} else {
			break;
		}
	}
	biggest_tsn_newly_acked = biggest_tsn_acked = last_tsn;
	/* always set this up to cum-ack */
	asoc->this_sack_highest_gap = last_tsn;

	if ((num_seg > 0) || (num_nr_seg > 0)) {

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
		if (sctp_handle_segments(m, &offset_seg, stcb, asoc, last_tsn, &biggest_tsn_acked,
		    &biggest_tsn_newly_acked, &this_sack_lowest_newack,
		    num_seg, num_nr_seg, &rto_ok)) {
			wake_him++;
		}
		if (SCTP_BASE_SYSCTL(sctp_strict_sacks)) {
			/*
			 * validate the biggest_tsn_acked in the gap acks if
			 * strict adherence is wanted.
			 */
			if (SCTP_TSN_GE(biggest_tsn_acked, send_s)) {
				/*
				 * peer is either confused or we are under
				 * attack. We must abort.
				 */
				SCTP_PRINTF("Hopeless peer! biggest_tsn_acked:%x largest seq:%x\n",
				    biggest_tsn_acked, send_s);
				goto hopeless_peer;
			}
		}
	}
	/*******************************************/
	/* cancel ALL T3-send timer if accum moved */
	/*******************************************/
	if (asoc->sctp_cmt_on_off > 0) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->new_pseudo_cumack)
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net,
				    SCTP_FROM_SCTP_INDATA + SCTP_LOC_29);

		}
	} else {
		if (accum_moved) {
			TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_30);
			}
		}
	}
	/********************************************/
	/* drop the acked chunks from the sentqueue */
	/********************************************/
	asoc->last_acked_seq = cum_ack;

	TAILQ_FOREACH_SAFE(tp1, &asoc->sent_queue, sctp_next, tp2) {
		if (SCTP_TSN_GT(tp1->rec.data.TSN_seq, cum_ack)) {
			break;
		}
		if (tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
			if (asoc->strmout[tp1->rec.data.stream_number].chunks_on_queues > 0) {
				asoc->strmout[tp1->rec.data.stream_number].chunks_on_queues--;
#ifdef INVARIANTS
			} else {
				panic("No chunks on the queues for sid %u.", tp1->rec.data.stream_number);
#endif
			}
		}
		TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
		if (PR_SCTP_ENABLED(tp1->flags)) {
			if (asoc->pr_sctp_cnt != 0)
				asoc->pr_sctp_cnt--;
		}
		asoc->sent_queue_cnt--;
		if (tp1->data) {
			/* sa_ignore NO_NULL_CHK */
			sctp_free_bufspace(stcb, asoc, tp1, 1);
			sctp_m_freem(tp1->data);
			tp1->data = NULL;
			if (asoc->prsctp_supported && PR_SCTP_BUF_ENABLED(tp1->flags)) {
				asoc->sent_queue_cnt_removeable--;
			}
		}
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
			sctp_log_sack(asoc->last_acked_seq,
			    cum_ack,
			    tp1->rec.data.TSN_seq,
			    0,
			    0,
			    SCTP_LOG_FREE_SENT);
		}
		sctp_free_a_chunk(stcb, tp1, SCTP_SO_NOT_LOCKED);
		wake_him++;
	}
	if (TAILQ_EMPTY(&asoc->sent_queue) && (asoc->total_flight > 0)) {
#ifdef INVARIANTS
		panic("Warning flight size is postive and should be 0");
#else
		SCTP_PRINTF("Warning flight size incorrect should be 0 is %d\n",
		    asoc->total_flight);
#endif
		asoc->total_flight = 0;
	}
	/* sa_ignore NO_NULL_CHK */
	if ((wake_him) && (stcb->sctp_socket)) {
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		struct socket *so;

#endif
		SOCKBUF_LOCK(&stcb->sctp_socket->so_snd);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, wake_him, SCTP_WAKESND_FROM_SACK);
		}
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
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
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	} else {
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, wake_him, SCTP_NOWAKE_FROM_SACK);
		}
	}

	if (asoc->fast_retran_loss_recovery && accum_moved) {
		if (SCTP_TSN_GE(asoc->last_acked_seq, asoc->fast_recovery_tsn)) {
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

	if (num_seg) {
		sctp_check_for_revoked(stcb, asoc, cum_ack, biggest_tsn_acked);
		asoc->saw_sack_with_frags = 1;
	} else if (asoc->saw_sack_with_frags) {
		int cnt_revoked = 0;

		/* Peer revoked all dg's marked or acked */
		TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
			if (tp1->sent == SCTP_DATAGRAM_ACKED) {
				tp1->sent = SCTP_DATAGRAM_SENT;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
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
				 * flightsize, which is artificial, does not
				 * throttle the sender, we also increase the
				 * cwnd artificially.
				 */
				tp1->whoTo->cwnd += tp1->book_size;
				cnt_revoked++;
			}
		}
		if (cnt_revoked) {
			reneged_all = 1;
		}
		asoc->saw_sack_with_frags = 0;
	}
	if (num_nr_seg > 0)
		asoc->saw_sack_with_nr_frags = 1;
	else
		asoc->saw_sack_with_nr_frags = 0;

	/* JRS - Use the congestion control given in the CC module */
	if (ecne_seen == 0) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->net_ack2 > 0) {
				/*
				 * Karn's rule applies to clearing error
				 * count, this is optional.
				 */
				net->error_count = 0;
				if (!(net->dest_state & SCTP_ADDR_REACHABLE)) {
					/* addr came good */
					net->dest_state |= SCTP_ADDR_REACHABLE;
					sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
					    0, (void *)net, SCTP_SO_NOT_LOCKED);
				}
				if (net == stcb->asoc.primary_destination) {
					if (stcb->asoc.alternate) {
						/*
						 * release the alternate,
						 * primary is good
						 */
						sctp_free_remote_addr(stcb->asoc.alternate);
						stcb->asoc.alternate = NULL;
					}
				}
				if (net->dest_state & SCTP_ADDR_PF) {
					net->dest_state &= ~SCTP_ADDR_PF;
					sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT,
					    stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_31);
					sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, stcb->sctp_ep, stcb, net);
					asoc->cc_functions.sctp_cwnd_update_exit_pf(stcb, net);
					/* Done with this net */
					net->net_ack = 0;
				}
				/* restore any doubled timers */
				net->RTO = (net->lastsa >> SCTP_RTT_SHIFT) + net->lastsv;
				if (net->RTO < stcb->asoc.minrto) {
					net->RTO = stcb->asoc.minrto;
				}
				if (net->RTO > stcb->asoc.maxrto) {
					net->RTO = stcb->asoc.maxrto;
				}
			}
		}
		asoc->cc_functions.sctp_cwnd_update_after_sack(stcb, asoc, accum_moved, reneged_all, will_exit_fast_recovery);
	}
	if (TAILQ_EMPTY(&asoc->sent_queue)) {
		/* nothing left in-flight */
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			/* stop all timers */
			sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
			    stcb, net,
			    SCTP_FROM_SCTP_INDATA + SCTP_LOC_32);
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
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
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
				struct mbuf *op_err;

		abort_out_now:
				*abort_now = 1;
				/* XXX */
				op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_33;
				sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
				return;
			} else {
				struct sctp_nets *netp;

				if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
				SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
				sctp_stop_timers_for_shutdown(stcb);
				if (asoc->alternate) {
					netp = asoc->alternate;
				} else {
					netp = asoc->primary_destination;
				}
				sctp_send_shutdown(stcb, netp);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
				    stcb->sctp_ep, stcb, netp);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb, netp);
			}
			return;
		} else if ((SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED) &&
		    (asoc->stream_queue_cnt == 0)) {
			struct sctp_nets *netp;

			if (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT) {
				goto abort_out_now;
			}
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_ACK_SENT);
			SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
			sctp_stop_timers_for_shutdown(stcb);
			if (asoc->alternate) {
				netp = asoc->alternate;
			} else {
				netp = asoc->primary_destination;
			}
			sctp_send_shutdown_ack(stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK,
			    stcb->sctp_ep, stcb, netp);
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
	if ((asoc->sctp_cmt_on_off > 0) &&
	    SCTP_BASE_SYSCTL(sctp_cmt_use_dac) &&
	    (cmt_dac_flag == 0)) {
		this_sack_lowest_newack = cum_ack;
	}
	if ((num_seg > 0) || (num_nr_seg > 0)) {
		sctp_strike_gap_ack_chunks(stcb, asoc, biggest_tsn_acked,
		    biggest_tsn_newly_acked, this_sack_lowest_newack, accum_moved);
	}
	/* JRS - Use the congestion control given in the CC module */
	asoc->cc_functions.sctp_cwnd_update_after_fr(stcb, asoc);

	/* Now are we exiting loss recovery ? */
	if (will_exit_fast_recovery) {
		/* Ok, we must exit fast recovery */
		asoc->fast_retran_loss_recovery = 0;
	}
	if ((asoc->sat_t3_loss_recovery) &&
	    SCTP_TSN_GE(asoc->last_acked_seq, asoc->sat_t3_recovery_tsn)) {
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
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
		sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
		    asoc->peers_rwnd, asoc->total_flight, (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh)), a_rwnd);
	}
	asoc->peers_rwnd = sctp_sbspace_sub(a_rwnd,
	    (uint32_t) (asoc->total_flight + (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh))));
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
			win_probe_recovered = 1;
			/*-
			 * Find first chunk that was used with
			 * window probe and clear the event. Put
			 * it back into the send queue as if has
			 * not been sent.
			 */
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->window_probe) {
					sctp_window_probe_recovery(stcb, asoc, tp1);
					break;
				}
			}
		}
		if (net->flight_size) {
			j++;
			if (!SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_start(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net);
			}
			if (net->window_probe) {
				net->window_probe = 0;
			}
		} else {
			if (net->window_probe) {
				/*
				 * In window probes we must assure a timer
				 * is still running there
				 */
				if (!SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
					sctp_timer_start(SCTP_TIMER_TYPE_SEND,
					    stcb->sctp_ep, stcb, net);

				}
			} else if (SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, net,
				    SCTP_FROM_SCTP_INDATA + SCTP_LOC_34);
			}
		}
	}
	if ((j == 0) &&
	    (!TAILQ_EMPTY(&asoc->sent_queue)) &&
	    (asoc->sent_queue_retran_cnt == 0) &&
	    (win_probe_recovered == 0) &&
	    (done_once == 0)) {
		/*
		 * huh, this should not happen unless all packets are
		 * PR-SCTP and marked to skip of course.
		 */
		if (sctp_fs_audit(asoc)) {
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
					sctp_ucount_incr(asoc->sent_queue_retran_cnt);
				}
			}
		}
		done_once = 1;
		goto again;
	}
	/*********************************************/
	/* Here we perform PR-SCTP procedures        */
	/* (section 4.2)                             */
	/*********************************************/
	/* C1. update advancedPeerAckPoint */
	if (SCTP_TSN_GT(cum_ack, asoc->advanced_peer_ack_point)) {
		asoc->advanced_peer_ack_point = cum_ack;
	}
	/* C2. try to further move advancedPeerAckPoint ahead */
	if ((asoc->prsctp_supported) && (asoc->pr_sctp_cnt > 0)) {
		struct sctp_tmit_chunk *lchk;
		uint32_t old_adv_peer_ack_point;

		old_adv_peer_ack_point = asoc->advanced_peer_ack_point;
		lchk = sctp_try_advance_peer_ack_point(stcb, asoc);
		/* C3. See if we need to send a Fwd-TSN */
		if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, cum_ack)) {
			/*
			 * ISSUE with ECN, see FWD-TSN processing.
			 */
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_TRY_ADVANCE) {
				sctp_misc_ints(SCTP_FWD_TSN_CHECK,
				    0xee, cum_ack, asoc->advanced_peer_ack_point,
				    old_adv_peer_ack_point);
			}
			if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, old_adv_peer_ack_point)) {
				send_forward_tsn(stcb, asoc);
			} else if (lchk) {
				/* try to FR fwd-tsn's that get lost too */
				if (lchk->rec.data.fwd_tsn_cnt >= 3) {
					send_forward_tsn(stcb, asoc);
				}
			}
		}
		if (lchk) {
			/* Assure a timer is up */
			sctp_timer_start(SCTP_TIMER_TYPE_SEND,
			    stcb->sctp_ep, stcb, lchk->whoTo);
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_RWND_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_SACK_RWND_UPDATE,
		    a_rwnd,
		    stcb->asoc.peers_rwnd,
		    stcb->asoc.total_flight,
		    stcb->asoc.total_output_queue_size);
	}
}

void
sctp_update_acked(struct sctp_tcb *stcb, struct sctp_shutdown_chunk *cp, int *abort_flag)
{
	/* Copy cum-ack */
	uint32_t cum_ack, a_rwnd;

	cum_ack = ntohl(cp->cumulative_tsn_ack);
	/* Arrange so a_rwnd does NOT change */
	a_rwnd = stcb->asoc.peers_rwnd + stcb->asoc.total_flight;

	/* Now call the express sack handling */
	sctp_express_handle_sack(stcb, cum_ack, a_rwnd, abort_flag, 0);
}

static void
sctp_kick_prsctp_reorder_queue(struct sctp_tcb *stcb,
    struct sctp_stream_in *strmin)
{
	struct sctp_queued_to_read *ctl, *nctl;
	struct sctp_association *asoc;
	uint16_t tt;

	asoc = &stcb->asoc;
	tt = strmin->last_sequence_delivered;
	/*
	 * First deliver anything prior to and including the stream no that
	 * came in
	 */
	TAILQ_FOREACH_SAFE(ctl, &strmin->inqueue, next, nctl) {
		if (SCTP_SSN_GE(tt, ctl->sinfo_ssn)) {
			/* this is deliverable now */
			TAILQ_REMOVE(&strmin->inqueue, ctl, next);
			/* subtract pending on streams */
			asoc->size_on_all_streams -= ctl->length;
			sctp_ucount_decr(asoc->cnt_on_all_streams);
			/* deliver it to at least the delivery-q */
			if (stcb->sctp_socket) {
				sctp_mark_non_revokable(asoc, ctl->sinfo_tsn);
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				    ctl,
				    &stcb->sctp_socket->so_rcv, 1, SCTP_READ_LOCK_HELD, SCTP_SO_NOT_LOCKED);
			}
		} else {
			/* no more delivery now. */
			break;
		}
	}
	/*
	 * now we must deliver things in queue the normal way  if any are
	 * now ready.
	 */
	tt = strmin->last_sequence_delivered + 1;
	TAILQ_FOREACH_SAFE(ctl, &strmin->inqueue, next, nctl) {
		if (tt == ctl->sinfo_ssn) {
			/* this is deliverable now */
			TAILQ_REMOVE(&strmin->inqueue, ctl, next);
			/* subtract pending on streams */
			asoc->size_on_all_streams -= ctl->length;
			sctp_ucount_decr(asoc->cnt_on_all_streams);
			/* deliver it to at least the delivery-q */
			strmin->last_sequence_delivered = ctl->sinfo_ssn;
			if (stcb->sctp_socket) {
				sctp_mark_non_revokable(asoc, ctl->sinfo_tsn);
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				    ctl,
				    &stcb->sctp_socket->so_rcv, 1, SCTP_READ_LOCK_HELD, SCTP_SO_NOT_LOCKED);

			}
			tt = strmin->last_sequence_delivered + 1;
		} else {
			break;
		}
	}
}

static void
sctp_flush_reassm_for_str_seq(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    uint16_t stream, uint16_t seq)
{
	struct sctp_tmit_chunk *chk, *nchk;

	/* For each one on here see if we need to toss it */
	/*
	 * For now large messages held on the reasmqueue that are complete
	 * will be tossed too. We could in theory do more work to spin
	 * through and stop after dumping one msg aka seeing the start of a
	 * new msg at the head, and call the delivery function... to see if
	 * it can be delivered... But for now we just dump everything on the
	 * queue.
	 */
	TAILQ_FOREACH_SAFE(chk, &asoc->reasmqueue, sctp_next, nchk) {
		/*
		 * Do not toss it if on a different stream or marked for
		 * unordered delivery in which case the stream sequence
		 * number has no meaning.
		 */
		if ((chk->rec.data.stream_number != stream) ||
		    ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) == SCTP_DATA_UNORDERED)) {
			continue;
		}
		if (chk->rec.data.stream_seq == seq) {
			/* It needs to be tossed */
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			if (SCTP_TSN_GT(chk->rec.data.TSN_seq, asoc->tsn_last_delivered)) {
				asoc->tsn_last_delivered = chk->rec.data.TSN_seq;
				asoc->str_of_pdapi = chk->rec.data.stream_number;
				asoc->ssn_of_pdapi = chk->rec.data.stream_seq;
				asoc->fragment_flags = chk->rec.data.rcv_flags;
			}
			asoc->size_on_reasm_queue -= chk->send_size;
			sctp_ucount_decr(asoc->cnt_on_reasm_queue);

			/* Clear up any stream problem */
			if ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) != SCTP_DATA_UNORDERED &&
			    SCTP_SSN_GT(chk->rec.data.stream_seq, asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered)) {
				/*
				 * We must dump forward this streams
				 * sequence number if the chunk is not
				 * unordered that is being skipped. There is
				 * a chance that if the peer does not
				 * include the last fragment in its FWD-TSN
				 * we WILL have a problem here since you
				 * would have a partial chunk in queue that
				 * may not be deliverable. Also if a Partial
				 * delivery API as started the user may get
				 * a partial chunk. The next read returning
				 * a new chunk... really ugly but I see no
				 * way around it! Maybe a notify??
				 */
				asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered = chk->rec.data.stream_seq;
			}
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
		} else if (SCTP_SSN_GT(chk->rec.data.stream_seq, seq)) {
			/*
			 * If the stream_seq is > than the purging one, we
			 * are done
			 */
			break;
		}
	}
}


void
sctp_handle_forward_tsn(struct sctp_tcb *stcb,
    struct sctp_forward_tsn_chunk *fwd,
    int *abort_flag, struct mbuf *m, int offset)
{
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
	unsigned int i, fwd_sz, m_size;
	uint32_t str_seq;
	struct sctp_stream_in *strm;
	struct sctp_tmit_chunk *chk, *nchk;
	struct sctp_queued_to_read *ctl, *sv;

	asoc = &stcb->asoc;
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

	if (SCTP_TSN_GE(asoc->cumulative_tsn, new_cum_tsn)) {
		/* Already got there ... */
		return;
	}
	/*
	 * now we know the new TSN is more advanced, let's find the actual
	 * gap
	 */
	SCTP_CALC_TSN_TO_GAP(gap, new_cum_tsn, asoc->mapping_array_base_tsn);
	asoc->cumulative_tsn = new_cum_tsn;
	if (gap >= m_size) {
		if ((long)gap > sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv)) {
			struct mbuf *op_err;
			char msg[SCTP_DIAG_INFO_LEN];

			/*
			 * out of range (of single byte chunks in the rwnd I
			 * give out). This must be an attacker.
			 */
			*abort_flag = 1;
			snprintf(msg, sizeof(msg),
			    "New cum ack %8.8x too high, highest TSN %8.8x",
			    new_cum_tsn, asoc->highest_tsn_inside_map);
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_35;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, SCTP_SO_NOT_LOCKED);
			return;
		}
		SCTP_STAT_INCR(sctps_fwdtsn_map_over);

		memset(stcb->asoc.mapping_array, 0, stcb->asoc.mapping_array_size);
		asoc->mapping_array_base_tsn = new_cum_tsn + 1;
		asoc->highest_tsn_inside_map = new_cum_tsn;

		memset(stcb->asoc.nr_mapping_array, 0, stcb->asoc.mapping_array_size);
		asoc->highest_tsn_inside_nr_map = new_cum_tsn;

		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 3, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
	} else {
		SCTP_TCB_LOCK_ASSERT(stcb);
		for (i = 0; i <= gap; i++) {
			if (!SCTP_IS_TSN_PRESENT(asoc->mapping_array, i) &&
			    !SCTP_IS_TSN_PRESENT(asoc->nr_mapping_array, i)) {
				SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, i);
				if (SCTP_TSN_GT(asoc->mapping_array_base_tsn + i, asoc->highest_tsn_inside_nr_map)) {
					asoc->highest_tsn_inside_nr_map = asoc->mapping_array_base_tsn + i;
				}
			}
		}
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
	/* For each one on here see if we need to toss it */
	/*
	 * For now large messages held on the reasmqueue that are complete
	 * will be tossed too. We could in theory do more work to spin
	 * through and stop after dumping one msg aka seeing the start of a
	 * new msg at the head, and call the delivery function... to see if
	 * it can be delivered... But for now we just dump everything on the
	 * queue.
	 */
	TAILQ_FOREACH_SAFE(chk, &asoc->reasmqueue, sctp_next, nchk) {
		if (SCTP_TSN_GE(new_cum_tsn, chk->rec.data.TSN_seq)) {
			/* It needs to be tossed */
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			if (SCTP_TSN_GT(chk->rec.data.TSN_seq, asoc->tsn_last_delivered)) {
				asoc->tsn_last_delivered = chk->rec.data.TSN_seq;
				asoc->str_of_pdapi = chk->rec.data.stream_number;
				asoc->ssn_of_pdapi = chk->rec.data.stream_seq;
				asoc->fragment_flags = chk->rec.data.rcv_flags;
			}
			asoc->size_on_reasm_queue -= chk->send_size;
			sctp_ucount_decr(asoc->cnt_on_reasm_queue);

			/* Clear up any stream problem */
			if ((chk->rec.data.rcv_flags & SCTP_DATA_UNORDERED) != SCTP_DATA_UNORDERED &&
			    SCTP_SSN_GT(chk->rec.data.stream_seq, asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered)) {
				/*
				 * We must dump forward this streams
				 * sequence number if the chunk is not
				 * unordered that is being skipped. There is
				 * a chance that if the peer does not
				 * include the last fragment in its FWD-TSN
				 * we WILL have a problem here since you
				 * would have a partial chunk in queue that
				 * may not be deliverable. Also if a Partial
				 * delivery API as started the user may get
				 * a partial chunk. The next read returning
				 * a new chunk... really ugly but I see no
				 * way around it! Maybe a notify??
				 */
				asoc->strmin[chk->rec.data.stream_number].last_sequence_delivered = chk->rec.data.stream_seq;
			}
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
		} else {
			/*
			 * Ok we have gone beyond the end of the fwd-tsn's
			 * mark.
			 */
			break;
		}
	}
	/*******************************************************/
	/* 3. Update the PR-stream re-ordering queues and fix  */
	/* delivery issues as needed.                       */
	/*******************************************************/
	fwd_sz -= sizeof(*fwd);
	if (m && fwd_sz) {
		/* New method. */
		unsigned int num_str;
		struct sctp_strseq *stseq, strseqbuf;

		offset += sizeof(*fwd);

		SCTP_INP_READ_LOCK(stcb->sctp_ep);
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

			/*
			 * Ok we now look for the stream/seq on the read
			 * queue where its not all delivered. If we find it
			 * we transmute the read entry into a PDI_ABORTED.
			 */
			if (stseq->stream >= asoc->streamincnt) {
				/* screwed up streams, stop!  */
				break;
			}
			if ((asoc->str_of_pdapi == stseq->stream) &&
			    (asoc->ssn_of_pdapi == stseq->sequence)) {
				/*
				 * If this is the one we were partially
				 * delivering now then we no longer are.
				 * Note this will change with the reassembly
				 * re-write.
				 */
				asoc->fragmented_delivery_inprogress = 0;
			}
			sctp_flush_reassm_for_str_seq(stcb, asoc, stseq->stream, stseq->sequence);
			TAILQ_FOREACH(ctl, &stcb->sctp_ep->read_queue, next) {
				if ((ctl->sinfo_stream == stseq->stream) &&
				    (ctl->sinfo_ssn == stseq->sequence)) {
					str_seq = (stseq->stream << 16) | stseq->sequence;
					ctl->end_added = 1;
					ctl->pdapi_aborted = 1;
					sv = stcb->asoc.control_pdapi;
					stcb->asoc.control_pdapi = ctl;
					sctp_ulp_notify(SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION,
					    stcb,
					    SCTP_PARTIAL_DELIVERY_ABORTED,
					    (void *)&str_seq,
					    SCTP_SO_NOT_LOCKED);
					stcb->asoc.control_pdapi = sv;
					break;
				} else if ((ctl->sinfo_stream == stseq->stream) &&
				    SCTP_SSN_GT(ctl->sinfo_ssn, stseq->sequence)) {
					/* We are past our victim SSN */
					break;
				}
			}
			strm = &asoc->strmin[stseq->stream];
			if (SCTP_SSN_GT(stseq->sequence, strm->last_sequence_delivered)) {
				/* Update the sequence number */
				strm->last_sequence_delivered = stseq->sequence;
			}
			/* now kick the stream the new way */
			/* sa_ignore NO_NULL_CHK */
			sctp_kick_prsctp_reorder_queue(stcb, strm);
		}
		SCTP_INP_READ_UNLOCK(stcb->sctp_ep);
	}
	/*
	 * Now slide thing forward.
	 */
	sctp_slide_mapping_arrays(stcb);

	if (!TAILQ_EMPTY(&asoc->reasmqueue)) {
		/* now lets kick out and check for more fragmented delivery */
		/* sa_ignore NO_NULL_CHK */
		sctp_deliver_reasm_check(stcb, &stcb->asoc);
	}
}
