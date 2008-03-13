/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tcp_output.c	8.4 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mac.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
#include <netinet/tcp.h>
#define	TCPOUTFLAGS
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif

#ifdef IPSEC
#include <netipsec/ipsec.h>
#endif /*IPSEC*/

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

#ifdef notyet
extern struct mbuf *m_copypack();
#endif

int path_mtu_discovery = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, path_mtu_discovery, CTLFLAG_RW,
	&path_mtu_discovery, 1, "Enable Path MTU Discovery");

int ss_fltsz = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, slowstart_flightsize, CTLFLAG_RW,
	&ss_fltsz, 1, "Slow start flight size");

int ss_fltsz_local = 4;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, local_slowstart_flightsize, CTLFLAG_RW,
	&ss_fltsz_local, 1, "Slow start flight size for local networks");

int     tcp_do_newreno = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, newreno, CTLFLAG_RW,
	&tcp_do_newreno, 0, "Enable NewReno Algorithms");

int	tcp_do_tso = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, tso, CTLFLAG_RW,
	&tcp_do_tso, 0, "Enable TCP Segmentation Offload");

int	tcp_do_autosndbuf = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, sendbuf_auto, CTLFLAG_RW,
	&tcp_do_autosndbuf, 0, "Enable automatic send buffer sizing");

int	tcp_autosndbuf_inc = 8*1024;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, sendbuf_inc, CTLFLAG_RW,
	&tcp_autosndbuf_inc, 0, "Incrementor step size of automatic send buffer");

int	tcp_autosndbuf_max = 256*1024;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, sendbuf_max, CTLFLAG_RW,
	&tcp_autosndbuf_max, 0, "Max size of automatic send buffer");


/*
 * Tcp output routine: figure out what should be sent and send it.
 */
int
tcp_output(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	long len, recwin, sendwin;
	int off, flags, error;
	struct mbuf *m;
	struct ip *ip = NULL;
	struct ipovly *ipov = NULL;
	struct tcphdr *th;
	u_char opt[TCP_MAXOLEN];
	unsigned ipoptlen, optlen, hdrlen;
#ifdef IPSEC
	unsigned ipsec_optlen = 0;
#endif
	int idle, sendalot;
	int sack_rxmit, sack_bytes_rxmt;
	struct sackhole *p;
	int tso = 0;
	struct tcpopt to;
#if 0
	int maxburst = TCP_MAXBURST;
#endif
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
	int isipv6;

	isipv6 = (tp->t_inpcb->inp_vflag & INP_IPV6) != 0;
#endif

	INP_LOCK_ASSERT(tp->t_inpcb);

	/*
	 * Determine length of data that should be transmitted,
	 * and flags that will be used.
	 * If there is some data or critical controls (SYN, RST)
	 * to send, then transmit; otherwise, investigate further.
	 */
	idle = (tp->t_flags & TF_LASTIDLE) || (tp->snd_max == tp->snd_una);
	if (idle && (ticks - tp->t_rcvtime) >= tp->t_rxtcur) {
		/*
		 * We have been idle for "a while" and no acks are
		 * expected to clock out any data we send --
		 * slow start to get ack "clock" running again.
		 *
		 * Set the slow-start flight size depending on whether
		 * this is a local network or not.
		 */
		int ss = ss_fltsz;
#ifdef INET6
		if (isipv6) {
			if (in6_localaddr(&tp->t_inpcb->in6p_faddr))
				ss = ss_fltsz_local;
		} else
#endif /* INET6 */
		if (in_localaddr(tp->t_inpcb->inp_faddr))
			ss = ss_fltsz_local;
		tp->snd_cwnd = tp->t_maxseg * ss;
	}
	tp->t_flags &= ~TF_LASTIDLE;
	if (idle) {
		if (tp->t_flags & TF_MORETOCOME) {
			tp->t_flags |= TF_LASTIDLE;
			idle = 0;
		}
	}
again:
	/*
	 * If we've recently taken a timeout, snd_max will be greater than
	 * snd_nxt.  There may be SACK information that allows us to avoid
	 * resending already delivered data.  Adjust snd_nxt accordingly.
	 */
	if ((tp->t_flags & TF_SACK_PERMIT) &&
	    SEQ_LT(tp->snd_nxt, tp->snd_max))
		tcp_sack_adjust(tp);
	sendalot = 0;
	off = tp->snd_nxt - tp->snd_una;
	sendwin = min(tp->snd_wnd, tp->snd_cwnd);
	sendwin = min(sendwin, tp->snd_bwnd);

	flags = tcp_outflags[tp->t_state];
	/*
	 * Send any SACK-generated retransmissions.  If we're explicitly trying
	 * to send out new data (when sendalot is 1), bypass this function.
	 * If we retransmit in fast recovery mode, decrement snd_cwnd, since
	 * we're replacing a (future) new transmission with a retransmission
	 * now, and we previously incremented snd_cwnd in tcp_input().
	 */
	/*
	 * Still in sack recovery , reset rxmit flag to zero.
	 */
	sack_rxmit = 0;
	sack_bytes_rxmt = 0;
	len = 0;
	p = NULL;
	if ((tp->t_flags & TF_SACK_PERMIT) && IN_FASTRECOVERY(tp) &&
	    (p = tcp_sack_output(tp, &sack_bytes_rxmt))) {
		long cwin;
		
		cwin = min(tp->snd_wnd, tp->snd_cwnd) - sack_bytes_rxmt;
		if (cwin < 0)
			cwin = 0;
		/* Do not retransmit SACK segments beyond snd_recover */
		if (SEQ_GT(p->end, tp->snd_recover)) {
			/*
			 * (At least) part of sack hole extends beyond
			 * snd_recover. Check to see if we can rexmit data
			 * for this hole.
			 */
			if (SEQ_GEQ(p->rxmit, tp->snd_recover)) {
				/*
				 * Can't rexmit any more data for this hole.
				 * That data will be rexmitted in the next
				 * sack recovery episode, when snd_recover
				 * moves past p->rxmit.
				 */
				p = NULL;
				goto after_sack_rexmit;
			} else
				/* Can rexmit part of the current hole */
				len = ((long)ulmin(cwin,
						   tp->snd_recover - p->rxmit));
		} else
			len = ((long)ulmin(cwin, p->end - p->rxmit));
		off = p->rxmit - tp->snd_una;
		KASSERT(off >= 0,("%s: sack block to the left of una : %d",
		    __func__, off));
		if (len > 0) {
			sack_rxmit = 1;
			sendalot = 1;
			tcpstat.tcps_sack_rexmits++;
			tcpstat.tcps_sack_rexmit_bytes +=
			    min(len, tp->t_maxseg);
		}
	}
after_sack_rexmit:
	/*
	 * Get standard flags, and add SYN or FIN if requested by 'hidden'
	 * state flags.
	 */
	if (tp->t_flags & TF_NEEDFIN)
		flags |= TH_FIN;
	if (tp->t_flags & TF_NEEDSYN)
		flags |= TH_SYN;

	SOCKBUF_LOCK(&so->so_snd);
	/*
	 * If in persist timeout with window of 0, send 1 byte.
	 * Otherwise, if window is small but nonzero
	 * and timer expired, we will send what we can
	 * and go to transmit state.
	 */
	if (tp->t_flags & TF_FORCEDATA) {
		if (sendwin == 0) {
			/*
			 * If we still have some data to send, then
			 * clear the FIN bit.  Usually this would
			 * happen below when it realizes that we
			 * aren't sending all the data.  However,
			 * if we have exactly 1 byte of unsent data,
			 * then it won't clear the FIN bit below,
			 * and if we are in persist state, we wind
			 * up sending the packet without recording
			 * that we sent the FIN bit.
			 *
			 * We can't just blindly clear the FIN bit,
			 * because if we don't have any more data
			 * to send then the probe will be the FIN
			 * itself.
			 */
			if (off < so->so_snd.sb_cc)
				flags &= ~TH_FIN;
			sendwin = 1;
		} else {
			tcp_timer_activate(tp, TT_PERSIST, 0);
			tp->t_rxtshift = 0;
		}
	}

	/*
	 * If snd_nxt == snd_max and we have transmitted a FIN, the
	 * offset will be > 0 even if so_snd.sb_cc is 0, resulting in
	 * a negative length.  This can also occur when TCP opens up
	 * its congestion window while receiving additional duplicate
	 * acks after fast-retransmit because TCP will reset snd_nxt
	 * to snd_max after the fast-retransmit.
	 *
	 * In the normal retransmit-FIN-only case, however, snd_nxt will
	 * be set to snd_una, the offset will be 0, and the length may
	 * wind up 0.
	 *
	 * If sack_rxmit is true we are retransmitting from the scoreboard
	 * in which case len is already set.
	 */
	if (sack_rxmit == 0) {
		if (sack_bytes_rxmt == 0)
			len = ((long)ulmin(so->so_snd.sb_cc, sendwin) - off);
		else {
			long cwin;

                        /*
			 * We are inside of a SACK recovery episode and are
			 * sending new data, having retransmitted all the
			 * data possible in the scoreboard.
			 */
			len = ((long)ulmin(so->so_snd.sb_cc, tp->snd_wnd) 
			       - off);
			/*
			 * Don't remove this (len > 0) check !
			 * We explicitly check for len > 0 here (although it 
			 * isn't really necessary), to work around a gcc 
			 * optimization issue - to force gcc to compute
			 * len above. Without this check, the computation
			 * of len is bungled by the optimizer.
			 */
			if (len > 0) {
				cwin = tp->snd_cwnd - 
					(tp->snd_nxt - tp->sack_newdata) -
					sack_bytes_rxmt;
				if (cwin < 0)
					cwin = 0;
				len = lmin(len, cwin);
			}
		}
	}

	/*
	 * Lop off SYN bit if it has already been sent.  However, if this
	 * is SYN-SENT state and if segment contains data and if we don't
	 * know that foreign host supports TAO, suppress sending segment.
	 */
	if ((flags & TH_SYN) && SEQ_GT(tp->snd_nxt, tp->snd_una)) {
		if (tp->t_state != TCPS_SYN_RECEIVED)
			flags &= ~TH_SYN;
		off--, len++;
	}

	/*
	 * Be careful not to send data and/or FIN on SYN segments.
	 * This measure is needed to prevent interoperability problems
	 * with not fully conformant TCP implementations.
	 */
	if ((flags & TH_SYN) && (tp->t_flags & TF_NOOPT)) {
		len = 0;
		flags &= ~TH_FIN;
	}

	if (len < 0) {
		/*
		 * If FIN has been sent but not acked,
		 * but we haven't been called to retransmit,
		 * len will be < 0.  Otherwise, window shrank
		 * after we sent into it.  If window shrank to 0,
		 * cancel pending retransmit, pull snd_nxt back
		 * to (closed) window, and set the persist timer
		 * if it isn't already going.  If the window didn't
		 * close completely, just wait for an ACK.
		 */
		len = 0;
		if (sendwin == 0) {
			tcp_timer_activate(tp, TT_REXMT, 0);
			tp->t_rxtshift = 0;
			tp->snd_nxt = tp->snd_una;
			if (!tcp_timer_active(tp, TT_PERSIST))
				tcp_setpersist(tp);
		}
	}

	/* len will be >= 0 after this point. */
	KASSERT(len >= 0, ("%s: len < 0", __func__));

	/*
	 * Automatic sizing of send socket buffer.  Often the send buffer
	 * size is not optimally adjusted to the actual network conditions
	 * at hand (delay bandwidth product).  Setting the buffer size too
	 * small limits throughput on links with high bandwidth and high
	 * delay (eg. trans-continental/oceanic links).  Setting the
	 * buffer size too big consumes too much real kernel memory,
	 * especially with many connections on busy servers.
	 *
	 * The criteria to step up the send buffer one notch are:
	 *  1. receive window of remote host is larger than send buffer
	 *     (with a fudge factor of 5/4th);
	 *  2. send buffer is filled to 7/8th with data (so we actually
	 *     have data to make use of it);
	 *  3. send buffer fill has not hit maximal automatic size;
	 *  4. our send window (slow start and cogestion controlled) is
	 *     larger than sent but unacknowledged data in send buffer.
	 *
	 * The remote host receive window scaling factor may limit the
	 * growing of the send buffer before it reaches its allowed
	 * maximum.
	 *
	 * It scales directly with slow start or congestion window
	 * and does at most one step per received ACK.  This fast
	 * scaling has the drawback of growing the send buffer beyond
	 * what is strictly necessary to make full use of a given
	 * delay*bandwith product.  However testing has shown this not
	 * to be much of an problem.  At worst we are trading wasting
	 * of available bandwith (the non-use of it) for wasting some
	 * socket buffer memory.
	 *
	 * TODO: Shrink send buffer during idle periods together
	 * with congestion window.  Requires another timer.  Has to
	 * wait for upcoming tcp timer rewrite.
	 */
	if (tcp_do_autosndbuf && so->so_snd.sb_flags & SB_AUTOSIZE) {
		if ((tp->snd_wnd / 4 * 5) >= so->so_snd.sb_hiwat &&
		    so->so_snd.sb_cc >= (so->so_snd.sb_hiwat / 8 * 7) &&
		    so->so_snd.sb_cc < tcp_autosndbuf_max &&
		    sendwin >= (so->so_snd.sb_cc - (tp->snd_nxt - tp->snd_una))) {
			if (!sbreserve_locked(&so->so_snd,
			    min(so->so_snd.sb_hiwat + tcp_autosndbuf_inc,
			     tcp_autosndbuf_max), so, curthread))
				so->so_snd.sb_flags &= ~SB_AUTOSIZE;
		}
	}

	/*
	 * Truncate to the maximum segment length or enable TCP Segmentation
	 * Offloading (if supported by hardware) and ensure that FIN is removed
	 * if the length no longer contains the last data byte.
	 *
	 * TSO may only be used if we are in a pure bulk sending state.  The
	 * presence of TCP-MD5, SACK retransmits, SACK advertizements and
	 * IP options prevent using TSO.  With TSO the TCP header is the same
	 * (except for the sequence number) for all generated packets.  This
	 * makes it impossible to transmit any options which vary per generated
	 * segment or packet.
	 *
	 * The length of TSO bursts is limited to TCP_MAXWIN.  That limit and
	 * removal of FIN (if not already catched here) are handled later after
	 * the exact length of the TCP options are known.
	 */
#ifdef IPSEC
	/*
	 * Pre-calculate here as we save another lookup into the darknesses
	 * of IPsec that way and can actually decide if TSO is ok.
	 */
	ipsec_optlen = ipsec_hdrsiz_tcp(tp);
#endif
	if (len > tp->t_maxseg) {
		if ((tp->t_flags & TF_TSO) && tcp_do_tso &&
		    ((tp->t_flags & TF_SIGNATURE) == 0) &&
		    tp->rcv_numsacks == 0 && sack_rxmit == 0 &&
		    tp->t_inpcb->inp_options == NULL &&
		    tp->t_inpcb->in6p_options == NULL
#ifdef IPSEC
		    && ipsec_optlen == 0
#endif
		    ) {
			tso = 1;
		} else {
			len = tp->t_maxseg;
			sendalot = 1;
			tso = 0;
		}
	}
	if (sack_rxmit) {
		if (SEQ_LT(p->rxmit + len, tp->snd_una + so->so_snd.sb_cc))
			flags &= ~TH_FIN;
	} else {
		if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
			flags &= ~TH_FIN;
	}

	recwin = sbspace(&so->so_rcv);

	/*
	 * Sender silly window avoidance.   We transmit under the following
	 * conditions when len is non-zero:
	 *
	 *	- We have a full segment (or more with TSO)
	 *	- This is the last buffer in a write()/send() and we are
	 *	  either idle or running NODELAY
	 *	- we've timed out (e.g. persist timer)
	 *	- we have more then 1/2 the maximum send window's worth of
	 *	  data (receiver may be limited the window size)
	 *	- we need to retransmit
	 */
	if (len) {
		if (len >= tp->t_maxseg)
			goto send;
		/*
		 * NOTE! on localhost connections an 'ack' from the remote
		 * end may occur synchronously with the output and cause
		 * us to flush a buffer queued with moretocome.  XXX
		 *
		 * note: the len + off check is almost certainly unnecessary.
		 */
		if (!(tp->t_flags & TF_MORETOCOME) &&	/* normal case */
		    (idle || (tp->t_flags & TF_NODELAY)) &&
		    len + off >= so->so_snd.sb_cc &&
		    (tp->t_flags & TF_NOPUSH) == 0) {
			goto send;
		}
		if (tp->t_flags & TF_FORCEDATA)		/* typ. timeout case */
			goto send;
		if (len >= tp->max_sndwnd / 2 && tp->max_sndwnd > 0)
			goto send;
		if (SEQ_LT(tp->snd_nxt, tp->snd_max))	/* retransmit case */
			goto send;
		if (sack_rxmit)
			goto send;
	}

	/*
	 * Compare available window to amount of window
	 * known to peer (as advertised window less
	 * next expected input).  If the difference is at least two
	 * max size segments, or at least 50% of the maximum possible
	 * window, then want to send a window update to peer.
	 * Skip this if the connection is in T/TCP half-open state.
	 * Don't send pure window updates when the peer has closed
	 * the connection and won't ever send more data.
	 */
	if (recwin > 0 && !(tp->t_flags & TF_NEEDSYN) &&
	    !TCPS_HAVERCVDFIN(tp->t_state)) {
		/*
		 * "adv" is the amount we can increase the window,
		 * taking into account that we are limited by
		 * TCP_MAXWIN << tp->rcv_scale.
		 */
		long adv = min(recwin, (long)TCP_MAXWIN << tp->rcv_scale) -
			(tp->rcv_adv - tp->rcv_nxt);

		if (adv >= (long) (2 * tp->t_maxseg))
			goto send;
		if (2 * adv >= (long) so->so_rcv.sb_hiwat)
			goto send;
	}

	/*
	 * Send if we owe the peer an ACK, RST, SYN, or urgent data.  ACKNOW
	 * is also a catch-all for the retransmit timer timeout case.
	 */
	if (tp->t_flags & TF_ACKNOW)
		goto send;
	if ((flags & TH_RST) ||
	    ((flags & TH_SYN) && (tp->t_flags & TF_NEEDSYN) == 0))
		goto send;
	if (SEQ_GT(tp->snd_up, tp->snd_una))
		goto send;
	/*
	 * If our state indicates that FIN should be sent
	 * and we have not yet done so, then we need to send.
	 */
	if (flags & TH_FIN &&
	    ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
		goto send;
	/*
	 * In SACK, it is possible for tcp_output to fail to send a segment
	 * after the retransmission timer has been turned off.  Make sure
	 * that the retransmission timer is set.
	 */
	if ((tp->t_flags & TF_SACK_PERMIT) &&
	    SEQ_GT(tp->snd_max, tp->snd_una) &&
	    !tcp_timer_active(tp, TT_REXMT) &&
	    !tcp_timer_active(tp, TT_PERSIST)) {
		tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);
		goto just_return;
	} 
	/*
	 * TCP window updates are not reliable, rather a polling protocol
	 * using ``persist'' packets is used to insure receipt of window
	 * updates.  The three ``states'' for the output side are:
	 *	idle			not doing retransmits or persists
	 *	persisting		to move a small or zero window
	 *	(re)transmitting	and thereby not persisting
	 *
	 * tcp_timer_active(tp, TT_PERSIST)
	 *	is true when we are in persist state.
	 * (tp->t_flags & TF_FORCEDATA)
	 *	is set when we are called to send a persist packet.
	 * tcp_timer_active(tp, TT_REXMT)
	 *	is set when we are retransmitting
	 * The output side is idle when both timers are zero.
	 *
	 * If send window is too small, there is data to transmit, and no
	 * retransmit or persist is pending, then go to persist state.
	 * If nothing happens soon, send when timer expires:
	 * if window is nonzero, transmit what we can,
	 * otherwise force out a byte.
	 */
	if (so->so_snd.sb_cc && !tcp_timer_active(tp, TT_REXMT) &&
	    !tcp_timer_active(tp, TT_PERSIST)) {
		tp->t_rxtshift = 0;
		tcp_setpersist(tp);
	}

	/*
	 * No reason to send a segment, just return.
	 */
just_return:
	SOCKBUF_UNLOCK(&so->so_snd);
	return (0);

send:
	SOCKBUF_LOCK_ASSERT(&so->so_snd);
	/*
	 * Before ESTABLISHED, force sending of initial options
	 * unless TCP set not to do any options.
	 * NOTE: we assume that the IP/TCP header plus TCP options
	 * always fit in a single mbuf, leaving room for a maximum
	 * link header, i.e.
	 *	max_linkhdr + sizeof (struct tcpiphdr) + optlen <= MCLBYTES
	 */
	optlen = 0;
#ifdef INET6
	if (isipv6)
		hdrlen = sizeof (struct ip6_hdr) + sizeof (struct tcphdr);
	else
#endif
	hdrlen = sizeof (struct tcpiphdr);

	/*
	 * Compute options for segment.
	 * We only have to care about SYN and established connection
	 * segments.  Options for SYN-ACK segments are handled in TCP
	 * syncache.
	 */
	if ((tp->t_flags & TF_NOOPT) == 0) {
		to.to_flags = 0;
		/* Maximum segment size. */
		if (flags & TH_SYN) {
			tp->snd_nxt = tp->iss;
			to.to_mss = tcp_mssopt(&tp->t_inpcb->inp_inc);
			to.to_flags |= TOF_MSS;
		}
		/* Window scaling. */
		if ((flags & TH_SYN) && (tp->t_flags & TF_REQ_SCALE)) {
			to.to_wscale = tp->request_r_scale;
			to.to_flags |= TOF_SCALE;
		}
		/* Timestamps. */
		if ((tp->t_flags & TF_RCVD_TSTMP) ||
		    ((flags & TH_SYN) && (tp->t_flags & TF_REQ_TSTMP))) {
			to.to_tsval = ticks + tp->ts_offset;
			to.to_tsecr = tp->ts_recent;
			to.to_flags |= TOF_TS;
			/* Set receive buffer autosizing timestamp. */
			if (tp->rfbuf_ts == 0 &&
			    (so->so_rcv.sb_flags & SB_AUTOSIZE))
				tp->rfbuf_ts = ticks;
		}
		/* Selective ACK's. */
		if (tp->t_flags & TF_SACK_PERMIT) {
			if (flags & TH_SYN)
				to.to_flags |= TOF_SACKPERM;
			else if (TCPS_HAVEESTABLISHED(tp->t_state) &&
			    (tp->t_flags & TF_SACK_PERMIT) &&
			    tp->rcv_numsacks > 0) {
				to.to_flags |= TOF_SACK;
				to.to_nsacks = tp->rcv_numsacks;
				to.to_sacks = (u_char *)tp->sackblks;
			}
		}
#ifdef TCP_SIGNATURE
		/* TCP-MD5 (RFC2385). */
#ifdef INET6
		if (!isipv6 && (tp->t_flags & TF_SIGNATURE))
#else
		if (tp->t_flags & TF_SIGNATURE)
#endif /* INET6 */
			to.to_flags |= TOF_SIGNATURE;
#endif /* TCP_SIGNATURE */

		/* Processing the options. */
		hdrlen += optlen = tcp_addoptions(&to, opt);
	}

#ifdef INET6
	if (isipv6)
		ipoptlen = ip6_optlen(tp->t_inpcb);
	else
#endif
	if (tp->t_inpcb->inp_options)
		ipoptlen = tp->t_inpcb->inp_options->m_len -
				offsetof(struct ipoption, ipopt_list);
	else
		ipoptlen = 0;
#ifdef IPSEC
	ipoptlen += ipsec_optlen;
#endif

	/*
	 * Adjust data length if insertion of options will
	 * bump the packet length beyond the t_maxopd length.
	 * Clear the FIN bit because we cut off the tail of
	 * the segment.
	 *
	 * When doing TSO limit a burst to TCP_MAXWIN minus the
	 * IP, TCP and Options length to keep ip->ip_len from
	 * overflowing.  Prevent the last segment from being
	 * fractional thus making them all equal sized and set
	 * the flag to continue sending.  TSO is disabled when
	 * IP options or IPSEC are present.
	 */
	if (len + optlen + ipoptlen > tp->t_maxopd) {
		flags &= ~TH_FIN;
		if (tso) {
			if (len > TCP_MAXWIN - hdrlen - optlen) {
				len = TCP_MAXWIN - hdrlen - optlen;
				len = len - (len % (tp->t_maxopd - optlen));
				sendalot = 1;
			} else if (tp->t_flags & TF_NEEDFIN)
				sendalot = 1;
		} else {
			len = tp->t_maxopd - optlen - ipoptlen;
			sendalot = 1;
		}
	}

/*#ifdef DIAGNOSTIC*/
#ifdef INET6
	if (max_linkhdr + hdrlen > MCLBYTES)
#else
	if (max_linkhdr + hdrlen > MHLEN)
#endif
		panic("tcphdr too big");
/*#endif*/

	/*
	 * Grab a header mbuf, attaching a copy of data to
	 * be transmitted, and initialize the header from
	 * the template for sends on this connection.
	 */
	if (len) {
		struct mbuf *mb;
		u_int moff;

		if ((tp->t_flags & TF_FORCEDATA) && len == 1)
			tcpstat.tcps_sndprobe++;
		else if (SEQ_LT(tp->snd_nxt, tp->snd_max) || sack_rxmit) {
			tcpstat.tcps_sndrexmitpack++;
			tcpstat.tcps_sndrexmitbyte += len;
		} else {
			tcpstat.tcps_sndpack++;
			tcpstat.tcps_sndbyte += len;
		}
#ifdef notyet
		if ((m = m_copypack(so->so_snd.sb_mb, off,
		    (int)len, max_linkhdr + hdrlen)) == 0) {
			SOCKBUF_UNLOCK(&so->so_snd);
			error = ENOBUFS;
			goto out;
		}
		/*
		 * m_copypack left space for our hdr; use it.
		 */
		m->m_len += hdrlen;
		m->m_data -= hdrlen;
#else
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			SOCKBUF_UNLOCK(&so->so_snd);
			error = ENOBUFS;
			goto out;
		}
#ifdef INET6
		if (MHLEN < hdrlen + max_linkhdr) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				SOCKBUF_UNLOCK(&so->so_snd);
				m_freem(m);
				error = ENOBUFS;
				goto out;
			}
		}
#endif
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;

		/*
		 * Start the m_copy functions from the closest mbuf
		 * to the offset in the socket buffer chain.
		 */
		mb = sbsndptr(&so->so_snd, off, len, &moff);

		if (len <= MHLEN - hdrlen - max_linkhdr) {
			m_copydata(mb, moff, (int)len,
			    mtod(m, caddr_t) + hdrlen);
			m->m_len += len;
		} else {
			m->m_next = m_copy(mb, moff, (int)len);
			if (m->m_next == NULL) {
				SOCKBUF_UNLOCK(&so->so_snd);
				(void) m_free(m);
				error = ENOBUFS;
				goto out;
			}
		}
#endif
		/*
		 * If we're sending everything we've got, set PUSH.
		 * (This will keep happy those implementations which only
		 * give data to the user when a buffer fills or
		 * a PUSH comes in.)
		 */
		if (off + len == so->so_snd.sb_cc)
			flags |= TH_PUSH;
		SOCKBUF_UNLOCK(&so->so_snd);
	} else {
		SOCKBUF_UNLOCK(&so->so_snd);
		if (tp->t_flags & TF_ACKNOW)
			tcpstat.tcps_sndacks++;
		else if (flags & (TH_SYN|TH_FIN|TH_RST))
			tcpstat.tcps_sndctrl++;
		else if (SEQ_GT(tp->snd_up, tp->snd_una))
			tcpstat.tcps_sndurg++;
		else
			tcpstat.tcps_sndwinup++;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			error = ENOBUFS;
			goto out;
		}
#ifdef INET6
		if (isipv6 && (MHLEN < hdrlen + max_linkhdr) &&
		    MHLEN >= hdrlen) {
			MH_ALIGN(m, hdrlen);
		} else
#endif
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
	}
	SOCKBUF_UNLOCK_ASSERT(&so->so_snd);
	m->m_pkthdr.rcvif = (struct ifnet *)0;
#ifdef MAC
	mac_create_mbuf_from_inpcb(tp->t_inpcb, m);
#endif
#ifdef INET6
	if (isipv6) {
		ip6 = mtod(m, struct ip6_hdr *);
		th = (struct tcphdr *)(ip6 + 1);
		tcpip_fillheaders(tp->t_inpcb, ip6, th);
	} else
#endif /* INET6 */
	{
		ip = mtod(m, struct ip *);
		ipov = (struct ipovly *)ip;
		th = (struct tcphdr *)(ip + 1);
		tcpip_fillheaders(tp->t_inpcb, ip, th);
	}

	/*
	 * Fill in fields, remembering maximum advertised
	 * window for use in delaying messages about window sizes.
	 * If resending a FIN, be sure not to use a new sequence number.
	 */
	if (flags & TH_FIN && tp->t_flags & TF_SENTFIN &&
	    tp->snd_nxt == tp->snd_max)
		tp->snd_nxt--;
	/*
	 * If we are doing retransmissions, then snd_nxt will
	 * not reflect the first unsent octet.  For ACK only
	 * packets, we do not want the sequence number of the
	 * retransmitted packet, we want the sequence number
	 * of the next unsent octet.  So, if there is no data
	 * (and no SYN or FIN), use snd_max instead of snd_nxt
	 * when filling in ti_seq.  But if we are in persist
	 * state, snd_max might reflect one byte beyond the
	 * right edge of the window, so use snd_nxt in that
	 * case, since we know we aren't doing a retransmission.
	 * (retransmit and persist are mutually exclusive...)
	 */
	if (sack_rxmit == 0) {
		if (len || (flags & (TH_SYN|TH_FIN)) ||
		    tcp_timer_active(tp, TT_PERSIST))
			th->th_seq = htonl(tp->snd_nxt);
		else
			th->th_seq = htonl(tp->snd_max);
	} else {
		th->th_seq = htonl(p->rxmit);
		p->rxmit += len;
		tp->sackhint.sack_bytes_rexmit += len;
	}
	th->th_ack = htonl(tp->rcv_nxt);
	if (optlen) {
		bcopy(opt, th + 1, optlen);
		th->th_off = (sizeof (struct tcphdr) + optlen) >> 2;
	}
	th->th_flags = flags;
	/*
	 * Calculate receive window.  Don't shrink window,
	 * but avoid silly window syndrome.
	 */
	if (recwin < (long)(so->so_rcv.sb_hiwat / 4) &&
	    recwin < (long)tp->t_maxseg)
		recwin = 0;
	if (recwin < (long)(tp->rcv_adv - tp->rcv_nxt))
		recwin = (long)(tp->rcv_adv - tp->rcv_nxt);
	if (recwin > (long)TCP_MAXWIN << tp->rcv_scale)
		recwin = (long)TCP_MAXWIN << tp->rcv_scale;

	/*
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN>
	 * or <SYN,ACK>) segment itself is never scaled.  The <SYN,ACK>
	 * case is handled in syncache.
	 */
	if (flags & TH_SYN)
		th->th_win = htons((u_short)
				(min(sbspace(&so->so_rcv), TCP_MAXWIN)));
	else
		th->th_win = htons((u_short)(recwin >> tp->rcv_scale));

	/*
	 * Adjust the RXWIN0SENT flag - indicate that we have advertised
	 * a 0 window.  This may cause the remote transmitter to stall.  This
	 * flag tells soreceive() to disable delayed acknowledgements when
	 * draining the buffer.  This can occur if the receiver is attempting
	 * to read more data then can be buffered prior to transmitting on
	 * the connection.
	 */
	if (recwin == 0)
		tp->t_flags |= TF_RXWIN0SENT;
	else
		tp->t_flags &= ~TF_RXWIN0SENT;
	if (SEQ_GT(tp->snd_up, tp->snd_nxt)) {
		th->th_urp = htons((u_short)(tp->snd_up - tp->snd_nxt));
		th->th_flags |= TH_URG;
	} else
		/*
		 * If no urgent pointer to send, then we pull
		 * the urgent pointer to the left edge of the send window
		 * so that it doesn't drift into the send window on sequence
		 * number wraparound.
		 */
		tp->snd_up = tp->snd_una;		/* drag it along */

#ifdef TCP_SIGNATURE
#ifdef INET6
	if (!isipv6)
#endif
	if (tp->t_flags & TF_SIGNATURE) {
		int sigoff = to.to_signature - opt;
		tcp_signature_compute(m, sizeof(struct ip), len, optlen,
		    (u_char *)(th + 1) + sigoff, IPSEC_DIR_OUTBOUND);
	}
#endif

	/*
	 * Put TCP length in extended header, and then
	 * checksum extended header and data.
	 */
	m->m_pkthdr.len = hdrlen + len; /* in6_cksum() need this */
#ifdef INET6
	if (isipv6)
		/*
		 * ip6_plen is not need to be filled now, and will be filled
		 * in ip6_output.
		 */
		th->th_sum = in6_cksum(m, IPPROTO_TCP, sizeof(struct ip6_hdr),
				       sizeof(struct tcphdr) + optlen + len);
	else
#endif /* INET6 */
	{
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
		th->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons(sizeof(struct tcphdr) + IPPROTO_TCP + len + optlen));

		/* IP version must be set here for ipv4/ipv6 checking later */
		KASSERT(ip->ip_v == IPVERSION,
		    ("%s: IP version incorrect: %d", __func__, ip->ip_v));
	}

	/*
	 * Enable TSO and specify the size of the segments.
	 * The TCP pseudo header checksum is always provided.
	 * XXX: Fixme: This is currently not the case for IPv6.
	 */
	if (tso) {
		m->m_pkthdr.csum_flags = CSUM_TSO;
		m->m_pkthdr.tso_segsz = tp->t_maxopd - optlen;
	}

	/*
	 * In transmit state, time the transmission and arrange for
	 * the retransmit.  In persist state, just set snd_max.
	 */
	if ((tp->t_flags & TF_FORCEDATA) == 0 || 
	    !tcp_timer_active(tp, TT_PERSIST)) {
		tcp_seq startseq = tp->snd_nxt;

		/*
		 * Advance snd_nxt over sequence space of this segment.
		 */
		if (flags & (TH_SYN|TH_FIN)) {
			if (flags & TH_SYN)
				tp->snd_nxt++;
			if (flags & TH_FIN) {
				tp->snd_nxt++;
				tp->t_flags |= TF_SENTFIN;
			}
		}
		if (sack_rxmit)
			goto timer;
		tp->snd_nxt += len;
		if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
			tp->snd_max = tp->snd_nxt;
			/*
			 * Time this transmission if not a retransmission and
			 * not currently timing anything.
			 */
			if (tp->t_rtttime == 0) {
				tp->t_rtttime = ticks;
				tp->t_rtseq = startseq;
				tcpstat.tcps_segstimed++;
			}
		}

		/*
		 * Set retransmit timer if not currently set,
		 * and not doing a pure ack or a keep-alive probe.
		 * Initial value for retransmit timer is smoothed
		 * round-trip time + 2 * round-trip time variance.
		 * Initialize shift counter which is used for backoff
		 * of retransmit time.
		 */
timer:
		if (!tcp_timer_active(tp, TT_REXMT) &&
		    ((sack_rxmit && tp->snd_nxt != tp->snd_max) ||
		     (tp->snd_nxt != tp->snd_una))) {
			if (tcp_timer_active(tp, TT_PERSIST)) {
				tcp_timer_activate(tp, TT_PERSIST, 0);
				tp->t_rxtshift = 0;
			}
			tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);
		}
	} else {
		/*
		 * Persist case, update snd_max but since we are in
		 * persist mode (no window) we do not update snd_nxt.
		 */
		int xlen = len;
		if (flags & TH_SYN)
			++xlen;
		if (flags & TH_FIN) {
			++xlen;
			tp->t_flags |= TF_SENTFIN;
		}
		if (SEQ_GT(tp->snd_nxt + xlen, tp->snd_max))
			tp->snd_max = tp->snd_nxt + len;
	}

#ifdef TCPDEBUG
	/*
	 * Trace.
	 */
	if (so->so_options & SO_DEBUG) {
		u_short save = 0;
#ifdef INET6
		if (!isipv6)
#endif
		{
			save = ipov->ih_len;
			ipov->ih_len = htons(m->m_pkthdr.len /* - hdrlen + (th->th_off << 2) */);
		}
		tcp_trace(TA_OUTPUT, tp->t_state, tp, mtod(m, void *), th, 0);
#ifdef INET6
		if (!isipv6)
#endif
		ipov->ih_len = save;
	}
#endif

	/*
	 * Fill in IP length and desired time to live and
	 * send to IP level.  There should be a better way
	 * to handle ttl and tos; we could keep them in
	 * the template, but need a way to checksum without them.
	 */
	/*
	 * m->m_pkthdr.len should have been set before cksum calcuration,
	 * because in6_cksum() need it.
	 */
#ifdef INET6
	if (isipv6) {
		/*
		 * we separately set hoplimit for every segment, since the
		 * user might want to change the value via setsockopt.
		 * Also, desired default hop limit might be changed via
		 * Neighbor Discovery.
		 */
		ip6->ip6_hlim = in6_selecthlim(tp->t_inpcb, NULL);

		/* TODO: IPv6 IP6TOS_ECT bit on */
		error = ip6_output(m,
			    tp->t_inpcb->in6p_outputopts, NULL,
			    ((so->so_options & SO_DONTROUTE) ?
			    IP_ROUTETOIF : 0), NULL, NULL, tp->t_inpcb);
	} else
#endif /* INET6 */
    {
	ip->ip_len = m->m_pkthdr.len;
#ifdef INET6
	if (INP_CHECK_SOCKAF(so, AF_INET6))
		ip->ip_ttl = in6_selecthlim(tp->t_inpcb, NULL);
#endif /* INET6 */
	/*
	 * If we do path MTU discovery, then we set DF on every packet.
	 * This might not be the best thing to do according to RFC3390
	 * Section 2. However the tcp hostcache migitates the problem
	 * so it affects only the first tcp connection with a host.
	 */
	if (path_mtu_discovery)
		ip->ip_off |= IP_DF;

	error = ip_output(m, tp->t_inpcb->inp_options, NULL,
	    ((so->so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0), 0,
	    tp->t_inpcb);
    }
	if (error) {

		/*
		 * We know that the packet was lost, so back out the
		 * sequence number advance, if any.
		 *
		 * If the error is EPERM the packet got blocked by the
		 * local firewall.  Normally we should terminate the
		 * connection but the blocking may have been spurious
		 * due to a firewall reconfiguration cycle.  So we treat
		 * it like a packet loss and let the retransmit timer and
		 * timeouts do their work over time.
		 * XXX: It is a POLA question whether calling tcp_drop right
		 * away would be the really correct behavior instead.
		 */
		if (((tp->t_flags & TF_FORCEDATA) == 0 ||
		    !tcp_timer_active(tp, TT_PERSIST)) &&
		    ((flags & TH_SYN) == 0) &&
		    (error != EPERM)) {
			if (sack_rxmit) {
				p->rxmit -= len;
				tp->sackhint.sack_bytes_rexmit -= len;
				KASSERT(tp->sackhint.sack_bytes_rexmit >= 0,
				    ("sackhint bytes rtx >= 0"));
			} else
				tp->snd_nxt -= len;
		}
out:
		SOCKBUF_UNLOCK_ASSERT(&so->so_snd);	/* Check gotos. */
		switch (error) {
		case EPERM:
			tp->t_softerror = error;
			return (error);
		case ENOBUFS:
	                if (!tcp_timer_active(tp, TT_REXMT) &&
			    !tcp_timer_active(tp, TT_PERSIST))
	                        tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);
			tp->snd_cwnd = tp->t_maxseg;
			return (0);
		case EMSGSIZE:
			/*
			 * For some reason the interface we used initially
			 * to send segments changed to another or lowered
			 * its MTU.
			 *
			 * tcp_mtudisc() will find out the new MTU and as
			 * its last action, initiate retransmission, so it
			 * is important to not do so here.
			 *
			 * If TSO was active we either got an interface
			 * without TSO capabilits or TSO was turned off.
			 * Disable it for this connection as too and
			 * immediatly retry with MSS sized segments generated
			 * by this function.
			 */
			if (tso)
				tp->t_flags &= ~TF_TSO;
			tcp_mtudisc(tp->t_inpcb, 0);
			return (0);
		case EHOSTDOWN:
		case EHOSTUNREACH:
		case ENETDOWN:
		case ENETUNREACH:
			if (TCPS_HAVERCVDSYN(tp->t_state)) {
				tp->t_softerror = error;
				return (0);
			}
			/* FALLTHROUGH */
		default:
			return (error);
		}
	}
	tcpstat.tcps_sndtotal++;

	/*
	 * Data sent (as far as we can tell).
	 * If this advertises a larger window than any other segment,
	 * then remember the size of the advertised window.
	 * Any pending ACK has now been sent.
	 */
	if (recwin > 0 && SEQ_GT(tp->rcv_nxt + recwin, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + recwin;
	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_flags &= ~(TF_ACKNOW | TF_DELACK);
	if (tcp_timer_active(tp, TT_DELACK))
		tcp_timer_activate(tp, TT_DELACK, 0);
#if 0
	/*
	 * This completely breaks TCP if newreno is turned on.  What happens
	 * is that if delayed-acks are turned on on the receiver, this code
	 * on the transmitter effectively destroys the TCP window, forcing
	 * it to four packets (1.5Kx4 = 6K window).
	 */
	if (sendalot && (!tcp_do_newreno || --maxburst))
		goto again;
#endif
	if (sendalot)
		goto again;
	return (0);
}

void
tcp_setpersist(struct tcpcb *tp)
{
	int t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;
	int tt;

	if (tcp_timer_active(tp, TT_REXMT))
		panic("tcp_setpersist: retransmit pending");
	/*
	 * Start/restart persistance timer.
	 */
	TCPT_RANGESET(tt, t * tcp_backoff[tp->t_rxtshift],
		      TCPTV_PERSMIN, TCPTV_PERSMAX);
	tcp_timer_activate(tp, TT_PERSIST, tt);
	if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
		tp->t_rxtshift++;
}

/*
 * Insert TCP options according to the supplied parameters to the place
 * optp in a consistent way.  Can handle unaligned destinations.
 *
 * The order of the option processing is crucial for optimal packing and
 * alignment for the scarce option space.
 *
 * The optimal order for a SYN/SYN-ACK segment is:
 *   MSS (4) + NOP (1) + Window scale (3) + SACK permitted (2) +
 *   Timestamp (10) + Signature (18) = 38 bytes out of a maximum of 40.
 *
 * The SACK options should be last.  SACK blocks consume 8*n+2 bytes.
 * So a full size SACK blocks option is 34 bytes (with 4 SACK blocks).
 * At minimum we need 10 bytes (to generate 1 SACK block).  If both
 * TCP Timestamps (12 bytes) and TCP Signatures (18 bytes) are present,
 * we only have 10 bytes for SACK options (40 - (12 + 18)).
 */
int
tcp_addoptions(struct tcpopt *to, u_char *optp)
{
	u_int mask, optlen = 0;

	for (mask = 1; mask < TOF_MAXOPT; mask <<= 1) {
		if ((to->to_flags & mask) != mask)
			continue;
		switch (to->to_flags & mask) {
		case TOF_MSS:
			while (optlen % 4) {
				optlen += TCPOLEN_NOP;
				*optp++ = TCPOPT_NOP;
			}
			optlen += TCPOLEN_MAXSEG;
			*optp++ = TCPOPT_MAXSEG;
			*optp++ = TCPOLEN_MAXSEG;
			to->to_mss = htons(to->to_mss);
			bcopy((u_char *)&to->to_mss, optp, sizeof(to->to_mss));
			optp += sizeof(to->to_mss);
			break;
		case TOF_SCALE:
			while (!optlen || optlen % 2 != 1) {
				optlen += TCPOLEN_NOP;
				*optp++ = TCPOPT_NOP;
			}
			optlen += TCPOLEN_WINDOW;
			*optp++ = TCPOPT_WINDOW;
			*optp++ = TCPOLEN_WINDOW;
			*optp++ = to->to_wscale;
			break;
		case TOF_SACKPERM:
			while (optlen % 2) {
				optlen += TCPOLEN_NOP;
				*optp++ = TCPOPT_NOP;
			}
			optlen += TCPOLEN_SACK_PERMITTED;
			*optp++ = TCPOPT_SACK_PERMITTED;
			*optp++ = TCPOLEN_SACK_PERMITTED;
			break;
		case TOF_TS:
			while (!optlen || optlen % 4 != 2) {
				optlen += TCPOLEN_NOP;
				*optp++ = TCPOPT_NOP;
			}
			optlen += TCPOLEN_TIMESTAMP;
			*optp++ = TCPOPT_TIMESTAMP;
			*optp++ = TCPOLEN_TIMESTAMP;
			to->to_tsval = htonl(to->to_tsval);
			to->to_tsecr = htonl(to->to_tsecr);
			bcopy((u_char *)&to->to_tsval, optp, sizeof(to->to_tsval));
			optp += sizeof(to->to_tsval);
			bcopy((u_char *)&to->to_tsecr, optp, sizeof(to->to_tsecr));
			optp += sizeof(to->to_tsecr);
			break;
		case TOF_SIGNATURE:
			{
			int siglen = TCPOLEN_SIGNATURE - 2;

			while (!optlen || optlen % 4 != 2) {
				optlen += TCPOLEN_NOP;
				*optp++ = TCPOPT_NOP;
			}
			if (TCP_MAXOLEN - optlen < TCPOLEN_SIGNATURE)
				continue;
			optlen += TCPOLEN_SIGNATURE;
			*optp++ = TCPOPT_SIGNATURE;
			*optp++ = TCPOLEN_SIGNATURE;
			to->to_signature = optp;
			while (siglen--)
				 *optp++ = 0;
			break;
			}
		case TOF_SACK:
			{
			int sackblks = 0;
			struct sackblk *sack = (struct sackblk *)to->to_sacks;
			tcp_seq sack_seq;

			while (!optlen || optlen % 4 != 2) {
				optlen += TCPOLEN_NOP;
				*optp++ = TCPOPT_NOP;
			}
			if (TCP_MAXOLEN - optlen < 2 + TCPOLEN_SACK)
				continue;
			optlen += TCPOLEN_SACKHDR;
			*optp++ = TCPOPT_SACK;
			sackblks = min(to->to_nsacks,
					(TCP_MAXOLEN - optlen) / TCPOLEN_SACK);
			*optp++ = TCPOLEN_SACKHDR + sackblks * TCPOLEN_SACK;
			while (sackblks--) {
				sack_seq = htonl(sack->start);
				bcopy((u_char *)&sack_seq, optp, sizeof(sack_seq));
				optp += sizeof(sack_seq);
				sack_seq = htonl(sack->end);
				bcopy((u_char *)&sack_seq, optp, sizeof(sack_seq));
				optp += sizeof(sack_seq);
				optlen += TCPOLEN_SACK;
				sack++;
			}
			tcpstat.tcps_sack_send_blocks++;
			break;
			}
		default:
			panic("%s: unknown TCP option type", __func__);
			break;
		}
	}

	/* Terminate and pad TCP options to a 4 byte boundary. */
	if (optlen % 4) {
		optlen += TCPOLEN_EOL;
		*optp++ = TCPOPT_EOL;
	}
	/*
	 * According to RFC 793 (STD0007):
	 *   "The content of the header beyond the End-of-Option option
	 *    must be header padding (i.e., zero)."
	 *   and later: "The padding is composed of zeros."
	 * While EOLs are zeros use an explicit 0x00 here to not confuse
	 * people with padding of EOLs.
	 */
	while (optlen % 4) {
		optlen += 1;
		*optp++ = 0x00;
	}

	KASSERT(optlen <= TCP_MAXOLEN, ("%s: TCP options too long", __func__));
	return (optlen);
}
