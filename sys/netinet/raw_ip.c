/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip_icmp.h>

#include <netipsec/ipsec_support.h>

#include <machine/stdarg.h>
#include <security/mac/mac_framework.h>

extern ipproto_input_t *ip_protox[];

VNET_DEFINE(int, ip_defttl) = IPDEFTTL;
SYSCTL_INT(_net_inet_ip, IPCTL_DEFTTL, ttl, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ip_defttl), 0,
    "Maximum TTL on IP packets");

VNET_DEFINE(struct inpcbinfo, ripcbinfo);
#define	V_ripcbinfo		VNET(ripcbinfo)

/*
 * Control and data hooks for ipfw, dummynet, divert and so on.
 * The data hooks are not used here but it is convenient
 * to keep them all in one place.
 */
VNET_DEFINE(ip_fw_ctl_ptr_t, ip_fw_ctl_ptr) = NULL;

int	(*ip_dn_ctl_ptr)(struct sockopt *);
int	(*ip_dn_io_ptr)(struct mbuf **, struct ip_fw_args *);
void	(*ip_divert_ptr)(struct mbuf *, bool);
int	(*ng_ipfw_input_p)(struct mbuf **, struct ip_fw_args *, bool);

#ifdef INET
/*
 * Hooks for multicast routing. They all default to NULL, so leave them not
 * initialized and rely on BSS being set to 0.
 */

/*
 * The socket used to communicate with the multicast routing daemon.
 */
VNET_DEFINE(struct socket *, ip_mrouter);

/*
 * The various mrouter and rsvp functions.
 */
int (*ip_mrouter_set)(struct socket *, struct sockopt *);
int (*ip_mrouter_get)(struct socket *, struct sockopt *);
int (*ip_mrouter_done)(void);
int (*ip_mforward)(struct ip *, struct ifnet *, struct mbuf *,
		   struct ip_moptions *);
int (*mrt_ioctl)(u_long, caddr_t, int);
int (*legal_vif_num)(int);
u_long (*ip_mcast_src)(int);

int (*rsvp_input_p)(struct mbuf **, int *, int);
int (*ip_rsvp_vif)(struct socket *, struct sockopt *);
void (*ip_rsvp_force_done)(struct socket *);
#endif /* INET */

u_long	rip_sendspace = 9216;
SYSCTL_ULONG(_net_inet_raw, OID_AUTO, maxdgram, CTLFLAG_RW,
    &rip_sendspace, 0, "Maximum outgoing raw IP datagram size");

u_long	rip_recvspace = 9216;
SYSCTL_ULONG(_net_inet_raw, OID_AUTO, recvspace, CTLFLAG_RW,
    &rip_recvspace, 0, "Maximum space for incoming raw IP datagrams");

/*
 * Hash functions
 */

#define INP_PCBHASH_RAW_SIZE	256
#define INP_PCBHASH_RAW(proto, laddr, faddr, mask) \
        (((proto) + (laddr) + (faddr)) % (mask) + 1)

#ifdef INET
static void
rip_inshash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbhead *pcbhash;
	int hash;

	INP_HASH_WLOCK_ASSERT(pcbinfo);
	INP_WLOCK_ASSERT(inp);

	if (inp->inp_ip_p != 0 &&
	    inp->inp_laddr.s_addr != INADDR_ANY &&
	    inp->inp_faddr.s_addr != INADDR_ANY) {
		hash = INP_PCBHASH_RAW(inp->inp_ip_p, inp->inp_laddr.s_addr,
		    inp->inp_faddr.s_addr, pcbinfo->ipi_hashmask);
	} else
		hash = 0;
	pcbhash = &pcbinfo->ipi_hash_exact[hash];
	CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_exact);
}

static void
rip_delhash(struct inpcb *inp)
{

	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_WLOCK_ASSERT(inp);

	CK_LIST_REMOVE(inp, inp_hash_exact);
}
#endif /* INET */

INPCBSTORAGE_DEFINE(ripcbstor, inpcb, "rawinp", "ripcb", "rip", "riphash");

static void
rip_init(void *arg __unused)
{

	in_pcbinfo_init(&V_ripcbinfo, &ripcbstor, INP_PCBHASH_RAW_SIZE, 1);
}
VNET_SYSINIT(rip_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, rip_init, NULL);

#ifdef VIMAGE
static void
rip_destroy(void *unused __unused)
{

	in_pcbinfo_destroy(&V_ripcbinfo);
}
VNET_SYSUNINIT(raw_ip, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH, rip_destroy, NULL);
#endif

#ifdef INET
static int
rip_append(struct inpcb *inp, struct ip *ip, struct mbuf *m,
    struct sockaddr_in *ripsrc)
{
	struct socket *so = inp->inp_socket;
	struct mbuf *n, *opts = NULL;

	INP_LOCK_ASSERT(inp);

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/* check AH/ESP integrity. */
	if (IPSEC_ENABLED(ipv4) && IPSEC_CHECK_POLICY(ipv4, m, inp) != 0)
		return (0);
#endif /* IPSEC */
#ifdef MAC
	if (mac_inpcb_check_deliver(inp, m) != 0)
		return (0);
#endif
	/* Check the minimum TTL for socket. */
	if (inp->inp_ip_minttl && inp->inp_ip_minttl > ip->ip_ttl)
		return (0);

	if ((n = m_copym(m, 0, M_COPYALL, M_NOWAIT)) == NULL)
		return (0);

	if ((inp->inp_flags & INP_CONTROLOPTS) ||
	    (so->so_options & (SO_TIMESTAMP | SO_BINTIME)))
		ip_savecontrol(inp, &opts, ip, n);
	SOCKBUF_LOCK(&so->so_rcv);
	if (sbappendaddr_locked(&so->so_rcv,
	    (struct sockaddr *)ripsrc, n, opts) == 0) {
		soroverflow_locked(so);
		m_freem(n);
		if (opts)
			m_freem(opts);
		return (0);
	}
	sorwakeup_locked(so);

	return (1);
}

struct rip_inp_match_ctx {
	struct ip *ip;
	int proto;
};

static bool
rip_inp_match1(const struct inpcb *inp, void *v)
{
	struct rip_inp_match_ctx *ctx = v;

	if (inp->inp_ip_p != ctx->proto)
		return (false);
#ifdef INET6
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (false);
#endif
	if (inp->inp_laddr.s_addr != ctx->ip->ip_dst.s_addr)
		return (false);
	if (inp->inp_faddr.s_addr != ctx->ip->ip_src.s_addr)
		return (false);
	return (true);
}

static bool
rip_inp_match2(const struct inpcb *inp, void *v)
{
	struct rip_inp_match_ctx *ctx = v;

	if (inp->inp_ip_p && inp->inp_ip_p != ctx->proto)
		return (false);
#ifdef INET6
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (false);
#endif
	if (!in_nullhost(inp->inp_laddr) &&
	    !in_hosteq(inp->inp_laddr, ctx->ip->ip_dst))
		return (false);
	if (!in_nullhost(inp->inp_faddr) &&
	    !in_hosteq(inp->inp_faddr, ctx->ip->ip_src))
		return (false);
	return (true);
}

/*
 * Setup generic address and protocol structures for raw_input routine, then
 * pass them along with mbuf chain.
 */
int
rip_input(struct mbuf **mp, int *offp, int proto)
{
	struct rip_inp_match_ctx ctx = {
		.ip = mtod(*mp, struct ip *),
		.proto = proto,
	};
	struct inpcb_iterator inpi = INP_ITERATOR(&V_ripcbinfo,
	    INPLOOKUP_RLOCKPCB, rip_inp_match1, &ctx);
	struct ifnet *ifp;
	struct mbuf *m = *mp;
	struct inpcb *inp;
	struct sockaddr_in ripsrc;
	int appended;

	*mp = NULL;
	appended = 0;

	bzero(&ripsrc, sizeof(ripsrc));
	ripsrc.sin_len = sizeof(ripsrc);
	ripsrc.sin_family = AF_INET;
	ripsrc.sin_addr = ctx.ip->ip_src;

	ifp = m->m_pkthdr.rcvif;

	inpi.hash = INP_PCBHASH_RAW(proto, ctx.ip->ip_src.s_addr,
	    ctx.ip->ip_dst.s_addr, V_ripcbinfo.ipi_hashmask);
	while ((inp = inp_next(&inpi)) != NULL) {
		INP_RLOCK_ASSERT(inp);
		if (jailed_without_vnet(inp->inp_cred) &&
		    prison_check_ip4(inp->inp_cred, &ctx.ip->ip_dst) != 0) {
			/*
			 * XXX: If faddr was bound to multicast group,
			 * jailed raw socket will drop datagram.
			 */
			continue;
		}
		appended += rip_append(inp, ctx.ip, m, &ripsrc);
	}

	inpi.hash = 0;
	inpi.match = rip_inp_match2;
	MPASS(inpi.inp == NULL);
	while ((inp = inp_next(&inpi)) != NULL) {
		INP_RLOCK_ASSERT(inp);
		if (jailed_without_vnet(inp->inp_cred) &&
		    !IN_MULTICAST(ntohl(ctx.ip->ip_dst.s_addr)) &&
		    prison_check_ip4(inp->inp_cred, &ctx.ip->ip_dst) != 0)
			/*
			 * Allow raw socket in jail to receive multicast;
			 * assume process had PRIV_NETINET_RAW at attach,
			 * and fall through into normal filter path if so.
			 */
			continue;
		/*
		 * If this raw socket has multicast state, and we
		 * have received a multicast, check if this socket
		 * should receive it, as multicast filtering is now
		 * the responsibility of the transport layer.
		 */
		if (inp->inp_moptions != NULL &&
		    IN_MULTICAST(ntohl(ctx.ip->ip_dst.s_addr))) {
			/*
			 * If the incoming datagram is for IGMP, allow it
			 * through unconditionally to the raw socket.
			 *
			 * In the case of IGMPv2, we may not have explicitly
			 * joined the group, and may have set IFF_ALLMULTI
			 * on the interface. imo_multi_filter() may discard
			 * control traffic we actually need to see.
			 *
			 * Userland multicast routing daemons should continue
			 * filter the control traffic appropriately.
			 */
			int blocked;

			blocked = MCAST_PASS;
			if (proto != IPPROTO_IGMP) {
				struct sockaddr_in group;

				bzero(&group, sizeof(struct sockaddr_in));
				group.sin_len = sizeof(struct sockaddr_in);
				group.sin_family = AF_INET;
				group.sin_addr = ctx.ip->ip_dst;

				blocked = imo_multi_filter(inp->inp_moptions,
				    ifp,
				    (struct sockaddr *)&group,
				    (struct sockaddr *)&ripsrc);
			}

			if (blocked != MCAST_PASS) {
				IPSTAT_INC(ips_notmember);
				continue;
			}
		}
		appended += rip_append(inp, ctx.ip, m, &ripsrc);
	}
	if (appended == 0 && ip_protox[ctx.ip->ip_p] == rip_input) {
		IPSTAT_INC(ips_noproto);
		IPSTAT_DEC(ips_delivered);
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PROTOCOL, 0, 0);
	} else
		m_freem(m);
	return (IPPROTO_DONE);
}

/*
 * Generate IP header and pass packet to ip_output.  Tack on options user may
 * have setup with control call.
 */
static int
rip_send(struct socket *so, int pruflags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	struct epoch_tracker et;
	struct ip *ip;
	struct inpcb *inp;
	in_addr_t *dst;
	int error, flags, cnt, hlen;
	u_char opttype, optlen, *cp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_send: inp == NULL"));

	if (control != NULL) {
		m_freem(control);
		control = NULL;
	}

	if (so->so_state & SS_ISCONNECTED) {
		if (nam) {
			error = EISCONN;
			m_freem(m);
			return (error);
		}
		dst = &inp->inp_faddr.s_addr;
	} else {
		if (nam == NULL)
			error = ENOTCONN;
		else if (nam->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		else if (nam->sa_len != sizeof(struct sockaddr_in))
			error = EINVAL;
		else
			error = 0;
		if (error != 0) {
			m_freem(m);
			return (error);
		}
		dst = &((struct sockaddr_in *)nam)->sin_addr.s_addr;
	}

	flags = ((so->so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0) |
	    IP_ALLOWBROADCAST;

	/*
	 * If the user handed us a complete IP packet, use it.  Otherwise,
	 * allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		if (m->m_pkthdr.len + sizeof(struct ip) > IP_MAXPACKET) {
			m_freem(m);
			return(EMSGSIZE);
		}
		M_PREPEND(m, sizeof(struct ip), M_NOWAIT);
		if (m == NULL)
			return(ENOBUFS);

		INP_RLOCK(inp);
		ip = mtod(m, struct ip *);
		ip->ip_tos = inp->inp_ip_tos;
		if (inp->inp_flags & INP_DONTFRAG)
			ip->ip_off = htons(IP_DF);
		else
			ip->ip_off = htons(0);
		ip->ip_p = inp->inp_ip_p;
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst.s_addr = *dst;
#ifdef ROUTE_MPATH
		if (CALC_FLOWID_OUTBOUND) {
			uint32_t hash_type, hash_val;

			hash_val = fib4_calc_software_hash(ip->ip_src,
			    ip->ip_dst, 0, 0, ip->ip_p, &hash_type);
			m->m_pkthdr.flowid = hash_val;
			M_HASHTYPE_SET(m, hash_type);
			flags |= IP_NODEFAULTFLOWID;
		}
#endif
		if (jailed(inp->inp_cred)) {
			/*
			 * prison_local_ip4() would be good enough but would
			 * let a source of INADDR_ANY pass, which we do not
			 * want to see from jails.
			 */
			if (ip->ip_src.s_addr == INADDR_ANY) {
				NET_EPOCH_ENTER(et);
				error = in_pcbladdr(inp, &ip->ip_dst,
				    &ip->ip_src, inp->inp_cred);
				NET_EPOCH_EXIT(et);
			} else {
				error = prison_local_ip4(inp->inp_cred,
				    &ip->ip_src);
			}
			if (error != 0) {
				INP_RUNLOCK(inp);
				m_freem(m);
				return (error);
			}
		}
		ip->ip_ttl = inp->inp_ip_ttl;
	} else {
		if (m->m_pkthdr.len > IP_MAXPACKET) {
			m_freem(m);
			return (EMSGSIZE);
		}
		if (m->m_pkthdr.len < sizeof(*ip)) {
			m_freem(m);
			return (EINVAL);
		}
		m = m_pullup(m, sizeof(*ip));
		if (m == NULL)
			return (ENOMEM);
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		if (m->m_len < hlen) {
			m = m_pullup(m, hlen);
			if (m == NULL)
				return (EINVAL);
			ip = mtod(m, struct ip *);
		}
#ifdef ROUTE_MPATH
		if (CALC_FLOWID_OUTBOUND) {
			uint32_t hash_type, hash_val;

			hash_val = fib4_calc_software_hash(ip->ip_dst,
			    ip->ip_src, 0, 0, ip->ip_p, &hash_type);
			m->m_pkthdr.flowid = hash_val;
			M_HASHTYPE_SET(m, hash_type);
			flags |= IP_NODEFAULTFLOWID;
		}
#endif
		INP_RLOCK(inp);
		/*
		 * Don't allow both user specified and setsockopt options,
		 * and don't allow packet length sizes that will crash.
		 */
		if ((hlen < sizeof (*ip))
		    || ((hlen > sizeof (*ip)) && inp->inp_options)
		    || (ntohs(ip->ip_len) != m->m_pkthdr.len)) {
			INP_RUNLOCK(inp);
			m_freem(m);
			return (EINVAL);
		}
		error = prison_check_ip4(inp->inp_cred, &ip->ip_src);
		if (error != 0) {
			INP_RUNLOCK(inp);
			m_freem(m);
			return (error);
		}
		/*
		 * Don't allow IP options which do not have the required
		 * structure as specified in section 3.1 of RFC 791 on
		 * pages 15-23.
		 */
		cp = (u_char *)(ip + 1);
		cnt = hlen - sizeof (struct ip);
		for (; cnt > 0; cnt -= optlen, cp += optlen) {
			opttype = cp[IPOPT_OPTVAL];
			if (opttype == IPOPT_EOL)
				break;
			if (opttype == IPOPT_NOP) {
				optlen = 1;
				continue;
			}
			if (cnt < IPOPT_OLEN + sizeof(u_char)) {
				INP_RUNLOCK(inp);
				m_freem(m);
				return (EINVAL);
			}
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN + sizeof(u_char) ||
			    optlen > cnt) {
				INP_RUNLOCK(inp);
				m_freem(m);
				return (EINVAL);
			}
		}
		/*
		 * This doesn't allow application to specify ID of zero,
		 * but we got this limitation from the beginning of history.
		 */
		if (ip->ip_id == 0)
			ip_fillid(ip);

		/*
		 * XXX prevent ip_output from overwriting header fields.
		 */
		flags |= IP_RAWOUTPUT;
		IPSTAT_INC(ips_rawout);
	}

	if (inp->inp_flags & INP_ONESBCAST)
		flags |= IP_SENDONES;

#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
#endif

	NET_EPOCH_ENTER(et);
	error = ip_output(m, inp->inp_options, NULL, flags,
	    inp->inp_moptions, inp);
	NET_EPOCH_EXIT(et);
	INP_RUNLOCK(inp);
	return (error);
}

/*
 * Raw IP socket option processing.
 *
 * IMPORTANT NOTE regarding access control: Traditionally, raw sockets could
 * only be created by a privileged process, and as such, socket option
 * operations to manage system properties on any raw socket were allowed to
 * take place without explicit additional access control checks.  However,
 * raw sockets can now also be created in jail(), and therefore explicit
 * checks are now required.  Likewise, raw sockets can be used by a process
 * after it gives up privilege, so some caution is required.  For options
 * passed down to the IP layer via ip_ctloutput(), checks are assumed to be
 * performed in ip_ctloutput() and therefore no check occurs here.
 * Unilaterally checking priv_check() here breaks normal IP socket option
 * operations on raw sockets.
 *
 * When adding new socket options here, make sure to add access control
 * checks here as necessary.
 *
 * XXX-BZ inp locking?
 */
int
rip_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct	inpcb *inp = sotoinpcb(so);
	int	error, optval;

	if (sopt->sopt_level != IPPROTO_IP) {
		if ((sopt->sopt_level == SOL_SOCKET) &&
		    (sopt->sopt_name == SO_SETFIB)) {
			inp->inp_inc.inc_fibnum = so->so_fibnum;
			return (0);
		}
		return (EINVAL);
	}

	error = 0;
	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			optval = inp->inp_flags & INP_HDRINCL;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;

		case IP_FW3:	/* generic ipfw v.3 functions */
		case IP_FW_ADD:	/* ADD actually returns the body... */
		case IP_FW_GET:
		case IP_FW_TABLE_GETSIZE:
		case IP_FW_TABLE_LIST:
		case IP_FW_NAT_GET_CONFIG:
		case IP_FW_NAT_GET_LOG:
			if (V_ip_fw_ctl_ptr != NULL)
				error = V_ip_fw_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT;
			break;

		case IP_DUMMYNET3:	/* generic dummynet v.3 functions */
		case IP_DUMMYNET_GET:
			if (ip_dn_ctl_ptr != NULL)
				error = ip_dn_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT;
			break ;

		case MRT_INIT:
		case MRT_DONE:
		case MRT_ADD_VIF:
		case MRT_DEL_VIF:
		case MRT_ADD_MFC:
		case MRT_DEL_MFC:
		case MRT_VERSION:
		case MRT_ASSERT:
		case MRT_API_SUPPORT:
		case MRT_API_CONFIG:
		case MRT_ADD_BW_UPCALL:
		case MRT_DEL_BW_UPCALL:
			error = priv_check(curthread, PRIV_NETINET_MROUTE);
			if (error != 0)
				return (error);
			if (inp->inp_ip_p != IPPROTO_IGMP)
				return (EOPNOTSUPP);
			error = ip_mrouter_get ? ip_mrouter_get(so, sopt) :
				EOPNOTSUPP;
			break;

		default:
			error = ip_ctloutput(so, sopt);
			break;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;
			if (optval)
				inp->inp_flags |= INP_HDRINCL;
			else
				inp->inp_flags &= ~INP_HDRINCL;
			break;

		case IP_FW3:	/* generic ipfw v.3 functions */
		case IP_FW_ADD:
		case IP_FW_DEL:
		case IP_FW_FLUSH:
		case IP_FW_ZERO:
		case IP_FW_RESETLOG:
		case IP_FW_TABLE_ADD:
		case IP_FW_TABLE_DEL:
		case IP_FW_TABLE_FLUSH:
		case IP_FW_NAT_CFG:
		case IP_FW_NAT_DEL:
			if (V_ip_fw_ctl_ptr != NULL)
				error = V_ip_fw_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT;
			break;

		case IP_DUMMYNET3:	/* generic dummynet v.3 functions */
		case IP_DUMMYNET_CONFIGURE:
		case IP_DUMMYNET_DEL:
		case IP_DUMMYNET_FLUSH:
			if (ip_dn_ctl_ptr != NULL)
				error = ip_dn_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT ;
			break ;

		case IP_RSVP_ON:
			error = priv_check(curthread, PRIV_NETINET_MROUTE);
			if (error != 0)
				return (error);
			if (inp->inp_ip_p != IPPROTO_RSVP)
				return (EOPNOTSUPP);
			error = ip_rsvp_init(so);
			break;

		case IP_RSVP_OFF:
			error = priv_check(curthread, PRIV_NETINET_MROUTE);
			if (error != 0)
				return (error);
			error = ip_rsvp_done();
			break;

		case IP_RSVP_VIF_ON:
		case IP_RSVP_VIF_OFF:
			error = priv_check(curthread, PRIV_NETINET_MROUTE);
			if (error != 0)
				return (error);
			if (inp->inp_ip_p != IPPROTO_RSVP)
				return (EOPNOTSUPP);
			error = ip_rsvp_vif ?
				ip_rsvp_vif(so, sopt) : EINVAL;
			break;

		case MRT_INIT:
		case MRT_DONE:
		case MRT_ADD_VIF:
		case MRT_DEL_VIF:
		case MRT_ADD_MFC:
		case MRT_DEL_MFC:
		case MRT_VERSION:
		case MRT_ASSERT:
		case MRT_API_SUPPORT:
		case MRT_API_CONFIG:
		case MRT_ADD_BW_UPCALL:
		case MRT_DEL_BW_UPCALL:
			error = priv_check(curthread, PRIV_NETINET_MROUTE);
			if (error != 0)
				return (error);
			if (inp->inp_ip_p != IPPROTO_IGMP)
				return (EOPNOTSUPP);
			error = ip_mrouter_set ? ip_mrouter_set(so, sopt) :
					EOPNOTSUPP;
			break;

		default:
			error = ip_ctloutput(so, sopt);
			break;
		}
		break;
	}

	return (error);
}

void
rip_ctlinput(struct icmp *icmp)
{
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	if (IPSEC_ENABLED(ipv4))
		IPSEC_CTLINPUT(ipv4, icmp);
#endif
}

static int
rip_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("rip_attach: inp != NULL"));

	error = priv_check(td, PRIV_NETINET_RAW);
	if (error)
		return (error);
	if (proto >= IPPROTO_MAX || proto < 0)
		return EPROTONOSUPPORT;
	error = soreserve(so, rip_sendspace, rip_recvspace);
	if (error)
		return (error);
	error = in_pcballoc(so, &V_ripcbinfo);
	if (error)
		return (error);
	inp = (struct inpcb *)so->so_pcb;
	inp->inp_ip_p = proto;
	inp->inp_ip_ttl = V_ip_defttl;
	INP_HASH_WLOCK(&V_ripcbinfo);
	rip_inshash(inp);
	INP_HASH_WUNLOCK(&V_ripcbinfo);
	INP_WUNLOCK(inp);
	return (0);
}

static void
rip_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_detach: inp == NULL"));
	KASSERT(inp->inp_faddr.s_addr == INADDR_ANY,
	    ("rip_detach: not closed"));

	/* Disable mrouter first */
	if (so == V_ip_mrouter && ip_mrouter_done)
		ip_mrouter_done();

	INP_WLOCK(inp);
	INP_HASH_WLOCK(&V_ripcbinfo);
	rip_delhash(inp);
	INP_HASH_WUNLOCK(&V_ripcbinfo);

	if (ip_rsvp_force_done)
		ip_rsvp_force_done(so);
	if (so == V_ip_rsvpd)
		ip_rsvp_done();
	in_pcbdetach(inp);
	in_pcbfree(inp);
}

static void
rip_dodisconnect(struct socket *so, struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo;

	pcbinfo = inp->inp_pcbinfo;
	INP_WLOCK(inp);
	INP_HASH_WLOCK(pcbinfo);
	rip_delhash(inp);
	inp->inp_faddr.s_addr = INADDR_ANY;
	rip_inshash(inp);
	INP_HASH_WUNLOCK(pcbinfo);
	SOCK_LOCK(so);
	so->so_state &= ~SS_ISCONNECTED;
	SOCK_UNLOCK(so);
	INP_WUNLOCK(inp);
}

static void
rip_abort(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_abort: inp == NULL"));

	rip_dodisconnect(so, inp);
}

static void
rip_close(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_close: inp == NULL"));

	rip_dodisconnect(so, inp);
}

static int
rip_disconnect(struct socket *so)
{
	struct inpcb *inp;

	if ((so->so_state & SS_ISCONNECTED) == 0)
		return (ENOTCONN);

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_disconnect: inp == NULL"));

	rip_dodisconnect(so, inp);
	return (0);
}

static int
rip_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)nam;
	struct inpcb *inp;
	int error;

	if (nam->sa_family != AF_INET)
		return (EAFNOSUPPORT);
	if (nam->sa_len != sizeof(*addr))
		return (EINVAL);

	error = prison_check_ip4(td->td_ucred, &addr->sin_addr);
	if (error != 0)
		return (error);

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_bind: inp == NULL"));

	if (CK_STAILQ_EMPTY(&V_ifnet) ||
	    (addr->sin_family != AF_INET && addr->sin_family != AF_IMPLINK) ||
	    (addr->sin_addr.s_addr &&
	     (inp->inp_flags & INP_BINDANY) == 0 &&
	     ifa_ifwithaddr_check((struct sockaddr *)addr) == 0))
		return (EADDRNOTAVAIL);

	INP_WLOCK(inp);
	INP_HASH_WLOCK(&V_ripcbinfo);
	rip_delhash(inp);
	inp->inp_laddr = addr->sin_addr;
	rip_inshash(inp);
	INP_HASH_WUNLOCK(&V_ripcbinfo);
	INP_WUNLOCK(inp);
	return (0);
}

static int
rip_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)nam;
	struct inpcb *inp;

	if (nam->sa_len != sizeof(*addr))
		return (EINVAL);
	if (CK_STAILQ_EMPTY(&V_ifnet))
		return (EADDRNOTAVAIL);
	if (addr->sin_family != AF_INET && addr->sin_family != AF_IMPLINK)
		return (EAFNOSUPPORT);

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_connect: inp == NULL"));

	INP_WLOCK(inp);
	INP_HASH_WLOCK(&V_ripcbinfo);
	rip_delhash(inp);
	inp->inp_faddr = addr->sin_addr;
	rip_inshash(inp);
	INP_HASH_WUNLOCK(&V_ripcbinfo);
	soisconnected(so);
	INP_WUNLOCK(inp);
	return (0);
}

static int
rip_shutdown(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("rip_shutdown: inp == NULL"));

	INP_WLOCK(inp);
	socantsendmore(so);
	INP_WUNLOCK(inp);
	return (0);
}
#endif /* INET */

static int
rip_pcblist(SYSCTL_HANDLER_ARGS)
{
	struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_ripcbinfo,
	    INPLOOKUP_RLOCKPCB);
	struct xinpgen xig;
	struct inpcb *inp;
	int error;

	if (req->newptr != 0)
		return (EPERM);

	if (req->oldptr == 0) {
		int n;

		n = V_ripcbinfo.ipi_count;
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xinpcb);
		return (0);
	}

	if ((error = sysctl_wire_old_buffer(req, 0)) != 0)
		return (error);

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = V_ripcbinfo.ipi_count;
	xig.xig_gen = V_ripcbinfo.ipi_gencnt;
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
		xig.xig_gen = V_ripcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_ripcbinfo.ipi_count;
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}

	return (error);
}

SYSCTL_PROC(_net_inet_raw, OID_AUTO/*XXX*/, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    rip_pcblist, "S,xinpcb",
    "List of active raw IP sockets");

#ifdef INET
struct protosw rip_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_ctloutput =		rip_ctloutput,
	.pr_abort =		rip_abort,
	.pr_attach =		rip_attach,
	.pr_bind =		rip_bind,
	.pr_connect =		rip_connect,
	.pr_control =		in_control,
	.pr_detach =		rip_detach,
	.pr_disconnect =	rip_disconnect,
	.pr_peeraddr =		in_getpeeraddr,
	.pr_send =		rip_send,
	.pr_shutdown =		rip_shutdown,
	.pr_sockaddr =		in_getsockaddr,
	.pr_sosetlabel =	in_pcbsosetlabel,
	.pr_close =		rip_close
};
#endif /* INET */
