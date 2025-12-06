/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
_DECLARE_DEBUG(LOG_INFO);

/*
 * The logic below provide a p2p interface for receiving and
 * sending netlink data between the kernel and userland.
 */

static bool nl_process_nbuf(struct nl_buf *nb, struct nlpcb *nlp);

struct nl_buf *
nl_buf_alloc(size_t len, int mflag)
{
	struct nl_buf *nb;

	KASSERT(len > 0 && len <= UINT_MAX, ("%s: invalid length %zu",
	    __func__, len));

	nb = malloc(sizeof(struct nl_buf) + len, M_NETLINK, mflag);
	if (__predict_true(nb != NULL)) {
		nb->buflen = len;
		nb->datalen = nb->offset = 0;
	}

	return (nb);
}

void
nl_buf_free(struct nl_buf *nb)
{

	free(nb, M_NETLINK);
}

void
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

static bool
nl_process_received_one(struct nlpcb *nlp)
{
	struct socket *so = nlp->nl_socket;
	struct sockbuf *sb;
	struct nl_buf *nb;
	bool reschedule = false;

	NLP_LOCK(nlp);
	nlp->nl_task_pending = false;
	NLP_UNLOCK(nlp);

	/*
	 * Do not process queued up requests if there is no space to queue
	 * replies.
	 */
	sb = &so->so_rcv;
	SOCK_RECVBUF_LOCK(so);
	if (sb->sb_hiwat <= sb->sb_ccc) {
		SOCK_RECVBUF_UNLOCK(so);
		NL_LOG(LOG_DEBUG3, "socket %p stuck", so);
		return (false);
	}
	SOCK_RECVBUF_UNLOCK(so);

	sb = &so->so_snd;
	SOCK_SENDBUF_LOCK(so);
	while ((nb = TAILQ_FIRST(&sb->nl_queue)) != NULL) {
		TAILQ_REMOVE(&sb->nl_queue, nb, tailq);
		SOCK_SENDBUF_UNLOCK(so);
		reschedule = nl_process_nbuf(nb, nlp);
		SOCK_SENDBUF_LOCK(so);
		if (reschedule) {
			sb->sb_acc -= nb->datalen;
			sb->sb_ccc -= nb->datalen;
			/* XXXGL: potentially can reduce lock&unlock count. */
			sowwakeup_locked(so);
			nl_buf_free(nb);
			SOCK_SENDBUF_LOCK(so);
		} else {
			TAILQ_INSERT_HEAD(&sb->nl_queue, nb, tailq);
			break;
		}
	}
	SOCK_SENDBUF_UNLOCK(so);

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
		    "bytes: [%u/%u]", dropped_messages, dropped_bytes,
		    sb->sb_ccc, sb->sb_hiwat);
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

/*
 * Tries to send current data buffer from writer.
 *
 * Returns true on success.
 * If no queue overrunes happened, wakes up socket owner.
 */
bool
nl_send(struct nl_writer *nw, struct nlpcb *nlp)
{
	struct socket *so = nlp->nl_socket;
	struct sockbuf *sb = &so->so_rcv;
	struct nl_buf *nb;

	MPASS(nw->hdr == NULL);
	MPASS(nw->buf != NULL);
	MPASS(nw->buf->datalen > 0);

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		struct nlmsghdr *hdr = (struct nlmsghdr *)nw->buf->data;
		NLP_LOG(LOG_DEBUG2, nlp,
		    "TX len %u msgs %u msg type %d first hdrlen %u",
		    nw->buf->datalen, nw->num_messages, hdr->nlmsg_type,
		    hdr->nlmsg_len);
	}

	if (nlp->nl_linux && linux_netlink_p != NULL) {
		nb = linux_netlink_p->msgs_to_linux(nw->buf, nlp);
		nl_buf_free(nw->buf);
		nw->buf = NULL;
		if (nb == NULL)
			return (false);
	} else {
		nb = nw->buf;
		nw->buf = NULL;
	}

	SOCK_RECVBUF_LOCK(so);
	if (!nw->ignore_limit && __predict_false(sb->sb_hiwat <= sb->sb_ccc)) {
		SOCK_RECVBUF_UNLOCK(so);
		NLP_LOCK(nlp);
		nlp->nl_dropped_bytes += nb->datalen;
		nlp->nl_dropped_messages += nw->num_messages;
		NLP_LOG(LOG_DEBUG2, nlp, "RX oveflow: %lu m (+%d), %lu b (+%d)",
		    (unsigned long)nlp->nl_dropped_messages, nw->num_messages,
		    (unsigned long)nlp->nl_dropped_bytes, nb->datalen);
		NLP_UNLOCK(nlp);
		nl_buf_free(nb);
		return (false);
	} else {
		bool full;

		TAILQ_INSERT_TAIL(&sb->nl_queue, nb, tailq);
		sb->sb_acc += nb->datalen;
		sb->sb_ccc += nb->datalen;
		full = sb->sb_hiwat <= sb->sb_ccc;
		sorwakeup_locked(so);
		if (full) {
			NLP_LOCK(nlp);
			nlp->nl_tx_blocked = true;
			NLP_UNLOCK(nlp);
		}
		return (true);
	}
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

	if (hdr->nlmsg_flags & NLM_F_REQUEST &&
	    hdr->nlmsg_type >= NLMSG_MIN_TYPE) {
		NL_LOG(LOG_DEBUG2, "handling message with msg type: %d",
		   hdr->nlmsg_type);
		if (nlp->nl_linux) {
			MPASS(linux_netlink_p != NULL);
			error = linux_netlink_p->msg_from_linux(nlp->nl_proto,
			    &hdr, npt);
			if (error)
				goto ack;
		}
		error = handler(hdr, npt);
		NL_LOG(LOG_DEBUG2, "retcode: %d", error);
	}
ack:
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
	npt->cookie = NULL;
	npt->error = 0;
	npt->err_msg = NULL;
	npt->err_off = 0;
	npt->hdr = NULL;
	npt->nw->suppress_ack = false;
}

/*
 * Processes an incoming packet, which can contain multiple netlink messages
 */
static bool
nl_process_nbuf(struct nl_buf *nb, struct nlpcb *nlp)
{
	struct nl_writer nw;
	struct nlmsghdr *hdr;
	int error;

	NL_LOG(LOG_DEBUG3, "RX netlink buf %p on %p", nb, nlp->nl_socket);

	if (!nl_writer_unicast(&nw, NLMSG_SMALL, nlp, false)) {
		NL_LOG(LOG_DEBUG, "error allocating socket writer");
		return (true);
	}

	nlmsg_ignore_limit(&nw);

	struct nl_pstate npt = {
		.nlp = nlp,
		.lb.base = &nb->data[roundup2(nb->datalen, 8)],
		.lb.size = nb->buflen - roundup2(nb->datalen, 8),
		.nw = &nw,
		.strict = nlp->nl_flags & NLF_STRICT,
	};

	for (; nb->offset + sizeof(struct nlmsghdr) <= nb->datalen;) {
		hdr = (struct nlmsghdr *)&nb->data[nb->offset];
		/* Save length prior to calling handler */
		int msglen = NLMSG_ALIGN(hdr->nlmsg_len);
		NL_LOG(LOG_DEBUG3, "parsing offset %d/%d",
		    nb->offset, nb->datalen);
		npt_clear(&npt);
		error = nl_receive_message(hdr, nb->datalen - nb->offset, nlp,
		    &npt);
		nb->offset += msglen;
		if (__predict_false(error != 0 || nlp->nl_tx_blocked))
			break;
	}
	NL_LOG(LOG_DEBUG3, "packet parsing done");
	nlmsg_flush(&nw);

	if (nlp->nl_tx_blocked) {
		NLP_LOCK(nlp);
		nlp->nl_tx_blocked = false;
		NLP_UNLOCK(nlp);
		return (false);
	} else
		return (true);
}
