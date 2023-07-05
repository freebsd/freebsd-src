/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Software implementation of iSCSI Common Layer kobj(9) interface.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/gsb_crc32.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/uio.h>
#include <vm/uma.h>
#include <vm/vm_page.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <dev/iscsi/icl.h>
#include <dev/iscsi/iscsi_proto.h>
#include <icl_conn_if.h>

#define ICL_CONN_STATE_BHS		1
#define ICL_CONN_STATE_AHS		2
#define ICL_CONN_STATE_HEADER_DIGEST	3
#define ICL_CONN_STATE_DATA		4
#define ICL_CONN_STATE_DATA_DIGEST	5

struct icl_soft_conn {
	struct icl_conn	 ic;

	/* soft specific stuff goes here. */
	STAILQ_HEAD(, icl_pdu) to_send;
	struct cv	 send_cv;
	struct cv	 receive_cv;
	struct icl_pdu	*receive_pdu;
	size_t		 receive_len;
	int		 receive_state;
	bool		 receive_running;
	bool		 check_send_space;
	bool		 send_running;
};

struct icl_soft_pdu {
	struct icl_pdu	 ip;

	/* soft specific stuff goes here. */
	u_int		 ref_cnt;
	icl_pdu_cb	 cb;
	int		 error;
};

SYSCTL_NODE(_kern_icl, OID_AUTO, soft, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Software iSCSI");
static int coalesce = 1;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, coalesce, CTLFLAG_RWTUN,
    &coalesce, 0, "Try to coalesce PDUs before sending");
static int partial_receive_len = 256 * 1024;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, partial_receive_len, CTLFLAG_RWTUN,
    &partial_receive_len, 0, "Minimum read size for partially received "
    "data segment");
static int max_data_segment_length = 256 * 1024;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, max_data_segment_length, CTLFLAG_RWTUN,
    &max_data_segment_length, 0, "Maximum data segment length");
static int first_burst_length = 1024 * 1024;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, first_burst_length, CTLFLAG_RWTUN,
    &first_burst_length, 0, "First burst length");
static int max_burst_length = 1024 * 1024;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, max_burst_length, CTLFLAG_RWTUN,
    &max_burst_length, 0, "Maximum burst length");
static int sendspace = 1536 * 1024;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, sendspace, CTLFLAG_RWTUN,
    &sendspace, 0, "Default send socket buffer size");
static int recvspace = 1536 * 1024;
SYSCTL_INT(_kern_icl_soft, OID_AUTO, recvspace, CTLFLAG_RWTUN,
    &recvspace, 0, "Default receive socket buffer size");

static MALLOC_DEFINE(M_ICL_SOFT, "icl_soft", "iSCSI software backend");
static uma_zone_t icl_soft_pdu_zone;

static volatile u_int	icl_ncons;

STAILQ_HEAD(icl_pdu_stailq, icl_pdu);

static icl_conn_new_pdu_t	icl_soft_conn_new_pdu;
static icl_conn_pdu_free_t	icl_soft_conn_pdu_free;
static icl_conn_pdu_data_segment_length_t
				    icl_soft_conn_pdu_data_segment_length;
static icl_conn_pdu_append_bio_t	icl_soft_conn_pdu_append_bio;
static icl_conn_pdu_append_data_t	icl_soft_conn_pdu_append_data;
static icl_conn_pdu_get_bio_t	icl_soft_conn_pdu_get_bio;
static icl_conn_pdu_get_data_t	icl_soft_conn_pdu_get_data;
static icl_conn_pdu_queue_t	icl_soft_conn_pdu_queue;
static icl_conn_pdu_queue_cb_t	icl_soft_conn_pdu_queue_cb;
static icl_conn_handoff_t	icl_soft_conn_handoff;
static icl_conn_free_t		icl_soft_conn_free;
static icl_conn_close_t		icl_soft_conn_close;
static icl_conn_task_setup_t	icl_soft_conn_task_setup;
static icl_conn_task_done_t	icl_soft_conn_task_done;
static icl_conn_transfer_setup_t	icl_soft_conn_transfer_setup;
static icl_conn_transfer_done_t	icl_soft_conn_transfer_done;
#ifdef ICL_KERNEL_PROXY
static icl_conn_connect_t	icl_soft_conn_connect;
#endif

static kobj_method_t icl_soft_methods[] = {
	KOBJMETHOD(icl_conn_new_pdu, icl_soft_conn_new_pdu),
	KOBJMETHOD(icl_conn_pdu_free, icl_soft_conn_pdu_free),
	KOBJMETHOD(icl_conn_pdu_data_segment_length,
	    icl_soft_conn_pdu_data_segment_length),
	KOBJMETHOD(icl_conn_pdu_append_bio, icl_soft_conn_pdu_append_bio),
	KOBJMETHOD(icl_conn_pdu_append_data, icl_soft_conn_pdu_append_data),
	KOBJMETHOD(icl_conn_pdu_get_bio, icl_soft_conn_pdu_get_bio),
	KOBJMETHOD(icl_conn_pdu_get_data, icl_soft_conn_pdu_get_data),
	KOBJMETHOD(icl_conn_pdu_queue, icl_soft_conn_pdu_queue),
	KOBJMETHOD(icl_conn_pdu_queue_cb, icl_soft_conn_pdu_queue_cb),
	KOBJMETHOD(icl_conn_handoff, icl_soft_conn_handoff),
	KOBJMETHOD(icl_conn_free, icl_soft_conn_free),
	KOBJMETHOD(icl_conn_close, icl_soft_conn_close),
	KOBJMETHOD(icl_conn_task_setup, icl_soft_conn_task_setup),
	KOBJMETHOD(icl_conn_task_done, icl_soft_conn_task_done),
	KOBJMETHOD(icl_conn_transfer_setup, icl_soft_conn_transfer_setup),
	KOBJMETHOD(icl_conn_transfer_done, icl_soft_conn_transfer_done),
#ifdef ICL_KERNEL_PROXY
	KOBJMETHOD(icl_conn_connect, icl_soft_conn_connect),
#endif
	{ 0, 0 }
};

DEFINE_CLASS(icl_soft, icl_soft_methods, sizeof(struct icl_soft_conn));

static void
icl_conn_fail(struct icl_conn *ic)
{
	if (ic->ic_socket == NULL)
		return;

	/*
	 * XXX
	 */
	ic->ic_socket->so_error = EDOOFUS;
	(ic->ic_error)(ic);
}

static void
icl_soft_conn_pdu_free(struct icl_conn *ic, struct icl_pdu *ip)
{
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)ip;

	KASSERT(isp->ref_cnt == 0, ("freeing active PDU"));
	m_freem(ip->ip_bhs_mbuf);
	m_freem(ip->ip_ahs_mbuf);
	m_freem(ip->ip_data_mbuf);
	uma_zfree(icl_soft_pdu_zone, isp);
#ifdef DIAGNOSTIC
	refcount_release(&ic->ic_outstanding_pdus);
#endif
}

static void
icl_soft_pdu_call_cb(struct icl_pdu *ip)
{
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)ip;

	if (isp->cb != NULL)
		isp->cb(ip, isp->error);
#ifdef DIAGNOSTIC
	refcount_release(&ip->ip_conn->ic_outstanding_pdus);
#endif
	uma_zfree(icl_soft_pdu_zone, isp);
}

static void
icl_soft_pdu_done(struct icl_pdu *ip, int error)
{
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)ip;

	if (error != 0)
		isp->error = error;

	m_freem(ip->ip_bhs_mbuf);
	ip->ip_bhs_mbuf = NULL;
	m_freem(ip->ip_ahs_mbuf);
	ip->ip_ahs_mbuf = NULL;
	m_freem(ip->ip_data_mbuf);
	ip->ip_data_mbuf = NULL;

	if (atomic_fetchadd_int(&isp->ref_cnt, -1) == 1)
		icl_soft_pdu_call_cb(ip);
}

static void
icl_soft_mbuf_done(struct mbuf *mb)
{
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)mb->m_ext.ext_arg1;

	icl_soft_pdu_call_cb(&isp->ip);
}

/*
 * Allocate icl_pdu with empty BHS to fill up by the caller.
 */
struct icl_pdu *
icl_soft_conn_new_pdu(struct icl_conn *ic, int flags)
{
	struct icl_soft_pdu *isp;
	struct icl_pdu *ip;

#ifdef DIAGNOSTIC
	refcount_acquire(&ic->ic_outstanding_pdus);
#endif
	isp = uma_zalloc(icl_soft_pdu_zone, flags | M_ZERO);
	if (isp == NULL) {
		ICL_WARN("failed to allocate soft PDU");
#ifdef DIAGNOSTIC
		refcount_release(&ic->ic_outstanding_pdus);
#endif
		return (NULL);
	}
	ip = &isp->ip;
	ip->ip_conn = ic;

	CTASSERT(sizeof(struct iscsi_bhs) <= MHLEN);
	ip->ip_bhs_mbuf = m_gethdr(flags, MT_DATA);
	if (ip->ip_bhs_mbuf == NULL) {
		ICL_WARN("failed to allocate BHS mbuf");
		icl_soft_conn_pdu_free(ic, ip);
		return (NULL);
	}
	ip->ip_bhs = mtod(ip->ip_bhs_mbuf, struct iscsi_bhs *);
	memset(ip->ip_bhs, 0, sizeof(struct iscsi_bhs));
	ip->ip_bhs_mbuf->m_len = sizeof(struct iscsi_bhs);

	return (ip);
}

static int
icl_pdu_ahs_length(const struct icl_pdu *request)
{

	return (request->ip_bhs->bhs_total_ahs_len * 4);
}

static size_t
icl_pdu_data_segment_length(const struct icl_pdu *request)
{
	uint32_t len = 0;

	len += request->ip_bhs->bhs_data_segment_len[0];
	len <<= 8;
	len += request->ip_bhs->bhs_data_segment_len[1];
	len <<= 8;
	len += request->ip_bhs->bhs_data_segment_len[2];

	return (len);
}

size_t
icl_soft_conn_pdu_data_segment_length(struct icl_conn *ic,
    const struct icl_pdu *request)
{

	return (icl_pdu_data_segment_length(request));
}

static void
icl_pdu_set_data_segment_length(struct icl_pdu *response, uint32_t len)
{

	response->ip_bhs->bhs_data_segment_len[2] = len;
	response->ip_bhs->bhs_data_segment_len[1] = len >> 8;
	response->ip_bhs->bhs_data_segment_len[0] = len >> 16;
}

static size_t
icl_pdu_padding(const struct icl_pdu *ip)
{

	if ((ip->ip_data_len % 4) != 0)
		return (4 - (ip->ip_data_len % 4));

	return (0);
}

static size_t
icl_pdu_size(const struct icl_pdu *response)
{
	size_t len;

	KASSERT(response->ip_ahs_len == 0, ("responding with AHS"));

	len = sizeof(struct iscsi_bhs) + response->ip_data_len +
	    icl_pdu_padding(response);
	if (response->ip_conn->ic_header_crc32c)
		len += ISCSI_HEADER_DIGEST_SIZE;
	if (response->ip_data_len != 0 && response->ip_conn->ic_data_crc32c)
		len += ISCSI_DATA_DIGEST_SIZE;

	return (len);
}

static void
icl_soft_receive_buf(struct mbuf **r, size_t *rs, void *buf, size_t s)
{

	m_copydata(*r, 0, s, buf);
	m_adj(*r, s);
	while ((*r) != NULL && (*r)->m_len == 0)
		*r = m_free(*r);
	*rs -= s;
}

static void
icl_pdu_receive_ahs(struct icl_pdu *request, struct mbuf **r, size_t *rs)
{

	request->ip_ahs_len = icl_pdu_ahs_length(request);
	if (request->ip_ahs_len == 0)
		return;

	request->ip_ahs_mbuf = *r;
	*r = m_split(request->ip_ahs_mbuf, request->ip_ahs_len, M_WAITOK);
	*rs -= request->ip_ahs_len;
}

static int
mbuf_crc32c_helper(void *arg, void *data, u_int len)
{
	uint32_t *digestp = arg;

	*digestp = calculate_crc32c(*digestp, data, len);
	return (0);
}

static uint32_t
icl_mbuf_to_crc32c(struct mbuf *m0, size_t len)
{
	uint32_t digest = 0xffffffff;

	m_apply(m0, 0, len, mbuf_crc32c_helper, &digest);
	digest = digest ^ 0xffffffff;

	return (digest);
}

static int
icl_pdu_check_header_digest(struct icl_pdu *request, struct mbuf **r, size_t *rs)
{
	uint32_t received_digest, valid_digest;

	if (request->ip_conn->ic_header_crc32c == false)
		return (0);

	CTASSERT(sizeof(received_digest) == ISCSI_HEADER_DIGEST_SIZE);
	icl_soft_receive_buf(r, rs, &received_digest, ISCSI_HEADER_DIGEST_SIZE);

	/* Temporary attach AHS to BHS to calculate header digest. */
	request->ip_bhs_mbuf->m_next = request->ip_ahs_mbuf;
	valid_digest = icl_mbuf_to_crc32c(request->ip_bhs_mbuf, ISCSI_BHS_SIZE);
	request->ip_bhs_mbuf->m_next = NULL;
	if (received_digest != valid_digest) {
		ICL_WARN("header digest check failed; got 0x%x, "
		    "should be 0x%x", received_digest, valid_digest);
		return (-1);
	}

	return (0);
}

/*
 * Return the number of bytes that should be waiting in the receive socket
 * before icl_pdu_receive_data_segment() gets called.
 */
static size_t
icl_pdu_data_segment_receive_len(const struct icl_pdu *request)
{
	size_t len;

	len = icl_pdu_data_segment_length(request);
	if (len == 0)
		return (0);

	/*
	 * Account for the parts of data segment already read from
	 * the socket buffer.
	 */
	KASSERT(len > request->ip_data_len, ("len <= request->ip_data_len"));
	len -= request->ip_data_len;

	/*
	 * Don't always wait for the full data segment to be delivered
	 * to the socket; this might badly affect performance due to
	 * TCP window scaling.
	 */
	if (len > partial_receive_len) {
#if 0
		ICL_DEBUG("need %zd bytes of data, limiting to %zd",
		    len, partial_receive_len));
#endif
		len = partial_receive_len;

		return (len);
	}

	/*
	 * Account for padding.  Note that due to the way code is written,
	 * the icl_pdu_receive_data_segment() must always receive padding
	 * along with the last part of data segment, because it would be
	 * impossible to tell whether we've already received the full data
	 * segment including padding, or without it.
	 */
	if ((len % 4) != 0)
		len += 4 - (len % 4);

#if 0
	ICL_DEBUG("need %zd bytes of data", len));
#endif

	return (len);
}

static int
icl_pdu_receive_data_segment(struct icl_pdu *request, struct mbuf **r,
    size_t *rs, bool *more_neededp)
{
	struct icl_soft_conn *isc;
	size_t len, padding = 0;
	struct mbuf *m;

	isc = (struct icl_soft_conn *)request->ip_conn;

	*more_neededp = false;
	isc->receive_len = 0;

	len = icl_pdu_data_segment_length(request);
	if (len == 0)
		return (0);

	if ((len % 4) != 0)
		padding = 4 - (len % 4);

	/*
	 * Account for already received parts of data segment.
	 */
	KASSERT(len > request->ip_data_len, ("len <= request->ip_data_len"));
	len -= request->ip_data_len;

	if (len + padding > *rs) {
		/*
		 * Not enough data in the socket buffer.  Receive as much
		 * as we can.  Don't receive padding, since, obviously, it's
		 * not the end of data segment yet.
		 */
#if 0
		ICL_DEBUG("limited from %zd to %zd",
		    len + padding, *rs - padding));
#endif
		len = *rs - padding;
		*more_neededp = true;
		padding = 0;
	}

	/*
	 * Must not try to receive padding without at least one byte
	 * of actual data segment.
	 */
	if (len > 0) {
		m = *r;
		*r = m_split(m, len + padding, M_WAITOK);
		*rs -= len + padding;

		if (request->ip_data_mbuf == NULL)
			request->ip_data_mbuf = m;
		else
			m_cat(request->ip_data_mbuf, m);

		request->ip_data_len += len;
	} else
		ICL_DEBUG("len 0");

	if (*more_neededp)
		isc->receive_len = icl_pdu_data_segment_receive_len(request);

	return (0);
}

static int
icl_pdu_check_data_digest(struct icl_pdu *request, struct mbuf **r, size_t *rs)
{
	uint32_t received_digest, valid_digest;

	if (request->ip_conn->ic_data_crc32c == false)
		return (0);

	if (request->ip_data_len == 0)
		return (0);

	CTASSERT(sizeof(received_digest) == ISCSI_DATA_DIGEST_SIZE);
	icl_soft_receive_buf(r, rs, &received_digest, ISCSI_DATA_DIGEST_SIZE);

	/*
	 * Note that ip_data_mbuf also contains padding; since digest
	 * calculation is supposed to include that, we iterate over
	 * the entire ip_data_mbuf chain, not just ip_data_len bytes of it.
	 */
	valid_digest = icl_mbuf_to_crc32c(request->ip_data_mbuf,
	    roundup2(request->ip_data_len, 4));
	if (received_digest != valid_digest) {
		ICL_WARN("data digest check failed; got 0x%x, "
		    "should be 0x%x", received_digest, valid_digest);
		return (-1);
	}

	return (0);
}

/*
 * Somewhat contrary to the name, this attempts to receive only one
 * "part" of PDU at a time; call it repeatedly until it returns non-NULL.
 */
static struct icl_pdu *
icl_conn_receive_pdu(struct icl_soft_conn *isc, struct mbuf **r, size_t *rs)
{
	struct icl_conn *ic = &isc->ic;
	struct icl_pdu *request;
	size_t len;
	int error = 0;
	bool more_needed;

	if (isc->receive_state == ICL_CONN_STATE_BHS) {
		KASSERT(isc->receive_pdu == NULL,
		    ("isc->receive_pdu != NULL"));
		request = icl_soft_conn_new_pdu(ic, M_NOWAIT);
		if (request == NULL) {
			ICL_DEBUG("failed to allocate PDU; "
			    "dropping connection");
			icl_conn_fail(ic);
			return (NULL);
		}
		isc->receive_pdu = request;
	} else {
		KASSERT(isc->receive_pdu != NULL,
		    ("isc->receive_pdu == NULL"));
		request = isc->receive_pdu;
	}

	switch (isc->receive_state) {
	case ICL_CONN_STATE_BHS:
		//ICL_DEBUG("receiving BHS");
		icl_soft_receive_buf(r, rs, request->ip_bhs,
		    sizeof(struct iscsi_bhs));

		/*
		 * We don't enforce any limit for AHS length;
		 * its length is stored in 8 bit field.
		 */

		len = icl_pdu_data_segment_length(request);
		if (len > ic->ic_max_recv_data_segment_length) {
			ICL_WARN("received data segment "
			    "length %zd is larger than negotiated; "
			    "dropping connection", len);
			error = EINVAL;
			break;
		}

		isc->receive_state = ICL_CONN_STATE_AHS;
		isc->receive_len = icl_pdu_ahs_length(request);
		break;

	case ICL_CONN_STATE_AHS:
		//ICL_DEBUG("receiving AHS");
		icl_pdu_receive_ahs(request, r, rs);
		isc->receive_state = ICL_CONN_STATE_HEADER_DIGEST;
		if (ic->ic_header_crc32c == false)
			isc->receive_len = 0;
		else
			isc->receive_len = ISCSI_HEADER_DIGEST_SIZE;
		break;

	case ICL_CONN_STATE_HEADER_DIGEST:
		//ICL_DEBUG("receiving header digest");
		error = icl_pdu_check_header_digest(request, r, rs);
		if (error != 0) {
			ICL_DEBUG("header digest failed; "
			    "dropping connection");
			break;
		}

		isc->receive_state = ICL_CONN_STATE_DATA;
		isc->receive_len = icl_pdu_data_segment_receive_len(request);
		break;

	case ICL_CONN_STATE_DATA:
		//ICL_DEBUG("receiving data segment");
		error = icl_pdu_receive_data_segment(request, r, rs,
		    &more_needed);
		if (error != 0) {
			ICL_DEBUG("failed to receive data segment;"
			    "dropping connection");
			break;
		}

		if (more_needed)
			break;

		isc->receive_state = ICL_CONN_STATE_DATA_DIGEST;
		if (request->ip_data_len == 0 || ic->ic_data_crc32c == false)
			isc->receive_len = 0;
		else
			isc->receive_len = ISCSI_DATA_DIGEST_SIZE;
		break;

	case ICL_CONN_STATE_DATA_DIGEST:
		//ICL_DEBUG("receiving data digest");
		error = icl_pdu_check_data_digest(request, r, rs);
		if (error != 0) {
			ICL_DEBUG("data digest failed; "
			    "dropping connection");
			break;
		}

		/*
		 * We've received complete PDU; reset the receive state machine
		 * and return the PDU.
		 */
		isc->receive_state = ICL_CONN_STATE_BHS;
		isc->receive_len = sizeof(struct iscsi_bhs);
		isc->receive_pdu = NULL;
		return (request);

	default:
		panic("invalid receive_state %d\n", isc->receive_state);
	}

	if (error != 0) {
		/*
		 * Don't free the PDU; it's pointed to by isc->receive_pdu
		 * and will get freed in icl_soft_conn_close().
		 */
		icl_conn_fail(ic);
	}

	return (NULL);
}

static void
icl_conn_receive_pdus(struct icl_soft_conn *isc, struct mbuf **r, size_t *rs)
{
	struct icl_conn *ic = &isc->ic;
	struct icl_pdu *response;

	for (;;) {
		if (ic->ic_disconnecting)
			return;

		/*
		 * Loop until we have a complete PDU or there is not enough
		 * data in the socket buffer.
		 */
		if (*rs < isc->receive_len) {
#if 0
			ICL_DEBUG("not enough data; have %zd, need %zd",
			    *rs, isc->receive_len);
#endif
			return;
		}

		response = icl_conn_receive_pdu(isc, r, rs);
		if (response == NULL)
			continue;

		if (response->ip_ahs_len > 0) {
			ICL_WARN("received PDU with unsupported "
			    "AHS; opcode 0x%x; dropping connection",
			    response->ip_bhs->bhs_opcode);
			icl_soft_conn_pdu_free(ic, response);
			icl_conn_fail(ic);
			return;
		}

		(ic->ic_receive)(response);
	}
}

static void
icl_receive_thread(void *arg)
{
	struct icl_soft_conn *isc = arg;
	struct icl_conn *ic = &isc->ic;
	size_t available, read = 0;
	struct socket *so;
	struct mbuf *m, *r = NULL;
	struct uio uio;
	int error, flags;

	so = ic->ic_socket;

	for (;;) {
		SOCKBUF_LOCK(&so->so_rcv);
		if (ic->ic_disconnecting) {
			SOCKBUF_UNLOCK(&so->so_rcv);
			break;
		}

		/*
		 * Set the low watermark, to be checked by
		 * soreadable() in icl_soupcall_receive()
		 * to avoid unnecessary wakeups until there
		 * is enough data received to read the PDU.
		 */
		available = sbavail(&so->so_rcv);
		if (read + available < isc->receive_len) {
			so->so_rcv.sb_lowat = isc->receive_len - read;
			cv_wait(&isc->receive_cv, SOCKBUF_MTX(&so->so_rcv));
			so->so_rcv.sb_lowat = so->so_rcv.sb_hiwat + 1;
			available = sbavail(&so->so_rcv);
		}
		SOCKBUF_UNLOCK(&so->so_rcv);

		if (available == 0) {
			if (so->so_error != 0) {
				ICL_DEBUG("connection error %d; "
				    "dropping connection", so->so_error);
				icl_conn_fail(ic);
				break;
			}
			continue;
		}

		memset(&uio, 0, sizeof(uio));
		uio.uio_resid = available;
		flags = MSG_DONTWAIT;
		error = soreceive(so, NULL, &uio, &m, NULL, &flags);
		if (error != 0) {
			ICL_DEBUG("soreceive error %d", error);
			break;
		}
		if (uio.uio_resid != 0) {
			m_freem(m);
			ICL_DEBUG("short read");
			break;
		}
		if (r)
			m_cat(r, m);
		else
			r = m;
		read += available;

		icl_conn_receive_pdus(isc, &r, &read);
	}

	if (r)
		m_freem(r);

	ICL_CONN_LOCK(ic);
	isc->receive_running = false;
	cv_signal(&isc->send_cv);
	ICL_CONN_UNLOCK(ic);
	kthread_exit();
}

static int
icl_soupcall_receive(struct socket *so, void *arg, int waitflag)
{
	struct icl_soft_conn *isc;

	if (!soreadable(so))
		return (SU_OK);

	isc = arg;
	cv_signal(&isc->receive_cv);
	return (SU_OK);
}

static int
icl_pdu_finalize(struct icl_pdu *request)
{
	size_t padding, pdu_len;
	uint32_t digest, zero = 0;
	int ok;
	struct icl_conn *ic;

	ic = request->ip_conn;

	icl_pdu_set_data_segment_length(request, request->ip_data_len);

	pdu_len = icl_pdu_size(request);

	if (ic->ic_header_crc32c) {
		digest = icl_mbuf_to_crc32c(request->ip_bhs_mbuf,
		    ISCSI_BHS_SIZE);
		ok = m_append(request->ip_bhs_mbuf, sizeof(digest),
		    (void *)&digest);
		if (ok != 1) {
			ICL_WARN("failed to append header digest");
			return (1);
		}
	}

	if (request->ip_data_len != 0) {
		padding = icl_pdu_padding(request);
		if (padding > 0) {
			ok = m_append(request->ip_data_mbuf, padding,
			    (void *)&zero);
			if (ok != 1) {
				ICL_WARN("failed to append padding");
				return (1);
			}
		}

		if (ic->ic_data_crc32c) {
			digest = icl_mbuf_to_crc32c(request->ip_data_mbuf,
			    roundup2(request->ip_data_len, 4));

			ok = m_append(request->ip_data_mbuf, sizeof(digest),
			    (void *)&digest);
			if (ok != 1) {
				ICL_WARN("failed to append data digest");
				return (1);
			}
		}

		m_cat(request->ip_bhs_mbuf, request->ip_data_mbuf);
		request->ip_data_mbuf = NULL;
	}

	request->ip_bhs_mbuf->m_pkthdr.len = pdu_len;

	return (0);
}

static void
icl_conn_send_pdus(struct icl_soft_conn *isc, struct icl_pdu_stailq *queue)
{
	struct icl_conn *ic = &isc->ic;
	struct icl_pdu *request, *request2;
	struct mbuf *m;
	struct socket *so;
	long available, size, size2;
#ifdef DEBUG_COALESCED
	int coalesced;
#endif
	int error;

	ICL_CONN_LOCK_ASSERT_NOT(ic);

	so = ic->ic_socket;

	SOCKBUF_LOCK(&so->so_snd);
	/*
	 * Check how much space do we have for transmit.  We can't just
	 * call sosend() and retry when we get EWOULDBLOCK or EMSGSIZE,
	 * as it always frees the mbuf chain passed to it, even in case
	 * of error.
	 */
	available = sbspace(&so->so_snd);
	isc->check_send_space = false;

	/*
	 * Notify the socket upcall that we don't need wakeups
	 * for the time being.
	 */
	so->so_snd.sb_lowat = so->so_snd.sb_hiwat + 1;
	SOCKBUF_UNLOCK(&so->so_snd);

	while (!STAILQ_EMPTY(queue)) {
		request = STAILQ_FIRST(queue);
		size = icl_pdu_size(request);
		if (available < size) {
			/*
			 * Set the low watermark, to be checked by
			 * sowriteable() in icl_soupcall_send()
			 * to avoid unnecessary wakeups until there
			 * is enough space for the PDU to fit.
			 */
			SOCKBUF_LOCK(&so->so_snd);
			available = sbspace(&so->so_snd);
			if (available < size) {
#if 1
				ICL_DEBUG("no space to send; "
				    "have %ld, need %ld",
				    available, size);
#endif
				so->so_snd.sb_lowat = max(size,
				    so->so_snd.sb_hiwat / 8);
				SOCKBUF_UNLOCK(&so->so_snd);
				return;
			}
			SOCKBUF_UNLOCK(&so->so_snd);
		}
		STAILQ_REMOVE_HEAD(queue, ip_next);
		error = icl_pdu_finalize(request);
		if (error != 0) {
			ICL_DEBUG("failed to finalize PDU; "
			    "dropping connection");
			icl_soft_pdu_done(request, EIO);
			icl_conn_fail(ic);
			return;
		}
		if (coalesce) {
			m = request->ip_bhs_mbuf;
			for (
#ifdef DEBUG_COALESCED
			    coalesced = 1
#endif
			    ; ;
#ifdef DEBUG_COALESCED
			    coalesced++
#endif
			    ) {
				request2 = STAILQ_FIRST(queue);
				if (request2 == NULL)
					break;
				size2 = icl_pdu_size(request2);
				if (available < size + size2)
					break;
				STAILQ_REMOVE_HEAD(queue, ip_next);
				error = icl_pdu_finalize(request2);
				if (error != 0) {
					ICL_DEBUG("failed to finalize PDU; "
					    "dropping connection");
					icl_soft_pdu_done(request, EIO);
					icl_soft_pdu_done(request2, EIO);
					icl_conn_fail(ic);
					return;
				}
				while (m->m_next)
					m = m->m_next;
				m_cat(m, request2->ip_bhs_mbuf);
				request2->ip_bhs_mbuf = NULL;
				request->ip_bhs_mbuf->m_pkthdr.len += size2;
				size += size2;
				icl_soft_pdu_done(request2, 0);
			}
#ifdef DEBUG_COALESCED
			if (coalesced > 1) {
				ICL_DEBUG("coalesced %d PDUs into %ld bytes",
				    coalesced, size);
			}
#endif
		}
		available -= size;
		error = sosend(so, NULL, NULL, request->ip_bhs_mbuf,
		    NULL, MSG_DONTWAIT, curthread);
		request->ip_bhs_mbuf = NULL; /* Sosend consumes the mbuf. */
		if (error != 0) {
			ICL_DEBUG("failed to send PDU, error %d; "
			    "dropping connection", error);
			icl_soft_pdu_done(request, error);
			icl_conn_fail(ic);
			return;
		}
		icl_soft_pdu_done(request, 0);
	}
}

static void
icl_send_thread(void *arg)
{
	struct icl_soft_conn *isc;
	struct icl_conn *ic;
	struct icl_pdu_stailq queue;

	isc = arg;
	ic = &isc->ic;

	STAILQ_INIT(&queue);

	ICL_CONN_LOCK(ic);
	for (;;) {
		for (;;) {
			/*
			 * Populate the local queue from the main one.
			 * This way the icl_conn_send_pdus() can go through
			 * all the queued PDUs without holding any locks.
			 */
			if (STAILQ_EMPTY(&queue) || isc->check_send_space)
				STAILQ_CONCAT(&queue, &isc->to_send);

			ICL_CONN_UNLOCK(ic);
			icl_conn_send_pdus(isc, &queue);
			ICL_CONN_LOCK(ic);

			/*
			 * The icl_soupcall_send() was called since the last
			 * call to sbspace(); go around;
			 */
			if (isc->check_send_space)
				continue;

			/*
			 * Local queue is empty, but we still have PDUs
			 * in the main one; go around.
			 */
			if (STAILQ_EMPTY(&queue) &&
			    !STAILQ_EMPTY(&isc->to_send))
				continue;

			/*
			 * There might be some stuff in the local queue,
			 * which didn't get sent due to not having enough send
			 * space.  Wait for socket upcall.
			 */
			break;
		}

		if (ic->ic_disconnecting) {
			//ICL_DEBUG("terminating");
			break;
		}

		cv_wait(&isc->send_cv, ic->ic_lock);
	}

	/*
	 * We're exiting; move PDUs back to the main queue, so they can
	 * get freed properly.  At this point ordering doesn't matter.
	 */
	STAILQ_CONCAT(&isc->to_send, &queue);

	isc->send_running = false;
	cv_signal(&isc->send_cv);
	ICL_CONN_UNLOCK(ic);
	kthread_exit();
}

static int
icl_soupcall_send(struct socket *so, void *arg, int waitflag)
{
	struct icl_soft_conn *isc;
	struct icl_conn *ic;

	if (!sowriteable(so))
		return (SU_OK);

	isc = arg;
	ic = &isc->ic;

	ICL_CONN_LOCK(ic);
	isc->check_send_space = true;
	ICL_CONN_UNLOCK(ic);

	cv_signal(&isc->send_cv);

	return (SU_OK);
}

static void
icl_soft_free_mext_pg(struct mbuf *m)
{
	struct icl_soft_pdu *isp;

	M_ASSERTEXTPG(m);

	/*
	 * Nothing to do for the pages; they are owned by the PDU /
	 * I/O request.
	 */

	/* Drop reference on the PDU. */
	isp = m->m_ext.ext_arg1;
	if (atomic_fetchadd_int(&isp->ref_cnt, -1) == 1)
		icl_soft_pdu_call_cb(&isp->ip);
}

static int
icl_soft_conn_pdu_append_bio(struct icl_conn *ic, struct icl_pdu *request,
    struct bio *bp, size_t offset, size_t len, int flags)
{
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)request;
	struct mbuf *m, *m_tail;
	vm_offset_t vaddr;
	size_t mtodo, page_offset, todo;
	int i;

	KASSERT(len > 0, ("len == 0"));

	m_tail = request->ip_data_mbuf;
	if (m_tail != NULL)
		for (; m_tail->m_next != NULL; m_tail = m_tail->m_next)
			;

	MPASS(bp->bio_flags & BIO_UNMAPPED);
	if (offset < PAGE_SIZE - bp->bio_ma_offset) {
		page_offset = bp->bio_ma_offset + offset;
		i = 0;
	} else {
		offset -= PAGE_SIZE - bp->bio_ma_offset;
		for (i = 1; offset >= PAGE_SIZE; i++)
			offset -= PAGE_SIZE;
		page_offset = offset;
	}

	if (flags & ICL_NOCOPY) {
		m = NULL;
		while (len > 0) {
			if (m == NULL) {
				m = mb_alloc_ext_pgs(flags & ~ICL_NOCOPY,
				    icl_soft_free_mext_pg);
				if (__predict_false(m == NULL))
					return (ENOMEM);
				atomic_add_int(&isp->ref_cnt, 1);
				m->m_ext.ext_arg1 = isp;
				m->m_epg_1st_off = page_offset;
			}

			todo = MIN(len, PAGE_SIZE - page_offset);

			m->m_epg_pa[m->m_epg_npgs] =
			    VM_PAGE_TO_PHYS(bp->bio_ma[i]);
			m->m_epg_npgs++;
			m->m_epg_last_len = todo;
			m->m_len += todo;
			m->m_ext.ext_size += PAGE_SIZE;
			MBUF_EXT_PGS_ASSERT_SANITY(m);

			if (m->m_epg_npgs == MBUF_PEXT_MAX_PGS) {
				if (m_tail != NULL)
					m_tail->m_next = m;
				else
					request->ip_data_mbuf = m;
				m_tail = m;
				request->ip_data_len += m->m_len;
				m = NULL;
			}

			page_offset = 0;
			len -= todo;
			i++;
		}

		if (m != NULL) {
			if (m_tail != NULL)
				m_tail->m_next = m;
			else
				request->ip_data_mbuf = m;
			request->ip_data_len += m->m_len;
		}
		return (0);
	}

	m = m_getm2(NULL, len, flags, MT_DATA, 0);
	if (__predict_false(m == NULL))
		return (ENOMEM);

	if (request->ip_data_mbuf == NULL) {
		request->ip_data_mbuf = m;
		request->ip_data_len = len;
	} else {
		m_tail->m_next = m;
		request->ip_data_len += len;
	}

	while (len > 0) {
		todo = MIN(len, PAGE_SIZE - page_offset);
		vaddr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(bp->bio_ma[i]));

		do {
			mtodo = min(todo, M_SIZE(m) - m->m_len);
			memcpy(mtod(m, char *) + m->m_len, (char *)vaddr +
			    page_offset, mtodo);
			m->m_len += mtodo;
			if (m->m_len == M_SIZE(m))
				m = m->m_next;
			page_offset += mtodo;
			todo -= mtodo;
		} while (todo > 0);

		page_offset = 0;
		len -= todo;
		i++;
	}

	return (0);
}

static int
icl_soft_conn_pdu_append_data(struct icl_conn *ic, struct icl_pdu *request,
    const void *addr, size_t len, int flags)
{
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)request;
	struct mbuf *mb, *newmb;
	size_t copylen, off = 0;

	KASSERT(len > 0, ("len == 0"));

	if (flags & ICL_NOCOPY) {
		newmb = m_get(flags & ~ICL_NOCOPY, MT_DATA);
		if (newmb == NULL) {
			ICL_WARN("failed to allocate mbuf");
			return (ENOMEM);
		}

		newmb->m_flags |= M_RDONLY;
		m_extaddref(newmb, __DECONST(char *, addr), len, &isp->ref_cnt,
		    icl_soft_mbuf_done, isp, NULL);
		newmb->m_len = len;
	} else {
		newmb = m_getm2(NULL, len, flags, MT_DATA, 0);
		if (newmb == NULL) {
			ICL_WARN("failed to allocate mbuf for %zd bytes", len);
			return (ENOMEM);
		}

		for (mb = newmb; mb != NULL; mb = mb->m_next) {
			copylen = min(M_TRAILINGSPACE(mb), len - off);
			memcpy(mtod(mb, char *), (const char *)addr + off, copylen);
			mb->m_len = copylen;
			off += copylen;
		}
		KASSERT(off == len, ("%s: off != len", __func__));
	}

	if (request->ip_data_mbuf == NULL) {
		request->ip_data_mbuf = newmb;
		request->ip_data_len = len;
	} else {
		m_cat(request->ip_data_mbuf, newmb);
		request->ip_data_len += len;
	}

	return (0);
}

void
icl_soft_conn_pdu_get_bio(struct icl_conn *ic, struct icl_pdu *ip,
    size_t pdu_off, struct bio *bp, size_t bio_off, size_t len)
{
	vm_offset_t vaddr;
	size_t page_offset, todo;
	int i __unused;

	MPASS(bp->bio_flags & BIO_UNMAPPED);
	if (bio_off < PAGE_SIZE - bp->bio_ma_offset) {
		page_offset = bp->bio_ma_offset + bio_off;
		i = 0;
	} else {
		bio_off -= PAGE_SIZE - bp->bio_ma_offset;
		for (i = 1; bio_off >= PAGE_SIZE; i++)
			bio_off -= PAGE_SIZE;
		page_offset = bio_off;
	}

	while (len > 0) {
		todo = MIN(len, PAGE_SIZE - page_offset);

		vaddr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(bp->bio_ma[i]));
		m_copydata(ip->ip_data_mbuf, pdu_off, todo, (char *)vaddr +
		    page_offset);

		page_offset = 0;
		pdu_off += todo;
		len -= todo;
		i++;
	}
}

void
icl_soft_conn_pdu_get_data(struct icl_conn *ic, struct icl_pdu *ip,
    size_t off, void *addr, size_t len)
{

	m_copydata(ip->ip_data_mbuf, off, len, addr);
}

static void
icl_soft_conn_pdu_queue(struct icl_conn *ic, struct icl_pdu *ip)
{

	icl_soft_conn_pdu_queue_cb(ic, ip, NULL);
}

static void
icl_soft_conn_pdu_queue_cb(struct icl_conn *ic, struct icl_pdu *ip,
    icl_pdu_cb cb)
{
	struct icl_soft_conn *isc = (struct icl_soft_conn *)ic;
	struct icl_soft_pdu *isp = (struct icl_soft_pdu *)ip;

	ICL_CONN_LOCK_ASSERT(ic);
	isp->ref_cnt++;
	isp->cb = cb;

	if (ic->ic_disconnecting || ic->ic_socket == NULL) {
		ICL_DEBUG("icl_pdu_queue on closed connection");
		icl_soft_pdu_done(ip, ENOTCONN);
		return;
	}

	if (!STAILQ_EMPTY(&isc->to_send)) {
		STAILQ_INSERT_TAIL(&isc->to_send, ip, ip_next);
		/*
		 * If the queue is not empty, someone else had already
		 * signaled the send thread; no need to do that again,
		 * just return.
		 */
		return;
	}

	STAILQ_INSERT_TAIL(&isc->to_send, ip, ip_next);
	cv_signal(&isc->send_cv);
}

static struct icl_conn *
icl_soft_new_conn(const char *name, struct mtx *lock)
{
	struct icl_soft_conn *isc;
	struct icl_conn *ic;

	refcount_acquire(&icl_ncons);

	isc = (struct icl_soft_conn *)kobj_create(&icl_soft_class, M_ICL_SOFT,
	    M_WAITOK | M_ZERO);

	STAILQ_INIT(&isc->to_send);
	cv_init(&isc->send_cv, "icl_tx");
	cv_init(&isc->receive_cv, "icl_rx");

	ic = &isc->ic;
	ic->ic_lock = lock;
#ifdef DIAGNOSTIC
	refcount_init(&ic->ic_outstanding_pdus, 0);
#endif
	ic->ic_name = name;
	ic->ic_offload = "None";
	ic->ic_unmapped = PMAP_HAS_DMAP;

	return (ic);
}

void
icl_soft_conn_free(struct icl_conn *ic)
{
	struct icl_soft_conn *isc = (struct icl_soft_conn *)ic;

#ifdef DIAGNOSTIC
	KASSERT(ic->ic_outstanding_pdus == 0,
	    ("destroying session with %d outstanding PDUs",
	     ic->ic_outstanding_pdus));
#endif
	cv_destroy(&isc->send_cv);
	cv_destroy(&isc->receive_cv);
	kobj_delete((struct kobj *)isc, M_ICL_SOFT);
	refcount_release(&icl_ncons);
}

static int
icl_conn_start(struct icl_conn *ic)
{
	struct icl_soft_conn *isc = (struct icl_soft_conn *)ic;
	size_t minspace;
	struct sockopt opt;
	int error, one = 1;

	ICL_CONN_LOCK(ic);

	/*
	 * XXX: Ugly hack.
	 */
	if (ic->ic_socket == NULL) {
		ICL_CONN_UNLOCK(ic);
		return (EINVAL);
	}

	isc->receive_state = ICL_CONN_STATE_BHS;
	isc->receive_len = sizeof(struct iscsi_bhs);
	ic->ic_disconnecting = false;

	ICL_CONN_UNLOCK(ic);

	/*
	 * For sendspace, this is required because the current code cannot
	 * send a PDU in pieces; thus, the minimum buffer size is equal
	 * to the maximum PDU size.  "+4" is to account for possible padding.
	 */
	minspace = sizeof(struct iscsi_bhs) +
	    ic->ic_max_send_data_segment_length +
	    ISCSI_HEADER_DIGEST_SIZE + ISCSI_DATA_DIGEST_SIZE + 4;
	if (sendspace < minspace) {
		ICL_WARN("kern.icl.sendspace too low; must be at least %zd",
		    minspace);
		sendspace = minspace;
	}
	minspace = sizeof(struct iscsi_bhs) +
	    ic->ic_max_recv_data_segment_length +
	    ISCSI_HEADER_DIGEST_SIZE + ISCSI_DATA_DIGEST_SIZE + 4;
	if (recvspace < minspace) {
		ICL_WARN("kern.icl.recvspace too low; must be at least %zd",
		    minspace);
		recvspace = minspace;
	}

	error = soreserve(ic->ic_socket, sendspace, recvspace);
	if (error != 0) {
		ICL_WARN("soreserve failed with error %d", error);
		icl_soft_conn_close(ic);
		return (error);
	}
	ic->ic_socket->so_snd.sb_flags |= SB_AUTOSIZE;
	ic->ic_socket->so_rcv.sb_flags |= SB_AUTOSIZE;

	/*
	 * Disable Nagle.
	 */
	bzero(&opt, sizeof(opt));
	opt.sopt_dir = SOPT_SET;
	opt.sopt_level = IPPROTO_TCP;
	opt.sopt_name = TCP_NODELAY;
	opt.sopt_val = &one;
	opt.sopt_valsize = sizeof(one);
	error = sosetopt(ic->ic_socket, &opt);
	if (error != 0) {
		ICL_WARN("disabling TCP_NODELAY failed with error %d", error);
		icl_soft_conn_close(ic);
		return (error);
	}

	/*
	 * Register socket upcall, to get notified about incoming PDUs
	 * and free space to send outgoing ones.
	 */
	SOCKBUF_LOCK(&ic->ic_socket->so_snd);
	soupcall_set(ic->ic_socket, SO_SND, icl_soupcall_send, isc);
	SOCKBUF_UNLOCK(&ic->ic_socket->so_snd);
	SOCKBUF_LOCK(&ic->ic_socket->so_rcv);
	soupcall_set(ic->ic_socket, SO_RCV, icl_soupcall_receive, isc);
	SOCKBUF_UNLOCK(&ic->ic_socket->so_rcv);

	/*
	 * Start threads.
	 */
	ICL_CONN_LOCK(ic);
	isc->send_running = isc->receive_running = true;
	ICL_CONN_UNLOCK(ic);
	error = kthread_add(icl_send_thread, ic, NULL, NULL, 0, 0, "%stx",
	    ic->ic_name);
	if (error != 0) {
		ICL_WARN("kthread_add(9) failed with error %d", error);
		ICL_CONN_LOCK(ic);
		isc->send_running = isc->receive_running = false;
		cv_signal(&isc->send_cv);
		ICL_CONN_UNLOCK(ic);
		icl_soft_conn_close(ic);
		return (error);
	}
	error = kthread_add(icl_receive_thread, ic, NULL, NULL, 0, 0, "%srx",
	    ic->ic_name);
	if (error != 0) {
		ICL_WARN("kthread_add(9) failed with error %d", error);
		ICL_CONN_LOCK(ic);
		isc->receive_running = false;
		cv_signal(&isc->send_cv);
		ICL_CONN_UNLOCK(ic);
		icl_soft_conn_close(ic);
		return (error);
	}

	return (0);
}

int
icl_soft_conn_handoff(struct icl_conn *ic, int fd)
{
	struct file *fp;
	struct socket *so;
	cap_rights_t rights;
	int error;

	ICL_CONN_LOCK_ASSERT_NOT(ic);

#ifdef ICL_KERNEL_PROXY
	/*
	 * We're transitioning to Full Feature phase, and we don't
	 * really care.
	 */
	if (fd == 0) {
		ICL_CONN_LOCK(ic);
		if (ic->ic_socket == NULL) {
			ICL_CONN_UNLOCK(ic);
			ICL_WARN("proxy handoff without connect"); 
			return (EINVAL);
		}
		ICL_CONN_UNLOCK(ic);
		return (0);
	}
#endif

	/*
	 * Steal the socket from userland.
	 */
	error = fget(curthread, fd,
	    cap_rights_init_one(&rights, CAP_SOCK_CLIENT), &fp);
	if (error != 0)
		return (error);
	if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, curthread);
		return (EINVAL);
	}
	so = fp->f_data;
	if (so->so_type != SOCK_STREAM) {
		fdrop(fp, curthread);
		return (EINVAL);
	}

	ICL_CONN_LOCK(ic);

	if (ic->ic_socket != NULL) {
		ICL_CONN_UNLOCK(ic);
		fdrop(fp, curthread);
		return (EBUSY);
	}

	ic->ic_socket = fp->f_data;
	fp->f_ops = &badfileops;
	fp->f_data = NULL;
	fdrop(fp, curthread);
	ICL_CONN_UNLOCK(ic);

	error = icl_conn_start(ic);

	return (error);
}

void
icl_soft_conn_close(struct icl_conn *ic)
{
	struct icl_soft_conn *isc = (struct icl_soft_conn *)ic;
	struct icl_pdu *pdu;
	struct socket *so;

	/*
	 * Wake up the threads, so they can properly terminate.
	 * Receive thread sleeps on so->so_rcv lock, send on ic->ic_lock.
	 */
	ICL_CONN_LOCK(ic);
	if (!ic->ic_disconnecting) {
		so = ic->ic_socket;
		if (so)
			SOCKBUF_LOCK(&so->so_rcv);
		ic->ic_disconnecting = true;
		if (so)
			SOCKBUF_UNLOCK(&so->so_rcv);
	}
	while (isc->receive_running || isc->send_running) {
		cv_signal(&isc->receive_cv);
		cv_signal(&isc->send_cv);
		cv_wait(&isc->send_cv, ic->ic_lock);
	}

	/* Some other thread could close the connection same time. */
	so = ic->ic_socket;
	if (so == NULL) {
		ICL_CONN_UNLOCK(ic);
		return;
	}
	ic->ic_socket = NULL;

	/*
	 * Deregister socket upcalls.
	 */
	ICL_CONN_UNLOCK(ic);
	SOCKBUF_LOCK(&so->so_snd);
	if (so->so_snd.sb_upcall != NULL)
		soupcall_clear(so, SO_SND);
	SOCKBUF_UNLOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);
	if (so->so_rcv.sb_upcall != NULL)
		soupcall_clear(so, SO_RCV);
	SOCKBUF_UNLOCK(&so->so_rcv);
	soclose(so);
	ICL_CONN_LOCK(ic);

	if (isc->receive_pdu != NULL) {
		//ICL_DEBUG("freeing partially received PDU");
		icl_soft_conn_pdu_free(ic, isc->receive_pdu);
		isc->receive_pdu = NULL;
	}

	/*
	 * Remove any outstanding PDUs from the send queue.
	 */
	while (!STAILQ_EMPTY(&isc->to_send)) {
		pdu = STAILQ_FIRST(&isc->to_send);
		STAILQ_REMOVE_HEAD(&isc->to_send, ip_next);
		icl_soft_pdu_done(pdu, ENOTCONN);
	}

	KASSERT(STAILQ_EMPTY(&isc->to_send),
	    ("destroying session with non-empty send queue"));
	ICL_CONN_UNLOCK(ic);
}

int
icl_soft_conn_task_setup(struct icl_conn *ic, struct icl_pdu *ip,
    struct ccb_scsiio *csio, uint32_t *task_tagp, void **prvp)
{

	return (0);
}

void
icl_soft_conn_task_done(struct icl_conn *ic, void *prv)
{
}

int
icl_soft_conn_transfer_setup(struct icl_conn *ic, struct icl_pdu *ip,
    union ctl_io *io, uint32_t *transfer_tag, void **prvp)
{

	return (0);
}

void
icl_soft_conn_transfer_done(struct icl_conn *ic, void *prv)
{
}

static int
icl_soft_limits(struct icl_drv_limits *idl, int socket)
{

	idl->idl_max_recv_data_segment_length = max_data_segment_length;
	idl->idl_max_send_data_segment_length = max_data_segment_length;
	idl->idl_max_burst_length = max_burst_length;
	idl->idl_first_burst_length = first_burst_length;

	return (0);
}

#ifdef ICL_KERNEL_PROXY
int
icl_soft_conn_connect(struct icl_conn *ic, int domain, int socktype,
    int protocol, struct sockaddr *from_sa, struct sockaddr *to_sa)
{

	return (icl_soft_proxy_connect(ic, domain, socktype, protocol,
	    from_sa, to_sa));
}

int
icl_soft_handoff_sock(struct icl_conn *ic, struct socket *so)
{
	int error;

	ICL_CONN_LOCK_ASSERT_NOT(ic);

	if (so->so_type != SOCK_STREAM)
		return (EINVAL);

	ICL_CONN_LOCK(ic);
	if (ic->ic_socket != NULL) {
		ICL_CONN_UNLOCK(ic);
		return (EBUSY);
	}
	ic->ic_socket = so;
	ICL_CONN_UNLOCK(ic);

	error = icl_conn_start(ic);

	return (error);
}
#endif /* ICL_KERNEL_PROXY */

static int
icl_soft_load(void)
{
	int error;

	icl_soft_pdu_zone = uma_zcreate("icl_soft_pdu",
	    sizeof(struct icl_soft_pdu), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	refcount_init(&icl_ncons, 0);

	/*
	 * The reason we call this "none" is that to the user,
	 * it's known as "offload driver"; "offload driver: soft"
	 * doesn't make much sense.
	 */
	error = icl_register("none", false, 0,
	    icl_soft_limits, icl_soft_new_conn);
	KASSERT(error == 0, ("failed to register"));

#if defined(ICL_KERNEL_PROXY) && 0
	/*
	 * Debugging aid for kernel proxy functionality.
	 */
	error = icl_register("proxytest", true, 0,
	    icl_soft_limits, icl_soft_new_conn);
	KASSERT(error == 0, ("failed to register"));
#endif

	return (error);
}

static int
icl_soft_unload(void)
{

	if (icl_ncons != 0)
		return (EBUSY);

	icl_unregister("none", false);
#if defined(ICL_KERNEL_PROXY) && 0
	icl_unregister("proxytest", true);
#endif

	uma_zdestroy(icl_soft_pdu_zone);

	return (0);
}

static int
icl_soft_modevent(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		return (icl_soft_load());
	case MOD_UNLOAD:
		return (icl_soft_unload());
	default:
		return (EINVAL);
	}
}

moduledata_t icl_soft_data = {
	"icl_soft",
	icl_soft_modevent,
	0
};

DECLARE_MODULE(icl_soft, icl_soft_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_DEPEND(icl_soft, icl, 1, 1, 1);
MODULE_VERSION(icl_soft, 1);
