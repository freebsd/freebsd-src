/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * $FreeBSD$
 */

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
 *	@(#)ip_icmp.c	8.2 (Berkeley) 1/4/94
 */

#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_key.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/icmp6.h>
#include <netinet6/mld6_var.h>
#include <netinet/in_pcb.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/ip6protosw.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif /* INET6 */
#include <netkey/key.h>
#ifdef KEY_DEBUG
#include <netkey/key_debug.h>
#ifdef INET6
#include <netkey/key_debug6.h>
#endif /* INET6 */
#else
#define DPRINTF(lev,arg)
#define DDO(lev, stmt)
#define DP(x, y, z)
#endif /* KEY_DEBUG */
#endif /* IPSEC */

/* #include "faith.h" */

#include <net/net_osdep.h>

extern struct	domain inet6domain;
extern struct	ip6protosw inet6sw[];
extern u_char	ip6_protox[];

struct	icmp6stat icmp6stat;

extern struct	inpcbhead ripcb;
extern u_int	icmp6errratelim;

static int	icmp6_rip6_input __P((struct mbuf **, int));
static int	icmp6_ratelimit __P((const struct in6_addr *, const int, const int));
static const char	*icmp6_redirect_diag __P((struct in6_addr *,
						  struct in6_addr *,
						  struct in6_addr *));
static struct	mbuf *ni6_input __P((struct mbuf *, int));
static int	ni6_addrs __P((struct icmp6_nodeinfo *, struct mbuf *,
			       struct ifnet **));
static int	ni6_store_addrs __P((struct icmp6_nodeinfo *,
				     struct icmp6_nodeinfo *,
				     struct ifnet *, int));

#ifdef COMPAT_RFC1885
static struct	route_in6 icmp6_reflect_rt;
#endif
static struct	timeval icmp6_nextsend = {0, 0};

void
icmp6_init()
{
	mld6_init();
}

/*
 * Generate an error packet of type error in response to bad IP6 packet.
 */
void
icmp6_error(m, type, code, param)
	struct mbuf *m;
	int type, code, param;
{
	struct ip6_hdr *oip6, *nip6;
	struct icmp6_hdr *icmp6;
	u_int prep;
	int off;
	u_char nxt;

	icmp6stat.icp6s_error++;

	if (m->m_flags & M_DECRYPTED)
		goto freeit;

	oip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Multicast destination check. For unrecognized option errors,
	 * this check has already done in ip6_unknown_opt(), so we can
	 * check only for other errors.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST) ||
	     IN6_IS_ADDR_MULTICAST(&oip6->ip6_dst)) &&
	    (type != ICMP6_PACKET_TOO_BIG &&
	     (type != ICMP6_PARAM_PROB ||
	      code != ICMP6_PARAMPROB_OPTION)))
		goto freeit;

	/* Source address check. XXX: the case of anycast source? */
	if (IN6_IS_ADDR_UNSPECIFIED(&oip6->ip6_src) ||
	    IN6_IS_ADDR_MULTICAST(&oip6->ip6_src))
		goto freeit;

	/*
	 * If the erroneous packet is also an ICMP error, discard it.
	 */
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), );
	off = sizeof(struct ip6_hdr);
	nxt = oip6->ip6_nxt;
	while(1) {		/* XXX: should avoid inf. loop explicitly? */
		struct ip6_ext *ip6e;
		struct icmp6_hdr *icp;

		switch(nxt) {
		case IPPROTO_IPV6:
		case IPPROTO_IPV4:
		case IPPROTO_UDP:
		case IPPROTO_TCP:
		case IPPROTO_ESP:
		case IPPROTO_FRAGMENT:
			/*
			 * ICMPv6 error must not be fragmented.
			 * XXX: but can we trust the sender?
			 */
		default:
			/* What if unknown header followed by ICMP error? */
			goto generate;
		case IPPROTO_ICMPV6:
			IP6_EXTHDR_CHECK(m, 0, off + sizeof(struct icmp6_hdr), );
			icp = (struct icmp6_hdr *)(mtod(m, caddr_t) + off);
			if (icp->icmp6_type < ICMP6_ECHO_REQUEST
			 || icp->icmp6_type == ND_REDIRECT) {
				/*
				 * ICMPv6 error
				 * Special case: for redirect (which is
				 * informational) we must not send icmp6 error.
				 */
				icmp6stat.icp6s_canterror++;
				goto freeit;
			} else {
				/* ICMPv6 informational */
				goto generate;
			}
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
		case IPPROTO_ROUTING:
		case IPPROTO_AH:
			IP6_EXTHDR_CHECK(m, 0, off + sizeof(struct ip6_ext), );
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + off);
			if (nxt == IPPROTO_AH)
				off += (ip6e->ip6e_len + 2) << 2;
			else
				off += (ip6e->ip6e_len + 1) << 3;
			nxt = ip6e->ip6e_nxt;
			break;
		}
	}

  freeit:
	/*
	 * If we can't tell wheter or not we can generate ICMP6, free it.
	 */
	m_freem(m);
	return;

  generate:
	oip6 = mtod(m, struct ip6_hdr *); /* adjust pointer */

	/* Finally, do rate limitation check. */
	if (icmp6_ratelimit(&oip6->ip6_src, type, code)) {
		icmp6stat.icp6s_toofreq++;
		goto freeit;
	}

	/*
	 * OK, ICMP6 can be generated.
	 */

	if (m->m_pkthdr.len >= ICMPV6_PLD_MAXLEN)
		m_adj(m, ICMPV6_PLD_MAXLEN - m->m_pkthdr.len);

	prep = sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr);
	M_PREPEND(m, prep, M_DONTWAIT);
	if (m && m->m_len < prep)
		m = m_pullup(m, prep);
	if (m == NULL) {
		printf("ENOBUFS in icmp6_error %d\n", __LINE__);
		return;
	}

	nip6 = mtod(m, struct ip6_hdr *);
	nip6->ip6_src  = oip6->ip6_src;
	nip6->ip6_dst  = oip6->ip6_dst;

	if (IN6_IS_SCOPE_LINKLOCAL(&oip6->ip6_src))
		oip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(&oip6->ip6_dst))
		oip6->ip6_dst.s6_addr16[1] = 0;

	icmp6 = (struct icmp6_hdr *)(nip6 + 1);
	icmp6->icmp6_type = type;
	icmp6->icmp6_code = code;
	icmp6->icmp6_pptr = htonl((u_int32_t)param);

	icmp6stat.icp6s_outhist[type]++;
	icmp6_reflect(m, sizeof(struct ip6_hdr)); /*header order: IPv6 - ICMPv6*/
}

/*
 * Process a received ICMP6 message.
 */
int
icmp6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp, *n;
	struct ip6_hdr *ip6, *nip6;
	struct icmp6_hdr *icmp6, *nicmp6;
	int off = *offp;
	int icmp6len = m->m_pkthdr.len - *offp;
	int code, sum, noff;
	struct sockaddr_in6 icmp6src;

	IP6_EXTHDR_CHECK(m, off, sizeof(struct icmp6_hdr), IPPROTO_DONE);
	/* m might change if M_LOOP. So, call mtod after this */

	/*
	 * Locate icmp6 structure in mbuf, and check
	 * that not corrupted and of at least minimum length
	 */

	ip6 = mtod(m, struct ip6_hdr *);
	if (icmp6len < sizeof(struct icmp6_hdr)) {
		icmp6stat.icp6s_tooshort++;
		goto freeit;
	}

	/*
	 * calculate the checksum
	 */

	icmp6 = (struct icmp6_hdr *)((caddr_t)ip6 + off);
	code = icmp6->icmp6_code;

	if ((sum = in6_cksum(m, IPPROTO_ICMPV6, off, icmp6len)) != 0) {
		log(LOG_ERR,
		    "ICMP6 checksum error(%d|%x) %s\n",
		    icmp6->icmp6_type,
		    sum,
		    ip6_sprintf(&ip6->ip6_src));
		icmp6stat.icp6s_checksum++;
		goto freeit;
	}

#if defined(NFAITH) && 0 < NFAITH
	if (m->m_pkthdr.rcvif && m->m_pkthdr.rcvif->if_type == IFT_FAITH) {
		/*
		 * Deliver very specific ICMP6 type only.
		 * This is important to deilver TOOBIG.  Otherwise PMTUD
		 * will not work.
		 */
		switch (icmp6->icmp6_type) {
		case ICMP6_DST_UNREACH:
		case ICMP6_PACKET_TOO_BIG:
		case ICMP6_TIME_EXCEEDED:
			break;
		default:
			goto freeit;
		}
	}
#endif

#ifdef IPSEC
	/* drop it if it does not match the default policy */
	if (ipsec6_in_reject(m, NULL)) {
		ipsecstat.in_polvio++;
		goto freeit;
	}
#endif

	icmp6stat.icp6s_inhist[icmp6->icmp6_type]++;
	icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_msg);
	if (icmp6->icmp6_type < ICMP6_INFOMSG_MASK)
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_error);

	switch (icmp6->icmp6_type) {

	case ICMP6_DST_UNREACH:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_dstunreach);
		switch (code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			code = PRC_UNREACH_NET;
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_adminprohib);
		case ICMP6_DST_UNREACH_ADDR:
			code = PRC_UNREACH_HOST;
			break;
		case ICMP6_DST_UNREACH_NOTNEIGHBOR:
			code = PRC_UNREACH_SRCFAIL;
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			code = PRC_UNREACH_PORT;
			break;
		default:
			goto badcode;
		}
		goto deliver;
		break;

	case ICMP6_PACKET_TOO_BIG:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_pkttoobig);
		if (code != 0)
			goto badcode;
	    {
		u_int mtu = ntohl(icmp6->icmp6_mtu);
		struct rtentry *rt = NULL;
		struct sockaddr_in6 sin6;

		code = PRC_MSGSIZE;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = PF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr = ((struct ip6_hdr *)(icmp6 + 1))->ip6_dst;
		rt = rtalloc1((struct sockaddr *)&sin6, 0,
			RTF_CLONING | RTF_PRCLONING);
		if (rt && (rt->rt_flags & RTF_HOST)
		    && !(rt->rt_rmx.rmx_locks & RTV_MTU)) {
			if (mtu < IPV6_MMTU) {
				/* xxx */
				rt->rt_rmx.rmx_locks |= RTV_MTU;
			} else if (mtu < rt->rt_ifp->if_mtu &&
				   rt->rt_rmx.rmx_mtu > mtu) {
				rt->rt_rmx.rmx_mtu = mtu;
			}
		}
		if (rt)
			RTFREE(rt);

		goto deliver;
	    }
		break;

	case ICMP6_TIME_EXCEEDED:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_timeexceed);
		switch (code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			code += PRC_TIMXCEED_INTRANS;
			break;
		default:
			goto badcode;
		}
		goto deliver;
		break;

	case ICMP6_PARAM_PROB:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_paramprob);
		switch (code) {
		case ICMP6_PARAMPROB_NEXTHEADER:
			code = PRC_UNREACH_PROTOCOL;
			break;
		case ICMP6_PARAMPROB_HEADER:
		case ICMP6_PARAMPROB_OPTION:
			code = PRC_PARAMPROB;
			break;
		default:
			goto badcode;
		}
		goto deliver;
		break;

	case ICMP6_ECHO_REQUEST:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_echo);
		if (code != 0)
			goto badcode;
		if ((n = m_copy(m, 0, M_COPYALL)) == NULL) {
			/* Give up remote */
			break;
		}
		if (n->m_flags & M_EXT) {
			int gap, move;
			struct mbuf *n0 = n;

			/*
			 * Prepare an internal mbuf. m_pullup() doesn't
			 * always copy the length we specified.
			 */
			MGETHDR(n, M_DONTWAIT, n0->m_type);
			if (n == NULL) {
				/* Give up remote */
				m_freem(n0);
				break;
			}
			M_COPY_PKTHDR(n, n0);
			n0->m_flags &= ~M_PKTHDR;
			n->m_next = n0;
			/*
			 * Copy IPv6 and ICMPv6 only.
			 */
			nip6 = mtod(n, struct ip6_hdr *);
			bcopy(ip6, nip6, sizeof(struct ip6_hdr));
			nicmp6 = (struct icmp6_hdr *)(nip6 + 1);
			bcopy(icmp6, nicmp6, sizeof(struct icmp6_hdr));
			/*
			 * Adjust mbuf. ip6_plen will be adjusted.
			 */
			noff = sizeof(struct ip6_hdr);
			n->m_len = noff + sizeof(struct icmp6_hdr);
			move = off + sizeof(struct icmp6_hdr);
			n0->m_len -= move;
			n0->m_data += move;
			gap = off - noff;
			n->m_pkthdr.len -= gap;
		} else {
			nip6 = mtod(n, struct ip6_hdr *);
			nicmp6 = (struct icmp6_hdr *)((caddr_t)nip6 + off);
			noff = off;
		}
		nicmp6->icmp6_type = ICMP6_ECHO_REPLY;
		nicmp6->icmp6_code = 0;
		if (n) {
			icmp6stat.icp6s_reflect++;
			icmp6stat.icp6s_outhist[ICMP6_ECHO_REPLY]++;
			icmp6_reflect(n, noff);
		}
		break;

	case ICMP6_ECHO_REPLY:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_echoreply);
		if (code != 0)
			goto badcode;
		break;

	case MLD6_LISTENER_QUERY:
	case MLD6_LISTENER_REPORT:
		if (icmp6len < sizeof(struct mld6_hdr))
			goto badlen;
		if (icmp6->icmp6_type == MLD6_LISTENER_QUERY) /* XXX: ugly... */
			icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mldquery);
		else
			icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mldreport);
		IP6_EXTHDR_CHECK(m, off, icmp6len, IPPROTO_DONE);
		mld6_input(m, off);
		/* m stays. */
		break;

	case MLD6_LISTENER_DONE:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mlddone);
		if (icmp6len < sizeof(struct mld6_hdr))	/* necessary? */
			goto badlen;
		break;		/* nothing to be done in kernel */

	case MLD6_MTRACE_RESP:
	case MLD6_MTRACE:
		/* XXX: these two are experimental. not officially defind. */
		/* XXX: per-interface statistics? */
		break;		/* just pass it to the userland daemon */

	case ICMP6_WRUREQUEST:	/* ICMP6_FQDN_QUERY */
	    {
		enum { WRU, FQDN } mode;

		if (code != 0)
			goto badcode;
		if (icmp6len == sizeof(struct icmp6_hdr) + 4)
			mode = WRU;
		else if (icmp6len >= sizeof(struct icmp6_hdr) + 8) /* XXX */
			mode = FQDN;
		else
			goto badlen;

#define hostnamelen	strlen(hostname)
		if (mode == FQDN) {
			IP6_EXTHDR_CHECK(m, off, sizeof(struct icmp6_nodeinfo),
					 IPPROTO_DONE);
			n = ni6_input(m, off);
			noff = sizeof(struct ip6_hdr);
		} else {
			u_char *p;

			MGETHDR(n, M_DONTWAIT, m->m_type);
			if (n == NULL) {
				/* Give up remote */
				break;
			}
			/*
			 * Copy IPv6 and ICMPv6 only.
			 */
			nip6 = mtod(n, struct ip6_hdr *);
			bcopy(ip6, nip6, sizeof(struct ip6_hdr));
			nicmp6 = (struct icmp6_hdr *)(nip6 + 1);
			bcopy(icmp6, nicmp6, sizeof(struct icmp6_hdr));
			p = (u_char *)(nicmp6 + 1);
			bzero(p, 4);
			bcopy(hostname, p + 4, hostnamelen);
			noff = sizeof(struct ip6_hdr);
			M_COPY_PKTHDR(n, m); /* just for recvif */
			n->m_pkthdr.len = n->m_len = sizeof(struct ip6_hdr) +
				sizeof(struct icmp6_hdr) + 4 + hostnamelen;
			nicmp6->icmp6_type = ICMP6_WRUREPLY;
			nicmp6->icmp6_code = 0;
		}
#undef hostnamelen
		if (n) {
			icmp6stat.icp6s_reflect++;
			icmp6stat.icp6s_outhist[ICMP6_WRUREPLY]++;
			icmp6_reflect(n, noff);
		}
		break;
	    }

	case ICMP6_WRUREPLY:
		if (code != 0)
			goto badcode;
		break;

	case ND_ROUTER_SOLICIT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_routersolicit);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_router_solicit))
			goto badlen;
		IP6_EXTHDR_CHECK(m, off, icmp6len, IPPROTO_DONE);
		nd6_rs_input(m, off, icmp6len);
		/* m stays. */
		break;

	case ND_ROUTER_ADVERT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_routeradvert);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_router_advert))
			goto badlen;
		IP6_EXTHDR_CHECK(m, off, icmp6len, IPPROTO_DONE);
		nd6_ra_input(m, off, icmp6len);
		/* m stays. */
		break;

	case ND_NEIGHBOR_SOLICIT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_neighborsolicit);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_neighbor_solicit))
			goto badlen;
		IP6_EXTHDR_CHECK(m, off, icmp6len, IPPROTO_DONE);
		nd6_ns_input(m, off, icmp6len);
		/* m stays. */
		break;

	case ND_NEIGHBOR_ADVERT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_neighboradvert);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_neighbor_advert))
			goto badlen;
		IP6_EXTHDR_CHECK(m, off, icmp6len, IPPROTO_DONE);
		nd6_na_input(m, off, icmp6len);
		/* m stays. */
		break;

	case ND_REDIRECT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_redirect);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_redirect))
			goto badlen;
		icmp6_redirect_input(m, off);
		/* m stays. */
		break;

	case ICMP6_ROUTER_RENUMBERING:
		if (code != ICMP6_ROUTER_RENUMBERING_COMMAND &&
		    code != ICMP6_ROUTER_RENUMBERING_RESULT)
			goto badcode;
		if (icmp6len < sizeof(struct icmp6_router_renum))
			goto badlen;
		break;

	default:
		printf("icmp6_input: unknown type %d(src=%s, dst=%s, ifid=%d)\n",
		       icmp6->icmp6_type, ip6_sprintf(&ip6->ip6_src),
		       ip6_sprintf(&ip6->ip6_dst),
		       m->m_pkthdr.rcvif ? m->m_pkthdr.rcvif->if_index : 0);
		if (icmp6->icmp6_type < ICMP6_ECHO_REQUEST) {
			/* ICMPv6 error: MUST deliver it by spec... */
			code = PRC_NCMDS;
			/* deliver */
		} else {
			/* ICMPv6 informational: MUST not deliver */
			break;
		}
	deliver:
		if (icmp6len < sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr)) {
			icmp6stat.icp6s_tooshort++;
			goto freeit;
		}
		IP6_EXTHDR_CHECK(m, off,
			sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr),
			IPPROTO_DONE);
		icmp6 = (struct icmp6_hdr *)(mtod(m, caddr_t) + off);
		bzero(&icmp6src, sizeof(icmp6src));
		icmp6src.sin6_len = sizeof(struct sockaddr_in6);
		icmp6src.sin6_family = AF_INET6;
		icmp6src.sin6_addr = ((struct ip6_hdr *)(icmp6 + 1))->ip6_dst;

		/* Detect the upper level protocol */
	    {
		void (*ctlfunc) __P((int, struct sockaddr *, void *));
		struct ip6_hdr *eip6 = (struct ip6_hdr *)(icmp6 + 1);
		u_int8_t nxt = eip6->ip6_nxt;
		int eoff = off + sizeof(struct icmp6_hdr) +
			sizeof(struct ip6_hdr);
		struct ip6ctlparam ip6cp;

		while (1) { /* XXX: should avoid inf. loop explicitly? */
			struct ip6_ext *eh;

			switch(nxt) {
			case IPPROTO_ESP:
			case IPPROTO_NONE:
				goto passit;
			case IPPROTO_HOPOPTS:
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
			case IPPROTO_AH:
			case IPPROTO_FRAGMENT:
				IP6_EXTHDR_CHECK(m, 0, eoff +
						 sizeof(struct ip6_ext),
						 IPPROTO_DONE);
				eh = (struct ip6_ext *)(mtod(m, caddr_t)
							+ eoff);
				if (nxt == IPPROTO_AH)
					eoff += (eh->ip6e_len + 2) << 2;
				else if (nxt == IPPROTO_FRAGMENT)
					eoff += sizeof(struct ip6_frag);
				else
					eoff += (eh->ip6e_len + 1) << 3;
				nxt = eh->ip6e_nxt;
				break;
			default:
				goto notify;
			}
		}
	    notify:
		icmp6 = (struct icmp6_hdr *)(mtod(m, caddr_t) + off);
		ctlfunc = (void (*) __P((int, struct sockaddr *, void *)))
			(inet6sw[ip6_protox[nxt]].pr_ctlinput);
		if (ctlfunc) {
			ip6cp.ip6c_m = m;
			ip6cp.ip6c_ip6 = (struct ip6_hdr *)(icmp6 + 1);
			ip6cp.ip6c_off = eoff;
			(*ctlfunc)(code, (struct sockaddr *)&icmp6src, &ip6cp);
		}
	    }
		break;

	badcode:
		icmp6stat.icp6s_badcode++;
		break;

	badlen:
		icmp6stat.icp6s_badlen++;
		break;
	}

 passit:
	icmp6_rip6_input(&m, *offp);
	return IPPROTO_DONE;

 freeit:
	m_freem(m);
	return IPPROTO_DONE;
}

/*
 * Process a Node Information Query
 */
#define	hostnamelen	strlen(hostname)
#ifndef offsetof		/* XXX */
#define	offsetof(type, member)	((size_t)(&((type *)0)->member))
#endif

static struct mbuf *
ni6_input(m, off)
	struct mbuf *m;
	int off;
{
	struct icmp6_nodeinfo *ni6 =
		(struct icmp6_nodeinfo *)(mtod(m, caddr_t) + off), *nni6;
	struct mbuf *n = NULL;
	u_int16_t qtype = ntohs(ni6->ni_qtype);
	int replylen = sizeof(struct ip6_hdr) + sizeof(struct icmp6_nodeinfo);
	struct ni_reply_fqdn *fqdn;
	int addrs;		/* for NI_QTYPE_NODEADDR */
	struct ifnet *ifp = NULL; /* for NI_QTYPE_NODEADDR */

	switch(qtype) {
	 case NI_QTYPE_NOOP:
		 break;		/* no reply data */
	 case NI_QTYPE_SUPTYPES:
		 goto bad;	/* xxx: to be implemented */
		 break;
	 case NI_QTYPE_FQDN:
		 replylen += offsetof(struct ni_reply_fqdn, ni_fqdn_name) +
			 hostnamelen;
		 break;
	 case NI_QTYPE_NODEADDR:
		 addrs = ni6_addrs(ni6, m, &ifp);
		 if ((replylen += addrs * sizeof(struct in6_addr)) > MCLBYTES)
			 replylen = MCLBYTES; /* XXX: we'll truncate later */

		 break;
	 default:
		 /*
		  * XXX: We must return a reply with the ICMP6 code
		  * `unknown Qtype' in this case. However we regard the case
		  * as an FQDN query for backward compatibility.
		  * Older versions set a random value to this field,
		  * so it rarely varies in the defined qtypes.
		  * But the mechanism is not reliable...
		  * maybe we should obsolete older versions.
		  */
		 qtype = NI_QTYPE_FQDN;
		 replylen += offsetof(struct ni_reply_fqdn, ni_fqdn_name) +
			 hostnamelen;
		 break;
	}

	/* allocate a mbuf to reply. */
	MGETHDR(n, M_DONTWAIT, m->m_type);
	if (n == NULL)
		return(NULL);
	M_COPY_PKTHDR(n, m); /* just for recvif */
	if (replylen > MHLEN) {
		if (replylen > MCLBYTES)
			 /*
			  * XXX: should we try to allocate more? But MCLBYTES is
			  * probably much larger than IPV6_MMTU...
			  */
			goto bad;
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			goto bad;
		}
	}
	n->m_pkthdr.len = n->m_len = replylen;

	/* copy mbuf header and IPv6 + Node Information base headers */
	bcopy(mtod(m, caddr_t), mtod(n, caddr_t), sizeof(struct ip6_hdr));
	nni6 = (struct icmp6_nodeinfo *)(mtod(n, struct ip6_hdr *) + 1);
	bcopy(mtod(m, caddr_t) + off, (caddr_t)nni6, sizeof(struct icmp6_nodeinfo));

	/* qtype dependent procedure */
	switch (qtype) {
	 case NI_QTYPE_NOOP:
		 nni6->ni_flags = 0;
		 break;
	 case NI_QTYPE_SUPTYPES:
		 goto bad;	/* xxx: to be implemented */
		 break;
	 case NI_QTYPE_FQDN:
		 if (hostnamelen > 255) { /* XXX: rare case, but may happen */
			 printf("ni6_input: "
				"hostname length(%d) is too large for reply\n",
				hostnamelen);
			 goto bad;
		 }
		 fqdn = (struct ni_reply_fqdn *)(mtod(n, caddr_t) +
						 sizeof(struct ip6_hdr) +
						 sizeof(struct icmp6_nodeinfo));
		 nni6->ni_flags = 0; /* XXX: meaningless TTL */
		 fqdn->ni_fqdn_ttl = 0;	/* ditto. */
		 fqdn->ni_fqdn_namelen = hostnamelen;
		 bcopy(hostname, &fqdn->ni_fqdn_name[0], hostnamelen);
		 break;
	 case NI_QTYPE_NODEADDR:
	 {
		 int lenlim, copied;

		 if (n->m_flags & M_EXT)
			 lenlim = MCLBYTES - sizeof(struct ip6_hdr) -
				 sizeof(struct icmp6_nodeinfo);
		 else
			 lenlim = MHLEN - sizeof(struct ip6_hdr) -
				 sizeof(struct icmp6_nodeinfo);
		 copied = ni6_store_addrs(ni6, nni6, ifp, lenlim);
		 /* XXX: reset mbuf length */
		 n->m_pkthdr.len = n->m_len = sizeof(struct ip6_hdr) +
			 sizeof(struct icmp6_nodeinfo) + copied;
		 break;
	 }
	 default:
		 break;		/* XXX impossible! */
	}

	nni6->ni_type = ICMP6_NI_REPLY;
	nni6->ni_code = ICMP6_NI_SUCESS;
	return(n);

  bad:
	if (n)
		m_freem(n);
	return(NULL);
}
#undef hostnamelen

/*
 * calculate the number of addresses to be returned in the node info reply.
 */
static int
ni6_addrs(ni6, m, ifpp)
	struct icmp6_nodeinfo *ni6;
	struct mbuf *m;
	struct ifnet **ifpp;
{
	register struct ifnet *ifp;
	register struct in6_ifaddr *ifa6;
	register struct ifaddr *ifa;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	int addrs = 0, addrsofif, iffound = 0;

	for (ifp = TAILQ_FIRST(&ifnet); ifp; ifp = TAILQ_NEXT(ifp, if_list))
	{
		addrsofif = 0;
		for (ifa = ifp->if_addrlist.tqh_first; ifa;
		     ifa = ifa->ifa_list.tqe_next)
		{
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ifa6 = (struct in6_ifaddr *)ifa;

			if (!(ni6->ni_flags & NI_NODEADDR_FLAG_ALL) &&
			    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
					       &ifa6->ia_addr.sin6_addr))
				iffound = 1;

			if (ifa6->ia6_flags & IN6_IFF_ANYCAST)
				continue; /* we need only unicast addresses */

			if ((ni6->ni_flags & (NI_NODEADDR_FLAG_LINKLOCAL |
					      NI_NODEADDR_FLAG_SITELOCAL |
					      NI_NODEADDR_FLAG_GLOBAL)) == 0)
				continue;

			/* What do we have to do about ::1? */
			switch(in6_addrscope(&ifa6->ia_addr.sin6_addr)) {
			 case IPV6_ADDR_SCOPE_LINKLOCAL:
				if (ni6->ni_flags & NI_NODEADDR_FLAG_LINKLOCAL)
					addrsofif++;
				break;
			 case IPV6_ADDR_SCOPE_SITELOCAL:
				if (ni6->ni_flags & NI_NODEADDR_FLAG_SITELOCAL)
					addrsofif++;
				break;
			 case IPV6_ADDR_SCOPE_GLOBAL:
				 if (ni6->ni_flags & NI_NODEADDR_FLAG_GLOBAL)
					 addrsofif++;
				 break;
			 default:
				 continue;
			}
		}
		if (iffound) {
			*ifpp = ifp;
			return(addrsofif);
		}

		addrs += addrsofif;
	}

	return(addrs);
}

static int
ni6_store_addrs(ni6, nni6, ifp0, resid)
	struct icmp6_nodeinfo *ni6, *nni6;
	struct ifnet *ifp0;
	int resid;
{
	register struct ifnet *ifp = ifp0 ? ifp0 : TAILQ_FIRST(&ifnet);
	register struct in6_ifaddr *ifa6;
	register struct ifaddr *ifa;
	int docopy, copied = 0;
	u_char *cp = (u_char *)(nni6 + 1);

	if (ifp0 == NULL && !(ni6->ni_flags & NI_NODEADDR_FLAG_ALL))
		return(0);	/* needless to copy */

	for (; ifp; ifp = TAILQ_NEXT(ifp, if_list))
	{
		for (ifa = ifp->if_addrlist.tqh_first; ifa;
		     ifa = ifa->ifa_list.tqe_next)
		{
			docopy = 0;

			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ifa6 = (struct in6_ifaddr *)ifa;

			if (ifa6->ia6_flags & IN6_IFF_ANYCAST) {
				/* just experimental. not in the spec. */
				if (ni6->ni_flags & NI_NODEADDR_FLAG_ANYCAST)
					docopy = 1;
				else
					continue;
			} else {	/* unicast address */
				if (ni6->ni_flags & NI_NODEADDR_FLAG_ANYCAST)
					continue;
				else
					docopy = 1;
			}

			/* What do we have to do about ::1? */
			switch(in6_addrscope(&ifa6->ia_addr.sin6_addr)) {
			 case IPV6_ADDR_SCOPE_LINKLOCAL:
				if (ni6->ni_flags & NI_NODEADDR_FLAG_LINKLOCAL)
					docopy = 1;
				break;
			 case IPV6_ADDR_SCOPE_SITELOCAL:
				if (ni6->ni_flags & NI_NODEADDR_FLAG_SITELOCAL)
					docopy = 1;
				break;
			 case IPV6_ADDR_SCOPE_GLOBAL:
				 if (ni6->ni_flags & NI_NODEADDR_FLAG_GLOBAL)
					 docopy = 1;
				 break;
			 default:
				 continue;
			}

			if (docopy) {
				if (resid < sizeof(struct in6_addr)) {
					/*
					 * We give up much more copy.
					 * Set the truncate flag and return.
					 */
					nni6->ni_flags |=
						NI_NODEADDR_FLAG_TRUNCATE;
					return(copied);
				}
				bcopy(&ifa6->ia_addr.sin6_addr, cp,
				      sizeof(struct in6_addr));
				/* XXX: KAME link-local hack; remove ifindex */
				if (IN6_IS_ADDR_LINKLOCAL(&ifa6->ia_addr.sin6_addr))
					((struct in6_addr *)cp)->s6_addr16[1] = 0;
				cp += sizeof(struct in6_addr);
				resid -= sizeof(struct in6_addr);
				copied += sizeof(struct in6_addr);
			}
		}
		if (ifp0)	/* we need search only on the specified IF */
			break;
	}

	return(copied);
}

/*
 * XXX almost dup'ed code with rip6_input.
 */
static int
icmp6_rip6_input(mp, off)
	struct	mbuf **mp;
	int	off;
{
	struct mbuf *m = *mp;
	register struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	register struct in6pcb *in6p;
	struct in6pcb *last = NULL;
	struct sockaddr_in6 rip6src;
	struct icmp6_hdr *icmp6;
	struct mbuf *opts = NULL;

	/* this is assumed to be safe. */
	icmp6 = (struct icmp6_hdr *)((caddr_t)ip6 + off);

	bzero(&rip6src, sizeof(rip6src));
	rip6src.sin6_len = sizeof(struct sockaddr_in6);
	rip6src.sin6_family = AF_INET6;
	rip6src.sin6_addr = ip6->ip6_src;
	if (IN6_IS_SCOPE_LINKLOCAL(&rip6src.sin6_addr))
		rip6src.sin6_addr.s6_addr16[1] = 0;
	if (m->m_pkthdr.rcvif) {
		if (IN6_IS_SCOPE_LINKLOCAL(&rip6src.sin6_addr))
			rip6src.sin6_scope_id = m->m_pkthdr.rcvif->if_index;
		else
			rip6src.sin6_scope_id = 0;
	} else
		rip6src.sin6_scope_id = 0;

	LIST_FOREACH(in6p, &ripcb, inp_list)
	{
		if ((in6p->inp_vflag & INP_IPV6) == NULL)
			continue;
		if (in6p->in6p_ip6_nxt != IPPROTO_ICMPV6)
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) &&
		   !IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &ip6->ip6_dst))
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) &&
		   !IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, &ip6->ip6_src))
			continue;
		if (in6p->in6p_icmp6filt
		    && ICMP6_FILTER_WILLBLOCK(icmp6->icmp6_type,
				 in6p->in6p_icmp6filt))
			continue;
		if (last) {
			struct	mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (last->in6p_flags & IN6P_CONTROLOPTS)
					ip6_savecontrol(last, &opts, ip6, n);
				/* strip intermediate headers */
				m_adj(n, off);
				if (sbappendaddr(&last->in6p_socket->so_rcv,
						 (struct sockaddr *)&rip6src,
						 n, opts) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					if (opts)
						m_freem(opts);
				} else
					sorwakeup(last->in6p_socket);
				opts = NULL;
			}
		}
		last = in6p;
	}
	if (last) {
		if (last->in6p_flags & IN6P_CONTROLOPTS)
			ip6_savecontrol(last, &opts, ip6, m);
		/* strip intermediate headers */
		m_adj(m, off);
		if (sbappendaddr(&last->in6p_socket->so_rcv,
				(struct sockaddr *)&rip6src, m, opts) == 0) {
			m_freem(m);
			if (opts)
				m_freem(opts);
		} else
			sorwakeup(last->in6p_socket);
	} else {
		m_freem(m);
		ip6stat.ip6s_delivered--;
	}
	return IPPROTO_DONE;
}

/*
 * Reflect the ip6 packet back to the source.
 * The caller MUST check if the destination is multicast or not.
 * This function is usually called with a unicast destination which
 * can be safely the source of the reply packet. But some exceptions
 * exist(e.g. ECHOREPLY, PATCKET_TOOBIG, "10" in OPTION type).
 * ``off'' points to the icmp6 header, counted from the top of the mbuf.
 */
void
icmp6_reflect(m, off)
	struct	mbuf *m;
	size_t off;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct icmp6_hdr *icmp6;
	struct in6_ifaddr *ia;
	struct in6_addr t, *src = 0;
	int plen = m->m_pkthdr.len - sizeof(struct ip6_hdr);
	int type, code;
	struct ifnet *outif = NULL;
#ifdef COMPAT_RFC1885
	int mtu = IPV6_MMTU;
	struct sockaddr_in6 *sin6 = &icmp6_reflect_rt.ro_dst;
#endif

	/*
	 * If there are extra headers between IPv6 and ICMPv6, strip
	 * off that header first.
	 */
	if (off != sizeof(struct ip6_hdr)) {
		size_t siz;

		/* sanity checks */
		if (off < sizeof(struct ip6_hdr)) {
			printf("sanity fail: off=%x, sizeof(ip6)=%x in %s:%d\n",
			       (unsigned int)off,
			       (unsigned int)sizeof(struct ip6_hdr),
			       __FILE__, __LINE__);
			goto bad;
		}
		siz = off - sizeof(struct ip6_hdr);
		if (plen < siz) {
			printf("sanity fail: siz=%x, payloadlen=%x in %s:%d\n",
			       (unsigned int)siz, plen, __FILE__, __LINE__);
			goto bad;
		}
		IP6_EXTHDR_CHECK(m, 0, off, /*nothing*/);
		IP6_EXTHDR_CHECK(m, off, sizeof(struct icmp6_hdr), /*nothing*/);

		ovbcopy((caddr_t)ip6,
			(caddr_t)(mtod(m, u_char *) + siz),
			sizeof(struct ip6_hdr));
		m->m_data += siz;
		m->m_len -= siz;
		m->m_pkthdr.len -= siz;
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		plen -= siz;
	}

	icmp6 = (struct icmp6_hdr *)(ip6 + 1);
	type = icmp6->icmp6_type; /* keep type for statistics */
	code = icmp6->icmp6_code; /* ditto. */

	t = ip6->ip6_dst;
	/*
	 * ip6_input() drops a packet if its src is multicast.
	 * So, the src is never multicast.
	 */
	ip6->ip6_dst = ip6->ip6_src;

	/* XXX hack for link-local addresses */
	if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] =
			htons(m->m_pkthdr.rcvif->if_index);
	if (IN6_IS_ADDR_LINKLOCAL(&t))
		t.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);

#ifdef COMPAT_RFC1885
	/*
	 * xxx guess MTU
	 * RFC 1885 requires that echo reply should be truncated if it
	 * does not fit in with (return) path MTU, but the description was
	 * removed in the new spec.
	 */
	if (icmp6_reflect_rt.ro_rt == 0 ||
	    ! (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &ip6->ip6_dst))) {
		if (icmp6_reflect_rt.ro_rt) {
			RTFREE(icmp6_reflect_rt.ro_rt);
			icmp6_reflect_rt.ro_rt = 0;
		}
		bzero(sin6, sizeof(*sin6));
		sin6->sin6_family = PF_INET6;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		sin6->sin6_addr = ip6->ip6_dst;

		rtalloc_ign((struct route *)&icmp6_reflect_rt.ro_rt,
			    RTF_PRCLONING);
	}

	if (icmp6_reflect_rt.ro_rt == 0)
		goto bad;

	if ((icmp6_reflect_rt.ro_rt->rt_flags & RTF_HOST)
	    && mtu < icmp6_reflect_rt.ro_rt->rt_ifp->if_mtu)
		mtu = icmp6_reflect_rt.ro_rt->rt_rmx.rmx_mtu;

	if (mtu < m->m_pkthdr.len) {
		plen -= (m->m_pkthdr.len - mtu);
		m_adj(m, mtu - m->m_pkthdr.len);
	}
#endif
	/*
	 * If the incoming packet was addressed directly to us(i.e. unicast),
	 * use dst as the src for the reply.
	 */
	for (ia = in6_ifaddr; ia; ia = ia->ia_next)
		if (IN6_ARE_ADDR_EQUAL(&t, &ia->ia_addr.sin6_addr) &&
		    (ia->ia6_flags & IN6_IFF_ANYCAST) == 0) {
			src = &t;
			break;
		}
	if (ia == NULL && IN6_IS_ADDR_LINKLOCAL(&t) && (m->m_flags & M_LOOP)) {
		/*
		 * This is the case if the dst is our link-local address
		 * and the sender is also ourseleves.
		 */
		src = &t;
	}

	if (src == 0)
		/*
		 * We have not multicast routing yet. So this case matches
		 * to our multicast, our anycast or not to our unicast.
		 * Select a source address which has the same scope.
		 */
		if ((ia = in6_ifawithscope(m->m_pkthdr.rcvif, &t)) != 0)
			src = &IA6_SIN6(ia)->sin6_addr;

	if (src == 0)
		goto bad;

	ip6->ip6_src = *src;

	ip6->ip6_flow = 0;
	ip6->ip6_vfc = IPV6_VERSION;
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	if (m->m_pkthdr.rcvif) {
		/* XXX: This may not be the outgoing interface */
		ip6->ip6_hlim = nd_ifinfo[m->m_pkthdr.rcvif->if_index].chlim;
	}

	icmp6->icmp6_cksum = 0;
	icmp6->icmp6_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					sizeof(struct ip6_hdr), plen);

	/*
	 * xxx option handling
	 */

	m->m_flags &= ~(M_BCAST|M_MCAST);
#ifdef IPSEC
	m->m_pkthdr.rcvif = NULL;
#endif /*IPSEC*/

#ifdef COMPAT_RFC1885
	ip6_output(m, NULL, &icmp6_reflect_rt, 0, NULL, &outif);
#else
	ip6_output(m, NULL, NULL, 0, NULL, &outif);
#endif
	if (outif)
		icmp6_ifoutstat_inc(outif, type, code);

	return;

 bad:
	m_freem(m);
	return;
}

void
icmp6_fasttimo()
{
	mld6_fasttimeo();
}

static const char *
icmp6_redirect_diag(src6, dst6, tgt6)
	struct in6_addr *src6;
	struct in6_addr *dst6;
	struct in6_addr *tgt6;
{
	static char buf[1024];
	snprintf(buf, sizeof(buf), "(src=%s dst=%s tgt=%s)",
		ip6_sprintf(src6), ip6_sprintf(dst6), ip6_sprintf(tgt6));
	return buf;
}

void
icmp6_redirect_input(m, off)
	register struct mbuf *m;
	int off;
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_redirect *nd_rd = (struct nd_redirect *)((caddr_t)ip6 + off);
	int icmp6len = ntohs(ip6->ip6_plen);
	char *lladdr = NULL;
	int lladdrlen = 0;
	u_char *redirhdr = NULL;
	int redirhdrlen = 0;
	struct rtentry *rt = NULL;
	int is_router;
	int is_onlink;
	struct in6_addr src6 = ip6->ip6_src;
	struct in6_addr redtgt6 = nd_rd->nd_rd_target;
	struct in6_addr reddst6 = nd_rd->nd_rd_dst;
	union nd_opts ndopts;

	if (!m || !ifp)
		return;

	/* XXX if we are router, we don't update route by icmp6 redirect */
	if (ip6_forwarding)
		return;
	if (!icmp6_rediraccept)
		return;

	if (IN6_IS_ADDR_LINKLOCAL(&redtgt6))
		redtgt6.s6_addr16[1] = htons(ifp->if_index);
	if (IN6_IS_ADDR_LINKLOCAL(&reddst6))
		reddst6.s6_addr16[1] = htons(ifp->if_index);

	/* validation */
	if (!IN6_IS_ADDR_LINKLOCAL(&src6)) {
		log(LOG_ERR,
			"ICMP6 redirect sent from %s rejected; "
			"must be from linklocal\n", ip6_sprintf(&src6));
		return;
	}
	if (ip6->ip6_hlim != 255) {
		log(LOG_ERR,
			"ICMP6 redirect sent from %s rejected; "
			"hlim=%d (must be 255)\n",
			ip6_sprintf(&src6), ip6->ip6_hlim);
		return;
	}
    {
	/* ip6->ip6_src must be equal to gw for icmp6->icmp6_reddst */
	struct sockaddr_in6 sin6;
	struct in6_addr *gw6;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&reddst6, &sin6.sin6_addr, sizeof(reddst6));
	rt = rtalloc1((struct sockaddr *)&sin6, 0, 0UL);
	if (rt) {
		gw6 = &(((struct sockaddr_in6 *)rt->rt_gateway)->sin6_addr);
		if (bcmp(&src6, gw6, sizeof(struct in6_addr)) != 0) {
			log(LOG_ERR,
				"ICMP6 redirect rejected; "
				"not equal to gw-for-src=%s (must be same): "
				"%s\n",
				ip6_sprintf(gw6),
				icmp6_redirect_diag(&src6, &reddst6, &redtgt6));
			RTFREE(rt);
			return;
		}
	} else {
		log(LOG_ERR,
			"ICMP6 redirect rejected; "
			"no route found for redirect dst: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6));
		return;
	}
	RTFREE(rt);
	rt = NULL;
    }
	if (IN6_IS_ADDR_MULTICAST(&reddst6)) {
		log(LOG_ERR,
			"ICMP6 redirect rejected; "
			"redirect dst must be unicast: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6));
		return;
	}

	is_router = is_onlink = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&redtgt6))
		is_router = 1;	/* router case */
	if (bcmp(&redtgt6, &reddst6, sizeof(redtgt6)) == 0)
		is_onlink = 1;	/* on-link destination case */
	if (!is_router && !is_onlink) {
		log(LOG_ERR,
			"ICMP6 redirect rejected; "
			"neither router case nor onlink case: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6));
		return;
	}
	/* validation passed */

	icmp6len -= sizeof(*nd_rd);
	nd6_option_init(nd_rd + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		log(LOG_INFO, "icmp6_redirect_input: "
			"invalid ND option, rejected: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6));
		return;
	}

	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
	}

	if (ndopts.nd_opts_rh) {
		redirhdrlen = ndopts.nd_opts_rh->nd_opt_rh_len;
		redirhdr = (u_char *)(ndopts.nd_opts_rh + 1); /* xxx */
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		log(LOG_INFO,
			"icmp6_redirect_input: lladdrlen mismatch for %s "
			"(if %d, icmp6 packet %d): %s\n",
			ip6_sprintf(&redtgt6), ifp->if_addrlen, lladdrlen - 2,
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6));
	}

	/* RFC 2461 8.3 */
	nd6_cache_lladdr(ifp, &redtgt6, lladdr, lladdrlen, ND_REDIRECT,
			 is_onlink ? ND_REDIRECT_ONLINK : ND_REDIRECT_ROUTER);

	if (!is_onlink) {	/* better router case. perform rtredirect. */
		/* perform rtredirect */
		struct sockaddr_in6 sdst;
		struct sockaddr_in6 sgw;
		struct sockaddr_in6 ssrc;

		bzero(&sdst, sizeof(sdst));
		bzero(&sgw, sizeof(sgw));
		bzero(&ssrc, sizeof(ssrc));
		sdst.sin6_family = sgw.sin6_family = ssrc.sin6_family = AF_INET6;
		sdst.sin6_len = sgw.sin6_len = ssrc.sin6_len =
			sizeof(struct sockaddr_in6);
		bcopy(&redtgt6, &sgw.sin6_addr, sizeof(struct in6_addr));
		bcopy(&reddst6, &sdst.sin6_addr, sizeof(struct in6_addr));
		bcopy(&src6, &ssrc.sin6_addr, sizeof(struct in6_addr));
		rtredirect((struct sockaddr *)&sdst, (struct sockaddr *)&sgw,
			   (struct sockaddr *)NULL, RTF_GATEWAY | RTF_HOST,
			   (struct sockaddr *)&ssrc,
			   (struct rtentry **)NULL);
	}
	/* finally update cached route in each socket via pfctlinput */
    {
	struct sockaddr_in6 sdst;

	bzero(&sdst, sizeof(sdst));
	sdst.sin6_family = AF_INET6;
	sdst.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&reddst6, &sdst.sin6_addr, sizeof(struct in6_addr));
	pfctlinput(PRC_REDIRECT_HOST, (struct sockaddr *)&sdst);
#ifdef IPSEC
	key_sa_routechange((struct sockaddr *)&sdst);
#endif
    }
}

void
icmp6_redirect_output(m0, rt)
	struct mbuf *m0;
	struct rtentry *rt;
{
	struct ifnet *ifp;	/* my outgoing interface */
	struct in6_addr *ifp_ll6;
	struct in6_addr *router_ll6;
	struct ip6_hdr *sip6;	/* m0 as struct ip6_hdr */
	struct mbuf *m = NULL;	/* newly allocated one */
	struct ip6_hdr *ip6;	/* m as struct ip6_hdr */
	struct nd_redirect *nd_rd;
	size_t maxlen;
	u_char *p;
	struct ifnet *outif = NULL;

	/* if we are not router, we don't send icmp6 redirect */
	if (!ip6_forwarding || ip6_accept_rtadv)
		goto fail;

	/* sanity check */
	if (!m0 || !rt || !(rt->rt_flags & RTF_UP) || !(ifp = rt->rt_ifp))
		goto fail;

	/*
	 * Address check:
	 *  the source address must identify a neighbor, and
	 *  the destination address must not be a multicast address
	 *  [RFC 2461, sec 8.2]
	 */
	sip6 = mtod(m0, struct ip6_hdr *);
	if (nd6_is_addr_neighbor(&sip6->ip6_src, ifp) == 0)
		goto fail;
	if (IN6_IS_ADDR_MULTICAST(&sip6->ip6_dst))
		goto fail;	/* what should we do here? */

	/* rate limit */
	if (icmp6_ratelimit(&sip6->ip6_src, ND_REDIRECT, 0))
		goto fail;

	/*
	 * Since we are going to append up to 1280 bytes (= IPV6_MMTU),
	 * we almost always ask for an mbuf cluster for simplicity.
	 * (MHLEN < IPV6_MMTU is almost always true)
	 */
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (!m)
		goto fail;
	if (MHLEN < IPV6_MMTU)
		MCLGET(m, M_DONTWAIT);
	maxlen = (m->m_flags & M_EXT) ? MCLBYTES : MHLEN;
	maxlen = min(IPV6_MMTU, maxlen);
	/* just for safety */
	if (maxlen < sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr))
		goto fail;

	{
		/* get ip6 linklocal address for ifp(my outgoing interface). */
		struct in6_ifaddr *ia = in6ifa_ifpforlinklocal(ifp);
		if (ia == NULL)
			goto fail;
		ifp_ll6 = &ia->ia_addr.sin6_addr;
	}

	/* get ip6 linklocal address for the router. */
	if (rt->rt_gateway && (rt->rt_flags & RTF_GATEWAY)) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)rt->rt_gateway;
		router_ll6 = &sin6->sin6_addr;
		if (!IN6_IS_ADDR_LINKLOCAL(router_ll6))
			router_ll6 = (struct in6_addr *)NULL;
	} else
		router_ll6 = (struct in6_addr *)NULL;

	/* ip6 */
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc = IPV6_VERSION;
	/* ip6->ip6_plen will be set later */
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	ip6->ip6_hlim = 255;
	/* ip6->ip6_src must be linklocal addr for my outgoing if. */
	bcopy(ifp_ll6, &ip6->ip6_src, sizeof(struct in6_addr));
	bcopy(&sip6->ip6_src, &ip6->ip6_dst, sizeof(struct in6_addr));

	/* ND Redirect */
	nd_rd = (struct nd_redirect *)(ip6 + 1);
	nd_rd->nd_rd_type = ND_REDIRECT;
	nd_rd->nd_rd_code = 0;
	nd_rd->nd_rd_reserved = 0;
	if (rt->rt_flags & RTF_GATEWAY) {
		/*
		 * nd_rd->nd_rd_target must be a link-local address in
		 * better router cases.
		 */
		if (!router_ll6)
			goto fail;
		bcopy(router_ll6, &nd_rd->nd_rd_target,
		      sizeof(nd_rd->nd_rd_target));
		bcopy(&sip6->ip6_dst, &nd_rd->nd_rd_dst,
		      sizeof(nd_rd->nd_rd_dst));
	} else {
		/* make sure redtgt == reddst */
		bcopy(&sip6->ip6_dst, &nd_rd->nd_rd_target,
		      sizeof(nd_rd->nd_rd_target));
		bcopy(&sip6->ip6_dst, &nd_rd->nd_rd_dst,
		      sizeof(nd_rd->nd_rd_dst));
	}

	p = (u_char *)(nd_rd + 1);

	if (!router_ll6)
		goto nolladdropt;

    {
	/* target lladdr option */
	struct rtentry *rt_router = NULL;
	int len;
	struct sockaddr_dl *sdl;
	struct nd_opt_hdr *nd_opt;
	char *lladdr;

	rt_router = nd6_lookup(router_ll6, 0, ifp);
	if (!rt_router)
		goto nolladdropt;
	if (!(rt_router->rt_flags & RTF_GATEWAY)
	 && (rt_router->rt_flags & RTF_LLINFO)
	 && (rt_router->rt_gateway->sa_family == AF_LINK)
	 && (sdl = (struct sockaddr_dl *)rt_router->rt_gateway)) {
		nd_opt = (struct nd_opt_hdr *)p;
		nd_opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
		len = 2 + ifp->if_addrlen;
		len = (len + 7) & ~7;	/*round by 8*/
		nd_opt->nd_opt_len = len >> 3;
		p += len;
		lladdr = (char *)(nd_opt + 1);
		bcopy(LLADDR(sdl), lladdr, ifp->if_addrlen);
	}
    }
nolladdropt:;

	m->m_pkthdr.len = m->m_len = p - (u_char *)ip6;

	/* just to be safe */
	if (m0->m_flags & M_DECRYPTED)
		goto noredhdropt;

    {
	/* redirected header option */
	int len;
	struct nd_opt_rd_hdr *nd_opt_rh;

	/*
	 * compute the maximum size for icmp6 redirect header option.
	 * XXX room for auth header?
	 */
	len = maxlen - (p - (u_char *)ip6);
	len &= ~7;

	/* This is just for simplicity. */
	if (m0->m_pkthdr.len != m0->m_len) {
		if (m0->m_next) {
			m_freem(m0->m_next);
			m0->m_next = NULL;
		}
		m0->m_pkthdr.len = m0->m_len;
	}

	/*
	 * Redirected header option spec (RFC2461 4.6.3) talks nothing
	 * about padding/truncate rule for the original IP packet.
	 * From the discussion on IPv6imp in Feb 1999, the consensus was:
	 * - "attach as much as possible" is the goal
	 * - pad if not aligned (original size can be guessed by original
	 *   ip6 header)
	 * Following code adds the padding if it is simple enough,
	 * and truncates if not.
	 */
	if (m0->m_next || m0->m_pkthdr.len != m0->m_len)
		panic("assumption failed in %s:%d\n", __FILE__, __LINE__);

	if (len - sizeof(*nd_opt_rh) < m0->m_pkthdr.len) {
		/* not enough room, truncate */
		m0->m_pkthdr.len = m0->m_len = len - sizeof(*nd_opt_rh);
	} else {
		/* enough room, pad or truncate */
		size_t extra;

		extra = m0->m_pkthdr.len % 8;
		if (extra) {
			/* pad if easy enough, truncate if not */
			if (8 - extra <= M_TRAILINGSPACE(m0)) {
				/* pad */
				m0->m_len += (8 - extra);
				m0->m_pkthdr.len += (8 - extra);
			} else {
				/* truncate */
				m0->m_pkthdr.len -= extra;
				m0->m_len -= extra;
			}
		}
		len = m0->m_pkthdr.len + sizeof(*nd_opt_rh);
		m0->m_pkthdr.len = m0->m_len = len - sizeof(*nd_opt_rh);
	}

	nd_opt_rh = (struct nd_opt_rd_hdr *)p;
	bzero(nd_opt_rh, sizeof(*nd_opt_rh));
	nd_opt_rh->nd_opt_rh_type = ND_OPT_REDIRECTED_HEADER;
	nd_opt_rh->nd_opt_rh_len = len >> 3;
	p += sizeof(*nd_opt_rh);
	m->m_pkthdr.len = m->m_len = p - (u_char *)ip6;

	/* connect m0 to m */
	m->m_next = m0;
	m->m_pkthdr.len = m->m_len + m0->m_len;
    }
noredhdropt:;

	if (IN6_IS_ADDR_LINKLOCAL(&sip6->ip6_src))
		sip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&sip6->ip6_dst))
		sip6->ip6_dst.s6_addr16[1] = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&nd_rd->nd_rd_target))
		nd_rd->nd_rd_target.s6_addr16[1] = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&nd_rd->nd_rd_dst))
		nd_rd->nd_rd_dst.s6_addr16[1] = 0;

	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));

	nd_rd->nd_rd_cksum = 0;
	nd_rd->nd_rd_cksum
		= in6_cksum(m, IPPROTO_ICMPV6, sizeof(*ip6), ntohs(ip6->ip6_plen));

	/* send the packet to outside... */
#ifdef IPSEC
	m->m_pkthdr.rcvif = NULL;
#endif /*IPSEC*/
	ip6_output(m, NULL, NULL, 0, NULL, &outif);
	if (outif) {
		icmp6_ifstat_inc(outif, ifs6_out_msg);
		icmp6_ifstat_inc(outif, ifs6_out_redirect);
	}
	icmp6stat.icp6s_outhist[ND_REDIRECT]++;

	return;

fail:
	if (m)
		m_freem(m);
	if (m0)
		m_freem(m0);
}

/*
 * ICMPv6 socket option processing.
 */
int
icmp6_ctloutput(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int error = 0;
	int optlen;
	register struct inpcb *inp = sotoinpcb(so);
	int level, op, optname;

	if (sopt) {
		level = sopt->sopt_level;
		op = sopt->sopt_dir;
		optname = sopt->sopt_name;
		optlen = sopt->sopt_valsize;
	} else
		level = op = optname = optlen = 0;
	if (level != IPPROTO_ICMPV6) {
		return EINVAL;
	}

	switch(op) {
	case PRCO_SETOPT:
		switch (optname) {
		case ICMP6_FILTER:
		    {
			struct icmp6_filter *p;

			if (optlen != sizeof(*p)) {
				error = EMSGSIZE;
				break;
			}
			if (inp->in6p_icmp6filt == NULL) {
				error = EINVAL;
				break;
			}
			error = sooptcopyin(sopt, inp->in6p_icmp6filt, optlen,
				optlen);
			break;
		    }

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case ICMP6_FILTER:
		    {
			if (inp->in6p_icmp6filt == NULL) {
				error = EINVAL;
				break;
			}
			error = sooptcopyout(sopt, inp->in6p_icmp6filt,
				sizeof(struct icmp6_filter));
			break;
		    }

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}

	return(error);
}

/*
 * Perform rate limit check.
 * Returns 0 if it is okay to send the icmp6 packet.
 * Returns 1 if the router SHOULD NOT send this icmp6 packet due to rate
 * limitation.
 *
 * XXX per-destination/type check necessary?
 */
static int
icmp6_ratelimit(dst, type, code)
	const struct in6_addr *dst;	/* not used at this moment */
	const int type;			/* not used at this moment */
	const int code;			/* not used at this moment */
{
	struct timeval tp;
	long sec_diff, usec_diff;

	/* If we are not doing rate limitation, it is always okay to send */
	if (!icmp6errratelim)
		return 0;

	microtime(&tp);
	tp.tv_sec = time_second;
	if (tp.tv_sec < icmp6_nextsend.tv_sec
	 || (tp.tv_sec == icmp6_nextsend.tv_sec
	  && tp.tv_usec < icmp6_nextsend.tv_usec)) {
		/* The packet is subject to rate limit */
		return 1;
	}
	sec_diff = icmp6errratelim / 1000000;
	usec_diff = icmp6errratelim % 1000000;
	icmp6_nextsend.tv_sec = tp.tv_sec + sec_diff;
	if ((tp.tv_usec = tp.tv_usec + usec_diff) >= 1000000) {
		icmp6_nextsend.tv_sec++;
		icmp6_nextsend.tv_usec -= 1000000;
	}

	/* it is okay to send this */
	return 0;
}
