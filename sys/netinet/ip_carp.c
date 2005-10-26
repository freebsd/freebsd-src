/* 	$FreeBSD$ */

/*
 * Copyright (c) 2002 Michael Shalayeff. All rights reserved.
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_carp.h"
#include "opt_bpf.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/signalvar.h>
#include <sys/filio.h>
#include <sys/sockio.h>

#include <sys/socket.h>
#include <sys/vnode.h>

#include <machine/stdarg.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/fddi.h>
#include <net/iso88025.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#include <machine/in_cksum.h>
#endif

#ifdef INET6
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#include <net/if_dl.h>
#endif

#include <crypto/sha1.h>
#include <netinet/ip_carp.h>

#define	CARP_IFNAME	"carp"
static MALLOC_DEFINE(M_CARP, "CARP", "CARP interfaces");
SYSCTL_DECL(_net_inet_carp);

struct carp_softc {
	struct ifnet	 	*sc_ifp;	/* Interface clue */
	struct ifnet		*sc_carpdev;	/* Pointer to parent interface */
	struct in_ifaddr 	*sc_ia;		/* primary iface address */
	struct ip_moptions 	 sc_imo;
#ifdef INET6
	struct in6_ifaddr 	*sc_ia6;	/* primary iface address v6 */
	struct ip6_moptions 	 sc_im6o;
#endif /* INET6 */
	TAILQ_ENTRY(carp_softc)	 sc_list;

	enum { INIT = 0, BACKUP, MASTER }	sc_state;

	int			 sc_flags_backup;
	int			 sc_suppress;

	int			 sc_sendad_errors;
#define	CARP_SENDAD_MAX_ERRORS	3
	int			 sc_sendad_success;
#define	CARP_SENDAD_MIN_SUCCESS 3

	int			 sc_vhid;
	int			 sc_advskew;
	int			 sc_naddrs;
	int			 sc_naddrs6;
	int			 sc_advbase;	/* seconds */
	int			 sc_init_counter;
	u_int64_t		 sc_counter;

	/* authentication */
#define CARP_HMAC_PAD	64
	unsigned char sc_key[CARP_KEY_LEN];
	unsigned char sc_pad[CARP_HMAC_PAD];
	SHA1_CTX sc_sha1;

	struct callout		 sc_ad_tmo;	/* advertisement timeout */
	struct callout		 sc_md_tmo;	/* master down timeout */
	struct callout 		 sc_md6_tmo;	/* master down timeout */
	
	LIST_ENTRY(carp_softc)	 sc_next;	/* Interface clue */
};
#define	SC2IFP(sc)	((sc)->sc_ifp)

int carp_suppress_preempt = 0;
int carp_opts[CARPCTL_MAXID] = { 0, 1, 0, 1, 0, 0 };	/* XXX for now */
SYSCTL_INT(_net_inet_carp, CARPCTL_ALLOW, allow, CTLFLAG_RW,
    &carp_opts[CARPCTL_ALLOW], 0, "Accept incoming CARP packets");
SYSCTL_INT(_net_inet_carp, CARPCTL_PREEMPT, preempt, CTLFLAG_RW,
    &carp_opts[CARPCTL_PREEMPT], 0, "high-priority backup preemption mode");
SYSCTL_INT(_net_inet_carp, CARPCTL_LOG, log, CTLFLAG_RW,
    &carp_opts[CARPCTL_LOG], 0, "log bad carp packets");
SYSCTL_INT(_net_inet_carp, CARPCTL_ARPBALANCE, arpbalance, CTLFLAG_RW,
    &carp_opts[CARPCTL_ARPBALANCE], 0, "balance arp responses");
SYSCTL_INT(_net_inet_carp, OID_AUTO, suppress_preempt, CTLFLAG_RD,
    &carp_suppress_preempt, 0, "Preemption is suppressed");

struct carpstats carpstats;
SYSCTL_STRUCT(_net_inet_carp, CARPCTL_STATS, stats, CTLFLAG_RW,
    &carpstats, carpstats,
    "CARP statistics (struct carpstats, netinet/ip_carp.h)");

struct carp_if {
	TAILQ_HEAD(, carp_softc) vhif_vrs;
	int vhif_nvrs;

	struct ifnet 	*vhif_ifp;
	struct mtx	 vhif_mtx;
};

/* Get carp_if from softc. Valid after carp_set_addr{,6}. */
#define	SC2CIF(sc)		((struct carp_if *)(sc)->sc_carpdev->if_carp)

/* lock per carp_if queue */
#define	CARP_LOCK_INIT(cif)	mtx_init(&(cif)->vhif_mtx, "carp_if", 	\
	NULL, MTX_DEF)
#define	CARP_LOCK_DESTROY(cif)	mtx_destroy(&(cif)->vhif_mtx)
#define	CARP_LOCK_ASSERT(cif)	mtx_assert(&(cif)->vhif_mtx, MA_OWNED)
#define	CARP_LOCK(cif)		mtx_lock(&(cif)->vhif_mtx)
#define	CARP_UNLOCK(cif)	mtx_unlock(&(cif)->vhif_mtx)

#define	CARP_SCLOCK(sc)		mtx_lock(&SC2CIF(sc)->vhif_mtx)
#define	CARP_SCUNLOCK(sc)	mtx_unlock(&SC2CIF(sc)->vhif_mtx)
#define	CARP_SCLOCK_ASSERT(sc)	mtx_assert(&SC2CIF(sc)->vhif_mtx, MA_OWNED)

#define	CARP_LOG(...)	do {				\
	if (carp_opts[CARPCTL_LOG] > 0)			\
		log(LOG_INFO, __VA_ARGS__);		\
} while (0)

#define	CARP_DEBUG(...)	do {				\
	if (carp_opts[CARPCTL_LOG] > 1)			\
		log(LOG_DEBUG, __VA_ARGS__);		\
} while (0)

static void	carp_hmac_prepare(struct carp_softc *);
static void	carp_hmac_generate(struct carp_softc *, u_int32_t *,
		    unsigned char *);
static int	carp_hmac_verify(struct carp_softc *, u_int32_t *,
		    unsigned char *);
static void	carp_setroute(struct carp_softc *, int);
static void	carp_input_c(struct mbuf *, struct carp_header *, sa_family_t);
static int 	carp_clone_create(struct if_clone *, int);
static void 	carp_clone_destroy(struct ifnet *);
static void	carpdetach(struct carp_softc *);
static int	carp_prepare_ad(struct mbuf *, struct carp_softc *,
		    struct carp_header *);
static void	carp_send_ad_all(void);
static void	carp_send_ad(void *);
static void	carp_send_ad_locked(struct carp_softc *);
static void	carp_send_arp(struct carp_softc *);
static void	carp_master_down(void *);
static void	carp_master_down_locked(struct carp_softc *);
static int	carp_ioctl(struct ifnet *, u_long, caddr_t);
static int	carp_looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static void	carp_start(struct ifnet *);
static void	carp_setrun(struct carp_softc *, sa_family_t);
static void	carp_set_state(struct carp_softc *, int);
static int	carp_addrcount(struct carp_if *, struct in_ifaddr *, int);
enum	{ CARP_COUNT_MASTER, CARP_COUNT_RUNNING };

static int	carp_set_addr(struct carp_softc *, struct sockaddr_in *);
static int	carp_del_addr(struct carp_softc *, struct sockaddr_in *);
static void	carp_carpdev_state_locked(struct carp_if *);
static void	carp_sc_state_locked(struct carp_softc *);
#ifdef INET6
static void	carp_send_na(struct carp_softc *);
static int	carp_set_addr6(struct carp_softc *, struct sockaddr_in6 *);
static int	carp_del_addr6(struct carp_softc *, struct sockaddr_in6 *);
#endif

static LIST_HEAD(, carp_softc) carpif_list;
static struct mtx carp_mtx;
IFC_SIMPLE_DECLARE(carp, 0);

static __inline u_int16_t
carp_cksum(struct mbuf *m, int len)
{
	return (in_cksum(m, len));
}

static void
carp_hmac_prepare(struct carp_softc *sc)
{
	u_int8_t version = CARP_VERSION, type = CARP_ADVERTISEMENT;
	u_int8_t vhid = sc->sc_vhid & 0xff;
	struct ifaddr *ifa;
	int i;
#ifdef INET6
	struct in6_addr in6;
#endif

	if (sc->sc_carpdev)
		CARP_SCLOCK(sc);

	/* XXX: possible race here */

	/* compute ipad from key */
	bzero(sc->sc_pad, sizeof(sc->sc_pad));
	bcopy(sc->sc_key, sc->sc_pad, sizeof(sc->sc_key));
	for (i = 0; i < sizeof(sc->sc_pad); i++)
		sc->sc_pad[i] ^= 0x36;

	/* precompute first part of inner hash */
	SHA1Init(&sc->sc_sha1);
	SHA1Update(&sc->sc_sha1, sc->sc_pad, sizeof(sc->sc_pad));
	SHA1Update(&sc->sc_sha1, (void *)&version, sizeof(version));
	SHA1Update(&sc->sc_sha1, (void *)&type, sizeof(type));
	SHA1Update(&sc->sc_sha1, (void *)&vhid, sizeof(vhid));
#ifdef INET
	TAILQ_FOREACH(ifa, &SC2IFP(sc)->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET)
			SHA1Update(&sc->sc_sha1,
			    (void *)&ifatoia(ifa)->ia_addr.sin_addr.s_addr,
			    sizeof(struct in_addr));
	}
#endif /* INET */
#ifdef INET6
	TAILQ_FOREACH(ifa, &SC2IFP(sc)->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			in6 = ifatoia6(ifa)->ia_addr.sin6_addr;
			in6_clearscope(&in6);
			SHA1Update(&sc->sc_sha1, (void *)&in6, sizeof(in6));
		}
	}
#endif /* INET6 */

	/* convert ipad to opad */
	for (i = 0; i < sizeof(sc->sc_pad); i++)
		sc->sc_pad[i] ^= 0x36 ^ 0x5c;

	if (sc->sc_carpdev)
		CARP_SCUNLOCK(sc);
}

static void
carp_hmac_generate(struct carp_softc *sc, u_int32_t counter[2],
    unsigned char md[20])
{
	SHA1_CTX sha1ctx;

	/* fetch first half of inner hash */
	bcopy(&sc->sc_sha1, &sha1ctx, sizeof(sha1ctx));

	SHA1Update(&sha1ctx, (void *)counter, sizeof(sc->sc_counter));
	SHA1Final(md, &sha1ctx);

	/* outer hash */
	SHA1Init(&sha1ctx);
	SHA1Update(&sha1ctx, sc->sc_pad, sizeof(sc->sc_pad));
	SHA1Update(&sha1ctx, md, 20);
	SHA1Final(md, &sha1ctx);
}

static int
carp_hmac_verify(struct carp_softc *sc, u_int32_t counter[2],
    unsigned char md[20])
{
	unsigned char md2[20];

	CARP_SCLOCK_ASSERT(sc);

	carp_hmac_generate(sc, counter, md2);

	return (bcmp(md, md2, sizeof(md2)));
}

static void
carp_setroute(struct carp_softc *sc, int cmd)
{
	struct ifaddr *ifa;
	int s;

	if (sc->sc_carpdev)
		CARP_SCLOCK_ASSERT(sc);

	s = splnet();
	TAILQ_FOREACH(ifa, &SC2IFP(sc)->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    sc->sc_carpdev != NULL) {
			int count = carp_addrcount(
			    (struct carp_if *)sc->sc_carpdev->if_carp,
			    ifatoia(ifa), CARP_COUNT_MASTER);

			if ((cmd == RTM_ADD && count == 1) ||
			    (cmd == RTM_DELETE && count == 0))
				rtinit(ifa, cmd, RTF_UP | RTF_HOST);
		}
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			if (cmd == RTM_ADD)
				in6_ifaddloop(ifa);
			else
				in6_ifremloop(ifa);
		}
#endif /* INET6 */
	}
	splx(s);
}

static int
carp_clone_create(struct if_clone *ifc, int unit)
{

	struct carp_softc *sc;
	struct ifnet *ifp;

	MALLOC(sc, struct carp_softc *, sizeof(*sc), M_CARP, M_WAITOK|M_ZERO);
	ifp = SC2IFP(sc) = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		FREE(sc, M_CARP);
		return (ENOSPC);
	}
	
	sc->sc_flags_backup = 0;
	sc->sc_suppress = 0;
	sc->sc_advbase = CARP_DFLTINTV;
	sc->sc_vhid = -1;	/* required setting */
	sc->sc_advskew = 0;
	sc->sc_init_counter = 1;
	sc->sc_naddrs = sc->sc_naddrs6 = 0; /* M_ZERO? */
#ifdef INET6
	sc->sc_im6o.im6o_multicast_hlim = CARP_DFLTTL;
#endif

	callout_init(&sc->sc_ad_tmo, NET_CALLOUT_MPSAFE);
	callout_init(&sc->sc_md_tmo, NET_CALLOUT_MPSAFE);
	callout_init(&sc->sc_md6_tmo, NET_CALLOUT_MPSAFE);
	
	ifp->if_softc = sc;
	if_initname(ifp, CARP_IFNAME, unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_LOOPBACK;
	ifp->if_ioctl = carp_ioctl;
	ifp->if_output = carp_looutput;
	ifp->if_start = carp_start;
	ifp->if_type = IFT_CARP;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = 0;
	if_attach(ifp);
	bpfattach(SC2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	mtx_lock(&carp_mtx);
	LIST_INSERT_HEAD(&carpif_list, sc, sc_next);
	mtx_unlock(&carp_mtx);
	return (0);
}

static void
carp_clone_destroy(struct ifnet *ifp)
{
	struct carp_softc *sc = ifp->if_softc;
	struct carp_if *cif;
	struct ip_moptions *imo = &sc->sc_imo;
#ifdef INET6
	struct ip6_moptions *im6o = &sc->sc_im6o;
#endif
	
/*	carpdetach(sc); */

	/*
	 * If an interface is destroyed which is suppressing the preemption,
	 * decrease the global counter, otherwise the host will never get
	 * out of the carp supressing state.
	 */
	if (sc->sc_suppress)
		carp_suppress_preempt--;
	sc->sc_suppress = 0;

	callout_stop(&sc->sc_ad_tmo);
	callout_stop(&sc->sc_md_tmo);
	callout_stop(&sc->sc_md6_tmo);

	if (imo->imo_num_memberships) {
		in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
		imo->imo_multicast_ifp = NULL;
	}
#ifdef INET6
	while (!LIST_EMPTY(&im6o->im6o_memberships)) {
		struct in6_multi_mship *imm =
		    LIST_FIRST(&im6o->im6o_memberships);
		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
	}
	im6o->im6o_multicast_ifp = NULL;
#endif

	/* Remove ourself from parents if_carp queue */
	if (sc->sc_carpdev && (cif = sc->sc_carpdev->if_carp)) {
		CARP_LOCK(cif);
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			sc->sc_carpdev->if_carp = NULL;
			CARP_LOCK_DESTROY(cif);
			FREE(cif, M_CARP);
			ifpromisc(sc->sc_carpdev, 0);
			sc->sc_carpdev = NULL;
		} else {
			CARP_UNLOCK(cif);
		}
	}

	mtx_lock(&carp_mtx);
	LIST_REMOVE(sc, sc_next);
	mtx_unlock(&carp_mtx);
	bpfdetach(ifp);
	if_detach(ifp);
	if_free_type(ifp, IFT_ETHER);
	free(sc, M_CARP);
}

/*
 * process input packet.
 * we have rearranged checks order compared to the rfc,
 * but it seems more efficient this way or not possible otherwise.
 */
void
carp_input(struct mbuf *m, int hlen)
{
	struct ip *ip = mtod(m, struct ip *);
	struct carp_header *ch;
	int iplen, len;

	carpstats.carps_ipackets++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return;
	}

	/* check if received on a valid carp interface */
	if (m->m_pkthdr.rcvif->if_carp == NULL) {
		carpstats.carps_badif++;
		CARP_LOG("carp_input: packet received on non-carp "
		    "interface: %s\n",
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return;
	}

	/* verify that the IP TTL is 255.  */
	if (ip->ip_ttl != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG("carp_input: received ttl %d != 255i on %s\n",
		    ip->ip_ttl,
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return;
	}

	iplen = ip->ip_hl << 2;

	if (m->m_pkthdr.len < iplen + sizeof(*ch)) {
		carpstats.carps_badlen++;
		CARP_LOG("carp_input: received len %zd < "
		    "sizeof(struct carp_header)\n",
		    m->m_len - sizeof(struct ip));
		m_freem(m);
		return;
	}

	if (iplen + sizeof(*ch) < m->m_len) {
		if ((m = m_pullup(m, iplen + sizeof(*ch))) == NULL) {
			carpstats.carps_hdrops++;
			CARP_LOG("carp_input: pullup failed\n");
			return;
		}
		ip = mtod(m, struct ip *);
	}
	ch = (struct carp_header *)((char *)ip + iplen);

	/*
	 * verify that the received packet length is
	 * equal to the CARP header
	 */
	len = iplen + sizeof(*ch);
	if (len > m->m_pkthdr.len) {
		carpstats.carps_badlen++;
		CARP_LOG("carp_input: packet too short %d on %s\n",
		    m->m_pkthdr.len,
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return;
	}

	if ((m = m_pullup(m, len)) == NULL) {
		carpstats.carps_hdrops++;
		return;
	}
	ip = mtod(m, struct ip *);
	ch = (struct carp_header *)((char *)ip + iplen);

	/* verify the CARP checksum */
	m->m_data += iplen;
	if (carp_cksum(m, len - iplen)) {
		carpstats.carps_badsum++;
		CARP_LOG("carp_input: checksum failed on %s\n",
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return;
	}
	m->m_data -= iplen;

	carp_input_c(m, ch, AF_INET);
}

#ifdef INET6
int
carp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct carp_header *ch;
	u_int len;

	carpstats.carps_ipackets6++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* check if received on a valid carp interface */
	if (m->m_pkthdr.rcvif->if_carp == NULL) {
		carpstats.carps_badif++;
		CARP_LOG("carp6_input: packet received on non-carp "
		    "interface: %s\n",
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that the IP TTL is 255 */
	if (ip6->ip6_hlim != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG("carp6_input: received ttl %d != 255 on %s\n",
		    ip6->ip6_hlim,
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that we have a complete carp packet */
	len = m->m_len;
	IP6_EXTHDR_GET(ch, struct carp_header *, m, *offp, sizeof(*ch));
	if (ch == NULL) {
		carpstats.carps_badlen++;
		CARP_LOG("carp6_input: packet size %u too small\n", len);
		return (IPPROTO_DONE);
	}


	/* verify the CARP checksum */
	m->m_data += *offp;
	if (carp_cksum(m, sizeof(*ch))) {
		carpstats.carps_badsum++;
		CARP_LOG("carp6_input: checksum failed, on %s\n",
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return (IPPROTO_DONE);
	}
	m->m_data -= *offp;

	carp_input_c(m, ch, AF_INET6);
	return (IPPROTO_DONE);
}
#endif /* INET6 */

static void
carp_input_c(struct mbuf *m, struct carp_header *ch, sa_family_t af)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct carp_softc *sc;
	u_int64_t tmp_counter;
	struct timeval sc_tv, ch_tv;

	/* verify that the VHID is valid on the receiving interface */
	CARP_LOCK(ifp->if_carp);
	TAILQ_FOREACH(sc, &((struct carp_if *)ifp->if_carp)->vhif_vrs, sc_list)
		if (sc->sc_vhid == ch->carp_vhid)
			break;

	if (!sc || !((SC2IFP(sc)->if_flags & IFF_UP) &&
	    (SC2IFP(sc)->if_drv_flags & IFF_DRV_RUNNING))) {
		carpstats.carps_badvhid++;
		CARP_UNLOCK(ifp->if_carp);
		m_freem(m);
		return;
	}

	getmicrotime(&SC2IFP(sc)->if_lastchange);
	SC2IFP(sc)->if_ipackets++;
	SC2IFP(sc)->if_ibytes += m->m_pkthdr.len;

	if (SC2IFP(sc)->if_bpf) {
		struct ip *ip = mtod(m, struct ip *);
		uint32_t af1 = af;

		/* BPF wants net byte order */
		ip->ip_len = htons(ip->ip_len + (ip->ip_hl << 2));
		ip->ip_off = htons(ip->ip_off);
		bpf_mtap2(SC2IFP(sc)->if_bpf, &af1, sizeof(af1), m);
	}

	/* verify the CARP version. */
	if (ch->carp_version != CARP_VERSION) {
		carpstats.carps_badver++;
		SC2IFP(sc)->if_ierrors++;
		CARP_UNLOCK(ifp->if_carp);
		CARP_LOG("%s; invalid version %d\n",
		    SC2IFP(sc)->if_xname,
		    ch->carp_version);
		m_freem(m);
		return;
	}

	/* verify the hash */
	if (carp_hmac_verify(sc, ch->carp_counter, ch->carp_md)) {
		carpstats.carps_badauth++;
		SC2IFP(sc)->if_ierrors++;
		CARP_UNLOCK(ifp->if_carp);
		CARP_LOG("%s: incorrect hash\n", SC2IFP(sc)->if_xname);
		m_freem(m);
		return;
	}

	tmp_counter = ntohl(ch->carp_counter[0]);
	tmp_counter = tmp_counter<<32;
	tmp_counter += ntohl(ch->carp_counter[1]);

	/* XXX Replay protection goes here */

	sc->sc_init_counter = 0;
	sc->sc_counter = tmp_counter;

	sc_tv.tv_sec = sc->sc_advbase;
	if (carp_suppress_preempt && sc->sc_advskew <  240)
		sc_tv.tv_usec = 240 * 1000000 / 256;
	else
		sc_tv.tv_usec = sc->sc_advskew * 1000000 / 256;
	ch_tv.tv_sec = ch->carp_advbase;
	ch_tv.tv_usec = ch->carp_advskew * 1000000 / 256;

	switch (sc->sc_state) {
	case INIT:
		break;
	case MASTER:
		/*
		 * If we receive an advertisement from a master who's going to
		 * be more frequent than us, go into BACKUP state.
		 */
		if (timevalcmp(&sc_tv, &ch_tv, >) ||
		    timevalcmp(&sc_tv, &ch_tv, ==)) {
			callout_stop(&sc->sc_ad_tmo);
			CARP_DEBUG("%s: MASTER -> BACKUP "
			   "(more frequent advertisement received)\n",
			   SC2IFP(sc)->if_xname);
			carp_set_state(sc, BACKUP);
			carp_setrun(sc, 0);
			carp_setroute(sc, RTM_DELETE);
		}
		break;
	case BACKUP:
		/*
		 * If we're pre-empting masters who advertise slower than us,
		 * and this one claims to be slower, treat him as down.
		 */
		if (carp_opts[CARPCTL_PREEMPT] &&
		    timevalcmp(&sc_tv, &ch_tv, <)) {
			CARP_DEBUG("%s: BACKUP -> MASTER "
			    "(preempting a slower master)\n",
			    SC2IFP(sc)->if_xname);
			carp_master_down_locked(sc);
			break;
		}

		/*
		 *  If the master is going to advertise at such a low frequency
		 *  that he's guaranteed to time out, we'd might as well just
		 *  treat him as timed out now.
		 */
		sc_tv.tv_sec = sc->sc_advbase * 3;
		if (timevalcmp(&sc_tv, &ch_tv, <)) {
			CARP_DEBUG("%s: BACKUP -> MASTER "
			    "(master timed out)\n",
			    SC2IFP(sc)->if_xname);
			carp_master_down_locked(sc);
			break;
		}

		/*
		 * Otherwise, we reset the counter and wait for the next
		 * advertisement.
		 */
		carp_setrun(sc, af);
		break;
	}

	CARP_UNLOCK(ifp->if_carp);

	m_freem(m);
	return;
}

static void
carpdetach(struct carp_softc *sc)
{
	struct ifaddr *ifa;

	callout_stop(&sc->sc_ad_tmo);
	callout_stop(&sc->sc_md_tmo);
	callout_stop(&sc->sc_md6_tmo);

	while ((ifa = TAILQ_FIRST(&SC2IFP(sc)->if_addrlist)) != NULL)
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct in_ifaddr *ia = ifatoia(ifa);

			carp_del_addr(sc, &ia->ia_addr);

			/* ripped screaming from in_control(SIOCDIFADDR) */
			in_ifscrub(SC2IFP(sc), ia);
			TAILQ_REMOVE(&SC2IFP(sc)->if_addrlist, ifa, ifa_link);
			TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
			IFAFREE((&ia->ia_ifa));
		}
}

/* Detach an interface from the carp.  */
void
carp_ifdetach(struct ifnet *ifp)
{
	struct carp_softc *sc;
	struct carp_if *cif = (struct carp_if *)ifp->if_carp;

	CARP_LOCK(cif);
	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list)
		carpdetach(sc);
	CARP_UNLOCK(cif);
}

static int
carp_prepare_ad(struct mbuf *m, struct carp_softc *sc, struct carp_header *ch)
{
	struct m_tag *mtag;
	struct ifnet *ifp = SC2IFP(sc);

	if (sc->sc_init_counter) {
		/* this could also be seconds since unix epoch */
		sc->sc_counter = arc4random();
		sc->sc_counter = sc->sc_counter << 32;
		sc->sc_counter += arc4random();
	} else
		sc->sc_counter++;

	ch->carp_counter[0] = htonl((sc->sc_counter>>32)&0xffffffff);
	ch->carp_counter[1] = htonl(sc->sc_counter&0xffffffff);

	carp_hmac_generate(sc, ch->carp_counter, ch->carp_md);

	/* Tag packet for carp_output */
	mtag = m_tag_get(PACKET_TAG_CARP, sizeof(struct ifnet *), M_NOWAIT);
	if (mtag == NULL) {
		m_freem(m);
		SC2IFP(sc)->if_oerrors++;
		return (ENOMEM);
	}
	bcopy(&ifp, (caddr_t)(mtag + 1), sizeof(struct ifnet *));
	m_tag_prepend(m, mtag);

	return (0);
}

static void
carp_send_ad_all(void)
{
	struct carp_softc *sc;

	mtx_lock(&carp_mtx);
	LIST_FOREACH(sc, &carpif_list, sc_next) {
		if (sc->sc_carpdev == NULL)
			continue;
		CARP_SCLOCK(sc);
		if ((SC2IFP(sc)->if_flags & IFF_UP) &&
		    (SC2IFP(sc)->if_drv_flags & IFF_DRV_RUNNING) &&
		     sc->sc_state == MASTER)
			carp_send_ad_locked(sc);
		CARP_SCUNLOCK(sc);
	}
	mtx_unlock(&carp_mtx);
}

static void
carp_send_ad(void *v)
{
	struct carp_softc *sc = v;

	CARP_SCLOCK(sc);
	carp_send_ad_locked(sc);
	CARP_SCUNLOCK(sc);
}

static void
carp_send_ad_locked(struct carp_softc *sc)
{
	struct carp_header ch;
	struct timeval tv;
	struct carp_header *ch_ptr;
	struct mbuf *m;
	int len, advbase, advskew;

	CARP_SCLOCK_ASSERT(sc);

	/* bow out if we've lost our UPness or RUNNINGuiness */
	if (!((SC2IFP(sc)->if_flags & IFF_UP) &&
	    (SC2IFP(sc)->if_drv_flags & IFF_DRV_RUNNING))) {
		advbase = 255;
		advskew = 255;
	} else {
		advbase = sc->sc_advbase;
		if (!carp_suppress_preempt || sc->sc_advskew > 240)
			advskew = sc->sc_advskew;
		else
			advskew = 240;
		tv.tv_sec = advbase;
		tv.tv_usec = advskew * 1000000 / 256;
	}

	ch.carp_version = CARP_VERSION;
	ch.carp_type = CARP_ADVERTISEMENT;
	ch.carp_vhid = sc->sc_vhid;
	ch.carp_advbase = advbase;
	ch.carp_advskew = advskew;
	ch.carp_authlen = 7;	/* XXX DEFINE */
	ch.carp_pad1 = 0;	/* must be zero */
	ch.carp_cksum = 0;

#ifdef INET
	if (sc->sc_ia) {
		struct ip *ip;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			SC2IFP(sc)->if_oerrors++;
			carpstats.carps_onomem++;
			/* XXX maybe less ? */
			if (advbase != 255 || advskew != 255)
				callout_reset(&sc->sc_ad_tmo, tvtohz(&tv),
				    carp_send_ad, sc);
			return;
		}
		len = sizeof(*ip) + sizeof(ch);
		m->m_pkthdr.len = len;
		m->m_pkthdr.rcvif = NULL;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
		m->m_flags |= M_MCAST;
		ip = mtod(m, struct ip *);
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		ip->ip_len = len;
		ip->ip_id = ip_newid();
		ip->ip_off = IP_DF;
		ip->ip_ttl = CARP_DFLTTL;
		ip->ip_p = IPPROTO_CARP;
		ip->ip_sum = 0;
		ip->ip_src.s_addr = sc->sc_ia->ia_addr.sin_addr.s_addr;
		ip->ip_dst.s_addr = htonl(INADDR_CARP_GROUP);

		ch_ptr = (struct carp_header *)(&ip[1]);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, sc, ch_ptr))
			return;

		m->m_data += sizeof(*ip);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip));
		m->m_data -= sizeof(*ip);

		getmicrotime(&SC2IFP(sc)->if_lastchange);
		SC2IFP(sc)->if_opackets++;
		SC2IFP(sc)->if_obytes += len;
		carpstats.carps_opackets++;

		if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL)) {
			SC2IFP(sc)->if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS) {
				carp_suppress_preempt++;
				if (carp_suppress_preempt == 1) {
					CARP_SCUNLOCK(sc);
					carp_send_ad_all();
					CARP_SCLOCK(sc);
				}
			}
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS) {
					carp_suppress_preempt--;
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
	}
#endif /* INET */
#ifdef INET6
	if (sc->sc_ia6) {
		struct ip6_hdr *ip6;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			SC2IFP(sc)->if_oerrors++;
			carpstats.carps_onomem++;
			/* XXX maybe less ? */
			if (advbase != 255 || advskew != 255)
				callout_reset(&sc->sc_ad_tmo, tvtohz(&tv),
				    carp_send_ad, sc);
			return;
		}
		len = sizeof(*ip6) + sizeof(ch);
		m->m_pkthdr.len = len;
		m->m_pkthdr.rcvif = NULL;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
		m->m_flags |= M_MCAST;
		ip6 = mtod(m, struct ip6_hdr *);
		bzero(ip6, sizeof(*ip6));
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_hlim = CARP_DFLTTL;
		ip6->ip6_nxt = IPPROTO_CARP;
		bcopy(&sc->sc_ia6->ia_addr.sin6_addr, &ip6->ip6_src,
		    sizeof(struct in6_addr));
		/* set the multicast destination */

		ip6->ip6_dst.s6_addr8[0] = 0xff;
		ip6->ip6_dst.s6_addr8[1] = 0x02;
		ip6->ip6_dst.s6_addr8[15] = 0x12;

		ch_ptr = (struct carp_header *)(&ip6[1]);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, sc, ch_ptr))
			return;

		m->m_data += sizeof(*ip6);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip6));
		m->m_data -= sizeof(*ip6);

		getmicrotime(&SC2IFP(sc)->if_lastchange);
		SC2IFP(sc)->if_opackets++;
		SC2IFP(sc)->if_obytes += len;
		carpstats.carps_opackets6++;

		if (ip6_output(m, NULL, NULL, 0, &sc->sc_im6o, NULL, NULL)) {
			SC2IFP(sc)->if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS) {
				carp_suppress_preempt++;
				if (carp_suppress_preempt == 1) {
					CARP_SCUNLOCK(sc);
					carp_send_ad_all();
					CARP_SCLOCK(sc);
				}
			}
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS) {
					carp_suppress_preempt--;
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
	}
#endif /* INET6 */

	if (advbase != 255 || advskew != 255)
		callout_reset(&sc->sc_ad_tmo, tvtohz(&tv),
		    carp_send_ad, sc);

}

/*
 * Broadcast a gratuitous ARP request containing
 * the virtual router MAC address for each IP address
 * associated with the virtual router.
 */
static void
carp_send_arp(struct carp_softc *sc)
{
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifa, &SC2IFP(sc)->if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

/*		arprequest(sc->sc_carpdev, &in, &in, IFP2ENADDR(sc->sc_ifp)); */
		arp_ifinit2(sc->sc_carpdev, ifa, IFP2ENADDR(sc->sc_ifp));

		DELAY(1000);	/* XXX */
	}
}

#ifdef INET6
static void
carp_send_na(struct carp_softc *sc)
{
	struct ifaddr *ifa;
	struct in6_addr *in6;
	static struct in6_addr mcast = IN6ADDR_LINKLOCAL_ALLNODES_INIT;

	TAILQ_FOREACH(ifa, &SC2IFP(sc)->if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		in6 = &ifatoia6(ifa)->ia_addr.sin6_addr;
		nd6_na_output(sc->sc_carpdev, &mcast, in6,
		    ND_NA_FLAG_OVERRIDE, 1, NULL);
		DELAY(1000);	/* XXX */
	}
}
#endif /* INET6 */

static int
carp_addrcount(struct carp_if *cif, struct in_ifaddr *ia, int type)
{
	struct carp_softc *vh;
	struct ifaddr *ifa;
	int count = 0;

	CARP_LOCK_ASSERT(cif);

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		if ((type == CARP_COUNT_RUNNING &&
		    (SC2IFP(vh)->if_flags & IFF_UP) &&
		    (SC2IFP(vh)->if_drv_flags & IFF_DRV_RUNNING)) ||
		    (type == CARP_COUNT_MASTER && vh->sc_state == MASTER)) {
			TAILQ_FOREACH(ifa, &SC2IFP(vh)->if_addrlist,
			    ifa_list) {
				if (ifa->ifa_addr->sa_family == AF_INET &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr)
					count++;
			}
		}
	}
	return (count);
}

int
carp_iamatch(void *v, struct in_ifaddr *ia,
    struct in_addr *isaddr, u_int8_t **enaddr)
{
	struct carp_if *cif = v;
	struct carp_softc *vh;
	int index, count = 0;
	struct ifaddr *ifa;

	CARP_LOCK(cif);

	if (carp_opts[CARPCTL_ARPBALANCE]) {
		/*
		 * XXX proof of concept implementation.
		 * We use the source ip to decide which virtual host should
		 * handle the request. If we're master of that virtual host,
		 * then we respond, otherwise, just drop the arp packet on
		 * the floor.
		 */
		count = carp_addrcount(cif, ia, CARP_COUNT_RUNNING);
		if (count == 0) {
			/* should never reach this */
			CARP_UNLOCK(cif);
			return (0);
		}

		/* this should be a hash, like pf_hash() */
		index = ntohl(isaddr->s_addr) % count;
		count = 0;

		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((SC2IFP(vh)->if_flags & IFF_UP) &&
			    (SC2IFP(vh)->if_drv_flags & IFF_DRV_RUNNING)) {
				TAILQ_FOREACH(ifa, &SC2IFP(vh)->if_addrlist,
				    ifa_list) {
					if (ifa->ifa_addr->sa_family ==
					    AF_INET &&
					    ia->ia_addr.sin_addr.s_addr ==
					    ifatoia(ifa)->ia_addr.sin_addr.s_addr) {
						if (count == index) {
							if (vh->sc_state ==
							    MASTER) {
								*enaddr = IFP2ENADDR(vh->sc_ifp);
								CARP_UNLOCK(cif);
								return (1);
							} else {
								CARP_UNLOCK(cif);
								return (0);
							}
						}
						count++;
					}
				}
			}
		}
	} else {
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((SC2IFP(vh)->if_flags & IFF_UP) &&
			    (SC2IFP(vh)->if_drv_flags & IFF_DRV_RUNNING) &&
			    ia->ia_ifp == SC2IFP(vh)) {
				*enaddr = IFP2ENADDR(vh->sc_ifp);
				CARP_UNLOCK(cif);
				return (1);
			}
		}
	}
	CARP_UNLOCK(cif);
	return (0);
}

#ifdef INET6
struct ifaddr *
carp_iamatch6(void *v, struct in6_addr *taddr)
{
	struct carp_if *cif = v;
	struct carp_softc *vh;
	struct ifaddr *ifa;

	CARP_LOCK(cif);
	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		TAILQ_FOREACH(ifa, &SC2IFP(vh)->if_addrlist, ifa_list) {
			if (IN6_ARE_ADDR_EQUAL(taddr,
			    &ifatoia6(ifa)->ia_addr.sin6_addr) &&
 			    (SC2IFP(vh)->if_flags & IFF_UP) &&
			    (SC2IFP(vh)->if_drv_flags & IFF_DRV_RUNNING)) {
			    	CARP_UNLOCK(cif);
				return (ifa);
			}
		}
	}
	CARP_UNLOCK(cif);
	
	return (NULL);
}

void *
carp_macmatch6(void *v, struct mbuf *m, const struct in6_addr *taddr)
{
	struct m_tag *mtag;
	struct carp_if *cif = v;
	struct carp_softc *sc;
	struct ifaddr *ifa;

	CARP_LOCK(cif);
	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list) {
		TAILQ_FOREACH(ifa, &SC2IFP(sc)->if_addrlist, ifa_list) {
			if (IN6_ARE_ADDR_EQUAL(taddr,
			    &ifatoia6(ifa)->ia_addr.sin6_addr) &&
 			    (SC2IFP(sc)->if_flags & IFF_UP) &&
			    (SC2IFP(sc)->if_drv_flags & IFF_DRV_RUNNING)) {
				struct ifnet *ifp = SC2IFP(sc);
				mtag = m_tag_get(PACKET_TAG_CARP,
				    sizeof(struct ifnet *), M_NOWAIT);
				if (mtag == NULL) {
					/* better a bit than nothing */
					CARP_UNLOCK(cif);
					return (IFP2ENADDR(sc->sc_ifp));
				}
				bcopy(&ifp, (caddr_t)(mtag + 1),
				    sizeof(struct ifnet *));
				m_tag_prepend(m, mtag);

				CARP_UNLOCK(cif);
				return (IFP2ENADDR(sc->sc_ifp));
			}
		}
	}
	CARP_UNLOCK(cif);

	return (NULL);
}
#endif

struct ifnet *
carp_forus(void *v, void *dhost)
{
	struct carp_if *cif = v;
	struct carp_softc *vh;
	u_int8_t *ena = dhost;

	if (ena[0] || ena[1] || ena[2] != 0x5e || ena[3] || ena[4] != 1)
		return (NULL);

	CARP_LOCK(cif);
	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list)
		if ((SC2IFP(vh)->if_flags & IFF_UP) &&
		    (SC2IFP(vh)->if_drv_flags & IFF_DRV_RUNNING) &&
		    vh->sc_state == MASTER &&
		    !bcmp(dhost, IFP2ENADDR(vh->sc_ifp), ETHER_ADDR_LEN)) {
		    	CARP_UNLOCK(cif);
			return (SC2IFP(vh));
		}

    	CARP_UNLOCK(cif);
	return (NULL);
}

static void
carp_master_down(void *v)
{
	struct carp_softc *sc = v;

	CARP_SCLOCK(sc);
	carp_master_down_locked(sc);
	CARP_SCUNLOCK(sc);
}

static void
carp_master_down_locked(struct carp_softc *sc)
{
	if (sc->sc_carpdev)
		CARP_SCLOCK_ASSERT(sc);

	switch (sc->sc_state) {
	case INIT:
		printf("%s: master_down event in INIT state\n",
		    SC2IFP(sc)->if_xname);
		break;
	case MASTER:
		break;
	case BACKUP:
		carp_set_state(sc, MASTER);
		carp_send_ad_locked(sc);
		carp_send_arp(sc);
#ifdef INET6
		carp_send_na(sc);
#endif /* INET6 */
		carp_setrun(sc, 0);
		carp_setroute(sc, RTM_ADD);
		break;
	}
}

/*
 * When in backup state, af indicates whether to reset the master down timer
 * for v4 or v6. If it's set to zero, reset the ones which are already pending.
 */
static void
carp_setrun(struct carp_softc *sc, sa_family_t af)
{
	struct timeval tv;

	if (sc->sc_carpdev)
		CARP_SCLOCK_ASSERT(sc);

	if (SC2IFP(sc)->if_flags & IFF_UP &&
	    sc->sc_vhid > 0 && (sc->sc_naddrs || sc->sc_naddrs6))
		SC2IFP(sc)->if_drv_flags |= IFF_DRV_RUNNING;
	else {
		SC2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
		carp_setroute(sc, RTM_DELETE);
		return;
	}

	switch (sc->sc_state) {
	case INIT:
		if (carp_opts[CARPCTL_PREEMPT] && !carp_suppress_preempt) {
			carp_send_ad_locked(sc);
			carp_send_arp(sc);
#ifdef INET6
			carp_send_na(sc);
#endif /* INET6 */
			CARP_DEBUG("%s: INIT -> MASTER (preempting)\n",
			    SC2IFP(sc)->if_xname);
			carp_set_state(sc, MASTER);
			carp_setroute(sc, RTM_ADD);
		} else {
			CARP_DEBUG("%s: INIT -> BACKUP\n", SC2IFP(sc)->if_xname);
			carp_set_state(sc, BACKUP);
			carp_setroute(sc, RTM_DELETE);
			carp_setrun(sc, 0);
		}
		break;
	case BACKUP:
		callout_stop(&sc->sc_ad_tmo);
		tv.tv_sec = 3 * sc->sc_advbase;
		tv.tv_usec = sc->sc_advskew * 1000000 / 256;
		switch (af) {
#ifdef INET
		case AF_INET:
			callout_reset(&sc->sc_md_tmo, tvtohz(&tv),
			    carp_master_down, sc);
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			callout_reset(&sc->sc_md6_tmo, tvtohz(&tv),
			    carp_master_down, sc);
			break;
#endif /* INET6 */
		default:
			if (sc->sc_naddrs)
				callout_reset(&sc->sc_md_tmo, tvtohz(&tv),
				    carp_master_down, sc);
			if (sc->sc_naddrs6)
				callout_reset(&sc->sc_md6_tmo, tvtohz(&tv),
				    carp_master_down, sc);
			break;
		}
		break;
	case MASTER:
		tv.tv_sec = sc->sc_advbase;
		tv.tv_usec = sc->sc_advskew * 1000000 / 256;
		callout_reset(&sc->sc_ad_tmo, tvtohz(&tv),
		    carp_send_ad, sc);
		break;
	}
}

static int
carp_set_addr(struct carp_softc *sc, struct sockaddr_in *sin)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct in_ifaddr *ia, *ia_if;
	struct ip_moptions *imo = &sc->sc_imo;
	struct in_addr addr;
	u_long iaddr = htonl(sin->sin_addr.s_addr);
	int own, error;

	if (sin->sin_addr.s_addr == 0) {
		if (!(SC2IFP(sc)->if_flags & IFF_UP))
			carp_set_state(sc, INIT);
		if (sc->sc_naddrs)
			SC2IFP(sc)->if_flags |= IFF_UP;
		carp_setrun(sc, 0);
		return (0);
	}

	/* we have to do it by hands to check we won't match on us */
	ia_if = NULL; own = 0;
	TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
		/* and, yeah, we need a multicast-capable iface too */
		if (ia->ia_ifp != SC2IFP(sc) &&
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) &&
		    (iaddr & ia->ia_subnetmask) == ia->ia_subnet) {
			if (!ia_if)
				ia_if = ia;
			if (sin->sin_addr.s_addr ==
			    ia->ia_addr.sin_addr.s_addr)
				own++;
		}
	}

	if (!ia_if)
		return (EADDRNOTAVAIL);

	ia = ia_if;
	ifp = ia->ia_ifp;

	if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0 ||
	    (imo->imo_multicast_ifp && imo->imo_multicast_ifp != ifp))
		return (EADDRNOTAVAIL);

	if (imo->imo_num_memberships == 0) {
		addr.s_addr = htonl(INADDR_CARP_GROUP);
		if ((imo->imo_membership[0] = in_addmulti(&addr, ifp)) == NULL)
			return (ENOBUFS);
		imo->imo_num_memberships++;
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = CARP_DFLTTL;
		imo->imo_multicast_loop = 0;
	}

	if (!ifp->if_carp) {

		MALLOC(cif, struct carp_if *, sizeof(*cif), M_CARP,
		    M_WAITOK|M_ZERO);
		if (!cif) {
			error = ENOBUFS;
			goto cleanup;
		}
		if ((error = ifpromisc(ifp, 1))) {
			FREE(cif, M_CARP);
			goto cleanup;
		}
		
		CARP_LOCK_INIT(cif);
		CARP_LOCK(cif);
		cif->vhif_ifp = ifp;
		TAILQ_INIT(&cif->vhif_vrs);
		ifp->if_carp = cif;

	} else {
		struct carp_softc *vr;

		cif = (struct carp_if *)ifp->if_carp;
		CARP_LOCK(cif);
		TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
			if (vr != sc && vr->sc_vhid == sc->sc_vhid) {
				CARP_UNLOCK(cif);
				error = EINVAL;
				goto cleanup;
			}
	}
	sc->sc_ia = ia;
	sc->sc_carpdev = ifp;

	{ /* XXX prevent endless loop if already in queue */
	struct carp_softc *vr, *after = NULL;
	int myself = 0;
	cif = (struct carp_if *)ifp->if_carp;

	/* XXX: cif should not change, right? So we still hold the lock */
	CARP_LOCK_ASSERT(cif);

	TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list) {
		if (vr == sc)
			myself = 1;
		if (vr->sc_vhid < sc->sc_vhid)
			after = vr;
	}

	if (!myself) {
		/* We're trying to keep things in order */
		if (after == NULL) {
			TAILQ_INSERT_TAIL(&cif->vhif_vrs, sc, sc_list);
		} else {
			TAILQ_INSERT_AFTER(&cif->vhif_vrs, after, sc, sc_list);
		}
		cif->vhif_nvrs++;
	}
	}

	sc->sc_naddrs++;
	SC2IFP(sc)->if_flags |= IFF_UP;
	if (own)
		sc->sc_advskew = 0;
	carp_sc_state_locked(sc);
	carp_setrun(sc, 0);

	CARP_UNLOCK(cif);

	return (0);

cleanup:
	in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
	return (error);
}

static int
carp_del_addr(struct carp_softc *sc, struct sockaddr_in *sin)
{
	int error = 0;

	if (!--sc->sc_naddrs) {
		struct carp_if *cif = (struct carp_if *)sc->sc_carpdev->if_carp;
		struct ip_moptions *imo = &sc->sc_imo;

		CARP_LOCK(cif);
		callout_stop(&sc->sc_ad_tmo);
		SC2IFP(sc)->if_flags &= ~IFF_UP;
		SC2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
		sc->sc_vhid = -1;
		in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
		imo->imo_multicast_ifp = NULL;
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			sc->sc_carpdev->if_carp = NULL;
			CARP_LOCK_DESTROY(cif);
			FREE(cif, M_IFADDR);
		} else {
			CARP_UNLOCK(cif);
		}
	}

	return (error);
}

#ifdef INET6
static int
carp_set_addr6(struct carp_softc *sc, struct sockaddr_in6 *sin6)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct in6_ifaddr *ia, *ia_if;
	struct ip6_moptions *im6o = &sc->sc_im6o;
	struct in6_multi_mship *imm;
	struct in6_addr in6;
	int own, error;

	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (!(SC2IFP(sc)->if_flags & IFF_UP))
			carp_set_state(sc, INIT);
		if (sc->sc_naddrs6)
			SC2IFP(sc)->if_flags |= IFF_UP;
		carp_setrun(sc, 0);
		return (0);
	}

	/* we have to do it by hands to check we won't match on us */
	ia_if = NULL; own = 0;
	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		int i;

		for (i = 0; i < 4; i++) {
			if ((sin6->sin6_addr.s6_addr32[i] &
			    ia->ia_prefixmask.sin6_addr.s6_addr32[i]) !=
			    (ia->ia_addr.sin6_addr.s6_addr32[i] &
			    ia->ia_prefixmask.sin6_addr.s6_addr32[i]))
				break;
		}
		/* and, yeah, we need a multicast-capable iface too */
		if (ia->ia_ifp != SC2IFP(sc) &&
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) &&
		    (i == 4)) {
			if (!ia_if)
				ia_if = ia;
			if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
			    &ia->ia_addr.sin6_addr))
				own++;
		}
	}

	if (!ia_if)
		return (EADDRNOTAVAIL);
	ia = ia_if;
	ifp = ia->ia_ifp;

	if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0 ||
	    (im6o->im6o_multicast_ifp && im6o->im6o_multicast_ifp != ifp))
		return (EADDRNOTAVAIL);

	if (!sc->sc_naddrs6) {
		im6o->im6o_multicast_ifp = ifp;

		/* join CARP multicast address */
		bzero(&in6, sizeof(in6));
		in6.s6_addr16[0] = htons(0xff02);
		in6.s6_addr8[15] = 0x12;
		if (in6_setscope(&in6, ifp, NULL) != 0)
			goto cleanup;
		if ((imm = in6_joingroup(ifp, &in6, &error, 0)) == NULL)
			goto cleanup;
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm, i6mm_chain);

		/* join solicited multicast address */
		bzero(&in6, sizeof(in6));
		in6.s6_addr16[0] = htons(0xff02);
		in6.s6_addr32[1] = 0;
		in6.s6_addr32[2] = htonl(1);
		in6.s6_addr32[3] = sin6->sin6_addr.s6_addr32[3];
		in6.s6_addr8[12] = 0xff;
		if (in6_setscope(&in6, ifp, NULL) != 0)
			goto cleanup;
		if ((imm = in6_joingroup(ifp, &in6, &error, 0)) == NULL)
			goto cleanup;
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm, i6mm_chain);
	}

	if (!ifp->if_carp) {
		MALLOC(cif, struct carp_if *, sizeof(*cif), M_CARP,
		    M_WAITOK|M_ZERO);
		if (!cif) {
			error = ENOBUFS;
			goto cleanup;
		}
		if ((error = ifpromisc(ifp, 1))) {
			FREE(cif, M_CARP);
			goto cleanup;
		}

		CARP_LOCK_INIT(cif);
		CARP_LOCK(cif);
		cif->vhif_ifp = ifp;
		TAILQ_INIT(&cif->vhif_vrs);
		ifp->if_carp = cif;

	} else {
		struct carp_softc *vr;

		cif = (struct carp_if *)ifp->if_carp;
		CARP_LOCK(cif);
		TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
			if (vr != sc && vr->sc_vhid == sc->sc_vhid) {
				CARP_UNLOCK(cif);
				error = EINVAL;
				goto cleanup;
			}
	}
	sc->sc_ia6 = ia;
	sc->sc_carpdev = ifp;

	{ /* XXX prevent endless loop if already in queue */
	struct carp_softc *vr, *after = NULL;
	int myself = 0;
	cif = (struct carp_if *)ifp->if_carp;
	CARP_LOCK_ASSERT(cif);

	TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list) {
		if (vr == sc)
			myself = 1;
		if (vr->sc_vhid < sc->sc_vhid)
			after = vr;
	}

	if (!myself) {
		/* We're trying to keep things in order */
		if (after == NULL) {
			TAILQ_INSERT_TAIL(&cif->vhif_vrs, sc, sc_list);
		} else {
			TAILQ_INSERT_AFTER(&cif->vhif_vrs, after, sc, sc_list);
		}
		cif->vhif_nvrs++;
	}
	}

	sc->sc_naddrs6++;
	SC2IFP(sc)->if_flags |= IFF_UP;
	if (own)
		sc->sc_advskew = 0;
	carp_sc_state_locked(sc);
	carp_setrun(sc, 0);

	CARP_UNLOCK(cif);

	return (0);

cleanup:
	/* clean up multicast memberships */
	if (!sc->sc_naddrs6) {
		while (!LIST_EMPTY(&im6o->im6o_memberships)) {
			imm = LIST_FIRST(&im6o->im6o_memberships);
			LIST_REMOVE(imm, i6mm_chain);
			in6_leavegroup(imm);
		}
	}
	return (error);
}

static int
carp_del_addr6(struct carp_softc *sc, struct sockaddr_in6 *sin6)
{
	int error = 0;

	if (!--sc->sc_naddrs6) {
		struct carp_if *cif = (struct carp_if *)sc->sc_carpdev->if_carp;
		struct ip6_moptions *im6o = &sc->sc_im6o;

		CARP_LOCK(cif);
		callout_stop(&sc->sc_ad_tmo);
		SC2IFP(sc)->if_flags &= ~IFF_UP;
		SC2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
		sc->sc_vhid = -1;
		while (!LIST_EMPTY(&im6o->im6o_memberships)) {
			struct in6_multi_mship *imm =
			    LIST_FIRST(&im6o->im6o_memberships);

			LIST_REMOVE(imm, i6mm_chain);
			in6_leavegroup(imm);
		}
		im6o->im6o_multicast_ifp = NULL;
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			CARP_LOCK_DESTROY(cif);
			sc->sc_carpdev->if_carp = NULL;
			FREE(cif, M_IFADDR);
		} else
			CARP_UNLOCK(cif);
	}

	return (error);
}
#endif /* INET6 */

static int
carp_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct carp_softc *sc = ifp->if_softc, *vr;
	struct carpreq carpr;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	struct ifaliasreq *ifra;
	int locked = 0, error = 0;

	ifa = (struct ifaddr *)addr;
	ifra = (struct ifaliasreq *)addr;
	ifr = (struct ifreq *)addr;

	switch (cmd) {
	case SIOCSIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			SC2IFP(sc)->if_flags |= IFF_UP;
			bcopy(ifa->ifa_addr, ifa->ifa_dstaddr,
			    sizeof(struct sockaddr));
			error = carp_set_addr(sc, satosin(ifa->ifa_addr));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			SC2IFP(sc)->if_flags |= IFF_UP;
			error = carp_set_addr6(sc, satosin6(ifa->ifa_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCAIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			SC2IFP(sc)->if_flags |= IFF_UP;
			bcopy(ifa->ifa_addr, ifa->ifa_dstaddr,
			    sizeof(struct sockaddr));
			error = carp_set_addr(sc, satosin(&ifra->ifra_addr));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			SC2IFP(sc)->if_flags |= IFF_UP;
			error = carp_set_addr6(sc, satosin6(&ifra->ifra_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCDIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			error = carp_del_addr(sc, satosin(&ifra->ifra_addr));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			error = carp_del_addr6(sc, satosin6(&ifra->ifra_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if (sc->sc_carpdev) {
			locked = 1;
			CARP_SCLOCK(sc);
		}
		if (sc->sc_state != INIT && !(ifr->ifr_flags & IFF_UP)) {
 			callout_stop(&sc->sc_ad_tmo);
 			callout_stop(&sc->sc_md_tmo);
 			callout_stop(&sc->sc_md6_tmo);
			if (sc->sc_state == MASTER)
				carp_send_ad_locked(sc);
			carp_set_state(sc, INIT);
			carp_setrun(sc, 0);
		} else if (sc->sc_state == INIT && (ifr->ifr_flags & IFF_UP)) {
			SC2IFP(sc)->if_flags |= IFF_UP;
			carp_setrun(sc, 0);
		}
		break;

	case SIOCSVH:
		if ((error = suser(curthread)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &carpr, sizeof carpr)))
			break;
		error = 1;
		if (sc->sc_carpdev) {
			locked = 1;
			CARP_SCLOCK(sc);
		}
		if (sc->sc_state != INIT && carpr.carpr_state != sc->sc_state) {
			switch (carpr.carpr_state) {
			case BACKUP:
				callout_stop(&sc->sc_ad_tmo);
				carp_set_state(sc, BACKUP);
				carp_setrun(sc, 0);
				carp_setroute(sc, RTM_DELETE);
				break;
			case MASTER:
				carp_master_down_locked(sc);
				break;
			default:
				break;
			}
		}
		if (carpr.carpr_vhid > 0) {
			if (carpr.carpr_vhid > 255) {
				error = EINVAL;
				break;
			}
			if (sc->sc_carpdev) {
				struct carp_if *cif;
				cif = (struct carp_if *)sc->sc_carpdev->if_carp;
				TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
					if (vr != sc &&
					    vr->sc_vhid == carpr.carpr_vhid)
						return EEXIST;
			}
			sc->sc_vhid = carpr.carpr_vhid;
			IFP2ENADDR(sc->sc_ifp)[0] = 0;
			IFP2ENADDR(sc->sc_ifp)[1] = 0;
			IFP2ENADDR(sc->sc_ifp)[2] = 0x5e;
			IFP2ENADDR(sc->sc_ifp)[3] = 0;
			IFP2ENADDR(sc->sc_ifp)[4] = 1;
			IFP2ENADDR(sc->sc_ifp)[5] = sc->sc_vhid;
			error--;
		}
		if (carpr.carpr_advbase > 0 || carpr.carpr_advskew > 0) {
			if (carpr.carpr_advskew >= 255) {
				error = EINVAL;
				break;
			}
			if (carpr.carpr_advbase > 255) {
				error = EINVAL;
				break;
			}
			sc->sc_advbase = carpr.carpr_advbase;
			sc->sc_advskew = carpr.carpr_advskew;
			error--;
		}
		bcopy(carpr.carpr_key, sc->sc_key, sizeof(sc->sc_key));
		if (error > 0)
			error = EINVAL;
		else {
			error = 0;
			carp_setrun(sc, 0);
		}
		break;

	case SIOCGVH:
		/* XXX: lockless read */
		bzero(&carpr, sizeof(carpr));
		carpr.carpr_state = sc->sc_state;
		carpr.carpr_vhid = sc->sc_vhid;
		carpr.carpr_advbase = sc->sc_advbase;
		carpr.carpr_advskew = sc->sc_advskew;
		if (suser(curthread) == 0)
			bcopy(sc->sc_key, carpr.carpr_key,
			    sizeof(carpr.carpr_key));
		error = copyout(&carpr, ifr->ifr_data, sizeof(carpr));
		break;

	default:
		error = EINVAL;
	}

	if (locked)
		CARP_SCUNLOCK(sc);

	carp_hmac_prepare(sc);

	return (error);
}

/*
 * XXX: this is looutput. We should eventually use it from there.
 */
static int
carp_looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	u_int32_t af;

	M_ASSERTPKTHDR(m); /* check if we have the packet header */

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
			rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC) {
		bcopy(dst->sa_data, &af, sizeof(af));
		dst->sa_family = af;
	}

#if 1	/* XXX */
	switch (dst->sa_family) {
	case AF_INET:
	case AF_INET6:
	case AF_IPX:
	case AF_APPLETALK:
		break;
	default:
		printf("carp_looutput: af=%d unexpected\n", dst->sa_family);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
#endif
	return(if_simloop(ifp, m, dst->sa_family, 0));
}

/*
 * Start output on carp interface. This function should never be called.
 */
static void
carp_start(struct ifnet *ifp)
{
#ifdef DEBUG
	printf("%s: start called\n", ifp->if_xname);
#endif
}

int
carp_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct m_tag *mtag;
	struct carp_softc *sc;
	struct ifnet *carp_ifp;

	if (!sa)
		return (0);

	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		break;
#endif /* INET6 */
	default:
		return (0);
	}

	mtag = m_tag_find(m, PACKET_TAG_CARP, NULL);
	if (mtag == NULL)
		return (0);

	bcopy(mtag + 1, &carp_ifp, sizeof(struct ifnet *));
	sc = carp_ifp->if_softc;

	/* Set the source MAC address to Virtual Router MAC Address */
	switch (ifp->if_type) {
	case IFT_ETHER:
	case IFT_L2VLAN: {
			struct ether_header *eh;

			eh = mtod(m, struct ether_header *);
			eh->ether_shost[0] = 0;
			eh->ether_shost[1] = 0;
			eh->ether_shost[2] = 0x5e;
			eh->ether_shost[3] = 0;
			eh->ether_shost[4] = 1;
			eh->ether_shost[5] = sc->sc_vhid;
		}
		break;
	case IFT_FDDI: {
			struct fddi_header *fh;

			fh = mtod(m, struct fddi_header *);
			fh->fddi_shost[0] = 0;
			fh->fddi_shost[1] = 0;
			fh->fddi_shost[2] = 0x5e;
			fh->fddi_shost[3] = 0;
			fh->fddi_shost[4] = 1;
			fh->fddi_shost[5] = sc->sc_vhid;
		}
		break;
	case IFT_ISO88025: {
 			struct iso88025_header *th;
 			th = mtod(m, struct iso88025_header *);
			th->iso88025_shost[0] = 3;
			th->iso88025_shost[1] = 0;
			th->iso88025_shost[2] = 0x40 >> (sc->sc_vhid - 1);
			th->iso88025_shost[3] = 0x40000 >> (sc->sc_vhid - 1);
			th->iso88025_shost[4] = 0;
			th->iso88025_shost[5] = 0;
		}
		break;
	default:
		printf("%s: carp is not supported for this interface type\n",
		    ifp->if_xname);
		return (EOPNOTSUPP);
	}

	return (0);
}

static void
carp_set_state(struct carp_softc *sc, int state)
{

	if (sc->sc_carpdev)
		CARP_SCLOCK_ASSERT(sc);

	if (sc->sc_state == state)
		return;

	sc->sc_state = state;
	switch (state) {
	case BACKUP:
		SC2IFP(sc)->if_link_state = LINK_STATE_DOWN;
		break;
	case MASTER:
		SC2IFP(sc)->if_link_state = LINK_STATE_UP;
		break;
	default:
		SC2IFP(sc)->if_link_state = LINK_STATE_UNKNOWN;
		break;
	}
	rt_ifmsg(SC2IFP(sc));
}

void
carp_carpdev_state(void *v)
{
	struct carp_if *cif = v;

	CARP_LOCK(cif);
	carp_carpdev_state_locked(cif);
	CARP_UNLOCK(cif);
}

static void
carp_carpdev_state_locked(struct carp_if *cif)
{
	struct carp_softc *sc;

	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list)
		carp_sc_state_locked(sc);
}

static void
carp_sc_state_locked(struct carp_softc *sc)
{
	CARP_SCLOCK_ASSERT(sc);

	if (sc->sc_carpdev->if_link_state != LINK_STATE_UP ||
	    !(sc->sc_carpdev->if_flags & IFF_UP)) {
		sc->sc_flags_backup = SC2IFP(sc)->if_flags;
		SC2IFP(sc)->if_flags &= ~IFF_UP;
		SC2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
		callout_stop(&sc->sc_ad_tmo);
		callout_stop(&sc->sc_md_tmo);
		callout_stop(&sc->sc_md6_tmo);
		carp_set_state(sc, INIT);
		carp_setrun(sc, 0);
		if (!sc->sc_suppress) {
			carp_suppress_preempt++;
			if (carp_suppress_preempt == 1) {
				CARP_SCUNLOCK(sc);
				carp_send_ad_all();
				CARP_SCLOCK(sc);
			}
		}
		sc->sc_suppress = 1;
	} else {
		SC2IFP(sc)->if_flags |= sc->sc_flags_backup;
		carp_set_state(sc, INIT);
		carp_setrun(sc, 0);
		if (sc->sc_suppress)
			carp_suppress_preempt--;
		sc->sc_suppress = 0;
	}

	return;
}

static int
carp_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		mtx_init(&carp_mtx, "carp_mtx", NULL, MTX_DEF);
		LIST_INIT(&carpif_list);
		if_clone_attach(&carp_cloner);
		break;

	case MOD_UNLOAD:
		if_clone_detach(&carp_cloner);
		while (!LIST_EMPTY(&carpif_list))
			ifc_simple_destroy(&carp_cloner,
			    SC2IFP(LIST_FIRST(&carpif_list)));
		mtx_destroy(&carp_mtx);
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}

static moduledata_t carp_mod = {
	"carp",
	carp_modevent,
	0
};

DECLARE_MODULE(carp, carp_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
