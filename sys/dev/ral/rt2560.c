/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005, 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
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

/*-
 * Ralink Technology RT2560 chipset driver
 * http://www.ralinktech.com/
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

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/ral/if_ralrate.h>
#include <dev/ral/rt2560reg.h>
#include <dev/ral/rt2560var.h>

#define RT2560_RSSI(sc, rssi)					\
	((rssi) > (RT2560_NOISE_FLOOR + (sc)->rssi_corr) ?	\
	 ((rssi) - RT2560_NOISE_FLOOR - (sc)->rssi_corr) : 0)

#ifdef RAL_DEBUG
#define DPRINTF(x)	do { if (ral_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (ral_debug >= (n)) printf x; } while (0)
extern int ral_debug;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static void		rt2560_dma_map_addr(void *, bus_dma_segment_t *, int,
			    int);
static int		rt2560_alloc_tx_ring(struct rt2560_softc *,
			    struct rt2560_tx_ring *, int);
static void		rt2560_reset_tx_ring(struct rt2560_softc *,
			    struct rt2560_tx_ring *);
static void		rt2560_free_tx_ring(struct rt2560_softc *,
			    struct rt2560_tx_ring *);
static int		rt2560_alloc_rx_ring(struct rt2560_softc *,
			    struct rt2560_rx_ring *, int);
static void		rt2560_reset_rx_ring(struct rt2560_softc *,
			    struct rt2560_rx_ring *);
static void		rt2560_free_rx_ring(struct rt2560_softc *,
			    struct rt2560_rx_ring *);
static struct		ieee80211_node *rt2560_node_alloc(
			    struct ieee80211_node_table *);
static int		rt2560_media_change(struct ifnet *);
static void		rt2560_iter_func(void *, struct ieee80211_node *);
static void		rt2560_update_rssadapt(void *);
static int		rt2560_newstate(struct ieee80211com *,
			    enum ieee80211_state, int);
static uint16_t		rt2560_eeprom_read(struct rt2560_softc *, uint8_t);
static void		rt2560_encryption_intr(struct rt2560_softc *);
static void		rt2560_tx_intr(struct rt2560_softc *);
static void		rt2560_prio_intr(struct rt2560_softc *);
static void		rt2560_decryption_intr(struct rt2560_softc *);
static void		rt2560_rx_intr(struct rt2560_softc *);
static void		rt2560_beacon_expire(struct rt2560_softc *);
static void		rt2560_wakeup_expire(struct rt2560_softc *);
static uint8_t		rt2560_rxrate(struct rt2560_rx_desc *);
static int		rt2560_ack_rate(struct ieee80211com *, int);
static void		rt2560_scan_start(struct ieee80211com *);
static void		rt2560_scan_end(struct ieee80211com *);
static void		rt2560_set_channel(struct ieee80211com *);
static uint16_t		rt2560_txtime(int, int, uint32_t);
static uint8_t		rt2560_plcp_signal(int);
static void		rt2560_setup_tx_desc(struct rt2560_softc *,
			    struct rt2560_tx_desc *, uint32_t, int, int, int,
			    bus_addr_t);
static int		rt2560_tx_bcn(struct rt2560_softc *, struct mbuf *,
			    struct ieee80211_node *);
static int		rt2560_tx_mgt(struct rt2560_softc *, struct mbuf *,
			    struct ieee80211_node *);
static struct		mbuf *rt2560_get_rts(struct rt2560_softc *,
			    struct ieee80211_frame *, uint16_t);
static int		rt2560_tx_data(struct rt2560_softc *, struct mbuf *,
			    struct ieee80211_node *);
static void		rt2560_start(struct ifnet *);
static void		rt2560_watchdog(void *);
static int		rt2560_reset(struct ifnet *);
static int		rt2560_ioctl(struct ifnet *, u_long, caddr_t);
static void		rt2560_bbp_write(struct rt2560_softc *, uint8_t,
			    uint8_t);
static uint8_t		rt2560_bbp_read(struct rt2560_softc *, uint8_t);
static void		rt2560_rf_write(struct rt2560_softc *, uint8_t,
			    uint32_t);
static void		rt2560_set_chan(struct rt2560_softc *,
			    struct ieee80211_channel *);
#if 0
static void		rt2560_disable_rf_tune(struct rt2560_softc *);
#endif
static void		rt2560_enable_tsf_sync(struct rt2560_softc *);
static void		rt2560_update_plcp(struct rt2560_softc *);
static void		rt2560_update_slot(struct ifnet *);
static void		rt2560_set_basicrates(struct rt2560_softc *);
static void		rt2560_update_led(struct rt2560_softc *, int, int);
static void		rt2560_set_bssid(struct rt2560_softc *, const uint8_t *);
static void		rt2560_set_macaddr(struct rt2560_softc *, uint8_t *);
static void		rt2560_get_macaddr(struct rt2560_softc *, uint8_t *);
static void		rt2560_update_promisc(struct rt2560_softc *);
static const char	*rt2560_get_rf(int);
static void		rt2560_read_eeprom(struct rt2560_softc *);
static int		rt2560_bbp_init(struct rt2560_softc *);
static void		rt2560_set_txantenna(struct rt2560_softc *, int);
static void		rt2560_set_rxantenna(struct rt2560_softc *, int);
static void		rt2560_init(void *);
static int		rt2560_raw_xmit(struct ieee80211_node *, struct mbuf *,
				const struct ieee80211_bpf_params *);

static const struct {
	uint32_t	reg;
	uint32_t	val;
} rt2560_def_mac[] = {
	RT2560_DEF_MAC
};

static const struct {
	uint8_t	reg;
	uint8_t	val;
} rt2560_def_bbp[] = {
	RT2560_DEF_BBP
};

static const uint32_t rt2560_rf2522_r2[]    = RT2560_RF2522_R2;
static const uint32_t rt2560_rf2523_r2[]    = RT2560_RF2523_R2;
static const uint32_t rt2560_rf2524_r2[]    = RT2560_RF2524_R2;
static const uint32_t rt2560_rf2525_r2[]    = RT2560_RF2525_R2;
static const uint32_t rt2560_rf2525_hi_r2[] = RT2560_RF2525_HI_R2;
static const uint32_t rt2560_rf2525e_r2[]   = RT2560_RF2525E_R2;
static const uint32_t rt2560_rf2526_r2[]    = RT2560_RF2526_R2;
static const uint32_t rt2560_rf2526_hi_r2[] = RT2560_RF2526_HI_R2;

static const struct {
	uint8_t		chan;
	uint32_t	r1, r2, r4;
} rt2560_rf5222[] = {
	RT2560_RF5222
};

int
rt2560_attach(device_t dev, int id)
{
	struct rt2560_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp;
	int error, bands;

	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	callout_init_mtx(&sc->watchdog_ch, &sc->sc_mtx, 0);
	callout_init(&sc->rssadapt_ch, CALLOUT_MPSAFE);

	/* retrieve RT2560 rev. no */
	sc->asic_rev = RAL_READ(sc, RT2560_CSR0);

	/* retrieve MAC address */
	rt2560_get_macaddr(sc, ic->ic_myaddr);

	/* retrieve RF rev. no and various other things from EEPROM */
	rt2560_read_eeprom(sc);

	device_printf(dev, "MAC/BBP RT2560 (rev 0x%02x), RF %s\n",
	    sc->asic_rev, rt2560_get_rf(sc->rf_rev));

	/*
	 * Allocate Tx and Rx rings.
	 */
	error = rt2560_alloc_tx_ring(sc, &sc->txq, RT2560_TX_RING_COUNT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Tx ring\n");
		goto fail1;
	}

	error = rt2560_alloc_tx_ring(sc, &sc->atimq, RT2560_ATIM_RING_COUNT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate ATIM ring\n");
		goto fail2;
	}

	error = rt2560_alloc_tx_ring(sc, &sc->prioq, RT2560_PRIO_RING_COUNT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Prio ring\n");
		goto fail3;
	}

	error = rt2560_alloc_tx_ring(sc, &sc->bcnq, RT2560_BEACON_RING_COUNT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Beacon ring\n");
		goto fail4;
	}

	error = rt2560_alloc_rx_ring(sc, &sc->rxq, RT2560_RX_RING_COUNT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Rx ring\n");
		goto fail5;
	}

	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		goto fail6;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = rt2560_init;
	ifp->if_ioctl = rt2560_ioctl;
	ifp->if_start = rt2560_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAp mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_BGSCAN |	/* bg scanning support */
	    IEEE80211_C_WPA;		/* 802.11i */

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	if (sc->rf_rev == RT2560_RF_5222)
		setbit(&bands, IEEE80211_MODE_11A);
	ieee80211_init_channels(ic, 0, CTRY_DEFAULT, bands, 0, 1);

	ieee80211_ifattach(ic);
	ic->ic_scan_start = rt2560_scan_start;
	ic->ic_scan_end = rt2560_scan_end;
	ic->ic_set_channel = rt2560_set_channel;
	ic->ic_node_alloc = rt2560_node_alloc;
	ic->ic_updateslot = rt2560_update_slot;
	ic->ic_reset = rt2560_reset;
	/* enable s/w bmiss handling in sta mode */
	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rt2560_newstate;
	ic->ic_raw_xmit = rt2560_raw_xmit;
	ieee80211_media_init(ic, rt2560_media_change, ieee80211_media_status);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + sizeof (sc->sc_txtap), 
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtap;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RT2560_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtap;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RT2560_TX_RADIOTAP_PRESENT);

	/*
	 * Add a few sysctl knobs.
	 */
	sc->dwelltime = 200;

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "txantenna", CTLFLAG_RW, &sc->tx_ant, 0, "tx antenna (0=auto)");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "rxantenna", CTLFLAG_RW, &sc->rx_ant, 0, "rx antenna (0=auto)");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "dwell",
	    CTLFLAG_RW, &sc->dwelltime, 0,
	    "channel dwell time (ms) for AP/station scanning");

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;

fail6:	rt2560_free_rx_ring(sc, &sc->rxq);
fail5:	rt2560_free_tx_ring(sc, &sc->bcnq);
fail4:	rt2560_free_tx_ring(sc, &sc->prioq);
fail3:	rt2560_free_tx_ring(sc, &sc->atimq);
fail2:	rt2560_free_tx_ring(sc, &sc->txq);
fail1:	mtx_destroy(&sc->sc_mtx);

	return ENXIO;
}

int
rt2560_detach(void *xsc)
{
	struct rt2560_softc *sc = xsc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	
	rt2560_stop(sc);
	callout_stop(&sc->watchdog_ch);
	callout_stop(&sc->rssadapt_ch);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);

	rt2560_free_tx_ring(sc, &sc->txq);
	rt2560_free_tx_ring(sc, &sc->atimq);
	rt2560_free_tx_ring(sc, &sc->prioq);
	rt2560_free_tx_ring(sc, &sc->bcnq);
	rt2560_free_rx_ring(sc, &sc->rxq);

	if_free(ifp);

	mtx_destroy(&sc->sc_mtx);

	return 0;
}

void
rt2560_resume(void *xsc)
{
	struct rt2560_softc *sc = xsc;
	struct ifnet *ifp = sc->sc_ic.ic_ifp;

	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp->if_softc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			ifp->if_start(ifp);
	}
}

static void
rt2560_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
rt2560_alloc_tx_ring(struct rt2560_softc *sc, struct rt2560_tx_ring *ring,
    int count)
{
	int i, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->cur_encrypt = ring->next_encrypt = 0;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 4, 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    count * RT2560_TX_DESC_SIZE, 1, count * RT2560_TX_DESC_SIZE,
	    0, NULL, NULL, &ring->desc_dmat);
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
	    count * RT2560_TX_DESC_SIZE, rt2560_dma_map_addr, &ring->physaddr,
	    0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct rt2560_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MCLBYTES, RT2560_MAX_SCATTER, MCLBYTES, 0, NULL, NULL,
	    &ring->data_dmat);
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

fail:	rt2560_free_tx_ring(sc, ring);
	return error;
}

static void
rt2560_reset_tx_ring(struct rt2560_softc *sc, struct rt2560_tx_ring *ring)
{
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		desc = &ring->desc[i];
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

		desc->flags = 0;
	}

	bus_dmamap_sync(ring->desc_dmat, ring->desc_map, BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->cur_encrypt = ring->next_encrypt = 0;
}

static void
rt2560_free_tx_ring(struct rt2560_softc *sc, struct rt2560_tx_ring *ring)
{
	struct rt2560_tx_data *data;
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
rt2560_alloc_rx_ring(struct rt2560_softc *sc, struct rt2560_rx_ring *ring,
    int count)
{
	struct rt2560_rx_desc *desc;
	struct rt2560_rx_data *data;
	bus_addr_t physaddr;
	int i, error;

	ring->count = count;
	ring->cur = ring->next = 0;
	ring->cur_decrypt = 0;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 4, 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    count * RT2560_RX_DESC_SIZE, 1, count * RT2560_RX_DESC_SIZE,
	    0, NULL, NULL, &ring->desc_dmat);
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
	    count * RT2560_RX_DESC_SIZE, rt2560_dma_map_addr, &ring->physaddr,
	    0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct rt2560_rx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    1, MCLBYTES, 0, NULL, NULL, &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		desc = &sc->rxq.desc[i];
		data = &sc->rxq.data[i];

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
		    mtod(data->m, void *), MCLBYTES, rt2560_dma_map_addr,
		    &physaddr, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load rx buf DMA map");
			goto fail;
		}

		desc->flags = htole32(RT2560_RX_BUSY);
		desc->physaddr = htole32(physaddr);
	}

	bus_dmamap_sync(ring->desc_dmat, ring->desc_map, BUS_DMASYNC_PREWRITE);

	return 0;

fail:	rt2560_free_rx_ring(sc, ring);
	return error;
}

static void
rt2560_reset_rx_ring(struct rt2560_softc *sc, struct rt2560_rx_ring *ring)
{
	int i;

	for (i = 0; i < ring->count; i++) {
		ring->desc[i].flags = htole32(RT2560_RX_BUSY);
		ring->data[i].drop = 0;
	}

	bus_dmamap_sync(ring->desc_dmat, ring->desc_map, BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
	ring->cur_decrypt = 0;
}

static void
rt2560_free_rx_ring(struct rt2560_softc *sc, struct rt2560_rx_ring *ring)
{
	struct rt2560_rx_data *data;
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

static struct ieee80211_node *
rt2560_node_alloc(struct ieee80211_node_table *nt)
{
	struct rt2560_node *rn;

	rn = malloc(sizeof (struct rt2560_node), M_80211_NODE,
	    M_NOWAIT | M_ZERO);

	return (rn != NULL) ? &rn->ni : NULL;
}

static int
rt2560_media_change(struct ifnet *ifp)
{
	struct rt2560_softc *sc = ifp->if_softc;
	int error;

	error = ieee80211_media_change(ifp);

	if (error == ENETRESET) {
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING))
		        rt2560_init(sc);
	}
	return error;
}

/*
 * This function is called for each node present in the node station table.
 */
static void
rt2560_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct rt2560_node *rn = (struct rt2560_node *)ni;

	ral_rssadapt_updatestats(&rn->rssadapt);
}

/*
 * This function is called periodically (every 100ms) in RUN state to update
 * the rate adaptation statistics.
 */
static void
rt2560_update_rssadapt(void *arg)
{
	struct rt2560_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	RAL_LOCK(sc);

	ieee80211_iterate_nodes(&ic->ic_sta, rt2560_iter_func, arg);
	callout_reset(&sc->rssadapt_ch, hz / 10, rt2560_update_rssadapt, sc);

	RAL_UNLOCK(sc);
}

static int
rt2560_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rt2560_softc *sc = ic->ic_ifp->if_softc;
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int error = 0;

	ostate = ic->ic_state;

	switch (nstate) {
	case IEEE80211_S_INIT:
		callout_stop(&sc->rssadapt_ch);

		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			RAL_WRITE(sc, RT2560_CSR14, 0);

			/* turn association led off */
			rt2560_update_led(sc, 0, 0);
		}
		break;
	case IEEE80211_S_RUN:
		ni = ic->ic_bss;

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			rt2560_update_plcp(sc);
			rt2560_set_basicrates(sc);
			rt2560_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS) {
			m = ieee80211_beacon_alloc(ic, ni, &sc->sc_bo);
			if (m == NULL) {
				device_printf(sc->sc_dev,
				    "could not allocate beacon\n");
				error = ENOBUFS;
				break;
			}

			ieee80211_ref_node(ni);
			error = rt2560_tx_bcn(sc, m, ni);
			if (error != 0)
				break;
		}

		/* turn assocation led on */
		rt2560_update_led(sc, 1, 0);

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			callout_reset(&sc->rssadapt_ch, hz / 10,
			    rt2560_update_rssadapt, sc);

			rt2560_enable_tsf_sync(sc);
		}
		break;
	case IEEE80211_S_SCAN:
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
	default:
		break;
	}

	return (error != 0) ? error : sc->sc_newstate(ic, nstate, arg);
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM (either 93C46 or
 * 93C66).
 */
static uint16_t
rt2560_eeprom_read(struct rt2560_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	RT2560_EEPROM_CTL(sc, 0);

	RT2560_EEPROM_CTL(sc, RT2560_S);
	RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_C);
	RT2560_EEPROM_CTL(sc, RT2560_S);

	/* write start bit (1) */
	RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_D);
	RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_D | RT2560_C);

	/* write READ opcode (10) */
	RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_D);
	RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_D | RT2560_C);
	RT2560_EEPROM_CTL(sc, RT2560_S);
	RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_C);

	/* write address (A5-A0 or A7-A0) */
	n = (RAL_READ(sc, RT2560_CSR21) & RT2560_93C46) ? 5 : 7;
	for (; n >= 0; n--) {
		RT2560_EEPROM_CTL(sc, RT2560_S |
		    (((addr >> n) & 1) << RT2560_SHIFT_D));
		RT2560_EEPROM_CTL(sc, RT2560_S |
		    (((addr >> n) & 1) << RT2560_SHIFT_D) | RT2560_C);
	}

	RT2560_EEPROM_CTL(sc, RT2560_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		RT2560_EEPROM_CTL(sc, RT2560_S | RT2560_C);
		tmp = RAL_READ(sc, RT2560_CSR21);
		val |= ((tmp & RT2560_Q) >> RT2560_SHIFT_Q) << n;
		RT2560_EEPROM_CTL(sc, RT2560_S);
	}

	RT2560_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	RT2560_EEPROM_CTL(sc, RT2560_S);
	RT2560_EEPROM_CTL(sc, 0);
	RT2560_EEPROM_CTL(sc, RT2560_C);

	return val;
}

/*
 * Some frames were processed by the hardware cipher engine and are ready for
 * transmission.
 */
static void
rt2560_encryption_intr(struct rt2560_softc *sc)
{
	struct rt2560_tx_desc *desc;
	int hw;

	/* retrieve last descriptor index processed by cipher engine */
	hw = RAL_READ(sc, RT2560_SECCSR1) - sc->txq.physaddr;
	hw /= RT2560_TX_DESC_SIZE;

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (; sc->txq.next_encrypt != hw;) {
		desc = &sc->txq.desc[sc->txq.next_encrypt];

		if ((le32toh(desc->flags) & RT2560_TX_BUSY) ||
		    (le32toh(desc->flags) & RT2560_TX_CIPHER_BUSY))
			break;

		/* for TKIP, swap eiv field to fix a bug in ASIC */
		if ((le32toh(desc->flags) & RT2560_TX_CIPHER_MASK) ==
		    RT2560_TX_CIPHER_TKIP)
			desc->eiv = bswap32(desc->eiv);

		/* mark the frame ready for transmission */
		desc->flags |= htole32(RT2560_TX_BUSY | RT2560_TX_VALID);

		DPRINTFN(15, ("encryption done idx=%u\n",
		    sc->txq.next_encrypt));

		sc->txq.next_encrypt =
		    (sc->txq.next_encrypt + 1) % RT2560_TX_RING_COUNT;
	}

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	/* kick Tx */
	RAL_WRITE(sc, RT2560_TXCSR0, RT2560_KICK_TX);
}

static void
rt2560_tx_intr(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	struct rt2560_node *rn;

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		desc = &sc->txq.desc[sc->txq.next];
		data = &sc->txq.data[sc->txq.next];

		if ((le32toh(desc->flags) & RT2560_TX_BUSY) ||
		    (le32toh(desc->flags) & RT2560_TX_CIPHER_BUSY) ||
		    !(le32toh(desc->flags) & RT2560_TX_VALID))
			break;

		rn = (struct rt2560_node *)data->ni;

		switch (le32toh(desc->flags) & RT2560_TX_RESULT_MASK) {
		case RT2560_TX_SUCCESS:
			DPRINTFN(10, ("data frame sent successfully\n"));
			if (data->id.id_node != NULL) {
				ral_rssadapt_raise_rate(ic, &rn->rssadapt,
				    &data->id);
			}
			ifp->if_opackets++;
			break;

		case RT2560_TX_SUCCESS_RETRY:
			DPRINTFN(9, ("data frame sent after %u retries\n",
			    (le32toh(desc->flags) >> 5) & 0x7));
			ifp->if_opackets++;
			break;

		case RT2560_TX_FAIL_RETRY:
			DPRINTFN(9, ("sending data frame failed (too much "
			    "retries)\n"));
			if (data->id.id_node != NULL) {
				ral_rssadapt_lower_rate(ic, data->ni,
				    &rn->rssadapt, &data->id);
			}
			ifp->if_oerrors++;
			break;

		case RT2560_TX_FAIL_INVALID:
		case RT2560_TX_FAIL_OTHER:
		default:
			device_printf(sc->sc_dev, "sending data frame failed "
			    "0x%08x\n", le32toh(desc->flags));
			ifp->if_oerrors++;
		}

		bus_dmamap_sync(sc->txq.data_dmat, data->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txq.data_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		ieee80211_free_node(data->ni);
		data->ni = NULL;

		/* descriptor is no longer valid */
		desc->flags &= ~htole32(RT2560_TX_VALID);

		DPRINTFN(15, ("tx done idx=%u\n", sc->txq.next));

		sc->txq.queued--;
		sc->txq.next = (sc->txq.next + 1) % RT2560_TX_RING_COUNT;
	}

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	rt2560_start(ifp);
}

static void
rt2560_prio_intr(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int flags;

	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		desc = &sc->prioq.desc[sc->prioq.next];
		data = &sc->prioq.data[sc->prioq.next];

		flags = le32toh(desc->flags);
		if ((flags & RT2560_TX_BUSY) || (flags & RT2560_TX_VALID) == 0)
			break;

		switch (flags & RT2560_TX_RESULT_MASK) {
		case RT2560_TX_SUCCESS:
			DPRINTFN(10, ("mgt frame sent successfully\n"));
			break;

		case RT2560_TX_SUCCESS_RETRY:
			DPRINTFN(9, ("mgt frame sent after %u retries\n",
			    (flags >> 5) & 0x7));
			break;

		case RT2560_TX_FAIL_RETRY:
			DPRINTFN(9, ("sending mgt frame failed (too much "
			    "retries)\n"));
			break;

		case RT2560_TX_FAIL_INVALID:
		case RT2560_TX_FAIL_OTHER:
		default:
			device_printf(sc->sc_dev, "sending mgt frame failed "
			    "0x%08x\n", flags);
			break;
		}

		bus_dmamap_sync(sc->prioq.data_dmat, data->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->prioq.data_dmat, data->map);

		m = data->m;
		data->m = NULL;
		ni = data->ni;
		data->ni = NULL;

		/* descriptor is no longer valid */
		desc->flags &= ~htole32(RT2560_TX_VALID);

		DPRINTFN(15, ("prio done idx=%u\n", sc->prioq.next));

		sc->prioq.queued--;
		sc->prioq.next = (sc->prioq.next + 1) % RT2560_PRIO_RING_COUNT;

		if (m->m_flags & M_TXCB)
			ieee80211_process_callback(ni, m,
				(flags & RT2560_TX_RESULT_MASK) &~
				(RT2560_TX_SUCCESS | RT2560_TX_SUCCESS_RETRY));
		m_freem(m);
		ieee80211_free_node(ni);
	}

	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	rt2560_start(ifp);
}

/*
 * Some frames were processed by the hardware cipher engine and are ready for
 * transmission to the IEEE802.11 layer.
 */
static void
rt2560_decryption_intr(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_rx_desc *desc;
	struct rt2560_rx_data *data;
	bus_addr_t physaddr;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct rt2560_node *rn;
	struct mbuf *mnew, *m;
	int hw, error;

	/* retrieve last decriptor index processed by cipher engine */
	hw = RAL_READ(sc, RT2560_SECCSR0) - sc->rxq.physaddr;
	hw /= RT2560_RX_DESC_SIZE;

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (; sc->rxq.cur_decrypt != hw;) {
		desc = &sc->rxq.desc[sc->rxq.cur_decrypt];
		data = &sc->rxq.data[sc->rxq.cur_decrypt];

		if ((le32toh(desc->flags) & RT2560_RX_BUSY) ||
		    (le32toh(desc->flags) & RT2560_RX_CIPHER_BUSY))
			break;

		if (data->drop) {
			ifp->if_ierrors++;
			goto skip;
		}

		if ((le32toh(desc->flags) & RT2560_RX_CIPHER_MASK) != 0 &&
		    (le32toh(desc->flags) & RT2560_RX_ICV_ERROR)) {
			ifp->if_ierrors++;
			goto skip;
		}

		/*
		 * Try to allocate a new mbuf for this ring element and load it
		 * before processing the current mbuf. If the ring element
		 * cannot be loaded, drop the received packet and reuse the old
		 * mbuf. In the unlikely case that the old mbuf can't be
		 * reloaded either, explicitly panic.
		 */
		mnew = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (mnew == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}

		bus_dmamap_sync(sc->rxq.data_dmat, data->map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rxq.data_dmat, data->map);

		error = bus_dmamap_load(sc->rxq.data_dmat, data->map,
		    mtod(mnew, void *), MCLBYTES, rt2560_dma_map_addr,
		    &physaddr, 0);
		if (error != 0) {
			m_freem(mnew);

			/* try to reload the old mbuf */
			error = bus_dmamap_load(sc->rxq.data_dmat, data->map,
			    mtod(data->m, void *), MCLBYTES,
			    rt2560_dma_map_addr, &physaddr, 0);
			if (error != 0) {
				/* very unlikely that it will fail... */
				panic("%s: could not load old rx mbuf",
				    device_get_name(sc->sc_dev));
			}
			ifp->if_ierrors++;
			goto skip;
		}

		/*
	 	 * New mbuf successfully loaded, update Rx ring and continue
		 * processing.
		 */
		m = data->m;
		data->m = mnew;
		desc->physaddr = htole32(physaddr);

		/* finalize mbuf */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len =
		    (le32toh(desc->flags) >> 16) & 0xfff;

		if (bpf_peers_present(sc->sc_drvbpf)) {
			struct rt2560_rx_radiotap_header *tap = &sc->sc_rxtap;
			uint32_t tsf_lo, tsf_hi;

			/* get timestamp (low and high 32 bits) */
			tsf_hi = RAL_READ(sc, RT2560_CSR17);
			tsf_lo = RAL_READ(sc, RT2560_CSR16);

			tap->wr_tsf =
			    htole64(((uint64_t)tsf_hi << 32) | tsf_lo);
			tap->wr_flags = 0;
			tap->wr_rate = rt2560_rxrate(desc);
			tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
			tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
			tap->wr_antenna = sc->rx_ant;
			tap->wr_antsignal = RT2560_RSSI(sc, desc->rssi);

			bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
		}

		sc->sc_flags |= RAL_INPUT_RUNNING;
		RAL_UNLOCK(sc);
		wh = mtod(m, struct ieee80211_frame *);
		ni = ieee80211_find_rxnode(ic,
		    (struct ieee80211_frame_min *)wh);

		/* send the frame to the 802.11 layer */
		ieee80211_input(ic, m, ni, RT2560_RSSI(sc, desc->rssi),
				RT2560_NOISE_FLOOR, 0);

		/* give rssi to the rate adatation algorithm */
		rn = (struct rt2560_node *)ni;
		ral_rssadapt_input(ic, ni, &rn->rssadapt,
				   RT2560_RSSI(sc, desc->rssi));

		/* node is no longer needed */
		ieee80211_free_node(ni);

		RAL_LOCK(sc);
		sc->sc_flags &= ~RAL_INPUT_RUNNING;
skip:		desc->flags = htole32(RT2560_RX_BUSY);

		DPRINTFN(15, ("decryption done idx=%u\n", sc->rxq.cur_decrypt));

		sc->rxq.cur_decrypt =
		    (sc->rxq.cur_decrypt + 1) % RT2560_RX_RING_COUNT;
	}

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_PREWRITE);
}

/*
 * Some frames were received. Pass them to the hardware cipher engine before
 * sending them to the 802.11 layer.
 */
static void
rt2560_rx_intr(struct rt2560_softc *sc)
{
	struct rt2560_rx_desc *desc;
	struct rt2560_rx_data *data;

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		desc = &sc->rxq.desc[sc->rxq.cur];
		data = &sc->rxq.data[sc->rxq.cur];

		if ((le32toh(desc->flags) & RT2560_RX_BUSY) ||
		    (le32toh(desc->flags) & RT2560_RX_CIPHER_BUSY))
			break;

		data->drop = 0;

		if ((le32toh(desc->flags) & RT2560_RX_PHY_ERROR) ||
		    (le32toh(desc->flags) & RT2560_RX_CRC_ERROR)) {
			/*
			 * This should not happen since we did not request
			 * to receive those frames when we filled RXCSR0.
			 */
			DPRINTFN(5, ("PHY or CRC error flags 0x%08x\n",
			    le32toh(desc->flags)));
			data->drop = 1;
		}

		if (((le32toh(desc->flags) >> 16) & 0xfff) > MCLBYTES) {
			DPRINTFN(5, ("bad length\n"));
			data->drop = 1;
		}

		/* mark the frame for decryption */
		desc->flags |= htole32(RT2560_RX_CIPHER_BUSY);

		DPRINTFN(15, ("rx done idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % RT2560_RX_RING_COUNT;
	}

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	/* kick decrypt */
	RAL_WRITE(sc, RT2560_SECCSR0, RT2560_KICK_DECRYPT);
}

/*
 * This function is called periodically in IBSS mode when a new beacon must be
 * sent out.
 */
static void
rt2560_beacon_expire(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2560_tx_data *data;

	if (ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP)
		return;	

	data = &sc->bcnq.data[sc->bcnq.next];
	/*
	 * Don't send beacon if bsschan isn't set
	 */
	if (data->ni == NULL)
	        return;

	bus_dmamap_sync(sc->bcnq.data_dmat, data->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->bcnq.data_dmat, data->map);

	ieee80211_beacon_update(ic, data->ni, &sc->sc_bo, data->m, 1);

	if (bpf_peers_present(ic->ic_rawbpf))
		bpf_mtap(ic->ic_rawbpf, data->m);

	rt2560_tx_bcn(sc, data->m, data->ni);

	DPRINTFN(15, ("beacon expired\n"));

	sc->bcnq.next = (sc->bcnq.next + 1) % RT2560_BEACON_RING_COUNT;
}

/* ARGSUSED */
static void
rt2560_wakeup_expire(struct rt2560_softc *sc)
{
	DPRINTFN(2, ("wakeup expired\n"));
}

void
rt2560_intr(void *arg)
{
	struct rt2560_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t r;

	RAL_LOCK(sc);

	/* disable interrupts */
	RAL_WRITE(sc, RT2560_CSR8, 0xffffffff);

	/* don't re-enable interrupts if we're shutting down */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		RAL_UNLOCK(sc);
		return;
	}

	r = RAL_READ(sc, RT2560_CSR7);
	RAL_WRITE(sc, RT2560_CSR7, r);

	if (r & RT2560_BEACON_EXPIRE)
		rt2560_beacon_expire(sc);

	if (r & RT2560_WAKEUP_EXPIRE)
		rt2560_wakeup_expire(sc);

	if (r & RT2560_ENCRYPTION_DONE)
		rt2560_encryption_intr(sc);

	if (r & RT2560_TX_DONE)
		rt2560_tx_intr(sc);

	if (r & RT2560_PRIO_DONE)
		rt2560_prio_intr(sc);

	if (r & RT2560_DECRYPTION_DONE)
		rt2560_decryption_intr(sc);

	if (r & RT2560_RX_DONE)
		rt2560_rx_intr(sc);

	/* re-enable interrupts */
	RAL_WRITE(sc, RT2560_CSR8, RT2560_INTR_MASK);

	RAL_UNLOCK(sc);
}

/* quickly determine if a given rate is CCK or OFDM */
#define RAL_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

#define RAL_ACK_SIZE	14	/* 10 + 4(FCS) */
#define RAL_CTS_SIZE	14	/* 10 + 4(FCS) */

#define RAL_SIFS		10	/* us */

#define RT2560_TXRX_TURNAROUND	10	/* us */

/*
 * This function is only used by the Rx radiotap code.
 */
static uint8_t
rt2560_rxrate(struct rt2560_rx_desc *desc)
{
	if (le32toh(desc->flags) & RT2560_RX_OFDM) {
		/* reverse function of rt2560_plcp_signal */
		switch (desc->rate) {
		case 0xb:	return 12;
		case 0xf:	return 18;
		case 0xa:	return 24;
		case 0xe:	return 36;
		case 0x9:	return 48;
		case 0xd:	return 72;
		case 0x8:	return 96;
		case 0xc:	return 108;
		}
	} else {
		if (desc->rate == 10)
			return 2;
		if (desc->rate == 20)
			return 4;
		if (desc->rate == 55)
			return 11;
		if (desc->rate == 110)
			return 22;
	}
	return 2;	/* should not get there */
}

/*
 * Return the expected ack rate for a frame transmitted at rate `rate'.
 * XXX: this should depend on the destination node basic rate set.
 */
static int
rt2560_ack_rate(struct ieee80211com *ic, int rate)
{
	switch (rate) {
	/* CCK rates */
	case 2:
		return 2;
	case 4:
	case 11:
	case 22:
		return (ic->ic_curmode == IEEE80211_MODE_11B) ? 4 : rate;

	/* OFDM rates */
	case 12:
	case 18:
		return 12;
	case 24:
	case 36:
		return 24;
	case 48:
	case 72:
	case 96:
	case 108:
		return 48;
	}

	/* default to 1Mbps */
	return 2;
}

/*
 * Compute the duration (in us) needed to transmit `len' bytes at rate `rate'.
 * The function automatically determines the operating mode depending on the
 * given rate. `flags' indicates whether short preamble is in use or not.
 */
static uint16_t
rt2560_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;

	if (RAL_RATE_IS_OFDM(rate)) {
		/* IEEE Std 802.11a-1999, pp. 37 */
		txtime = (8 + 4 * len + 3 + rate - 1) / rate;
		txtime = 16 + 4 + 4 * txtime + 6;
	} else {
		/* IEEE Std 802.11b-1999, pp. 28 */
		txtime = (16 * len + rate - 1) / rate;
		if (rate != 2 && (flags & IEEE80211_F_SHPREAMBLE))
			txtime +=  72 + 24;
		else
			txtime += 144 + 48;
	}

	return txtime;
}

static uint8_t
rt2560_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 0x0;
	case 4:		return 0x1;
	case 11:	return 0x2;
	case 22:	return 0x3;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:	return 0xb;
	case 18:	return 0xf;
	case 24:	return 0xa;
	case 36:	return 0xe;
	case 48:	return 0x9;
	case 72:	return 0xd;
	case 96:	return 0x8;
	case 108:	return 0xc;

	/* unsupported rates (should not get there) */
	default:	return 0xff;
	}
}

static void
rt2560_setup_tx_desc(struct rt2560_softc *sc, struct rt2560_tx_desc *desc,
    uint32_t flags, int len, int rate, int encrypt, bus_addr_t physaddr)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t plcp_length;
	int remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(len << 16);
	desc->flags |= encrypt ? htole32(RT2560_TX_CIPHER_BUSY) :
	    htole32(RT2560_TX_BUSY | RT2560_TX_VALID);

	desc->physaddr = htole32(physaddr);
	desc->wme = htole16(
	    RT2560_AIFSN(2) |
	    RT2560_LOGCWMIN(3) |
	    RT2560_LOGCWMAX(8));

	/* setup PLCP fields */
	desc->plcp_signal  = rt2560_plcp_signal(rate);
	desc->plcp_service = 4;

	len += IEEE80211_CRC_LEN;
	if (RAL_RATE_IS_OFDM(rate)) {
		desc->flags |= htole32(RT2560_TX_OFDM);

		plcp_length = len & 0xfff;
		desc->plcp_length_hi = plcp_length >> 6;
		desc->plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = (16 * len + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if (remainder != 0 && remainder < 7)
				desc->plcp_service |= RT2560_PLCP_LENGEXT;
		}
		desc->plcp_length_hi = plcp_length >> 8;
		desc->plcp_length_lo = plcp_length & 0xff;

		if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			desc->plcp_signal |= 0x08;
	}
}

static int
rt2560_tx_bcn(struct rt2560_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	bus_dma_segment_t segs[RT2560_MAX_SCATTER];
	int nsegs, rate, error;

	desc = &sc->bcnq.desc[sc->bcnq.cur];
	data = &sc->bcnq.data[sc->bcnq.cur];

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 2;

	error = bus_dmamap_load_mbuf_sg(sc->bcnq.data_dmat, data->map, m0,
	    segs, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rt2560_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	rt2560_setup_tx_desc(sc, desc, RT2560_TX_IFS_NEWBACKOFF |
	    RT2560_TX_TIMESTAMP, m0->m_pkthdr.len, rate, 0, segs->ds_addr);

	DPRINTFN(10, ("sending beacon frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->bcnq.cur, rate));

	bus_dmamap_sync(sc->bcnq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->bcnq.desc_dmat, sc->bcnq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	sc->bcnq.cur = (sc->bcnq.cur + 1) % RT2560_BEACON_RING_COUNT;

	return 0;
}

static int
rt2560_tx_mgt(struct rt2560_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	struct ieee80211_frame *wh;
	bus_dma_segment_t segs[RT2560_MAX_SCATTER];
	uint16_t dur;
	uint32_t flags = 0;
	int nsegs, rate, error;

	desc = &sc->prioq.desc[sc->prioq.cur];
	data = &sc->prioq.data[sc->prioq.cur];

	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;

	error = bus_dmamap_load_mbuf_sg(sc->prioq.data_dmat, data->map, m0,
	    segs, &nsegs, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rt2560_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2560_TX_ACK;

		dur = rt2560_txtime(RAL_ACK_SIZE, rate, ic->ic_flags) +
		      RAL_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT &&
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= RT2560_TX_TIMESTAMP;
	}

	rt2560_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate, 0,
	    segs->ds_addr);

	bus_dmamap_sync(sc->prioq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending mgt frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->prioq.cur, rate));

	/* kick prio */
	sc->prioq.queued++;
	sc->prioq.cur = (sc->prioq.cur + 1) % RT2560_PRIO_RING_COUNT;
	RAL_WRITE(sc, RT2560_TXCSR0, RT2560_KICK_PRIO);

	return 0;
}

static int
rt2560_tx_raw(struct rt2560_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni, const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	bus_dma_segment_t segs[RT2560_MAX_SCATTER];
	uint32_t flags;
	int nsegs, rate, error;

	desc = &sc->prioq.desc[sc->prioq.cur];
	data = &sc->prioq.data[sc->prioq.cur];

	rate = params->ibp_rate0 & IEEE80211_RATE_VAL;
	/* XXX validate */
	if (rate == 0) {
		m_freem(m0);
		return EINVAL;
	}

	error = bus_dmamap_load_mbuf_sg(sc->prioq.data_dmat, data->map, m0,
	    segs, &nsegs, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rt2560_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	flags = 0;
	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0)
		flags |= RT2560_TX_ACK;

	/* XXX need to setup descriptor ourself */
	rt2560_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len,
	    rate, (params->ibp_flags & IEEE80211_BPF_CRYPTO) != 0,
	    segs->ds_addr);

	bus_dmamap_sync(sc->prioq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending raw frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->prioq.cur, rate));

	/* kick prio */
	sc->prioq.queued++;
	sc->prioq.cur = (sc->prioq.cur + 1) % RT2560_PRIO_RING_COUNT;
	RAL_WRITE(sc, RT2560_TXCSR0, RT2560_KICK_PRIO);

	return 0;
}

/*
 * Build a RTS control frame.
 */
static struct mbuf *
rt2560_get_rts(struct rt2560_softc *sc, struct ieee80211_frame *wh,
    uint16_t dur)
{
	struct ieee80211_frame_rts *rts;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		sc->sc_ic.ic_stats.is_tx_nobuf++;
		device_printf(sc->sc_dev, "could not allocate RTS frame\n");
		return NULL;
	}

	rts = mtod(m, struct ieee80211_frame_rts *);

	rts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	    IEEE80211_FC0_SUBTYPE_RTS;
	rts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(uint16_t *)rts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(rts->i_ra, wh->i_addr1);
	IEEE80211_ADDR_COPY(rts->i_ta, wh->i_addr2);

	m->m_pkthdr.len = m->m_len = sizeof (struct ieee80211_frame_rts);

	return m;
}

static int
rt2560_tx_data(struct rt2560_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2560_tx_desc *desc;
	struct rt2560_tx_data *data;
	struct rt2560_node *rn;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct mbuf *mnew;
	bus_dma_segment_t segs[RT2560_MAX_SCATTER];
	uint16_t dur;
	uint32_t flags = 0;
	int nsegs, rate, error;

	wh = mtod(m0, struct ieee80211_frame *);

	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		rate = ic->ic_fixed_rate;
	} else {
		struct ieee80211_rateset *rs;

		rs = &ni->ni_rates;
		rn = (struct rt2560_node *)ni;
		ni->ni_txrate = ral_rssadapt_choose(&rn->rssadapt, rs, wh,
		    m0->m_pkthdr.len, NULL, 0);
		rate = rs->rs_rates[ni->ni_txrate];
	}
	rate &= IEEE80211_RATE_VAL;

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	/*
	 * IEEE Std 802.11-1999, pp 82: "A STA shall use an RTS/CTS exchange
	 * for directed frames only when the length of the MPDU is greater
	 * than the length threshold indicated by [...]" ic_rtsthreshold.
	 */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    m0->m_pkthdr.len > ic->ic_rtsthreshold) {
		struct mbuf *m;
		uint16_t dur;
		int rtsrate, ackrate;

		rtsrate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;
		ackrate = rt2560_ack_rate(ic, rate);

		dur = rt2560_txtime(m0->m_pkthdr.len + 4, rate, ic->ic_flags) +
		      rt2560_txtime(RAL_CTS_SIZE, rtsrate, ic->ic_flags) +
		      rt2560_txtime(RAL_ACK_SIZE, ackrate, ic->ic_flags) +
		      3 * RAL_SIFS;

		m = rt2560_get_rts(sc, wh, dur);

		desc = &sc->txq.desc[sc->txq.cur_encrypt];
		data = &sc->txq.data[sc->txq.cur_encrypt];

		error = bus_dmamap_load_mbuf_sg(sc->txq.data_dmat, data->map,
		    m, segs, &nsegs, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not map mbuf (error %d)\n", error);
			m_freem(m);
			m_freem(m0);
			return error;
		}

		/* avoid multiple free() of the same node for each fragment */
		ieee80211_ref_node(ni);

		data->m = m;
		data->ni = ni;

		/* RTS frames are not taken into account for rssadapt */
		data->id.id_node = NULL;

		rt2560_setup_tx_desc(sc, desc, RT2560_TX_ACK |
		    RT2560_TX_MORE_FRAG, m->m_pkthdr.len, rtsrate, 1,
		    segs->ds_addr);

		bus_dmamap_sync(sc->txq.data_dmat, data->map,
		    BUS_DMASYNC_PREWRITE);

		sc->txq.queued++;
		sc->txq.cur_encrypt =
		    (sc->txq.cur_encrypt + 1) % RT2560_TX_RING_COUNT;

		/*
		 * IEEE Std 802.11-1999: when an RTS/CTS exchange is used, the
		 * asynchronous data frame shall be transmitted after the CTS
		 * frame and a SIFS period.
		 */
		flags |= RT2560_TX_LONG_RETRY | RT2560_TX_IFS_SIFS;
	}

	data = &sc->txq.data[sc->txq.cur_encrypt];
	desc = &sc->txq.desc[sc->txq.cur_encrypt];

	error = bus_dmamap_load_mbuf_sg(sc->txq.data_dmat, data->map, m0,
	    segs, &nsegs, 0);
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

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rt2560_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	/* remember link conditions for rate adaptation algorithm */
	if (ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE) {
		data->id.id_len = m0->m_pkthdr.len;
		data->id.id_rateidx = ni->ni_txrate;
		data->id.id_node = ni;
		data->id.id_rssi = ni->ni_rssi;
	} else
		data->id.id_node = NULL;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2560_TX_ACK;

		dur = rt2560_txtime(RAL_ACK_SIZE, rt2560_ack_rate(ic, rate),
		    ic->ic_flags) + RAL_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

	rt2560_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate, 1,
	    segs->ds_addr);

	bus_dmamap_sync(sc->txq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending data frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->txq.cur_encrypt, rate));

	/* kick encrypt */
	sc->txq.queued++;
	sc->txq.cur_encrypt = (sc->txq.cur_encrypt + 1) % RT2560_TX_RING_COUNT;
	RAL_WRITE(sc, RT2560_SECCSR1, RT2560_KICK_ENCRYPT);

	return 0;
}

static void
rt2560_start(struct ifnet *ifp)
{
	struct rt2560_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni;

	RAL_LOCK(sc);

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		RAL_UNLOCK(sc);
		return;
	}

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->prioq.queued >= RT2560_PRIO_RING_COUNT) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;

			if (bpf_peers_present(ic->ic_rawbpf))
				bpf_mtap(ic->ic_rawbpf, m0);

			if (rt2560_tx_mgt(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				break;
			}
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DRV_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->txq.queued >= RT2560_TX_RING_COUNT - 1) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m0);
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
			/*
			 * Cancel any background scan.
			 */
			if (ic->ic_flags & IEEE80211_F_SCAN)
				ieee80211_cancel_scan(ic);

			if (m0->m_len < sizeof (struct ether_header) &&
			    !(m0 = m_pullup(m0, sizeof (struct ether_header))))
				continue;

			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m0);
				continue;
			}
			if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
			    (m0->m_flags & M_PWR_SAV) == 0) {
				/*
				 * Station in power save mode; pass the frame
				 * to the 802.11 layer and continue.  We'll get
				 * the frame back when the time is right.
				 */
				ieee80211_pwrsave(ni, m0);
				/*
				 * If we're in power save mode 'cuz of a bg
				 * scan cancel it so the traffic can flow.
				 * The packet we just queued will automatically
				 * get sent when we drop out of power save.
				 * XXX locking
				 */
				if (ic->ic_flags & IEEE80211_F_SCAN)
					ieee80211_cancel_scan(ic);
				ieee80211_free_node(ni);
				continue;

			}

			BPF_MTAP(ifp, m0);

			m0 = ieee80211_encap(ic, m0, ni);
			if (m0 == NULL) {
				ieee80211_free_node(ni);
				continue;
			}
			
			if (bpf_peers_present(ic->ic_rawbpf))
				bpf_mtap(ic->ic_rawbpf, m0);

			if (rt2560_tx_data(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->sc_tx_timer = 5;
		ic->ic_lastdata = ticks;
		callout_reset(&sc->watchdog_ch, hz, rt2560_watchdog, sc);
	}

	RAL_UNLOCK(sc);
}

static void
rt2560_watchdog(void *arg)
{
	struct rt2560_softc *sc = arg;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			rt2560_init(sc);
			sc->sc_ifp->if_oerrors++;
			return;
		}
		callout_reset(&sc->watchdog_ch, hz, rt2560_watchdog, sc);
	}
}

/*
 * This function allows for fast channel switching in monitor mode (used by
 * net-mgmt/kismet). In IBSS mode, we must explicitly reset the interface to
 * generate a new beacon frame.
 */
static int
rt2560_reset(struct ifnet *ifp)
{
	struct rt2560_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		return ENETRESET;

	rt2560_set_chan(sc, ic->ic_curchan);

	return 0;
}

static int
rt2560_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rt2560_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;



	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			RAL_LOCK(sc);
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				rt2560_update_promisc(sc);
			else
				rt2560_init(sc);
			RAL_UNLOCK(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				rt2560_stop(sc);
		}

		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) &&
		    (ic->ic_roaming != IEEE80211_ROAMING_MANUAL))
			rt2560_init(sc);
		error = 0;
	}


	return error;
}

static void
rt2560_bbp_write(struct rt2560_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RT2560_BBPCSR) & RT2560_BBP_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "could not write to BBP\n");
		return;
	}

	tmp = RT2560_BBP_WRITE | RT2560_BBP_BUSY | reg << 8 | val;
	RAL_WRITE(sc, RT2560_BBPCSR, tmp);

	DPRINTFN(15, ("BBP R%u <- 0x%02x\n", reg, val));
}

static uint8_t
rt2560_bbp_read(struct rt2560_softc *sc, uint8_t reg)
{
	uint32_t val;
	int ntries;

	val = RT2560_BBP_BUSY | reg << 8;
	RAL_WRITE(sc, RT2560_BBPCSR, val);

	for (ntries = 0; ntries < 100; ntries++) {
		val = RAL_READ(sc, RT2560_BBPCSR);
		if (!(val & RT2560_BBP_BUSY))
			return val & 0xff;
		DELAY(1);
	}

	device_printf(sc->sc_dev, "could not read from BBP\n");
	return 0;
}

static void
rt2560_rf_write(struct rt2560_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RT2560_RFCSR) & RT2560_RF_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "could not write to RF\n");
		return;
	}

	tmp = RT2560_RF_BUSY | RT2560_RF_20BIT | (val & 0xfffff) << 2 |
	    (reg & 0x3);
	RAL_WRITE(sc, RT2560_RFCSR, tmp);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 0x3, val & 0xfffff));
}

static void
rt2560_set_chan(struct rt2560_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t power, tmp;
	u_int i, chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		power = min(sc->txpow[chan - 1], 31);
	else
		power = 31;

	/* adjust txpower using ifconfig settings */
	power -= (100 - ic->ic_txpowlimit) / 8;

	DPRINTFN(2, ("setting channel to %u, txpower to %u\n", chan, power));

	switch (sc->rf_rev) {
	case RT2560_RF_2522:
		rt2560_rf_write(sc, RAL_RF1, 0x00814);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2522_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		break;

	case RT2560_RF_2523:
		rt2560_rf_write(sc, RAL_RF1, 0x08804);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2523_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x38044);
		rt2560_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RT2560_RF_2524:
		rt2560_rf_write(sc, RAL_RF1, 0x0c808);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2524_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		rt2560_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RT2560_RF_2525:
		rt2560_rf_write(sc, RAL_RF1, 0x08808);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2525_hi_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		rt2560_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);

		rt2560_rf_write(sc, RAL_RF1, 0x08808);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2525_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		rt2560_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RT2560_RF_2525E:
		rt2560_rf_write(sc, RAL_RF1, 0x08808);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2525e_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		rt2560_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00286 : 0x00282);
		break;

	case RT2560_RF_2526:
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2526_hi_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		rt2560_rf_write(sc, RAL_RF1, 0x08804);

		rt2560_rf_write(sc, RAL_RF2, rt2560_rf2526_r2[chan - 1]);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		rt2560_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		break;

	/* dual-band RF */
	case RT2560_RF_5222:
		for (i = 0; rt2560_rf5222[i].chan != chan; i++);

		rt2560_rf_write(sc, RAL_RF1, rt2560_rf5222[i].r1);
		rt2560_rf_write(sc, RAL_RF2, rt2560_rf5222[i].r2);
		rt2560_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		rt2560_rf_write(sc, RAL_RF4, rt2560_rf5222[i].r4);
		break;
	default: 
 	        printf("unknown ral rev=%d\n", sc->rf_rev);
	}

	if (ic->ic_state != IEEE80211_S_SCAN) {
		/* set Japan filter bit for channel 14 */
		tmp = rt2560_bbp_read(sc, 70);

		tmp &= ~RT2560_JAPAN_FILTER;
		if (chan == 14)
			tmp |= RT2560_JAPAN_FILTER;

		rt2560_bbp_write(sc, 70, tmp);

		/* clear CRC errors */
		RAL_READ(sc, RT2560_CNT0);
	}
}

static void
rt2560_set_channel(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_softc *sc = ifp->if_softc;

	RAL_LOCK(sc);
	rt2560_set_chan(sc, ic->ic_curchan);
	RAL_UNLOCK(sc);

}

#if 0
/*
 * Disable RF auto-tuning.
 */
static void
rt2560_disable_rf_tune(struct rt2560_softc *sc)
{
	uint32_t tmp;

	if (sc->rf_rev != RT2560_RF_2523) {
		tmp = sc->rf_regs[RAL_RF1] & ~RAL_RF1_AUTOTUNE;
		rt2560_rf_write(sc, RAL_RF1, tmp);
	}

	tmp = sc->rf_regs[RAL_RF3] & ~RAL_RF3_AUTOTUNE;
	rt2560_rf_write(sc, RAL_RF3, tmp);

	DPRINTFN(2, ("disabling RF autotune\n"));
}
#endif

/*
 * Refer to IEEE Std 802.11-1999 pp. 123 for more information on TSF
 * synchronization.
 */
static void
rt2560_enable_tsf_sync(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t logcwmin, preload;
	uint32_t tmp;

	/* first, disable TSF synchronization */
	RAL_WRITE(sc, RT2560_CSR14, 0);

	tmp = 16 * ic->ic_bss->ni_intval;
	RAL_WRITE(sc, RT2560_CSR12, tmp);

	RAL_WRITE(sc, RT2560_CSR13, 0);

	logcwmin = 5;
	preload = (ic->ic_opmode == IEEE80211_M_STA) ? 384 : 1024;
	tmp = logcwmin << 16 | preload;
	RAL_WRITE(sc, RT2560_BCNOCSR, tmp);

	/* finally, enable TSF synchronization */
	tmp = RT2560_ENABLE_TSF | RT2560_ENABLE_TBCN;
	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= RT2560_ENABLE_TSF_SYNC(1);
	else
		tmp |= RT2560_ENABLE_TSF_SYNC(2) |
		       RT2560_ENABLE_BEACON_GENERATOR;
	RAL_WRITE(sc, RT2560_CSR14, tmp);

	DPRINTF(("enabling TSF synchronization\n"));
}

static void
rt2560_update_plcp(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* no short preamble for 1Mbps */
	RAL_WRITE(sc, RT2560_PLCP1MCSR, 0x00700400);

	if (!(ic->ic_flags & IEEE80211_F_SHPREAMBLE)) {
		/* values taken from the reference driver */
		RAL_WRITE(sc, RT2560_PLCP2MCSR,   0x00380401);
		RAL_WRITE(sc, RT2560_PLCP5p5MCSR, 0x00150402);
		RAL_WRITE(sc, RT2560_PLCP11MCSR,  0x000b8403);
	} else {
		/* same values as above or'ed 0x8 */
		RAL_WRITE(sc, RT2560_PLCP2MCSR,   0x00380409);
		RAL_WRITE(sc, RT2560_PLCP5p5MCSR, 0x0015040a);
		RAL_WRITE(sc, RT2560_PLCP11MCSR,  0x000b840b);
	}

	DPRINTF(("updating PLCP for %s preamble\n",
	    (ic->ic_flags & IEEE80211_F_SHPREAMBLE) ? "short" : "long"));
}

/*
 * This function can be called by ieee80211_set_shortslottime(). Refer to
 * IEEE Std 802.11-1999 pp. 85 to know how these values are computed.
 */
static void
rt2560_update_slot(struct ifnet *ifp)
{
	struct rt2560_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t slottime;
	uint16_t tx_sifs, tx_pifs, tx_difs, eifs;
	uint32_t tmp;

	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	/* update the MAC slot boundaries */
	tx_sifs = RAL_SIFS - RT2560_TXRX_TURNAROUND;
	tx_pifs = tx_sifs + slottime;
	tx_difs = tx_sifs + 2 * slottime;
	eifs = (ic->ic_curmode == IEEE80211_MODE_11B) ? 364 : 60;

	tmp = RAL_READ(sc, RT2560_CSR11);
	tmp = (tmp & ~0x1f00) | slottime << 8;
	RAL_WRITE(sc, RT2560_CSR11, tmp);

	tmp = tx_pifs << 16 | tx_sifs;
	RAL_WRITE(sc, RT2560_CSR18, tmp);

	tmp = eifs << 16 | tx_difs;
	RAL_WRITE(sc, RT2560_CSR19, tmp);

	DPRINTF(("setting slottime to %uus\n", slottime));
}

static void
rt2560_set_basicrates(struct rt2560_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* update basic rate set */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		/* 11b basic rates: 1, 2Mbps */
		RAL_WRITE(sc, RT2560_ARSP_PLCP_1, 0x3);
	} else if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
		/* 11a basic rates: 6, 12, 24Mbps */
		RAL_WRITE(sc, RT2560_ARSP_PLCP_1, 0x150);
	} else {
		/* 11g basic rates: 1, 2, 5.5, 11, 6, 12, 24Mbps */
		RAL_WRITE(sc, RT2560_ARSP_PLCP_1, 0x15f);
	}
}

static void
rt2560_update_led(struct rt2560_softc *sc, int led1, int led2)
{
	uint32_t tmp;

	/* set ON period to 70ms and OFF period to 30ms */
	tmp = led1 << 16 | led2 << 17 | 70 << 8 | 30;
	RAL_WRITE(sc, RT2560_LEDCSR, tmp);
}

static void
rt2560_set_bssid(struct rt2560_softc *sc, const uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24;
	RAL_WRITE(sc, RT2560_CSR5, tmp);

	tmp = bssid[4] | bssid[5] << 8;
	RAL_WRITE(sc, RT2560_CSR6, tmp);

	DPRINTF(("setting BSSID to %6D\n", bssid, ":"));
}

static void
rt2560_set_macaddr(struct rt2560_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
	RAL_WRITE(sc, RT2560_CSR3, tmp);

	tmp = addr[4] | addr[5] << 8;
	RAL_WRITE(sc, RT2560_CSR4, tmp);

	DPRINTF(("setting MAC address to %6D\n", addr, ":"));
}

static void
rt2560_get_macaddr(struct rt2560_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = RAL_READ(sc, RT2560_CSR3);
	addr[0] = tmp & 0xff;
	addr[1] = (tmp >>  8) & 0xff;
	addr[2] = (tmp >> 16) & 0xff;
	addr[3] = (tmp >> 24);

	tmp = RAL_READ(sc, RT2560_CSR4);
	addr[4] = tmp & 0xff;
	addr[5] = (tmp >> 8) & 0xff;
}

static void
rt2560_update_promisc(struct rt2560_softc *sc)
{
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t tmp;

	tmp = RAL_READ(sc, RT2560_RXCSR0);

	tmp &= ~RT2560_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RT2560_DROP_NOT_TO_ME;

	RAL_WRITE(sc, RT2560_RXCSR0, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

static const char *
rt2560_get_rf(int rev)
{
	switch (rev) {
	case RT2560_RF_2522:	return "RT2522";
	case RT2560_RF_2523:	return "RT2523";
	case RT2560_RF_2524:	return "RT2524";
	case RT2560_RF_2525:	return "RT2525";
	case RT2560_RF_2525E:	return "RT2525e";
	case RT2560_RF_2526:	return "RT2526";
	case RT2560_RF_5222:	return "RT5222";
	default:		return "unknown";
	}
}

static void
rt2560_read_eeprom(struct rt2560_softc *sc)
{
	uint16_t val;
	int i;

	val = rt2560_eeprom_read(sc, RT2560_EEPROM_CONFIG0);
	sc->rf_rev =   (val >> 11) & 0x7;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->led_mode = (val >> 6)  & 0x7;
	sc->rx_ant =   (val >> 4)  & 0x3;
	sc->tx_ant =   (val >> 2)  & 0x3;
	sc->nb_ant =   val & 0x3;

	/* read default values for BBP registers */
	for (i = 0; i < 16; i++) {
		val = rt2560_eeprom_read(sc, RT2560_EEPROM_BBP_BASE + i);
		sc->bbp_prom[i].reg = val >> 8;
		sc->bbp_prom[i].val = val & 0xff;
	}

	/* read Tx power for all b/g channels */
	for (i = 0; i < 14 / 2; i++) {
		val = rt2560_eeprom_read(sc, RT2560_EEPROM_TXPOWER + i);
		sc->txpow[i * 2] = val >> 8;
		sc->txpow[i * 2 + 1] = val & 0xff;
	}

	val = rt2560_eeprom_read(sc, RT2560_EEPROM_CALIBRATE);
	if ((val & 0xff) == 0xff)
		sc->rssi_corr = RT2560_DEFAULT_RSSI_CORR;
	else
		sc->rssi_corr = val & 0xff;
	DPRINTF(("rssi correction %d, calibrate 0x%02x\n",
		 sc->rssi_corr, val));
}


static void
rt2560_scan_start(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_softc *sc = ifp->if_softc;

	/* abort TSF synchronization */
	RAL_WRITE(sc, RT2560_CSR14, 0);
	rt2560_set_bssid(sc, ifp->if_broadcastaddr);
}

static void
rt2560_scan_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_softc *sc = ifp->if_softc;

	rt2560_enable_tsf_sync(sc);
	/* XXX keep local copy */
	rt2560_set_bssid(sc, ic->ic_bss->ni_bssid);
}

static int
rt2560_bbp_init(struct rt2560_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		if (rt2560_bbp_read(sc, RT2560_BBP_VERSION) != 0)
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for BBP\n");
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(rt2560_def_bbp); i++) {
		rt2560_bbp_write(sc, rt2560_def_bbp[i].reg,
		    rt2560_def_bbp[i].val);
	}
#if 0
	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0xff)
			continue;
		rt2560_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}
#endif

	return 0;
#undef N
}

static void
rt2560_set_txantenna(struct rt2560_softc *sc, int antenna)
{
	uint32_t tmp;
	uint8_t tx;

	tx = rt2560_bbp_read(sc, RT2560_BBP_TX) & ~RT2560_BBP_ANTMASK;
	if (antenna == 1)
		tx |= RT2560_BBP_ANTA;
	else if (antenna == 2)
		tx |= RT2560_BBP_ANTB;
	else
		tx |= RT2560_BBP_DIVERSITY;

	/* need to force I/Q flip for RF 2525e, 2526 and 5222 */
	if (sc->rf_rev == RT2560_RF_2525E || sc->rf_rev == RT2560_RF_2526 ||
	    sc->rf_rev == RT2560_RF_5222)
		tx |= RT2560_BBP_FLIPIQ;

	rt2560_bbp_write(sc, RT2560_BBP_TX, tx);

	/* update values for CCK and OFDM in BBPCSR1 */
	tmp = RAL_READ(sc, RT2560_BBPCSR1) & ~0x00070007;
	tmp |= (tx & 0x7) << 16 | (tx & 0x7);
	RAL_WRITE(sc, RT2560_BBPCSR1, tmp);
}

static void
rt2560_set_rxantenna(struct rt2560_softc *sc, int antenna)
{
	uint8_t rx;

	rx = rt2560_bbp_read(sc, RT2560_BBP_RX) & ~RT2560_BBP_ANTMASK;
	if (antenna == 1)
		rx |= RT2560_BBP_ANTA;
	else if (antenna == 2)
		rx |= RT2560_BBP_ANTB;
	else
		rx |= RT2560_BBP_DIVERSITY;

	/* need to force no I/Q flip for RF 2525e and 2526 */
	if (sc->rf_rev == RT2560_RF_2525E || sc->rf_rev == RT2560_RF_2526)
		rx &= ~RT2560_BBP_FLIPIQ;

	rt2560_bbp_write(sc, RT2560_BBP_RX, rx);
}

static void
rt2560_init(void *priv)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct rt2560_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	uint32_t tmp;
	int i;



	rt2560_stop(sc);

	RAL_LOCK(sc);
	/* setup tx rings */
	tmp = RT2560_PRIO_RING_COUNT << 24 |
	      RT2560_ATIM_RING_COUNT << 16 |
	      RT2560_TX_RING_COUNT   <<  8 |
	      RT2560_TX_DESC_SIZE;

	/* rings must be initialized in this exact order */
	RAL_WRITE(sc, RT2560_TXCSR2, tmp);
	RAL_WRITE(sc, RT2560_TXCSR3, sc->txq.physaddr);
	RAL_WRITE(sc, RT2560_TXCSR5, sc->prioq.physaddr);
	RAL_WRITE(sc, RT2560_TXCSR4, sc->atimq.physaddr);
	RAL_WRITE(sc, RT2560_TXCSR6, sc->bcnq.physaddr);

	/* setup rx ring */
	tmp = RT2560_RX_RING_COUNT << 8 | RT2560_RX_DESC_SIZE;

	RAL_WRITE(sc, RT2560_RXCSR1, tmp);
	RAL_WRITE(sc, RT2560_RXCSR2, sc->rxq.physaddr);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(rt2560_def_mac); i++)
		RAL_WRITE(sc, rt2560_def_mac[i].reg, rt2560_def_mac[i].val);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	rt2560_set_macaddr(sc, ic->ic_myaddr);

	/* set basic rate set (will be updated later) */
	RAL_WRITE(sc, RT2560_ARSP_PLCP_1, 0x153);

	rt2560_set_txantenna(sc, sc->tx_ant);
	rt2560_set_rxantenna(sc, sc->rx_ant);
	rt2560_update_slot(ifp);
	rt2560_update_plcp(sc);
	rt2560_update_led(sc, 0, 0);

	RAL_WRITE(sc, RT2560_CSR1, RT2560_RESET_ASIC);
	RAL_WRITE(sc, RT2560_CSR1, RT2560_HOST_READY);

	if (rt2560_bbp_init(sc) != 0) {
		rt2560_stop(sc);
		RAL_UNLOCK(sc);
		return;
	}

	/* set default BSS channel */
	rt2560_set_chan(sc, ic->ic_curchan);

	/* kick Rx */
	tmp = RT2560_DROP_PHY_ERROR | RT2560_DROP_CRC_ERROR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2560_DROP_CTL | RT2560_DROP_VERSION_ERROR;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RT2560_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RT2560_DROP_NOT_TO_ME;
	}
	RAL_WRITE(sc, RT2560_RXCSR0, tmp);

	/* clear old FCS and Rx FIFO errors */
	RAL_READ(sc, RT2560_CNT0);
	RAL_READ(sc, RT2560_CNT4);

	/* clear any pending interrupts */
	RAL_WRITE(sc, RT2560_CSR7, 0xffffffff);

	/* enable interrupts */
	RAL_WRITE(sc, RT2560_CSR8, RT2560_INTR_MASK);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	RAL_UNLOCK(sc);
#undef N
}

void
rt2560_stop(void *arg)
{
	struct rt2560_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	volatile int *flags = &sc->sc_flags;

	while (*flags & RAL_INPUT_RUNNING) {
		tsleep(sc, 0, "ralrunning", hz/10);
	}

	RAL_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		sc->sc_tx_timer = 0;
		ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

		/* abort Tx */
		RAL_WRITE(sc, RT2560_TXCSR0, RT2560_ABORT_TX);
		
		/* disable Rx */
		RAL_WRITE(sc, RT2560_RXCSR0, RT2560_DISABLE_RX);

		/* reset ASIC (imply reset BBP) */
		RAL_WRITE(sc, RT2560_CSR1, RT2560_RESET_ASIC);
		RAL_WRITE(sc, RT2560_CSR1, 0);

		/* disable interrupts */
		RAL_WRITE(sc, RT2560_CSR8, 0xffffffff);
		
		/* reset Tx and Rx rings */
		rt2560_reset_tx_ring(sc, &sc->txq);
		rt2560_reset_tx_ring(sc, &sc->atimq);
		rt2560_reset_tx_ring(sc, &sc->prioq);
		rt2560_reset_tx_ring(sc, &sc->bcnq);
		rt2560_reset_rx_ring(sc, &sc->rxq);
	}
	RAL_UNLOCK(sc);
}

static int
rt2560_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rt2560_softc *sc = ifp->if_softc;

	RAL_LOCK(sc);

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		RAL_UNLOCK(sc);
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}
	if (sc->prioq.queued >= RT2560_PRIO_RING_COUNT) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		RAL_UNLOCK(sc);
		m_freem(m);
		ieee80211_free_node(ni);
		return ENOBUFS;		/* XXX */
	}

	if (bpf_peers_present(ic->ic_rawbpf))
		bpf_mtap(ic->ic_rawbpf, m);

	ifp->if_opackets++;

	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		if (rt2560_tx_mgt(sc, m, ni) != 0)
			goto bad;
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		if (rt2560_tx_raw(sc, m, ni, params))
			goto bad;
	}
	sc->sc_tx_timer = 5;
	callout_reset(&sc->watchdog_ch, hz, rt2560_watchdog, sc);

	RAL_UNLOCK(sc);

	return 0;
bad:
	ifp->if_oerrors++;
	ieee80211_free_node(ni);
	RAL_UNLOCK(sc);
	return EIO;		/* XXX */
}

