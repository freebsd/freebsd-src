/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 * $Id: ip_input.c,v 1.52 1996/12/11 03:26:36 davidg Exp $
 *	$ANA: ip_input.c,v 1.5 1996/09/18 14:34:59 wollman Exp $
 */

#define	_IP_VHL

#include "opt_ipfw.h"

#include <stddef.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <machine/in_cksum.h>

#include <sys/socketvar.h>

#ifdef IPFIREWALL
#include <netinet/ip_fw.h>
#endif

int rsvp_on = 0;
static int ip_rsvp_on;
struct socket *ip_rsvpd;

static int	ipforwarding = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_FORWARDING, forwarding, CTLFLAG_RW,
	&ipforwarding, 0, "");

static int	ipsendredirects = 1; /* XXX */
SYSCTL_INT(_net_inet_ip, IPCTL_SENDREDIRECTS, redirect, CTLFLAG_RW,
	&ipsendredirects, 0, "");

int	ip_defttl = IPDEFTTL;
SYSCTL_INT(_net_inet_ip, IPCTL_DEFTTL, ttl, CTLFLAG_RW,
	&ip_defttl, 0, "");

static int	ip_dosourceroute = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_SOURCEROUTE, sourceroute, CTLFLAG_RW,
	&ip_dosourceroute, 0, "");
#ifdef DIAGNOSTIC
static int	ipprintfs = 0;
#endif

extern	struct domain inetdomain;
extern	struct protosw inetsw[];
u_char	ip_protox[IPPROTO_MAX];
static int	ipqmaxlen = IFQ_MAXLEN;
struct	in_ifaddrhead in_ifaddrhead; /* first inet address */
struct	ifqueue ipintrq;
SYSCTL_INT(_net_inet_ip, IPCTL_INTRQMAXLEN, intr_queue_maxlen, CTLFLAG_RD,
	&ipintrq.ifq_maxlen, 0, "");
SYSCTL_INT(_net_inet_ip, IPCTL_INTRQDROPS, intr_queue_drops, CTLFLAG_RD,
	&ipintrq.ifq_drops, 0, "");

struct ipstat ipstat;
static struct ipq ipq;

#ifdef IPCTL_DEFMTU
SYSCTL_INT(_net_inet_ip, IPCTL_DEFMTU, mtu, CTLFLAG_RW,
	&ip_mtu, 0, "");
#endif

#if !defined(COMPAT_IPFW) || COMPAT_IPFW == 1
#undef COMPAT_IPFW
#define COMPAT_IPFW 1
#else
#undef COMPAT_IPFW
#endif

#ifdef COMPAT_IPFW
/* Firewall hooks */
ip_fw_chk_t *ip_fw_chk_ptr;
ip_fw_ctl_t *ip_fw_ctl_ptr;

/* IP Network Address Translation (NAT) hooks */ 
ip_nat_t *ip_nat_ptr;
ip_nat_ctl_t *ip_nat_ctl_ptr;
#endif

/*
 * We need to save the IP options in case a protocol wants to respond
 * to an incoming packet over the same route if the packet got here
 * using IP source routing.  This allows connection establishment and
 * maintenance when the remote end is on a network that is not known
 * to us.
 */
static int	ip_nhops = 0;
static	struct ip_srcrt {
	struct	in_addr dst;			/* final destination */
	char	nop;				/* one NOP to align */
	char	srcopt[IPOPT_OFFSET + 1];	/* OPTVAL, OLEN and OFFSET */
	struct	in_addr route[MAX_IPOPTLEN/sizeof(struct in_addr)];
} ip_srcrt;

#ifdef IPDIVERT
/*
 * Shared variable between ip_input() and ip_reass() to communicate
 * about which packets, once assembled from fragments, get diverted,
 * and to which port.
 */
static u_short	frag_divert_port;
#endif

static void save_rte __P((u_char *, struct in_addr));
static void	 ip_deq __P((struct ipasfrag *));
static int	 ip_dooptions __P((struct mbuf *));
static void	 ip_enq __P((struct ipasfrag *, struct ipasfrag *));
static void	 ip_forward __P((struct mbuf *, int));
static void	 ip_freef __P((struct ipq *));
static struct ip *
	 ip_reass __P((struct ipasfrag *, struct ipq *));
static struct in_ifaddr *
	 ip_rtaddr __P((struct in_addr));
static void	ipintr __P((void));
/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init()
{
	register struct protosw *pr;
	register int i;

	TAILQ_INIT(&in_ifaddrhead);
	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = pr - inetsw;
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip_protox[pr->pr_protocol] = pr - inetsw;
	ipq.next = ipq.prev = &ipq;
	ip_id = time.tv_sec & 0xffff;
	ipintrq.ifq_maxlen = ipqmaxlen;
#ifdef IPFIREWALL
	ip_fw_init();
#endif
#ifdef IPNAT
        ip_nat_init(); 
#endif

}

static struct	sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };
static struct	route ipforward_rt;

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
void
ip_input(struct mbuf *m)
{
	struct ip *ip;
	struct ipq *fp;
	struct in_ifaddr *ia;
	int hlen;

#ifdef	DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("ip_input no HDR");
#endif
	/*
	 * If no IP addresses have been set yet but the interfaces
	 * are receiving, can't do anything with incoming packets yet.
	 * XXX This is broken! We should be able to receive broadcasts
	 * and multicasts even without any local addresses configured.
	 */
	if (TAILQ_EMPTY(&in_ifaddrhead))
		goto bad;
	ipstat.ips_total++;

	if (m->m_pkthdr.len < sizeof(struct ip))
		goto tooshort;

#ifdef	DIAGNOSTIC
	if (m->m_len < sizeof(struct ip))
		panic("ipintr mbuf too short");
#endif

	if (m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == 0) {
		ipstat.ips_toosmall++;
		return;
	}
	ip = mtod(m, struct ip *);

	if (IP_VHL_V(ip->ip_vhl) != IPVERSION) {
		ipstat.ips_badvers++;
		goto bad;
	}

	hlen = IP_VHL_HL(ip->ip_vhl) << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
		ipstat.ips_badhlen++;
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			ipstat.ips_badhlen++;
			return;
		}
		ip = mtod(m, struct ip *);
	}
	if (hlen == sizeof(struct ip)) {
		ip->ip_sum = in_cksum_hdr(ip);
	} else {
		ip->ip_sum = in_cksum(m, hlen);
	}
	if (ip->ip_sum) {
		ipstat.ips_badsum++;
		goto bad;
	}

	/*
	 * Convert fields to host representation.
	 */
	NTOHS(ip->ip_len);
	if (ip->ip_len < hlen) {
		ipstat.ips_badlen++;
		goto bad;
	}
	NTOHS(ip->ip_id);
	NTOHS(ip->ip_off);

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < ip->ip_len) {
tooshort:
		ipstat.ips_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > ip->ip_len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip->ip_len;
			m->m_pkthdr.len = ip->ip_len;
		} else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}
	/*
	 * IpHack's section.
	 * Right now when no processing on packet has done
	 * and it is still fresh out of network we do our black
	 * deals with it.
	 * - Firewall: deny/allow/divert
	 * - Xlate: translate packet's addr/port (NAT).
	 * - Wrap: fake packet's addr/port <unimpl.>
	 * - Encapsulate: put it in another IP and send out. <unimp.>
 	 */

#ifdef COMPAT_IPFW
	if (ip_fw_chk_ptr) {
		int action;

#ifdef IPDIVERT
		action = (*ip_fw_chk_ptr)(&ip, hlen,
				m->m_pkthdr.rcvif, ip_divert_ignore, &m);
#else
		action = (*ip_fw_chk_ptr)(&ip, hlen, m->m_pkthdr.rcvif, 0, &m);
#endif
		if (action == -1)
			return;
		if (action != 0) {
#ifdef IPDIVERT
			frag_divert_port = action;
			goto ours;
#else
			goto bad;	/* ipfw said divert but we can't */
#endif
		}
	}

        if (ip_nat_ptr && !(*ip_nat_ptr)(&ip, &m, m->m_pkthdr.rcvif, IP_NAT_IN))
		return;
#endif

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	ip_nhops = 0;		/* for source routed packets */
	if (hlen > sizeof (struct ip) && ip_dooptions(m))
		return;

        /* greedy RSVP, snatches any PATH packet of the RSVP protocol and no
         * matter if it is destined to another node, or whether it is 
         * a multicast one, RSVP wants it! and prevents it from being forwarded
         * anywhere else. Also checks if the rsvp daemon is running before
	 * grabbing the packet.
         */
	if (rsvp_on && ip->ip_p==IPPROTO_RSVP) 
		goto ours;

	/*
	 * Check our list of addresses, to see if the packet is for us.
	 */
	for (ia = in_ifaddrhead.tqh_first; ia; ia = ia->ia_link.tqe_next) {
#define	satosin(sa)	((struct sockaddr_in *)(sa))

		if (IA_SIN(ia)->sin_addr.s_addr == ip->ip_dst.s_addr)
			goto ours;
		if (ia->ia_ifp && ia->ia_ifp->if_flags & IFF_BROADCAST) {
			if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
			    ip->ip_dst.s_addr)
				goto ours;
			if (ip->ip_dst.s_addr == ia->ia_netbroadcast.s_addr)
				goto ours;
		}
	}
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		struct in_multi *inm;
		if (ip_mrouter) {
			/*
			 * If we are acting as a multicast router, all
			 * incoming multicast packets are passed to the
			 * kernel-level multicast forwarding function.
			 * The packet is returned (relatively) intact; if
			 * ip_mforward() returns a non-zero value, the packet
			 * must be discarded, else it may be accepted below.
			 *
			 * (The IP ident field is put in the same byte order
			 * as expected when ip_mforward() is called from
			 * ip_output().)
			 */
			ip->ip_id = htons(ip->ip_id);
			if (ip_mforward(ip, m->m_pkthdr.rcvif, m, 0) != 0) {
				ipstat.ips_cantforward++;
				m_freem(m);
				return;
			}
			ip->ip_id = ntohs(ip->ip_id);

			/*
			 * The process-level routing demon needs to receive
			 * all multicast IGMP packets, whether or not this
			 * host belongs to their destination groups.
			 */
			if (ip->ip_p == IPPROTO_IGMP)
				goto ours;
			ipstat.ips_forward++;
		}
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN_LOOKUP_MULTI(ip->ip_dst, m->m_pkthdr.rcvif, inm);
		if (inm == NULL) {
			ipstat.ips_cantforward++;
			m_freem(m);
			return;
		}
		goto ours;
	}
	if (ip->ip_dst.s_addr == (u_long)INADDR_BROADCAST)
		goto ours;
	if (ip->ip_dst.s_addr == INADDR_ANY)
		goto ours;

	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (ipforwarding == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
	} else
		ip_forward(m, 0);
	return;

ours:

	/*
	 * If offset or IP_MF are set, must reassemble.
	 * Otherwise, nothing need be done.
	 * (We could look in the reassembly queue to see
	 * if the packet was previously fragmented,
	 * but it's not worth the time; just let them time out.)
	 */
	if (ip->ip_off & (IP_MF | IP_OFFMASK)) {
		if (m->m_flags & M_EXT) {		/* XXX */
			if ((m = m_pullup(m, sizeof (struct ip))) == 0) {
				ipstat.ips_toosmall++;
#ifdef IPDIVERT
				frag_divert_port = 0;
#endif
				return;
			}
			ip = mtod(m, struct ip *);
		}
		/*
		 * Look for queue of fragments
		 * of this datagram.
		 */
		for (fp = ipq.next; fp != &ipq; fp = fp->next)
			if (ip->ip_id == fp->ipq_id &&
			    ip->ip_src.s_addr == fp->ipq_src.s_addr &&
			    ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
			    ip->ip_p == fp->ipq_p)
				goto found;
		fp = 0;
found:

		/*
		 * Adjust ip_len to not reflect header,
		 * set ip_mff if more fragments are expected,
		 * convert offset of this to bytes.
		 */
		ip->ip_len -= hlen;
		((struct ipasfrag *)ip)->ipf_mff &= ~1;
		if (ip->ip_off & IP_MF)
			((struct ipasfrag *)ip)->ipf_mff |= 1;
		ip->ip_off <<= 3;

		/*
		 * If datagram marked as having more fragments
		 * or if this is not the first fragment,
		 * attempt reassembly; if it succeeds, proceed.
		 */
		if (((struct ipasfrag *)ip)->ipf_mff & 1 || ip->ip_off) {
			ipstat.ips_fragments++;
			ip = ip_reass((struct ipasfrag *)ip, fp);
			if (ip == 0)
				return;
			ipstat.ips_reassembled++;
			m = dtom(ip);
		} else
			if (fp)
				ip_freef(fp);
	} else
		ip->ip_len -= hlen;

#ifdef IPDIVERT
	/*
	 * Divert packets here to the divert protocol if required
	 */
	if (frag_divert_port) {
		ip_divert_port = frag_divert_port;
		frag_divert_port = 0;
		(*inetsw[ip_protox[IPPROTO_DIVERT]].pr_input)(m, hlen);
		return;
	}
#endif

	/*
	 * Switch out to protocol's input routine.
	 */
	ipstat.ips_delivered++;
	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, hlen);
	return;
bad:
	m_freem(m);
}

/*
 * IP software interrupt routine - to go away sometime soon
 */
static void
ipintr(void)
{
	int s;
	struct mbuf *m;

	while(1) {
		s = splimp();
		IF_DEQUEUE(&ipintrq, m);
		splx(s);
		if (m == 0)
			return;
		ip_input(m);
	}
}

NETISR_SET(NETISR_IP, ipintr);
  
/*
 * Take incoming datagram fragment and try to
 * reassemble it into whole datagram.  If a chain for
 * reassembly of this datagram already exists, then it
 * is given as fp; otherwise have to make a chain.
 */
static struct ip *
ip_reass(ip, fp)
	register struct ipasfrag *ip;
	register struct ipq *fp;
{
	register struct mbuf *m = dtom(ip);
	register struct ipasfrag *q;
	struct mbuf *t;
	int hlen = ip->ip_hl << 2;
	int i, next;

	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	m->m_data += hlen;
	m->m_len -= hlen;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == 0) {
		if ((t = m_get(M_DONTWAIT, MT_FTABLE)) == NULL)
			goto dropfrag;
		fp = mtod(t, struct ipq *);
		insque(fp, &ipq);
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ip->ip_p;
		fp->ipq_id = ip->ip_id;
		fp->ipq_next = fp->ipq_prev = (struct ipasfrag *)fp;
		fp->ipq_src = ((struct ip *)ip)->ip_src;
		fp->ipq_dst = ((struct ip *)ip)->ip_dst;
#ifdef IPDIVERT
		fp->ipq_divert = 0;
#endif
		q = (struct ipasfrag *)fp;
		goto insert;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (q = fp->ipq_next; q != (struct ipasfrag *)fp; q = q->ipf_next)
		if (q->ip_off > ip->ip_off)
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (q->ipf_prev != (struct ipasfrag *)fp) {
		i = q->ipf_prev->ip_off + q->ipf_prev->ip_len - ip->ip_off;
		if (i > 0) {
			if (i >= ip->ip_len)
				goto dropfrag;
			m_adj(dtom(ip), i);
			ip->ip_off += i;
			ip->ip_len -= i;
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q != (struct ipasfrag *)fp && ip->ip_off + ip->ip_len > q->ip_off) {
		struct mbuf *m0;

		i = (ip->ip_off + ip->ip_len) - q->ip_off;
		if (i < q->ip_len) {
			q->ip_len -= i;
			q->ip_off += i;
			m_adj(dtom(q), i);
			break;
		}
		m0 = dtom(q);
		q = q->ipf_next;
		ip_deq(q->ipf_prev);
		m_freem(m0);
	}

insert:

#ifdef IPDIVERT
	/*
	 * Any fragment diverting causes the whole packet to divert
	 */
	if (frag_divert_port != 0)
		fp->ipq_divert = frag_divert_port;
	frag_divert_port = 0;
#endif

	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 */
	ip_enq(ip, q->ipf_prev);
	next = 0;
	for (q = fp->ipq_next; q != (struct ipasfrag *)fp; q = q->ipf_next) {
		if (q->ip_off != next)
			return (0);
		next += q->ip_len;
	}
	if (q->ipf_prev->ipf_mff & 1)
		return (0);

	/*
	 * Reassembly is complete.  Make sure the packet is a sane size.
	 */
	if (next + (IP_VHL_HL(((struct ip *)fp->ipq_next)->ip_vhl) << 2)
							> IP_MAXPACKET) {
		ipstat.ips_toolong++;
		ip_freef(fp);
		return (0);
	}

	/*
	 * Concatenate fragments.
	 */
	q = fp->ipq_next;
	m = dtom(q);
	t = m->m_next;
	m->m_next = 0;
	m_cat(m, t);
	q = q->ipf_next;
	while (q != (struct ipasfrag *)fp) {
		t = dtom(q);
		q = q->ipf_next;
		m_cat(m, t);
	}

#ifdef IPDIVERT
	/*
	 * Record divert port for packet, if any
	 */
	frag_divert_port = fp->ipq_divert;
#endif

	/*
	 * Create header for new ip packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip = fp->ipq_next;
	ip->ip_len = next;
	ip->ipf_mff &= ~1;
	((struct ip *)ip)->ip_src = fp->ipq_src;
	((struct ip *)ip)->ip_dst = fp->ipq_dst;
	remque(fp);
	(void) m_free(dtom(fp));
	m = dtom(ip);
	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);
	/* some debugging cruft by sklower, below, will go away soon */
	if (m->m_flags & M_PKTHDR) { /* XXX this should be done elsewhere */
		register int plen = 0;
		for (t = m; m; m = m->m_next)
			plen += m->m_len;
		t->m_pkthdr.len = plen;
	}
	return ((struct ip *)ip);

dropfrag:
	ipstat.ips_fragdropped++;
	m_freem(m);
	return (0);
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
static void
ip_freef(fp)
	struct ipq *fp;
{
	register struct ipasfrag *q, *p;

	for (q = fp->ipq_next; q != (struct ipasfrag *)fp; q = p) {
		p = q->ipf_next;
		ip_deq(q);
		m_freem(dtom(q));
	}
	remque(fp);
	(void) m_free(dtom(fp));
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
static void
ip_enq(p, prev)
	register struct ipasfrag *p, *prev;
{

	p->ipf_prev = prev;
	p->ipf_next = prev->ipf_next;
	prev->ipf_next->ipf_prev = p;
	prev->ipf_next = p;
}

/*
 * To ip_enq as remque is to insque.
 */
static void
ip_deq(p)
	register struct ipasfrag *p;
{

	p->ipf_prev->ipf_next = p->ipf_next;
	p->ipf_next->ipf_prev = p->ipf_prev;
}

/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
ip_slowtimo()
{
	register struct ipq *fp;
	int s = splnet();

	fp = ipq.next;
	if (fp == 0) {
		splx(s);
		return;
	}
	while (fp != &ipq) {
		--fp->ipq_ttl;
		fp = fp->next;
		if (fp->prev->ipq_ttl == 0) {
			ipstat.ips_fragtimeout++;
			ip_freef(fp->prev);
		}
	}
	splx(s);
}

/*
 * Drain off all datagram fragments.
 */
void
ip_drain()
{
	while (ipq.next != &ipq) {
		ipstat.ips_fragdropped++;
		ip_freef(ipq.next);
	}

	in_rtqdrain();
}

/*
 * Do option processing on a datagram,
 * possibly discarding it if bad options are encountered,
 * or forwarding it if source-routed.
 * Returns 1 if packet has been forwarded/freed,
 * 0 if the packet should be processed further.
 */
static int
ip_dooptions(m)
	struct mbuf *m;
{
	register struct ip *ip = mtod(m, struct ip *);
	register u_char *cp;
	register struct ip_timestamp *ipt;
	register struct in_ifaddr *ia;
	int opt, optlen, cnt, off, code, type = ICMP_PARAMPROB, forward = 0;
	struct in_addr *sin, dst;
	n_time ntime;

	dst = ip->ip_dst;
	cp = (u_char *)(ip + 1);
	cnt = (IP_VHL_HL(ip->ip_vhl) << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
		}
		switch (opt) {

		default:
			break;

		/*
		 * Source routing with record.
		 * Find interface with current destination address.
		 * If none on this machine then drop if strictly routed,
		 * or do nothing if loosely routed.
		 * Record interface address and bring up next address
		 * component.  If strictly routed make sure next
		 * address is on directly accessible net.
		 */
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			ipaddr.sin_addr = ip->ip_dst;
			ia = (struct in_ifaddr *)
				ifa_ifwithaddr((struct sockaddr *)&ipaddr);
			if (ia == 0) {
				if (opt == IPOPT_SSRR) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				/*
				 * Loose routing, and not at next destination
				 * yet; nothing to do except forward.
				 */
				break;
			}
			off--;			/* 0 origin */
			if (off > optlen - sizeof(struct in_addr)) {
				/*
				 * End of source route.  Should be for us.
				 */
				save_rte(cp, ip->ip_src);
				break;
			}

			if (!ip_dosourceroute) {
				char buf[4*sizeof "123"];
				strcpy(buf, inet_ntoa(ip->ip_dst));

				log(LOG_WARNING, 
				    "attempted source route from %s to %s\n",
				    inet_ntoa(ip->ip_src), buf);
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}

			/*
			 * locate outgoing interface
			 */
			(void)memcpy(&ipaddr.sin_addr, cp + off,
			    sizeof(ipaddr.sin_addr));

			if (opt == IPOPT_SSRR) {
#define	INA	struct in_ifaddr *
#define	SA	struct sockaddr *
			    if ((ia = (INA)ifa_ifwithdstaddr((SA)&ipaddr)) == 0)
				ia = (INA)ifa_ifwithnet((SA)&ipaddr);
			} else
				ia = ip_rtaddr(ipaddr.sin_addr);
			if (ia == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			ip->ip_dst = ipaddr.sin_addr;
			(void)memcpy(cp + off, &(IA_SIN(ia)->sin_addr),
			    sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			/*
			 * Let ip_intr's mcast routing check handle mcast pkts
			 */
			forward = !IN_MULTICAST(ntohl(ip->ip_dst.s_addr));
			break;

		case IPOPT_RR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			/*
			 * If no space remains, ignore.
			 */
			off--;			/* 0 origin */
			if (off > optlen - sizeof(struct in_addr))
				break;
			(void)memcpy(&ipaddr.sin_addr, &ip->ip_dst,
			    sizeof(ipaddr.sin_addr));
			/*
			 * locate outgoing interface; if we're the destination,
			 * use the incoming interface (should be same).
			 */
			if ((ia = (INA)ifa_ifwithaddr((SA)&ipaddr)) == 0 &&
			    (ia = ip_rtaddr(ipaddr.sin_addr)) == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_HOST;
				goto bad;
			}
			(void)memcpy(cp + off, &(IA_SIN(ia)->sin_addr),
			    sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;

		case IPOPT_TS:
			code = cp - (u_char *)ip;
			ipt = (struct ip_timestamp *)cp;
			if (ipt->ipt_len < 5)
				goto bad;
			if (ipt->ipt_ptr > ipt->ipt_len - sizeof (long)) {
				if (++ipt->ipt_oflw == 0)
					goto bad;
				break;
			}
			sin = (struct in_addr *)(cp + ipt->ipt_ptr - 1);
			switch (ipt->ipt_flg) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (ipt->ipt_ptr + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				ipaddr.sin_addr = dst;
				ia = (INA)ifaof_ifpforaddr((SA)&ipaddr,
							    m->m_pkthdr.rcvif);
				if (ia == 0)
					continue;
				(void)memcpy(sin, &IA_SIN(ia)->sin_addr,
				    sizeof(struct in_addr));
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (ipt->ipt_ptr + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				(void)memcpy(&ipaddr.sin_addr, sin,
				    sizeof(struct in_addr));
				if (ifa_ifwithaddr((SA)&ipaddr) == 0)
					continue;
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				goto bad;
			}
			ntime = iptime();
			(void)memcpy(cp + ipt->ipt_ptr - 1, &ntime,
			    sizeof(n_time));
			ipt->ipt_ptr += sizeof(n_time);
		}
	}
	if (forward) {
		ip_forward(m, 1);
		return (1);
	}
	return (0);
bad:
	ip->ip_len -= IP_VHL_HL(ip->ip_vhl) << 2;   /* XXX icmp_error adds in hdr length */
	icmp_error(m, type, code, 0, 0);
	ipstat.ips_badoptions++;
	return (1);
}

/*
 * Given address of next destination (final or next hop),
 * return internet address info of interface to be used to get there.
 */
static struct in_ifaddr *
ip_rtaddr(dst)
	 struct in_addr dst;
{
	register struct sockaddr_in *sin;

	sin = (struct sockaddr_in *) &ipforward_rt.ro_dst;

	if (ipforward_rt.ro_rt == 0 || dst.s_addr != sin->sin_addr.s_addr) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = dst;

		rtalloc_ign(&ipforward_rt, RTF_PRCLONING);
	}
	if (ipforward_rt.ro_rt == 0)
		return ((struct in_ifaddr *)0);
	return ((struct in_ifaddr *) ipforward_rt.ro_rt->rt_ifa);
}

/*
 * Save incoming source route for use in replies,
 * to be picked up later by ip_srcroute if the receiver is interested.
 */
void
save_rte(option, dst)
	u_char *option;
	struct in_addr dst;
{
	unsigned olen;

	olen = option[IPOPT_OLEN];
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("save_rte: olen %d\n", olen);
#endif
	if (olen > sizeof(ip_srcrt) - (1 + sizeof(dst)))
		return;
	bcopy(option, ip_srcrt.srcopt, olen);
	ip_nhops = (olen - IPOPT_OFFSET - 1) / sizeof(struct in_addr);
	ip_srcrt.dst = dst;
}

/*
 * Retrieve incoming source route for use in replies,
 * in the same form used by setsockopt.
 * The first hop is placed before the options, will be removed later.
 */
struct mbuf *
ip_srcroute()
{
	register struct in_addr *p, *q;
	register struct mbuf *m;

	if (ip_nhops == 0)
		return ((struct mbuf *)0);
	m = m_get(M_DONTWAIT, MT_SOOPTS);
	if (m == 0)
		return ((struct mbuf *)0);

#define OPTSIZ	(sizeof(ip_srcrt.nop) + sizeof(ip_srcrt.srcopt))

	/* length is (nhops+1)*sizeof(addr) + sizeof(nop + srcrt header) */
	m->m_len = ip_nhops * sizeof(struct in_addr) + sizeof(struct in_addr) +
	    OPTSIZ;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("ip_srcroute: nhops %d mlen %d", ip_nhops, m->m_len);
#endif

	/*
	 * First save first hop for return route
	 */
	p = &ip_srcrt.route[ip_nhops - 1];
	*(mtod(m, struct in_addr *)) = *p--;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf(" hops %lx", ntohl(mtod(m, struct in_addr *)->s_addr));
#endif

	/*
	 * Copy option fields and padding (nop) to mbuf.
	 */
	ip_srcrt.nop = IPOPT_NOP;
	ip_srcrt.srcopt[IPOPT_OFFSET] = IPOPT_MINOFF;
	(void)memcpy(mtod(m, caddr_t) + sizeof(struct in_addr),
	    &ip_srcrt.nop, OPTSIZ);
	q = (struct in_addr *)(mtod(m, caddr_t) +
	    sizeof(struct in_addr) + OPTSIZ);
#undef OPTSIZ
	/*
	 * Record return path as an IP source route,
	 * reversing the path (pointers are now aligned).
	 */
	while (p >= ip_srcrt.route) {
#ifdef DIAGNOSTIC
		if (ipprintfs)
			printf(" %lx", ntohl(q->s_addr));
#endif
		*q++ = *p--;
	}
	/*
	 * Last hop goes to final destination.
	 */
	*q = ip_srcrt.dst;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf(" %lx\n", ntohl(q->s_addr));
#endif
	return (m);
}

/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 * Second argument is buffer to which options
 * will be moved, and return value is their length.
 * XXX should be deleted; last arg currently ignored.
 */
void
ip_stripoptions(m, mopt)
	register struct mbuf *m;
	struct mbuf *mopt;
{
	register int i;
	struct ip *ip = mtod(m, struct ip *);
	register caddr_t opts;
	int olen;

	olen = (IP_VHL_HL(ip->ip_vhl) << 2) - sizeof (struct ip);
	opts = (caddr_t)(ip + 1);
	i = m->m_len - (sizeof (struct ip) + olen);
	bcopy(opts + olen, opts, (unsigned)i);
	m->m_len -= olen;
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len -= olen;
	ip->ip_vhl = IP_MAKE_VHL(IPVERSION, sizeof(struct ip) >> 2);
}

u_char inetctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
static void
ip_forward(m, srcrt)
	struct mbuf *m;
	int srcrt;
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct sockaddr_in *sin;
	register struct rtentry *rt;
	int error, type = 0, code = 0;
	struct mbuf *mcopy;
	n_long dest;
	struct ifnet *destifp;

	dest = 0;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("forward: src %lx dst %lx ttl %x\n",
			ip->ip_src.s_addr, ip->ip_dst.s_addr, ip->ip_ttl);
#endif


	if (m->m_flags & M_BCAST || in_canforward(ip->ip_dst) == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
		return;
	}
	HTONS(ip->ip_id);
	if (ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, dest, 0);
		return;
	}
	ip->ip_ttl -= IPTTLDEC;

	sin = (struct sockaddr_in *)&ipforward_rt.ro_dst;
	if ((rt = ipforward_rt.ro_rt) == 0 ||
	    ip->ip_dst.s_addr != sin->sin_addr.s_addr) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;

		rtalloc_ign(&ipforward_rt, RTF_PRCLONING);
		if (ipforward_rt.ro_rt == 0) {
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, dest, 0);
			return;
		}
		rt = ipforward_rt.ro_rt;
	}

	/*
	 * Save at most 64 bytes of the packet in case
	 * we need to generate an ICMP message to the src.
	 */
	mcopy = m_copy(m, 0, imin((int)ip->ip_len, 64));

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 */
#define	satosin(sa)	((struct sockaddr_in *)(sa))
	if (rt->rt_ifp == m->m_pkthdr.rcvif &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
	    satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
	    ipsendredirects && !srcrt) {
#define	RTA(rt)	((struct in_ifaddr *)(rt->rt_ifa))
		u_long src = ntohl(ip->ip_src.s_addr);

		if (RTA(rt) &&
		    (src & RTA(rt)->ia_subnetmask) == RTA(rt)->ia_subnet) {
		    if (rt->rt_flags & RTF_GATEWAY)
			dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
		    else
			dest = ip->ip_dst.s_addr;
		    /* Router requirements says to only send host redirects */
		    type = ICMP_REDIRECT;
		    code = ICMP_REDIRECT_HOST;
#ifdef DIAGNOSTIC
		    if (ipprintfs)
		        printf("redirect (%d) to %lx\n", code, (u_long)dest);
#endif
		}
	}

	error = ip_output(m, (struct mbuf *)0, &ipforward_rt, 
			  IP_FORWARDING, 0);
	if (error)
		ipstat.ips_cantforward++;
	else {
		ipstat.ips_forward++;
		if (type)
			ipstat.ips_redirectsent++;
		else {
			if (mcopy)
				m_freem(mcopy);
			return;
		}
	}
	if (mcopy == NULL)
		return;
	destifp = NULL;

	switch (error) {

	case 0:				/* forwarded, but need redirect */
		/* type, code set above */
		break;

	case ENETUNREACH:		/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;
		if (ipforward_rt.ro_rt)
			destifp = ipforward_rt.ro_rt->rt_ifp;
		ipstat.ips_cantfrag++;
		break;

	case ENOBUFS:
		type = ICMP_SOURCEQUENCH;
		code = 0;
		break;
	}
	icmp_error(mcopy, type, code, dest, destifp);
}

void
ip_savecontrol(inp, mp, ip, m)
	register struct inpcb *inp;
	register struct mbuf **mp;
	register struct ip *ip;
	register struct mbuf *m;
{
	if (inp->inp_socket->so_options & SO_TIMESTAMP) {
		struct timeval tv;

		microtime(&tv);
		*mp = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
			SCM_TIMESTAMP, SOL_SOCKET);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inp->inp_flags & INP_RECVDSTADDR) {
		*mp = sbcreatecontrol((caddr_t) &ip->ip_dst,
		    sizeof(struct in_addr), IP_RECVDSTADDR, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#ifdef notyet
	/* XXX
	 * Moving these out of udp_input() made them even more broken
	 * than they already were.
	 */
	/* options were tossed already */
	if (inp->inp_flags & INP_RECVOPTS) {
		*mp = sbcreatecontrol((caddr_t) opts_deleted_above,
		    sizeof(struct in_addr), IP_RECVOPTS, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	/* ip_srcroute doesn't do what we want here, need to fix */
	if (inp->inp_flags & INP_RECVRETOPTS) {
		*mp = sbcreatecontrol((caddr_t) ip_srcroute(),
		    sizeof(struct in_addr), IP_RECVRETOPTS, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#endif
	if (inp->inp_flags & INP_RECVIF) {
		struct sockaddr_dl sdl;

		sdl.sdl_len = offsetof(struct sockaddr_dl, sdl_data[0]);
		sdl.sdl_family = AF_LINK;
		sdl.sdl_index = m->m_pkthdr.rcvif ?
			m->m_pkthdr.rcvif->if_index : 0;
		sdl.sdl_nlen = sdl.sdl_alen = sdl.sdl_slen = 0;
		*mp = sbcreatecontrol((caddr_t) &sdl, sdl.sdl_len,
			IP_RECVIF, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
}

int
ip_rsvp_init(struct socket *so)
{
	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
	  return EOPNOTSUPP;

	if (ip_rsvpd != NULL)
	  return EADDRINUSE;

	ip_rsvpd = so;
	/*
	 * This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!ip_rsvp_on) {
		ip_rsvp_on = 1;
		rsvp_on++;
	}

	return 0;
}

int
ip_rsvp_done(void)
{
	ip_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (ip_rsvp_on) {
		ip_rsvp_on = 0;
		rsvp_on--;
	}
	return 0;
}
