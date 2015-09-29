/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2015 Chelsio Communications, Inc.
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
 */

/*
 * cxgbei implementation of iSCSI Common Layer kobj(9) interface.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/conf.h>
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
#include <machine/bus.h>
#include <vm/uma.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <dev/iscsi/icl.h>
#include <dev/iscsi/iscsi_proto.h>
#include <icl_conn_if.h>

#include "common/common.h"
#include "cxgbei.h"

SYSCTL_NODE(_kern_icl, OID_AUTO, cxgbei, CTLFLAG_RD, 0, "Chelsio iSCSI offload");
static int coalesce = 1;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, coalesce, CTLFLAG_RWTUN,
	&coalesce, 0, "Try to coalesce PDUs before sending");
static int partial_receive_len = 128 * 1024;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, partial_receive_len, CTLFLAG_RWTUN,
    &partial_receive_len, 0, "Minimum read size for partially received "
    "data segment");
static int sendspace = 1048576;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, sendspace, CTLFLAG_RWTUN,
    &sendspace, 0, "Default send socket buffer size");
static int recvspace = 1048576;
SYSCTL_INT(_kern_icl_cxgbei, OID_AUTO, recvspace, CTLFLAG_RWTUN,
    &recvspace, 0, "Default receive socket buffer size");

static uma_zone_t icl_cxgbei_pdu_zone;
static uma_zone_t icl_transfer_zone;

static volatile u_int icl_cxgbei_ncons;

#define ICL_CONN_LOCK(X)		mtx_lock(X->ic_lock)
#define ICL_CONN_UNLOCK(X)		mtx_unlock(X->ic_lock)
#define ICL_CONN_LOCK_ASSERT(X)		mtx_assert(X->ic_lock, MA_OWNED)
#define ICL_CONN_LOCK_ASSERT_NOT(X)	mtx_assert(X->ic_lock, MA_NOTOWNED)

static icl_conn_new_pdu_t	icl_cxgbei_conn_new_pdu;
static icl_conn_pdu_free_t	icl_cxgbei_conn_pdu_free;
static icl_conn_pdu_data_segment_length_t
				    icl_cxgbei_conn_pdu_data_segment_length;
static icl_conn_pdu_append_data_t	icl_cxgbei_conn_pdu_append_data;
static icl_conn_pdu_get_data_t	icl_cxgbei_conn_pdu_get_data;
static icl_conn_pdu_queue_t	icl_cxgbei_conn_pdu_queue;
static icl_conn_handoff_t	icl_cxgbei_conn_handoff;
static icl_conn_free_t		icl_cxgbei_conn_free;
static icl_conn_close_t		icl_cxgbei_conn_close;
static icl_conn_task_setup_t	icl_cxgbei_conn_task_setup;
static icl_conn_task_done_t	icl_cxgbei_conn_task_done;
static icl_conn_transfer_setup_t	icl_cxgbei_conn_transfer_setup;
static icl_conn_transfer_done_t	icl_cxgbei_conn_transfer_done;

static kobj_method_t icl_cxgbei_methods[] = {
	KOBJMETHOD(icl_conn_new_pdu, icl_cxgbei_conn_new_pdu),
	KOBJMETHOD(icl_conn_pdu_free, icl_cxgbei_conn_pdu_free),
	KOBJMETHOD(icl_conn_pdu_data_segment_length,
	    icl_cxgbei_conn_pdu_data_segment_length),
	KOBJMETHOD(icl_conn_pdu_append_data, icl_cxgbei_conn_pdu_append_data),
	KOBJMETHOD(icl_conn_pdu_get_data, icl_cxgbei_conn_pdu_get_data),
	KOBJMETHOD(icl_conn_pdu_queue, icl_cxgbei_conn_pdu_queue),
	KOBJMETHOD(icl_conn_handoff, icl_cxgbei_conn_handoff),
	KOBJMETHOD(icl_conn_free, icl_cxgbei_conn_free),
	KOBJMETHOD(icl_conn_close, icl_cxgbei_conn_close),
	KOBJMETHOD(icl_conn_task_setup, icl_cxgbei_conn_task_setup),
	KOBJMETHOD(icl_conn_task_done, icl_cxgbei_conn_task_done),
	KOBJMETHOD(icl_conn_transfer_setup, icl_cxgbei_conn_transfer_setup),
	KOBJMETHOD(icl_conn_transfer_done, icl_cxgbei_conn_transfer_done),
	{ 0, 0 }
};

DEFINE_CLASS(icl_cxgbei, icl_cxgbei_methods, sizeof(struct icl_cxgbei_conn));

struct icl_pdu * icl_pdu_new_empty(struct icl_conn *ic, int flags);
void icl_pdu_free(struct icl_pdu *ip);

#define CXGBEI_PDU_SIGNATURE 0x12344321

struct icl_pdu *
icl_pdu_new_empty(struct icl_conn *ic, int flags)
{
	struct icl_cxgbei_pdu *icp;
	struct icl_pdu *ip;

#ifdef DIAGNOSTIC
	refcount_acquire(&ic->ic_outstanding_pdus);
#endif
	icp = uma_zalloc(icl_cxgbei_pdu_zone, flags | M_ZERO);
	if (icp == NULL) {
#ifdef DIAGNOSTIC
		refcount_release(&ic->ic_outstanding_pdus);
#endif
		return (NULL);
	}
	icp->icp_signature = CXGBEI_PDU_SIGNATURE;

	ip = &icp->ip;
	ip->ip_conn = ic;

	return (ip);
}

void
icl_pdu_free(struct icl_pdu *ip)
{
	struct icl_conn *ic;
	struct icl_cxgbei_pdu *icp;

	icp = (void *)ip;
	MPASS(icp->icp_signature == CXGBEI_PDU_SIGNATURE);
	ic = ip->ip_conn;

	m_freem(ip->ip_bhs_mbuf);
	m_freem(ip->ip_ahs_mbuf);
	m_freem(ip->ip_data_mbuf);
	uma_zfree(icl_cxgbei_pdu_zone, ip);
#ifdef DIAGNOSTIC
	refcount_release(&ic->ic_outstanding_pdus);
#endif
}

void
icl_cxgbei_conn_pdu_free(struct icl_conn *ic, struct icl_pdu *ip)
{

	icl_pdu_free(ip);
}

/*
 * Allocate icl_pdu with empty BHS to fill up by the caller.
 */
struct icl_pdu *
icl_cxgbei_conn_new_pdu(struct icl_conn *ic, int flags)
{
	struct icl_pdu *ip;

	ip = icl_pdu_new_empty(ic, flags);
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
icl_cxgbei_conn_pdu_data_segment_length(struct icl_conn *ic,
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

	return (len);
}

static uint32_t
icl_conn_build_tasktag(struct icl_conn *ic, uint32_t tag)
{
	return tag;
}

static int
icl_soupcall_receive(struct socket *so, void *arg, int waitflag)
{
	struct icl_conn *ic;

	if (!soreadable(so))
		return (SU_OK);

	ic = arg;
	cv_signal(&ic->ic_receive_cv);
	return (SU_OK);
}

static int
icl_pdu_finalize(struct icl_pdu *request)
{
	size_t padding, pdu_len;
	uint32_t zero = 0;
	int ok;
	struct icl_conn *ic;

	ic = request->ip_conn;

	icl_pdu_set_data_segment_length(request, request->ip_data_len);

	pdu_len = icl_pdu_size(request);

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

		m_cat(request->ip_bhs_mbuf, request->ip_data_mbuf);
		request->ip_data_mbuf = NULL;
	}

	request->ip_bhs_mbuf->m_pkthdr.len = pdu_len;

	return (0);
}

static int
icl_soupcall_send(struct socket *so, void *arg, int waitflag)
{
	struct icl_conn *ic;

	if (!sowriteable(so))
		return (SU_OK);

	ic = arg;

	ICL_CONN_LOCK(ic);
	ic->ic_check_send_space = true;
	ICL_CONN_UNLOCK(ic);

	cv_signal(&ic->ic_send_cv);

	return (SU_OK);
}

static int
icl_pdu_append_data(struct icl_pdu *request, const void *addr, size_t len,
    int flags)
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

int
icl_cxgbei_conn_pdu_append_data(struct icl_conn *ic, struct icl_pdu *request,
    const void *addr, size_t len, int flags)
{

	return (icl_pdu_append_data(request, addr, len, flags));
}

static void
icl_pdu_get_data(struct icl_pdu *ip, size_t off, void *addr, size_t len)
{
	/* data is DDP'ed, no need to copy */
	if (ip->ip_ofld_prv0) return;
	m_copydata(ip->ip_data_mbuf, off, len, addr);
}

void
icl_cxgbei_conn_pdu_get_data(struct icl_conn *ic, struct icl_pdu *ip,
    size_t off, void *addr, size_t len)
{

	return (icl_pdu_get_data(ip, off, addr, len));
}

static void
icl_pdu_queue(struct icl_pdu *ip)
{
	struct icl_conn *ic;

	ic = ip->ip_conn;

	ICL_CONN_LOCK_ASSERT(ic);

	if (ic->ic_disconnecting || ic->ic_socket == NULL) {
		ICL_DEBUG("icl_pdu_queue on closed connection");
		icl_pdu_free(ip);
		return;
	}
	icl_pdu_finalize(ip);
	cxgbei_conn_xmit_pdu(ic, ip);
}

void
icl_cxgbei_conn_pdu_queue(struct icl_conn *ic, struct icl_pdu *ip)
{

	icl_pdu_queue(ip);
}

#define CXGBEI_CONN_SIGNATURE 0x56788765

static struct icl_conn *
icl_cxgbei_new_conn(const char *name, struct mtx *lock)
{
	struct icl_cxgbei_conn *icc;
	struct icl_conn *ic;

	refcount_acquire(&icl_cxgbei_ncons);

	icc = (struct icl_cxgbei_conn *)kobj_create(&icl_cxgbei_class, M_CXGBE,
	    M_WAITOK | M_ZERO);
	icc->icc_signature = CXGBEI_CONN_SIGNATURE;

	ic = &icc->ic;
	STAILQ_INIT(&ic->ic_to_send);
	ic->ic_lock = lock;
	cv_init(&ic->ic_send_cv, "icl_cxgbei_tx");
	cv_init(&ic->ic_receive_cv, "icl_cxgbei_rx");
#ifdef DIAGNOSTIC
	refcount_init(&ic->ic_outstanding_pdus, 0);
#endif
	ic->ic_max_data_segment_length = ICL_MAX_DATA_SEGMENT_LENGTH;
	ic->ic_name = name;
	ic->ic_offload = "cxgbei";

	return (ic);
}

void
icl_cxgbei_conn_free(struct icl_conn *ic)
{
	struct icl_cxgbei_conn *icc = (void *)ic;

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);

	cv_destroy(&ic->ic_send_cv);
	cv_destroy(&ic->ic_receive_cv);
	kobj_delete((struct kobj *)icc, M_CXGBE);
	refcount_release(&icl_cxgbei_ncons);
}

/* XXXNP: what is this for?  There's no conn_start method. */
static int
icl_conn_start(struct icl_conn *ic)
{
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

	ic->ic_receive_state = ICL_CONN_STATE_BHS;
	ic->ic_receive_len = sizeof(struct iscsi_bhs);
	ic->ic_disconnecting = false;

	ICL_CONN_UNLOCK(ic);

	/*
	 * For sendspace, this is required because the current code cannot
	 * send a PDU in pieces; thus, the minimum buffer size is equal
	 * to the maximum PDU size.  "+4" is to account for possible padding.
	 *
	 * What we should actually do here is to use autoscaling, but set
	 * some minimal buffer size to "minspace".  I don't know a way to do
	 * that, though.
	 */
	minspace = sizeof(struct iscsi_bhs) + ic->ic_max_data_segment_length +
	    ISCSI_HEADER_DIGEST_SIZE + ISCSI_DATA_DIGEST_SIZE + 4;
	if (sendspace < minspace) {
		ICL_WARN("kern.icl.sendspace too low; must be at least %zd",
		    minspace);
		sendspace = minspace;
	}
	if (recvspace < minspace) {
		ICL_WARN("kern.icl.recvspace too low; must be at least %zd",
		    minspace);
		recvspace = minspace;
	}

	error = soreserve(ic->ic_socket, sendspace, recvspace);
	if (error != 0) {
		ICL_WARN("soreserve failed with error %d", error);
		icl_cxgbei_conn_close(ic);
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
		icl_cxgbei_conn_close(ic);
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
icl_cxgbei_conn_handoff(struct icl_conn *ic, int fd)
{
	struct file *fp;
	struct socket *so;
	cap_rights_t rights;
	int error;

	ICL_CONN_LOCK_ASSERT_NOT(ic);

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
	if (!error)
		cxgbei_conn_handoff(ic);

	return (error);
}

void
icl_cxgbei_conn_close(struct icl_conn *ic)
{
	struct icl_pdu *pdu;

	ICL_CONN_LOCK_ASSERT_NOT(ic);

	ICL_CONN_LOCK(ic);
	if (ic->ic_socket == NULL) {
		ICL_CONN_UNLOCK(ic);
		return;
	}

	/*
	 * Deregister socket upcalls.
	 */
	ICL_CONN_UNLOCK(ic);
	SOCKBUF_LOCK(&ic->ic_socket->so_snd);
	if (ic->ic_socket->so_snd.sb_upcall != NULL)
		soupcall_clear(ic->ic_socket, SO_SND);
	SOCKBUF_UNLOCK(&ic->ic_socket->so_snd);
	SOCKBUF_LOCK(&ic->ic_socket->so_rcv);
	if (ic->ic_socket->so_rcv.sb_upcall != NULL)
		soupcall_clear(ic->ic_socket, SO_RCV);
	SOCKBUF_UNLOCK(&ic->ic_socket->so_rcv);
	ICL_CONN_LOCK(ic);

	ic->ic_disconnecting = true;

	/*
	 * Wake up the threads, so they can properly terminate.
	 */
	while (ic->ic_receive_running || ic->ic_send_running) {
		//ICL_DEBUG("waiting for send/receive threads to terminate");
		cv_signal(&ic->ic_receive_cv);
		cv_signal(&ic->ic_send_cv);
		cv_wait(&ic->ic_send_cv, ic->ic_lock);
	}
	//ICL_DEBUG("send/receive threads terminated");

	ICL_CONN_UNLOCK(ic);
	cxgbei_conn_close(ic);
	soclose(ic->ic_socket);
	ICL_CONN_LOCK(ic);
	ic->ic_socket = NULL;

	if (ic->ic_receive_pdu != NULL) {
		//ICL_DEBUG("freeing partially received PDU");
		icl_pdu_free(ic->ic_receive_pdu);
		ic->ic_receive_pdu = NULL;
	}

	/*
	 * Remove any outstanding PDUs from the send queue.
	 */
	while (!STAILQ_EMPTY(&ic->ic_to_send)) {
		pdu = STAILQ_FIRST(&ic->ic_to_send);
		STAILQ_REMOVE_HEAD(&ic->ic_to_send, ip_next);
		icl_pdu_free(pdu);
	}

	KASSERT(STAILQ_EMPTY(&ic->ic_to_send),
	    ("destroying session with non-empty send queue"));
#ifdef DIAGNOSTIC
	KASSERT(ic->ic_outstanding_pdus == 0,
	    ("destroying session with %d outstanding PDUs",
	     ic->ic_outstanding_pdus));
#endif
	ICL_CONN_UNLOCK(ic);
}

int
icl_cxgbei_conn_task_setup(struct icl_conn *ic, struct ccb_scsiio *csio,
    uint32_t *task_tagp, void **prvp)
{
	void *prv;

	*task_tagp = icl_conn_build_tasktag(ic, *task_tagp);

	prv = uma_zalloc(icl_transfer_zone, M_NOWAIT | M_ZERO);
	if (prv == NULL)
		return (ENOMEM);

	*prvp = prv;

	cxgbei_conn_task_reserve_itt(ic, prvp, csio, task_tagp);

	return (0);
}

void
icl_cxgbei_conn_task_done(struct icl_conn *ic, void *prv)
{

	cxgbei_cleanup_task(ic, prv);
	uma_zfree(icl_transfer_zone, prv);
}

int
icl_cxgbei_conn_transfer_setup(struct icl_conn *ic, union ctl_io *io,
    uint32_t *transfer_tag, void **prvp)
{
	void *prv;

	*transfer_tag = icl_conn_build_tasktag(ic, *transfer_tag);

	prv = uma_zalloc(icl_transfer_zone, M_NOWAIT | M_ZERO);
	if (prv == NULL)
		return (ENOMEM);

	*prvp = prv;

	cxgbei_conn_transfer_reserve_ttt(ic, prvp, io, transfer_tag);

	return (0);
}

void
icl_cxgbei_conn_transfer_done(struct icl_conn *ic, void *prv)
{
	cxgbei_cleanup_task(ic, prv);
	uma_zfree(icl_transfer_zone, prv);
}

static int
icl_cxgbei_limits(size_t *limitp)
{

	*limitp = 8 * 1024;

	return (0);
}

#ifdef ICL_KERNEL_PROXY
int
icl_conn_handoff_sock(struct icl_conn *ic, struct socket *so)
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
icl_cxgbei_load(void)
{
	int error;

	icl_cxgbei_pdu_zone = uma_zcreate("icl_cxgbei_pdu",
	    sizeof(struct icl_cxgbei_pdu), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	icl_transfer_zone = uma_zcreate("icl_transfer",
	    16 * 1024, NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	refcount_init(&icl_cxgbei_ncons, 0);

	error = icl_register("cxgbei", 100, icl_cxgbei_limits,
	    icl_cxgbei_new_conn);
	KASSERT(error == 0, ("failed to register"));

	return (error);
}

static int
icl_cxgbei_unload(void)
{

	if (icl_cxgbei_ncons != 0)
		return (EBUSY);

	icl_unregister("cxgbei");

	uma_zdestroy(icl_cxgbei_pdu_zone);
	uma_zdestroy(icl_transfer_zone);

	return (0);
}

static int
icl_cxgbei_modevent(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		return (icl_cxgbei_load());
	case MOD_UNLOAD:
		return (icl_cxgbei_unload());
	default:
		return (EINVAL);
	}
}

moduledata_t icl_cxgbei_data = {
	"icl_cxgbei",
	icl_cxgbei_modevent,
	0
};

DECLARE_MODULE(icl_cxgbei, icl_cxgbei_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_DEPEND(icl_cxgbei, icl, 1, 1, 1);
MODULE_VERSION(icl_cxgbei, 1);
