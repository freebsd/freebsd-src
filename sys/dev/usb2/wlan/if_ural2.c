/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005, 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Copyright (c) 2006, 2008
 *	Hans Petter Selasky <hselasky@freebsd.org>
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
 *
 */

/*
 *
 * NOTE: all function names beginning like "ural_cfg_" can only
 * be called from within the config thread function !
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Ralink Technology RT2500USB chipset driver
 * http://www.ralinktech.com/
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc ural_config_copy
#define	usb2_config_td_softc ural_softc

#define	USB_DEBUG_VAR ural_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/wlan/usb2_wlan.h>
#include <dev/usb2/wlan/if_ural2_reg.h>
#include <dev/usb2/wlan/if_ural2_var.h>

#if USB_DEBUG
static int ural_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ural, CTLFLAG_RW, 0, "USB ural");
SYSCTL_INT(_hw_usb2_ural, OID_AUTO, debug, CTLFLAG_RW, &ural_debug, 0,
    "Debug level");
#endif

#define	URAL_RSSI(rssi)	\
  ((rssi) > (RAL_NOISE_FLOOR + RAL_RSSI_CORR) ? \
   ((rssi) - (RAL_NOISE_FLOOR + RAL_RSSI_CORR)) : 0)

/* prototypes */

static device_probe_t ural_probe;
static device_attach_t ural_attach;
static device_detach_t ural_detach;

static usb2_callback_t ural_bulk_read_callback;
static usb2_callback_t ural_bulk_read_clear_stall_callback;
static usb2_callback_t ural_bulk_write_callback;
static usb2_callback_t ural_bulk_write_clear_stall_callback;

static usb2_config_td_command_t ural_cfg_first_time_setup;
static usb2_config_td_command_t ural_config_copy;
static usb2_config_td_command_t ural_cfg_scan_start;
static usb2_config_td_command_t ural_cfg_scan_end;
static usb2_config_td_command_t ural_cfg_set_chan;
static usb2_config_td_command_t ural_cfg_enable_tsf_sync;
static usb2_config_td_command_t ural_cfg_update_slot;
static usb2_config_td_command_t ural_cfg_set_txpreamble;
static usb2_config_td_command_t ural_cfg_update_promisc;
static usb2_config_td_command_t ural_cfg_pre_init;
static usb2_config_td_command_t ural_cfg_init;
static usb2_config_td_command_t ural_cfg_pre_stop;
static usb2_config_td_command_t ural_cfg_stop;
static usb2_config_td_command_t ural_cfg_amrr_timeout;
static usb2_config_td_command_t ural_cfg_newstate;

static void ural_cfg_do_request(struct ural_softc *sc, struct usb2_device_request *req, void *data);
static void ural_cfg_set_testmode(struct ural_softc *sc);
static void ural_cfg_eeprom_read(struct ural_softc *sc, uint16_t addr, void *buf, uint16_t len);
static uint16_t ural_cfg_read(struct ural_softc *sc, uint16_t reg);
static void ural_cfg_read_multi(struct ural_softc *sc, uint16_t reg, void *buf, uint16_t len);
static void ural_cfg_write(struct ural_softc *sc, uint16_t reg, uint16_t val);
static void ural_cfg_write_multi(struct ural_softc *sc, uint16_t reg, void *buf, uint16_t len);
static void ural_cfg_bbp_write(struct ural_softc *sc, uint8_t reg, uint8_t val);
static uint8_t ural_cfg_bbp_read(struct ural_softc *sc, uint8_t reg);
static void ural_cfg_rf_write(struct ural_softc *sc, uint8_t reg, uint32_t val);
static void ural_end_of_commands(struct ural_softc *sc);
static const char *ural_get_rf(int rev);
static void ural_watchdog(void *arg);
static void ural_init_cb(void *arg);
static int ural_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data);
static void ural_start_cb(struct ifnet *ifp);
static int ural_newstate_cb(struct ieee80211vap *ic, enum ieee80211_state nstate, int arg);
static void ural_std_command(struct ieee80211com *ic, usb2_config_td_command_t *func);
static void ural_scan_start_cb(struct ieee80211com *);
static void ural_scan_end_cb(struct ieee80211com *);
static void ural_set_channel_cb(struct ieee80211com *);
static void ural_cfg_disable_rf_tune(struct ural_softc *sc);
static void ural_cfg_set_bssid(struct ural_softc *sc, uint8_t *bssid);
static void ural_cfg_set_macaddr(struct ural_softc *sc, uint8_t *addr);
static void ural_cfg_set_txantenna(struct ural_softc *sc, uint8_t antenna);
static void ural_cfg_set_rxantenna(struct ural_softc *sc, uint8_t antenna);
static void ural_cfg_read_eeprom(struct ural_softc *sc);
static uint8_t ural_cfg_bbp_init(struct ural_softc *sc);
static void ural_cfg_amrr_start(struct ural_softc *sc);
static struct ieee80211vap *ural_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit, int opmode, int flags, const uint8_t bssid[IEEE80211_ADDR_LEN], const uint8_t mac[IEEE80211_ADDR_LEN]);
static void ural_vap_delete(struct ieee80211vap *);
static struct ieee80211_node *ural_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN]);
static void ural_newassoc(struct ieee80211_node *, int);
static void ural_cfg_disable_tsf_sync(struct ural_softc *sc);
static void ural_cfg_set_run(struct ural_softc *sc, struct usb2_config_td_cc *cc);
static void ural_fill_write_queue(struct ural_softc *sc);
static void ural_tx_clean_queue(struct ural_softc *sc);
static void ural_tx_freem(struct mbuf *m);
static void ural_tx_mgt(struct ural_softc *sc, struct mbuf *m, struct ieee80211_node *ni);
static struct ieee80211vap *ural_get_vap(struct ural_softc *sc);
static void ural_tx_bcn(struct ural_softc *sc);
static void ural_tx_data(struct ural_softc *sc, struct mbuf *m, struct ieee80211_node *ni);
static void ural_tx_prot(struct ural_softc *sc, const struct mbuf *m, struct ieee80211_node *ni, uint8_t prot, uint16_t rate);
static void ural_tx_raw(struct ural_softc *sc, struct mbuf *m, struct ieee80211_node *ni, const struct ieee80211_bpf_params *params);
static int ural_raw_xmit_cb(struct ieee80211_node *ni, struct mbuf *m, const struct ieee80211_bpf_params *params);
static void ural_setup_desc_and_tx(struct ural_softc *sc, struct mbuf *m, uint32_t flags, uint16_t rate);
static void ural_update_mcast_cb(struct ifnet *ifp);
static void ural_update_promisc_cb(struct ifnet *ifp);

/* various supported device vendors/products */
static const struct usb2_device_id ural_devs[] = {
	{USB_VPI(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_WL167G, 0)},
	{USB_VPI(USB_VENDOR_ASUS, USB_PRODUCT_RALINK_RT2570, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D7050, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D7051, 0)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_HU200TS, 0)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_WUSB54G, 0)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_WUSB54GP, 0)},
	{USB_VPI(USB_VENDOR_CONCEPTRONIC2, USB_PRODUCT_CONCEPTRONIC2_C54RU, 0)},
	{USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DWLG122, 0)},
	{USB_VPI(USB_VENDOR_GIGABYTE, USB_PRODUCT_GIGABYTE_GN54G, 0)},
	{USB_VPI(USB_VENDOR_GIGABYTE, USB_PRODUCT_GIGABYTE_GNWBKG, 0)},
	{USB_VPI(USB_VENDOR_GUILLEMOT, USB_PRODUCT_GUILLEMOT_HWGUSB254, 0)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_KG54, 0)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_KG54AI, 0)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_KG54YB, 0)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_NINWIFI, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2570, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2570_2, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2570_3, 0)},
	{USB_VPI(USB_VENDOR_NOVATECH, USB_PRODUCT_NOVATECH_NV902, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2570, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2570_2, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2570_3, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2573, 0)},
	{USB_VPI(USB_VENDOR_SIEMENS2, USB_PRODUCT_SIEMENS2_WL54G, 0)},
	{USB_VPI(USB_VENDOR_SMC, USB_PRODUCT_SMC_2862WG, 0)},
	{USB_VPI(USB_VENDOR_SPHAIRON, USB_PRODUCT_SPHAIRON_UB801R, 0)},
	{USB_VPI(USB_VENDOR_SURECOM, USB_PRODUCT_SURECOM_RT2570, 0)},
	{USB_VPI(USB_VENDOR_VTECH, USB_PRODUCT_VTECH_RT2570, 0)},
	{USB_VPI(USB_VENDOR_ZINWELL, USB_PRODUCT_ZINWELL_RT2570, 0)},
};

/*
 * Default values for MAC registers; values taken from
 * the reference driver:
 */
struct ural_def_mac {
	uint16_t reg;
	uint16_t val;
};

static const struct ural_def_mac ural_def_mac[] = {
	{RAL_TXRX_CSR5, 0x8c8d},
	{RAL_TXRX_CSR6, 0x8b8a},
	{RAL_TXRX_CSR7, 0x8687},
	{RAL_TXRX_CSR8, 0x0085},
	{RAL_MAC_CSR13, 0x1111},
	{RAL_MAC_CSR14, 0x1e11},
	{RAL_TXRX_CSR21, 0xe78f},
	{RAL_MAC_CSR9, 0xff1d},
	{RAL_MAC_CSR11, 0x0002},
	{RAL_MAC_CSR22, 0x0053},
	{RAL_MAC_CSR15, 0x0000},
	{RAL_MAC_CSR8, RAL_FRAME_SIZE},
	{RAL_TXRX_CSR19, 0x0000},
	{RAL_TXRX_CSR18, 0x005a},
	{RAL_PHY_CSR2, 0x0000},
	{RAL_TXRX_CSR0, 0x1ec0},
	{RAL_PHY_CSR4, 0x000f}
};

/*
 * Default values for BBP registers; values taken from the reference driver.
 */
struct ural_def_bbp {
	uint8_t	reg;
	uint8_t	val;
};

static const struct ural_def_bbp ural_def_bbp[] = {
	{3, 0x02},
	{4, 0x19},
	{14, 0x1c},
	{15, 0x30},
	{16, 0xac},
	{17, 0x48},
	{18, 0x18},
	{19, 0xff},
	{20, 0x1e},
	{21, 0x08},
	{22, 0x08},
	{23, 0x08},
	{24, 0x80},
	{25, 0x50},
	{26, 0x08},
	{27, 0x23},
	{30, 0x10},
	{31, 0x2b},
	{32, 0xb9},
	{34, 0x12},
	{35, 0x50},
	{39, 0xc4},
	{40, 0x02},
	{41, 0x60},
	{53, 0x10},
	{54, 0x18},
	{56, 0x08},
	{57, 0x10},
	{58, 0x08},
	{61, 0x60},
	{62, 0x10},
	{75, 0xff}
};

/*
 * Default values for RF register R2 indexed by channel numbers.
 */
static const uint32_t ural_rf2522_r2[] = {
	0x307f6, 0x307fb, 0x30800, 0x30805, 0x3080a, 0x3080f, 0x30814,
	0x30819, 0x3081e, 0x30823, 0x30828, 0x3082d, 0x30832, 0x3083e
};

static const uint32_t ural_rf2523_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ural_rf2524_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ural_rf2525_r2[] = {
	0x20327, 0x20328, 0x20329, 0x2032a, 0x2032b, 0x2032c, 0x2032d,
	0x2032e, 0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20346
};

static const uint32_t ural_rf2525_hi_r2[] = {
	0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20344, 0x20345,
	0x20346, 0x20347, 0x20348, 0x20349, 0x2034a, 0x2034b, 0x2034e
};

static const uint32_t ural_rf2525e_r2[] = {
	0x2044d, 0x2044e, 0x2044f, 0x20460, 0x20461, 0x20462, 0x20463,
	0x20464, 0x20465, 0x20466, 0x20467, 0x20468, 0x20469, 0x2046b
};

static const uint32_t ural_rf2526_hi_r2[] = {
	0x0022a, 0x0022b, 0x0022b, 0x0022c, 0x0022c, 0x0022d, 0x0022d,
	0x0022e, 0x0022e, 0x0022f, 0x0022d, 0x00240, 0x00240, 0x00241
};

static const uint32_t ural_rf2526_r2[] = {
	0x00226, 0x00227, 0x00227, 0x00228, 0x00228, 0x00229, 0x00229,
	0x0022a, 0x0022a, 0x0022b, 0x0022b, 0x0022c, 0x0022c, 0x0022d
};

/*
 * For dual-band RF, RF registers R1 and R4 also depend on channel number;
 * values taken from the reference driver.
 */
struct ural_rf5222 {
	uint8_t	chan;
	uint32_t r1;
	uint32_t r2;
	uint32_t r4;
};

static const struct ural_rf5222 ural_rf5222[] = {
	{1, 0x08808, 0x0044d, 0x00282},
	{2, 0x08808, 0x0044e, 0x00282},
	{3, 0x08808, 0x0044f, 0x00282},
	{4, 0x08808, 0x00460, 0x00282},
	{5, 0x08808, 0x00461, 0x00282},
	{6, 0x08808, 0x00462, 0x00282},
	{7, 0x08808, 0x00463, 0x00282},
	{8, 0x08808, 0x00464, 0x00282},
	{9, 0x08808, 0x00465, 0x00282},
	{10, 0x08808, 0x00466, 0x00282},
	{11, 0x08808, 0x00467, 0x00282},
	{12, 0x08808, 0x00468, 0x00282},
	{13, 0x08808, 0x00469, 0x00282},
	{14, 0x08808, 0x0046b, 0x00286},

	{36, 0x08804, 0x06225, 0x00287},
	{40, 0x08804, 0x06226, 0x00287},
	{44, 0x08804, 0x06227, 0x00287},
	{48, 0x08804, 0x06228, 0x00287},
	{52, 0x08804, 0x06229, 0x00287},
	{56, 0x08804, 0x0622a, 0x00287},
	{60, 0x08804, 0x0622b, 0x00287},
	{64, 0x08804, 0x0622c, 0x00287},

	{100, 0x08804, 0x02200, 0x00283},
	{104, 0x08804, 0x02201, 0x00283},
	{108, 0x08804, 0x02202, 0x00283},
	{112, 0x08804, 0x02203, 0x00283},
	{116, 0x08804, 0x02204, 0x00283},
	{120, 0x08804, 0x02205, 0x00283},
	{124, 0x08804, 0x02206, 0x00283},
	{128, 0x08804, 0x02207, 0x00283},
	{132, 0x08804, 0x02208, 0x00283},
	{136, 0x08804, 0x02209, 0x00283},
	{140, 0x08804, 0x0220a, 0x00283},

	{149, 0x08808, 0x02429, 0x00281},
	{153, 0x08808, 0x0242b, 0x00281},
	{157, 0x08808, 0x0242d, 0x00281},
	{161, 0x08808, 0x0242f, 0x00281}
};

static const struct usb2_config ural_config[URAL_N_TRANSFER] = {
	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = (RAL_FRAME_SIZE + RAL_TX_DESC_SIZE + 4),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &ural_bulk_write_callback,
		.mh.timeout = 5000,	/* ms */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = (RAL_FRAME_SIZE + RAL_RX_DESC_SIZE),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &ural_bulk_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ural_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ural_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static devclass_t ural_devclass;

static device_method_t ural_methods[] = {
	DEVMETHOD(device_probe, ural_probe),
	DEVMETHOD(device_attach, ural_attach),
	DEVMETHOD(device_detach, ural_detach),
	{0, 0}
};

static driver_t ural_driver = {
	.name = "ural",
	.methods = ural_methods,
	.size = sizeof(struct ural_softc),
};

DRIVER_MODULE(ural, ushub, ural_driver, ural_devclass, NULL, 0);
MODULE_DEPEND(ural, usb2_wlan, 1, 1, 1);
MODULE_DEPEND(ural, usb2_core, 1, 1, 1);
MODULE_DEPEND(ural, wlan, 1, 1, 1);
MODULE_DEPEND(ural, wlan_amrr, 1, 1, 1);

static int
ural_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != 0) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != RAL_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(ural_devs, sizeof(ural_devs), uaa));
}

static int
ural_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ural_softc *sc = device_get_softc(dev);
	int error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "ural lock", MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	snprintf(sc->sc_name, sizeof(sc->sc_name), "%s",
	    device_get_nameunit(dev));

	sc->sc_udev = uaa->device;
	sc->sc_unit = device_get_unit(dev);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	iface_index = RAL_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device,
	    &iface_index, sc->sc_xfer, ural_config,
	    URAL_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		device_printf(dev, "could not allocate USB transfers, "
		    "err=%s\n", usb2_errstr(error));
		goto detach;
	}
	error = usb2_config_td_setup(&sc->sc_config_td, sc, &sc->sc_mtx,
	    &ural_end_of_commands,
	    sizeof(struct usb2_config_td_cc), 24);
	if (error) {
		device_printf(dev, "could not setup config "
		    "thread!\n");
		goto detach;
	}
	mtx_lock(&sc->sc_mtx);

	/* start setup */

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &ural_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	ural_watchdog(sc);

	return (0);			/* success */

detach:
	ural_detach(dev);
	return (ENXIO);			/* failure */
}

static int
ural_detach(device_t dev)
{
	struct ural_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic;
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	ural_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;
	ic = ifp->if_l2com;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, URAL_N_TRANSFER);

	/* get rid of any late children */
	bus_generic_detach(dev);

	if (ifp) {
		bpfdetach(ifp);
		ieee80211_ifdetach(ic);
		if_free(ifp);
	}
	usb2_config_td_unsetup(&sc->sc_config_td);

	usb2_callout_drain(&sc->sc_watchdog);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/*========================================================================*
 * REGISTER READ / WRITE WRAPPER ROUTINES
 *========================================================================*/

static void
ural_cfg_do_request(struct ural_softc *sc, struct usb2_device_request *req,
    void *data)
{
	uint16_t length;
	usb2_error_t err;

repeat:

	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		goto error;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &sc->sc_mtx, req, data, 0, NULL, 1000);

	if (err) {

		DPRINTF("device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));

		/* wait a little before next try */
		if (usb2_config_td_sleep(&sc->sc_config_td, hz / 4)) {
			goto error;
		}
		/* try until we are detached */
		goto repeat;

error:
		/* the device has been detached */
		length = UGETW(req->wLength);

		if ((req->bmRequestType & UT_READ) && length) {
			bzero(data, length);
		}
	}
	return;
}

static void
ural_cfg_set_testmode(struct ural_softc *sc)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_VENDOR_REQUEST;
	USETW(req.wValue, 4);
	USETW(req.wIndex, 1);
	USETW(req.wLength, 0);

	ural_cfg_do_request(sc, &req, NULL);
	return;
}

static void
ural_cfg_eeprom_read(struct ural_softc *sc, uint16_t addr,
    void *buf, uint16_t len)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_EEPROM;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	ural_cfg_do_request(sc, &req, buf);
	return;
}

static uint16_t
ural_cfg_read(struct ural_softc *sc, uint16_t reg)
{
	struct usb2_device_request req;
	uint16_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, sizeof(val));

	ural_cfg_do_request(sc, &req, &val);

	return (le16toh(val));
}

static void
ural_cfg_read_multi(struct ural_softc *sc, uint16_t reg,
    void *buf, uint16_t len)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	ural_cfg_do_request(sc, &req, buf);
	return;
}

static void
ural_cfg_write(struct ural_softc *sc, uint16_t reg, uint16_t val)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_WRITE_MAC;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	ural_cfg_do_request(sc, &req, NULL);
	return;
}

static void
ural_cfg_write_multi(struct ural_softc *sc, uint16_t reg,
    void *buf, uint16_t len)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_WRITE_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	ural_cfg_do_request(sc, &req, buf);
	return;
}

static uint8_t
ural_cfg_bbp_disbusy(struct ural_softc *sc)
{
	uint16_t tmp;
	uint8_t to;

	for (to = 0;; to++) {
		if (to < 100) {
			tmp = ural_cfg_read(sc, RAL_PHY_CSR8);
			tmp &= RAL_BBP_BUSY;

			if (tmp == 0) {
				return (0);
			}
			if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
				break;
			}
		} else {
			break;
		}
	}
	DPRINTF("could not disbusy BBP\n");
	return (1);			/* failure */
}

static void
ural_cfg_bbp_write(struct ural_softc *sc, uint8_t reg, uint8_t val)
{
	uint16_t tmp;

	if (ural_cfg_bbp_disbusy(sc)) {
		return;
	}
	tmp = (reg << 8) | val;
	ural_cfg_write(sc, RAL_PHY_CSR7, tmp);
	return;
}

static uint8_t
ural_cfg_bbp_read(struct ural_softc *sc, uint8_t reg)
{
	uint16_t val;

	if (ural_cfg_bbp_disbusy(sc)) {
		return (0);
	}
	val = RAL_BBP_WRITE | (reg << 8);
	ural_cfg_write(sc, RAL_PHY_CSR7, val);

	if (ural_cfg_bbp_disbusy(sc)) {
		return (0);
	}
	return (ural_cfg_read(sc, RAL_PHY_CSR7) & 0xff);
}

static void
ural_cfg_rf_write(struct ural_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	uint8_t to;

	reg &= 3;

	/* remember last written value */
	sc->sc_rf_regs[reg] = val;

	for (to = 0;; to++) {
		if (to < 100) {
			tmp = ural_cfg_read(sc, RAL_PHY_CSR10);

			if (!(tmp & RAL_RF_LOBUSY)) {
				break;
			}
			if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
				return;
			}
		} else {
			DPRINTF("could not write to RF\n");
			return;
		}
	}

	tmp = RAL_RF_BUSY | RAL_RF_20BIT | ((val & 0xfffff) << 2) | reg;
	ural_cfg_write(sc, RAL_PHY_CSR9, tmp & 0xffff);
	ural_cfg_write(sc, RAL_PHY_CSR10, tmp >> 16);

	DPRINTFN(16, "RF R[%u] <- 0x%05x\n", reg, val & 0xfffff);
	return;
}

static void
ural_cfg_first_time_setup(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ieee80211com *ic;
	struct ifnet *ifp;
	uint8_t bands;

	/* setup RX tap header */
	sc->sc_rxtap_len = sizeof(sc->sc_rxtap);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RAL_RX_RADIOTAP_PRESENT);

	/* setup TX tap header */
	sc->sc_txtap_len = sizeof(sc->sc_txtap);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RAL_TX_RADIOTAP_PRESENT);

	/* retrieve RT2570 rev. no */
	sc->sc_asic_rev = ural_cfg_read(sc, RAL_MAC_CSR0);

	/* retrieve MAC address and various other things from EEPROM */
	ural_cfg_read_eeprom(sc);

	printf("%s: MAC/BBP RT2570 (rev 0x%02x), RF %s\n",
	    sc->sc_name, sc->sc_asic_rev, ural_get_rf(sc->sc_rf_rev));

	mtx_unlock(&sc->sc_mtx);

	ifp = if_alloc(IFT_IEEE80211);

	mtx_lock(&sc->sc_mtx);

	if (ifp == NULL) {
		DPRINTFN(0, "could not if_alloc()!\n");
		goto done;
	}
	sc->sc_evilhack = ifp;
	sc->sc_ifp = ifp;
	ic = ifp->if_l2com;

	ifp->if_softc = sc;
	if_initname(ifp, "ural", sc->sc_unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = &ural_init_cb;
	ifp->if_ioctl = &ural_ioctl_cb;
	ifp->if_start = &ural_start_cb;
	ifp->if_watchdog = NULL;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	bcopy(sc->sc_myaddr, ic->ic_myaddr, sizeof(ic->ic_myaddr));

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_STA		/* station mode supported */
	    | IEEE80211_C_IBSS		/* IBSS mode supported */
	    | IEEE80211_C_MONITOR	/* monitor mode supported */
	    | IEEE80211_C_HOSTAP	/* HostAp mode supported */
	    | IEEE80211_C_TXPMGT	/* tx power management */
	    | IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	    | IEEE80211_C_SHSLOT	/* short slot time supported */
	    | IEEE80211_C_BGSCAN	/* bg scanning supported */
	    | IEEE80211_C_WPA		/* 802.11i */
	    ;

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);

	if (sc->sc_rf_rev == RAL_RF_5222) {
		setbit(&bands, IEEE80211_MODE_11A);
	}
	ieee80211_init_channels(ic, NULL, &bands);

	mtx_unlock(&sc->sc_mtx);

	ieee80211_ifattach(ic);

	mtx_lock(&sc->sc_mtx);

	ic->ic_newassoc = &ural_newassoc;
	ic->ic_raw_xmit = &ural_raw_xmit_cb;
	ic->ic_node_alloc = &ural_node_alloc;
	ic->ic_update_mcast = &ural_update_mcast_cb;
	ic->ic_update_promisc = &ural_update_promisc_cb;
	ic->ic_scan_start = &ural_scan_start_cb;
	ic->ic_scan_end = &ural_scan_end_cb;
	ic->ic_set_channel = &ural_set_channel_cb;
	ic->ic_vap_create = &ural_vap_create;
	ic->ic_vap_delete = &ural_vap_delete;

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);

	mtx_unlock(&sc->sc_mtx);

	bpfattach(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + sizeof(sc->sc_txtap));

	if (bootverbose) {
		ieee80211_announce(ic);
	}
	mtx_lock(&sc->sc_mtx);
done:
	return;
}

static void
ural_end_of_commands(struct ural_softc *sc)
{
	sc->sc_flags &= ~URAL_FLAG_WAIT_COMMAND;

	/* start write transfer, if not started */
	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
ural_config_copy_chan(struct ural_config_copy_chan *cc,
    struct ieee80211com *ic, struct ieee80211_channel *c)
{
	if (!c)
		return;
	cc->chan_to_ieee =
	    ieee80211_chan2ieee(ic, c);
	if (c != IEEE80211_CHAN_ANYC) {
		cc->chan_to_mode =
		    ieee80211_chan2mode(c);
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
ural_config_copy(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp;
	struct ieee80211com *ic;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	const struct ieee80211_txparam *tp;

	bzero(cc, sizeof(*cc));

	ifp = sc->sc_ifp;
	if (ifp) {
		cc->if_flags = ifp->if_flags;
		bcopy(ifp->if_broadcastaddr, cc->if_broadcastaddr,
		    sizeof(cc->if_broadcastaddr));

		ic = ifp->if_l2com;
		if (ic) {
			ural_config_copy_chan(&cc->ic_curchan, ic, ic->ic_curchan);
			ural_config_copy_chan(&cc->ic_bsschan, ic, ic->ic_bsschan);
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
	sc->sc_flags |= URAL_FLAG_WAIT_COMMAND;
	return;
}

static const char *
ural_get_rf(int rev)
{
	switch (rev) {
		case RAL_RF_2522:return "RT2522";
	case RAL_RF_2523:
		return "RT2523";
	case RAL_RF_2524:
		return "RT2524";
	case RAL_RF_2525:
		return "RT2525";
	case RAL_RF_2525E:
		return "RT2525e";
	case RAL_RF_2526:
		return "RT2526";
	case RAL_RF_5222:
		return "RT5222";
	default:
		return "unknown";
	}
}

/*------------------------------------------------------------------------*
 * ural_bulk_read_callback - data read "thread"
 *------------------------------------------------------------------------*/
static void
ural_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct ural_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_node *ni;
	struct mbuf *m = NULL;
	uint32_t flags;
	uint32_t max_len;
	uint32_t real_len;
	uint8_t rssi = 0;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTFN(15, "rx done, actlen=%d\n", xfer->actlen);

		if (xfer->actlen < RAL_RX_DESC_SIZE) {
			DPRINTF("too short transfer, "
			    "%d bytes\n", xfer->actlen);
			ifp->if_ierrors++;
			goto tr_setup;
		}
		max_len = (xfer->actlen - RAL_RX_DESC_SIZE);

		usb2_copy_out(xfer->frbuffers, max_len,
		    &sc->sc_rx_desc, RAL_RX_DESC_SIZE);

		flags = le32toh(sc->sc_rx_desc.flags);

		if (flags & (RAL_RX_PHY_ERROR | RAL_RX_CRC_ERROR)) {
			/*
		         * This should not happen since we did not
		         * request to receive those frames when we
		         * filled RAL_TXRX_CSR2:
		         */
			DPRINTFN(6, "PHY or CRC error\n");
			ifp->if_ierrors++;
			goto tr_setup;
		}
		if (max_len > MCLBYTES) {
			max_len = MCLBYTES;
		}
		real_len = (flags >> 16) & 0xfff;

		if (real_len > max_len) {
			DPRINTF("invalid length in RX "
			    "descriptor, %u bytes, received %u bytes\n",
			    real_len, max_len);
			ifp->if_ierrors++;
			goto tr_setup;
		}
		/* ieee80211_input() will check if the mbuf is too short */

		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);

		if (m == NULL) {
			DPRINTF("could not allocate mbuf\n");
			ifp->if_ierrors++;
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, 0, m->m_data, max_len);

		/* finalize mbuf */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = real_len;

		DPRINTF("real length=%d bytes\n", real_len);

		rssi = URAL_RSSI(sc->sc_rx_desc.rssi);

		if (bpf_peers_present(ifp->if_bpf)) {
			struct ural_rx_radiotap_header *tap = &sc->sc_rxtap;

			tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
			tap->wr_rate = ieee80211_plcp2rate(sc->sc_rx_desc.rate,
			    (sc->sc_rx_desc.flags & htole32(RAL_RX_OFDM)) ?
			    IEEE80211_T_OFDM : IEEE80211_T_CCK);

			tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
			tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
			tap->wr_antenna = sc->sc_rx_ant;
			tap->wr_antsignal = rssi;

			bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
		}
		/* Strip trailing 802.11 MAC FCS. */
		m_adj(m, -IEEE80211_CRC_LEN);

	case USB_ST_SETUP:
tr_setup:

		if (sc->sc_flags & URAL_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}

		/*
		 * At the end of a USB callback it is always safe to unlock
		 * the private mutex of a device! That is why we do the
		 * "ieee80211_input" here, and not some lines up!
		 */
		if (m) {
			mtx_unlock(&sc->sc_mtx);

			ni = ieee80211_find_rxnode(ic, (void *)(m->m_data));

			if (ni) {
				/* send the frame to the 802.11 layer */
				if (ieee80211_input(ni, m, rssi, RAL_NOISE_FLOOR, 0)) {
					/* ignore */
				}
				/* node is no longer needed */
				ieee80211_free_node(ni);
			} else {
				/* broadcast */
				if (ieee80211_input_all(ic, m, rssi, RAL_NOISE_FLOOR, 0)) {
					/* ignore */
				}
			}

			mtx_lock(&sc->sc_mtx);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= URAL_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;

	}
}

static void
ural_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ural_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~URAL_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static uint8_t
ural_plcp_signal(uint16_t rate)
{
	;				/* indent fix */
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

/*
 * We assume that "m->m_pkthdr.rcvif" is pointing to the "ni" that
 * should be freed, when "ural_setup_desc_and_tx" is called.
 */
static void
ural_setup_desc_and_tx(struct ural_softc *sc, struct mbuf *m,
    uint32_t flags, uint16_t rate)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct mbuf *mm;
	enum ieee80211_phytype phytype;
	uint16_t plcp_length;
	uint16_t len;
	uint8_t remainder;

	DPRINTF("in\n");

	if (sc->sc_tx_queue.ifq_len >= IFQ_MAXLEN) {
		/* free packet */
		ural_tx_freem(m);
		ifp->if_oerrors++;
		return;
	}
	if (!((sc->sc_flags & URAL_FLAG_LL_READY) &&
	    (sc->sc_flags & URAL_FLAG_HL_READY))) {
		/* free packet */
		ural_tx_freem(m);
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
		struct ural_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->sc_tx_ant;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m);
	}
	len = m->m_pkthdr.len;

	sc->sc_tx_desc.flags = htole32(flags);
	sc->sc_tx_desc.flags |= htole32(RAL_TX_NEWSEQ);
	sc->sc_tx_desc.flags |= htole32(len << 16);

	sc->sc_tx_desc.wme = htole16(RAL_AIFSN(2) |
	    RAL_LOGCWMIN(3) |
	    RAL_LOGCWMAX(5) |
	    RAL_IVOFFSET(sizeof(struct ieee80211_frame)));

	/* setup PLCP fields */
	sc->sc_tx_desc.plcp_signal = ural_plcp_signal(rate);
	sc->sc_tx_desc.plcp_service = 4;

	len += IEEE80211_CRC_LEN;

	phytype = ieee80211_rate2phytype(sc->sc_rates, rate);

	if (phytype == IEEE80211_T_OFDM) {
		sc->sc_tx_desc.flags |= htole32(RAL_TX_OFDM);

		plcp_length = len & 0xfff;
		sc->sc_tx_desc.plcp_length_hi = plcp_length >> 6;
		sc->sc_tx_desc.plcp_length_lo = plcp_length & 0x3f;

	} else {
		plcp_length = ((16 * len) + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if ((remainder != 0) && (remainder < 7)) {
				sc->sc_tx_desc.plcp_service |=
				    RAL_PLCP_LENGEXT;
			}
		}
		sc->sc_tx_desc.plcp_length_hi = plcp_length >> 8;
		sc->sc_tx_desc.plcp_length_lo = plcp_length & 0xff;

		if ((rate != 2) && (ic->ic_flags & IEEE80211_F_SHPREAMBLE)) {
			sc->sc_tx_desc.plcp_signal |= 0x08;
		}
	}

	sc->sc_tx_desc.iv = 0;
	sc->sc_tx_desc.eiv = 0;

	if (sizeof(sc->sc_tx_desc) > MHLEN) {
		DPRINTF("No room for header structure!\n");
		ural_tx_freem(m);
		return;
	}
	mm = m_gethdr(M_NOWAIT, MT_DATA);
	if (mm == NULL) {
		DPRINTF("Could not allocate header mbuf!\n");
		ural_tx_freem(m);
		return;
	}
	DPRINTF(" %zu %u (out)\n", sizeof(sc->sc_tx_desc), m->m_pkthdr.len);

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
ural_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct ural_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint16_t temp_len;
	uint8_t align;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete, %d bytes\n", xfer->actlen);

		ifp->if_opackets++;

	case USB_ST_SETUP:
		if (sc->sc_flags & URAL_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			break;
		}
		if (sc->sc_flags & URAL_FLAG_WAIT_COMMAND) {
			/*
			 * don't send anything while a command is pending !
			 */
			break;
		}
		ural_fill_write_queue(sc);

		_IF_DEQUEUE(&sc->sc_tx_queue, m);

		if (m) {

			if (m->m_pkthdr.len > (RAL_FRAME_SIZE + RAL_TX_DESC_SIZE)) {
				DPRINTFN(0, "data overflow, %u bytes\n",
				    m->m_pkthdr.len);
				m->m_pkthdr.len = (RAL_FRAME_SIZE + RAL_TX_DESC_SIZE);
			}
			usb2_m_copy_in(xfer->frbuffers, 0,
			    m, 0, m->m_pkthdr.len);

			/* compute transfer length */
			temp_len = m->m_pkthdr.len;

			/* make transfer length 16-bit aligned */
			align = (temp_len & 1);

			/* check if we need to add two extra bytes */
			if (((temp_len + align) % 64) == 0) {
				align += 2;
			}
			/* check if we need to align length */
			if (align != 0) {
				/* zero the extra bytes */
				usb2_bzero(xfer->frbuffers, temp_len, align);
				temp_len += align;
			}
			DPRINTFN(11, "sending frame len=%u xferlen=%u\n",
			    m->m_pkthdr.len, temp_len);

			xfer->frlengths[0] = temp_len;

			usb2_start_hardware(xfer);

			/* free mbuf and node */
			ural_tx_freem(m);
		}
		break;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= URAL_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		ifp->if_oerrors++;
		break;
	}
	return;
}

static void
ural_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ural_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~URAL_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ural_watchdog(void *arg)
{
	struct ural_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->sc_amrr_timer) {
		usb2_config_td_queue_command
		    (&sc->sc_config_td, NULL,
		    &ural_cfg_amrr_timeout, 0, 0);
	}
	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &ural_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

/*========================================================================*
 * IF-net callbacks
 *========================================================================*/

static void
ural_init_cb(void *arg)
{
	struct ural_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &ural_cfg_pre_init,
	    &ural_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static int
ural_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	int error;

	switch (cmd) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &ural_cfg_pre_init,
				    &ural_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &ural_cfg_pre_stop,
				    &ural_cfg_stop, 0, 0);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		error = 0;
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, (void *)data, &ic->ic_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
	}
	return (error);
}

static void
ural_start_cb(struct ifnet *ifp)
{
	struct ural_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	/* start write transfer, if not started */
	usb2_transfer_start(sc->sc_xfer[0]);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
ural_cfg_newstate(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ural_vap *uvp = URAL_VAP(vap);
	enum ieee80211_state ostate;
	enum ieee80211_state nstate;
	int arg;

	ostate = vap->iv_state;
	nstate = sc->sc_ns_state;
	arg = sc->sc_ns_arg;

	if (ostate == IEEE80211_S_INIT) {
		/* We are leaving INIT. TSF sync should be off. */
		ural_cfg_disable_tsf_sync(sc);
	}
	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_RUN:
		ural_cfg_set_run(sc, cc);
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

static int
ural_newstate_cb(struct ieee80211vap *vap,
    enum ieee80211_state nstate, int arg)
{
	struct ural_vap *uvp = URAL_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct ural_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF("setting new state: %d\n", nstate);

	/* Special case - cannot defer this call and cannot block ! */
	if (nstate == IEEE80211_S_INIT) {
		/* stop timers */
		mtx_lock(&sc->sc_mtx);
		sc->sc_amrr_timer = 0;
		mtx_unlock(&sc->sc_mtx);
		return (uvp->newstate(vap, nstate, arg));
	}
	mtx_lock(&sc->sc_mtx);
	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		mtx_unlock(&sc->sc_mtx);
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
	    (&sc->sc_config_td, &ural_config_copy,
	    &ural_cfg_newstate, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return (EINPROGRESS);
}

static void
ural_std_command(struct ieee80211com *ic, usb2_config_td_command_t *func)
{
	struct ural_softc *sc = ic->ic_ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &ural_config_copy, func, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
ural_scan_start_cb(struct ieee80211com *ic)
{
	ural_std_command(ic, &ural_cfg_scan_start);
	return;
}

static void
ural_scan_end_cb(struct ieee80211com *ic)
{
	ural_std_command(ic, &ural_cfg_scan_end);
	return;
}

static void
ural_set_channel_cb(struct ieee80211com *ic)
{
	ural_std_command(ic, &ural_cfg_set_chan);
	return;
}

/*========================================================================*
 * configure sub-routines, ural_cfg_xxx
 *========================================================================*/

static void
ural_cfg_scan_start(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* abort TSF synchronization */
	ural_cfg_disable_tsf_sync(sc);
	ural_cfg_set_bssid(sc, cc->if_broadcastaddr);
	return;
}

static void
ural_cfg_scan_end(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* enable TSF synchronization */
	ural_cfg_enable_tsf_sync(sc, cc, 0);
	ural_cfg_set_bssid(sc, cc->iv_bss.ni_bssid);
	return;
}

static void
ural_cfg_set_chan(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	enum {
		N_RF5222 = (sizeof(ural_rf5222) / sizeof(ural_rf5222[0])),
	};
	uint32_t i;
	uint32_t chan;
	uint8_t power;
	uint8_t tmp;

	chan = cc->ic_curchan.chan_to_ieee;

	if ((chan == 0) ||
	    (chan == IEEE80211_CHAN_ANY)) {
		/* nothing to do */
		return;
	}
	if (cc->ic_curchan.chan_is_2ghz)
		power = min(sc->sc_txpow[chan - 1], 31);
	else
		power = 31;

	/* adjust txpower using ifconfig settings */
	power -= (100 - cc->ic_txpowlimit) / 8;

	DPRINTFN(3, "setting channel to %u, "
	    "tx-power to %u\n", chan, power);

	switch (sc->sc_rf_rev) {
	case RAL_RF_2522:
		ural_cfg_rf_write(sc, RAL_RF1, 0x00814);
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2522_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x00040);
		break;

	case RAL_RF_2523:
		ural_cfg_rf_write(sc, RAL_RF1, 0x08804);
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2523_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x38044);
		ural_cfg_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2524:
		ural_cfg_rf_write(sc, RAL_RF1, 0x0c808);
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2524_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x00040);
		ural_cfg_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525:
		ural_cfg_rf_write(sc, RAL_RF1, 0x08808);
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2525_hi_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x18044);
		ural_cfg_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);

		ural_cfg_rf_write(sc, RAL_RF1, 0x08808);
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2525_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x18044);
		ural_cfg_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525E:
		ural_cfg_rf_write(sc, RAL_RF1, 0x08808);
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2525e_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x18044);
		ural_cfg_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00286 : 0x00282);
		break;

	case RAL_RF_2526:
		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2526_hi_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		ural_cfg_rf_write(sc, RAL_RF1, 0x08804);

		ural_cfg_rf_write(sc, RAL_RF2, ural_rf2526_r2[chan - 1]);
		ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x18044);
		ural_cfg_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		break;

		/* dual-band RF */
	case RAL_RF_5222:
		for (i = 0; i < N_RF5222; i++) {
			if (ural_rf5222[i].chan == chan) {
				ural_cfg_rf_write(sc, RAL_RF1, ural_rf5222[i].r1);
				ural_cfg_rf_write(sc, RAL_RF2, ural_rf5222[i].r2);
				ural_cfg_rf_write(sc, RAL_RF3, (power << 7) | 0x00040);
				ural_cfg_rf_write(sc, RAL_RF4, ural_rf5222[i].r4);
				break;
			}
		}
		break;
	}

	if ((cc->ic_opmode != IEEE80211_M_MONITOR) &&
	    (!(cc->ic_flags & IEEE80211_F_SCAN))) {

		/* set Japan filter bit for channel 14 */
		tmp = ural_cfg_bbp_read(sc, 70);

		if (chan == 14) {
			tmp |= RAL_JAPAN_FILTER;
		} else {
			tmp &= ~RAL_JAPAN_FILTER;
		}

		ural_cfg_bbp_write(sc, 70, tmp);

		/* clear CRC errors */
		ural_cfg_read(sc, RAL_STA_CSR0);

		ural_cfg_disable_rf_tune(sc);
	}
	/* update basic rate set */
	if (cc->ic_curchan.chan_is_b) {
		/* 11b basic rates: 1, 2Mbps */
		ural_cfg_write(sc, RAL_TXRX_CSR11, 0x3);
	} else if (cc->ic_curchan.chan_is_a) {
		/* 11a basic rates: 6, 12, 24Mbps */
		ural_cfg_write(sc, RAL_TXRX_CSR11, 0x150);
	} else {
		/* 11g basic rates: 1, 2, 5.5, 11, 6, 12, 24Mbps */
		ural_cfg_write(sc, RAL_TXRX_CSR11, 0x15f);
	}

	/* wait a little */
	if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
		return;
	}
	return;
}

static void
ural_cfg_set_run(struct ural_softc *sc,
    struct usb2_config_td_cc *cc)
{
	if (cc->ic_opmode != IEEE80211_M_MONITOR) {
		ural_cfg_update_slot(sc, cc, 0);
		ural_cfg_set_txpreamble(sc, cc, 0);

		/* update basic rate set */

		if (cc->ic_bsschan.chan_is_5ghz) {
			/* 11a basic rates: 6, 12, 24Mbps */
			ural_cfg_write(sc, RAL_TXRX_CSR11, 0x150);
		} else if (cc->ic_bsschan.chan_is_g) {
			/* 11g basic rates: 1, 2, 5.5, 11, 6, 12, 24Mbps */
			ural_cfg_write(sc, RAL_TXRX_CSR11, 0x15f);
		} else {
			/* 11b basic rates: 1, 2Mbps */
			ural_cfg_write(sc, RAL_TXRX_CSR11, 0x3);
		}
		ural_cfg_set_bssid(sc, cc->iv_bss.ni_bssid);
	}
	if ((cc->ic_opmode == IEEE80211_M_HOSTAP) ||
	    (cc->ic_opmode == IEEE80211_M_IBSS)) {
		ural_tx_bcn(sc);
	}
	/* make tx led blink on tx (controlled by ASIC) */
	ural_cfg_write(sc, RAL_MAC_CSR20, 1);

	if (cc->ic_opmode != IEEE80211_M_MONITOR) {
		ural_cfg_enable_tsf_sync(sc, cc, 0);
	}
	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_cfg_read_multi(sc, RAL_STA_CSR0, sc->sc_sta, sizeof(sc->sc_sta));

	if (cc->iv_bss.fixed_rate_none) {
		/* enable automatic rate adaptation */
		ural_cfg_amrr_start(sc);
	}
	return;
}

/*------------------------------------------------------------------------*
 * ural_cfg_disable_rf_tune - disable RF auto-tuning
 *------------------------------------------------------------------------*/
static void
ural_cfg_disable_rf_tune(struct ural_softc *sc)
{
	uint32_t tmp;

	if (sc->sc_rf_rev != RAL_RF_2523) {
		tmp = sc->sc_rf_regs[RAL_RF1] & ~RAL_RF1_AUTOTUNE;
		ural_cfg_rf_write(sc, RAL_RF1, tmp);
	}
	tmp = sc->sc_rf_regs[RAL_RF3] & ~RAL_RF3_AUTOTUNE;
	ural_cfg_rf_write(sc, RAL_RF3, tmp);

	DPRINTFN(3, "disabling RF autotune\n");

	return;
}

/*------------------------------------------------------------------------*
 * ural_cfg_enable_tsf_sync - refer to IEEE Std 802.11-1999 pp. 123
 * for more information on TSF synchronization
 *------------------------------------------------------------------------*/
static void
ural_cfg_enable_tsf_sync(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint16_t logcwmin;
	uint16_t preload;
	uint16_t tmp;

	/* first, disable TSF synchronization */
	ural_cfg_write(sc, RAL_TXRX_CSR19, 0);

	tmp = (16 * cc->iv_bss.ni_intval) << 4;
	ural_cfg_write(sc, RAL_TXRX_CSR18, tmp);

	logcwmin = (cc->ic_opmode == IEEE80211_M_IBSS) ? 2 : 0;
	preload = (cc->ic_opmode == IEEE80211_M_IBSS) ? 320 : 6;
	tmp = (logcwmin << 12) | preload;
	ural_cfg_write(sc, RAL_TXRX_CSR20, tmp);

	/* finally, enable TSF synchronization */
	tmp = RAL_ENABLE_TSF | RAL_ENABLE_TBCN;
	if (cc->ic_opmode == IEEE80211_M_STA)
		tmp |= RAL_ENABLE_TSF_SYNC(1);
	else
		tmp |= RAL_ENABLE_TSF_SYNC(2) | RAL_ENABLE_BEACON_GENERATOR;

	ural_cfg_write(sc, RAL_TXRX_CSR19, tmp);

	DPRINTF("enabling TSF synchronization\n");

	return;
}

static void
ural_cfg_disable_tsf_sync(struct ural_softc *sc)
{
	/* abort TSF synchronization */
	ural_cfg_write(sc, RAL_TXRX_CSR19, 0);

	/* force tx led to stop blinking */
	ural_cfg_write(sc, RAL_MAC_CSR20, 0);

	return;
}

#define	RAL_RXTX_TURNAROUND    5	/* us */

static void
ural_cfg_update_slot(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint16_t slottime;
	uint16_t sifs;
	uint16_t eifs;

	slottime = (cc->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	/*
	 * These settings may sound a bit inconsistent but this is what the
	 * reference driver does.
	 */
	if (cc->ic_curmode == IEEE80211_MODE_11B) {
		sifs = 16 - RAL_RXTX_TURNAROUND;
		eifs = 364;
	} else {
		sifs = 10 - RAL_RXTX_TURNAROUND;
		eifs = 64;
	}

	ural_cfg_write(sc, RAL_MAC_CSR10, slottime);
	ural_cfg_write(sc, RAL_MAC_CSR11, sifs);
	ural_cfg_write(sc, RAL_MAC_CSR12, eifs);
	return;
}

static void
ural_cfg_set_txpreamble(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint16_t tmp;

	tmp = ural_cfg_read(sc, RAL_TXRX_CSR10);

	if (cc->ic_flags & IEEE80211_F_SHPREAMBLE) {
		tmp |= RAL_SHORT_PREAMBLE;
	} else {
		tmp &= ~RAL_SHORT_PREAMBLE;
	}

	ural_cfg_write(sc, RAL_TXRX_CSR10, tmp);
	return;
}

static void
ural_cfg_set_bssid(struct ural_softc *sc, uint8_t *bssid)
{
	ural_cfg_write_multi(sc, RAL_MAC_CSR5, bssid, IEEE80211_ADDR_LEN);

	DPRINTF("setting BSSID to 0x%02x%02x%02x%02x%02x%02x\n",
	    bssid[5], bssid[4], bssid[3],
	    bssid[2], bssid[1], bssid[0]);
	return;
}

static void
ural_cfg_set_macaddr(struct ural_softc *sc, uint8_t *addr)
{
	ural_cfg_write_multi(sc, RAL_MAC_CSR2, addr, IEEE80211_ADDR_LEN);

	DPRINTF("setting MAC to 0x%02x%02x%02x%02x%02x%02x\n",
	    addr[5], addr[4], addr[3],
	    addr[2], addr[1], addr[0]);
	return;
}

static void
ural_cfg_update_promisc(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint16_t tmp;

	tmp = ural_cfg_read(sc, RAL_TXRX_CSR2);

	if (cc->if_flags & IFF_PROMISC) {
		tmp &= ~RAL_DROP_NOT_TO_ME;
	} else {
		tmp |= RAL_DROP_NOT_TO_ME;
	}

	ural_cfg_write(sc, RAL_TXRX_CSR2, tmp);

	DPRINTF("%s promiscuous mode\n",
	    (cc->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving");
	return;
}

static void
ural_cfg_set_txantenna(struct ural_softc *sc, uint8_t antenna)
{
	uint16_t tmp;
	uint8_t tx;

	tx = ural_cfg_bbp_read(sc, RAL_BBP_TX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		tx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		tx |= RAL_BBP_ANTB;
	else
		tx |= RAL_BBP_DIVERSITY;

	/* need to force I/Q flip for RF 2525e, 2526 and 5222 */
	if ((sc->sc_rf_rev == RAL_RF_2525E) ||
	    (sc->sc_rf_rev == RAL_RF_2526) ||
	    (sc->sc_rf_rev == RAL_RF_5222)) {
		tx |= RAL_BBP_FLIPIQ;
	}
	ural_cfg_bbp_write(sc, RAL_BBP_TX, tx);

	/* update values in PHY_CSR5 and PHY_CSR6 */
	tmp = ural_cfg_read(sc, RAL_PHY_CSR5) & ~0x7;
	ural_cfg_write(sc, RAL_PHY_CSR5, tmp | (tx & 0x7));

	tmp = ural_cfg_read(sc, RAL_PHY_CSR6) & ~0x7;
	ural_cfg_write(sc, RAL_PHY_CSR6, tmp | (tx & 0x7));

	return;
}

static void
ural_cfg_set_rxantenna(struct ural_softc *sc, uint8_t antenna)
{
	uint8_t rx;

	rx = ural_cfg_bbp_read(sc, RAL_BBP_RX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		rx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		rx |= RAL_BBP_ANTB;
	else
		rx |= RAL_BBP_DIVERSITY;

	/* need to force no I/Q flip for RF 2525e and 2526 */

	if ((sc->sc_rf_rev == RAL_RF_2525E) ||
	    (sc->sc_rf_rev == RAL_RF_2526)) {
		rx &= ~RAL_BBP_FLIPIQ;
	}
	ural_cfg_bbp_write(sc, RAL_BBP_RX, rx);
	return;
}

static void
ural_cfg_read_eeprom(struct ural_softc *sc)
{
	uint16_t val;

	ural_cfg_eeprom_read(sc, RAL_EEPROM_CONFIG0, &val, 2);

	val = le16toh(val);

	sc->sc_rf_rev = (val >> 11) & 0x7;
	sc->sc_hw_radio = (val >> 10) & 0x1;
	sc->sc_led_mode = (val >> 6) & 0x7;
	sc->sc_rx_ant = (val >> 4) & 0x3;
	sc->sc_tx_ant = (val >> 2) & 0x3;
	sc->sc_nb_ant = (val & 0x3);

	DPRINTF("val = 0x%04x\n", val);

	/* read MAC address */
	ural_cfg_eeprom_read(sc, RAL_EEPROM_ADDRESS, sc->sc_myaddr,
	    sizeof(sc->sc_myaddr));

	/* read default values for BBP registers */
	ural_cfg_eeprom_read(sc, RAL_EEPROM_BBP_BASE, sc->sc_bbp_prom,
	    sizeof(sc->sc_bbp_prom));

	/* read Tx power for all b/g channels */
	ural_cfg_eeprom_read(sc, RAL_EEPROM_TXPOWER, sc->sc_txpow,
	    sizeof(sc->sc_txpow));
	return;
}

static uint8_t
ural_cfg_bbp_init(struct ural_softc *sc)
{
	enum {
		N_DEF_BBP = (sizeof(ural_def_bbp) / sizeof(ural_def_bbp[0])),
	};
	uint16_t i;
	uint8_t to;

	/* wait for BBP to become ready */
	for (to = 0;; to++) {
		if (to < 100) {
			if (ural_cfg_bbp_read(sc, RAL_BBP_VERSION) != 0) {
				break;
			}
			if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
				return (1);	/* failure */
			}
		} else {
			DPRINTF("timeout waiting for BBP\n");
			return (1);	/* failure */
		}
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N_DEF_BBP; i++) {
		ural_cfg_bbp_write(sc, ural_def_bbp[i].reg,
		    ural_def_bbp[i].val);
	}

#if 0
	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->sc_bbp_prom[i].reg == 0xff) {
			continue;
		}
		ural_cfg_bbp_write(sc, sc->sc_bbp_prom[i].reg,
		    sc->sc_bbp_prom[i].val);
	}
#endif
	return (0);			/* success */
}

static void
ural_cfg_pre_init(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	/* immediate configuration */

	ural_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= URAL_FLAG_HL_READY;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));

	return;
}

static void
ural_cfg_init(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	enum {
		N_DEF_MAC = (sizeof(ural_def_mac) / sizeof(ural_def_mac[0])),
	};
	uint16_t tmp;
	uint16_t i;
	uint8_t to;

	/* delayed configuration */

	ural_cfg_set_testmode(sc);

	ural_cfg_write(sc, 0x308, 0x00f0);	/* XXX magic */

	ural_cfg_stop(sc, cc, 0);

	/* initialize MAC registers to default values */
	for (i = 0; i < N_DEF_MAC; i++) {
		ural_cfg_write(sc, ural_def_mac[i].reg,
		    ural_def_mac[i].val);
	}

	/* wait for BBP and RF to wake up (this can take a long time!) */
	for (to = 0;; to++) {
		if (to < 100) {
			tmp = ural_cfg_read(sc, RAL_MAC_CSR17);
			if ((tmp & (RAL_BBP_AWAKE | RAL_RF_AWAKE)) ==
			    (RAL_BBP_AWAKE | RAL_RF_AWAKE)) {
				break;
			}
			if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
				goto fail;
			}
		} else {
			DPRINTF("timeout waiting for "
			    "BBP/RF to wakeup\n");
			goto fail;
		}
	}

	/* we're ready! */
	ural_cfg_write(sc, RAL_MAC_CSR1, RAL_HOST_READY);

	/* set basic rate set (will be updated later) */
	ural_cfg_write(sc, RAL_TXRX_CSR11, 0x15f);

	if (ural_cfg_bbp_init(sc)) {
		goto fail;
	}
	/* set default BSS channel */
	ural_cfg_set_chan(sc, cc, 0);

	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_cfg_read_multi(sc, RAL_STA_CSR0, sc->sc_sta,
	    sizeof(sc->sc_sta));

	DPRINTF("rx_ant=%d, tx_ant=%d\n",
	    sc->sc_rx_ant, sc->sc_tx_ant);

	ural_cfg_set_txantenna(sc, sc->sc_tx_ant);
	ural_cfg_set_rxantenna(sc, sc->sc_rx_ant);

	ural_cfg_set_macaddr(sc, cc->ic_myaddr);

	/*
	 * make sure that the first transaction
	 * clears the stall:
	 */
	sc->sc_flags |= (URAL_FLAG_READ_STALL |
	    URAL_FLAG_WRITE_STALL |
	    URAL_FLAG_LL_READY);

	if ((sc->sc_flags & URAL_FLAG_LL_READY) &&
	    (sc->sc_flags & URAL_FLAG_HL_READY)) {
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
	/*
	 * start Rx
	 */
	tmp = RAL_DROP_PHY | RAL_DROP_CRC;
	if (cc->ic_opmode != IEEE80211_M_MONITOR) {

		tmp |= (RAL_DROP_CTL | RAL_DROP_BAD_VERSION);

		if (cc->ic_opmode != IEEE80211_M_HOSTAP) {
			tmp |= RAL_DROP_TODS;
		}
		if (!(cc->if_flags & IFF_PROMISC)) {
			tmp |= RAL_DROP_NOT_TO_ME;
		}
	}
	ural_cfg_write(sc, RAL_TXRX_CSR2, tmp);

	return;

fail:
	ural_cfg_pre_stop(sc, NULL, 0);

	if (cc) {
		ural_cfg_stop(sc, cc, 0);
	}
	return;
}

static void
ural_cfg_pre_stop(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		ural_config_copy(sc, cc, refcount);
	}
	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(URAL_FLAG_HL_READY |
	    URAL_FLAG_LL_READY);

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[0]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[3]);

	/* clean up transmission */
	ural_tx_clean_queue(sc);
	return;
}

static void
ural_cfg_stop(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* disable Rx */
	ural_cfg_write(sc, RAL_TXRX_CSR2, RAL_DISABLE_RX);

	/* reset ASIC and BBP (but won't reset MAC registers!) */
	ural_cfg_write(sc, RAL_MAC_CSR1, RAL_RESET_ASIC | RAL_RESET_BBP);

	/* wait a little */
	usb2_config_td_sleep(&sc->sc_config_td, hz / 10);

	/* clear reset */
	ural_cfg_write(sc, RAL_MAC_CSR1, 0);

	/* wait a little */
	usb2_config_td_sleep(&sc->sc_config_td, hz / 10);

	return;
}

static void
ural_cfg_amrr_start(struct ural_softc *sc)
{
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;

	vap = ural_get_vap(sc);

	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	/* init AMRR */

	ieee80211_amrr_node_init(&URAL_VAP(vap)->amrr, &URAL_NODE(ni)->amn, ni);
	/* enable AMRR timer */

	sc->sc_amrr_timer = 1;
	return;
}

static void
ural_cfg_amrr_timeout(struct ural_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;
	uint32_t ok;
	uint32_t fail;

	/* read and clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_cfg_read_multi(sc, RAL_STA_CSR0, sc->sc_sta, sizeof(sc->sc_sta));

	vap = ural_get_vap(sc);
	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	if ((sc->sc_flags & URAL_FLAG_LL_READY) &&
	    (sc->sc_flags & URAL_FLAG_HL_READY)) {

		ok = sc->sc_sta[7] +	/* TX ok w/o retry */
		    sc->sc_sta[8];	/* TX ok w/ retry */
		fail = sc->sc_sta[9];	/* TX retry-fail count */

		if (sc->sc_amrr_timer) {

			ieee80211_amrr_tx_update(&URAL_NODE(ni)->amn,
			    ok + fail, ok, sc->sc_sta[8] + fail);

			if (ieee80211_amrr_choose(ni, &URAL_NODE(ni)->amn)) {
				/* ignore */
			}
		}
		ifp->if_oerrors += fail;
	}
	return;
}

static struct ieee80211vap *
ural_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    int opmode, int flags, const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ural_vap *uvp;
	struct ieee80211vap *vap;
	struct ural_softc *sc = ic->ic_ifp->if_softc;

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
	uvp = (struct ural_vap *)malloc(sizeof(struct ural_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (uvp == NULL)
		return NULL;

	vap = &uvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = &ural_newstate_cb;

	ieee80211_amrr_init(&uvp->amrr, vap,
	    IEEE80211_AMRR_MIN_SUCCESS_THRESHOLD,
	    IEEE80211_AMRR_MAX_SUCCESS_THRESHOLD,
	    1000 /* 1 sec */ );

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change, ieee80211_media_status);

	/* store current operation mode */
	ic->ic_opmode = opmode;

	return (vap);
}

static void
ural_vap_delete(struct ieee80211vap *vap)
{
	struct ural_vap *uvp = URAL_VAP(vap);
	struct ural_softc *sc = vap->iv_ic->ic_ifp->if_softc;

	/* Need to sync with config thread: */
	mtx_lock(&sc->sc_mtx);
	if (usb2_config_td_sync(&sc->sc_config_td)) {
		/* ignore */
	}
	mtx_unlock(&sc->sc_mtx);

	ieee80211_amrr_cleanup(&uvp->amrr);
	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
	return;
}

/* ARGUSED */
static struct ieee80211_node *
ural_node_alloc(struct ieee80211vap *vap __unused,
    const uint8_t mac[IEEE80211_ADDR_LEN] __unused)
{
	struct ural_node *un;

	un = malloc(sizeof(struct ural_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	return ((un != NULL) ? &un->ni : NULL);
}

static void
ural_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ieee80211_amrr_node_init(&URAL_VAP(vap)->amrr, &URAL_NODE(ni)->amn, ni);
	return;
}

static void
ural_fill_write_queue(struct ural_softc *sc)
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
		ural_tx_data(sc, m, ni);
	}
	return;
}

static void
ural_tx_clean_queue(struct ural_softc *sc)
{
	struct mbuf *m;

	for (;;) {
		_IF_DEQUEUE(&sc->sc_tx_queue, m);

		if (!m) {
			break;
		}
		ural_tx_freem(m);
	}

	return;
}

static void
ural_tx_freem(struct mbuf *m)
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
ural_tx_mgt(struct ural_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_txparam *tp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	uint32_t flags;
	uint16_t dur;

	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];

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
	flags = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;

		dur = ieee80211_ack_duration(sc->sc_rates, tp->mgmtrate,
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		USETW(wh->i_dur, dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT &&
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= RAL_TX_TIMESTAMP;
	}
	m->m_pkthdr.rcvif = (void *)ni;
	ural_setup_desc_and_tx(sc, m, flags, tp->mgmtrate);
	return;
}

static struct ieee80211vap *
ural_get_vap(struct ural_softc *sc)
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

static void
ural_tx_bcn(struct ural_softc *sc)
{
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	const struct ieee80211_txparam *tp;
	struct mbuf *m;

	vap = ural_get_vap(sc);
	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	ic = vap->iv_ic;
	if (ic == NULL) {
		return;
	}
	DPRINTFN(11, "Sending beacon frame.\n");

	m = ieee80211_beacon_alloc(ni, &URAL_VAP(vap)->bo);
	if (m == NULL) {
		DPRINTFN(0, "could not allocate beacon\n");
		return;
	}
	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_bsschan)];

	m->m_pkthdr.rcvif = (void *)ieee80211_ref_node(ni);
	ural_setup_desc_and_tx(sc, m, RAL_TX_IFS_NEWBACKOFF | RAL_TX_TIMESTAMP,
	    tp->mgmtrate);
	return;
}

static void
ural_tx_data(struct ural_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_txparam *tp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	uint32_t flags = 0;
	uint16_t dur;
	uint16_t rate;

	DPRINTFN(11, "Sending data.\n");

	wh = mtod(m, struct ieee80211_frame *);

	tp = &vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)];
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		rate = tp->mcastrate;
	else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rate = tp->ucastrate;
	else
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
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		uint8_t prot = IEEE80211_PROT_NONE;

		if (m->m_pkthdr.len + IEEE80211_CRC_LEN > vap->iv_rtsthreshold)
			prot = IEEE80211_PROT_RTSCTS;
		else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    ieee80211_rate2phytype(sc->sc_rates, rate) == IEEE80211_T_OFDM)
			prot = ic->ic_protmode;
		if (prot != IEEE80211_PROT_NONE) {
			ural_tx_prot(sc, m, ni, prot, rate);
			flags |= RAL_TX_IFS_SIFS;
		}
		flags |= RAL_TX_ACK;
		flags |= RAL_TX_RETRY(7);

		dur = ieee80211_ack_duration(sc->sc_rates, rate,
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		USETW(wh->i_dur, dur);
	}
	m->m_pkthdr.rcvif = (void *)ni;
	ural_setup_desc_and_tx(sc, m, flags, rate);
	return;
}

static void
ural_tx_prot(struct ural_softc *sc,
    const struct mbuf *m, struct ieee80211_node *ni,
    uint8_t prot, uint16_t rate)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_frame *wh;
	struct mbuf *mprot;
	uint32_t flags;
	uint16_t protrate;
	uint16_t ackrate;
	uint16_t pktlen;
	uint16_t dur;
	uint8_t isshort;

	KASSERT((prot == IEEE80211_PROT_RTSCTS) ||
	    (prot == IEEE80211_PROT_CTSONLY),
	    ("protection %u", prot));

	DPRINTFN(16, "Sending protection frame.\n");

	wh = mtod(m, const struct ieee80211_frame *);
	pktlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;

	protrate = ieee80211_ctl_rate(sc->sc_rates, rate);
	ackrate = ieee80211_ack_rate(sc->sc_rates, rate);

	isshort = (ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0;
	dur = ieee80211_compute_duration(sc->sc_rates, pktlen, rate, isshort);
	+ieee80211_ack_duration(sc->sc_rates, rate, isshort);
	flags = RAL_TX_RETRY(7);
	if (prot == IEEE80211_PROT_RTSCTS) {
		/* NB: CTS is the same size as an ACK */
		dur += ieee80211_ack_duration(sc->sc_rates, rate, isshort);
		flags |= RAL_TX_ACK;
		mprot = ieee80211_alloc_rts(ic, wh->i_addr1, wh->i_addr2, dur);
	} else {
		mprot = ieee80211_alloc_cts(ic, ni->ni_vap->iv_myaddr, dur);
	}
	if (mprot == NULL) {
		return;
	}
	mprot->m_pkthdr.rcvif = (void *)ieee80211_ref_node(ni);
	ural_setup_desc_and_tx(sc, mprot, flags, protrate);
	return;
}

static void
ural_tx_raw(struct ural_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
    const struct ieee80211_bpf_params *params)
{
	uint32_t flags;
	uint16_t rate;

	DPRINTFN(11, "Sending raw frame.\n");

	rate = params->ibp_rate0 & IEEE80211_RATE_VAL;

	/* XXX validate */
	if (rate == 0) {
		m_freem(m);
		ieee80211_free_node(ni);
		return;
	}
	flags = 0;
	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0)
		flags |= RAL_TX_ACK;
	if (params->ibp_flags & (IEEE80211_BPF_RTS | IEEE80211_BPF_CTS)) {
		ural_tx_prot(sc, m, ni,
		    (params->ibp_flags & IEEE80211_BPF_RTS) ?
		    IEEE80211_PROT_RTSCTS : IEEE80211_PROT_CTSONLY,
		    rate);
		flags |= RAL_TX_IFS_SIFS;
	}
	m->m_pkthdr.rcvif = (void *)ni;
	ural_setup_desc_and_tx(sc, m, flags, rate);
	return;
}

static int
ural_raw_xmit_cb(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ural_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		ural_tx_mgt(sc, m, ni);
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		ural_tx_raw(sc, m, ni, params);
	}
	mtx_unlock(&sc->sc_mtx);
	return (0);
}

static void
ural_update_mcast_cb(struct ifnet *ifp)
{
	/* not supported */
	return;
}

static void
ural_update_promisc_cb(struct ifnet *ifp)
{
	struct ural_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &ural_config_copy,
	    &ural_cfg_update_promisc, 0, 0);
	mtx_unlock(&sc->sc_mtx);
	return;
}
