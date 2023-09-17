/*-
 * Copyright (c) 2016-2020 Netflix, Inc.
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
 */
/*
 * Author: Randall Stewart <rrs@netflix.com>
 * This work is based on the ACM Queue paper
 * BBR - Congestion Based Congestion Control
 * and also numerous discussions with Neal, Yuchung and Van.
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_ratelimit.h"
#include <sys/param.h>
#include <sys/arb.h>
#include <sys/module.h>
#include <sys/kernel.h>
#ifdef TCP_HHOOK
#include <sys/hhook.h>
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/qmath.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/tree.h>
#ifdef NETFLIX_STATS
#include <sys/stats.h> /* Must come after qmath.h and tree.h */
#endif
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/smp.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/tim_filter.h>
#include <sys/time.h>
#include <vm/uma.h>
#include <sys/kern_prefetch.h>

#include <net/route.h>
#include <net/vnet.h>
#include <net/ethernet.h>
#include <net/bpf.h>

#define TCPSTATES		/* for logging */

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* required for icmp_var.h */
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM */
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_ecn.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_lro.h>
#include <netinet/cc/cc.h>
#include <netinet/tcp_log_buf.h>
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif
#include <netinet/tcp_fastopen.h>

#include <netipsec/ipsec_support.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif				/* IPSEC */

#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <machine/in_cksum.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif
#include "rack_bbr_common.h"

/*
 * Common TCP Functions - These are shared by borth
 * rack and BBR.
 */
static int
ctf_get_enet_type(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;	/* Keep compiler happy. */
#endif
#ifdef INET
	struct ip *ip = NULL;		/* Keep compiler happy. */
#endif
#if defined(INET) || defined(INET6)
	struct tcphdr *th;
	int32_t tlen;
	uint16_t drop_hdrlen;
#endif
	uint16_t etype;
#ifdef INET
	uint8_t iptos;
#endif

	/* Is it the easy way? */
	if (m->m_flags & M_LRO_EHDRSTRP)
		return (m->m_pkthdr.lro_etype);
	/*
	 * Ok this is the old style call, the ethernet header is here.
	 * This also means no checksum or BPF were done. This
	 * can happen if the race to setup the inp fails and
	 * LRO sees no INP at packet input, but by the time
	 * we queue the packets an INP gets there. Its rare
	 * but it can occur so we will handle it. Note that
	 * this means duplicated work but with the rarity of it
	 * its not worth worrying about.
	 */
	/* Let the BPF see the packet */
	if (bpf_peers_present(ifp->if_bpf))
		ETHER_BPF_MTAP(ifp, m);
	/* Now the csum */
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	m_adj(m,  sizeof(*eh));
	switch (etype) {
#ifdef INET6
		case ETHERTYPE_IPV6:
		{
			if (m->m_len < (sizeof(*ip6) + sizeof(*th))) {
				m = m_pullup(m, sizeof(*ip6) + sizeof(*th));
				if (m == NULL) {
					KMOD_TCPSTAT_INC(tcps_rcvshort);
					return (-1);
				}
			}
			ip6 = (struct ip6_hdr *)(eh + 1);
			th = (struct tcphdr *)(ip6 + 1);
			drop_hdrlen = sizeof(*ip6);
			tlen = ntohs(ip6->ip6_plen);
			if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID_IPV6) {
				if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
					th->th_sum = m->m_pkthdr.csum_data;
				else
					th->th_sum = in6_cksum_pseudo(ip6, tlen,
								      IPPROTO_TCP,
								      m->m_pkthdr.csum_data);
				th->th_sum ^= 0xffff;
			} else
				th->th_sum = in6_cksum(m, IPPROTO_TCP, drop_hdrlen, tlen);
			if (th->th_sum) {
				KMOD_TCPSTAT_INC(tcps_rcvbadsum);
				m_freem(m);
				return (-1);
			}
			return (etype);
		}
#endif
#ifdef INET
		case ETHERTYPE_IP:
		{
			if (m->m_len < sizeof (struct tcpiphdr)) {
				m = m_pullup(m, sizeof (struct tcpiphdr));
				if (m == NULL) {
					KMOD_TCPSTAT_INC(tcps_rcvshort);
					return (-1);
				}
			}
			ip = (struct ip *)(eh + 1);
			th = (struct tcphdr *)(ip + 1);
			drop_hdrlen = sizeof(*ip);
			iptos = ip->ip_tos;
			tlen = ntohs(ip->ip_len) - sizeof(struct ip);
			if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
				if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
					th->th_sum = m->m_pkthdr.csum_data;
				else
					th->th_sum = in_pseudo(ip->ip_src.s_addr,
							       ip->ip_dst.s_addr,
							       htonl(m->m_pkthdr.csum_data + tlen + IPPROTO_TCP));
				th->th_sum ^= 0xffff;
			} else {
				int len;
				struct ipovly *ipov = (struct ipovly *)ip;
				/*
				 * Checksum extended TCP header and data.
				 */
				len = drop_hdrlen + tlen;
				bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
				ipov->ih_len = htons(tlen);
				th->th_sum = in_cksum(m, len);
				/* Reset length for SDT probes. */
				ip->ip_len = htons(len);
				/* Reset TOS bits */
				ip->ip_tos = iptos;
				/* Re-initialization for later version check */
				ip->ip_v = IPVERSION;
				ip->ip_hl = sizeof(*ip) >> 2;
			}
			if (th->th_sum) {
				KMOD_TCPSTAT_INC(tcps_rcvbadsum);
				m_freem(m);
				return (-1);
			}
			break;
		}
#endif
	};
	return (etype);
}

/*
 * The function ctf_process_inbound_raw() is used by
 * transport developers to do the steps needed to
 * support MBUF Queuing i.e. the flags in
 * inp->inp_flags2:
 *
 * - INP_SUPPORTS_MBUFQ
 * - INP_MBUF_QUEUE_READY
 * - INP_DONT_SACK_QUEUE
 * - INP_MBUF_ACKCMP
 *
 * These flags help control how LRO will deliver
 * packets to the transport. You first set in inp_flags2
 * the INP_SUPPORTS_MBUFQ to tell the LRO code that you
 * will gladly take a queue of packets instead of a compressed
 * single packet. You also set in your t_fb pointer the
 * tfb_do_queued_segments to point to ctf_process_inbound_raw.
 *
 * This then gets you lists of inbound ACK's/Data instead
 * of a condensed compressed ACK/DATA packet. Why would you
 * want that? This will get you access to all the arrival
 * times of at least LRO and possibly at the Hardware (if
 * the interface card supports that) of the actual ACK/DATA.
 * In some transport designs this is important since knowing
 * the actual time we got the packet is useful information.
 *
 * A new special type of mbuf may also be supported by the transport
 * if it has set the INP_MBUF_ACKCMP flag. If its set, LRO will
 * possibly create a M_ACKCMP type mbuf. This is a mbuf with
 * an array of "acks". One thing also to note is that when this
 * occurs a subsequent LRO may find at the back of the untouched
 * mbuf queue chain a M_ACKCMP and append on to it. This means
 * that until the transport pulls in the mbuf chain queued
 * for it more ack's may get on the mbufs that were already
 * delivered. There currently is a limit of 6 acks condensed
 * into 1 mbuf which means often when this is occuring, we
 * don't get that effect but it does happen.
 *
 * Now there are some interesting Caveats that the transport
 * designer needs to take into account when using this feature.
 *
 * 1) It is used with HPTS and pacing, when the pacing timer
 *    for output calls it will first call the input.
 * 2) When you set INP_MBUF_QUEUE_READY this tells LRO
 *    queue normal packets, I am busy pacing out data and
 *    will process the queued packets before my tfb_tcp_output
 *    call from pacing. If a non-normal packet arrives, (e.g. sack)
 *    you will be awoken immediately.
 * 3) Finally you can add the INP_DONT_SACK_QUEUE to not even
 *    be awoken if a SACK has arrived. You would do this when
 *    you were not only running a pacing for output timer
 *    but a Rack timer as well i.e. you know you are in recovery
 *    and are in the process (via the timers) of dealing with
 *    the loss.
 *
 * Now a critical thing you must be aware of here is that the
 * use of the flags has a far greater scope then just your
 * typical LRO. Why? Well thats because in the normal compressed
 * LRO case at the end of a driver interupt all packets are going
 * to get presented to the transport no matter if there is one
 * or 100. With the MBUF_QUEUE model, this is not true. You will
 * only be awoken to process the queue of packets when:
 *     a) The flags discussed above allow it.
 *          <or>
 *     b) You exceed a ack or data limit (by default the
 *        ack limit is infinity (64k acks) and the data
 *        limit is 64k of new TCP data)
 *         <or>
 *     c) The push bit has been set by the peer
 */

static int
ctf_process_inbound_raw(struct tcpcb *tp, struct mbuf *m, int has_pkt)
{
	/*
	 * We are passed a raw change of mbuf packets
	 * that arrived in LRO. They are linked via
	 * the m_nextpkt link in the pkt-headers.
	 *
	 * We process each one by:
	 * a) saving off the next
	 * b) stripping off the ether-header
	 * c) formulating the arguments for tfb_do_segment_nounlock()
	 * d) calling each mbuf to tfb_do_segment_nounlock()
	 *    after adjusting the time to match the arrival time.
	 * Note that the LRO code assures no IP options are present.
	 *
	 * The symantics for calling tfb_do_segment_nounlock() are the
	 * following:
	 * 1) It returns 0 if all went well and you (the caller) need
	 *    to release the lock.
	 * 2) If nxt_pkt is set, then the function will surpress calls
	 *    to tcp_output() since you are promising to call again
	 *    with another packet.
	 * 3) If it returns 1, then you must free all the packets being
	 *    shipped in, the tcb has been destroyed (or about to be destroyed).
	 */
	struct mbuf *m_save;
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;	/* Keep compiler happy. */
#endif
#ifdef INET
	struct ip *ip = NULL;		/* Keep compiler happy. */
#endif
	struct ifnet *ifp;
	struct timeval tv;
	struct inpcb *inp __diagused;
	int32_t retval, nxt_pkt, tlen, off;
	int etype = 0;
	uint16_t drop_hdrlen;
	uint8_t iptos, no_vn=0;

	inp = tptoinpcb(tp);
	INP_WLOCK_ASSERT(inp);
	NET_EPOCH_ASSERT();

	if (m)
		ifp = m_rcvif(m);
	else
		ifp = NULL;
	if (ifp == NULL) {
		/*
		 * We probably should not work around
		 * but kassert, since lro alwasy sets rcvif.
		 */
		no_vn = 1;
		goto skip_vnet;
	}
	CURVNET_SET(ifp->if_vnet);
skip_vnet:
	tcp_get_usecs(&tv);
	while (m) {
		m_save = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if ((m->m_flags & M_ACKCMP) == 0) {
			/* Now lets get the ether header */
			etype = ctf_get_enet_type(ifp, m);
			if (etype == -1) {
				/* Skip this packet it was freed by checksum */
				goto skipped_pkt;
			}
			KASSERT(((etype == ETHERTYPE_IPV6) || (etype == ETHERTYPE_IP)),
				("tp:%p m:%p etype:0x%x -- not IP or IPv6", tp, m, etype));
			/* Trim off the ethernet header */
			switch (etype) {
#ifdef INET6
			case ETHERTYPE_IPV6:
				ip6 = mtod(m, struct ip6_hdr *);
				th = (struct tcphdr *)(ip6 + 1);
				tlen = ntohs(ip6->ip6_plen);
				drop_hdrlen = sizeof(*ip6);
				iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
				break;
#endif
#ifdef INET
			case ETHERTYPE_IP:
				ip = mtod(m, struct ip *);
				th = (struct tcphdr *)(ip + 1);
				drop_hdrlen = sizeof(*ip);
				iptos = ip->ip_tos;
				tlen = ntohs(ip->ip_len) - sizeof(struct ip);
				break;
#endif
			} /* end switch */
			/*
			 * Convert TCP protocol specific fields to host format.
			 */
			tcp_fields_to_host(th);
			off = th->th_off << 2;
			if (off < sizeof (struct tcphdr) || off > tlen) {
				printf("off:%d < hdrlen:%zu || > tlen:%u -- dump\n",
				       off,
				       sizeof(struct tcphdr),
				       tlen);
				KMOD_TCPSTAT_INC(tcps_rcvbadoff);
				m_freem(m);
				goto skipped_pkt;
			}
			tlen -= off;
			drop_hdrlen += off;
			/*
			 * Now lets setup the timeval to be when we should
			 * have been called (if we can).
			 */
			m->m_pkthdr.lro_nsegs = 1;
			/* Now what about next packet? */
		} else {
			/*
			 * This mbuf is an array of acks that have
			 * been compressed. We assert the inp has
			 * the flag set to enable this!
			 */
			KASSERT((tp->t_flags2 & TF2_MBUF_ACKCMP),
			    ("tp:%p no TF2_MBUF_ACKCMP flags?", tp));
			tlen = 0;
			drop_hdrlen = 0;
			th = NULL;
			iptos = 0;
		}
		tcp_get_usecs(&tv);
		if (m_save || has_pkt)
			nxt_pkt = 1;
		else
			nxt_pkt = 0;
		if ((m->m_flags & M_ACKCMP) == 0)
			KMOD_TCPSTAT_INC(tcps_rcvtotal);
		else
			KMOD_TCPSTAT_ADD(tcps_rcvtotal, (m->m_len / sizeof(struct tcp_ackent)));
		retval = (*tp->t_fb->tfb_do_segment_nounlock)(tp, m, th,
		    drop_hdrlen, tlen, iptos, nxt_pkt, &tv);
		if (retval) {
			/* We lost the lock and tcb probably */
			m = m_save;
			while(m) {
				m_save = m->m_nextpkt;
				m->m_nextpkt = NULL;
				m_freem(m);
				m = m_save;
			}
			if (no_vn == 0) {
				CURVNET_RESTORE();
			}
			INP_UNLOCK_ASSERT(inp);
			return(retval);
		}
skipped_pkt:
		m = m_save;
	}
	if (no_vn == 0) {
		CURVNET_RESTORE();
	}
	return(retval);
}

int
ctf_do_queued_segments(struct tcpcb *tp, int have_pkt)
{
	struct mbuf *m;

	/* First lets see if we have old packets */
	if ((m = STAILQ_FIRST(&tp->t_inqueue)) != NULL) {
		STAILQ_INIT(&tp->t_inqueue);
		if (ctf_process_inbound_raw(tp, m, have_pkt)) {
			/* We lost the tcpcb (maybe a RST came in)? */
			return(1);
		}
	}
	return (0);
}

uint32_t
ctf_outstanding(struct tcpcb *tp)
{
	uint32_t bytes_out;

	bytes_out = tp->snd_max - tp->snd_una;
	if (tp->t_state < TCPS_ESTABLISHED)
		bytes_out++;
	if (tp->t_flags & TF_SENTFIN)
		bytes_out++;
	return (bytes_out);
}

uint32_t
ctf_flight_size(struct tcpcb *tp, uint32_t rc_sacked)
{
	if (rc_sacked <= ctf_outstanding(tp))
		return(ctf_outstanding(tp) - rc_sacked);
	else {
		return (0);
	}
}

void
ctf_do_dropwithreset(struct mbuf *m, struct tcpcb *tp, struct tcphdr *th,
    int32_t rstreason, int32_t tlen)
{
	if (tp != NULL) {
		tcp_dropwithreset(m, th, tp, tlen, rstreason);
		INP_WUNLOCK(tptoinpcb(tp));
	} else
		tcp_dropwithreset(m, th, NULL, tlen, rstreason);
}

void
ctf_ack_war_checks(struct tcpcb *tp, uint32_t *ts, uint32_t *cnt)
{
	if ((ts != NULL) && (cnt != NULL) &&
	    (tcp_ack_war_time_window > 0) &&
	    (tcp_ack_war_cnt > 0)) {
		/* We are possibly doing ack war prevention */
		uint32_t cts;

		/*
		 * We use a msec tick here which gives us
		 * roughly 49 days. We don't need the
		 * precision of a microsecond timestamp which
		 * would only give us hours.
		 */
		cts = tcp_ts_getticks();
		if (TSTMP_LT((*ts), cts)) {
			/* Timestamp is in the past */
			*cnt = 0;
			*ts = (cts + tcp_ack_war_time_window);
		}
		if (*cnt < tcp_ack_war_cnt) {
			*cnt = (*cnt + 1);
			tp->t_flags |= TF_ACKNOW;
		} else
			tp->t_flags &= ~TF_ACKNOW;
	} else
		tp->t_flags |= TF_ACKNOW;
}

/*
 * ctf_drop_checks returns 1 for you should not proceed. It places
 * in ret_val what should be returned 1/0 by the caller. The 1 indicates
 * that the TCB is unlocked and probably dropped. The 0 indicates the
 * TCB is still valid and locked.
 */
int
_ctf_drop_checks(struct tcpopt *to, struct mbuf *m, struct tcphdr *th,
		 struct tcpcb *tp, int32_t *tlenp,
		 int32_t *thf, int32_t *drop_hdrlen, int32_t *ret_val,
		 uint32_t *ts, uint32_t *cnt)
{
	int32_t todrop;
	int32_t thflags;
	int32_t tlen;

	thflags = *thf;
	tlen = *tlenp;
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
			ctf_ack_war_checks(tp, ts, cnt);
			todrop = tlen;
			KMOD_TCPSTAT_INC(tcps_rcvduppack);
			KMOD_TCPSTAT_ADD(tcps_rcvdupbyte, todrop);
		} else {
			KMOD_TCPSTAT_INC(tcps_rcvpartduppack);
			KMOD_TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		/*
		 * DSACK - add SACK block for dropped range
		 */
		if ((todrop > 0) && (tp->t_flags & TF_SACK_PERMIT)) {
			/*
			 * ACK now, as the next in-sequence segment
			 * will clear the DSACK block again
			 */
			ctf_ack_war_checks(tp, ts, cnt);
			if (tp->t_flags & TF_ACKNOW)
				tcp_update_sack_list(tp, th->th_seq,
						     th->th_seq + todrop);
		}
		*drop_hdrlen += todrop;	/* drop from the top afterwards */
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
	 * If segment ends after window, drop trailing data (and PUSH and
	 * FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
	if (todrop > 0) {
		KMOD_TCPSTAT_INC(tcps_rcvpackafterwin);
		if (todrop >= tlen) {
			KMOD_TCPSTAT_ADD(tcps_rcvbyteafterwin, tlen);
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment and
			 * ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				ctf_ack_war_checks(tp, ts, cnt);
				KMOD_TCPSTAT_INC(tcps_rcvwinprobe);
			} else {
				__ctf_do_dropafterack(m, tp, th, thflags, tlen, ret_val, ts, cnt);
				return (1);
			}
		} else
			KMOD_TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		m_adj(m, -todrop);
		tlen -= todrop;
		thflags &= ~(TH_PUSH | TH_FIN);
	}
	*thf = thflags;
	*tlenp = tlen;
	return (0);
}

/*
 * The value in ret_val informs the caller
 * if we dropped the tcb (and lock) or not.
 * 1 = we dropped it, 0 = the TCB is still locked
 * and valid.
 */
void
__ctf_do_dropafterack(struct mbuf *m, struct tcpcb *tp, struct tcphdr *th, int32_t thflags, int32_t tlen, int32_t *ret_val, uint32_t *ts, uint32_t *cnt)
{
	/*
	 * Generate an ACK dropping incoming segment if it occupies sequence
	 * space, where the ACK reflects our state.
	 *
	 * We can now skip the test for the RST flag since all paths to this
	 * code happen after packets containing RST have been dropped.
	 *
	 * In the SYN-RECEIVED state, don't send an ACK unless the segment
	 * we received passes the SYN-RECEIVED ACK test. If it fails send a
	 * RST.  This breaks the loop in the "LAND" DoS attack, and also
	 * prevents an ACK storm between two listening ports that have been
	 * sent forged SYN segments, each with the source address of the
	 * other.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && (thflags & TH_ACK) &&
	    (SEQ_GT(tp->snd_una, th->th_ack) ||
	    SEQ_GT(th->th_ack, tp->snd_max))) {
		*ret_val = 1;
		ctf_do_dropwithreset(m, tp, th, BANDLIM_RST_OPENPORT, tlen);
		return;
	} else
		*ret_val = 0;
	ctf_ack_war_checks(tp, ts, cnt);
	if (m)
		m_freem(m);
}

void
ctf_do_drop(struct mbuf *m, struct tcpcb *tp)
{

	/*
	 * Drop space held by incoming segment and return.
	 */
	if (tp != NULL)
		INP_WUNLOCK(tptoinpcb(tp));
	if (m)
		m_freem(m);
}

int
__ctf_process_rst(struct mbuf *m, struct tcphdr *th, struct socket *so,
		struct tcpcb *tp, uint32_t *ts, uint32_t *cnt)
{
	/*
	 * RFC5961 Section 3.2
	 *
	 * - RST drops connection only if SEG.SEQ == RCV.NXT. - If RST is in
	 * window, we send challenge ACK.
	 *
	 * Note: to take into account delayed ACKs, we should test against
	 * last_ack_sent instead of rcv_nxt. Note 2: we handle special case
	 * of closed window, not covered by the RFC.
	 */
	int dropped = 0;

	if ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
	    (tp->rcv_wnd == 0 && tp->last_ack_sent == th->th_seq)) {
		KASSERT(tp->t_state != TCPS_SYN_SENT,
		    ("%s: TH_RST for TCPS_SYN_SENT th %p tp %p",
		    __func__, th, tp));

		if (V_tcp_insecure_rst ||
		    (tp->last_ack_sent == th->th_seq) ||
		    (tp->rcv_nxt == th->th_seq)) {
			KMOD_TCPSTAT_INC(tcps_drops);
			/* Drop the connection. */
			switch (tp->t_state) {
			case TCPS_SYN_RECEIVED:
				so->so_error = ECONNREFUSED;
				goto close;
			case TCPS_ESTABLISHED:
			case TCPS_FIN_WAIT_1:
			case TCPS_FIN_WAIT_2:
			case TCPS_CLOSE_WAIT:
			case TCPS_CLOSING:
			case TCPS_LAST_ACK:
				so->so_error = ECONNRESET;
		close:
				tcp_state_change(tp, TCPS_CLOSED);
				/* FALLTHROUGH */
			default:
				tcp_log_end_status(tp, TCP_EI_STATUS_CLIENT_RST);
				tp = tcp_close(tp);
			}
			dropped = 1;
			ctf_do_drop(m, tp);
		} else {
			int send_challenge;

			KMOD_TCPSTAT_INC(tcps_badrst);
			if ((ts != NULL) && (cnt != NULL) &&
			    (tcp_ack_war_time_window > 0) &&
			    (tcp_ack_war_cnt > 0)) {
				/* We are possibly preventing an  ack-rst  war prevention */
				uint32_t cts;

				/*
				 * We use a msec tick here which gives us
				 * roughly 49 days. We don't need the
				 * precision of a microsecond timestamp which
				 * would only give us hours.
				 */
				cts = tcp_ts_getticks();
				if (TSTMP_LT((*ts), cts)) {
					/* Timestamp is in the past */
					*cnt = 0;
					*ts = (cts + tcp_ack_war_time_window);
				}
				if (*cnt < tcp_ack_war_cnt) {
					*cnt = (*cnt + 1);
					send_challenge = 1;
				} else
					send_challenge = 0;
			} else
				send_challenge = 1;
			if (send_challenge) {
				/* Send challenge ACK. */
				tcp_respond(tp, mtod(m, void *), th, m,
					    tp->rcv_nxt, tp->snd_nxt, TH_ACK);
				tp->last_ack_sent = tp->rcv_nxt;
			}
		}
	} else {
		m_freem(m);
	}
	return (dropped);
}

/*
 * The value in ret_val informs the caller
 * if we dropped the tcb (and lock) or not.
 * 1 = we dropped it, 0 = the TCB is still locked
 * and valid.
 */
void
ctf_challenge_ack(struct mbuf *m, struct tcphdr *th, struct tcpcb *tp, uint8_t iptos, int32_t * ret_val)
{

	NET_EPOCH_ASSERT();

	KMOD_TCPSTAT_INC(tcps_badsyn);
	if (V_tcp_insecure_syn &&
	    SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) {
		tp = tcp_drop(tp, ECONNRESET);
		*ret_val = 1;
		ctf_do_drop(m, tp);
	} else {
		tcp_ecn_input_syn_sent(tp, tcp_get_flags(th), iptos);
		/* Send challenge ACK. */
		tcp_respond(tp, mtod(m, void *), th, m, tp->rcv_nxt,
		    tp->snd_nxt, TH_ACK);
		tp->last_ack_sent = tp->rcv_nxt;
		m = NULL;
		*ret_val = 0;
		ctf_do_drop(m, NULL);
	}
}

/*
 * ctf_ts_check returns 1 for you should not proceed, the state
 * machine should return. It places in ret_val what should
 * be returned 1/0 by the caller (hpts_do_segment). The 1 indicates
 * that the TCB is unlocked and probably dropped. The 0 indicates the
 * TCB is still valid and locked.
 */
int
ctf_ts_check(struct mbuf *m, struct tcphdr *th, struct tcpcb *tp,
    int32_t tlen, int32_t thflags, int32_t * ret_val)
{

	if (tcp_ts_getticks() - tp->ts_recent_age > TCP_PAWS_IDLE) {
		/*
		 * Invalidate ts_recent.  If this segment updates ts_recent,
		 * the age will be reset later and ts_recent will get a
		 * valid value.  If it does not, setting ts_recent to zero
		 * will at least satisfy the requirement that zero be placed
		 * in the timestamp echo reply when ts_recent isn't valid.
		 * The age isn't reset until we get a valid ts_recent
		 * because we don't want out-of-order segments to be dropped
		 * when ts_recent is old.
		 */
		tp->ts_recent = 0;
	} else {
		KMOD_TCPSTAT_INC(tcps_rcvduppack);
		KMOD_TCPSTAT_ADD(tcps_rcvdupbyte, tlen);
		KMOD_TCPSTAT_INC(tcps_pawsdrop);
		*ret_val = 0;
		if (tlen) {
			ctf_do_dropafterack(m, tp, th, thflags, tlen, ret_val);
		} else {
			ctf_do_drop(m, NULL);
		}
		return (1);
	}
	return (0);
}

int
ctf_ts_check_ac(struct tcpcb *tp, int32_t thflags)
{

	if (tcp_ts_getticks() - tp->ts_recent_age > TCP_PAWS_IDLE) {
		/*
		 * Invalidate ts_recent.  If this segment updates ts_recent,
		 * the age will be reset later and ts_recent will get a
		 * valid value.  If it does not, setting ts_recent to zero
		 * will at least satisfy the requirement that zero be placed
		 * in the timestamp echo reply when ts_recent isn't valid.
		 * The age isn't reset until we get a valid ts_recent
		 * because we don't want out-of-order segments to be dropped
		 * when ts_recent is old.
		 */
		tp->ts_recent = 0;
	} else {
		KMOD_TCPSTAT_INC(tcps_rcvduppack);
		KMOD_TCPSTAT_INC(tcps_pawsdrop);
		return (1);
	}
	return (0);
}



void
ctf_calc_rwin(struct socket *so, struct tcpcb *tp)
{
	int32_t win;

	/*
	 * Calculate amount of space in receive window, and then do TCP
	 * input processing. Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));
}

void
ctf_do_dropwithreset_conn(struct mbuf *m, struct tcpcb *tp, struct tcphdr *th,
    int32_t rstreason, int32_t tlen)
{

	tcp_dropwithreset(m, th, tp, tlen, rstreason);
	tp = tcp_drop(tp, ETIMEDOUT);
	if (tp)
		INP_WUNLOCK(tptoinpcb(tp));
}

uint32_t
ctf_fixed_maxseg(struct tcpcb *tp)
{
	return (tcp_fixed_maxseg(tp));
}

void
ctf_log_sack_filter(struct tcpcb *tp, int num_sack_blks, struct sackblk *sack_blocks)
{
	if (tcp_bblogging_on(tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log, 0, sizeof(log));
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		log.u_bbr.flex8 = num_sack_blks;
		if (num_sack_blks > 0) {
			log.u_bbr.flex1 = sack_blocks[0].start;
			log.u_bbr.flex2 = sack_blocks[0].end;
		}
		if (num_sack_blks > 1) {
			log.u_bbr.flex3 = sack_blocks[1].start;
			log.u_bbr.flex4 = sack_blocks[1].end;
		}
		if (num_sack_blks > 2) {
			log.u_bbr.flex5 = sack_blocks[2].start;
			log.u_bbr.flex6 = sack_blocks[2].end;
		}
		if (num_sack_blks > 3) {
			log.u_bbr.applimited = sack_blocks[3].start;
			log.u_bbr.pkts_out = sack_blocks[3].end;
		}
		TCP_LOG_EVENTP(tp, NULL,
		    &tptosocket(tp)->so_rcv,
		    &tptosocket(tp)->so_snd,
		    TCP_SACK_FILTER_RES, 0,
		    0, &log, false, &tv);
	}
}

uint32_t
ctf_decay_count(uint32_t count, uint32_t decay)
{
	/*
	 * Given a count, decay it by a set percentage. The
	 * percentage is in thousands i.e. 100% = 1000,
	 * 19.3% = 193.
	 */
	uint64_t perc_count, decay_per;
	uint32_t decayed_count;
	if (decay > 1000) {
		/* We don't raise it */
		return (count);
	}
	perc_count = count;
	decay_per = decay;
	perc_count *= decay_per;
	perc_count /= 1000;
	/*
	 * So now perc_count holds the
	 * count decay value.
	 */
	decayed_count = count - (uint32_t)perc_count;
	return(decayed_count);
}

int32_t
ctf_progress_timeout_check(struct tcpcb *tp, bool log)
{
	if (tp->t_maxunacktime && tp->t_acktime && TSTMP_GT(ticks, tp->t_acktime)) {
		if ((ticks - tp->t_acktime) >= tp->t_maxunacktime) {
			/*
			 * There is an assumption that the caller
			 * will drop the connection so we will
			 * increment the counters here.
			 */
			if (log)
				tcp_log_end_status(tp, TCP_EI_STATUS_PROGRESS);
#ifdef NETFLIX_STATS
			KMOD_TCPSTAT_INC(tcps_progdrops);
#endif
			return (1);
		}
	}
	return (0);
}
