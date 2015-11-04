/*-
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/vnet.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>

#include <machine/in_cksum.h>
#include <security/mac/mac_framework.h>

#define	MEMTU			1500
static const char mename[] = "me";
static MALLOC_DEFINE(M_IFME, mename, "Minimal Encapsulation for IP");
static VNET_DEFINE(struct mtx, me_mtx);
#define	V_me_mtx	VNET(me_mtx)
/* Minimal forwarding header RFC 2004 */
struct mobhdr {
	uint8_t		mob_proto;	/* protocol */
	uint8_t		mob_flags;	/* flags */
#define	MOB_FLAGS_SP	0x80		/* source present */
	uint16_t	mob_csum;	/* header checksum */
	struct in_addr	mob_dst;	/* original destination address */
	struct in_addr	mob_src;	/* original source addr (optional) */
} __packed;

struct me_softc {
	struct ifnet		*me_ifp;
	LIST_ENTRY(me_softc)	me_list;
	struct rmlock		me_lock;
	u_int			me_fibnum;
	const struct encaptab	*me_ecookie;
	struct in_addr		me_src;
	struct in_addr		me_dst;
};
#define	ME2IFP(sc)		((sc)->me_ifp)
#define	ME_READY(sc)		((sc)->me_src.s_addr != 0)
#define	ME_LOCK_INIT(sc)	rm_init(&(sc)->me_lock, "me softc")
#define	ME_LOCK_DESTROY(sc)	rm_destroy(&(sc)->me_lock)
#define	ME_RLOCK_TRACKER	struct rm_priotracker me_tracker
#define	ME_RLOCK(sc)		rm_rlock(&(sc)->me_lock, &me_tracker)
#define	ME_RUNLOCK(sc)		rm_runlock(&(sc)->me_lock, &me_tracker)
#define	ME_RLOCK_ASSERT(sc)	rm_assert(&(sc)->me_lock, RA_RLOCKED)
#define	ME_WLOCK(sc)		rm_wlock(&(sc)->me_lock)
#define	ME_WUNLOCK(sc)		rm_wunlock(&(sc)->me_lock)
#define	ME_WLOCK_ASSERT(sc)	rm_assert(&(sc)->me_lock, RA_WLOCKED)

#define	ME_LIST_LOCK_INIT(x)	mtx_init(&V_me_mtx, "me_mtx", NULL, MTX_DEF)
#define	ME_LIST_LOCK_DESTROY(x)	mtx_destroy(&V_me_mtx)
#define	ME_LIST_LOCK(x)		mtx_lock(&V_me_mtx)
#define	ME_LIST_UNLOCK(x)	mtx_unlock(&V_me_mtx)

static VNET_DEFINE(LIST_HEAD(, me_softc), me_softc_list);
#define	V_me_softc_list	VNET(me_softc_list)
static struct sx me_ioctl_sx;
SX_SYSINIT(me_ioctl_sx, &me_ioctl_sx, "me_ioctl");

static int	me_clone_create(struct if_clone *, int, caddr_t);
static void	me_clone_destroy(struct ifnet *);
static VNET_DEFINE(struct if_clone *, me_cloner);
#define	V_me_cloner	VNET(me_cloner)

static void	me_qflush(struct ifnet *);
static int	me_transmit(struct ifnet *, struct mbuf *);
static int	me_ioctl(struct ifnet *, u_long, caddr_t);
static int	me_output(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);
static int	me_input(struct mbuf **, int *, int);

static int	me_set_tunnel(struct ifnet *, struct sockaddr_in *,
    struct sockaddr_in *);
static void	me_delete_tunnel(struct ifnet *);

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_TUNNEL, me, CTLFLAG_RW, 0,
    "Minimal Encapsulation for IP (RFC 2004)");
#ifndef MAX_ME_NEST
#define MAX_ME_NEST 1
#endif

static VNET_DEFINE(int, max_me_nesting) = MAX_ME_NEST;
#define	V_max_me_nesting	VNET(max_me_nesting)
SYSCTL_INT(_net_link_me, OID_AUTO, max_nesting, CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(max_me_nesting), 0, "Max nested tunnels");

extern struct domain inetdomain;
static void me_input10(struct mbuf *, int);
static const struct protosw in_mobile_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_MOBILE,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		me_input10,
	.pr_output =		(pr_output_t *)rip_output,
	.pr_ctlinput =		rip_ctlinput,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};

static void
vnet_me_init(const void *unused __unused)
{
	LIST_INIT(&V_me_softc_list);
	ME_LIST_LOCK_INIT();
	V_me_cloner = if_clone_simple(mename, me_clone_create,
	    me_clone_destroy, 0);
}
VNET_SYSINIT(vnet_me_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_me_init, NULL);

static void
vnet_me_uninit(const void *unused __unused)
{

	if_clone_detach(V_me_cloner);
	ME_LIST_LOCK_DESTROY();
}
VNET_SYSUNINIT(vnet_me_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_me_uninit, NULL);

static int
me_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct me_softc *sc;

	sc = malloc(sizeof(struct me_softc), M_IFME, M_WAITOK | M_ZERO);
	sc->me_fibnum = curthread->td_proc->p_fibnum;
	ME2IFP(sc) = if_alloc(IFT_TUNNEL);
	ME_LOCK_INIT(sc);
	ME2IFP(sc)->if_softc = sc;
	if_initname(ME2IFP(sc), mename, unit);

	ME2IFP(sc)->if_mtu = MEMTU - sizeof(struct mobhdr);
	ME2IFP(sc)->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	ME2IFP(sc)->if_output = me_output;
	ME2IFP(sc)->if_ioctl = me_ioctl;
	ME2IFP(sc)->if_transmit = me_transmit;
	ME2IFP(sc)->if_qflush = me_qflush;
	ME2IFP(sc)->if_capabilities |= IFCAP_LINKSTATE;
	ME2IFP(sc)->if_capenable |= IFCAP_LINKSTATE;
	if_attach(ME2IFP(sc));
	bpfattach(ME2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	ME_LIST_LOCK();
	LIST_INSERT_HEAD(&V_me_softc_list, sc, me_list);
	ME_LIST_UNLOCK();
	return (0);
}

static void
me_clone_destroy(struct ifnet *ifp)
{
	struct me_softc *sc;

	sx_xlock(&me_ioctl_sx);
	sc = ifp->if_softc;
	me_delete_tunnel(ifp);
	ME_LIST_LOCK();
	LIST_REMOVE(sc, me_list);
	ME_LIST_UNLOCK();
	bpfdetach(ifp);
	if_detach(ifp);
	ifp->if_softc = NULL;
	sx_xunlock(&me_ioctl_sx);

	if_free(ifp);
	ME_LOCK_DESTROY(sc);
	free(sc, M_IFME);
}

static int
me_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	ME_RLOCK_TRACKER;
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr_in *src, *dst;
	struct me_softc *sc;
	int error;

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 576)
			return (EINVAL);
		ifp->if_mtu = ifr->ifr_mtu - sizeof(struct mobhdr);
		return (0);
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		return (0);
	}
	sx_xlock(&me_ioctl_sx);
	sc = ifp->if_softc;
	if (sc == NULL) {
		error = ENXIO;
		goto end;
	}
	error = 0;
	switch (cmd) {
	case SIOCSIFPHYADDR:
		src = (struct sockaddr_in *)
			&(((struct in_aliasreq *)data)->ifra_addr);
		dst = (struct sockaddr_in *)
			&(((struct in_aliasreq *)data)->ifra_dstaddr);
		if (src->sin_family != dst->sin_family ||
		    src->sin_family != AF_INET ||
		    src->sin_len != dst->sin_len ||
		    src->sin_len != sizeof(struct sockaddr_in)) {
			error = EINVAL;
			break;
		}
		if (src->sin_addr.s_addr == INADDR_ANY ||
		    dst->sin_addr.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		error = me_set_tunnel(ifp, src, dst);
		break;
	case SIOCDIFPHYADDR:
		me_delete_tunnel(ifp);
		break;
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
		ME_RLOCK(sc);
		if (!ME_READY(sc)) {
			error = EADDRNOTAVAIL;
			ME_RUNLOCK(sc);
			break;
		}
		src = (struct sockaddr_in *)&ifr->ifr_addr;
		memset(src, 0, sizeof(*src));
		src->sin_family = AF_INET;
		src->sin_len = sizeof(*src);
		switch (cmd) {
		case SIOCGIFPSRCADDR:
			src->sin_addr = sc->me_src;
			break;
		case SIOCGIFPDSTADDR:
			src->sin_addr = sc->me_dst;
			break;
		}
		ME_RUNLOCK(sc);
		error = prison_if(curthread->td_ucred, sintosa(src));
		if (error != 0)
			memset(src, 0, sizeof(*src));
		break;
	case SIOCGTUNFIB:
		ifr->ifr_fib = sc->me_fibnum;
		break;
	case SIOCSTUNFIB:
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		if (ifr->ifr_fib >= rt_numfibs)
			error = EINVAL;
		else
			sc->me_fibnum = ifr->ifr_fib;
		break;
	default:
		error = EINVAL;
		break;
	}
end:
	sx_xunlock(&me_ioctl_sx);
	return (error);
}

static int
me_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{
	ME_RLOCK_TRACKER;
	struct me_softc *sc;
	struct ip *ip;
	int ret;

	sc = (struct me_softc *)arg;
	if ((ME2IFP(sc)->if_flags & IFF_UP) == 0)
		return (0);

	M_ASSERTPKTHDR(m);

	if (m->m_pkthdr.len < sizeof(struct ip) + sizeof(struct mobhdr) -
	    sizeof(struct in_addr))
		return (0);

	ret = 0;
	ME_RLOCK(sc);
	if (ME_READY(sc)) {
		ip = mtod(m, struct ip *);
		if (sc->me_src.s_addr == ip->ip_dst.s_addr &&
		    sc->me_dst.s_addr == ip->ip_src.s_addr)
			ret = 32 * 2;
	}
	ME_RUNLOCK(sc);
	return (ret);
}

static int
me_set_tunnel(struct ifnet *ifp, struct sockaddr_in *src,
    struct sockaddr_in *dst)
{
	struct me_softc *sc, *tsc;

	sx_assert(&me_ioctl_sx, SA_XLOCKED);
	ME_LIST_LOCK();
	sc = ifp->if_softc;
	LIST_FOREACH(tsc, &V_me_softc_list, me_list) {
		if (tsc == sc || !ME_READY(tsc))
			continue;
		if (tsc->me_src.s_addr == src->sin_addr.s_addr &&
		    tsc->me_dst.s_addr == dst->sin_addr.s_addr) {
			ME_LIST_UNLOCK();
			return (EADDRNOTAVAIL);
		}
	}
	ME_LIST_UNLOCK();

	ME_WLOCK(sc);
	sc->me_dst = dst->sin_addr;
	sc->me_src = src->sin_addr;
	ME_WUNLOCK(sc);

	if (sc->me_ecookie == NULL)
		sc->me_ecookie = encap_attach_func(AF_INET, IPPROTO_MOBILE,
		    me_encapcheck, &in_mobile_protosw, sc);
	if (sc->me_ecookie != NULL) {
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		if_link_state_change(ifp, LINK_STATE_UP);
	}
	return (0);
}

static void
me_delete_tunnel(struct ifnet *ifp)
{
	struct me_softc *sc = ifp->if_softc;

	sx_assert(&me_ioctl_sx, SA_XLOCKED);
	if (sc->me_ecookie != NULL)
		encap_detach(sc->me_ecookie);
	sc->me_ecookie = NULL;
	ME_WLOCK(sc);
	sc->me_src.s_addr = 0;
	sc->me_dst.s_addr = 0;
	ME_WUNLOCK(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	if_link_state_change(ifp, LINK_STATE_DOWN);
}

static uint16_t
me_in_cksum(uint16_t *p, int nwords)
{
	uint32_t sum = 0;

	while (nwords-- > 0)
		sum += *p++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (~sum);
}

static void
me_input10(struct mbuf *m, int off)
{
	int proto;

	proto = (mtod(m, struct ip *))->ip_p;
	me_input(&m, &off, proto);
}

int
me_input(struct mbuf **mp, int *offp, int proto)
{
	struct me_softc *sc;
	struct mobhdr *mh;
	struct ifnet *ifp;
	struct mbuf *m;
	struct ip *ip;
	int hlen;

	m = *mp;
	sc = encap_getarg(m);
	KASSERT(sc != NULL, ("encap_getarg returned NULL"));

	ifp = ME2IFP(sc);
	/* checks for short packets */
	hlen = sizeof(struct mobhdr);
	if (m->m_pkthdr.len < sizeof(struct ip) + hlen)
		hlen -= sizeof(struct in_addr);
	if (m->m_len < sizeof(struct ip) + hlen)
		m = m_pullup(m, sizeof(struct ip) + hlen);
	if (m == NULL)
		goto drop;
	mh = (struct mobhdr *)mtodo(m, sizeof(struct ip));
	/* check for wrong flags */
	if (mh->mob_flags & (~MOB_FLAGS_SP)) {
		m_freem(m);
		goto drop;
	}
	if (mh->mob_flags) {
	       if (hlen != sizeof(struct mobhdr)) {
			m_freem(m);
			goto drop;
	       }
	} else
		hlen = sizeof(struct mobhdr) - sizeof(struct in_addr);
	/* check mobile header checksum */
	if (me_in_cksum((uint16_t *)mh, hlen / sizeof(uint16_t)) != 0) {
		m_freem(m);
		goto drop;
	}
#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif
	ip = mtod(m, struct ip *);
	ip->ip_dst = mh->mob_dst;
	ip->ip_p = mh->mob_proto;
	ip->ip_sum = 0;
	ip->ip_len = htons(m->m_pkthdr.len - hlen);
	if (mh->mob_flags)
		ip->ip_src = mh->mob_src;
	memmove(mtodo(m, hlen), ip, sizeof(struct ip));
	m_adj(m, hlen);
	m_clrprotoflags(m);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.csum_flags |= (CSUM_IP_CHECKED | CSUM_IP_VALID);
	M_SETFIB(m, ifp->if_fib);
	hlen = AF_INET;
	BPF_MTAP2(ifp, &hlen, sizeof(hlen), m);
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	if ((ifp->if_flags & IFF_MONITOR) != 0)
		m_freem(m);
	else
		netisr_dispatch(NETISR_IP, m);
	return (IPPROTO_DONE);
drop:
	if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
	return (IPPROTO_DONE);
}

#define	MTAG_ME	1414491977
static int
me_check_nesting(struct ifnet *ifp, struct mbuf *m)
{
	struct m_tag *mtag;
	int count;

	count = 1;
	mtag = NULL;
	while ((mtag = m_tag_locate(m, MTAG_ME, 0, mtag)) != NULL) {
		if (*(struct ifnet **)(mtag + 1) == ifp) {
			log(LOG_NOTICE, "%s: loop detected\n", ifp->if_xname);
			return (EIO);
		}
		count++;
	}
	if (count > V_max_me_nesting) {
		log(LOG_NOTICE,
		    "%s: if_output recursively called too many times(%d)\n",
		    ifp->if_xname, count);
		return (EIO);
	}
	mtag = m_tag_alloc(MTAG_ME, 0, sizeof(struct ifnet *), M_NOWAIT);
	if (mtag == NULL)
		return (ENOMEM);
	*(struct ifnet **)(mtag + 1) = ifp;
	m_tag_prepend(m, mtag);
	return (0);
}

static int
me_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
   struct route *ro)
{
	uint32_t af;
	int error;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error != 0)
		goto drop;
#endif
	if ((ifp->if_flags & IFF_MONITOR) != 0 ||
	    (ifp->if_flags & IFF_UP) == 0) {
		error = ENETDOWN;
		goto drop;
	}

	error = me_check_nesting(ifp, m);
	if (error != 0)
		goto drop;

	m->m_flags &= ~(M_BCAST|M_MCAST);
	if (dst->sa_family == AF_UNSPEC)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = dst->sa_family;
	if (af != AF_INET) {
		error = EAFNOSUPPORT;
		goto drop;
	}
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	return (ifp->if_transmit(ifp, m));
drop:
	m_freem(m);
	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	return (error);
}

static int
me_transmit(struct ifnet *ifp, struct mbuf *m)
{
	ME_RLOCK_TRACKER;
	struct mobhdr mh;
	struct me_softc *sc;
	struct ip *ip;
	int error, hlen, plen;

	sc = ifp->if_softc;
	if (sc == NULL) {
		error = ENETDOWN;
		m_freem(m);
		goto drop;
	}
	if (m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	ip = mtod(m, struct ip *);
	/* Fragmented datagramms shouldn't be encapsulated */
	if (ip->ip_off & htons(IP_MF | IP_OFFMASK)) {
		error = EINVAL;
		m_freem(m);
		goto drop;
	}
	mh.mob_proto = ip->ip_p;
	mh.mob_src = ip->ip_src;
	mh.mob_dst = ip->ip_dst;
	ME_RLOCK(sc);
	if (!ME_READY(sc)) {
		ME_RUNLOCK(sc);
		error = ENETDOWN;
		m_freem(m);
		goto drop;
	}
	if (in_hosteq(sc->me_src, ip->ip_src)) {
		hlen = sizeof(struct mobhdr) - sizeof(struct in_addr);
		mh.mob_flags = 0;
	} else {
		hlen = sizeof(struct mobhdr);
		mh.mob_flags = MOB_FLAGS_SP;
	}
	plen = m->m_pkthdr.len;
	ip->ip_src = sc->me_src;
	ip->ip_dst = sc->me_dst;
	M_SETFIB(m, sc->me_fibnum);
	ME_RUNLOCK(sc);
	M_PREPEND(m, hlen, M_NOWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	if (m->m_len < sizeof(struct ip) + hlen)
		m = m_pullup(m, sizeof(struct ip) + hlen);
	if (m == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	memmove(mtod(m, void *), mtodo(m, hlen), sizeof(struct ip));
	ip = mtod(m, struct ip *);
	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_p = IPPROTO_MOBILE;
	ip->ip_sum = 0;
	mh.mob_csum = 0;
	mh.mob_csum = me_in_cksum((uint16_t *)&mh, hlen / sizeof(uint16_t));
	bcopy(&mh, mtodo(m, sizeof(struct ip)), hlen);
	error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL);
drop:
	if (error)
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	else {
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_OBYTES, plen);
	}
	return (error);
}

static void
me_qflush(struct ifnet *ifp __unused)
{

}

static int
memodevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
	case MOD_UNLOAD:
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t me_mod = {
	"if_me",
	memodevent,
	0
};

DECLARE_MODULE(if_me, me_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_me, 1);
