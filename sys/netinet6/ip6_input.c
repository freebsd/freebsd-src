/*	$FreeBSD$	*/
/*	$KAME: ip6_input.c,v 1.259 2002/01/21 04:58:09 jinmei Exp $	*/

/*-
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
 */

/*-
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
 */

#include "opt_ip6fw.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif /* INET */
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#endif

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#define	IPSEC
#endif /* FAST_IPSEC */

#include <netinet6/ip6_fw.h>

#include <netinet6/ip6protosw.h>

#include <net/net_osdep.h>

extern struct domain inet6domain;

u_char ip6_protox[IPPROTO_MAX];
static struct ifqueue ip6intrq;
static int ip6qmaxlen = IFQ_MAXLEN;
struct in6_ifaddr *in6_ifaddr;

extern struct callout in6_tmpaddrtimer_ch;

int ip6_forward_srcrt;			/* XXX */
int ip6_sourcecheck;			/* XXX */
int ip6_sourcecheck_interval;		/* XXX */

int ip6_ours_check_algorithm;

struct pfil_head inet6_pfil_hook;

/* firewall hooks */
ip6_fw_chk_t *ip6_fw_chk_ptr;
ip6_fw_ctl_t *ip6_fw_ctl_ptr;
int ip6_fw_enable = 1;

struct ip6stat ip6stat;

static void ip6_init2 __P((void *));
static struct ip6aux *ip6_setdstifaddr __P((struct mbuf *, struct in6_ifaddr *));
static int ip6_hopopts_input __P((u_int32_t *, u_int32_t *, struct mbuf **, int *));
#ifdef PULLDOWN_TEST
static struct mbuf *ip6_pullexthdr __P((struct mbuf *, size_t, int));
#endif

/*
 * IP6 initialization: fill in IP6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP6 protocol handler.
 */
void
ip6_init()
{
	struct ip6protosw *pr;
	int i;

#ifdef DIAGNOSTIC
	if (sizeof(struct protosw) != sizeof(struct ip6protosw))
		panic("sizeof(protosw) != sizeof(ip6protosw)");
#endif
	pr = (struct ip6protosw *)pffindproto(PF_INET6, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip6_init");

	/* Initialize the entire ip_protox[] array to IPPROTO_RAW. */
	for (i = 0; i < IPPROTO_MAX; i++)
		ip6_protox[i] = pr - inet6sw;
	/*
	 * Cycle through IP protocols and put them into the appropriate place
	 * in ip6_protox[].
	 */
	for (pr = (struct ip6protosw *)inet6domain.dom_protosw;
	    pr < (struct ip6protosw *)inet6domain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET6 &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW) {
			/* Be careful to only index valid IP protocols. */
			if (pr->pr_protocol < IPPROTO_MAX)
				ip6_protox[pr->pr_protocol] = pr - inet6sw;
		}

	/* Initialize packet filter hooks. */
	inet6_pfil_hook.ph_type = PFIL_TYPE_AF;
	inet6_pfil_hook.ph_af = AF_INET6;
	if ((i = pfil_head_register(&inet6_pfil_hook)) != 0)
		printf("%s: WARNING: unable to register pfil hook, "
			"error %d\n", __func__, i);

	ip6intrq.ifq_maxlen = ip6qmaxlen;
	mtx_init(&ip6intrq.ifq_mtx, "ip6_inq", NULL, MTX_DEF);
	netisr_register(NETISR_IPV6, ip6_input, &ip6intrq, 0);
	scope6_init();
	addrsel_policy_init();
	nd6_init();
	frag6_init();
	ip6_desync_factor = arc4random() % MAX_TEMP_DESYNC_FACTOR;
}

static void
ip6_init2(dummy)
	void *dummy;
{

	/* nd6_timer_init */
	callout_init(&nd6_timer_ch, 0);
	callout_reset(&nd6_timer_ch, hz, nd6_timer, NULL);

	/* timer for regeneranation of temporary addresses randomize ID */
	callout_init(&in6_tmpaddrtimer_ch, 0);
	callout_reset(&in6_tmpaddrtimer_ch,
		      (ip6_temp_preferred_lifetime - ip6_desync_factor -
		       ip6_temp_regen_advance) * hz,
		      in6_tmpaddrtimer, NULL);
}

/* cheat */
/* This must be after route_init(), which is now SI_ORDER_THIRD */
SYSINIT(netinet6init2, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, ip6_init2, NULL);

extern struct	route_in6 ip6_forward_rt;

void
ip6_input(m)
	struct mbuf *m;
{
	struct ip6_hdr *ip6;
	int off = sizeof(struct ip6_hdr), nest;
	u_int32_t plen;
	u_int32_t rtalert = ~0;
	int nxt, ours = 0;
	struct ifnet *deliverifp = NULL;
	struct in6_addr odst;
	int srcrt = 0;

	GIANT_REQUIRED;			/* XXX for now */
#ifdef IPSEC
	/*
	 * should the inner packet be considered authentic?
	 * see comment in ah4_input().
	 */
	if (m) {
		m->m_flags &= ~M_AUTHIPHDR;
		m->m_flags &= ~M_AUTHIPDGM;
	}
#endif

	/*
	 * make sure we don't have onion peering information into m_tag.
	 */
	ip6_delaux(m);

	/*
	 * mbuf statistics
	 */
	if (m->m_flags & M_EXT) {
		if (m->m_next)
			ip6stat.ip6s_mext2m++;
		else
			ip6stat.ip6s_mext1++;
	} else {
#define M2MMAX	(sizeof(ip6stat.ip6s_m2m)/sizeof(ip6stat.ip6s_m2m[0]))
		if (m->m_next) {
			if (m->m_flags & M_LOOP) {
				ip6stat.ip6s_m2m[loif[0].if_index]++; /* XXX */
			} else if (m->m_pkthdr.rcvif->if_index < M2MMAX)
				ip6stat.ip6s_m2m[m->m_pkthdr.rcvif->if_index]++;
			else
				ip6stat.ip6s_m2m[0]++;
		} else
			ip6stat.ip6s_m1++;
#undef M2MMAX
	}

	/* drop the packet if IPv6 operation is disabled on the IF */
	if ((ND_IFINFO(m->m_pkthdr.rcvif)->flags & ND6_IFF_IFDISABLED)) {
		m_freem(m);
		return;
	}

	in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_receive);
	ip6stat.ip6s_total++;

#ifndef PULLDOWN_TEST
	/*
	 * L2 bridge code and some other code can return mbuf chain
	 * that does not conform to KAME requirement.  too bad.
	 * XXX: fails to join if interface MTU > MCLBYTES.  jumbogram?
	 */
	if (m && m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		struct mbuf *n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n)
			M_MOVE_PKTHDR(n, m);
		if (n && n->m_pkthdr.len > MHLEN) {
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_freem(n);
				n = NULL;
			}
		}
		if (n == NULL) {
			m_freem(m);
			return;	/* ENOBUFS */
		}

		m_copydata(m, 0, n->m_pkthdr.len, mtod(n, caddr_t));
		n->m_len = n->m_pkthdr.len;
		m_freem(m);
		m = n;
	}
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), /* nothing */);
#endif

	if (m->m_len < sizeof(struct ip6_hdr)) {
		struct ifnet *inifp;
		inifp = m->m_pkthdr.rcvif;
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			return;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6stat.ip6s_badvers++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
		goto bad;
	}

	ip6stat.ip6s_nxthist[ip6->ip6_nxt]++;

	/*
	 * Check against address spoofing/corruption.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst)) {
		/*
		 * XXX: "badscope" is not very suitable for a multicast source.
		 */
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
	if (IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) &&
	    !(m->m_flags & M_LOOP)) {
		/*
		 * In this case, the packet should come from the loopback
		 * interface.  However, we cannot just check the if_flags,
		 * because ip6_mloopback() passes the "actual" interface
		 * as the outgoing/incoming interface.
		 */
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}

#ifdef ALTQ
	if (altq_input != NULL && (*altq_input)(m, AF_INET6) == 0) {
		/* packet is dropped by traffic conditioner */
		return;
	}
#endif
	/*
	 * The following check is not documented in specs.  A malicious
	 * party may be able to use IPv4 mapped addr to confuse tcp/udp stack
	 * and bypass security checks (act as if it was from 127.0.0.1 by using
	 * IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * This check chokes if we are in an SIIT cloud.  As none of BSDs
	 * support IPv4-less kernel compilation, we cannot support SIIT
	 * environment at all.  So, it makes more sense for us to reject any
	 * malicious packets for non-SIIT environment, than try to do a
	 * partial support for SIIT environment.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
#if 0
	/*
	 * Reject packets with IPv4 compatible addresses (auto tunnel).
	 *
	 * The code forbids auto tunnel relay case in RFC1933 (the check is
	 * stronger than RFC1933).  We may want to re-enable it if mech-xx
	 * is revised to forbid relaying case.
	 */
	if (IN6_IS_ADDR_V4COMPAT(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4COMPAT(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
#endif

	/*
	 * Run through list of hooks for input packets.
	 *
	 * NB: Beware of the destination address changing
	 *     (e.g. by NAT rewriting).  When this happens,
	 *     tell ip6_forward to do the right thing.
	 */
	odst = ip6->ip6_dst;

	/* Jump over all PFIL processing if hooks are not active. */
	if (inet6_pfil_hook.ph_busy_count == -1)
		goto passin;

	if (pfil_run_hooks(&inet6_pfil_hook, &m, m->m_pkthdr.rcvif, PFIL_IN, NULL))
		return;
	if (m == NULL)			/* consumed by filter */
		return;
	ip6 = mtod(m, struct ip6_hdr *);
	srcrt = !IN6_ARE_ADDR_EQUAL(&odst, &ip6->ip6_dst);

passin:
	/*
	 * Check with the firewall...
	 */
	if (ip6_fw_enable && ip6_fw_chk_ptr) {
		u_short port = 0;
		/* If ipfw says divert, we have to just drop packet */
		/* use port as a dummy argument */
		if ((*ip6_fw_chk_ptr)(&ip6, NULL, &port, &m)) {
			m_freem(m);
			m = NULL;
		}
		if (!m)
			return;
	}

	/*
	 * Disambiguate address scope zones (if there is ambiguity).
	 * We first make sure that the original source or destination address
	 * is not in our internal form for scoped addresses.  Such addresses
	 * are not necessarily invalid spec-wise, but we cannot accept them due
	 * to the usage conflict.
	 * in6_setscope() then also checks and rejects the cases where src or
	 * dst are the loopback address and the receiving interface
	 * is not loopback.
	 */
	if (in6_clearscope(&ip6->ip6_src) || in6_clearscope(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++; /* XXX */
		goto bad;
	}
	if (in6_setscope(&ip6->ip6_src, m->m_pkthdr.rcvif, NULL) ||
	    in6_setscope(&ip6->ip6_dst, m->m_pkthdr.rcvif, NULL)) {
		ip6stat.ip6s_badscope++;
		goto bad;
	}

	/*
	 * Multicast check
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
	  	struct in6_multi *in6m = 0;

		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mcast);
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN6_LOOKUP_MULTI(ip6->ip6_dst, m->m_pkthdr.rcvif, in6m);
		if (in6m)
			ours = 1;
		else if (!ip6_mrouter) {
			ip6stat.ip6s_notmember++;
			ip6stat.ip6s_cantforward++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
			goto bad;
		}
		deliverifp = m->m_pkthdr.rcvif;
		goto hbhcheck;
	}

	/*
	 *  Unicast check
	 */
	if (ip6_forward_rt.ro_rt != NULL &&
	    (ip6_forward_rt.ro_rt->rt_flags & RTF_UP) != 0 &&
	    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
	    &((struct sockaddr_in6 *)(&ip6_forward_rt.ro_dst))->sin6_addr))
		ip6stat.ip6s_forward_cachehit++;
	else {
		struct sockaddr_in6 *dst6;

		if (ip6_forward_rt.ro_rt) {
			/* route is down or destination is different */
			ip6stat.ip6s_forward_cachemiss++;
			RTFREE(ip6_forward_rt.ro_rt);
			ip6_forward_rt.ro_rt = 0;
		}

		bzero(&ip6_forward_rt.ro_dst, sizeof(struct sockaddr_in6));
		dst6 = (struct sockaddr_in6 *)&ip6_forward_rt.ro_dst;
		dst6->sin6_len = sizeof(struct sockaddr_in6);
		dst6->sin6_family = AF_INET6;
		dst6->sin6_addr = ip6->ip6_dst;

		rtalloc((struct route *)&ip6_forward_rt);
	}

#define rt6_key(r) ((struct sockaddr_in6 *)((r)->rt_nodes->rn_key))

	/*
	 * Accept the packet if the forwarding interface to the destination
	 * according to the routing table is the loopback interface,
	 * unless the associated route has a gateway.
	 * Note that this approach causes to accept a packet if there is a
	 * route to the loopback interface for the destination of the packet.
	 * But we think it's even useful in some situations, e.g. when using
	 * a special daemon which wants to intercept the packet.
	 *
	 * XXX: some OSes automatically make a cloned route for the destination
	 * of an outgoing packet.  If the outgoing interface of the packet
	 * is a loopback one, the kernel would consider the packet to be
	 * accepted, even if we have no such address assinged on the interface.
	 * We check the cloned flag of the route entry to reject such cases,
	 * assuming that route entries for our own addresses are not made by
	 * cloning (it should be true because in6_addloop explicitly installs
	 * the host route).  However, we might have to do an explicit check
	 * while it would be less efficient.  Or, should we rather install a
	 * reject route for such a case?
	 */
	if (ip6_forward_rt.ro_rt &&
	    (ip6_forward_rt.ro_rt->rt_flags &
	     (RTF_HOST|RTF_GATEWAY)) == RTF_HOST &&
#ifdef RTF_WASCLONED
	    !(ip6_forward_rt.ro_rt->rt_flags & RTF_WASCLONED) &&
#endif
#ifdef RTF_CLONED
	    !(ip6_forward_rt.ro_rt->rt_flags & RTF_CLONED) &&
#endif
#if 0
	    /*
	     * The check below is redundant since the comparison of
	     * the destination and the key of the rtentry has
	     * already done through looking up the routing table.
	     */
	    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
	    &rt6_key(ip6_forward_rt.ro_rt)->sin6_addr)
#endif
	    ip6_forward_rt.ro_rt->rt_ifp->if_type == IFT_LOOP) {
		struct in6_ifaddr *ia6 =
			(struct in6_ifaddr *)ip6_forward_rt.ro_rt->rt_ifa;

		/*
		 * record address information into m_tag.
		 */
		(void)ip6_setdstifaddr(m, ia6);

		/*
		 * packets to a tentative, duplicated, or somehow invalid
		 * address must not be accepted.
		 */
		if (!(ia6->ia6_flags & IN6_IFF_NOTREADY)) {
			/* this address is ready */
			ours = 1;
			deliverifp = ia6->ia_ifp;	/* correct? */
			/* Count the packet in the ip address stats */
			ia6->ia_ifa.if_ipackets++;
			ia6->ia_ifa.if_ibytes += m->m_pkthdr.len;
			goto hbhcheck;
		} else {
			/* address is not ready, so discard the packet. */
			nd6log((LOG_INFO,
			    "ip6_input: packet to an unready address %s->%s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst)));

			goto bad;
		}
	}

	/*
	 * FAITH (Firewall Aided Internet Translator)
	 */
	if (ip6_keepfaith) {
		if (ip6_forward_rt.ro_rt && ip6_forward_rt.ro_rt->rt_ifp
		 && ip6_forward_rt.ro_rt->rt_ifp->if_type == IFT_FAITH) {
			/* XXX do we need more sanity checks? */
			ours = 1;
			deliverifp = ip6_forward_rt.ro_rt->rt_ifp; /* faith */
			goto hbhcheck;
		}
	}

	/*
	 * Now there is no reason to process the packet if it's not our own
	 * and we're not a router.
	 */
	if (!ip6_forwarding) {
		ip6stat.ip6s_cantforward++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
		goto bad;
	}

  hbhcheck:
	/*
	 * record address information into m_tag, if we don't have one yet.
	 * note that we are unable to record it, if the address is not listed
	 * as our interface address (e.g. multicast addresses, addresses
	 * within FAITH prefixes and such).
	 */
	if (deliverifp && !ip6_getdstifaddr(m)) {
		struct in6_ifaddr *ia6;

		ia6 = in6_ifawithifp(deliverifp, &ip6->ip6_dst);
		if (ia6) {
			if (!ip6_setdstifaddr(m, ia6)) {
				/*
				 * XXX maybe we should drop the packet here,
				 * as we could not provide enough information
				 * to the upper layers.
				 */
			}
		}
	}

	/*
	 * Process Hop-by-Hop options header if it's contained.
	 * m may be modified in ip6_hopopts_input().
	 * If a JumboPayload option is included, plen will also be modified.
	 */
	plen = (u_int32_t)ntohs(ip6->ip6_plen);
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
		struct ip6_hbh *hbh;

		if (ip6_hopopts_input(&plen, &rtalert, &m, &off)) {
#if 0	/*touches NULL pointer*/
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
#endif
			return;	/* m have already been freed */
		}

		/* adjust pointer */
		ip6 = mtod(m, struct ip6_hdr *);

		/*
		 * if the payload length field is 0 and the next header field
		 * indicates Hop-by-Hop Options header, then a Jumbo Payload
		 * option MUST be included.
		 */
		if (ip6->ip6_plen == 0 && plen == 0) {
			/*
			 * Note that if a valid jumbo payload option is
			 * contained, ip6_hopopts_input() must set a valid
			 * (non-zero) payload length to the variable plen.
			 */
			ip6stat.ip6s_badoptions++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (caddr_t)&ip6->ip6_plen - (caddr_t)ip6);
			return;
		}
#ifndef PULLDOWN_TEST
		/* ip6_hopopts_input() ensures that mbuf is contiguous */
		hbh = (struct ip6_hbh *)(ip6 + 1);
#else
		IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m, sizeof(struct ip6_hdr),
			sizeof(struct ip6_hbh));
		if (hbh == NULL) {
			ip6stat.ip6s_tooshort++;
			return;
		}
#endif
		nxt = hbh->ip6h_nxt;

		/*
		 * accept the packet if a router alert option is included
		 * and we act as an IPv6 router.
		 */
		if (rtalert != ~0 && ip6_forwarding)
			ours = 1;
	} else
		nxt = ip6->ip6_nxt;

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		ip6stat.ip6s_tooshort++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
		goto bad;
	}
	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = sizeof(struct ip6_hdr) + plen;
			m->m_pkthdr.len = sizeof(struct ip6_hdr) + plen;
		} else
			m_adj(m, sizeof(struct ip6_hdr) + plen - m->m_pkthdr.len);
	}

	/*
	 * Forward if desirable.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/*
		 * If we are acting as a multicast router, all
		 * incoming multicast packets are passed to the
		 * kernel-level multicast forwarding function.
		 * The packet is returned (relatively) intact; if
		 * ip6_mforward() returns a non-zero value, the packet
		 * must be discarded, else it may be accepted below.
		 */
		if (ip6_mrouter && ip6_mforward(ip6, m->m_pkthdr.rcvif, m)) {
			ip6stat.ip6s_cantforward++;
			m_freem(m);
			return;
		}
		if (!ours) {
			m_freem(m);
			return;
		}
	} else if (!ours) {
		ip6_forward(m, srcrt);
		return;
	}

	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Malicious party may be able to use IPv4 mapped addr to confuse
	 * tcp/udp stack and bypass security checks (act as if it was from
	 * 127.0.0.1 by using IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * For SIIT end node behavior, you may want to disable the check.
	 * However, you will  become vulnerable to attacks using IPv4 mapped
	 * source.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}

	/*
	 * Tell launch routine the next header
	 */
	ip6stat.ip6s_delivered++;
	in6_ifstat_inc(deliverifp, ifs6_in_deliver);
	nest = 0;

	while (nxt != IPPROTO_DONE) {
		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			ip6stat.ip6s_toomanyhdr++;
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			ip6stat.ip6s_tooshort++;
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
			goto bad;
		}

#ifdef IPSEC
		/*
		 * enforce IPsec policy checking if we are seeing last header.
		 * note that we do not visit this with protocols with pcb layer
		 * code - like udp/tcp/raw ip.
		 */
		if ((inet6sw[ip6_protox[nxt]].pr_flags & PR_LASTHDR) != 0 &&
		    ipsec6_in_reject(m, NULL)) {
			ipsec6stat.in_polvio++;
			goto bad;
		}
#endif
		nxt = (*inet6sw[ip6_protox[nxt]].pr_input)(&m, &off, nxt);
	}
	return;
 bad:
	m_freem(m);
}

/*
 * set/grab in6_ifaddr correspond to IPv6 destination address.
 * XXX backward compatibility wrapper
 */
static struct ip6aux *
ip6_setdstifaddr(m, ia6)
	struct mbuf *m;
	struct in6_ifaddr *ia6;
{
	struct ip6aux *ip6a;

	ip6a = ip6_addaux(m);
	if (ip6a)
		ip6a->ip6a_dstia6 = ia6;
	return ip6a;	/* NULL if failed to set */
}

struct in6_ifaddr *
ip6_getdstifaddr(m)
	struct mbuf *m;
{
	struct ip6aux *ip6a;

	ip6a = ip6_findaux(m);
	if (ip6a)
		return ip6a->ip6a_dstia6;
	else
		return NULL;
}

/*
 * Hop-by-Hop options header processing. If a valid jumbo payload option is
 * included, the real payload length will be stored in plenp.
 */
static int
ip6_hopopts_input(plenp, rtalertp, mp, offp)
	u_int32_t *plenp;
	u_int32_t *rtalertp;	/* XXX: should be stored more smart way */
	struct mbuf **mp;
	int *offp;
{
	struct mbuf *m = *mp;
	int off = *offp, hbhlen;
	struct ip6_hbh *hbh;
	u_int8_t *opt;

	/* validation of the length of the header */
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, sizeof(*hbh), -1);
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	hbhlen = (hbh->ip6h_len + 1) << 3;

	IP6_EXTHDR_CHECK(m, off, hbhlen, -1);
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m,
		sizeof(struct ip6_hdr), sizeof(struct ip6_hbh));
	if (hbh == NULL) {
		ip6stat.ip6s_tooshort++;
		return -1;
	}
	hbhlen = (hbh->ip6h_len + 1) << 3;
	IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m, sizeof(struct ip6_hdr),
		hbhlen);
	if (hbh == NULL) {
		ip6stat.ip6s_tooshort++;
		return -1;
	}
#endif
	off += hbhlen;
	hbhlen -= sizeof(struct ip6_hbh);
	opt = (u_int8_t *)hbh + sizeof(struct ip6_hbh);

	if (ip6_process_hopopts(m, (u_int8_t *)hbh + sizeof(struct ip6_hbh),
				hbhlen, rtalertp, plenp) < 0)
		return (-1);

	*offp = off;
	*mp = m;
	return (0);
}

/*
 * Search header for all Hop-by-hop options and process each option.
 * This function is separate from ip6_hopopts_input() in order to
 * handle a case where the sending node itself process its hop-by-hop
 * options header. In such a case, the function is called from ip6_output().
 *
 * The function assumes that hbh header is located right after the IPv6 header
 * (RFC2460 p7), opthead is pointer into data content in m, and opthead to
 * opthead + hbhlen is located in continuous memory region.
 */
int
ip6_process_hopopts(m, opthead, hbhlen, rtalertp, plenp)
	struct mbuf *m;
	u_int8_t *opthead;
	int hbhlen;
	u_int32_t *rtalertp;
	u_int32_t *plenp;
{
	struct ip6_hdr *ip6;
	int optlen = 0;
	u_int8_t *opt = opthead;
	u_int16_t rtalert_val;
	u_int32_t jumboplen;
	const int erroff = sizeof(struct ip6_hdr) + sizeof(struct ip6_hbh);

	for (; hbhlen > 0; hbhlen -= optlen, opt += optlen) {
		switch (*opt) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_PADN:
			if (hbhlen < IP6OPT_MINLEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			optlen = *(opt + 1) + 2;
			break;
		case IP6OPT_ROUTER_ALERT:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_RTALERT_LEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_RTALERT_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 1 - opthead);
				return (-1);
			}
			optlen = IP6OPT_RTALERT_LEN;
			bcopy((caddr_t)(opt + 2), (caddr_t)&rtalert_val, 2);
			*rtalertp = ntohs(rtalert_val);
			break;
		case IP6OPT_JUMBO:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_JUMBO_LEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_JUMBO_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 1 - opthead);
				return (-1);
			}
			optlen = IP6OPT_JUMBO_LEN;

			/*
			 * IPv6 packets that have non 0 payload length
			 * must not contain a jumbo payload option.
			 */
			ip6 = mtod(m, struct ip6_hdr *);
			if (ip6->ip6_plen) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt - opthead);
				return (-1);
			}

			/*
			 * We may see jumbolen in unaligned location, so
			 * we'd need to perform bcopy().
			 */
			bcopy(opt + 2, &jumboplen, sizeof(jumboplen));
			jumboplen = (u_int32_t)htonl(jumboplen);

#if 1
			/*
			 * if there are multiple jumbo payload options,
			 * *plenp will be non-zero and the packet will be
			 * rejected.
			 * the behavior may need some debate in ipngwg -
			 * multiple options does not make sense, however,
			 * there's no explicit mention in specification.
			 */
			if (*plenp != 0) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 2 - opthead);
				return (-1);
			}
#endif

			/*
			 * jumbo payload length must be larger than 65535.
			 */
			if (jumboplen <= IPV6_MAXPACKET) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 2 - opthead);
				return (-1);
			}
			*plenp = jumboplen;

			break;
		default:		/* unknown option */
			if (hbhlen < IP6OPT_MINLEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			optlen = ip6_unknown_opt(opt, m,
			    erroff + opt - opthead);
			if (optlen == -1)
				return (-1);
			optlen += 2;
			break;
		}
	}

	return (0);

  bad:
	m_freem(m);
	return (-1);
}

/*
 * Unknown option processing.
 * The third argument `off' is the offset from the IPv6 header to the option,
 * which is necessary if the IPv6 header the and option header and IPv6 header
 * is not continuous in order to return an ICMPv6 error.
 */
int
ip6_unknown_opt(optp, m, off)
	u_int8_t *optp;
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6;

	switch (IP6OPT_TYPE(*optp)) {
	case IP6OPT_TYPE_SKIP: /* ignore the option */
		return ((int)*(optp + 1));
	case IP6OPT_TYPE_DISCARD:	/* silently discard */
		m_freem(m);
		return (-1);
	case IP6OPT_TYPE_FORCEICMP: /* send ICMP even if multicasted */
		ip6stat.ip6s_badoptions++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION, off);
		return (-1);
	case IP6OPT_TYPE_ICMP: /* send ICMP if not multicasted */
		ip6stat.ip6s_badoptions++;
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    (m->m_flags & (M_BCAST|M_MCAST)))
			m_freem(m);
		else
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_OPTION, off);
		return (-1);
	}

	m_freem(m);		/* XXX: NOTREACHED */
	return (-1);
}

/*
 * Create the "control" list for this pcb.
 * The function will not modify mbuf chain at all.
 *
 * with KAME mbuf chain restriction:
 * The routine will be called from upper layer handlers like tcp6_input().
 * Thus the routine assumes that the caller (tcp6_input) have already
 * called IP6_EXTHDR_CHECK() and all the extension headers are located in the
 * very first mbuf on the mbuf chain.
 */
void
ip6_savecontrol(in6p, m, mp)
	struct inpcb *in6p;
	struct mbuf *m, **mp;
{
#define IS2292(x, y)	((in6p->in6p_flags & IN6P_RFC2292) ? (x) : (y))
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

#ifdef SO_TIMESTAMP
	if ((in6p->in6p_socket->so_options & SO_TIMESTAMP) != 0) {
		struct timeval tv;

		microtime(&tv);
		*mp = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
		    SCM_TIMESTAMP, SOL_SOCKET);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#endif

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION)
		return;

	/* RFC 2292 sec. 5 */
	if ((in6p->in6p_flags & IN6P_PKTINFO) != 0) {
		struct in6_pktinfo pi6;

		bcopy(&ip6->ip6_dst, &pi6.ipi6_addr, sizeof(struct in6_addr));
		in6_clearscope(&pi6.ipi6_addr);	/* XXX */
		pi6.ipi6_ifindex =
		    (m && m->m_pkthdr.rcvif) ? m->m_pkthdr.rcvif->if_index : 0;

		*mp = sbcreatecontrol((caddr_t) &pi6,
		    sizeof(struct in6_pktinfo),
		    IS2292(IPV6_2292PKTINFO, IPV6_PKTINFO), IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if ((in6p->in6p_flags & IN6P_HOPLIMIT) != 0) {
		int hlim = ip6->ip6_hlim & 0xff;

		*mp = sbcreatecontrol((caddr_t) &hlim, sizeof(int),
		    IS2292(IPV6_2292HOPLIMIT, IPV6_HOPLIMIT), IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if ((in6p->in6p_flags & IN6P_TCLASS) != 0) {
		u_int32_t flowinfo;
		int tclass;

		flowinfo = (u_int32_t)ntohl(ip6->ip6_flow & IPV6_FLOWINFO_MASK);
		flowinfo >>= 20;

		tclass = flowinfo & 0xff;
		*mp = sbcreatecontrol((caddr_t) &tclass, sizeof(tclass),
		    IPV6_TCLASS, IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	/*
	 * IPV6_HOPOPTS socket option.  Recall that we required super-user
	 * privilege for the option (see ip6_ctloutput), but it might be too
	 * strict, since there might be some hop-by-hop options which can be
	 * returned to normal user.
	 * See also RFC 2292 section 6 (or RFC 3542 section 8).
	 */
	if ((in6p->in6p_flags & IN6P_HOPOPTS) != 0) {
		/*
		 * Check if a hop-by-hop options header is contatined in the
		 * received packet, and if so, store the options as ancillary
		 * data. Note that a hop-by-hop options header must be
		 * just after the IPv6 header, which is assured through the
		 * IPv6 input processing.
		 */
		if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh;
			int hbhlen = 0;
#ifdef PULLDOWN_TEST
			struct mbuf *ext;
#endif

#ifndef PULLDOWN_TEST
			hbh = (struct ip6_hbh *)(ip6 + 1);
			hbhlen = (hbh->ip6h_len + 1) << 3;
#else
			ext = ip6_pullexthdr(m, sizeof(struct ip6_hdr),
			    ip6->ip6_nxt);
			if (ext == NULL) {
				ip6stat.ip6s_tooshort++;
				return;
			}
			hbh = mtod(ext, struct ip6_hbh *);
			hbhlen = (hbh->ip6h_len + 1) << 3;
			if (hbhlen != ext->m_len) {
				m_freem(ext);
				ip6stat.ip6s_tooshort++;
				return;
			}
#endif

			/*
			 * XXX: We copy the whole header even if a
			 * jumbo payload option is included, the option which
			 * is to be removed before returning according to
			 * RFC2292.
			 * Note: this constraint is removed in RFC3542
			 */
			*mp = sbcreatecontrol((caddr_t)hbh, hbhlen,
			    IS2292(IPV6_2292HOPOPTS, IPV6_HOPOPTS),
			    IPPROTO_IPV6);
			if (*mp)
				mp = &(*mp)->m_next;
#ifdef PULLDOWN_TEST
			m_freem(ext);
#endif
		}
	}

	if ((in6p->in6p_flags & (IN6P_RTHDR | IN6P_DSTOPTS)) != 0) {
		int nxt = ip6->ip6_nxt, off = sizeof(struct ip6_hdr);

		/*
		 * Search for destination options headers or routing
		 * header(s) through the header chain, and stores each
		 * header as ancillary data.
		 * Note that the order of the headers remains in
		 * the chain of ancillary data.
		 */
		while (1) {	/* is explicit loop prevention necessary? */
			struct ip6_ext *ip6e = NULL;
			int elen;
#ifdef PULLDOWN_TEST
			struct mbuf *ext = NULL;
#endif

			/*
			 * if it is not an extension header, don't try to
			 * pull it from the chain.
			 */
			switch (nxt) {
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;
			default:
				goto loopend;
			}

#ifndef PULLDOWN_TEST
			if (off + sizeof(*ip6e) > m->m_len)
				goto loopend;
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + off);
			if (nxt == IPPROTO_AH)
				elen = (ip6e->ip6e_len + 2) << 2;
			else
				elen = (ip6e->ip6e_len + 1) << 3;
			if (off + elen > m->m_len)
				goto loopend;
#else
			ext = ip6_pullexthdr(m, off, nxt);
			if (ext == NULL) {
				ip6stat.ip6s_tooshort++;
				return;
			}
			ip6e = mtod(ext, struct ip6_ext *);
			if (nxt == IPPROTO_AH)
				elen = (ip6e->ip6e_len + 2) << 2;
			else
				elen = (ip6e->ip6e_len + 1) << 3;
			if (elen != ext->m_len) {
				m_freem(ext);
				ip6stat.ip6s_tooshort++;
				return;
			}
#endif

			switch (nxt) {
			case IPPROTO_DSTOPTS:
				if (!(in6p->in6p_flags & IN6P_DSTOPTS))
					break;

				*mp = sbcreatecontrol((caddr_t)ip6e, elen,
				    IS2292(IPV6_2292DSTOPTS, IPV6_DSTOPTS),
				    IPPROTO_IPV6);
				if (*mp)
					mp = &(*mp)->m_next;
				break;
			case IPPROTO_ROUTING:
				if (!in6p->in6p_flags & IN6P_RTHDR)
					break;

				*mp = sbcreatecontrol((caddr_t)ip6e, elen,
				    IS2292(IPV6_2292RTHDR, IPV6_RTHDR),
				    IPPROTO_IPV6);
				if (*mp)
					mp = &(*mp)->m_next;
				break;
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;

			default:
				/*
			 	 * other cases have been filtered in the above.
				 * none will visit this case.  here we supply
				 * the code just in case (nxt overwritten or
				 * other cases).
				 */
#ifdef PULLDOWN_TEST
				m_freem(ext);
#endif
				goto loopend;

			}

			/* proceed with the next header. */
			off += elen;
			nxt = ip6e->ip6e_nxt;
			ip6e = NULL;
#ifdef PULLDOWN_TEST
			m_freem(ext);
			ext = NULL;
#endif
		}
	  loopend:
		;
	}

#undef IS2292
}

void
ip6_notify_pmtu(in6p, dst, mtu)
	struct inpcb *in6p;
	struct sockaddr_in6 *dst;
	u_int32_t *mtu;
{
	struct socket *so;
	struct mbuf *m_mtu;
	struct ip6_mtuinfo mtuctl;

	so =  in6p->inp_socket;

	if (mtu == NULL)
		return;

#ifdef DIAGNOSTIC
	if (so == NULL)		/* I believe this is impossible */
		panic("ip6_notify_pmtu: socket is NULL");
#endif

	bzero(&mtuctl, sizeof(mtuctl));	/* zero-clear for safety */
	mtuctl.ip6m_mtu = *mtu;
	mtuctl.ip6m_addr = *dst;
	if (sa6_recoverscope(&mtuctl.ip6m_addr))
		return;

	if ((m_mtu = sbcreatecontrol((caddr_t)&mtuctl, sizeof(mtuctl),
	    IPV6_PATHMTU, IPPROTO_IPV6)) == NULL)
		return;

	if (sbappendaddr(&so->so_rcv, (struct sockaddr *)dst, NULL, m_mtu)
	    == 0) {
		m_freem(m_mtu);
		/* XXX: should count statistics */
	} else
		sorwakeup(so);

	return;
}

#ifdef PULLDOWN_TEST
/*
 * pull single extension header from mbuf chain.  returns single mbuf that
 * contains the result, or NULL on error.
 */
static struct mbuf *
ip6_pullexthdr(m, off, nxt)
	struct mbuf *m;
	size_t off;
	int nxt;
{
	struct ip6_ext ip6e;
	size_t elen;
	struct mbuf *n;

#ifdef DIAGNOSTIC
	switch (nxt) {
	case IPPROTO_DSTOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_HOPOPTS:
	case IPPROTO_AH: /* is it possible? */
		break;
	default:
		printf("ip6_pullexthdr: invalid nxt=%d\n", nxt);
	}
#endif

	m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
	if (nxt == IPPROTO_AH)
		elen = (ip6e.ip6e_len + 2) << 2;
	else
		elen = (ip6e.ip6e_len + 1) << 3;

	MGET(n, M_DONTWAIT, MT_DATA);
	if (n && elen >= MLEN) {
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_free(n);
			n = NULL;
		}
	}
	if (!n)
		return NULL;

	n->m_len = 0;
	if (elen >= M_TRAILINGSPACE(n)) {
		m_free(n);
		return NULL;
	}

	m_copydata(m, off, elen, mtod(n, caddr_t));
	n->m_len = elen;
	return n;
}
#endif

/*
 * Get pointer to the previous header followed by the header
 * currently processed.
 * XXX: This function supposes that
 *	M includes all headers,
 *	the next header field and the header length field of each header
 *	are valid, and
 *	the sum of each header length equals to OFF.
 * Because of these assumptions, this function must be called very
 * carefully. Moreover, it will not be used in the near future when
 * we develop `neater' mechanism to process extension headers.
 */
char *
ip6_get_prevhdr(m, off)
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

	if (off == sizeof(struct ip6_hdr))
		return (&ip6->ip6_nxt);
	else {
		int len, nxt;
		struct ip6_ext *ip6e = NULL;

		nxt = ip6->ip6_nxt;
		len = sizeof(struct ip6_hdr);
		while (len < off) {
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + len);

			switch (nxt) {
			case IPPROTO_FRAGMENT:
				len += sizeof(struct ip6_frag);
				break;
			case IPPROTO_AH:
				len += (ip6e->ip6e_len + 2) << 2;
				break;
			default:
				len += (ip6e->ip6e_len + 1) << 3;
				break;
			}
			nxt = ip6e->ip6e_nxt;
		}
		if (ip6e)
			return (&ip6e->ip6e_nxt);
		else
			return NULL;
	}
}

/*
 * get next header offset.  m will be retained.
 */
int
ip6_nexthdr(m, off, proto, nxtp)
	struct mbuf *m;
	int off;
	int proto;
	int *nxtp;
{
	struct ip6_hdr ip6;
	struct ip6_ext ip6e;
	struct ip6_frag fh;

	/* just in case */
	if (m == NULL)
		panic("ip6_nexthdr: m == NULL");
	if ((m->m_flags & M_PKTHDR) == 0 || m->m_pkthdr.len < off)
		return -1;

	switch (proto) {
	case IPPROTO_IPV6:
		if (m->m_pkthdr.len < off + sizeof(ip6))
			return -1;
		m_copydata(m, off, sizeof(ip6), (caddr_t)&ip6);
		if (nxtp)
			*nxtp = ip6.ip6_nxt;
		off += sizeof(ip6);
		return off;

	case IPPROTO_FRAGMENT:
		/*
		 * terminate parsing if it is not the first fragment,
		 * it does not make sense to parse through it.
		 */
		if (m->m_pkthdr.len < off + sizeof(fh))
			return -1;
		m_copydata(m, off, sizeof(fh), (caddr_t)&fh);
		/* IP6F_OFF_MASK = 0xfff8(BigEndian), 0xf8ff(LittleEndian) */
		if (fh.ip6f_offlg & IP6F_OFF_MASK)
			return -1;
		if (nxtp)
			*nxtp = fh.ip6f_nxt;
		off += sizeof(struct ip6_frag);
		return off;

	case IPPROTO_AH:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 2) << 2;
		return off;

	case IPPROTO_HOPOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_DSTOPTS:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 1) << 3;
		return off;

	case IPPROTO_NONE:
	case IPPROTO_ESP:
	case IPPROTO_IPCOMP:
		/* give up */
		return -1;

	default:
		return -1;
	}

	return -1;
}

/*
 * get offset for the last header in the chain.  m will be kept untainted.
 */
int
ip6_lasthdr(m, off, proto, nxtp)
	struct mbuf *m;
	int off;
	int proto;
	int *nxtp;
{
	int newoff;
	int nxt;

	if (!nxtp) {
		nxt = -1;
		nxtp = &nxt;
	}
	while (1) {
		newoff = ip6_nexthdr(m, off, proto, nxtp);
		if (newoff < 0)
			return off;
		else if (newoff < off)
			return -1;	/* invalid */
		else if (newoff == off)
			return newoff;

		off = newoff;
		proto = *nxtp;
	}
}

struct ip6aux *
ip6_addaux(m)
	struct mbuf *m;
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_IPV6_INPUT, NULL);
	if (!mtag) {
		mtag = m_tag_get(PACKET_TAG_IPV6_INPUT, sizeof(struct ip6aux),
		    M_NOWAIT);
		if (mtag) {
			m_tag_prepend(m, mtag);
			bzero(mtag + 1, sizeof(struct ip6aux));
		}
	}
	return mtag ? (struct ip6aux *)(mtag + 1) : NULL;
}

struct ip6aux *
ip6_findaux(m)
	struct mbuf *m;
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_IPV6_INPUT, NULL);
	return mtag ? (struct ip6aux *)(mtag + 1) : NULL;
}

void
ip6_delaux(m)
	struct mbuf *m;
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_IPV6_INPUT, NULL);
	if (mtag)
		m_tag_delete(m, mtag);
}

/*
 * System control for IP6
 */

u_char	inet6ctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};
