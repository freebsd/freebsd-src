/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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

/* TODO: (in no order)
 *
 * 8) Need to sync busdma goo in atestop
 * 9) atestop should maybe free the mbufs?
 *
 * 1) detach
 * 2) Free dma setup
 * 3) Turn on the clock in pmc?  Turn off?
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <arm/at91/if_atereg.h>

#include "miibus_if.h"

#define ATE_MAX_TX_BUFFERS 2		/* We have ping-pong tx buffers */
#define ATE_MAX_RX_BUFFERS 64

struct ate_softc
{
	struct ifnet *ifp;		/* ifnet pointer */
	struct mtx sc_mtx;		/* basically a perimeter lock */
	device_t dev;			/* Myself */
	device_t miibus;		/* My child miibus */
	void *intrhand;			/* Interrupt handle */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct callout tick_ch;		/* Tick callout */
	bus_dma_tag_t mtag;		/* bus dma tag for mbufs */
	bus_dmamap_t tx_map[ATE_MAX_TX_BUFFERS];
	struct mbuf *sent_mbuf[ATE_MAX_TX_BUFFERS]; /* Sent mbufs */
	bus_dma_tag_t rxtag;
	bus_dmamap_t rx_map[ATE_MAX_RX_BUFFERS];
	void *rx_buf[ATE_MAX_RX_BUFFERS]; /* RX buffer space */
	int rx_buf_ptr;
	bus_dma_tag_t rx_desc_tag;
	bus_dmamap_t rx_desc_map;
	int txcur;			/* current tx map pointer */
	bus_addr_t rx_desc_phys;
	eth_rx_desc_t *rx_descs;
	int use_rmii;
	struct	ifmib_iso_8802_3 mibdata; /* stuff for network mgmt */
};

static inline uint32_t
RD4(struct ate_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct ate_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

#define ATE_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	ATE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define ATE_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    MTX_NETWORK_LOCK, MTX_DEF)
#define ATE_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define ATE_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define ATE_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static devclass_t ate_devclass;

/* ifnet entry points */

static void ateinit_locked(void *);
static void atestart_locked(struct ifnet *);

static void ateinit(void *);
static void atestart(struct ifnet *);
static void atestop(struct ate_softc *);
static int ateioctl(struct ifnet * ifp, u_long, caddr_t);

/* bus entry points */

static int ate_probe(device_t dev);
static int ate_attach(device_t dev);
static int ate_detach(device_t dev);
static void ate_intr(void *);

/* helper routines */
static int ate_activate(device_t dev);
static void ate_deactivate(device_t dev);
static int ate_ifmedia_upd(struct ifnet *ifp);
static void ate_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int ate_get_mac(struct ate_softc *sc, u_char *eaddr);
static void ate_set_mac(struct ate_softc *sc, u_char *eaddr);

/*
 * The AT91 family of products has the ethernet called EMAC.  However,
 * it isn't self identifying.  It is anticipated that the parent bus
 * code will take care to only add ate devices where they really are.  As
 * such, we do nothing here to identify the device and just set its name.
 */
static int
ate_probe(device_t dev)
{
	device_set_desc(dev, "EMAC");
	return (0);
}

static int
ate_attach(device_t dev)
{
	struct ate_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = NULL;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	int err;
	u_char eaddr[6];

	sc->dev = dev;
	err = ate_activate(dev);
	if (err)
		goto out;

	sc->use_rmii = (RD4(sc, ETH_CFG) & ETH_CFG_RMII) == ETH_CFG_RMII;

	/* Sysctls */
	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);
	SYSCTL_ADD_UINT(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "rmii",
	    CTLFLAG_RD, &sc->use_rmii, 0, "rmii in use");

	/* calling atestop before ifp is set is OK */
	atestop(sc);
	ATE_LOCK_INIT(sc);
	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	if ((err = ate_get_mac(sc, eaddr)) != 0) {
		device_printf(dev, "No MAC address set\n");
		goto out;
	}
	ate_set_mac(sc, eaddr);

	sc->ifp = ifp = if_alloc(IFT_ETHER);
	if (mii_phy_probe(dev, &sc->miibus, ate_ifmedia_upd, ate_ifmedia_sts)) {
		device_printf(dev, "Cannot find my PHY.\n");
		err = ENXIO;
		goto out;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU; /* the hw bits already set */
	ifp->if_start = atestart;
	ifp->if_ioctl = ateioctl;
	ifp->if_init = ateinit;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_timer = 0;
	ifp->if_linkmib = &sc->mibdata;
	ifp->if_linkmiblen = sizeof(sc->mibdata);
	sc->mibdata.dot3Compliance = DOT3COMPLIANCE_COLLS;

	ether_ifattach(ifp, eaddr);

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, ate_intr, sc, &sc->intrhand);
	if (err) {
		ether_ifdetach(ifp);
		ATE_LOCK_DESTROY(sc);
	}
out:;
	if (err)
		ate_deactivate(dev);
	if (err && ifp)
		if_free(ifp);
	return (err);
}

static int
ate_detach(device_t dev)
{
	return EBUSY;	/* XXX TODO(1) */
}

static void
ate_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct ate_softc *sc;

	if (error != 0)
		return;
	sc = (struct ate_softc *)arg;
	sc->rx_desc_phys = segs[0].ds_addr;
}

static void
ate_load_rx_buf(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct ate_softc *sc;
	int i;

	if (error != 0)
		return;
	sc = (struct ate_softc *)arg;
	i = sc->rx_buf_ptr;

	/*
	 * For the last buffer, set the wrap bit so the controller
	 * restarts from the first descriptor.
	 */
	bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map, BUS_DMASYNC_PREWRITE);
	if (i == ATE_MAX_RX_BUFFERS - 1)
		sc->rx_descs[i].addr = segs[0].ds_addr | ETH_WRAP_BIT;
	else
		sc->rx_descs[i].addr = segs[0].ds_addr;
	bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map, BUS_DMASYNC_POSTWRITE);
	sc->rx_descs[i].status = 0;
	/* Flush the memory in the mbuf */
	bus_dmamap_sync(sc->rxtag, sc->rx_map[i], BUS_DMASYNC_PREREAD);
}

/*
 * Compute the multicast filter for this device using the standard
 * algorithm.  I wonder why this isn't in ether somewhere as a lot
 * of different MAC chips use this method (or the reverse the bits)
 * method.
 */
static void
ate_setmcast(struct ate_softc *sc)
{
	uint32_t index;
	uint32_t mcaf[2];
	u_char *af = (u_char *) mcaf;
	struct ifmultiaddr *ifma;

	mcaf[0] = 0;
	mcaf[1] = 0;

	IF_ADDR_LOCK(sc->ifp);
	TAILQ_FOREACH(ifma, &sc->ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		index = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		af[index >> 3] |= 1 << (index & 7);
	}
	IF_ADDR_UNLOCK(sc->ifp);

	/*
	 * Write the hash to the hash register.  This card can also
	 * accept unicast packets as well as multicast packets using this
	 * register for easier bridging operations, but we don't take
	 * advantage of that.  Locks here are to avoid LOR with the
	 * IF_ADDR_LOCK, but might not be strictly necessary.
	 */
	WR4(sc, ETH_HSL, mcaf[0]);
	WR4(sc, ETH_HSH, mcaf[1]);
}

static int
ate_activate(device_t dev)
{
	struct ate_softc *sc;
	int rid, err, i;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL)
		goto errout;

	/*
	 * Allocate DMA tags and maps
	 */
	err = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1, MCLBYTES, 0,
	    busdma_lock_mutex, &sc->sc_mtx, &sc->mtag);
	if (err != 0)
		goto errout;
	for (i = 0; i < ATE_MAX_TX_BUFFERS; i++) {
		err = bus_dmamap_create(sc->mtag, 0, &sc->tx_map[i]);
		if (err != 0)
			goto errout;
	}
	 /*
	  * Allocate our Rx buffers.  This chip has a rx structure that's filled
	  * in
	  */
	
	/*
	 * Allocate DMA tags and maps for RX.
	 */
	err = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1, MCLBYTES, 0,
	    busdma_lock_mutex, &sc->sc_mtx, &sc->rxtag);
	if (err != 0)
		goto errout;

	/* Dma TAG and MAP for the rx descriptors. */
	err = bus_dma_tag_create(NULL, sizeof(eth_rx_desc_t), 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    ATE_MAX_RX_BUFFERS * sizeof(eth_rx_desc_t), 1,
	    ATE_MAX_RX_BUFFERS * sizeof(eth_rx_desc_t), 0, busdma_lock_mutex,
	    &sc->sc_mtx, &sc->rx_desc_tag);
	if (err != 0)
		goto errout;
	if (bus_dmamem_alloc(sc->rx_desc_tag, (void **)&sc->rx_descs,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &sc->rx_desc_map) != 0)
		goto errout;
	if (bus_dmamap_load(sc->rx_desc_tag, sc->rx_desc_map,
	    sc->rx_descs, ATE_MAX_RX_BUFFERS * sizeof(eth_rx_desc_t),
	    ate_getaddr, sc, 0) != 0)
		goto errout;
	/* XXX TODO(5) Put this in ateinit_locked? */
	for (i = 0; i < ATE_MAX_RX_BUFFERS; i++) {
		sc->rx_buf_ptr = i;
		if (bus_dmamem_alloc(sc->rxtag, (void **)&sc->rx_buf[i],
		      BUS_DMA_NOWAIT, &sc->rx_map[i]) != 0)
			goto errout;
		if (bus_dmamap_load(sc->rxtag, sc->rx_map[i], sc->rx_buf[i],
		    MCLBYTES, ate_load_rx_buf, sc, 0) != 0)
			goto errout;
	}
	sc->rx_buf_ptr = 0;
	/* Flush the memory for the EMAC rx descriptor */
	bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map, BUS_DMASYNC_PREWRITE);
	/* Write the descriptor queue address. */
	WR4(sc, ETH_RBQP, sc->rx_desc_phys);
	return (0);
errout:
	ate_deactivate(dev);
	return (ENOMEM);
}

static void
ate_deactivate(device_t dev)
{
	struct ate_softc *sc;

	sc = device_get_softc(dev);
	/* XXX TODO(2) teardown busdma junk, below from fxp -- customize */
#if 0
	if (sc->fxp_mtag) {
		for (i = 0; i < FXP_NRFABUFS; i++) {
			rxp = &sc->fxp_desc.rx_list[i];
			if (rxp->rx_mbuf != NULL) {
				bus_dmamap_sync(sc->fxp_mtag, rxp->rx_map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->fxp_mtag, rxp->rx_map);
				m_freem(rxp->rx_mbuf);
			}
			bus_dmamap_destroy(sc->fxp_mtag, rxp->rx_map);
		}
		bus_dmamap_destroy(sc->fxp_mtag, sc->spare_map);
		for (i = 0; i < FXP_NTXCB; i++) {
			txp = &sc->fxp_desc.tx_list[i];
			if (txp->tx_mbuf != NULL) {
				bus_dmamap_sync(sc->fxp_mtag, txp->tx_map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->fxp_mtag, txp->tx_map);
				m_freem(txp->tx_mbuf);
			}
			bus_dmamap_destroy(sc->fxp_mtag, txp->tx_map);
		}
		bus_dma_tag_destroy(sc->fxp_mtag);
	}
	if (sc->fxp_stag)
		bus_dma_tag_destroy(sc->fxp_stag);
	if (sc->cbl_tag)
		bus_dma_tag_destroy(sc->cbl_tag);
	if (sc->mcs_tag)
		bus_dma_tag_destroy(sc->mcs_tag);
#endif
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
	bus_generic_detach(sc->dev);
	if (sc->miibus)
		device_delete_child(sc->dev, sc->miibus);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = 0;
	return;
}

/*
 * Change media according to request.
 */
static int
ate_ifmedia_upd(struct ifnet *ifp)
{
	struct ate_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	ATE_LOCK(sc);
	mii_mediachg(mii);
	ATE_UNLOCK(sc);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
static void
ate_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ate_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	ATE_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	ATE_UNLOCK(sc);
}

static void
ate_stat_update(struct ate_softc *sc, int active)
{
	/*
	 * The speed and full/half-duplex state needs to be reflected
	 * in the ETH_CFG register.
	 */
	if (IFM_SUBTYPE(active) == IFM_10_T)
		WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) & ~ETH_CFG_SPD);
	else
		WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_SPD);
	if (active & IFM_FDX)
		WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_FD);
	else
		WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) & ~ETH_CFG_FD);
}

static void
ate_tick(void *xsc)
{
	struct ate_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	struct mii_data *mii;
	int active;
	uint32_t c;

	/*
	 * The KB920x boot loader tests ETH_SR & ETH_SR_LINK and will ask
	 * the MII if there's a link if this bit is clear.  Not sure if we
	 * should do the same thing here or not.
	 */
	ATE_ASSERT_LOCKED(sc);
	if (sc->miibus != NULL) {
		mii = device_get_softc(sc->miibus);
		active = mii->mii_media_active;
		mii_tick(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		     active != mii->mii_media_active)
			ate_stat_update(sc, mii->mii_media_active);
	}

	/*
	 * Update the stats as best we can.  When we're done, clear
	 * the status counters and start over.  We're supposed to read these
	 * registers often enough that they won't overflow.  Hopefully
	 * once a second is often enough.  Some don't map well to
	 * the dot3Stats mib, so for those we just count them as general
	 * errors.  Stats for iframes, ibutes, oframes and obytes are
	 * collected elsewhere.  These registers zero on a read to prevent
	 * races.  For all the collision stats, also update the collision
	 * stats for the interface.
	 */
	sc->mibdata.dot3StatsAlignmentErrors += RD4(sc, ETH_ALE);
	sc->mibdata.dot3StatsFCSErrors += RD4(sc, ETH_SEQE);
	c = RD4(sc, ETH_SCOL);
	ifp->if_collisions += c;
	sc->mibdata.dot3StatsSingleCollisionFrames += c;
	c = RD4(sc, ETH_MCOL);
	sc->mibdata.dot3StatsMultipleCollisionFrames += c;
	ifp->if_collisions += c;
	sc->mibdata.dot3StatsSQETestErrors += RD4(sc, ETH_SQEE);
	sc->mibdata.dot3StatsDeferredTransmissions += RD4(sc, ETH_DTE);
	c = RD4(sc, ETH_LCOL);
	sc->mibdata.dot3StatsLateCollisions += c;
	ifp->if_collisions += c;
	c = RD4(sc, ETH_ECOL);
	sc->mibdata.dot3StatsExcessiveCollisions += c;
	ifp->if_collisions += c;
	sc->mibdata.dot3StatsCarrierSenseErrors += RD4(sc, ETH_CSE);
	sc->mibdata.dot3StatsFrameTooLongs += RD4(sc, ETH_ELR);
	sc->mibdata.dot3StatsInternalMacReceiveErrors += RD4(sc, ETH_DRFC);
	/*
	 * not sure where to lump these, so count them against the errors
	 * for the interface.
	 */
	sc->ifp->if_oerrors += RD4(sc, ETH_TUE);
	sc->ifp->if_ierrors += RD4(sc, ETH_CDE) + RD4(sc, ETH_RJB) +
	    RD4(sc, ETH_USF);

	/*
	 * Schedule another timeout one second from now.
	 */
	callout_reset(&sc->tick_ch, hz, ate_tick, sc);
}

static void
ate_set_mac(struct ate_softc *sc, u_char *eaddr)
{
	WR4(sc, ETH_SA1L, (eaddr[3] << 24) | (eaddr[2] << 16) |
	    (eaddr[1] << 8) | eaddr[0]);
	WR4(sc, ETH_SA1H, (eaddr[5] << 8) | (eaddr[4]));
}

static int
ate_get_mac(struct ate_softc *sc, u_char *eaddr)
{
	uint32_t low, high;

	/*
	 * The boot loader setup the MAC with an address, if one is set in
	 * the loader.  The TSC loader will also set the MAC address in a
	 * similar way.  Grab the MAC address from the SA1[HL] registers.
	 */
	low = RD4(sc, ETH_SA1L);
	high =  RD4(sc, ETH_SA1H);
	if ((low | (high & 0xffff)) == 0)
		return (ENXIO);
	eaddr[0] = low & 0xff;
	eaddr[1] = (low >> 8) & 0xff;
	eaddr[2] = (low >> 16) & 0xff;
	eaddr[3] = (low >> 24) & 0xff;
	eaddr[4] = high & 0xff;
	eaddr[5] = (high >> 8) & 0xff;
	return (0);
}

static void
ate_intr(void *xsc)
{
	struct ate_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	int status;
	int i;
	void *bp;
	struct mbuf *mb;
	uint32_t rx_stat;

	status = RD4(sc, ETH_ISR);
	if (status == 0)
		return;
	if (status & ETH_ISR_RCOM) {
		bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
		    BUS_DMASYNC_POSTREAD);
		while (sc->rx_descs[sc->rx_buf_ptr].addr & ETH_CPU_OWNER) {
			i = sc->rx_buf_ptr;
			sc->rx_buf_ptr = (i + 1) % ATE_MAX_RX_BUFFERS;
			bp = sc->rx_buf[i];
			rx_stat = sc->rx_descs[i].status;
			if ((rx_stat & ETH_LEN_MASK) == 0) {
				printf("ignoring bogus 0 len packet\n");
				bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
				    BUS_DMASYNC_PREWRITE);
				sc->rx_descs[i].addr &= ~ETH_CPU_OWNER;
				bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
				    BUS_DMASYNC_POSTWRITE);
				continue;
			}
			/* Flush memory for mbuf so we don't get stale bytes */
			bus_dmamap_sync(sc->rxtag, sc->rx_map[i],
			    BUS_DMASYNC_POSTREAD);
			WR4(sc, ETH_RSR, RD4(sc, ETH_RSR));

			/*
			 * The length returned by the device includes the
			 * ethernet CRC calculation for the packet, but
			 * ifnet drivers are supposed to discard it.
			 */
			mb = m_devget(sc->rx_buf[i],
			    (rx_stat & ETH_LEN_MASK) - ETHER_CRC_LEN,
			    ETHER_ALIGN, ifp, NULL);
			bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
			    BUS_DMASYNC_PREWRITE);
			sc->rx_descs[i].addr &= ~ETH_CPU_OWNER;
			bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_sync(sc->rxtag, sc->rx_map[i],
			    BUS_DMASYNC_PREREAD);
			if (mb != NULL) {
				ifp->if_ipackets++;
				(*ifp->if_input)(ifp, mb);
			}
			
		}
	}
	if (status & ETH_ISR_TCOM) {
		ATE_LOCK(sc);
		/* XXX TSR register should be cleared */
		if (sc->sent_mbuf[0]) {
			bus_dmamap_sync(sc->mtag, sc->tx_map[0],
			    BUS_DMASYNC_POSTWRITE);
			m_freem(sc->sent_mbuf[0]);
			ifp->if_opackets++;
			sc->sent_mbuf[0] = NULL;
		}
		if (sc->sent_mbuf[1]) {
			if (RD4(sc, ETH_TSR) & ETH_TSR_IDLE) {
				bus_dmamap_sync(sc->mtag, sc->tx_map[1],
				    BUS_DMASYNC_POSTWRITE);
				m_freem(sc->sent_mbuf[1]);
				ifp->if_opackets++;
				sc->txcur = 0;
				sc->sent_mbuf[0] = sc->sent_mbuf[1] = NULL;
			} else {
				sc->sent_mbuf[0] = sc->sent_mbuf[1];
				sc->sent_mbuf[1] = NULL;
				sc->txcur = 1;
			}
		} else {
			sc->sent_mbuf[0] = NULL;
			sc->txcur = 0;
		}
		/*
		 * We're no longer busy, so clear the busy flag and call the
		 * start routine to xmit more packets.
		 */
		sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		atestart_locked(sc->ifp);
		ATE_UNLOCK(sc);
	}
	if (status & ETH_ISR_RBNA) {
		printf("RBNA workaround\n");
		/* Workaround Errata #11 */
		WR4(sc, ETH_CTL, RD4(sc, ETH_CTL) &~ ETH_CTL_RE);
		WR4(sc, ETH_CTL, RD4(sc, ETH_CTL) | ETH_CTL_RE);
	}
}

/*
 * Reset and initialize the chip
 */
static void
ateinit_locked(void *xsc)
{
	struct ate_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
 	struct mii_data *mii;

	ATE_ASSERT_LOCKED(sc);

	/*
	 * XXX TODO(3)
	 * we need to turn on the EMAC clock in the pmc.  With the
	 * default boot loader, this is already turned on.  However, we
	 * need to think about how best to turn it on/off as the interface
	 * is brought up/down, as well as dealing with the mii bus...
	 *
	 * We also need to multiplex the pins correctly.
	 */

	/*
	 * There are two different ways that the mii bus is connected
	 * to this chip.  Select the right one based on a compile-time
	 * option.
	 */
	if (sc->use_rmii)
		WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_RMII);
	else
		WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) & ~ETH_CFG_RMII);

	/*
	 * Turn on the multicast hash, and write 0's to it.
	 */
	WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_MTI);
	WR4(sc, ETH_HSH, 0);
	WR4(sc, ETH_HSL, 0);

	WR4(sc, ETH_CTL, RD4(sc, ETH_CTL) | ETH_CTL_TE | ETH_CTL_RE);
	WR4(sc, ETH_IER, ETH_ISR_RCOM | ETH_ISR_TCOM | ETH_ISR_RBNA);

	/*
	 * Boot loader fills in MAC address.  If that's not the case, then
	 * we should set SA1L and SA1H here to the appropriate value.  Note:
	 * the byte order is big endian, not little endian, so we have some
	 * swapping to do.  Again, if we need it (which I don't think we do).
	 */
	ate_setmcast(sc);

	/* enable big packets */
	WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_BIG);

	/*
	 * Set 'running' flag, and clear output active flag
	 * and attempt to start the output
	 */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	ate_stat_update(sc, mii->mii_media_active);
	atestart_locked(ifp);

	callout_reset(&sc->tick_ch, hz, ate_tick, sc);
}

/*
 * dequeu packets and transmit
 */
static void
atestart_locked(struct ifnet *ifp)
{
	struct ate_softc *sc = ifp->if_softc;
	struct mbuf *m, *mdefrag;
	bus_dma_segment_t segs[1];
	int nseg, e;

	ATE_ASSERT_LOCKED(sc);
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	while (sc->txcur < ATE_MAX_TX_BUFFERS) {
		/*
		 * check to see if there's room to put another packet into the
		 * xmit queue.  The EMAC chip has a ping-pong buffer for xmit
		 * packets.  We use OACTIVE to indicate "we can stuff more into
		 * our buffers (clear) or not (set)."
		 */
		if (!(RD4(sc, ETH_TSR) & ETH_TSR_BNQ)) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			return;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == 0) {
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
			return;
		}
		e = bus_dmamap_load_mbuf_sg(sc->mtag, sc->tx_map[sc->txcur], m,
		    segs, &nseg, 0);
		if (e == EFBIG) {
			mdefrag = m_defrag(m, M_DONTWAIT);
			if (mdefrag == NULL) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m);
				return;
			}
			m = mdefrag;
			e = bus_dmamap_load_mbuf_sg(sc->mtag,
			    sc->tx_map[sc->txcur], m, segs, &nseg, 0);
		}
		if (e != 0) {
			m_freem(m);
			continue;
		}
		bus_dmamap_sync(sc->mtag, sc->tx_map[sc->txcur],
		    BUS_DMASYNC_PREWRITE);

		/*
		 * tell the hardware to xmit the packet.
		 */
		WR4(sc, ETH_TAR, segs[0].ds_addr);
		WR4(sc, ETH_TCR, segs[0].ds_len);
	
		/*
		 * Tap off here if there is a bpf listener.
		 */
		BPF_MTAP(ifp, m);

		sc->sent_mbuf[sc->txcur] = m;
		sc->txcur++;
	}
}

static void
ateinit(void *xsc)
{
	struct ate_softc *sc = xsc;
	ATE_LOCK(sc);
	ateinit_locked(sc);
	ATE_UNLOCK(sc);
}

static void
atestart(struct ifnet *ifp)
{
	struct ate_softc *sc = ifp->if_softc;
	ATE_LOCK(sc);
	atestart_locked(ifp);
	ATE_UNLOCK(sc);
}

/*
 * Turn off interrupts, and stop the nic.  Can be called with sc->ifp NULL
 * so be careful.
 */
static void
atestop(struct ate_softc *sc)
{
	struct ifnet *ifp = sc->ifp;

	if (ifp) {
		ifp->if_timer = 0;
		ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	}

	callout_stop(&sc->tick_ch);

	/*
	 * Enable some parts of the MAC that are needed always (like the
	 * MII bus.  This turns off the RE and TE bits, which will remain
	 * off until ateinit() is called to turn them on.  With RE and TE
	 * turned off, there's no DMA to worry about after this write.
	 */
	WR4(sc, ETH_CTL, ETH_CTL_MPE);

	/*
	 * Turn off all the configured options and revert to defaults.
	 */
	WR4(sc, ETH_CFG, ETH_CFG_CLK_32);

	/*
	 * Turn off all the interrupts, and ack any pending ones by reading
	 * the ISR.
	 */
	WR4(sc, ETH_IDR, 0xffffffff);
	RD4(sc, ETH_ISR);

	/*
	 * Clear out the Transmit and Receiver Status registers of any
	 * errors they may be reporting
	 */
	WR4(sc, ETH_TSR, 0xffffffff);
	WR4(sc, ETH_RSR, 0xffffffff);

	/*
	 * XXX TODO(8)
	 * need to worry about the busdma resources?  Yes, I think we need
	 * to sync and unload them.  We may also need to release the mbufs
	 * that are assocaited with RX and TX operations.
	 */

	/*
	 * XXX we should power down the EMAC if it isn't in use, after
	 * putting it into loopback mode.  This saves about 400uA according
	 * to the datasheet.
	 */
}

static int
ateioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ate_softc *sc = ifp->if_softc;
 	struct mii_data *mii;
 	struct ifreq *ifr = (struct ifreq *)data;	
	int mask, error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		ATE_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_drv_flags & IFF_DRV_RUNNING) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			atestop(sc);
		} else {
			/* reinitialize card on any parameter change */
			ateinit_locked(sc);
		}
		ATE_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* update multicast filter list. */
		ATE_LOCK(sc);
		ate_setmcast(sc);
		ATE_UNLOCK(sc);
		error = 0;
		break;

  	case SIOCSIFMEDIA:
  	case SIOCGIFMEDIA:
 		mii = device_get_softc(sc->miibus);
 		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
  		break;
	case SIOCSIFCAP:
		mask = ifp->if_capenable ^ ifr->ifr_reqcap;
		if (mask & IFCAP_VLAN_MTU) {
			ATE_LOCK(sc);
			if (ifr->ifr_reqcap & IFCAP_VLAN_MTU) {
				WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_BIG);
				ifp->if_capenable |= IFCAP_VLAN_MTU;
			} else {
				WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) & ~ETH_CFG_BIG);
				ifp->if_capenable &= ~IFCAP_VLAN_MTU;
			}
			ATE_UNLOCK(sc);
		}
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
ate_child_detached(device_t dev, device_t child)
{
	struct ate_softc *sc;

	sc = device_get_softc(dev);
	if (child == sc->miibus)
		sc->miibus = NULL;
}

/*
 * MII bus support routines.
 */
static int
ate_miibus_readreg(device_t dev, int phy, int reg)
{
	struct ate_softc *sc;
	int val;

	/*
	 * XXX if we implement agressive power savings, then we need
	 * XXX to make sure that the clock to the emac is on here
	 */

	sc = device_get_softc(dev);
	DELAY(1);	/* Hangs w/o this delay really 30.5us atm */
	WR4(sc, ETH_MAN, ETH_MAN_REG_RD(phy, reg));
	while ((RD4(sc, ETH_SR) & ETH_SR_IDLE) == 0)
		continue;
	val = RD4(sc, ETH_MAN) & ETH_MAN_VALUE_MASK;

	return (val);
}

static void
ate_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct ate_softc *sc;
	
	/*
	 * XXX if we implement agressive power savings, then we need
	 * XXX to make sure that the clock to the emac is on here
	 */

	sc = device_get_softc(dev);
	WR4(sc, ETH_MAN, ETH_MAN_REG_WR(phy, reg, data));
	while ((RD4(sc, ETH_SR) & ETH_SR_IDLE) == 0)
		continue;
	return;
}

static device_method_t ate_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ate_probe),
	DEVMETHOD(device_attach,	ate_attach),
	DEVMETHOD(device_detach,	ate_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	ate_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	ate_miibus_readreg),
	DEVMETHOD(miibus_writereg,	ate_miibus_writereg),

	{ 0, 0 }
};

static driver_t ate_driver = {
	"ate",
	ate_methods,
	sizeof(struct ate_softc),
};

DRIVER_MODULE(ate, atmelarm, ate_driver, ate_devclass, 0, 0);
DRIVER_MODULE(miibus, ate, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(ate, miibus, 1, 1, 1);
MODULE_DEPEND(ate, ether, 1, 1, 1);
