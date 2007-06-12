/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005-2007 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
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
 * Ralink Technology RT2501USB/RT2601USB chipset driver
 * http://www.ralinktech.com.tw/
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
#include <sys/rman.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/if_rumreg.h>
#include <dev/usb/if_rumvar.h>
#include <dev/usb/rt2573_ucode.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (rumdebug > 0) logprintf x; } while (0)
#define DPRINTFN(n, x)	do { if (rumdebug >= (n)) logprintf x; } while (0)
int rumdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, rum, CTLFLAG_RW, 0, "USB rum");
SYSCTL_INT(_hw_usb_rum, OID_AUTO, debug, CTLFLAG_RW, &rumdebug, 0,
    "rum debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* various supported device vendors/products */
static const struct usb_devno rum_devs[] = {
	{ USB_VENDOR_ABOCOM,            USB_PRODUCT_ABOCOM_HWU54DM },
	{ USB_VENDOR_ABOCOM,            USB_PRODUCT_ABOCOM_RT2573_2 },
	{ USB_VENDOR_ABOCOM,            USB_PRODUCT_ABOCOM_RT2573_3 },
	{ USB_VENDOR_ABOCOM,            USB_PRODUCT_ABOCOM_RT2573_4 },
	{ USB_VENDOR_ABOCOM,            USB_PRODUCT_ABOCOM_WUG2700 },
	{ USB_VENDOR_AMIT,              USB_PRODUCT_AMIT_CGWLUSB2GO },
	{ USB_VENDOR_ASUS,              USB_PRODUCT_ASUS_RT2573_1 },
	{ USB_VENDOR_ASUS,              USB_PRODUCT_ASUS_RT2573_2 },
	{ USB_VENDOR_BELKIN,            USB_PRODUCT_BELKIN_F5D7050A },
	{ USB_VENDOR_BELKIN,            USB_PRODUCT_BELKIN_F5D9050V3 },
	{ USB_VENDOR_CISCOLINKSYS,      USB_PRODUCT_CISCOLINKSYS_WUSB54GC },
	{ USB_VENDOR_CISCOLINKSYS,      USB_PRODUCT_CISCOLINKSYS_WUSB54GR },
	{ USB_VENDOR_CONCEPTRONIC2,     USB_PRODUCT_CONCEPTRONIC2_C54RU2 },
	{ USB_VENDOR_DICKSMITH,         USB_PRODUCT_DICKSMITH_CWD854F },
	{ USB_VENDOR_DICKSMITH,         USB_PRODUCT_DICKSMITH_RT2573 },
	{ USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_DWLG122C1 },
	{ USB_VENDOR_DLINK2,            USB_PRODUCT_DLINK2_WUA1340 },
	{ USB_VENDOR_GIGABYTE,          USB_PRODUCT_GIGABYTE_GNWB01GS },
	{ USB_VENDOR_GIGABYTE,          USB_PRODUCT_GIGABYTE_GNWI05GS },
	{ USB_VENDOR_GIGASET,           USB_PRODUCT_GIGASET_RT2573 },
	{ USB_VENDOR_GOODWAY,           USB_PRODUCT_GOODWAY_RT2573 },
	{ USB_VENDOR_GUILLEMOT,         USB_PRODUCT_GUILLEMOT_HWGUSB254LB },
	{ USB_VENDOR_GUILLEMOT,         USB_PRODUCT_GUILLEMOT_HWGUSB254V2AP },
	{ USB_VENDOR_HUAWEI3COM,        USB_PRODUCT_HUAWEI3COM_WUB320G },
	{ USB_VENDOR_MELCO,             USB_PRODUCT_MELCO_G54HP },
	{ USB_VENDOR_MELCO,             USB_PRODUCT_MELCO_SG54HP },
	{ USB_VENDOR_MSI,               USB_PRODUCT_MSI_RT2573_1 },
	{ USB_VENDOR_MSI,               USB_PRODUCT_MSI_RT2573_2 },
	{ USB_VENDOR_MSI,               USB_PRODUCT_MSI_RT2573_3 },
	{ USB_VENDOR_MSI,               USB_PRODUCT_MSI_RT2573_4 },
	{ USB_VENDOR_NOVATECH,          USB_PRODUCT_NOVATECH_RT2573 },
	{ USB_VENDOR_PLANEX2,           USB_PRODUCT_PLANEX2_GWUS54HP },
	{ USB_VENDOR_PLANEX2,           USB_PRODUCT_PLANEX2_GWUS54MINI2 },
	{ USB_VENDOR_PLANEX2,           USB_PRODUCT_PLANEX2_GWUSMM },
	{ USB_VENDOR_QCOM,              USB_PRODUCT_QCOM_RT2573 },
	{ USB_VENDOR_QCOM,              USB_PRODUCT_QCOM_RT2573_2 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2573 },
	{ USB_VENDOR_RALINK,            USB_PRODUCT_RALINK_RT2573_2 },
	{ USB_VENDOR_RALINK,            USB_PRODUCT_RALINK_RT2671 },
	{ USB_VENDOR_SITECOMEU,         USB_PRODUCT_SITECOMEU_WL113R2 },
	{ USB_VENDOR_SITECOMEU,         USB_PRODUCT_SITECOMEU_WL172 },
	{ USB_VENDOR_SURECOM,           USB_PRODUCT_SURECOM_RT2573 }
};

MODULE_DEPEND(rum, wlan, 1, 1, 1);
MODULE_DEPEND(rum, wlan_amrr, 1, 1, 1);

static int		rum_alloc_tx_list(struct rum_softc *);
static void		rum_free_tx_list(struct rum_softc *);
static int		rum_alloc_rx_list(struct rum_softc *);
static void		rum_free_rx_list(struct rum_softc *);
static int		rum_media_change(struct ifnet *);
static void		rum_task(void *);
static void		rum_scantask(void *);
static int		rum_newstate(struct ieee80211com *,
			    enum ieee80211_state, int);
static void		rum_txeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		rum_rxeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static int		rum_rxrate(struct rum_rx_desc *);
static int		rum_ack_rate(struct ieee80211com *, int);
static uint16_t		rum_txtime(int, int, uint32_t);
static uint8_t		rum_plcp_signal(int);
static void		rum_setup_tx_desc(struct rum_softc *,
			    struct rum_tx_desc *, uint32_t, uint16_t, int,
			    int);
static int		rum_tx_mgt(struct rum_softc *, struct mbuf *,
			    struct ieee80211_node *);
static int		rum_tx_raw(struct rum_softc *, struct mbuf *,
			    struct ieee80211_node *, 
			    const struct ieee80211_bpf_params *);
static int		rum_tx_data(struct rum_softc *, struct mbuf *,
			    struct ieee80211_node *);
static void		rum_start(struct ifnet *);
static void		rum_watchdog(void *);
static int		rum_ioctl(struct ifnet *, u_long, caddr_t);
static void		rum_eeprom_read(struct rum_softc *, uint16_t, void *,
			    int);
static uint32_t		rum_read(struct rum_softc *, uint16_t);
static void		rum_read_multi(struct rum_softc *, uint16_t, void *,
			    int);
static void		rum_write(struct rum_softc *, uint16_t, uint32_t);
static void		rum_write_multi(struct rum_softc *, uint16_t, void *,
			    size_t);
static void		rum_bbp_write(struct rum_softc *, uint8_t, uint8_t);
static uint8_t		rum_bbp_read(struct rum_softc *, uint8_t);
static void		rum_rf_write(struct rum_softc *, uint8_t, uint32_t);
static void		rum_select_antenna(struct rum_softc *);
static void		rum_enable_mrr(struct rum_softc *);
static void		rum_set_txpreamble(struct rum_softc *);
static void		rum_set_basicrates(struct rum_softc *);
static void		rum_select_band(struct rum_softc *,
			    struct ieee80211_channel *);
static void		rum_set_chan(struct rum_softc *,
			    struct ieee80211_channel *);
static void		rum_enable_tsf_sync(struct rum_softc *);
static void		rum_update_slot(struct ifnet *);
static void		rum_set_bssid(struct rum_softc *, const uint8_t *);
static void		rum_set_macaddr(struct rum_softc *, const uint8_t *);
static void		rum_update_promisc(struct rum_softc *);
static const char	*rum_get_rf(int);
static void		rum_read_eeprom(struct rum_softc *);
static int		rum_bbp_init(struct rum_softc *);
static void		rum_init(void *);
static void		rum_stop(void *);
static int		rum_load_microcode(struct rum_softc *, const u_char *,
			    size_t);
static int		rum_prepare_beacon(struct rum_softc *);
static int		rum_raw_xmit(struct ieee80211_node *, struct mbuf *,
			    const struct ieee80211_bpf_params *);
static void		rum_scan_start(struct ieee80211com *);
static void		rum_scan_end(struct ieee80211com *);
static void		rum_set_channel(struct ieee80211com *);
static int		rum_get_rssi(struct rum_softc *, uint8_t);
static void		rum_amrr_start(struct rum_softc *,
			    struct ieee80211_node *);
static void		rum_amrr_timeout(void *);
static void		rum_amrr_update(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);

static const struct {
	uint32_t	reg;
	uint32_t	val;
} rum_def_mac[] = {
	{ RT2573_TXRX_CSR0,  0x025fb032 },
	{ RT2573_TXRX_CSR1,  0x9eaa9eaf },
	{ RT2573_TXRX_CSR2,  0x8a8b8c8d }, 
	{ RT2573_TXRX_CSR3,  0x00858687 },
	{ RT2573_TXRX_CSR7,  0x2e31353b },
	{ RT2573_TXRX_CSR8,  0x2a2a2a2c },
	{ RT2573_TXRX_CSR15, 0x0000000f },
	{ RT2573_MAC_CSR6,   0x00000fff },
	{ RT2573_MAC_CSR8,   0x016c030a },
	{ RT2573_MAC_CSR10,  0x00000718 },
	{ RT2573_MAC_CSR12,  0x00000004 },
	{ RT2573_MAC_CSR13,  0x00007f00 },
	{ RT2573_SEC_CSR0,   0x00000000 },
	{ RT2573_SEC_CSR1,   0x00000000 },
	{ RT2573_SEC_CSR5,   0x00000000 },
	{ RT2573_PHY_CSR1,   0x000023b0 },
	{ RT2573_PHY_CSR5,   0x00040a06 },
	{ RT2573_PHY_CSR6,   0x00080606 },
	{ RT2573_PHY_CSR7,   0x00000408 },
	{ RT2573_AIFSN_CSR,  0x00002273 },
	{ RT2573_CWMIN_CSR,  0x00002344 },
	{ RT2573_CWMAX_CSR,  0x000034aa }
};

static const struct {
	uint8_t	reg;
	uint8_t	val;
} rum_def_bbp[] = {
	{   3, 0x80 },
	{  15, 0x30 },
	{  17, 0x20 },
	{  21, 0xc8 },
	{  22, 0x38 },
	{  23, 0x06 },
	{  24, 0xfe },
	{  25, 0x0a },
	{  26, 0x0d },
	{  32, 0x0b },
	{  34, 0x12 },
	{  37, 0x07 },
	{  39, 0xf8 },
	{  41, 0x60 },
	{  53, 0x10 },
	{  54, 0x18 },
	{  60, 0x10 },
	{  61, 0x04 },
	{  62, 0x04 },
	{  75, 0xfe },
	{  86, 0xfe },
	{  88, 0xfe },
	{  90, 0x0f },
	{  99, 0x00 },
	{ 102, 0x16 },
	{ 107, 0x04 }
};

static const struct rfprog {
	uint8_t		chan;
	uint32_t	r1, r2, r3, r4;
}  rum_rf5226[] = {
	{   1, 0x00b03, 0x001e1, 0x1a014, 0x30282 },
	{   2, 0x00b03, 0x001e1, 0x1a014, 0x30287 },
	{   3, 0x00b03, 0x001e2, 0x1a014, 0x30282 },
	{   4, 0x00b03, 0x001e2, 0x1a014, 0x30287 },
	{   5, 0x00b03, 0x001e3, 0x1a014, 0x30282 },
	{   6, 0x00b03, 0x001e3, 0x1a014, 0x30287 },
	{   7, 0x00b03, 0x001e4, 0x1a014, 0x30282 },
	{   8, 0x00b03, 0x001e4, 0x1a014, 0x30287 },
	{   9, 0x00b03, 0x001e5, 0x1a014, 0x30282 },
	{  10, 0x00b03, 0x001e5, 0x1a014, 0x30287 },
	{  11, 0x00b03, 0x001e6, 0x1a014, 0x30282 },
	{  12, 0x00b03, 0x001e6, 0x1a014, 0x30287 },
	{  13, 0x00b03, 0x001e7, 0x1a014, 0x30282 },
	{  14, 0x00b03, 0x001e8, 0x1a014, 0x30284 },

	{  34, 0x00b03, 0x20266, 0x36014, 0x30282 },
	{  38, 0x00b03, 0x20267, 0x36014, 0x30284 },
	{  42, 0x00b03, 0x20268, 0x36014, 0x30286 },
	{  46, 0x00b03, 0x20269, 0x36014, 0x30288 },

	{  36, 0x00b03, 0x00266, 0x26014, 0x30288 },
	{  40, 0x00b03, 0x00268, 0x26014, 0x30280 },
	{  44, 0x00b03, 0x00269, 0x26014, 0x30282 },
	{  48, 0x00b03, 0x0026a, 0x26014, 0x30284 },
	{  52, 0x00b03, 0x0026b, 0x26014, 0x30286 },
	{  56, 0x00b03, 0x0026c, 0x26014, 0x30288 },
	{  60, 0x00b03, 0x0026e, 0x26014, 0x30280 },
	{  64, 0x00b03, 0x0026f, 0x26014, 0x30282 },

	{ 100, 0x00b03, 0x0028a, 0x2e014, 0x30280 },
	{ 104, 0x00b03, 0x0028b, 0x2e014, 0x30282 },
	{ 108, 0x00b03, 0x0028c, 0x2e014, 0x30284 },
	{ 112, 0x00b03, 0x0028d, 0x2e014, 0x30286 },
	{ 116, 0x00b03, 0x0028e, 0x2e014, 0x30288 },
	{ 120, 0x00b03, 0x002a0, 0x2e014, 0x30280 },
	{ 124, 0x00b03, 0x002a1, 0x2e014, 0x30282 },
	{ 128, 0x00b03, 0x002a2, 0x2e014, 0x30284 },
	{ 132, 0x00b03, 0x002a3, 0x2e014, 0x30286 },
	{ 136, 0x00b03, 0x002a4, 0x2e014, 0x30288 },
	{ 140, 0x00b03, 0x002a6, 0x2e014, 0x30280 },

	{ 149, 0x00b03, 0x002a8, 0x2e014, 0x30287 },
	{ 153, 0x00b03, 0x002a9, 0x2e014, 0x30289 },
	{ 157, 0x00b03, 0x002ab, 0x2e014, 0x30281 },
	{ 161, 0x00b03, 0x002ac, 0x2e014, 0x30283 },
	{ 165, 0x00b03, 0x002ad, 0x2e014, 0x30285 }
}, rum_rf5225[] = {
	{   1, 0x00b33, 0x011e1, 0x1a014, 0x30282 },
	{   2, 0x00b33, 0x011e1, 0x1a014, 0x30287 },
	{   3, 0x00b33, 0x011e2, 0x1a014, 0x30282 },
	{   4, 0x00b33, 0x011e2, 0x1a014, 0x30287 },
	{   5, 0x00b33, 0x011e3, 0x1a014, 0x30282 },
	{   6, 0x00b33, 0x011e3, 0x1a014, 0x30287 },
	{   7, 0x00b33, 0x011e4, 0x1a014, 0x30282 },
	{   8, 0x00b33, 0x011e4, 0x1a014, 0x30287 },
	{   9, 0x00b33, 0x011e5, 0x1a014, 0x30282 },
	{  10, 0x00b33, 0x011e5, 0x1a014, 0x30287 },
	{  11, 0x00b33, 0x011e6, 0x1a014, 0x30282 },
	{  12, 0x00b33, 0x011e6, 0x1a014, 0x30287 },
	{  13, 0x00b33, 0x011e7, 0x1a014, 0x30282 },
	{  14, 0x00b33, 0x011e8, 0x1a014, 0x30284 },

	{  34, 0x00b33, 0x01266, 0x26014, 0x30282 },
	{  38, 0x00b33, 0x01267, 0x26014, 0x30284 },
	{  42, 0x00b33, 0x01268, 0x26014, 0x30286 },
	{  46, 0x00b33, 0x01269, 0x26014, 0x30288 },

	{  36, 0x00b33, 0x01266, 0x26014, 0x30288 },
	{  40, 0x00b33, 0x01268, 0x26014, 0x30280 },
	{  44, 0x00b33, 0x01269, 0x26014, 0x30282 },
	{  48, 0x00b33, 0x0126a, 0x26014, 0x30284 },
	{  52, 0x00b33, 0x0126b, 0x26014, 0x30286 },
	{  56, 0x00b33, 0x0126c, 0x26014, 0x30288 },
	{  60, 0x00b33, 0x0126e, 0x26014, 0x30280 },
	{  64, 0x00b33, 0x0126f, 0x26014, 0x30282 },

	{ 100, 0x00b33, 0x0128a, 0x2e014, 0x30280 },
	{ 104, 0x00b33, 0x0128b, 0x2e014, 0x30282 },
	{ 108, 0x00b33, 0x0128c, 0x2e014, 0x30284 },
	{ 112, 0x00b33, 0x0128d, 0x2e014, 0x30286 },
	{ 116, 0x00b33, 0x0128e, 0x2e014, 0x30288 },
	{ 120, 0x00b33, 0x012a0, 0x2e014, 0x30280 },
	{ 124, 0x00b33, 0x012a1, 0x2e014, 0x30282 },
	{ 128, 0x00b33, 0x012a2, 0x2e014, 0x30284 },
	{ 132, 0x00b33, 0x012a3, 0x2e014, 0x30286 },
	{ 136, 0x00b33, 0x012a4, 0x2e014, 0x30288 },
	{ 140, 0x00b33, 0x012a6, 0x2e014, 0x30280 },

	{ 149, 0x00b33, 0x012a8, 0x2e014, 0x30287 },
	{ 153, 0x00b33, 0x012a9, 0x2e014, 0x30289 },
	{ 157, 0x00b33, 0x012ab, 0x2e014, 0x30281 },
	{ 161, 0x00b33, 0x012ac, 0x2e014, 0x30283 },
	{ 165, 0x00b33, 0x012ad, 0x2e014, 0x30285 }
};

USB_DECLARE_DRIVER(rum);

USB_MATCH(rum)
{
	USB_MATCH_START(rum, uaa);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(rum_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

USB_ATTACH(rum)
{
	USB_ATTACH_START(rum, sc, uaa);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp;
	const uint8_t *ucode = NULL;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	int i, ntries, size, bands;
	uint32_t tmp;

	sc->sc_udev = uaa->device;
	sc->sc_dev = self;

	if (usbd_set_config_no(sc->sc_udev, RT2573_CONFIG_NO, 0) != 0) {
		printf("%s: could not set configuration no\n",
		    device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, RT2573_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		printf("%s: could not get interface handle\n",
		    device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	/*
	 * Find endpoints.
	 */
	id = usbd_get_interface_descriptor(sc->sc_iface);

	sc->sc_rx_no = sc->sc_tx_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for iface %d\n",
			    device_get_nameunit(sc->sc_dev), i);
			return ENXIO;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_rx_no = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_tx_no = ed->bEndpointAddress;
	}
	if (sc->sc_rx_no == -1 || sc->sc_tx_no == -1) {
		printf("%s: missing endpoint\n", 
		    device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	usb_init_task(&sc->sc_task, rum_task, sc);
	usb_init_task(&sc->sc_scantask, rum_scantask, sc);
	callout_init(&sc->watchdog_ch, 0);
	callout_init(&sc->amrr_ch, 0);

	/* retrieve RT2573 rev. no */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((tmp = rum_read(sc, RT2573_MAC_CSR0)) != 0)
			break;
		DELAY(1000);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for chip to settle\n",
		    device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	/* retrieve MAC address and various other things from EEPROM */
	rum_read_eeprom(sc);

	printf("%s: MAC/BBP RT2573 (rev 0x%05x), RF %s\n",
	    device_get_nameunit(sc->sc_dev), tmp, rum_get_rf(sc->rf_rev));

	ucode = rt2573_ucode;
	size = sizeof rt2573_ucode;
	error = rum_load_microcode(sc, ucode, size);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load 8051 microcode\n");
		mtx_destroy(&sc->sc_mtx);
		return ENXIO;
	}

	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("%s: can not if_alloc()\n", 
		    device_get_nameunit(sc->sc_dev));
		mtx_destroy(&sc->sc_mtx);
		return ENXIO;
	}

	ifp->if_softc = sc;
	if_initname(ifp, "rum", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT; /* USB stack is still under Giant lock */
	ifp->if_init = rum_init;
	ifp->if_ioctl = rum_ioctl;
	ifp->if_start = rum_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAp mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_BGSCAN |	/* bg scanning supported */
	    IEEE80211_C_WPA;		/* 802.11i */

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, 0, CTRY_DEFAULT, bands, 0, 1);

	if (sc->rf_rev == RT2573_RF_5225 || sc->rf_rev == RT2573_RF_5226) {
		struct ieee80211_channel *c;

		/* set supported .11a channels */
		for (i = 34; i <= 46; i += 4) {
			c = &ic->ic_channels[ic->ic_nchans++];
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
		for (i = 36; i <= 64; i += 4) {
			c = &ic->ic_channels[ic->ic_nchans++];
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
		for (i = 100; i <= 140; i += 4) {
			c = &ic->ic_channels[ic->ic_nchans++];
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
		for (i = 149; i <= 165; i += 4) {
			c = &ic->ic_channels[ic->ic_nchans++];
			c->ic_freq = ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A;
			c->ic_ieee = i;
		}
	}

	ieee80211_ifattach(ic);
	ic->ic_scan_start = rum_scan_start;
	ic->ic_scan_end = rum_scan_end;
	ic->ic_set_channel = rum_set_channel;

	/* enable s/w bmiss handling in sta mode */
	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rum_newstate;
	ic->ic_raw_xmit = rum_raw_xmit;
	ieee80211_media_init(ic, rum_media_change, ieee80211_media_status);

	ieee80211_amrr_init(&sc->amrr, ic,
	    IEEE80211_AMRR_MIN_SUCCESS_THRESHOLD,
	    IEEE80211_AMRR_MAX_SUCCESS_THRESHOLD);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN, 
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RT2573_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RT2573_TX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;
}

USB_DETACH(rum)
{
	USB_DETACH_START(rum, sc);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	rum_stop(sc);
	usb_rem_task(sc->sc_udev, &sc->sc_task);
	usb_rem_task(sc->sc_udev, &sc->sc_scantask);
	callout_stop(&sc->watchdog_ch);
	callout_stop(&sc->amrr_ch);

	if (sc->amrr_xfer != NULL) {
		usbd_free_xfer(sc->amrr_xfer);
		sc->amrr_xfer = NULL;
	}

	if (sc->sc_rx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_rx_pipeh);
		usbd_close_pipe(sc->sc_rx_pipeh);
	}
	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
	}
	
	rum_free_rx_list(sc);
	rum_free_tx_list(sc);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);
	if_free(ifp);

	mtx_destroy(&sc->sc_mtx);

	return 0;
}

static int
rum_alloc_tx_list(struct rum_softc *sc)
{
	struct rum_tx_data *data;
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < RUM_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate tx xfer\n",
			    device_get_nameunit(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer,
		    RT2573_TX_DESC_SIZE + MCLBYTES);
		if (data->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    device_get_nameunit(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		/* clean Tx descriptor */
		bzero(data->buf, RT2573_TX_DESC_SIZE);
	}

	return 0;

fail:	rum_free_tx_list(sc);
	return error;
}

static void
rum_free_tx_list(struct rum_softc *sc)
{
	struct rum_tx_data *data;
	int i;

	for (i = 0; i < RUM_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

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
rum_alloc_rx_list(struct rum_softc *sc)
{
	struct rum_rx_data *data;
	int i, error;

	for (i = 0; i < RUM_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate rx xfer\n",
			    device_get_nameunit(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		if (usbd_alloc_buffer(data->xfer, MCLBYTES) == NULL) {
			printf("%s: could not allocate rx buffer\n",
			    device_get_nameunit(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		data->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    device_get_nameunit(sc->sc_dev));
		    	error = ENOMEM;
			goto fail;
		}

		data->buf = mtod(data->m, uint8_t *);
	}

	return 0;

fail:	rum_free_tx_list(sc);
	return error;
}

static void
rum_free_rx_list(struct rum_softc *sc)
{
	struct rum_rx_data *data;
	int i;

	for (i = 0; i < RUM_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
	}
}

static int
rum_media_change(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;
	int error;

	RUM_LOCK(sc);

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET) {
		RUM_UNLOCK(sc);
		return error;
	}

	if ((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING))
		rum_init(sc);

	RUM_UNLOCK(sc);

	return 0;
}

static void
rum_task(void *arg)
{
	struct rum_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	uint32_t tmp;

	ostate = ic->ic_state;

	RUM_LOCK(sc);

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			tmp = rum_read(sc, RT2573_TXRX_CSR9);
			rum_write(sc, RT2573_TXRX_CSR9, tmp & ~0x00ffffff);
		}
		break;

	case IEEE80211_S_RUN:
		ni = ic->ic_bss;

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			rum_update_slot(ic->ic_ifp);
			rum_enable_mrr(sc);
			rum_set_txpreamble(sc);
			rum_set_basicrates(sc);
			rum_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
			rum_prepare_beacon(sc);

		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			rum_enable_tsf_sync(sc);

		/* enable automatic rate adaptation in STA mode */
		if (ic->ic_opmode == IEEE80211_M_STA &&
		    ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE)
			rum_amrr_start(sc, ni);
		break;
	default:
		break;
	}

	RUM_UNLOCK(sc);

	sc->sc_newstate(ic, sc->sc_state, sc->sc_arg);
}

static int
rum_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rum_softc *sc = ic->ic_ifp->if_softc;

	callout_stop(&sc->amrr_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	if (nstate == IEEE80211_S_INIT)
		sc->sc_newstate(ic, nstate, arg);
	else
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);
	return 0;
}

/* quickly determine if a given rate is CCK or OFDM */
#define RUM_RATE_IS_OFDM(rate)	((rate) >= 12 && (rate) != 22)

#define RUM_ACK_SIZE	14	/* 10 + 4(FCS) */
#define RUM_CTS_SIZE	14	/* 10 + 4(FCS) */

static void
rum_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct rum_tx_data *data = priv;
	struct rum_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ic.ic_ifp;

	if (data->m->m_flags & M_TXCB)
		ieee80211_process_callback(data->ni, data->m,
			status == USBD_NORMAL_COMPLETION ? 0 : ETIMEDOUT);

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_tx_pipeh);

		ifp->if_oerrors++;
		return;
	}

	m_freem(data->m);
	data->m = NULL;
	ieee80211_free_node(data->ni);
	data->ni = NULL;

	sc->tx_queued--;
	ifp->if_opackets++;

	DPRINTFN(10, ("tx done\n"));

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	rum_start(ifp);
}

static void
rum_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct rum_rx_data *data = priv;
	struct rum_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rum_rx_desc *desc;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *mnew, *m;
	int len, rssi;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipeh);
		goto skip;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len < RT2573_RX_DESC_SIZE + sizeof (struct ieee80211_frame_min)) {
		DPRINTF(("%s: xfer too short %d\n", 
		    device_get_nameunit(sc->sc_dev), len));
		ifp->if_ierrors++;
		goto skip;
	}

	desc = (struct rum_rx_desc *)data->buf;

	if (le32toh(desc->flags) & RT2573_RX_CRC_ERROR) {
		/*
		 * This should not happen since we did not request to receive
		 * those frames when we filled RT2573_TXRX_CSR0.
		 */
		DPRINTFN(5, ("CRC error\n"));
		ifp->if_ierrors++;
		goto skip;
	}

	mnew = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (mnew == NULL) {
		ifp->if_ierrors++;
		goto skip;
	}

	m = data->m;
	data->m = mnew;
	data->buf = mtod(data->m, uint8_t *);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_data = (caddr_t)(desc + 1); 
	m->m_pkthdr.len = m->m_len = (le32toh(desc->flags) >> 16) & 0xfff;

	rssi = rum_get_rssi(sc, desc->rssi);

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* Error happened during RSSI conversion. */
	if (rssi < 0)
		rssi = ni->ni_rssi;

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rum_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
		tap->wr_rate = rum_rxrate(desc);
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_antenna = sc->rx_ant;
		tap->wr_antsignal = rssi;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	/* send the frame to the 802.11 layer */
	ieee80211_input(ic, m, ni, rssi, RT2573_NOISE_FLOOR, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	DPRINTFN(15, ("rx done\n"));

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->sc_rx_pipeh, data, data->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rum_rxeof);
	usbd_transfer(xfer);
}

/*
 * This function is only used by the Rx radiotap code. 
 */
static int
rum_rxrate(struct rum_rx_desc *desc)
{
	if (le32toh(desc->flags) & RT2573_RX_OFDM) {
		/* reverse function of rum_plcp_signal */
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
 */
static int
rum_ack_rate(struct ieee80211com *ic, int rate)
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
rum_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;

	if (RUM_RATE_IS_OFDM(rate)) {
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
rum_plcp_signal(int rate)
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
rum_setup_tx_desc(struct rum_softc *sc, struct rum_tx_desc *desc,
    uint32_t flags, uint16_t xflags, int len, int rate)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t plcp_length;
	int remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(RT2573_TX_VALID);
	desc->flags |= htole32(len << 16);

	desc->xflags = htole16(xflags);

	desc->wme = htole16(RT2573_QID(0) | RT2573_AIFSN(2) | 
	    RT2573_LOGCWMIN(4) | RT2573_LOGCWMAX(10));

	/* setup PLCP fields */
	desc->plcp_signal  = rum_plcp_signal(rate);
	desc->plcp_service = 4;

	len += IEEE80211_CRC_LEN;
	if (RUM_RATE_IS_OFDM(rate)) {
		desc->flags |= htole32(RT2573_TX_OFDM);

		plcp_length = len & 0xfff;
		desc->plcp_length_hi = plcp_length >> 6;
		desc->plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = (16 * len + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if (remainder != 0 && remainder < 7)
				desc->plcp_service |= RT2573_PLCP_LENGEXT;
		}
		desc->plcp_length_hi = plcp_length >> 8;
		desc->plcp_length_lo = plcp_length & 0xff;

		if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			desc->plcp_signal |= 0x08;
	}
}

#define RUM_TX_TIMEOUT	5000

static int
rum_tx_mgt(struct rum_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rum_tx_desc *desc;
	struct rum_tx_data *data;
	struct ieee80211_frame *wh;
	uint32_t flags = 0;
	uint16_t dur;
	usbd_status error;
	int xferlen, rate;

	data = &sc->tx_data[0];
	desc = (struct rum_tx_desc *)data->buf;

	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2573_TX_NEED_ACK;

		dur = rum_txtime(RUM_ACK_SIZE, rum_ack_rate(ic, rate), 
		    ic->ic_flags) + sc->sifs;
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			flags |= RT2573_TX_TIMESTAMP;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rum_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RT2573_TX_DESC_SIZE);
	rum_setup_tx_desc(sc, desc, flags, 0, m0->m_pkthdr.len, rate);

	/* align end on a 4-bytes boundary */
	xferlen = (RT2573_TX_DESC_SIZE + m0->m_pkthdr.len + 3) & ~3;

	/*
	 * No space left in the last URB to store the extra 4 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 4;

	DPRINTFN(10, ("sending mgt frame len=%d rate=%d xfer len=%d\n",
	    m0->m_pkthdr.len + (int)RT2573_TX_DESC_SIZE, rate, xferlen));
	
	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RUM_TX_TIMEOUT, rum_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		data->m = NULL;
		data->ni = NULL;
		return error;
	}

	sc->tx_queued++;

	return 0;
}

static int
rum_tx_raw(struct rum_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rum_tx_desc *desc;
	struct rum_tx_data *data;
	uint32_t flags;
	usbd_status error;
	int xferlen, rate;

	data = &sc->tx_data[0];
	desc = (struct rum_tx_desc *)data->buf;

	rate = params->ibp_rate0 & IEEE80211_RATE_VAL;
	/* XXX validate */
	if (rate == 0) {
		m_freem(m0);
		return EINVAL;
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rum_tx_radiotap_header *tap = &sc->sc_txtap;

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
		flags |= RT2573_TX_NEED_ACK;

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RT2573_TX_DESC_SIZE);
	/* XXX need to setup descriptor ourself */
	rum_setup_tx_desc(sc, desc, flags, 0, m0->m_pkthdr.len, rate);

	/* align end on a 4-bytes boundary */
	xferlen = (RT2573_TX_DESC_SIZE + m0->m_pkthdr.len + 3) & ~3;

	/*
	 * No space left in the last URB to store the extra 4 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 4;

	DPRINTFN(10, ("sending raw frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf,
	    xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RUM_TX_TIMEOUT,
	    rum_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS)
		return error;

	sc->tx_queued++;

	return 0;
}

static int
rum_tx_data(struct rum_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rum_tx_desc *desc;
	struct rum_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	uint32_t flags = 0;
	uint16_t dur;
	usbd_status error;
	int rate, xferlen;

	wh = mtod(m0, struct ieee80211_frame *);

	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE)
		rate = ic->ic_fixed_rate;
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

	data = &sc->tx_data[0];
	desc = (struct rum_tx_desc *)data->buf;

	data->m = m0;
	data->ni = ni;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2573_TX_NEED_ACK;
		flags |= RT2573_TX_MORE_FRAG;

		dur = rum_txtime(RUM_ACK_SIZE, rum_ack_rate(ic, rate),
		    ic->ic_flags) + sc->sifs;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct rum_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RT2573_TX_DESC_SIZE);
	rum_setup_tx_desc(sc, desc, flags, 0, m0->m_pkthdr.len, rate);

	/* align end on a 4-bytes boundary */
	xferlen = (RT2573_TX_DESC_SIZE + m0->m_pkthdr.len + 3) & ~3;

	/*
	 * No space left in the last URB to store the extra 4 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 4;

	DPRINTFN(10, ("sending frame len=%d rate=%d xfer len=%d\n",
	    m0->m_pkthdr.len + (int)RT2573_TX_DESC_SIZE, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RUM_TX_TIMEOUT, rum_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		data->m = NULL;
		data->ni = NULL;
		return error;
	}

	sc->tx_queued++;

	return 0;
}

static void
rum_start(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m0;
	struct ether_header *eh;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->tx_queued >= RUM_TX_LIST_COUNT) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;

			if (bpf_peers_present(ic->ic_rawbpf))
				bpf_mtap(ic->ic_rawbpf, m0);

			if (rum_tx_mgt(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				break;
			}
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DRV_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->tx_queued >= RUM_TX_LIST_COUNT) {
				IFQ_DRV_PREPEND(&ifp->if_snd, m0);
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
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

			if (bpf_peers_present(ic->ic_rawbpf))
				bpf_mtap(ic->ic_rawbpf, m0);

			if (rum_tx_data(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->sc_tx_timer = 5;
		callout_reset(&sc->watchdog_ch, hz, rum_watchdog, sc);
	}
}

static void
rum_watchdog(void *arg)
{
	struct rum_softc *sc = arg;

	RUM_LOCK(sc);

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			/*rum_init(ifp); XXX needs a process context! */
			sc->sc_ifp->if_oerrors++;
			RUM_UNLOCK(sc);
			return;
		}
		callout_reset(&sc->watchdog_ch, hz, rum_watchdog, sc);
	}

	RUM_UNLOCK(sc);
}

static int
rum_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;

	RUM_LOCK(sc);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				rum_update_promisc(sc);
			else
				rum_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				rum_stop(sc);
		}
		break;
	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) &&
		    (ic->ic_roaming != IEEE80211_ROAMING_MANUAL))
			rum_init(sc);
		error = 0;
	}

	RUM_UNLOCK(sc);

	return error;
}

static void
rum_eeprom_read(struct rum_softc *sc, uint16_t addr, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_EEPROM;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not read EEPROM: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(error));
	}
}

static uint32_t
rum_read(struct rum_softc *sc, uint16_t reg)
{
	uint32_t val;

	rum_read_multi(sc, reg, &val, sizeof val);

	return le32toh(val);
}

static void
rum_read_multi(struct rum_softc *sc, uint16_t reg, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not multi read MAC register: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(error));
	}
}

static void
rum_write(struct rum_softc *sc, uint16_t reg, uint32_t val)
{
	uint32_t tmp = htole32(val);

	rum_write_multi(sc, reg, &tmp, sizeof tmp);
}

static void
rum_write_multi(struct rum_softc *sc, uint16_t reg, void *buf, size_t len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2573_WRITE_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not multi write MAC register: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(error));
	}
}

static void
rum_bbp_write(struct rum_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(rum_read(sc, RT2573_PHY_CSR3) & RT2573_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not write to BBP\n", 
		    device_get_nameunit(sc->sc_dev));
		return;
	}

	tmp = RT2573_BBP_BUSY | (reg & 0x7f) << 8 | val;
	rum_write(sc, RT2573_PHY_CSR3, tmp);
}

static uint8_t
rum_bbp_read(struct rum_softc *sc, uint8_t reg)
{
	uint32_t val;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(rum_read(sc, RT2573_PHY_CSR3) & RT2573_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not read BBP\n", 
		    device_get_nameunit(sc->sc_dev));
		return 0;
	}

	val = RT2573_BBP_BUSY | RT2573_BBP_READ | reg << 8;
	rum_write(sc, RT2573_PHY_CSR3, val);

	for (ntries = 0; ntries < 100; ntries++) {
		val = rum_read(sc, RT2573_PHY_CSR3);
		if (!(val & RT2573_BBP_BUSY))
			return val & 0xff;
		DELAY(1);
	}

	printf("%s: could not read BBP\n", device_get_nameunit(sc->sc_dev));
	return 0;
}

static void
rum_rf_write(struct rum_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(rum_read(sc, RT2573_PHY_CSR4) & RT2573_RF_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not write to RF\n", 
		    device_get_nameunit(sc->sc_dev));
		return;
	}

	tmp = RT2573_RF_BUSY | RT2573_RF_20BIT | (val & 0xfffff) << 2 |
	    (reg & 3);
	rum_write(sc, RT2573_PHY_CSR4, tmp);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 3, val & 0xfffff));
}

static void
rum_select_antenna(struct rum_softc *sc)
{
	uint8_t bbp4, bbp77;
	uint32_t tmp;

	bbp4  = rum_bbp_read(sc, 4);
	bbp77 = rum_bbp_read(sc, 77);

	/* TBD */

	/* make sure Rx is disabled before switching antenna */
	tmp = rum_read(sc, RT2573_TXRX_CSR0);
	rum_write(sc, RT2573_TXRX_CSR0, tmp | RT2573_DISABLE_RX);

	rum_bbp_write(sc,  4, bbp4);
	rum_bbp_write(sc, 77, bbp77);

	rum_write(sc, RT2573_TXRX_CSR0, tmp);
}

/*
 * Enable multi-rate retries for frames sent at OFDM rates.
 * In 802.11b/g mode, allow fallback to CCK rates.
 */
static void
rum_enable_mrr(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	tmp = rum_read(sc, RT2573_TXRX_CSR4);

	tmp &= ~RT2573_MRR_CCK_FALLBACK;
	if (!IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
		tmp |= RT2573_MRR_CCK_FALLBACK;
	tmp |= RT2573_MRR_ENABLED;

	rum_write(sc, RT2573_TXRX_CSR4, tmp);
}

static void
rum_set_txpreamble(struct rum_softc *sc)
{
	uint32_t tmp;

	tmp = rum_read(sc, RT2573_TXRX_CSR4);

	tmp &= ~RT2573_SHORT_PREAMBLE;
	if (sc->sc_ic.ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RT2573_SHORT_PREAMBLE;

	rum_write(sc, RT2573_TXRX_CSR4, tmp);
}

static void
rum_set_basicrates(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* update basic rate set */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		/* 11b basic rates: 1, 2Mbps */
		rum_write(sc, RT2573_TXRX_CSR5, 0x3);
	} else if (IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan)) {
		/* 11a basic rates: 6, 12, 24Mbps */
		rum_write(sc, RT2573_TXRX_CSR5, 0x150);
	} else {
		/* 11b/g basic rates: 1, 2, 5.5, 11Mbps */
		rum_write(sc, RT2573_TXRX_CSR5, 0xf);
	}
}

/*
 * Reprogram MAC/BBP to switch to a new band.  Values taken from the reference
 * driver.
 */
static void
rum_select_band(struct rum_softc *sc, struct ieee80211_channel *c)
{
	uint8_t bbp17, bbp35, bbp96, bbp97, bbp98, bbp104;
	uint32_t tmp;

	/* update all BBP registers that depend on the band */
	bbp17 = 0x20; bbp96 = 0x48; bbp104 = 0x2c;
	bbp35 = 0x50; bbp97 = 0x48; bbp98  = 0x48;
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		bbp17 += 0x08; bbp96 += 0x10; bbp104 += 0x0c;
		bbp35 += 0x10; bbp97 += 0x10; bbp98  += 0x10;
	}
	if ((IEEE80211_IS_CHAN_2GHZ(c) && sc->ext_2ghz_lna) ||
	    (IEEE80211_IS_CHAN_5GHZ(c) && sc->ext_5ghz_lna)) {
		bbp17 += 0x10; bbp96 += 0x10; bbp104 += 0x10;
	}

	sc->bbp17 = bbp17;
	rum_bbp_write(sc,  17, bbp17);
	rum_bbp_write(sc,  96, bbp96);
	rum_bbp_write(sc, 104, bbp104);

	if ((IEEE80211_IS_CHAN_2GHZ(c) && sc->ext_2ghz_lna) ||
	    (IEEE80211_IS_CHAN_5GHZ(c) && sc->ext_5ghz_lna)) {
		rum_bbp_write(sc, 75, 0x80);
		rum_bbp_write(sc, 86, 0x80);
		rum_bbp_write(sc, 88, 0x80);
	}

	rum_bbp_write(sc, 35, bbp35);
	rum_bbp_write(sc, 97, bbp97);
	rum_bbp_write(sc, 98, bbp98);

	tmp = rum_read(sc, RT2573_PHY_CSR0);
	tmp &= ~(RT2573_PA_PE_2GHZ | RT2573_PA_PE_5GHZ);
	if (IEEE80211_IS_CHAN_2GHZ(c))
		tmp |= RT2573_PA_PE_2GHZ;
	else
		tmp |= RT2573_PA_PE_5GHZ;
	rum_write(sc, RT2573_PHY_CSR0, tmp);

	/* 802.11a uses a 16 microseconds short interframe space */
	sc->sifs = IEEE80211_IS_CHAN_5GHZ(c) ? 16 : 10;
}

static void
rum_set_chan(struct rum_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct rfprog *rfprog;
	uint8_t bbp3, bbp94 = RT2573_BBPR94_DEFAULT;
	int8_t power;
	u_int i, chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	/* select the appropriate RF settings based on what EEPROM says */
	rfprog = (sc->rf_rev == RT2573_RF_5225 ||
		  sc->rf_rev == RT2573_RF_2527) ? rum_rf5225 : rum_rf5226;

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rfprog[i].chan != chan; i++);

	power = sc->txpow[i];
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
	if (c->ic_flags != ic->ic_curchan->ic_flags) {
		rum_select_band(sc, c);
		rum_select_antenna(sc);
	}
	ic->ic_curchan = c;

	rum_rf_write(sc, RT2573_RF1, rfprog[i].r1);
	rum_rf_write(sc, RT2573_RF2, rfprog[i].r2);
	rum_rf_write(sc, RT2573_RF3, rfprog[i].r3 | power << 7);
	rum_rf_write(sc, RT2573_RF4, rfprog[i].r4 | sc->rffreq << 10);

	rum_rf_write(sc, RT2573_RF1, rfprog[i].r1);
	rum_rf_write(sc, RT2573_RF2, rfprog[i].r2);
	rum_rf_write(sc, RT2573_RF3, rfprog[i].r3 | power << 7 | 1);
	rum_rf_write(sc, RT2573_RF4, rfprog[i].r4 | sc->rffreq << 10);

	rum_rf_write(sc, RT2573_RF1, rfprog[i].r1);
	rum_rf_write(sc, RT2573_RF2, rfprog[i].r2);
	rum_rf_write(sc, RT2573_RF3, rfprog[i].r3 | power << 7);
	rum_rf_write(sc, RT2573_RF4, rfprog[i].r4 | sc->rffreq << 10);

	DELAY(10);

	/* enable smart mode for MIMO-capable RFs */
	bbp3 = rum_bbp_read(sc, 3);

	bbp3 &= ~RT2573_SMART_MODE;
	if (sc->rf_rev == RT2573_RF_5225 || sc->rf_rev == RT2573_RF_2527)
		bbp3 |= RT2573_SMART_MODE;

	rum_bbp_write(sc, 3, bbp3);

	if (bbp94 != RT2573_BBPR94_DEFAULT)
		rum_bbp_write(sc, 94, bbp94);
}

/*
 * Enable TSF synchronization and tell h/w to start sending beacons for IBSS
 * and HostAP operating modes.
 */
static void
rum_enable_tsf_sync(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	if (ic->ic_opmode != IEEE80211_M_STA) {
		/*
		 * Change default 16ms TBTT adjustment to 8ms.
		 * Must be done before enabling beacon generation.
		 */
		rum_write(sc, RT2573_TXRX_CSR10, 1 << 12 | 8);
	}

	tmp = rum_read(sc, RT2573_TXRX_CSR9) & 0xff000000;

	/* set beacon interval (in 1/16ms unit) */
	tmp |= ic->ic_bss->ni_intval * 16;

	tmp |= RT2573_TSF_TICKING | RT2573_ENABLE_TBTT;
	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= RT2573_TSF_MODE(1);
	else
		tmp |= RT2573_TSF_MODE(2) | RT2573_GENERATE_BEACON;

	rum_write(sc, RT2573_TXRX_CSR9, tmp);
}

static void
rum_update_slot(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t slottime;
	uint32_t tmp;

	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	tmp = rum_read(sc, RT2573_MAC_CSR9);
	tmp = (tmp & ~0xff) | slottime;
	rum_write(sc, RT2573_MAC_CSR9, tmp);

	DPRINTF(("setting slot time to %uus\n", slottime));
}

static void
rum_set_bssid(struct rum_softc *sc, const uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24;
	rum_write(sc, RT2573_MAC_CSR4, tmp);

	tmp = bssid[4] | bssid[5] << 8 | RT2573_ONE_BSSID << 16;
	rum_write(sc, RT2573_MAC_CSR5, tmp);
}

static void
rum_set_macaddr(struct rum_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
	rum_write(sc, RT2573_MAC_CSR2, tmp);

	tmp = addr[4] | addr[5] << 8 | 0xff << 16;
	rum_write(sc, RT2573_MAC_CSR3, tmp);
}

static void
rum_update_promisc(struct rum_softc *sc)
{
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t tmp;

	tmp = rum_read(sc, RT2573_TXRX_CSR0);

	tmp &= ~RT2573_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RT2573_DROP_NOT_TO_ME;

	rum_write(sc, RT2573_TXRX_CSR0, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

static const char *
rum_get_rf(int rev)
{
	switch (rev) {
	case RT2573_RF_2527:	return "RT2527 (MIMO XR)";
	case RT2573_RF_2528:	return "RT2528";
	case RT2573_RF_5225:	return "RT5225 (MIMO XR)";
	case RT2573_RF_5226:	return "RT5226";
	default:		return "unknown";
	}
}

static void
rum_read_eeprom(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
#ifdef RUM_DEBUG
	int i;
#endif

	/* read MAC address */
	rum_eeprom_read(sc, RT2573_EEPROM_ADDRESS, ic->ic_myaddr, 6);

	rum_eeprom_read(sc, RT2573_EEPROM_ANTENNA, &val, 2);
	val = le16toh(val);
	sc->rf_rev =   (val >> 11) & 0x1f;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->rx_ant =   (val >> 4)  & 0x3;
	sc->tx_ant =   (val >> 2)  & 0x3;
	sc->nb_ant =   val & 0x3;

	DPRINTF(("RF revision=%d\n", sc->rf_rev));

	rum_eeprom_read(sc, RT2573_EEPROM_CONFIG2, &val, 2);
	val = le16toh(val);
	sc->ext_5ghz_lna = (val >> 6) & 0x1;
	sc->ext_2ghz_lna = (val >> 4) & 0x1;

	DPRINTF(("External 2GHz LNA=%d\nExternal 5GHz LNA=%d\n",
	    sc->ext_2ghz_lna, sc->ext_5ghz_lna));

	rum_eeprom_read(sc, RT2573_EEPROM_RSSI_2GHZ_OFFSET, &val, 2);
	val = le16toh(val);
	if ((val & 0xff) != 0xff)
		sc->rssi_2ghz_corr = (int8_t)(val & 0xff);	/* signed */

	/* Only [-10, 10] is valid */
	if (sc->rssi_2ghz_corr < -10 || sc->rssi_2ghz_corr > 10)
		sc->rssi_2ghz_corr = 0;

	rum_eeprom_read(sc, RT2573_EEPROM_RSSI_5GHZ_OFFSET, &val, 2);
	val = le16toh(val);
	if ((val & 0xff) != 0xff)
		sc->rssi_5ghz_corr = (int8_t)(val & 0xff);	/* signed */

	/* Only [-10, 10] is valid */
	if (sc->rssi_5ghz_corr < -10 || sc->rssi_5ghz_corr > 10)
		sc->rssi_5ghz_corr = 0;

	if (sc->ext_2ghz_lna)
		sc->rssi_2ghz_corr -= 14;
	if (sc->ext_5ghz_lna)
		sc->rssi_5ghz_corr -= 14;

	DPRINTF(("RSSI 2GHz corr=%d\nRSSI 5GHz corr=%d\n",
	    sc->rssi_2ghz_corr, sc->rssi_5ghz_corr));

	rum_eeprom_read(sc, RT2573_EEPROM_FREQ_OFFSET, &val, 2);
	val = le16toh(val);
	if ((val & 0xff) != 0xff)
		sc->rffreq = val & 0xff;

	DPRINTF(("RF freq=%d\n", sc->rffreq));

	/* read Tx power for all a/b/g channels */
	rum_eeprom_read(sc, RT2573_EEPROM_TXPOWER, sc->txpow, 14);
	/* XXX default Tx power for 802.11a channels */
	memset(sc->txpow + 14, 24, sizeof (sc->txpow) - 14);
#ifdef RUM_DEBUG
	for (i = 0; i < 14; i++)
		DPRINTF(("Channel=%d Tx power=%d\n", i + 1,  sc->txpow[i]));
#endif

	/* read default values for BBP registers */
	rum_eeprom_read(sc, RT2573_EEPROM_BBP_BASE, sc->bbp_prom, 2 * 16);
#ifdef RUM_DEBUG
	for (i = 0; i < 14; i++) {
		if (sc->bbp_prom[i].reg == 0 || sc->bbp_prom[i].reg == 0xff)
			continue;
		DPRINTF(("BBP R%d=%02x\n", sc->bbp_prom[i].reg,
		    sc->bbp_prom[i].val));
	}
#endif
}

static int
rum_bbp_init(struct rum_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		const uint8_t val = rum_bbp_read(sc, 0);
		if (val != 0 && val != 0xff)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for BBP\n");
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(rum_def_bbp); i++)
		rum_bbp_write(sc, rum_def_bbp[i].reg, rum_def_bbp[i].val);

	/* write vendor-specific BBP values (from EEPROM) */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0 || sc->bbp_prom[i].reg == 0xff)
			continue;
		rum_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}

	return 0;
#undef N
}

static void
rum_init(void *priv)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct rum_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rum_rx_data *data;
	uint32_t tmp;
	usbd_status error;
	int i, ntries;

	rum_stop(sc);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(rum_def_mac); i++)
		rum_write(sc, rum_def_mac[i].reg, rum_def_mac[i].val);

	/* set host ready */
	rum_write(sc, RT2573_MAC_CSR1, 3);
	rum_write(sc, RT2573_MAC_CSR1, 0);

	/* wait for BBP/RF to wakeup */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rum_read(sc, RT2573_MAC_CSR12) & 8)
			break;
		rum_write(sc, RT2573_MAC_CSR12, 4);	/* force wakeup */
		DELAY(1000);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for BBP/RF to wakeup\n",
		    device_get_nameunit(sc->sc_dev));
		goto fail;
	}

	if ((error = rum_bbp_init(sc)) != 0)
		goto fail;

	/* select default channel */
	rum_select_band(sc, ic->ic_curchan);
	rum_select_antenna(sc);
	rum_set_chan(sc, ic->ic_curchan);

	/* clear STA registers */
	rum_read_multi(sc, RT2573_STA_CSR0, sc->sta, sizeof sc->sta);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	rum_set_macaddr(sc, ic->ic_myaddr);

	/* initialize ASIC */
	rum_write(sc, RT2573_MAC_CSR1, 4);

	/*
	 * Allocate xfer for AMRR statistics requests.
	 */
	sc->amrr_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->amrr_xfer == NULL) {
		printf("%s: could not allocate AMRR xfer\n",
		    device_get_nameunit(sc->sc_dev));
		goto fail;
	}

	/*
	 * Open Tx and Rx USB bulk pipes.
	 */
	error = usbd_open_pipe(sc->sc_iface, sc->sc_tx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != 0) {
		printf("%s: could not open Tx pipe: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}
	error = usbd_open_pipe(sc->sc_iface, sc->sc_rx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_rx_pipeh);
	if (error != 0) {
		printf("%s: could not open Rx pipe: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	error = rum_alloc_tx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Tx list\n",
		    device_get_nameunit(sc->sc_dev));
		goto fail;
	}
	error = rum_alloc_rx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Rx list\n",
		    device_get_nameunit(sc->sc_dev));
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < RUM_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->sc_rx_pipeh, data, data->buf,
		    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rum_rxeof);
		usbd_transfer(data->xfer);
	}

	/* update Rx filter */
	tmp = rum_read(sc, RT2573_TXRX_CSR0) & 0xffff;

	tmp |= RT2573_DROP_PHY_ERROR | RT2573_DROP_CRC_ERROR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2573_DROP_CTL | RT2573_DROP_VER_ERROR |
		       RT2573_DROP_ACKCTS;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RT2573_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RT2573_DROP_NOT_TO_ME;
	}
	rum_write(sc, RT2573_TXRX_CSR0, tmp);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	return;

fail:	rum_stop(sc);
#undef N
}

static void
rum_stop(void *priv)
{
	struct rum_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	uint32_t tmp;

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* disable Rx */
	tmp = rum_read(sc, RT2573_TXRX_CSR0);
	rum_write(sc, RT2573_TXRX_CSR0, tmp | RT2573_DISABLE_RX);

	/* reset ASIC */
	rum_write(sc, RT2573_MAC_CSR1, 3);
	rum_write(sc, RT2573_MAC_CSR1, 0);

	if (sc->amrr_xfer != NULL) {
		usbd_free_xfer(sc->amrr_xfer);
		sc->amrr_xfer = NULL;
	}

	if (sc->sc_rx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_rx_pipeh);
		usbd_close_pipe(sc->sc_rx_pipeh);
		sc->sc_rx_pipeh = NULL;
	}
	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
		sc->sc_tx_pipeh = NULL;
	}

	rum_free_rx_list(sc);
	rum_free_tx_list(sc);
}

static int
rum_load_microcode(struct rum_softc *sc, const u_char *ucode, size_t size)
{
	usb_device_request_t req;
	uint16_t reg = RT2573_MCU_CODE_BASE;
	usbd_status error;

	/* copy firmware image into NIC */
	for (; size >= 4; reg += 4, ucode += 4, size -= 4)
		rum_write(sc, reg, UGETDW(ucode));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2573_MCU_CNTL;
	USETW(req.wValue, RT2573_MCU_RUN);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	error = usbd_do_request(sc->sc_udev, &req, NULL);
	if (error != 0) {
		printf("%s: could not run firmware: %s\n",
		    device_get_nameunit(sc->sc_dev), usbd_errstr(error));
	}
	return error;
}

static int
rum_prepare_beacon(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rum_tx_desc desc;
	struct mbuf *m0;
	int rate;

	m0 = ieee80211_beacon_alloc(ic, ic->ic_bss, &sc->sc_bo);
	if (m0 == NULL) {
		return ENOBUFS;
	}

	/* send beacons at the lowest available rate */
	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;

	rum_setup_tx_desc(sc, &desc, RT2573_TX_TIMESTAMP, RT2573_TX_HWSEQ,
	    m0->m_pkthdr.len, rate);

	/* copy the first 24 bytes of Tx descriptor into NIC memory */
	rum_write_multi(sc, RT2573_HW_BEACON_BASE0, (uint8_t *)&desc, 24);

	/* copy beacon header and payload into NIC memory */
	rum_write_multi(sc, RT2573_HW_BEACON_BASE0 + 24, mtod(m0, uint8_t *),
	    m0->m_pkthdr.len);

	m_freem(m0);

	return 0;
}

static int
rum_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct rum_softc *sc = ifp->if_softc;

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}
	if (sc->tx_queued >= RUM_TX_LIST_COUNT) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		m_freem(m);
		ieee80211_free_node(ni);
		return EIO;
	}

	if (bpf_peers_present(ic->ic_rawbpf))
		bpf_mtap(ic->ic_rawbpf, m);

	ifp->if_opackets++;

	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		if (rum_tx_mgt(sc, m, ni) != 0)
			goto bad;
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		if (rum_tx_raw(sc, m, ni, params) != 0)
			goto bad;
	}
	sc->sc_tx_timer = 5;
	callout_reset(&sc->watchdog_ch, hz, rum_watchdog, sc);

	return 0;
bad:
	ifp->if_oerrors++;
	ieee80211_free_node(ni);
	return EIO;
}

static void
rum_amrr_start(struct rum_softc *sc, struct ieee80211_node *ni)
{
	int i;

	/* clear statistic registers (STA_CSR0 to STA_CSR5) */
	rum_read_multi(sc, RT2573_STA_CSR0, sc->sta, sizeof sc->sta);

	ieee80211_amrr_node_init(&sc->amrr, &sc->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;

	callout_reset(&sc->amrr_ch, hz, rum_amrr_timeout, sc);
}

static void
rum_amrr_timeout(void *arg)
{
	struct rum_softc *sc = (struct rum_softc *)arg;
	usb_device_request_t req;

	/*
	 * Asynchronously read statistic registers (cleared by read).
	 */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, RT2573_STA_CSR0);
	USETW(req.wLength, sizeof sc->sta);

	usbd_setup_default_xfer(sc->amrr_xfer, sc->sc_udev, sc,
	    USBD_DEFAULT_TIMEOUT, &req, sc->sta, sizeof sc->sta, 0,
	    rum_amrr_update);
	(void)usbd_transfer(sc->amrr_xfer);
}

static void
rum_amrr_update(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct rum_softc *sc = (struct rum_softc *)priv;
	struct ifnet *ifp = sc->sc_ic.ic_ifp;

	if (status != USBD_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "could not retrieve Tx statistics - "
		    "cancelling automatic rate control\n");
		return;
	}

	/* count TX retry-fail as Tx errors */
	ifp->if_oerrors += le32toh(sc->sta[5]) >> 16;

	sc->amn.amn_retrycnt =
	    (le32toh(sc->sta[4]) >> 16) +	/* TX one-retry ok count */
	    (le32toh(sc->sta[5]) & 0xffff) +	/* TX more-retry ok count */
	    (le32toh(sc->sta[5]) >> 16);	/* TX retry-fail count */

	sc->amn.amn_txcnt =
	    sc->amn.amn_retrycnt +
	    (le32toh(sc->sta[4]) & 0xffff);	/* TX no-retry ok count */

	ieee80211_amrr_choose(&sc->amrr, sc->sc_ic.ic_bss, &sc->amn);

	callout_reset(&sc->amrr_ch, hz, rum_amrr_timeout, sc);
}

static void
rum_scan_start(struct ieee80211com *ic)
{
	struct rum_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = RUM_SCAN_START;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
rum_scan_end(struct ieee80211com *ic)
{
	struct rum_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = RUM_SCAN_END;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
rum_set_channel(struct ieee80211com *ic)
{
	struct rum_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = RUM_SET_CHANNEL;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
rum_scantask(void *arg)
{
	struct rum_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	uint32_t tmp;

	RUM_LOCK(sc);

	switch (sc->sc_scan_action) {
	case RUM_SCAN_START:
		/* abort TSF synchronization */
		tmp = rum_read(sc, RT2573_TXRX_CSR9);
		rum_write(sc, RT2573_TXRX_CSR9, tmp & ~0x00ffffff);
		rum_set_bssid(sc, ifp->if_broadcastaddr);
		break;

	case RUM_SCAN_END:
		rum_enable_tsf_sync(sc);
		/* XXX keep local copy */
		rum_set_bssid(sc, ic->ic_bss->ni_bssid);
		break;

	case RUM_SET_CHANNEL:
		mtx_lock(&Giant);
		rum_set_chan(sc, ic->ic_curchan);
		mtx_unlock(&Giant);
		break;

	default:
		panic("unknown scan action %d\n", sc->sc_scan_action);
		/* NEVER REACHED */
		break;
	}

	RUM_UNLOCK(sc);
}

static int
rum_get_rssi(struct rum_softc *sc, uint8_t raw)
{
	int lna, agc, rssi;

	lna = (raw >> 5) & 0x3;
	agc = raw & 0x1f;

	if (lna == 0) {
		/*
		 * No RSSI mapping
		 *
		 * NB: Since RSSI is relative to noise floor, -1 is
		 *     adequate for caller to know error happened.
		 */
		return -1;
	}

	rssi = (2 * agc) - RT2573_NOISE_FLOOR;

	if (IEEE80211_IS_CHAN_2GHZ(sc->sc_ic.ic_curchan)) {
		rssi += sc->rssi_2ghz_corr;

		if (lna == 1)
			rssi -= 64;
		else if (lna == 2)
			rssi -= 74;
		else if (lna == 3)
			rssi -= 90;
	} else {
		rssi += sc->rssi_5ghz_corr;

		if (!sc->ext_5ghz_lna && lna != 1)
			rssi += 4;

		if (lna == 1)
			rssi -= 64;
		else if (lna == 2)
			rssi -= 86;
		else if (lna == 3)
			rssi -= 100;
	}
	return rssi;
}

DRIVER_MODULE(rum, uhub, rum_driver, rum_devclass, usbd_driver_load, 0);
