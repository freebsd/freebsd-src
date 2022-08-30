/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_sctp.h"
#ifndef INET
#error "IPDIVERT requires INET"		/* XXX! */
#endif

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_divert.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
#if defined(SCTP) || defined(SCTP_SUPPORT)
#include <netinet/sctp_crc32.h>
#endif

#include <security/mac/mac_framework.h>
/*
 * Divert sockets
 */

/*
 * Allocate enough space to hold a full IP packet
 */
#define	DIVSNDQ		(65536 + 100)
#define	DIVRCVQ		(65536 + 100)

/*
 * Divert sockets work in conjunction with ipfw or other packet filters,
 * see the divert(4) manpage for features.
 * Packets are selected by the packet filter and tagged with an
 * MTAG_IPFW_RULE tag carrying the 'divert port' number (as set by
 * the packet filter) and information on the matching filter rule for
 * subsequent reinjection. The divert_port is used to put the packet
 * on the corresponding divert socket, while the rule number is passed
 * up (at least partially) as the sin_port in the struct sockaddr.
 *
 * Packets written to the divert socket carry in sin_addr a
 * destination address, and in sin_port the number of the filter rule
 * after which to continue processing.
 * If the destination address is INADDR_ANY, the packet is treated as
 * as outgoing and sent to ip_output(); otherwise it is treated as
 * incoming and sent to ip_input().
 * Further, sin_zero carries some information on the interface,
 * which can be used in the reinject -- see comments in the code.
 *
 * On reinjection, processing in ip_input() and ip_output()
 * will be exactly the same as for the original packet, except that
 * packet filter processing will start at the rule number after the one
 * written in the sin_port (ipfw does not allow a rule #0, so sin_port=0
 * will apply the entire ruleset to the packet).
 */
static SYSCTL_NODE(_net_inet, OID_AUTO, divert, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "divert(4)");

VNET_PCPUSTAT_DEFINE_STATIC(struct divstat, divstat);
VNET_PCPUSTAT_SYSINIT(divstat);
#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(divstat);
#endif
SYSCTL_VNET_PCPUSTAT(_net_inet_divert, OID_AUTO, stats, struct divstat,
    divstat, "divert(4) socket statistics");
#define	DIVSTAT_INC(name)	\
    VNET_PCPUSTAT_ADD(struct divstat, divstat, div_ ## name, 1)

VNET_DEFINE_STATIC(struct inpcbinfo, divcbinfo);
#define	V_divcbinfo			VNET(divcbinfo)

static u_long	div_sendspace = DIVSNDQ;	/* XXX sysctl ? */
static u_long	div_recvspace = DIVRCVQ;	/* XXX sysctl ? */

static int div_output_inbound(int fmaily, struct socket *so, struct mbuf *m,
    struct sockaddr_in *sin);
static int div_output_outbound(int family, struct socket *so, struct mbuf *m);

/*
 * Initialize divert connection block queue.
 */
INPCBSTORAGE_DEFINE(divcbstor, "divinp", "divcb", "div", "divhash");

static void
div_init(void *arg __unused)
{

	/*
	 * XXX We don't use the hash list for divert IP, but it's easier to
	 * allocate one-entry hash lists than it is to check all over the
	 * place for hashbase == NULL.
	 */
	in_pcbinfo_init(&V_divcbinfo, &divcbstor, 1, 1);
}
VNET_SYSINIT(div_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, div_init, NULL);

static void
div_destroy(void *unused __unused)
{

	in_pcbinfo_destroy(&V_divcbinfo);
}
VNET_SYSUNINIT(divert, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, div_destroy, NULL);

static bool
div_port_match(const struct inpcb *inp, void *v)
{
	uint16_t nport = *(uint16_t *)v;

	return (inp->inp_lport == nport);
}

/*
 * Divert a packet by passing it up to the divert socket at port 'port'.
 */
static void
divert_packet(struct mbuf *m, bool incoming)
{
#if defined(SCTP) || defined(SCTP_SUPPORT)
	struct ip *ip;
#endif
	struct inpcb *inp;
	struct socket *sa;
	u_int16_t nport;
	struct sockaddr_in divsrc;
	struct inpcb_iterator inpi = INP_ITERATOR(&V_divcbinfo,
	    INPLOOKUP_RLOCKPCB, div_port_match, &nport);
	struct m_tag *mtag;

	NET_EPOCH_ASSERT();

	mtag = m_tag_locate(m, MTAG_IPFW_RULE, 0, NULL);
	if (mtag == NULL) {
		m_freem(m);
		return;
	}
	/* Assure header */
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		return;

	/* Delayed checksums are currently not compatible with divert. */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}
#if defined(SCTP) || defined(SCTP_SUPPORT)
	if (m->m_pkthdr.csum_flags & CSUM_SCTP) {
		ip = mtod(m, struct ip *);
		sctp_delayed_cksum(m, (uint32_t)(ip->ip_hl << 2));
		m->m_pkthdr.csum_flags &= ~CSUM_SCTP;
	}
#endif
#ifdef INET6
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA_IPV6) {
		in6_delayed_cksum(m, m->m_pkthdr.len -
		    sizeof(struct ip6_hdr), sizeof(struct ip6_hdr));
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA_IPV6;
	}
#if defined(SCTP) || defined(SCTP_SUPPORT)
	if (m->m_pkthdr.csum_flags & CSUM_SCTP_IPV6) {
		sctp_delayed_cksum(m, sizeof(struct ip6_hdr));
		m->m_pkthdr.csum_flags &= ~CSUM_SCTP_IPV6;
	}
#endif
#endif /* INET6 */
	bzero(&divsrc, sizeof(divsrc));
	divsrc.sin_len = sizeof(divsrc);
	divsrc.sin_family = AF_INET;
	/* record matching rule, in host format */
	divsrc.sin_port = ((struct ipfw_rule_ref *)(mtag+1))->rulenum;
	/*
	 * Record receive interface address, if any.
	 * But only for incoming packets.
	 */
	if (incoming) {
		struct ifaddr *ifa;
		struct ifnet *ifp;

		/* Sanity check */
		M_ASSERTPKTHDR(m);

		/* Find IP address for receive interface */
		ifp = m->m_pkthdr.rcvif;
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			divsrc.sin_addr =
			    ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			break;
		}
	}
	/*
	 * Record the incoming interface name whenever we have one.
	 */
	if (m->m_pkthdr.rcvif) {
		/*
		 * Hide the actual interface name in there in the
		 * sin_zero array. XXX This needs to be moved to a
		 * different sockaddr type for divert, e.g.
		 * sockaddr_div with multiple fields like
		 * sockaddr_dl. Presently we have only 7 bytes
		 * but that will do for now as most interfaces
		 * are 4 or less + 2 or less bytes for unit.
		 * There is probably a faster way of doing this,
		 * possibly taking it from the sockaddr_dl on the iface.
		 * This solves the problem of a P2P link and a LAN interface
		 * having the same address, which can result in the wrong
		 * interface being assigned to the packet when fed back
		 * into the divert socket. Theoretically if the daemon saves
		 * and re-uses the sockaddr_in as suggested in the man pages,
		 * this iface name will come along for the ride.
		 * (see div_output for the other half of this.)
		 */
		strlcpy(divsrc.sin_zero, m->m_pkthdr.rcvif->if_xname,
		    sizeof(divsrc.sin_zero));
	}

	/* Put packet on socket queue, if any */
	sa = NULL;
	/* nport is inp_next's context. */
	nport = htons((u_int16_t)(((struct ipfw_rule_ref *)(mtag+1))->info));
	while ((inp = inp_next(&inpi)) != NULL) {
		sa = inp->inp_socket;
		SOCKBUF_LOCK(&sa->so_rcv);
		if (sbappendaddr_locked(&sa->so_rcv,
		    (struct sockaddr *)&divsrc, m, NULL) == 0) {
			soroverflow_locked(sa);
			sa = NULL;	/* force mbuf reclaim below */
		} else {
			sorwakeup_locked(sa);
			DIVSTAT_INC(diverted);
		}
		/* XXX why does only one socket match? */
		INP_RUNLOCK(inp);
		break;
	}
	if (sa == NULL) {
		m_freem(m);
		DIVSTAT_INC(noport);
	}
}

/*
 * Deliver packet back into the IP processing machinery.
 *
 * If no address specified, or address is 0.0.0.0, send to ip_output();
 * otherwise, send to ip_input() and mark as having been received on
 * the interface with that address.
 */
static int
div_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	struct epoch_tracker et;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;
	const struct ip *ip;
	struct m_tag *mtag;
	struct ipfw_rule_ref *dt;
	int error, family;

	if (control)
		m_freem(control);

	/* Packet must have a header (but that's about it) */
	if (m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
		m_freem(m);
		return (EINVAL);
	}

	if (sin != NULL) {
		if (sin->sin_family != AF_INET) {
			m_freem(m);
			return (EAFNOSUPPORT);
		}
		if (sin->sin_len != sizeof(*sin)) {
			m_freem(m);
			return (EINVAL);
		}
	}

	/*
	 * An mbuf may hasn't come from userland, but we pretend
	 * that it has.
	 */
	m->m_pkthdr.rcvif = NULL;
	m->m_nextpkt = NULL;
	M_SETFIB(m, so->so_fibnum);

	mtag = m_tag_locate(m, MTAG_IPFW_RULE, 0, NULL);
	if (mtag == NULL) {
		/* this should be normal */
		mtag = m_tag_alloc(MTAG_IPFW_RULE, 0,
		    sizeof(struct ipfw_rule_ref), M_NOWAIT | M_ZERO);
		if (mtag == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		m_tag_prepend(m, mtag);
	}
	dt = (struct ipfw_rule_ref *)(mtag+1);

	/* Loopback avoidance and state recovery */
	if (sin) {
		int i;

		/* set the starting point. We provide a non-zero slot,
		 * but a non_matching chain_id to skip that info and use
		 * the rulenum/rule_id.
		 */
		dt->slot = 1; /* dummy, chain_id is invalid */
		dt->chain_id = 0;
		dt->rulenum = sin->sin_port+1; /* host format ? */
		dt->rule_id = 0;
		/* XXX: broken for IPv6 */
		/*
		 * Find receive interface with the given name, stuffed
		 * (if it exists) in the sin_zero[] field.
		 * The name is user supplied data so don't trust its size
		 * or that it is zero terminated.
		 */
		for (i = 0; i < sizeof(sin->sin_zero) && sin->sin_zero[i]; i++)
			;
		if ( i > 0 && i < sizeof(sin->sin_zero))
			m->m_pkthdr.rcvif = ifunit(sin->sin_zero);
	}

	ip = mtod(m, struct ip *);
	switch (ip->ip_v) {
	case IPVERSION:
		family = AF_INET;
		break;
#ifdef INET6
	case IPV6_VERSION >> 4:
		family = AF_INET6;
		break;
#endif
	default:
		m_freem(m);
		return (EAFNOSUPPORT);
	}

	/* Reinject packet into the system as incoming or outgoing */
	NET_EPOCH_ENTER(et);
	if (!sin || sin->sin_addr.s_addr == 0) {
		dt->info |= IPFW_IS_DIVERT | IPFW_INFO_OUT;
		error = div_output_outbound(family, so, m);
	} else {
		dt->info |= IPFW_IS_DIVERT | IPFW_INFO_IN;
		error = div_output_inbound(family, so, m, sin);
	}
	NET_EPOCH_EXIT(et);

	return (error);
}

/*
 * Sends mbuf @m to the wire via ip[6]_output().
 *
 * Returns 0 on success or an errno value on failure.  @m is always consumed.
 */
static int
div_output_outbound(int family, struct socket *so, struct mbuf *m)
{
	struct ip *const ip = mtod(m, struct ip *);
	struct mbuf *options;
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	INP_RLOCK(inp);
	switch (family) {
	case AF_INET:
		/*
		 * Don't allow both user specified and setsockopt
		 * options, and don't allow packet length sizes that
		 * will crash.
		 */
		if ((((ip->ip_hl << 2) != sizeof(struct ip)) &&
		    inp->inp_options != NULL) ||
		    ((u_short)ntohs(ip->ip_len) > m->m_pkthdr.len)) {
			INP_RUNLOCK(inp);
			m_freem(m);
			return (EINVAL);
		}
		break;
#ifdef INET6
	case AF_INET6:
	    {
		struct ip6_hdr *const ip6 = mtod(m, struct ip6_hdr *);

		/* Don't allow packet length sizes that will crash */
		if (((u_short)ntohs(ip6->ip6_plen) > m->m_pkthdr.len)) {
			INP_RUNLOCK(inp);
			m_freem(m);
			return (EINVAL);
		}
		break;
	    }
#endif
	}

#ifdef MAC
	mac_inpcb_create_mbuf(inp, m);
#endif
	/*
	 * Get ready to inject the packet into ip_output().
	 * Just in case socket options were specified on the
	 * divert socket, we duplicate them.  This is done
	 * to avoid having to hold the PCB locks over the call
	 * to ip_output(), as doing this results in a number of
	 * lock ordering complexities.
	 *
	 * Note that we set the multicast options argument for
	 * ip_output() to NULL since it should be invariant that
	 * they are not present.
	 */
	KASSERT(inp->inp_moptions == NULL,
	    ("multicast options set on a divert socket"));
	/*
	 * XXXCSJP: It is unclear to me whether or not it makes
	 * sense for divert sockets to have options.  However,
	 * for now we will duplicate them with the INP locks
	 * held so we can use them in ip_output() without
	 * requring a reference to the pcb.
	 */
	options = NULL;
	if (inp->inp_options != NULL) {
		options = m_dup(inp->inp_options, M_NOWAIT);
		if (options == NULL) {
			INP_RUNLOCK(inp);
			m_freem(m);
			return (ENOBUFS);
		}
	}
	INP_RUNLOCK(inp);

	error = 0;
	switch (family) {
	case AF_INET:
		error = ip_output(m, options, NULL,
		    ((so->so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0)
		    | IP_ALLOWBROADCAST | IP_RAWOUTPUT, NULL, NULL);
		break;
#ifdef INET6
	case AF_INET6:
		error = ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
		break;
#endif
	}
	if (error == 0)
		DIVSTAT_INC(outbound);
	if (options != NULL)
		m_freem(options);

	return (error);
}

/*
 * Schedules mbuf @m for local processing via IPv4/IPv6 netisr queue.
 *
 * Returns 0 on success or an errno value on failure.  @m is always consumed.
 */
static int
div_output_inbound(int family, struct socket *so, struct mbuf *m,
    struct sockaddr_in *sin)
{
	const struct ip *ip;
	struct ifaddr *ifa;

	if (m->m_pkthdr.rcvif == NULL) {
		/*
		 * No luck with the name, check by IP address.
		 * Clear the port and the ifname to make sure
		 * there are no distractions for ifa_ifwithaddr.
		 */

		/* XXX: broken for IPv6 */
		bzero(sin->sin_zero, sizeof(sin->sin_zero));
		sin->sin_port = 0;
		ifa = ifa_ifwithaddr((struct sockaddr *) sin);
		if (ifa == NULL) {
			m_freem(m);
			return (EADDRNOTAVAIL);
		}
		m->m_pkthdr.rcvif = ifa->ifa_ifp;
	}
#ifdef MAC
	mac_socket_create_mbuf(so, m);
#endif
	/* Send packet to input processing via netisr */
	switch (family) {
	case AF_INET:
		ip = mtod(m, struct ip *);
		/*
		 * Restore M_BCAST flag when destination address is
		 * broadcast. It is expected by ip_tryforward().
		 */
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)))
			m->m_flags |= M_MCAST;
		else if (in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
			m->m_flags |= M_BCAST;
		netisr_queue_src(NETISR_IP, (uintptr_t)so, m);
		DIVSTAT_INC(inbound);
		break;
#ifdef INET6
	case AF_INET6:
		netisr_queue_src(NETISR_IPV6, (uintptr_t)so, m);
		DIVSTAT_INC(inbound);
		break;
#endif
	default:
		m_freem(m);
		return (EINVAL);
	}

	return (0);
}

static int
div_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp  = sotoinpcb(so);
	KASSERT(inp == NULL, ("div_attach: inp != NULL"));
	if (td != NULL) {
		error = priv_check(td, PRIV_NETINET_DIVERT);
		if (error)
			return (error);
	}
	error = soreserve(so, div_sendspace, div_recvspace);
	if (error)
		return error;
	error = in_pcballoc(so, &V_divcbinfo);
	if (error)
		return error;
	inp = (struct inpcb *)so->so_pcb;
	inp->inp_ip_p = proto;
	inp->inp_flags |= INP_HDRINCL;
	INP_WUNLOCK(inp);
	return 0;
}

static void
div_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("div_detach: inp == NULL"));
	INP_WLOCK(inp);
	in_pcbdetach(inp);
	in_pcbfree(inp);
}

static int
div_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("div_bind: inp == NULL"));
	/* in_pcbbind assumes that nam is a sockaddr_in
	 * and in_pcbbind requires a valid address. Since divert
	 * sockets don't we need to make sure the address is
	 * filled in properly.
	 * XXX -- divert should not be abusing in_pcbind
	 * and should probably have its own family.
	 */
	if (nam->sa_family != AF_INET)
		return EAFNOSUPPORT;
	if (nam->sa_len != sizeof(struct sockaddr_in))
		return EINVAL;
	((struct sockaddr_in *)nam)->sin_addr.s_addr = INADDR_ANY;
	INP_WLOCK(inp);
	INP_HASH_WLOCK(&V_divcbinfo);
	error = in_pcbbind(inp, nam, td->td_ucred);
	INP_HASH_WUNLOCK(&V_divcbinfo);
	INP_WUNLOCK(inp);
	return error;
}

static int
div_shutdown(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("div_shutdown: inp == NULL"));
	INP_WLOCK(inp);
	socantsendmore(so);
	INP_WUNLOCK(inp);
	return 0;
}

static int
div_pcblist(SYSCTL_HANDLER_ARGS)
{
	struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_divcbinfo,
	    INPLOOKUP_RLOCKPCB);
	struct xinpgen xig;
	struct inpcb *inp;
	int error;

	if (req->newptr != 0)
		return EPERM;

	if (req->oldptr == 0) {
		int n;

		n = V_divcbinfo.ipi_count;
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xinpcb);
		return 0;
	}

	if ((error = sysctl_wire_old_buffer(req, 0)) != 0)
		return (error);

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = V_divcbinfo.ipi_count;
	xig.xig_gen = V_divcbinfo.ipi_gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return error;

	while ((inp = inp_next(&inpi)) != NULL) {
		if (inp->inp_gencnt <= xig.xig_gen) {
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
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		xig.xig_gen = V_divcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_divcbinfo.ipi_count;
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}

	return (error);
}
SYSCTL_PROC(_net_inet_divert, OID_AUTO, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0, div_pcblist,
    "S,xinpcb", "List of active divert sockets");

static struct protosw div_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_attach =		div_attach,
	.pr_bind =		div_bind,
	.pr_control =		in_control,
	.pr_detach =		div_detach,
	.pr_peeraddr =		in_getpeeraddr,
	.pr_send =		div_send,
	.pr_shutdown =		div_shutdown,
	.pr_sockaddr =		in_getsockaddr,
	.pr_sosetlabel =	in_pcbsosetlabel
};

static struct domain divertdomain = {
	.dom_family =	PF_DIVERT,
	.dom_name =	"divert",
	.dom_nprotosw =	1,
	.dom_protosw =	{ &div_protosw },
};

static int
div_modevent(module_t mod, int type, void *unused)
{
	int err = 0;

	switch (type) {
	case MOD_LOAD:
		domain_add(&divertdomain);
		ip_divert_ptr = divert_packet;
		break;
	case MOD_QUIESCE:
		/*
		 * IPDIVERT may normally not be unloaded because of the
		 * potential race conditions.  Tell kldunload we can't be
		 * unloaded unless the unload is forced.
		 */
		err = EPERM;
		break;
	case MOD_UNLOAD:
		/*
		 * Forced unload.
		 *
		 * Module ipdivert can only be unloaded if no sockets are
		 * connected.  Maybe this can be changed later to forcefully
		 * disconnect any open sockets.
		 *
		 * XXXRW: Note that there is a slight race here, as a new
		 * socket open request could be spinning on the lock and then
		 * we destroy the lock.
		 *
		 * XXXGL: One more reason this code is incorrect is that it
		 * checks only the current vnet.
		 */
		INP_INFO_WLOCK(&V_divcbinfo);
		if (V_divcbinfo.ipi_count != 0) {
			err = EBUSY;
			INP_INFO_WUNLOCK(&V_divcbinfo);
			break;
		}
		ip_divert_ptr = NULL;
		domain_remove(&divertdomain);
		INP_INFO_WUNLOCK(&V_divcbinfo);
#ifndef VIMAGE
		div_destroy(NULL);
#endif
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return err;
}

static moduledata_t ipdivertmod = {
        "ipdivert",
        div_modevent,
        0
};

DECLARE_MODULE(ipdivert, ipdivertmod, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY);
MODULE_DEPEND(ipdivert, ipfw, 3, 3, 3);
MODULE_VERSION(ipdivert, 1);
