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
 * $FreeBSD$
 */

#define	_IP_VHL

#include "opt_bootp.h"
#include "opt_ipfw.h"
#include "opt_ipdn.h"
#include "opt_ipdivert.h"
#include "opt_ipfilter.h"
#include "opt_ipstealth.h"
#include "opt_ipsec.h"
#include "opt_random_ip_id.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/intrq.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <machine/in_cksum.h>

#include <netinet/ipprotosw.h>

#include <sys/socketvar.h>

#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/key.h>
#endif

int rsvp_on = 0;
static int ip_rsvp_on;
struct socket *ip_rsvpd;

int	ipforwarding = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_FORWARDING, forwarding, CTLFLAG_RW,
    &ipforwarding, 0, "Enable IP forwarding between interfaces");

static int	ipsendredirects = 1; /* XXX */
SYSCTL_INT(_net_inet_ip, IPCTL_SENDREDIRECTS, redirect, CTLFLAG_RW,
    &ipsendredirects, 0, "Enable sending IP redirects");

int	ip_defttl = IPDEFTTL;
SYSCTL_INT(_net_inet_ip, IPCTL_DEFTTL, ttl, CTLFLAG_RW,
    &ip_defttl, 0, "Maximum TTL on IP packets");

static int	ip_dosourceroute = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_SOURCEROUTE, sourceroute, CTLFLAG_RW,
    &ip_dosourceroute, 0, "Enable forwarding source routed IP packets");

static int	ip_acceptsourceroute = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_ACCEPTSOURCEROUTE, accept_sourceroute, 
    CTLFLAG_RW, &ip_acceptsourceroute, 0, 
    "Enable accepting source routed IP packets");

static int	ip_keepfaith = 0;
SYSCTL_INT(_net_inet_ip, IPCTL_KEEPFAITH, keepfaith, CTLFLAG_RW,
	&ip_keepfaith,	0,
	"Enable packet capture for FAITH IPv4->IPv6 translater daemon");

static int	nipq = 0;	/* total # of reass queues */
static int	maxnipq;
SYSCTL_INT(_net_inet_ip, OID_AUTO, maxfragpackets, CTLFLAG_RW,
	&maxnipq, 0,
	"Maximum number of IPv4 fragment reassembly queue entries");

static int    maxfragsperpacket;
SYSCTL_INT(_net_inet_ip, OID_AUTO, maxfragsperpacket, CTLFLAG_RW,
	&maxfragsperpacket, 0,
	"Maximum number of IPv4 fragments allowed per packet");

static int	ip_sendsourcequench = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, sendsourcequench, CTLFLAG_RW,
	&ip_sendsourcequench, 0,
	"Enable the transmission of source quench packets");

/*
 * XXX - Setting ip_checkinterface mostly implements the receive side of
 * the Strong ES model described in RFC 1122, but since the routing table
 * and transmit implementation do not implement the Strong ES model,
 * setting this to 1 results in an odd hybrid.
 *
 * XXX - ip_checkinterface currently must be disabled if you use ipnat
 * to translate the destination address to another local interface.
 *
 * XXX - ip_checkinterface must be disabled if you add IP aliases
 * to the loopback interface instead of the interface where the
 * packets for those addresses are received.
 */
static int	ip_checkinterface = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, check_interface, CTLFLAG_RW,
    &ip_checkinterface, 0, "Verify packet arrives on correct interface");

#ifdef DIAGNOSTIC
static int	ipprintfs = 0;
#endif

static int	ipqmaxlen = IFQ_MAXLEN;

extern	struct domain inetdomain;
extern	struct ipprotosw inetsw[];
u_char	ip_protox[IPPROTO_MAX];
struct	in_ifaddrhead in_ifaddrhead;		/* first inet address */
struct	in_ifaddrhashhead *in_ifaddrhashtbl;	/* inet addr hash table */
u_long	in_ifaddrhmask;				/* mask for hash table */
SYSCTL_INT(_net_inet_ip, IPCTL_INTRQMAXLEN, intr_queue_maxlen, CTLFLAG_RW,
    &ipintrq.ifq_maxlen, 0, "Maximum size of the IP input queue");
SYSCTL_INT(_net_inet_ip, IPCTL_INTRQDROPS, intr_queue_drops, CTLFLAG_RD,
    &ipintrq.ifq_drops, 0, "Number of packets dropped from the IP input queue");

struct ipstat ipstat;
SYSCTL_STRUCT(_net_inet_ip, IPCTL_STATS, stats, CTLFLAG_RW,
    &ipstat, ipstat, "IP statistics (struct ipstat, netinet/ip_var.h)");

/* Packet reassembly stuff */
#define IPREASS_NHASH_LOG2      6
#define IPREASS_NHASH           (1 << IPREASS_NHASH_LOG2)
#define IPREASS_HMASK           (IPREASS_NHASH - 1)
#define IPREASS_HASH(x,y) \
	(((((x) & 0xF) | ((((x) >> 8) & 0xF) << 4)) ^ (y)) & IPREASS_HMASK)

static struct ipq ipq[IPREASS_NHASH];
const  int    ipintrq_present = 1;

#ifdef IPCTL_DEFMTU
SYSCTL_INT(_net_inet_ip, IPCTL_DEFMTU, mtu, CTLFLAG_RW,
    &ip_mtu, 0, "Default MTU");
#endif

#ifdef IPSTEALTH
static int	ipstealth = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, stealth, CTLFLAG_RW,
    &ipstealth, 0, "");
#endif


/* Firewall hooks */
ip_fw_chk_t *ip_fw_chk_ptr;
int fw_enable = 1 ;
int fw_one_pass = 1;

/* Dummynet hooks */
ip_dn_io_t *ip_dn_io_ptr;

int (*fr_checkp) __P((struct ip *, int, struct ifnet *, int, struct mbuf **)) = NULL;

/*
 * XXX this is ugly -- the following two global variables are
 * used to store packet state while it travels through the stack.
 * Note that the code even makes assumptions on the size and
 * alignment of fields inside struct ip_srcrt so e.g. adding some
 * fields will break the code. This needs to be fixed.
 *
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

static void	save_rte(u_char *, struct in_addr);
static int	ip_dooptions(struct mbuf *m, int,
			struct sockaddr_in *next_hop);
static void	ip_forward(struct mbuf *m, int srcrt,
			struct sockaddr_in *next_hop);
static void	ip_freef(struct ipq *);
static struct	mbuf *ip_reass(struct mbuf *, struct ipq *,
		struct ipq *, u_int32_t *, u_int16_t *);
static void	ipintr(void);

/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init()
{
	register struct ipprotosw *pr;
	register int i;

	TAILQ_INIT(&in_ifaddrhead);
	in_ifaddrhashtbl = hashinit(INADDR_NHASH, M_IFADDR, &in_ifaddrhmask);
	pr = (struct ipprotosw *)pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = pr - inetsw;
	for (pr = (struct ipprotosw *)inetdomain.dom_protosw;
	    pr < (struct ipprotosw *)inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip_protox[pr->pr_protocol] = pr - inetsw;

	for (i = 0; i < IPREASS_NHASH; i++)
	    ipq[i].next = ipq[i].prev = &ipq[i];

	maxnipq = nmbclusters / 32;
	maxfragsperpacket = 16;

#ifndef RANDOM_IP_ID
	ip_id = time_second & 0xffff;
#endif
	ipintrq.ifq_maxlen = ipqmaxlen;

	register_netisr(NETISR_IP, ipintr);
}

/*
 * XXX watch out this one. It is perhaps used as a cache for
 * the most recently used route ? it is cleared in in_addroute()
 * when a new route is successfully created.
 */
struct	route ipforward_rt;
static struct	sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
void
ip_input(struct mbuf *m)
{
	struct ip *ip;
	struct ipq *fp;
	struct in_ifaddr *ia = NULL;
	struct ifaddr *ifa;
	int    i, hlen, checkif;
	u_short sum;
	struct in_addr pkt_dst;
	u_int32_t divert_info = 0;		/* packet divert/tee info */
	struct ip_fw_args args;
#ifdef FAST_IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int s, error;
#endif /* FAST_IPSEC */

	args.eh = NULL;
	args.oif = NULL;
	args.rule = NULL;
	args.divert_rule = 0;			/* divert cookie */
	args.next_hop = NULL;

	/* Grab info from MT_TAG mbufs prepended to the chain.	*/
	for (; m && m->m_type == MT_TAG; m = m->m_next) {
		switch(m->_m_tag_id) {
		default:
			printf("ip_input: unrecognised MT_TAG tag %d\n",
			    m->_m_tag_id);
			break;

		case PACKET_TAG_DUMMYNET:
			args.rule = ((struct dn_pkt *)m)->rule;
			break;

		case PACKET_TAG_DIVERT:
			args.divert_rule = (int)m->m_hdr.mh_data & 0xffff;
			break;

		case PACKET_TAG_IPFORWARD:
			args.next_hop = (struct sockaddr_in *)m->m_hdr.mh_data;
			break;
		}
	}

	KASSERT(m != NULL && (m->m_flags & M_PKTHDR) != 0,
	    ("ip_input: no HDR"));

	if (args.rule) {	/* dummynet already filtered us */
		ip = mtod(m, struct ip *);
		hlen = IP_VHL_HL(ip->ip_vhl) << 2;
		goto iphack ;
	}

	ipstat.ips_total++;

	if (m->m_pkthdr.len < sizeof(struct ip))
		goto tooshort;

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

	/* 127/8 must not appear on wire - RFC1122 */
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		if ((m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) == 0) {
			ipstat.ips_badaddr++;
			goto bad;
		}
	}

	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		sum = !(m->m_pkthdr.csum_flags & CSUM_IP_VALID);
	} else {
		if (hlen == sizeof(struct ip)) {
			sum = in_cksum_hdr(ip);
		} else {
			sum = in_cksum(m, hlen);
		}
	}
	if (sum) {
		ipstat.ips_badsum++;
		goto bad;
	}

	/*
	 * Convert fields to host representation.
	 */
	ip->ip_len = ntohs(ip->ip_len);
	if (ip->ip_len < hlen) {
		ipstat.ips_badlen++;
		goto bad;
	}
	ip->ip_off = ntohs(ip->ip_off);

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
#if defined(IPSEC) && !defined(IPSEC_FILTERGIF)
	/*
	 * Bypass packet filtering for packets from a tunnel (gif).
	 */
	if (ipsec_gethist(m, NULL))
		goto pass;
#endif
#if defined(FAST_IPSEC) && !defined(IPSEC_FILTERGIF)
	/*
	 * Bypass packet filtering for packets from a tunnel (gif).
	 */
	if (m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL)
		goto pass;
#endif

	/*
	 * IpHack's section.
	 * Right now when no processing on packet has done
	 * and it is still fresh out of network we do our black
	 * deals with it.
	 * - Firewall: deny/allow/divert
	 * - Xlate: translate packet's addr/port (NAT).
	 * - Pipe: pass pkt through dummynet.
	 * - Wrap: fake packet's addr/port <unimpl.>
	 * - Encapsulate: put it in another IP and send out. <unimp.>
 	 */

iphack:
	/*
	 * Check if we want to allow this packet to be processed.
	 * Consider it to be bad if not.
	 */
	if (fr_checkp) {
		struct	mbuf	*m1 = m;

		if ((*fr_checkp)(ip, hlen, m->m_pkthdr.rcvif, 0, &m1) || !m1)
			return;
		ip = mtod(m = m1, struct ip *);
	}
	if (fw_enable && IPFW_LOADED) {
		/*
		 * If we've been forwarded from the output side, then
		 * skip the firewall a second time
		 */
		if (args.next_hop)
			goto ours;

		args.m = m;
		i = ip_fw_chk_ptr(&args);
		m = args.m;

		if ( (i & IP_FW_PORT_DENY_FLAG) || m == NULL) { /* drop */
			if (m)
				m_freem(m);
			return;
		}
		ip = mtod(m, struct ip *); /* just in case m changed */
		if (i == 0 && args.next_hop == NULL)	/* common case */
			goto pass;
                if (DUMMYNET_LOADED && (i & IP_FW_PORT_DYNT_FLAG) != 0) {
			/* Send packet to the appropriate pipe */
			ip_dn_io_ptr(m, i&0xffff, DN_TO_IP_IN, &args);
			return;
		}
#ifdef IPDIVERT
		if (i != 0 && (i & IP_FW_PORT_DYNT_FLAG) == 0) {
			/* Divert or tee packet */
			divert_info = i;
			goto ours;
		}
#endif
		if (i == 0 && args.next_hop != NULL)
			goto pass;
		/*
		 * if we get here, the packet must be dropped
		 */
		m_freem(m);
		return;
	}
pass:

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	ip_nhops = 0;		/* for source routed packets */
	if (hlen > sizeof (struct ip) && ip_dooptions(m, 0, args.next_hop))
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
	 * If we don't have any addresses, assume any unicast packet
	 * we receive might be for us (and let the upper layers deal
	 * with it).
	 */
	if (TAILQ_EMPTY(&in_ifaddrhead) &&
	    (m->m_flags & (M_MCAST|M_BCAST)) == 0)
		goto ours;

	/*
	 * Cache the destination address of the packet; this may be
	 * changed by use of 'ipfw fwd'.
	 */
	pkt_dst = args.next_hop ? args.next_hop->sin_addr : ip->ip_dst;

	/*
	 * Enable a consistency check between the destination address
	 * and the arrival interface for a unicast packet (the RFC 1122
	 * strong ES model) if IP forwarding is disabled and the packet
	 * is not locally generated and the packet is not subject to
	 * 'ipfw fwd'.
	 *
	 * XXX - Checking also should be disabled if the destination
	 * address is ipnat'ed to a different interface.
	 *
	 * XXX - Checking is incompatible with IP aliases added
	 * to the loopback interface instead of the interface where
	 * the packets are received.
	 */
	checkif = ip_checkinterface && (ipforwarding == 0) && 
	    m->m_pkthdr.rcvif != NULL &&
	    ((m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) == 0) &&
	    (args.next_hop == NULL);

	/*
	 * Check for exact addresses in the hash bucket.
	 */
	LIST_FOREACH(ia, INADDR_HASH(pkt_dst.s_addr), ia_hash) {
		/*
		 * If the address matches, verify that the packet
		 * arrived via the correct interface if checking is
		 * enabled.
		 */
		if (IA_SIN(ia)->sin_addr.s_addr == pkt_dst.s_addr && 
		    (!checkif || ia->ia_ifp == m->m_pkthdr.rcvif))
			goto ours;
	}
	/*
	 * Check for broadcast addresses.
	 *
	 * Only accept broadcast packets that arrive via the matching
	 * interface.  Reception of forwarded directed broadcasts would
	 * be handled via ip_forward() and ether_output() with the loopback
	 * into the stack for SIMPLEX interfaces handled by ether_output().
	 */
	if (m->m_pkthdr.rcvif->if_flags & IFF_BROADCAST) {
	        TAILQ_FOREACH(ifa, &m->m_pkthdr.rcvif->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = ifatoia(ifa);
			if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
			    pkt_dst.s_addr)
				goto ours;
			if (ia->ia_netbroadcast.s_addr == pkt_dst.s_addr)
				goto ours;
#ifdef BOOTP_COMPAT
			if (IA_SIN(ia)->sin_addr.s_addr == INADDR_ANY)
				goto ours;
#endif
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
			 */
			if (ip_mforward &&
			    ip_mforward(ip, m->m_pkthdr.rcvif, m, 0) != 0) {
				ipstat.ips_cantforward++;
				m_freem(m);
				return;
			}

			/*
			 * The process-level routing daemon needs to receive
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
			ipstat.ips_notmember++;
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
	 * FAITH(Firewall Aided Internet Translator)
	 */
	if (m->m_pkthdr.rcvif && m->m_pkthdr.rcvif->if_type == IFT_FAITH) {
		if (ip_keepfaith) {
			if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_ICMP) 
				goto ours;
		}
		m_freem(m);
		return;
	}

	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (ipforwarding == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
	} else {
#ifdef IPSEC
		/*
		 * Enforce inbound IPsec SPD.
		 */
		if (ipsec4_in_reject(m, NULL)) {
			ipsecstat.in_polvio++;
			goto bad;
		}
#endif /* IPSEC */
#ifdef FAST_IPSEC
		mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
		s = splnet();
		if (mtag != NULL) {
			tdbi = (struct tdb_ident *)(mtag + 1);
			sp = ipsec_getpolicy(tdbi, IPSEC_DIR_INBOUND);
		} else {
			sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
						   IP_FORWARDING, &error);   
		}
		if (sp == NULL) {	/* NB: can happen if error */
			splx(s);
			/*XXX error stat???*/
			DPRINTF(("ip_input: no SP for forwarding\n"));	/*XXX*/
			goto bad;
		}

		/*
		 * Check security policy against packet attributes.
		 */
		error = ipsec_in_reject(sp, m);
		KEY_FREESP(&sp);
		splx(s);
		if (error) {
			ipstat.ips_cantforward++;
			goto bad;
		}
#endif /* FAST_IPSEC */
		ip_forward(m, 0, args.next_hop);
	}
	return;

ours:
#ifdef IPSTEALTH
	/*
	 * IPSTEALTH: Process non-routing options only
	 * if the packet is destined for us.
	 */
	if (ipstealth && hlen > sizeof (struct ip) &&
	    ip_dooptions(m, 1, args.next_hop))
		return;
#endif /* IPSTEALTH */

	/* Count the packet in the ip address stats */
	if (ia != NULL) {
		ia->ia_ifa.if_ipackets++;
		ia->ia_ifa.if_ibytes += m->m_pkthdr.len;
	}

	/*
	 * If offset or IP_MF are set, must reassemble.
	 * Otherwise, nothing need be done.
	 * (We could look in the reassembly queue to see
	 * if the packet was previously fragmented,
	 * but it's not worth the time; just let them time out.)
	 */
	if (ip->ip_off & (IP_MF | IP_OFFMASK)) {

		/* If maxnipq is 0, never accept fragments. */
		if (maxnipq == 0) {
                	ipstat.ips_fragments++;
			ipstat.ips_fragdropped++;
			goto bad;
		}

		sum = IPREASS_HASH(ip->ip_src.s_addr, ip->ip_id);
		/*
		 * Look for queue of fragments
		 * of this datagram.
		 */
		for (fp = ipq[sum].next; fp != &ipq[sum]; fp = fp->next)
			if (ip->ip_id == fp->ipq_id &&
			    ip->ip_src.s_addr == fp->ipq_src.s_addr &&
			    ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
			    ip->ip_p == fp->ipq_p)
				goto found;

		fp = 0;

		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly;
		 * If maxnipq is -1, accept all fragments without limitation.
		 */
		if ((nipq > maxnipq) && (maxnipq > 0)) {
		    /*
		     * drop something from the tail of the current queue
		     * before proceeding further
		     */
		    if (ipq[sum].prev == &ipq[sum]) {   /* gak */
			for (i = 0; i < IPREASS_NHASH; i++) {
			    if (ipq[i].prev != &ipq[i]) {
				ipstat.ips_fragtimeout +=
				    ipq[i].prev->ipq_nfrags;
				ip_freef(ipq[i].prev);
				break;
			    }
			}
		    } else {
			ipstat.ips_fragtimeout += ipq[sum].prev->ipq_nfrags;
			ip_freef(ipq[sum].prev);
		    }
		}
found:
		/*
		 * Adjust ip_len to not reflect header,
		 * convert offset of this to bytes.
		 */
		ip->ip_len -= hlen;
		if (ip->ip_off & IP_MF) {
		        /*
		         * Make sure that fragments have a data length
			 * that's a non-zero multiple of 8 bytes.
		         */
			if (ip->ip_len == 0 || (ip->ip_len & 0x7) != 0) {
				ipstat.ips_toosmall++; /* XXX */
				goto bad;
			}
			m->m_flags |= M_FRAG;
		} else
			m->m_flags &= ~M_FRAG;
		ip->ip_off <<= 3;

		/*
		 * Attempt reassembly; if it succeeds, proceed.
		 * ip_reass() will return a different mbuf, and update
		 * the divert info in divert_info and args.divert_rule.
		 */
		ipstat.ips_fragments++;
		m->m_pkthdr.header = ip;
		m = ip_reass(m,
		    fp, &ipq[sum], &divert_info, &args.divert_rule);
		if (m == 0)
			return;
		ipstat.ips_reassembled++;
		ip = mtod(m, struct ip *);
		/* Get the header length of the reassembled packet */
		hlen = IP_VHL_HL(ip->ip_vhl) << 2;
#ifdef IPDIVERT
		/* Restore original checksum before diverting packet */
		if (divert_info != 0) {
			ip->ip_len += hlen;
			ip->ip_len = htons(ip->ip_len);
			ip->ip_off = htons(ip->ip_off);
			ip->ip_sum = 0;
			if (hlen == sizeof(struct ip))
				ip->ip_sum = in_cksum_hdr(ip);
			else
				ip->ip_sum = in_cksum(m, hlen);
			ip->ip_off = ntohs(ip->ip_off);
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_len -= hlen;
		}
#endif
	} else
		ip->ip_len -= hlen;

#ifdef IPDIVERT
	/*
	 * Divert or tee packet to the divert protocol if required.
	 */
	if (divert_info != 0) {
		struct mbuf *clone = NULL;

		/* Clone packet if we're doing a 'tee' */
		if ((divert_info & IP_FW_PORT_TEE_FLAG) != 0)
			clone = m_dup(m, M_DONTWAIT);

		/* Restore packet header fields to original values */
		ip->ip_len += hlen;
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);

		/* Deliver packet to divert input routine */
		divert_packet(m, 1, divert_info & 0xffff, args.divert_rule);
		ipstat.ips_delivered++;

		/* If 'tee', continue with original packet */
		if (clone == NULL)
			return;
		m = clone;
		ip = mtod(m, struct ip *);
		ip->ip_len += hlen;
		/*
		 * Jump backwards to complete processing of the
		 * packet. But first clear divert_info to avoid
		 * entering this block again.
		 * We do not need to clear args.divert_rule
		 * or args.next_hop as they will not be used.
		 */
		divert_info = 0;
		goto pass;
	}
#endif

#ifdef IPSEC
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inetsw[ip_protox[ip->ip_p]].pr_flags & PR_LASTHDR) != 0 &&
	    ipsec4_in_reject(m, NULL)) {
		ipsecstat.in_polvio++;
		goto bad;
	}
#endif
#if FAST_IPSEC
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inetsw[ip_protox[ip->ip_p]].pr_flags & PR_LASTHDR) != 0) {
		/*
		 * Check if the packet has already had IPsec processing
		 * done.  If so, then just pass it along.  This tag gets
		 * set during AH, ESP, etc. input handling, before the
		 * packet is returned to the ip input queue for delivery.
		 */ 
		mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
		s = splnet();
		if (mtag != NULL) {
			tdbi = (struct tdb_ident *)(mtag + 1);
			sp = ipsec_getpolicy(tdbi, IPSEC_DIR_INBOUND);
		} else {
			sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
						   IP_FORWARDING, &error);   
		}
		if (sp != NULL) {
			/*
			 * Check security policy against packet attributes.
			 */
			error = ipsec_in_reject(sp, m);
			KEY_FREESP(&sp);
		} else {
			/* XXX error stat??? */
			error = EINVAL;
DPRINTF(("ip_input: no SP, packet discarded\n"));/*XXX*/
			goto bad;
		}
		splx(s);
		if (error)
			goto bad;
	}
#endif /* FAST_IPSEC */

	/*
	 * Switch out to protocol's input routine.
	 */
	ipstat.ips_delivered++;
	if (args.next_hop && ip->ip_p == IPPROTO_TCP) {
		/* TCP needs IPFORWARD info if available */
		struct m_hdr tag;

		tag.mh_type = MT_TAG;
		tag.mh_flags = PACKET_TAG_IPFORWARD;
		tag.mh_data = (caddr_t)args.next_hop;
		tag.mh_next = m;

		(*inetsw[ip_protox[ip->ip_p]].pr_input)(
			(struct mbuf *)&tag, hlen, ip->ip_p);
    	} else
		(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, hlen, ip->ip_p);
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

/*
 * Take incoming datagram fragment and try to reassemble it into
 * whole datagram.  If a chain for reassembly of this datagram already
 * exists, then it is given as fp; otherwise have to make a chain.
 *
 * When IPDIVERT enabled, keep additional state with each packet that
 * tells us if we need to divert or tee the packet we're building.
 * In particular, *divinfo includes the port and TEE flag,
 * *divert_rule is the number of the matching rule.
 */

static struct mbuf *
ip_reass(struct mbuf *m, struct ipq *fp, struct ipq *where,
	u_int32_t *divinfo, u_int16_t *divert_rule)
{
	struct ip *ip = mtod(m, struct ip *);
	register struct mbuf *p = 0, *q, *nq;
	struct mbuf *t;
	int hlen = IP_VHL_HL(ip->ip_vhl) << 2;
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
		insque(fp, where);
		nipq++;
		fp->ipq_nfrags = 1;
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ip->ip_p;
		fp->ipq_id = ip->ip_id;
		fp->ipq_src = ip->ip_src;
		fp->ipq_dst = ip->ip_dst;
		fp->ipq_frags = m;
		m->m_nextpkt = NULL;
#ifdef IPDIVERT
		fp->ipq_div_info = 0;
		fp->ipq_div_cookie = 0;
#endif
		goto inserted;
	} else {
		fp->ipq_nfrags++;
	}

#define GETIP(m)	((struct ip*)((m)->m_pkthdr.header))

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = fp->ipq_frags; q; p = q, q = q->m_nextpkt)
		if (GETIP(q)->ip_off > ip->ip_off)
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us, otherwise
	 * stick new segment in the proper place.
	 *
	 * If some of the data is dropped from the the preceding
	 * segment, then it's checksum is invalidated.
	 */
	if (p) {
		i = GETIP(p)->ip_off + GETIP(p)->ip_len - ip->ip_off;
		if (i > 0) {
			if (i >= ip->ip_len)
				goto dropfrag;
			m_adj(m, i);
			m->m_pkthdr.csum_flags = 0;
			ip->ip_off += i;
			ip->ip_len -= i;
		}
		m->m_nextpkt = p->m_nextpkt;
		p->m_nextpkt = m;
	} else {
		m->m_nextpkt = fp->ipq_frags;
		fp->ipq_frags = m;
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL && ip->ip_off + ip->ip_len > GETIP(q)->ip_off;
	     q = nq) {
		i = (ip->ip_off + ip->ip_len) -
		    GETIP(q)->ip_off;
		if (i < GETIP(q)->ip_len) {
			GETIP(q)->ip_len -= i;
			GETIP(q)->ip_off += i;
			m_adj(q, i);
			q->m_pkthdr.csum_flags = 0;
			break;
		}
		nq = q->m_nextpkt;
		m->m_nextpkt = nq;
		ipstat.ips_fragdropped++;
		fp->ipq_nfrags--;
		m_freem(q);
	}

inserted:

#ifdef IPDIVERT
	/*
	 * Transfer firewall instructions to the fragment structure.
	 * Only trust info in the fragment at offset 0.
	 */
	if (ip->ip_off == 0) {
		fp->ipq_div_info = *divinfo;
		fp->ipq_div_cookie = *divert_rule;
	}
	*divinfo = 0;
	*divert_rule = 0;
#endif

	/*
	 * Check for complete reassembly and perform frag per packet
	 * limiting.
	 *
	 * Frag limiting is performed here so that the nth frag has
	 * a chance to complete the packet before we drop the packet.
	 * As a result, n+1 frags are actually allowed per packet, but
	 * only n will ever be stored. (n = maxfragsperpacket.)
	 *
	 */
	next = 0;
	for (p = NULL, q = fp->ipq_frags; q; p = q, q = q->m_nextpkt) {
		if (GETIP(q)->ip_off != next) {
			if (fp->ipq_nfrags > maxfragsperpacket) {
				ipstat.ips_fragdropped += fp->ipq_nfrags;
				ip_freef(fp);
			}
			return (0);
		}
		next += GETIP(q)->ip_len;
	}
	/* Make sure the last packet didn't have the IP_MF flag */
	if (p->m_flags & M_FRAG) {
		if (fp->ipq_nfrags > maxfragsperpacket) {
			ipstat.ips_fragdropped += fp->ipq_nfrags;
			ip_freef(fp);
		}
		return (0);
	}

	/*
	 * Reassembly is complete.  Make sure the packet is a sane size.
	 */
	q = fp->ipq_frags;
	ip = GETIP(q);
	if (next + (IP_VHL_HL(ip->ip_vhl) << 2) > IP_MAXPACKET) {
		ipstat.ips_toolong++;
		ipstat.ips_fragdropped += fp->ipq_nfrags;
		ip_freef(fp);
		return (0);
	}

	/*
	 * Concatenate fragments.
	 */
	m = q;
	t = m->m_next;
	m->m_next = 0;
	m_cat(m, t);
	nq = q->m_nextpkt;
	q->m_nextpkt = 0;
	for (q = nq; q != NULL; q = nq) {
		nq = q->m_nextpkt;
		q->m_nextpkt = NULL;
		m->m_pkthdr.csum_flags &= q->m_pkthdr.csum_flags;
		m->m_pkthdr.csum_data += q->m_pkthdr.csum_data;
		m_cat(m, q);
	}

#ifdef IPDIVERT
	/*
	 * Extract firewall instructions from the fragment structure.
	 */
	*divinfo = fp->ipq_div_info;
	*divert_rule = fp->ipq_div_cookie;
#endif

	/*
	 * Create header for new ip packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip->ip_len = next;
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;
	remque(fp);
	nipq--;
	(void) m_free(dtom(fp));
	m->m_len += (IP_VHL_HL(ip->ip_vhl) << 2);
	m->m_data -= (IP_VHL_HL(ip->ip_vhl) << 2);
	/* some debugging cruft by sklower, below, will go away soon */
	if (m->m_flags & M_PKTHDR) { /* XXX this should be done elsewhere */
		register int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}
	return (m);

dropfrag:
#ifdef IPDIVERT
	*divinfo = 0;
	*divert_rule = 0;
#endif
	ipstat.ips_fragdropped++;
	if (fp != 0)
		fp->ipq_nfrags--;
	m_freem(m);
	return (0);

#undef GETIP
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
static void
ip_freef(fp)
	struct ipq *fp;
{
	register struct mbuf *q;

	while (fp->ipq_frags) {
		q = fp->ipq_frags;
		fp->ipq_frags = q->m_nextpkt;
		m_freem(q);
	}
	remque(fp);
	(void) m_free(dtom(fp));
	nipq--;
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
	int i;

	for (i = 0; i < IPREASS_NHASH; i++) {
		fp = ipq[i].next;
		if (fp == 0)
			continue;
		while (fp != &ipq[i]) {
			--fp->ipq_ttl;
			fp = fp->next;
			if (fp->prev->ipq_ttl == 0) {
				ipstat.ips_fragtimeout += fp->prev->ipq_nfrags;
				ip_freef(fp->prev);
			}
		}
	}
	/*
	 * If we are over the maximum number of fragments
	 * (due to the limit being lowered), drain off
	 * enough to get down to the new limit.
	 */
	if (maxnipq >= 0 && nipq > maxnipq) {
		for (i = 0; i < IPREASS_NHASH; i++) {
			while (nipq > maxnipq &&
				(ipq[i].next != &ipq[i])) {
				ipstat.ips_fragdropped +=
				    ipq[i].next->ipq_nfrags;
				ip_freef(ipq[i].next);
			}
		}
	}
	ipflow_slowtimo();
	splx(s);
}

/*
 * Drain off all datagram fragments.
 */
void
ip_drain()
{
	int     i;

	for (i = 0; i < IPREASS_NHASH; i++) {
		while (ipq[i].next != &ipq[i]) {
			ipstat.ips_fragdropped += ipq[i].next->ipq_nfrags;
			ip_freef(ipq[i].next);
		}
	}
	in_rtqdrain();
}

/*
 * Do option processing on a datagram,
 * possibly discarding it if bad options are encountered,
 * or forwarding it if source-routed.
 * The pass argument is used when operating in the IPSTEALTH
 * mode to tell what options to process:
 * [LS]SRR (pass 0) or the others (pass 1).
 * The reason for as many as two passes is that when doing IPSTEALTH,
 * non-routing options should be processed only if the packet is for us.
 * Returns 1 if packet has been forwarded/freed,
 * 0 if the packet should be processed further.
 */
static int
ip_dooptions(struct mbuf *m, int pass, struct sockaddr_in *next_hop)
{
	struct ip *ip = mtod(m, struct ip *);
	u_char *cp;
	struct in_ifaddr *ia;
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
			if (cnt < IPOPT_OLEN + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt) {
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
#ifdef IPSTEALTH
			if (ipstealth && pass > 0)
				break;
#endif
			if (optlen < IPOPT_OFFSET + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
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
				if (!ip_dosourceroute)
					goto nosourcerouting;
				/*
				 * Loose routing, and not at next destination
				 * yet; nothing to do except forward.
				 */
				break;
			}
			off--;			/* 0 origin */
			if (off > optlen - (int)sizeof(struct in_addr)) {
				/*
				 * End of source route.  Should be for us.
				 */
				if (!ip_acceptsourceroute)
					goto nosourcerouting;
				save_rte(cp, ip->ip_src);
				break;
			}
#ifdef IPSTEALTH
			if (ipstealth)
				goto dropit;
#endif
			if (!ip_dosourceroute) {
				if (ipforwarding) {
					char buf[16]; /* aaa.bbb.ccc.ddd\0 */
					/*
					 * Acting as a router, so generate ICMP
					 */
nosourcerouting:
					strcpy(buf, inet_ntoa(ip->ip_dst));
					log(LOG_WARNING, 
					    "attempted source route from %s to %s\n",
					    inet_ntoa(ip->ip_src), buf);
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				} else {
					/*
					 * Not acting as a router, so silently drop.
					 */
#ifdef IPSTEALTH
dropit:
#endif
					ipstat.ips_cantforward++;
					m_freem(m);
					return (1);
				}
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
				ia = ip_rtaddr(ipaddr.sin_addr, &ipforward_rt);
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
#ifdef IPSTEALTH
			if (ipstealth && pass == 0)
				break;
#endif
			if (optlen < IPOPT_OFFSET + sizeof(*cp)) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			/*
			 * If no space remains, ignore.
			 */
			off--;			/* 0 origin */
			if (off > optlen - (int)sizeof(struct in_addr))
				break;
			(void)memcpy(&ipaddr.sin_addr, &ip->ip_dst,
			    sizeof(ipaddr.sin_addr));
			/*
			 * locate outgoing interface; if we're the destination,
			 * use the incoming interface (should be same).
			 */
			if ((ia = (INA)ifa_ifwithaddr((SA)&ipaddr)) == 0 &&
			    (ia = ip_rtaddr(ipaddr.sin_addr,
			    &ipforward_rt)) == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_HOST;
				goto bad;
			}
			(void)memcpy(cp + off, &(IA_SIN(ia)->sin_addr),
			    sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;

		case IPOPT_TS:
#ifdef IPSTEALTH
			if (ipstealth && pass == 0)
				break;
#endif
			code = cp - (u_char *)ip;
			if (optlen < 4 || optlen > 40) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < 5) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			if (off > optlen - (int)sizeof(int32_t)) {
				cp[IPOPT_OFFSET + 1] += (1 << 4);
				if ((cp[IPOPT_OFFSET + 1] & 0xf0) == 0) {
					code = &cp[IPOPT_OFFSET] - (u_char *)ip;
					goto bad;
				}
				break;
			}
			off--;				/* 0 origin */
			sin = (struct in_addr *)(cp + off);
			switch (cp[IPOPT_OFFSET + 1] & 0x0f) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (off + sizeof(n_time) +
				    sizeof(struct in_addr) > optlen) {
					code = &cp[IPOPT_OFFSET] - (u_char *)ip;
					goto bad;
				}
				ipaddr.sin_addr = dst;
				ia = (INA)ifaof_ifpforaddr((SA)&ipaddr,
							    m->m_pkthdr.rcvif);
				if (ia == 0)
					continue;
				(void)memcpy(sin, &IA_SIN(ia)->sin_addr,
				    sizeof(struct in_addr));
				cp[IPOPT_OFFSET] += sizeof(struct in_addr);
				off += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (off + sizeof(n_time) +
				    sizeof(struct in_addr) > optlen) {
					code = &cp[IPOPT_OFFSET] - (u_char *)ip;
					goto bad;
				}
				(void)memcpy(&ipaddr.sin_addr, sin,
				    sizeof(struct in_addr));
				if (ifa_ifwithaddr((SA)&ipaddr) == 0)
					continue;
				cp[IPOPT_OFFSET] += sizeof(struct in_addr);
				off += sizeof(struct in_addr);
				break;

			default:
				code = &cp[IPOPT_OFFSET + 1] - (u_char *)ip;
				goto bad;
			}
			ntime = iptime();
			(void)memcpy(cp + off, &ntime, sizeof(n_time));
			cp[IPOPT_OFFSET] += sizeof(n_time);
		}
	}
	if (forward && ipforwarding) {
		ip_forward(m, 1, next_hop);
		return (1);
	}
	return (0);
bad:
	icmp_error(m, type, code, 0, 0);
	ipstat.ips_badoptions++;
	return (1);
}

/*
 * Given address of next destination (final or next hop),
 * return internet address info of interface to be used to get there.
 */
struct in_ifaddr *
ip_rtaddr(dst, rt)
	struct in_addr dst;
	struct route *rt;
{
	register struct sockaddr_in *sin;

	sin = (struct sockaddr_in *)&rt->ro_dst;

	if (rt->ro_rt == 0 ||
	    dst.s_addr != sin->sin_addr.s_addr) {
		if (rt->ro_rt) {
			RTFREE(rt->ro_rt);
			rt->ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = dst;

		rtalloc_ign(rt, RTF_PRCLONING);
	}
	if (rt->ro_rt == 0)
		return ((struct in_ifaddr *)0);
	return (ifatoia(rt->ro_rt->rt_ifa));
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
	m = m_get(M_DONTWAIT, MT_HEADER);
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
		printf(" hops %lx", (u_long)ntohl(mtod(m, struct in_addr *)->s_addr));
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
			printf(" %lx", (u_long)ntohl(q->s_addr));
#endif
		*q++ = *p--;
	}
	/*
	 * Last hop goes to final destination.
	 */
	*q = ip_srcrt.dst;
#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf(" %lx\n", (u_long)ntohl(q->s_addr));
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
	0,		0,		EHOSTUNREACH,	0,
	ENOPROTOOPT,	ECONNREFUSED
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
ip_forward(struct mbuf *m, int srcrt, struct sockaddr_in *next_hop)
{
	struct ip *ip = mtod(m, struct ip *);
	struct sockaddr_in *sin;
	struct rtentry *rt;
	int error, type = 0, code = 0;
	struct mbuf *mcopy;
	n_long dest;
	struct in_addr pkt_dst;
	struct ifnet *destifp;
#if defined(IPSEC) || defined(FAST_IPSEC)
	struct ifnet dummyifp;
#endif

	dest = 0;
	/*
	 * Cache the destination address of the packet; this may be
	 * changed by use of 'ipfw fwd'.
	 */
	pkt_dst = next_hop ? next_hop->sin_addr : ip->ip_dst;

#ifdef DIAGNOSTIC
	if (ipprintfs)
		printf("forward: src %lx dst %lx ttl %x\n",
		    (u_long)ip->ip_src.s_addr, (u_long)pkt_dst.s_addr,
		    ip->ip_ttl);
#endif


	if (m->m_flags & (M_BCAST|M_MCAST) || in_canforward(pkt_dst) == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
		return;
	}
#ifdef IPSTEALTH
	if (!ipstealth) {
#endif
		if (ip->ip_ttl <= IPTTLDEC) {
			icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS,
			    dest, 0);
			return;
		}
#ifdef IPSTEALTH
	}
#endif

	sin = (struct sockaddr_in *)&ipforward_rt.ro_dst;
	if ((rt = ipforward_rt.ro_rt) == 0 ||
	    pkt_dst.s_addr != sin->sin_addr.s_addr) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = pkt_dst;

		rtalloc_ign(&ipforward_rt, RTF_PRCLONING);
		if (ipforward_rt.ro_rt == 0) {
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, dest, 0);
			return;
		}
		rt = ipforward_rt.ro_rt;
	}

	/*
	 * Save the IP header and at most 8 bytes of the payload,
	 * in case we need to generate an ICMP message to the src.
	 *
	 * XXX this can be optimized a lot by saving the data in a local
	 * buffer on the stack (72 bytes at most), and only allocating the
	 * mbuf if really necessary. The vast majority of the packets
	 * are forwarded without having to send an ICMP back (either
	 * because unnecessary, or because rate limited), so we are
	 * really we are wasting a lot of work here.
	 *
	 * We don't use m_copy() because it might return a reference
	 * to a shared cluster. Both this function and ip_output()
	 * assume exclusive access to the IP header in `m', so any
	 * data in a cluster may change before we reach icmp_error().
	 */
	MGET(mcopy, M_DONTWAIT, m->m_type);
	if (mcopy != NULL && !m_dup_pkthdr(mcopy, m, M_DONTWAIT)) {
		/*
		 * It's probably ok if the pkthdr dup fails (because
		 * the deep copy of the tag chain failed), but for now
		 * be conservative and just discard the copy since
		 * code below may some day want the tags.
		 */
		m_free(mcopy);
		mcopy = NULL;
	}
	if (mcopy != NULL) {
		mcopy->m_len = imin((IP_VHL_HL(ip->ip_vhl) << 2) + 8,
		    (int)ip->ip_len);
		m_copydata(m, 0, mcopy->m_len, mtod(mcopy, caddr_t));
	}

#ifdef IPSTEALTH
	if (!ipstealth) {
#endif
		ip->ip_ttl -= IPTTLDEC;
#ifdef IPSTEALTH
	}
#endif

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 */
	if (rt->rt_ifp == m->m_pkthdr.rcvif &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
	    satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
	    ipsendredirects && !srcrt && !next_hop) {
#define	RTA(rt)	((struct in_ifaddr *)(rt->rt_ifa))
		u_long src = ntohl(ip->ip_src.s_addr);

		if (RTA(rt) &&
		    (src & RTA(rt)->ia_subnetmask) == RTA(rt)->ia_subnet) {
		    if (rt->rt_flags & RTF_GATEWAY)
			dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
		    else
			dest = pkt_dst.s_addr;
		    /* Router requirements says to only send host redirects */
		    type = ICMP_REDIRECT;
		    code = ICMP_REDIRECT_HOST;
#ifdef DIAGNOSTIC
		    if (ipprintfs)
		        printf("redirect (%d) to %lx\n", code, (u_long)dest);
#endif
		}
	}

    {
	struct m_hdr tag;

	if (next_hop) {
		/* Pass IPFORWARD info if available */
 
		tag.mh_type = MT_TAG;
		tag.mh_flags = PACKET_TAG_IPFORWARD;
		tag.mh_data = (caddr_t)next_hop;
		tag.mh_next = m;
		m = (struct mbuf *)&tag;
	}
	error = ip_output(m, (struct mbuf *)0, &ipforward_rt, 
			  IP_FORWARDING, 0, NULL);
    }
	if (error)
		ipstat.ips_cantforward++;
	else {
		ipstat.ips_forward++;
		if (type)
			ipstat.ips_redirectsent++;
		else {
			if (mcopy) {
				ipflow_create(&ipforward_rt, mcopy);
				m_freem(mcopy);
			}
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
#ifdef IPSEC
		/*
		 * If the packet is routed over IPsec tunnel, tell the
		 * originator the tunnel MTU.
		 *	tunnel MTU = if MTU - sizeof(IP) - ESP/AH hdrsiz
		 * XXX quickhack!!!
		 */
		if (ipforward_rt.ro_rt) {
			struct secpolicy *sp = NULL;
			int ipsecerror;
			int ipsechdr;
			struct route *ro;

			sp = ipsec4_getpolicybyaddr(mcopy,
						    IPSEC_DIR_OUTBOUND,
			                            IP_FORWARDING,
			                            &ipsecerror);

			if (sp == NULL)
				destifp = ipforward_rt.ro_rt->rt_ifp;
			else {
				/* count IPsec header size */
				ipsechdr = ipsec4_hdrsiz(mcopy,
							 IPSEC_DIR_OUTBOUND,
							 NULL);

				/*
				 * find the correct route for outer IPv4
				 * header, compute tunnel MTU.
				 *
				 * XXX BUG ALERT
				 * The "dummyifp" code relies upon the fact
				 * that icmp_error() touches only ifp->if_mtu.
				 */
				/*XXX*/
				destifp = NULL;
				if (sp->req != NULL
				 && sp->req->sav != NULL
				 && sp->req->sav->sah != NULL) {
					ro = &sp->req->sav->sah->sa_route;
					if (ro->ro_rt && ro->ro_rt->rt_ifp) {
						dummyifp.if_mtu =
						    ro->ro_rt->rt_ifp->if_mtu;
						dummyifp.if_mtu -= ipsechdr;
						destifp = &dummyifp;
					}
				}

				key_freesp(sp);
			}
		}
#elif FAST_IPSEC
		/*
		 * If the packet is routed over IPsec tunnel, tell the
		 * originator the tunnel MTU.
		 *	tunnel MTU = if MTU - sizeof(IP) - ESP/AH hdrsiz
		 * XXX quickhack!!!
		 */
		if (ipforward_rt.ro_rt) {
			struct secpolicy *sp = NULL;
			int ipsecerror;
			int ipsechdr;
			struct route *ro;

			sp = ipsec_getpolicybyaddr(mcopy,
						   IPSEC_DIR_OUTBOUND,
			                           IP_FORWARDING,
			                           &ipsecerror);

			if (sp == NULL)
				destifp = ipforward_rt.ro_rt->rt_ifp;
			else {
				/* count IPsec header size */
				ipsechdr = ipsec4_hdrsiz(mcopy,
							 IPSEC_DIR_OUTBOUND,
							 NULL);

				/*
				 * find the correct route for outer IPv4
				 * header, compute tunnel MTU.
				 *
				 * XXX BUG ALERT
				 * The "dummyifp" code relies upon the fact
				 * that icmp_error() touches only ifp->if_mtu.
				 */
				/*XXX*/
				destifp = NULL;
				if (sp->req != NULL
				 && sp->req->sav != NULL
				 && sp->req->sav->sah != NULL) {
					ro = &sp->req->sav->sah->sa_route;
					if (ro->ro_rt && ro->ro_rt->rt_ifp) {
						dummyifp.if_mtu =
						    ro->ro_rt->rt_ifp->if_mtu;
						dummyifp.if_mtu -= ipsechdr;
						destifp = &dummyifp;
					}
				}

				KEY_FREESP(&sp);
			}
		}
#else /* !IPSEC && !FAST_IPSEC */
		if (ipforward_rt.ro_rt)
			destifp = ipforward_rt.ro_rt->rt_ifp;
#endif /*IPSEC*/
		ipstat.ips_cantfrag++;
		break;

	case ENOBUFS:
		/*
		 * A router should not generate ICMP_SOURCEQUENCH as
		 * required in RFC1812 Requirements for IP Version 4 Routers.
		 * Source quench could be a big problem under DoS attacks,
		 * or if the underlying interface is rate-limited.
		 * Those who need source quench packets may re-enable them
		 * via the net.inet.ip.sendsourcequench sysctl.
		 */
		if (ip_sendsourcequench == 0) {
			m_freem(mcopy);
			return;
		} else {
			type = ICMP_SOURCEQUENCH;
			code = 0;
		}
		break;

	case EACCES:			/* ipfw denied packet */
		m_freem(mcopy);
		return;
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
		struct ifnet *ifp;
		struct sdlbuf {
			struct sockaddr_dl sdl;
			u_char	pad[32];
		} sdlbuf;
		struct sockaddr_dl *sdp;
		struct sockaddr_dl *sdl2 = &sdlbuf.sdl;

		if (((ifp = m->m_pkthdr.rcvif)) 
		&& ( ifp->if_index && (ifp->if_index <= if_index))) {
			sdp = (struct sockaddr_dl *)(ifnet_addrs
					[ifp->if_index - 1]->ifa_addr);
			/*
			 * Change our mind and don't try copy.
			 */
			if ((sdp->sdl_family != AF_LINK)
			|| (sdp->sdl_len > sizeof(sdlbuf))) {
				goto makedummy;
			}
			bcopy(sdp, sdl2, sdp->sdl_len);
		} else {
makedummy:	
			sdl2->sdl_len
				= offsetof(struct sockaddr_dl, sdl_data[0]);
			sdl2->sdl_family = AF_LINK;
			sdl2->sdl_index = 0;
			sdl2->sdl_nlen = sdl2->sdl_alen = sdl2->sdl_slen = 0;
		}
		*mp = sbcreatecontrol((caddr_t) sdl2, sdl2->sdl_len,
			IP_RECVIF, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
}

/*
 * XXX these routines are called from the upper part of the kernel.
 *
 * They could also be moved to ip_mroute.c, since all the RSVP
 *  handling is done there already.
 */
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

void
rsvp_input(struct mbuf *m, int off, int proto)	/* XXX must fixup manually */
{
	if (rsvp_input_p) { /* call the real one if loaded */
		rsvp_input_p(m, off, proto);
		return;
	}

	/* Can still get packets with rsvp_on = 0 if there is a local member
	 * of the group to which the RSVP packet is addressed.  But in this
	 * case we want to throw the packet away.
	 */

	if (!rsvp_on) {
		m_freem(m);
		return;
	}

	if (ip_rsvpd != NULL) { 
		rip_input(m, off, proto);
		return;
	}
	/* Drop the packet */
	m_freem(m);
}

