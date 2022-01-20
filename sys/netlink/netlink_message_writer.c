/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/mbuf.h>
#include <sys/ck.h>
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
_DECLARE_DEBUG(LOG_DEBUG);

/*
 * The goal of this file is to provide convenient message writing KPI on top of
 * different storage methods (mbufs, uio, temporary memory chunks).
 *
 * The main KPI guarantee is the the (last) message always resides in the contiguous
 *  memory buffer, so one is able to update the header after writing the entire message.
 *
 * This guarantee comes with a side effect of potentially reallocating underlying
 *  buffer, so one needs to update the desired pointers after something is added
 *  to the header.
 *
 * Messaging layer contains hooks performing transparent Linux translation for the messages.
 *
 * There are 3 types of supported targets:
 *  * socket (adds mbufs to the socket buffer, used for message replies)
 *  * group (sends mbuf/chain to the specified groups, used for the notifications)
 *  * chain (returns mbuf chain, used in Linux message translation code)
 *
 * There are 3 types of storage:
 * * NS_WRITER_TYPE_MBUF (mbuf-based, most efficient, used when a single message
 *    fits in MCLBYTES)
 * * NS_WRITER_TYPE_BUF (fallback, malloc-based, used when a single message needs
 *    to be larger than one supported by NS_WRITER_TYPE_MBUF)
 * * NS_WRITER_TYPE_LBUF (malloc-based, similar to NS_WRITER_TYPE_BUF, used for
 *    Linux sockets, calls translation hook prior to sending messages to the socket).
 *
 * Internally, KPI switches between different types of storage when memory requirements
 *  change. It happens transparently to the caller.
 */


typedef bool nlwriter_op_init(struct nl_writer *nw, int size, bool waitok);
typedef bool nlwriter_op_write(struct nl_writer *nw, void *buf, int buflen, int cnt);

struct nlwriter_ops {
	nlwriter_op_init	*init;
	nlwriter_op_write	*write_socket;
	nlwriter_op_write	*write_group;
	nlwriter_op_write	*write_chain;
};

/*
 * NS_WRITER_TYPE_BUF
 * Writes message to a temporary memory buffer,
 * flushing to the socket/group when buffer size limit is reached
 */
static bool
nlmsg_get_ns_buf(struct nl_writer *nw, int size, bool waitok)
{
	int mflag = waitok ? M_WAITOK : M_NOWAIT;
	nw->_storage = malloc(size, M_NETLINK, mflag | M_ZERO);
	if (__predict_false(nw->_storage == NULL))
		return (false);
	nw->alloc_len = size;
	nw->offset = 0;
	nw->hdr = NULL;
	nw->data = nw->_storage;
	nw->writer_type = NS_WRITER_TYPE_BUF;
	nw->malloc_flag = mflag;
	nw->num_messages = 0;
	nw->enomem = false;
	return (true);
}

static bool
nlmsg_write_socket_buf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	NL_LOG(LOG_DEBUG2, "IN: ptr: %p len: %d arg: %p", buf, datalen, nw);
	if (__predict_false(datalen == 0)) {
		free(buf, M_NETLINK);
		return (true);
	}

	struct mbuf *m = m_getm2(NULL, datalen, nw->malloc_flag, MT_DATA, M_PKTHDR);
	if (__predict_false(m == NULL)) {
		/* XXX: should we set sorcverr? */
		free(buf, M_NETLINK);
		return (false);
	}
	m_append(m, datalen, buf);
	free(buf, M_NETLINK);

	int io_flags = (nw->ignore_limit) ? NL_IOF_IGNORE_LIMIT : 0;
        return (nl_send_one(m, (struct nlpcb *)(nw->arg_ptr), cnt, io_flags));
}

static bool
nlmsg_write_group_buf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	NL_LOG(LOG_DEBUG2, "IN: ptr: %p len: %d arg: %p", buf, datalen, nw->arg_ptr);
	if (__predict_false(datalen == 0)) {
		free(buf, M_NETLINK);
		return (true);
	}

	struct mbuf *m = m_getm2(NULL, datalen, nw->malloc_flag, MT_DATA, M_PKTHDR);
	if (__predict_false(m == NULL)) {
		free(buf, M_NETLINK);
		return (false);
	}
	bool success = m_append(m, datalen, buf) != 0;
	free(buf, M_NETLINK);

	if (!success)
		return (false);

        nl_send_group(m, cnt, nw->arg_uint >> 16, nw->arg_uint & 0xFFFF);
	return (true);
}

static bool
nlmsg_write_chain_buf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	struct mbuf **m0 = (struct mbuf **)(nw->arg_ptr);
	NL_LOG(LOG_DEBUG2, "IN: ptr: %p len: %d arg: %p", buf, datalen, nw->arg_ptr);

	if (__predict_false(datalen == 0)) {
		free(buf, M_NETLINK);
		return (true);
	}

	if (*m0 == NULL) {
		struct mbuf *m;

		m = m_getm2(NULL, datalen, nw->malloc_flag, MT_DATA, M_PKTHDR);
		if (__predict_false(m == NULL)) {
			free(buf, M_NETLINK);
			return (false);
		}
		*m0 = m;
	}
	if (__predict_false(m_append(*m0, datalen, buf) == 0)) {
		free(buf, M_NETLINK);
		return (false);
	}
        return (true);
}


/*
 * NS_WRITER_TYPE_MBUF
 * Writes message to the allocated mbuf,
 * flushing to socket/group when mbuf size limit is reached.
 * This is the most efficient mechanism as it avoids double-copying.
 *
 * Allocates a single mbuf suitable to store up to @size bytes of data.
 * If size < MHLEN (around 160 bytes), allocates mbuf with pkghdr
 * If size <= MCLBYTES (2k), allocate a single mbuf cluster
 * Otherwise, return NULL.
 */
static bool
nlmsg_get_ns_mbuf(struct nl_writer *nw, int size, bool waitok)
{
	struct mbuf *m;

	int mflag = waitok ? M_WAITOK : M_NOWAIT;
        m = m_get2(size, mflag, MT_DATA, M_PKTHDR);
        if (__predict_false(m == NULL))
                return (false);
        nw->alloc_len = M_TRAILINGSPACE(m);
        nw->offset = 0;
        nw->hdr = NULL;
	nw->_storage = (void *)m;
	nw->data = mtod(m, void *);
	nw->writer_type = NS_WRITER_TYPE_MBUF;
	nw->malloc_flag = mflag;
	nw->num_messages = 0;
	nw->enomem = false;
        NL_LOG(LOG_DEBUG2, "alloc mbuf %p req_len %d alloc_len %d data_ptr %p",
            m, size, nw->alloc_len, nw->data);
        return (true);
}

static bool
nlmsg_write_socket_mbuf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	struct mbuf *m = (struct mbuf *)buf;
	NL_LOG(LOG_DEBUG2, "IN: ptr: %p len: %d arg: %p", buf, datalen, nw->arg_ptr);

	if (__predict_false(datalen == 0)) {
		m_freem(m);
		return (true);
	}

	m->m_pkthdr.len = datalen;
	m->m_len = datalen;
	int io_flags = (nw->ignore_limit) ? NL_IOF_IGNORE_LIMIT : 0;
        return (nl_send_one(m, (struct nlpcb *)(nw->arg_ptr), cnt, io_flags));
}

static bool
nlmsg_write_group_mbuf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	struct mbuf *m = (struct mbuf *)buf;
	NL_LOG(LOG_DEBUG2, "IN: ptr: %p len: %d arg: %p", buf, datalen, nw->arg_ptr);

	if (__predict_false(datalen == 0)) {
		m_freem(m);
		return (true);
	}

	m->m_pkthdr.len = datalen;
	m->m_len = datalen;
        nl_send_group(m, cnt, nw->arg_uint >> 16, nw->arg_uint & 0xFFFF);
	return (true);
}

static bool
nlmsg_write_chain_mbuf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	struct mbuf *m_new = (struct mbuf *)buf;
	struct mbuf **m0 = (struct mbuf **)(nw->arg_ptr);

	NL_LOG(LOG_DEBUG2, "IN: ptr: %p len: %d arg: %p", buf, datalen, nw->arg_ptr);

	if (__predict_false(datalen == 0)) {
		m_freem(m_new);
		return (true);
	}

	m_new->m_pkthdr.len = datalen;
	m_new->m_len = datalen;

	if (*m0 == NULL) {
		*m0 = m_new;
	} else {
		struct mbuf *m_last;
		for (m_last = *m0; m_last->m_next != NULL; m_last = m_last->m_next)
			;
		m_last->m_next = m_new;
		(*m0)->m_pkthdr.len += datalen;
	}

        return (true);
}

/*
 * NS_WRITER_TYPE_LBUF
 * Writes message to the allocated memory buffer,
 * flushing to socket/group when mbuf size limit is reached.
 * Calls linux handler to rewrite messages before sending to the socket.
 */
static bool
nlmsg_get_ns_lbuf(struct nl_writer *nw, int size, bool waitok)
{
	int mflag = waitok ? M_WAITOK : M_NOWAIT;
	size = roundup2(size, sizeof(void *));
	int add_size = sizeof(struct linear_buffer) + SCRATCH_BUFFER_SIZE;
	char *buf = malloc(add_size + size * 2, M_NETLINK, mflag | M_ZERO);
	if (__predict_false(buf == NULL))
		return (false);

	/* Fill buffer header first */
	struct linear_buffer *lb = (struct linear_buffer *)buf;
	lb->base = &buf[sizeof(struct linear_buffer) + size];
	lb->size = size + SCRATCH_BUFFER_SIZE;

	nw->alloc_len = size;
	nw->offset = 0;
	nw->hdr = NULL;
	nw->_storage = buf;
	nw->data = (char *)(lb + 1);
	nw->malloc_flag = mflag;
	nw->writer_type = NS_WRITER_TYPE_LBUF;
	nw->num_messages = 0;
	nw->enomem = false;
	return (true);
}


static bool
nlmsg_write_socket_lbuf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	struct linear_buffer *lb = (struct linear_buffer *)buf;
	char *data = (char *)(lb + 1);
	struct nlpcb *nlp = (struct nlpcb *)(nw->arg_ptr);

	if (__predict_false(datalen == 0)) {
		free(buf, M_NETLINK);
		return (true);
	}

	struct mbuf *m = NULL;
	if (linux_netlink_p != NULL)
		m = linux_netlink_p->msgs_to_linux(nlp->nl_proto, data, datalen, nlp);
	free(buf, M_NETLINK);

	if (__predict_false(m == NULL)) {
		/* XXX: should we set sorcverr? */
		return (false);
	}

	int io_flags = (nw->ignore_limit) ? NL_IOF_IGNORE_LIMIT : 0;
        return (nl_send_one(m, nlp, cnt, io_flags));
}

/* Shouldn't be called (maybe except Linux code originating message) */
static bool
nlmsg_write_group_lbuf(struct nl_writer *nw, void *buf, int datalen, int cnt)
{
	struct linear_buffer *lb = (struct linear_buffer *)buf;
	char *data = (char *)(lb + 1);

	if (__predict_false(datalen == 0)) {
		free(buf, M_NETLINK);
		return (true);
	}

	struct mbuf *m = m_getm2(NULL, datalen, nw->malloc_flag, MT_DATA, M_PKTHDR);
	if (__predict_false(m == NULL)) {
		free(buf, M_NETLINK);
		return (false);
	}
	m_append(m, datalen, data);
	free(buf, M_NETLINK);

        nl_send_group(m, cnt, nw->arg_uint >> 16, nw->arg_uint & 0xFFFF);
	return (true);
}

struct nlwriter_ops nlmsg_writers[] = {
	/* NS_WRITER_TYPE_MBUF */
	{
		.init = nlmsg_get_ns_mbuf,
		.write_socket = nlmsg_write_socket_mbuf,
		.write_group = nlmsg_write_group_mbuf,
		.write_chain = nlmsg_write_chain_mbuf,
	},
	/* NS_WRITER_TYPE_BUF */
	{
		.init = nlmsg_get_ns_buf,
		.write_socket = nlmsg_write_socket_buf,
		.write_group = nlmsg_write_group_buf,
		.write_chain = nlmsg_write_chain_buf,
	},
	/* NS_WRITER_TYPE_LBUF */
	{
		.init = nlmsg_get_ns_lbuf,
		.write_socket = nlmsg_write_socket_lbuf,
		.write_group = nlmsg_write_group_lbuf,
	},
};

static void
nlmsg_set_callback(struct nl_writer *nw)
{
	struct nlwriter_ops *pops = &nlmsg_writers[nw->writer_type];

	switch (nw->writer_target) {
	case NS_WRITER_TARGET_SOCKET:
		nw->cb = pops->write_socket;
		break;
	case NS_WRITER_TARGET_GROUP:
		nw->cb = pops->write_group;
		break;
	case NS_WRITER_TARGET_CHAIN:
		nw->cb = pops->write_chain;
		break;
	default:
		panic("not implemented");
	}
}

static bool
nlmsg_get_buf_type(struct nl_writer *nw, int size, int type, bool waitok)
{
	MPASS(type + 1 <= sizeof(nlmsg_writers) / sizeof(nlmsg_writers[0]));
	NL_LOG(LOG_DEBUG3, "Setting up nw %p size %d type %d", nw, size, type);
	return (nlmsg_writers[type].init(nw, size, waitok));
}

static bool
nlmsg_get_buf(struct nl_writer *nw, int size, bool waitok, bool is_linux)
{
	int type;

	if (!is_linux) {
		if (__predict_true(size <= MCLBYTES))
			type = NS_WRITER_TYPE_MBUF;
		else
			type = NS_WRITER_TYPE_BUF;
	} else
		type = NS_WRITER_TYPE_LBUF;
	return (nlmsg_get_buf_type(nw, size, type, waitok));
}

bool
nlmsg_get_unicast_writer(struct nl_writer *nw, int size, struct nlpcb *nlp)
{
        if (!nlmsg_get_buf(nw, size, false, nlp->nl_linux))
                return (false);
        nw->arg_ptr = (void *)nlp;
	nw->writer_target = NS_WRITER_TARGET_SOCKET;
	nlmsg_set_callback(nw);
        return (true);
}

bool
nlmsg_get_group_writer(struct nl_writer *nw, int size, int protocol, int group_id)
{
        if (!nlmsg_get_buf(nw, size, false, false))
                return (false);
        nw->arg_uint = (uint64_t)protocol << 16 | (uint64_t)group_id;
	nw->writer_target = NS_WRITER_TARGET_GROUP;
	nlmsg_set_callback(nw);
        return (true);
}

bool
nlmsg_get_chain_writer(struct nl_writer *nw, int size, struct mbuf **pm)
{
        if (!nlmsg_get_buf(nw, size, false, false))
                return (false);
	*pm = NULL;
        nw->arg_ptr = (void *)pm;
	nw->writer_target = NS_WRITER_TARGET_CHAIN;
	nlmsg_set_callback(nw);
	NL_LOG(LOG_DEBUG3, "setup cb %p (need %p)", nw->cb, &nlmsg_write_chain_mbuf);
        return (true);
}

void
nlmsg_ignore_limit(struct nl_writer *nw)
{
	nw->ignore_limit = true;
}

bool
nlmsg_flush(struct nl_writer *nw)
{

        if (__predict_false(nw->hdr != NULL)) {
                /* Last message has not been completed, skip it. */
                int completed_len = (char *)nw->hdr - nw->data;
		/* Send completed messages */
		nw->offset -= nw->offset - completed_len;
		nw->hdr = NULL;
        }

	NL_LOG(LOG_DEBUG2, "OUT");
        bool result = nw->cb(nw, nw->_storage, nw->offset, nw->num_messages);
        nw->_storage = NULL;

        if (!result) {
                NL_LOG(LOG_DEBUG, "nw %p offset %d: flush with %p() failed", nw, nw->offset, nw->cb);
        }

        return (result);
}

/*
 * Flushes previous data and allocates new underlying storage
 *  sufficient for holding at least @required_len bytes.
 * Return true on success.
 */
bool
nlmsg_refill_buffer(struct nl_writer *nw, int required_len)
{
        struct nl_writer ns_new = {};
        int completed_len, new_len;

	if (nw->enomem)
		return (false);

	NL_LOG(LOG_DEBUG3, "no space at offset %d/%d (want %d), trying to reclaim",
	    nw->offset, nw->alloc_len, required_len);

        /* Calculated new buffer size and allocate it s*/
	completed_len = (nw->hdr != NULL) ? (char *)nw->hdr - nw->data : nw->offset;
	if (completed_len > 0 && required_len < MCLBYTES) {
		/* We already ran out of space, use the largest effective size */
		new_len = max(nw->alloc_len, MCLBYTES);
	} else {
		if (nw->alloc_len < MCLBYTES)
			new_len = MCLBYTES;
		else
			new_len = nw->alloc_len * 2;
		while (new_len < required_len)
			new_len *= 2;
	}
	bool waitok = (nw->malloc_flag == M_WAITOK);
	bool is_linux = (nw->writer_type == NS_WRITER_TYPE_LBUF);
        if (!nlmsg_get_buf(&ns_new, new_len, waitok, is_linux)) {
		nw->enomem = true;
		NL_LOG(LOG_DEBUG, "getting new buf failed, setting ENOMEM");
                return (false);
	}
	if (nw->ignore_limit)
		nlmsg_ignore_limit(&ns_new);

	/* Update callback data */
	ns_new.writer_target = nw->writer_target;
	nlmsg_set_callback(&ns_new);
	ns_new.arg_uint = nw->arg_uint;

        /* Copy last (unfinished) header to the new storage */
        int last_len = nw->offset - completed_len;
	if (last_len > 0) {
		memcpy(ns_new.data, nw->hdr, last_len);
		ns_new.hdr = (struct nlmsghdr *)ns_new.data;
		ns_new.offset = last_len;
	}

        NL_LOG(LOG_DEBUG2, "completed: %d bytes, copied: %d bytes", completed_len, last_len);

        /* Flush completed headers & switch to the new nw */
	nlmsg_flush(nw);
	memcpy(nw, &ns_new, sizeof(struct nl_writer));
        NL_LOG(LOG_DEBUG2, "switched buffer: used %d/%d bytes", nw->offset, nw->alloc_len);

        return (true);
}

bool
nlmsg_add(struct nl_writer *nw, uint32_t portid, uint32_t seq, uint16_t type,
    uint16_t flags, uint32_t len)
{
	struct nlmsghdr *hdr;

	MPASS(nw->hdr == NULL);

	int required_len = NETLINK_ALIGN(len + sizeof(struct nlmsghdr));
        if (__predict_false(nw->offset + required_len > nw->alloc_len)) {
		if (!nlmsg_refill_buffer(nw, required_len))
			return (false);
        }

        hdr = (struct nlmsghdr *)(&nw->data[nw->offset]);

        hdr->nlmsg_len = len;
        hdr->nlmsg_type = type;
        hdr->nlmsg_flags = flags;
        hdr->nlmsg_seq = seq;
        hdr->nlmsg_pid = portid;

        nw->hdr = hdr;
        nw->offset += sizeof(struct nlmsghdr);

        return (true);
}

bool
nlmsg_end(struct nl_writer *nw)
{
	MPASS(nw->hdr != NULL);

	if (nw->enomem) {
		NL_LOG(LOG_DEBUG, "ENOMEM when dumping message");
		nlmsg_abort(nw);
		return (false);
	}

        nw->hdr->nlmsg_len = (uint32_t)(nw->data + nw->offset - (char *)nw->hdr);
        nw->hdr = NULL;
	nw->num_messages++;
	return (true);
}

void
nlmsg_abort(struct nl_writer *nw)
{
        if (nw->hdr != NULL) {
                nw->offset = (uint32_t)((char *)nw->hdr - nw->data);
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

	/*
	 * TODO: handle cookies
	 */

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

	if (nlmsg_end(nw))
		return;
enomem:
	NLP_LOG(LOG_DEBUG, nlp, "error allocating ack data for message %d seq %u",
	    hdr->nlmsg_type, hdr->nlmsg_seq);
	nlmsg_abort(nw);
}

bool
nlmsg_end_dump(struct nl_writer *nw, int error, struct nlmsghdr *hdr)
{
	if (!nlmsg_add(nw, hdr->nlmsg_pid, hdr->nlmsg_seq, NLMSG_DONE, 0, sizeof(int))) {
		NL_LOG(LOG_DEBUG, "Error finalizing table dump");
		return (false);
	}
	/* Save operation result */
	int *perror = nlmsg_reserve_object(nw, int);
	NL_LOG(LOG_DEBUG2, "record error=%d at off %d (%p)", error,
	    nw->offset, perror);
	*perror = error;
	nlmsg_end(nw);

	return (true);
}
