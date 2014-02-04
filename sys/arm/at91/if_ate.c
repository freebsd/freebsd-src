/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2009 Greg Ansley.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* TODO
 *
 * 1) Turn on the clock in pmc?  Turn off?
 * 2) GPIO initializtion in board setup code.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
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
#include <net/if_var.h>

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

#include "opt_at91.h"
#include <arm/at91/at91reg.h>
#include <arm/at91/at91var.h>
#include <arm/at91/if_atereg.h>

#include "miibus_if.h"

/*
 * Driver-specific flags.
 */
#define	ATE_FLAG_DETACHING	0x01
#define	ATE_FLAG_MULTICAST	0x02

/*
 * Old EMAC assumes whole packet fits in one buffer;
 * new EBACB assumes all receive buffers are 128 bytes
 */
#define	RX_BUF_SIZE(sc)	(sc->is_emacb ? 128 : MCLBYTES)

/*
 * EMACB has an 11 bit counter for Rx/Tx Descriptors
 * for max total of 1024 decriptors each.
 */
#define	ATE_MAX_RX_DESCR	1024
#define	ATE_MAX_TX_DESCR	1024

/* How many buffers to allocate */
#define	ATE_MAX_TX_BUFFERS	4	/* We have ping-pong tx buffers */

/* How much memory to use for rx buffers */
#define	ATE_RX_MEMORY		(ATE_MAX_RX_DESCR * 128)

/* Actual number of descriptors we allocate */
#define	ATE_NUM_RX_DESCR	ATE_MAX_RX_DESCR
#define	ATE_NUM_TX_DESCR	ATE_MAX_TX_BUFFERS

#if ATE_NUM_TX_DESCR > ATE_MAX_TX_DESCR
#error "Can't have more TX buffers that descriptors"
#endif
#if ATE_NUM_RX_DESCR > ATE_MAX_RX_DESCR
#error "Can't have more RX buffers that descriptors"
#endif

/* Wrap indexes the same way the hardware does */
#define	NEXT_RX_IDX(sc, cur)	\
    ((sc->rx_descs[cur].addr & ETH_WRAP_BIT) ? 0 : (cur + 1))

#define	NEXT_TX_IDX(sc, cur)	\
    ((sc->tx_descs[cur].status & ETHB_TX_WRAP) ? 0 : (cur + 1))

struct ate_softc
{
	struct ifnet	*ifp;		/* ifnet pointer */
	struct mtx	sc_mtx;		/* Basically a perimeter lock */
	device_t	dev;		/* Myself */
	device_t	miibus;		/* My child miibus */
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct callout  tick_ch;	/* Tick callout */
	struct ifmib_iso_8802_3 mibdata; /* Stuff for network mgmt */
	bus_dma_tag_t   mtag;		/* bus dma tag for mbufs */
	bus_dma_tag_t   rx_tag;
	bus_dma_tag_t   rx_desc_tag;
	bus_dmamap_t    rx_desc_map;
	bus_dmamap_t    rx_map[ATE_MAX_RX_DESCR];
	bus_addr_t	rx_desc_phys;   /* PA of rx descriptors */
	eth_rx_desc_t   *rx_descs;	/* VA of rx descriptors */
	void		*rx_buf[ATE_NUM_RX_DESCR]; /* RX buffer space */
	int		rxhead;		/* Current RX map/desc index */
	uint32_t	rx_buf_size;    /* Size of Rx buffers */

	bus_dma_tag_t   tx_desc_tag;
	bus_dmamap_t    tx_desc_map;
	bus_dmamap_t    tx_map[ATE_MAX_TX_BUFFERS];
	bus_addr_t	tx_desc_phys;   /* PA of tx descriptors */
	eth_tx_desc_t   *tx_descs;	/* VA of tx descriptors */
	int		txhead;		/* Current TX map/desc index */
	int		txtail;		/* Current TX map/desc index */
	struct mbuf	*sent_mbuf[ATE_MAX_TX_BUFFERS]; /* Sent mbufs */
	void		*intrhand;	/* Interrupt handle */
	int		flags;
	int		if_flags;
	int		use_rmii;
	int		is_emacb;	/* SAM9x hardware version */
};

static inline uint32_t
RD4(struct ate_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct ate_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

static inline void
BARRIER(struct ate_softc *sc, bus_size_t off, bus_size_t len, int flags)
{

	bus_barrier(sc->mem_res, off, len, flags);
}

#define	ATE_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	ATE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	ATE_LOCK_INIT(_sc)					\
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev),	\
	    MTX_NETWORK_LOCK, MTX_DEF)
#define	ATE_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	ATE_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	ATE_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static devclass_t ate_devclass;

/*
 * ifnet entry points.
 */
static void	ateinit_locked(void *);
static void	atestart_locked(struct ifnet *);

static void	ateinit(void *);
static void	atestart(struct ifnet *);
static void	atestop(struct ate_softc *);
static int	ateioctl(struct ifnet * ifp, u_long, caddr_t);

/*
 * Bus entry points.
 */
static int	ate_probe(device_t dev);
static int	ate_attach(device_t dev);
static int	ate_detach(device_t dev);
static void	ate_intr(void *);

/*
 * Helper routines.
 */
static int	ate_activate(device_t dev);
static void	ate_deactivate(struct ate_softc *sc);
static int	ate_ifmedia_upd(struct ifnet *ifp);
static void	ate_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int	ate_get_mac(struct ate_softc *sc, u_char *eaddr);
static void	ate_set_mac(struct ate_softc *sc, u_char *eaddr);
static void	ate_rxfilter(struct ate_softc *sc);

static int	ate_miibus_readreg(device_t dev, int phy, int reg);

static int	ate_miibus_writereg(device_t dev, int phy, int reg, int data);
/*
 * The AT91 family of products has the ethernet interface called EMAC.
 * However, it isn't self identifying.  It is anticipated that the parent bus
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
	struct ate_softc *sc;
	struct ifnet *ifp = NULL;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	u_char eaddr[ETHER_ADDR_LEN];
	uint32_t rnd;
	int rid, err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	ATE_LOCK_INIT(sc);

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		err = ENOMEM;
		goto out;
	}
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate interrupt resources.\n");
		err = ENOMEM;
		goto out;
	}

	/* New or old version, chooses buffer size. */
	sc->is_emacb = at91_is_sam9() || at91_is_sam9xe();
	sc->rx_buf_size = RX_BUF_SIZE(sc);

	err = ate_activate(dev);
	if (err)
		goto out;

	/* Default to what boot rom did */
	if (!sc->is_emacb)
		sc->use_rmii =
		    (RD4(sc, ETH_CFG) & ETH_CFG_RMII) == ETH_CFG_RMII;
	else
		sc->use_rmii =
		    (RD4(sc, ETHB_UIO) & ETHB_UIO_RMII) == ETHB_UIO_RMII;

#ifdef AT91_ATE_USE_RMII
	/* Compile time override */
	sc->use_rmii = 1;
#endif
	/* Sysctls */
	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);
	SYSCTL_ADD_UINT(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "rmii",
	    CTLFLAG_RW, &sc->use_rmii, 0, "rmii in use");

	/* Calling atestop before ifp is set is OK. */
	ATE_LOCK(sc);
	atestop(sc);
	ATE_UNLOCK(sc);
	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	if ((err = ate_get_mac(sc, eaddr)) != 0) {
		/* No MAC address configured. Generate the random one. */
		if (bootverbose)
			device_printf(dev,
			    "Generating random ethernet address.\n");
		rnd = arc4random();

		/*
		 * Set OUI to convenient locally assigned address.  'b'
		 * is 0x62, which has the locally assigned bit set, and
		 * the broadcast/multicast bit clear.
		 */
		eaddr[0] = 'b';
		eaddr[1] = 's';
		eaddr[2] = 'd';
		eaddr[3] = (rnd >> 16) & 0xff;
		eaddr[4] = (rnd >>  8) & 0xff;
		eaddr[5] = (rnd >>  0) & 0xff;
	}

	sc->ifp = ifp = if_alloc(IFT_ETHER);
	err = mii_attach(dev, &sc->miibus, ifp, ate_ifmedia_upd,
	    ate_ifmedia_sts, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (err != 0) {
		device_printf(dev, "attaching PHYs failed\n");
		goto out;
	}
	/*
	 * XXX: Clear the isolate bit, or we won't get up,
	 * at least on the HL201
	 */
	ate_miibus_writereg(dev, 0, 0, 0x3000);

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU;	/* The hw bits already set. */
	ifp->if_start = atestart;
	ifp->if_ioctl = ateioctl;
	ifp->if_init = ateinit;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_linkmib = &sc->mibdata;
	ifp->if_linkmiblen = sizeof(sc->mibdata);
	sc->mibdata.dot3Compliance = DOT3COMPLIANCE_COLLS;
	sc->if_flags = ifp->if_flags;

	ether_ifattach(ifp, eaddr);

	/* Activate the interrupt. */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, ate_intr, sc, &sc->intrhand);
	if (err) {
		device_printf(dev, "could not establish interrupt handler.\n");
		ether_ifdetach(ifp);
		goto out;
	}

out:
	if (err)
		ate_detach(dev);
	return (err);
}

static int
ate_detach(device_t dev)
{
	struct ate_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	KASSERT(sc != NULL, ("[ate: %d]: sc is NULL", __LINE__));
	ifp = sc->ifp;
	if (device_is_attached(dev)) {
		ATE_LOCK(sc);
			sc->flags |= ATE_FLAG_DETACHING;
			atestop(sc);
		ATE_UNLOCK(sc);
		callout_drain(&sc->tick_ch);
		ether_ifdetach(ifp);
	}
	if (sc->miibus != NULL) {
		device_delete_child(dev, sc->miibus);
		sc->miibus = NULL;
	}
	bus_generic_detach(sc->dev);
	ate_deactivate(sc);
	if (sc->intrhand != NULL) {
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
		sc->intrhand = NULL;
	}
	if (ifp != NULL) {
		if_free(ifp);
		sc->ifp = NULL;
	}
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
		sc->mem_res = NULL;
	}
	if (sc->irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
		sc->irq_res = NULL;
	}
	ATE_LOCK_DESTROY(sc);
	return (0);
}

static void
ate_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static void
ate_load_rx_buf(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct ate_softc *sc;

	if (error != 0)
		return;
	sc = (struct ate_softc *)arg;

	bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map, BUS_DMASYNC_PREWRITE);
	sc->rx_descs[sc->rxhead].addr = segs[0].ds_addr;
	sc->rx_descs[sc->rxhead].status = 0;
	bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map, BUS_DMASYNC_POSTWRITE);
}

static uint32_t
ate_mac_hash(const uint8_t *buf)
{
	uint32_t index = 0;
	for (int i = 0; i < 48; i++) {
		index ^= ((buf[i >> 3] >> (i & 7)) & 1) << (i % 6);
	}
	return (index);
}

/*
 * Compute the multicast filter for this device.
 */
static int
ate_setmcast(struct ate_softc *sc)
{
	uint32_t index;
	uint32_t mcaf[2];
	u_char *af = (u_char *) mcaf;
	struct ifmultiaddr *ifma;
	struct ifnet *ifp;

	ifp = sc->ifp;

	if ((ifp->if_flags & IFF_PROMISC) != 0)
		return (0);
	if ((ifp->if_flags & IFF_ALLMULTI) != 0) {
		WR4(sc, ETH_HSL, 0xffffffff);
		WR4(sc, ETH_HSH, 0xffffffff);
		return (1);
	}

	/* Compute the multicast hash. */
	mcaf[0] = 0;
	mcaf[1] = 0;
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		index = ate_mac_hash(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr));
		af[index >> 3] |= 1 << (index & 7);
	}
	if_maddr_runlock(ifp);

	/*
	 * Write the hash to the hash register.  This card can also
	 * accept unicast packets as well as multicast packets using this
	 * register for easier bridging operations, but we don't take
	 * advantage of that.  Locks here are to avoid LOR with the
	 * if_maddr_rlock, but might not be strictly necessary.
	 */
	WR4(sc, ETH_HSL, mcaf[0]);
	WR4(sc, ETH_HSH, mcaf[1]);
	return (mcaf[0] || mcaf[1]);
}

static int
ate_activate(device_t dev)
{
	struct ate_softc *sc;
	int i;

	sc = device_get_softc(dev);

	/* Allocate DMA tags and maps for TX mbufs */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    1, MCLBYTES, 0, busdma_lock_mutex, &sc->sc_mtx, &sc->mtag))
		goto errout;
	for (i = 0; i < ATE_MAX_TX_BUFFERS; i++) {
		if ( bus_dmamap_create(sc->mtag, 0, &sc->tx_map[i]))
			goto errout;
	}


	/* DMA tag and map for the RX descriptors. */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), sizeof(eth_rx_desc_t),
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    ATE_NUM_RX_DESCR * sizeof(eth_rx_desc_t), 1,
	    ATE_NUM_RX_DESCR * sizeof(eth_rx_desc_t), 0, busdma_lock_mutex,
	    &sc->sc_mtx, &sc->rx_desc_tag))
		goto errout;
	if (bus_dmamem_alloc(sc->rx_desc_tag, (void **)&sc->rx_descs,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &sc->rx_desc_map) != 0)
		goto errout;
	if (bus_dmamap_load(sc->rx_desc_tag, sc->rx_desc_map,
	    sc->rx_descs, ATE_NUM_RX_DESCR * sizeof(eth_rx_desc_t),
	    ate_getaddr, &sc->rx_desc_phys, 0) != 0)
		goto errout;

	/* Allocate DMA tags and maps for RX. buffers */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sc->rx_buf_size, 1, sc->rx_buf_size, 0,
	    busdma_lock_mutex, &sc->sc_mtx, &sc->rx_tag))
		goto errout;

	/*
	 * Allocate our RX buffers.
	 * This chip has a RX structure that's filled in.
	 * XXX On MACB (SAM9 part) we should receive directly into mbuf
	 * to avoid the copy.  XXX
	 */
	sc->rxhead = 0;
	for (sc->rxhead = 0; sc->rxhead < ATE_RX_MEMORY/sc->rx_buf_size;
	    sc->rxhead++) {
		if (bus_dmamem_alloc(sc->rx_tag,
		    (void **)&sc->rx_buf[sc->rxhead], BUS_DMA_NOWAIT,
		    &sc->rx_map[sc->rxhead]) != 0)
			goto errout;

		if (bus_dmamap_load(sc->rx_tag, sc->rx_map[sc->rxhead],
		    sc->rx_buf[sc->rxhead], sc->rx_buf_size,
		    ate_load_rx_buf, sc, 0) != 0) {
			printf("bus_dmamem_load\n");
			goto errout;
		}
		bus_dmamap_sync(sc->rx_tag, sc->rx_map[sc->rxhead], BUS_DMASYNC_PREREAD);
	}

	/*
	 * For the last buffer, set the wrap bit so the controller
	 * restarts from the first descriptor.
	 */
	sc->rx_descs[--sc->rxhead].addr |= ETH_WRAP_BIT;
	sc->rxhead = 0;

	/* Flush the memory for the EMAC rx descriptor. */
	bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map, BUS_DMASYNC_PREWRITE);

	/* Write the descriptor queue address. */
	WR4(sc, ETH_RBQP, sc->rx_desc_phys);

	/*
	 * DMA tag and map for the TX descriptors.
	 */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), sizeof(eth_tx_desc_t),
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    ATE_MAX_TX_BUFFERS * sizeof(eth_tx_desc_t), 1,
	    ATE_MAX_TX_BUFFERS * sizeof(eth_tx_desc_t), 0, busdma_lock_mutex,
	    &sc->sc_mtx, &sc->tx_desc_tag) != 0)
		goto errout;

	if (bus_dmamem_alloc(sc->tx_desc_tag, (void **)&sc->tx_descs,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &sc->tx_desc_map) != 0)
		goto errout;

	if (bus_dmamap_load(sc->tx_desc_tag, sc->tx_desc_map,
	    sc->tx_descs, ATE_MAX_TX_BUFFERS * sizeof(eth_tx_desc_t),
	    ate_getaddr, &sc->tx_desc_phys, 0) != 0)
		goto errout;

	/* Initilize descriptors; mark all empty */
	for (i = 0; i < ATE_MAX_TX_BUFFERS; i++) {
		sc->tx_descs[i].addr =0;
		sc->tx_descs[i].status = ETHB_TX_USED;
		sc->sent_mbuf[i] = NULL;
	}

	/* Mark last entry to cause wrap when indexing through */
	sc->tx_descs[ATE_MAX_TX_BUFFERS - 1].status =
	    ETHB_TX_WRAP | ETHB_TX_USED;

	/* Flush the memory for the EMAC tx descriptor. */
	bus_dmamap_sync(sc->tx_desc_tag, sc->tx_desc_map, BUS_DMASYNC_PREWRITE);

	sc->txhead = sc->txtail = 0;
	if (sc->is_emacb) {
		/* Write the descriptor queue address. */
		WR4(sc, ETHB_TBQP, sc->tx_desc_phys);

		/* EMACB: Enable transceiver input clock */
		WR4(sc, ETHB_UIO, RD4(sc, ETHB_UIO) | ETHB_UIO_CLKE);
	}

	return (0);

errout:
	return (ENOMEM);
}

static void
ate_deactivate(struct ate_softc *sc)
{
	int i;

	KASSERT(sc != NULL, ("[ate, %d]: sc is NULL!", __LINE__));
	if (sc->mtag != NULL) {
		for (i = 0; i < ATE_MAX_TX_BUFFERS; i++) {
			if (sc->sent_mbuf[i] != NULL) {
				bus_dmamap_sync(sc->mtag, sc->tx_map[i],
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->mtag, sc->tx_map[i]);
				m_freem(sc->sent_mbuf[i]);
			}
			bus_dmamap_destroy(sc->mtag, sc->tx_map[i]);
			sc->sent_mbuf[i] = NULL;
			sc->tx_map[i] = NULL;
		}
		bus_dma_tag_destroy(sc->mtag);
	}
	if (sc->rx_desc_tag != NULL) {
		if (sc->rx_descs != NULL) {
			if (sc->rx_desc_phys != 0) {
				bus_dmamap_sync(sc->rx_desc_tag,
				    sc->rx_desc_map, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->rx_desc_tag,
				    sc->rx_desc_map);
				sc->rx_desc_phys = 0;
			}
		}
	}
	if (sc->rx_tag != NULL) {
		for (i = 0; sc->rx_buf[i] != NULL; i++) {
			if (sc->rx_descs[i].addr != 0) {
				bus_dmamap_sync(sc->rx_tag,
				    sc->rx_map[i],
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->rx_tag,
				    sc->rx_map[i]);
				sc->rx_descs[i].addr = 0;
			}
			bus_dmamem_free(sc->rx_tag, sc->rx_buf[i],
			    sc->rx_map[i]);
			sc->rx_buf[i] = NULL;
			sc->rx_map[i] = NULL;
		}
		bus_dma_tag_destroy(sc->rx_tag);
	}
	if (sc->rx_desc_tag != NULL) {
		if (sc->rx_descs != NULL)
			bus_dmamem_free(sc->rx_desc_tag, sc->rx_descs,
			    sc->rx_desc_map);
		bus_dma_tag_destroy(sc->rx_desc_tag);
		sc->rx_descs = NULL;
		sc->rx_desc_tag = NULL;
	}

	if (sc->is_emacb)
		WR4(sc, ETHB_UIO, RD4(sc, ETHB_UIO) & ~ETHB_UIO_CLKE);
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
	uint32_t reg;

	/*
	 * The speed and full/half-duplex state needs to be reflected
	 * in the ETH_CFG register.
	 */
	reg = RD4(sc, ETH_CFG);
	reg &= ~(ETH_CFG_SPD | ETH_CFG_FD);
	if (IFM_SUBTYPE(active) != IFM_10_T)
		reg |= ETH_CFG_SPD;
	if (active & IFM_FDX)
		reg |= ETH_CFG_FD;
	WR4(sc, ETH_CFG, reg);
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
	 * Not sure where to lump these, so count them against the errors
	 * for the interface.
	 */
	sc->ifp->if_oerrors += RD4(sc, ETH_TUE);
	sc->ifp->if_ierrors += RD4(sc, ETH_CDE) + RD4(sc, ETH_RJB) +
	    RD4(sc, ETH_USF);

	/* Schedule another timeout one second from now. */
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
	bus_size_t sa_low_reg[] = { ETH_SA1L, ETH_SA2L, ETH_SA3L, ETH_SA4L };
	bus_size_t sa_high_reg[] = { ETH_SA1H, ETH_SA2H, ETH_SA3H, ETH_SA4H };
	uint32_t low, high;
	int i;

	/*
	 * The boot loader may setup the MAC with an address(es), grab the
	 * first MAC address from the SA[1-4][HL] registers.
	 */
	for (i = 0; i < 4; i++) {
		low = RD4(sc, sa_low_reg[i]);
		high = RD4(sc, sa_high_reg[i]);
		if ((low | (high & 0xffff)) != 0) {
			eaddr[0] = low & 0xff;
			eaddr[1] = (low >> 8) & 0xff;
			eaddr[2] = (low >> 16) & 0xff;
			eaddr[3] = (low >> 24) & 0xff;
			eaddr[4] = high & 0xff;
			eaddr[5] = (high >> 8) & 0xff;
			return (0);
		}
	}
	return (ENXIO);
}

static void
ate_intr(void *xsc)
{
	struct ate_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	struct mbuf *mb;
	eth_rx_desc_t	*rxdhead;
	uint32_t status, reg, idx;
	int remain, count, done;

	status = RD4(sc, ETH_ISR);
	if (status == 0)
		return;

	if (status & ETH_ISR_RCOM) {
		bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
		    BUS_DMASYNC_POSTREAD);

		rxdhead = &sc->rx_descs[sc->rxhead];
		while (rxdhead->addr & ETH_CPU_OWNER) {
			if (!sc->is_emacb) {
				/*
				 * Simulate SAM9 FIRST/LAST bits for RM9200.
				 * RM9200 EMAC has only on Rx buffer per packet.
				 * But sometime we are handed a zero lenght packet.
				 */
				if ((rxdhead->status & ETH_LEN_MASK) == 0)
					rxdhead->status = 0; /* Mark error */
				else
					rxdhead->status |= ETH_BUF_FIRST | ETH_BUF_LAST;
			}

			if ((rxdhead->status & ETH_BUF_FIRST) == 0) {
				/* Something went wrong during RX so
				   release back to EMAC all buffers of invalid packets.
				*/
				rxdhead->status = 0;
				rxdhead->addr &= ~ETH_CPU_OWNER;
				sc->rxhead = NEXT_RX_IDX(sc, sc->rxhead);
				rxdhead = &sc->rx_descs[sc->rxhead];
				continue;
			}

			/* Find end of packet or start of next */
			idx = sc->rxhead;
			if ((sc->rx_descs[idx].status & ETH_BUF_LAST) == 0) {
				idx = NEXT_RX_IDX(sc, idx);

				while ((sc->rx_descs[idx].addr & ETH_CPU_OWNER) &&
					((sc->rx_descs[idx].status &
					    (ETH_BUF_FIRST|ETH_BUF_LAST))== 0))
					idx = NEXT_RX_IDX(sc, idx);
			}

			/* Packet NOT yet completely in memory; we are done */
			if ((sc->rx_descs[idx].addr & ETH_CPU_OWNER) == 0 ||
			    ((sc->rx_descs[idx].status & (ETH_BUF_FIRST|ETH_BUF_LAST))== 0))
					break;

			/* Packets with no end descriptor are invalid. */
			if ((sc->rx_descs[idx].status & ETH_BUF_LAST) == 0) {
					rxdhead->status &= ~ETH_BUF_FIRST;
					continue;
			}

			/* FCS is not coppied into mbuf. */
			remain = (sc->rx_descs[idx].status & ETH_LEN_MASK) - 4;

			/* Get an appropriately sized mbuf. */
			mb = m_get2(remain + ETHER_ALIGN, M_NOWAIT, MT_DATA,
			    M_PKTHDR);
			if (mb == NULL) {
				sc->ifp->if_iqdrops++;
				rxdhead->status = 0;
				continue;
			}
			mb->m_data += ETHER_ALIGN;
			mb->m_pkthdr.rcvif = ifp;

			WR4(sc, ETH_RSR, RD4(sc, ETH_RSR));	/* Reset status */

			/* Now we process the buffers that make up the packet */
			do {

				/* Last buffer may just be 1-4 bytes of FCS so remain
				 * may be zero for last decriptor.  */
				if (remain > 0) {
						/* Make sure we get the current bytes */
						bus_dmamap_sync(sc->rx_tag, sc->rx_map[sc->rxhead],
						    BUS_DMASYNC_POSTREAD);

						count = MIN(remain, sc->rx_buf_size);

						/* XXX Performance robbing copy. Could
						 * recieve directly to mbufs if not an
						 * RM9200. And even then we could likely
						 * copy just the protocol headers. XXX  */
						m_append(mb, count, sc->rx_buf[sc->rxhead]);
						remain -= count;
				}

				done = (rxdhead->status & ETH_BUF_LAST) != 0;

				/* Return the descriptor to the EMAC */
				rxdhead->status = 0;
				rxdhead->addr &= ~ETH_CPU_OWNER;
				bus_dmamap_sync(sc->rx_desc_tag, sc->rx_desc_map,
				    BUS_DMASYNC_PREWRITE);

				/* Move on to next descriptor with wrap */
				sc->rxhead = NEXT_RX_IDX(sc, sc->rxhead);
				rxdhead = &sc->rx_descs[sc->rxhead];

			} while (!done);

			if (mb != NULL) {
				ifp->if_ipackets++;
				(*ifp->if_input)(ifp, mb);
			}
		}
	}


	if (status & ETH_ISR_TCOM) {
		bus_dmamap_sync(sc->tx_desc_tag, sc->tx_desc_map,
		    BUS_DMASYNC_POSTREAD);

		ATE_LOCK(sc);
		/* XXX TSR register should be cleared */
		if (!sc->is_emacb) {
			/* Simulate Transmit descriptor table */

			/* First packet done */
			if (sc->txtail < sc->txhead)
				sc->tx_descs[sc->txtail].status |= ETHB_TX_USED;

			/* Second Packet done */
			if (sc->txtail + 1 < sc->txhead &&
			    RD4(sc, ETH_TSR) & ETH_TSR_IDLE)
				sc->tx_descs[sc->txtail + 1].status |= ETHB_TX_USED;
		}

		while (sc->txtail != sc->txhead &&
		    sc->tx_descs[sc->txtail].status & ETHB_TX_USED ) {

			bus_dmamap_sync(sc->mtag, sc->tx_map[sc->txtail],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->mtag, sc->tx_map[sc->txtail]);
			m_freem(sc->sent_mbuf[sc->txtail]);
			sc->tx_descs[sc->txtail].addr = 0;
			sc->sent_mbuf[sc->txtail] = NULL;

			ifp->if_opackets++;
			sc->txtail = NEXT_TX_IDX(sc, sc->txtail);
		}

		/* Flush descriptors to EMAC */
		bus_dmamap_sync(sc->tx_desc_tag, sc->tx_desc_map, BUS_DMASYNC_PREWRITE);

		/*
		 * We're no longer busy, so clear the busy flag and call the
		 * start routine to xmit more packets.
		 */
		sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		atestart_locked(sc->ifp);
		ATE_UNLOCK(sc);
	}

	if (status & ETH_ISR_RBNA) {
		/* Workaround RM9200 Errata #11 */
		if (bootverbose)
			device_printf(sc->dev, "RBNA workaround\n");
		reg = RD4(sc, ETH_CTL);
		WR4(sc, ETH_CTL, reg & ~ETH_CTL_RE);
		BARRIER(sc, ETH_CTL, 4, BUS_SPACE_BARRIER_WRITE);
		WR4(sc, ETH_CTL, reg | ETH_CTL_RE);
	}

	/* XXX need to work around SAM9260 errata 43.2.4.1:
	 * disable the mac, reset tx buffer, enable mac on TUND */
}

/*
 * Reset and initialize the chip.
 */
static void
ateinit_locked(void *xsc)
{
	struct ate_softc *sc = xsc;
	struct ifnet *ifp = sc->ifp;
	struct mii_data *mii;
	uint8_t eaddr[ETHER_ADDR_LEN];
	uint32_t reg;

	ATE_ASSERT_LOCKED(sc);

	/*
	 * XXX TODO(3)
	 * we need to turn on the EMAC clock in the pmc.  With the
	 * default boot loader, this is already turned on.  However, we
	 * need to think about how best to turn it on/off as the interface
	 * is brought up/down, as well as dealing with the mii bus...
	 *
	 * We also need to multiplex the pins correctly (in board_xxx.c).
	 */

	/*
	 * There are two different ways that the mii bus is connected
	 * to this chip mii or rmii.
	 */
	if (!sc->is_emacb) {
		/* RM9200 */
		reg = RD4(sc, ETH_CFG);
		if (sc->use_rmii)
			reg |= ETH_CFG_RMII;
		else
			reg &= ~ETH_CFG_RMII;
		WR4(sc, ETH_CFG, reg);
	} else  {
		/* SAM9 */
		reg = ETHB_UIO_CLKE;
		reg |= (sc->use_rmii) ? ETHB_UIO_RMII : 0;
		WR4(sc, ETHB_UIO, reg);
	}

	ate_rxfilter(sc);

	/*
	 * Set the chip MAC address.
	 */
	bcopy(IF_LLADDR(ifp), eaddr, ETHER_ADDR_LEN);
	ate_set_mac(sc, eaddr);

	/* Make sure we know state of TX queue */
	sc->txhead = sc->txtail = 0;
	if (sc->is_emacb) {
		/* Write the descriptor queue address. */
		WR4(sc, ETHB_TBQP, sc->tx_desc_phys);
	}

	/*
	 * Turn on MACs and interrupt processing.
	 */
	WR4(sc, ETH_CTL, RD4(sc, ETH_CTL) | ETH_CTL_TE | ETH_CTL_RE);
	WR4(sc, ETH_IER, ETH_ISR_RCOM | ETH_ISR_TCOM | ETH_ISR_RBNA);

	/* Enable big packets. */
	WR4(sc, ETH_CFG, RD4(sc, ETH_CFG) | ETH_CFG_BIG);

	/*
	 * Set 'running' flag, and clear output active flag
	 * and attempt to start the output.
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
 * Dequeue packets and transmit.
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

	while (sc->tx_descs[sc->txhead].status & ETHB_TX_USED) {
		/*
		 * Check to see if there's room to put another packet into the
		 * xmit queue. The old EMAC version has a ping-pong buffer for
		 * xmit packets.  We use OACTIVE to indicate "we can stuff more
		 * into our buffers (clear) or not (set)."
		 */
		if (!sc->is_emacb) {
			/* RM9200 has only two hardware entries */
			if (!sc->is_emacb && (RD4(sc, ETH_TSR) & ETH_TSR_BNQ) == 0) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				return;
			}
		}

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		e = bus_dmamap_load_mbuf_sg(sc->mtag, sc->tx_map[sc->txhead], m,
		    segs, &nseg, 0);
		if (e == EFBIG) {
			mdefrag = m_defrag(m, M_NOWAIT);
			if (mdefrag == NULL) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m);
				return;
			}
			m = mdefrag;
			e = bus_dmamap_load_mbuf_sg(sc->mtag,
			    sc->tx_map[sc->txhead], m, segs, &nseg, 0);
		}
		if (e != 0) {
			m_freem(m);
			continue;
		}
		sc->sent_mbuf[sc->txhead] = m;

		bus_dmamap_sync(sc->mtag, sc->tx_map[sc->txhead],
		    BUS_DMASYNC_PREWRITE);

		/* Tell the hardware to xmit the packet. */
		if (!sc->is_emacb) {
			WR4(sc, ETH_TAR, segs[0].ds_addr);
			BARRIER(sc, ETH_TAR, 4, BUS_SPACE_BARRIER_WRITE);
			WR4(sc, ETH_TCR, segs[0].ds_len);
		} else {
			bus_dmamap_sync(sc->tx_desc_tag, sc->tx_desc_map,
			    BUS_DMASYNC_POSTWRITE);
			sc->tx_descs[sc->txhead].addr = segs[0].ds_addr;
			sc->tx_descs[sc->txhead].status = segs[0].ds_len |
			    (sc->tx_descs[sc->txhead].status & ETHB_TX_WRAP) |
			    ETHB_TX_BUF_LAST;
			bus_dmamap_sync(sc->tx_desc_tag, sc->tx_desc_map,
			    BUS_DMASYNC_PREWRITE);
			WR4(sc, ETH_CTL, RD4(sc, ETH_CTL) | ETHB_CTL_TGO);
		}
		sc->txhead = NEXT_TX_IDX(sc, sc->txhead);

		/* Tap off here if there is a bpf listener. */
		BPF_MTAP(ifp, m);
	}

	if ((sc->tx_descs[sc->txhead].status & ETHB_TX_USED) == 0)
	    ifp->if_drv_flags |= IFF_DRV_OACTIVE;
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
 * Turn off interrupts, and stop the NIC.  Can be called with sc->ifp NULL,
 * so be careful.
 */
static void
atestop(struct ate_softc *sc)
{
	struct ifnet *ifp;
	int i;

	ATE_ASSERT_LOCKED(sc);
	ifp = sc->ifp;
	if (ifp) {
		//ifp->if_timer = 0;
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

	/* Make sure thate the MDIO clk is less than
	 * 2.5 Mhz. Can no longer default to /32 since
	 * SAM9 family may have MCK > 80 Mhz */
	if (at91_master_clock <= 2000000)
		WR4(sc, ETH_CFG, ETH_CFG_CLK_8);
	else if (at91_master_clock <= 4000000)
		WR4(sc, ETH_CFG, ETH_CFG_CLK_16);
	else if (at91_master_clock <= 800000)
		WR4(sc, ETH_CFG, ETH_CFG_CLK_32);
	else
		WR4(sc, ETH_CFG, ETH_CFG_CLK_64);

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

	/* Release TX resources. */
	for (i = 0; i < ATE_MAX_TX_BUFFERS; i++) {
		if (sc->sent_mbuf[i] != NULL) {
			bus_dmamap_sync(sc->mtag, sc->tx_map[i],
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->mtag, sc->tx_map[i]);
			m_freem(sc->sent_mbuf[i]);
			sc->sent_mbuf[i] = NULL;
		}
	}

	/* Turn off transeiver input clock */
	if (sc->is_emacb)
		WR4(sc, ETHB_UIO, RD4(sc, ETHB_UIO) & ~ETHB_UIO_CLKE);

	/*
	 * XXX we should power down the EMAC if it isn't in use, after
	 * putting it into loopback mode.  This saves about 400uA according
	 * to the datasheet.
	 */
}

static void
ate_rxfilter(struct ate_softc *sc)
{
	struct ifnet *ifp;
	uint32_t reg;
	int enabled;

	KASSERT(sc != NULL, ("[ate, %d]: sc is NULL!", __LINE__));
	ATE_ASSERT_LOCKED(sc);
	ifp = sc->ifp;

	/* Wipe out old filter settings. */
	reg = RD4(sc, ETH_CFG);
	reg &= ~(ETH_CFG_CAF | ETH_CFG_MTI | ETH_CFG_UNI);
	reg |= ETH_CFG_NBC;
	sc->flags &= ~ATE_FLAG_MULTICAST;

	/* Set new parameters. */
	if ((ifp->if_flags & IFF_BROADCAST) != 0)
		reg &= ~ETH_CFG_NBC;
	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		reg |= ETH_CFG_CAF;
	} else {
		enabled = ate_setmcast(sc);
		if (enabled != 0) {
			reg |= ETH_CFG_MTI;
			sc->flags |= ATE_FLAG_MULTICAST;
		}
	}
	WR4(sc, ETH_CFG, reg);
}

static int
ateioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ate_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	struct ifreq *ifr = (struct ifreq *)data;
	int drv_flags, flags;
	int mask, error, enabled;

	error = 0;
	flags = ifp->if_flags;
	drv_flags = ifp->if_drv_flags;
	switch (cmd) {
	case SIOCSIFFLAGS:
		ATE_LOCK(sc);
		if ((flags & IFF_UP) != 0) {
			if ((drv_flags & IFF_DRV_RUNNING) != 0) {
				if (((flags ^ sc->if_flags)
				    & (IFF_PROMISC | IFF_ALLMULTI)) != 0)
					ate_rxfilter(sc);
			} else {
				if ((sc->flags & ATE_FLAG_DETACHING) == 0)
					ateinit_locked(sc);
			}
		} else if ((drv_flags & IFF_DRV_RUNNING) != 0) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			atestop(sc);
		}
		sc->if_flags = flags;
		ATE_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((drv_flags & IFF_DRV_RUNNING) != 0) {
			ATE_LOCK(sc);
			enabled = ate_setmcast(sc);
			if (enabled != (sc->flags & ATE_FLAG_MULTICAST))
				ate_rxfilter(sc);
			ATE_UNLOCK(sc);
		}
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

static int
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
	return (0);
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

	DEVMETHOD_END
};

static driver_t ate_driver = {
	"ate",
	ate_methods,
	sizeof(struct ate_softc),
};

DRIVER_MODULE(ate, atmelarm, ate_driver, ate_devclass, NULL, NULL);
DRIVER_MODULE(miibus, ate, miibus_driver, miibus_devclass, NULL, NULL);
MODULE_DEPEND(ate, miibus, 1, 1, 1);
MODULE_DEPEND(ate, ether, 1, 1, 1);
