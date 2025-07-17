/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_linux.h>
#include <netlink/netlink_var.h>

#define	DEBUG_MOD_NAME	nl_writer
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

static bool
nlmsg_get_buf(struct nl_writer *nw, size_t len, bool waitok)
{
	const int mflag = waitok ? M_WAITOK : M_NOWAIT;

	MPASS(nw->buf == NULL);

	NL_LOG(LOG_DEBUG3, "Setting up nw %p len %zu %s", nw, len,
	    waitok ? "wait" : "nowait");

	nw->buf = nl_buf_alloc(len, mflag);
	if (__predict_false(nw->buf == NULL))
		return (false);
	nw->hdr = NULL;
	nw->malloc_flag = mflag;
	nw->num_messages = 0;
	nw->enomem = false;

	return (true);
}

static bool
nl_send_one(struct nl_writer *nw)
{

	return (nl_send(nw, nw->nlp));
}

bool
_nl_writer_unicast(struct nl_writer *nw, size_t size, struct nlpcb *nlp,
    bool waitok)
{
	*nw = (struct nl_writer){
		.nlp = nlp,
		.cb = nl_send_one,
	};

	return (nlmsg_get_buf(nw, size, waitok));
}

bool
_nl_writer_group(struct nl_writer *nw, size_t size, uint16_t protocol,
    uint16_t group_id, int priv, bool waitok)
{
	*nw = (struct nl_writer){
		.group.proto = protocol,
		.group.id = group_id,
		.group.priv = priv,
		.cb = nl_send_group,
	};

	return (nlmsg_get_buf(nw, size, waitok));
}

void
_nlmsg_ignore_limit(struct nl_writer *nw)
{
	nw->ignore_limit = true;
}

bool
_nlmsg_flush(struct nl_writer *nw)
{
	bool result;

	if (__predict_false(nw->hdr != NULL)) {
		/* Last message has not been completed, skip it. */
		int completed_len = (char *)nw->hdr - nw->buf->data;
		/* Send completed messages */
		nw->buf->datalen -= nw->buf->datalen - completed_len;
		nw->hdr = NULL;
        }

	if (nw->buf->datalen == 0) {
		MPASS(nw->num_messages == 0);
		nl_buf_free(nw->buf);
		nw->buf = NULL;
		return (true);
	}

	result = nw->cb(nw);
	nw->num_messages = 0;

	if (!result) {
		NL_LOG(LOG_DEBUG, "nw %p flush with %p() failed", nw, nw->cb);
	}

	return (result);
}

/*
 * Flushes previous data and allocates new underlying storage
 *  sufficient for holding at least @required_len bytes.
 * Return true on success.
 */
bool
_nlmsg_refill_buffer(struct nl_writer *nw, size_t required_len)
{
	struct nl_buf *new;
	size_t completed_len, new_len, last_len;

	MPASS(nw->buf != NULL);

	if (nw->enomem)
		return (false);

	NL_LOG(LOG_DEBUG3, "no space at offset %u/%u (want %zu), trying to "
	    "reclaim", nw->buf->datalen, nw->buf->buflen, required_len);

	/* Calculate new buffer size and allocate it. */
	completed_len = (nw->hdr != NULL) ?
	    (char *)nw->hdr - nw->buf->data : nw->buf->datalen;
	if (completed_len > 0 && required_len < NLMBUFSIZE) {
		/* We already ran out of space, use largest effective size. */
		new_len = max(nw->buf->buflen, NLMBUFSIZE);
	} else {
		if (nw->buf->buflen < NLMBUFSIZE)
			/* XXXGL: does this happen? */
			new_len = NLMBUFSIZE;
		else
			new_len = nw->buf->buflen * 2;
		while (new_len < required_len)
			new_len *= 2;
	}

	new = nl_buf_alloc(new_len, nw->malloc_flag | M_ZERO);
	if (__predict_false(new == NULL)) {
		nw->enomem = true;
		NL_LOG(LOG_DEBUG, "getting new buf failed, setting ENOMEM");
		return (false);
	}

	/* Copy last (unfinished) header to the new storage. */
	last_len = nw->buf->datalen - completed_len;
	if (last_len > 0) {
		memcpy(new->data, nw->hdr, last_len);
		new->datalen = last_len;
	}

	NL_LOG(LOG_DEBUG2, "completed: %zu bytes, copied: %zu bytes",
	    completed_len, last_len);

	if (completed_len > 0) {
		nlmsg_flush(nw);
		MPASS(nw->buf == NULL);
	} else
		nl_buf_free(nw->buf);
	nw->buf = new;
	nw->hdr = (last_len > 0) ? (struct nlmsghdr *)new->data : NULL;
	NL_LOG(LOG_DEBUG2, "switched buffer: used %u/%u bytes",
	    new->datalen, new->buflen);

	return (true);
}

bool
_nlmsg_add(struct nl_writer *nw, uint32_t portid, uint32_t seq, uint16_t type,
    uint16_t flags, uint32_t len)
{
	struct nl_buf *nb = nw->buf;
	struct nlmsghdr *hdr;
	size_t required_len;

	MPASS(nw->hdr == NULL);

	required_len = NETLINK_ALIGN(len + sizeof(struct nlmsghdr));
	if (__predict_false(nb->datalen + required_len > nb->buflen)) {
		if (!nlmsg_refill_buffer(nw, required_len))
			return (false);
		nb = nw->buf;
	}

	hdr = (struct nlmsghdr *)(&nb->data[nb->datalen]);

	hdr->nlmsg_len = len;
	hdr->nlmsg_type = type;
	hdr->nlmsg_flags = flags;
	hdr->nlmsg_seq = seq;
	hdr->nlmsg_pid = portid;

	nw->hdr = hdr;
	nb->datalen += sizeof(struct nlmsghdr);

	return (true);
}

bool
_nlmsg_end(struct nl_writer *nw)
{
	struct nl_buf *nb = nw->buf;

	MPASS(nw->hdr != NULL);

	if (nw->enomem) {
		NL_LOG(LOG_DEBUG, "ENOMEM when dumping message");
		nlmsg_abort(nw);
		return (false);
	}

	nw->hdr->nlmsg_len = nb->data + nb->datalen - (char *)nw->hdr;
	NL_LOG(LOG_DEBUG2, "wrote msg len: %u type: %d: flags: 0x%X seq: %u pid: %u",
	    nw->hdr->nlmsg_len, nw->hdr->nlmsg_type, nw->hdr->nlmsg_flags,
	    nw->hdr->nlmsg_seq, nw->hdr->nlmsg_pid);
	nw->hdr = NULL;
	nw->num_messages++;
	return (true);
}

void
_nlmsg_abort(struct nl_writer *nw)
{
	struct nl_buf *nb = nw->buf;

	if (nw->hdr != NULL) {
		nb->datalen = (char *)nw->hdr - nb->data;
		nw->hdr = NULL;
	}
}

void
nlmsg_ack(struct nlpcb *nlp, int error, struct nlmsghdr *hdr,
    struct nl_pstate *npt)
{
	struct nlmsgerr *errmsg;
	int payload_len;
	uint32_t flags = nlp->nl_flags;
	struct nl_writer *nw = npt->nw;
	bool cap_ack;

	payload_len = sizeof(struct nlmsgerr);

	/*
	 * The only case when we send the full message in the
	 * reply is when there is an error and NETLINK_CAP_ACK
	 * is not set.
	 */
	cap_ack = (error == 0) || (flags & NLF_CAP_ACK);
	if (!cap_ack)
		payload_len += hdr->nlmsg_len - sizeof(struct nlmsghdr);
	payload_len = NETLINK_ALIGN(payload_len);

	uint16_t nl_flags = cap_ack ? NLM_F_CAPPED : 0;
	if ((npt->err_msg || npt->err_off) && nlp->nl_flags & NLF_EXT_ACK)
		nl_flags |= NLM_F_ACK_TLVS;

	NL_LOG(LOG_DEBUG3, "acknowledging message type %d seq %d",
	    hdr->nlmsg_type, hdr->nlmsg_seq);

	if (!nlmsg_add(nw, nlp->nl_port, hdr->nlmsg_seq, NLMSG_ERROR, nl_flags, payload_len))
		goto enomem;

	errmsg = nlmsg_reserve_data(nw, payload_len, struct nlmsgerr);
	errmsg->error = error;
	/* In case of error copy the whole message, else just the header */
	memcpy(&errmsg->msg, hdr, cap_ack ? sizeof(*hdr) : hdr->nlmsg_len);

	if (npt->err_msg != NULL && nlp->nl_flags & NLF_EXT_ACK)
		nlattr_add_string(nw, NLMSGERR_ATTR_MSG, npt->err_msg);
	if (npt->err_off != 0 && nlp->nl_flags & NLF_EXT_ACK)
		nlattr_add_u32(nw, NLMSGERR_ATTR_OFFS, npt->err_off);
	if (npt->cookie != NULL)
		nlattr_add_raw(nw, npt->cookie);

	if (nlmsg_end(nw))
		return;
enomem:
	NLP_LOG(LOG_DEBUG, nlp, "error allocating ack data for message %d seq %u",
	    hdr->nlmsg_type, hdr->nlmsg_seq);
	nlmsg_abort(nw);
}

bool
_nlmsg_end_dump(struct nl_writer *nw, int error, struct nlmsghdr *hdr)
{
	if (!nlmsg_add(nw, hdr->nlmsg_pid, hdr->nlmsg_seq, NLMSG_DONE, 0, sizeof(int))) {
		NL_LOG(LOG_DEBUG, "Error finalizing table dump");
		return (false);
	}
	/* Save operation result */
	int *perror = nlmsg_reserve_object(nw, int);
	NL_LOG(LOG_DEBUG2, "record error=%d at off %d (%p)", error,
	    nw->buf->datalen, perror);
	*perror = error;
	nlmsg_end(nw);
	nw->suppress_ack = true;

	return (true);
}

/*
 * KPI functions.
 */

u_int
nlattr_save_offset(const struct nl_writer *nw)
{
	return (nw->buf->datalen - ((char *)nw->hdr - nw->buf->data));
}

void *
nlmsg_reserve_data_raw(struct nl_writer *nw, size_t sz)
{
	struct nl_buf *nb = nw->buf;
	void *data;

	sz = NETLINK_ALIGN(sz);
	if (__predict_false(nb->datalen + sz > nb->buflen)) {
		if (!nlmsg_refill_buffer(nw, sz))
			return (NULL);
		nb = nw->buf;
	}

	data = &nb->data[nb->datalen];
	bzero(data, sz);
	nb->datalen += sz;

	return (data);
}

bool
nlattr_add(struct nl_writer *nw, uint16_t attr_type, uint16_t attr_len,
    const void *data)
{
	struct nl_buf *nb = nw->buf;
	struct nlattr *nla;
	size_t required_len;

	KASSERT(attr_len <= UINT16_MAX - sizeof(struct nlattr),
	   ("%s: invalid attribute length %u", __func__, attr_len));

	required_len = NLA_ALIGN(attr_len + sizeof(struct nlattr));
	if (__predict_false(nb->datalen + required_len > nb->buflen)) {
		if (!nlmsg_refill_buffer(nw, required_len))
			return (false);
		nb = nw->buf;
	}

	nla = (struct nlattr *)(&nb->data[nb->datalen]);

	nla->nla_len = attr_len + sizeof(struct nlattr);
	nla->nla_type = attr_type;
	if (attr_len > 0) {
		if ((attr_len % 4) != 0) {
			/* clear padding bytes */
			bzero((char *)nla + required_len - 4, 4);
		}
		memcpy((nla + 1), data, attr_len);
	}
	nb->datalen += required_len;
	return (true);
}

#include <netlink/ktest_netlink_message_writer.h>
