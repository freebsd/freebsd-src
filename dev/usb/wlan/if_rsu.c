/*	$OpenBSD: if_rsu.c,v 1.17 2013/04/15 09:23:01 mglocker Exp $	*/

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
 * Driver for Realtek RTL8188SU/RTL8191SU/RTL8192SU.
 *
 * TODO:
 *   o h/w crypto
 *   o hostap / ibss / mesh
 *   o sensible RSSI levels
 *   o power-save operation
 */

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/firmware.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
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

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include "usbdevs.h"

#define USB_DEBUG_VAR rsu_debug
#include <dev/usb/usb_debug.h>

#include <dev/usb/wlan/if_rsureg.h>

#ifdef USB_DEBUG
static int rsu_debug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, rsu, CTLFLAG_RW, 0, "USB rsu");
SYSCTL_INT(_hw_usb_rsu, OID_AUTO, debug, CTLFLAG_RWTUN, &rsu_debug, 0,
    "Debug level");
#define	RSU_DPRINTF(_sc, _flg, ...)					\
	do								\
		if (((_flg) == (RSU_DEBUG_ANY)) || (rsu_debug & (_flg))) \
			device_printf((_sc)->sc_dev, __VA_ARGS__);	\
	while (0)
#else
#define	RSU_DPRINTF(_sc, _flg, ...)
#endif

static int rsu_enable_11n = 1;
TUNABLE_INT("hw.usb.rsu.enable_11n", &rsu_enable_11n);

#define	RSU_DEBUG_ANY		0xffffffff
#define	RSU_DEBUG_TX		0x00000001
#define	RSU_DEBUG_RX		0x00000002
#define	RSU_DEBUG_RESET		0x00000004
#define	RSU_DEBUG_CALIB		0x00000008
#define	RSU_DEBUG_STATE		0x00000010
#define	RSU_DEBUG_SCAN		0x00000020
#define	RSU_DEBUG_FWCMD		0x00000040
#define	RSU_DEBUG_TXDONE	0x00000080
#define	RSU_DEBUG_FW		0x00000100
#define	RSU_DEBUG_FWDBG		0x00000200
#define	RSU_DEBUG_AMPDU		0x00000400

static const STRUCT_USB_HOST_ID rsu_devs[] = {
#define	RSU_HT_NOT_SUPPORTED 0
#define	RSU_HT_SUPPORTED 1
#define RSU_DEV_HT(v,p)  { USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, \
				   RSU_HT_SUPPORTED) }
#define RSU_DEV(v,p)     { USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, \
				   RSU_HT_NOT_SUPPORTED) }
	RSU_DEV(ASUS,			RTL8192SU),
	RSU_DEV(AZUREWAVE,		RTL8192SU_4),
	RSU_DEV_HT(ACCTON,		RTL8192SU),
	RSU_DEV_HT(ASUS,		USBN10),
	RSU_DEV_HT(AZUREWAVE,		RTL8192SU_1),
	RSU_DEV_HT(AZUREWAVE,		RTL8192SU_2),
	RSU_DEV_HT(AZUREWAVE,		RTL8192SU_3),
	RSU_DEV_HT(AZUREWAVE,		RTL8192SU_5),
	RSU_DEV_HT(BELKIN,		RTL8192SU_1),
	RSU_DEV_HT(BELKIN,		RTL8192SU_2),
	RSU_DEV_HT(BELKIN,		RTL8192SU_3),
	RSU_DEV_HT(CONCEPTRONIC2,	RTL8192SU_1),
	RSU_DEV_HT(CONCEPTRONIC2,	RTL8192SU_2),
	RSU_DEV_HT(CONCEPTRONIC2,	RTL8192SU_3),
	RSU_DEV_HT(COREGA,		RTL8192SU),
	RSU_DEV_HT(DLINK2,		DWA131A1),
	RSU_DEV_HT(DLINK2,		RTL8192SU_1),
	RSU_DEV_HT(DLINK2,		RTL8192SU_2),
	RSU_DEV_HT(EDIMAX,		RTL8192SU_1),
	RSU_DEV_HT(EDIMAX,		RTL8192SU_2),
	RSU_DEV_HT(EDIMAX,		EW7622UMN),
	RSU_DEV_HT(GUILLEMOT,		HWGUN54),
	RSU_DEV_HT(GUILLEMOT,		HWNUM300),
	RSU_DEV_HT(HAWKING,		RTL8192SU_1),
	RSU_DEV_HT(HAWKING,		RTL8192SU_2),
	RSU_DEV_HT(PLANEX2,		GWUSNANO),
	RSU_DEV_HT(REALTEK,		RTL8171),
	RSU_DEV_HT(REALTEK,		RTL8172),
	RSU_DEV_HT(REALTEK,		RTL8173),
	RSU_DEV_HT(REALTEK,		RTL8174),
	RSU_DEV_HT(REALTEK,		RTL8192SU),
	RSU_DEV_HT(REALTEK,		RTL8712),
	RSU_DEV_HT(REALTEK,		RTL8713),
	RSU_DEV_HT(SENAO,		RTL8192SU_1),
	RSU_DEV_HT(SENAO,		RTL8192SU_2),
	RSU_DEV_HT(SITECOMEU,		WL349V1),
	RSU_DEV_HT(SITECOMEU,		WL353),
	RSU_DEV_HT(SWEEX2,		LW154),
	RSU_DEV_HT(TRENDNET,		TEW646UBH),
#undef RSU_DEV_HT
#undef RSU_DEV
};

static device_probe_t   rsu_match;
static device_attach_t  rsu_attach;
static device_detach_t  rsu_detach;
static usb_callback_t   rsu_bulk_tx_callback_be_bk;
static usb_callback_t   rsu_bulk_tx_callback_vi_vo;
static usb_callback_t   rsu_bulk_tx_callback_h2c;
static usb_callback_t   rsu_bulk_rx_callback;
static usb_error_t	rsu_do_request(struct rsu_softc *,
			    struct usb_device_request *, void *);
static struct ieee80211vap *
		rsu_vap_create(struct ieee80211com *, const char name[],
		    int, enum ieee80211_opmode, int, const uint8_t bssid[],
		    const uint8_t mac[]);
static void	rsu_vap_delete(struct ieee80211vap *);
static void	rsu_scan_start(struct ieee80211com *);
static void	rsu_scan_end(struct ieee80211com *);
static void	rsu_getradiocaps(struct ieee80211com *, int, int *,
		    struct ieee80211_channel[]);
static void	rsu_set_channel(struct ieee80211com *);
static void	rsu_update_mcast(struct ieee80211com *);
static int	rsu_alloc_rx_list(struct rsu_softc *);
static void	rsu_free_rx_list(struct rsu_softc *);
static int	rsu_alloc_tx_list(struct rsu_softc *);
static void	rsu_free_tx_list(struct rsu_softc *);
static void	rsu_free_list(struct rsu_softc *, struct rsu_data [], int);
static struct rsu_data *_rsu_getbuf(struct rsu_softc *);
static struct rsu_data *rsu_getbuf(struct rsu_softc *);
static void	rsu_freebuf(struct rsu_softc *, struct rsu_data *);
static int	rsu_write_region_1(struct rsu_softc *, uint16_t, uint8_t *,
		    int);
static void	rsu_write_1(struct rsu_softc *, uint16_t, uint8_t);
static void	rsu_write_2(struct rsu_softc *, uint16_t, uint16_t);
static void	rsu_write_4(struct rsu_softc *, uint16_t, uint32_t);
static int	rsu_read_region_1(struct rsu_softc *, uint16_t, uint8_t *,
		    int);
static uint8_t	rsu_read_1(struct rsu_softc *, uint16_t);
static uint16_t	rsu_read_2(struct rsu_softc *, uint16_t);
static uint32_t	rsu_read_4(struct rsu_softc *, uint16_t);
static int	rsu_fw_iocmd(struct rsu_softc *, uint32_t);
static uint8_t	rsu_efuse_read_1(struct rsu_softc *, uint16_t);
static int	rsu_read_rom(struct rsu_softc *);
static int	rsu_fw_cmd(struct rsu_softc *, uint8_t, void *, int);
static void	rsu_calib_task(void *, int);
static void	rsu_tx_task(void *, int);
static int	rsu_newstate(struct ieee80211vap *, enum ieee80211_state, int);
#ifdef notyet
static void	rsu_set_key(struct rsu_softc *, const struct ieee80211_key *);
static void	rsu_delete_key(struct rsu_softc *, const struct ieee80211_key *);
#endif
static int	rsu_site_survey(struct rsu_softc *, struct ieee80211vap *);
static int	rsu_join_bss(struct rsu_softc *, struct ieee80211_node *);
static int	rsu_disconnect(struct rsu_softc *);
static int	rsu_hwrssi_to_rssi(struct rsu_softc *, int hw_rssi);
static void	rsu_event_survey(struct rsu_softc *, uint8_t *, int);
static void	rsu_event_join_bss(struct rsu_softc *, uint8_t *, int);
static void	rsu_rx_event(struct rsu_softc *, uint8_t, uint8_t *, int);
static void	rsu_rx_multi_event(struct rsu_softc *, uint8_t *, int);
#if 0
static int8_t	rsu_get_rssi(struct rsu_softc *, int, void *);
#endif
static struct mbuf * rsu_rx_frame(struct rsu_softc *, uint8_t *, int);
static struct mbuf * rsu_rx_multi_frame(struct rsu_softc *, uint8_t *, int);
static struct mbuf *
		rsu_rxeof(struct usb_xfer *, struct rsu_data *);
static void	rsu_txeof(struct usb_xfer *, struct rsu_data *);
static int	rsu_raw_xmit(struct ieee80211_node *, struct mbuf *, 
		    const struct ieee80211_bpf_params *);
static void	rsu_init(struct rsu_softc *);
static int	rsu_tx_start(struct rsu_softc *, struct ieee80211_node *, 
		    struct mbuf *, struct rsu_data *);
static int	rsu_transmit(struct ieee80211com *, struct mbuf *);
static void	rsu_start(struct rsu_softc *);
static void	_rsu_start(struct rsu_softc *);
static void	rsu_parent(struct ieee80211com *);
static void	rsu_stop(struct rsu_softc *);
static void	rsu_ms_delay(struct rsu_softc *, int);

static device_method_t rsu_methods[] = {
	DEVMETHOD(device_probe,		rsu_match),
	DEVMETHOD(device_attach,	rsu_attach),
	DEVMETHOD(device_detach,	rsu_detach),

	DEVMETHOD_END
};

static driver_t rsu_driver = {
	.name = "rsu",
	.methods = rsu_methods,
	.size = sizeof(struct rsu_softc)
};

static devclass_t rsu_devclass;

DRIVER_MODULE(rsu, uhub, rsu_driver, rsu_devclass, NULL, 0);
MODULE_DEPEND(rsu, wlan, 1, 1, 1);
MODULE_DEPEND(rsu, usb, 1, 1, 1);
MODULE_DEPEND(rsu, firmware, 1, 1, 1);
MODULE_VERSION(rsu, 1);
USB_PNP_HOST_INFO(rsu_devs);

static const uint8_t rsu_chan_2ghz[] =
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

static uint8_t rsu_wme_ac_xfer_map[4] = {
	[WME_AC_BE] = RSU_BULK_TX_BE_BK,
	[WME_AC_BK] = RSU_BULK_TX_BE_BK,
	[WME_AC_VI] = RSU_BULK_TX_VI_VO,
	[WME_AC_VO] = RSU_BULK_TX_VI_VO,
};

/* XXX hard-coded */
#define	RSU_H2C_ENDPOINT	3

static const struct usb_config rsu_config[RSU_N_TRANSFER] = {
	[RSU_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = RSU_RXBUFSZ,
		.flags = {
			.pipe_bof = 1,
			.short_xfer_ok = 1
		},
		.callback = rsu_bulk_rx_callback
	},
	[RSU_BULK_TX_BE_BK] = {
		.type = UE_BULK,
		.endpoint = 0x06,
		.direction = UE_DIR_OUT,
		.bufsize = RSU_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.force_short_xfer = 1
		},
		.callback = rsu_bulk_tx_callback_be_bk,
		.timeout = RSU_TX_TIMEOUT
	},
	[RSU_BULK_TX_VI_VO] = {
		.type = UE_BULK,
		.endpoint = 0x04,
		.direction = UE_DIR_OUT,
		.bufsize = RSU_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.force_short_xfer = 1
		},
		.callback = rsu_bulk_tx_callback_vi_vo,
		.timeout = RSU_TX_TIMEOUT
	},
	[RSU_BULK_TX_H2C] = {
		.type = UE_BULK,
		.endpoint = 0x0d,
		.direction = UE_DIR_OUT,
		.bufsize = RSU_TXBUFSZ,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.short_xfer_ok = 1
		},
		.callback = rsu_bulk_tx_callback_h2c,
		.timeout = RSU_TX_TIMEOUT
	},
};

static int
rsu_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST ||
	    uaa->info.bIfaceIndex != 0 ||
	    uaa->info.bConfigIndex != 0)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(rsu_devs, sizeof(rsu_devs), uaa));
}

static int
rsu_send_mgmt(struct ieee80211_node *ni, int type, int arg)
{

	return (ENOTSUP);
}

static void
rsu_update_chw(struct ieee80211com *ic)
{

}

/*
 * notification from net80211 that it'd like to do A-MPDU on the given TID.
 *
 * Note: this actually hangs traffic at the present moment, so don't use it.
 * The firmware debug does indiciate it's sending and establishing a TX AMPDU
 * session, but then no traffic flows.
 */
static int
rsu_ampdu_enable(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
#if 0
	struct rsu_softc *sc = ni->ni_ic->ic_softc;
	struct r92s_add_ba_req req;

	/* Don't enable if it's requested or running */
	if (IEEE80211_AMPDU_REQUESTED(tap))
		return (0);
	if (IEEE80211_AMPDU_RUNNING(tap))
		return (0);

	/* We've decided to send addba; so send it */
	req.tid = htole32(tap->txa_tid);

	/* Attempt net80211 state */
	if (ieee80211_ampdu_tx_request_ext(ni, tap->txa_tid) != 1)
		return (0);

	/* Send the firmware command */
	RSU_DPRINTF(sc, RSU_DEBUG_AMPDU, "%s: establishing AMPDU TX for TID %d\n",
	    __func__,
	    tap->txa_tid);

	RSU_LOCK(sc);
	if (rsu_fw_cmd(sc, R92S_CMD_ADDBA_REQ, &req, sizeof(req)) != 1) {
		RSU_UNLOCK(sc);
		/* Mark failure */
		(void) ieee80211_ampdu_tx_request_active_ext(ni, tap->txa_tid, 0);
		return (0);
	}
	RSU_UNLOCK(sc);

	/* Mark success; we don't get any further notifications */
	(void) ieee80211_ampdu_tx_request_active_ext(ni, tap->txa_tid, 1);
#endif
	/* Return 0, we're driving this ourselves */
	return (0);
}

static int
rsu_wme_update(struct ieee80211com *ic)
{

	/* Firmware handles this; not our problem */
	return (0);
}

static int
rsu_attach(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct rsu_softc *sc = device_get_softc(self);
	struct ieee80211com *ic = &sc->sc_ic;
	int error;
	uint8_t iface_index;
	struct usb_interface *iface;
	const char *rft;

	device_set_usb_desc(self);
	sc->sc_udev = uaa->device;
	sc->sc_dev = self;
	if (rsu_enable_11n)
		sc->sc_ht = !! (USB_GET_DRIVER_INFO(uaa) & RSU_HT_SUPPORTED);

	/* Get number of endpoints */
	iface = usbd_get_iface(sc->sc_udev, 0);
	sc->sc_nendpoints = iface->idesc->bNumEndpoints;

	/* Endpoints are hard-coded for now, so enforce 4-endpoint only */
	if (sc->sc_nendpoints != 4) {
		device_printf(sc->sc_dev,
		    "the driver currently only supports 4-endpoint devices\n");
		return (ENXIO);
	}

	mtx_init(&sc->sc_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
	    MTX_DEF);
	TIMEOUT_TASK_INIT(taskqueue_thread, &sc->calib_task, 0, 
	    rsu_calib_task, sc);
	TASK_INIT(&sc->tx_task, 0, rsu_tx_task, sc);
	mbufq_init(&sc->sc_snd, ifqmaxlen);

	/* Allocate Tx/Rx buffers. */
	error = rsu_alloc_rx_list(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Rx buffers\n");
		goto fail_usb;
	}

	error = rsu_alloc_tx_list(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Tx buffers\n");
		rsu_free_rx_list(sc);
		goto fail_usb;
	}

	iface_index = 0;
	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    rsu_config, RSU_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(sc->sc_dev,
		    "could not allocate USB transfers, err=%s\n", 
		    usbd_errstr(error));
		goto fail_usb;
	}
	RSU_LOCK(sc);
	/* Read chip revision. */
	sc->cut = MS(rsu_read_4(sc, R92S_PMC_FSM), R92S_PMC_FSM_CUT);
	if (sc->cut != 3)
		sc->cut = (sc->cut >> 1) + 1;
	error = rsu_read_rom(sc);
	RSU_UNLOCK(sc);
	if (error != 0) {
		device_printf(self, "could not read ROM\n");
		goto fail_rom;
	}

	/* Figure out TX/RX streams */
	switch (sc->rom[84]) {
	case 0x0:
		sc->sc_rftype = RTL8712_RFCONFIG_1T1R;
		sc->sc_nrxstream = 1;
		sc->sc_ntxstream = 1;
		rft = "1T1R";
		break;
	case 0x1:
		sc->sc_rftype = RTL8712_RFCONFIG_1T2R;
		sc->sc_nrxstream = 2;
		sc->sc_ntxstream = 1;
		rft = "1T2R";
		break;
	case 0x2:
		sc->sc_rftype = RTL8712_RFCONFIG_2T2R;
		sc->sc_nrxstream = 2;
		sc->sc_ntxstream = 2;
		rft = "2T2R";
		break;
	default:
		device_printf(sc->sc_dev,
		    "%s: unknown board type (rfconfig=0x%02x)\n",
		    __func__,
		    sc->rom[84]);
		goto fail_rom;
	}

	IEEE80211_ADDR_COPY(ic->ic_macaddr, &sc->rom[0x12]);
	device_printf(self, "MAC/BB RTL8712 cut %d %s\n", sc->cut, rft);

	ic->ic_softc = sc;
	ic->ic_name = device_get_nameunit(self);
	ic->ic_phytype = IEEE80211_T_OFDM;	/* Not only, but not used. */
	ic->ic_opmode = IEEE80211_M_STA;	/* Default to BSS mode. */

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_STA |		/* station mode */
#if 0
	    IEEE80211_C_BGSCAN |	/* Background scan. */
#endif
	    IEEE80211_C_SHPREAMBLE |	/* Short preamble supported. */
	    IEEE80211_C_WME |		/* WME/QoS */
	    IEEE80211_C_SHSLOT |	/* Short slot time supported. */
	    IEEE80211_C_WPA;		/* WPA/RSN. */

	/* Check if HT support is present. */
	if (sc->sc_ht) {
		device_printf(sc->sc_dev, "%s: enabling 11n\n", __func__);

		/* Enable basic HT */
		ic->ic_htcaps = IEEE80211_HTC_HT |
#if 0
		    IEEE80211_HTC_AMPDU |
#endif
		    IEEE80211_HTC_AMSDU |
		    IEEE80211_HTCAP_MAXAMSDU_3839 |
		    IEEE80211_HTCAP_SMPS_OFF;
		ic->ic_htcaps |= IEEE80211_HTCAP_CHWIDTH40;

		/* set number of spatial streams */
		ic->ic_txstream = sc->sc_ntxstream;
		ic->ic_rxstream = sc->sc_nrxstream;
	}

	rsu_getradiocaps(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	ieee80211_ifattach(ic);
	ic->ic_raw_xmit = rsu_raw_xmit;
	ic->ic_scan_start = rsu_scan_start;
	ic->ic_scan_end = rsu_scan_end;
	ic->ic_getradiocaps = rsu_getradiocaps;
	ic->ic_set_channel = rsu_set_channel;
	ic->ic_vap_create = rsu_vap_create;
	ic->ic_vap_delete = rsu_vap_delete;
	ic->ic_update_mcast = rsu_update_mcast;
	ic->ic_parent = rsu_parent;
	ic->ic_transmit = rsu_transmit;
	ic->ic_send_mgmt = rsu_send_mgmt;
	ic->ic_update_chw = rsu_update_chw;
	ic->ic_ampdu_enable = rsu_ampdu_enable;
	ic->ic_wme.wme_update = rsu_wme_update;

	ieee80211_radiotap_attach(ic, &sc->sc_txtap.wt_ihdr,
	    sizeof(sc->sc_txtap), RSU_TX_RADIOTAP_PRESENT, 
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
	    RSU_RX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

fail_rom:
	usbd_transfer_unsetup(sc->sc_xfer, RSU_N_TRANSFER);
fail_usb:
	mtx_destroy(&sc->sc_mtx);
	return (ENXIO);
}

static int
rsu_detach(device_t self)
{
	struct rsu_softc *sc = device_get_softc(self);
	struct ieee80211com *ic = &sc->sc_ic;

	RSU_LOCK(sc);
	rsu_stop(sc);
	RSU_UNLOCK(sc);

	usbd_transfer_unsetup(sc->sc_xfer, RSU_N_TRANSFER);

	/*
	 * Free buffers /before/ we detach from net80211, else node
	 * references to destroyed vaps will lead to a panic.
	 */
	/* Free Tx/Rx buffers. */
	RSU_LOCK(sc);
	rsu_free_tx_list(sc);
	rsu_free_rx_list(sc);
	RSU_UNLOCK(sc);

	/* Frames are freed; detach from net80211 */
	ieee80211_ifdetach(ic);

	taskqueue_drain_timeout(taskqueue_thread, &sc->calib_task);
	taskqueue_drain(taskqueue_thread, &sc->tx_task);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static usb_error_t
rsu_do_request(struct rsu_softc *sc, struct usb_device_request *req,
    void *data)
{
	usb_error_t err;
	int ntries = 10;
	
	RSU_ASSERT_LOCKED(sc);

	while (ntries--) {
		err = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx,
		    req, data, 0, NULL, 250 /* ms */);
		if (err == 0 || err == USB_ERR_NOT_CONFIGURED)
			break;
		DPRINTFN(1, "Control request failed, %s (retrying)\n",
		    usbd_errstr(err));
		rsu_ms_delay(sc, 10);
        }

        return (err);
}

static struct ieee80211vap *
rsu_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct rsu_vap *uvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))         /* only one at a time */
		return (NULL);

	uvp =  malloc(sizeof(struct rsu_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &uvp->vap;

	if (ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags, bssid) != 0) {
		/* out of memory */
		free(uvp, M_80211_VAP);
		return (NULL);
	}

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = rsu_newstate;

	/* Limits from the r92su driver */
	vap->iv_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_16;
	vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_32K;

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);
	ic->ic_opmode = opmode;

	return (vap);
}

static void
rsu_vap_delete(struct ieee80211vap *vap)
{
	struct rsu_vap *uvp = RSU_VAP(vap);

	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

static void
rsu_scan_start(struct ieee80211com *ic)
{
	struct rsu_softc *sc = ic->ic_softc;
	int error;

	/* Scanning is done by the firmware. */
	RSU_LOCK(sc);
	/* XXX TODO: force awake if in in network-sleep? */
	error = rsu_site_survey(sc, TAILQ_FIRST(&ic->ic_vaps));
	RSU_UNLOCK(sc);
	if (error != 0)
		device_printf(sc->sc_dev,
		    "could not send site survey command\n");
}

static void
rsu_scan_end(struct ieee80211com *ic)
{
	/* Nothing to do here. */
}

static void
rsu_getradiocaps(struct ieee80211com *ic,
    int maxchans, int *nchans, struct ieee80211_channel chans[])
{
	struct rsu_softc *sc = ic->ic_softc;
	uint8_t bands[IEEE80211_MODE_BYTES];

	/* Set supported .11b and .11g rates. */
	memset(bands, 0, sizeof(bands));
	setbit(bands, IEEE80211_MODE_11B);
	setbit(bands, IEEE80211_MODE_11G);
	if (sc->sc_ht)
		setbit(bands, IEEE80211_MODE_11NG);
	ieee80211_add_channel_list_2ghz(chans, maxchans, nchans,
	    rsu_chan_2ghz, nitems(rsu_chan_2ghz), bands, 0);
}

static void
rsu_set_channel(struct ieee80211com *ic __unused)
{
	/* We are unable to switch channels, yet. */
}

static void
rsu_update_mcast(struct ieee80211com *ic)
{
        /* XXX do nothing?  */
}

static int
rsu_alloc_list(struct rsu_softc *sc, struct rsu_data data[],
    int ndata, int maxsz)
{
	int i, error;

	for (i = 0; i < ndata; i++) {
		struct rsu_data *dp = &data[i];
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
	rsu_free_list(sc, data, ndata);
	return (error);
}

static int
rsu_alloc_rx_list(struct rsu_softc *sc)
{
        int error, i;

	error = rsu_alloc_list(sc, sc->sc_rx, RSU_RX_LIST_COUNT,
	    RSU_RXBUFSZ);
	if (error != 0)
		return (error);

	STAILQ_INIT(&sc->sc_rx_active);
	STAILQ_INIT(&sc->sc_rx_inactive);

	for (i = 0; i < RSU_RX_LIST_COUNT; i++)
		STAILQ_INSERT_HEAD(&sc->sc_rx_inactive, &sc->sc_rx[i], next);

	return (0);
}

static int
rsu_alloc_tx_list(struct rsu_softc *sc)
{
	int error, i;

	error = rsu_alloc_list(sc, sc->sc_tx, RSU_TX_LIST_COUNT,
	    RSU_TXBUFSZ);
	if (error != 0)
		return (error);

	STAILQ_INIT(&sc->sc_tx_inactive);

	for (i = 0; i != RSU_N_TRANSFER; i++) {
		STAILQ_INIT(&sc->sc_tx_active[i]);
		STAILQ_INIT(&sc->sc_tx_pending[i]);
	}

	for (i = 0; i < RSU_TX_LIST_COUNT; i++) {
		STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, &sc->sc_tx[i], next);
	}

	return (0);
}

static void
rsu_free_tx_list(struct rsu_softc *sc)
{
	int i;

	/* prevent further allocations from TX list(s) */
	STAILQ_INIT(&sc->sc_tx_inactive);

	for (i = 0; i != RSU_N_TRANSFER; i++) {
		STAILQ_INIT(&sc->sc_tx_active[i]);
		STAILQ_INIT(&sc->sc_tx_pending[i]);
	}

	rsu_free_list(sc, sc->sc_tx, RSU_TX_LIST_COUNT);
}

static void
rsu_free_rx_list(struct rsu_softc *sc)
{
	/* prevent further allocations from RX list(s) */
	STAILQ_INIT(&sc->sc_rx_inactive);
	STAILQ_INIT(&sc->sc_rx_active);

	rsu_free_list(sc, sc->sc_rx, RSU_RX_LIST_COUNT);
}

static void
rsu_free_list(struct rsu_softc *sc, struct rsu_data data[], int ndata)
{
	int i;

	for (i = 0; i < ndata; i++) {
		struct rsu_data *dp = &data[i];

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

static struct rsu_data *
_rsu_getbuf(struct rsu_softc *sc)
{
	struct rsu_data *bf;

	bf = STAILQ_FIRST(&sc->sc_tx_inactive);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&sc->sc_tx_inactive, next);
	else
		bf = NULL;
	return (bf);
}

static struct rsu_data *
rsu_getbuf(struct rsu_softc *sc)
{
	struct rsu_data *bf;

	RSU_ASSERT_LOCKED(sc);

	bf = _rsu_getbuf(sc);
	if (bf == NULL) {
		RSU_DPRINTF(sc, RSU_DEBUG_TX, "%s: no buffers\n", __func__);
	}
	return (bf);
}

static void
rsu_freebuf(struct rsu_softc *sc, struct rsu_data *bf)
{

	RSU_ASSERT_LOCKED(sc);
	STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, bf, next);
}

static int
rsu_write_region_1(struct rsu_softc *sc, uint16_t addr, uint8_t *buf,
    int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = R92S_REQ_REGS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	return (rsu_do_request(sc, &req, buf));
}

static void
rsu_write_1(struct rsu_softc *sc, uint16_t addr, uint8_t val)
{
	rsu_write_region_1(sc, addr, &val, 1);
}

static void
rsu_write_2(struct rsu_softc *sc, uint16_t addr, uint16_t val)
{
	val = htole16(val);
	rsu_write_region_1(sc, addr, (uint8_t *)&val, 2);
}

static void
rsu_write_4(struct rsu_softc *sc, uint16_t addr, uint32_t val)
{
	val = htole32(val);
	rsu_write_region_1(sc, addr, (uint8_t *)&val, 4);
}

static int
rsu_read_region_1(struct rsu_softc *sc, uint16_t addr, uint8_t *buf,
    int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = R92S_REQ_REGS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	return (rsu_do_request(sc, &req, buf));
}

static uint8_t
rsu_read_1(struct rsu_softc *sc, uint16_t addr)
{
	uint8_t val;

	if (rsu_read_region_1(sc, addr, &val, 1) != 0)
		return (0xff);
	return (val);
}

static uint16_t
rsu_read_2(struct rsu_softc *sc, uint16_t addr)
{
	uint16_t val;

	if (rsu_read_region_1(sc, addr, (uint8_t *)&val, 2) != 0)
		return (0xffff);
	return (le16toh(val));
}

static uint32_t
rsu_read_4(struct rsu_softc *sc, uint16_t addr)
{
	uint32_t val;

	if (rsu_read_region_1(sc, addr, (uint8_t *)&val, 4) != 0)
		return (0xffffffff);
	return (le32toh(val));
}

static int
rsu_fw_iocmd(struct rsu_softc *sc, uint32_t iocmd)
{
	int ntries;

	rsu_write_4(sc, R92S_IOCMD_CTRL, iocmd);
	rsu_ms_delay(sc, 1);
	for (ntries = 0; ntries < 50; ntries++) {
		if (rsu_read_4(sc, R92S_IOCMD_CTRL) == 0)
			return (0);
		rsu_ms_delay(sc, 1);
	}
	return (ETIMEDOUT);
}

static uint8_t
rsu_efuse_read_1(struct rsu_softc *sc, uint16_t addr)
{
	uint32_t reg;
	int ntries;

	reg = rsu_read_4(sc, R92S_EFUSE_CTRL);
	reg = RW(reg, R92S_EFUSE_CTRL_ADDR, addr);
	reg &= ~R92S_EFUSE_CTRL_VALID;
	rsu_write_4(sc, R92S_EFUSE_CTRL, reg);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rsu_read_4(sc, R92S_EFUSE_CTRL);
		if (reg & R92S_EFUSE_CTRL_VALID)
			return (MS(reg, R92S_EFUSE_CTRL_DATA));
		rsu_ms_delay(sc, 1);
	}
	device_printf(sc->sc_dev,
	    "could not read efuse byte at address 0x%x\n", addr);
	return (0xff);
}

static int
rsu_read_rom(struct rsu_softc *sc)
{
	uint8_t *rom = sc->rom;
	uint16_t addr = 0;
	uint32_t reg;
	uint8_t off, msk;
	int i;

	/* Make sure that ROM type is eFuse and that autoload succeeded. */
	reg = rsu_read_1(sc, R92S_EE_9346CR);
	if ((reg & (R92S_9356SEL | R92S_EEPROM_EN)) != R92S_EEPROM_EN)
		return (EIO);

	/* Turn on 2.5V to prevent eFuse leakage. */
	reg = rsu_read_1(sc, R92S_EFUSE_TEST + 3);
	rsu_write_1(sc, R92S_EFUSE_TEST + 3, reg | 0x80);
	rsu_ms_delay(sc, 1);
	rsu_write_1(sc, R92S_EFUSE_TEST + 3, reg & ~0x80);

	/* Read full ROM image. */
	memset(&sc->rom, 0xff, sizeof(sc->rom));
	while (addr < 512) {
		reg = rsu_efuse_read_1(sc, addr);
		if (reg == 0xff)
			break;
		addr++;
		off = reg >> 4;
		msk = reg & 0xf;
		for (i = 0; i < 4; i++) {
			if (msk & (1 << i))
				continue;
			rom[off * 8 + i * 2 + 0] =
			    rsu_efuse_read_1(sc, addr);
			addr++;
			rom[off * 8 + i * 2 + 1] =
			    rsu_efuse_read_1(sc, addr);
			addr++;
		}
	}
#ifdef USB_DEBUG
	if (rsu_debug >= 5) {
		/* Dump ROM content. */
		printf("\n");
		for (i = 0; i < sizeof(sc->rom); i++)
			printf("%02x:", rom[i]);
		printf("\n");
	}
#endif
	return (0);
}

static int
rsu_fw_cmd(struct rsu_softc *sc, uint8_t code, void *buf, int len)
{
	const uint8_t which = RSU_H2C_ENDPOINT;
	struct rsu_data *data;
	struct r92s_tx_desc *txd;
	struct r92s_fw_cmd_hdr *cmd;
	int cmdsz;
	int xferlen;

	RSU_ASSERT_LOCKED(sc);

	data = rsu_getbuf(sc);
	if (data == NULL)
		return (ENOMEM);

	/* Blank the entire payload, just to be safe */
	memset(data->buf, '\0', RSU_TXBUFSZ);

	/* Round-up command length to a multiple of 8 bytes. */
	/* XXX TODO: is this required? */
	cmdsz = (len + 7) & ~7;

	xferlen = sizeof(*txd) + sizeof(*cmd) + cmdsz;
	KASSERT(xferlen <= RSU_TXBUFSZ, ("%s: invalid length", __func__));
	memset(data->buf, 0, xferlen);

	/* Setup Tx descriptor. */
	txd = (struct r92s_tx_desc *)data->buf;
	txd->txdw0 = htole32(
	    SM(R92S_TXDW0_OFFSET, sizeof(*txd)) |
	    SM(R92S_TXDW0_PKTLEN, sizeof(*cmd) + cmdsz) |
	    R92S_TXDW0_OWN | R92S_TXDW0_FSG | R92S_TXDW0_LSG);
	txd->txdw1 = htole32(SM(R92S_TXDW1_QSEL, R92S_TXDW1_QSEL_H2C));

	/* Setup command header. */
	cmd = (struct r92s_fw_cmd_hdr *)&txd[1];
	cmd->len = htole16(cmdsz);
	cmd->code = code;
	cmd->seq = sc->cmd_seq;
	sc->cmd_seq = (sc->cmd_seq + 1) & 0x7f;

	/* Copy command payload. */
	memcpy(&cmd[1], buf, len);

	RSU_DPRINTF(sc, RSU_DEBUG_TX | RSU_DEBUG_FWCMD,
	    "%s: Tx cmd code=0x%x len=0x%x\n",
	    __func__, code, cmdsz);
	data->buflen = xferlen;
	STAILQ_INSERT_TAIL(&sc->sc_tx_pending[which], data, next);
	usbd_transfer_start(sc->sc_xfer[which]);

	return (0);
}

/* ARGSUSED */
static void
rsu_calib_task(void *arg, int pending __unused)
{
	struct rsu_softc *sc = arg;
#ifdef notyet
	uint32_t reg;
#endif

	RSU_DPRINTF(sc, RSU_DEBUG_CALIB, "%s: running calibration task\n",
	    __func__);

	RSU_LOCK(sc);
#ifdef notyet
	/* Read WPS PBC status. */
	rsu_write_1(sc, R92S_MAC_PINMUX_CTRL,
	    R92S_GPIOMUX_EN | SM(R92S_GPIOSEL_GPIO, R92S_GPIOSEL_GPIO_JTAG));
	rsu_write_1(sc, R92S_GPIO_IO_SEL,
	    rsu_read_1(sc, R92S_GPIO_IO_SEL) & ~R92S_GPIO_WPS);
	reg = rsu_read_1(sc, R92S_GPIO_CTRL);
	if (reg != 0xff && (reg & R92S_GPIO_WPS))
		DPRINTF(("WPS PBC is pushed\n"));
#endif
	/* Read current signal level. */
	if (rsu_fw_iocmd(sc, 0xf4000001) == 0) {
		sc->sc_currssi = rsu_read_4(sc, R92S_IOCMD_DATA);
		RSU_DPRINTF(sc, RSU_DEBUG_CALIB, "%s: RSSI=%d (%d)\n",
		    __func__, sc->sc_currssi,
		    rsu_hwrssi_to_rssi(sc, sc->sc_currssi));
	}
	if (sc->sc_calibrating)
		taskqueue_enqueue_timeout(taskqueue_thread, &sc->calib_task, hz);
	RSU_UNLOCK(sc);
}

static void
rsu_tx_task(void *arg, int pending __unused)
{
	struct rsu_softc *sc = arg;

	RSU_LOCK(sc);
	_rsu_start(sc);
	RSU_UNLOCK(sc);
}

#define	RSU_PWR_UNKNOWN		0x0
#define	RSU_PWR_ACTIVE		0x1
#define	RSU_PWR_OFF		0x2
#define	RSU_PWR_SLEEP		0x3

/*
 * Set the current power state.
 *
 * The rtlwifi code doesn't do this so aggressively; it
 * waits for an idle period after association with
 * no traffic before doing this.
 *
 * For now - it's on in all states except RUN, and
 * in RUN it'll transition to allow sleep.
 */

struct r92s_pwr_cmd {
	uint8_t mode;
	uint8_t smart_ps;
	uint8_t bcn_pass_time;
};

static int
rsu_set_fw_power_state(struct rsu_softc *sc, int state)
{
	struct r92s_set_pwr_mode cmd;
	//struct r92s_pwr_cmd cmd;
	int error;

	RSU_ASSERT_LOCKED(sc);

	/* only change state if required */
	if (sc->sc_curpwrstate == state)
		return (0);

	memset(&cmd, 0, sizeof(cmd));

	switch (state) {
	case RSU_PWR_ACTIVE:
		/* Force the hardware awake */
		rsu_write_1(sc, R92S_USB_HRPWM,
		    R92S_USB_HRPWM_PS_ST_ACTIVE | R92S_USB_HRPWM_PS_ALL_ON);
		cmd.mode = R92S_PS_MODE_ACTIVE;
		break;
	case RSU_PWR_SLEEP:
		cmd.mode = R92S_PS_MODE_DTIM;	/* XXX configurable? */
		cmd.smart_ps = 1; /* XXX 2 if doing p2p */
		cmd.bcn_pass_time = 5; /* in 100mS usb.c, linux/rtlwifi */
		break;
	case RSU_PWR_OFF:
		cmd.mode = R92S_PS_MODE_RADIOOFF;
		break;
	default:
		device_printf(sc->sc_dev, "%s: unknown ps mode (%d)\n",
		    __func__,
		    state);
		return (ENXIO);
	}

	RSU_DPRINTF(sc, RSU_DEBUG_RESET,
	    "%s: setting ps mode to %d (mode %d)\n",
	    __func__, state, cmd.mode);
	error = rsu_fw_cmd(sc, R92S_CMD_SET_PWR_MODE, &cmd, sizeof(cmd));
	if (error == 0)
		sc->sc_curpwrstate = state;

	return (error);
}

static int
rsu_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct rsu_vap *uvp = RSU_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct rsu_softc *sc = ic->ic_softc;
	struct ieee80211_node *ni;
	struct ieee80211_rateset *rs;
	enum ieee80211_state ostate;
	int error, startcal = 0;

	ostate = vap->iv_state;
	RSU_DPRINTF(sc, RSU_DEBUG_STATE, "%s: %s -> %s\n",
	    __func__,
	    ieee80211_state_name[ostate],
	    ieee80211_state_name[nstate]);

	IEEE80211_UNLOCK(ic);
	if (ostate == IEEE80211_S_RUN) {
		RSU_LOCK(sc);
		/* Stop calibration. */
		sc->sc_calibrating = 0;
		RSU_UNLOCK(sc);
		taskqueue_drain_timeout(taskqueue_thread, &sc->calib_task);
		taskqueue_drain(taskqueue_thread, &sc->tx_task);
		/* Disassociate from our current BSS. */
		RSU_LOCK(sc);
		rsu_disconnect(sc);
	} else
		RSU_LOCK(sc);
	switch (nstate) {
	case IEEE80211_S_INIT:
		(void) rsu_set_fw_power_state(sc, RSU_PWR_ACTIVE);
		break;
	case IEEE80211_S_AUTH:
		ni = ieee80211_ref_node(vap->iv_bss);
		(void) rsu_set_fw_power_state(sc, RSU_PWR_ACTIVE);
		error = rsu_join_bss(sc, ni);
		ieee80211_free_node(ni);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not send join command\n");
		}
		break;
	case IEEE80211_S_RUN:
		ni = ieee80211_ref_node(vap->iv_bss);
		rs = &ni->ni_rates;
		/* Indicate highest supported rate. */
		ni->ni_txrate = rs->rs_rates[rs->rs_nrates - 1];
		(void) rsu_set_fw_power_state(sc, RSU_PWR_SLEEP);
		ieee80211_free_node(ni);
		startcal = 1;
		break;
	default:
		break;
	}
	sc->sc_calibrating = 1;
	/* Start periodic calibration. */
	taskqueue_enqueue_timeout(taskqueue_thread, &sc->calib_task, hz);
	RSU_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return (uvp->newstate(vap, nstate, arg));
}

#ifdef notyet
static void
rsu_set_key(struct rsu_softc *sc, const struct ieee80211_key *k)
{
	struct r92s_fw_cmd_set_key key;

	memset(&key, 0, sizeof(key));
	/* Map net80211 cipher to HW crypto algorithm. */
	switch (k->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_WEP:
		if (k->wk_keylen < 8)
			key.algo = R92S_KEY_ALGO_WEP40;
		else
			key.algo = R92S_KEY_ALGO_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		key.algo = R92S_KEY_ALGO_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		key.algo = R92S_KEY_ALGO_AES;
		break;
	default:
		return;
	}
	key.id = k->wk_keyix;
	key.grpkey = (k->wk_flags & IEEE80211_KEY_GROUP) != 0;
	memcpy(key.key, k->wk_key, MIN(k->wk_keylen, sizeof(key.key)));
	(void)rsu_fw_cmd(sc, R92S_CMD_SET_KEY, &key, sizeof(key));
}

static void
rsu_delete_key(struct rsu_softc *sc, const struct ieee80211_key *k)
{
	struct r92s_fw_cmd_set_key key;

	memset(&key, 0, sizeof(key));
	key.id = k->wk_keyix;
	(void)rsu_fw_cmd(sc, R92S_CMD_SET_KEY, &key, sizeof(key));
}
#endif

static int
rsu_site_survey(struct rsu_softc *sc, struct ieee80211vap *vap)
{
	struct r92s_fw_cmd_sitesurvey cmd;
	struct ieee80211com *ic = &sc->sc_ic;
	int r;

	RSU_ASSERT_LOCKED(sc);

	memset(&cmd, 0, sizeof(cmd));
	if ((ic->ic_flags & IEEE80211_F_ASCAN) || sc->sc_scan_pass == 1)
		cmd.active = htole32(1);
	cmd.limit = htole32(48);
	if (sc->sc_scan_pass == 1 && vap->iv_des_nssid > 0) {
		/* Do a directed scan for second pass. */
		cmd.ssidlen = htole32(vap->iv_des_ssid[0].len);
		memcpy(cmd.ssid, vap->iv_des_ssid[0].ssid,
		    vap->iv_des_ssid[0].len);

	}
	DPRINTF("sending site survey command, pass=%d\n", sc->sc_scan_pass);
	r = rsu_fw_cmd(sc, R92S_CMD_SITE_SURVEY, &cmd, sizeof(cmd));
	if (r == 0) {
		sc->sc_scanning = 1;
	}
	return (r);
}

static int
rsu_join_bss(struct rsu_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ndis_wlan_bssid_ex *bss;
	struct ndis_802_11_fixed_ies *fixed;
	struct r92s_fw_cmd_auth auth;
	uint8_t buf[sizeof(*bss) + 128] __aligned(4);
	uint8_t *frm;
	uint8_t opmode;
	int error;
	int cnt;
	char *msg = "rsujoin";

	RSU_ASSERT_LOCKED(sc);

	/*
	 * Until net80211 scanning doesn't automatically finish
	 * before we tell it to, let's just wait until any pending
	 * scan is done.
	 *
	 * XXX TODO: yes, this releases and re-acquires the lock.
	 * We should re-verify the state whenever we re-attempt this!
	 */
	cnt = 0;
	while (sc->sc_scanning && cnt < 10) {
		device_printf(sc->sc_dev,
		    "%s: still scanning! (attempt %d)\n",
		    __func__, cnt);
		msleep(msg, &sc->sc_mtx, 0, msg, hz / 2);
		cnt++;
	}

	/* Let the FW decide the opmode based on the capinfo field. */
	opmode = NDIS802_11AUTOUNKNOWN;
	RSU_DPRINTF(sc, RSU_DEBUG_RESET,
	    "%s: setting operating mode to %d\n",
	    __func__, opmode);
	error = rsu_fw_cmd(sc, R92S_CMD_SET_OPMODE, &opmode, sizeof(opmode));
	if (error != 0)
		return (error);

	memset(&auth, 0, sizeof(auth));
	if (vap->iv_flags & IEEE80211_F_WPA) {
		auth.mode = R92S_AUTHMODE_WPA;
		auth.dot1x = (ni->ni_authmode == IEEE80211_AUTH_8021X);
	} else
		auth.mode = R92S_AUTHMODE_OPEN;
	RSU_DPRINTF(sc, RSU_DEBUG_RESET,
	    "%s: setting auth mode to %d\n",
	    __func__, auth.mode);
	error = rsu_fw_cmd(sc, R92S_CMD_SET_AUTH, &auth, sizeof(auth));
	if (error != 0)
		return (error);

	memset(buf, 0, sizeof(buf));
	bss = (struct ndis_wlan_bssid_ex *)buf;
	IEEE80211_ADDR_COPY(bss->macaddr, ni->ni_bssid);
	bss->ssid.ssidlen = htole32(ni->ni_esslen);
	memcpy(bss->ssid.ssid, ni->ni_essid, ni->ni_esslen);
	if (vap->iv_flags & (IEEE80211_F_PRIVACY | IEEE80211_F_WPA))
		bss->privacy = htole32(1);
	bss->rssi = htole32(ni->ni_avgrssi);
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		bss->networktype = htole32(NDIS802_11DS);
	else
		bss->networktype = htole32(NDIS802_11OFDM24);
	bss->config.len = htole32(sizeof(bss->config));
	bss->config.bintval = htole32(ni->ni_intval);
	bss->config.dsconfig = htole32(ieee80211_chan2ieee(ic, ni->ni_chan));
	bss->inframode = htole32(NDIS802_11INFRASTRUCTURE);
	/* XXX verify how this is supposed to look! */
	memcpy(bss->supprates, ni->ni_rates.rs_rates,
	    ni->ni_rates.rs_nrates);
	/* Write the fixed fields of the beacon frame. */
	fixed = (struct ndis_802_11_fixed_ies *)&bss[1];
	memcpy(&fixed->tstamp, ni->ni_tstamp.data, 8);
	fixed->bintval = htole16(ni->ni_intval);
	fixed->capabilities = htole16(ni->ni_capinfo);
	/* Write IEs to be included in the association request. */
	frm = (uint8_t *)&fixed[1];
	frm = ieee80211_add_rsn(frm, vap);
	frm = ieee80211_add_wpa(frm, vap);
	frm = ieee80211_add_qos(frm, ni);
	if ((ic->ic_flags & IEEE80211_F_WME) &&
	    (ni->ni_ies.wme_ie != NULL))
		frm = ieee80211_add_wme_info(frm, &ic->ic_wme);
	if (ni->ni_flags & IEEE80211_NODE_HT) {
		frm = ieee80211_add_htcap(frm, ni);
		frm = ieee80211_add_htinfo(frm, ni);
	}
	bss->ieslen = htole32(frm - (uint8_t *)fixed);
	bss->len = htole32(((frm - buf) + 3) & ~3);
	RSU_DPRINTF(sc, RSU_DEBUG_RESET | RSU_DEBUG_FWCMD,
	    "%s: sending join bss command to %s chan %d\n",
	    __func__,
	    ether_sprintf(bss->macaddr), le32toh(bss->config.dsconfig));
	return (rsu_fw_cmd(sc, R92S_CMD_JOIN_BSS, buf, sizeof(buf)));
}

static int
rsu_disconnect(struct rsu_softc *sc)
{
	uint32_t zero = 0;	/* :-) */

	/* Disassociate from our current BSS. */
	RSU_DPRINTF(sc, RSU_DEBUG_STATE | RSU_DEBUG_FWCMD,
	    "%s: sending disconnect command\n", __func__);
	return (rsu_fw_cmd(sc, R92S_CMD_DISCONNECT, &zero, sizeof(zero)));
}

/*
 * Map the hardware provided RSSI value to a signal level.
 * For the most part it's just something we divide by and cap
 * so it doesn't overflow the representation by net80211.
 */
static int
rsu_hwrssi_to_rssi(struct rsu_softc *sc, int hw_rssi)
{
	int v;

	if (hw_rssi == 0)
		return (0);
	v = hw_rssi >> 4;
	if (v > 80)
		v = 80;
	return (v);
}

static void
rsu_event_survey(struct rsu_softc *sc, uint8_t *buf, int len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ndis_wlan_bssid_ex *bss;
	struct ieee80211_rx_stats rxs;
	struct mbuf *m;
	int pktlen;

	if (__predict_false(len < sizeof(*bss)))
		return;
	bss = (struct ndis_wlan_bssid_ex *)buf;
	if (__predict_false(len < sizeof(*bss) + le32toh(bss->ieslen)))
		return;

	RSU_DPRINTF(sc, RSU_DEBUG_SCAN,
	    "%s: found BSS %s: len=%d chan=%d inframode=%d "
	    "networktype=%d privacy=%d, RSSI=%d\n",
	    __func__,
	    ether_sprintf(bss->macaddr), le32toh(bss->len),
	    le32toh(bss->config.dsconfig), le32toh(bss->inframode),
	    le32toh(bss->networktype), le32toh(bss->privacy),
	    le32toh(bss->rssi));

	/* Build a fake beacon frame to let net80211 do all the parsing. */
	/* XXX TODO: just call the new scan API methods! */
	pktlen = sizeof(*wh) + le32toh(bss->ieslen);
	if (__predict_false(pktlen > MCLBYTES))
		return;
	m = m_get2(pktlen, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (__predict_false(m == NULL))
		return;
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	USETW(wh->i_dur, 0);
	IEEE80211_ADDR_COPY(wh->i_addr1, ieee80211broadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, bss->macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, bss->macaddr);
	*(uint16_t *)wh->i_seq = 0;
	memcpy(&wh[1], (uint8_t *)&bss[1], le32toh(bss->ieslen));

	/* Finalize mbuf. */
	m->m_pkthdr.len = m->m_len = pktlen;

	/* Set channel flags for input path */
	bzero(&rxs, sizeof(rxs));
	rxs.r_flags |= IEEE80211_R_IEEE | IEEE80211_R_FREQ;
	rxs.r_flags |= IEEE80211_R_NF | IEEE80211_R_RSSI;
	rxs.c_ieee = le32toh(bss->config.dsconfig);
	rxs.c_freq = ieee80211_ieee2mhz(rxs.c_ieee, IEEE80211_CHAN_2GHZ);
	/* This is a number from 0..100; so let's just divide it down a bit */
	rxs.rssi = le32toh(bss->rssi) / 2;
	rxs.nf = -96;

	/* XXX avoid a LOR */
	RSU_UNLOCK(sc);
	ieee80211_input_mimo_all(ic, m, &rxs);
	RSU_LOCK(sc);
}

static void
rsu_event_join_bss(struct rsu_softc *sc, uint8_t *buf, int len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = vap->iv_bss;
	struct r92s_event_join_bss *rsp;
	uint32_t tmp;
	int res;

	if (__predict_false(len < sizeof(*rsp)))
		return;
	rsp = (struct r92s_event_join_bss *)buf;
	res = (int)le32toh(rsp->join_res);

	RSU_DPRINTF(sc, RSU_DEBUG_STATE | RSU_DEBUG_FWCMD,
	    "%s: Rx join BSS event len=%d res=%d\n",
	    __func__, len, res);

	/*
	 * XXX Don't do this; there's likely a better way to tell
	 * the caller we failed.
	 */
	if (res <= 0) {
		RSU_UNLOCK(sc);
		ieee80211_new_state(vap, IEEE80211_S_SCAN, -1);
		RSU_LOCK(sc);
		return;
	}

	tmp = le32toh(rsp->associd);
	if (tmp >= vap->iv_max_aid) {
		DPRINTF("Assoc ID overflow\n");
		tmp = 1;
	}
	RSU_DPRINTF(sc, RSU_DEBUG_STATE | RSU_DEBUG_FWCMD,
	    "%s: associated with %s associd=%d\n",
	    __func__, ether_sprintf(rsp->bss.macaddr), tmp);
	/* XXX is this required? What's the top two bits for again? */
	ni->ni_associd = tmp | 0xc000;
	RSU_UNLOCK(sc);
	ieee80211_new_state(vap, IEEE80211_S_RUN,
	    IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
	RSU_LOCK(sc);
}

static void
rsu_event_addba_req_report(struct rsu_softc *sc, uint8_t *buf, int len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct r92s_add_ba_event *ba = (void *) buf;
	struct ieee80211_node *ni;

	if (len < sizeof(*ba)) {
		device_printf(sc->sc_dev, "%s: short read (%d)\n", __func__, len);
		return;
	}

	if (vap == NULL)
		return;

	RSU_DPRINTF(sc, RSU_DEBUG_AMPDU, "%s: mac=%s, tid=%d, ssn=%d\n",
	    __func__,
	    ether_sprintf(ba->mac_addr),
	    (int) ba->tid,
	    (int) le16toh(ba->ssn));

	/* XXX do node lookup; this is STA specific */

	ni = ieee80211_ref_node(vap->iv_bss);
	ieee80211_ampdu_rx_start_ext(ni, ba->tid, le16toh(ba->ssn) >> 4, 32);
	ieee80211_free_node(ni);
}

static void
rsu_rx_event(struct rsu_softc *sc, uint8_t code, uint8_t *buf, int len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	RSU_DPRINTF(sc, RSU_DEBUG_RX | RSU_DEBUG_FWCMD,
	    "%s: Rx event code=%d len=%d\n", __func__, code, len);
	switch (code) {
	case R92S_EVT_SURVEY:
		rsu_event_survey(sc, buf, len);
		break;
	case R92S_EVT_SURVEY_DONE:
		RSU_DPRINTF(sc, RSU_DEBUG_SCAN,
		    "%s: site survey pass %d done, found %d BSS\n",
		    __func__, sc->sc_scan_pass, le32toh(*(uint32_t *)buf));
		sc->sc_scanning = 0;
		if (vap->iv_state != IEEE80211_S_SCAN)
			break;	/* Ignore if not scanning. */

		/*
		 * XXX TODO: This needs to be done without a transition to
		 * the SCAN state again.  Grr.
		 */
		if (sc->sc_scan_pass == 0 && vap->iv_des_nssid != 0) {
			/* Schedule a directed scan for hidden APs. */
			/* XXX bad! */
			sc->sc_scan_pass = 1;
			RSU_UNLOCK(sc);
			ieee80211_new_state(vap, IEEE80211_S_SCAN, -1);
			RSU_LOCK(sc);
			break;
		}
		sc->sc_scan_pass = 0;
		break;
	case R92S_EVT_JOIN_BSS:
		if (vap->iv_state == IEEE80211_S_AUTH)
			rsu_event_join_bss(sc, buf, len);
		break;
	case R92S_EVT_DEL_STA:
		RSU_DPRINTF(sc, RSU_DEBUG_FWCMD | RSU_DEBUG_STATE,
		    "%s: disassociated from %s\n", __func__,
		    ether_sprintf(buf));
		if (vap->iv_state == IEEE80211_S_RUN &&
		    IEEE80211_ADDR_EQ(vap->iv_bss->ni_bssid, buf)) {
			RSU_UNLOCK(sc);
			ieee80211_new_state(vap, IEEE80211_S_SCAN, -1);
			RSU_LOCK(sc);
		}
		break;
	case R92S_EVT_WPS_PBC:
		RSU_DPRINTF(sc, RSU_DEBUG_RX | RSU_DEBUG_FWCMD,
		    "%s: WPS PBC pushed.\n", __func__);
		break;
	case R92S_EVT_FWDBG:
		buf[60] = '\0';
		RSU_DPRINTF(sc, RSU_DEBUG_FWDBG, "FWDBG: %s\n", (char *)buf);
		break;
	case R92S_EVT_ADDBA_REQ_REPORT:
		rsu_event_addba_req_report(sc, buf, len);
		break;
	default:
		device_printf(sc->sc_dev, "%s: unhandled code (%d)\n", __func__, code);
		break;
	}
}

static void
rsu_rx_multi_event(struct rsu_softc *sc, uint8_t *buf, int len)
{
	struct r92s_fw_cmd_hdr *cmd;
	int cmdsz;

	RSU_DPRINTF(sc, RSU_DEBUG_RX, "%s: Rx events len=%d\n", __func__, len);

	/* Skip Rx status. */
	buf += sizeof(struct r92s_rx_stat);
	len -= sizeof(struct r92s_rx_stat);

	/* Process all events. */
	for (;;) {
		/* Check that command header fits. */
		if (__predict_false(len < sizeof(*cmd)))
			break;
		cmd = (struct r92s_fw_cmd_hdr *)buf;
		/* Check that command payload fits. */
		cmdsz = le16toh(cmd->len);
		if (__predict_false(len < sizeof(*cmd) + cmdsz))
			break;

		/* Process firmware event. */
		rsu_rx_event(sc, cmd->code, (uint8_t *)&cmd[1], cmdsz);

		if (!(cmd->seq & R92S_FW_CMD_MORE))
			break;
		buf += sizeof(*cmd) + cmdsz;
		len -= sizeof(*cmd) + cmdsz;
	}
}

#if 0
static int8_t
rsu_get_rssi(struct rsu_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 14, -2, -20, -40 };
	struct r92s_rx_phystat *phy;
	struct r92s_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	if (rate <= 3) {
		cck = (struct r92s_rx_cck *)physt;
		rpt = (cck->agc_rpt >> 6) & 0x3;
		rssi = cck->agc_rpt & 0x3e;
		rssi = cckoff[rpt] - rssi;
	} else {	/* OFDM/HT. */
		phy = (struct r92s_rx_phystat *)physt;
		rssi = ((le32toh(phy->phydw1) >> 1) & 0x7f) - 106;
	}
	return (rssi);
}
#endif

static struct mbuf *
rsu_rx_frame(struct rsu_softc *sc, uint8_t *buf, int pktlen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct r92s_rx_stat *stat;
	uint32_t rxdw0, rxdw3;
	struct mbuf *m;
	uint8_t rate;
	int infosz;

	stat = (struct r92s_rx_stat *)buf;
	rxdw0 = le32toh(stat->rxdw0);
	rxdw3 = le32toh(stat->rxdw3);

	if (__predict_false(rxdw0 & R92S_RXDW0_CRCERR)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return NULL;
	}
	if (__predict_false(pktlen < sizeof(*wh) || pktlen > MCLBYTES)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return NULL;
	}

	rate = MS(rxdw3, R92S_RXDW3_RATE);
	infosz = MS(rxdw0, R92S_RXDW0_INFOSZ) * 8;

#if 0
	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0)
		*rssi = rsu_get_rssi(sc, rate, &stat[1]);
	else
		*rssi = 0;
#endif

	RSU_DPRINTF(sc, RSU_DEBUG_RX,
	    "%s: Rx frame len=%d rate=%d infosz=%d\n",
	    __func__, pktlen, rate, infosz);

	m = m_get2(pktlen, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (__predict_false(m == NULL)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return NULL;
	}
	/* Hardware does Rx TCP checksum offload. */
	if (rxdw3 & R92S_RXDW3_TCPCHKVALID) {
		if (__predict_true(rxdw3 & R92S_RXDW3_TCPCHKRPT))
			m->m_pkthdr.csum_flags |= CSUM_DATA_VALID;
	}
	wh = (struct ieee80211_frame *)((uint8_t *)&stat[1] + infosz);
	memcpy(mtod(m, uint8_t *), wh, pktlen);
	m->m_pkthdr.len = m->m_len = pktlen;

	if (ieee80211_radiotap_active(ic)) {
		struct rsu_rx_radiotap_header *tap = &sc->sc_rxtap;

		/* Map HW rate index to 802.11 rate. */
		tap->wr_flags = 2;
		if (!(rxdw3 & R92S_RXDW3_HTC)) {
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
#if 0
		tap->wr_dbm_antsignal = *rssi;
#endif
		/* XXX not nice */
		tap->wr_dbm_antsignal = rsu_hwrssi_to_rssi(sc, sc->sc_currssi);
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
	}

	return (m);
}

static struct mbuf *
rsu_rx_multi_frame(struct rsu_softc *sc, uint8_t *buf, int len)
{
	struct r92s_rx_stat *stat;
	uint32_t rxdw0;
	int totlen, pktlen, infosz, npkts;
	struct mbuf *m, *m0 = NULL, *prevm = NULL;

	/* Get the number of encapsulated frames. */
	stat = (struct r92s_rx_stat *)buf;
	npkts = MS(le32toh(stat->rxdw2), R92S_RXDW2_PKTCNT);
	RSU_DPRINTF(sc, RSU_DEBUG_RX,
	    "%s: Rx %d frames in one chunk\n", __func__, npkts);

	/* Process all of them. */
	while (npkts-- > 0) {
		if (__predict_false(len < sizeof(*stat)))
			break;
		stat = (struct r92s_rx_stat *)buf;
		rxdw0 = le32toh(stat->rxdw0);

		pktlen = MS(rxdw0, R92S_RXDW0_PKTLEN);
		if (__predict_false(pktlen == 0))
			break;

		infosz = MS(rxdw0, R92S_RXDW0_INFOSZ) * 8;

		/* Make sure everything fits in xfer. */
		totlen = sizeof(*stat) + infosz + pktlen;
		if (__predict_false(totlen > len))
			break;

		/* Process 802.11 frame. */
		m = rsu_rx_frame(sc, buf, pktlen);
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

static struct mbuf *
rsu_rxeof(struct usb_xfer *xfer, struct rsu_data *data)
{
	struct rsu_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92s_rx_stat *stat;
	int len;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	if (__predict_false(len < sizeof(*stat))) {
		DPRINTF("xfer too short %d\n", len);
		counter_u64_add(ic->ic_ierrors, 1);
		return (NULL);
	}
	/* Determine if it is a firmware C2H event or an 802.11 frame. */
	stat = (struct r92s_rx_stat *)data->buf;
	if ((le32toh(stat->rxdw1) & 0x1ff) == 0x1ff) {
		rsu_rx_multi_event(sc, data->buf, len);
		/* No packets to process. */
		return (NULL);
	} else
		return (rsu_rx_multi_frame(sc, data->buf, len));
}

static void
rsu_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct rsu_softc *sc = usbd_xfer_softc(xfer);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m = NULL, *next;
	struct rsu_data *data;

	RSU_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_rx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&sc->sc_rx_active, next);
		m = rsu_rxeof(xfer, data);
		STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		/*
		 * XXX TODO: if we have an mbuf list, but then
		 * we hit data == NULL, what now?
		 */
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
		RSU_UNLOCK(sc);
		while (m != NULL) {
			int rssi;

			/* Cheat and get the last calibrated RSSI */
			rssi = rsu_hwrssi_to_rssi(sc, sc->sc_currssi);

			next = m->m_next;
			m->m_next = NULL;
			wh = mtod(m, struct ieee80211_frame *);
			ni = ieee80211_find_rxnode(ic,
			    (struct ieee80211_frame_min *)wh);
			if (ni != NULL) {
				if (ni->ni_flags & IEEE80211_NODE_HT)
					m->m_flags |= M_AMPDU;
				(void)ieee80211_input(ni, m, rssi, -96);
				ieee80211_free_node(ni);
			} else
				(void)ieee80211_input_all(ic, m, rssi, -96);
			m = next;
		}
		RSU_LOCK(sc);
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
			counter_u64_add(ic->ic_ierrors, 1);
			goto tr_setup;
		}
		break;
	}

}

static void
rsu_txeof(struct usb_xfer *xfer, struct rsu_data *data)
{
#ifdef	USB_DEBUG
	struct rsu_softc *sc = usbd_xfer_softc(xfer);
#endif

	RSU_DPRINTF(sc, RSU_DEBUG_TXDONE, "%s: called; data=%p\n",
	    __func__,
	    data);

	if (data->m) {
		/* XXX status? */
		ieee80211_tx_complete(data->ni, data->m, 0);
		data->m = NULL;
		data->ni = NULL;
	}
}

static void
rsu_bulk_tx_callback_sub(struct usb_xfer *xfer, usb_error_t error,
    uint8_t which)
{
	struct rsu_softc *sc = usbd_xfer_softc(xfer);
	struct ieee80211com *ic = &sc->sc_ic;
	struct rsu_data *data;

	RSU_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_tx_active[which]);
		if (data == NULL)
			goto tr_setup;
		RSU_DPRINTF(sc, RSU_DEBUG_TXDONE, "%s: transfer done %p\n",
		    __func__, data);
		STAILQ_REMOVE_HEAD(&sc->sc_tx_active[which], next);
		rsu_txeof(xfer, data);
		rsu_freebuf(sc, data);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&sc->sc_tx_pending[which]);
		if (data == NULL) {
			RSU_DPRINTF(sc, RSU_DEBUG_TXDONE,
			    "%s: empty pending queue sc %p\n", __func__, sc);
			return;
		}
		STAILQ_REMOVE_HEAD(&sc->sc_tx_pending[which], next);
		STAILQ_INSERT_TAIL(&sc->sc_tx_active[which], data, next);
		usbd_xfer_set_frame_data(xfer, 0, data->buf, data->buflen);
		RSU_DPRINTF(sc, RSU_DEBUG_TXDONE,
		    "%s: submitting transfer %p\n",
		    __func__,
		    data);
		usbd_transfer_submit(xfer);
		break;
	default:
		data = STAILQ_FIRST(&sc->sc_tx_active[which]);
		if (data != NULL) {
			STAILQ_REMOVE_HEAD(&sc->sc_tx_active[which], next);
			rsu_txeof(xfer, data);
			rsu_freebuf(sc, data);
		}
		counter_u64_add(ic->ic_oerrors, 1);

		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}

	/*
	 * XXX TODO: if the queue is low, flush out FF TX frames.
	 * Remember to unlock the driver for now; net80211 doesn't
	 * defer it for us.
	 */
}

static void
rsu_bulk_tx_callback_be_bk(struct usb_xfer *xfer, usb_error_t error)
{
	struct rsu_softc *sc = usbd_xfer_softc(xfer);

	rsu_bulk_tx_callback_sub(xfer, error, RSU_BULK_TX_BE_BK);

	/* This kicks the TX taskqueue */
	rsu_start(sc);
}

static void
rsu_bulk_tx_callback_vi_vo(struct usb_xfer *xfer, usb_error_t error)
{
	struct rsu_softc *sc = usbd_xfer_softc(xfer);

	rsu_bulk_tx_callback_sub(xfer, error, RSU_BULK_TX_VI_VO);

	/* This kicks the TX taskqueue */
	rsu_start(sc);
}

static void
rsu_bulk_tx_callback_h2c(struct usb_xfer *xfer, usb_error_t error)
{
	struct rsu_softc *sc = usbd_xfer_softc(xfer);

	rsu_bulk_tx_callback_sub(xfer, error, RSU_BULK_TX_H2C);

	/* This kicks the TX taskqueue */
	rsu_start(sc);
}

/*
 * Transmit the given frame.
 *
 * This doesn't free the node or mbuf upon failure.
 */
static int
rsu_tx_start(struct rsu_softc *sc, struct ieee80211_node *ni, 
    struct mbuf *m0, struct rsu_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
        struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct r92s_tx_desc *txd;
	uint8_t type;
	int prio = 0;
	uint8_t which;
	int hasqos;
	int xferlen;
	int qid;

	RSU_ASSERT_LOCKED(sc);

	wh = mtod(m0, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	RSU_DPRINTF(sc, RSU_DEBUG_TX, "%s: data=%p, m=%p\n",
	    __func__, data, m0);

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			device_printf(sc->sc_dev,
			    "ieee80211_crypto_encap returns NULL.\n");
			/* XXX we don't expect the fragmented frames */
			return (ENOBUFS);
		}
		wh = mtod(m0, struct ieee80211_frame *);
	}
	/* If we have QoS then use it */
	/* XXX TODO: mbuf WME/PRI versus TID? */
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		/* Has QoS */
		prio = M_WME_GETAC(m0);
		which = rsu_wme_ac_xfer_map[prio];
		hasqos = 1;
	} else {
		/* Non-QoS TID */
		/* XXX TODO: tid=0 for non-qos TID? */
		which = rsu_wme_ac_xfer_map[WME_AC_BE];
		hasqos = 0;
		prio = 0;
	}

	qid = rsu_ac2qid[prio];
#if 0
	switch (type) {
	case IEEE80211_FC0_TYPE_CTL:
	case IEEE80211_FC0_TYPE_MGT:
		which = rsu_wme_ac_xfer_map[WME_AC_VO];
		break;
	default:
		which = rsu_wme_ac_xfer_map[M_WME_GETAC(m0)];
		break;
	}
	hasqos = 0;
#endif

	RSU_DPRINTF(sc, RSU_DEBUG_TX, "%s: pri=%d, which=%d, hasqos=%d\n",
	    __func__,
	    prio,
	    which,
	    hasqos);

	/* Fill Tx descriptor. */
	txd = (struct r92s_tx_desc *)data->buf;
	memset(txd, 0, sizeof(*txd));

	txd->txdw0 |= htole32(
	    SM(R92S_TXDW0_PKTLEN, m0->m_pkthdr.len) |
	    SM(R92S_TXDW0_OFFSET, sizeof(*txd)) |
	    R92S_TXDW0_OWN | R92S_TXDW0_FSG | R92S_TXDW0_LSG);

	txd->txdw1 |= htole32(
	    SM(R92S_TXDW1_MACID, R92S_MACID_BSS) | SM(R92S_TXDW1_QSEL, qid));
	if (!hasqos)
		txd->txdw1 |= htole32(R92S_TXDW1_NONQOS);
#ifdef notyet
	if (k != NULL) {
		switch (k->wk_cipher->ic_cipher) {
		case IEEE80211_CIPHER_WEP:
			cipher = R92S_TXDW1_CIPHER_WEP;
			break;
		case IEEE80211_CIPHER_TKIP:
			cipher = R92S_TXDW1_CIPHER_TKIP;
			break;
		case IEEE80211_CIPHER_AES_CCM:
			cipher = R92S_TXDW1_CIPHER_AES;
			break;
		default:
			cipher = R92S_TXDW1_CIPHER_NONE;
		}
		txd->txdw1 |= htole32(
		    SM(R92S_TXDW1_CIPHER, cipher) |
		    SM(R92S_TXDW1_KEYIDX, k->k_id));
	}
#endif
	/* XXX todo: set AGGEN bit if appropriate? */
	txd->txdw2 |= htole32(R92S_TXDW2_BK);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw2 |= htole32(R92S_TXDW2_BMCAST);
	/*
	 * Firmware will use and increment the sequence number for the
	 * specified priority.
	 */
	txd->txdw3 |= htole32(SM(R92S_TXDW3_SEQ, prio));

	if (ieee80211_radiotap_active_vap(vap)) {
		struct rsu_tx_radiotap_header *tap = &sc->sc_txtap;

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
	STAILQ_INSERT_TAIL(&sc->sc_tx_pending[which], data, next);

	/* start transfer, if any */
	usbd_transfer_start(sc->sc_xfer[which]);
	return (0);
}

static int
rsu_transmit(struct ieee80211com *ic, struct mbuf *m)   
{
	struct rsu_softc *sc = ic->ic_softc;
	int error;

	RSU_LOCK(sc);
	if (!sc->sc_running) {
		RSU_UNLOCK(sc);
		return (ENXIO);
	}

	/*
	 * XXX TODO: ensure that we treat 'm' as a list of frames
	 * to transmit!
	 */
	error = mbufq_enqueue(&sc->sc_snd, m);
	if (error) {
		RSU_DPRINTF(sc, RSU_DEBUG_TX,
		    "%s: mbufq_enable: failed (%d)\n",
		    __func__,
		    error);
		RSU_UNLOCK(sc);
		return (error);
	}
	RSU_UNLOCK(sc);

	/* This kicks the TX taskqueue */
	rsu_start(sc);

	return (0);
}

static void
rsu_drain_mbufq(struct rsu_softc *sc)
{
	struct mbuf *m;
	struct ieee80211_node *ni;

	RSU_ASSERT_LOCKED(sc);
	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;
		ieee80211_free_node(ni);
		m_freem(m);
	}
}

static void
_rsu_start(struct rsu_softc *sc)
{
	struct ieee80211_node *ni;
	struct rsu_data *bf;
	struct mbuf *m;

	RSU_ASSERT_LOCKED(sc);

	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		bf = rsu_getbuf(sc);
		if (bf == NULL) {
			RSU_DPRINTF(sc, RSU_DEBUG_TX,
			    "%s: failed to get buffer\n", __func__);
			mbufq_prepend(&sc->sc_snd, m);
			break;
		}

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;

		if (rsu_tx_start(sc, ni, m, bf) != 0) {
			RSU_DPRINTF(sc, RSU_DEBUG_TX,
			    "%s: failed to transmit\n", __func__);
			if_inc_counter(ni->ni_vap->iv_ifp,
			    IFCOUNTER_OERRORS, 1);
			rsu_freebuf(sc, bf);
			ieee80211_free_node(ni);
			m_freem(m);
			break;
		}
	}
}

static void
rsu_start(struct rsu_softc *sc)
{

	taskqueue_enqueue(taskqueue_thread, &sc->tx_task);
}

static void
rsu_parent(struct ieee80211com *ic)
{
	struct rsu_softc *sc = ic->ic_softc;
	int startall = 0;

	RSU_LOCK(sc);
	if (ic->ic_nrunning > 0) {
		if (!sc->sc_running) {
			rsu_init(sc);
			startall = 1;
		}
	} else if (sc->sc_running)
		rsu_stop(sc);
	RSU_UNLOCK(sc);

	if (startall)
		ieee80211_start_all(ic);
}

/*
 * Power on sequence for A-cut adapters.
 */
static void
rsu_power_on_acut(struct rsu_softc *sc)
{
	uint32_t reg;

	rsu_write_1(sc, R92S_SPS0_CTRL + 1, 0x53);
	rsu_write_1(sc, R92S_SPS0_CTRL + 0, 0x57);

	/* Enable AFE macro block's bandgap and Mbias. */
	rsu_write_1(sc, R92S_AFE_MISC,
	    rsu_read_1(sc, R92S_AFE_MISC) |
	    R92S_AFE_MISC_BGEN | R92S_AFE_MISC_MBEN);
	/* Enable LDOA15 block. */
	rsu_write_1(sc, R92S_LDOA15_CTRL,
	    rsu_read_1(sc, R92S_LDOA15_CTRL) | R92S_LDA15_EN);

	rsu_write_1(sc, R92S_SPS1_CTRL,
	    rsu_read_1(sc, R92S_SPS1_CTRL) | R92S_SPS1_LDEN);
	rsu_ms_delay(sc, 2000);
	/* Enable switch regulator block. */
	rsu_write_1(sc, R92S_SPS1_CTRL,
	    rsu_read_1(sc, R92S_SPS1_CTRL) | R92S_SPS1_SWEN);

	rsu_write_4(sc, R92S_SPS1_CTRL, 0x00a7b267);

	rsu_write_1(sc, R92S_SYS_ISO_CTRL + 1,
	    rsu_read_1(sc, R92S_SYS_ISO_CTRL + 1) | 0x08);

	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) | 0x20);

	rsu_write_1(sc, R92S_SYS_ISO_CTRL + 1,
	    rsu_read_1(sc, R92S_SYS_ISO_CTRL + 1) & ~0x90);

	/* Enable AFE clock. */
	rsu_write_1(sc, R92S_AFE_XTAL_CTRL + 1,
	    rsu_read_1(sc, R92S_AFE_XTAL_CTRL + 1) & ~0x04);
	/* Enable AFE PLL macro block. */
	rsu_write_1(sc, R92S_AFE_PLL_CTRL,
	    rsu_read_1(sc, R92S_AFE_PLL_CTRL) | 0x11);
	/* Attach AFE PLL to MACTOP/BB. */
	rsu_write_1(sc, R92S_SYS_ISO_CTRL,
	    rsu_read_1(sc, R92S_SYS_ISO_CTRL) & ~0x11);

	/* Switch to 40MHz clock instead of 80MHz. */
	rsu_write_2(sc, R92S_SYS_CLKR,
	    rsu_read_2(sc, R92S_SYS_CLKR) & ~R92S_SYS_CLKSEL);

	/* Enable MAC clock. */
	rsu_write_2(sc, R92S_SYS_CLKR,
	    rsu_read_2(sc, R92S_SYS_CLKR) |
	    R92S_MAC_CLK_EN | R92S_SYS_CLK_EN);

	rsu_write_1(sc, R92S_PMC_FSM, 0x02);

	/* Enable digital core and IOREG R/W. */
	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) | 0x08);

	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) | 0x80);

	/* Switch the control path to firmware. */
	reg = rsu_read_2(sc, R92S_SYS_CLKR);
	reg = (reg & ~R92S_SWHW_SEL) | R92S_FWHW_SEL;
	rsu_write_2(sc, R92S_SYS_CLKR, reg);

	rsu_write_2(sc, R92S_CR, 0x37fc);

	/* Fix USB RX FIFO issue. */
	rsu_write_1(sc, 0xfe5c,
	    rsu_read_1(sc, 0xfe5c) | 0x80);
	rsu_write_1(sc, 0x00ab,
	    rsu_read_1(sc, 0x00ab) | 0xc0);

	rsu_write_1(sc, R92S_SYS_CLKR,
	    rsu_read_1(sc, R92S_SYS_CLKR) & ~R92S_SYS_CPU_CLKSEL);
}

/*
 * Power on sequence for B-cut and C-cut adapters.
 */
static void
rsu_power_on_bcut(struct rsu_softc *sc)
{
	uint32_t reg;
	int ntries;

	/* Prevent eFuse leakage. */
	rsu_write_1(sc, 0x37, 0xb0);
	rsu_ms_delay(sc, 10);
	rsu_write_1(sc, 0x37, 0x30);

	/* Switch the control path to hardware. */
	reg = rsu_read_2(sc, R92S_SYS_CLKR);
	if (reg & R92S_FWHW_SEL) {
		rsu_write_2(sc, R92S_SYS_CLKR,
		    reg & ~(R92S_SWHW_SEL | R92S_FWHW_SEL));
	}
	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) & ~0x8c);
	rsu_ms_delay(sc, 1);

	rsu_write_1(sc, R92S_SPS0_CTRL + 1, 0x53);
	rsu_write_1(sc, R92S_SPS0_CTRL + 0, 0x57);

	reg = rsu_read_1(sc, R92S_AFE_MISC);
	rsu_write_1(sc, R92S_AFE_MISC, reg | R92S_AFE_MISC_BGEN);
	rsu_write_1(sc, R92S_AFE_MISC, reg | R92S_AFE_MISC_BGEN |
	    R92S_AFE_MISC_MBEN | R92S_AFE_MISC_I32_EN);

	/* Enable PLL. */
	rsu_write_1(sc, R92S_LDOA15_CTRL,
	    rsu_read_1(sc, R92S_LDOA15_CTRL) | R92S_LDA15_EN);

	rsu_write_1(sc, R92S_LDOV12D_CTRL,
	    rsu_read_1(sc, R92S_LDOV12D_CTRL) | R92S_LDV12_EN);

	rsu_write_1(sc, R92S_SYS_ISO_CTRL + 1,
	    rsu_read_1(sc, R92S_SYS_ISO_CTRL + 1) | 0x08);

	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) | 0x20);

	/* Support 64KB IMEM. */
	rsu_write_1(sc, R92S_SYS_ISO_CTRL + 1,
	    rsu_read_1(sc, R92S_SYS_ISO_CTRL + 1) & ~0x97);

	/* Enable AFE clock. */
	rsu_write_1(sc, R92S_AFE_XTAL_CTRL + 1,
	    rsu_read_1(sc, R92S_AFE_XTAL_CTRL + 1) & ~0x04);
	/* Enable AFE PLL macro block. */
	reg = rsu_read_1(sc, R92S_AFE_PLL_CTRL);
	rsu_write_1(sc, R92S_AFE_PLL_CTRL, reg | 0x11);
	rsu_ms_delay(sc, 1);
	rsu_write_1(sc, R92S_AFE_PLL_CTRL, reg | 0x51);
	rsu_ms_delay(sc, 1);
	rsu_write_1(sc, R92S_AFE_PLL_CTRL, reg | 0x11);
	rsu_ms_delay(sc, 1);

	/* Attach AFE PLL to MACTOP/BB. */
	rsu_write_1(sc, R92S_SYS_ISO_CTRL,
	    rsu_read_1(sc, R92S_SYS_ISO_CTRL) & ~0x11);

	/* Switch to 40MHz clock. */
	rsu_write_1(sc, R92S_SYS_CLKR, 0x00);
	/* Disable CPU clock and 80MHz SSC. */
	rsu_write_1(sc, R92S_SYS_CLKR,
	    rsu_read_1(sc, R92S_SYS_CLKR) | 0xa0);
	/* Enable MAC clock. */
	rsu_write_2(sc, R92S_SYS_CLKR,
	    rsu_read_2(sc, R92S_SYS_CLKR) |
	    R92S_MAC_CLK_EN | R92S_SYS_CLK_EN);

	rsu_write_1(sc, R92S_PMC_FSM, 0x02);

	/* Enable digital core and IOREG R/W. */
	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) | 0x08);

	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1,
	    rsu_read_1(sc, R92S_SYS_FUNC_EN + 1) | 0x80);

	/* Switch the control path to firmware. */
	reg = rsu_read_2(sc, R92S_SYS_CLKR);
	reg = (reg & ~R92S_SWHW_SEL) | R92S_FWHW_SEL;
	rsu_write_2(sc, R92S_SYS_CLKR, reg);

	rsu_write_2(sc, R92S_CR, 0x37fc);

	/* Fix USB RX FIFO issue. */
	rsu_write_1(sc, 0xfe5c,
	    rsu_read_1(sc, 0xfe5c) | 0x80);

	rsu_write_1(sc, R92S_SYS_CLKR,
	    rsu_read_1(sc, R92S_SYS_CLKR) & ~R92S_SYS_CPU_CLKSEL);

	rsu_write_1(sc, 0xfe1c, 0x80);

	/* Make sure TxDMA is ready to download firmware. */
	for (ntries = 0; ntries < 20; ntries++) {
		reg = rsu_read_1(sc, R92S_TCR);
		if ((reg & (R92S_TCR_IMEM_CHK_RPT | R92S_TCR_EMEM_CHK_RPT)) ==
		    (R92S_TCR_IMEM_CHK_RPT | R92S_TCR_EMEM_CHK_RPT))
			break;
		rsu_ms_delay(sc, 1);
	}
	if (ntries == 20) {
		RSU_DPRINTF(sc, RSU_DEBUG_RESET | RSU_DEBUG_TX,
		    "%s: TxDMA is not ready\n",
		    __func__);
		/* Reset TxDMA. */
		reg = rsu_read_1(sc, R92S_CR);
		rsu_write_1(sc, R92S_CR, reg & ~R92S_CR_TXDMA_EN);
		rsu_ms_delay(sc, 1);
		rsu_write_1(sc, R92S_CR, reg | R92S_CR_TXDMA_EN);
	}
}

static void
rsu_power_off(struct rsu_softc *sc)
{
	/* Turn RF off. */
	rsu_write_1(sc, R92S_RF_CTRL, 0x00);
	rsu_ms_delay(sc, 5);

	/* Turn MAC off. */
	/* Switch control path. */
	rsu_write_1(sc, R92S_SYS_CLKR + 1, 0x38);
	/* Reset MACTOP. */
	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1, 0x70);
	rsu_write_1(sc, R92S_PMC_FSM, 0x06);
	rsu_write_1(sc, R92S_SYS_ISO_CTRL + 0, 0xf9);
	rsu_write_1(sc, R92S_SYS_ISO_CTRL + 1, 0xe8);

	/* Disable AFE PLL. */
	rsu_write_1(sc, R92S_AFE_PLL_CTRL, 0x00);
	/* Disable A15V. */
	rsu_write_1(sc, R92S_LDOA15_CTRL, 0x54);
	/* Disable eFuse 1.2V. */
	rsu_write_1(sc, R92S_SYS_FUNC_EN + 1, 0x50);
	rsu_write_1(sc, R92S_LDOV12D_CTRL, 0x24);
	/* Enable AFE macro block's bandgap and Mbias. */
	rsu_write_1(sc, R92S_AFE_MISC, 0x30);
	/* Disable 1.6V LDO. */
	rsu_write_1(sc, R92S_SPS0_CTRL + 0, 0x56);
	rsu_write_1(sc, R92S_SPS0_CTRL + 1, 0x43);

	/* Firmware - tell it to switch things off */
	(void) rsu_set_fw_power_state(sc, RSU_PWR_OFF);
}

static int
rsu_fw_loadsection(struct rsu_softc *sc, const uint8_t *buf, int len)
{
	const uint8_t which = rsu_wme_ac_xfer_map[WME_AC_VO];
	struct rsu_data *data;
	struct r92s_tx_desc *txd;
	int mlen;

	while (len > 0) {
		data = rsu_getbuf(sc);
		if (data == NULL)
			return (ENOMEM);
		txd = (struct r92s_tx_desc *)data->buf;
		memset(txd, 0, sizeof(*txd));
		if (len <= RSU_TXBUFSZ - sizeof(*txd)) {
			/* Last chunk. */
			txd->txdw0 |= htole32(R92S_TXDW0_LINIP);
			mlen = len;
		} else
			mlen = RSU_TXBUFSZ - sizeof(*txd);
		txd->txdw0 |= htole32(SM(R92S_TXDW0_PKTLEN, mlen));
		memcpy(&txd[1], buf, mlen);
		data->buflen = sizeof(*txd) + mlen;
		RSU_DPRINTF(sc, RSU_DEBUG_TX | RSU_DEBUG_FW | RSU_DEBUG_RESET,
		    "%s: starting transfer %p\n",
		    __func__, data);
		STAILQ_INSERT_TAIL(&sc->sc_tx_pending[which], data, next);
		buf += mlen;
		len -= mlen;
	}
	usbd_transfer_start(sc->sc_xfer[which]);
	return (0);
}

static int
rsu_load_firmware(struct rsu_softc *sc)
{
	const struct r92s_fw_hdr *hdr;
	struct r92s_fw_priv *dmem;
	struct ieee80211com *ic = &sc->sc_ic;
	const uint8_t *imem, *emem;
	int imemsz, ememsz;
	const struct firmware *fw;
	size_t size;
	uint32_t reg;
	int ntries, error;

	if (rsu_read_1(sc, R92S_TCR) & R92S_TCR_FWRDY) {
		RSU_DPRINTF(sc, RSU_DEBUG_ANY,
		    "%s: Firmware already loaded\n",
		    __func__);
		return (0);
	}

	RSU_UNLOCK(sc);
	/* Read firmware image from the filesystem. */
	if ((fw = firmware_get("rsu-rtl8712fw")) == NULL) {
		device_printf(sc->sc_dev, 
		    "%s: failed load firmware of file rsu-rtl8712fw\n",
		    __func__);
		RSU_LOCK(sc);
		return (ENXIO);
	}
	RSU_LOCK(sc);
	size = fw->datasize;
	if (size < sizeof(*hdr)) {
		device_printf(sc->sc_dev, "firmware too short\n");
		error = EINVAL;
		goto fail;
	}
	hdr = (const struct r92s_fw_hdr *)fw->data;
	if (hdr->signature != htole16(0x8712) &&
	    hdr->signature != htole16(0x8192)) {
		device_printf(sc->sc_dev,
		    "invalid firmware signature 0x%x\n",
		    le16toh(hdr->signature));
		error = EINVAL;
		goto fail;
	}
	DPRINTF("FW V%d %02x-%02x %02x:%02x\n", le16toh(hdr->version),
	    hdr->month, hdr->day, hdr->hour, hdr->minute);

	/* Make sure that driver and firmware are in sync. */
	if (hdr->privsz != htole32(sizeof(*dmem))) {
		device_printf(sc->sc_dev, "unsupported firmware image\n");
		error = EINVAL;
		goto fail;
	}
	/* Get FW sections sizes. */
	imemsz = le32toh(hdr->imemsz);
	ememsz = le32toh(hdr->sramsz);
	/* Check that all FW sections fit in image. */
	if (size < sizeof(*hdr) + imemsz + ememsz) {
		device_printf(sc->sc_dev, "firmware too short\n");
		error = EINVAL;
		goto fail;
	}
	imem = (const uint8_t *)&hdr[1];
	emem = imem + imemsz;

	/* Load IMEM section. */
	error = rsu_fw_loadsection(sc, imem, imemsz);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not load firmware section %s\n", "IMEM");
		goto fail;
	}
	/* Wait for load to complete. */
	for (ntries = 0; ntries != 50; ntries++) {
		rsu_ms_delay(sc, 10);
		reg = rsu_read_1(sc, R92S_TCR);
		if (reg & R92S_TCR_IMEM_CODE_DONE)
			break;
	}
	if (ntries == 50) {
		device_printf(sc->sc_dev, "timeout waiting for IMEM transfer\n");
		error = ETIMEDOUT;
		goto fail;
	}
	/* Load EMEM section. */
	error = rsu_fw_loadsection(sc, emem, ememsz);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not load firmware section %s\n", "EMEM");
		goto fail;
	}
	/* Wait for load to complete. */
	for (ntries = 0; ntries != 50; ntries++) {
		rsu_ms_delay(sc, 10);
		reg = rsu_read_2(sc, R92S_TCR);
		if (reg & R92S_TCR_EMEM_CODE_DONE)
			break;
	}
	if (ntries == 50) {
		device_printf(sc->sc_dev, "timeout waiting for EMEM transfer\n");
		error = ETIMEDOUT;
		goto fail;
	}
	/* Enable CPU. */
	rsu_write_1(sc, R92S_SYS_CLKR,
	    rsu_read_1(sc, R92S_SYS_CLKR) | R92S_SYS_CPU_CLKSEL);
	if (!(rsu_read_1(sc, R92S_SYS_CLKR) & R92S_SYS_CPU_CLKSEL)) {
		device_printf(sc->sc_dev, "could not enable system clock\n");
		error = EIO;
		goto fail;
	}
	rsu_write_2(sc, R92S_SYS_FUNC_EN,
	    rsu_read_2(sc, R92S_SYS_FUNC_EN) | R92S_FEN_CPUEN);
	if (!(rsu_read_2(sc, R92S_SYS_FUNC_EN) & R92S_FEN_CPUEN)) {
		device_printf(sc->sc_dev, 
		    "could not enable microcontroller\n");
		error = EIO;
		goto fail;
	}
	/* Wait for CPU to initialize. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (rsu_read_1(sc, R92S_TCR) & R92S_TCR_IMEM_RDY)
			break;
		rsu_ms_delay(sc, 1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "timeout waiting for microcontroller\n");
		error = ETIMEDOUT;
		goto fail;
	}

	/* Update DMEM section before loading. */
	dmem = __DECONST(struct r92s_fw_priv *, &hdr->priv);
	memset(dmem, 0, sizeof(*dmem));
	dmem->hci_sel = R92S_HCI_SEL_USB | R92S_HCI_SEL_8172;
	dmem->nendpoints = sc->sc_nendpoints;
	dmem->chip_version = sc->cut;
	dmem->rf_config = sc->sc_rftype;
	dmem->vcs_type = R92S_VCS_TYPE_AUTO;
	dmem->vcs_mode = R92S_VCS_MODE_RTS_CTS;
	dmem->turbo_mode = 0;
	dmem->bw40_en = !! (ic->ic_htcaps & IEEE80211_HTCAP_CHWIDTH40);
	dmem->amsdu2ampdu_en = !! (sc->sc_ht);
	dmem->ampdu_en = !! (sc->sc_ht);
	dmem->agg_offload = !! (sc->sc_ht);
	dmem->qos_en = 1;
	dmem->ps_offload = 1;
	dmem->lowpower_mode = 1;	/* XXX TODO: configurable? */
	/* Load DMEM section. */
	error = rsu_fw_loadsection(sc, (uint8_t *)dmem, sizeof(*dmem));
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not load firmware section %s\n", "DMEM");
		goto fail;
	}
	/* Wait for load to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (rsu_read_1(sc, R92S_TCR) & R92S_TCR_DMEM_CODE_DONE)
			break;
		rsu_ms_delay(sc, 1);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for %s transfer\n",
		    "DMEM");
		error = ETIMEDOUT;
		goto fail;
	}
	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 60; ntries++) {
		if (!(rsu_read_1(sc, R92S_TCR) & R92S_TCR_FWRDY))
			break;
		rsu_ms_delay(sc, 1);
	}
	if (ntries == 60) {
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
rsu_raw_xmit(struct ieee80211_node *ni, struct mbuf *m, 
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct rsu_softc *sc = ic->ic_softc;
	struct rsu_data *bf;

	/* prevent management frames from being sent if we're not ready */
	if (!sc->sc_running) {
		m_freem(m);
		return (ENETDOWN);
	}
	RSU_LOCK(sc);
	bf = rsu_getbuf(sc);
	if (bf == NULL) {
		m_freem(m);
		RSU_UNLOCK(sc);
		return (ENOBUFS);
	}
	if (rsu_tx_start(sc, ni, m, bf) != 0) {
		m_freem(m);
		rsu_freebuf(sc, bf);
		RSU_UNLOCK(sc);
		return (EIO);
	}
	RSU_UNLOCK(sc);

	return (0);
}

static void
rsu_init(struct rsu_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	int error;
	int i;

	RSU_ASSERT_LOCKED(sc);

	/* Ensure the mbuf queue is drained */
	rsu_drain_mbufq(sc);

	/* Init host async commands ring. */
	sc->cmdq.cur = sc->cmdq.next = sc->cmdq.queued = 0;

	/* Reset power management state. */
	rsu_write_1(sc, R92S_USB_HRPWM, 0);

	/* Power on adapter. */
	if (sc->cut == 1)
		rsu_power_on_acut(sc);
	else
		rsu_power_on_bcut(sc);

	/* Load firmware. */
	error = rsu_load_firmware(sc);
	if (error != 0)
		goto fail;

	/* Enable Rx TCP checksum offload. */
	rsu_write_4(sc, R92S_RCR,
	    rsu_read_4(sc, R92S_RCR) | 0x04000000);
	/* Append PHY status. */
	rsu_write_4(sc, R92S_RCR,
	    rsu_read_4(sc, R92S_RCR) | 0x02000000);

	rsu_write_4(sc, R92S_CR,
	    rsu_read_4(sc, R92S_CR) & ~0xff000000);

	/* Use 128 bytes pages. */
	rsu_write_1(sc, 0x00b5,
	    rsu_read_1(sc, 0x00b5) | 0x01);
	/* Enable USB Rx aggregation. */
	rsu_write_1(sc, 0x00bd,
	    rsu_read_1(sc, 0x00bd) | 0x80);
	/* Set USB Rx aggregation threshold. */
	rsu_write_1(sc, 0x00d9, 0x01);
	/* Set USB Rx aggregation timeout (1.7ms/4). */
	rsu_write_1(sc, 0xfe5b, 0x04);
	/* Fix USB Rx FIFO issue. */
	rsu_write_1(sc, 0xfe5c,
	    rsu_read_1(sc, 0xfe5c) | 0x80);

	/* Set MAC address. */
	IEEE80211_ADDR_COPY(macaddr, vap ? vap->iv_myaddr : ic->ic_macaddr);
	rsu_write_region_1(sc, R92S_MACID, macaddr, IEEE80211_ADDR_LEN);

	/* It really takes 1.5 seconds for the firmware to boot: */
	rsu_ms_delay(sc, 2000);

	RSU_DPRINTF(sc, RSU_DEBUG_RESET, "%s: setting MAC address to %s\n",
	    __func__,
	    ether_sprintf(macaddr));
	error = rsu_fw_cmd(sc, R92S_CMD_SET_MAC_ADDRESS, macaddr,
	    IEEE80211_ADDR_LEN);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not set MAC address\n");
		goto fail;
	}

	/* Set PS mode fully active */
	error = rsu_set_fw_power_state(sc, RSU_PWR_ACTIVE);

	if (error != 0) {
		device_printf(sc->sc_dev, "could not set PS mode\n");
		goto fail;
	}

	sc->sc_scan_pass = 0;
	usbd_transfer_start(sc->sc_xfer[RSU_BULK_RX]);

	/* We're ready to go. */
	sc->sc_running = 1;
	sc->sc_scanning = 0;
	return;
fail:
	/* Need to stop all failed transfers, if any */
	for (i = 0; i != RSU_N_TRANSFER; i++)
		usbd_transfer_stop(sc->sc_xfer[i]);
}

static void
rsu_stop(struct rsu_softc *sc)
{
	int i;

	RSU_ASSERT_LOCKED(sc);

	sc->sc_running = 0;
	sc->sc_calibrating = 0;
	taskqueue_cancel_timeout(taskqueue_thread, &sc->calib_task, NULL);
	taskqueue_cancel(taskqueue_thread, &sc->tx_task, NULL);

	/* Power off adapter. */
	rsu_power_off(sc);

	for (i = 0; i < RSU_N_TRANSFER; i++)
		usbd_transfer_stop(sc->sc_xfer[i]);

	/* Ensure the mbuf queue is drained */
	rsu_drain_mbufq(sc);
}

/*
 * Note: usb_pause_mtx() actually releases the mutex before calling pause(),
 * which breaks any kind of driver serialisation.
 */
static void
rsu_ms_delay(struct rsu_softc *sc, int ms)
{

	//usb_pause_mtx(&sc->sc_mtx, hz / 1000);
	DELAY(ms * 1000);
}
