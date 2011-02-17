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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if !defined(KLD_MODULE)
#include "opt_inet.h"
#include "opt_sctp.h"
#ifndef INET
#error "IPDIVERT requires INET."
#endif
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/netisr.h> 

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef SCTP
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

/* Internal variables. */
static VNET_DEFINE(struct inpcbhead, divcb);
static VNET_DEFINE(struct inpcbinfo, divcbinfo);

#define	V_divcb				VNET(divcb)
#define	V_divcbinfo			VNET(divcbinfo)

static u_long	div_sendspace = DIVSNDQ;	/* XXX sysctl ? */
static u_long	div_recvspace = DIVRCVQ;	/* XXX sysctl ? */

static eventhandler_tag ip_divert_event_tag;

/*
 * Initialize divert connection block queue.
 */
static void
div_zone_change(void *tag)
{

	uma_zone_set_max(V_divcbinfo.ipi_zone, maxsockets);
}

static int
div_inpcb_init(void *mem, int size, int flags)
{
	struct inpcb *inp = mem;

	INP_LOCK_INIT(inp, "inp", "divinp");
	return (0);
}

static void
div_inpcb_fini(void *mem, int size)
{
	struct inpcb *inp = mem;

	INP_LOCK_DESTROY(inp);
}

static void
div_init(void)
{

	/*
	 * XXX We don't use the hash list for divert IP, but it's easier to
	 * allocate one-entry hash lists than it is to check all over the
	 * place for hashbase == NULL.
	 */
	in_pcbinfo_init(&V_divcbinfo, "div", &V_divcb, 1, 1, "divcb",
	    div_inpcb_init, div_inpcb_fini, UMA_ZONE_NOFREE);
}

static void
div_destroy(void)
{

	in_pcbinfo_destroy(&V_divcbinfo);
}

/*
 * IPPROTO_DIVERT is not in the real IP protocol number space; this
 * function should never be called.  Just in case, drop any packets.
 */
static void
div_input(struct mbuf *m, int off)
{

	KMOD_IPSTAT_INC(ips_noproto);
	m_freem(m);
}

/*
 * Divert a packet by passing it up to the divert socket at port 'port'.
 *
 * Setup generic address and protocol structures for div_input routine,
 * then pass them along with mbuf chain.
 */
static void
divert_packet(struct mbuf *m, int incoming)
{
	struct ip *ip;
	struct inpcb *inp;
	struct socket *sa;
	u_int16_t nport;
	struct sockaddr_in divsrc;
	struct m_tag *mtag;

	mtag = m_tag_locate(m, MTAG_IPFW_RULE, 0, NULL);
	if (mtag == NULL) {
		m_freem(m);
		return;
	}
	/* Assure header */
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == 0)
		return;
	ip = mtod(m, struct ip *);

	/* Delayed checksums are currently not compatible with divert. */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		ip->ip_len = ntohs(ip->ip_len);
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
		ip->ip_len = htons(ip->ip_len);
	}
#ifdef SCTP
	if (m->m_pkthdr.csum_flags & CSUM_SCTP) {
		ip->ip_len = ntohs(ip->ip_len);
		sctp_delayed_cksum(m, (uint32_t)(ip->ip_hl << 2));
		m->m_pkthdr.csum_flags &= ~CSUM_SCTP;
		ip->ip_len = htons(ip->ip_len);
	}
#endif
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
		if_addr_rlock(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			divsrc.sin_addr =
			    ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			break;
		}
		if_addr_runlock(ifp);
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
	nport = htons((u_int16_t)(((struct ipfw_rule_ref *)(mtag+1))->info));
	INP_INFO_RLOCK(&V_divcbinfo);
	LIST_FOREACH(inp, &V_divcb, inp_list) {
		/* XXX why does only one socket match? */
		if (inp->inp_lport == nport) {
			INP_RLOCK(inp);
			sa = inp->inp_socket;
			SOCKBUF_LOCK(&sa->so_rcv);
			if (sbappendaddr_locked(&sa->so_rcv,
			    (struct sockaddr *)&divsrc, m,
			    (struct mbuf *)0) == 0) {
				SOCKBUF_UNLOCK(&sa->so_rcv);
				sa = NULL;	/* force mbuf reclaim below */
			} else
				sorwakeup_locked(sa);
			INP_RUNLOCK(inp);
			break;
		}
	}
	INP_INFO_RUNLOCK(&V_divcbinfo);
	if (sa == NULL) {
		m_freem(m);
		KMOD_IPSTAT_INC(ips_noproto);
		KMOD_IPSTAT_DEC(ips_delivered);
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
div_output(struct socket *so, struct mbuf *m, struct sockaddr_in *sin,
    struct mbuf *control)
{
	struct m_tag *mtag;
	struct ipfw_rule_ref *dt;
	int error = 0;
	struct mbuf *options;

	/*
	 * An mbuf may hasn't come from userland, but we pretend
	 * that it has.
	 */
	m->m_pkthdr.rcvif = NULL;
	m->m_nextpkt = NULL;
	M_SETFIB(m, so->so_fibnum);

	if (control)
		m_freem(control);		/* XXX */

	mtag = m_tag_locate(m, MTAG_IPFW_RULE, 0, NULL);
	if (mtag == NULL) {
		/* this should be normal */
		mtag = m_tag_alloc(MTAG_IPFW_RULE, 0,
		    sizeof(struct ipfw_rule_ref), M_NOWAIT | M_ZERO);
		if (mtag == NULL) {
			error = ENOBUFS;
			goto cantsend;
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

	/* Reinject packet into the system as incoming or outgoing */
	if (!sin || sin->sin_addr.s_addr == 0) {
		struct ip *const ip = mtod(m, struct ip *);
		struct inpcb *inp;

		dt->info |= IPFW_IS_DIVERT | IPFW_INFO_OUT;
		inp = sotoinpcb(so);
		INP_RLOCK(inp);
		/*
		 * Don't allow both user specified and setsockopt options,
		 * and don't allow packet length sizes that will crash
		 */
		if (((ip->ip_hl != (sizeof (*ip) >> 2)) && inp->inp_options) ||
		     ((u_short)ntohs(ip->ip_len) > m->m_pkthdr.len)) {
			error = EINVAL;
			INP_RUNLOCK(inp);
			m_freem(m);
		} else {
			/* Convert fields to host order for ip_output() */
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);

			/* Send packet to output processing */
			KMOD_IPSTAT_INC(ips_rawout);		/* XXX */

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
			options = NULL;
			/*
			 * XXXCSJP: It is unclear to me whether or not it makes
			 * sense for divert sockets to have options.  However,
			 * for now we will duplicate them with the INP locks
			 * held so we can use them in ip_output() without
			 * requring a reference to the pcb.
			 */
			if (inp->inp_options != NULL) {
				options = m_dup(inp->inp_options, M_DONTWAIT);
				if (options == NULL)
					error = ENOBUFS;
			}
			INP_RUNLOCK(inp);
			if (error == ENOBUFS) {
				m_freem(m);
				return (error);
			}
			error = ip_output(m, options, NULL,
			    ((so->so_options & SO_DONTROUTE) ?
			    IP_ROUTETOIF : 0) | IP_ALLOWBROADCAST |
			    IP_RAWOUTPUT, NULL, NULL);
			if (options != NULL)
				m_freem(options);
		}
	} else {
		dt->info |= IPFW_IS_DIVERT | IPFW_INFO_IN;
		if (m->m_pkthdr.rcvif == NULL) {
			/*
			 * No luck with the name, check by IP address.
			 * Clear the port and the ifname to make sure
			 * there are no distractions for ifa_ifwithaddr.
			 */
			struct	ifaddr *ifa;

			bzero(sin->sin_zero, sizeof(sin->sin_zero));
			sin->sin_port = 0;
			ifa = ifa_ifwithaddr((struct sockaddr *) sin);
			if (ifa == NULL) {
				error = EADDRNOTAVAIL;
				goto cantsend;
			}
			m->m_pkthdr.rcvif = ifa->ifa_ifp;
			ifa_free(ifa);
		}
#ifdef MAC
		mac_socket_create_mbuf(so, m);
#endif
		/* Send packet to input processing via netisr */
		netisr_queue_src(NETISR_IP, (uintptr_t)so, m);
	}

	return error;

cantsend:
	m_freem(m);
	return error;
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
	INP_INFO_WLOCK(&V_divcbinfo);
	error = in_pcballoc(so, &V_divcbinfo);
	if (error) {
		INP_INFO_WUNLOCK(&V_divcbinfo);
		return error;
	}
	inp = (struct inpcb *)so->so_pcb;
	INP_INFO_WUNLOCK(&V_divcbinfo);
	inp->inp_ip_p = proto;
	inp->inp_vflag |= INP_IPV4;
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
	INP_INFO_WLOCK(&V_divcbinfo);
	INP_WLOCK(inp);
	in_pcbdetach(inp);
	in_pcbfree(inp);
	INP_INFO_WUNLOCK(&V_divcbinfo);
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
	((struct sockaddr_in *)nam)->sin_addr.s_addr = INADDR_ANY;
	INP_INFO_WLOCK(&V_divcbinfo);
	INP_WLOCK(inp);
	error = in_pcbbind(inp, nam, td->td_ucred);
	INP_WUNLOCK(inp);
	INP_INFO_WUNLOCK(&V_divcbinfo);
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
div_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{

	/* Packet must have a header (but that's about it) */
	if (m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == 0) {
		KMOD_IPSTAT_INC(ips_toosmall);
		m_freem(m);
		return EINVAL;
	}

	/* Send packet */
	return div_output(so, m, (struct sockaddr_in *)nam, control);
}

static void
div_ctlinput(int cmd, struct sockaddr *sa, void *vip)
{
        struct in_addr faddr;

	faddr = ((struct sockaddr_in *)sa)->sin_addr;
	if (sa->sa_family != AF_INET || faddr.s_addr == INADDR_ANY)
        	return;
	if (PRC_IS_REDIRECT(cmd))
		return;
}

static int
div_pcblist(SYSCTL_HANDLER_ARGS)
{
	int error, i, n;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == 0) {
		n = V_divcbinfo.ipi_count;
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xinpcb);
		return 0;
	}

	if (req->newptr != 0)
		return EPERM;

	/*
	 * OK, now we're committed to doing something.
	 */
	INP_INFO_RLOCK(&V_divcbinfo);
	gencnt = V_divcbinfo.ipi_gencnt;
	n = V_divcbinfo.ipi_count;
	INP_INFO_RUNLOCK(&V_divcbinfo);

	error = sysctl_wire_old_buffer(req,
	    2 * sizeof(xig) + n*sizeof(struct xinpcb));
	if (error != 0)
		return (error);

	xig.xig_len = sizeof xig;
	xig.xig_count = n;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return error;

	inp_list = malloc(n * sizeof *inp_list, M_TEMP, M_WAITOK);
	if (inp_list == 0)
		return ENOMEM;
	
	INP_INFO_RLOCK(&V_divcbinfo);
	for (inp = LIST_FIRST(V_divcbinfo.ipi_listhead), i = 0; inp && i < n;
	     inp = LIST_NEXT(inp, inp_list)) {
		INP_WLOCK(inp);
		if (inp->inp_gencnt <= gencnt &&
		    cr_canseeinpcb(req->td->td_ucred, inp) == 0) {
			in_pcbref(inp);
			inp_list[i++] = inp;
		}
		INP_WUNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&V_divcbinfo);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_RLOCK(inp);
		if (inp->inp_gencnt <= gencnt) {
			struct xinpcb xi;
			bzero(&xi, sizeof(xi));
			xi.xi_len = sizeof xi;
			/* XXX should avoid extra copy */
			bcopy(inp, &xi.xi_inp, sizeof *inp);
			if (inp->inp_socket)
				sotoxsocket(inp->inp_socket, &xi.xi_socket);
			INP_RUNLOCK(inp);
			error = SYSCTL_OUT(req, &xi, sizeof xi);
		} else
			INP_RUNLOCK(inp);
	}
	INP_INFO_WLOCK(&V_divcbinfo);
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_WLOCK(inp);
		if (!in_pcbrele(inp))
			INP_WUNLOCK(inp);
	}
	INP_INFO_WUNLOCK(&V_divcbinfo);

	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		INP_INFO_RLOCK(&V_divcbinfo);
		xig.xig_gen = V_divcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_divcbinfo.ipi_count;
		INP_INFO_RUNLOCK(&V_divcbinfo);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return error;
}

#ifdef SYSCTL_NODE
SYSCTL_NODE(_net_inet, IPPROTO_DIVERT, divert, CTLFLAG_RW, 0, "IPDIVERT");
SYSCTL_PROC(_net_inet_divert, OID_AUTO, pcblist, CTLTYPE_OPAQUE | CTLFLAG_RD,
    NULL, 0, div_pcblist, "S,xinpcb", "List of active divert sockets");
#endif

struct pr_usrreqs div_usrreqs = {
	.pru_attach =		div_attach,
	.pru_bind =		div_bind,
	.pru_control =		in_control,
	.pru_detach =		div_detach,
	.pru_peeraddr =		in_getpeeraddr,
	.pru_send =		div_send,
	.pru_shutdown =		div_shutdown,
	.pru_sockaddr =		in_getsockaddr,
	.pru_sosetlabel =	in_pcbsosetlabel
};

struct protosw div_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_protocol =		IPPROTO_DIVERT,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		div_input,
	.pr_ctlinput =		div_ctlinput,
	.pr_ctloutput =		ip_ctloutput,
	.pr_init =		div_init,
#ifdef VIMAGE
	.pr_destroy =		div_destroy,
#endif
	.pr_usrreqs =		&div_usrreqs
};

static int
div_modevent(module_t mod, int type, void *unused)
{
	int err = 0;
#ifndef VIMAGE
	int n;
#endif

	switch (type) {
	case MOD_LOAD:
		/*
		 * Protocol will be initialized by pf_proto_register().
		 * We don't have to register ip_protox because we are not
		 * a true IP protocol that goes over the wire.
		 */
		err = pf_proto_register(PF_INET, &div_protosw);
		if (err != 0)
			return (err);
		ip_divert_ptr = divert_packet;
		ip_divert_event_tag = EVENTHANDLER_REGISTER(maxsockets_change,
		    div_zone_change, NULL, EVENTHANDLER_PRI_ANY);
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
#ifdef VIMAGE
		err = EPERM;
		break;
#else
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
		 */
		INP_INFO_WLOCK(&V_divcbinfo);
		n = V_divcbinfo.ipi_count;
		if (n != 0) {
			err = EBUSY;
			INP_INFO_WUNLOCK(&V_divcbinfo);
			break;
		}
		ip_divert_ptr = NULL;
		err = pf_proto_unregister(PF_INET, IPPROTO_DIVERT, SOCK_RAW);
		INP_INFO_WUNLOCK(&V_divcbinfo);
		div_destroy();
		EVENTHANDLER_DEREGISTER(maxsockets_change, ip_divert_event_tag);
		break;
#endif /* !VIMAGE */
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

DECLARE_MODULE(ipdivert, ipdivertmod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_DEPEND(ipdivert, ipfw, 2, 2, 2);
MODULE_VERSION(ipdivert, 1);
