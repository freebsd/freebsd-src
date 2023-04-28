/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Ng Peng Nam Sean
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_netlink.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/ck.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_linux.h>
#include <netlink/netlink_var.h>

#define	DEBUG_MOD_NAME	nl_io
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG);

/*
 * The logic below provide a p2p interface for receiving and
 * sending netlink data between the kernel and userland.
 */

static const struct sockaddr_nl _nl_empty_src = {
	.nl_len = sizeof(struct sockaddr_nl),
	.nl_family = PF_NETLINK,
	.nl_pid = 0 /* comes from the kernel */
};
static const struct sockaddr *nl_empty_src = (const struct sockaddr *)&_nl_empty_src;

static struct mbuf *nl_process_mbuf(struct mbuf *m, struct nlpcb *nlp);


static void
queue_push(struct nl_io_queue *q, struct mbuf *mq)
{
	while (mq != NULL) {
		struct mbuf *m = mq;
		mq = mq->m_nextpkt;
		m->m_nextpkt = NULL;

		q->length += m_length(m, NULL);
		STAILQ_INSERT_TAIL(&q->head, m, m_stailqpkt);
	}
}

static void
queue_push_head(struct nl_io_queue *q, struct mbuf *m)
{
	MPASS(m->m_nextpkt == NULL);

	q->length += m_length(m, NULL);
	STAILQ_INSERT_HEAD(&q->head, m, m_stailqpkt);
}

static struct mbuf *
queue_pop(struct nl_io_queue *q)
{
	if (!STAILQ_EMPTY(&q->head)) {
		struct mbuf *m = STAILQ_FIRST(&q->head);
		STAILQ_REMOVE_HEAD(&q->head, m_stailqpkt);
		m->m_nextpkt = NULL;
		q->length -= m_length(m, NULL);

		return (m);
	}
	return (NULL);
}

static struct mbuf *
queue_head(const struct nl_io_queue *q)
{
	return (STAILQ_FIRST(&q->head));
}

static inline bool
queue_empty(const struct nl_io_queue *q)
{
	return (q->length == 0);
}

static void
queue_free(struct nl_io_queue *q)
{
	while (!STAILQ_EMPTY(&q->head)) {
		struct mbuf *m = STAILQ_FIRST(&q->head);
		STAILQ_REMOVE_HEAD(&q->head, m_stailqpkt);
		m->m_nextpkt = NULL;
		m_freem(m);
	}
	q->length = 0;
}

void
nl_add_msg_info(struct mbuf *m)
{
	struct nlpcb *nlp = nl_get_thread_nlp(curthread);
	NL_LOG(LOG_DEBUG2, "Trying to recover nlp from thread %p: %p",
	    curthread, nlp);

	if (nlp == NULL)
		return;

	/* Prepare what we want to encode - PID, socket PID & msg seq */
	struct {
		struct nlattr nla;
		uint32_t val;
	} data[] = {
		{
			.nla.nla_len = sizeof(struct nlattr) + sizeof(uint32_t),
			.nla.nla_type = NLMSGINFO_ATTR_PROCESS_ID,
			.val = nlp->nl_process_id,
		},
		{
			.nla.nla_len = sizeof(struct nlattr) + sizeof(uint32_t),
			.nla.nla_type = NLMSGINFO_ATTR_PORT_ID,
			.val = nlp->nl_port,
		},
	};


	while (m->m_next != NULL)
		m = m->m_next;
	m->m_next = sbcreatecontrol(data, sizeof(data),
	    NETLINK_MSG_INFO, SOL_NETLINK, M_NOWAIT);

	NL_LOG(LOG_DEBUG2, "Storing %u bytes of data, ctl: %p",
	    (unsigned)sizeof(data), m->m_next);
}

static __noinline struct mbuf *
extract_msg_info(struct mbuf *m)
{
	while (m->m_next != NULL) {
		if (m->m_next->m_type == MT_CONTROL) {
			struct mbuf *ctl = m->m_next;
			m->m_next = NULL;
			return (ctl);
		}
		m = m->m_next;
	}
	return (NULL);
}

static void
nl_schedule_taskqueue(struct nlpcb *nlp)
{
	if (!nlp->nl_task_pending) {
		nlp->nl_task_pending = true;
		taskqueue_enqueue(nlp->nl_taskqueue, &nlp->nl_task);
		NL_LOG(LOG_DEBUG3, "taskqueue scheduled");
	} else {
		NL_LOG(LOG_DEBUG3, "taskqueue schedule skipped");
	}
}

int
nl_receive_async(struct mbuf *m, struct socket *so)
{
	struct nlpcb *nlp = sotonlpcb(so);
	int error = 0;

	m->m_nextpkt = NULL;

	NLP_LOCK(nlp);

	if ((__predict_true(nlp->nl_active))) {
		sbappend(&so->so_snd, m, 0);
		NL_LOG(LOG_DEBUG3, "enqueue %u bytes", m_length(m, NULL));
		nl_schedule_taskqueue(nlp);
	} else {
		NL_LOG(LOG_DEBUG, "ignoring %u bytes on non-active socket",
		    m_length(m, NULL));
		m_free(m);
		error = EINVAL;
	}

	NLP_UNLOCK(nlp);

	return (error);
}

static bool
tx_check_locked(struct nlpcb *nlp)
{
	if (queue_empty(&nlp->tx_queue))
		return (true);

	/*
	 * Check if something can be moved from the internal TX queue
	 * to the socket queue.
	 */

	bool appended = false;
	struct sockbuf *sb = &nlp->nl_socket->so_rcv;
	SOCKBUF_LOCK(sb);

	while (true) {
		struct mbuf *m = queue_head(&nlp->tx_queue);
		if (m != NULL) {
			struct mbuf *ctl = NULL;
			if (__predict_false(m->m_next != NULL))
				ctl = extract_msg_info(m);
			if (sbappendaddr_locked(sb, nl_empty_src, m, ctl) != 0) {
				/* appended successfully */
				queue_pop(&nlp->tx_queue);
				appended = true;
			} else
				break;
		} else
			break;
	}

	SOCKBUF_UNLOCK(sb);

	if (appended)
		sorwakeup(nlp->nl_socket);

	return (queue_empty(&nlp->tx_queue));
}

static bool
nl_process_received_one(struct nlpcb *nlp)
{
	bool reschedule = false;

	NLP_LOCK(nlp);
	nlp->nl_task_pending = false;

	if (!tx_check_locked(nlp)) {
		/* TX overflow queue still not empty, ignore RX */
		NLP_UNLOCK(nlp);
		return (false);
	}

	if (queue_empty(&nlp->rx_queue)) {
		/*
		 * Grab all data we have from the socket TX queue
		 * and store it the internal queue, so it can be worked on
		 * w/o holding socket lock.
		 */
		struct sockbuf *sb = &nlp->nl_socket->so_snd;

		SOCKBUF_LOCK(sb);
		unsigned int avail = sbavail(sb);
		if (avail > 0) {
			NL_LOG(LOG_DEBUG3, "grabbed %u bytes", avail);
			queue_push(&nlp->rx_queue, sbcut_locked(sb, avail));
		}
		SOCKBUF_UNLOCK(sb);
	} else {
		/* Schedule another pass to read from the socket queue */
		reschedule = true;
	}

	int prev_hiwat = nlp->tx_queue.hiwat;
	NLP_UNLOCK(nlp);

	while (!queue_empty(&nlp->rx_queue)) {
		struct mbuf *m = queue_pop(&nlp->rx_queue);

		m = nl_process_mbuf(m, nlp);
		if (m != NULL) {
			queue_push_head(&nlp->rx_queue, m);
			reschedule = false;
			break;
		}
	}
	if (nlp->tx_queue.hiwat > prev_hiwat) {
		NLP_LOG(LOG_DEBUG, nlp, "TX override peaked to %d", nlp->tx_queue.hiwat);

	}

	return (reschedule);
}

static void
nl_process_received(struct nlpcb *nlp)
{
	NL_LOG(LOG_DEBUG3, "taskqueue called");

	if (__predict_false(nlp->nl_need_thread_setup)) {
		nl_set_thread_nlp(curthread, nlp);
		NLP_LOCK(nlp);
		nlp->nl_need_thread_setup = false;
		NLP_UNLOCK(nlp);
	}

	while (nl_process_received_one(nlp))
		;
}

void
nl_init_io(struct nlpcb *nlp)
{
	STAILQ_INIT(&nlp->rx_queue.head);
	STAILQ_INIT(&nlp->tx_queue.head);
}

void
nl_free_io(struct nlpcb *nlp)
{
	queue_free(&nlp->rx_queue);
	queue_free(&nlp->tx_queue);
}

/*
 * Called after some data have been read from the socket.
 */
void
nl_on_transmit(struct nlpcb *nlp)
{
	NLP_LOCK(nlp);

	struct socket *so = nlp->nl_socket;
	if (__predict_false(nlp->nl_dropped_bytes > 0 && so != NULL)) {
		unsigned long dropped_bytes = nlp->nl_dropped_bytes;
		unsigned long dropped_messages = nlp->nl_dropped_messages;
		nlp->nl_dropped_bytes = 0;
		nlp->nl_dropped_messages = 0;

		struct sockbuf *sb = &so->so_rcv;
		NLP_LOG(LOG_DEBUG, nlp,
		    "socket RX overflowed, %lu messages (%lu bytes) dropped. "
		    "bytes: [%u/%u] mbufs: [%u/%u]", dropped_messages, dropped_bytes,
		    sb->sb_ccc, sb->sb_hiwat, sb->sb_mbcnt, sb->sb_mbmax);
		/* TODO: send netlink message */
	}

	nl_schedule_taskqueue(nlp);
	NLP_UNLOCK(nlp);
}

void
nl_taskqueue_handler(void *_arg, int pending)
{
	struct nlpcb *nlp = (struct nlpcb *)_arg;

	CURVNET_SET(nlp->nl_socket->so_vnet);
	nl_process_received(nlp);
	CURVNET_RESTORE();
}

static __noinline void
queue_push_tx(struct nlpcb *nlp, struct mbuf *m)
{
	queue_push(&nlp->tx_queue, m);
	nlp->nl_tx_blocked = true;

	if (nlp->tx_queue.length > nlp->tx_queue.hiwat)
		nlp->tx_queue.hiwat = nlp->tx_queue.length;
}

/*
 * Tries to send @m to the socket @nlp.
 *
 * @m: mbuf(s) to send to. Consumed in any case.
 * @nlp: socket to send to
 * @cnt: number of messages in @m
 * @io_flags: combination of NL_IOF_* flags
 *
 * Returns true on success.
 * If no queue overrunes happened, wakes up socket owner.
 */
bool
nl_send_one(struct mbuf *m, struct nlpcb *nlp, int num_messages, int io_flags)
{
	bool untranslated = io_flags & NL_IOF_UNTRANSLATED;
	bool ignore_limits = io_flags & NL_IOF_IGNORE_LIMIT;
	bool result = true;

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		struct nlmsghdr *hdr = mtod(m, struct nlmsghdr *);
		NLP_LOG(LOG_DEBUG2, nlp,
		    "TX mbuf len %u msgs %u msg type %d first hdrlen %u io_flags %X",
		    m_length(m, NULL), num_messages, hdr->nlmsg_type, hdr->nlmsg_len,
		    io_flags);
	}

	if (__predict_false(nlp->nl_linux && linux_netlink_p != NULL && untranslated)) {
		m = linux_netlink_p->mbufs_to_linux(nlp->nl_proto, m, nlp);
		if (m == NULL)
			return (false);
	}

	NLP_LOCK(nlp);

	if (__predict_false(nlp->nl_socket == NULL)) {
		NLP_UNLOCK(nlp);
		m_freem(m);
		return (false);
	}

	if (!queue_empty(&nlp->tx_queue)) {
		if (ignore_limits) {
			queue_push_tx(nlp, m);
		} else {
			m_free(m);
			result = false;
		}
		NLP_UNLOCK(nlp);
		return (result);
	}

	struct socket *so = nlp->nl_socket;
	struct mbuf *ctl = NULL;
	if (__predict_false(m->m_next != NULL))
		ctl = extract_msg_info(m);
	if (sbappendaddr(&so->so_rcv, nl_empty_src, m, ctl) != 0) {
		sorwakeup(so);
		NLP_LOG(LOG_DEBUG3, nlp, "appended data & woken up");
	} else {
		if (ignore_limits) {
			queue_push_tx(nlp, m);
		} else {
			/*
			 * Store dropped data so it can be reported
			 * on the next read
			 */
			nlp->nl_dropped_bytes += m_length(m, NULL);
			nlp->nl_dropped_messages += num_messages;
			NLP_LOG(LOG_DEBUG2, nlp, "RX oveflow: %lu m (+%d), %lu b (+%d)",
			    (unsigned long)nlp->nl_dropped_messages, num_messages,
			    (unsigned long)nlp->nl_dropped_bytes, m_length(m, NULL));
			soroverflow(so);
			m_freem(m);
			result = false;
		}
	}
	NLP_UNLOCK(nlp);

	return (result);
}

static int
nl_receive_message(struct nlmsghdr *hdr, int remaining_length,
    struct nlpcb *nlp, struct nl_pstate *npt)
{
	nl_handler_f handler = nl_handlers[nlp->nl_proto].cb;
	int error = 0;

	NLP_LOG(LOG_DEBUG2, nlp, "msg len: %u type: %d: flags: 0x%X seq: %u pid: %u",
	    hdr->nlmsg_len, hdr->nlmsg_type, hdr->nlmsg_flags, hdr->nlmsg_seq,
	    hdr->nlmsg_pid);

	if (__predict_false(hdr->nlmsg_len > remaining_length)) {
		NLP_LOG(LOG_DEBUG, nlp, "message is not entirely present: want %d got %d",
		    hdr->nlmsg_len, remaining_length);
		return (EINVAL);
	} else if (__predict_false(hdr->nlmsg_len < sizeof(*hdr))) {
		NL_LOG(LOG_DEBUG, "message too short: %d", hdr->nlmsg_len);
		return (EINVAL);
	}
	/* Stamp each message with sender pid */
	hdr->nlmsg_pid = nlp->nl_port;

	npt->hdr = hdr;

	if (hdr->nlmsg_flags & NLM_F_REQUEST && hdr->nlmsg_type >= NLMSG_MIN_TYPE) {
		NL_LOG(LOG_DEBUG2, "handling message with msg type: %d",
		   hdr->nlmsg_type);

		if (nlp->nl_linux && linux_netlink_p != NULL) {
			struct nlmsghdr *hdr_orig = hdr;
			hdr = linux_netlink_p->msg_from_linux(nlp->nl_proto, hdr, npt);
			if (hdr == NULL) {
				 /* Failed to translate to kernel format. Report an error back */
				hdr = hdr_orig;
				npt->hdr = hdr;
				if (hdr->nlmsg_flags & NLM_F_ACK)
					nlmsg_ack(nlp, EOPNOTSUPP, hdr, npt);
				return (0);
			}
		}
		error = handler(hdr, npt);
		NL_LOG(LOG_DEBUG2, "retcode: %d", error);
	}
	if ((hdr->nlmsg_flags & NLM_F_ACK) || (error != 0 && error != EINTR)) {
		if (!npt->nw->suppress_ack) {
			NL_LOG(LOG_DEBUG3, "ack");
			nlmsg_ack(nlp, error, hdr, npt);
		}
	}

	return (0);
}

static void
npt_clear(struct nl_pstate *npt)
{
	lb_clear(&npt->lb);
	npt->error = 0;
	npt->err_msg = NULL;
	npt->err_off = 0;
	npt->hdr = NULL;
	npt->nw->suppress_ack = false;
}

/*
 * Processes an incoming packet, which can contain multiple netlink messages
 */
static struct mbuf *
nl_process_mbuf(struct mbuf *m, struct nlpcb *nlp)
{
	int offset, buffer_length;
	struct nlmsghdr *hdr;
	char *buffer;
	int error;

	NL_LOG(LOG_DEBUG3, "RX netlink mbuf %p on %p", m, nlp->nl_socket);

	struct nl_writer nw = {};
	if (!nlmsg_get_unicast_writer(&nw, NLMSG_SMALL, nlp)) {
		m_freem(m);
		NL_LOG(LOG_DEBUG, "error allocating socket writer");
		return (NULL);
	}

	nlmsg_ignore_limit(&nw);
	/* TODO: alloc this buf once for nlp */
	int data_length = m_length(m, NULL);
	buffer_length = roundup2(data_length, 8) + SCRATCH_BUFFER_SIZE;
	if (nlp->nl_linux)
		buffer_length += roundup2(data_length, 8);
	buffer = malloc(buffer_length, M_NETLINK, M_NOWAIT | M_ZERO);
	if (buffer == NULL) {
		m_freem(m);
		nlmsg_flush(&nw);
		NL_LOG(LOG_DEBUG, "Unable to allocate %d bytes of memory",
		    buffer_length);
		return (NULL);
	}
	m_copydata(m, 0, data_length, buffer);

	struct nl_pstate npt = {
		.nlp = nlp,
		.lb.base = &buffer[roundup2(data_length, 8)],
		.lb.size = buffer_length - roundup2(data_length, 8),
		.nw = &nw,
		.strict = nlp->nl_flags & NLF_STRICT,
	};

	for (offset = 0; offset + sizeof(struct nlmsghdr) <= data_length;) {
		hdr = (struct nlmsghdr *)&buffer[offset];
		/* Save length prior to calling handler */
		int msglen = NLMSG_ALIGN(hdr->nlmsg_len);
		NL_LOG(LOG_DEBUG3, "parsing offset %d/%d", offset, data_length);
		npt_clear(&npt);
		error = nl_receive_message(hdr, data_length - offset, nlp, &npt);
		offset += msglen;
		if (__predict_false(error != 0 || nlp->nl_tx_blocked))
			break;
	}
	NL_LOG(LOG_DEBUG3, "packet parsing done");
	free(buffer, M_NETLINK);
	nlmsg_flush(&nw);

	if (nlp->nl_tx_blocked) {
		NLP_LOCK(nlp);
		nlp->nl_tx_blocked = false;
		NLP_UNLOCK(nlp);
		m_adj(m, offset);
		return (m);
	} else {
		m_freem(m);
		return (NULL);
	}
}
