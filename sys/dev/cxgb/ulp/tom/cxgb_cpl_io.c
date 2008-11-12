/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
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
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/sockstate.h>
#include <sys/sockopt.h>
#include <sys/socket.h>
#include <sys/sockbuf.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/priv.h>

#if __FreeBSD_version >= 800044
#include <sys/vimage.h>
#else
#define V_tcp_do_autosndbuf tcp_do_autosndbuf
#define V_tcp_autosndbuf_max tcp_autosndbuf_max
#define V_tcp_do_rfc1323 tcp_do_rfc1323
#define V_tcp_do_autorcvbuf tcp_do_autorcvbuf
#define V_tcp_autorcvbuf_max tcp_autorcvbuf_max
#define V_tcpstat tcpstat
#endif

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>


#include <cxgb_osdep.h>
#include <sys/mbufq.h>

#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_offload.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_syncache.h>
#include <netinet/tcp_timer.h>
#include <net/route.h>

#include <t3cdev.h>
#include <common/cxgb_firmware_exports.h>
#include <common/cxgb_t3_cpl.h>
#include <common/cxgb_tcb.h>
#include <common/cxgb_ctl_defs.h>
#include <cxgb_offload.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bus.h>
#include <sys/mvec.h>
#include <ulp/toecore/cxgb_toedev.h>
#include <ulp/tom/cxgb_l2t.h>
#include <ulp/tom/cxgb_defs.h>
#include <ulp/tom/cxgb_tom.h>
#include <ulp/tom/cxgb_t3_ddp.h>
#include <ulp/tom/cxgb_toepcb.h>
#include <ulp/tom/cxgb_tcp.h>
#include <ulp/tom/cxgb_tcp_offload.h>

/*
 * For ULP connections HW may add headers, e.g., for digests, that aren't part
 * of the messages sent by the host but that are part of the TCP payload and
 * therefore consume TCP sequence space.  Tx connection parameters that
 * operate in TCP sequence space are affected by the HW additions and need to
 * compensate for them to accurately track TCP sequence numbers. This array
 * contains the compensating extra lengths for ULP packets.  It is indexed by
 * a packet's ULP submode.
 */
const unsigned int t3_ulp_extra_len[] = {0, 4, 4, 8};

#ifdef notyet
/*
 * This sk_buff holds a fake header-only TCP segment that we use whenever we
 * need to exploit SW TCP functionality that expects TCP headers, such as
 * tcp_create_openreq_child().  It's a RO buffer that may be used by multiple
 * CPUs without locking.
 */
static struct mbuf *tcphdr_mbuf __read_mostly;
#endif

/*
 * Size of WRs in bytes.  Note that we assume all devices we are handling have
 * the same WR size.
 */
static unsigned int wrlen __read_mostly;

/*
 * The number of WRs needed for an skb depends on the number of page fragments
 * in the skb and whether it has any payload in its main body.  This maps the
 * length of the gather list represented by an skb into the # of necessary WRs.
 */
static unsigned int mbuf_wrs[TX_MAX_SEGS + 1] __read_mostly;

/*
 * Max receive window supported by HW in bytes.  Only a small part of it can
 * be set through option0, the rest needs to be set through RX_DATA_ACK.
 */
#define MAX_RCV_WND ((1U << 27) - 1)

/*
 * Min receive window.  We want it to be large enough to accommodate receive
 * coalescing, handle jumbo frames, and not trigger sender SWS avoidance.
 */
#define MIN_RCV_WND (24 * 1024U)
#define INP_TOS(inp) ((inp_ip_tos_get(inp) >> 2) & M_TOS)

#define VALIDATE_SEQ 0
#define VALIDATE_SOCK(so)
#define DEBUG_WR 0

#define TCP_TIMEWAIT	1
#define TCP_CLOSE	2
#define TCP_DROP	3

extern int tcp_do_autorcvbuf;
extern int tcp_do_autosndbuf;
extern int tcp_autorcvbuf_max;
extern int tcp_autosndbuf_max;

static void t3_send_reset(struct toepcb *toep);
static void send_abort_rpl(struct mbuf *m, struct toedev *tdev, int rst_status);
static inline void free_atid(struct t3cdev *cdev, unsigned int tid);
static void handle_syncache_event(int event, void *arg);

static inline void
SBAPPEND(struct sockbuf *sb, struct mbuf *n)
{
	struct mbuf *m;

	m = sb->sb_mb;
	while (m) {
		KASSERT(((m->m_flags & M_EXT) && (m->m_ext.ext_type == EXT_EXTREF)) ||
		    !(m->m_flags & M_EXT), ("unexpected type M_EXT=%d ext_type=%d m_len=%d\n",
			!!(m->m_flags & M_EXT), m->m_ext.ext_type, m->m_len));
		KASSERT(m->m_next != (struct mbuf *)0xffffffff, ("bad next value m_next=%p m_nextpkt=%p m_flags=0x%x",
			m->m_next, m->m_nextpkt, m->m_flags));
		m = m->m_next;
	}
	m = n;
	while (m) {
		KASSERT(((m->m_flags & M_EXT) && (m->m_ext.ext_type == EXT_EXTREF)) ||
		    !(m->m_flags & M_EXT), ("unexpected type M_EXT=%d ext_type=%d m_len=%d\n",
			!!(m->m_flags & M_EXT), m->m_ext.ext_type, m->m_len));
		KASSERT(m->m_next != (struct mbuf *)0xffffffff, ("bad next value m_next=%p m_nextpkt=%p m_flags=0x%x",
			m->m_next, m->m_nextpkt, m->m_flags));
		m = m->m_next;
	}
	KASSERT(sb->sb_flags & SB_NOCOALESCE, ("NOCOALESCE not set"));
	sbappendstream_locked(sb, n);
	m = sb->sb_mb;

	while (m) {
		KASSERT(m->m_next != (struct mbuf *)0xffffffff, ("bad next value m_next=%p m_nextpkt=%p m_flags=0x%x",
			m->m_next, m->m_nextpkt, m->m_flags));
		m = m->m_next;
	}
}

static inline int
is_t3a(const struct toedev *dev)
{
	return (dev->tod_ttid == TOE_ID_CHELSIO_T3);
}

static void
dump_toepcb(struct toepcb *toep)
{
	DPRINTF("qset_idx=%d qset=%d ulp_mode=%d mtu_idx=%d tid=%d\n",
	    toep->tp_qset_idx, toep->tp_qset, toep->tp_ulp_mode,
	    toep->tp_mtu_idx, toep->tp_tid);

	DPRINTF("wr_max=%d wr_avail=%d wr_unacked=%d mss_clamp=%d flags=0x%x\n",
	    toep->tp_wr_max, toep->tp_wr_avail, toep->tp_wr_unacked, 
	    toep->tp_mss_clamp, toep->tp_flags);
}

#ifndef RTALLOC2_DEFINED
static struct rtentry *
rtalloc2(struct sockaddr *dst, int report, u_long ignflags)
{
	struct rtentry *rt = NULL;
	
	if ((rt = rtalloc1(dst, report, ignflags)) != NULL)
		RT_UNLOCK(rt);

	return (rt);
}
#endif

/*
 * Determine whether to send a CPL message now or defer it.  A message is
 * deferred if the connection is in SYN_SENT since we don't know the TID yet.
 * For connections in other states the message is sent immediately.
 * If through_l2t is set the message is subject to ARP processing, otherwise
 * it is sent directly.
 */
static inline void
send_or_defer(struct toepcb *toep, struct mbuf *m, int through_l2t)
{
	struct tcpcb *tp = toep->tp_tp;

	if (__predict_false(tp->t_state == TCPS_SYN_SENT)) {
		inp_wlock(tp->t_inpcb);
		mbufq_tail(&toep->out_of_order_queue, m);  // defer
		inp_wunlock(tp->t_inpcb);
	} else if (through_l2t)
		l2t_send(TOEP_T3C_DEV(toep), m, toep->tp_l2t);  // send through L2T
	else
		cxgb_ofld_send(TOEP_T3C_DEV(toep), m);          // send directly
}

static inline unsigned int
mkprio(unsigned int cntrl, const struct toepcb *toep)
{
        return (cntrl);
}

/*
 * Populate a TID_RELEASE WR.  The skb must be already propely sized.
 */
static inline void
mk_tid_release(struct mbuf *m, const struct toepcb *toep, unsigned int tid)
{
	struct cpl_tid_release *req;

	m_set_priority(m, mkprio(CPL_PRIORITY_SETUP, toep));
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	req = mtod(m, struct cpl_tid_release *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = 0;
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_TID_RELEASE, tid));
}

static inline void
make_tx_data_wr(struct socket *so, struct mbuf *m, int len, struct mbuf *tail)
{
	INIT_VNET_INET(so->so_vnet);
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct tx_data_wr *req;
	struct sockbuf *snd;
	
	inp_lock_assert(tp->t_inpcb);
	snd = so_sockbuf_snd(so);
	
	req = mtod(m, struct tx_data_wr *);
	m->m_len = sizeof(*req);
	req->wr_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_TX_DATA));
	req->wr_lo = htonl(V_WR_TID(toep->tp_tid));
	/* len includes the length of any HW ULP additions */
	req->len = htonl(len);
	req->param = htonl(V_TX_PORT(toep->tp_l2t->smt_idx));
	/* V_TX_ULP_SUBMODE sets both the mode and submode */
	req->flags = htonl(V_TX_ULP_SUBMODE(/*skb_ulp_mode(skb)*/ 0) |
	                   V_TX_URG(/* skb_urgent(skb) */ 0 ) |
	                   V_TX_SHOVE((!(tp->t_flags & TF_MORETOCOME) &&
				   (tail ? 0 : 1))));
	req->sndseq = htonl(tp->snd_nxt);
	if (__predict_false((toep->tp_flags & TP_DATASENT) == 0)) {
		req->flags |= htonl(V_TX_ACK_PAGES(2) | F_TX_INIT | 
				    V_TX_CPU_IDX(toep->tp_qset));
 
		/* Sendbuffer is in units of 32KB.
		 */
		if (V_tcp_do_autosndbuf && snd->sb_flags & SB_AUTOSIZE) 
			req->param |= htonl(V_TX_SNDBUF(V_tcp_autosndbuf_max >> 15));
		else {
			req->param |= htonl(V_TX_SNDBUF(snd->sb_hiwat >> 15));
		}
		
		toep->tp_flags |= TP_DATASENT;
	}
}

#define IMM_LEN 64 /* XXX - see WR_LEN in the cxgb driver */

int
t3_push_frames(struct socket *so, int req_completion)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	
	struct mbuf *tail, *m0, *last;
	struct t3cdev *cdev;
	struct tom_data *d;
	int state, bytes, count, total_bytes;
	bus_dma_segment_t segs[TX_MAX_SEGS], *segp;
	struct sockbuf *snd;
	
	if (tp->t_state == TCPS_SYN_SENT || tp->t_state == TCPS_CLOSED) {
		DPRINTF("tcp state=%d\n", tp->t_state);	
		return (0);
	}	

	state = so_state_get(so);
	
	if (state & (SS_ISDISCONNECTING|SS_ISDISCONNECTED)) {
		DPRINTF("disconnecting\n");
		
		return (0);
	}

	inp_lock_assert(tp->t_inpcb);

	snd = so_sockbuf_snd(so);
	sockbuf_lock(snd);

	d = TOM_DATA(toep->tp_toedev);
	cdev = d->cdev;

	last = tail = snd->sb_sndptr ? snd->sb_sndptr : snd->sb_mb;

	total_bytes = 0;
	DPRINTF("wr_avail=%d tail=%p snd.cc=%d tp_last=%p\n",
	    toep->tp_wr_avail, tail, snd->sb_cc, toep->tp_m_last);

	if (last && toep->tp_m_last == last  && snd->sb_sndptroff != 0) {
		KASSERT(tail, ("sbdrop error"));
		last = tail = tail->m_next;
	}

	if ((toep->tp_wr_avail == 0 ) || (tail == NULL)) {
		DPRINTF("wr_avail=%d tail=%p\n", toep->tp_wr_avail, tail);
		sockbuf_unlock(snd);

		return (0);		
	}
			
	toep->tp_m_last = NULL;
	while (toep->tp_wr_avail && (tail != NULL)) {
		count = bytes = 0;
		segp = segs;
		if ((m0 = m_gethdr(M_NOWAIT, MT_DATA)) == NULL) {
			sockbuf_unlock(snd);
			return (0);
		}
		/*
		 * If the data in tail fits as in-line, then
		 * make an immediate data wr.
		 */
		if (tail->m_len <= IMM_LEN) {
			count = 1;
			bytes = tail->m_len;
			last = tail;
			tail = tail->m_next;
			m_set_sgl(m0, NULL);
			m_set_sgllen(m0, 0);
			make_tx_data_wr(so, m0, bytes, tail);
			m_append(m0, bytes, mtod(last, caddr_t));
			KASSERT(!m0->m_next, ("bad append"));
		} else {
			while ((mbuf_wrs[count + 1] <= toep->tp_wr_avail)
			    && (tail != NULL) && (count < TX_MAX_SEGS-1)) {
				bytes += tail->m_len;
				last = tail;
				count++;
				/*
				 * technically an abuse to be using this for a VA
				 * but less gross than defining my own structure
				 * or calling pmap_kextract from here :-|
				 */
				segp->ds_addr = (bus_addr_t)tail->m_data;
				segp->ds_len = tail->m_len;
				DPRINTF("count=%d wr_needed=%d ds_addr=%p ds_len=%d\n",
				    count, mbuf_wrs[count], tail->m_data, tail->m_len);
				segp++;
				tail = tail->m_next;
			}
			DPRINTF("wr_avail=%d mbuf_wrs[%d]=%d tail=%p\n",
			    toep->tp_wr_avail, count, mbuf_wrs[count], tail);	

			m_set_sgl(m0, segs);
			m_set_sgllen(m0, count);
			make_tx_data_wr(so, m0, bytes, tail);
		}
		m_set_priority(m0, mkprio(CPL_PRIORITY_DATA, toep));

		if (tail) {
			snd->sb_sndptr = tail;
			toep->tp_m_last = NULL;
		} else 
			toep->tp_m_last = snd->sb_sndptr = last;


		DPRINTF("toep->tp_m_last=%p\n", toep->tp_m_last);

		snd->sb_sndptroff += bytes;
		total_bytes += bytes;
		toep->tp_write_seq += bytes;
		CTR6(KTR_TOM, "t3_push_frames: wr_avail=%d mbuf_wrs[%d]=%d"
		    " tail=%p sndptr=%p sndptroff=%d",
		    toep->tp_wr_avail, count, mbuf_wrs[count],
		    tail, snd->sb_sndptr, snd->sb_sndptroff);	
		if (tail)
			CTR4(KTR_TOM, "t3_push_frames: total_bytes=%d"
			    " tp_m_last=%p tailbuf=%p snd_una=0x%08x",
			    total_bytes, toep->tp_m_last, tail->m_data,
			    tp->snd_una);
		else
			CTR3(KTR_TOM, "t3_push_frames: total_bytes=%d"
			    " tp_m_last=%p snd_una=0x%08x",
			    total_bytes, toep->tp_m_last, tp->snd_una);


#ifdef KTR		
{
		int i;

		i = 0;
		while (i < count && m_get_sgllen(m0)) {
			if ((count - i) >= 3) {
				CTR6(KTR_TOM,
				    "t3_push_frames: pa=0x%zx len=%d pa=0x%zx"
				    " len=%d pa=0x%zx len=%d",
				    segs[i].ds_addr, segs[i].ds_len,
				    segs[i + 1].ds_addr, segs[i + 1].ds_len,
				    segs[i + 2].ds_addr, segs[i + 2].ds_len);
				    i += 3;
			} else if ((count - i) == 2) {
				CTR4(KTR_TOM, 
				    "t3_push_frames: pa=0x%zx len=%d pa=0x%zx"
				    " len=%d",
				    segs[i].ds_addr, segs[i].ds_len,
				    segs[i + 1].ds_addr, segs[i + 1].ds_len);
				    i += 2;
			} else {
				CTR2(KTR_TOM, "t3_push_frames: pa=0x%zx len=%d",
				    segs[i].ds_addr, segs[i].ds_len);
				i++;
			}
	
		}
}
#endif		
                 /*
		 * remember credits used
		 */
		m0->m_pkthdr.csum_data = mbuf_wrs[count];
		m0->m_pkthdr.len = bytes;
		toep->tp_wr_avail -= mbuf_wrs[count];
		toep->tp_wr_unacked += mbuf_wrs[count];
		
		if ((req_completion && toep->tp_wr_unacked == mbuf_wrs[count]) ||
		    toep->tp_wr_unacked >= toep->tp_wr_max / 2) {
			struct work_request_hdr *wr = cplhdr(m0);

			wr->wr_hi |= htonl(F_WR_COMPL);
			toep->tp_wr_unacked = 0;	
		}
		KASSERT((m0->m_pkthdr.csum_data > 0) &&
		    (m0->m_pkthdr.csum_data <= 4), ("bad credit count %d",
			m0->m_pkthdr.csum_data));
		m0->m_type = MT_DONTFREE;
		enqueue_wr(toep, m0);
		DPRINTF("sending offload tx with %d bytes in %d segments\n",
		    bytes, count);
		l2t_send(cdev, m0, toep->tp_l2t);
	}
	sockbuf_unlock(snd);
	return (total_bytes);
}

/*
 * Close a connection by sending a CPL_CLOSE_CON_REQ message.  Cannot fail
 * under any circumstances.  We take the easy way out and always queue the
 * message to the write_queue.  We can optimize the case where the queue is
 * already empty though the optimization is probably not worth it.
 */
static void
close_conn(struct socket *so)
{
	struct mbuf *m;
	struct cpl_close_con_req *req;
	struct tom_data *d;
	struct inpcb *inp = so_sotoinpcb(so);
	struct tcpcb *tp;
	struct toepcb *toep;
	unsigned int tid; 


	inp_wlock(inp);
	tp = so_sototcpcb(so);
	toep = tp->t_toe;
	
	if (tp->t_state != TCPS_SYN_SENT)
		t3_push_frames(so, 1);
	
	if (toep->tp_flags & TP_FIN_SENT) {
		inp_wunlock(inp);
		return;
	}

	tid = toep->tp_tid;
	    
	d = TOM_DATA(toep->tp_toedev);
	
	m = m_gethdr_nofail(sizeof(*req));
	m_set_priority(m, CPL_PRIORITY_DATA);
	m_set_sgl(m, NULL);
	m_set_sgllen(m, 0);

	toep->tp_flags |= TP_FIN_SENT;
	req = mtod(m, struct cpl_close_con_req *);
	
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_CLOSE_CON));
	req->wr.wr_lo = htonl(V_WR_TID(tid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_CON_REQ, tid));
	req->rsvd = 0;
	inp_wunlock(inp);
	/*
	 * XXX - need to defer shutdown while there is still data in the queue
	 *
	 */
	CTR4(KTR_TOM, "%s CLOSE_CON_REQ so %p tp %p tid=%u", __FUNCTION__, so, tp, tid);
	cxgb_ofld_send(d->cdev, m);

}

/*
 * Handle an ARP failure for a CPL_ABORT_REQ.  Change it into a no RST variant
 * and send it along.
 */
static void
abort_arp_failure(struct t3cdev *cdev, struct mbuf *m)
{
	struct cpl_abort_req *req = cplhdr(m);

	req->cmd = CPL_ABORT_NO_RST;
	cxgb_ofld_send(cdev, m);
}

/*
 * Send RX credits through an RX_DATA_ACK CPL message.  If nofail is 0 we are
 * permitted to return without sending the message in case we cannot allocate
 * an sk_buff.  Returns the number of credits sent.
 */
uint32_t
t3_send_rx_credits(struct tcpcb *tp, uint32_t credits, uint32_t dack, int nofail)
{
	struct mbuf *m;
	struct cpl_rx_data_ack *req;
	struct toepcb *toep = tp->t_toe;
	struct toedev *tdev = toep->tp_toedev;
	
	m = m_gethdr_nofail(sizeof(*req));

	DPRINTF("returning %u credits to HW\n", credits);
	
	req = mtod(m, struct cpl_rx_data_ack *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = 0;
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_RX_DATA_ACK, toep->tp_tid));
	req->credit_dack = htonl(dack | V_RX_CREDITS(credits));
	m_set_priority(m, mkprio(CPL_PRIORITY_ACK, toep)); 
	cxgb_ofld_send(TOM_DATA(tdev)->cdev, m);
	return (credits);
}

/*
 * Send RX_DATA_ACK CPL message to request a modulation timer to be scheduled.
 * This is only used in DDP mode, so we take the opportunity to also set the
 * DACK mode and flush any Rx credits.
 */
void
t3_send_rx_modulate(struct toepcb *toep)
{
	struct mbuf *m;
	struct cpl_rx_data_ack *req;

	m = m_gethdr_nofail(sizeof(*req));

	req = mtod(m, struct cpl_rx_data_ack *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = 0;
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_RX_DATA_ACK, toep->tp_tid));
	req->credit_dack = htonl(F_RX_MODULATE | F_RX_DACK_CHANGE |
				 V_RX_DACK_MODE(1) |
				 V_RX_CREDITS(toep->tp_copied_seq - toep->tp_rcv_wup));
	m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
	cxgb_ofld_send(TOEP_T3C_DEV(toep), m);
	toep->tp_rcv_wup = toep->tp_copied_seq;
}

/*
 * Handle receipt of an urgent pointer.
 */
static void
handle_urg_ptr(struct socket *so, uint32_t urg_seq)
{
#ifdef URGENT_DATA_SUPPORTED
	struct tcpcb *tp = so_sototcpcb(so);

	urg_seq--;   /* initially points past the urgent data, per BSD */

	if (tp->urg_data && !after(urg_seq, tp->urg_seq))
		return;                                 /* duplicate pointer */
	sk_send_sigurg(sk);
	if (tp->urg_seq == tp->copied_seq && tp->urg_data &&
	    !sock_flag(sk, SOCK_URGINLINE) && tp->copied_seq != tp->rcv_nxt) {
		struct sk_buff *skb = skb_peek(&sk->sk_receive_queue);

		tp->copied_seq++;
		if (skb && tp->copied_seq - TCP_SKB_CB(skb)->seq >= skb->len)
			tom_eat_skb(sk, skb, 0);
	}
	tp->urg_data = TCP_URG_NOTYET;
	tp->urg_seq = urg_seq;
#endif
}

/*
 * Returns true if a socket cannot accept new Rx data.
 */
static inline int
so_no_receive(const struct socket *so)
{
	return (so_state_get(so) & (SS_ISDISCONNECTED|SS_ISDISCONNECTING));
}

/*
 * Process an urgent data notification.
 */
static void
rx_urg_notify(struct toepcb *toep, struct mbuf *m)
{
	struct cpl_rx_urg_notify *hdr = cplhdr(m);
	struct socket *so = inp_inpcbtosocket(toep->tp_tp->t_inpcb);

	VALIDATE_SOCK(so);

	if (!so_no_receive(so))
		handle_urg_ptr(so, ntohl(hdr->seq));

	m_freem(m);
}

/*
 * Handler for RX_URG_NOTIFY CPL messages.
 */
static int
do_rx_urg_notify(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;

	rx_urg_notify(toep, m);
	return (0);
}

static __inline int
is_delack_mode_valid(struct toedev *dev, struct toepcb *toep)
{
	return (toep->tp_ulp_mode ||
		(toep->tp_ulp_mode == ULP_MODE_TCPDDP &&
		    dev->tod_ttid >= TOE_ID_CHELSIO_T3));
}

/*
 * Set of states for which we should return RX credits.
 */
#define CREDIT_RETURN_STATE (TCPF_ESTABLISHED | TCPF_FIN_WAIT1 | TCPF_FIN_WAIT2)

/*
 * Called after some received data has been read.  It returns RX credits
 * to the HW for the amount of data processed.
 */
void
t3_cleanup_rbuf(struct tcpcb *tp, int copied)
{
	struct toepcb *toep = tp->t_toe;
	struct socket *so;
	struct toedev *dev;
	int dack_mode, must_send, read;
	u32 thres, credits, dack = 0;
	struct sockbuf *rcv;
	
	so = inp_inpcbtosocket(tp->t_inpcb);
	rcv = so_sockbuf_rcv(so);

	if (!((tp->t_state == TCPS_ESTABLISHED) || (tp->t_state == TCPS_FIN_WAIT_1) ||
		(tp->t_state == TCPS_FIN_WAIT_2))) {
		if (copied) {
			sockbuf_lock(rcv);
			toep->tp_copied_seq += copied;
			sockbuf_unlock(rcv);
		}
		
		return;
	}
	
	inp_lock_assert(tp->t_inpcb); 

	sockbuf_lock(rcv);
	if (copied)
		toep->tp_copied_seq += copied;
	else {
		read = toep->tp_enqueued_bytes - rcv->sb_cc;
		toep->tp_copied_seq += read;
	}
	credits = toep->tp_copied_seq - toep->tp_rcv_wup;
	toep->tp_enqueued_bytes = rcv->sb_cc;
	sockbuf_unlock(rcv);

	if (credits > rcv->sb_mbmax) {
		log(LOG_ERR, "copied_seq=%u rcv_wup=%u credits=%u\n",
		    toep->tp_copied_seq, toep->tp_rcv_wup, credits);
	    credits = rcv->sb_mbmax;
	}
	
	    
	/*
	 * XXX this won't accurately reflect credit return - we need
	 * to look at the difference between the amount that has been 
	 * put in the recv sockbuf and what is there now
	 */

	if (__predict_false(!credits))
		return;

	dev = toep->tp_toedev;
	thres = TOM_TUNABLE(dev, rx_credit_thres);

	if (__predict_false(thres == 0))
		return;

	if (is_delack_mode_valid(dev, toep)) {
		dack_mode = TOM_TUNABLE(dev, delack);
		if (__predict_false(dack_mode != toep->tp_delack_mode)) {
			u32 r = tp->rcv_nxt - toep->tp_delack_seq;

			if (r >= tp->rcv_wnd || r >= 16 * toep->tp_mss_clamp)
				dack = F_RX_DACK_CHANGE |
				       V_RX_DACK_MODE(dack_mode);
		}
	} else 
		dack = F_RX_DACK_CHANGE | V_RX_DACK_MODE(1);
		
	/*
	 * For coalescing to work effectively ensure the receive window has
	 * at least 16KB left.
	 */
	must_send = credits + 16384 >= tp->rcv_wnd;

	if (must_send || credits >= thres)
		toep->tp_rcv_wup += t3_send_rx_credits(tp, credits, dack, must_send);
}

static int
cxgb_toe_disconnect(struct tcpcb *tp)
{
	struct socket *so;
	
	DPRINTF("cxgb_toe_disconnect\n");

	so = inp_inpcbtosocket(tp->t_inpcb);
	close_conn(so);
	return (0);
}

static int
cxgb_toe_reset(struct tcpcb *tp)
{
	struct toepcb *toep = tp->t_toe;

	t3_send_reset(toep);

	/*
	 * unhook from socket
	 */
	tp->t_flags &= ~TF_TOE;
	toep->tp_tp = NULL;
	tp->t_toe = NULL;
	return (0);
}

static int
cxgb_toe_send(struct tcpcb *tp)
{
	struct socket *so;
	
	DPRINTF("cxgb_toe_send\n");
	dump_toepcb(tp->t_toe);

	so = inp_inpcbtosocket(tp->t_inpcb);
	t3_push_frames(so, 1);
	return (0);
}

static int
cxgb_toe_rcvd(struct tcpcb *tp)
{

	inp_lock_assert(tp->t_inpcb);

	t3_cleanup_rbuf(tp, 0);
	
	return (0);
}

static void
cxgb_toe_detach(struct tcpcb *tp)
{
	struct toepcb *toep;

        /*
	 * XXX how do we handle teardown in the SYN_SENT state?
	 *
	 */
	inp_lock_assert(tp->t_inpcb);
	toep = tp->t_toe;
	toep->tp_tp = NULL;

	/*
	 * unhook from socket
	 */
	tp->t_flags &= ~TF_TOE;
	tp->t_toe = NULL;
}
	

static struct toe_usrreqs cxgb_toe_usrreqs = {
	.tu_disconnect = cxgb_toe_disconnect,
	.tu_reset = cxgb_toe_reset,
	.tu_send = cxgb_toe_send,
	.tu_rcvd = cxgb_toe_rcvd,
	.tu_detach = cxgb_toe_detach,
	.tu_detach = cxgb_toe_detach,
	.tu_syncache_event = handle_syncache_event,
};


static void
__set_tcb_field(struct toepcb *toep, struct mbuf *m, uint16_t word,
			    uint64_t mask, uint64_t val, int no_reply)
{
	struct cpl_set_tcb_field *req;

	CTR4(KTR_TCB, "__set_tcb_field_ulp(tid=%u word=0x%x mask=%jx val=%jx",
	    toep->tp_tid, word, mask, val);

	req = mtod(m, struct cpl_set_tcb_field *);
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = 0;
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SET_TCB_FIELD, toep->tp_tid));
	req->reply = V_NO_REPLY(no_reply);
	req->cpu_idx = 0;
	req->word = htons(word);
	req->mask = htobe64(mask);
	req->val = htobe64(val);

	m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
	send_or_defer(toep, m, 0);
}

static void
t3_set_tcb_field(struct toepcb *toep, uint16_t word, uint64_t mask, uint64_t val)
{
	struct mbuf *m;
	struct tcpcb *tp = toep->tp_tp;
	
	if (toep == NULL)
		return;
 
	if (tp->t_state == TCPS_CLOSED || (toep->tp_flags & TP_ABORT_SHUTDOWN)) {
		printf("not seting field\n");
		return;
	}
	
	m = m_gethdr_nofail(sizeof(struct cpl_set_tcb_field));

	__set_tcb_field(toep, m, word, mask, val, 1);
}

/*
 * Set one of the t_flags bits in the TCB.
 */
static void
set_tcb_tflag(struct toepcb *toep, unsigned int bit_pos, int val)
{

	t3_set_tcb_field(toep, W_TCB_T_FLAGS1, 1ULL << bit_pos, val << bit_pos);
}

/*
 * Send a SET_TCB_FIELD CPL message to change a connection's Nagle setting.
 */
static void
t3_set_nagle(struct toepcb *toep)
{
	struct tcpcb *tp = toep->tp_tp;
	
	set_tcb_tflag(toep, S_TF_NAGLE, !(tp->t_flags & TF_NODELAY));
}

/*
 * Send a SET_TCB_FIELD CPL message to change a connection's keepalive setting.
 */
void
t3_set_keepalive(struct toepcb *toep, int on_off)
{

	set_tcb_tflag(toep, S_TF_KEEPALIVE, on_off);
}

void
t3_set_rcv_coalesce_enable(struct toepcb *toep, int on_off)
{
	set_tcb_tflag(toep, S_TF_RCV_COALESCE_ENABLE, on_off);
}

void
t3_set_dack_mss(struct toepcb *toep, int on_off)
{

	set_tcb_tflag(toep, S_TF_DACK_MSS, on_off);
}

/*
 * Send a SET_TCB_FIELD CPL message to change a connection's TOS setting.
 */
static void
t3_set_tos(struct toepcb *toep)
{
	int tos = inp_ip_tos_get(toep->tp_tp->t_inpcb);	
	
	t3_set_tcb_field(toep, W_TCB_TOS, V_TCB_TOS(M_TCB_TOS),
			 V_TCB_TOS(tos));
}


/*
 * In DDP mode, TP fails to schedule a timer to push RX data to the host when
 * DDP is disabled (data is delivered to freelist). [Note that, the peer should
 * set the PSH bit in the last segment, which would trigger delivery.]
 * We work around the issue by setting a DDP buffer in a partial placed state,
 * which guarantees that TP will schedule a timer.
 */
#define TP_DDP_TIMER_WORKAROUND_MASK\
    (V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_ACTIVE_BUF(1) |\
     ((V_TCB_RX_DDP_BUF0_OFFSET(M_TCB_RX_DDP_BUF0_OFFSET) |\
       V_TCB_RX_DDP_BUF0_LEN(3)) << 32))
#define TP_DDP_TIMER_WORKAROUND_VAL\
    (V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_ACTIVE_BUF(0) |\
     ((V_TCB_RX_DDP_BUF0_OFFSET((uint64_t)1) | V_TCB_RX_DDP_BUF0_LEN((uint64_t)2)) <<\
      32))

static void
t3_enable_ddp(struct toepcb *toep, int on)
{
	if (on) {
		
		t3_set_tcb_field(toep, W_TCB_RX_DDP_FLAGS, V_TF_DDP_OFF(1),
				 V_TF_DDP_OFF(0));
	} else
		t3_set_tcb_field(toep, W_TCB_RX_DDP_FLAGS,
				 V_TF_DDP_OFF(1) |
				 TP_DDP_TIMER_WORKAROUND_MASK,
				 V_TF_DDP_OFF(1) |
				 TP_DDP_TIMER_WORKAROUND_VAL);

}

void
t3_set_ddp_tag(struct toepcb *toep, int buf_idx, unsigned int tag_color)
{
	t3_set_tcb_field(toep, W_TCB_RX_DDP_BUF0_TAG + buf_idx,
			 V_TCB_RX_DDP_BUF0_TAG(M_TCB_RX_DDP_BUF0_TAG),
			 tag_color);
}

void
t3_set_ddp_buf(struct toepcb *toep, int buf_idx, unsigned int offset,
		    unsigned int len)
{
	if (buf_idx == 0)
		t3_set_tcb_field(toep, W_TCB_RX_DDP_BUF0_OFFSET,
			 V_TCB_RX_DDP_BUF0_OFFSET(M_TCB_RX_DDP_BUF0_OFFSET) |
			 V_TCB_RX_DDP_BUF0_LEN(M_TCB_RX_DDP_BUF0_LEN),
			 V_TCB_RX_DDP_BUF0_OFFSET((uint64_t)offset) |
			 V_TCB_RX_DDP_BUF0_LEN((uint64_t)len));
	else
		t3_set_tcb_field(toep, W_TCB_RX_DDP_BUF1_OFFSET,
			 V_TCB_RX_DDP_BUF1_OFFSET(M_TCB_RX_DDP_BUF1_OFFSET) |
			 V_TCB_RX_DDP_BUF1_LEN(M_TCB_RX_DDP_BUF1_LEN << 32),
			 V_TCB_RX_DDP_BUF1_OFFSET((uint64_t)offset) |
			 V_TCB_RX_DDP_BUF1_LEN(((uint64_t)len) << 32));
}

static int
t3_set_cong_control(struct socket *so, const char *name)
{
#ifdef CONGESTION_CONTROL_SUPPORTED	
	int cong_algo;

	for (cong_algo = 0; cong_algo < ARRAY_SIZE(t3_cong_ops); cong_algo++)
		if (!strcmp(name, t3_cong_ops[cong_algo].name))
			break;

	if (cong_algo >= ARRAY_SIZE(t3_cong_ops))
		return -EINVAL;
#endif
	return 0;
}

int
t3_get_tcb(struct toepcb *toep)
{
	struct cpl_get_tcb *req;
	struct tcpcb *tp = toep->tp_tp;
	struct mbuf *m = m_gethdr(M_NOWAIT, MT_DATA);

	if (!m)
		return (ENOMEM);
	
	inp_lock_assert(tp->t_inpcb);	
	m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
	req = mtod(m, struct cpl_get_tcb *);
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = 0;
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_GET_TCB, toep->tp_tid));
	req->cpuno = htons(toep->tp_qset);
	req->rsvd = 0;
	if (tp->t_state == TCPS_SYN_SENT)
		mbufq_tail(&toep->out_of_order_queue, m);	// defer
	else
		cxgb_ofld_send(TOEP_T3C_DEV(toep), m);
	return 0;
}

static inline void
so_insert_tid(struct tom_data *d, struct toepcb *toep, unsigned int tid)
{

	toepcb_hold(toep);

	cxgb_insert_tid(d->cdev, d->client, toep, tid);
}

/**
 *	find_best_mtu - find the entry in the MTU table closest to an MTU
 *	@d: TOM state
 *	@mtu: the target MTU
 *
 *	Returns the index of the value in the MTU table that is closest to but
 *	does not exceed the target MTU.
 */
static unsigned int
find_best_mtu(const struct t3c_data *d, unsigned short mtu)
{
	int i = 0;

	while (i < d->nmtus - 1 && d->mtus[i + 1] <= mtu)
		++i;
	return (i);
}

static unsigned int
select_mss(struct t3c_data *td, struct tcpcb *tp, unsigned int pmtu)
{
	unsigned int idx;
	
#ifdef notyet
	struct rtentry *dst = so_sotoinpcb(so)->inp_route.ro_rt;
#endif
	if (tp) {
		tp->t_maxseg = pmtu - 40;
		if (tp->t_maxseg < td->mtus[0] - 40)
			tp->t_maxseg = td->mtus[0] - 40;
		idx = find_best_mtu(td, tp->t_maxseg + 40);

		tp->t_maxseg = td->mtus[idx] - 40;
	} else
		idx = find_best_mtu(td, pmtu);
	
	return (idx);
}

static inline void
free_atid(struct t3cdev *cdev, unsigned int tid)
{
	struct toepcb *toep = cxgb_free_atid(cdev, tid);

	if (toep)
		toepcb_release(toep);
}

/*
 * Release resources held by an offload connection (TID, L2T entry, etc.)
 */
static void
t3_release_offload_resources(struct toepcb *toep)
{
	struct tcpcb *tp = toep->tp_tp;
	struct toedev *tdev = toep->tp_toedev;
	struct t3cdev *cdev;
	struct socket *so;
	unsigned int tid = toep->tp_tid;
	struct sockbuf *rcv;
	
	CTR0(KTR_TOM, "t3_release_offload_resources");

	if (!tdev)
		return;

	cdev = TOEP_T3C_DEV(toep);
	if (!cdev)
		return;

	toep->tp_qset = 0;
	t3_release_ddp_resources(toep);

#ifdef CTRL_SKB_CACHE
	kfree_skb(CTRL_SKB_CACHE(tp));
	CTRL_SKB_CACHE(tp) = NULL;
#endif

	if (toep->tp_wr_avail != toep->tp_wr_max) {
		purge_wr_queue(toep);
		reset_wr_list(toep);
	}

	if (toep->tp_l2t) {
		l2t_release(L2DATA(cdev), toep->tp_l2t);
		toep->tp_l2t = NULL;
	}
	toep->tp_tp = NULL;
	if (tp) {
		inp_lock_assert(tp->t_inpcb);
		so = inp_inpcbtosocket(tp->t_inpcb);
		rcv = so_sockbuf_rcv(so);		
		/*
		 * cancel any offloaded reads
		 *
		 */
		sockbuf_lock(rcv);
		tp->t_toe = NULL;
		tp->t_flags &= ~TF_TOE;
		if (toep->tp_ddp_state.user_ddp_pending) {
			t3_cancel_ubuf(toep, rcv);
			toep->tp_ddp_state.user_ddp_pending = 0;
		}
		so_sorwakeup_locked(so);
			
	}
	
	if (toep->tp_state == TCPS_SYN_SENT) {
		free_atid(cdev, tid);
#ifdef notyet		
		__skb_queue_purge(&tp->out_of_order_queue);
#endif		
	} else {                                          // we have TID
		cxgb_remove_tid(cdev, toep, tid);
		toepcb_release(toep);
	}
#if 0
	log(LOG_INFO, "closing TID %u, state %u\n", tid, tp->t_state);
#endif
}

static void
install_offload_ops(struct socket *so)
{
	struct tcpcb *tp = so_sototcpcb(so);

	KASSERT(tp->t_toe != NULL, ("toepcb not set"));
	
	t3_install_socket_ops(so);
	tp->t_flags |= TF_TOE;
	tp->t_tu = &cxgb_toe_usrreqs;
}

/*
 * Determine the receive window scaling factor given a target max
 * receive window.
 */
static __inline int
select_rcv_wscale(int space)
{
	INIT_VNET_INET(so->so_vnet);
	int wscale = 0;

	if (space > MAX_RCV_WND)
		space = MAX_RCV_WND;

	if (V_tcp_do_rfc1323)
		for (; space > 65535 && wscale < 14; space >>= 1, ++wscale) ;

	return (wscale);
}

/*
 * Determine the receive window size for a socket.
 */
static unsigned long
select_rcv_wnd(struct toedev *dev, struct socket *so)
{
	INIT_VNET_INET(so->so_vnet);
	struct tom_data *d = TOM_DATA(dev);
	unsigned int wnd;
	unsigned int max_rcv_wnd;
	struct sockbuf *rcv;

	rcv = so_sockbuf_rcv(so);
	
	if (V_tcp_do_autorcvbuf)
		wnd = V_tcp_autorcvbuf_max;
	else
		wnd = rcv->sb_hiwat;

	
	
	/* XXX
	 * For receive coalescing to work effectively we need a receive window
	 * that can accomodate a coalesced segment.
	 */	
	if (wnd < MIN_RCV_WND)
		wnd = MIN_RCV_WND; 
	
	/* PR 5138 */
	max_rcv_wnd = (dev->tod_ttid < TOE_ID_CHELSIO_T3C ? 
				    (uint32_t)d->rx_page_size * 23 :
				    MAX_RCV_WND);
	
	return min(wnd, max_rcv_wnd);
}

/*
 * Assign offload parameters to some socket fields.  This code is used by
 * both active and passive opens.
 */
static inline void
init_offload_socket(struct socket *so, struct toedev *dev, unsigned int tid,
    struct l2t_entry *e, struct rtentry *dst, struct toepcb *toep)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct t3c_data *td = T3C_DATA(TOM_DATA(dev)->cdev);
	struct sockbuf *snd, *rcv;
	
#ifdef notyet	
	SOCK_LOCK_ASSERT(so);
#endif
	
	snd = so_sockbuf_snd(so);
	rcv = so_sockbuf_rcv(so);
	
	log(LOG_INFO, "initializing offload socket\n");
	/*
	 * We either need to fix push frames to work with sbcompress
	 * or we need to add this
	 */
	snd->sb_flags |= SB_NOCOALESCE;
	rcv->sb_flags |= SB_NOCOALESCE;
	
	tp->t_toe = toep;
	toep->tp_tp = tp;
	toep->tp_toedev = dev;
	
	toep->tp_tid = tid;
	toep->tp_l2t = e;
	toep->tp_wr_max = toep->tp_wr_avail = TOM_TUNABLE(dev, max_wrs);
	toep->tp_wr_unacked = 0;
	toep->tp_delack_mode = 0;
	
	toep->tp_mtu_idx = select_mss(td, tp, dst->rt_ifp->if_mtu);
	/*
	 * XXX broken
	 * 
	 */
	tp->rcv_wnd = select_rcv_wnd(dev, so);

        toep->tp_ulp_mode = TOM_TUNABLE(dev, ddp) && !(so_options_get(so) & SO_NO_DDP) &&
		       tp->rcv_wnd >= MIN_DDP_RCV_WIN ? ULP_MODE_TCPDDP : 0;
	toep->tp_qset_idx = 0;
	
	reset_wr_list(toep);
	DPRINTF("initialization done\n");
}

/*
 * The next two functions calculate the option 0 value for a socket.
 */
static inline unsigned int
calc_opt0h(struct socket *so, int mtu_idx)
{
	struct tcpcb *tp = so_sototcpcb(so);
	int wscale = select_rcv_wscale(tp->rcv_wnd);
	
	return V_NAGLE((tp->t_flags & TF_NODELAY) == 0) |
	    V_KEEP_ALIVE((so_options_get(so) & SO_KEEPALIVE) != 0) | F_TCAM_BYPASS |
	    V_WND_SCALE(wscale) | V_MSS_IDX(mtu_idx);
}

static inline unsigned int
calc_opt0l(struct socket *so, int ulp_mode)
{
	struct tcpcb *tp = so_sototcpcb(so);
	unsigned int val;
	
	val = V_TOS(INP_TOS(tp->t_inpcb)) | V_ULP_MODE(ulp_mode) |
	       V_RCV_BUFSIZ(min(tp->rcv_wnd >> 10, (u32)M_RCV_BUFSIZ));

	DPRINTF("opt0l tos=%08x rcv_wnd=%ld opt0l=%08x\n", INP_TOS(tp->t_inpcb), tp->rcv_wnd, val);
	return (val);
}

static inline unsigned int
calc_opt2(const struct socket *so, struct toedev *dev)
{
	int flv_valid;

	flv_valid = (TOM_TUNABLE(dev, cong_alg) != -1);

	return (V_FLAVORS_VALID(flv_valid) |
	    V_CONG_CONTROL_FLAVOR(flv_valid ? TOM_TUNABLE(dev, cong_alg) : 0));
}

#if DEBUG_WR > 1
static int
count_pending_wrs(const struct toepcb *toep)
{
	const struct mbuf *m;
	int n = 0;

	wr_queue_walk(toep, m)
		n += m->m_pkthdr.csum_data;
	return (n);
}
#endif

#if 0
(((*(struct tom_data **)&(dev)->l4opt)->conf.cong_alg) != -1)
#endif
	
static void
mk_act_open_req(struct socket *so, struct mbuf *m,
    unsigned int atid, const struct l2t_entry *e)
{
	struct cpl_act_open_req *req;
	struct inpcb *inp = so_sotoinpcb(so);
	struct tcpcb *tp = inp_inpcbtotcpcb(inp);
	struct toepcb *toep = tp->t_toe;
	struct toedev *tdev = toep->tp_toedev;
	
	m_set_priority((struct mbuf *)m, mkprio(CPL_PRIORITY_SETUP, toep));
	
	req = mtod(m, struct cpl_act_open_req *);
	m->m_pkthdr.len = m->m_len = sizeof(*req);

	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	req->wr.wr_lo = 0;
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_ACT_OPEN_REQ, atid));
	inp_4tuple_get(inp, &req->local_ip, &req->local_port, &req->peer_ip, &req->peer_port);
#if 0	
	req->local_port = inp->inp_lport;
	req->peer_port = inp->inp_fport;
	memcpy(&req->local_ip, &inp->inp_laddr, 4);
	memcpy(&req->peer_ip, &inp->inp_faddr, 4);
#endif	
	req->opt0h = htonl(calc_opt0h(so, toep->tp_mtu_idx) | V_L2T_IDX(e->idx) |
			   V_TX_CHANNEL(e->smt_idx));
	req->opt0l = htonl(calc_opt0l(so, toep->tp_ulp_mode));
	req->params = 0;
	req->opt2 = htonl(calc_opt2(so, tdev));
}


/*
 * Convert an ACT_OPEN_RPL status to an errno.
 */
static int
act_open_rpl_status_to_errno(int status)
{
	switch (status) {
	case CPL_ERR_CONN_RESET:
		return (ECONNREFUSED);
	case CPL_ERR_ARP_MISS:
		return (EHOSTUNREACH);
	case CPL_ERR_CONN_TIMEDOUT:
		return (ETIMEDOUT);
	case CPL_ERR_TCAM_FULL:
		return (ENOMEM);
	case CPL_ERR_CONN_EXIST:
		log(LOG_ERR, "ACTIVE_OPEN_RPL: 4-tuple in use\n");
		return (EADDRINUSE);
	default:
		return (EIO);
	}
}

static void
fail_act_open(struct toepcb *toep, int errno)
{
	struct tcpcb *tp = toep->tp_tp;

	t3_release_offload_resources(toep);
	if (tp) {
		inp_wunlock(tp->t_inpcb);		
		tcp_offload_drop(tp, errno);
	}
	
#ifdef notyet
	TCP_INC_STATS_BH(TCP_MIB_ATTEMPTFAILS);
#endif
}

/*
 * Handle active open failures.
 */
static void
active_open_failed(struct toepcb *toep, struct mbuf *m)
{
	struct cpl_act_open_rpl *rpl = cplhdr(m);
	struct inpcb *inp;

	if (toep->tp_tp == NULL)
		goto done;

	inp = toep->tp_tp->t_inpcb;

/*
 * Don't handle connection retry for now
 */
#ifdef notyet
	struct inet_connection_sock *icsk = inet_csk(sk);

	if (rpl->status == CPL_ERR_CONN_EXIST &&
	    icsk->icsk_retransmit_timer.function != act_open_retry_timer) {
		icsk->icsk_retransmit_timer.function = act_open_retry_timer;
		sk_reset_timer(so, &icsk->icsk_retransmit_timer,
			       jiffies + HZ / 2);
	} else
#endif
	{
		inp_wlock(inp);
		/*
		 * drops the inpcb lock
		 */
		fail_act_open(toep, act_open_rpl_status_to_errno(rpl->status));
	}
	
	done:
	m_free(m);
}

/*
 * Return whether a failed active open has allocated a TID
 */
static inline int
act_open_has_tid(int status)
{
	return status != CPL_ERR_TCAM_FULL && status != CPL_ERR_CONN_EXIST &&
	       status != CPL_ERR_ARP_MISS;
}

/*
 * Process an ACT_OPEN_RPL CPL message.
 */
static int
do_act_open_rpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;
	struct cpl_act_open_rpl *rpl = cplhdr(m);
	
	if (cdev->type != T3A && act_open_has_tid(rpl->status))
		cxgb_queue_tid_release(cdev, GET_TID(rpl));
	
	active_open_failed(toep, m);
	return (0);
}

/*
 * Handle an ARP failure for an active open.   XXX purge ofo queue
 *
 * XXX badly broken for crossed SYNs as the ATID is no longer valid.
 * XXX crossed SYN errors should be generated by PASS_ACCEPT_RPL which should
 * check SOCK_DEAD or sk->sk_sock.  Or maybe generate the error here but don't
 * free the atid.  Hmm.
 */
#ifdef notyet
static void
act_open_req_arp_failure(struct t3cdev *dev, struct mbuf *m)
{
	struct toepcb *toep = m_get_toep(m);
	struct tcpcb *tp = toep->tp_tp;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so;
	
	inp_wlock(inp);
	if (tp->t_state == TCPS_SYN_SENT || tp->t_state == TCPS_SYN_RECEIVED) {
		/*
		 * drops the inpcb lock
		 */
		fail_act_open(so, EHOSTUNREACH);
		printf("freeing %p\n", m);
		
		m_free(m);
	} else
		inp_wunlock(inp);
}
#endif
/*
 * Send an active open request.
 */
int
t3_connect(struct toedev *tdev, struct socket *so,
    struct rtentry *rt, struct sockaddr *nam)
{
	struct mbuf *m;
	struct l2t_entry *e;
	struct tom_data *d = TOM_DATA(tdev);
	struct inpcb *inp = so_sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct toepcb *toep; /* allocated by init_offload_socket */
		
	int atid;

	toep = toepcb_alloc();
	if (toep == NULL)
		goto out_err;
	
	if ((atid = cxgb_alloc_atid(d->cdev, d->client, toep)) < 0)
		goto out_err;
	
	e = t3_l2t_get(d->cdev, rt, rt->rt_ifp, nam);
	if (!e)
		goto free_tid;

	inp_lock_assert(inp);
	m = m_gethdr(MT_DATA, M_WAITOK);
	
#if 0	
	m->m_toe.mt_toepcb = tp->t_toe;
	set_arp_failure_handler((struct mbuf *)m, act_open_req_arp_failure);
#endif
	so_lock(so);
	
	init_offload_socket(so, tdev, atid, e, rt, toep);
	
	install_offload_ops(so);
	
	mk_act_open_req(so, m, atid, e);
	so_unlock(so);
	
	soisconnecting(so);
	toep = tp->t_toe;
	m_set_toep(m, tp->t_toe);
	
	toep->tp_state = TCPS_SYN_SENT;
	l2t_send(d->cdev, (struct mbuf *)m, e);

	if (toep->tp_ulp_mode)
		t3_enable_ddp(toep, 0);
	return 	(0);
	
free_tid:
	printf("failing connect - free atid\n");
	
	free_atid(d->cdev, atid);
out_err:
	printf("return ENOMEM\n");
       return (ENOMEM);
}

/*
 * Send an ABORT_REQ message.  Cannot fail.  This routine makes sure we do
 * not send multiple ABORT_REQs for the same connection and also that we do
 * not try to send a message after the connection has closed.  Returns 1 if
 * an ABORT_REQ wasn't generated after all, 0 otherwise.
 */
static void
t3_send_reset(struct toepcb *toep)
{
	
	struct cpl_abort_req *req;
	unsigned int tid = toep->tp_tid;
	int mode = CPL_ABORT_SEND_RST;
	struct tcpcb *tp = toep->tp_tp;
	struct toedev *tdev = toep->tp_toedev;
	struct socket *so = NULL;
	struct mbuf *m;
	struct sockbuf *snd;
	
	if (tp) {
		inp_lock_assert(tp->t_inpcb);
		so = inp_inpcbtosocket(tp->t_inpcb);
	}
	
	if (__predict_false((toep->tp_flags & TP_ABORT_SHUTDOWN) ||
		tdev == NULL))
		return;
	toep->tp_flags |= (TP_ABORT_RPL_PENDING|TP_ABORT_SHUTDOWN);

	snd = so_sockbuf_snd(so);
	/* Purge the send queue so we don't send anything after an abort. */
	if (so)
		sbflush(snd);
	if ((toep->tp_flags & TP_CLOSE_CON_REQUESTED) && is_t3a(tdev))
		mode |= CPL_ABORT_POST_CLOSE_REQ;

	m = m_gethdr_nofail(sizeof(*req));
	m_set_priority(m, mkprio(CPL_PRIORITY_DATA, toep));
	set_arp_failure_handler(m, abort_arp_failure);

	req = mtod(m, struct cpl_abort_req *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_HOST_ABORT_CON_REQ));
	req->wr.wr_lo = htonl(V_WR_TID(tid));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_ABORT_REQ, tid));
	req->rsvd0 = tp ? htonl(tp->snd_nxt) : 0;
	req->rsvd1 = !(toep->tp_flags & TP_DATASENT);
	req->cmd = mode;
	if (tp && (tp->t_state == TCPS_SYN_SENT))
		mbufq_tail(&toep->out_of_order_queue, m);	// defer
	else
		l2t_send(TOEP_T3C_DEV(toep), m, toep->tp_l2t);
}

static int
t3_ip_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct inpcb *inp;
	int error, optval;
	
	if (sopt->sopt_name == IP_OPTIONS)
		return (ENOPROTOOPT);

	if (sopt->sopt_name != IP_TOS)
		return (EOPNOTSUPP);
	
	error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);

	if (error)
		return (error);

	if (optval > IPTOS_PREC_CRITIC_ECP)
		return (EINVAL);

	inp = so_sotoinpcb(so);
	inp_wlock(inp);
	inp_ip_tos_set(inp, optval);
#if 0	
	inp->inp_ip_tos = optval;
#endif
	t3_set_tos(inp_inpcbtotcpcb(inp)->t_toe);
	inp_wunlock(inp);

	return (0);
}

static int
t3_tcp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int err = 0;
	size_t copied;

	if (sopt->sopt_name != TCP_CONGESTION &&
	    sopt->sopt_name != TCP_NODELAY)
		return (EOPNOTSUPP);

	if (sopt->sopt_name == TCP_CONGESTION) {
		char name[TCP_CA_NAME_MAX];
		int optlen = sopt->sopt_valsize;
		struct tcpcb *tp;
		
		if (sopt->sopt_dir == SOPT_GET) {
			KASSERT(0, ("unimplemented"));
			return (EOPNOTSUPP);
		}

		if (optlen < 1)
			return (EINVAL);
		
		err = copyinstr(sopt->sopt_val, name, 
		    min(TCP_CA_NAME_MAX - 1, optlen), &copied);
		if (err)
			return (err);
		if (copied < 1)
			return (EINVAL);

		tp = so_sototcpcb(so);
		/*
		 * XXX I need to revisit this
		 */
		if ((err = t3_set_cong_control(so, name)) == 0) {
#ifdef CONGESTION_CONTROL_SUPPORTED
			tp->t_cong_control = strdup(name, M_CXGB);
#endif			
		} else
			return (err);
	} else {
		int optval, oldval;
		struct inpcb *inp;
		struct tcpcb *tp;

		if (sopt->sopt_dir == SOPT_GET)
			return (EOPNOTSUPP);
	
		err = sooptcopyin(sopt, &optval, sizeof optval,
		    sizeof optval);

		if (err)
			return (err);

		inp = so_sotoinpcb(so);
		inp_wlock(inp);
		tp = inp_inpcbtotcpcb(inp);

		oldval = tp->t_flags;
		if (optval)
			tp->t_flags |= TF_NODELAY;
		else
			tp->t_flags &= ~TF_NODELAY;
		inp_wunlock(inp);


		if (oldval != tp->t_flags && (tp->t_toe != NULL))
			t3_set_nagle(tp->t_toe);

	}

	return (0);
}

int
t3_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int err;

	if (sopt->sopt_level != IPPROTO_TCP) 
		err =  t3_ip_ctloutput(so, sopt);
	else
		err = t3_tcp_ctloutput(so, sopt);

	if (err != EOPNOTSUPP)
		return (err);

	return (tcp_ctloutput(so, sopt));
}

/*
 * Returns true if we need to explicitly request RST when we receive new data
 * on an RX-closed connection.
 */
static inline int
need_rst_on_excess_rx(const struct toepcb *toep)
{
	return (1);
}

/*
 * Handles Rx data that arrives in a state where the socket isn't accepting
 * new data.
 */
static void
handle_excess_rx(struct toepcb *toep, struct mbuf *m)
{
	
	if (need_rst_on_excess_rx(toep) &&
	    !(toep->tp_flags & TP_ABORT_SHUTDOWN))
		t3_send_reset(toep);
	m_freem(m); 
}

/*
 * Process a get_tcb_rpl as a DDP completion (similar to RX_DDP_COMPLETE)
 * by getting the DDP offset from the TCB.
 */
static void
tcb_rpl_as_ddp_complete(struct toepcb *toep, struct mbuf *m)
{
	struct ddp_state *q = &toep->tp_ddp_state;
	struct ddp_buf_state *bsp;
	struct cpl_get_tcb_rpl *hdr;
	unsigned int ddp_offset;
	struct socket *so;
	struct tcpcb *tp;
	struct sockbuf *rcv;	
	int state;
	
	uint64_t t;
	__be64 *tcb;

	tp = toep->tp_tp;
	so = inp_inpcbtosocket(tp->t_inpcb);

	inp_lock_assert(tp->t_inpcb);
	rcv = so_sockbuf_rcv(so);
	sockbuf_lock(rcv);	
	
	/* Note that we only accout for CPL_GET_TCB issued by the DDP code.
	 * We really need a cookie in order to dispatch the RPLs.
	 */
	q->get_tcb_count--;

	/* It is a possible that a previous CPL already invalidated UBUF DDP
	 * and moved the cur_buf idx and hence no further processing of this
	 * skb is required. However, the app might be sleeping on
	 * !q->get_tcb_count and we need to wake it up.
	 */
	if (q->cancel_ubuf && !t3_ddp_ubuf_pending(toep)) {
		int state = so_state_get(so);

		m_freem(m);
		if (__predict_true((state & SS_NOFDREF) == 0))
			so_sorwakeup_locked(so);
		else
			sockbuf_unlock(rcv);

		return;
	}

	bsp = &q->buf_state[q->cur_buf];
	hdr = cplhdr(m);
	tcb = (__be64 *)(hdr + 1);
	if (q->cur_buf == 0) {
		t = be64toh(tcb[(31 - W_TCB_RX_DDP_BUF0_OFFSET) / 2]);
		ddp_offset = t >> (32 + S_TCB_RX_DDP_BUF0_OFFSET);
	} else {
		t = be64toh(tcb[(31 - W_TCB_RX_DDP_BUF1_OFFSET) / 2]);
		ddp_offset = t >> S_TCB_RX_DDP_BUF1_OFFSET;
	}
	ddp_offset &= M_TCB_RX_DDP_BUF0_OFFSET;
	m->m_cur_offset = bsp->cur_offset;
	bsp->cur_offset = ddp_offset;
	m->m_len = m->m_pkthdr.len = ddp_offset - m->m_cur_offset;

	CTR5(KTR_TOM,
	    "tcb_rpl_as_ddp_complete: idx=%d seq=0x%x hwbuf=%u ddp_offset=%u cur_offset=%u",
	    q->cur_buf, tp->rcv_nxt, q->cur_buf, ddp_offset, m->m_cur_offset);
	KASSERT(ddp_offset >= m->m_cur_offset,
	    ("ddp_offset=%u less than cur_offset=%u",
		ddp_offset, m->m_cur_offset));
	
#if 0
{
	unsigned int ddp_flags, rcv_nxt, rx_hdr_offset, buf_idx;

	t = be64toh(tcb[(31 - W_TCB_RX_DDP_FLAGS) / 2]);
	ddp_flags = (t >> S_TCB_RX_DDP_FLAGS) & M_TCB_RX_DDP_FLAGS;

        t = be64toh(tcb[(31 - W_TCB_RCV_NXT) / 2]);
        rcv_nxt = t >> S_TCB_RCV_NXT;
        rcv_nxt &= M_TCB_RCV_NXT;

        t = be64toh(tcb[(31 - W_TCB_RX_HDR_OFFSET) / 2]);
        rx_hdr_offset = t >> (32 + S_TCB_RX_HDR_OFFSET);
        rx_hdr_offset &= M_TCB_RX_HDR_OFFSET;

	T3_TRACE2(TIDTB(sk),
		  "tcb_rpl_as_ddp_complete: DDP FLAGS 0x%x dma up to 0x%x",
		  ddp_flags, rcv_nxt - rx_hdr_offset);
	T3_TRACE4(TB(q),
		  "tcb_rpl_as_ddp_complete: rcvnxt 0x%x hwbuf %u cur_offset %u cancel %u",
		  tp->rcv_nxt, q->cur_buf, bsp->cur_offset, q->cancel_ubuf);
	T3_TRACE3(TB(q),
		  "tcb_rpl_as_ddp_complete: TCB rcvnxt 0x%x hwbuf 0x%x ddp_offset %u",
		  rcv_nxt - rx_hdr_offset, ddp_flags, ddp_offset);
	T3_TRACE2(TB(q),
		  "tcb_rpl_as_ddp_complete: flags0 0x%x flags1 0x%x",
		 q->buf_state[0].flags, q->buf_state[1].flags);

}
#endif
	if (__predict_false(so_no_receive(so) && m->m_pkthdr.len)) {
		handle_excess_rx(toep, m);
		return;
	}

#ifdef T3_TRACE
	if ((int)m->m_pkthdr.len < 0) {
		t3_ddp_error(so, "tcb_rpl_as_ddp_complete: neg len");
	}
#endif
	if (bsp->flags & DDP_BF_NOCOPY) {
#ifdef T3_TRACE
		T3_TRACE0(TB(q),
			  "tcb_rpl_as_ddp_complete: CANCEL UBUF");

		if (!q->cancel_ubuf && !(sk->sk_shutdown & RCV_SHUTDOWN)) {
			printk("!cancel_ubuf");
			t3_ddp_error(sk, "tcb_rpl_as_ddp_complete: !cancel_ubuf");
		}
#endif
		m->m_ddp_flags = DDP_BF_PSH | DDP_BF_NOCOPY | 1;
		bsp->flags &= ~(DDP_BF_NOCOPY|DDP_BF_NODATA);
		q->cur_buf ^= 1;
	} else if (bsp->flags & DDP_BF_NOFLIP) {

		m->m_ddp_flags = 1;    /* always a kernel buffer */

		/* now HW buffer carries a user buffer */
		bsp->flags &= ~DDP_BF_NOFLIP;
		bsp->flags |= DDP_BF_NOCOPY;

		/* It is possible that the CPL_GET_TCB_RPL doesn't indicate
		 * any new data in which case we're done. If in addition the
		 * offset is 0, then there wasn't a completion for the kbuf
		 * and we need to decrement the posted count.
		 */
		if (m->m_pkthdr.len == 0) {
			if (ddp_offset == 0) {
				q->kbuf_posted--;
				bsp->flags |= DDP_BF_NODATA;
			}
			sockbuf_unlock(rcv);
			m_free(m);
			return;
		}
	} else {
		sockbuf_unlock(rcv);

		/* This reply is for a CPL_GET_TCB_RPL to cancel the UBUF DDP,
		 * but it got here way late and nobody cares anymore.
		 */
		m_free(m);
		return;
	}

	m->m_ddp_gl = (unsigned char *)bsp->gl;
	m->m_flags |= M_DDP;
	m->m_seq = tp->rcv_nxt;
	tp->rcv_nxt += m->m_pkthdr.len;
	tp->t_rcvtime = ticks;
	CTR3(KTR_TOM, "tcb_rpl_as_ddp_complete: seq 0x%x hwbuf %u m->m_pktlen %u",
		  m->m_seq, q->cur_buf, m->m_pkthdr.len);
	if (m->m_pkthdr.len == 0) {
		q->user_ddp_pending = 0;
		m_free(m);
	} else 
		SBAPPEND(rcv, m);

	state = so_state_get(so);	
	if (__predict_true((state & SS_NOFDREF) == 0))
		so_sorwakeup_locked(so);
	else
		sockbuf_unlock(rcv);
}

/*
 * Process a CPL_GET_TCB_RPL.  These can also be generated by the DDP code,
 * in that case they are similar to DDP completions.
 */
static int
do_get_tcb_rpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;

	/* OK if socket doesn't exist */
	if (toep == NULL) {
		printf("null toep in do_get_tcb_rpl\n");
		return (CPL_RET_BUF_DONE);
	}

	inp_wlock(toep->tp_tp->t_inpcb);
	tcb_rpl_as_ddp_complete(toep, m);
	inp_wunlock(toep->tp_tp->t_inpcb);
	
	return (0);
}

static void
handle_ddp_data(struct toepcb *toep, struct mbuf *m)
{
	struct tcpcb *tp = toep->tp_tp;
	struct socket *so;
	struct ddp_state *q;
	struct ddp_buf_state *bsp;
	struct cpl_rx_data *hdr = cplhdr(m);
	unsigned int rcv_nxt = ntohl(hdr->seq);
	struct sockbuf *rcv;	
	
	if (tp->rcv_nxt == rcv_nxt)
		return;

	inp_lock_assert(tp->t_inpcb);
	so  = inp_inpcbtosocket(tp->t_inpcb);
	rcv = so_sockbuf_rcv(so);	
	sockbuf_lock(rcv);	

	q = &toep->tp_ddp_state;
	bsp = &q->buf_state[q->cur_buf];
	KASSERT(SEQ_GT(rcv_nxt, tp->rcv_nxt), ("tp->rcv_nxt=0x%08x decreased rcv_nxt=0x08%x",
		rcv_nxt, tp->rcv_nxt));
	m->m_len = m->m_pkthdr.len = rcv_nxt - tp->rcv_nxt;
	KASSERT(m->m_len > 0, ("%s m_len=%d", __FUNCTION__, m->m_len));
	CTR3(KTR_TOM, "rcv_nxt=0x%x tp->rcv_nxt=0x%x len=%d",
	    rcv_nxt, tp->rcv_nxt, m->m_pkthdr.len);

#ifdef T3_TRACE
	if ((int)m->m_pkthdr.len < 0) {
		t3_ddp_error(so, "handle_ddp_data: neg len");
	}
#endif
	m->m_ddp_gl = (unsigned char *)bsp->gl;
	m->m_flags |= M_DDP;
	m->m_cur_offset = bsp->cur_offset;
	m->m_ddp_flags = DDP_BF_PSH | (bsp->flags & DDP_BF_NOCOPY) | 1;
	if (bsp->flags & DDP_BF_NOCOPY)
		bsp->flags &= ~DDP_BF_NOCOPY;

	m->m_seq = tp->rcv_nxt;
	tp->rcv_nxt = rcv_nxt;
	bsp->cur_offset += m->m_pkthdr.len;
	if (!(bsp->flags & DDP_BF_NOFLIP))
		q->cur_buf ^= 1;
	/*
	 * For now, don't re-enable DDP after a connection fell out of  DDP
	 * mode.
	 */
	q->ubuf_ddp_ready = 0;
	sockbuf_unlock(rcv);
}

/*
 * Process new data received for a connection.
 */
static void
new_rx_data(struct toepcb *toep, struct mbuf *m)
{
	struct cpl_rx_data *hdr = cplhdr(m);
	struct tcpcb *tp = toep->tp_tp;
	struct socket *so;
	struct sockbuf *rcv;	
	int state;
	int len = be16toh(hdr->len);

	inp_wlock(tp->t_inpcb);

	so  = inp_inpcbtosocket(tp->t_inpcb);
	
	if (__predict_false(so_no_receive(so))) {
		handle_excess_rx(toep, m);
		inp_wunlock(tp->t_inpcb);
		TRACE_EXIT;
		return;
	}

	if (toep->tp_ulp_mode == ULP_MODE_TCPDDP)
		handle_ddp_data(toep, m);
	
	m->m_seq = ntohl(hdr->seq);
	m->m_ulp_mode = 0;                    /* for iSCSI */

#if VALIDATE_SEQ
	if (__predict_false(m->m_seq != tp->rcv_nxt)) {
		log(LOG_ERR,
		       "%s: TID %u: Bad sequence number %u, expected %u\n",
		    toep->tp_toedev->name, toep->tp_tid, m->m_seq,
		       tp->rcv_nxt);
		m_freem(m);
		inp_wunlock(tp->t_inpcb);
		return;
	}
#endif
	m_adj(m, sizeof(*hdr));

#ifdef URGENT_DATA_SUPPORTED
	/*
	 * We don't handle urgent data yet
	 */
	if (__predict_false(hdr->urg))
		handle_urg_ptr(so, tp->rcv_nxt + ntohs(hdr->urg));
	if (__predict_false(tp->urg_data == TCP_URG_NOTYET &&
		     tp->urg_seq - tp->rcv_nxt < skb->len))
		tp->urg_data = TCP_URG_VALID | skb->data[tp->urg_seq -
							 tp->rcv_nxt];
#endif	
	if (__predict_false(hdr->dack_mode != toep->tp_delack_mode)) {
		toep->tp_delack_mode = hdr->dack_mode;
		toep->tp_delack_seq = tp->rcv_nxt;
	}
	CTR6(KTR_TOM, "appending mbuf=%p pktlen=%d m_len=%d len=%d rcv_nxt=0x%x enqueued_bytes=%d",
	    m, m->m_pkthdr.len, m->m_len, len, tp->rcv_nxt, toep->tp_enqueued_bytes);
	
	if (len < m->m_pkthdr.len)
		m->m_pkthdr.len = m->m_len = len;

	tp->rcv_nxt += m->m_pkthdr.len;
	tp->t_rcvtime = ticks;
	toep->tp_enqueued_bytes += m->m_pkthdr.len;
	CTR2(KTR_TOM,
	    "new_rx_data: seq 0x%x len %u",
	    m->m_seq, m->m_pkthdr.len);
	inp_wunlock(tp->t_inpcb);
	rcv = so_sockbuf_rcv(so);
	sockbuf_lock(rcv);
#if 0	
	if (sb_notify(rcv))
		DPRINTF("rx_data so=%p flags=0x%x len=%d\n", so, rcv->sb_flags, m->m_pkthdr.len);
#endif
	SBAPPEND(rcv, m);

#ifdef notyet
	/*
	 * We're giving too many credits to the card - but disable this check so we can keep on moving :-|
	 *
	 */
	KASSERT(rcv->sb_cc < (rcv->sb_mbmax << 1),

	    ("so=%p, data contents exceed mbmax, sb_cc=%d sb_mbmax=%d",
		so, rcv->sb_cc, rcv->sb_mbmax));
#endif
	

	CTR2(KTR_TOM, "sb_cc=%d sb_mbcnt=%d",
	    rcv->sb_cc, rcv->sb_mbcnt);
	
	state = so_state_get(so);	
	if (__predict_true((state & SS_NOFDREF) == 0))
		so_sorwakeup_locked(so);
	else
		sockbuf_unlock(rcv);
}

/*
 * Handler for RX_DATA CPL messages.
 */
static int
do_rx_data(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;

	DPRINTF("rx_data len=%d\n", m->m_pkthdr.len);
	
	new_rx_data(toep, m);

	return (0);
}

static void
new_rx_data_ddp(struct toepcb *toep, struct mbuf *m)
{
	struct tcpcb *tp;
	struct ddp_state *q;
	struct ddp_buf_state *bsp;
	struct cpl_rx_data_ddp *hdr;
	struct socket *so;	
	unsigned int ddp_len, rcv_nxt, ddp_report, end_offset, buf_idx;
	int nomoredata = 0;
	unsigned int delack_mode;
	struct sockbuf *rcv;
	
	tp = toep->tp_tp;	
	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(tp->t_inpcb);

	if (__predict_false(so_no_receive(so))) {

		handle_excess_rx(toep, m);
		inp_wunlock(tp->t_inpcb);
		return;
	}
	
	q = &toep->tp_ddp_state;
	hdr = cplhdr(m);
	ddp_report = ntohl(hdr->u.ddp_report);
	buf_idx = (ddp_report >> S_DDP_BUF_IDX) & 1;
	bsp = &q->buf_state[buf_idx];

	CTR4(KTR_TOM,
	    "new_rx_data_ddp: tp->rcv_nxt 0x%x cur_offset %u "
	    "hdr seq 0x%x len %u",
	    tp->rcv_nxt, bsp->cur_offset, ntohl(hdr->seq),
	    ntohs(hdr->len));
	CTR3(KTR_TOM,
	    "new_rx_data_ddp: offset %u ddp_report 0x%x buf_idx=%d",
	    G_DDP_OFFSET(ddp_report), ddp_report, buf_idx);
	
	ddp_len = ntohs(hdr->len);
	rcv_nxt = ntohl(hdr->seq) + ddp_len;

	delack_mode = G_DDP_DACK_MODE(ddp_report);
	if (__predict_false(G_DDP_DACK_MODE(ddp_report) != toep->tp_delack_mode)) {
		toep->tp_delack_mode = delack_mode;
		toep->tp_delack_seq = tp->rcv_nxt;
	}
	
	m->m_seq = tp->rcv_nxt;
	tp->rcv_nxt = rcv_nxt;

	tp->t_rcvtime = ticks;
	/*
	 * Store the length in m->m_len.  We are changing the meaning of
	 * m->m_len here, we need to be very careful that nothing from now on
	 * interprets ->len of this packet the usual way.
	 */
	m->m_len = m->m_pkthdr.len = rcv_nxt - m->m_seq;
	inp_wunlock(tp->t_inpcb);
	CTR3(KTR_TOM,
	    "new_rx_data_ddp: m_len=%u rcv_next 0x%08x rcv_nxt_prev=0x%08x ",
	    m->m_len, rcv_nxt, m->m_seq);
	/*
	 * Figure out where the new data was placed in the buffer and store it
	 * in when.  Assumes the buffer offset starts at 0, consumer needs to
	 * account for page pod's pg_offset.
	 */
	end_offset = G_DDP_OFFSET(ddp_report) + ddp_len;
	m->m_cur_offset = end_offset - m->m_pkthdr.len;

	rcv = so_sockbuf_rcv(so);
	sockbuf_lock(rcv);	

	m->m_ddp_gl = (unsigned char *)bsp->gl;
	m->m_flags |= M_DDP;
	bsp->cur_offset = end_offset;
	toep->tp_enqueued_bytes += m->m_pkthdr.len;

	/*
	 * Length is only meaningful for kbuf
	 */
	if (!(bsp->flags & DDP_BF_NOCOPY))
		KASSERT(m->m_len <= bsp->gl->dgl_length,
		    ("length received exceeds ddp pages: len=%d dgl_length=%d",
			m->m_len, bsp->gl->dgl_length));

	KASSERT(m->m_len > 0, ("%s m_len=%d", __FUNCTION__, m->m_len));
	KASSERT(m->m_next == NULL, ("m_len=%p", m->m_next));
        /*
	 * Bit 0 of flags stores whether the DDP buffer is completed.
	 * Note that other parts of the code depend on this being in bit 0.
	 */
	if ((bsp->flags & DDP_BF_NOINVAL) && end_offset != bsp->gl->dgl_length) {
		panic("spurious ddp completion");
	} else {
		m->m_ddp_flags = !!(ddp_report & F_DDP_BUF_COMPLETE);
		if (m->m_ddp_flags && !(bsp->flags & DDP_BF_NOFLIP)) 
			q->cur_buf ^= 1;                     /* flip buffers */
	}

	if (bsp->flags & DDP_BF_NOCOPY) {
		m->m_ddp_flags |= (bsp->flags & DDP_BF_NOCOPY);
		bsp->flags &= ~DDP_BF_NOCOPY;
	}

	if (ddp_report & F_DDP_PSH)
		m->m_ddp_flags |= DDP_BF_PSH;
	if (nomoredata)
		m->m_ddp_flags |= DDP_BF_NODATA;

#ifdef notyet	
	skb_reset_transport_header(skb);
	tcp_hdr(skb)->fin = 0;          /* changes original hdr->ddp_report */
#endif
	SBAPPEND(rcv, m);

	if ((so_state_get(so) & SS_NOFDREF) == 0 && ((ddp_report & F_DDP_PSH) ||
	    (((m->m_ddp_flags & (DDP_BF_NOCOPY|1)) == (DDP_BF_NOCOPY|1))
		|| !(m->m_ddp_flags & DDP_BF_NOCOPY))))
		so_sorwakeup_locked(so);
	else
		sockbuf_unlock(rcv);
}

#define DDP_ERR (F_DDP_PPOD_MISMATCH | F_DDP_LLIMIT_ERR | F_DDP_ULIMIT_ERR |\
		 F_DDP_PPOD_PARITY_ERR | F_DDP_PADDING_ERR | F_DDP_OFFSET_ERR |\
		 F_DDP_INVALID_TAG | F_DDP_COLOR_ERR | F_DDP_TID_MISMATCH |\
		 F_DDP_INVALID_PPOD)

/*
 * Handler for RX_DATA_DDP CPL messages.
 */
static int
do_rx_data_ddp(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = ctx;
	const struct cpl_rx_data_ddp *hdr = cplhdr(m);

	VALIDATE_SOCK(so);

	if (__predict_false(ntohl(hdr->ddpvld_status) & DDP_ERR)) {
		log(LOG_ERR, "RX_DATA_DDP for TID %u reported error 0x%x\n",
		       GET_TID(hdr), G_DDP_VALID(ntohl(hdr->ddpvld_status)));
		return (CPL_RET_BUF_DONE);
	}
#if 0
	skb->h.th = tcphdr_skb->h.th;
#endif	
	new_rx_data_ddp(toep, m);
	return (0);
}

static void
process_ddp_complete(struct toepcb *toep, struct mbuf *m)
{
	struct tcpcb *tp = toep->tp_tp;
	struct socket *so;
	struct ddp_state *q;
	struct ddp_buf_state *bsp;
	struct cpl_rx_ddp_complete *hdr;
	unsigned int ddp_report, buf_idx, when, delack_mode;
	int nomoredata = 0;
	struct sockbuf *rcv;
	
	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(tp->t_inpcb);

	if (__predict_false(so_no_receive(so))) {
		struct inpcb *inp = so_sotoinpcb(so);

		handle_excess_rx(toep, m);
		inp_wunlock(inp);
		return;
	}
	q = &toep->tp_ddp_state; 
	hdr = cplhdr(m);
	ddp_report = ntohl(hdr->ddp_report);
	buf_idx = (ddp_report >> S_DDP_BUF_IDX) & 1;
	m->m_pkthdr.csum_data = tp->rcv_nxt;

	rcv = so_sockbuf_rcv(so);
	sockbuf_lock(rcv);

	bsp = &q->buf_state[buf_idx];
	when = bsp->cur_offset;
	m->m_len = m->m_pkthdr.len = G_DDP_OFFSET(ddp_report) - when;
	tp->rcv_nxt += m->m_len;
	tp->t_rcvtime = ticks;

	delack_mode = G_DDP_DACK_MODE(ddp_report);
	if (__predict_false(G_DDP_DACK_MODE(ddp_report) != toep->tp_delack_mode)) {
		toep->tp_delack_mode = delack_mode;
		toep->tp_delack_seq = tp->rcv_nxt;
	}
#ifdef notyet
	skb_reset_transport_header(skb);
	tcp_hdr(skb)->fin = 0;          /* changes valid memory past CPL */
#endif
	inp_wunlock(tp->t_inpcb);

	KASSERT(m->m_len >= 0, ("%s m_len=%d", __FUNCTION__, m->m_len));
	CTR5(KTR_TOM,
		  "process_ddp_complete: tp->rcv_nxt 0x%x cur_offset %u "
		  "ddp_report 0x%x offset %u, len %u",
		  tp->rcv_nxt, bsp->cur_offset, ddp_report,
		   G_DDP_OFFSET(ddp_report), m->m_len);

	m->m_cur_offset = bsp->cur_offset;
	bsp->cur_offset += m->m_len;

	if (!(bsp->flags & DDP_BF_NOFLIP)) {
		q->cur_buf ^= 1;                     /* flip buffers */
		if (G_DDP_OFFSET(ddp_report) < q->kbuf[0]->dgl_length)
			nomoredata=1;
	}
		
	CTR4(KTR_TOM,
		  "process_ddp_complete: tp->rcv_nxt 0x%x cur_offset %u "
		  "ddp_report %u offset %u",
		  tp->rcv_nxt, bsp->cur_offset, ddp_report,
		   G_DDP_OFFSET(ddp_report));
	
	m->m_ddp_gl = (unsigned char *)bsp->gl;
	m->m_flags |= M_DDP;
	m->m_ddp_flags = (bsp->flags & DDP_BF_NOCOPY) | 1;
	if (bsp->flags & DDP_BF_NOCOPY)
		bsp->flags &= ~DDP_BF_NOCOPY;
	if (nomoredata)
		m->m_ddp_flags |= DDP_BF_NODATA;

	SBAPPEND(rcv, m);
	if ((so_state_get(so) & SS_NOFDREF) == 0)
		so_sorwakeup_locked(so);
	else
		sockbuf_unlock(rcv);
}

/*
 * Handler for RX_DDP_COMPLETE CPL messages.
 */
static int
do_rx_ddp_complete(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = ctx;

	VALIDATE_SOCK(so);
#if 0
	skb->h.th = tcphdr_skb->h.th;
#endif	
	process_ddp_complete(toep, m);
	return (0);
}

/*
 * Move a socket to TIME_WAIT state.  We need to make some adjustments to the
 * socket state before calling tcp_time_wait to comply with its expectations.
 */
static void
enter_timewait(struct tcpcb *tp)
{
	/*
	 * Bump rcv_nxt for the peer FIN.  We don't do this at the time we
	 * process peer_close because we don't want to carry the peer FIN in
	 * the socket's receive queue and if we increment rcv_nxt without
	 * having the FIN in the receive queue we'll confuse facilities such
	 * as SIOCINQ.
	 */
	inp_wlock(tp->t_inpcb);	
	tp->rcv_nxt++;

	tp->ts_recent_age = 0;	     /* defeat recycling */
	tp->t_srtt = 0;                        /* defeat tcp_update_metrics */
	inp_wunlock(tp->t_inpcb);
	tcp_offload_twstart(tp);
}

/*
 * For TCP DDP a PEER_CLOSE may also be an implicit RX_DDP_COMPLETE.  This
 * function deals with the data that may be reported along with the FIN.
 * Returns -1 if no further processing of the PEER_CLOSE is needed, >= 0 to
 * perform normal FIN-related processing.  In the latter case 1 indicates that
 * there was an implicit RX_DDP_COMPLETE and the skb should not be freed, 0 the
 * skb can be freed.
 */
static int
handle_peer_close_data(struct socket *so, struct mbuf *m)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct ddp_state *q;
	struct ddp_buf_state *bsp;
	struct cpl_peer_close *req = cplhdr(m);
	unsigned int rcv_nxt = ntohl(req->rcv_nxt) - 1; /* exclude FIN */
	struct sockbuf *rcv;
	
	if (tp->rcv_nxt == rcv_nxt)			/* no data */
		return (0);

	CTR0(KTR_TOM, "handle_peer_close_data");
	if (__predict_false(so_no_receive(so))) {
		handle_excess_rx(toep, m);

		/*
		 * Although we discard the data we want to process the FIN so
		 * that PEER_CLOSE + data behaves the same as RX_DATA_DDP +
		 * PEER_CLOSE without data.  In particular this PEER_CLOSE
		 * may be what will close the connection.  We return 1 because
		 * handle_excess_rx() already freed the packet.
		 */
		return (1);
	}

	inp_lock_assert(tp->t_inpcb);
	q = &toep->tp_ddp_state;
	rcv = so_sockbuf_rcv(so);
	sockbuf_lock(rcv);

	bsp = &q->buf_state[q->cur_buf];
	m->m_len = m->m_pkthdr.len = rcv_nxt - tp->rcv_nxt;
	KASSERT(m->m_len > 0, ("%s m_len=%d", __FUNCTION__, m->m_len));
	m->m_ddp_gl = (unsigned char *)bsp->gl;
	m->m_flags |= M_DDP;
	m->m_cur_offset = bsp->cur_offset;
	m->m_ddp_flags = 
	    DDP_BF_PSH | (bsp->flags & DDP_BF_NOCOPY) | 1;
	m->m_seq = tp->rcv_nxt;
	tp->rcv_nxt = rcv_nxt;
	bsp->cur_offset += m->m_pkthdr.len;
	if (!(bsp->flags & DDP_BF_NOFLIP))
		q->cur_buf ^= 1;
#ifdef notyet	
	skb_reset_transport_header(skb);
	tcp_hdr(skb)->fin = 0;          /* changes valid memory past CPL */
#endif	
	tp->t_rcvtime = ticks;
	SBAPPEND(rcv, m);
	if (__predict_true((so_state_get(so) & SS_NOFDREF) == 0))
		so_sorwakeup_locked(so);
	else
		sockbuf_unlock(rcv);

	return (1);
}

/*
 * Handle a peer FIN.
 */
static void
do_peer_fin(struct toepcb *toep, struct mbuf *m)
{
	struct socket *so;
	struct tcpcb *tp = toep->tp_tp;
	int keep, action;
	
	action = keep = 0;	
	CTR1(KTR_TOM, "do_peer_fin state=%d", tp->t_state);
	if (!is_t3a(toep->tp_toedev) && (toep->tp_flags & TP_ABORT_RPL_PENDING)) {
		printf("abort_pending set\n");
		
		goto out;
	}
	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(toep->tp_tp->t_inpcb);
	if (toep->tp_ulp_mode == ULP_MODE_TCPDDP) {
		keep = handle_peer_close_data(so, m);
		if (keep < 0) {
			inp_wunlock(tp->t_inpcb);					
			return;
		}
	}
	if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		CTR1(KTR_TOM,
		    "waking up waiters for cantrcvmore on %p ", so);	
		socantrcvmore(so);

		/*
		 * If connection is half-synchronized
		 * (ie NEEDSYN flag on) then delay ACK,
		 * so it may be piggybacked when SYN is sent.
		 * Otherwise, since we received a FIN then no
		 * more input can be expected, send ACK now.
		 */
		if (tp->t_flags & TF_NEEDSYN)
			tp->t_flags |= TF_DELACK;
		else
			tp->t_flags |= TF_ACKNOW;
		tp->rcv_nxt++;
	}
	
	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
	    tp->t_starttime = ticks;
	/* FALLTHROUGH */ 
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_CLOSE_WAIT;
		break;
	case TCPS_FIN_WAIT_1:
		tp->t_state = TCPS_CLOSING;
		break;
	case TCPS_FIN_WAIT_2:
		/*
		 * If we've sent an abort_req we must have sent it too late,
		 * HW will send us a reply telling us so, and this peer_close
		 * is really the last message for this connection and needs to
		 * be treated as an abort_rpl, i.e., transition the connection
		 * to TCP_CLOSE (note that the host stack does this at the
		 * time of generating the RST but we must wait for HW).
		 * Otherwise we enter TIME_WAIT.
		 */
		t3_release_offload_resources(toep);
		if (toep->tp_flags & TP_ABORT_RPL_PENDING) {
			action = TCP_CLOSE;
		} else {
			action = TCP_TIMEWAIT;			
		}
		break;
	default:
		log(LOG_ERR,
		       "%s: TID %u received PEER_CLOSE in bad state %d\n",
		    toep->tp_toedev->tod_name, toep->tp_tid, tp->t_state);
	}
	inp_wunlock(tp->t_inpcb);					

	if (action == TCP_TIMEWAIT) {
		enter_timewait(tp);
	} else if (action == TCP_DROP) {
		tcp_offload_drop(tp, 0);		
	} else if (action == TCP_CLOSE) {
		tcp_offload_close(tp);		
	}

#ifdef notyet		
	/* Do not send POLL_HUP for half duplex close. */
	if ((sk->sk_shutdown & SEND_SHUTDOWN) ||
	    sk->sk_state == TCP_CLOSE)
		sk_wake_async(so, 1, POLL_HUP);
	else
		sk_wake_async(so, 1, POLL_IN);
#endif

out:
	if (!keep)
		m_free(m);
}

/*
 * Handler for PEER_CLOSE CPL messages.
 */
static int
do_peer_close(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;

	VALIDATE_SOCK(so);

	do_peer_fin(toep, m);
	return (0);
}

static void
process_close_con_rpl(struct toepcb *toep, struct mbuf *m)
{
	struct cpl_close_con_rpl *rpl = cplhdr(m);
	struct tcpcb *tp = toep->tp_tp;	
	struct socket *so;	
	int action = 0;
	struct sockbuf *rcv;	
	
	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(tp->t_inpcb);	
	
	tp->snd_una = ntohl(rpl->snd_nxt) - 1;  /* exclude FIN */

	if (!is_t3a(toep->tp_toedev) && (toep->tp_flags & TP_ABORT_RPL_PENDING)) {
		inp_wunlock(tp->t_inpcb);
		goto out;
	}
	
	CTR3(KTR_TOM, "process_close_con_rpl(%p) state=%d dead=%d", toep, 
	    tp->t_state, !!(so_state_get(so) & SS_NOFDREF));

	switch (tp->t_state) {
	case TCPS_CLOSING:              /* see FIN_WAIT2 case in do_peer_fin */
		t3_release_offload_resources(toep);
		if (toep->tp_flags & TP_ABORT_RPL_PENDING) {
			action = TCP_CLOSE;

		} else {
			action = TCP_TIMEWAIT;
		}
		break;
	case TCPS_LAST_ACK:
		/*
		 * In this state we don't care about pending abort_rpl.
		 * If we've sent abort_req it was post-close and was sent too
		 * late, this close_con_rpl is the actual last message.
		 */
		t3_release_offload_resources(toep);
		action = TCP_CLOSE;
		break;
	case TCPS_FIN_WAIT_1:
		/*
		 * If we can't receive any more
		 * data, then closing user can proceed.
		 * Starting the timer is contrary to the
		 * specification, but if we don't get a FIN
		 * we'll hang forever.
		 *
		 * XXXjl:
		 * we should release the tp also, and use a
		 * compressed state.
		 */
		if (so)
			rcv = so_sockbuf_rcv(so);
		else
			break;
		
		if (rcv->sb_state & SBS_CANTRCVMORE) {
			int timeout;

			if (so)
				soisdisconnected(so);
			timeout = (tcp_fast_finwait2_recycle) ? 
			    tcp_finwait2_timeout : tcp_maxidle;
			tcp_timer_activate(tp, TT_2MSL, timeout);
		}
		tp->t_state = TCPS_FIN_WAIT_2;
		if ((so_options_get(so) & SO_LINGER) && so_linger_get(so) == 0 &&
		    (toep->tp_flags & TP_ABORT_SHUTDOWN) == 0) {
			action = TCP_DROP;
		}

		break;
	default:
		log(LOG_ERR,
		       "%s: TID %u received CLOSE_CON_RPL in bad state %d\n",
		       toep->tp_toedev->tod_name, toep->tp_tid,
		       tp->t_state);
	}
	inp_wunlock(tp->t_inpcb);


	if (action == TCP_TIMEWAIT) {
		enter_timewait(tp);
	} else if (action == TCP_DROP) {
		tcp_offload_drop(tp, 0);		
	} else if (action == TCP_CLOSE) {
		tcp_offload_close(tp);		
	}
out:
	m_freem(m);
}

/*
 * Handler for CLOSE_CON_RPL CPL messages.
 */
static int
do_close_con_rpl(struct t3cdev *cdev, struct mbuf *m,
			    void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;

	process_close_con_rpl(toep, m);
	return (0);
}

/*
 * Process abort replies.  We only process these messages if we anticipate
 * them as the coordination between SW and HW in this area is somewhat lacking
 * and sometimes we get ABORT_RPLs after we are done with the connection that
 * originated the ABORT_REQ.
 */
static void
process_abort_rpl(struct toepcb *toep, struct mbuf *m)
{
	struct tcpcb *tp = toep->tp_tp;
	struct socket *so;	
	int needclose = 0;
	
#ifdef T3_TRACE
	T3_TRACE1(TIDTB(sk),
		  "process_abort_rpl: GTS rpl pending %d",
		  sock_flag(sk, ABORT_RPL_PENDING));
#endif
	
	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(tp->t_inpcb);
	
	if (toep->tp_flags & TP_ABORT_RPL_PENDING) {
		/*
		 * XXX panic on tcpdrop
		 */
		if (!(toep->tp_flags & TP_ABORT_RPL_RCVD) && !is_t3a(toep->tp_toedev))
			toep->tp_flags |= TP_ABORT_RPL_RCVD;
		else {
			toep->tp_flags &= ~(TP_ABORT_RPL_RCVD|TP_ABORT_RPL_PENDING);
			if (!(toep->tp_flags & TP_ABORT_REQ_RCVD) ||
			    !is_t3a(toep->tp_toedev)) {
				if (toep->tp_flags & TP_ABORT_REQ_RCVD)
					panic("TP_ABORT_REQ_RCVD set");
				t3_release_offload_resources(toep);
				needclose = 1;
			}
		}
	}
	inp_wunlock(tp->t_inpcb);

	if (needclose)
		tcp_offload_close(tp);

	m_free(m);
}

/*
 * Handle an ABORT_RPL_RSS CPL message.
 */
static int
do_abort_rpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct cpl_abort_rpl_rss *rpl = cplhdr(m);
	struct toepcb *toep;
	
	/*
	 * Ignore replies to post-close aborts indicating that the abort was
	 * requested too late.  These connections are terminated when we get
	 * PEER_CLOSE or CLOSE_CON_RPL and by the time the abort_rpl_rss
	 * arrives the TID is either no longer used or it has been recycled.
	 */
	if (rpl->status == CPL_ERR_ABORT_FAILED) {
discard:
		m_free(m);
		return (0);
	}

	toep = (struct toepcb *)ctx;
	
        /*
	 * Sometimes we've already closed the socket, e.g., a post-close
	 * abort races with ABORT_REQ_RSS, the latter frees the socket
	 * expecting the ABORT_REQ will fail with CPL_ERR_ABORT_FAILED,
	 * but FW turns the ABORT_REQ into a regular one and so we get
	 * ABORT_RPL_RSS with status 0 and no socket.  Only on T3A.
	 */
	if (!toep)
		goto discard;

	if (toep->tp_tp == NULL) {
		log(LOG_NOTICE, "removing tid for abort\n");
		cxgb_remove_tid(cdev, toep, toep->tp_tid);
		if (toep->tp_l2t) 
			l2t_release(L2DATA(cdev), toep->tp_l2t);

		toepcb_release(toep);
		goto discard;
	}
	
	log(LOG_NOTICE, "toep=%p\n", toep);
	log(LOG_NOTICE, "tp=%p\n", toep->tp_tp);

	toepcb_hold(toep);
	process_abort_rpl(toep, m);
	toepcb_release(toep);
	return (0);
}

/*
 * Convert the status code of an ABORT_REQ into a FreeBSD error code.  Also
 * indicate whether RST should be sent in response.
 */
static int
abort_status_to_errno(struct socket *so, int abort_reason, int *need_rst)
{
	struct tcpcb *tp = so_sototcpcb(so);

	switch (abort_reason) {
	case CPL_ERR_BAD_SYN:
#if 0		
		NET_INC_STATS_BH(LINUX_MIB_TCPABORTONSYN);	// fall through
#endif		
	case CPL_ERR_CONN_RESET:
		// XXX need to handle SYN_RECV due to crossed SYNs
		return (tp->t_state == TCPS_CLOSE_WAIT ? EPIPE : ECONNRESET);
	case CPL_ERR_XMIT_TIMEDOUT:
	case CPL_ERR_PERSIST_TIMEDOUT:
	case CPL_ERR_FINWAIT2_TIMEDOUT:
	case CPL_ERR_KEEPALIVE_TIMEDOUT:
#if 0		
		NET_INC_STATS_BH(LINUX_MIB_TCPABORTONTIMEOUT);
#endif		
		return (ETIMEDOUT);
	default:
		return (EIO);
	}
}

static inline void
set_abort_rpl_wr(struct mbuf *m, unsigned int tid, int cmd)
{
	struct cpl_abort_rpl *rpl = cplhdr(m);

	rpl->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_OFLD_HOST_ABORT_CON_RPL));
	rpl->wr.wr_lo = htonl(V_WR_TID(tid));
	m->m_len = m->m_pkthdr.len = sizeof(*rpl);
	
	OPCODE_TID(rpl) = htonl(MK_OPCODE_TID(CPL_ABORT_RPL, tid));
	rpl->cmd = cmd;
}

static void
send_deferred_abort_rpl(struct toedev *tdev, struct mbuf *m)
{
	struct mbuf *reply_mbuf;
	struct cpl_abort_req_rss *req = cplhdr(m);

	reply_mbuf = m_gethdr_nofail(sizeof(struct cpl_abort_rpl));
	m_set_priority(m, CPL_PRIORITY_DATA);
	m->m_len = m->m_pkthdr.len = sizeof(struct cpl_abort_rpl);
	set_abort_rpl_wr(reply_mbuf, GET_TID(req), req->status);
	cxgb_ofld_send(TOM_DATA(tdev)->cdev, reply_mbuf);
	m_free(m);
}

/*
 * Returns whether an ABORT_REQ_RSS message is a negative advice.
 */
static inline int
is_neg_adv_abort(unsigned int status)
{
	return status == CPL_ERR_RTX_NEG_ADVICE ||
	    status == CPL_ERR_PERSIST_NEG_ADVICE;
}

static void
send_abort_rpl(struct mbuf *m, struct toedev *tdev, int rst_status)
{
	struct mbuf  *reply_mbuf;
	struct cpl_abort_req_rss *req = cplhdr(m);

	reply_mbuf = m_gethdr(M_NOWAIT, MT_DATA);

	if (!reply_mbuf) {
		/* Defer the reply.  Stick rst_status into req->cmd. */
		req->status = rst_status;
		t3_defer_reply(m, tdev, send_deferred_abort_rpl);
		return;
	}

	m_set_priority(reply_mbuf, CPL_PRIORITY_DATA);
	set_abort_rpl_wr(reply_mbuf, GET_TID(req), rst_status);
	m_free(m);

	/*
	 * XXX need to sync with ARP as for SYN_RECV connections we can send
	 * these messages while ARP is pending.  For other connection states
	 * it's not a problem.
	 */
	cxgb_ofld_send(TOM_DATA(tdev)->cdev, reply_mbuf);
}

#ifdef notyet
static void
cleanup_syn_rcv_conn(struct socket *child, struct socket *parent)
{
	CXGB_UNIMPLEMENTED();
#ifdef notyet	
	struct request_sock *req = child->sk_user_data;

	inet_csk_reqsk_queue_removed(parent, req);
	synq_remove(tcp_sk(child));
	__reqsk_free(req);
	child->sk_user_data = NULL;
#endif
}


/*
 * Performs the actual work to abort a SYN_RECV connection.
 */
static void
do_abort_syn_rcv(struct socket *child, struct socket *parent)
{
	struct tcpcb *parenttp = so_sototcpcb(parent);
	struct tcpcb *childtp = so_sototcpcb(child);

	/*
	 * If the server is still open we clean up the child connection,
	 * otherwise the server already did the clean up as it was purging
	 * its SYN queue and the skb was just sitting in its backlog.
	 */
	if (__predict_false(parenttp->t_state == TCPS_LISTEN)) {
		cleanup_syn_rcv_conn(child, parent);
		inp_wlock(childtp->t_inpcb);
		t3_release_offload_resources(childtp->t_toe);
		inp_wunlock(childtp->t_inpcb);
		tcp_offload_close(childtp);
	}
}
#endif

/*
 * Handle abort requests for a SYN_RECV connection.  These need extra work
 * because the socket is on its parent's SYN queue.
 */
static int
abort_syn_rcv(struct socket *so, struct mbuf *m)
{
	CXGB_UNIMPLEMENTED();
#ifdef notyet	
	struct socket *parent;
	struct toedev *tdev = toep->tp_toedev;
	struct t3cdev *cdev = TOM_DATA(tdev)->cdev;
	struct socket *oreq = so->so_incomp;
	struct t3c_tid_entry *t3c_stid;
	struct tid_info *t;

	if (!oreq)
		return -1;        /* somehow we are not on the SYN queue */

	t = &(T3C_DATA(cdev))->tid_maps;
	t3c_stid = lookup_stid(t, oreq->ts_recent);
	parent = ((struct listen_ctx *)t3c_stid->ctx)->lso;

	so_lock(parent);
	do_abort_syn_rcv(so, parent);
	send_abort_rpl(m, tdev, CPL_ABORT_NO_RST);
	so_unlock(parent);
#endif
	return (0);
}

/*
 * Process abort requests.  If we are waiting for an ABORT_RPL we ignore this
 * request except that we need to reply to it.
 */
static void
process_abort_req(struct toepcb *toep, struct mbuf *m, struct toedev *tdev)
{
	int rst_status = CPL_ABORT_NO_RST;
	const struct cpl_abort_req_rss *req = cplhdr(m);
	struct tcpcb *tp = toep->tp_tp; 
	struct socket *so;
	int needclose = 0;
	
	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(toep->tp_tp->t_inpcb);
	if ((toep->tp_flags & TP_ABORT_REQ_RCVD) == 0) {
		toep->tp_flags |= (TP_ABORT_REQ_RCVD|TP_ABORT_SHUTDOWN);
		m_free(m);
		goto skip;
	}

	toep->tp_flags &= ~TP_ABORT_REQ_RCVD;
	/*
	 * Three cases to consider:
	 * a) We haven't sent an abort_req; close the connection.
	 * b) We have sent a post-close abort_req that will get to TP too late
	 *    and will generate a CPL_ERR_ABORT_FAILED reply.  The reply will
	 *    be ignored and the connection should be closed now.
	 * c) We have sent a regular abort_req that will get to TP too late.
	 *    That will generate an abort_rpl with status 0, wait for it.
	 */
	if (((toep->tp_flags & TP_ABORT_RPL_PENDING) == 0) ||
	    (is_t3a(toep->tp_toedev) && (toep->tp_flags & TP_CLOSE_CON_REQUESTED))) {
		int error;
		
		error = abort_status_to_errno(so, req->status,
		    &rst_status);
		so_error_set(so, error);

		if (__predict_true((so_state_get(so) & SS_NOFDREF) == 0))
			so_sorwakeup(so);
		/*
		 * SYN_RECV needs special processing.  If abort_syn_rcv()
		 * returns 0 is has taken care of the abort.
		 */
		if ((tp->t_state == TCPS_SYN_RECEIVED) && !abort_syn_rcv(so, m))
			goto skip;

		t3_release_offload_resources(toep);
		needclose = 1;
	}
	inp_wunlock(tp->t_inpcb);

	if (needclose)
		tcp_offload_close(tp);

	send_abort_rpl(m, tdev, rst_status);
	return;
skip:
	inp_wunlock(tp->t_inpcb);	
}

/*
 * Handle an ABORT_REQ_RSS CPL message.
 */
static int
do_abort_req(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	const struct cpl_abort_req_rss *req = cplhdr(m);
	struct toepcb *toep = (struct toepcb *)ctx;
	
	if (is_neg_adv_abort(req->status)) {
		m_free(m);
		return (0);
	}

	log(LOG_NOTICE, "aborting tid=%d\n", toep->tp_tid);
	
	if ((toep->tp_flags & (TP_SYN_RCVD|TP_ABORT_REQ_RCVD)) == TP_SYN_RCVD) {
		cxgb_remove_tid(cdev, toep, toep->tp_tid);
		toep->tp_flags |= TP_ABORT_REQ_RCVD;
		
		send_abort_rpl(m, toep->tp_toedev, CPL_ABORT_NO_RST);
		if (toep->tp_l2t) 
			l2t_release(L2DATA(cdev), toep->tp_l2t);

		/*
		 *  Unhook
		 */
		toep->tp_tp->t_toe = NULL;
		toep->tp_tp->t_flags &= ~TF_TOE;
		toep->tp_tp = NULL;
		/*
		 * XXX need to call syncache_chkrst - but we don't
		 * have a way of doing that yet
		 */
		toepcb_release(toep);
		log(LOG_ERR, "abort for unestablished connection :-(\n");
		return (0);
	}
	if (toep->tp_tp == NULL) {
		log(LOG_NOTICE, "disconnected toepcb\n");
		/* should be freed momentarily */
		return (0);
	}


	toepcb_hold(toep);
	process_abort_req(toep, m, toep->tp_toedev);
	toepcb_release(toep);
	return (0);
}
#ifdef notyet
static void
pass_open_abort(struct socket *child, struct socket *parent, struct mbuf *m)
{
	struct toedev *tdev = TOE_DEV(parent);

	do_abort_syn_rcv(child, parent);
	if (tdev->tod_ttid == TOE_ID_CHELSIO_T3) {
		struct cpl_pass_accept_rpl *rpl = cplhdr(m);

		rpl->opt0h = htonl(F_TCAM_BYPASS);
		rpl->opt0l_status = htonl(CPL_PASS_OPEN_REJECT);
		cxgb_ofld_send(TOM_DATA(tdev)->cdev, m);
	} else
		m_free(m);
}
#endif
static void
handle_pass_open_arp_failure(struct socket *so, struct mbuf *m)
{
	CXGB_UNIMPLEMENTED();
	
#ifdef notyet	
	struct t3cdev *cdev;
	struct socket *parent;
	struct socket *oreq;
	struct t3c_tid_entry *t3c_stid;
	struct tid_info *t;
	struct tcpcb *otp, *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	
	/*
	 * If the connection is being aborted due to the parent listening
	 * socket going away there's nothing to do, the ABORT_REQ will close
	 * the connection.
	 */
	if (toep->tp_flags & TP_ABORT_RPL_PENDING) {
		m_free(m);
		return;
	}

	oreq = so->so_incomp;
	otp = so_sototcpcb(oreq);
	
	cdev = T3C_DEV(so);
	t = &(T3C_DATA(cdev))->tid_maps;
	t3c_stid = lookup_stid(t, otp->ts_recent);
	parent = ((struct listen_ctx *)t3c_stid->ctx)->lso;

	so_lock(parent);
	pass_open_abort(so, parent, m);
	so_unlock(parent);
#endif	
}

/*
 * Handle an ARP failure for a CPL_PASS_ACCEPT_RPL.  This is treated similarly
 * to an ABORT_REQ_RSS in SYN_RECV as both events need to tear down a SYN_RECV
 * connection.
 */
static void
pass_accept_rpl_arp_failure(struct t3cdev *cdev, struct mbuf *m)
{

#ifdef notyet	
	TCP_INC_STATS_BH(TCP_MIB_ATTEMPTFAILS);
	BLOG_SKB_CB(skb)->dev = TOE_DEV(skb->sk);
#endif
	handle_pass_open_arp_failure(m_get_socket(m), m);
}

/*
 * Populate a reject CPL_PASS_ACCEPT_RPL WR.
 */
static void
mk_pass_accept_rpl(struct mbuf *reply_mbuf, struct mbuf *req_mbuf)
{
	struct cpl_pass_accept_req *req = cplhdr(req_mbuf);
	struct cpl_pass_accept_rpl *rpl = cplhdr(reply_mbuf);
	unsigned int tid = GET_TID(req);

	m_set_priority(reply_mbuf, CPL_PRIORITY_SETUP);
	rpl->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(rpl) = htonl(MK_OPCODE_TID(CPL_PASS_ACCEPT_RPL, tid));
	rpl->peer_ip = req->peer_ip;   // req->peer_ip not overwritten yet
	rpl->opt0h = htonl(F_TCAM_BYPASS);
	rpl->opt0l_status = htonl(CPL_PASS_OPEN_REJECT);
	rpl->opt2 = 0;
	rpl->rsvd = rpl->opt2;   /* workaround for HW bug */
}

/*
 * Send a deferred reject to an accept request.
 */
static void
reject_pass_request(struct toedev *tdev, struct mbuf *m)
{
	struct mbuf *reply_mbuf;

	reply_mbuf = m_gethdr_nofail(sizeof(struct cpl_pass_accept_rpl));
	mk_pass_accept_rpl(reply_mbuf, m);
	cxgb_ofld_send(TOM_DATA(tdev)->cdev, reply_mbuf);
	m_free(m);
}

static void
handle_syncache_event(int event, void *arg)
{
	struct toepcb *toep = arg;

	switch (event) {
	case TOE_SC_ENTRY_PRESENT:
		/*
		 * entry already exists - free toepcb
		 * and l2t
		 */
		printf("syncache entry present\n");
		toepcb_release(toep);
		break;
	case TOE_SC_DROP:
		/*
		 * The syncache has given up on this entry
		 * either it timed out, or it was evicted
		 * we need to explicitly release the tid
		 */
		printf("syncache entry dropped\n");
		toepcb_release(toep);		
		break;
	default:
		log(LOG_ERR, "unknown syncache event %d\n", event);
		break;
	}
}

static void
syncache_add_accept_req(struct cpl_pass_accept_req *req, struct socket *lso, struct toepcb *toep)
{
	struct in_conninfo inc;
	struct tcpopt to;
	struct tcphdr th;
	struct inpcb *inp;
	int mss, wsf, sack, ts;
	uint32_t rcv_isn = ntohl(req->rcv_isn);
	
	bzero(&to, sizeof(struct tcpopt));
	inp = so_sotoinpcb(lso);
	
	/*
	 * Fill out information for entering us into the syncache
	 */
	bzero(&inc, sizeof(inc));
	inc.inc_fport = th.th_sport = req->peer_port;
	inc.inc_lport = th.th_dport = req->local_port;
	th.th_seq = req->rcv_isn;
	th.th_flags = TH_SYN;

	toep->tp_iss = toep->tp_delack_seq = toep->tp_rcv_wup = toep->tp_copied_seq = rcv_isn + 1;

	
	inc.inc_isipv6 = 0;
	inc.inc_len = 0;
	inc.inc_faddr.s_addr = req->peer_ip;
	inc.inc_laddr.s_addr = req->local_ip;

	DPRINTF("syncache add of %d:%d %d:%d\n",
	    ntohl(req->local_ip), ntohs(req->local_port),
	    ntohl(req->peer_ip), ntohs(req->peer_port));
	
	mss = req->tcp_options.mss;
	wsf = req->tcp_options.wsf;
	ts = req->tcp_options.tstamp;
	sack = req->tcp_options.sack;
	to.to_mss = mss;
	to.to_wscale = wsf;
	to.to_flags = (mss ? TOF_MSS : 0) | (wsf ? TOF_SCALE : 0) | (ts ? TOF_TS : 0) | (sack ? TOF_SACKPERM : 0);
	tcp_offload_syncache_add(&inc, &to, &th, inp, &lso, &cxgb_toe_usrreqs, toep);
}


/*
 * Process a CPL_PASS_ACCEPT_REQ message.  Does the part that needs the socket
 * lock held.  Note that the sock here is a listening socket that is not owned
 * by the TOE.
 */
static void
process_pass_accept_req(struct socket *so, struct mbuf *m, struct toedev *tdev,
    struct listen_ctx *lctx)
{
	int rt_flags;
	struct l2t_entry *e;
	struct iff_mac tim;
	struct mbuf *reply_mbuf, *ddp_mbuf = NULL;
	struct cpl_pass_accept_rpl *rpl;
	struct cpl_pass_accept_req *req = cplhdr(m);
	unsigned int tid = GET_TID(req);
	struct tom_data *d = TOM_DATA(tdev);
	struct t3cdev *cdev = d->cdev;
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *newtoep;
	struct rtentry *dst;
	struct sockaddr_in nam;
	struct t3c_data *td = T3C_DATA(cdev);

	reply_mbuf = m_gethdr(M_NOWAIT, MT_DATA);
	if (__predict_false(reply_mbuf == NULL)) {
		if (tdev->tod_ttid == TOE_ID_CHELSIO_T3)
			t3_defer_reply(m, tdev, reject_pass_request);
		else {
			cxgb_queue_tid_release(cdev, tid);
			m_free(m);
		}
		DPRINTF("failed to get reply_mbuf\n");
		
		goto out;
	}

	if (tp->t_state != TCPS_LISTEN) {
		DPRINTF("socket not in listen state\n");
		
		goto reject;
	}
	
	tim.mac_addr = req->dst_mac;
	tim.vlan_tag = ntohs(req->vlan_tag);
	if (cdev->ctl(cdev, GET_IFF_FROM_MAC, &tim) < 0 || !tim.dev) {
		DPRINTF("rejecting from failed GET_IFF_FROM_MAC\n");
		goto reject;
	}
	
#ifdef notyet
	/*
	 * XXX do route lookup to confirm that we're still listening on this
	 * address
	 */
	if (ip_route_input(skb, req->local_ip, req->peer_ip,
			   G_PASS_OPEN_TOS(ntohl(req->tos_tid)), tim.dev))
		goto reject;
	rt_flags = ((struct rtable *)skb->dst)->rt_flags &
		(RTCF_BROADCAST | RTCF_MULTICAST | RTCF_LOCAL);
	dst_release(skb->dst);	// done with the input route, release it
	skb->dst = NULL;
	
	if ((rt_flags & RTF_LOCAL) == 0)
		goto reject;
#endif
	/*
	 * XXX
	 */
	rt_flags = RTF_LOCAL;
	if ((rt_flags & RTF_LOCAL) == 0)
		goto reject;
	
	/*
	 * Calculate values and add to syncache
	 */

	newtoep = toepcb_alloc();
	if (newtoep == NULL)
		goto reject;

	bzero(&nam, sizeof(struct sockaddr_in));
	
	nam.sin_len = sizeof(struct sockaddr_in);
	nam.sin_family = AF_INET;
	nam.sin_addr.s_addr =req->peer_ip;
	dst = rtalloc2((struct sockaddr *)&nam, 1, 0);

	if (dst == NULL) {
		printf("failed to find route\n");
		goto reject;
	}
	e = newtoep->tp_l2t = t3_l2t_get(d->cdev, dst, tim.dev,
	    (struct sockaddr *)&nam);
	if (e == NULL) {
		DPRINTF("failed to get l2t\n");
	}
	/*
	 * Point to our listen socket until accept
	 */
	newtoep->tp_tp = tp;
	newtoep->tp_flags = TP_SYN_RCVD;
	newtoep->tp_tid = tid;
	newtoep->tp_toedev = tdev;
	tp->rcv_wnd = select_rcv_wnd(tdev, so);
	
	cxgb_insert_tid(cdev, d->client, newtoep, tid);
	so_lock(so);
	LIST_INSERT_HEAD(&lctx->synq_head, newtoep, synq_entry);
	so_unlock(so);

	newtoep->tp_ulp_mode = TOM_TUNABLE(tdev, ddp) && !(so_options_get(so) & SO_NO_DDP) &&
		       tp->rcv_wnd >= MIN_DDP_RCV_WIN ? ULP_MODE_TCPDDP : 0;

	if (newtoep->tp_ulp_mode) {
		ddp_mbuf = m_gethdr(M_NOWAIT, MT_DATA);
		
		if (ddp_mbuf == NULL)
			newtoep->tp_ulp_mode = 0;
	}
	
	CTR4(KTR_TOM, "ddp=%d rcv_wnd=%ld min_win=%d ulp_mode=%d",
	    TOM_TUNABLE(tdev, ddp), tp->rcv_wnd, MIN_DDP_RCV_WIN, newtoep->tp_ulp_mode);
	set_arp_failure_handler(reply_mbuf, pass_accept_rpl_arp_failure);
	/*
	 * XXX workaround for lack of syncache drop
	 */
	toepcb_hold(newtoep);
	syncache_add_accept_req(req, so, newtoep);
	
	rpl = cplhdr(reply_mbuf);
	reply_mbuf->m_pkthdr.len = reply_mbuf->m_len = sizeof(*rpl);
	rpl->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	rpl->wr.wr_lo = 0;
	OPCODE_TID(rpl) = htonl(MK_OPCODE_TID(CPL_PASS_ACCEPT_RPL, tid));
	rpl->opt2 = htonl(calc_opt2(so, tdev));
	rpl->rsvd = rpl->opt2;                /* workaround for HW bug */
	rpl->peer_ip = req->peer_ip;	// req->peer_ip is not overwritten

	rpl->opt0h = htonl(calc_opt0h(so, select_mss(td, NULL, dst->rt_ifp->if_mtu)) |
	    V_L2T_IDX(e->idx) | V_TX_CHANNEL(e->smt_idx));
	rpl->opt0l_status = htonl(calc_opt0l(so, newtoep->tp_ulp_mode) |
				  CPL_PASS_OPEN_ACCEPT);

	DPRINTF("opt0l_status=%08x\n", rpl->opt0l_status);
	
	m_set_priority(reply_mbuf, mkprio(CPL_PRIORITY_SETUP, newtoep));
		
	l2t_send(cdev, reply_mbuf, e);
	m_free(m);
	if (newtoep->tp_ulp_mode) {	
		__set_tcb_field(newtoep, ddp_mbuf, W_TCB_RX_DDP_FLAGS,
				V_TF_DDP_OFF(1) |
				TP_DDP_TIMER_WORKAROUND_MASK,
				V_TF_DDP_OFF(1) |
		    TP_DDP_TIMER_WORKAROUND_VAL, 1);
	} else
		DPRINTF("no DDP\n");

	return;
reject:
	if (tdev->tod_ttid == TOE_ID_CHELSIO_T3)
		mk_pass_accept_rpl(reply_mbuf, m);
	else 
		mk_tid_release(reply_mbuf, newtoep, tid);
	cxgb_ofld_send(cdev, reply_mbuf);
	m_free(m);
out:
#if 0
	TCP_INC_STATS_BH(TCP_MIB_ATTEMPTFAILS);
#else
	return;
#endif	
}      

/*
 * Handle a CPL_PASS_ACCEPT_REQ message.
 */
static int
do_pass_accept_req(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct listen_ctx *listen_ctx = (struct listen_ctx *)ctx;
	struct socket *lso = listen_ctx->lso; /* XXX need an interlock against the listen socket going away */
	struct tom_data *d = listen_ctx->tom_data;

#if VALIDATE_TID
	struct cpl_pass_accept_req *req = cplhdr(m);
	unsigned int tid = GET_TID(req);
	struct tid_info *t = &(T3C_DATA(cdev))->tid_maps;

	if (unlikely(!lsk)) {
		printk(KERN_ERR "%s: PASS_ACCEPT_REQ had unknown STID %lu\n",
		       cdev->name,
		       (unsigned long)((union listen_entry *)ctx -
					t->stid_tab));
		return CPL_RET_BUF_DONE;
	}
	if (unlikely(tid >= t->ntids)) {
		printk(KERN_ERR "%s: passive open TID %u too large\n",
		       cdev->name, tid);
		return CPL_RET_BUF_DONE;
	}
	/*
	 * For T3A the current user of the TID may have closed but its last
	 * message(s) may have been backlogged so the TID appears to be still
	 * in use.  Just take the TID away, the connection can close at its
	 * own leisure.  For T3B this situation is a bug.
	 */
	if (!valid_new_tid(t, tid) &&
	    cdev->type != T3A) {
		printk(KERN_ERR "%s: passive open uses existing TID %u\n",
		       cdev->name, tid);
		return CPL_RET_BUF_DONE;
	}
#endif

	process_pass_accept_req(lso, m, &d->tdev, listen_ctx);
	return (0);
}

/*
 * Called when a connection is established to translate the TCP options
 * reported by HW to FreeBSD's native format.
 */
static void
assign_rxopt(struct socket *so, unsigned int opt)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	const struct t3c_data *td = T3C_DATA(TOEP_T3C_DEV(toep));

	inp_lock_assert(tp->t_inpcb);
	
	toep->tp_mss_clamp = td->mtus[G_TCPOPT_MSS(opt)] - 40;
	tp->t_flags         |= G_TCPOPT_TSTAMP(opt) ? TF_RCVD_TSTMP : 0;
	tp->t_flags         |= G_TCPOPT_SACK(opt) ? TF_SACK_PERMIT : 0;
	tp->t_flags 	    |= G_TCPOPT_WSCALE_OK(opt) ? TF_RCVD_SCALE : 0;
	if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
	    (TF_RCVD_SCALE|TF_REQ_SCALE))
		tp->rcv_scale = tp->request_r_scale;
}

/*
 * Completes some final bits of initialization for just established connections
 * and changes their state to TCP_ESTABLISHED.
 *
 * snd_isn here is the ISN after the SYN, i.e., the true ISN + 1.
 */
static void
make_established(struct socket *so, u32 snd_isn, unsigned int opt)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	
	toep->tp_write_seq = tp->iss = tp->snd_max = tp->snd_nxt = tp->snd_una = snd_isn;
	assign_rxopt(so, opt);

	/*
	 *XXXXXXXXXXX
	 * 
	 */
#ifdef notyet
	so->so_proto->pr_ctloutput = t3_ctloutput;
#endif
	
#if 0	
	inet_sk(sk)->id = tp->write_seq ^ jiffies;
#endif	
	/*
	 * XXX not clear what rcv_wup maps to
	 */
	/*
	 * Causes the first RX_DATA_ACK to supply any Rx credits we couldn't
	 * pass through opt0.
	 */
	if (tp->rcv_wnd > (M_RCV_BUFSIZ << 10))
		toep->tp_rcv_wup -= tp->rcv_wnd - (M_RCV_BUFSIZ << 10);

	dump_toepcb(toep);

#ifdef notyet
/*
 * no clean interface for marking ARP up to date
 */
	dst_confirm(sk->sk_dst_cache);
#endif
	tp->t_starttime = ticks;
	tp->t_state = TCPS_ESTABLISHED;
	soisconnected(so);
}

static int
syncache_expand_establish_req(struct cpl_pass_establish *req, struct socket **so, struct toepcb *toep)
{

	struct in_conninfo inc;
	struct tcpopt to;
	struct tcphdr th;
	int mss, wsf, sack, ts;
	struct mbuf *m = NULL;
	const struct t3c_data *td = T3C_DATA(TOM_DATA(toep->tp_toedev)->cdev);
	unsigned int opt;
	
#ifdef MAC
#error	"no MAC support"
#endif	
	
	opt = ntohs(req->tcp_opt);
	
	bzero(&to, sizeof(struct tcpopt));
	
	/*
	 * Fill out information for entering us into the syncache
	 */
	bzero(&inc, sizeof(inc));
	inc.inc_fport = th.th_sport = req->peer_port;
	inc.inc_lport = th.th_dport = req->local_port;
	th.th_seq = req->rcv_isn;
	th.th_flags = TH_ACK;
	
	inc.inc_isipv6 = 0;
	inc.inc_len = 0;
	inc.inc_faddr.s_addr = req->peer_ip;
	inc.inc_laddr.s_addr = req->local_ip;
	
	mss  = td->mtus[G_TCPOPT_MSS(opt)] - 40;
	wsf  = G_TCPOPT_WSCALE_OK(opt);
	ts   = G_TCPOPT_TSTAMP(opt);
	sack = G_TCPOPT_SACK(opt);
	
	to.to_mss = mss;
	to.to_wscale =  G_TCPOPT_SND_WSCALE(opt);
	to.to_flags = (mss ? TOF_MSS : 0) | (wsf ? TOF_SCALE : 0) | (ts ? TOF_TS : 0) | (sack ? TOF_SACKPERM : 0);

	DPRINTF("syncache expand of %d:%d %d:%d mss:%d wsf:%d ts:%d sack:%d\n",
	    ntohl(req->local_ip), ntohs(req->local_port),
	    ntohl(req->peer_ip), ntohs(req->peer_port),
	    mss, wsf, ts, sack);
	return tcp_offload_syncache_expand(&inc, &to, &th, so, m);
}


/*
 * Process a CPL_PASS_ESTABLISH message.  XXX a lot of the locking doesn't work
 * if we are in TCP_SYN_RECV due to crossed SYNs
 */
static int
do_pass_establish(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct cpl_pass_establish *req = cplhdr(m);
	struct toepcb *toep = (struct toepcb *)ctx;
	struct tcpcb *tp = toep->tp_tp;
	struct socket *so, *lso;
	struct t3c_data *td = T3C_DATA(cdev);
	struct sockbuf *snd, *rcv;
	
	// Complete socket initialization now that we have the SND_ISN
	
	struct toedev *tdev;


	tdev = toep->tp_toedev;

	inp_wlock(tp->t_inpcb);
	
	/*
	 *
	 * XXX need to add reference while we're manipulating
	 */
	so = lso = inp_inpcbtosocket(tp->t_inpcb);

	inp_wunlock(tp->t_inpcb);

	so_lock(so);
	LIST_REMOVE(toep, synq_entry);
	so_unlock(so);
	
	if (!syncache_expand_establish_req(req, &so, toep)) {
		/*
		 * No entry 
		 */
		CXGB_UNIMPLEMENTED();
	}
	if (so == NULL) {
		/*
		 * Couldn't create the socket
		 */
		CXGB_UNIMPLEMENTED();
	}

	tp = so_sototcpcb(so);
	inp_wlock(tp->t_inpcb);

	snd = so_sockbuf_snd(so);
	rcv = so_sockbuf_rcv(so);

	snd->sb_flags |= SB_NOCOALESCE;
	rcv->sb_flags |= SB_NOCOALESCE;

	toep->tp_tp = tp;
	toep->tp_flags = 0;
	tp->t_toe = toep;
	reset_wr_list(toep);
	tp->rcv_wnd = select_rcv_wnd(tdev, so);
	tp->rcv_nxt = toep->tp_copied_seq;
	install_offload_ops(so);
	
	toep->tp_wr_max = toep->tp_wr_avail = TOM_TUNABLE(tdev, max_wrs);
	toep->tp_wr_unacked = 0;
	toep->tp_qset = G_QNUM(ntohl(m->m_pkthdr.csum_data));
	toep->tp_qset_idx = 0;
	toep->tp_mtu_idx = select_mss(td, tp, toep->tp_l2t->neigh->rt_ifp->if_mtu);
	
	/*
	 * XXX Cancel any keep alive timer
	 */
	     
	make_established(so, ntohl(req->snd_isn), ntohs(req->tcp_opt));

	/*
	 * XXX workaround for lack of syncache drop
	 */
	toepcb_release(toep);
	inp_wunlock(tp->t_inpcb);
	
	CTR1(KTR_TOM, "do_pass_establish tid=%u", toep->tp_tid);
	cxgb_log_tcb(cdev->adapter, toep->tp_tid);
#ifdef notyet
	/*
	 * XXX not sure how these checks map to us
	 */
	if (unlikely(sk->sk_socket)) {   // simultaneous opens only
		sk->sk_state_change(sk);
		sk_wake_async(so, 0, POLL_OUT);
	}
	/*
	 * The state for the new connection is now up to date.
	 * Next check if we should add the connection to the parent's
	 * accept queue.  When the parent closes it resets connections
	 * on its SYN queue, so check if we are being reset.  If so we
	 * don't need to do anything more, the coming ABORT_RPL will
	 * destroy this socket.  Otherwise move the connection to the
	 * accept queue.
	 *
	 * Note that we reset the synq before closing the server so if
	 * we are not being reset the stid is still open.
	 */
	if (unlikely(!tp->forward_skb_hint)) { // removed from synq
		__kfree_skb(skb);
		goto unlock;
	}
#endif
	m_free(m);

	return (0);
}

/*
 * Fill in the right TID for CPL messages waiting in the out-of-order queue
 * and send them to the TOE.
 */
static void
fixup_and_send_ofo(struct toepcb *toep)
{
	struct mbuf *m;
	struct toedev *tdev = toep->tp_toedev;
	struct tcpcb *tp = toep->tp_tp;
	unsigned int tid = toep->tp_tid;

	log(LOG_NOTICE, "fixup_and_send_ofo\n");
	
	inp_lock_assert(tp->t_inpcb);
	while ((m = mbufq_dequeue(&toep->out_of_order_queue)) != NULL) {
		/*
		 * A variety of messages can be waiting but the fields we'll
		 * be touching are common to all so any message type will do.
		 */
		struct cpl_close_con_req *p = cplhdr(m);

		p->wr.wr_lo = htonl(V_WR_TID(tid));
		OPCODE_TID(p) = htonl(MK_OPCODE_TID(p->ot.opcode, tid));
		cxgb_ofld_send(TOM_DATA(tdev)->cdev, m);
	}
}

/*
 * Updates socket state from an active establish CPL message.  Runs with the
 * socket lock held.
 */
static void
socket_act_establish(struct socket *so, struct mbuf *m)
{
	INIT_VNET_INET(so->so_vnet);
	struct cpl_act_establish *req = cplhdr(m);
	u32 rcv_isn = ntohl(req->rcv_isn);	/* real RCV_ISN + 1 */
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	
	if (__predict_false(tp->t_state != TCPS_SYN_SENT))
		log(LOG_ERR, "TID %u expected SYN_SENT, found %d\n",
		    toep->tp_tid, tp->t_state);

	tp->ts_recent_age = ticks;
	tp->irs = tp->rcv_wnd = tp->rcv_nxt = rcv_isn;
	toep->tp_delack_seq = toep->tp_rcv_wup = toep->tp_copied_seq = tp->irs;

	make_established(so, ntohl(req->snd_isn), ntohs(req->tcp_opt));
	
	/*
	 * Now that we finally have a TID send any CPL messages that we had to
	 * defer for lack of a TID.
	 */
	if (mbufq_len(&toep->out_of_order_queue))
		fixup_and_send_ofo(toep);

	if (__predict_false(so_state_get(so) & SS_NOFDREF)) {
		/*
		 * XXX does this even make sense?
		 */
		so_sorwakeup(so);
	}
	m_free(m);
#ifdef notyet
/*
 * XXX assume no write requests permitted while socket connection is
 * incomplete
 */
	/*
	 * Currently the send queue must be empty at this point because the
	 * socket layer does not send anything before a connection is
	 * established.  To be future proof though we handle the possibility
	 * that there are pending buffers to send (either TX_DATA or
	 * CLOSE_CON_REQ).  First we need to adjust the sequence number of the
	 * buffers according to the just learned write_seq, and then we send
	 * them on their way.
	 */
	fixup_pending_writeq_buffers(sk);
	if (t3_push_frames(so, 1))
		sk->sk_write_space(sk);
#endif

	toep->tp_state = tp->t_state;
	V_tcpstat.tcps_connects++;
				
}

/*
 * Process a CPL_ACT_ESTABLISH message.
 */
static int
do_act_establish(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct cpl_act_establish *req = cplhdr(m);
	unsigned int tid = GET_TID(req);
	unsigned int atid = G_PASS_OPEN_TID(ntohl(req->tos_tid));
	struct toepcb *toep = (struct toepcb *)ctx;
	struct tcpcb *tp = toep->tp_tp;
	struct socket *so; 
	struct toedev *tdev;
	struct tom_data *d;
	
	if (tp == NULL) {
		free_atid(cdev, atid);
		return (0);
	}
	inp_wlock(tp->t_inpcb);

	/*
	 * XXX
	 */
	so = inp_inpcbtosocket(tp->t_inpcb);
	tdev = toep->tp_toedev; /* blow up here if link was down */
	d = TOM_DATA(tdev);

	/*
	 * It's OK if the TID is currently in use, the owning socket may have
	 * backlogged its last CPL message(s).  Just take it away.
	 */
	toep->tp_tid = tid;
	toep->tp_tp = tp;
	so_insert_tid(d, toep, tid);
	free_atid(cdev, atid);
	toep->tp_qset = G_QNUM(ntohl(m->m_pkthdr.csum_data));

	socket_act_establish(so, m);
	inp_wunlock(tp->t_inpcb);
	CTR1(KTR_TOM, "do_act_establish tid=%u", toep->tp_tid);
	cxgb_log_tcb(cdev->adapter, toep->tp_tid);

	return (0);
}

/*
 * Process an acknowledgment of WR completion.  Advance snd_una and send the
 * next batch of work requests from the write queue.
 */
static void
wr_ack(struct toepcb *toep, struct mbuf *m)
{
	struct tcpcb *tp = toep->tp_tp;
	struct cpl_wr_ack *hdr = cplhdr(m);
	struct socket *so;
	unsigned int credits = ntohs(hdr->credits);
	u32 snd_una = ntohl(hdr->snd_una);
	int bytes = 0;
	struct sockbuf *snd;
	
	CTR2(KTR_SPARE2, "wr_ack: snd_una=%u credits=%d", snd_una, credits);

	inp_wlock(tp->t_inpcb);
	so = inp_inpcbtosocket(tp->t_inpcb);
	toep->tp_wr_avail += credits;
	if (toep->tp_wr_unacked > toep->tp_wr_max - toep->tp_wr_avail)
		toep->tp_wr_unacked = toep->tp_wr_max - toep->tp_wr_avail;

	while (credits) {
		struct mbuf *p = peek_wr(toep);
		
		if (__predict_false(!p)) {
			log(LOG_ERR, "%u WR_ACK credits for TID %u with "
			    "nothing pending, state %u wr_avail=%u\n",
			    credits, toep->tp_tid, tp->t_state, toep->tp_wr_avail);
			break;
		}
		CTR2(KTR_TOM,
			"wr_ack: p->credits=%d p->bytes=%d",
		    p->m_pkthdr.csum_data, p->m_pkthdr.len);
		KASSERT(p->m_pkthdr.csum_data != 0,
		    ("empty request still on list"));

		if (__predict_false(credits < p->m_pkthdr.csum_data)) {

#if DEBUG_WR > 1
			struct tx_data_wr *w = cplhdr(p);
			log(LOG_ERR,
			       "TID %u got %u WR credits, need %u, len %u, "
			       "main body %u, frags %u, seq # %u, ACK una %u,"
			       " ACK nxt %u, WR_AVAIL %u, WRs pending %u\n",
			       toep->tp_tid, credits, p->csum, p->len,
			       p->len - p->data_len, skb_shinfo(p)->nr_frags,
			       ntohl(w->sndseq), snd_una, ntohl(hdr->snd_nxt),
			    toep->tp_wr_avail, count_pending_wrs(tp) - credits);
#endif
			p->m_pkthdr.csum_data -= credits;
			break;
		} else {
			dequeue_wr(toep);
			credits -= p->m_pkthdr.csum_data;
			bytes += p->m_pkthdr.len;
			CTR3(KTR_TOM,
			    "wr_ack: done with wr of %d bytes remain credits=%d wr credits=%d",
			    p->m_pkthdr.len, credits, p->m_pkthdr.csum_data);
	
			m_free(p);
		}
	}

#if DEBUG_WR
	check_wr_invariants(tp);
#endif

	if (__predict_false(SEQ_LT(snd_una, tp->snd_una))) {
#if VALIDATE_SEQ
		struct tom_data *d = TOM_DATA(TOE_DEV(so));

		log(LOG_ERR "%s: unexpected sequence # %u in WR_ACK "
		    "for TID %u, snd_una %u\n", (&d->tdev)->name, snd_una,
		    toep->tp_tid, tp->snd_una);
#endif
		goto out_free;
	}

	if (tp->snd_una != snd_una) {
		tp->snd_una = snd_una;
		tp->ts_recent_age = ticks;
#ifdef notyet
		/*
		 * Keep ARP entry "minty fresh"
		 */
		dst_confirm(sk->sk_dst_cache);
#endif
		if (tp->snd_una == tp->snd_nxt)
			toep->tp_flags &= ~TP_TX_WAIT_IDLE;
	}

	snd = so_sockbuf_snd(so);
	if (bytes) {
		CTR1(KTR_SPARE2, "wr_ack: sbdrop(%d)", bytes);
		snd = so_sockbuf_snd(so);
		sockbuf_lock(snd);		
		sbdrop_locked(snd, bytes);
		so_sowwakeup_locked(so);
	}

	if (snd->sb_sndptroff < snd->sb_cc)
		t3_push_frames(so, 0);

out_free:
	inp_wunlock(tp->t_inpcb);
	m_free(m);
}

/*
 * Handler for TX_DATA_ACK CPL messages.
 */
static int
do_wr_ack(struct t3cdev *dev, struct mbuf *m, void *ctx)
{
	struct toepcb *toep = (struct toepcb *)ctx;

	VALIDATE_SOCK(so);

	wr_ack(toep, m);
	return 0;
}

/*
 * Handler for TRACE_PKT CPL messages.  Just sink these packets.
 */
static int
do_trace_pkt(struct t3cdev *dev, struct mbuf *m, void *ctx)
{
	m_freem(m);
	return 0;
}

/*
 * Reset a connection that is on a listener's SYN queue or accept queue,
 * i.e., one that has not had a struct socket associated with it.
 * Must be called from process context.
 *
 * Modeled after code in inet_csk_listen_stop().
 */
static void
t3_reset_listen_child(struct socket *child)
{
	struct tcpcb *tp = so_sototcpcb(child);
	
	t3_send_reset(tp->t_toe);
}


static void
t3_child_disconnect(struct socket *so, void *arg)
{
	struct tcpcb *tp = so_sototcpcb(so);
		
	if (tp->t_flags & TF_TOE) {
		inp_wlock(tp->t_inpcb);
		t3_reset_listen_child(so);
		inp_wunlock(tp->t_inpcb);
	}	
}

/*
 * Disconnect offloaded established but not yet accepted connections sitting
 * on a server's accept_queue.  We just send an ABORT_REQ at this point and
 * finish off the disconnect later as we may need to wait for the ABORT_RPL.
 */
void
t3_disconnect_acceptq(struct socket *listen_so)
{

	so_lock(listen_so);
	so_listeners_apply_all(listen_so, t3_child_disconnect, NULL);
	so_unlock(listen_so);
}

/*
 * Reset offloaded connections sitting on a server's syn queue.  As above
 * we send ABORT_REQ and finish off when we get ABORT_RPL.
 */

void
t3_reset_synq(struct listen_ctx *lctx)
{
	struct toepcb *toep;

	so_lock(lctx->lso);	
	while (!LIST_EMPTY(&lctx->synq_head)) {
		toep = LIST_FIRST(&lctx->synq_head);
		LIST_REMOVE(toep, synq_entry);
		toep->tp_tp = NULL;
		t3_send_reset(toep);
		cxgb_remove_tid(TOEP_T3C_DEV(toep), toep, toep->tp_tid);
		toepcb_release(toep);
	}
	so_unlock(lctx->lso); 
}


int
t3_setup_ppods(struct toepcb *toep, const struct ddp_gather_list *gl,
		   unsigned int nppods, unsigned int tag, unsigned int maxoff,
		   unsigned int pg_off, unsigned int color)
{
	unsigned int i, j, pidx;
	struct pagepod *p;
	struct mbuf *m;
	struct ulp_mem_io *req;
	unsigned int tid = toep->tp_tid;
	const struct tom_data *td = TOM_DATA(toep->tp_toedev);
	unsigned int ppod_addr = tag * PPOD_SIZE + td->ddp_llimit;

	CTR6(KTR_TOM, "t3_setup_ppods(gl=%p nppods=%u tag=%u maxoff=%u pg_off=%u color=%u)",
	    gl, nppods, tag, maxoff, pg_off, color);
	
	for (i = 0; i < nppods; ++i) {
		m = m_gethdr_nofail(sizeof(*req) + PPOD_SIZE);
		m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
		req = mtod(m, struct ulp_mem_io *);
		m->m_pkthdr.len = m->m_len = sizeof(*req) + PPOD_SIZE;
		req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_BYPASS));
		req->wr.wr_lo = 0;
		req->cmd_lock_addr = htonl(V_ULP_MEMIO_ADDR(ppod_addr >> 5) |
					   V_ULPTX_CMD(ULP_MEM_WRITE));
		req->len = htonl(V_ULP_MEMIO_DATA_LEN(PPOD_SIZE / 32) |
				 V_ULPTX_NFLITS(PPOD_SIZE / 8 + 1));

		p = (struct pagepod *)(req + 1);
		if (__predict_false(i < nppods - NUM_SENTINEL_PPODS)) {
			p->pp_vld_tid = htonl(F_PPOD_VALID | V_PPOD_TID(tid));
			p->pp_pgsz_tag_color = htonl(V_PPOD_TAG(tag) |
						  V_PPOD_COLOR(color));
			p->pp_max_offset = htonl(maxoff);
			p->pp_page_offset = htonl(pg_off);
			p->pp_rsvd = 0;
			for (pidx = 4 * i, j = 0; j < 5; ++j, ++pidx)
				p->pp_addr[j] = pidx < gl->dgl_nelem ?
				    htobe64(VM_PAGE_TO_PHYS(gl->dgl_pages[pidx])) : 0;
		} else
			p->pp_vld_tid = 0;   /* mark sentinel page pods invalid */
		send_or_defer(toep, m, 0);
		ppod_addr += PPOD_SIZE;
	}
	return (0);
}

/*
 * Build a CPL_BARRIER message as payload of a ULP_TX_PKT command.
 */
static inline void
mk_cpl_barrier_ulp(struct cpl_barrier *b)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)b;

	txpkt->cmd_dest = htonl(V_ULPTX_CMD(ULP_TXPKT));
	txpkt->len = htonl(V_ULPTX_NFLITS(sizeof(*b) / 8));
	b->opcode = CPL_BARRIER;
}

/*
 * Build a CPL_GET_TCB message as payload of a ULP_TX_PKT command.
 */
static inline void
mk_get_tcb_ulp(struct cpl_get_tcb *req, unsigned int tid, unsigned int cpuno)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)req;

	txpkt = (struct ulp_txpkt *)req;
	txpkt->cmd_dest = htonl(V_ULPTX_CMD(ULP_TXPKT));
	txpkt->len = htonl(V_ULPTX_NFLITS(sizeof(*req) / 8));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_GET_TCB, tid));
	req->cpuno = htons(cpuno);
}

/*
 * Build a CPL_SET_TCB_FIELD message as payload of a ULP_TX_PKT command.
 */
static inline void
mk_set_tcb_field_ulp(struct cpl_set_tcb_field *req, unsigned int tid,
                     unsigned int word, uint64_t mask, uint64_t val)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)req;
	
	CTR4(KTR_TCB, "mk_set_tcb_field_ulp(tid=%u word=0x%x mask=%jx val=%jx",
	    tid, word, mask, val);
	
	txpkt->cmd_dest = htonl(V_ULPTX_CMD(ULP_TXPKT));
	txpkt->len = htonl(V_ULPTX_NFLITS(sizeof(*req) / 8));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SET_TCB_FIELD, tid));
	req->reply = V_NO_REPLY(1);
	req->cpu_idx = 0;
	req->word = htons(word);
	req->mask = htobe64(mask);
	req->val = htobe64(val);
}

/*
 * Build a CPL_RX_DATA_ACK message as payload of a ULP_TX_PKT command.
 */
static void
mk_rx_data_ack_ulp(struct toepcb *toep, struct cpl_rx_data_ack *ack,
    unsigned int tid, unsigned int credits)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)ack;

	txpkt->cmd_dest = htonl(V_ULPTX_CMD(ULP_TXPKT));
	txpkt->len = htonl(V_ULPTX_NFLITS(sizeof(*ack) / 8));
	OPCODE_TID(ack) = htonl(MK_OPCODE_TID(CPL_RX_DATA_ACK, tid));
	ack->credit_dack = htonl(F_RX_MODULATE | F_RX_DACK_CHANGE |
	    V_RX_DACK_MODE(TOM_TUNABLE(toep->tp_toedev, delack)) |
				 V_RX_CREDITS(credits));
}

void
t3_cancel_ddpbuf(struct toepcb *toep, unsigned int bufidx)
{
	unsigned int wrlen;
	struct mbuf *m;
	struct work_request_hdr *wr;
	struct cpl_barrier *lock;
	struct cpl_set_tcb_field *req;
	struct cpl_get_tcb *getreq;
	struct ddp_state *p = &toep->tp_ddp_state;

#if 0
	SOCKBUF_LOCK_ASSERT(&toeptoso(toep)->so_rcv);
#endif
	wrlen = sizeof(*wr) + sizeof(*req) + 2 * sizeof(*lock) +
		sizeof(*getreq);
	m = m_gethdr_nofail(wrlen);
	m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
	wr = mtod(m, struct work_request_hdr *);
	bzero(wr, wrlen);
	
	wr->wr_hi = htonl(V_WR_OP(FW_WROPCODE_BYPASS));
	m->m_pkthdr.len = m->m_len = wrlen;

	lock = (struct cpl_barrier *)(wr + 1);
	mk_cpl_barrier_ulp(lock);

	req = (struct cpl_set_tcb_field *)(lock + 1);

	CTR1(KTR_TCB, "t3_cancel_ddpbuf(bufidx=%u)", bufidx);

	/* Hmmm, not sure if this actually a good thing: reactivating
	 * the other buffer might be an issue if it has been completed
	 * already. However, that is unlikely, since the fact that the UBUF
	 * is not completed indicates that there is no oustanding data.
	 */
	if (bufidx == 0)
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_FLAGS,
				     V_TF_DDP_ACTIVE_BUF(1) |
				     V_TF_DDP_BUF0_VALID(1),
				     V_TF_DDP_ACTIVE_BUF(1));
	else
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_FLAGS,
				     V_TF_DDP_ACTIVE_BUF(1) |
				     V_TF_DDP_BUF1_VALID(1), 0);

	getreq = (struct cpl_get_tcb *)(req + 1);
	mk_get_tcb_ulp(getreq, toep->tp_tid, toep->tp_qset);

	mk_cpl_barrier_ulp((struct cpl_barrier *)(getreq + 1));

	/* Keep track of the number of oustanding CPL_GET_TCB requests
	 */
	p->get_tcb_count++;
	
#ifdef T3_TRACE
	T3_TRACE1(TIDTB(so),
		  "t3_cancel_ddpbuf: bufidx %u", bufidx);
#endif
	cxgb_ofld_send(TOEP_T3C_DEV(toep), m);
}

/**
 * t3_overlay_ddpbuf - overlay an existing DDP buffer with a new one
 * @sk: the socket associated with the buffers
 * @bufidx: index of HW DDP buffer (0 or 1)
 * @tag0: new tag for HW buffer 0
 * @tag1: new tag for HW buffer 1
 * @len: new length for HW buf @bufidx
 *
 * Sends a compound WR to overlay a new DDP buffer on top of an existing
 * buffer by changing the buffer tag and length and setting the valid and
 * active flag accordingly.  The caller must ensure the new buffer is at
 * least as big as the existing one.  Since we typically reprogram both HW
 * buffers this function sets both tags for convenience. Read the TCB to
 * determine how made data was written into the buffer before the overlay
 * took place.
 */
void
t3_overlay_ddpbuf(struct toepcb *toep, unsigned int bufidx, unsigned int tag0,
	 	       unsigned int tag1, unsigned int len)
{
	unsigned int wrlen;
	struct mbuf *m;
	struct work_request_hdr *wr;
	struct cpl_get_tcb *getreq;
	struct cpl_set_tcb_field *req;
	struct ddp_state *p = &toep->tp_ddp_state;

	CTR4(KTR_TCB, "t3_setup_ppods(bufidx=%u tag0=%u tag1=%u len=%u)",
	    bufidx, tag0, tag1, len);
#if 0
	SOCKBUF_LOCK_ASSERT(&toeptoso(toep)->so_rcv);
#endif	
	wrlen = sizeof(*wr) + 3 * sizeof(*req) + sizeof(*getreq);
	m = m_gethdr_nofail(wrlen);
	m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
	wr = mtod(m, struct work_request_hdr *);
	m->m_pkthdr.len = m->m_len = wrlen;
	bzero(wr, wrlen);

	
	/* Set the ATOMIC flag to make sure that TP processes the following
	 * CPLs in an atomic manner and no wire segments can be interleaved.
	 */
	wr->wr_hi = htonl(V_WR_OP(FW_WROPCODE_BYPASS) | F_WR_ATOMIC);
	req = (struct cpl_set_tcb_field *)(wr + 1);
	mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_BUF0_TAG,
			     V_TCB_RX_DDP_BUF0_TAG(M_TCB_RX_DDP_BUF0_TAG) |
			     V_TCB_RX_DDP_BUF1_TAG(M_TCB_RX_DDP_BUF1_TAG) << 32,
			     V_TCB_RX_DDP_BUF0_TAG(tag0) |
			     V_TCB_RX_DDP_BUF1_TAG((uint64_t)tag1) << 32);
	req++;
	if (bufidx == 0) {
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_BUF0_LEN,
			    V_TCB_RX_DDP_BUF0_LEN(M_TCB_RX_DDP_BUF0_LEN),
			    V_TCB_RX_DDP_BUF0_LEN((uint64_t)len));
		req++;
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_FLAGS,
			    V_TF_DDP_PUSH_DISABLE_0(1) |
			    V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_ACTIVE_BUF(1),
			    V_TF_DDP_PUSH_DISABLE_0(0) |
			    V_TF_DDP_BUF0_VALID(1));
	} else {
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_BUF1_LEN,
			    V_TCB_RX_DDP_BUF1_LEN(M_TCB_RX_DDP_BUF1_LEN),
			    V_TCB_RX_DDP_BUF1_LEN((uint64_t)len));
		req++;
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_FLAGS,
			    V_TF_DDP_PUSH_DISABLE_1(1) |
			    V_TF_DDP_BUF1_VALID(1) | V_TF_DDP_ACTIVE_BUF(1),
			    V_TF_DDP_PUSH_DISABLE_1(0) |
			    V_TF_DDP_BUF1_VALID(1) | V_TF_DDP_ACTIVE_BUF(1));
	}

	getreq = (struct cpl_get_tcb *)(req + 1);
	mk_get_tcb_ulp(getreq, toep->tp_tid, toep->tp_qset);

	/* Keep track of the number of oustanding CPL_GET_TCB requests
	 */
	p->get_tcb_count++;

#ifdef T3_TRACE
	T3_TRACE4(TIDTB(sk),
		  "t3_overlay_ddpbuf: bufidx %u tag0 %u tag1 %u "
		  "len %d",
		  bufidx, tag0, tag1, len);
#endif
	cxgb_ofld_send(TOEP_T3C_DEV(toep), m);
}

/*
 * Sends a compound WR containing all the CPL messages needed to program the
 * two HW DDP buffers, namely optionally setting up the length and offset of
 * each buffer, programming the DDP flags, and optionally sending RX_DATA_ACK.
 */
void
t3_setup_ddpbufs(struct toepcb *toep, unsigned int len0, unsigned int offset0,
		      unsigned int len1, unsigned int offset1,
                      uint64_t ddp_flags, uint64_t flag_mask, int modulate)
{
	unsigned int wrlen;
	struct mbuf *m;
	struct work_request_hdr *wr;
	struct cpl_set_tcb_field *req;

	CTR6(KTR_TCB, "t3_setup_ddpbufs(len0=%u offset0=%u len1=%u offset1=%u ddp_flags=0x%08x%08x ",
	    len0, offset0, len1, offset1, ddp_flags >> 32, ddp_flags & 0xffffffff);
	
#if 0
	SOCKBUF_LOCK_ASSERT(&toeptoso(toep)->so_rcv);
#endif
	wrlen = sizeof(*wr) + sizeof(*req) + (len0 ? sizeof(*req) : 0) +
		(len1 ? sizeof(*req) : 0) +
		(modulate ? sizeof(struct cpl_rx_data_ack) : 0);
	m = m_gethdr_nofail(wrlen);
	m_set_priority(m, mkprio(CPL_PRIORITY_CONTROL, toep));
	wr = mtod(m, struct work_request_hdr *);
	bzero(wr, wrlen);
	
	wr->wr_hi = htonl(V_WR_OP(FW_WROPCODE_BYPASS));
	m->m_pkthdr.len = m->m_len = wrlen;

	req = (struct cpl_set_tcb_field *)(wr + 1);
	if (len0) {                  /* program buffer 0 offset and length */
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_BUF0_OFFSET,
			V_TCB_RX_DDP_BUF0_OFFSET(M_TCB_RX_DDP_BUF0_OFFSET) |
			V_TCB_RX_DDP_BUF0_LEN(M_TCB_RX_DDP_BUF0_LEN),
			V_TCB_RX_DDP_BUF0_OFFSET((uint64_t)offset0) |
			V_TCB_RX_DDP_BUF0_LEN((uint64_t)len0));
		req++;
	}
	if (len1) {                  /* program buffer 1 offset and length */
		mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_BUF1_OFFSET,
			V_TCB_RX_DDP_BUF1_OFFSET(M_TCB_RX_DDP_BUF1_OFFSET) |
			V_TCB_RX_DDP_BUF1_LEN(M_TCB_RX_DDP_BUF1_LEN) << 32,
			V_TCB_RX_DDP_BUF1_OFFSET((uint64_t)offset1) |
			V_TCB_RX_DDP_BUF1_LEN((uint64_t)len1) << 32);
		req++;
	}

	mk_set_tcb_field_ulp(req, toep->tp_tid, W_TCB_RX_DDP_FLAGS, flag_mask,
			     ddp_flags);

	if (modulate) {
		mk_rx_data_ack_ulp(toep,
		    (struct cpl_rx_data_ack *)(req + 1), toep->tp_tid,
		    toep->tp_copied_seq - toep->tp_rcv_wup);
		toep->tp_rcv_wup = toep->tp_copied_seq;
	}

#ifdef T3_TRACE
	T3_TRACE5(TIDTB(sk),
		  "t3_setup_ddpbufs: len0 %u len1 %u ddp_flags 0x%08x%08x "
		  "modulate %d",
		  len0, len1, ddp_flags >> 32, ddp_flags & 0xffffffff,
		  modulate);
#endif

	cxgb_ofld_send(TOEP_T3C_DEV(toep), m);
}

void
t3_init_wr_tab(unsigned int wr_len)
{
	int i;

	if (mbuf_wrs[1])     /* already initialized */
		return;

	for (i = 1; i < ARRAY_SIZE(mbuf_wrs); i++) {
		int sgl_len = (3 * i) / 2 + (i & 1);

		sgl_len += 3;
		mbuf_wrs[i] = sgl_len <= wr_len ?
		       	1 : 1 + (sgl_len - 2) / (wr_len - 1);
	}

	wrlen = wr_len * 8;
}

int
t3_init_cpl_io(void)
{
#ifdef notyet
	tcphdr_skb = alloc_skb(sizeof(struct tcphdr), GFP_KERNEL);
	if (!tcphdr_skb) {
		log(LOG_ERR,
		       "Chelsio TCP offload: can't allocate sk_buff\n");
		return -1;
	}
	skb_put(tcphdr_skb, sizeof(struct tcphdr));
	tcphdr_skb->h.raw = tcphdr_skb->data;
	memset(tcphdr_skb->data, 0, tcphdr_skb->len);
#endif
	
	t3tom_register_cpl_handler(CPL_ACT_ESTABLISH, do_act_establish);
	t3tom_register_cpl_handler(CPL_ACT_OPEN_RPL, do_act_open_rpl);
	t3tom_register_cpl_handler(CPL_TX_DMA_ACK, do_wr_ack);
	t3tom_register_cpl_handler(CPL_RX_DATA, do_rx_data);
	t3tom_register_cpl_handler(CPL_CLOSE_CON_RPL, do_close_con_rpl);
	t3tom_register_cpl_handler(CPL_PEER_CLOSE, do_peer_close);
	t3tom_register_cpl_handler(CPL_PASS_ESTABLISH, do_pass_establish);
	t3tom_register_cpl_handler(CPL_PASS_ACCEPT_REQ, do_pass_accept_req);
	t3tom_register_cpl_handler(CPL_ABORT_REQ_RSS, do_abort_req);
	t3tom_register_cpl_handler(CPL_ABORT_RPL_RSS, do_abort_rpl);
	t3tom_register_cpl_handler(CPL_RX_DATA_DDP, do_rx_data_ddp);
	t3tom_register_cpl_handler(CPL_RX_DDP_COMPLETE, do_rx_ddp_complete);
	t3tom_register_cpl_handler(CPL_RX_URG_NOTIFY, do_rx_urg_notify);
	t3tom_register_cpl_handler(CPL_TRACE_PKT, do_trace_pkt);
	t3tom_register_cpl_handler(CPL_GET_TCB_RPL, do_get_tcb_rpl);
	return (0);
}

