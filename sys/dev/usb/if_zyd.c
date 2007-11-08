/*	$OpenBSD: if_zyd.c,v 1.52 2007/02/11 00:08:04 jsg Exp $	*/
/*	$NetBSD: if_zyd.c,v 1.7 2007/06/21 04:04:29 kiyohara Exp $	*/
/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2006 by Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 by Florian Stoehr <ich@florian-stoehr.de>
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

/*
 * ZyDAS ZD1211/ZD1211B USB WLAN driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <sys/linker.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_proto.h>
#include <net80211/ieee80211_node.h>
#include <net80211/ieee80211_regdomain.h>

#include <net/bpf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"
#include <dev/usb/usb_ethersubr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/if_zydreg.h>
#include <dev/usb/if_zydfw.h>

#ifdef USB_DEBUG
#define ZYD_DEBUG
#endif

#ifdef ZYD_DEBUG
#define DPRINTF(x)	do { if (zyddebug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (zyddebug > (n)) printf x; } while (0)
int zyddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static const struct zyd_phy_pair zyd_def_phy[] = ZYD_DEF_PHY;
static const struct zyd_phy_pair zyd_def_phyB[] = ZYD_DEF_PHYB;

/* various supported device vendors/products */
#define ZYD_ZD1211_DEV(v, p)	\
	{ { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }, ZYD_ZD1211 }
#define ZYD_ZD1211B_DEV(v, p)	\
	{ { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }, ZYD_ZD1211B }
static const struct zyd_type {
	struct usb_devno	dev;
	uint8_t			rev;
#define ZYD_ZD1211	0
#define ZYD_ZD1211B	1
} zyd_devs[] = {
	ZYD_ZD1211_DEV(3COM2,		3CRUSB10075),
	ZYD_ZD1211_DEV(ABOCOM,		WL54),
	ZYD_ZD1211_DEV(ASUS,		WL159G),
	ZYD_ZD1211_DEV(CYBERTAN,	TG54USB),
	ZYD_ZD1211_DEV(DRAYTEK,		VIGOR550),
	ZYD_ZD1211_DEV(PLANEX2,		GWUS54GD),
	ZYD_ZD1211_DEV(PLANEX2,		GWUS54GZL),
	ZYD_ZD1211_DEV(PLANEX3,		GWUS54GZ),
	ZYD_ZD1211_DEV(PLANEX3,		GWUS54MINI),
	ZYD_ZD1211_DEV(SAGEM,		XG760A),
	ZYD_ZD1211_DEV(SENAO,		NUB8301),
	ZYD_ZD1211_DEV(SITECOMEU,	WL113),
	ZYD_ZD1211_DEV(SWEEX,		ZD1211),
	ZYD_ZD1211_DEV(TEKRAM,		QUICKWLAN),
	ZYD_ZD1211_DEV(TEKRAM,		ZD1211_1),
	ZYD_ZD1211_DEV(TEKRAM,		ZD1211_2),
	ZYD_ZD1211_DEV(TWINMOS,		G240),
	ZYD_ZD1211_DEV(UMEDIA,		ALL0298V2),
	ZYD_ZD1211_DEV(UMEDIA,		TEW429UB_A),
	ZYD_ZD1211_DEV(UMEDIA,		TEW429UB),
	ZYD_ZD1211_DEV(WISTRONNEWEB,	UR055G),
	ZYD_ZD1211_DEV(ZCOM,		ZD1211),
	ZYD_ZD1211_DEV(ZYDAS,		ZD1211),
	ZYD_ZD1211_DEV(ZYXEL,		AG225H),
	ZYD_ZD1211_DEV(ZYXEL,		ZYAIRG220),
	ZYD_ZD1211_DEV(ZYXEL,		G200V2),

	ZYD_ZD1211B_DEV(ACCTON,		SMCWUSBG),
	ZYD_ZD1211B_DEV(ACCTON,		ZD1211B),
	ZYD_ZD1211B_DEV(ASUS,		A9T_WIFI),
	ZYD_ZD1211B_DEV(BELKIN,		F5D7050_V4000),
	ZYD_ZD1211B_DEV(BELKIN,		ZD1211B),
	ZYD_ZD1211B_DEV(CISCOLINKSYS,	WUSBF54G),
	ZYD_ZD1211B_DEV(FIBERLINE,	WL430U),
	ZYD_ZD1211B_DEV(MELCO,		KG54L),
	ZYD_ZD1211B_DEV(PHILIPS,	SNU5600),
	ZYD_ZD1211B_DEV(SAGEM,		XG76NA),
	ZYD_ZD1211B_DEV(SITECOMEU,	ZD1211B),
	ZYD_ZD1211B_DEV(UMEDIA,		TEW429UBC1),
#if 0	/* Shall we needs? */
	ZYD_ZD1211B_DEV(UNKNOWN1,	ZD1211B_1),
	ZYD_ZD1211B_DEV(UNKNOWN1,	ZD1211B_2),
	ZYD_ZD1211B_DEV(UNKNOWN2,	ZD1211B),
	ZYD_ZD1211B_DEV(UNKNOWN3,	ZD1211B),
#endif
	ZYD_ZD1211B_DEV(USR,		USR5423),
	ZYD_ZD1211B_DEV(VTECH,		ZD1211B),
	ZYD_ZD1211B_DEV(ZCOM,		ZD1211B),
	ZYD_ZD1211B_DEV(ZYDAS,		ZD1211B),
	ZYD_ZD1211B_DEV(ZYXEL,		M202),
	ZYD_ZD1211B_DEV(ZYXEL,		G220V2),
};
#define zyd_lookup(v, p)	\
	((const struct zyd_type *)usb_lookup(zyd_devs, v, p))

static device_probe_t zyd_match;
static device_attach_t zyd_attach;
static device_detach_t zyd_detach;

static int	zyd_attachhook(struct zyd_softc *);
static int	zyd_complete_attach(struct zyd_softc *);
static int	zyd_open_pipes(struct zyd_softc *);
static void	zyd_close_pipes(struct zyd_softc *);
static int	zyd_alloc_tx_list(struct zyd_softc *);
static void	zyd_free_tx_list(struct zyd_softc *);
static int	zyd_alloc_rx_list(struct zyd_softc *);
static void	zyd_free_rx_list(struct zyd_softc *);
static struct	ieee80211_node *zyd_node_alloc(struct ieee80211_node_table *);
static int	zyd_media_change(struct ifnet *);
static void	zyd_task(void *);
static int	zyd_newstate(struct ieee80211com *, enum ieee80211_state, int);
static int	zyd_cmd(struct zyd_softc *, uint16_t, const void *, int,
		    void *, int, u_int);
static int	zyd_read16(struct zyd_softc *, uint16_t, uint16_t *);
static int	zyd_read32(struct zyd_softc *, uint16_t, uint32_t *);
static int	zyd_write16(struct zyd_softc *, uint16_t, uint16_t);
static int	zyd_write32(struct zyd_softc *, uint16_t, uint32_t);
static int	zyd_rfwrite(struct zyd_softc *, uint32_t);
static void	zyd_lock_phy(struct zyd_softc *);
static void	zyd_unlock_phy(struct zyd_softc *);
static int	zyd_rfmd_init(struct zyd_rf *);
static int	zyd_rfmd_switch_radio(struct zyd_rf *, int);
static int	zyd_rfmd_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_al2230_init(struct zyd_rf *);
static int	zyd_al2230_switch_radio(struct zyd_rf *, int);
static int	zyd_al2230_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_al2230_init_b(struct zyd_rf *);
static int	zyd_al7230B_init(struct zyd_rf *);
static int	zyd_al7230B_switch_radio(struct zyd_rf *, int);
static int	zyd_al7230B_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_al2210_init(struct zyd_rf *);
static int	zyd_al2210_switch_radio(struct zyd_rf *, int);
static int	zyd_al2210_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_gct_init(struct zyd_rf *);
static int	zyd_gct_switch_radio(struct zyd_rf *, int);
static int	zyd_gct_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_maxim_init(struct zyd_rf *);
static int	zyd_maxim_switch_radio(struct zyd_rf *, int);
static int	zyd_maxim_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_maxim2_init(struct zyd_rf *);
static int	zyd_maxim2_switch_radio(struct zyd_rf *, int);
static int	zyd_maxim2_set_channel(struct zyd_rf *, uint8_t);
static int	zyd_rf_attach(struct zyd_softc *, uint8_t);
static const char *zyd_rf_name(uint8_t);
static int	zyd_hw_init(struct zyd_softc *);
static int	zyd_read_eeprom(struct zyd_softc *);
static int	zyd_set_macaddr(struct zyd_softc *, const uint8_t *);
static int	zyd_set_bssid(struct zyd_softc *, const uint8_t *);
static int	zyd_switch_radio(struct zyd_softc *, int);
static void	zyd_set_led(struct zyd_softc *, int, int);
static void	zyd_set_multi(struct zyd_softc *);
static int	zyd_set_rxfilter(struct zyd_softc *);
static void	zyd_set_chan(struct zyd_softc *, struct ieee80211_channel *);
static int	zyd_set_beacon_interval(struct zyd_softc *, int);
static uint8_t	zyd_plcp_signal(int);
static void	zyd_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	zyd_rx_data(struct zyd_softc *, const uint8_t *, uint16_t);
static void	zyd_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	zyd_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static int	zyd_tx_mgt(struct zyd_softc *, struct mbuf *,
		    struct ieee80211_node *);
static int	zyd_tx_data(struct zyd_softc *, struct mbuf *,
		    struct ieee80211_node *);
static void	zyd_start(struct ifnet *);
static void	zyd_watchdog(void *);
static int	zyd_ioctl(struct ifnet *, u_long, caddr_t);
static void	zyd_init(void *);
static void	zyd_stop(struct zyd_softc *, int);
static int	zyd_loadfirmware(struct zyd_softc *, u_char *, size_t);
static void	zyd_iter_func(void *, struct ieee80211_node *);
static void	zyd_amrr_timeout(void *);
static void	zyd_newassoc(struct ieee80211_node *, int);
static void	zyd_scantask(void *);
static void	zyd_scan_start(struct ieee80211com *);
static void	zyd_scan_end(struct ieee80211com *);
static void	zyd_set_channel(struct ieee80211com *);

static int
zyd_match(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (!uaa->iface)
		return UMATCH_NONE;

	return (zyd_lookup(uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

static int
zyd_attachhook(struct zyd_softc *sc)
{
	u_char *firmware;
	int len, error;

	if (sc->mac_rev == ZYD_ZD1211) {
		firmware = (u_char *)zd1211_firmware;
		len = sizeof(zd1211_firmware);
	} else {
		firmware = (u_char *)zd1211b_firmware;
		len = sizeof(zd1211b_firmware);
	}

	error = zyd_loadfirmware(sc, firmware, len);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not load firmware (error=%d)\n", error);
		return error;
	}

	sc->sc_flags |= ZD1211_FWLOADED;

	/* complete the attach process */
	return zyd_complete_attach(sc);
}

static int
zyd_attach(device_t dev)
{
	int error = ENXIO;
	struct zyd_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usb_device_descriptor_t* ddesc;
	struct ifnet *ifp;

	sc->sc_dev = dev;

	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		return ENXIO;
	}

	sc->sc_udev = uaa->device;
	sc->sc_flags = 0;
	sc->mac_rev = zyd_lookup(uaa->vendor, uaa->product)->rev;

	ddesc = usbd_get_device_descriptor(sc->sc_udev);
	if (UGETW(ddesc->bcdDevice) < 0x4330) {
		device_printf(dev, "device version mismatch: 0x%x "
		    "(only >= 43.30 supported)\n",
		    UGETW(ddesc->bcdDevice));
		goto bad;
	}

	ifp->if_softc = sc;
	if_initname(ifp, "zyd", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT; /* USB stack is still under Giant lock */
	ifp->if_init = zyd_init;
	ifp->if_ioctl = zyd_ioctl;
	ifp->if_start = zyd_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	STAILQ_INIT(&sc->sc_rqh);

	error = zyd_attachhook(sc);
	if (error != 0) {
bad:
		if_free(ifp);
		return error;
	}

	return 0;
}

static int
zyd_complete_attach(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = sc->sc_ifp;
	usbd_status error;
	int bands;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	usb_init_task(&sc->sc_scantask, zyd_scantask, sc);
	usb_init_task(&sc->sc_task, zyd_task, sc);

	callout_init(&sc->sc_amrr_ch, 0);
	callout_init(&sc->sc_watchdog_ch, 0);

	error = usbd_set_config_no(sc->sc_udev, ZYD_CONFIG_NO, 1);
	if (error != 0) {
		device_printf(sc->sc_dev, "setting config no failed\n");
		error = ENXIO;
		goto fail;
	}

	error = usbd_device2interface_handle(sc->sc_udev, ZYD_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		device_printf(sc->sc_dev, "getting interface handle failed\n");
		error = ENXIO;
		goto fail;
	}

	if ((error = zyd_open_pipes(sc)) != 0) {
		device_printf(sc->sc_dev, "could not open pipes\n");
		goto fail;
	}

	if ((error = zyd_read_eeprom(sc)) != 0) {
		device_printf(sc->sc_dev, "could not read EEPROM\n");
		goto fail;
	}

	if ((error = zyd_rf_attach(sc, sc->rf_rev)) != 0) {
		device_printf(sc->sc_dev, "could not attach RF, rev 0x%x\n",
		    sc->rf_rev);
		goto fail;
	}

	if ((error = zyd_hw_init(sc)) != 0) {
		device_printf(sc->sc_dev, "hardware initialization failed\n");
		goto fail;
	}

	device_printf(sc->sc_dev,
	    "HMAC ZD1211%s, FW %02x.%02x, RF %s, PA %x, address %s\n",
	    (sc->mac_rev == ZYD_ZD1211) ? "": "B",
	    sc->fw_rev >> 8, sc->fw_rev & 0xff, zyd_rf_name(sc->rf_rev),
	    sc->pa_rev, ether_sprintf(ic->ic_myaddr));

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
		  IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	        | IEEE80211_C_SHSLOT	/* short slot time supported */
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
	        | IEEE80211_C_WPA		/* 802.11i */
		;

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, 0, CTRY_DEFAULT, bands, 0, 1);

	ieee80211_ifattach(ic);
	ic->ic_node_alloc = zyd_node_alloc;
	ic->ic_newassoc = zyd_newassoc;

	/* enable s/w bmiss handling in sta mode */
	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;
	ic->ic_scan_start = zyd_scan_start;
	ic->ic_scan_end = zyd_scan_end;
	ic->ic_set_channel = zyd_set_channel;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = zyd_newstate;
	ieee80211_media_init(ic, zyd_media_change, ieee80211_media_status);
	ieee80211_amrr_init(&sc->amrr, ic,
	    IEEE80211_AMRR_MIN_SUCCESS_THRESHOLD,
	    IEEE80211_AMRR_MAX_SUCCESS_THRESHOLD);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + sizeof(sc->sc_txtap),
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtap);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(ZYD_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtap);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(ZYD_TX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

	return error;

fail:
	mtx_destroy(&sc->sc_mtx);

	return error;
}

static int
zyd_detach(device_t dev)
{
	struct zyd_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = sc->sc_ifp;

	if (!device_is_attached(dev))
		return 0;

	/* protect a race when we have listeners related with the driver.  */
	ifp->if_flags &= ~IFF_UP;

	zyd_stop(sc, 1);
	usb_rem_task(sc->sc_udev, &sc->sc_scantask);
	usb_rem_task(sc->sc_udev, &sc->sc_task);
	callout_stop(&sc->sc_amrr_ch);
	callout_stop(&sc->sc_watchdog_ch);

	zyd_close_pipes(sc);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);
	if_free(ifp);

	mtx_destroy(&sc->sc_mtx);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return 0;
}

static int
zyd_open_pipes(struct zyd_softc *sc)
{
	usb_endpoint_descriptor_t *edesc;
	int isize;
	usbd_status error;

	/* interrupt in */
	edesc = usbd_get_endpoint_descriptor(sc->sc_iface, 0x83);
	if (edesc == NULL)
		return EINVAL;

	isize = UGETW(edesc->wMaxPacketSize);
	if (isize == 0)	/* should not happen */
		return EINVAL;

	sc->ibuf = malloc(isize, M_USBDEV, M_NOWAIT);
	if (sc->ibuf == NULL)
		return ENOMEM;

	error = usbd_open_pipe_intr(sc->sc_iface, 0x83, USBD_SHORT_XFER_OK,
	    &sc->zyd_ep[ZYD_ENDPT_IIN], sc, sc->ibuf, isize, zyd_intr,
	    USBD_DEFAULT_INTERVAL);
	if (error != 0) {
		device_printf(sc->sc_dev, "open rx intr pipe failed: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	/* interrupt out (not necessarily an interrupt pipe) */
	error = usbd_open_pipe(sc->sc_iface, 0x04, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_IOUT]);
	if (error != 0) {
		device_printf(sc->sc_dev, "open tx intr pipe failed: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	/* bulk in */
	error = usbd_open_pipe(sc->sc_iface, 0x82, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_BIN]);
	if (error != 0) {
		device_printf(sc->sc_dev, "open rx pipe failed: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	/* bulk out */
	error = usbd_open_pipe(sc->sc_iface, 0x01, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_BOUT]);
	if (error != 0) {
		device_printf(sc->sc_dev, "open tx pipe failed: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	return 0;

fail:	zyd_close_pipes(sc);
	return ENXIO;
}

static void
zyd_close_pipes(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_ENDPT_CNT; i++) {
		if (sc->zyd_ep[i] != NULL) {
			usbd_abort_pipe(sc->zyd_ep[i]);
			usbd_close_pipe(sc->zyd_ep[i]);
			sc->zyd_ep[i] = NULL;
		}
	}
	if (sc->ibuf != NULL) {
		free(sc->ibuf, M_USBDEV);
		sc->ibuf = NULL;
	}
}

static int
zyd_alloc_tx_list(struct zyd_softc *sc)
{
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		struct zyd_tx_data *data = &sc->tx_data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate tx xfer\n");
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ZYD_MAX_TXBUFSZ);
		if (data->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate tx buffer\n");
			error = ENOMEM;
			goto fail;
		}

		/* clear Tx descriptor */
		bzero(data->buf, sizeof(struct zyd_tx_desc));
	}
	return 0;

fail:	zyd_free_tx_list(sc);
	return error;
}

static void
zyd_free_tx_list(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		struct zyd_tx_data *data = &sc->tx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}
}

static int
zyd_alloc_rx_list(struct zyd_softc *sc)
{
	int i, error;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx xfer\n");
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ZYX_MAX_RXBUFSZ);
		if (data->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx buffer\n");
			error = ENOMEM;
			goto fail;
		}
	}
	return 0;

fail:	zyd_free_rx_list(sc);
	return error;
}

static void
zyd_free_rx_list(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
	}
}

/* ARGUSED */
static struct ieee80211_node *
zyd_node_alloc(struct ieee80211_node_table *nt __unused)
{
	struct zyd_node *zn;

	zn = malloc(sizeof(struct zyd_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	return zn != NULL ? &zn->ni : NULL;
}

static int
zyd_media_change(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & IFF_UP) == IFF_UP &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == IFF_DRV_RUNNING)
		zyd_init(sc);

	return 0;
}

static void
zyd_task(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;

	switch (sc->sc_state) {
	case IEEE80211_S_RUN:
	{
		struct ieee80211_node *ni = ic->ic_bss;

		zyd_set_chan(sc, ic->ic_curchan);

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			/* turn link LED on */
			zyd_set_led(sc, ZYD_LED1, 1);

			/* make data LED blink upon Tx */
			zyd_write32(sc, sc->fwbase + ZYD_FW_LINK_STATUS, 1);

			zyd_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* fake a join to init the tx rate */
			zyd_newassoc(ni, 1);
		}

		/* start automatic rate control timer */
		if (ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE)
			callout_reset(&sc->sc_amrr_ch, hz,
			    zyd_amrr_timeout, sc);

		break;
	}
	default:
		break;
	}

	sc->sc_newstate(ic, sc->sc_state, sc->sc_arg);
}

static int
zyd_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	callout_stop(&sc->sc_amrr_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;

	if (nstate == IEEE80211_S_INIT)
		sc->sc_newstate(ic, nstate, arg);
	else
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);

	return 0;
}

static int
zyd_cmd(struct zyd_softc *sc, uint16_t code, const void *idata, int ilen,
    void *odata, int olen, u_int flags)
{
	usbd_xfer_handle xfer;
	struct zyd_cmd cmd;
	struct rq rq;
	uint16_t xferflags;
	usbd_status error;

	if ((xfer = usbd_alloc_xfer(sc->sc_udev)) == NULL)
		return ENOMEM;

	cmd.code = htole16(code);
	bcopy(idata, cmd.data, ilen);

	xferflags = USBD_FORCE_SHORT_XFER;
	if (!(flags & ZYD_CMD_FLAG_READ))
		xferflags |= USBD_SYNCHRONOUS;
	else {
		rq.idata = idata;
		rq.odata = odata;
		rq.len = olen / sizeof(struct zyd_pair);
		STAILQ_INSERT_TAIL(&sc->sc_rqh, &rq, rq);
	}

	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_IOUT], 0, &cmd,
	    sizeof(uint16_t) + ilen, xferflags, ZYD_INTR_TIMEOUT, NULL);
	error = usbd_transfer(xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		device_printf(sc->sc_dev, "could not send command (error=%s)\n",
		    usbd_errstr(error));
		(void)usbd_free_xfer(xfer);
		return EIO;
	}
	if (!(flags & ZYD_CMD_FLAG_READ)) {
		(void)usbd_free_xfer(xfer);
		return 0;	/* write: don't wait for reply */
	}
	/* wait at most one second for command reply */
	error = tsleep(odata, PCATCH, "zydcmd", hz);
	if (error == EWOULDBLOCK)
		device_printf(sc->sc_dev, "zyd_read sleep timeout\n");
	STAILQ_REMOVE(&sc->sc_rqh, &rq, rq, rq);

	(void)usbd_free_xfer(xfer);
	return error;
}

static int
zyd_read16(struct zyd_softc *sc, uint16_t reg, uint16_t *val)
{
	struct zyd_pair tmp;
	int error;

	reg = htole16(reg);
	error = zyd_cmd(sc, ZYD_CMD_IORD, &reg, sizeof(reg), &tmp, sizeof(tmp),
	    ZYD_CMD_FLAG_READ);
	if (error == 0)
		*val = le16toh(tmp.val);
	return error;
}

static int
zyd_read32(struct zyd_softc *sc, uint16_t reg, uint32_t *val)
{
	struct zyd_pair tmp[2];
	uint16_t regs[2];
	int error;

	regs[0] = htole16(ZYD_REG32_HI(reg));
	regs[1] = htole16(ZYD_REG32_LO(reg));
	error = zyd_cmd(sc, ZYD_CMD_IORD, regs, sizeof(regs), tmp, sizeof(tmp),
	    ZYD_CMD_FLAG_READ);
	if (error == 0)
		*val = le16toh(tmp[0].val) << 16 | le16toh(tmp[1].val);
	return error;
}

static int
zyd_write16(struct zyd_softc *sc, uint16_t reg, uint16_t val)
{
	struct zyd_pair pair;

	pair.reg = htole16(reg);
	pair.val = htole16(val);

	return zyd_cmd(sc, ZYD_CMD_IOWR, &pair, sizeof(pair), NULL, 0, 0);
}

static int
zyd_write32(struct zyd_softc *sc, uint16_t reg, uint32_t val)
{
	struct zyd_pair pair[2];

	pair[0].reg = htole16(ZYD_REG32_HI(reg));
	pair[0].val = htole16(val >> 16);
	pair[1].reg = htole16(ZYD_REG32_LO(reg));
	pair[1].val = htole16(val & 0xffff);

	return zyd_cmd(sc, ZYD_CMD_IOWR, pair, sizeof(pair), NULL, 0, 0);
}

static int
zyd_rfwrite(struct zyd_softc *sc, uint32_t val)
{
	struct zyd_rf *rf = &sc->sc_rf;
	struct zyd_rfwrite req;
	uint16_t cr203;
	int i;

	(void)zyd_read16(sc, ZYD_CR203, &cr203);
	cr203 &= ~(ZYD_RF_IF_LE | ZYD_RF_CLK | ZYD_RF_DATA);

	req.code  = htole16(2);
	req.width = htole16(rf->width);
	for (i = 0; i < rf->width; i++) {
		req.bit[i] = htole16(cr203);
		if (val & (1 << (rf->width - 1 - i)))
			req.bit[i] |= htole16(ZYD_RF_DATA);
	}
	return zyd_cmd(sc, ZYD_CMD_RFCFG, &req, 4 + 2 * rf->width, NULL, 0, 0);
}

static void
zyd_lock_phy(struct zyd_softc *sc)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_MISC, &tmp);
	tmp &= ~ZYD_UNLOCK_PHY_REGS;
	(void)zyd_write32(sc, ZYD_MAC_MISC, tmp);
}

static void
zyd_unlock_phy(struct zyd_softc *sc)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_MISC, &tmp);
	tmp |= ZYD_UNLOCK_PHY_REGS;
	(void)zyd_write32(sc, ZYD_MAC_MISC, tmp);
}

/*
 * RFMD RF methods.
 */
static int
zyd_rfmd_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_RFMD_PHY;
	static const uint32_t rfini[] = ZYD_RFMD_RF;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init RFMD radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
#undef N
}

static int
zyd_rfmd_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;

	(void)zyd_write16(sc, ZYD_CR10, on ? 0x89 : 0x15);
	(void)zyd_write16(sc, ZYD_CR11, on ? 0x00 : 0x81);

	return 0;
}

static int
zyd_rfmd_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_RFMD_CHANTABLE;

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	return 0;
}

/*
 * AL2230 RF methods.
 */
static int
zyd_al2230_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY;
	static const uint32_t rfini[] = ZYD_AL2230_RF;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init AL2230 radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
#undef N
}

static int
zyd_al2230_init_b(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY_B;
	static const uint32_t rfini[] = ZYD_AL2230_RF_B;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init AL2230 radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
#undef N
}

static int
zyd_al2230_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;
	int on251 = (sc->mac_rev == ZYD_ZD1211) ? 0x3f : 0x7f;

	(void)zyd_write16(sc, ZYD_CR11,  on ? 0x00 : 0x04);
	(void)zyd_write16(sc, ZYD_CR251, on ? on251 : 0x2f);

	return 0;
}

static int
zyd_al2230_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2, r3;
	} rfprog[] = ZYD_AL2230_CHANTABLE;

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r3);

	(void)zyd_write16(sc, ZYD_CR138, 0x28);
	(void)zyd_write16(sc, ZYD_CR203, 0x06);

	return 0;
}

/*
 * AL7230B RF methods.
 */
static int
zyd_al7230B_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini_1[] = ZYD_AL7230B_PHY_1;
	static const struct zyd_phy_pair phyini_2[] = ZYD_AL7230B_PHY_2;
	static const struct zyd_phy_pair phyini_3[] = ZYD_AL7230B_PHY_3;
	static const uint32_t rfini_1[] = ZYD_AL7230B_RF_1;
	static const uint32_t rfini_2[] = ZYD_AL7230B_RF_2;
	int i, error;

	/* for AL7230B, PHY and RF need to be initialized in "phases" */

	/* init RF-dependent PHY registers, part one */
	for (i = 0; i < N(phyini_1); i++) {
		error = zyd_write16(sc, phyini_1[i].reg, phyini_1[i].val);
		if (error != 0)
			return error;
	}
	/* init AL7230B radio, part one */
	for (i = 0; i < N(rfini_1); i++) {
		if ((error = zyd_rfwrite(sc, rfini_1[i])) != 0)
			return error;
	}
	/* init RF-dependent PHY registers, part two */
	for (i = 0; i < N(phyini_2); i++) {
		error = zyd_write16(sc, phyini_2[i].reg, phyini_2[i].val);
		if (error != 0)
			return error;
	}
	/* init AL7230B radio, part two */
	for (i = 0; i < N(rfini_2); i++) {
		if ((error = zyd_rfwrite(sc, rfini_2[i])) != 0)
			return error;
	}
	/* init RF-dependent PHY registers, part three */
	for (i = 0; i < N(phyini_3); i++) {
		error = zyd_write16(sc, phyini_3[i].reg, phyini_3[i].val);
		if (error != 0)
			return error;
	}

	return 0;
#undef N
}

static int
zyd_al7230B_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;

	(void)zyd_write16(sc, ZYD_CR11,  on ? 0x00 : 0x04);
	(void)zyd_write16(sc, ZYD_CR251, on ? 0x3f : 0x2f);

	return 0;
}

static int
zyd_al7230B_set_channel(struct zyd_rf *rf, uint8_t chan)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_AL7230B_CHANTABLE;
	static const uint32_t rfsc[] = ZYD_AL7230B_RF_SETCHANNEL;
	int i, error;

	(void)zyd_write16(sc, ZYD_CR240, 0x57);
	(void)zyd_write16(sc, ZYD_CR251, 0x2f);

	for (i = 0; i < N(rfsc); i++) {
		if ((error = zyd_rfwrite(sc, rfsc[i])) != 0)
			return error;
	}

	(void)zyd_write16(sc, ZYD_CR128, 0x14);
	(void)zyd_write16(sc, ZYD_CR129, 0x12);
	(void)zyd_write16(sc, ZYD_CR130, 0x10);
	(void)zyd_write16(sc, ZYD_CR38,  0x38);
	(void)zyd_write16(sc, ZYD_CR136, 0xdf);

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);
	(void)zyd_rfwrite(sc, 0x3c9000);

	(void)zyd_write16(sc, ZYD_CR251, 0x3f);
	(void)zyd_write16(sc, ZYD_CR203, 0x06);
	(void)zyd_write16(sc, ZYD_CR240, 0x08);

	return 0;
#undef N
}

/*
 * AL2210 RF methods.
 */
static int
zyd_al2210_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2210_PHY;
	static const uint32_t rfini[] = ZYD_AL2210_RF;
	uint32_t tmp;
	int i, error;

	(void)zyd_write32(sc, ZYD_CR18, 2);

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	/* init AL2210 radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);
	(void)zyd_read32(sc, ZYD_CR_RADIO_PD, &tmp);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp & ~1);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp | 1);
	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x05);
	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x00);
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);
	(void)zyd_write32(sc, ZYD_CR18, 3);

	return 0;
#undef N
}

static int
zyd_al2210_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

static int
zyd_al2210_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const uint32_t rfprog[] = ZYD_AL2210_CHANTABLE;
	uint32_t tmp;

	(void)zyd_write32(sc, ZYD_CR18, 2);
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);
	(void)zyd_read32(sc, ZYD_CR_RADIO_PD, &tmp);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp & ~1);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp | 1);
	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x05);

	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x00);
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);

	/* actually set the channel */
	(void)zyd_rfwrite(sc, rfprog[chan - 1]);

	(void)zyd_write32(sc, ZYD_CR18, 3);

	return 0;
}

/*
 * GCT RF methods.
 */
static int
zyd_gct_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_GCT_PHY;
	static const uint32_t rfini[] = ZYD_GCT_RF;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	/* init cgt radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
#undef N
}

static int
zyd_gct_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

static int
zyd_gct_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const uint32_t rfprog[] = ZYD_GCT_CHANTABLE;

	(void)zyd_rfwrite(sc, 0x1c0000);
	(void)zyd_rfwrite(sc, rfprog[chan - 1]);
	(void)zyd_rfwrite(sc, 0x1c0008);

	return 0;
}

/*
 * Maxim RF methods.
 */
static int
zyd_maxim_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM_RF;
	uint16_t tmp;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* init maxim radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
#undef N
}

static int
zyd_maxim_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

static int
zyd_maxim_set_channel(struct zyd_rf *rf, uint8_t chan)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM_RF;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_MAXIM_CHANTABLE;
	uint16_t tmp;
	int i, error;

	/*
	 * Do the same as we do when initializing it, except for the channel
	 * values coming from the two channel tables.
	 */

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* first two values taken from the chantables */
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	/* init maxim radio - skipping the two first values */
	for (i = 2; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
#undef N
}

/*
 * Maxim2 RF methods.
 */
static int
zyd_maxim2_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM2_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM2_RF;
	uint16_t tmp;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* init maxim2 radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
#undef N
}

static int
zyd_maxim2_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

static int
zyd_maxim2_set_channel(struct zyd_rf *rf, uint8_t chan)
{
#define N(a)	(sizeof(a) / sizeof((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM2_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM2_RF;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_MAXIM2_CHANTABLE;
	uint16_t tmp;
	int i, error;

	/*
	 * Do the same as we do when initializing it, except for the channel
	 * values coming from the two channel tables.
	 */

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* first two values taken from the chantables */
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	/* init maxim2 radio - skipping the two first values */
	for (i = 2; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
#undef N
}

static int
zyd_rf_attach(struct zyd_softc *sc, uint8_t type)
{
	struct zyd_rf *rf = &sc->sc_rf;

	rf->rf_sc = sc;

	switch (type) {
	case ZYD_RF_RFMD:
		rf->init         = zyd_rfmd_init;
		rf->switch_radio = zyd_rfmd_switch_radio;
		rf->set_channel  = zyd_rfmd_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL2230:
		if (sc->mac_rev == ZYD_ZD1211B)
			rf->init = zyd_al2230_init_b;
		else
			rf->init = zyd_al2230_init;
		rf->switch_radio = zyd_al2230_switch_radio;
		rf->set_channel  = zyd_al2230_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL7230B:
		rf->init         = zyd_al7230B_init;
		rf->switch_radio = zyd_al7230B_switch_radio;
		rf->set_channel  = zyd_al7230B_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL2210:
		rf->init         = zyd_al2210_init;
		rf->switch_radio = zyd_al2210_switch_radio;
		rf->set_channel  = zyd_al2210_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_GCT:
		rf->init         = zyd_gct_init;
		rf->switch_radio = zyd_gct_switch_radio;
		rf->set_channel  = zyd_gct_set_channel;
		rf->width        = 21;	/* 21-bit RF values */
		break;
	case ZYD_RF_MAXIM_NEW:
		rf->init         = zyd_maxim_init;
		rf->switch_radio = zyd_maxim_switch_radio;
		rf->set_channel  = zyd_maxim_set_channel;
		rf->width        = 18;	/* 18-bit RF values */
		break;
	case ZYD_RF_MAXIM_NEW2:
		rf->init         = zyd_maxim2_init;
		rf->switch_radio = zyd_maxim2_switch_radio;
		rf->set_channel  = zyd_maxim2_set_channel;
		rf->width        = 18;	/* 18-bit RF values */
		break;
	default:
		device_printf(sc->sc_dev,
		    "sorry, radio \"%s\" is not supported yet\n",
		    zyd_rf_name(type));
		return EINVAL;
	}
	return 0;
}

static const char *
zyd_rf_name(uint8_t type)
{
	static const char * const zyd_rfs[] = {
		"unknown", "unknown", "UW2451",   "UCHIP",     "AL2230",
		"AL7230B", "THETA",   "AL2210",   "MAXIM_NEW", "GCT",
		"PV2000",  "RALINK",  "INTERSIL", "RFMD",      "MAXIM_NEW2",
		"PHILIPS"
	};

	return zyd_rfs[(type > 15) ? 0 : type];
}

static int
zyd_hw_init(struct zyd_softc *sc)
{
	struct zyd_rf *rf = &sc->sc_rf;
	const struct zyd_phy_pair *phyp;
	uint32_t tmp;
	int error;

	/* specify that the plug and play is finished */
	(void)zyd_write32(sc, ZYD_MAC_AFTER_PNP, 1);

	(void)zyd_read16(sc, ZYD_FIRMWARE_BASE_ADDR, &sc->fwbase);
	DPRINTF(("firmware base address=0x%04x\n", sc->fwbase));

	/* retrieve firmware revision number */
	(void)zyd_read16(sc, sc->fwbase + ZYD_FW_FIRMWARE_REV, &sc->fw_rev);

	(void)zyd_write32(sc, ZYD_CR_GPI_EN, 0);
	(void)zyd_write32(sc, ZYD_MAC_CONT_WIN_LIMIT, 0x7f043f);

	/* disable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, 0);

	/* PHY init */
	zyd_lock_phy(sc);
	phyp = (sc->mac_rev == ZYD_ZD1211B) ? zyd_def_phyB : zyd_def_phy;
	for (; phyp->reg != 0; phyp++) {
		if ((error = zyd_write16(sc, phyp->reg, phyp->val)) != 0)
			goto fail;
	}
	if (sc->fix_cr157) {
		if (zyd_read32(sc, ZYD_EEPROM_PHY_REG, &tmp) == 0)
			(void)zyd_write32(sc, ZYD_CR157, tmp >> 8);
	}
	zyd_unlock_phy(sc);

	/* HMAC init */
	zyd_write32(sc, ZYD_MAC_ACK_EXT, 0x00000020);
	zyd_write32(sc, ZYD_CR_ADDA_MBIAS_WT, 0x30000808);

	if (sc->mac_rev == ZYD_ZD1211) {
		zyd_write32(sc, ZYD_MAC_RETRY, 0x00000002);
	} else {
		zyd_write32(sc, ZYD_MACB_MAX_RETRY, 0x02020202);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL4, 0x007f003f);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL3, 0x007f003f);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL2, 0x003f001f);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL1, 0x001f000f);
		zyd_write32(sc, ZYD_MACB_AIFS_CTL1, 0x00280028);
		zyd_write32(sc, ZYD_MACB_AIFS_CTL2, 0x008C003C);
		zyd_write32(sc, ZYD_MACB_TXOP, 0x01800824);
	}

	zyd_write32(sc, ZYD_MAC_SNIFFER, 0x00000000);
	zyd_write32(sc, ZYD_MAC_RXFILTER, 0x00000000);
	zyd_write32(sc, ZYD_MAC_GHTBL, 0x00000000);
	zyd_write32(sc, ZYD_MAC_GHTBH, 0x80000000);
	zyd_write32(sc, ZYD_MAC_MISC, 0x000000a4);
	zyd_write32(sc, ZYD_CR_ADDA_PWR_DWN, 0x0000007f);
	zyd_write32(sc, ZYD_MAC_BCNCFG, 0x00f00401);
	zyd_write32(sc, ZYD_MAC_PHY_DELAY2, 0x00000000);
	zyd_write32(sc, ZYD_MAC_ACK_EXT, 0x00000080);
	zyd_write32(sc, ZYD_CR_ADDA_PWR_DWN, 0x00000000);
	zyd_write32(sc, ZYD_MAC_SIFS_ACK_TIME, 0x00000100);
	zyd_write32(sc, ZYD_MAC_DIFS_EIFS_SIFS, 0x0547c032);
	zyd_write32(sc, ZYD_CR_RX_PE_DELAY, 0x00000070);
	zyd_write32(sc, ZYD_CR_PS_CTRL, 0x10000000);
	zyd_write32(sc, ZYD_MAC_RTSCTSRATE, 0x02030203);
	zyd_write32(sc, ZYD_MAC_RX_THRESHOLD, 0x000c0640);
	zyd_write32(sc, ZYD_MAC_BACKOFF_PROTECT, 0x00000114);

	/* RF chip init */
	zyd_lock_phy(sc);
	error = (*rf->init)(rf);
	zyd_unlock_phy(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "radio initialization failed, error %d\n", error);
		goto fail;
	}

	/* init beacon interval to 100ms */
	if ((error = zyd_set_beacon_interval(sc, 100)) != 0)
		goto fail;

fail:	return error;
}

static int
zyd_read_eeprom(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	uint16_t val;
	int i;

	/* read MAC address */
	(void)zyd_read32(sc, ZYD_EEPROM_MAC_ADDR_P1, &tmp);
	ic->ic_myaddr[0] = tmp & 0xff;
	ic->ic_myaddr[1] = tmp >>  8;
	ic->ic_myaddr[2] = tmp >> 16;
	ic->ic_myaddr[3] = tmp >> 24;
	(void)zyd_read32(sc, ZYD_EEPROM_MAC_ADDR_P2, &tmp);
	ic->ic_myaddr[4] = tmp & 0xff;
	ic->ic_myaddr[5] = tmp >>  8;

	(void)zyd_read32(sc, ZYD_EEPROM_POD, &tmp);
	sc->rf_rev    = tmp & 0x0f;
	sc->fix_cr47  = (tmp >> 8 ) & 0x01;
	sc->fix_cr157 = (tmp >> 13) & 0x01;
	sc->pa_rev    = (tmp >> 16) & 0x0f;

	/* read regulatory domain (currently unused) */
	(void)zyd_read32(sc, ZYD_EEPROM_SUBID, &tmp);
	sc->regdomain = tmp >> 16;
	DPRINTF(("regulatory domain %x\n", sc->regdomain));

	/* read Tx power calibration tables */
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_PWR_CAL + i, &val);
		sc->pwr_cal[i * 2] = val >> 8;
		sc->pwr_cal[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_PWR_INT + i, &val);
		sc->pwr_int[i * 2] = val >> 8;
		sc->pwr_int[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_36M_CAL + i, &val);
		sc->ofdm36_cal[i * 2] = val >> 8;
		sc->ofdm36_cal[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_48M_CAL + i, &val);
		sc->ofdm48_cal[i * 2] = val >> 8;
		sc->ofdm48_cal[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_54M_CAL + i, &val);
		sc->ofdm54_cal[i * 2] = val >> 8;
		sc->ofdm54_cal[i * 2 + 1] = val & 0xff;
	}
	return 0;
}

static int
zyd_set_macaddr(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	(void)zyd_write32(sc, ZYD_MAC_MACADRL, tmp);

	tmp = addr[5] << 8 | addr[4];
	(void)zyd_write32(sc, ZYD_MAC_MACADRH, tmp);

	return 0;
}

static int
zyd_set_bssid(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	(void)zyd_write32(sc, ZYD_MAC_BSSADRL, tmp);

	tmp = addr[5] << 8 | addr[4];
	(void)zyd_write32(sc, ZYD_MAC_BSSADRH, tmp);

	return 0;
}

static int
zyd_switch_radio(struct zyd_softc *sc, int on)
{
	struct zyd_rf *rf = &sc->sc_rf;
	int error;

	zyd_lock_phy(sc);
	error = (*rf->switch_radio)(rf, on);
	zyd_unlock_phy(sc);

	return error;
}

static void
zyd_set_led(struct zyd_softc *sc, int which, int on)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_TX_PE_CONTROL, &tmp);
	tmp &= ~which;
	if (on)
		tmp |= which;
	(void)zyd_write32(sc, ZYD_MAC_TX_PE_CONTROL, tmp);
}

static void
zyd_set_multi(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ifmultiaddr *ifma;
	uint32_t low, high;
	uint8_t v;

	if (!(ifp->if_flags & IFF_UP))
		return;

	low = 0x00000000;
	high = 0x80000000;

	if (ic->ic_opmode == IEEE80211_M_MONITOR ||
	    (ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC))) {
		low = 0xffffffff;
		high = 0xffffffff;
	} else {
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			v = ((uint8_t *)LLADDR((struct sockaddr_dl *)
			    ifma->ifma_addr))[5] >> 2;
			if (v < 32)
				low |= 1 << v;
			else
				high |= 1 << (v - 32);
		}
		IF_ADDR_UNLOCK(ifp);
	}

	/* reprogram multicast global hash table */
	zyd_write32(sc, ZYD_MAC_GHTBL, low);
	zyd_write32(sc, ZYD_MAC_GHTBH, high);
}

static int
zyd_set_rxfilter(struct zyd_softc *sc)
{
	uint32_t rxfilter;

	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_STA:
		rxfilter = ZYD_FILTER_BSS;
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_HOSTAP:
		rxfilter = ZYD_FILTER_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		rxfilter = ZYD_FILTER_MONITOR;
		break;
	default:
		/* should not get there */
		return EINVAL;
	}
	return zyd_write32(sc, ZYD_MAC_RXFILTER, rxfilter);
}

static void
zyd_set_chan(struct zyd_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct zyd_rf *rf = &sc->sc_rf;
	uint32_t tmp;
	u_int chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY) {
		/* XXX should NEVER happen */
		device_printf(sc->sc_dev,
		    "%s: invalid channel %x\n", __func__, chan);
		return;
	}

	zyd_lock_phy(sc);

	(*rf->set_channel)(rf, chan);

	/* update Tx power */
	(void)zyd_write16(sc, ZYD_CR31, sc->pwr_int[chan - 1]);

	if (sc->mac_rev == ZYD_ZD1211B) {
		(void)zyd_write16(sc, ZYD_CR67, sc->ofdm36_cal[chan - 1]);
		(void)zyd_write16(sc, ZYD_CR66, sc->ofdm48_cal[chan - 1]);
		(void)zyd_write16(sc, ZYD_CR65, sc->ofdm54_cal[chan - 1]);

		(void)zyd_write16(sc, ZYD_CR68, sc->pwr_cal[chan - 1]);

		(void)zyd_write16(sc, ZYD_CR69, 0x28);
		(void)zyd_write16(sc, ZYD_CR69, 0x2a);
	}

	if (sc->fix_cr47) {
		/* set CCK baseband gain from EEPROM */
		if (zyd_read32(sc, ZYD_EEPROM_PHY_REG, &tmp) == 0)
			(void)zyd_write16(sc, ZYD_CR47, tmp & 0xff);
	}

	(void)zyd_write32(sc, ZYD_CR_CONFIG_PHILIPS, 0);

	zyd_unlock_phy(sc);

	sc->sc_rxtap.wr_chan_freq = sc->sc_txtap.wt_chan_freq =
	    htole16(c->ic_freq);
	sc->sc_rxtap.wr_chan_flags = sc->sc_txtap.wt_chan_flags =
	    htole16(c->ic_flags);
}

static int
zyd_set_beacon_interval(struct zyd_softc *sc, int bintval)
{
	/* XXX this is probably broken.. */
	(void)zyd_write32(sc, ZYD_CR_ATIM_WND_PERIOD, bintval - 2);
	(void)zyd_write32(sc, ZYD_CR_PRE_TBTT,        bintval - 1);
	(void)zyd_write32(sc, ZYD_CR_BCN_INTERVAL,    bintval);

	return 0;
}

static uint8_t
zyd_plcp_signal(int rate)
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
zyd_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_softc *sc = (struct zyd_softc *)priv;
	struct zyd_cmd *cmd;
	uint32_t datalen;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(
			    sc->zyd_ep[ZYD_ENDPT_IIN]);
		}
		return;
	}

	cmd = (struct zyd_cmd *)sc->ibuf;

	if (le16toh(cmd->code) == ZYD_NOTIF_RETRYSTATUS) {
		struct zyd_notif_retry *retry =
		    (struct zyd_notif_retry *)cmd->data;
		struct ieee80211com *ic = &sc->sc_ic;
		struct ifnet *ifp = sc->sc_ifp;
		struct ieee80211_node *ni;

		DPRINTF(("retry intr: rate=0x%x addr=%s count=%d (0x%x)\n",
		    le16toh(retry->rate), ether_sprintf(retry->macaddr),
		    le16toh(retry->count) & 0xff, le16toh(retry->count)));

		/*
		 * Find the node to which the packet was sent and update its
		 * retry statistics.  In BSS mode, this node is the AP we're
		 * associated to so no lookup is actually needed.
		 */
		if (ic->ic_opmode != IEEE80211_M_STA) {
			ni = ieee80211_find_node(&ic->ic_sta, retry->macaddr);
			if (ni == NULL)
				return;	/* just ignore */
		} else
			ni = ic->ic_bss;

		((struct zyd_node *)ni)->amn.amn_retrycnt++;

		if (le16toh(retry->count) & 0x100)
			ifp->if_oerrors++;	/* too many retries */
	} else if (le16toh(cmd->code) == ZYD_NOTIF_IORD) {
		struct rq *rqp;

		if (le16toh(*(uint16_t *)cmd->data) == ZYD_CR_INTERRUPT)
			return;	/* HMAC interrupt */

		usbd_get_xfer_status(xfer, NULL, NULL, &datalen, NULL);
		datalen -= sizeof(cmd->code);
		datalen -= 2;	/* XXX: padding? */

		STAILQ_FOREACH(rqp, &sc->sc_rqh, rq) {
			int i;

			if (sizeof(struct zyd_pair) * rqp->len != datalen)
				continue;
			for (i = 0; i < rqp->len; i++) {
				if (*(((const uint16_t *)rqp->idata) + i) !=
				    (((struct zyd_pair *)cmd->data) + i)->reg)
					break;
			}
			if (i != rqp->len)
				continue;

			/* copy answer into caller-supplied buffer */
			bcopy(cmd->data, rqp->odata,
			    sizeof(struct zyd_pair) * rqp->len);
			wakeup(rqp->odata);	/* wakeup caller */

			return;
		}
		return;	/* unexpected IORD notification */
	} else {
		device_printf(sc->sc_dev, "unknown notification %x\n",
		    le16toh(cmd->code));
	}
}

static __inline uint8_t
zyd_plcp2ieee(int signal, int isofdm)
{
       if (isofdm) {
               static const uint8_t ofdmrates[16] =
                   { 0, 0, 0, 0, 0, 0, 0, 96, 48, 24, 12, 108, 72, 36, 18 };
               return ofdmrates[signal & 0xf];
       } else {
               static const uint8_t cckrates[16] =
                   { 0, 0, 0, 0, 4, 0, 0, 11, 0, 0, 2, 0, 0, 0, 22, 0 };
               return cckrates[signal & 0xf];
       }
}

static void
zyd_rx_data(struct zyd_softc *sc, const uint8_t *buf, uint16_t len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211_node *ni;
	const struct zyd_plcphdr *plcp;
	const struct zyd_rx_stat *stat;
	struct mbuf *m;
	int rlen;

	if (len < ZYD_MIN_FRAGSZ) {
		DPRINTF(("%s: frame too short (length=%d)\n",
		    device_get_nameunit(sc->sc_dev), len));
		ifp->if_ierrors++;
		return;
	}

	plcp = (const struct zyd_plcphdr *)buf;
	stat = (const struct zyd_rx_stat *)
	    (buf + len - sizeof(struct zyd_rx_stat));

	if (stat->flags & ZYD_RX_ERROR) {
		DPRINTF(("%s: RX status indicated error (%x)\n",
		    device_get_nameunit(sc->sc_dev), stat->flags));
		ifp->if_ierrors++;
		return;
	}

	/* compute actual frame length */
	rlen = len - sizeof(struct zyd_plcphdr) -
	    sizeof(struct zyd_rx_stat) - IEEE80211_CRC_LEN;

	/* allocate a mbuf to store the frame */
	if (rlen > MHLEN)
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	else
		m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		DPRINTF(("%s: could not allocate rx mbuf\n",
		    device_get_nameunit(sc->sc_dev)));
		ifp->if_ierrors++;
		return;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = rlen;
	bcopy((const uint8_t *)(plcp + 1), mtod(m, uint8_t *), rlen);

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct zyd_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		if (stat->flags & (ZYD_RX_BADCRC16 | ZYD_RX_BADCRC32))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
		/* XXX toss, no way to express errors */
		if (stat->flags & ZYD_RX_DECRYPTERR)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
		tap->wr_rate =
		    zyd_plcp2ieee(plcp->signal, stat->flags & ZYD_RX_OFDM);
		tap->wr_antsignal = stat->rssi + -95;
		tap->wr_antnoise = -95;		/* XXX */
		
		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));
	ieee80211_input(ic, m, ni,
	    stat->rssi > 63 ? 127 : 2 * stat->rssi, -95/*XXX*/, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);
}

static void
zyd_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_rx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	const struct zyd_rx_desc *desc;
	int len;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->zyd_ep[ZYD_ENDPT_BIN]);

		goto skip;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len < ZYD_MIN_RXBUFSZ) {
		DPRINTFN(3, ("%s: xfer too short (length=%d)\n",
		    device_get_nameunit(sc->sc_dev), len));
		ifp->if_ierrors++;		/* XXX not really errors */
		goto skip;
	}

	desc = (const struct zyd_rx_desc *)
	    (data->buf + len - sizeof(struct zyd_rx_desc));

	if (UGETW(desc->tag) == ZYD_TAG_MULTIFRAME) {
		const uint8_t *p = data->buf, *end = p + len;
		int i;

		DPRINTFN(3, ("received multi-frame transfer\n"));

		for (i = 0; i < ZYD_MAX_RXFRAMECNT; i++) {
			const uint16_t len16 = UGETW(desc->len[i]);

			if (len16 == 0 || p + len16 > end)
				break;

			zyd_rx_data(sc, p, len16);
			/* next frame is aligned on a 32-bit boundary */
			p += (len16 + 3) & ~3;
		}
	} else {
		DPRINTFN(3, ("received single-frame transfer\n"));

		zyd_rx_data(sc, data->buf, len);
	}

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data, NULL,
	    ZYX_MAX_RXBUFSZ, USBD_NO_COPY | USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, zyd_rxeof);
	(void)usbd_transfer(xfer);
}

static int
zyd_tx_mgt(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = sc->sc_ifp;
	struct zyd_tx_desc *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	int xferlen, totlen, rate;
	uint16_t pktlen;
	usbd_status error;

	data = &sc->tx_data[0];
	desc = (struct zyd_tx_desc *)data->buf;

	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}
	}

	data->ni = ni;
	data->m = m0;

	wh = mtod(m0, struct ieee80211_frame *);

	xferlen = sizeof(struct zyd_tx_desc) + m0->m_pkthdr.len;
	totlen = m0->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* fill Tx descriptor */
	desc->len = htole16(totlen);

	desc->flags = ZYD_TX_FLAG_BACKOFF;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (totlen > ic->ic_rtsthreshold) {
			desc->flags |= ZYD_TX_FLAG_RTS;
		} else if (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				desc->flags |= ZYD_TX_FLAG_CTS_TO_SELF;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				desc->flags |= ZYD_TX_FLAG_RTS;
		}
	} else
		desc->flags |= ZYD_TX_FLAG_MULTICAST;

	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		desc->flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	desc->phy = zyd_plcp_signal(rate);
	if (ZYD_RATE_IS_OFDM(rate)) {
		desc->phy |= ZYD_TX_PHY_OFDM;
		if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
			desc->phy |= ZYD_TX_PHY_5GHZ;
	} else if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		desc->phy |= ZYD_TX_PHY_SHPREAMBLE;

	/* actual transmit length (XXX why +10?) */
	pktlen = sizeof(struct zyd_tx_desc) + 10;
	if (sc->mac_rev == ZYD_ZD1211)
		pktlen += totlen;
	desc->pktlen = htole16(pktlen);

	desc->plcp_length = (16 * totlen + rate - 1) / rate;
	desc->plcp_service = 0;
	if (rate == 22) {
		const int remainder = (16 * totlen) % 22;
		if (remainder != 0 && remainder < 7)
			desc->plcp_service |= ZYD_PLCP_LENGEXT;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct zyd_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len,
	    data->buf + sizeof(struct zyd_tx_desc));

	DPRINTFN(10, ("%s: sending mgt frame len=%zu rate=%u xferlen=%u\n",
	    device_get_nameunit(sc->sc_dev), (size_t)m0->m_pkthdr.len,
		rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);
	error = usbd_transfer(data->xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		ifp->if_oerrors++;
		return EIO;
	}
	sc->tx_queued++;

	return 0;
}

static void
zyd_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_tx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		device_printf(sc->sc_dev, "could not transmit buffer: %s\n",
		    usbd_errstr(status));

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(
			    sc->zyd_ep[ZYD_ENDPT_BOUT]);
		}
		ifp->if_oerrors++;
		return;
	}

	ni = data->ni;
	/* update rate control statistics */
	((struct zyd_node *)ni)->amn.amn_txcnt++;

	/*
	 * Do any tx complete callback.  Note this must
	 * be done before releasing the node reference.
	 */
	m = data->m;
	if (m != NULL && m->m_flags & M_TXCB) {
		ieee80211_process_callback(ni, m, 0);	/* XXX status? */
		m_freem(m);
		data->m = NULL;
	}

	ieee80211_free_node(ni);
	data->ni = NULL;

	sc->tx_queued--;
	ifp->if_opackets++;

	sc->tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	zyd_start(ifp);
}

static int
zyd_tx_data(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = sc->sc_ifp;
	struct zyd_tx_desc *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	int xferlen, totlen, rate;
	uint16_t pktlen;
	usbd_status error;

	wh = mtod(m0, struct ieee80211_frame *);
	data = &sc->tx_data[0];
	desc = (struct zyd_tx_desc *)data->buf;

	desc->flags = ZYD_TX_FLAG_BACKOFF;
	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		rate = ic->ic_mcast_rate;
		desc->flags |= ZYD_TX_FLAG_MULTICAST;
	} else if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE)
		rate = ic->ic_bss->ni_rates.rs_rates[ic->ic_fixed_rate];
	else
		rate = ni->ni_rates.rs_rates[ni->ni_txrate];
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

	data->ni = ni;
	data->m = NULL;

	xferlen = sizeof(struct zyd_tx_desc) + m0->m_pkthdr.len;
	totlen = m0->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* fill Tx descriptor */
	desc->len = htole16(totlen);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (totlen > ic->ic_rtsthreshold) {
			desc->flags |= ZYD_TX_FLAG_RTS;
		} else if (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				desc->flags |= ZYD_TX_FLAG_CTS_TO_SELF;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				desc->flags |= ZYD_TX_FLAG_RTS;
		}
	}

	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		desc->flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	desc->phy = zyd_plcp_signal(rate);
	if (ZYD_RATE_IS_OFDM(rate)) {
		desc->phy |= ZYD_TX_PHY_OFDM;
		if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
			desc->phy |= ZYD_TX_PHY_5GHZ;
	} else if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		desc->phy |= ZYD_TX_PHY_SHPREAMBLE;

	/* actual transmit length (XXX why +10?) */
	pktlen = sizeof(struct zyd_tx_desc) + 10;
	if (sc->mac_rev == ZYD_ZD1211)
		pktlen += totlen;
	desc->pktlen = htole16(pktlen);

	desc->plcp_length = (16 * totlen + rate - 1) / rate;
	desc->plcp_service = 0;
	if (rate == 22) {
		const int remainder = (16 * totlen) % 22;
		if (remainder != 0 && remainder < 7)
			desc->plcp_service |= ZYD_PLCP_LENGEXT;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct zyd_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len,
	    data->buf + sizeof(struct zyd_tx_desc));

	DPRINTFN(10, ("%s: sending data frame len=%zu rate=%u xferlen=%u\n",
	    device_get_nameunit(sc->sc_dev), (size_t)m0->m_pkthdr.len,
		rate, xferlen));

	m_freem(m0);	/* mbuf no longer needed */

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);
	error = usbd_transfer(data->xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		ifp->if_oerrors++;
		return EIO;
	}
	sc->tx_queued++;

	return 0;
}

static void
zyd_start(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
			if (bpf_peers_present(ic->ic_rawbpf))
				bpf_mtap(ic->ic_rawbpf, m0);
			if (zyd_tx_mgt(sc, m0, ni) != 0)
				break;
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_POLL(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			/*
			 * Cancel any background scan.
			 */
			if (ic->ic_flags & IEEE80211_F_SCAN)
				ieee80211_cancel_scan(ic);

			if (m0->m_len < sizeof(struct ether_header) &&
			    !(m0 = m_pullup(m0, sizeof(struct ether_header))))
				continue;

			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m0);
				continue;
			}
			if (bpf_peers_present(ifp->if_bpf))
				bpf_mtap(ifp->if_bpf, m0);
			if ((m0 = ieee80211_encap(ic, m0, ni)) == NULL) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				continue;
			}
			if (bpf_peers_present(ic->ic_rawbpf))
				bpf_mtap(ic->ic_rawbpf, m0);
			if (zyd_tx_data(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->tx_timer = 5;
		ic->ic_lastdata = ticks;
		callout_reset(&sc->sc_watchdog_ch, hz, zyd_watchdog, sc);
	}
}

static void
zyd_watchdog(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	if (sc->tx_timer > 0) {
		if (--sc->tx_timer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			/* zyd_init(ifp); XXX needs a process context ? */
			ifp->if_oerrors++;
			return;
		}
		callout_reset(&sc->sc_watchdog_ch, hz, zyd_watchdog, sc);
	}
}

static int
zyd_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;

	ZYD_LOCK(sc);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->sc_if_flags) &
				    (IFF_ALLMULTI | IFF_PROMISC))
					zyd_set_multi(sc);
			} else
				zyd_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				zyd_stop(sc, 1);
		}
		sc->sc_if_flags = ifp->if_flags;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			zyd_set_multi(sc);
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & IFF_UP) == IFF_UP && 
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) == IFF_DRV_RUNNING)
			zyd_init(sc);
		error = 0;
	}

	ZYD_UNLOCK(sc);

	return error;
}

static void
zyd_init(void *priv)
{
	struct zyd_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	int i, error;

	zyd_stop(sc, 0);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	DPRINTF(("setting MAC address to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = zyd_set_macaddr(sc, ic->ic_myaddr);
	if (error != 0)
		return;

	/* we'll do software WEP decryption for now */
	DPRINTF(("setting encryption type\n"));
	error = zyd_write32(sc, ZYD_MAC_ENCRYPTION_TYPE, ZYD_ENC_SNIFFER);
	if (error != 0)
		return;

	/* promiscuous mode */
	(void)zyd_write32(sc, ZYD_MAC_SNIFFER,
	    (ic->ic_opmode == IEEE80211_M_MONITOR) ? 1 : 0);

	/* multicast setup */
	(void)zyd_set_multi(sc);

	(void)zyd_set_rxfilter(sc);

	/* switch radio transmitter ON */
	(void)zyd_switch_radio(sc, 1);

	/* XXX wrong, can't set here */
	/* set basic rates */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x0003);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x1500);
	else	/* assumes 802.11b/g */
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x000f);

	/* set mandatory rates */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x000f);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x1500);
	else	/* assumes 802.11b/g */
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x150f);

	/* set default BSS channel */
	zyd_set_chan(sc, ic->ic_curchan);

	/* enable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, ZYD_HWINT_MASK);

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	if ((error = zyd_alloc_tx_list(sc)) != 0) {
		device_printf(sc->sc_dev, "could not allocate Tx list\n");
		goto fail;
	}
	if ((error = zyd_alloc_rx_list(sc)) != 0) {
		device_printf(sc->sc_dev, "could not allocate Rx list\n");
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data,
		    NULL, ZYX_MAX_RXBUFSZ, USBD_NO_COPY | USBD_SHORT_XFER_OK,
		    USBD_NO_TIMEOUT, zyd_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != USBD_IN_PROGRESS && error != 0) {
			device_printf(sc->sc_dev,
			    "could not queue Rx transfer\n");
			goto fail;
		}
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	return;

fail:	zyd_stop(sc, 1);
	return;
}

static void
zyd_stop(struct zyd_softc *sc, int disable)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);	/* free all nodes */

	sc->tx_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* switch radio transmitter OFF */
	(void)zyd_switch_radio(sc, 0);

	/* disable Rx */
	(void)zyd_write32(sc, ZYD_MAC_RXFILTER, 0);

	/* disable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, 0);

	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_BIN]);
	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_BOUT]);

	zyd_free_rx_list(sc);
	zyd_free_tx_list(sc);
}

static int
zyd_loadfirmware(struct zyd_softc *sc, u_char *fw, size_t size)
{
	usb_device_request_t req;
	uint16_t addr;
	uint8_t stat;

	DPRINTF(("firmware size=%zu\n", size));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADREQ;
	USETW(req.wIndex, 0);

	addr = ZYD_FIRMWARE_START_ADDR;
	while (size > 0) {
#if 0
		const int mlen = min(size, 4096);
#else
		/*
		 * XXXX: When the transfer size is 4096 bytes, it is not
		 * likely to be able to transfer it.
		 * The cause is port or machine or chip?
		 */
		const int mlen = min(size, 64);
#endif

		DPRINTF(("loading firmware block: len=%d, addr=0x%x\n", mlen,
		    addr));

		USETW(req.wValue, addr);
		USETW(req.wLength, mlen);
		if (usbd_do_request(sc->sc_udev, &req, fw) != 0)
			return EIO;

		addr += mlen / 2;
		fw   += mlen;
		size -= mlen;
	}

	/* check whether the upload succeeded */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADSTS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(stat));
	if (usbd_do_request(sc->sc_udev, &req, &stat) != 0)
		return EIO;

	return (stat & 0x80) ? EIO : 0;
}

static void
zyd_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct zyd_softc *sc = arg;
	struct zyd_node *zn = (struct zyd_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &zn->amn);
}

static void
zyd_amrr_timeout(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	ZYD_LOCK(sc);
	if (ic->ic_opmode == IEEE80211_M_STA)
		zyd_iter_func(sc, ic->ic_bss);
	else
		ieee80211_iterate_nodes(&ic->ic_sta, zyd_iter_func, sc);
	ZYD_UNLOCK(sc);

	callout_reset(&sc->sc_amrr_ch, hz, zyd_amrr_timeout, sc);
}

static void
zyd_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct zyd_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct zyd_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
}

static void
zyd_scan_start(struct ieee80211com *ic)
{
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = ZYD_SCAN_START;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
zyd_scan_end(struct ieee80211com *ic)
{
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = ZYD_SCAN_END;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
zyd_set_channel(struct ieee80211com *ic)
{
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = ZYD_SET_CHANNEL;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
zyd_scantask(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	ZYD_LOCK(sc);

	switch (sc->sc_scan_action) {
	case ZYD_SCAN_START:
                zyd_set_bssid(sc, ifp->if_broadcastaddr);
                break;

        case ZYD_SCAN_END:
                zyd_set_bssid(sc, ic->ic_bss->ni_bssid);
                break;

        case ZYD_SET_CHANNEL:
                mtx_lock(&Giant);
                zyd_set_chan(sc, ic->ic_curchan);
                mtx_unlock(&Giant);
                break;

        default:
                device_printf(sc->sc_dev, "unknown scan action %d\n",
		    sc->sc_scan_action);
                break;
        }

        ZYD_UNLOCK(sc);
}

static device_method_t zyd_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe, zyd_match),
        DEVMETHOD(device_attach, zyd_attach),
        DEVMETHOD(device_detach, zyd_detach),
	
	{ 0, 0 }
};

static driver_t zyd_driver = {
        "zyd",
        zyd_methods,
        sizeof(struct zyd_softc)
};

static devclass_t zyd_devclass;

DRIVER_MODULE(zyd, uhub, zyd_driver, zyd_devclass, usbd_driver_load, 0);
MODULE_DEPEND(rum, wlan, 1, 1, 1);
MODULE_DEPEND(rum, wlan_amrr, 1, 1, 1);
MODULE_DEPEND(rum, usb, 1, 1, 1);
