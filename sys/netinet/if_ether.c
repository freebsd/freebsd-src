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
 *	@(#)if_ether.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_mac.h"
#include "opt_carp.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
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
#include <net/ethernet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

#include <net/if_arc.h>
#include <net/iso88025.h>

#ifdef DEV_CARP
#include <netinet/ip_carp.h>
#endif

#include <security/mac/mac_framework.h>

#define SIN(s) ((struct sockaddr_in *)s)
#define SDL(s) ((struct sockaddr_dl *)s)

SYSCTL_DECL(_net_link_ether);
SYSCTL_NODE(_net_link_ether, PF_INET, inet, CTLFLAG_RW, 0, "");

/* timer values */
static int arpt_keep = (20*60); /* once resolved, good for 20 more minutes */

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, max_age, CTLFLAG_RW, 
	   &arpt_keep, 0, "ARP entry lifetime in seconds");

#define	rt_expire rt_rmx.rmx_expire

struct llinfo_arp {
	struct	callout la_timer;
	struct	rtentry *la_rt;
	struct	mbuf *la_hold;	/* last packet until resolved/timeout */
	u_short	la_preempt;	/* countdown for pre-expiry arps */
	u_short	la_asked;	/* # requests sent */
};

static struct	ifqueue arpintrq;
static int	arp_allocated;

static int	arp_maxtries = 5;
static int	useloopback = 1; /* use loopback interface for local traffic */
static int	arp_proxyall = 0;

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, maxtries, CTLFLAG_RW,
	   &arp_maxtries, 0, "ARP resolution attempts before returning error");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, useloopback, CTLFLAG_RW,
	   &useloopback, 0, "Use the loopback interface for local traffic");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, proxyall, CTLFLAG_RW,
	   &arp_proxyall, 0, "Enable proxy ARP for all suitable requests");

static void	arp_init(void);
static void	arp_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
static void	arprequest(struct ifnet *,
			struct in_addr *, struct in_addr *, u_char *);
static void	arpintr(struct mbuf *);
static void	arptimer(void *);
static struct rtentry
		*arplookup(u_long, int, int);
#ifdef INET
static void	in_arpinput(struct mbuf *);
#endif

/*
 * Timeout routine.
 */
static void
arptimer(void *arg)
{
	struct rtentry *rt = (struct rtentry *)arg;

	RT_LOCK_ASSERT(rt);
	/*
	 * The lock is needed to close a theoretical race
	 * between spontaneous expiry and intentional removal.
	 * We still got an extra reference on rtentry, so can
	 * safely pass pointers to its contents.
	 */
	RT_UNLOCK(rt);

	rtrequest(RTM_DELETE, rt_key(rt), NULL, rt_mask(rt), 0, NULL);
}

/*
 * Parallel to llc_rtrequest.
 */
static void
arp_rtrequest(int req, struct rtentry *rt, struct rt_addrinfo *info)
{
	struct sockaddr *gate;
	struct llinfo_arp *la;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
	struct in_ifaddr *ia;
	struct ifaddr *ifa;

	RT_LOCK_ASSERT(rt);

	if (rt->rt_flags & RTF_GATEWAY)
		return;
	gate = rt->rt_gateway;
	la = (struct llinfo_arp *)rt->rt_llinfo;
	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 &&
		    rt_mask(rt) != NULL &&
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
			rt->rt_expire = time_uptime;
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
			log(LOG_DEBUG, "%s: bad gateway %s%s\n", __func__,
			    inet_ntoa(SIN(rt_key(rt))->sin_addr),
			    (gate->sa_family != AF_LINK) ?
			    " (!AF_LINK)": "");
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
		R_Zalloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) {
			log(LOG_DEBUG, "%s: malloc failed\n", __func__);
			break;
		}
		arp_allocated++;
		/*
		 * We are storing a route entry outside of radix tree. So,
		 * it can be found and accessed by other means than radix
		 * lookup. The routing code assumes that any rtentry detached
		 * from radix can be destroyed safely. To prevent this, we
		 * add an additional reference.
		 */
		RT_ADDREF(rt);
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		callout_init_mtx(&la->la_timer, &rt->rt_mtx,
		    CALLOUT_RETURNUNLOCKED);

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

		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
			if (ia->ia_ifp == rt->rt_ifp &&
			    SIN(rt_key(rt))->sin_addr.s_addr ==
			    (IA_SIN(ia))->sin_addr.s_addr)
				break;
		}
		if (ia) {
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
			bcopy(IF_LLADDR(rt->rt_ifp), LLADDR(SDL(gate)),
			      SDL(gate)->sdl_alen = rt->rt_ifp->if_addrlen);
			if (useloopback) {
				rt->rt_ifp = loif;
				rt->rt_rmx.rmx_mtu = loif->if_mtu;
			}

		    /*
		     * make sure to set rt->rt_ifa to the interface
		     * address we are using, otherwise we will have trouble
		     * with source address selection.
		     */
			ifa = &ia->ia_ifa;
			if (ifa != rt->rt_ifa) {
				IFAFREE(rt->rt_ifa);
				IFAREF(ifa);
				rt->rt_ifa = ifa;
			}
		}
		break;

	case RTM_DELETE:
		if (la == NULL)	/* XXX: at least CARP does this. */
			break;
		callout_stop(&la->la_timer);
		rt->rt_llinfo = NULL;
		rt->rt_flags &= ~RTF_LLINFO;
		RT_REMREF(rt);
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
arprequest(struct ifnet *ifp, struct in_addr *sip, struct in_addr *tip,
    u_char *enaddr)
{
	struct mbuf *m;
	struct arphdr *ah;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ah) + 2*sizeof(struct in_addr) +
		2*ifp->if_data.ifi_addrlen;
	m->m_pkthdr.len = m->m_len;
	MH_ALIGN(m, m->m_len);
	ah = mtod(m, struct arphdr *);
	bzero((caddr_t)ah, m->m_len);
#ifdef MAC
	mac_netinet_arp_send(ifp, m);
#endif
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ifp->if_addrlen;		/* hardware address length */
	ah->ar_pln = sizeof(struct in_addr);	/* protocol address length */
	ah->ar_op = htons(ARPOP_REQUEST);
	bcopy((caddr_t)enaddr, (caddr_t)ar_sha(ah), ah->ar_hln);
	bcopy((caddr_t)sip, (caddr_t)ar_spa(ah), ah->ar_pln);
	bcopy((caddr_t)tip, (caddr_t)ar_tpa(ah), ah->ar_pln);
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	m->m_flags |= M_BCAST;
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);

	return;
}

/*
 * Resolve an IP address into an ethernet address.
 * On input:
 *    ifp is the interface we use
 *    dst is the next hop,
 *    rt0 is the route to the final destination (possibly useless)
 *    m is the mbuf
 *    desten is where we want the address.
 *
 * On success, desten is filled in and the function returns 0;
 * If the packet must be held pending resolution, we return EWOULDBLOCK
 * On other errors, we return the corresponding error code.
 */
int
arpresolve(struct ifnet *ifp, struct rtentry *rt0, struct mbuf *m,
    struct sockaddr *dst, u_char *desten)
{
	struct llinfo_arp *la = NULL;
	struct rtentry *rt = NULL;
	struct sockaddr_dl *sdl;
	int error;

	if (m->m_flags & M_BCAST) {	/* broadcast */
		(void)memcpy(desten, ifp->if_broadcastaddr, ifp->if_addrlen);
		return (0);
	}
	if (m->m_flags & M_MCAST && ifp->if_type != IFT_ARCNET) {/* multicast */
		ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
		return (0);
	}

	if (rt0 != NULL) {
		error = rt_check(&rt, &rt0, dst);
		if (error) {
			m_freem(m);
			return error;
		}
		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (la == NULL)
			RT_UNLOCK(rt);
	}
	if (la == NULL) {
		/*
		 * We enter this block in case if rt0 was NULL,
		 * or if rt found by rt_check() didn't have llinfo.
		 */
		rt = arplookup(SIN(dst)->sin_addr.s_addr, 1, 0);
		if (rt == NULL) {
			log(LOG_DEBUG,
			    "arpresolve: can't allocate route for %s\n",
			    inet_ntoa(SIN(dst)->sin_addr));
			m_freem(m);
			return (EINVAL); /* XXX */
		}
		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (la == NULL) {
			RT_UNLOCK(rt);
			log(LOG_DEBUG,
			    "arpresolve: can't allocate llinfo for %s\n",
			    inet_ntoa(SIN(dst)->sin_addr));
			m_freem(m);
			return (EINVAL); /* XXX */
		}
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time_uptime) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {

		bcopy(LLADDR(sdl), desten, sdl->sdl_alen);

		/*
		 * If entry has an expiry time and it is approaching,
		 * send an ARP request.
		 */
		if ((rt->rt_expire != 0) &&
		    (time_uptime + la->la_preempt > rt->rt_expire)) {
			struct in_addr sin = 
			    SIN(rt->rt_ifa->ifa_addr)->sin_addr;

			la->la_preempt--;
			RT_UNLOCK(rt);
			arprequest(ifp, &sin, &SIN(dst)->sin_addr,
			    IF_LLADDR(ifp));
			return (0);
		} 

		RT_UNLOCK(rt);
		return (0);
	}
	/*
	 * If ARP is disabled or static on this interface, stop.
	 * XXX
	 * Probably should not allocate empty llinfo struct if we are
	 * not going to be sending out an arp request.
	 */
	if (ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) {
		RT_UNLOCK(rt);
		m_freem(m);
		return (EINVAL);
	}
	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	if (la->la_hold)
		m_freem(la->la_hold);
	la->la_hold = m;

	KASSERT(rt->rt_expire > 0, ("sending ARP request for static entry"));

	/*
	 * Return EWOULDBLOCK if we have tried less than arp_maxtries. It
	 * will be masked by ether_output(). Return EHOSTDOWN/EHOSTUNREACH
	 * if we have already sent arp_maxtries ARP requests. Retransmit the
	 * ARP request, but not faster than one request per second.
	 */
	if (la->la_asked < arp_maxtries)
		error = EWOULDBLOCK;	/* First request. */
	else
		error = (rt == rt0) ? EHOSTDOWN : EHOSTUNREACH;

	if (la->la_asked == 0 || rt->rt_expire != time_uptime) {
		struct in_addr sin =
		    SIN(rt->rt_ifa->ifa_addr)->sin_addr;

		rt->rt_expire = time_uptime;
		callout_reset(&la->la_timer, hz, arptimer, rt);
		la->la_asked++;
		RT_UNLOCK(rt);

		arprequest(ifp, &sin, &SIN(dst)->sin_addr,
		    IF_LLADDR(ifp));
	} else
		RT_UNLOCK(rt);

	return (error);
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
static void
arpintr(struct mbuf *m)
{
	struct arphdr *ar;

	if (m->m_len < sizeof(struct arphdr) &&
	    ((m = m_pullup(m, sizeof(struct arphdr))) == NULL)) {
		log(LOG_ERR, "arp: runt packet -- m_pullup failed\n");
		return;
	}
	ar = mtod(m, struct arphdr *);

	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER &&
	    ntohs(ar->ar_hrd) != ARPHRD_IEEE802 &&
	    ntohs(ar->ar_hrd) != ARPHRD_ARCNET &&
	    ntohs(ar->ar_hrd) != ARPHRD_IEEE1394) {
		log(LOG_ERR, "arp: unknown hardware address format (0x%2D)\n",
		    (unsigned char *)&ar->ar_hrd, "");
		m_freem(m);
		return;
	}

	if (m->m_len < arphdr_len(ar)) {
		if ((m = m_pullup(m, arphdr_len(ar))) == NULL) {
			log(LOG_ERR, "arp: runt packet\n");
			m_freem(m);
			return;
		}
		ar = mtod(m, struct arphdr *);
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
static int log_arp_permanent_modify = 1;

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_wrong_iface, CTLFLAG_RW,
	&log_arp_wrong_iface, 0,
	"log arp packets arriving on the wrong interface");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_movements, CTLFLAG_RW,
        &log_arp_movements, 0,
        "log arp replies from MACs different than the one in the cache");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_permanent_modify, CTLFLAG_RW,
        &log_arp_permanent_modify, 0,
        "log arp replies from MACs different than the one in the permanent arp entry");


static void
in_arpinput(struct mbuf *m)
{
	struct arphdr *ah;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct llinfo_arp *la;
	struct rtentry *rt;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	struct mbuf *hold;
	u_int8_t *enaddr = NULL;
	int op, rif_len;
	int req_len;
	int bridged = 0;
#ifdef DEV_CARP
	int carp_match = 0;
#endif

	if (ifp->if_bridge)
		bridged = 1;

	req_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
	if (m->m_len < req_len && (m = m_pullup(m, req_len)) == NULL) {
		log(LOG_ERR, "in_arp: runt packet -- m_pullup failed\n");
		return;
	}

	ah = mtod(m, struct arphdr *);
	op = ntohs(ah->ar_op);
	(void)memcpy(&isaddr, ar_spa(ah), sizeof (isaddr));
	(void)memcpy(&itaddr, ar_tpa(ah), sizeof (itaddr));

	/*
	 * For a bridge, we want to check the address irrespective
	 * of the receive interface. (This will change slightly
	 * when we have clusters of interfaces).
	 * If the interface does not match, but the recieving interface
	 * is part of carp, we call carp_iamatch to see if this is a
	 * request for the virtual host ip.
	 * XXX: This is really ugly!
	 */
	LIST_FOREACH(ia, INADDR_HASH(itaddr.s_addr), ia_hash) {
		if (((bridged && ia->ia_ifp->if_bridge != NULL) ||
		    (ia->ia_ifp == ifp)) &&
		    itaddr.s_addr == ia->ia_addr.sin_addr.s_addr)
			goto match;
#ifdef DEV_CARP
		if (ifp->if_carp != NULL &&
		    carp_iamatch(ifp->if_carp, ia, &isaddr, &enaddr) &&
		    itaddr.s_addr == ia->ia_addr.sin_addr.s_addr) {
			carp_match = 1;
			goto match;
		}
#endif
	}
	LIST_FOREACH(ia, INADDR_HASH(isaddr.s_addr), ia_hash)
		if (((bridged && ia->ia_ifp->if_bridge != NULL) ||
		    (ia->ia_ifp == ifp)) &&
		    isaddr.s_addr == ia->ia_addr.sin_addr.s_addr)
			goto match;
	/*
	 * No match, use the first inet address on the receive interface
	 * as a dummy address for the rest of the function.
	 */
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_INET) {
			ia = ifatoia(ifa);
			goto match;
		}
	/*
	 * If bridging, fall back to using any inet address.
	 */
	if (!bridged || (ia = TAILQ_FIRST(&in_ifaddrhead)) == NULL)
		goto drop;
match:
	if (!enaddr)
		enaddr = (u_int8_t *)IF_LLADDR(ifp);
	myaddr = ia->ia_addr.sin_addr;
	if (!bcmp(ar_sha(ah), enaddr, ifp->if_addrlen))
		goto drop;	/* it's from me, ignore it. */
	if (!bcmp(ar_sha(ah), ifp->if_broadcastaddr, ifp->if_addrlen)) {
		log(LOG_ERR,
		    "arp: link address is broadcast for IP address %s!\n",
		    inet_ntoa(isaddr));
		goto drop;
	}
	/*
	 * Warn if another host is using the same IP address, but only if the
	 * IP address isn't 0.0.0.0, which is used for DHCP only, in which
	 * case we suppress the warning to avoid false positive complaints of
	 * potential misconfiguration.
	 */
	if (!bridged && isaddr.s_addr == myaddr.s_addr && myaddr.s_addr != 0) {
		log(LOG_ERR,
		   "arp: %*D is using my IP address %s on %s!\n",
		   ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
		   inet_ntoa(isaddr), ifp->if_xname);
		itaddr = myaddr;
		goto reply;
	}
	if (ifp->if_flags & IFF_STATICARP)
		goto reply;
	rt = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
	if (rt != NULL) {
		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (la == NULL) {
			RT_UNLOCK(rt);
			goto reply;
		}
	} else
		goto reply;

	/* The following is not an error when doing bridging. */
	if (!bridged && rt->rt_ifp != ifp
#ifdef DEV_CARP
	    && (ifp->if_type != IFT_CARP || !carp_match)
#endif
							) {
		if (log_arp_wrong_iface)
			log(LOG_ERR, "arp: %s is on %s but got reply from %*D on %s\n",
			    inet_ntoa(isaddr),
			    rt->rt_ifp->if_xname,
			    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
			    ifp->if_xname);
		RT_UNLOCK(rt);
		goto reply;
	}
	sdl = SDL(rt->rt_gateway);
	if (sdl->sdl_alen &&
	    bcmp(ar_sha(ah), LLADDR(sdl), sdl->sdl_alen)) {
		if (rt->rt_expire) {
		    if (log_arp_movements)
		        log(LOG_INFO, "arp: %s moved from %*D to %*D on %s\n",
			    inet_ntoa(isaddr),
			    ifp->if_addrlen, (u_char *)LLADDR(sdl), ":",
			    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
			    ifp->if_xname);
		} else {
			RT_UNLOCK(rt);
			if (log_arp_permanent_modify)
				log(LOG_ERR, "arp: %*D attempts to modify "
				    "permanent entry for %s on %s\n",
				    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				    inet_ntoa(isaddr), ifp->if_xname);
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
		RT_UNLOCK(rt);
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
		struct iso88025_header *th = NULL;
		struct iso88025_sockaddr_dl_data *trld;

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
	if (rt->rt_expire) {
		rt->rt_expire = time_uptime + arpt_keep;
		callout_reset(&la->la_timer, hz * arpt_keep, arptimer, rt);
	}
	la->la_asked = 0;
	la->la_preempt = arp_maxtries;
	hold = la->la_hold;
	la->la_hold = NULL;
	RT_UNLOCK(rt);
	if (hold != NULL)
		(*ifp->if_output)(ifp, hold, rt_key(rt), rt);

reply:
	if (op != ARPOP_REQUEST)
		goto drop;
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
		(void)memcpy(ar_sha(ah), enaddr, ah->ar_hln);
	} else {
		rt = arplookup(itaddr.s_addr, 0, SIN_PROXY);
		if (rt == NULL) {
			struct sockaddr_in sin;

			if (!arp_proxyall)
				goto drop;

			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = itaddr;

			rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
			if (!rt)
				goto drop;
			/*
			 * Don't send proxies for nodes on the same interface
			 * as this one came out of, or we'll get into a fight
			 * over who claims what Ether address.
			 */
			if (rt->rt_ifp == ifp) {
				rtfree(rt);
				goto drop;
			}
			(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
			(void)memcpy(ar_sha(ah), enaddr, ah->ar_hln);
			rtfree(rt);

			/*
			 * Also check that the node which sent the ARP packet
			 * is on the the interface we expect it to be on. This
			 * avoids ARP chaos if an interface is connected to the
			 * wrong network.
			 */
			sin.sin_addr = isaddr;

			rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
			if (!rt)
				goto drop;
			if (rt->rt_ifp != ifp) {
				log(LOG_INFO, "arp_proxy: ignoring request"
				    " from %s via %s, expecting %s\n",
				    inet_ntoa(isaddr), ifp->if_xname,
				    rt->rt_ifp->if_xname);
				rtfree(rt);
				goto drop;
			}
			rtfree(rt);

#ifdef DEBUG_PROXY
			printf("arp: proxying for %s\n",
			       inet_ntoa(itaddr));
#endif
		} else {
			/*
			 * Return proxied ARP replies only on the interface
			 * or bridge cluster where this network resides.
			 * Otherwise we may conflict with the host we are
			 * proxying for.
			 */
			if (rt->rt_ifp != ifp &&
			    (rt->rt_ifp->if_bridge != ifp->if_bridge ||
			    ifp->if_bridge == NULL)) {
				RT_UNLOCK(rt);
				goto drop;
			}
			sdl = SDL(rt->rt_gateway);
			(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
			(void)memcpy(ar_sha(ah), LLADDR(sdl), ah->ar_hln);
			RT_UNLOCK(rt);
		}
	}

	if (itaddr.s_addr == myaddr.s_addr &&
	    IN_LINKLOCAL(ntohl(itaddr.s_addr))) {
		/* RFC 3927 link-local IPv4; always reply by broadcast. */
#ifdef DEBUG_LINKLOCAL
		printf("arp: sending reply for link-local addr %s\n",
		    inet_ntoa(itaddr));
#endif
		m->m_flags |= M_BCAST;
		m->m_flags &= ~M_MCAST;
	} else {
		/* default behaviour; never reply by broadcast. */
		m->m_flags &= ~(M_BCAST|M_MCAST);
	}
	(void)memcpy(ar_tpa(ah), ar_spa(ah), ah->ar_pln);
	(void)memcpy(ar_spa(ah), &itaddr, ah->ar_pln);
	ah->ar_op = htons(ARPOP_REPLY);
	ah->ar_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	m->m_len = sizeof(*ah) + (2 * ah->ar_pln) + (2 * ah->ar_hln);   
	m->m_pkthdr.len = m->m_len;   
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
	return;

drop:
	m_freem(m);
}
#endif

/*
 * Lookup or enter a new address in arptab.
 */
static struct rtentry *
arplookup(u_long addr, int create, int proxy)
{
	struct rtentry *rt;
	struct sockaddr_inarp sin;
	const char *why = 0;

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	if (proxy)
		sin.sin_other = SIN_PROXY;
	rt = rtalloc1((struct sockaddr *)&sin, create, 0UL);
	if (rt == 0)
		return (0);

	if (rt->rt_flags & RTF_GATEWAY)
		why = "host is not on local network";
	else if ((rt->rt_flags & RTF_LLINFO) == 0)
		why = "could not allocate llinfo";
	else if (rt->rt_gateway->sa_family != AF_LINK)
		why = "gateway route is not ours";

	if (why) {
#define	ISDYNCLONE(_rt) \
	(((_rt)->rt_flags & (RTF_STATIC | RTF_WASCLONED)) == RTF_WASCLONED)
		if (create)
			log(LOG_DEBUG, "arplookup %s failed: %s\n",
			    inet_ntoa(sin.sin_addr), why);
		/*
		 * If there are no references to this Layer 2 route,
		 * and it is a cloned route, and not static, and
		 * arplookup() is creating the route, then purge
		 * it from the routing table as it is probably bogus.
		 */
		if (rt->rt_refcnt == 1 && ISDYNCLONE(rt))
			rtexpunge(rt);
		RTFREE_LOCKED(rt);
		return (0);
#undef ISDYNCLONE
	} else {
		RT_REMREF(rt);
		return (rt);
	}
}

void
arp_ifinit(struct ifnet *ifp, struct ifaddr *ifa)
{
	if (ntohl(IA_SIN(ifa)->sin_addr.s_addr) != INADDR_ANY)
		arprequest(ifp, &IA_SIN(ifa)->sin_addr,
				&IA_SIN(ifa)->sin_addr, IF_LLADDR(ifp));
	ifa->ifa_rtrequest = arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}

void
arp_ifinit2(struct ifnet *ifp, struct ifaddr *ifa, u_char *enaddr)
{
	if (ntohl(IA_SIN(ifa)->sin_addr.s_addr) != INADDR_ANY)
		arprequest(ifp, &IA_SIN(ifa)->sin_addr,
				&IA_SIN(ifa)->sin_addr, enaddr);
	ifa->ifa_rtrequest = arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}

static void
arp_init(void)
{

	arpintrq.ifq_maxlen = 50;
	mtx_init(&arpintrq.ifq_mtx, "arp_inq", NULL, MTX_DEF);
	netisr_register(NETISR_ARP, arpintr, &arpintrq, NETISR_MPSAFE);
}
SYSINIT(arp, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, arp_init, 0);
