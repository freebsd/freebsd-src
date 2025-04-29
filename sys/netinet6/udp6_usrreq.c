/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$KAME: udp6_usrreq.c,v 1.27 2001/05/21 05:45:10 jinmei Exp $
 *	$KAME: udp6_output.c,v 1.31 2001/05/21 16:39:15 jinmei Exp $
 */

/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.
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
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
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

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/rss_config.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udplite.h>

#include <netinet6/ip6_var.h>
#include <netinet6/in6_fib.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_rss.h>
#include <netinet6/udp6_var.h>
#include <netinet6/scope6_var.h>

#include <netipsec/ipsec_support.h>

#include <security/mac/mac_framework.h>

VNET_DEFINE(int, zero_checksum_port) = 0;
#define	V_zero_checksum_port	VNET(zero_checksum_port)
SYSCTL_INT(_net_inet6_udp6, OID_AUTO, rfc6935_port, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(zero_checksum_port), 0,
    "Zero UDP checksum allowed for traffic to/from this port.");

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */

static void		udp6_detach(struct socket *so);

static int
udp6_append(struct inpcb *inp, struct mbuf *n, int off,
    struct sockaddr_in6 *fromsa)
{
	struct socket *so;
	struct mbuf *opts = NULL, *tmp_opts;
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
		filtered = (*up->u_tun_func)(n, off, inp,
		    (struct sockaddr *)&fromsa[0], up->u_tun_ctx);
		INP_RLOCK(inp);
		if (filtered)
			return (in_pcbrele_rlocked(inp));
	}

	off += sizeof(struct udphdr);

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/* Check AH/ESP integrity. */
	if (IPSEC_ENABLED(ipv6)) {
		if (IPSEC_CHECK_POLICY(ipv6, n, inp) != 0) {
			m_freem(n);
			return (0);
		}

		/* IPSec UDP encaps. */
		if ((up->u_flags & UF_ESPINUDP) != 0 &&
		    UDPENCAP_INPUT(ipv6, n, off, AF_INET6) != 0) {
			return (0); /* Consumed. */
		}
	}
#endif /* IPSEC */
#ifdef MAC
	if (mac_inpcb_check_deliver(inp, n) != 0) {
		m_freem(n);
		return (0);
	}
#endif
	opts = NULL;
	if (inp->inp_flags & INP_CONTROLOPTS ||
	    inp->inp_socket->so_options & SO_TIMESTAMP)
		ip6_savecontrol(inp, n, &opts);
	if ((inp->inp_vflag & INP_IPV6) && (inp->inp_flags2 & INP_ORIGDSTADDR)) {
		tmp_opts = sbcreatecontrol(&fromsa[1],
		    sizeof(struct sockaddr_in6), IPV6_ORIGDSTADDR,
		    IPPROTO_IPV6, M_NOWAIT);
                if (tmp_opts) {
                        if (opts) {
                                tmp_opts->m_next = opts;
                                opts = tmp_opts;
                        } else
                                opts = tmp_opts;
                }
	}
	m_adj(n, off);

	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_rcv);
	if (sbappendaddr_locked(&so->so_rcv, (struct sockaddr *)&fromsa[0], n,
	    opts) == 0) {
		soroverflow_locked(so);
		m_freem(n);
		if (opts)
			m_freem(opts);
		UDPSTAT_INC(udps_fullsock);
	} else
		sorwakeup_locked(so);
	return (0);
}

struct udp6_multi_match_ctx {
	struct ip6_hdr *ip6;
	struct udphdr *uh;
};

static bool
udp6_multi_match(const struct inpcb *inp, void *v)
{
	struct udp6_multi_match_ctx *ctx = v;

	if ((inp->inp_vflag & INP_IPV6) == 0)
		return(false);
	if (inp->inp_lport != ctx->uh->uh_dport)
		return(false);
	if (inp->inp_fport != 0 && inp->inp_fport != ctx->uh->uh_sport)
		return(false);
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) &&
	    !IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, &ctx->ip6->ip6_dst))
		return (false);
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
	    (!IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, &ctx->ip6->ip6_src) ||
	    inp->inp_fport != ctx->uh->uh_sport))
		return (false);

	return (true);
}

static int
udp6_multi_input(struct mbuf *m, int off, int proto,
    struct sockaddr_in6 *fromsa)
{
	struct udp6_multi_match_ctx ctx;
	struct inpcb_iterator inpi = INP_ITERATOR(udp_get_inpcbinfo(proto),
	    INPLOOKUP_RLOCKPCB, udp6_multi_match, &ctx);
	struct inpcb *inp;
	struct ip6_moptions *imo;
	struct mbuf *n;
	int appends = 0;

	/*
	 * In the event that laddr should be set to the link-local
	 * address (this happens in RIPng), the multicast address
	 * specified in the received packet will not match laddr.  To
	 * handle this situation, matching is relaxed if the
	 * receiving interface is the same as one specified in the
	 * socket and if the destination multicast address matches
	 * one of the multicast groups specified in the socket.
	 */

	/*
	 * KAME note: traditionally we dropped udpiphdr from mbuf
	 * here.  We need udphdr for IPsec processing so we do that
	 * later.
	 */
	ctx.ip6 = mtod(m, struct ip6_hdr *);
	ctx.uh = (struct udphdr *)((char *)ctx.ip6 + off);
	while ((inp = inp_next(&inpi)) != NULL) {
		INP_RLOCK_ASSERT(inp);
		/*
		 * XXXRW: Because we weren't holding either the inpcb
		 * or the hash lock when we checked for a match
		 * before, we should probably recheck now that the
		 * inpcb lock is (supposed to be) held.
		 */
		/*
		 * Handle socket delivery policy for any-source
		 * and source-specific multicast. [RFC3678]
		 */
		if ((imo = inp->in6p_moptions) != NULL) {
			struct sockaddr_in6	 mcaddr;
			int			 blocked;

			bzero(&mcaddr, sizeof(struct sockaddr_in6));
			mcaddr.sin6_len = sizeof(struct sockaddr_in6);
			mcaddr.sin6_family = AF_INET6;
			mcaddr.sin6_addr = ctx.ip6->ip6_dst;

			blocked = im6o_mc_filter(imo, m->m_pkthdr.rcvif,
				(struct sockaddr *)&mcaddr,
				(struct sockaddr *)&fromsa[0]);
			if (blocked != MCAST_PASS) {
				if (blocked == MCAST_NOTGMEMBER)
					IP6STAT_INC(ip6s_notmember);
				if (blocked == MCAST_NOTSMEMBER ||
				    blocked == MCAST_MUTED)
					UDPSTAT_INC(udps_filtermcast);
				continue;
			}
		}
		if ((n = m_copym(m, 0, M_COPYALL, M_NOWAIT)) != NULL) {
			if (proto == IPPROTO_UDPLITE)
				UDPLITE_PROBE(receive, NULL, inp, ctx.ip6,
				    inp, ctx.uh);
			else
				UDP_PROBE(receive, NULL, inp, ctx.ip6, inp,
				    ctx.uh);
			if (udp6_append(inp, n, off, fromsa)) {
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
	m_freem(m);

	if (appends == 0) {
		/*
		 * No matching pcb found; discard datagram.  (No need
		 * to send an ICMP Port Unreachable for a broadcast
		 * or multicast datgram.)
		 */
		UDPSTAT_INC(udps_noport);
		UDPSTAT_INC(udps_noportmcast);
	}

	return (IPPROTO_DONE);
}

int
udp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;
	struct udphdr *uh;
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;
	struct udpcb *up;
	int off = *offp;
	int cscov_partial;
	int plen, ulen;
	int lookupflags;
	struct sockaddr_in6 fromsa[2];
	struct m_tag *fwd_tag;
	uint16_t uh_sum;
	uint8_t nxt;

	NET_EPOCH_ASSERT();

	if (m->m_len < off + sizeof(struct udphdr)) {
		m = m_pullup(m, off + sizeof(struct udphdr));
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			*mp = NULL;
			return (IPPROTO_DONE);
		}
	}
	ip6 = mtod(m, struct ip6_hdr *);
	uh = (struct udphdr *)((caddr_t)ip6 + off);

	UDPSTAT_INC(udps_ipackets);

	/*
	 * Destination port of 0 is illegal, based on RFC768.
	 */
	if (uh->uh_dport == 0)
		goto badunlocked;

	plen = ntohs(ip6->ip6_plen) - off + sizeof(*ip6);
	ulen = ntohs((u_short)uh->uh_ulen);

	nxt = proto;
	cscov_partial = (nxt == IPPROTO_UDPLITE) ? 1 : 0;
	if (nxt == IPPROTO_UDPLITE) {
		/* Zero means checksum over the complete packet. */
		if (ulen == 0)
			ulen = plen;
		if (ulen == plen)
			cscov_partial = 0;
		if ((ulen < sizeof(struct udphdr)) || (ulen > plen)) {
			/* XXX: What is the right UDPLite MIB counter? */
			goto badunlocked;
		}
		if (uh->uh_sum == 0) {
			/* XXX: What is the right UDPLite MIB counter? */
			goto badunlocked;
		}
	} else {
		if ((ulen < sizeof(struct udphdr)) || (plen != ulen)) {
			UDPSTAT_INC(udps_badlen);
			goto badunlocked;
		}
		if (uh->uh_sum == 0) {
			UDPSTAT_INC(udps_nosum);
			/*
			 * dport 0 was rejected earlier so this is OK even if
			 * zero_checksum_port is 0 (which is its default value).
			 */
			if (ntohs(uh->uh_dport) == V_zero_checksum_port)
				goto skip_checksum;
			else
				goto badunlocked;
		}
	}

	if ((m->m_pkthdr.csum_flags & CSUM_DATA_VALID_IPV6) &&
	    !cscov_partial) {
		if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
			uh_sum = m->m_pkthdr.csum_data;
		else
			uh_sum = in6_cksum_pseudo(ip6, ulen, nxt,
			    m->m_pkthdr.csum_data);
		uh_sum ^= 0xffff;
	} else
		uh_sum = in6_cksum_partial(m, nxt, off, plen, ulen);

	if (uh_sum != 0) {
		UDPSTAT_INC(udps_badsum);
		goto badunlocked;
	}

skip_checksum:
	/*
	 * Construct sockaddr format source address.
	 */
	init_sin6(&fromsa[0], m, 0);
	fromsa[0].sin6_port = uh->uh_sport;
	init_sin6(&fromsa[1], m, 1);
	fromsa[1].sin6_port = uh->uh_dport;

	pcbinfo = udp_get_inpcbinfo(nxt);
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst))  {
		*mp = NULL;
		return (udp6_multi_input(m, off, proto, fromsa));
	}

	/*
	 * Locate pcb for datagram.
	 */
	lookupflags = INPLOOKUP_RLOCKPCB |
	    (V_udp_bind_all_fibs ? 0 : INPLOOKUP_FIB);

	/*
	 * Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain.
	 */
	if ((m->m_flags & M_IP6_NEXTHOP) &&
	    (fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL)) != NULL) {
		struct sockaddr_in6 *next_hop6;

		next_hop6 = (struct sockaddr_in6 *)(fwd_tag + 1);

		/*
		 * Transparently forwarded. Pretend to be the destination.
		 * Already got one like this?
		 */
		inp = in6_pcblookup_mbuf(pcbinfo, &ip6->ip6_src,
		    uh->uh_sport, &ip6->ip6_dst, uh->uh_dport,
		    lookupflags, m->m_pkthdr.rcvif, m);
		if (!inp) {
			/*
			 * It's new.  Try to find the ambushing socket.
			 * Because we've rewritten the destination address,
			 * any hardware-generated hash is ignored.
			 */
			inp = in6_pcblookup(pcbinfo, &ip6->ip6_src,
			    uh->uh_sport, &next_hop6->sin6_addr,
			    next_hop6->sin6_port ? htons(next_hop6->sin6_port) :
			    uh->uh_dport, INPLOOKUP_WILDCARD | lookupflags,
			    m->m_pkthdr.rcvif);
		}
		/* Remove the tag from the packet. We don't need it anymore. */
		m_tag_delete(m, fwd_tag);
		m->m_flags &= ~M_IP6_NEXTHOP;
	} else
		inp = in6_pcblookup_mbuf(pcbinfo, &ip6->ip6_src,
		    uh->uh_sport, &ip6->ip6_dst, uh->uh_dport,
		    INPLOOKUP_WILDCARD | lookupflags,
		    m->m_pkthdr.rcvif, m);
	if (inp == NULL) {
		if (V_udp_log_in_vain) {
			char ip6bufs[INET6_ADDRSTRLEN];
			char ip6bufd[INET6_ADDRSTRLEN];

			log(LOG_INFO,
			    "Connection attempt to UDP [%s]:%d from [%s]:%d\n",
			    ip6_sprintf(ip6bufd, &ip6->ip6_dst),
			    ntohs(uh->uh_dport),
			    ip6_sprintf(ip6bufs, &ip6->ip6_src),
			    ntohs(uh->uh_sport));
		}
		if (nxt == IPPROTO_UDPLITE)
			UDPLITE_PROBE(receive, NULL, NULL, ip6, NULL, uh);
		else
			UDP_PROBE(receive, NULL, NULL, ip6, NULL, uh);
		UDPSTAT_INC(udps_noport);
		if (m->m_flags & M_MCAST) {
			printf("UDP6: M_MCAST is set in a unicast packet.\n");
			UDPSTAT_INC(udps_noportmcast);
			goto badunlocked;
		}
		if (V_udp_blackhole && (V_udp_blackhole_local ||
		    !in6_localaddr(&ip6->ip6_src)))
			goto badunlocked;
		icmp6_error(m, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOPORT, 0);
		*mp = NULL;
		return (IPPROTO_DONE);
	}
	INP_RLOCK_ASSERT(inp);
	up = intoudpcb(inp);
	if (cscov_partial) {
		if (up->u_rxcslen == 0 || up->u_rxcslen > ulen) {
			INP_RUNLOCK(inp);
			m_freem(m);
			*mp = NULL;
			return (IPPROTO_DONE);
		}
	}
	if (nxt == IPPROTO_UDPLITE)
		UDPLITE_PROBE(receive, NULL, inp, ip6, inp, uh);
	else
		UDP_PROBE(receive, NULL, inp, ip6, inp, uh);
	if (udp6_append(inp, m, off, fromsa) == 0)
		INP_RUNLOCK(inp);
	*mp = NULL;
	return (IPPROTO_DONE);

badunlocked:
	m_freem(m);
	*mp = NULL;
	return (IPPROTO_DONE);
}

static void
udp6_common_ctlinput(struct ip6ctlparam *ip6cp, struct inpcbinfo *pcbinfo)
{
	struct udphdr uh;
	struct ip6_hdr *ip6;
	struct mbuf *m;
	struct inpcb *inp;
	int errno, off = 0;
	struct udp_portonly {
		u_int16_t uh_sport;
		u_int16_t uh_dport;
	} *uhp;

	if ((errno = icmp6_errmap(ip6cp->ip6c_icmp6)) == 0)
		return;

	m = ip6cp->ip6c_m;
	ip6 = ip6cp->ip6c_ip6;
	off = ip6cp->ip6c_off;

	/* Check if we can safely examine src and dst ports. */
	if (m->m_pkthdr.len < off + sizeof(*uhp))
		return;

	bzero(&uh, sizeof(uh));
	m_copydata(m, off, sizeof(*uhp), (caddr_t)&uh);

	/* Check to see if its tunneled */
	inp = in6_pcblookup_mbuf(pcbinfo, &ip6->ip6_dst, uh.uh_dport,
	    &ip6->ip6_src, uh.uh_sport, INPLOOKUP_WILDCARD | INPLOOKUP_RLOCKPCB,
	    m->m_pkthdr.rcvif, m);
	if (inp != NULL) {
		struct udpcb *up;
		udp_tun_icmp_t *func;

		up = intoudpcb(inp);
		func = up->u_icmp_func;
		INP_RUNLOCK(inp);
		if (func != NULL)
			func(ip6cp);
	}
	in6_pcbnotify(pcbinfo, ip6cp->ip6c_finaldst, uh.uh_dport,
	    ip6cp->ip6c_src, uh.uh_sport, errno, ip6cp->ip6c_cmdarg,
	    udp_notify);
}

static void
udp6_ctlinput(struct ip6ctlparam *ctl)
{

	return (udp6_common_ctlinput(ctl, &V_udbinfo));
}

static void
udplite6_ctlinput(struct ip6ctlparam *ctl)
{

	return (udp6_common_ctlinput(ctl, &V_ulitecbinfo));
}

static int
udp6_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in6 addrs[2];
	struct epoch_tracker et;
	struct inpcb *inp;
	int error;

	if (req->newptr == NULL)
		return (EINVAL);
	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);

	if (req->newlen != sizeof(addrs))
		return (EINVAL);
	if (req->oldlen != sizeof(struct xucred))
		return (EINVAL);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	if ((error = sa6_embedscope(&addrs[0], V_ip6_use_defzone)) != 0 ||
	    (error = sa6_embedscope(&addrs[1], V_ip6_use_defzone)) != 0) {
		return (error);
	}
	NET_EPOCH_ENTER(et);
	inp = in6_pcblookup(&V_udbinfo, &addrs[1].sin6_addr,
	    addrs[1].sin6_port, &addrs[0].sin6_addr, addrs[0].sin6_port,
	    INPLOOKUP_WILDCARD | INPLOOKUP_RLOCKPCB, NULL);
	NET_EPOCH_EXIT(et);
	if (inp != NULL) {
		INP_RLOCK_ASSERT(inp);
		if (inp->inp_socket == NULL)
			error = ENOENT;
		if (error == 0)
			error = cr_canseesocket(req->td->td_ucred,
			    inp->inp_socket);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else
		error = ENOENT;
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet6_udp6, OID_AUTO, getcred,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, udp6_getcred, "S,xucred",
    "Get the xucred of a UDP6 connection");

static int
udp6_send(struct socket *so, int flags_arg, struct mbuf *m,
    struct sockaddr *addr6, struct mbuf *control, struct thread *td)
{
	struct inpcb *inp;
	struct ip6_hdr *ip6;
	struct udphdr *udp6;
	struct in6_addr *laddr, *faddr, in6a;
	struct ip6_pktopts *optp, opt;
	struct sockaddr_in6 *sin6, tmp;
	struct epoch_tracker et;
	int cscov_partial, error, flags, hlen, scope_ambiguous;
	u_int32_t ulen, plen;
	uint16_t cscov;
	u_short fport;
	uint8_t nxt;

	if (addr6) {
		error = 0;
		if (addr6->sa_family != AF_INET6)
			error = EAFNOSUPPORT;
		else if (addr6->sa_len != sizeof(struct sockaddr_in6))
			error = EINVAL;
		if (__predict_false(error != 0)) {
			m_freem(control);
			m_freem(m);
			return (error);
		}
	}

	sin6 = (struct sockaddr_in6 *)addr6;

	/*
	 * In contrast to IPv4 we do not validate the max. packet length
	 * here due to IPv6 Jumbograms (RFC2675).
	 */

	scope_ambiguous = 0;
	if (sin6) {
		/* Protect *addr6 from overwrites. */
		tmp = *sin6;
		sin6 = &tmp;

		/*
		 * Application should provide a proper zone ID or the use of
		 * default zone IDs should be enabled.  Unfortunately, some
		 * applications do not behave as it should, so we need a
		 * workaround.  Even if an appropriate ID is not determined,
		 * we'll see if we can determine the outgoing interface.  If we
		 * can, determine the zone ID based on the interface below.
		 */
		if (sin6->sin6_scope_id == 0 && !V_ip6_use_defzone)
			scope_ambiguous = 1;
		if ((error = sa6_embedscope(sin6, V_ip6_use_defzone)) != 0) {
			if (control)
				m_freem(control);
			m_freem(m);
			return (error);
		}
	}

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
	/*
	 * In the following cases we want a write lock on the inp for either
	 * local operations or for possible route cache updates in the IPv6
	 * output path:
	 * - on connected sockets (sin6 is NULL) for route cache updates,
	 * - when we are not bound to an address and source port (it is
	 *   in6_pcbsetport() which will require the write lock).
	 *
	 * We check the inp fields before actually locking the inp, so
	 * here exists a race, and we may WLOCK the inp and end with already
	 * bound one by other thread. This is fine.
	 */
	if (sin6 == NULL || (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) &&
	    inp->inp_lport == 0))
		INP_WLOCK(inp);
	else
		INP_RLOCK(inp);

	nxt = (inp->inp_socket->so_proto->pr_protocol == IPPROTO_UDP) ?
	    IPPROTO_UDP : IPPROTO_UDPLITE;

#ifdef INET
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		int hasv4addr;

		if (sin6 == NULL)
			hasv4addr = (inp->inp_vflag & INP_IPV4);
		else
			hasv4addr = IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)
			    ? 1 : 0;
		if (hasv4addr) {
			/*
			 * XXXRW: We release UDP-layer locks before calling
			 * udp_send() in order to avoid recursion.  However,
			 * this does mean there is a short window where inp's
			 * fields are unstable.  Could this lead to a
			 * potential race in which the factors causing us to
			 * select the UDPv4 output routine are invalidated?
			 */
			INP_UNLOCK(inp);
			if (sin6)
				in6_sin6_2_sin_in_sock((struct sockaddr *)sin6);
			/* addr will just be freed in sendit(). */
			return (udp_send(so, flags_arg | PRUS_IPV6, m,
			    (struct sockaddr *)sin6, control, td));
		}
	} else
#endif
	if (sin6 && IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		/*
		 * Given this is either an IPv6-only socket or no INET is
		 * supported we will fail the send if the given destination
		 * address is a v4mapped address.
		 */
		INP_UNLOCK(inp);
		m_freem(m);
		m_freem(control);
		return (EINVAL);
	}

	NET_EPOCH_ENTER(et);
	if (control) {
		if ((error = ip6_setpktopts(control, &opt,
		    inp->in6p_outputopts, td->td_ucred, nxt)) != 0) {
			goto release;
		}
		optp = &opt;
	} else
		optp = inp->in6p_outputopts;

	if (sin6) {
		/*
		 * Since we saw no essential reason for calling in_pcbconnect,
		 * we get rid of such kind of logic, and call in6_selectsrc
		 * and in6_pcbsetport in order to fill in the local address
		 * and the local port.
		 */
		if (sin6->sin6_port == 0) {
			error = EADDRNOTAVAIL;
			goto release;
		}

		if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
			/* how about ::ffff:0.0.0.0 case? */
			error = EISCONN;
			goto release;
		}

		/*
		 * Given we handle the v4mapped case in the INET block above
		 * assert here that it must not happen anymore.
		 */
		KASSERT(!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr),
		    ("%s: sin6(%p)->sin6_addr is v4mapped which we "
		    "should have handled.", __func__, sin6));

		/* This only requires read-locking. */
		error = in6_selectsrc_socket(sin6, optp, inp,
		    td->td_ucred, scope_ambiguous, &in6a, NULL);
		if (error)
			goto release;
		laddr = &in6a;

		if (inp->inp_lport == 0) {
			struct inpcbinfo *pcbinfo;

			INP_WLOCK_ASSERT(inp);

			pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
			INP_HASH_WLOCK(pcbinfo);
			error = in6_pcbsetport(laddr, inp, td->td_ucred);
			INP_HASH_WUNLOCK(pcbinfo);
			if (error != 0) {
				/* Undo an address bind that may have occurred. */
				inp->in6p_laddr = in6addr_any;
				goto release;
			}
		}
		faddr = &sin6->sin6_addr;
		fport = sin6->sin6_port; /* allow 0 port */

	} else {
		if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
			error = ENOTCONN;
			goto release;
		}
		laddr = &inp->in6p_laddr;
		faddr = &inp->in6p_faddr;
		fport = inp->inp_fport;
	}

	ulen = m->m_pkthdr.len;
	plen = sizeof(struct udphdr) + ulen;
	hlen = sizeof(struct ip6_hdr);

	/*
	 * Calculate data length and get a mbuf for UDP, IP6, and possible
	 * link-layer headers.  Immediate slide the data pointer back forward
	 * since we won't use that space at this layer.
	 */
	M_PREPEND(m, hlen + sizeof(struct udphdr) + max_linkhdr, M_NOWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	m->m_data += max_linkhdr;
	m->m_len -= max_linkhdr;
	m->m_pkthdr.len -= max_linkhdr;

	/*
	 * Stuff checksum and output datagram.
	 */
	cscov = cscov_partial = 0;
	udp6 = (struct udphdr *)(mtod(m, caddr_t) + hlen);
	udp6->uh_sport = inp->inp_lport; /* lport is always set in the PCB */
	udp6->uh_dport = fport;
	if (nxt == IPPROTO_UDPLITE) {
		struct udpcb *up;

		up = intoudpcb(inp);
		cscov = up->u_txcslen;
		if (cscov >= plen)
			cscov = 0;
		udp6->uh_ulen = htons(cscov);
		/*
		 * For UDP-Lite, checksum coverage length of zero means
		 * the entire UDPLite packet is covered by the checksum.
		 */
		cscov_partial = (cscov == 0) ? 0 : 1;
	} else if (plen <= 0xffff)
		udp6->uh_ulen = htons((u_short)plen);
	else
		udp6->uh_ulen = 0;
	udp6->uh_sum = 0;

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow	= inp->inp_flow & IPV6_FLOWINFO_MASK;
	ip6->ip6_vfc	&= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc	|= IPV6_VERSION;
	ip6->ip6_plen	= htons((u_short)plen);
	ip6->ip6_nxt	= nxt;
	ip6->ip6_hlim	= in6_selecthlim(inp, NULL);
	ip6->ip6_src	= *laddr;
	ip6->ip6_dst	= *faddr;

#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
#endif

	if (cscov_partial) {
		if ((udp6->uh_sum = in6_cksum_partial(m, nxt,
		    sizeof(struct ip6_hdr), plen, cscov)) == 0)
			udp6->uh_sum = 0xffff;
	} else {
		udp6->uh_sum = in6_cksum_pseudo(ip6, plen, nxt, 0);
		m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	}

	flags = 0;
#if defined(ROUTE_MPATH) || defined(RSS)
	if (CALC_FLOWID_OUTBOUND_SENDTO) {
		uint32_t hash_type, hash_val;
		uint8_t pr;

		pr = inp->inp_socket->so_proto->pr_protocol;

		hash_val = fib6_calc_packet_hash(laddr, faddr,
		    inp->inp_lport, fport, pr, &hash_type);
		m->m_pkthdr.flowid = hash_val;
		M_HASHTYPE_SET(m, hash_type);
	}
	/* do not use inp flowid */
	flags |= IP_NODEFAULTFLOWID;
#endif

	UDPSTAT_INC(udps_opackets);
	if (nxt == IPPROTO_UDPLITE)
		UDPLITE_PROBE(send, NULL, inp, ip6, inp, udp6);
	else
		UDP_PROBE(send, NULL, inp, ip6, inp, udp6);
	error = ip6_output(m, optp,
	    INP_WLOCKED(inp) ? &inp->inp_route6 : NULL, flags,
	    inp->in6p_moptions, NULL, inp);
	INP_UNLOCK(inp);
	NET_EPOCH_EXIT(et);

	if (control) {
		ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	return (error);

release:
	INP_UNLOCK(inp);
	NET_EPOCH_EXIT(et);
	if (control) {
		ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	m_freem(m);

	return (error);
}

static void
udp6_abort(struct socket *so)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp6_abort: inp == NULL"));

	INP_WLOCK(inp);
#ifdef INET
	if (inp->inp_vflag & INP_IPV4) {
		INP_WUNLOCK(inp);
		udp_abort(so);
		return;
	}
#endif

	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
		INP_HASH_WLOCK(pcbinfo);
		in6_pcbdisconnect(inp);
		INP_HASH_WUNLOCK(pcbinfo);
		soisdisconnected(so);
	}
	INP_WUNLOCK(inp);
}

static int
udp6_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcbinfo *pcbinfo;
	struct inpcb *inp;
	struct udpcb *up;
	int error;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("udp6_attach: inp != NULL"));

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, udp_sendspace, udp_recvspace);
		if (error)
			return (error);
	}
	error = in_pcballoc(so, pcbinfo);
	if (error)
		return (error);
	inp = (struct inpcb *)so->so_pcb;
	inp->in6p_cksum = -1;	/* just to be sure */
	/*
	 * XXX: ugly!!
	 * IPv4 TTL initialization is necessary for an IPv6 socket as well,
	 * because the socket may be bound to an IPv6 wildcard address,
	 * which may match an IPv4-mapped IPv6 address.
	 */
	inp->inp_ip_ttl = V_ip_defttl;
	up = intoudpcb(inp);
	bzero(&up->u_start_zero, u_zero_size);
	INP_WUNLOCK(inp);
	return (0);
}

static int
udp6_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_in6 *sin6_p;
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;
	int error;
	u_char vflagsav;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp6_bind: inp == NULL"));

	if (nam->sa_family != AF_INET6)
		return (EAFNOSUPPORT);
	if (nam->sa_len != sizeof(struct sockaddr_in6))
		return (EINVAL);

	sin6_p = (struct sockaddr_in6 *)nam;

	INP_WLOCK(inp);
	INP_HASH_WLOCK(pcbinfo);
	vflagsav = inp->inp_vflag;
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6_p->sin6_addr))
			inp->inp_vflag |= INP_IPV4;
#ifdef INET
		else if (IN6_IS_ADDR_V4MAPPED(&sin6_p->sin6_addr)) {
			struct sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6_p);
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
			error = in_pcbbind(inp, &sin,
			    V_udp_bind_all_fibs ? 0 : INPBIND_FIB,
			    td->td_ucred);
			goto out;
		}
#endif
	}

	error = in6_pcbbind(inp, sin6_p, V_udp_bind_all_fibs ? 0 : INPBIND_FIB,
	    td->td_ucred);
#ifdef INET
out:
#endif
	if (error != 0)
		inp->inp_vflag = vflagsav;
	INP_HASH_WUNLOCK(pcbinfo);
	INP_WUNLOCK(inp);
	return (error);
}

static void
udp6_close(struct socket *so)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp6_close: inp == NULL"));

	INP_WLOCK(inp);
#ifdef INET
	if (inp->inp_vflag & INP_IPV4) {
		INP_WUNLOCK(inp);
		(void)udp_disconnect(so);
		return;
	}
#endif
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
		INP_HASH_WLOCK(pcbinfo);
		in6_pcbdisconnect(inp);
		INP_HASH_WUNLOCK(pcbinfo);
		soisdisconnected(so);
	}
	INP_WUNLOCK(inp);
}

static int
udp6_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct epoch_tracker et;
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;
	struct sockaddr_in6 *sin6;
	int error;
	u_char vflagsav;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp6_connect: inp == NULL"));

	sin6 = (struct sockaddr_in6 *)nam;
	if (sin6->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);
	if (sin6->sin6_len != sizeof(*sin6))
		return (EINVAL);

	/*
	 * XXXRW: Need to clarify locking of v4/v6 flags.
	 */
	INP_WLOCK(inp);
#ifdef INET
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		struct sockaddr_in sin;

		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0) {
			error = EINVAL;
			goto out;
		}
		if ((inp->inp_vflag & INP_IPV4) == 0) {
			error = EAFNOSUPPORT;
			goto out;
		}
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			goto out;
		}
		in6_sin6_2_sin(&sin, sin6);
		error = prison_remote_ip4(td->td_ucred, &sin.sin_addr);
		if (error != 0)
			goto out;
		vflagsav = inp->inp_vflag;
		inp->inp_vflag |= INP_IPV4;
		inp->inp_vflag &= ~INP_IPV6;
		NET_EPOCH_ENTER(et);
		INP_HASH_WLOCK(pcbinfo);
		error = in_pcbconnect(inp, &sin, td->td_ucred);
		INP_HASH_WUNLOCK(pcbinfo);
		NET_EPOCH_EXIT(et);
		/*
		 * If connect succeeds, mark socket as connected. If
		 * connect fails and socket is unbound, reset inp_vflag
		 * field.
		 */
		if (error == 0)
			soisconnected(so);
		else if (inp->inp_laddr.s_addr == INADDR_ANY &&
		    inp->inp_lport == 0)
			inp->inp_vflag = vflagsav;
		goto out;
	} else {
		if ((inp->inp_vflag & INP_IPV6) == 0) {
			error = EAFNOSUPPORT;
			goto out;
		}
	}
#endif
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
		error = EISCONN;
		goto out;
	}
	error = prison_remote_ip6(td->td_ucred, &sin6->sin6_addr);
	if (error != 0)
		goto out;
	vflagsav = inp->inp_vflag;
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	NET_EPOCH_ENTER(et);
	INP_HASH_WLOCK(pcbinfo);
	error = in6_pcbconnect(inp, sin6, td->td_ucred, true);
	INP_HASH_WUNLOCK(pcbinfo);
	NET_EPOCH_EXIT(et);
	/*
	 * If connect succeeds, mark socket as connected. If
	 * connect fails and socket is unbound, reset inp_vflag
	 * field.
	 */
	if (error == 0)
		soisconnected(so);
	else if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) &&
	    inp->inp_lport == 0)
		inp->inp_vflag = vflagsav;
out:
	INP_WUNLOCK(inp);
	return (error);
}

static void
udp6_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp6_detach: inp == NULL"));

	INP_WLOCK(inp);
	in_pcbfree(inp);
}

static int
udp6_disconnect(struct socket *so)
{
	struct inpcb *inp;
	struct inpcbinfo *pcbinfo;

	pcbinfo = udp_get_inpcbinfo(so->so_proto->pr_protocol);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp6_disconnect: inp == NULL"));

	INP_WLOCK(inp);
#ifdef INET
	if (inp->inp_vflag & INP_IPV4) {
		INP_WUNLOCK(inp);
		(void)udp_disconnect(so);
		return (0);
	}
#endif

	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
		INP_WUNLOCK(inp);
		return (ENOTCONN);
	}

	INP_HASH_WLOCK(pcbinfo);
	in6_pcbdisconnect(inp);
	INP_HASH_WUNLOCK(pcbinfo);
	SOCK_LOCK(so);
	so->so_state &= ~SS_ISCONNECTED;		/* XXX */
	SOCK_UNLOCK(so);
	INP_WUNLOCK(inp);
	return (0);
}

#define	UDP6_PROTOSW							\
	.pr_type =		SOCK_DGRAM,				\
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_CAPATTACH,		\
	.pr_ctloutput =		udp_ctloutput,				\
	.pr_abort =		udp6_abort,				\
	.pr_attach =		udp6_attach,				\
	.pr_bind =		udp6_bind,				\
	.pr_connect =		udp6_connect,				\
	.pr_control =		in6_control,				\
	.pr_detach =		udp6_detach,				\
	.pr_disconnect =	udp6_disconnect,			\
	.pr_peeraddr =		in6_mapped_peeraddr,			\
	.pr_send =		udp6_send,				\
	.pr_shutdown =		udp_shutdown,				\
	.pr_sockaddr =		in6_mapped_sockaddr,			\
	.pr_soreceive =		soreceive_dgram,			\
	.pr_sosend =		sosend_dgram,				\
	.pr_sosetlabel =	in_pcbsosetlabel,			\
	.pr_close =		udp6_close

struct protosw udp6_protosw = {
	.pr_protocol =		IPPROTO_UDP,
	UDP6_PROTOSW
};

struct protosw udplite6_protosw = {
	.pr_protocol =		IPPROTO_UDPLITE,
	UDP6_PROTOSW
};

static void
udp6_init(void *arg __unused)
{

	IP6PROTO_REGISTER(IPPROTO_UDP, udp6_input, udp6_ctlinput);
	IP6PROTO_REGISTER(IPPROTO_UDPLITE, udp6_input, udplite6_ctlinput);
}
SYSINIT(udp6_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, udp6_init, NULL);
