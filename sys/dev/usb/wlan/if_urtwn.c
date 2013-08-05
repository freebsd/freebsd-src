/*	$OpenBSD: if_urtwn.c,v 1.16 2011/02/10 17:26:40 jakemsr Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
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
 * Driver for Realtek RTL8188CE-VAU/RTL8188CUS/RTL8188RU/RTL8192CU.
 */

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/kdb.h>

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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include "usbdevs.h"

#define USB_DEBUG_VAR urtwn_debug
#include <dev/usb/usb_debug.h>

#include <dev/usb/wlan/if_urtwnreg.h>

#ifdef USB_DEBUG
static int urtwn_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, urtwn, CTLFLAG_RW, 0, "USB urtwn");
SYSCTL_INT(_hw_usb_urtwn, OID_AUTO, debug, CTLFLAG_RW, &urtwn_debug, 0,
    "Debug level");
#endif

#define	URTWN_RSSI(r)  (r) - 110
#define	IEEE80211_HAS_ADDR4(wh)	\
	(((wh)->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)

/* various supported device vendors/products */
static const STRUCT_USB_HOST_ID urtwn_devs[] = {
#define URTWN_DEV(v,p)  { USB_VP(USB_VENDOR_##v, USB_PRODUCT_##v##_##p) }
	URTWN_DEV(ABOCOM,	RTL8188CU_1),
	URTWN_DEV(ABOCOM,	RTL8188CU_2),
	URTWN_DEV(ABOCOM,	RTL8192CU),
	URTWN_DEV(ASUS,		RTL8192CU),
	URTWN_DEV(AZUREWAVE,	RTL8188CE_1),
	URTWN_DEV(AZUREWAVE,	RTL8188CE_2),
	URTWN_DEV(AZUREWAVE,	RTL8188CU),
	URTWN_DEV(BELKIN,	F7D2102),
	URTWN_DEV(BELKIN,	RTL8188CU),
	URTWN_DEV(BELKIN,	RTL8192CU),
	URTWN_DEV(CHICONY,	RTL8188CUS_1),
	URTWN_DEV(CHICONY,	RTL8188CUS_2),
	URTWN_DEV(CHICONY,	RTL8188CUS_3),
	URTWN_DEV(CHICONY,	RTL8188CUS_4),
	URTWN_DEV(CHICONY,	RTL8188CUS_5),
	URTWN_DEV(COREGA,	RTL8192CU),
	URTWN_DEV(DLINK,	RTL8188CU),
	URTWN_DEV(DLINK,	RTL8192CU_1),
	URTWN_DEV(DLINK,	RTL8192CU_2),
	URTWN_DEV(DLINK,	RTL8192CU_3),
	URTWN_DEV(DLINK,	DWA131B),
	URTWN_DEV(EDIMAX,	EW7811UN),
	URTWN_DEV(EDIMAX,	RTL8192CU),
	URTWN_DEV(FEIXUN,	RTL8188CU),
	URTWN_DEV(FEIXUN,	RTL8192CU),
	URTWN_DEV(GUILLEMOT,	HWNUP150),
	URTWN_DEV(HAWKING,	RTL8192CU),
	URTWN_DEV(HP3,		RTL8188CU),
	URTWN_DEV(NETGEAR,	WNA1000M),
	URTWN_DEV(NETGEAR,	RTL8192CU),
	URTWN_DEV(NETGEAR4,	RTL8188CU),
	URTWN_DEV(NOVATECH,	RTL8188CU),
	URTWN_DEV(PLANEX2,	RTL8188CU_1),
	URTWN_DEV(PLANEX2,	RTL8188CU_2),
	URTWN_DEV(PLANEX2,	RTL8188CU_3),
	URTWN_DEV(PLANEX2,	RTL8188CU_4),
	URTWN_DEV(PLANEX2,	RTL8188CUS),
	URTWN_DEV(PLANEX2,	RTL8192CU),
	URTWN_DEV(REALTEK,	RTL8188CE_0),
	URTWN_DEV(REALTEK,	RTL8188CE_1),
	URTWN_DEV(REALTEK,	RTL8188CTV),
	URTWN_DEV(REALTEK,	RTL8188CU_0),
	URTWN_DEV(REALTEK,	RTL8188CU_1),
	URTWN_DEV(REALTEK,	RTL8188CU_2),
	URTWN_DEV(REALTEK,	RTL8188CU_COMBO),
	URTWN_DEV(REALTEK,	RTL8188CUS),
	URTWN_DEV(REALTEK,	RTL8188RU_1),
	URTWN_DEV(REALTEK,	RTL8188RU_2),
	URTWN_DEV(REALTEK,	RTL8191CU),
	URTWN_DEV(REALTEK,	RTL8192CE),
	URTWN_DEV(REALTEK,	RTL8192CU),
	URTWN_DEV(SITECOMEU,	RTL8188CU_1),
	URTWN_DEV(SITECOMEU,	RTL8188CU_2),
	URTWN_DEV(SITECOMEU,	RTL8192CU),
	URTWN_DEV(TRENDNET,	RTL8188CU),
	URTWN_DEV(TRENDNET,	RTL8192CU),
	URTWN_DEV(ZYXEL,	RTL8192CU),
#undef URTWN_DEV
};

static device_probe_t	urtwn_match;
static device_attach_t	urtwn_attach;
static device_detach_t	urtwn_detach;

static usb_callback_t   urtwn_bulk_tx_callback;
static usb_callback_t	urtwn_bulk_rx_callback;

static usb_error_t	urtwn_do_request(struct urtwn_softc *sc,
			    struct usb_device_request *req, void *data);
static struct ieee80211vap *urtwn_vap_create(struct ieee80211com *,
		    const char [IFNAMSIZ], int, enum ieee80211_opmode, int,
                    const uint8_t [IEEE80211_ADDR_LEN],
                    const uint8_t [IEEE80211_ADDR_LEN]);
static void		urtwn_vap_delete(struct ieee80211vap *);
static struct mbuf *	urtwn_rx_frame(struct urtwn_softc *, uint8_t *, int, 
			    int *);
static struct mbuf *	urtwn_rxeof(struct usb_xfer *, struct urtwn_data *, 
			    int *, int8_t *);
static void		urtwn_txeof(struct usb_xfer *, struct urtwn_data *);
static int		urtwn_alloc_list(struct urtwn_softc *, 
			    struct urtwn_data[], int, int);
static int		urtwn_alloc_rx_list(struct urtwn_softc *);
static int		urtwn_alloc_tx_list(struct urtwn_softc *);
static void		urtwn_free_tx_list(struct urtwn_softc *);
static void		urtwn_free_rx_list(struct urtwn_softc *);
static void		urtwn_free_list(struct urtwn_softc *,
			    struct urtwn_data data[], int);
static struct urtwn_data *	_urtwn_getbuf(struct urtwn_softc *);
static struct urtwn_data *	urtwn_getbuf(struct urtwn_softc *);
static int		urtwn_write_region_1(struct urtwn_softc *, uint16_t, 
			    uint8_t *, int);
static void		urtwn_write_1(struct urtwn_softc *, uint16_t, uint8_t);
static void		urtwn_write_2(struct urtwn_softc *, uint16_t, uint16_t);
static void		urtwn_write_4(struct urtwn_softc *, uint16_t, uint32_t);
static int		urtwn_read_region_1(struct urtwn_softc *, uint16_t, 
			    uint8_t *, int);
static uint8_t		urtwn_read_1(struct urtwn_softc *, uint16_t);
static uint16_t		urtwn_read_2(struct urtwn_softc *, uint16_t);
static uint32_t		urtwn_read_4(struct urtwn_softc *, uint16_t);
static int		urtwn_fw_cmd(struct urtwn_softc *, uint8_t, 
			    const void *, int);
static void		urtwn_rf_write(struct urtwn_softc *, int, uint8_t, 
			    uint32_t);
static uint32_t		urtwn_rf_read(struct urtwn_softc *, int, uint8_t);
static int		urtwn_llt_write(struct urtwn_softc *, uint32_t, 
			    uint32_t);
static uint8_t		urtwn_efuse_read_1(struct urtwn_softc *, uint16_t);
static void		urtwn_efuse_read(struct urtwn_softc *);
static int		urtwn_read_chipid(struct urtwn_softc *);
static void		urtwn_read_rom(struct urtwn_softc *);
static int		urtwn_ra_init(struct urtwn_softc *);
static void		urtwn_tsf_sync_enable(struct urtwn_softc *);
static void		urtwn_set_led(struct urtwn_softc *, int, int);
static int		urtwn_newstate(struct ieee80211vap *, 
			    enum ieee80211_state, int);
static void		urtwn_watchdog(void *);
static void		urtwn_update_avgrssi(struct urtwn_softc *, int, int8_t);
static int8_t		urtwn_get_rssi(struct urtwn_softc *, int, void *);
static int		urtwn_tx_start(struct urtwn_softc *,
			    struct ieee80211_node *, struct mbuf *,
			    struct urtwn_data *);
static void		urtwn_start(struct ifnet *);
static int		urtwn_ioctl(struct ifnet *, u_long, caddr_t);
static int		urtwn_power_on(struct urtwn_softc *);
static int		urtwn_llt_init(struct urtwn_softc *);
static void		urtwn_fw_reset(struct urtwn_softc *);
static int		urtwn_fw_loadpage(struct urtwn_softc *, int, 
			    const uint8_t *, int);
static int		urtwn_load_firmware(struct urtwn_softc *);
static int		urtwn_dma_init(struct urtwn_softc *);
static void		urtwn_mac_init(struct urtwn_softc *);
static void		urtwn_bb_init(struct urtwn_softc *);
static void		urtwn_rf_init(struct urtwn_softc *);
static void		urtwn_cam_init(struct urtwn_softc *);
static void		urtwn_pa_bias_init(struct urtwn_softc *);
static void		urtwn_rxfilter_init(struct urtwn_softc *);
static void		urtwn_edca_init(struct urtwn_softc *);
static void		urtwn_write_txpower(struct urtwn_softc *, int, 
			    uint16_t[]);
static void		urtwn_get_txpower(struct urtwn_softc *, int,
		      	    struct ieee80211_channel *, 
			    struct ieee80211_channel *, uint16_t[]);
static void		urtwn_set_txpower(struct urtwn_softc *,
		    	    struct ieee80211_channel *, 
			    struct ieee80211_channel *);
static void		urtwn_scan_start(struct ieee80211com *);
static void		urtwn_scan_end(struct ieee80211com *);
static void		urtwn_set_channel(struct ieee80211com *);
static void		urtwn_set_chan(struct urtwn_softc *,
		    	    struct ieee80211_channel *, 
			    struct ieee80211_channel *);
static void		urtwn_update_mcast(struct ifnet *);
static void		urtwn_iq_calib(struct urtwn_softc *);
static void		urtwn_lc_calib(struct urtwn_softc *);
static void		urtwn_init(void *);
static void		urtwn_init_locked(void *);
static void		urtwn_stop(struct ifnet *, int);
static void		urtwn_stop_locked(struct ifnet *, int);
static void		urtwn_abort_xfers(struct urtwn_softc *);
static int		urtwn_raw_xmit(struct ieee80211_node *, struct mbuf *,
			    const struct ieee80211_bpf_params *);

/* Aliases. */
#define	urtwn_bb_write	urtwn_write_4
#define urtwn_bb_read	urtwn_read_4

static const struct usb_config urtwn_config[URTWN_N_TRANSFER] = {
	[URTWN_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = URTWN_RXBUFSZ,
		.flags = {
			.pipe_bof = 1,
			.short_xfer_ok = 1
		},
		.callback = urtwn_bulk_rx_callback,
	},
	[URTWN_BULK_TX_BE] = {
		.type = UE_BULK,
		.endpoint = 0x03,
		.direction = UE_DIR_OUT,
		.bufsize = URTWN_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.force_short_xfer = 1
		},
		.callback = urtwn_bulk_tx_callback,
		.timeout = URTWN_TX_TIMEOUT,	/* ms */
	},
	[URTWN_BULK_TX_BK] = {
		.type = UE_BULK,
		.endpoint = 0x03,
		.direction = UE_DIR_OUT,
		.bufsize = URTWN_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.force_short_xfer = 1,
		},
		.callback = urtwn_bulk_tx_callback,
		.timeout = URTWN_TX_TIMEOUT,	/* ms */
	},
	[URTWN_BULK_TX_VI] = {
		.type = UE_BULK,
		.endpoint = 0x02,
		.direction = UE_DIR_OUT,
		.bufsize = URTWN_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.force_short_xfer = 1
		},
		.callback = urtwn_bulk_tx_callback,
		.timeout = URTWN_TX_TIMEOUT,	/* ms */
	},
	[URTWN_BULK_TX_VO] = {
		.type = UE_BULK,
		.endpoint = 0x02,
		.direction = UE_DIR_OUT,
		.bufsize = URTWN_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.force_short_xfer = 1
		},
		.callback = urtwn_bulk_tx_callback,
		.timeout = URTWN_TX_TIMEOUT,	/* ms */
	},
};

static int
urtwn_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != URTWN_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != URTWN_IFACE_INDEX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(urtwn_devs, sizeof(urtwn_devs), uaa));
}

static int
urtwn_attach(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct urtwn_softc *sc = device_get_softc(self);
	struct ifnet *ifp;
	struct ieee80211com *ic;
	uint8_t iface_index, bands;
	int error;

	device_set_usb_desc(self);
	sc->sc_udev = uaa->device;
	sc->sc_dev = self;

	mtx_init(&sc->sc_mtx, device_get_nameunit(self),
	    MTX_NETWORK_LOCK, MTX_DEF);
	callout_init(&sc->sc_watchdog_ch, 0);

	iface_index = URTWN_IFACE_INDEX;
	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    urtwn_config, URTWN_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(self, "could not allocate USB transfers, "
		    "err=%s\n", usbd_errstr(error));
		goto detach;
	}

	URTWN_LOCK(sc);

	error = urtwn_read_chipid(sc);
	if (error) {
		device_printf(sc->sc_dev, "unsupported test chip\n");
		URTWN_UNLOCK(sc);
		goto detach;
	}

	/* Determine number of Tx/Rx chains. */
	if (sc->chip & URTWN_CHIP_92C) {
		sc->ntxchains = (sc->chip & URTWN_CHIP_92C_1T2R) ? 1 : 2;
		sc->nrxchains = 2;
	} else {
		sc->ntxchains = 1;
		sc->nrxchains = 1;
	}
	urtwn_read_rom(sc);

	device_printf(sc->sc_dev, "MAC/BB RTL%s, RF 6052 %dT%dR\n",
	    (sc->chip & URTWN_CHIP_92C) ? "8192CU" :
	    (sc->board_type == R92C_BOARD_TYPE_HIGHPA) ? "8188RU" :
	    (sc->board_type == R92C_BOARD_TYPE_MINICARD) ? "8188CE-VAU" :
	    "8188CUS", sc->ntxchains, sc->nrxchains);

	URTWN_UNLOCK(sc);

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		goto detach;
	}
	ic = ifp->if_l2com;

	ifp->if_softc = sc;
	if_initname(ifp, "urtwn", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = urtwn_init;
	ifp->if_ioctl = urtwn_ioctl;
	ifp->if_start = urtwn_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */

	/* set device capabilities */
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
		| IEEE80211_C_WPA		/* 802.11i */
		;

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, NULL, &bands);

	ieee80211_ifattach(ic, sc->sc_bssid);
	ic->ic_raw_xmit = urtwn_raw_xmit;
	ic->ic_scan_start = urtwn_scan_start;
	ic->ic_scan_end = urtwn_scan_end;
	ic->ic_set_channel = urtwn_set_channel;

	ic->ic_vap_create = urtwn_vap_create;
	ic->ic_vap_delete = urtwn_vap_delete;
	ic->ic_update_mcast = urtwn_update_mcast;

	ieee80211_radiotap_attach(ic, &sc->sc_txtap.wt_ihdr, 
	    sizeof(sc->sc_txtap), URTWN_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
	    URTWN_RX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

detach:
	urtwn_detach(self);
	return (ENXIO);			/* failure */
}

static int
urtwn_detach(device_t self)
{
	struct urtwn_softc *sc = device_get_softc(self);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	
	if (!device_is_attached(self))
		return (0);

	urtwn_stop(ifp, 1);

	callout_drain(&sc->sc_watchdog_ch);

	/* stop all USB transfers */
	usbd_transfer_unsetup(sc->sc_xfer, URTWN_N_TRANSFER);
	ieee80211_ifdetach(ic);

	urtwn_free_tx_list(sc);
	urtwn_free_rx_list(sc);

	if_free(ifp);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
urtwn_free_tx_list(struct urtwn_softc *sc)
{
	urtwn_free_list(sc, sc->sc_tx, URTWN_TX_LIST_COUNT);
}

static void
urtwn_free_rx_list(struct urtwn_softc *sc)
{
	urtwn_free_list(sc, sc->sc_rx, URTWN_RX_LIST_COUNT);
}

static void
urtwn_free_list(struct urtwn_softc *sc, struct urtwn_data data[], int ndata)
{
	int i;

	for (i = 0; i < ndata; i++) {
		struct urtwn_data *dp = &data[i];

		if (dp->buf != NULL) {
			free(dp->buf, M_USBDEV);
			dp->buf = NULL;
		}
		if (dp->ni != NULL) {
			ieee80211_free_node(dp->ni);
			dp->ni = NULL;
		}
	}
}

static usb_error_t
urtwn_do_request(struct urtwn_softc *sc, struct usb_device_request *req,
    void *data)
{
	usb_error_t err;
	int ntries = 10;

	URTWN_ASSERT_LOCKED(sc);

	while (ntries--) {
		err = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx,
		    req, data, 0, NULL, 250 /* ms */);
		if (err == 0)
			break;

		DPRINTFN(1, "Control request failed, %s (retrying)\n",
		    usbd_errstr(err));
		usb_pause_mtx(&sc->sc_mtx, hz / 100);
	}
	return (err);
}

static struct ieee80211vap *
urtwn_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct urtwn_vap *uvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return (NULL);

	uvp = (struct urtwn_vap *) malloc(sizeof(struct urtwn_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (uvp == NULL)
		return (NULL);
	vap = &uvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode, 
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = urtwn_newstate;

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status);
	ic->ic_opmode = opmode;
	return (vap);
}

static void
urtwn_vap_delete(struct ieee80211vap *vap)
{
	struct urtwn_vap *uvp = URTWN_VAP(vap);

	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

static struct mbuf *
urtwn_rx_frame(struct urtwn_softc *sc, uint8_t *buf, int pktlen, int *rssi_p)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_frame *wh;
	struct mbuf *m;
	struct r92c_rx_stat *stat;
	uint32_t rxdw0, rxdw3;
	uint8_t rate;
	int8_t rssi = 0;
	int infosz;

	/*
	 * don't pass packets to the ieee80211 framework if the driver isn't
	 * RUNNING.
	 */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return (NULL);

	stat = (struct r92c_rx_stat *)buf;
	rxdw0 = le32toh(stat->rxdw0);
	rxdw3 = le32toh(stat->rxdw3);

	if (rxdw0 & (R92C_RXDW0_CRCERR | R92C_RXDW0_ICVERR)) {
		/*
		 * This should not happen since we setup our Rx filter
		 * to not receive these frames.
		 */
		ifp->if_ierrors++;
		return (NULL);
	}

	rate = MS(rxdw3, R92C_RXDW3_RATE);
	infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;

	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0 && (rxdw0 & R92C_RXDW0_PHYST)) {
		rssi = urtwn_get_rssi(sc, rate, &stat[1]);
		/* Update our average RSSI. */
		urtwn_update_avgrssi(sc, rate, rssi);
		/*
		 * Convert the RSSI to a range that will be accepted
		 * by net80211.
		 */
		rssi = URTWN_RSSI(rssi);
	}

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		device_printf(sc->sc_dev, "could not create RX mbuf\n");
		return (NULL);
	}

	/* Finalize mbuf. */
	m->m_pkthdr.rcvif = ifp;
	wh = (struct ieee80211_frame *)((uint8_t *)&stat[1] + infosz);
	memcpy(mtod(m, uint8_t *), wh, pktlen);
	m->m_pkthdr.len = m->m_len = pktlen;

	if (ieee80211_radiotap_active(ic)) {
		struct urtwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		/* Map HW rate index to 802.11 rate. */
		if (!(rxdw3 & R92C_RXDW3_HT)) {
			switch (rate) {
			/* CCK. */
			case  0: tap->wr_rate =   2; break;
			case  1: tap->wr_rate =   4; break;
			case  2: tap->wr_rate =  11; break;
			case  3: tap->wr_rate =  22; break;
			/* OFDM. */
			case  4: tap->wr_rate =  12; break;
			case  5: tap->wr_rate =  18; break;
			case  6: tap->wr_rate =  24; break;
			case  7: tap->wr_rate =  36; break;
			case  8: tap->wr_rate =  48; break;
			case  9: tap->wr_rate =  72; break;
			case 10: tap->wr_rate =  96; break;
			case 11: tap->wr_rate = 108; break;
			}
		} else if (rate >= 12) {	/* MCS0~15. */
			/* Bit 7 set means HT MCS instead of rate. */
			tap->wr_rate = 0x80 | (rate - 12);
		}
		tap->wr_dbm_antsignal = rssi;
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
	}

	*rssi_p = rssi;

	return (m);
}

static struct mbuf *
urtwn_rxeof(struct usb_xfer *xfer, struct urtwn_data *data, int *rssi,
    int8_t *nf)
{
	struct urtwn_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct r92c_rx_stat *stat;
	struct mbuf *m, *m0 = NULL, *prevm = NULL;
	uint32_t rxdw0;
	uint8_t *buf;
	int len, totlen, pktlen, infosz, npkts;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	if (len < sizeof(*stat)) {
		ifp->if_ierrors++;
		return (NULL);
	}

	buf = data->buf;
	/* Get the number of encapsulated frames. */
	stat = (struct r92c_rx_stat *)buf;
	npkts = MS(le32toh(stat->rxdw2), R92C_RXDW2_PKTCNT);
	DPRINTFN(6, "Rx %d frames in one chunk\n", npkts);

	/* Process all of them. */
	while (npkts-- > 0) {
		if (len < sizeof(*stat))
			break;
		stat = (struct r92c_rx_stat *)buf;
		rxdw0 = le32toh(stat->rxdw0);

		pktlen = MS(rxdw0, R92C_RXDW0_PKTLEN);
		if (pktlen == 0)
			break;

		infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;

		/* Make sure everything fits in xfer. */
		totlen = sizeof(*stat) + infosz + pktlen;
		if (totlen > len)
			break;

		m = urtwn_rx_frame(sc, buf, pktlen, rssi);
		if (m0 == NULL)
			m0 = m;
		if (prevm == NULL)
			prevm = m;
		else {
			prevm->m_next = m;
			prevm = m;
		}

		/* Next chunk is 128-byte aligned. */
		totlen = (totlen + 127) & ~127;
		buf += totlen;
		len -= totlen;
	}

	return (m0);
}

static void
urtwn_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct urtwn_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m = NULL, *next;
	struct urtwn_data *data;
	int8_t nf;
	int rssi = 1;

	URTWN_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_rx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&sc->sc_rx_active, next);
		m = urtwn_rxeof(xfer, data, &rssi, &nf);
		STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&sc->sc_rx_inactive);
		if (data == NULL) {
			KASSERT(m == NULL, ("mbuf isn't NULL"));
			return;
		}
		STAILQ_REMOVE_HEAD(&sc->sc_rx_inactive, next);
		STAILQ_INSERT_TAIL(&sc->sc_rx_active, data, next);
		usbd_xfer_set_frame_data(xfer, 0, data->buf,
		    usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);

		/*
		 * To avoid LOR we should unlock our private mutex here to call
		 * ieee80211_input() because here is at the end of a USB
		 * callback and safe to unlock.
		 */
		URTWN_UNLOCK(sc);
		while (m != NULL) {
			next = m->m_next;
			m->m_next = NULL;
			wh = mtod(m, struct ieee80211_frame *);
			ni = ieee80211_find_rxnode(ic,
			    (struct ieee80211_frame_min *)wh);
			nf = URTWN_NOISE_FLOOR;
			if (ni != NULL) {
				(void)ieee80211_input(ni, m, rssi, nf);
				ieee80211_free_node(ni);
			} else
				(void)ieee80211_input_all(ic, m, rssi, nf);
			m = next;
		}
		URTWN_LOCK(sc);
		break;
	default:
		/* needs it to the inactive queue due to a error. */
		data = STAILQ_FIRST(&sc->sc_rx_active);
		if (data != NULL) {
			STAILQ_REMOVE_HEAD(&sc->sc_rx_active, next);
			STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
		}
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			ifp->if_ierrors++;
			goto tr_setup;
		}
		break;
	}
}

static void
urtwn_txeof(struct usb_xfer *xfer, struct urtwn_data *data)
{
	struct urtwn_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	URTWN_ASSERT_LOCKED(sc);

	/*
	 * Do any tx complete callback.  Note this must be done before releasing
	 * the node reference.
	 */
	if (data->m) {
		m = data->m;
		if (m->m_flags & M_TXCB) {
			/* XXX status? */
			ieee80211_process_callback(data->ni, m, 0);
		}
		m_freem(m);
		data->m = NULL;
	}
	if (data->ni) {
		ieee80211_free_node(data->ni);
		data->ni = NULL;
	}
	sc->sc_txtimer = 0;
	ifp->if_opackets++;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

static void
urtwn_bulk_tx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct urtwn_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct urtwn_data *data;

	URTWN_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)){
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_tx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&sc->sc_tx_active, next);
		urtwn_txeof(xfer, data);
		STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data, next);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&sc->sc_tx_pending);
		if (data == NULL) {
			DPRINTF("%s: empty pending queue\n", __func__);
			return;
		}
		STAILQ_REMOVE_HEAD(&sc->sc_tx_pending, next);
		STAILQ_INSERT_TAIL(&sc->sc_tx_active, data, next);

		usbd_xfer_set_frame_data(xfer, 0, data->buf, data->buflen);
		usbd_transfer_submit(xfer);

		URTWN_UNLOCK(sc);
		urtwn_start(ifp);
		URTWN_LOCK(sc);
		break;
	default:
		data = STAILQ_FIRST(&sc->sc_tx_active);
		if (data == NULL)
			goto tr_setup;
		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
			ifp->if_oerrors++;
		}
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static struct urtwn_data *
_urtwn_getbuf(struct urtwn_softc *sc)
{
	struct urtwn_data *bf;

	bf = STAILQ_FIRST(&sc->sc_tx_inactive);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&sc->sc_tx_inactive, next);
	else
		bf = NULL;
	if (bf == NULL)
		DPRINTF("%s: %s\n", __func__, "out of xmit buffers");
	return (bf);
}

static struct urtwn_data *
urtwn_getbuf(struct urtwn_softc *sc)
{
        struct urtwn_data *bf;

	URTWN_ASSERT_LOCKED(sc);

	bf = _urtwn_getbuf(sc);
	if (bf == NULL) {
		struct ifnet *ifp = sc->sc_ifp;
		DPRINTF("%s: stop queue\n", __func__);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	}
	return (bf);
}

static int
urtwn_write_region_1(struct urtwn_softc *sc, uint16_t addr, uint8_t *buf,
    int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = R92C_REQ_REGS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (urtwn_do_request(sc, &req, buf));
}

static void
urtwn_write_1(struct urtwn_softc *sc, uint16_t addr, uint8_t val)
{
	urtwn_write_region_1(sc, addr, &val, 1);
}


static void
urtwn_write_2(struct urtwn_softc *sc, uint16_t addr, uint16_t val)
{
	val = htole16(val);
	urtwn_write_region_1(sc, addr, (uint8_t *)&val, 2);
}

static void
urtwn_write_4(struct urtwn_softc *sc, uint16_t addr, uint32_t val)
{
	val = htole32(val);
	urtwn_write_region_1(sc, addr, (uint8_t *)&val, 4);
}

static int
urtwn_read_region_1(struct urtwn_softc *sc, uint16_t addr, uint8_t *buf,
    int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = R92C_REQ_REGS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (urtwn_do_request(sc, &req, buf));
}

static uint8_t
urtwn_read_1(struct urtwn_softc *sc, uint16_t addr)
{
	uint8_t val;

	if (urtwn_read_region_1(sc, addr, &val, 1) != 0)
		return (0xff);
	return (val);
}

static uint16_t
urtwn_read_2(struct urtwn_softc *sc, uint16_t addr)
{
	uint16_t val;

	if (urtwn_read_region_1(sc, addr, (uint8_t *)&val, 2) != 0)
		return (0xffff);
	return (le16toh(val));
}

static uint32_t
urtwn_read_4(struct urtwn_softc *sc, uint16_t addr)
{
	uint32_t val;

	if (urtwn_read_region_1(sc, addr, (uint8_t *)&val, 4) != 0)
		return (0xffffffff);
	return (le32toh(val));
}

static int
urtwn_fw_cmd(struct urtwn_softc *sc, uint8_t id, const void *buf, int len)
{
	struct r92c_fw_cmd cmd;
	int ntries;

	/* Wait for current FW box to be empty. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(urtwn_read_1(sc, R92C_HMETFR) & (1 << sc->fwcur)))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "could not send firmware command\n");
		return (ETIMEDOUT);
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.id = id;
	if (len > 3)
		cmd.id |= R92C_CMD_FLAG_EXT;
	KASSERT(len <= sizeof(cmd.msg), ("urtwn_fw_cmd\n"));
	memcpy(cmd.msg, buf, len);

	/* Write the first word last since that will trigger the FW. */
	urtwn_write_region_1(sc, R92C_HMEBOX_EXT(sc->fwcur),
	    (uint8_t *)&cmd + 4, 2);
	urtwn_write_region_1(sc, R92C_HMEBOX(sc->fwcur),
	    (uint8_t *)&cmd + 0, 4);

	sc->fwcur = (sc->fwcur + 1) % R92C_H2C_NBOX;
	return (0);
}

static void
urtwn_rf_write(struct urtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{
	urtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) |
	    SM(R92C_LSSI_PARAM_DATA, val));
}

static uint32_t
urtwn_rf_read(struct urtwn_softc *sc, int chain, uint8_t addr)
{
	uint32_t reg[R92C_MAX_CHAINS], val;

	reg[0] = urtwn_bb_read(sc, R92C_HSSI_PARAM2(0));
	if (chain != 0)
		reg[chain] = urtwn_bb_read(sc, R92C_HSSI_PARAM2(chain));

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] & ~R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	if (urtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI)
		val = urtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	else
		val = urtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	return (MS(val, R92C_LSSI_READBACK_DATA));
}

static int
urtwn_llt_write(struct urtwn_softc *sc, uint32_t addr, uint32_t data)
{
	int ntries;

	urtwn_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(urtwn_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return (0);
		DELAY(5);
	}
	return (ETIMEDOUT);
}

static uint8_t
urtwn_efuse_read_1(struct urtwn_softc *sc, uint16_t addr)
{
	uint32_t reg;
	int ntries;

	reg = urtwn_read_4(sc, R92C_EFUSE_CTRL);
	reg = RW(reg, R92C_EFUSE_CTRL_ADDR, addr);
	reg &= ~R92C_EFUSE_CTRL_VALID;
	urtwn_write_4(sc, R92C_EFUSE_CTRL, reg);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = urtwn_read_4(sc, R92C_EFUSE_CTRL);
		if (reg & R92C_EFUSE_CTRL_VALID)
			return (MS(reg, R92C_EFUSE_CTRL_DATA));
		DELAY(5);
	}
	device_printf(sc->sc_dev, 
	    "could not read efuse byte at address 0x%x\n", addr);
	return (0xff);
}

static void
urtwn_efuse_read(struct urtwn_softc *sc)
{
	uint8_t *rom = (uint8_t *)&sc->rom;
	uint16_t addr = 0;
	uint32_t reg;
	uint8_t off, msk;
	int i;

	reg = urtwn_read_2(sc, R92C_SYS_ISO_CTRL);
	if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) {
		urtwn_write_2(sc, R92C_SYS_ISO_CTRL,
		    reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
	}
	reg = urtwn_read_2(sc, R92C_SYS_FUNC_EN);
	if (!(reg & R92C_SYS_FUNC_EN_ELDR)) {
		urtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    reg | R92C_SYS_FUNC_EN_ELDR);
	}
	reg = urtwn_read_2(sc, R92C_SYS_CLKR);
	if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) !=
	    (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) {
		urtwn_write_2(sc, R92C_SYS_CLKR,
		    reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);
	}
	memset(&sc->rom, 0xff, sizeof(sc->rom));
	while (addr < 512) {
		reg = urtwn_efuse_read_1(sc, addr);
		if (reg == 0xff)
			break;
		addr++;
		off = reg >> 4;
		msk = reg & 0xf;
		for (i = 0; i < 4; i++) {
			if (msk & (1 << i))
				continue;
			rom[off * 8 + i * 2 + 0] =
			    urtwn_efuse_read_1(sc, addr);
			addr++;
			rom[off * 8 + i * 2 + 1] =
			    urtwn_efuse_read_1(sc, addr);
			addr++;
		}
	}
#ifdef URTWN_DEBUG
	if (urtwn_debug >= 2) {
		/* Dump ROM content. */
		printf("\n");
		for (i = 0; i < sizeof(sc->rom); i++)
			printf("%02x:", rom[i]);
		printf("\n");
	}
#endif
}

static int
urtwn_read_chipid(struct urtwn_softc *sc)
{
	uint32_t reg;

	reg = urtwn_read_4(sc, R92C_SYS_CFG);
	if (reg & R92C_SYS_CFG_TRP_VAUX_EN)
		return (EIO);

	if (reg & R92C_SYS_CFG_TYPE_92C) {
		sc->chip |= URTWN_CHIP_92C;
		/* Check if it is a castrated 8192C. */
		if (MS(urtwn_read_4(sc, R92C_HPON_FSM),
		    R92C_HPON_FSM_CHIP_BONDING_ID) ==
		    R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R)
			sc->chip |= URTWN_CHIP_92C_1T2R;
	}
	if (reg & R92C_SYS_CFG_VENDOR_UMC) {
		sc->chip |= URTWN_CHIP_UMC;
		if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) == 0)
			sc->chip |= URTWN_CHIP_UMC_A_CUT;
	}
	return (0);
}

static void
urtwn_read_rom(struct urtwn_softc *sc)
{
	struct r92c_rom *rom = &sc->rom;

	/* Read full ROM image. */
	urtwn_efuse_read(sc);

	/* XXX Weird but this is what the vendor driver does. */
	sc->pa_setting = urtwn_efuse_read_1(sc, 0x1fa);
	DPRINTF("PA setting=0x%x\n", sc->pa_setting);

	sc->board_type = MS(rom->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);

	sc->regulatory = MS(rom->rf_opt1, R92C_ROM_RF1_REGULATORY);
	DPRINTF("regulatory type=%d\n", sc->regulatory);

	IEEE80211_ADDR_COPY(sc->sc_bssid, rom->macaddr);
}

/*
 * Initialize rate adaptation in firmware.
 */
static int
urtwn_ra_init(struct urtwn_softc *sc)
{
	static const uint8_t map[] =
	    { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni;
	struct ieee80211_rateset *rs;
	struct r92c_fw_cmd_macid_cfg cmd;
	uint32_t rates, basicrates;
	uint8_t mode;
	int maxrate, maxbasicrate, error, i, j;

	ni = ieee80211_ref_node(vap->iv_bss);
	rs = &ni->ni_rates;

	/* Get normal and basic rates mask. */
	rates = basicrates = 0;
	maxrate = maxbasicrate = 0;
	for (i = 0; i < rs->rs_nrates; i++) {
		/* Convert 802.11 rate to HW rate index. */
		for (j = 0; j < nitems(map); j++)
			if ((rs->rs_rates[i] & IEEE80211_RATE_VAL) == map[j])
				break;
		if (j == nitems(map))	/* Unknown rate, skip. */
			continue;
		rates |= 1 << j;
		if (j > maxrate)
			maxrate = j;
		if (rs->rs_rates[i] & IEEE80211_RATE_BASIC) {
			basicrates |= 1 << j;
			if (j > maxbasicrate)
				maxbasicrate = j;
		}
	}
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	DPRINTF("mode=0x%x rates=0x%08x, basicrates=0x%08x\n",
	    mode, rates, basicrates);

	/* Set rates mask for group addressed frames. */
	cmd.macid = URTWN_MACID_BC | URTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | basicrates);
	error = urtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		ieee80211_free_node(ni);
		device_printf(sc->sc_dev,
		    "could not add broadcast station\n");
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF("maxbasicrate=%d\n", maxbasicrate);
	urtwn_write_1(sc, R92C_INIDATA_RATE_SEL(URTWN_MACID_BC),
	    maxbasicrate);

	/* Set rates mask for unicast frames. */
	cmd.macid = URTWN_MACID_BSS | URTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | rates);
	error = urtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		ieee80211_free_node(ni);
		device_printf(sc->sc_dev, "could not add BSS station\n");
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF("maxrate=%d\n", maxrate);
	urtwn_write_1(sc, R92C_INIDATA_RATE_SEL(URTWN_MACID_BSS),
	    maxrate);

	/* Indicate highest supported rate. */
	ni->ni_txrate = rs->rs_rates[rs->rs_nrates - 1];
	ieee80211_free_node(ni);

	return (0);
}

void
urtwn_tsf_sync_enable(struct urtwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = vap->iv_bss;

	uint64_t tsf;

	/* Enable TSF synchronization. */
	urtwn_write_1(sc, R92C_BCN_CTRL,
	    urtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_DIS_TSF_UDT0);

	urtwn_write_1(sc, R92C_BCN_CTRL,
	    urtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_EN_BCN);

	/* Set initial TSF. */
	memcpy(&tsf, ni->ni_tstamp.data, 8);
	tsf = le64toh(tsf);
	tsf = tsf - (tsf % (vap->iv_bss->ni_intval * IEEE80211_DUR_TU));
	tsf -= IEEE80211_DUR_TU;
	urtwn_write_4(sc, R92C_TSFTR + 0, tsf);
	urtwn_write_4(sc, R92C_TSFTR + 4, tsf >> 32);

	urtwn_write_1(sc, R92C_BCN_CTRL,
	    urtwn_read_1(sc, R92C_BCN_CTRL) | R92C_BCN_CTRL_EN_BCN);
}

static void
urtwn_set_led(struct urtwn_softc *sc, int led, int on)
{
	uint8_t reg;

	if (led == URTWN_LED_LINK) {
		reg = urtwn_read_1(sc, R92C_LEDCFG0) & 0x70;
		if (!on)
			reg |= R92C_LEDCFG0_DIS;
		urtwn_write_1(sc, R92C_LEDCFG0, reg);
		sc->ledlink = on;	/* Save LED state. */
	}
}

static int
urtwn_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct urtwn_vap *uvp = URTWN_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct urtwn_softc *sc = ic->ic_ifp->if_softc;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;
	uint32_t reg;

	ostate = vap->iv_state;
	DPRINTF("%s -> %s\n", ieee80211_state_name[ostate],
	    ieee80211_state_name[nstate]);

	IEEE80211_UNLOCK(ic);
	URTWN_LOCK(sc);
	callout_stop(&sc->sc_watchdog_ch);

	if (ostate == IEEE80211_S_RUN) {
		/* Turn link LED off. */
		urtwn_set_led(sc, URTWN_LED_LINK, 0);

		/* Set media status to 'No Link'. */
		reg = urtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_NOLINK);
		urtwn_write_4(sc, R92C_CR, reg);

		/* Stop Rx of data frames. */
		urtwn_write_2(sc, R92C_RXFLTMAP2, 0);

		/* Rest TSF. */
		urtwn_write_1(sc, R92C_DUAL_TSF_RST, 0x03);

		/* Disable TSF synchronization. */
		urtwn_write_1(sc, R92C_BCN_CTRL,
		    urtwn_read_1(sc, R92C_BCN_CTRL) |
		    R92C_BCN_CTRL_DIS_TSF_UDT0);

		/* Reset EDCA parameters. */
		urtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3217);
		urtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4317);
		urtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x00105320);
		urtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a444);
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		/* Turn link LED off. */
		urtwn_set_led(sc, URTWN_LED_LINK, 0);
		break;
	case IEEE80211_S_SCAN:
		if (ostate != IEEE80211_S_SCAN) {
			/* Allow Rx from any BSSID. */
			urtwn_write_4(sc, R92C_RCR,
			    urtwn_read_4(sc, R92C_RCR) &
			    ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

			/* Set gain for scanning. */
			reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

			reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);
		}

		/* Make link LED blink during scan. */
		urtwn_set_led(sc, URTWN_LED_LINK, !sc->ledlink);

		/* Pause AC Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE,
		    urtwn_read_1(sc, R92C_TXPAUSE) | 0x0f);

		urtwn_set_chan(sc, ic->ic_curchan, NULL);
		break;
	case IEEE80211_S_AUTH:
		/* Set initial gain under link. */
		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);

		urtwn_set_chan(sc, ic->ic_curchan, NULL);
		break;
	case IEEE80211_S_RUN:
		if (vap->iv_opmode == IEEE80211_M_MONITOR) {
			/* Enable Rx of data frames. */
			urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

			/* Turn link LED on. */
			urtwn_set_led(sc, URTWN_LED_LINK, 1);
			break;
		}

		ni = ieee80211_ref_node(vap->iv_bss);
		/* Set media status to 'Associated'. */
		reg = urtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
		urtwn_write_4(sc, R92C_CR, reg);

		/* Set BSSID. */
		urtwn_write_4(sc, R92C_BSSID + 0, LE_READ_4(&ni->ni_bssid[0]));
		urtwn_write_4(sc, R92C_BSSID + 4, LE_READ_2(&ni->ni_bssid[4]));

		if (ic->ic_curmode == IEEE80211_MODE_11B)
			urtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 0);
		else	/* 802.11b/g */
			urtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 3);

		/* Enable Rx of data frames. */
		urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

		/* Flush all AC queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0);

		/* Set beacon interval. */
		urtwn_write_2(sc, R92C_BCN_INTERVAL, ni->ni_intval);

		/* Allow Rx from our BSSID only. */
		urtwn_write_4(sc, R92C_RCR,
		    urtwn_read_4(sc, R92C_RCR) |
		    R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);

		/* Enable TSF synchronization. */
		urtwn_tsf_sync_enable(sc);

		urtwn_write_1(sc, R92C_SIFS_CCK + 1, 10);
		urtwn_write_1(sc, R92C_SIFS_OFDM + 1, 10);
		urtwn_write_1(sc, R92C_SPEC_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_MAC_SPEC_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_R2T_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_T2T_SIFS + 1, 10);

		/* Intialize rate adaptation. */
		urtwn_ra_init(sc);
		/* Turn link LED on. */
		urtwn_set_led(sc, URTWN_LED_LINK, 1);

		sc->avg_pwdb = -1;	/* Reset average RSSI. */
		/* Reset temperature calibration state machine. */
		sc->thcal_state = 0;
		sc->thcal_lctemp = 0;
		ieee80211_free_node(ni);
		break;
	default:
		break;
	}
	URTWN_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return(uvp->newstate(vap, nstate, arg));
}

static void
urtwn_watchdog(void *arg)
{
	struct urtwn_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->sc_txtimer > 0) {
		if (--sc->sc_txtimer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			ifp->if_oerrors++;
			return;
		}
		callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);
	}
}

static void
urtwn_update_avgrssi(struct urtwn_softc *sc, int rate, int8_t rssi)
{
	int pwdb;

	/* Convert antenna signal to percentage. */
	if (rssi <= -100 || rssi >= 20)
		pwdb = 0;
	else if (rssi >= 0)
		pwdb = 100;
	else
		pwdb = 100 + rssi;
	if (rate <= 3) {
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
	DPRINTFN(4, "PWDB=%d EMA=%d\n", pwdb, sc->avg_pwdb);
}

static int8_t
urtwn_get_rssi(struct urtwn_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 16, -12, -26, -46 };
	struct r92c_rx_phystat *phy;
	struct r92c_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	if (rate <= 3) {
		cck = (struct r92c_rx_cck *)physt;
		if (sc->sc_flags & URTWN_FLAG_CCK_HIPWR) {
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

static int
urtwn_tx_start(struct urtwn_softc *sc, struct ieee80211_node *ni, 
    struct mbuf *m0, struct urtwn_data *data)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = ni->ni_vap;
	struct usb_xfer *xfer;
	struct r92c_tx_desc *txd;
	uint8_t raid, type;
	uint16_t sum;
	int i, hasqos, xferlen;
	struct usb_xfer *urtwn_pipes[4] = {
		sc->sc_xfer[URTWN_BULK_TX_BE],
		sc->sc_xfer[URTWN_BULK_TX_BK],
		sc->sc_xfer[URTWN_BULK_TX_VI],
		sc->sc_xfer[URTWN_BULK_TX_VO]
	};

	URTWN_ASSERT_LOCKED(sc);

	/*
	 * Software crypto.
	 */
	wh = mtod(m0, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			device_printf(sc->sc_dev,
			    "ieee80211_crypto_encap returns NULL.\n");
			/* XXX we don't expect the fragmented frames */
			m_freem(m0);
			return (ENOBUFS);
		}

		/* in case packet header moved, reset pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}
	
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_CTL:
	case IEEE80211_FC0_TYPE_MGT:
		xfer = sc->sc_xfer[URTWN_BULK_TX_VO];
		break;
	default:
		KASSERT(M_WME_GETAC(m0) < 4,
		    ("unsupported WME pipe %d", M_WME_GETAC(m0)));
		xfer = urtwn_pipes[M_WME_GETAC(m0)];
		break;
	}
			    
	hasqos = 0;

	/* Fill Tx descriptor. */
	txd = (struct r92c_tx_desc *)data->buf;
	memset(txd, 0, sizeof(*txd));

	txd->txdw0 |= htole32(
	    SM(R92C_TXDW0_PKTLEN, m0->m_pkthdr.len) |
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_OWN | R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    type == IEEE80211_FC0_TYPE_DATA) {
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			raid = R92C_RAID_11B;
		else
			raid = R92C_RAID_11BG;
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, URTWN_MACID_BSS) |
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
		/* Send RTS at OFDM24. */
		txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE, 8));
		txd->txdw5 |= htole32(0x0001ff00);
		/* Send data at OFDM54. */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 11));
	} else {
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, 0) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_MGNT) |
		    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

		/* Force CCK1. */
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 0));
	}
	/* Set sequence number (already little endian). */
	txd->txdseq |= *(uint16_t *)wh->i_seq;

	if (!hasqos) {
		/* Use HW sequence numbering for non-QoS frames. */
		txd->txdw4  |= htole32(R92C_TXDW4_HWSEQ);
		txd->txdseq |= htole16(0x8000);
	} else
		txd->txdw4 |= htole32(R92C_TXDW4_QOS);

	/* Compute Tx descriptor checksum. */
	sum = 0;
	for (i = 0; i < sizeof(*txd) / 2; i++)
		sum ^= ((uint16_t *)txd)[i];
	txd->txdsum = sum; 	/* NB: already little endian. */

	if (ieee80211_radiotap_active_vap(vap)) {
		struct urtwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		ieee80211_radiotap_tx(vap, m0);
	}

	xferlen = sizeof(*txd) + m0->m_pkthdr.len;
	m_copydata(m0, 0, m0->m_pkthdr.len, (caddr_t)&txd[1]);

	data->buflen = xferlen;
	data->ni = ni;
	data->m = m0;

	STAILQ_INSERT_TAIL(&sc->sc_tx_pending, data, next);
	usbd_transfer_start(xfer);
	return (0);
}

static void
urtwn_start(struct ifnet *ifp)
{
	struct urtwn_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct mbuf *m;
	struct urtwn_data *bf;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	URTWN_LOCK(sc);
	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		bf = urtwn_getbuf(sc);
		if (bf == NULL) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			break;
		}
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;

		if (urtwn_tx_start(sc, ni, m, bf) != 0) {
			ifp->if_oerrors++;
			STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, bf, next);
			ieee80211_free_node(ni);
			break;
		}

		sc->sc_txtimer = 5;
		callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);
	}
	URTWN_UNLOCK(sc);
}

static int
urtwn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
				urtwn_init(ifp->if_softc);
				startall = 1;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				urtwn_stop(ifp, 1);
		}
		if (startall)
			ieee80211_start_all(ic);
		break;
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, cmd, data);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
urtwn_alloc_list(struct urtwn_softc *sc, struct urtwn_data data[],
    int ndata, int maxsz)
{
	int i, error;

	for (i = 0; i < ndata; i++) {
		struct urtwn_data *dp = &data[i];
		dp->sc = sc;
		dp->m = NULL;
		dp->buf = malloc(maxsz, M_USBDEV, M_NOWAIT);
		if (dp->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate buffer\n");
			error = ENOMEM;
			goto fail;
		}
		dp->ni = NULL;
	}

	return (0);
fail:
	urtwn_free_list(sc, data, ndata);
	return (error);
}

static int
urtwn_alloc_rx_list(struct urtwn_softc *sc)
{
        int error, i;

	error = urtwn_alloc_list(sc, sc->sc_rx, URTWN_RX_LIST_COUNT,
	    URTWN_RXBUFSZ);
	if (error != 0)
		return (error);

	STAILQ_INIT(&sc->sc_rx_active);
	STAILQ_INIT(&sc->sc_rx_inactive);

	for (i = 0; i < URTWN_RX_LIST_COUNT; i++)
		STAILQ_INSERT_HEAD(&sc->sc_rx_inactive, &sc->sc_rx[i], next);

	return (0);
}

static int
urtwn_alloc_tx_list(struct urtwn_softc *sc)
{
	int error, i;

	error = urtwn_alloc_list(sc, sc->sc_tx, URTWN_TX_LIST_COUNT,
	    URTWN_TXBUFSZ);
	if (error != 0)
		return (error);

	STAILQ_INIT(&sc->sc_tx_active);
	STAILQ_INIT(&sc->sc_tx_inactive);
	STAILQ_INIT(&sc->sc_tx_pending);

	for (i = 0; i < URTWN_TX_LIST_COUNT; i++)
		STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, &sc->sc_tx[i], next);

	return (0);
}

static int
urtwn_power_on(struct urtwn_softc *sc)
{
	uint32_t reg;
	int ntries;

	/* Wait for autoload done bit. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_1(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for chip autoload\n");
		return (ETIMEDOUT);
	}

	/* Unlock ISO/CLK/Power control register. */
	urtwn_write_1(sc, R92C_RSV_CTRL, 0);
	/* Move SPS into PWM mode. */
	urtwn_write_1(sc, R92C_SPS0_CTRL, 0x2b);
	DELAY(100);

	reg = urtwn_read_1(sc, R92C_LDOV12D_CTRL);
	if (!(reg & R92C_LDOV12D_CTRL_LDV12_EN)) {
		urtwn_write_1(sc, R92C_LDOV12D_CTRL,
		    reg | R92C_LDOV12D_CTRL_LDV12_EN);
		DELAY(100);
		urtwn_write_1(sc, R92C_SYS_ISO_CTRL,
		    urtwn_read_1(sc, R92C_SYS_ISO_CTRL) &
		    ~R92C_SYS_ISO_CTRL_MD2PP);
	}

	/* Auto enable WLAN. */
	urtwn_write_2(sc, R92C_APS_FSMCO,
	    urtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for MAC auto ON\n");
		return (ETIMEDOUT);
	}

	/* Enable radio, GPIO and LED functions. */
	urtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_HSUS |
	    R92C_APS_FSMCO_PDN_EN |
	    R92C_APS_FSMCO_PFM_ALDN);
	/* Release RF digital isolation. */
	urtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	    urtwn_read_2(sc, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);

	/* Initialize MAC. */
	urtwn_write_1(sc, R92C_APSD_CTRL,
	    urtwn_read_1(sc, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
	for (ntries = 0; ntries < 200; ntries++) {
		if (!(urtwn_read_1(sc, R92C_APSD_CTRL) &
		    R92C_APSD_CTRL_OFF_STATUS))
			break;
		DELAY(5);
	}
	if (ntries == 200) {
		device_printf(sc->sc_dev,
		    "timeout waiting for MAC initialization\n");
		return (ETIMEDOUT);
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	reg = urtwn_read_2(sc, R92C_CR);
	reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC;
	urtwn_write_2(sc, R92C_CR, reg);

	urtwn_write_1(sc, 0xfe10, 0x19);
	return (0);
}

static int
urtwn_llt_init(struct urtwn_softc *sc)
{
	int i, error;

	/* Reserve pages [0; R92C_TX_PAGE_COUNT]. */
	for (i = 0; i < R92C_TX_PAGE_COUNT; i++) {
		if ((error = urtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* NB: 0xff indicates end-of-list. */
	if ((error = urtwn_llt_write(sc, i, 0xff)) != 0)
		return (error);
	/*
	 * Use pages [R92C_TX_PAGE_COUNT + 1; R92C_TXPKTBUF_COUNT - 1]
	 * as ring buffer.
	 */
	for (++i; i < R92C_TXPKTBUF_COUNT - 1; i++) {
		if ((error = urtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* Make the last page point to the beginning of the ring buffer. */
	error = urtwn_llt_write(sc, i, R92C_TX_PAGE_COUNT + 1);
	return (error);
}

static void
urtwn_fw_reset(struct urtwn_softc *sc)
{
	uint16_t reg;
	int ntries;

	/* Tell 8051 to reset itself. */
	urtwn_write_1(sc, R92C_HMETFR + 3, 0x20);

	/* Wait until 8051 resets by itself. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = urtwn_read_2(sc, R92C_SYS_FUNC_EN);
		if (!(reg & R92C_SYS_FUNC_EN_CPUEN))
			return;
		DELAY(50);
	}
	/* Force 8051 reset. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
}

static int
urtwn_fw_loadpage(struct urtwn_softc *sc, int page, const uint8_t *buf, int len)
{
	uint32_t reg;
	int off, mlen, error = 0;

	reg = urtwn_read_4(sc, R92C_MCUFWDL);
	reg = RW(reg, R92C_MCUFWDL_PAGE, page);
	urtwn_write_4(sc, R92C_MCUFWDL, reg);

	off = R92C_FW_START_ADDR;
	while (len > 0) {
		if (len > 196)
			mlen = 196;
		else if (len > 4)
			mlen = 4;
		else
			mlen = 1;
		/* XXX fix this deconst */
		error = urtwn_write_region_1(sc, off, 
		    __DECONST(uint8_t *, buf), mlen);
		if (error != 0)
			break;
		off += mlen;
		buf += mlen;
		len -= mlen;
	}
	return (error);
}

static int
urtwn_load_firmware(struct urtwn_softc *sc)
{
	const struct firmware *fw;
	const struct r92c_fw_hdr *hdr;
	const char *imagename;
	const u_char *ptr;
	size_t len;
	uint32_t reg;
	int mlen, ntries, page, error;

	/* Read firmware image from the filesystem. */
	if ((sc->chip & (URTWN_CHIP_UMC_A_CUT | URTWN_CHIP_92C)) ==
	    URTWN_CHIP_UMC_A_CUT)
		imagename = "urtwn-rtl8192cfwU";
	else
		imagename = "urtwn-rtl8192cfwT";

	fw = firmware_get(imagename);
	if (fw == NULL) {
		device_printf(sc->sc_dev,
		    "failed loadfirmware of file %s\n", imagename);
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
		DPRINTF("FW V%d.%d %02d-%02d %02d:%02d\n",
		    le16toh(hdr->version), le16toh(hdr->subversion),
		    hdr->month, hdr->date, hdr->hour, hdr->minute);
		ptr += sizeof(*hdr);
		len -= sizeof(*hdr);
	}

	if (urtwn_read_1(sc, R92C_MCUFWDL) & 0x80) {
		urtwn_fw_reset(sc);
		urtwn_write_1(sc, R92C_MCUFWDL, 0);
	}
	urtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_CPUEN);
	urtwn_write_1(sc, R92C_MCUFWDL,
	    urtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
	urtwn_write_1(sc, R92C_MCUFWDL + 2,
	    urtwn_read_1(sc, R92C_MCUFWDL + 2) & ~0x08);

	for (page = 0; len > 0; page++) {
		mlen = min(len, R92C_FW_PAGE_SIZE);
		error = urtwn_fw_loadpage(sc, page, ptr, mlen);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load firmware page\n");
			goto fail;
		}
		ptr += mlen;
		len -= mlen;
	}
	urtwn_write_1(sc, R92C_MCUFWDL,
	    urtwn_read_1(sc, R92C_MCUFWDL) & ~R92C_MCUFWDL_EN);
	urtwn_write_1(sc, R92C_MCUFWDL + 1, 0);

	/* Wait for checksum report. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_CHKSUM_RPT)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for checksum report\n");
		error = ETIMEDOUT;
		goto fail;
	}

	reg = urtwn_read_4(sc, R92C_MCUFWDL);
	reg = (reg & ~R92C_MCUFWDL_WINTINI_RDY) | R92C_MCUFWDL_RDY;
	urtwn_write_4(sc, R92C_MCUFWDL, reg);
	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY)
			break;
		DELAY(5);
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
urtwn_dma_init(struct urtwn_softc *sc)
{
	int hashq, hasnq, haslq, nqueues, nqpages, nrempages;
	uint32_t reg;
	int error;

	/* Initialize LLT table. */
	error = urtwn_llt_init(sc);
	if (error != 0)
		return (error);

	/* Get Tx queues to USB endpoints mapping. */
	hashq = hasnq = haslq = 0;
	reg = urtwn_read_2(sc, R92C_USB_EP + 1);
	DPRINTFN(2, "USB endpoints mapping 0x%x\n", reg);
	if (MS(reg, R92C_USB_EP_HQ) != 0)
		hashq = 1;
	if (MS(reg, R92C_USB_EP_NQ) != 0)
		hasnq = 1;
	if (MS(reg, R92C_USB_EP_LQ) != 0)
		haslq = 1;
	nqueues = hashq + hasnq + haslq;
	if (nqueues == 0)
		return (EIO);
	/* Get the number of pages for each queue. */
	nqpages = (R92C_TX_PAGE_COUNT - R92C_PUBQ_NPAGES) / nqueues;
	/* The remaining pages are assigned to the high priority queue. */
	nrempages = (R92C_TX_PAGE_COUNT - R92C_PUBQ_NPAGES) % nqueues;

	/* Set number of pages for normal priority queue. */
	urtwn_write_1(sc, R92C_RQPN_NPQ, hasnq ? nqpages : 0);
	urtwn_write_4(sc, R92C_RQPN,
	    /* Set number of pages for public queue. */
	    SM(R92C_RQPN_PUBQ, R92C_PUBQ_NPAGES) |
	    /* Set number of pages for high priority queue. */
	    SM(R92C_RQPN_HPQ, hashq ? nqpages + nrempages : 0) |
	    /* Set number of pages for low priority queue. */
	    SM(R92C_RQPN_LPQ, haslq ? nqpages : 0) |
	    /* Load values. */
	    R92C_RQPN_LD);

	urtwn_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TRXFF_BNDY, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TDECTRL + 1, R92C_TX_PAGE_BOUNDARY);

	/* Set queue to USB pipe mapping. */
	reg = urtwn_read_2(sc, R92C_TRXDMA_CTRL);
	reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
	if (nqueues == 1) {
		if (hashq)
			reg |= R92C_TRXDMA_CTRL_QMAP_HQ;
		else if (hasnq)
			reg |= R92C_TRXDMA_CTRL_QMAP_NQ;
		else
			reg |= R92C_TRXDMA_CTRL_QMAP_LQ;
	} else if (nqueues == 2) {
		/* All 2-endpoints configs have a high priority queue. */
		if (!hashq)
			return (EIO);
		if (hasnq)
			reg |= R92C_TRXDMA_CTRL_QMAP_HQ_NQ;
		else
			reg |= R92C_TRXDMA_CTRL_QMAP_HQ_LQ;
	} else
		reg |= R92C_TRXDMA_CTRL_QMAP_3EP;
	urtwn_write_2(sc, R92C_TRXDMA_CTRL, reg);

	/* Set Tx/Rx transfer page boundary. */
	urtwn_write_2(sc, R92C_TRXFF_BNDY + 2, 0x27ff);

	/* Set Tx/Rx transfer page size. */
	urtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));
	return (0);
}

static void
urtwn_mac_init(struct urtwn_softc *sc)
{
	int i;

	/* Write MAC initialization values. */
	for (i = 0; i < nitems(rtl8192cu_mac); i++)
		urtwn_write_1(sc, rtl8192cu_mac[i].reg, rtl8192cu_mac[i].val);
}

static void
urtwn_bb_init(struct urtwn_softc *sc)
{
	const struct urtwn_bb_prog *prog;
	uint32_t reg;
	int i;

	/* Enable BB and RF. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	urtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0xdb83);

	urtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_USBA | R92C_SYS_FUNC_EN_USBD |
	    R92C_SYS_FUNC_EN_BB_GLB_RST | R92C_SYS_FUNC_EN_BBRSTB);

	urtwn_write_1(sc, R92C_LDOHCI12_CTRL, 0x0f);
	urtwn_write_1(sc, 0x15, 0xe9);
	urtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);

	/* Select BB programming based on board type. */
	if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = &rtl8188ce_bb_prog;
		else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			prog = &rtl8188ru_bb_prog;
		else
			prog = &rtl8188cu_bb_prog;
	} else {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = &rtl8192ce_bb_prog;
		else
			prog = &rtl8192cu_bb_prog;
	}
	/* Write BB initialization values. */
	for (i = 0; i < prog->count; i++) {
		urtwn_bb_write(sc, prog->regs[i], prog->vals[i]);
		DELAY(1);
	}

	if (sc->chip & URTWN_CHIP_92C_1T2R) {
		/* 8192C 1T only configuration. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_TXINFO);
		reg = (reg & ~0x00000003) | 0x2;
		urtwn_bb_write(sc, R92C_FPGA0_TXINFO, reg);

		reg = urtwn_bb_read(sc, R92C_FPGA1_TXINFO);
		reg = (reg & ~0x00300033) | 0x00200022;
		urtwn_bb_write(sc, R92C_FPGA1_TXINFO, reg);

		reg = urtwn_bb_read(sc, R92C_CCK0_AFESETTING);
		reg = (reg & ~0xff000000) | 0x45 << 24;
		urtwn_bb_write(sc, R92C_CCK0_AFESETTING, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		reg = (reg & ~0x000000ff) | 0x23;
		urtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCPARAM1);
		reg = (reg & ~0x00000030) | 1 << 4;
		urtwn_bb_write(sc, R92C_OFDM0_AGCPARAM1, reg);

		reg = urtwn_bb_read(sc, 0xe74);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe74, reg);
		reg = urtwn_bb_read(sc, 0xe78);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe78, reg);
		reg = urtwn_bb_read(sc, 0xe7c);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe7c, reg);
		reg = urtwn_bb_read(sc, 0xe80);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe80, reg);
		reg = urtwn_bb_read(sc, 0xe88);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe88, reg);
	}

	/* Write AGC values. */
	for (i = 0; i < prog->agccount; i++) {
		urtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE,
		    prog->agcvals[i]);
		DELAY(1);
	}

	if (urtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
	    R92C_HSSI_PARAM2_CCK_HIPWR)
		sc->sc_flags |= URTWN_FLAG_CCK_HIPWR;
}

void
urtwn_rf_init(struct urtwn_softc *sc)
{
	const struct urtwn_rf_prog *prog;
	uint32_t reg, type;
	int i, j, idx, off;

	/* Select RF programming based on board type. */
	if (!(sc->chip & URTWN_CHIP_92C)) {
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
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		type = (reg >> off) & 0x10;

		/* Set RF_ENV enable. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x100000;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set RF_ENV output high. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set address and data lengths of RF registers. */
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				usb_pause_mtx(&sc->sc_mtx, 50);
				continue;
			}
			urtwn_rf_write(sc, i, prog[i].regs[j],
			    prog[i].vals[j]);
			DELAY(1);
		}

		/* Restore RF_ENV control type. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		reg &= ~(0x10 << off) | (type << off);
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(idx), reg);

		/* Cache RF register CHNLBW. */
		sc->rf_chnlbw[i] = urtwn_rf_read(sc, i, R92C_RF_CHNLBW);
	}

	if ((sc->chip & (URTWN_CHIP_UMC_A_CUT | URTWN_CHIP_92C)) ==
	    URTWN_CHIP_UMC_A_CUT) {
		urtwn_rf_write(sc, 0, R92C_RF_RX_G1, 0x30255);
		urtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x50a00);
	}
}

static void
urtwn_cam_init(struct urtwn_softc *sc)
{
	/* Invalidate all CAM entries. */
	urtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_CLR);
}

static void
urtwn_pa_bias_init(struct urtwn_softc *sc)
{
	uint8_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		if (sc->pa_setting & (1 << i))
			continue;
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x0f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x4f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x8f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0xcf406);
	}
	if (!(sc->pa_setting & 0x10)) {
		reg = urtwn_read_1(sc, 0x16);
		reg = (reg & ~0xf0) | 0x90;
		urtwn_write_1(sc, 0x16, reg);
	}
}

static void
urtwn_rxfilter_init(struct urtwn_softc *sc)
{
	/* Initialize Rx filter. */
	/* TODO: use better filter for monitor mode. */
	urtwn_write_4(sc, R92C_RCR,
	    R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB |
	    R92C_RCR_APP_ICV | R92C_RCR_AMF | R92C_RCR_HTC_LOC_CTRL |
	    R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
	/* Accept all multicast frames. */
	urtwn_write_4(sc, R92C_MAR + 0, 0xffffffff);
	urtwn_write_4(sc, R92C_MAR + 4, 0xffffffff);
	/* Accept all management frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP0, 0xffff);
	/* Reject all control frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);
	/* Accept all data frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);
}

static void
urtwn_edca_init(struct urtwn_softc *sc)
{
	urtwn_write_2(sc, R92C_SPEC_SIFS, 0x100a);
	urtwn_write_2(sc, R92C_MAC_SPEC_SIFS, 0x100a);
	urtwn_write_2(sc, R92C_SIFS_CCK, 0x100a);
	urtwn_write_2(sc, R92C_SIFS_OFDM, 0x100a);
	urtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x005ea42b);
	urtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a44f);
	urtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005ea324);
	urtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002fa226);
}

void
urtwn_write_txpower(struct urtwn_softc *sc, int chain,
    uint16_t power[URTWN_RIDX_COUNT])
{
	uint32_t reg;

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = urtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[0]);
		urtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[2]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[3]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[0]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[2]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[3]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	urtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[ 4]) |
	    SM(R92C_TXAGC_RATE09, power[ 5]) |
	    SM(R92C_TXAGC_RATE12, power[ 6]) |
	    SM(R92C_TXAGC_RATE18, power[ 7]));
	urtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[ 8]) |
	    SM(R92C_TXAGC_RATE36, power[ 9]) |
	    SM(R92C_TXAGC_RATE48, power[10]) |
	    SM(R92C_TXAGC_RATE54, power[11]));
	/* Write per-MCS Tx power. */
	urtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[12]) |
	    SM(R92C_TXAGC_MCS01,  power[13]) |
	    SM(R92C_TXAGC_MCS02,  power[14]) |
	    SM(R92C_TXAGC_MCS03,  power[15]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[16]) |
	    SM(R92C_TXAGC_MCS05,  power[17]) |
	    SM(R92C_TXAGC_MCS06,  power[18]) |
	    SM(R92C_TXAGC_MCS07,  power[19]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
	    SM(R92C_TXAGC_MCS08,  power[20]) |
	    SM(R92C_TXAGC_MCS08,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

void
urtwn_get_txpower(struct urtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[URTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct r92c_rom *rom = &sc->rom;
	uint16_t cckpow, ofdmpow, htpow, diff, max;
	const struct urtwn_txpwr *base;
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
	if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			base = &rtl8188ru_txagc[chain];
		else
			base = &rtl8192cu_txagc[chain];
	} else
		base = &rtl8192cu_txagc[chain];

	memset(power, 0, URTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = 0; ridx <= 3; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = 4; ridx < URTWN_RIDX_COUNT; ridx++) {
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
	for (ridx = 0; ridx <= 3; ridx++) {
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
	for (ridx = 4; ridx <= 11; ridx++) {
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
	for (ridx = 12; ridx <= 27; ridx++) {
		power[ridx] += htpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
#ifdef URTWN_DEBUG
	if (urtwn_debug >= 4) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = 0; ridx < URTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}

void
urtwn_set_txpower(struct urtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint16_t power[URTWN_RIDX_COUNT];
	int i;

	for (i = 0; i < sc->ntxchains; i++) {
		/* Compute per-rate Tx power values. */
		urtwn_get_txpower(sc, i, c, extc, power);
		/* Write per-rate Tx power values to hardware. */
		urtwn_write_txpower(sc, i, power);
	}
}

static void
urtwn_scan_start(struct ieee80211com *ic)
{
	/* XXX do nothing?  */
}

static void
urtwn_scan_end(struct ieee80211com *ic)
{
	/* XXX do nothing?  */
}

static void
urtwn_set_channel(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_ifp->if_softc;

	URTWN_LOCK(sc);
	urtwn_set_chan(sc, ic->ic_curchan, NULL);
	URTWN_UNLOCK(sc);
}

static void
urtwn_update_mcast(struct ifnet *ifp)
{
	/* XXX do nothing?  */
}

static void
urtwn_set_chan(struct urtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint32_t reg;
	u_int chan;
	int i;

	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan == 0 || chan == IEEE80211_CHAN_ANY) {
		device_printf(sc->sc_dev,
		    "%s: invalid channel %x\n", __func__, chan);
		return;
	}

	/* Set Tx power for this new channel. */
	urtwn_set_txpower(sc, c, extc);

	for (i = 0; i < sc->nrxchains; i++) {
		urtwn_rf_write(sc, i, R92C_RF_CHNLBW,
		    RW(sc->rf_chnlbw[i], R92C_RF_CHNLBW_CHNL, chan));
	}
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		/* Is secondary channel below or above primary? */
		int prichlo = c->ic_freq < extc->ic_freq;

		urtwn_write_1(sc, R92C_BWOPMODE,
		    urtwn_read_1(sc, R92C_BWOPMODE) & ~R92C_BWOPMODE_20MHZ);

		reg = urtwn_read_1(sc, R92C_RRSR + 2);
		reg = (reg & ~0x6f) | (prichlo ? 1 : 2) << 5;
		urtwn_write_1(sc, R92C_RRSR + 2, reg);

		urtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA0_RFMOD) | R92C_RFMOD_40MHZ);
		urtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA1_RFMOD) | R92C_RFMOD_40MHZ);

		/* Set CCK side band. */
		reg = urtwn_bb_read(sc, R92C_CCK0_SYSTEM);
		reg = (reg & ~0x00000010) | (prichlo ? 0 : 1) << 4;
		urtwn_bb_write(sc, R92C_CCK0_SYSTEM, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM1_LSTF);
		reg = (reg & ~0x00000c00) | (prichlo ? 1 : 2) << 10;
		urtwn_bb_write(sc, R92C_OFDM1_LSTF, reg);

		urtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    urtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) &
		    ~R92C_FPGA0_ANAPARAM2_CBW20);

		reg = urtwn_bb_read(sc, 0x818);
		reg = (reg & ~0x0c000000) | (prichlo ? 2 : 1) << 26;
		urtwn_bb_write(sc, 0x818, reg);

		/* Select 40MHz bandwidth. */
		urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | chan);
	} else
#endif
	{
		urtwn_write_1(sc, R92C_BWOPMODE,
		    urtwn_read_1(sc, R92C_BWOPMODE) | R92C_BWOPMODE_20MHZ);

		urtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA0_RFMOD) & ~R92C_RFMOD_40MHZ);
		urtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA1_RFMOD) & ~R92C_RFMOD_40MHZ);

		urtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    urtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) |
		    R92C_FPGA0_ANAPARAM2_CBW20);

		/* Select 20MHz bandwidth. */
		urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | R92C_RF_CHNLBW_BW20 | chan);
	}
}

static void
urtwn_iq_calib(struct urtwn_softc *sc)
{
	/* TODO */
}

static void
urtwn_lc_calib(struct urtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	int i;

	txmode = urtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		urtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = urtwn_rf_read(sc, i, R92C_RF_AC);
			urtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}
	/* Start calibration. */
	urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    urtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	usb_pause_mtx(&sc->sc_mtx, 100);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		urtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++)
			urtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
	} else {
		/* Unblock all Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}

static void
urtwn_init_locked(void *arg)
{
	struct urtwn_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t reg;
	int error;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		urtwn_stop_locked(ifp, 0);

	/* Init firmware commands ring. */
	sc->fwcur = 0;

	/* Allocate Tx/Rx buffers. */
	error = urtwn_alloc_rx_list(sc);
	if (error != 0)
		goto fail;
	
	error = urtwn_alloc_tx_list(sc);
	if (error != 0)
		goto fail;

	/* Power on adapter. */
	error = urtwn_power_on(sc);
	if (error != 0)
		goto fail;

	/* Initialize DMA. */
	error = urtwn_dma_init(sc);
	if (error != 0)
		goto fail;

	/* Set info size in Rx descriptors (in 64-bit words). */
	urtwn_write_1(sc, R92C_RX_DRVINFO_SZ, 4);

	/* Init interrupts. */
	urtwn_write_4(sc, R92C_HISR, 0xffffffff);
	urtwn_write_4(sc, R92C_HIMR, 0xffffffff);

	/* Set MAC address. */
	urtwn_write_region_1(sc, R92C_MACID, IF_LLADDR(ifp),
	    IEEE80211_ADDR_LEN);

	/* Set initial network type. */
	reg = urtwn_read_4(sc, R92C_CR);
	reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
	urtwn_write_4(sc, R92C_CR, reg);

	urtwn_rxfilter_init(sc);

	reg = urtwn_read_4(sc, R92C_RRSR);
	reg = RW(reg, R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_CCK_ONLY_1M);
	urtwn_write_4(sc, R92C_RRSR, reg);

	/* Set short/long retry limits. */
	urtwn_write_2(sc, R92C_RL,
	    SM(R92C_RL_SRL, 0x30) | SM(R92C_RL_LRL, 0x30));

	/* Initialize EDCA parameters. */
	urtwn_edca_init(sc);

	/* Setup rate fallback. */
	urtwn_write_4(sc, R92C_DARFRC + 0, 0x00000000);
	urtwn_write_4(sc, R92C_DARFRC + 4, 0x10080404);
	urtwn_write_4(sc, R92C_RARFRC + 0, 0x04030201);
	urtwn_write_4(sc, R92C_RARFRC + 4, 0x08070605);

	urtwn_write_1(sc, R92C_FWHW_TXQ_CTRL,
	    urtwn_read_1(sc, R92C_FWHW_TXQ_CTRL) |
	    R92C_FWHW_TXQ_CTRL_AMPDU_RTY_NEW);
	/* Set ACK timeout. */
	urtwn_write_1(sc, R92C_ACKTO, 0x40);

	/* Setup USB aggregation. */
	reg = urtwn_read_4(sc, R92C_TDECTRL);
	reg = RW(reg, R92C_TDECTRL_BLK_DESC_NUM, 6);
	urtwn_write_4(sc, R92C_TDECTRL, reg);
	urtwn_write_1(sc, R92C_TRXDMA_CTRL,
	    urtwn_read_1(sc, R92C_TRXDMA_CTRL) |
	    R92C_TRXDMA_CTRL_RXDMA_AGG_EN);
	urtwn_write_1(sc, R92C_USB_SPECIAL_OPTION,
	    urtwn_read_1(sc, R92C_USB_SPECIAL_OPTION) |
	    R92C_USB_SPECIAL_OPTION_AGG_EN);
	urtwn_write_1(sc, R92C_RXDMA_AGG_PG_TH, 48);
	urtwn_write_1(sc, R92C_USB_DMA_AGG_TO, 4);
	urtwn_write_1(sc, R92C_USB_AGG_TH, 8);
	urtwn_write_1(sc, R92C_USB_AGG_TO, 6);

	/* Initialize beacon parameters. */
	urtwn_write_2(sc, R92C_TBTT_PROHIBIT, 0x6404);
	urtwn_write_1(sc, R92C_DRVERLYINT, 0x05);
	urtwn_write_1(sc, R92C_BCNDMATIM, 0x02);
	urtwn_write_2(sc, R92C_BCNTCFG, 0x660f);

	/* Setup AMPDU aggregation. */
	urtwn_write_4(sc, R92C_AGGLEN_LMT, 0x99997631);	/* MCS7~0 */
	urtwn_write_1(sc, R92C_AGGR_BREAK_TIME, 0x16);
	urtwn_write_2(sc, 0x4ca, 0x0708);

	urtwn_write_1(sc, R92C_BCN_MAX_ERR, 0xff);
	urtwn_write_1(sc, R92C_BCN_CTRL, R92C_BCN_CTRL_DIS_TSF_UDT0);

	/* Load 8051 microcode. */
	error = urtwn_load_firmware(sc);
	if (error != 0)
		goto fail;

	/* Initialize MAC/BB/RF blocks. */
	urtwn_mac_init(sc);
	urtwn_bb_init(sc);
	urtwn_rf_init(sc);

	/* Turn CCK and OFDM blocks on. */
	reg = urtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_CCK_EN;
	urtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	reg = urtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_OFDM_EN;
	urtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);

	/* Clear per-station keys table. */
	urtwn_cam_init(sc);

	/* Enable hardware sequence numbering. */
	urtwn_write_1(sc, R92C_HWSEQ_CTRL, 0xff);

	/* Perform LO and IQ calibrations. */
	urtwn_iq_calib(sc);
	/* Perform LC calibration. */
	urtwn_lc_calib(sc);

	/* Fix USB interference issue. */
	urtwn_write_1(sc, 0xfe40, 0xe0);
	urtwn_write_1(sc, 0xfe41, 0x8d);
	urtwn_write_1(sc, 0xfe42, 0x80);

	urtwn_pa_bias_init(sc);

	/* Initialize GPIO setting. */
	urtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    urtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);

	/* Fix for lower temperature. */
	urtwn_write_1(sc, 0x15, 0xe9);

	usbd_transfer_start(sc->sc_xfer[URTWN_BULK_RX]);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);
fail:
	return;
}

static void
urtwn_init(void *arg)
{
	struct urtwn_softc *sc = arg;

	URTWN_LOCK(sc);
	urtwn_init_locked(arg);
	URTWN_UNLOCK(sc);
}

static void
urtwn_stop_locked(struct ifnet *ifp, int disable)
{
	struct urtwn_softc *sc = ifp->if_softc;

	(void)disable;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->sc_watchdog_ch);
	urtwn_abort_xfers(sc);
}

static void
urtwn_stop(struct ifnet *ifp, int disable)
{
	struct urtwn_softc *sc = ifp->if_softc;

	URTWN_LOCK(sc);
	urtwn_stop_locked(ifp, disable);
	URTWN_UNLOCK(sc);
}

static void
urtwn_abort_xfers(struct urtwn_softc *sc)
{
	int i;

	URTWN_ASSERT_LOCKED(sc);

	/* abort any pending transfers */
	for (i = 0; i < URTWN_N_TRANSFER; i++)
		usbd_transfer_stop(sc->sc_xfer[i]);
}

static int
urtwn_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct urtwn_softc *sc = ifp->if_softc;
	struct urtwn_data *bf;

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return (ENETDOWN);
	}
	URTWN_LOCK(sc);
	bf = urtwn_getbuf(sc);
	if (bf == NULL) {
		ieee80211_free_node(ni);
		m_freem(m);
		URTWN_UNLOCK(sc);
		return (ENOBUFS);
	}

	ifp->if_opackets++;
	if (urtwn_tx_start(sc, ni, m, bf) != 0) {
		ieee80211_free_node(ni);
		ifp->if_oerrors++;
		STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, bf, next);
		URTWN_UNLOCK(sc);
		return (EIO);
	}
	URTWN_UNLOCK(sc);

	sc->sc_txtimer = 5;
	return (0);
}

static device_method_t urtwn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		urtwn_match),
	DEVMETHOD(device_attach,	urtwn_attach),
	DEVMETHOD(device_detach,	urtwn_detach),

	{ 0, 0 }
};

static driver_t urtwn_driver = {
	"urtwn",
	urtwn_methods,
	sizeof(struct urtwn_softc)
};

static devclass_t urtwn_devclass;

DRIVER_MODULE(urtwn, uhub, urtwn_driver, urtwn_devclass, NULL, NULL);
MODULE_DEPEND(urtwn, usb, 1, 1, 1);
MODULE_DEPEND(urtwn, wlan, 1, 1, 1);
MODULE_DEPEND(urtwn, firmware, 1, 1, 1);
MODULE_VERSION(urtwn, 1);
