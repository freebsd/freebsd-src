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
 *	@(#)if_ether.c	8.1 (Berkeley) 6/10/93
 * $Id: if_ether.c,v 1.19 1995/10/05 20:08:39 wollman Exp $
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)
#define SRP(s) ((struct sockaddr_inarp *)s)

/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL


/* timer values */
int	arpt_prune = (5*60*1);	/* walk list every 5 minutes */
int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	arpt_down = 20;		/* once declared down, don't send for 20 secs */
#define	rt_expire rt_rmx.rmx_expire

struct llinfo_arp {
	LIST_ENTRY(llinfo_arp) la_le;
	struct	rtentry *la_rt;
	struct	mbuf *la_hold;		/* last packet until resolved/timeout */
	long	la_asked;		/* last time we QUERIED for this addr */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};
static	LIST_HEAD(, llinfo_arp) llinfo_arp;

static	void arprequest __P((struct arpcom *, u_long *, u_long *, u_char *));
static	void arptfree __P((struct llinfo_arp *));
static	void arptimer __P((void *));
static	struct llinfo_arp *arplookup __P((u_long, int, int));
static	void in_arpinput __P((struct mbuf *));

struct	ifqueue arpintrq = {0, 0, 0, 50};
int	arp_inuse, arp_allocated, arp_intimer;
int	arp_maxtries = 5;
int	useloopback = 1;	/* use loopback interface for local traffic */
int	arpinit_done = 0;

#ifdef	ARP_PROXYALL
int	arp_proxyall = 1;
#endif

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
/* ARGSUSED */
static void
arptimer(ignored_arg)
	void *ignored_arg;
{
	int s = splnet();
	register struct llinfo_arp *la = llinfo_arp.lh_first;
	struct llinfo_arp *ola;

	timeout(arptimer, (caddr_t)0, arpt_prune * hz);
	while ((ola = la) != 0) {
		register struct rtentry *rt = la->la_rt;
		la = la->la_le.le_next;
		if (rt->rt_expire && rt->rt_expire <= time.tv_sec)
			arptfree(ola); /* timer has expired, clear */
	}
	splx(s);
}

/*
 * Parallel to llc_rtrequest.
 */
static void
arp_rtrequest(req, rt, sa)
	int req;
	register struct rtentry *rt;
	struct sockaddr *sa;
{
	register struct sockaddr *gate = rt->rt_gateway;
	register struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};

	if (!arpinit_done) {
		arpinit_done = 1;
		LIST_INIT(&llinfo_arp);
		timeout(arptimer, (caddr_t)0, hz);
	}
	if (rt->rt_flags & RTF_GATEWAY)
		return;
	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 &&
		    SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1: This route should come from a route to iface.
			 */
			rt_setgate(rt, rt_key(rt),
					(struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			rt->rt_expire = time.tv_sec;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			arprequest((struct arpcom *)rt->rt_ifp,
			    &SIN(rt_key(rt))->sin_addr.s_addr,
			    &SIN(rt_key(rt))->sin_addr.s_addr,
			    (u_char *)LLADDR(SDL(gate)));
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(null_sdl)) {
			log(LOG_DEBUG, "arp_rtrequest: bad gateway value\n");
			break;
		}
		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;
		if (la != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) {
			log(LOG_DEBUG, "arp_rtrequest: malloc failed\n");
			break;
		}
		arp_inuse++, arp_allocated++;
		Bzero(la, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		LIST_INSERT_HEAD(&llinfo_arp, la, la_le);
		if (SIN(rt_key(rt))->sin_addr.s_addr ==
		    (IA_SIN(rt->rt_ifa))->sin_addr.s_addr) {
		    /*
		     * This test used to be
		     *	if (loif.if_flags & IFF_UP)
		     * It allowed local traffic to be forced
		     * through the hardware by configuring the loopback down.
		     * However, it causes problems during network configuration
		     * for boards that can't receive packets they send.
		     * It is now necessary to clear "useloopback" and remove
		     * the route to force traffic out to the hardware.
		     */
			rt->rt_expire = 0;
			Bcopy(((struct arpcom *)rt->rt_ifp)->ac_enaddr,
				LLADDR(SDL(gate)), SDL(gate)->sdl_alen = 6);
			if (useloopback)
				rt->rt_ifp = loif;

		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		arp_inuse--;
		LIST_REMOVE(la, la_le);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold)
			m_freem(la->la_hold);
		Free((caddr_t)la);
	}
}

/*
 * Broadcast an ARP packet, asking who has addr on interface ac.
 */
void
arpwhohas(ac, addr)
	register struct arpcom *ac;
	register struct in_addr *addr;
{
	arprequest(ac, &ac->ac_ipaddr.s_addr, &addr->s_addr, ac->ac_enaddr);
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
static void
arprequest(ac, sip, tip, enaddr)
	register struct arpcom *ac;
	register u_long *sip, *tip;
	register u_char *enaddr;
{
	register struct mbuf *m;
	register struct ether_header *eh;
	register struct ether_arp *ea;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	bzero((caddr_t)ea, sizeof (*ea));
	(void)memcpy(eh->ether_dhost, etherbroadcastaddr, sizeof(eh->ether_dhost));
	eh->ether_type = ETHERTYPE_ARP;		/* if_output will swap */
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REQUEST);
	(void)memcpy(ea->arp_sha, enaddr, sizeof(ea->arp_sha));
	(void)memcpy(ea->arp_spa, sip, sizeof(ea->arp_spa));
	(void)memcpy(ea->arp_tpa, tip, sizeof(ea->arp_tpa));
	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, (struct rtentry *)0);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
arpresolve(ac, rt, m, dst, desten, rt0)
	register struct arpcom *ac;
	register struct rtentry *rt;
	struct mbuf *m;
	register struct sockaddr *dst;
	register u_char *desten;
	struct rtentry *rt0;
{
	register struct llinfo_arp *la;
	struct sockaddr_dl *sdl;

	if (m->m_flags & M_BCAST) {	/* broadcast */
		(void)memcpy(desten, etherbroadcastaddr, sizeof(etherbroadcastaddr));
		return (1);
	}
	if (m->m_flags & M_MCAST) {	/* multicast */
		ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
		return(1);
	}
	if (rt)
		la = (struct llinfo_arp *)rt->rt_llinfo;
	else {
		la = arplookup(SIN(dst)->sin_addr.s_addr, 1, 0);
		if (la)
			rt = la->la_rt;
	}
	if (la == 0 || rt == 0) {
		log(LOG_DEBUG, "arpresolve: can't allocate llinfo\n");
		m_freem(m);
		return (0);
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time.tv_sec) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		(void)memcpy(desten, LLADDR(sdl), sdl->sdl_alen);
		return 1;
	}
	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	if (la->la_hold)
		m_freem(la->la_hold);
	la->la_hold = m;
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != time.tv_sec) {
			rt->rt_expire = time.tv_sec;
			if (la->la_asked++ < arp_maxtries)
				arpwhohas(ac, &(SIN(dst)->sin_addr));
			else {
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
			}

		}
	}
	return (0);
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpintr(void)
{
	register struct mbuf *m;
	register struct arphdr *ar;
	int s;

	while (arpintrq.ifq_head) {
		s = splimp();
		IF_DEQUEUE(&arpintrq, m);
		splx(s);
		if (m == 0 || (m->m_flags & M_PKTHDR) == 0)
			panic("arpintr");
		if (m->m_len >= sizeof(struct arphdr) &&
		    (ar = mtod(m, struct arphdr *)) &&
		    ntohs(ar->ar_hrd) == ARPHRD_ETHER &&
		    m->m_len >=
		      sizeof(struct arphdr) + 2 * ar->ar_hln + 2 * ar->ar_pln)

			    switch (ntohs(ar->ar_pro)) {

			    case ETHERTYPE_IP:
			    case ETHERTYPE_IPTRAILERS:
				    in_arpinput(m);
				    continue;
			    }
		m_freem(m);
	}
}

NETISR_SET(NETISR_ARP, arpintr);

/*
 * ARP for Internet protocols on 10 Mb/s Ethernet.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 * We no longer handle negotiations for use of trailer protocol:
 * Formerly, ARP replied for protocol type ETHERTYPE_TRAIL sent
 * along with IP replies if we wanted trailers sent to us,
 * and also sent them in response to IP replies.
 * This allowed either end to announce the desire to receive
 * trailer packets.
 * We no longer reply to requests for ETHERTYPE_TRAIL protocol either,
 * but formerly didn't normally send requests.
 */
static void
in_arpinput(m)
	struct mbuf *m;
{
	register struct ether_arp *ea;
	register struct arpcom *ac = (struct arpcom *)m->m_pkthdr.rcvif;
	struct ether_header *eh;
	register struct llinfo_arp *la = 0;
	register struct rtentry *rt;
	struct in_ifaddr *ia, *maybe_ia = 0;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op;

	ea = mtod(m, struct ether_arp *);
	op = ntohs(ea->arp_op);
	(void)memcpy(&isaddr, ea->arp_spa, sizeof (isaddr));
	(void)memcpy(&itaddr, ea->arp_tpa, sizeof (itaddr));
	for (ia = in_ifaddr; ia; ia = ia->ia_next)
		if (ia->ia_ifp == &ac->ac_if) {
			maybe_ia = ia;
			if ((itaddr.s_addr == ia->ia_addr.sin_addr.s_addr) ||
			     (isaddr.s_addr == ia->ia_addr.sin_addr.s_addr))
				break;
		}
	if (maybe_ia == 0)
		goto out;
	myaddr = ia ? ia->ia_addr.sin_addr : maybe_ia->ia_addr.sin_addr;
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)ac->ac_enaddr,
	    sizeof (ea->arp_sha)))
		goto out;	/* it's from me, ignore it. */
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)etherbroadcastaddr,
	    sizeof (ea->arp_sha))) {
		log(LOG_ERR,
		    "arp: ether address is broadcast for IP address %s!\n",
		    inet_ntoa(isaddr));
		goto out;
	}
	if (isaddr.s_addr == myaddr.s_addr) {
		log(LOG_ERR,
		   "duplicate IP address %s! sent from ethernet address: %s\n",
		   inet_ntoa(isaddr), ether_sprintf(ea->arp_sha));
		itaddr = myaddr;
		goto reply;
	}
	la = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
		if (sdl->sdl_alen &&
		    bcmp((caddr_t)ea->arp_sha, LLADDR(sdl), sdl->sdl_alen))
			log(LOG_INFO, "arp info overwritten for %s by %s\n",
			    inet_ntoa(isaddr), ether_sprintf(ea->arp_sha));
		(void)memcpy(LLADDR(sdl), ea->arp_sha, sizeof(ea->arp_sha));
		sdl->sdl_alen = sizeof(ea->arp_sha);
		if (rt->rt_expire)
			rt->rt_expire = time.tv_sec + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		if (la->la_hold) {
			(*ac->ac_if.if_output)(&ac->ac_if, la->la_hold,
				rt_key(rt), rt);
			la->la_hold = 0;
		}
	}
reply:
	if (op != ARPOP_REQUEST) {
	out:
		m_freem(m);
		return;
	}
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		(void)memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
		(void)memcpy(ea->arp_sha, ac->ac_enaddr, sizeof(ea->arp_sha));
	} else {
		la = arplookup(itaddr.s_addr, 0, SIN_PROXY);
		if (la == NULL) {
#ifdef ARP_PROXYALL
			struct sockaddr_in sin;

			if(!arp_proxyall) goto out;

			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = itaddr;

			rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
			if( !rt )
				goto out;
			/*
			 * Don't send proxies for nodes on the same interface
			 * as this one came out of, or we'll get into a fight
			 * over who claims what Ether address.
			 */
			if(rt->rt_ifp == &ac->ac_if) {
				rtfree(rt);
				goto out;
			}
			(void)memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
			(void)memcpy(ea->arp_sha, ac->ac_enaddr, sizeof(ea->arp_sha));
			rtfree(rt);
#ifdef DEBUG_PROXY
			printf("arp: proxying for %s\n",
			       inet_ntoa(itaddr));
#endif
#else
			goto out;
#endif
		} else {
			rt = la->la_rt;
			(void)memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
			sdl = SDL(rt->rt_gateway);
			(void)memcpy(ea->arp_sha, LLADDR(sdl), sizeof(ea->arp_sha));
		}
	}

	(void)memcpy(ea->arp_tpa, ea->arp_spa, sizeof(ea->arp_spa));
	(void)memcpy(ea->arp_spa, &itaddr, sizeof(ea->arp_spa));
	ea->arp_op = htons(ARPOP_REPLY);
	ea->arp_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	eh = (struct ether_header *)sa.sa_data;
	(void)memcpy(eh->ether_dhost, ea->arp_tha, sizeof(eh->ether_dhost));
	eh->ether_type = ETHERTYPE_ARP;
	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, (struct rtentry *)0);
	return;
}

/*
 * Free an arp entry.
 */
static void
arptfree(la)
	register struct llinfo_arp *la;
{
	register struct rtentry *rt = la->la_rt;
	register struct sockaddr_dl *sdl;
	if (rt == 0)
		panic("arptfree");
	if (rt->rt_refcnt > 0 && (sdl = SDL(rt->rt_gateway)) &&
	    sdl->sdl_family == AF_LINK) {
		sdl->sdl_alen = 0;
		la->la_asked = 0;
		rt->rt_flags &= ~RTF_REJECT;
		return;
	}
	rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0, rt_mask(rt),
			0, (struct rtentry **)0);
}
/*
 * Lookup or enter a new address in arptab.
 */
static struct llinfo_arp *
arplookup(addr, create, proxy)
	u_long addr;
	int create, proxy;
{
	register struct rtentry *rt;
	static struct sockaddr_inarp sin = {sizeof(sin), AF_INET };
	const char *why = 0;

	sin.sin_addr.s_addr = addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;
	rt = rtalloc1((struct sockaddr *)&sin, create, 0UL);
	if (rt == 0)
		return (0);
	rt->rt_refcnt--;

	if(rt->rt_flags & RTF_GATEWAY)
		why = "host is not on local network";
	else if((rt->rt_flags & RTF_LLINFO) == 0)
		why = "could not allocate llinfo";
	else if(rt->rt_gateway->sa_family != AF_LINK)
		why = "gateway route is not ours";

	if(why && create) {
		log(LOG_DEBUG, "arplookup %s failed: %s\n",
		    inet_ntoa(sin.sin_addr), why);
		return 0;
	} else if(why) {
		return 0;
	}
	return ((struct llinfo_arp *)rt->rt_llinfo);
}

int
arpioctl(cmd, data)
	int cmd;
	caddr_t data;
{
	return (EOPNOTSUPP);
}

void
arp_ifinit(ac, ifa)
	struct arpcom *ac;
	struct ifaddr *ifa;
{
	ac->ac_ipaddr = IA_SIN(ifa)->sin_addr;
	arpwhohas(ac, &ac->ac_ipaddr);
	ifa->ifa_rtrequest = arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}
