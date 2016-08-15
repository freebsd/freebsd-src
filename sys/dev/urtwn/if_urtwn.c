/*	$OpenBSD: if_urtwn.c,v 1.16 2011/02/10 17:26:40 jakemsr Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2014 Kevin Lo <kevlo@FreeBSD.org>
 * Copyright (c) 2015 Andriy Voskoboinyk <avos@FreeBSD.org>
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
 * Driver for Realtek RTL8188CE-VAU/RTL8188CUS/RTL8188EU/RTL8188RU/RTL8192CU.
 */

#include "opt_wlan.h"
#include "opt_urtwn.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
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
#include <net/if_var.h>
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
#ifdef	IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_device.h>
#include "usbdevs.h"

#include <dev/usb/usb_debug.h>

#include <dev/urtwn/if_urtwnreg.h>
#include <dev/urtwn/if_urtwnvar.h>

#ifdef USB_DEBUG
enum {
	URTWN_DEBUG_XMIT	= 0x00000001,	/* basic xmit operation */
	URTWN_DEBUG_RECV	= 0x00000002,	/* basic recv operation */
	URTWN_DEBUG_STATE	= 0x00000004,	/* 802.11 state transitions */
	URTWN_DEBUG_RA		= 0x00000008,	/* f/w rate adaptation setup */
	URTWN_DEBUG_USB		= 0x00000010,	/* usb requests */
	URTWN_DEBUG_FIRMWARE	= 0x00000020,	/* firmware(9) loading debug */
	URTWN_DEBUG_BEACON	= 0x00000040,	/* beacon handling */
	URTWN_DEBUG_INTR	= 0x00000080,	/* ISR */
	URTWN_DEBUG_TEMP	= 0x00000100,	/* temperature calibration */
	URTWN_DEBUG_ROM		= 0x00000200,	/* various ROM info */
	URTWN_DEBUG_KEY		= 0x00000400,	/* crypto keys management */
	URTWN_DEBUG_TXPWR	= 0x00000800,	/* dump Tx power values */
	URTWN_DEBUG_RSSI	= 0x00001000,	/* dump RSSI lookups */
	URTWN_DEBUG_ANY		= 0xffffffff
};

#define URTWN_DPRINTF(_sc, _m, ...) do {			\
	if ((_sc)->sc_debug & (_m))				\
		device_printf((_sc)->sc_dev, __VA_ARGS__);	\
} while(0)

#else
#define URTWN_DPRINTF(_sc, _m, ...)	do { (void) sc; } while (0)
#endif

#define	IEEE80211_HAS_ADDR4(wh)	IEEE80211_IS_DSTODS(wh)

static int urtwn_enable_11n = 1;
TUNABLE_INT("hw.usb.urtwn.enable_11n", &urtwn_enable_11n);

/* various supported device vendors/products */
static const STRUCT_USB_HOST_ID urtwn_devs[] = {
#define URTWN_DEV(v,p)  { USB_VP(USB_VENDOR_##v, USB_PRODUCT_##v##_##p) }
#define	URTWN_RTL8188E_DEV(v,p)	\
	{ USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, URTWN_RTL8188E) }
#define URTWN_RTL8188E  1
	URTWN_DEV(ABOCOM,	RTL8188CU_1),
	URTWN_DEV(ABOCOM,	RTL8188CU_2),
	URTWN_DEV(ABOCOM,	RTL8192CU),
	URTWN_DEV(ASUS,		RTL8192CU),
	URTWN_DEV(ASUS,		USBN10NANO),
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
	URTWN_DEV(REALTEK,	RTL8188CU_3),
	URTWN_DEV(REALTEK,	RTL8188CU_COMBO),
	URTWN_DEV(REALTEK,	RTL8188CUS),
	URTWN_DEV(REALTEK,	RTL8188RU_1),
	URTWN_DEV(REALTEK,	RTL8188RU_2),
	URTWN_DEV(REALTEK,	RTL8188RU_3),
	URTWN_DEV(REALTEK,	RTL8191CU),
	URTWN_DEV(REALTEK,	RTL8192CE),
	URTWN_DEV(REALTEK,	RTL8192CU),
	URTWN_DEV(SITECOMEU,	RTL8188CU_1),
	URTWN_DEV(SITECOMEU,	RTL8188CU_2),
	URTWN_DEV(SITECOMEU,	RTL8192CU),
	URTWN_DEV(TRENDNET,	RTL8188CU),
	URTWN_DEV(TRENDNET,	RTL8192CU),
	URTWN_DEV(ZYXEL,	RTL8192CU),
	/* URTWN_RTL8188E */
	URTWN_RTL8188E_DEV(ABOCOM,	RTL8188EU),
	URTWN_RTL8188E_DEV(DLINK,	DWA123D1),
	URTWN_RTL8188E_DEV(DLINK,	DWA125D1),
	URTWN_RTL8188E_DEV(ELECOM,	WDC150SU2M),
	URTWN_RTL8188E_DEV(REALTEK,	RTL8188ETV),
	URTWN_RTL8188E_DEV(REALTEK,	RTL8188EU),
#undef URTWN_RTL8188E_DEV
#undef URTWN_DEV
};

static device_probe_t	urtwn_match;
static device_attach_t	urtwn_attach;
static device_detach_t	urtwn_detach;

static usb_callback_t   urtwn_bulk_tx_callback;
static usb_callback_t	urtwn_bulk_rx_callback;

static void		urtwn_sysctlattach(struct urtwn_softc *);
static void		urtwn_drain_mbufq(struct urtwn_softc *);
static usb_error_t	urtwn_do_request(struct urtwn_softc *,
			    struct usb_device_request *, void *);
static struct ieee80211vap *urtwn_vap_create(struct ieee80211com *,
		    const char [IFNAMSIZ], int, enum ieee80211_opmode, int,
                    const uint8_t [IEEE80211_ADDR_LEN],
                    const uint8_t [IEEE80211_ADDR_LEN]);
static void		urtwn_vap_delete(struct ieee80211vap *);
static void		urtwn_vap_clear_tx(struct urtwn_softc *,
			    struct ieee80211vap *);
static void		urtwn_vap_clear_tx_queue(struct urtwn_softc *,
			    urtwn_datahead *, struct ieee80211vap *);
static struct mbuf *	urtwn_rx_copy_to_mbuf(struct urtwn_softc *,
			    struct r92c_rx_stat *, int);
static struct mbuf *	urtwn_report_intr(struct usb_xfer *,
			    struct urtwn_data *);
static struct mbuf *	urtwn_rxeof(struct urtwn_softc *, uint8_t *, int);
static void		urtwn_r88e_ratectl_tx_complete(struct urtwn_softc *,
			    void *);
static struct ieee80211_node *urtwn_rx_frame(struct urtwn_softc *,
			    struct mbuf *, int8_t *);
static void		urtwn_txeof(struct urtwn_softc *, struct urtwn_data *,
			    int);
static int		urtwn_alloc_list(struct urtwn_softc *,
			    struct urtwn_data[], int, int);
static int		urtwn_alloc_rx_list(struct urtwn_softc *);
static int		urtwn_alloc_tx_list(struct urtwn_softc *);
static void		urtwn_free_list(struct urtwn_softc *,
			    struct urtwn_data data[], int);
static void		urtwn_free_rx_list(struct urtwn_softc *);
static void		urtwn_free_tx_list(struct urtwn_softc *);
static struct urtwn_data *	_urtwn_getbuf(struct urtwn_softc *);
static struct urtwn_data *	urtwn_getbuf(struct urtwn_softc *);
static usb_error_t	urtwn_write_region_1(struct urtwn_softc *, uint16_t,
			    uint8_t *, int);
static usb_error_t	urtwn_write_1(struct urtwn_softc *, uint16_t, uint8_t);
static usb_error_t	urtwn_write_2(struct urtwn_softc *, uint16_t, uint16_t);
static usb_error_t	urtwn_write_4(struct urtwn_softc *, uint16_t, uint32_t);
static usb_error_t	urtwn_read_region_1(struct urtwn_softc *, uint16_t,
			    uint8_t *, int);
static uint8_t		urtwn_read_1(struct urtwn_softc *, uint16_t);
static uint16_t		urtwn_read_2(struct urtwn_softc *, uint16_t);
static uint32_t		urtwn_read_4(struct urtwn_softc *, uint16_t);
static int		urtwn_fw_cmd(struct urtwn_softc *, uint8_t,
			    const void *, int);
static void		urtwn_cmdq_cb(void *, int);
static int		urtwn_cmd_sleepable(struct urtwn_softc *, const void *,
			    size_t, CMD_FUNC_PROTO);
static void		urtwn_r92c_rf_write(struct urtwn_softc *, int,
			    uint8_t, uint32_t);
static void		urtwn_r88e_rf_write(struct urtwn_softc *, int,
			    uint8_t, uint32_t);
static uint32_t		urtwn_rf_read(struct urtwn_softc *, int, uint8_t);
static int		urtwn_llt_write(struct urtwn_softc *, uint32_t,
			    uint32_t);
static int		urtwn_efuse_read_next(struct urtwn_softc *, uint8_t *);
static int		urtwn_efuse_read_data(struct urtwn_softc *, uint8_t *,
			    uint8_t, uint8_t);
#ifdef USB_DEBUG
static void		urtwn_dump_rom_contents(struct urtwn_softc *,
			    uint8_t *, uint16_t);
#endif
static int		urtwn_efuse_read(struct urtwn_softc *, uint8_t *,
			    uint16_t);
static int		urtwn_efuse_switch_power(struct urtwn_softc *);
static int		urtwn_read_chipid(struct urtwn_softc *);
static int		urtwn_read_rom(struct urtwn_softc *);
static int		urtwn_r88e_read_rom(struct urtwn_softc *);
static int		urtwn_ra_init(struct urtwn_softc *);
static void		urtwn_init_beacon(struct urtwn_softc *,
			    struct urtwn_vap *);
static int		urtwn_setup_beacon(struct urtwn_softc *,
			    struct ieee80211_node *);
static void		urtwn_update_beacon(struct ieee80211vap *, int);
static int		urtwn_tx_beacon(struct urtwn_softc *sc,
			    struct urtwn_vap *);
static int		urtwn_key_alloc(struct ieee80211vap *,
			    struct ieee80211_key *, ieee80211_keyix *,
			    ieee80211_keyix *);
static void		urtwn_key_set_cb(struct urtwn_softc *,
			    union sec_param *);
static void		urtwn_key_del_cb(struct urtwn_softc *,
			    union sec_param *);
static int		urtwn_key_set(struct ieee80211vap *,
			    const struct ieee80211_key *);
static int		urtwn_key_delete(struct ieee80211vap *,
			    const struct ieee80211_key *);
static void		urtwn_tsf_task_adhoc(void *, int);
static void		urtwn_tsf_sync_enable(struct urtwn_softc *,
			    struct ieee80211vap *);
static void		urtwn_get_tsf(struct urtwn_softc *, uint64_t *);
static void		urtwn_set_led(struct urtwn_softc *, int, int);
static void		urtwn_set_mode(struct urtwn_softc *, uint8_t);
static void		urtwn_ibss_recv_mgmt(struct ieee80211_node *,
			    struct mbuf *, int,
			    const struct ieee80211_rx_stats *, int, int);
static int		urtwn_newstate(struct ieee80211vap *,
			    enum ieee80211_state, int);
static void		urtwn_calib_to(void *);
static void		urtwn_calib_cb(struct urtwn_softc *,
			    union sec_param *);
static void		urtwn_watchdog(void *);
static void		urtwn_update_avgrssi(struct urtwn_softc *, int, int8_t);
static int8_t		urtwn_get_rssi(struct urtwn_softc *, int, void *);
static int8_t		urtwn_r88e_get_rssi(struct urtwn_softc *, int, void *);
static int		urtwn_tx_data(struct urtwn_softc *,
			    struct ieee80211_node *, struct mbuf *,
			    struct urtwn_data *);
static int		urtwn_tx_raw(struct urtwn_softc *,
			    struct ieee80211_node *, struct mbuf *,
			    struct urtwn_data *,
			    const struct ieee80211_bpf_params *);
static void		urtwn_tx_start(struct urtwn_softc *, struct mbuf *,
			    uint8_t, struct urtwn_data *);
static int		urtwn_transmit(struct ieee80211com *, struct mbuf *);
static void		urtwn_start(struct urtwn_softc *);
static void		urtwn_parent(struct ieee80211com *);
static int		urtwn_r92c_power_on(struct urtwn_softc *);
static int		urtwn_r88e_power_on(struct urtwn_softc *);
static void		urtwn_r92c_power_off(struct urtwn_softc *);
static void		urtwn_r88e_power_off(struct urtwn_softc *);
static int		urtwn_llt_init(struct urtwn_softc *);
#ifndef URTWN_WITHOUT_UCODE
static void		urtwn_fw_reset(struct urtwn_softc *);
static void		urtwn_r88e_fw_reset(struct urtwn_softc *);
static int		urtwn_fw_loadpage(struct urtwn_softc *, int,
			    const uint8_t *, int);
static int		urtwn_load_firmware(struct urtwn_softc *);
#endif
static int		urtwn_dma_init(struct urtwn_softc *);
static int		urtwn_mac_init(struct urtwn_softc *);
static void		urtwn_bb_init(struct urtwn_softc *);
static void		urtwn_rf_init(struct urtwn_softc *);
static void		urtwn_cam_init(struct urtwn_softc *);
static int		urtwn_cam_write(struct urtwn_softc *, uint32_t,
			    uint32_t);
static void		urtwn_pa_bias_init(struct urtwn_softc *);
static void		urtwn_rxfilter_init(struct urtwn_softc *);
static void		urtwn_edca_init(struct urtwn_softc *);
static void		urtwn_write_txpower(struct urtwn_softc *, int,
			    uint16_t[]);
static void		urtwn_get_txpower(struct urtwn_softc *, int,
		      	    struct ieee80211_channel *,
			    struct ieee80211_channel *, uint16_t[]);
static void		urtwn_r88e_get_txpower(struct urtwn_softc *, int,
		      	    struct ieee80211_channel *,
			    struct ieee80211_channel *, uint16_t[]);
static void		urtwn_set_txpower(struct urtwn_softc *,
		    	    struct ieee80211_channel *,
			    struct ieee80211_channel *);
static void		urtwn_set_rx_bssid_all(struct urtwn_softc *, int);
static void		urtwn_set_gain(struct urtwn_softc *, uint8_t);
static void		urtwn_scan_start(struct ieee80211com *);
static void		urtwn_scan_end(struct ieee80211com *);
static void		urtwn_getradiocaps(struct ieee80211com *, int, int *,
			    struct ieee80211_channel[]);
static void		urtwn_set_channel(struct ieee80211com *);
static int		urtwn_wme_update(struct ieee80211com *);
static void		urtwn_update_slot(struct ieee80211com *);
static void		urtwn_update_slot_cb(struct urtwn_softc *,
			    union sec_param *);
static void		urtwn_update_aifs(struct urtwn_softc *, uint8_t);
static uint8_t		urtwn_get_multi_pos(const uint8_t[]);
static void		urtwn_set_multi(struct urtwn_softc *);
static void		urtwn_set_promisc(struct urtwn_softc *);
static void		urtwn_update_promisc(struct ieee80211com *);
static void		urtwn_update_mcast(struct ieee80211com *);
static struct ieee80211_node *urtwn_node_alloc(struct ieee80211vap *,
			    const uint8_t mac[IEEE80211_ADDR_LEN]);
static void		urtwn_newassoc(struct ieee80211_node *, int);
static void		urtwn_node_free(struct ieee80211_node *);
static void		urtwn_set_chan(struct urtwn_softc *,
		    	    struct ieee80211_channel *,
			    struct ieee80211_channel *);
static void		urtwn_iq_calib(struct urtwn_softc *);
static void		urtwn_lc_calib(struct urtwn_softc *);
static void		urtwn_temp_calib(struct urtwn_softc *);
static void		urtwn_setup_static_keys(struct urtwn_softc *,
			    struct urtwn_vap *);
static int		urtwn_init(struct urtwn_softc *);
static void		urtwn_stop(struct urtwn_softc *);
static void		urtwn_abort_xfers(struct urtwn_softc *);
static int		urtwn_raw_xmit(struct ieee80211_node *, struct mbuf *,
			    const struct ieee80211_bpf_params *);
static void		urtwn_ms_delay(struct urtwn_softc *);

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

static const struct wme_to_queue {
	uint16_t reg;
	uint8_t qid;
} wme2queue[WME_NUM_AC] = {
	{ R92C_EDCA_BE_PARAM, URTWN_BULK_TX_BE},
	{ R92C_EDCA_BK_PARAM, URTWN_BULK_TX_BK},
	{ R92C_EDCA_VI_PARAM, URTWN_BULK_TX_VI},
	{ R92C_EDCA_VO_PARAM, URTWN_BULK_TX_VO}
};

static const uint8_t urtwn_chan_2ghz[] =
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

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

static void
urtwn_update_chw(struct ieee80211com *ic)
{
}

static int
urtwn_ampdu_enable(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{

	/* We're driving this ourselves (eventually); don't involve net80211 */
	return (0);
}

static int
urtwn_attach(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct urtwn_softc *sc = device_get_softc(self);
	struct ieee80211com *ic = &sc->sc_ic;
	int error;

	device_set_usb_desc(self);
	sc->sc_udev = uaa->device;
	sc->sc_dev = self;
	if (USB_GET_DRIVER_INFO(uaa) == URTWN_RTL8188E)
		sc->chip |= URTWN_CHIP_88E;

#ifdef USB_DEBUG
	int debug;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev), "debug", &debug) == 0)
		sc->sc_debug = debug;
#endif

	mtx_init(&sc->sc_mtx, device_get_nameunit(self),
	    MTX_NETWORK_LOCK, MTX_DEF);
	URTWN_CMDQ_LOCK_INIT(sc);
	URTWN_NT_LOCK_INIT(sc);
	callout_init(&sc->sc_calib_to, 0);
	callout_init(&sc->sc_watchdog_ch, 0);
	mbufq_init(&sc->sc_snd, ifqmaxlen);

	sc->sc_iface_index = URTWN_IFACE_INDEX;
	error = usbd_transfer_setup(uaa->device, &sc->sc_iface_index,
	    sc->sc_xfer, urtwn_config, URTWN_N_TRANSFER, sc, &sc->sc_mtx);
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

	if (sc->chip & URTWN_CHIP_88E)
		error = urtwn_r88e_read_rom(sc);
	else
		error = urtwn_read_rom(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "%s: cannot read rom, error %d\n",
		    __func__, error);
		URTWN_UNLOCK(sc);
		goto detach;
	}

	device_printf(sc->sc_dev, "MAC/BB RTL%s, RF 6052 %dT%dR\n",
	    (sc->chip & URTWN_CHIP_92C) ? "8192CU" :
	    (sc->chip & URTWN_CHIP_88E) ? "8188EU" :
	    (sc->board_type == R92C_BOARD_TYPE_HIGHPA) ? "8188RU" :
	    (sc->board_type == R92C_BOARD_TYPE_MINICARD) ? "8188CE-VAU" :
	    "8188CUS", sc->ntxchains, sc->nrxchains);

	URTWN_UNLOCK(sc);

	ic->ic_softc = sc;
	ic->ic_name = device_get_nameunit(self);
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */

	/* set device capabilities */
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_IBSS		/* adhoc mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
#if 0
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
#endif
		| IEEE80211_C_WPA		/* 802.11i */
		| IEEE80211_C_WME		/* 802.11e */
		| IEEE80211_C_SWAMSDUTX		/* Do software A-MSDU TX */
		| IEEE80211_C_FF		/* Atheros fast-frames */
		;

	ic->ic_cryptocaps =
	    IEEE80211_CRYPTO_WEP |
	    IEEE80211_CRYPTO_TKIP |
	    IEEE80211_CRYPTO_AES_CCM;

	/* Assume they're all 11n capable for now */
	if (urtwn_enable_11n) {
		device_printf(self, "enabling 11n\n");
		ic->ic_htcaps = IEEE80211_HTC_HT |
#if 0
		    IEEE80211_HTC_AMPDU |
#endif
		    IEEE80211_HTC_AMSDU |
		    IEEE80211_HTCAP_MAXAMSDU_3839 |
		    IEEE80211_HTCAP_SMPS_OFF;
		/* no HT40 just yet */
		// ic->ic_htcaps |= IEEE80211_HTCAP_CHWIDTH40;

		/* XXX TODO: verify chains versus streams for urtwn */
		ic->ic_txstream = sc->ntxchains;
		ic->ic_rxstream = sc->nrxchains;
	}

	/* XXX TODO: setup regdomain if R92C_CHANNEL_PLAN_BY_HW bit is set. */

	urtwn_getradiocaps(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	ieee80211_ifattach(ic);
	ic->ic_raw_xmit = urtwn_raw_xmit;
	ic->ic_scan_start = urtwn_scan_start;
	ic->ic_scan_end = urtwn_scan_end;
	ic->ic_getradiocaps = urtwn_getradiocaps;
	ic->ic_set_channel = urtwn_set_channel;
	ic->ic_transmit = urtwn_transmit;
	ic->ic_parent = urtwn_parent;
	ic->ic_vap_create = urtwn_vap_create;
	ic->ic_vap_delete = urtwn_vap_delete;
	ic->ic_wme.wme_update = urtwn_wme_update;
	ic->ic_updateslot = urtwn_update_slot;
	ic->ic_update_promisc = urtwn_update_promisc;
	ic->ic_update_mcast = urtwn_update_mcast;
	if (sc->chip & URTWN_CHIP_88E) {
		ic->ic_node_alloc = urtwn_node_alloc;
		ic->ic_newassoc = urtwn_newassoc;
		sc->sc_node_free = ic->ic_node_free;
		ic->ic_node_free = urtwn_node_free;
	}
	ic->ic_update_chw = urtwn_update_chw;
	ic->ic_ampdu_enable = urtwn_ampdu_enable;

	ieee80211_radiotap_attach(ic, &sc->sc_txtap.wt_ihdr,
	    sizeof(sc->sc_txtap), URTWN_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
	    URTWN_RX_RADIOTAP_PRESENT);

	TASK_INIT(&sc->cmdq_task, 0, urtwn_cmdq_cb, sc);

	urtwn_sysctlattach(sc);

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

detach:
	urtwn_detach(self);
	return (ENXIO);			/* failure */
}

static void
urtwn_sysctlattach(struct urtwn_softc *sc)
{
#ifdef USB_DEBUG
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, sc->sc_debug,
	    "control debugging printfs");
#endif
}

static int
urtwn_detach(device_t self)
{
	struct urtwn_softc *sc = device_get_softc(self);
	struct ieee80211com *ic = &sc->sc_ic;

	/* Prevent further ioctls. */
	URTWN_LOCK(sc);
	sc->sc_flags |= URTWN_DETACHED;
	URTWN_UNLOCK(sc);

	urtwn_stop(sc);

	callout_drain(&sc->sc_watchdog_ch);
	callout_drain(&sc->sc_calib_to);

	/* stop all USB transfers */
	usbd_transfer_unsetup(sc->sc_xfer, URTWN_N_TRANSFER);

	if (ic->ic_softc == sc) {
		ieee80211_draintask(ic, &sc->cmdq_task);
		ieee80211_ifdetach(ic);
	}

	URTWN_NT_LOCK_DESTROY(sc);
	URTWN_CMDQ_LOCK_DESTROY(sc);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
urtwn_drain_mbufq(struct urtwn_softc *sc)
{
	struct mbuf *m;
	struct ieee80211_node *ni;
	URTWN_ASSERT_LOCKED(sc);
	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;
		ieee80211_free_node(ni);
		m_freem(m);
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

		URTWN_DPRINTF(sc, URTWN_DEBUG_USB,
		    "%s: control request failed, %s (retries left: %d)\n",
		    __func__, usbd_errstr(err), ntries);
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
	struct urtwn_softc *sc = ic->ic_softc;
	struct urtwn_vap *uvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return (NULL);

	uvp = malloc(sizeof(struct urtwn_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &uvp->vap;
	/* enable s/w bmiss handling for sta mode */

	if (ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid) != 0) {
		/* out of memory */
		free(uvp, M_80211_VAP);
		return (NULL);
	}

	if (opmode == IEEE80211_M_HOSTAP || opmode == IEEE80211_M_IBSS)
		urtwn_init_beacon(sc, uvp);

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = urtwn_newstate;
	vap->iv_update_beacon = urtwn_update_beacon;
	vap->iv_key_alloc = urtwn_key_alloc;
	vap->iv_key_set = urtwn_key_set;
	vap->iv_key_delete = urtwn_key_delete;

	/* 802.11n parameters */
	vap->iv_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_16;
	vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_64K;

	if (opmode == IEEE80211_M_IBSS) {
		uvp->recv_mgmt = vap->iv_recv_mgmt;
		vap->iv_recv_mgmt = urtwn_ibss_recv_mgmt;
		TASK_INIT(&uvp->tsf_task_adhoc, 0, urtwn_tsf_task_adhoc, vap);
	}

	if (URTWN_CHIP_HAS_RATECTL(sc))
		ieee80211_ratectl_init(vap);
	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);
	ic->ic_opmode = opmode;
	return (vap);
}

static void
urtwn_vap_delete(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct urtwn_softc *sc = ic->ic_softc;
	struct urtwn_vap *uvp = URTWN_VAP(vap);

	/* Guarantee that nothing will go through this vap. */
	ieee80211_new_state(vap, IEEE80211_S_INIT, -1);
	ieee80211_draintask(ic, &vap->iv_nstate_task);

	URTWN_LOCK(sc);
	if (uvp->bcn_mbuf != NULL)
		m_freem(uvp->bcn_mbuf);
	/* Cancel any unfinished Tx. */
	urtwn_vap_clear_tx(sc, vap);
	URTWN_UNLOCK(sc);
	if (vap->iv_opmode == IEEE80211_M_IBSS)
		ieee80211_draintask(ic, &uvp->tsf_task_adhoc);
	if (URTWN_CHIP_HAS_RATECTL(sc))
		ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

static void
urtwn_vap_clear_tx(struct urtwn_softc *sc, struct ieee80211vap *vap)
{

	URTWN_ASSERT_LOCKED(sc);

	urtwn_vap_clear_tx_queue(sc, &sc->sc_tx_active, vap);
	urtwn_vap_clear_tx_queue(sc, &sc->sc_tx_pending, vap);
}

static void
urtwn_vap_clear_tx_queue(struct urtwn_softc *sc, urtwn_datahead *head,
    struct ieee80211vap *vap)
{
	struct urtwn_data *dp, *tmp;

	STAILQ_FOREACH_SAFE(dp, head, next, tmp) {
		if (dp->ni != NULL) {
			if (dp->ni->ni_vap == vap) {
				ieee80211_free_node(dp->ni);
				dp->ni = NULL;

				if (dp->m != NULL) {
					m_freem(dp->m);
					dp->m = NULL;
				}

				STAILQ_REMOVE(head, dp, urtwn_data, next);
				STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, dp,
				    next);
			}
		}
	}
}

static struct mbuf *
urtwn_rx_copy_to_mbuf(struct urtwn_softc *sc, struct r92c_rx_stat *stat,
    int totlen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m;
	uint32_t rxdw0;
	int pktlen;

	/*
	 * don't pass packets to the ieee80211 framework if the driver isn't
	 * RUNNING.
	 */
	if (!(sc->sc_flags & URTWN_RUNNING))
		return (NULL);

	rxdw0 = le32toh(stat->rxdw0);
	if (rxdw0 & (R92C_RXDW0_CRCERR | R92C_RXDW0_ICVERR)) {
		/*
		 * This should not happen since we setup our Rx filter
		 * to not receive these frames.
		 */
		URTWN_DPRINTF(sc, URTWN_DEBUG_RECV,
		    "%s: RX flags error (%s)\n", __func__,
		    rxdw0 & R92C_RXDW0_CRCERR ? "CRC" : "ICV");
		goto fail;
	}

	pktlen = MS(rxdw0, R92C_RXDW0_PKTLEN);
	if (pktlen < sizeof(struct ieee80211_frame_ack)) {
		URTWN_DPRINTF(sc, URTWN_DEBUG_RECV,
		    "%s: frame is too short: %d\n", __func__, pktlen);
		goto fail;
	}

	m = m_get2(totlen, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (__predict_false(m == NULL)) {
		device_printf(sc->sc_dev, "%s: could not allocate RX mbuf\n",
		    __func__);
		goto fail;
	}

	/* Finalize mbuf. */
	memcpy(mtod(m, uint8_t *), (uint8_t *)stat, totlen);
	m->m_pkthdr.len = m->m_len = totlen;
 
	return (m);
fail:
	counter_u64_add(ic->ic_ierrors, 1);
	return (NULL);
}

static struct mbuf *
urtwn_report_intr(struct usb_xfer *xfer, struct urtwn_data *data)
{
	struct urtwn_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rx_stat *stat;
	uint8_t *buf;
	int len;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	if (len < sizeof(*stat)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return (NULL);
	}

	buf = data->buf;
	stat = (struct r92c_rx_stat *)buf;

	/*
	 * For 88E chips we can tie the FF flushing here;
	 * this is where we do know exactly how deep the
	 * transmit queue is.
	 *
	 * But it won't work for R92 chips, so we can't
	 * take the easy way out.
	 */

	if (sc->chip & URTWN_CHIP_88E) {
		int report_sel = MS(le32toh(stat->rxdw3), R88E_RXDW3_RPT);

		switch (report_sel) {
		case R88E_RXDW3_RPT_RX:
			return (urtwn_rxeof(sc, buf, len));
		case R88E_RXDW3_RPT_TX1:
			urtwn_r88e_ratectl_tx_complete(sc, &stat[1]);
			break;
		default:
			URTWN_DPRINTF(sc, URTWN_DEBUG_INTR,
			    "%s: case %d was not handled\n", __func__,
			    report_sel);
			break;
		}
	} else
		return (urtwn_rxeof(sc, buf, len));

	return (NULL);
}

static struct mbuf *
urtwn_rxeof(struct urtwn_softc *sc, uint8_t *buf, int len)
{
	struct r92c_rx_stat *stat;
	struct mbuf *m, *m0 = NULL, *prevm = NULL;
	uint32_t rxdw0;
	int totlen, pktlen, infosz, npkts;

	/* Get the number of encapsulated frames. */
	stat = (struct r92c_rx_stat *)buf;
	npkts = MS(le32toh(stat->rxdw2), R92C_RXDW2_PKTCNT);
	URTWN_DPRINTF(sc, URTWN_DEBUG_RECV,
	    "%s: Rx %d frames in one chunk\n", __func__, npkts);

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

		m = urtwn_rx_copy_to_mbuf(sc, stat, totlen);
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
urtwn_r88e_ratectl_tx_complete(struct urtwn_softc *sc, void *arg)
{
	struct r88e_tx_rpt_ccx *rpt = arg;
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;
	uint8_t macid;
	int ntries;

	macid = MS(rpt->rptb1, R88E_RPTB1_MACID);
	ntries = MS(rpt->rptb2, R88E_RPTB2_RETRY_CNT);

	URTWN_NT_LOCK(sc);
	ni = sc->node_list[macid];
	if (ni != NULL) {
		vap = ni->ni_vap;
		URTWN_DPRINTF(sc, URTWN_DEBUG_INTR, "%s: frame for macid %d was"
		    "%s sent (%d retries)\n", __func__, macid,
		    (rpt->rptb1 & R88E_RPTB1_PKT_OK) ? "" : " not",
		    ntries);

		if (rpt->rptb1 & R88E_RPTB1_PKT_OK) {
			ieee80211_ratectl_tx_complete(vap, ni,
			    IEEE80211_RATECTL_TX_SUCCESS, &ntries, NULL);
		} else {
			ieee80211_ratectl_tx_complete(vap, ni,
			    IEEE80211_RATECTL_TX_FAILURE, &ntries, NULL);
		}
	} else {
		URTWN_DPRINTF(sc, URTWN_DEBUG_INTR, "%s: macid %d, ni is NULL\n",
		    __func__, macid);
	}
	URTWN_NT_UNLOCK(sc);
}

static struct ieee80211_node *
urtwn_rx_frame(struct urtwn_softc *sc, struct mbuf *m, int8_t *rssi_p)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame_min *wh;
	struct r92c_rx_stat *stat;
	uint32_t rxdw0, rxdw3;
	uint8_t rate, cipher;
	int8_t rssi = -127;
	int infosz;

	stat = mtod(m, struct r92c_rx_stat *);
	rxdw0 = le32toh(stat->rxdw0);
	rxdw3 = le32toh(stat->rxdw3);

	rate = MS(rxdw3, R92C_RXDW3_RATE);
	cipher = MS(rxdw0, R92C_RXDW0_CIPHER);
	infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;

	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0 && (rxdw0 & R92C_RXDW0_PHYST)) {
		if (sc->chip & URTWN_CHIP_88E)
			rssi = urtwn_r88e_get_rssi(sc, rate, &stat[1]);
		else
			rssi = urtwn_get_rssi(sc, rate, &stat[1]);
		URTWN_DPRINTF(sc, URTWN_DEBUG_RSSI, "%s: rssi=%d\n", __func__, rssi);
		/* Update our average RSSI. */
		urtwn_update_avgrssi(sc, rate, rssi);
	}

	if (ieee80211_radiotap_active(ic)) {
		struct urtwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;

		urtwn_get_tsf(sc, &tap->wr_tsft);
		if (__predict_false(le32toh((uint32_t)tap->wr_tsft) <
				    le32toh(stat->rxdw5))) {
			tap->wr_tsft = le32toh(tap->wr_tsft  >> 32) - 1;
			tap->wr_tsft = (uint64_t)htole32(tap->wr_tsft) << 32;
		} else
			tap->wr_tsft &= 0xffffffff00000000;
		tap->wr_tsft += stat->rxdw5;

		/* XXX 20/40? */
		/* XXX shortgi? */

		/* Map HW rate index to 802.11 rate. */
		if (!(rxdw3 & R92C_RXDW3_HT)) {
			tap->wr_rate = ridx2rate[rate];
		} else if (rate >= 12) {	/* MCS0~15. */
			/* Bit 7 set means HT MCS instead of rate. */
			tap->wr_rate = 0x80 | (rate - 12);
		}

		/* XXX TODO: this isn't right; should use the last good RSSI */
		tap->wr_dbm_antsignal = rssi;
		tap->wr_dbm_antnoise = URTWN_NOISE_FLOOR;
	}

	*rssi_p = rssi;

	/* Drop descriptor. */
	m_adj(m, sizeof(*stat) + infosz);
	wh = mtod(m, struct ieee80211_frame_min *);

	if ((wh->i_fc[1] & IEEE80211_FC1_PROTECTED) &&
	    cipher != R92C_CAM_ALGO_NONE) {
		m->m_flags |= M_WEP;
	}

	if (m->m_len >= sizeof(*wh))
		return (ieee80211_find_rxnode(ic, wh));

	return (NULL);
}

static void
urtwn_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct urtwn_softc *sc = usbd_xfer_softc(xfer);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m = NULL, *next;
	struct urtwn_data *data;
	int8_t nf, rssi;

	URTWN_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_rx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&sc->sc_rx_active, next);
		m = urtwn_report_intr(xfer, data);
		STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&sc->sc_rx_inactive);
		if (data == NULL) {
			KASSERT(m == NULL, ("mbuf isn't NULL"));
			goto finish;
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
		while (m != NULL) {
			next = m->m_next;
			m->m_next = NULL;

			ni = urtwn_rx_frame(sc, m, &rssi);

			/* Store a global last-good RSSI */
			if (rssi != -127)
				sc->last_rssi = rssi;

			URTWN_UNLOCK(sc);

			nf = URTWN_NOISE_FLOOR;
			if (ni != NULL) {
				if (rssi != -127)
					URTWN_NODE(ni)->last_rssi = rssi;
				if (ni->ni_flags & IEEE80211_NODE_HT)
					m->m_flags |= M_AMPDU;
				(void)ieee80211_input(ni, m,
				    URTWN_NODE(ni)->last_rssi - nf, nf);
				ieee80211_free_node(ni);
			} else {
				/* Use last good global RSSI */
				(void)ieee80211_input_all(ic, m,
				    sc->last_rssi - nf, nf);
			}
			URTWN_LOCK(sc);
			m = next;
		}
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
finish:
	/* Finished receive; age anything left on the FF queue by a little bump */
	/*
	 * XXX TODO: just make this a callout timer schedule so we can
	 * flush the FF staging queue if we're approaching idle.
	 */
#ifdef	IEEE80211_SUPPORT_SUPERG
	URTWN_UNLOCK(sc);
	ieee80211_ff_age_all(ic, 1);
	URTWN_LOCK(sc);
#endif

	/* Kick-start more transmit in case we stalled */
	urtwn_start(sc);
}

static void
urtwn_txeof(struct urtwn_softc *sc, struct urtwn_data *data, int status)
{

	URTWN_ASSERT_LOCKED(sc);

	if (data->ni != NULL)	/* not a beacon frame */
		ieee80211_tx_complete(data->ni, data->m, status);

	if (sc->sc_tx_n_active > 0)
		sc->sc_tx_n_active--;

	data->ni = NULL;
	data->m = NULL;

	sc->sc_txtimer = 0;

	STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data, next);
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

static void
urtwn_free_rx_list(struct urtwn_softc *sc)
{
	urtwn_free_list(sc, sc->sc_rx, URTWN_RX_LIST_COUNT);

	STAILQ_INIT(&sc->sc_rx_active);
	STAILQ_INIT(&sc->sc_rx_inactive);
}

static void
urtwn_free_tx_list(struct urtwn_softc *sc)
{
	urtwn_free_list(sc, sc->sc_tx, URTWN_TX_LIST_COUNT);

	STAILQ_INIT(&sc->sc_tx_active);
	STAILQ_INIT(&sc->sc_tx_inactive);
	STAILQ_INIT(&sc->sc_tx_pending);
}

static void
urtwn_bulk_tx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct urtwn_softc *sc = usbd_xfer_softc(xfer);
#ifdef	IEEE80211_SUPPORT_SUPERG
	struct ieee80211com *ic = &sc->sc_ic;
#endif
	struct urtwn_data *data;

	URTWN_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)){
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_tx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&sc->sc_tx_active, next);
		urtwn_txeof(sc, data, 0);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&sc->sc_tx_pending);
		if (data == NULL) {
			URTWN_DPRINTF(sc, URTWN_DEBUG_XMIT,
			    "%s: empty pending queue\n", __func__);
			sc->sc_tx_n_active = 0;
			goto finish;
		}
		STAILQ_REMOVE_HEAD(&sc->sc_tx_pending, next);
		STAILQ_INSERT_TAIL(&sc->sc_tx_active, data, next);
		usbd_xfer_set_frame_data(xfer, 0, data->buf, data->buflen);
		usbd_transfer_submit(xfer);
		sc->sc_tx_n_active++;
		break;
	default:
		data = STAILQ_FIRST(&sc->sc_tx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&sc->sc_tx_active, next);
		urtwn_txeof(sc, data, 1);
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
finish:
#ifdef	IEEE80211_SUPPORT_SUPERG
	/*
	 * If the TX active queue drops below a certain
	 * threshold, ensure we age fast-frames out so they're
	 * transmitted.
	 */
	if (sc->sc_tx_n_active <= 1) {
		/* XXX ew - net80211 should defer this for us! */

		/*
		 * Note: this sc_tx_n_active currently tracks
		 * the number of pending transmit submissions
		 * and not the actual depth of the TX frames
		 * pending to the hardware.  That means that
		 * we're going to end up with some sub-optimal
		 * aggregation behaviour.
		 */
		/*
		 * XXX TODO: just make this a callout timer schedule so we can
		 * flush the FF staging queue if we're approaching idle.
		 */
		URTWN_UNLOCK(sc);
		ieee80211_ff_flush(ic, WME_AC_VO);
		ieee80211_ff_flush(ic, WME_AC_VI);
		ieee80211_ff_flush(ic, WME_AC_BE);
		ieee80211_ff_flush(ic, WME_AC_BK);
		URTWN_LOCK(sc);
	}
#endif
	/* Kick-start more transmit */
	urtwn_start(sc);
}

static struct urtwn_data *
_urtwn_getbuf(struct urtwn_softc *sc)
{
	struct urtwn_data *bf;

	bf = STAILQ_FIRST(&sc->sc_tx_inactive);
	if (bf != NULL)
		STAILQ_REMOVE_HEAD(&sc->sc_tx_inactive, next);
	else {
		URTWN_DPRINTF(sc, URTWN_DEBUG_XMIT,
		    "%s: out of xmit buffers\n", __func__);
	}
	return (bf);
}

static struct urtwn_data *
urtwn_getbuf(struct urtwn_softc *sc)
{
        struct urtwn_data *bf;

	URTWN_ASSERT_LOCKED(sc);

	bf = _urtwn_getbuf(sc);
	if (bf == NULL) {
		URTWN_DPRINTF(sc, URTWN_DEBUG_XMIT, "%s: stop queue\n",
		    __func__);
	}
	return (bf);
}

static usb_error_t
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

static usb_error_t
urtwn_write_1(struct urtwn_softc *sc, uint16_t addr, uint8_t val)
{
	return (urtwn_write_region_1(sc, addr, &val, sizeof(val)));
}

static usb_error_t
urtwn_write_2(struct urtwn_softc *sc, uint16_t addr, uint16_t val)
{
	val = htole16(val);
	return (urtwn_write_region_1(sc, addr, (uint8_t *)&val, sizeof(val)));
}

static usb_error_t
urtwn_write_4(struct urtwn_softc *sc, uint16_t addr, uint32_t val)
{
	val = htole32(val);
	return (urtwn_write_region_1(sc, addr, (uint8_t *)&val, sizeof(val)));
}

static usb_error_t
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
	usb_error_t error;
	int ntries;

	if (!(sc->sc_flags & URTWN_FW_LOADED)) {
		URTWN_DPRINTF(sc, URTWN_DEBUG_FIRMWARE, "%s: firmware "
		    "was not loaded; command (id %d) will be discarded\n",
		    __func__, id);
		return (0);
	}

	/* Wait for current FW box to be empty. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(urtwn_read_1(sc, R92C_HMETFR) & (1 << sc->fwcur)))
			break;
		urtwn_ms_delay(sc);
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
	error = urtwn_write_region_1(sc, R92C_HMEBOX_EXT(sc->fwcur),
	    (uint8_t *)&cmd + 4, 2);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	error = urtwn_write_region_1(sc, R92C_HMEBOX(sc->fwcur),
	    (uint8_t *)&cmd + 0, 4);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	sc->fwcur = (sc->fwcur + 1) % R92C_H2C_NBOX;
	return (0);
}

static void
urtwn_cmdq_cb(void *arg, int pending)
{
	struct urtwn_softc *sc = arg;
	struct urtwn_cmdq *item;

	/*
	 * Device must be powered on (via urtwn_power_on())
	 * before any command may be sent.
	 */
	URTWN_LOCK(sc);
	if (!(sc->sc_flags & URTWN_RUNNING)) {
		URTWN_UNLOCK(sc);
		return;
	}

	URTWN_CMDQ_LOCK(sc);
	while (sc->cmdq[sc->cmdq_first].func != NULL) {
		item = &sc->cmdq[sc->cmdq_first];
		sc->cmdq_first = (sc->cmdq_first + 1) % URTWN_CMDQ_SIZE;
		URTWN_CMDQ_UNLOCK(sc);

		item->func(sc, &item->data);

		URTWN_CMDQ_LOCK(sc);
		memset(item, 0, sizeof (*item));
	}
	URTWN_CMDQ_UNLOCK(sc);
	URTWN_UNLOCK(sc);
}

static int
urtwn_cmd_sleepable(struct urtwn_softc *sc, const void *ptr, size_t len,
    CMD_FUNC_PROTO)
{
	struct ieee80211com *ic = &sc->sc_ic;

	KASSERT(len <= sizeof(union sec_param), ("buffer overflow"));

	URTWN_CMDQ_LOCK(sc);
	if (sc->cmdq[sc->cmdq_last].func != NULL) {
		device_printf(sc->sc_dev, "%s: cmdq overflow\n", __func__);
		URTWN_CMDQ_UNLOCK(sc);

		return (EAGAIN);
	}

	if (ptr != NULL)
		memcpy(&sc->cmdq[sc->cmdq_last].data, ptr, len);
	sc->cmdq[sc->cmdq_last].func = func;
	sc->cmdq_last = (sc->cmdq_last + 1) % URTWN_CMDQ_SIZE;
	URTWN_CMDQ_UNLOCK(sc);

	ieee80211_runtask(ic, &sc->cmdq_task);

	return (0);
}

static __inline void
urtwn_rf_write(struct urtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{

	sc->sc_rf_write(sc, chain, addr, val);
}

static void
urtwn_r92c_rf_write(struct urtwn_softc *sc, int chain, uint8_t addr,
    uint32_t val)
{
	urtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) |
	    SM(R92C_LSSI_PARAM_DATA, val));
}

static void
urtwn_r88e_rf_write(struct urtwn_softc *sc, int chain, uint8_t addr,
uint32_t val)
{
	urtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R88E_LSSI_PARAM_ADDR, addr) |
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
	urtwn_ms_delay(sc);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	urtwn_ms_delay(sc);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	urtwn_ms_delay(sc);

	if (urtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI)
		val = urtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	else
		val = urtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	return (MS(val, R92C_LSSI_READBACK_DATA));
}

static int
urtwn_llt_write(struct urtwn_softc *sc, uint32_t addr, uint32_t data)
{
	usb_error_t error;
	int ntries;

	error = urtwn_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(urtwn_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return (0);
		urtwn_ms_delay(sc);
	}
	return (ETIMEDOUT);
}

static int
urtwn_efuse_read_next(struct urtwn_softc *sc, uint8_t *val)
{
	uint32_t reg;
	usb_error_t error;
	int ntries;

	if (sc->last_rom_addr >= URTWN_EFUSE_MAX_LEN)
		return (EFAULT);

	reg = urtwn_read_4(sc, R92C_EFUSE_CTRL);
	reg = RW(reg, R92C_EFUSE_CTRL_ADDR, sc->last_rom_addr);
	reg &= ~R92C_EFUSE_CTRL_VALID;

	error = urtwn_write_4(sc, R92C_EFUSE_CTRL, reg);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = urtwn_read_4(sc, R92C_EFUSE_CTRL);
		if (reg & R92C_EFUSE_CTRL_VALID)
			break;
		urtwn_ms_delay(sc);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "could not read efuse byte at address 0x%x\n",
		    sc->last_rom_addr);
		return (ETIMEDOUT);
	}

	*val = MS(reg, R92C_EFUSE_CTRL_DATA);
	sc->last_rom_addr++;

	return (0);
}

static int
urtwn_efuse_read_data(struct urtwn_softc *sc, uint8_t *rom, uint8_t off,
    uint8_t msk)
{
	uint8_t reg;
	int i, error;

	for (i = 0; i < 4; i++) {
		if (msk & (1 << i))
			continue;
		error = urtwn_efuse_read_next(sc, &reg);
		if (error != 0)
			return (error);
		URTWN_DPRINTF(sc, URTWN_DEBUG_ROM, "rom[0x%03X] == 0x%02X\n",
		    off * 8 + i * 2, reg);
		rom[off * 8 + i * 2 + 0] = reg;

		error = urtwn_efuse_read_next(sc, &reg);
		if (error != 0)
			return (error);
		URTWN_DPRINTF(sc, URTWN_DEBUG_ROM, "rom[0x%03X] == 0x%02X\n",
		    off * 8 + i * 2 + 1, reg);
		rom[off * 8 + i * 2 + 1] = reg;
	}

	return (0);
}

#ifdef USB_DEBUG
static void
urtwn_dump_rom_contents(struct urtwn_softc *sc, uint8_t *rom, uint16_t size)
{
	int i;

	/* Dump ROM contents. */
	device_printf(sc->sc_dev, "%s:", __func__);
	for (i = 0; i < size; i++) {
		if (i % 32 == 0)
			printf("\n%03X: ", i);
		else if (i % 4 == 0)
			printf(" ");

		printf("%02X", rom[i]);
	}
	printf("\n");
}
#endif

static int
urtwn_efuse_read(struct urtwn_softc *sc, uint8_t *rom, uint16_t size)
{
#define URTWN_CHK(res) do {	\
	if ((error = res) != 0)	\
		goto end;	\
} while(0)
	uint8_t msk, off, reg;
	int error;

	URTWN_CHK(urtwn_efuse_switch_power(sc));

	/* Read full ROM image. */
	sc->last_rom_addr = 0;
	memset(rom, 0xff, size);

	URTWN_CHK(urtwn_efuse_read_next(sc, &reg));
	while (reg != 0xff) {
		/* check for extended header */
		if ((sc->chip & URTWN_CHIP_88E) && (reg & 0x1f) == 0x0f) {
			off = reg >> 5;
			URTWN_CHK(urtwn_efuse_read_next(sc, &reg));

			if ((reg & 0x0f) != 0x0f)
				off = ((reg & 0xf0) >> 1) | off;
			else
				continue;
		} else
			off = reg >> 4;
		msk = reg & 0xf;

		URTWN_CHK(urtwn_efuse_read_data(sc, rom, off, msk));
		URTWN_CHK(urtwn_efuse_read_next(sc, &reg));
	}

end:

#ifdef USB_DEBUG
	if (sc->sc_debug & URTWN_DEBUG_ROM)
		urtwn_dump_rom_contents(sc, rom, size);
#endif

	urtwn_write_1(sc, R92C_EFUSE_ACCESS, R92C_EFUSE_ACCESS_OFF);

	if (error != 0) {
		device_printf(sc->sc_dev, "%s: error while reading ROM\n",
		    __func__);
	}

	return (error);
#undef URTWN_CHK
}

static int
urtwn_efuse_switch_power(struct urtwn_softc *sc)
{
	usb_error_t error;
	uint32_t reg;

	error = urtwn_write_1(sc, R92C_EFUSE_ACCESS, R92C_EFUSE_ACCESS_ON);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	reg = urtwn_read_2(sc, R92C_SYS_ISO_CTRL);
	if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) {
		error = urtwn_write_2(sc, R92C_SYS_ISO_CTRL,
		    reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
	}
	reg = urtwn_read_2(sc, R92C_SYS_FUNC_EN);
	if (!(reg & R92C_SYS_FUNC_EN_ELDR)) {
		error = urtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    reg | R92C_SYS_FUNC_EN_ELDR);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
	}
	reg = urtwn_read_2(sc, R92C_SYS_CLKR);
	if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) !=
	    (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) {
		error = urtwn_write_2(sc, R92C_SYS_CLKR,
		    reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
	}

	return (0);
}

static int
urtwn_read_chipid(struct urtwn_softc *sc)
{
	uint32_t reg;

	if (sc->chip & URTWN_CHIP_88E)
		return (0);

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

static int
urtwn_read_rom(struct urtwn_softc *sc)
{
	struct r92c_rom *rom = &sc->rom.r92c_rom;
	int error;

	/* Read full ROM image. */
	error = urtwn_efuse_read(sc, (uint8_t *)rom, sizeof(*rom));
	if (error != 0)
		return (error);

	/* XXX Weird but this is what the vendor driver does. */
	sc->last_rom_addr = 0x1fa;
	error = urtwn_efuse_read_next(sc, &sc->pa_setting);
	if (error != 0)
		return (error);
	URTWN_DPRINTF(sc, URTWN_DEBUG_ROM, "%s: PA setting=0x%x\n", __func__,
	    sc->pa_setting);

	sc->board_type = MS(rom->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);

	sc->regulatory = MS(rom->rf_opt1, R92C_ROM_RF1_REGULATORY);
	URTWN_DPRINTF(sc, URTWN_DEBUG_ROM, "%s: regulatory type=%d\n",
	    __func__, sc->regulatory);
	IEEE80211_ADDR_COPY(sc->sc_ic.ic_macaddr, rom->macaddr);

	sc->sc_rf_write = urtwn_r92c_rf_write;
	sc->sc_power_on = urtwn_r92c_power_on;
	sc->sc_power_off = urtwn_r92c_power_off;

	return (0);
}

static int
urtwn_r88e_read_rom(struct urtwn_softc *sc)
{
	struct r88e_rom *rom = &sc->rom.r88e_rom;
	int error;

	error = urtwn_efuse_read(sc, (uint8_t *)rom, sizeof(sc->rom.r88e_rom));
	if (error != 0)
		return (error);

	sc->bw20_tx_pwr_diff = (rom->tx_pwr_diff >> 4);
	if (sc->bw20_tx_pwr_diff & 0x08)
		sc->bw20_tx_pwr_diff |= 0xf0;
	sc->ofdm_tx_pwr_diff = (rom->tx_pwr_diff & 0xf);
	if (sc->ofdm_tx_pwr_diff & 0x08)
		sc->ofdm_tx_pwr_diff |= 0xf0;
	sc->regulatory = MS(rom->rf_board_opt, R92C_ROM_RF1_REGULATORY);
	URTWN_DPRINTF(sc, URTWN_DEBUG_ROM, "%s: regulatory type %d\n",
	    __func__,sc->regulatory);
	IEEE80211_ADDR_COPY(sc->sc_ic.ic_macaddr, rom->macaddr);

	sc->sc_rf_write = urtwn_r88e_rf_write;
	sc->sc_power_on = urtwn_r88e_power_on;
	sc->sc_power_off = urtwn_r88e_power_off;

	return (0);
}

static __inline uint8_t
rate2ridx(uint8_t rate)
{
	if (rate & IEEE80211_RATE_MCS) {
		/* 11n rates start at idx 12 */
		return ((rate & 0xf) + 12);
	}
	switch (rate) {
	/* 11g */
	case 12:	return 4;
	case 18:	return 5;
	case 24:	return 6;
	case 36:	return 7;
	case 48:	return 8;
	case 72:	return 9;
	case 96:	return 10;
	case 108:	return 11;
	/* 11b */
	case 2:		return 0;
	case 4:		return 1;
	case 11:	return 2;
	case 22:	return 3;
	default:	return URTWN_RIDX_UNKNOWN;
	}
}

/*
 * Initialize rate adaptation in firmware.
 */
static int
urtwn_ra_init(struct urtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni;
	struct ieee80211_rateset *rs, *rs_ht;
	struct r92c_fw_cmd_macid_cfg cmd;
	uint32_t rates, basicrates;
	uint8_t mode, ridx;
	int maxrate, maxbasicrate, error, i;

	ni = ieee80211_ref_node(vap->iv_bss);
	rs = &ni->ni_rates;
	rs_ht = (struct ieee80211_rateset *) &ni->ni_htrates;

	/* Get normal and basic rates mask. */
	rates = basicrates = 0;
	maxrate = maxbasicrate = 0;

	/* This is for 11bg */
	for (i = 0; i < rs->rs_nrates; i++) {
		/* Convert 802.11 rate to HW rate index. */
		ridx = rate2ridx(IEEE80211_RV(rs->rs_rates[i]));
		if (ridx == URTWN_RIDX_UNKNOWN)	/* Unknown rate, skip. */
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

	/* If we're doing 11n, enable 11n rates */
	if (ni->ni_flags & IEEE80211_NODE_HT) {
		for (i = 0; i < rs_ht->rs_nrates; i++) {
			if ((rs_ht->rs_rates[i] & 0x7f) > 0xf)
				continue;
			/* 11n rates start at index 12 */
			ridx = ((rs_ht->rs_rates[i]) & 0xf) + 12;
			rates |= (1 << ridx);

			/* Guard against the rate table being oddly ordered */
			if (ridx > maxrate)
				maxrate = ridx;
		}
	}

#if 0
	if (ic->ic_curmode == IEEE80211_MODE_11NG)
		raid = R92C_RAID_11GN;
#endif
	/* NB: group addressed frames are done at 11bg rates for now */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	/* XXX misleading 'mode' value here for unicast frames */
	URTWN_DPRINTF(sc, URTWN_DEBUG_RA,
	    "%s: mode 0x%x, rates 0x%08x, basicrates 0x%08x\n", __func__,
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
	URTWN_DPRINTF(sc, URTWN_DEBUG_RA, "%s: maxbasicrate %d\n", __func__,
	    maxbasicrate);
	urtwn_write_1(sc, R92C_INIDATA_RATE_SEL(URTWN_MACID_BC),
	    maxbasicrate);

	/* Set rates mask for unicast frames. */
	if (ni->ni_flags & IEEE80211_NODE_HT)
		mode = R92C_RAID_11GN;
	else if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	cmd.macid = URTWN_MACID_BSS | URTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | rates);
	error = urtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		ieee80211_free_node(ni);
		device_printf(sc->sc_dev, "could not add BSS station\n");
		return (error);
	}
	/* Set initial MRR rate. */
	URTWN_DPRINTF(sc, URTWN_DEBUG_RA, "%s: maxrate %d\n", __func__,
	    maxrate);
	urtwn_write_1(sc, R92C_INIDATA_RATE_SEL(URTWN_MACID_BSS),
	    maxrate);

	/* Indicate highest supported rate. */
	if (ni->ni_flags & IEEE80211_NODE_HT)
		ni->ni_txrate = rs_ht->rs_rates[rs_ht->rs_nrates - 1]
		    | IEEE80211_RATE_MCS;
	else
		ni->ni_txrate = rs->rs_rates[rs->rs_nrates - 1];
	ieee80211_free_node(ni);

	return (0);
}

static void
urtwn_init_beacon(struct urtwn_softc *sc, struct urtwn_vap *uvp)
{
	struct r92c_tx_desc *txd = &uvp->bcn_desc;

	txd->txdw0 = htole32(
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) | R92C_TXDW0_BMCAST |
	    R92C_TXDW0_OWN | R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	txd->txdw1 = htole32(
	    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_BEACON) |
	    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

	if (sc->chip & URTWN_CHIP_88E) {
		txd->txdw1 |= htole32(SM(R88E_TXDW1_MACID, URTWN_MACID_BC));
		txd->txdseq |= htole16(R88E_TXDSEQ_HWSEQ_EN);
	} else {
		txd->txdw1 |= htole32(SM(R92C_TXDW1_MACID, URTWN_MACID_BC));
		txd->txdw4 |= htole32(R92C_TXDW4_HWSEQ_EN);
	}

	txd->txdw4 = htole32(R92C_TXDW4_DRVRATE);
	txd->txdw5 = htole32(SM(R92C_TXDW5_DATARATE, URTWN_RIDX_CCK1));
}

static int
urtwn_setup_beacon(struct urtwn_softc *sc, struct ieee80211_node *ni)
{
 	struct ieee80211vap *vap = ni->ni_vap;
	struct urtwn_vap *uvp = URTWN_VAP(vap);
	struct mbuf *m;
	int error;

	URTWN_ASSERT_LOCKED(sc);

	if (ni->ni_chan == IEEE80211_CHAN_ANYC)
		return (EINVAL);

	m = ieee80211_beacon_alloc(ni);
	if (m == NULL) {
		device_printf(sc->sc_dev,
		    "%s: could not allocate beacon frame\n", __func__);
		return (ENOMEM);
	}

	if (uvp->bcn_mbuf != NULL)
		m_freem(uvp->bcn_mbuf);

	uvp->bcn_mbuf = m;

	if ((error = urtwn_tx_beacon(sc, uvp)) != 0)
		return (error);

	/* XXX bcnq stuck workaround */
	if ((error = urtwn_tx_beacon(sc, uvp)) != 0)
		return (error);

	URTWN_DPRINTF(sc, URTWN_DEBUG_BEACON, "%s: beacon was %srecognized\n",
	    __func__, urtwn_read_1(sc, R92C_TDECTRL + 2) &
	    (R92C_TDECTRL_BCN_VALID >> 16) ? "" : "not ");

	return (0);
}

static void
urtwn_update_beacon(struct ieee80211vap *vap, int item)
{
	struct urtwn_softc *sc = vap->iv_ic->ic_softc;
	struct urtwn_vap *uvp = URTWN_VAP(vap);
	struct ieee80211_beacon_offsets *bo = &vap->iv_bcn_off;
	struct ieee80211_node *ni = vap->iv_bss;
	int mcast = 0;

	URTWN_LOCK(sc);
	if (uvp->bcn_mbuf == NULL) {
		uvp->bcn_mbuf = ieee80211_beacon_alloc(ni);
		if (uvp->bcn_mbuf == NULL) {
			device_printf(sc->sc_dev,
			    "%s: could not allocate beacon frame\n", __func__);
			URTWN_UNLOCK(sc);
			return;
		}
	}
	URTWN_UNLOCK(sc);

	if (item == IEEE80211_BEACON_TIM)
		mcast = 1;	/* XXX */

	setbit(bo->bo_flags, item);
	ieee80211_beacon_update(ni, uvp->bcn_mbuf, mcast);

	URTWN_LOCK(sc);
	urtwn_tx_beacon(sc, uvp);
	URTWN_UNLOCK(sc);
}

/*
 * Push a beacon frame into the chip. Beacon will
 * be repeated by the chip every R92C_BCN_INTERVAL.
 */
static int
urtwn_tx_beacon(struct urtwn_softc *sc, struct urtwn_vap *uvp)
{
	struct r92c_tx_desc *desc = &uvp->bcn_desc;
	struct urtwn_data *bf;

	URTWN_ASSERT_LOCKED(sc);

	bf = urtwn_getbuf(sc);
	if (bf == NULL)
		return (ENOMEM);

	memcpy(bf->buf, desc, sizeof(*desc));
	urtwn_tx_start(sc, uvp->bcn_mbuf, IEEE80211_FC0_TYPE_MGT, bf);

	sc->sc_txtimer = 5;
	callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);

	return (0);
}

static int
urtwn_key_alloc(struct ieee80211vap *vap, struct ieee80211_key *k,
    ieee80211_keyix *keyix, ieee80211_keyix *rxkeyix)
{
	struct urtwn_softc *sc = vap->iv_ic->ic_softc;
	uint8_t i;

	if (!(&vap->iv_nw_keys[0] <= k &&
	     k < &vap->iv_nw_keys[IEEE80211_WEP_NKID])) {
		if (!(k->wk_flags & IEEE80211_KEY_SWCRYPT)) {
			URTWN_LOCK(sc);
			/*
			 * First 4 slots for group keys,
			 * what is left - for pairwise.
			 * XXX incompatible with IBSS RSN.
			 */
			for (i = IEEE80211_WEP_NKID;
			     i < R92C_CAM_ENTRY_COUNT; i++) {
				if ((sc->keys_bmap & (1 << i)) == 0) {
					sc->keys_bmap |= 1 << i;
					*keyix = i;
					break;
				}
			}
			URTWN_UNLOCK(sc);
			if (i == R92C_CAM_ENTRY_COUNT) {
				device_printf(sc->sc_dev,
				    "%s: no free space in the key table\n",
				    __func__);
				return 0;
			}
		} else
			*keyix = 0;
	} else {
		*keyix = k - vap->iv_nw_keys;
	}
	*rxkeyix = *keyix;
	return 1;
}

static void
urtwn_key_set_cb(struct urtwn_softc *sc, union sec_param *data)
{
	struct ieee80211_key *k = &data->key;
	uint8_t algo, keyid;
	int i, error;

	if (k->wk_keyix < IEEE80211_WEP_NKID)
		keyid = k->wk_keyix;
	else
		keyid = 0;

	/* Map net80211 cipher to HW crypto algorithm. */
	switch (k->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_WEP:
		if (k->wk_keylen < 8)
			algo = R92C_CAM_ALGO_WEP40;
		else
			algo = R92C_CAM_ALGO_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		algo = R92C_CAM_ALGO_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		algo = R92C_CAM_ALGO_AES;
		break;
	default:
		device_printf(sc->sc_dev, "%s: undefined cipher %d\n",
		    __func__, k->wk_cipher->ic_cipher);
		return;
	}

	URTWN_DPRINTF(sc, URTWN_DEBUG_KEY,
	    "%s: keyix %d, keyid %d, algo %d/%d, flags %04X, len %d, "
	    "macaddr %s\n", __func__, k->wk_keyix, keyid,
	    k->wk_cipher->ic_cipher, algo, k->wk_flags, k->wk_keylen,
	    ether_sprintf(k->wk_macaddr));

	/* Clear high bits. */
	urtwn_cam_write(sc, R92C_CAM_CTL6(k->wk_keyix), 0);
	urtwn_cam_write(sc, R92C_CAM_CTL7(k->wk_keyix), 0);

	/* Write key. */
	for (i = 0; i < 4; i++) {
		error = urtwn_cam_write(sc, R92C_CAM_KEY(k->wk_keyix, i),
		    le32dec(&k->wk_key[i * 4]));
		if (error != 0)
			goto fail;
	}

	/* Write CTL0 last since that will validate the CAM entry. */
	error = urtwn_cam_write(sc, R92C_CAM_CTL1(k->wk_keyix),
	    le32dec(&k->wk_macaddr[2]));
	if (error != 0)
		goto fail;
	error = urtwn_cam_write(sc, R92C_CAM_CTL0(k->wk_keyix),
	    SM(R92C_CAM_ALGO, algo) |
	    SM(R92C_CAM_KEYID, keyid) |
	    SM(R92C_CAM_MACLO, le16dec(&k->wk_macaddr[0])) |
	    R92C_CAM_VALID);
	if (error != 0)
		goto fail;

	return;

fail:
	device_printf(sc->sc_dev, "%s fails, error %d\n", __func__, error);
}

static void
urtwn_key_del_cb(struct urtwn_softc *sc, union sec_param *data)
{
	struct ieee80211_key *k = &data->key;
	int i;

	URTWN_DPRINTF(sc, URTWN_DEBUG_KEY,
	    "%s: keyix %d, flags %04X, macaddr %s\n", __func__,
	    k->wk_keyix, k->wk_flags, ether_sprintf(k->wk_macaddr));

	urtwn_cam_write(sc, R92C_CAM_CTL0(k->wk_keyix), 0);
	urtwn_cam_write(sc, R92C_CAM_CTL1(k->wk_keyix), 0);

	/* Clear key. */
	for (i = 0; i < 4; i++)
		urtwn_cam_write(sc, R92C_CAM_KEY(k->wk_keyix, i), 0);
	sc->keys_bmap &= ~(1 << k->wk_keyix);
}

static int
urtwn_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct urtwn_softc *sc = vap->iv_ic->ic_softc;
	struct urtwn_vap *uvp = URTWN_VAP(vap);

	if (k->wk_flags & IEEE80211_KEY_SWCRYPT) {
		/* Not for us. */
		return (1);
	}

	if (&vap->iv_nw_keys[0] <= k &&
	    k < &vap->iv_nw_keys[IEEE80211_WEP_NKID]) {
		URTWN_LOCK(sc);
		uvp->keys[k->wk_keyix] = k;
		if ((sc->sc_flags & URTWN_RUNNING) == 0) {
			/*
			 * The device was not started;
			 * the key will be installed later.
			 */
			URTWN_UNLOCK(sc);
			return (1);
		}
		URTWN_UNLOCK(sc);
	}

	return (!urtwn_cmd_sleepable(sc, k, sizeof(*k), urtwn_key_set_cb));
}

static int
urtwn_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct urtwn_softc *sc = vap->iv_ic->ic_softc;
	struct urtwn_vap *uvp = URTWN_VAP(vap);

	if (k->wk_flags & IEEE80211_KEY_SWCRYPT) {
		/* Not for us. */
		return (1);
	}

	if (&vap->iv_nw_keys[0] <= k &&
	    k < &vap->iv_nw_keys[IEEE80211_WEP_NKID]) {
		URTWN_LOCK(sc);                  
		uvp->keys[k->wk_keyix] = NULL;
		if ((sc->sc_flags & URTWN_RUNNING) == 0) {
			/* All keys are removed on device reset. */
			URTWN_UNLOCK(sc);
			return (1);
		}
		URTWN_UNLOCK(sc);
	}

	return (!urtwn_cmd_sleepable(sc, k, sizeof(*k), urtwn_key_del_cb));
}

static void
urtwn_tsf_task_adhoc(void *arg, int pending)
{
	struct ieee80211vap *vap = arg;
	struct urtwn_softc *sc = vap->iv_ic->ic_softc;
	struct ieee80211_node *ni;
	uint32_t reg;

	URTWN_LOCK(sc);
	ni = ieee80211_ref_node(vap->iv_bss);
	reg = urtwn_read_1(sc, R92C_BCN_CTRL);

	/* Accept beacons with the same BSSID. */
	urtwn_set_rx_bssid_all(sc, 0);

	/* Enable synchronization. */
	reg &= ~R92C_BCN_CTRL_DIS_TSF_UDT0;
	urtwn_write_1(sc, R92C_BCN_CTRL, reg);

	/* Synchronize. */
	usb_pause_mtx(&sc->sc_mtx, hz * ni->ni_intval * 5 / 1000);

	/* Disable synchronization. */
	reg |= R92C_BCN_CTRL_DIS_TSF_UDT0;
	urtwn_write_1(sc, R92C_BCN_CTRL, reg);

	/* Remove beacon filter. */
	urtwn_set_rx_bssid_all(sc, 1);

	/* Enable beaconing. */
	urtwn_write_1(sc, R92C_MBID_NUM,
	    urtwn_read_1(sc, R92C_MBID_NUM) | R92C_MBID_TXBCN_RPT0);
	reg |= R92C_BCN_CTRL_EN_BCN;

	urtwn_write_1(sc, R92C_BCN_CTRL, reg);
	ieee80211_free_node(ni);
	URTWN_UNLOCK(sc);
}

static void
urtwn_tsf_sync_enable(struct urtwn_softc *sc, struct ieee80211vap *vap)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct urtwn_vap *uvp = URTWN_VAP(vap);

	/* Reset TSF. */
	urtwn_write_1(sc, R92C_DUAL_TSF_RST, R92C_DUAL_TSF_RST0);

	switch (vap->iv_opmode) {
	case IEEE80211_M_STA:
		/* Enable TSF synchronization. */
		urtwn_write_1(sc, R92C_BCN_CTRL,
		    urtwn_read_1(sc, R92C_BCN_CTRL) &
		    ~R92C_BCN_CTRL_DIS_TSF_UDT0);
		break;
	case IEEE80211_M_IBSS:
		ieee80211_runtask(ic, &uvp->tsf_task_adhoc);
		break;
	case IEEE80211_M_HOSTAP:
		/* Enable beaconing. */
		urtwn_write_1(sc, R92C_MBID_NUM,
		    urtwn_read_1(sc, R92C_MBID_NUM) | R92C_MBID_TXBCN_RPT0);
		urtwn_write_1(sc, R92C_BCN_CTRL,
		    urtwn_read_1(sc, R92C_BCN_CTRL) | R92C_BCN_CTRL_EN_BCN);
		break;
	default:
		device_printf(sc->sc_dev, "undefined opmode %d\n",
		    vap->iv_opmode);
		return;
	}
}

static void
urtwn_get_tsf(struct urtwn_softc *sc, uint64_t *buf)
{
	urtwn_read_region_1(sc, R92C_TSFTR, (uint8_t *)buf, sizeof(*buf));
}

static void
urtwn_set_led(struct urtwn_softc *sc, int led, int on)
{
	uint8_t reg;

	if (led == URTWN_LED_LINK) {
		if (sc->chip & URTWN_CHIP_88E) {
			reg = urtwn_read_1(sc, R92C_LEDCFG2) & 0xf0;
			urtwn_write_1(sc, R92C_LEDCFG2, reg | 0x60);
			if (!on) {
				reg = urtwn_read_1(sc, R92C_LEDCFG2) & 0x90;
				urtwn_write_1(sc, R92C_LEDCFG2,
				    reg | R92C_LEDCFG0_DIS);
				urtwn_write_1(sc, R92C_MAC_PINMUX_CFG,
				    urtwn_read_1(sc, R92C_MAC_PINMUX_CFG) &
				    0xfe);
			}
		} else {
			reg = urtwn_read_1(sc, R92C_LEDCFG0) & 0x70;
			if (!on)
				reg |= R92C_LEDCFG0_DIS;
			urtwn_write_1(sc, R92C_LEDCFG0, reg);
		}
		sc->ledlink = on;       /* Save LED state. */
	}
}

static void
urtwn_set_mode(struct urtwn_softc *sc, uint8_t mode)
{
	uint8_t reg;

	reg = urtwn_read_1(sc, R92C_MSR);
	reg = (reg & ~R92C_MSR_MASK) | mode;
	urtwn_write_1(sc, R92C_MSR, reg);
}

static void
urtwn_ibss_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m, int subtype,
    const struct ieee80211_rx_stats *rxs,
    int rssi, int nf)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct urtwn_softc *sc = vap->iv_ic->ic_softc;
	struct urtwn_vap *uvp = URTWN_VAP(vap);
	uint64_t ni_tstamp, curr_tstamp;

	uvp->recv_mgmt(ni, m, subtype, rxs, rssi, nf);

	if (vap->iv_state == IEEE80211_S_RUN &&
	    (subtype == IEEE80211_FC0_SUBTYPE_BEACON ||
	    subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)) {
		ni_tstamp = le64toh(ni->ni_tstamp.tsf);
		URTWN_LOCK(sc);
		urtwn_get_tsf(sc, &curr_tstamp);
		URTWN_UNLOCK(sc);
		curr_tstamp = le64toh(curr_tstamp);

		if (ni_tstamp >= curr_tstamp)
			(void) ieee80211_ibss_merge(ni);
	}
}

static int
urtwn_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct urtwn_vap *uvp = URTWN_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct urtwn_softc *sc = ic->ic_softc;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;
	uint32_t reg;
	uint8_t mode;
	int error = 0;

	ostate = vap->iv_state;
	URTWN_DPRINTF(sc, URTWN_DEBUG_STATE, "%s -> %s\n",
	    ieee80211_state_name[ostate], ieee80211_state_name[nstate]);

	IEEE80211_UNLOCK(ic);
	URTWN_LOCK(sc);
	callout_stop(&sc->sc_watchdog_ch);

	if (ostate == IEEE80211_S_RUN) {
		/* Stop calibration. */
		callout_stop(&sc->sc_calib_to);

		/* Turn link LED off. */
		urtwn_set_led(sc, URTWN_LED_LINK, 0);

		/* Set media status to 'No Link'. */
		urtwn_set_mode(sc, R92C_MSR_NOLINK);

		/* Stop Rx of data frames. */
		urtwn_write_2(sc, R92C_RXFLTMAP2, 0);

		/* Disable TSF synchronization. */
		urtwn_write_1(sc, R92C_BCN_CTRL,
		    (urtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_EN_BCN) |
		    R92C_BCN_CTRL_DIS_TSF_UDT0);

		/* Disable beaconing. */
		urtwn_write_1(sc, R92C_MBID_NUM,
		    urtwn_read_1(sc, R92C_MBID_NUM) & ~R92C_MBID_TXBCN_RPT0);

		/* Reset TSF. */
		urtwn_write_1(sc, R92C_DUAL_TSF_RST, R92C_DUAL_TSF_RST0);

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
		/* Pause AC Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE,
		    urtwn_read_1(sc, R92C_TXPAUSE) | R92C_TX_QUEUE_AC);
		break;
	case IEEE80211_S_AUTH:
		urtwn_set_chan(sc, ic->ic_curchan, NULL);
		break;
	case IEEE80211_S_RUN:
		if (vap->iv_opmode == IEEE80211_M_MONITOR) {
			/* Turn link LED on. */
			urtwn_set_led(sc, URTWN_LED_LINK, 1);
			break;
		}

		ni = ieee80211_ref_node(vap->iv_bss);

		if (ic->ic_bsschan == IEEE80211_CHAN_ANYC ||
		    ni->ni_chan == IEEE80211_CHAN_ANYC) {
			device_printf(sc->sc_dev,
			    "%s: could not move to RUN state\n", __func__);
			error = EINVAL;
			goto end_run;
		}

		switch (vap->iv_opmode) {
		case IEEE80211_M_STA:
			mode = R92C_MSR_INFRA;
			break;
		case IEEE80211_M_IBSS:
			mode = R92C_MSR_ADHOC;
			break;
		case IEEE80211_M_HOSTAP:
			mode = R92C_MSR_AP;
			break;
		default:
			device_printf(sc->sc_dev, "undefined opmode %d\n",
			    vap->iv_opmode);
			error = EINVAL;
			goto end_run;
		}

		/* Set media status to 'Associated'. */
		urtwn_set_mode(sc, mode);

		/* Set BSSID. */
		urtwn_write_4(sc, R92C_BSSID + 0, le32dec(&ni->ni_bssid[0]));
		urtwn_write_4(sc, R92C_BSSID + 4, le16dec(&ni->ni_bssid[4]));

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
		if (ic->ic_promisc == 0) {
			reg = urtwn_read_4(sc, R92C_RCR);

			if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
				reg |= R92C_RCR_CBSSID_DATA;
				if (vap->iv_opmode != IEEE80211_M_IBSS)
					reg |= R92C_RCR_CBSSID_BCN;
			}

			urtwn_write_4(sc, R92C_RCR, reg);
		}

		if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
		    vap->iv_opmode == IEEE80211_M_IBSS) {
			error = urtwn_setup_beacon(sc, ni);
			if (error != 0) {
				device_printf(sc->sc_dev,
				    "unable to push beacon into the chip, "
				    "error %d\n", error);
				goto end_run;
			}
		}

		/* Enable TSF synchronization. */
		urtwn_tsf_sync_enable(sc, vap);

		urtwn_write_1(sc, R92C_SIFS_CCK + 1, 10);
		urtwn_write_1(sc, R92C_SIFS_OFDM + 1, 10);
		urtwn_write_1(sc, R92C_SPEC_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_MAC_SPEC_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_R2T_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_T2T_SIFS + 1, 10);

		/* Intialize rate adaptation. */
		if (!(sc->chip & URTWN_CHIP_88E))
			urtwn_ra_init(sc);
		/* Turn link LED on. */
		urtwn_set_led(sc, URTWN_LED_LINK, 1);

		sc->avg_pwdb = -1;	/* Reset average RSSI. */
		/* Reset temperature calibration state machine. */
		sc->sc_flags &= ~URTWN_TEMP_MEASURED;
		sc->thcal_lctemp = 0;
		/* Start periodic calibration. */
		callout_reset(&sc->sc_calib_to, 2*hz, urtwn_calib_to, sc);

end_run:
		ieee80211_free_node(ni);
		break;
	default:
		break;
	}

	URTWN_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return (error != 0 ? error : uvp->newstate(vap, nstate, arg));
}

static void
urtwn_calib_to(void *arg)
{
	struct urtwn_softc *sc = arg;

	/* Do it in a process context. */
	urtwn_cmd_sleepable(sc, NULL, 0, urtwn_calib_cb);
}

static void
urtwn_calib_cb(struct urtwn_softc *sc, union sec_param *data)
{
	/* Do temperature compensation. */
	urtwn_temp_calib(sc);

	if ((urtwn_read_1(sc, R92C_MSR) & R92C_MSR_MASK) != R92C_MSR_NOLINK)
		callout_reset(&sc->sc_calib_to, 2*hz, urtwn_calib_to, sc);
}

static void
urtwn_watchdog(void *arg)
{
	struct urtwn_softc *sc = arg;

	if (sc->sc_txtimer > 0) {
		if (--sc->sc_txtimer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			counter_u64_add(sc->sc_ic.ic_oerrors, 1);
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
	if (!(sc->chip & URTWN_CHIP_88E)) {
		if (rate <= URTWN_RIDX_CCK11) {
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
	}
	if (sc->avg_pwdb == -1)	/* Init. */
		sc->avg_pwdb = pwdb;
	else if (sc->avg_pwdb < pwdb)
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20) + 1;
	else
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20);
	URTWN_DPRINTF(sc, URTWN_DEBUG_RSSI, "%s: PWDB %d, EMA %d\n", __func__,
	    pwdb, sc->avg_pwdb);
}

static int8_t
urtwn_get_rssi(struct urtwn_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 16, -12, -26, -46 };
	struct r92c_rx_phystat *phy;
	struct r92c_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	if (rate <= URTWN_RIDX_CCK11) {
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

static int8_t
urtwn_r88e_get_rssi(struct urtwn_softc *sc, int rate, void *physt)
{
	struct r92c_rx_phystat *phy;
	struct r88e_rx_cck *cck;
	uint8_t cck_agc_rpt, lna_idx, vga_idx;
	int8_t rssi;

	rssi = 0;
	if (rate <= URTWN_RIDX_CCK11) {
		cck = (struct r88e_rx_cck *)physt;
		cck_agc_rpt = cck->agc_rpt;
		lna_idx = (cck_agc_rpt & 0xe0) >> 5;
		vga_idx = cck_agc_rpt & 0x1f;
		switch (lna_idx) {
		case 7:
			if (vga_idx <= 27)
				rssi = -100 + 2* (27 - vga_idx);
			else
				rssi = -100;
			break;
		case 6:
			rssi = -48 + 2 * (2 - vga_idx);
			break;
		case 5:
			rssi = -42 + 2 * (7 - vga_idx);
			break;
		case 4:
			rssi = -36 + 2 * (7 - vga_idx);
			break;
		case 3:
			rssi = -24 + 2 * (7 - vga_idx);
			break;
		case 2:
			rssi = -12 + 2 * (5 - vga_idx);
			break;
		case 1:
			rssi = 8 - (2 * vga_idx);
			break;
		case 0:
			rssi = 14 - (2 * vga_idx);
			break;
		}
		rssi += 6;
	} else {	/* OFDM/HT. */
		phy = (struct r92c_rx_phystat *)physt;
		rssi = ((le32toh(phy->phydw1) >> 1) & 0x7f) - 110;
	}
	return (rssi);
}

static int
urtwn_tx_data(struct urtwn_softc *sc, struct ieee80211_node *ni,
    struct mbuf *m, struct urtwn_data *data)
{
	const struct ieee80211_txparam *tp;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_key *k = NULL;
	struct ieee80211_channel *chan;
	struct ieee80211_frame *wh;
	struct r92c_tx_desc *txd;
	uint8_t macid, raid, rate, ridx, type, tid, qos, qsel;
	int hasqos, ismcast;

	URTWN_ASSERT_LOCKED(sc);

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	hasqos = IEEE80211_QOS_HAS_SEQ(wh);
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);

	/* Select TX ring for this frame. */
	if (hasqos) {
		qos = ((const struct ieee80211_qosframe *)wh)->i_qos[0];
		tid = qos & IEEE80211_QOS_TID;
	} else {
		qos = 0;
		tid = 0;
	}

	chan = (ni->ni_chan != IEEE80211_CHAN_ANYC) ?
		ni->ni_chan : ic->ic_curchan;
	tp = &vap->iv_txparms[ieee80211_chan2mode(chan)];

	/* Choose a TX rate index. */
	if (type == IEEE80211_FC0_TYPE_MGT)
		rate = tp->mgmtrate;
	else if (ismcast)
		rate = tp->mcastrate;
	else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rate = tp->ucastrate;
	else if (m->m_flags & M_EAPOL)
		rate = tp->mgmtrate;
	else {
		if (URTWN_CHIP_HAS_RATECTL(sc)) {
			/* XXX pass pktlen */
			(void) ieee80211_ratectl_rate(ni, NULL, 0);
			rate = ni->ni_txrate;
		} else {
			/* XXX TODO: drop the default rate for 11b/11g? */
			if (ni->ni_flags & IEEE80211_NODE_HT)
				rate = IEEE80211_RATE_MCS | 0x4; /* MCS4 */
			else if (ic->ic_curmode != IEEE80211_MODE_11B)
				rate = 108;
			else
				rate = 22;
		}
	}

	/*
	 * XXX TODO: this should be per-node, for 11b versus 11bg
	 * nodes in hostap mode
	 */
	ridx = rate2ridx(rate);
	if (ni->ni_flags & IEEE80211_NODE_HT)
		raid = R92C_RAID_11GN;
	else if (ic->ic_curmode != IEEE80211_MODE_11B)
		raid = R92C_RAID_11BG;
	else
		raid = R92C_RAID_11B;

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			device_printf(sc->sc_dev,
			    "ieee80211_crypto_encap returns NULL.\n");
			return (ENOBUFS);
		}

		/* in case packet header moved, reset pointer */
		wh = mtod(m, struct ieee80211_frame *);
	}

	/* Fill Tx descriptor. */
	txd = (struct r92c_tx_desc *)data->buf;
	memset(txd, 0, sizeof(*txd));

	txd->txdw0 |= htole32(
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_OWN | R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (ismcast)
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

	if (!ismcast) {
		/* Unicast frame, check if an ACK is expected. */
		if (!qos || (qos & IEEE80211_QOS_ACKPOLICY) !=
		    IEEE80211_QOS_ACKPOLICY_NOACK) {
			txd->txdw5 |= htole32(R92C_TXDW5_RTY_LMT_ENA);
			txd->txdw5 |= htole32(SM(R92C_TXDW5_RTY_LMT,
			    tp->maxretry));
		}

		if (sc->chip & URTWN_CHIP_88E) {
			struct urtwn_node *un = URTWN_NODE(ni);
			macid = un->id;
		} else
			macid = URTWN_MACID_BSS;

		if (type == IEEE80211_FC0_TYPE_DATA) {
			qsel = tid % URTWN_MAX_TID;

			if (sc->chip & URTWN_CHIP_88E) {
				txd->txdw2 |= htole32(
				    R88E_TXDW2_AGGBK |
				    R88E_TXDW2_CCX_RPT);
			} else
				txd->txdw1 |= htole32(R92C_TXDW1_AGGBK);

			/* protmode, non-HT */
			/* XXX TODO: noack frames? */
			if ((rate & 0x80) == 0 &&
			    (ic->ic_flags & IEEE80211_F_USEPROT)) {
				switch (ic->ic_protmode) {
				case IEEE80211_PROT_CTSONLY:
					txd->txdw4 |= htole32(
					    R92C_TXDW4_CTS2SELF);
					break;
				case IEEE80211_PROT_RTSCTS:
					txd->txdw4 |= htole32(
					    R92C_TXDW4_RTSEN |
					    R92C_TXDW4_HWRTSEN);
					break;
				default:
					break;
				}
			}

			/* protmode, HT */
			/* XXX TODO: noack frames? */
			if ((rate & 0x80) &&
			    (ic->ic_htprotmode == IEEE80211_PROT_RTSCTS)) {
				txd->txdw4 |= htole32(
				    R92C_TXDW4_RTSEN |
				    R92C_TXDW4_HWRTSEN);
			}

			/* XXX TODO: rtsrate is configurable? 24mbit may
			 * be a bit high for RTS rate? */
			txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE,
			    URTWN_RIDX_OFDM24));

			txd->txdw5 |= htole32(0x0001ff00);
		} else	/* IEEE80211_FC0_TYPE_MGT */
			qsel = R92C_TXDW1_QSEL_MGNT;
	} else {
		macid = URTWN_MACID_BC;
		qsel = R92C_TXDW1_QSEL_MGNT;
	}

	txd->txdw1 |= htole32(
	    SM(R92C_TXDW1_QSEL, qsel) |
	    SM(R92C_TXDW1_RAID, raid));

	/* XXX TODO: 40MHZ flag? */
	/* XXX TODO: AMPDU flag? (AGG_ENABLE or AGG_BREAK?) Density shift? */
	/* XXX Short preamble? */
	/* XXX Short-GI? */

	if (sc->chip & URTWN_CHIP_88E)
		txd->txdw1 |= htole32(SM(R88E_TXDW1_MACID, macid));
	else
		txd->txdw1 |= htole32(SM(R92C_TXDW1_MACID, macid));

	txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, ridx));

	/* Force this rate if needed. */
	if (URTWN_CHIP_HAS_RATECTL(sc) || ismcast ||
	    (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) ||
	    (m->m_flags & M_EAPOL) || type != IEEE80211_FC0_TYPE_DATA)
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);

	if (!hasqos) {
		/* Use HW sequence numbering for non-QoS frames. */
		if (sc->chip & URTWN_CHIP_88E)
			txd->txdseq = htole16(R88E_TXDSEQ_HWSEQ_EN);
		else
			txd->txdw4 |= htole32(R92C_TXDW4_HWSEQ_EN);
	} else {
		/* Set sequence number. */
		txd->txdseq = htole16(M_SEQNO_GET(m) % IEEE80211_SEQ_RANGE);
	}

	if (k != NULL && !(k->wk_flags & IEEE80211_KEY_SWCRYPT)) {
		uint8_t cipher;

		switch (k->wk_cipher->ic_cipher) {
		case IEEE80211_CIPHER_WEP:
		case IEEE80211_CIPHER_TKIP:
			cipher = R92C_TXDW1_CIPHER_RC4;
			break;
		case IEEE80211_CIPHER_AES_CCM:
			cipher = R92C_TXDW1_CIPHER_AES;
			break;
		default:
			device_printf(sc->sc_dev, "%s: unknown cipher %d\n",
			    __func__, k->wk_cipher->ic_cipher);
			return (EINVAL);
		}

		txd->txdw1 |= htole32(SM(R92C_TXDW1_CIPHER, cipher));
	}

	if (ieee80211_radiotap_active_vap(vap)) {
		struct urtwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		if (k != NULL)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		ieee80211_radiotap_tx(vap, m);
	}

	data->ni = ni;

	urtwn_tx_start(sc, m, type, data);

	return (0);
}

static int
urtwn_tx_raw(struct urtwn_softc *sc, struct ieee80211_node *ni,
    struct mbuf *m, struct urtwn_data *data,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_key *k = NULL;
	struct ieee80211_frame *wh;
	struct r92c_tx_desc *txd;
	uint8_t cipher, ridx, type;

	/* Encrypt the frame if need be. */
	cipher = R92C_TXDW1_CIPHER_NONE;
	if (params->ibp_flags & IEEE80211_BPF_CRYPTO) {
		/* Retrieve key for TX. */
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL)
			return (ENOBUFS);

		if (!(k->wk_flags & IEEE80211_KEY_SWCRYPT)) {
			switch (k->wk_cipher->ic_cipher) {
			case IEEE80211_CIPHER_WEP:
			case IEEE80211_CIPHER_TKIP:
				cipher = R92C_TXDW1_CIPHER_RC4;
				break;
			case IEEE80211_CIPHER_AES_CCM:
				cipher = R92C_TXDW1_CIPHER_AES;
				break;
			default:
				device_printf(sc->sc_dev,
				    "%s: unknown cipher %d\n",
				    __func__, k->wk_cipher->ic_cipher);
				return (EINVAL);
			}
		}
	}

	/* XXX TODO: 11n checks, matching urtwn_tx_data() */

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/* Fill Tx descriptor. */
	txd = (struct r92c_tx_desc *)data->buf;
	memset(txd, 0, sizeof(*txd));

	txd->txdw0 |= htole32(
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_OWN | R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0) {
		txd->txdw5 |= htole32(R92C_TXDW5_RTY_LMT_ENA);
		txd->txdw5 |= htole32(SM(R92C_TXDW5_RTY_LMT,
		    params->ibp_try0));
	}
	if (params->ibp_flags & IEEE80211_BPF_RTS)
		txd->txdw4 |= htole32(R92C_TXDW4_RTSEN | R92C_TXDW4_HWRTSEN);
	if (params->ibp_flags & IEEE80211_BPF_CTS)
		txd->txdw4 |= htole32(R92C_TXDW4_CTS2SELF);
	if (txd->txdw4 & htole32(R92C_TXDW4_RTSEN | R92C_TXDW4_CTS2SELF)) {
		txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE,
		    URTWN_RIDX_OFDM24));
	}

	if (sc->chip & URTWN_CHIP_88E)
		txd->txdw1 |= htole32(SM(R88E_TXDW1_MACID, URTWN_MACID_BC));
	else
		txd->txdw1 |= htole32(SM(R92C_TXDW1_MACID, URTWN_MACID_BC));

	/* XXX TODO: rate index/config (RAID) for 11n? */
	txd->txdw1 |= htole32(SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_MGNT));
	txd->txdw1 |= htole32(SM(R92C_TXDW1_CIPHER, cipher));

	/* Choose a TX rate index. */
	ridx = rate2ridx(params->ibp_rate0);
	txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, ridx));
	txd->txdw5 |= htole32(0x0001ff00);
	txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);

	if (!IEEE80211_QOS_HAS_SEQ(wh)) {
		/* Use HW sequence numbering for non-QoS frames. */
		if (sc->chip & URTWN_CHIP_88E)
			txd->txdseq = htole16(R88E_TXDSEQ_HWSEQ_EN);
		else
			txd->txdw4 |= htole32(R92C_TXDW4_HWSEQ_EN);
	} else {
		/* Set sequence number. */
		txd->txdseq = htole16(M_SEQNO_GET(m) % IEEE80211_SEQ_RANGE);
	}

	if (ieee80211_radiotap_active_vap(vap)) {
		struct urtwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		if (k != NULL)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		ieee80211_radiotap_tx(vap, m);
	}

	data->ni = ni;

	urtwn_tx_start(sc, m, type, data);

	return (0);
}

static void
urtwn_tx_start(struct urtwn_softc *sc, struct mbuf *m, uint8_t type,
    struct urtwn_data *data)
{
	struct usb_xfer *xfer;
	struct r92c_tx_desc *txd;
	uint16_t ac, sum;
	int i, xferlen;

	URTWN_ASSERT_LOCKED(sc);

	ac = M_WME_GETAC(m);

	switch (type) {
	case IEEE80211_FC0_TYPE_CTL:
	case IEEE80211_FC0_TYPE_MGT:
		xfer = sc->sc_xfer[URTWN_BULK_TX_VO];
		break;
	default:
		xfer = sc->sc_xfer[wme2queue[ac].qid];
		break;
	}

	txd = (struct r92c_tx_desc *)data->buf;
	txd->txdw0 |= htole32(SM(R92C_TXDW0_PKTLEN, m->m_pkthdr.len));

	/* Compute Tx descriptor checksum. */
	sum = 0;
	for (i = 0; i < sizeof(*txd) / 2; i++)
		sum ^= ((uint16_t *)txd)[i];
	txd->txdsum = sum;	/* NB: already little endian. */

	xferlen = sizeof(*txd) + m->m_pkthdr.len;
	m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)&txd[1]);

	data->buflen = xferlen;
	data->m = m;

	STAILQ_INSERT_TAIL(&sc->sc_tx_pending, data, next);
	usbd_transfer_start(xfer);
}

static int
urtwn_transmit(struct ieee80211com *ic, struct mbuf *m)
{
	struct urtwn_softc *sc = ic->ic_softc;
	int error;

	URTWN_LOCK(sc);
	if ((sc->sc_flags & URTWN_RUNNING) == 0) {
		URTWN_UNLOCK(sc);
		return (ENXIO);
	}
	error = mbufq_enqueue(&sc->sc_snd, m);
	if (error) {
		URTWN_UNLOCK(sc);
		return (error);
	}
	urtwn_start(sc);
	URTWN_UNLOCK(sc);

	return (0);
}

static void
urtwn_start(struct urtwn_softc *sc)
{
	struct ieee80211_node *ni;
	struct mbuf *m;
	struct urtwn_data *bf;

	URTWN_ASSERT_LOCKED(sc);
	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		bf = urtwn_getbuf(sc);
		if (bf == NULL) {
			mbufq_prepend(&sc->sc_snd, m);
			break;
		}
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;

		URTWN_DPRINTF(sc, URTWN_DEBUG_XMIT, "%s: called; m=%p\n",
		    __func__,
		    m);

		if (urtwn_tx_data(sc, ni, m, bf) != 0) {
			if_inc_counter(ni->ni_vap->iv_ifp,
			    IFCOUNTER_OERRORS, 1);
			STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, bf, next);
			m_freem(m);
			ieee80211_free_node(ni);
			break;
		}
		sc->sc_txtimer = 5;
		callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);
	}
}

static void
urtwn_parent(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_softc;

	URTWN_LOCK(sc);
	if (sc->sc_flags & URTWN_DETACHED) {
		URTWN_UNLOCK(sc);
		return;
	}
	URTWN_UNLOCK(sc);

	if (ic->ic_nrunning > 0) {
		if (urtwn_init(sc) != 0) {
			struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
			if (vap != NULL)
				ieee80211_stop(vap);
		} else
			ieee80211_start_all(ic);
	} else
		urtwn_stop(sc);
}

static __inline int
urtwn_power_on(struct urtwn_softc *sc)
{

	return sc->sc_power_on(sc);
}

static int
urtwn_r92c_power_on(struct urtwn_softc *sc)
{
	uint32_t reg;
	usb_error_t error;
	int ntries;

	/* Wait for autoload done bit. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_1(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN)
			break;
		urtwn_ms_delay(sc);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for chip autoload\n");
		return (ETIMEDOUT);
	}

	/* Unlock ISO/CLK/Power control register. */
	error = urtwn_write_1(sc, R92C_RSV_CTRL, 0);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	/* Move SPS into PWM mode. */
	error = urtwn_write_1(sc, R92C_SPS0_CTRL, 0x2b);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	urtwn_ms_delay(sc);

	reg = urtwn_read_1(sc, R92C_LDOV12D_CTRL);
	if (!(reg & R92C_LDOV12D_CTRL_LDV12_EN)) {
		error = urtwn_write_1(sc, R92C_LDOV12D_CTRL,
		    reg | R92C_LDOV12D_CTRL_LDV12_EN);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
		urtwn_ms_delay(sc);
		error = urtwn_write_1(sc, R92C_SYS_ISO_CTRL,
		    urtwn_read_1(sc, R92C_SYS_ISO_CTRL) &
		    ~R92C_SYS_ISO_CTRL_MD2PP);
		if (error != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
	}

	/* Auto enable WLAN. */
	error = urtwn_write_2(sc, R92C_APS_FSMCO,
	    urtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(urtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC))
			break;
		urtwn_ms_delay(sc);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for MAC auto ON\n");
		return (ETIMEDOUT);
	}

	/* Enable radio, GPIO and LED functions. */
	error = urtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_HSUS |
	    R92C_APS_FSMCO_PDN_EN |
	    R92C_APS_FSMCO_PFM_ALDN);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	/* Release RF digital isolation. */
	error = urtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	    urtwn_read_2(sc, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	/* Initialize MAC. */
	error = urtwn_write_1(sc, R92C_APSD_CTRL,
	    urtwn_read_1(sc, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	for (ntries = 0; ntries < 200; ntries++) {
		if (!(urtwn_read_1(sc, R92C_APSD_CTRL) &
		    R92C_APSD_CTRL_OFF_STATUS))
			break;
		urtwn_ms_delay(sc);
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
	error = urtwn_write_2(sc, R92C_CR, reg);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	error = urtwn_write_1(sc, 0xfe10, 0x19);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	return (0);
}

static int
urtwn_r88e_power_on(struct urtwn_softc *sc)
{
	uint32_t reg;
	usb_error_t error;
	int ntries;

	/* Wait for power ready bit. */
	for (ntries = 0; ntries < 5000; ntries++) {
		if (urtwn_read_4(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_SUS_HOST)
			break;
		urtwn_ms_delay(sc);
	}
	if (ntries == 5000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for chip power up\n");
		return (ETIMEDOUT);
	}

	/* Reset BB. */
	error = urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_1(sc, R92C_SYS_FUNC_EN) & ~(R92C_SYS_FUNC_EN_BBRSTB |
	    R92C_SYS_FUNC_EN_BB_GLB_RST));
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	error = urtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 2,
	    urtwn_read_1(sc, R92C_AFE_XTAL_CTRL + 2) | 0x80);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	/* Disable HWPDN. */
	error = urtwn_write_2(sc, R92C_APS_FSMCO,
	    urtwn_read_2(sc, R92C_APS_FSMCO) & ~R92C_APS_FSMCO_APDM_HPDN);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	/* Disable WL suspend. */
	error = urtwn_write_2(sc, R92C_APS_FSMCO,
	    urtwn_read_2(sc, R92C_APS_FSMCO) &
	    ~(R92C_APS_FSMCO_AFSM_HSUS | R92C_APS_FSMCO_AFSM_PCIE));
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	error = urtwn_write_2(sc, R92C_APS_FSMCO,
	    urtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	for (ntries = 0; ntries < 5000; ntries++) {
		if (!(urtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC))
			break;
		urtwn_ms_delay(sc);
	}
	if (ntries == 5000)
		return (ETIMEDOUT);

	/* Enable LDO normal mode. */
	error = urtwn_write_1(sc, R92C_LPLDO_CTRL,
	    urtwn_read_1(sc, R92C_LPLDO_CTRL) & ~R92C_LPLDO_CTRL_SLEEP);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	error = urtwn_write_2(sc, R92C_CR, 0);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	reg = urtwn_read_2(sc, R92C_CR);
	reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_ENSEC | R92C_CR_CALTMR_EN;
	error = urtwn_write_2(sc, R92C_CR, reg);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	return (0);
}

static __inline void
urtwn_power_off(struct urtwn_softc *sc)
{

	return sc->sc_power_off(sc);
}

static void
urtwn_r92c_power_off(struct urtwn_softc *sc)
{
	uint32_t reg;

	/* Block all Tx queues. */
	urtwn_write_1(sc, R92C_TXPAUSE, R92C_TX_QUEUE_ALL);

	/* Disable RF */
	urtwn_rf_write(sc, 0, 0, 0);

	urtwn_write_1(sc, R92C_APSD_CTRL, R92C_APSD_CTRL_OFF);

	/* Reset BB state machine */
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_USBD | R92C_SYS_FUNC_EN_USBA |
	    R92C_SYS_FUNC_EN_BB_GLB_RST);
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_USBD | R92C_SYS_FUNC_EN_USBA);

	/*
	 * Reset digital sequence
	 */
#ifndef URTWN_WITHOUT_UCODE
	if (urtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RDY) {
		/* Reset MCU ready status */
		urtwn_write_1(sc, R92C_MCUFWDL, 0);

		/* If firmware in ram code, do reset */
		urtwn_fw_reset(sc);
	}
#endif

	/* Reset MAC and Enable 8051 */
	urtwn_write_1(sc, R92C_SYS_FUNC_EN + 1,
	    (R92C_SYS_FUNC_EN_CPUEN |
	     R92C_SYS_FUNC_EN_ELDR |
	     R92C_SYS_FUNC_EN_HWPDN) >> 8);

	/* Reset MCU ready status */
	urtwn_write_1(sc, R92C_MCUFWDL, 0);

	/* Disable MAC clock */
	urtwn_write_2(sc, R92C_SYS_CLKR,
	    R92C_SYS_CLKR_ANAD16V_EN |
	    R92C_SYS_CLKR_ANA8M |
	    R92C_SYS_CLKR_LOADER_EN | 
	    R92C_SYS_CLKR_80M_SSC_DIS |
	    R92C_SYS_CLKR_SYS_EN |
	    R92C_SYS_CLKR_RING_EN |
	    0x4000);

	/* Disable AFE PLL */
	urtwn_write_1(sc, R92C_AFE_PLL_CTRL, 0x80);

	/* Gated AFE DIG_CLOCK */
	urtwn_write_2(sc, R92C_AFE_XTAL_CTRL, 0x880F);

	/* Isolated digital to PON */
	urtwn_write_1(sc, R92C_SYS_ISO_CTRL,
	    R92C_SYS_ISO_CTRL_MD2PP |
	    R92C_SYS_ISO_CTRL_PA2PCIE |
	    R92C_SYS_ISO_CTRL_PD2CORE |
	    R92C_SYS_ISO_CTRL_IP2MAC |
	    R92C_SYS_ISO_CTRL_DIOP |
	    R92C_SYS_ISO_CTRL_DIOE);

	/*
	 * Pull GPIO PIN to balance level and LED control
	 */
	/* 1. Disable GPIO[7:0] */
	urtwn_write_2(sc, R92C_GPIO_IOSEL, 0x0000);

	reg = urtwn_read_4(sc, R92C_GPIO_PIN_CTRL) & ~0x0000ff00;
	reg |= ((reg << 8) & 0x0000ff00) | 0x00ff0000;
	urtwn_write_4(sc, R92C_GPIO_PIN_CTRL, reg);

	/* Disable GPIO[10:8] */
	urtwn_write_1(sc, R92C_MAC_PINMUX_CFG, 0x00);

	reg = urtwn_read_2(sc, R92C_GPIO_IO_SEL) & ~0x00f0;
	reg |= (((reg & 0x000f) << 4) | 0x0780);
	urtwn_write_2(sc, R92C_GPIO_IO_SEL, reg);

	/* Disable LED0 & 1 */
	urtwn_write_2(sc, R92C_LEDCFG0, 0x8080);

	/*
	 * Reset digital sequence
	 */
	/* Disable ELDR clock */
	urtwn_write_2(sc, R92C_SYS_CLKR,
	    R92C_SYS_CLKR_ANAD16V_EN |
	    R92C_SYS_CLKR_ANA8M |
	    R92C_SYS_CLKR_LOADER_EN |
	    R92C_SYS_CLKR_80M_SSC_DIS |
	    R92C_SYS_CLKR_SYS_EN |
	    R92C_SYS_CLKR_RING_EN |
	    0x4000);

	/* Isolated ELDR to PON */
	urtwn_write_1(sc, R92C_SYS_ISO_CTRL + 1,
	    (R92C_SYS_ISO_CTRL_DIOR |
	     R92C_SYS_ISO_CTRL_PWC_EV12V) >> 8);

	/*
	 * Disable analog sequence
	 */
	/* Disable A15 power */
	urtwn_write_1(sc, R92C_LDOA15_CTRL, R92C_LDOA15_CTRL_OBUF);
	/* Disable digital core power */
	urtwn_write_1(sc, R92C_LDOV12D_CTRL,
	    urtwn_read_1(sc, R92C_LDOV12D_CTRL) &
	      ~R92C_LDOV12D_CTRL_LDV12_EN);

	/* Enter PFM mode */
	urtwn_write_1(sc, R92C_SPS0_CTRL, 0x23);

	/* Set USB suspend */
	urtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_APDM_HOST |
	    R92C_APS_FSMCO_AFSM_HSUS |
	    R92C_APS_FSMCO_PFM_ALDN);

	/* Lock ISO/CLK/Power control register. */
	urtwn_write_1(sc, R92C_RSV_CTRL, 0x0E);
}

static void
urtwn_r88e_power_off(struct urtwn_softc *sc)
{
	uint8_t reg;
	int ntries;

	/* Disable any kind of TX reports. */
	urtwn_write_1(sc, R88E_TX_RPT_CTRL,
	    urtwn_read_1(sc, R88E_TX_RPT_CTRL) &
	      ~(R88E_TX_RPT1_ENA | R88E_TX_RPT2_ENA));

	/* Stop Rx. */
	urtwn_write_1(sc, R92C_CR, 0);

	/* Move card to Low Power State. */
	/* Block all Tx queues. */
	urtwn_write_1(sc, R92C_TXPAUSE, R92C_TX_QUEUE_ALL);

	for (ntries = 0; ntries < 20; ntries++) {
		/* Should be zero if no packet is transmitting. */
		if (urtwn_read_4(sc, R88E_SCH_TXCMD) == 0)
			break;

		urtwn_ms_delay(sc);
	}
	if (ntries == 20) {
		device_printf(sc->sc_dev, "%s: failed to block Tx queues\n",
		    __func__);
		return;
	}

	/* CCK and OFDM are disabled, and clock are gated. */
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_1(sc, R92C_SYS_FUNC_EN) & ~R92C_SYS_FUNC_EN_BBRSTB);

	urtwn_ms_delay(sc);

	/* Reset MAC TRX */
	urtwn_write_1(sc, R92C_CR,
	    R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN |
	    R92C_CR_PROTOCOL_EN | R92C_CR_SCHEDULE_EN);

	/* check if removed later */
	urtwn_write_1(sc, R92C_CR + 1,
	    urtwn_read_1(sc, R92C_CR + 1) & ~(R92C_CR_ENSEC >> 8));

	/* Respond TxOK to scheduler */
	urtwn_write_1(sc, R92C_DUAL_TSF_RST,
	    urtwn_read_1(sc, R92C_DUAL_TSF_RST) | 0x20);

	/* If firmware in ram code, do reset. */
#ifndef URTWN_WITHOUT_UCODE
	if (urtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RDY)
		urtwn_r88e_fw_reset(sc);
#endif

	/* Reset MCU ready status. */
	urtwn_write_1(sc, R92C_MCUFWDL, 0x00);

	/* Disable 32k. */
	urtwn_write_1(sc, R88E_32K_CTRL,
	    urtwn_read_1(sc, R88E_32K_CTRL) & ~0x01);

	/* Move card to Disabled state. */
	/* Turn off RF. */
	urtwn_write_1(sc, R92C_RF_CTRL, 0);

	/* LDO Sleep mode. */
	urtwn_write_1(sc, R92C_LPLDO_CTRL, 
	    urtwn_read_1(sc, R92C_LPLDO_CTRL) | R92C_LPLDO_CTRL_SLEEP);

	/* Turn off MAC by HW state machine */
	urtwn_write_1(sc, R92C_APS_FSMCO + 1,
	    urtwn_read_1(sc, R92C_APS_FSMCO + 1) |
	    (R92C_APS_FSMCO_APFM_OFF >> 8));

	for (ntries = 0; ntries < 20; ntries++) {
		/* Wait until it will be disabled. */
		if ((urtwn_read_1(sc, R92C_APS_FSMCO + 1) &
		    (R92C_APS_FSMCO_APFM_OFF >> 8)) == 0)
			break;

		urtwn_ms_delay(sc);
	}
	if (ntries == 20) {
		device_printf(sc->sc_dev, "%s: could not turn off MAC\n",
		    __func__);
		return;
	}

	/* schmit trigger */
	urtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 2,
	    urtwn_read_1(sc, R92C_AFE_XTAL_CTRL + 2) | 0x80);

	/* Enable WL suspend. */
	urtwn_write_1(sc, R92C_APS_FSMCO + 1,
	    (urtwn_read_1(sc, R92C_APS_FSMCO + 1) & ~0x10) | 0x08);

	/* Enable bandgap mbias in suspend. */
	urtwn_write_1(sc, R92C_APS_FSMCO + 3, 0);

	/* Clear SIC_EN register. */
	urtwn_write_1(sc, R92C_GPIO_MUXCFG + 1,
	    urtwn_read_1(sc, R92C_GPIO_MUXCFG + 1) & ~0x10);

	/* Set USB suspend enable local register */
	urtwn_write_1(sc, R92C_USB_SUSPEND,
	    urtwn_read_1(sc, R92C_USB_SUSPEND) | 0x10);

	/* Reset MCU IO Wrapper. */
	reg = urtwn_read_1(sc, R92C_RSV_CTRL + 1);
	urtwn_write_1(sc, R92C_RSV_CTRL + 1, reg & ~0x08);
	urtwn_write_1(sc, R92C_RSV_CTRL + 1, reg | 0x08);

	/* marked as 'For Power Consumption' code. */
	urtwn_write_1(sc, R92C_GPIO_OUT, urtwn_read_1(sc, R92C_GPIO_IN));
	urtwn_write_1(sc, R92C_GPIO_IOSEL, 0xff);

	urtwn_write_1(sc, R92C_GPIO_IO_SEL,
	    urtwn_read_1(sc, R92C_GPIO_IO_SEL) << 4);
	urtwn_write_1(sc, R92C_GPIO_MOD,
	    urtwn_read_1(sc, R92C_GPIO_MOD) | 0x0f);

	/* Set LNA, TRSW, EX_PA Pin to output mode. */
	urtwn_write_4(sc, R88E_BB_PAD_CTRL, 0x00080808);
}

static int
urtwn_llt_init(struct urtwn_softc *sc)
{
	int i, error, page_count, pktbuf_count;

	page_count = (sc->chip & URTWN_CHIP_88E) ?
	    R88E_TX_PAGE_COUNT : R92C_TX_PAGE_COUNT;
	pktbuf_count = (sc->chip & URTWN_CHIP_88E) ?
	    R88E_TXPKTBUF_COUNT : R92C_TXPKTBUF_COUNT;

	/* Reserve pages [0; page_count]. */
	for (i = 0; i < page_count; i++) {
		if ((error = urtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* NB: 0xff indicates end-of-list. */
	if ((error = urtwn_llt_write(sc, i, 0xff)) != 0)
		return (error);
	/*
	 * Use pages [page_count + 1; pktbuf_count - 1]
	 * as ring buffer.
	 */
	for (++i; i < pktbuf_count - 1; i++) {
		if ((error = urtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* Make the last page point to the beginning of the ring buffer. */
	error = urtwn_llt_write(sc, i, page_count + 1);
	return (error);
}

#ifndef URTWN_WITHOUT_UCODE
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
		urtwn_ms_delay(sc);
	}
	/* Force 8051 reset. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
}

static void
urtwn_r88e_fw_reset(struct urtwn_softc *sc)
{
	uint16_t reg;

	reg = urtwn_read_2(sc, R92C_SYS_FUNC_EN);
	urtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
	urtwn_write_2(sc, R92C_SYS_FUNC_EN, reg | R92C_SYS_FUNC_EN_CPUEN);
}

static int
urtwn_fw_loadpage(struct urtwn_softc *sc, int page, const uint8_t *buf, int len)
{
	uint32_t reg;
	usb_error_t error = USB_ERR_NORMAL_COMPLETION;
	int off, mlen;

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
		if (error != USB_ERR_NORMAL_COMPLETION)
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

	URTWN_UNLOCK(sc);
	/* Read firmware image from the filesystem. */
	if (sc->chip & URTWN_CHIP_88E)
		imagename = "urtwn-rtl8188eufw";
	else if ((sc->chip & (URTWN_CHIP_UMC_A_CUT | URTWN_CHIP_92C)) ==
		    URTWN_CHIP_UMC_A_CUT)
		imagename = "urtwn-rtl8192cfwU";
	else
		imagename = "urtwn-rtl8192cfwT";

	fw = firmware_get(imagename);
	URTWN_LOCK(sc);
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
	    (le16toh(hdr->signature) >> 4) == 0x88e ||
	    (le16toh(hdr->signature) >> 4) == 0x92c) {
		URTWN_DPRINTF(sc, URTWN_DEBUG_FIRMWARE,
		    "FW V%d.%d %02d-%02d %02d:%02d\n",
		    le16toh(hdr->version), le16toh(hdr->subversion),
		    hdr->month, hdr->date, hdr->hour, hdr->minute);
		ptr += sizeof(*hdr);
		len -= sizeof(*hdr);
	}

	if (urtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL) {
		if (sc->chip & URTWN_CHIP_88E)
			urtwn_r88e_fw_reset(sc);
		else
			urtwn_fw_reset(sc);
		urtwn_write_1(sc, R92C_MCUFWDL, 0);
	}

	if (!(sc->chip & URTWN_CHIP_88E)) {
		urtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
		    R92C_SYS_FUNC_EN_CPUEN);
	}
	urtwn_write_1(sc, R92C_MCUFWDL,
	    urtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
	urtwn_write_1(sc, R92C_MCUFWDL + 2,
	    urtwn_read_1(sc, R92C_MCUFWDL + 2) & ~0x08);

	/* Reset the FWDL checksum. */
	urtwn_write_1(sc, R92C_MCUFWDL,
	    urtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_CHKSUM_RPT);

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
		urtwn_ms_delay(sc);
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
	if (sc->chip & URTWN_CHIP_88E)
		urtwn_r88e_fw_reset(sc);
	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY)
			break;
		urtwn_ms_delay(sc);
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
#endif

static int
urtwn_dma_init(struct urtwn_softc *sc)
{
	struct usb_endpoint *ep, *ep_end;
	usb_error_t usb_err;
	uint32_t reg;
	int hashq, hasnq, haslq, nqueues, ntx;
	int error, pagecount, npubqpages, nqpages, nrempages, tx_boundary;

	/* Initialize LLT table. */
	error = urtwn_llt_init(sc);
	if (error != 0)
		return (error);

	/* Determine the number of bulk-out pipes. */
	ntx = 0;
	ep = sc->sc_udev->endpoints;
	ep_end = sc->sc_udev->endpoints + sc->sc_udev->endpoints_max;
	for (; ep != ep_end; ep++) {
		if ((ep->edesc == NULL) ||
		    (ep->iface_index != sc->sc_iface_index))
			continue;
		if (UE_GET_DIR(ep->edesc->bEndpointAddress) == UE_DIR_OUT)
			ntx++;
	}
	if (ntx == 0) {
		device_printf(sc->sc_dev,
		    "%d: invalid number of Tx bulk pipes\n", ntx);
		return (EIO);
	}

	/* Get Tx queues to USB endpoints mapping. */
	hashq = hasnq = haslq = nqueues = 0;
	switch (ntx) {
	case 1: hashq = 1; break;
	case 2: hashq = hasnq = 1; break;
	case 3: case 4: hashq = hasnq = haslq = 1; break;
	}
	nqueues = hashq + hasnq + haslq;
	if (nqueues == 0)
		return (EIO);

	npubqpages = nqpages = nrempages = pagecount = 0;
	if (sc->chip & URTWN_CHIP_88E)
		tx_boundary = R88E_TX_PAGE_BOUNDARY;
	else {
		pagecount = R92C_TX_PAGE_COUNT;
		npubqpages = R92C_PUBQ_NPAGES;
		tx_boundary = R92C_TX_PAGE_BOUNDARY;
	}

	/* Set number of pages for normal priority queue. */
	if (sc->chip & URTWN_CHIP_88E) {
		usb_err = urtwn_write_2(sc, R92C_RQPN_NPQ, 0xd);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
		usb_err = urtwn_write_4(sc, R92C_RQPN, 0x808e000d);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
	} else {
		/* Get the number of pages for each queue. */
		nqpages = (pagecount - npubqpages) / nqueues;
		/* 
		 * The remaining pages are assigned to the high priority
		 * queue.
		 */
		nrempages = (pagecount - npubqpages) % nqueues;
		usb_err = urtwn_write_1(sc, R92C_RQPN_NPQ, hasnq ? nqpages : 0);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
		usb_err = urtwn_write_4(sc, R92C_RQPN,
		    /* Set number of pages for public queue. */
		    SM(R92C_RQPN_PUBQ, npubqpages) |
		    /* Set number of pages for high priority queue. */
		    SM(R92C_RQPN_HPQ, hashq ? nqpages + nrempages : 0) |
		    /* Set number of pages for low priority queue. */
		    SM(R92C_RQPN_LPQ, haslq ? nqpages : 0) |
		    /* Load values. */
		    R92C_RQPN_LD);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			return (EIO);
	}

	usb_err = urtwn_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, tx_boundary);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	usb_err = urtwn_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, tx_boundary);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	usb_err = urtwn_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD, tx_boundary);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	usb_err = urtwn_write_1(sc, R92C_TRXFF_BNDY, tx_boundary);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	usb_err = urtwn_write_1(sc, R92C_TDECTRL + 1, tx_boundary);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

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
		/* 
		 * All 2-endpoints configs have high and normal 
		 * priority queues.
		 */
		reg |= R92C_TRXDMA_CTRL_QMAP_HQ_NQ;
	} else
		reg |= R92C_TRXDMA_CTRL_QMAP_3EP;
	usb_err = urtwn_write_2(sc, R92C_TRXDMA_CTRL, reg);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	/* Set Tx/Rx transfer page boundary. */
	usb_err = urtwn_write_2(sc, R92C_TRXFF_BNDY + 2,
	    (sc->chip & URTWN_CHIP_88E) ? 0x23ff : 0x27ff);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	/* Set Tx/Rx transfer page size. */
	usb_err = urtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	return (0);
}

static int
urtwn_mac_init(struct urtwn_softc *sc)
{
	usb_error_t error;
	int i;

	/* Write MAC initialization values. */
	if (sc->chip & URTWN_CHIP_88E) {
		for (i = 0; i < nitems(rtl8188eu_mac); i++) {
			error = urtwn_write_1(sc, rtl8188eu_mac[i].reg,
			    rtl8188eu_mac[i].val);
			if (error != USB_ERR_NORMAL_COMPLETION)
				return (EIO);
		}
		urtwn_write_1(sc, R92C_MAX_AGGR_NUM, 0x07);
	} else {
		for (i = 0; i < nitems(rtl8192cu_mac); i++)
			error = urtwn_write_1(sc, rtl8192cu_mac[i].reg,
			    rtl8192cu_mac[i].val);
			if (error != USB_ERR_NORMAL_COMPLETION)
				return (EIO);
	}

	return (0);
}

static void
urtwn_bb_init(struct urtwn_softc *sc)
{
	const struct urtwn_bb_prog *prog;
	uint32_t reg;
	uint8_t crystalcap;
	int i;

	/* Enable BB and RF. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	if (!(sc->chip & URTWN_CHIP_88E))
		urtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0xdb83);

	urtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_USBA | R92C_SYS_FUNC_EN_USBD |
	    R92C_SYS_FUNC_EN_BB_GLB_RST | R92C_SYS_FUNC_EN_BBRSTB);

	if (!(sc->chip & URTWN_CHIP_88E)) {
		urtwn_write_1(sc, R92C_LDOHCI12_CTRL, 0x0f);
		urtwn_write_1(sc, 0x15, 0xe9);
		urtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);
	}

	/* Select BB programming based on board type. */
	if (sc->chip & URTWN_CHIP_88E)
		prog = &rtl8188eu_bb_prog;
	else if (!(sc->chip & URTWN_CHIP_92C)) {
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
		urtwn_ms_delay(sc);
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
		urtwn_ms_delay(sc);
	}

	if (sc->chip & URTWN_CHIP_88E) {
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), 0x69553422);
		urtwn_ms_delay(sc);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), 0x69553420);
		urtwn_ms_delay(sc);

		crystalcap = sc->rom.r88e_rom.crystalcap;
		if (crystalcap == 0xff)
			crystalcap = 0x20;
		crystalcap &= 0x3f;
		reg = urtwn_bb_read(sc, R92C_AFE_XTAL_CTRL);
		urtwn_bb_write(sc, R92C_AFE_XTAL_CTRL,
		    RW(reg, R92C_AFE_XTAL_CTRL_ADDR,
		    crystalcap | crystalcap << 6));
	} else {
		if (urtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
		    R92C_HSSI_PARAM2_CCK_HIPWR)
			sc->sc_flags |= URTWN_FLAG_CCK_HIPWR;
	}
}

static void
urtwn_rf_init(struct urtwn_softc *sc)
{
	const struct urtwn_rf_prog *prog;
	uint32_t reg, type;
	int i, j, idx, off;

	/* Select RF programming based on board type. */
	if (sc->chip & URTWN_CHIP_88E)
		prog = rtl8188eu_rf_prog;
	else if (!(sc->chip & URTWN_CHIP_92C)) {
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
		urtwn_ms_delay(sc);
		/* Set RF_ENV output high. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		urtwn_ms_delay(sc);
		/* Set address and data lengths of RF registers. */
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		urtwn_ms_delay(sc);
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		urtwn_ms_delay(sc);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				usb_pause_mtx(&sc->sc_mtx, hz / 20);	/* 50ms */
				continue;
			}
			urtwn_rf_write(sc, i, prog[i].regs[j],
			    prog[i].vals[j]);
			urtwn_ms_delay(sc);
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

static int
urtwn_cam_write(struct urtwn_softc *sc, uint32_t addr, uint32_t data)
{
	usb_error_t error;

	error = urtwn_write_4(sc, R92C_CAMWRITE, data);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);
	error = urtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_WRITE |
	    SM(R92C_CAMCMD_ADDR, addr));
	if (error != USB_ERR_NORMAL_COMPLETION)
		return (EIO);

	return (0);
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
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t rcr;
	uint16_t filter;

	URTWN_ASSERT_LOCKED(sc);

	/* Setup multicast filter. */
	urtwn_set_multi(sc);

	/* Filter for management frames. */
	filter = 0x7f3f;
	switch (vap->iv_opmode) {
	case IEEE80211_M_STA:
		filter &= ~(
		    R92C_RXFLTMAP_SUBTYPE(IEEE80211_FC0_SUBTYPE_ASSOC_REQ) |
		    R92C_RXFLTMAP_SUBTYPE(IEEE80211_FC0_SUBTYPE_REASSOC_REQ) |
		    R92C_RXFLTMAP_SUBTYPE(IEEE80211_FC0_SUBTYPE_PROBE_REQ));
		break;
	case IEEE80211_M_HOSTAP:
		filter &= ~(
		    R92C_RXFLTMAP_SUBTYPE(IEEE80211_FC0_SUBTYPE_ASSOC_RESP) |
		    R92C_RXFLTMAP_SUBTYPE(IEEE80211_FC0_SUBTYPE_REASSOC_RESP));
		break;
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_IBSS:
		break;
	default:
		device_printf(sc->sc_dev, "%s: undefined opmode %d\n",
		    __func__, vap->iv_opmode);
		break;
	}
	urtwn_write_2(sc, R92C_RXFLTMAP0, filter);

	/* Reject all control frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);

	/* Reject all data frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP2, 0x0000);

	rcr = R92C_RCR_AM | R92C_RCR_AB | R92C_RCR_APM |
	      R92C_RCR_HTC_LOC_CTRL | R92C_RCR_APP_PHYSTS |
	      R92C_RCR_APP_ICV | R92C_RCR_APP_MIC;

	if (vap->iv_opmode == IEEE80211_M_MONITOR) {
		/* Accept all frames. */
		rcr |= R92C_RCR_ACF | R92C_RCR_ADF | R92C_RCR_AMF |
		       R92C_RCR_AAP;
	}

	/* Set Rx filter. */
	urtwn_write_4(sc, R92C_RCR, rcr);

	if (ic->ic_promisc != 0) {
		/* Update Rx filter. */
		urtwn_set_promisc(sc);
	}
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

static void
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
	    SM(R92C_TXAGC_MCS09,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

static void
urtwn_get_txpower(struct urtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[URTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom.r92c_rom;
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
		for (ridx = URTWN_RIDX_CCK1; ridx <= URTWN_RIDX_CCK11; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = URTWN_RIDX_OFDM6; ridx < URTWN_RIDX_COUNT; ridx++) {
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
	for (ridx = URTWN_RIDX_CCK1; ridx <= URTWN_RIDX_CCK11; ridx++) {
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
	for (ridx = URTWN_RIDX_OFDM6; ridx <= URTWN_RIDX_OFDM54; ridx++) {
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
#ifdef USB_DEBUG
	if (sc->sc_debug & URTWN_DEBUG_TXPWR) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = URTWN_RIDX_CCK1; ridx < URTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}

static void
urtwn_r88e_get_txpower(struct urtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[URTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r88e_rom *rom = &sc->rom.r88e_rom;
	uint16_t cckpow, ofdmpow, bw20pow, htpow;
	const struct urtwn_r88e_txpwr *base;
	int ridx, chan, group;

	/* Determine channel group. */
	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan <= 2)
		group = 0;
	else if (chan <= 5)
		group = 1;
	else if (chan <= 8)
		group = 2;
	else if (chan <= 11)
		group = 3;
	else if (chan <= 13)
		group = 4;
	else
		group = 5;

	/* Get original Tx power based on board type and RF chain. */
	base = &rtl8188eu_txagc[chain];

	memset(power, 0, URTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = URTWN_RIDX_CCK1; ridx <= URTWN_RIDX_CCK11; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = URTWN_RIDX_OFDM6; ridx < URTWN_RIDX_COUNT; ridx++) {
		if (sc->regulatory == 3)
			power[ridx] = base->pwr[0][ridx];
		else if (sc->regulatory == 1) {
			if (extc == NULL)
				power[ridx] = base->pwr[group][ridx];
		} else if (sc->regulatory != 2)
			power[ridx] = base->pwr[0][ridx];
	}

	/* Compute per-CCK rate Tx power. */
	cckpow = rom->cck_tx_pwr[group];
	for (ridx = URTWN_RIDX_CCK1; ridx <= URTWN_RIDX_CCK11; ridx++) {
		power[ridx] += cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	htpow = rom->ht40_tx_pwr[group];

	/* Compute per-OFDM rate Tx power. */
	ofdmpow = htpow + sc->ofdm_tx_pwr_diff;
	for (ridx = URTWN_RIDX_OFDM6; ridx <= URTWN_RIDX_OFDM54; ridx++) {
		power[ridx] += ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	bw20pow = htpow + sc->bw20_tx_pwr_diff;
	for (ridx = 12; ridx <= 27; ridx++) {
		power[ridx] += bw20pow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
}

static void
urtwn_set_txpower(struct urtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint16_t power[URTWN_RIDX_COUNT];
	int i;

	for (i = 0; i < sc->ntxchains; i++) {
		/* Compute per-rate Tx power values. */
		if (sc->chip & URTWN_CHIP_88E)
			urtwn_r88e_get_txpower(sc, i, c, extc, power);
		else
			urtwn_get_txpower(sc, i, c, extc, power);
		/* Write per-rate Tx power values to hardware. */
		urtwn_write_txpower(sc, i, power);
	}
}

static void
urtwn_set_rx_bssid_all(struct urtwn_softc *sc, int enable)
{
	uint32_t reg;

	reg = urtwn_read_4(sc, R92C_RCR);
	if (enable)
		reg &= ~R92C_RCR_CBSSID_BCN;
	else
		reg |= R92C_RCR_CBSSID_BCN;
	urtwn_write_4(sc, R92C_RCR, reg);
}

static void
urtwn_set_gain(struct urtwn_softc *sc, uint8_t gain)
{
	uint32_t reg;

	reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
	reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, gain);
	urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

	if (!(sc->chip & URTWN_CHIP_88E)) {
		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, gain);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);
	}
}

static void
urtwn_scan_start(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_softc;

	URTWN_LOCK(sc);
	/* Receive beacons / probe responses from any BSSID. */
	if (ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP)
		urtwn_set_rx_bssid_all(sc, 1);

	/* Set gain for scanning. */
	urtwn_set_gain(sc, 0x20);
	URTWN_UNLOCK(sc);
}

static void
urtwn_scan_end(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_softc;

	URTWN_LOCK(sc);
	/* Restore limitations. */
	if (ic->ic_promisc == 0 &&
	    ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP)
		urtwn_set_rx_bssid_all(sc, 0);

	/* Set gain under link. */
	urtwn_set_gain(sc, 0x32);
	URTWN_UNLOCK(sc);
}

static void
urtwn_getradiocaps(struct ieee80211com *ic,
    int maxchans, int *nchans, struct ieee80211_channel chans[])
{
	uint8_t bands[IEEE80211_MODE_BYTES];

	memset(bands, 0, sizeof(bands));
	setbit(bands, IEEE80211_MODE_11B);
	setbit(bands, IEEE80211_MODE_11G);
	if (urtwn_enable_11n)
		setbit(bands, IEEE80211_MODE_11NG);
	ieee80211_add_channel_list_2ghz(chans, maxchans, nchans,
	    urtwn_chan_2ghz, nitems(urtwn_chan_2ghz), bands, 0);
}

static void
urtwn_set_channel(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_softc;
	struct ieee80211_channel *c = ic->ic_curchan;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	URTWN_LOCK(sc);
	if (vap->iv_state == IEEE80211_S_SCAN) {
		/* Make link LED blink during scan. */
		urtwn_set_led(sc, URTWN_LED_LINK, !sc->ledlink);
	}
	urtwn_set_chan(sc, c, NULL);
	sc->sc_rxtap.wr_chan_freq = htole16(c->ic_freq);
	sc->sc_rxtap.wr_chan_flags = htole16(c->ic_flags);
	sc->sc_txtap.wt_chan_freq = htole16(c->ic_freq);
	sc->sc_txtap.wt_chan_flags = htole16(c->ic_flags);
	URTWN_UNLOCK(sc);
}

static int
urtwn_wme_update(struct ieee80211com *ic)
{
	const struct wmeParams *wmep =
	    ic->ic_wme.wme_chanParams.cap_wmeParams;
	struct urtwn_softc *sc = ic->ic_softc;
	uint8_t aifs, acm, slottime;
	int ac;

	acm = 0;
	slottime = IEEE80211_GET_SLOTTIME(ic);

	URTWN_LOCK(sc);
	for (ac = WME_AC_BE; ac < WME_NUM_AC; ac++) {
		/* AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime. */
		aifs = wmep[ac].wmep_aifsn * slottime + IEEE80211_DUR_SIFS;
		urtwn_write_4(sc, wme2queue[ac].reg,
		    SM(R92C_EDCA_PARAM_TXOP, wmep[ac].wmep_txopLimit) |
		    SM(R92C_EDCA_PARAM_ECWMIN, wmep[ac].wmep_logcwmin) |
		    SM(R92C_EDCA_PARAM_ECWMAX, wmep[ac].wmep_logcwmax) |
		    SM(R92C_EDCA_PARAM_AIFS, aifs));
		if (ac != WME_AC_BE)
			acm |= wmep[ac].wmep_acm << ac;
	}

	if (acm != 0)
		acm |= R92C_ACMHWCTRL_EN;
	urtwn_write_1(sc, R92C_ACMHWCTRL,
	    (urtwn_read_1(sc, R92C_ACMHWCTRL) & ~R92C_ACMHWCTRL_ACM_MASK) |
	    acm);

	URTWN_UNLOCK(sc);

	return 0;
}

static void
urtwn_update_slot(struct ieee80211com *ic)
{
	urtwn_cmd_sleepable(ic->ic_softc, NULL, 0, urtwn_update_slot_cb);
}

static void
urtwn_update_slot_cb(struct urtwn_softc *sc, union sec_param *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t slottime;

	slottime = IEEE80211_GET_SLOTTIME(ic);

	URTWN_DPRINTF(sc, URTWN_DEBUG_ANY, "%s: setting slot time to %uus\n",
	    __func__, slottime);

	urtwn_write_1(sc, R92C_SLOT, slottime);
	urtwn_update_aifs(sc, slottime);
}

static void
urtwn_update_aifs(struct urtwn_softc *sc, uint8_t slottime)
{
	const struct wmeParams *wmep =
	    sc->sc_ic.ic_wme.wme_chanParams.cap_wmeParams;
	uint8_t aifs, ac;

	for (ac = WME_AC_BE; ac < WME_NUM_AC; ac++) {
		/* AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime. */
		aifs = wmep[ac].wmep_aifsn * slottime + IEEE80211_DUR_SIFS;
		urtwn_write_1(sc, wme2queue[ac].reg, aifs);
        }
}

static uint8_t
urtwn_get_multi_pos(const uint8_t maddr[])
{
	uint64_t mask = 0x00004d101df481b4;
	uint8_t pos = 0x27;	/* initial value */
	int i, j;

	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		for (j = (i == 0) ? 1 : 0; j < 8; j++)
			if ((maddr[i] >> j) & 1)
				pos ^= (mask >> (i * 8 + j - 1));

	pos &= 0x3f;

	return (pos);
}

static void
urtwn_set_multi(struct urtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t mfilt[2];

	URTWN_ASSERT_LOCKED(sc);

	/* general structure was copied from ath(4). */
	if (ic->ic_allmulti == 0) {
		struct ieee80211vap *vap;
		struct ifnet *ifp;
		struct ifmultiaddr *ifma;

		/*
		 * Merge multicast addresses to form the hardware filter.
		 */
		mfilt[0] = mfilt[1] = 0;
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
			ifp = vap->iv_ifp;
			if_maddr_rlock(ifp);
			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				caddr_t dl;
				uint8_t pos;

				dl = LLADDR((struct sockaddr_dl *)
				    ifma->ifma_addr);
				pos = urtwn_get_multi_pos(dl);

				mfilt[pos / 32] |= (1 << (pos % 32));
			}
			if_maddr_runlock(ifp);
		}
	} else
		mfilt[0] = mfilt[1] = ~0;


	urtwn_write_4(sc, R92C_MAR + 0, mfilt[0]);
	urtwn_write_4(sc, R92C_MAR + 4, mfilt[1]);

	URTWN_DPRINTF(sc, URTWN_DEBUG_STATE, "%s: MC filter %08x:%08x\n",
	     __func__, mfilt[0], mfilt[1]);
}

static void
urtwn_set_promisc(struct urtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t rcr, mask1, mask2;

	URTWN_ASSERT_LOCKED(sc);

	if (vap->iv_opmode == IEEE80211_M_MONITOR)
		return;

	mask1 = R92C_RCR_ACF | R92C_RCR_ADF | R92C_RCR_AMF | R92C_RCR_AAP;
	mask2 = R92C_RCR_APM;

	if (vap->iv_state == IEEE80211_S_RUN) {
		switch (vap->iv_opmode) {
		case IEEE80211_M_STA:
			mask2 |= R92C_RCR_CBSSID_BCN;
			/* FALLTHROUGH */
		case IEEE80211_M_IBSS:
			mask2 |= R92C_RCR_CBSSID_DATA;
			break;
		case IEEE80211_M_HOSTAP:
			break;
		default:
			device_printf(sc->sc_dev, "%s: undefined opmode %d\n",
			    __func__, vap->iv_opmode);
			return;
		}
	}

	rcr = urtwn_read_4(sc, R92C_RCR);
	if (ic->ic_promisc == 0)
		rcr = (rcr & ~mask1) | mask2;
	else
		rcr = (rcr & ~mask2) | mask1;
	urtwn_write_4(sc, R92C_RCR, rcr);
}

static void
urtwn_update_promisc(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_softc;

	URTWN_LOCK(sc);
	if (sc->sc_flags & URTWN_RUNNING)
		urtwn_set_promisc(sc);
	URTWN_UNLOCK(sc);
}

static void
urtwn_update_mcast(struct ieee80211com *ic)
{
	struct urtwn_softc *sc = ic->ic_softc;

	URTWN_LOCK(sc);
	if (sc->sc_flags & URTWN_RUNNING)
		urtwn_set_multi(sc);
	URTWN_UNLOCK(sc);
}

static struct ieee80211_node *
urtwn_node_alloc(struct ieee80211vap *vap,
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct urtwn_node *un;

	un = malloc(sizeof (struct urtwn_node), M_80211_NODE,
	    M_NOWAIT | M_ZERO);

	if (un == NULL)
		return NULL;

	un->id = URTWN_MACID_UNDEFINED;

	return &un->ni;
}

static void
urtwn_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct urtwn_softc *sc = ni->ni_ic->ic_softc;
	struct urtwn_node *un = URTWN_NODE(ni);
	uint8_t id;

	/* Only do this bit for R88E chips */
	if (! (sc->chip & URTWN_CHIP_88E))
		return;

	if (!isnew)
		return;

	URTWN_NT_LOCK(sc);
	for (id = 0; id <= URTWN_MACID_MAX(sc); id++) {
		if (id != URTWN_MACID_BC && sc->node_list[id] == NULL) {
			un->id = id;
			sc->node_list[id] = ni;
			break;
		}
	}
	URTWN_NT_UNLOCK(sc);

	if (id > URTWN_MACID_MAX(sc)) {
		device_printf(sc->sc_dev, "%s: node table is full\n",
		    __func__);
	}
}

static void
urtwn_node_free(struct ieee80211_node *ni)
{
	struct urtwn_softc *sc = ni->ni_ic->ic_softc;
	struct urtwn_node *un = URTWN_NODE(ni);

	URTWN_NT_LOCK(sc);
	if (un->id != URTWN_MACID_UNDEFINED)
		sc->node_list[un->id] = NULL;
	URTWN_NT_UNLOCK(sc);

	sc->sc_node_free(ni);
}

static void
urtwn_set_chan(struct urtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = &sc->sc_ic;
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

		if (!(sc->chip & URTWN_CHIP_88E)) {
			urtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
			    urtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) |
			    R92C_FPGA0_ANAPARAM2_CBW20);
		}

		/* Select 20MHz bandwidth. */
		urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | chan |
		    ((sc->chip & URTWN_CHIP_88E) ? R88E_RF_CHNLBW_BW20 :
		    R92C_RF_CHNLBW_BW20));
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
		urtwn_write_1(sc, R92C_TXPAUSE, R92C_TX_QUEUE_ALL);
	}
	/* Start calibration. */
	urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    urtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	usb_pause_mtx(&sc->sc_mtx, hz / 10);		/* 100ms */

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
urtwn_temp_calib(struct urtwn_softc *sc)
{
	uint8_t temp;

	URTWN_ASSERT_LOCKED(sc);

	if (!(sc->sc_flags & URTWN_TEMP_MEASURED)) {
		/* Start measuring temperature. */
		URTWN_DPRINTF(sc, URTWN_DEBUG_TEMP,
		    "%s: start measuring temperature\n", __func__);
		if (sc->chip & URTWN_CHIP_88E) {
			urtwn_rf_write(sc, 0, R88E_RF_T_METER,
			    R88E_RF_T_METER_START);
		} else {
			urtwn_rf_write(sc, 0, R92C_RF_T_METER,
			    R92C_RF_T_METER_START);
		}
		sc->sc_flags |= URTWN_TEMP_MEASURED;
		return;
	}
	sc->sc_flags &= ~URTWN_TEMP_MEASURED;

	/* Read measured temperature. */
	if (sc->chip & URTWN_CHIP_88E) {
		temp = MS(urtwn_rf_read(sc, 0, R88E_RF_T_METER),
		    R88E_RF_T_METER_VAL);
	} else {
		temp = MS(urtwn_rf_read(sc, 0, R92C_RF_T_METER),
		    R92C_RF_T_METER_VAL);
	}
	if (temp == 0) {	/* Read failed, skip. */
		URTWN_DPRINTF(sc, URTWN_DEBUG_TEMP,
		    "%s: temperature read failed, skipping\n", __func__);
		return;
	}

	URTWN_DPRINTF(sc, URTWN_DEBUG_TEMP,
	    "%s: temperature: previous %u, current %u\n",
	    __func__, sc->thcal_lctemp, temp);

	/*
	 * Redo LC calibration if temperature changed significantly since
	 * last calibration.
	 */
	if (sc->thcal_lctemp == 0) {
		/* First LC calibration is performed in urtwn_init(). */
		sc->thcal_lctemp = temp;
	} else if (abs(temp - sc->thcal_lctemp) > 1) {
		URTWN_DPRINTF(sc, URTWN_DEBUG_TEMP,
		    "%s: LC calib triggered by temp: %u -> %u\n",
		    __func__, sc->thcal_lctemp, temp);
		urtwn_lc_calib(sc);
		/* Record temperature of last LC calibration. */
		sc->thcal_lctemp = temp;
	}
}

static void
urtwn_setup_static_keys(struct urtwn_softc *sc, struct urtwn_vap *uvp)
{
	int i;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		const struct ieee80211_key *k = uvp->keys[i];
		if (k != NULL) {
			urtwn_cmd_sleepable(sc, k, sizeof(*k),
			    urtwn_key_set_cb);
		}
	}
}

static int
urtwn_init(struct urtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	uint32_t reg;
	usb_error_t usb_err = USB_ERR_NORMAL_COMPLETION;
	int error;

	URTWN_LOCK(sc);
	if (sc->sc_flags & URTWN_RUNNING) {
		URTWN_UNLOCK(sc);
		return (0);
	}

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
	if (sc->chip & URTWN_CHIP_88E) {
		usb_err = urtwn_write_4(sc, R88E_HISR, 0xffffffff);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			goto fail;
		usb_err = urtwn_write_4(sc, R88E_HIMR, R88E_HIMR_CPWM | R88E_HIMR_CPWM2 |
		    R88E_HIMR_TBDER | R88E_HIMR_PSTIMEOUT);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			goto fail;
		usb_err = urtwn_write_4(sc, R88E_HIMRE, R88E_HIMRE_RXFOVW |
		    R88E_HIMRE_TXFOVW | R88E_HIMRE_RXERR | R88E_HIMRE_TXERR);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			goto fail;
		usb_err = urtwn_write_1(sc, R92C_USB_SPECIAL_OPTION,
		    urtwn_read_1(sc, R92C_USB_SPECIAL_OPTION) |
		    R92C_USB_SPECIAL_OPTION_INT_BULK_SEL);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			goto fail;
	} else {
		usb_err = urtwn_write_4(sc, R92C_HISR, 0xffffffff);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			goto fail;
		usb_err = urtwn_write_4(sc, R92C_HIMR, 0xffffffff);
		if (usb_err != USB_ERR_NORMAL_COMPLETION)
			goto fail;
	}

	/* Set MAC address. */
	IEEE80211_ADDR_COPY(macaddr, vap ? vap->iv_myaddr : ic->ic_macaddr);
	usb_err = urtwn_write_region_1(sc, R92C_MACID, macaddr, IEEE80211_ADDR_LEN);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		goto fail;

	/* Set initial network type. */
	urtwn_set_mode(sc, R92C_MSR_INFRA);

	/* Initialize Rx filter. */
	urtwn_rxfilter_init(sc);

	/* Set response rate. */
	reg = urtwn_read_4(sc, R92C_RRSR);
	reg = RW(reg, R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_CCK_ONLY_1M);
	urtwn_write_4(sc, R92C_RRSR, reg);

	/* Set short/long retry limits. */
	urtwn_write_2(sc, R92C_RL,
	    SM(R92C_RL_SRL, 0x30) | SM(R92C_RL_LRL, 0x30));

	/* Initialize EDCA parameters. */
	urtwn_edca_init(sc);

	/* Setup rate fallback. */
	if (!(sc->chip & URTWN_CHIP_88E)) {
		urtwn_write_4(sc, R92C_DARFRC + 0, 0x00000000);
		urtwn_write_4(sc, R92C_DARFRC + 4, 0x10080404);
		urtwn_write_4(sc, R92C_RARFRC + 0, 0x04030201);
		urtwn_write_4(sc, R92C_RARFRC + 4, 0x08070605);
	}

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
	urtwn_write_1(sc, R92C_RXDMA_AGG_PG_TH, 48);
	if (sc->chip & URTWN_CHIP_88E)
		urtwn_write_1(sc, R92C_RXDMA_AGG_PG_TH + 1, 4);
	else {
		urtwn_write_1(sc, R92C_USB_DMA_AGG_TO, 4);
		urtwn_write_1(sc, R92C_USB_SPECIAL_OPTION,
		    urtwn_read_1(sc, R92C_USB_SPECIAL_OPTION) |
		    R92C_USB_SPECIAL_OPTION_AGG_EN);
		urtwn_write_1(sc, R92C_USB_AGG_TH, 8);
		urtwn_write_1(sc, R92C_USB_AGG_TO, 6);
	}

	/* Initialize beacon parameters. */
	urtwn_write_2(sc, R92C_BCN_CTRL, 0x1010);
	urtwn_write_2(sc, R92C_TBTT_PROHIBIT, 0x6404);
	urtwn_write_1(sc, R92C_DRVERLYINT, 0x05);
	urtwn_write_1(sc, R92C_BCNDMATIM, 0x02);
	urtwn_write_2(sc, R92C_BCNTCFG, 0x660f);

	if (!(sc->chip & URTWN_CHIP_88E)) {
		/* Setup AMPDU aggregation. */
		urtwn_write_4(sc, R92C_AGGLEN_LMT, 0x99997631);	/* MCS7~0 */
		urtwn_write_1(sc, R92C_AGGR_BREAK_TIME, 0x16);
		urtwn_write_2(sc, R92C_MAX_AGGR_NUM, 0x0708);

		urtwn_write_1(sc, R92C_BCN_MAX_ERR, 0xff);
	}

#ifndef URTWN_WITHOUT_UCODE
	/* Load 8051 microcode. */
	error = urtwn_load_firmware(sc);
	if (error == 0)
		sc->sc_flags |= URTWN_FW_LOADED;
#endif

	/* Initialize MAC/BB/RF blocks. */
	error = urtwn_mac_init(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: error while initializing MAC block\n", __func__);
		goto fail;
	}
	urtwn_bb_init(sc);
	urtwn_rf_init(sc);

	/* Reinitialize Rx filter (D3845 is not committed yet). */
	urtwn_rxfilter_init(sc);

	if (sc->chip & URTWN_CHIP_88E) {
		urtwn_write_2(sc, R92C_CR,
		    urtwn_read_2(sc, R92C_CR) | R92C_CR_MACTXEN |
		    R92C_CR_MACRXEN);
	}

	/* Turn CCK and OFDM blocks on. */
	reg = urtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_CCK_EN;
	usb_err = urtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		goto fail;
	reg = urtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_OFDM_EN;
	usb_err = urtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		goto fail;

	/* Clear per-station keys table. */
	urtwn_cam_init(sc);

	/* Enable decryption / encryption. */
	urtwn_write_2(sc, R92C_SECCFG,
	    R92C_SECCFG_TXUCKEY_DEF | R92C_SECCFG_RXUCKEY_DEF |
	    R92C_SECCFG_TXENC_ENA | R92C_SECCFG_RXDEC_ENA |
	    R92C_SECCFG_TXBCKEY_DEF | R92C_SECCFG_RXBCKEY_DEF);

	/* Enable hardware sequence numbering. */
	urtwn_write_1(sc, R92C_HWSEQ_CTRL, R92C_TX_QUEUE_ALL);

	/* Enable per-packet TX report. */
	if (sc->chip & URTWN_CHIP_88E) {
		urtwn_write_1(sc, R88E_TX_RPT_CTRL,
		    urtwn_read_1(sc, R88E_TX_RPT_CTRL) | R88E_TX_RPT1_ENA);
	}

	/* Perform LO and IQ calibrations. */
	urtwn_iq_calib(sc);
	/* Perform LC calibration. */
	urtwn_lc_calib(sc);

	/* Fix USB interference issue. */
	if (!(sc->chip & URTWN_CHIP_88E)) {
		urtwn_write_1(sc, 0xfe40, 0xe0);
		urtwn_write_1(sc, 0xfe41, 0x8d);
		urtwn_write_1(sc, 0xfe42, 0x80);

		urtwn_pa_bias_init(sc);
	}

	/* Initialize GPIO setting. */
	urtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    urtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);

	/* Fix for lower temperature. */
	if (!(sc->chip & URTWN_CHIP_88E))
		urtwn_write_1(sc, 0x15, 0xe9);

	usbd_transfer_start(sc->sc_xfer[URTWN_BULK_RX]);

	sc->sc_flags |= URTWN_RUNNING;

	/*
	 * Install static keys (if any).
	 * Must be called after urtwn_cam_init().
	 */
	if (vap != NULL)
		urtwn_setup_static_keys(sc, URTWN_VAP(vap));

	callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);
fail:
	if (usb_err != USB_ERR_NORMAL_COMPLETION)
		error = EIO;                

	URTWN_UNLOCK(sc);                   

	return (error);
}

static void
urtwn_stop(struct urtwn_softc *sc)
{

	URTWN_LOCK(sc);
	if (!(sc->sc_flags & URTWN_RUNNING)) {
		URTWN_UNLOCK(sc);
		return;
	}

	sc->sc_flags &= ~(URTWN_RUNNING | URTWN_FW_LOADED |
	    URTWN_TEMP_MEASURED);
	sc->thcal_lctemp = 0;
	callout_stop(&sc->sc_watchdog_ch);

	urtwn_abort_xfers(sc);
	urtwn_drain_mbufq(sc);
	urtwn_free_tx_list(sc);
	urtwn_free_rx_list(sc);
	urtwn_power_off(sc);
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
	struct urtwn_softc *sc = ic->ic_softc;
	struct urtwn_data *bf;
	int error;

	URTWN_DPRINTF(sc, URTWN_DEBUG_XMIT, "%s: called; m=%p\n",
	    __func__,
	    m);

	/* prevent management frames from being sent if we're not ready */
	URTWN_LOCK(sc);
	if (!(sc->sc_flags & URTWN_RUNNING)) {
		error = ENETDOWN;
		goto end;
	}

	bf = urtwn_getbuf(sc);
	if (bf == NULL) {
		error = ENOBUFS;
		goto end;
	}

	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		error = urtwn_tx_data(sc, ni, m, bf);
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		error = urtwn_tx_raw(sc, ni, m, bf, params);
	}
	if (error != 0) {
		STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, bf, next);
		goto end;
	}

	sc->sc_txtimer = 5;
	callout_reset(&sc->sc_watchdog_ch, hz, urtwn_watchdog, sc);

end:
	if (error != 0)
		m_freem(m);

	URTWN_UNLOCK(sc);

	return (error);
}

static void
urtwn_ms_delay(struct urtwn_softc *sc)
{
	usb_pause_mtx(&sc->sc_mtx, hz / 1000);
}

static device_method_t urtwn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		urtwn_match),
	DEVMETHOD(device_attach,	urtwn_attach),
	DEVMETHOD(device_detach,	urtwn_detach),

	DEVMETHOD_END
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
#ifndef URTWN_WITHOUT_UCODE
MODULE_DEPEND(urtwn, firmware, 1, 1, 1);
#endif
MODULE_VERSION(urtwn, 1);
USB_PNP_HOST_INFO(urtwn_devs);
