/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 * $FreeBSD$
 */

#include "opt_ipfw.h"		/* for ipfw_fwd		*/
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mac.h"
#include "opt_tcpdebug.h"
#include "opt_tcp_input.h"
#include "opt_tcp_sack.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/cpu.h>	/* before tcp_seq.h, for tcp_random18() */

#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* for ICMP_BANDLIM		*/
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM		*/
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif /* TCPDEBUG */

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif /*FAST_IPSEC*/

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
#include <netkey/key.h>
#endif /*IPSEC*/

#include <machine/in_cksum.h>

static const int tcprexmtthresh = 3;
tcp_cc	tcp_ccgen;

struct	tcpstat tcpstat;
SYSCTL_STRUCT(_net_inet_tcp, TCPCTL_STATS, stats, CTLFLAG_RW,
    &tcpstat , tcpstat, "TCP statistics (struct tcpstat, netinet/tcp_var.h)");

static int log_in_vain = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, log_in_vain, CTLFLAG_RW,
    &log_in_vain, 0, "Log all incoming TCP connections");

static int blackhole = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, blackhole, CTLFLAG_RW,
	&blackhole, 0, "Do not send RST when dropping refused connections");

int tcp_delack_enabled = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, delayed_ack, CTLFLAG_RW,
    &tcp_delack_enabled, 0,
    "Delay ACK to try and piggyback it onto a data packet");

#ifdef TCP_DROP_SYNFIN
static int drop_synfin = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, drop_synfin, CTLFLAG_RW,
    &drop_synfin, 0, "Drop TCP packets with SYN+FIN set");
#endif

static int tcp_do_rfc3042 = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, rfc3042, CTLFLAG_RW,
    &tcp_do_rfc3042, 0, "Enable RFC 3042 (Limited Transmit)");

static int tcp_do_rfc3390 = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, rfc3390, CTLFLAG_RW,
    &tcp_do_rfc3390, 0,
    "Enable RFC 3390 (Increasing TCP's Initial Congestion Window)");

static int tcp_insecure_rst = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, insecure_rst, CTLFLAG_RW,
    &tcp_insecure_rst, 0,
    "Follow the old (insecure) criteria for accepting RST packets.");

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, reass, CTLFLAG_RW, 0,
	    "TCP Segment Reassembly Queue");

static int tcp_reass_maxseg = 0;
SYSCTL_INT(_net_inet_tcp_reass, OID_AUTO, maxsegments, CTLFLAG_RDTUN,
	   &tcp_reass_maxseg, 0,
	   "Global maximum number of TCP Segments in Reassembly Queue");

int tcp_reass_qsize = 0;
SYSCTL_INT(_net_inet_tcp_reass, OID_AUTO, cursegments, CTLFLAG_RD,
	   &tcp_reass_qsize, 0,
	   "Global number of TCP Segments currently in Reassembly Queue");

static int tcp_reass_maxqlen = 48;
SYSCTL_INT(_net_inet_tcp_reass, OID_AUTO, maxqlen, CTLFLAG_RW,
	   &tcp_reass_maxqlen, 0,
	   "Maximum number of TCP Segments per individual Reassembly Queue");

static int tcp_reass_overflows = 0;
SYSCTL_INT(_net_inet_tcp_reass, OID_AUTO, overflows, CTLFLAG_RD,
	   &tcp_reass_overflows, 0,
	   "Global number of TCP Segment Reassembly Queue Overflows");

struct inpcbhead tcb;
#define	tcb6	tcb  /* for KAME src sync over BSD*'s */
struct inpcbinfo tcbinfo;
struct mtx	*tcbinfo_mtx;

static void	 tcp_dooptions(struct tcpcb *, struct tcpopt *, u_char *,
		     int, int, struct tcphdr *);

static void	 tcp_pulloutofband(struct socket *,
		     struct tcphdr *, struct mbuf *, int);
static int	 tcp_reass(struct tcpcb *, struct tcphdr *, int *,
		     struct mbuf *);
static void	 tcp_xmit_timer(struct tcpcb *, int);
static void	 tcp_newreno_partial_ack(struct tcpcb *, struct tcphdr *);
static int	 tcp_timewait(struct tcptw *, struct tcpopt *,
		     struct tcphdr *, struct mbuf *, int);

/* Neighbor Discovery, Neighbor Unreachability Detection Upper layer hint. */
#ifdef INET6
#define ND6_HINT(tp) \
do { \
	if ((tp) && (tp)->t_inpcb && \
	    ((tp)->t_inpcb->inp_vflag & INP_IPV6) != 0) \
		nd6_nud_hint(NULL, NULL, 0); \
} while (0)
#else
#define ND6_HINT(tp)
#endif

/*
 * Indicate whether this ack should be delayed.  We can delay the ack if
 *	- there is no delayed ack timer in progress and
 *	- our last ack wasn't a 0-sized window.  We never want to delay
 *	  the ack that opens up a 0-sized window and
 *		- delayed acks are enabled or
 *		- this is a half-synchronized T/TCP connection.
 */
#define DELAY_ACK(tp)							\
	((!callout_active(tp->tt_delack) &&				\
	    (tp->t_flags & TF_RXWIN0SENT) == 0) &&			\
	    (tcp_delack_enabled || (tp->t_flags & TF_NEEDSYN)))

/* Initialize TCP reassembly queue */
uma_zone_t	tcp_reass_zone;
void
tcp_reass_init()
{
	tcp_reass_maxseg = nmbclusters / 16;
	TUNABLE_INT_FETCH("net.inet.tcp.reass.maxsegments",
	    &tcp_reass_maxseg);
	tcp_reass_zone = uma_zcreate("tcpreass", sizeof (struct tseg_qent),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(tcp_reass_zone, tcp_reass_maxseg);
}

static int
tcp_reass(tp, th, tlenp, m)
	register struct tcpcb *tp;
	register struct tcphdr *th;
	int *tlenp;
	struct mbuf *m;
{
	struct tseg_qent *q;
	struct tseg_qent *p = NULL;
	struct tseg_qent *nq;
	struct tseg_qent *te = NULL;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;

	INP_LOCK_ASSERT(tp->t_inpcb);

	/*
	 * XXX: tcp_reass() is rather inefficient with its data structures
	 * and should be rewritten (see NetBSD for optimizations).  While
	 * doing that it should move to its own file tcp_reass.c.
	 */

	/*
	 * Call with th==NULL after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (th == NULL)
		goto present;

	/*
	 * Limit the number of segments in the reassembly queue to prevent
	 * holding on to too many segments (and thus running out of mbufs).
	 * Make sure to let the missing segment through which caused this
	 * queue.  Always keep one global queue entry spare to be able to
	 * process the missing segment.
	 */
	if (th->th_seq != tp->rcv_nxt &&
	    (tcp_reass_qsize + 1 >= tcp_reass_maxseg ||
	     tp->t_segqlen >= tcp_reass_maxqlen)) {
		tcp_reass_overflows++;
		tcpstat.tcps_rcvmemdrop++;
		m_freem(m);
		*tlenp = 0;
		return (0);
	}

	/*
	 * Allocate a new queue entry. If we can't, or hit the zone limit
	 * just drop the pkt.
	 */
	te = uma_zalloc(tcp_reass_zone, M_NOWAIT);
	if (te == NULL) {
		tcpstat.tcps_rcvmemdrop++;
		m_freem(m);
		*tlenp = 0;
		return (0);
	}
	tp->t_segqlen++;
	tcp_reass_qsize++;

	/*
	 * Find a segment which begins after this one does.
	 */
	LIST_FOREACH(q, &tp->t_segq, tqe_q) {
		if (SEQ_GT(q->tqe_th->th_seq, th->th_seq))
			break;
		p = q;
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		register int i;
		/* conversion to int (in i) handles seq wraparound */
		i = p->tqe_th->th_seq + p->tqe_len - th->th_seq;
		if (i > 0) {
			if (i >= *tlenp) {
				tcpstat.tcps_rcvduppack++;
				tcpstat.tcps_rcvdupbyte += *tlenp;
				m_freem(m);
				uma_zfree(tcp_reass_zone, te);
				tp->t_segqlen--;
				tcp_reass_qsize--;
				/*
				 * Try to present any queued data
				 * at the left window edge to the user.
				 * This is needed after the 3-WHS
				 * completes.
				 */
				goto present;	/* ??? */
			}
			m_adj(m, i);
			*tlenp -= i;
			th->th_seq += i;
		}
	}
	tcpstat.tcps_rcvoopack++;
	tcpstat.tcps_rcvoobyte += *tlenp;

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q) {
		register int i = (th->th_seq + *tlenp) - q->tqe_th->th_seq;
		if (i <= 0)
			break;
		if (i < q->tqe_len) {
			q->tqe_th->th_seq += i;
			q->tqe_len -= i;
			m_adj(q->tqe_m, i);
			break;
		}

		nq = LIST_NEXT(q, tqe_q);
		LIST_REMOVE(q, tqe_q);
		m_freem(q->tqe_m);
		uma_zfree(tcp_reass_zone, q);
		tp->t_segqlen--;
		tcp_reass_qsize--;
		q = nq;
	}

	/* Insert the new segment queue entry into place. */
	te->tqe_m = m;
	te->tqe_th = th;
	te->tqe_len = *tlenp;

	if (p == NULL) {
		LIST_INSERT_HEAD(&tp->t_segq, te, tqe_q);
	} else {
		LIST_INSERT_AFTER(p, te, tqe_q);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (!TCPS_HAVEESTABLISHED(tp->t_state))
		return (0);
	q = LIST_FIRST(&tp->t_segq);
	if (!q || q->tqe_th->th_seq != tp->rcv_nxt)
		return (0);
	SOCKBUF_LOCK(&so->so_rcv);
	do {
		tp->rcv_nxt += q->tqe_len;
		flags = q->tqe_th->th_flags & TH_FIN;
		nq = LIST_NEXT(q, tqe_q);
		LIST_REMOVE(q, tqe_q);
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
			m_freem(q->tqe_m);
		else
			sbappendstream_locked(&so->so_rcv, q->tqe_m);
		uma_zfree(tcp_reass_zone, q);
		tp->t_segqlen--;
		tcp_reass_qsize--;
		q = nq;
	} while (q && q->tqe_th->th_seq == tp->rcv_nxt);
	ND6_HINT(tp);
	sorwakeup_locked(so);
	return (flags);
}

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
#ifdef INET6
int
tcp6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	register struct mbuf *m = *mp;
	struct in6_ifaddr *ia6;

	IP6_EXTHDR_CHECK(m, *offp, sizeof(struct tcphdr), IPPROTO_DONE);

	/*
	 * draft-itojun-ipv6-tcp-to-anycast
	 * better place to put this in?
	 */
	ia6 = ip6_getdstifaddr(m);
	if (ia6 && (ia6->ia6_flags & IN6_IFF_ANYCAST)) {
		struct ip6_hdr *ip6;

		ip6 = mtod(m, struct ip6_hdr *);
		icmp6_error(m, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADDR,
			    (caddr_t)&ip6->ip6_dst - (caddr_t)ip6);
		return IPPROTO_DONE;
	}

	tcp_input(m, *offp);
	return IPPROTO_DONE;
}
#endif

void
tcp_input(m, off0)
	register struct mbuf *m;
	int off0;
{
	register struct tcphdr *th;
	register struct ip *ip = NULL;
	register struct ipovly *ipov;
	register struct inpcb *inp = NULL;
	u_char *optp = NULL;
	int optlen = 0;
	int len, tlen, off;
	int drop_hdrlen;
	register struct tcpcb *tp = 0;
	register int thflags;
	struct socket *so = 0;
	int todrop, acked, ourfinisacked, needoutput = 0;
	u_long tiwin;
	struct tcpopt to;		/* options in this segment */
	struct rmxp_tao tao;		/* our TAO cache entry */
	int headlocked = 0;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif
	int rstreason; /* For badport_bandlim accounting purposes */

	struct ip6_hdr *ip6 = NULL;
#ifdef INET6
	int isipv6;
#else
	const int isipv6 = 0;
#endif

#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[40];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif

#ifdef INET6
	isipv6 = (mtod(m, struct ip *)->ip_v == 6) ? 1 : 0;
#endif
	bzero(&tao, sizeof(tao));
	bzero((char *)&to, sizeof(to));

	tcpstat.tcps_rcvtotal++;

	if (isipv6) {
#ifdef INET6
		/* IP6_EXTHDR_CHECK() is already done at tcp6_input() */
		ip6 = mtod(m, struct ip6_hdr *);
		tlen = sizeof(*ip6) + ntohs(ip6->ip6_plen) - off0;
		if (in6_cksum(m, IPPROTO_TCP, off0, tlen)) {
			tcpstat.tcps_rcvbadsum++;
			goto drop;
		}
		th = (struct tcphdr *)((caddr_t)ip6 + off0);

		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			goto drop;
		}
#else
		th = NULL;		/* XXX: avoid compiler warning */
#endif
	} else {
		/*
		 * Get IP and TCP header together in first mbuf.
		 * Note: IP leaves IP header in first mbuf.
		 */
		if (off0 > sizeof (struct ip)) {
			ip_stripoptions(m, (struct mbuf *)0);
			off0 = sizeof(struct ip);
		}
		if (m->m_len < sizeof (struct tcpiphdr)) {
			if ((m = m_pullup(m, sizeof (struct tcpiphdr))) == 0) {
				tcpstat.tcps_rcvshort++;
				return;
			}
		}
		ip = mtod(m, struct ip *);
		ipov = (struct ipovly *)ip;
		th = (struct tcphdr *)((caddr_t)ip + off0);
		tlen = ip->ip_len;

		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				th->th_sum = m->m_pkthdr.csum_data;
			else
				th->th_sum = in_pseudo(ip->ip_src.s_addr,
						ip->ip_dst.s_addr,
						htonl(m->m_pkthdr.csum_data +
							ip->ip_len +
							IPPROTO_TCP));
			th->th_sum ^= 0xffff;
#ifdef TCPDEBUG
			ipov->ih_len = (u_short)tlen;
			ipov->ih_len = htons(ipov->ih_len);
#endif
		} else {
			/*
			 * Checksum extended TCP header and data.
			 */
			len = sizeof (struct ip) + tlen;
			bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
			ipov->ih_len = (u_short)tlen;
			ipov->ih_len = htons(ipov->ih_len);
			th->th_sum = in_cksum(m, len);
		}
		if (th->th_sum) {
			tcpstat.tcps_rcvbadsum++;
			goto drop;
		}
#ifdef INET6
		/* Re-initialization for later version check */
		ip->ip_v = IPVERSION;
#endif
	}

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		tcpstat.tcps_rcvbadoff++;
		goto drop;
	}
	tlen -= off;	/* tlen is used instead of ti->ti_len */
	if (off > sizeof (struct tcphdr)) {
		if (isipv6) {
#ifdef INET6
			IP6_EXTHDR_CHECK(m, off0, off, );
			ip6 = mtod(m, struct ip6_hdr *);
			th = (struct tcphdr *)((caddr_t)ip6 + off0);
#endif
		} else {
			if (m->m_len < sizeof(struct ip) + off) {
				if ((m = m_pullup(m, sizeof (struct ip) + off))
						== 0) {
					tcpstat.tcps_rcvshort++;
					return;
				}
				ip = mtod(m, struct ip *);
				ipov = (struct ipovly *)ip;
				th = (struct tcphdr *)((caddr_t)ip + off0);
			}
		}
		optlen = off - sizeof (struct tcphdr);
		optp = (u_char *)(th + 1);
	}
	thflags = th->th_flags;

#ifdef TCP_DROP_SYNFIN
	/*
	 * If the drop_synfin option is enabled, drop all packets with
	 * both the SYN and FIN bits set. This prevents e.g. nmap from
	 * identifying the TCP/IP stack.
	 *
	 * This is a violation of the TCP specification.
	 */
	if (drop_synfin && (thflags & (TH_SYN|TH_FIN)) == (TH_SYN|TH_FIN))
		goto drop;
#endif

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	th->th_seq = ntohl(th->th_seq);
	th->th_ack = ntohl(th->th_ack);
	th->th_win = ntohs(th->th_win);
	th->th_urp = ntohs(th->th_urp);

	/*
	 * Delay dropping TCP, IP headers, IPv6 ext headers, and TCP options,
	 * until after ip6_savecontrol() is called and before other functions
	 * which don't want those proto headers.
	 * Because ip6_savecontrol() is going to parse the mbuf to
	 * search for data to be passed up to user-land, it wants mbuf
	 * parameters to be unchanged.
	 * XXX: the call of ip6_savecontrol() has been obsoleted based on
	 * latest version of the advanced API (20020110).
	 */
	drop_hdrlen = off0 + off;

	/*
	 * Locate pcb for segment.
	 */
	INP_INFO_WLOCK(&tcbinfo);
	headlocked = 1;
findpcb:
	KASSERT(headlocked, ("tcp_input: findpcb: head not locked"));
#ifdef IPFIREWALL_FORWARD
	/* Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain. */
	fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);

	if (fwd_tag != NULL && isipv6 == 0) {	/* IPv6 support is not yet */
		struct sockaddr_in *next_hop;

		next_hop = (struct sockaddr_in *)(fwd_tag+1);
		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * already got one like this?
		 */
		inp = in_pcblookup_hash(&tcbinfo,
					ip->ip_src, th->th_sport,
					ip->ip_dst, th->th_dport,
					0, m->m_pkthdr.rcvif);
		if (!inp) {
			/* It's new.  Try to find the ambushing socket. */
			inp = in_pcblookup_hash(&tcbinfo,
						ip->ip_src, th->th_sport,
						next_hop->sin_addr,
						next_hop->sin_port ?
						    ntohs(next_hop->sin_port) :
						    th->th_dport,
						1, m->m_pkthdr.rcvif);
		}
		/* Remove the tag from the packet.  We don't need it anymore. */
		m_tag_delete(m, fwd_tag);
	} else {
#endif /* IPFIREWALL_FORWARD */
		if (isipv6) {
#ifdef INET6
			inp = in6_pcblookup_hash(&tcbinfo,
						 &ip6->ip6_src, th->th_sport,
						 &ip6->ip6_dst, th->th_dport,
						 1, m->m_pkthdr.rcvif);
#endif
		} else
			inp = in_pcblookup_hash(&tcbinfo,
						ip->ip_src, th->th_sport,
						ip->ip_dst, th->th_dport,
						1, m->m_pkthdr.rcvif);
#ifdef IPFIREWALL_FORWARD
	}
#endif /* IPFIREWALL_FORWARD */

#if defined(IPSEC) || defined(FAST_IPSEC)
#ifdef INET6
	if (isipv6) {
		if (inp != NULL && ipsec6_in_reject(m, inp)) {
#ifdef IPSEC
			ipsec6stat.in_polvio++;
#endif
			goto drop;
		}
	} else
#endif /* INET6 */
	if (inp != NULL && ipsec4_in_reject(m, inp)) {
#ifdef IPSEC
		ipsecstat.in_polvio++;
#endif
		goto drop;
	}
#endif /*IPSEC || FAST_IPSEC*/

	/*
	 * If the state is CLOSED (i.e., TCB does not exist) then
	 * all data in the incoming segment is discarded.
	 * If the TCB exists but is in CLOSED state, it is embryonic,
	 * but should either do a listen or a connect soon.
	 */
	if (inp == NULL) {
		if (log_in_vain) {
#ifdef INET6
			char dbuf[INET6_ADDRSTRLEN+2], sbuf[INET6_ADDRSTRLEN+2];
#else
			char dbuf[4*sizeof "123"], sbuf[4*sizeof "123"];
#endif

			if (isipv6) {
#ifdef INET6
				strcpy(dbuf, "[");
				strcpy(sbuf, "[");
				strcat(dbuf, ip6_sprintf(&ip6->ip6_dst));
				strcat(sbuf, ip6_sprintf(&ip6->ip6_src));
				strcat(dbuf, "]");
				strcat(sbuf, "]");
#endif
			} else {
				strcpy(dbuf, inet_ntoa(ip->ip_dst));
				strcpy(sbuf, inet_ntoa(ip->ip_src));
			}
			switch (log_in_vain) {
			case 1:
				if ((thflags & TH_SYN) == 0)
					break;
				/* FALLTHROUGH */
			case 2:
				log(LOG_INFO,
				    "Connection attempt to TCP %s:%d "
				    "from %s:%d flags:0x%02x\n",
				    dbuf, ntohs(th->th_dport), sbuf,
				    ntohs(th->th_sport), thflags);
				break;
			default:
				break;
			}
		}
		if (blackhole) {
			switch (blackhole) {
			case 1:
				if (thflags & TH_SYN)
					goto drop;
				break;
			case 2:
				goto drop;
			default:
				goto drop;
			}
		}
		rstreason = BANDLIM_RST_CLOSEDPORT;
		goto dropwithreset;
	}
	INP_LOCK(inp);
	if (inp->inp_vflag & INP_TIMEWAIT) {
		/*
		 * The only option of relevance is TOF_CC, and only if
		 * present in a SYN segment.  See tcp_timewait().
		 */
		if (thflags & TH_SYN)
			tcp_dooptions((struct tcpcb *)NULL, &to, optp, optlen, 1, th);
		if (tcp_timewait((struct tcptw *)inp->inp_ppcb,
		    &to, th, m, tlen))
			goto findpcb;
		/*
		 * tcp_timewait unlocks inp.
		 */
		INP_INFO_WUNLOCK(&tcbinfo);
		return;
	}
	tp = intotcpcb(inp);
	if (tp == 0) {
		INP_UNLOCK(inp);
		rstreason = BANDLIM_RST_CLOSEDPORT;
		goto dropwithreset;
	}
	if (tp->t_state == TCPS_CLOSED)
		goto drop;

	/* Unscale the window into a 32-bit value. */
	if ((thflags & TH_SYN) == 0)
		tiwin = th->th_win << tp->snd_scale;
	else
		tiwin = th->th_win;

#ifdef MAC
	INP_LOCK_ASSERT(inp);
	if (mac_check_inpcb_deliver(inp, m))
		goto drop;
#endif
	so = inp->inp_socket;
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG) {
		ostate = tp->t_state;
		if (isipv6)
			bcopy((char *)ip6, (char *)tcp_saveipgen, sizeof(*ip6));
		else
			bcopy((char *)ip, (char *)tcp_saveipgen, sizeof(*ip));
		tcp_savetcp = *th;
	}
#endif
	if (so->so_options & SO_ACCEPTCONN) {
		struct in_conninfo inc;

#ifdef INET6
		inc.inc_isipv6 = isipv6;
#endif
		if (isipv6) {
			inc.inc6_faddr = ip6->ip6_src;
			inc.inc6_laddr = ip6->ip6_dst;
		} else {
			inc.inc_faddr = ip->ip_src;
			inc.inc_laddr = ip->ip_dst;
		}
		inc.inc_fport = th->th_sport;
		inc.inc_lport = th->th_dport;

	        /*
	         * If the state is LISTEN then ignore segment if it contains
		 * a RST.  If the segment contains an ACK then it is bad and
		 * send a RST.  If it does not contain a SYN then it is not
		 * interesting; drop it.
		 *
		 * If the state is SYN_RECEIVED (syncache) and seg contains
		 * an ACK, but not for our SYN/ACK, send a RST.  If the seg
		 * contains a RST, check the sequence number to see if it
		 * is a valid reset segment.
		 */
		if ((thflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN) {
			if ((thflags & (TH_RST|TH_ACK|TH_SYN)) == TH_ACK) {
				if (!syncache_expand(&inc, th, &so, m)) {
					/*
					 * No syncache entry, or ACK was not
					 * for our SYN/ACK.  Send a RST.
					 */
					tcpstat.tcps_badsyn++;
					rstreason = BANDLIM_RST_OPENPORT;
					goto dropwithreset;
				}
				if (so == NULL) {
					/*
					 * Could not complete 3-way handshake,
					 * connection is being closed down, and
					 * syncache will free mbuf.
					 */
					INP_UNLOCK(inp);
					INP_INFO_WUNLOCK(&tcbinfo);
					return;
				}
				/*
				 * Socket is created in state SYN_RECEIVED.
				 * Continue processing segment.
				 */
				INP_UNLOCK(inp);
				inp = sotoinpcb(so);
				INP_LOCK(inp);
				tp = intotcpcb(inp);
				/*
				 * This is what would have happened in
				 * tcp_output() when the SYN,ACK was sent.
				 */
				tp->snd_up = tp->snd_una;
				tp->snd_max = tp->snd_nxt = tp->iss + 1;
				tp->last_ack_sent = tp->rcv_nxt;
				/*
				 * RFC1323: The window in SYN & SYN/ACK
				 * segments is never scaled.
				 */
				tp->snd_wnd = tiwin;	/* unscaled */
				goto after_listen;
			}
			if (thflags & TH_RST) {
				syncache_chkrst(&inc, th);
				goto drop;
			}
			if (thflags & TH_ACK) {
				syncache_badack(&inc);
				tcpstat.tcps_badsyn++;
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
			}
			goto drop;
		}

		/*
		 * Segment's flags are (SYN) or (SYN|FIN).
		 */
#ifdef INET6
		/*
		 * If deprecated address is forbidden,
		 * we do not accept SYN to deprecated interface
		 * address to prevent any new inbound connection from
		 * getting established.
		 * When we do not accept SYN, we send a TCP RST,
		 * with deprecated source address (instead of dropping
		 * it).  We compromise it as it is much better for peer
		 * to send a RST, and RST will be the final packet
		 * for the exchange.
		 *
		 * If we do not forbid deprecated addresses, we accept
		 * the SYN packet.  RFC2462 does not suggest dropping
		 * SYN in this case.
		 * If we decipher RFC2462 5.5.4, it says like this:
		 * 1. use of deprecated addr with existing
		 *    communication is okay - "SHOULD continue to be
		 *    used"
		 * 2. use of it with new communication:
		 *   (2a) "SHOULD NOT be used if alternate address
		 *        with sufficient scope is available"
		 *   (2b) nothing mentioned otherwise.
		 * Here we fall into (2b) case as we have no choice in
		 * our source address selection - we must obey the peer.
		 *
		 * The wording in RFC2462 is confusing, and there are
		 * multiple description text for deprecated address
		 * handling - worse, they are not exactly the same.
		 * I believe 5.5.4 is the best one, so we follow 5.5.4.
		 */
		if (isipv6 && !ip6_use_deprecated) {
			struct in6_ifaddr *ia6;

			if ((ia6 = ip6_getdstifaddr(m)) &&
			    (ia6->ia6_flags & IN6_IFF_DEPRECATED)) {
				INP_UNLOCK(inp);
				tp = NULL;
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
			}
		}
#endif
		/*
		 * If it is from this socket, drop it, it must be forged.
		 * Don't bother responding if the destination was a broadcast.
		 */
		if (th->th_dport == th->th_sport) {
			if (isipv6) {
				if (IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
						       &ip6->ip6_src))
					goto drop;
			} else {
				if (ip->ip_dst.s_addr == ip->ip_src.s_addr)
					goto drop;
			}
		}
		/*
		 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
		 *
		 * Note that it is quite possible to receive unicast
		 * link-layer packets with a broadcast IP address. Use
		 * in_broadcast() to find them.
		 */
		if (m->m_flags & (M_BCAST|M_MCAST))
			goto drop;
		if (isipv6) {
			if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
			    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src))
				goto drop;
		} else {
			if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
			    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
			    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
			    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
				goto drop;
		}
		/*
		 * SYN appears to be valid; create compressed TCP state
		 * for syncache, or perform t/tcp connection.
		 */
		if (so->so_qlen <= so->so_qlimit) {
#ifdef TCPDEBUG
			if (so->so_options & SO_DEBUG)
				tcp_trace(TA_INPUT, ostate, tp,
				    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
			tcp_dooptions(tp, &to, optp, optlen, 1, th);
			if (!syncache_add(&inc, &to, th, &so, m))
				goto drop;
			if (so == NULL) {
				/*
				 * Entry added to syncache, mbuf used to
				 * send SYN,ACK packet.
				 */
				KASSERT(headlocked, ("headlocked"));
				INP_UNLOCK(inp);
				INP_INFO_WUNLOCK(&tcbinfo);
				return;
			}
			/*
			 * Segment passed TAO tests.
			 */
			INP_UNLOCK(inp);
			inp = sotoinpcb(so);
			INP_LOCK(inp);
			tp = intotcpcb(inp);
			tp->snd_wnd = tiwin;
			tp->t_starttime = ticks;
			tp->t_state = TCPS_ESTABLISHED;

			/*
			 * T/TCP logic:
			 * If there is a FIN or if there is data, then
			 * delay SYN,ACK(SYN) in the hope of piggy-backing
			 * it on a response segment.  Otherwise must send
			 * ACK now in case the other side is slow starting.
			 */
			if (thflags & TH_FIN || tlen != 0)
				tp->t_flags |= (TF_DELACK | TF_NEEDSYN);
			else
				tp->t_flags |= (TF_ACKNOW | TF_NEEDSYN);
			tcpstat.tcps_connects++;
			soisconnected(so);
			goto trimthenstep6;
		}
		goto drop;
	}
after_listen:
	KASSERT(headlocked, ("tcp_input: after_listen: head not locked"));
	INP_LOCK_ASSERT(inp);

	/* XXX temp debugging */
	/* should not happen - syncache should pick up these connections */
	if (tp->t_state == TCPS_LISTEN)
		panic("tcp_input: TCPS_LISTEN");

	/*
	 * This is the second part of the MSS DoS prevention code (after
	 * minmss on the sending side) and it deals with too many too small
	 * tcp packets in a too short timeframe (1 second).
	 *
	 * For every full second we count the number of received packets
	 * and bytes. If we get a lot of packets per second for this connection
	 * (tcp_minmssoverload) we take a closer look at it and compute the
	 * average packet size for the past second. If that is less than
	 * tcp_minmss we get too many packets with very small payload which
	 * is not good and burdens our system (and every packet generates
	 * a wakeup to the process connected to our socket). We can reasonable
	 * expect this to be small packet DoS attack to exhaust our CPU
	 * cycles.
	 *
	 * Care has to be taken for the minimum packet overload value. This
	 * value defines the minimum number of packets per second before we
	 * start to worry. This must not be too low to avoid killing for
	 * example interactive connections with many small packets like
	 * telnet or SSH.
	 *
	 * Setting either tcp_minmssoverload or tcp_minmss to "0" disables
	 * this check.
	 *
	 * Account for packet if payload packet, skip over ACK, etc.
	 */
	if (tcp_minmss && tcp_minmssoverload &&
	    tp->t_state == TCPS_ESTABLISHED && tlen > 0) {
		if (tp->rcv_second > ticks) {
			tp->rcv_pps++;
			tp->rcv_byps += tlen + off;
			if (tp->rcv_pps > tcp_minmssoverload) {
				if ((tp->rcv_byps / tp->rcv_pps) < tcp_minmss) {
					printf("too many small tcp packets from "
					       "%s:%u, av. %lubyte/packet, "
					       "dropping connection\n",
#ifdef INET6
						isipv6 ?
						ip6_sprintf(&inp->inp_inc.inc6_faddr) :
#endif
						inet_ntoa(inp->inp_inc.inc_faddr),
						inp->inp_inc.inc_fport,
						tp->rcv_byps / tp->rcv_pps);
					tp = tcp_drop(tp, ECONNRESET);
					tcpstat.tcps_minmssdrops++;
					goto drop;
				}
			}
		} else {
			tp->rcv_second = ticks + hz;
			tp->rcv_pps = 1;
			tp->rcv_byps = tlen + off;
		}
	}

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_rcvtime = ticks;
	if (TCPS_HAVEESTABLISHED(tp->t_state))
		callout_reset(tp->tt_keep, tcp_keepidle, tcp_timer_keep, tp);

	/*
	 * Process options only when we get SYN/ACK back. The SYN case
	 * for incoming connections is handled in tcp_syncache.
	 * XXX this is traditional behavior, may need to be cleaned up.
	 */
	tcp_dooptions(tp, &to, optp, optlen, thflags & TH_SYN, th);
	if (thflags & TH_SYN) {
		if (to.to_flags & TOF_SCALE) {
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = to.to_requested_s_scale;
		}
		if (to.to_flags & TOF_TS) {
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = to.to_tsval;
			tp->ts_recent_age = ticks;
		}
		if (to.to_flags & (TOF_CC|TOF_CCNEW))
			tp->t_flags |= TF_RCVD_CC;
		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);
		if (tp->sack_enable) {
			if (!(to.to_flags & TOF_SACK))
				tp->sack_enable = 0;
			else
				tp->t_flags |= TF_SACK_PERMIT;
		}

	}

	if (tp->sack_enable) {
		/* Delete stale (cumulatively acked) SACK holes */
		tcp_del_sackholes(tp, th);
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
	 * Since we check for TCPS_ESTABLISHED above, it can only
	 * be TH_NEEDSYN.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (thflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    ((tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN)) == 0) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	     TSTMP_GEQ(to.to_tsval, tp->ts_recent)) &&
	    /*
	     * Using the CC option is compulsory if once started:
	     *   the segment is OK if no T/TCP was negotiated or
	     *   if the segment has a CC option equal to CCrecv
	     */
	    ((tp->t_flags & (TF_REQ_CC|TF_RCVD_CC)) != (TF_REQ_CC|TF_RCVD_CC) ||
	     ((to.to_flags & TOF_CC) != 0 && to.to_cc == tp->cc_recv)) &&
	    th->th_seq == tp->rcv_nxt &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {

		/*
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 */
		if ((to.to_flags & TOF_TS) != 0 &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = ticks;
			tp->ts_recent = to.to_tsval;
		}

		if (tlen == 0) {
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_wnd &&
			    ((!tcp_do_newreno && !tp->sack_enable &&
			      tp->t_dupacks < tcprexmtthresh) ||
			     ((tcp_do_newreno || tp->sack_enable) &&
			      !IN_FASTRECOVERY(tp)))) {
				KASSERT(headlocked, ("headlocked"));
				INP_INFO_WUNLOCK(&tcbinfo);
				headlocked = 0;
				/*
				 * this is a pure ack for outstanding data.
				 */
				++tcpstat.tcps_predack;
				/*
				 * "bad retransmit" recovery
				 */
				if (tp->t_rxtshift == 1 &&
				    ticks < tp->t_badrxtwin) {
					++tcpstat.tcps_sndrexmitbad;
					tp->snd_cwnd = tp->snd_cwnd_prev;
					tp->snd_ssthresh =
					    tp->snd_ssthresh_prev;
					tp->snd_recover = tp->snd_recover_prev;
					if (tp->t_flags & TF_WASFRECOVERY)
					    ENTER_FASTRECOVERY(tp);
					tp->snd_nxt = tp->snd_max;
					tp->t_badrxtwin = 0;
				}

				/*
				 * Recalculate the transmit timer / rtt.
				 *
				 * Some boxes send broken timestamp replies
				 * during the SYN+ACK phase, ignore
				 * timestamps of 0 or we could calculate a
				 * huge RTT and blow up the retransmit timer.
				 */
				if ((to.to_flags & TOF_TS) != 0 &&
				    to.to_tsecr) {
					tcp_xmit_timer(tp,
					    ticks - to.to_tsecr + 1);
				} else if (tp->t_rtttime &&
					    SEQ_GT(th->th_ack, tp->t_rtseq)) {
					tcp_xmit_timer(tp,
							ticks - tp->t_rtttime);
				}
				tcp_xmit_bandwidth_limit(tp, th->th_ack);
				acked = th->th_ack - tp->snd_una;
				tcpstat.tcps_rcvackpack++;
				tcpstat.tcps_rcvackbyte += acked;
				sbdrop(&so->so_snd, acked);
				if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
				    SEQ_LEQ(th->th_ack, tp->snd_recover))
					tp->snd_recover = th->th_ack - 1;
				tp->snd_una = th->th_ack;
				/*
				 * pull snd_wl2 up to prevent seq wrap relative
				 * to th_ack.
				 */
				tp->snd_wl2 = th->th_ack;
				tp->t_dupacks = 0;
				m_freem(m);
				ND6_HINT(tp); /* some progress has been done */

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.

#ifdef TCPDEBUG
				if (so->so_options & SO_DEBUG)
					tcp_trace(TA_INPUT, ostate, tp,
					    (void *)tcp_saveipgen,
					    &tcp_savetcp, 0);
#endif
				 */
				if (tp->snd_una == tp->snd_max)
					callout_stop(tp->tt_rexmt);
				else if (!callout_active(tp->tt_persist))
					callout_reset(tp->tt_rexmt,
						      tp->t_rxtcur,
						      tcp_timer_rexmt, tp);

				sowwakeup(so);
				if (so->so_snd.sb_cc)
					(void) tcp_output(tp);
				goto check_delack;
			}
		} else if (th->th_ack == tp->snd_una &&
		    LIST_EMPTY(&tp->t_segq) &&
		    tlen <= sbspace(&so->so_rcv)) {
			KASSERT(headlocked, ("headlocked"));
			INP_INFO_WUNLOCK(&tcbinfo);
			headlocked = 0;
			/*
			 * this is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
			/* Clean receiver SACK report if present */
			if (tp->sack_enable && tp->rcv_numsacks)
				tcp_clean_sackreport(tp);
			++tcpstat.tcps_preddat;
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
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += tlen;
			ND6_HINT(tp);	/* some progress has been done */
			/*
#ifdef TCPDEBUG
			if (so->so_options & SO_DEBUG)
				tcp_trace(TA_INPUT, ostate, tp,
				    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
			 * Add data to socket buffer.
			 */
			SOCKBUF_LOCK(&so->so_rcv);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
				m_freem(m);
			} else {
				m_adj(m, drop_hdrlen);	/* delayed header drop */
				sbappendstream_locked(&so->so_rcv, m);
			}
			sorwakeup_locked(so);
			if (DELAY_ACK(tp)) {
				tp->t_flags |= TF_DELACK;
			} else {
				tp->t_flags |= TF_ACKNOW;
				tcp_output(tp);
			}
			goto check_delack;
		}
	}

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	{ int win;

	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));
	}

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
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if (tcp_do_rfc1644)
			tcp_hc_gettao(&inp->inp_inc, &tao);

		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
			/*
			 * If we have a cached CCsent for the remote host,
			 * hence we haven't just crashed and restarted,
			 * do not send a RST.  This may be a retransmission
			 * from the other side after our earlier ACK was lost.
			 * Our new SYN, when it arrives, will serve as the
			 * needed ACK.
			 */
			if (tao.tao_ccsent != 0)
				goto drop;
			else {
				rstreason = BANDLIM_UNLIMITED;
				goto dropwithreset;
			}
		}
		if (thflags & TH_RST) {
			if (thflags & TH_ACK)
				tp = tcp_drop(tp, ECONNREFUSED);
			goto drop;
		}
		if ((thflags & TH_SYN) == 0)
			goto drop;
		tp->snd_wnd = th->th_win;	/* initial send window */
		tp->cc_recv = to.to_cc;		/* foreign CC */

		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		if (thflags & TH_ACK) {
			/*
			 * Our SYN was acked.  If segment contains CC.ECHO
			 * option, check it to make sure this segment really
			 * matches our SYN.  If not, just drop it as old
			 * duplicate, but send an RST if we're still playing
			 * by the old rules.  If no CC.ECHO option, make sure
			 * we don't get fooled into using T/TCP.
			 */
			if (to.to_flags & TOF_CCECHO) {
				if (tp->cc_send != to.to_ccecho) {
					if (tao.tao_ccsent != 0)
						goto drop;
					else {
						rstreason = BANDLIM_UNLIMITED;
						goto dropwithreset;
					}
				}
			} else
				tp->t_flags &= ~TF_RCVD_CC;
			tcpstat.tcps_connects++;
			soisconnected(so);
#ifdef MAC
			SOCK_LOCK(so);
			mac_set_socket_peer_from_mbuf(m, so);
			SOCK_UNLOCK(so);
#endif
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			/* Segment is acceptable, update cache if undefined. */
			if (tao.tao_ccsent == 0 && tcp_do_rfc1644)
				tcp_hc_updatetao(&inp->inp_inc, TCP_HC_TAO_CCSENT, to.to_ccecho, 0);

			tp->rcv_adv += tp->rcv_wnd;
			tp->snd_una++;		/* SYN is acked */
			/*
			 * If there's data, delay ACK; if there's also a FIN
			 * ACKNOW will be turned on later.
			 */
			if (DELAY_ACK(tp) && tlen != 0)
				callout_reset(tp->tt_delack, tcp_delacktime,
				    tcp_timer_delack, tp);
			else
				tp->t_flags |= TF_ACKNOW;
			/*
			 * Received <SYN,ACK> in SYN_SENT[*] state.
			 * Transitions:
			 *	SYN_SENT  --> ESTABLISHED
			 *	SYN_SENT* --> FIN_WAIT_1
			 */
			tp->t_starttime = ticks;
			if (tp->t_flags & TF_NEEDFIN) {
				tp->t_state = TCPS_FIN_WAIT_1;
				tp->t_flags &= ~TF_NEEDFIN;
				thflags &= ~TH_SYN;
			} else {
				tp->t_state = TCPS_ESTABLISHED;
				callout_reset(tp->tt_keep, tcp_keepidle,
					      tcp_timer_keep, tp);
			}
		} else {
			/*
			 * Received initial SYN in SYN-SENT[*] state =>
			 * simultaneous open.  If segment contains CC option
			 * and there is a cached CC, apply TAO test.
			 * If it succeeds, connection is * half-synchronized.
			 * Otherwise, do 3-way handshake:
			 *        SYN-SENT -> SYN-RECEIVED
			 *        SYN-SENT* -> SYN-RECEIVED*
			 * If there was no CC option, clear cached CC value.
			 */
			tp->t_flags |= TF_ACKNOW;
			callout_stop(tp->tt_rexmt);
			if (to.to_flags & TOF_CC) {
				if (tao.tao_cc != 0 &&
				    CC_GT(to.to_cc, tao.tao_cc)) {
					/*
					 * update cache and make transition:
					 *        SYN-SENT -> ESTABLISHED*
					 *        SYN-SENT* -> FIN-WAIT-1*
					 */
					tao.tao_cc = to.to_cc;
					tcp_hc_updatetao(&inp->inp_inc,
						TCP_HC_TAO_CC, to.to_cc, 0);
					tp->t_starttime = ticks;
					if (tp->t_flags & TF_NEEDFIN) {
						tp->t_state = TCPS_FIN_WAIT_1;
						tp->t_flags &= ~TF_NEEDFIN;
					} else {
						tp->t_state = TCPS_ESTABLISHED;
						callout_reset(tp->tt_keep,
							      tcp_keepidle,
							      tcp_timer_keep,
							      tp);
					}
					tp->t_flags |= TF_NEEDSYN;
				} else
					tp->t_state = TCPS_SYN_RECEIVED;
			} else {
				if (tcp_do_rfc1644) {
					/* CC.NEW or no option => invalidate cache */
					tao.tao_cc = 0;
					tcp_hc_updatetao(&inp->inp_inc,
						TCP_HC_TAO_CC, to.to_cc, 0);
				}
				tp->t_state = TCPS_SYN_RECEIVED;
			}
		}

trimthenstep6:
		KASSERT(headlocked, ("tcp_input: trimthenstep6: head not "
		    "locked"));
		INP_LOCK_ASSERT(inp);

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
			tcpstat.tcps_rcvpackafterwin++;
			tcpstat.tcps_rcvbyteafterwin += todrop;
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
	 *	if segment contains a SYN and CC [not CC.NEW] option:
	 *              if state == TIME_WAIT and connection duration > MSL,
	 *                  drop packet and send RST;
	 *
	 *		if SEG.CC > CCrecv then is new SYN, and can implicitly
	 *		    ack the FIN (and data) in retransmission queue.
	 *                  Complete close and delete TCPCB.  Then reprocess
	 *                  segment, hoping to find new TCPCB in LISTEN state;
	 *
	 *		else must be old SYN; drop it.
	 *      else do normal processing.
	 */
	case TCPS_LAST_ACK:
	case TCPS_CLOSING:
	case TCPS_TIME_WAIT:
		KASSERT(tp->t_state != TCPS_TIME_WAIT, ("timewait"));
		if ((thflags & TH_SYN) &&
		    (to.to_flags & TOF_CC) && tp->cc_recv != 0) {
			if (tp->t_state == TCPS_TIME_WAIT &&
					(ticks - tp->t_starttime) > tcp_msl) {
				rstreason = BANDLIM_UNLIMITED;
				goto dropwithreset;
			}
			if (CC_GT(to.to_cc, tp->cc_recv)) {
				tp = tcp_close(tp);
				goto findpcb;
			}
			else
				goto drop;
		}
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
	 *
	 *
	 * If the RST bit is set, check the sequence number to see
	 * if this is a valid reset segment.
	 * RFC 793 page 37:
	 *   In all states except SYN-SENT, all reset (RST) segments
	 *   are validated by checking their SEQ-fields.  A reset is
	 *   valid if its sequence number is in the window.
	 * Note: this does not take into account delayed ACKs, so
	 *   we should test against last_ack_sent instead of rcv_nxt.
	 *   The sequence number in the reset segment is normally an
	 *   echo of our outgoing acknowlegement numbers, but some hosts
	 *   send a reset with the sequence number at the rightmost edge
	 *   of our receive window, and we have to handle this case.
	 * Note 2: Paul Watson's paper "Slipping in the Window" has shown
	 *   that brute force RST attacks are possible.  To combat this,
	 *   we use a much stricter check while in the ESTABLISHED state,
	 *   only accepting RSTs where the sequence number is equal to
	 *   last_ack_sent.  In all other states (the states in which a
	 *   RST is more likely), the more permissive check is used.
	 * If we have multiple segments in flight, the intial reset
	 * segment sequence numbers will be to the left of last_ack_sent,
	 * but they will eventually catch up.
	 * In any case, it never made sense to trim reset segments to
	 * fit the receive window since RFC 1122 says:
	 *   4.2.2.12  RST Segment: RFC-793 Section 3.4
	 *
	 *    A TCP SHOULD allow a received RST segment to include data.
	 *
	 *    DISCUSSION
	 *         It has been suggested that a RST segment could contain
	 *         ASCII text that encoded and explained the cause of the
	 *         RST.  No standard has yet been established for such
	 *         data.
	 *
	 * If the reset segment passes the sequence number test examine
	 * the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK STATES:
	 *	Close the tcb.
	 *    TIME_WAIT STATE:
	 *	Drop the segment - see Stevens, vol. 2, p. 964 and
	 *      RFC 1337.
	 */
	if (thflags & TH_RST) {
		if ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
		    (tp->rcv_wnd == 0 && tp->last_ack_sent == th->th_seq)) {
			switch (tp->t_state) {

			case TCPS_SYN_RECEIVED:
				so->so_error = ECONNREFUSED;
				goto close;

			case TCPS_ESTABLISHED:
				if (tp->last_ack_sent != th->th_seq &&
			 	    tcp_insecure_rst == 0) {
					tcpstat.tcps_badrst++;
					goto drop;
				}
			case TCPS_FIN_WAIT_1:
			case TCPS_FIN_WAIT_2:
			case TCPS_CLOSE_WAIT:
				so->so_error = ECONNRESET;
			close:
				tp->t_state = TCPS_CLOSED;
				tcpstat.tcps_drops++;
				tp = tcp_close(tp);
				break;

			case TCPS_CLOSING:
			case TCPS_LAST_ACK:
				tp = tcp_close(tp);
				break;

			case TCPS_TIME_WAIT:
				KASSERT(tp->t_state != TCPS_TIME_WAIT,
				    ("timewait"));
				break;
			}
		}
		goto drop;
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if ((int)(ticks - tp->ts_recent_age) > TCP_PAWS_IDLE) {
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
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += tlen;
			tcpstat.tcps_pawsdrop++;
			if (tlen)
				goto dropafterack;
			goto drop;
		}
	}

	/*
	 * T/TCP mechanism
	 *   If T/TCP was negotiated and the segment doesn't have CC,
	 *   or if its CC is wrong then drop the segment.
	 *   RST segments do not have to comply with this.
	 */
	if ((tp->t_flags & (TF_REQ_CC|TF_RCVD_CC)) == (TF_REQ_CC|TF_RCVD_CC) &&
	    ((to.to_flags & TOF_CC) == 0 || tp->cc_recv != to.to_cc))
		goto dropafterack;

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
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += todrop;
		} else {
			tcpstat.tcps_rcvpartduppack++;
			tcpstat.tcps_rcvpartdupbyte += todrop;
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
		tp = tcp_close(tp);
		tcpstat.tcps_rcvafterclose++;
		rstreason = BANDLIM_UNLIMITED;
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq+tlen) - (tp->rcv_nxt+tp->rcv_wnd);
	if (todrop > 0) {
		tcpstat.tcps_rcvpackafterwin++;
		if (todrop >= tlen) {
			tcpstat.tcps_rcvbyteafterwin += tlen;
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			KASSERT(tp->t_state != TCPS_TIME_WAIT, ("timewait"));
			if (thflags & TH_SYN &&
			    tp->t_state == TCPS_TIME_WAIT &&
			    SEQ_GT(th->th_seq, tp->rcv_nxt)) {
				tp = tcp_close(tp);
				goto findpcb;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				tcpstat.tcps_rcvwinprobe++;
			} else
				goto dropafterack;
		} else
			tcpstat.tcps_rcvbyteafterwin += todrop;
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
	if ((to.to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		((thflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = ticks;
		tp->ts_recent = to.to_tsval;
	}

	/*
	 * If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 */
	if (thflags & TH_SYN) {
		tp = tcp_drop(tp, ECONNRESET);
		rstreason = BANDLIM_UNLIMITED;
		goto drop;
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

		tcpstat.tcps_connects++;
		soisconnected(so);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		/*
		 * Upon successful completion of 3-way handshake,
		 * update cache.CC, pass any queued data to the user,
		 * and advance state appropriately.
		 */
		if (tcp_do_rfc1644) {
			tao.tao_cc = tp->cc_recv;
			tcp_hc_updatetao(&inp->inp_inc, TCP_HC_TAO_CC,
					 tp->cc_recv, 0);
		}
		/*
		 * Make transitions:
		 *      SYN-RECEIVED  -> ESTABLISHED
		 *      SYN-RECEIVED* -> FIN-WAIT-1
		 */
		tp->t_starttime = ticks;
		if (tp->t_flags & TF_NEEDFIN) {
			tp->t_state = TCPS_FIN_WAIT_1;
			tp->t_flags &= ~TF_NEEDFIN;
		} else {
			tp->t_state = TCPS_ESTABLISHED;
			callout_reset(tp->tt_keep, tcp_keepidle,
				      tcp_timer_keep, tp);
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
	case TCPS_TIME_WAIT:
		KASSERT(tp->t_state != TCPS_TIME_WAIT, ("timewait"));
		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			if (tlen == 0 && tiwin == tp->snd_wnd) {
				tcpstat.tcps_rcvdupack++;
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
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
				 */
				if (!callout_active(tp->tt_rexmt) ||
				    th->th_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks > tcprexmtthresh ||
					 ((tcp_do_newreno || tp->sack_enable) &&
					  IN_FASTRECOVERY(tp))) {

                                        if (tp->sack_enable && IN_FASTRECOVERY(tp)) {
						int data_in_pipe;
						int sacked, lost_not_rexmitted;
						
						/*
						 * Compute the amount of data in flight first.
						 * We can inject new data into the pipe iff 
						 * we have less than 1/2 the original window's 	
						 * worth of data in flight.
						 */
						sacked = tcp_sacked_bytes(tp, &lost_not_rexmitted);
						data_in_pipe = (tp->snd_nxt - tp->snd_una) -
							(sacked + lost_not_rexmitted);
						if (data_in_pipe < tp->snd_ssthresh) {
							tp->snd_cwnd += tp->t_maxseg;
							if (tp->snd_cwnd > tp->snd_ssthresh)
								tp->snd_cwnd = tp->snd_ssthresh;
						}
					} else
						tp->snd_cwnd += tp->t_maxseg;
					(void) tcp_output(tp);
					goto drop;
				} else if (tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;
					u_int win;

					/*
					 * If we're doing sack, check to
					 * see if we're already in sack
					 * recovery. If we're not doing sack,
					 * check to see if we're in newreno
					 * recovery.
					 */
					if (tp->sack_enable) {
						if (IN_FASTRECOVERY(tp)) {
							tp->t_dupacks = 0;
							break;
						}
					} else if (tcp_do_newreno) {
						if (SEQ_LEQ(th->th_ack,
						    tp->snd_recover)) {
							tp->t_dupacks = 0;
							break;
						}
					}
					win = min(tp->snd_wnd, tp->snd_cwnd) /
					    2 / tp->t_maxseg;
					if (win < 2)
						win = 2;
					tp->snd_ssthresh = win * tp->t_maxseg;
					ENTER_FASTRECOVERY(tp);
					tp->snd_recover = tp->snd_max;
					callout_stop(tp->tt_rexmt);
					tp->t_rtttime = 0;
					if (tp->sack_enable) {
						tcpstat.tcps_sack_recovery_episode++;
						tp->sack_newdata = tp->snd_nxt;
						tp->snd_cwnd = tp->t_maxseg;
						(void) tcp_output(tp);
						goto drop;
					}
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tcp_output(tp);
					KASSERT(tp->snd_limited <= 2,
					    ("tp->snd_limited too big"));
					tp->snd_cwnd = tp->snd_ssthresh +
					     tp->t_maxseg *
					     (tp->t_dupacks - tp->snd_limited);
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (tcp_do_rfc3042) {
					u_long oldcwnd = tp->snd_cwnd;
					tcp_seq oldsndmax = tp->snd_max;
					u_int sent;

					KASSERT(tp->t_dupacks == 1 ||
					    tp->t_dupacks == 2,
					    ("dupacks not 1 or 2"));
					if (tp->t_dupacks == 1)
						tp->snd_limited = 0;
					tp->snd_cwnd =
					    (tp->snd_nxt - tp->snd_una) +
					    (tp->t_dupacks - tp->snd_limited) *
					    tp->t_maxseg;
					(void) tcp_output(tp);
					sent = tp->snd_max - oldsndmax;
					if (sent > tp->t_maxseg) {
						KASSERT((tp->t_dupacks == 2 &&
						    tp->snd_limited == 0) ||
						   (sent == tp->t_maxseg + 1 &&
						    tp->t_flags & TF_SENTFIN),
						    ("sent too much"));
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

		KASSERT(SEQ_GT(th->th_ack, tp->snd_una), ("th_ack <= snd_una"));

		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (tcp_do_newreno || tp->sack_enable) {
			if (IN_FASTRECOVERY(tp)) {
				if (SEQ_LT(th->th_ack, tp->snd_recover)) {
					if (tp->sack_enable)
						tcp_sack_partialack(tp, th);
					else
						tcp_newreno_partial_ack(tp, th);
				} else {
					/*
					 * Out of fast recovery.
					 * Window inflation should have left us
					 * with approximately snd_ssthresh
					 * outstanding data.
					 * But in case we would be inclined to
					 * send a burst, better to do it via
					 * the slow start mechanism.
					 */
					if (SEQ_GT(th->th_ack +
							tp->snd_ssthresh,
						   tp->snd_max))
						tp->snd_cwnd = tp->snd_max -
								th->th_ack +
								tp->t_maxseg;
					else
						tp->snd_cwnd = tp->snd_ssthresh;
				}
			}
		} else {
			if (tp->t_dupacks >= tcprexmtthresh &&
			    tp->snd_cwnd > tp->snd_ssthresh)
				tp->snd_cwnd = tp->snd_ssthresh;
		}
		tp->t_dupacks = 0;
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			tcpstat.tcps_rcvacktoomuch++;
			goto dropafterack;
		}
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
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
		}

process_ACK:
		KASSERT(headlocked, ("tcp_input: process_ACK: head not "
		    "locked"));
		INP_LOCK_ASSERT(inp);

		acked = th->th_ack - tp->snd_una;
		tcpstat.tcps_rcvackpack++;
		tcpstat.tcps_rcvackbyte += acked;

		/*
		 * If we just performed our first retransmit, and the ACK
		 * arrives within our recovery window, then it was a mistake
		 * to do the retransmit in the first place.  Recover our
		 * original cwnd and ssthresh, and proceed to transmit where
		 * we left off.
		 */
		if (tp->t_rxtshift == 1 && ticks < tp->t_badrxtwin) {
			++tcpstat.tcps_sndrexmitbad;
			tp->snd_cwnd = tp->snd_cwnd_prev;
			tp->snd_ssthresh = tp->snd_ssthresh_prev;
			tp->snd_recover = tp->snd_recover_prev;
			if (tp->t_flags & TF_WASFRECOVERY)
				ENTER_FASTRECOVERY(tp);
			tp->snd_nxt = tp->snd_max;
			tp->t_badrxtwin = 0;	/* XXX probably not required */
		}

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
		if ((to.to_flags & TOF_TS) != 0 &&
		    to.to_tsecr) {
			tcp_xmit_timer(tp, ticks - to.to_tsecr + 1);
		} else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq)) {
			tcp_xmit_timer(tp, ticks - tp->t_rtttime);
		}
		tcp_xmit_bandwidth_limit(tp, th->th_ack);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			callout_stop(tp->tt_rexmt);
			needoutput = 1;
		} else if (!callout_active(tp->tt_persist))
			callout_reset(tp->tt_rexmt, tp->t_rxtcur,
				      tcp_timer_rexmt, tp);

		/*
		 * If no data (only SYN) was ACK'd,
		 *    skip rest of ACK processing.
		 */
		if (acked == 0)
			goto step6;

		/*
		 * When new data is acked, open the congestion window.
		 * If the window gives us less than ssthresh packets
		 * in flight, open exponentially (maxseg per packet).
		 * Otherwise open linearly: maxseg per window
		 * (maxseg^2 / cwnd per packet).
		 */
		if ((!tcp_do_newreno && !tp->sack_enable) ||
		    !IN_FASTRECOVERY(tp)) {
			register u_int cw = tp->snd_cwnd;
			register u_int incr = tp->t_maxseg;
			if (cw > tp->snd_ssthresh)
				incr = incr * incr / cw;
			tp->snd_cwnd = min(cw+incr, TCP_MAXWIN<<tp->snd_scale);
		}
		SOCKBUF_LOCK(&so->so_snd);
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop_locked(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop_locked(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		sowwakeup_locked(so);
		/* detect una wraparound */
		if ((tcp_do_newreno || tp->sack_enable) &&
		    !IN_FASTRECOVERY(tp) &&
		    SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
		if ((tcp_do_newreno || tp->sack_enable) &&
		    IN_FASTRECOVERY(tp) &&
		    SEQ_GEQ(th->th_ack, tp->snd_recover))
			EXIT_FASTRECOVERY(tp);
		tp->snd_una = th->th_ack;
		if (tp->sack_enable) {
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
				 */
		/* XXXjl
		 * we should release the tp also, and use a
		 * compressed state.
		 */
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					callout_reset(tp->tt_2msl, tcp_maxidle,
						      tcp_timer_2msl, tp);
				}
				tp->t_state = TCPS_FIN_WAIT_2;
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
				KASSERT(headlocked, ("tcp_input: process_ACK: "
				    "head not locked"));
				tcp_twstart(tp);
				INP_INFO_WUNLOCK(&tcbinfo);
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
				tp = tcp_close(tp);
				goto drop;
			}
			break;

		/*
		 * In TIME_WAIT state the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TCPS_TIME_WAIT:
			KASSERT(tp->t_state != TCPS_TIME_WAIT, ("timewait"));
			callout_reset(tp->tt_2msl, 2 * tcp_msl,
				      tcp_timer_2msl, tp);
			goto dropafterack;
		}
	}

step6:
	KASSERT(headlocked, ("tcp_input: step6: head not locked"));
	INP_LOCK_ASSERT(inp);

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
			tcpstat.tcps_rcvwinupd++;
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
		if (th->th_urp + so->so_rcv.sb_cc > sb_max) {
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
			so->so_oobmark = so->so_rcv.sb_cc +
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
	KASSERT(headlocked, ("tcp_input: dodata: head not locked"));
	INP_LOCK_ASSERT(inp);

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
			if (DELAY_ACK(tp))
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt += tlen;
			thflags = th->th_flags & TH_FIN;
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += tlen;
			ND6_HINT(tp);
			SOCKBUF_LOCK(&so->so_rcv);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
				m_freem(m);
			else
				sbappendstream_locked(&so->so_rcv, m);
			sorwakeup_locked(so);
		} else {
			thflags = tcp_reass(tp, th, &tlen, m);
			tp->t_flags |= TF_ACKNOW;
		}
		if (tlen > 0 && tp->sack_enable)
			tcp_update_sack_list(tp, th->th_seq, th->th_seq + tlen);
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 */
		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
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
			/*FALLTHROUGH*/
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			KASSERT(headlocked == 1, ("tcp_input: dodata: "
			    "TCP_FIN_WAIT_2: head not locked"));
			tcp_twstart(tp);
			INP_INFO_WUNLOCK(&tcbinfo);
			return;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			KASSERT(tp->t_state != TCPS_TIME_WAIT, ("timewait"));
			callout_reset(tp->tt_2msl, 2 * tcp_msl,
				      tcp_timer_2msl, tp);
			break;
		}
	}
	INP_INFO_WUNLOCK(&tcbinfo);
	headlocked = 0;
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tcp_output(tp);

check_delack:
	KASSERT(headlocked == 0, ("tcp_input: check_delack: head locked"));
	INP_LOCK_ASSERT(inp);
	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		callout_reset(tp->tt_delack, tcp_delacktime,
		    tcp_timer_delack, tp);
	}
	INP_UNLOCK(inp);
	return;

dropafterack:
	KASSERT(headlocked, ("tcp_input: dropafterack: head not locked"));
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
	KASSERT(headlocked, ("headlocked should be 1"));
	INP_INFO_WUNLOCK(&tcbinfo);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	INP_UNLOCK(inp);
	m_freem(m);
	return;

dropwithreset:
	KASSERT(headlocked, ("tcp_input: dropwithreset: head not locked"));
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast/multicast.
	 */
	if ((thflags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST))
		goto drop;
	if (isipv6) {
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src))
			goto drop;
	} else {
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
		    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
		    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
		    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			goto drop;
	}
	/* IPv6 anycast check is done at tcp6_input() */

	/*
	 * Perform bandwidth limiting.
	 */
	if (badport_bandlim(rstreason) < 0)
		goto drop;

#ifdef TCPDEBUG
	if (tp == 0 || (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif

	if (thflags & TH_ACK)
		/* mtod() below is safe as long as hdr dropping is delayed */
		tcp_respond(tp, mtod(m, void *), th, m, (tcp_seq)0, th->th_ack,
			    TH_RST);
	else {
		if (thflags & TH_SYN)
			tlen++;
		/* mtod() below is safe as long as hdr dropping is delayed */
		tcp_respond(tp, mtod(m, void *), th, m, th->th_seq+tlen,
			    (tcp_seq)0, TH_RST|TH_ACK);
	}

	if (tp)
		INP_UNLOCK(inp);
	if (headlocked)
		INP_INFO_WUNLOCK(&tcbinfo);
	return;

drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
#ifdef TCPDEBUG
	if (tp == 0 || (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	if (tp)
		INP_UNLOCK(inp);
	if (headlocked)
		INP_INFO_WUNLOCK(&tcbinfo);
	m_freem(m);
	return;
}

/*
 * Parse TCP options and place in tcpopt.
 */
static void
tcp_dooptions(tp, to, cp, cnt, is_syn, th)
	struct tcpcb *tp;
	struct tcpopt *to;
	u_char *cp;
	int cnt;
	int is_syn;
	struct tcphdr *th;
{
	int opt, optlen;

	to->to_flags = 0;
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {
		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!is_syn)
				continue;
			to->to_flags |= TOF_MSS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_mss, sizeof(to->to_mss));
			to->to_mss = ntohs(to->to_mss);
			break;
		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (! is_syn)
				continue;
			to->to_flags |= TOF_SCALE;
			to->to_requested_s_scale = min(cp[2], TCP_MAX_WINSHIFT);
			break;
		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			to->to_flags |= TOF_TS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_tsval, sizeof(to->to_tsval));
			to->to_tsval = ntohl(to->to_tsval);
			bcopy((char *)cp + 6,
			    (char *)&to->to_tsecr, sizeof(to->to_tsecr));
			to->to_tsecr = ntohl(to->to_tsecr);
			/*
			 * If echoed timestamp is later than the current time,
			 * fall back to non RFC1323 RTT calculation.
			 */
			if ((to->to_tsecr != 0) && TSTMP_GT(to->to_tsecr, ticks))
				to->to_tsecr = 0;
			break;
		case TCPOPT_CC:
			if (optlen != TCPOLEN_CC)
				continue;
			to->to_flags |= TOF_CC;
			bcopy((char *)cp + 2,
			    (char *)&to->to_cc, sizeof(to->to_cc));
			to->to_cc = ntohl(to->to_cc);
			break;
		case TCPOPT_CCNEW:
			if (optlen != TCPOLEN_CC)
				continue;
			if (!is_syn)
				continue;
			to->to_flags |= TOF_CCNEW;
			bcopy((char *)cp + 2,
			    (char *)&to->to_cc, sizeof(to->to_cc));
			to->to_cc = ntohl(to->to_cc);
			break;
		case TCPOPT_CCECHO:
			if (optlen != TCPOLEN_CC)
				continue;
			if (!is_syn)
				continue;
			to->to_flags |= TOF_CCECHO;
			bcopy((char *)cp + 2,
			    (char *)&to->to_ccecho, sizeof(to->to_ccecho));
			to->to_ccecho = ntohl(to->to_ccecho);
			break;
#ifdef TCP_SIGNATURE
		/*
		 * XXX In order to reply to a host which has set the
		 * TCP_SIGNATURE option in its initial SYN, we have to
		 * record the fact that the option was observed here
		 * for the syncache code to perform the correct response.
		 */
		case TCPOPT_SIGNATURE:
			if (optlen != TCPOLEN_SIGNATURE)
				continue;
			to->to_flags |= (TOF_SIGNATURE | TOF_SIGLEN);
			break;
#endif
		case TCPOPT_SACK_PERMITTED:
			if (!tcp_do_sack ||
			    optlen != TCPOLEN_SACK_PERMITTED)
				continue;
			if (is_syn) {
				/* MUST only be set on SYN */
				to->to_flags |= TOF_SACK;
			}
			break;

		case TCPOPT_SACK:
			if (!tp || tcp_sack_option(tp, th, cp, optlen))
				continue;
			break;
		default:
			continue;
		}
	}
}

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
static void
tcp_pulloutofband(so, th, m, off)
	struct socket *so;
	struct tcphdr *th;
	register struct mbuf *m;
	int off;		/* delayed to be droped hdrlen */
{
	int cnt = off + th->th_urp - 1;

	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, caddr_t) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			if (m->m_flags & M_PKTHDR)
				m->m_pkthdr.len--;
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == 0)
			break;
	}
	panic("tcp_pulloutofband");
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
static void
tcp_xmit_timer(tp, rtt)
	register struct tcpcb *tp;
	int rtt;
{
	register int delta;

	INP_LOCK_ASSERT(tp->t_inpcb);

	tcpstat.tcps_rttupdated++;
	tp->t_rttupdated++;
	if (tp->t_srtt != 0) {
		/*
		 * srtt is stored as fixed point with 5 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		delta = ((rtt - 1) << TCP_DELTA_SHIFT)
			- (tp->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT));

		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1;

		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 4 bits after the
		 * binary point (scaled by 16).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= tp->t_rttvar >> (TCP_RTTVAR_SHIFT - TCP_DELTA_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1;
		if (tp->t_rttbest > tp->t_srtt + tp->t_rttvar)
		    tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
		tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
	}
	tp->t_rtttime = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
		      max(tp->t_rttmin, rtt + 2), TCPTV_REXMTMAX);

	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 *
 * Also take into account the space needed for options that we
 * send regularly.  Make maxseg shorter by that amount to assure
 * that we can send maxseg amount of data even when the options
 * are present.  Store the upper limit of the length of options plus
 * data in maxopd.
 *
 *
 * In case of T/TCP, we call this routine during implicit connection
 * setup as well (offer = -1), to initialize maxseg from the cached
 * MSS of our peer.
 *
 * NOTE that this routine is only called when we process an incoming
 * segment. Outgoing SYN/ACK MSS settings are handled in tcp_mssopt().
 */
void
tcp_mss(tp, offer)
	struct tcpcb *tp;
	int offer;
{
	int rtt, mss;
	u_long bufsize;
	u_long maxmtu;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so;
	struct hc_metrics_lite metrics;
	struct rmxp_tao tao;
	int origoffer = offer;
#ifdef INET6
	int isipv6 = ((inp->inp_vflag & INP_IPV6) != 0) ? 1 : 0;
	size_t min_protoh = isipv6 ?
			    sizeof (struct ip6_hdr) + sizeof (struct tcphdr) :
			    sizeof (struct tcpiphdr);
#else
	const size_t min_protoh = sizeof(struct tcpiphdr);
#endif
	bzero(&tao, sizeof(tao));

	/* initialize */
#ifdef INET6
	if (isipv6) {
		maxmtu = tcp_maxmtu6(&inp->inp_inc);
		tp->t_maxopd = tp->t_maxseg = tcp_v6mssdflt;
	} else
#endif
	{
		maxmtu = tcp_maxmtu(&inp->inp_inc);
		tp->t_maxopd = tp->t_maxseg = tcp_mssdflt;
	}
	so = inp->inp_socket;

	/*
	 * no route to sender, stay with default mss and return
	 */
	if (maxmtu == 0)
		return;

	/* what have we got? */
	switch (offer) {
		case 0:
			/*
			 * Offer == 0 means that there was no MSS on the SYN
			 * segment, in this case we use tcp_mssdflt.
			 */
			offer =
#ifdef INET6
				isipv6 ? tcp_v6mssdflt :
#endif
				tcp_mssdflt;
			break;

		case -1:
			/*
			 * Offer == -1 means that we didn't receive SYN yet,
			 * use cached value in that case;
			 */
			if (tcp_do_rfc1644)
				tcp_hc_gettao(&inp->inp_inc, &tao);
			if (tao.tao_mssopt != 0)
				offer = tao.tao_mssopt;
			/* FALLTHROUGH */

		default:
			/*
			 * Prevent DoS attack with too small MSS. Round up
			 * to at least minmss.
			 */
			offer = max(offer, tcp_minmss);
			/*
			 * Sanity check: make sure that maxopd will be large
			 * enough to allow some data on segments even if the
			 * all the option space is used (40bytes).  Otherwise
			 * funny things may happen in tcp_output.
			 */
			offer = max(offer, 64);
			if (tcp_do_rfc1644)
				tcp_hc_updatetao(&inp->inp_inc,
						 TCP_HC_TAO_MSSOPT, 0, offer);
	}

	/*
	 * rmx information is now retrieved from tcp_hostcache
	 */
	tcp_hc_get(&inp->inp_inc, &metrics);

	/*
	 * if there's a discovered mtu int tcp hostcache, use it
	 * else, use the link mtu.
	 */
	if (metrics.rmx_mtu)
		mss = min(metrics.rmx_mtu, maxmtu) - min_protoh;
	else {
#ifdef INET6
		if (isipv6) {
			mss = maxmtu - min_protoh;
			if (!path_mtu_discovery &&
			    !in6_localaddr(&inp->in6p_faddr))
				mss = min(mss, tcp_v6mssdflt);
		} else
#endif
		{
			mss = maxmtu - min_protoh;
			if (!path_mtu_discovery &&
			    !in_localaddr(inp->inp_faddr))
				mss = min(mss, tcp_mssdflt);
		}
	}
	mss = min(mss, offer);

	/*
	 * maxopd stores the maximum length of data AND options
	 * in a segment; maxseg is the amount of data in a normal
	 * segment.  We need to store this value (maxopd) apart
	 * from maxseg, because now every segment carries options
	 * and thus we normally have somewhat less data in segments.
	 */
	tp->t_maxopd = mss;

	/*
	 * In case of T/TCP, origoffer==-1 indicates, that no segments
	 * were received yet.  In this case we just guess, otherwise
	 * we do the same as before T/TCP.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (origoffer == -1 ||
	     (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP))
		mss -= TCPOLEN_TSTAMP_APPA;
	if ((tp->t_flags & (TF_REQ_CC|TF_NOOPT)) == TF_REQ_CC &&
	    (origoffer == -1 ||
	     (tp->t_flags & TF_RCVD_CC) == TF_RCVD_CC))
		mss -= TCPOLEN_CC_APPA;
	tp->t_maxseg = mss;

#if	(MCLBYTES & (MCLBYTES - 1)) == 0
		if (mss > MCLBYTES)
			mss &= ~(MCLBYTES-1);
#else
		if (mss > MCLBYTES)
			mss = mss / MCLBYTES * MCLBYTES;
#endif
	tp->t_maxseg = mss;

	/*
	 * If there's a pipesize, change the socket buffer to that size,
	 * don't change if sb_hiwat is different than default (then it
	 * has been changed on purpose with setsockopt).
	 * Make the socket buffers an integral number of mss units;
	 * if the mss is larger than the socket buffer, decrease the mss.
	 */
	SOCKBUF_LOCK(&so->so_snd);
	if ((so->so_snd.sb_hiwat == tcp_sendspace) && metrics.rmx_sendpipe)
		bufsize = metrics.rmx_sendpipe;
	else
		bufsize = so->so_snd.sb_hiwat;
	if (bufsize < mss)
		mss = bufsize;
	else {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		if (bufsize > so->so_snd.sb_hiwat)
			(void)sbreserve_locked(&so->so_snd, bufsize, so, NULL);
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	tp->t_maxseg = mss;

	SOCKBUF_LOCK(&so->so_rcv);
	if ((so->so_rcv.sb_hiwat == tcp_recvspace) && metrics.rmx_recvpipe)
		bufsize = metrics.rmx_recvpipe;
	else
		bufsize = so->so_rcv.sb_hiwat;
	if (bufsize > mss) {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		if (bufsize > so->so_rcv.sb_hiwat)
			(void)sbreserve_locked(&so->so_rcv, bufsize, so, NULL);
	}
	SOCKBUF_UNLOCK(&so->so_rcv);
	/*
	 * While we're here, check the others too
	 */
	if (tp->t_srtt == 0 && (rtt = metrics.rmx_rtt)) {
		tp->t_srtt = rtt;
		tp->t_rttbest = tp->t_srtt + TCP_RTT_SCALE;
		tcpstat.tcps_usedrtt++;
		if (metrics.rmx_rttvar) {
			tp->t_rttvar = metrics.rmx_rttvar;
			tcpstat.tcps_usedrttvar++;
		} else {
			/* default variation is +- 1 rtt */
			tp->t_rttvar =
			    tp->t_srtt * TCP_RTTVAR_SCALE / TCP_RTT_SCALE;
		}
		TCPT_RANGESET(tp->t_rxtcur,
			      ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1,
			      tp->t_rttmin, TCPTV_REXMTMAX);
	}
	if (metrics.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface
		 * buffer limit on the path.  Use this to set
		 * the slow start threshhold, but set the
		 * threshold to no less than 2*mss.
		 */
		tp->snd_ssthresh = max(2 * mss, metrics.rmx_ssthresh);
		tcpstat.tcps_usedssthresh++;
	}
	if (metrics.rmx_bandwidth)
		tp->snd_bandwidth = metrics.rmx_bandwidth;

	/*
	 * Set the slow-start flight size depending on whether this
	 * is a local network or not.
	 *
	 * Extend this so we cache the cwnd too and retrieve it here.
	 * Make cwnd even bigger than RFC3390 suggests but only if we
	 * have previous experience with the remote host. Be careful
	 * not make cwnd bigger than remote receive window or our own
	 * send socket buffer. Maybe put some additional upper bound
	 * on the retrieved cwnd. Should do incremental updates to
	 * hostcache when cwnd collapses so next connection doesn't
	 * overloads the path again.
	 *
	 * RFC3390 says only do this if SYN or SYN/ACK didn't got lost.
	 * We currently check only in syncache_socket for that.
	 */
#define TCP_METRICS_CWND
#ifdef TCP_METRICS_CWND
	if (metrics.rmx_cwnd)
		tp->snd_cwnd = max(mss,
				min(metrics.rmx_cwnd / 2,
				 min(tp->snd_wnd, so->so_snd.sb_hiwat)));
	else
#endif
	if (tcp_do_rfc3390)
		tp->snd_cwnd = min(4 * mss, max(2 * mss, 4380));
#ifdef INET6
	else if ((isipv6 && in6_localaddr(&inp->in6p_faddr)) ||
		 (!isipv6 && in_localaddr(inp->inp_faddr)))
#else
	else if (in_localaddr(inp->inp_faddr))
#endif
		tp->snd_cwnd = mss * ss_fltsz_local;
	else
		tp->snd_cwnd = mss * ss_fltsz;
}

/*
 * Determine the MSS option to send on an outgoing SYN.
 */
int
tcp_mssopt(inc)
	struct in_conninfo *inc;
{
	int mss = 0;
	u_long maxmtu = 0;
	u_long thcmtu = 0;
	size_t min_protoh;
#ifdef INET6
	int isipv6 = inc->inc_isipv6 ? 1 : 0;
#endif

	KASSERT(inc != NULL, ("tcp_mssopt with NULL in_conninfo pointer"));

#ifdef INET6
	if (isipv6) {
		mss = tcp_v6mssdflt;
		maxmtu = tcp_maxmtu6(inc);
		thcmtu = tcp_hc_getmtu(inc); /* IPv4 and IPv6 */
		min_protoh = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	} else
#endif
	{
		mss = tcp_mssdflt;
		maxmtu = tcp_maxmtu(inc);
		thcmtu = tcp_hc_getmtu(inc); /* IPv4 and IPv6 */
		min_protoh = sizeof(struct tcpiphdr);
	}
	if (maxmtu && thcmtu)
		mss = min(maxmtu, thcmtu) - min_protoh;
	else if (maxmtu || thcmtu)
		mss = max(maxmtu, thcmtu) - min_protoh;

	return (mss);
}


/*
 * On a partial ack arrives, force the retransmission of the
 * next unacknowledged segment.  Do not clear tp->t_dupacks.
 * By setting snd_nxt to ti_ack, this forces retransmission timer to
 * be started again.
 */
static void
tcp_newreno_partial_ack(tp, th)
	struct tcpcb *tp;
	struct tcphdr *th;
{
	tcp_seq onxt = tp->snd_nxt;
	u_long  ocwnd = tp->snd_cwnd;

	callout_stop(tp->tt_rexmt);
	tp->t_rtttime = 0;
	tp->snd_nxt = th->th_ack;
	/*
	 * Set snd_cwnd to one segment beyond acknowledged offset.
	 * (tp->snd_una has not yet been updated when this function is called.)
	 */
	tp->snd_cwnd = tp->t_maxseg + (th->th_ack - tp->snd_una);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	tp->snd_cwnd = ocwnd;
	if (SEQ_GT(onxt, tp->snd_nxt))
		tp->snd_nxt = onxt;
	/*
	 * Partial window deflation.  Relies on fact that tp->snd_una
	 * not updated yet.
	 */
	tp->snd_cwnd -= (th->th_ack - tp->snd_una - tp->t_maxseg);
}

/*
 * Returns 1 if the TIME_WAIT state was killed and we should start over,
 * looking for a pcb in the listen state.  Returns 0 otherwise.
 */
static int
tcp_timewait(tw, to, th, m, tlen)
	struct tcptw *tw;
	struct tcpopt *to;
	struct tcphdr *th;
	struct mbuf *m;
	int tlen;
{
	int thflags;
	tcp_seq seq;
#ifdef INET6
	int isipv6 = (mtod(m, struct ip *)->ip_v == 6) ? 1 : 0;
#else
	const int isipv6 = 0;
#endif

	/* tcbinfo lock required for tcp_twclose(), tcp_2msl_reset. */
	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(tw->tw_inpcb);

	thflags = th->th_flags;

	/*
	 * NOTE: for FIN_WAIT_2 (to be added later),
	 * must validate sequence number before accepting RST
	 */

	/*
	 * If the segment contains RST:
	 *	Drop the segment - see Stevens, vol. 2, p. 964 and
	 *      RFC 1337.
	 */
	if (thflags & TH_RST)
		goto drop;

	/*
	 * If segment contains a SYN and CC [not CC.NEW] option:
	 *	if connection duration > MSL, drop packet and send RST;
	 *
	 *	if SEG.CC > CCrecv then is new SYN.
	 *	    Complete close and delete TCPCB.  Then reprocess
	 *	    segment, hoping to find new TCPCB in LISTEN state;
	 *
	 *	else must be old SYN; drop it.
	 * else do normal processing.
	 */
	if ((thflags & TH_SYN) && (to->to_flags & TOF_CC) && tw->cc_recv != 0) {
		if ((ticks - tw->t_starttime) > tcp_msl)
			goto reset;
		if (CC_GT(to->to_cc, tw->cc_recv)) {
			(void) tcp_twclose(tw, 0);
			return (1);
		}
		goto drop;
	}

#if 0
/* PAWS not needed at the moment */
	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {
		if ((thflags & TH_ACK) == 0)
			goto drop;
		goto ack;
	}
	/*
	 * ts_recent is never updated because we never accept new segments.
	 */
#endif

	/*
	 * If a new connection request is received
	 * while in TIME_WAIT, drop the old connection
	 * and start over if the sequence numbers
	 * are above the previous ones.
	 */
	if ((thflags & TH_SYN) && SEQ_GT(th->th_seq, tw->rcv_nxt)) {
		(void) tcp_twclose(tw, 0);
		return (1);
	}

	/*
	 * Drop the the segment if it does not contain an ACK.
	 */
	if ((thflags & TH_ACK) == 0)
		goto drop;

	/*
	 * Reset the 2MSL timer if this is a duplicate FIN.
	 */
	if (thflags & TH_FIN) {
		seq = th->th_seq + tlen + (thflags & TH_SYN ? 1 : 0);
		if (seq + 1 == tw->rcv_nxt)
			tcp_timer_2msl_reset(tw, 2 * tcp_msl);
	}

	/*
	 * Acknowledge the segment if it has data or is not a duplicate ACK.
	 */
	if (thflags != TH_ACK || tlen != 0 ||
	    th->th_seq != tw->rcv_nxt || th->th_ack != tw->snd_nxt)
		tcp_twrespond(tw, TH_ACK);
	goto drop;

reset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast/multicast.
	 */
	if (m->m_flags & (M_BCAST|M_MCAST))
		goto drop;
	if (isipv6) {
		struct ip6_hdr *ip6;

		/* IPv6 anycast check is done at tcp6_input() */
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src))
			goto drop;
	} else {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
		    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
		    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
		    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			goto drop;
	}
	if (thflags & TH_ACK) {
		tcp_respond(NULL,
		    mtod(m, void *), th, m, 0, th->th_ack, TH_RST);
	} else {
		seq = th->th_seq + (thflags & TH_SYN ? 1 : 0);
		tcp_respond(NULL,
		    mtod(m, void *), th, m, seq, 0, TH_RST|TH_ACK);
	}
	INP_UNLOCK(tw->tw_inpcb);
	return (0);

drop:
	INP_UNLOCK(tw->tw_inpcb);
	m_freem(m);
	return (0);
}
