/*
 * Copyright (c) 1982, 1986, 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)tcp_output.c	7.22 (Berkeley) 8/31/90
 *	$Id: tcp_output.c,v 1.2 1993/10/16 18:26:29 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "malloc.h"
#include "mbuf.h"
#include "protosw.h"
#include "socket.h"
#include "socketvar.h"
#include "errno.h"

#include "../net/route.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "ip_var.h"
#include "tcp.h"
#define	TCPOUTFLAGS
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"

#ifdef notyet
extern struct mbuf *m_copypack();
#endif

/*
 * Initial options.
 */
u_char	tcp_initopt[4] = { TCPOPT_MAXSEG, 4, 0x0, 0x0, };

/*
 * Tcp output routine: figure out what should be sent and send it.
 */
tcp_output(tp)
	register struct tcpcb *tp;
{
	register struct socket *so = tp->t_inpcb->inp_socket;
	register long len, win;
	int off, flags, error;
	register struct mbuf *m;
	register struct tcpiphdr *ti;
	u_char *opt;
	unsigned optlen, hdrlen;
	int idle, sendalot;

	/*
	 * Determine length of data that should be transmitted,
	 * and flags that will be used.
	 * If there is some data or critical controls (SYN, RST)
	 * to send, then transmit; otherwise, investigate further.
	 */
	idle = (tp->snd_max == tp->snd_una);
	if (idle && tp->t_idle >= tp->t_rxtcur)
		/*
		 * We have been idle for "a while" and no acks are
		 * expected to clock out any data we send --
		 * slow start to get ack "clock" running again.
		 */
		tp->snd_cwnd = tp->t_maxseg;
again:
	sendalot = 0;
	off = tp->snd_nxt - tp->snd_una;
	win = min(tp->snd_wnd, tp->snd_cwnd);

	/*
	 * If in persist timeout with window of 0, send 1 byte.
	 * Otherwise, if window is small but nonzero
	 * and timer expired, we will send what we can
	 * and go to transmit state.
	 */
	if (tp->t_force) {
		if (win == 0)
			win = 1;
		else {
			tp->t_timer[TCPT_PERSIST] = 0;
			tp->t_rxtshift = 0;
		}
	}

	flags = tcp_outflags[tp->t_state];
	len = min(so->so_snd.sb_cc, win) - off;

	if (len < 0) {
		/*
		 * If FIN has been sent but not acked,
		 * but we haven't been called to retransmit,
		 * len will be -1.  Otherwise, window shrank
		 * after we sent into it.  If window shrank to 0,
		 * cancel pending retransmit and pull snd_nxt
		 * back to (closed) window.  We will enter persist
		 * state below.  If the window didn't close completely,
		 * just wait for an ACK.
		 */
		len = 0;
		if (win == 0) {
			tp->t_timer[TCPT_REXMT] = 0;
			tp->snd_nxt = tp->snd_una;
		}
	}
	if (len > tp->t_maxseg) {
		len = tp->t_maxseg;
		sendalot = 1;
	}
	if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
		flags &= ~TH_FIN;

	win = sbspace(&so->so_rcv);

	/*
	 * Sender silly window avoidance.  If connection is idle
	 * and can send all data, a maximum segment,
	 * at least a maximum default-size segment do it,
	 * or are forced, do it; otherwise don't bother.
	 * If peer's buffer is tiny, then send
	 * when window is at least half open.
	 * If retransmitting (possibly after persist timer forced us
	 * to send into a small window), then must resend.
	 */
	if (len) {
		if (len == tp->t_maxseg)
			goto send;
		if ((idle || tp->t_flags & TF_NODELAY) &&
		    len + off >= so->so_snd.sb_cc)
			goto send;
		if (tp->t_force)
			goto send;
		if (len >= tp->max_sndwnd / 2)
			goto send;
		if (SEQ_LT(tp->snd_nxt, tp->snd_max))
			goto send;
	}

	/*
	 * Compare available window to amount of window
	 * known to peer (as advertised window less
	 * next expected input).  If the difference is at least two
	 * max size segments, or at least 50% of the maximum possible
	 * window, then want to send a window update to peer.
	 */
	if (win > 0) {
		long adv = win - (tp->rcv_adv - tp->rcv_nxt);

		if (adv >= (long) (2 * tp->t_maxseg))
			goto send;
		if (2 * adv >= (long) so->so_rcv.sb_hiwat)
			goto send;
	}

	/*
	 * Send if we owe peer an ACK.
	 */
	if (tp->t_flags & TF_ACKNOW)
		goto send;
	if (flags & (TH_SYN|TH_RST))
		goto send;
	if (SEQ_GT(tp->snd_up, tp->snd_una))
		goto send;
	/*
	 * If our state indicates that FIN should be sent
	 * and we have not yet done so, or we're retransmitting the FIN,
	 * then we need to send.
	 */
	if (flags & TH_FIN &&
	    ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
		goto send;

	/*
	 * TCP window updates are not reliable, rather a polling protocol
	 * using ``persist'' packets is used to insure receipt of window
	 * updates.  The three ``states'' for the output side are:
	 *	idle			not doing retransmits or persists
	 *	persisting		to move a small or zero window
	 *	(re)transmitting	and thereby not persisting
	 *
	 * tp->t_timer[TCPT_PERSIST]
	 *	is set when we are in persist state.
	 * tp->t_force
	 *	is set when we are called to send a persist packet.
	 * tp->t_timer[TCPT_REXMT]
	 *	is set when we are retransmitting
	 * The output side is idle when both timers are zero.
	 *
	 * If send window is too small, there is data to transmit, and no
	 * retransmit or persist is pending, then go to persist state.
	 * If nothing happens soon, send when timer expires:
	 * if window is nonzero, transmit what we can,
	 * otherwise force out a byte.
	 */
	if (so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 &&
	    tp->t_timer[TCPT_PERSIST] == 0) {
		tp->t_rxtshift = 0;
		tcp_setpersist(tp);
	}

	/*
	 * No reason to send a segment, just return.
	 */
	return (0);

send:
	/*
	 * Before ESTABLISHED, force sending of initial options
	 * unless TCP set not to do any options.
	 * NOTE: we assume that the IP/TCP header plus TCP options
	 * always fit in a single mbuf, leaving room for a maximum
	 * link header, i.e.
	 *	max_linkhdr + sizeof (struct tcpiphdr) + optlen <= MHLEN
	 */
	optlen = 0;
	hdrlen = sizeof (struct tcpiphdr);
	if (flags & TH_SYN && (tp->t_flags & TF_NOOPT) == 0) {
		opt = tcp_initopt;
		optlen = sizeof (tcp_initopt);
		hdrlen += sizeof (tcp_initopt);
		*(u_short *)(opt + 2) = htons((u_short) tcp_mss(tp, 0));
#ifdef DIAGNOSTIC
	 	if (max_linkhdr + hdrlen > MHLEN)
			panic("tcphdr too big");
#endif
	}

	/*
	 * Grab a header mbuf, attaching a copy of data to
	 * be transmitted, and initialize the header from
	 * the template for sends on this connection.
	 */
	if (len) {
		if (tp->t_force && len == 1)
			tcpstat.tcps_sndprobe++;
		else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
			tcpstat.tcps_sndrexmitpack++;
			tcpstat.tcps_sndrexmitbyte += len;
		} else {
			tcpstat.tcps_sndpack++;
			tcpstat.tcps_sndbyte += len;
		}
#ifdef notyet
		if ((m = m_copypack(so->so_snd.sb_mb, off,
		    (int)len, max_linkhdr + hdrlen)) == 0) {
			error = ENOBUFS;
			goto out;
		}
		/*
		 * m_copypack left space for our hdr; use it.
		 */
		m->m_len += hdrlen;
		m->m_data -= hdrlen;
#else
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			error = ENOBUFS;
			goto out;
		}
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
		if (len <= MHLEN - hdrlen - max_linkhdr) {
			m_copydata(so->so_snd.sb_mb, off, (int) len,
			    mtod(m, caddr_t) + hdrlen);
			m->m_len += len;
		} else {
			m->m_next = m_copy(so->so_snd.sb_mb, off, (int) len);
			if (m->m_next == 0)
				len = 0;
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
	} else {
		if (tp->t_flags & TF_ACKNOW)
			tcpstat.tcps_sndacks++;
		else if (flags & (TH_SYN|TH_FIN|TH_RST))
			tcpstat.tcps_sndctrl++;
		else if (SEQ_GT(tp->snd_up, tp->snd_una))
			tcpstat.tcps_sndurg++;
		else
			tcpstat.tcps_sndwinup++;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			error = ENOBUFS;
			goto out;
		}
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
	}
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	ti = mtod(m, struct tcpiphdr *);
	if (tp->t_template == 0)
		panic("tcp_output");
	bcopy((caddr_t)tp->t_template, (caddr_t)ti, sizeof (struct tcpiphdr));

	/*
	 * Fill in fields, remembering maximum advertised
	 * window for use in delaying messages about window sizes.
	 * If resending a FIN, be sure not to use a new sequence number.
	 */
	if (flags & TH_FIN && tp->t_flags & TF_SENTFIN && 
	    tp->snd_nxt == tp->snd_max)
		tp->snd_nxt--;
	ti->ti_seq = htonl(tp->snd_nxt);
	ti->ti_ack = htonl(tp->rcv_nxt);
	if (optlen) {
		bcopy((caddr_t)opt, (caddr_t)(ti + 1), optlen);
		ti->ti_off = (sizeof (struct tcphdr) + optlen) >> 2;
	}
	ti->ti_flags = flags;
	/*
	 * Calculate receive window.  Don't shrink window,
	 * but avoid silly window syndrome.
	 */
	if (win < (long)(so->so_rcv.sb_hiwat / 4) && win < (long)tp->t_maxseg)
		win = 0;
	if (win > TCP_MAXWIN)
		win = TCP_MAXWIN;
	if (win < (long)(tp->rcv_adv - tp->rcv_nxt))
		win = (long)(tp->rcv_adv - tp->rcv_nxt);
	ti->ti_win = htons((u_short)win);
	if (SEQ_GT(tp->snd_up, tp->snd_nxt)) {
		ti->ti_urp = htons((u_short)(tp->snd_up - tp->snd_nxt));
		ti->ti_flags |= TH_URG;
	} else
		/*
		 * If no urgent pointer to send, then we pull
		 * the urgent pointer to the left edge of the send window
		 * so that it doesn't drift into the send window on sequence
		 * number wraparound.
		 */
		tp->snd_up = tp->snd_una;		/* drag it along */

	/*
	 * Put TCP length in extended header, and then
	 * checksum extended header and data.
	 */
	if (len + optlen)
		ti->ti_len = htons((u_short)(sizeof (struct tcphdr) +
		    optlen + len));
	ti->ti_sum = in_cksum(m, (int)(hdrlen + len));

	/*
	 * In transmit state, time the transmission and arrange for
	 * the retransmit.  In persist state, just set snd_max.
	 */
	if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0) {
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
		tp->snd_nxt += len;
		if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
			tp->snd_max = tp->snd_nxt;
			/*
			 * Time this transmission if not a retransmission and
			 * not currently timing anything.
			 */
			if (tp->t_rtt == 0) {
				tp->t_rtt = 1;
				tp->t_rtseq = startseq;
				tcpstat.tcps_segstimed++;
			}
		}

		/*
		 * Set retransmit timer if not currently set,
		 * and not doing an ack or a keep-alive probe.
		 * Initial value for retransmit timer is smoothed
		 * round-trip time + 2 * round-trip time variance.
		 * Initialize shift counter which is used for backoff
		 * of retransmit time.
		 */
		if (tp->t_timer[TCPT_REXMT] == 0 &&
		    tp->snd_nxt != tp->snd_una) {
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
			if (tp->t_timer[TCPT_PERSIST]) {
				tp->t_timer[TCPT_PERSIST] = 0;
				tp->t_rxtshift = 0;
			}
		}
	} else
		if (SEQ_GT(tp->snd_nxt + len, tp->snd_max))
			tp->snd_max = tp->snd_nxt + len;

	/*
	 * Trace.
	 */
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_OUTPUT, tp->t_state, tp, ti, 0);

	/*
	 * Fill in IP length and desired time to live and
	 * send to IP level.  There should be a better way
	 * to handle ttl and tos; we could keep them in
	 * the template, but need a way to checksum without them.
	 */
	m->m_pkthdr.len = hdrlen + len;
	((struct ip *)ti)->ip_len = m->m_pkthdr.len;
	((struct ip *)ti)->ip_ttl = tp->t_inpcb->inp_ip.ip_ttl;	/* XXX */
	((struct ip *)ti)->ip_tos = tp->t_inpcb->inp_ip.ip_tos;	/* XXX */
#if BSD >= 43
	error = ip_output(m, tp->t_inpcb->inp_options, &tp->t_inpcb->inp_route,
	    so->so_options & SO_DONTROUTE);
#else
	error = ip_output(m, (struct mbuf *)0, &tp->t_inpcb->inp_route, 
	    so->so_options & SO_DONTROUTE);
#endif
	if (error) {
out:
		if (error == ENOBUFS) {
			tcp_quench(tp->t_inpcb);
			return (0);
		}
		if ((error == EHOSTUNREACH || error == ENETDOWN)
		    && TCPS_HAVERCVDSYN(tp->t_state)) {
			tp->t_softerror = error;
			return (0);
		}
		return (error);
	}
	tcpstat.tcps_sndtotal++;

	/*
	 * Data sent (as far as we can tell).
	 * If this advertises a larger window than any other segment,
	 * then remember the size of the advertised window.
	 * Any pending ACK has now been sent.
	 */
	if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + win;
	tp->t_flags &= ~(TF_ACKNOW|TF_DELACK);
	if (sendalot)
		goto again;
	return (0);
}

tcp_setpersist(tp)
	register struct tcpcb *tp;
{
	register t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

	if (tp->t_timer[TCPT_REXMT])
		panic("tcp_output REXMT");
	/*
	 * Start/restart persistance timer.
	 */
	TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
	    t * tcp_backoff[tp->t_rxtshift],
	    TCPTV_PERSMIN, TCPTV_PERSMAX);
	if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
		tp->t_rxtshift++;
}
