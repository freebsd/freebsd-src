/*	$OpenBSD: if_rtwn.c,v 1.6 2015/08/28 00:03:53 deraadt Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2015 Stefan Sperling <stsp@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for Realtek RTL8188CE
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/firmware.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_ratectl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/rtwn/if_rtwnreg.h>

#define	RTWN_DEBUG
#ifdef RTWN_DEBUG
#define	DPRINTF(x)	do { if (sc->sc_debug > 0) printf x; } while (0)
#define	DPRINTFN(n, x)	do { if (sc->sc_debug >= (n)) printf x; } while (0)
#else
#define	DPRINTF(x)
#define	DPRINTFN(n, x)
#endif

/*
 * PCI configuration space registers.
 */
#define	RTWN_PCI_IOBA		0x10	/* i/o mapped base */
#define	RTWN_PCI_MMBA		0x18	/* memory mapped base */

#define RTWN_INT_ENABLE	(R92C_IMR_ROK | R92C_IMR_VODOK | R92C_IMR_VIDOK | \
			R92C_IMR_BEDOK | R92C_IMR_BKDOK | R92C_IMR_MGNTDOK | \
			R92C_IMR_HIGHDOK | R92C_IMR_BDOK | R92C_IMR_RDU | \
			R92C_IMR_RXFOVW)

struct rtwn_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};


static const struct rtwn_ident rtwn_ident_table[] = {
	{ 0x10ec, 0x8176, "Realtek RTL8188CE" },
	{ 0, 0, NULL }
};


static void	rtwn_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static void	rtwn_setup_rx_desc(struct rtwn_softc *, struct r92c_rx_desc *,
		    bus_addr_t, size_t, int);
static int	rtwn_alloc_rx_list(struct rtwn_softc *);
static void	rtwn_reset_rx_list(struct rtwn_softc *);
static void	rtwn_free_rx_list(struct rtwn_softc *);
static int	rtwn_alloc_tx_list(struct rtwn_softc *, int);
static void	rtwn_reset_tx_list(struct rtwn_softc *, int);
static void	rtwn_free_tx_list(struct rtwn_softc *, int);
static struct ieee80211vap *rtwn_vap_create(struct ieee80211com *,
		    const char [IFNAMSIZ], int, enum ieee80211_opmode, int,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN]);
static void	rtwn_vap_delete(struct ieee80211vap *);
static void	rtwn_write_1(struct rtwn_softc *, uint16_t, uint8_t);
static void	rtwn_write_2(struct rtwn_softc *, uint16_t, uint16_t);
static void	rtwn_write_4(struct rtwn_softc *, uint16_t, uint32_t);
static uint8_t	rtwn_read_1(struct rtwn_softc *, uint16_t);
static uint16_t	rtwn_read_2(struct rtwn_softc *, uint16_t);
static uint32_t	rtwn_read_4(struct rtwn_softc *, uint16_t);
static int	rtwn_fw_cmd(struct rtwn_softc *, uint8_t, const void *, int);
static void	rtwn_rf_write(struct rtwn_softc *, int, uint8_t, uint32_t);
static uint32_t	rtwn_rf_read(struct rtwn_softc *, int, uint8_t);
static int	rtwn_llt_write(struct rtwn_softc *, uint32_t, uint32_t);
static uint8_t	rtwn_efuse_read_1(struct rtwn_softc *, uint16_t);
static void	rtwn_efuse_read(struct rtwn_softc *);
static int	rtwn_read_chipid(struct rtwn_softc *);
static void	rtwn_read_rom(struct rtwn_softc *);
static int	rtwn_ra_init(struct rtwn_softc *);
static void	rtwn_tsf_sync_enable(struct rtwn_softc *);
static void	rtwn_set_led(struct rtwn_softc *, int, int);
static void	rtwn_calib_to(void *);
static int	rtwn_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static int	rtwn_updateedca(struct ieee80211com *);
static void	rtwn_update_avgrssi(struct rtwn_softc *, int, int8_t);
static int8_t	rtwn_get_rssi(struct rtwn_softc *, int, void *);
static void	rtwn_rx_frame(struct rtwn_softc *, struct r92c_rx_desc *,
		    struct rtwn_rx_data *, int);
static int	rtwn_tx(struct rtwn_softc *, struct mbuf *,
		    struct ieee80211_node *);
static void	rtwn_tx_done(struct rtwn_softc *, int);
static int	rtwn_raw_xmit(struct ieee80211_node *, struct mbuf *,
		    const struct ieee80211_bpf_params *);
static int	rtwn_transmit(struct ieee80211com *, struct mbuf *);
static void	rtwn_parent(struct ieee80211com *);
static void	rtwn_start(struct rtwn_softc *sc);
static void	rtwn_watchdog(void *);
static int	rtwn_power_on(struct rtwn_softc *);
static int	rtwn_llt_init(struct rtwn_softc *);
static void	rtwn_fw_reset(struct rtwn_softc *);
static void	rtwn_fw_loadpage(struct rtwn_softc *, int, const uint8_t *,
		    int);
static int	rtwn_load_firmware(struct rtwn_softc *);
static int	rtwn_dma_init(struct rtwn_softc *);
static void	rtwn_mac_init(struct rtwn_softc *);
static void	rtwn_bb_init(struct rtwn_softc *);
static void	rtwn_rf_init(struct rtwn_softc *);
static void	rtwn_cam_init(struct rtwn_softc *);
static void	rtwn_pa_bias_init(struct rtwn_softc *);
static void	rtwn_rxfilter_init(struct rtwn_softc *);
static void	rtwn_edca_init(struct rtwn_softc *);
static void	rtwn_write_txpower(struct rtwn_softc *, int, uint16_t[]);
static void	rtwn_get_txpower(struct rtwn_softc *, int,
		    struct ieee80211_channel *, struct ieee80211_channel *,
		    uint16_t[]);
static void	rtwn_set_txpower(struct rtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
static void	rtwn_set_rx_bssid_all(struct rtwn_softc *, int);
static void	rtwn_set_gain(struct rtwn_softc *, uint8_t);
static void	rtwn_scan_start(struct ieee80211com *);
static void	rtwn_scan_end(struct ieee80211com *);
static void	rtwn_getradiocaps(struct ieee80211com *, int, int *,
		    struct ieee80211_channel[]);
static void	rtwn_set_channel(struct ieee80211com *);
static void	rtwn_update_mcast(struct ieee80211com *);
static void	rtwn_set_chan(struct rtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
static int	rtwn_iq_calib_chain(struct rtwn_softc *, int, uint16_t[2],
		    uint16_t[2]);
static void	rtwn_iq_calib_run(struct rtwn_softc *, int, uint16_t[2][2],
		    uint16_t[2][2]);
static int	rtwn_iq_calib_compare_results(uint16_t[2][2], uint16_t[2][2],
		    uint16_t[2][2], uint16_t[2][2], int);
static void	rtwn_iq_calib_write_results(struct rtwn_softc *, uint16_t[2],
		    uint16_t[2], int);
static void	rtwn_iq_calib(struct rtwn_softc *);
static void	rtwn_lc_calib(struct rtwn_softc *);
static void	rtwn_temp_calib(struct rtwn_softc *);
static int	rtwn_init(struct rtwn_softc *);
static void	rtwn_stop_locked(struct rtwn_softc *);
static void	rtwn_stop(struct rtwn_softc *);
static void	rtwn_intr(void *);

/* Aliases. */
#define	rtwn_bb_write	rtwn_write_4
#define rtwn_bb_read	rtwn_read_4

static int	rtwn_probe(device_t);
static int	rtwn_attach(device_t);
static int	rtwn_detach(device_t);
static int	rtwn_shutdown(device_t);
static int	rtwn_suspend(device_t);
static int	rtwn_resume(device_t);

static device_method_t rtwn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rtwn_probe),
	DEVMETHOD(device_attach,	rtwn_attach),
	DEVMETHOD(device_detach,	rtwn_detach),
	DEVMETHOD(device_shutdown,	rtwn_shutdown),
	DEVMETHOD(device_suspend,	rtwn_suspend),
	DEVMETHOD(device_resume,	rtwn_resume),

	DEVMETHOD_END
};

static driver_t rtwn_driver = {
	"rtwn",
	rtwn_methods,
	sizeof (struct rtwn_softc)
};
static devclass_t rtwn_devclass;

DRIVER_MODULE(rtwn, pci, rtwn_driver, rtwn_devclass, NULL, NULL);

MODULE_VERSION(rtwn, 1);

MODULE_DEPEND(rtwn, pci,  1, 1, 1);
MODULE_DEPEND(rtwn, wlan, 1, 1, 1);
MODULE_DEPEND(rtwn, firmware, 1, 1, 1);

static const uint8_t rtwn_chan_2ghz[] =
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

static int
rtwn_probe(device_t dev)
{
	const struct rtwn_ident *ident;

	for (ident = rtwn_ident_table; ident->name != NULL; ident++) {
		if (pci_get_vendor(dev) == ident->vendor &&
		    pci_get_device(dev) == ident->device) {
			device_set_desc(dev, ident->name);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
rtwn_attach(device_t dev)
{
	struct rtwn_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t lcsr;
	int i, count, error, rid;

	sc->sc_dev = dev;
	sc->sc_debug = 0;

	/*
	 * Get the offset of the PCI Express Capability Structure in PCI
	 * Configuration Space.
	 */
	error = pci_find_cap(dev, PCIY_EXPRESS, &sc->sc_cap_off);
	if (error != 0) {
		device_printf(dev, "PCIe capability structure not found!\n");
		return (error);
	}

	/* Enable bus-mastering. */
	pci_enable_busmaster(dev);

	rid = PCIR_BAR(2);
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "can't map mem space\n");
		return (ENOMEM);
	}
	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	/* Install interrupt handler. */
	count = 1;
	rid = 0;
	if (pci_alloc_msi(dev, &count) == 0)
		rid = 1;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE |
	    (rid != 0 ? 0 : RF_SHAREABLE));
	if (sc->irq == NULL) {
		device_printf(dev, "can't map interrupt\n");
		return (ENXIO);
	}

	RTWN_LOCK_INIT(sc);
	callout_init_mtx(&sc->calib_to, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->watchdog_to, &sc->sc_mtx, 0);
	mbufq_init(&sc->sc_snd, ifqmaxlen);

	error = rtwn_read_chipid(sc);
	if (error != 0) {
		device_printf(dev, "unsupported test chip\n");
		goto fail;
	}

	/* Disable PCIe Active State Power Management (ASPM). */
	lcsr = pci_read_config(sc->sc_dev, sc->sc_cap_off + PCIER_LINK_CTL, 4);
	lcsr &= ~PCIEM_LINK_CTL_ASPMC;
	pci_write_config(sc->sc_dev, sc->sc_cap_off + PCIER_LINK_CTL, lcsr, 4);

	/* Allocate Tx/Rx buffers. */
	error = rtwn_alloc_rx_list(sc);
	if (error != 0) {
		device_printf(dev, "could not allocate Rx buffers\n");
		goto fail;
	}
	for (i = 0; i < RTWN_NTXQUEUES; i++) {
		error = rtwn_alloc_tx_list(sc, i);
		if (error != 0) {
			device_printf(dev, "could not allocate Tx buffers\n");
			goto fail;
		}
	}

	/* Determine number of Tx/Rx chains. */
	if (sc->chip & RTWN_CHIP_92C) {
		sc->ntxchains = (sc->chip & RTWN_CHIP_92C_1T2R) ? 1 : 2;
		sc->nrxchains = 2;
	} else {
		sc->ntxchains = 1;
		sc->nrxchains = 1;
	}
	rtwn_read_rom(sc);

	device_printf(sc->sc_dev, "MAC/BB RTL%s, RF 6052 %dT%dR\n",
	    (sc->chip & RTWN_CHIP_92C) ? "8192CE" : "8188CE",
	    sc->ntxchains, sc->nrxchains);

	ic->ic_softc = sc;
	ic->ic_name = device_get_nameunit(dev);
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */

	/* set device capabilities */
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_WPA		/* capable of WPA1+WPA2 */
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
		| IEEE80211_C_WME		/* 802.11e */
		;

	/* XXX TODO: setup regdomain if R92C_CHANNEL_PLAN_BY_HW bit is set. */

	rtwn_getradiocaps(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	ieee80211_ifattach(ic);

	ic->ic_wme.wme_update = rtwn_updateedca;
	ic->ic_update_mcast = rtwn_update_mcast;
	ic->ic_scan_start = rtwn_scan_start;
	ic->ic_scan_end = rtwn_scan_end;
	ic->ic_getradiocaps = rtwn_getradiocaps;
	ic->ic_set_channel = rtwn_set_channel;
	ic->ic_raw_xmit = rtwn_raw_xmit;
	ic->ic_transmit = rtwn_transmit;
	ic->ic_parent = rtwn_parent;
	ic->ic_vap_create = rtwn_vap_create;
	ic->ic_vap_delete = rtwn_vap_delete;

	ieee80211_radiotap_attach(ic,
	    &sc->sc_txtap.wt_ihdr, sizeof(sc->sc_txtap),
		RTWN_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
		RTWN_RX_RADIOTAP_PRESENT);

	/*
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, rtwn_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "can't establish interrupt, error %d\n",
		    error);
		goto fail;
	}

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

fail:
	rtwn_detach(dev);
	return (error);
}
	

static int
rtwn_detach(device_t dev)
{
	struct rtwn_softc *sc = device_get_softc(dev);
	int i;

	if (sc->sc_ic.ic_softc != NULL) {
		rtwn_stop(sc);

		callout_drain(&sc->calib_to);
		callout_drain(&sc->watchdog_to);
		ieee80211_ifdetach(&sc->sc_ic);
		mbufq_drain(&sc->sc_snd);
	}

	/* Uninstall interrupt handler. */
	if (sc->irq != NULL) {
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->irq),
		    sc->irq);
		pci_release_msi(dev);
	}

	/* Free Tx/Rx buffers. */
	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_free_tx_list(sc, i);
	rtwn_free_rx_list(sc);

	if (sc->mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem), sc->mem);

	RTWN_LOCK_DESTROY(sc);
	return (0);
}

static int
rtwn_shutdown(device_t dev)
{

	return (0);
}

static int
rtwn_suspend(device_t dev)
{
	return (0);
}

static int
rtwn_resume(device_t dev)
{

	return (0);
}

static void
rtwn_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	KASSERT(nsegs == 1, ("too many DMA segments, %d should be 1", nsegs));
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static void
rtwn_setup_rx_desc(struct rtwn_softc *sc, struct r92c_rx_desc *desc,
    bus_addr_t addr, size_t len, int idx)
{

	memset(desc, 0, sizeof(*desc));
	desc->rxdw0 = htole32(SM(R92C_RXDW0_PKTLEN, len) |
		((idx == RTWN_RX_LIST_COUNT - 1) ? R92C_RXDW0_EOR : 0));
	desc->rxbufaddr = htole32(addr);
	bus_space_barrier(sc->sc_st, sc->sc_sh, 0, sc->sc_mapsize,
	    BUS_SPACE_BARRIER_WRITE);
	desc->rxdw0 |= htole32(R92C_RXDW0_OWN);
}

static int
rtwn_alloc_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	bus_size_t size;
	int i, error;

	/* Allocate Rx descriptors. */
	size = sizeof(struct r92c_rx_desc) * RTWN_RX_LIST_COUNT;
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, size, 0, NULL, NULL, &rx_ring->desc_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create rx desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(rx_ring->desc_dmat, (void **)&rx_ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &rx_ring->desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate rx desc\n");
		goto fail;
	}
	error = bus_dmamap_load(rx_ring->desc_dmat, rx_ring->desc_map,
	    rx_ring->desc, size, rtwn_dma_map_addr, &rx_ring->paddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load rx desc DMA map\n");
		goto fail;
	}
	bus_dmamap_sync(rx_ring->desc_dmat, rx_ring->desc_map,
	    BUS_DMASYNC_PREWRITE);

	/* Create RX buffer DMA tag. */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    1, MCLBYTES, 0, NULL, NULL, &rx_ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create rx buf DMA tag\n");
		goto fail;
	}

	/* Allocate Rx buffers. */
	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];
		error = bus_dmamap_create(rx_ring->data_dmat, 0, &rx_data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not create rx buf DMA map\n");
			goto fail;
		}

		rx_data->m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (rx_data->m == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(rx_ring->data_dmat, rx_data->map,
		    mtod(rx_data->m, void *), MCLBYTES, rtwn_dma_map_addr,
		    &rx_data->paddr, BUS_DMA_NOWAIT);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load rx buf DMA map");
			goto fail;
		}

		rtwn_setup_rx_desc(sc, &rx_ring->desc[i], rx_data->paddr,
		    MCLBYTES, i);
	}
	return (0);

fail:
	rtwn_free_rx_list(sc);
	return (error);
}

static void
rtwn_reset_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i;

	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];
		rtwn_setup_rx_desc(sc, &rx_ring->desc[i], rx_data->paddr,
		    MCLBYTES, i);
	}
}

static void
rtwn_free_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i;

	if (rx_ring->desc_dmat != NULL) {
		if (rx_ring->desc != NULL) {
			bus_dmamap_unload(rx_ring->desc_dmat,
			    rx_ring->desc_map);
			bus_dmamem_free(rx_ring->desc_dmat, rx_ring->desc,
			    rx_ring->desc_map);
			rx_ring->desc = NULL;
		}
		bus_dma_tag_destroy(rx_ring->desc_dmat);
		rx_ring->desc_dmat = NULL;
	}

	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];

		if (rx_data->m != NULL) {
			bus_dmamap_unload(rx_ring->data_dmat, rx_data->map);
			m_freem(rx_data->m);
			rx_data->m = NULL;
		}
		bus_dmamap_destroy(rx_ring->data_dmat, rx_data->map);
		rx_data->map = NULL;
	}
	if (rx_ring->data_dmat != NULL) {
		bus_dma_tag_destroy(rx_ring->data_dmat);
		rx_ring->data_dmat = NULL;
	}
}

static int
rtwn_alloc_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	bus_size_t size;
	int i, error;

	size = sizeof(struct r92c_tx_desc) * RTWN_TX_LIST_COUNT;
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, size, 0, NULL, NULL, &tx_ring->desc_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create tx ring DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(tx_ring->desc_dmat, (void **)&tx_ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &tx_ring->desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev, "can't map tx ring DMA memory\n");
		goto fail;
	}
	error = bus_dmamap_load(tx_ring->desc_dmat, tx_ring->desc_map,
	    tx_ring->desc, size, rtwn_dma_map_addr, &tx_ring->paddr,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    1, MCLBYTES, 0, NULL, NULL, &tx_ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create tx buf DMA tag\n");
		goto fail;
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc *desc = &tx_ring->desc[i];

		/* setup tx desc */
		desc->nextdescaddr = htole32(tx_ring->paddr +
		    + sizeof(struct r92c_tx_desc)
		    * ((i + 1) % RTWN_TX_LIST_COUNT));
		tx_data = &tx_ring->tx_data[i];
		error = bus_dmamap_create(tx_ring->data_dmat, 0, &tx_data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not create tx buf DMA map\n");
			goto fail;
		}
		tx_data->m = NULL;
		tx_data->ni = NULL;
	}
	return (0);

fail:
	rtwn_free_tx_list(sc, qid);
	return (error);
}

static void
rtwn_reset_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	int i;

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc *desc = &tx_ring->desc[i];
		struct rtwn_tx_data *tx_data = &tx_ring->tx_data[i];

		memset(desc, 0, sizeof(*desc) -
		    (sizeof(desc->reserved) + sizeof(desc->nextdescaddr64) +
		    sizeof(desc->nextdescaddr)));

		if (tx_data->m != NULL) {
			bus_dmamap_unload(tx_ring->data_dmat, tx_data->map);
			m_freem(tx_data->m);
			tx_data->m = NULL;
		}
		if (tx_data->ni != NULL) {
			ieee80211_free_node(tx_data->ni);
			tx_data->ni = NULL;
		}
	}

	bus_dmamap_sync(tx_ring->desc_dmat, tx_ring->desc_map,
	    BUS_DMASYNC_POSTWRITE);

	sc->qfullmsk &= ~(1 << qid);
	tx_ring->queued = 0;
	tx_ring->cur = 0;
}

static void
rtwn_free_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	int i;

	if (tx_ring->desc_dmat != NULL) {
		if (tx_ring->desc != NULL) {
			bus_dmamap_unload(tx_ring->desc_dmat,
			    tx_ring->desc_map);
			bus_dmamem_free(tx_ring->desc_dmat, tx_ring->desc,
			    tx_ring->desc_map);
		}
		bus_dma_tag_destroy(tx_ring->desc_dmat);
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		tx_data = &tx_ring->tx_data[i];

		if (tx_data->m != NULL) {
			bus_dmamap_unload(tx_ring->data_dmat, tx_data->map);
			m_freem(tx_data->m);
			tx_data->m = NULL;
		}
	}
	if (tx_ring->data_dmat != NULL) {
		bus_dma_tag_destroy(tx_ring->data_dmat);
		tx_ring->data_dmat = NULL;
	}

	sc->qfullmsk &= ~(1 << qid);
	tx_ring->queued = 0;
	tx_ring->cur = 0;
}


static struct ieee80211vap *
rtwn_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct rtwn_vap *rvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))
		return (NULL);

	rvp = malloc(sizeof(struct rtwn_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &rvp->vap;
	if (ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid) != 0) {
		/* out of memory */
		 free(rvp, M_80211_VAP);
		 return (NULL);
	}

	/* Override state transition machine. */
	rvp->newstate = vap->iv_newstate;
	vap->iv_newstate = rtwn_newstate;

	/* Complete setup. */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);
	ic->ic_opmode = opmode;
	return (vap);
}

static void
rtwn_vap_delete(struct ieee80211vap *vap)
{
	struct rtwn_vap *rvp = RTWN_VAP(vap);

	ieee80211_vap_detach(vap);
	free(rvp, M_80211_VAP);
}

static void
rtwn_write_1(struct rtwn_softc *sc, uint16_t addr, uint8_t val)
{

	bus_space_write_1(sc->sc_st, sc->sc_sh, addr, val);
}

static void
rtwn_write_2(struct rtwn_softc *sc, uint16_t addr, uint16_t val)
{

	val = htole16(val);
	bus_space_write_2(sc->sc_st, sc->sc_sh, addr, val);
}

static void
rtwn_write_4(struct rtwn_softc *sc, uint16_t addr, uint32_t val)
{

	val = htole32(val);
	bus_space_write_4(sc->sc_st, sc->sc_sh, addr, val);
}

static uint8_t
rtwn_read_1(struct rtwn_softc *sc, uint16_t addr)
{

	return (bus_space_read_1(sc->sc_st, sc->sc_sh, addr));
}

static uint16_t
rtwn_read_2(struct rtwn_softc *sc, uint16_t addr)
{

	return (bus_space_read_2(sc->sc_st, sc->sc_sh, addr));
}

static uint32_t
rtwn_read_4(struct rtwn_softc *sc, uint16_t addr)
{

	return (bus_space_read_4(sc->sc_st, sc->sc_sh, addr));
}

static int
rtwn_fw_cmd(struct rtwn_softc *sc, uint8_t id, const void *buf, int len)
{
	struct r92c_fw_cmd cmd;
	int ntries;

	/* Wait for current FW box to be empty. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(rtwn_read_1(sc, R92C_HMETFR) & (1 << sc->fwcur)))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "could not send firmware command %d\n", id);
		return (ETIMEDOUT);
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.id = id;
	if (len > 3)
		cmd.id |= R92C_CMD_FLAG_EXT;
	KASSERT(len <= sizeof(cmd.msg), ("rtwn_fw_cmd\n"));
	memcpy(cmd.msg, buf, len);

	/* Write the first word last since that will trigger the FW. */
	rtwn_write_2(sc, R92C_HMEBOX_EXT(sc->fwcur), *((uint8_t *)&cmd + 4));
	rtwn_write_4(sc, R92C_HMEBOX(sc->fwcur), *((uint8_t *)&cmd + 0));

	sc->fwcur = (sc->fwcur + 1) % R92C_H2C_NBOX;

	/* Give firmware some time for processing. */
	DELAY(2000);

	return (0);
}

static void
rtwn_rf_write(struct rtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{
	rtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) |
	    SM(R92C_LSSI_PARAM_DATA, val));
}

static uint32_t
rtwn_rf_read(struct rtwn_softc *sc, int chain, uint8_t addr)
{
	uint32_t reg[R92C_MAX_CHAINS], val;

	reg[0] = rtwn_bb_read(sc, R92C_HSSI_PARAM2(0));
	if (chain != 0)
		reg[chain] = rtwn_bb_read(sc, R92C_HSSI_PARAM2(chain));

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] & ~R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	if (rtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI)
		val = rtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	else
		val = rtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	return (MS(val, R92C_LSSI_READBACK_DATA));
}

static int
rtwn_llt_write(struct rtwn_softc *sc, uint32_t addr, uint32_t data)
{
	int ntries;

	rtwn_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(rtwn_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return (0);
		DELAY(5);
	}
	return (ETIMEDOUT);
}

static uint8_t
rtwn_efuse_read_1(struct rtwn_softc *sc, uint16_t addr)
{
	uint32_t reg;
	int ntries;

	reg = rtwn_read_4(sc, R92C_EFUSE_CTRL);
	reg = RW(reg, R92C_EFUSE_CTRL_ADDR, addr);
	reg &= ~R92C_EFUSE_CTRL_VALID;
	rtwn_write_4(sc, R92C_EFUSE_CTRL, reg);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rtwn_read_4(sc, R92C_EFUSE_CTRL);
		if (reg & R92C_EFUSE_CTRL_VALID)
			return (MS(reg, R92C_EFUSE_CTRL_DATA));
		DELAY(5);
	}
	device_printf(sc->sc_dev,
	    "could not read efuse byte at address 0x%x\n", addr);
	return (0xff);
}

static void
rtwn_efuse_read(struct rtwn_softc *sc)
{
	uint8_t *rom = (uint8_t *)&sc->rom;
	uint16_t addr = 0;
	uint32_t reg;
	uint8_t off, msk;
	int i;

	reg = rtwn_read_2(sc, R92C_SYS_ISO_CTRL);
	if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) {
		rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
		    reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
	}
	reg = rtwn_read_2(sc, R92C_SYS_FUNC_EN);
	if (!(reg & R92C_SYS_FUNC_EN_ELDR)) {
		rtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    reg | R92C_SYS_FUNC_EN_ELDR);
	}
	reg = rtwn_read_2(sc, R92C_SYS_CLKR);
	if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) !=
	    (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) {
		rtwn_write_2(sc, R92C_SYS_CLKR,
		    reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);
	}
	memset(&sc->rom, 0xff, sizeof(sc->rom));
	while (addr < 512) {
		reg = rtwn_efuse_read_1(sc, addr);
		if (reg == 0xff)
			break;
		addr++;
		off = reg >> 4;
		msk = reg & 0xf;
		for (i = 0; i < 4; i++) {
			if (msk & (1 << i))
				continue;
			rom[off * 8 + i * 2 + 0] =
			    rtwn_efuse_read_1(sc, addr);
			addr++;
			rom[off * 8 + i * 2 + 1] =
			    rtwn_efuse_read_1(sc, addr);
			addr++;
		}
	}
#ifdef RTWN_DEBUG
	if (sc->sc_debug >= 2) {
		/* Dump ROM content. */
		printf("\n");
		for (i = 0; i < sizeof(sc->rom); i++)
			printf("%02x:", rom[i]);
		printf("\n");
	}
#endif
}

static int
rtwn_read_chipid(struct rtwn_softc *sc)
{
	uint32_t reg;

	reg = rtwn_read_4(sc, R92C_SYS_CFG);
	if (reg & R92C_SYS_CFG_TRP_VAUX_EN)
		/* Unsupported test chip. */
		return (EIO);

	if (reg & R92C_SYS_CFG_TYPE_92C) {
		sc->chip |= RTWN_CHIP_92C;
		/* Check if it is a castrated 8192C. */
		if (MS(rtwn_read_4(sc, R92C_HPON_FSM),
		    R92C_HPON_FSM_CHIP_BONDING_ID) ==
		    R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R)
			sc->chip |= RTWN_CHIP_92C_1T2R;
	}
	if (reg & R92C_SYS_CFG_VENDOR_UMC) {
		sc->chip |= RTWN_CHIP_UMC;
		if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) == 0)
			sc->chip |= RTWN_CHIP_UMC_A_CUT;
	}
	return (0);
}

static void
rtwn_read_rom(struct rtwn_softc *sc)
{
	struct r92c_rom *rom = &sc->rom;

	/* Read full ROM image. */
	rtwn_efuse_read(sc);

	if (rom->id != 0x8129)
		device_printf(sc->sc_dev, "invalid EEPROM ID 0x%x\n", rom->id);

	/* XXX Weird but this is what the vendor driver does. */
	sc->pa_setting = rtwn_efuse_read_1(sc, 0x1fa);
	DPRINTF(("PA setting=0x%x\n", sc->pa_setting));

	sc->board_type = MS(rom->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);

	sc->regulatory = MS(rom->rf_opt1, R92C_ROM_RF1_REGULATORY);
	DPRINTF(("regulatory type=%d\n", sc->regulatory));

	IEEE80211_ADDR_COPY(sc->sc_ic.ic_macaddr, rom->macaddr);
}

static __inline uint8_t
rate2ridx(uint8_t rate)
{
	switch (rate) {
	case 12:	return 4;
	case 18:	return 5;
	case 24:	return 6;
	case 36:	return 7;
	case 48:	return 8;
	case 72:	return 9;
	case 96:	return 10;
	case 108:	return 11;
	case 2:		return 0;
	case 4:		return 1;
	case 11:	return 2;
	case 22:	return 3;
	default:	return RTWN_RIDX_UNKNOWN;
	}
}

/*
 * Initialize rate adaptation in firmware.
 */
static int
rtwn_ra_init(struct rtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = ieee80211_ref_node(vap->iv_bss);
	struct ieee80211_rateset *rs = &ni->ni_rates;
	struct r92c_fw_cmd_macid_cfg cmd;
	uint32_t rates, basicrates;
	uint8_t maxrate, maxbasicrate, mode, ridx;
	int error, i;

	/* Get normal and basic rates mask. */
	rates = basicrates = 0;
	maxrate = maxbasicrate = 0;
	for (i = 0; i < rs->rs_nrates; i++) {
		/* Convert 802.11 rate to HW rate index. */
		ridx = rate2ridx(IEEE80211_RV(rs->rs_rates[i]));
		if (ridx == RTWN_RIDX_UNKNOWN)	/* Unknown rate, skip. */
			continue;
		rates |= 1 << ridx;
		if (ridx > maxrate)
			maxrate = ridx;
		if (rs->rs_rates[i] & IEEE80211_RATE_BASIC) {
			basicrates |= 1 << ridx;
			if (ridx > maxbasicrate)
				maxbasicrate = ridx;
		}
	}
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	DPRINTF(("mode=0x%x rates=0x%08x, basicrates=0x%08x\n",
	    mode, rates, basicrates));

	/* Set rates mask for group addressed frames. */
	cmd.macid = RTWN_MACID_BC | RTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | basicrates);
	error = rtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not add broadcast station\n");
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF(("maxbasicrate=%d\n", maxbasicrate));
	rtwn_write_1(sc, R92C_INIDATA_RATE_SEL(RTWN_MACID_BC),
	    maxbasicrate);

	/* Set rates mask for unicast frames. */
	cmd.macid = RTWN_MACID_BSS | RTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | rates);
	error = rtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		device_printf(sc->sc_dev, "could not add BSS station\n");
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF(("maxrate=%d\n", maxrate));
	rtwn_write_1(sc, R92C_INIDATA_RATE_SEL(RTWN_MACID_BSS),
	    maxrate);

	/* Configure Automatic Rate Fallback Register. */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		if (rates & 0x0c)
			rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0d));
		else
			rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0f));
	} else
		rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0ff5));

	/* Indicate highest supported rate. */
	ni->ni_txrate = rs->rs_rates[rs->rs_nrates - 1];
	return (0);
}

static void
rtwn_tsf_sync_enable(struct rtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = vap->iv_bss;
	uint64_t tsf;

	/* Enable TSF synchronization. */
	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_DIS_TSF_UDT0);

	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_EN_BCN);

	/* Set initial TSF. */
	memcpy(&tsf, ni->ni_tstamp.data, 8);
	tsf = le64toh(tsf);
	tsf = tsf - (tsf % (vap->iv_bss->ni_intval * IEEE80211_DUR_TU));
	tsf -= IEEE80211_DUR_TU;
	rtwn_write_4(sc, R92C_TSFTR + 0, tsf);
	rtwn_write_4(sc, R92C_TSFTR + 4, tsf >> 32);

	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) | R92C_BCN_CTRL_EN_BCN);
}

static void
rtwn_set_led(struct rtwn_softc *sc, int led, int on)
{
	uint8_t reg;

	if (led == RTWN_LED_LINK) {
		reg = rtwn_read_1(sc, R92C_LEDCFG2) & 0xf0;
		if (!on)
			reg |= R92C_LEDCFG2_DIS;
		else
			reg |= R92C_LEDCFG2_EN;
		rtwn_write_1(sc, R92C_LEDCFG2, reg);
		sc->ledlink = on;	/* Save LED state. */
	}
}

static void
rtwn_calib_to(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct r92c_fw_cmd_rssi cmd;

	if (sc->avg_pwdb != -1) {
		/* Indicate Rx signal strength to FW for rate adaptation. */
		memset(&cmd, 0, sizeof(cmd));
		cmd.macid = 0;	/* BSS. */
		cmd.pwdb = sc->avg_pwdb;
		DPRINTFN(3, ("sending RSSI command avg=%d\n", sc->avg_pwdb));
		rtwn_fw_cmd(sc, R92C_CMD_RSSI_SETTING, &cmd, sizeof(cmd));
	}

	/* Do temperature compensation. */
	rtwn_temp_calib(sc);

	callout_reset(&sc->calib_to, hz * 2, rtwn_calib_to, sc);
}

static int
rtwn_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct rtwn_vap *rvp = RTWN_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	struct rtwn_softc *sc = ic->ic_softc;
	uint32_t reg;

	IEEE80211_UNLOCK(ic);
	RTWN_LOCK(sc);

	if (vap->iv_state == IEEE80211_S_RUN) {
		/* Stop calibration. */
		callout_stop(&sc->calib_to);

		/* Turn link LED off. */
		rtwn_set_led(sc, RTWN_LED_LINK, 0);

		/* Set media status to 'No Link'. */
		reg = rtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_NOLINK);
		rtwn_write_4(sc, R92C_CR, reg);

		/* Stop Rx of data frames. */
		rtwn_write_2(sc, R92C_RXFLTMAP2, 0);

		/* Rest TSF. */
		rtwn_write_1(sc, R92C_DUAL_TSF_RST, 0x03);

		/* Disable TSF synchronization. */
		rtwn_write_1(sc, R92C_BCN_CTRL,
		    rtwn_read_1(sc, R92C_BCN_CTRL) |
		    R92C_BCN_CTRL_DIS_TSF_UDT0);

		/* Reset EDCA parameters. */
		rtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3217);
		rtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4317);
		rtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x00105320);
		rtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a444);
	}
	switch (nstate) {
	case IEEE80211_S_INIT:
		/* Turn link LED off. */
		rtwn_set_led(sc, RTWN_LED_LINK, 0);
		break;
	case IEEE80211_S_SCAN:
		/* Make link LED blink during scan. */
		rtwn_set_led(sc, RTWN_LED_LINK, !sc->ledlink);

		/* Pause AC Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE,
		    rtwn_read_1(sc, R92C_TXPAUSE) | 0x0f);
		break;
	case IEEE80211_S_AUTH:
		rtwn_set_chan(sc, ic->ic_curchan, NULL);
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			/* Enable Rx of data frames. */
			rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

			/* Turn link LED on. */
			rtwn_set_led(sc, RTWN_LED_LINK, 1);
			break;
		}

		/* Set media status to 'Associated'. */
		reg = rtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
		rtwn_write_4(sc, R92C_CR, reg);

		/* Set BSSID. */
		rtwn_write_4(sc, R92C_BSSID + 0, le32dec(&ni->ni_bssid[0]));
		rtwn_write_4(sc, R92C_BSSID + 4, le16dec(&ni->ni_bssid[4]));

		if (ic->ic_curmode == IEEE80211_MODE_11B)
			rtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 0);
		else	/* 802.11b/g */
			rtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 3);

		/* Enable Rx of data frames. */
		rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

		/* Flush all AC queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0);

		/* Set beacon interval. */
		rtwn_write_2(sc, R92C_BCN_INTERVAL, ni->ni_intval);

		/* Allow Rx from our BSSID only. */
		rtwn_write_4(sc, R92C_RCR,
		    rtwn_read_4(sc, R92C_RCR) |
		    R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);

		/* Enable TSF synchronization. */
		rtwn_tsf_sync_enable(sc);

		rtwn_write_1(sc, R92C_SIFS_CCK + 1, 10);
		rtwn_write_1(sc, R92C_SIFS_OFDM + 1, 10);
		rtwn_write_1(sc, R92C_SPEC_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_MAC_SPEC_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_R2T_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_T2T_SIFS + 1, 10);

		/* Intialize rate adaptation. */
		rtwn_ra_init(sc);
		/* Turn link LED on. */
		rtwn_set_led(sc, RTWN_LED_LINK, 1);

		sc->avg_pwdb = -1;	/* Reset average RSSI. */
		/* Reset temperature calibration state machine. */
		sc->thcal_state = 0;
		sc->thcal_lctemp = 0;
		/* Start periodic calibration. */
		callout_reset(&sc->calib_to, hz * 2, rtwn_calib_to, sc);
		break;
	default:
		break;
	}
	RTWN_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return (rvp->newstate(vap, nstate, arg));
}

static int
rtwn_updateedca(struct ieee80211com *ic)
{
	struct rtwn_softc *sc = ic->ic_softc;
	const uint16_t aci2reg[WME_NUM_AC] = {
		R92C_EDCA_BE_PARAM,
		R92C_EDCA_BK_PARAM,
		R92C_EDCA_VI_PARAM,
		R92C_EDCA_VO_PARAM
	};
	int aci, aifs, slottime;

	IEEE80211_LOCK(ic);
	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	for (aci = 0; aci < WME_NUM_AC; aci++) {
		const struct wmeParams *ac =
		    &ic->ic_wme.wme_chanParams.cap_wmeParams[aci];
		/* AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime. */
		aifs = ac->wmep_aifsn * slottime + 10;
		rtwn_write_4(sc, aci2reg[aci],
		    SM(R92C_EDCA_PARAM_TXOP, ac->wmep_txopLimit) |
		    SM(R92C_EDCA_PARAM_ECWMIN, ac->wmep_logcwmin) |
		    SM(R92C_EDCA_PARAM_ECWMAX, ac->wmep_logcwmax) |
		    SM(R92C_EDCA_PARAM_AIFS, aifs));
	}
	IEEE80211_UNLOCK(ic);
	return (0);
}

static void
rtwn_update_avgrssi(struct rtwn_softc *sc, int rate, int8_t rssi)
{
	int pwdb;

	/* Convert antenna signal to percentage. */
	if (rssi <= -100 || rssi >= 20)
		pwdb = 0;
	else if (rssi >= 0)
		pwdb = 100;
	else
		pwdb = 100 + rssi;
	if (RTWN_RATE_IS_CCK(rate)) {
		/* CCK gain is smaller than OFDM/MCS gain. */
		pwdb += 6;
		if (pwdb > 100)
			pwdb = 100;
		if (pwdb <= 14)
			pwdb -= 4;
		else if (pwdb <= 26)
			pwdb -= 8;
		else if (pwdb <= 34)
			pwdb -= 6;
		else if (pwdb <= 42)
			pwdb -= 2;
	}
	if (sc->avg_pwdb == -1)	/* Init. */
		sc->avg_pwdb = pwdb;
	else if (sc->avg_pwdb < pwdb)
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20) + 1;
	else
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20);
	DPRINTFN(4, ("PWDB=%d EMA=%d\n", pwdb, sc->avg_pwdb));
}

static int8_t
rtwn_get_rssi(struct rtwn_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 16, -12, -26, -46 };
	struct r92c_rx_phystat *phy;
	struct r92c_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	if (RTWN_RATE_IS_CCK(rate)) {
		cck = (struct r92c_rx_cck *)physt;
		if (sc->sc_flags & RTWN_FLAG_CCK_HIPWR) {
			rpt = (cck->agc_rpt >> 5) & 0x3;
			rssi = (cck->agc_rpt & 0x1f) << 1;
		} else {
			rpt = (cck->agc_rpt >> 6) & 0x3;
			rssi = cck->agc_rpt & 0x3e;
		}
		rssi = cckoff[rpt] - rssi;
	} else {	/* OFDM/HT. */
		phy = (struct r92c_rx_phystat *)physt;
		rssi = ((le32toh(phy->phydw1) >> 1) & 0x7f) - 110;
	}
	return (rssi);
}

static void
rtwn_rx_frame(struct rtwn_softc *sc, struct r92c_rx_desc *rx_desc,
    struct rtwn_rx_data *rx_data, int desc_idx)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame_min *wh;
	struct ieee80211_node *ni;
	struct r92c_rx_phystat *phy = NULL;
	uint32_t rxdw0, rxdw3;
	struct mbuf *m, *m1;
	bus_dma_segment_t segs[1];
	bus_addr_t physaddr;
	uint8_t rate;
	int8_t rssi = 0, nf;
	int infosz, nsegs, pktlen, shift, error;

	rxdw0 = le32toh(rx_desc->rxdw0);
	rxdw3 = le32toh(rx_desc->rxdw3);

	if (__predict_false(rxdw0 & (R92C_RXDW0_CRCERR | R92C_RXDW0_ICVERR))) {
		/*
		 * This should not happen since we setup our Rx filter
		 * to not receive these frames.
		 */
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	pktlen = MS(rxdw0, R92C_RXDW0_PKTLEN);
	if (__predict_false(pktlen < sizeof(struct ieee80211_frame_ack) ||
	    pktlen > MCLBYTES)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	rate = MS(rxdw3, R92C_RXDW3_RATE);
	infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;
	if (infosz > sizeof(struct r92c_rx_phystat))
		infosz = sizeof(struct r92c_rx_phystat);
	shift = MS(rxdw0, R92C_RXDW0_SHIFT);

	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0 && (rxdw0 & R92C_RXDW0_PHYST)) {
		phy = mtod(rx_data->m, struct r92c_rx_phystat *);
		rssi = rtwn_get_rssi(sc, rate, phy);
		/* Update our average RSSI. */
		rtwn_update_avgrssi(sc, rate, rssi);
	}

	DPRINTFN(5, ("Rx frame len=%d rate=%d infosz=%d shift=%d rssi=%d\n",
	    pktlen, rate, infosz, shift, rssi));

	m1 = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m1 == NULL) {
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}
	bus_dmamap_unload(sc->rx_ring.data_dmat, rx_data->map);

	error = bus_dmamap_load(sc->rx_ring.data_dmat, rx_data->map,
	     mtod(m1, void *), MCLBYTES, rtwn_dma_map_addr,
	     &physaddr, 0);
	if (error != 0) {
		m_freem(m1);

		if (bus_dmamap_load_mbuf_sg(sc->rx_ring.data_dmat,
		    rx_data->map, rx_data->m, segs, &nsegs, 0)) 
			panic("%s: could not load old RX mbuf",
			    device_get_name(sc->sc_dev));

		/* Physical address may have changed. */
		rtwn_setup_rx_desc(sc, rx_desc, physaddr, MCLBYTES, desc_idx);
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	/* Finalize mbuf. */
	m = rx_data->m;
	rx_data->m = m1;
	m->m_pkthdr.len = m->m_len = pktlen + infosz + shift;

	/* Update RX descriptor. */
	rtwn_setup_rx_desc(sc, rx_desc, physaddr, MCLBYTES, desc_idx);

	/* Get ieee80211 frame header. */
	if (rxdw0 & R92C_RXDW0_PHYST)
		m_adj(m, infosz + shift);
	else
		m_adj(m, shift);

	nf = -95;
	if (ieee80211_radiotap_active(ic)) {
		struct rtwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		if (!(rxdw3 & R92C_RXDW3_HT)) {
			tap->wr_rate = ridx2rate[rate];
		} else if (rate >= 12) {	/* MCS0~15. */
			/* Bit 7 set means HT MCS instead of rate. */
			tap->wr_rate = 0x80 | (rate - 12);
		}
		tap->wr_dbm_antsignal = rssi;
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
	}

	RTWN_UNLOCK(sc);
	wh = mtod(m, struct ieee80211_frame_min *);
	if (m->m_len >= sizeof(*wh))
		ni = ieee80211_find_rxnode(ic, wh);
	else
		ni = NULL;

	/* Send the frame to the 802.11 layer. */
	if (ni != NULL) {
		(void)ieee80211_input(ni, m, rssi - nf, nf);
		/* Node is no longer needed. */
		ieee80211_free_node(ni);
	} else
		(void)ieee80211_input_all(ic, m, rssi - nf, nf);

	RTWN_LOCK(sc);
}

static int
rtwn_tx(struct rtwn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct rtwn_tx_ring *tx_ring;
	struct rtwn_tx_data *data;
	struct r92c_tx_desc *txd;
	bus_dma_segment_t segs[1];
	uint16_t qos;
	uint8_t raid, type, tid, qid;
	int nsegs, error;

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/* Encrypt the frame if need be. */
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		/* 802.11 header may have moved. */
		wh = mtod(m, struct ieee80211_frame *);
	}

	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		qos = ((const struct ieee80211_qosframe *)wh)->i_qos[0];
		tid = qos & IEEE80211_QOS_TID;
	} else {
		qos = 0;
		tid = 0;
	}

	switch (type) {
	case IEEE80211_FC0_TYPE_CTL:
	case IEEE80211_FC0_TYPE_MGT:
		qid = RTWN_VO_QUEUE;
		break;
	default:
		qid = M_WME_GETAC(m);
		break;
	}

	/* Grab a Tx buffer from the ring. */
	tx_ring = &sc->tx_ring[qid];
	data = &tx_ring->tx_data[tx_ring->cur];
	if (data->m != NULL) {
		m_freem(m);
		return (ENOBUFS);
	}

	/* Fill Tx descriptor. */
	txd = &tx_ring->desc[tx_ring->cur];
	if (htole32(txd->txdw0) & R92C_RXDW0_OWN) {
		m_freem(m);
		return (ENOBUFS);
	}
	txd->txdw0 = htole32(
	    SM(R92C_TXDW0_PKTLEN, m->m_pkthdr.len) |
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

	txd->txdw1 = 0;
	txd->txdw4 = 0;
	txd->txdw5 = 0;

	/* XXX TODO: rate control; implement low-rate for EAPOL */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    type == IEEE80211_FC0_TYPE_DATA) {
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			raid = R92C_RAID_11B;
		else
			raid = R92C_RAID_11BG;
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, RTWN_MACID_BSS) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_BE) |
		    SM(R92C_TXDW1_RAID, raid) |
		    R92C_TXDW1_AGGBK);

		if (ic->ic_flags & IEEE80211_F_USEPROT) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
				txd->txdw4 |= htole32(R92C_TXDW4_CTS2SELF |
				    R92C_TXDW4_HWRTSEN);
			} else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS) {
				txd->txdw4 |= htole32(R92C_TXDW4_RTSEN |
				    R92C_TXDW4_HWRTSEN);
			}
		}

		/* XXX TODO: implement rate control */

		/* Send RTS at OFDM24. */
		txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE,
		    RTWN_RIDX_OFDM24));
		txd->txdw5 |= htole32(SM(R92C_TXDW5_RTSRATE_FBLIMIT, 0xf));
		/* Send data at OFDM54. */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE,
		    RTWN_RIDX_OFDM54));
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE_FBLIMIT, 0x1f));

	} else {
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, 0) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_MGNT) |
		    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

		/* Force CCK1. */
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, RTWN_RIDX_CCK1));
	}
	/* Set sequence number (already little endian). */
	txd->txdseq = htole16(M_SEQNO_GET(m) % IEEE80211_SEQ_RANGE);
	
	if (!qos) {
		/* Use HW sequence numbering for non-QoS frames. */
		txd->txdw4  |= htole32(R92C_TXDW4_HWSEQ);
		txd->txdseq |= htole16(0x8000);
	} else
		txd->txdw4 |= htole32(R92C_TXDW4_QOS);

	error = bus_dmamap_load_mbuf_sg(tx_ring->data_dmat, data->map, m, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev, "can't map mbuf (error %d)\n", error);
		m_freem(m);
		return (error);
	}
	if (error != 0) {
		struct mbuf *mnew;

		mnew = m_defrag(m, M_NOWAIT);
		if (mnew == NULL) {
			device_printf(sc->sc_dev,
			    "can't defragment mbuf\n");
			m_freem(m);
			return (ENOBUFS);
		}
		m = mnew;

		error = bus_dmamap_load_mbuf_sg(tx_ring->data_dmat, data->map,
		    m, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "can't map mbuf (error %d)\n", error);
			m_freem(m);
			return (error);
		}
	}

	txd->txbufaddr = htole32(segs[0].ds_addr);
	txd->txbufsize = htole16(m->m_pkthdr.len);
	bus_space_barrier(sc->sc_st, sc->sc_sh, 0, sc->sc_mapsize,
	    BUS_SPACE_BARRIER_WRITE);
	txd->txdw0 |= htole32(R92C_TXDW0_OWN);

	bus_dmamap_sync(tx_ring->desc_dmat, tx_ring->desc_map,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(tx_ring->data_dmat, data->map, BUS_DMASYNC_POSTWRITE);

	data->m = m;
	data->ni = ni;

	if (ieee80211_radiotap_active_vap(vap)) {
		struct rtwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		ieee80211_radiotap_tx(vap, m);
	}

	tx_ring->cur = (tx_ring->cur + 1) % RTWN_TX_LIST_COUNT;
	tx_ring->queued++;

	if (tx_ring->queued >= (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk |= (1 << qid);

	/* Kick TX. */
	rtwn_write_2(sc, R92C_PCIE_CTRL_REG, (1 << qid));
	return (0);
}

static void
rtwn_tx_done(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	struct r92c_tx_desc *tx_desc;
	int i;

	bus_dmamap_sync(tx_ring->desc_dmat, tx_ring->desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		tx_data = &tx_ring->tx_data[i];
		if (tx_data->m == NULL)
			continue;

		tx_desc = &tx_ring->desc[i];
		if (le32toh(tx_desc->txdw0) & R92C_TXDW0_OWN)
			continue;

		bus_dmamap_unload(tx_ring->desc_dmat, tx_ring->desc_map);

		/*
		 * XXX TODO: figure out whether the transmit succeeded or not.
		 * .. and then notify rate control.
		 */
		ieee80211_tx_complete(tx_data->ni, tx_data->m, 0);
		tx_data->ni = NULL;
		tx_data->m = NULL;

		sc->sc_tx_timer = 0;
		tx_ring->queued--;
	}

	if (tx_ring->queued < (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk &= ~(1 << qid);
	rtwn_start(sc);
}

static int
rtwn_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct rtwn_softc *sc = ic->ic_softc;

	RTWN_LOCK(sc);

	/* Prevent management frames from being sent if we're not ready. */
	if (!(sc->sc_flags & RTWN_RUNNING)) {
		RTWN_UNLOCK(sc);
		m_freem(m);
		return (ENETDOWN);
	}

	if (rtwn_tx(sc, m, ni) != 0) {
		RTWN_UNLOCK(sc);
		return (EIO);
	}
	sc->sc_tx_timer = 5;
	RTWN_UNLOCK(sc);
	return (0);
}

static int
rtwn_transmit(struct ieee80211com *ic, struct mbuf *m)   
{
	struct rtwn_softc *sc = ic->ic_softc;
	int error;

	RTWN_LOCK(sc);
	if ((sc->sc_flags & RTWN_RUNNING) == 0) {
		RTWN_UNLOCK(sc);
		return (ENXIO);
	}
	error = mbufq_enqueue(&sc->sc_snd, m);
	if (error) {
		RTWN_UNLOCK(sc);
		return (error);
	}
	rtwn_start(sc);
	RTWN_UNLOCK(sc);
	return (0);
}

static void
rtwn_parent(struct ieee80211com *ic)
{
	struct rtwn_softc *sc = ic->ic_softc;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (ic->ic_nrunning > 0) {
		if (rtwn_init(sc) == 0)
			ieee80211_start_all(ic);
		else
			ieee80211_stop(vap);
	} else
		rtwn_stop(sc);
}

static void
rtwn_start(struct rtwn_softc *sc)
{
	struct ieee80211_node *ni;
	struct mbuf *m;

	RTWN_LOCK_ASSERT(sc);

	if ((sc->sc_flags & RTWN_RUNNING) == 0)
		return;

	while (sc->qfullmsk == 0 && (m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (rtwn_tx(sc, m, ni) != 0) {
			if_inc_counter(ni->ni_vap->iv_ifp,
			    IFCOUNTER_OERRORS, 1);
			ieee80211_free_node(ni);
			continue;
		}
		sc->sc_tx_timer = 5;
	}
}

static void
rtwn_watchdog(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	RTWN_LOCK_ASSERT(sc);

	KASSERT(sc->sc_flags & RTWN_RUNNING, ("not running"));

	if (sc->sc_tx_timer != 0 && --sc->sc_tx_timer == 0) {
		ic_printf(ic, "device timeout\n");
		ieee80211_restart_all(ic);
		return;
	}
	callout_reset(&sc->watchdog_to, hz, rtwn_watchdog, sc);
}

static int
rtwn_power_on(struct rtwn_softc *sc)
{
	uint32_t reg;
	int ntries;

	/* Wait for autoload done bit. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_1(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for chip autoload\n");
		return (ETIMEDOUT);
	}

	/* Unlock ISO/CLK/Power control register. */
	rtwn_write_1(sc, R92C_RSV_CTRL, 0);

	/* TODO: check if we need this for 8188CE */
	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_APS_FSMCO);
		reg |= (R92C_APS_FSMCO_SOP_ABG |
			R92C_APS_FSMCO_SOP_AMB |
			R92C_APS_FSMCO_XOP_BTCK);
		rtwn_write_4(sc, R92C_APS_FSMCO, reg);
	}

	/* Move SPS into PWM mode. */
	rtwn_write_1(sc, R92C_SPS0_CTRL, 0x2b);

	/* Set low byte to 0x0f, leave others unchanged. */
	rtwn_write_4(sc, R92C_AFE_XTAL_CTRL,
	    (rtwn_read_4(sc, R92C_AFE_XTAL_CTRL) & 0xffffff00) | 0x0f);

	/* TODO: check if we need this for 8188CE */
	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_AFE_XTAL_CTRL);
		reg &= (~0x00024800); /* XXX magic from linux */
		rtwn_write_4(sc, R92C_AFE_XTAL_CTRL, reg);
	}

	rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	  (rtwn_read_2(sc, R92C_SYS_ISO_CTRL) & 0xff) |
	  R92C_SYS_ISO_CTRL_PWC_EV12V | R92C_SYS_ISO_CTRL_DIOR);
	DELAY(200);

	/* TODO: linux does additional btcoex stuff here */

	/* Auto enable WLAN. */
	rtwn_write_2(sc, R92C_APS_FSMCO,
	    rtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(rtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC))
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev, "timeout waiting for MAC auto ON\n");
		return (ETIMEDOUT);
	}

	/* Enable radio, GPIO and LED functions. */
	rtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_PCIE |
	    R92C_APS_FSMCO_PDN_EN |
	    R92C_APS_FSMCO_PFM_ALDN);
	/* Release RF digital isolation. */
	rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	    rtwn_read_2(sc, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);

	if (sc->chip & RTWN_CHIP_92C)
		rtwn_write_1(sc, R92C_PCIE_CTRL_REG + 3, 0x77);
	else
		rtwn_write_1(sc, R92C_PCIE_CTRL_REG + 3, 0x22);

	rtwn_write_4(sc, R92C_INT_MIG, 0);

	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_AFE_XTAL_CTRL + 2);
		reg &= 0xfd; /* XXX magic from linux */
		rtwn_write_4(sc, R92C_AFE_XTAL_CTRL + 2, reg);
	}

	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    rtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_RFKILL);

	reg = rtwn_read_1(sc, R92C_GPIO_IO_SEL);
	if (!(reg & R92C_GPIO_IO_SEL_RFKILL)) {
		device_printf(sc->sc_dev,
		    "radio is disabled by hardware switch\n");
		return (EPERM);
	}

	/* Initialize MAC. */
	reg = rtwn_read_1(sc, R92C_APSD_CTRL);
	rtwn_write_1(sc, R92C_APSD_CTRL,
	    rtwn_read_1(sc, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
	for (ntries = 0; ntries < 200; ntries++) {
		if (!(rtwn_read_1(sc, R92C_APSD_CTRL) &
		    R92C_APSD_CTRL_OFF_STATUS))
			break;
		DELAY(500);
	}
	if (ntries == 200) {
		device_printf(sc->sc_dev,
		    "timeout waiting for MAC initialization\n");
		return (ETIMEDOUT);
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	reg = rtwn_read_2(sc, R92C_CR);
	reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC;
	rtwn_write_2(sc, R92C_CR, reg);

	rtwn_write_1(sc, 0xfe10, 0x19);

	return (0);
}

static int
rtwn_llt_init(struct rtwn_softc *sc)
{
	int i, error;

	/* Reserve pages [0; R92C_TX_PAGE_COUNT]. */
	for (i = 0; i < R92C_TX_PAGE_COUNT; i++) {
		if ((error = rtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* NB: 0xff indicates end-of-list. */
	if ((error = rtwn_llt_write(sc, i, 0xff)) != 0)
		return (error);
	/*
	 * Use pages [R92C_TX_PAGE_COUNT + 1; R92C_TXPKTBUF_COUNT - 1]
	 * as ring buffer.
	 */
	for (++i; i < R92C_TXPKTBUF_COUNT - 1; i++) {
		if ((error = rtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* Make the last page point to the beginning of the ring buffer. */
	error = rtwn_llt_write(sc, i, R92C_TX_PAGE_COUNT + 1);
	return (error);
}

static void
rtwn_fw_reset(struct rtwn_softc *sc)
{
	uint16_t reg;
	int ntries;

	/* Tell 8051 to reset itself. */
	rtwn_write_1(sc, R92C_HMETFR + 3, 0x20);

	/* Wait until 8051 resets by itself. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rtwn_read_2(sc, R92C_SYS_FUNC_EN);
		if (!(reg & R92C_SYS_FUNC_EN_CPUEN))
			goto sleep;
		DELAY(50);
	}
	/* Force 8051 reset. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
sleep:
	/* 
	 * We must sleep for one second to let the firmware settle.
	 * Accessing registers too early will hang the whole system.
	 */
	if (msleep(&reg, &sc->sc_mtx, 0, "rtwnrst", hz)) {
		device_printf(sc->sc_dev, "timeout waiting for firmware "
		    "initialization to complete\n");
	}
}

static void
rtwn_fw_loadpage(struct rtwn_softc *sc, int page, const uint8_t *buf, int len)
{
	uint32_t reg;
	int off, mlen, i;

	reg = rtwn_read_4(sc, R92C_MCUFWDL);
	reg = RW(reg, R92C_MCUFWDL_PAGE, page);
	rtwn_write_4(sc, R92C_MCUFWDL, reg);

	DELAY(5);

	off = R92C_FW_START_ADDR;
	while (len > 0) {
		if (len > 196)
			mlen = 196;
		else if (len > 4)
			mlen = 4;
		else
			mlen = 1;
		for (i = 0; i < mlen; i++)
			rtwn_write_1(sc, off++, buf[i]);
		buf += mlen;
		len -= mlen;
	}
}

static int
rtwn_load_firmware(struct rtwn_softc *sc)
{
	const struct firmware *fw;
	const struct r92c_fw_hdr *hdr;
	const char *name;
	const u_char *ptr;
	size_t len;
	uint32_t reg;
	int mlen, ntries, page, error = 0;

	/* Read firmware image from the filesystem. */
	if ((sc->chip & (RTWN_CHIP_UMC_A_CUT | RTWN_CHIP_92C)) ==
	    RTWN_CHIP_UMC_A_CUT)
		name = "rtwn-rtl8192cfwU";
	else
		name = "rtwn-rtl8192cfwU_B";
	RTWN_UNLOCK(sc);
	fw = firmware_get(name);
	RTWN_LOCK(sc);
	if (fw == NULL) {
		device_printf(sc->sc_dev,
		    "could not read firmware %s\n", name);
		return (ENOENT);
	}
	len = fw->datasize;
	if (len < sizeof(*hdr)) {
		device_printf(sc->sc_dev, "firmware too short\n");
		error = EINVAL;
		goto fail;
	}
	ptr = fw->data;
	hdr = (const struct r92c_fw_hdr *)ptr;
	/* Check if there is a valid FW header and skip it. */
	if ((le16toh(hdr->signature) >> 4) == 0x88c ||
	    (le16toh(hdr->signature) >> 4) == 0x92c) {
		DPRINTF(("FW V%d.%d %02d-%02d %02d:%02d\n",
		    le16toh(hdr->version), le16toh(hdr->subversion),
		    hdr->month, hdr->date, hdr->hour, hdr->minute));
		ptr += sizeof(*hdr);
		len -= sizeof(*hdr);
	}

	if (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL)
		rtwn_fw_reset(sc);

	/* Enable FW download. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    rtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_CPUEN);
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
	rtwn_write_1(sc, R92C_MCUFWDL + 2,
	    rtwn_read_1(sc, R92C_MCUFWDL + 2) & ~0x08);

	/* Reset the FWDL checksum. */
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_CHKSUM_RPT);

	for (page = 0; len > 0; page++) {
		mlen = MIN(len, R92C_FW_PAGE_SIZE);
		rtwn_fw_loadpage(sc, page, ptr, mlen);
		ptr += mlen;
		len -= mlen;
	}

	/* Disable FW download. */
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) & ~R92C_MCUFWDL_EN);
	rtwn_write_1(sc, R92C_MCUFWDL + 1, 0);

	/* Wait for checksum report. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_CHKSUM_RPT)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for checksum report\n");
		error = ETIMEDOUT;
		goto fail;
	}

	reg = rtwn_read_4(sc, R92C_MCUFWDL);
	reg = (reg & ~R92C_MCUFWDL_WINTINI_RDY) | R92C_MCUFWDL_RDY;
	rtwn_write_4(sc, R92C_MCUFWDL, reg);
	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 2000; ntries++) {
		if (rtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY)
			break;
		DELAY(50);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for firmware readiness\n");
		error = ETIMEDOUT;
		goto fail;
	}
fail:
	firmware_put(fw, FIRMWARE_UNLOAD);
	return (error);
}

static int
rtwn_dma_init(struct rtwn_softc *sc)
{
	uint32_t reg;
	int error;

	/* Initialize LLT table. */
	error = rtwn_llt_init(sc);
	if (error != 0)
		return error;

	/* Set number of pages for normal priority queue. */
	rtwn_write_2(sc, R92C_RQPN_NPQ, 0);
	rtwn_write_4(sc, R92C_RQPN,
	    /* Set number of pages for public queue. */
	    SM(R92C_RQPN_PUBQ, R92C_PUBQ_NPAGES) |
	    /* Set number of pages for high priority queue. */
	    SM(R92C_RQPN_HPQ, R92C_HPQ_NPAGES) |
	    /* Set number of pages for low priority queue. */
	    SM(R92C_RQPN_LPQ, R92C_LPQ_NPAGES) |
	    /* Load values. */
	    R92C_RQPN_LD);

	rtwn_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TRXFF_BNDY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TDECTRL + 1, R92C_TX_PAGE_BOUNDARY);

	reg = rtwn_read_2(sc, R92C_TRXDMA_CTRL);
	reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
	reg |= 0xF771; 
	rtwn_write_2(sc, R92C_TRXDMA_CTRL, reg);

	rtwn_write_4(sc, R92C_TCR, R92C_TCR_CFENDFORM | (1 << 12) | (1 << 13));

	/* Configure Tx DMA. */
	rtwn_write_4(sc, R92C_BKQ_DESA, sc->tx_ring[RTWN_BK_QUEUE].paddr);
	rtwn_write_4(sc, R92C_BEQ_DESA, sc->tx_ring[RTWN_BE_QUEUE].paddr);
	rtwn_write_4(sc, R92C_VIQ_DESA, sc->tx_ring[RTWN_VI_QUEUE].paddr);
	rtwn_write_4(sc, R92C_VOQ_DESA, sc->tx_ring[RTWN_VO_QUEUE].paddr);
	rtwn_write_4(sc, R92C_BCNQ_DESA, sc->tx_ring[RTWN_BEACON_QUEUE].paddr);
	rtwn_write_4(sc, R92C_MGQ_DESA, sc->tx_ring[RTWN_MGNT_QUEUE].paddr);
	rtwn_write_4(sc, R92C_HQ_DESA, sc->tx_ring[RTWN_HIGH_QUEUE].paddr);

	/* Configure Rx DMA. */
	rtwn_write_4(sc, R92C_RX_DESA, sc->rx_ring.paddr);

	/* Set Tx/Rx transfer page boundary. */
	rtwn_write_2(sc, R92C_TRXFF_BNDY + 2, 0x27ff);

	/* Set Tx/Rx transfer page size. */
	rtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));
	return (0);
}

static void
rtwn_mac_init(struct rtwn_softc *sc)
{
	int i;

	/* Write MAC initialization values. */
	for (i = 0; i < nitems(rtl8192ce_mac); i++)
		rtwn_write_1(sc, rtl8192ce_mac[i].reg, rtl8192ce_mac[i].val);
}

static void
rtwn_bb_init(struct rtwn_softc *sc)
{
	const struct rtwn_bb_prog *prog;
	uint32_t reg;
	int i;

	/* Enable BB and RF. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    rtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	rtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0xdb83);

	rtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);

	rtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_DIO_PCIE | R92C_SYS_FUNC_EN_PCIEA |
	    R92C_SYS_FUNC_EN_PPLL | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_BBRSTB);

	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);

	rtwn_write_4(sc, R92C_LEDCFG0,
	    rtwn_read_4(sc, R92C_LEDCFG0) | 0x00800000);

	/* Select BB programming. */ 
	prog = (sc->chip & RTWN_CHIP_92C) ?
	    &rtl8192ce_bb_prog_2t : &rtl8192ce_bb_prog_1t;

	/* Write BB initialization values. */
	for (i = 0; i < prog->count; i++) {
		rtwn_bb_write(sc, prog->regs[i], prog->vals[i]);
		DELAY(1);
	}

	if (sc->chip & RTWN_CHIP_92C_1T2R) {
		/* 8192C 1T only configuration. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_TXINFO);
		reg = (reg & ~0x00000003) | 0x2;
		rtwn_bb_write(sc, R92C_FPGA0_TXINFO, reg);

		reg = rtwn_bb_read(sc, R92C_FPGA1_TXINFO);
		reg = (reg & ~0x00300033) | 0x00200022;
		rtwn_bb_write(sc, R92C_FPGA1_TXINFO, reg);

		reg = rtwn_bb_read(sc, R92C_CCK0_AFESETTING);
		reg = (reg & ~0xff000000) | 0x45 << 24;
		rtwn_bb_write(sc, R92C_CCK0_AFESETTING, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		reg = (reg & ~0x000000ff) | 0x23;
		rtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCPARAM1);
		reg = (reg & ~0x00000030) | 1 << 4;
		rtwn_bb_write(sc, R92C_OFDM0_AGCPARAM1, reg);

		reg = rtwn_bb_read(sc, 0xe74);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe74, reg);
		reg = rtwn_bb_read(sc, 0xe78);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe78, reg);
		reg = rtwn_bb_read(sc, 0xe7c);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe7c, reg);
		reg = rtwn_bb_read(sc, 0xe80);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe80, reg);
		reg = rtwn_bb_read(sc, 0xe88);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe88, reg);
	}

	/* Write AGC values. */
	for (i = 0; i < prog->agccount; i++) {
		rtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE,
		    prog->agcvals[i]);
		DELAY(1);
	}

	if (rtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
	    R92C_HSSI_PARAM2_CCK_HIPWR)
		sc->sc_flags |= RTWN_FLAG_CCK_HIPWR;
}

static void
rtwn_rf_init(struct rtwn_softc *sc)
{
	const struct rtwn_rf_prog *prog;
	uint32_t reg, type;
	int i, j, idx, off;

	/* Select RF programming based on board type. */
	if (!(sc->chip & RTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = rtl8188ce_rf_prog;
		else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			prog = rtl8188ru_rf_prog;
		else
			prog = rtl8188cu_rf_prog;
	} else
		prog = rtl8192ce_rf_prog;

	for (i = 0; i < sc->nrxchains; i++) {
		/* Save RF_ENV control type. */
		idx = i / 2;
		off = (i % 2) * 16;
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		type = (reg >> off) & 0x10;

		/* Set RF_ENV enable. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x100000;
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set RF_ENV output high. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set address and data lengths of RF registers. */
		reg = rtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		rtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);
		reg = rtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		rtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				DELAY(50);
				continue;
			}
			rtwn_rf_write(sc, i, prog[i].regs[j],
			    prog[i].vals[j]);
			DELAY(1);
		}

		/* Restore RF_ENV control type. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		reg &= ~(0x10 << off) | (type << off);
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(idx), reg);

		/* Cache RF register CHNLBW. */
		sc->rf_chnlbw[i] = rtwn_rf_read(sc, i, R92C_RF_CHNLBW);
	}

	if ((sc->chip & (RTWN_CHIP_UMC_A_CUT | RTWN_CHIP_92C)) ==
	    RTWN_CHIP_UMC_A_CUT) {
		rtwn_rf_write(sc, 0, R92C_RF_RX_G1, 0x30255);
		rtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x50a00);
	}
}

static void
rtwn_cam_init(struct rtwn_softc *sc)
{
	/* Invalidate all CAM entries. */
	rtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_CLR);
}

static void
rtwn_pa_bias_init(struct rtwn_softc *sc)
{
	uint8_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		if (sc->pa_setting & (1 << i))
			continue;
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x0f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x4f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x8f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0xcf406);
	}
	if (!(sc->pa_setting & 0x10)) {
		reg = rtwn_read_1(sc, 0x16);
		reg = (reg & ~0xf0) | 0x90;
		rtwn_write_1(sc, 0x16, reg);
	}
}

static void
rtwn_rxfilter_init(struct rtwn_softc *sc)
{
	/* Initialize Rx filter. */
	/* TODO: use better filter for monitor mode. */
	rtwn_write_4(sc, R92C_RCR,
	    R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB |
	    R92C_RCR_APP_ICV | R92C_RCR_AMF | R92C_RCR_HTC_LOC_CTRL |
	    R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
	/* Accept all multicast frames. */
	rtwn_write_4(sc, R92C_MAR + 0, 0xffffffff);
	rtwn_write_4(sc, R92C_MAR + 4, 0xffffffff);
	/* Accept all management frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP0, 0xffff);
	/* Reject all control frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);
	/* Accept all data frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);
}

static void
rtwn_edca_init(struct rtwn_softc *sc)
{

	rtwn_write_2(sc, R92C_SPEC_SIFS, 0x1010);
	rtwn_write_2(sc, R92C_MAC_SPEC_SIFS, 0x1010);
	rtwn_write_2(sc, R92C_SIFS_CCK, 0x1010);
	rtwn_write_2(sc, R92C_SIFS_OFDM, 0x0e0e);
	rtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x005ea42b);
	rtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a44f);
	rtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4322);
	rtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3222);
}

static void
rtwn_write_txpower(struct rtwn_softc *sc, int chain,
    uint16_t power[RTWN_RIDX_COUNT])
{
	uint32_t reg;

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = rtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[0]);
		rtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[2]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[3]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[0]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[2]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[3]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[ 4]) |
	    SM(R92C_TXAGC_RATE09, power[ 5]) |
	    SM(R92C_TXAGC_RATE12, power[ 6]) |
	    SM(R92C_TXAGC_RATE18, power[ 7]));
	rtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[ 8]) |
	    SM(R92C_TXAGC_RATE36, power[ 9]) |
	    SM(R92C_TXAGC_RATE48, power[10]) |
	    SM(R92C_TXAGC_RATE54, power[11]));
	/* Write per-MCS Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[12]) |
	    SM(R92C_TXAGC_MCS01,  power[13]) |
	    SM(R92C_TXAGC_MCS02,  power[14]) |
	    SM(R92C_TXAGC_MCS03,  power[15]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[16]) |
	    SM(R92C_TXAGC_MCS05,  power[17]) |
	    SM(R92C_TXAGC_MCS06,  power[18]) |
	    SM(R92C_TXAGC_MCS07,  power[19]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
	    SM(R92C_TXAGC_MCS08,  power[20]) |
	    SM(R92C_TXAGC_MCS09,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

static void
rtwn_get_txpower(struct rtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[RTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;
	uint16_t cckpow, ofdmpow, htpow, diff, max;
	const struct rtwn_txpwr *base;
	int ridx, chan, group;

	/* Determine channel group. */
	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan <= 3)
		group = 0;
	else if (chan <= 9)
		group = 1;
	else
		group = 2;

	/* Get original Tx power based on board type and RF chain. */
	if (!(sc->chip & RTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			base = &rtl8188ru_txagc[chain];
		else
			base = &rtl8192cu_txagc[chain];
	} else
		base = &rtl8192cu_txagc[chain];

	memset(power, 0, RTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = RTWN_RIDX_CCK1; ridx <= RTWN_RIDX_CCK11; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = RTWN_RIDX_OFDM6; ridx < RTWN_RIDX_COUNT; ridx++) {
		if (sc->regulatory == 3) {
			power[ridx] = base->pwr[0][ridx];
			/* Apply vendor limits. */
			if (extc != NULL)
				max = rom->ht40_max_pwr[group];
			else
				max = rom->ht20_max_pwr[group];
			max = (max >> (chain * 4)) & 0xf;
			if (power[ridx] > max)
				power[ridx] = max;
		} else if (sc->regulatory == 1) {
			if (extc == NULL)
				power[ridx] = base->pwr[group][ridx];
		} else if (sc->regulatory != 2)
			power[ridx] = base->pwr[0][ridx];
	}

	/* Compute per-CCK rate Tx power. */
	cckpow = rom->cck_tx_pwr[chain][group];
	for (ridx = RTWN_RIDX_CCK1; ridx <= RTWN_RIDX_CCK11; ridx++) {
		power[ridx] += cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	htpow = rom->ht40_1s_tx_pwr[chain][group];
	if (sc->ntxchains > 1) {
		/* Apply reduction for 2 spatial streams. */
		diff = rom->ht40_2s_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow = (htpow > diff) ? htpow - diff : 0;
	}

	/* Compute per-OFDM rate Tx power. */
	diff = rom->ofdm_tx_pwr_diff[group];
	diff = (diff >> (chain * 4)) & 0xf;
	ofdmpow = htpow + diff;	/* HT->OFDM correction. */
	for (ridx = RTWN_RIDX_OFDM6; ridx <= RTWN_RIDX_OFDM54; ridx++) {
		power[ridx] += ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	/* Compute per-MCS Tx power. */
	if (extc == NULL) {
		diff = rom->ht20_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow += diff;	/* HT40->HT20 correction. */
	}
	for (ridx = RTWN_RIDX_MCS0; ridx <= RTWN_RIDX_MCS15; ridx++) {
		power[ridx] += htpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
#ifdef RTWN_DEBUG
	if (sc->sc_debug >= 4) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = RTWN_RIDX_CCK1; ridx < RTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}

static void
rtwn_set_txpower(struct rtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint16_t power[RTWN_RIDX_COUNT];
	int i;

	for (i = 0; i < sc->ntxchains; i++) {
		/* Compute per-rate Tx power values. */
		rtwn_get_txpower(sc, i, c, extc, power);
		/* Write per-rate Tx power values to hardware. */
		rtwn_write_txpower(sc, i, power);
	}
}

static void
rtwn_set_rx_bssid_all(struct rtwn_softc *sc, int enable)
{
	uint32_t reg;

	reg = rtwn_read_4(sc, R92C_RCR);
	if (enable)
		reg &= ~R92C_RCR_CBSSID_BCN;
	else
		reg |= R92C_RCR_CBSSID_BCN;
	rtwn_write_4(sc, R92C_RCR, reg);
}

static void
rtwn_set_gain(struct rtwn_softc *sc, uint8_t gain)
{
	uint32_t reg;

	reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
	reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, gain);
	rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

	reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
	reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, gain);
	rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);
}

static void
rtwn_scan_start(struct ieee80211com *ic)
{
	struct rtwn_softc *sc = ic->ic_softc;

	RTWN_LOCK(sc);
	/* Receive beacons / probe responses from any BSSID. */
	rtwn_set_rx_bssid_all(sc, 1);
	/* Set gain for scanning. */
	rtwn_set_gain(sc, 0x20);
	RTWN_UNLOCK(sc);
}

static void
rtwn_scan_end(struct ieee80211com *ic)
{
	struct rtwn_softc *sc = ic->ic_softc;

	RTWN_LOCK(sc);
	/* Restore limitations. */
	rtwn_set_rx_bssid_all(sc, 0);
	/* Set gain under link. */
	rtwn_set_gain(sc, 0x32);
	RTWN_UNLOCK(sc);
}

static void
rtwn_getradiocaps(struct ieee80211com *ic,
    int maxchans, int *nchans, struct ieee80211_channel chans[])
{
	uint8_t bands[IEEE80211_MODE_BYTES];

	memset(bands, 0, sizeof(bands));
	setbit(bands, IEEE80211_MODE_11B);
	setbit(bands, IEEE80211_MODE_11G);
	ieee80211_add_channel_list_2ghz(chans, maxchans, nchans,
	    rtwn_chan_2ghz, nitems(rtwn_chan_2ghz), bands, 0);
}

static void
rtwn_set_channel(struct ieee80211com *ic)
{
	struct rtwn_softc *sc = ic->ic_softc;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	RTWN_LOCK(sc);
	if (vap->iv_state == IEEE80211_S_SCAN) {
		/* Make link LED blink during scan. */
		rtwn_set_led(sc, RTWN_LED_LINK, !sc->ledlink);
	}
	rtwn_set_chan(sc, ic->ic_curchan, NULL);
	RTWN_UNLOCK(sc);
}

static void
rtwn_update_mcast(struct ieee80211com *ic)
{

	/* XXX do nothing?  */
}

static void
rtwn_set_chan(struct rtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan;
	int i;

	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan == 0 || chan == IEEE80211_CHAN_ANY) {
		device_printf(sc->sc_dev,
		    "%s: invalid channel %x\n", __func__, chan);
		return;
	}

	/* Set Tx power for this new channel. */
	rtwn_set_txpower(sc, c, extc);

	for (i = 0; i < sc->nrxchains; i++) {
		rtwn_rf_write(sc, i, R92C_RF_CHNLBW,
		    RW(sc->rf_chnlbw[i], R92C_RF_CHNLBW_CHNL, chan));
	}
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		uint32_t reg;

		/* Is secondary channel below or above primary? */
		int prichlo = c->ic_freq < extc->ic_freq;

		rtwn_write_1(sc, R92C_BWOPMODE,
		    rtwn_read_1(sc, R92C_BWOPMODE) & ~R92C_BWOPMODE_20MHZ);

		reg = rtwn_read_1(sc, R92C_RRSR + 2);
		reg = (reg & ~0x6f) | (prichlo ? 1 : 2) << 5;
		rtwn_write_1(sc, R92C_RRSR + 2, reg);

		rtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA0_RFMOD) | R92C_RFMOD_40MHZ);
		rtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA1_RFMOD) | R92C_RFMOD_40MHZ);

		/* Set CCK side band. */
		reg = rtwn_bb_read(sc, R92C_CCK0_SYSTEM);
		reg = (reg & ~0x00000010) | (prichlo ? 0 : 1) << 4;
		rtwn_bb_write(sc, R92C_CCK0_SYSTEM, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM1_LSTF);
		reg = (reg & ~0x00000c00) | (prichlo ? 1 : 2) << 10;
		rtwn_bb_write(sc, R92C_OFDM1_LSTF, reg);

		rtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    rtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) &
		    ~R92C_FPGA0_ANAPARAM2_CBW20);

		reg = rtwn_bb_read(sc, 0x818);
		reg = (reg & ~0x0c000000) | (prichlo ? 2 : 1) << 26;
		rtwn_bb_write(sc, 0x818, reg);

		/* Select 40MHz bandwidth. */
		rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | chan);
	} else
#endif
	{
		rtwn_write_1(sc, R92C_BWOPMODE,
		    rtwn_read_1(sc, R92C_BWOPMODE) | R92C_BWOPMODE_20MHZ);

		rtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA0_RFMOD) & ~R92C_RFMOD_40MHZ);
		rtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA1_RFMOD) & ~R92C_RFMOD_40MHZ);

		rtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    rtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) |
		    R92C_FPGA0_ANAPARAM2_CBW20);

		/* Select 20MHz bandwidth. */
		rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | R92C_RF_CHNLBW_BW20 | chan);
	}
}

static int
rtwn_iq_calib_chain(struct rtwn_softc *sc, int chain, uint16_t tx[2],
    uint16_t rx[2])
{
	uint32_t status;
	int offset = chain * 0x20;

	if (chain == 0) {	/* IQ calibration for chain 0. */
		/* IQ calibration settings for chain 0. */
		rtwn_bb_write(sc, 0xe30, 0x10008c1f);
		rtwn_bb_write(sc, 0xe34, 0x10008c1f);
		rtwn_bb_write(sc, 0xe38, 0x82140102);

		if (sc->ntxchains > 1) {
			rtwn_bb_write(sc, 0xe3c, 0x28160202);	/* 2T */
			/* IQ calibration settings for chain 1. */
			rtwn_bb_write(sc, 0xe50, 0x10008c22);
			rtwn_bb_write(sc, 0xe54, 0x10008c22);
			rtwn_bb_write(sc, 0xe58, 0x82140102);
			rtwn_bb_write(sc, 0xe5c, 0x28160202);
		} else
			rtwn_bb_write(sc, 0xe3c, 0x28160502);	/* 1T */

		/* LO calibration settings. */
		rtwn_bb_write(sc, 0xe4c, 0x001028d1);
		/* We're doing LO and IQ calibration in one shot. */
		rtwn_bb_write(sc, 0xe48, 0xf9000000);
		rtwn_bb_write(sc, 0xe48, 0xf8000000);

	} else {		/* IQ calibration for chain 1. */
		/* We're doing LO and IQ calibration in one shot. */
		rtwn_bb_write(sc, 0xe60, 0x00000002);
		rtwn_bb_write(sc, 0xe60, 0x00000000);
	}

	/* Give LO and IQ calibrations the time to complete. */
	DELAY(1000);

	/* Read IQ calibration status. */
	status = rtwn_bb_read(sc, 0xeac);

	if (status & (1 << (28 + chain * 3)))
		return (0);	/* Tx failed. */
	/* Read Tx IQ calibration results. */
	tx[0] = (rtwn_bb_read(sc, 0xe94 + offset) >> 16) & 0x3ff;
	tx[1] = (rtwn_bb_read(sc, 0xe9c + offset) >> 16) & 0x3ff;
	if (tx[0] == 0x142 || tx[1] == 0x042)
		return (0);	/* Tx failed. */

	if (status & (1 << (27 + chain * 3)))
		return (1);	/* Rx failed. */
	/* Read Rx IQ calibration results. */
	rx[0] = (rtwn_bb_read(sc, 0xea4 + offset) >> 16) & 0x3ff;
	rx[1] = (rtwn_bb_read(sc, 0xeac + offset) >> 16) & 0x3ff;
	if (rx[0] == 0x132 || rx[1] == 0x036)
		return (1);	/* Rx failed. */

	return (3);	/* Both Tx and Rx succeeded. */
}

static void
rtwn_iq_calib_run(struct rtwn_softc *sc, int n, uint16_t tx[2][2],
    uint16_t rx[2][2])
{
	/* Registers to save and restore during IQ calibration. */
	struct iq_cal_regs {
		uint32_t	adda[16];
		uint8_t		txpause;
		uint8_t		bcn_ctrl;
		uint8_t		ustime_tsf;
		uint32_t	gpio_muxcfg;
		uint32_t	ofdm0_trxpathena;
		uint32_t	ofdm0_trmuxpar;
		uint32_t	fpga0_rfifacesw1;
	} iq_cal_regs;
	static const uint16_t reg_adda[16] = {
		0x85c, 0xe6c, 0xe70, 0xe74,
		0xe78, 0xe7c, 0xe80, 0xe84,
		0xe88, 0xe8c, 0xed0, 0xed4,
		0xed8, 0xedc, 0xee0, 0xeec
	};
	int i, chain;
	uint32_t hssi_param1;

	if (n == 0) {
		for (i = 0; i < nitems(reg_adda); i++)
			iq_cal_regs.adda[i] = rtwn_bb_read(sc, reg_adda[i]);

		iq_cal_regs.txpause = rtwn_read_1(sc, R92C_TXPAUSE);
		iq_cal_regs.bcn_ctrl = rtwn_read_1(sc, R92C_BCN_CTRL);
		iq_cal_regs.ustime_tsf = rtwn_read_1(sc, R92C_USTIME_TSF);
		iq_cal_regs.gpio_muxcfg = rtwn_read_4(sc, R92C_GPIO_MUXCFG);
	}

	if (sc->ntxchains == 1) {
		rtwn_bb_write(sc, reg_adda[0], 0x0b1b25a0);
		for (i = 1; i < nitems(reg_adda); i++)
			rtwn_bb_write(sc, reg_adda[i], 0x0bdb25a0);
	} else {
		for (i = 0; i < nitems(reg_adda); i++)
			rtwn_bb_write(sc, reg_adda[i], 0x04db25a4);
	}

	hssi_param1 = rtwn_bb_read(sc, R92C_HSSI_PARAM1(0));
	if (!(hssi_param1 & R92C_HSSI_PARAM1_PI)) {
		rtwn_bb_write(sc, R92C_HSSI_PARAM1(0),
		    hssi_param1 | R92C_HSSI_PARAM1_PI);
		rtwn_bb_write(sc, R92C_HSSI_PARAM1(1),
		    hssi_param1 | R92C_HSSI_PARAM1_PI);
	}

	if (n == 0) {
		iq_cal_regs.ofdm0_trxpathena =
		    rtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		iq_cal_regs.ofdm0_trmuxpar =
		    rtwn_bb_read(sc, R92C_OFDM0_TRMUXPAR);
		iq_cal_regs.fpga0_rfifacesw1 =
		    rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(1));
	}

	rtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, 0x03a05600);
	rtwn_bb_write(sc, R92C_OFDM0_TRMUXPAR, 0x000800e4);
	rtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(1), 0x22204000);
	if (sc->ntxchains > 1) {
		rtwn_bb_write(sc, R92C_LSSI_PARAM(0), 0x00010000);
		rtwn_bb_write(sc, R92C_LSSI_PARAM(1), 0x00010000);
	}

	rtwn_write_1(sc, R92C_TXPAUSE, 0x3f);
	rtwn_write_1(sc, R92C_BCN_CTRL, iq_cal_regs.bcn_ctrl & ~(0x08));
	rtwn_write_1(sc, R92C_USTIME_TSF, iq_cal_regs.ustime_tsf & ~(0x08));
	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    iq_cal_regs.gpio_muxcfg & ~(0x20));

	rtwn_bb_write(sc, 0x0b68, 0x00080000);
	if (sc->ntxchains > 1)
		rtwn_bb_write(sc, 0x0b6c, 0x00080000);

	rtwn_bb_write(sc, 0x0e28, 0x80800000);
	rtwn_bb_write(sc, 0x0e40, 0x01007c00);
	rtwn_bb_write(sc, 0x0e44, 0x01004800);

	rtwn_bb_write(sc, 0x0b68, 0x00080000);

	for (chain = 0; chain < sc->ntxchains; chain++) {
		if (chain > 0) {
			/* Put chain 0 on standby. */
			rtwn_bb_write(sc, 0x0e28, 0x00);
			rtwn_bb_write(sc, R92C_LSSI_PARAM(0), 0x00010000);
			rtwn_bb_write(sc, 0x0e28, 0x80800000);

			/* Enable chain 1. */
			for (i = 0; i < nitems(reg_adda); i++)
				rtwn_bb_write(sc, reg_adda[i], 0x0b1b25a4);
		}

		/* Run IQ calibration twice. */
		for (i = 0; i < 2; i++) {
			int ret;

			ret = rtwn_iq_calib_chain(sc, chain,
			    tx[chain], rx[chain]);
			if (ret == 0) {
				DPRINTF(("%s: chain %d: Tx failed.\n",
				    __func__, chain));
				tx[chain][0] = 0xff;
				tx[chain][1] = 0xff;
				rx[chain][0] = 0xff;
				rx[chain][1] = 0xff;
			} else if (ret == 1) {
				DPRINTF(("%s: chain %d: Rx failed.\n",
				    __func__, chain));
				rx[chain][0] = 0xff;
				rx[chain][1] = 0xff;
			} else if (ret == 3) {
				DPRINTF(("%s: chain %d: Both Tx and Rx "
				    "succeeded.\n", __func__, chain));
			}
		}

		DPRINTF(("%s: results for run %d chain %d: tx[0]=0x%x, "
		    "tx[1]=0x%x rx[0]=0x%x rx[1]=0x%x\n", __func__, n, chain,
		    tx[chain][0], tx[chain][1], rx[chain][0], rx[chain][1]));
	}

	rtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA,
	    iq_cal_regs.ofdm0_trxpathena); 
	rtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(1),
	    iq_cal_regs.fpga0_rfifacesw1);
	rtwn_bb_write(sc, R92C_OFDM0_TRMUXPAR, iq_cal_regs.ofdm0_trmuxpar);

	rtwn_bb_write(sc, 0x0e28, 0x00);
	rtwn_bb_write(sc, R92C_LSSI_PARAM(0), 0x00032ed3);
	if (sc->ntxchains > 1)
		rtwn_bb_write(sc, R92C_LSSI_PARAM(1), 0x00032ed3);

	if (n != 0) {
		if (!(hssi_param1 & R92C_HSSI_PARAM1_PI)) {
			rtwn_bb_write(sc, R92C_HSSI_PARAM1(0), hssi_param1);
			rtwn_bb_write(sc, R92C_HSSI_PARAM1(1), hssi_param1);
		}

		for (i = 0; i < nitems(reg_adda); i++)
			rtwn_bb_write(sc, reg_adda[i], iq_cal_regs.adda[i]);

		rtwn_write_1(sc, R92C_TXPAUSE, iq_cal_regs.txpause);
		rtwn_write_1(sc, R92C_BCN_CTRL, iq_cal_regs.bcn_ctrl);
		rtwn_write_1(sc, R92C_USTIME_TSF, iq_cal_regs.ustime_tsf);
		rtwn_write_4(sc, R92C_GPIO_MUXCFG, iq_cal_regs.gpio_muxcfg);
	}
}

#define RTWN_IQ_CAL_MAX_TOLERANCE 5
static int
rtwn_iq_calib_compare_results(uint16_t tx1[2][2], uint16_t rx1[2][2],
    uint16_t tx2[2][2], uint16_t rx2[2][2], int ntxchains)
{
	int chain, i, tx_ok[2], rx_ok[2];

	tx_ok[0] = tx_ok[1] = rx_ok[0] = rx_ok[1] = 0;
	for (chain = 0; chain < ntxchains; chain++) {
		for (i = 0; i < 2; i++)	{
			if (tx1[chain][i] == 0xff || tx2[chain][i] == 0xff ||
			    rx1[chain][i] == 0xff || rx2[chain][i] == 0xff)
				continue;

			tx_ok[chain] = (abs(tx1[chain][i] - tx2[chain][i]) <=
			    RTWN_IQ_CAL_MAX_TOLERANCE);

			rx_ok[chain] = (abs(rx1[chain][i] - rx2[chain][i]) <=
			    RTWN_IQ_CAL_MAX_TOLERANCE);
		}
	}

	if (ntxchains > 1)
		return (tx_ok[0] && tx_ok[1] && rx_ok[0] && rx_ok[1]);
	else
		return (tx_ok[0] && rx_ok[0]);
}
#undef RTWN_IQ_CAL_MAX_TOLERANCE

static void
rtwn_iq_calib_write_results(struct rtwn_softc *sc, uint16_t tx[2],
    uint16_t rx[2], int chain)
{
	uint32_t reg, val, x;
	long y, tx_c;

	if (tx[0] == 0xff || tx[1] == 0xff)
		return;

	reg = rtwn_bb_read(sc, R92C_OFDM0_TXIQIMBALANCE(chain)); 
	val = ((reg >> 22) & 0x3ff);
	x = tx[0];
	if (x & 0x0200)
		x |= 0xfc00;
	reg = (((x * val) >> 8) & 0x3ff);
	rtwn_bb_write(sc, R92C_OFDM0_TXIQIMBALANCE(chain), reg);

	reg = rtwn_bb_read(sc, R92C_OFDM0_ECCATHRESHOLD);
	if (((x * val) >> 7) & 0x01)
		reg |= 0x80000000;
	else
		reg &= ~0x80000000;
	rtwn_bb_write(sc, R92C_OFDM0_ECCATHRESHOLD, reg);

	y = tx[1];
	if (y & 0x00000200)
		y |= 0xfffffc00;
	tx_c = (y * val) >> 8;
	reg = rtwn_bb_read(sc, R92C_OFDM0_TXAFE(chain));
	reg |= ((((tx_c & 0x3c0) >> 6) << 24) & 0xf0000000);
	rtwn_bb_write(sc, R92C_OFDM0_TXAFE(chain), reg);

	reg = rtwn_bb_read(sc, R92C_OFDM0_TXIQIMBALANCE(chain)); 
	reg |= (((tx_c & 0x3f) << 16) & 0x003F0000);
	rtwn_bb_write(sc, R92C_OFDM0_TXIQIMBALANCE(chain), reg);

	reg = rtwn_bb_read(sc, R92C_OFDM0_ECCATHRESHOLD);
	if (((y * val) >> 7) & 0x01)
		reg |= 0x20000000;
	else
		reg &= ~0x20000000;
	rtwn_bb_write(sc, R92C_OFDM0_ECCATHRESHOLD, reg);

	if (rx[0] == 0xff || rx[1] == 0xff)
		return;

	reg = rtwn_bb_read(sc, R92C_OFDM0_RXIQIMBALANCE(chain));
	reg |= (rx[0] & 0x3ff);
	rtwn_bb_write(sc, R92C_OFDM0_RXIQIMBALANCE(chain), reg);
	reg |= (((rx[1] & 0x03f) << 8) & 0xFC00);
	rtwn_bb_write(sc, R92C_OFDM0_RXIQIMBALANCE(chain), reg);

	if (chain == 0) {
		reg = rtwn_bb_read(sc, R92C_OFDM0_RXIQEXTANTA);
		reg |= (((rx[1] & 0xf) >> 6) & 0x000f);
		rtwn_bb_write(sc, R92C_OFDM0_RXIQEXTANTA, reg);
	} else {
		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCRSSITABLE);
		reg |= ((((rx[1] & 0xf) >> 6) << 12) & 0xf000);
		rtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE, reg);
	}
}

#define RTWN_IQ_CAL_NRUN	3
static void
rtwn_iq_calib(struct rtwn_softc *sc)
{
	uint16_t tx[RTWN_IQ_CAL_NRUN][2][2], rx[RTWN_IQ_CAL_NRUN][2][2];
	int n, valid;

	valid = 0;
	for (n = 0; n < RTWN_IQ_CAL_NRUN; n++) {
		rtwn_iq_calib_run(sc, n, tx[n], rx[n]);

		if (n == 0)
			continue;

		/* Valid results remain stable after consecutive runs. */
		valid = rtwn_iq_calib_compare_results(tx[n - 1], rx[n - 1],
		    tx[n], rx[n], sc->ntxchains);
		if (valid)
			break;
	}

	if (valid) {
		rtwn_iq_calib_write_results(sc, tx[n][0], rx[n][0], 0);
		if (sc->ntxchains > 1)
			rtwn_iq_calib_write_results(sc, tx[n][1], rx[n][1], 1);
	}
}
#undef RTWN_IQ_CAL_NRUN

static void
rtwn_lc_calib(struct rtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	int i;

	txmode = rtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = rtwn_rf_read(sc, i, R92C_RF_AC);
			rtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}
	/* Start calibration. */
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    rtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	DELAY(100);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++)
			rtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
	} else {
		/* Unblock all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}

static void
rtwn_temp_calib(struct rtwn_softc *sc)
{
	int temp;

	if (sc->thcal_state == 0) {
		/* Start measuring temperature. */
		rtwn_rf_write(sc, 0, R92C_RF_T_METER, 0x60);
		sc->thcal_state = 1;
		return;
	}
	sc->thcal_state = 0;

	/* Read measured temperature. */
	temp = rtwn_rf_read(sc, 0, R92C_RF_T_METER) & 0x1f;
	if (temp == 0)	/* Read failed, skip. */
		return;
	DPRINTFN(2, ("temperature=%d\n", temp));

	/*
	 * Redo IQ and LC calibration if temperature changed significantly
	 * since last calibration.
	 */
	if (sc->thcal_lctemp == 0) {
		/* First calibration is performed in rtwn_init(). */
		sc->thcal_lctemp = temp;
	} else if (abs(temp - sc->thcal_lctemp) > 1) {
		DPRINTF(("IQ/LC calib triggered by temp: %d -> %d\n",
		    sc->thcal_lctemp, temp));
		rtwn_iq_calib(sc);
		rtwn_lc_calib(sc);
		/* Record temperature of last calibration. */
		sc->thcal_lctemp = temp;
	}
}

static int
rtwn_init(struct rtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t reg;
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	int i, error;

	RTWN_LOCK(sc);

	if (sc->sc_flags & RTWN_RUNNING) {
		RTWN_UNLOCK(sc);
		return 0;
	}
	sc->sc_flags |= RTWN_RUNNING;

	/* Init firmware commands ring. */
	sc->fwcur = 0;

	/* Power on adapter. */
	error = rtwn_power_on(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not power on adapter\n");
		goto fail;
	}

	/* Initialize DMA. */
	error = rtwn_dma_init(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not initialize DMA\n");
		goto fail;
	}

	/* Set info size in Rx descriptors (in 64-bit words). */
	rtwn_write_1(sc, R92C_RX_DRVINFO_SZ, 4);

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0x00000000);
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Set MAC address. */
	IEEE80211_ADDR_COPY(macaddr, vap ? vap->iv_myaddr : ic->ic_macaddr);
	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		rtwn_write_1(sc, R92C_MACID + i, macaddr[i]);

	/* Set initial network type. */
	reg = rtwn_read_4(sc, R92C_CR);
	reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
	rtwn_write_4(sc, R92C_CR, reg);

	rtwn_rxfilter_init(sc);

	reg = rtwn_read_4(sc, R92C_RRSR);
	reg = RW(reg, R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_ALL);
	rtwn_write_4(sc, R92C_RRSR, reg);

	/* Set short/long retry limits. */
	rtwn_write_2(sc, R92C_RL,
	    SM(R92C_RL_SRL, 0x07) | SM(R92C_RL_LRL, 0x07));

	/* Initialize EDCA parameters. */
	rtwn_edca_init(sc);

	/* Set data and response automatic rate fallback retry counts. */
	rtwn_write_4(sc, R92C_DARFRC + 0, 0x01000000);
	rtwn_write_4(sc, R92C_DARFRC + 4, 0x07060504);
	rtwn_write_4(sc, R92C_RARFRC + 0, 0x01000000);
	rtwn_write_4(sc, R92C_RARFRC + 4, 0x07060504);

	rtwn_write_2(sc, R92C_FWHW_TXQ_CTRL, 0x1f80);

	/* Set ACK timeout. */
	rtwn_write_1(sc, R92C_ACKTO, 0x40);

	/* Initialize beacon parameters. */
	rtwn_write_2(sc, R92C_TBTT_PROHIBIT, 0x6404);
	rtwn_write_1(sc, R92C_DRVERLYINT, 0x05);
	rtwn_write_1(sc, R92C_BCNDMATIM, 0x02);
	rtwn_write_2(sc, R92C_BCNTCFG, 0x660f);

	/* Setup AMPDU aggregation. */
	rtwn_write_4(sc, R92C_AGGLEN_LMT, 0x99997631);	/* MCS7~0 */
	rtwn_write_1(sc, R92C_AGGR_BREAK_TIME, 0x16);

	rtwn_write_1(sc, R92C_BCN_MAX_ERR, 0xff);
	rtwn_write_1(sc, R92C_BCN_CTRL, R92C_BCN_CTRL_DIS_TSF_UDT0);

	rtwn_write_4(sc, R92C_PIFS, 0x1c);
	rtwn_write_4(sc, R92C_MCUTST_1, 0x0);

	/* Load 8051 microcode. */
	error = rtwn_load_firmware(sc);
	if (error != 0)
		goto fail;

	/* Initialize MAC/BB/RF blocks. */
	rtwn_mac_init(sc);
	rtwn_bb_init(sc);
	rtwn_rf_init(sc);

	/* Turn CCK and OFDM blocks on. */
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_CCK_EN;
	rtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_OFDM_EN;
	rtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);

	/* Clear per-station keys table. */
	rtwn_cam_init(sc);

	/* Enable hardware sequence numbering. */
	rtwn_write_1(sc, R92C_HWSEQ_CTRL, 0xff);

	/* Perform LO and IQ calibrations. */
	rtwn_iq_calib(sc);
	/* Perform LC calibration. */
	rtwn_lc_calib(sc);

	rtwn_pa_bias_init(sc);

	/* Initialize GPIO setting. */
	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    rtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);

	/* Fix for lower temperature. */
	rtwn_write_1(sc, 0x15, 0xe9);

	/* CLear pending interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0xffffffff);

	/* Enable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	callout_reset(&sc->watchdog_to, hz, rtwn_watchdog, sc);

fail:
	if (error != 0)
		rtwn_stop_locked(sc);

	RTWN_UNLOCK(sc);

	return error;
}

static void
rtwn_stop_locked(struct rtwn_softc *sc)
{
	uint16_t reg;
	int i;

	RTWN_LOCK_ASSERT(sc);

	if (!(sc->sc_flags & RTWN_RUNNING))
		return;

	sc->sc_tx_timer = 0;
	callout_stop(&sc->watchdog_to);
	callout_stop(&sc->calib_to);
	sc->sc_flags &= ~RTWN_RUNNING;

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0x00000000);
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Stop hardware. */
	rtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	rtwn_write_1(sc, R92C_RF_CTRL, 0x00);
	reg = rtwn_read_1(sc, R92C_SYS_FUNC_EN);
	reg |= R92C_SYS_FUNC_EN_BB_GLB_RST;
	rtwn_write_1(sc, R92C_SYS_FUNC_EN, reg);
	reg &= ~R92C_SYS_FUNC_EN_BB_GLB_RST;
	rtwn_write_1(sc, R92C_SYS_FUNC_EN, reg);
	reg = rtwn_read_2(sc, R92C_CR);
	reg &= ~(R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC);
	rtwn_write_2(sc, R92C_CR, reg);
	if (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL)
		rtwn_fw_reset(sc);
	/* TODO: linux does additional btcoex stuff here */
	rtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0x80); /* linux magic number */
	rtwn_write_1(sc, R92C_SPS0_CTRL, 0x23); /* ditto */
	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL, 0x0e); /* different with btcoex */
	rtwn_write_1(sc, R92C_RSV_CTRL, 0x0e);
	rtwn_write_1(sc, R92C_APS_FSMCO, R92C_APS_FSMCO_PDN_EN);

	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_reset_tx_list(sc, i);
	rtwn_reset_rx_list(sc);
}

static void
rtwn_stop(struct rtwn_softc *sc)
{
	RTWN_LOCK(sc);
	rtwn_stop_locked(sc);
	RTWN_UNLOCK(sc);
}

static void
rtwn_intr(void *arg)
{
	struct rtwn_softc *sc = arg;
	uint32_t status;
	int i;

	RTWN_LOCK(sc);
	status = rtwn_read_4(sc, R92C_HISR);
	if (status == 0 || status == 0xffffffff) {
		RTWN_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Ack interrupts. */
	rtwn_write_4(sc, R92C_HISR, status);

	/* Vendor driver treats RX errors like ROK... */
	if (status & (R92C_IMR_ROK | R92C_IMR_RXFOVW | R92C_IMR_RDU)) {
		bus_dmamap_sync(sc->rx_ring.desc_dmat, sc->rx_ring.desc_map,
		    BUS_DMASYNC_POSTREAD);

		for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
			struct r92c_rx_desc *rx_desc = &sc->rx_ring.desc[i];
			struct rtwn_rx_data *rx_data = &sc->rx_ring.rx_data[i];

			if (le32toh(rx_desc->rxdw0) & R92C_RXDW0_OWN)
				continue;

			rtwn_rx_frame(sc, rx_desc, rx_data, i);
		}
	}

	if (status & R92C_IMR_BDOK)
		rtwn_tx_done(sc, RTWN_BEACON_QUEUE);
	if (status & R92C_IMR_HIGHDOK)
		rtwn_tx_done(sc, RTWN_HIGH_QUEUE);
	if (status & R92C_IMR_MGNTDOK)
		rtwn_tx_done(sc, RTWN_MGNT_QUEUE);
	if (status & R92C_IMR_BKDOK)
		rtwn_tx_done(sc, RTWN_BK_QUEUE);
	if (status & R92C_IMR_BEDOK)
		rtwn_tx_done(sc, RTWN_BE_QUEUE);
	if (status & R92C_IMR_VIDOK)
		rtwn_tx_done(sc, RTWN_VI_QUEUE);
	if (status & R92C_IMR_VODOK)
		rtwn_tx_done(sc, RTWN_VO_QUEUE);

	/* Enable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	RTWN_UNLOCK(sc);
}
