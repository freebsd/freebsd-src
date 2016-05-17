/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007-2008,2010
 *	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * Copyright (c) 2015 Netflix Inc.
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Lawrence Stewart,
 * James Healy and David Hayes, made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
 *
 * Portions of this software were developed by Randall R. Stewart while
 * working for Netflix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipfw.h"		/* for ipfw_fwd	*/
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/hhook.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/protosw.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/cpu.h>	/* before tcp_seq.h, for tcp_random18() */

#include <vm/uma.h>

#include <net/route.h>
#include <net/vnet.h>

#define TCPSTATES		/* for logging */

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* required for icmp_var.h */
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM */
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_syncache.h>
#include <netinet/cc/cc.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif /* TCPDEBUG */
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif /*IPSEC*/

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

VNET_DECLARE(int, tcp_autorcvbuf_inc);
#define	V_tcp_autorcvbuf_inc	VNET(tcp_autorcvbuf_inc)
VNET_DECLARE(int, tcp_autorcvbuf_max);
#define	V_tcp_autorcvbuf_max	VNET(tcp_autorcvbuf_max)
VNET_DECLARE(int, tcp_do_rfc3042);
#define	V_tcp_do_rfc3042	VNET(tcp_do_rfc3042)
VNET_DECLARE(int, tcp_do_autorcvbuf);
#define	V_tcp_do_autorcvbuf	VNET(tcp_do_autorcvbuf)
VNET_DECLARE(int, tcp_insecure_rst);
#define	V_tcp_insecure_rst	VNET(tcp_insecure_rst)
VNET_DECLARE(int, tcp_insecure_syn);
#define	V_tcp_insecure_syn	VNET(tcp_insecure_syn)

static void	 tcp_do_segment_fastslow(struct mbuf *, struct tcphdr *,
			struct socket *, struct tcpcb *, int, int, uint8_t,
			int);

static void	 tcp_do_segment_fastack(struct mbuf *, struct tcphdr *,
			struct socket *, struct tcpcb *, int, int, uint8_t,
			int);

/*
 * Indicate whether this ack should be delayed.  We can delay the ack if
 * following conditions are met:
 *	- There is no delayed ack timer in progress.
 *	- Our last ack wasn't a 0-sized window. We never want to delay
 *	  the ack that opens up a 0-sized window.
 *	- LRO wasn't used for this segment. We make sure by checking that the
 *	  segment size is not larger than the MSS.
 */
#define DELAY_ACK(tp, tlen)						\
	((!tcp_timer_active(tp, TT_DELACK) &&				\
	    (tp->t_flags & TF_RXWIN0SENT) == 0) &&			\
	    (tlen <= tp->t_maxseg) &&					\
	    (V_tcp_delack_enabled || (tp->t_flags & TF_NEEDSYN)))

/*
 * So how is this faster than the normal fast ack?
 * It basically allows us to also stay in the fastpath
 * when a window-update ack also arrives. In testing
 * we saw only 25-30% of connections doing fastpath 
 * due to the fact that along with moving forward
 * in sequence the window was also updated.
 */
static void
tcp_do_fastack(struct mbuf *m, struct tcphdr *th, struct socket *so,
	       struct tcpcb *tp, struct tcpopt *to, int drop_hdrlen, int tlen, 
	       int ti_locked, u_long tiwin)
{
	int acked;
	int winup_only=0;
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif
        /*
	 * The following if statement will be true if
	 * we are doing the win_up_in_fp <and>
	 * - We have more new data (SEQ_LT(tp->snd_wl1, th->th_seq)) <or>
	 * - No more new data, but we have an ack for new data
	 *   (tp->snd_wl1 == th->th_seq && SEQ_LT(tp->snd_wl2, th->th_ack))
	 * - No more new data, the same ack point but the window grew
	 *   (tp->snd_wl1 == th->th_seq && tp->snd_wl2 == th->th_ack && twin > tp->snd_wnd)
	 */
	if ((SEQ_LT(tp->snd_wl1, th->th_seq) ||
	     (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
					    (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd) {
			winup_only = 1;
			TCPSTAT_INC(tcps_rcvwinupd);
		}
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
	}
	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record the timestamp.
	 * NOTE that the test is modified according to the latest
	 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * This is a pure ack for outstanding data.
	 */
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}
	ti_locked = TI_UNLOCKED;

	TCPSTAT_INC(tcps_predack);

	/*
	 * "bad retransmit" recovery.
	 */
	if (tp->t_rxtshift == 1 &&
	    tp->t_flags & TF_PREVVALID &&
	    (int)(ticks - tp->t_badrxtwin) < 0) {
		cc_cong_signal(tp, th, CC_RTO_ERR);
	}

	/*
	 * Recalculate the transmit timer / rtt.
	 *
	 * Some boxes send broken timestamp replies
	 * during the SYN+ACK phase, ignore
	 * timestamps of 0 or we could calculate a
	 * huge RTT and blow up the retransmit timer.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    to->to_tsecr) {
		u_int t;

		t = tcp_ts_getticks() - to->to_tsecr;
		if (!tp->t_rttlow || tp->t_rttlow > t)
			tp->t_rttlow = t;
		tcp_xmit_timer(tp,
			       TCP_TS_TO_TICKS(t) + 1);
	} else if (tp->t_rtttime &&
		   SEQ_GT(th->th_ack, tp->t_rtseq)) {
		if (!tp->t_rttlow ||
		    tp->t_rttlow > ticks - tp->t_rtttime)
			tp->t_rttlow = ticks - tp->t_rtttime;
		tcp_xmit_timer(tp,
			       ticks - tp->t_rtttime);
	}
	if (winup_only == 0) {
		acked = BYTES_THIS_ACK(tp, th);

		/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
		hhook_run_tcp_est_in(tp, th, to);

		TCPSTAT_ADD(tcps_rcvackbyte, acked);
		sbdrop(&so->so_snd, acked);
		if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
				
		/*
		 * Let the congestion control algorithm update
		 * congestion control related information. This
		 * typically means increasing the congestion
		 * window.
		 */
		cc_ack_received(tp, th, CC_ACK);

		tp->snd_una = th->th_ack;
		/*
		 * Pull snd_wl2 up to prevent seq wrap relative
		 * to th_ack.
		 */
		tp->snd_wl2 = th->th_ack;
		tp->t_dupacks = 0;

		/*
		 * If all outstanding data are acked, stop
		 * retransmit timer, otherwise restart timer
		 * using current (possibly backed-off) value.
		 * If process is waiting for space,
		 * wakeup/selwakeup/signal.  If data
		 * are ready to send, let tcp_output
		 * decide between more output or persist.
		 */
#ifdef TCPDEBUG
		if (so->so_options & SO_DEBUG)
			tcp_trace(TA_INPUT, ostate, tp,
				  (void *)tcp_saveipgen,
				  &tcp_savetcp, 0);
#endif
		TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
		m_freem(m);
		if (tp->snd_una == tp->snd_max)
			tcp_timer_activate(tp, TT_REXMT, 0);
		else if (!tcp_timer_active(tp, TT_PERSIST))
			tcp_timer_activate(tp, TT_REXMT,
					   tp->t_rxtcur);
	} else {
		/* 
		 * Window update only, just free the mbufs and
		 * send out whatever we can.
		 */
		m_freem(m);
	}
	sowwakeup(so);
	if (sbavail(&so->so_snd))
		(void) tcp_output(tp);
	KASSERT(ti_locked == TI_UNLOCKED, ("%s: check_delack ti_locked %d",
					    __func__, ti_locked));
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, tcp_delacktime);
	}
	INP_WUNLOCK(tp->t_inpcb);
}

/*
 * Here nothing is really faster, its just that we
 * have broken out the fast-data path also just like
 * the fast-ack. 
 */
static void
tcp_do_fastnewdata(struct mbuf *m, struct tcphdr *th, struct socket *so,
		   struct tcpcb *tp, struct tcpopt *to, int drop_hdrlen, int tlen, 
		   int ti_locked, u_long tiwin)
{
	int newsize = 0;	/* automatic sockbuf scaling */
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif
	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record the timestamp.
	 * NOTE that the test is modified according to the latest
	 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}

	/*
	 * This is a pure, in-sequence data packet with
	 * nothing on the reassembly queue and we have enough
	 * buffer space to take it.
	 */
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}
	ti_locked = TI_UNLOCKED;

	/* Clean receiver SACK report if present */
	if ((tp->t_flags & TF_SACK_PERMIT) && tp->rcv_numsacks)
		tcp_clean_sackreport(tp);
	TCPSTAT_INC(tcps_preddat);
	tp->rcv_nxt += tlen;
	/*
	 * Pull snd_wl1 up to prevent seq wrap relative to
	 * th_seq.
	 */
	tp->snd_wl1 = th->th_seq;
	/*
	 * Pull rcv_up up to prevent seq wrap relative to
	 * rcv_nxt.
	 */
	tp->rcv_up = tp->rcv_nxt;
	TCPSTAT_ADD(tcps_rcvbyte, tlen);
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp,
			  (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
	TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
	/*
	 * Automatic sizing of receive socket buffer.  Often the send
	 * buffer size is not optimally adjusted to the actual network
	 * conditions at hand (delay bandwidth product).  Setting the
	 * buffer size too small limits throughput on links with high
	 * bandwidth and high delay (eg. trans-continental/oceanic links).
	 *
	 * On the receive side the socket buffer memory is only rarely
	 * used to any significant extent.  This allows us to be much
	 * more aggressive in scaling the receive socket buffer.  For
	 * the case that the buffer space is actually used to a large
	 * extent and we run out of kernel memory we can simply drop
	 * the new segments; TCP on the sender will just retransmit it
	 * later.  Setting the buffer size too big may only consume too
	 * much kernel memory if the application doesn't read() from
	 * the socket or packet loss or reordering makes use of the
	 * reassembly queue.
	 *
	 * The criteria to step up the receive buffer one notch are:
	 *  1. Application has not set receive buffer size with
	 *     SO_RCVBUF. Setting SO_RCVBUF clears SB_AUTOSIZE.
	 *  2. the number of bytes received during the time it takes
	 *     one timestamp to be reflected back to us (the RTT);
	 *  3. received bytes per RTT is within seven eighth of the
	 *     current socket buffer size;
	 *  4. receive buffer size has not hit maximal automatic size;
	 *
	 * This algorithm does one step per RTT at most and only if
	 * we receive a bulk stream w/o packet losses or reorderings.
	 * Shrinking the buffer during idle times is not necessary as
	 * it doesn't consume any memory when idle.
	 *
	 * TODO: Only step up if the application is actually serving
	 * the buffer to better manage the socket buffer resources.
	 */
	if (V_tcp_do_autorcvbuf &&
	    (to->to_flags & TOF_TS) &&
	    to->to_tsecr &&
	    (so->so_rcv.sb_flags & SB_AUTOSIZE)) {
		if (TSTMP_GT(to->to_tsecr, tp->rfbuf_ts) &&
		    to->to_tsecr - tp->rfbuf_ts < hz) {
			if (tp->rfbuf_cnt >
			    (so->so_rcv.sb_hiwat / 8 * 7) &&
			    so->so_rcv.sb_hiwat <
			    V_tcp_autorcvbuf_max) {
				newsize =
					min(so->so_rcv.sb_hiwat +
					    V_tcp_autorcvbuf_inc,
					    V_tcp_autorcvbuf_max);
			}
			/* Start over with next RTT. */
			tp->rfbuf_ts = 0;
			tp->rfbuf_cnt = 0;
		} else
			tp->rfbuf_cnt += tlen;	/* add up */
	}

	/* Add data to socket buffer. */
	SOCKBUF_LOCK(&so->so_rcv);
	if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
		m_freem(m);
	} else {
		/*
		 * Set new socket buffer size.
		 * Give up when limit is reached.
		 */
		if (newsize)
			if (!sbreserve_locked(&so->so_rcv,
					      newsize, so, NULL))
				so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
		m_adj(m, drop_hdrlen);	/* delayed header drop */
		sbappendstream_locked(&so->so_rcv, m, 0);
	}
	/* NB: sorwakeup_locked() does an implicit unlock. */
	sorwakeup_locked(so);
	if (DELAY_ACK(tp, tlen)) {
		tp->t_flags |= TF_DELACK;
	} else {
		tp->t_flags |= TF_ACKNOW;
		tcp_output(tp);
	}
	KASSERT(ti_locked == TI_UNLOCKED, ("%s: check_delack ti_locked %d",
					    __func__, ti_locked));
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, tcp_delacktime);
	}
	INP_WUNLOCK(tp->t_inpcb);
}

/*
 * The slow-path is the clone of the long long part
 * of tcp_do_segment past all the fast-path stuff. We
 * use it here by two different callers, the fast/slow and
 * the fastack only.
 */
static void
tcp_do_slowpath(struct mbuf *m, struct tcphdr *th, struct socket *so,
		struct tcpcb *tp, struct tcpopt *to, int drop_hdrlen, int tlen, 
		int ti_locked, u_long tiwin, int thflags)
{
	int  acked, ourfinisacked, needoutput = 0;
	int rstreason, todrop, win;
	char *s;
	struct in_conninfo *inc;
	struct mbuf *mfree = NULL;
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif
	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	inc = &tp->t_inpcb->inp_inc;
	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));

	/* Reset receive buffer auto scaling when not in bulk receive mode. */
	tp->rfbuf_ts = 0;
	tp->rfbuf_cnt = 0;

	switch (tp->t_state) {

	/*
	 * If the state is SYN_RECEIVED:
	 *	if seg contains an ACK, but not for our SYN/ACK, send a RST.
	 */
	case TCPS_SYN_RECEIVED:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->snd_una) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
		}
		break;

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if seg contains an ECE and ECN support is enabled, the stream
	 *	    is ECN capable.
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
			rstreason = BANDLIM_UNLIMITED;
			goto dropwithreset;
		}
		if ((thflags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) {
			TCP_PROBE5(connect__refused, NULL, tp,
			    mtod(m, const char *), tp, th);
			tp = tcp_drop(tp, ECONNREFUSED);
		}
		if (thflags & TH_RST)
			goto drop;
		if (!(thflags & TH_SYN))
			goto drop;

		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		if (thflags & TH_ACK) {
			TCPSTAT_INC(tcps_connects);
			soisconnected(so);
#ifdef MAC
			mac_socketpeer_set_from_mbuf(m, so);
#endif
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
			}
			tp->rcv_adv += imin(tp->rcv_wnd,
			    TCP_MAXWIN << tp->rcv_scale);
			tp->snd_una++;		/* SYN is acked */
			/*
			 * If there's data, delay ACK; if there's also a FIN
			 * ACKNOW will be turned on later.
			 */
			if (DELAY_ACK(tp, tlen) && tlen != 0)
				tcp_timer_activate(tp, TT_DELACK,
				    tcp_delacktime);
			else
				tp->t_flags |= TF_ACKNOW;

			if ((thflags & TH_ECE) && V_tcp_do_ecn) {
				tp->t_flags |= TF_ECN_PERMIT;
				TCPSTAT_INC(tcps_ecn_shs);
			}
			
			/*
			 * Received <SYN,ACK> in SYN_SENT[*] state.
			 * Transitions:
			 *	SYN_SENT  --> ESTABLISHED
			 *	SYN_SENT* --> FIN_WAIT_1
			 */
			tp->t_starttime = ticks;
			if (tp->t_flags & TF_NEEDFIN) {
				tcp_state_change(tp, TCPS_FIN_WAIT_1);
				tp->t_flags &= ~TF_NEEDFIN;
				thflags &= ~TH_SYN;
			} else {
				tcp_state_change(tp, TCPS_ESTABLISHED);
				TCP_PROBE5(connect__established, NULL, tp,
				    mtod(m, const char *), tp, th);
				cc_conn_init(tp);
				tcp_timer_activate(tp, TT_KEEP,
				    TP_KEEPIDLE(tp));
			}
		} else {
			/*
			 * Received initial SYN in SYN-SENT[*] state =>
			 * simultaneous open.
			 * If it succeeds, connection is * half-synchronized.
			 * Otherwise, do 3-way handshake:
			 *        SYN-SENT -> SYN-RECEIVED
			 *        SYN-SENT* -> SYN-RECEIVED*
			 */
			tp->t_flags |= (TF_ACKNOW | TF_NEEDSYN);
			tcp_timer_activate(tp, TT_REXMT, 0);
			tcp_state_change(tp, TCPS_SYN_RECEIVED);
		}

		KASSERT(ti_locked == TI_RLOCKED, ("%s: trimthenstep6: "
		    "ti_locked %d", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
		INP_WLOCK_ASSERT(tp->t_inpcb);

		/*
		 * Advance th->th_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
			m_adj(m, -todrop);
			tlen = tp->rcv_wnd;
			thflags &= ~TH_FIN;
			TCPSTAT_INC(tcps_rcvpackafterwin);
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		/*
		 * Client side of transaction: already sent SYN and data.
		 * If the remote host used T/TCP to validate the SYN,
		 * our data will be ACK'd; if so, enter normal data segment
		 * processing in the middle of step 5, ack processing.
		 * Otherwise, goto step 6.
		 */
		if (thflags & TH_ACK)
			goto process_ACK;

		goto step6;

	/*
	 * If the state is LAST_ACK or CLOSING or TIME_WAIT:
	 *      do normal processing.
	 *
	 * NB: Leftover from RFC1644 T/TCP.  Cases to be reused later.
	 */
	case TCPS_LAST_ACK:
	case TCPS_CLOSING:
		break;  /* continue normal processing */
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check the RST flag and sequence number since reset segments
	 * are exempt from the timestamp and connection count tests.  This
	 * fixes a bug introduced by the Stevens, vol. 2, p. 960 bugfix
	 * below which allowed reset segments in half the sequence space
	 * to fall though and be processed (which gives forged reset
	 * segments with a random sequence number a 50 percent chance of
	 * killing a connection).
	 * Then check timestamp, if present.
	 * Then check the connection count, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 */
	if (thflags & TH_RST) {
		/*
		 * RFC5961 Section 3.2
		 *
		 * - RST drops connection only if SEG.SEQ == RCV.NXT.
		 * - If RST is in window, we send challenge ACK.
		 *
		 * Note: to take into account delayed ACKs, we should
		 *   test against last_ack_sent instead of rcv_nxt.
		 * Note 2: we handle special case of closed window, not
		 *   covered by the RFC.
		 */
		if ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
		    (tp->rcv_wnd == 0 && tp->last_ack_sent == th->th_seq)) {
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
			KASSERT(ti_locked == TI_RLOCKED,
			    ("%s: TH_RST ti_locked %d, th %p tp %p",
			    __func__, ti_locked, th, tp));
			KASSERT(tp->t_state != TCPS_SYN_SENT,
			    ("%s: TH_RST for TCPS_SYN_SENT th %p tp %p",
			    __func__, th, tp));

			if (V_tcp_insecure_rst ||
			    tp->last_ack_sent == th->th_seq) {
				TCPSTAT_INC(tcps_drops);
				/* Drop the connection. */
				switch (tp->t_state) {
				case TCPS_SYN_RECEIVED:
					so->so_error = ECONNREFUSED;
					goto close;
				case TCPS_ESTABLISHED:
				case TCPS_FIN_WAIT_1:
				case TCPS_FIN_WAIT_2:
				case TCPS_CLOSE_WAIT:
					so->so_error = ECONNRESET;
				close:
					tcp_state_change(tp, TCPS_CLOSED);
					/* FALLTHROUGH */
				default:
					tp = tcp_close(tp);
				}
			} else {
				TCPSTAT_INC(tcps_badrst);
				/* Send challenge ACK. */
				tcp_respond(tp, mtod(m, void *), th, m,
				    tp->rcv_nxt, tp->snd_nxt, TH_ACK);
				tp->last_ack_sent = tp->rcv_nxt;
				m = NULL;
			}
		}
		goto drop;
	}

	/*
	 * RFC5961 Section 4.2
	 * Send challenge ACK for any SYN in synchronized state.
	 */
	if ((thflags & TH_SYN) && tp->t_state != TCPS_SYN_SENT) {
		KASSERT(ti_locked == TI_RLOCKED,
		    ("tcp_do_segment: TH_SYN ti_locked %d", ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);

		TCPSTAT_INC(tcps_badsyn);
		if (V_tcp_insecure_syn &&
		    SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) {
			tp = tcp_drop(tp, ECONNRESET);
			rstreason = BANDLIM_UNLIMITED;
		} else {
			/* Send challenge ACK. */
			tcp_respond(tp, mtod(m, void *), th, m, tp->rcv_nxt,
			    tp->snd_nxt, TH_ACK);
			tp->last_ack_sent = tp->rcv_nxt;
			m = NULL;
		}
		goto drop;
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to->to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to->to_tsval, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if (tcp_ts_getticks() - tp->ts_recent_age > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, tlen);
			TCPSTAT_INC(tcps_pawsdrop);
			if (tlen)
				goto dropafterack;
			goto drop;
		}
	}

	/*
	 * In the SYN-RECEIVED state, validate that the packet belongs to
	 * this connection before trimming the data to fit the receive
	 * window.  Check the sequence number versus IRS since we know
	 * the sequence numbers haven't wrapped.  This is a partial fix
	 * for the "LAND" DoS attack.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && SEQ_LT(th->th_seq, tp->irs)) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}

	todrop = tp->rcv_nxt - th->th_seq;
	if (todrop > 0) {
		if (thflags & TH_SYN) {
			thflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1)
				th->th_urp--;
			else
				thflags &= ~TH_URG;
			todrop--;
		}
		/*
		 * Following if statement from Stevens, vol. 2, p. 960.
		 */
		if (todrop > tlen
		    || (todrop == tlen && (thflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the window.
			 * At this point the FIN must be a duplicate or out
			 * of sequence; drop it.
			 */
			thflags &= ~TH_FIN;

			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			todrop = tlen;
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, todrop);
		} else {
			TCPSTAT_INC(tcps_rcvpartduppack);
			TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		drop_hdrlen += todrop;	/* drop from the top afterwards */
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			thflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tlen) {
		KASSERT(ti_locked == TI_RLOCKED, ("%s: SS_NOFDEREF && "
		    "CLOSE_WAIT && tlen ti_locked %d", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);

		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: %s: Received %d bytes of data "
			    "after socket was closed, "
			    "sending RST and removing tcpcb\n",
			    s, __func__, tcpstates[tp->t_state], tlen);
			free(s, M_TCPLOG);
		}
		tp = tcp_close(tp);
		TCPSTAT_INC(tcps_rcvafterclose);
		rstreason = BANDLIM_UNLIMITED;
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
	if (todrop > 0) {
		TCPSTAT_INC(tcps_rcvpackafterwin);
		if (todrop >= tlen) {
			TCPSTAT_ADD(tcps_rcvbyteafterwin, tlen);
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				TCPSTAT_INC(tcps_rcvwinprobe);
			} else
				goto dropafterack;
		} else
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		m_adj(m, -todrop);
		tlen -= todrop;
		thflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * NOTE: 
	 * 1) That the test incorporates suggestions from the latest
	 *    proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 * 2) That updating only on newer timestamps interferes with
	 *    our earlier PAWS tests, so this check should be solely
	 *    predicated on the sequence space of this segment.
	 * 3) That we modify the segment boundary check to be 
	 *        Last.ACK.Sent <= SEG.SEQ + SEG.Len  
	 *    instead of RFC1323's
	 *        Last.ACK.Sent < SEG.SEQ + SEG.Len,
	 *    This modified check allows us to overcome RFC1323's
	 *    limitations as described in Stevens TCP/IP Illustrated
	 *    Vol. 2 p.869. In such cases, we can still calculate the
	 *    RTT correctly when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		((thflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}

	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN
	 * flag is on (half-synchronized state), then queue data for
	 * later processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_state == TCPS_SYN_RECEIVED ||
		    (tp->t_flags & TF_NEEDSYN))
			goto step6;
		else if (tp->t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}

	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state, the ack ACKs our SYN, so enter
	 * ESTABLISHED state and continue processing.
	 * The ACK was checked above.
	 */
	case TCPS_SYN_RECEIVED:

		TCPSTAT_INC(tcps_connects);
		soisconnected(so);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->rcv_scale = tp->request_r_scale;
			tp->snd_wnd = tiwin;
		}
		/*
		 * Make transitions:
		 *      SYN-RECEIVED  -> ESTABLISHED
		 *      SYN-RECEIVED* -> FIN-WAIT-1
		 */
		tp->t_starttime = ticks;
		if (tp->t_flags & TF_NEEDFIN) {
			tcp_state_change(tp, TCPS_FIN_WAIT_1);
			tp->t_flags &= ~TF_NEEDFIN;
		} else {
			tcp_state_change(tp, TCPS_ESTABLISHED);
			TCP_PROBE5(accept__established, NULL, tp,
			    mtod(m, const char *), tp, th);
			cc_conn_init(tp);
			tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));
		}
		/*
		 * If segment contains data or ACK, will call tcp_reass()
		 * later; if not, do so now to pass queued data to user.
		 */
		if (tlen == 0 && (thflags & TH_FIN) == 0)
			(void) tcp_reass(tp, (struct tcphdr *)0, 0,
			    (struct mbuf *)0);
		tp->snd_wl1 = th->th_seq - 1;
		/* FALLTHROUGH */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			TCPSTAT_INC(tcps_rcvacktoomuch);
			goto dropafterack;
		}
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    ((to->to_flags & TOF_SACK) ||
		     !TAILQ_EMPTY(&tp->snd_holes)))
			tcp_sack_doack(tp, to, th->th_ack);
		else
			/*
			 * Reset the value so that previous (valid) value
			 * from the last ack with SACK doesn't get used.
			 */
			tp->sackhint.sacked_bytes = 0;

		/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
		hhook_run_tcp_est_in(tp, th, to);

		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			if (tlen == 0 && tiwin == tp->snd_wnd) {
				/*
				 * If this is the first time we've seen a
				 * FIN from the remote, this is not a
				 * duplicate and it needs to be processed
				 * normally.  This happens during a
				 * simultaneous close.
				 */
				if ((thflags & TH_FIN) &&
				    (TCPS_HAVERCVDFIN(tp->t_state) == 0)) {
					tp->t_dupacks = 0;
					break;
				}
				TCPSTAT_INC(tcps_rcvdupack);
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change and FIN isn't set),
				 * the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver)
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 *
				 * When using TCP ECN, notify the peer that
				 * we reduced the cwnd.
				 */
				if (!tcp_timer_active(tp, TT_REXMT) ||
				    th->th_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks > tcprexmtthresh ||
				     IN_FASTRECOVERY(tp->t_flags)) {
					cc_ack_received(tp, th, CC_DUPACK);
					if ((tp->t_flags & TF_SACK_PERMIT) &&
					    IN_FASTRECOVERY(tp->t_flags)) {
						int awnd;
						
						/*
						 * Compute the amount of data in flight first.
						 * We can inject new data into the pipe iff 
						 * we have less than 1/2 the original window's
						 * worth of data in flight.
						 */
						if (V_tcp_do_rfc6675_pipe)
							awnd = tcp_compute_pipe(tp);
						else
							awnd = (tp->snd_nxt - tp->snd_fack) +
								tp->sackhint.sack_bytes_rexmit;

						if (awnd < tp->snd_ssthresh) {
							tp->snd_cwnd += tp->t_maxseg;
							if (tp->snd_cwnd > tp->snd_ssthresh)
								tp->snd_cwnd = tp->snd_ssthresh;
						}
					} else
						tp->snd_cwnd += tp->t_maxseg;
					(void) tp->t_fb->tfb_tcp_output(tp);
					goto drop;
				} else if (tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;

					/*
					 * If we're doing sack, check to
					 * see if we're already in sack
					 * recovery. If we're not doing sack,
					 * check to see if we're in newreno
					 * recovery.
					 */
					if (tp->t_flags & TF_SACK_PERMIT) {
						if (IN_FASTRECOVERY(tp->t_flags)) {
							tp->t_dupacks = 0;
							break;
						}
					} else {
						if (SEQ_LEQ(th->th_ack,
						    tp->snd_recover)) {
							tp->t_dupacks = 0;
							break;
						}
					}
					/* Congestion signal before ack. */
					cc_cong_signal(tp, th, CC_NDUPACK);
					cc_ack_received(tp, th, CC_DUPACK);
					tcp_timer_activate(tp, TT_REXMT, 0);
					tp->t_rtttime = 0;
					if (tp->t_flags & TF_SACK_PERMIT) {
						TCPSTAT_INC(
						    tcps_sack_recovery_episode);
						tp->sack_newdata = tp->snd_nxt;
						tp->snd_cwnd = tp->t_maxseg;
						(void) tp->t_fb->tfb_tcp_output(tp);
						goto drop;
					}
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tp->t_fb->tfb_tcp_output(tp);
					KASSERT(tp->snd_limited <= 2,
					    ("%s: tp->snd_limited too big",
					    __func__));
					tp->snd_cwnd = tp->snd_ssthresh +
					     tp->t_maxseg *
					     (tp->t_dupacks - tp->snd_limited);
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (V_tcp_do_rfc3042) {
					/*
					 * Process first and second duplicate
					 * ACKs. Each indicates a segment
					 * leaving the network, creating room
					 * for more. Make sure we can send a
					 * packet on reception of each duplicate
					 * ACK by increasing snd_cwnd by one
					 * segment. Restore the original
					 * snd_cwnd after packet transmission.
					 */
					cc_ack_received(tp, th, CC_DUPACK);
					u_long oldcwnd = tp->snd_cwnd;
					tcp_seq oldsndmax = tp->snd_max;
					u_int sent;
					int avail;

					KASSERT(tp->t_dupacks == 1 ||
					    tp->t_dupacks == 2,
					    ("%s: dupacks not 1 or 2",
					    __func__));
					if (tp->t_dupacks == 1)
						tp->snd_limited = 0;
					tp->snd_cwnd =
					    (tp->snd_nxt - tp->snd_una) +
					    (tp->t_dupacks - tp->snd_limited) *
					    tp->t_maxseg;
					/*
					 * Only call tcp_output when there
					 * is new data available to be sent.
					 * Otherwise we would send pure ACKs.
					 */
					SOCKBUF_LOCK(&so->so_snd);
					avail = sbavail(&so->so_snd) -
					    (tp->snd_nxt - tp->snd_una);
					SOCKBUF_UNLOCK(&so->so_snd);
					if (avail > 0)
						(void) tp->t_fb->tfb_tcp_output(tp);
					sent = tp->snd_max - oldsndmax;
					if (sent > tp->t_maxseg) {
						KASSERT((tp->t_dupacks == 2 &&
						    tp->snd_limited == 0) ||
						   (sent == tp->t_maxseg + 1 &&
						    tp->t_flags & TF_SENTFIN),
						    ("%s: sent too much",
						    __func__));
						tp->snd_limited = 2;
					} else if (sent > 0)
						++tp->snd_limited;
					tp->snd_cwnd = oldcwnd;
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}

		KASSERT(SEQ_GT(th->th_ack, tp->snd_una),
		    ("%s: th_ack <= snd_una", __func__));

		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (IN_FASTRECOVERY(tp->t_flags)) {
			if (SEQ_LT(th->th_ack, tp->snd_recover)) {
				if (tp->t_flags & TF_SACK_PERMIT)
					tcp_sack_partialack(tp, th);
				else
					tcp_newreno_partial_ack(tp, th);
			} else
				cc_post_recovery(tp, th);
		}
		tp->t_dupacks = 0;
		/*
		 * If we reach this point, ACK is not a duplicate,
		 *     i.e., it ACKs something we sent.
		 */
		if (tp->t_flags & TF_NEEDSYN) {
			/*
			 * T/TCP: Connection was half-synchronized, and our
			 * SYN has been ACK'd (so connection is now fully
			 * synchronized).  Go to non-starred state,
			 * increment snd_una for ACK of SYN, and check if
			 * we can do window scaling.
			 */
			tp->t_flags &= ~TF_NEEDSYN;
			tp->snd_una++;
			/* Do window scaling? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
				/* Send window already scaled. */
			}
		}

process_ACK:
		INP_WLOCK_ASSERT(tp->t_inpcb);

		acked = BYTES_THIS_ACK(tp, th);
		TCPSTAT_INC(tcps_rcvackpack);
		TCPSTAT_ADD(tcps_rcvackbyte, acked);

		/*
		 * If we just performed our first retransmit, and the ACK
		 * arrives within our recovery window, then it was a mistake
		 * to do the retransmit in the first place.  Recover our
		 * original cwnd and ssthresh, and proceed to transmit where
		 * we left off.
		 */
		if (tp->t_rxtshift == 1 && tp->t_flags & TF_PREVVALID &&
		    (int)(ticks - tp->t_badrxtwin) < 0)
			cc_cong_signal(tp, th, CC_RTO_ERR);

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 *
		 * Some boxes send broken timestamp replies
		 * during the SYN+ACK phase, ignore
		 * timestamps of 0 or we could calculate a
		 * huge RTT and blow up the retransmit timer.
		 */
		if ((to->to_flags & TOF_TS) != 0 && to->to_tsecr) {
			u_int t;

			t = tcp_ts_getticks() - to->to_tsecr;
			if (!tp->t_rttlow || tp->t_rttlow > t)
				tp->t_rttlow = t;
			tcp_xmit_timer(tp, TCP_TS_TO_TICKS(t) + 1);
		} else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq)) {
			if (!tp->t_rttlow || tp->t_rttlow > ticks - tp->t_rtttime)
				tp->t_rttlow = ticks - tp->t_rtttime;
			tcp_xmit_timer(tp, ticks - tp->t_rtttime);
		}

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			tcp_timer_activate(tp, TT_REXMT, 0);
			needoutput = 1;
		} else if (!tcp_timer_active(tp, TT_PERSIST))
			tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);

		/*
		 * If no data (only SYN) was ACK'd,
		 *    skip rest of ACK processing.
		 */
		if (acked == 0)
			goto step6;

		/*
		 * Let the congestion control algorithm update congestion
		 * control related information. This typically means increasing
		 * the congestion window.
		 */
		cc_ack_received(tp, th, CC_ACK);

		SOCKBUF_LOCK(&so->so_snd);
		if (acked > sbavail(&so->so_snd)) {
			tp->snd_wnd -= sbavail(&so->so_snd);
			mfree = sbcut_locked(&so->so_snd,
			    (int)sbavail(&so->so_snd));
			ourfinisacked = 1;
		} else {
			mfree = sbcut_locked(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		/* NB: sowwakeup_locked() does an implicit unlock. */
		sowwakeup_locked(so);
		m_freem(mfree);
		/* Detect una wraparound. */
		if (!IN_RECOVERY(tp->t_flags) &&
		    SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
		/* XXXLAS: Can this be moved up into cc_post_recovery? */
		if (IN_RECOVERY(tp->t_flags) &&
		    SEQ_GEQ(th->th_ack, tp->snd_recover)) {
			EXIT_RECOVERY(tp->t_flags);
		}
		tp->snd_una = th->th_ack;
		if (tp->t_flags & TF_SACK_PERMIT) {
			if (SEQ_GT(tp->snd_una, tp->snd_recover))
				tp->snd_recover = tp->snd_una;
		}
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
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
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL,
					    (tcp_fast_finwait2_recycle ?
					    tcp_finwait2_timeout :
					    TP_MAXIDLE(tp)));
				}
				tcp_state_change(tp, TCPS_FIN_WAIT_2);
			}
			break;

		/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
				tcp_twstart(tp);
				INP_INFO_RUNLOCK(&V_tcbinfo);
				m_freem(m);
				return;
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
				tp = tcp_close(tp);
				goto drop;
			}
			break;
		}
	}

step6:
	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((thflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	     (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
			TCPSTAT_INC(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((thflags & TH_URG) && th->th_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
		SOCKBUF_LOCK(&so->so_rcv);
		if (th->th_urp + sbavail(&so->so_rcv) > sb_max) {
			th->th_urp = 0;			/* XXX */
			thflags &= ~TH_URG;		/* XXX */
			SOCKBUF_UNLOCK(&so->so_rcv);	/* XXX */
			goto dodata;			/* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side.
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(th->th_seq+th->th_urp, tp->rcv_up)) {
			tp->rcv_up = th->th_seq + th->th_urp;
			so->so_oobmark = sbavail(&so->so_rcv) +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_rcv.sb_state |= SBS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		SOCKBUF_UNLOCK(&so->so_rcv);
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (th->th_urp <= (u_long)tlen &&
		    !(so->so_options & SO_OOBINLINE)) {
			/* hdr drop is delayed */
			tcp_pulloutofband(so, th, m, drop_hdrlen);
		}
	} else {
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
	}
dodata:							/* XXX */
	INP_WLOCK_ASSERT(tp->t_inpcb);

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((tlen || (thflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		tcp_seq save_start = th->th_seq;
		m_adj(m, drop_hdrlen);	/* delayed header drop */
		/*
		 * Insert segment which includes th into TCP reassembly queue
		 * with control block tp.  Set thflags to whether reassembly now
		 * includes a segment with FIN.  This handles the common case
		 * inline (segment is the next to be received on an established
		 * connection, and the queue is empty), avoiding linkage into
		 * and removal from the queue and repetition of various
		 * conversions.
		 * Set DELACK for segments received in order, but ack
		 * immediately when segments are out of order (so
		 * fast retransmit can work).
		 */
		if (th->th_seq == tp->rcv_nxt &&
		    LIST_EMPTY(&tp->t_segq) &&
		    TCPS_HAVEESTABLISHED(tp->t_state)) {
			if (DELAY_ACK(tp, tlen))
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt += tlen;
			thflags = th->th_flags & TH_FIN;
			TCPSTAT_INC(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, tlen);
			SOCKBUF_LOCK(&so->so_rcv);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
				m_freem(m);
			else
				sbappendstream_locked(&so->so_rcv, m, 0);
			/* NB: sorwakeup_locked() does an implicit unlock. */
			sorwakeup_locked(so);
		} else {
			/*
			 * XXX: Due to the header drop above "th" is
			 * theoretically invalid by now.  Fortunately
			 * m_adj() doesn't actually frees any mbufs
			 * when trimming from the head.
			 */
			thflags = tcp_reass(tp, th, &tlen, m);
			tp->t_flags |= TF_ACKNOW;
		}
		if (tlen > 0 && (tp->t_flags & TF_SACK_PERMIT))
			tcp_update_sack_list(tp, save_start, save_start + tlen);
#if 0
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 * XXX: Unused.
		 */
		if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt))
			len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
		else
			len = so->so_rcv.sb_hiwat;
#endif
	} else {
		m_freem(m);
		thflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (thflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
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

		/*
		 * In SYN_RECEIVED and ESTABLISHED STATES
		 * enter the CLOSE_WAIT state.
		 */
		case TCPS_SYN_RECEIVED:
			tp->t_starttime = ticks;
			/* FALLTHROUGH */
		case TCPS_ESTABLISHED:
			tcp_state_change(tp, TCPS_CLOSE_WAIT);
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tcp_state_change(tp, TCPS_CLOSING);
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
			KASSERT(ti_locked == TI_RLOCKED, ("%s: dodata "
			    "TCP_FIN_WAIT_2 ti_locked: %d", __func__,
			    ti_locked));

			tcp_twstart(tp);
			INP_INFO_RUNLOCK(&V_tcbinfo);
			return;
		}
	}
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}
	ti_locked = TI_UNLOCKED;

#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tp->t_fb->tfb_tcp_output(tp);

	KASSERT(ti_locked == TI_UNLOCKED, ("%s: check_delack ti_locked %d",
	    __func__, ti_locked));
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, tcp_delacktime);
	}
	INP_WUNLOCK(tp->t_inpcb);
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 *
	 * We can now skip the test for the RST flag since all
	 * paths to this code happen after packets containing
	 * RST have been dropped.
	 *
	 * In the SYN-RECEIVED state, don't send an ACK unless the
	 * segment we received passes the SYN-RECEIVED ACK test.
	 * If it fails send a RST.  This breaks the loop in the
	 * "LAND" DoS attack, and also prevents an ACK storm
	 * between two listening ports that have been sent forged
	 * SYN segments, each with the source address of the other.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && (thflags & TH_ACK) &&
	    (SEQ_GT(tp->snd_una, th->th_ack) ||
	     SEQ_GT(th->th_ack, tp->snd_max)) ) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	TCP_PROBE3(debug__drop, tp, th, mtod(m, const char *));
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}
	ti_locked = TI_UNLOCKED;

	tp->t_flags |= TF_ACKNOW;
	(void) tp->t_fb->tfb_tcp_output(tp);
	INP_WUNLOCK(tp->t_inpcb);
	m_freem(m);
	return;

dropwithreset:
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}
	ti_locked = TI_UNLOCKED;

	if (tp != NULL) {
		tcp_dropwithreset(m, th, tp, tlen, rstreason);
		INP_WUNLOCK(tp->t_inpcb);
	} else
		tcp_dropwithreset(m, th, NULL, tlen, rstreason);
	return;

drop:
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
		ti_locked = TI_UNLOCKED;
	}
#ifdef INVARIANTS
	else
		INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
#endif

	/*
	 * Drop space held by incoming segment and return.
	 */
#ifdef TCPDEBUG
	if (tp == NULL || (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	TCP_PROBE3(debug__drop, tp, th, mtod(m, const char *));
	if (tp != NULL)
		INP_WUNLOCK(tp->t_inpcb);
	m_freem(m);
}


/*
 * Do fast slow is a combination of the original
 * tcp_dosegment and a split fastpath, one function
 * for the fast-ack which also includes allowing fastpath
 * for window advanced in sequence acks. And also a
 * sub-function that handles the insequence data.
 */
void
tcp_do_segment_fastslow(struct mbuf *m, struct tcphdr *th, struct socket *so,
			struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos,
			int ti_locked)
{
	int thflags;
	u_long tiwin;
	char *s;
	int can_enter;
	struct in_conninfo *inc;
	struct tcpopt to;

	thflags = th->th_flags;
	tp->sackhint.last_sack_ack = 0;
	inc = &tp->t_inpcb->inp_inc;
	/*
	 * If this is either a state-changing packet or current state isn't
	 * established, we require a write lock on tcbinfo.  Otherwise, we
	 * allow the tcbinfo to be in either alocked or unlocked, as the
	 * caller may have unnecessarily acquired a write lock due to a race.
	 */
	if ((thflags & (TH_SYN | TH_FIN | TH_RST)) != 0 ||
	    tp->t_state != TCPS_ESTABLISHED) {
		KASSERT(ti_locked == TI_RLOCKED, ("%s ti_locked %d for "
						  "SYN/FIN/RST/!EST", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	} else {
#ifdef INVARIANTS
		if (ti_locked == TI_RLOCKED) {
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
		} else {
			KASSERT(ti_locked == TI_UNLOCKED, ("%s: EST "
							   "ti_locked: %d", __func__, ti_locked));
			INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
		}
#endif
	}
	INP_WLOCK_ASSERT(tp->t_inpcb);
	KASSERT(tp->t_state > TCPS_LISTEN, ("%s: TCPS_LISTEN",
					    __func__));
	KASSERT(tp->t_state != TCPS_TIME_WAIT, ("%s: TCPS_TIME_WAIT",
						__func__));

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 * XXX: This should be done after segment
	 * validation to ignore broken/spoofed segs.
	 */
	tp->t_rcvtime = ticks;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));

	/*
	 * Unscale the window into a 32-bit value.
	 * For the SYN_SENT state the scale is zero.
	 */
	tiwin = th->th_win << tp->snd_scale;

	/*
	 * TCP ECN processing.
	 */
	if (tp->t_flags & TF_ECN_PERMIT) {
		if (thflags & TH_CWR)
			tp->t_flags &= ~TF_ECN_SND_ECE;
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->t_flags |= TF_ECN_SND_ECE;
			TCPSTAT_INC(tcps_ecn_ce);
			break;
		case IPTOS_ECN_ECT0:
			TCPSTAT_INC(tcps_ecn_ect0);
			break;
		case IPTOS_ECN_ECT1:
			TCPSTAT_INC(tcps_ecn_ect1);
			break;
		}
		/* Congestion experienced. */
		if (thflags & TH_ECE) {
			cc_cong_signal(tp, th, CC_ECN);
		}
	}

	/*
	 * Parse options on any incoming segment.
	 */
	tcp_dooptions(&to, (u_char *)(th + 1),
		      (th->th_off << 2) - sizeof(struct tcphdr),
		      (thflags & TH_SYN) ? TO_SYN : 0);

	/*
	 * If echoed timestamp is later than the current time,
	 * fall back to non RFC1323 RTT calculation.  Normalize
	 * timestamp if syncookies were used when this connection
	 * was established.
	 */
	if ((to.to_flags & TOF_TS) && (to.to_tsecr != 0)) {
		to.to_tsecr -= tp->ts_offset;
		if (TSTMP_GT(to.to_tsecr, tcp_ts_getticks()))
			to.to_tsecr = 0;
	}
	/*
	 * If timestamps were negotiated during SYN/ACK they should
	 * appear on every segment during this session and vice versa.
	 */
	if ((tp->t_flags & TF_RCVD_TSTMP) && !(to.to_flags & TOF_TS)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Timestamp missing, "
			    "no action\n", s, __func__);
			free(s, M_TCPLOG);
		}
	}
	if (!(tp->t_flags & TF_RCVD_TSTMP) && (to.to_flags & TOF_TS)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Timestamp not expected, "
			    "no action\n", s, __func__);
			free(s, M_TCPLOG);
		}
	}

	/*
	 * Process options only when we get SYN/ACK back. The SYN case
	 * for incoming connections is handled in tcp_syncache.
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN>
	 * or <SYN,ACK>) segment itself is never scaled.
	 * XXX this is traditional behavior, may need to be cleaned up.
	 */
	if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
		if ((to.to_flags & TOF_SCALE) &&
		    (tp->t_flags & TF_REQ_SCALE)) {
			tp->t_flags |= TF_RCVD_SCALE;
			tp->snd_scale = to.to_wscale;
		}
		/*
		 * Initial send window.  It will be updated with
		 * the next incoming segment to the scaled value.
		 */
		tp->snd_wnd = th->th_win;
		if (to.to_flags & TOF_TS) {
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = to.to_tsval;
			tp->ts_recent_age = tcp_ts_getticks();
		}
		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    (to.to_flags & TOF_SACKPERM) == 0)
			tp->t_flags &= ~TF_SACK_PERMIT;
	}
	can_enter = 0;
	if (__predict_true((tlen == 0))) {
		/*
		 * The ack moved forward and we have a window (non-zero)
		 * <or>
		 * The ack did not move forward, but the window increased.
		 */
		if (__predict_true((SEQ_GT(th->th_ack, tp->snd_una) && tiwin) ||
				   ((th->th_ack == tp->snd_una) && tiwin && (tiwin > tp->snd_wnd)))) {
			can_enter = 1;
		}
	} else {
		/* 
		 * Data incoming, use the old entry criteria
		 * for fast-path with data.
		 */
		if ((tiwin && tiwin == tp->snd_wnd)) {
			can_enter = 1;
		}
	}
	/*
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 * Make sure that the hidden state-flags are also off.
	 * Since we check for TCPS_ESTABLISHED first, it can only
	 * be TH_NEEDSYN.
	 */
	if (__predict_true(tp->t_state == TCPS_ESTABLISHED &&
	    th->th_seq == tp->rcv_nxt &&
	    (thflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    tp->snd_nxt == tp->snd_max &&
	    can_enter &&
	    ((tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN)) == 0) &&
	    LIST_EMPTY(&tp->t_segq) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	     TSTMP_GEQ(to.to_tsval, tp->ts_recent)))) {
		if (__predict_true((tlen == 0) &&
		    (SEQ_LEQ(th->th_ack, tp->snd_max) &&
		     !IN_RECOVERY(tp->t_flags) &&
		     (to.to_flags & TOF_SACK) == 0 &&
		     TAILQ_EMPTY(&tp->snd_holes)))) {
			/* We are done */
			tcp_do_fastack(m, th, so, tp, &to, drop_hdrlen, tlen, 
				       ti_locked, tiwin);
			return;
		} else if ((tlen) &&
			   (th->th_ack == tp->snd_una &&
			    tlen <= sbspace(&so->so_rcv))) {
			tcp_do_fastnewdata(m, th, so, tp, &to, drop_hdrlen, tlen, 
					   ti_locked, tiwin);
			/* We are done */
			return;
		}
	}
	tcp_do_slowpath(m, th, so, tp, &to, drop_hdrlen, tlen,
			ti_locked, tiwin, thflags);
}


/*
 * This subfunction is used to try to highly optimize the
 * fast path. We again allow window updates that are
 * in sequence to remain in the fast-path. We also add
 * in the __predict's to attempt to help the compiler.
 * Note that if we return a 0, then we can *not* process
 * it and the caller should push the packet into the 
 * slow-path.
 */
static int
tcp_fastack(struct mbuf *m, struct tcphdr *th, struct socket *so,
	       struct tcpcb *tp, struct tcpopt *to, int drop_hdrlen, int tlen, 
	       int ti_locked, u_long tiwin)
{
	int acked;
	int winup_only=0;
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif


	if (__predict_false(SEQ_LEQ(th->th_ack, tp->snd_una))) {
		/* Old ack, behind (or duplicate to) the last one rcv'd */
		return (0);
	}
	if (__predict_false(th->th_ack == tp->snd_una) && 
	    __predict_false(tiwin <= tp->snd_wnd)) {
		/* duplicate ack <or> a shrinking dup ack with shrinking window */
		return (0);
	}
	if (__predict_false(tiwin == 0)) {
		/* zero window */
		return (0);
	}
	if (__predict_false(SEQ_GT(th->th_ack, tp->snd_max))) {
		/* Above what we have sent? */
		return (0);
	}
	if (__predict_false(tp->snd_nxt != tp->snd_max)) {
		/* We are retransmitting */
		return (0);
	}
	if (__predict_false(tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN))) {
		/* We need a SYN or a FIN, unlikely.. */
		return (0);
	}
	if((to->to_flags & TOF_TS) && __predict_false(TSTMP_LT(to->to_tsval, tp->ts_recent))) {
		/* Timestamp is behind .. old ack with seq wrap? */
		return (0);
	}
	if (__predict_false(IN_RECOVERY(tp->t_flags))) {
		/* Still recovering */
		return (0);
	}
	if (__predict_false(to->to_flags & TOF_SACK)) {
		/* Sack included in the ack..  */
		return (0);
	}
	if (!TAILQ_EMPTY(&tp->snd_holes)) {
		/* We have sack holes on our scoreboard */
		return (0);
	}
	/* Ok if we reach here, we can process a fast-ack */

	/* Did the window get updated? */
	if (tiwin != tp->snd_wnd) {
		/* keep track of pure window updates */
		if (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd) {
			winup_only = 1;
			TCPSTAT_INC(tcps_rcvwinupd);
		}
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
	}
	/*
	 * Pull snd_wl2 up to prevent seq wrap relative
	 * to th_ack.
	 */
	tp->snd_wl2 = th->th_ack;
	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record the timestamp.
	 * NOTE that the test is modified according to the latest
	 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to->to_tsval;
	}
	/*
	 * This is a pure ack for outstanding data.
	 */
	if (ti_locked == TI_RLOCKED) {
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}
	ti_locked = TI_UNLOCKED;

	TCPSTAT_INC(tcps_predack);

	/*
	 * "bad retransmit" recovery.
	 */
	if (tp->t_rxtshift == 1 &&
	    tp->t_flags & TF_PREVVALID &&
	    (int)(ticks - tp->t_badrxtwin) < 0) {
		cc_cong_signal(tp, th, CC_RTO_ERR);
	}

	/*
	 * Recalculate the transmit timer / rtt.
	 *
	 * Some boxes send broken timestamp replies
	 * during the SYN+ACK phase, ignore
	 * timestamps of 0 or we could calculate a
	 * huge RTT and blow up the retransmit timer.
	 */
	if ((to->to_flags & TOF_TS) != 0 &&
	    to->to_tsecr) {
		u_int t;

		t = tcp_ts_getticks() - to->to_tsecr;
		if (!tp->t_rttlow || tp->t_rttlow > t)
			tp->t_rttlow = t;
		tcp_xmit_timer(tp,
			       TCP_TS_TO_TICKS(t) + 1);
	} else if (tp->t_rtttime &&
		   SEQ_GT(th->th_ack, tp->t_rtseq)) {
		if (!tp->t_rttlow ||
		    tp->t_rttlow > ticks - tp->t_rtttime)
			tp->t_rttlow = ticks - tp->t_rtttime;
		tcp_xmit_timer(tp,
			       ticks - tp->t_rtttime);
	}
	if (winup_only == 0) {
		acked = BYTES_THIS_ACK(tp, th);

		/* Run HHOOK_TCP_ESTABLISHED_IN helper hooks. */
		hhook_run_tcp_est_in(tp, th, to);

		TCPSTAT_ADD(tcps_rcvackbyte, acked);
		sbdrop(&so->so_snd, acked);
		if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
				
		/*
		 * Let the congestion control algorithm update
		 * congestion control related information. This
		 * typically means increasing the congestion
		 * window.
		 */
		cc_ack_received(tp, th, CC_ACK);

		tp->snd_una = th->th_ack;
		tp->t_dupacks = 0;

		/*
		 * If all outstanding data are acked, stop
		 * retransmit timer, otherwise restart timer
		 * using current (possibly backed-off) value.
		 * If process is waiting for space,
		 * wakeup/selwakeup/signal.  If data
		 * are ready to send, let tcp_output
		 * decide between more output or persist.
		 */
#ifdef TCPDEBUG
		if (so->so_options & SO_DEBUG)
			tcp_trace(TA_INPUT, ostate, tp,
				  (void *)tcp_saveipgen,
				  &tcp_savetcp, 0);
#endif
		TCP_PROBE3(debug__input, tp, th, mtod(m, const char *));
		m_freem(m);
		if (tp->snd_una == tp->snd_max)
			tcp_timer_activate(tp, TT_REXMT, 0);
		else if (!tcp_timer_active(tp, TT_PERSIST))
			tcp_timer_activate(tp, TT_REXMT,
					   tp->t_rxtcur);
		/* Wake up the socket if we have room to write more */
		sowwakeup(so);
	} else {
		/* 
		 * Window update only, just free the mbufs and
		 * send out whatever we can.
		 */
		m_freem(m);
	}
	if (sbavail(&so->so_snd))
		(void) tcp_output(tp);
	KASSERT(ti_locked == TI_UNLOCKED, ("%s: check_delack ti_locked %d",
					    __func__, ti_locked));
	INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
	INP_WLOCK_ASSERT(tp->t_inpcb);

	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, tcp_delacktime);
	}
	INP_WUNLOCK(tp->t_inpcb);
	return (1);
}

/*
 * This tcp-do-segment concentrates on making the fastest
 * ack processing path. It does not have a fast-path for
 * data (it possibly could which would then eliminate the
 * need for fast-slow above). For a content distributor having
 * large outgoing elephants and very very little coming in
 * having no fastpath for data does not really help (since you
 * don't get much data in). The most important thing is 
 * processing ack's quickly and getting the rest of the data
 * output to the peer as quickly as possible. This routine
 * seems to be about an overall 3% faster then the old
 * tcp_do_segment and keeps us in the fast-path for packets
 * much more (by allowing window updates to also stay in the fastpath).
 */
void
tcp_do_segment_fastack(struct mbuf *m, struct tcphdr *th, struct socket *so,
		       struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos,
		       int ti_locked)
{
	int thflags;
	u_long tiwin;
	char *s;
	struct in_conninfo *inc;
	struct tcpopt to;

	thflags = th->th_flags;
	tp->sackhint.last_sack_ack = 0;
	inc = &tp->t_inpcb->inp_inc;
	/*
	 * If this is either a state-changing packet or current state isn't
	 * established, we require a write lock on tcbinfo.  Otherwise, we
	 * allow the tcbinfo to be in either alocked or unlocked, as the
	 * caller may have unnecessarily acquired a write lock due to a race.
	 */
	if ((thflags & (TH_SYN | TH_FIN | TH_RST)) != 0 ||
	    tp->t_state != TCPS_ESTABLISHED) {
		KASSERT(ti_locked == TI_RLOCKED, ("%s ti_locked %d for "
						  "SYN/FIN/RST/!EST", __func__, ti_locked));
		INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
	} else {
#ifdef INVARIANTS
		if (ti_locked == TI_RLOCKED) {
			INP_INFO_RLOCK_ASSERT(&V_tcbinfo);
		} else {
			KASSERT(ti_locked == TI_UNLOCKED, ("%s: EST "
							   "ti_locked: %d", __func__, ti_locked));
			INP_INFO_UNLOCK_ASSERT(&V_tcbinfo);
		}
#endif
	}
	INP_WLOCK_ASSERT(tp->t_inpcb);
	KASSERT(tp->t_state > TCPS_LISTEN, ("%s: TCPS_LISTEN",
					    __func__));
	KASSERT(tp->t_state != TCPS_TIME_WAIT, ("%s: TCPS_TIME_WAIT",
						__func__));

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 * XXX: This should be done after segment
	 * validation to ignore broken/spoofed segs.
	 */
	tp->t_rcvtime = ticks;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));

	/*
	 * Unscale the window into a 32-bit value.
	 * For the SYN_SENT state the scale is zero.
	 */
	tiwin = th->th_win << tp->snd_scale;

	/*
	 * TCP ECN processing.
	 */
	if (tp->t_flags & TF_ECN_PERMIT) {
		if (thflags & TH_CWR)
			tp->t_flags &= ~TF_ECN_SND_ECE;
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->t_flags |= TF_ECN_SND_ECE;
			TCPSTAT_INC(tcps_ecn_ce);
			break;
		case IPTOS_ECN_ECT0:
			TCPSTAT_INC(tcps_ecn_ect0);
			break;
		case IPTOS_ECN_ECT1:
			TCPSTAT_INC(tcps_ecn_ect1);
			break;
		}
		/* Congestion experienced. */
		if (thflags & TH_ECE) {
			cc_cong_signal(tp, th, CC_ECN);
		}
	}

	/*
	 * Parse options on any incoming segment.
	 */
	tcp_dooptions(&to, (u_char *)(th + 1),
		      (th->th_off << 2) - sizeof(struct tcphdr),
		      (thflags & TH_SYN) ? TO_SYN : 0);

	/*
	 * If echoed timestamp is later than the current time,
	 * fall back to non RFC1323 RTT calculation.  Normalize
	 * timestamp if syncookies were used when this connection
	 * was established.
	 */
	if ((to.to_flags & TOF_TS) && (to.to_tsecr != 0)) {
		to.to_tsecr -= tp->ts_offset;
		if (TSTMP_GT(to.to_tsecr, tcp_ts_getticks()))
			to.to_tsecr = 0;
	}
	/*
	 * If timestamps were negotiated during SYN/ACK they should
	 * appear on every segment during this session and vice versa.
	 */
	if ((tp->t_flags & TF_RCVD_TSTMP) && !(to.to_flags & TOF_TS)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Timestamp missing, "
			    "no action\n", s, __func__);
			free(s, M_TCPLOG);
		}
	}
	if (!(tp->t_flags & TF_RCVD_TSTMP) && (to.to_flags & TOF_TS)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Timestamp not expected, "
			    "no action\n", s, __func__);
			free(s, M_TCPLOG);
		}
	}

	/*
	 * Process options only when we get SYN/ACK back. The SYN case
	 * for incoming connections is handled in tcp_syncache.
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN>
	 * or <SYN,ACK>) segment itself is never scaled.
	 * XXX this is traditional behavior, may need to be cleaned up.
	 */
	if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
		if ((to.to_flags & TOF_SCALE) &&
		    (tp->t_flags & TF_REQ_SCALE)) {
			tp->t_flags |= TF_RCVD_SCALE;
			tp->snd_scale = to.to_wscale;
		}
		/*
		 * Initial send window.  It will be updated with
		 * the next incoming segment to the scaled value.
		 */
		tp->snd_wnd = th->th_win;
		if (to.to_flags & TOF_TS) {
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = to.to_tsval;
			tp->ts_recent_age = tcp_ts_getticks();
		}
		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    (to.to_flags & TOF_SACKPERM) == 0)
			tp->t_flags &= ~TF_SACK_PERMIT;
	}
	/*
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 * Make sure that the hidden state-flags are also off.
	 * Since we check for TCPS_ESTABLISHED first, it can only
	 * be TH_NEEDSYN.
	 */
	if (__predict_true(tp->t_state == TCPS_ESTABLISHED) &&
	    __predict_true(((to.to_flags & TOF_SACK) == 0)) &&
	    __predict_true(tlen == 0) &&
	    __predict_true((thflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK) &&
	    __predict_true(LIST_EMPTY(&tp->t_segq)) &&
	    __predict_true(th->th_seq == tp->rcv_nxt)) {
		    if (tcp_fastack(m, th, so, tp, &to, drop_hdrlen, tlen, 
				    ti_locked, tiwin)) {
			    return;
		    }
	} 
	tcp_do_slowpath(m, th, so, tp, &to, drop_hdrlen, tlen,
			ti_locked, tiwin, thflags);
}

struct tcp_function_block __tcp_fastslow = {
	.tfb_tcp_block_name = "fastslow",
	.tfb_tcp_output = tcp_output,
	.tfb_tcp_do_segment = tcp_do_segment_fastslow,
	.tfb_tcp_ctloutput = tcp_default_ctloutput,
};

struct tcp_function_block __tcp_fastack = {
	.tfb_tcp_block_name = "fastack",
	.tfb_tcp_output = tcp_output,
	.tfb_tcp_do_segment = tcp_do_segment_fastack,
	.tfb_tcp_ctloutput = tcp_default_ctloutput
};

static int
tcp_addfastpaths(module_t mod, int type, void *data)
{
	int err=0;

	switch (type) {
	case MOD_LOAD:
		err = register_tcp_functions(&__tcp_fastack, M_WAITOK);
		if (err) {
			printf("Failed to register fastack module -- err:%d\n", err);
			return(err);
		}
		err = register_tcp_functions(&__tcp_fastslow, M_WAITOK); 
		if (err) {
			printf("Failed to register fastslow module -- err:%d\n", err);
			deregister_tcp_functions(&__tcp_fastack);
			return(err);
		}
		break;
	case MOD_QUIESCE:
		if ((__tcp_fastslow.tfb_refcnt) ||( __tcp_fastack.tfb_refcnt)) {
			return(EBUSY);
		}
		break;
	case MOD_UNLOAD:
		err = deregister_tcp_functions(&__tcp_fastack);
		if (err == EBUSY)
			break;
		err = deregister_tcp_functions(&__tcp_fastslow);
		if (err == EBUSY)
			break;
		err = 0;
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (err);
}

static moduledata_t new_tcp_fastpaths = {
	.name = "tcp_fastpaths",
	.evhand = tcp_addfastpaths,
	.priv = 0
};

MODULE_VERSION(kern_tcpfastpaths, 1);
DECLARE_MODULE(kern_tcpfastpaths, new_tcp_fastpaths, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
