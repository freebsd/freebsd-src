/*-
 * Copyright (c) 2010, by Randall Stewart & Michael Tuexen,
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netinet/sctp_pcb.h>

/*
 * Default simple round-robin algorithm.
 * Just interates the streams in the order they appear.
 */

static void
sctp_ss_default_add(struct sctp_tcb *, struct sctp_association *,
    struct sctp_stream_out *,
    struct sctp_stream_queue_pending *, int);

static void
sctp_ss_default_remove(struct sctp_tcb *, struct sctp_association *,
    struct sctp_stream_out *,
    struct sctp_stream_queue_pending *, int);

static void
sctp_ss_default_init(struct sctp_tcb *stcb, struct sctp_association *asoc,
    int holds_lock)
{
	uint16_t i;

	TAILQ_INIT(&asoc->ss_data.out_wheel);
	for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
		if (!TAILQ_EMPTY(&stcb->asoc.strmout[i].outqueue)) {
			sctp_ss_default_add(stcb, &stcb->asoc,
			    &stcb->asoc.strmout[i],
			    NULL, holds_lock);
		}
	}
	return;
}

static void
sctp_ss_default_clear(struct sctp_tcb *stcb, struct sctp_association *asoc,
    int clear_values, int holds_lock)
{
	uint16_t i;

	for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
		if (!TAILQ_EMPTY(&stcb->asoc.strmout[i].outqueue)) {
			sctp_ss_default_remove(stcb, &stcb->asoc,
			    &stcb->asoc.strmout[i],
			    NULL, holds_lock);
		}
	}
	return;
}

static void
sctp_ss_default_init_stream(struct sctp_stream_out *strq)
{
	strq->ss_params.rr.next_spoke.tqe_next = NULL;
	strq->ss_params.rr.next_spoke.tqe_prev = NULL;
	return;
}

static void
sctp_ss_default_add(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq,
    struct sctp_stream_queue_pending *sp, int holds_lock)
{
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if ((strq->ss_params.rr.next_spoke.tqe_next == NULL) &&
	    (strq->ss_params.rr.next_spoke.tqe_prev == NULL)) {
		TAILQ_INSERT_TAIL(&asoc->ss_data.out_wheel,
		    strq, ss_params.rr.next_spoke);
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static int
sctp_ss_default_is_empty(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	if (TAILQ_EMPTY(&asoc->ss_data.out_wheel)) {
		return (1);
	} else {
		return (0);
	}
}

static void
sctp_ss_default_remove(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq,
    struct sctp_stream_queue_pending *sp, int holds_lock)
{
	/* take off and then setup so we know it is not on the wheel */
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if (TAILQ_EMPTY(&strq->outqueue)) {
		if (asoc->last_out_stream == strq) {
			asoc->last_out_stream = TAILQ_PREV(asoc->last_out_stream,
			    sctpwheel_listhead,
			    ss_params.rr.next_spoke);
			if (asoc->last_out_stream == NULL) {
				asoc->last_out_stream = TAILQ_LAST(&asoc->ss_data.out_wheel,
				    sctpwheel_listhead);
			}
			if (asoc->last_out_stream == strq) {
				asoc->last_out_stream = NULL;
			}
		}
		TAILQ_REMOVE(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
		strq->ss_params.rr.next_spoke.tqe_next = NULL;
		strq->ss_params.rr.next_spoke.tqe_prev = NULL;
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}


static struct sctp_stream_out *
sctp_ss_default_select(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	struct sctp_stream_out *strq, *strqt;

	strqt = asoc->last_out_stream;
default_again:
	/* Find the next stream to use */
	if (strqt == NULL) {
		strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
	} else {
		strq = TAILQ_NEXT(strqt, ss_params.rr.next_spoke);
		if (strq == NULL) {
			strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
		}
	}

	/*
	 * If CMT is off, we must validate that the stream in question has
	 * the first item pointed towards are network destionation requested
	 * by the caller. Note that if we turn out to be locked to a stream
	 * (assigning TSN's then we must stop, since we cannot look for
	 * another stream with data to send to that destination). In CMT's
	 * case, by skipping this check, we will send one data packet
	 * towards the requested net.
	 */
	if (net != NULL && strq != NULL &&
	    SCTP_BASE_SYSCTL(sctp_cmt_on_off) == 0) {
		if (TAILQ_FIRST(&strq->outqueue) &&
		    TAILQ_FIRST(&strq->outqueue)->net != NULL &&
		    TAILQ_FIRST(&strq->outqueue)->net != net) {
			if (strq == asoc->last_out_stream) {
				return (NULL);
			} else {
				strqt = strq;
				goto default_again;
			}
		}
	}
	return (strq);
}

static void
sctp_ss_default_scheduled(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc,
    struct sctp_stream_out *strq, int moved_how_much)
{
	asoc->last_out_stream = strq;
	return;
}

static void
sctp_ss_default_packet_done(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	/* Nothing to be done here */
	return;
}

static int
sctp_ss_default_get_value(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, uint16_t * value)
{
	/* Nothing to be done here */
	return (-1);
}

static int
sctp_ss_default_set_value(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, uint16_t value)
{
	/* Nothing to be done here */
	return (-1);
}

/*
 * Real round-robin algorithm.
 * Always interates the streams in ascending order.
 */
static void
sctp_ss_rr_add(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq,
    struct sctp_stream_queue_pending *sp, int holds_lock)
{
	struct sctp_stream_out *strqt;

	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if ((strq->ss_params.rr.next_spoke.tqe_next == NULL) &&
	    (strq->ss_params.rr.next_spoke.tqe_prev == NULL)) {
		if (TAILQ_EMPTY(&asoc->ss_data.out_wheel)) {
			TAILQ_INSERT_HEAD(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
		} else {
			strqt = TAILQ_FIRST(&asoc->ss_data.out_wheel);
			while (strqt != NULL && (strqt->stream_no < strq->stream_no)) {
				strqt = TAILQ_NEXT(strqt, ss_params.rr.next_spoke);
			}
			if (strqt != NULL) {
				TAILQ_INSERT_BEFORE(strqt, strq, ss_params.rr.next_spoke);
			} else {
				TAILQ_INSERT_TAIL(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
			}
		}
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

/*
 * Real round-robin per packet algorithm.
 * Always interates the streams in ascending order and
 * only fills messages of the same stream in a packet.
 */
static void
sctp_ss_rrp_add(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq,
    struct sctp_stream_queue_pending *sp, int holds_lock)
{
	struct sctp_stream_out *strqt;

	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if ((strq->ss_params.rr.next_spoke.tqe_next == NULL) &&
	    (strq->ss_params.rr.next_spoke.tqe_prev == NULL)) {

		if (TAILQ_EMPTY(&asoc->ss_data.out_wheel)) {
			TAILQ_INSERT_HEAD(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
		} else {
			strqt = TAILQ_FIRST(&asoc->ss_data.out_wheel);
			while (strqt != NULL && strqt->stream_no < strq->stream_no) {
				strqt = TAILQ_NEXT(strqt, ss_params.rr.next_spoke);
			}
			if (strqt != NULL) {
				TAILQ_INSERT_BEFORE(strqt, strq, ss_params.rr.next_spoke);
			} else {
				TAILQ_INSERT_TAIL(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
			}
		}
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static struct sctp_stream_out *
sctp_ss_rrp_select(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	struct sctp_stream_out *strq, *strqt;

	strqt = asoc->last_out_stream;
	if (strqt != NULL && !TAILQ_EMPTY(&strqt->outqueue)) {
		return (strqt);
	}
rrp_again:
	/* Find the next stream to use */
	if (strqt == NULL) {
		strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
	} else {
		strq = TAILQ_NEXT(strqt, ss_params.rr.next_spoke);
		if (strq == NULL) {
			strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
		}
	}

	/*
	 * If CMT is off, we must validate that the stream in question has
	 * the first item pointed towards are network destionation requested
	 * by the caller. Note that if we turn out to be locked to a stream
	 * (assigning TSN's then we must stop, since we cannot look for
	 * another stream with data to send to that destination). In CMT's
	 * case, by skipping this check, we will send one data packet
	 * towards the requested net.
	 */
	if (net != NULL && strq != NULL &&
	    SCTP_BASE_SYSCTL(sctp_cmt_on_off) == 0) {
		if (TAILQ_FIRST(&strq->outqueue) &&
		    TAILQ_FIRST(&strq->outqueue)->net != NULL &&
		    TAILQ_FIRST(&strq->outqueue)->net != net) {
			if (strq == asoc->last_out_stream) {
				return (NULL);
			} else {
				strqt = strq;
				goto rrp_again;
			}
		}
	}
	return (strq);
}

static void
sctp_ss_rrp_packet_done(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	struct sctp_stream_out *strq, *strqt;

	strqt = asoc->last_out_stream;
rrp_pd_again:
	/* Find the next stream to use */
	if (strqt == NULL) {
		strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
	} else {
		strq = TAILQ_NEXT(strqt, ss_params.rr.next_spoke);
		if (strq == NULL) {
			strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
		}
	}

	/*
	 * If CMT is off, we must validate that the stream in question has
	 * the first item pointed towards are network destionation requested
	 * by the caller. Note that if we turn out to be locked to a stream
	 * (assigning TSN's then we must stop, since we cannot look for
	 * another stream with data to send to that destination). In CMT's
	 * case, by skipping this check, we will send one data packet
	 * towards the requested net.
	 */
	if ((strq != NULL) && TAILQ_FIRST(&strq->outqueue) &&
	    (net != NULL && TAILQ_FIRST(&strq->outqueue)->net != net) &&
	    (SCTP_BASE_SYSCTL(sctp_cmt_on_off) == 0)) {
		if (strq == asoc->last_out_stream) {
			strq = NULL;
		} else {
			strqt = strq;
			goto rrp_pd_again;
		}
	}
	asoc->last_out_stream = strq;
	return;
}


/*
 * Priority algorithm.
 * Always prefers streams based on their priority id.
 */
static void
sctp_ss_prio_clear(struct sctp_tcb *stcb, struct sctp_association *asoc,
    int clear_values, int holds_lock)
{
	uint16_t i;

	for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
		if (!TAILQ_EMPTY(&stcb->asoc.strmout[i].outqueue)) {
			if (clear_values)
				stcb->asoc.strmout[i].ss_params.prio.priority = 0;
			sctp_ss_default_remove(stcb, &stcb->asoc, &stcb->asoc.strmout[i], NULL, holds_lock);
		}
	}
	return;
}

static void
sctp_ss_prio_init_stream(struct sctp_stream_out *strq)
{
	strq->ss_params.prio.next_spoke.tqe_next = NULL;
	strq->ss_params.prio.next_spoke.tqe_prev = NULL;
	strq->ss_params.prio.priority = 0;
	return;
}

static void
sctp_ss_prio_add(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp,
    int holds_lock)
{
	struct sctp_stream_out *strqt;

	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if ((strq->ss_params.prio.next_spoke.tqe_next == NULL) &&
	    (strq->ss_params.prio.next_spoke.tqe_prev == NULL)) {

		if (TAILQ_EMPTY(&asoc->ss_data.out_wheel)) {
			TAILQ_INSERT_HEAD(&asoc->ss_data.out_wheel, strq, ss_params.prio.next_spoke);
		} else {
			strqt = TAILQ_FIRST(&asoc->ss_data.out_wheel);
			while (strqt != NULL && strqt->ss_params.prio.priority < strq->ss_params.prio.priority) {
				strqt = TAILQ_NEXT(strqt, ss_params.prio.next_spoke);
			}
			if (strqt != NULL) {
				TAILQ_INSERT_BEFORE(strqt, strq, ss_params.prio.next_spoke);
			} else {
				TAILQ_INSERT_TAIL(&asoc->ss_data.out_wheel, strq, ss_params.prio.next_spoke);
			}
		}
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static void
sctp_ss_prio_remove(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp,
    int holds_lock)
{
	/* take off and then setup so we know it is not on the wheel */
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if (TAILQ_EMPTY(&strq->outqueue)) {
		if (asoc->last_out_stream == strq) {
			asoc->last_out_stream = TAILQ_PREV(asoc->last_out_stream, sctpwheel_listhead,
			    ss_params.prio.next_spoke);
			if (asoc->last_out_stream == NULL) {
				asoc->last_out_stream = TAILQ_LAST(&asoc->ss_data.out_wheel,
				    sctpwheel_listhead);
			}
			if (asoc->last_out_stream == strq) {
				asoc->last_out_stream = NULL;
			}
		}
		TAILQ_REMOVE(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
		strq->ss_params.prio.next_spoke.tqe_next = NULL;
		strq->ss_params.prio.next_spoke.tqe_prev = NULL;
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static struct sctp_stream_out *
sctp_ss_prio_select(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	struct sctp_stream_out *strq, *strqt, *strqn;

	strqt = asoc->last_out_stream;
prio_again:
	/* Find the next stream to use */
	if (strqt == NULL) {
		strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
	} else {
		strqn = TAILQ_NEXT(strqt, ss_params.prio.next_spoke);
		if (strqn != NULL &&
		    strqn->ss_params.prio.priority == strqt->ss_params.prio.priority) {
			strq = TAILQ_NEXT(strqt, ss_params.prio.next_spoke);
		} else {
			strq = TAILQ_FIRST(&asoc->ss_data.out_wheel);
		}
	}

	/*
	 * If CMT is off, we must validate that the stream in question has
	 * the first item pointed towards are network destionation requested
	 * by the caller. Note that if we turn out to be locked to a stream
	 * (assigning TSN's then we must stop, since we cannot look for
	 * another stream with data to send to that destination). In CMT's
	 * case, by skipping this check, we will send one data packet
	 * towards the requested net.
	 */
	if (net != NULL && strq != NULL &&
	    SCTP_BASE_SYSCTL(sctp_cmt_on_off) == 0) {
		if (TAILQ_FIRST(&strq->outqueue) &&
		    TAILQ_FIRST(&strq->outqueue)->net != NULL &&
		    TAILQ_FIRST(&strq->outqueue)->net != net) {
			if (strq == asoc->last_out_stream) {
				return (NULL);
			} else {
				strqt = strq;
				goto prio_again;
			}
		}
	}
	return (strq);
}

static int
sctp_ss_prio_get_value(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, uint16_t * value)
{
	if (strq == NULL) {
		return (-1);
	}
	*value = strq->ss_params.prio.priority;
	return (1);
}

static int
sctp_ss_prio_set_value(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, uint16_t value)
{
	if (strq == NULL) {
		return (-1);
	}
	strq->ss_params.prio.priority = value;
	sctp_ss_prio_remove(stcb, asoc, strq, NULL, 1);
	sctp_ss_prio_add(stcb, asoc, strq, NULL, 1);
	return (1);
}

/*
 * Fair bandwidth algorithm.
 * Maintains an equal troughput per stream.
 */
static void
sctp_ss_fb_clear(struct sctp_tcb *stcb, struct sctp_association *asoc,
    int clear_values, int holds_lock)
{
	uint16_t i;

	for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
		if (!TAILQ_EMPTY(&stcb->asoc.strmout[i].outqueue)) {
			if (clear_values) {
				stcb->asoc.strmout[i].ss_params.fb.rounds = -1;
			}
			sctp_ss_default_remove(stcb, &stcb->asoc, &stcb->asoc.strmout[i], NULL, holds_lock);
		}
	}
	return;
}

static void
sctp_ss_fb_init_stream(struct sctp_stream_out *strq)
{
	strq->ss_params.fb.next_spoke.tqe_next = NULL;
	strq->ss_params.fb.next_spoke.tqe_prev = NULL;
	strq->ss_params.fb.rounds = -1;
	return;
}

static void
sctp_ss_fb_add(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp,
    int holds_lock)
{
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if ((strq->ss_params.rr.next_spoke.tqe_next == NULL) &&
	    (strq->ss_params.rr.next_spoke.tqe_prev == NULL)) {
		if (!TAILQ_EMPTY(&strq->outqueue) && strq->ss_params.fb.rounds < 0)
			strq->ss_params.fb.rounds = TAILQ_FIRST(&strq->outqueue)->length;
		TAILQ_INSERT_TAIL(&asoc->ss_data.out_wheel, strq, ss_params.rr.next_spoke);
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static void
sctp_ss_fb_remove(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp,
    int holds_lock)
{
	/* take off and then setup so we know it is not on the wheel */
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if (TAILQ_EMPTY(&strq->outqueue)) {
		if (asoc->last_out_stream == strq) {
			asoc->last_out_stream = TAILQ_PREV(asoc->last_out_stream, sctpwheel_listhead,
			    ss_params.fb.next_spoke);
			if (asoc->last_out_stream == NULL) {
				asoc->last_out_stream = TAILQ_LAST(&asoc->ss_data.out_wheel,
				    sctpwheel_listhead);
			}
			if (asoc->last_out_stream == strq) {
				asoc->last_out_stream = NULL;
			}
		}
		strq->ss_params.fb.rounds = -1;
		TAILQ_REMOVE(&asoc->ss_data.out_wheel, strq, ss_params.fb.next_spoke);
		strq->ss_params.fb.next_spoke.tqe_next = NULL;
		strq->ss_params.fb.next_spoke.tqe_prev = NULL;
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static struct sctp_stream_out *
sctp_ss_fb_select(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	struct sctp_stream_out *strq = NULL, *strqt;

	if (TAILQ_FIRST(&asoc->ss_data.out_wheel) == TAILQ_LAST(&asoc->ss_data.out_wheel, sctpwheel_listhead)) {
		strqt = TAILQ_FIRST(&asoc->ss_data.out_wheel);
	} else {
		if (asoc->last_out_stream != NULL) {
			strqt = TAILQ_NEXT(asoc->last_out_stream, ss_params.fb.next_spoke);
		} else {
			strqt = TAILQ_FIRST(&asoc->ss_data.out_wheel);
		}
	}
	do {
		if ((strqt != NULL) && TAILQ_FIRST(&strqt->outqueue) &&
		    TAILQ_FIRST(&strqt->outqueue)->net != NULL &&
		    ((net == NULL || TAILQ_FIRST(&strqt->outqueue)->net == net) ||
		    (SCTP_BASE_SYSCTL(sctp_cmt_on_off) > 0))) {
			if ((strqt->ss_params.fb.rounds >= 0) && (strq == NULL ||
			    strqt->ss_params.fb.rounds < strq->ss_params.fb.rounds)) {
				strq = strqt;
			}
		}
		if (strqt != NULL) {
			strqt = TAILQ_NEXT(strqt, ss_params.fb.next_spoke);
		} else {
			strqt = TAILQ_FIRST(&asoc->ss_data.out_wheel);
		}
	} while (strqt != strq);
	return (strq);
}

static void
sctp_ss_fb_scheduled(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc, struct sctp_stream_out *strq,
    int moved_how_much)
{
	struct sctp_stream_out *strqt;
	int subtract;

	subtract = strq->ss_params.fb.rounds;
	TAILQ_FOREACH(strqt, &asoc->ss_data.out_wheel, ss_params.fb.next_spoke) {
		strqt->ss_params.fb.rounds -= subtract;
		if (strqt->ss_params.fb.rounds < 0)
			strqt->ss_params.fb.rounds = 0;
	}
	if (TAILQ_FIRST(&strq->outqueue)) {
		strq->ss_params.fb.rounds = TAILQ_FIRST(&strq->outqueue)->length;
	} else {
		strq->ss_params.fb.rounds = -1;
	}
	asoc->last_out_stream = strq;
	return;
}

/*
 * First-come, first-serve algorithm.
 * Maintains the order provided by the application.
 */
static void
sctp_ss_fcfs_init(struct sctp_tcb *stcb, struct sctp_association *asoc,
    int holds_lock)
{
	int x, element = 0, add_more = 1;
	struct sctp_stream_queue_pending *sp;
	uint16_t i;

	TAILQ_INIT(&asoc->ss_data.out_list);
	while (add_more) {
		add_more = 0;
		for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
			sp = TAILQ_FIRST(&asoc->ss_data.out_list);
			x = element;
			while (sp != NULL && x > 0) {
				sp = TAILQ_NEXT(sp, next);
			}
			if (sp != NULL) {
				sctp_ss_default_add(stcb, &stcb->asoc, &stcb->asoc.strmout[i], NULL, holds_lock);
				add_more = 1;
			}
		}
		element++;
	}
	return;
}

static void
sctp_ss_fcfs_clear(struct sctp_tcb *stcb, struct sctp_association *asoc,
    int clear_values, int holds_lock)
{
	if (clear_values) {
		while (!TAILQ_EMPTY(&asoc->ss_data.out_list)) {
			TAILQ_REMOVE(&asoc->ss_data.out_list, TAILQ_FIRST(&asoc->ss_data.out_list), next);
		}
	}
	return;
}

static void
sctp_ss_fcfs_init_stream(struct sctp_stream_out *strq)
{
	/* Nothing to be done here */
	return;
}

static void
sctp_ss_fcfs_add(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp,
    int holds_lock)
{
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if (sp && (sp->next.tqe_next == NULL) &&
	    (sp->next.tqe_prev == NULL)) {
		TAILQ_INSERT_TAIL(&asoc->ss_data.out_list, sp, next);
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}

static int
sctp_ss_fcfs_is_empty(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	if (TAILQ_EMPTY(&asoc->ss_data.out_list)) {
		return (1);
	} else {
		return (0);
	}
}

static void
sctp_ss_fcfs_remove(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp,
    int holds_lock)
{
	if (holds_lock == 0) {
		SCTP_TCB_SEND_LOCK(stcb);
	}
	if (sp &&
	    ((sp->next.tqe_next != NULL) ||
	    (sp->next.tqe_prev != NULL))) {
		TAILQ_REMOVE(&asoc->ss_data.out_list, sp, next);
	}
	if (holds_lock == 0) {
		SCTP_TCB_SEND_UNLOCK(stcb);
	}
	return;
}


static struct sctp_stream_out *
sctp_ss_fcfs_select(struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sctp_association *asoc)
{
	struct sctp_stream_out *strq;
	struct sctp_stream_queue_pending *sp;

	sp = TAILQ_FIRST(&asoc->ss_data.out_list);
default_again:
	if (sp != NULL) {
		strq = &asoc->strmout[sp->stream];
	} else {
		strq = NULL;
	}

	/*
	 * If CMT is off, we must validate that the stream in question has
	 * the first item pointed towards are network destionation requested
	 * by the caller. Note that if we turn out to be locked to a stream
	 * (assigning TSN's then we must stop, since we cannot look for
	 * another stream with data to send to that destination). In CMT's
	 * case, by skipping this check, we will send one data packet
	 * towards the requested net.
	 */
	if (net != NULL && strq != NULL &&
	    SCTP_BASE_SYSCTL(sctp_cmt_on_off) == 0) {
		if (TAILQ_FIRST(&strq->outqueue) &&
		    TAILQ_FIRST(&strq->outqueue)->net != NULL &&
		    TAILQ_FIRST(&strq->outqueue)->net != net) {
			sp = TAILQ_NEXT(sp, next);
			goto default_again;
		}
	}
	return (strq);
}

struct sctp_ss_functions sctp_ss_functions[] = {
/* SCTP_SS_DEFAULT */
	{
		.sctp_ss_init = sctp_ss_default_init,
		.sctp_ss_clear = sctp_ss_default_clear,
		.sctp_ss_init_stream = sctp_ss_default_init_stream,
		.sctp_ss_add_to_stream = sctp_ss_default_add,
		.sctp_ss_is_empty = sctp_ss_default_is_empty,
		.sctp_ss_remove_from_stream = sctp_ss_default_remove,
		.sctp_ss_select_stream = sctp_ss_default_select,
		.sctp_ss_scheduled = sctp_ss_default_scheduled,
		.sctp_ss_packet_done = sctp_ss_default_packet_done,
		.sctp_ss_get_value = sctp_ss_default_get_value,
		.sctp_ss_set_value = sctp_ss_default_set_value
	},
/* SCTP_SS_ROUND_ROBIN */
	{
		.sctp_ss_init = sctp_ss_default_init,
		.sctp_ss_clear = sctp_ss_default_clear,
		.sctp_ss_init_stream = sctp_ss_default_init_stream,
		.sctp_ss_add_to_stream = sctp_ss_rr_add,
		.sctp_ss_is_empty = sctp_ss_default_is_empty,
		.sctp_ss_remove_from_stream = sctp_ss_default_remove,
		.sctp_ss_select_stream = sctp_ss_default_select,
		.sctp_ss_scheduled = sctp_ss_default_scheduled,
		.sctp_ss_packet_done = sctp_ss_default_packet_done,
		.sctp_ss_get_value = sctp_ss_default_get_value,
		.sctp_ss_set_value = sctp_ss_default_set_value
	},
/* SCTP_SS_ROUND_ROBIN_PACKET */
	{
		.sctp_ss_init = sctp_ss_default_init,
		.sctp_ss_clear = sctp_ss_default_clear,
		.sctp_ss_init_stream = sctp_ss_default_init_stream,
		.sctp_ss_add_to_stream = sctp_ss_rrp_add,
		.sctp_ss_is_empty = sctp_ss_default_is_empty,
		.sctp_ss_remove_from_stream = sctp_ss_default_remove,
		.sctp_ss_select_stream = sctp_ss_rrp_select,
		.sctp_ss_scheduled = sctp_ss_default_scheduled,
		.sctp_ss_packet_done = sctp_ss_rrp_packet_done,
		.sctp_ss_get_value = sctp_ss_default_get_value,
		.sctp_ss_set_value = sctp_ss_default_set_value
	},
/* SCTP_SS_PRIORITY */
	{
		.sctp_ss_init = sctp_ss_default_init,
		.sctp_ss_clear = sctp_ss_prio_clear,
		.sctp_ss_init_stream = sctp_ss_prio_init_stream,
		.sctp_ss_add_to_stream = sctp_ss_prio_add,
		.sctp_ss_is_empty = sctp_ss_default_is_empty,
		.sctp_ss_remove_from_stream = sctp_ss_prio_remove,
		.sctp_ss_select_stream = sctp_ss_prio_select,
		.sctp_ss_scheduled = sctp_ss_default_scheduled,
		.sctp_ss_packet_done = sctp_ss_default_packet_done,
		.sctp_ss_get_value = sctp_ss_prio_get_value,
		.sctp_ss_set_value = sctp_ss_prio_set_value
	},
/* SCTP_SS_FAIR_BANDWITH */
	{
		.sctp_ss_init = sctp_ss_default_init,
		.sctp_ss_clear = sctp_ss_fb_clear,
		.sctp_ss_init_stream = sctp_ss_fb_init_stream,
		.sctp_ss_add_to_stream = sctp_ss_fb_add,
		.sctp_ss_is_empty = sctp_ss_default_is_empty,
		.sctp_ss_remove_from_stream = sctp_ss_fb_remove,
		.sctp_ss_select_stream = sctp_ss_fb_select,
		.sctp_ss_scheduled = sctp_ss_fb_scheduled,
		.sctp_ss_packet_done = sctp_ss_default_packet_done,
		.sctp_ss_get_value = sctp_ss_default_get_value,
		.sctp_ss_set_value = sctp_ss_default_set_value
	},
/* SCTP_SS_FIRST_COME */
	{
		.sctp_ss_init = sctp_ss_fcfs_init,
		.sctp_ss_clear = sctp_ss_fcfs_clear,
		.sctp_ss_init_stream = sctp_ss_fcfs_init_stream,
		.sctp_ss_add_to_stream = sctp_ss_fcfs_add,
		.sctp_ss_is_empty = sctp_ss_fcfs_is_empty,
		.sctp_ss_remove_from_stream = sctp_ss_fcfs_remove,
		.sctp_ss_select_stream = sctp_ss_fcfs_select,
		.sctp_ss_scheduled = sctp_ss_default_scheduled,
		.sctp_ss_packet_done = sctp_ss_default_packet_done,
		.sctp_ss_get_value = sctp_ss_default_get_value,
		.sctp_ss_set_value = sctp_ss_default_set_value
	}
};
