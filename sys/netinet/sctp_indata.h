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

/* $KAME: sctp_indata.h,v 1.9 2005/03/06 16:04:17 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/sctp_indata.h,v 1.9.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#ifndef __sctp_indata_h__
#define __sctp_indata_h__

#if defined(_KERNEL) || defined(__Userspace__)

struct sctp_queued_to_read *
sctp_build_readq_entry(struct sctp_tcb *stcb,
    struct sctp_nets *net,
    uint32_t tsn, uint32_t ppid,
    uint32_t context, uint16_t stream_no,
    uint16_t stream_seq, uint8_t flags,
    struct mbuf *dm);


#define sctp_build_readq_entry_mac(_ctl, in_it, a, net, tsn, ppid, context, stream_no, stream_seq, flags, dm) do { \
	if (_ctl) { \
		atomic_add_int(&((net)->ref_count), 1); \
		(_ctl)->sinfo_stream = stream_no; \
		(_ctl)->sinfo_ssn = stream_seq; \
		(_ctl)->sinfo_flags = (flags << 8); \
		(_ctl)->sinfo_ppid = ppid; \
		(_ctl)->sinfo_context = a; \
		(_ctl)->sinfo_timetolive = 0; \
		(_ctl)->sinfo_tsn = tsn; \
		(_ctl)->sinfo_cumtsn = tsn; \
		(_ctl)->sinfo_assoc_id = sctp_get_associd((in_it)); \
		(_ctl)->length = 0; \
		(_ctl)->held_length = 0; \
		(_ctl)->whoFrom = net; \
		(_ctl)->data = dm; \
		(_ctl)->tail_mbuf = NULL; \
	        (_ctl)->aux_data = NULL; \
		(_ctl)->stcb = (in_it); \
		(_ctl)->port_from = (in_it)->rport; \
		(_ctl)->spec_flags = 0; \
		(_ctl)->do_not_ref_stcb = 0; \
		(_ctl)->end_added = 0; \
		(_ctl)->pdapi_aborted = 0; \
		(_ctl)->some_taken = 0; \
	} \
} while (0)



struct mbuf *
sctp_build_ctl_nchunk(struct sctp_inpcb *inp,
    struct sctp_sndrcvinfo *sinfo);

char *
sctp_build_ctl_cchunk(struct sctp_inpcb *inp,
    int *control_len,
    struct sctp_sndrcvinfo *sinfo);

void sctp_set_rwnd(struct sctp_tcb *, struct sctp_association *);

uint32_t
sctp_calc_rwnd(struct sctp_tcb *stcb, struct sctp_association *asoc);

void
sctp_express_handle_sack(struct sctp_tcb *stcb, uint32_t cumack,
    uint32_t rwnd, int nonce_sum_flag, int *abort_now);

void
sctp_handle_sack(struct mbuf *m, int offset, struct sctp_sack_chunk *, struct sctp_tcb *,
    struct sctp_nets *, int *, int, uint32_t);

/* draft-ietf-tsvwg-usctp */
void
sctp_handle_forward_tsn(struct sctp_tcb *,
    struct sctp_forward_tsn_chunk *, int *, struct mbuf *, int);

struct sctp_tmit_chunk *
                sctp_try_advance_peer_ack_point(struct sctp_tcb *, struct sctp_association *);

void sctp_service_queues(struct sctp_tcb *, struct sctp_association *);

void
sctp_update_acked(struct sctp_tcb *, struct sctp_shutdown_chunk *,
    struct sctp_nets *, int *);

int
sctp_process_data(struct mbuf **, int, int *, int, struct sctphdr *,
    struct sctp_inpcb *, struct sctp_tcb *,
    struct sctp_nets *, uint32_t *);

void sctp_sack_check(struct sctp_tcb *, int, int, int *);

#endif
#endif
