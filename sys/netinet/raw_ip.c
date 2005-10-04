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
 *	@(#)raw_ip.c	8.7 (Berkeley) 5/15/95
 * $FreeBSD$
 */

#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_mroute.h>

#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#endif /*FAST_IPSEC*/

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

struct	inpcbhead ripcb;
struct	inpcbinfo ripcbinfo;

/* control hooks for ipfw and dummynet */
ip_fw_ctl_t *ip_fw_ctl_ptr = NULL;
ip_dn_ctl_t *ip_dn_ctl_ptr = NULL;

/*
 * hooks for multicast routing. They all default to NULL,
 * so leave them not initialized and rely on BSS being set to 0.
 */

/* The socket used to communicate with the multicast routing daemon.  */
struct socket  *ip_mrouter;

/* The various mrouter and rsvp functions */
int (*ip_mrouter_set)(struct socket *, struct sockopt *);
int (*ip_mrouter_get)(struct socket *, struct sockopt *);
int (*ip_mrouter_done)(void);
int (*ip_mforward)(struct ip *, struct ifnet *, struct mbuf *,
		   struct ip_moptions *);
int (*mrt_ioctl)(int, caddr_t);
int (*legal_vif_num)(int);
u_long (*ip_mcast_src)(int);

void (*rsvp_input_p)(struct mbuf *m, int off);
int (*ip_rsvp_vif)(struct socket *, struct sockopt *);
void (*ip_rsvp_force_done)(struct socket *);

/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPSNDQ		8192
#define	RIPRCVQ		8192

/*
 * Raw interface to IP protocol.
 */

/*
 * Initialize raw connection block q.
 */
void
rip_init()
{
	INP_INFO_LOCK_INIT(&ripcbinfo, "rip");
	LIST_INIT(&ripcb);
	ripcbinfo.listhead = &ripcb;
	/*
	 * XXX We don't use the hash list for raw IP, but it's easier
	 * to allocate a one entry hash list than it is to check all
	 * over the place for hashbase == NULL.
	 */
	ripcbinfo.hashbase = hashinit(1, M_PCB, &ripcbinfo.hashmask);
	ripcbinfo.porthashbase = hashinit(1, M_PCB, &ripcbinfo.porthashmask);
	ripcbinfo.ipi_zone = uma_zcreate("ripcb", sizeof(struct inpcb),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(ripcbinfo.ipi_zone, maxsockets);
}

static struct	sockaddr_in ripsrc = { sizeof(ripsrc), AF_INET };

static int
raw_append(struct inpcb *last, struct ip *ip, struct mbuf *n)
{
	int policyfail = 0;

	INP_LOCK_ASSERT(last);

#if defined(IPSEC) || defined(FAST_IPSEC)
	/* check AH/ESP integrity. */
	if (ipsec4_in_reject(n, last)) {
		policyfail = 1;
#ifdef IPSEC
		ipsecstat.in_polvio++;
#endif /*IPSEC*/
		/* do not inject data to pcb */
	}
#endif /*IPSEC || FAST_IPSEC*/
#ifdef MAC
	if (!policyfail && mac_check_inpcb_deliver(last, n) != 0)
		policyfail = 1;
#endif
	/* Check the minimum TTL for socket. */
	if (last->inp_ip_minttl && last->inp_ip_minttl > ip->ip_ttl)
		policyfail = 1;
	if (!policyfail) {
		struct mbuf *opts = NULL;
		struct socket *so;

		so = last->inp_socket;
		if ((last->inp_flags & INP_CONTROLOPTS) ||
		    (so->so_options & SO_TIMESTAMP | SO_BINTIME))
			ip_savecontrol(last, &opts, ip, n);
		SOCKBUF_LOCK(&so->so_rcv);
		if (sbappendaddr_locked(&so->so_rcv,
		    (struct sockaddr *)&ripsrc, n, opts) == 0) {
			/* should notify about lost packet */
			m_freem(n);
			if (opts)
				m_freem(opts);
			SOCKBUF_UNLOCK(&so->so_rcv);
		} else
			sorwakeup_locked(so);
	} else
		m_freem(n);
	return policyfail;
}

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
rip_input(struct mbuf *m, int off)
{
	struct ip *ip = mtod(m, struct ip *);
	int proto = ip->ip_p;
	struct inpcb *inp, *last;

	INP_INFO_RLOCK(&ripcbinfo);
	ripsrc.sin_addr = ip->ip_src;
	last = NULL;
	LIST_FOREACH(inp, &ripcb, inp_list) {
		INP_LOCK(inp);
		if (inp->inp_ip_p && inp->inp_ip_p != proto) {
	docontinue:
			INP_UNLOCK(inp);
			continue;
		}
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV4) == 0)
			goto docontinue;
#endif
		if (inp->inp_laddr.s_addr &&
		    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
			goto docontinue;
		if (inp->inp_faddr.s_addr &&
		    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
			goto docontinue;
		if (jailed(inp->inp_socket->so_cred))
			if (htonl(prison_getip(inp->inp_socket->so_cred)) !=
			    ip->ip_dst.s_addr)
				goto docontinue;
		if (last) {
			struct mbuf *n;

			n = m_copy(m, 0, (int)M_COPYALL);
			if (n != NULL)
				(void) raw_append(last, ip, n);
			/* XXX count dropped packet */
			INP_UNLOCK(last);
		}
		last = inp;
	}
	if (last != NULL) {
		if (raw_append(last, ip, m) != 0)
			ipstat.ips_delivered--;
		INP_UNLOCK(last);
	} else {
		m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
	}
	INP_INFO_RUNLOCK(&ripcbinfo);
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
rip_output(struct mbuf *m, struct socket *so, u_long dst)
{
	struct ip *ip;
	int error;
	struct inpcb *inp = sotoinpcb(so);
	int flags = ((so->so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0) |
	    IP_ALLOWBROADCAST;

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		if (m->m_pkthdr.len + sizeof(struct ip) > IP_MAXPACKET) {
			m_freem(m);
			return(EMSGSIZE);
		}
		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (m == NULL)
			return(ENOBUFS);

		INP_LOCK(inp);
		ip = mtod(m, struct ip *);
		ip->ip_tos = inp->inp_ip_tos;
		if (inp->inp_flags & INP_DONTFRAG)
			ip->ip_off = IP_DF;
		else
			ip->ip_off = 0;
		ip->ip_p = inp->inp_ip_p;
		ip->ip_len = m->m_pkthdr.len;
		if (jailed(inp->inp_socket->so_cred))
			ip->ip_src.s_addr =
			    htonl(prison_getip(inp->inp_socket->so_cred));
		else
			ip->ip_src = inp->inp_laddr;
		ip->ip_dst.s_addr = dst;
		ip->ip_ttl = inp->inp_ip_ttl;
	} else {
		if (m->m_pkthdr.len > IP_MAXPACKET) {
			m_freem(m);
			return(EMSGSIZE);
		}
		INP_LOCK(inp);
		ip = mtod(m, struct ip *);
		if (jailed(inp->inp_socket->so_cred)) {
			if (ip->ip_src.s_addr !=
			    htonl(prison_getip(inp->inp_socket->so_cred))) {
				INP_UNLOCK(inp);
				m_freem(m);
				return (EPERM);
			}
		}
		/* don't allow both user specified and setsockopt options,
		   and don't allow packet length sizes that will crash */
		if (((ip->ip_hl != (sizeof (*ip) >> 2))
		     && inp->inp_options)
		    || (ip->ip_len > m->m_pkthdr.len)
		    || (ip->ip_len < (ip->ip_hl << 2))) {
			INP_UNLOCK(inp);
			m_freem(m);
			return EINVAL;
		}
		if (ip->ip_id == 0)
			ip->ip_id = ip_newid();
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		ipstat.ips_rawout++;
	}

	if (inp->inp_flags & INP_ONESBCAST)
		flags |= IP_SENDONES;

#ifdef MAC
	mac_create_mbuf_from_inpcb(inp, m);
#endif

	error = ip_output(m, inp->inp_options, NULL, flags,
	    inp->inp_moptions, inp);
	INP_UNLOCK(inp);
	return error;
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
 * Unilaterally checking suser() here breaks normal IP socket option
 * operations on raw sockets.
 *
 * When adding new socket options here, make sure to add access control
 * checks here as necessary.
 */
int
rip_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct	inpcb *inp = sotoinpcb(so);
	int	error, optval;

	if (sopt->sopt_level != IPPROTO_IP)
		return (EINVAL);

	error = 0;
	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			optval = inp->inp_flags & INP_HDRINCL;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;

		case IP_FW_ADD:	/* ADD actually returns the body... */
		case IP_FW_GET:
		case IP_FW_TABLE_GETSIZE:
		case IP_FW_TABLE_LIST:
			error = suser(curthread);
			if (error != 0)
				return (error);
			if (ip_fw_ctl_ptr != NULL)
				error = ip_fw_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT;
			break;

		case IP_DUMMYNET_GET:
			error = suser(curthread);
			if (error != 0)
				return (error);
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
			error = suser(curthread);
			if (error != 0)
				return (error);
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

		case IP_FW_ADD:
		case IP_FW_DEL:
		case IP_FW_FLUSH:
		case IP_FW_ZERO:
		case IP_FW_RESETLOG:
		case IP_FW_TABLE_ADD:
		case IP_FW_TABLE_DEL:
		case IP_FW_TABLE_FLUSH:
			error = suser(curthread);
			if (error != 0)
				return (error);
			if (ip_fw_ctl_ptr != NULL)
				error = ip_fw_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT;
			break;

		case IP_DUMMYNET_CONFIGURE:
		case IP_DUMMYNET_DEL:
		case IP_DUMMYNET_FLUSH:
			error = suser(curthread);
			if (error != 0)
				return (error);
			if (ip_dn_ctl_ptr != NULL)
				error = ip_dn_ctl_ptr(sopt);
			else
				error = ENOPROTOOPT ;
			break ;

		case IP_RSVP_ON:
			error = suser(curthread);
			if (error != 0)
				return (error);
			error = ip_rsvp_init(so);
			break;

		case IP_RSVP_OFF:
			error = suser(curthread);
			if (error != 0)
				return (error);
			error = ip_rsvp_done();
			break;

		case IP_RSVP_VIF_ON:
		case IP_RSVP_VIF_OFF:
			error = suser(curthread);
			if (error != 0)
				return (error);
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
			error = suser(curthread);
			if (error != 0)
				return (error);
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

/*
 * This function exists solely to receive the PRC_IFDOWN messages which
 * are sent by if_down().  It looks for an ifaddr whose ifa_addr is sa,
 * and calls in_ifadown() to remove all routes corresponding to that address.
 * It also receives the PRC_IFUP messages from if_up() and reinstalls the
 * interface routes.
 */
void
rip_ctlinput(int cmd, struct sockaddr *sa, void *vip)
{
	struct in_ifaddr *ia;
	struct ifnet *ifp;
	int err;
	int flags;

	switch (cmd) {
	case PRC_IFDOWN:
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
			if (ia->ia_ifa.ifa_addr == sa
			    && (ia->ia_flags & IFA_ROUTE)) {
				/*
				 * in_ifscrub kills the interface route.
				 */
				in_ifscrub(ia->ia_ifp, ia);
				/*
				 * in_ifadown gets rid of all the rest of
				 * the routes.  This is not quite the right
				 * thing to do, but at least if we are running
				 * a routing process they will come back.
				 */
				in_ifadown(&ia->ia_ifa, 0);
				break;
			}
		}
		break;

	case PRC_IFUP:
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
			if (ia->ia_ifa.ifa_addr == sa)
				break;
		}
		if (ia == 0 || (ia->ia_flags & IFA_ROUTE))
			return;
		flags = RTF_UP;
		ifp = ia->ia_ifa.ifa_ifp;

		if ((ifp->if_flags & IFF_LOOPBACK)
		    || (ifp->if_flags & IFF_POINTOPOINT))
			flags |= RTF_HOST;

		err = rtinit(&ia->ia_ifa, RTM_ADD, flags);
		if (err == 0)
			ia->ia_flags |= IFA_ROUTE;
		break;
	}
}

u_long	rip_sendspace = RIPSNDQ;
u_long	rip_recvspace = RIPRCVQ;

SYSCTL_INT(_net_inet_raw, OID_AUTO, maxdgram, CTLFLAG_RW,
    &rip_sendspace, 0, "Maximum outgoing raw IP datagram size");
SYSCTL_INT(_net_inet_raw, OID_AUTO, recvspace, CTLFLAG_RW,
    &rip_recvspace, 0, "Maximum space for incoming raw IP datagrams");

static int
rip_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	int error;

	/* XXX why not lower? */
	INP_INFO_WLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (inp) {
		/* XXX counter, printf */
		INP_INFO_WUNLOCK(&ripcbinfo);
		return EINVAL;
	}
	if (jailed(td->td_ucred) && !jail_allow_raw_sockets) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return (EPERM);
	}
	if ((error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL)) != 0) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return error;
	}
	if (proto >= IPPROTO_MAX || proto < 0) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return EPROTONOSUPPORT;
	}

	error = soreserve(so, rip_sendspace, rip_recvspace);
	if (error) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return error;
	}
	error = in_pcballoc(so, &ripcbinfo, "rawinp");
	if (error) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return error;
	}
	inp = (struct inpcb *)so->so_pcb;
	INP_LOCK(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	inp->inp_vflag |= INP_IPV4;
	inp->inp_ip_p = proto;
	inp->inp_ip_ttl = ip_defttl;
	INP_UNLOCK(inp);
	return 0;
}

static void
rip_pcbdetach(struct socket *so, struct inpcb *inp)
{

	INP_INFO_WLOCK_ASSERT(&ripcbinfo);
	INP_LOCK_ASSERT(inp);

	if (so == ip_mrouter && ip_mrouter_done)
		ip_mrouter_done();
	if (ip_rsvp_force_done)
		ip_rsvp_force_done(so);
	if (so == ip_rsvpd)
		ip_rsvp_done();
	in_pcbdetach(inp);
}

static int
rip_detach(struct socket *so)
{
	struct inpcb *inp;

	INP_INFO_WLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		/* XXX counter, printf */
		INP_INFO_WUNLOCK(&ripcbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	rip_pcbdetach(so, inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return 0;
}

static int
rip_abort(struct socket *so)
{
	struct inpcb *inp;

	INP_INFO_WLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return EINVAL;	/* ??? possible? panic instead? */
	}
	INP_LOCK(inp);
	soisdisconnected(so);
	if (so->so_state & SS_NOFDREF)
		rip_pcbdetach(so, inp);
	else
		INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return 0;
}

static int
rip_disconnect(struct socket *so)
{
	if ((so->so_state & SS_ISCONNECTED) == 0)
		return ENOTCONN;
	return rip_abort(so);
}

static int
rip_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)nam;
	struct inpcb *inp;

	if (nam->sa_len != sizeof(*addr))
		return EINVAL;

	if (jailed(td->td_ucred)) {
		if (addr->sin_addr.s_addr == INADDR_ANY)
			addr->sin_addr.s_addr =
			    htonl(prison_getip(td->td_ucred));
		if (htonl(prison_getip(td->td_ucred)) != addr->sin_addr.s_addr)
			return (EADDRNOTAVAIL);
	}

	if (TAILQ_EMPTY(&ifnet) ||
	    (addr->sin_family != AF_INET && addr->sin_family != AF_IMPLINK) ||
	    (addr->sin_addr.s_addr &&
	     ifa_ifwithaddr((struct sockaddr *)addr) == 0))
		return EADDRNOTAVAIL;

	INP_INFO_WLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	inp->inp_laddr = addr->sin_addr;
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return 0;
}

static int
rip_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)nam;
	struct inpcb *inp;

	if (nam->sa_len != sizeof(*addr))
		return EINVAL;
	if (TAILQ_EMPTY(&ifnet))
		return EADDRNOTAVAIL;
	if (addr->sin_family != AF_INET && addr->sin_family != AF_IMPLINK)
		return EAFNOSUPPORT;

	INP_INFO_WLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&ripcbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	inp->inp_faddr = addr->sin_addr;
	soisconnected(so);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return 0;
}

static int
rip_shutdown(struct socket *so)
{
	struct inpcb *inp;

	INP_INFO_RLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_RUNLOCK(&ripcbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&ripcbinfo);
	socantsendmore(so);
	INP_UNLOCK(inp);
	return 0;
}

static int
rip_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
	 struct mbuf *control, struct thread *td)
{
	struct inpcb *inp;
	u_long dst;
	int ret;

	INP_INFO_WLOCK(&ripcbinfo);
	inp = sotoinpcb(so);
	if (so->so_state & SS_ISCONNECTED) {
		if (nam) {
			INP_INFO_WUNLOCK(&ripcbinfo);
			m_freem(m);
			return EISCONN;
		}
		dst = inp->inp_faddr.s_addr;
	} else {
		if (nam == NULL) {
			INP_INFO_WUNLOCK(&ripcbinfo);
			m_freem(m);
			return ENOTCONN;
		}
		dst = ((struct sockaddr_in *)nam)->sin_addr.s_addr;
	}
	ret = rip_output(m, so, dst);
	INP_INFO_WUNLOCK(&ripcbinfo);
	return ret;
}

static int
rip_pcblist(SYSCTL_HANDLER_ARGS)
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
		n = ripcbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
			+ (n + n/8) * sizeof(struct xinpcb);
		return 0;
	}

	if (req->newptr != 0)
		return EPERM;

	/*
	 * OK, now we're committed to doing something.
	 */
	INP_INFO_RLOCK(&ripcbinfo);
	gencnt = ripcbinfo.ipi_gencnt;
	n = ripcbinfo.ipi_count;
	INP_INFO_RUNLOCK(&ripcbinfo);

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
	
	INP_INFO_RLOCK(&ripcbinfo);
	for (inp = LIST_FIRST(ripcbinfo.listhead), i = 0; inp && i < n;
	     inp = LIST_NEXT(inp, inp_list)) {
		INP_LOCK(inp);
		if (inp->inp_gencnt <= gencnt &&
		    cr_canseesocket(req->td->td_ucred, inp->inp_socket) == 0) {
			/* XXX held references? */
			inp_list[i++] = inp;
		}
		INP_UNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&ripcbinfo);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		if (inp->inp_gencnt <= gencnt) {
			struct xinpcb xi;
			bzero(&xi, sizeof(xi));
			xi.xi_len = sizeof xi;
			/* XXX should avoid extra copy */
			bcopy(inp, &xi.xi_inp, sizeof *inp);
			if (inp->inp_socket)
				sotoxsocket(inp->inp_socket, &xi.xi_socket);
			error = SYSCTL_OUT(req, &xi, sizeof xi);
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
		INP_INFO_RLOCK(&ripcbinfo);
		xig.xig_gen = ripcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = ripcbinfo.ipi_count;
		INP_INFO_RUNLOCK(&ripcbinfo);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return error;
}

/*
 * This is the wrapper function for in_setsockaddr.  We just pass down
 * the pcbinfo for in_setpeeraddr to lock.
 */
static int
rip_sockaddr(struct socket *so, struct sockaddr **nam)
{
	return (in_setsockaddr(so, nam, &ripcbinfo));
}

/*
 * This is the wrapper function for in_setpeeraddr.  We just pass down
 * the pcbinfo for in_setpeeraddr to lock.
 */
static int
rip_peeraddr(struct socket *so, struct sockaddr **nam)
{
	return (in_setpeeraddr(so, nam, &ripcbinfo));
}


SYSCTL_PROC(_net_inet_raw, OID_AUTO/*XXX*/, pcblist, CTLFLAG_RD, 0, 0,
	    rip_pcblist, "S,xinpcb", "List of active raw IP sockets");

struct pr_usrreqs rip_usrreqs = {
	.pru_abort =		rip_abort,
	.pru_attach =		rip_attach,
	.pru_bind =		rip_bind,
	.pru_connect =		rip_connect,
	.pru_control =		in_control,
	.pru_detach =		rip_detach,
	.pru_disconnect =	rip_disconnect,
	.pru_peeraddr =		rip_peeraddr,
	.pru_send =		rip_send,
	.pru_shutdown =		rip_shutdown,
	.pru_sockaddr =		rip_sockaddr,
	.pru_sosetlabel =	in_pcbsosetlabel
};
