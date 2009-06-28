/*-
 * Copyright (c) 2008 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by CK Software GmbH under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * A pair of virtual ethernet interfaces directly connected with
 * a virtual cross-over cable.
 * This is mostly intended to be used to provide connectivity between
 * different virtual network stack instances.
 */
/*
 * Things to re-think once we have more experience:
 * - ifp->if_reassign function once we can test with vimage.
 * - Real random etheraddrs that are checked to be uniquish;
 *   in case we bridge we may need this or let the user handle that case?
 * - netisr and callback logic.
 * - netisr queue lengths.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/vimage.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/netisr.h>

#define	EPAIRNAME	"epair"

#ifdef DEBUG_EPAIR
static int epair_debug = 0;
SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, OID_AUTO, epair, CTLFLAG_RW, 0, "epair sysctl");
SYSCTL_XINT(_net_link_epair, OID_AUTO, epair_debug, CTLFLAG_RW,
    &epair_debug, 0, "if_epair(4) debugging.");
#define	DPRINTF(fmt, arg...)	if (epair_debug) \
    printf("[%s:%d] " fmt, __func__, __LINE__, ##arg)
#else
#define	DPRINTF(fmt, arg...)
#endif

struct epair_softc {
	struct ifnet	*ifp;
	struct ifnet	*oifp;
	u_int		refcount;
	void		(*if_qflush)(struct ifnet *);
};

struct epair_ifp_drain {
	STAILQ_ENTRY(epair_ifp_drain)	ifp_next;
	struct ifnet			*ifp;
};

static STAILQ_HEAD(, epair_ifp_drain) epair_ifp_drain_list =
    STAILQ_HEAD_INITIALIZER(epair_ifp_drain_list);

#define ADD_IFQ_FOR_DRAINING(ifp)					\
	do {								\
		struct epair_ifp_drain *elm = NULL;			\
									\
		STAILQ_FOREACH(elm, &epair_ifp_drain_list, ifp_next) {	\
			if (elm->ifp == (ifp))				\
				break;					\
		}							\
		if (elm == NULL) {					\
			elm = malloc(sizeof(struct epair_ifp_drain),	\
			    M_EPAIR, M_ZERO);				\
			if (elm != NULL) {				\
				elm->ifp = (ifp);			\
				STAILQ_INSERT_TAIL(			\
				    &epair_ifp_drain_list,		\
			    	    elm, ifp_next);			\
			}						\
		}							\
	} while(0)

/* Our "hw" tx queue. */
static struct ifqueue epairinq;
static int epair_drv_flags;

static struct mtx if_epair_mtx;
#define	EPAIR_LOCK_INIT()	mtx_init(&if_epair_mtx, "if_epair", \
				    NULL, MTX_DEF)
#define	EPAIR_LOCK_DESTROY()	mtx_destroy(&if_epair_mtx)
#define	EPAIR_LOCK_ASSERT()	mtx_assert(&if_epair_mtx, MA_OWNED)
#define	EPAIR_LOCK()		mtx_lock(&if_epair_mtx)
#define	EPAIR_UNLOCK()		mtx_unlock(&if_epair_mtx)

static MALLOC_DEFINE(M_EPAIR, EPAIRNAME,
    "Pair of virtual cross-over connected Ethernet-like interfaces");

static int epair_clone_match(struct if_clone *, const char *);
static int epair_clone_create(struct if_clone *, char *, size_t, caddr_t);
static int epair_clone_destroy(struct if_clone *, struct ifnet *);

static void epair_start_locked(struct ifnet *);

static struct if_clone epair_cloner = IFC_CLONE_INITIALIZER(
    EPAIRNAME, NULL, IF_MAXUNIT,
    NULL, epair_clone_match, epair_clone_create, epair_clone_destroy);


/*
 * Netisr handler functions.
 */
static void
epair_sintr(struct mbuf *m)
{
	struct ifnet *ifp;
	struct epair_softc *sc;

	ifp = m->m_pkthdr.rcvif;
	(*ifp->if_input)(ifp, m);
	sc = ifp->if_softc;
	refcount_release(&sc->refcount);
	DPRINTF("ifp=%p refcount=%u\n", ifp, sc->refcount);
}

static void
epair_sintr_drained(void)
{
	struct epair_ifp_drain *elm, *tvar;
	struct ifnet *ifp;

	EPAIR_LOCK();
	/*
	 * Assume our "hw" queue and possibly ifq will be emptied
	 * again. In case we will overflow the "hw" queue while
	 * draining, epair_start_locked will set IFF_DRV_OACTIVE
	 * again and we will stop and return.
	 */
	STAILQ_FOREACH_SAFE(elm, &epair_ifp_drain_list, ifp_next, tvar) {
		ifp = elm->ifp;
		epair_drv_flags &= ~IFF_DRV_OACTIVE;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		epair_start_locked(ifp);

		IFQ_LOCK(&ifp->if_snd);
		if (IFQ_IS_EMPTY(&ifp->if_snd)) {
			STAILQ_REMOVE(&epair_ifp_drain_list, elm,
			    epair_ifp_drain, ifp_next);
			free(elm, M_EPAIR);
		}
		IFQ_UNLOCK(&ifp->if_snd);

		if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) != 0) {
			/* Our "hw"q overflew again. */
			epair_drv_flags |= IFF_DRV_OACTIVE
			DPRINTF("hw queue length overflow at %u\n",
			    epairinq.ifq_maxlen);
#if 0
			/* ``Auto-tuning.'' */
			epairinq.ifq_maxlen += ifqmaxlen;
#endif
			break;
		}
	}
	EPAIR_UNLOCK();
}

/*
 * Network interface (`if') related functions.
 */
static void
epair_start_locked(struct ifnet *ifp)
{
	struct mbuf *m;
	struct epair_softc *sc;
	struct ifnet *oifp;
	int error;

	EPAIR_LOCK_ASSERT();
	DPRINTF("ifp=%p\n", ifp);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;
	if ((ifp->if_flags & IFF_UP) == 0)
		return;

	/*
	 * We get patckets here from ether_output via if_handoff()
	 * and ned to put them into the input queue of the oifp
	 * and call oifp->if_input() via netisr/epair_sintr().
	 */
	sc = ifp->if_softc;
	oifp = sc->oifp;
	sc = oifp->if_softc;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		BPF_MTAP(ifp, m);

		/*
		 * In case the outgoing interface is not usable,
		 * drop the packet.
		 */
		if ((oifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
		    (oifp->if_flags & IFF_UP) ==0) {
			ifp->if_oerrors++;
			m_freem(m);
			continue;
		}
		DPRINTF("packet %s -> %s\n", ifp->if_xname, oifp->if_xname);

		/*
		 * Add a reference so the interface cannot go while the
		 * packet is in transit as we rely on rcvif to stay valid.
		 */
		refcount_acquire(&sc->refcount);
		m->m_pkthdr.rcvif = oifp;
		CURVNET_SET_QUIET(oifp->if_vnet);
		error = netisr_queue(NETISR_EPAIR, m);
		CURVNET_RESTORE();
		if (!error) {
			ifp->if_opackets++;
			/* Someone else received the packet. */
			oifp->if_ipackets++;
		} else {
			epair_drv_flags |= IFF_DRV_OACTIVE;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			ADD_IFQ_FOR_DRAINING(ifp);
			refcount_release(&sc->refcount);
		}
	}
}

static void
epair_start(struct ifnet *ifp)
{

	EPAIR_LOCK();
	epair_start_locked(ifp);
	EPAIR_UNLOCK();
}

static int
epair_transmit_locked(struct ifnet *ifp, struct mbuf *m)
{
	struct epair_softc *sc;
	struct ifnet *oifp;
	int error, len;
	short mflags;

	EPAIR_LOCK_ASSERT();
	DPRINTF("ifp=%p m=%p\n", ifp, m);

	if (m == NULL)
		return (0);
	
	/*
	 * We are not going to use the interface en/dequeue mechanism
	 * on the TX side. We are called from ether_output_frame()
	 * and will put the packet into the incoming queue of the
	 * other interface of our pair via the netsir.
	 */
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		m_freem(m);
		return (ENXIO);
	}
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return (ENETDOWN);
	}

	BPF_MTAP(ifp, m);

	/*
	 * In case the outgoing interface is not usable,
	 * drop the packet.
	 */
	sc = ifp->if_softc;
	oifp = sc->oifp;
	if ((oifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    (oifp->if_flags & IFF_UP) ==0) {
		ifp->if_oerrors++;
		m_freem(m);
		return (0);
	}
	len = m->m_pkthdr.len;
	mflags = m->m_flags;
	DPRINTF("packet %s -> %s\n", ifp->if_xname, oifp->if_xname);

#ifdef ALTQ
	/* Support ALTQ via the clasic if_start() path. */
	IF_LOCK(&ifp->if_snd);
	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		ALTQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
		if (error)
			ifp->if_snd.ifq_drops++;
		IF_UNLOCK(&ifp->if_snd);
		if (!error) {
			ifp->if_obytes += len;
			if (mflags & (M_BCAST|M_MCAST))
				ifp->if_omcasts++;
			
			if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0)
				epair_start_locked(ifp);
			else
				ADD_IFQ_FOR_DRAINING(ifp);
		}
		return (error);
	}
	IF_UNLOCK(&ifp->if_snd);
#endif

	if ((epair_drv_flags & IFF_DRV_OACTIVE) != 0) {
		/*
		 * Our hardware queue is full, try to fall back
		 * queuing to the ifq but do not call ifp->if_start.
		 * Either we are lucky or the packet is gone.
		 */
		IFQ_ENQUEUE(&ifp->if_snd, m, error);
		if (!error)
			ADD_IFQ_FOR_DRAINING(ifp);
		return (error);
	}
	sc = oifp->if_softc;
	/*
	 * Add a reference so the interface cannot go while the
	 * packet is in transit as we rely on rcvif to stay valid.
	 */
	refcount_acquire(&sc->refcount);
	m->m_pkthdr.rcvif = oifp;
	CURVNET_SET_QUIET(oifp->if_vnet);
	error = netisr_queue(NETISR_EPAIR, m);
	CURVNET_RESTORE();
	if (!error) {
		ifp->if_opackets++;
		/*
		 * IFQ_HANDOFF_ADJ/ip_handoff() update statistics,
		 * but as we bypass all this we have to duplicate
		 * the logic another time.
		 */
		ifp->if_obytes += len;
		if (mflags & (M_BCAST|M_MCAST))
			ifp->if_omcasts++;
		/* Someone else received the packet. */
		oifp->if_ipackets++;
	} else {
		/* The packet was freed already. */
		refcount_release(&sc->refcount);
		epair_drv_flags |= IFF_DRV_OACTIVE;
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	}

	return (error);
}

static int
epair_transmit(struct ifnet *ifp, struct mbuf *m)
{
	int error;

	EPAIR_LOCK();
	error = epair_transmit_locked(ifp, m);
	EPAIR_UNLOCK();
	return (error);
}

static void
epair_qflush(struct ifnet *ifp)
{
	struct epair_softc *sc;
	struct ifaltq *ifq;
	
	EPAIR_LOCK();
	sc = ifp->if_softc;
	ifq = &ifp->if_snd;
	DPRINTF("ifp=%p sc refcnt=%u ifq_len=%u\n",
	    ifp, sc->refcount, ifq->ifq_len);
	/*
	 * Instead of calling refcount_release(&sc->refcount);
	 * n times, just subtract for the cleanup.
	 */
	sc->refcount -= ifq->ifq_len;
	EPAIR_UNLOCK();
	if (sc->if_qflush)
		sc->if_qflush(ifp);
}

static int
epair_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr;
	int error;

	ifr = (struct ifreq *)data;
	switch (cmd) {
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = 0;
		break;

	default:
		/* Let the common ethernet handler process this. */
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

static void
epair_init(void *dummy __unused)
{
}


/*
 * Interface cloning functions.
 * We use our private ones so that we can create/destroy our secondary
 * device along with the primary one.
 */
static int
epair_clone_match(struct if_clone *ifc, const char *name)
{
	const char *cp;

	DPRINTF("name='%s'\n", name);

	/*
	 * Our base name is epair.
	 * Our interfaces will be named epair<n>[ab].
	 * So accept anything of the following list:
	 * - epair
	 * - epair<n>
	 * but not the epair<n>[ab] versions.
	 */
	if (strncmp(EPAIRNAME, name, sizeof(EPAIRNAME)-1) != 0)
		return (0);

	for (cp = name + sizeof(EPAIRNAME) - 1; *cp != '\0'; cp++) {
		if (*cp < '0' || *cp > '9')
			return (0);
	}

	return (1);
}

static int
epair_clone_create(struct if_clone *ifc, char *name, size_t len, caddr_t params)
{
	struct epair_softc *sca, *scb;
	struct ifnet *ifp;
	char *dp;
	int error, unit, wildcard;
	uint8_t eaddr[ETHER_ADDR_LEN];	/* 00:00:00:00:00:00 */

	/*
	 * We are abusing params to create our second interface.
	 * Actually we already created it and called if_clone_createif()
	 * for it to do the official insertion procedure the moment we knew
	 * it cannot fail anymore. So just do attach it here.
	 */
	if (params) {
		scb = (struct epair_softc *)params;
		ifp = scb->ifp;
		/* Assign a hopefully unique, locally administered etheraddr. */
		eaddr[0] = 0x02;
		eaddr[3] = (ifp->if_index >> 8) & 0xff;
		eaddr[4] = ifp->if_index & 0xff;
		eaddr[5] = 0x0b;
		ether_ifattach(ifp, eaddr);
		/* Correctly set the name for the cloner list. */
		strlcpy(name, scb->ifp->if_xname, len);
		return (0);
	}

	/* Try to see if a special unit was requested. */
	error = ifc_name2unit(name, &unit);
	if (error != 0)
		return (error);
	wildcard = (unit < 0);

	error = ifc_alloc_unit(ifc, &unit);
	if (error != 0)
		return (error);

	/*
	 * If no unit had been given, we need to adjust the ifName.
	 * Also make sure there is space for our extra [ab] suffix.
	 */
	for (dp = name; *dp != '\0'; dp++);
	if (wildcard) {
		error = snprintf(dp, len - (dp - name), "%d", unit);
		if (error > len - (dp - name) - 1) {
			/* ifName too long. */
			ifc_free_unit(ifc, unit);
			return (ENOSPC);
		}
		dp += error;
	}
	if (len - (dp - name) - 1 < 1) {
		/* No space left for our [ab] suffix. */
		ifc_free_unit(ifc, unit);
		return (ENOSPC);
	}
	*dp = 'a';
	/* Must not change dp so we can replace 'a' by 'b' later. */
	*(dp+1) = '\0';

	/* Allocate memory for both [ab] interfaces */
	sca = malloc(sizeof(struct epair_softc), M_EPAIR, M_WAITOK | M_ZERO);
	refcount_init(&sca->refcount, 1);
	sca->ifp = if_alloc(IFT_ETHER);
	if (sca->ifp == NULL) {
		free(sca, M_EPAIR);
		ifc_free_unit(ifc, unit);
		return (ENOSPC);
	}

	scb = malloc(sizeof(struct epair_softc), M_EPAIR, M_WAITOK | M_ZERO);
	refcount_init(&scb->refcount, 1);
	scb->ifp = if_alloc(IFT_ETHER);
	if (scb->ifp == NULL) {
		free(scb, M_EPAIR);
		if_free(sca->ifp);
		free(sca, M_EPAIR);
		ifc_free_unit(ifc, unit);
		return (ENOSPC);
	}
	
	/*
	 * Cross-reference the interfaces so we will be able to free both.
	 */
	sca->oifp = scb->ifp;
	scb->oifp = sca->ifp;
	
	/* Finish initialization of interface <n>a. */
	ifp = sca->ifp;
	ifp->if_softc = sca;
	strlcpy(ifp->if_xname, name, IFNAMSIZ);
	ifp->if_dname = ifc->ifc_name;
	ifp->if_dunit = unit;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = epair_start;
	ifp->if_ioctl = epair_ioctl;
	ifp->if_init  = epair_init;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	/* Assign a hopefully unique, locally administered etheraddr. */
	eaddr[0] = 0x02;
	eaddr[3] = (ifp->if_index >> 8) & 0xff;
	eaddr[4] = ifp->if_index & 0xff;
	eaddr[5] = 0x0a;
	ether_ifattach(ifp, eaddr);
	sca->if_qflush = ifp->if_qflush;
	ifp->if_qflush = epair_qflush;
	ifp->if_transmit = epair_transmit;
	ifp->if_baudrate = IF_Gbps(10UL);	/* arbitrary maximum */

	/* Swap the name and finish initialization of interface <n>b. */
	*dp = 'b';

	ifp = scb->ifp;
	ifp->if_softc = scb;
	strlcpy(ifp->if_xname, name, IFNAMSIZ);
	ifp->if_dname = ifc->ifc_name;
	ifp->if_dunit = unit;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = epair_start;
	ifp->if_ioctl = epair_ioctl;
	ifp->if_init  = epair_init;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	/* We need to play some tricks here for the second interface. */
	strlcpy(name, EPAIRNAME, len);
	error = if_clone_create(name, len, (caddr_t)scb);
	if (error)
		panic("%s: if_clone_createif() for our 2nd iface failed: %d",
		    __func__, error);
	scb->if_qflush = ifp->if_qflush;
	ifp->if_qflush = epair_qflush;
	ifp->if_transmit = epair_transmit;
	ifp->if_baudrate = IF_Gbps(10UL);	/* arbitrary maximum */

	/*
	 * Restore name to <n>a as the ifp for this will go into the
	 * cloner list for the initial call.
	 */
	strlcpy(name, sca->ifp->if_xname, len);
	DPRINTF("name='%s/%db' created sca=%p scb=%p\n", name, unit, sca, scb);

	/* Tell the world, that we are ready to rock. */
	sca->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	scb->ifp->if_drv_flags |= IFF_DRV_RUNNING;

	return (0);
}

static int
epair_clone_destroy(struct if_clone *ifc, struct ifnet *ifp)
{
	struct ifnet *oifp;
	struct epair_softc *sca, *scb;
	int unit, error;

	DPRINTF("ifp=%p\n", ifp);

	/*
	 * In case we called into if_clone_destroyif() ourselves
	 * again to remove the second interface, the softc will be
	 * NULL. In that case so not do anything but return success.
	 */
	if (ifp->if_softc == NULL)
		return (0);
	
	unit = ifp->if_dunit;
	sca = ifp->if_softc;
	oifp = sca->oifp;
	scb = oifp->if_softc;

	DPRINTF("ifp=%p oifp=%p\n", ifp, oifp);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	oifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	ether_ifdetach(oifp);
	ether_ifdetach(ifp);
	/*
	 * Wait for all packets to be dispatched to if_input.
	 * The numbers can only go down as the interfaces are
	 * detached so there is no need to use atomics.
	 */
	DPRINTF("sca refcnt=%u scb refcnt=%u\n", sca->refcount, scb->refcount);
	KASSERT(sca->refcount == 1 && scb->refcount == 1,
	    ("%s: sca->refcount!=1: %d || scb->refcount!=1: %d",
	    __func__, sca->refcount, scb->refcount));

	/*
	 * Get rid of our second half.
	 */
	oifp->if_softc = NULL;
	error = if_clone_destroyif(ifc, oifp);
	if (error)
		panic("%s: if_clone_destroyif() for our 2nd iface failed: %d",
		    __func__, error);

	/* Finish cleaning up. Free them and release the unit. */
	if_free_type(oifp, IFT_ETHER);
	if_free_type(ifp, IFT_ETHER);
	free(scb, M_EPAIR);
	free(sca, M_EPAIR);
	ifc_free_unit(ifc, unit);

	return (0);
}

static int
epair_modevent(module_t mod, int type, void *data)
{
	int tmp;

	switch (type) {
	case MOD_LOAD:
		/* For now limit us to one global mutex and one inq. */
		EPAIR_LOCK_INIT();
		epair_drv_flags = 0;
		epairinq.ifq_maxlen = 16 * ifqmaxlen; /* What is a good 16? */
		if (TUNABLE_INT_FETCH("net.link.epair.netisr_maxqlen", &tmp))
		    epairinq.ifq_maxlen = tmp;
		mtx_init(&epairinq.ifq_mtx, "epair_inq", NULL, MTX_DEF);
		netisr_register2(NETISR_EPAIR, (netisr_t *)epair_sintr,
		    epair_sintr_drained, &epairinq, 0);
		if_clone_attach(&epair_cloner);
		if (bootverbose)
			printf("%s initialized.\n", EPAIRNAME);
		break;
	case MOD_UNLOAD:
		if_clone_detach(&epair_cloner);
		netisr_unregister(NETISR_EPAIR);
		mtx_destroy(&epairinq.ifq_mtx);
		EPAIR_LOCK_DESTROY();
		if (bootverbose)
			printf("%s unloaded.\n", EPAIRNAME);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t epair_mod = {
	"if_epair",
	epair_modevent,
	0
};

DECLARE_MODULE(if_epair, epair_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_epair, 1);
