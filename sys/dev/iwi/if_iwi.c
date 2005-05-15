/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

/*-
 * Intel(R) PRO/Wireless 2200BG/2225BG/2915ABG driver
 * http://www.intel.com/network/connectivity/products/wireless/prowireless_mobile.htm
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/iwi/if_iwireg.h>
#include <dev/iwi/if_iwivar.h>

#ifdef IWI_DEBUG
#define DPRINTF(x)	do { if (iwi_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwi_debug >= (n)) printf x; } while (0)
int iwi_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, iwi, CTLFLAG_RW, &iwi_debug, 0, "iwi debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

MODULE_DEPEND(iwi, pci,  1, 1, 1);
MODULE_DEPEND(iwi, wlan, 1, 1, 1);

struct iwi_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};

static const struct iwi_ident iwi_ident_table[] = {
	{ 0x8086, 0x4220, "Intel(R) PRO/Wireless 2200BG" },
	{ 0x8086, 0x4221, "Intel(R) PRO/Wireless 2225BG" },
	{ 0x8086, 0x4223, "Intel(R) PRO/Wireless 2915ABG" },
	{ 0x8086, 0x4224, "Intel(R) PRO/Wireless 2915ABG" },

	{ 0, 0, NULL }
};

static void	iwi_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int	iwi_alloc_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *,
		    int);
static void	iwi_reset_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
static void	iwi_free_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
static int	iwi_alloc_tx_ring(struct iwi_softc *, struct iwi_tx_ring *,
		    int);
static void	iwi_reset_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
static void	iwi_free_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
static int	iwi_alloc_rx_ring(struct iwi_softc *, struct iwi_rx_ring *,
		    int);
static void	iwi_reset_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
static void	iwi_free_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
static int	iwi_media_change(struct ifnet *);
static void	iwi_media_status(struct ifnet *, struct ifmediareq *);
static int	iwi_newstate(struct ieee80211com *, enum ieee80211_state, int);
static uint16_t	iwi_read_prom_word(struct iwi_softc *, uint8_t);
static void	iwi_fix_channel(struct ieee80211com *, struct mbuf *);
static void	iwi_frame_intr(struct iwi_softc *, struct iwi_rx_data *, int,
		    struct iwi_frame *);
static void	iwi_notification_intr(struct iwi_softc *, struct iwi_notif *);
static void	iwi_rx_intr(struct iwi_softc *);
static void	iwi_tx_intr(struct iwi_softc *);
static void	iwi_intr(void *);
static int	iwi_cmd(struct iwi_softc *, uint8_t, void *, uint8_t, int);
static int	iwi_tx_start(struct ifnet *, struct mbuf *,
		    struct ieee80211_node *);
static void	iwi_start(struct ifnet *);
static void	iwi_watchdog(struct ifnet *);
static int	iwi_ioctl(struct ifnet *, u_long, caddr_t);
static void	iwi_stop_master(struct iwi_softc *);
static int	iwi_reset(struct iwi_softc *);
static int	iwi_load_ucode(struct iwi_softc *, void *, int);
static int	iwi_load_firmware(struct iwi_softc *, void *, int);
static int	iwi_cache_firmware(struct iwi_softc *, void *);
static void	iwi_free_firmware(struct iwi_softc *);
static int	iwi_config(struct iwi_softc *);
static int	iwi_scan(struct iwi_softc *);
static int	iwi_auth_and_assoc(struct iwi_softc *);
static void	iwi_init(void *);
static void	iwi_stop(void *);
#ifdef IWI_DEBUG
static int	iwi_sysctl_stats(SYSCTL_HANDLER_ARGS);
#endif
static int	iwi_sysctl_radio(SYSCTL_HANDLER_ARGS);

static int iwi_probe(device_t);
static int iwi_attach(device_t);
static int iwi_detach(device_t);
static int iwi_shutdown(device_t);
static int iwi_suspend(device_t);
static int iwi_resume(device_t);

static device_method_t iwi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		iwi_probe),
	DEVMETHOD(device_attach,	iwi_attach),
	DEVMETHOD(device_detach,	iwi_detach),
	DEVMETHOD(device_shutdown,	iwi_shutdown),
	DEVMETHOD(device_suspend,	iwi_suspend),
	DEVMETHOD(device_resume,	iwi_resume),

	{ 0, 0 }
};

static driver_t iwi_driver = {
	"iwi",
	iwi_methods,
	sizeof (struct iwi_softc)
};

static devclass_t iwi_devclass;

DRIVER_MODULE(iwi, pci, iwi_driver, iwi_devclass, 0, 0);

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset iwi_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset iwi_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset iwi_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

static __inline uint8_t
MEM_READ_1(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IWI_CSR_INDIRECT_DATA);
}

static __inline uint32_t
MEM_READ_4(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_4(sc, IWI_CSR_INDIRECT_DATA);
}

static int
iwi_probe(device_t dev)
{
	const struct iwi_ident *ident;

	for (ident = iwi_ident_table; ident->name != NULL; ident++) {
		if (pci_get_vendor(dev) == ident->vendor &&
		    pci_get_device(dev) == ident->device) {
			device_set_desc(dev, ident->name);
			return 0;
		}
	}
	return ENXIO;
}

/* Base Address Register */
#define IWI_PCI_BAR0	0x10

static int
iwi_attach(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->sc_arp.ac_if;
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
	int error, i;

	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}

	/* enable bus-mastering */
	pci_enable_busmaster(dev);

	sc->mem_rid = IWI_PCI_BAR0;
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		goto fail;
	}

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		goto fail;
	}

	if (iwi_reset(sc) != 0) {
		device_printf(dev, "could not reset adapter\n");
		goto fail;
	}

	/*
	 * Allocate rings.
	 */
	if (iwi_alloc_cmd_ring(sc, &sc->cmdq, IWI_CMD_RING_COUNT) != 0) {
		device_printf(dev, "could not allocate Cmd ring\n");
		goto fail;
	}

	if (iwi_alloc_tx_ring(sc, &sc->txq, IWI_TX_RING_COUNT) != 0) {
		device_printf(dev, "could not allocate Tx ring\n");
		goto fail;
	}

	if (iwi_alloc_rx_ring(sc, &sc->rxq, IWI_RX_RING_COUNT) != 0) {
		device_printf(dev, "could not allocate Rx ring\n");
		goto fail;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = iwi_init;
	ifp->if_ioctl = iwi_ioctl;
	ifp->if_start = iwi_start;
	ifp->if_watchdog = iwi_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps = IEEE80211_C_WEP | IEEE80211_C_PMGT | IEEE80211_C_TXPMGT |
	    IEEE80211_C_SHPREAMBLE;

	/* read MAC address from EEPROM */
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val >> 8;
	ic->ic_myaddr[1] = val & 0xff;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val >> 8;
	ic->ic_myaddr[3] = val & 0xff;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val >> 8;
	ic->ic_myaddr[5] = val & 0xff;

	if (pci_get_device(dev) >= 0x4223) {
		/* set supported .11a rates (2915ABG only) */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = iwi_rateset_11a;

		/* set supported .11a channels */
		for (i = 36; i <= 64; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 149; i <= 165; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = iwi_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = iwi_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ieee80211_ifattach(ic);
	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwi_newstate;
	ieee80211_media_init(ic, iwi_media_change, iwi_media_status);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64, &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWI_TX_RADIOTAP_PRESENT);

	/*
	 * Add a few sysctl knobs.
	 */
	sc->dwelltime = 100;
	sc->bluetooth = 1;

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "radio",
	    CTLTYPE_INT | CTLFLAG_RD, sc, 0, iwi_sysctl_radio, "I",
	    "radio transmitter switch state (0=off, 1=on)");

#ifdef IWI_DEBUG
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "stats",
	    CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0, iwi_sysctl_stats, "S",
	    "statistics");
#endif

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "dwell",
	    CTLFLAG_RW, &sc->dwelltime, 0,
	    "channel dwell time (ms) for AP/station scanning");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "bluetooth",
	    CTLFLAG_RW, &sc->bluetooth, 0, "bluetooth coexistence");

	/*
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    iwi_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "could not set up interrupt\n");
		goto fail;
	}

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;

fail:	iwi_detach(dev);
	return ENXIO;
}

static int
iwi_detach(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	iwi_stop(sc);

	iwi_free_firmware(sc);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);

	iwi_free_cmd_ring(sc, &sc->cmdq);
	iwi_free_tx_ring(sc, &sc->txq);
	iwi_free_rx_ring(sc, &sc->rxq);

	if (sc->irq != NULL) {
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
	}

	if (sc->mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem);

	mtx_destroy(&sc->sc_mtx);

	return 0;
}

static void
iwi_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
iwi_alloc_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring, int count)
{
	int error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, count * IWI_CMD_DESC_SIZE, 1,
	    count * IWI_CMD_DESC_SIZE, 0, NULL, NULL, &ring->desc_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->desc_dmat, (void **)&ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->desc_dmat, ring->desc_map, ring->desc,
	    count * IWI_CMD_DESC_SIZE, iwi_dma_map_addr, &ring->physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	return 0;

fail:	iwi_free_cmd_ring(sc, ring);
	return error;
}

static void
iwi_reset_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	ring->queued = 0;
	ring->cur = ring->next = 0;
}

static void
iwi_free_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	if (ring->desc != NULL) {
		bus_dmamap_sync(ring->desc_dmat, ring->desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->desc_dmat, ring->desc_map);
		bus_dmamem_free(ring->desc_dmat, ring->desc, ring->desc_map);
	}

	if (ring->desc_dmat != NULL)
		bus_dma_tag_destroy(ring->desc_dmat);	
}

static int
iwi_alloc_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring, int count)
{
	int i, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, count * IWI_TX_DESC_SIZE, 1,
	    count * IWI_TX_DESC_SIZE, 0, NULL, NULL, &ring->desc_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->desc_dmat, (void **)&ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->desc_dmat, ring->desc_map, ring->desc,
	    count * IWI_TX_DESC_SIZE, iwi_dma_map_addr, &ring->physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct iwi_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1, MCLBYTES, 0, NULL,
	    NULL, &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(ring->data_dmat, 0,
		    &ring->data[i].map);
		if (error != 0) {
			device_printf(sc->sc_dev, "could not create DMA map\n");
			goto fail;
		}
	}

	return 0;

fail:	iwi_free_tx_ring(sc, ring);
	return error;
}

static void
iwi_reset_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dmat, data->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

static void
iwi_free_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(ring->desc_dmat, ring->desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->desc_dmat, ring->desc_map);
		bus_dmamem_free(ring->desc_dmat, ring->desc, ring->desc_map);
	}

	if (ring->desc_dmat != NULL)
		bus_dma_tag_destroy(ring->desc_dmat);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(ring->data_dmat, data->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(ring->data_dmat, data->map);
				m_freem(data->m);
			}

			if (data->ni != NULL)
				ieee80211_free_node(data->ni);

			if (data->map != NULL)
				bus_dmamap_destroy(ring->data_dmat, data->map);
		}

		free(ring->data, M_DEVBUF);
	}

	if (ring->data_dmat != NULL)
		bus_dma_tag_destroy(ring->data_dmat);
}

static int
iwi_alloc_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring, int count)
{
	struct iwi_rx_data *data;
	int i, error;

	ring->count = count;
	ring->cur = 0;

	ring->data = malloc(count * sizeof (struct iwi_rx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1, MCLBYTES, 0, NULL,
	    NULL, &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev, "could not create DMA map\n");
			goto fail;
		}

		data->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (data->m == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(ring->data_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, iwi_dma_map_addr,
		    &data->physaddr, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load rx buf DMA map");
			goto fail;
		}

		data->reg = IWI_CSR_RX_BASE + i * 4;
	}

	return 0;

fail:	iwi_free_rx_ring(sc, ring);
	return error;
}

static void
iwi_reset_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	ring->cur = 0;
}

static void
iwi_free_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	struct iwi_rx_data *data;
	int i;

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(ring->data_dmat, data->map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(ring->data_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(ring->data_dmat, data->map);
		}

		free(ring->data, M_DEVBUF);
	}

	if (ring->data_dmat != NULL)
		bus_dma_tag_destroy(ring->data_dmat);
}

static int
iwi_shutdown(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);

	iwi_stop(sc);

	return 0;
}

static int
iwi_suspend(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);

	iwi_stop(sc);

	return 0;
}

static int
iwi_resume(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ic.ic_ifp;

	IWI_LOCK(sc);

	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp->if_softc);
		if (ifp->if_flags & IFF_RUNNING)
			ifp->if_start(ifp);
	}

	IWI_UNLOCK(sc);

	return 0;
}

static int
iwi_media_change(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	int error;

	IWI_LOCK(sc);

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET) {
		IWI_UNLOCK(sc);
		return error;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		iwi_init(sc);

	IWI_UNLOCK(sc);

	return 0;
}

/*
 * The firmware automaticly adapt the transmit speed. We report the current
 * transmit speed here.
 */
static void
iwi_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
#define N(a)	(sizeof (a) / sizeof (a[0]))
	static const struct {
		uint32_t	val;
		int		rate;
	} rates[] = {
		{ IWI_RATE_DS1,      2 },
		{ IWI_RATE_DS2,      4 },
		{ IWI_RATE_DS5,     11 },
		{ IWI_RATE_DS11,    22 },
		{ IWI_RATE_OFDM6,   12 },
		{ IWI_RATE_OFDM9,   18 },
		{ IWI_RATE_OFDM12,  24 },
		{ IWI_RATE_OFDM18,  36 },
		{ IWI_RATE_OFDM24,  48 },
		{ IWI_RATE_OFDM36,  72 },
		{ IWI_RATE_OFDM48,  96 },
		{ IWI_RATE_OFDM54, 108 },
	};
	uint32_t val;
	int rate, i;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/* read current transmission rate from adapter */
	val = CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE);

	/* convert rate to 802.11 rate */
	for (i = 0; i < N(rates) && rates[i].val != val; i++);
	rate = (i < N(rates)) ? rates[i].rate : 0;

	imr->ifm_active |= ieee80211_rate2media(ic, rate, ic->ic_curmode);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;

	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;

	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;

	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
		/* should not get there */
		break;
	}
#undef N
}

static int
iwi_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	switch (nstate) {
	case IEEE80211_S_SCAN:
		iwi_scan(sc);
		break;

	case IEEE80211_S_AUTH:
		iwi_auth_and_assoc(sc);
		break;

	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			ieee80211_new_state(ic, IEEE80211_S_AUTH, -1);
		break;

	case IEEE80211_S_ASSOC:
	case IEEE80211_S_INIT:
		break;
	}

	ic->ic_state = nstate;
	return 0;
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM.
 */
static uint16_t
iwi_read_prom_word(struct iwi_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* write start bit (1) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);

	/* write READ opcode (10) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);

	/* write address A7-A0 */
	for (n = 7; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D));
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D) | IWI_EEPROM_C);
	}

	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
		tmp = MEM_READ_4(sc, IWI_MEM_EEPROM_CTL);
		val |= ((tmp & IWI_EEPROM_Q) >> IWI_EEPROM_SHIFT_Q) << n;
	}

	IWI_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_C);

	return be16toh(val);
}

/*
 * XXX: Hack to set the current channel to the value advertised in beacons or
 * probe responses. Only used during AP detection.
 */
static void
iwi_fix_channel(struct ieee80211com *ic, struct mbuf *m)
{
	struct ieee80211_frame *wh;
	uint8_t subtype;
	uint8_t *frm, *efrm;

	wh = mtod(m, struct ieee80211_frame *);

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_MGT)
		return;

	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
	    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP)
		return;

	frm = (uint8_t *)(wh + 1);
	efrm = mtod(m, uint8_t *) + m->m_len;

	frm += 12;	/* skip tstamp, bintval and capinfo fields */
	while (frm < efrm) {
		if (*frm == IEEE80211_ELEMID_DSPARMS)
#if IEEE80211_CHAN_MAX < 255
		if (frm[2] <= IEEE80211_CHAN_MAX)
#endif
			ic->ic_bss->ni_chan = &ic->ic_channels[frm[2]];

		frm += frm[1] + 2;
	}
}

static void
iwi_frame_intr(struct iwi_softc *sc, struct iwi_rx_data *data, int i,
    struct iwi_frame *frame)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	int error;

	DPRINTFN(5, ("received frame len=%u chan=%u rssi=%u\n",
	    le16toh(frame->len), frame->chan, frame->rssi_dbm));

	bus_dmamap_unload(sc->rxq.data_dmat, data->map);

	/* finalize mbuf */
	m = data->m;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = sizeof (struct iwi_hdr) +
	    sizeof (struct iwi_frame) + le16toh(frame->len);

	m_adj(m, sizeof (struct iwi_hdr) + sizeof (struct iwi_frame));

	if (ic->ic_state == IEEE80211_S_SCAN)
		iwi_fix_channel(ic, m);

	if (sc->sc_drvbpf != NULL) {
		struct iwi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = frame->rate;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[frame->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[frame->chan].ic_flags);
		tap->wr_antsignal = frame->signal;
		tap->wr_antenna = frame->antenna;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* send the frame to the 802.11 layer */
	ieee80211_input(ic, m, ni, IWI_RSSIDBM2RAW(frame->rssi_dbm), 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	data->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (data->m == NULL) {
		device_printf(sc->sc_dev, "could not allocate rx mbuf\n");
		return;
	}

	error = bus_dmamap_load(sc->rxq.data_dmat, data->map,
	    mtod(data->m, void *), MCLBYTES, iwi_dma_map_addr, &data->physaddr,
	    0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load rx buf DMA map\n");
		m_freem(data->m);
		data->m = NULL;
		return;
	}

	CSR_WRITE_4(sc, data->reg, data->physaddr);
}

static void
iwi_notification_intr(struct iwi_softc *sc, struct iwi_notif *notif)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_notif_scan_channel *chan;
	struct iwi_notif_scan_complete *scan;
	struct iwi_notif_authentication *auth;
	struct iwi_notif_association *assoc;

	switch (notif->type) {
	case IWI_NOTIF_TYPE_SCAN_CHANNEL:
		chan = (struct iwi_notif_scan_channel *)(notif + 1);

		DPRINTFN(2, ("Scanning channel (%u)\n", chan->nchan));
		break;

	case IWI_NOTIF_TYPE_SCAN_COMPLETE:
		scan = (struct iwi_notif_scan_complete *)(notif + 1);

		DPRINTFN(2, ("Scan completed (%u, %u)\n", scan->nchan,
		    scan->status));

		ieee80211_end_scan(ic);
		break;

	case IWI_NOTIF_TYPE_AUTHENTICATION:
		auth = (struct iwi_notif_authentication *)(notif + 1);

		DPRINTFN(2, ("Authentication (%u)\n", auth->state));

		switch (auth->state) {
		case IWI_AUTHENTICATED:
			ieee80211_node_authorize(ic, ic->ic_bss);
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
			break;

		case IWI_DEAUTHENTICATED:
			break;

		default:
			device_printf(sc->sc_dev,
			    "unknown authentication state %u\n", auth->state);
		}
		break;

	case IWI_NOTIF_TYPE_ASSOCIATION:
		assoc = (struct iwi_notif_association *)(notif + 1);

		DPRINTFN(2, ("Association (%u, %u)\n", assoc->state,
		    assoc->status));

		switch (assoc->state) {
		case IWI_AUTHENTICATED:
			/* re-association, do nothing */
			break;

		case IWI_ASSOCIATED:
			ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
			break;

		case IWI_DEASSOCIATED:
			ieee80211_begin_scan(ic, 1);
			break;

		default:
			device_printf(sc->sc_dev,
			    "unknown association state %u\n", assoc->state);
		}
		break;

	case IWI_NOTIF_TYPE_CALIBRATION:
	case IWI_NOTIF_TYPE_BEACON:
	case IWI_NOTIF_TYPE_NOISE:
		DPRINTFN(5, ("Notification (%u)\n", notif->type));
		break;

	default:
		device_printf(sc->sc_dev, "unknown notification type %u\n",
		    notif->type);
	}
}

static void
iwi_rx_intr(struct iwi_softc *sc)
{
	struct iwi_rx_data *data;
	struct iwi_hdr *hdr;
	uint32_t hw;

	hw = CSR_READ_4(sc, IWI_CSR_RX_RIDX);

	for (; sc->rxq.cur != hw;) {
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->rxq.data_dmat, data->map,
		    BUS_DMASYNC_POSTREAD);

		hdr = mtod(data->m, struct iwi_hdr *);

		switch (hdr->type) {
		case IWI_HDR_TYPE_FRAME:
			iwi_frame_intr(sc, data, sc->rxq.cur,
			    (struct iwi_frame *)(hdr + 1));
			break;

		case IWI_HDR_TYPE_NOTIF:
			iwi_notification_intr(sc,
			    (struct iwi_notif *)(hdr + 1));
			break;

		default:
			device_printf(sc->sc_dev, "unknown hdr type %u\n",
			    hdr->type);
		}

		DPRINTFN(15, ("rx done idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % IWI_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? IWI_RX_RING_COUNT - 1 : hw - 1;
	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, hw);
}

static void
iwi_tx_intr(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_tx_data *data;
	uint32_t hw;

	hw = CSR_READ_4(sc, IWI_CSR_TX1_RIDX);

	for (; sc->txq.next != hw;) {
		data = &sc->txq.data[sc->txq.next];

		bus_dmamap_sync(sc->txq.data_dmat, data->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txq.data_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		ieee80211_free_node(data->ni);
		data->ni = NULL;

		DPRINTFN(15, ("tx done idx=%u\n", sc->txq.next));

		ifp->if_opackets++;

		sc->txq.queued--;
		sc->txq.next = (sc->txq.next + 1) % IWI_TX_RING_COUNT;
	}

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	iwi_start(ifp);
}

static void
iwi_intr(void *arg)
{
	struct iwi_softc *sc = arg;
	uint32_t r;

	IWI_LOCK(sc);

	if ((r = CSR_READ_4(sc, IWI_CSR_INTR)) == 0 || r == 0xffffffff) {
		IWI_UNLOCK(sc);
		return;
	}

	/* disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	if (r & (IWI_INTR_FATAL_ERROR | IWI_INTR_PARITY_ERROR)) {
		device_printf(sc->sc_dev, "fatal error\n");
		sc->sc_ic.ic_ifp->if_flags &= ~IFF_UP;
		iwi_stop(sc);
	}

	if (r & IWI_INTR_FW_INITED) {
		if (!(r & (IWI_INTR_FATAL_ERROR | IWI_INTR_PARITY_ERROR)))
			wakeup(sc);
	}

	if (r & IWI_INTR_RADIO_OFF) {
		DPRINTF(("radio transmitter turned off\n"));
		sc->sc_ic.ic_ifp->if_flags &= ~IFF_UP;
		iwi_stop(sc);
	}

	if (r & IWI_INTR_RX_DONE)
		iwi_rx_intr(sc);

	if (r & IWI_INTR_CMD_DONE)
		wakeup(sc);

	if (r & IWI_INTR_TX1_DONE)
		iwi_tx_intr(sc);

	/* acknowledge interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR, r);

	/* re-enable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, IWI_INTR_MASK);

	IWI_UNLOCK(sc);
}

static int
iwi_cmd(struct iwi_softc *sc, uint8_t type, void *data, uint8_t len, int async)
{
	struct iwi_cmd_desc *desc;

	desc = &sc->cmdq.desc[sc->cmdq.cur];

	desc->hdr.type = IWI_HDR_TYPE_COMMAND;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->type = type;
	desc->len = len;
	memcpy(desc->data, data, len);

	bus_dmamap_sync(sc->cmdq.desc_dmat, sc->cmdq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(2, ("sending command idx=%u type=%u len=%u\n", sc->cmdq.cur,
	    type, len));

	sc->cmdq.cur = (sc->cmdq.cur + 1) % IWI_CMD_RING_COUNT;
	CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	return async ? 0 : msleep(sc, &sc->sc_mtx, 0, "iwicmd", hz);
}

static int
iwi_tx_start(struct ifnet *ifp, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct iwi_tx_data *data;
	struct iwi_tx_desc *desc;
	struct mbuf *mnew;
	bus_dma_segment_t segs[IWI_MAX_NSEG];
	int nsegs, error, i;

	if (sc->sc_drvbpf != NULL) {
		struct iwi_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data = &sc->txq.data[sc->txq.cur];
	desc = &sc->txq.desc[sc->txq.cur];

	wh = mtod(m0, struct ieee80211_frame *);

	/* trim IEEE802.11 header */
	m_adj(m0, sizeof (struct ieee80211_frame));

	error = bus_dmamap_load_mbuf_sg(sc->txq.data_dmat, data->map, m0, segs,
	    &nsegs, 0);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}
	if (error != 0) {
		mnew = m_defrag(m0, M_DONTWAIT);
		if (mnew == NULL) {
			device_printf(sc->sc_dev,
			    "could not defragment mbuf\n");
			m_freem(m0);
			return ENOBUFS;
		}
		m0 = mnew;

		error = bus_dmamap_load_mbuf_sg(sc->txq.data_dmat, data->map,
		    m0, segs, &nsegs, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not map mbuf (error %d)\n", error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	desc->hdr.type = IWI_HDR_TYPE_DATA;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->cmd = IWI_DATA_CMD_TX;
	desc->len = htole16(m0->m_pkthdr.len);
	memcpy(&desc->wh, wh, sizeof (struct ieee80211_frame));
	desc->flags = 0;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1))
		desc->flags |= IWI_DATA_FLAG_NEED_ACK;

	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
		desc->wep_txkey = ic->ic_crypto.cs_def_txkey;
	} else
		desc->flags |= IWI_DATA_FLAG_NO_WEP;

	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		desc->flags |= IWI_DATA_FLAG_SHPREAMBLE;

	desc->nseg = htole32(nsegs);
	for (i = 0; i < nsegs; i++) {
		desc->seg_addr[i] = htole32(segs[i].ds_addr);
		desc->seg_len[i]  = htole32(segs[i].ds_len);
	}

	bus_dmamap_sync(sc->txq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(5, ("sending data frame idx=%u len=%u nseg=%u\n", sc->txq.cur,
	    desc->len, desc->nseg));

	sc->txq.queued++;
	sc->txq.cur = (sc->txq.cur + 1) % IWI_TX_RING_COUNT;
	CSR_WRITE_4(sc, IWI_CSR_TX1_WIDX, sc->txq.cur);

	return 0;
}

static void
iwi_start(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni;

	IWI_LOCK(sc);

	if (ic->ic_state != IEEE80211_S_RUN) {
		IWI_UNLOCK(sc);
		return;
	}

	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (sc->txq.queued >= IWI_TX_RING_COUNT - 4) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m0);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		if (m0->m_len < sizeof (struct ether_header) &&
		    (m0 = m_pullup(m0, sizeof (struct ether_header))) == NULL)
			continue;

		eh = mtod(m0, struct ether_header *);
		ni = ieee80211_find_txnode(ic, eh->ether_dhost);
		if (ni == NULL) {
			m_freem(m0);
			continue;
		}
		BPF_MTAP(ifp, m0);

		m0 = ieee80211_encap(ic, m0, ni);
		if (m0 == NULL)
			continue;

		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m0);

		if (iwi_tx_start(ifp, m0, ni) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}

	IWI_UNLOCK(sc);
}

static void
iwi_watchdog(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	IWI_LOCK(sc);

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			if_printf(ifp, "device timeout\n");
			ifp->if_oerrors++;
			ifp->if_flags &= ~IFF_UP;
			iwi_stop(sc);
			IWI_UNLOCK(sc);
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ic);

	IWI_UNLOCK(sc);
}

static int
iwi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr;
	int error = 0;

	IWI_LOCK(sc);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				iwi_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwi_stop(sc);
		}
		break;

	case SIOCSLOADFW:
		/* only super-user can do that! */
		if ((error = suser(curthread)) != 0)
			break;

		ifr = (struct ifreq *)data;
		error = iwi_cache_firmware(sc, ifr->ifr_data);
		break;

	case SIOCSKILLFW:
		/* only super-user can do that! */
		if ((error = suser(curthread)) != 0)
			break;

		ifp->if_flags &= ~IFF_UP;
		iwi_stop(sc);
		iwi_free_firmware(sc);
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			iwi_init(sc);
		error = 0;
	}

	IWI_UNLOCK(sc);

	return error;
}

static void
iwi_stop_master(struct iwi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/* disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5)
		device_printf(sc->sc_dev, "timeout waiting for master\n");

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp | IWI_RST_PRINCETON_RESET);

	sc->flags &= ~IWI_FLAG_FW_INITED;
}

static int
iwi_reset(struct iwi_softc *sc)
{
	uint32_t tmp;
	int i, ntries;

	iwi_stop_master(sc);

	tmp = CSR_READ_4(sc, IWI_CSR_CTL);
	CSR_WRITE_4(sc, IWI_CSR_CTL, tmp | IWI_CTL_INIT);

	CSR_WRITE_4(sc, IWI_CSR_READ_INT, IWI_READ_INT_INIT_HOST);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_CTL) & IWI_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for clock stabilization\n");
		return EIO;
	}

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp | IWI_RST_SOFT_RESET);

	DELAY(10);

	tmp = CSR_READ_4(sc, IWI_CSR_CTL);
	CSR_WRITE_4(sc, IWI_CSR_CTL, tmp | IWI_CTL_INIT);

	/* clear NIC memory */
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0);
	for (i = 0; i < 0xc000; i++)
		CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	return 0;
}

static int
iwi_load_ucode(struct iwi_softc *sc, void *uc, int size)
{
	uint32_t tmp;
	uint16_t *w;
	int ntries, i;

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5) {
		device_printf(sc->sc_dev, "timeout waiting for master\n");
		return EIO;
	}

	MEM_WRITE_4(sc, 0x3000e0, 0x80000000);
	DELAY(5000);

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	tmp &= ~IWI_RST_PRINCETON_RESET;
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp);

	DELAY(5000);
	MEM_WRITE_4(sc, 0x3000e0, 0);
	DELAY(1000);
	MEM_WRITE_4(sc, 0x300004, 1);
	DELAY(1000);
	MEM_WRITE_4(sc, 0x300004, 0);
	DELAY(1000);
	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x40);
	DELAY(1000);

	/* write microcode into adapter memory */
	for (w = uc; size > 0; w++, size -= 2)
		MEM_WRITE_2(sc, 0x200010, *w);

	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x80);

	/* wait until we get an answer */
	for (ntries = 0; ntries < 100; ntries++) {
		if (MEM_READ_1(sc, 0x200000) & 1)
			break;
		DELAY(100);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "timeout waiting for ucode to initialize\n");
		return EIO;
	}

	/* read the answer or the firmware will not initialize properly */
	for (i = 0; i < 7; i++)
		MEM_READ_4(sc, 0x200004);

	MEM_WRITE_1(sc, 0x200000, 0x00);

	return 0;
}

/* macro to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)

static int
iwi_load_firmware(struct iwi_softc *sc, void *fw, int size)
{
	bus_dma_tag_t dmat;
	bus_dmamap_t map;
	bus_addr_t physaddr;
	void *virtaddr;
	u_char *p, *end;
	uint32_t sentinel, ctl, src, dst, sum, len, mlen, tmp;
	int ntries, error = 0;

	/* allocate DMA memory for mapping firmware image */
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, size, 1, size, 0, NULL, NULL, &dmat);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not create firmware DMA tag\n");
		goto fail1;
	}

	error = bus_dmamem_alloc(dmat, &virtaddr, BUS_DMA_NOWAIT, &map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate firmware DMA memory\n");
		goto fail2;
	}

	error = bus_dmamap_load(dmat, map, virtaddr, size, iwi_dma_map_addr,
	    &physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load firmware DMA map\n");
		goto fail3;
	}

	/* copy firmware image to DMA memory */
	memcpy(virtaddr, fw, size);

	/* make sure the adapter will get up-to-date values */
	bus_dmamap_sync(dmat, map, BUS_DMASYNC_PREWRITE);

	/* tell the adapter where the command blocks are stored */
	MEM_WRITE_4(sc, 0x3000a0, 0x27000);

	/*
	 * Store command blocks into adapter's internal memory using register
	 * indirections. The adapter will read the firmware image through DMA
	 * using information stored in command blocks.
	 */
	src = physaddr;
	p = virtaddr;
	end = p + size;
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0x27000);

	while (p < end) {
		dst = GETLE32(p); p += 4; src += 4;
		len = GETLE32(p); p += 4; src += 4;
		p += len;

		while (len > 0) {
			mlen = min(len, IWI_CB_MAXDATALEN);

			ctl = IWI_CB_DEFAULT_CTL | mlen;
			sum = ctl ^ src ^ dst;

			/* write a command block */
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, ctl);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, src);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, dst);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, sum);

			src += mlen;
			dst += mlen;
			len -= mlen;
		}
	}

	/* write a fictive final command block (sentinel) */
	sentinel = CSR_READ_4(sc, IWI_CSR_AUTOINC_ADDR);
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	tmp &= ~(IWI_RST_MASTER_DISABLED | IWI_RST_STOP_MASTER);
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp);

	/* tell the adapter to start processing command blocks */
	MEM_WRITE_4(sc, 0x3000a4, 0x540100);

	/* wait until the adapter reach the sentinel */
	for (ntries = 0; ntries < 400; ntries++) {
		if (MEM_READ_4(sc, 0x3000d0) >= sentinel)
			break;
		DELAY(100);
	}
	if (ntries == 400) {
		device_printf(sc->sc_dev,
		    "timeout processing command blocks\n");
		error = EIO;
		goto fail4;
	}

	/* we're done with command blocks processing */
	MEM_WRITE_4(sc, 0x3000a4, 0x540c00);

	/* allow interrupts so we know when the firmware is inited */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, IWI_INTR_MASK);

	/* tell the adapter to initialize the firmware */
	CSR_WRITE_4(sc, IWI_CSR_RST, 0);

	tmp = CSR_READ_4(sc, IWI_CSR_CTL);
	CSR_WRITE_4(sc, IWI_CSR_CTL, tmp | IWI_CTL_ALLOW_STANDBY);

	/* wait at most one second for firmware initialization to complete */
	if ((error = msleep(sc, &sc->sc_mtx, 0, "iwiinit", hz)) != 0) {
		device_printf(sc->sc_dev, "timeout waiting for firmware "
		    "initialization to complete\n");
		goto fail4;
	}

fail4:	bus_dmamap_sync(dmat, map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dmat, map);
fail3:	bus_dmamem_free(dmat, virtaddr, map);
fail2:	bus_dma_tag_destroy(dmat);
fail1:
	return error;
}

/*
 * Store firmware into kernel memory so we can download it when we need to,
 * e.g when the adapter wakes up from suspend mode.
 */
static int
iwi_cache_firmware(struct iwi_softc *sc, void *data)
{
	struct iwi_firmware *kfw = &sc->fw;
	struct iwi_firmware ufw;
	int error;

	iwi_free_firmware(sc);

	IWI_UNLOCK(sc);

	if ((error = copyin(data, &ufw, sizeof ufw)) != 0)
		goto fail1;

	kfw->boot_size  = ufw.boot_size;
	kfw->ucode_size = ufw.ucode_size;
	kfw->main_size  = ufw.main_size;

	kfw->boot = malloc(kfw->boot_size, M_DEVBUF, M_NOWAIT);
	if (kfw->boot == NULL) {
		error = ENOMEM;
		goto fail1;
	}

	kfw->ucode = malloc(kfw->ucode_size, M_DEVBUF, M_NOWAIT);
	if (kfw->ucode == NULL) {
		error = ENOMEM;
		goto fail2;
	}

	kfw->main = malloc(kfw->main_size, M_DEVBUF, M_NOWAIT);
	if (kfw->main == NULL) {
		error = ENOMEM;
		goto fail3;
	}

	if ((error = copyin(ufw.boot, kfw->boot, kfw->boot_size)) != 0)
		goto fail4;

	if ((error = copyin(ufw.ucode, kfw->ucode, kfw->ucode_size)) != 0)
		goto fail4;

	if ((error = copyin(ufw.main, kfw->main, kfw->main_size)) != 0)
		goto fail4;

	DPRINTF(("Firmware cached: boot %u, ucode %u, main %u\n",
	    kfw->boot_size, kfw->ucode_size, kfw->main_size));

	IWI_LOCK(sc);

	sc->flags |= IWI_FLAG_FW_CACHED;

	return 0;

fail4:	free(kfw->boot, M_DEVBUF);
fail3:	free(kfw->ucode, M_DEVBUF);
fail2:	free(kfw->main, M_DEVBUF);
fail1:	IWI_LOCK(sc);

	return error;
}

static void
iwi_free_firmware(struct iwi_softc *sc)
{
	if (!(sc->flags & IWI_FLAG_FW_CACHED))
		return;

	free(sc->fw.boot, M_DEVBUF);
	free(sc->fw.ucode, M_DEVBUF);
	free(sc->fw.main, M_DEVBUF);

	sc->flags &= ~IWI_FLAG_FW_CACHED;
}

static int
iwi_config(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_configuration config;
	struct iwi_rateset rs;
	struct iwi_txpower power;
	struct ieee80211_key *wk;
	struct iwi_wep_key wepkey;
	uint32_t data;
	int error, i;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	DPRINTF(("Setting MAC address to %6D\n", ic->ic_myaddr, ":"));
	error = iwi_cmd(sc, IWI_CMD_SET_MAC_ADDRESS, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN, 0);
	if (error != 0)
		return error;

	memset(&config, 0, sizeof config);
	config.bluetooth_coexistence = sc->bluetooth;
	config.multicast_enabled = 1;
	DPRINTF(("Configuring adapter\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_CONFIG, &config, sizeof config, 0);
	if (error != 0)
		return error;

	data = htole32(IWI_POWER_MODE_CAM);
	DPRINTF(("Setting power mode to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_POWER_MODE, &data, sizeof data, 0);
	if (error != 0)
		return error;

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting RTS threshold to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_RTS_THRESHOLD, &data, sizeof data, 0);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		power.mode = IWI_MODE_11B;
		power.nchan = 11;
		for (i = 0; i < 11; i++) {
			power.chan[i].chan = i + 1;
			power.chan[i].power = IWI_TXPOWER_MAX;
		}
		DPRINTF(("Setting .11b channels tx power\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power,
		    0);
		if (error != 0)
			return error;

		power.mode = IWI_MODE_11G;
		DPRINTF(("Setting .11g channels tx power\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power,
		    0);
		if (error != 0)
			return error;
	}

	rs.mode = IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates;
	memcpy(rs.rates, ic->ic_sup_rates[IEEE80211_MODE_11G].rs_rates,
	    rs.nrates);
	DPRINTF(("Setting .11bg supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 0);
	if (error != 0)
		return error;

	rs.mode = IWI_MODE_11A;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates;
	memcpy(rs.rates, ic->ic_sup_rates[IEEE80211_MODE_11A].rs_rates,
	    rs.nrates);
	DPRINTF(("Setting .11a supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 0);
	if (error != 0)
		return error;

	data = htole32(arc4random());
	DPRINTF(("Setting initialization vector to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_IV, &data, sizeof data, 0);
	if (error != 0)
		return error;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		wk = &ic->ic_crypto.cs_nw_keys[i];

		wepkey.cmd = IWI_WEP_KEY_CMD_SETKEY;
		wepkey.idx = i;
		wepkey.len = wk->wk_keylen;
		memset(wepkey.key, 0, sizeof wepkey.key);
		memcpy(wepkey.key, wk->wk_key, wk->wk_keylen);
		DPRINTF(("Setting wep key index %u len %u\n", wepkey.idx,
		    wepkey.len));
		error = iwi_cmd(sc, IWI_CMD_SET_WEP_KEY, &wepkey,
		    sizeof wepkey, 0);
		if (error != 0)
			return error;
	}

	/* enable adapter */
	DPRINTF(("Enabling adapter\n"));
	return iwi_cmd(sc, IWI_CMD_ENABLE, NULL, 0, 0);
}

static int
iwi_scan(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_scan scan;
	uint8_t *p;
	int i, count;

	memset(&scan, 0, sizeof scan);
	scan.type = IWI_SCAN_TYPE_BROADCAST;
	scan.intval = htole16(sc->dwelltime);

	p = scan.channels;
	count = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (IEEE80211_IS_CHAN_5GHZ(&ic->ic_channels[i]) &&
		    isset(ic->ic_chan_active, i)) {
			*++p = i;
			count++;
		}
	}
	*(p - count) = IWI_CHAN_5GHZ | count;

	count = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (IEEE80211_IS_CHAN_2GHZ(&ic->ic_channels[i]) &&
		    isset(ic->ic_chan_active, i)) {
			*++p = i;
			count++;
		}
	}
	*(p - count) = IWI_CHAN_2GHZ | count;

	DPRINTF(("Start scanning\n"));
	return iwi_cmd(sc, IWI_CMD_SCAN, &scan, sizeof scan, 1);
}

static int
iwi_auth_and_assoc(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwi_configuration config;
	struct iwi_associate assoc;
	struct iwi_rateset rs;
	uint32_t data;
	int error;

	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		memset(&config, 0, sizeof config);
		config.bluetooth_coexistence = sc->bluetooth;
		config.multicast_enabled = 1;
		config.use_protection = 1;
		DPRINTF(("Configuring adapter\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_CONFIG, &config, sizeof config,
		    1);
		if (error != 0)
			return error;
	}

#ifdef IWI_DEBUG
	if (iwi_debug > 0) {
		printf("Setting ESSID to ");
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("\n");
	}
#endif
	error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ni->ni_essid, ni->ni_esslen, 1);
	if (error != 0)
		return error;

	/* the rate set has already been "negociated" */
	rs.mode = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? IWI_MODE_11A :
	    IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_NEGOCIATED;
	rs.nrates = ni->ni_rates.rs_nrates;
	memcpy(rs.rates, ni->ni_rates.rs_rates, rs.nrates);
	DPRINTF(("Setting negociated rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 1);
	if (error != 0)
		return error;

	data = htole32(ni->ni_rssi);
	DPRINTF(("Setting sensitivity to %d\n", (int8_t)ni->ni_rssi));
	error = iwi_cmd(sc, IWI_CMD_SET_SENSITIVITY, &data, sizeof data, 1);
	if (error != 0)
		return error;

	memset(&assoc, 0, sizeof assoc);
	assoc.mode = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? IWI_MODE_11A :
	    IWI_MODE_11G;
	assoc.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (ni->ni_authmode == IEEE80211_AUTH_SHARED)
		assoc.auth = ic->ic_crypto.cs_def_txkey << 4 | IWI_AUTH_SHARED;
	memcpy(assoc.tstamp, ni->ni_tstamp.data, 8);
	assoc.capinfo = htole16(ni->ni_capinfo);
	assoc.lintval = htole16(ic->ic_lintval);
	assoc.intval = htole16(ni->ni_intval);
	IEEE80211_ADDR_COPY(assoc.bssid, ni->ni_bssid);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		IEEE80211_ADDR_COPY(assoc.dst, ifp->if_broadcastaddr);
	else
		IEEE80211_ADDR_COPY(assoc.dst, ni->ni_bssid);

	DPRINTF(("Trying to associate to %6D channel %u auth %u\n",
	    assoc.bssid, ":", assoc.chan, assoc.auth));
	return iwi_cmd(sc, IWI_CMD_ASSOCIATE, &assoc, sizeof assoc, 1);
}

static void
iwi_init(void *priv)
{
	struct iwi_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_firmware *fw = &sc->fw;
	struct iwi_rx_data *data;
	int i;

	/* exit immediately if firmware has not been ioctl'd */
	if (!(sc->flags & IWI_FLAG_FW_CACHED)) {
		if (!(sc->flags & IWI_FLAG_FW_WARNED))
			device_printf(sc->sc_dev, "Please load firmware\n");
		sc->flags |= IWI_FLAG_FW_WARNED;
		ifp->if_flags &= ~IFF_UP;
		return;
	}

	iwi_stop(sc);

	if (iwi_reset(sc) != 0) {
		device_printf(sc->sc_dev, "could not reset adapter\n");
		goto fail;
	}

	if (iwi_load_firmware(sc, fw->boot, fw->boot_size) != 0) {
		device_printf(sc->sc_dev, "could not load boot firmware\n");
		goto fail;
	}

	if (iwi_load_ucode(sc, fw->ucode, fw->ucode_size) != 0) {
		device_printf(sc->sc_dev, "could not load microcode\n");
		goto fail;
	}

	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_CMD_BASE, sc->cmdq.physaddr);
	CSR_WRITE_4(sc, IWI_CSR_CMD_SIZE, sc->cmdq.count);
	CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX1_BASE, sc->txq.physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX1_SIZE, sc->txq.count);
	CSR_WRITE_4(sc, IWI_CSR_TX1_WIDX, sc->txq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX2_BASE, sc->txq.physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX2_SIZE, sc->txq.count);
	CSR_WRITE_4(sc, IWI_CSR_TX2_WIDX, sc->txq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX3_BASE, sc->txq.physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX3_SIZE, sc->txq.count);
	CSR_WRITE_4(sc, IWI_CSR_TX3_WIDX, sc->txq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX4_BASE, sc->txq.physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX4_SIZE, sc->txq.count);
	CSR_WRITE_4(sc, IWI_CSR_TX4_WIDX, sc->txq.cur);

	for (i = 0; i < sc->rxq.count; i++) {
		data = &sc->rxq.data[i];
		CSR_WRITE_4(sc, data->reg, data->physaddr);
	}

	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, sc->rxq.count - 1);

	if (iwi_load_firmware(sc, fw->main, fw->main_size) != 0) {
		device_printf(sc->sc_dev, "could not load main firmware\n");
		goto fail;
	}

	sc->flags |= IWI_FLAG_FW_INITED;

	if (iwi_config(sc) != 0) {
		device_printf(sc->sc_dev, "device configuration failed\n");
		goto fail;
	}

	ieee80211_begin_scan(ic, 1);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	return;

fail:	ifp->if_flags &= ~IFF_UP;
	iwi_stop(sc);
}

static void
iwi_stop(void *priv)
{
	struct iwi_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_SOFT_RESET);

	/* reset rings */
	iwi_reset_cmd_ring(sc, &sc->cmdq);
	iwi_reset_tx_ring(sc, &sc->txq);
	iwi_reset_rx_ring(sc, &sc->rxq);

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

#ifdef IWI_DEBUG
static int
iwi_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct iwi_softc *sc = arg1;
	uint32_t size, buf[128];

	if (!(sc->flags & IWI_FLAG_FW_INITED)) {
		memset(buf, 0, sizeof buf);
		return SYSCTL_OUT(req, buf, sizeof buf);
	}

	size = min(CSR_READ_4(sc, IWI_CSR_TABLE0_SIZE), 128 - 1);
	CSR_READ_REGION_4(sc, IWI_CSR_TABLE0_BASE, &buf[1], size);

	return SYSCTL_OUT(req, buf, sizeof buf);
}
#endif

static int
iwi_sysctl_radio(SYSCTL_HANDLER_ARGS)
{
	struct iwi_softc *sc = arg1;
	int val;

	val = (CSR_READ_4(sc, IWI_CSR_IO) & IWI_IO_RADIO_ENABLED) ? 1 : 0;

	return SYSCTL_OUT(req, &val, sizeof val);
}
