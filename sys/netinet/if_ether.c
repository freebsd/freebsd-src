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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/if_llc.h>
#include <net/ethernet.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <net/if_llatbl.h>
#include <netinet/if_ether.h>
#ifdef INET
#include <netinet/ip_carp.h>
#endif

#include <net/if_arc.h>
#include <net/iso88025.h>

#include <security/mac/mac_framework.h>

#define SIN(s) ((const struct sockaddr_in *)(s))
#define SDL(s) ((struct sockaddr_dl *)s)

SYSCTL_DECL(_net_link_ether);
static SYSCTL_NODE(_net_link_ether, PF_INET, inet, CTLFLAG_RW, 0, "");
static SYSCTL_NODE(_net_link_ether, PF_ARP, arp, CTLFLAG_RW, 0, "");

/* timer values */
static VNET_DEFINE(int, arpt_keep) = (20*60);	/* once resolved, good for 20
						 * minutes */
static VNET_DEFINE(int, arp_maxtries) = 5;
static VNET_DEFINE(int, arp_proxyall) = 0;
static VNET_DEFINE(int, arpt_down) = 20;	/* keep incomplete entries for
						 * 20 seconds */
VNET_PCPUSTAT_DEFINE(struct arpstat, arpstat);  /* ARP statistics, see if_arp.h */
VNET_PCPUSTAT_SYSINIT(arpstat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(arpstat);
#endif /* VIMAGE */

static VNET_DEFINE(int, arp_maxhold) = 1;

#define	V_arpt_keep		VNET(arpt_keep)
#define	V_arpt_down		VNET(arpt_down)
#define	V_arp_maxtries		VNET(arp_maxtries)
#define	V_arp_proxyall		VNET(arp_proxyall)
#define	V_arp_maxhold		VNET(arp_maxhold)

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, max_age, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(arpt_keep), 0,
	"ARP entry lifetime in seconds");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, maxtries, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(arp_maxtries), 0,
	"ARP resolution attempts before returning error");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, proxyall, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(arp_proxyall), 0,
	"Enable proxy ARP for all suitable requests");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, wait, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(arpt_down), 0,
	"Incomplete ARP entry lifetime in seconds");
SYSCTL_VNET_PCPUSTAT(_net_link_ether_arp, OID_AUTO, stats, struct arpstat,
    arpstat, "ARP statistics (struct arpstat, net/if_arp.h)");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, maxhold, CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(arp_maxhold), 0,
	"Number of packets to hold per ARP entry");

static void	arp_init(void);
static void	arpintr(struct mbuf *);
static void	arptimer(void *);
#ifdef INET
static void	in_arpinput(struct mbuf *);
#endif

static const struct netisr_handler arp_nh = {
	.nh_name = "arp",
	.nh_handler = arpintr,
	.nh_proto = NETISR_ARP,
	.nh_policy = NETISR_POLICY_SOURCE,
};

#ifdef AF_INET
/*
 * called by in_scrubprefix() to remove entry from the table when
 * the interface goes away
 */
void
arp_ifscrub(struct ifnet *ifp, uint32_t addr)
{
	struct sockaddr_in addr4;

	bzero((void *)&addr4, sizeof(addr4));
	addr4.sin_len    = sizeof(addr4);
	addr4.sin_family = AF_INET;
	addr4.sin_addr.s_addr = addr;
	IF_AFDATA_WLOCK(ifp);
	lla_delete(LLTABLE(ifp), LLE_IFADDR, (struct sockaddr *)&addr4);
	IF_AFDATA_WUNLOCK(ifp);
}
#endif

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
static void
arptimer(void *arg)
{
	struct llentry *lle = (struct llentry *)arg;
	struct ifnet *ifp;

	if (lle->la_flags & LLE_STATIC) {
		return;
	}
	LLE_WLOCK(lle);
	if (callout_pending(&lle->la_timer)) {
		/*
		 * Here we are a bit odd here in the treatment of 
		 * active/pending. If the pending bit is set, it got
		 * rescheduled before I ran. The active
		 * bit we ignore, since if it was stopped
		 * in ll_tablefree() and was currently running
		 * it would have return 0 so the code would
		 * not have deleted it since the callout could
		 * not be stopped so we want to go through
		 * with the delete here now. If the callout
		 * was restarted, the pending bit will be back on and
		 * we just want to bail since the callout_reset would
		 * return 1 and our reference would have been removed
		 * by arpresolve() below.
		 */
		LLE_WUNLOCK(lle);
 		return;
 	}
	ifp = lle->lle_tbl->llt_ifp;
	CURVNET_SET(ifp->if_vnet);

	if ((lle->la_flags & LLE_DELETED) == 0) {
		int evt;

		if (lle->la_flags & LLE_VALID)
			evt = LLENTRY_EXPIRED;
		else
			evt = LLENTRY_TIMEDOUT;
		EVENTHANDLER_INVOKE(lle_event, lle, evt);
	}

	callout_stop(&lle->la_timer);

	/* XXX: LOR avoidance. We still have ref on lle. */
	LLE_WUNLOCK(lle);
	IF_AFDATA_LOCK(ifp);
	LLE_WLOCK(lle);

	/* Guard against race with other llentry_free(). */
	if (lle->la_flags & LLE_LINKED) {
		size_t pkts_dropped;

		LLE_REMREF(lle);
		pkts_dropped = llentry_free(lle);
		ARPSTAT_ADD(dropped, pkts_dropped);
	} else
		LLE_FREE_LOCKED(lle);

	IF_AFDATA_UNLOCK(ifp);

	ARPSTAT_INC(timeouts);

	CURVNET_RESTORE();
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
void
arprequest(struct ifnet *ifp, const struct in_addr *sip,
    const struct in_addr *tip, u_char *enaddr)
{
	struct mbuf *m;
	struct arphdr *ah;
	struct sockaddr sa;
	u_char *carpaddr = NULL;

	if (sip == NULL) {
		/*
		 * The caller did not supply a source address, try to find
		 * a compatible one among those assigned to this interface.
		 */
		struct ifaddr *ifa;

		IF_ADDR_RLOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;

			if (ifa->ifa_carp) {
				if ((*carp_iamatch_p)(ifa, &carpaddr) == 0)
					continue;
				sip = &IA_SIN(ifa)->sin_addr;
			} else {
				carpaddr = NULL;
				sip = &IA_SIN(ifa)->sin_addr;
			}

			if (0 == ((sip->s_addr ^ tip->s_addr) &
			    IA_MASKSIN(ifa)->sin_addr.s_addr))
				break;  /* found it. */
		}
		IF_ADDR_RUNLOCK(ifp);
		if (sip == NULL) {
			printf("%s: cannot find matching address\n", __func__);
			return;
		}
	}
	if (enaddr == NULL)
		enaddr = carpaddr ? carpaddr : (u_char *)IF_LLADDR(ifp);

	if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ah) + 2 * sizeof(struct in_addr) +
		2 * ifp->if_addrlen;
	m->m_pkthdr.len = m->m_len;
	M_ALIGN(m, m->m_len);
	ah = mtod(m, struct arphdr *);
	bzero((caddr_t)ah, m->m_len);
#ifdef MAC
	mac_netinet_arp_send(ifp, m);
#endif
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ifp->if_addrlen;		/* hardware address length */
	ah->ar_pln = sizeof(struct in_addr);	/* protocol address length */
	ah->ar_op = htons(ARPOP_REQUEST);
	bcopy(enaddr, ar_sha(ah), ah->ar_hln);
	bcopy(sip, ar_spa(ah), ah->ar_pln);
	bcopy(tip, ar_tpa(ah), ah->ar_pln);
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	m->m_flags |= M_BCAST;
	m_clrprotoflags(m);	/* Avoid confusing lower layers. */
	(*ifp->if_output)(ifp, m, &sa, NULL);
	ARPSTAT_INC(txrequests);
}

/*
 * Resolve an IP address into an ethernet address.
 * On input:
 *    ifp is the interface we use
 *    is_gw != if @dst represents gateway to some destination
 *    m is the mbuf. May be NULL if we don't have a packet.
 *    dst is the next hop,
 *    desten is where we want the address.
 *    flags returns lle entry flags.
 *
 * On success, desten and flags are filled in and the function returns 0;
 * If the packet must be held pending resolution, we return EWOULDBLOCK
 * On other errors, we return the corresponding error code.
 * Note that m_freem() handles NULL.
 */
int
arpresolve(struct ifnet *ifp, int is_gw, struct mbuf *m,
	const struct sockaddr *dst, u_char *desten, uint32_t *pflags)
{
	struct llentry *la = 0;
	u_int flags = 0;
	struct mbuf *curr = NULL;
	struct mbuf *next = NULL;
	int create, error, renew;

	if (pflags != NULL)
		*pflags = 0;

	create = 0;
	if (m != NULL) {
		if (m->m_flags & M_BCAST) {
			/* broadcast */
			(void)memcpy(desten,
			    ifp->if_broadcastaddr, ifp->if_addrlen);
			return (0);
		}
		if (m->m_flags & M_MCAST) {
			/* multicast */
			ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr, desten);
			return (0);
		}
	}
retry:
	IF_AFDATA_RLOCK(ifp);
	la = lla_lookup(LLTABLE(ifp), flags, dst);
	IF_AFDATA_RUNLOCK(ifp);
	if ((la == NULL) && ((flags & LLE_EXCLUSIVE) == 0)
	    && ((ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) == 0)) {
		create = 1;
		flags |= LLE_EXCLUSIVE;
		IF_AFDATA_WLOCK(ifp);
		la = lla_create(LLTABLE(ifp), flags, dst);
		IF_AFDATA_WUNLOCK(ifp);
	}
	if (la == NULL) {
		if (create != 0)
			log(LOG_DEBUG,
			    "arpresolve: can't allocate llinfo for %s on %s\n",
			    inet_ntoa(SIN(dst)->sin_addr), ifp->if_xname);
		m_freem(m);
		return (EINVAL);
	}

	if ((la->la_flags & LLE_VALID) &&
	    ((la->la_flags & LLE_STATIC) || la->la_expire > time_uptime)) {
		bcopy(&la->ll_addr, desten, ifp->if_addrlen);
		renew = 0;
		/*
		 * If entry has an expiry time and it is approaching,
		 * see if we need to send an ARP request within this
		 * arpt_down interval.
		 */
		if (!(la->la_flags & LLE_STATIC) &&
		    time_uptime + la->la_preempt > la->la_expire) {
			renew = 1;
			la->la_preempt--;
		}

		if (pflags != NULL)
			*pflags = la->la_flags;

		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(la);
		else
			LLE_RUNLOCK(la);

		if (renew == 1)
			arprequest(ifp, NULL, &SIN(dst)->sin_addr, NULL);

		return (0);
	}

	if (la->la_flags & LLE_STATIC) {   /* should not happen! */
		log(LOG_DEBUG, "arpresolve: ouch, empty static llinfo for %s\n",
		    inet_ntoa(SIN(dst)->sin_addr));
		m_freem(m);
		error = EINVAL;
		goto done;
	}

	renew = (la->la_asked == 0 || la->la_expire != time_uptime);
	if ((renew || m != NULL) && (flags & LLE_EXCLUSIVE) == 0) {
		flags |= LLE_EXCLUSIVE;
		LLE_RUNLOCK(la);
		goto retry;
	}
	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Add the mbuf to the list, dropping
	 * the oldest packet if we have exceeded the system
	 * setting.
	 */
	if (m != NULL) {
		if (la->la_numheld >= V_arp_maxhold) {
			if (la->la_hold != NULL) {
				next = la->la_hold->m_nextpkt;
				m_freem(la->la_hold);
				la->la_hold = next;
				la->la_numheld--;
				ARPSTAT_INC(dropped);
			}
		}
		if (la->la_hold != NULL) {
			curr = la->la_hold;
			while (curr->m_nextpkt != NULL)
				curr = curr->m_nextpkt;
			curr->m_nextpkt = m;
		} else
			la->la_hold = m;
		la->la_numheld++;
		if (renew == 0 && (flags & LLE_EXCLUSIVE)) {
			flags &= ~LLE_EXCLUSIVE;
			LLE_DOWNGRADE(la);
		}

	}
	/*
	 * Return EWOULDBLOCK if we have tried less than arp_maxtries. It
	 * will be masked by ether_output(). Return EHOSTDOWN/EHOSTUNREACH
	 * if we have already sent arp_maxtries ARP requests. Retransmit the
	 * ARP request, but not faster than one request per second.
	 */
	if (la->la_asked < V_arp_maxtries)
		error = EWOULDBLOCK;	/* First request. */
	else
		error = is_gw != 0 ? EHOSTUNREACH : EHOSTDOWN;

	if (renew) {
		int canceled;

		LLE_ADDREF(la);
		la->la_expire = time_uptime;
		canceled = callout_reset(&la->la_timer, hz * V_arpt_down,
		    arptimer, la);
		if (canceled)
			LLE_REMREF(la);
		la->la_asked++;
		LLE_WUNLOCK(la);
		arprequest(ifp, NULL, &SIN(dst)->sin_addr, NULL);
		return (error);
	}
done:
	if (flags & LLE_EXCLUSIVE)
		LLE_WUNLOCK(la);
	else
		LLE_RUNLOCK(la);
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
		log(LOG_NOTICE, "arp: runt packet -- m_pullup failed\n");
		return;
	}
	ar = mtod(m, struct arphdr *);

	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER &&
	    ntohs(ar->ar_hrd) != ARPHRD_IEEE802 &&
	    ntohs(ar->ar_hrd) != ARPHRD_ARCNET &&
	    ntohs(ar->ar_hrd) != ARPHRD_IEEE1394 &&
	    ntohs(ar->ar_hrd) != ARPHRD_INFINIBAND) {
		log(LOG_NOTICE, "arp: unknown hardware address format (0x%2D)"
		    " (from %*D to %*D)\n", (unsigned char *)&ar->ar_hrd, "",
		    ETHER_ADDR_LEN, (u_char *)ar_sha(ar), ":",
		    ETHER_ADDR_LEN, (u_char *)ar_tha(ar), ":");
		m_freem(m);
		return;
	}

	if (m->m_len < arphdr_len(ar)) {
		if ((m = m_pullup(m, arphdr_len(ar))) == NULL) {
			log(LOG_NOTICE, "arp: runt packet\n");
			m_freem(m);
			return;
		}
		ar = mtod(m, struct arphdr *);
	}

	ARPSTAT_INC(received);
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
static int allow_multicast = 0;
static struct timeval arp_lastlog;
static int arp_curpps;
static int arp_maxpps = 1;

SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_wrong_iface, CTLFLAG_RW,
	&log_arp_wrong_iface, 0,
	"log arp packets arriving on the wrong interface");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_movements, CTLFLAG_RW,
	&log_arp_movements, 0,
	"log arp replies from MACs different than the one in the cache");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, log_arp_permanent_modify, CTLFLAG_RW,
	&log_arp_permanent_modify, 0,
	"log arp replies from MACs different than the one in the permanent arp entry");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, allow_multicast, CTLFLAG_RW,
	&allow_multicast, 0, "accept multicast addresses");
SYSCTL_INT(_net_link_ether_inet, OID_AUTO, max_log_per_second,
	CTLFLAG_RW, &arp_maxpps, 0,
	"Maximum number of remotely triggered ARP messages that can be "
	"logged per second");

#define	ARP_LOG(pri, ...)	do {					\
	if (ppsratecheck(&arp_lastlog, &arp_curpps, arp_maxpps))	\
		log((pri), "arp: " __VA_ARGS__);			\
} while (0)

static void
in_arpinput(struct mbuf *m)
{
	struct rm_priotracker in_ifa_tracker;
	struct arphdr *ah;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct llentry *la = NULL;
	struct rtentry *rt;
	struct ifaddr *ifa;
	struct in_ifaddr *ia;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	u_int8_t *enaddr = NULL;
	int op, flags;
	int req_len;
	int bridged = 0, is_bridge = 0;
	int carped, create;
	struct sockaddr_in sin;
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;

	if (ifp->if_bridge)
		bridged = 1;
	if (ifp->if_type == IFT_BRIDGE)
		is_bridge = 1;

	req_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
	if (m->m_len < req_len && (m = m_pullup(m, req_len)) == NULL) {
		ARP_LOG(LOG_NOTICE, "runt packet -- m_pullup failed\n");
		return;
	}

	ah = mtod(m, struct arphdr *);
	/*
	 * ARP is only for IPv4 so we can reject packets with
	 * a protocol length not equal to an IPv4 address.
	 */
	if (ah->ar_pln != sizeof(struct in_addr)) {
		ARP_LOG(LOG_NOTICE, "requested protocol length != %zu\n",
		    sizeof(struct in_addr));
		goto drop;
	}

	if (allow_multicast == 0 && ETHER_IS_MULTICAST(ar_sha(ah))) {
		ARP_LOG(LOG_NOTICE, "%*D is multicast\n",
		    ifp->if_addrlen, (u_char *)ar_sha(ah), ":");
		goto drop;
	}

	op = ntohs(ah->ar_op);
	(void)memcpy(&isaddr, ar_spa(ah), sizeof (isaddr));
	(void)memcpy(&itaddr, ar_tpa(ah), sizeof (itaddr));

	if (op == ARPOP_REPLY)
		ARPSTAT_INC(rxreplies);

	/*
	 * For a bridge, we want to check the address irrespective
	 * of the receive interface. (This will change slightly
	 * when we have clusters of interfaces).
	 */
	IN_IFADDR_RLOCK(&in_ifa_tracker);
	LIST_FOREACH(ia, INADDR_HASH(itaddr.s_addr), ia_hash) {
		if (((bridged && ia->ia_ifp->if_bridge == ifp->if_bridge) ||
		    ia->ia_ifp == ifp) &&
		    itaddr.s_addr == ia->ia_addr.sin_addr.s_addr &&
		    (ia->ia_ifa.ifa_carp == NULL ||
		    (*carp_iamatch_p)(&ia->ia_ifa, &enaddr))) {
			ifa_ref(&ia->ia_ifa);
			IN_IFADDR_RUNLOCK(&in_ifa_tracker);
			goto match;
		}
	}
	LIST_FOREACH(ia, INADDR_HASH(isaddr.s_addr), ia_hash)
		if (((bridged && ia->ia_ifp->if_bridge == ifp->if_bridge) ||
		    ia->ia_ifp == ifp) &&
		    isaddr.s_addr == ia->ia_addr.sin_addr.s_addr) {
			ifa_ref(&ia->ia_ifa);
			IN_IFADDR_RUNLOCK(&in_ifa_tracker);
			goto match;
		}

#define BDG_MEMBER_MATCHES_ARP(addr, ifp, ia)				\
  (ia->ia_ifp->if_bridge == ifp->if_softc &&				\
  !bcmp(IF_LLADDR(ia->ia_ifp), IF_LLADDR(ifp), ifp->if_addrlen) &&	\
  addr == ia->ia_addr.sin_addr.s_addr)
	/*
	 * Check the case when bridge shares its MAC address with
	 * some of its children, so packets are claimed by bridge
	 * itself (bridge_input() does it first), but they are really
	 * meant to be destined to the bridge member.
	 */
	if (is_bridge) {
		LIST_FOREACH(ia, INADDR_HASH(itaddr.s_addr), ia_hash) {
			if (BDG_MEMBER_MATCHES_ARP(itaddr.s_addr, ifp, ia)) {
				ifa_ref(&ia->ia_ifa);
				ifp = ia->ia_ifp;
				IN_IFADDR_RUNLOCK(&in_ifa_tracker);
				goto match;
			}
		}
	}
#undef BDG_MEMBER_MATCHES_ARP
	IN_IFADDR_RUNLOCK(&in_ifa_tracker);

	/*
	 * No match, use the first inet address on the receive interface
	 * as a dummy address for the rest of the function.
	 */
	IF_ADDR_RLOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (ifa->ifa_carp == NULL ||
		    (*carp_iamatch_p)(ifa, &enaddr))) {
			ia = ifatoia(ifa);
			ifa_ref(ifa);
			IF_ADDR_RUNLOCK(ifp);
			goto match;
		}
	IF_ADDR_RUNLOCK(ifp);

	/*
	 * If bridging, fall back to using any inet address.
	 */
	IN_IFADDR_RLOCK(&in_ifa_tracker);
	if (!bridged || (ia = TAILQ_FIRST(&V_in_ifaddrhead)) == NULL) {
		IN_IFADDR_RUNLOCK(&in_ifa_tracker);
		goto drop;
	}
	ifa_ref(&ia->ia_ifa);
	IN_IFADDR_RUNLOCK(&in_ifa_tracker);
match:
	if (!enaddr)
		enaddr = (u_int8_t *)IF_LLADDR(ifp);
	carped = (ia->ia_ifa.ifa_carp != NULL);
	myaddr = ia->ia_addr.sin_addr;
	ifa_free(&ia->ia_ifa);
	if (!bcmp(ar_sha(ah), enaddr, ifp->if_addrlen))
		goto drop;	/* it's from me, ignore it. */
	if (!bcmp(ar_sha(ah), ifp->if_broadcastaddr, ifp->if_addrlen)) {
		ARP_LOG(LOG_NOTICE, "link address is broadcast for IP address "
		    "%s!\n", inet_ntoa(isaddr));
		goto drop;
	}
	/*
	 * Warn if another host is using the same IP address, but only if the
	 * IP address isn't 0.0.0.0, which is used for DHCP only, in which
	 * case we suppress the warning to avoid false positive complaints of
	 * potential misconfiguration.
	 */
	if (!bridged && !carped && isaddr.s_addr == myaddr.s_addr &&
	    myaddr.s_addr != 0) {
		ARP_LOG(LOG_ERR, "%*D is using my IP address %s on %s!\n",
		   ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
		   inet_ntoa(isaddr), ifp->if_xname);
		itaddr = myaddr;
		ARPSTAT_INC(dupips);
		goto reply;
	}
	if (ifp->if_flags & IFF_STATICARP)
		goto reply;

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_family = AF_INET;
	sin.sin_addr = isaddr;
	create = (itaddr.s_addr == myaddr.s_addr) ? 1 : 0;
	flags = LLE_EXCLUSIVE;
	IF_AFDATA_LOCK(ifp);
	if (create != 0)
		la = lla_create(LLTABLE(ifp), 0, (struct sockaddr *)&sin);
	else
		la = lla_lookup(LLTABLE(ifp), flags, (struct sockaddr *)&sin);
	IF_AFDATA_UNLOCK(ifp);
	if (la != NULL) {
		/* the following is not an error when doing bridging */
		if (!bridged && la->lle_tbl->llt_ifp != ifp) {
			if (log_arp_wrong_iface)
				ARP_LOG(LOG_WARNING, "%s is on %s "
				    "but got reply from %*D on %s\n",
				    inet_ntoa(isaddr),
				    la->lle_tbl->llt_ifp->if_xname,
				    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				    ifp->if_xname);
			LLE_WUNLOCK(la);
			goto reply;
		}
		if ((la->la_flags & LLE_VALID) &&
		    bcmp(ar_sha(ah), &la->ll_addr, ifp->if_addrlen)) {
			if (la->la_flags & LLE_STATIC) {
				LLE_WUNLOCK(la);
				if (log_arp_permanent_modify)
					ARP_LOG(LOG_ERR,
					    "%*D attempts to modify "
					    "permanent entry for %s on %s\n",
					    ifp->if_addrlen,
					    (u_char *)ar_sha(ah), ":",
					    inet_ntoa(isaddr), ifp->if_xname);
				goto reply;
			}
			if (log_arp_movements) {
				ARP_LOG(LOG_INFO, "%s moved from %*D "
				    "to %*D on %s\n",
				    inet_ntoa(isaddr),
				    ifp->if_addrlen,
				    (u_char *)&la->ll_addr, ":",
				    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
				    ifp->if_xname);
			}
		}

		if (ifp->if_addrlen != ah->ar_hln) {
			LLE_WUNLOCK(la);
			ARP_LOG(LOG_WARNING, "from %*D: addr len: new %d, "
			    "i/f %d (ignored)\n", ifp->if_addrlen,
			    (u_char *) ar_sha(ah), ":", ah->ar_hln,
			    ifp->if_addrlen);
			goto drop;
		}
		(void)memcpy(&la->ll_addr, ar_sha(ah), ifp->if_addrlen);
		la->la_flags |= LLE_VALID;

		EVENTHANDLER_INVOKE(lle_event, la, LLENTRY_RESOLVED);

		if (!(la->la_flags & LLE_STATIC)) {
			int canceled;

			LLE_ADDREF(la);
			la->la_expire = time_uptime + V_arpt_keep;
			canceled = callout_reset(&la->la_timer,
			    hz * V_arpt_keep, arptimer, la);
			if (canceled)
				LLE_REMREF(la);
		}
		la->la_asked = 0;
		la->la_preempt = V_arp_maxtries;
		/*
		 * The packets are all freed within the call to the output
		 * routine.
		 *
		 * NB: The lock MUST be released before the call to the
		 * output routine.
		 */
		if (la->la_hold != NULL) {
			struct mbuf *m_hold, *m_hold_next;

			m_hold = la->la_hold;
			la->la_hold = NULL;
			la->la_numheld = 0;
			memcpy(&sa, L3_ADDR(la), sizeof(sa));
			LLE_WUNLOCK(la);
			for (; m_hold != NULL; m_hold = m_hold_next) {
				m_hold_next = m_hold->m_nextpkt;
				m_hold->m_nextpkt = NULL;
				/* Avoid confusing lower layers. */
				m_clrprotoflags(m_hold);
				(*ifp->if_output)(ifp, m_hold, &sa, NULL);
			}
		} else
			LLE_WUNLOCK(la);
	}
reply:
	if (op != ARPOP_REQUEST)
		goto drop;
	ARPSTAT_INC(rxrequests);

	if (itaddr.s_addr == myaddr.s_addr) {
		/* Shortcut.. the receiving interface is the target. */
		(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
		(void)memcpy(ar_sha(ah), enaddr, ah->ar_hln);
	} else {
		struct llentry *lle = NULL;

		sin.sin_addr = itaddr;
		IF_AFDATA_RLOCK(ifp);
		lle = lla_lookup(LLTABLE(ifp), 0, (struct sockaddr *)&sin);
		IF_AFDATA_RUNLOCK(ifp);

		if ((lle != NULL) && (lle->la_flags & LLE_PUB)) {
			(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
			(void)memcpy(ar_sha(ah), &lle->ll_addr, ah->ar_hln);
			LLE_RUNLOCK(lle);
		} else {

			if (lle != NULL)
				LLE_RUNLOCK(lle);

			if (!V_arp_proxyall)
				goto drop;

			sin.sin_addr = itaddr;
			/* XXX MRT use table 0 for arp reply  */
			rt = in_rtalloc1((struct sockaddr *)&sin, 0, 0UL, 0);
			if (!rt)
				goto drop;

			/*
			 * Don't send proxies for nodes on the same interface
			 * as this one came out of, or we'll get into a fight
			 * over who claims what Ether address.
			 */
			if (!rt->rt_ifp || rt->rt_ifp == ifp) {
				RTFREE_LOCKED(rt);
				goto drop;
			}
			RTFREE_LOCKED(rt);

			(void)memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
			(void)memcpy(ar_sha(ah), enaddr, ah->ar_hln);

			/*
			 * Also check that the node which sent the ARP packet
			 * is on the interface we expect it to be on. This
			 * avoids ARP chaos if an interface is connected to the
			 * wrong network.
			 */
			sin.sin_addr = isaddr;

			/* XXX MRT use table 0 for arp checks */
			rt = in_rtalloc1((struct sockaddr *)&sin, 0, 0UL, 0);
			if (!rt)
				goto drop;
			if (rt->rt_ifp != ifp) {
				ARP_LOG(LOG_INFO, "proxy: ignoring request"
				    " from %s via %s, expecting %s\n",
				    inet_ntoa(isaddr), ifp->if_xname,
				    rt->rt_ifp->if_xname);
				RTFREE_LOCKED(rt);
				goto drop;
			}
			RTFREE_LOCKED(rt);

#ifdef DEBUG_PROXY
			printf("arp: proxying for %s\n", inet_ntoa(itaddr));
#endif
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
	m->m_pkthdr.rcvif = NULL;
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	m_clrprotoflags(m);	/* Avoid confusing lower layers. */
	(*ifp->if_output)(ifp, m, &sa, NULL);
	ARPSTAT_INC(txreplies);
	return;

drop:
	m_freem(m);
}
#endif

void
arp_ifinit(struct ifnet *ifp, struct ifaddr *ifa)
{
	struct llentry *lle;

	if (ifa->ifa_carp != NULL)
		return;

	if (ntohl(IA_SIN(ifa)->sin_addr.s_addr) != INADDR_ANY) {
		arprequest(ifp, &IA_SIN(ifa)->sin_addr,
				&IA_SIN(ifa)->sin_addr, IF_LLADDR(ifp));
		/*
		 * interface address is considered static entry
		 * because the output of the arp utility shows
		 * that L2 entry as permanent
		 */
		IF_AFDATA_LOCK(ifp);
		lle = lla_create(LLTABLE(ifp), LLE_IFADDR | LLE_STATIC,
				 (struct sockaddr *)IA_SIN(ifa));
		IF_AFDATA_UNLOCK(ifp);
		if (lle == NULL)
			log(LOG_INFO, "arp_ifinit: cannot create arp "
			    "entry for interface address\n");
		else
			LLE_WUNLOCK(lle);
	}
	ifa->ifa_rtrequest = NULL;
}

void
arp_ifinit2(struct ifnet *ifp, struct ifaddr *ifa, u_char *enaddr)
{
	if (ntohl(IA_SIN(ifa)->sin_addr.s_addr) != INADDR_ANY)
		arprequest(ifp, &IA_SIN(ifa)->sin_addr,
				&IA_SIN(ifa)->sin_addr, enaddr);
	ifa->ifa_rtrequest = NULL;
}

static void
arp_init(void)
{

	netisr_register(&arp_nh);
}
SYSINIT(arp, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, arp_init, 0);
