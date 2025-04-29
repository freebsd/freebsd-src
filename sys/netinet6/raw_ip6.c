/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include "opt_ipsec.h"
#include "opt_inet6.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>

#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/raw_ip6.h>
#include <netinet6/in6_fib.h>
#include <netinet6/scope6_var.h>
#include <netinet6/send.h>

#include <netipsec/ipsec_support.h>

#include <machine/stdarg.h>

#define	satosin6(sa)	((struct sockaddr_in6 *)(sa))
#define	ifatoia6(ifa)	((struct in6_ifaddr *)(ifa))

/*
 * Raw interface to IP6 protocol.
 */

VNET_DECLARE(struct inpcbinfo, ripcbinfo);
#define	V_ripcbinfo			VNET(ripcbinfo)

VNET_DECLARE(int, rip_bind_all_fibs);
#define	V_rip_bind_all_fibs	VNET(rip_bind_all_fibs)

extern u_long	rip_sendspace;
extern u_long	rip_recvspace;

VNET_PCPUSTAT_DEFINE(struct rip6stat, rip6stat);
VNET_PCPUSTAT_SYSINIT(rip6stat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(rip6stat);
#endif /* VIMAGE */

/*
 * Hooks for multicast routing. They all default to NULL, so leave them not
 * initialized and rely on BSS being set to 0.
 */

/*
 * The socket used to communicate with the multicast routing daemon.
 */
VNET_DEFINE(struct socket *, ip6_mrouter);

/*
 * The various mrouter functions.
 */
int (*ip6_mrouter_set)(struct socket *, struct sockopt *);
int (*ip6_mrouter_get)(struct socket *, struct sockopt *);
int (*ip6_mrouter_done)(void);
int (*ip6_mforward)(struct ip6_hdr *, struct ifnet *, struct mbuf *);
int (*mrt6_ioctl)(u_long, caddr_t);

struct rip6_inp_match_ctx {
	struct ip6_hdr *ip6;
	int proto;
};

static bool
rip6_inp_match(const struct inpcb *inp, void *v)
{
	struct rip6_inp_match_ctx *c = v;
	struct ip6_hdr *ip6 = c->ip6;
	int proto = c->proto;

	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV6) == 0)
		return (false);
	if (inp->inp_ip_p && inp->inp_ip_p != proto)
		return (false);
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) &&
	    !IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, &ip6->ip6_dst))
		return (false);
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
	    !IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, &ip6->ip6_src))
		return (false);

	return (true);
}

/*
 * Setup generic address and protocol structures for raw_input routine, then
 * pass them along with mbuf chain.
 */
int
rip6_input(struct mbuf **mp, int *offp, int proto)
{
	struct ifnet *ifp;
	struct mbuf *n, *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct inpcb *inp;
	struct mbuf *opts = NULL;
	struct sockaddr_in6 fromsa;
	struct rip6_inp_match_ctx ctx = { .ip6 = ip6, .proto = proto };
	struct inpcb_iterator inpi = INP_ITERATOR(&V_ripcbinfo,
	    INPLOOKUP_RLOCKPCB, rip6_inp_match, &ctx);
	int delivered = 0, fib;

	M_ASSERTPKTHDR(m);
	NET_EPOCH_ASSERT();

	RIP6STAT_INC(rip6s_ipackets);

	init_sin6(&fromsa, m, 0); /* general init */

	fib = M_GETFIB(m);
	ifp = m->m_pkthdr.rcvif;

	while ((inp = inp_next(&inpi)) != NULL) {
		INP_RLOCK_ASSERT(inp);
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
		/*
		 * Check AH/ESP integrity.
		 */
		if (IPSEC_ENABLED(ipv6) &&
		    IPSEC_CHECK_POLICY(ipv6, m, inp) != 0) {
			/* Do not inject data into pcb. */
			continue;
		}
#endif /* IPSEC */
		if (jailed_without_vnet(inp->inp_cred) &&
		    !IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) &&
		    prison_check_ip6(inp->inp_cred, &ip6->ip6_dst) != 0)
			/*
			 * Allow raw socket in jail to receive multicast;
			 * assume process had PRIV_NETINET_RAW at attach,
			 * and fall through into normal filter path if so.
			 */
			continue;
		if (V_rip_bind_all_fibs == 0 && fib != inp->inp_inc.inc_fibnum)
			/*
			 * Sockets bound to a specific FIB can only receive
			 * packets from that FIB.
			 */
			continue;
		if (inp->in6p_cksum != -1) {
			RIP6STAT_INC(rip6s_isum);
			if (m->m_pkthdr.len - (*offp + inp->in6p_cksum) < 2 ||
			    in6_cksum(m, proto, *offp,
			    m->m_pkthdr.len - *offp)) {
				RIP6STAT_INC(rip6s_badsum);
				/*
				 * Drop the received message, don't send an
				 * ICMP6 message. Set proto to IPPROTO_NONE
				 * to achieve that.
				 */
				INP_RUNLOCK(inp);
				proto = IPPROTO_NONE;
				break;
			}
		}
		/*
		 * If this raw socket has multicast state, and we
		 * have received a multicast, check if this socket
		 * should receive it, as multicast filtering is now
		 * the responsibility of the transport layer.
		 */
		if (inp->in6p_moptions &&
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
			/*
			 * If the incoming datagram is for MLD, allow it
			 * through unconditionally to the raw socket.
			 *
			 * Use the M_RTALERT_MLD flag to check for MLD
			 * traffic without having to inspect the mbuf chain
			 * more deeply, as all MLDv1/v2 host messages MUST
			 * contain the Router Alert option.
			 *
			 * In the case of MLDv1, we may not have explicitly
			 * joined the group, and may have set IFF_ALLMULTI
			 * on the interface. im6o_mc_filter() may discard
			 * control traffic we actually need to see.
			 *
			 * Userland multicast routing daemons should continue
			 * filter the control traffic appropriately.
			 */
			int blocked;

			blocked = MCAST_PASS;
			if ((m->m_flags & M_RTALERT_MLD) == 0) {
				struct sockaddr_in6 mcaddr;

				bzero(&mcaddr, sizeof(struct sockaddr_in6));
				mcaddr.sin6_len = sizeof(struct sockaddr_in6);
				mcaddr.sin6_family = AF_INET6;
				mcaddr.sin6_addr = ip6->ip6_dst;

				blocked = im6o_mc_filter(inp->in6p_moptions,
				    ifp,
				    (struct sockaddr *)&mcaddr,
				    (struct sockaddr *)&fromsa);
			}
			if (blocked != MCAST_PASS) {
				IP6STAT_INC(ip6s_notmember);
				continue;
			}
		}
		if ((n = m_copym(m, 0, M_COPYALL, M_NOWAIT)) == NULL)
			continue;
		if (inp->inp_flags & INP_CONTROLOPTS ||
		    inp->inp_socket->so_options & SO_TIMESTAMP)
			ip6_savecontrol(inp, n, &opts);
		/* strip intermediate headers */
		m_adj(n, *offp);
		if (sbappendaddr(&inp->inp_socket->so_rcv,
		    (struct sockaddr *)&fromsa, n, opts) == 0) {
			soroverflow(inp->inp_socket);
			m_freem(n);
			if (opts)
				m_freem(opts);
			RIP6STAT_INC(rip6s_fullsock);
		} else {
			sorwakeup(inp->inp_socket);
			delivered++;
		}
		opts = NULL;
	}
	if (delivered == 0) {
		RIP6STAT_INC(rip6s_nosock);
		if (m->m_flags & M_MCAST)
			RIP6STAT_INC(rip6s_nosockmcast);
		if (proto == IPPROTO_NONE)
			m_freem(m);
		else
			icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_NEXTHEADER,
			    ip6_get_prevhdr(m, *offp));
		IP6STAT_DEC(ip6s_delivered);
	} else
		m_freem(m);
	return (IPPROTO_DONE);
}

void
rip6_ctlinput(struct ip6ctlparam *ip6cp)
{
	int errno;

	if ((errno = icmp6_errmap(ip6cp->ip6c_icmp6)) != 0)
		in6_pcbnotify(&V_ripcbinfo, ip6cp->ip6c_finaldst, 0,
		    ip6cp->ip6c_src, 0, errno, ip6cp->ip6c_cmdarg,
		    in6_rtchange);
}

/*
 * Generate IPv6 header and pass packet to ip6_output.  Tack on options user
 * may have setup with control call.
 */
static int
rip6_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	struct epoch_tracker et;
	struct inpcb *inp;
	struct sockaddr_in6 tmp, *dstsock;
	struct m_tag *mtag;
	struct ip6_hdr *ip6;
	u_int	plen = m->m_pkthdr.len;
	struct ip6_pktopts opt, *optp;
	struct ifnet *oifp = NULL;
	int error;
	int type = 0, code = 0;		/* for ICMPv6 output statistics only */
	int scope_ambiguous = 0;
	int use_defzone = 0;
	int hlim = 0;
	struct in6_addr in6a;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_send: inp == NULL"));

	/* Always copy sockaddr to avoid overwrites. */
	/* Unlocked read. */
	if (so->so_state & SS_ISCONNECTED) {
		if (nam) {
			error = EISCONN;
			goto release;
		}
		tmp = (struct sockaddr_in6 ){
			.sin6_family = AF_INET6,
			.sin6_len = sizeof(struct sockaddr_in6),
		};
		INP_RLOCK(inp);
		bcopy(&inp->in6p_faddr, &tmp.sin6_addr,
		    sizeof(struct in6_addr));
		INP_RUNLOCK(inp);
		dstsock = &tmp;
	} else {
		if (nam == NULL)
			error = ENOTCONN;
		else if (nam->sa_family != AF_INET6)
			error = EAFNOSUPPORT;
		else if (nam->sa_len != sizeof(struct sockaddr_in6))
			error = EINVAL;
		else
			error = 0;
		if (error != 0)
			goto release;
		dstsock = (struct sockaddr_in6 *)nam;
		if (dstsock->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			goto release;
		}
	}

	INP_WLOCK(inp);

	if (control != NULL) {
		NET_EPOCH_ENTER(et);
		error = ip6_setpktopts(control, &opt, inp->in6p_outputopts,
		    so->so_cred, inp->inp_ip_p);
		NET_EPOCH_EXIT(et);

		if (error != 0) {
			goto bad;
		}
		optp = &opt;
	} else
		optp = inp->in6p_outputopts;

	/*
	 * Check and convert scope zone ID into internal form.
	 *
	 * XXX: we may still need to determine the zone later.
	 */
	if (!(so->so_state & SS_ISCONNECTED)) {
		if (!optp || !optp->ip6po_pktinfo ||
		    !optp->ip6po_pktinfo->ipi6_ifindex)
			use_defzone = V_ip6_use_defzone;
		if (dstsock->sin6_scope_id == 0 && !use_defzone)
			scope_ambiguous = 1;
		if ((error = sa6_embedscope(dstsock, use_defzone)) != 0)
			goto bad;
	}

	/*
	 * For an ICMPv6 packet, we should know its type and code to update
	 * statistics.
	 */
	if (inp->inp_ip_p == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icmp6;
		if (m->m_len < sizeof(struct icmp6_hdr) &&
		    (m = m_pullup(m, sizeof(struct icmp6_hdr))) == NULL) {
			error = ENOBUFS;
			goto bad;
		}
		icmp6 = mtod(m, struct icmp6_hdr *);
		type = icmp6->icmp6_type;
		code = icmp6->icmp6_code;
	}

	M_PREPEND(m, sizeof(*ip6), M_NOWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto bad;
	}
	ip6 = mtod(m, struct ip6_hdr *);

#ifdef ROUTE_MPATH
	if (CALC_FLOWID_OUTBOUND) {
		uint32_t hash_type, hash_val;

		hash_val = fib6_calc_software_hash(&inp->in6p_laddr,
		    &dstsock->sin6_addr, 0, 0, inp->inp_ip_p, &hash_type);
		inp->inp_flowid = hash_val;
		inp->inp_flowtype = hash_type;
	}
#endif
	/*
	 * Source address selection.
	 */
	NET_EPOCH_ENTER(et);
	error = in6_selectsrc_socket(dstsock, optp, inp, so->so_cred,
	    scope_ambiguous, &in6a, &hlim);
	NET_EPOCH_EXIT(et);

	if (error)
		goto bad;
	error = prison_check_ip6(inp->inp_cred, &in6a);
	if (error != 0)
		goto bad;
	ip6->ip6_src = in6a;

	ip6->ip6_dst = dstsock->sin6_addr;

	/*
	 * Fill in the rest of the IPv6 header fields.
	 */
	ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
	    (inp->inp_flow & IPV6_FLOWINFO_MASK);
	ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
	    (IPV6_VERSION & IPV6_VERSION_MASK);

	/*
	 * ip6_plen will be filled in ip6_output, so not fill it here.
	 */
	ip6->ip6_nxt = inp->inp_ip_p;
	ip6->ip6_hlim = hlim;

	if (inp->inp_ip_p == IPPROTO_ICMPV6 || inp->in6p_cksum != -1) {
		struct mbuf *n;
		int off;
		u_int16_t *p;

		/* Compute checksum. */
		if (inp->inp_ip_p == IPPROTO_ICMPV6)
			off = offsetof(struct icmp6_hdr, icmp6_cksum);
		else
			off = inp->in6p_cksum;
		if (plen < off + 2) {
			error = EINVAL;
			goto bad;
		}
		off += sizeof(struct ip6_hdr);

		n = m;
		while (n && n->m_len <= off) {
			off -= n->m_len;
			n = n->m_next;
		}
		if (!n)
			goto bad;
		p = (u_int16_t *)(mtod(n, caddr_t) + off);
		*p = 0;
		*p = in6_cksum(m, ip6->ip6_nxt, sizeof(*ip6), plen);
	}

	/*
	 * Send RA/RS messages to user land for protection, before sending
	 * them to rtadvd/rtsol.
	 */
	if ((send_sendso_input_hook != NULL) &&
	    inp->inp_ip_p == IPPROTO_ICMPV6) {
		switch (type) {
		case ND_ROUTER_ADVERT:
		case ND_ROUTER_SOLICIT:
			mtag = m_tag_get(PACKET_TAG_ND_OUTGOING,
				sizeof(unsigned short), M_NOWAIT);
			if (mtag == NULL)
				goto bad;
			m_tag_prepend(m, mtag);
		}
	}

	NET_EPOCH_ENTER(et);
	error = ip6_output(m, optp, NULL, 0, inp->in6p_moptions, &oifp, inp);
	NET_EPOCH_EXIT(et);
	if (inp->inp_ip_p == IPPROTO_ICMPV6) {
		if (oifp)
			icmp6_ifoutstat_inc(oifp, type, code);
		ICMP6STAT_INC2(icp6s_outhist, type);
	} else
		RIP6STAT_INC(rip6s_opackets);

	goto freectl;

 bad:
	if (m)
		m_freem(m);

 freectl:
	if (control != NULL) {
		ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	INP_WUNLOCK(inp);
	return (error);

release:
	if (control != NULL)
		m_freem(control);
	m_freem(m);
	return (error);
}

/*
 * Raw IPv6 socket option processing.
 */
int
rip6_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct inpcb *inp = sotoinpcb(so);
	int error;

	if (sopt->sopt_level == IPPROTO_ICMPV6)
		/*
		 * XXX: is it better to call icmp6_ctloutput() directly
		 * from protosw?
		 */
		return (icmp6_ctloutput(so, sopt));
	else if (sopt->sopt_level != IPPROTO_IPV6) {
		if (sopt->sopt_dir == SOPT_SET &&
		    sopt->sopt_level == SOL_SOCKET &&
		    sopt->sopt_name == SO_SETFIB)
			return (ip6_ctloutput(so, sopt));
		return (EINVAL);
	}

	error = 0;

	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case MRT6_INIT:
		case MRT6_DONE:
		case MRT6_ADD_MIF:
		case MRT6_DEL_MIF:
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
		case MRT6_PIM:
			if (inp->inp_ip_p != IPPROTO_ICMPV6)
				return (EOPNOTSUPP);
			error = ip6_mrouter_get ?  ip6_mrouter_get(so, sopt) :
			    EOPNOTSUPP;
			break;
		case IPV6_CHECKSUM:
			error = ip6_raw_ctloutput(so, sopt);
			break;
		default:
			error = ip6_ctloutput(so, sopt);
			break;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case MRT6_INIT:
		case MRT6_DONE:
		case MRT6_ADD_MIF:
		case MRT6_DEL_MIF:
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
		case MRT6_PIM:
			if (inp->inp_ip_p != IPPROTO_ICMPV6)
				return (EOPNOTSUPP);
			error = ip6_mrouter_set ?  ip6_mrouter_set(so, sopt) :
			    EOPNOTSUPP;
			break;
		case IPV6_CHECKSUM:
			error = ip6_raw_ctloutput(so, sopt);
			break;
		default:
			error = ip6_ctloutput(so, sopt);
			break;
		}
		break;
	}

	return (error);
}

static int
rip6_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	struct icmp6_filter *filter;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("rip6_attach: inp != NULL"));

	error = priv_check(td, PRIV_NETINET_RAW);
	if (error)
		return (error);
	if (proto >= IPPROTO_MAX || proto < 0)
		return (EPROTONOSUPPORT);
	error = soreserve(so, rip_sendspace, rip_recvspace);
	if (error)
		return (error);
	filter = malloc(sizeof(struct icmp6_filter), M_PCB, M_NOWAIT);
	if (filter == NULL)
		return (ENOMEM);
	error = in_pcballoc(so, &V_ripcbinfo);
	if (error) {
		free(filter, M_PCB);
		return (error);
	}
	inp = (struct inpcb *)so->so_pcb;
	inp->inp_ip_p = proto;
	inp->in6p_cksum = -1;
	inp->in6p_icmp6filt = filter;
	ICMP6_FILTER_SETPASSALL(inp->in6p_icmp6filt);
	INP_WUNLOCK(inp);
	return (0);
}

static void
rip6_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_detach: inp == NULL"));

	if (so == V_ip6_mrouter && ip6_mrouter_done)
		ip6_mrouter_done();
	/* xxx: RSVP */
	INP_WLOCK(inp);
	free(inp->in6p_icmp6filt, M_PCB);
	in_pcbfree(inp);
}

/* XXXRW: This can't ever be called. */
static void
rip6_abort(struct socket *so)
{
	struct inpcb *inp __diagused;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_abort: inp == NULL"));

	soisdisconnected(so);
}

static void
rip6_close(struct socket *so)
{
	struct inpcb *inp __diagused;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_close: inp == NULL"));

	soisdisconnected(so);
}

static int
rip6_disconnect(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_disconnect: inp == NULL"));

	if ((so->so_state & SS_ISCONNECTED) == 0)
		return (ENOTCONN);
	inp->in6p_faddr = in6addr_any;
	rip6_abort(so);
	return (0);
}

static int
rip6_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct epoch_tracker et;
	struct inpcb *inp;
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)nam;
	struct ifaddr *ifa = NULL;
	int error = 0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_bind: inp == NULL"));

	if (nam->sa_family != AF_INET6)
		return (EAFNOSUPPORT);
	if (nam->sa_len != sizeof(*addr))
		return (EINVAL);
	if ((error = prison_check_ip6(td->td_ucred, &addr->sin6_addr)) != 0)
		return (error);
	if (CK_STAILQ_EMPTY(&V_ifnet) || addr->sin6_family != AF_INET6)
		return (EADDRNOTAVAIL);
	if ((error = sa6_embedscope(addr, V_ip6_use_defzone)) != 0)
		return (error);

	NET_EPOCH_ENTER(et);
	if (!IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr) &&
	    (ifa = ifa_ifwithaddr((struct sockaddr *)addr)) == NULL) {
		NET_EPOCH_EXIT(et);
		return (EADDRNOTAVAIL);
	}
	if (ifa != NULL &&
	    ((struct in6_ifaddr *)ifa)->ia6_flags &
	    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|
	     IN6_IFF_DETACHED|IN6_IFF_DEPRECATED)) {
		NET_EPOCH_EXIT(et);
		return (EADDRNOTAVAIL);
	}
	NET_EPOCH_EXIT(et);
	INP_WLOCK(inp);
	inp->in6p_laddr = addr->sin6_addr;
	INP_WUNLOCK(inp);
	return (0);
}

static int
rip6_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)nam;
	struct in6_addr in6a;
	struct epoch_tracker et;
	int error = 0, scope_ambiguous = 0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip6_connect: inp == NULL"));

	if (nam->sa_len != sizeof(*addr))
		return (EINVAL);
	if (CK_STAILQ_EMPTY(&V_ifnet))
		return (EADDRNOTAVAIL);
	if (addr->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);

	/*
	 * Application should provide a proper zone ID or the use of default
	 * zone IDs should be enabled.  Unfortunately, some applications do
	 * not behave as it should, so we need a workaround.  Even if an
	 * appropriate ID is not determined, we'll see if we can determine
	 * the outgoing interface.  If we can, determine the zone ID based on
	 * the interface below.
	 */
	if (addr->sin6_scope_id == 0 && !V_ip6_use_defzone)
		scope_ambiguous = 1;
	if ((error = sa6_embedscope(addr, V_ip6_use_defzone)) != 0)
		return (error);

	INP_WLOCK(inp);
	/* Source address selection. XXX: need pcblookup? */
	NET_EPOCH_ENTER(et);
	error = in6_selectsrc_socket(addr, inp->in6p_outputopts,
	    inp, so->so_cred, scope_ambiguous, &in6a, NULL);
	NET_EPOCH_EXIT(et);
	if (error) {
		INP_WUNLOCK(inp);
		return (error);
	}

	inp->in6p_faddr = addr->sin6_addr;
	inp->in6p_laddr = in6a;
	soisconnected(so);
	INP_WUNLOCK(inp);
	return (0);
}

static int
rip6_shutdown(struct socket *so, enum shutdown_how how)
{

	SOCK_LOCK(so);
	if (!(so->so_state & SS_ISCONNECTED)) {
		SOCK_UNLOCK(so);
		return (ENOTCONN);
	}
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

	return (0);
}

struct protosw rip6_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_ctloutput =		rip6_ctloutput,
	.pr_abort =		rip6_abort,
	.pr_attach =		rip6_attach,
	.pr_bind =		rip6_bind,
	.pr_connect =		rip6_connect,
	.pr_control =		in6_control,
	.pr_detach =		rip6_detach,
	.pr_disconnect =	rip6_disconnect,
	.pr_peeraddr =		in6_getpeeraddr,
	.pr_send =		rip6_send,
	.pr_shutdown =		rip6_shutdown,
	.pr_sockaddr =		in6_getsockaddr,
	.pr_close =		rip6_close
};
