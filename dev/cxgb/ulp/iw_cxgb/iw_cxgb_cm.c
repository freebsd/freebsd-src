/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/rwlock.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <contrib/rdma/ib_verbs.h>

#include <cxgb_include.h>
#include <ulp/tom/cxgb_tom.h>
#include <ulp/tom/cxgb_t3_ddp.h>
#include <ulp/tom/cxgb_defs.h>
#include <ulp/tom/cxgb_toepcb.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>

#ifdef KTR
static char *states[] = {
	"idle",
	"listen",
	"connecting",
	"mpa_wait_req",
	"mpa_req_sent",
	"mpa_req_rcvd",
	"mpa_rep_sent",
	"fpdu_mode",
	"aborting",
	"closing",
	"moribund",
	"dead",
	NULL,
};
#endif

SYSCTL_NODE(_hw, OID_AUTO, cxgb, CTLFLAG_RD, 0, "iw_cxgb driver parameters");

static int ep_timeout_secs = 10;
TUNABLE_INT("hw.iw_cxgb.ep_timeout_secs", &ep_timeout_secs);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, ep_timeout_secs, CTLFLAG_RDTUN, &ep_timeout_secs, 0,
    "CM Endpoint operation timeout in seconds (default=10)");

static int mpa_rev = 1;
TUNABLE_INT("hw.iw_cxgb.mpa_rev", &mpa_rev);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, mpa_rev, CTLFLAG_RDTUN, &mpa_rev, 0,
    "MPA Revision, 0 supports amso1100, 1 is spec compliant. (default=1)");

static int markers_enabled = 0;
TUNABLE_INT("hw.iw_cxgb.markers_enabled", &markers_enabled);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, markers_enabled, CTLFLAG_RDTUN, &markers_enabled, 0,
    "Enable MPA MARKERS (default(0)=disabled)");

static int crc_enabled = 1;
TUNABLE_INT("hw.iw_cxgb.crc_enabled", &crc_enabled);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, crc_enabled, CTLFLAG_RDTUN, &crc_enabled, 0,
    "Enable MPA CRC (default(1)=enabled)");

static int rcv_win = 256 * 1024;
TUNABLE_INT("hw.iw_cxgb.rcv_win", &rcv_win);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, rcv_win, CTLFLAG_RDTUN, &rcv_win, 0,
    "TCP receive window in bytes (default=256KB)");

static int snd_win = 32 * 1024;
TUNABLE_INT("hw.iw_cxgb.snd_win", &snd_win);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, snd_win, CTLFLAG_RDTUN, &snd_win, 0,
    "TCP send window in bytes (default=32KB)");

static unsigned int nocong = 0;
TUNABLE_INT("hw.iw_cxgb.nocong", &nocong);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, nocong, CTLFLAG_RDTUN, &nocong, 0,
    "Turn off congestion control (default=0)");

static unsigned int cong_flavor = 1;
TUNABLE_INT("hw.iw_cxgb.cong_flavor", &cong_flavor);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, cong_flavor, CTLFLAG_RDTUN, &cong_flavor, 0,
    "TCP Congestion control flavor (default=1)");

static void ep_timeout(void *arg);
static void connect_reply_upcall(struct iwch_ep *ep, int status);
static void iwch_so_upcall(struct socket *so, void *arg, int waitflag);

/*
 * Cruft to offload socket upcalls onto thread.
 */
static struct mtx req_lock;
static TAILQ_HEAD(iwch_ep_list, iwch_ep_common) req_list;
static struct task iw_cxgb_task;
static struct taskqueue *iw_cxgb_taskq;
static void process_req(void *ctx, int pending);

static void
start_ep_timer(struct iwch_ep *ep)
{
	CTR2(KTR_IW_CXGB, "%s ep %p", __FUNCTION__, ep);
	if (callout_pending(&ep->timer)) {
		CTR2(KTR_IW_CXGB, "%s stopped / restarted timer ep %p", __FUNCTION__, ep);
		callout_deactivate(&ep->timer);
		callout_drain(&ep->timer);
	} else {
		/*
		 * XXX this looks racy
		 */
		get_ep(&ep->com);
		callout_init(&ep->timer, TRUE);
	}
	callout_reset(&ep->timer, ep_timeout_secs * hz, ep_timeout, ep);
}

static void
stop_ep_timer(struct iwch_ep *ep)
{
	CTR2(KTR_IW_CXGB, "%s ep %p", __FUNCTION__, ep);
	callout_drain(&ep->timer);
	put_ep(&ep->com);
}

static int set_tcpinfo(struct iwch_ep *ep)
{
	struct tcp_info ti;
	struct sockopt sopt;
	int err;

	sopt.sopt_dir = SOPT_GET;
	sopt.sopt_level = IPPROTO_TCP;
	sopt.sopt_name = TCP_INFO;
	sopt.sopt_val = (caddr_t)&ti;
	sopt.sopt_valsize = sizeof ti;
	sopt.sopt_td = NULL;
	
	err = sogetopt(ep->com.so, &sopt);
	if (err) {
		printf("%s can't get tcpinfo\n", __FUNCTION__);
		return -err;
	}
	if (!(ti.tcpi_options & TCPI_OPT_TOE)) {
		printf("%s connection NOT OFFLOADED!\n", __FUNCTION__);
		return -EINVAL;
	}

	ep->snd_seq = ti.tcpi_snd_nxt;
	ep->rcv_seq = ti.tcpi_rcv_nxt;
	ep->emss = ti.__tcpi_snd_mss - sizeof(struct tcpiphdr);
	ep->hwtid = TOEPCB(ep->com.so)->tp_tid; /* XXX */
	if (ti.tcpi_options & TCPI_OPT_TIMESTAMPS)
		ep->emss -= 12;
	if (ep->emss < 128)
		ep->emss = 128;
	return 0;
}

static enum iwch_ep_state
state_read(struct iwch_ep_common *epc)
{
	enum iwch_ep_state state;

	mtx_lock(&epc->lock);
	state = epc->state;
	mtx_unlock(&epc->lock);
	return state;
}

static void
__state_set(struct iwch_ep_common *epc, enum iwch_ep_state new)
{
	epc->state = new;
}

static void
state_set(struct iwch_ep_common *epc, enum iwch_ep_state new)
{

	mtx_lock(&epc->lock);
	CTR3(KTR_IW_CXGB, "%s - %s -> %s", __FUNCTION__, states[epc->state], states[new]);
	__state_set(epc, new);
	mtx_unlock(&epc->lock);
	return;
}

static void *
alloc_ep(int size, int flags)
{
	struct iwch_ep_common *epc;

	epc = malloc(size, M_DEVBUF, flags);
	if (epc) {
		memset(epc, 0, size);
		refcount_init(&epc->refcount, 1);
		mtx_init(&epc->lock, "iwch_epc lock", NULL, MTX_DEF|MTX_DUPOK);
		cv_init(&epc->waitq, "iwch_epc cv");
	}
	CTR2(KTR_IW_CXGB, "%s alloc ep %p", __FUNCTION__, epc);
	return epc;
}

void __free_ep(struct iwch_ep_common *epc)
{
	CTR3(KTR_IW_CXGB, "%s ep %p state %s", __FUNCTION__, epc, states[state_read(epc)]);
	KASSERT(!epc->so, ("%s warning ep->so %p \n", __FUNCTION__, epc->so));
	KASSERT(!epc->entry.tqe_prev, ("%s epc %p still on req list!\n", __FUNCTION__, epc));
	free(epc, M_DEVBUF);
}

int
iwch_quiesce_tid(struct iwch_ep *ep)
{
#ifdef notyet
	struct cpl_set_tcb_field *req;
	struct mbuf *m = get_mbuf(NULL, sizeof(*req), M_NOWAIT);

	if (m == NULL)
		return (-ENOMEM);
	req = (struct cpl_set_tcb_field *) mbuf_put(m, sizeof(*req));
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = htonl(V_WR_TID(ep->hwtid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SET_TCB_FIELD, ep->hwtid));
	req->reply = 0;
	req->cpu_idx = 0;
	req->word = htons(W_TCB_RX_QUIESCE);
	req->mask = cpu_to_be64(1ULL << S_TCB_RX_QUIESCE);
	req->val = cpu_to_be64(1 << S_TCB_RX_QUIESCE);

	m_set_priority(m, CPL_PRIORITY_DATA); 
	cxgb_ofld_send(ep->com.tdev, m);
#endif
	return 0;
}

int
iwch_resume_tid(struct iwch_ep *ep)
{
#ifdef notyet
	struct cpl_set_tcb_field *req;
	struct mbuf *m = get_mbuf(NULL, sizeof(*req), M_NOWAIT);

	if (m == NULL)
		return (-ENOMEM);
	req = (struct cpl_set_tcb_field *) mbuf_put(m, sizeof(*req));
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = htonl(V_WR_TID(ep->hwtid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SET_TCB_FIELD, ep->hwtid));
	req->reply = 0;
	req->cpu_idx = 0;
	req->word = htons(W_TCB_RX_QUIESCE);
	req->mask = cpu_to_be64(1ULL << S_TCB_RX_QUIESCE);
	req->val = 0;

	m_set_priority(m, CPL_PRIORITY_DATA);
	cxgb_ofld_send(ep->com.tdev, m);
#endif
	return 0;
}

static struct rtentry *
find_route(__be32 local_ip, __be32 peer_ip, __be16 local_port,
    __be16 peer_port, u8 tos)
{
        struct route iproute;
        struct sockaddr_in *dst = (struct sockaddr_in *)&iproute.ro_dst;
 
        bzero(&iproute, sizeof iproute);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof *dst;
        dst->sin_addr.s_addr = peer_ip;
 
        rtalloc(&iproute);
	return iproute.ro_rt;
}

static void
close_socket(struct iwch_ep_common *epc)
{
	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, epc, epc->so, states[epc->state]);
	SOCK_LOCK(epc->so);
	epc->so->so_upcall = NULL;
	epc->so->so_upcallarg = NULL;
	epc->so->so_rcv.sb_flags &= ~SB_UPCALL;
	SOCK_UNLOCK(epc->so);
	soshutdown(epc->so, SHUT_WR|SHUT_RD);
	epc->so = NULL;
}

static void
shutdown_socket(struct iwch_ep_common *epc)
{
	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, epc, epc->so, states[epc->state]);
	soshutdown(epc->so, SHUT_WR);
}

static void
abort_socket(struct iwch_ep *ep)
{
	struct sockopt sopt;
	int err;
	struct linger l;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	l.l_onoff = 1;
	l.l_linger = 0;

	/* linger_time of 0 forces RST to be sent */
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = SO_LINGER;
	sopt.sopt_val = (caddr_t)&l;
	sopt.sopt_valsize = sizeof l;
	sopt.sopt_td = NULL;
	err = sosetopt(ep->com.so, &sopt);
	if (err) 
		printf("%s can't set linger to 0, no RST! err %d\n", __FUNCTION__, err);
}

static void
send_mpa_req(struct iwch_ep *ep)
{
	int mpalen;
	struct mpa_message *mpa;
	struct mbuf *m;
	int err;

	CTR3(KTR_IW_CXGB, "%s ep %p pd_len %d", __FUNCTION__, ep, ep->plen);

	mpalen = sizeof(*mpa) + ep->plen;
	m = m_gethdr(mpalen, M_NOWAIT);
	if (m == NULL) {
		connect_reply_upcall(ep, -ENOMEM);
		return;
	}
	mpa = mtod(m, struct mpa_message *);
	m->m_len = mpalen;
	m->m_pkthdr.len = mpalen;
	memset(mpa, 0, sizeof(*mpa));
	memcpy(mpa->key, MPA_KEY_REQ, sizeof(mpa->key));
	mpa->flags = (crc_enabled ? MPA_CRC : 0) |
		     (markers_enabled ? MPA_MARKERS : 0);
	mpa->private_data_size = htons(ep->plen);
	mpa->revision = mpa_rev;
	if (ep->plen)
		memcpy(mpa->private_data, ep->mpa_pkt + sizeof(*mpa), ep->plen);

	err = sosend(ep->com.so, NULL, NULL, m, NULL, MSG_DONTWAIT, ep->com.thread);
	if (err) {
		m_freem(m);
		connect_reply_upcall(ep, -ENOMEM);
		return;
	}
		
	start_ep_timer(ep);
	state_set(&ep->com, MPA_REQ_SENT);
	return;
}

static int
send_mpa_reject(struct iwch_ep *ep, const void *pdata, u8 plen)
{
	int mpalen;
	struct mpa_message *mpa;
	struct mbuf *m;
	int err;

	CTR3(KTR_IW_CXGB, "%s ep %p plen %d", __FUNCTION__, ep, plen);

	mpalen = sizeof(*mpa) + plen;

	m = m_gethdr(mpalen, M_NOWAIT);
	if (m == NULL) {
		printf("%s - cannot alloc mbuf!\n", __FUNCTION__);
		return (-ENOMEM);
	}
	mpa = mtod(m, struct mpa_message *);
	m->m_len = mpalen;
	m->m_pkthdr.len = mpalen;
	memset(mpa, 0, sizeof(*mpa));
	memcpy(mpa->key, MPA_KEY_REP, sizeof(mpa->key));
	mpa->flags = MPA_REJECT;
	mpa->revision = mpa_rev;
	mpa->private_data_size = htons(plen);
	if (plen)
		memcpy(mpa->private_data, pdata, plen);
	err = sosend(ep->com.so, NULL, NULL, m, NULL, MSG_DONTWAIT, ep->com.thread);
	PANIC_IF(err);
	return 0;
}

static int
send_mpa_reply(struct iwch_ep *ep, const void *pdata, u8 plen)
{
	int mpalen;
	struct mpa_message *mpa;
	struct mbuf *m;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p plen %d", __FUNCTION__, ep, ep->com.so, plen);

	mpalen = sizeof(*mpa) + plen;

	m = m_gethdr(mpalen, M_NOWAIT);
	if (m == NULL) {
		printf("%s - cannot alloc mbuf!\n", __FUNCTION__);
		return (-ENOMEM);
	}
	mpa = mtod(m, struct mpa_message *);
	m->m_len = mpalen;
	m->m_pkthdr.len = mpalen;
	memset(mpa, 0, sizeof(*mpa));
	memcpy(mpa->key, MPA_KEY_REP, sizeof(mpa->key));
	mpa->flags = (ep->mpa_attr.crc_enabled ? MPA_CRC : 0) |
		     (markers_enabled ? MPA_MARKERS : 0);
	mpa->revision = mpa_rev;
	mpa->private_data_size = htons(plen);
	if (plen)
		memcpy(mpa->private_data, pdata, plen);

	state_set(&ep->com, MPA_REP_SENT);
	return sosend(ep->com.so, NULL, NULL, m, NULL, MSG_DONTWAIT, 
		ep->com.thread);
}

static void
close_complete_upcall(struct iwch_ep *ep)
{
	struct iw_cm_event event;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_CLOSE;
	if (ep->com.cm_id) {
		CTR3(KTR_IW_CXGB, "close complete delivered ep %p cm_id %p tid %d",
		     ep, ep->com.cm_id, ep->hwtid);
		ep->com.cm_id->event_handler(ep->com.cm_id, &event);
		ep->com.cm_id->rem_ref(ep->com.cm_id);
		ep->com.cm_id = NULL;
		ep->com.qp = NULL;
	}
}

static void
abort_connection(struct iwch_ep *ep)
{
	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	state_set(&ep->com, ABORTING);
	abort_socket(ep);
	close_socket(&ep->com);
	close_complete_upcall(ep);
	state_set(&ep->com, DEAD);
	put_ep(&ep->com);
}

static void
peer_close_upcall(struct iwch_ep *ep)
{
	struct iw_cm_event event;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_DISCONNECT;
	if (ep->com.cm_id) {
		CTR3(KTR_IW_CXGB, "peer close delivered ep %p cm_id %p tid %d",
		     ep, ep->com.cm_id, ep->hwtid);
		ep->com.cm_id->event_handler(ep->com.cm_id, &event);
	}
}

static void
peer_abort_upcall(struct iwch_ep *ep)
{
	struct iw_cm_event event;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_CLOSE;
	event.status = ECONNRESET;
	if (ep->com.cm_id) {
		CTR3(KTR_IW_CXGB, "abort delivered ep %p cm_id %p tid %d", ep,
		     ep->com.cm_id, ep->hwtid);
		ep->com.cm_id->event_handler(ep->com.cm_id, &event);
		ep->com.cm_id->rem_ref(ep->com.cm_id);
		ep->com.cm_id = NULL;
		ep->com.qp = NULL;
	}
}

static void
connect_reply_upcall(struct iwch_ep *ep, int status)
{
	struct iw_cm_event event;

	CTR5(KTR_IW_CXGB, "%s ep %p so %p state %s status %d", __FUNCTION__, ep, ep->com.so, states[ep->com.state], status);
	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_CONNECT_REPLY;
	event.status = status;
	event.local_addr = ep->com.local_addr;
	event.remote_addr = ep->com.remote_addr;

	if ((status == 0) || (status == ECONNREFUSED)) {
		event.private_data_len = ep->plen;
		event.private_data = ep->mpa_pkt + sizeof(struct mpa_message);
	}
	if (ep->com.cm_id) {
		CTR4(KTR_IW_CXGB, "%s ep %p tid %d status %d", __FUNCTION__, ep,
		     ep->hwtid, status);
		ep->com.cm_id->event_handler(ep->com.cm_id, &event);
	}
	if (status < 0) {
		ep->com.cm_id->rem_ref(ep->com.cm_id);
		ep->com.cm_id = NULL;
		ep->com.qp = NULL;
	}
}

static void
connect_request_upcall(struct iwch_ep *ep)
{
	struct iw_cm_event event;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_CONNECT_REQUEST;
	event.local_addr = ep->com.local_addr;
	event.remote_addr = ep->com.remote_addr;
	event.private_data_len = ep->plen;
	event.private_data = ep->mpa_pkt + sizeof(struct mpa_message);
	event.provider_data = ep;
	event.so = ep->com.so;
	if (state_read(&ep->parent_ep->com) != DEAD)
		ep->parent_ep->com.cm_id->event_handler(
						ep->parent_ep->com.cm_id,
						&event);
	put_ep(&ep->parent_ep->com);
	ep->parent_ep = NULL;
}

static void
established_upcall(struct iwch_ep *ep)
{
	struct iw_cm_event event;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	memset(&event, 0, sizeof(event));
	event.event = IW_CM_EVENT_ESTABLISHED;
	if (ep->com.cm_id) {
		CTR3(KTR_IW_CXGB, "%s ep %p tid %d", __FUNCTION__, ep, ep->hwtid);
		ep->com.cm_id->event_handler(ep->com.cm_id, &event);
	}
}

static void
process_mpa_reply(struct iwch_ep *ep)
{
	struct mpa_message *mpa;
	u16 plen;
	struct iwch_qp_attributes attrs;
	enum iwch_qp_attr_mask mask;
	int err;
	struct mbuf *top, *m;
	int flags = MSG_DONTWAIT;
	struct uio uio;
	int len;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);

	/*
	 * Stop mpa timer.  If it expired, then the state has
	 * changed and we bail since ep_timeout already aborted
	 * the connection.
	 */
	stop_ep_timer(ep);
	if (state_read(&ep->com) != MPA_REQ_SENT)
		return;

	uio.uio_resid = len = 1000000;
	uio.uio_td = ep->com.thread;
	err = soreceive(ep->com.so, NULL, &uio, &top, NULL, &flags);
	if (err) {
		if (err == EWOULDBLOCK) {
			start_ep_timer(ep);
			return;
		}
		err = -err;
		goto err;
	}

	if (ep->com.so->so_rcv.sb_mb) {
		printf("%s data after soreceive called! so %p sb_mb %p top %p\n", 
			__FUNCTION__, ep->com.so, ep->com.so->so_rcv.sb_mb, top);
	}
		
	m = top;
	do {
		/*
		 * If we get more than the supported amount of private data
		 * then we must fail this connection.
		 */
		if (ep->mpa_pkt_len + m->m_len > sizeof(ep->mpa_pkt)) {
			err = (-EINVAL);
			goto err;
		}

		/*
		 * copy the new data into our accumulation buffer.
		 */
		m_copydata(m, 0, m->m_len, &(ep->mpa_pkt[ep->mpa_pkt_len]));
		ep->mpa_pkt_len += m->m_len;
		if (!m->m_next)
			m = m->m_nextpkt;
		else
			m = m->m_next;
	} while (m);

	m_freem(top);

	/*
	 * if we don't even have the mpa message, then bail.
	 */
	if (ep->mpa_pkt_len < sizeof(*mpa))
		return;
	mpa = (struct mpa_message *)ep->mpa_pkt;

	/* Validate MPA header. */
	if (mpa->revision != mpa_rev) {
		CTR2(KTR_IW_CXGB, "%s bad mpa rev %d", __FUNCTION__, mpa->revision);
		err = EPROTO;
		goto err;
	}
	if (memcmp(mpa->key, MPA_KEY_REP, sizeof(mpa->key))) {
		CTR2(KTR_IW_CXGB, "%s bad mpa key |%16s|", __FUNCTION__, mpa->key);
		err = EPROTO;
		goto err;
	}

	plen = ntohs(mpa->private_data_size);

	/*
	 * Fail if there's too much private data.
	 */
	if (plen > MPA_MAX_PRIVATE_DATA) {
		CTR2(KTR_IW_CXGB, "%s plen too big %d", __FUNCTION__, plen);
		err = EPROTO;
		goto err;
	}

	/*
	 * If plen does not account for pkt size
	 */
	if (ep->mpa_pkt_len > (sizeof(*mpa) + plen)) {
		CTR2(KTR_IW_CXGB, "%s pkt too big %d", __FUNCTION__, ep->mpa_pkt_len);
		err = EPROTO;
		goto err;
	}

	ep->plen = (u8) plen;

	/*
	 * If we don't have all the pdata yet, then bail.
	 * We'll continue process when more data arrives.
	 */
	if (ep->mpa_pkt_len < (sizeof(*mpa) + plen))
		return;

	if (mpa->flags & MPA_REJECT) {
		err = ECONNREFUSED;
		goto err;
	}

	/*
	 * If we get here we have accumulated the entire mpa
	 * start reply message including private data. And
	 * the MPA header is valid.
	 */
	CTR1(KTR_IW_CXGB, "%s mpa rpl looks good!", __FUNCTION__);
	state_set(&ep->com, FPDU_MODE);
	ep->mpa_attr.crc_enabled = (mpa->flags & MPA_CRC) | crc_enabled ? 1 : 0;
	ep->mpa_attr.recv_marker_enabled = markers_enabled;
	ep->mpa_attr.xmit_marker_enabled = mpa->flags & MPA_MARKERS ? 1 : 0;
	ep->mpa_attr.version = mpa_rev;
	if (set_tcpinfo(ep)) {
		printf("%s set_tcpinfo error\n", __FUNCTION__);
		goto err;
	}
	CTR5(KTR_IW_CXGB, "%s - crc_enabled=%d, recv_marker_enabled=%d, "
	     "xmit_marker_enabled=%d, version=%d", __FUNCTION__,
	     ep->mpa_attr.crc_enabled, ep->mpa_attr.recv_marker_enabled,
	     ep->mpa_attr.xmit_marker_enabled, ep->mpa_attr.version);

	attrs.mpa_attr = ep->mpa_attr;
	attrs.max_ird = ep->ird;
	attrs.max_ord = ep->ord;
	attrs.llp_stream_handle = ep;
	attrs.next_state = IWCH_QP_STATE_RTS;

	mask = IWCH_QP_ATTR_NEXT_STATE |
	    IWCH_QP_ATTR_LLP_STREAM_HANDLE | IWCH_QP_ATTR_MPA_ATTR |
	    IWCH_QP_ATTR_MAX_IRD | IWCH_QP_ATTR_MAX_ORD;

	/* bind QP and TID with INIT_WR */
	err = iwch_modify_qp(ep->com.qp->rhp,
			     ep->com.qp, mask, &attrs, 1);
	if (!err)
		goto out;
err:
	abort_connection(ep);
out:
	connect_reply_upcall(ep, err);
	return;
}

static void
process_mpa_request(struct iwch_ep *ep)
{
	struct mpa_message *mpa;
	u16 plen;
	int flags = MSG_DONTWAIT;
	struct mbuf *top, *m;
	int err;
	struct uio uio;
	int len;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);

	/*
	 * Stop mpa timer.  If it expired, then the state has
	 * changed and we bail since ep_timeout already aborted
	 * the connection.
	 */
	stop_ep_timer(ep);
	if (state_read(&ep->com) != MPA_REQ_WAIT)
		return;

	uio.uio_resid = len = 1000000;
	uio.uio_td = ep->com.thread;
	err = soreceive(ep->com.so, NULL, &uio, &top, NULL, &flags);
	if (err) {
		if (err == EWOULDBLOCK) {
			start_ep_timer(ep);
			return;
		}
		err = -err;
		goto err;
	}

	m = top;
	do {

		/*
		 * If we get more than the supported amount of private data
		 * then we must fail this connection.
		 */
		if (ep->mpa_pkt_len + m->m_len > sizeof(ep->mpa_pkt)) {
			CTR2(KTR_IW_CXGB, "%s mpa message too big %d", __FUNCTION__, 
				ep->mpa_pkt_len + m->m_len);
			goto err;
		}


		/*
		 * Copy the new data into our accumulation buffer.
		 */
		m_copydata(m, 0, m->m_len, &(ep->mpa_pkt[ep->mpa_pkt_len]));
		ep->mpa_pkt_len += m->m_len;

		if (!m->m_next)
			m = m->m_nextpkt;
		else
			m = m->m_next;
	} while (m);

	m_freem(top);

	/*
	 * If we don't even have the mpa message, then bail.
	 * We'll continue process when more data arrives.
	 */
	if (ep->mpa_pkt_len < sizeof(*mpa)) {
		start_ep_timer(ep);
		CTR2(KTR_IW_CXGB, "%s not enough header %d...waiting...", __FUNCTION__, 
			ep->mpa_pkt_len);
		return;
	}
	mpa = (struct mpa_message *) ep->mpa_pkt;

	/*
	 * Validate MPA Header.
	 */
	if (mpa->revision != mpa_rev) {
		CTR2(KTR_IW_CXGB, "%s bad mpa rev %d", __FUNCTION__, mpa->revision);
		goto err;
	}

	if (memcmp(mpa->key, MPA_KEY_REQ, sizeof(mpa->key))) {
		CTR2(KTR_IW_CXGB, "%s bad mpa key |%16s|", __FUNCTION__, mpa->key);
		goto err;
	}

	plen = ntohs(mpa->private_data_size);

	/*
	 * Fail if there's too much private data.
	 */
	if (plen > MPA_MAX_PRIVATE_DATA) {
		CTR2(KTR_IW_CXGB, "%s plen too big %d", __FUNCTION__, plen);
		goto err;
	}

	/*
	 * If plen does not account for pkt size
	 */
	if (ep->mpa_pkt_len > (sizeof(*mpa) + plen)) {
		CTR2(KTR_IW_CXGB, "%s more data after private data %d", __FUNCTION__, 
			ep->mpa_pkt_len);
		goto err;
	}
	ep->plen = (u8) plen;

	/*
	 * If we don't have all the pdata yet, then bail.
	 */
	if (ep->mpa_pkt_len < (sizeof(*mpa) + plen)) {
		start_ep_timer(ep);
		CTR2(KTR_IW_CXGB, "%s more mpa msg to come %d", __FUNCTION__, 
			ep->mpa_pkt_len);
		return;
	}

	/*
	 * If we get here we have accumulated the entire mpa
	 * start reply message including private data.
	 */
	ep->mpa_attr.crc_enabled = (mpa->flags & MPA_CRC) | crc_enabled ? 1 : 0;
	ep->mpa_attr.recv_marker_enabled = markers_enabled;
	ep->mpa_attr.xmit_marker_enabled = mpa->flags & MPA_MARKERS ? 1 : 0;
	ep->mpa_attr.version = mpa_rev;
	if (set_tcpinfo(ep)) {
		printf("%s set_tcpinfo error\n", __FUNCTION__);
		goto err;
	}
	CTR5(KTR_IW_CXGB, "%s - crc_enabled=%d, recv_marker_enabled=%d, "
	     "xmit_marker_enabled=%d, version=%d", __FUNCTION__,
	     ep->mpa_attr.crc_enabled, ep->mpa_attr.recv_marker_enabled,
	     ep->mpa_attr.xmit_marker_enabled, ep->mpa_attr.version);

	state_set(&ep->com, MPA_REQ_RCVD);

	/* drive upcall */
	connect_request_upcall(ep);
	return;
err:
	abort_connection(ep);
	return;
}

static void
process_peer_close(struct iwch_ep *ep)
{
	struct iwch_qp_attributes attrs;
	int disconnect = 1;
	int release = 0;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);

	mtx_lock(&ep->com.lock);
	switch (ep->com.state) {
	case MPA_REQ_WAIT:
		__state_set(&ep->com, CLOSING);
		break;
	case MPA_REQ_SENT:
		__state_set(&ep->com, CLOSING);
		connect_reply_upcall(ep, -ECONNRESET);
		break;
	case MPA_REQ_RCVD:

		/*
		 * We're gonna mark this puppy DEAD, but keep
		 * the reference on it until the ULP accepts or
		 * rejects the CR.
		 */
		__state_set(&ep->com, CLOSING);
		get_ep(&ep->com);
		break;
	case MPA_REP_SENT:
		__state_set(&ep->com, CLOSING);
		break;
	case FPDU_MODE:
		start_ep_timer(ep);
		__state_set(&ep->com, CLOSING);
		attrs.next_state = IWCH_QP_STATE_CLOSING;
		iwch_modify_qp(ep->com.qp->rhp, ep->com.qp,
			       IWCH_QP_ATTR_NEXT_STATE, &attrs, 1);
		peer_close_upcall(ep);
		break;
	case ABORTING:
		disconnect = 0;
		break;
	case CLOSING:
		__state_set(&ep->com, MORIBUND);
		disconnect = 0;
		break;
	case MORIBUND:
		stop_ep_timer(ep);
		if (ep->com.cm_id && ep->com.qp) {
			attrs.next_state = IWCH_QP_STATE_IDLE;
			iwch_modify_qp(ep->com.qp->rhp, ep->com.qp,
				       IWCH_QP_ATTR_NEXT_STATE, &attrs, 1);
		}
		close_socket(&ep->com);
		close_complete_upcall(ep);
		__state_set(&ep->com, DEAD);
		release = 1;
		disconnect = 0;
		break;
	case DEAD:
		disconnect = 0;
		break;
	default:
		PANIC_IF(1);
	}
	mtx_unlock(&ep->com.lock);
	if (disconnect)
		iwch_ep_disconnect(ep, 0, M_NOWAIT);
	if (release)
		put_ep(&ep->com);
	return;
}

static void
process_conn_error(struct iwch_ep *ep)
{
	struct iwch_qp_attributes attrs;
	int ret;
	int state;

	state = state_read(&ep->com);
	CTR5(KTR_IW_CXGB, "%s ep %p so %p so->so_error %u state %s", __FUNCTION__, ep, ep->com.so, ep->com.so->so_error, states[ep->com.state]);
	switch (state) {
	case MPA_REQ_WAIT:
		stop_ep_timer(ep);
		break;
	case MPA_REQ_SENT:
		stop_ep_timer(ep);
		connect_reply_upcall(ep, -ECONNRESET);
		break;
	case MPA_REP_SENT:
		ep->com.rpl_err = ECONNRESET;
		CTR1(KTR_IW_CXGB, "waking up ep %p", ep);
		break;
	case MPA_REQ_RCVD:

		/*
		 * We're gonna mark this puppy DEAD, but keep
		 * the reference on it until the ULP accepts or
		 * rejects the CR.
		 */
		get_ep(&ep->com);
		break;
	case MORIBUND:
	case CLOSING:
		stop_ep_timer(ep);
		/*FALLTHROUGH*/
	case FPDU_MODE:
		if (ep->com.cm_id && ep->com.qp) {
			attrs.next_state = IWCH_QP_STATE_ERROR;
			ret = iwch_modify_qp(ep->com.qp->rhp,
				     ep->com.qp, IWCH_QP_ATTR_NEXT_STATE,
				     &attrs, 1);
			if (ret)
				log(LOG_ERR,
				       "%s - qp <- error failed!\n",
				       __FUNCTION__);
		}
		peer_abort_upcall(ep);
		break;
	case ABORTING:
		break;
	case DEAD:
		CTR2(KTR_IW_CXGB, "%s so_error %d IN DEAD STATE!!!!", __FUNCTION__, 
			ep->com.so->so_error);
		return;
	default:
		PANIC_IF(1);
		break;
	}

	if (state != ABORTING) {
		close_socket(&ep->com);
		state_set(&ep->com, DEAD);
		put_ep(&ep->com);
	}
	return;
}

static void
process_close_complete(struct iwch_ep *ep)
{
	struct iwch_qp_attributes attrs;
	int release = 0;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	PANIC_IF(!ep);

	/* The cm_id may be null if we failed to connect */
	mtx_lock(&ep->com.lock);
	switch (ep->com.state) {
	case CLOSING:
		__state_set(&ep->com, MORIBUND);
		break;
	case MORIBUND:
		stop_ep_timer(ep);
		if ((ep->com.cm_id) && (ep->com.qp)) {
			attrs.next_state = IWCH_QP_STATE_IDLE;
			iwch_modify_qp(ep->com.qp->rhp,
					     ep->com.qp,
					     IWCH_QP_ATTR_NEXT_STATE,
					     &attrs, 1);
		}
		close_socket(&ep->com);
		close_complete_upcall(ep);
		__state_set(&ep->com, DEAD);
		release = 1;
		break;
	case ABORTING:
		break;
	case DEAD:
	default:
		PANIC_IF(1);
		break;
	}
	mtx_unlock(&ep->com.lock);
	if (release)
		put_ep(&ep->com);
	return;
}

/*
 * T3A does 3 things when a TERM is received:
 * 1) send up a CPL_RDMA_TERMINATE message with the TERM packet
 * 2) generate an async event on the QP with the TERMINATE opcode
 * 3) post a TERMINATE opcde cqe into the associated CQ.
 *
 * For (1), we save the message in the qp for later consumer consumption.
 * For (2), we move the QP into TERMINATE, post a QP event and disconnect.
 * For (3), we toss the CQE in cxio_poll_cq().
 *
 * terminate() handles case (1)...
 */
static int
terminate(struct t3cdev *tdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;
	struct socket *so = toeptoso(toep);
	struct iwch_ep *ep = so->so_upcallarg;

	CTR2(KTR_IW_CXGB, "%s ep %p", __FUNCTION__, ep);
	m_adj(m, sizeof(struct cpl_rdma_terminate));
	CTR2(KTR_IW_CXGB, "%s saving %d bytes of term msg", __FUNCTION__, m->m_len);
	m_copydata(m, 0, m->m_len, ep->com.qp->attr.terminate_buffer);
	ep->com.qp->attr.terminate_msg_len = m->m_len;
	ep->com.qp->attr.is_terminate_local = 0;
	return CPL_RET_BUF_DONE;
}

static int
ec_status(struct t3cdev *tdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;
	struct socket *so = toeptoso(toep);
	struct cpl_rdma_ec_status *rep = cplhdr(m);
	struct iwch_ep *ep;
	struct iwch_qp_attributes attrs;
	int release = 0;

	ep = so->so_upcallarg;
	CTR5(KTR_IW_CXGB, "%s ep %p so %p state %s ec_status %d", __FUNCTION__, ep, ep->com.so, states[ep->com.state], rep->status);
	if (!so || !ep) {
		panic("bogosity ep %p state %d, so %p state %x\n", ep, ep ? ep->com.state : -1, so, so ? so->so_state : -1); 
	}
	mtx_lock(&ep->com.lock);
	switch (ep->com.state) {
	case CLOSING:
		if (!rep->status)
			__state_set(&ep->com, MORIBUND);
		else
			__state_set(&ep->com, ABORTING);
		break;
	case MORIBUND:
		stop_ep_timer(ep);
		if (!rep->status) {
			if ((ep->com.cm_id) && (ep->com.qp)) {
				attrs.next_state = IWCH_QP_STATE_IDLE;
				iwch_modify_qp(ep->com.qp->rhp,
					     ep->com.qp,
					     IWCH_QP_ATTR_NEXT_STATE,
					     &attrs, 1);
			}
			close_socket(&ep->com);
			close_complete_upcall(ep);
			__state_set(&ep->com, DEAD);
			release = 1;
		}
		break;
	case DEAD:
		break;
	default:
		panic("unknown state: %d\n", ep->com.state);
	}
	mtx_unlock(&ep->com.lock);
	if (rep->status) {
		log(LOG_ERR, "%s BAD CLOSE - Aborting tid %u\n",
		       __FUNCTION__, ep->hwtid);
		attrs.next_state = IWCH_QP_STATE_ERROR;
		iwch_modify_qp(ep->com.qp->rhp,
			       ep->com.qp, IWCH_QP_ATTR_NEXT_STATE,
			       &attrs, 1);
	}
	if (release)
		put_ep(&ep->com);
	return CPL_RET_BUF_DONE;
}

static void
ep_timeout(void *arg)
{
	struct iwch_ep *ep = (struct iwch_ep *)arg;
	struct iwch_qp_attributes attrs;
	int err = 0;

	mtx_lock(&ep->com.lock);
	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	switch (ep->com.state) {
	case MPA_REQ_SENT:
		connect_reply_upcall(ep, -ETIMEDOUT);
		break;
	case MPA_REQ_WAIT:
		break;
	case CLOSING:
	case MORIBUND:
		if (ep->com.cm_id && ep->com.qp)
			err = 1;
		break;
	default:
		panic("unknown state: %d\n", ep->com.state);
	}
	__state_set(&ep->com, ABORTING);
	mtx_unlock(&ep->com.lock);
	if (err){
		attrs.next_state = IWCH_QP_STATE_ERROR;
		iwch_modify_qp(ep->com.qp->rhp,
			     ep->com.qp, IWCH_QP_ATTR_NEXT_STATE,
			     &attrs, 1);
	}
	abort_connection(ep);
	put_ep(&ep->com);
}

int
iwch_reject_cr(struct iw_cm_id *cm_id, const void *pdata, u8 pdata_len)
{
	int err;
	struct iwch_ep *ep = to_ep(cm_id);
	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);

	if (state_read(&ep->com) == DEAD) {
		put_ep(&ep->com);
		return (-ECONNRESET);
	}
	PANIC_IF(state_read(&ep->com) != MPA_REQ_RCVD);
	if (mpa_rev == 0) {
		abort_connection(ep);
	} else {
		err = send_mpa_reject(ep, pdata, pdata_len);
		err = soshutdown(ep->com.so, 3);
	}
	return 0;
}

int
iwch_accept_cr(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param)
{
	int err;
	struct iwch_qp_attributes attrs;
	enum iwch_qp_attr_mask mask;
	struct iwch_ep *ep = to_ep(cm_id);
	struct iwch_dev *h = to_iwch_dev(cm_id->device);
	struct iwch_qp *qp = get_qhp(h, conn_param->qpn);

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	if (state_read(&ep->com) == DEAD)
		return (-ECONNRESET);

	PANIC_IF(state_read(&ep->com) != MPA_REQ_RCVD);
	PANIC_IF(!qp);

	if ((conn_param->ord > qp->rhp->attr.max_rdma_read_qp_depth) ||
	    (conn_param->ird > qp->rhp->attr.max_rdma_reads_per_qp)) {
		abort_connection(ep);
		return (-EINVAL);
	}

	cm_id->add_ref(cm_id);
	ep->com.cm_id = cm_id;
	ep->com.qp = qp;

	ep->com.rpl_err = 0;
	ep->com.rpl_done = 0;
	ep->ird = conn_param->ird;
	ep->ord = conn_param->ord;
	CTR3(KTR_IW_CXGB, "%s ird %d ord %d", __FUNCTION__, ep->ird, ep->ord);
	get_ep(&ep->com);

	/* bind QP to EP and move to RTS */
	attrs.mpa_attr = ep->mpa_attr;
	attrs.max_ird = ep->ord;
	attrs.max_ord = ep->ord;
	attrs.llp_stream_handle = ep;
	attrs.next_state = IWCH_QP_STATE_RTS;

	/* bind QP and TID with INIT_WR */
	mask = IWCH_QP_ATTR_NEXT_STATE |
			     IWCH_QP_ATTR_LLP_STREAM_HANDLE |
			     IWCH_QP_ATTR_MPA_ATTR |
			     IWCH_QP_ATTR_MAX_IRD |
			     IWCH_QP_ATTR_MAX_ORD;

	err = iwch_modify_qp(ep->com.qp->rhp,
			     ep->com.qp, mask, &attrs, 1);

	if (err) 
		goto err;

	err = send_mpa_reply(ep, conn_param->private_data,
 			     conn_param->private_data_len);
	if (err)
		goto err;
	state_set(&ep->com, FPDU_MODE);
	established_upcall(ep);
	put_ep(&ep->com);
	return 0;
err:
	ep->com.cm_id = NULL;
	ep->com.qp = NULL;
	cm_id->rem_ref(cm_id);
	put_ep(&ep->com);
	return err;
}

static int init_sock(struct iwch_ep_common *epc)
{
	int err;
	struct sockopt sopt;
	int on=1;

	epc->so->so_upcall = iwch_so_upcall;
	epc->so->so_upcallarg = epc;
	epc->so->so_rcv.sb_flags |= SB_UPCALL;
	epc->so->so_state |= SS_NBIO;
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = SO_NO_DDP;
	sopt.sopt_val = (caddr_t)&on;
	sopt.sopt_valsize = sizeof on;
	sopt.sopt_td = NULL;
	err = sosetopt(epc->so, &sopt);
	if (err) 
		printf("%s can't set SO_NO_DDP err %d\n", __FUNCTION__, err);
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = IPPROTO_TCP;
	sopt.sopt_name = TCP_NODELAY;
	sopt.sopt_val = (caddr_t)&on;
	sopt.sopt_valsize = sizeof on;
	sopt.sopt_td = NULL;
	err = sosetopt(epc->so, &sopt);
	if (err) 
		printf("%s can't set TCP_NODELAY err %d\n", __FUNCTION__, err);

	return 0;
}

static int 
is_loopback_dst(struct iw_cm_id *cm_id)
{
	uint16_t port = cm_id->remote_addr.sin_port;
	struct ifaddr *ifa;

	cm_id->remote_addr.sin_port = 0;
	ifa = ifa_ifwithaddr((struct sockaddr *)&cm_id->remote_addr);
	cm_id->remote_addr.sin_port = port;
	return (ifa != NULL);
}

int
iwch_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *conn_param)
{
	int err = 0;
	struct iwch_dev *h = to_iwch_dev(cm_id->device);
	struct iwch_ep *ep;
	struct rtentry *rt;
	struct toedev *tdev;
	
	if (is_loopback_dst(cm_id)) {
		err = -ENOSYS;
		goto out;
	}

	ep = alloc_ep(sizeof(*ep), M_NOWAIT);
	if (!ep) {
		printf("%s - cannot alloc ep.\n", __FUNCTION__);
		err = (-ENOMEM);
		goto out;
	}
	callout_init(&ep->timer, TRUE);
	ep->plen = conn_param->private_data_len;
	if (ep->plen)
		memcpy(ep->mpa_pkt + sizeof(struct mpa_message),
		       conn_param->private_data, ep->plen);
	ep->ird = conn_param->ird;
	ep->ord = conn_param->ord;

	cm_id->add_ref(cm_id);
	ep->com.cm_id = cm_id;
	ep->com.qp = get_qhp(h, conn_param->qpn);
	ep->com.thread = curthread;
	PANIC_IF(!ep->com.qp);
	CTR4(KTR_IW_CXGB, "%s qpn 0x%x qp %p cm_id %p", __FUNCTION__, conn_param->qpn,
	     ep->com.qp, cm_id);

	ep->com.so = cm_id->so;
	err = init_sock(&ep->com);
	if (err)
		goto fail2;

	/* find a route */
	rt = find_route(cm_id->local_addr.sin_addr.s_addr,
			cm_id->remote_addr.sin_addr.s_addr,
			cm_id->local_addr.sin_port,
			cm_id->remote_addr.sin_port, IPTOS_LOWDELAY);
	if (!rt) {
		printf("%s - cannot find route.\n", __FUNCTION__);
		err = EHOSTUNREACH;
		goto fail2;
	}

	if (!(rt->rt_ifp->if_flags & IFCAP_TOE)) {
		printf("%s - interface not TOE capable.\n", __FUNCTION__);
		goto fail3;
	}
	tdev = TOEDEV(rt->rt_ifp);
	if (tdev == NULL) {
		printf("%s - No toedev for interface.\n", __FUNCTION__);
		goto fail3;
	}
	if (!tdev->tod_can_offload(tdev, ep->com.so)) {
		printf("%s - interface cannot offload!.\n", __FUNCTION__);
		goto fail3;
	}
	RTFREE(rt);

	state_set(&ep->com, CONNECTING);
	ep->com.local_addr = cm_id->local_addr;
	ep->com.remote_addr = cm_id->remote_addr;
	err = soconnect(ep->com.so, (struct sockaddr *)&ep->com.remote_addr, 
		ep->com.thread);
	if (!err)
		goto out;
fail3:
	RTFREE(ep->dst);
fail2:
	put_ep(&ep->com);
out:
	return err;
}

int
iwch_create_listen(struct iw_cm_id *cm_id, int backlog)
{
	int err = 0;
	struct iwch_listen_ep *ep;

	ep = alloc_ep(sizeof(*ep), M_NOWAIT);
	if (!ep) {
		printf("%s - cannot alloc ep.\n", __FUNCTION__);
		err = ENOMEM;
		goto out;
	}
	CTR2(KTR_IW_CXGB, "%s ep %p", __FUNCTION__, ep);
	cm_id->add_ref(cm_id);
	ep->com.cm_id = cm_id;
	ep->backlog = backlog;
	ep->com.local_addr = cm_id->local_addr;
	ep->com.thread = curthread;
	state_set(&ep->com, LISTEN);

	ep->com.so = cm_id->so;
	err = init_sock(&ep->com);
	if (err)
		goto fail;

	err = solisten(ep->com.so, ep->backlog, ep->com.thread);
	if (!err) {
		cm_id->provider_data = ep;
		goto out;
	}
	close_socket(&ep->com);
fail:
	cm_id->rem_ref(cm_id);
	put_ep(&ep->com);
out:
	return err;
}

int
iwch_destroy_listen(struct iw_cm_id *cm_id)
{
	struct iwch_listen_ep *ep = to_listen_ep(cm_id);

	CTR2(KTR_IW_CXGB, "%s ep %p", __FUNCTION__, ep);

	state_set(&ep->com, DEAD);
	close_socket(&ep->com);
	cm_id->rem_ref(cm_id);
	put_ep(&ep->com);
	return 0;
}

int
iwch_ep_disconnect(struct iwch_ep *ep, int abrupt, int flags)
{
	int close = 0;

	mtx_lock(&ep->com.lock);

	PANIC_IF(!ep);
	PANIC_IF(!ep->com.so);

	CTR5(KTR_IW_CXGB, "%s ep %p so %p state %s, abrupt %d", __FUNCTION__, ep,
	     ep->com.so, states[ep->com.state], abrupt);

	if (ep->com.state == DEAD) {
		CTR2(KTR_IW_CXGB, "%s already dead ep %p", __FUNCTION__, ep);
		goto out;
	}

	if (abrupt) {
		if (ep->com.state != ABORTING) {
			ep->com.state = ABORTING;
			close = 1;
		}
		goto out;
	}

	switch (ep->com.state) {
	case MPA_REQ_WAIT:
	case MPA_REQ_SENT:
	case MPA_REQ_RCVD:
	case MPA_REP_SENT:
	case FPDU_MODE:
		start_ep_timer(ep);
		ep->com.state = CLOSING;
		close = 1;
		break;
	case CLOSING:
		ep->com.state = MORIBUND;
		close = 1;
		break;
	case MORIBUND:
	case ABORTING:
		break;
	default:
		panic("unknown state: %d\n", ep->com.state);
		break;
	}
out:
	mtx_unlock(&ep->com.lock);
	if (close) {
		if (abrupt)
			abort_connection(ep);
		else
			shutdown_socket(&ep->com);
	}
	return 0;
}

static void
process_data(struct iwch_ep *ep)
{
	struct sockaddr_in *local, *remote;

	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);

	switch (state_read(&ep->com)) {
	case MPA_REQ_SENT:
		process_mpa_reply(ep);
		break;
	case MPA_REQ_WAIT:

		/*
		 * XXX
		 * Set local and remote addrs here because when we
		 * dequeue the newly accepted socket, they aren't set
		 * yet in the pcb!
		 */
		in_getsockaddr(ep->com.so, (struct sockaddr **)&local);
		in_getpeeraddr(ep->com.so, (struct sockaddr **)&remote);
		CTR3(KTR_IW_CXGB, "%s local %s remote %s", __FUNCTION__, 
			inet_ntoa(local->sin_addr),
			inet_ntoa(remote->sin_addr));
		ep->com.local_addr = *local;
		ep->com.remote_addr = *remote;
		free(local, M_SONAME);
		free(remote, M_SONAME);
		process_mpa_request(ep);
		break;
	default:
		if (ep->com.so->so_rcv.sb_cc) 
			printf("%s Unexpected streaming data."
			       " ep %p state %d so %p so_state %x so_rcv.sb_cc %u so_rcv.sb_mb %p\n",
			       __FUNCTION__, ep, state_read(&ep->com), ep->com.so, ep->com.so->so_state,
			       ep->com.so->so_rcv.sb_cc, ep->com.so->so_rcv.sb_mb);
		break;
	}
	return;
}

static void
process_connected(struct iwch_ep *ep)
{
	CTR4(KTR_IW_CXGB, "%s ep %p so %p state %s", __FUNCTION__, ep, ep->com.so, states[ep->com.state]);
	if ((ep->com.so->so_state & SS_ISCONNECTED) && !ep->com.so->so_error) {
		send_mpa_req(ep);
	} else {
		connect_reply_upcall(ep, -ep->com.so->so_error);
		close_socket(&ep->com);
		state_set(&ep->com, DEAD);
		put_ep(&ep->com);
	}
}

static struct socket *
dequeue_socket(struct socket *head, struct sockaddr_in **remote, struct iwch_ep *child_ep)
{
	struct socket *so;

	ACCEPT_LOCK();
	so = TAILQ_FIRST(&head->so_comp);
	if (!so) {
		ACCEPT_UNLOCK();
		return NULL;
	}
	TAILQ_REMOVE(&head->so_comp, so, so_list);
	head->so_qlen--;
	SOCK_LOCK(so);
	so->so_qstate &= ~SQ_COMP;
	so->so_head = NULL;
	soref(so);
	so->so_rcv.sb_flags |= SB_UPCALL;
	so->so_state |= SS_NBIO;
	so->so_upcall = iwch_so_upcall;
	so->so_upcallarg = child_ep;
	PANIC_IF(!(so->so_state & SS_ISCONNECTED));
	PANIC_IF(so->so_error);
	SOCK_UNLOCK(so);
	ACCEPT_UNLOCK();
	soaccept(so, (struct sockaddr **)remote);
	return so;
}

static void
process_newconn(struct iwch_ep *parent_ep)
{
	struct socket *child_so;
	struct iwch_ep *child_ep;
	struct sockaddr_in *remote;

	CTR3(KTR_IW_CXGB, "%s parent ep %p so %p", __FUNCTION__, parent_ep, parent_ep->com.so);
	child_ep = alloc_ep(sizeof(*child_ep), M_NOWAIT);
	if (!child_ep) {
		log(LOG_ERR, "%s - failed to allocate ep entry!\n",
		       __FUNCTION__);
		return;
	}
	child_so = dequeue_socket(parent_ep->com.so, &remote, child_ep);
	if (!child_so) {
		log(LOG_ERR, "%s - failed to dequeue child socket!\n",
		       __FUNCTION__);
		__free_ep(&child_ep->com);
		return;
	}
	CTR3(KTR_IW_CXGB, "%s remote addr %s port %d", __FUNCTION__, 
		inet_ntoa(remote->sin_addr), ntohs(remote->sin_port));
	child_ep->com.so = child_so;
	child_ep->com.cm_id = NULL;
	child_ep->com.thread = parent_ep->com.thread;
	child_ep->parent_ep = parent_ep;
	free(remote, M_SONAME);
	get_ep(&parent_ep->com);
	child_ep->parent_ep = parent_ep;
	callout_init(&child_ep->timer, TRUE);
	state_set(&child_ep->com, MPA_REQ_WAIT);
	start_ep_timer(child_ep);

	/* maybe the request has already been queued up on the socket... */
	process_mpa_request(child_ep);
}

static void
iwch_so_upcall(struct socket *so, void *arg, int waitflag)
{
	struct iwch_ep *ep = arg;

	CTR6(KTR_IW_CXGB, "%s so %p so state %x ep %p ep state(%d)=%s", __FUNCTION__, so, so->so_state, ep, ep->com.state, states[ep->com.state]);
	mtx_lock(&req_lock);
	if (ep && ep->com.so && !ep->com.entry.tqe_prev) {
		get_ep(&ep->com);
		TAILQ_INSERT_TAIL(&req_list, &ep->com, entry);
		taskqueue_enqueue(iw_cxgb_taskq, &iw_cxgb_task);
	}
	mtx_unlock(&req_lock);
}

static void
process_socket_event(struct iwch_ep *ep)
{
	int state = state_read(&ep->com);
	struct socket *so = ep->com.so;
	
	CTR6(KTR_IW_CXGB, "%s so %p so state %x ep %p ep state(%d)=%s", __FUNCTION__, so, so->so_state, ep, ep->com.state, states[ep->com.state]);
	if (state == CONNECTING) {
		process_connected(ep);
		return;
	}

	if (state == LISTEN) {
		process_newconn(ep);
		return;
	}

	/* connection error */
	if (so->so_error) {
		process_conn_error(ep);
		return;
	}

	/* peer close */
	if ((so->so_rcv.sb_state & SBS_CANTRCVMORE) && state < CLOSING) {
		process_peer_close(ep);
		return;
	}

	/* close complete */
	if (so->so_state & (SS_ISDISCONNECTED)) {
		process_close_complete(ep);
		return;
	}
	
	/* rx data */
	process_data(ep);
	return;
}

static void
process_req(void *ctx, int pending)
{
	struct iwch_ep_common *epc;

	CTR1(KTR_IW_CXGB, "%s enter", __FUNCTION__);
	mtx_lock(&req_lock);
	while (!TAILQ_EMPTY(&req_list)) {
		epc = TAILQ_FIRST(&req_list);
		TAILQ_REMOVE(&req_list, epc, entry);
		epc->entry.tqe_prev = NULL;
		mtx_unlock(&req_lock);
		if (epc->so)
			process_socket_event((struct iwch_ep *)epc);
		put_ep(epc);
		mtx_lock(&req_lock);
	}
	mtx_unlock(&req_lock);
}

int
iwch_cm_init(void)
{
	TAILQ_INIT(&req_list);
	mtx_init(&req_lock, "iw_cxgb req_list lock", NULL, MTX_DEF);
	iw_cxgb_taskq = taskqueue_create("iw_cxgb_taskq", M_NOWAIT,
		taskqueue_thread_enqueue, &iw_cxgb_taskq);
        if (iw_cxgb_taskq == NULL) {
                printf("failed to allocate iw_cxgb taskqueue\n");
                return (ENOMEM);
        }
        taskqueue_start_threads(&iw_cxgb_taskq, 1, PI_NET, "iw_cxgb taskq");
        TASK_INIT(&iw_cxgb_task, 0, process_req, NULL);
	t3tom_register_cpl_handler(CPL_RDMA_TERMINATE, terminate);
	t3tom_register_cpl_handler(CPL_RDMA_EC_STATUS, ec_status);
	return 0;
}

void
iwch_cm_term(void)
{
	t3tom_register_cpl_handler(CPL_RDMA_TERMINATE, NULL);
	t3tom_register_cpl_handler(CPL_RDMA_EC_STATUS, NULL);
	taskqueue_drain(iw_cxgb_taskq, &iw_cxgb_task);
	taskqueue_free(iw_cxgb_taskq);
}

