/*-
 * Copyright (c) 2006-2008 Sam Leffler.  All rights reserved.
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

/*
 * Intel XScale NPE Ethernet driver.
 *
 * This driver handles the two ports present on the IXP425.
 * Packet processing is done by the Network Processing Engines
 * (NPE's) that work together with a MAC and PHY. The MAC
 * is also mapped to the XScale cpu; the PHY is accessed via
 * the MAC. NPE-XScale communication happens through h/w
 * queues managed by the Q Manager block.
 *
 * The code here replaces the ethAcc, ethMii, and ethDB classes
 * in the Intel Access Library (IAL) and the OS-specific driver.
 *
 * XXX add vlan support
 */
#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/if_types.h>
#include <net/if_var.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>
#include <arm/xscale/ixp425/ixp425_qmgr.h>
#include <arm/xscale/ixp425/ixp425_npevar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <arm/xscale/ixp425/if_npereg.h>

#include <machine/armreg.h>

#include "miibus_if.h"

/*
 * XXX: For the main bus dma tag. Can go away if the new method to get the
 * dma tag from the parent got MFC'd into RELENG_6.
 */
extern struct ixp425_softc *ixp425_softc;

struct npebuf {
	struct npebuf	*ix_next;	/* chain to next buffer */
	void		*ix_m;		/* backpointer to mbuf */
	bus_dmamap_t	ix_map;		/* bus dma map for associated data */
	struct npehwbuf	*ix_hw;		/* associated h/w block */
	uint32_t	ix_neaddr;	/* phys address of ix_hw */
};

struct npedma {
	const char*	name;
	int		nbuf;		/* # npebuf's allocated */
	bus_dma_tag_t	mtag;		/* bus dma tag for mbuf data */
	struct npehwbuf	*hwbuf;		/* NPE h/w buffers */
	bus_dma_tag_t	buf_tag;	/* tag+map for NPE buffers */
	bus_dmamap_t	buf_map;
	bus_addr_t	buf_phys;	/* phys addr of buffers */
	struct npebuf	*buf;		/* s/w buffers (1-1 w/ h/w) */
};

struct npe_softc {
	/* XXX mii requires this be first; do not move! */
	struct ifnet	*sc_ifp;	/* ifnet pointer */
	struct mtx	sc_mtx;		/* basically a perimeter lock */
	device_t	sc_dev;
	bus_space_tag_t	sc_iot;		
	bus_space_handle_t sc_ioh;	/* MAC register window */
	device_t	sc_mii;		/* child miibus */
	bus_space_handle_t sc_miih;	/* MII register window */
	int		sc_npeid;
	struct ixpnpe_softc *sc_npe;	/* NPE support */
	int		sc_debug;	/* DPRINTF* control */
	int		sc_tickinterval;
	struct callout	tick_ch;	/* Tick callout */
	int		npe_watchdog_timer;
	struct npedma	txdma;
	struct npebuf	*tx_free;	/* list of free tx buffers */
	struct npedma	rxdma;
	bus_addr_t	buf_phys;	/* XXX for returning a value */
	int		rx_qid;		/* rx qid */
	int		rx_freeqid;	/* rx free buffers qid */
	int		tx_qid;		/* tx qid */
	int		tx_doneqid;	/* tx completed qid */
	struct ifmib_iso_8802_3 mibdata;
	bus_dma_tag_t	sc_stats_tag;	/* bus dma tag for stats block */
	struct npestats	*sc_stats;
	bus_dmamap_t	sc_stats_map;
	bus_addr_t	sc_stats_phys;	/* phys addr of sc_stats */
	struct npestats	sc_totals;	/* accumulated sc_stats */
};

/*
 * Static configuration for IXP425.  The tx and
 * rx free Q id's are fixed by the NPE microcode.  The
 * rx Q id's are programmed to be separate to simplify
 * multi-port processing.  It may be better to handle
 * all traffic through one Q (as done by the Intel drivers).
 *
 * Note that the PHY's are accessible only from MAC B on the
 * IXP425 and from MAC C on other devices.  This and other
 * platform-specific assumptions are handled with hints.
 */
static const struct {
	uint32_t	macbase;
	uint32_t	miibase;
	int		phy;		/* phy id */
	uint8_t		rx_qid;
	uint8_t		rx_freeqid;
	uint8_t		tx_qid;
	uint8_t		tx_doneqid;
} npeconfig[NPE_MAX] = {
	[NPE_A] = {
	  .macbase	= IXP435_MAC_A_HWBASE,
	  .miibase	= IXP425_MAC_C_HWBASE,
	  .phy		= 2,
	  .rx_qid	= 4,
	  .rx_freeqid	= 26,
	  .tx_qid	= 23,
	  .tx_doneqid	= 31
	},
	[NPE_B] = {
	  .macbase	= IXP425_MAC_B_HWBASE,
	  .miibase	= IXP425_MAC_B_HWBASE,
	  .phy		= 0,
	  .rx_qid	= 4,
	  .rx_freeqid	= 27,
	  .tx_qid	= 24,
	  .tx_doneqid	= 31
	},
	[NPE_C] = {
	  .macbase	= IXP425_MAC_C_HWBASE,
	  .miibase	= IXP425_MAC_B_HWBASE,
	  .phy		= 1,
	  .rx_qid	= 12,
	  .rx_freeqid	= 28,
	  .tx_qid	= 25,
	  .tx_doneqid	= 31
	},
};
static struct npe_softc *npes[NPE_MAX];	/* NB: indexed by npeid */

static __inline uint32_t
RD4(struct npe_softc *sc, bus_size_t off)
{
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, off);
}

static __inline void
WR4(struct npe_softc *sc, bus_size_t off, uint32_t val)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, off, val);
}

#define NPE_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	NPE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define NPE_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    MTX_NETWORK_LOCK, MTX_DEF)
#define NPE_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define NPE_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define NPE_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static devclass_t npe_devclass;

static int	override_npeid(device_t, const char *resname, int *val);
static int	npe_activate(device_t dev);
static void	npe_deactivate(device_t dev);
static int	npe_ifmedia_update(struct ifnet *ifp);
static void	npe_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr);
static void	npe_setmac(struct npe_softc *sc, u_char *eaddr);
static void	npe_getmac(struct npe_softc *sc, u_char *eaddr);
static void	npe_txdone(int qid, void *arg);
static int	npe_rxbuf_init(struct npe_softc *, struct npebuf *,
			struct mbuf *);
static int	npe_rxdone(int qid, void *arg);
static void	npeinit(void *);
static void	npestart_locked(struct ifnet *);
static void	npestart(struct ifnet *);
static void	npestop(struct npe_softc *);
static void	npewatchdog(struct npe_softc *);
static int	npeioctl(struct ifnet * ifp, u_long, caddr_t);

static int	npe_setrxqosentry(struct npe_softc *, int classix,
			int trafclass, int qid);
static int	npe_setportaddress(struct npe_softc *, const uint8_t mac[]);
static int	npe_setfirewallmode(struct npe_softc *, int onoff);
static int	npe_updatestats(struct npe_softc *);
#if 0
static int	npe_getstats(struct npe_softc *);
static uint32_t	npe_getimageid(struct npe_softc *);
static int	npe_setloopback(struct npe_softc *, int ena);
#endif

/* NB: all tx done processing goes through one queue */
static int tx_doneqid = -1;

static SYSCTL_NODE(_hw, OID_AUTO, npe, CTLFLAG_RD, 0,
    "IXP4XX NPE driver parameters");

static int npe_debug = 0;
SYSCTL_INT(_hw_npe, OID_AUTO, debug, CTLFLAG_RW, &npe_debug,
	   0, "IXP4XX NPE network interface debug msgs");
TUNABLE_INT("hw.npe.debug", &npe_debug);
#define	DPRINTF(sc, fmt, ...) do {					\
	if (sc->sc_debug) device_printf(sc->sc_dev, fmt, __VA_ARGS__);	\
} while (0)
#define	DPRINTFn(n, sc, fmt, ...) do {					\
	if (sc->sc_debug >= n) device_printf(sc->sc_dev, fmt, __VA_ARGS__);\
} while (0)
static int npe_tickinterval = 3;		/* npe_tick frequency (secs) */
SYSCTL_INT(_hw_npe, OID_AUTO, tickinterval, CTLFLAG_RD, &npe_tickinterval,
	    0, "periodic work interval (secs)");
TUNABLE_INT("hw.npe.tickinterval", &npe_tickinterval);

static	int npe_rxbuf = 64;		/* # rx buffers to allocate */
SYSCTL_INT(_hw_npe, OID_AUTO, rxbuf, CTLFLAG_RD, &npe_rxbuf,
	    0, "rx buffers allocated");
TUNABLE_INT("hw.npe.rxbuf", &npe_rxbuf);
static	int npe_txbuf = 128;		/* # tx buffers to allocate */
SYSCTL_INT(_hw_npe, OID_AUTO, txbuf, CTLFLAG_RD, &npe_txbuf,
	    0, "tx buffers allocated");
TUNABLE_INT("hw.npe.txbuf", &npe_txbuf);

static int
unit2npeid(int unit)
{
	static const int npeidmap[2][3] = {
		/* on 425 A is for HSS, B & C are for Ethernet */
		{ NPE_B, NPE_C, -1 },	/* IXP425 */
		/* 435 only has A & C, order C then A */
		{ NPE_C, NPE_A, -1 },	/* IXP435 */
	};
	/* XXX check feature register instead */
	return (unit < 3 ? npeidmap[
	    (cpu_id() & CPU_ID_CPU_MASK) == CPU_ID_IXP435][unit] : -1);
}

static int
npe_probe(device_t dev)
{
	static const char *desc[NPE_MAX] = {
		[NPE_A] = "IXP NPE-A",
		[NPE_B] = "IXP NPE-B",
		[NPE_C] = "IXP NPE-C"
	};
	int unit = device_get_unit(dev);
	int npeid;

	if (unit > 2 ||
	    (ixp4xx_read_feature_bits() &
	     (unit == 0 ? EXP_FCTRL_ETH0 : EXP_FCTRL_ETH1)) == 0)
		return EINVAL;

	npeid = -1;
	if (!override_npeid(dev, "npeid", &npeid))
		npeid = unit2npeid(unit);
	if (npeid == -1) {
		device_printf(dev, "unit %d not supported\n", unit);
		return EINVAL;
	}
	device_set_desc(dev, desc[npeid]);
	return 0;
}

static int
npe_attach(device_t dev)
{
	struct npe_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct ifnet *ifp;
	int error;
	u_char eaddr[6];

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	NPE_LOCK_INIT(sc);
	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);
	sc->sc_debug = npe_debug;
	sc->sc_tickinterval = npe_tickinterval;

	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "cannot allocate ifnet\n");
		error = EIO;		/* XXX */
		goto out;
	}
	/* NB: must be setup prior to invoking mii code */
	sc->sc_ifp = ifp;

	error = npe_activate(dev);
	if (error) {
		device_printf(dev, "cannot activate npe\n");
		goto out;
	}

	npe_getmac(sc, eaddr);

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = npestart;
	ifp->if_ioctl = npeioctl;
	ifp->if_init = npeinit;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->txdma.nbuf - 1);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_linkmib = &sc->mibdata;
	ifp->if_linkmiblen = sizeof(sc->mibdata);
	sc->mibdata.dot3Compliance = DOT3COMPLIANCE_STATS;
	/* device supports oversided vlan frames */
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "debug",
	    CTLFLAG_RW, &sc->sc_debug, 0, "control debugging printfs");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "tickinterval",
	    CTLFLAG_RW, &sc->sc_tickinterval, 0, "periodic work frequency");
	SYSCTL_ADD_STRUCT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "stats",
	    CTLFLAG_RD, &sc->sc_totals, npestats, "onboard stats");

	ether_ifattach(ifp, eaddr);
	return 0;
out:
	if (ifp != NULL)
		if_free(ifp);
	NPE_LOCK_DESTROY(sc);
	npe_deactivate(dev);
	return error;
}

static int
npe_detach(device_t dev)
{
	struct npe_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif
	npestop(sc);
	if (ifp != NULL) {
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	NPE_LOCK_DESTROY(sc);
	npe_deactivate(dev);
	return 0;
}

/*
 * Compute and install the multicast filter.
 */
static void
npe_setmcast(struct npe_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t mask[ETHER_ADDR_LEN], addr[ETHER_ADDR_LEN];
	int i;

	if (ifp->if_flags & IFF_PROMISC) {
		memset(mask, 0, ETHER_ADDR_LEN);
		memset(addr, 0, ETHER_ADDR_LEN);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		static const uint8_t allmulti[ETHER_ADDR_LEN] =
		    { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
		memcpy(mask, allmulti, ETHER_ADDR_LEN);
		memcpy(addr, allmulti, ETHER_ADDR_LEN);
	} else {
		uint8_t clr[ETHER_ADDR_LEN], set[ETHER_ADDR_LEN];
		struct ifmultiaddr *ifma;
		const uint8_t *mac;

		memset(clr, 0, ETHER_ADDR_LEN);
		memset(set, 0xff, ETHER_ADDR_LEN);

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			mac = LLADDR((struct sockaddr_dl *) ifma->ifma_addr);
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				clr[i] |= mac[i];
				set[i] &= mac[i];
			}
		}
		if_maddr_runlock(ifp);

		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			mask[i] = set[i] | ~clr[i];
			addr[i] = set[i];
		}
	}

	/*
	 * Write the mask and address registers.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		WR4(sc, NPE_MAC_ADDR_MASK(i), mask[i]);
		WR4(sc, NPE_MAC_ADDR(i), addr[i]);
	}
}

static void
npe_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct npe_softc *sc;

	if (error != 0)
		return;
	sc = (struct npe_softc *)arg;
	sc->buf_phys = segs[0].ds_addr;
}

static int
npe_dma_setup(struct npe_softc *sc, struct npedma *dma,
	const char *name, int nbuf, int maxseg)
{
	int error, i;

	memset(dma, 0, sizeof(*dma));

	dma->name = name;
	dma->nbuf = nbuf;

	/* DMA tag for mapped mbufs  */
	error = bus_dma_tag_create(ixp425_softc->sc_dmat, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MCLBYTES, maxseg, MCLBYTES, 0,
	    busdma_lock_mutex, &sc->sc_mtx, &dma->mtag);
	if (error != 0) {
		device_printf(sc->sc_dev, "unable to create %s mbuf dma tag, "
		     "error %u\n", dma->name, error);
		return error;
	}

	/* DMA tag and map for the NPE buffers */
	error = bus_dma_tag_create(ixp425_softc->sc_dmat, sizeof(uint32_t), 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    nbuf * sizeof(struct npehwbuf), 1,
	    nbuf * sizeof(struct npehwbuf), 0,
	    busdma_lock_mutex, &sc->sc_mtx, &dma->buf_tag);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "unable to create %s npebuf dma tag, error %u\n",
		    dma->name, error);
		return error;
	}
	if (bus_dmamem_alloc(dma->buf_tag, (void **)&dma->hwbuf,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &dma->buf_map) != 0) {
		device_printf(sc->sc_dev,
		     "unable to allocate memory for %s h/w buffers, error %u\n",
		     dma->name, error);
		return error;
	}
	/* XXX M_TEMP */
	dma->buf = malloc(nbuf * sizeof(struct npebuf), M_TEMP, M_NOWAIT | M_ZERO);
	if (dma->buf == NULL) {
		device_printf(sc->sc_dev,
		     "unable to allocate memory for %s s/w buffers\n",
		     dma->name);
		return error;
	}
	if (bus_dmamap_load(dma->buf_tag, dma->buf_map,
	    dma->hwbuf, nbuf*sizeof(struct npehwbuf), npe_getaddr, sc, 0) != 0) {
		device_printf(sc->sc_dev,
		     "unable to map memory for %s h/w buffers, error %u\n",
		     dma->name, error);
		return error;
	}
	dma->buf_phys = sc->buf_phys;
	for (i = 0; i < dma->nbuf; i++) {
		struct npebuf *npe = &dma->buf[i];
		struct npehwbuf *hw = &dma->hwbuf[i];

		/* calculate offset to shared area */
		npe->ix_neaddr = dma->buf_phys +
			((uintptr_t)hw - (uintptr_t)dma->hwbuf);
		KASSERT((npe->ix_neaddr & 0x1f) == 0,
		    ("ixpbuf misaligned, PA 0x%x", npe->ix_neaddr));
		error = bus_dmamap_create(dma->mtag, BUS_DMA_NOWAIT,
				&npe->ix_map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			     "unable to create dmamap for %s buffer %u, "
			     "error %u\n", dma->name, i, error);
			return error;
		}
		npe->ix_hw = hw;
	}
	bus_dmamap_sync(dma->buf_tag, dma->buf_map, BUS_DMASYNC_PREWRITE);
	return 0;
}

static void
npe_dma_destroy(struct npe_softc *sc, struct npedma *dma)
{
	int i;

	if (dma->hwbuf != NULL) {
		for (i = 0; i < dma->nbuf; i++) {
			struct npebuf *npe = &dma->buf[i];
			bus_dmamap_destroy(dma->mtag, npe->ix_map);
		}
		bus_dmamap_unload(dma->buf_tag, dma->buf_map);
		bus_dmamem_free(dma->buf_tag, dma->hwbuf, dma->buf_map);
	}
	if (dma->buf != NULL)
		free(dma->buf, M_TEMP);
	if (dma->buf_tag)
		bus_dma_tag_destroy(dma->buf_tag);
	if (dma->mtag)
		bus_dma_tag_destroy(dma->mtag);
	memset(dma, 0, sizeof(*dma));
}

static int
override_addr(device_t dev, const char *resname, int *base)
{
	int unit = device_get_unit(dev);
	const char *resval;

	/* XXX warn for wrong hint type */
	if (resource_string_value("npe", unit, resname, &resval) != 0)
		return 0;
	switch (resval[0]) {
	case 'A':
		*base = IXP435_MAC_A_HWBASE;
		break;
	case 'B':
		*base = IXP425_MAC_B_HWBASE;
		break;
	case 'C':
		*base = IXP425_MAC_C_HWBASE;
		break;
	default:
		device_printf(dev, "Warning, bad value %s for "
		    "npe.%d.%s ignored\n", resval, unit, resname);
		return 0;
	}
	if (bootverbose)
		device_printf(dev, "using npe.%d.%s=%s override\n",
		    unit, resname, resval);
	return 1;
}

static int
override_npeid(device_t dev, const char *resname, int *npeid)
{
	int unit = device_get_unit(dev);
	const char *resval;

	/* XXX warn for wrong hint type */
	if (resource_string_value("npe", unit, resname, &resval) != 0)
		return 0;
	switch (resval[0]) {
	case 'A': *npeid = NPE_A; break;
	case 'B': *npeid = NPE_B; break;
	case 'C': *npeid = NPE_C; break;
	default:
		device_printf(dev, "Warning, bad value %s for "
		    "npe.%d.%s ignored\n", resval, unit, resname);
		return 0;
	}
	if (bootverbose)
		device_printf(dev, "using npe.%d.%s=%s override\n",
		    unit, resname, resval);
	return 1;
}

static int
override_unit(device_t dev, const char *resname, int *val, int min, int max)
{
	int unit = device_get_unit(dev);
	int resval;

	if (resource_int_value("npe", unit, resname, &resval) != 0)
		return 0;
	if (!(min <= resval && resval <= max)) {
		device_printf(dev, "Warning, bad value %d for npe.%d.%s "
		    "ignored (value must be [%d-%d])\n", resval, unit,
		    resname, min, max);
		return 0;
	}
	if (bootverbose)
		device_printf(dev, "using npe.%d.%s=%d override\n",
		    unit, resname, resval);
	*val = resval;
	return 1;
}

static void
npe_mac_reset(struct npe_softc *sc)
{
	/*
	 * Reset MAC core.
	 */
	WR4(sc, NPE_MAC_CORE_CNTRL, NPE_CORE_RESET);
	DELAY(NPE_MAC_RESET_DELAY);
	/* configure MAC to generate MDC clock */
	WR4(sc, NPE_MAC_CORE_CNTRL, NPE_CORE_MDC_EN);
}

static int
npe_activate(device_t dev)
{
	struct npe_softc *sc = device_get_softc(dev);
	int error, i, macbase, miibase, phy;

	/*
	 * Setup NEP ID, MAC, and MII bindings.  We allow override
	 * via hints to handle unexpected board configs.
	 */
	if (!override_npeid(dev, "npeid", &sc->sc_npeid))
		sc->sc_npeid = unit2npeid(device_get_unit(dev));
	sc->sc_npe = ixpnpe_attach(dev, sc->sc_npeid);
	if (sc->sc_npe == NULL) {
		device_printf(dev, "cannot attach ixpnpe\n");
		return EIO;		/* XXX */
	}

	/* MAC */
	if (!override_addr(dev, "mac", &macbase))
		macbase = npeconfig[sc->sc_npeid].macbase;
	device_printf(sc->sc_dev, "MAC at 0x%x\n", macbase);
	if (bus_space_map(sc->sc_iot, macbase, IXP425_REG_SIZE, 0, &sc->sc_ioh)) {
		device_printf(dev, "cannot map mac registers 0x%x:0x%x\n",
		    macbase, IXP425_REG_SIZE);
		return ENOMEM;
	}

	/* PHY */
	if (!override_unit(dev, "phy", &phy, 0, MII_NPHY - 1))
		phy = npeconfig[sc->sc_npeid].phy;
	if (!override_addr(dev, "mii", &miibase))
		miibase = npeconfig[sc->sc_npeid].miibase;
	device_printf(sc->sc_dev, "MII at 0x%x\n", miibase);
	if (miibase != macbase) {
		/*
		 * PHY is mapped through a different MAC, setup an
		 * additional mapping for frobbing the PHY registers.
		 */
		if (bus_space_map(sc->sc_iot, miibase, IXP425_REG_SIZE, 0, &sc->sc_miih)) {
			device_printf(dev,
			    "cannot map MII registers 0x%x:0x%x\n",
			    miibase, IXP425_REG_SIZE);
			return ENOMEM;
		}
	} else
		sc->sc_miih = sc->sc_ioh;

	/*
	 * Load NPE firmware and start it running.
	 */
	error = ixpnpe_init(sc->sc_npe);
	if (error != 0) {
		device_printf(dev, "cannot init NPE (error %d)\n", error);
		return error;
	}

	/* attach PHY */
	error = mii_attach(dev, &sc->sc_mii, sc->sc_ifp, npe_ifmedia_update,
	    npe_ifmedia_status, BMSR_DEFCAPMASK, phy, MII_OFFSET_ANY, 0);
	if (error != 0) {
		device_printf(dev, "attaching PHYs failed\n");
		return error;
	}

	error = npe_dma_setup(sc, &sc->txdma, "tx", npe_txbuf, NPE_MAXSEG);
	if (error != 0)
		return error;
	error = npe_dma_setup(sc, &sc->rxdma, "rx", npe_rxbuf, 1);
	if (error != 0)
		return error;

	/* setup statistics block */
	error = bus_dma_tag_create(ixp425_softc->sc_dmat, sizeof(uint32_t), 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct npestats), 1, sizeof(struct npestats), 0,
	    busdma_lock_mutex, &sc->sc_mtx, &sc->sc_stats_tag);
	if (error != 0) {
		device_printf(sc->sc_dev, "unable to create stats tag, "
		     "error %u\n", error);
		return error;
	}
	if (bus_dmamem_alloc(sc->sc_stats_tag, (void **)&sc->sc_stats,
	    BUS_DMA_NOWAIT, &sc->sc_stats_map) != 0) {
		device_printf(sc->sc_dev,
		     "unable to allocate memory for stats block, error %u\n",
		     error);
		return error;
	}
	if (bus_dmamap_load(sc->sc_stats_tag, sc->sc_stats_map,
	    sc->sc_stats, sizeof(struct npestats), npe_getaddr, sc, 0) != 0) {
		device_printf(sc->sc_dev,
		     "unable to load memory for stats block, error %u\n",
		     error);
		return error;
	}
	sc->sc_stats_phys = sc->buf_phys;

	/*
	 * Setup h/w rx/tx queues.  There are four q's:
	 *   rx		inbound q of rx'd frames
	 *   rx_free	pool of ixpbuf's for receiving frames
	 *   tx		outbound q of frames to send
	 *   tx_done	q of tx frames that have been processed
	 *
	 * The NPE handles the actual tx/rx process and the q manager
	 * handles the queues.  The driver just writes entries to the
	 * q manager mailbox's and gets callbacks when there are rx'd
	 * frames to process or tx'd frames to reap.  These callbacks
	 * are controlled by the q configurations; e.g. we get a
	 * callback when tx_done has 2 or more frames to process and
	 * when the rx q has at least one frame.  These setings can
	 * changed at the time the q is configured.
	 */
	sc->rx_qid = npeconfig[sc->sc_npeid].rx_qid;
	ixpqmgr_qconfig(sc->rx_qid, npe_rxbuf, 0,  1,
		IX_QMGR_Q_SOURCE_ID_NOT_E, (qconfig_hand_t *)npe_rxdone, sc);
	sc->rx_freeqid = npeconfig[sc->sc_npeid].rx_freeqid;
	ixpqmgr_qconfig(sc->rx_freeqid,	npe_rxbuf, 0, npe_rxbuf/2, 0, NULL, sc);
	/*
	 * Setup the NPE to direct all traffic to rx_qid.
	 * When QoS is enabled in the firmware there are
	 * 8 traffic classes; otherwise just 4.
	 */
	for (i = 0; i < 8; i++)
		npe_setrxqosentry(sc, i, 0, sc->rx_qid);

	/* disable firewall mode just in case (should be off) */
	npe_setfirewallmode(sc, 0);

	sc->tx_qid = npeconfig[sc->sc_npeid].tx_qid;
	sc->tx_doneqid = npeconfig[sc->sc_npeid].tx_doneqid;
	ixpqmgr_qconfig(sc->tx_qid, npe_txbuf, 0, npe_txbuf, 0, NULL, sc);
	if (tx_doneqid == -1) {
		ixpqmgr_qconfig(sc->tx_doneqid,	npe_txbuf, 0,  2,
			IX_QMGR_Q_SOURCE_ID_NOT_E, npe_txdone, sc);
		tx_doneqid = sc->tx_doneqid;
	}

	KASSERT(npes[sc->sc_npeid] == NULL,
	    ("npe %u already setup", sc->sc_npeid));
	npes[sc->sc_npeid] = sc;

	return 0;
}

static void
npe_deactivate(device_t dev)
{
	struct npe_softc *sc = device_get_softc(dev);

	npes[sc->sc_npeid] = NULL;

	/* XXX disable q's */
	if (sc->sc_npe != NULL) {
		ixpnpe_stop(sc->sc_npe);
		ixpnpe_detach(sc->sc_npe);
	}
	if (sc->sc_stats != NULL) {
		bus_dmamap_unload(sc->sc_stats_tag, sc->sc_stats_map);
		bus_dmamem_free(sc->sc_stats_tag, sc->sc_stats,
			sc->sc_stats_map);
	}
	if (sc->sc_stats_tag != NULL)
		bus_dma_tag_destroy(sc->sc_stats_tag);
	npe_dma_destroy(sc, &sc->txdma);
	npe_dma_destroy(sc, &sc->rxdma);
	bus_generic_detach(sc->sc_dev);
	if (sc->sc_mii != NULL)
		device_delete_child(sc->sc_dev, sc->sc_mii);
}

/*
 * Change media according to request.
 */
static int
npe_ifmedia_update(struct ifnet *ifp)
{
	struct npe_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->sc_mii);
	NPE_LOCK(sc);
	mii_mediachg(mii);
	/* XXX push state ourself? */
	NPE_UNLOCK(sc);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
static void
npe_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct npe_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->sc_mii);
	NPE_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	NPE_UNLOCK(sc);
}

static void
npe_addstats(struct npe_softc *sc)
{
#define	NPEADD(x)	sc->sc_totals.x += be32toh(ns->x)
#define	MIBADD(x) do { sc->mibdata.x += be32toh(ns->x); NPEADD(x); } while (0)
	struct ifnet *ifp = sc->sc_ifp;
	struct npestats *ns = sc->sc_stats;

	MIBADD(dot3StatsAlignmentErrors);
	MIBADD(dot3StatsFCSErrors);
	MIBADD(dot3StatsInternalMacReceiveErrors);
	NPEADD(RxOverrunDiscards);
	NPEADD(RxLearnedEntryDiscards);
	NPEADD(RxLargeFramesDiscards);
	NPEADD(RxSTPBlockedDiscards);
	NPEADD(RxVLANTypeFilterDiscards);
	NPEADD(RxVLANIdFilterDiscards);
	NPEADD(RxInvalidSourceDiscards);
	NPEADD(RxBlackListDiscards);
	NPEADD(RxWhiteListDiscards);
	NPEADD(RxUnderflowEntryDiscards);
	MIBADD(dot3StatsSingleCollisionFrames);
	MIBADD(dot3StatsMultipleCollisionFrames);
	MIBADD(dot3StatsDeferredTransmissions);
	MIBADD(dot3StatsLateCollisions);
	MIBADD(dot3StatsExcessiveCollisions);
	MIBADD(dot3StatsInternalMacTransmitErrors);
	MIBADD(dot3StatsCarrierSenseErrors);
	NPEADD(TxLargeFrameDiscards);
	NPEADD(TxVLANIdFilterDiscards);

	sc->mibdata.dot3StatsFrameTooLongs +=
	      be32toh(ns->RxLargeFramesDiscards)
	    + be32toh(ns->TxLargeFrameDiscards);
	sc->mibdata.dot3StatsMissedFrames +=
	      be32toh(ns->RxOverrunDiscards)
	    + be32toh(ns->RxUnderflowEntryDiscards);

	ifp->if_oerrors +=
		  be32toh(ns->dot3StatsInternalMacTransmitErrors)
		+ be32toh(ns->dot3StatsCarrierSenseErrors)
		+ be32toh(ns->TxVLANIdFilterDiscards)
		;
	ifp->if_ierrors += be32toh(ns->dot3StatsFCSErrors)
		+ be32toh(ns->dot3StatsInternalMacReceiveErrors)
		+ be32toh(ns->RxOverrunDiscards)
		+ be32toh(ns->RxUnderflowEntryDiscards)
		;
	ifp->if_collisions +=
		  be32toh(ns->dot3StatsSingleCollisionFrames)
		+ be32toh(ns->dot3StatsMultipleCollisionFrames)
		;
#undef NPEADD
#undef MIBADD
}

static void
npe_tick(void *xsc)
{
#define	ACK	(NPE_RESETSTATS << NPE_MAC_MSGID_SHL)
	struct npe_softc *sc = xsc;
	struct mii_data *mii = device_get_softc(sc->sc_mii);
	uint32_t msg[2];

	NPE_ASSERT_LOCKED(sc);

	/*
	 * NB: to avoid sleeping with the softc lock held we
	 * split the NPE msg processing into two parts.  The
	 * request for statistics is sent w/o waiting for a
	 * reply and then on the next tick we retrieve the
	 * results.  This works because npe_tick is the only
	 * code that talks via the mailbox's (except at setup).
	 * This likely can be handled better.
	 */
	if (ixpnpe_recvmsg_async(sc->sc_npe, msg) == 0 && msg[0] == ACK) {
		bus_dmamap_sync(sc->sc_stats_tag, sc->sc_stats_map,
		    BUS_DMASYNC_POSTREAD);
		npe_addstats(sc);
	}
	npe_updatestats(sc);
	mii_tick(mii);

	npewatchdog(sc);

	/* schedule next poll */
	callout_reset(&sc->tick_ch, sc->sc_tickinterval * hz, npe_tick, sc);
#undef ACK
}

static void
npe_setmac(struct npe_softc *sc, u_char *eaddr)
{
	WR4(sc, NPE_MAC_UNI_ADDR_1, eaddr[0]);
	WR4(sc, NPE_MAC_UNI_ADDR_2, eaddr[1]);
	WR4(sc, NPE_MAC_UNI_ADDR_3, eaddr[2]);
	WR4(sc, NPE_MAC_UNI_ADDR_4, eaddr[3]);
	WR4(sc, NPE_MAC_UNI_ADDR_5, eaddr[4]);
	WR4(sc, NPE_MAC_UNI_ADDR_6, eaddr[5]);
}

static void
npe_getmac(struct npe_softc *sc, u_char *eaddr)
{
	/* NB: the unicast address appears to be loaded from EEPROM on reset */
	eaddr[0] = RD4(sc, NPE_MAC_UNI_ADDR_1) & 0xff;
	eaddr[1] = RD4(sc, NPE_MAC_UNI_ADDR_2) & 0xff;
	eaddr[2] = RD4(sc, NPE_MAC_UNI_ADDR_3) & 0xff;
	eaddr[3] = RD4(sc, NPE_MAC_UNI_ADDR_4) & 0xff;
	eaddr[4] = RD4(sc, NPE_MAC_UNI_ADDR_5) & 0xff;
	eaddr[5] = RD4(sc, NPE_MAC_UNI_ADDR_6) & 0xff;
}

struct txdone {
	struct npebuf *head;
	struct npebuf **tail;
	int count;
};

static __inline void
npe_txdone_finish(struct npe_softc *sc, const struct txdone *td)
{
	struct ifnet *ifp = sc->sc_ifp;

	NPE_LOCK(sc);
	*td->tail = sc->tx_free;
	sc->tx_free = td->head;
	/*
	 * We're no longer busy, so clear the busy flag and call the
	 * start routine to xmit more packets.
	 */
	ifp->if_opackets += td->count;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->npe_watchdog_timer = 0;
	npestart_locked(ifp);
	NPE_UNLOCK(sc);
}

/*
 * Q manager callback on tx done queue.  Reap mbufs
 * and return tx buffers to the free list.  Finally
 * restart output.  Note the microcode has only one
 * txdone q wired into it so we must use the NPE ID
 * returned with each npehwbuf to decide where to
 * send buffers.
 */
static void
npe_txdone(int qid, void *arg)
{
#define	P2V(a, dma) \
	&(dma)->buf[((a) - (dma)->buf_phys) / sizeof(struct npehwbuf)]
	struct npe_softc *sc0 = arg;
	struct npe_softc *sc;
	struct npebuf *npe;
	struct txdone *td, q[NPE_MAX];
	uint32_t entry;

	q[NPE_A].tail = &q[NPE_A].head; q[NPE_A].count = 0;
	q[NPE_B].tail = &q[NPE_B].head; q[NPE_B].count = 0;
	q[NPE_C].tail = &q[NPE_C].head; q[NPE_C].count = 0;
	/* XXX max # at a time? */
	while (ixpqmgr_qread(qid, &entry) == 0) {
		DPRINTF(sc0, "%s: entry 0x%x NPE %u port %u\n",
		    __func__, entry, NPE_QM_Q_NPE(entry), NPE_QM_Q_PORT(entry));

		sc = npes[NPE_QM_Q_NPE(entry)];
		npe = P2V(NPE_QM_Q_ADDR(entry), &sc->txdma);
		m_freem(npe->ix_m);
		npe->ix_m = NULL;

		td = &q[NPE_QM_Q_NPE(entry)];
		*td->tail = npe;
		td->tail = &npe->ix_next;
		td->count++;
	}

	if (q[NPE_A].count)
		npe_txdone_finish(npes[NPE_A], &q[NPE_A]);
	if (q[NPE_B].count)
		npe_txdone_finish(npes[NPE_B], &q[NPE_B]);
	if (q[NPE_C].count)
		npe_txdone_finish(npes[NPE_C], &q[NPE_C]);
#undef P2V
}

static int
npe_rxbuf_init(struct npe_softc *sc, struct npebuf *npe, struct mbuf *m)
{
	bus_dma_segment_t segs[1];
	struct npedma *dma = &sc->rxdma;
	struct npehwbuf *hw;
	int error, nseg;

	if (m == NULL) {
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			return ENOBUFS;
	}
	KASSERT(m->m_ext.ext_size >= 1536 + ETHER_ALIGN,
		("ext_size %d", m->m_ext.ext_size));
	m->m_pkthdr.len = m->m_len = 1536;
	/* backload payload and align ip hdr */
	m->m_data = m->m_ext.ext_buf + (m->m_ext.ext_size - (1536+ETHER_ALIGN));
	bus_dmamap_unload(dma->mtag, npe->ix_map);
	error = bus_dmamap_load_mbuf_sg(dma->mtag, npe->ix_map, m,
			segs, &nseg, 0);
	if (error != 0) {
		m_freem(m);
		return error;
	}
	hw = npe->ix_hw;
	hw->ix_ne[0].data = htobe32(segs[0].ds_addr);
	/* NB: NPE requires length be a multiple of 64 */
	/* NB: buffer length is shifted in word */
	hw->ix_ne[0].len = htobe32(segs[0].ds_len << 16);
	hw->ix_ne[0].next = 0;
	bus_dmamap_sync(dma->buf_tag, dma->buf_map, 
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	npe->ix_m = m;
	/* Flush the memory in the mbuf */
	bus_dmamap_sync(dma->mtag, npe->ix_map, BUS_DMASYNC_PREREAD);
	return 0;
}

/*
 * RX q processing for a specific NPE.  Claim entries
 * from the hardware queue and pass the frames up the
 * stack. Pass the rx buffers to the free list.
 */
static int
npe_rxdone(int qid, void *arg)
{
#define	P2V(a, dma) \
	&(dma)->buf[((a) - (dma)->buf_phys) / sizeof(struct npehwbuf)]
	struct npe_softc *sc = arg;
	struct npedma *dma = &sc->rxdma;
	uint32_t entry;
	int rx_npkts = 0;

	while (ixpqmgr_qread(qid, &entry) == 0) {
		struct npebuf *npe = P2V(NPE_QM_Q_ADDR(entry), dma);
		struct mbuf *m;

		bus_dmamap_sync(dma->buf_tag, dma->buf_map,
		    BUS_DMASYNC_POSTREAD);
		DPRINTF(sc, "%s: entry 0x%x neaddr 0x%x ne_len 0x%x\n",
		    __func__, entry, npe->ix_neaddr, npe->ix_hw->ix_ne[0].len);
		/*
		 * Allocate a new mbuf to replenish the rx buffer.
		 * If doing so fails we drop the rx'd frame so we
		 * can reuse the previous mbuf.  When we're able to
		 * allocate a new mbuf dispatch the mbuf w/ rx'd
		 * data up the stack and replace it with the newly
		 * allocated one.
		 */
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m != NULL) {
			struct mbuf *mrx = npe->ix_m;
			struct npehwbuf *hw = npe->ix_hw;
			struct ifnet *ifp = sc->sc_ifp;

			/* Flush mbuf memory for rx'd data */
			bus_dmamap_sync(dma->mtag, npe->ix_map,
			    BUS_DMASYNC_POSTREAD);

			/* set m_len etc. per rx frame size */
			mrx->m_len = be32toh(hw->ix_ne[0].len) & 0xffff;
			mrx->m_pkthdr.len = mrx->m_len;
			mrx->m_pkthdr.rcvif = ifp;

			ifp->if_ipackets++;
			ifp->if_input(ifp, mrx);
			rx_npkts++;
		} else {
			/* discard frame and re-use mbuf */
			m = npe->ix_m;
		}
		if (npe_rxbuf_init(sc, npe, m) == 0) {
			/* return npe buf to rx free list */
			ixpqmgr_qwrite(sc->rx_freeqid, npe->ix_neaddr);
		} else {
			/* XXX should not happen */
		}
	}
	return rx_npkts;
#undef P2V
}

#ifdef DEVICE_POLLING
static int
npe_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct npe_softc *sc = ifp->if_softc;
	int rx_npkts = 0;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		rx_npkts = npe_rxdone(sc->rx_qid, sc);
		npe_txdone(sc->tx_doneqid, sc);	/* XXX polls both NPE's */
	}
	return rx_npkts;
}
#endif /* DEVICE_POLLING */

static void
npe_startxmit(struct npe_softc *sc)
{
	struct npedma *dma = &sc->txdma;
	int i;

	NPE_ASSERT_LOCKED(sc);
	sc->tx_free = NULL;
	for (i = 0; i < dma->nbuf; i++) {
		struct npebuf *npe = &dma->buf[i];
		if (npe->ix_m != NULL) {
			/* NB: should not happen */
			device_printf(sc->sc_dev,
			    "%s: free mbuf at entry %u\n", __func__, i);
			m_freem(npe->ix_m);
		}
		npe->ix_m = NULL;
		npe->ix_next = sc->tx_free;
		sc->tx_free = npe;
	}
}

static void
npe_startrecv(struct npe_softc *sc)
{
	struct npedma *dma = &sc->rxdma;
	struct npebuf *npe;
	int i;

	NPE_ASSERT_LOCKED(sc);
	for (i = 0; i < dma->nbuf; i++) {
		npe = &dma->buf[i];
		npe_rxbuf_init(sc, npe, npe->ix_m);
		/* set npe buf on rx free list */
		ixpqmgr_qwrite(sc->rx_freeqid, npe->ix_neaddr);
	}
}

/*
 * Reset and initialize the chip
 */
static void
npeinit_locked(void *xsc)
{
	struct npe_softc *sc = xsc;
	struct ifnet *ifp = sc->sc_ifp;

	NPE_ASSERT_LOCKED(sc);
if (ifp->if_drv_flags & IFF_DRV_RUNNING) return;/*XXX*/

	/*
	 * Reset MAC core.
	 */
	npe_mac_reset(sc);

	/* disable transmitter and reciver in the MAC */
 	WR4(sc, NPE_MAC_RX_CNTRL1,
	    RD4(sc, NPE_MAC_RX_CNTRL1) &~ NPE_RX_CNTRL1_RX_EN);
 	WR4(sc, NPE_MAC_TX_CNTRL1,
	    RD4(sc, NPE_MAC_TX_CNTRL1) &~ NPE_TX_CNTRL1_TX_EN);

	/*
	 * Set the MAC core registers.
	 */
	WR4(sc, NPE_MAC_INT_CLK_THRESH, 0x1);	/* clock ratio: for ipx4xx */
	WR4(sc, NPE_MAC_TX_CNTRL2,	0xf);	/* max retries */
	WR4(sc, NPE_MAC_RANDOM_SEED,	0x8);	/* LFSR back-off seed */
	/* thresholds determined by NPE firmware FS */
	WR4(sc, NPE_MAC_THRESH_P_EMPTY,	0x12);
	WR4(sc, NPE_MAC_THRESH_P_FULL,	0x30);
	WR4(sc, NPE_MAC_BUF_SIZE_TX,	0x8);	/* tx fifo threshold (bytes) */
	WR4(sc, NPE_MAC_TX_DEFER,	0x15);	/* for single deferral */
	WR4(sc, NPE_MAC_RX_DEFER,	0x16);	/* deferral on inter-frame gap*/
	WR4(sc, NPE_MAC_TX_TWO_DEFER_1,	0x8);	/* for 2-part deferral */
	WR4(sc, NPE_MAC_TX_TWO_DEFER_2,	0x7);	/* for 2-part deferral */
	WR4(sc, NPE_MAC_SLOT_TIME,	0x80);	/* assumes MII mode */

	WR4(sc, NPE_MAC_TX_CNTRL1,
		  NPE_TX_CNTRL1_RETRY		/* retry failed xmits */
		| NPE_TX_CNTRL1_FCS_EN		/* append FCS */
		| NPE_TX_CNTRL1_2DEFER		/* 2-part deferal */
		| NPE_TX_CNTRL1_PAD_EN);	/* pad runt frames */
	/* XXX pad strip? */
	/* ena pause frame handling */
	WR4(sc, NPE_MAC_RX_CNTRL1, NPE_RX_CNTRL1_PAUSE_EN);
	WR4(sc, NPE_MAC_RX_CNTRL2, 0);

	npe_setmac(sc, IF_LLADDR(ifp));
	npe_setportaddress(sc, IF_LLADDR(ifp));
	npe_setmcast(sc);

	npe_startxmit(sc);
	npe_startrecv(sc);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->npe_watchdog_timer = 0;		/* just in case */

	/* enable transmitter and reciver in the MAC */
 	WR4(sc, NPE_MAC_RX_CNTRL1,
	    RD4(sc, NPE_MAC_RX_CNTRL1) | NPE_RX_CNTRL1_RX_EN);
 	WR4(sc, NPE_MAC_TX_CNTRL1,
	    RD4(sc, NPE_MAC_TX_CNTRL1) | NPE_TX_CNTRL1_TX_EN);

	callout_reset(&sc->tick_ch, sc->sc_tickinterval * hz, npe_tick, sc);
}

static void
npeinit(void *xsc)
{
	struct npe_softc *sc = xsc;
	NPE_LOCK(sc);
	npeinit_locked(sc);
	NPE_UNLOCK(sc);
}

/*
 * Dequeue packets and place on the h/w transmit queue.
 */
static void
npestart_locked(struct ifnet *ifp)
{
	struct npe_softc *sc = ifp->if_softc;
	struct npebuf *npe;
	struct npehwbuf *hw;
	struct mbuf *m, *n;
	struct npedma *dma = &sc->txdma;
	bus_dma_segment_t segs[NPE_MAXSEG];
	int nseg, len, error, i;
	uint32_t next;

	NPE_ASSERT_LOCKED(sc);
	/* XXX can this happen? */
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	while (sc->tx_free != NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			/* XXX? */
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
			return;
		}
		npe = sc->tx_free;
		bus_dmamap_unload(dma->mtag, npe->ix_map);
		error = bus_dmamap_load_mbuf_sg(dma->mtag, npe->ix_map,
		    m, segs, &nseg, 0);
		if (error == EFBIG) {
			n = m_collapse(m, M_NOWAIT, NPE_MAXSEG);
			if (n == NULL) {
				if_printf(ifp, "%s: too many fragments %u\n",
				    __func__, nseg);
				m_freem(m);
				return;	/* XXX? */
			}
			m = n;
			error = bus_dmamap_load_mbuf_sg(dma->mtag, npe->ix_map,
			    m, segs, &nseg, 0);
		}
		if (error != 0 || nseg == 0) {
			if_printf(ifp, "%s: error %u nseg %u\n",
			    __func__, error, nseg);
			m_freem(m);
			return;	/* XXX? */
		}
		sc->tx_free = npe->ix_next;

		bus_dmamap_sync(dma->mtag, npe->ix_map, BUS_DMASYNC_PREWRITE);
	
		/*
		 * Tap off here if there is a bpf listener.
		 */
		BPF_MTAP(ifp, m);

		npe->ix_m = m;
		hw = npe->ix_hw;
		len = m->m_pkthdr.len;
		next = npe->ix_neaddr + sizeof(hw->ix_ne[0]);
		for (i = 0; i < nseg; i++) {
			hw->ix_ne[i].data = htobe32(segs[i].ds_addr);
			hw->ix_ne[i].len = htobe32((segs[i].ds_len<<16) | len);
			hw->ix_ne[i].next = htobe32(next);

			len = 0;		/* zero for segments > 1 */
			next += sizeof(hw->ix_ne[0]);
		}
		hw->ix_ne[i-1].next = 0;	/* zero last in chain */
		bus_dmamap_sync(dma->buf_tag, dma->buf_map,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		DPRINTF(sc, "%s: qwrite(%u, 0x%x) ne_data %x ne_len 0x%x\n",
		    __func__, sc->tx_qid, npe->ix_neaddr,
		    hw->ix_ne[0].data, hw->ix_ne[0].len);
		/* stick it on the tx q */
		/* XXX add vlan priority */
		ixpqmgr_qwrite(sc->tx_qid, npe->ix_neaddr);

		sc->npe_watchdog_timer = 5;
	}
	if (sc->tx_free == NULL)
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
}

void
npestart(struct ifnet *ifp)
{
	struct npe_softc *sc = ifp->if_softc;
	NPE_LOCK(sc);
	npestart_locked(ifp);
	NPE_UNLOCK(sc);
}

static void
npe_stopxmit(struct npe_softc *sc)
{
	struct npedma *dma = &sc->txdma;
	int i;

	NPE_ASSERT_LOCKED(sc);

	/* XXX qmgr */
	for (i = 0; i < dma->nbuf; i++) {
		struct npebuf *npe = &dma->buf[i];

		if (npe->ix_m != NULL) {
			bus_dmamap_unload(dma->mtag, npe->ix_map);
			m_freem(npe->ix_m);
			npe->ix_m = NULL;
		}
	}
}

static void
npe_stoprecv(struct npe_softc *sc)
{
	struct npedma *dma = &sc->rxdma;
	int i;

	NPE_ASSERT_LOCKED(sc);

	/* XXX qmgr */
	for (i = 0; i < dma->nbuf; i++) {
		struct npebuf *npe = &dma->buf[i];

		if (npe->ix_m != NULL) {
			bus_dmamap_unload(dma->mtag, npe->ix_map);
			m_freem(npe->ix_m);
			npe->ix_m = NULL;
		}
	}
}

/*
 * Turn off interrupts, and stop the nic.
 */
void
npestop(struct npe_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	/*  disable transmitter and reciver in the MAC  */
 	WR4(sc, NPE_MAC_RX_CNTRL1,
	    RD4(sc, NPE_MAC_RX_CNTRL1) &~ NPE_RX_CNTRL1_RX_EN);
 	WR4(sc, NPE_MAC_TX_CNTRL1,
	    RD4(sc, NPE_MAC_TX_CNTRL1) &~ NPE_TX_CNTRL1_TX_EN);

	sc->npe_watchdog_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->tick_ch);

	npe_stopxmit(sc);
	npe_stoprecv(sc);
	/* XXX go into loopback & drain q's? */
	/* XXX but beware of disabling tx above */

	/*
	 * The MAC core rx/tx disable may leave the MAC hardware in an
	 * unpredictable state. A hw reset is executed before resetting
	 * all the MAC parameters to a known value.
	 */
	WR4(sc, NPE_MAC_CORE_CNTRL, NPE_CORE_RESET);
	DELAY(NPE_MAC_RESET_DELAY);
	WR4(sc, NPE_MAC_INT_CLK_THRESH, NPE_MAC_INT_CLK_THRESH_DEFAULT);
	WR4(sc, NPE_MAC_CORE_CNTRL, NPE_CORE_MDC_EN);
}

void
npewatchdog(struct npe_softc *sc)
{
	NPE_ASSERT_LOCKED(sc);

	if (sc->npe_watchdog_timer == 0 || --sc->npe_watchdog_timer != 0)
		return;

	device_printf(sc->sc_dev, "watchdog timeout\n");
	sc->sc_ifp->if_oerrors++;

	npeinit_locked(sc);
}

static int
npeioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct npe_softc *sc = ifp->if_softc;
 	struct mii_data *mii;
 	struct ifreq *ifr = (struct ifreq *)data;	
	int error = 0;
#ifdef DEVICE_POLLING
	int mask;
#endif

	switch (cmd) {
	case SIOCSIFFLAGS:
		NPE_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_drv_flags & IFF_DRV_RUNNING) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			npestop(sc);
		} else {
			/* reinitialize card on any parameter change */
			npeinit_locked(sc);
		}
		NPE_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* update multicast filter list. */
		NPE_LOCK(sc);
		npe_setmcast(sc);
		NPE_UNLOCK(sc);
		error = 0;
		break;

  	case SIOCSIFMEDIA:
  	case SIOCGIFMEDIA:
 		mii = device_get_softc(sc->sc_mii);
 		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
  		break;

#ifdef DEVICE_POLLING
	case SIOCSIFCAP:
		mask = ifp->if_capenable ^ ifr->ifr_reqcap;
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(npe_poll, ifp);
				if (error)
					return error;
				NPE_LOCK(sc);
				/* disable callbacks XXX txdone is shared */
				ixpqmgr_notify_disable(sc->rx_qid);
				ixpqmgr_notify_disable(sc->tx_doneqid);
				ifp->if_capenable |= IFCAP_POLLING;
				NPE_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* NB: always enable qmgr callbacks */
				NPE_LOCK(sc);
				/* enable qmgr callbacks */
				ixpqmgr_notify_enable(sc->rx_qid,
				    IX_QMGR_Q_SOURCE_ID_NOT_E);
				ixpqmgr_notify_enable(sc->tx_doneqid,
				    IX_QMGR_Q_SOURCE_ID_NOT_E);
				ifp->if_capenable &= ~IFCAP_POLLING;
				NPE_UNLOCK(sc);
			}
		}
		break;
#endif
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return error;
}

/*
 * Setup a traffic class -> rx queue mapping.
 */
static int
npe_setrxqosentry(struct npe_softc *sc, int classix, int trafclass, int qid)
{
	uint32_t msg[2];

	msg[0] = (NPE_SETRXQOSENTRY << 24) | (sc->sc_npeid << 20) | classix;
	msg[1] = (trafclass << 24) | (1 << 23) | (qid << 16) | (qid << 4);
	return ixpnpe_sendandrecvmsg_sync(sc->sc_npe, msg, msg);
}

static int
npe_setportaddress(struct npe_softc *sc, const uint8_t mac[ETHER_ADDR_LEN])
{
	uint32_t msg[2];

	msg[0] = (NPE_SETPORTADDRESS << 24)
	       | (sc->sc_npeid << 20)
	       | (mac[0] << 8)
	       | (mac[1] << 0);
	msg[1] = (mac[2] << 24)
	       | (mac[3] << 16)
	       | (mac[4] << 8)
	       | (mac[5] << 0);
	return ixpnpe_sendandrecvmsg_sync(sc->sc_npe, msg, msg);
}

static int
npe_setfirewallmode(struct npe_softc *sc, int onoff)
{
	uint32_t msg[2];

	/* XXX honor onoff */
	msg[0] = (NPE_SETFIREWALLMODE << 24) | (sc->sc_npeid << 20);
	msg[1] = 0;
	return ixpnpe_sendandrecvmsg_sync(sc->sc_npe, msg, msg);
}

/*
 * Update and reset the statistics in the NPE.
 */
static int
npe_updatestats(struct npe_softc *sc)
{
	uint32_t msg[2];

	msg[0] = NPE_RESETSTATS << NPE_MAC_MSGID_SHL;
	msg[1] = sc->sc_stats_phys;	/* physical address of stat block */
	return ixpnpe_sendmsg_async(sc->sc_npe, msg);
}

#if 0
/*
 * Get the current statistics block.
 */
static int
npe_getstats(struct npe_softc *sc)
{
	uint32_t msg[2];

	msg[0] = NPE_GETSTATS << NPE_MAC_MSGID_SHL;
	msg[1] = sc->sc_stats_phys;	/* physical address of stat block */
	return ixpnpe_sendandrecvmsg(sc->sc_npe, msg, msg);
}

/*
 * Query the image id of the loaded firmware.
 */
static uint32_t
npe_getimageid(struct npe_softc *sc)
{
	uint32_t msg[2];

	msg[0] = NPE_GETSTATUS << NPE_MAC_MSGID_SHL;
	msg[1] = 0;
	return ixpnpe_sendandrecvmsg_sync(sc->sc_npe, msg, msg) == 0 ? msg[1] : 0;
}

/*
 * Enable/disable loopback.
 */
static int
npe_setloopback(struct npe_softc *sc, int ena)
{
	uint32_t msg[2];

	msg[0] = (NPE_SETLOOPBACK << NPE_MAC_MSGID_SHL) | (ena != 0);
	msg[1] = 0;
	return ixpnpe_sendandrecvmsg_sync(sc->sc_npe, msg, msg);
}
#endif

static void
npe_child_detached(device_t dev, device_t child)
{
	struct npe_softc *sc;

	sc = device_get_softc(dev);
	if (child == sc->sc_mii)
		sc->sc_mii = NULL;
}

/*
 * MII bus support routines.
 */
#define	MII_RD4(sc, reg)	bus_space_read_4(sc->sc_iot, sc->sc_miih, reg)
#define	MII_WR4(sc, reg, v) \
	bus_space_write_4(sc->sc_iot, sc->sc_miih, reg, v)

static uint32_t
npe_mii_mdio_read(struct npe_softc *sc, int reg)
{
	uint32_t v;

	/* NB: registers are known to be sequential */
	v =  (MII_RD4(sc, reg+0) & 0xff) << 0;
	v |= (MII_RD4(sc, reg+4) & 0xff) << 8;
	v |= (MII_RD4(sc, reg+8) & 0xff) << 16;
	v |= (MII_RD4(sc, reg+12) & 0xff) << 24;
	return v;
}

static void
npe_mii_mdio_write(struct npe_softc *sc, int reg, uint32_t cmd)
{
	/* NB: registers are known to be sequential */
	MII_WR4(sc, reg+0, cmd & 0xff);
	MII_WR4(sc, reg+4, (cmd >> 8) & 0xff);
	MII_WR4(sc, reg+8, (cmd >> 16) & 0xff);
	MII_WR4(sc, reg+12, (cmd >> 24) & 0xff);
}

static int
npe_mii_mdio_wait(struct npe_softc *sc)
{
	uint32_t v;
	int i;

	/* NB: typically this takes 25-30 trips */
	for (i = 0; i < 1000; i++) {
		v = npe_mii_mdio_read(sc, NPE_MAC_MDIO_CMD);
		if ((v & NPE_MII_GO) == 0)
			return 1;
		DELAY(1);
	}
	device_printf(sc->sc_dev, "%s: timeout after ~1ms, cmd 0x%x\n",
	    __func__, v);
	return 0;		/* NB: timeout */
}

static int
npe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct npe_softc *sc = device_get_softc(dev);
	uint32_t v;

	v = (phy << NPE_MII_ADDR_SHL) | (reg << NPE_MII_REG_SHL) | NPE_MII_GO;
	npe_mii_mdio_write(sc, NPE_MAC_MDIO_CMD, v);
	if (npe_mii_mdio_wait(sc))
		v = npe_mii_mdio_read(sc, NPE_MAC_MDIO_STS);
	else
		v = 0xffff | NPE_MII_READ_FAIL;
	return (v & NPE_MII_READ_FAIL) ? 0xffff : (v & 0xffff);
}

static int
npe_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct npe_softc *sc = device_get_softc(dev);
	uint32_t v;

	v = (phy << NPE_MII_ADDR_SHL) | (reg << NPE_MII_REG_SHL)
	  | data | NPE_MII_WRITE
	  | NPE_MII_GO;
	npe_mii_mdio_write(sc, NPE_MAC_MDIO_CMD, v);
	/* XXX complain about timeout */
	(void) npe_mii_mdio_wait(sc);
	return (0);
}

static void
npe_miibus_statchg(device_t dev)
{
	struct npe_softc *sc = device_get_softc(dev);
	struct mii_data *mii = device_get_softc(sc->sc_mii);
	uint32_t tx1, rx1;

	/* sync MAC duplex state */
	tx1 = RD4(sc, NPE_MAC_TX_CNTRL1);
	rx1 = RD4(sc, NPE_MAC_RX_CNTRL1);
	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		tx1 &= ~NPE_TX_CNTRL1_DUPLEX;
		rx1 |= NPE_RX_CNTRL1_PAUSE_EN;
	} else {
		tx1 |= NPE_TX_CNTRL1_DUPLEX;
		rx1 &= ~NPE_RX_CNTRL1_PAUSE_EN;
	}
	WR4(sc, NPE_MAC_RX_CNTRL1, rx1);
	WR4(sc, NPE_MAC_TX_CNTRL1, tx1);
}

static device_method_t npe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		npe_probe),
	DEVMETHOD(device_attach,	npe_attach),
	DEVMETHOD(device_detach,	npe_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	npe_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	npe_miibus_readreg),
	DEVMETHOD(miibus_writereg,	npe_miibus_writereg),
	DEVMETHOD(miibus_statchg,	npe_miibus_statchg),

	{ 0, 0 }
};

static driver_t npe_driver = {
	"npe",
	npe_methods,
	sizeof(struct npe_softc),
};

DRIVER_MODULE(npe, ixp, npe_driver, npe_devclass, 0, 0);
DRIVER_MODULE(miibus, npe, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(npe, ixpqmgr, 1, 1, 1);
MODULE_DEPEND(npe, miibus, 1, 1, 1);
MODULE_DEPEND(npe, ether, 1, 1, 1);
