/*
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
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
 *	@(#)in_pcb.c	8.4 (Berkeley) 5/24/95
 *	$Id: in_pcb.c,v 1.35 1997/10/28 15:58:42 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

struct	in_addr zeroin_addr;

static void	 in_pcbinshash __P((struct inpcb *));
static void	 in_rtchange __P((struct inpcb *, int));

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
static int ipport_lowfirstauto  = IPPORT_RESERVED - 1;	/* 1023 */
static int ipport_lowlastauto = IPPORT_RESERVEDSTART;	/* 600 */
static int ipport_firstauto = IPPORT_RESERVED;		/* 1024 */
static int ipport_lastauto  = IPPORT_USERRESERVED;	/* 5000 */
static int ipport_hifirstauto = IPPORT_HIFIRSTAUTO;	/* 40000 */
static int ipport_hilastauto  = IPPORT_HILASTAUTO;	/* 44999 */

#define RANGECHK(var, min, max) \
	if ((var) < (min)) { (var) = (min); } \
	else if ((var) > (max)) { (var) = (max); }

static int
sysctl_net_ipport_check SYSCTL_HANDLER_ARGS
{
	int error = sysctl_handle_int(oidp,
		oidp->oid_arg1, oidp->oid_arg2, req);
	if (!error) {
		RANGECHK(ipport_lowfirstauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(ipport_lowlastauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(ipport_firstauto, IPPORT_RESERVED, USHRT_MAX);
		RANGECHK(ipport_lastauto, IPPORT_RESERVED, USHRT_MAX);
		RANGECHK(ipport_hifirstauto, IPPORT_RESERVED, USHRT_MAX);
		RANGECHK(ipport_hilastauto, IPPORT_RESERVED, USHRT_MAX);
	}
	return error;
}

#undef RANGECHK

SYSCTL_NODE(_net_inet_ip, IPPROTO_IP, portrange, CTLFLAG_RW, 0, "IP Ports");

SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowfirst, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lowfirstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowlast, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lowlastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, first, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_firstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, last, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hifirst, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_hifirstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hilast, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_hilastauto, 0, &sysctl_net_ipport_check, "I", "");

int
in_pcballoc(so, pcbinfo, p)
	struct socket *so;
	struct inpcbinfo *pcbinfo;
	struct proc *p;
{
	register struct inpcb *inp;
	int s;

	MALLOC(inp, struct inpcb *, sizeof(*inp), M_PCB, 
	       p ? M_WAITOK : M_NOWAIT);
	if (inp == NULL)
		return (ENOBUFS);
	bzero((caddr_t)inp, sizeof(*inp));
	inp->inp_pcbinfo = pcbinfo;
	inp->inp_socket = so;
	s = splnet();
	LIST_INSERT_HEAD(pcbinfo->listhead, inp, inp_list);
	in_pcbinshash(inp);
	splx(s);
	so->so_pcb = (caddr_t)inp;
	return (0);
}

int
in_pcbbind(inp, nam, p)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct proc *p;
{
	register struct socket *so = inp->inp_socket;
	unsigned short *lastport;
	struct sockaddr_in *sin;
	u_short lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error;

	if (TAILQ_EMPTY(&in_ifaddrhead)) /* XXX broken! */
		return (EADDRNOTAVAIL);
	if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	     (so->so_options & SO_ACCEPTCONN) == 0))
		wild = 1;
	if (nam) {
		sin = (struct sockaddr_in *)nam;
		if (nam->sa_len != sizeof (*sin))
			return (EINVAL);
#ifdef notdef
		/*
		 * We should check the family, but old programs
		 * incorrectly fail to initialize it.
		 */
		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);
#endif
		lport = sin->sin_port;
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow complete duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (sin->sin_addr.s_addr != INADDR_ANY) {
			sin->sin_port = 0;		/* yech... */
			if (ifa_ifwithaddr((struct sockaddr *)sin) == 0)
				return (EADDRNOTAVAIL);
		}
		if (lport) {
			struct inpcb *t;

			/* GROSS */
			if (ntohs(lport) < IPPORT_RESERVED && p &&
			    suser(p->p_ucred, &p->p_acflag))
				return (EACCES);
			t = in_pcblookup(inp->inp_pcbinfo, zeroin_addr, 0,
			    sin->sin_addr, lport, wild);
			if (t && (reuseport & t->inp_socket->so_options) == 0)
				return (EADDRINUSE);
		}
		inp->inp_laddr = sin->sin_addr;
	}
	if (lport == 0) {
		ushort first, last;
		int count;

		inp->inp_flags |= INP_ANONPORT;

		if (inp->inp_flags & INP_HIGHPORT) {
			first = ipport_hifirstauto;	/* sysctl */
			last  = ipport_hilastauto;
			lastport = &inp->inp_pcbinfo->lasthi;
		} else if (inp->inp_flags & INP_LOWPORT) {
			if (p && (error = suser(p->p_ucred, &p->p_acflag)))
				return error;
			first = ipport_lowfirstauto;	/* 1023 */
			last  = ipport_lowlastauto;	/* 600 */
			lastport = &inp->inp_pcbinfo->lastlow;
		} else {
			first = ipport_firstauto;	/* sysctl */
			last  = ipport_lastauto;
			lastport = &inp->inp_pcbinfo->lastport;
		}
		/*
		 * Simple check to ensure all ports are not used up causing
		 * a deadlock here.
		 *
		 * We split the two cases (up and down) so that the direction
		 * is not being tested on each round of the loop.
		 */
		if (first > last) {
			/*
			 * counting down
			 */
			count = first - last;

			do {
				if (count-- <= 0)	/* completely used? */
					return (EADDRNOTAVAIL);
				--*lastport;
				if (*lastport > first || *lastport < last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup(inp->inp_pcbinfo,
				 zeroin_addr, 0, inp->inp_laddr, lport, wild));
		} else {
			/*
			 * counting up
			 */
			count = last - first;

			do {
				if (count-- <= 0)	/* completely used? */
					return (EADDRNOTAVAIL);
				++*lastport;
				if (*lastport < first || *lastport > last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup(inp->inp_pcbinfo,
				 zeroin_addr, 0, inp->inp_laddr, lport, wild));
		}
	}
	inp->inp_lport = lport;
	in_pcbrehash(inp);
	return (0);
}

/*
 *   Transform old in_pcbconnect() into an inner subroutine for new
 *   in_pcbconnect(): Do some validity-checking on the remote
 *   address (in mbuf 'nam') and then determine local host address
 *   (i.e., which interface) to use to access that remote host.
 *
 *   This preserves definition of in_pcbconnect(), while supporting a
 *   slightly different version for T/TCP.  (This is more than
 *   a bit of a kludge, but cleaning up the internal interfaces would
 *   have forced minor changes in every protocol).
 */

int
in_pcbladdr(inp, nam, plocal_sin)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct sockaddr_in **plocal_sin;
{
	struct in_ifaddr *ia;
	register struct sockaddr_in *sin = (struct sockaddr_in *)nam;

	if (nam->sa_len != sizeof (*sin))
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	if (!TAILQ_EMPTY(&in_ifaddrhead)) {
		/*
		 * If the destination address is INADDR_ANY,
		 * use the primary local address.
		 * If the supplied address is INADDR_BROADCAST,
		 * and the primary interface supports broadcast,
		 * choose the broadcast address for that interface.
		 */
#define	satosin(sa)	((struct sockaddr_in *)(sa))
#define sintosa(sin)	((struct sockaddr *)(sin))
#define ifatoia(ifa)	((struct in_ifaddr *)(ifa))
		if (sin->sin_addr.s_addr == INADDR_ANY)
		    sin->sin_addr = IA_SIN(in_ifaddrhead.tqh_first)->sin_addr;
		else if (sin->sin_addr.s_addr == (u_long)INADDR_BROADCAST &&
		  (in_ifaddrhead.tqh_first->ia_ifp->if_flags & IFF_BROADCAST))
		    sin->sin_addr = satosin(&in_ifaddrhead.tqh_first->ia_broadaddr)->sin_addr;
	}
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		register struct route *ro;

		ia = (struct in_ifaddr *)0;
		/*
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */
		ro = &inp->inp_route;
		if (ro->ro_rt &&
		    (satosin(&ro->ro_dst)->sin_addr.s_addr !=
			sin->sin_addr.s_addr ||
		    inp->inp_socket->so_options & SO_DONTROUTE)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
		if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
		    (ro->ro_rt == (struct rtentry *)0 ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
			/* No route yet, so try to acquire one */
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
				sin->sin_addr;
			rtalloc(ro);
		}
		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 * unless it is the loopback (in case a route
		 * to our address on another net goes to loopback).
		 */
		if (ro->ro_rt && !(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
			ia = ifatoia(ro->ro_rt->rt_ifa);
		if (ia == 0) {
			u_short fport = sin->sin_port;

			sin->sin_port = 0;
			ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
			if (ia == 0)
				ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
			sin->sin_port = fport;
			if (ia == 0)
				ia = in_ifaddrhead.tqh_first;
			if (ia == 0)
				return (EADDRNOTAVAIL);
		}
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
		    inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				for (ia = in_ifaddrhead.tqh_first; ia; 
				     ia = ia->ia_link.tqe_next)
					if (ia->ia_ifp == ifp)
						break;
				if (ia == 0)
					return (EADDRNOTAVAIL);
			}
		}
	/*
	 * Don't do pcblookup call here; return interface in plocal_sin
	 * and exit to caller, that will do the lookup.
	 */
		*plocal_sin = &ia->ia_addr;

	}
	return(0);
}

/*
 * Outer subroutine:
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(inp, nam, p)
	register struct inpcb *inp;
	struct sockaddr *nam;
	struct proc *p;
{
	struct sockaddr_in *ifaddr;
	register struct sockaddr_in *sin = (struct sockaddr_in *)nam;
	int error;

	/*
	 *   Call inner routine, to assign local interface address.
	 */
	if (error = in_pcbladdr(inp, nam, &ifaddr))
		return(error);

	if (in_pcblookuphash(inp->inp_pcbinfo, sin->sin_addr, sin->sin_port,
	    inp->inp_laddr.s_addr ? inp->inp_laddr : ifaddr->sin_addr,
	    inp->inp_lport, 0) != NULL)
		return (EADDRINUSE);
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		if (inp->inp_lport == 0)
			(void)in_pcbbind(inp, (struct sockaddr *)0, p);
		inp->inp_laddr = ifaddr->sin_addr;
	}
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	in_pcbrehash(inp);
	return (0);
}

void
in_pcbdisconnect(inp)
	struct inpcb *inp;
{

	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_fport = 0;
	in_pcbrehash(inp);
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
}

void
in_pcbdetach(inp)
	struct inpcb *inp;
{
	struct socket *so = inp->inp_socket;
	int s;

	so->so_pcb = 0;
	sofree(so);
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
	if (inp->inp_route.ro_rt)
		rtfree(inp->inp_route.ro_rt);
	ip_freemoptions(inp->inp_moptions);
	s = splnet();
	LIST_REMOVE(inp, inp_hash);
	LIST_REMOVE(inp, inp_list);
	splx(s);
	FREE(inp, M_PCB);
}

/*
 * The calling convention of in_setsockaddr() and in_setpeeraddr() was
 * modified to match the pru_sockaddr() and pru_peeraddr() entry points
 * in struct pr_usrreqs, so that protocols can just reference then directly
 * without the need for a wrapper function.  The socket must have a valid
 * (i.e., non-nil) PCB, but it should be impossible to get an invalid one
 * except through a kernel programming error, so it is acceptable to panic
 * (or in this case trap) if the PCB is invalid.  (Actually, we don't trap
 * because there actually /is/ a programming error somewhere... XXX)
 */
int
in_setsockaddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	register struct inpcb *inp;
	register struct sockaddr_in *sin;

	s = splnet();
	inp = sotoinpcb(so);
	if (!inp) {
		splx(s);
		return EINVAL;
	}
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME, M_WAITOK);
	*nam = (struct sockaddr *)sin;
	bzero(sin, sizeof *sin);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_lport;
	sin->sin_addr = inp->inp_laddr;
	splx(s);
	return 0;
}

int
in_setpeeraddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	struct inpcb *inp;
	register struct sockaddr_in *sin;

	s = splnet();
	inp = sotoinpcb(so);
	if (!inp) {
		splx(s);
		return EINVAL;
	}
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME, M_WAITOK);
	*nam = (struct sockaddr *)sin;
	bzero((caddr_t)sin, sizeof (*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_fport;
	sin->sin_addr = inp->inp_faddr;
	splx(s);
	return 0;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splnet.
 */
void
in_pcbnotify(head, dst, fport_arg, laddr, lport_arg, cmd, notify)
	struct inpcbhead *head;
	struct sockaddr *dst;
	u_int fport_arg, lport_arg;
	struct in_addr laddr;
	int cmd;
	void (*notify) __P((struct inpcb *, int));
{
	register struct inpcb *inp, *oinp;
	struct in_addr faddr;
	u_short fport = fport_arg, lport = lport_arg;
	int errno, s;

	if ((unsigned)cmd > PRC_NCMDS || dst->sa_family != AF_INET)
		return;
	faddr = ((struct sockaddr_in *)dst)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return;

	/*
	 * Redirects go to all references to the destination,
	 * and use in_rtchange to invalidate the route cache.
	 * Dead host indications: notify all references to the destination.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		laddr.s_addr = 0;
		if (cmd != PRC_HOSTDEAD)
			notify = in_rtchange;
	}
	errno = inetctlerrmap[cmd];
	s = splnet();
	for (inp = head->lh_first; inp != NULL;) {
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == 0 ||
		    (lport && inp->inp_lport != lport) ||
		    (laddr.s_addr && inp->inp_laddr.s_addr != laddr.s_addr) ||
		    (fport && inp->inp_fport != fport)) {
			inp = inp->inp_list.le_next;
			continue;
		}
		oinp = inp;
		inp = inp->inp_list.le_next;
		if (notify)
			(*notify)(oinp, errno);
	}
	splx(s);
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(inp)
	struct inpcb *inp;
{
	register struct rtentry *rt;
	struct rt_addrinfo info;

	if ((rt = inp->inp_route.ro_rt)) {
		inp->inp_route.ro_rt = 0;
		bzero((caddr_t)&info, sizeof(info));
		info.rti_info[RTAX_DST] =
			(struct sockaddr *)&inp->inp_route.ro_dst;
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
		if (rt->rt_flags & RTF_DYNAMIC)
			(void) rtrequest(RTM_DELETE, rt_key(rt),
				rt->rt_gateway, rt_mask(rt), rt->rt_flags,
				(struct rtentry **)0);
		else
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 */
			rtfree(rt);
	}
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
static void
in_rtchange(inp, errno)
	register struct inpcb *inp;
	int errno;
{
	if (inp->inp_route.ro_rt) {
		rtfree(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
}

struct inpcb *
in_pcblookup(pcbinfo, faddr, fport_arg, laddr, lport_arg, wild_okay)
	struct inpcbinfo *pcbinfo;
	struct in_addr faddr, laddr;
	u_int fport_arg, lport_arg;
	int wild_okay;
{
	register struct inpcb *inp, *match = NULL;
	int matchwild = 3, wildcard;
	u_short fport = fport_arg, lport = lport_arg;
	int s;

	s = splnet();

	for (inp = pcbinfo->listhead->lh_first; inp != NULL; inp = inp->inp_list.le_next) {
		if (inp->inp_lport != lport)
			continue;
		wildcard = 0;
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			if (faddr.s_addr == INADDR_ANY)
				wildcard++;
			else if (inp->inp_faddr.s_addr != faddr.s_addr ||
			    inp->inp_fport != fport)
				continue;
		} else {
			if (faddr.s_addr != INADDR_ANY)
				wildcard++;
		}
		if (inp->inp_laddr.s_addr != INADDR_ANY) {
			if (laddr.s_addr == INADDR_ANY)
				wildcard++;
			else if (inp->inp_laddr.s_addr != laddr.s_addr)
				continue;
		} else {
			if (laddr.s_addr != INADDR_ANY)
				wildcard++;
		}
		if (wildcard && wild_okay == 0)
			continue;
		if (wildcard < matchwild) {
			match = inp;
			matchwild = wildcard;
			if (matchwild == 0) {
				break;
			}
		}
	}
	splx(s);
	return (match);
}

/*
 * Lookup PCB in hash list.
 */
struct inpcb *
in_pcblookuphash(pcbinfo, faddr, fport_arg, laddr, lport_arg, wildcard)
	struct inpcbinfo *pcbinfo;
	struct in_addr faddr, laddr;
	u_int fport_arg, lport_arg;
	int wildcard;
{
	struct inpcbhead *head;
	register struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;
	int s;

	s = splnet();
	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->hashbase[INP_PCBHASH(faddr.s_addr, lport, fport, pcbinfo->hashmask)];
	for (inp = head->lh_first; inp != NULL; inp = inp->inp_hash.le_next) {
		if (inp->inp_faddr.s_addr == faddr.s_addr &&
		    inp->inp_laddr.s_addr == laddr.s_addr &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport)
			goto found;
	}
	if (wildcard) {
		struct inpcb *local_wild = NULL;

		head = &pcbinfo->hashbase[INP_PCBHASH(INADDR_ANY, lport, 0, pcbinfo->hashmask)];
		for (inp = head->lh_first; inp != NULL; inp = inp->inp_hash.le_next) {
			if (inp->inp_faddr.s_addr == INADDR_ANY &&
			    inp->inp_fport == 0 && inp->inp_lport == lport) {
				if (inp->inp_laddr.s_addr == laddr.s_addr)
					goto found;
				else if (inp->inp_laddr.s_addr == INADDR_ANY)
					local_wild = inp;
			}
		}
		if (local_wild != NULL) {
			inp = local_wild;
			goto found;
		}
	}
	splx(s);
	return (NULL);

found:
	/*
	 * Move PCB to head of this hash chain so that it can be
	 * found more quickly in the future.
	 * XXX - this is a pessimization on machines with few
	 * concurrent connections.
	 */
	if (inp != head->lh_first) {
		LIST_REMOVE(inp, inp_hash);
		LIST_INSERT_HEAD(head, inp, inp_hash);
	}
	splx(s);
	return (inp);
}

/*
 * Insert PCB into hash chain. Must be called at splnet.
 */
static void
in_pcbinshash(inp)
	struct inpcb *inp;
{
	struct inpcbhead *head;

	head = &inp->inp_pcbinfo->hashbase[INP_PCBHASH(inp->inp_faddr.s_addr,
		 inp->inp_lport, inp->inp_fport, inp->inp_pcbinfo->hashmask)];

	LIST_INSERT_HEAD(head, inp, inp_hash);
}

void
in_pcbrehash(inp)
	struct inpcb *inp;
{
	struct inpcbhead *head;
	int s;

	s = splnet();
	LIST_REMOVE(inp, inp_hash);

	head = &inp->inp_pcbinfo->hashbase[INP_PCBHASH(inp->inp_faddr.s_addr,
		inp->inp_lport, inp->inp_fport, inp->inp_pcbinfo->hashmask)];

	LIST_INSERT_HEAD(head, inp, inp_hash);
	splx(s);
}
