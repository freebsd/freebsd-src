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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_sctp.h"

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
#include <net/if_private.h>
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
 * Usually a system has very few divert ports.  Previous implementation
 * used a linked list.
 */
#define	DIVHASHSIZE	(1 << 3)	/* 8 entries, one cache line. */
#define	DIVHASH(port)	(port % DIVHASHSIZE)
#define	DCBHASH(dcb)	((dcb)->dcb_port % DIVHASHSIZE)

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

static u_long	div_sendspace = DIVSNDQ;	/* XXX sysctl ? */
static u_long	div_recvspace = DIVRCVQ;	/* XXX sysctl ? */

static int div_output_inbound(int fmaily, struct socket *so, struct mbuf *m,
    struct sockaddr_in *sin);
static int div_output_outbound(int family, struct socket *so, struct mbuf *m);

struct divcb {
	union {
		SLIST_ENTRY(divcb)	dcb_next;
		intptr_t		dcb_bound;
#define	DCB_UNBOUND	((intptr_t)-1)
	};
	struct socket		*dcb_socket;
	uint16_t		 dcb_port;
	uint64_t		 dcb_gencnt;
	struct epoch_context	 dcb_epochctx;
};

SLIST_HEAD(divhashhead, divcb);

VNET_DEFINE_STATIC(struct divhashhead, divhash[DIVHASHSIZE]) = {};
#define	V_divhash	VNET(divhash)
VNET_DEFINE_STATIC(uint64_t, dcb_count) = 0;
#define	V_dcb_count	VNET(dcb_count)
VNET_DEFINE_STATIC(uint64_t, dcb_gencnt) = 0;
#define	V_dcb_gencnt	VNET(dcb_gencnt)

static struct mtx divert_mtx;
MTX_SYSINIT(divert, &divert_mtx, "divert(4) socket pcb lists", MTX_DEF);
#define	DIVERT_LOCK()	mtx_lock(&divert_mtx)
#define	DIVERT_UNLOCK()	mtx_unlock(&divert_mtx)

/*
 * Divert a packet by passing it up to the divert socket at port 'port'.
 */
static void
divert_packet(struct mbuf *m, bool incoming)
{
	struct divcb *dcb;
	u_int16_t nport;
	struct sockaddr_in divsrc;
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
#ifdef INET
	/* Delayed checksums are currently not compatible with divert. */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}
#if defined(SCTP) || defined(SCTP_SUPPORT)
	if (m->m_pkthdr.csum_flags & CSUM_SCTP) {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		sctp_delayed_cksum(m, (uint32_t)(ip->ip_hl << 2));
		m->m_pkthdr.csum_flags &= ~CSUM_SCTP;
	}
#endif
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
	nport = htons((uint16_t)(((struct ipfw_rule_ref *)(mtag+1))->info));
	SLIST_FOREACH(dcb, &V_divhash[DIVHASH(nport)], dcb_next)
		if (dcb->dcb_port == nport)
			break;

	if (dcb != NULL) {
		struct socket *sa = dcb->dcb_socket;

		SOCKBUF_LOCK(&sa->so_rcv);
		if (sbappendaddr_locked(&sa->so_rcv,
		    (struct sockaddr *)&divsrc, m, NULL) == 0) {
			soroverflow_locked(sa);
			m_freem(m);
		} else {
			sorwakeup_locked(sa);
			DIVSTAT_INC(diverted);
		}
	} else {
		DIVSTAT_INC(noport);
		m_freem(m);
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
#ifdef INET
	case IPVERSION:
		family = AF_INET;
		break;
#endif
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
	int error;

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		struct ip *const ip = mtod(m, struct ip *);

		/* Don't allow packet length sizes that will crash. */
		if (((u_short)ntohs(ip->ip_len) > m->m_pkthdr.len)) {
			m_freem(m);
			return (EINVAL);
		}
		break;
	    }
#endif
#ifdef INET6
	case AF_INET6:
	    {
		struct ip6_hdr *const ip6 = mtod(m, struct ip6_hdr *);

		/* Don't allow packet length sizes that will crash */
		if (((u_short)ntohs(ip6->ip6_plen) > m->m_pkthdr.len)) {
			m_freem(m);
			return (EINVAL);
		}
		break;
	    }
#endif
	}

#ifdef MAC
	mac_socket_create_mbuf(so, m);
#endif

	error = 0;
	switch (family) {
#ifdef INET
	case AF_INET:
		error = ip_output(m, NULL, NULL,
		    ((so->so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0)
		    | IP_ALLOWBROADCAST | IP_RAWOUTPUT, NULL, NULL);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
		break;
#endif
	}
	if (error == 0)
		DIVSTAT_INC(outbound);

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
#ifdef INET
	case AF_INET:
	    {
		const struct ip *ip;

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
	    }
#endif
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
	struct divcb *dcb;
	int error;

	if (td != NULL) {
		error = priv_check(td, PRIV_NETINET_DIVERT);
		if (error)
			return (error);
	}
	error = soreserve(so, div_sendspace, div_recvspace);
	if (error)
		return error;
	dcb = malloc(sizeof(*dcb), M_PCB, M_WAITOK);
	dcb->dcb_bound = DCB_UNBOUND;
	dcb->dcb_socket = so;
	DIVERT_LOCK();
	V_dcb_count++;
	dcb->dcb_gencnt = ++V_dcb_gencnt;
	DIVERT_UNLOCK();
	so->so_pcb = dcb;

	return (0);
}

static void
div_free(epoch_context_t ctx)
{
	struct divcb *dcb = __containerof(ctx, struct divcb, dcb_epochctx);

	free(dcb, M_PCB);
}

static void
div_detach(struct socket *so)
{
	struct divcb *dcb = so->so_pcb;

	so->so_pcb = NULL;
	DIVERT_LOCK();
	if (dcb->dcb_bound != DCB_UNBOUND)
		SLIST_REMOVE(&V_divhash[DCBHASH(dcb)], dcb, divcb, dcb_next);
	V_dcb_count--;
	V_dcb_gencnt++;
	DIVERT_UNLOCK();
	NET_EPOCH_CALL(div_free, &dcb->dcb_epochctx);
}

static int
div_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct divcb *dcb;
	uint16_t port;

	if (nam->sa_family != AF_INET)
		return EAFNOSUPPORT;
	if (nam->sa_len != sizeof(struct sockaddr_in))
		return EINVAL;
	port = ((struct sockaddr_in *)nam)->sin_port;
	DIVERT_LOCK();
	SLIST_FOREACH(dcb, &V_divhash[DIVHASH(port)], dcb_next)
		if (dcb->dcb_port == port) {
			DIVERT_UNLOCK();
			return (EADDRINUSE);
		}
	dcb = so->so_pcb;
	if (dcb->dcb_bound != DCB_UNBOUND)
		SLIST_REMOVE(&V_divhash[DCBHASH(dcb)], dcb, divcb, dcb_next);
	dcb->dcb_port = port;
	SLIST_INSERT_HEAD(&V_divhash[DIVHASH(port)], dcb, dcb_next);
	DIVERT_UNLOCK();

	return (0);
}

static int
div_shutdown(struct socket *so)
{

	socantsendmore(so);
	return 0;
}

static int
div_pcblist(SYSCTL_HANDLER_ARGS)
{
	struct xinpgen xig;
	struct divcb *dcb;
	int error;

	if (req->newptr != 0)
		return EPERM;

	if (req->oldptr == 0) {
		u_int n;

		n = V_dcb_count;
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xinpcb);
		return 0;
	}

	if ((error = sysctl_wire_old_buffer(req, 0)) != 0)
		return (error);

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = V_dcb_count;
	xig.xig_gen = V_dcb_gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return error;

	DIVERT_LOCK();
	for (int i = 0; i < DIVHASHSIZE; i++)
		SLIST_FOREACH(dcb, &V_divhash[i], dcb_next) {
			if (dcb->dcb_gencnt <= xig.xig_gen) {
				struct xinpcb xi;

				bzero(&xi, sizeof(xi));
				xi.xi_len = sizeof(struct xinpcb);
				sotoxsocket(dcb->dcb_socket, &xi.xi_socket);
				xi.inp_gencnt = dcb->dcb_gencnt;
				xi.inp_vflag = INP_IPV4; /* XXX: netstat(1) */
				xi.inp_inc.inc_ie.ie_lport = dcb->dcb_port;
				error = SYSCTL_OUT(req, &xi, sizeof xi);
				if (error)
					goto errout;
			}
		}

	/*
	 * Give the user an updated idea of our state.
	 * If the generation differs from what we told
	 * her before, she knows that something happened
	 * while we were processing this request, and it
	 * might be necessary to retry.
	 */
	xig.xig_gen = V_dcb_gencnt;
	xig.xig_sogen = so_gencnt;
	xig.xig_count = V_dcb_count;
	error = SYSCTL_OUT(req, &xig, sizeof xig);

errout:
	DIVERT_UNLOCK();

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
	.pr_detach =		div_detach,
	.pr_send =		div_send,
	.pr_shutdown =		div_shutdown,
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
		DIVERT_LOCK();
		if (V_dcb_count != 0) {
			DIVERT_UNLOCK();
			err = EBUSY;
			break;
		}
		DIVERT_UNLOCK();
		ip_divert_ptr = NULL;
		domain_remove(&divertdomain);
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
MODULE_VERSION(ipdivert, 1);
