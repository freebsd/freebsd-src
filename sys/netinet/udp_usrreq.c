/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2008 Robert N. M. Watson
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * Copyright (c) 2014 Kevin Lo
 * All rights reserved.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_route.h"
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/rss_config.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udplite.h>
#include <netinet/in_rss.h>

#include <netipsec/ipsec_support.h>

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

/*
 * UDP and UDP-Lite protocols implementation.
 * Per RFC 768, August, 1980.
 * Per RFC 3828, July, 2004.
 */

VNET_DEFINE(int, udp_bind_all_fibs) = 1;
SYSCTL_INT(_net_inet_udp, OID_AUTO, bind_all_fibs, CTLFLAG_VNET | CTLFLAG_RDTUN,
    &VNET_NAME(udp_bind_all_fibs), 0,
    "Bound sockets receive traffic from all FIBs");

/*
 * BSD 4.2 defaulted the udp checksum to be off.  Turning off udp checksums
 * removes the only data integrity mechanism for packets and malformed
 * packets that would otherwise be discarded due to bad checksums, and may
 * cause problems (especially for NFS data blocks).
 */
VNET_DEFINE(int, udp_cksum) = 1;
SYSCTL_INT(_net_inet_udp, UDPCTL_CHECKSUM, checksum, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(udp_cksum), 0, "compute udp checksum");

VNET_DEFINE(int, udp_log_in_vain) = 0;
SYSCTL_INT(_net_inet_udp, OID_AUTO, log_in_vain, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(udp_log_in_vain), 0, "Log all incoming UDP packets");

VNET_DEFINE(int, udp_blackhole) = 0;
SYSCTL_INT(_net_inet_udp, OID_AUTO, blackhole, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(udp_blackhole), 0,
    "Do not send port unreachables for refused connects");
VNET_DEFINE(bool, udp_blackhole_local) = false;
SYSCTL_BOOL(_net_inet_udp, OID_AUTO, blackhole_local, CTLFLAG_VNET |
    CTLFLAG_RW, &VNET_NAME(udp_blackhole_local), false,
    "Enforce net.inet.udp.blackhole for locally originated packets");

u_long	udp_sendspace = 9216;		/* really max datagram size */
SYSCTL_ULONG(_net_inet_udp, UDPCTL_MAXDGRAM, maxdgram, CTLFLAG_RW,
    &udp_sendspace, 0, "Maximum outgoing UDP datagram size");

u_long	udp_recvspace = 40 * (1024 +
#ifdef INET6
				      sizeof(struct sockaddr_in6)
#else
				      sizeof(struct sockaddr_in)
#endif
				      );	/* 40 1K datagrams */

SYSCTL_ULONG(_net_inet_udp, UDPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
    &udp_recvspace, 0, "Maximum space for incoming UDP datagrams");

VNET_DEFINE(struct inpcbinfo, udbinfo);
VNET_DEFINE(struct inpcbinfo, ulitecbinfo);

#ifndef UDBHASHSIZE
#define	UDBHASHSIZE	128
#endif

VNET_PCPUSTAT_DEFINE(struct udpstat, udpstat);		/* from udp_var.h */
VNET_PCPUSTAT_SYSINIT(udpstat);
SYSCTL_VNET_PCPUSTAT(_net_inet_udp, UDPCTL_STATS, stats, struct udpstat,
    udpstat, "UDP statistics (struct udpstat, netinet/udp_var.h)");

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(udpstat);
#endif /* VIMAGE */
#ifdef INET
static void	udp_detach(struct socket *so);
#endif

INPCBSTORAGE_DEFINE(udpcbstor, udpcb, "udpinp", "udp_inpcb", "udp", "udphash");
INPCBSTORAGE_DEFINE(udplitecbstor, udpcb, "udpliteinp", "udplite_inpcb",
    "udplite", "udplitehash");

static void
udp_vnet_init(void *arg __unused)
{

	/*
	 * For now default to 2-tuple UDP hashing - until the fragment
	 * reassembly code can also update the flowid.
	 *
	 * Once we can calculate the flowid that way and re-establish
	 * a 4-tuple, flip this to 4-tuple.
	 */
	in_pcbinfo_init(&V_udbinfo, &udpcbstor, UDBHASHSIZE, UDBHASHSIZE);
	/* Additional pcbinfo for UDP-Lite */
	in_pcbinfo_init(&V_ulitecbinfo, &udplitecbstor, UDBHASHSIZE,
	    UDBHASHSIZE);
}
VNET_SYSINIT(udp_vnet_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH,
    udp_vnet_init, NULL);

/*
 * Kernel module interface for updating udpstat.  The argument is an index
 * into udpstat treated as an array of u_long.  While this encodes the
 * general layout of udpstat into the caller, it doesn't encode its location,
 * so that future changes to add, for example, per-CPU stats support won't
 * cause binary compatibility problems for kernel modules.
 */
void
kmod_udpstat_inc(int statnum)
{

	counter_u64_add(VNET(udpstat)[statnum], 1);
}

#ifdef VIMAGE
static void
udp_destroy(void *unused __unused)
{

	in_pcbinfo_destroy(&V_udbinfo);
	in_pcbinfo_destroy(&V_ulitecbinfo);
}
VNET_SYSUNINIT(udp, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH, udp_destroy, NULL);
#endif

#ifdef INET
/*
 * Subroutine of udp_input(), which appends the provided mbuf chain to the
 * passed pcb/socket.  The caller must provide a sockaddr_in via udp_in that
 * contains the source address.  If the socket ends up being an IPv6 socket,
 * udp_append() will convert to a sockaddr_in6 before passing the address
 * into the socket code.
 *
 * In the normal case udp_append() will return 0, indicating that you
 * must unlock the inp. However if a tunneling protocol is in place we increment
 * the inpcb refcnt and unlock the inp, on return from the tunneling protocol we
 * then decrement the reference count. If the inp_rele returns 1, indicating the
 * inp is gone, we return that to the caller to tell them *not* to unlock
 * the inp. In the case of multi-cast this will cause the distribution
 * to stop (though most tunneling protocols known currently do *not* use
 * multicast).
 */
static int
udp_append(struct inpcb *inp, struct ip *ip, struct mbuf *n, int off,
    struct sockaddr_in *udp_in)
{
	struct sockaddr *append_sa;
	struct socket *so;
	struct mbuf *tmpopts, *opts = NULL;
#ifdef INET6
	struct sockaddr_in6 udp_in6;
#endif
	struct udpcb *up;
	bool filtered;

	INP_LOCK_ASSERT(inp);

	/*
	 * Engage the tunneling protocol.
	 */
	up = intoudpcb(inp);
	if (up->u_tun_func != NULL) {
		in_pcbref(inp);
		INP_RUNLOCK(inp);
		filtered = (*up->u_tun_func)(n, off, inp, (struct sockaddr *)&udp_in[0],
		    up->u_tun_ctx);
		INP_RLOCK(inp);
		if (filtered)
			return (in_pcbrele_rlocked(inp));
	}

	off += sizeof(struct udphdr);

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/* Check AH/ESP integrity. */
	if (IPSEC_ENABLED(ipv4) &&
	    IPSEC_CHECK_POLICY(ipv4, n, inp) != 0) {
		m_freem(n);
		return (0);
	}
	if (up->u_flags & UF_ESPINUDP) {/* IPSec UDP encaps. */
		if (IPSEC_ENABLED(ipv4) &&
		    UDPENCAP_INPUT(ipv4, n, off, AF_INET) != 0)
			return (0);	/* Consumed. */
	}
#endif /* IPSEC */
#ifdef MAC
	if (mac_inpcb_check_deliver(inp, n) != 0) {
		m_freem(n);
		return (0);
	}
#endif /* MAC */
	if (inp->inp_flags & INP_CONTROLOPTS ||
	    inp->inp_socket->so_options & (SO_TIMESTAMP | SO_BINTIME)) {
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6)
			(void)ip6_savecontrol_v4(inp, n, &opts, NULL);
		else
#endif /* INET6 */
			ip_savecontrol(inp, &opts, ip, n);
	}
	if ((inp->inp_vflag & INP_IPV4) && (inp->inp_flags2 & INP_ORIGDSTADDR)) {
		tmpopts = sbcreatecontrol(&udp_in[1],
		    sizeof(struct sockaddr_in), IP_ORIGDSTADDR, IPPROTO_IP,
		    M_NOWAIT);
		if (tmpopts) {
			if (opts) {
				tmpopts->m_next = opts;
				opts = tmpopts;
			} else
				opts = tmpopts;
		}
	}
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		bzero(&udp_in6, sizeof(udp_in6));
		udp_in6.sin6_len = sizeof(udp_in6);
		udp_in6.sin6_family = AF_INET6;
		in6_sin_2_v4mapsin6(&udp_in[0], &udp_in6);
		append_sa = (struct sockaddr *)&udp_in6;
	} else
#endif /* INET6 */
		append_sa = (struct sockaddr *)&udp_in[0];
	m_adj(n, off);

	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_rcv);
	if (sbappendaddr_locked(&so->so_rcv, append_sa, n, opts) == 0) {
		soroverflow_locked(so);
		m_freem(n);
		if (opts)
			m_freem(opts);
		UDPSTAT_INC(udps_fullsock);
	} else
		sorwakeup_locked(so);
	return (0);
}

static bool
udp_multi_match(const struct inpcb *inp, void *v)
{
	struct ip *ip = v;
	struct udphdr *uh = (struct udphdr *)(ip + 1);

	if (inp->inp_lport != uh->uh_dport)
		return (false);
#ifdef INET6
	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (false);
#endif
	if (inp->inp_laddr.s_addr != INADDR_ANY &&
	    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
		return (false);
	if (inp->inp_faddr.s_addr != INADDR_ANY &&
	    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
		return (false);
	if (inp->inp_fport != 0 &&
	    inp->inp_fport != uh->uh_sport)
		return (false);

	return (true);
}

static int
udp_multi_input(struct mbuf *m, int proto, struct sockaddr_in *udp_in)
{
	struct ip *ip = mtod(m, struct ip *);
	struct inpcb_iterator inpi = INP_ITERATOR(udp_get_inpcbinfo(proto),
	    INPLOOKUP_RLOCKPCB, udp_multi_match, ip);
#ifdef KDTRACE_HOOKS
	struct udphdr *uh = (struct udphdr *)(ip + 1);
#endif
	struct inpcb *inp;
	struct mbuf *n;
	int appends = 0, fib;

	MPASS(ip->ip_hl == sizeof(struct ip) >> 2);

	fib = M_GETFIB(m);

	while ((inp = inp_next(&inpi)) != NULL) {
		/*
		 * XXXRW: Because we weren't holding either the inpcb
		 * or the hash lock when we checked for a match
		 * before, we should probably recheck now that the
		 * inpcb lock is held.
		 */

		if (V_udp_bind_all_fibs == 0 && fib != inp->inp_inc.inc_fibnum)
			/*
			 * Sockets bound to a specific FIB can only receive
			 * packets from that FIB.
			 */
			continue;

		/*
		 * Handle socket delivery policy for any-source
		 * and source-specific multicast. [RFC3678]
		 */
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
			struct ip_moptions	*imo;
			struct sockaddr_in	 group;
			int			 blocked;

			imo = inp->inp_moptions;
			if (imo == NULL)
				continue;
			bzero(&group, sizeof(struct sockaddr_in));
			group.sin_len = sizeof(struct sockaddr_in);
			group.sin_family = AF_INET;
			group.sin_addr = ip->ip_dst;

			blocked = imo_multi_filter(imo, m->m_pkthdr.rcvif,
				(struct sockaddr *)&group,
				(struct sockaddr *)&udp_in[0]);
			if (blocked != MCAST_PASS) {
				if (blocked == MCAST_NOTGMEMBER)
					IPSTAT_INC(ips_notmember);
				if (blocked == MCAST_NOTSMEMBER ||
				    blocked == MCAST_MUTED)
					UDPSTAT_INC(udps_filtermcast);
				continue;
			}
		}
		if ((n = m_copym(m, 0, M_COPYALL, M_NOWAIT)) != NULL) {
			if (proto == IPPROTO_UDPLITE)
				UDPLITE_PROBE(receive, NULL, inp, ip, inp, uh);
			else
				UDP_PROBE(receive, NULL, inp, ip, inp, uh);
			if (udp_append(inp, ip, n, sizeof(struct ip), udp_in)) {
				break;
			} else
				appends++;
		}
		/*
		 * Don't look for additional matches if this one does
		 * not have either the SO_REUSEPORT or SO_REUSEADDR
		 * socket options set.  This heuristic avoids
		 * searching through all pcbs in the common case of a
		 * non-shared port.  It assumes that an application
		 * will never clear these options after setting them.
		 */
		if ((inp->inp_socket->so_options &
		    (SO_REUSEPORT|SO_REUSEPORT_LB|SO_REUSEADDR)) == 0) {
			INP_RUNLOCK(inp);
			break;
		}
	}

	if (appends == 0) {
		/*
		 * No matching pcb found; discard datagram.  (No need
		 * to send an ICMP Port Unreachable for a broadcast
		 * or multicast datgram.)
		 */
		UDPSTAT_INC(udps_noport);
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)))
			UDPSTAT_INC(udps_noportmcast);
		else
			UDPSTAT_INC(udps_noportbcast);
	}
	m_freem(m);

	return (IPPROTO_DONE);
}

static int
udp_input(struct mbuf **mp, int *offp, int proto)
{
	struct ip *ip;
	struct udphdr *uh;
	struct ifnet *ifp;
	struct inpcb *inp;
	uint16_t len, ip_len;
	struct inpcbinfo *pcbinfo;
	struct sockaddr_in udp_in[2];
	struct mbuf *m;
	struct m_tag *fwd_tag;
	int cscov_partial, iphlen, lookupflags;

	m = *mp;
	iphlen = *offp;
	ifp = m->m_pkthdr.rcvif;
	*mp = NULL;
	UDPSTAT_INC(udps_ipackets);

	/*
	 * Strip IP options, if any; should skip this, make available to
	 * user, and use on returned packets, but we don't yet have a way to
	 * check the checksum with options still present.
	 */
	if (iphlen > sizeof (struct ip)) {
		ip_stripoptions(m);
		iphlen = sizeof(struct ip);
	}

	/*
	 * Get IP and UDP header together in first mbuf.
	 */
	if (m->m_len < iphlen + sizeof(struct udphdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct udphdr))) == NULL) {
			UDPSTAT_INC(udps_hdrops);
			return (IPPROTO_DONE);
		}
	}
	ip = mtod(m, struct ip *);
	uh = (struct udphdr *)((caddr_t)ip + iphlen);
	cscov_partial = (proto == IPPROTO_UDPLITE) ? 1 : 0;

	/*
	 * Destination port of 0 is illegal, based on RFC768.
	 */
	if (uh->uh_dport == 0)
		goto badunlocked;

	/*
	 * Construct sockaddr format source address.  Stuff source address
	 * and datagram in user buffer.
	 */
	bzero(&udp_in[0], sizeof(struct sockaddr_in) * 2);
	udp_in[0].sin_len = sizeof(struct sockaddr_in);
	udp_in[0].sin_family = AF_INET;
	udp_in[0].sin_port = uh->uh_sport;
	udp_in[0].sin_addr = ip->ip_src;
	udp_in[1].sin_len = sizeof(struct sockaddr_in);
	udp_in[1].sin_family = AF_INET;
	udp_in[1].sin_port = uh->uh_dport;
	udp_in[1].sin_addr = ip->ip_dst;

	/*
	 * Make mbuf data length reflect UDP length.  If not enough data to
	 * reflect UDP length, drop.
	 */
	len = ntohs((u_short)uh->uh_ulen);
	ip_len = ntohs(ip->ip_len) - iphlen;
	if (proto == IPPROTO_UDPLITE && (len == 0 || len == ip_len)) {
		/* Zero means checksum over the complete packet. */
		if (len == 0)
			len = ip_len;
		cscov_partial = 0;
	}
	if (ip_len != len) {
		if (len > ip_len || len < sizeof(struct udphdr)) {
			UDPSTAT_INC(udps_badlen);
			goto badunlocked;
		}
		if (proto == IPPROTO_UDP)
			m_adj(m, len - ip_len);
	}

	/*
	 * Checksum extended UDP header and data.
	 */
	if (uh->uh_sum) {
		u_short uh_sum;

		if ((m->m_pkthdr.csum_flags & CSUM_DATA_VALID) &&
		    !cscov_partial) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				uh_sum = m->m_pkthdr.csum_data;
			else
				uh_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr, htonl((u_short)len +
				    m->m_pkthdr.csum_data + proto));
			uh_sum ^= 0xffff;
		} else {
			char b[offsetof(struct ipovly, ih_src)];
			struct ipovly *ipov = (struct ipovly *)ip;

			memcpy(b, ipov, sizeof(b));
			bzero(ipov, sizeof(ipov->ih_x1));
			ipov->ih_len = (proto == IPPROTO_UDP) ?
			    uh->uh_ulen : htons(ip_len);
			uh_sum = in_cksum(m, len + sizeof (struct ip));
			memcpy(ipov, b, sizeof(b));
		}
		if (uh_sum) {
			UDPSTAT_INC(udps_badsum);
			m_freem(m);
			return (IPPROTO_DONE);
		}
	} else {
		if (proto == IPPROTO_UDP) {
			UDPSTAT_INC(udps_nosum);
		} else {
			/* UDPLite requires a checksum */
			/* XXX: What is the right UDPLite MIB counter here? */
			m_freem(m);
			return (IPPROTO_DONE);
		}
	}

	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
	    in_ifnet_broadcast(ip->ip_dst, ifp))
		return (udp_multi_input(m, proto, udp_in));

	pcbinfo = udp_get_inpcbinfo(proto);

	/*
	 * Locate pcb for datagram.
	 */
	lookupflags = INPLOOKUP_RLOCKPCB |
	    (V_udp_bind_all_fibs ? 0 : INPLOOKUP_FIB);

	/*
	 * Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain.
	 */
	if ((m->m_flags & M_IP_NEXTHOP) &&
	    (fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL)) != NULL) {
		struct sockaddr_in *next_hop;

		next_hop = (struct sockaddr_in *)(fwd_tag + 1);

		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * Already got one like this?
		 */
		inp = in_pcblookup_mbuf(pcbinfo, ip->ip_src, uh->uh_sport,
		    ip->ip_dst, uh->uh_dport, lookupflags, ifp, m);
		if (!inp) {
			/*
			 * It's new.  Try to find the ambushing socket.
			 * Because we've rewritten the destination address,
			 * any hardware-generated hash is ignored.
			 */
			inp = in_pcblookup(pcbinfo, ip->ip_src,
			    uh->uh_sport, next_hop->sin_addr,
			    next_hop->sin_port ? htons(next_hop->sin_port) :
			    uh->uh_dport, INPLOOKUP_WILDCARD | lookupflags,
			    ifp);
		}
		/* Remove the tag from the packet. We don't need it anymore. */
		m_tag_delete(m, fwd_tag);
		m->m_flags &= ~M_IP_NEXTHOP;
	} else
		inp = in_pcblookup_mbuf(pcbinfo, ip->ip_src, uh->uh_sport,
		    ip->ip_dst, uh->uh_dport, INPLOOKUP_WILDCARD |
		    lookupflags, ifp, m);
	if (inp == NULL) {
		if (V_udp_log_in_vain) {
			char src[INET_ADDRSTRLEN];
			char dst[INET_ADDRSTRLEN];

			log(LOG_INFO,
			    "Connection attempt to UDP %s:%d from %s:%d\n",
			    inet_ntoa_r(ip->ip_dst, dst), ntohs(uh->uh_dport),
			    inet_ntoa_r(ip->ip_src, src), ntohs(uh->uh_sport));
		}
		if (proto == IPPROTO_UDPLITE)
			UDPLITE_PROBE(receive, NULL, NULL, ip, NULL, uh);
		else
			UDP_PROBE(receive, NULL, NULL, ip, NULL, uh);
		UDPSTAT_INC(udps_noport);
		if (m->m_flags & (M_BCAST | M_MCAST)) {
			UDPSTAT_INC(udps_noportbcast);
			goto badunlocked;
		}
		if (V_udp_blackhole && (V_udp_blackhole_local ||
		    !in_localip(ip->ip_src)))
			goto badunlocked;
		if (badport_bandlim(BANDLIM_ICMP_UNREACH) < 0)
			goto badunlocked;
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
		return (IPPROTO_DONE);
	}

	/*
	 * Check the minimum TTL for socket.
	 */
	INP_RLOCK_ASSERT(inp);
	if (inp->inp_ip_minttl && inp->inp_ip_minttl > ip->ip_ttl) {
		if (proto == IPPROTO_UDPLITE)
			UDPLITE_PROBE(receive, NULL, inp, ip, inp, uh);
		else
			UDP_PROBE(receive, NULL, inp, ip, inp, uh);
		INP_RUNLOCK(inp);
		m_freem(m);
		return (IPPROTO_DONE);
	}
	if (cscov_partial) {
		struct udpcb *up;

		up = intoudpcb(inp);
		if (up->u_rxcslen == 0 || up->u_rxcslen > len) {
			INP_RUNLOCK(inp);
			m_freem(m);
			return (IPPROTO_DONE);
		}
	}

	if (proto == IPPROTO_UDPLITE)
		UDPLITE_PROBE(receive, NULL, inp, ip, inp, uh);
	else
		UDP_PROBE(receive, NULL, inp, ip, inp, uh);
	if (udp_append(inp, ip, m, iphlen, udp_in) == 0)
		INP_RUNLOCK(inp);
	return (IPPROTO_DONE);

badunlocked:
	m_freem(m);
	return (IPPROTO_DONE);
}
#endif /* INET */

/*
 * Notify a udp user of an asynchronous error; just wake up so that they can
 * collect error status.
 */
struct inpcb *
udp_notify(struct inpcb *inp, int errno)
{

	INP_WLOCK_ASSERT(inp);
	if ((errno == EHOSTUNREACH || errno == ENETUNREACH ||
	     errno == EHOSTDOWN) && inp->inp_route.ro_nh) {
		NH_FREE(inp->inp_route.ro_nh);
		inp->inp_route.ro_nh = (struct nhop_object *)NULL;
	}

	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
	return (inp);
}

#ifdef INET
static void
udp_common_ctlinput(struct icmp *icmp, struct inpcbinfo *pcbinfo)
{
	struct ip *ip = &icmp->icmp_ip;
	struct udphdr *uh;
	struct inpcb *inp;

	if (icmp_errmap(icmp) == 0)
		return;

	uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
	inp = in_pcblookup(pcbinfo, ip->ip_dst, uh->uh_dport, ip->ip_src,
	    uh->uh_sport, INPLOOKUP_WLOCKPCB, NULL);
	if (inp != NULL) {
		INP_WLOCK_ASSERT(inp);
		if (inp->inp_socket != NULL)
			udp_notify(inp, icmp_errmap(icmp));
		INP_WUNLOCK(inp);
	} else {
		inp = in_pcblookup(pcbinfo, ip->ip_dst, uh->uh_dport,
		    ip->ip_src, uh->uh_sport,
		    INPLOOKUP_WILDCARD | INPLOOKUP_RLOCKPCB, NULL);
		if (inp != NULL) {
			struct udpcb *up;
			udp_tun_icmp_t *func;

			up = intoudpcb(inp);
			func = up->u_icmp_func;
			INP_RUNLOCK(inp);
			if (func != NULL)
				func(icmp);
		}
	}
}

static void
udp_ctlinput(struct icmp *icmp)
{

	return (udp_common_ctlinput(icmp, &V_udbinfo));
}

static void
udplite_ctlinput(struct icmp *icmp)
{

	return (udp_common_ctlinput(icmp, &V_ulitecbinfo));
}
#endif /* INET */

static int
udp_pcblist(SYSCTL_HANDLER_ARGS)
{
	struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_udbinfo,
	    INPLOOKUP_RLOCKPCB);
	struct xinpgen xig;
	struct inpcb *inp;
	int error;

	if (req->newptr != 0)
		return (EPERM);

	if (req->oldptr == 0) {
		int n;

		n = V_udbinfo.ipi_count;
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xinpcb);
		return (0);
	}

	if ((error = sysctl_wire_old_buffer(req, 0)) != 0)
		return (error);

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = V_udbinfo.ipi_count;
	xig.xig_gen = V_udbinfo.ipi_gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return (error);

	while ((inp = inp_next(&inpi)) != NULL) {
		if (inp->inp_gencnt <= xig.xig_gen &&
		    cr_canseeinpcb(req->td->td_ucred, inp) == 0) {
			struct xinpcb xi;

			in_pcbtoxinpcb(inp, &xi);
			error = SYSCTL_OUT(req, &xi, sizeof xi);
			if (error) {
				INP_RUNLOCK(inp);
				break;
			}
		}
	}

	if (!error) {
		/*
		 * Give the user an updated idea of our state.  If the
		 * generation differs from what we told her before, she knows
		 * that something happened while we were processing this
		 * request, and it might be necessary to retry.
		 */
		xig.xig_gen = V_udbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_udbinfo.ipi_count;
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}

	return (error);
}

SYSCTL_PROC(_net_inet_udp, UDPCTL_PCBLIST, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    udp_pcblist, "S,xinpcb",
    "List of active UDP sockets");

#ifdef INET
static int
udp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in addrs[2];
	struct epoch_tracker et;
	struct inpcb *inp;
	int error;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	NET_EPOCH_ENTER(et);
	inp = in_pcblookup(&V_udbinfo, addrs[1].sin_addr, addrs[1].sin_port,
	    addrs[0].sin_addr, addrs[0].sin_port,
	    INPLOOKUP_WILDCARD | INPLOOKUP_RLOCKPCB, NULL);
	NET_EPOCH_EXIT(et);
	if (inp != NULL) {
		INP_RLOCK_ASSERT(inp);
		if (inp->inp_socket == NULL)
			error = ENOENT;
		if (error == 0)
			error = cr_canseeinpcb(req->td->td_ucred, inp);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else
		error = ENOENT;
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet_udp, OID_AUTO, getcred,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    0, 0, udp_getcred, "S,xucred",
    "Get the xucred of a UDP connection");
#endif /* INET */

int
udp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct inpcb *inp;
	struct udpcb *up;
	int isudplite, error, optval;

	error = 0;
	isudplite = (so->so_proto->pr_protocol == IPPROTO_UDPLITE) ? 1 : 0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
	INP_WLOCK(inp);
	if (sopt->sopt_level != so->so_proto->pr_protocol) {
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6)) {
			INP_WUNLOCK(inp);
			error = ip6_ctloutput(so, sopt);
		}
#endif
#if defined(INET) && defined(INET6)
		else
#endif
#ifdef INET
		{
			INP_WUNLOCK(inp);
			error = ip_ctloutput(so, sopt);
		}
#endif
		return (error);
	}

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
#if defined(INET) || defined(INET6)
		case UDP_ENCAP:
#ifdef INET
			if (INP_SOCKAF(so) == AF_INET) {
				if (!IPSEC_ENABLED(ipv4)) {
					INP_WUNLOCK(inp);
					return (ENOPROTOOPT);
				}
				error = UDPENCAP_PCBCTL(ipv4, inp, sopt);
				break;
			}
#endif /* INET */
#ifdef INET6
			if (INP_SOCKAF(so) == AF_INET6) {
				if (!IPSEC_ENABLED(ipv6)) {
					INP_WUNLOCK(inp);
					return (ENOPROTOOPT);
				}
				error = UDPENCAP_PCBCTL(ipv6, inp, sopt);
				break;
			}
#endif /* INET6 */
			INP_WUNLOCK(inp);
			return (EINVAL);
#endif /* INET || INET6 */

#endif /* IPSEC */
		case UDPLITE_SEND_CSCOV:
		case UDPLITE_RECV_CSCOV:
			if (!isudplite) {
				INP_WUNLOCK(inp);
				error = ENOPROTOOPT;
				break;
			}
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof(optval),
			    sizeof(optval));
			if (error != 0)
				break;
			inp = sotoinpcb(so);
			KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
			INP_WLOCK(inp);
			up = intoudpcb(inp);
			KASSERT(up != NULL, ("%s: up == NULL", __func__));
			if ((optval != 0 && optval < 8) || (optval > 65535)) {
				INP_WUNLOCK(inp);
				error = EINVAL;
				break;
			}
			if (sopt->sopt_name == UDPLITE_SEND_CSCOV)
				up->u_txcslen = optval;
			else
				up->u_rxcslen = optval;
			INP_WUNLOCK(inp);
			break;
		default:
			INP_WUNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;
	case SOPT_GET:
		switch (sopt->sopt_name) {
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
#if defined(INET) || defined(INET6)
		case UDP_ENCAP:
#ifdef INET
			if (INP_SOCKAF(so) == AF_INET) {
				if (!IPSEC_ENABLED(ipv4)) {
					INP_WUNLOCK(inp);
					return (ENOPROTOOPT);
				}
				error = UDPENCAP_PCBCTL(ipv4, inp, sopt);
				break;
			}
#endif /* INET */
#ifdef INET6
			if (INP_SOCKAF(so) == AF_INET6) {
				if (!IPSEC_ENABLED(ipv6)) {
					INP_WUNLOCK(inp);
					return (ENOPROTOOPT);
				}
				error = UDPENCAP_PCBCTL(ipv6, inp, sopt);
				break;
			}
#endif /* INET6 */
			INP_WUNLOCK(inp);
			return (EINVAL);
#endif /* INET || INET6 */

#endif /* IPSEC */
		case UDPLITE_SEND_CSCOV:
		case UDPLITE_RECV_CSCOV:
			if (!isudplite) {
				INP_WUNLOCK(inp);
				error = ENOPROTOOPT;
				break;
			}
			up = intoudpcb(inp);
			KASSERT(up != NULL, ("%s: up == NULL", __func__));
			if (sopt->sopt_name == UDPLITE_SEND_CSCOV)
				optval = up->u_txcslen;
			else
				optval = up->u_rxcslen;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		default:
			INP_WUNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}

#ifdef INET
#ifdef INET6
/* The logic here is derived from ip6_setpktopt(). See comments there. */
static int
udp_v4mapped_pktinfo(struct cmsghdr *cm, struct sockaddr_in * src,
    struct inpcb *inp, int flags)
{
	struct ifnet *ifp;
	struct in6_pktinfo *pktinfo;
	struct in_addr ia;

	if ((flags & PRUS_IPV6) == 0)
		return (0);

	if (cm->cmsg_level != IPPROTO_IPV6)
		return (0);

	if  (cm->cmsg_type != IPV6_2292PKTINFO &&
	    cm->cmsg_type != IPV6_PKTINFO)
		return (0);

	if (cm->cmsg_len !=
	    CMSG_LEN(sizeof(struct in6_pktinfo)))
		return (EINVAL);

	pktinfo = (struct in6_pktinfo *)CMSG_DATA(cm);
	if (!IN6_IS_ADDR_V4MAPPED(&pktinfo->ipi6_addr) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&pktinfo->ipi6_addr))
		return (EINVAL);

	/* Validate the interface index if specified. */
	if (pktinfo->ipi6_ifindex) {
		struct epoch_tracker et;

		NET_EPOCH_ENTER(et);
		ifp = ifnet_byindex(pktinfo->ipi6_ifindex);
		NET_EPOCH_EXIT(et);	/* XXXGL: unsafe ifp */
		if (ifp == NULL)
			return (ENXIO);
	} else
		ifp = NULL;
	if (ifp != NULL && !IN6_IS_ADDR_UNSPECIFIED(&pktinfo->ipi6_addr)) {
		ia.s_addr = pktinfo->ipi6_addr.s6_addr32[3];
		if (!in_ifhasaddr(ifp, ia))
			return (EADDRNOTAVAIL);
	}

	bzero(src, sizeof(*src));
	src->sin_family = AF_INET;
	src->sin_len = sizeof(*src);
	src->sin_port = inp->inp_lport;
	src->sin_addr.s_addr = pktinfo->ipi6_addr.s6_addr32[3];

	return (0);
}
#endif	/* INET6 */

int
udp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *td)
{
	struct inpcb *inp;
	struct udpiphdr *ui;
	int len, error = 0;
	struct in_addr faddr, laddr;
	struct cmsghdr *cm;
	struct inpcbinfo *pcbinfo;
	struct sockaddr_in *sin, src;
	struct epoch_tracker et;
	int cscov_partial = 0;
	int ipflags = 0;
	u_short fport, lport;
	u_char tos, vflagsav;
	uint8_t pr;
	uint16_t cscov = 0;
	uint32_t flowid = 0;
	uint8_t flowtype = M_HASHTYPE_NONE;
	bool use_cached_route;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_send: inp == NULL"));

	if (addr != NULL) {
		if (addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		else if (addr->sa_len != sizeof(struct sockaddr_in))
			error = EINVAL;
		if (__predict_false(error != 0)) {
			m_freem(control);
			m_freem(m);
			return (error);
		}
	}

	len = m->m_pkthdr.len;
	if (len + sizeof(struct udpiphdr) > IP_MAXPACKET) {
		if (control)
			m_freem(control);
		m_freem(m);
		return (EMSGSIZE);
	}

	src.sin_family = 0;
	sin = (struct sockaddr_in *)addr;

	/*
	 * udp_send() may need to bind the current inpcb.  As such, we don't
	 * know up front whether we will need the pcbinfo lock or not.  Do any
	 * work to decide what is needed up front before acquiring any locks.
	 *
	 * We will need network epoch in either case, to safely lookup into
	 * pcb hash.
	 */
	use_cached_route = sin == NULL || (inp->inp_laddr.s_addr == INADDR_ANY && inp->inp_lport == 0);
	if (use_cached_route || (flags & PRUS_IPV6) != 0)
		INP_WLOCK(inp);
	else
		INP_RLOCK(inp);
	NET_EPOCH_ENTER(et);
	tos = inp->inp_ip_tos;
	if (control != NULL) {
		/*
		 * XXX: Currently, we assume all the optional information is
		 * stored in a single mbuf.
		 */
		if (control->m_next) {
			m_freem(control);
			error = EINVAL;
			goto release;
		}
		for (; control->m_len > 0;
		    control->m_data += CMSG_ALIGN(cm->cmsg_len),
		    control->m_len -= CMSG_ALIGN(cm->cmsg_len)) {
			cm = mtod(control, struct cmsghdr *);
			if (control->m_len < sizeof(*cm) || cm->cmsg_len == 0
			    || cm->cmsg_len > control->m_len) {
				error = EINVAL;
				break;
			}
#ifdef INET6
			error = udp_v4mapped_pktinfo(cm, &src, inp, flags);
			if (error != 0)
				break;
#endif
			if (cm->cmsg_level != IPPROTO_IP)
				continue;

			switch (cm->cmsg_type) {
			case IP_SENDSRCADDR:
				if (cm->cmsg_len !=
				    CMSG_LEN(sizeof(struct in_addr))) {
					error = EINVAL;
					break;
				}
				bzero(&src, sizeof(src));
				src.sin_family = AF_INET;
				src.sin_len = sizeof(src);
				src.sin_port = inp->inp_lport;
				src.sin_addr =
				    *(struct in_addr *)CMSG_DATA(cm);
				break;

			case IP_TOS:
				if (cm->cmsg_len != CMSG_LEN(sizeof(u_char))) {
					error = EINVAL;
					break;
				}
				tos = *(u_char *)CMSG_DATA(cm);
				break;

			case IP_FLOWID:
				if (cm->cmsg_len != CMSG_LEN(sizeof(uint32_t))) {
					error = EINVAL;
					break;
				}
				flowid = *(uint32_t *) CMSG_DATA(cm);
				break;

			case IP_FLOWTYPE:
				if (cm->cmsg_len != CMSG_LEN(sizeof(uint32_t))) {
					error = EINVAL;
					break;
				}
				flowtype = *(uint32_t *) CMSG_DATA(cm);
				break;

#ifdef	RSS
			case IP_RSSBUCKETID:
				if (cm->cmsg_len != CMSG_LEN(sizeof(uint32_t))) {
					error = EINVAL;
					break;
				}
				/* This is just a placeholder for now */
				break;
#endif	/* RSS */
			default:
				error = ENOPROTOOPT;
				break;
			}
			if (error)
				break;
		}
		m_freem(control);
		control = NULL;
	}
	if (error)
		goto release;

	pr = inp->inp_socket->so_proto->pr_protocol;
	pcbinfo = udp_get_inpcbinfo(pr);

	/*
	 * If the IP_SENDSRCADDR control message was specified, override the
	 * source address for this datagram.  Its use is invalidated if the
	 * address thus specified is incomplete or clobbers other inpcbs.
	 */
	laddr = inp->inp_laddr;
	lport = inp->inp_lport;
	if (src.sin_family == AF_INET) {
		if ((lport == 0) ||
		    (laddr.s_addr == INADDR_ANY &&
		     src.sin_addr.s_addr == INADDR_ANY)) {
			error = EINVAL;
			goto release;
		}
		if ((flags & PRUS_IPV6) != 0) {
			vflagsav = inp->inp_vflag;
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
		}
		INP_HASH_WLOCK(pcbinfo);
		error = in_pcbbind_setup(inp, &src, &laddr.s_addr, &lport,
		    V_udp_bind_all_fibs ? 0 : INPBIND_FIB, td->td_ucred);
		INP_HASH_WUNLOCK(pcbinfo);
		if ((flags & PRUS_IPV6) != 0)
			inp->inp_vflag = vflagsav;
		if (error)
			goto release;
	}

	/*
	 * If a UDP socket has been connected, then a local address/port will
	 * have been selected and bound.
	 *
	 * If a UDP socket has not been connected to, then an explicit
	 * destination address must be used, in which case a local
	 * address/port may not have been selected and bound.
	 */
	if (sin != NULL) {
		INP_LOCK_ASSERT(inp);
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			goto release;
		}

		/*
		 * Jail may rewrite the destination address, so let it do
		 * that before we use it.
		 */
		error = prison_remote_ip4(td->td_ucred, &sin->sin_addr);
		if (error)
			goto release;
		/*
		 * sendto(2) on unconnected UDP socket results in implicit
		 * binding to INADDR_ANY and anonymous port.  This has two
		 * side effects:
		 * 1) after first sendto(2) the socket will receive datagrams
		 *    destined to the selected port.
		 * 2) subsequent sendto(2) calls will use the same source port.
		 */
		if (inp->inp_lport == 0) {
			struct sockaddr_in wild = {
				.sin_family = AF_INET,
				.sin_len = sizeof(struct sockaddr_in),
			};

			INP_HASH_WLOCK(pcbinfo);
			error = in_pcbbind(inp, &wild, V_udp_bind_all_fibs ?
			    0 : INPBIND_FIB, td->td_ucred);
			INP_HASH_WUNLOCK(pcbinfo);
			if (error)
				goto release;
			lport = inp->inp_lport;
			laddr = inp->inp_laddr;
		}
		if (laddr.s_addr == INADDR_ANY) {
			error = in_pcbladdr(inp, &sin->sin_addr, &laddr,
			    td->td_ucred);
			if (error)
				goto release;
		}
		faddr = sin->sin_addr;
		fport = sin->sin_port;
	} else {
		INP_LOCK_ASSERT(inp);
		faddr = inp->inp_faddr;
		fport = inp->inp_fport;
		if (faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			goto release;
		}
	}

	/*
	 * Calculate data length and get a mbuf for UDP, IP, and possible
	 * link-layer headers.  Immediate slide the data pointer back forward
	 * since we won't use that space at this layer.
	 */
	M_PREPEND(m, sizeof(struct udpiphdr) + max_linkhdr, M_NOWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	m->m_data += max_linkhdr;
	m->m_len -= max_linkhdr;
	m->m_pkthdr.len -= max_linkhdr;

	/*
	 * Fill in mbuf with extended UDP header and addresses and length put
	 * into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	/*
	 * Filling only those fields of udpiphdr that participate in the
	 * checksum calculation. The rest must be zeroed and will be filled
	 * later.
	 */
	bzero(ui->ui_x1, sizeof(ui->ui_x1));
	ui->ui_pr = pr;
	ui->ui_src = laddr;
	ui->ui_dst = faddr;
	ui->ui_sport = lport;
	ui->ui_dport = fport;
	ui->ui_ulen = htons((u_short)len + sizeof(struct udphdr));
	if (pr == IPPROTO_UDPLITE) {
		struct udpcb *up;
		uint16_t plen;

		up = intoudpcb(inp);
		cscov = up->u_txcslen;
		plen = (u_short)len + sizeof(struct udphdr);
		if (cscov >= plen)
			cscov = 0;
		ui->ui_len = htons(plen);
		ui->ui_ulen = htons(cscov);
		/*
		 * For UDP-Lite, checksum coverage length of zero means
		 * the entire UDPLite packet is covered by the checksum.
		 */
		cscov_partial = (cscov == 0) ? 0 : 1;
	}

	if (inp->inp_socket->so_options & SO_DONTROUTE)
		ipflags |= IP_ROUTETOIF;
	if (inp->inp_socket->so_options & SO_BROADCAST)
		ipflags |= IP_ALLOWBROADCAST;
	if (inp->inp_flags & INP_ONESBCAST)
		ipflags |= IP_SENDONES;

#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
#endif

	/*
	 * Set up checksum and output datagram.
	 */
	ui->ui_sum = 0;
	if (pr == IPPROTO_UDPLITE) {
		if (inp->inp_flags & INP_ONESBCAST)
			faddr.s_addr = INADDR_BROADCAST;
		if (cscov_partial) {
			if ((ui->ui_sum = in_cksum(m, sizeof(struct ip) + cscov)) == 0)
				ui->ui_sum = 0xffff;
		} else {
			if ((ui->ui_sum = in_cksum(m, sizeof(struct udpiphdr) + len)) == 0)
				ui->ui_sum = 0xffff;
		}
	} else if (V_udp_cksum) {
		if (inp->inp_flags & INP_ONESBCAST)
			faddr.s_addr = INADDR_BROADCAST;
		ui->ui_sum = in_pseudo(ui->ui_src.s_addr, faddr.s_addr,
		    htons((u_short)len + sizeof(struct udphdr) + pr));
		m->m_pkthdr.csum_flags = CSUM_UDP;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	}
	/*
	 * After finishing the checksum computation, fill the remaining fields
	 * of udpiphdr.
	 */
	((struct ip *)ui)->ip_v = IPVERSION;
	((struct ip *)ui)->ip_tos = tos;
	((struct ip *)ui)->ip_len = htons(sizeof(struct udpiphdr) + len);
	if (inp->inp_flags & INP_DONTFRAG)
		((struct ip *)ui)->ip_off |= htons(IP_DF);
	((struct ip *)ui)->ip_ttl = inp->inp_ip_ttl;
	UDPSTAT_INC(udps_opackets);

	/*
	 * Setup flowid / RSS information for outbound socket.
	 *
	 * Once the UDP code decides to set a flowid some other way,
	 * this allows the flowid to be overridden by userland.
	 */
	if (flowtype != M_HASHTYPE_NONE) {
		m->m_pkthdr.flowid = flowid;
		M_HASHTYPE_SET(m, flowtype);
	}
#if defined(ROUTE_MPATH) || defined(RSS)
	else if (CALC_FLOWID_OUTBOUND_SENDTO) {
		uint32_t hash_val, hash_type;

		hash_val = fib4_calc_packet_hash(laddr, faddr,
		    lport, fport, pr, &hash_type);
		m->m_pkthdr.flowid = hash_val;
		M_HASHTYPE_SET(m, hash_type);
	}

	/*
	 * Don't override with the inp cached flowid value.
	 *
	 * Depending upon the kind of send being done, the inp
	 * flowid/flowtype values may actually not be appropriate
	 * for this particular socket send.
	 *
	 * We should either leave the flowid at zero (which is what is
	 * currently done) or set it to some software generated
	 * hash value based on the packet contents.
	 */
	ipflags |= IP_NODEFAULTFLOWID;
#endif	/* RSS */

	if (pr == IPPROTO_UDPLITE)
		UDPLITE_PROBE(send, NULL, inp, &ui->ui_i, inp, &ui->ui_u);
	else
		UDP_PROBE(send, NULL, inp, &ui->ui_i, inp, &ui->ui_u);
	error = ip_output(m, inp->inp_options,
	    use_cached_route ? &inp->inp_route : NULL, ipflags,
	    inp->inp_moptions, inp);
	INP_UNLOCK(inp);
	NET_EPOCH_EXIT(et);
	return (error);

release:
	INP_UNLOCK(inp);
	NET_EPOCH_EXIT(et);
	m_freem(m);
	return (error);
}

void
udp_abort(struct socket *so)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_abort: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_HASH_WLOCK(pcbinfo);
		in_pcbdisconnect(inp);
		INP_HASH_WUNLOCK(pcbinfo);
		soisdisconnected(so);
	}
	INP_WUNLOCK(inp);
}

static int
udp_attach(struct socket *so, int proto, struct thread *td)
{
	static uint32_t udp_flowid;
	struct inpcbinfo *pcbinfo;
	struct inpcb *inp;
	struct udpcb *up;
	int error;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("udp_attach: inp != NULL"));
	error = soreserve(so, udp_sendspace, udp_recvspace);
	if (error)
		return (error);
	error = in_pcballoc(so, pcbinfo);
	if (error)
		return (error);

	inp = sotoinpcb(so);
	inp->inp_ip_ttl = V_ip_defttl;
	inp->inp_flowid = atomic_fetchadd_int(&udp_flowid, 1);
	inp->inp_flowtype = M_HASHTYPE_OPAQUE;
	up = intoudpcb(inp);
	bzero(&up->u_start_zero, u_zero_size);
	INP_WUNLOCK(inp);

	return (0);
}
#endif /* INET */

int
udp_set_kernel_tunneling(struct socket *so, udp_tun_func_t f, udp_tun_icmp_t i, void *ctx)
{
	struct inpcb *inp;
	struct udpcb *up;

	KASSERT(so->so_type == SOCK_DGRAM,
	    ("udp_set_kernel_tunneling: !dgram"));
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_set_kernel_tunneling: inp == NULL"));
	INP_WLOCK(inp);
	up = intoudpcb(inp);
	if ((f != NULL || i != NULL) && ((up->u_tun_func != NULL) ||
	    (up->u_icmp_func != NULL))) {
		INP_WUNLOCK(inp);
		return (EBUSY);
	}
	up->u_tun_func = f;
	up->u_icmp_func = i;
	up->u_tun_ctx = ctx;
	INP_WUNLOCK(inp);
	return (0);
}

#ifdef INET
static int
udp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;
	struct sockaddr_in *sinp;
	int error;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_bind: inp == NULL"));

	sinp = (struct sockaddr_in *)nam;
	if (nam->sa_family != AF_INET) {
		/*
		 * Preserve compatibility with old programs.
		 */
		if (nam->sa_family != AF_UNSPEC ||
		    nam->sa_len < offsetof(struct sockaddr_in, sin_zero) ||
		    sinp->sin_addr.s_addr != INADDR_ANY)
			return (EAFNOSUPPORT);
		nam->sa_family = AF_INET;
	}
	if (nam->sa_len != sizeof(struct sockaddr_in))
		return (EINVAL);

	INP_WLOCK(inp);
	INP_HASH_WLOCK(pcbinfo);
	error = in_pcbbind(inp, sinp, V_udp_bind_all_fibs ? 0 : INPBIND_FIB,
	    td->td_ucred);
	INP_HASH_WUNLOCK(pcbinfo);
	INP_WUNLOCK(inp);
	return (error);
}

static void
udp_close(struct socket *so)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_close: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_HASH_WLOCK(pcbinfo);
		in_pcbdisconnect(inp);
		INP_HASH_WUNLOCK(pcbinfo);
		soisdisconnected(so);
	}
	INP_WUNLOCK(inp);
}

static int
udp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct epoch_tracker et;
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;
	struct sockaddr_in *sin;
	int error;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_connect: inp == NULL"));

	sin = (struct sockaddr_in *)nam;
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_len != sizeof(*sin))
		return (EINVAL);

	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_WUNLOCK(inp);
		return (EISCONN);
	}
	error = prison_remote_ip4(td->td_ucred, &sin->sin_addr);
	if (error != 0) {
		INP_WUNLOCK(inp);
		return (error);
	}
	NET_EPOCH_ENTER(et);
	INP_HASH_WLOCK(pcbinfo);
	error = in_pcbconnect(inp, sin, td->td_ucred);
	INP_HASH_WUNLOCK(pcbinfo);
	NET_EPOCH_EXIT(et);
	if (error == 0)
		soisconnected(so);
	INP_WUNLOCK(inp);
	return (error);
}

static void
udp_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_detach: inp == NULL"));
	KASSERT(inp->inp_faddr.s_addr == INADDR_ANY,
	    ("udp_detach: not disconnected"));
	INP_WLOCK(inp);
	in_pcbfree(inp);
}

int
udp_disconnect(struct socket *so)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_disconnect: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_faddr.s_addr == INADDR_ANY) {
		INP_WUNLOCK(inp);
		return (ENOTCONN);
	}
	INP_HASH_WLOCK(pcbinfo);
	in_pcbdisconnect(inp);
	INP_HASH_WUNLOCK(pcbinfo);
	SOCK_LOCK(so);
	so->so_state &= ~SS_ISCONNECTED;		/* XXX */
	SOCK_UNLOCK(so);
	INP_WUNLOCK(inp);
	return (0);
}
#endif /* INET */

int
udp_shutdown(struct socket *so, enum shutdown_how how)
{
	int error;

	SOCK_LOCK(so);
	if (!(so->so_state & SS_ISCONNECTED))
		/*
		 * POSIX mandates us to just return ENOTCONN when shutdown(2) is
		 * invoked on a datagram sockets, however historically we would
		 * actually tear socket down.  This is known to be leveraged by
		 * some applications to unblock process waiting in recv(2) by
		 * other process that it shares that socket with.  Try to meet
		 * both backward-compatibility and POSIX requirements by forcing
		 * ENOTCONN but still flushing buffers and performing wakeup(9).
		 *
		 * XXXGL: it remains unknown what applications expect this
		 * behavior and is this isolated to unix/dgram or inet/dgram or
		 * both.  See: D10351, D3039.
		 */
		error = ENOTCONN;
	else
		error = 0;
	SOCK_UNLOCK(so);

	switch (how) {
	case SHUT_RD:
		sorflush(so);
		break;
	case SHUT_RDWR:
		sorflush(so);
		/* FALLTHROUGH */
	case SHUT_WR:
		socantsendmore(so);
	}

	return (error);
}

#ifdef INET
#define	UDP_PROTOSW							\
	.pr_type =		SOCK_DGRAM,				\
	.pr_flags =		PR_ATOMIC | PR_ADDR | PR_CAPATTACH,	\
	.pr_ctloutput =		udp_ctloutput,				\
	.pr_abort =		udp_abort,				\
	.pr_attach =		udp_attach,				\
	.pr_bind =		udp_bind,				\
	.pr_connect =		udp_connect,				\
	.pr_control =		in_control,				\
	.pr_detach =		udp_detach,				\
	.pr_disconnect =	udp_disconnect,				\
	.pr_peeraddr =		in_getpeeraddr,				\
	.pr_send =		udp_send,				\
	.pr_soreceive =		soreceive_dgram,			\
	.pr_sosend =		sosend_dgram,				\
	.pr_shutdown =		udp_shutdown,				\
	.pr_sockaddr =		in_getsockaddr,				\
	.pr_sosetlabel =	in_pcbsosetlabel,			\
	.pr_close =		udp_close

struct protosw udp_protosw = {
	.pr_protocol =		IPPROTO_UDP,
	UDP_PROTOSW
};

struct protosw udplite_protosw = {
	.pr_protocol =		IPPROTO_UDPLITE,
	UDP_PROTOSW
};

static void
udp_init(void *arg __unused)
{

	IPPROTO_REGISTER(IPPROTO_UDP, udp_input, udp_ctlinput);
	IPPROTO_REGISTER(IPPROTO_UDPLITE, udp_input, udplite_ctlinput);
}
SYSINIT(udp_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, udp_init, NULL);
#endif /* INET */
