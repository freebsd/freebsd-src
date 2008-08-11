/*-
 * Copyright (c) 2003 Stuart Walsh<stu@ipng.org.uk>
 * and Duncan Barclay<dmlb@dmlb.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS 'AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/bfe/if_bfereg.h>

MODULE_DEPEND(bfe, pci, 1, 1, 1);
MODULE_DEPEND(bfe, ether, 1, 1, 1);
MODULE_DEPEND(bfe, miibus, 1, 1, 1);

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#define BFE_DEVDESC_MAX		64	/* Maximum device description length */

static struct bfe_type bfe_devs[] = {
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM4401,
		"Broadcom BCM4401 Fast Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM4401B0,
		"Broadcom BCM4401-B0 Fast Ethernet" },
		{ 0, 0, NULL }
};

static int  bfe_probe				(device_t);
static int  bfe_attach				(device_t);
static int  bfe_detach				(device_t);
static int  bfe_suspend				(device_t);
static int  bfe_resume				(device_t);
static void bfe_release_resources	(struct bfe_softc *);
static void bfe_intr				(void *);
static void bfe_start				(struct ifnet *);
static void bfe_start_locked			(struct ifnet *);
static int  bfe_ioctl				(struct ifnet *, u_long, caddr_t);
static void bfe_init				(void *);
static void bfe_init_locked			(void *);
static void bfe_stop				(struct bfe_softc *);
static void bfe_watchdog			(struct bfe_softc *);
static int  bfe_shutdown			(device_t);
static void bfe_tick				(void *);
static void bfe_txeof				(struct bfe_softc *);
static void bfe_rxeof				(struct bfe_softc *);
static void bfe_set_rx_mode			(struct bfe_softc *);
static int  bfe_list_rx_init		(struct bfe_softc *);
static int  bfe_list_newbuf			(struct bfe_softc *, int, struct mbuf*);
static void bfe_rx_ring_free		(struct bfe_softc *);

static void bfe_pci_setup			(struct bfe_softc *, u_int32_t);
static int  bfe_ifmedia_upd			(struct ifnet *);
static void bfe_ifmedia_sts			(struct ifnet *, struct ifmediareq *);
static int  bfe_miibus_readreg		(device_t, int, int);
static int  bfe_miibus_writereg		(device_t, int, int, int);
static void bfe_miibus_statchg		(device_t);
static int  bfe_wait_bit			(struct bfe_softc *, u_int32_t, u_int32_t,
		u_long, const int);
static void bfe_get_config			(struct bfe_softc *sc);
static void bfe_read_eeprom			(struct bfe_softc *, u_int8_t *);
static void bfe_stats_update		(struct bfe_softc *);
static void bfe_clear_stats			(struct bfe_softc *);
static int  bfe_readphy				(struct bfe_softc *, u_int32_t, u_int32_t*);
static int  bfe_writephy			(struct bfe_softc *, u_int32_t, u_int32_t);
static int  bfe_resetphy			(struct bfe_softc *);
static int  bfe_setupphy			(struct bfe_softc *);
static void bfe_chip_reset			(struct bfe_softc *);
static void bfe_chip_halt			(struct bfe_softc *);
static void bfe_core_reset			(struct bfe_softc *);
static void bfe_core_disable		(struct bfe_softc *);
static int  bfe_dma_alloc			(device_t);
static void bfe_dma_map_desc		(void *, bus_dma_segment_t *, int, int);
static void bfe_dma_map				(void *, bus_dma_segment_t *, int, int);
static void bfe_cam_write			(struct bfe_softc *, u_char *, int);

static device_method_t bfe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bfe_probe),
	DEVMETHOD(device_attach,	bfe_attach),
	DEVMETHOD(device_detach,	bfe_detach),
	DEVMETHOD(device_shutdown,	bfe_shutdown),
	DEVMETHOD(device_suspend,	bfe_suspend),
	DEVMETHOD(device_resume,	bfe_resume),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	bfe_miibus_readreg),
	DEVMETHOD(miibus_writereg,	bfe_miibus_writereg),
	DEVMETHOD(miibus_statchg,	bfe_miibus_statchg),

	{ 0, 0 }
};

static driver_t bfe_driver = {
	"bfe",
	bfe_methods,
	sizeof(struct bfe_softc)
};

static devclass_t bfe_devclass;

DRIVER_MODULE(bfe, pci, bfe_driver, bfe_devclass, 0, 0);
DRIVER_MODULE(miibus, bfe, miibus_driver, miibus_devclass, 0, 0);

/*
 * Probe for a Broadcom 4401 chip.
 */
static int
bfe_probe(device_t dev)
{
	struct bfe_type *t;

	t = bfe_devs;

	while (t->bfe_name != NULL) {
		if (pci_get_vendor(dev) == t->bfe_vid &&
		    pci_get_device(dev) == t->bfe_did) {
			device_set_desc(dev, t->bfe_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return (ENXIO);
}

static int
bfe_dma_alloc(device_t dev)
{
	struct bfe_softc *sc;
	int error, i;

	sc = device_get_softc(dev);

	/*
	 * parent tag.  Apparently the chip cannot handle any DMA address
	 * greater than 1GB.
	 */
	error = bus_dma_tag_create(NULL,  /* parent */
			4096, 0,                  /* alignment, boundary */
			0x3FFFFFFF,               /* lowaddr */
			BUS_SPACE_MAXADDR,        /* highaddr */
			NULL, NULL,               /* filter, filterarg */
			MAXBSIZE,                 /* maxsize */
			BUS_SPACE_UNRESTRICTED,   /* num of segments */
			BUS_SPACE_MAXSIZE_32BIT,  /* max segment size */
			0,                        /* flags */
			NULL, NULL,               /* lockfunc, lockarg */
			&sc->bfe_parent_tag);

	/* tag for TX ring */
	error = bus_dma_tag_create(sc->bfe_parent_tag,
			4096, 0,
			BUS_SPACE_MAXADDR,
			BUS_SPACE_MAXADDR,
			NULL, NULL,
			BFE_TX_LIST_SIZE,
			1,
			BUS_SPACE_MAXSIZE_32BIT,
			0,
			NULL, NULL,
			&sc->bfe_tx_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* tag for RX ring */
	error = bus_dma_tag_create(sc->bfe_parent_tag,
			4096, 0,
			BUS_SPACE_MAXADDR,
			BUS_SPACE_MAXADDR,
			NULL, NULL,
			BFE_RX_LIST_SIZE,
			1,
			BUS_SPACE_MAXSIZE_32BIT,
			0,
			NULL, NULL,
			&sc->bfe_rx_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* tag for mbufs */
	error = bus_dma_tag_create(sc->bfe_parent_tag,
			ETHER_ALIGN, 0,
			BUS_SPACE_MAXADDR,
			BUS_SPACE_MAXADDR,
			NULL, NULL,
			MCLBYTES,
			1,
			BUS_SPACE_MAXSIZE_32BIT,
			BUS_DMA_ALLOCNOW,
			NULL, NULL,
			&sc->bfe_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* pre allocate dmamaps for RX list */
	for (i = 0; i < BFE_RX_LIST_CNT; i++) {
		error = bus_dmamap_create(sc->bfe_tag, 0,
		    &sc->bfe_rx_ring[i].bfe_map);
		if (error) {
			device_printf(dev, "cannot create DMA map for RX\n");
			return (ENOMEM);
		}
	}

	/* pre allocate dmamaps for TX list */
	for (i = 0; i < BFE_TX_LIST_CNT; i++) {
		error = bus_dmamap_create(sc->bfe_tag, 0,
		    &sc->bfe_tx_ring[i].bfe_map);
		if (error) {
			device_printf(dev, "cannot create DMA map for TX\n");
			return (ENOMEM);
		}
	}

	/* Alloc dma for rx ring */
	error = bus_dmamem_alloc(sc->bfe_rx_tag, (void *)&sc->bfe_rx_list,
			BUS_DMA_NOWAIT, &sc->bfe_rx_map);

	if (error)
		return (ENOMEM);

	bzero(sc->bfe_rx_list, BFE_RX_LIST_SIZE);
	error = bus_dmamap_load(sc->bfe_rx_tag, sc->bfe_rx_map,
			sc->bfe_rx_list, sizeof(struct bfe_desc),
			bfe_dma_map, &sc->bfe_rx_dma, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	bus_dmamap_sync(sc->bfe_rx_tag, sc->bfe_rx_map, BUS_DMASYNC_PREWRITE);

	error = bus_dmamem_alloc(sc->bfe_tx_tag, (void *)&sc->bfe_tx_list,
			BUS_DMA_NOWAIT, &sc->bfe_tx_map);
	if (error)
		return (ENOMEM);


	error = bus_dmamap_load(sc->bfe_tx_tag, sc->bfe_tx_map,
			sc->bfe_tx_list, sizeof(struct bfe_desc),
			bfe_dma_map, &sc->bfe_tx_dma, BUS_DMA_NOWAIT);
	if (error)
		return (ENOMEM);

	bzero(sc->bfe_tx_list, BFE_TX_LIST_SIZE);
	bus_dmamap_sync(sc->bfe_tx_tag, sc->bfe_tx_map, BUS_DMASYNC_PREWRITE);

	return (0);
}

static int
bfe_attach(device_t dev)
{
	struct ifnet *ifp = NULL;
	struct bfe_softc *sc;
	int error = 0, rid;

	sc = device_get_softc(dev);
	mtx_init(&sc->bfe_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
			MTX_DEF);
	callout_init_mtx(&sc->bfe_stat_co, &sc->bfe_mtx, 0);

	sc->bfe_dev = dev;

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = BFE_PCI_MEMLO;
	sc->bfe_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
			RF_ACTIVE);
	if (sc->bfe_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->bfe_btag = rman_get_bustag(sc->bfe_res);
	sc->bfe_bhandle = rman_get_bushandle(sc->bfe_res);
	sc->bfe_vhandle = (vm_offset_t)rman_get_virtual(sc->bfe_res);

	/* Allocate interrupt */
	rid = 0;

	sc->bfe_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
			RF_SHAREABLE | RF_ACTIVE);
	if (sc->bfe_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	if (bfe_dma_alloc(dev)) {
		device_printf(dev, "failed to allocate DMA resources\n");
		error = ENXIO;
		goto fail;
	}

	/* Set up ifnet structure */
	ifp = sc->bfe_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "failed to if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bfe_ioctl;
	ifp->if_start = bfe_start;
	ifp->if_init = bfe_init;
	ifp->if_mtu = ETHERMTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, BFE_TX_QLEN);
	ifp->if_snd.ifq_drv_maxlen = BFE_TX_QLEN;
	IFQ_SET_READY(&ifp->if_snd);

	bfe_get_config(sc);

	/* Reset the chip and turn on the PHY */
	BFE_LOCK(sc);
	bfe_chip_reset(sc);
	BFE_UNLOCK(sc);

	if (mii_phy_probe(dev, &sc->bfe_miibus,
				bfe_ifmedia_upd, bfe_ifmedia_sts)) {
		device_printf(dev, "MII without any PHY!\n");
		error = ENXIO;
		goto fail;
	}

	ether_ifattach(ifp, sc->bfe_enaddr);

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU;

	/*
	 * Hook interrupt last to avoid having to lock softc
	 */
	error = bus_setup_intr(dev, sc->bfe_irq, INTR_TYPE_NET | INTR_MPSAFE,
			NULL, bfe_intr, sc, &sc->bfe_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		goto fail;
	}
fail:
	if (error)
		bfe_release_resources(sc);
	return (error);
}

static int
bfe_detach(device_t dev)
{
	struct bfe_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->bfe_mtx), ("bfe mutex not initialized"));

	ifp = sc->bfe_ifp;

	if (device_is_attached(dev)) {
		BFE_LOCK(sc);
		bfe_stop(sc);
		BFE_UNLOCK(sc);
		callout_drain(&sc->bfe_stat_co);
		if (ifp != NULL)
			ether_ifdetach(ifp);
	}

	bfe_chip_reset(sc);

	bus_generic_detach(dev);
	if (sc->bfe_miibus != NULL)
		device_delete_child(dev, sc->bfe_miibus);

	bfe_release_resources(sc);
	mtx_destroy(&sc->bfe_mtx);

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
bfe_shutdown(device_t dev)
{
	struct bfe_softc *sc;

	sc = device_get_softc(dev);
	BFE_LOCK(sc);
	bfe_stop(sc);

	BFE_UNLOCK(sc);

	return (0);
}

static int
bfe_suspend(device_t dev)
{
	struct bfe_softc *sc;

	sc = device_get_softc(dev);
	BFE_LOCK(sc);
	bfe_stop(sc);
	BFE_UNLOCK(sc);

	return (0);
}

static int
bfe_resume(device_t dev)
{
	struct bfe_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->bfe_ifp;
	BFE_LOCK(sc);
	bfe_chip_reset(sc);
	if (ifp->if_flags & IFF_UP) {
		bfe_init_locked(sc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
		    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			bfe_start_locked(ifp);
	}
	BFE_UNLOCK(sc);

	return (0);
}

static int
bfe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct bfe_softc *sc;
	u_int32_t ret;

	sc = device_get_softc(dev);
	if (phy != sc->bfe_phyaddr)
		return (0);
	bfe_readphy(sc, reg, &ret);

	return (ret);
}

static int
bfe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct bfe_softc *sc;

	sc = device_get_softc(dev);
	if (phy != sc->bfe_phyaddr)
		return (0);
	bfe_writephy(sc, reg, val);

	return (0);
}

static void
bfe_miibus_statchg(device_t dev)
{
	struct bfe_softc *sc;
	struct mii_data *mii;
	u_int32_t val, flow;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->bfe_miibus);

	if ((mii->mii_media_status & IFM_ACTIVE) != 0) {
		if (IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
			sc->bfe_link = 1;
	} else
		sc->bfe_link = 0;

	/* XXX Should stop Rx/Tx engine prior to touching MAC. */
	val = CSR_READ_4(sc, BFE_TX_CTRL);
	val &= ~BFE_TX_DUPLEX;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		val |= BFE_TX_DUPLEX;
		flow = 0;
#ifdef notyet
		flow = CSR_READ_4(sc, BFE_RXCONF);
		flow &= ~BFE_RXCONF_FLOW;
		if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) &
		    IFM_ETH_RXPAUSE) != 0)
			flow |= BFE_RXCONF_FLOW;
		CSR_WRITE_4(sc, BFE_RXCONF, flow);
		/*
		 * It seems that the hardware has Tx pause issues
		 * so enable only Rx pause.
		 */
		flow = CSR_READ_4(sc, BFE_MAC_FLOW);
		flow &= ~BFE_FLOW_PAUSE_ENAB;
		CSR_WRITE_4(sc, BFE_MAC_FLOW, flow);
#endif
	}
	CSR_WRITE_4(sc, BFE_TX_CTRL, val);
}

static void
bfe_tx_ring_free(struct bfe_softc *sc)
{
	int i;

	for(i = 0; i < BFE_TX_LIST_CNT; i++) {
		if (sc->bfe_tx_ring[i].bfe_mbuf != NULL) {
			m_freem(sc->bfe_tx_ring[i].bfe_mbuf);
			sc->bfe_tx_ring[i].bfe_mbuf = NULL;
			bus_dmamap_unload(sc->bfe_tag,
					sc->bfe_tx_ring[i].bfe_map);
		}
	}
	bzero(sc->bfe_tx_list, BFE_TX_LIST_SIZE);
	bus_dmamap_sync(sc->bfe_tx_tag, sc->bfe_tx_map, BUS_DMASYNC_PREWRITE);
}

static void
bfe_rx_ring_free(struct bfe_softc *sc)
{
	int i;

	for (i = 0; i < BFE_RX_LIST_CNT; i++) {
		if (sc->bfe_rx_ring[i].bfe_mbuf != NULL) {
			m_freem(sc->bfe_rx_ring[i].bfe_mbuf);
			sc->bfe_rx_ring[i].bfe_mbuf = NULL;
			bus_dmamap_unload(sc->bfe_tag,
					sc->bfe_rx_ring[i].bfe_map);
		}
	}
	bzero(sc->bfe_rx_list, BFE_RX_LIST_SIZE);
	bus_dmamap_sync(sc->bfe_rx_tag, sc->bfe_rx_map, BUS_DMASYNC_PREWRITE);
}

static int
bfe_list_rx_init(struct bfe_softc *sc)
{
	int i;

	for(i = 0; i < BFE_RX_LIST_CNT; i++) {
		if (bfe_list_newbuf(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	}

	bus_dmamap_sync(sc->bfe_rx_tag, sc->bfe_rx_map, BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, BFE_DMARX_PTR, (i * sizeof(struct bfe_desc)));

	sc->bfe_rx_cons = 0;

	return (0);
}

static int
bfe_list_newbuf(struct bfe_softc *sc, int c, struct mbuf *m)
{
	struct bfe_rxheader *rx_header;
	struct bfe_desc *d;
	struct bfe_data *r;
	u_int32_t ctrl;
	int allocated, error;

	if ((c < 0) || (c >= BFE_RX_LIST_CNT))
		return (EINVAL);

	allocated = 0;
	if (m == NULL) {
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			return (ENOBUFS);
		m->m_len = m->m_pkthdr.len = MCLBYTES;
		allocated++;
	}
	else
		m->m_data = m->m_ext.ext_buf;

	rx_header = mtod(m, struct bfe_rxheader *);
	rx_header->len = 0;
	rx_header->flags = 0;

	/* Map the mbuf into DMA */
	sc->bfe_rx_cnt = c;
	d = &sc->bfe_rx_list[c];
	r = &sc->bfe_rx_ring[c];
	error = bus_dmamap_load(sc->bfe_tag, r->bfe_map, mtod(m, void *),
			MCLBYTES, bfe_dma_map_desc, d, BUS_DMA_NOWAIT);
	if (error != 0) {
		if (allocated != 0)
			m_free(m);
		if (error != ENOMEM)
			device_printf(sc->bfe_dev,
			    "failed to map RX buffer, error %d\n", error);
		return (ENOBUFS);
	}
	bus_dmamap_sync(sc->bfe_tag, r->bfe_map, BUS_DMASYNC_PREWRITE);

	ctrl = ETHER_MAX_LEN + 32;

	if (c == BFE_RX_LIST_CNT - 1)
		ctrl |= BFE_DESC_EOT;

	d->bfe_ctrl = ctrl;
	r->bfe_mbuf = m;
	bus_dmamap_sync(sc->bfe_rx_tag, sc->bfe_rx_map, BUS_DMASYNC_PREWRITE);
	return (0);
}

static void
bfe_get_config(struct bfe_softc *sc)
{
	u_int8_t eeprom[128];

	bfe_read_eeprom(sc, eeprom);

	sc->bfe_enaddr[0] = eeprom[79];
	sc->bfe_enaddr[1] = eeprom[78];
	sc->bfe_enaddr[2] = eeprom[81];
	sc->bfe_enaddr[3] = eeprom[80];
	sc->bfe_enaddr[4] = eeprom[83];
	sc->bfe_enaddr[5] = eeprom[82];

	sc->bfe_phyaddr = eeprom[90] & 0x1f;
	sc->bfe_mdc_port = (eeprom[90] >> 14) & 0x1;

	sc->bfe_core_unit = 0;
	sc->bfe_dma_offset = BFE_PCI_DMA;
}

static void
bfe_pci_setup(struct bfe_softc *sc, u_int32_t cores)
{
	u_int32_t bar_orig, pci_rev, val;

	bar_orig = pci_read_config(sc->bfe_dev, BFE_BAR0_WIN, 4);
	pci_write_config(sc->bfe_dev, BFE_BAR0_WIN, BFE_REG_PCI, 4);
	pci_rev = CSR_READ_4(sc, BFE_SBIDHIGH) & BFE_RC_MASK;

	val = CSR_READ_4(sc, BFE_SBINTVEC);
	val |= cores;
	CSR_WRITE_4(sc, BFE_SBINTVEC, val);

	val = CSR_READ_4(sc, BFE_SSB_PCI_TRANS_2);
	val |= BFE_SSB_PCI_PREF | BFE_SSB_PCI_BURST;
	CSR_WRITE_4(sc, BFE_SSB_PCI_TRANS_2, val);

	pci_write_config(sc->bfe_dev, BFE_BAR0_WIN, bar_orig, 4);
}

static void
bfe_clear_stats(struct bfe_softc *sc)
{
	u_long reg;

	BFE_LOCK_ASSERT(sc);

	CSR_WRITE_4(sc, BFE_MIB_CTRL, BFE_MIB_CLR_ON_READ);
	for (reg = BFE_TX_GOOD_O; reg <= BFE_TX_PAUSE; reg += 4)
		CSR_READ_4(sc, reg);
	for (reg = BFE_RX_GOOD_O; reg <= BFE_RX_NPAUSE; reg += 4)
		CSR_READ_4(sc, reg);
}

static int
bfe_resetphy(struct bfe_softc *sc)
{
	u_int32_t val;

	bfe_writephy(sc, 0, BMCR_RESET);
	DELAY(100);
	bfe_readphy(sc, 0, &val);
	if (val & BMCR_RESET) {
		device_printf(sc->bfe_dev, "PHY Reset would not complete.\n");
		return (ENXIO);
	}
	return (0);
}

static void
bfe_chip_halt(struct bfe_softc *sc)
{
	BFE_LOCK_ASSERT(sc);
	/* disable interrupts - not that it actually does..*/
	CSR_WRITE_4(sc, BFE_IMASK, 0);
	CSR_READ_4(sc, BFE_IMASK);

	CSR_WRITE_4(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE);
	bfe_wait_bit(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE, 200, 1);

	CSR_WRITE_4(sc, BFE_DMARX_CTRL, 0);
	CSR_WRITE_4(sc, BFE_DMATX_CTRL, 0);
	DELAY(10);
}

static void
bfe_chip_reset(struct bfe_softc *sc)
{
	u_int32_t val;

	BFE_LOCK_ASSERT(sc);

	/* Set the interrupt vector for the enet core */
	bfe_pci_setup(sc, BFE_INTVEC_ENET0);

	/* is core up? */
	val = CSR_READ_4(sc, BFE_SBTMSLOW) &
	    (BFE_RESET | BFE_REJECT | BFE_CLOCK);
	if (val == BFE_CLOCK) {
		/* It is, so shut it down */
		CSR_WRITE_4(sc, BFE_RCV_LAZY, 0);
		CSR_WRITE_4(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE);
		bfe_wait_bit(sc, BFE_ENET_CTRL, BFE_ENET_DISABLE, 100, 1);
		CSR_WRITE_4(sc, BFE_DMATX_CTRL, 0);
		sc->bfe_tx_cnt = sc->bfe_tx_prod = sc->bfe_tx_cons = 0;
		if (CSR_READ_4(sc, BFE_DMARX_STAT) & BFE_STAT_EMASK)
			bfe_wait_bit(sc, BFE_DMARX_STAT, BFE_STAT_SIDLE,
			    100, 0);
		CSR_WRITE_4(sc, BFE_DMARX_CTRL, 0);
		sc->bfe_rx_prod = sc->bfe_rx_cons = 0;
	}

	bfe_core_reset(sc);
	bfe_clear_stats(sc);

	/*
	 * We want the phy registers to be accessible even when
	 * the driver is "downed" so initialize MDC preamble, frequency,
	 * and whether internal or external phy here.
	 */

	/* 4402 has 62.5Mhz SB clock and internal phy */
	CSR_WRITE_4(sc, BFE_MDIO_CTRL, 0x8d);

	/* Internal or external PHY? */
	val = CSR_READ_4(sc, BFE_DEVCTRL);
	if (!(val & BFE_IPP))
		CSR_WRITE_4(sc, BFE_ENET_CTRL, BFE_ENET_EPSEL);
	else if (CSR_READ_4(sc, BFE_DEVCTRL) & BFE_EPR) {
		BFE_AND(sc, BFE_DEVCTRL, ~BFE_EPR);
		DELAY(100);
	}

	/* Enable CRC32 generation and set proper LED modes */
	BFE_OR(sc, BFE_MAC_CTRL, BFE_CTRL_CRC32_ENAB | BFE_CTRL_LED);

	/* Reset or clear powerdown control bit  */
	BFE_AND(sc, BFE_MAC_CTRL, ~BFE_CTRL_PDOWN);

	CSR_WRITE_4(sc, BFE_RCV_LAZY, ((1 << BFE_LAZY_FC_SHIFT) &
				BFE_LAZY_FC_MASK));

	/*
	 * We don't want lazy interrupts, so just send them at
	 * the end of a frame, please
	 */
	BFE_OR(sc, BFE_RCV_LAZY, 0);

	/* Set max lengths, accounting for VLAN tags */
	CSR_WRITE_4(sc, BFE_RXMAXLEN, ETHER_MAX_LEN+32);
	CSR_WRITE_4(sc, BFE_TXMAXLEN, ETHER_MAX_LEN+32);

	/* Set watermark XXX - magic */
	CSR_WRITE_4(sc, BFE_TX_WMARK, 56);

	/*
	 * Initialise DMA channels
	 * - not forgetting dma addresses need to be added to BFE_PCI_DMA
	 */
	CSR_WRITE_4(sc, BFE_DMATX_CTRL, BFE_TX_CTRL_ENABLE);
	CSR_WRITE_4(sc, BFE_DMATX_ADDR, sc->bfe_tx_dma + BFE_PCI_DMA);

	CSR_WRITE_4(sc, BFE_DMARX_CTRL, (BFE_RX_OFFSET << BFE_RX_CTRL_ROSHIFT) |
			BFE_RX_CTRL_ENABLE);
	CSR_WRITE_4(sc, BFE_DMARX_ADDR, sc->bfe_rx_dma + BFE_PCI_DMA);

	bfe_resetphy(sc);
	bfe_setupphy(sc);
}

static void
bfe_core_disable(struct bfe_softc *sc)
{
	if ((CSR_READ_4(sc, BFE_SBTMSLOW)) & BFE_RESET)
		return;

	/*
	 * Set reject, wait for it set, then wait for the core to stop
	 * being busy, then set reset and reject and enable the clocks.
	 */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_REJECT | BFE_CLOCK));
	bfe_wait_bit(sc, BFE_SBTMSLOW, BFE_REJECT, 1000, 0);
	bfe_wait_bit(sc, BFE_SBTMSHIGH, BFE_BUSY, 1000, 1);
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_FGC | BFE_CLOCK | BFE_REJECT |
				BFE_RESET));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);
	/* Leave reset and reject set */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_REJECT | BFE_RESET));
	DELAY(10);
}

static void
bfe_core_reset(struct bfe_softc *sc)
{
	u_int32_t val;

	/* Disable the core */
	bfe_core_disable(sc);

	/* and bring it back up */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_RESET | BFE_CLOCK | BFE_FGC));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);

	/* Chip bug, clear SERR, IB and TO if they are set. */
	if (CSR_READ_4(sc, BFE_SBTMSHIGH) & BFE_SERR)
		CSR_WRITE_4(sc, BFE_SBTMSHIGH, 0);
	val = CSR_READ_4(sc, BFE_SBIMSTATE);
	if (val & (BFE_IBE | BFE_TO))
		CSR_WRITE_4(sc, BFE_SBIMSTATE, val & ~(BFE_IBE | BFE_TO));

	/* Clear reset and allow it to move through the core */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, (BFE_CLOCK | BFE_FGC));
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);

	/* Leave the clock set */
	CSR_WRITE_4(sc, BFE_SBTMSLOW, BFE_CLOCK);
	CSR_READ_4(sc, BFE_SBTMSLOW);
	DELAY(10);
}

static void
bfe_cam_write(struct bfe_softc *sc, u_char *data, int index)
{
	u_int32_t val;

	val  = ((u_int32_t) data[2]) << 24;
	val |= ((u_int32_t) data[3]) << 16;
	val |= ((u_int32_t) data[4]) <<  8;
	val |= ((u_int32_t) data[5]);
	CSR_WRITE_4(sc, BFE_CAM_DATA_LO, val);
	val = (BFE_CAM_HI_VALID |
			(((u_int32_t) data[0]) << 8) |
			(((u_int32_t) data[1])));
	CSR_WRITE_4(sc, BFE_CAM_DATA_HI, val);
	CSR_WRITE_4(sc, BFE_CAM_CTRL, (BFE_CAM_WRITE |
				((u_int32_t) index << BFE_CAM_INDEX_SHIFT)));
	bfe_wait_bit(sc, BFE_CAM_CTRL, BFE_CAM_BUSY, 10000, 1);
}

static void
bfe_set_rx_mode(struct bfe_softc *sc)
{
	struct ifnet *ifp = sc->bfe_ifp;
	struct ifmultiaddr  *ifma;
	u_int32_t val;
	int i = 0;

	val = CSR_READ_4(sc, BFE_RXCONF);

	if (ifp->if_flags & IFF_PROMISC)
		val |= BFE_RXCONF_PROMISC;
	else
		val &= ~BFE_RXCONF_PROMISC;

	if (ifp->if_flags & IFF_BROADCAST)
		val &= ~BFE_RXCONF_DBCAST;
	else
		val |= BFE_RXCONF_DBCAST;


	CSR_WRITE_4(sc, BFE_CAM_CTRL, 0);
	bfe_cam_write(sc, IF_LLADDR(sc->bfe_ifp), i++);

	if (ifp->if_flags & IFF_ALLMULTI)
		val |= BFE_RXCONF_ALLMULTI;
	else {
		val &= ~BFE_RXCONF_ALLMULTI;
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			bfe_cam_write(sc,
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr), i++);
		}
		IF_ADDR_UNLOCK(ifp);
	}

	CSR_WRITE_4(sc, BFE_RXCONF, val);
	BFE_OR(sc, BFE_CAM_CTRL, BFE_CAM_ENABLE);
}

static void
bfe_dma_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	u_int32_t *ptr;

	ptr = arg;
	*ptr = segs->ds_addr;
}

static void
bfe_dma_map_desc(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bfe_desc *d;

	d = arg;
	/* The chip needs all addresses to be added to BFE_PCI_DMA */
	d->bfe_addr = segs->ds_addr + BFE_PCI_DMA;
}

static void
bfe_release_resources(struct bfe_softc *sc)
{
	device_t dev;
	int i;

	dev = sc->bfe_dev;

	if (sc->bfe_vpd_prodname != NULL)
		free(sc->bfe_vpd_prodname, M_DEVBUF);

	if (sc->bfe_vpd_readonly != NULL)
		free(sc->bfe_vpd_readonly, M_DEVBUF);

	if (sc->bfe_intrhand != NULL)
		bus_teardown_intr(dev, sc->bfe_irq, sc->bfe_intrhand);

	if (sc->bfe_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->bfe_irq);

	if (sc->bfe_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0x10, sc->bfe_res);

	if (sc->bfe_ifp != NULL)
		if_free(sc->bfe_ifp);

	if (sc->bfe_tx_tag != NULL) {
		bus_dmamap_unload(sc->bfe_tx_tag, sc->bfe_tx_map);
		bus_dmamem_free(sc->bfe_tx_tag, sc->bfe_tx_list,
		    sc->bfe_tx_map);
		bus_dma_tag_destroy(sc->bfe_tx_tag);
		sc->bfe_tx_tag = NULL;
	}

	if (sc->bfe_rx_tag != NULL) {
		bus_dmamap_unload(sc->bfe_rx_tag, sc->bfe_rx_map);
		bus_dmamem_free(sc->bfe_rx_tag, sc->bfe_rx_list,
		    sc->bfe_rx_map);
		bus_dma_tag_destroy(sc->bfe_rx_tag);
		sc->bfe_rx_tag = NULL;
	}

	if (sc->bfe_tag != NULL) {
		for(i = 0; i < BFE_TX_LIST_CNT; i++) {
			bus_dmamap_destroy(sc->bfe_tag,
			    sc->bfe_tx_ring[i].bfe_map);
		}
		for(i = 0; i < BFE_RX_LIST_CNT; i++) {
			bus_dmamap_destroy(sc->bfe_tag,
			    sc->bfe_rx_ring[i].bfe_map);
		}
		bus_dma_tag_destroy(sc->bfe_tag);
		sc->bfe_tag = NULL;
	}

	if (sc->bfe_parent_tag != NULL)
		bus_dma_tag_destroy(sc->bfe_parent_tag);

	return;
}

static void
bfe_read_eeprom(struct bfe_softc *sc, u_int8_t *data)
{
	long i;
	u_int16_t *ptr = (u_int16_t *)data;

	for(i = 0; i < 128; i += 2)
		ptr[i/2] = CSR_READ_4(sc, 4096 + i);
}

static int
bfe_wait_bit(struct bfe_softc *sc, u_int32_t reg, u_int32_t bit,
		u_long timeout, const int clear)
{
	u_long i;

	for (i = 0; i < timeout; i++) {
		u_int32_t val = CSR_READ_4(sc, reg);

		if (clear && !(val & bit))
			break;
		if (!clear && (val & bit))
			break;
		DELAY(10);
	}
	if (i == timeout) {
		device_printf(sc->bfe_dev,
		    "BUG!  Timeout waiting for bit %08x of register "
		    "%x to %s.\n", bit, reg, (clear ? "clear" : "set"));
		return (-1);
	}
	return (0);
}

static int
bfe_readphy(struct bfe_softc *sc, u_int32_t reg, u_int32_t *val)
{
	int err;

	/* Clear MII ISR */
	CSR_WRITE_4(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII);
	CSR_WRITE_4(sc, BFE_MDIO_DATA, (BFE_MDIO_SB_START |
				(BFE_MDIO_OP_READ << BFE_MDIO_OP_SHIFT) |
				(sc->bfe_phyaddr << BFE_MDIO_PMD_SHIFT) |
				(reg << BFE_MDIO_RA_SHIFT) |
				(BFE_MDIO_TA_VALID << BFE_MDIO_TA_SHIFT)));
	err = bfe_wait_bit(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII, 100, 0);
	*val = CSR_READ_4(sc, BFE_MDIO_DATA) & BFE_MDIO_DATA_DATA;

	return (err);
}

static int
bfe_writephy(struct bfe_softc *sc, u_int32_t reg, u_int32_t val)
{
	int status;

	CSR_WRITE_4(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII);
	CSR_WRITE_4(sc, BFE_MDIO_DATA, (BFE_MDIO_SB_START |
				(BFE_MDIO_OP_WRITE << BFE_MDIO_OP_SHIFT) |
				(sc->bfe_phyaddr << BFE_MDIO_PMD_SHIFT) |
				(reg << BFE_MDIO_RA_SHIFT) |
				(BFE_MDIO_TA_VALID << BFE_MDIO_TA_SHIFT) |
				(val & BFE_MDIO_DATA_DATA)));
	status = bfe_wait_bit(sc, BFE_EMAC_ISTAT, BFE_EMAC_INT_MII, 100, 0);

	return (status);
}

/*
 * XXX - I think this is handled by the PHY driver, but it can't hurt to do it
 * twice
 */
static int
bfe_setupphy(struct bfe_softc *sc)
{
	u_int32_t val;

	/* Enable activity LED */
	bfe_readphy(sc, 26, &val);
	bfe_writephy(sc, 26, val & 0x7fff);
	bfe_readphy(sc, 26, &val);

	/* Enable traffic meter LED mode */
	bfe_readphy(sc, 27, &val);
	bfe_writephy(sc, 27, val | (1 << 6));

	return (0);
}

static void
bfe_stats_update(struct bfe_softc *sc)
{
	u_long reg;
	u_int32_t *val;

	val = &sc->bfe_hwstats.tx_good_octets;
	for (reg = BFE_TX_GOOD_O; reg <= BFE_TX_PAUSE; reg += 4) {
		*val++ += CSR_READ_4(sc, reg);
	}
	val = &sc->bfe_hwstats.rx_good_octets;
	for (reg = BFE_RX_GOOD_O; reg <= BFE_RX_NPAUSE; reg += 4) {
		*val++ += CSR_READ_4(sc, reg);
	}
}

static void
bfe_txeof(struct bfe_softc *sc)
{
	struct ifnet *ifp;
	int i, chipidx;

	BFE_LOCK_ASSERT(sc);

	ifp = sc->bfe_ifp;

	chipidx = CSR_READ_4(sc, BFE_DMATX_STAT) & BFE_STAT_CDMASK;
	chipidx /= sizeof(struct bfe_desc);

	i = sc->bfe_tx_cons;
	/* Go through the mbufs and free those that have been transmitted */
	while (i != chipidx) {
		struct bfe_data *r = &sc->bfe_tx_ring[i];
		if (r->bfe_mbuf != NULL) {
			ifp->if_opackets++;
			m_freem(r->bfe_mbuf);
			r->bfe_mbuf = NULL;
		}
		bus_dmamap_unload(sc->bfe_tag, r->bfe_map);
		sc->bfe_tx_cnt--;
		BFE_INC(i, BFE_TX_LIST_CNT);
	}

	if (i != sc->bfe_tx_cons) {
		/* we freed up some mbufs */
		sc->bfe_tx_cons = i;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	}

	if (sc->bfe_tx_cnt == 0)
		sc->bfe_watchdog_timer = 0;
}

/* Pass a received packet up the stack */
static void
bfe_rxeof(struct bfe_softc *sc)
{
	struct mbuf *m;
	struct ifnet *ifp;
	struct bfe_rxheader *rxheader;
	struct bfe_data *r;
	int cons;
	u_int32_t status, current, len, flags;

	BFE_LOCK_ASSERT(sc);
	cons = sc->bfe_rx_cons;
	status = CSR_READ_4(sc, BFE_DMARX_STAT);
	current = (status & BFE_STAT_CDMASK) / sizeof(struct bfe_desc);

	ifp = sc->bfe_ifp;

	while (current != cons) {
		r = &sc->bfe_rx_ring[cons];
		m = r->bfe_mbuf;
		rxheader = mtod(m, struct bfe_rxheader*);
		bus_dmamap_sync(sc->bfe_tag, r->bfe_map, BUS_DMASYNC_POSTREAD);
		len = rxheader->len;
		r->bfe_mbuf = NULL;

		bus_dmamap_unload(sc->bfe_tag, r->bfe_map);
		flags = rxheader->flags;

		len -= ETHER_CRC_LEN;

		/* flag an error and try again */
		if ((len > ETHER_MAX_LEN+32) || (flags & BFE_RX_FLAG_ERRORS)) {
			ifp->if_ierrors++;
			if (flags & BFE_RX_FLAG_SERR)
				ifp->if_collisions++;
			bfe_list_newbuf(sc, cons, m);
			BFE_INC(cons, BFE_RX_LIST_CNT);
			continue;
		}

		/* Go past the rx header */
		if (bfe_list_newbuf(sc, cons, NULL) == 0) {
			m_adj(m, BFE_RX_OFFSET);
			m->m_len = m->m_pkthdr.len = len;
		} else {
			bfe_list_newbuf(sc, cons, m);
			ifp->if_ierrors++;
			BFE_INC(cons, BFE_RX_LIST_CNT);
			continue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		BFE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		BFE_LOCK(sc);

		BFE_INC(cons, BFE_RX_LIST_CNT);
	}
	sc->bfe_rx_cons = cons;
}

static void
bfe_intr(void *xsc)
{
	struct bfe_softc *sc = xsc;
	struct ifnet *ifp;
	u_int32_t istat, imask, flag;

	ifp = sc->bfe_ifp;

	BFE_LOCK(sc);

	istat = CSR_READ_4(sc, BFE_ISTAT);
	imask = CSR_READ_4(sc, BFE_IMASK);

	/*
	 * Defer unsolicited interrupts - This is necessary because setting the
	 * chips interrupt mask register to 0 doesn't actually stop the
	 * interrupts
	 */
	istat &= imask;
	CSR_WRITE_4(sc, BFE_ISTAT, istat);
	CSR_READ_4(sc, BFE_ISTAT);

	/* not expecting this interrupt, disregard it */
	if (istat == 0) {
		BFE_UNLOCK(sc);
		return;
	}

	if (istat & BFE_ISTAT_ERRORS) {

		if (istat & BFE_ISTAT_DSCE) {
			device_printf(sc->bfe_dev, "Descriptor Error\n");
			bfe_stop(sc);
			BFE_UNLOCK(sc);
			return;
		}

		if (istat & BFE_ISTAT_DPE) {
			device_printf(sc->bfe_dev,
			    "Descriptor Protocol Error\n");
			bfe_stop(sc);
			BFE_UNLOCK(sc);
			return;
		}
		
		flag = CSR_READ_4(sc, BFE_DMATX_STAT);
		if (flag & BFE_STAT_EMASK)
			ifp->if_oerrors++;

		flag = CSR_READ_4(sc, BFE_DMARX_STAT);
		if (flag & BFE_RX_FLAG_ERRORS)
			ifp->if_ierrors++;

		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		bfe_init_locked(sc);
	}

	/* A packet was received */
	if (istat & BFE_ISTAT_RX)
		bfe_rxeof(sc);

	/* A packet was sent */
	if (istat & BFE_ISTAT_TX)
		bfe_txeof(sc);

	/* We have packets pending, fire them out */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bfe_start_locked(ifp);

	BFE_UNLOCK(sc);
}

static int
bfe_encap(struct bfe_softc *sc, struct mbuf **m_head, u_int32_t *txidx)
{
	struct bfe_desc *d = NULL;
	struct bfe_data *r = NULL;
	struct mbuf	*m;
	u_int32_t	   frag, cur, cnt = 0;
	int chainlen = 0;
	int error;

	if (BFE_TX_LIST_CNT - sc->bfe_tx_cnt < 2)
		return (ENOBUFS);

	/*
	 * Count the number of frags in this chain to see if
	 * we need to m_defrag.  Since the descriptor list is shared
	 * by all packets, we'll m_defrag long chains so that they
	 * do not use up the entire list, even if they would fit.
	 */
	for(m = *m_head; m != NULL; m = m->m_next)
		chainlen++;


	if ((chainlen > BFE_TX_LIST_CNT / 4) ||
			((BFE_TX_LIST_CNT - (chainlen + sc->bfe_tx_cnt)) < 2)) {
		m = m_defrag(*m_head, M_DONTWAIT);
		if (m == NULL)
			return (ENOBUFS);
		*m_head = m;
	}

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	cur = frag = *txidx;
	cnt = 0;

	for(m = *m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((BFE_TX_LIST_CNT - (sc->bfe_tx_cnt + cnt)) < 2)
				return (ENOBUFS);

			d = &sc->bfe_tx_list[cur];
			r = &sc->bfe_tx_ring[cur];
			d->bfe_ctrl = BFE_DESC_LEN & m->m_len;
			/* always intterupt on completion */
			d->bfe_ctrl |= BFE_DESC_IOC;
			if (cnt == 0)
				/* Set start of frame */
				d->bfe_ctrl |= BFE_DESC_SOF;
			if (cur == BFE_TX_LIST_CNT - 1)
				/*
				 * Tell the chip to wrap to the start of
				 * the descriptor list
				 */
				d->bfe_ctrl |= BFE_DESC_EOT;

			error = bus_dmamap_load(sc->bfe_tag,
			    r->bfe_map, mtod(m, void*), m->m_len,
			    bfe_dma_map_desc, d, BUS_DMA_NOWAIT);
			if (error)
				return (ENOBUFS);
			bus_dmamap_sync(sc->bfe_tag, r->bfe_map,
			    BUS_DMASYNC_PREWRITE);

			frag = cur;
			BFE_INC(cur, BFE_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return (ENOBUFS);

	sc->bfe_tx_list[frag].bfe_ctrl |= BFE_DESC_EOF;
	sc->bfe_tx_ring[frag].bfe_mbuf = *m_head;
	bus_dmamap_sync(sc->bfe_tx_tag, sc->bfe_tx_map, BUS_DMASYNC_PREWRITE);

	*txidx = cur;
	sc->bfe_tx_cnt += cnt;
	return (0);
}

/*
 * Set up to transmit a packet.
 */
static void
bfe_start(struct ifnet *ifp)
{
	BFE_LOCK((struct bfe_softc *)ifp->if_softc);
	bfe_start_locked(ifp);
	BFE_UNLOCK((struct bfe_softc *)ifp->if_softc);
}

/*
 * Set up to transmit a packet. The softc is already locked.
 */
static void
bfe_start_locked(struct ifnet *ifp)
{
	struct bfe_softc *sc;
	struct mbuf *m_head = NULL;
	int idx, queued = 0;

	sc = ifp->if_softc;
	idx = sc->bfe_tx_prod;

	BFE_LOCK_ASSERT(sc);

	/*
	 * Not much point trying to send if the link is down
	 * or we have nothing to send.
	 */
	if (!sc->bfe_link && ifp->if_snd.ifq_len < 10)
		return;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	while (sc->bfe_tx_ring[idx].bfe_mbuf == NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the tx ring.  If we dont have
		 * enough room, let the chip drain the ring.
		 */
		if (bfe_encap(sc, &m_head, &idx)) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		queued++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);
	}

	if (queued) {
		sc->bfe_tx_prod = idx;
		/* Transmit - twice due to apparent hardware bug */
		CSR_WRITE_4(sc, BFE_DMATX_PTR, idx * sizeof(struct bfe_desc));
		CSR_WRITE_4(sc, BFE_DMATX_PTR, idx * sizeof(struct bfe_desc));

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		sc->bfe_watchdog_timer = 5;
	}
}

static void
bfe_init(void *xsc)
{
	BFE_LOCK((struct bfe_softc *)xsc);
	bfe_init_locked(xsc);
	BFE_UNLOCK((struct bfe_softc *)xsc);
}

static void
bfe_init_locked(void *xsc)
{
	struct bfe_softc *sc = (struct bfe_softc*)xsc;
	struct ifnet *ifp = sc->bfe_ifp;
	struct mii_data *mii;

	BFE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->bfe_miibus);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	bfe_stop(sc);
	bfe_chip_reset(sc);

	if (bfe_list_rx_init(sc) == ENOBUFS) {
		device_printf(sc->bfe_dev,
		    "%s: Not enough memory for list buffers\n", __func__);
		bfe_stop(sc);
		return;
	}

	bfe_set_rx_mode(sc);

	/* Enable the chip and core */
	BFE_OR(sc, BFE_ENET_CTRL, BFE_ENET_ENABLE);
	/* Enable interrupts */
	CSR_WRITE_4(sc, BFE_IMASK, BFE_IMASK_DEF);

	/* Clear link state and change media. */
	sc->bfe_link = 0;
	mii_mediachg(mii);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->bfe_stat_co, hz, bfe_tick, sc);
}

/*
 * Set media options.
 */
static int
bfe_ifmedia_upd(struct ifnet *ifp)
{
	struct bfe_softc *sc;
	struct mii_data *mii;
	int error;

	sc = ifp->if_softc;
	BFE_LOCK(sc);

	mii = device_get_softc(sc->bfe_miibus);
	if (mii->mii_instance) {
		struct mii_softc *miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
				miisc = LIST_NEXT(miisc, mii_list))
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);
	BFE_UNLOCK(sc);

	return (error);
}

/*
 * Report current media status.
 */
static void
bfe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bfe_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	BFE_LOCK(sc);
	mii = device_get_softc(sc->bfe_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	BFE_UNLOCK(sc);
}

static int
bfe_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct bfe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct mii_data *mii;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		BFE_LOCK(sc);
		if (ifp->if_flags & IFF_UP)
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				bfe_set_rx_mode(sc);
			else
				bfe_init_locked(sc);
		else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			bfe_stop(sc);
		BFE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		BFE_LOCK(sc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			bfe_set_rx_mode(sc);
		BFE_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->bfe_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
bfe_watchdog(struct bfe_softc *sc)
{
	struct ifnet *ifp;

	BFE_LOCK_ASSERT(sc);

	if (sc->bfe_watchdog_timer == 0 || --sc->bfe_watchdog_timer)
		return;

	ifp = sc->bfe_ifp;

	device_printf(sc->bfe_dev, "watchdog timeout -- resetting\n");

	ifp->if_oerrors++;
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	bfe_init_locked(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bfe_start_locked(ifp);
}

static void
bfe_tick(void *xsc)
{
	struct bfe_softc *sc = xsc;
	struct mii_data *mii;

	BFE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->bfe_miibus);
	mii_tick(mii);
	bfe_stats_update(sc);
	bfe_watchdog(sc);
	callout_reset(&sc->bfe_stat_co, hz, bfe_tick, sc);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
bfe_stop(struct bfe_softc *sc)
{
	struct ifnet *ifp;

	BFE_LOCK_ASSERT(sc);

	ifp = sc->bfe_ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->bfe_link = 0;
	callout_stop(&sc->bfe_stat_co);
	sc->bfe_watchdog_timer = 0;

	bfe_chip_halt(sc);
	bfe_tx_ring_free(sc);
	bfe_rx_ring_free(sc);
}
