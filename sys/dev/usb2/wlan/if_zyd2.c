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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * ZyDAS ZD1211/ZD1211B USB WLAN driver
 *
 * NOTE: all function names beginning like "zyd_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc zyd_config_copy
#define	usb2_config_td_softc zyd_softc

#define	USB_DEBUG_VAR zyd_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/wlan/usb2_wlan.h>
#include <dev/usb2/wlan/if_zyd2_reg.h>
#include <dev/usb2/wlan/if_zyd2_fw.h>

#if USB_DEBUG
static int zyd_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, zyd, CTLFLAG_RW, 0, "USB zyd");
SYSCTL_INT(_hw_usb2_zyd, OID_AUTO, debug, CTLFLAG_RW, &zyd_debug, 0,
    "zyd debug level");
#endif

#undef INDEXES
#define	INDEXES(a) (sizeof(a) / sizeof((a)[0]))

static device_probe_t zyd_probe;
static device_attach_t zyd_attach;
static device_detach_t zyd_detach;

static usb2_callback_t zyd_intr_read_clear_stall_callback;
static usb2_callback_t zyd_intr_read_callback;
static usb2_callback_t zyd_intr_write_clear_stall_callback;
static usb2_callback_t zyd_intr_write_callback;
static usb2_callback_t zyd_bulk_read_clear_stall_callback;
static usb2_callback_t zyd_bulk_read_callback;
static usb2_callback_t zyd_bulk_write_clear_stall_callback;
static usb2_callback_t zyd_bulk_write_callback;

static usb2_config_td_command_t zyd_cfg_first_time_setup;
static usb2_config_td_command_t zyd_cfg_update_promisc;
static usb2_config_td_command_t zyd_cfg_set_chan;
static usb2_config_td_command_t zyd_cfg_pre_init;
static usb2_config_td_command_t zyd_cfg_init;
static usb2_config_td_command_t zyd_cfg_pre_stop;
static usb2_config_td_command_t zyd_cfg_stop;
static usb2_config_td_command_t zyd_config_copy;
static usb2_config_td_command_t zyd_cfg_scan_start;
static usb2_config_td_command_t zyd_cfg_scan_end;
static usb2_config_td_command_t zyd_cfg_set_rxfilter;
static usb2_config_td_command_t zyd_cfg_amrr_timeout;

static uint8_t zyd_plcp2ieee(uint8_t signal, uint8_t isofdm);
static void zyd_cfg_usbrequest(struct zyd_softc *sc, struct usb2_device_request *req, uint8_t *data);
static void zyd_cfg_usb2_intr_read(struct zyd_softc *sc, void *data, uint32_t size);
static void zyd_cfg_usb2_intr_write(struct zyd_softc *sc, const void *data, uint16_t code, uint32_t size);
static void zyd_cfg_read16(struct zyd_softc *sc, uint16_t addr, uint16_t *value);
static void zyd_cfg_read32(struct zyd_softc *sc, uint16_t addr, uint32_t *value);
static void zyd_cfg_write16(struct zyd_softc *sc, uint16_t addr, uint16_t value);
static void zyd_cfg_write32(struct zyd_softc *sc, uint16_t addr, uint32_t value);
static void zyd_cfg_rfwrite(struct zyd_softc *sc, uint32_t value);
static uint8_t zyd_cfg_uploadfirmware(struct zyd_softc *sc, const uint8_t *fw_ptr, uint32_t fw_len);
static void zyd_cfg_lock_phy(struct zyd_softc *sc);
static void zyd_cfg_unlock_phy(struct zyd_softc *sc);
static void zyd_cfg_set_beacon_interval(struct zyd_softc *sc, uint32_t interval);
static const char *zyd_rf_name(uint8_t type);
static void zyd_cfg_rf_rfmd_init(struct zyd_softc *sc, struct zyd_rf *rf);
static void zyd_cfg_rf_rfmd_switch_radio(struct zyd_softc *sc, uint8_t onoff);
static void zyd_cfg_rf_rfmd_set_channel(struct zyd_softc *sc, struct zyd_rf *rf, uint8_t channel);
static void zyd_cfg_rf_al2230_switch_radio(struct zyd_softc *sc, uint8_t onoff);
static void zyd_cfg_rf_al2230_init(struct zyd_softc *sc, struct zyd_rf *rf);
static void zyd_cfg_rf_al2230_init_b(struct zyd_softc *sc, struct zyd_rf *rf);
static void zyd_cfg_rf_al2230_set_channel(struct zyd_softc *sc, struct zyd_rf *rf, uint8_t channel);
static uint8_t zyd_cfg_rf_init_hw(struct zyd_softc *sc, struct zyd_rf *rf);
static uint8_t zyd_cfg_hw_init(struct zyd_softc *sc);
static void zyd_cfg_set_mac_addr(struct zyd_softc *sc, const uint8_t *addr);
static void zyd_cfg_switch_radio(struct zyd_softc *sc, uint8_t onoff);
static void zyd_cfg_set_bssid(struct zyd_softc *sc, uint8_t *addr);
static void zyd_start_cb(struct ifnet *ifp);
static void zyd_init_cb(void *arg);
static int zyd_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data);
static void zyd_watchdog(void *arg);
static void zyd_end_of_commands(struct zyd_softc *sc);
static void zyd_newassoc_cb(struct ieee80211_node *ni, int isnew);
static void zyd_scan_start_cb(struct ieee80211com *ic);
static void zyd_scan_end_cb(struct ieee80211com *ic);
static void zyd_set_channel_cb(struct ieee80211com *ic);
static void zyd_cfg_set_led(struct zyd_softc *sc, uint32_t which, uint8_t on);
static struct ieee80211vap *zyd_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit, int opmode, int flags, const uint8_t bssid[IEEE80211_ADDR_LEN], const uint8_t mac[IEEE80211_ADDR_LEN]);
static void zyd_vap_delete(struct ieee80211vap *);
static struct ieee80211_node *zyd_node_alloc_cb(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN]);
static void zyd_cfg_set_run(struct zyd_softc *sc, struct usb2_config_td_cc *cc);
static void zyd_fill_write_queue(struct zyd_softc *sc);
static void zyd_tx_clean_queue(struct zyd_softc *sc);
static void zyd_tx_freem(struct mbuf *m);
static void zyd_tx_mgt(struct zyd_softc *sc, struct mbuf *m, struct ieee80211_node *ni);
static struct ieee80211vap *zyd_get_vap(struct zyd_softc *sc);
static void zyd_tx_data(struct zyd_softc *sc, struct mbuf *m, struct ieee80211_node *ni);
static int zyd_raw_xmit_cb(struct ieee80211_node *ni, struct mbuf *m, const struct ieee80211_bpf_params *params);
static void zyd_setup_desc_and_tx(struct zyd_softc *sc, struct mbuf *m, uint16_t rate);
static int zyd_newstate_cb(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg);
static void zyd_cfg_amrr_start(struct zyd_softc *sc);
static void zyd_update_mcast_cb(struct ifnet *ifp);
static void zyd_update_promisc_cb(struct ifnet *ifp);

static const struct zyd_phy_pair zyd_def_phy[] = ZYD_DEF_PHY;
static const struct zyd_phy_pair zyd_def_phyB[] = ZYD_DEF_PHYB;

/* various supported device vendors/products */
#define	ZYD_ZD1211	0
#define	ZYD_ZD1211B	1

static const struct usb2_device_id zyd_devs[] = {
	/* ZYD_ZD1211 */
	{USB_VPI(USB_VENDOR_3COM2, USB_PRODUCT_3COM2_3CRUSB10075, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_WL54, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_WL159G, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_CYBERTAN, USB_PRODUCT_CYBERTAN_TG54USB, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_DRAYTEK, USB_PRODUCT_DRAYTEK_VIGOR550, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_PLANEX2, USB_PRODUCT_PLANEX2_GWUS54GD, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_PLANEX2, USB_PRODUCT_PLANEX2_GWUS54GZL, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_PLANEX3, USB_PRODUCT_PLANEX3_GWUS54GZ, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_PLANEX3, USB_PRODUCT_PLANEX3_GWUS54MINI, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_SAGEM, USB_PRODUCT_SAGEM_XG760A, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_SENAO, USB_PRODUCT_SENAO_NUB8301, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_SITECOMEU, USB_PRODUCT_SITECOMEU_WL113, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_SWEEX, USB_PRODUCT_SWEEX_ZD1211, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_TEKRAM, USB_PRODUCT_TEKRAM_QUICKWLAN, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_TEKRAM, USB_PRODUCT_TEKRAM_ZD1211_1, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_TEKRAM, USB_PRODUCT_TEKRAM_ZD1211_2, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_TWINMOS, USB_PRODUCT_TWINMOS_G240, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_UMEDIA, USB_PRODUCT_UMEDIA_ALL0298V2, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_UMEDIA, USB_PRODUCT_UMEDIA_TEW429UB_A, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_UMEDIA, USB_PRODUCT_UMEDIA_TEW429UB, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_WISTRONNEWEB, USB_PRODUCT_WISTRONNEWEB_UR055G, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ZCOM, USB_PRODUCT_ZCOM_ZD1211, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ZYDAS, USB_PRODUCT_ZYDAS_ZD1211, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ZYXEL, USB_PRODUCT_ZYXEL_AG225H, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ZYXEL, USB_PRODUCT_ZYXEL_ZYAIRG220, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ZYXEL, USB_PRODUCT_ZYXEL_G200V2, ZYD_ZD1211)},
	{USB_VPI(USB_VENDOR_ZYXEL, USB_PRODUCT_ZYXEL_G202, ZYD_ZD1211)},
	/* ZYD_ZD1211B */
	{USB_VPI(USB_VENDOR_ACCTON, USB_PRODUCT_ACCTON_SMCWUSBG, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_ACCTON, USB_PRODUCT_ACCTON_ZD1211B, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_A9T_WIFI, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D7050_V4000, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_ZD1211B, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_WUSBF54G, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_FIBERLINE, USB_PRODUCT_FIBERLINE_WL430U, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_KG54L, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_PHILIPS, USB_PRODUCT_PHILIPS_SNU5600, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_PLANEX2, USB_PRODUCT_PLANEX2_GW_US54GXS, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_SAGEM, USB_PRODUCT_SAGEM_XG76NA, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_SITECOMEU, USB_PRODUCT_SITECOMEU_ZD1211B, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_UMEDIA, USB_PRODUCT_UMEDIA_TEW429UBC1, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_USR, USB_PRODUCT_USR_USR5423, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_VTECH, USB_PRODUCT_VTECH_ZD1211B, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_ZCOM, USB_PRODUCT_ZCOM_ZD1211B, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_ZYDAS, USB_PRODUCT_ZYDAS_ZD1211B, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_ZYXEL, USB_PRODUCT_ZYXEL_M202, ZYD_ZD1211B)},
	{USB_VPI(USB_VENDOR_ZYXEL, USB_PRODUCT_ZYXEL_G220V2, ZYD_ZD1211B)},
};

static const struct usb2_config zyd_config[ZYD_N_TRANSFER] = {
	[ZYD_TR_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = ZYD_MAX_TXBUFSZ,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &zyd_bulk_write_callback,
		.ep_index = 0,
		.mh.timeout = 10000,	/* 10 seconds */
	},

	[ZYD_TR_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = ZYX_MAX_RXBUFSZ,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &zyd_bulk_read_callback,
		.ep_index = 0,
	},

	[ZYD_TR_BULK_CS_WR] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &zyd_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[ZYD_TR_BULK_CS_RD] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &zyd_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[ZYD_TR_INTR_DT_WR] = {
		.type = UE_BULK_INTR,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = sizeof(struct zyd_cmd),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &zyd_intr_write_callback,
		.mh.timeout = 1000,	/* 1 second */
		.ep_index = 1,
	},

	[ZYD_TR_INTR_DT_RD] = {
		.type = UE_BULK_INTR,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = sizeof(struct zyd_cmd),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &zyd_intr_read_callback,
		.ep_index = 1,
	},

	[ZYD_TR_INTR_CS_WR] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &zyd_intr_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[ZYD_TR_INTR_CS_RD] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &zyd_intr_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static devclass_t zyd_devclass;

static device_method_t zyd_methods[] = {
	DEVMETHOD(device_probe, zyd_probe),
	DEVMETHOD(device_attach, zyd_attach),
	DEVMETHOD(device_detach, zyd_detach),
	{0, 0}
};

static driver_t zyd_driver = {
	.name = "zyd",
	.methods = zyd_methods,
	.size = sizeof(struct zyd_softc),
};

DRIVER_MODULE(zyd, ushub, zyd_driver, zyd_devclass, NULL, 0);
MODULE_DEPEND(zyd, usb2_wlan, 1, 1, 1);
MODULE_DEPEND(zyd, usb2_core, 1, 1, 1);
MODULE_DEPEND(zyd, wlan, 1, 1, 1);
MODULE_DEPEND(zyd, wlan_amrr, 1, 1, 1);

static uint8_t
zyd_plcp2ieee(uint8_t signal, uint8_t isofdm)
{
	if (isofdm) {
		static const uint8_t ofdmrates[16] =
		{0, 0, 0, 0, 0, 0, 0, 96, 48, 24, 12, 108, 72, 36, 18};

		return ofdmrates[signal & 0xf];
	} else {
		static const uint8_t cckrates[16] =
		{0, 0, 0, 0, 4, 0, 0, 11, 0, 0, 2, 0, 0, 0, 22, 0};

		return cckrates[signal & 0xf];
	}
}

/*
 * USB request basic wrapper
 */
static void
zyd_cfg_usbrequest(struct zyd_softc *sc, struct usb2_device_request *req, uint8_t *data)
{
	usb2_error_t err;
	uint16_t length;

	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		goto error;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &sc->sc_mtx, req, data, 0, NULL, 1000);

	if (err) {

		DPRINTFN(0, "%s: device request failed, err=%s "
		    "(ignored)\n", sc->sc_name, usb2_errstr(err));

error:
		length = UGETW(req->wLength);

		if ((req->bmRequestType & UT_READ) && length) {
			bzero(data, length);
		}
	}
	return;
}

static void
zyd_intr_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[ZYD_TR_INTR_DT_RD];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~ZYD_FLAG_INTR_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

/*
 * Callback handler for interrupt transfer
 */
static void
zyd_intr_read_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct zyd_cmd *cmd = &sc->sc_intr_ibuf;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		actlen = xfer->actlen;

		DPRINTFN(3, "length=%d\n", actlen);

		if (actlen > sizeof(sc->sc_intr_ibuf)) {
			actlen = sizeof(sc->sc_intr_ibuf);
		}
		usb2_copy_out(xfer->frbuffers, 0,
		    &sc->sc_intr_ibuf, actlen);

		switch (cmd->code) {
		case htole16(ZYD_NOTIF_RETRYSTATUS):
			goto handle_notif_retrystatus;
		case htole16(ZYD_NOTIF_IORD):
			goto handle_notif_iord;
		default:
			DPRINTFN(2, "unknown indication: 0x%04x\n",
			    le16toh(cmd->code));
		}

		/* fallthrough */

	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_flags & ZYD_FLAG_INTR_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_CS_RD]);
			break;
		}
		xfer->frlengths[0] = xfer->max_data_length;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTFN(3, "error = %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= ZYD_FLAG_INTR_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_CS_RD]);
		}
		break;
	}
	return;

handle_notif_retrystatus:{

		struct zyd_notif_retry *retry = (void *)(cmd->data);
		struct ifnet *ifp = sc->sc_ifp;
		struct ieee80211vap *vap;
		struct ieee80211_node *ni;

		DPRINTF("retry intr: rate=0x%x "
		    "addr=%02x:%02x:%02x:%02x:%02x:%02x count=%d (0x%x)\n",
		    le16toh(retry->rate), retry->macaddr[0], retry->macaddr[1],
		    retry->macaddr[2], retry->macaddr[3], retry->macaddr[4],
		    retry->macaddr[5], le16toh(retry->count) & 0xff,
		    le16toh(retry->count));

		vap = zyd_get_vap(sc);
		if ((vap != NULL) && (sc->sc_amrr_timer)) {
			/*
			 * Find the node to which the packet was sent
			 * and update its retry statistics.  In BSS
			 * mode, this node is the AP we're associated
			 * to so no lookup is actually needed.
			 */
			ni = ieee80211_find_txnode(vap, retry->macaddr);
			if (ni != NULL) {
				ieee80211_amrr_tx_complete(&ZYD_NODE(ni)->amn,
				    IEEE80211_AMRR_FAILURE, 1);
				ieee80211_free_node(ni);
			}
		}
		if (retry->count & htole16(0x100)) {
			ifp->if_oerrors++;	/* too many retries */
		}
		goto tr_setup;
	}

handle_notif_iord:

	if (*(uint16_t *)cmd->data == htole16(ZYD_CR_INTERRUPT)) {
		goto tr_setup;		/* HMAC interrupt */
	}
	if (actlen < 4) {
		DPRINTFN(0, "too short, %u bytes\n", actlen);
		goto tr_setup;		/* too short */
	}
	actlen -= 4;

	sc->sc_intr_ilen = actlen;

	if (sc->sc_intr_iwakeup) {
		sc->sc_intr_iwakeup = 0;
		usb2_cv_signal(&sc->sc_intr_cv);
	} else {
		sc->sc_intr_iwakeup = 1;
	}
	/*
	 * We pause reading data from the interrupt endpoint until the
	 * data has been picked up!
	 */
	return;
}

/*
 * Interrupt call reply transfer, read
 */
static void
zyd_cfg_usb2_intr_read(struct zyd_softc *sc, void *data, uint32_t size)
{
	uint16_t actlen;
	uint16_t x;

	if (size > sizeof(sc->sc_intr_ibuf.data)) {
		DPRINTFN(0, "truncating transfer size!\n");
		size = sizeof(sc->sc_intr_ibuf.data);
	}
	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		bzero(data, size);
		goto done;
	}
	if (sc->sc_intr_iwakeup) {
		DPRINTF("got data already!\n");
		sc->sc_intr_iwakeup = 0;
		goto skip0;
	}
repeat:
	sc->sc_intr_iwakeup = 1;

	while (sc->sc_intr_iwakeup) {

		/* wait for data */

		usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_DT_RD]);

		if (usb2_cv_timedwait(&sc->sc_intr_cv,
		    &sc->sc_mtx, hz / 2)) {
			/* should not happen */
		}
		if (usb2_config_td_is_gone(&sc->sc_config_td)) {
			bzero(data, size);
			goto done;
		}
	}
skip0:
	if (size != sc->sc_intr_ilen) {
		DPRINTFN(0, "unexpected length %u != %u\n",
		    size, sc->sc_intr_ilen);
		goto repeat;
	}
	actlen = sc->sc_intr_ilen;
	actlen /= 4;

	/* verify register values */
	for (x = 0; x != actlen; x++) {
		if (sc->sc_intr_obuf.data[(2 * x)] !=
		    sc->sc_intr_ibuf.data[(4 * x)]) {
			/* invalid register */
			DPRINTFN(0, "Invalid register (1) at %u!\n", x);
			goto repeat;
		}
		if (sc->sc_intr_obuf.data[(2 * x) + 1] !=
		    sc->sc_intr_ibuf.data[(4 * x) + 1]) {
			/* invalid register */
			DPRINTFN(0, "Invalid register (2) at %u!\n", x);
			goto repeat;
		}
	}

	bcopy(sc->sc_intr_ibuf.data, data, size);

	/*
	 * We have fetched the data from the shared buffer and it is
	 * safe to restart the interrupt transfer!
	 */
	usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_DT_RD]);
done:
	return;
}

static void
zyd_intr_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[ZYD_TR_INTR_DT_WR];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~ZYD_FLAG_INTR_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
zyd_intr_write_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(3, "length=%d\n", xfer->actlen);
		goto wakeup;

	case USB_ST_SETUP:

		if (sc->sc_flags & ZYD_FLAG_INTR_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_CS_WR]);
			goto wakeup;
		}
		if (sc->sc_intr_owakeup) {
			usb2_copy_in(xfer->frbuffers, 0, &sc->sc_intr_obuf,
			    sc->sc_intr_olen);

			xfer->frlengths[0] = sc->sc_intr_olen;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		DPRINTFN(3, "error = %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= ZYD_FLAG_INTR_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_CS_WR]);
		}
		goto wakeup;
	}
	return;

wakeup:
	if (sc->sc_intr_owakeup) {
		sc->sc_intr_owakeup = 0;
		usb2_cv_signal(&sc->sc_intr_cv);
	}
	return;
}

/*
 * Interrupt transfer, write.
 *
 * Not always an "interrupt transfer". If operating in
 * full speed mode, EP4 is bulk out, not interrupt out.
 */
static void
zyd_cfg_usb2_intr_write(struct zyd_softc *sc, const void *data,
    uint16_t code, uint32_t size)
{
	if (size > sizeof(sc->sc_intr_obuf.data)) {
		DPRINTFN(0, "truncating transfer size!\n");
		size = sizeof(sc->sc_intr_obuf.data);
	}
	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		goto done;
	}
	sc->sc_intr_olen = size + 2;
	sc->sc_intr_owakeup = 1;

	sc->sc_intr_obuf.code = htole16(code);
	bcopy(data, sc->sc_intr_obuf.data, size);

	usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_DT_WR]);

	while (sc->sc_intr_owakeup) {
		if (usb2_cv_timedwait(&sc->sc_intr_cv,
		    &sc->sc_mtx, hz / 2)) {
			/* should not happen */
		}
		if (usb2_config_td_is_gone(&sc->sc_config_td)) {
			sc->sc_intr_owakeup = 0;
			goto done;
		}
	}
done:
	return;
}

static void
zyd_cfg_cmd(struct zyd_softc *sc, uint16_t code, const void *idata, uint16_t ilen,
    void *odata, uint16_t olen, uint16_t flags)
{
	zyd_cfg_usb2_intr_write(sc, idata, code, ilen);

	if (flags & ZYD_CMD_FLAG_READ) {
		zyd_cfg_usb2_intr_read(sc, odata, olen);
	}
	return;
}

static void
zyd_cfg_read16(struct zyd_softc *sc, uint16_t addr, uint16_t *value)
{
	struct zyd_pair tmp[1];

	addr = htole16(addr);
	zyd_cfg_cmd(sc, ZYD_CMD_IORD, &addr, sizeof(addr),
	    tmp, sizeof(tmp), ZYD_CMD_FLAG_READ);
	*value = le16toh(tmp[0].val);
	return;
}

static void
zyd_cfg_read32(struct zyd_softc *sc, uint16_t addr, uint32_t *value)
{
	struct zyd_pair tmp[2];
	uint16_t regs[2];

	regs[0] = ZYD_REG32_HI(addr);
	regs[1] = ZYD_REG32_LO(addr);
	regs[0] = htole16(regs[0]);
	regs[1] = htole16(regs[1]);

	zyd_cfg_cmd(sc, ZYD_CMD_IORD, regs, sizeof(regs),
	    tmp, sizeof(tmp), ZYD_CMD_FLAG_READ);
	*value = (le16toh(tmp[0].val) << 16) | le16toh(tmp[1].val);
	return;
}

static void
zyd_cfg_write16(struct zyd_softc *sc, uint16_t reg, uint16_t val)
{
	struct zyd_pair pair[1];

	pair[0].reg = htole16(reg);
	pair[0].val = htole16(val);

	zyd_cfg_cmd(sc, ZYD_CMD_IOWR, pair, sizeof(pair), NULL, 0, 0);
	return;
}

static void
zyd_cfg_write32(struct zyd_softc *sc, uint16_t reg, uint32_t val)
{
	struct zyd_pair pair[2];

	pair[0].reg = htole16(ZYD_REG32_HI(reg));
	pair[0].val = htole16(val >> 16);
	pair[1].reg = htole16(ZYD_REG32_LO(reg));
	pair[1].val = htole16(val & 0xffff);

	zyd_cfg_cmd(sc, ZYD_CMD_IOWR, pair, sizeof(pair), NULL, 0, 0);
	return;
}

/*------------------------------------------------------------------------*
 *	zyd_cfg_rfwrite - write RF registers
 *------------------------------------------------------------------------*/
static void
zyd_cfg_rfwrite(struct zyd_softc *sc, uint32_t value)
{
	struct zyd_rf *rf = &sc->sc_rf;
	struct zyd_rfwrite req;
	uint16_t cr203;
	uint16_t i;

	zyd_cfg_read16(sc, ZYD_CR203, &cr203);
	cr203 &= ~(ZYD_RF_IF_LE | ZYD_RF_CLK | ZYD_RF_DATA);

	req.code = htole16(2);
	req.width = htole16(rf->width);
	for (i = 0; i != rf->width; i++) {
		req.bit[i] = htole16(cr203);
		if (value & (1 << (rf->width - 1 - i)))
			req.bit[i] |= htole16(ZYD_RF_DATA);
	}
	zyd_cfg_cmd(sc, ZYD_CMD_RFCFG, &req, 4 + (2 * rf->width), NULL, 0, 0);
	return;
}

static void
zyd_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[ZYD_TR_BULK_DT_RD];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~ZYD_FLAG_BULK_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
zyd_bulk_read_callback_sub(struct usb2_xfer *xfer, struct zyd_ifq *mq,
    uint32_t offset, uint16_t len)
{
	enum {
		ZYD_OVERHEAD = (ZYD_HW_PADDING + IEEE80211_CRC_LEN),
	};
	struct zyd_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct zyd_plcphdr plcp;
	struct zyd_rx_stat stat;
	struct mbuf *m;

	if (len < ZYD_OVERHEAD) {
		DPRINTF("frame too "
		    "short (length=%d)\n", len);
		ifp->if_ierrors++;
		return;
	}
	usb2_copy_out(xfer->frbuffers, offset, &plcp, sizeof(plcp));
	usb2_copy_out(xfer->frbuffers, offset + len - sizeof(stat),
	    &stat, sizeof(stat));

	if (stat.flags & ZYD_RX_ERROR) {
		DPRINTF("RX status indicated "
		    "error (0x%02x)\n", stat.flags);
		ifp->if_ierrors++;
		return;
	}
	/* compute actual frame length */
	len -= ZYD_OVERHEAD;

	/* allocate a mbuf to store the frame */
	if (len > MCLBYTES) {
		DPRINTF("too large frame, "
		    "%u bytes\n", len);
		return;
	} else if (len > MHLEN)
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	else
		m = m_gethdr(M_DONTWAIT, MT_DATA);

	if (m == NULL) {
		DPRINTF("could not allocate rx mbuf\n");
		ifp->if_ierrors++;
		return;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;
	m->m_len = len;

	usb2_copy_out(xfer->frbuffers, offset +
	    sizeof(plcp), m->m_data, len);

	if (bpf_peers_present(ifp->if_bpf)) {
		struct zyd_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		if (stat.flags & (ZYD_RX_BADCRC16 | ZYD_RX_BADCRC32))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
		/* XXX toss, no way to express errors */
		if (stat.flags & ZYD_RX_DECRYPTERR)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
		tap->wr_rate =
		    zyd_plcp2ieee(plcp.signal, stat.flags & ZYD_RX_OFDM);
		tap->wr_antsignal = stat.rssi + -95;
		tap->wr_antnoise = -95;	/* XXX */

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
	}
	if (sizeof(m->m_hdr.pad) > 0) {
		m->m_hdr.pad[0] = stat.rssi;	/* XXX hack */
	}
	_IF_ENQUEUE(mq, m);

	return;
}

static void
zyd_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_node *ni;
	struct zyd_rx_desc rx_desc;
	struct zyd_ifq mq = {NULL, NULL, 0};
	struct mbuf *m;
	uint32_t offset;
	uint16_t len16;
	uint8_t x;
	uint8_t rssi;
	int8_t nf;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen < MAX(sizeof(rx_desc), ZYD_MIN_FRAGSZ)) {
			DPRINTFN(0, "xfer too short, %d bytes\n", xfer->actlen);
			ifp->if_ierrors++;
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, xfer->actlen - sizeof(rx_desc),
		    &rx_desc, sizeof(rx_desc));

		if (UGETW(rx_desc.tag) == ZYD_TAG_MULTIFRAME) {

			offset = 0;

			DPRINTFN(4, "received multi-frame transfer, "
			    "%u bytes\n", xfer->actlen);

			for (x = 0; x < ZYD_MAX_RXFRAMECNT; x++) {
				len16 = UGETW(rx_desc.len[x]);

				if ((len16 == 0) || (len16 > xfer->actlen)) {
					break;
				}
				zyd_bulk_read_callback_sub(xfer, &mq, offset, len16);

				/*
				 * next frame is aligned on a 32-bit
				 * boundary
				 */
				len16 = (len16 + 3) & ~3;
				offset += len16;
				if (len16 > xfer->actlen) {
					break;
				}
				xfer->actlen -= len16;
			}
		} else {
			DPRINTFN(4, "received single-frame transfer, "
			    "%u bytes\n", xfer->actlen);
			zyd_bulk_read_callback_sub(xfer, &mq, 0, xfer->actlen);
		}

	case USB_ST_SETUP:
tr_setup:
		DPRINTF("setup\n");

		if (sc->sc_flags & ZYD_FLAG_BULK_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_BULK_CS_RD]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}

		/*
		 * At the end of a USB callback it is always safe to unlock
		 * the private mutex of a device! That is why we do the
		 * "ieee80211_input" here, and not some lines up!
		 */
		if (mq.ifq_head) {

			mtx_unlock(&sc->sc_mtx);

			while (1) {

				_IF_DEQUEUE(&mq, m);

				if (m == NULL)
					break;

				rssi = m->m_hdr.pad[0];	/* XXX hack */

				rssi = (rssi > 63) ? 127 : 2 * rssi;
				nf = -95;	/* XXX */

				ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));
				if (ni != NULL) {
					if (ieee80211_input(ni, m, rssi, nf, 0)) {
						/* ignore */
					}
					ieee80211_free_node(ni);
				} else {
					if (ieee80211_input_all(ic, m, rssi, nf, 0)) {
						/* ignore */
					}
				}
			}

			mtx_lock(&sc->sc_mtx);
		}
		break;

	default:			/* Error */
		DPRINTF("frame error: %s\n", usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= ZYD_FLAG_BULK_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_BULK_CS_RD]);
		}
		break;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	zyd_cfg_uploadfirmware
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
zyd_cfg_uploadfirmware(struct zyd_softc *sc, const uint8_t *fw_ptr,
    uint32_t fw_len)
{
	struct usb2_device_request req;
	uint16_t temp;
	uint16_t addr;
	uint8_t stat;

	DPRINTF("firmware %p size=%u\n", fw_ptr, fw_len);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADREQ;
	USETW(req.wIndex, 0);

	temp = 64;

	addr = ZYD_FIRMWARE_START_ADDR;
	while (fw_len > 0) {

		if (fw_len < 64) {
			temp = fw_len;
		}
		DPRINTF("firmware block: fw_len=%u\n", fw_len);

		USETW(req.wValue, addr);
		USETW(req.wLength, temp);

		zyd_cfg_usbrequest(sc, &req,
		    USB_ADD_BYTES(fw_ptr, 0));

		addr += (temp / 2);
		fw_len -= temp;
		fw_ptr += temp;
	}

	/* check whether the upload succeeded */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADSTS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(stat));

	zyd_cfg_usbrequest(sc, &req, &stat);

	return ((stat & 0x80) ? 1 : 0);
}

/*
 * Driver OS interface
 */

/*
 * Probe for a ZD1211-containing product
 */
static int
zyd_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != 0) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != ZYD_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(zyd_devs, sizeof(zyd_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do
 * setup and ethernet/BPF attach.
 */
static int
zyd_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct zyd_softc *sc = device_get_softc(dev);
	int error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	if (uaa->info.bcdDevice < 0x4330) {
		device_printf(dev, "device version mismatch: 0x%X "
		    "(only >= 43.30 supported)\n",
		    uaa->info.bcdDevice);
		return (EINVAL);
	}
	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name), "%s",
	    device_get_nameunit(dev));

	sc->sc_unit = device_get_unit(dev);
	sc->sc_udev = uaa->device;
	sc->sc_mac_rev = USB_GET_DRIVER_INFO(uaa);

	mtx_init(&sc->sc_mtx, "zyd lock", MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	usb2_cv_init(&sc->sc_intr_cv, "IWAIT");

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	/*
	 * Endpoint 1 = Bulk out (512b @ high speed / 64b @ full speed)
	 * Endpoint 2 = Bulk in  (512b @ high speed / 64b @ full speed)
	 * Endpoint 3 = Intr in (64b)
	 * Endpoint 4 = Intr out @ high speed / bulk out @ full speed (64b)
	 */
	iface_index = ZYD_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, zyd_config, ZYD_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "could not allocate USB "
		    "transfers: %s\n", usb2_errstr(error));
		goto detach;
	}
	error = usb2_config_td_setup(&sc->sc_config_td, sc, &sc->sc_mtx,
	    &zyd_end_of_commands, sizeof(struct usb2_config_td_cc), 16);
	if (error) {
		device_printf(dev, "could not setup config "
		    "thread!\n");
		goto detach;
	}
	mtx_lock(&sc->sc_mtx);

	/* start setup */

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &zyd_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	zyd_watchdog(sc);

	return (0);

detach:
	zyd_detach(dev);
	return (ENXIO);
}

/*
 * Lock PHY registers
 */
static void
zyd_cfg_lock_phy(struct zyd_softc *sc)
{
	uint32_t temp;

	zyd_cfg_read32(sc, ZYD_MAC_MISC, &temp);
	temp &= ~ZYD_UNLOCK_PHY_REGS;
	zyd_cfg_write32(sc, ZYD_MAC_MISC, temp);
}

/*
 * Unlock PHY registers
 */
static void
zyd_cfg_unlock_phy(struct zyd_softc *sc)
{
	uint32_t temp;

	zyd_cfg_read32(sc, ZYD_MAC_MISC, &temp);
	temp |= ZYD_UNLOCK_PHY_REGS;
	zyd_cfg_write32(sc, ZYD_MAC_MISC, temp);
}

static void
zyd_cfg_set_beacon_interval(struct zyd_softc *sc, uint32_t bintval)
{
	/* XXX this is probably broken.. */
	zyd_cfg_write32(sc, ZYD_CR_ATIM_WND_PERIOD, bintval - 2);
	zyd_cfg_write32(sc, ZYD_CR_PRE_TBTT, bintval - 1);
	zyd_cfg_write32(sc, ZYD_CR_BCN_INTERVAL, bintval);
	return;
}

/*
 * Get RF name
 */
static const char *
zyd_rf_name(uint8_t type)
{
	static const char *const zyd_rfs[] = {
		"unknown", "unknown", "UW2451", "UCHIP", "AL2230",
		"AL7230B", "THETA", "AL2210", "MAXIM_NEW", "GCT",
		"PV2000", "RALINK", "INTERSIL", "RFMD", "MAXIM_NEW2",
		"PHILIPS"
	};

	return (zyd_rfs[(type > 15) ? 0 : type]);
}

/*
 * RF driver: Init for RFMD chip
 */
static void
zyd_cfg_rf_rfmd_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_RFMD_PHY;
	static const uint32_t rfini[] = ZYD_RFMD_RF;
	uint32_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}

	/* init RFMD radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	return;
}

/*
 * RF driver: Switch radio on/off for RFMD chip
 */
static void
zyd_cfg_rf_rfmd_switch_radio(struct zyd_softc *sc, uint8_t on)
{
	zyd_cfg_write16(sc, ZYD_CR10, on ? 0x89 : 0x15);
	zyd_cfg_write16(sc, ZYD_CR11, on ? 0x00 : 0x81);
	return;
}

/*
 * RF driver: Channel setting for RFMD chip
 */
static void
zyd_cfg_rf_rfmd_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const struct {
		uint32_t r1, r2;
	}      rfprog[] = ZYD_RFMD_CHANTABLE;

	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r1);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r2);
	return;
}

/*
 * RF driver: Switch radio on/off for AL2230 chip
 */
static void
zyd_cfg_rf_al2230_switch_radio(struct zyd_softc *sc, uint8_t on)
{
	uint8_t on251 = (sc->sc_mac_rev == ZYD_ZD1211) ? 0x3f : 0x7f;

	zyd_cfg_write16(sc, ZYD_CR11, on ? 0x00 : 0x04);
	zyd_cfg_write16(sc, ZYD_CR251, on ? on251 : 0x2f);
	return;
}

/*
 * RF driver: Init for AL2230 chip
 */
static void
zyd_cfg_rf_al2230_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY;
	static const uint32_t rfini[] = ZYD_AL2230_RF;
	uint32_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}

	/* init AL2230 radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	return;
}

static void
zyd_cfg_rf_al2230_init_b(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY_B;
	static const uint32_t rfini[] = ZYD_AL2230_RF_B;
	uint32_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}

	/* init AL2230 radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	return;
}

/*
 * RF driver: Channel setting for AL2230 chip
 */
static void
zyd_cfg_rf_al2230_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const struct {
		uint32_t r1, r2, r3;
	}      rfprog[] = ZYD_AL2230_CHANTABLE;

	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r1);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r2);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r3);

	zyd_cfg_write16(sc, ZYD_CR138, 0x28);
	zyd_cfg_write16(sc, ZYD_CR203, 0x06);
	return;
}

/*
 * AL7230B RF methods.
 */
static void
zyd_cfg_rf_al7230b_switch_radio(struct zyd_softc *sc, uint8_t on)
{
	zyd_cfg_write16(sc, ZYD_CR11, on ? 0x00 : 0x04);
	zyd_cfg_write16(sc, ZYD_CR251, on ? 0x3f : 0x2f);
	return;
}

static void
zyd_cfg_rf_al7230b_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini_1[] = ZYD_AL7230B_PHY_1;
	static const struct zyd_phy_pair phyini_2[] = ZYD_AL7230B_PHY_2;
	static const struct zyd_phy_pair phyini_3[] = ZYD_AL7230B_PHY_3;
	static const uint32_t rfini_1[] = ZYD_AL7230B_RF_1;
	static const uint32_t rfini_2[] = ZYD_AL7230B_RF_2;
	uint32_t i;

	/* for AL7230B, PHY and RF need to be initialized in "phases" */

	/* init RF-dependent PHY registers, part one */
	for (i = 0; i != INDEXES(phyini_1); i++) {
		zyd_cfg_write16(sc, phyini_1[i].reg, phyini_1[i].val);
	}
	/* init AL7230B radio, part one */
	for (i = 0; i != INDEXES(rfini_1); i++) {
		zyd_cfg_rfwrite(sc, rfini_1[i]);
	}
	/* init RF-dependent PHY registers, part two */
	for (i = 0; i != INDEXES(phyini_2); i++) {
		zyd_cfg_write16(sc, phyini_2[i].reg, phyini_2[i].val);
	}
	/* init AL7230B radio, part two */
	for (i = 0; i != INDEXES(rfini_2); i++) {
		zyd_cfg_rfwrite(sc, rfini_2[i]);
	}
	/* init RF-dependent PHY registers, part three */
	for (i = 0; i != INDEXES(phyini_3); i++) {
		zyd_cfg_write16(sc, phyini_3[i].reg, phyini_3[i].val);
	}
	return;
}

static void
zyd_cfg_rf_al7230b_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const struct {
		uint32_t r1, r2;
	}      rfprog[] = ZYD_AL7230B_CHANTABLE;
	static const uint32_t rfsc[] = ZYD_AL7230B_RF_SETCHANNEL;
	uint32_t i;

	zyd_cfg_write16(sc, ZYD_CR240, 0x57);
	zyd_cfg_write16(sc, ZYD_CR251, 0x2f);

	for (i = 0; i != INDEXES(rfsc); i++) {
		zyd_cfg_rfwrite(sc, rfsc[i]);
	}

	zyd_cfg_write16(sc, ZYD_CR128, 0x14);
	zyd_cfg_write16(sc, ZYD_CR129, 0x12);
	zyd_cfg_write16(sc, ZYD_CR130, 0x10);
	zyd_cfg_write16(sc, ZYD_CR38, 0x38);
	zyd_cfg_write16(sc, ZYD_CR136, 0xdf);

	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r1);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r2);
	zyd_cfg_rfwrite(sc, 0x3c9000);

	zyd_cfg_write16(sc, ZYD_CR251, 0x3f);
	zyd_cfg_write16(sc, ZYD_CR203, 0x06);
	zyd_cfg_write16(sc, ZYD_CR240, 0x08);

	return;
}

/*
 * AL2210 RF methods.
 */
static void
zyd_cfg_rf_al2210_switch_radio(struct zyd_softc *sc, uint8_t on)
{

}

static void
zyd_cfg_rf_al2210_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_AL2210_PHY;
	static const uint32_t rfini[] = ZYD_AL2210_RF;
	uint32_t tmp;
	uint32_t i;

	zyd_cfg_write32(sc, ZYD_CR18, 2);

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}
	/* init AL2210 radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	zyd_cfg_write16(sc, ZYD_CR47, 0x1e);
	zyd_cfg_read32(sc, ZYD_CR_RADIO_PD, &tmp);
	zyd_cfg_write32(sc, ZYD_CR_RADIO_PD, tmp & ~1);
	zyd_cfg_write32(sc, ZYD_CR_RADIO_PD, tmp | 1);
	zyd_cfg_write32(sc, ZYD_CR_RFCFG, 0x05);
	zyd_cfg_write32(sc, ZYD_CR_RFCFG, 0x00);
	zyd_cfg_write16(sc, ZYD_CR47, 0x1e);
	zyd_cfg_write32(sc, ZYD_CR18, 3);

	return;
}

static void
zyd_cfg_rf_al2210_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const uint32_t rfprog[] = ZYD_AL2210_CHANTABLE;
	uint32_t tmp;

	zyd_cfg_write32(sc, ZYD_CR18, 2);
	zyd_cfg_write16(sc, ZYD_CR47, 0x1e);
	zyd_cfg_read32(sc, ZYD_CR_RADIO_PD, &tmp);
	zyd_cfg_write32(sc, ZYD_CR_RADIO_PD, tmp & ~1);
	zyd_cfg_write32(sc, ZYD_CR_RADIO_PD, tmp | 1);
	zyd_cfg_write32(sc, ZYD_CR_RFCFG, 0x05);

	zyd_cfg_write32(sc, ZYD_CR_RFCFG, 0x00);
	zyd_cfg_write16(sc, ZYD_CR47, 0x1e);

	/* actually set the channel */
	zyd_cfg_rfwrite(sc, rfprog[channel - 1]);

	zyd_cfg_write32(sc, ZYD_CR18, 3);
	return;
}

/*
 * GCT RF methods.
 */
static void
zyd_cfg_rf_gct_switch_radio(struct zyd_softc *sc, uint8_t on)
{
	/* vendor driver does nothing for this RF chip */

	return;
}

static void
zyd_cfg_rf_gct_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_GCT_PHY;
	static const uint32_t rfini[] = ZYD_GCT_RF;
	uint32_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}
	/* init cgt radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	return;
}

static void
zyd_cfg_rf_gct_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const uint32_t rfprog[] = ZYD_GCT_CHANTABLE;

	zyd_cfg_rfwrite(sc, 0x1c0000);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1]);
	zyd_cfg_rfwrite(sc, 0x1c0008);

	return;
}

/*
 * Maxim RF methods.
 */
static void
zyd_cfg_rf_maxim_switch_radio(struct zyd_softc *sc, uint8_t on)
{
	/* vendor driver does nothing for this RF chip */

	return;
}

static void
zyd_cfg_rf_maxim_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM_RF;
	uint16_t tmp;
	uint32_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* init maxim radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return;
}

static void
zyd_cfg_rf_maxim_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM_RF;
	static const struct {
		uint32_t r1, r2;
	}      rfprog[] = ZYD_MAXIM_CHANTABLE;
	uint16_t tmp;
	uint32_t i;

	/*
	 * Do the same as we do when initializing it, except for the channel
	 * values coming from the two channel tables.
	 */

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* first two values taken from the chantables */
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r1);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r2);

	/* init maxim radio - skipping the two first values */
	if (INDEXES(rfini) > 2) {
		for (i = 2; i != INDEXES(rfini); i++) {
			zyd_cfg_rfwrite(sc, rfini[i]);
		}
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return;
}

/*
 * Maxim2 RF methods.
 */
static void
zyd_cfg_rf_maxim2_switch_radio(struct zyd_softc *sc, uint8_t on)
{
	/* vendor driver does nothing for this RF chip */
	return;
}

static void
zyd_cfg_rf_maxim2_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM2_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM2_RF;
	uint16_t tmp;
	uint32_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* init maxim2 radio */
	for (i = 0; i != INDEXES(rfini); i++) {
		zyd_cfg_rfwrite(sc, rfini[i]);
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp | (1 << 4));
	return;
}

static void
zyd_cfg_rf_maxim2_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
    uint8_t channel)
{
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM2_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM2_RF;
	static const struct {
		uint32_t r1, r2;
	}      rfprog[] = ZYD_MAXIM2_CHANTABLE;
	uint16_t tmp;
	uint32_t i;

	/*
	 * Do the same as we do when initializing it, except for the channel
	 * values coming from the two channel tables.
	 */

	/* init RF-dependent PHY registers */
	for (i = 0; i != INDEXES(phyini); i++) {
		zyd_cfg_write16(sc, phyini[i].reg, phyini[i].val);
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* first two values taken from the chantables */
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r1);
	zyd_cfg_rfwrite(sc, rfprog[channel - 1].r2);

	/* init maxim2 radio - skipping the two first values */
	if (INDEXES(rfini) > 2) {
		for (i = 2; i != INDEXES(rfini); i++) {
			zyd_cfg_rfwrite(sc, rfini[i]);
		}
	}
	zyd_cfg_read16(sc, ZYD_CR203, &tmp);
	zyd_cfg_write16(sc, ZYD_CR203, tmp | (1 << 4));
	return;
}

/*
 * Assign drivers and init the RF
 */
static uint8_t
zyd_cfg_rf_init_hw(struct zyd_softc *sc, struct zyd_rf *rf)
{
	;				/* fix for indent */

	switch (sc->sc_rf_rev) {
	case ZYD_RF_RFMD:
		rf->cfg_init_hw = zyd_cfg_rf_rfmd_init;
		rf->cfg_switch_radio = zyd_cfg_rf_rfmd_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_rfmd_set_channel;
		rf->width = 24;		/* 24-bit RF values */
		break;
	case ZYD_RF_AL2230:
		if (sc->sc_mac_rev == ZYD_ZD1211B)
			rf->cfg_init_hw = zyd_cfg_rf_al2230_init_b;
		else
			rf->cfg_init_hw = zyd_cfg_rf_al2230_init;
		rf->cfg_switch_radio = zyd_cfg_rf_al2230_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_al2230_set_channel;
		rf->width = 24;		/* 24-bit RF values */
		break;
	case ZYD_RF_AL7230B:
		rf->cfg_init_hw = zyd_cfg_rf_al7230b_init;
		rf->cfg_switch_radio = zyd_cfg_rf_al7230b_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_al7230b_set_channel;
		rf->width = 24;		/* 24-bit RF values */
		break;
	case ZYD_RF_AL2210:
		rf->cfg_init_hw = zyd_cfg_rf_al2210_init;
		rf->cfg_switch_radio = zyd_cfg_rf_al2210_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_al2210_set_channel;
		rf->width = 24;		/* 24-bit RF values */
		break;
	case ZYD_RF_GCT:
		rf->cfg_init_hw = zyd_cfg_rf_gct_init;
		rf->cfg_switch_radio = zyd_cfg_rf_gct_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_gct_set_channel;
		rf->width = 21;		/* 21-bit RF values */
		break;
	case ZYD_RF_MAXIM_NEW:
		rf->cfg_init_hw = zyd_cfg_rf_maxim_init;
		rf->cfg_switch_radio = zyd_cfg_rf_maxim_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_maxim_set_channel;
		rf->width = 18;		/* 18-bit RF values */
		break;
	case ZYD_RF_MAXIM_NEW2:
		rf->cfg_init_hw = zyd_cfg_rf_maxim2_init;
		rf->cfg_switch_radio = zyd_cfg_rf_maxim2_switch_radio;
		rf->cfg_set_channel = zyd_cfg_rf_maxim2_set_channel;
		rf->width = 18;		/* 18-bit RF values */
		break;
	default:
		DPRINTFN(0, "%s: Sorry, radio %s is not supported yet\n",
		    sc->sc_name, zyd_rf_name(sc->sc_rf_rev));
		return (1);
	}

	zyd_cfg_lock_phy(sc);
	(rf->cfg_init_hw) (sc, rf);
	zyd_cfg_unlock_phy(sc);

	return (0);			/* success */
}

/*
 * Init the hardware
 */
static uint8_t
zyd_cfg_hw_init(struct zyd_softc *sc)
{
	const struct zyd_phy_pair *phyp;
	uint32_t tmp;

	/* specify that the plug and play is finished */
	zyd_cfg_write32(sc, ZYD_MAC_AFTER_PNP, 1);

	zyd_cfg_read16(sc, ZYD_FIRMWARE_BASE_ADDR, &sc->sc_firmware_base);
	DPRINTF("firmware base address=0x%04x\n", sc->sc_firmware_base);

	/* retrieve firmware revision number */
	zyd_cfg_read16(sc, sc->sc_firmware_base + ZYD_FW_FIRMWARE_REV, &sc->sc_fw_rev);

	zyd_cfg_write32(sc, ZYD_CR_GPI_EN, 0);
	zyd_cfg_write32(sc, ZYD_MAC_CONT_WIN_LIMIT, 0x7f043f);

	/* disable interrupts */
	zyd_cfg_write32(sc, ZYD_CR_INTERRUPT, 0);

	/* PHY init */
	zyd_cfg_lock_phy(sc);
	phyp = (sc->sc_mac_rev == ZYD_ZD1211B) ? zyd_def_phyB : zyd_def_phy;
	for (; phyp->reg != 0; phyp++) {
		zyd_cfg_write16(sc, phyp->reg, phyp->val);
	}
	if (sc->sc_fix_cr157) {
		zyd_cfg_read32(sc, ZYD_EEPROM_PHY_REG, &tmp);
		zyd_cfg_write32(sc, ZYD_CR157, tmp >> 8);
	}
	zyd_cfg_unlock_phy(sc);

	/* HMAC init */
	zyd_cfg_write32(sc, ZYD_MAC_ACK_EXT, 0x00000020);
	zyd_cfg_write32(sc, ZYD_CR_ADDA_MBIAS_WT, 0x30000808);

	if (sc->sc_mac_rev == ZYD_ZD1211) {
		zyd_cfg_write32(sc, ZYD_MAC_RETRY, 0x00000002);
	} else {
		zyd_cfg_write32(sc, ZYD_MACB_MAX_RETRY, 0x02020202);
		zyd_cfg_write32(sc, ZYD_MACB_TXPWR_CTL4, 0x007f003f);
		zyd_cfg_write32(sc, ZYD_MACB_TXPWR_CTL3, 0x007f003f);
		zyd_cfg_write32(sc, ZYD_MACB_TXPWR_CTL2, 0x003f001f);
		zyd_cfg_write32(sc, ZYD_MACB_TXPWR_CTL1, 0x001f000f);
		zyd_cfg_write32(sc, ZYD_MACB_AIFS_CTL1, 0x00280028);
		zyd_cfg_write32(sc, ZYD_MACB_AIFS_CTL2, 0x008C003C);
		zyd_cfg_write32(sc, ZYD_MACB_TXOP, 0x01800824);
	}

	zyd_cfg_write32(sc, ZYD_MAC_SNIFFER, 0x00000000);
	zyd_cfg_write32(sc, ZYD_MAC_RXFILTER, 0x00000000);
	zyd_cfg_write32(sc, ZYD_MAC_GHTBL, 0x00000000);
	zyd_cfg_write32(sc, ZYD_MAC_GHTBH, 0x80000000);
	zyd_cfg_write32(sc, ZYD_MAC_MISC, 0x000000a4);
	zyd_cfg_write32(sc, ZYD_CR_ADDA_PWR_DWN, 0x0000007f);
	zyd_cfg_write32(sc, ZYD_MAC_BCNCFG, 0x00f00401);
	zyd_cfg_write32(sc, ZYD_MAC_PHY_DELAY2, 0x00000000);
	zyd_cfg_write32(sc, ZYD_MAC_ACK_EXT, 0x00000080);
	zyd_cfg_write32(sc, ZYD_CR_ADDA_PWR_DWN, 0x00000000);
	zyd_cfg_write32(sc, ZYD_MAC_SIFS_ACK_TIME, 0x00000100);
	zyd_cfg_write32(sc, ZYD_MAC_DIFS_EIFS_SIFS, 0x0547c032);
	zyd_cfg_write32(sc, ZYD_CR_RX_PE_DELAY, 0x00000070);
	zyd_cfg_write32(sc, ZYD_CR_PS_CTRL, 0x10000000);
	zyd_cfg_write32(sc, ZYD_MAC_RTSCTSRATE, 0x02030203);
	zyd_cfg_write32(sc, ZYD_MAC_RX_THRESHOLD, 0x000c0640);
	zyd_cfg_write32(sc, ZYD_MAC_BACKOFF_PROTECT, 0x00000114);

	/* init beacon interval to 100ms */
	zyd_cfg_set_beacon_interval(sc, 100);

	return (0);			/* success */
}

/*
 * Read information from EEPROM
 */
static void
zyd_cfg_read_eeprom(struct zyd_softc *sc)
{
	uint32_t tmp;
	uint16_t i;
	uint16_t val;

	/* read MAC address */
	zyd_cfg_read32(sc, ZYD_EEPROM_MAC_ADDR_P1, &tmp);
	sc->sc_myaddr[0] = tmp & 0xff;
	sc->sc_myaddr[1] = tmp >> 8;
	sc->sc_myaddr[2] = tmp >> 16;
	sc->sc_myaddr[3] = tmp >> 24;
	zyd_cfg_read32(sc, ZYD_EEPROM_MAC_ADDR_P2, &tmp);
	sc->sc_myaddr[4] = tmp & 0xff;
	sc->sc_myaddr[5] = tmp >> 8;

	zyd_cfg_read32(sc, ZYD_EEPROM_POD, &tmp);
	sc->sc_rf_rev = tmp & 0x0f;
	sc->sc_fix_cr47 = (tmp >> 8) & 0x01;
	sc->sc_fix_cr157 = (tmp >> 13) & 0x01;
	sc->sc_pa_rev = (tmp >> 16) & 0x0f;

	/* read regulatory domain (currently unused) */
	zyd_cfg_read32(sc, ZYD_EEPROM_SUBID, &tmp);
	sc->sc_regdomain = tmp >> 16;
	DPRINTF("regulatory domain %x\n", sc->sc_regdomain);

	/* read Tx power calibration tables */
	for (i = 0; i < 7; i++) {
		zyd_cfg_read16(sc, ZYD_EEPROM_PWR_CAL + i, &val);
		sc->sc_pwr_cal[(i * 2)] = val >> 8;
		sc->sc_pwr_cal[(i * 2) + 1] = val & 0xff;

		zyd_cfg_read16(sc, ZYD_EEPROM_PWR_INT + i, &val);
		sc->sc_pwr_int[(i * 2)] = val >> 8;
		sc->sc_pwr_int[(i * 2) + 1] = val & 0xff;

		zyd_cfg_read16(sc, ZYD_EEPROM_36M_CAL + i, &val);
		sc->sc_ofdm36_cal[(i * 2)] = val >> 8;
		sc->sc_ofdm36_cal[(i * 2) + 1] = val & 0xff;

		zyd_cfg_read16(sc, ZYD_EEPROM_48M_CAL + i, &val);
		sc->sc_ofdm48_cal[(i * 2)] = val >> 8;
		sc->sc_ofdm48_cal[(i * 2) + 1] = val & 0xff;

		zyd_cfg_read16(sc, ZYD_EEPROM_54M_CAL + i, &val);
		sc->sc_ofdm54_cal[(i * 2)] = val >> 8;
		sc->sc_ofdm54_cal[(i * 2) + 1] = val & 0xff;
	}
	return;
}

static void
zyd_cfg_set_mac_addr(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	zyd_cfg_write32(sc, ZYD_MAC_MACADRL, tmp);

	tmp = (addr[5] << 8) | addr[4];
	zyd_cfg_write32(sc, ZYD_MAC_MACADRH, tmp);
	return;
}

/*
 * Switch radio on/off
 */
static void
zyd_cfg_switch_radio(struct zyd_softc *sc, uint8_t onoff)
{
	zyd_cfg_lock_phy(sc);
	(sc->sc_rf.cfg_switch_radio) (sc, onoff);
	zyd_cfg_unlock_phy(sc);

	return;
}

/*
 * Set BSSID
 */
static void
zyd_cfg_set_bssid(struct zyd_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	zyd_cfg_write32(sc, ZYD_MAC_BSSADRL, tmp);

	tmp = (addr[5] << 8) | addr[4];
	zyd_cfg_write32(sc, ZYD_MAC_BSSADRH, tmp);
	return;
}

/*
 * Complete the attach process
 */
static void
zyd_cfg_first_time_setup(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct usb2_config_descriptor *cd;
	struct ieee80211com *ic;
	struct ifnet *ifp;
	const uint8_t *fw_ptr;
	uint32_t fw_len;
	uint8_t bands;
	usb2_error_t err;

	/* setup RX tap header */
	sc->sc_rxtap_len = sizeof(sc->sc_rxtap);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(ZYD_RX_RADIOTAP_PRESENT);

	/* setup TX tap header */
	sc->sc_txtap_len = sizeof(sc->sc_txtap);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(ZYD_TX_RADIOTAP_PRESENT);

	if (sc->sc_mac_rev == ZYD_ZD1211) {
		fw_ptr = zd1211_firmware;
		fw_len = sizeof(zd1211_firmware);
	} else {
		fw_ptr = zd1211b_firmware;
		fw_len = sizeof(zd1211b_firmware);
	}

	if (zyd_cfg_uploadfirmware(sc, fw_ptr, fw_len)) {
		DPRINTFN(0, "%s: could not "
		    "upload firmware!\n", sc->sc_name);
		return;
	}
	cd = usb2_get_config_descriptor(sc->sc_udev);

	/* reset device */
	err = usb2_req_set_config(sc->sc_udev, &sc->sc_mtx,
	    cd->bConfigurationValue);
	if (err) {
		DPRINTF("reset failed (ignored)\n");
	}
	/* Read MAC and other stuff rom EEPROM */
	zyd_cfg_read_eeprom(sc);

	/* Init hardware */
	if (zyd_cfg_hw_init(sc)) {
		DPRINTFN(0, "%s: HW init failed!\n", sc->sc_name);
		return;
	}
	/* Now init the RF chip */
	if (zyd_cfg_rf_init_hw(sc, &sc->sc_rf)) {
		DPRINTFN(0, "%s: RF init failed!\n", sc->sc_name);
		return;
	}
	printf("%s: HMAC ZD1211%s, FW %02x.%02x, RF %s, PA %x, address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    sc->sc_name, (sc->sc_mac_rev == ZYD_ZD1211) ? "" : "B",
	    sc->sc_fw_rev >> 8, sc->sc_fw_rev & 0xff, zyd_rf_name(sc->sc_rf_rev),
	    sc->sc_pa_rev, sc->sc_myaddr[0],
	    sc->sc_myaddr[1], sc->sc_myaddr[2],
	    sc->sc_myaddr[3], sc->sc_myaddr[4],
	    sc->sc_myaddr[5]);

	mtx_unlock(&sc->sc_mtx);

	ifp = if_alloc(IFT_IEEE80211);

	mtx_lock(&sc->sc_mtx);

	if (ifp == NULL) {
		DPRINTFN(0, "%s: could not if_alloc()!\n",
		    sc->sc_name);
		goto done;
	}
	sc->sc_evilhack = ifp;
	sc->sc_ifp = ifp;
	ic = ifp->if_l2com;

	ifp->if_softc = sc;
	if_initname(ifp, "zyd", sc->sc_unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = &zyd_init_cb;
	ifp->if_ioctl = &zyd_ioctl_cb;
	ifp->if_start = &zyd_start_cb;
	ifp->if_watchdog = NULL;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	bcopy(sc->sc_myaddr, ic->ic_myaddr, sizeof(ic->ic_myaddr));

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;

	/* Set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_STA		/* station mode supported */
	    | IEEE80211_C_MONITOR	/* monitor mode */
	    | IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	    | IEEE80211_C_SHSLOT	/* short slot time supported */
	    | IEEE80211_C_BGSCAN	/* capable of bg scanning */
	    | IEEE80211_C_WPA		/* 802.11i */
	    ;

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, NULL, &bands);

	mtx_unlock(&sc->sc_mtx);

	ieee80211_ifattach(ic);

	mtx_lock(&sc->sc_mtx);

	ic->ic_node_alloc = &zyd_node_alloc_cb;
	ic->ic_raw_xmit = &zyd_raw_xmit_cb;
	ic->ic_newassoc = &zyd_newassoc_cb;

	ic->ic_scan_start = &zyd_scan_start_cb;
	ic->ic_scan_end = &zyd_scan_end_cb;
	ic->ic_set_channel = &zyd_set_channel_cb;
	ic->ic_vap_create = &zyd_vap_create;
	ic->ic_vap_delete = &zyd_vap_delete;
	ic->ic_update_mcast = &zyd_update_mcast_cb;
	ic->ic_update_promisc = &zyd_update_promisc_cb;

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);

	mtx_unlock(&sc->sc_mtx);

	bpfattach(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) +
	    sizeof(sc->sc_txtap));

	mtx_lock(&sc->sc_mtx);

	if (bootverbose) {
		ieee80211_announce(ic);
	}
	usb2_transfer_start(sc->sc_xfer[ZYD_TR_INTR_DT_RD]);
done:
	return;
}

/*
 * Detach device
 */
static int
zyd_detach(device_t dev)
{
	struct zyd_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic;
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	zyd_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;
	ic = ifp->if_l2com;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, ZYD_N_TRANSFER);

	/* get rid of any late children */
	bus_generic_detach(dev);

	if (ifp) {
		bpfdetach(ifp);
		ieee80211_ifdetach(ic);
		if_free(ifp);
	}
	usb2_config_td_unsetup(&sc->sc_config_td);

	usb2_callout_drain(&sc->sc_watchdog);

	usb2_cv_destroy(&sc->sc_intr_cv);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
zyd_cfg_newstate(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct zyd_vap *uvp = ZYD_VAP(vap);
	enum ieee80211_state ostate;
	enum ieee80211_state nstate;
	int arg;

	ostate = vap->iv_state;
	nstate = sc->sc_ns_state;
	arg = sc->sc_ns_arg;

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_RUN:
		zyd_cfg_set_run(sc, cc);
		break;

	default:
		break;
	}

	mtx_unlock(&sc->sc_mtx);
	IEEE80211_LOCK(ic);
	uvp->newstate(vap, nstate, arg);
	if (vap->iv_newstate_cb != NULL)
		vap->iv_newstate_cb(vap, nstate, arg);
	IEEE80211_UNLOCK(ic);
	mtx_lock(&sc->sc_mtx);
	return;
}

static void
zyd_cfg_set_run(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc)
{
	zyd_cfg_set_chan(sc, cc, 0);

	if (cc->ic_opmode != IEEE80211_M_MONITOR) {
		/* turn link LED on */
		zyd_cfg_set_led(sc, ZYD_LED1, 1);

		/* make data LED blink upon Tx */
		zyd_cfg_write32(sc, sc->sc_firmware_base + ZYD_FW_LINK_STATUS, 1);

		zyd_cfg_set_bssid(sc, cc->iv_bss.ni_bssid);
	}
	if (cc->iv_bss.fixed_rate_none) {
		/* enable automatic rate adaptation */
		zyd_cfg_amrr_start(sc);
	}
	return;
}

static int
zyd_newstate_cb(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct zyd_vap *uvp = ZYD_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF("setting new state: %d\n", nstate);

	mtx_lock(&sc->sc_mtx);
	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		mtx_unlock(&sc->sc_mtx);
		/* Special case which happens at detach. */
		if (nstate == IEEE80211_S_INIT) {
			(uvp->newstate) (vap, nstate, arg);
		}
		return (0);		/* nothing to do */
	}
	/* store next state */
	sc->sc_ns_state = nstate;
	sc->sc_ns_arg = arg;

	/* stop timers */
	sc->sc_amrr_timer = 0;

	/*
	 * USB configuration can only be done from the USB configuration
	 * thread:
	 */
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &zyd_config_copy,
	    &zyd_cfg_newstate, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return EINPROGRESS;
}

static void
zyd_cfg_update_promisc(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t low;
	uint32_t high;

	if ((cc->ic_opmode == IEEE80211_M_MONITOR) ||
	    (cc->if_flags & (IFF_ALLMULTI | IFF_PROMISC))) {
		low = 0xffffffff;
		high = 0xffffffff;
	} else {
		low = cc->zyd_multi_low;
		high = cc->zyd_multi_high;
	}

	/* reprogram multicast global hash table */
	zyd_cfg_write32(sc, ZYD_MAC_GHTBL, low);
	zyd_cfg_write32(sc, ZYD_MAC_GHTBH, high);
	return;
}

/*
 * Rate-to-bit-converter (Field "rate" in zyd_controlsetformat)
 */
static uint8_t
zyd_plcp_signal(uint8_t rate)
{
	;				/* fix for indent */

	switch (rate) {
		/* CCK rates (NB: not IEEE std, device-specific) */
	case 2:
		return (0x0);
	case 4:
		return (0x1);
	case 11:
		return (0x2);
	case 22:
		return (0x3);

		/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:
		return (0xb);
	case 18:
		return (0xf);
	case 24:
		return (0xa);
	case 36:
		return (0xe);
	case 48:
		return (0x9);
	case 72:
		return (0xd);
	case 96:
		return (0x8);
	case 108:
		return (0xc);

		/* XXX unsupported/unknown rate */
	default:
		return (0xff);
	}
}

static void
zyd_std_command(struct ieee80211com *ic, usb2_config_td_command_t *func)
{
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &zyd_config_copy, func, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
zyd_scan_start_cb(struct ieee80211com *ic)
{
	zyd_std_command(ic, &zyd_cfg_scan_start);
	return;
}

static void
zyd_scan_end_cb(struct ieee80211com *ic)
{
	zyd_std_command(ic, &zyd_cfg_scan_end);
	return;
}

static void
zyd_set_channel_cb(struct ieee80211com *ic)
{
	zyd_std_command(ic, &zyd_cfg_set_chan);
	return;
}

/*========================================================================*
 * configure sub-routines, zyd_cfg_xxx
 *========================================================================*/

static void
zyd_cfg_scan_start(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	zyd_cfg_set_bssid(sc, cc->if_broadcastaddr);
	return;
}

static void
zyd_cfg_scan_end(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	zyd_cfg_set_bssid(sc, cc->iv_bss.ni_bssid);
	return;
}

static void
zyd_cfg_set_chan(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t chan;
	uint32_t tmp;

	chan = cc->ic_curchan.chan_to_ieee;

	DPRINTF("Will try %d\n", chan);

	if ((chan == 0) || (chan == IEEE80211_CHAN_ANY)) {
		DPRINTF("0 or ANY, exiting\n");
		return;
	}
	zyd_cfg_lock_phy(sc);

	(sc->sc_rf.cfg_set_channel) (sc, &sc->sc_rf, chan);

	/* update Tx power */
	zyd_cfg_write16(sc, ZYD_CR31, sc->sc_pwr_int[chan - 1]);

	if (sc->sc_mac_rev == ZYD_ZD1211B) {
		zyd_cfg_write16(sc, ZYD_CR67, sc->sc_ofdm36_cal[chan - 1]);
		zyd_cfg_write16(sc, ZYD_CR66, sc->sc_ofdm48_cal[chan - 1]);
		zyd_cfg_write16(sc, ZYD_CR65, sc->sc_ofdm54_cal[chan - 1]);

		zyd_cfg_write16(sc, ZYD_CR68, sc->sc_pwr_cal[chan - 1]);

		zyd_cfg_write16(sc, ZYD_CR69, 0x28);
		zyd_cfg_write16(sc, ZYD_CR69, 0x2a);
	}
	if (sc->sc_fix_cr47) {
		/* set CCK baseband gain from EEPROM */
		zyd_cfg_read32(sc, ZYD_EEPROM_PHY_REG, &tmp);
		zyd_cfg_write16(sc, ZYD_CR47, tmp & 0xff);
	}
	zyd_cfg_write32(sc, ZYD_CR_CONFIG_PHILIPS, 0);

	zyd_cfg_unlock_phy(sc);

	sc->sc_rxtap.wr_chan_freq =
	    sc->sc_txtap.wt_chan_freq =
	    htole16(cc->ic_curchan.ic_freq);

	sc->sc_rxtap.wr_chan_flags =
	    sc->sc_txtap.wt_chan_flags =
	    htole16(cc->ic_flags);

	return;
}

/*
 * Interface: init
 */

/* immediate configuration */

static void
zyd_cfg_pre_init(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	zyd_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= ZYD_FLAG_HL_READY;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));

	return;
}

/* delayed configuration */

static void
zyd_cfg_init(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	zyd_cfg_stop(sc, cc, 0);

	/* Do initial setup */

	zyd_cfg_set_mac_addr(sc, cc->ic_myaddr);

	zyd_cfg_write32(sc, ZYD_MAC_ENCRYPTION_TYPE, ZYD_ENC_SNIFFER);

	/* promiscuous mode */
	zyd_cfg_write32(sc, ZYD_MAC_SNIFFER,
	    (cc->ic_opmode == IEEE80211_M_MONITOR) ? 1 : 0);

	/* multicast setup */
	zyd_cfg_update_promisc(sc, cc, refcount);

	zyd_cfg_set_rxfilter(sc, cc, refcount);

	/* switch radio transmitter ON */
	zyd_cfg_switch_radio(sc, 1);

	/* XXX wrong, can't set here */
	/* set basic rates */
	if (cc->ic_curmode == IEEE80211_MODE_11B)
		zyd_cfg_write32(sc, ZYD_MAC_BAS_RATE, 0x0003);
	else if (cc->ic_curmode == IEEE80211_MODE_11A)
		zyd_cfg_write32(sc, ZYD_MAC_BAS_RATE, 0x1500);
	else				/* assumes 802.11b/g */
		zyd_cfg_write32(sc, ZYD_MAC_BAS_RATE, 0x000f);

	/* set mandatory rates */
	if (cc->ic_curmode == IEEE80211_MODE_11B)
		zyd_cfg_write32(sc, ZYD_MAC_MAN_RATE, 0x000f);
	else if (cc->ic_curmode == IEEE80211_MODE_11A)
		zyd_cfg_write32(sc, ZYD_MAC_MAN_RATE, 0x1500);
	else				/* assumes 802.11b/g */
		zyd_cfg_write32(sc, ZYD_MAC_MAN_RATE, 0x150f);

	/* set default BSS channel */
	zyd_cfg_set_chan(sc, cc, 0);

	/* enable interrupts */
	zyd_cfg_write32(sc, ZYD_CR_INTERRUPT, ZYD_HWINT_MASK);

	/* make sure that the transfers get started */
	sc->sc_flags |= (
	    ZYD_FLAG_BULK_READ_STALL |
	    ZYD_FLAG_BULK_WRITE_STALL |
	    ZYD_FLAG_LL_READY);

	if ((sc->sc_flags & ZYD_FLAG_LL_READY) &&
	    (sc->sc_flags & ZYD_FLAG_HL_READY)) {
		struct ifnet *ifp = sc->sc_ifp;
		struct ieee80211com *ic = ifp->if_l2com;

		/*
		 * start the USB transfers, if not already started:
		 */
		usb2_transfer_start(sc->sc_xfer[1]);
		usb2_transfer_start(sc->sc_xfer[0]);

		/*
		 * start IEEE802.11 layer
		 */
		mtx_unlock(&sc->sc_mtx);
		ieee80211_start_all(ic);
		mtx_lock(&sc->sc_mtx);
	}
	return;
}

/* immediate configuration */

static void
zyd_cfg_pre_stop(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		zyd_config_copy(sc, cc, refcount);
	}
	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(ZYD_FLAG_HL_READY |
	    ZYD_FLAG_LL_READY);

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[ZYD_TR_BULK_DT_WR]);
	usb2_transfer_stop(sc->sc_xfer[ZYD_TR_BULK_DT_RD]);
	usb2_transfer_stop(sc->sc_xfer[ZYD_TR_BULK_CS_WR]);
	usb2_transfer_stop(sc->sc_xfer[ZYD_TR_BULK_CS_RD]);

	/* clean up transmission */
	zyd_tx_clean_queue(sc);
	return;
}

/* delayed configuration */

static void
zyd_cfg_stop(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* switch radio transmitter OFF */
	zyd_cfg_switch_radio(sc, 0);

	/* disable Rx */
	zyd_cfg_write32(sc, ZYD_MAC_RXFILTER, 0);

	/* disable interrupts */
	zyd_cfg_write32(sc, ZYD_CR_INTERRUPT, 0);

	return;
}

static void
zyd_update_mcast_cb(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &zyd_config_copy,
	    &zyd_cfg_update_promisc, 0, 0);
	mtx_unlock(&sc->sc_mtx);
	return;
}

static void
zyd_update_promisc_cb(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &zyd_config_copy,
	    &zyd_cfg_update_promisc, 0, 0);
	mtx_unlock(&sc->sc_mtx);
	return;
}

static void
zyd_cfg_set_rxfilter(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t rxfilter;

	switch (cc->ic_opmode) {
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
		return;
	}
	zyd_cfg_write32(sc, ZYD_MAC_RXFILTER, rxfilter);
	return;
}

static void
zyd_cfg_set_led(struct zyd_softc *sc, uint32_t which, uint8_t on)
{
	uint32_t tmp;

	zyd_cfg_read32(sc, ZYD_MAC_TX_PE_CONTROL, &tmp);
	if (on)
		tmp |= which;
	else
		tmp &= ~which;

	zyd_cfg_write32(sc, ZYD_MAC_TX_PE_CONTROL, tmp);
	return;
}

static void
zyd_start_cb(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_transfer_start(sc->sc_xfer[ZYD_TR_BULK_DT_WR]);
	mtx_unlock(&sc->sc_mtx);
	return;
}

static void
zyd_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[ZYD_TR_BULK_DT_WR];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~ZYD_FLAG_BULK_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

/*
 * We assume that "m->m_pkthdr.rcvif" is pointing to the "ni" that
 * should be freed, when "zyd_setup_desc_and_tx" is called.
 */
static void
zyd_setup_desc_and_tx(struct zyd_softc *sc, struct mbuf *m,
    uint16_t rate)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct mbuf *mm;
	enum ieee80211_phytype phytype;
	uint16_t len;
	uint16_t totlen;
	uint16_t pktlen;
	uint8_t remainder;

	if (sc->sc_tx_queue.ifq_len >= IFQ_MAXLEN) {
		/* free packet */
		zyd_tx_freem(m);
		ifp->if_oerrors++;
		return;
	}
	if (!((sc->sc_flags & ZYD_FLAG_LL_READY) &&
	    (sc->sc_flags & ZYD_FLAG_HL_READY))) {
		/* free packet */
		zyd_tx_freem(m);
		ifp->if_oerrors++;
		return;
	}
	if (rate < 2) {
		DPRINTF("rate < 2!\n");

		/* avoid division by zero */
		rate = 2;
	}
	ic->ic_lastdata = ticks;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct zyd_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m);
	}
	len = m->m_pkthdr.len;
	totlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;
	phytype = ieee80211_rate2phytype(sc->sc_rates, rate);

	sc->sc_tx_desc.len = htole16(totlen);
	sc->sc_tx_desc.phy = zyd_plcp_signal(rate);
	if (phytype == IEEE80211_T_OFDM) {
		sc->sc_tx_desc.phy |= ZYD_TX_PHY_OFDM;
		if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
			sc->sc_tx_desc.phy |= ZYD_TX_PHY_5GHZ;
	} else if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		sc->sc_tx_desc.phy |= ZYD_TX_PHY_SHPREAMBLE;

	/* actual transmit length (XXX why +10?) */
	pktlen = sizeof(struct zyd_tx_desc) + 10;
	if (sc->sc_mac_rev == ZYD_ZD1211)
		pktlen += totlen;
	sc->sc_tx_desc.pktlen = htole16(pktlen);

	sc->sc_tx_desc.plcp_length = ((16 * totlen) + rate - 1) / rate;
	sc->sc_tx_desc.plcp_service = 0;
	if (rate == 22) {
		remainder = (16 * totlen) % 22;
		if ((remainder != 0) && (remainder < 7))
			sc->sc_tx_desc.plcp_service |= ZYD_PLCP_LENGEXT;
	}
	if (sizeof(sc->sc_tx_desc) > MHLEN) {
		DPRINTF("No room for header structure!\n");
		zyd_tx_freem(m);
		return;
	}
	mm = m_gethdr(M_NOWAIT, MT_DATA);
	if (mm == NULL) {
		DPRINTF("Could not allocate header mbuf!\n");
		zyd_tx_freem(m);
		return;
	}
	bcopy(&sc->sc_tx_desc, mm->m_data, sizeof(sc->sc_tx_desc));
	mm->m_len = sizeof(sc->sc_tx_desc);

	mm->m_next = m;
	mm->m_pkthdr.len = mm->m_len + m->m_pkthdr.len;
	mm->m_pkthdr.rcvif = NULL;

	/* start write transfer, if not started */
	_IF_ENQUEUE(&sc->sc_tx_queue, mm);

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
zyd_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct zyd_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint16_t temp_len;

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");

		ifp->if_opackets++;

	case USB_ST_SETUP:
		if (sc->sc_flags & ZYD_FLAG_BULK_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_BULK_CS_WR]);
			DPRINTFN(11, "write stalled\n");
			break;
		}
		if (sc->sc_flags & ZYD_FLAG_WAIT_COMMAND) {
			/*
			 * don't send anything while a command is pending !
			 */
			DPRINTFN(11, "wait command\n");
			break;
		}
		zyd_fill_write_queue(sc);

		_IF_DEQUEUE(&sc->sc_tx_queue, m);

		if (m) {
			if (m->m_pkthdr.len > ZYD_MAX_TXBUFSZ) {
				DPRINTFN(0, "data overflow, %u bytes\n",
				    m->m_pkthdr.len);
				m->m_pkthdr.len = ZYD_MAX_TXBUFSZ;
			}
			usb2_m_copy_in(xfer->frbuffers, 0,
			    m, 0, m->m_pkthdr.len);

			/* get transfer length */
			temp_len = m->m_pkthdr.len;

			DPRINTFN(11, "sending frame len=%u xferlen=%u\n",
			    m->m_pkthdr.len, temp_len);

			xfer->frlengths[0] = temp_len;

			usb2_start_hardware(xfer);

			/* free mbuf and node */
			zyd_tx_freem(m);
		}
		break;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= ZYD_FLAG_BULK_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[ZYD_TR_BULK_CS_WR]);
		}
		ifp->if_oerrors++;
		break;
	}
	return;
}

static void
zyd_init_cb(void *arg)
{
	struct zyd_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &zyd_cfg_pre_init,
	    &zyd_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static int
zyd_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	int error;

	switch (cmd) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &zyd_cfg_pre_init,
				    &zyd_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &zyd_cfg_pre_stop,
				    &zyd_cfg_stop, 0, 0);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		error = 0;
		break;

	case SIOCGIFMEDIA:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = ifmedia_ioctl(ifp, (void *)data, &ic->ic_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
zyd_watchdog(void *arg)
{
	struct zyd_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->sc_amrr_timer) {
		usb2_config_td_queue_command
		    (&sc->sc_config_td, NULL,
		    &zyd_cfg_amrr_timeout, 0, 0);
	}
	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &zyd_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
zyd_config_copy_chan(struct zyd_config_copy_chan *cc,
    struct ieee80211com *ic, struct ieee80211_channel *c)
{
	if (!c)
		return;
	cc->chan_to_ieee =
	    ieee80211_chan2ieee(ic, c);
	if (c != IEEE80211_CHAN_ANYC) {
		cc->chan_to_mode =
		    ieee80211_chan2mode(c);
		cc->ic_freq = c->ic_freq;
		if (IEEE80211_IS_CHAN_B(c))
			cc->chan_is_b = 1;
		if (IEEE80211_IS_CHAN_A(c))
			cc->chan_is_a = 1;
		if (IEEE80211_IS_CHAN_2GHZ(c))
			cc->chan_is_2ghz = 1;
		if (IEEE80211_IS_CHAN_5GHZ(c))
			cc->chan_is_5ghz = 1;
		if (IEEE80211_IS_CHAN_ANYG(c))
			cc->chan_is_g = 1;
	}
	return;
}

static void
zyd_config_copy(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	const struct ieee80211_txparam *tp;
	struct ieee80211vap *vap;
	struct ifmultiaddr *ifma;
	struct ieee80211_node *ni;
	struct ieee80211com *ic;
	struct ifnet *ifp;

	bzero(cc, sizeof(*cc));

	ifp = sc->sc_ifp;
	if (ifp) {
		cc->if_flags = ifp->if_flags;
		bcopy(ifp->if_broadcastaddr, cc->if_broadcastaddr,
		    sizeof(cc->if_broadcastaddr));

		cc->zyd_multi_low = 0x00000000;
		cc->zyd_multi_high = 0x80000000;

		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			uint8_t v;

			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			v = ((uint8_t *)LLADDR((struct sockaddr_dl *)
			    ifma->ifma_addr))[5] >> 2;
			if (v < 32)
				cc->zyd_multi_low |= 1 << v;
			else
				cc->zyd_multi_high |= 1 << (v - 32);
		}
		IF_ADDR_UNLOCK(ifp);

		ic = ifp->if_l2com;
		if (ic) {
			zyd_config_copy_chan(&cc->ic_curchan, ic, ic->ic_curchan);
			zyd_config_copy_chan(&cc->ic_bsschan, ic, ic->ic_bsschan);
			vap = TAILQ_FIRST(&ic->ic_vaps);
			if (vap) {
				ni = vap->iv_bss;
				if (ni) {
					cc->iv_bss.ni_intval = ni->ni_intval;
					bcopy(ni->ni_bssid, cc->iv_bss.ni_bssid,
					    sizeof(cc->iv_bss.ni_bssid));
				}
				tp = vap->iv_txparms + cc->ic_bsschan.chan_to_mode;
				if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE) {
					cc->iv_bss.fixed_rate_none = 1;
				}
			}
			cc->ic_opmode = ic->ic_opmode;
			cc->ic_flags = ic->ic_flags;
			cc->ic_txpowlimit = ic->ic_txpowlimit;
			cc->ic_curmode = ic->ic_curmode;

			bcopy(ic->ic_myaddr, cc->ic_myaddr,
			    sizeof(cc->ic_myaddr));
		}
	}
	sc->sc_flags |= ZYD_FLAG_WAIT_COMMAND;
	return;
}

static void
zyd_end_of_commands(struct zyd_softc *sc)
{
	sc->sc_flags &= ~ZYD_FLAG_WAIT_COMMAND;

	/* start write transfer, if not started */
	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
zyd_newassoc_cb(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ieee80211_amrr_node_init(&ZYD_VAP(vap)->amrr, &ZYD_NODE(ni)->amn, ni);
	return;
}

static void
zyd_cfg_amrr_timeout(struct zyd_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;

	vap = zyd_get_vap(sc);
	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	if ((sc->sc_flags & ZYD_FLAG_LL_READY) &&
	    (sc->sc_flags & ZYD_FLAG_HL_READY)) {

		if (sc->sc_amrr_timer) {

			if (ieee80211_amrr_choose(ni, &ZYD_NODE(ni)->amn)) {
				/* ignore */
			}
		}
	}
	return;
}

static void
zyd_cfg_amrr_start(struct zyd_softc *sc)
{
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;

	vap = zyd_get_vap(sc);

	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	/* init AMRR */

	ieee80211_amrr_node_init(&ZYD_VAP(vap)->amrr, &ZYD_NODE(ni)->amn, ni);

	/* enable AMRR timer */

	sc->sc_amrr_timer = 1;
	return;
}

static struct ieee80211vap *
zyd_vap_create(struct ieee80211com *ic,
    const char name[IFNAMSIZ], int unit, int opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct zyd_vap *zvp;
	struct ieee80211vap *vap;
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	/* Need to sync with config thread: */
	mtx_lock(&sc->sc_mtx);
	if (usb2_config_td_sync(&sc->sc_config_td)) {
		mtx_unlock(&sc->sc_mtx);
		/* config thread is gone */
		return (NULL);
	}
	mtx_unlock(&sc->sc_mtx);

	if (!TAILQ_EMPTY(&ic->ic_vaps))	/* only one at a time */
		return NULL;
	zvp = (struct zyd_vap *)malloc(sizeof(struct zyd_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (zvp == NULL)
		return NULL;
	vap = &zvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	zvp->newstate = vap->iv_newstate;
	vap->iv_newstate = &zyd_newstate_cb;

	ieee80211_amrr_init(&zvp->amrr, vap,
	    IEEE80211_AMRR_MIN_SUCCESS_THRESHOLD,
	    IEEE80211_AMRR_MAX_SUCCESS_THRESHOLD,
	    1000 /* 1 sec */ );

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change, ieee80211_media_status);
	ic->ic_opmode = opmode;

	return (vap);
}

static void
zyd_vap_delete(struct ieee80211vap *vap)
{
	struct zyd_vap *zvp = ZYD_VAP(vap);
	struct zyd_softc *sc = vap->iv_ic->ic_ifp->if_softc;

	/* Need to sync with config thread: */
	mtx_lock(&sc->sc_mtx);
	if (usb2_config_td_sync(&sc->sc_config_td)) {
		/* ignore */
	}
	mtx_unlock(&sc->sc_mtx);

	ieee80211_amrr_cleanup(&zvp->amrr);
	ieee80211_vap_detach(vap);
	free(zvp, M_80211_VAP);
	return;
}

/* ARGUSED */
static struct ieee80211_node *
zyd_node_alloc_cb(struct ieee80211vap *vap __unused,
    const uint8_t mac[IEEE80211_ADDR_LEN] __unused)
{
	struct zyd_node *zn;

	zn = malloc(sizeof(struct zyd_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	return ((zn != NULL) ? &zn->ni : NULL);
}

static void
zyd_fill_write_queue(struct zyd_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211_node *ni;
	struct mbuf *m;

	/*
	 * We only fill up half of the queue with data frames. The rest is
	 * reserved for other kinds of frames.
	 */

	while (sc->sc_tx_queue.ifq_len < (IFQ_MAXLEN / 2)) {

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		ni = (void *)(m->m_pkthdr.rcvif);
		m = ieee80211_encap(ni, m);
		if (m == NULL) {
			ieee80211_free_node(ni);
			continue;
		}
		zyd_tx_data(sc, m, ni);
	}
	return;
}

static void
zyd_tx_clean_queue(struct zyd_softc *sc)
{
	struct mbuf *m;

	for (;;) {
		_IF_DEQUEUE(&sc->sc_tx_queue, m);

		if (!m) {
			break;
		}
		zyd_tx_freem(m);
	}

	return;
}

static void
zyd_tx_freem(struct mbuf *m)
{
	struct ieee80211_node *ni;

	while (m) {
		ni = (void *)(m->m_pkthdr.rcvif);
		if (!ni) {
			m = m_free(m);
			continue;
		}
		if (m->m_flags & M_TXCB) {
			ieee80211_process_callback(ni, m, 0);
		}
		m_freem(m);
		ieee80211_free_node(ni);

		break;
	}
	return;
}

static void
zyd_tx_mgt(struct zyd_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_txparam *tp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	uint16_t totlen;
	uint16_t rate;

	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
	rate = tp->mgmtrate;

	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			m_freem(m);
			ieee80211_free_node(ni);
			return;
		}
		wh = mtod(m, struct ieee80211_frame *);
	}
	/* fill Tx descriptor */

	sc->sc_tx_desc.flags = ZYD_TX_FLAG_BACKOFF;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* get total length */
		totlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (totlen > vap->iv_rtsthreshold) {
			sc->sc_tx_desc.flags |= ZYD_TX_FLAG_RTS;
		} else if (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				sc->sc_tx_desc.flags |= ZYD_TX_FLAG_CTS_TO_SELF;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				sc->sc_tx_desc.flags |= ZYD_TX_FLAG_RTS;
		}
	} else
		sc->sc_tx_desc.flags |= ZYD_TX_FLAG_MULTICAST;

	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		sc->sc_tx_desc.flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	m->m_pkthdr.rcvif = (void *)ni;
	zyd_setup_desc_and_tx(sc, m, rate);
	return;
}

static void
zyd_tx_data(struct zyd_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_txparam *tp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	uint16_t rate;

	wh = mtod(m, struct ieee80211_frame *);

	sc->sc_tx_desc.flags = ZYD_TX_FLAG_BACKOFF;
	tp = &vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)];
	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		rate = tp->mcastrate;
		sc->sc_tx_desc.flags |= ZYD_TX_FLAG_MULTICAST;
	} else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
		rate = tp->ucastrate;
	} else
		rate = ni->ni_txrate;

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			m_freem(m);
			ieee80211_free_node(ni);
			return;
		}
		/* packet header may have moved, reset our local pointer */
		wh = mtod(m, struct ieee80211_frame *);
	}
	/* fill Tx descriptor */

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		uint16_t totlen;

		totlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;

		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (totlen > vap->iv_rtsthreshold) {
			sc->sc_tx_desc.flags |= ZYD_TX_FLAG_RTS;
		} else if (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				sc->sc_tx_desc.flags |= ZYD_TX_FLAG_CTS_TO_SELF;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				sc->sc_tx_desc.flags |= ZYD_TX_FLAG_RTS;
		}
	}
	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		sc->sc_tx_desc.flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	m->m_pkthdr.rcvif = (void *)ni;
	zyd_setup_desc_and_tx(sc, m, rate);
	return;
}

static int
zyd_raw_xmit_cb(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct zyd_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		zyd_tx_mgt(sc, m, ni);
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		zyd_tx_mgt(sc, m, ni);	/* XXX zyd_tx_raw() */
	}
	mtx_unlock(&sc->sc_mtx);
	return (0);
}

static struct ieee80211vap *
zyd_get_vap(struct zyd_softc *sc)
{
	struct ifnet *ifp;
	struct ieee80211com *ic;

	if (sc == NULL) {
		return NULL;
	}
	ifp = sc->sc_ifp;
	if (ifp == NULL) {
		return NULL;
	}
	ic = ifp->if_l2com;
	if (ic == NULL) {
		return NULL;
	}
	return TAILQ_FIRST(&ic->ic_vaps);
}
