/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005
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
 * Ralink Technology RT2500 chipset driver
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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/ral/if_ralrate.h>
#include <dev/ral/if_ralreg.h>
#include <dev/ral/if_ralvar.h>

#ifdef RAL_DEBUG
#define DPRINTF(x)	do { if (ral_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (ral_debug >= (n)) printf x; } while (0)
int ral_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, ral, CTLFLAG_RW, &ral_debug, 0, "ral debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

MODULE_DEPEND(ral, wlan, 1, 1, 1);

static void		ral_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int		ral_alloc_tx_ring(struct ral_softc *,
			    struct ral_tx_ring *, int);
static void		ral_reset_tx_ring(struct ral_softc *,
			    struct ral_tx_ring *);
static void		ral_free_tx_ring(struct ral_softc *,
			    struct ral_tx_ring *);
static int		ral_alloc_rx_ring(struct ral_softc *,
			    struct ral_rx_ring *, int);
static void		ral_reset_rx_ring(struct ral_softc *,
			    struct ral_rx_ring *);
static void		ral_free_rx_ring(struct ral_softc *,
			    struct ral_rx_ring *);
static struct		ieee80211_node *ral_node_alloc(
			    struct ieee80211_node_table *);
static int		ral_media_change(struct ifnet *);
static void		ral_next_scan(void *);
static void		ral_iter_func(void *, struct ieee80211_node *);
static void		ral_update_rssadapt(void *);
static int		ral_newstate(struct ieee80211com *,
			    enum ieee80211_state, int);
static uint16_t		ral_eeprom_read(struct ral_softc *, uint8_t);
static void		ral_encryption_intr(struct ral_softc *);
static void		ral_tx_intr(struct ral_softc *);
static void		ral_prio_intr(struct ral_softc *);
static void		ral_decryption_intr(struct ral_softc *);
static void		ral_rx_intr(struct ral_softc *);
static void		ral_beacon_expire(struct ral_softc *);
static void		ral_wakeup_expire(struct ral_softc *);
static void		ral_intr(void *);
static int		ral_ack_rate(int);
static uint16_t		ral_txtime(int, int, uint32_t);
static uint8_t		ral_plcp_signal(int);
static void		ral_setup_tx_desc(struct ral_softc *,
			    struct ral_tx_desc *, uint32_t, int, int, int,
			    bus_addr_t);
static int		ral_tx_bcn(struct ral_softc *, struct mbuf *,
			    struct ieee80211_node *);
static int		ral_tx_mgt(struct ral_softc *, struct mbuf *,
			    struct ieee80211_node *);
static struct		mbuf *ral_get_rts(struct ral_softc *,
			    struct ieee80211_frame *, uint16_t);
static int		ral_tx_data(struct ral_softc *, struct mbuf *,
			    struct ieee80211_node *);
static void		ral_start(struct ifnet *);
static void		ral_watchdog(struct ifnet *);
static int		ral_reset(struct ifnet *);
static int		ral_ioctl(struct ifnet *, u_long, caddr_t);
static void		ral_bbp_write(struct ral_softc *, uint8_t, uint8_t);
static uint8_t		ral_bbp_read(struct ral_softc *, uint8_t);
static void		ral_rf_write(struct ral_softc *, uint8_t, uint32_t);
static void		ral_set_chan(struct ral_softc *,
			    struct ieee80211_channel *);
#if 0
static void		ral_disable_rf_tune(struct ral_softc *);
#endif
static void		ral_enable_tsf_sync(struct ral_softc *);
static void		ral_update_plcp(struct ral_softc *);
static void		ral_update_slot(struct ifnet *);
static void		ral_update_led(struct ral_softc *, int, int);
static void		ral_set_bssid(struct ral_softc *, uint8_t *);
static void		ral_set_macaddr(struct ral_softc *, uint8_t *);
static void		ral_get_macaddr(struct ral_softc *, uint8_t *);
static void		ral_update_promisc(struct ral_softc *);
static const char	*ral_get_rf(int);
static void		ral_read_eeprom(struct ral_softc *);
static int		ral_bbp_init(struct ral_softc *);
static void		ral_set_txantenna(struct ral_softc *, int);
static void		ral_set_rxantenna(struct ral_softc *, int);
static void		ral_init(void *);

devclass_t ral_devclass;

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset ral_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset ral_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset ral_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

/*
 * Default values for MAC registers; values taken from the reference driver.
 */
static const struct {
	uint32_t	reg;
	uint32_t	val;
} ral_def_mac[] = {
	{ RAL_PSCSR0,      0x00020002 },
	{ RAL_PSCSR1,      0x00000002 },
	{ RAL_PSCSR2,      0x00020002 },
	{ RAL_PSCSR3,      0x00000002 },
	{ RAL_TIMECSR,     0x00003f21 },
	{ RAL_CSR9,        0x00000780 },
	{ RAL_CSR11,       0x07041483 },
	{ RAL_CNT3,        0x00000000 },
	{ RAL_TXCSR1,      0x07614562 },
	{ RAL_ARSP_PLCP_0, 0x8c8d8b8a },
	{ RAL_ACKPCTCSR,   0x7038140a },
	{ RAL_ARTCSR1,     0x1d21252d },
	{ RAL_ARTCSR2,     0x1919191d },
	{ RAL_RXCSR0,      0xffffffff },
	{ RAL_RXCSR3,      0xb3aab3af },
	{ RAL_PCICSR,      0x000003b8 },
	{ RAL_PWRCSR0,     0x3f3b3100 },
	{ RAL_GPIOCSR,     0x0000ff00 },
	{ RAL_TESTCSR,     0x000000f0 },
	{ RAL_PWRCSR1,     0x000001ff },
	{ RAL_MACCSR0,     0x00213223 },
	{ RAL_MACCSR1,     0x00235518 },
	{ RAL_RLPWCSR,     0x00000040 },
	{ RAL_RALINKCSR,   0x9a009a11 },
	{ RAL_CSR7,        0xffffffff },
	{ RAL_BBPCSR1,     0x82188200 },
	{ RAL_TXACKCSR0,   0x00000020 },
	{ RAL_SECCSR3,     0x0000e78f }
};

/*
 * Default values for BBP registers; values taken from the reference driver.
 */
static const struct {
	uint8_t	reg;
	uint8_t	val;
} ral_def_bbp[] = {
	{  3, 0x02 },
	{  4, 0x19 },
	{ 14, 0x1c },
	{ 15, 0x30 },
	{ 16, 0xac },
	{ 17, 0x48 },
	{ 18, 0x18 },
	{ 19, 0xff },
	{ 20, 0x1e },
	{ 21, 0x08 },
	{ 22, 0x08 },
	{ 23, 0x08 },
	{ 24, 0x80 },
	{ 25, 0x50 },
	{ 26, 0x08 },
	{ 27, 0x23 },
	{ 30, 0x10 },
	{ 31, 0x2b },
	{ 32, 0xb9 },
	{ 34, 0x12 },
	{ 35, 0x50 },
	{ 39, 0xc4 },
	{ 40, 0x02 },
	{ 41, 0x60 },
	{ 53, 0x10 },
	{ 54, 0x18 },
	{ 56, 0x08 },
	{ 57, 0x10 },
	{ 58, 0x08 },
	{ 61, 0x60 },
	{ 62, 0x10 },
	{ 75, 0xff }
};

/*
 * Default values for RF register R2 indexed by channel numbers; values taken
 * from the reference driver.
 */
static const uint32_t ral_rf2522_r2[] = {
	0x307f6, 0x307fb, 0x30800, 0x30805, 0x3080a, 0x3080f, 0x30814,
	0x30819, 0x3081e, 0x30823, 0x30828, 0x3082d, 0x30832, 0x3083e
};

static const uint32_t ral_rf2523_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ral_rf2524_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ral_rf2525_r2[] = {
	0x20327, 0x20328, 0x20329, 0x2032a, 0x2032b, 0x2032c, 0x2032d,
	0x2032e, 0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20346
};

static const uint32_t ral_rf2525_hi_r2[] = {
	0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20344, 0x20345,
	0x20346, 0x20347, 0x20348, 0x20349, 0x2034a, 0x2034b, 0x2034e
};

static const uint32_t ral_rf2525e_r2[] = {
	0x2044d, 0x2044e, 0x2044f, 0x20460, 0x20461, 0x20462, 0x20463,
	0x20464, 0x20465, 0x20466, 0x20467, 0x20468, 0x20469, 0x2046b
};

static const uint32_t ral_rf2526_hi_r2[] = {
	0x0022a, 0x0022b, 0x0022b, 0x0022c, 0x0022c, 0x0022d, 0x0022d,
	0x0022e, 0x0022e, 0x0022f, 0x0022d, 0x00240, 0x00240, 0x00241
};

static const uint32_t ral_rf2526_r2[] = {
	0x00226, 0x00227, 0x00227, 0x00228, 0x00228, 0x00229, 0x00229,
	0x0022a, 0x0022a, 0x0022b, 0x0022b, 0x0022c, 0x0022c, 0x0022d
};

/*
 * For dual-band RF, RF registers R1 and R4 also depend on channel number;
 * values taken from the reference driver.
 */
static const struct {
	uint8_t		chan;
	uint32_t	r1;
	uint32_t	r2;
	uint32_t	r4;
} ral_rf5222[] = {
	/* channels in the 2.4GHz band */
	{   1, 0x08808, 0x0044d, 0x00282 },
	{   2, 0x08808, 0x0044e, 0x00282 },
	{   3, 0x08808, 0x0044f, 0x00282 },
	{   4, 0x08808, 0x00460, 0x00282 },
	{   5, 0x08808, 0x00461, 0x00282 },
	{   6, 0x08808, 0x00462, 0x00282 },
	{   7, 0x08808, 0x00463, 0x00282 },
	{   8, 0x08808, 0x00464, 0x00282 },
	{   9, 0x08808, 0x00465, 0x00282 },
	{  10, 0x08808, 0x00466, 0x00282 },
	{  11, 0x08808, 0x00467, 0x00282 },
	{  12, 0x08808, 0x00468, 0x00282 },
	{  13, 0x08808, 0x00469, 0x00282 },
	{  14, 0x08808, 0x0046b, 0x00286 },

	/* channels in the 5.2GHz band */
	{  36, 0x08804, 0x06225, 0x00287 },
	{  40, 0x08804, 0x06226, 0x00287 },
	{  44, 0x08804, 0x06227, 0x00287 },
	{  48, 0x08804, 0x06228, 0x00287 },
	{  52, 0x08804, 0x06229, 0x00287 },
	{  56, 0x08804, 0x0622a, 0x00287 },
	{  60, 0x08804, 0x0622b, 0x00287 },
	{  64, 0x08804, 0x0622c, 0x00287 },

	{ 100, 0x08804, 0x02200, 0x00283 },
	{ 104, 0x08804, 0x02201, 0x00283 },
	{ 108, 0x08804, 0x02202, 0x00283 },
	{ 112, 0x08804, 0x02203, 0x00283 },
	{ 116, 0x08804, 0x02204, 0x00283 },
	{ 120, 0x08804, 0x02205, 0x00283 },
	{ 124, 0x08804, 0x02206, 0x00283 },
	{ 128, 0x08804, 0x02207, 0x00283 },
	{ 132, 0x08804, 0x02208, 0x00283 },
	{ 136, 0x08804, 0x02209, 0x00283 },
	{ 140, 0x08804, 0x0220a, 0x00283 },

	{ 149, 0x08808, 0x02429, 0x00281 },
	{ 153, 0x08808, 0x0242b, 0x00281 },
	{ 157, 0x08808, 0x0242d, 0x00281 },
	{ 161, 0x08808, 0x0242f, 0x00281 }
};

int
ral_attach(device_t dev)
{
	struct ral_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	struct ieee80211com *ic = &sc->sc_ic;
	int error, i;

	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	callout_init(&sc->scan_ch, debug_mpsafenet ? CALLOUT_MPSAFE : 0);
	callout_init(&sc->rssadapt_ch, CALLOUT_MPSAFE);

	/* retrieve RT2560 rev. no */
	sc->asic_rev = RAL_READ(sc, RAL_CSR0);

	/* retrieve MAC address */
	ral_get_macaddr(sc, ic->ic_myaddr);

	/* retrieve RF rev. no and various other things from EEPROM */
	ral_read_eeprom(sc);

	device_printf(dev, "MAC/BBP RT2560 (rev 0x%02x), RF %s\n",
	    sc->asic_rev, ral_get_rf(sc->rf_rev));

	/*
	 * Allocate Tx and Rx rings.
	 */
	if (ral_alloc_tx_ring(sc, &sc->txq, RAL_TX_RING_COUNT) != 0) {
		device_printf(sc->sc_dev, "could not allocate Tx ring\n");
		goto fail1;
	}

	if (ral_alloc_tx_ring(sc, &sc->atimq, RAL_ATIM_RING_COUNT) != 0) {
		device_printf(sc->sc_dev, "could not allocate ATIM ring\n");
		goto fail2;
	}

	if (ral_alloc_tx_ring(sc, &sc->prioq, RAL_PRIO_RING_COUNT) != 0) {
		device_printf(sc->sc_dev, "could not allocate Prio ring\n");
		goto fail3;
	}

	if (ral_alloc_tx_ring(sc, &sc->bcnq, RAL_BEACON_RING_COUNT) != 0) {
		device_printf(sc->sc_dev, "could not allocate Beacon ring\n");
		goto fail4;
	}

	if (ral_alloc_rx_ring(sc, &sc->rxq, RAL_RX_RING_COUNT) != 0) {
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
	ifp->if_init = ral_init;
	ifp->if_ioctl = ral_ioctl;
	ifp->if_start = ral_start;
	ifp->if_watchdog = ral_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps = IEEE80211_C_MONITOR | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_SHPREAMBLE | IEEE80211_C_SHSLOT |
	    IEEE80211_C_PMGT | IEEE80211_C_TXPMGT | IEEE80211_C_WPA;

	if (sc->rf_rev == RAL_RF_5222) {
		/* set supported .11a rates */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = ral_rateset_11a;

		/* set supported .11a channels */
		for (i = 36; i <= 64; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 100; i <= 140; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 149; i <= 161; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ral_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ral_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ieee80211_ifattach(ic);
	ic->ic_node_alloc = ral_node_alloc;
	ic->ic_updateslot = ral_update_slot;
	ic->ic_reset = ral_reset;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ral_newstate;
	ieee80211_media_init(ic, ral_media_change, ieee80211_media_status);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64, &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RAL_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RAL_TX_RADIOTAP_PRESENT);

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

	/*
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    ral_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "could not set up interrupt\n");
		goto fail7;
	}

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;

fail7:	bpfdetach(ifp);
	ieee80211_ifdetach(ic);
fail6:	if_free(ifp);

	ral_free_rx_ring(sc, &sc->rxq);
fail5:	ral_free_tx_ring(sc, &sc->bcnq);
fail4:	ral_free_tx_ring(sc, &sc->prioq);
fail3:	ral_free_tx_ring(sc, &sc->atimq);
fail2:	ral_free_tx_ring(sc, &sc->txq);
fail1:	mtx_destroy(&sc->sc_mtx);

	return ENXIO;
}

int
ral_detach(device_t dev)
{
	struct ral_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	callout_stop(&sc->scan_ch);
	callout_stop(&sc->rssadapt_ch);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);
	if_free(ifp);

	ral_free_tx_ring(sc, &sc->txq);
	ral_free_tx_ring(sc, &sc->atimq);
	ral_free_tx_ring(sc, &sc->prioq);
	ral_free_tx_ring(sc, &sc->bcnq);
	ral_free_rx_ring(sc, &sc->rxq);

	bus_teardown_intr(dev, sc->irq, sc->sc_ih);
	ral_free(dev);

	mtx_destroy(&sc->sc_mtx);

	return 0;
}

void
ral_shutdown(device_t dev)
{
	struct ral_softc *sc = device_get_softc(dev);

	ral_stop(sc);
}

int
ral_alloc(device_t dev, int rid)
{
	struct ral_softc *sc = device_get_softc(dev);

	sc->mem_rid = rid;
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		return ENXIO;
	}

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		ral_free(dev);
		return ENXIO;
	}

	return 0;
}

void
ral_free(device_t dev)
{
	struct ral_softc *sc = device_get_softc(dev);

	if (sc->irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
		sc->irq = NULL;
	}

	if (sc->mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem);
		sc->mem = NULL;
	}
}

static void
ral_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
ral_alloc_tx_ring(struct ral_softc *sc, struct ral_tx_ring *ring, int count)
{
	int i, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->cur_encrypt = ring->next_encrypt = 0;

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, count * RAL_TX_DESC_SIZE, 1,
	    count * RAL_TX_DESC_SIZE, 0, NULL, NULL, &ring->desc_dmat);
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
	    count * RAL_TX_DESC_SIZE, ral_dma_map_addr, &ring->physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct ral_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, RAL_MAX_SCATTER, MCLBYTES,
	    0, NULL, NULL, &ring->data_dmat);
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

fail:	ral_free_tx_ring(sc, ring);
	return error;
}

static void
ral_reset_tx_ring(struct ral_softc *sc, struct ral_tx_ring *ring)
{
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
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
ral_free_tx_ring(struct ral_softc *sc, struct ral_tx_ring *ring)
{
	struct ral_tx_data *data;
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
ral_alloc_rx_ring(struct ral_softc *sc, struct ral_rx_ring *ring, int count)
{
	struct ral_rx_desc *desc;
	struct ral_rx_data *data;
	bus_addr_t physaddr;
	int i, error;

	ring->count = count;
	ring->cur = ring->next = 0;
	ring->cur_decrypt = 0;

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, count * RAL_RX_DESC_SIZE, 1,
	    count * RAL_RX_DESC_SIZE, 0, NULL, NULL, &ring->desc_dmat);
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
	    count * RAL_RX_DESC_SIZE, ral_dma_map_addr, &ring->physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct ral_rx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	error = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, 1, MCLBYTES, 0, NULL,
	    NULL, &ring->data_dmat);
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
		    mtod(data->m, void *), MCLBYTES, ral_dma_map_addr,
		    &physaddr, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load rx buf DMA map");
			goto fail;
		}

		desc->flags = htole32(RAL_RX_BUSY);
		desc->physaddr = htole32(physaddr);
	}

	bus_dmamap_sync(ring->desc_dmat, ring->desc_map, BUS_DMASYNC_PREWRITE);

	return 0;

fail:	ral_free_rx_ring(sc, ring);
	return error;
}

static void
ral_reset_rx_ring(struct ral_softc *sc, struct ral_rx_ring *ring)
{
	int i;

	for (i = 0; i < ring->count; i++) {
		ring->desc[i].flags = htole32(RAL_RX_BUSY);
		ring->data[i].drop = 0;
	}

	bus_dmamap_sync(ring->desc_dmat, ring->desc_map, BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
	ring->cur_decrypt = 0;
}

static void
ral_free_rx_ring(struct ral_softc *sc, struct ral_rx_ring *ring)
{
	struct ral_rx_data *data;
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
ral_node_alloc(struct ieee80211_node_table *nt)
{
	struct ral_node *rn;

	rn = malloc(sizeof (struct ral_node), M_80211_NODE, M_NOWAIT | M_ZERO);

	return (rn != NULL) ? &rn->ni : NULL;
}

static int
ral_media_change(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		ral_init(sc);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
static void
ral_next_scan(void *arg)
{
	struct ral_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
}

/*
 * This function is called for each node present in the node station table.
 */
static void
ral_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct ral_node *rn = (struct ral_node *)ni;

	ral_rssadapt_updatestats(&rn->rssadapt);
}

/*
 * This function is called periodically (every 100ms) in RUN state to update
 * the rate adaptation statistics.
 */
static void
ral_update_rssadapt(void *arg)
{
	struct ral_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	RAL_LOCK(sc);

	ieee80211_iterate_nodes(&ic->ic_sta, ral_iter_func, arg);
	callout_reset(&sc->rssadapt_ch, hz / 10, ral_update_rssadapt, sc);

	RAL_UNLOCK(sc);
}

static int
ral_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ral_softc *sc = ic->ic_ifp->if_softc;
	struct mbuf *m;
	enum ieee80211_state ostate;
	int error = 0;

	ostate = ic->ic_state;
	callout_stop(&sc->scan_ch);

	switch (nstate) {
	case IEEE80211_S_INIT:
		callout_stop(&sc->rssadapt_ch);

		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			RAL_WRITE(sc, RAL_CSR14, 0);

			/* turn association led off */
			ral_update_led(sc, 0, 0);
		}
		break;

	case IEEE80211_S_SCAN:
		ral_set_chan(sc, ic->ic_bss->ni_chan);
		callout_reset(&sc->scan_ch, (sc->dwelltime * hz) / 1000,
		    ral_next_scan, sc);
		break;

	case IEEE80211_S_AUTH:
		ral_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_ASSOC:
		ral_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_RUN:
		ral_set_chan(sc, ic->ic_bss->ni_chan);

		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			ral_set_bssid(sc, ic->ic_bss->ni_bssid);

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS) {
			m = ieee80211_beacon_alloc(ic, ic->ic_bss, &sc->sc_bo);
			if (m == NULL) {
				device_printf(sc->sc_dev,
				    "could not allocate beacon\n");
				error = ENOBUFS;
				break;
			}

			ieee80211_ref_node(ic->ic_bss);
			error = ral_tx_bcn(sc, m, ic->ic_bss);
			if (error != 0)
				break;
		}

		/* turn assocation led on */
		ral_update_led(sc, 1, 0);

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			callout_reset(&sc->rssadapt_ch, hz / 10,
			    ral_update_rssadapt, sc);

			ral_enable_tsf_sync(sc);
		}
		break;
	}

	return (error != 0) ? error : sc->sc_newstate(ic, nstate, arg);
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM (either 93C46 or
 * 93C66).
 */
static uint16_t
ral_eeprom_read(struct ral_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	RAL_EEPROM_CTL(sc, 0);

	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_C);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);

	/* write start bit (1) */
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D | RAL_EEPROM_C);

	/* write READ opcode (10) */
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_D | RAL_EEPROM_C);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_C);

	/* write address (A5-A0 or A7-A0) */
	n = (RAL_READ(sc, RAL_CSR21) & RAL_EEPROM_93C46) ? 5 : 7;
	for (; n >= 0; n--) {
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S |
		    (((addr >> n) & 1) << RAL_EEPROM_SHIFT_D));
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S |
		    (((addr >> n) & 1) << RAL_EEPROM_SHIFT_D) | RAL_EEPROM_C);
	}

	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S | RAL_EEPROM_C);
		tmp = RAL_READ(sc, RAL_CSR21);
		val |= ((tmp & RAL_EEPROM_Q) >> RAL_EEPROM_SHIFT_Q) << n;
		RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	}

	RAL_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	RAL_EEPROM_CTL(sc, RAL_EEPROM_S);
	RAL_EEPROM_CTL(sc, 0);
	RAL_EEPROM_CTL(sc, RAL_EEPROM_C);

	return le16toh(val);
}

/*
 * Some frames were processed by the hardware cipher engine and are ready for
 * transmission.
 */
static void
ral_encryption_intr(struct ral_softc *sc)
{
	struct ral_tx_desc *desc;
	int hw;

	/* retrieve last descriptor index processed by cipher engine */
	hw = (RAL_READ(sc, RAL_SECCSR1) - sc->txq.physaddr) / RAL_TX_DESC_SIZE;

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (; sc->txq.next_encrypt != hw;) {
		desc = &sc->txq.desc[sc->txq.next_encrypt];

		if ((le32toh(desc->flags) & RAL_TX_BUSY) ||
		    (le32toh(desc->flags) & RAL_TX_CIPHER_BUSY))
			break;

		/* for TKIP, swap eiv field to fix a bug in ASIC */
		if ((le32toh(desc->flags) & RAL_TX_CIPHER_MASK) ==
		    RAL_TX_CIPHER_TKIP)
			desc->eiv = bswap32(desc->eiv);

		/* mark the frame ready for transmission */
		desc->flags |= htole32(RAL_TX_BUSY | RAL_TX_VALID);

		DPRINTFN(15, ("encryption done idx=%u\n",
		    sc->txq.next_encrypt));

		sc->txq.next_encrypt =
		    (sc->txq.next_encrypt + 1) % RAL_TX_RING_COUNT;
	}

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	/* kick Tx */
	RAL_WRITE(sc, RAL_TXCSR0, RAL_KICK_TX);
}

static void
ral_tx_intr(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	struct ral_node *rn;

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		desc = &sc->txq.desc[sc->txq.next];
		data = &sc->txq.data[sc->txq.next];

		if ((le32toh(desc->flags) & RAL_TX_BUSY) ||
		    (le32toh(desc->flags) & RAL_TX_CIPHER_BUSY) ||
		    !(le32toh(desc->flags) & RAL_TX_VALID))
			break;

		rn = (struct ral_node *)data->ni;

		switch (le32toh(desc->flags) & RAL_TX_RESULT_MASK) {
		case RAL_TX_SUCCESS:
			DPRINTFN(10, ("data frame sent successfully\n"));
			if (data->id.id_node != NULL) {
				ral_rssadapt_raise_rate(ic, &rn->rssadapt,
				    &data->id);
			}
			ifp->if_opackets++;
			break;

		case RAL_TX_SUCCESS_RETRY:
			DPRINTFN(9, ("data frame sent after %u retries\n",
			    (le32toh(desc->flags) >> 5) & 0x7));
			ifp->if_opackets++;
			break;

		case RAL_TX_FAIL_RETRY:
			DPRINTFN(9, ("sending data frame failed (too much "
			    "retries)\n"));
			if (data->id.id_node != NULL) {
				ral_rssadapt_lower_rate(ic, data->ni,
				    &rn->rssadapt, &data->id);
			}
			ifp->if_oerrors++;
			break;

		case RAL_TX_FAIL_INVALID:
		case RAL_TX_FAIL_OTHER:
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
		desc->flags &= ~htole32(RAL_TX_VALID);

		DPRINTFN(15, ("tx done idx=%u\n", sc->txq.next));

		sc->txq.queued--;
		sc->txq.next = (sc->txq.next + 1) % RAL_TX_RING_COUNT;
	}

	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	ral_start(ifp);
}

static void
ral_prio_intr(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;

	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		desc = &sc->prioq.desc[sc->prioq.next];
		data = &sc->prioq.data[sc->prioq.next];

		if ((le32toh(desc->flags) & RAL_TX_BUSY) ||
		    !(le32toh(desc->flags) & RAL_TX_VALID))
			break;

		switch (le32toh(desc->flags) & RAL_TX_RESULT_MASK) {
		case RAL_TX_SUCCESS:
			DPRINTFN(10, ("mgt frame sent successfully\n"));
			break;

		case RAL_TX_SUCCESS_RETRY:
			DPRINTFN(9, ("mgt frame sent after %u retries\n",
			    (le32toh(desc->flags) >> 5) & 0x7));
			break;

		case RAL_TX_FAIL_RETRY:
			DPRINTFN(9, ("sending mgt frame failed (too much "
			    "retries)\n"));
			break;

		case RAL_TX_FAIL_INVALID:
		case RAL_TX_FAIL_OTHER:
		default:
			device_printf(sc->sc_dev, "sending mgt frame failed "
			    "0x%08x\n", le32toh(desc->flags));
		}

		bus_dmamap_sync(sc->prioq.data_dmat, data->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->prioq.data_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		ieee80211_free_node(data->ni);
		data->ni = NULL;

		/* descriptor is no longer valid */
		desc->flags &= ~htole32(RAL_TX_VALID);

		DPRINTFN(15, ("prio done idx=%u\n", sc->prioq.next));

		sc->prioq.queued--;
		sc->prioq.next = (sc->prioq.next + 1) % RAL_PRIO_RING_COUNT;
	}

	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	ral_start(ifp);
}

/*
 * Some frames were processed by the hardware cipher engine and are ready for
 * transmission to the IEEE802.11 layer.
 */
static void
ral_decryption_intr(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ral_rx_desc *desc;
	struct ral_rx_data *data;
	bus_addr_t physaddr;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ral_node *rn;
	struct mbuf *m;
	int hw, error;

	/* retrieve last decriptor index processed by cipher engine */
	hw = (RAL_READ(sc, RAL_SECCSR0) - sc->rxq.physaddr) / RAL_RX_DESC_SIZE;

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (; sc->rxq.cur_decrypt != hw;) {
		desc = &sc->rxq.desc[sc->rxq.cur_decrypt];
		data = &sc->rxq.data[sc->rxq.cur_decrypt];

		if ((le32toh(desc->flags) & RAL_RX_BUSY) ||
		    (le32toh(desc->flags) & RAL_RX_CIPHER_BUSY))
			break;

		if (data->drop) {
			ifp->if_ierrors++;
			goto skip;
		}

		if ((le32toh(desc->flags) & RAL_RX_CIPHER_MASK) != 0 &&
		    (le32toh(desc->flags) & RAL_RX_ICV_ERROR)) {
			ifp->if_ierrors++;
			goto skip;
		}

		bus_dmamap_sync(sc->rxq.data_dmat, data->map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rxq.data_dmat, data->map);

		/* finalize mbuf */
		m = data->m;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len =
		    (le32toh(desc->flags) >> 16) & 0xfff;

		if (sc->sc_drvbpf != NULL) {
			struct ral_rx_radiotap_header *tap = &sc->sc_rxtap;
			uint32_t tsf_lo, tsf_hi;

			/* get timestamp (low and high 32 bits) */
			tsf_lo = RAL_READ(sc, RAL_CSR16);
			tsf_hi = RAL_READ(sc, RAL_CSR17);

			tap->wr_tsf =
			    htole64(((uint64_t)tsf_hi << 32) | tsf_lo);
			tap->wr_flags = 0;
			tap->wr_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
			tap->wr_chan_flags =
			    htole16(ic->ic_ibss_chan->ic_flags);
			tap->wr_antenna = sc->rx_ant;
			tap->wr_antsignal = desc->rssi;

			bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
		}

		wh = mtod(m, struct ieee80211_frame *);
		ni = ieee80211_find_rxnode(ic,
		    (struct ieee80211_frame_min *)wh);

		/* send the frame to the 802.11 layer */
		ieee80211_input(ic, m, ni, desc->rssi, 0);

		/* give rssi to the rate adatation algorithm */
		rn = (struct ral_node *)ni;
		ral_rssadapt_input(ic, ni, &rn->rssadapt, desc->rssi);

		/* node is no longer needed */
		ieee80211_free_node(ni);

		data->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (data->m == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			break;
		}

		error = bus_dmamap_load(sc->rxq.data_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, ral_dma_map_addr,
		    &physaddr, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load rx buf DMA map\n");
			m_freem(data->m);
			data->m = NULL;
			break;
		}

		desc->physaddr = htole32(physaddr);
skip:		desc->flags = htole32(RAL_RX_BUSY);

		DPRINTFN(15, ("decryption done idx=%u\n", sc->rxq.cur_decrypt));

		sc->rxq.cur_decrypt =
		    (sc->rxq.cur_decrypt + 1) % RAL_RX_RING_COUNT;
	}

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_PREWRITE);
}

/*
 * Some frames were received. Pass them to the hardware cipher engine before
 * sending them to the 802.11 layer.
 */
static void
ral_rx_intr(struct ral_softc *sc)
{
	struct ral_rx_desc *desc;
	struct ral_rx_data *data;

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		desc = &sc->rxq.desc[sc->rxq.cur];
		data = &sc->rxq.data[sc->rxq.cur];

		if ((le32toh(desc->flags) & RAL_RX_BUSY) ||
		    (le32toh(desc->flags) & RAL_RX_CIPHER_BUSY))
			break;

		data->drop = 0;

		if ((le32toh(desc->flags) & RAL_RX_PHY_ERROR) ||
		    (le32toh(desc->flags) & RAL_RX_CRC_ERROR)) {
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
		desc->flags |= htole32(RAL_RX_CIPHER_BUSY);

		DPRINTFN(15, ("rx done idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % RAL_RX_RING_COUNT;
	}

	bus_dmamap_sync(sc->rxq.desc_dmat, sc->rxq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	/* kick decrypt */
	RAL_WRITE(sc, RAL_SECCSR0, RAL_KICK_DECRYPT);
}

/*
 * This function is called periodically in IBSS mode when a new beacon must be
 * sent out.
 */
static void
ral_beacon_expire(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_data *data;

	if (ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP)
		return;

	data = &sc->bcnq.data[sc->bcnq.next];

	bus_dmamap_sync(sc->bcnq.data_dmat, data->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->bcnq.data_dmat, data->map);

	ieee80211_beacon_update(ic, data->ni, &sc->sc_bo, data->m, 1);

	if (ic->ic_rawbpf != NULL)
		bpf_mtap(ic->ic_rawbpf, data->m);

	ral_tx_bcn(sc, data->m, data->ni);

	DPRINTFN(15, ("beacon expired\n"));

	sc->bcnq.next = (sc->bcnq.next + 1) % RAL_BEACON_RING_COUNT;
}

static void
ral_wakeup_expire(struct ral_softc *sc)
{
	DPRINTFN(2, ("wakeup expired\n"));
}

static void
ral_intr(void *arg)
{
	struct ral_softc *sc = arg;
	uint32_t r;

	RAL_LOCK(sc);

	/* disable interrupts */
	RAL_WRITE(sc, RAL_CSR8, 0xffffffff);

	r = RAL_READ(sc, RAL_CSR7);
	RAL_WRITE(sc, RAL_CSR7, r);

	if (r & RAL_BEACON_EXPIRE)
		ral_beacon_expire(sc);

	if (r & RAL_WAKEUP_EXPIRE)
		ral_wakeup_expire(sc);

	if (r & RAL_ENCRYPTION_DONE)
		ral_encryption_intr(sc);

	if (r & RAL_TX_DONE)
		ral_tx_intr(sc);

	if (r & RAL_PRIO_DONE)
		ral_prio_intr(sc);

	if (r & RAL_DECRYPTION_DONE)
		ral_decryption_intr(sc);

	if (r & RAL_RX_DONE)
		ral_rx_intr(sc);

	/* re-enable interrupts */
	RAL_WRITE(sc, RAL_CSR8, RAL_INTR_MASK);

	RAL_UNLOCK(sc);
}

/* quickly determine if a given rate is CCK or OFDM */
#define RAL_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

#define RAL_ACK_SIZE	14	/* 10 + 4(FCS) */
#define RAL_CTS_SIZE	14	/* 10 + 4(FCS) */
#define RAL_SIFS	10

/*
 * Return the expected ack rate for a frame transmitted at rate `rate'.
 * XXX: this should depend on the destination node basic rate set.
 */
static int
ral_ack_rate(int rate)
{
	switch (rate) {
	/* CCK rates */
	case 2:
		return 2;
	case 4:
	case 11:
	case 22:
		return 4;

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
ral_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;
	int ceil, dbps;

	if (RAL_RATE_IS_OFDM(rate)) {
		/*
		 * OFDM TXTIME calculation.
		 * From IEEE Std 802.11a-1999, pp. 37.
		 */
		dbps = rate * 2; /* data bits per OFDM symbol */

		ceil = (16 + 8 * len + 6) / dbps;
		if ((16 + 8 * len + 6) % dbps != 0)
			ceil++;

		txtime = 16 + 4 + 4 * ceil + 6;
	} else {
		/*
		 * High Rate TXTIME calculation.
		 * From IEEE Std 802.11b-1999, pp. 28.
		 */
		ceil = (8 * len * 2) / rate;
		if ((8 * len * 2) % rate != 0)
			ceil++;

		if (rate != 2 && (flags & IEEE80211_F_SHPREAMBLE))
			txtime =  72 + 24 + ceil;
		else
			txtime = 144 + 48 + ceil;
	}

	return txtime;
}

static uint8_t
ral_plcp_signal(int rate)
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
ral_setup_tx_desc(struct ral_softc *sc, struct ral_tx_desc *desc,
    uint32_t flags, int len, int rate, int encrypt, bus_addr_t physaddr)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t plcp_length;
	int remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(len << 16);
	desc->flags |= encrypt ? htole32(RAL_TX_CIPHER_BUSY) :
	    htole32(RAL_TX_BUSY | RAL_TX_VALID);
	if (RAL_RATE_IS_OFDM(rate))
		desc->flags |= htole32(RAL_TX_OFDM);

	desc->physaddr = htole32(physaddr);
	desc->wme = htole16(RAL_LOGCWMAX(8) | RAL_LOGCWMIN(3) | RAL_AIFSN(2));

	/*
	 * Fill PLCP fields.
	 */
	desc->plcp_service = 4;

	len += 4; /* account for FCS */
	if (RAL_RATE_IS_OFDM(rate)) {
		/*
		 * PLCP length field (LENGTH).
		 * From IEEE Std 802.11a-1999, pp. 14.
		 */
		plcp_length = len & 0xfff;
		desc->plcp_length = htole16((plcp_length >> 6) << 8 |
		    (plcp_length & 0x3f));
	} else {
		/*
		 * Long PLCP LENGTH field.
		 * From IEEE Std 802.11b-1999, pp. 16.
		 */
		plcp_length = (8 * len * 2) / rate;
		remainder = (8 * len * 2) % rate;
		if (remainder != 0) {
			if (rate == 22 && (rate - remainder) / 16 != 0)
				desc->plcp_service |= RAL_PLCP_LENGEXT;
			plcp_length++;
		}
		desc->plcp_length = htole16(plcp_length);
	}

	desc->plcp_signal = ral_plcp_signal(rate);
	if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		desc->plcp_signal |= 0x08;
}

static int
ral_tx_bcn(struct ral_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	bus_dma_segment_t segs[RAL_MAX_SCATTER];
	int nsegs, rate, error;

	desc = &sc->bcnq.desc[sc->bcnq.cur];
	data = &sc->bcnq.data[sc->bcnq.cur];

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 4;

	error = bus_dmamap_load_mbuf_sg(sc->bcnq.data_dmat, data->map, m0,
	    segs, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (sc->sc_drvbpf != NULL) {
		struct ral_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	ral_setup_tx_desc(sc, desc, RAL_TX_IFS_NEWBACKOFF | RAL_TX_TIMESTAMP,
	    m0->m_pkthdr.len, rate, 0, segs->ds_addr);

	DPRINTFN(10, ("sending beacon frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->bcnq.cur, rate));

	bus_dmamap_sync(sc->bcnq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->bcnq.desc_dmat, sc->bcnq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	sc->bcnq.cur = (sc->bcnq.cur + 1) % RAL_BEACON_RING_COUNT;

	return 0;
}

static int
ral_tx_mgt(struct ral_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	struct ieee80211_frame *wh;
	bus_dma_segment_t segs[RAL_MAX_SCATTER];
	uint16_t dur;
	uint32_t flags = 0;
	int nsegs, rate, error;

	desc = &sc->prioq.desc[sc->prioq.cur];
	data = &sc->prioq.data[sc->prioq.cur];

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 4;

	error = bus_dmamap_load_mbuf_sg(sc->prioq.data_dmat, data->map, m0,
	    segs, &nsegs, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (sc->sc_drvbpf != NULL) {
		struct ral_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;

		dur = ral_txtime(RAL_ACK_SIZE, rate, ic->ic_flags) + RAL_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT &&
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= RAL_TX_TIMESTAMP;
	}

	ral_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate, 0,
	    segs->ds_addr);

	bus_dmamap_sync(sc->prioq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->prioq.desc_dmat, sc->prioq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending mgt frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->prioq.cur, rate));

	/* kick prio */
	sc->prioq.queued++;
	sc->prioq.cur = (sc->prioq.cur + 1) % RAL_PRIO_RING_COUNT;
	RAL_WRITE(sc, RAL_TXCSR0, RAL_KICK_PRIO);

	return 0;
}

/*
 * Build a RTS control frame.
 */
static struct mbuf *
ral_get_rts(struct ral_softc *sc, struct ieee80211_frame *wh, uint16_t dur)
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
ral_tx_data(struct ral_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ral_tx_desc *desc;
	struct ral_tx_data *data;
	struct ral_node *rn;
	struct ieee80211_rateset *rs;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct mbuf *mnew;
	bus_dma_segment_t segs[RAL_MAX_SCATTER];
	uint16_t dur;
	uint32_t flags = 0;
	int nsegs, rate, error;

	wh = mtod(m0, struct ieee80211_frame *);

	if (ic->ic_fixed_rate != -1) {
		rs = &ic->ic_sup_rates[ic->ic_curmode];
		rate = rs->rs_rates[ic->ic_fixed_rate];
	} else {
		rs = &ni->ni_rates;
		rn = (struct ral_node *)ni;
		ni->ni_txrate = ral_rssadapt_choose(&rn->rssadapt, rs,
		    wh, m0->m_pkthdr.len, NULL, 0);
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

		rtsrate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 4;
		ackrate = ral_ack_rate(rate);

		dur = ral_txtime(m0->m_pkthdr.len + 4, rate, ic->ic_flags) +
		      ral_txtime(RAL_CTS_SIZE, rtsrate, ic->ic_flags) +
		      ral_txtime(RAL_ACK_SIZE, ackrate, ic->ic_flags) +
		      3 * RAL_SIFS;

		m = ral_get_rts(sc, wh, dur);

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

		ral_setup_tx_desc(sc, desc, RAL_TX_ACK | RAL_TX_MORE_FRAG,
		    m->m_pkthdr.len, rtsrate, 1, segs->ds_addr);

		bus_dmamap_sync(sc->txq.data_dmat, data->map,
		    BUS_DMASYNC_PREWRITE);

		sc->txq.queued++;
		sc->txq.cur_encrypt =
		    (sc->txq.cur_encrypt + 1) % RAL_TX_RING_COUNT;

		/*
		 * IEEE Std 802.11-1999: when an RTS/CTS exchange is used, the
		 * asynchronous data frame shall be transmitted after the CTS
		 * frame and a SIFS period.
		 */
		flags |= RAL_TX_LONG_RETRY | RAL_TX_IFS_SIFS;
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

	if (sc->sc_drvbpf != NULL) {
		struct ral_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	/* remember link conditions for rate adaptation algorithm */
	if (ic->ic_fixed_rate == -1) {
		data->id.id_len = m0->m_pkthdr.len;
		data->id.id_rateidx = ni->ni_txrate;
		data->id.id_node = ni;
		data->id.id_rssi = ni->ni_rssi;
	} else
		data->id.id_node = NULL;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;

		dur = ral_txtime(RAL_ACK_SIZE, ral_ack_rate(rate),
		    ic->ic_flags) + RAL_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

	ral_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate, 1,
	    segs->ds_addr);

	bus_dmamap_sync(sc->txq.data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->txq.desc_dmat, sc->txq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending data frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->txq.cur_encrypt, rate));

	/* kick encrypt */
	sc->txq.queued++;
	sc->txq.cur_encrypt = (sc->txq.cur_encrypt + 1) % RAL_TX_RING_COUNT;
	RAL_WRITE(sc, RAL_SECCSR1, RAL_KICK_ENCRYPT);

	return 0;
}

static void
ral_start(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni;

	RAL_LOCK(sc);

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->prioq.queued >= RAL_PRIO_RING_COUNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;

			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0);

			if (ral_tx_mgt(sc, m0, ni) != 0)
				break;

		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DRV_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->txq.queued >= RAL_TX_RING_COUNT - 1) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}

			if (m0->m_len < sizeof (struct ether_header) &&
			    !(m0 = m_pullup(m0, sizeof (struct ether_header))))
				continue;

			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m0);
				continue;
			}
			BPF_MTAP(ifp, m0);

			m0 = ieee80211_encap(ic, m0, ni);
			if (m0 == NULL) {
				ieee80211_free_node(ni);
				continue;
			}

			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0);

			if (ral_tx_data(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}

	RAL_UNLOCK(sc);
}

static void
ral_watchdog(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	RAL_LOCK(sc);

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			ral_init(sc);
			ifp->if_oerrors++;
			RAL_UNLOCK(sc);
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ic);

	RAL_UNLOCK(sc);
}

/*
 * This function allows for fast channel switching in monitor mode (used by
 * net-mgmt/kismet). In IBSS mode, we must explicitly reset the interface to
 * generate a new beacon frame.
 */
static int
ral_reset(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		return ENETRESET;

	ral_set_chan(sc, ic->ic_ibss_chan);

	return 0;
}

static int
ral_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;

	RAL_LOCK(sc);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				ral_update_promisc(sc);
			else
				ral_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ral_stop(sc);
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			ral_init(sc);
		error = 0;
	}

	RAL_UNLOCK(sc);

	return error;
}

static void
ral_bbp_write(struct ral_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RAL_BBPCSR) & RAL_BBP_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "could not write to BBP\n");
		return;
	}

	tmp = RAL_BBP_WRITE | RAL_BBP_BUSY | reg << 8 | val;
	RAL_WRITE(sc, RAL_BBPCSR, tmp);

	DPRINTFN(15, ("BBP R%u <- 0x%02x\n", reg, val));
}

static uint8_t
ral_bbp_read(struct ral_softc *sc, uint8_t reg)
{
	uint32_t val;
	int ntries;

	val = RAL_BBP_BUSY | reg << 8;
	RAL_WRITE(sc, RAL_BBPCSR, val);

	for (ntries = 0; ntries < 100; ntries++) {
		val = RAL_READ(sc, RAL_BBPCSR);
		if (!(val & RAL_BBP_BUSY))
			return val & 0xff;
		DELAY(1);
	}

	device_printf(sc->sc_dev, "could not read from BBP\n");
	return 0;
}

static void
ral_rf_write(struct ral_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RAL_RFCSR) & RAL_RF_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "could not write to RF\n");
		return;
	}

	tmp = RAL_RF_BUSY | RAL_RF_20BIT | (val & 0xfffff) << 2 | (reg & 0x3);
	RAL_WRITE(sc, RAL_RFCSR, tmp);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 0x3, val & 0xfffff));
}

static void
ral_set_chan(struct ral_softc *sc, struct ieee80211_channel *c)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
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

	DPRINTFN(2, ("setting channel to %u, txpower to %u\n", chan, power));

	switch (sc->rf_rev) {
	case RAL_RF_2522:
		ral_rf_write(sc, RAL_RF1, 0x00814);
		ral_rf_write(sc, RAL_RF2, ral_rf2522_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		break;

	case RAL_RF_2523:
		ral_rf_write(sc, RAL_RF1, 0x08804);
		ral_rf_write(sc, RAL_RF2, ral_rf2523_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x38044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2524:
		ral_rf_write(sc, RAL_RF1, 0x0c808);
		ral_rf_write(sc, RAL_RF2, ral_rf2524_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525:
		ral_rf_write(sc, RAL_RF1, 0x08808);
		ral_rf_write(sc, RAL_RF2, ral_rf2525_hi_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);

		ral_rf_write(sc, RAL_RF1, 0x08808);
		ral_rf_write(sc, RAL_RF2, ral_rf2525_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525E:
		ral_rf_write(sc, RAL_RF1, 0x08808);
		ral_rf_write(sc, RAL_RF2, ral_rf2525e_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00286 : 0x00282);
		break;

	case RAL_RF_2526:
		ral_rf_write(sc, RAL_RF2, ral_rf2526_hi_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		ral_rf_write(sc, RAL_RF1, 0x08804);

		ral_rf_write(sc, RAL_RF2, ral_rf2526_r2[chan - 1]);
		ral_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ral_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		break;

	/* dual-band RF */
	case RAL_RF_5222:
		for (i = 0; i < N(ral_rf5222); i++)
			if (ral_rf5222[i].chan == chan)
				break;

		if (i < N(ral_rf5222)) {
			ral_rf_write(sc, RAL_RF1, ral_rf5222[i].r1);
			ral_rf_write(sc, RAL_RF2, ral_rf5222[i].r2);
			ral_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
			ral_rf_write(sc, RAL_RF4, ral_rf5222[i].r4);
		}
		break;
	}

	if (ic->ic_state != IEEE80211_S_SCAN) {
		/* set Japan filter bit for channel 14 */
		tmp = ral_bbp_read(sc, 70);

		tmp &= ~RAL_JAPAN_FILTER;
		if (chan == 14)
			tmp |= RAL_JAPAN_FILTER;

		ral_bbp_write(sc, 70, tmp);

		/* clear CRC errors */
		RAL_READ(sc, RAL_CNT0);
	}
#undef N
}

#if 0
/*
 * Disable RF auto-tuning.
 */
static void
ral_disable_rf_tune(struct ral_softc *sc)
{
	uint32_t tmp;

	if (sc->rf_rev != RAL_RF_2523) {
		tmp = sc->rf_regs[RAL_RF1] & ~RAL_RF1_AUTOTUNE;
		ral_rf_write(sc, RAL_RF1, tmp);
	}

	tmp = sc->rf_regs[RAL_RF3] & ~RAL_RF3_AUTOTUNE;
	ral_rf_write(sc, RAL_RF3, tmp);

	DPRINTFN(2, ("disabling RF autotune\n"));
}
#endif

/*
 * Refer to IEEE Std 802.11-1999 pp. 123 for more information on TSF
 * synchronization.
 */
static void
ral_enable_tsf_sync(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t logcwmin, preload;
	uint32_t tmp;

	/* first, disable TSF synchronization */
	RAL_WRITE(sc, RAL_CSR14, 0);

	tmp = 16 * ic->ic_bss->ni_intval;
	RAL_WRITE(sc, RAL_CSR12, tmp);

	RAL_WRITE(sc, RAL_CSR13, 0);

	logcwmin = 5;
	preload = (ic->ic_opmode == IEEE80211_M_STA) ? 384 : 1024;
	tmp = logcwmin << 16 | preload;
	RAL_WRITE(sc, RAL_BCNOCSR, tmp);

	/* finally, enable TSF synchronization */
	tmp = RAL_ENABLE_TSF | RAL_ENABLE_TBCN;
	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= RAL_ENABLE_TSF_SYNC(1);
	else
		tmp |= RAL_ENABLE_TSF_SYNC(2) | RAL_ENABLE_BEACON_GENERATOR;
	RAL_WRITE(sc, RAL_CSR14, tmp);

	DPRINTF(("enabling TSF synchronization\n"));
}

static void
ral_update_plcp(struct ral_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* no short preamble for 1Mbps */
	RAL_WRITE(sc, RAL_PLCP1MCSR, 0x00700400);

	if (!(ic->ic_flags & IEEE80211_F_SHPREAMBLE)) {
		/* values taken from the reference driver */
		RAL_WRITE(sc, RAL_PLCP2MCSR,   0x00380401);
		RAL_WRITE(sc, RAL_PLCP5p5MCSR, 0x00150402);
		RAL_WRITE(sc, RAL_PLCP11MCSR,  0x000b8403);
	} else {
		/* same values as above or'ed 0x8 */
		RAL_WRITE(sc, RAL_PLCP2MCSR,   0x00380409);
		RAL_WRITE(sc, RAL_PLCP5p5MCSR, 0x0015040a);
		RAL_WRITE(sc, RAL_PLCP11MCSR,  0x000b840b);
	}

	DPRINTF(("updating PLCP for %s preamble\n",
	    (ic->ic_flags & IEEE80211_F_SHPREAMBLE) ? "short" : "long"));
}

/*
 * This function can be called by ieee80211_set_shortslottime(). Refer to
 * IEEE Std 802.11-1999 pp. 85 to know how these values are computed.
 */
static void
ral_update_slot(struct ifnet *ifp)
{
	struct ral_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t slottime;
	uint16_t sifs, pifs, difs, eifs;
	uint32_t tmp;

	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	/* update the MAC slot boundaries */
	sifs = RAL_SIFS;
	pifs = sifs + slottime;
	difs = sifs + 2 * slottime;
	eifs = sifs + ral_txtime(RAL_ACK_SIZE,
	    (ic->ic_curmode == IEEE80211_MODE_11A) ? 12 : 2, 0) + difs;

	tmp = RAL_READ(sc, RAL_CSR11);
	tmp = (tmp & ~0x1f00) | slottime << 8;
	RAL_WRITE(sc, RAL_CSR11, tmp);

	tmp = pifs << 16 | sifs;
	RAL_WRITE(sc, RAL_CSR18, tmp);

	tmp = eifs << 16 | difs;
	RAL_WRITE(sc, RAL_CSR19, tmp);

	DPRINTF(("setting slottime to %uus\n", slottime));
}

static void
ral_update_led(struct ral_softc *sc, int led1, int led2)
{
	uint32_t tmp;

	/* set ON period to 70ms and OFF period to 30ms */
	tmp = led1 << 16 | led2 << 17 | 70 << 8 | 30;
	RAL_WRITE(sc, RAL_LEDCSR, tmp);
}

static void
ral_set_bssid(struct ral_softc *sc, uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24;
	RAL_WRITE(sc, RAL_CSR5, tmp);

	tmp = bssid[4] | bssid[5] << 8;
	RAL_WRITE(sc, RAL_CSR6, tmp);

	DPRINTF(("setting BSSID to %6D\n", bssid, ":"));
}

static void
ral_set_macaddr(struct ral_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
	RAL_WRITE(sc, RAL_CSR3, tmp);

	tmp = addr[4] | addr[5] << 8;
	RAL_WRITE(sc, RAL_CSR4, tmp);

	DPRINTF(("setting MAC address to %6D\n", addr, ":"));
}

static void
ral_get_macaddr(struct ral_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = RAL_READ(sc, RAL_CSR3);
	addr[0] = tmp & 0xff;
	addr[1] = (tmp >>  8) & 0xff;
	addr[2] = (tmp >> 16) & 0xff;
	addr[3] = (tmp >> 24);

	tmp = RAL_READ(sc, RAL_CSR4);
	addr[4] = tmp & 0xff;
	addr[5] = (tmp >> 8) & 0xff;
}

static void
ral_update_promisc(struct ral_softc *sc)
{
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t tmp;

	tmp = RAL_READ(sc, RAL_RXCSR0);

	tmp &= ~RAL_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RAL_DROP_NOT_TO_ME;

	RAL_WRITE(sc, RAL_RXCSR0, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

static const char *
ral_get_rf(int rev)
{
	switch (rev) {
	case RAL_RF_2522:	return "RT2522";
	case RAL_RF_2523:	return "RT2523";
	case RAL_RF_2524:	return "RT2524";
	case RAL_RF_2525:	return "RT2525";
	case RAL_RF_2525E:	return "RT2525e";
	case RAL_RF_2526:	return "RT2526";
	case RAL_RF_5222:	return "RT5222";
	default:		return "unknown";
	}
}

static void
ral_read_eeprom(struct ral_softc *sc)
{
	uint16_t val;
	int i;

	val = ral_eeprom_read(sc, RAL_EEPROM_CONFIG0);
	sc->rf_rev =   (val >> 11) & 0x7;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->led_mode = (val >> 6)  & 0x7;
	sc->rx_ant =   (val >> 4)  & 0x3;
	sc->tx_ant =   (val >> 2)  & 0x3;
	sc->nb_ant =   val & 0x3;

	/* read default values for BBP registers */
	for (i = 0; i < 16; i++) {
		val = ral_eeprom_read(sc, RAL_EEPROM_BBP_BASE + i);
		sc->bbp_prom[i].reg = val >> 8;
		sc->bbp_prom[i].val = val & 0xff;
	}

	/* read Tx power for all b/g channels */
	for (i = 0; i < 14 / 2; i++) {
		val = ral_eeprom_read(sc, RAL_EEPROM_TXPOWER + i);
		sc->txpow[i * 2] = val >> 8;
		sc->txpow[i * 2 + 1] = val & 0xff;
	}
}

static int
ral_bbp_init(struct ral_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		if (ral_bbp_read(sc, RAL_BBP_VERSION) != 0)
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for BBP\n");
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(ral_def_bbp); i++)
		ral_bbp_write(sc, ral_def_bbp[i].reg, ral_def_bbp[i].val);

#if 0
	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0xff)
			continue;
		ral_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}
#endif

	return 0;
#undef N
}

static void
ral_set_txantenna(struct ral_softc *sc, int antenna)
{
	uint32_t tmp;
	uint8_t tx;

	tx = ral_bbp_read(sc, RAL_BBP_TX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		tx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		tx |= RAL_BBP_ANTB;
	else
		tx |= RAL_BBP_DIVERSITY;

	/* need to force I/Q flip for RF 2525e, 2526 and 5222 */
	if (sc->rf_rev == RAL_RF_2525E || sc->rf_rev == RAL_RF_2526 ||
	    sc->rf_rev == RAL_RF_5222)
		tx |= RAL_BBP_FLIPIQ;

	ral_bbp_write(sc, RAL_BBP_TX, tx);

	/* update values for CCK and OFDM in BBPCSR1 */
	tmp = RAL_READ(sc, RAL_BBPCSR1) & ~0x00070007;
	tmp |= (tx & 0x7) << 16 | (tx & 0x7);
	RAL_WRITE(sc, RAL_BBPCSR1, tmp);
}

static void
ral_set_rxantenna(struct ral_softc *sc, int antenna)
{
	uint8_t rx;

	rx = ral_bbp_read(sc, RAL_BBP_RX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		rx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		rx |= RAL_BBP_ANTB;
	else
		rx |= RAL_BBP_DIVERSITY;

	/* need to force no I/Q flip for RF 2525e and 2526 */
	if (sc->rf_rev == RAL_RF_2525E || sc->rf_rev == RAL_RF_2526)
		rx &= ~RAL_BBP_FLIPIQ;

	ral_bbp_write(sc, RAL_BBP_RX, rx);
}

static void
ral_init(void *priv)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct ral_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	uint32_t tmp;
	int i;

	ral_stop(sc);

	/* setup tx rings */
	tmp = RAL_PRIO_RING_COUNT << 24 |
	      RAL_ATIM_RING_COUNT << 16 |
	      RAL_TX_RING_COUNT   <<  8 |
	      RAL_TX_DESC_SIZE;

	/* rings _must_ be initialized in this _exact_ order! */
	RAL_WRITE(sc, RAL_TXCSR2, tmp);
	RAL_WRITE(sc, RAL_TXCSR3, sc->txq.physaddr);
	RAL_WRITE(sc, RAL_TXCSR5, sc->prioq.physaddr);
	RAL_WRITE(sc, RAL_TXCSR4, sc->atimq.physaddr);
	RAL_WRITE(sc, RAL_TXCSR6, sc->bcnq.physaddr);

	/* setup rx ring */
	tmp = RAL_RX_RING_COUNT << 8 | RAL_RX_DESC_SIZE;

	RAL_WRITE(sc, RAL_RXCSR1, tmp);
	RAL_WRITE(sc, RAL_RXCSR2, sc->rxq.physaddr);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(ral_def_mac); i++)
		RAL_WRITE(sc, ral_def_mac[i].reg, ral_def_mac[i].val);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	ral_set_macaddr(sc, ic->ic_myaddr);

	/* set supported basic rates (1, 2, 6, 12, 24) */
	RAL_WRITE(sc, RAL_ARSP_PLCP_1, 0x153);

	ral_set_txantenna(sc, sc->tx_ant);
	ral_set_rxantenna(sc, sc->rx_ant);
	ral_update_slot(ifp);
	ral_update_plcp(sc);
	ral_update_led(sc, 0, 0);

	RAL_WRITE(sc, RAL_CSR1, RAL_RESET_ASIC);
	RAL_WRITE(sc, RAL_CSR1, RAL_HOST_READY);

	if (ral_bbp_init(sc) != 0) {
		ral_stop(sc);
		return;
	}

	/* set default BSS channel */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	ral_set_chan(sc, ic->ic_bss->ni_chan);

	/* kick Rx */
	tmp = RAL_DROP_PHY_ERROR | RAL_DROP_CRC_ERROR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RAL_DROP_CTL | RAL_DROP_VERSION_ERROR;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RAL_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RAL_DROP_NOT_TO_ME;
	}
	RAL_WRITE(sc, RAL_RXCSR0, tmp);

	/* clear old FCS and Rx FIFO errors */
	RAL_READ(sc, RAL_CNT0);
	RAL_READ(sc, RAL_CNT4);

	/* clear any pending interrupts */
	RAL_WRITE(sc, RAL_CSR7, 0xffffffff);

	/* enable interrupts */
	RAL_WRITE(sc, RAL_CSR8, RAL_INTR_MASK);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
#undef N
}

void
ral_stop(void *priv)
{
	struct ral_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* abort Tx */
	RAL_WRITE(sc, RAL_TXCSR0, RAL_ABORT_TX);

	/* disable Rx */
	RAL_WRITE(sc, RAL_RXCSR0, RAL_DISABLE_RX);

	/* reset ASIC (imply reset BBP) */
	RAL_WRITE(sc, RAL_CSR1, RAL_RESET_ASIC);
	RAL_WRITE(sc, RAL_CSR1, 0);

	/* disable interrupts */
	RAL_WRITE(sc, RAL_CSR8, 0xffffffff);

	/* reset Tx and Rx rings */
	ral_reset_tx_ring(sc, &sc->txq);
	ral_reset_tx_ring(sc, &sc->atimq);
	ral_reset_tx_ring(sc, &sc->prioq);
	ral_reset_tx_ring(sc, &sc->bcnq);
	ral_reset_rx_ring(sc, &sc->rxq);
}
