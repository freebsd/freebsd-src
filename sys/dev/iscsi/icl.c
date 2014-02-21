/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
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
 * $FreeBSD$
 */

/*
 * iSCSI Common Layer.  It's used by both the initiator and target to send
 * and receive iSCSI PDUs.
 */

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/uio.h>
#include <vm/uma.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "icl.h"
#include "iscsi_proto.h"

SYSCTL_NODE(_kern, OID_AUTO, icl, CTLFLAG_RD, 0, "iSCSI Common Layer");
static int debug = 1;
TUNABLE_INT("kern.icl.debug", &debug);
SYSCTL_INT(_kern_icl, OID_AUTO, debug, CTLFLAG_RW,
    &debug, 1, "Enable debug messages");
static int partial_receive_len = 1 * 1024; /* XXX: More? */
TUNABLE_INT("kern.icl.partial_receive_len", &partial_receive_len);
SYSCTL_INT(_kern_icl, OID_AUTO, partial_receive_len, CTLFLAG_RW,
    &partial_receive_len, 1 * 1024, "Minimum read size for partially received "
    "data segment");

static uma_zone_t icl_conn_zone;
static uma_zone_t icl_pdu_zone;

static volatile u_int	icl_ncons;

#define	ICL_DEBUG(X, ...)					\
	if (debug > 1) {					\
		printf("%s: " X "\n", __func__, ## __VA_ARGS__);\
	} while (0)

#define	ICL_WARN(X, ...)					\
	if (debug > 0) {					\
		printf("WARNING: %s: " X "\n",			\
		    __func__, ## __VA_ARGS__);			\
	} while (0)

#define ICL_CONN_LOCK(X)		mtx_lock(&X->ic_lock)
#define ICL_CONN_UNLOCK(X)		mtx_unlock(&X->ic_lock)
#define ICL_CONN_LOCK_ASSERT(X)		mtx_assert(&X->ic_lock, MA_OWNED)

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

static struct mbuf *
icl_conn_receive(struct icl_conn *ic, size_t len)
{
	struct uio uio;
	struct socket *so;
	struct mbuf *m;
	int error, flags;

	so = ic->ic_socket;

	memset(&uio, 0, sizeof(uio));
	uio.uio_resid = len;

	flags = MSG_DONTWAIT;
	error = soreceive(so, NULL, &uio, &m, NULL, &flags);
	if (error != 0) {
		ICL_DEBUG("soreceive error %d", error);
		return (NULL);
	}
	if (uio.uio_resid != 0) {
		m_freem(m);
		ICL_DEBUG("short read");
		return (NULL);
	}

	return (m);
}

static struct icl_pdu *
icl_pdu_new(struct icl_conn *ic, int flags)
{
	struct icl_pdu *ip;

	refcount_acquire(&ic->ic_outstanding_pdus);
	ip = uma_zalloc(icl_pdu_zone, flags | M_ZERO);
	if (ip == NULL) {
		ICL_WARN("failed to allocate %zd bytes", sizeof(*ip));
		refcount_release(&ic->ic_outstanding_pdus);
		return (NULL);
	}

	ip->ip_conn = ic;

	return (ip);
}

void
icl_pdu_free(struct icl_pdu *ip)
{
	struct icl_conn *ic;

	ic = ip->ip_conn;

	m_freem(ip->ip_bhs_mbuf);
	m_freem(ip->ip_ahs_mbuf);
	m_freem(ip->ip_data_mbuf);
	uma_zfree(icl_pdu_zone, ip);
	refcount_release(&ic->ic_outstanding_pdus);
}

/*
 * Allocate icl_pdu with empty BHS to fill up by the caller.
 */
struct icl_pdu *
icl_pdu_new_bhs(struct icl_conn *ic, int flags)
{
	struct icl_pdu *ip;

	ip = icl_pdu_new(ic, flags);
	if (ip == NULL)
		return (NULL);

	ip->ip_bhs_mbuf = m_getm2(NULL, sizeof(struct iscsi_bhs),
	    flags, MT_DATA, M_PKTHDR);
	if (ip->ip_bhs_mbuf == NULL) {
		ICL_WARN("failed to allocate %zd bytes", sizeof(*ip));
		icl_pdu_free(ip);
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

size_t
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

static int
icl_pdu_receive_bhs(struct icl_pdu *request, size_t *availablep)
{
	struct mbuf *m;

	m = icl_conn_receive(request->ip_conn, sizeof(struct iscsi_bhs));
	if (m == NULL) {
		ICL_DEBUG("failed to receive BHS");
		return (-1);
	}

	request->ip_bhs_mbuf = m_pullup(m, sizeof(struct iscsi_bhs));
	if (request->ip_bhs_mbuf == NULL) {
		ICL_WARN("m_pullup failed");
		return (-1);
	}
	request->ip_bhs = mtod(request->ip_bhs_mbuf, struct iscsi_bhs *);

	/*
	 * XXX: For architectures with strict alignment requirements
	 * 	we may need to allocate ip_bhs and copy the data into it.
	 * 	For some reason, though, not doing this doesn't seem
	 * 	to cause problems; tested on sparc64.
	 */

	*availablep -= sizeof(struct iscsi_bhs);
	return (0);
}

static int
icl_pdu_receive_ahs(struct icl_pdu *request, size_t *availablep)
{

	request->ip_ahs_len = icl_pdu_ahs_length(request);
	if (request->ip_ahs_len == 0)
		return (0);

	request->ip_ahs_mbuf = icl_conn_receive(request->ip_conn,
	    request->ip_ahs_len);
	if (request->ip_ahs_mbuf == NULL) {
		ICL_DEBUG("failed to receive AHS");
		return (-1);
	}

	*availablep -= request->ip_ahs_len;
	return (0);
}

static uint32_t
icl_mbuf_to_crc32c(const struct mbuf *m0)
{
	uint32_t digest = 0xffffffff;
	const struct mbuf *m;

	for (m = m0; m != NULL; m = m->m_next)
		digest = calculate_crc32c(digest,
		    mtod(m, const void *), m->m_len);

	digest = digest ^ 0xffffffff;

	return (digest);
}

static int
icl_pdu_check_header_digest(struct icl_pdu *request, size_t *availablep)
{
	struct mbuf *m;
	uint32_t received_digest, valid_digest;

	if (request->ip_conn->ic_header_crc32c == false)
		return (0);

	m = icl_conn_receive(request->ip_conn, ISCSI_HEADER_DIGEST_SIZE);
	if (m == NULL) {
		ICL_DEBUG("failed to receive header digest");
		return (-1);
	}

	CTASSERT(sizeof(received_digest) == ISCSI_HEADER_DIGEST_SIZE);
	m_copydata(m, 0, ISCSI_HEADER_DIGEST_SIZE, (void *)&received_digest);
	m_freem(m);

	*availablep -= ISCSI_HEADER_DIGEST_SIZE;

	/*
	 * XXX: Handle AHS.
	 */
	valid_digest = icl_mbuf_to_crc32c(request->ip_bhs_mbuf);
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
icl_pdu_receive_data_segment(struct icl_pdu *request,
    size_t *availablep, bool *more_neededp)
{
	struct icl_conn *ic;
	size_t len, padding = 0;
	struct mbuf *m;

	ic = request->ip_conn;

	*more_neededp = false;
	ic->ic_receive_len = 0;

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

	if (len + padding > *availablep) {
		/*
		 * Not enough data in the socket buffer.  Receive as much
		 * as we can.  Don't receive padding, since, obviously, it's
		 * not the end of data segment yet.
		 */
#if 0
		ICL_DEBUG("limited from %zd to %zd",
		    len + padding, *availablep - padding));
#endif
		len = *availablep - padding;
		*more_neededp = true;
		padding = 0;
	}

	/*
	 * Must not try to receive padding without at least one byte
	 * of actual data segment.
	 */
	if (len > 0) {
		m = icl_conn_receive(request->ip_conn, len + padding);
		if (m == NULL) {
			ICL_DEBUG("failed to receive data segment");
			return (-1);
		}

		if (request->ip_data_mbuf == NULL)
			request->ip_data_mbuf = m;
		else
			m_cat(request->ip_data_mbuf, m);

		request->ip_data_len += len;
		*availablep -= len + padding;
	} else
		ICL_DEBUG("len 0");

	if (*more_neededp)
		ic->ic_receive_len =
		    icl_pdu_data_segment_receive_len(request);

	return (0);
}

static int
icl_pdu_check_data_digest(struct icl_pdu *request, size_t *availablep)
{
	struct mbuf *m;
	uint32_t received_digest, valid_digest;

	if (request->ip_conn->ic_data_crc32c == false)
		return (0);

	if (request->ip_data_len == 0)
		return (0);

	m = icl_conn_receive(request->ip_conn, ISCSI_DATA_DIGEST_SIZE);
	if (m == NULL) {
		ICL_DEBUG("failed to receive data digest");
		return (-1);
	}

	CTASSERT(sizeof(received_digest) == ISCSI_DATA_DIGEST_SIZE);
	m_copydata(m, 0, ISCSI_DATA_DIGEST_SIZE, (void *)&received_digest);
	m_freem(m);

	*availablep -= ISCSI_DATA_DIGEST_SIZE;

	/*
	 * Note that ip_data_mbuf also contains padding; since digest
	 * calculation is supposed to include that, we iterate over
	 * the entire ip_data_mbuf chain, not just ip_data_len bytes of it.
	 */
	valid_digest = icl_mbuf_to_crc32c(request->ip_data_mbuf);
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
icl_conn_receive_pdu(struct icl_conn *ic, size_t *availablep)
{
	struct icl_pdu *request;
	struct socket *so;
	size_t len;
	int error;
	bool more_needed;

	so = ic->ic_socket;

	if (ic->ic_receive_state == ICL_CONN_STATE_BHS) {
		KASSERT(ic->ic_receive_pdu == NULL,
		    ("ic->ic_receive_pdu != NULL"));
		request = icl_pdu_new(ic, M_NOWAIT);
		if (request == NULL) {
			ICL_DEBUG("failed to allocate PDU; "
			    "dropping connection");
			icl_conn_fail(ic);
			return (NULL);
		}
		ic->ic_receive_pdu = request;
	} else {
		KASSERT(ic->ic_receive_pdu != NULL,
		    ("ic->ic_receive_pdu == NULL"));
		request = ic->ic_receive_pdu;
	}

	if (*availablep < ic->ic_receive_len) {
#if 0
		ICL_DEBUG("not enough data; need %zd, "
		    "have %zd", ic->ic_receive_len, *availablep);
#endif
		return (NULL);
	}

	switch (ic->ic_receive_state) {
	case ICL_CONN_STATE_BHS:
		//ICL_DEBUG("receiving BHS");
		error = icl_pdu_receive_bhs(request, availablep);
		if (error != 0) {
			ICL_DEBUG("failed to receive BHS; "
			    "dropping connection");
			break;
		}

		/*
		 * We don't enforce any limit for AHS length;
		 * its length is stored in 8 bit field.
		 */

		len = icl_pdu_data_segment_length(request);
		if (len > ic->ic_max_data_segment_length) {
			ICL_WARN("received data segment "
			    "length %zd is larger than negotiated "
			    "MaxDataSegmentLength %zd; "
			    "dropping connection",
			    len, ic->ic_max_data_segment_length);
			error = EINVAL;
			break;
		}

		ic->ic_receive_state = ICL_CONN_STATE_AHS;
		ic->ic_receive_len = icl_pdu_ahs_length(request);
		break;

	case ICL_CONN_STATE_AHS:
		//ICL_DEBUG("receiving AHS");
		error = icl_pdu_receive_ahs(request, availablep);
		if (error != 0) {
			ICL_DEBUG("failed to receive AHS; "
			    "dropping connection");
			break;
		}
		ic->ic_receive_state = ICL_CONN_STATE_HEADER_DIGEST;
		if (ic->ic_header_crc32c == false)
			ic->ic_receive_len = 0;
		else
			ic->ic_receive_len = ISCSI_HEADER_DIGEST_SIZE;
		break;

	case ICL_CONN_STATE_HEADER_DIGEST:
		//ICL_DEBUG("receiving header digest");
		error = icl_pdu_check_header_digest(request, availablep);
		if (error != 0) {
			ICL_DEBUG("header digest failed; "
			    "dropping connection");
			break;
		}

		ic->ic_receive_state = ICL_CONN_STATE_DATA;
		ic->ic_receive_len =
		    icl_pdu_data_segment_receive_len(request);
		break;

	case ICL_CONN_STATE_DATA:
		//ICL_DEBUG("receiving data segment");
		error = icl_pdu_receive_data_segment(request, availablep,
		    &more_needed);
		if (error != 0) {
			ICL_DEBUG("failed to receive data segment;"
			    "dropping connection");
			break;
		}

		if (more_needed)
			break;

		ic->ic_receive_state = ICL_CONN_STATE_DATA_DIGEST;
		if (request->ip_data_len == 0 || ic->ic_data_crc32c == false)
			ic->ic_receive_len = 0;
		else
			ic->ic_receive_len = ISCSI_DATA_DIGEST_SIZE;
		break;

	case ICL_CONN_STATE_DATA_DIGEST:
		//ICL_DEBUG("receiving data digest");
		error = icl_pdu_check_data_digest(request, availablep);
		if (error != 0) {
			ICL_DEBUG("data digest failed; "
			    "dropping connection");
			break;
		}

		/*
		 * We've received complete PDU; reset the receive state machine
		 * and return the PDU.
		 */
		ic->ic_receive_state = ICL_CONN_STATE_BHS;
		ic->ic_receive_len = sizeof(struct iscsi_bhs);
		ic->ic_receive_pdu = NULL;
		return (request);

	default:
		panic("invalid ic_receive_state %d\n", ic->ic_receive_state);
	}

	if (error != 0) {
		icl_pdu_free(request);
		icl_conn_fail(ic);
	}

	return (NULL);
}

static void
icl_conn_receive_pdus(struct icl_conn *ic, size_t available)
{
	struct icl_pdu *response;
	struct socket *so;

	so = ic->ic_socket;

	/*
	 * This can never happen; we're careful to only mess with ic->ic_socket
	 * pointer when the send/receive threads are not running.
	 */
	KASSERT(so != NULL, ("NULL socket"));

	for (;;) {
		if (ic->ic_disconnecting)
			return;

		if (so->so_error != 0) {
			ICL_DEBUG("connection error %d; "
			    "dropping connection", so->so_error);
			icl_conn_fail(ic);
			return;
		}

		/*
		 * Loop until we have a complete PDU or there is not enough
		 * data in the socket buffer.
		 */
		if (available < ic->ic_receive_len) {
#if 0
			ICL_DEBUG("not enough data; have %zd, "
			    "need %zd", available,
			    ic->ic_receive_len);
#endif
			return;
		}

		response = icl_conn_receive_pdu(ic, &available);
		if (response == NULL)
			continue;

		if (response->ip_ahs_len > 0) {
			ICL_WARN("received PDU with unsupported "
			    "AHS; opcode 0x%x; dropping connection",
			    response->ip_bhs->bhs_opcode);
			icl_pdu_free(response);
			icl_conn_fail(ic);
			return;
		}

		(ic->ic_receive)(response);
	}
}

static void
icl_receive_thread(void *arg)
{
	struct icl_conn *ic;
	size_t available;
	struct socket *so;

	ic = arg;
	so = ic->ic_socket;

	ICL_CONN_LOCK(ic);
	ic->ic_receive_running = true;
	ICL_CONN_UNLOCK(ic);

	for (;;) {
		if (ic->ic_disconnecting) {
			//ICL_DEBUG("terminating");
			break;
		}

		SOCKBUF_LOCK(&so->so_rcv);
		available = so->so_rcv.sb_cc;
		if (available < ic->ic_receive_len) {
			so->so_rcv.sb_lowat = ic->ic_receive_len;
			cv_wait(&ic->ic_receive_cv, &so->so_rcv.sb_mtx);
		}
		SOCKBUF_UNLOCK(&so->so_rcv);

		icl_conn_receive_pdus(ic, available);
	}

	ICL_CONN_LOCK(ic);
	ic->ic_receive_running = false;
	ICL_CONN_UNLOCK(ic);
	kthread_exit();
}

static int
icl_soupcall_receive(struct socket *so, void *arg, int waitflag)
{
	struct icl_conn *ic;

	ic = arg;
	cv_signal(&ic->ic_receive_cv);
	return (SU_OK);
}

static int
icl_pdu_send(struct icl_pdu *request)
{
	size_t padding, pdu_len;
	uint32_t digest, zero = 0;
	int error, ok;
	struct socket *so;
	struct icl_conn *ic;

	ic = request->ip_conn;
	so = request->ip_conn->ic_socket;

	ICL_CONN_LOCK_ASSERT(ic);

	icl_pdu_set_data_segment_length(request, request->ip_data_len);

	pdu_len = icl_pdu_size(request);

	if (ic->ic_header_crc32c) {
		digest = icl_mbuf_to_crc32c(request->ip_bhs_mbuf);
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
			digest = icl_mbuf_to_crc32c(request->ip_data_mbuf);

			ok = m_append(request->ip_data_mbuf, sizeof(digest),
			    (void *)&digest);
			if (ok != 1) {
				ICL_WARN("failed to append header digest");
				return (1);
			}
		}

		m_cat(request->ip_bhs_mbuf, request->ip_data_mbuf);
		request->ip_data_mbuf = NULL;
	}

	request->ip_bhs_mbuf->m_pkthdr.len = pdu_len;

	error = sosend(so, NULL, NULL, request->ip_bhs_mbuf,
	    NULL, MSG_DONTWAIT, curthread);
	request->ip_bhs_mbuf = NULL; /* Sosend consumes the mbuf. */
	if (error != 0) {
		ICL_DEBUG("sosend error %d", error);
		return (error);
	}

	return (0);
}

static void
icl_conn_send_pdus(struct icl_conn *ic)
{
	struct icl_pdu *request;
	struct socket *so;
	size_t available, size;
	int error;

	ICL_CONN_LOCK_ASSERT(ic);

	so = ic->ic_socket;

	SOCKBUF_LOCK(&so->so_snd);
	available = sbspace(&so->so_snd);
	SOCKBUF_UNLOCK(&so->so_snd);

	while (!TAILQ_EMPTY(&ic->ic_to_send)) {
		if (ic->ic_disconnecting)
			return;

		request = TAILQ_FIRST(&ic->ic_to_send);
		size = icl_pdu_size(request);
		if (available < size) {
			/*
			 * Set the low watermark on the socket,
			 * to avoid waking up until there is enough
			 * space.
			 */
			SOCKBUF_LOCK(&so->so_snd);
			so->so_snd.sb_lowat = size;
			SOCKBUF_UNLOCK(&so->so_snd);
#if 1
			ICL_DEBUG("no space to send; "
			    "have %zd, need %zd",
			    available, size);
#endif
			return;
		}
		available -= size;
		TAILQ_REMOVE(&ic->ic_to_send, request, ip_next);
		error = icl_pdu_send(request);
		if (error != 0) {
			ICL_DEBUG("failed to send PDU; "
			    "dropping connection");
			icl_conn_fail(ic);
			return;
		} 
		icl_pdu_free(request);
	}
}

static void
icl_send_thread(void *arg)
{
	struct icl_conn *ic;

	ic = arg;

	ICL_CONN_LOCK(ic);
	ic->ic_send_running = true;

	for (;;) {
		if (ic->ic_disconnecting) {
			//ICL_DEBUG("terminating");
			break;
		}
		icl_conn_send_pdus(ic);
		cv_wait(&ic->ic_send_cv, &ic->ic_lock);
	}

	ic->ic_send_running = false;
	ICL_CONN_UNLOCK(ic);
	kthread_exit();
}

static int
icl_soupcall_send(struct socket *so, void *arg, int waitflag)
{
	struct icl_conn *ic;

	ic = arg;
	cv_signal(&ic->ic_send_cv);
	return (SU_OK);
}

int
icl_pdu_append_data(struct icl_pdu *request, const void *addr, size_t len, int flags)
{
	struct mbuf *mb, *newmb;
	size_t copylen, off = 0;

	KASSERT(len > 0, ("len == 0"));

	newmb = m_getm2(NULL, len, flags, MT_DATA, M_PKTHDR);
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
icl_pdu_get_data(struct icl_pdu *ip, size_t off, void *addr, size_t len)
{

	m_copydata(ip->ip_data_mbuf, off, len, addr);
}

void
icl_pdu_queue(struct icl_pdu *ip)
{
	struct icl_conn *ic;

	ic = ip->ip_conn;

	ICL_CONN_LOCK(ic);
	if (ic->ic_disconnecting || ic->ic_socket == NULL) {
		ICL_DEBUG("icl_pdu_queue on closed connection");
		ICL_CONN_UNLOCK(ic);
		icl_pdu_free(ip);
		return;
	}
	TAILQ_INSERT_TAIL(&ic->ic_to_send, ip, ip_next);
	ICL_CONN_UNLOCK(ic);
	cv_signal(&ic->ic_send_cv);
}

struct icl_conn *
icl_conn_new(void)
{
	struct icl_conn *ic;

	refcount_acquire(&icl_ncons);

	ic = uma_zalloc(icl_conn_zone, M_WAITOK | M_ZERO);

	TAILQ_INIT(&ic->ic_to_send);
	mtx_init(&ic->ic_lock, "icl_lock", NULL, MTX_DEF);
	cv_init(&ic->ic_send_cv, "icl_tx");
	cv_init(&ic->ic_receive_cv, "icl_rx");
	refcount_init(&ic->ic_outstanding_pdus, 0);
	ic->ic_max_data_segment_length = ICL_MAX_DATA_SEGMENT_LENGTH;

	return (ic);
}

void
icl_conn_free(struct icl_conn *ic)
{

	mtx_destroy(&ic->ic_lock);
	cv_destroy(&ic->ic_send_cv);
	cv_destroy(&ic->ic_receive_cv);
	uma_zfree(icl_conn_zone, ic);
	refcount_release(&icl_ncons);
}

static int
icl_conn_start(struct icl_conn *ic)
{
	size_t bufsize;
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

	ic->ic_receive_state = ICL_CONN_STATE_BHS;
	ic->ic_receive_len = sizeof(struct iscsi_bhs);
	ic->ic_disconnecting = false;

	ICL_CONN_UNLOCK(ic);

	/*
	 * Use max available sockbuf size for sending.  Do it manually
	 * instead of sbreserve(9) to work around resource limits.
	 *
	 * XXX: This kind of sucks.  On one hand, we don't currently support
	 *	sending a part of data segment; we always do it in one piece,
	 *	so we have to make sure it can fit in the socket buffer.
	 *	Once I've implemented partial send, we'll get rid of this
	 *	and use autoscaling.
	 */
        bufsize = (sizeof(struct iscsi_bhs) +
            ic->ic_max_data_segment_length) * 8;
	error = soreserve(ic->ic_socket, bufsize, bufsize);
	if (error != 0) {
		ICL_WARN("soreserve failed with error %d", error);
		icl_conn_close(ic);
		return (error);
	}

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
		icl_conn_close(ic);
		return (error);
	}

	/*
	 * Start threads.
	 */
	error = kthread_add(icl_send_thread, ic, NULL, NULL, 0, 0, "icltx");
	if (error != 0) {
		ICL_WARN("kthread_add(9) failed with error %d", error);
		icl_conn_close(ic);
		return (error);
	}

	error = kthread_add(icl_receive_thread, ic, NULL, NULL, 0, 0, "iclrx");
	if (error != 0) {
		ICL_WARN("kthread_add(9) failed with error %d", error);
		icl_conn_close(ic);
		return (error);
	}

	/*
	 * Register socket upcall, to get notified about incoming PDUs
	 * and free space to send outgoing ones.
	 */
	SOCKBUF_LOCK(&ic->ic_socket->so_snd);
	soupcall_set(ic->ic_socket, SO_SND, icl_soupcall_send, ic);
	SOCKBUF_UNLOCK(&ic->ic_socket->so_snd);
	SOCKBUF_LOCK(&ic->ic_socket->so_rcv);
	soupcall_set(ic->ic_socket, SO_RCV, icl_soupcall_receive, ic);
	SOCKBUF_UNLOCK(&ic->ic_socket->so_rcv);

	return (0);
}

int
icl_conn_handoff(struct icl_conn *ic, int fd)
{
	struct file *fp;
	struct socket *so;
	cap_rights_t rights;
	int error;

	/*
	 * Steal the socket from userland.
	 */
	error = fget(curthread, fd,
	    cap_rights_init(&rights, CAP_SOCK_CLIENT), &fp);
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
icl_conn_shutdown(struct icl_conn *ic)
{

	ICL_CONN_LOCK(ic);
	if (ic->ic_socket == NULL) {
		ICL_CONN_UNLOCK(ic);
		return;
	}
	ICL_CONN_UNLOCK(ic);

	soshutdown(ic->ic_socket, SHUT_RDWR);
}

void
icl_conn_close(struct icl_conn *ic)
{
	struct icl_pdu *pdu;

	ICL_CONN_LOCK(ic);
	if (ic->ic_socket == NULL) {
		ICL_CONN_UNLOCK(ic);
		return;
	}

	ic->ic_disconnecting = true;

	/*
	 * Wake up the threads, so they can properly terminate.
	 */
	cv_signal(&ic->ic_receive_cv);
	cv_signal(&ic->ic_send_cv);
	while (ic->ic_receive_running || ic->ic_send_running) {
		//ICL_DEBUG("waiting for send/receive threads to terminate");
		ICL_CONN_UNLOCK(ic);
		cv_signal(&ic->ic_receive_cv);
		cv_signal(&ic->ic_send_cv);
		pause("icl_close", 1 * hz);
		ICL_CONN_LOCK(ic);
	}
	//ICL_DEBUG("send/receive threads terminated");

	soclose(ic->ic_socket);
	ic->ic_socket = NULL;

	if (ic->ic_receive_pdu != NULL) {
		//ICL_DEBUG("freeing partially received PDU");
		icl_pdu_free(ic->ic_receive_pdu);
		ic->ic_receive_pdu = NULL;
	}

	/*
	 * Remove any outstanding PDUs from the send queue.
	 */
	while (!TAILQ_EMPTY(&ic->ic_to_send)) {
		pdu = TAILQ_FIRST(&ic->ic_to_send);
		TAILQ_REMOVE(&ic->ic_to_send, pdu, ip_next);
		icl_pdu_free(pdu);
	}

	KASSERT(TAILQ_EMPTY(&ic->ic_to_send),
	    ("destroying session with non-empty send queue"));
	/*
	 * XXX
	 */
#if 0
	KASSERT(ic->ic_outstanding_pdus == 0,
	    ("destroying session with %d outstanding PDUs",
	     ic->ic_outstanding_pdus));
#endif
	ICL_CONN_UNLOCK(ic);
}

bool
icl_conn_connected(struct icl_conn *ic)
{

	ICL_CONN_LOCK(ic);
	if (ic->ic_socket == NULL) {
		ICL_CONN_UNLOCK(ic);
		return (false);
	}
	if (ic->ic_socket->so_error != 0) {
		ICL_CONN_UNLOCK(ic);
		return (false);
	}
	ICL_CONN_UNLOCK(ic);
	return (true);
}

#ifdef ICL_KERNEL_PROXY
int
icl_conn_handoff_sock(struct icl_conn *ic, struct socket *so)
{
	int error;

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
icl_unload(void)
{

	if (icl_ncons != 0)
		return (EBUSY);

	uma_zdestroy(icl_conn_zone);
	uma_zdestroy(icl_pdu_zone);

	return (0);
}

static void
icl_load(void)
{

	icl_conn_zone = uma_zcreate("icl_conn",
	    sizeof(struct icl_conn), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	icl_pdu_zone = uma_zcreate("icl_pdu",
	    sizeof(struct icl_pdu), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	refcount_init(&icl_ncons, 0);
}

static int
icl_modevent(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		icl_load();
		return (0);
	case MOD_UNLOAD:
		return (icl_unload());
	default:
		return (EINVAL);
	}
}

moduledata_t icl_data = {
	"icl",
	icl_modevent,
	0
};

DECLARE_MODULE(icl, icl_data, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(icl, 1);
