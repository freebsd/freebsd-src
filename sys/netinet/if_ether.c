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
 * $FreeBSD$
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#include "opt_inet.h"
#include "opt_bdg.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/if_llc.h>
#ifdef BRIDGE
#include <net/ethernet.h>
#include <net/bridge.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

#include <net/if_arc.h>
#include <net/iso88025.h>

#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)

SYSCTL_DECL(_net_link_ether);
SYSCTL_NODE(_net_link_ether, PF_INET, inet, CTLFLAG_RW, 0, "");

/* timer values */
static int arpt_prune = (5*60*1); /* walk list every 5 minutes */
static int arpt_keep = (20*60); /* once resolved, good for 20 more minutes */
static int arpt_down = 20;	/* once declared down, don't send for 20 sec */

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, prune_intvl, CTLFLAG_RW,
	   &arpt_prune, 0, "");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, max_age, CTLFLAG_RW, 
	   &arpt_keep, 0, "");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, host_down_time, CTLFLAG_RW,
	   &arpt_down, 0, "");

#define	rt_expire rt_rmx.rmx_expire

struct llinfo_arp {
	LIST_ENTRY(llinfo_arp) la_le;
	struct	rtentry *la_rt;
	struct	mbuf *la_hold;	/* last packet until resolved/timeout */
	u_short	la_preempt;	/* countdown for pre-expiry arps */
	u_short	la_asked;	/* #times we QUERIED following expiration */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};

static	LIST_HEAD(, llinfo_arp) llinfo_arp;

static struct	ifqueue arpintrq;
static int	arp_inuse, arp_allocated, arpinit_done;

static int	arp_maxtries = 5;
static int	useloopback = 1; /* use loopback interface for local traffic */
static int	arp_proxyall = 0;

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, maxtries, CTLFLAG_RW,
	   &arp_maxtries, 0, "");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, useloopback, CTLFLAG_RW,
	   &useloopback, 0, "");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, proxyall, CTLFLAG_RW,
	   &arp_proxyall, 0, "");

static void	arp_init(void);
static void	arp_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
static void	arprequest(struct ifnet *,
			struct in_addr *, struct in_addr *, u_char *);
static void	arpintr(struct mbuf *);
static void	arptfree(struct llinfo_arp *);
static void	arptimer(void *);
static struct llinfo_arp
		*arplookup(u_long, int, int);
#ifdef INET
static void	in_arpinput(struct mbuf *);
#endif

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
/* ARGSUSED */
static void
arptimer(ignored_arg)
	void *ignored_arg;
{
	struct llinfo_arp *la, *ola;
	int s = splnet();

	RADIX_NODE_HEAD_LOCK(rt_tables[AF_INET]);
	la = LIST_FIRST(&llinfo_arp);
	while (la != NULL) {
		struct rtentry *rt = la->la_rt;
		ola = la;
		la = LIST_NEXT(la, la_le);
		if (rt->rt_expire && rt->rt_expire <= time_second)
			arptfree(ola);		/* timer has expired, clear */
	}
	RADIX_NODE_HEAD_UNLOCK(rt_tables[AF_INET]);
	splx(s);
	timeout(arptimer, NULL, arpt_prune * hz);
}

/*
 * Parallel to llc_rtrequest.
 */
static void
arp_rtrequest(req, rt, info)
	int req;
	register struct rtentry *rt;
	struct rt_addrinfo *info;
{
	register struct sockaddr *gate = rt->rt_gateway;
	register struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};

	if (!arpinit_done) {
		arpinit_done = 1;
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
			rt->rt_expire = time_second;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			arprequest(rt->rt_ifp,
			    &SIN(rt_key(rt))->sin_addr,
			    &SIN(rt_key(rt))->sin_addr,
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
		RADIX_NODE_HEAD_LOCK_ASSERT(rt_tables[AF_INET]);
		LIST_INSERT_HEAD(&llinfo_arp, la, la_le);

#ifdef INET
		/*
		 * This keeps the multicast addresses from showing up
		 * in `arp -a' listings as unresolved.  It's not actually
		 * functional.  Then the same for broadcast.
		 */
		if (IN_MULTICAST(ntohl(SIN(rt_key(rt))->sin_addr.s_addr)) &&
		    rt->rt_ifp->if_type != IFT_ARCNET) {
			ETHER_MAP_IP_MULTICAST(&SIN(rt_key(rt))->sin_addr,
					       LLADDR(SDL(gate)));
			SDL(gate)->sdl_alen = 6;
			rt->rt_expire = 0;
		}
		if (in_broadcast(SIN(rt_key(rt))->sin_addr, rt->rt_ifp)) {
			memcpy(LLADDR(SDL(gate)), rt->rt_ifp->if_broadcastaddr,
			       rt->rt_ifp->if_addrlen);
			SDL(gate)->sdl_alen = rt->rt_ifp->if_addrlen;
			rt->rt_expire = 0;
		}
#endif

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
			Bcopy(IF_LLADDR(rt->rt_ifp), LLADDR(SDL(gate)),
			      SDL(gate)->sdl_alen = rt->rt_ifp->if_addrlen);
			if (useloopback)
				rt->rt_ifp = loif;

		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		arp_inuse--;
		RADIX_NODE_HEAD_LOCK_ASSERT(rt_tables[AF_INET]);
		LIST_REMOVE(la, la_le);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold)
			m_freem(la->la_hold);
		Free((caddr_t)la);
	}
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
static void
arprequest(ifp, sip, tip, enaddr)
	register struct ifnet *ifp;
	register struct in_addr *sip, *tip;
	register u_char *enaddr;
{
	register struct mbuf *m;
	register struct ether_header *eh;
	register struct arc_header *arh;
	register struct arphdr *ah;
	struct sockaddr sa;
	static u_char	llcx[] = { 0x82, 0x40, LLC_SNAP_LSAP, LLC_SNAP_LSAP,
				   LLC_UI, 0x00, 0x00, 0x00, 0x08, 0x06 };
	u_short ar_hrd;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
#ifdef MAC
	mac_create_mbuf_linklayer(ifp, m);
#endif
	switch (ifp->if_type) {
	case IFT_ARCNET:
		ar_hrd = htons(ARPHRD_ARCNET);

		m->m_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
		m->m_pkthdr.len = m->m_len;
		MH_ALIGN(m, m->m_len);

		arh = (struct arc_header *)sa.sa_data;
		arh->arc_dhost = *ifp->if_broadcastaddr;
		arh->arc_type = ARCTYPE_ARP;

		ah = mtod(m, struct arphdr *);
		break;

	case IFT_ISO88025:
		ar_hrd = htons(ARPHRD_IEEE802);

		m->m_len = sizeof(llcx) +
		    arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
		m->m_pkthdr.len = m->m_len;
		MH_ALIGN(m, m->m_len);

		(void)memcpy(mtod(m, caddr_t), llcx, sizeof(llcx));
		(void)memcpy(sa.sa_data, ifp->if_broadcastaddr, 6);
		(void)memcpy(sa.sa_data + 6, enaddr, 6);
		sa.sa_data[6] |= TR_RII;
		sa.sa_data[12] = TR_AC;
		sa.sa_data[13] = TR_LLC_FRAME;

		ah = (struct arphdr *)(mtod(m, char *) + sizeof(llcx));
		break;
	case IFT_FDDI:
	case IFT_ETHER:
		/*
		 * This may not be correct for types not explicitly
		 * listed, but this is our best guess
		 */
	default:
		ar_hrd = htons(ARPHRD_ETHER);

		m->m_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
		m->m_pkthdr.len = m->m_len;
		MH_ALIGN(m, m->m_len);

		eh = (struct ether_header *)sa.sa_data;
		/* if_output will not swap */
		eh->ether_type = htons(ETHERTYPE_ARP);
		(void)memcpy(eh->ether_dhost, ifp->if_broadcastaddr,
		    sizeof(eh->ether_dhost));

		ah = mtod(m, struct arphdr *);
		break;
	}

	ah->ar_hrd = ar_hrd;
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ifp->if_addrlen;		/* hardware address length */
	ah->ar_pln = sizeof(struct in_addr);	/* protocol address length */
	ah->ar_op = htons(ARPOP_REQUEST);
	(void)memcpy(ar_sha(ah), enaddr, ah->ar_hln);
	memset(ar_tha(ah), 0, ah->ar_hln);
	(void)memcpy(ar_spa(ah), sip, ah->ar_pln);
	(void)memcpy(ar_tpa(ah), tip, ah->ar_pln);

	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
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
arpresolve(ifp, rt, m, dst, desten, rt0)
	register struct ifnet *ifp;
	register struct rtentry *rt;
	struct mbuf *m;
	register struct sockaddr *dst;
	register u_char *desten;
	struct rtentry *rt0;
{
	struct llinfo_arp *la = 0;
	struct sockaddr_dl *sdl;

	if (m->m_flags & M_BCAST) {	/* broadcast */
		(void)memcpy(desten, ifp->if_broadcastaddr, ifp->if_addrlen);
		return (1);
	}
	if (m->m_flags & M_MCAST && ifp->if_type != IFT_ARCNET) {/* multicast */
		ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
		return(1);
	}
	if (rt)
		la = (struct llinfo_arp *)rt->rt_llinfo;
	if (la == 0) {
		la = arplookup(SIN(dst)->sin_addr.s_addr, 1, 0);
		if (la)
			rt = la->la_rt;
	}
	if (la == 0 || rt == 0) {
		log(LOG_DEBUG, "arpresolve: can't allocate llinfo for %s%s%s\n",
			inet_ntoa(SIN(dst)->sin_addr), la ? "la" : "",
				rt ? "rt" : "");
		m_freem(m);
		return (0);
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time_second) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		/*
		 * If entry has an expiry time and it is approaching,
		 * see if we need to send an ARP request within this
		 * arpt_down interval.
		 */
		if ((rt->rt_expire != 0) &&
		    (time_second + la->la_preempt > rt->rt_expire)) {
			arprequest(ifp,
				   &SIN(rt->rt_ifa->ifa_addr)->sin_addr,
				   &SIN(dst)->sin_addr,
				   IF_LLADDR(ifp));
			la->la_preempt--;
		} 

		bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
		return 1;
	}
	/*
	 * If ARP is disabled on this interface, stop.
	 * XXX
	 * Probably should not allocate empty llinfo struct if we are
	 * not going to be sending out an arp request.
	 */
	if (ifp->if_flags & IFF_NOARP) {
		m_freem(m);
		return (0);
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
		if (la->la_asked == 0 || rt->rt_expire != time_second) {
			rt->rt_expire = time_second;
			if (la->la_asked++ < arp_maxtries) {
				arprequest(ifp,
					   &SIN(rt->rt_ifa->ifa_addr)->sin_addr,
					   &SIN(dst)->sin_addr,
					   IF_LLADDR(ifp));
			} else {
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
				la->la_preempt = arp_maxtries;
			}

		}
	}
	return (0);
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
static void
arpintr(struct mbuf *m)
{
	struct arphdr *ar;

	if (!arpinit_done) {
		arpinit_done = 1;
		timeout(arptimer, (caddr_t)0, hz);
	}
	if (m->m_len < sizeof(struct arphdr) &&
	    ((m = m_pullup(m, sizeof(struct arphdr))) == NULL)) {
		log(LOG_ERR, "arp: runt packet -- m_pullup failed\n");
		return;
	}
	ar = mtod(m, struct arphdr *);

	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER &&
	    ntohs(ar->ar_hrd) != ARPHRD_IEEE802 &&
	    ntohs(ar->ar_hrd) != ARPHRD_ARCNET) {
		log(LOG_ERR, "arp: unknown hardware address format (0x%2D)\n",
		    (unsigned char *)&ar->ar_hrd, "");
		m_freem(m);
		return;
	}

	if (m->m_pkthdr.len < arphdr_len(ar) &&
	    (m = m_pullup(m, arphdr_len(ar))) == NULL) {
		log(LOG_ERR, "arp: runt packet\n");
		m_freem(m);
		return;
	}

	switch (ntohs(ar->ar_pro)) {
#ifdef INET
	case ETHERTYPE_IP:
		in_arpinput(m);
		return;
#endif
	}
	m_freem(m);
}

#ifdef INET
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
static int log_arp_wrong_iface = 1;
static int log_arp_movements = 1;

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_wrong_iface, CTLFLAG_RW,
	&log_arp_wrong_iface, 0,
	"log arp packets arriving on the wrong interface");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_movements, CTLFLAG_RW,
        &log_arp_movements, 0,
        "log arp replies from MACs different than the one in the cache");


static void
in_arpinput(m)
	struct mbuf *m;
{
	register struct arphdr *ah;
	register struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ether_header *eh;
	struct arc_header *arh;
	struct iso88025_header *th = (struct iso88025_header *)0;
	struct iso88025_sockaddr_dl_data *trld;
	register struct llinfo_arp *la = 0;
	register struct rtentry *rt;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op, rif_len;
	int req_len;

	req_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
	if (m->m_len < req_len && (m = m_pullup(m, req_len)) == NULL) {
		log(LOG_ERR, "in_arp: runt packet -- m_pullup failed\n");
		return;
	}

	ah = mtod(m, struct arphdr *);
	op = ntohs(ah->ar_op);
	(void)memcpy(&isaddr, ar_spa(ah), sizeof (isaddr));
	(void)memcpy(&itaddr, ar_tpa(ah), sizeof (itaddr));
#ifdef BRIDGE
#define BRIDGE_TEST (do_bridge)
#else
#define BRIDGE_TEST (0) /* cc will optimise the test away */
#endif
	/*
	 * For a bridge, we want to check the address irrespective
	 * of the receive interface. (This will change slightly
	 * when we have clusters of interfaces).
	 */
	LIST_FOREACH(ia, INADDR_HASH(itaddr.s_addr), ia_hash)
		if ((BRIDGE_TEST || (ia->ia_ifp == ifp)) &&
		    itaddr.s_addr == ia->ia_addr.sin_addr.s_addr)
			goto match;
	LIST_FOREACH(ia, INADDR_HASH(isaddr.s_addr), ia_hash)
		if ((BRIDGE_TEST || (ia->ia_ifp == ifp)) &&
		    isaddr.s_addr == ia->ia_addr.sin_addr.s_addr)
			goto match;
	/*
	 * No match, use the first inet address on the receive interface
	 * as a dummy address for the rest of the function.
	 */
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
			ia = ifatoia(ifa);
			goto match;
		}
	/*
	 * If bridging, fall back to using any inet address.
	 */
	if (!BRIDGE_TEST ||
	    (ia = TAILQ_FIRST(&in_ifaddrhead)) == NULL) {
		m_freem(m);
		return;
	}
match:
	myaddr = ia->ia_addr.sin_addr;
	if (!bcmp(ar_sha(ah), IF_LLADDR(ifp), ifp->if_addrlen)) {
		m_freem(m);	/* it's from me, ignore it. */
		return;
	}
	if (!bcmp(ar_sha(ah), ifp->if_broadcastaddr, ifp->if_addrlen)) {
		log(LOG_ERR,
		    "arp: link address is broadcast for IP address %s!\n",
		    inet_ntoa(isaddr));
		m_freem(m);
		return;
	}
	if (isaddr.s_addr == myaddr.s_addr) {
		log(LOG_ERR,
		   "arp: %*D is using my IP address %s!\n",
		   ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
		   inet_ntoa(isaddr));
		itaddr = myaddr;
		goto reply;
	}
	la = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
		/* the following is not an error when doing bridging */
		if (!BRIDGE_TEST && rt->rt_ifp != ifp) {
			if (log_arp_wrong_iface)
				log(LOG_ERR, "arp: %s is on %s%d but got reply from %*D on %s%d\n",
				    inet_ntoa(isaddr),
				    rt->rt_ifp->if_name, rt->rt_ifp->if_unit,
				    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				    ifp->if_name, ifp->if_unit);
			goto reply;
		}
		if (sdl->sdl_alen &&
		    bcmp(ar_sha(ah), LLADDR(sdl), sdl->sdl_alen)) {
			if (rt->rt_expire) {
			    if (log_arp_movements)
			        log(LOG_INFO, "arp: %s moved from %*D to %*D on %s%d\n",
				    inet_ntoa(isaddr),
				    ifp->if_addrlen, (u_char *)LLADDR(sdl), ":",
				    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				    ifp->if_name, ifp->if_unit);
			} else {
			    log(LOG_ERR,
				"arp: %*D attempts to modify permanent entry for %s on %s%d\n",
				ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				inet_ntoa(isaddr), ifp->if_name, ifp->if_unit);
			    goto reply;
			}
		}
		/*
		 * sanity check for the address length.
		 * XXX this does not work for protocols with variable address
		 * length. -is
		 */
		if (sdl->sdl_alen &&
		    sdl->sdl_alen != ah->ar_hln) {
			log(LOG_WARNING,
			    "arp from %*D: new addr len %d, was %d",
			    ifp->if_addrlen, (u_char *) ar_sha(ah), ":",
			    ah->ar_hln, sdl->sdl_alen);
		}
		if (ifp->if_addrlen != ah->ar_hln) {
			log(LOG_WARNING,
			    "arp from %*D: addr len: new %d, i/f %d (ignored)",
			    ifp->if_addrlen, (u_char *) ar_sha(ah), ":",
			    ah->ar_hln, ifp->if_addrlen);
			goto reply;
		}
		(void)memcpy(LLADDR(sdl), ar_sha(ah),
		    sdl->sdl_alen = ah->ar_hln);
		/*
		 * If we receive an arp from a token-ring station over
		 * a token-ring nic then try to save the source
		 * routing info.
		 */
		if (ifp->if_type == IFT_ISO88025) {
			th = (struct iso88025_header *)m->m_pkthdr.header;
			trld = SDL_ISO88025(sdl);
			rif_len = TR_RCF_RIFLEN(th->rcf);
			if ((th->iso88025_shost[0] & TR_RII) &&
			    (rif_len > 2)) {
				trld->trld_rcf = th->rcf;
				trld->trld_rcf ^= htons(TR_RCF_DIR);
				memcpy(trld->trld_route, th->rd, rif_len - 2);
				trld->trld_rcf &= ~htons(TR_RCF_BCST_MASK);
				/*
				 * Set up source routing information for
				 * reply packet (XXX)
				 */
				m->m_data -= rif_len;
				m->m_len  += rif_len;
				m->m_pkthdr.len += rif_len;
			} else {
				th->iso88025_shost[0] &= ~TR_RII;
				trld->trld_rcf = 0;
			}
			m->m_data -= 8;
			m->m_len  += 8;
			m->m_pkthdr.len += 8;
			th->rcf = trld->trld_rcf;
		}
		if (rt->rt_expire)
			rt->rt_expire = time_second + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		la->la_preempt = arp_maxtries;
		if (la->la_hold) {
			(*ifp->if_output)(ifp, la->la_hold,
				rt_key(rt), rt);
			la->la_hold = 0;
		}
	}
reply:
	if (op != ARPOP_REQUEST) {
		m_freem(m);
		return;
	}
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
		(void)memcpy(ar_sha(ah), IF_LLADDR(ifp), ah->ar_hln);
	} else {
		la = arplookup(itaddr.s_addr, 0, SIN_PROXY);
		if (la == NULL) {
			struct sockaddr_in sin;

			if (!arp_proxyall) {
				m_freem(m);
				return;
			}

			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = itaddr;

			rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
			if (!rt) {
				m_freem(m);
				return;
			}
			/*
			 * Don't send proxies for nodes on the same interface
			 * as this one came out of, or we'll get into a fight
			 * over who claims what Ether address.
			 */
			if (rt->rt_ifp == ifp) {
				rtfree(rt);
				m_freem(m);
				return;
			}
			(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
			(void)memcpy(ar_sha(ah), IF_LLADDR(ifp), ah->ar_hln);
			rtfree(rt);

			/*
			 * Also check that the node which sent the ARP packet
			 * is on the the interface we expect it to be on. This
			 * avoids ARP chaos if an interface is connected to the
			 * wrong network.
			 */
			sin.sin_addr = isaddr;

			rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
			if (!rt) {
				m_freem(m);
				return;
			}
			if (rt->rt_ifp != ifp) {
				log(LOG_INFO, "arp_proxy: ignoring request"
				    " from %s via %s%d, expecting %s%d\n",
				    inet_ntoa(isaddr), ifp->if_name,
				    ifp->if_unit, rt->rt_ifp->if_name,
				    rt->rt_ifp->if_unit);
				rtfree(rt);
				m_freem(m);
				return;
			}
			rtfree(rt);

#ifdef DEBUG_PROXY
			printf("arp: proxying for %s\n",
			       inet_ntoa(itaddr));
#endif
		} else {
			rt = la->la_rt;
			(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
			sdl = SDL(rt->rt_gateway);
			(void)memcpy(ar_sha(ah), LLADDR(sdl), ah->ar_hln);
		}
	}

	(void)memcpy(ar_tpa(ah), ar_spa(ah), ah->ar_pln);
	(void)memcpy(ar_spa(ah), &itaddr, ah->ar_pln);
	ah->ar_op = htons(ARPOP_REPLY);
	ah->ar_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	switch (ifp->if_type) {
	case IFT_ARCNET:
		arh = (struct arc_header *)sa.sa_data;
		arh->arc_dhost = *ar_tha(ah);
		arh->arc_type = ARCTYPE_ARP;
		break;

	case IFT_ISO88025:
		/* Re-arrange the source/dest address */
		memcpy(th->iso88025_dhost, th->iso88025_shost,
		    sizeof(th->iso88025_dhost));
		memcpy(th->iso88025_shost, IF_LLADDR(ifp),
		    sizeof(th->iso88025_shost));
		/* Set the source routing bit if neccesary */
		if (th->iso88025_dhost[0] & TR_RII) {
			th->iso88025_dhost[0] &= ~TR_RII;
			if (TR_RCF_RIFLEN(th->rcf) > 2)
				th->iso88025_shost[0] |= TR_RII;
		}
		/* Copy the addresses, ac and fc into sa_data */
		memcpy(sa.sa_data, th->iso88025_dhost,
		    sizeof(th->iso88025_dhost) * 2);
		sa.sa_data[(sizeof(th->iso88025_dhost) * 2)] = TR_AC;
		sa.sa_data[(sizeof(th->iso88025_dhost) * 2) + 1] = TR_LLC_FRAME;
		break;
	case IFT_ETHER:
	case IFT_FDDI:
	/*
	 * May not be correct for types not explictly
	 * listed, but it is our best guess.
	 */
	default:
		eh = (struct ether_header *)sa.sa_data;
		(void)memcpy(eh->ether_dhost, ar_tha(ah),
		    sizeof(eh->ether_dhost));
		eh->ether_type = htons(ETHERTYPE_ARP);
		break;
	}
	sa.sa_family = AF_UNSPEC;
	sa.sa_len = sizeof(sa);
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
	return;
}
#endif

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
		la->la_preempt = la->la_asked = 0;
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

	if (rt->rt_flags & RTF_GATEWAY)
		why = "host is not on local network";
	else if ((rt->rt_flags & RTF_LLINFO) == 0)
		why = "could not allocate llinfo";
	else if (rt->rt_gateway->sa_family != AF_LINK)
		why = "gateway route is not ours";

	if (why) {
		if (create)
			log(LOG_DEBUG, "arplookup %s failed: %s\n",
			    inet_ntoa(sin.sin_addr), why);

		/* If there are no references to this route, purge it */
		if (rt->rt_refcnt <= 0 &&
		    (rt->rt_flags & RTF_WASCLONED) == RTF_WASCLONED) {
			rtrequest(RTM_DELETE,
					(struct sockaddr *)rt_key(rt),
					rt->rt_gateway, rt_mask(rt),
					rt->rt_flags, 0);
		}
		return (0);
	}
	return ((struct llinfo_arp *)rt->rt_llinfo);
}

void
arp_ifinit(ifp, ifa)
	struct ifnet *ifp;
	struct ifaddr *ifa;
{
	if (ntohl(IA_SIN(ifa)->sin_addr.s_addr) != INADDR_ANY)
		arprequest(ifp, &IA_SIN(ifa)->sin_addr,
				&IA_SIN(ifa)->sin_addr, IF_LLADDR(ifp));
	ifa->ifa_rtrequest = arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}

static void
arp_init(void)
{

	arpintrq.ifq_maxlen = 50;
	mtx_init(&arpintrq.ifq_mtx, "arp_inq", NULL, MTX_DEF);
	LIST_INIT(&llinfo_arp);
	netisr_register(NETISR_ARP, arpintr, &arpintrq);
}

SYSINIT(arp, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, arp_init, 0);
