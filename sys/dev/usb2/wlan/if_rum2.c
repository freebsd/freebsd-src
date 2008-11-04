/*-
 * Copyright (c) 2005-2007 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
 * Copyright (c) 2007-2008 Hans Petter Selasky <hselasky@freebsd.org>
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
 * NOTE: all function names beginning like "rum_cfg_" can only
 * be called from within the config thread function !
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Ralink Technology RT2501USB/RT2601USB chipset driver
 * http://www.ralinktech.com.tw/
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc rum_config_copy
#define	usb2_config_td_softc rum_softc

#define	USB_DEBUG_VAR rum_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/wlan/usb2_wlan.h>
#include <dev/usb2/wlan/if_rum2_reg.h>
#include <dev/usb2/wlan/if_rum2_var.h>
#include <dev/usb2/wlan/if_rum2_fw.h>

#if USB_DEBUG
static int rum_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, rum, CTLFLAG_RW, 0, "USB rum");
SYSCTL_INT(_hw_usb2_rum, OID_AUTO, debug, CTLFLAG_RW, &rum_debug, 0,
    "Debug level");
#endif

/* prototypes */

static device_probe_t rum_probe;
static device_attach_t rum_attach;
static device_detach_t rum_detach;

static usb2_callback_t rum_bulk_read_callback;
static usb2_callback_t rum_bulk_read_clear_stall_callback;
static usb2_callback_t rum_bulk_write_callback;
static usb2_callback_t rum_bulk_write_clear_stall_callback;

static usb2_config_td_command_t rum_cfg_first_time_setup;
static usb2_config_td_command_t rum_config_copy;
static usb2_config_td_command_t rum_cfg_scan_start;
static usb2_config_td_command_t rum_cfg_scan_end;
static usb2_config_td_command_t rum_cfg_select_band;
static usb2_config_td_command_t rum_cfg_set_chan;
static usb2_config_td_command_t rum_cfg_enable_tsf_sync;
static usb2_config_td_command_t rum_cfg_enable_mrr;
static usb2_config_td_command_t rum_cfg_update_slot;
static usb2_config_td_command_t rum_cfg_select_antenna;
static usb2_config_td_command_t rum_cfg_set_txpreamble;
static usb2_config_td_command_t rum_cfg_update_promisc;
static usb2_config_td_command_t rum_cfg_pre_init;
static usb2_config_td_command_t rum_cfg_init;
static usb2_config_td_command_t rum_cfg_pre_stop;
static usb2_config_td_command_t rum_cfg_stop;
static usb2_config_td_command_t rum_cfg_amrr_timeout;
static usb2_config_td_command_t rum_cfg_prepare_beacon;
static usb2_config_td_command_t rum_cfg_newstate;

static const char *rum_get_rf(uint32_t rev);
static int rum_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data);
static void rum_std_command(struct ieee80211com *ic, usb2_config_td_command_t *func);
static void rum_scan_start_cb(struct ieee80211com *);
static void rum_scan_end_cb(struct ieee80211com *);
static void rum_set_channel_cb(struct ieee80211com *);
static uint16_t rum_cfg_eeprom_read_2(struct rum_softc *sc, uint16_t addr);
static uint32_t rum_cfg_bbp_disbusy(struct rum_softc *sc);
static uint32_t rum_cfg_read(struct rum_softc *sc, uint16_t reg);
static uint8_t rum_cfg_bbp_init(struct rum_softc *sc);
static uint8_t rum_cfg_bbp_read(struct rum_softc *sc, uint8_t reg);
static void rum_cfg_amrr_start(struct rum_softc *sc);
static void rum_cfg_bbp_write(struct rum_softc *sc, uint8_t reg, uint8_t val);
static void rum_cfg_do_request(struct rum_softc *sc, struct usb2_device_request *req, void *data);
static void rum_cfg_eeprom_read(struct rum_softc *sc, uint16_t addr, void *buf, uint16_t len);
static void rum_cfg_load_microcode(struct rum_softc *sc, const uint8_t *ucode, uint16_t size);
static void rum_cfg_read_eeprom(struct rum_softc *sc);
static void rum_cfg_read_multi(struct rum_softc *sc, uint16_t reg, void *buf, uint16_t len);
static void rum_cfg_rf_write(struct rum_softc *sc, uint8_t reg, uint32_t val);
static void rum_cfg_set_bssid(struct rum_softc *sc, uint8_t *bssid);
static void rum_cfg_set_macaddr(struct rum_softc *sc, uint8_t *addr);
static void rum_cfg_write(struct rum_softc *sc, uint16_t reg, uint32_t val);
static void rum_cfg_write_multi(struct rum_softc *sc, uint16_t reg, void *buf, uint16_t len);
static void rum_end_of_commands(struct rum_softc *sc);
static void rum_init_cb(void *arg);
static void rum_start_cb(struct ifnet *ifp);
static void rum_watchdog(void *arg);
static uint8_t rum_get_rssi(struct rum_softc *sc, uint8_t raw);
static struct ieee80211vap *rum_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit, int opmode, int flags, const uint8_t bssid[IEEE80211_ADDR_LEN], const uint8_t mac[IEEE80211_ADDR_LEN]);
static void rum_vap_delete(struct ieee80211vap *);
static struct ieee80211_node *rum_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN]);
static void rum_newassoc(struct ieee80211_node *, int);
static void rum_cfg_disable_tsf_sync(struct rum_softc *sc);
static void rum_cfg_set_run(struct rum_softc *sc, struct rum_config_copy *cc);
static void rum_fill_write_queue(struct rum_softc *sc);
static void rum_tx_clean_queue(struct rum_softc *sc);
static void rum_tx_freem(struct mbuf *m);
static void rum_tx_mgt(struct rum_softc *sc, struct mbuf *m, struct ieee80211_node *ni);
static struct ieee80211vap *rum_get_vap(struct rum_softc *sc);
static void rum_tx_data(struct rum_softc *sc, struct mbuf *m, struct ieee80211_node *ni);
static void rum_tx_prot(struct rum_softc *sc, const struct mbuf *m, struct ieee80211_node *ni, uint8_t prot, uint16_t rate);
static void rum_tx_raw(struct rum_softc *sc, struct mbuf *m, struct ieee80211_node *ni, const struct ieee80211_bpf_params *params);
static int rum_raw_xmit_cb(struct ieee80211_node *ni, struct mbuf *m, const struct ieee80211_bpf_params *params);
static void rum_setup_desc_and_tx(struct rum_softc *sc, struct mbuf *m, uint32_t flags, uint16_t xflags, uint16_t rate);
static int rum_newstate_cb(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg);
static void rum_update_mcast_cb(struct ifnet *ifp);
static void rum_update_promisc_cb(struct ifnet *ifp);

/* various supported device vendors/products */
static const struct usb2_device_id rum_devs[] = {
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_HWU54DM, 0)},
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_RT2573_2, 0)},
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_RT2573_3, 0)},
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_RT2573_4, 0)},
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_WUG2700, 0)},
	{USB_VPI(USB_VENDOR_AMIT, USB_PRODUCT_AMIT_CGWLUSB2GO, 0)},
	{USB_VPI(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_RT2573_1, 0)},
	{USB_VPI(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_RT2573_2, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D7050A, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D9050V3, 0)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_WUSB54GC, 0)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_WUSB54GR, 0)},
	{USB_VPI(USB_VENDOR_CONCEPTRONIC2, USB_PRODUCT_CONCEPTRONIC2_C54RU2, 0)},
	{USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_CGWLUSB2GL, 0)},
	{USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_CGWLUSB2GPX, 0)},
	{USB_VPI(USB_VENDOR_DICKSMITH, USB_PRODUCT_DICKSMITH_CWD854F, 0)},
	{USB_VPI(USB_VENDOR_DICKSMITH, USB_PRODUCT_DICKSMITH_RT2573, 0)},
	{USB_VPI(USB_VENDOR_DLINK2, USB_PRODUCT_DLINK2_DWLG122C1, 0)},
	{USB_VPI(USB_VENDOR_DLINK2, USB_PRODUCT_DLINK2_WUA1340, 0)},
	{USB_VPI(USB_VENDOR_DLINK2, USB_PRODUCT_DLINK2_DWA111, 0)},
	{USB_VPI(USB_VENDOR_DLINK2, USB_PRODUCT_DLINK2_DWA110, 0)},
	{USB_VPI(USB_VENDOR_GIGABYTE, USB_PRODUCT_GIGABYTE_GNWB01GS, 0)},
	{USB_VPI(USB_VENDOR_GIGABYTE, USB_PRODUCT_GIGABYTE_GNWI05GS, 0)},
	{USB_VPI(USB_VENDOR_GIGASET, USB_PRODUCT_GIGASET_RT2573, 0)},
	{USB_VPI(USB_VENDOR_GOODWAY, USB_PRODUCT_GOODWAY_RT2573, 0)},
	{USB_VPI(USB_VENDOR_GUILLEMOT, USB_PRODUCT_GUILLEMOT_HWGUSB254LB, 0)},
	{USB_VPI(USB_VENDOR_GUILLEMOT, USB_PRODUCT_GUILLEMOT_HWGUSB254V2AP, 0)},
	{USB_VPI(USB_VENDOR_HUAWEI3COM, USB_PRODUCT_HUAWEI3COM_WUB320G, 0)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_G54HP, 0)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_SG54HP, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2573_1, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2573_2, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2573_3, 0)},
	{USB_VPI(USB_VENDOR_MSI, USB_PRODUCT_MSI_RT2573_4, 0)},
	{USB_VPI(USB_VENDOR_NOVATECH, USB_PRODUCT_NOVATECH_RT2573, 0)},
	{USB_VPI(USB_VENDOR_PLANEX2, USB_PRODUCT_PLANEX2_GWUS54HP, 0)},
	{USB_VPI(USB_VENDOR_PLANEX2, USB_PRODUCT_PLANEX2_GWUS54MINI2, 0)},
	{USB_VPI(USB_VENDOR_PLANEX2, USB_PRODUCT_PLANEX2_GWUSMM, 0)},
	{USB_VPI(USB_VENDOR_QCOM, USB_PRODUCT_QCOM_RT2573, 0)},
	{USB_VPI(USB_VENDOR_QCOM, USB_PRODUCT_QCOM_RT2573_2, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2573, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2573_2, 0)},
	{USB_VPI(USB_VENDOR_RALINK, USB_PRODUCT_RALINK_RT2671, 0)},
	{USB_VPI(USB_VENDOR_SITECOMEU, USB_PRODUCT_SITECOMEU_WL113R2, 0)},
	{USB_VPI(USB_VENDOR_SITECOMEU, USB_PRODUCT_SITECOMEU_WL172, 0)},
	{USB_VPI(USB_VENDOR_SPARKLAN, USB_PRODUCT_SPARKLAN_RT2573, 0)},
	{USB_VPI(USB_VENDOR_SURECOM, USB_PRODUCT_SURECOM_RT2573, 0)},
};

struct rum_def_mac {
	uint32_t reg;
	uint32_t val;
};

static const struct rum_def_mac rum_def_mac[] = {
	{RT2573_TXRX_CSR0, 0x025fb032},
	{RT2573_TXRX_CSR1, 0x9eaa9eaf},
	{RT2573_TXRX_CSR2, 0x8a8b8c8d},
	{RT2573_TXRX_CSR3, 0x00858687},
	{RT2573_TXRX_CSR7, 0x2e31353b},
	{RT2573_TXRX_CSR8, 0x2a2a2a2c},
	{RT2573_TXRX_CSR15, 0x0000000f},
	{RT2573_MAC_CSR6, 0x00000fff},
	{RT2573_MAC_CSR8, 0x016c030a},
	{RT2573_MAC_CSR10, 0x00000718},
	{RT2573_MAC_CSR12, 0x00000004},
	{RT2573_MAC_CSR13, 0x00007f00},
	{RT2573_SEC_CSR0, 0x00000000},
	{RT2573_SEC_CSR1, 0x00000000},
	{RT2573_SEC_CSR5, 0x00000000},
	{RT2573_PHY_CSR1, 0x000023b0},
	{RT2573_PHY_CSR5, 0x00040a06},
	{RT2573_PHY_CSR6, 0x00080606},
	{RT2573_PHY_CSR7, 0x00000408},
	{RT2573_AIFSN_CSR, 0x00002273},
	{RT2573_CWMIN_CSR, 0x00002344},
	{RT2573_CWMAX_CSR, 0x000034aa}
};

struct rum_def_bbp {
	uint8_t	reg;
	uint8_t	val;
};

static const struct rum_def_bbp rum_def_bbp[] = {
	{3, 0x80},
	{15, 0x30},
	{17, 0x20},
	{21, 0xc8},
	{22, 0x38},
	{23, 0x06},
	{24, 0xfe},
	{25, 0x0a},
	{26, 0x0d},
	{32, 0x0b},
	{34, 0x12},
	{37, 0x07},
	{39, 0xf8},
	{41, 0x60},
	{53, 0x10},
	{54, 0x18},
	{60, 0x10},
	{61, 0x04},
	{62, 0x04},
	{75, 0xfe},
	{86, 0xfe},
	{88, 0xfe},
	{90, 0x0f},
	{99, 0x00},
	{102, 0x16},
	{107, 0x04}
};

struct rfprog {
	uint8_t	chan;
	uint32_t r1, r2, r3, r4;
};

static const struct rfprog rum_rf5226[] = {
	{1, 0x00b03, 0x001e1, 0x1a014, 0x30282},
	{2, 0x00b03, 0x001e1, 0x1a014, 0x30287},
	{3, 0x00b03, 0x001e2, 0x1a014, 0x30282},
	{4, 0x00b03, 0x001e2, 0x1a014, 0x30287},
	{5, 0x00b03, 0x001e3, 0x1a014, 0x30282},
	{6, 0x00b03, 0x001e3, 0x1a014, 0x30287},
	{7, 0x00b03, 0x001e4, 0x1a014, 0x30282},
	{8, 0x00b03, 0x001e4, 0x1a014, 0x30287},
	{9, 0x00b03, 0x001e5, 0x1a014, 0x30282},
	{10, 0x00b03, 0x001e5, 0x1a014, 0x30287},
	{11, 0x00b03, 0x001e6, 0x1a014, 0x30282},
	{12, 0x00b03, 0x001e6, 0x1a014, 0x30287},
	{13, 0x00b03, 0x001e7, 0x1a014, 0x30282},
	{14, 0x00b03, 0x001e8, 0x1a014, 0x30284},

	{34, 0x00b03, 0x20266, 0x36014, 0x30282},
	{38, 0x00b03, 0x20267, 0x36014, 0x30284},
	{42, 0x00b03, 0x20268, 0x36014, 0x30286},
	{46, 0x00b03, 0x20269, 0x36014, 0x30288},

	{36, 0x00b03, 0x00266, 0x26014, 0x30288},
	{40, 0x00b03, 0x00268, 0x26014, 0x30280},
	{44, 0x00b03, 0x00269, 0x26014, 0x30282},
	{48, 0x00b03, 0x0026a, 0x26014, 0x30284},
	{52, 0x00b03, 0x0026b, 0x26014, 0x30286},
	{56, 0x00b03, 0x0026c, 0x26014, 0x30288},
	{60, 0x00b03, 0x0026e, 0x26014, 0x30280},
	{64, 0x00b03, 0x0026f, 0x26014, 0x30282},

	{100, 0x00b03, 0x0028a, 0x2e014, 0x30280},
	{104, 0x00b03, 0x0028b, 0x2e014, 0x30282},
	{108, 0x00b03, 0x0028c, 0x2e014, 0x30284},
	{112, 0x00b03, 0x0028d, 0x2e014, 0x30286},
	{116, 0x00b03, 0x0028e, 0x2e014, 0x30288},
	{120, 0x00b03, 0x002a0, 0x2e014, 0x30280},
	{124, 0x00b03, 0x002a1, 0x2e014, 0x30282},
	{128, 0x00b03, 0x002a2, 0x2e014, 0x30284},
	{132, 0x00b03, 0x002a3, 0x2e014, 0x30286},
	{136, 0x00b03, 0x002a4, 0x2e014, 0x30288},
	{140, 0x00b03, 0x002a6, 0x2e014, 0x30280},

	{149, 0x00b03, 0x002a8, 0x2e014, 0x30287},
	{153, 0x00b03, 0x002a9, 0x2e014, 0x30289},
	{157, 0x00b03, 0x002ab, 0x2e014, 0x30281},
	{161, 0x00b03, 0x002ac, 0x2e014, 0x30283},
	{165, 0x00b03, 0x002ad, 0x2e014, 0x30285}
};

static const struct rfprog rum_rf5225[] = {
	{1, 0x00b33, 0x011e1, 0x1a014, 0x30282},
	{2, 0x00b33, 0x011e1, 0x1a014, 0x30287},
	{3, 0x00b33, 0x011e2, 0x1a014, 0x30282},
	{4, 0x00b33, 0x011e2, 0x1a014, 0x30287},
	{5, 0x00b33, 0x011e3, 0x1a014, 0x30282},
	{6, 0x00b33, 0x011e3, 0x1a014, 0x30287},
	{7, 0x00b33, 0x011e4, 0x1a014, 0x30282},
	{8, 0x00b33, 0x011e4, 0x1a014, 0x30287},
	{9, 0x00b33, 0x011e5, 0x1a014, 0x30282},
	{10, 0x00b33, 0x011e5, 0x1a014, 0x30287},
	{11, 0x00b33, 0x011e6, 0x1a014, 0x30282},
	{12, 0x00b33, 0x011e6, 0x1a014, 0x30287},
	{13, 0x00b33, 0x011e7, 0x1a014, 0x30282},
	{14, 0x00b33, 0x011e8, 0x1a014, 0x30284},

	{34, 0x00b33, 0x01266, 0x26014, 0x30282},
	{38, 0x00b33, 0x01267, 0x26014, 0x30284},
	{42, 0x00b33, 0x01268, 0x26014, 0x30286},
	{46, 0x00b33, 0x01269, 0x26014, 0x30288},

	{36, 0x00b33, 0x01266, 0x26014, 0x30288},
	{40, 0x00b33, 0x01268, 0x26014, 0x30280},
	{44, 0x00b33, 0x01269, 0x26014, 0x30282},
	{48, 0x00b33, 0x0126a, 0x26014, 0x30284},
	{52, 0x00b33, 0x0126b, 0x26014, 0x30286},
	{56, 0x00b33, 0x0126c, 0x26014, 0x30288},
	{60, 0x00b33, 0x0126e, 0x26014, 0x30280},
	{64, 0x00b33, 0x0126f, 0x26014, 0x30282},

	{100, 0x00b33, 0x0128a, 0x2e014, 0x30280},
	{104, 0x00b33, 0x0128b, 0x2e014, 0x30282},
	{108, 0x00b33, 0x0128c, 0x2e014, 0x30284},
	{112, 0x00b33, 0x0128d, 0x2e014, 0x30286},
	{116, 0x00b33, 0x0128e, 0x2e014, 0x30288},
	{120, 0x00b33, 0x012a0, 0x2e014, 0x30280},
	{124, 0x00b33, 0x012a1, 0x2e014, 0x30282},
	{128, 0x00b33, 0x012a2, 0x2e014, 0x30284},
	{132, 0x00b33, 0x012a3, 0x2e014, 0x30286},
	{136, 0x00b33, 0x012a4, 0x2e014, 0x30288},
	{140, 0x00b33, 0x012a6, 0x2e014, 0x30280},

	{149, 0x00b33, 0x012a8, 0x2e014, 0x30287},
	{153, 0x00b33, 0x012a9, 0x2e014, 0x30289},
	{157, 0x00b33, 0x012ab, 0x2e014, 0x30281},
	{161, 0x00b33, 0x012ac, 0x2e014, 0x30283},
	{165, 0x00b33, 0x012ad, 0x2e014, 0x30285}
};

static const struct usb2_config rum_config[RUM_N_TRANSFER] = {
	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = (MCLBYTES + RT2573_TX_DESC_SIZE + 8),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &rum_bulk_write_callback,
		.mh.timeout = 5000,	/* ms */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = (MCLBYTES + RT2573_RX_DESC_SIZE),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &rum_bulk_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &rum_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &rum_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static devclass_t rum_devclass;

static device_method_t rum_methods[] = {
	DEVMETHOD(device_probe, rum_probe),
	DEVMETHOD(device_attach, rum_attach),
	DEVMETHOD(device_detach, rum_detach),
	{0, 0}
};

static driver_t rum_driver = {
	.name = "rum",
	.methods = rum_methods,
	.size = sizeof(struct rum_softc),
};

DRIVER_MODULE(rum, ushub, rum_driver, rum_devclass, NULL, 0);
MODULE_DEPEND(rum, usb2_wlan, 1, 1, 1);
MODULE_DEPEND(rum, usb2_core, 1, 1, 1);
MODULE_DEPEND(rum, wlan, 1, 1, 1);
MODULE_DEPEND(rum, wlan_amrr, 1, 1, 1);

static int
rum_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != 0) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != RT2573_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(rum_devs, sizeof(rum_devs), uaa));
}

static int
rum_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct rum_softc *sc = device_get_softc(dev);
	int error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "rum lock", MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	snprintf(sc->sc_name, sizeof(sc->sc_name), "%s",
	    device_get_nameunit(dev));

	sc->sc_udev = uaa->device;
	sc->sc_unit = device_get_unit(dev);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	iface_index = RT2573_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, rum_config, RUM_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "could not allocate USB transfers, "
		    "err=%s\n", usb2_errstr(error));
		goto detach;
	}
	error = usb2_config_td_setup(&sc->sc_config_td, sc, &sc->sc_mtx,
	    &rum_end_of_commands,
	    sizeof(struct usb2_config_td_cc), 24);
	if (error) {
		device_printf(dev, "could not setup config "
		    "thread!\n");
		goto detach;
	}
	mtx_lock(&sc->sc_mtx);

	/* start setup */

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &rum_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	rum_watchdog(sc);

	return (0);			/* success */

detach:
	rum_detach(dev);
	return (ENXIO);			/* failure */
}

static int
rum_detach(device_t dev)
{
	struct rum_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic;
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	rum_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;
	ic = ifp->if_l2com;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, RUM_N_TRANSFER);

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

static void
rum_cfg_do_request(struct rum_softc *sc, struct usb2_device_request *req,
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
rum_cfg_eeprom_read(struct rum_softc *sc, uint16_t addr, void *buf, uint16_t len)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_EEPROM;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	rum_cfg_do_request(sc, &req, buf);
	return;
}

static uint16_t
rum_cfg_eeprom_read_2(struct rum_softc *sc, uint16_t addr)
{
	uint16_t tmp;

	rum_cfg_eeprom_read(sc, addr, &tmp, sizeof(tmp));
	return (le16toh(tmp));
}

static uint32_t
rum_cfg_read(struct rum_softc *sc, uint16_t reg)
{
	uint32_t val;

	rum_cfg_read_multi(sc, reg, &val, sizeof(val));
	return (le32toh(val));
}

static void
rum_cfg_read_multi(struct rum_softc *sc, uint16_t reg, void *buf, uint16_t len)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	rum_cfg_do_request(sc, &req, buf);
	return;
}

static void
rum_cfg_write(struct rum_softc *sc, uint16_t reg, uint32_t val)
{
	uint32_t tmp = htole32(val);

	rum_cfg_write_multi(sc, reg, &tmp, sizeof(tmp));
	return;
}

static void
rum_cfg_write_multi(struct rum_softc *sc, uint16_t reg, void *buf, uint16_t len)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2573_WRITE_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	rum_cfg_do_request(sc, &req, buf);
	return;
}

static uint32_t
rum_cfg_bbp_disbusy(struct rum_softc *sc)
{
	uint32_t tmp;
	uint8_t to;

	for (to = 0;; to++) {
		if (to < 100) {
			tmp = rum_cfg_read(sc, RT2573_PHY_CSR3);

			if ((tmp & RT2573_BBP_BUSY) == 0) {
				return (tmp);
			}
			if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
				break;
			}
		} else {
			break;
		}
	}
	DPRINTF("could not disbusy BBP\n");
	return (RT2573_BBP_BUSY);	/* failure */
}

static void
rum_cfg_bbp_write(struct rum_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;

	if (rum_cfg_bbp_disbusy(sc) & RT2573_BBP_BUSY) {
		return;
	}
	tmp = RT2573_BBP_BUSY | ((reg & 0x7f) << 8) | val;
	rum_cfg_write(sc, RT2573_PHY_CSR3, tmp);
	return;
}

static uint8_t
rum_cfg_bbp_read(struct rum_softc *sc, uint8_t reg)
{
	uint32_t val;

	if (rum_cfg_bbp_disbusy(sc) & RT2573_BBP_BUSY) {
		return (0);
	}
	val = RT2573_BBP_BUSY | RT2573_BBP_READ | (reg << 8);
	rum_cfg_write(sc, RT2573_PHY_CSR3, val);

	val = rum_cfg_bbp_disbusy(sc);
	return (val & 0xff);
}

static void
rum_cfg_rf_write(struct rum_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	uint8_t to;

	reg &= 3;

	for (to = 0;; to++) {
		if (to < 100) {
			tmp = rum_cfg_read(sc, RT2573_PHY_CSR4);
			if (!(tmp & RT2573_RF_BUSY)) {
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

	tmp = RT2573_RF_BUSY | RT2573_RF_20BIT | ((val & 0xfffff) << 2) | reg;
	rum_cfg_write(sc, RT2573_PHY_CSR4, tmp);

	DPRINTFN(16, "RF R[%u] <- 0x%05x\n", reg, val & 0xfffff);
	return;
}

static void
rum_cfg_first_time_setup(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ieee80211com *ic;
	struct ifnet *ifp;
	uint32_t tmp;
	uint16_t i;
	uint8_t bands;

	/* setup RX tap header */
	sc->sc_rxtap_len = sizeof(sc->sc_rxtap);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RT2573_RX_RADIOTAP_PRESENT);

	/* setup TX tap header */
	sc->sc_txtap_len = sizeof(sc->sc_txtap);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RT2573_TX_RADIOTAP_PRESENT);

	/* retrieve RT2573 rev. no */
	for (i = 0; i < 100; i++) {

		tmp = rum_cfg_read(sc, RT2573_MAC_CSR0);
		if (tmp != 0) {
			break;
		}
		/* wait a little */
		if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
			/* device detached */
			goto done;
		}
	}

	if (tmp == 0) {
		DPRINTF("chip is maybe not ready\n");
	}
	/* retrieve MAC address and various other things from EEPROM */
	rum_cfg_read_eeprom(sc);

	printf("%s: MAC/BBP RT2573 (rev 0x%05x), RF %s\n",
	    sc->sc_name, tmp, rum_get_rf(sc->sc_rf_rev));

	rum_cfg_load_microcode(sc, rt2573_ucode, sizeof(rt2573_ucode));

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
	if_initname(ifp, "rum", sc->sc_unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = &rum_init_cb;
	ifp->if_ioctl = &rum_ioctl_cb;
	ifp->if_start = &rum_start_cb;
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
	ieee80211_init_channels(ic, NULL, &bands);

	if ((sc->sc_rf_rev == RT2573_RF_5225) ||
	    (sc->sc_rf_rev == RT2573_RF_5226)) {

		struct ieee80211_channel *c;

		/* set supported .11a channels */
		for (i = 34; i <= 46; i += 4) {
			c = ic->ic_channels + (ic->ic_nchans++);
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
		for (i = 36; i <= 64; i += 4) {
			c = ic->ic_channels + (ic->ic_nchans++);
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
		for (i = 100; i <= 140; i += 4) {
			c = ic->ic_channels + (ic->ic_nchans++);
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
		for (i = 149; i <= 165; i += 4) {
			c = ic->ic_channels + (ic->ic_nchans++);
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
	}
	mtx_unlock(&sc->sc_mtx);

	ieee80211_ifattach(ic);

	mtx_lock(&sc->sc_mtx);

	ic->ic_newassoc = &rum_newassoc;
	ic->ic_raw_xmit = &rum_raw_xmit_cb;
	ic->ic_node_alloc = &rum_node_alloc;
	ic->ic_update_mcast = &rum_update_mcast_cb;
	ic->ic_update_promisc = &rum_update_promisc_cb;
	ic->ic_scan_start = &rum_scan_start_cb;
	ic->ic_scan_end = &rum_scan_end_cb;
	ic->ic_set_channel = &rum_set_channel_cb;
	ic->ic_vap_create = &rum_vap_create;
	ic->ic_vap_delete = &rum_vap_delete;

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
rum_end_of_commands(struct rum_softc *sc)
{
	sc->sc_flags &= ~RUM_FLAG_WAIT_COMMAND;

	/* start write transfer, if not started */
	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
rum_config_copy_chan(struct rum_config_copy_chan *cc,
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
rum_config_copy(struct rum_softc *sc,
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
			rum_config_copy_chan(&cc->ic_curchan, ic, ic->ic_curchan);
			rum_config_copy_chan(&cc->ic_bsschan, ic, ic->ic_bsschan);
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
	sc->sc_flags |= RUM_FLAG_WAIT_COMMAND;
	return;
}

static const char *
rum_get_rf(uint32_t rev)
{
	;				/* indent fix */
	switch (rev) {
	case RT2573_RF_2527:
		return "RT2527 (MIMO XR)";
	case RT2573_RF_2528:
		return "RT2528";
	case RT2573_RF_5225:
		return "RT5225 (MIMO XR)";
	case RT2573_RF_5226:
		return "RT5226";
	default:
		return "unknown";
	}
}

static void
rum_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct rum_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_node *ni;

	struct mbuf *m = NULL;
	uint32_t flags;
	uint32_t max_len;
	uint8_t rssi = 0;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTFN(15, "rx done, actlen=%d\n", xfer->actlen);

		if (xfer->actlen < (RT2573_RX_DESC_SIZE + IEEE80211_MIN_LEN)) {
			DPRINTF("too short transfer, "
			    "%d bytes\n", xfer->actlen);
			ifp->if_ierrors++;
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, 0,
		    &sc->sc_rx_desc, RT2573_RX_DESC_SIZE);

		flags = le32toh(sc->sc_rx_desc.flags);

		if (flags & RT2573_RX_CRC_ERROR) {
			/*
		         * This should not happen since we did not
		         * request to receive those frames when we
		         * filled RAL_TXRX_CSR2:
		         */
			DPRINTFN(6, "PHY or CRC error\n");
			ifp->if_ierrors++;
			goto tr_setup;
		}
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);

		if (m == NULL) {
			DPRINTF("could not allocate mbuf\n");
			ifp->if_ierrors++;
			goto tr_setup;
		}
		max_len = (xfer->actlen - RT2573_RX_DESC_SIZE);

		usb2_copy_out(xfer->frbuffers, RT2573_RX_DESC_SIZE,
		    m->m_data, max_len);

		/* finalize mbuf */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = (flags >> 16) & 0xfff;

		if (m->m_len > max_len) {
			DPRINTF("invalid length in RX "
			    "descriptor, %u bytes, received %u bytes\n",
			    m->m_len, max_len);
			ifp->if_ierrors++;
			m_freem(m);
			m = NULL;
			goto tr_setup;
		}
		rssi = rum_get_rssi(sc, sc->sc_rx_desc.rssi);

		DPRINTF("real length=%d bytes, rssi=%d\n", m->m_len, rssi);

		if (bpf_peers_present(ifp->if_bpf)) {
			struct rum_rx_radiotap_header *tap = &sc->sc_rxtap;

			tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
			tap->wr_rate = ieee80211_plcp2rate(sc->sc_rx_desc.rate,
			    (sc->sc_rx_desc.flags & htole32(RT2573_RX_OFDM)) ?
			    IEEE80211_T_OFDM : IEEE80211_T_CCK);
			tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
			tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
			tap->wr_antenna = sc->sc_rx_ant;
			tap->wr_antsignal = rssi;

			bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
		}
	case USB_ST_SETUP:
tr_setup:

		if (sc->sc_flags & RUM_FLAG_READ_STALL) {
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

			ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));
			if (ni != NULL) {
				if (ieee80211_input(ni, m, rssi, RT2573_NOISE_FLOOR, 0)) {
					/* ignore */
				}
				/* node is no longer needed */
				ieee80211_free_node(ni);
			} else {
				if (ieee80211_input_all(ic, m, rssi, RT2573_NOISE_FLOOR, 0)) {
					/* ignore */
				}
			}

			mtx_lock(&sc->sc_mtx);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= RUM_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;

	}
}

static void
rum_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct rum_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~RUM_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static uint8_t
rum_plcp_signal(uint16_t rate)
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
 * should be freed, when "rum_setup_desc_and_tx" is called.
 */

static void
rum_setup_desc_and_tx(struct rum_softc *sc, struct mbuf *m, uint32_t flags,
    uint16_t xflags, uint16_t rate)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct mbuf *mm;
	enum ieee80211_phytype phytype;
	uint16_t plcp_length;
	uint16_t len;
	uint8_t remainder;
	uint8_t is_beacon;

	if (xflags & RT2573_TX_BEACON) {
		xflags &= ~RT2573_TX_BEACON;
		is_beacon = 1;
	} else {
		is_beacon = 0;
	}

	if (sc->sc_tx_queue.ifq_len >= IFQ_MAXLEN) {
		/* free packet */
		rum_tx_freem(m);
		ifp->if_oerrors++;
		return;
	}
	if (!((sc->sc_flags & RUM_FLAG_LL_READY) &&
	    (sc->sc_flags & RUM_FLAG_HL_READY))) {
		/* free packet */
		rum_tx_freem(m);
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
		struct rum_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->sc_tx_ant;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m);
	}
	len = m->m_pkthdr.len;

	flags |= RT2573_TX_VALID;
	flags |= (len << 16);

	sc->sc_tx_desc.flags = htole32(flags);
	sc->sc_tx_desc.xflags = htole16(xflags);

	sc->sc_tx_desc.wme = htole16(RT2573_QID(0) | RT2573_AIFSN(2) |
	    RT2573_LOGCWMIN(4) | RT2573_LOGCWMAX(10));

	/* setup PLCP fields */
	sc->sc_tx_desc.plcp_signal = rum_plcp_signal(rate);
	sc->sc_tx_desc.plcp_service = 4;

	len += IEEE80211_CRC_LEN;

	phytype = ieee80211_rate2phytype(sc->sc_rates, rate);

	if (phytype == IEEE80211_T_OFDM) {
		sc->sc_tx_desc.flags |= htole32(RT2573_TX_OFDM);

		plcp_length = (len & 0xfff);
		sc->sc_tx_desc.plcp_length_hi = plcp_length >> 6;
		sc->sc_tx_desc.plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = ((16 * len) + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if ((remainder != 0) && (remainder < 7)) {
				sc->sc_tx_desc.plcp_service |=
				    RT2573_PLCP_LENGEXT;
			}
		}
		sc->sc_tx_desc.plcp_length_hi = plcp_length >> 8;
		sc->sc_tx_desc.plcp_length_lo = plcp_length & 0xff;

		if ((rate != 2) && (ic->ic_flags & IEEE80211_F_SHPREAMBLE)) {
			sc->sc_tx_desc.plcp_signal |= 0x08;
		}
	}

	if (sizeof(sc->sc_tx_desc) > MHLEN) {
		DPRINTF("No room for header structure!\n");
		rum_tx_freem(m);
		return;
	}
	mm = m_gethdr(M_NOWAIT, MT_DATA);
	if (mm == NULL) {
		DPRINTF("Could not allocate header mbuf!\n");
		rum_tx_freem(m);
		return;
	}
	bcopy(&sc->sc_tx_desc, mm->m_data, sizeof(sc->sc_tx_desc));
	mm->m_len = sizeof(sc->sc_tx_desc);
	mm->m_next = m;
	mm->m_pkthdr.len = mm->m_len + m->m_pkthdr.len;
	mm->m_pkthdr.rcvif = NULL;

	if (is_beacon) {

		if (mm->m_pkthdr.len > sizeof(sc->sc_beacon_buf)) {
			DPRINTFN(0, "Truncating beacon"
			    ", %u bytes!\n", mm->m_pkthdr.len);
			mm->m_pkthdr.len = sizeof(sc->sc_beacon_buf);
		}
		m_copydata(mm, 0, mm->m_pkthdr.len, sc->sc_beacon_buf);

		/* copy the first 24 bytes of Tx descriptor into NIC memory */
		rum_cfg_write_multi(sc, RT2573_HW_BEACON_BASE0,
		    sc->sc_beacon_buf, mm->m_pkthdr.len);
		rum_tx_freem(mm);
		return;
	}
	/* start write transfer, if not started */
	_IF_ENQUEUE(&sc->sc_tx_queue, mm);

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
rum_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct rum_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint16_t temp_len;
	uint8_t align;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");

		ifp->if_opackets++;

	case USB_ST_SETUP:
		if (sc->sc_flags & RUM_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			break;
		}
		if (sc->sc_flags & RUM_FLAG_WAIT_COMMAND) {
			/*
			 * don't send anything while a command is pending !
			 */
			break;
		}
		rum_fill_write_queue(sc);

		_IF_DEQUEUE(&sc->sc_tx_queue, m);

		if (m) {

			if (m->m_pkthdr.len > (MCLBYTES + RT2573_TX_DESC_SIZE)) {
				DPRINTFN(0, "data overflow, %u bytes\n",
				    m->m_pkthdr.len);
				m->m_pkthdr.len = (MCLBYTES + RT2573_TX_DESC_SIZE);
			}
			usb2_m_copy_in(xfer->frbuffers, 0,
			    m, 0, m->m_pkthdr.len);

			/* compute transfer length */
			temp_len = m->m_pkthdr.len;

			/* make transfer length 32-bit aligned */
			align = (-(temp_len)) & 3;

			/* check if we need to add four extra bytes */
			if (((temp_len + align) % 64) == 0) {
				align += 4;
			}
			/* check if we need to align length */
			if (align != 0) {
				/* zero the extra bytes */
				usb2_bzero(xfer->frbuffers, temp_len, align);
				temp_len += align;
			}
			DPRINTFN(11, "sending frame len=%u ferlen=%u\n",
			    m->m_pkthdr.len, temp_len);

			xfer->frlengths[0] = temp_len;
			usb2_start_hardware(xfer);

			/* free mbuf and node */
			rum_tx_freem(m);

		}
		break;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= RUM_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		ifp->if_oerrors++;
		break;
	}
	return;
}

static void
rum_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct rum_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~RUM_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
rum_watchdog(void *arg)
{
	struct rum_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->sc_amrr_timer) {
		usb2_config_td_queue_command
		    (&sc->sc_config_td, NULL,
		    &rum_cfg_amrr_timeout, 0, 0);
	}
	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &rum_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
rum_init_cb(void *arg)
{
	struct rum_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &rum_cfg_pre_init,
	    &rum_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static int
rum_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	int error;

	switch (cmd) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &rum_cfg_pre_init,
				    &rum_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &rum_cfg_pre_stop,
				    &rum_cfg_stop, 0, 0);
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
rum_start_cb(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	/* start write transfer, if not started */
	usb2_transfer_start(sc->sc_xfer[0]);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
rum_cfg_newstate(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct rum_vap *uvp = RUM_VAP(vap);
	enum ieee80211_state ostate;
	enum ieee80211_state nstate;
	int arg;

	ostate = vap->iv_state;
	nstate = sc->sc_ns_state;
	arg = sc->sc_ns_arg;

	if (ostate == IEEE80211_S_INIT) {
		/* We are leaving INIT. TSF sync should be off. */
		rum_cfg_disable_tsf_sync(sc);
	}
	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_RUN:
		rum_cfg_set_run(sc, cc);
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
rum_newstate_cb(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct rum_vap *uvp = RUM_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct rum_softc *sc = ic->ic_ifp->if_softc;

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
	    (&sc->sc_config_td, &rum_config_copy,
	    &rum_cfg_newstate, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return (EINPROGRESS);
}

static void
rum_std_command(struct ieee80211com *ic, usb2_config_td_command_t *func)
{
	struct rum_softc *sc = ic->ic_ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &rum_config_copy, func, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
rum_scan_start_cb(struct ieee80211com *ic)
{
	rum_std_command(ic, &rum_cfg_scan_start);
	return;
}

static void
rum_scan_end_cb(struct ieee80211com *ic)
{
	rum_std_command(ic, &rum_cfg_scan_end);
	return;
}

static void
rum_set_channel_cb(struct ieee80211com *ic)
{
	rum_std_command(ic, &rum_cfg_set_chan);
	return;
}

static void
rum_cfg_scan_start(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* abort TSF synchronization */
	rum_cfg_disable_tsf_sync(sc);
	rum_cfg_set_bssid(sc, cc->if_broadcastaddr);
	return;
}

static void
rum_cfg_scan_end(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* enable TSF synchronization */
	rum_cfg_enable_tsf_sync(sc, cc, 0);
	rum_cfg_set_bssid(sc, cc->iv_bss.ni_bssid);
	return;
}

/*
 * Reprogram MAC/BBP to switch to a new band. Values taken from the reference
 * driver.
 */
static void
rum_cfg_select_band(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;
	uint8_t bbp17, bbp35, bbp96, bbp97, bbp98, bbp104;

	/* update all BBP registers that depend on the band */
	bbp17 = 0x20;
	bbp96 = 0x48;
	bbp104 = 0x2c;
	bbp35 = 0x50;
	bbp97 = 0x48;
	bbp98 = 0x48;

	if (cc->ic_curchan.chan_is_5ghz) {
		bbp17 += 0x08;
		bbp96 += 0x10;
		bbp104 += 0x0c;
		bbp35 += 0x10;
		bbp97 += 0x10;
		bbp98 += 0x10;
	}
	if ((cc->ic_curchan.chan_is_2ghz && sc->sc_ext_2ghz_lna) ||
	    (cc->ic_curchan.chan_is_5ghz && sc->sc_ext_5ghz_lna)) {
		bbp17 += 0x10;
		bbp96 += 0x10;
		bbp104 += 0x10;
	}
	sc->sc_bbp17 = bbp17;
	rum_cfg_bbp_write(sc, 17, bbp17);
	rum_cfg_bbp_write(sc, 96, bbp96);
	rum_cfg_bbp_write(sc, 104, bbp104);

	if ((cc->ic_curchan.chan_is_2ghz && sc->sc_ext_2ghz_lna) ||
	    (cc->ic_curchan.chan_is_5ghz && sc->sc_ext_5ghz_lna)) {
		rum_cfg_bbp_write(sc, 75, 0x80);
		rum_cfg_bbp_write(sc, 86, 0x80);
		rum_cfg_bbp_write(sc, 88, 0x80);
	}
	rum_cfg_bbp_write(sc, 35, bbp35);
	rum_cfg_bbp_write(sc, 97, bbp97);
	rum_cfg_bbp_write(sc, 98, bbp98);

	tmp = rum_cfg_read(sc, RT2573_PHY_CSR0);
	tmp &= ~(RT2573_PA_PE_2GHZ | RT2573_PA_PE_5GHZ);
	if (cc->ic_curchan.chan_is_2ghz)
		tmp |= RT2573_PA_PE_2GHZ;
	else
		tmp |= RT2573_PA_PE_5GHZ;
	rum_cfg_write(sc, RT2573_PHY_CSR0, tmp);

	/* 802.11a uses a 16 microseconds short interframe space */
	sc->sc_sifs = cc->ic_curchan.chan_is_5ghz ? 16 : 10;

	return;
}

static void
rum_cfg_set_chan(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	enum {
	N_RF5225 = (sizeof(rum_rf5225) / sizeof(rum_rf5225[0]))};
	const struct rfprog *rfprog;
	uint32_t chan;
	uint16_t i;
	uint8_t bbp3;
	uint8_t bbp94 = RT2573_BBPR94_DEFAULT;
	int8_t power;

	chan = cc->ic_curchan.chan_to_ieee;

	if ((chan == 0) ||
	    (chan == IEEE80211_CHAN_ANY)) {
		/* nothing to do */
		return;
	}
	if (chan == sc->sc_last_chan) {
		return;
	}
	sc->sc_last_chan = chan;

	/* select the appropriate RF settings based on what EEPROM says */
	rfprog = ((sc->sc_rf_rev == RT2573_RF_5225) ||
	    (sc->sc_rf_rev == RT2573_RF_2527)) ? rum_rf5225 : rum_rf5226;

	/* find the settings for this channel */
	for (i = 0;; i++) {
		if (i == (N_RF5225 - 1))
			break;
		if (rfprog[i].chan == chan)
			break;
	}

	DPRINTF("chan=%d, i=%d\n", chan, i);

	power = sc->sc_txpow[i];
	if (power < 0) {
		bbp94 += power;
		power = 0;
	} else if (power > 31) {
		bbp94 += power - 31;
		power = 31;
	}
	/*
	 * If we are switching from the 2GHz band to the 5GHz band or
	 * vice-versa, BBP registers need to be reprogrammed.
	 */
	rum_cfg_select_band(sc, cc, 0);
	rum_cfg_select_antenna(sc, cc, 0);

	rum_cfg_rf_write(sc, RT2573_RF1, rfprog[i].r1);
	rum_cfg_rf_write(sc, RT2573_RF2, rfprog[i].r2);
	rum_cfg_rf_write(sc, RT2573_RF3, rfprog[i].r3 | (power << 7));
	rum_cfg_rf_write(sc, RT2573_RF4, rfprog[i].r4 | (sc->sc_rffreq << 10));

	rum_cfg_rf_write(sc, RT2573_RF1, rfprog[i].r1);
	rum_cfg_rf_write(sc, RT2573_RF2, rfprog[i].r2);
	rum_cfg_rf_write(sc, RT2573_RF3, rfprog[i].r3 | (power << 7) | 1);
	rum_cfg_rf_write(sc, RT2573_RF4, rfprog[i].r4 | (sc->sc_rffreq << 10));

	rum_cfg_rf_write(sc, RT2573_RF1, rfprog[i].r1);
	rum_cfg_rf_write(sc, RT2573_RF2, rfprog[i].r2);
	rum_cfg_rf_write(sc, RT2573_RF3, rfprog[i].r3 | (power << 7));
	rum_cfg_rf_write(sc, RT2573_RF4, rfprog[i].r4 | (sc->sc_rffreq << 10));

	if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
		return;
	}
	/* enable smart mode for MIMO-capable RFs */
	bbp3 = rum_cfg_bbp_read(sc, 3);

	if ((sc->sc_rf_rev == RT2573_RF_5225) ||
	    (sc->sc_rf_rev == RT2573_RF_2527))
		bbp3 &= ~RT2573_SMART_MODE;
	else
		bbp3 |= RT2573_SMART_MODE;

	rum_cfg_bbp_write(sc, 3, bbp3);

	rum_cfg_bbp_write(sc, 94, bbp94);

	/* update basic rate set */

	if (cc->ic_curchan.chan_is_b) {
		/* 11b basic rates: 1, 2Mbps */
		rum_cfg_write(sc, RT2573_TXRX_CSR5, 0x3);
	} else if (cc->ic_curchan.chan_is_a) {
		/* 11a basic rates: 6, 12, 24Mbps */
		rum_cfg_write(sc, RT2573_TXRX_CSR5, 0x150);
	} else {
		/* 11b/g basic rates: 1, 2, 5.5, 11Mbps */
		rum_cfg_write(sc, RT2573_TXRX_CSR5, 0xf);
	}

	if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
		return;
	}
	return;
}

static void
rum_cfg_set_run(struct rum_softc *sc,
    struct usb2_config_td_cc *cc)
{

	if (cc->ic_opmode != IEEE80211_M_MONITOR) {
		rum_cfg_update_slot(sc, cc, 0);
		rum_cfg_enable_mrr(sc, cc, 0);
		rum_cfg_set_txpreamble(sc, cc, 0);

		/* update basic rate set */

		if (cc->ic_bsschan.chan_is_5ghz) {
			/* 11a basic rates: 6, 12, 24Mbps */
			rum_cfg_write(sc, RT2573_TXRX_CSR5, 0x150);
		} else if (cc->ic_bsschan.chan_is_g) {
			/* 11b/g basic rates: 1, 2, 5.5, 11Mbps */
			rum_cfg_write(sc, RT2573_TXRX_CSR5, 0xf);
		} else {
			/* 11b basic rates: 1, 2Mbps */
			rum_cfg_write(sc, RT2573_TXRX_CSR5, 0x3);
		}
		rum_cfg_set_bssid(sc, cc->iv_bss.ni_bssid);
	}
	if ((cc->ic_opmode == IEEE80211_M_HOSTAP) ||
	    (cc->ic_opmode == IEEE80211_M_IBSS)) {
		rum_cfg_prepare_beacon(sc, cc, 0);
	}
	if (cc->ic_opmode != IEEE80211_M_MONITOR) {
		rum_cfg_enable_tsf_sync(sc, cc, 0);
	}
	if (cc->iv_bss.fixed_rate_none) {
		/* enable automatic rate adaptation */
		rum_cfg_amrr_start(sc);
	}
	return;
}

static void
rum_cfg_enable_tsf_sync(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;

	if (cc->ic_opmode != IEEE80211_M_STA) {
		/*
		 * Change default 16ms TBTT adjustment to 8ms.
		 * Must be done before enabling beacon generation.
		 */
		rum_cfg_write(sc, RT2573_TXRX_CSR10, (1 << 12) | 8);
	}
	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR9) & 0xff000000;

	/* set beacon interval (in 1/16ms unit) */
	tmp |= cc->iv_bss.ni_intval * 16;

	tmp |= RT2573_TSF_TICKING | RT2573_ENABLE_TBTT;
	if (cc->ic_opmode == IEEE80211_M_STA)
		tmp |= RT2573_TSF_MODE(1);
	else
		tmp |= RT2573_TSF_MODE(2) | RT2573_GENERATE_BEACON;

	rum_cfg_write(sc, RT2573_TXRX_CSR9, tmp);

	return;
}

static void
rum_cfg_disable_tsf_sync(struct rum_softc *sc)
{
	uint32_t tmp;

	/* abort TSF synchronization */
	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR9);
	rum_cfg_write(sc, RT2573_TXRX_CSR9, tmp & ~0x00ffffff);
	return;
}

/*
 * Enable multi-rate retries for frames sent at OFDM rates.
 * In 802.11b/g mode, allow fallback to CCK rates.
 */
static void
rum_cfg_enable_mrr(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;

	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR4);

	if (cc->ic_curchan.chan_is_5ghz)
		tmp &= ~RT2573_MRR_CCK_FALLBACK;
	else
		tmp |= RT2573_MRR_CCK_FALLBACK;

	tmp |= RT2573_MRR_ENABLED;

	rum_cfg_write(sc, RT2573_TXRX_CSR4, tmp);

	return;
}

static void
rum_cfg_update_slot(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;
	uint8_t slottime;

	slottime = (cc->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	tmp = rum_cfg_read(sc, RT2573_MAC_CSR9);
	tmp = (tmp & ~0xff) | slottime;
	rum_cfg_write(sc, RT2573_MAC_CSR9, tmp);

	DPRINTF("setting slot time to %u us\n", slottime);

	return;
}

static void
rum_cfg_set_txpreamble(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;

	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR4);

	if (cc->ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RT2573_SHORT_PREAMBLE;
	else
		tmp &= ~RT2573_SHORT_PREAMBLE;

	rum_cfg_write(sc, RT2573_TXRX_CSR4, tmp);

	return;
}

static void
rum_cfg_set_bssid(struct rum_softc *sc, uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | (bssid[1] << 8) | (bssid[2] << 16) | (bssid[3] << 24);
	rum_cfg_write(sc, RT2573_MAC_CSR4, tmp);

	tmp = (bssid[4]) | (bssid[5] << 8) | (RT2573_ONE_BSSID << 16);
	rum_cfg_write(sc, RT2573_MAC_CSR5, tmp);

	return;
}

static void
rum_cfg_set_macaddr(struct rum_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
	rum_cfg_write(sc, RT2573_MAC_CSR2, tmp);

	tmp = addr[4] | (addr[5] << 8) | (0xff << 16);
	rum_cfg_write(sc, RT2573_MAC_CSR3, tmp);

	return;
}

static void
rum_cfg_update_promisc(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;

	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR0);

	if (cc->if_flags & IFF_PROMISC)
		tmp &= ~RT2573_DROP_NOT_TO_ME;
	else
		tmp |= RT2573_DROP_NOT_TO_ME;

	rum_cfg_write(sc, RT2573_TXRX_CSR0, tmp);

	DPRINTF("%s promiscuous mode\n",
	    (cc->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving");
	return;
}

static void
rum_cfg_select_antenna(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;
	uint8_t bbp3;
	uint8_t bbp4;
	uint8_t bbp77;
	uint8_t rx_ant;
	uint8_t is_5ghz;

	bbp3 = rum_cfg_bbp_read(sc, 3);
	bbp4 = rum_cfg_bbp_read(sc, 4);
	bbp77 = rum_cfg_bbp_read(sc, 77);

	bbp3 &= ~0x01;
	bbp4 &= ~0x23;

	rx_ant = sc->sc_rx_ant;
	is_5ghz = cc->ic_curchan.chan_is_5ghz;

	switch (sc->sc_rf_rev) {
	case RT2573_RF_5226:
	case RT2573_RF_5225:
		if (rx_ant == 0) {
			/* Diversity */
			bbp4 |= 0x02;
			if (is_5ghz == 0)
				bbp4 |= 0x20;
		} else if (rx_ant == 1) {
			/* RX: Antenna A */
			bbp4 |= 0x01;
			if (is_5ghz)
				bbp77 &= ~0x03;
			else
				bbp77 |= 0x03;
		} else if (rx_ant == 2) {
			/* RX: Antenna B */
			bbp4 |= 0x01;
			if (is_5ghz)
				bbp77 |= 0x03;
			else
				bbp77 &= ~0x03;
		}
		break;

	case RT2573_RF_2528:
	case RT2573_RF_2527:
		if (rx_ant == 0) {
			/* Diversity */
			bbp4 |= 0x22;
		} else if (rx_ant == 1) {
			/* RX: Antenna A */
			bbp4 |= 0x21;
			bbp77 |= 0x03;
		} else if (rx_ant == 2) {
			/* RX: Antenna B */
			bbp4 |= 0x21;
			bbp77 &= ~0x03;
		}
		break;
	default:
		break;
	}
	bbp4 &= ~(sc->sc_ftype << 5);

	/* make sure Rx is disabled before switching antenna */
	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR0);
	rum_cfg_write(sc, RT2573_TXRX_CSR0, tmp | RT2573_DISABLE_RX);

	rum_cfg_bbp_write(sc, 3, bbp3);
	rum_cfg_bbp_write(sc, 4, bbp4);
	rum_cfg_bbp_write(sc, 77, bbp77);

	rum_cfg_write(sc, RT2573_TXRX_CSR0, tmp);

	return;
}

static void
rum_cfg_read_eeprom(struct rum_softc *sc)
{
	uint16_t val;

	/* read MAC address */
	rum_cfg_eeprom_read(sc, RT2573_EEPROM_ADDRESS, sc->sc_myaddr, 6);

	val = rum_cfg_eeprom_read_2(sc, RT2573_EEPROM_ANTENNA);
	sc->sc_rf_rev = (val >> 11) & 0x1f;
	sc->sc_hw_radio = (val >> 10) & 0x1;
	sc->sc_ftype = (val >> 6) & 0x1;
	sc->sc_rx_ant = (val >> 4) & 0x3;
	sc->sc_tx_ant = (val >> 2) & 0x3;
	sc->sc_nb_ant = (val & 0x3);

	DPRINTF("RF revision=%d\n", sc->sc_rf_rev);

	val = rum_cfg_eeprom_read_2(sc, RT2573_EEPROM_CONFIG2);
	sc->sc_ext_5ghz_lna = (val >> 6) & 0x1;
	sc->sc_ext_2ghz_lna = (val >> 4) & 0x1;

	DPRINTF("External 2GHz LNA=%d, External 5GHz LNA=%d\n",
	    sc->sc_ext_2ghz_lna, sc->sc_ext_5ghz_lna);

	val = rum_cfg_eeprom_read_2(sc, RT2573_EEPROM_RSSI_2GHZ_OFFSET);
	if ((val & 0xff) != 0xff)
		sc->sc_rssi_2ghz_corr = (int8_t)(val & 0xff);	/* signed */
	else
		sc->sc_rssi_2ghz_corr = 0;

	/* range check */
	if ((sc->sc_rssi_2ghz_corr < -10) ||
	    (sc->sc_rssi_2ghz_corr > 10)) {
		sc->sc_rssi_2ghz_corr = 0;
	}
	val = rum_cfg_eeprom_read_2(sc, RT2573_EEPROM_RSSI_5GHZ_OFFSET);
	if ((val & 0xff) != 0xff)
		sc->sc_rssi_5ghz_corr = (int8_t)(val & 0xff);	/* signed */
	else
		sc->sc_rssi_5ghz_corr = 0;

	/* range check */
	if ((sc->sc_rssi_5ghz_corr < -10) ||
	    (sc->sc_rssi_5ghz_corr > 10)) {
		sc->sc_rssi_5ghz_corr = 0;
	}
	if (sc->sc_ext_2ghz_lna) {
		sc->sc_rssi_2ghz_corr -= 14;
	}
	if (sc->sc_ext_5ghz_lna) {
		sc->sc_rssi_5ghz_corr -= 14;
	}
	DPRINTF("RSSI 2GHz corr=%d, RSSI 5GHz corr=%d\n",
	    sc->sc_rssi_2ghz_corr, sc->sc_rssi_5ghz_corr);

	val = rum_cfg_eeprom_read_2(sc, RT2573_EEPROM_FREQ_OFFSET);
	if ((val & 0xff) != 0xff)
		sc->sc_rffreq = (val & 0xff);
	else
		sc->sc_rffreq = 0;

	DPRINTF("RF freq=%d\n", sc->sc_rffreq);

	/* read Tx power for all a/b/g channels */
	rum_cfg_eeprom_read(sc, RT2573_EEPROM_TXPOWER, sc->sc_txpow, 14);

	/* XXX default Tx power for 802.11a channels */
	memset(sc->sc_txpow + 14, 24, sizeof(sc->sc_txpow) - 14);

	/* read default values for BBP registers */
	rum_cfg_eeprom_read(sc, RT2573_EEPROM_BBP_BASE, sc->sc_bbp_prom, 2 * 16);

	return;
}

static uint8_t
rum_cfg_bbp_init(struct rum_softc *sc)
{
	enum {
		N_DEF_BBP = (sizeof(rum_def_bbp) / sizeof(rum_def_bbp[0])),
	};
	uint16_t i;
	uint8_t to;
	uint8_t tmp;

	/* wait for BBP to become ready */
	for (to = 0;; to++) {
		if (to < 100) {
			tmp = rum_cfg_bbp_read(sc, 0);
			if ((tmp != 0x00) &&
			    (tmp != 0xff)) {
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
		rum_cfg_bbp_write(sc, rum_def_bbp[i].reg, rum_def_bbp[i].val);
	}

	/* write vendor-specific BBP values (from EEPROM) */
	for (i = 0; i < 16; i++) {
		if ((sc->sc_bbp_prom[i].reg == 0) ||
		    (sc->sc_bbp_prom[i].reg == 0xff)) {
			continue;
		}
		rum_cfg_bbp_write(sc, sc->sc_bbp_prom[i].reg, sc->sc_bbp_prom[i].val);
	}
	return (0);
}

static void
rum_cfg_pre_init(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	/* immediate configuration */

	rum_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= RUM_FLAG_HL_READY;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	return;
}

static void
rum_cfg_init(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	enum {
		N_DEF_MAC = (sizeof(rum_def_mac) / sizeof(rum_def_mac[0])),
	};

	uint32_t tmp;
	uint16_t i;
	uint8_t to;

	/* delayed configuration */

	rum_cfg_stop(sc, cc, 0);

	/* initialize MAC registers to default values */
	for (i = 0; i < N_DEF_MAC; i++) {
		rum_cfg_write(sc, rum_def_mac[i].reg, rum_def_mac[i].val);
	}

	/* set host ready */
	rum_cfg_write(sc, RT2573_MAC_CSR1, 3);
	rum_cfg_write(sc, RT2573_MAC_CSR1, 0);

	/* wait for BBP/RF to wakeup */
	for (to = 0;; to++) {
		if (to < 100) {
			if (rum_cfg_read(sc, RT2573_MAC_CSR12) & 8) {
				break;
			}
			rum_cfg_write(sc, RT2573_MAC_CSR12, 4);	/* force wakeup */

			if (usb2_config_td_sleep(&sc->sc_config_td, hz / 100)) {
				goto fail;
			}
		} else {
			DPRINTF("timeout waiting for "
			    "BBP/RF to wakeup\n");
			goto fail;
		}
	}

	if (rum_cfg_bbp_init(sc)) {
		goto fail;
	}
	/* select default channel */

	sc->sc_last_chan = 0;

	rum_cfg_set_chan(sc, cc, 0);

	/* clear STA registers */
	rum_cfg_read_multi(sc, RT2573_STA_CSR0, sc->sc_sta, sizeof(sc->sc_sta));
	/* set MAC address */
	rum_cfg_set_macaddr(sc, cc->ic_myaddr);

	/* initialize ASIC */
	rum_cfg_write(sc, RT2573_MAC_CSR1, 4);

	/*
	 * make sure that the first transaction
	 * clears the stall:
	 */
	sc->sc_flags |= (RUM_FLAG_READ_STALL |
	    RUM_FLAG_WRITE_STALL |
	    RUM_FLAG_LL_READY);

	if ((sc->sc_flags & RUM_FLAG_LL_READY) &&
	    (sc->sc_flags & RUM_FLAG_HL_READY)) {
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
	/* update Rx filter */
	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR0) & 0xffff;

	tmp |= RT2573_DROP_PHY_ERROR | RT2573_DROP_CRC_ERROR;

	if (cc->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2573_DROP_CTL | RT2573_DROP_VER_ERROR |
		    RT2573_DROP_ACKCTS;
		if (cc->ic_opmode != IEEE80211_M_HOSTAP) {
			tmp |= RT2573_DROP_TODS;
		}
		if (!(cc->if_flags & IFF_PROMISC)) {
			tmp |= RT2573_DROP_NOT_TO_ME;
		}
	}
	rum_cfg_write(sc, RT2573_TXRX_CSR0, tmp);

	return;

fail:
	rum_cfg_pre_stop(sc, NULL, 0);

	if (cc) {
		rum_cfg_stop(sc, cc, 0);
	}
	return;
}

static void
rum_cfg_pre_stop(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		rum_config_copy(sc, cc, refcount);
	}
	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(RUM_FLAG_HL_READY |
	    RUM_FLAG_LL_READY);

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[0]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[3]);

	/* clean up transmission */
	rum_tx_clean_queue(sc);
	return;
}

static void
rum_cfg_stop(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint32_t tmp;

	/* disable Rx */
	tmp = rum_cfg_read(sc, RT2573_TXRX_CSR0);
	rum_cfg_write(sc, RT2573_TXRX_CSR0, tmp | RT2573_DISABLE_RX);

	/* reset ASIC */
	rum_cfg_write(sc, RT2573_MAC_CSR1, 3);

	/* wait a little */
	usb2_config_td_sleep(&sc->sc_config_td, hz / 10);

	rum_cfg_write(sc, RT2573_MAC_CSR1, 0);

	/* wait a little */
	usb2_config_td_sleep(&sc->sc_config_td, hz / 10);

	return;
}

static void
rum_cfg_amrr_start(struct rum_softc *sc)
{
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;

	vap = rum_get_vap(sc);

	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	/* init AMRR */

	ieee80211_amrr_node_init(&RUM_VAP(vap)->amrr, &RUM_NODE(ni)->amn, ni);

	/* enable AMRR timer */

	sc->sc_amrr_timer = 1;
	return;
}

static void
rum_cfg_amrr_timeout(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;
	uint32_t ok;
	uint32_t fail;

	/* clear statistic registers (STA_CSR0 to STA_CSR5) */
	rum_cfg_read_multi(sc, RT2573_STA_CSR0, sc->sc_sta, sizeof(sc->sc_sta));

	vap = rum_get_vap(sc);
	if (vap == NULL) {
		return;
	}
	ni = vap->iv_bss;
	if (ni == NULL) {
		return;
	}
	if ((sc->sc_flags & RUM_FLAG_LL_READY) &&
	    (sc->sc_flags & RUM_FLAG_HL_READY)) {

		ok = (le32toh(sc->sc_sta[4]) >> 16) +	/* TX ok w/o retry */
		    (le32toh(sc->sc_sta[5]) & 0xffff);	/* TX ok w/ retry */
		fail = (le32toh(sc->sc_sta[5]) >> 16);	/* TX retry-fail count */

		if (sc->sc_amrr_timer) {
			ieee80211_amrr_tx_update(&RUM_NODE(vap->iv_bss)->amn,
			    ok + fail, ok, (le32toh(sc->sc_sta[5]) & 0xffff) + fail);

			if (ieee80211_amrr_choose(ni, &RUM_NODE(ni)->amn)) {
				/* ignore */
			}
		}
		ifp->if_oerrors += fail;/* count TX retry-fail as Tx errors */
	}
	return;
}

static void
rum_cfg_load_microcode(struct rum_softc *sc, const uint8_t *ucode, uint16_t size)
{
	struct usb2_device_request req;
	uint16_t reg = RT2573_MCU_CODE_BASE;

	/* copy firmware image into NIC */
	while (size >= 4) {
		rum_cfg_write(sc, reg, UGETDW(ucode));
		reg += 4;
		ucode += 4;
		size -= 4;
	}

	if (size != 0) {
		DPRINTF("possibly invalid firmware\n");
	}
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2573_MCU_CNTL;
	USETW(req.wValue, RT2573_MCU_RUN);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	rum_cfg_do_request(sc, &req, NULL);

	return;
}

static void
rum_cfg_prepare_beacon(struct rum_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	const struct ieee80211_txparam *tp;
	struct mbuf *m;

	vap = rum_get_vap(sc);
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

	m = ieee80211_beacon_alloc(ni, &RUM_VAP(vap)->bo);
	if (m == NULL) {
		DPRINTFN(0, "could not allocate beacon\n");
		return;
	}
	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_bsschan)];

	m->m_pkthdr.rcvif = (void *)ieee80211_ref_node(ni);
	rum_setup_desc_and_tx(sc, m, RT2573_TX_TIMESTAMP, RT2573_TX_HWSEQ | RT2573_TX_BEACON, tp->mgmtrate);
	return;
}

static uint8_t
rum_get_rssi(struct rum_softc *sc, uint8_t raw)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int16_t rssi;
	uint8_t lna;
	uint8_t agc;

	lna = (raw >> 5) & 0x3;
	agc = raw & 0x1f;

	if (lna == 0) {
		/*
                 * No RSSI mapping
                 *
                 * NB: Since RSSI is relative to noise floor, -1 is
                 *     adequate for caller to know error happened.
                 */
		return (0);
	}
	rssi = (2 * agc) - RT2573_NOISE_FLOOR;

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {

		rssi += sc->sc_rssi_2ghz_corr;

		if (lna == 1)
			rssi -= 64;
		else if (lna == 2)
			rssi -= 74;
		else if (lna == 3)
			rssi -= 90;
	} else {

		rssi += sc->sc_rssi_5ghz_corr;

		if ((!sc->sc_ext_5ghz_lna) && (lna != 1))
			rssi += 4;

		if (lna == 1)
			rssi -= 64;
		else if (lna == 2)
			rssi -= 86;
		else if (lna == 3)
			rssi -= 100;
	}

	/* range check */

	if (rssi < 0)
		rssi = 0;
	else if (rssi > 255)
		rssi = 255;

	return (rssi);
}

static struct ieee80211vap *
rum_vap_create(struct ieee80211com *ic,
    const char name[IFNAMSIZ], int unit, int opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct rum_vap *rvp;
	struct ieee80211vap *vap;
	struct rum_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF("\n");

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
	rvp = (struct rum_vap *)malloc(sizeof(struct rum_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (rvp == NULL)
		return NULL;
	vap = &rvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	rvp->newstate = vap->iv_newstate;
	vap->iv_newstate = &rum_newstate_cb;

	ieee80211_amrr_init(&rvp->amrr, vap,
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
rum_vap_delete(struct ieee80211vap *vap)
{
	struct rum_vap *rvp = RUM_VAP(vap);
	struct rum_softc *sc = vap->iv_ic->ic_ifp->if_softc;

	DPRINTF("\n");

	/* Need to sync with config thread: */
	mtx_lock(&sc->sc_mtx);
	if (usb2_config_td_sync(&sc->sc_config_td)) {
		/* ignore */
	}
	mtx_unlock(&sc->sc_mtx);

	ieee80211_amrr_cleanup(&rvp->amrr);
	ieee80211_vap_detach(vap);
	free(rvp, M_80211_VAP);
	return;
}

/* ARGUSED */
static struct ieee80211_node *
rum_node_alloc(struct ieee80211vap *vap __unused,
    const uint8_t mac[IEEE80211_ADDR_LEN] __unused)
{
	struct rum_node *rn;

	rn = malloc(sizeof(struct rum_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	return ((rn != NULL) ? &rn->ni : NULL);
}

static void
rum_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ieee80211_amrr_node_init(&RUM_VAP(vap)->amrr, &RUM_NODE(ni)->amn, ni);
	return;
}

static void
rum_fill_write_queue(struct rum_softc *sc)
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
		rum_tx_data(sc, m, ni);
	}
	return;
}

static void
rum_tx_clean_queue(struct rum_softc *sc)
{
	struct mbuf *m;

	for (;;) {
		_IF_DEQUEUE(&sc->sc_tx_queue, m);

		if (!m) {
			break;
		}
		rum_tx_freem(m);
	}
	return;
}

static void
rum_tx_freem(struct mbuf *m)
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
rum_tx_mgt(struct rum_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
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
		flags |= RT2573_TX_NEED_ACK;

		dur = ieee80211_ack_duration(sc->sc_rates, tp->mgmtrate,
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		USETW(wh->i_dur, dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			flags |= RT2573_TX_TIMESTAMP;
	}
	m->m_pkthdr.rcvif = (void *)ni;
	rum_setup_desc_and_tx(sc, m, flags, 0, tp->mgmtrate);
	return;
}

static struct ieee80211vap *
rum_get_vap(struct rum_softc *sc)
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
rum_tx_data(struct rum_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni)
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
			rum_tx_prot(sc, m, ni, prot, rate);
			flags |= RT2573_TX_LONG_RETRY | RT2573_TX_IFS_SIFS;
		}
		flags |= RT2573_TX_NEED_ACK;
		flags |= RT2573_TX_MORE_FRAG;

		dur = ieee80211_ack_duration(sc->sc_rates, rate,
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		USETW(wh->i_dur, dur);
	}
	m->m_pkthdr.rcvif = (void *)ni;
	rum_setup_desc_and_tx(sc, m, flags, 0, rate);
	return;
}

static void
rum_tx_prot(struct rum_softc *sc,
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

	DPRINTFN(11, "Sending protection frame.\n");

	wh = mtod(m, const struct ieee80211_frame *);
	pktlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;

	protrate = ieee80211_ctl_rate(sc->sc_rates, rate);
	ackrate = ieee80211_ack_rate(sc->sc_rates, rate);

	isshort = (ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0;
	dur = ieee80211_compute_duration(sc->sc_rates, pktlen, rate, isshort);
	+ieee80211_ack_duration(sc->sc_rates, rate, isshort);
	flags = RT2573_TX_MORE_FRAG;
	if (prot == IEEE80211_PROT_RTSCTS) {
		/* NB: CTS is the same size as an ACK */
		dur += ieee80211_ack_duration(sc->sc_rates, rate, isshort);
		flags |= RT2573_TX_NEED_ACK;
		mprot = ieee80211_alloc_rts(ic, wh->i_addr1, wh->i_addr2, dur);
	} else {
		mprot = ieee80211_alloc_cts(ic, ni->ni_vap->iv_myaddr, dur);
	}
	if (mprot == NULL) {
		return;
	}
	mprot->m_pkthdr.rcvif = (void *)ieee80211_ref_node(ni);
	rum_setup_desc_and_tx(sc, mprot, flags, 0, protrate);
	return;
}

static void
rum_tx_raw(struct rum_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
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
		flags |= RT2573_TX_NEED_ACK;
	if (params->ibp_flags & (IEEE80211_BPF_RTS | IEEE80211_BPF_CTS)) {
		rum_tx_prot(sc, m, ni,
		    params->ibp_flags & IEEE80211_BPF_RTS ?
		    IEEE80211_PROT_RTSCTS : IEEE80211_PROT_CTSONLY,
		    rate);
		flags |= RT2573_TX_LONG_RETRY | RT2573_TX_IFS_SIFS;
	}
	m->m_pkthdr.rcvif = (void *)ni;
	rum_setup_desc_and_tx(sc, m, flags, 0, rate);
	return;
}

static int
rum_raw_xmit_cb(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rum_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		rum_tx_mgt(sc, m, ni);
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		rum_tx_raw(sc, m, ni, params);
	}
	mtx_unlock(&sc->sc_mtx);
	return (0);
}

static void
rum_update_mcast_cb(struct ifnet *ifp)
{
	/* not supported */
	return;
}

static void
rum_update_promisc_cb(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &rum_config_copy,
	    &rum_cfg_update_promisc, 0, 0);
	mtx_unlock(&sc->sc_mtx);
	return;
}
