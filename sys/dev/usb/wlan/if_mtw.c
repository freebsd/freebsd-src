/*-
 * Copyright (c) 2008-2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2013-2014 Kevin Lo
 * Copyright (c) 2021 James Hastings
 * Ported to FreeBSD by Jesper Schmitz Mouridsen jsm@FreeBSD.org
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
 * MediaTek MT7601U 802.11b/g/n WLAN.
 */

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/firmware.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef	IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include "usbdevs.h"

#define USB_DEBUG_VAR mtw_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_msctest.h>

#include "if_mtwreg.h"
#include "if_mtwvar.h"

#define MTW_DEBUG

#ifdef MTW_DEBUG
int mtw_debug;
static SYSCTL_NODE(_hw_usb, OID_AUTO, mtw, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB mtw");
SYSCTL_INT(_hw_usb_mtw, OID_AUTO, debug, CTLFLAG_RWTUN, &mtw_debug, 0,
    "mtw debug level");

enum {
	MTW_DEBUG_XMIT = 0x00000001,	  /* basic xmit operation */
	MTW_DEBUG_XMIT_DESC = 0x00000002, /* xmit descriptors */
	MTW_DEBUG_RECV = 0x00000004,	  /* basic recv operation */
	MTW_DEBUG_RECV_DESC = 0x00000008, /* recv descriptors */
	MTW_DEBUG_STATE = 0x00000010,	  /* 802.11 state transitions */
	MTW_DEBUG_RATE = 0x00000020,	  /* rate adaptation */
	MTW_DEBUG_USB = 0x00000040,	  /* usb requests */
	MTW_DEBUG_FIRMWARE = 0x00000080,  /* firmware(9) loading debug */
	MTW_DEBUG_BEACON = 0x00000100,	  /* beacon handling */
	MTW_DEBUG_INTR = 0x00000200,	  /* ISR */
	MTW_DEBUG_TEMP = 0x00000400,	  /* temperature calibration */
	MTW_DEBUG_ROM = 0x00000800,	  /* various ROM info */
	MTW_DEBUG_KEY = 0x00001000,	  /* crypto keys management */
	MTW_DEBUG_TXPWR = 0x00002000,	  /* dump Tx power values */
	MTW_DEBUG_RSSI = 0x00004000,	  /* dump RSSI lookups */
	MTW_DEBUG_RESET = 0x00008000,	  /* initialization progress */
	MTW_DEBUG_CALIB = 0x00010000,	  /* calibration progress */
	MTW_DEBUG_CMD = 0x00020000,	  /* command queue */
	MTW_DEBUG_ANY = 0xffffffff
};

#define MTW_DPRINTF(_sc, _m, ...)                                  \
	do {                                                       \
		if (mtw_debug & (_m))                              \
			device_printf((_sc)->sc_dev, __VA_ARGS__); \
	} while (0)

#else
#define MTW_DPRINTF(_sc, _m, ...) \
	do {                      \
		(void)_sc;        \
	} while (0)
#endif

#define IEEE80211_HAS_ADDR4(wh) IEEE80211_IS_DSTODS(wh)

/* NB: "11" is the maximum number of padding bytes needed for Tx */
#define MTW_MAX_TXSZ \
	(sizeof(struct mtw_txd) + sizeof(struct mtw_txwi) + MCLBYTES + 11)

/*
 * Because of LOR in mtw_key_delete(), use atomic instead.
 * '& MTW_CMDQ_MASQ' is to loop cmdq[].
 */
#define MTW_CMDQ_GET(c) (atomic_fetchadd_32((c), 1) & MTW_CMDQ_MASQ)

static const STRUCT_USB_HOST_ID mtw_devs[] = {
#define MTW_DEV(v, p)                                         \
	{                                                     \
		USB_VP(USB_VENDOR_##v, USB_PRODUCT_##v##_##p) \
	}
	MTW_DEV(EDIMAX, MT7601U),
	MTW_DEV(RALINK, MT7601U),
	MTW_DEV(XIAOMI, MT7601U)
};
#undef MTW_DEV

static device_probe_t mtw_match;
static device_attach_t mtw_attach;
static device_detach_t mtw_detach;

static usb_callback_t mtw_bulk_rx_callback;
static usb_callback_t mtw_bulk_tx_callback0;
static usb_callback_t mtw_bulk_tx_callback1;
static usb_callback_t mtw_bulk_tx_callback2;
static usb_callback_t mtw_bulk_tx_callback3;
static usb_callback_t mtw_bulk_tx_callback4;
static usb_callback_t mtw_bulk_tx_callback5;
static usb_callback_t mtw_fw_callback;

static void mtw_autoinst(void *, struct usb_device *, struct usb_attach_arg *);
static int mtw_driver_loaded(struct module *, int, void *);
static void mtw_bulk_tx_callbackN(struct usb_xfer *xfer, usb_error_t error,
				  u_int index);
static struct ieee80211vap *mtw_vap_create(struct ieee80211com *,
	const char[IFNAMSIZ], int, enum ieee80211_opmode, int,
	const uint8_t[IEEE80211_ADDR_LEN], const uint8_t[IEEE80211_ADDR_LEN]);
static void mtw_vap_delete(struct ieee80211vap *);
static void mtw_cmdq_cb(void *, int);
static void mtw_setup_tx_list(struct mtw_softc *, struct mtw_endpoint_queue *);
static void mtw_unsetup_tx_list(struct mtw_softc *,
				struct mtw_endpoint_queue *);
static void mtw_load_microcode(void *arg);

static usb_error_t mtw_do_request(struct mtw_softc *,
				  struct usb_device_request *, void *);
static int mtw_read(struct mtw_softc *, uint16_t, uint32_t *);
static int mtw_read_region_1(struct mtw_softc *, uint16_t, uint8_t *, int);
static int mtw_write_2(struct mtw_softc *, uint16_t, uint16_t);
static int mtw_write(struct mtw_softc *, uint16_t, uint32_t);
static int mtw_write_region_1(struct mtw_softc *, uint16_t, uint8_t *, int);
static int mtw_set_region_4(struct mtw_softc *, uint16_t, uint32_t, int);
static int mtw_efuse_read_2(struct mtw_softc *, uint16_t, uint16_t *);
static int mtw_bbp_read(struct mtw_softc *, uint8_t, uint8_t *);
static int mtw_bbp_write(struct mtw_softc *, uint8_t, uint8_t);
static int mtw_mcu_cmd(struct mtw_softc *sc, uint8_t cmd, void *buf, int len);
static void mtw_get_txpower(struct mtw_softc *);
static int mtw_read_eeprom(struct mtw_softc *);
static struct ieee80211_node *mtw_node_alloc(struct ieee80211vap *,
    const uint8_t mac[IEEE80211_ADDR_LEN]);
static int mtw_media_change(if_t);
static int mtw_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static int mtw_wme_update(struct ieee80211com *);
static void mtw_key_set_cb(void *);
static int mtw_key_set(struct ieee80211vap *, struct ieee80211_key *);
static void mtw_key_delete_cb(void *);
static int mtw_key_delete(struct ieee80211vap *, struct ieee80211_key *);
static void mtw_ratectl_to(void *);
static void mtw_ratectl_cb(void *, int);
static void mtw_drain_fifo(void *);
static void mtw_iter_func(void *, struct ieee80211_node *);
static void mtw_newassoc_cb(void *);
static void mtw_newassoc(struct ieee80211_node *, int);
static int mtw_mcu_radio(struct mtw_softc *sc, int func, uint32_t val);
static void mtw_recv_mgmt(struct ieee80211_node *, struct mbuf *, int,
    const struct ieee80211_rx_stats *, int, int);
static void mtw_rx_frame(struct mtw_softc *, struct mbuf *, uint32_t);
static void mtw_tx_free(struct mtw_endpoint_queue *pq, struct mtw_tx_data *,
    int);
static void mtw_set_tx_desc(struct mtw_softc *, struct mtw_tx_data *);
static int mtw_tx(struct mtw_softc *, struct mbuf *, struct ieee80211_node *);
static int mtw_tx_mgt(struct mtw_softc *, struct mbuf *,
    struct ieee80211_node *);
static int mtw_sendprot(struct mtw_softc *, const struct mbuf *,
    struct ieee80211_node *, int, int);
static int mtw_tx_param(struct mtw_softc *, struct mbuf *,
    struct ieee80211_node *, const struct ieee80211_bpf_params *);
static int mtw_raw_xmit(struct ieee80211_node *, struct mbuf *,
    const struct ieee80211_bpf_params *);
static int mtw_transmit(struct ieee80211com *, struct mbuf *);
static void mtw_start(struct mtw_softc *);
static void mtw_parent(struct ieee80211com *);
static void mtw_select_chan_group(struct mtw_softc *, int);

static int mtw_set_chan(struct mtw_softc *, struct ieee80211_channel *);
static void mtw_set_channel(struct ieee80211com *);
static void mtw_getradiocaps(struct ieee80211com *, int, int *,
    struct ieee80211_channel[]);
static void mtw_scan_start(struct ieee80211com *);
static void mtw_scan_end(struct ieee80211com *);
static void mtw_update_beacon(struct ieee80211vap *, int);
static void mtw_update_beacon_cb(void *);
static void mtw_updateprot(struct ieee80211com *);
static void mtw_updateprot_cb(void *);
static void mtw_usb_timeout_cb(void *);
static int mtw_reset(struct mtw_softc *sc);
static void mtw_enable_tsf_sync(struct mtw_softc *);


static void mtw_enable_mrr(struct mtw_softc *);
static void mtw_set_txpreamble(struct mtw_softc *);
static void mtw_set_basicrates(struct mtw_softc *);
static void mtw_set_leds(struct mtw_softc *, uint16_t);
static void mtw_set_bssid(struct mtw_softc *, const uint8_t *);
static void mtw_set_macaddr(struct mtw_softc *, const uint8_t *);
static void mtw_updateslot(struct ieee80211com *);
static void mtw_updateslot_cb(void *);
static void mtw_update_mcast(struct ieee80211com *);
static int8_t mtw_rssi2dbm(struct mtw_softc *, uint8_t, uint8_t);
static void mtw_update_promisc_locked(struct mtw_softc *);
static void mtw_update_promisc(struct ieee80211com *);
static int mtw_txrx_enable(struct mtw_softc *);
static void mtw_init_locked(struct mtw_softc *);
static void mtw_stop(void *);
static void mtw_delay(struct mtw_softc *, u_int);
static void mtw_update_chw(struct ieee80211com *ic);
static int mtw_ampdu_enable(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap);

static eventhandler_tag mtw_etag;

static const struct {
	uint8_t reg;
	uint8_t val;
} mt7601_rf_bank0[] = { MT7601_BANK0_RF },
  mt7601_rf_bank4[] = { MT7601_BANK4_RF },
  mt7601_rf_bank5[] = { MT7601_BANK5_RF };
static const struct {
	uint32_t reg;
	uint32_t val;
} mt7601_def_mac[] = { MT7601_DEF_MAC };
static const struct {
	uint8_t reg;
	uint8_t val;
} mt7601_def_bbp[] = { MT7601_DEF_BBP };


static const struct {
	u_int chan;
	uint8_t r17, r18, r19, r20;
} mt7601_rf_chan[] = { MT7601_RF_CHAN };


static const struct usb_config mtw_config[MTW_N_XFER] = {
	[MTW_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = MTW_MAX_RXSZ,
		.flags = {.pipe_bof = 1,
			  .short_xfer_ok = 1,},
		.callback = mtw_bulk_rx_callback,
	},
	[MTW_BULK_TX_BE] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MTW_MAX_TXSZ,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 0,},
		.callback = mtw_bulk_tx_callback0,
		.timeout = 5000,	/* ms */
    },
	[MTW_BULK_TX_BK] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MTW_MAX_TXSZ,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 1,},
		.callback = mtw_bulk_tx_callback1,
		.timeout = 5000,	/* ms */
	},
	[MTW_BULK_TX_VI] = {
	    .type = UE_BULK,
	    .endpoint = UE_ADDR_ANY,
	    .direction = UE_DIR_OUT,
	    .bufsize = MTW_MAX_TXSZ,
	    .flags = {.pipe_bof = 1,
		      .force_short_xfer = 1,},
	    .callback = mtw_bulk_tx_callback2,
	    .timeout = 5000,	/* ms */
	},
	[MTW_BULK_TX_VO] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MTW_MAX_TXSZ,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 1,},
		.callback = mtw_bulk_tx_callback3,
		.timeout = 5000,	/* ms */
    },
	[MTW_BULK_TX_HCCA] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MTW_MAX_TXSZ,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 1, .no_pipe_ok = 1,},
		.callback = mtw_bulk_tx_callback4,
		.timeout = 5000,	/* ms */
    },
	[MTW_BULK_TX_PRIO] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MTW_MAX_TXSZ,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 1, .no_pipe_ok = 1,},
		.callback = mtw_bulk_tx_callback5,
		.timeout = 5000,	/* ms */
	},

	[MTW_BULK_FW_CMD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = 0x2c44,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 1, .no_pipe_ok = 1,},
		.callback = mtw_fw_callback,

	},

	[MTW_BULK_RAW_TX] = {
		.type = UE_BULK,
		.ep_index = 0,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MTW_MAX_TXSZ,
		.flags = {.pipe_bof = 1,
			  .force_short_xfer = 1, .no_pipe_ok = 1,},
		.callback = mtw_bulk_tx_callback0,
		.timeout = 5000,	/* ms */
	},

};
static uint8_t mtw_wme_ac_xfer_map[4] = {
	[WME_AC_BE] = MTW_BULK_TX_BE,
	[WME_AC_BK] = MTW_BULK_TX_BK,
	[WME_AC_VI] = MTW_BULK_TX_VI,
	[WME_AC_VO] = MTW_BULK_TX_VO,
};
static void
mtw_autoinst(void *arg, struct usb_device *udev, struct usb_attach_arg *uaa)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;

	if (uaa->dev_state != UAA_DEV_READY)
		return;

	iface = usbd_get_iface(udev, 0);
	if (iface == NULL)
		return;
	id = iface->idesc;
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return;
	if (usbd_lookup_id_by_uaa(mtw_devs, sizeof(mtw_devs), uaa))
		return;

	if (usb_msc_eject(udev, 0, MSC_EJECT_STOPUNIT) == 0)
		uaa->dev_state = UAA_DEV_EJECTING;
}

static int
mtw_driver_loaded(struct module *mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		mtw_etag = EVENTHANDLER_REGISTER(usb_dev_configured,
		    mtw_autoinst, NULL, EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(usb_dev_configured, mtw_etag);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static const char *
mtw_get_rf(int rev)
{
	switch (rev) {
	case MT7601_RF_7601:
		return ("MT7601");
	case MT7610_RF_7610:
		return ("MT7610");
	case MT7612_RF_7612:
		return ("MT7612");
	}
	return ("unknown");
}
static int
mtw_wlan_enable(struct mtw_softc *sc, int enable)
{
	uint32_t tmp;
	int error = 0;

	if (enable) {
		mtw_read(sc, MTW_WLAN_CTRL, &tmp);
		if (sc->asic_ver == 0x7612)
			tmp &= ~0xfffff000;

		tmp &= ~MTW_WLAN_CLK_EN;
		tmp |= MTW_WLAN_EN;
		mtw_write(sc, MTW_WLAN_CTRL, tmp);
		mtw_delay(sc, 2);

		tmp |= MTW_WLAN_CLK_EN;
		if (sc->asic_ver == 0x7612) {
			tmp |= (MTW_WLAN_RESET | MTW_WLAN_RESET_RF);
		}
		mtw_write(sc, MTW_WLAN_CTRL, tmp);
		mtw_delay(sc, 2);

		mtw_read(sc, MTW_OSC_CTRL, &tmp);
		tmp |= MTW_OSC_EN;
		mtw_write(sc, MTW_OSC_CTRL, tmp);
		tmp |= MTW_OSC_CAL_REQ;
		mtw_write(sc, MTW_OSC_CTRL, tmp);
	} else {
		mtw_read(sc, MTW_WLAN_CTRL, &tmp);
		tmp &= ~(MTW_WLAN_CLK_EN | MTW_WLAN_EN);
		mtw_write(sc, MTW_WLAN_CTRL, tmp);

		mtw_read(sc, MTW_OSC_CTRL, &tmp);
		tmp &= ~MTW_OSC_EN;
		mtw_write(sc, MTW_OSC_CTRL, tmp);
	}
	return (error);
}

static int
mtw_read_cfg(struct mtw_softc *sc, uint16_t reg, uint32_t *val)
{
	usb_device_request_t req;
	uint32_t tmp;
	uint16_t actlen;
	int error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = MTW_READ_CFG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 4);
	error = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, &tmp, 0,
	    &actlen, 1000);

	if (error == 0)
		*val = le32toh(tmp);
	else
		*val = 0xffffffff;
	return (error);
}

static int
mtw_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != 0)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(mtw_devs, sizeof(mtw_devs), uaa));
}

static int
mtw_attach(device_t self)
{
	struct mtw_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t ver;
	int i, ret;
	uint32_t tmp;
	uint8_t iface_index;
	int ntries, error;

	device_set_usb_desc(self);
	sc->sc_udev = uaa->device;
	sc->sc_dev = self;
	sc->sc_sent = 0;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev),
                 MTX_NETWORK_LOCK, MTX_DEF);

	iface_index = 0;

	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    mtw_config, MTW_N_XFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(sc->sc_dev,
		    "could not allocate USB transfers, "
		    "err=%s\n",
		    usbd_errstr(error));
		goto detach;
	}
	for (i = 0; i < 4; i++) {
		sc->txd_fw[i] = (struct mtw_txd_fw *)
		    malloc(sizeof(struct mtw_txd_fw),
			M_USBDEV, M_NOWAIT | M_ZERO);
	}
	MTW_LOCK(sc);
	sc->sc_idx = 0;
	mbufq_init(&sc->sc_snd, ifqmaxlen);

	/*enable WLAN core */
	if ((error = mtw_wlan_enable(sc, 1)) != 0) {
		device_printf(sc->sc_dev, "could not enable WLAN core\n");
		return (ENXIO);
	}

	/* wait for the chip to settle */
	DELAY(100);
	for (ntries = 0; ntries < 100; ntries++) {
		if (mtw_read(sc, MTW_ASIC_VER, &ver) != 0) {
			goto detach;
		}
		if (ver != 0 && ver != 0xffffffff)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "timeout waiting for NIC to initialize\n");
		goto detach;
	}
	sc->asic_ver = ver >> 16;
	sc->asic_rev = ver & 0xffff;
	DELAY(100);
	if (sc->asic_ver != 0x7601) {
		device_printf(sc->sc_dev,
		    "Your revision 0x04%x is not supported yet\n",
		 sc->asic_rev);
		goto detach;
	}


	if (mtw_read(sc, MTW_MAC_VER_ID, &tmp) != 0)
		goto detach;
	sc->mac_rev = tmp & 0xffff;

	mtw_load_microcode(sc);
	ret = msleep(&sc->fwloading, &sc->sc_mtx, 0, "fwload", 3 * hz);
	if (ret == EWOULDBLOCK || sc->fwloading != 1) {
		device_printf(sc->sc_dev,
		    "timeout waiting for MCU to initialize\n");
		goto detach;
	}

	sc->sc_srom_read = mtw_efuse_read_2;
	/* retrieve RF rev. no and various other things from EEPROM */
	mtw_read_eeprom(sc);

	device_printf(sc->sc_dev,
	    "MAC/BBP RT%04X (rev 0x%04X), RF %s (MIMO %dT%dR), address %s\n",
	    sc->asic_ver, sc->mac_rev, mtw_get_rf(sc->rf_rev), sc->ntxchains,
	    sc->nrxchains, ether_sprintf(ic->ic_macaddr));
	DELAY(100);

	//mtw_set_leds(sc,5);
	// mtw_mcu_radio(sc,0x31,0);
	MTW_UNLOCK(sc);


	ic->ic_softc = sc;
	ic->ic_name = device_get_nameunit(self);
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;   /* default to BSS mode */

	ic->ic_caps = IEEE80211_C_STA | /* station mode supported */
		IEEE80211_C_MONITOR |	/* monitor mode supported */
		IEEE80211_C_IBSS |
		IEEE80211_C_HOSTAP |
		IEEE80211_C_WDS | /* 4-address traffic works */
		IEEE80211_C_MBSS |
		IEEE80211_C_SHPREAMBLE | /* short preamble supported */
		IEEE80211_C_SHSLOT |     /* short slot time supported */
		IEEE80211_C_WME |             /* WME */
		IEEE80211_C_WPA;	     /* WPA1|WPA2(RSN) */
	    device_printf(sc->sc_dev, "[HT] Enabling 802.11n\n");
	    ic->ic_htcaps =	  IEEE80211_HTC_HT
            | IEEE80211_HTC_AMPDU
            | IEEE80211_HTC_AMSDU
            | IEEE80211_HTCAP_MAXAMSDU_3839
            | IEEE80211_HTCAP_SMPS_OFF;

	ic->ic_rxstream = sc->nrxchains;
	ic->ic_txstream = sc->ntxchains;

	ic->ic_cryptocaps = IEEE80211_CRYPTO_WEP | IEEE80211_CRYPTO_AES_CCM |
	    IEEE80211_CRYPTO_AES_OCB | IEEE80211_CRYPTO_TKIP |
	    IEEE80211_CRYPTO_TKIPMIC;

	ic->ic_flags |= IEEE80211_F_DATAPAD;
	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;

	mtw_getradiocaps(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	ieee80211_ifattach(ic);

	ic->ic_scan_start = mtw_scan_start;
	ic->ic_scan_end = mtw_scan_end;
	ic->ic_set_channel = mtw_set_channel;
	ic->ic_getradiocaps = mtw_getradiocaps;
	ic->ic_node_alloc = mtw_node_alloc;
	ic->ic_newassoc = mtw_newassoc;
	ic->ic_update_mcast = mtw_update_mcast;
	ic->ic_updateslot = mtw_updateslot;
	ic->ic_wme.wme_update = mtw_wme_update;
	ic->ic_raw_xmit = mtw_raw_xmit;
	ic->ic_update_promisc = mtw_update_promisc;
	ic->ic_vap_create = mtw_vap_create;
	ic->ic_vap_delete = mtw_vap_delete;
	ic->ic_transmit = mtw_transmit;
	ic->ic_parent = mtw_parent;

	ic->ic_update_chw = mtw_update_chw;
	ic->ic_ampdu_enable = mtw_ampdu_enable;

	ieee80211_radiotap_attach(ic, &sc->sc_txtap.wt_ihdr,
	    sizeof(sc->sc_txtap), MTW_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
	    MTW_RX_RADIOTAP_PRESENT);
	TASK_INIT(&sc->cmdq_task, 0, mtw_cmdq_cb, sc);
	TASK_INIT(&sc->ratectl_task, 0, mtw_ratectl_cb, sc);
	usb_callout_init_mtx(&sc->ratectl_ch, &sc->sc_mtx, 0);

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

detach:
	MTW_UNLOCK(sc);
	mtw_detach(self);
	return (ENXIO);
}

static void
mtw_drain_mbufq(struct mtw_softc *sc)
{
	struct mbuf *m;
	struct ieee80211_node *ni;

	MTW_LOCK_ASSERT(sc, MA_OWNED);
	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;
		ieee80211_free_node(ni);
		m_freem(m);
	}
}

static int
mtw_detach(device_t self)
{
	struct mtw_softc *sc = device_get_softc(self);
	struct ieee80211com *ic = &sc->sc_ic;
	int i;
	MTW_LOCK(sc);
	mtw_reset(sc);
	DELAY(10000);
	sc->sc_detached = 1;
	MTW_UNLOCK(sc);


	/* stop all USB transfers */
	for (i = 0; i < MTW_N_XFER; i++)
		usbd_transfer_drain(sc->sc_xfer[i]);

	MTW_LOCK(sc);
	sc->ratectl_run = MTW_RATECTL_OFF;
	sc->cmdq_run = sc->cmdq_key_set = MTW_CMDQ_ABORT;

	/* free TX list, if any */
	if (ic->ic_nrunning > 0)
		for (i = 0; i < MTW_EP_QUEUES; i++)
			mtw_unsetup_tx_list(sc, &sc->sc_epq[i]);

	/* Free TX queue */
	mtw_drain_mbufq(sc);
	MTW_UNLOCK(sc);
	if (sc->sc_ic.ic_softc == sc) {
		/* drain tasks */
		usb_callout_drain(&sc->ratectl_ch);
		ieee80211_draintask(ic, &sc->cmdq_task);
		ieee80211_draintask(ic, &sc->ratectl_task);
		ieee80211_ifdetach(ic);
	}
	for (i = 0; i < 4; i++) {
		free(sc->txd_fw[i], M_USBDEV);
	}
	firmware_unregister("/mediatek/mt7601u");
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static struct ieee80211vap *
mtw_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct mtw_softc *sc = ic->ic_softc;
	struct mtw_vap *rvp;
	struct ieee80211vap *vap;
	int i;

	if (sc->rvp_cnt >= MTW_VAP_MAX) {
		device_printf(sc->sc_dev, "number of VAPs maxed out\n");
		return (NULL);
	}

	switch (opmode) {
	case IEEE80211_M_STA:
		/* enable s/w bmiss handling for sta mode */
		flags |= IEEE80211_CLONE_NOBEACONS;
		/* fall though */
	case IEEE80211_M_IBSS:
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_HOSTAP:
	case IEEE80211_M_MBSS:
		/* other than WDS vaps, only one at a time */
		if (!TAILQ_EMPTY(&ic->ic_vaps))
			return (NULL);
		break;
	case IEEE80211_M_WDS:
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			if (vap->iv_opmode != IEEE80211_M_HOSTAP)
				continue;
			/* WDS vap's always share the local mac address. */
			flags &= ~IEEE80211_CLONE_BSSID;
			break;
		}
		if (vap == NULL) {
			device_printf(sc->sc_dev,
			    "wds only supported in ap mode\n");
			return (NULL);
		}
		break;
	default:
		device_printf(sc->sc_dev, "unknown opmode %d\n", opmode);
		return (NULL);
	}

	rvp = malloc(sizeof(struct mtw_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &rvp->vap;

	if (ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid) !=
	    0) {
		/* out of memory */
		free(rvp, M_80211_VAP);
		return (NULL);
	}

	vap->iv_update_beacon = mtw_update_beacon;
	vap->iv_max_aid = MTW_WCID_MAX;

	/*
	 * The linux rt2800 driver limits 1 stream devices to a 32KB
	 * RX AMPDU.
	 */
	if (ic->ic_rxstream > 1)
		vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_64K;
	else
		vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_64K;
	vap->iv_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_2; /* 2uS */

	/*
	 * To delete the right key from h/w, we need wcid.
	 * Luckily, there is unused space in ieee80211_key{}, wk_pad,
	 * and matching wcid will be written into there. So, cast
	 * some spells to remove 'const' from ieee80211_key{}
	 */
	vap->iv_key_delete = (void *)mtw_key_delete;
	vap->iv_key_set = (void *)mtw_key_set;

	// override state transition machine
	rvp->newstate = vap->iv_newstate;
	vap->iv_newstate = mtw_newstate;
	if (opmode == IEEE80211_M_IBSS) {
		rvp->recv_mgmt = vap->iv_recv_mgmt;
		vap->iv_recv_mgmt = mtw_recv_mgmt;
	}

	ieee80211_ratectl_init(vap);
	ieee80211_ratectl_setinterval(vap, 1000); // 1 second

	/* complete setup */
	ieee80211_vap_attach(vap, mtw_media_change, ieee80211_media_status,
	    mac);

	/* make sure id is always unique */
	for (i = 0; i < MTW_VAP_MAX; i++) {
		if ((sc->rvp_bmap & 1 << i) == 0) {
			sc->rvp_bmap |= 1 << i;
			rvp->rvp_id = i;
			break;
		}
	}
	if (sc->rvp_cnt++ == 0)
		ic->ic_opmode = opmode;

	if (opmode == IEEE80211_M_HOSTAP)
		sc->cmdq_run = MTW_CMDQ_GO;

	MTW_DPRINTF(sc, MTW_DEBUG_STATE, "rvp_id=%d bmap=%x rvp_cnt=%d\n",
	    rvp->rvp_id, sc->rvp_bmap, sc->rvp_cnt);

	return (vap);
}

static void
mtw_vap_delete(struct ieee80211vap *vap)
{
	struct mtw_vap *rvp = MTW_VAP(vap);
	struct ieee80211com *ic;
	struct mtw_softc *sc;
	uint8_t rvp_id;

	if (vap == NULL)
		return;

	ic = vap->iv_ic;
	sc = ic->ic_softc;

	MTW_LOCK(sc);
	m_freem(rvp->beacon_mbuf);
	rvp->beacon_mbuf = NULL;

	rvp_id = rvp->rvp_id;
	sc->ratectl_run &= ~(1 << rvp_id);
	sc->rvp_bmap &= ~(1 << rvp_id);
	mtw_set_region_4(sc, MTW_SKEY(rvp_id, 0), 0, 256);
	mtw_set_region_4(sc, (0x7800 + (rvp_id) * 512), 0, 512);
	--sc->rvp_cnt;

	MTW_DPRINTF(sc, MTW_DEBUG_STATE,
	    "vap=%p rvp_id=%d bmap=%x rvp_cnt=%d\n", vap, rvp_id, sc->rvp_bmap,
	    sc->rvp_cnt);

	MTW_UNLOCK(sc);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(rvp, M_80211_VAP);
}

/*
 * There are numbers of functions need to be called in context thread.
 * Rather than creating taskqueue event for each of those functions,
 * here is all-for-one taskqueue callback function. This function
 * guarantees deferred functions are executed in the same order they
 * were enqueued.
 * '& MTW_CMDQ_MASQ' is to loop cmdq[].
 */
static void
mtw_cmdq_cb(void *arg, int pending)
{
	struct mtw_softc *sc = arg;
	uint8_t i;
	/* call cmdq[].func locked */
	MTW_LOCK(sc);
	for (i = sc->cmdq_exec; sc->cmdq[i].func && pending;
	     i = sc->cmdq_exec, pending--) {
		MTW_DPRINTF(sc, MTW_DEBUG_CMD, "cmdq_exec=%d pending=%d\n", i,
		    pending);
		if (sc->cmdq_run == MTW_CMDQ_GO) {
			/*
			 * If arg0 is NULL, callback func needs more
			 * than one arg. So, pass ptr to cmdq struct.
			 */
			if (sc->cmdq[i].arg0)
				sc->cmdq[i].func(sc->cmdq[i].arg0);
			else
				sc->cmdq[i].func(&sc->cmdq[i]);
		}
		sc->cmdq[i].arg0 = NULL;
		sc->cmdq[i].func = NULL;
		sc->cmdq_exec++;
		sc->cmdq_exec &= MTW_CMDQ_MASQ;
	}
	MTW_UNLOCK(sc);
}

static void
mtw_setup_tx_list(struct mtw_softc *sc, struct mtw_endpoint_queue *pq)
{
	struct mtw_tx_data *data;

	memset(pq, 0, sizeof(*pq));

	STAILQ_INIT(&pq->tx_qh);
	STAILQ_INIT(&pq->tx_fh);

	for (data = &pq->tx_data[0]; data < &pq->tx_data[MTW_TX_RING_COUNT];
	     data++) {
		data->sc = sc;
		STAILQ_INSERT_TAIL(&pq->tx_fh, data, next);
	}
	pq->tx_nfree = MTW_TX_RING_COUNT;
}

static void
mtw_unsetup_tx_list(struct mtw_softc *sc, struct mtw_endpoint_queue *pq)
{
	struct mtw_tx_data *data;
	/* make sure any subsequent use of the queues will fail */
	pq->tx_nfree = 0;

	STAILQ_INIT(&pq->tx_fh);
	STAILQ_INIT(&pq->tx_qh);

	/* free up all node references and mbufs */
	for (data = &pq->tx_data[0]; data < &pq->tx_data[MTW_TX_RING_COUNT];
	     data++) {
		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}
}

static int
mtw_write_ivb(struct mtw_softc *sc, void *buf, uint16_t len)
{
	usb_device_request_t req;
	uint16_t actlen;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MTW_RESET;
	USETW(req.wValue, 0x12);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	int error = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf,
	    0, &actlen, 1000);

	return (error);
}

static int
mtw_write_cfg(struct mtw_softc *sc, uint16_t reg, uint32_t val)
{
	usb_device_request_t req;
	int error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MTW_WRITE_CFG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 4);
	val = htole32(val);
	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, &val);
	return (error);
}

static int
mtw_usb_dma_write(struct mtw_softc *sc, uint32_t val)
{
	// if (sc->asic_ver == 0x7612)
	//		return mtw_write_cfg(sc, MTW_USB_U3DMA_CFG, val);
	//	else
	return (mtw_write(sc, MTW_USB_DMA_CFG, val));
}

static void
mtw_ucode_setup(struct mtw_softc *sc)
{

	mtw_usb_dma_write(sc, (MTW_USB_TX_EN | MTW_USB_RX_EN));
	mtw_write(sc, MTW_FCE_PSE_CTRL, 1);
	mtw_write(sc, MTW_TX_CPU_FCE_BASE, 0x400230);
	mtw_write(sc, MTW_TX_CPU_FCE_MAX_COUNT, 1);
	mtw_write(sc, MTW_MCU_FW_IDX, 1);
	mtw_write(sc, MTW_FCE_PDMA, 0x44);
	mtw_write(sc, MTW_FCE_SKIP_FS, 3);
}
static int
mtw_ucode_write(struct mtw_softc *sc, const uint8_t *fw, const uint8_t *ivb,
    int32_t len, uint32_t offset)
{

	// struct usb_attach_arg *uaa = device_get_ivars(sc->sc_dev);
#if 0 // firmware not tested

	if (sc->asic_ver == 0x7612 && offset >= 0x90000)
		blksz = 0x800; /* MT7612 ROM Patch */

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL) {
		error = ENOMEM;
		goto fail;
	}
	buf = usbd_alloc_buffer(xfer, blksz + 12);
	if (buf == NULL) {
		error = ENOMEM;
		goto fail;
	}
#endif



	int mlen;
	int idx = 0;

	mlen = 0x2c44;

	while (len > 0) {

		if (len < 0x2c44 && len > 0) {
			mlen = len;
		}

		sc->txd_fw[idx]->len = htole16(mlen);
		sc->txd_fw[idx]->flags = htole16(MTW_TXD_DATA | MTW_TXD_MCU);

		memcpy(&sc->txd_fw[idx]->fw, fw, mlen);
		// memcpy(&txd[1], fw, mlen);
		//	memset(&txd[1]  + mlen, 0, MTW_DMA_PAD);
		//		mtw_write_cfg(sc, MTW_MCU_DMA_ADDR, offset
		//+sent); 1mtw_write_cfg(sc, MTW_MCU_DMA_LEN, (mlen << 16));

		//	  sc->sc_fw_data[idx]->len=htole16(mlen);

		// memcpy(tmpbuf,fw,mlen);
		// memset(tmpbuf+mlen,0,MTW_DMA_PAD);
		// memcpy(sc->sc_fw_data[idx].buf, fw, mlen);

		fw += mlen;
		len -= mlen;
		// sent+=mlen;
		idx++;
	}
	sc->sc_sent = 0;
	memcpy(sc->sc_ivb_1, ivb, MTW_MCU_IVB_LEN);

	usbd_transfer_start(sc->sc_xfer[7]);

	return (0);
}

static void
mtw_load_microcode(void *arg)
{

	struct mtw_softc *sc = (struct mtw_softc *)arg;
	const struct mtw_ucode_hdr *hdr;
	// onst struct mtw_ucode *fw = NULL;
	const char *fwname;
	size_t size;
	int error = 0;
	uint32_t tmp, iofs = 0x40;
	//	int ntries;
	int dlen, ilen;
	device_printf(sc->sc_dev, "version:0x%hx\n", sc->asic_ver);
	/* is firmware already running? */
	mtw_read_cfg(sc, MTW_MCU_DMA_ADDR, &tmp);
	if (tmp == MTW_MCU_READY) {
		return;
	}
	if (sc->asic_ver == 0x7612) {
		fwname = "mtw-mt7662u_rom_patch";

		const struct firmware *firmware = firmware_get_flags(fwname,FIRMWARE_GET_NOWARN);
		if (firmware == NULL) {
			device_printf(sc->sc_dev,
			    "failed loadfirmware of file %s (error %d)\n",
			    fwname, error);
			return;
		}
		size = firmware->datasize;

		const struct mtw_ucode *fw = (const struct mtw_ucode *)
						 firmware->data;
		hdr = (const struct mtw_ucode_hdr *)&fw->hdr;
		// memcpy(fw,(const unsigned char*)firmware->data +
		// 0x1e,size-0x1e);
		ilen = size - 0x1e;

		mtw_ucode_setup(sc);

		if ((error = mtw_ucode_write(sc, firmware->data, fw->ivb, ilen,
			 0x90000)) != 0) {
			goto fail;
		}
		mtw_usb_dma_write(sc, 0x00e41814);
	}

	fwname = "/mediatek/mt7601u.bin";
	iofs = 0x40;
	// dofs = 0;
	if (sc->asic_ver == 0x7612) {
		fwname = "mtw-mt7662u";
		iofs = 0x80040;
		//	dofs = 0x110800;
	} else if (sc->asic_ver == 0x7610) {
		fwname = "mt7610u";
		// dofs = 0x80000;
	}
	MTW_UNLOCK(sc);
	const struct firmware *firmware = firmware_get_flags(fwname, FIRMWARE_GET_NOWARN);

	if (firmware == NULL) {
		device_printf(sc->sc_dev,
		    "failed loadfirmware of file %s (error %d)\n", fwname,
		    error);
		MTW_LOCK(sc);
		return;
	}
	MTW_LOCK(sc);
	size = firmware->datasize;
	MTW_DPRINTF(sc, MTW_DEBUG_FIRMWARE, "firmware size:%zu\n", size);
	const struct mtw_ucode *fw = (const struct mtw_ucode *)firmware->data;

	if (size < sizeof(struct mtw_ucode_hdr)) {
		device_printf(sc->sc_dev, "firmware header too short\n");
		goto fail;
	}

	hdr = (const struct mtw_ucode_hdr *)&fw->hdr;

	if (size < sizeof(struct mtw_ucode_hdr) + le32toh(hdr->ilm_len) +
		le32toh(hdr->dlm_len)) {
		device_printf(sc->sc_dev, "firmware payload too short\n");
		goto fail;
	}

	ilen = le32toh(hdr->ilm_len) - MTW_MCU_IVB_LEN;
	dlen = le32toh(hdr->dlm_len);

	if (ilen > size || dlen > size) {
		device_printf(sc->sc_dev, "firmware payload too large\n");
		goto fail;
	}

	mtw_write(sc, MTW_FCE_PDMA, 0);
	mtw_write(sc, MTW_FCE_PSE_CTRL, 0);
	mtw_ucode_setup(sc);

	if ((error = mtw_ucode_write(sc, fw->data, fw->ivb, ilen, iofs)) != 0)
		device_printf(sc->sc_dev, "Could not write ucode errro=%d\n",
		    error);

	device_printf(sc->sc_dev, "loaded firmware ver %.8x %.8x %s\n",
	    le32toh(hdr->fw_ver), le32toh(hdr->build_ver), hdr->build_time);

	return;
fail:
	return;
}
static usb_error_t
mtw_do_request(struct mtw_softc *sc, struct usb_device_request *req, void *data)
{
	usb_error_t err;
	int ntries = 5;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	while (ntries--) {
		err = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, req, data,
		    0, NULL, 2000); // ms seconds
		if (err == 0)
			break;
		MTW_DPRINTF(sc, MTW_DEBUG_USB,
		    "Control request failed, %s (retrying)\n",
		    usbd_errstr(err));
		mtw_delay(sc, 10);
	}
	return (err);
}

static int
mtw_read(struct mtw_softc *sc, uint16_t reg, uint32_t *val)
{
	uint32_t tmp;
	int error;

	error = mtw_read_region_1(sc, reg, (uint8_t *)&tmp, sizeof tmp);
	if (error == 0)
		*val = le32toh(tmp);
	else
		*val = 0xffffffff;
	return (error);
}

static int
mtw_read_region_1(struct mtw_softc *sc, uint16_t reg, uint8_t *buf, int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = MTW_READ_REGION_1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	return (mtw_do_request(sc, &req, buf));
}

static int
mtw_write_2(struct mtw_softc *sc, uint16_t reg, uint16_t val)
{

	usb_device_request_t req;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MTW_WRITE_2;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);
	return (usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, NULL));
}

static int
mtw_write(struct mtw_softc *sc, uint16_t reg, uint32_t val)
{

	int error;

	if ((error = mtw_write_2(sc, reg, val & 0xffff)) == 0) {

		error = mtw_write_2(sc, reg + 2, val >> 16);
	}

	return (error);
}

static int
mtw_write_region_1(struct mtw_softc *sc, uint16_t reg, uint8_t *buf, int len)
{

	usb_device_request_t req;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MTW_WRITE_REGION_1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);
	return (usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, buf));
}

static int
mtw_set_region_4(struct mtw_softc *sc, uint16_t reg, uint32_t val, int count)
{
	int i, error = 0;

	KASSERT((count & 3) == 0, ("mte_set_region_4: Invalid data length.\n"));
	for (i = 0; i < count && error == 0; i += 4)
		error = mtw_write(sc, reg + i, val);
	return (error);
}

static int
mtw_efuse_read_2(struct mtw_softc *sc, uint16_t addr, uint16_t *val)
{

	uint32_t tmp;
	uint16_t reg;
	int error, ntries;

	if ((error = mtw_read(sc, MTW_EFUSE_CTRL, &tmp)) != 0)
		return (error);

	addr *= 2;
	/*
	 * Read one 16-byte block into registers EFUSE_DATA[0-3]:
	 * DATA0: 3 2 1 0
	 * DATA1: 7 6 5 4
	 * DATA2: B A 9 8
	 * DATA3: F E D C
	 */
	tmp &= ~(MTW_EFSROM_MODE_MASK | MTW_EFSROM_AIN_MASK);
	tmp |= (addr & ~0xf) << MTW_EFSROM_AIN_SHIFT | MTW_EFSROM_KICK;
	mtw_write(sc, MTW_EFUSE_CTRL, tmp);
	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = mtw_read(sc, MTW_EFUSE_CTRL, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_EFSROM_KICK))
			break;
		DELAY(2);
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	if ((tmp & MTW_EFUSE_AOUT_MASK) == MTW_EFUSE_AOUT_MASK) {
		*val = 0xffff; // address not found
		return (0);
	}
// determine to which 32-bit register our 16-bit word belongs
	reg = MTW_EFUSE_DATA0 + (addr & 0xc);
	if ((error = mtw_read(sc, reg, &tmp)) != 0)
		return (error);

	*val = (addr & 2) ? tmp >> 16 : tmp & 0xffff;
	return (0);
}

static __inline int
mtw_srom_read(struct mtw_softc *sc, uint16_t addr, uint16_t *val)
{
	/* either eFUSE ROM or EEPROM */
	return (sc->sc_srom_read(sc, addr, val));
}

static int
mtw_bbp_read(struct mtw_softc *sc, uint8_t reg, uint8_t *val)
{
	uint32_t tmp;
	int ntries, error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = mtw_read(sc, MTW_BBP_CSR, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	tmp = MTW_BBP_CSR_READ | MTW_BBP_CSR_KICK | reg << 8;
	if ((error = mtw_write(sc, MTW_BBP_CSR, tmp)) != 0)
		return (error);

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = mtw_read(sc, MTW_BBP_CSR, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	*val = tmp & 0xff;
	return (0);
}

static int
mtw_bbp_write(struct mtw_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries, error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = mtw_read(sc, MTW_BBP_CSR, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	tmp = MTW_BBP_CSR_KICK | reg << 8 | val;
	return (mtw_write(sc, MTW_BBP_CSR, tmp));
}

static int
mtw_mcu_cmd(struct mtw_softc *sc, u_int8_t cmd, void *buf, int len)
{
	sc->sc_idx = 0;
	sc->txd_fw[sc->sc_idx]->len = htole16(
	    len + 8);
	sc->txd_fw[sc->sc_idx]->flags = htole16(MTW_TXD_CMD | MTW_TXD_MCU |
	    (cmd & 0x1f) << MTW_TXD_CMD_SHIFT | (0 & 0xf));

	memset(&sc->txd_fw[sc->sc_idx]->fw, 0, 2004);
	memcpy(&sc->txd_fw[sc->sc_idx]->fw, buf, len);
	usbd_transfer_start(sc->sc_xfer[7]);
	return (0);
}

/*
 * Add `delta' (signed) to each 4-bit sub-word of a 32-bit word.
 * Used to adjust per-rate Tx power registers.
 */
static __inline uint32_t
b4inc(uint32_t b32, int8_t delta)
{
	int8_t i, b4;

	for (i = 0; i < 8; i++) {
		b4 = b32 & 0xf;
		b4 += delta;
		if (b4 < 0)
			b4 = 0;
		else if (b4 > 0xf)
			b4 = 0xf;
		b32 = b32 >> 4 | b4 << 28;
	}
	return (b32);
}
static void
mtw_get_txpower(struct mtw_softc *sc)
{
	uint16_t val;
	int i;

	/* Read power settings for 2GHz channels. */
	for (i = 0; i < 14; i += 2) {
		mtw_srom_read(sc, MTW_EEPROM_PWR2GHZ_BASE1 + i / 2, &val);
		sc->txpow1[i + 0] = (int8_t)(val & 0xff);
		sc->txpow1[i + 1] = (int8_t)(val >> 8);
		mtw_srom_read(sc, MTW_EEPROM_PWR2GHZ_BASE2 + i / 2, &val);
		sc->txpow2[i + 0] = (int8_t)(val & 0xff);
		sc->txpow2[i + 1] = (int8_t)(val >> 8);
	}
	/* Fix broken Tx power entries. */
	for (i = 0; i < 14; i++) {
		if (sc->txpow1[i] < 0 || sc->txpow1[i] > 27)
			sc->txpow1[i] = 5;
		if (sc->txpow2[i] < 0 || sc->txpow2[i] > 27)
			sc->txpow2[i] = 5;
		MTW_DPRINTF(sc, MTW_DEBUG_TXPWR,
		"chan %d: power1=%d, power2=%d\n", mt7601_rf_chan[i].chan,
		sc->txpow1[i], sc->txpow2[i]);
	}
}

struct ieee80211_node *
mtw_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	return (malloc(sizeof(struct mtw_node), M_80211_NODE,
		M_NOWAIT | M_ZERO));
}
static int
mtw_read_eeprom(struct mtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int8_t delta_2ghz, delta_5ghz;
	uint16_t val;
	int ridx, ant;

	sc->sc_srom_read = mtw_efuse_read_2;

	/* read RF information */
	mtw_srom_read(sc, MTW_EEPROM_CHIPID, &val);
	sc->rf_rev = val;
	mtw_srom_read(sc, MTW_EEPROM_ANTENNA, &val);
	sc->ntxchains = (val >> 4) & 0xf;
	sc->nrxchains = val & 0xf;
	MTW_DPRINTF(sc, MTW_DEBUG_ROM, "EEPROM RF rev=0x%02x chains=%dT%dR\n",
	    sc->rf_rev, sc->ntxchains, sc->nrxchains);

	/* read ROM version */
	mtw_srom_read(sc, MTW_EEPROM_VERSION, &val);
	MTW_DPRINTF(sc, MTW_DEBUG_ROM, "EEPROM rev=%d, FAE=%d\n", val & 0xff,
	    val >> 8);

	/* read MAC address */
	mtw_srom_read(sc, MTW_EEPROM_MAC01, &val);
	ic->ic_macaddr[0] = val & 0xff;
	ic->ic_macaddr[1] = val >> 8;
	mtw_srom_read(sc, MTW_EEPROM_MAC23, &val);
	ic->ic_macaddr[2] = val & 0xff;
	ic->ic_macaddr[3] = val >> 8;
	mtw_srom_read(sc, MTW_EEPROM_MAC45, &val);
	ic->ic_macaddr[4] = val & 0xff;
	ic->ic_macaddr[5] = val >> 8;
#if 0
	printf("eFUSE ROM\n00: ");
	for (int i = 0; i < 256; i++) {
		if (((i % 8) == 0) && i > 0)
			printf("\n%02x: ", i);
		mtw_srom_read(sc, i, &val);
		printf(" %04x", val);
	}
	printf("\n");
#endif
	/* check if RF supports automatic Tx access gain control */
	mtw_srom_read(sc, MTW_EEPROM_CONFIG, &val);
	device_printf(sc->sc_dev, "EEPROM CFG 0x%04x\n", val);
	if ((val & 0xff) != 0xff) {
		sc->ext_5ghz_lna = (val >> 3) & 1;
		sc->ext_2ghz_lna = (val >> 2) & 1;
		/* check if RF supports automatic Tx access gain control */
		sc->calib_2ghz = sc->calib_5ghz = (val >> 1) & 1;
		/* check if we have a hardware radio switch */
		sc->rfswitch = val & 1;
	}

	/* read RF frequency offset from EEPROM */
	mtw_srom_read(sc, MTW_EEPROM_FREQ_OFFSET, &val);
	if ((val & 0xff) != 0xff)
		sc->rf_freq_offset = val;
	else
		sc->rf_freq_offset = 0;
	MTW_DPRINTF(sc, MTW_DEBUG_ROM, "frequency offset 0x%x\n",
	    sc->rf_freq_offset);

	/* Read Tx power settings. */
	mtw_get_txpower(sc);

	/* read Tx power compensation for each Tx rate */
	mtw_srom_read(sc, MTW_EEPROM_DELTAPWR, &val);
	delta_2ghz = delta_5ghz = 0;
	if ((val & 0xff) != 0xff && (val & 0x80)) {
		delta_2ghz = val & 0xf;
		if (!(val & 0x40)) /* negative number */
			delta_2ghz = -delta_2ghz;
	}
	val >>= 8;
	if ((val & 0xff) != 0xff && (val & 0x80)) {
		delta_5ghz = val & 0xf;
		if (!(val & 0x40)) /* negative number */
			delta_5ghz = -delta_5ghz;
	}
	MTW_DPRINTF(sc, MTW_DEBUG_ROM | MTW_DEBUG_TXPWR,
	    "power compensation=%d (2GHz), %d (5GHz)\n", delta_2ghz,
	    delta_5ghz);

	for (ridx = 0; ridx < 5; ridx++) {
		uint32_t reg;

		mtw_srom_read(sc, MTW_EEPROM_RPWR + ridx * 2, &val);
		reg = val;
		mtw_srom_read(sc, MTW_EEPROM_RPWR + ridx * 2 + 1, &val);
		reg |= (uint32_t)val << 16;

		sc->txpow20mhz[ridx] = reg;
		sc->txpow40mhz_2ghz[ridx] = b4inc(reg, delta_2ghz);
		sc->txpow40mhz_5ghz[ridx] = b4inc(reg, delta_5ghz);

		MTW_DPRINTF(sc, MTW_DEBUG_ROM | MTW_DEBUG_TXPWR,
		    "ridx %d: power 20MHz=0x%08x, 40MHz/2GHz=0x%08x, "
		    "40MHz/5GHz=0x%08x\n",
		    ridx, sc->txpow20mhz[ridx], sc->txpow40mhz_2ghz[ridx],
		    sc->txpow40mhz_5ghz[ridx]);
	}

	/* read RSSI offsets and LNA gains from EEPROM */
	val = 0;
	mtw_srom_read(sc, MTW_EEPROM_RSSI1_2GHZ, &val);
	sc->rssi_2ghz[0] = val & 0xff; /* Ant A */
	sc->rssi_2ghz[1] = val >> 8;   /* Ant B */
	mtw_srom_read(sc, MTW_EEPROM_RSSI2_2GHZ, &val);
	/*
	 * On RT3070 chips (limited to 2 Rx chains), this ROM
	 * field contains the Tx mixer gain for the 2GHz band.
	 */
	if ((val & 0xff) != 0xff)
		sc->txmixgain_2ghz = val & 0x7;
	MTW_DPRINTF(sc, MTW_DEBUG_ROM, "tx mixer gain=%u (2GHz)\n",
	    sc->txmixgain_2ghz);
	sc->lna[2] = val >> 8; /* channel group 2 */
	mtw_srom_read(sc, MTW_EEPROM_RSSI1_5GHZ, &val);
	sc->rssi_5ghz[0] = val & 0xff; /* Ant A */
	sc->rssi_5ghz[1] = val >> 8;   /* Ant B */
	mtw_srom_read(sc, MTW_EEPROM_RSSI2_5GHZ, &val);
	sc->rssi_5ghz[2] = val & 0xff; /* Ant C */

	sc->lna[3] = val >> 8; /* channel group 3 */

	mtw_srom_read(sc, MTW_EEPROM_LNA, &val);
	sc->lna[0] = val & 0xff; /* channel group 0 */
	sc->lna[1] = val >> 8;	 /* channel group 1 */
	MTW_DPRINTF(sc, MTW_DEBUG_ROM, "LNA0 0x%x\n", sc->lna[0]);

	/* fix broken 5GHz LNA entries */
	if (sc->lna[2] == 0 || sc->lna[2] == 0xff) {
		MTW_DPRINTF(sc, MTW_DEBUG_ROM,
		    "invalid LNA for channel group %d\n", 2);
		sc->lna[2] = sc->lna[1];
	}
	if (sc->lna[3] == 0 || sc->lna[3] == 0xff) {
		MTW_DPRINTF(sc, MTW_DEBUG_ROM,
		    "invalid LNA for channel group %d\n", 3);
		sc->lna[3] = sc->lna[1];
	}

	/* fix broken RSSI offset entries */
	for (ant = 0; ant < 3; ant++) {
		if (sc->rssi_2ghz[ant] < -10 || sc->rssi_2ghz[ant] > 10) {
			MTW_DPRINTF(sc, MTW_DEBUG_ROM,
			    "invalid RSSI%d offset: %d (2GHz)\n", ant + 1,
			    sc->rssi_2ghz[ant]);
			sc->rssi_2ghz[ant] = 0;
		}
		if (sc->rssi_5ghz[ant] < -10 || sc->rssi_5ghz[ant] > 10) {
			MTW_DPRINTF(sc, MTW_DEBUG_ROM,
			    "invalid RSSI%d offset: %d (5GHz)\n", ant + 1,
			    sc->rssi_5ghz[ant]);
			sc->rssi_5ghz[ant] = 0;
		}
	}
	return (0);
}
static int
mtw_media_change(if_t ifp)
{
	struct ieee80211vap *vap = if_getsoftc(ifp);
	struct ieee80211com *ic = vap->iv_ic;
	const struct ieee80211_txparam *tp;
	struct mtw_softc *sc = ic->ic_softc;
	uint8_t rate, ridx;

	MTW_LOCK(sc);
	ieee80211_media_change(ifp);
	//tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
	tp = &vap->iv_txparms[ic->ic_curmode];
	if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
		struct ieee80211_node *ni;
		struct mtw_node	*rn;
		/* XXX TODO: methodize with MCS rates */
		rate =
		    ic->ic_sup_rates[ic->ic_curmode].rs_rates[tp->ucastrate] &
		    IEEE80211_RATE_VAL;
		for (ridx = 0; ridx < MTW_RIDX_MAX; ridx++) {
			if (rt2860_rates[ridx].rate == rate)
				break;
		}
		ni = ieee80211_ref_node(vap->iv_bss);
		rn = MTW_NODE(ni);
		rn->fix_ridx = ridx;

		MTW_DPRINTF(sc, MTW_DEBUG_RATE, "rate=%d, fix_ridx=%d\n", rate,
		    rn->fix_ridx);
		ieee80211_free_node(ni);
	}
	MTW_UNLOCK(sc);

	return (0);
}

void
mtw_set_leds(struct mtw_softc *sc, uint16_t which)
{
	struct mtw_mcu_cmd_8 cmd;
	cmd.func = htole32(0x1);
	cmd.val = htole32(which);
	mtw_mcu_cmd(sc, CMD_LED_MODE, &cmd, sizeof(struct mtw_mcu_cmd_8));
}
static void
mtw_abort_tsf_sync(struct mtw_softc *sc)
{
	uint32_t tmp;

	mtw_read(sc, MTW_BCN_TIME_CFG, &tmp);
	tmp &= ~(MTW_BCN_TX_EN | MTW_TSF_TIMER_EN | MTW_TBTT_TIMER_EN);
	mtw_write(sc, MTW_BCN_TIME_CFG, tmp);
}
static int
mtw_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	const struct ieee80211_txparam *tp;
	struct ieee80211com *ic = vap->iv_ic;
	struct mtw_softc *sc = ic->ic_softc;
	struct mtw_vap *rvp = MTW_VAP(vap);
	enum ieee80211_state ostate;
	uint32_t sta[3];
	uint8_t ratectl = 0;
	uint8_t restart_ratectl = 0;
	uint8_t bid = 1 << rvp->rvp_id;


	ostate = vap->iv_state;
	MTW_DPRINTF(sc, MTW_DEBUG_STATE, "%s -> %s\n",
	    ieee80211_state_name[ostate], ieee80211_state_name[nstate]);
	IEEE80211_UNLOCK(ic);
	MTW_LOCK(sc);
	ratectl = sc->ratectl_run; /* remember current state */
	usb_callout_stop(&sc->ratectl_ch);
	sc->ratectl_run = MTW_RATECTL_OFF;
	if (ostate == IEEE80211_S_RUN) {
		/* turn link LED off */
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
	restart_ratectl = 1;
		if (ostate != IEEE80211_S_RUN)
			break;

		ratectl &= ~bid;
		sc->runbmap &= ~bid;

		/* abort TSF synchronization if there is no vap running  */
		if (--sc->running == 0)
			mtw_abort_tsf_sync(sc);
		break;

	case IEEE80211_S_RUN:
		if (!(sc->runbmap & bid)) {
			if (sc->running++)
				restart_ratectl = 1;
			sc->runbmap |= bid;
		}

		m_freem(rvp->beacon_mbuf);
		rvp->beacon_mbuf = NULL;

		switch (vap->iv_opmode) {
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_MBSS:
			sc->ap_running |= bid;
			ic->ic_opmode = vap->iv_opmode;
			mtw_update_beacon_cb(vap);
			break;
		case IEEE80211_M_IBSS:
			sc->adhoc_running |= bid;
			if (!sc->ap_running)
				ic->ic_opmode = vap->iv_opmode;
			mtw_update_beacon_cb(vap);
			break;
		case IEEE80211_M_STA:
			sc->sta_running |= bid;
			if (!sc->ap_running && !sc->adhoc_running)
				ic->ic_opmode = vap->iv_opmode;

			/* read statistic counters (clear on read) */
			mtw_read_region_1(sc, MTW_TX_STA_CNT0, (uint8_t *)sta,
			    sizeof sta);

			break;
		default:
			ic->ic_opmode = vap->iv_opmode;
			break;
		}

		if (vap->iv_opmode != IEEE80211_M_MONITOR) {
			struct ieee80211_node *ni;

			if (ic->ic_bsschan == IEEE80211_CHAN_ANYC) {
				MTW_UNLOCK(sc);
				IEEE80211_LOCK(ic);
				return (-1);
			}
			mtw_updateslot(ic);
			mtw_enable_mrr(sc);
			mtw_set_txpreamble(sc);
			mtw_set_basicrates(sc);
			ni = ieee80211_ref_node(vap->iv_bss);
			IEEE80211_ADDR_COPY(sc->sc_bssid, ni->ni_bssid);
			mtw_set_bssid(sc, sc->sc_bssid);
			ieee80211_free_node(ni);
			mtw_enable_tsf_sync(sc);

			/* enable automatic rate adaptation */
			tp = &vap->iv_txparms[ieee80211_chan2mode(
			    ic->ic_curchan)];
			if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE)
				ratectl |= bid;
		} else {
		mtw_enable_tsf_sync(sc);
		}

		break;
	default:
		MTW_DPRINTF(sc, MTW_DEBUG_STATE, "undefined state\n");
		break;
	}

	/* restart amrr for running VAPs */
	if ((sc->ratectl_run = ratectl) && restart_ratectl) {
		usb_callout_reset(&sc->ratectl_ch, hz, mtw_ratectl_to, sc);
	}
	MTW_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return (rvp->newstate(vap, nstate, arg));
}

static int
mtw_wme_update(struct ieee80211com *ic)
{
	struct chanAccParams chp;
	struct mtw_softc *sc = ic->ic_softc;
	const struct wmeParams *ac;
	int aci, error = 0;
	ieee80211_wme_ic_getparams(ic, &chp);
	ac = chp.cap_wmeParams;

	MTW_LOCK(sc);
	/* update MAC TX configuration registers */
	for (aci = 0; aci < WME_NUM_AC; aci++) {
		error = mtw_write(sc, MTW_EDCA_AC_CFG(aci),
		    ac[aci].wmep_logcwmax << 16 | ac[aci].wmep_logcwmin << 12 |
			ac[aci].wmep_aifsn << 8 | ac[aci].wmep_txopLimit);
		if (error)
			goto err;
	}

	/* update SCH/DMA registers too */
	error = mtw_write(sc, MTW_WMM_AIFSN_CFG,
	    ac[WME_AC_VO].wmep_aifsn << 12 | ac[WME_AC_VI].wmep_aifsn << 8 |
		ac[WME_AC_BK].wmep_aifsn << 4 | ac[WME_AC_BE].wmep_aifsn);
	if (error)
		goto err;
	error = mtw_write(sc, MTW_WMM_CWMIN_CFG,
	    ac[WME_AC_VO].wmep_logcwmin << 12 |
		ac[WME_AC_VI].wmep_logcwmin << 8 |
		ac[WME_AC_BK].wmep_logcwmin << 4 | ac[WME_AC_BE].wmep_logcwmin);
	if (error)
		goto err;
	error = mtw_write(sc, MTW_WMM_CWMAX_CFG,
	    ac[WME_AC_VO].wmep_logcwmax << 12 |
		ac[WME_AC_VI].wmep_logcwmax << 8 |
		ac[WME_AC_BK].wmep_logcwmax << 4 | ac[WME_AC_BE].wmep_logcwmax);
	if (error)
		goto err;
	error = mtw_write(sc, MTW_WMM_TXOP0_CFG,
	    ac[WME_AC_BK].wmep_txopLimit << 16 | ac[WME_AC_BE].wmep_txopLimit);
	if (error)
		goto err;
	error = mtw_write(sc, MTW_WMM_TXOP1_CFG,
	    ac[WME_AC_VO].wmep_txopLimit << 16 | ac[WME_AC_VI].wmep_txopLimit);

err:
	MTW_UNLOCK(sc);
	if (error)
		MTW_DPRINTF(sc, MTW_DEBUG_USB, "WME update failed\n");

	return (error);
}

static int
mtw_key_set(struct ieee80211vap *vap, struct ieee80211_key *k)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct mtw_softc *sc = ic->ic_softc;
	uint32_t i;

	i = MTW_CMDQ_GET(&sc->cmdq_store);
	MTW_DPRINTF(sc, MTW_DEBUG_KEY, "cmdq_store=%d\n", i);
	sc->cmdq[i].func = mtw_key_set_cb;
	sc->cmdq[i].arg0 = NULL;
	sc->cmdq[i].arg1 = vap;
	sc->cmdq[i].k = k;
	IEEE80211_ADDR_COPY(sc->cmdq[i].mac, k->wk_macaddr);
	ieee80211_runtask(ic, &sc->cmdq_task);

	/*
	 * To make sure key will be set when hostapd
	 * calls iv_key_set() before if_init().
	 */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		MTW_LOCK(sc);
		sc->cmdq_key_set = MTW_CMDQ_GO;
		MTW_UNLOCK(sc);
	}

	return (1);
}
static void
mtw_key_set_cb(void *arg)
{
	struct mtw_cmdq *cmdq = arg;
	struct ieee80211vap *vap = cmdq->arg1;
	struct ieee80211_key *k = cmdq->k;
	struct ieee80211com *ic = vap->iv_ic;
	struct mtw_softc *sc = ic->ic_softc;
	struct ieee80211_node *ni;
	u_int cipher = k->wk_cipher->ic_cipher;
	uint32_t attr;
	uint16_t base;
	uint8_t mode, wcid, iv[8];
	MTW_LOCK_ASSERT(sc, MA_OWNED);

	if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		ni = ieee80211_find_vap_node(&ic->ic_sta, vap, cmdq->mac);
	else
		ni = vap->iv_bss;

	/* map net80211 cipher to RT2860 security mode */
	switch (cipher) {
	case IEEE80211_CIPHER_WEP:
		if (k->wk_keylen < 8)
			mode = MTW_MODE_WEP40;
		else
			mode = MTW_MODE_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		mode = MTW_MODE_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		mode = MTW_MODE_AES_CCMP;
		break;
	default:
		MTW_DPRINTF(sc, MTW_DEBUG_KEY, "undefined case\n");
		return;
	}

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		wcid = 0; /* NB: update WCID0 for group keys */
		base = MTW_SKEY(0, k->wk_keyix);
	} else {
		wcid = (ni != NULL) ? MTW_AID2WCID(ni->ni_associd) : 0;
		base = MTW_PKEY(wcid);
	}

	if (cipher == IEEE80211_CIPHER_TKIP) {
		mtw_write_region_1(sc, base, k->wk_key, 16);
		mtw_write_region_1(sc, base + 16, &k->wk_key[24], 8);
		mtw_write_region_1(sc, base + 24, &k->wk_key[16], 8);
	} else {
		/* roundup len to 16-bit: XXX fix write_region_1() instead */
		mtw_write_region_1(sc, base, k->wk_key,
		    (k->wk_keylen + 1) & ~1);
	}

	if (!(k->wk_flags & IEEE80211_KEY_GROUP) ||
	    (k->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))) {
		/* set initial packet number in IV+EIV */
		if (cipher == IEEE80211_CIPHER_WEP) {
			memset(iv, 0, sizeof iv);
			iv[3] = vap->iv_def_txkey << 6;
		} else {
			if (cipher == IEEE80211_CIPHER_TKIP) {
				iv[0] = k->wk_keytsc >> 8;
				iv[1] = (iv[0] | 0x20) & 0x7f;
				iv[2] = k->wk_keytsc;
			} else  { //CCMP
				iv[0] = k->wk_keytsc;
				iv[1] = k->wk_keytsc >> 8;
				iv[2] = 0;
			}
			iv[3] = k->wk_keyix << 6 | IEEE80211_WEP_EXTIV;
			iv[4] = k->wk_keytsc >> 16;
			iv[5] = k->wk_keytsc >> 24;
			iv[6] = k->wk_keytsc >> 32;
			iv[7] = k->wk_keytsc >> 40;
		}
		mtw_write_region_1(sc, MTW_IVEIV(wcid), iv, 8);
	}

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		/* install group key */
		mtw_read(sc, MTW_SKEY_MODE_0_7, &attr);
		attr &= ~(0xf << (k->wk_keyix * 4));
		attr |= mode << (k->wk_keyix * 4);
		mtw_write(sc, MTW_SKEY_MODE_0_7, attr);

		if (cipher & (IEEE80211_CIPHER_WEP)) {
			mtw_read(sc, MTW_WCID_ATTR(wcid + 1), &attr);
			attr = (attr & ~0xf) | (mode << 1);
			mtw_write(sc, MTW_WCID_ATTR(wcid + 1), attr);

			mtw_set_region_4(sc, MTW_IVEIV(0), 0, 4);

			mtw_read(sc, MTW_WCID_ATTR(wcid), &attr);
			attr = (attr & ~0xf) | (mode << 1);
			mtw_write(sc, MTW_WCID_ATTR(wcid), attr);
		}
	} else {
		/* install pairwise key */
		mtw_read(sc, MTW_WCID_ATTR(wcid), &attr);
		attr = (attr & ~0xf) | (mode << 1) | MTW_RX_PKEY_EN;
		mtw_write(sc, MTW_WCID_ATTR(wcid), attr);
	}
	k->wk_pad = wcid;
}

/*
 * If wlan is destroyed without being brought down i.e. without
 * wlan down or wpa_cli terminate, this function is called after
 * vap is gone. Don't refer it.
 */
static void
mtw_key_delete_cb(void *arg)
{
	struct mtw_cmdq *cmdq = arg;
	struct mtw_softc *sc = cmdq->arg1;
	struct ieee80211_key *k = &cmdq->key;
	uint32_t attr;
	uint8_t wcid;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		/* remove group key */
		MTW_DPRINTF(sc, MTW_DEBUG_KEY, "removing group key\n");
		mtw_read(sc, MTW_SKEY_MODE_0_7, &attr);
		attr &= ~(0xf << (k->wk_keyix * 4));
		mtw_write(sc, MTW_SKEY_MODE_0_7, attr);
	} else {
		/* remove pairwise key */
		MTW_DPRINTF(sc, MTW_DEBUG_KEY, "removing key for wcid %x\n",
		    k->wk_pad);
		/* matching wcid was written to wk_pad in mtw_key_set() */
		wcid = k->wk_pad;
		mtw_read(sc, MTW_WCID_ATTR(wcid), &attr);
		attr &= ~0xf;
		mtw_write(sc, MTW_WCID_ATTR(wcid), attr);
	}

	k->wk_pad = 0;
}

/*
 * return 0 on error
 */
static int
mtw_key_delete(struct ieee80211vap *vap, struct ieee80211_key *k)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct mtw_softc *sc = ic->ic_softc;
	struct ieee80211_key *k0;
	uint32_t i;
	if (sc->sc_flags & MTW_RUNNING)
		return (1);

	/*
	 * When called back, key might be gone. So, make a copy
	 * of some values need to delete keys before deferring.
	 * But, because of LOR with node lock, cannot use lock here.
	 * So, use atomic instead.
	 */
	i = MTW_CMDQ_GET(&sc->cmdq_store);
	MTW_DPRINTF(sc, MTW_DEBUG_KEY, "cmdq_store=%d\n", i);
	sc->cmdq[i].func = mtw_key_delete_cb;
	sc->cmdq[i].arg0 = NULL;
	sc->cmdq[i].arg1 = sc;
	k0 = &sc->cmdq[i].key;
	k0->wk_flags = k->wk_flags;
	k0->wk_keyix = k->wk_keyix;
	/* matching wcid was written to wk_pad in mtw_key_set() */
	k0->wk_pad = k->wk_pad;
	ieee80211_runtask(ic, &sc->cmdq_task);
	return (1); /* return fake success */
}

static void
mtw_ratectl_to(void *arg)
{
	struct mtw_softc *sc = arg;
	/* do it in a process context, so it can go sleep */
	ieee80211_runtask(&sc->sc_ic, &sc->ratectl_task);
	/* next timeout will be rescheduled in the callback task */
}

/* ARGSUSED */
static void
mtw_ratectl_cb(void *arg, int pending)
{

	struct mtw_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (vap == NULL)
		return;

	ieee80211_iterate_nodes(&ic->ic_sta, mtw_iter_func, sc);

	usb_callout_reset(&sc->ratectl_ch, hz, mtw_ratectl_to, sc);


}

static void
mtw_drain_fifo(void *arg)
{
	struct mtw_softc *sc = arg;
	uint32_t stat;
	uint16_t(*wstat)[3];
	uint8_t wcid, mcs, pid;
	int8_t retry;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	for (;;) {
		/* drain Tx status FIFO (maxsize = 16) */
		mtw_read(sc, MTW_TX_STAT_FIFO, &stat);
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "tx stat 0x%08x\n", stat);
		if (!(stat & MTW_TXQ_VLD))
			break;

		wcid = (stat >> MTW_TXQ_WCID_SHIFT) & 0xff;

		/* if no ACK was requested, no feedback is available */
		if (!(stat & MTW_TXQ_ACKREQ) || wcid > MTW_WCID_MAX ||
		    wcid == 0)
			continue;

		/*
		 * Even though each stat is Tx-complete-status like format,
		 * the device can poll stats. Because there is no guarantee
		 * that the referring node is still around when read the stats.
		 * So that, if we use ieee80211_ratectl_tx_update(), we will
		 * have hard time not to refer already freed node.
		 *
		 * To eliminate such page faults, we poll stats in softc.
		 * Then, update the rates later with
		 * ieee80211_ratectl_tx_update().
		 */
		wstat = &(sc->wcid_stats[wcid]);
		(*wstat)[MTW_TXCNT]++;
		if (stat & MTW_TXQ_OK)
			(*wstat)[MTW_SUCCESS]++;
		else
			counter_u64_add(sc->sc_ic.ic_oerrors, 1);
		/*
		 * Check if there were retries, ie if the Tx success rate is
		 * different from the requested rate. Note that it works only
		 * because we do not allow rate fallback from OFDM to CCK.
		 */
		mcs = (stat >> MTW_TXQ_MCS_SHIFT) & 0x7f;
		pid = (stat >> MTW_TXQ_PID_SHIFT) & 0xf;
		if ((retry = pid - 1 - mcs) > 0) {
			(*wstat)[MTW_TXCNT] += retry;
			(*wstat)[MTW_RETRY] += retry;
		}
	}
	MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "count=%d\n", sc->fifo_cnt);

	sc->fifo_cnt = 0;
}

static void
mtw_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct mtw_softc *sc = arg;
	MTW_LOCK(sc);
	struct ieee80211_ratectl_tx_stats *txs = &sc->sc_txs;
	struct ieee80211vap *vap = ni->ni_vap;
	struct mtw_node *rn = MTW_NODE(ni);
	uint32_t sta[3];
	uint16_t(*wstat)[3];
	int error, ridx;
	uint8_t txrate = 0;

	/* Check for special case */
	if (sc->rvp_cnt <= 1 && vap->iv_opmode == IEEE80211_M_STA &&
	    ni != vap->iv_bss)
		goto fail;

	txs->flags = IEEE80211_RATECTL_TX_STATS_NODE |
	    IEEE80211_RATECTL_TX_STATS_RETRIES;
	txs->ni = ni;
	if (sc->rvp_cnt <= 1 &&
	    (vap->iv_opmode == IEEE80211_M_IBSS ||
		vap->iv_opmode == IEEE80211_M_STA)) {
		/*
		 * read statistic counters (clear on read) and update AMRR state
		 */
		error = mtw_read_region_1(sc, MTW_TX_STA_CNT0, (uint8_t *)sta,
		    sizeof sta);
		MTW_DPRINTF(sc, MTW_DEBUG_RATE, "error:%d\n", error);
		if (error != 0)
			goto fail;

		/* count failed TX as errors */
		if_inc_counter(vap->iv_ifp, IFCOUNTER_OERRORS,
		    le32toh(sta[0]) & 0xffff);

		txs->nretries = (le32toh(sta[1]) >> 16);
		txs->nsuccess = (le32toh(sta[1]) & 0xffff);
		/* nretries??? */
		txs->nframes = txs->nsuccess + (le32toh(sta[0]) & 0xffff);

		MTW_DPRINTF(sc, MTW_DEBUG_RATE,
		    "retrycnt=%d success=%d failcnt=%d\n", txs->nretries,
		    txs->nsuccess, le32toh(sta[0]) & 0xffff);
	} else {
		wstat = &(sc->wcid_stats[MTW_AID2WCID(ni->ni_associd)]);

		if (wstat == &(sc->wcid_stats[0]) ||
		    wstat > &(sc->wcid_stats[MTW_WCID_MAX]))
			goto fail;

		txs->nretries = (*wstat)[MTW_RETRY];
		txs->nsuccess = (*wstat)[MTW_SUCCESS];
		txs->nframes = (*wstat)[MTW_TXCNT];
		MTW_DPRINTF(sc, MTW_DEBUG_RATE,
		    "wstat retrycnt=%d txcnt=%d success=%d\n", txs->nretries,
		    txs->nframes, txs->nsuccess);

		memset(wstat, 0, sizeof(*wstat));
	}

	ieee80211_ratectl_tx_update(vap, txs);
	ieee80211_ratectl_rate(ni, NULL, 0);
	txrate = ieee80211_node_get_txrate_dot11rate(ni);

	/* XXX TODO: methodize with MCS rates */
	for (ridx = 0; ridx < MTW_RIDX_MAX; ridx++) {
		MTW_DPRINTF(sc, MTW_DEBUG_RATE, "ni_txrate=0x%x\n",
			     txrate);
		if (rt2860_rates[ridx].rate == txrate) {
			break;
		}
	}
	rn->amrr_ridx = ridx;
fail:
	MTW_UNLOCK(sc);

	MTW_DPRINTF(sc, MTW_DEBUG_RATE, "rate=%d, ridx=%d\n",
		    txrate, rn->amrr_ridx);
}

static void
mtw_newassoc_cb(void *arg)
{
	struct mtw_cmdq *cmdq = arg;
	struct ieee80211_node *ni = cmdq->arg1;
	struct mtw_softc *sc = ni->ni_vap->iv_ic->ic_softc;

	uint8_t wcid = cmdq->wcid;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	mtw_write_region_1(sc, MTW_WCID_ENTRY(wcid), ni->ni_macaddr,
	    IEEE80211_ADDR_LEN);

	memset(&(sc->wcid_stats[wcid]), 0, sizeof(sc->wcid_stats[wcid]));
}

static void
mtw_newassoc(struct ieee80211_node *ni, int isnew)
{

	struct mtw_node *mn = MTW_NODE(ni);
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct mtw_softc *sc = ic->ic_softc;

	uint8_t rate;
	uint8_t ridx;
	uint8_t wcid;
	//int i;
	// int i,j;
	wcid = MTW_AID2WCID(ni->ni_associd);

	if (wcid > MTW_WCID_MAX) {
		device_printf(sc->sc_dev, "wcid=%d out of range\n", wcid);
		return;
	}

	/* only interested in true associations */
	if (isnew && ni->ni_associd != 0) {
		/*
		 * This function could is called though timeout function.
		 * Need to deferggxr.
		 */

		uint32_t cnt = MTW_CMDQ_GET(&sc->cmdq_store);
		MTW_DPRINTF(sc, MTW_DEBUG_STATE, "cmdq_store=%d\n", cnt);
		sc->cmdq[cnt].func = mtw_newassoc_cb;
		sc->cmdq[cnt].arg0 = NULL;
		sc->cmdq[cnt].arg1 = ni;
		sc->cmdq[cnt].wcid = wcid;
		ieee80211_runtask(ic, &sc->cmdq_task);
	}

	MTW_DPRINTF(sc, MTW_DEBUG_STATE,
	    "new assoc isnew=%d associd=%x addr=%s\n", isnew, ni->ni_associd,
	    ether_sprintf(ni->ni_macaddr));
	rate = vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)].mgmtrate;
	/* XXX TODO: methodize with MCS rates */
	for (ridx = 0; ridx < MTW_RIDX_MAX; ridx++)
		if (rt2860_rates[ridx].rate == rate)
			break;
	mn->mgt_ridx = ridx;
	MTW_DPRINTF(sc, MTW_DEBUG_STATE | MTW_DEBUG_RATE,
	    "rate=%d, ctl_ridx=%d\n", rate, ridx);
	MTW_LOCK(sc);
	if (sc->ratectl_run != MTW_RATECTL_OFF) {
		usb_callout_reset(&sc->ratectl_ch, hz, &mtw_ratectl_to, sc);
	}
	MTW_UNLOCK(sc);

}

/*
 * Return the Rx chain with the highest RSSI for a given frame.
 */
static __inline uint8_t
mtw_maxrssi_chain(struct mtw_softc *sc, const struct mtw_rxwi *rxwi)
{
	uint8_t rxchain = 0;

	if (sc->nrxchains > 1) {
		if (rxwi->rssi[1] > rxwi->rssi[rxchain])
			rxchain = 1;
		if (sc->nrxchains > 2)
			if (rxwi->rssi[2] > rxwi->rssi[rxchain])
				rxchain = 2;
	}
	return (rxchain);
}
static void
mtw_get_tsf(struct mtw_softc *sc, uint64_t *buf)
{
	mtw_read_region_1(sc, MTW_TSF_TIMER_DW0, (uint8_t *)buf, sizeof(*buf));
}

static void
mtw_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m, int subtype,
    const struct ieee80211_rx_stats *rxs, int rssi, int nf)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct mtw_softc *sc = vap->iv_ic->ic_softc;
	struct mtw_vap *rvp = MTW_VAP(vap);
	uint64_t ni_tstamp, rx_tstamp;

	rvp->recv_mgmt(ni, m, subtype, rxs, rssi, nf);

	if (vap->iv_state == IEEE80211_S_RUN &&
	    (subtype == IEEE80211_FC0_SUBTYPE_BEACON ||
		subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)) {
		ni_tstamp = le64toh(ni->ni_tstamp.tsf);
		MTW_LOCK(sc);
		mtw_get_tsf(sc, &rx_tstamp);
		MTW_UNLOCK(sc);
		rx_tstamp = le64toh(rx_tstamp);

		if (ni_tstamp >= rx_tstamp) {
			MTW_DPRINTF(sc, MTW_DEBUG_RECV | MTW_DEBUG_BEACON,
			    "ibss merge, tsf %ju tstamp %ju\n",
			    (uintmax_t)rx_tstamp, (uintmax_t)ni_tstamp);
			(void)ieee80211_ibss_merge(ni);
		}
	}
}
static void
mtw_rx_frame(struct mtw_softc *sc, struct mbuf *m, uint32_t dmalen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct epoch_tracker et;

	struct mtw_rxwi *rxwi;
	uint32_t flags;
	uint16_t len, rxwisize;
	uint8_t ant, rssi;
	int8_t nf;

	rxwisize = sizeof(struct mtw_rxwi);

	if (__predict_false(
		dmalen < rxwisize + sizeof(struct ieee80211_frame_ack))) {
		MTW_DPRINTF(sc, MTW_DEBUG_RECV,
		    "payload is too short: dma length %u < %zu\n", dmalen,
		    rxwisize + sizeof(struct ieee80211_frame_ack));
		goto fail;
	}

	rxwi = mtod(m, struct mtw_rxwi *);
	len = le16toh(rxwi->len) & 0xfff;
	flags = le32toh(rxwi->flags);
	if (__predict_false(len > dmalen - rxwisize)) {
		MTW_DPRINTF(sc, MTW_DEBUG_RECV, "bad RXWI length %u > %u\n",
		    len, dmalen);
		goto fail;
	}

	if (__predict_false(flags & (MTW_RX_CRCERR | MTW_RX_ICVERR))) {
		MTW_DPRINTF(sc, MTW_DEBUG_RECV, "%s error.\n",
		    (flags & MTW_RX_CRCERR) ? "CRC" : "ICV");
		goto fail;
	}

	if (flags & MTW_RX_L2PAD) {
		MTW_DPRINTF(sc, MTW_DEBUG_RECV,
		    "received RT2860_RX_L2PAD frame\n");
		len += 2;
	}

	m->m_data += rxwisize;
	m->m_pkthdr.len = m->m_len = len;

	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		wh->i_fc[1] &= ~IEEE80211_FC1_PROTECTED;
		m->m_flags |= M_WEP;
	}

	if (len >= sizeof(struct ieee80211_frame_min)) {
		ni = ieee80211_find_rxnode(ic,
		    mtod(m, struct ieee80211_frame_min *));
	} else
		ni = NULL;

	if (ni && ni->ni_flags & IEEE80211_NODE_HT) {
		m->m_flags |= M_AMPDU;
	}

	if (__predict_false(flags & MTW_RX_MICERR)) {
		/* report MIC failures to net80211 for TKIP */
		if (ni != NULL)
			ieee80211_notify_michael_failure(ni->ni_vap, wh,
			    rxwi->keyidx);
		MTW_DPRINTF(sc, MTW_DEBUG_RECV,
		    "MIC error. Someone is lying.\n");
		goto fail;
	}

	ant = mtw_maxrssi_chain(sc, rxwi);
	rssi = rxwi->rssi[ant];
	nf = mtw_rssi2dbm(sc, rssi, ant);

	if (__predict_false(ieee80211_radiotap_active(ic))) {
		struct mtw_rx_radiotap_header *tap = &sc->sc_rxtap;
		uint16_t phy;

		tap->wr_flags = 0;
		if (flags & MTW_RX_L2PAD)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_DATAPAD;
		tap->wr_antsignal = rssi;
		tap->wr_antenna = ant;
		tap->wr_dbm_antsignal = mtw_rssi2dbm(sc, rssi, ant);
		tap->wr_rate = 2; /* in case it can't be found below */
		//MTW_LOCK(sc);

	//	MTW_UNLOCK(sc);
		phy = le16toh(rxwi->phy);
		switch (phy >> MT7601_PHY_SHIFT) {
		case MTW_PHY_CCK:
			switch ((phy & MTW_PHY_MCS) & ~MTW_PHY_SHPRE) {
			case 0:
				tap->wr_rate = 2;
				break;
			case 1:
				tap->wr_rate = 4;
				break;
			case 2:
				tap->wr_rate = 11;
				break;
			case 3:
				tap->wr_rate = 22;
				break;
			}
			if (phy & MTW_PHY_SHPRE)
				tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
			break;
		case MTW_PHY_OFDM:
			switch (phy & MTW_PHY_MCS) {
			case 0:
				tap->wr_rate = 12;
				break;
			case 1:
				tap->wr_rate = 18;
				break;
			case 2:
				tap->wr_rate = 24;
				break;
			case 3:
				tap->wr_rate = 36;
				break;
			case 4:
				tap->wr_rate = 48;
				break;
			case 5:
				tap->wr_rate = 72;
				break;
			case 6:
				tap->wr_rate = 96;
				break;
			case 7:
				tap->wr_rate = 108;
				break;
			}
			break;
		}
	}

	NET_EPOCH_ENTER(et);
	if (ni != NULL) {
		(void)ieee80211_input(ni, m, rssi, nf);
		ieee80211_free_node(ni);
	} else {
		(void)ieee80211_input_all(ic, m, rssi, nf);
	}
	NET_EPOCH_EXIT(et);

	return;

fail:
	m_freem(m);
	counter_u64_add(ic->ic_ierrors, 1);
}

static void
mtw_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct mtw_softc *sc = usbd_xfer_softc(xfer);
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m = NULL;
	struct mbuf *m0;
	uint32_t dmalen, mbuf_len;
	uint16_t rxwisize;
	int xferlen;

	rxwisize = sizeof(struct mtw_rxwi);

	usbd_xfer_status(xfer, &xferlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		MTW_DPRINTF(sc, MTW_DEBUG_RECV, "rx done, actlen=%d\n",
		    xferlen);
		if (xferlen < (int)(sizeof(uint32_t) + rxwisize +
				  sizeof(struct mtw_rxd))) {
			MTW_DPRINTF(sc, MTW_DEBUG_RECV_DESC | MTW_DEBUG_USB,
			    "xfer too short %d %d\n", xferlen,
			    (int)(sizeof(uint32_t) + rxwisize +
				sizeof(struct mtw_rxd)));
			goto tr_setup;
		}

		m = sc->rx_m;
		sc->rx_m = NULL;

		/* FALLTHROUGH */
	case USB_ST_SETUP:
	tr_setup:

		if (sc->rx_m == NULL) {
			sc->rx_m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR,
			    MTW_MAX_RXSZ);
		}
		if (sc->rx_m == NULL) {
			MTW_DPRINTF(sc,
			    MTW_DEBUG_RECV | MTW_DEBUG_RECV_DESC |
				MTW_DEBUG_USB,
			    "could not allocate mbuf - idle with stall\n");
			counter_u64_add(ic->ic_ierrors, 1);
			usbd_xfer_set_stall(xfer);
			usbd_xfer_set_frames(xfer, 0);
		} else {
			/*
			 * Directly loading a mbuf cluster into DMA to
			 * save some data copying. This works because
			 * there is only one cluster.
			 */
			usbd_xfer_set_frame_data(xfer, 0,
			    mtod(sc->rx_m, caddr_t), MTW_MAX_RXSZ);
			usbd_xfer_set_frames(xfer, 1);
		}
		usbd_transfer_submit(xfer);
		break;

	default: /* Error */
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT | MTW_DEBUG_USB,
		    "USB transfer error, %s\n", usbd_errstr(error));

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			if (error == USB_ERR_TIMEOUT)
				device_printf(sc->sc_dev, "device timeout %s\n",
				    __func__);
			counter_u64_add(ic->ic_ierrors, 1);
			goto tr_setup;
		}
		if (sc->rx_m != NULL) {
			m_freem(sc->rx_m);
			sc->rx_m = NULL;
		}
		break;
	}

	if (m == NULL)
		return;

	/* inputting all the frames must be last */

	MTW_UNLOCK(sc);

	m->m_pkthdr.len = m->m_len = xferlen;

	/* HW can aggregate multiple 802.11 frames in a single USB xfer */
	for (;;) {
		dmalen = le32toh(*mtod(m, uint32_t *)) & 0xffff;

		if ((dmalen >= (uint32_t)-8) || (dmalen == 0) ||
		    ((dmalen & 3) != 0)) {
			MTW_DPRINTF(sc, MTW_DEBUG_RECV_DESC | MTW_DEBUG_USB,
			    "bad DMA length %u\n", dmalen);
			break;
		}
		if ((dmalen + 8) > (uint32_t)xferlen) {
			MTW_DPRINTF(sc, MTW_DEBUG_RECV_DESC | MTW_DEBUG_USB,
			    "bad DMA length %u > %d\n", dmalen + 8, xferlen);
			break;
		}

		/* If it is the last one or a single frame, we won't copy. */
		if ((xferlen -= dmalen + 8) <= 8) {
			/* trim 32-bit DMA-len header */
			m->m_data += 4;
			m->m_pkthdr.len = m->m_len -= 4;
			mtw_rx_frame(sc, m, dmalen);
			m = NULL; /* don't free source buffer */
			break;
		}

		mbuf_len = dmalen + sizeof(struct mtw_rxd);
		if (__predict_false(mbuf_len > MCLBYTES)) {
			MTW_DPRINTF(sc, MTW_DEBUG_RECV_DESC | MTW_DEBUG_USB,
			    "payload is too big: mbuf_len %u\n", mbuf_len);
			counter_u64_add(ic->ic_ierrors, 1);
			break;
		}

		/* copy aggregated frames to another mbuf */
		m0 = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (__predict_false(m0 == NULL)) {
			MTW_DPRINTF(sc, MTW_DEBUG_RECV_DESC,
			    "could not allocate mbuf\n");
			counter_u64_add(ic->ic_ierrors, 1);
			break;
		}
		m_copydata(m, 4 /* skip 32-bit DMA-len header */, mbuf_len,
		    mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len = mbuf_len;
		mtw_rx_frame(sc, m0, dmalen);

		/* update data ptr */
		m->m_data += mbuf_len + 4;
		m->m_pkthdr.len = m->m_len -= mbuf_len + 4;
	}

	/* make sure we free the source buffer, if any */
	m_freem(m);

#ifdef IEEE80211_SUPPORT_SUPERG
	ieee80211_ff_age_all(ic, 100);
#endif
	MTW_LOCK(sc);
}

static void
mtw_tx_free(struct mtw_endpoint_queue *pq, struct mtw_tx_data *data, int txerr)
{

	ieee80211_tx_complete(data->ni, data->m, txerr);
	data->m = NULL;
	data->ni = NULL;

	STAILQ_INSERT_TAIL(&pq->tx_fh, data, next);
	pq->tx_nfree++;
}
static void
mtw_bulk_tx_callbackN(struct usb_xfer *xfer, usb_error_t error, u_int index)
{
	struct mtw_softc *sc = usbd_xfer_softc(xfer);
	struct ieee80211com *ic = &sc->sc_ic;
	struct mtw_tx_data *data;
	struct ieee80211vap *vap = NULL;
	struct usb_page_cache *pc;
	struct mtw_endpoint_queue *pq = &sc->sc_epq[index];
	struct mbuf *m;
	usb_frlength_t size;
	int actlen;
	int sumlen;
	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT | MTW_DEBUG_USB,
		    "transfer complete: %d bytes @ index %d\n", actlen, index);

		data = usbd_xfer_get_priv(xfer);
		mtw_tx_free(pq, data, 0);
		usbd_xfer_set_priv(xfer, NULL);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
	tr_setup:
		data = STAILQ_FIRST(&pq->tx_qh);
		if (data == NULL)
			break;

		STAILQ_REMOVE_HEAD(&pq->tx_qh, next);

		m = data->m;

		size = sizeof(data->desc);
		if ((m->m_pkthdr.len + size + 3 + 8) > MTW_MAX_TXSZ) {
			MTW_DPRINTF(sc, MTW_DEBUG_XMIT_DESC | MTW_DEBUG_USB,
			    "data overflow, %u bytes\n", m->m_pkthdr.len);
			mtw_tx_free(pq, data, 1);
			goto tr_setup;
		}

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, &data->desc, size);
		usbd_m_copy_in(pc, size, m, 0, m->m_pkthdr.len);
		size += m->m_pkthdr.len;
		/*
		 * Align end on a 4-byte boundary, pad 8 bytes (CRC +
		 * 4-byte padding), and be sure to zero those trailing
		 * bytes:
		 */
		usbd_frame_zero(pc, size, ((-size) & 3) + MTW_DMA_PAD);
		size += ((-size) & 3) + MTW_DMA_PAD;

		vap = data->ni->ni_vap;
		if (ieee80211_radiotap_active_vap(vap)) {
			const struct ieee80211_frame *wh;
			struct mtw_tx_radiotap_header *tap = &sc->sc_txtap;
			struct mtw_txwi *txwi =
			    (struct mtw_txwi *)(&data->desc +
				sizeof(struct mtw_txd));
			int has_l2pad;

			wh = mtod(m, struct ieee80211_frame *);
			has_l2pad = IEEE80211_HAS_ADDR4(wh) !=
			    IEEE80211_QOS_HAS_SEQ(wh);

			tap->wt_flags = 0;
			tap->wt_rate = rt2860_rates[data->ridx].rate;
			tap->wt_hwqueue = index;
			if (le16toh(txwi->phy) & MTW_PHY_SHPRE)
				tap->wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
			if (has_l2pad)
				tap->wt_flags |= IEEE80211_RADIOTAP_F_DATAPAD;

			ieee80211_radiotap_tx(vap, m);
		}

		MTW_DPRINTF(sc, MTW_DEBUG_XMIT | MTW_DEBUG_USB,
		    "sending frame len=%u/%u @ index %d\n", m->m_pkthdr.len,
		    size, index);

		usbd_xfer_set_frame_len(xfer, 0, size);
		usbd_xfer_set_priv(xfer, data);
		usbd_transfer_submit(xfer);
		mtw_start(sc);

		break;

	default:
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT | MTW_DEBUG_USB,
		    "USB transfer error, %s\n", usbd_errstr(error));

		data = usbd_xfer_get_priv(xfer);

		if (data != NULL) {
			if (data->ni != NULL)
				vap = data->ni->ni_vap;
			mtw_tx_free(pq, data, error);
			usbd_xfer_set_priv(xfer, NULL);
		}

		if (vap == NULL)
			vap = TAILQ_FIRST(&ic->ic_vaps);

		if (error != USB_ERR_CANCELLED) {
			if (error == USB_ERR_TIMEOUT) {
				device_printf(sc->sc_dev, "device timeout %s\n",
				    __func__);
				uint32_t i = MTW_CMDQ_GET(&sc->cmdq_store);
				MTW_DPRINTF(sc, MTW_DEBUG_XMIT | MTW_DEBUG_USB,
				    "cmdq_store=%d\n", i);
				sc->cmdq[i].func = mtw_usb_timeout_cb;
				sc->cmdq[i].arg0 = vap;
				ieee80211_runtask(ic, &sc->cmdq_task);
			}

			/*
			 * Try to clear stall first, also if other
			 * errors occur, hence clearing stall
			 * introduces a 50 ms delay:
			 */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
#ifdef IEEE80211_SUPPORT_SUPERG
	/* XXX TODO: make this deferred rather than unlock/relock */
	/* XXX TODO: should only do the QoS AC this belongs to */
	if (pq->tx_nfree >= MTW_TX_RING_COUNT) {
		MTW_UNLOCK(sc);
		ieee80211_ff_flush_all(ic);
		MTW_LOCK(sc);
	}
#endif
}

static void
mtw_fw_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct mtw_softc *sc = usbd_xfer_softc(xfer);

	int actlen;
	int ntries, tmp;
	// struct mtw_txd *data;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);
	// data = usbd_xfer_get_priv(xfer);
	usbd_xfer_set_priv(xfer, NULL);
	switch (USB_GET_STATE(xfer)) {

	case USB_ST_TRANSFERRED:
		sc->sc_sent += actlen;
		memset(sc->txd_fw[sc->sc_idx], 0, actlen);

		if (actlen < 0x2c44 && sc->sc_idx == 0) {
			return;
		}
		if (sc->sc_idx == 3) {

			if ((error = mtw_write_ivb(sc, sc->sc_ivb_1,
				    MTW_MCU_IVB_LEN)) != 0) {
				device_printf(sc->sc_dev,
				    "Could not write ivb error:  %d\n", error);
			}

			mtw_delay(sc, 10);
			for (ntries = 0; ntries < 100; ntries++) {
				if ((error = mtw_read_cfg(sc, MTW_MCU_DMA_ADDR,
					 &tmp)) != 0) {
					device_printf(sc->sc_dev,
				    "Could not read cfg error:  %d\n", error);

				}
				if (tmp == MTW_MCU_READY) {
					MTW_DPRINTF(sc, MTW_DEBUG_FIRMWARE,
					    "mcu reaady %d\n", tmp);
					sc->fwloading = 1;
					break;
				}

				mtw_delay(sc, 10);
			}
			if (ntries == 100)
				sc->fwloading = 0;
			wakeup(&sc->fwloading);
			return;
		}

		if (actlen == 0x2c44) {
			sc->sc_idx++;
			DELAY(1000);
		}

	case USB_ST_SETUP: {
		int dlen = 0;
		dlen = sc->txd_fw[sc->sc_idx]->len;

		mtw_write_cfg(sc, MTW_MCU_DMA_ADDR, 0x40 + sc->sc_sent);
		mtw_write_cfg(sc, MTW_MCU_DMA_LEN, (dlen << 16));

		usbd_xfer_set_frame_len(xfer, 0, dlen);
		usbd_xfer_set_frame_data(xfer, 0, sc->txd_fw[sc->sc_idx], dlen);

		// usbd_xfer_set_priv(xfer,sc->txd[sc->sc_idx]);
		usbd_transfer_submit(xfer);
		break;

	default: /* Error */
		device_printf(sc->sc_dev, "%s:%d %s\n", __FILE__, __LINE__,
		    usbd_errstr(error));
		sc->fwloading = 0;
		wakeup(&sc->fwloading);
		/*
		 * Print error message and clear stall
		 * for example.
		 */
		break;
	}
		/*
		 * Here it is safe to do something without the private
		 * USB mutex	locked.
		 */
	}
	return;
}
static void
mtw_bulk_tx_callback0(struct usb_xfer *xfer, usb_error_t error)
{
	mtw_bulk_tx_callbackN(xfer, error, 0);
}

static void
mtw_bulk_tx_callback1(struct usb_xfer *xfer, usb_error_t error)
{


	mtw_bulk_tx_callbackN(xfer, error, 1);
}

static void
mtw_bulk_tx_callback2(struct usb_xfer *xfer, usb_error_t error)
{
	mtw_bulk_tx_callbackN(xfer, error, 2);
}

static void
mtw_bulk_tx_callback3(struct usb_xfer *xfer, usb_error_t error)
{
	mtw_bulk_tx_callbackN(xfer, error, 3);
}

static void
mtw_bulk_tx_callback4(struct usb_xfer *xfer, usb_error_t error)
{
	mtw_bulk_tx_callbackN(xfer, error, 4);
}

static void
mtw_bulk_tx_callback5(struct usb_xfer *xfer, usb_error_t error)
{
	mtw_bulk_tx_callbackN(xfer, error, 5);
}

static void
mtw_set_tx_desc(struct mtw_softc *sc, struct mtw_tx_data *data)
{
	struct mbuf *m = data->m;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = data->ni->ni_vap;
	struct ieee80211_frame *wh;
	struct mtw_txd *txd;
	struct mtw_txwi *txwi;
	uint16_t xferlen, txwisize;
	uint16_t mcs;
	uint8_t ridx = data->ridx;
	uint8_t pad;

	/* get MCS code from rate index */
	mcs = rt2860_rates[ridx].mcs;

	txwisize = sizeof(*txwi);
	xferlen = txwisize + m->m_pkthdr.len;

	/* roundup to 32-bit alignment */
	xferlen = (xferlen + 3) & ~3;

	txd = (struct mtw_txd *)&data->desc;
	txd->len = htole16(xferlen);

	wh = mtod(m, struct ieee80211_frame *);

	/*
	 * Ether both are true or both are false, the header
	 * are nicely aligned to 32-bit. So, no L2 padding.
	 */
	if (IEEE80211_HAS_ADDR4(wh) == IEEE80211_QOS_HAS_SEQ(wh))
		pad = 0;
	else
		pad = 2;

	/* setup TX Wireless Information */
	txwi = (struct mtw_txwi *)(txd + 1);
	txwi->len = htole16(m->m_pkthdr.len - pad);
	if (rt2860_rates[ridx].phy == IEEE80211_T_DS) {
		mcs |=	MTW_PHY_CCK;
		if (ridx != MTW_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			mcs |=	MTW_PHY_SHPRE;
	} else if (rt2860_rates[ridx].phy == IEEE80211_T_OFDM) {
		mcs |=	MTW_PHY_OFDM;
	} else if (rt2860_rates[ridx].phy == IEEE80211_T_HT) {
		/* XXX TODO: [adrian] set short preamble for MCS? */
		mcs |=	MTW_PHY_HT; /* Mixed, not greenfield */
	}
	txwi->phy = htole16(mcs);

	/* check if RTS/CTS or CTS-to-self protection is required */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    ((m->m_pkthdr.len + IEEE80211_CRC_LEN > vap->iv_rtsthreshold) ||
	     ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	      rt2860_rates[ridx].phy == IEEE80211_T_OFDM) ||
	     ((ic->ic_htprotmode == IEEE80211_PROT_RTSCTS) &&
	      rt2860_rates[ridx].phy == IEEE80211_T_HT)))
		txwi->txop |= MTW_TX_TXOP_HT;
	else
		txwi->txop |=	MTW_TX_TXOP_BACKOFF;

}

/* This function must be called locked */
static int
mtw_tx(struct mtw_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_frame *wh;


	//const struct ieee80211_txparam *tp = ni->ni_txparms;
	struct mtw_node *rn = MTW_NODE(ni);
	struct mtw_tx_data *data;
	struct mtw_txd *txd;
	struct mtw_txwi *txwi;
	uint16_t qos;
	uint16_t dur;
	uint16_t qid;
	uint8_t type;
	uint8_t tid;
	uint16_t ridx;
	uint8_t ctl_ridx;
	uint16_t qflags;
	uint8_t xflags = 0;

	int hasqos;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	wh = mtod(m, struct ieee80211_frame *);
	const struct ieee80211_txparam *tp = ni->ni_txparms;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	qflags = htole16(MTW_TXD_DATA | MTW_TXD_80211 |
			 MTW_TXD_WLAN | MTW_TXD_QSEL_HCCA);

	if ((hasqos = IEEE80211_QOS_HAS_SEQ(wh))) {
		uint8_t *frm;
		frm = ieee80211_getqos(wh);


		//device_printf(sc->sc_dev,"JSS:frm:%d",*frm);
		qos = le16toh(*(const uint16_t *)frm);
		tid = ieee80211_gettid(wh);
		qid = TID_TO_WME_AC(tid);
		qflags |= MTW_TXD_QSEL_EDCA;
	} else {
		qos = 0;
		tid = 0;
		qid = WME_AC_BE;
	}
	if (type & IEEE80211_FC0_TYPE_MGT) {
		qid = 0;
	}

	if (type != IEEE80211_FC0_TYPE_DATA)
		qflags |= htole16(MTW_TXD_WIV);

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA || m->m_flags & M_EAPOL) {
		/* XXX TODO: methodize for 11n; use MCS0 for 11NA/11NG */
		ridx = (ic->ic_curmode == IEEE80211_MODE_11A
			|| ic->ic_curmode == IEEE80211_MODE_11NA) ?
			MTW_RIDX_OFDM6 : MTW_RIDX_CCK1;
		if (type == IEEE80211_MODE_11NG) {
			ridx = 12;
		}
		ctl_ridx = rt2860_rates[ridx].ctl_ridx;
	} else {
		if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
			ridx = rn->fix_ridx;

		} else {
			ridx = rn->amrr_ridx;
			ctl_ridx = rt2860_rates[ridx].ctl_ridx;
		}
	}

	if (hasqos)
		xflags = 0;
	else
		xflags = MTW_TX_NSEQ;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (!hasqos ||
		(qos & IEEE80211_QOS_ACKPOLICY) !=
		    IEEE80211_QOS_ACKPOLICY_NOACK)) {
		xflags |= MTW_TX_ACK;
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			dur = rt2860_rates[ctl_ridx].sp_ack_dur;
		else
			dur = rt2860_rates[ctl_ridx].lp_ack_dur;
		USETW(wh->i_dur, dur);
	}
	/* reserve slots for mgmt packets, just in case */
	if (sc->sc_epq[qid].tx_nfree < 3) {
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "tx ring %d is full\n", qid);
		return (-1);
	}

	data = STAILQ_FIRST(&sc->sc_epq[qid].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[qid].tx_fh, next);
	sc->sc_epq[qid].tx_nfree--;

	txd = (struct mtw_txd *)&data->desc;
	txd->flags = qflags;

	txwi = (struct mtw_txwi *)(txd + 1);
	txwi->xflags = xflags;
	txwi->wcid = (type == IEEE80211_FC0_TYPE_DATA) ?

	   MTW_AID2WCID(ni->ni_associd) :
	    0xff;

	/* clear leftover garbage bits */
	txwi->flags = 0;
	txwi->txop = 0;

	data->m = m;
	data->ni = ni;
	data->ridx = ridx;

	mtw_set_tx_desc(sc, data);

	/*
	 * The chip keeps track of 2 kind of Tx stats,
	 *  * TX_STAT_FIFO, for per WCID stats, and
	 *  * TX_STA_CNT0 for all-TX-in-one stats.
	 *
	 * To use FIFO stats, we need to store MCS into the driver-private
	 * PacketID field. So that, we can tell whose stats when we read them.
	 * We add 1 to the MCS because setting the PacketID field to 0 means
	 * that we don't want feedback in TX_STAT_FIFO.
	 * And, that's what we want for STA mode, since TX_STA_CNT0 does the
	 * job.
	 *
	 * FIFO stats doesn't count Tx with WCID 0xff, so we do this in
	 * run_tx().
	 */

	if (sc->rvp_cnt > 1 || vap->iv_opmode == IEEE80211_M_HOSTAP ||
	    vap->iv_opmode == IEEE80211_M_MBSS) {

		/*
		 * Unlike PCI based devices, we don't get any interrupt from
		 * USB devices, so we simulate FIFO-is-full interrupt here.
		 * Ralink recommends to drain FIFO stats every 100 ms, but 16
		 * slots quickly get fulled. To prevent overflow, increment a
		 * counter on every FIFO stat request, so we know how many slots
		 * are left. We do this only in HOSTAP or multiple vap mode
		 * since FIFO stats are used only in those modes. We just drain
		 * stats. AMRR gets updated every 1 sec by run_ratectl_cb() via
		 * callout. Call it early. Otherwise overflow.
		 */
		if (sc->fifo_cnt++ == 10) {
			/*
			 * With multiple vaps or if_bridge, if_start() is called
			 * with a non-sleepable lock, tcpinp. So, need to defer.
			 */
			uint32_t i = MTW_CMDQ_GET(&sc->cmdq_store);
			MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "cmdq_store=%d\n", i);
			sc->cmdq[i].func = mtw_drain_fifo;
			sc->cmdq[i].arg0 = sc;
			ieee80211_runtask(ic, &sc->cmdq_task);
		}
	}

	STAILQ_INSERT_TAIL(&sc->sc_epq[qid].tx_qh, data, next);
	usbd_transfer_start(sc->sc_xfer[mtw_wme_ac_xfer_map[qid]]);

	MTW_DPRINTF(sc, MTW_DEBUG_XMIT,
	    "sending data frame len=%d rate=%d qid=%d\n",
	    m->m_pkthdr.len +
		(int)(sizeof(struct mtw_txd) + sizeof(struct mtw_txwi)),
	    rt2860_rates[ridx].rate, qid);

	return (0);
	}

static int
mtw_tx_mgt(struct mtw_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct mtw_node *rn = MTW_NODE(ni);
	struct mtw_tx_data *data;
	struct ieee80211_frame *wh;
	struct mtw_txd *txd;
	struct mtw_txwi *txwi;
	uint8_t type;
	uint16_t dur;
	uint8_t ridx = rn->mgt_ridx;
	uint8_t xflags = 0;
	uint8_t wflags = 0;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	wh = mtod(m, struct ieee80211_frame *);

	/* tell hardware to add timestamp for probe responses  */
	if ((wh->i_fc[0] &
		(IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
		wflags |= MTW_TX_TS;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		xflags |= MTW_TX_ACK;

		dur = ieee80211_ack_duration(ic->ic_rt, rt2860_rates[ridx].rate,
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		USETW(wh->i_dur, dur);
	}
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	if (sc->sc_epq[0].tx_nfree == 0)
		/* let caller free mbuf */
		return (EIO);
	data = STAILQ_FIRST(&sc->sc_epq[0].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[0].tx_fh, next);
	sc->sc_epq[0].tx_nfree--;

	txd = (struct mtw_txd *)&data->desc;
	txd->flags = htole16(
	    MTW_TXD_DATA | MTW_TXD_80211 | MTW_TXD_WLAN | MTW_TXD_QSEL_EDCA);
	if (type != IEEE80211_FC0_TYPE_DATA)
		txd->flags |= htole16(MTW_TXD_WIV);

	txwi = (struct mtw_txwi *)(txd + 1);
	txwi->wcid = 0xff;
	txwi->xflags = xflags;
	txwi->flags = wflags;

	txwi->txop = 0; /* clear leftover garbage bits */

	data->m = m;
	data->ni = ni;
	data->ridx = ridx;

	MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "sending mgt frame len=%d rate=%d\n",
	    m->m_pkthdr.len +
		(int)(sizeof(struct mtw_txd) + sizeof(struct mtw_txwi)),
	    rt2860_rates[ridx].rate);

	STAILQ_INSERT_TAIL(&sc->sc_epq[0].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[MTW_BULK_TX_BE]);

	return (0);
}

static int
mtw_sendprot(struct mtw_softc *sc, const struct mbuf *m,
    struct ieee80211_node *ni, int prot, int rate)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mtw_tx_data *data;
	struct mtw_txd *txd;
	struct mtw_txwi *txwi;
	struct mbuf *mprot;
	int ridx;
	int protrate;
	uint8_t wflags = 0;
	uint8_t xflags = 0;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	/* check that there are free slots before allocating the mbuf */
	if (sc->sc_epq[0].tx_nfree == 0)
		/* let caller free mbuf */
		return (ENOBUFS);

	mprot = ieee80211_alloc_prot(ni, m, rate, prot);
	if (mprot == NULL) {
		if_inc_counter(ni->ni_vap->iv_ifp, IFCOUNTER_OERRORS, 1);
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "could not allocate mbuf\n");
		return (ENOBUFS);
	}

	protrate = ieee80211_ctl_rate(ic->ic_rt, rate);
	wflags = MTW_TX_FRAG;
	xflags = 0;
	if (prot == IEEE80211_PROT_RTSCTS)
		xflags |= MTW_TX_ACK;

	data = STAILQ_FIRST(&sc->sc_epq[0].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[0].tx_fh, next);
	sc->sc_epq[0].tx_nfree--;

	txd = (struct mtw_txd *)&data->desc;
	txd->flags = RT2860_TX_QSEL_EDCA;
	txwi = (struct mtw_txwi *)(txd + 1);
	txwi->wcid = 0xff;
	txwi->flags = wflags;
	txwi->xflags = xflags;
	txwi->txop = 0; /* clear leftover garbage bits */

	data->m = mprot;
	data->ni = ieee80211_ref_node(ni);

	/* XXX TODO: methodize with MCS rates */
	for (ridx = 0; ridx < MTW_RIDX_MAX; ridx++)
		if (rt2860_rates[ridx].rate == protrate)
			break;
	data->ridx = ridx;

	mtw_set_tx_desc(sc, data);
	MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "sending prot len=%u rate=%u\n",
	    m->m_pkthdr.len, rate);

	STAILQ_INSERT_TAIL(&sc->sc_epq[0].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[0]);

	return (0);
}

static int
mtw_tx_param(struct mtw_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mtw_tx_data *data;
	struct mtw_txd *txd;
	struct mtw_txwi *txwi;
	uint8_t ridx;
	uint8_t rate;
	uint8_t opflags = 0;
	uint8_t xflags = 0;
	int error;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	KASSERT(params != NULL, ("no raw xmit params"));

	rate = params->ibp_rate0;
	if (!ieee80211_isratevalid(ic->ic_rt, rate)) {
		/* let caller free mbuf */
		return (EINVAL);
	}

	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0)
		xflags |= MTW_TX_ACK;
	if (params->ibp_flags & (IEEE80211_BPF_RTS | IEEE80211_BPF_CTS)) {
		error = mtw_sendprot(sc, m, ni,
		    params->ibp_flags & IEEE80211_BPF_RTS ?
			IEEE80211_PROT_RTSCTS :
			IEEE80211_PROT_CTSONLY,
		    rate);
		if (error) {
			device_printf(sc->sc_dev, "%s:%d %d\n", __FILE__,
			    __LINE__, error);
			return (error);
		}
		opflags |=  MTW_TX_TXOP_SIFS;
	}

	if (sc->sc_epq[0].tx_nfree == 0) {
		/* let caller free mbuf */
		MTW_DPRINTF(sc, MTW_DEBUG_XMIT,
		    "sending raw frame, but tx ring is full\n");
		return (EIO);
	}
	data = STAILQ_FIRST(&sc->sc_epq[0].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[0].tx_fh, next);
	sc->sc_epq[0].tx_nfree--;

	txd = (struct mtw_txd *)&data->desc;
	txd->flags = htole16(
	    MTW_TXD_DATA | MTW_TXD_80211 | MTW_TXD_WLAN | MTW_TXD_QSEL_EDCA);
	// txd->flags = htole16(MTW_TXD_QSEL_EDCA);
	txwi = (struct mtw_txwi *)(txd + 1);
	txwi->wcid = 0xff;
	txwi->xflags = xflags;
	txwi->txop = opflags;
	txwi->flags = 0; /* clear leftover garbage bits */

	data->m = m;
	data->ni = ni;
	/* XXX TODO: methodize with MCS rates */
	for (ridx = 0; ridx < MTW_RIDX_MAX; ridx++)
		if (rt2860_rates[ridx].rate == rate)
			break;
	data->ridx = ridx;

	mtw_set_tx_desc(sc, data);

	MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "sending raw frame len=%u rate=%u\n",
	    m->m_pkthdr.len, rate);

	STAILQ_INSERT_TAIL(&sc->sc_epq[0].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[MTW_BULK_RAW_TX]);

	return (0);
}

static int
mtw_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct mtw_softc *sc = ni->ni_ic->ic_softc;
	int error = 0;
	MTW_LOCK(sc);
	/* prevent management frames from being sent if we're not ready */
	if (!(sc->sc_flags & MTW_RUNNING)) {
		error = ENETDOWN;
		goto done;
	}

	if (params == NULL) {
		/* tx mgt packet */
		if ((error = mtw_tx_mgt(sc, m, ni)) != 0) {
			MTW_DPRINTF(sc, MTW_DEBUG_XMIT, "mgt tx failed\n");
			goto done;
		}
	} else {
		/* tx raw packet with param */
		if ((error = mtw_tx_param(sc, m, ni, params)) != 0) {
			MTW_DPRINTF(sc, MTW_DEBUG_XMIT,
			    "tx with param failed\n");
			goto done;
		}
	}

done:

	MTW_UNLOCK(sc);

	if (error != 0) {
		if (m != NULL)
			m_freem(m);
	}

	return (error);
}

static int
mtw_transmit(struct ieee80211com *ic, struct mbuf *m)
{
	struct mtw_softc *sc = ic->ic_softc;
	int error;
	MTW_LOCK(sc);
	if ((sc->sc_flags & MTW_RUNNING) == 0) {
		MTW_UNLOCK(sc);
		return (ENXIO);
	}
	error = mbufq_enqueue(&sc->sc_snd, m);
	if (error) {
		MTW_UNLOCK(sc);
		return (error);
	}
	mtw_start(sc);
	MTW_UNLOCK(sc);

	return (0);
}

static void
mtw_start(struct mtw_softc *sc)
{
	struct ieee80211_node *ni;
	struct mbuf *m;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	if ((sc->sc_flags & MTW_RUNNING) == 0) {

		return;
	}
	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (mtw_tx(sc, m, ni) != 0) {
			mbufq_prepend(&sc->sc_snd, m);
			break;
		}
	}
}

static void
mtw_parent(struct ieee80211com *ic)
{

	struct mtw_softc *sc = ic->ic_softc;

	MTW_LOCK(sc);
	if (sc->sc_detached) {
		MTW_UNLOCK(sc);
		return;
	}

	if (!(sc->sc_flags & MTW_RUNNING) && ic->ic_nrunning > 0) {
		mtw_init_locked(sc);
		MTW_UNLOCK(sc);
		ieee80211_start_all(ic);
		return;
	}
	if (!(sc->sc_flags & MTW_RUNNING) && ic->ic_nrunning > 0) {
		mtw_update_promisc_locked(sc);
		MTW_UNLOCK(sc);
		return;
	}
	if ((sc->sc_flags & MTW_RUNNING) && sc->rvp_cnt <= 1 &&
	    ic->ic_nrunning == 0) {
		mtw_stop(sc);
		MTW_UNLOCK(sc);
		return;
	}
	return;
}

static void
mt7601_set_agc(struct mtw_softc *sc, uint8_t agc)
{
	uint8_t bbp;

	mtw_bbp_write(sc, 66, agc);
	mtw_bbp_write(sc, 195, 0x87);
	bbp = (agc & 0xf0) | 0x08;
	mtw_bbp_write(sc, 196, bbp);
}

static int
mtw_mcu_calibrate(struct mtw_softc *sc, int func, uint32_t val)
{
	struct mtw_mcu_cmd_8 cmd;

	cmd.func = htole32(func);
	cmd.val = htole32(val);
	return (mtw_mcu_cmd(sc, 31, &cmd, sizeof(struct mtw_mcu_cmd_8)));
}

static int
mtw_rf_write(struct mtw_softc *sc, uint8_t bank, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int error, ntries, shift;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = mtw_read(sc, MTW_RF_CSR, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_RF_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	if (sc->asic_ver == 0x7601)
		shift = MT7601_BANK_SHIFT;
	else
		shift = MT7610_BANK_SHIFT;

	tmp = MTW_RF_CSR_WRITE | MTW_RF_CSR_KICK | (bank & 0xf) << shift |
	    reg << 8 | val;
	return (mtw_write(sc, MTW_RF_CSR, tmp));
}

void
mtw_select_chan_group(struct mtw_softc *sc, int group)
{
	uint32_t tmp;
	uint8_t bbp;

	/* Tx band 20MHz 2G */
	mtw_read(sc, MTW_TX_BAND_CFG, &tmp);
	tmp &= ~(
	    MTW_TX_BAND_SEL_2G | MTW_TX_BAND_SEL_5G | MTW_TX_BAND_UPPER_40M);
	tmp |= (group == 0) ? MTW_TX_BAND_SEL_2G : MTW_TX_BAND_SEL_5G;
	mtw_write(sc, MTW_TX_BAND_CFG, tmp);

	/* select 20 MHz bandwidth */
	mtw_bbp_read(sc, 4, &bbp);
	bbp &= ~0x18;
	bbp |= 0x40;
	mtw_bbp_write(sc, 4, bbp);

	/* calibrate BBP */
	mtw_bbp_write(sc, 69, 0x12);
	mtw_bbp_write(sc, 91, 0x07);
	mtw_bbp_write(sc, 195, 0x23);
	mtw_bbp_write(sc, 196, 0x17);
	mtw_bbp_write(sc, 195, 0x24);
	mtw_bbp_write(sc, 196, 0x06);
	mtw_bbp_write(sc, 195, 0x81);
	mtw_bbp_write(sc, 196, 0x12);
	mtw_bbp_write(sc, 195, 0x83);
	mtw_bbp_write(sc, 196, 0x17);
	mtw_rf_write(sc, 5, 8, 0x00);
	// mtw_mcu_calibrate(sc, 0x6, 0x10001);

	/* set initial AGC value */
	mt7601_set_agc(sc, 0x14);
}

static int
mtw_rf_read(struct mtw_softc *sc, uint8_t bank, uint8_t reg, uint8_t *val)
{
	uint32_t tmp;
	int error, ntries, shift;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = mtw_read(sc, MTW_RF_CSR, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_RF_CSR_KICK))
			break;
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	if (sc->asic_ver == 0x7601)
		shift = MT7601_BANK_SHIFT;
	else
		shift = MT7610_BANK_SHIFT;

	tmp = MTW_RF_CSR_KICK | (bank & 0xf) << shift | reg << 8;
	if ((error = mtw_write(sc, MTW_RF_CSR, tmp)) != 0)
		return (error);

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = mtw_read(sc, MTW_RF_CSR, &tmp)) != 0)
			return (error);
		if (!(tmp & MTW_RF_CSR_KICK))
			break;
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	*val = tmp & 0xff;
	return (0);
}
static void
mt7601_set_chan(struct mtw_softc *sc, u_int chan)
{
	uint32_t tmp;
	uint8_t bbp, rf, txpow1;
	int i;
	/* find the settings for this channel */
	for (i = 0; mt7601_rf_chan[i].chan != chan; i++)
		;

	mtw_rf_write(sc, 0, 17, mt7601_rf_chan[i].r17);
	mtw_rf_write(sc, 0, 18, mt7601_rf_chan[i].r18);
	mtw_rf_write(sc, 0, 19, mt7601_rf_chan[i].r19);
	mtw_rf_write(sc, 0, 20, mt7601_rf_chan[i].r20);

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];

	/* Tx automatic level control */
	mtw_read(sc, MTW_TX_ALC_CFG0, &tmp);
	tmp &= ~0x3f3f;
	tmp |= (txpow1 & 0x3f);
	mtw_write(sc, MTW_TX_ALC_CFG0, tmp);

	/* LNA */
	mtw_bbp_write(sc, 62, 0x37 - sc->lna[0]);
	mtw_bbp_write(sc, 63, 0x37 - sc->lna[0]);
	mtw_bbp_write(sc, 64, 0x37 - sc->lna[0]);

	/* VCO calibration */
	mtw_rf_write(sc, 0, 4, 0x0a);
	mtw_rf_write(sc, 0, 5, 0x20);
	mtw_rf_read(sc, 0, 4, &rf);
	mtw_rf_write(sc, 0, 4, rf | 0x80);

	/* select 20 MHz bandwidth */
	mtw_bbp_read(sc, 4, &bbp);
	bbp &= ~0x18;
	bbp |= 0x40;
	mtw_bbp_write(sc, 4, bbp);
	mtw_bbp_write(sc, 178, 0xff);
}

static int
mtw_set_chan(struct mtw_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan, group;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return (EINVAL);

	/* determine channel group */
	if (chan <= 14)
		group = 0;
	else if (chan <= 64)
		group = 1;
	else if (chan <= 128)
		group = 2;
	else
		group = 3;

	if (group != sc->sc_chan_group || !sc->sc_bw_calibrated)
		mtw_select_chan_group(sc, group);

	sc->sc_chan_group = group;

	/* chipset specific */
	if (sc->asic_ver == 0x7601)
		mt7601_set_chan(sc, chan);

	DELAY(1000);
	return (0);
}

static void
mtw_set_channel(struct ieee80211com *ic)
{
	struct mtw_softc *sc = ic->ic_softc;

	MTW_LOCK(sc);
	mtw_set_chan(sc, ic->ic_curchan);
	MTW_UNLOCK(sc);

	return;
}

static void
mtw_getradiocaps(struct ieee80211com *ic, int maxchans, int *nchans,
    struct ieee80211_channel chans[])
{
	// struct mtw_softc *sc = ic->ic_softc;
	uint8_t bands[IEEE80211_MODE_BYTES];

	memset(bands, 0, sizeof(bands));
	setbit(bands, IEEE80211_MODE_11B);
	setbit(bands, IEEE80211_MODE_11G);
	setbit(bands, IEEE80211_MODE_11NG);

	/* Note: for now, only support HT20 channels */
	ieee80211_add_channels_default_2ghz(chans, maxchans, nchans, bands, 0);
}

static void
mtw_scan_start(struct ieee80211com *ic)
{
	struct mtw_softc *sc = ic->ic_softc;
	MTW_LOCK(sc);
	/* abort TSF synchronization */
	mtw_abort_tsf_sync(sc);
	mtw_set_bssid(sc, ieee80211broadcastaddr);

	MTW_UNLOCK(sc);

	return;
}

static void
mtw_scan_end(struct ieee80211com *ic)
{
	struct mtw_softc *sc = ic->ic_softc;

	MTW_LOCK(sc);

	mtw_enable_tsf_sync(sc);
	mtw_set_bssid(sc, sc->sc_bssid);

	MTW_UNLOCK(sc);

	return;
}

/*
 * Could be called from ieee80211_node_timeout()
 * (non-sleepable thread)
 */
static void
mtw_update_beacon(struct ieee80211vap *vap, int item)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_beacon_offsets *bo = &vap->iv_bcn_off;
	struct ieee80211_node *ni = vap->iv_bss;
	struct mtw_softc *sc = ic->ic_softc;
	struct mtw_vap *rvp = MTW_VAP(vap);
	int mcast = 0;
	uint32_t i;

	switch (item) {
	case IEEE80211_BEACON_ERP:
		mtw_updateslot(ic);
		break;
	case IEEE80211_BEACON_HTINFO:
		mtw_updateprot(ic);
		break;
	case IEEE80211_BEACON_TIM:
		mcast = 1; /*TODO*/
		break;
	default:
		break;
	}

	setbit(bo->bo_flags, item);
	if (rvp->beacon_mbuf == NULL) {
		rvp->beacon_mbuf = ieee80211_beacon_alloc(ni);
		if (rvp->beacon_mbuf == NULL)
			return;
	}
	ieee80211_beacon_update(ni, rvp->beacon_mbuf, mcast);

	i = MTW_CMDQ_GET(&sc->cmdq_store);
	MTW_DPRINTF(sc, MTW_DEBUG_BEACON, "cmdq_store=%d\n", i);
	sc->cmdq[i].func = mtw_update_beacon_cb;
	sc->cmdq[i].arg0 = vap;
	ieee80211_runtask(ic, &sc->cmdq_task);

	return;
}

static void
mtw_update_beacon_cb(void *arg)
{

	struct ieee80211vap *vap = arg;
	struct ieee80211_node *ni = vap->iv_bss;
	struct mtw_vap *rvp = MTW_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct mtw_softc *sc = ic->ic_softc;
	struct mtw_txwi txwi;
	struct mbuf *m;
	uint16_t txwisize;
	uint8_t ridx;
	if (ni->ni_chan == IEEE80211_CHAN_ANYC)
		return;
	if (ic->ic_bsschan == IEEE80211_CHAN_ANYC)
		return;

	/*
	 * No need to call ieee80211_beacon_update(), mtw_update_beacon()
	 * is taking care of appropriate calls.
	 */
	if (rvp->beacon_mbuf == NULL) {
		rvp->beacon_mbuf = ieee80211_beacon_alloc(ni);
		if (rvp->beacon_mbuf == NULL)
			return;
	}
	m = rvp->beacon_mbuf;

	memset(&txwi, 0, sizeof(txwi));
	txwi.wcid = 0xff;
	txwi.len = htole16(m->m_pkthdr.len);

	/* send beacons at the lowest available rate */
	ridx = (ic->ic_curmode == IEEE80211_MODE_11A) ? MTW_RIDX_OFDM6 :
							MTW_RIDX_CCK1;
	txwi.phy = htole16(rt2860_rates[ridx].mcs);
	if (rt2860_rates[ridx].phy == IEEE80211_T_OFDM)
		txwi.phy |= htole16(MTW_PHY_OFDM);
	txwi.txop = MTW_TX_TXOP_HT;
	txwi.flags = MTW_TX_TS;
	txwi.xflags = MTW_TX_NSEQ;

	txwisize =  sizeof(txwi);
	mtw_write_region_1(sc, MTW_BCN_BASE, (uint8_t *)&txwi, txwisize);
	mtw_write_region_1(sc, MTW_BCN_BASE + txwisize, mtod(m, uint8_t *),
	    (m->m_pkthdr.len + 1) & ~1);
}

static void
mtw_updateprot(struct ieee80211com *ic)
{
	struct mtw_softc *sc = ic->ic_softc;
	uint32_t i;

	i = MTW_CMDQ_GET(&sc->cmdq_store);
	MTW_DPRINTF(sc, MTW_DEBUG_BEACON, "test cmdq_store=%d\n", i);
	sc->cmdq[i].func = mtw_updateprot_cb;
	sc->cmdq[i].arg0 = ic;
	ieee80211_runtask(ic, &sc->cmdq_task);
}

static void
mtw_updateprot_cb(void *arg)
{

	struct ieee80211com *ic = arg;
	struct mtw_softc *sc = ic->ic_softc;
	uint32_t tmp;

	tmp = RT2860_RTSTH_EN | RT2860_PROT_NAV_SHORT | RT2860_TXOP_ALLOW_ALL;
	/* setup protection frame rate (MCS code) */
	tmp |= (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    rt2860_rates[MTW_RIDX_OFDM6].mcs | MTW_PHY_OFDM :
	    rt2860_rates[MTW_RIDX_CCK11].mcs;

	/* CCK frames don't require protection */
	mtw_write(sc, MTW_CCK_PROT_CFG, tmp);
	if (ic->ic_flags & IEEE80211_F_USEPROT) {
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			tmp |= RT2860_PROT_CTRL_RTS_CTS;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			tmp |= RT2860_PROT_CTRL_CTS;
	}
	mtw_write(sc, MTW_OFDM_PROT_CFG, tmp);
}

static void
mtw_usb_timeout_cb(void *arg)
{
	struct ieee80211vap *vap = arg;
	struct mtw_softc *sc = vap->iv_ic->ic_softc;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	if (vap->iv_state == IEEE80211_S_SCAN) {
		MTW_DPRINTF(sc, MTW_DEBUG_USB | MTW_DEBUG_STATE,
		    "timeout caused by scan\n");
		/* cancel bgscan */
		ieee80211_cancel_scan(vap);
	} else {
		MTW_DPRINTF(sc, MTW_DEBUG_USB | MTW_DEBUG_STATE,
		    "timeout by unknown cause\n");
	}
}
static int mtw_reset(struct mtw_softc *sc)
{

	usb_device_request_t req;
	uint16_t tmp;
	uint16_t actlen;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MTW_RESET;
	USETW(req.wValue, 1);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx,
				 &req, &tmp, 0, &actlen, 1000));

}


static void
mtw_update_promisc_locked(struct mtw_softc *sc)
{

	uint32_t tmp;

	mtw_read(sc, MTW_RX_FILTR_CFG, &tmp);

	tmp |= MTW_DROP_UC_NOME;
	if (sc->sc_ic.ic_promisc > 0)
		tmp &= ~MTW_DROP_UC_NOME;

	mtw_write(sc, MTW_RX_FILTR_CFG, tmp);

	MTW_DPRINTF(sc, MTW_DEBUG_RECV, "%s promiscuous mode\n",
	    (sc->sc_ic.ic_promisc > 0) ? "entering" : "leaving");
}

static void
mtw_update_promisc(struct ieee80211com *ic)
{
	struct mtw_softc *sc = ic->ic_softc;

	if ((sc->sc_flags & MTW_RUNNING) == 0)
		return;

	MTW_LOCK(sc);
	mtw_update_promisc_locked(sc);
	MTW_UNLOCK(sc);
}

static void
mtw_enable_tsf_sync(struct mtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t tmp;
	int error;
	mtw_read(sc, MTW_BCN_TIME_CFG, &tmp);
	tmp &= ~0x1fffff;
	tmp |= vap->iv_bss->ni_intval * 16;
	tmp |= MTW_TSF_TIMER_EN | MTW_TBTT_TIMER_EN;

	/* local TSF is always updated with remote TSF on beacon reception */
	tmp |= 1 << MTW_TSF_SYNC_MODE_SHIFT;
	error = mtw_write(sc, MTW_BCN_TIME_CFG, tmp);
	if (error != 0) {
		device_printf(sc->sc_dev, "enable_tsf_sync failed error:%d\n",
		    error);
	}
	return;
}

static void
mtw_enable_mrr(struct mtw_softc *sc)
{
#define CCK(mcs) (mcs)

#define OFDM(mcs) (1 << 3 | (mcs))
	mtw_write(sc, MTW_LG_FBK_CFG0,
	    OFDM(6) << 28 |	/* 54->48 */
		OFDM(5) << 24 | /* 48->36 */
		OFDM(4) << 20 | /* 36->24 */
		OFDM(3) << 16 | /* 24->18 */
		OFDM(2) << 12 | /* 18->12 */
		OFDM(1) << 8 |	/* 12-> 9 */
		OFDM(0) << 4 |	/*  9-> 6 */
		OFDM(0));	/*  6-> 6 */

	mtw_write(sc, MTW_LG_FBK_CFG1,
	    CCK(2) << 12 |    /* 11->5.5 */
		CCK(1) << 8 | /* 5.5-> 2 */
		CCK(0) << 4 | /*   2-> 1 */
		CCK(0));      /*   1-> 1 */
#undef OFDM
#undef CCK
}

static void
mtw_set_txpreamble(struct mtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	mtw_read(sc, MTW_AUTO_RSP_CFG, &tmp);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= MTW_CCK_SHORT_EN;
	else
		tmp &= ~MTW_CCK_SHORT_EN;
	mtw_write(sc, MTW_AUTO_RSP_CFG, tmp);
}

static void
mtw_set_basicrates(struct mtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* set basic rates mask */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mtw_write(sc, MTW_LEGACY_BASIC_RATE, 0x003);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		mtw_write(sc, MTW_LEGACY_BASIC_RATE, 0x150);
	else /* 11g */
		mtw_write(sc, MTW_LEGACY_BASIC_RATE, 0x17f);
}

static void
mtw_set_bssid(struct mtw_softc *sc, const uint8_t *bssid)
{
	mtw_write(sc, MTW_MAC_BSSID_DW0,
	    bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24);
	mtw_write(sc, MTW_MAC_BSSID_DW1, bssid[4] | bssid[5] << 8);
}

static void
mtw_set_macaddr(struct mtw_softc *sc, const uint8_t *addr)
{
	mtw_write(sc, MTW_MAC_ADDR_DW0,
	    addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24);
	mtw_write(sc, MTW_MAC_ADDR_DW1, addr[4] | addr[5] << 8 | 0xff << 16);
}

static void
mtw_updateslot(struct ieee80211com *ic)
{

	struct mtw_softc *sc = ic->ic_softc;
	uint32_t i;

	i = MTW_CMDQ_GET(&sc->cmdq_store);
	MTW_DPRINTF(sc, MTW_DEBUG_BEACON, "cmdq_store=%d\n", i);
	sc->cmdq[i].func = mtw_updateslot_cb;
	sc->cmdq[i].arg0 = ic;
	ieee80211_runtask(ic, &sc->cmdq_task);

	return;
}

/* ARGSUSED */
static void
mtw_updateslot_cb(void *arg)
{
  struct ieee80211com *ic = arg;
	struct mtw_softc *sc = ic->ic_softc;
	uint32_t tmp;
	mtw_read(sc, MTW_BKOFF_SLOT_CFG, &tmp);
	tmp &= ~0xff;
	tmp |= IEEE80211_GET_SLOTTIME(ic);
	mtw_write(sc, MTW_BKOFF_SLOT_CFG, tmp);
}

static void
mtw_update_mcast(struct ieee80211com *ic)
{
}

static int8_t
mtw_rssi2dbm(struct mtw_softc *sc, uint8_t rssi, uint8_t rxchain)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c = ic->ic_curchan;
	int delta;

	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		u_int chan = ieee80211_chan2ieee(ic, c);
		delta = sc->rssi_5ghz[rxchain];

		/* determine channel group */
		if (chan <= 64)
			delta -= sc->lna[1];
		else if (chan <= 128)
			delta -= sc->lna[2];
		else
			delta -= sc->lna[3];
	} else
		delta = sc->rssi_2ghz[rxchain] - sc->lna[0];

	return (-12 - delta - rssi);
}
static int
mt7601_bbp_init(struct mtw_softc *sc)
{
	uint8_t bbp;
	int i, error, ntries;

	/* wait for BBP to wake up */
	for (ntries = 0; ntries < 20; ntries++) {
		if ((error = mtw_bbp_read(sc, 0, &bbp)) != 0)
			return (error);
		if (bbp != 0 && bbp != 0xff)
			break;
	}

	if (ntries == 20)
		return (ETIMEDOUT);

	mtw_bbp_read(sc, 3, &bbp);
	mtw_bbp_write(sc, 3, 0);
	mtw_bbp_read(sc, 105, &bbp);
	mtw_bbp_write(sc, 105, 0);

	/* initialize BBP registers to default values */
	for (i = 0; i < nitems(mt7601_def_bbp); i++) {
		if ((error = mtw_bbp_write(sc, mt7601_def_bbp[i].reg,
			 mt7601_def_bbp[i].val)) != 0)
			return (error);
	}

	sc->sc_bw_calibrated = 0;

	return (0);
}

static int
mt7601_rf_init(struct mtw_softc *sc)
{
	int i, error;

	/* RF bank 0 */
	for (i = 0; i < nitems(mt7601_rf_bank0); i++) {
		error = mtw_rf_write(sc, 0, mt7601_rf_bank0[i].reg,
		    mt7601_rf_bank0[i].val);
		if (error != 0)
			return (error);
	}
	/* RF bank 4 */
	for (i = 0; i < nitems(mt7601_rf_bank4); i++) {
		error = mtw_rf_write(sc, 4, mt7601_rf_bank4[i].reg,
		    mt7601_rf_bank4[i].val);
		if (error != 0)
			return (error);
	}
	/* RF bank 5 */
	for (i = 0; i < nitems(mt7601_rf_bank5); i++) {
		error = mtw_rf_write(sc, 5, mt7601_rf_bank5[i].reg,
		    mt7601_rf_bank5[i].val);
		if (error != 0)
			return (error);
	}
	return (0);
}

static int
mtw_txrx_enable(struct mtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int error, ntries;
	mtw_write(sc, MTW_MAC_SYS_CTRL, MTW_MAC_TX_EN);
	for (ntries = 0; ntries < 200; ntries++) {
		if ((error = mtw_read(sc, MTW_WPDMA_GLO_CFG, &tmp)) != 0) {
			return (error);
		}
		if ((tmp & (MTW_TX_DMA_BUSY | MTW_RX_DMA_BUSY)) == 0)
			break;
		mtw_delay(sc, 50);
	}
	if (ntries == 200) {
		return (ETIMEDOUT);
	}

	DELAY(50);

	tmp |= MTW_RX_DMA_EN | MTW_TX_DMA_EN | MTW_TX_WB_DDONE;
	mtw_write(sc, MTW_WPDMA_GLO_CFG, tmp);

	/* enable Rx bulk aggregation (set timeout and limit) */
	tmp = MTW_USB_TX_EN | MTW_USB_RX_EN | MTW_USB_RX_AGG_EN |
	    MTW_USB_RX_AGG_TO(128) | MTW_USB_RX_AGG_LMT(2);
	mtw_write(sc, MTW_USB_DMA_CFG, tmp);

	/* set Rx filter */
	tmp = MTW_DROP_CRC_ERR | MTW_DROP_PHY_ERR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= MTW_DROP_UC_NOME | MTW_DROP_DUPL | MTW_DROP_CTS |
		    MTW_DROP_BA | MTW_DROP_ACK | MTW_DROP_VER_ERR |
		    MTW_DROP_CTRL_RSV | MTW_DROP_CFACK | MTW_DROP_CFEND;
		if (ic->ic_opmode == IEEE80211_M_STA)
			tmp |= MTW_DROP_RTS | MTW_DROP_PSPOLL;
	}
	mtw_write(sc, MTW_RX_FILTR_CFG, tmp);

	mtw_write(sc, MTW_MAC_SYS_CTRL, MTW_MAC_RX_EN | MTW_MAC_TX_EN);
	return (0);
}
static int
mt7601_rxdc_cal(struct mtw_softc *sc)
{
	uint32_t tmp;
	uint8_t bbp;
	int ntries;

	mtw_read(sc, MTW_MAC_SYS_CTRL, &tmp);
	mtw_write(sc, MTW_MAC_SYS_CTRL, MTW_MAC_RX_EN);
	mtw_bbp_write(sc, 158, 0x8d);
	mtw_bbp_write(sc, 159, 0xfc);
	mtw_bbp_write(sc, 158, 0x8c);
	mtw_bbp_write(sc, 159, 0x4c);

	for (ntries = 0; ntries < 20; ntries++) {
		DELAY(300);
		mtw_bbp_write(sc, 158, 0x8c);
		mtw_bbp_read(sc, 159, &bbp);
		if (bbp == 0x0c)
			break;
	}

	if (ntries == 20)
		return (ETIMEDOUT);

	mtw_write(sc, MTW_MAC_SYS_CTRL, 0);
	mtw_bbp_write(sc, 158, 0x8d);
	mtw_bbp_write(sc, 159, 0xe0);
	mtw_write(sc, MTW_MAC_SYS_CTRL, tmp);
	return (0);
}

static int
mt7601_r49_read(struct mtw_softc *sc, uint8_t flag, int8_t *val)
{
	uint8_t bbp;

	mtw_bbp_read(sc, 47, &bbp);
	bbp = 0x90;
	mtw_bbp_write(sc, 47, bbp);
	bbp &= ~0x0f;
	bbp |= flag;
	mtw_bbp_write(sc, 47, bbp);
	return (mtw_bbp_read(sc, 49, val));
}

static int
mt7601_rf_temperature(struct mtw_softc *sc, int8_t *val)
{
	uint32_t rfb, rfs;
	uint8_t bbp;
	int ntries;

	mtw_read(sc, MTW_RF_BYPASS0, &rfb);
	mtw_read(sc, MTW_RF_SETTING0, &rfs);
	mtw_write(sc, MTW_RF_BYPASS0, 0);
	mtw_write(sc, MTW_RF_SETTING0, 0x10);
	mtw_write(sc, MTW_RF_BYPASS0, 0x10);

	mtw_bbp_read(sc, 47, &bbp);
	bbp &= ~0x7f;
	bbp |= 0x10;
	mtw_bbp_write(sc, 47, bbp);

	mtw_bbp_write(sc, 22, 0x40);

	for (ntries = 0; ntries < 10; ntries++) {
		mtw_bbp_read(sc, 47, &bbp);
		if ((bbp & 0x10) == 0)
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	mt7601_r49_read(sc, MT7601_R47_TEMP, val);

	mtw_bbp_write(sc, 22, 0);

	mtw_bbp_read(sc, 21, &bbp);
	bbp |= 0x02;
	mtw_bbp_write(sc, 21, bbp);
	bbp &= ~0x02;
	mtw_bbp_write(sc, 21, bbp);

	mtw_write(sc, MTW_RF_BYPASS0, 0);
	mtw_write(sc, MTW_RF_SETTING0, rfs);
	mtw_write(sc, MTW_RF_BYPASS0, rfb);
	return (0);
}

static int
mt7601_rf_setup(struct mtw_softc *sc)
{
	uint32_t tmp;
	uint8_t rf;
	int error;

	if (sc->sc_rf_calibrated)
		return (0);

	/* init RF registers */
	if ((error = mt7601_rf_init(sc)) != 0)
		return (error);

	/* init frequency offset */
	mtw_rf_write(sc, 0, 12, sc->rf_freq_offset);
	mtw_rf_read(sc, 0, 12, &rf);

	/* read temperature */
	mt7601_rf_temperature(sc, &rf);
	sc->bbp_temp = rf;
	device_printf(sc->sc_dev, "BBP temp 0x%x\n", rf);

	mtw_rf_read(sc, 0, 7, &rf);
	if ((error = mtw_mcu_calibrate(sc, 0x1, 0)) != 0)
		return (error);
	mtw_delay(sc, 100);
	mtw_rf_read(sc, 0, 7, &rf);

	/* Calibrate VCO RF 0/4 */
	mtw_rf_write(sc, 0, 4, 0x0a);
	mtw_rf_write(sc, 0, 4, 0x20);
	mtw_rf_read(sc, 0, 4, &rf);
	mtw_rf_write(sc, 0, 4, rf | 0x80);

	if ((error = mtw_mcu_calibrate(sc, 0x9, 0)) != 0)
		return (error);
	if ((error = mt7601_rxdc_cal(sc)) != 0)
		return (error);
	if ((error = mtw_mcu_calibrate(sc, 0x6, 1)) != 0)
		return (error);
	if ((error = mtw_mcu_calibrate(sc, 0x6, 0)) != 0)
		return (error);
	if ((error = mtw_mcu_calibrate(sc, 0x4, 0)) != 0)
		return (error);
	if ((error = mtw_mcu_calibrate(sc, 0x5, 0)) != 0)
		return (error);

	mtw_read(sc, MTW_LDO_CFG0, &tmp);
	tmp &= ~(1 << 4);
	tmp |= (1 << 2);
	mtw_write(sc, MTW_LDO_CFG0, tmp);

	if ((error = mtw_mcu_calibrate(sc, 0x8, 0)) != 0)
		return (error);
	if ((error = mt7601_rxdc_cal(sc)) != 0)
		return (error);

	sc->sc_rf_calibrated = 1;
	return (0);
}

static void
mtw_set_txrts(struct mtw_softc *sc)
{
	uint32_t tmp;

	/* set RTS threshold */
	mtw_read(sc, MTW_TX_RTS_CFG, &tmp);
	tmp &= ~0xffff00;
	tmp |= 0x1000 << MTW_RTS_THRES_SHIFT;
	mtw_write(sc, MTW_TX_RTS_CFG, tmp);
}
static int
mtw_mcu_radio(struct mtw_softc *sc, int func, uint32_t val)
{
	struct mtw_mcu_cmd_16 cmd;

	cmd.r1 = htole32(func);
	cmd.r2 = htole32(val);
	cmd.r3 = 0;
	cmd.r4 = 0;
	return (mtw_mcu_cmd(sc, 20, &cmd, sizeof(struct mtw_mcu_cmd_16)));
}
static void
mtw_init_locked(struct mtw_softc *sc)
{

	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t tmp;
	int i, error, ridx, ntries;
	if (ic->ic_nrunning > 1)
		return;
	mtw_stop(sc);

	for (i = 0; i != MTW_EP_QUEUES; i++)
		mtw_setup_tx_list(sc, &sc->sc_epq[i]);

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = mtw_read(sc, MTW_WPDMA_GLO_CFG, &tmp)) != 0)
			goto fail;
		if ((tmp & (MTW_TX_DMA_BUSY | MTW_RX_DMA_BUSY)) == 0)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for DMA engine\n");
		error = ETIMEDOUT;
		goto fail;
	}
	tmp &= 0xff0;
	tmp |= MTW_TX_WB_DDONE;
	mtw_write(sc, MTW_WPDMA_GLO_CFG, tmp);

	mtw_set_leds(sc, MTW_LED_MODE_ON);
	/* reset MAC and baseband */
	mtw_write(sc, MTW_MAC_SYS_CTRL, MTW_BBP_HRST | MTW_MAC_SRST);
	mtw_write(sc, MTW_USB_DMA_CFG, 0);
	mtw_write(sc, MTW_MAC_SYS_CTRL, 0);

	/* init MAC values */
	if (sc->asic_ver == 0x7601) {
		for (i = 0; i < nitems(mt7601_def_mac); i++)
			mtw_write(sc, mt7601_def_mac[i].reg,
			    mt7601_def_mac[i].val);
	}

	/* wait while MAC is busy */
	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = mtw_read(sc, MTW_MAC_STATUS_REG, &tmp)) != 0)
			goto fail;
		if (!(tmp & (MTW_RX_STATUS_BUSY | MTW_TX_STATUS_BUSY)))
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		error = ETIMEDOUT;
		goto fail;
	}

	/* set MAC address */

	mtw_set_macaddr(sc, vap ? vap->iv_myaddr : ic->ic_macaddr);

	/* clear WCID attribute table */
	mtw_set_region_4(sc, MTW_WCID_ATTR(0), 1, 8 * 32);

	mtw_write(sc, 0x1648, 0x00830083);
	mtw_read(sc, MTW_FCE_L2_STUFF, &tmp);
	tmp &= ~MTW_L2S_WR_MPDU_LEN_EN;
	mtw_write(sc, MTW_FCE_L2_STUFF, tmp);

	/* RTS config */
	mtw_set_txrts(sc);

	/* clear Host to MCU mailbox */
	mtw_write(sc, MTW_BBP_CSR, 0);
	mtw_write(sc, MTW_H2M_MAILBOX, 0);

	/* clear RX WCID search table */
	mtw_set_region_4(sc, MTW_WCID_ENTRY(0), 0xffffffff, 512);

	/* abort TSF synchronization */
	mtw_abort_tsf_sync(sc);

	mtw_read(sc, MTW_US_CYC_CNT, &tmp);
	tmp = (tmp & ~0xff);
	if (sc->asic_ver == 0x7601)
		tmp |= 0x1e;
	mtw_write(sc, MTW_US_CYC_CNT, tmp);

	/* clear shared key table */
	mtw_set_region_4(sc, MTW_SKEY(0, 0), 0, 8 * 32);

	/* clear IV/EIV table */
	mtw_set_region_4(sc, MTW_IVEIV(0), 0, 8 * 32);

	/* clear shared key mode */
	mtw_write(sc, MTW_SKEY_MODE_0_7, 0);
	mtw_write(sc, MTW_SKEY_MODE_8_15, 0);

	/* txop truncation */
	mtw_write(sc, MTW_TXOP_CTRL_CFG, 0x0000583f);

	/* init Tx power for all Tx rates */
	for (ridx = 0; ridx < 5; ridx++) {
		if (sc->txpow20mhz[ridx] == 0xffffffff)
			continue;
		mtw_write(sc, MTW_TX_PWR_CFG(ridx), sc->txpow20mhz[ridx]);
	}
	mtw_write(sc, MTW_TX_PWR_CFG7, 0);
	mtw_write(sc, MTW_TX_PWR_CFG9, 0);

	mtw_read(sc, MTW_CMB_CTRL, &tmp);
	tmp &= ~(1 << 18 | 1 << 14);
	mtw_write(sc, MTW_CMB_CTRL, tmp);

	/* clear USB DMA */
	mtw_write(sc, MTW_USB_DMA_CFG,
	    MTW_USB_TX_EN | MTW_USB_RX_EN | MTW_USB_RX_AGG_EN |
		MTW_USB_TX_CLEAR | MTW_USB_TXOP_HALT | MTW_USB_RX_WL_DROP);
	mtw_delay(sc, 50);
	mtw_read(sc, MTW_USB_DMA_CFG, &tmp);
	tmp &= ~(MTW_USB_TX_CLEAR | MTW_USB_TXOP_HALT | MTW_USB_RX_WL_DROP);
	mtw_write(sc, MTW_USB_DMA_CFG, tmp);

	/* enable radio */
	mtw_mcu_radio(sc, 0x31, 0);

	/* init RF registers */
	if (sc->asic_ver == 0x7601)
		mt7601_rf_init(sc);

	/* init baseband registers */
	if (sc->asic_ver == 0x7601)
		error = mt7601_bbp_init(sc);

	if (error != 0) {
		device_printf(sc->sc_dev, "could not initialize BBP\n");
		goto fail;
	}

	/* setup and calibrate RF */
	error = mt7601_rf_setup(sc);

	if (error != 0) {
		device_printf(sc->sc_dev, "could not initialize RF\n");
		goto fail;
	}

	/* select default channel */
	mtw_set_chan(sc, ic->ic_curchan);

	/* setup initial protection mode */
	mtw_updateprot_cb(ic);

	sc->sc_flags |= MTW_RUNNING;
	sc->cmdq_run = MTW_CMDQ_GO;
	for (i = 0; i != MTW_N_XFER; i++)
		usbd_xfer_set_stall(sc->sc_xfer[i]);

	usbd_transfer_start(sc->sc_xfer[MTW_BULK_RX]);

	error = mtw_txrx_enable(sc);
	if (error != 0) {
		goto fail;
	}

	return;

fail:

	mtw_stop(sc);
	return;
}

static void
mtw_stop(void *arg)
{
	struct mtw_softc *sc = (struct mtw_softc *)arg;
	uint32_t tmp;
	int i, ntries, error;

	MTW_LOCK_ASSERT(sc, MA_OWNED);

	sc->sc_flags &= ~MTW_RUNNING;

	sc->ratectl_run = MTW_RATECTL_OFF;
	sc->cmdq_run = sc->cmdq_key_set;

	MTW_UNLOCK(sc);

	for (i = 0; i < MTW_N_XFER; i++)
		usbd_transfer_drain(sc->sc_xfer[i]);

	MTW_LOCK(sc);

	mtw_drain_mbufq(sc);

	if (sc->rx_m != NULL) {
		m_free(sc->rx_m);
		sc->rx_m = NULL;
	}

	/* Disable Tx/Rx DMA. */
	mtw_read(sc, MTW_WPDMA_GLO_CFG, &tmp);
	tmp &= ~(MTW_RX_DMA_EN | MTW_TX_DMA_EN);
	mtw_write(sc, MTW_WPDMA_GLO_CFG, tmp);
	// mtw_usb_dma_write(sc, 0);

	for (ntries = 0; ntries < 100; ntries++) {
		if (mtw_read(sc, MTW_WPDMA_GLO_CFG, &tmp) != 0)
			break;
		if ((tmp & (MTW_TX_DMA_BUSY | MTW_RX_DMA_BUSY)) == 0)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for DMA engine\n");
	}

	/* stop MAC Tx/Rx */
	mtw_read(sc, MTW_MAC_SYS_CTRL, &tmp);
	tmp &= ~(MTW_MAC_RX_EN | MTW_MAC_TX_EN);
	mtw_write(sc, MTW_MAC_SYS_CTRL, tmp);

	/* disable RTS retry */
	mtw_read(sc, MTW_TX_RTS_CFG, &tmp);
	tmp &= ~0xff;
	mtw_write(sc, MTW_TX_RTS_CFG, tmp);

	/* US_CYC_CFG */
	mtw_read(sc, MTW_US_CYC_CNT, &tmp);
	tmp = (tmp & ~0xff);
	mtw_write(sc, MTW_US_CYC_CNT, tmp);

	/* stop PBF */
	mtw_read(sc, MTW_PBF_CFG, &tmp);
	tmp &= ~0x3;
	mtw_write(sc, MTW_PBF_CFG, tmp);

	/* wait for pending Tx to complete */
	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = mtw_read(sc, MTW_TXRXQ_PCNT, &tmp)) != 0)
			break;
		if ((tmp & MTW_TX2Q_PCNT_MASK) == 0)
			break;
	}

}

static void
mtw_delay(struct mtw_softc *sc, u_int ms)
{
	usb_pause_mtx(mtx_owned(&sc->sc_mtx) ? &sc->sc_mtx : NULL,
	    USB_MS_TO_TICKS(ms));
}

static void
mtw_update_chw(struct ieee80211com *ic)
{

	printf("%s: TODO\n", __func__);
}

static int
mtw_ampdu_enable(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{

	/* For now, no A-MPDU TX support in the driver */
	return (0);
}

static device_method_t mtw_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, mtw_match),
	DEVMETHOD(device_attach, mtw_attach),
	DEVMETHOD(device_detach, mtw_detach), DEVMETHOD_END
};

static driver_t mtw_driver = { .name = "mtw",
	.methods = mtw_methods,
	.size = sizeof(struct mtw_softc) };

DRIVER_MODULE(mtw, uhub, mtw_driver, mtw_driver_loaded, NULL);
MODULE_DEPEND(mtw, wlan, 1, 1, 1);
MODULE_DEPEND(mtw, usb, 1, 1, 1);
MODULE_DEPEND(mtw, firmware, 1, 1, 1);
MODULE_VERSION(mtw, 1);
USB_PNP_HOST_INFO(mtw_devs);
