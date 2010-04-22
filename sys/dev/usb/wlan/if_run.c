/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2008,2010 Damien Bergamini <damien.bergamini@free.fr>
 * ported to FreeBSD by Akinori Furukoshi <moonlightakkiy@yahoo.ca>
 * USB Consulting, Hans Petter Selasky <hselasky@freebsd.org>
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
 * Ralink Technology RT2700U/RT2800U/RT3000U chipset driver.
 * http://www.ralinktech.com/
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
#include <sys/systm.h>
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

#define USB_DEBUG_VAR run_debug
#include <dev/usb/usb_debug.h>

#include "if_runreg.h"		/* shared with ral(4) */
#include "if_runvar.h"

#define nitems(_a)      (sizeof((_a)) / sizeof((_a)[0]))

#ifdef	USB_DEBUG
#define RUN_DEBUG
#endif

#ifdef	RUN_DEBUG
int run_debug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, run, CTLFLAG_RW, 0, "USB run");
SYSCTL_INT(_hw_usb_run, OID_AUTO, debug, CTLFLAG_RW, &run_debug, 0,
    "run debug level");
#endif

#define IEEE80211_HAS_ADDR4(wh) \
	(((wh)->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)

static const struct usb_device_id run_devs[] = {
    { USB_VP(USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_RT2770) },
    { USB_VP(USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_RT2870) },
    { USB_VP(USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_RT3070) },
    { USB_VP(USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_RT3071) },
    { USB_VP(USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_RT3072) },
    { USB_VP(USB_VENDOR_ABOCOM2,	USB_PRODUCT_ABOCOM2_RT2870_1) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT2770) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT2870_1) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT2870_2) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT2870_3) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT2870_4) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT2870_5) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT3070) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT3070_1) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT3070_2) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT3070_3) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT3070_4) },
    { USB_VP(USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_RT3070_5) },
    { USB_VP(USB_VENDOR_AIRTIES,	USB_PRODUCT_AIRTIES_RT3070) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT2070) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT2770) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT2870) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT3070) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT3071) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT3072) },
    { USB_VP(USB_VENDOR_ALLWIN,		USB_PRODUCT_ALLWIN_RT3572) },
    { USB_VP(USB_VENDOR_AMIGO,		USB_PRODUCT_AMIGO_RT2870_1) },
    { USB_VP(USB_VENDOR_AMIGO,		USB_PRODUCT_AMIGO_RT2870_2) },
    { USB_VP(USB_VENDOR_AMIT,		USB_PRODUCT_AMIT_CGWLUSB2GNR) },
    { USB_VP(USB_VENDOR_AMIT,		USB_PRODUCT_AMIT_RT2870_1) },
    { USB_VP(USB_VENDOR_AMIT2,		USB_PRODUCT_AMIT2_RT2870) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2870_1) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2870_2) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2870_3) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2870_4) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2870_5) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_USBN13) },
    { USB_VP(USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT3070_1) },
    { USB_VP(USB_VENDOR_ASUS2,		USB_PRODUCT_ASUS2_USBN11) },
    { USB_VP(USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RT2870_1) },
    { USB_VP(USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RT2870_2) },
    { USB_VP(USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RT3070_1) },
    { USB_VP(USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RT3070_2) },
    { USB_VP(USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RT3070_3) },
    { USB_VP(USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D8053V3) },
    { USB_VP(USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D8055) },
    { USB_VP(USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F6D4050V1) },
    { USB_VP(USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_RT2870_1) },
    { USB_VP(USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_RT2870_2) },
    { USB_VP(USB_VENDOR_CISCOLINKSYS2,	USB_PRODUCT_CISCOLINKSYS2_RT3070) },
    { USB_VP(USB_VENDOR_CISCOLINKSYS3,	USB_PRODUCT_CISCOLINKSYS2_RT3070) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_1) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_2) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_3) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_4) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_5) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_6) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_7) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT2870_8) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT3070_1) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_RT3070_2) },
    { USB_VP(USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_VIGORN61) },
    { USB_VP(USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_CGWLUSB300GNM) },
    { USB_VP(USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_RT2870_1) },
    { USB_VP(USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_RT2870_2) },
    { USB_VP(USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_RT2870_3) },
    { USB_VP(USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_RT3070) },
    { USB_VP(USB_VENDOR_CYBERTAN,	USB_PRODUCT_CYBERTAN_RT2870) },
    { USB_VP(USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_RT2870) },
    { USB_VP(USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_RT3072) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_DWA130) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT2870_1) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT2870_2) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3070_1) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3070_2) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3070_3) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3070_4) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3070_5) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3072) },
    { USB_VP(USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_RT3072_1) },
    { USB_VP(USB_VENDOR_EDIMAX,		USB_PRODUCT_EDIMAX_EW7717) },
    { USB_VP(USB_VENDOR_EDIMAX,		USB_PRODUCT_EDIMAX_EW7718) },
    { USB_VP(USB_VENDOR_EDIMAX,		USB_PRODUCT_EDIMAX_RT2870_1) },
    { USB_VP(USB_VENDOR_ENCORE,		USB_PRODUCT_ENCORE_RT3070_1) },
    { USB_VP(USB_VENDOR_ENCORE,		USB_PRODUCT_ENCORE_RT3070_2) },
    { USB_VP(USB_VENDOR_ENCORE,		USB_PRODUCT_ENCORE_RT3070_3) },
    { USB_VP(USB_VENDOR_GIGABYTE,	USB_PRODUCT_GIGABYTE_GNWB31N) },
    { USB_VP(USB_VENDOR_GIGABYTE,	USB_PRODUCT_GIGABYTE_GNWB32L) },
    { USB_VP(USB_VENDOR_GIGABYTE,	USB_PRODUCT_GIGABYTE_RT2870_1) },
    { USB_VP(USB_VENDOR_GIGASET,	USB_PRODUCT_GIGASET_RT3070_1) },
    { USB_VP(USB_VENDOR_GIGASET,	USB_PRODUCT_GIGASET_RT3070_2) },
    { USB_VP(USB_VENDOR_GUILLEMOT,	USB_PRODUCT_GUILLEMOT_HWNU300) },
    { USB_VP(USB_VENDOR_HAWKING,	USB_PRODUCT_HAWKING_HWUN2) },
    { USB_VP(USB_VENDOR_HAWKING,	USB_PRODUCT_HAWKING_RT2870_1) },
    { USB_VP(USB_VENDOR_HAWKING,	USB_PRODUCT_HAWKING_RT2870_2) },
    { USB_VP(USB_VENDOR_HAWKING,	USB_PRODUCT_HAWKING_RT3070) },
    { USB_VP(USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_RT3072_1) },
    { USB_VP(USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_RT3072_2) },
    { USB_VP(USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_RT3072_3) },
    { USB_VP(USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_RT3072_4) },
    { USB_VP(USB_VENDOR_LINKSYS4,	USB_PRODUCT_LINKSYS4_RT3070) },
    { USB_VP(USB_VENDOR_LINKSYS4,	USB_PRODUCT_LINKSYS4_WUSB100) },
    { USB_VP(USB_VENDOR_LINKSYS4,	USB_PRODUCT_LINKSYS4_WUSB54GCV3) },
    { USB_VP(USB_VENDOR_LINKSYS4,	USB_PRODUCT_LINKSYS4_WUSB600N) },
    { USB_VP(USB_VENDOR_LINKSYS4,	USB_PRODUCT_LINKSYS4_WUSB600NV2) },
    { USB_VP(USB_VENDOR_LOGITEC,	USB_PRODUCT_LOGITEC_RT2870_1) },
    { USB_VP(USB_VENDOR_LOGITEC,	USB_PRODUCT_LOGITEC_RT2870_2) },
    { USB_VP(USB_VENDOR_LOGITEC,	USB_PRODUCT_LOGITEC_RT2870_3) },
    { USB_VP(USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_RT2870_1) },
    { USB_VP(USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_RT2870_2) },
    { USB_VP(USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_WLIUCAG300N) },
    { USB_VP(USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_WLIUCG300N) },
    { USB_VP(USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_WLIUCGN) },
    { USB_VP(USB_VENDOR_MOTOROLA4,	USB_PRODUCT_MOTOROLA4_RT2770) },
    { USB_VP(USB_VENDOR_MOTOROLA4,	USB_PRODUCT_MOTOROLA4_RT3070) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_1) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_2) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_3) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_4) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_5) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_6) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_7) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_8) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_9) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_10) },
    { USB_VP(USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT3070_11) },
    { USB_VP(USB_VENDOR_OVISLINK,	USB_PRODUCT_OVISLINK_RT3072) },
    { USB_VP(USB_VENDOR_PARA,		USB_PRODUCT_PARA_RT3070) },
    { USB_VP(USB_VENDOR_PEGATRON,	USB_PRODUCT_PEGATRON_RT2870) },
    { USB_VP(USB_VENDOR_PEGATRON,	USB_PRODUCT_PEGATRON_RT3070) },
    { USB_VP(USB_VENDOR_PEGATRON,	USB_PRODUCT_PEGATRON_RT3070_2) },
    { USB_VP(USB_VENDOR_PEGATRON,	USB_PRODUCT_PEGATRON_RT3070_3) },
    { USB_VP(USB_VENDOR_PHILIPS,	USB_PRODUCT_PHILIPS_RT2870) },
    { USB_VP(USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_GWUS300MINIS) },
    { USB_VP(USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_GWUSMICRON) },
    { USB_VP(USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_RT2870) },
    { USB_VP(USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_RT3070) },
    { USB_VP(USB_VENDOR_QCOM,		USB_PRODUCT_QCOM_RT2870) },
    { USB_VP(USB_VENDOR_QUANTA,		USB_PRODUCT_QUANTA_RT3070) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2070) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2770) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2870) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT3070) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT3071) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT3072) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT3370) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT3572) },
    { USB_VP(USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT8070) },
    { USB_VP(USB_VENDOR_SAMSUNG2,	USB_PRODUCT_SAMSUNG2_RT2870_1) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT2870_1) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT2870_2) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT2870_3) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT2870_4) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3070) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3071) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3072_1) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3072_2) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3072_3) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3072_4) },
    { USB_VP(USB_VENDOR_SENAO,		USB_PRODUCT_SENAO_RT3072_5) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT2770) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT2870_1) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT2870_2) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT2870_3) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT2870_4) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3070) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3070_2) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3070_3) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3070_4) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3071) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3072_1) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3072_2) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3072_3) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3072_4) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3072_5) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RT3072_6) },
    { USB_VP(USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_WL608) },
    { USB_VP(USB_VENDOR_SPARKLAN,	USB_PRODUCT_SPARKLAN_RT2870_1) },
    { USB_VP(USB_VENDOR_SPARKLAN,	USB_PRODUCT_SPARKLAN_RT3070) },
    { USB_VP(USB_VENDOR_SWEEX2,		USB_PRODUCT_SWEEX2_LW153) },
    { USB_VP(USB_VENDOR_SWEEX2,		USB_PRODUCT_SWEEX2_LW303) },
    { USB_VP(USB_VENDOR_SWEEX2,		USB_PRODUCT_SWEEX2_LW313) },
    { USB_VP(USB_VENDOR_TOSHIBA,	USB_PRODUCT_TOSHIBA_RT3070) },
    { USB_VP(USB_VENDOR_UMEDIA,		USB_PRODUCT_UMEDIA_RT2870_1) },
    { USB_VP(USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_RT2870_1) },
    { USB_VP(USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_RT2870_2) },
    { USB_VP(USB_VENDOR_ZINWELL,	USB_PRODUCT_ZINWELL_RT2870_1) },
    { USB_VP(USB_VENDOR_ZINWELL,	USB_PRODUCT_ZINWELL_RT2870_2) },
    { USB_VP(USB_VENDOR_ZINWELL,	USB_PRODUCT_ZINWELL_RT3070) },
    { USB_VP(USB_VENDOR_ZINWELL,	USB_PRODUCT_ZINWELL_RT3072_1) },
    { USB_VP(USB_VENDOR_ZINWELL,	USB_PRODUCT_ZINWELL_RT3072_2) },
    { USB_VP(USB_VENDOR_ZYXEL,		USB_PRODUCT_ZYXEL_RT2870_1) },
    { USB_VP(USB_VENDOR_ZYXEL,		USB_PRODUCT_ZYXEL_RT2870_2) },
};

MODULE_DEPEND(run, wlan, 1, 1, 1);
MODULE_DEPEND(run, usb, 1, 1, 1);
MODULE_DEPEND(run, firmware, 1, 1, 1);

static device_probe_t	run_match;
static device_attach_t	run_attach;
static device_detach_t	run_detach;

static usb_callback_t	run_bulk_rx_callback;
static usb_callback_t	run_bulk_tx_callback0;
static usb_callback_t	run_bulk_tx_callback1;
static usb_callback_t	run_bulk_tx_callback2;
static usb_callback_t	run_bulk_tx_callback3;
static usb_callback_t	run_bulk_tx_callback4;
static usb_callback_t	run_bulk_tx_callback5;

static void	run_bulk_tx_callbackN(struct usb_xfer *xfer,
		    usb_error_t error, unsigned int index);
static struct ieee80211vap *run_vap_create(struct ieee80211com *,
		    const char name[IFNAMSIZ], int unit, int opmode, int flags,
		    const uint8_t bssid[IEEE80211_ADDR_LEN], const uint8_t
		    mac[IEEE80211_ADDR_LEN]);
static void	run_vap_delete(struct ieee80211vap *);
static void	run_setup_tx_list(struct run_softc *,
		    struct run_endpoint_queue *);
static void	run_unsetup_tx_list(struct run_softc *,
		    struct run_endpoint_queue *);
static int	run_load_microcode(struct run_softc *);
static int	run_reset(struct run_softc *);
static usb_error_t run_do_request(struct run_softc *,
		    struct usb_device_request *, void *);
static int	run_read(struct run_softc *, uint16_t, uint32_t *);
static int	run_read_region_1(struct run_softc *, uint16_t, uint8_t *, int);
static int	run_write_2(struct run_softc *, uint16_t, uint16_t);
static int	run_write(struct run_softc *, uint16_t, uint32_t);
static int	run_write_region_1(struct run_softc *, uint16_t,
		    const uint8_t *, int);
static int	run_set_region_4(struct run_softc *, uint16_t, uint32_t, int);
static int	run_efuse_read_2(struct run_softc *, uint16_t, uint16_t *);
static int	run_eeprom_read_2(struct run_softc *, uint16_t, uint16_t *);
static int	run_rt2870_rf_write(struct run_softc *, uint8_t, uint32_t);
static int	run_rt3070_rf_read(struct run_softc *, uint8_t, uint8_t *);
static int	run_rt3070_rf_write(struct run_softc *, uint8_t, uint8_t);
static int	run_bbp_read(struct run_softc *, uint8_t, uint8_t *);
static int	run_bbp_write(struct run_softc *, uint8_t, uint8_t);
static int	run_mcu_cmd(struct run_softc *, uint8_t, uint16_t);
static const char *run_get_rf(int);
static int	run_read_eeprom(struct run_softc *);
static struct ieee80211_node *run_node_alloc(struct ieee80211vap *,
			    const uint8_t mac[IEEE80211_ADDR_LEN]);
static int	run_media_change(struct ifnet *);
static int	run_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static int	run_wme_update(struct ieee80211com *);
static void	run_wme_update_cb(void *, int);
static void	run_key_update_begin(struct ieee80211vap *);
static void	run_key_update_end(struct ieee80211vap *);
static int	run_key_set(struct ieee80211vap *, const struct ieee80211_key *,
			    const uint8_t mac[IEEE80211_ADDR_LEN]);
static int	run_key_delete(struct ieee80211vap *,
		    const struct ieee80211_key *);
static void	run_ratectl_start(struct run_softc *, struct ieee80211_node *);
static void	run_ratectl_to(void *);
static void	run_ratectl_cb(void *, int);
static void	run_iter_func(void *, struct ieee80211_node *);
static void	run_newassoc(struct ieee80211_node *, int);
static void	run_rx_frame(struct run_softc *, struct mbuf *, uint32_t);
static void	run_tx_free(struct run_endpoint_queue *pq,
		    struct run_tx_data *, int);
static void	run_set_tx_desc(struct run_softc *, struct run_tx_data *,
		    uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
static int	run_tx(struct run_softc *, struct mbuf *,
		    struct ieee80211_node *);
static int	run_tx_mgt(struct run_softc *, struct mbuf *,
		    struct ieee80211_node *);
static int	run_sendprot(struct run_softc *, const struct mbuf *,
		    struct ieee80211_node *, int, int);
static int	run_tx_param(struct run_softc *, struct mbuf *,
		    struct ieee80211_node *,
		    const struct ieee80211_bpf_params *);
static int	run_raw_xmit(struct ieee80211_node *, struct mbuf *,
		    const struct ieee80211_bpf_params *);
static void	run_start(struct ifnet *);
static int	run_ioctl(struct ifnet *, u_long, caddr_t);
static void	run_set_agc(struct run_softc *, uint8_t);
static void	run_select_chan_group(struct run_softc *, int);
static void	run_set_rx_antenna(struct run_softc *, int);
static void	run_rt2870_set_chan(struct run_softc *, u_int);
static void	run_rt3070_set_chan(struct run_softc *, u_int);
static void	run_rt3572_set_chan(struct run_softc *, u_int);
static int	run_set_chan(struct run_softc *, struct ieee80211_channel *);
static void	run_set_channel(struct ieee80211com *);
static void	run_scan_start(struct ieee80211com *);
static void	run_scan_end(struct ieee80211com *);
static uint8_t	run_rate2mcs(uint8_t);
static void	run_update_beacon(struct ieee80211vap *, int);
static void	run_update_beacon_locked(struct ieee80211vap *, int);
static void	run_updateprot(struct ieee80211com *);
static void	run_usb_timeout_cb(void *, int);
static void	run_reset_livelock(struct run_softc *);
static void	run_enable_tsf_sync(struct run_softc *);
static void	run_enable_mrr(struct run_softc *);
static void	run_set_txpreamble(struct run_softc *);
static void	run_set_basicrates(struct run_softc *);
static void	run_set_leds(struct run_softc *, uint16_t);
static void	run_set_bssid(struct run_softc *, const uint8_t *);
static void	run_set_macaddr(struct run_softc *, const uint8_t *);
static void	run_updateslot(struct ifnet *);
static int8_t	run_rssi2dbm(struct run_softc *, uint8_t, uint8_t);
static void	run_update_promisc_locked(struct ifnet *);
static void	run_update_promisc(struct ifnet *);
static int	run_bbp_init(struct run_softc *);
static int	run_rt3070_rf_init(struct run_softc *);
static int	run_rt3070_filter_calib(struct run_softc *, uint8_t, uint8_t,
		    uint8_t *);
static void	run_rt3070_rf_setup(struct run_softc *);
static int	run_txrx_enable(struct run_softc *);
static void	run_init(void *);
static void	run_init_locked(struct run_softc *);
static void	run_stop(void *);
static void	run_delay(struct run_softc *, unsigned int);

static const struct {
	uint32_t	reg;
	uint32_t	val;
} rt2870_def_mac[] = {
	RT2870_DEF_MAC
};

static const struct {
	uint8_t	reg;
	uint8_t	val;
} rt2860_def_bbp[] = {
	RT2860_DEF_BBP
};

static const struct rfprog {
	uint8_t		chan;
	uint32_t	r1, r2, r3, r4;
} rt2860_rf2850[] = {
	RT2860_RF2850
};

struct {
	uint8_t	n, r, k;
} rt3070_freqs[] = {
	RT3070_RF3052
};

static const struct {
	uint8_t	reg;
	uint8_t	val;
} rt3070_def_rf[] = {
	RT3070_DEF_RF
},rt3572_def_rf[] = {
	RT3572_DEF_RF
};

static const struct usb_config run_config[RUN_N_XFER] = {
    [RUN_BULK_TX_BE] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.ep_index = 0,
	.direction = UE_DIR_OUT,
	.bufsize = RUN_MAX_TXSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
	.callback = run_bulk_tx_callback0,
	.timeout = 5000,	/* ms */
    },
    [RUN_BULK_TX_BK] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.ep_index = 1,
	.bufsize = RUN_MAX_TXSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
	.callback = run_bulk_tx_callback1,
	.timeout = 5000,	/* ms */
    },
    [RUN_BULK_TX_VI] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.ep_index = 2,
	.bufsize = RUN_MAX_TXSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
	.callback = run_bulk_tx_callback2,
	.timeout = 5000,	/* ms */
    },
    [RUN_BULK_TX_VO] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.ep_index = 3,
	.bufsize = RUN_MAX_TXSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
	.callback = run_bulk_tx_callback3,
	.timeout = 5000,	/* ms */
    },
    [RUN_BULK_TX_HCCA] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.ep_index = 4,
	.bufsize = RUN_MAX_TXSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,.no_pipe_ok = 1,},
	.callback = run_bulk_tx_callback4,
	.timeout = 5000,	/* ms */
    },
    [RUN_BULK_TX_PRIO] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_OUT,
	.ep_index = 5,
	.bufsize = RUN_MAX_TXSZ,
	.flags = {.pipe_bof = 1,.force_short_xfer = 1,.no_pipe_ok = 1,},
	.callback = run_bulk_tx_callback5,
	.timeout = 5000,	/* ms */
    },
    [RUN_BULK_RX] = {
	.type = UE_BULK,
	.endpoint = UE_ADDR_ANY,
	.direction = UE_DIR_IN,
	.bufsize = RUN_MAX_RXSZ,
	.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
	.callback = run_bulk_rx_callback,
    }
};

int
run_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != 0)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != RT2860_IFACE_INDEX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(run_devs, sizeof(run_devs), uaa));
}

static int
run_attach(device_t self)
{
	struct run_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct ieee80211com *ic;
	struct ifnet *ifp;
	uint32_t ver;
	int i, ntries, error;
	uint8_t iface_index, bands;

	device_set_usb_desc(self);
	sc->sc_udev = uaa->device;
	sc->sc_dev = self;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	iface_index = RT2860_IFACE_INDEX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, run_config, RUN_N_XFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(self, "could not allocate USB transfers, "
		    "err=%s\n", usbd_errstr(error));
		goto detach;
	}

	RUN_LOCK(sc);

	/* wait for the chip to settle */
	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_ASIC_VER_ID, &ver) != 0){
			RUN_UNLOCK(sc);
			goto detach;
		}
		if (ver != 0 && ver != 0xffffffff)
			break;
		run_delay(sc, 10);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "timeout waiting for NIC to initialize\n");
		RUN_UNLOCK(sc);
		goto detach;
	}
	sc->mac_ver = ver >> 16;
	sc->mac_rev = ver & 0xffff;

	/* retrieve RF rev. no and various other things from EEPROM */
	run_read_eeprom(sc);

	device_printf(sc->sc_dev,
	    "MAC/BBP RT%04X (rev 0x%04X), RF %s (MIMO %dT%dR), address %s\n",
	    sc->mac_ver, sc->mac_rev, run_get_rf(sc->rf_rev),
	    sc->ntxchains, sc->nrxchains, ether_sprintf(sc->sc_bssid));

	if ((error = run_load_microcode(sc)) != 0) {
		device_printf(sc->sc_dev, "could not load 8051 microcode\n");
		RUN_UNLOCK(sc);
		goto detach;
	}

	RUN_UNLOCK(sc);

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if(ifp == NULL){
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		goto detach;
	}
	ic = ifp->if_l2com;

	ifp->if_softc = sc;
	if_initname(ifp, "run", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = run_init;
	ifp->if_ioctl = run_ioctl;
	ifp->if_start = run_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
#if 0
	ic->ic_state = IEEE80211_S_INIT;
#endif
	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_STA |		/* station mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP |
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WME |		/* WME */
	    IEEE80211_C_WPA;		/* WPA1|WPA2(RSN) */

	ic->ic_cryptocaps =
	    IEEE80211_CRYPTO_WEP |
	    IEEE80211_CRYPTO_AES_CCM |
	    IEEE80211_CRYPTO_TKIPMIC |
	    IEEE80211_CRYPTO_TKIP;

	ic->ic_flags |= IEEE80211_F_DATAPAD;
	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, NULL, &bands);

	/*
	 * Do this by own because h/w supports
	 * more channels than ieee80211_init_channels()
	 */
	if (sc->rf_rev == RT2860_RF_2750 ||
	    sc->rf_rev == RT2860_RF_2850 ||
	    sc->rf_rev == RT3070_RF_3052) {
		/* set supported .11a rates */
		for (i = 14; i < nitems(rt2860_rf2850); i++) {
			uint8_t chan = rt2860_rf2850[i].chan;
			ic->ic_channels[ic->ic_nchans].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_A);
			ic->ic_channels[ic->ic_nchans].ic_ieee = chan;
			ic->ic_channels[ic->ic_nchans].ic_flags = IEEE80211_CHAN_A;
			ic->ic_channels[ic->ic_nchans].ic_extieee = 0;
			ic->ic_nchans++;
		}
	}

	ieee80211_ifattach(ic, sc->sc_bssid);

	ic->ic_scan_start = run_scan_start;
	ic->ic_scan_end = run_scan_end;
	ic->ic_set_channel = run_set_channel;
	ic->ic_node_alloc = run_node_alloc;
	ic->ic_newassoc = run_newassoc;
	//ic->ic_updateslot = run_updateslot;
	ic->ic_wme.wme_update = run_wme_update;
	ic->ic_raw_xmit = run_raw_xmit;
	ic->ic_update_promisc = run_update_promisc;

	ic->ic_vap_create = run_vap_create;
	ic->ic_vap_delete = run_vap_delete;

	ieee80211_radiotap_attach(ic,
	    &sc->sc_txtap.wt_ihdr, sizeof(sc->sc_txtap),
		RUN_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
		RUN_RX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;

detach:
	run_detach(self);
	return(ENXIO);
}

static int
run_detach(device_t self)
{
	struct run_softc *sc = device_get_softc(self);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic;
	int i;

	/* stop all USB transfers */
	usbd_transfer_unsetup(sc->sc_xfer, RUN_N_XFER);

	RUN_LOCK(sc);
	/* free TX list, if any */
	for (i = 0; i != RUN_EP_QUEUES; i++)
		run_unsetup_tx_list(sc, &sc->sc_epq[i]);
	RUN_UNLOCK(sc);

	if (ifp) {
		ic = ifp->if_l2com;
		ieee80211_ifdetach(ic);
		if_free(ifp);
	}

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static struct ieee80211vap *
run_vap_create(struct ieee80211com *ic,
    const char name[IFNAMSIZ], int unit, int opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct run_vap *rvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))         /* only one at a time */
		return NULL;
	sc->sc_rvp = rvp = (struct run_vap *) malloc(sizeof(struct run_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (rvp == NULL)
		return NULL;
	vap = &rvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	vap->iv_key_update_begin = run_key_update_begin;
	vap->iv_key_update_end = run_key_update_end;
	vap->iv_key_delete = run_key_delete;
	vap->iv_key_set = run_key_set;
	vap->iv_update_beacon = run_update_beacon;

	/* override state transition machine */
	rvp->newstate = vap->iv_newstate;
	vap->iv_newstate = run_newstate;

	TASK_INIT(&rvp->ratectl_task, 0, run_ratectl_cb, rvp);
	TASK_INIT(&sc->wme_task, 0, run_wme_update_cb, ic);
	TASK_INIT(&sc->usb_timeout_task, 0, run_usb_timeout_cb, sc);
	callout_init((struct callout *)&rvp->ratectl_ch, 1);
	ieee80211_ratectl_init(vap);
	ieee80211_ratectl_setinterval(vap, 1000 /* 1 sec */);

	/* complete setup */
	ieee80211_vap_attach(vap, run_media_change, ieee80211_media_status);
	ic->ic_opmode = opmode;
	return vap;
}

static void
run_vap_delete(struct ieee80211vap *vap)
{
	struct run_vap *rvp = RUN_VAP(vap);
	struct ifnet *ifp;
	struct ieee80211com *ic;
	struct run_softc *sc;

	if(vap == NULL)
		return;

	ic = vap->iv_ic;
	ifp = ic->ic_ifp;

	sc = ifp->if_softc;

	RUN_LOCK(sc);
	sc->sc_rvp->ratectl_run = RUN_RATECTL_OFF;
	RUN_UNLOCK(sc);

	/* drain them all */
	usb_callout_drain(&sc->sc_rvp->ratectl_ch);
	ieee80211_draintask(ic, &sc->sc_rvp->ratectl_task);
	ieee80211_draintask(ic, &sc->wme_task);
	ieee80211_draintask(ic, &sc->usb_timeout_task);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(rvp, M_80211_VAP);
	sc->sc_rvp = NULL;
}

static void
run_setup_tx_list(struct run_softc *sc, struct run_endpoint_queue *pq)
{
	struct run_tx_data *data;

	memset(pq, 0, sizeof(*pq));

	STAILQ_INIT(&pq->tx_qh);
	STAILQ_INIT(&pq->tx_fh);

	for (data = &pq->tx_data[0];
	    data < &pq->tx_data[RUN_TX_RING_COUNT]; data++) {
		data->sc = sc;
		STAILQ_INSERT_TAIL(&pq->tx_fh, data, next);
	}
	pq->tx_nfree = RUN_TX_RING_COUNT;
}

static void
run_unsetup_tx_list(struct run_softc *sc, struct run_endpoint_queue *pq)
{
	struct run_tx_data *data;

	/* make sure any subsequent use of the queues will fail */
	pq->tx_nfree = 0;
	STAILQ_INIT(&pq->tx_fh);
	STAILQ_INIT(&pq->tx_qh);

	/* free up all node references and mbufs */
	for (data = &pq->tx_data[0];
	    data < &pq->tx_data[RUN_TX_RING_COUNT]; data++){
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

int
run_load_microcode(struct run_softc *sc)
{
	usb_device_request_t req;
	const struct firmware *fw;
	const u_char *base;
	uint32_t tmp;
	int ntries, error;
	const uint64_t *temp;
	uint64_t bytes;

	RUN_UNLOCK(sc);
	fw = firmware_get("runfw");
	RUN_LOCK(sc);
	if(fw == NULL){
		device_printf(sc->sc_dev,
		    "failed loadfirmware of file %s\n", "runfw");
		return ENOENT;
	}

	if (fw->datasize != 8192) {
		device_printf(sc->sc_dev,
		    "invalid firmware size (should be 8KB)\n");
		error = EINVAL;
		goto fail;
	}

	/*
	 * RT3071/RT3072 use a different firmware
	 * run-rt2870 (8KB) contains both,
	 * first half (4KB) is for rt2870,
	 * last half is for rt3071.
	 */
	base = fw->data;
	if ((sc->mac_ver) != 0x2860 &&
	    (sc->mac_ver) != 0x2872 &&
	    (sc->mac_ver) != 0x3070){ 
		base += 4096;
	}

	/* cheap sanity check */
	temp = fw->data;
	bytes = *temp;
	if(bytes != be64toh(0xffffff0210280210)) {
		device_printf(sc->sc_dev, "firmware checksum failed\n");
		error = EINVAL;
		goto fail;
	}

	run_read(sc, RT2860_ASIC_VER_ID, &tmp);
	/* write microcode image */
	run_write_region_1(sc, RT2870_FW_BASE, base, 4096);
	run_write(sc, RT2860_H2M_MAILBOX_CID, 0xffffffff);
	run_write(sc, RT2860_H2M_MAILBOX_STATUS, 0xffffffff);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_RESET;
	USETW(req.wValue, 8);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if ((error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, NULL)) != 0) {
		device_printf(sc->sc_dev, "firmware reset failed\n");
		goto fail;
	}

	run_delay(sc, 10);

	run_write(sc, RT2860_H2M_MAILBOX, 0);
	if ((error = run_mcu_cmd(sc, RT2860_MCU_CMD_RFRESET, 0)) != 0)
		goto fail;

	/* wait until microcontroller is ready */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((error = run_read(sc, RT2860_SYS_CTRL, &tmp)) != 0) {
			goto fail;
		}
		if (tmp & RT2860_MCU_READY)
			break;
		run_delay(sc, 10);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for MCU to initialize\n");
		error = ETIMEDOUT;
		goto fail;
	}
	device_printf(sc->sc_dev, "firmware %s loaded\n",
	    (base == fw->data) ? "RT2870" : "RT3071");

fail:
	firmware_put(fw, FIRMWARE_UNLOAD);
	return (error);
}

int
run_reset(struct run_softc *sc)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_RESET;
	USETW(req.wValue, 1);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, NULL);
}

static usb_error_t
run_do_request(struct run_softc *sc,
    struct usb_device_request *req, void *data)
{
	usb_error_t err;
	int ntries = 10;

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	while (ntries--) {
		err = usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx,
		    req, data, 0, NULL, 250 /* ms */);
		if (err == 0)
			break;
		DPRINTFN(1, "Control request failed, %s (retrying)\n",
		    usbd_errstr(err));
		run_delay(sc, 10);
	}
	return (err);
}

static int
run_read(struct run_softc *sc, uint16_t reg, uint32_t *val)
{
	uint32_t tmp;
	int error;

	error = run_read_region_1(sc, reg, (uint8_t *)&tmp, sizeof tmp);
	if (error == 0)
		*val = le32toh(tmp);
	else
		*val = 0xffffffff;
	return error;
}

static int
run_read_region_1(struct run_softc *sc, uint16_t reg, uint8_t *buf, int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2870_READ_REGION_1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	return run_do_request(sc, &req, buf);
}

static int
run_write_2(struct run_softc *sc, uint16_t reg, uint16_t val)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_WRITE_2;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	return run_do_request(sc, &req, NULL);
}

static int
run_write(struct run_softc *sc, uint16_t reg, uint32_t val)
{
	int error;

	if ((error = run_write_2(sc, reg, val & 0xffff)) == 0)
		error = run_write_2(sc, reg + 2, val >> 16);
	return error;
}

static int
run_write_region_1(struct run_softc *sc, uint16_t reg, const uint8_t *buf,
    int len)
{
#if 1
	int i, error = 0;
	/*
	 * NB: the WRITE_REGION_1 command is not stable on RT2860.
	 * We thus issue multiple WRITE_2 commands instead.
	 */
	KASSERT((len & 1) == 0, ("run_write_region_1: Data too long.\n"));
	for (i = 0; i < len && error == 0; i += 2)
		error = run_write_2(sc, reg + i, buf[i] | buf[i + 1] << 8);
	return error;
#else
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_WRITE_REGION_1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);
	return run_do_request(sc, &req, buf);
#endif
}

static int
run_set_region_4(struct run_softc *sc, uint16_t reg, uint32_t val, int len)
{
	int i, error = 0;

	KASSERT((len & 3) == 0, ("run_set_region_4: Invalid data length.\n"));
	for (i = 0; i < len && error == 0; i += 4)
		error = run_write(sc, reg + i, val);
	return error;
}

/* Read 16-bit from eFUSE ROM (RT3070 only.) */
static int
run_efuse_read_2(struct run_softc *sc, uint16_t addr, uint16_t *val)
{
	uint32_t tmp;
	uint16_t reg;
	int error, ntries;

	if ((error = run_read(sc, RT3070_EFUSE_CTRL, &tmp)) != 0)
		return error;

	addr *= 2;
	/*-
	 * Read one 16-byte block into registers EFUSE_DATA[0-3]:
	 * DATA0: F E D C
	 * DATA1: B A 9 8
	 * DATA2: 7 6 5 4
	 * DATA3: 3 2 1 0
	 */
	tmp &= ~(RT3070_EFSROM_MODE_MASK | RT3070_EFSROM_AIN_MASK);
	tmp |= (addr & ~0xf) << RT3070_EFSROM_AIN_SHIFT | RT3070_EFSROM_KICK;
	run_write(sc, RT3070_EFUSE_CTRL, tmp);
	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT3070_EFUSE_CTRL, &tmp)) != 0)
			return error;
		if (!(tmp & RT3070_EFSROM_KICK))
			break;
		run_delay(sc, 2);
	}
	if (ntries == 100)
		return ETIMEDOUT;

	if ((tmp & RT3070_EFUSE_AOUT_MASK) == RT3070_EFUSE_AOUT_MASK) {
		*val = 0xffff;	/* address not found */
		return 0;
	}
	/* determine to which 32-bit register our 16-bit word belongs */
	reg = RT3070_EFUSE_DATA3 - (addr & 0xc);
	if ((error = run_read(sc, reg, &tmp)) != 0)
		return error;

	*val = (addr & 2) ? tmp >> 16 : tmp & 0xffff;
	return 0;
}

static int
run_eeprom_read_2(struct run_softc *sc, uint16_t addr, uint16_t *val)
{
	usb_device_request_t req;
	uint16_t tmp;
	int error;

	addr *= 2;
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2870_EEPROM_READ;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, sizeof tmp);

	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, &tmp);
	if (error == 0)
		*val = le16toh(tmp);
	else
		*val = 0xffff;
	return error;
}

static __inline int
run_srom_read(struct run_softc *sc, uint16_t addr, uint16_t *val)
{
	/* either eFUSE ROM or EEPROM */
	return sc->sc_srom_read(sc, addr, val);
}

static int
run_rt2870_rf_write(struct run_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_RF_CSR_CFG0, &tmp)) != 0)
			return error;
		if (!(tmp & RT2860_RF_REG_CTRL))
			break;
	}
	if (ntries == 10)
		return ETIMEDOUT;

	/* RF registers are 24-bit on the RT2860 */
	tmp = RT2860_RF_REG_CTRL | 24 << RT2860_RF_REG_WIDTH_SHIFT |
	    (val & 0x3fffff) << 2 | (reg & 3);
	return run_write(sc, RT2860_RF_CSR_CFG0, tmp);
}

static int
run_rt3070_rf_read(struct run_softc *sc, uint8_t reg, uint8_t *val)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT3070_RF_CSR_CFG, &tmp)) != 0)
			return error;
		if (!(tmp & RT3070_RF_KICK))
			break;
	}
	if (ntries == 100)
		return ETIMEDOUT;

	tmp = RT3070_RF_KICK | reg << 8;
	if ((error = run_write(sc, RT3070_RF_CSR_CFG, tmp)) != 0)
		return error;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT3070_RF_CSR_CFG, &tmp)) != 0)
			return error;
		if (!(tmp & RT3070_RF_KICK))
			break;
	}
	if (ntries == 100)
		return ETIMEDOUT;

	*val = tmp & 0xff;
	return 0;
}

static int
run_rt3070_rf_write(struct run_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT3070_RF_CSR_CFG, &tmp)) != 0)
			return error;
		if (!(tmp & RT3070_RF_KICK))
			break;
	}
	if (ntries == 10)
		return ETIMEDOUT;

	tmp = RT3070_RF_WRITE | RT3070_RF_KICK | reg << 8 | val;
	return run_write(sc, RT3070_RF_CSR_CFG, tmp);
}

static int
run_bbp_read(struct run_softc *sc, uint8_t reg, uint8_t *val)
{
	uint32_t tmp;
	int ntries, error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_BBP_CSR_CFG, &tmp)) != 0)
			return error;
		if (!(tmp & RT2860_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return ETIMEDOUT;

	tmp = RT2860_BBP_CSR_READ | RT2860_BBP_CSR_KICK | reg << 8;
	if ((error = run_write(sc, RT2860_BBP_CSR_CFG, tmp)) != 0)
		return error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_BBP_CSR_CFG, &tmp)) != 0)
			return error;
		if (!(tmp & RT2860_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return ETIMEDOUT;

	*val = tmp & 0xff;
	return 0;
}

static int
run_bbp_write(struct run_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries, error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_BBP_CSR_CFG, &tmp)) != 0)
			return error;
		if (!(tmp & RT2860_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return ETIMEDOUT;

	tmp = RT2860_BBP_CSR_KICK | reg << 8 | val;
	return run_write(sc, RT2860_BBP_CSR_CFG, tmp);
}

/*
 * Send a command to the 8051 microcontroller unit.
 */
static int
run_mcu_cmd(struct run_softc *sc, uint8_t cmd, uint16_t arg)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT2860_H2M_MAILBOX, &tmp)) != 0)
			return error;
		if (!(tmp & RT2860_H2M_BUSY))
			break;
	}
	if (ntries == 100)
		return ETIMEDOUT;

	tmp = RT2860_H2M_BUSY | RT2860_TOKEN_NO_INTR << 16 | arg;
	if ((error = run_write(sc, RT2860_H2M_MAILBOX, tmp)) == 0)
		error = run_write(sc, RT2860_HOST_CMD, cmd);
	return error;
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
	return b32;
}

static const char *
run_get_rf(int rev)
{
	switch (rev) {
	case RT2860_RF_2820:	return "RT2820";
	case RT2860_RF_2850:	return "RT2850";
	case RT2860_RF_2720:	return "RT2720";
	case RT2860_RF_2750:	return "RT2750";
	case RT3070_RF_3020:	return "RT3020";
	case RT3070_RF_2020:	return "RT2020";
	case RT3070_RF_3021:	return "RT3021";
	case RT3070_RF_3022:	return "RT3022";
	case RT3070_RF_3052:	return "RT3052";
	}
	return "unknown";
}

int
run_read_eeprom(struct run_softc *sc)
{
	int8_t delta_2ghz, delta_5ghz;
	uint32_t tmp;
	uint16_t val;
	int ridx, ant, i;

	/* check whether the ROM is eFUSE ROM or EEPROM */
	sc->sc_srom_read = run_eeprom_read_2;
	if (sc->mac_ver >= 0x3070) {
		run_read(sc, RT3070_EFUSE_CTRL, &tmp);
		DPRINTF("EFUSE_CTRL=0x%08x\n", tmp);
		if (tmp & RT3070_SEL_EFUSE)
			sc->sc_srom_read = run_efuse_read_2;
	}

	/* read ROM version */
	run_srom_read(sc, RT2860_EEPROM_VERSION, &val);
	DPRINTF("EEPROM rev=%d, FAE=%d\n", val & 0xff, val >> 8);

	/* read MAC address */
	run_srom_read(sc, RT2860_EEPROM_MAC01, &val);
	sc->sc_bssid[0] = val & 0xff;
	sc->sc_bssid[1] = val >> 8;
	run_srom_read(sc, RT2860_EEPROM_MAC23, &val);
	sc->sc_bssid[2] = val & 0xff;
	sc->sc_bssid[3] = val >> 8;
	run_srom_read(sc, RT2860_EEPROM_MAC45, &val);
	sc->sc_bssid[4] = val & 0xff;
	sc->sc_bssid[5] = val >> 8;

	/* read vender BBP settings */
	for (i = 0; i < 10; i++) {
		run_srom_read(sc, RT2860_EEPROM_BBP_BASE + i, &val);
		sc->bbp[i].val = val & 0xff;
		sc->bbp[i].reg = val >> 8;
		DPRINTF("BBP%d=0x%02x\n", sc->bbp[i].reg, sc->bbp[i].val);
	}
	if (sc->mac_ver >= 0x3071) {
		/* read vendor RF settings */
		for (i = 0; i < 10; i++) {
			run_srom_read(sc, RT3071_EEPROM_RF_BASE + i, &val);
			sc->rf[i].val = val & 0xff;
			sc->rf[i].reg = val >> 8;
			DPRINTF("RF%d=0x%02x\n", sc->rf[i].reg,
			    sc->rf[i].val);
		}
	}

	/* read RF frequency offset from EEPROM */
	run_srom_read(sc, RT2860_EEPROM_FREQ_LEDS, &val);
	sc->freq = ((val & 0xff) != 0xff) ? val & 0xff : 0;
	DPRINTF("EEPROM freq offset %d\n", sc->freq & 0xff);

	if (val >> 8 != 0xff) {
		/* read LEDs operating mode */
		sc->leds = val >> 8;
		run_srom_read(sc, RT2860_EEPROM_LED1, &sc->led[0]);
		run_srom_read(sc, RT2860_EEPROM_LED2, &sc->led[1]);
		run_srom_read(sc, RT2860_EEPROM_LED3, &sc->led[2]);
	} else {
		/* broken EEPROM, use default settings */
		sc->leds = 0x01;
		sc->led[0] = 0x5555;
		sc->led[1] = 0x2221;
		sc->led[2] = 0x5627;	/* differs from RT2860 */
	}
	DPRINTF("EEPROM LED mode=0x%02x, LEDs=0x%04x/0x%04x/0x%04x\n",
	    sc->leds, sc->led[0], sc->led[1], sc->led[2]);

	/* read RF information */
	run_srom_read(sc, RT2860_EEPROM_ANTENNA, &val);
	if (val == 0xffff) {
		DPRINTF("invalid EEPROM antenna info, using default\n");
		if (sc->mac_ver == 0x3572) {
			/* default to RF3052 2T2R */
			sc->rf_rev = RT3070_RF_3052;
			sc->ntxchains = 2;
			sc->nrxchains = 2;
		} else if (sc->mac_ver >= 0x3070) {
			/* default to RF3020 1T1R */
			sc->rf_rev = RT3070_RF_3020;
			sc->ntxchains = 1;
			sc->nrxchains = 1;
		} else {
			/* default to RF2820 1T2R */
			sc->rf_rev = RT2860_RF_2820;
			sc->ntxchains = 1;
			sc->nrxchains = 2;
		}
	} else {
		sc->rf_rev = (val >> 8) & 0xf;
		sc->ntxchains = (val >> 4) & 0xf;
		sc->nrxchains = val & 0xf;
	}
	DPRINTF("EEPROM RF rev=0x%02x chains=%dT%dR\n",
	    sc->rf_rev, sc->ntxchains, sc->nrxchains);

	run_srom_read(sc, RT2860_EEPROM_CONFIG, &val);
	DPRINTF("EEPROM CFG 0x%04x\n", val);
	/* check if driver should patch the DAC issue */
	if ((val >> 8) != 0xff)
		sc->patch_dac = (val >> 15) & 1;
	if ((val & 0xff) != 0xff) {
		sc->ext_5ghz_lna = (val >> 3) & 1;
		sc->ext_2ghz_lna = (val >> 2) & 1;
		/* check if RF supports automatic Tx access gain control */
		sc->calib_2ghz = sc->calib_5ghz = (val >> 1) & 1;
		/* check if we have a hardware radio switch */
		sc->rfswitch = val & 1;
	}

	/* read power settings for 2GHz channels */
	for (i = 0; i < 14; i += 2) {
		run_srom_read(sc, RT2860_EEPROM_PWR2GHZ_BASE1 + i / 2, &val);
		sc->txpow1[i + 0] = (int8_t)(val & 0xff);
		sc->txpow1[i + 1] = (int8_t)(val >> 8);

		run_srom_read(sc, RT2860_EEPROM_PWR2GHZ_BASE2 + i / 2, &val);
		sc->txpow2[i + 0] = (int8_t)(val & 0xff);
		sc->txpow2[i + 1] = (int8_t)(val >> 8);
	}
	/* fix broken Tx power entries */
	for (i = 0; i < 14; i++) {
		if (sc->txpow1[i] < 0 || sc->txpow1[i] > 31)
			sc->txpow1[i] = 5;
		if (sc->txpow2[i] < 0 || sc->txpow2[i] > 31)
			sc->txpow2[i] = 5;
		DPRINTF("chan %d: power1=%d, power2=%d\n",
		    rt2860_rf2850[i].chan, sc->txpow1[i], sc->txpow2[i]);
	}
	/* read power settings for 5GHz channels */
	for (i = 0; i < 40; i += 2) {
		run_srom_read(sc, RT2860_EEPROM_PWR5GHZ_BASE1 + i / 2, &val);
		sc->txpow1[i + 14] = (int8_t)(val & 0xff);
		sc->txpow1[i + 15] = (int8_t)(val >> 8);

		run_srom_read(sc, RT2860_EEPROM_PWR5GHZ_BASE2 + i / 2, &val);
		sc->txpow2[i + 14] = (int8_t)(val & 0xff);
		sc->txpow2[i + 15] = (int8_t)(val >> 8);
	}
	/* fix broken Tx power entries */
	for (i = 0; i < 40; i++) {
		if (sc->txpow1[14 + i] < -7 || sc->txpow1[14 + i] > 15)
			sc->txpow1[14 + i] = 5;
		if (sc->txpow2[14 + i] < -7 || sc->txpow2[14 + i] > 15)
			sc->txpow2[14 + i] = 5;
		DPRINTF("chan %d: power1=%d, power2=%d\n",
		    rt2860_rf2850[14 + i].chan, sc->txpow1[14 + i],
		    sc->txpow2[14 + i]);
	}

	/* read Tx power compensation for each Tx rate */
	run_srom_read(sc, RT2860_EEPROM_DELTAPWR, &val);
	delta_2ghz = delta_5ghz = 0;
	if ((val & 0xff) != 0xff && (val & 0x80)) {
		delta_2ghz = val & 0xf;
		if (!(val & 0x40))	/* negative number */
			delta_2ghz = -delta_2ghz;
	}
	val >>= 8;
	if ((val & 0xff) != 0xff && (val & 0x80)) {
		delta_5ghz = val & 0xf;
		if (!(val & 0x40))	/* negative number */
			delta_5ghz = -delta_5ghz;
	}
	DPRINTF("power compensation=%d (2GHz), %d (5GHz)\n",
	    delta_2ghz, delta_5ghz);

	for (ridx = 0; ridx < 5; ridx++) {
		uint32_t reg;

		run_srom_read(sc, RT2860_EEPROM_RPWR + ridx, &val);
		reg = (uint32_t)val << 16;
		run_srom_read(sc, RT2860_EEPROM_RPWR + ridx + 1, &val);
		reg |= val;

		sc->txpow20mhz[ridx] = reg;
		sc->txpow40mhz_2ghz[ridx] = b4inc(reg, delta_2ghz);
		sc->txpow40mhz_5ghz[ridx] = b4inc(reg, delta_5ghz);

		DPRINTF("ridx %d: power 20MHz=0x%08x, 40MHz/2GHz=0x%08x, "
		    "40MHz/5GHz=0x%08x\n", ridx, sc->txpow20mhz[ridx],
		    sc->txpow40mhz_2ghz[ridx], sc->txpow40mhz_5ghz[ridx]);
	}

	/* read RSSI offsets and LNA gains from EEPROM */
	run_srom_read(sc, RT2860_EEPROM_RSSI1_2GHZ, &val);
	sc->rssi_2ghz[0] = val & 0xff;	/* Ant A */
	sc->rssi_2ghz[1] = val >> 8;	/* Ant B */
	run_srom_read(sc, RT2860_EEPROM_RSSI2_2GHZ, &val);
	if (sc->mac_ver >= 0x3070) {
		/*
		 * On RT3070 chips (limited to 2 Rx chains), this ROM
		 * field contains the Tx mixer gain for the 2GHz band.
		 */
		if ((val & 0xff) != 0xff)
			sc->txmixgain_2ghz = val & 0x7;
		DPRINTF("tx mixer gain=%u (2GHz)\n", sc->txmixgain_2ghz);
	} else
		sc->rssi_2ghz[2] = val & 0xff;	/* Ant C */
	sc->lna[2] = val >> 8;		/* channel group 2 */

	run_srom_read(sc, RT2860_EEPROM_RSSI1_5GHZ, &val);
	sc->rssi_5ghz[0] = val & 0xff;	/* Ant A */
	sc->rssi_5ghz[1] = val >> 8;	/* Ant B */
	run_srom_read(sc, RT2860_EEPROM_RSSI2_5GHZ, &val);
	if (sc->mac_ver == 0x3572) {
		/*
		 * On RT3572 chips (limited to 2 Rx chains), this ROM
		 * field contains the Tx mixer gain for the 5GHz band.
		 */
		if ((val & 0xff) != 0xff)
			sc->txmixgain_5ghz = val & 0x7;
		DPRINTF("tx mixer gain=%u (5GHz)\n", sc->txmixgain_5ghz);
	} else
		sc->rssi_5ghz[2] = val & 0xff;	/* Ant C */
	sc->lna[3] = val >> 8;		/* channel group 3 */

	run_srom_read(sc, RT2860_EEPROM_LNA, &val);
	sc->lna[0] = val & 0xff;	/* channel group 0 */
	sc->lna[1] = val >> 8;		/* channel group 1 */

	/* fix broken 5GHz LNA entries */
	if (sc->lna[2] == 0 || sc->lna[2] == 0xff) {
		DPRINTF("invalid LNA for channel group %d\n", 2);
		sc->lna[2] = sc->lna[1];
	}
	if (sc->lna[3] == 0 || sc->lna[3] == 0xff) {
		DPRINTF("invalid LNA for channel group %d\n", 3);
		sc->lna[3] = sc->lna[1];
	}

	/* fix broken RSSI offset entries */
	for (ant = 0; ant < 3; ant++) {
		if (sc->rssi_2ghz[ant] < -10 || sc->rssi_2ghz[ant] > 10) {
			DPRINTF("invalid RSSI%d offset: %d (2GHz)\n",
			    ant + 1, sc->rssi_2ghz[ant]);
			sc->rssi_2ghz[ant] = 0;
		}
		if (sc->rssi_5ghz[ant] < -10 || sc->rssi_5ghz[ant] > 10) {
			DPRINTF("invalid RSSI%d offset: %d (5GHz)\n",
			    ant + 1, sc->rssi_5ghz[ant]);
			sc->rssi_5ghz[ant] = 0;
		}
	}
	return 0;
}

struct ieee80211_node *
run_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	return malloc(sizeof (struct run_node), M_DEVBUF, M_NOWAIT | M_ZERO);
}

static int
run_media_change(struct ifnet *ifp)
{
	const struct ieee80211_txparam *tp;
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211vap *vap = &sc->sc_rvp->vap;
	uint8_t rate, ridx;
	int error;

	RUN_LOCK(sc);

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		RUN_UNLOCK(sc);
		return error;

	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
	if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[tp->ucastrate] & IEEE80211_RATE_VAL;
		for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++)
			if (rt2860_rates[ridx].rate == rate)
				break;
		sc->fixed_ridx = ridx;
	}

	if ((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags &  IFF_DRV_RUNNING)){
		run_init_locked(sc);
	}

	RUN_UNLOCK(sc);

	return 0;
}

static int
run_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	const struct ieee80211_txparam *tp;
	struct ieee80211com *ic = vap->iv_ic;
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct run_vap *rvp = RUN_VAP(vap);
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	uint32_t tmp;
	uint8_t wcid;

	ostate = vap->iv_state;
	DPRINTF("%s -> %s\n",
		ieee80211_state_name[ostate],
		ieee80211_state_name[nstate]);

	IEEE80211_UNLOCK(ic);
	RUN_LOCK(sc);

	sc->sc_rvp->ratectl_run = RUN_RATECTL_OFF;
	usb_callout_stop(&rvp->ratectl_ch);

	if (ostate == IEEE80211_S_RUN) {
		/* turn link LED off */
		run_set_leds(sc, RT2860_LED_RADIO);
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
			run_write(sc, RT2860_BCN_TIME_CFG,
			    tmp & ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN |
			    RT2860_TBTT_TIMER_EN));
		}
		break;

	case IEEE80211_S_RUN:
		ni = vap->iv_bss;

		if (vap->iv_opmode != IEEE80211_M_MONITOR) {
			run_updateslot(ic->ic_ifp);
			run_enable_mrr(sc);
			run_set_txpreamble(sc);
			run_set_basicrates(sc);
			IEEE80211_ADDR_COPY(sc->sc_bssid, ni->ni_bssid);
			run_set_bssid(sc, ni->ni_bssid);
		}

		if (vap->iv_opmode == IEEE80211_M_STA) {
			/* add BSS entry to the WCID table */
			wcid = RUN_AID2WCID(ni->ni_associd);
			run_write_region_1(sc, RT2860_WCID_ENTRY(wcid),
			    ni->ni_macaddr, IEEE80211_ADDR_LEN);
		}

		if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
		    vap->iv_opmode == IEEE80211_M_IBSS)
			run_update_beacon_locked(vap, 0);

		if (vap->iv_opmode != IEEE80211_M_MONITOR) {
			run_enable_tsf_sync(sc);
		} /* else tsf */

		/* enable automatic rate adaptation */
		tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
		if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE)
			run_ratectl_start(sc, ni);

		/* turn link LED on */
		run_set_leds(sc, RT2860_LED_RADIO |
		    (IEEE80211_IS_CHAN_2GHZ(vap->iv_bss->ni_chan) ?
		     RT2860_LED_LINK_2GHZ : RT2860_LED_LINK_5GHZ));

		break;
	default:
		DPRINTFN(6, "undefined case\n");
		break;
	}

	RUN_UNLOCK(sc);
	IEEE80211_LOCK(ic);

	return(rvp->newstate(vap, nstate, arg));
}

/* another taskqueue, so usbd_do_request() can go sleep */
static int
run_wme_update(struct ieee80211com *ic)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;

	ieee80211_runtask(ic, &sc->wme_task);

	/* return whatever, upper layer desn't care anyway */
	return 0;
}

/* ARGSUSED */
static void
run_wme_update_cb(void *arg, int pending)
{
	struct ieee80211com *ic = arg;
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct ieee80211_wme_state *wmesp = &ic->ic_wme;
	int aci, error = 0;

	RUN_LOCK(sc);

	/* update MAC TX configuration registers */
	for (aci = 0; aci < WME_NUM_AC; aci++) {
		error = run_write(sc, RT2860_EDCA_AC_CFG(aci),
		    wmesp->wme_params[aci].wmep_logcwmax << 16 |
		    wmesp->wme_params[aci].wmep_logcwmin << 12 |
		    wmesp->wme_params[aci].wmep_aifsn  <<  8 |
		    wmesp->wme_params[aci].wmep_txopLimit);
		if(error) goto err;
	}

	/* update SCH/DMA registers too */
	error = run_write(sc, RT2860_WMM_AIFSN_CFG,
	    wmesp->wme_params[WME_AC_VO].wmep_aifsn  << 12 |
	    wmesp->wme_params[WME_AC_VI].wmep_aifsn  <<  8 |
	    wmesp->wme_params[WME_AC_BK].wmep_aifsn  <<  4 |
	    wmesp->wme_params[WME_AC_BE].wmep_aifsn);
	if(error) goto err;
	error = run_write(sc, RT2860_WMM_CWMIN_CFG,
	    wmesp->wme_params[WME_AC_VO].wmep_logcwmin << 12 |
	    wmesp->wme_params[WME_AC_VI].wmep_logcwmin <<  8 |
	    wmesp->wme_params[WME_AC_BK].wmep_logcwmin <<  4 |
	    wmesp->wme_params[WME_AC_BE].wmep_logcwmin);
	if(error) goto err;
	error = run_write(sc, RT2860_WMM_CWMAX_CFG,
	    wmesp->wme_params[WME_AC_VO].wmep_logcwmax << 12 |
	    wmesp->wme_params[WME_AC_VI].wmep_logcwmax <<  8 |
	    wmesp->wme_params[WME_AC_BK].wmep_logcwmax <<  4 |
	    wmesp->wme_params[WME_AC_BE].wmep_logcwmax);
	if(error) goto err;
	error = run_write(sc, RT2860_WMM_TXOP0_CFG,
	    wmesp->wme_params[WME_AC_BK].wmep_txopLimit << 16 |
	    wmesp->wme_params[WME_AC_BE].wmep_txopLimit);
	if(error) goto err;
	error = run_write(sc, RT2860_WMM_TXOP1_CFG,
	    wmesp->wme_params[WME_AC_VO].wmep_txopLimit << 16 |
	    wmesp->wme_params[WME_AC_VI].wmep_txopLimit);

err:
	if(error)
		DPRINTF("WME update failed\n");

	RUN_UNLOCK(sc);
	return;
}

static void
run_key_update_begin(struct ieee80211vap *vap)
{
	/*
	 * Because run_key_delete() needs special attention
	 * on lock related operation, lock handling is being done
	 * differently in run_key_set and _delete.
	 *
	 * So, we don't use key_update_begin and _end.
	 */
}

static void
run_key_update_end(struct ieee80211vap *vap)
{
	/* null */
}

/*
 * return 0 on error
 */
static int
run_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k,
		const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	uint32_t attr;
	uint16_t base, associd;
	uint8_t mode, wcid, txmic, rxmic, iv[8];
	int error = 0;

	RUN_LOCK(sc);

	if(vap->iv_opmode == IEEE80211_M_HOSTAP){
		ni = ieee80211_find_vap_node(&ic->ic_sta, vap, mac);
		associd = (ni != NULL) ? ni->ni_associd : 0;
		if(ni != NULL)
			ieee80211_free_node(ni);
		txmic = 24;
		rxmic = 16;
	} else {
		ni = vap->iv_bss;
		associd = (ni != NULL) ? ni->ni_associd : 0;
		txmic = 16;
		rxmic = 24;
	}

	/* map net80211 cipher to RT2860 security mode */
	switch (k->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_WEP:
		if(k->wk_keylen < 8)
			mode = RT2860_MODE_WEP40;
		else
			mode = RT2860_MODE_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		mode = RT2860_MODE_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		mode = RT2860_MODE_AES_CCMP;
		break;
	default:
		DPRINTF("undefined case\n");
		goto fail;
	}

	DPRINTFN(1, "associd=%x, keyix=%d, mode=%x, type=%s\n",
	    associd, k->wk_keyix, mode,
	    (k->wk_flags & IEEE80211_KEY_GROUP) ? "group" : "pairwise");

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		wcid = 0;	/* NB: update WCID0 for group keys */
		base = RT2860_SKEY(0, k->wk_keyix);
	} else {
		wcid = RUN_AID2WCID(associd);
		base = RT2860_PKEY(wcid);
	}

	if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) {
		if(run_write_region_1(sc, base, k->wk_key, 16))
			goto fail;
		if(run_write_region_1(sc, base + 16, &k->wk_key[txmic], 8))	/* wk_txmic */
			goto fail;
		if(run_write_region_1(sc, base + 24, &k->wk_key[rxmic], 8))	/* wk_rxmic */
			goto fail;
	} else {
		/* roundup len to 16-bit: XXX fix write_region_1() instead */
		if(run_write_region_1(sc, base, k->wk_key, (k->wk_keylen + 1) & ~1))
			goto fail;
	}

	if (!(k->wk_flags & IEEE80211_KEY_GROUP) ||
	    (k->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))) {
		/* set initial packet number in IV+EIV */
		if (k->wk_cipher == IEEE80211_CIPHER_WEP){
			memset(iv, 0, sizeof iv);
			iv[3] = sc->sc_rvp->vap.iv_def_txkey << 6;
		} else {
			if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) {
				iv[0] = k->wk_keytsc >> 8;
				iv[1] = (iv[0] | 0x20) & 0x7f;
				iv[2] = k->wk_keytsc;
			} else /* CCMP */ {
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
		if(run_write_region_1(sc, RT2860_IVEIV(wcid), iv, 8))
			goto fail;
	}

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		/* install group key */
		if(run_read(sc, RT2860_SKEY_MODE_0_7, &attr))
			goto fail;
		attr &= ~(0xf << (k->wk_keyix * 4));
		attr |= mode << (k->wk_keyix * 4);
		if(run_write(sc, RT2860_SKEY_MODE_0_7, attr))
			goto fail;
	} else {
		/* install pairwise key */
		if(run_read(sc, RT2860_WCID_ATTR(wcid), &attr))
			goto fail;
		attr = (attr & ~0xf) | (mode << 1) | RT2860_RX_PKEY_EN;
		if(run_write(sc, RT2860_WCID_ATTR(wcid), attr))
			goto fail;
	}

	/* TODO create a pass-thru key entry? */

fail:
	RUN_UNLOCK(sc);
	return (error? 0 : 1);
}

/*
 * return 0 on error
 */
static int
run_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct ieee80211_node *ni = vap->iv_bss;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	uint32_t attr;
	uint8_t wcid;
	int error = 0;
	uint8_t nislocked, cislocked;

	if((nislocked = IEEE80211_NODE_IS_LOCKED(nt)))
		IEEE80211_NODE_UNLOCK(nt);
	if((cislocked = mtx_owned(&ic->ic_comlock.mtx)))
		IEEE80211_UNLOCK(ic);
	RUN_LOCK(sc);

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		/* remove group key */
		if(run_read(sc, RT2860_SKEY_MODE_0_7, &attr))
			goto fail;
		attr &= ~(0xf << (k->wk_keyix * 4));
		if(run_write(sc, RT2860_SKEY_MODE_0_7, attr))
			goto fail;
	} else {
		/* remove pairwise key */
		wcid = RUN_AID2WCID((ni != NULL) ? ni->ni_associd : 0);
		if(run_read(sc, RT2860_WCID_ATTR(wcid), &attr))
			goto fail;
		attr &= ~0xf;
		if(run_write(sc, RT2860_WCID_ATTR(wcid), attr))
			goto fail;
	}

fail:
	RUN_UNLOCK(sc);
	if(cislocked)
		IEEE80211_LOCK(ic);
	if(nislocked)
		IEEE80211_NODE_LOCK(nt);

	return (error? 0 : 1);
}

static void
run_ratectl_start(struct run_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct run_vap *rvp = RUN_VAP(vap);
	uint32_t sta[3];

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	/* read statistic counters (clear on read) and update AMRR state */
	run_read_region_1(sc, RT2860_TX_STA_CNT0,
	    (uint8_t *)sta, sizeof sta);

	ieee80211_ratectl_node_init(ni);

	/* start at lowest available bit-rate, AMRR will raise */
	ni->ni_txrate = 2;

	/* start calibration timer */
	rvp->ratectl_run = RUN_RATECTL_ON;
	usb_callout_reset(&rvp->ratectl_ch, hz, run_ratectl_to, rvp);
}

static void
run_ratectl_to(void *arg)
{
	struct run_vap *rvp = arg;

	/* do it in a process context, so it can go sleep */
	ieee80211_runtask(rvp->vap.iv_ic, &rvp->ratectl_task);
	/* next timeout will be rescheduled in the callback task */
}

/* ARGSUSED */
static void
run_ratectl_cb(void *arg, int pending)
{
	struct run_vap *rvp = arg;
	struct ieee80211vap *vap = &rvp->vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct run_softc *sc = ic->ic_ifp->if_softc;

	if (ic->ic_opmode == IEEE80211_M_STA)
		run_iter_func(rvp, vap->iv_bss);
	else {
		/*
		 * run_reset_livelock() doesn't do anything with AMRR,
		 * but Ralink wants us to call it every 1 sec. So, we
		 * piggyback here rather than creating another callout.
		 * Livelock may occur only in HOSTAP or IBSS mode
		 * (when h/w is sending beacons).
		 */
		RUN_LOCK(sc);
		run_reset_livelock(sc);
		RUN_UNLOCK(sc);
		ieee80211_iterate_nodes(&ic->ic_sta, run_iter_func, rvp);
	}

	if(rvp->ratectl_run == RUN_RATECTL_ON)
		usb_callout_reset(&rvp->ratectl_ch, hz, run_ratectl_to, rvp);
}


static void
run_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct run_vap *rvp = arg;
	struct ieee80211com *ic = rvp->vap.iv_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	uint32_t sta[3], stat;
	int error;
	uint8_t wcid, mcs, pid;
	struct ieee80211vap *vap = ni->ni_vap;
	int txcnt = 0, success = 0, retrycnt = 0;

	if(ic->ic_opmode != IEEE80211_M_STA)
		IEEE80211_NODE_ITERATE_UNLOCK(nt);

	RUN_LOCK(sc);

	if(ic->ic_opmode != IEEE80211_M_STA){
		/* drain Tx status FIFO (maxsize = 16) */
		run_read(sc, RT2860_TX_STAT_FIFO, &stat);
		while (stat & RT2860_TXQ_VLD) {
			DPRINTFN(4, "tx stat 0x%08x\n", stat);

			wcid = (stat >> RT2860_TXQ_WCID_SHIFT) & 0xff;

			/* if no ACK was requested, no feedback is available */
			if (!(stat & RT2860_TXQ_ACKREQ) || wcid == 0xff)
				continue;

			/* update per-STA AMRR stats */
			if (stat & RT2860_TXQ_OK) {
				/*
				 * Check if there were retries, ie if the Tx
				 * success rate is different from the requested
				 * rate.  Note that it works only because we do
				 * not allow rate fallback from OFDM to CCK.
				 */
				mcs = (stat >> RT2860_TXQ_MCS_SHIFT) & 0x7f;
				pid = (stat >> RT2860_TXQ_PID_SHIFT) & 0xf;
				if (mcs + 1 != pid)
					retrycnt = 1;
				ieee80211_ratectl_tx_complete(vap, ni,
				    IEEE80211_RATECTL_TX_SUCCESS,
				    &retrycnt, NULL);
			} else {
				retrycnt = 1;
				ieee80211_ratectl_tx_complete(vap, ni,
				    IEEE80211_RATECTL_TX_SUCCESS,
				    &retrycnt, NULL);
				ifp->if_oerrors++;
			}
			run_read_region_1(sc, RT2860_TX_STAT_FIFO,
			    (uint8_t *)&stat, sizeof stat);
		}
	} else {
		/* read statistic counters (clear on read) and update AMRR state */
		error = run_read_region_1(sc, RT2860_TX_STA_CNT0, (uint8_t *)sta,
		    sizeof sta);
		if (error != 0)
			goto skip;

		DPRINTFN(3, "retrycnt=%d txcnt=%d failcnt=%d\n",
		    le32toh(sta[1]) >> 16, le32toh(sta[1]) & 0xffff,
		    le32toh(sta[0]) & 0xffff);

		/* count failed TX as errors */
		ifp->if_oerrors += le32toh(sta[0]) & 0xffff;

		retrycnt =
		    (le32toh(sta[0]) & 0xffff) +	/* failed TX count */
		    (le32toh(sta[1]) >> 16);		/* TX retransmission count */

		txcnt =
		    retrycnt +
		    (le32toh(sta[1]) & 0xffff);		/* successful TX count */

		success =
		    (le32toh(sta[1]) >> 16) +
		    (le32toh(sta[1]) & 0xffff);
		ieee80211_ratectl_tx_update(vap, ni, &txcnt, &success,
		    &retrycnt);
	}

	ieee80211_ratectl_rate(ni, NULL, 0);

skip:;
	RUN_UNLOCK(sc);

	if(ic->ic_opmode != IEEE80211_M_STA)
		IEEE80211_NODE_ITERATE_LOCK(nt);
}

static void
run_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct run_node *rn = (void *)ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	uint8_t rate;
	int ridx, i, j;

	DPRINTF("new assoc isnew=%d addr=%s\n",
	    isnew, ether_sprintf(ni->ni_macaddr));

	for (i = 0; i < rs->rs_nrates; i++) {
		rate = rs->rs_rates[i] & IEEE80211_RATE_VAL;
		/* convert 802.11 rate to hardware rate index */
		for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++)
			if (rt2860_rates[ridx].rate == rate)
				break;
		rn->ridx[i] = ridx;
		/* determine rate of control response frames */
		for (j = i; j >= 0; j--) {
			if ((rs->rs_rates[j] & IEEE80211_RATE_BASIC) &&
			    rt2860_rates[rn->ridx[i]].phy ==
			    rt2860_rates[rn->ridx[j]].phy)
				break;
		}
		if (j >= 0) {
			rn->ctl_ridx[i] = rn->ridx[j];
		} else {
			/* no basic rate found, use mandatory one */
			rn->ctl_ridx[i] = rt2860_rates[ridx].ctl_ridx;
		}
		DPRINTF("rate=0x%02x ridx=%d ctl_ridx=%d\n",
		    rs->rs_rates[i], rn->ridx[i], rn->ctl_ridx[i]);
	}
}

/*
 * Return the Rx chain with the highest RSSI for a given frame.
 */
static __inline uint8_t
run_maxrssi_chain(struct run_softc *sc, const struct rt2860_rxwi *rxwi)
{
	uint8_t rxchain = 0;

	if (sc->nrxchains > 1) {
		if (rxwi->rssi[1] > rxwi->rssi[rxchain])
			rxchain = 1;
		if (sc->nrxchains > 2)
			if (rxwi->rssi[2] > rxwi->rssi[rxchain])
				rxchain = 2;
	}
	return rxchain;
}

static void
run_rx_frame(struct run_softc *sc, struct mbuf *m, uint32_t dmalen)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211vap *vap = &sc->sc_rvp->vap;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct rt2870_rxd *rxd;
	struct rt2860_rxwi *rxwi;
	uint32_t flags;
	uint16_t len, phy;
	uint8_t ant, rssi;
	int8_t nf;

	rxwi = mtod(m, struct rt2860_rxwi *);
	len = le16toh(rxwi->len) & 0xfff;
	if (__predict_false(len > dmalen)) {
		m_freem(m);
		ifp->if_ierrors++;
		DPRINTF("bad RXWI length %u > %u\n", len, dmalen);
		return;
	}
	/* Rx descriptor is located at the end */
	rxd = (struct rt2870_rxd *)(mtod(m, caddr_t) + dmalen);
	flags = le32toh(rxd->flags);

	if (__predict_false(flags & (RT2860_RX_CRCERR | RT2860_RX_ICVERR))) {
		m_freem(m);
		ifp->if_ierrors++;
		DPRINTF("%s error.\n", (flags & RT2860_RX_CRCERR)?"CRC":"ICV");
		return;
	}

	m->m_data += sizeof(struct rt2860_rxwi);
	m->m_pkthdr.len = m->m_len -= sizeof(struct rt2860_rxwi);

	wh = mtod(m, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP){
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		m->m_flags |= M_WEP;
	}

	if (flags & RT2860_RX_L2PAD){
		DPRINTFN(8, "received RT2860_RX_L2PAD frame\n");
		len += 2;
	}

	if (__predict_false(flags & RT2860_RX_MICERR)) {
		/* report MIC failures to net80211 for TKIP */
		ieee80211_notify_michael_failure(vap, wh, rxwi->keyidx);
		m_freem(m);
		ifp->if_ierrors++;
		DPRINTF("MIC error. Someone is lying.\n");
		return;
	}

	ant = run_maxrssi_chain(sc, rxwi);
	rssi = rxwi->rssi[ant];
	nf = run_rssi2dbm(sc, rssi, ant);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = len;

	ni = ieee80211_find_rxnode(ic,
	    mtod(m, struct ieee80211_frame_min *));
	if (ni != NULL) {
		(void)ieee80211_input(ni, m, rssi, nf);
		ieee80211_free_node(ni);
	} else {
		(void)ieee80211_input_all(ic, m, rssi, nf);
	}

	if(__predict_false(ieee80211_radiotap_active(ic))){
		struct run_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq = htole16(ic->ic_bsschan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_bsschan->ic_flags);
		tap->wr_antsignal = rssi;
		tap->wr_antenna = ant;
		tap->wr_dbm_antsignal = run_rssi2dbm(sc, rssi, ant);
		tap->wr_rate = 2;	/* in case it can't be found below */
		phy = le16toh(rxwi->phy);
		switch (phy & RT2860_PHY_MODE) {
		case RT2860_PHY_CCK:
			switch ((phy & RT2860_PHY_MCS) & ~RT2860_PHY_SHPRE) {
			case 0:	tap->wr_rate =   2; break;
			case 1:	tap->wr_rate =   4; break;
			case 2:	tap->wr_rate =  11; break;
			case 3:	tap->wr_rate =  22; break;
			}
			if (phy & RT2860_PHY_SHPRE)
				tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
			break;
		case RT2860_PHY_OFDM:
			switch (phy & RT2860_PHY_MCS) {
			case 0:	tap->wr_rate =  12; break;
			case 1:	tap->wr_rate =  18; break;
			case 2:	tap->wr_rate =  24; break;
			case 3:	tap->wr_rate =  36; break;
			case 4:	tap->wr_rate =  48; break;
			case 5:	tap->wr_rate =  72; break;
			case 6:	tap->wr_rate =  96; break;
			case 7:	tap->wr_rate = 108; break;
			}
			break;
		}
	}
}

static void
run_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct run_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m = NULL;
	struct mbuf *m0;
	uint32_t dmalen;
	int xferlen;

	usbd_xfer_status(xfer, &xferlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTFN(15, "rx done, actlen=%d\n", xferlen);

		if (xferlen < sizeof (uint32_t) +
		    sizeof (struct rt2860_rxwi) + sizeof (struct rt2870_rxd)) {
			DPRINTF("xfer too short %d\n", xferlen);
			goto tr_setup;
		}

		m = sc->rx_m;
		sc->rx_m = NULL;

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		if (sc->rx_m == NULL) {
			sc->rx_m = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR,
			    MJUMPAGESIZE /* xfer can be bigger than MCLBYTES */);
		}
		if (sc->rx_m == NULL) {
			DPRINTF("could not allocate mbuf - idle with stall\n");
			ifp->if_ierrors++;
			usbd_xfer_set_stall(xfer);
			usbd_xfer_set_frames(xfer, 0);
		} else {
			/*
			 * Directly loading a mbuf cluster into DMA to
			 * save some data copying. This works because
			 * there is only one cluster.
			 */
			usbd_xfer_set_frame_data(xfer, 0, 
			    mtod(sc->rx_m, caddr_t), RUN_MAX_RXSZ);
			usbd_xfer_set_frames(xfer, 1);
		}
		usbd_transfer_submit(xfer);
		break;

	default:	/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);

			if (error == USB_ERR_TIMEOUT)
				device_printf(sc->sc_dev, "device timeout\n");

			ifp->if_ierrors++;

			goto tr_setup;
		}
		if(sc->rx_m != NULL){
			m_freem(sc->rx_m);
			sc->rx_m = NULL;
		}
		break;
	}

	if (m == NULL)
		return;

	/* inputting all the frames must be last */

	RUN_UNLOCK(sc);

	m->m_pkthdr.len = m->m_len = xferlen;

	/* HW can aggregate multiple 802.11 frames in a single USB xfer */
	for(;;) {
		dmalen = le32toh(*mtod(m, uint32_t *)) & 0xffff;

		if ((dmalen == 0) || ((dmalen & 3) != 0)) {
			DPRINTF("bad DMA length %u\n", dmalen);
			break;
		}
		if ((dmalen + 8) > xferlen) {
			DPRINTF("bad DMA length %u > %d\n",
			dmalen + 8, xferlen);
			break;
		}

		/* If it is the last one or a single frame, we won't copy. */
		if((xferlen -= dmalen + 8) <= 8){
			/* trim 32-bit DMA-len header */
			m->m_data += 4;
			m->m_pkthdr.len = m->m_len -= 4;
			run_rx_frame(sc, m, dmalen);
			break;
		}

		/* copy aggregated frames to another mbuf */
		m0 = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (__predict_false(m0 == NULL)) {
			DPRINTF("could not allocate mbuf\n");
			ifp->if_ierrors++;
			break;
		}
		m_copydata(m, 4 /* skip 32-bit DMA-len header */,
		    dmalen + sizeof(struct rt2870_rxd), mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len =
		    dmalen + sizeof(struct rt2870_rxd);
		run_rx_frame(sc, m0, dmalen);

		/* update data ptr */
		m->m_data += dmalen + 8;
		m->m_pkthdr.len = m->m_len -= dmalen + 8;
	}

	RUN_LOCK(sc);
}

static void
run_tx_free(struct run_endpoint_queue *pq,
    struct run_tx_data *data, int txerr)
{
	if (data->m != NULL) {
		if (data->m->m_flags & M_TXCB)
			ieee80211_process_callback(data->ni, data->m,
			    txerr ? ETIMEDOUT : 0);
		m_freem(data->m);
		data->m = NULL;

		if(data->ni == NULL) {
			DPRINTF("no node\n");
		} else {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}

	STAILQ_INSERT_TAIL(&pq->tx_fh, data, next);
	pq->tx_nfree++;
}

static void
run_bulk_tx_callbackN(struct usb_xfer *xfer, usb_error_t error, unsigned int index)
{
	struct run_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct run_tx_data *data;
	struct ieee80211vap *vap = NULL;
	struct usb_page_cache *pc;
	struct run_endpoint_queue *pq = &sc->sc_epq[index];
	struct mbuf *m;
	usb_frlength_t size;
	unsigned int len;
	int actlen;
	int sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	switch (USB_GET_STATE(xfer)){
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete: %d "
		    "bytes @ index %d\n", actlen, index);

		data = usbd_xfer_get_priv(xfer);

		run_tx_free(pq, data, 0);
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		usbd_xfer_set_priv(xfer, NULL);

		ifp->if_opackets++;

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		data = STAILQ_FIRST(&pq->tx_qh);
		if(data == NULL)
			break;

		STAILQ_REMOVE_HEAD(&pq->tx_qh, next);

		m = data->m;
		if (m->m_pkthdr.len > RUN_MAX_TXSZ) {
			DPRINTF("data overflow, %u bytes\n",
			    m->m_pkthdr.len);

			ifp->if_oerrors++;

			run_tx_free(pq, data, 1);

			goto tr_setup;
		}

		pc = usbd_xfer_get_frame(xfer, 0);
		size = sizeof(data->desc);
		usbd_copy_in(pc, 0, &data->desc, size);
		usbd_m_copy_in(pc, size, m, 0, m->m_pkthdr.len);

		vap = data->ni->ni_vap;
		if (ieee80211_radiotap_active_vap(vap)) {
			struct run_tx_radiotap_header *tap = &sc->sc_txtap;

			tap->wt_flags = 0;
			tap->wt_rate = rt2860_rates[data->ridx].rate;
			tap->wt_chan_freq = htole16(vap->iv_bss->ni_chan->ic_freq);
			tap->wt_chan_flags = htole16(vap->iv_bss->ni_chan->ic_flags);
			tap->wt_hwqueue = index;
			if (data->mcs & RT2860_PHY_SHPRE)
				tap->wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

			ieee80211_radiotap_tx(vap, m);
		}

		/* align end on a 4-bytes boundary */
		len = (size + m->m_pkthdr.len + 3) & ~3;

		DPRINTFN(11, "sending frame len=%u xferlen=%u @ index %d\n",
			m->m_pkthdr.len, len, index);

		usbd_xfer_set_frame_len(xfer, 0, len);
		usbd_xfer_set_priv(xfer, data);

		usbd_transfer_submit(xfer);

		RUN_UNLOCK(sc);
		run_start(ifp);
		RUN_LOCK(sc);

		break;

	default:
		DPRINTF("USB transfer error, %s\n",
		    usbd_errstr(error));

		data = usbd_xfer_get_priv(xfer);

		ifp->if_oerrors++;

		if (data != NULL) {
			run_tx_free(pq, data, error);
			usbd_xfer_set_priv(xfer, NULL);
		}

		if (error != USB_ERR_CANCELLED) {
			if (error == USB_ERR_TIMEOUT) {
				device_printf(sc->sc_dev, "device timeout\n");
				ieee80211_runtask(ifp->if_l2com, &sc->usb_timeout_task);
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
}

static void
run_bulk_tx_callback0(struct usb_xfer *xfer, usb_error_t error)
{
	run_bulk_tx_callbackN(xfer, error, 0);
}

static void
run_bulk_tx_callback1(struct usb_xfer *xfer, usb_error_t error)
{
	run_bulk_tx_callbackN(xfer, error, 1);
}

static void
run_bulk_tx_callback2(struct usb_xfer *xfer, usb_error_t error)
{
	run_bulk_tx_callbackN(xfer, error, 2);
}

static void
run_bulk_tx_callback3(struct usb_xfer *xfer, usb_error_t error)
{
	run_bulk_tx_callbackN(xfer, error, 3);
}

static void
run_bulk_tx_callback4(struct usb_xfer *xfer, usb_error_t error)
{
	run_bulk_tx_callbackN(xfer, error, 4);
}

static void
run_bulk_tx_callback5(struct usb_xfer *xfer, usb_error_t error)
{
	run_bulk_tx_callbackN(xfer, error, 5);
}

static void
run_set_tx_desc(struct run_softc *sc, struct run_tx_data *data,
	uint8_t wflags, uint8_t xflags, uint8_t opflags, uint8_t dflags,
	uint8_t type, uint8_t pad)
{
	struct mbuf *m = data->m;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211vap *vap = &sc->sc_rvp->vap;
	struct ieee80211_frame *wh;
	struct rt2870_txd *txd;
	struct rt2860_txwi *txwi;
	int xferlen;
	uint8_t mcs;
	uint8_t ridx = data->ridx;

	/* get MCS code from rate index */
	data->mcs = mcs = rt2860_rates[ridx].mcs;

	xferlen = sizeof(*txwi) + m->m_pkthdr.len;

	/* roundup to 32-bit alignment */
	xferlen = (xferlen + 3) & ~3;

	txd = (struct rt2870_txd *)&data->desc;
	txd->flags = dflags;
	txd->len = htole16(xferlen);

	/* setup TX Wireless Information */
	txwi = (struct rt2860_txwi *)(txd + 1);
	txwi->flags = wflags;
	txwi->xflags = xflags;
	txwi->wcid = (type == IEEE80211_FC0_TYPE_DATA) ?
	    RUN_AID2WCID(data->ni->ni_associd) : 0xff;
	txwi->len = htole16(m->m_pkthdr.len - pad);
	if (rt2860_rates[ridx].phy == IEEE80211_T_DS) {
		txwi->phy = htole16(RT2860_PHY_CCK);
		if (ridx != RT2860_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			mcs |= RT2860_PHY_SHPRE;
	} else
		txwi->phy = htole16(RT2860_PHY_OFDM);
	txwi->phy |= htole16(mcs);

	wh = mtod(m, struct ieee80211_frame *);

	/* check if RTS/CTS or CTS-to-self protection is required */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (m->m_pkthdr.len + IEEE80211_CRC_LEN > vap->iv_rtsthreshold ||
	     ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	      rt2860_rates[ridx].phy == IEEE80211_T_OFDM)))
		txwi->txop = RT2860_TX_TXOP_HT | opflags;
	else
		txwi->txop = RT2860_TX_TXOP_BACKOFF | opflags;
}

/* This function must be called locked */
static int
run_tx(struct run_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211vap *vap = &sc->sc_rvp->vap;
	struct ieee80211_frame *wh;
	const struct ieee80211_txparam *tp;
	struct run_tx_data *data;
	uint16_t qos;
	uint16_t dur;
	uint8_t type;
	uint8_t tid;
	uint8_t qid;
	uint8_t qflags;
	uint8_t pad;
	uint8_t xflags = 0;
	int hasqos;
	int ridx;
	int ctl_ridx;

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	wh = mtod(m, struct ieee80211_frame *);

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/*
	 * There are 7 bulk endpoints: 1 for RX
	 * and 6 for TX (4 EDCAs + HCCA + Prio).
	 * Update 03-14-2009:  some devices like the Planex GW-US300MiniS
	 * seem to have only 4 TX bulk endpoints (Fukaumi Naoki).
	 */
	if ((hasqos = IEEE80211_QOS_HAS_SEQ(wh))) {
		uint8_t *frm;

		if(IEEE80211_HAS_ADDR4(wh))
			frm = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos;
		else
			frm =((struct ieee80211_qosframe *)wh)->i_qos;

		qos = le16toh(*(const uint16_t *)frm);
		tid = qos & IEEE80211_QOS_TID;
		qid = TID_TO_WME_AC(tid);
		pad = 2;
	} else {
		qos = 0;
		tid = 0;
		qid = WME_AC_BE;
		pad = 0;
	}
	qflags = (qid < 4) ? RT2860_TX_QSEL_EDCA : RT2860_TX_QSEL_HCCA;

	DPRINTFN(8, "qos %d\tqid %d\ttid %d\tqflags %x\n",
	    qos, qid, tid, qflags);

	tp = &vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)];

	/* pickup a rate index */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA) {
		ridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		    RT2860_RIDX_OFDM6 : RT2860_RIDX_CCK1;
		ctl_ridx = rt2860_rates[ridx].ctl_ridx;
	} else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
		ridx = sc->fixed_ridx;
		ctl_ridx = rt2860_rates[ridx].ctl_ridx;
	} else {
		for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++){
		        if (rt2860_rates[ridx].rate == ni->ni_txrate)
		                break;
		}
		ctl_ridx = rt2860_rates[ridx].ctl_ridx;
	}

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (!hasqos || (qos & IEEE80211_QOS_ACKPOLICY) !=
	     IEEE80211_QOS_ACKPOLICY_NOACK)) {
		xflags |= RT2860_TX_ACK;
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			dur = rt2860_rates[ridx].sp_ack_dur;
		else
			dur = rt2860_rates[ridx].lp_ack_dur;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

	/* reserve slots for mgmt packets, just in case */
	if (sc->sc_epq[qid].tx_nfree < 3) {
		DPRINTFN(10, "tx ring %d is full\n", qid);
		return (-1);
	}

	data = STAILQ_FIRST(&sc->sc_epq[qid].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[qid].tx_fh, next);
	sc->sc_epq[qid].tx_nfree--;

	data->m = m;
	data->ni = ni;
	data->ridx = ridx;

	run_set_tx_desc(sc, data, 0, xflags, 0, qflags, type, pad);

        STAILQ_INSERT_TAIL(&sc->sc_epq[qid].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[qid]);

	DPRINTFN(8, "sending data frame len=%d rate=%d qid=%d\n", m->m_pkthdr.len +
	    (int)(sizeof (struct rt2870_txd) + sizeof (struct rt2860_rxwi)),
	    rt2860_rates[ridx].rate, qid);

	return (0);
}

static int
run_tx_mgt(struct run_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	const struct ieee80211_txparam *tp;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ifp->if_l2com;
	struct run_tx_data *data;
	struct ieee80211_frame *wh;
	int ridx;
	uint16_t dur;
	uint8_t type;
	uint8_t xflags = 0;

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	wh = mtod(m, struct ieee80211_frame *);

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		xflags |= RT2860_TX_ACK;

		dur = ieee80211_ack_duration(ic->ic_rt, tp->mgmtrate, 
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			xflags |= RT2860_TX_TS;
	}

	if (sc->sc_epq[0].tx_nfree == 0) {
		/* let caller free mbuf */
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		return (EIO);
	}
	data = STAILQ_FIRST(&sc->sc_epq[0].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[0].tx_fh, next);
	sc->sc_epq[0].tx_nfree--;

	data->m = m;
	data->ni = ni;
	for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++)
		if (rt2860_rates[ridx].rate == tp->mgmtrate)
			break;
	data->ridx = ridx;

	run_set_tx_desc(sc, data, 0, xflags, 0, RT2860_TX_QSEL_MGMT,
	    wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, 0);

	DPRINTFN(10, "sending mgt frame len=%d rate=%d\n", m->m_pkthdr.len +
	    (int)(sizeof (struct rt2870_txd) + sizeof (struct rt2860_rxwi)),
	    tp->mgmtrate);

	STAILQ_INSERT_TAIL(&sc->sc_epq[0].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[0]);

	return (0);
}

static int
run_sendprot(struct run_softc *sc,
    const struct mbuf *m, struct ieee80211_node *ni, int prot, int rate)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame *wh;
	struct run_tx_data *data;
	struct mbuf *mprot;
	int ridx;
	int protrate;
	int ackrate;
	int pktlen;
	int isshort;
	uint16_t dur;
	uint8_t type;
	uint8_t wflags;
	uint8_t txflags = 0;

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	KASSERT(prot == IEEE80211_PROT_RTSCTS || prot == IEEE80211_PROT_CTSONLY,
	    ("protection %d", prot));

	wh = mtod(m, struct ieee80211_frame *);
	pktlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	protrate = ieee80211_ctl_rate(ic->ic_rt, rate);
	ackrate = ieee80211_ack_rate(ic->ic_rt, rate);

	isshort = (ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0;
	dur = ieee80211_compute_duration(ic->ic_rt, pktlen, rate, isshort);
	    + ieee80211_ack_duration(ic->ic_rt, rate, isshort);
	wflags = RT2860_TX_FRAG;

	/* check that there are free slots before allocating the mbuf */
	if (sc->sc_epq[0].tx_nfree == 0) {
		/* let caller free mbuf */
		sc->sc_ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		return (ENOBUFS);
	}

	if (prot == IEEE80211_PROT_RTSCTS) {
		/* NB: CTS is the same size as an ACK */
		dur += ieee80211_ack_duration(ic->ic_rt, rate, isshort);
		txflags |= RT2860_TX_ACK;
		mprot = ieee80211_alloc_rts(ic, wh->i_addr1, wh->i_addr2, dur);
	} else {
		mprot = ieee80211_alloc_cts(ic, ni->ni_vap->iv_myaddr, dur);
	}
	if (mprot == NULL) {
		sc->sc_ifp->if_oerrors++;
		DPRINTF("could not allocate mbuf\n");
		return (ENOBUFS);
	}

        data = STAILQ_FIRST(&sc->sc_epq[0].tx_fh);
        STAILQ_REMOVE_HEAD(&sc->sc_epq[0].tx_fh, next);
        sc->sc_epq[0].tx_nfree--;

	data->m = mprot;
	data->ni = ieee80211_ref_node(ni);

	for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++)
		if (rt2860_rates[ridx].rate == protrate)
			break;
	data->ridx = ridx;

	run_set_tx_desc(sc, data, wflags, txflags, 0,
	    RT2860_TX_QSEL_EDCA, type, 0);

        DPRINTFN(1, "sending prot len=%u rate=%u\n",
            m->m_pkthdr.len, rate);

        STAILQ_INSERT_TAIL(&sc->sc_epq[0].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[0]);

	return (0);
}

static int
run_tx_param(struct run_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame *wh;
	struct run_tx_data *data;
	uint8_t type;
	uint8_t opflags;
	uint8_t txflags;
	int ridx;
	int rate;
	int error;

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	KASSERT(params != NULL, ("no raw xmit params"));

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	rate = params->ibp_rate0;
	if (!ieee80211_isratevalid(ic->ic_rt, rate)) {
		/* let caller free mbuf */
		return (EINVAL);
	}

	opflags = 0;
	txflags = 0;
	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0)
		txflags |= RT2860_TX_ACK;
	if (params->ibp_flags & (IEEE80211_BPF_RTS|IEEE80211_BPF_CTS)) {
		error = run_sendprot(sc, m, ni,
		    params->ibp_flags & IEEE80211_BPF_RTS ?
			IEEE80211_PROT_RTSCTS : IEEE80211_PROT_CTSONLY,
		    rate);
		if (error) {
			/* let caller free mbuf */
			return (error);
		}
		opflags |= /*XXX RT2573_TX_LONG_RETRY |*/ RT2860_TX_TXOP_SIFS;
	}

	if (sc->sc_epq[0].tx_nfree == 0) {
		/* let caller free mbuf */
		sc->sc_ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		DPRINTF("sending raw frame, but tx ring is full\n");
		return (EIO);
	}
        data = STAILQ_FIRST(&sc->sc_epq[0].tx_fh);
        STAILQ_REMOVE_HEAD(&sc->sc_epq[0].tx_fh, next);
        sc->sc_epq[0].tx_nfree--;

        data->m = m;
        data->ni = ni;
	for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++)
		if (rt2860_rates[ridx].rate == rate)
			break;
	data->ridx = ridx;

        run_set_tx_desc(sc, data, 0, txflags, opflags, 
	    RT2860_TX_QSEL_EDCA, type, 0);

        DPRINTFN(10, "sending raw frame len=%u rate=%u\n",
            m->m_pkthdr.len, rate);

        STAILQ_INSERT_TAIL(&sc->sc_epq[0].tx_qh, data, next);

	usbd_transfer_start(sc->sc_xfer[0]);

        return (0); 
}

static int
run_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ifnet *ifp = ni->ni_ic->ic_ifp;
	struct run_softc *sc = ifp->if_softc;
	int error;

	RUN_LOCK(sc);

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error =  ENETDOWN;
		goto bad;
	}

	if (params == NULL) {
		/* tx mgt packet */
		if ((error = run_tx_mgt(sc, m, ni)) != 0){
			ifp->if_oerrors++;
			DPRINTF("mgt tx failed\n");
			goto bad;
		}
	} else {
		/* tx raw packet with param */
		if ((error = run_tx_param(sc, m, ni, params)) != 0){
			ifp->if_oerrors++;
			DPRINTF("tx with param failed\n");
			goto bad;
		}
	}

	ifp->if_opackets++;

	RUN_UNLOCK(sc);

	return (0);

bad:
	RUN_UNLOCK(sc);
	if(m != NULL)
		m_freem(m);
	ieee80211_free_node(ni);

	return (error);
}

static void
run_start(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct mbuf *m;

	RUN_LOCK(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		RUN_UNLOCK(sc);
		return;
	}

	for (;;) {
		/* send data frames */
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (run_tx(sc, m, ni) != 0) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
	}

	RUN_UNLOCK(sc);
}

static int
run_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		RUN_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)){
				run_init_locked(sc);
				startall = 1;
			} else
				run_update_promisc_locked(ifp);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				run_stop(sc);
		}
		RUN_UNLOCK(sc);
		if(startall)
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

static void
run_set_agc(struct run_softc *sc, uint8_t agc)
{
	uint8_t bbp;

	if (sc->mac_ver == 0x3572) {
		run_bbp_read(sc, 27, &bbp);
		bbp &= ~(0x3 << 5);
		run_bbp_write(sc, 27, bbp | 0 << 5);	/* select Rx0 */
		run_bbp_write(sc, 66, agc);
		run_bbp_write(sc, 27, bbp | 1 << 5);	/* select Rx1 */
		run_bbp_write(sc, 66, agc);
	} else
		run_bbp_write(sc, 66, agc);
}

static void
run_select_chan_group(struct run_softc *sc, int group)
{
	uint32_t tmp;
	uint8_t agc;

	run_bbp_write(sc, 62, 0x37 - sc->lna[group]);
	run_bbp_write(sc, 63, 0x37 - sc->lna[group]);
	run_bbp_write(sc, 64, 0x37 - sc->lna[group]);
	run_bbp_write(sc, 86, 0x00);

	if (group == 0) {
		if (sc->ext_2ghz_lna) {
			run_bbp_write(sc, 82, 0x62);
			run_bbp_write(sc, 75, 0x46);
		} else {
			run_bbp_write(sc, 82, 0x84);
			run_bbp_write(sc, 75, 0x50);
		}
	} else {
		if (sc->mac_ver == 0x3572)
			run_bbp_write(sc, 82, 0x94);
		else
			run_bbp_write(sc, 82, 0xf2);
		if (sc->ext_5ghz_lna)
			run_bbp_write(sc, 75, 0x46);
		else 
			run_bbp_write(sc, 75, 0x50);
	}

	run_read(sc, RT2860_TX_BAND_CFG, &tmp);
	tmp &= ~(RT2860_5G_BAND_SEL_N | RT2860_5G_BAND_SEL_P);
	tmp |= (group == 0) ? RT2860_5G_BAND_SEL_N : RT2860_5G_BAND_SEL_P;
	run_write(sc, RT2860_TX_BAND_CFG, tmp);

	/* enable appropriate Power Amplifiers and Low Noise Amplifiers */
	tmp = RT2860_RFTR_EN | RT2860_TRSW_EN;
	if (group == 0) {	/* 2GHz */
		tmp |= RT2860_PA_PE_G0_EN | RT2860_LNA_PE_G0_EN;
		if (sc->ntxchains > 1)
			tmp |= RT2860_PA_PE_G1_EN;
		if (sc->nrxchains > 1)
			tmp |= RT2860_LNA_PE_G1_EN;
	} else {		/* 5GHz */
		tmp |= RT2860_PA_PE_A0_EN | RT2860_LNA_PE_A0_EN;
		if (sc->ntxchains > 1)
			tmp |= RT2860_PA_PE_A1_EN;
		if (sc->nrxchains > 1)
			tmp |= RT2860_LNA_PE_A1_EN;
	}
	if (sc->mac_ver == 0x3572) {
		run_rt3070_rf_write(sc, 8, 0x00);
		run_write(sc, RT2860_TX_PIN_CFG, tmp);
		run_rt3070_rf_write(sc, 8, 0x80);
	} else
		run_write(sc, RT2860_TX_PIN_CFG, tmp);

	/* set initial AGC value */
	if (group == 0) {	/* 2GHz band */
		if (sc->mac_ver >= 0x3070)
			agc = 0x1c + sc->lna[0] * 2;
		else
			agc = 0x2e + sc->lna[0];
	} else {		/* 5GHz band */
		if (sc->mac_ver == 0x3572)
			agc = 0x22 + (sc->lna[group] * 5) / 3;
		else
			agc = 0x32 + (sc->lna[group] * 5) / 3;
	}
	run_set_agc(sc, agc);
}

static void
run_rt2870_set_chan(struct run_softc *sc, uint32_t chan)
{
	const struct rfprog *rfprog = rt2860_rf2850;
	uint32_t r2, r3, r4;
	int8_t txpow1, txpow2;
	int i;

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rfprog[i].chan != chan; i++);

	r2 = rfprog[i].r2;
	if (sc->ntxchains == 1)
		r2 |= 1 << 12;		/* 1T: disable Tx chain 2 */
	if (sc->nrxchains == 1)
		r2 |= 1 << 15 | 1 << 4;	/* 1R: disable Rx chains 2 & 3 */
	else if (sc->nrxchains == 2)
		r2 |= 1 << 4;		/* 2R: disable Rx chain 3 */

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];
	txpow2 = sc->txpow2[i];
	if (chan > 14) {
		if (txpow1 >= 0)
			txpow1 = txpow1 << 1;
		else
			txpow1 = (7 + txpow1) << 1 | 1;
		if (txpow2 >= 0)
			txpow2 = txpow2 << 1;
		else
			txpow2 = (7 + txpow2) << 1 | 1;
	}
	r3 = rfprog[i].r3 | txpow1 << 7;
	r4 = rfprog[i].r4 | sc->freq << 13 | txpow2 << 4;

	run_rt2870_rf_write(sc, RT2860_RF1, rfprog[i].r1);
	run_rt2870_rf_write(sc, RT2860_RF2, r2);
	run_rt2870_rf_write(sc, RT2860_RF3, r3);
	run_rt2870_rf_write(sc, RT2860_RF4, r4);

	run_delay(sc, 10);

	run_rt2870_rf_write(sc, RT2860_RF1, rfprog[i].r1);
	run_rt2870_rf_write(sc, RT2860_RF2, r2);
	run_rt2870_rf_write(sc, RT2860_RF3, r3 | 1);
	run_rt2870_rf_write(sc, RT2860_RF4, r4);

	run_delay(sc, 10);

	run_rt2870_rf_write(sc, RT2860_RF1, rfprog[i].r1);
	run_rt2870_rf_write(sc, RT2860_RF2, r2);
	run_rt2870_rf_write(sc, RT2860_RF3, r3);
	run_rt2870_rf_write(sc, RT2860_RF4, r4);
}

static void
run_rt3070_set_chan(struct run_softc *sc, uint32_t chan)
{
	int8_t txpow1, txpow2;
	uint8_t rf;
	int i;

	/* RT3070 is 2GHz only */
	KASSERT(chan >= 1 && chan <= 14, ("wrong channel selected\n"));

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rt2860_rf2850[i].chan != chan; i++);

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];
	txpow2 = sc->txpow2[i];

	run_rt3070_rf_write(sc, 2, rt3070_freqs[i].n);
	run_rt3070_rf_write(sc, 3, rt3070_freqs[i].k);
	run_rt3070_rf_read(sc, 6, &rf);
	rf = (rf & ~0x03) | rt3070_freqs[i].r;
	run_rt3070_rf_write(sc, 6, rf);

	/* set Tx0 power */
	run_rt3070_rf_read(sc, 12, &rf);
	rf = (rf & ~0x1f) | txpow1;
	run_rt3070_rf_write(sc, 12, rf);

	/* set Tx1 power */
	run_rt3070_rf_read(sc, 13, &rf);
	rf = (rf & ~0x1f) | txpow2;
	run_rt3070_rf_write(sc, 13, rf);

	run_rt3070_rf_read(sc, 1, &rf);
	rf &= ~0xfc;
	if (sc->ntxchains == 1)
		rf |= 1 << 7 | 1 << 5;	/* 1T: disable Tx chains 2 & 3 */
	else if (sc->ntxchains == 2)
		rf |= 1 << 7;		/* 2T: disable Tx chain 3 */
	if (sc->nrxchains == 1)
		rf |= 1 << 6 | 1 << 4;	/* 1R: disable Rx chains 2 & 3 */
	else if (sc->nrxchains == 2)
		rf |= 1 << 6;		/* 2R: disable Rx chain 3 */
	run_rt3070_rf_write(sc, 1, rf);

	/* set RF offset */
	run_rt3070_rf_read(sc, 23, &rf);
	rf = (rf & ~0x7f) | sc->freq;
	run_rt3070_rf_write(sc, 23, rf);

	/* program RF filter */
	run_rt3070_rf_read(sc, 24, &rf);	/* Tx */
	rf = (rf & ~0x3f) | sc->rf24_20mhz;
	run_rt3070_rf_write(sc, 24, rf);
	run_rt3070_rf_read(sc, 31, &rf);	/* Rx */
	rf = (rf & ~0x3f) | sc->rf24_20mhz;
	run_rt3070_rf_write(sc, 31, rf);

	/* enable RF tuning */
	run_rt3070_rf_read(sc, 7, &rf);
	run_rt3070_rf_write(sc, 7, rf | 0x01);
}

static void
run_rt3572_set_chan(struct run_softc *sc, u_int chan)
{
	int8_t txpow1, txpow2;
	uint32_t tmp;
	uint8_t rf;
	int i;

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rt2860_rf2850[i].chan != chan; i++);

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];
	txpow2 = sc->txpow2[i];

	if (chan <= 14) {
		run_bbp_write(sc, 25, sc->bbp25);
		run_bbp_write(sc, 26, sc->bbp26);
	} else {
		/* enable IQ phase correction */
		run_bbp_write(sc, 25, 0x09);
		run_bbp_write(sc, 26, 0xff);
	}

	run_rt3070_rf_write(sc, 2, rt3070_freqs[i].n);
	run_rt3070_rf_write(sc, 3, rt3070_freqs[i].k);
	run_rt3070_rf_read(sc, 6, &rf);
	rf  = (rf & ~0x0f) | rt3070_freqs[i].r;
	rf |= (chan <= 14) ? 0x08 : 0x04;
	run_rt3070_rf_write(sc, 6, rf);

	/* set PLL mode */
	run_rt3070_rf_read(sc, 5, &rf);
	rf &= ~(0x08 | 0x04);
	rf |= (chan <= 14) ? 0x04 : 0x08;
	run_rt3070_rf_write(sc, 5, rf);

	/* set Tx power for chain 0 */
	if (chan <= 14)
		rf = 0x60 | txpow1;
	else
		rf = 0xe0 | (txpow1 & 0xc) << 1 | (txpow1 & 0x3);
	run_rt3070_rf_write(sc, 12, rf);

	/* set Tx power for chain 1 */
	if (chan <= 14)
		rf = 0x60 | txpow2;
	else
		rf = 0xe0 | (txpow2 & 0xc) << 1 | (txpow2 & 0x3);
	run_rt3070_rf_write(sc, 13, rf);

	/* set Tx/Rx streams */
	run_rt3070_rf_read(sc, 1, &rf);
	rf &= ~0xfc;
	if (sc->ntxchains == 1)
		rf |= 1 << 7 | 1 << 5;  /* 1T: disable Tx chains 2 & 3 */
	else if (sc->ntxchains == 2)
		rf |= 1 << 7;           /* 2T: disable Tx chain 3 */
	if (sc->nrxchains == 1)
		rf |= 1 << 6 | 1 << 4;  /* 1R: disable Rx chains 2 & 3 */
	else if (sc->nrxchains == 2)
		rf |= 1 << 6;           /* 2R: disable Rx chain 3 */
	run_rt3070_rf_write(sc, 1, rf);

	/* set RF offset */
	run_rt3070_rf_read(sc, 23, &rf);
	rf = (rf & ~0x7f) | sc->freq;
	run_rt3070_rf_write(sc, 23, rf);

	/* program RF filter */
	rf = sc->rf24_20mhz;
	run_rt3070_rf_write(sc, 24, rf);	/* Tx */
	run_rt3070_rf_write(sc, 31, rf);	/* Rx */

	/* enable RF tuning */
	run_rt3070_rf_read(sc, 7, &rf);
	rf = (chan <= 14) ? 0xd8 : ((rf & ~0xc8) | 0x14);
	run_rt3070_rf_write(sc, 7, rf);

	/* TSSI */
	rf = (chan <= 14) ? 0xc3 : 0xc0;
	run_rt3070_rf_write(sc, 9, rf);

	/* set loop filter 1 */
	run_rt3070_rf_write(sc, 10, 0xf1);
	/* set loop filter 2 */
	run_rt3070_rf_write(sc, 11, (chan <= 14) ? 0xb9 : 0x00);

	/* set tx_mx2_ic */
	run_rt3070_rf_write(sc, 15, (chan <= 14) ? 0x53 : 0x43);
	/* set tx_mx1_ic */
	if (chan <= 14)
		rf = 0x48 | sc->txmixgain_2ghz;
	else
		rf = 0x78 | sc->txmixgain_5ghz;
	run_rt3070_rf_write(sc, 16, rf);

	/* set tx_lo1 */
	run_rt3070_rf_write(sc, 17, 0x23);
	/* set tx_lo2 */
	if (chan <= 14)
		rf = 0x93;
	else if (chan <= 64)
		rf = 0xb7;
	else if (chan <= 128)
		rf = 0x74;
	else
		rf = 0x72;
	run_rt3070_rf_write(sc, 19, rf);

	/* set rx_lo1 */
	if (chan <= 14)
		rf = 0xb3;
	else if (chan <= 64)
		rf = 0xf6;
	else if (chan <= 128)
		rf = 0xf4;
	else
		rf = 0xf3;
	run_rt3070_rf_write(sc, 20, rf);

	/* set pfd_delay */
	if (chan <= 14)
		rf = 0x15;
	else if (chan <= 64)
		rf = 0x3d;
	else
		rf = 0x01;
	run_rt3070_rf_write(sc, 25, rf);

	/* set rx_lo2 */
	run_rt3070_rf_write(sc, 26, (chan <= 14) ? 0x85 : 0x87);
	/* set ldo_rf_vc */
	run_rt3070_rf_write(sc, 27, (chan <= 14) ? 0x00 : 0x01);
	/* set drv_cc */
	run_rt3070_rf_write(sc, 29, (chan <= 14) ? 0x9b : 0x9f);

	run_read(sc, RT2860_GPIO_CTRL, &tmp);
	tmp &= ~0x8080;
	if (chan <= 14)
		tmp |= 0x80;
	run_write(sc, RT2860_GPIO_CTRL, tmp);

	/* enable RF tuning */
	run_rt3070_rf_read(sc, 7, &rf);
	run_rt3070_rf_write(sc, 7, rf | 0x01);

	run_delay(sc, 2);
}

static void
run_set_rx_antenna(struct run_softc *sc, int aux)
{
	uint32_t tmp;

	if (aux) {
		run_mcu_cmd(sc, RT2860_MCU_CMD_ANTSEL, 0);
		run_read(sc, RT2860_GPIO_CTRL, &tmp);
		run_write(sc, RT2860_GPIO_CTRL, (tmp & ~0x0808) | 0x08);
	} else {
		run_mcu_cmd(sc, RT2860_MCU_CMD_ANTSEL, 1);
		run_read(sc, RT2860_GPIO_CTRL, &tmp);
		run_write(sc, RT2860_GPIO_CTRL, tmp & ~0x0808);
	}
}

static int
run_set_chan(struct run_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint32_t chan, group;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return EINVAL;

	if (sc->mac_ver == 0x3572)
		run_rt3572_set_chan(sc, chan);
	else if (sc->mac_ver >= 0x3070)
		run_rt3070_set_chan(sc, chan);
	else
		run_rt2870_set_chan(sc, chan);

	/* determine channel group */
	if (chan <= 14)
		group = 0;
	else if (chan <= 64)
		group = 1;
	else if (chan <= 128)
		group = 2;
	else
		group = 3;

	/* XXX necessary only when group has changed! */
	run_select_chan_group(sc, group);

	run_delay(sc, 10);

	return 0;
}

static void
run_set_channel(struct ieee80211com *ic)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;

	RUN_LOCK(sc);
	run_set_chan(sc, ic->ic_curchan);
	RUN_UNLOCK(sc);

	return;
}

static void
run_scan_start(struct ieee80211com *ic)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;
	uint32_t tmp;

	RUN_LOCK(sc);

	/* abort TSF synchronization */
	run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
	run_write(sc, RT2860_BCN_TIME_CFG,
	    tmp & ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN |
	    RT2860_TBTT_TIMER_EN));
	run_set_bssid(sc, sc->sc_ifp->if_broadcastaddr);

	RUN_UNLOCK(sc);

	return;
}

static void
run_scan_end(struct ieee80211com *ic)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;

	RUN_LOCK(sc);

	run_enable_tsf_sync(sc);
	/* XXX keep local copy */
	run_set_bssid(sc, sc->sc_bssid);

	RUN_UNLOCK(sc);

	return;
}

static uint8_t
run_rate2mcs(uint8_t rate)
{
	switch (rate) {
	/* CCK rates */
	case 2:		return 0;
	case 4:		return 1;
	case 11:	return 2;
	case 22:	return 3;
	/* OFDM rates */
	case 12:	return 0;
	case 18:	return 1;
	case 24:	return 2;
	case 36:	return 3;
	case 48:	return 4;
	case 72:	return 5;
	case 96:	return 6;
	case 108:	return 7;
	}
	return 0;	/* shouldn't get here */
}

static void
run_update_beacon_locked(struct ieee80211vap *vap, int item)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct rt2860_txwi txwi;
	struct mbuf *m;
	int rate;

	if ((m = ieee80211_beacon_alloc(vap->iv_bss, &RUN_VAP(vap)->bo)) == NULL)
	        return;

	memset(&txwi, 0, sizeof txwi);
	txwi.wcid = 0xff;
	txwi.len = htole16(m->m_pkthdr.len);
	/* send beacons at the lowest available rate */
	rate = (ic->ic_curmode == IEEE80211_MODE_11A) ? 12 : 2;
	txwi.phy = htole16(run_rate2mcs(rate));
	if (rate == 12)
	        txwi.phy |= htole16(RT2860_PHY_OFDM);
	txwi.txop = RT2860_TX_TXOP_HT;
	txwi.flags = RT2860_TX_TS;

	run_write_region_1(sc, RT2860_BCN_BASE(0),
	    (u_int8_t *)&txwi, sizeof txwi);
	run_write_region_1(sc, RT2860_BCN_BASE(0) + sizeof txwi,
	    mtod(m, uint8_t *), (m->m_pkthdr.len + 1) & ~1);	/* roundup len */

	m_freem(m);

	return;
}

static void
run_update_beacon(struct ieee80211vap *vap, int item)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct run_softc *sc = ic->ic_ifp->if_softc;

	IEEE80211_UNLOCK(ic);
	RUN_LOCK(sc);
	run_update_beacon_locked(vap, item);
	RUN_UNLOCK(sc);
	IEEE80211_LOCK(ic);

	return;
}

static void
run_updateprot(struct ieee80211com *ic)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;
	uint32_t tmp;

	tmp = RT2860_RTSTH_EN | RT2860_PROT_NAV_SHORT | RT2860_TXOP_ALLOW_ALL;
	/* setup protection frame rate (MCS code) */
	tmp |= (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    rt2860_rates[RT2860_RIDX_OFDM6].mcs :
	    rt2860_rates[RT2860_RIDX_CCK11].mcs;

	/* CCK frames don't require protection */
	run_write(sc, RT2860_CCK_PROT_CFG, tmp);
	if (ic->ic_flags & IEEE80211_F_USEPROT) {
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			tmp |= RT2860_PROT_CTRL_RTS_CTS;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			tmp |= RT2860_PROT_CTRL_CTS;
	}
	run_write(sc, RT2860_OFDM_PROT_CFG, tmp);
}

static void
run_usb_timeout_cb(void *arg, int pending)
{
	struct run_softc *sc = arg;
	struct ieee80211vap *vap = &sc->sc_rvp->vap;

	RUN_LOCK(sc);

	if(vap->iv_state == IEEE80211_S_RUN &&
	    vap->iv_opmode != IEEE80211_M_STA)
		run_reset_livelock(sc);
	else if(vap->iv_state == IEEE80211_S_SCAN){
		DPRINTF("timeout caused by scan\n");
		/* cancel bgscan */
		ieee80211_cancel_scan(vap);
	} else
		DPRINTF("timeout by unknown cause\n");

	RUN_UNLOCK(sc);
}

static void
run_reset_livelock(struct run_softc *sc)
{
	uint32_t tmp;

	/*
	 * In IBSS or HostAP modes (when the hardware sends beacons), the MAC
	 * can run into a livelock and start sending CTS-to-self frames like
	 * crazy if protection is enabled.  Reset MAC/BBP for a while
	 */
	run_read(sc, RT2860_DEBUG, &tmp);
	if((tmp & (1 << 29)) && (tmp & (1 << 7 | 1 << 5))){
		DPRINTF("CTS-to-self livelock detected\n");
		run_write(sc, RT2860_MAC_SYS_CTRL, RT2860_MAC_SRST);
		run_delay(sc, 1);
		run_write(sc, RT2860_MAC_SYS_CTRL,
		    RT2860_MAC_RX_EN | RT2860_MAC_TX_EN);
	}
}

static void
run_update_promisc_locked(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
        uint32_t tmp;

	run_read(sc, RT2860_RX_FILTR_CFG, &tmp);

	tmp |= RT2860_DROP_UC_NOME;
        if (ifp->if_flags & IFF_PROMISC)
		tmp &= ~RT2860_DROP_UC_NOME;

	run_write(sc, RT2860_RX_FILTR_CFG, tmp);

        DPRINTF("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
            "entering" : "leaving");
}

static void
run_update_promisc(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	RUN_LOCK(sc);
	run_update_promisc_locked(ifp);
	RUN_UNLOCK(sc);
}

static void
run_enable_tsf_sync(struct run_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t tmp;

	run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
	tmp &= ~0x1fffff;
	tmp |= vap->iv_bss->ni_intval * 16;
	tmp |= RT2860_TSF_TIMER_EN | RT2860_TBTT_TIMER_EN;

	if (vap->iv_opmode == IEEE80211_M_STA) {
		/*
		 * Local TSF is always updated with remote TSF on beacon
		 * reception.
		 */
		tmp |= 1 << RT2860_TSF_SYNC_MODE_SHIFT;
	} else if (vap->iv_opmode == IEEE80211_M_IBSS) {
	        tmp |= RT2860_BCN_TX_EN;
	        /*
	         * Local TSF is updated with remote TSF on beacon reception
	         * only if the remote TSF is greater than local TSF.
	         */
	        tmp |= 2 << RT2860_TSF_SYNC_MODE_SHIFT;
	} else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
	        tmp |= RT2860_BCN_TX_EN;
	        /* SYNC with nobody */
	        tmp |= 3 << RT2860_TSF_SYNC_MODE_SHIFT;
	} else
		DPRINTF("Enabling TSF failed. undefined opmode\n");

	run_write(sc, RT2860_BCN_TIME_CFG, tmp);
}

static void
run_enable_mrr(struct run_softc *sc)
{
#define CCK(mcs)	(mcs)
#define OFDM(mcs)	(1 << 3 | (mcs))
	run_write(sc, RT2860_LG_FBK_CFG0,
	    OFDM(6) << 28 |	/* 54->48 */
	    OFDM(5) << 24 |	/* 48->36 */
	    OFDM(4) << 20 |	/* 36->24 */
	    OFDM(3) << 16 |	/* 24->18 */
	    OFDM(2) << 12 |	/* 18->12 */
	    OFDM(1) <<  8 |	/* 12-> 9 */
	    OFDM(0) <<  4 |	/*  9-> 6 */
	    OFDM(0));		/*  6-> 6 */

	run_write(sc, RT2860_LG_FBK_CFG1,
	    CCK(2) << 12 |	/* 11->5.5 */
	    CCK(1) <<  8 |	/* 5.5-> 2 */
	    CCK(0) <<  4 |	/*   2-> 1 */
	    CCK(0));		/*   1-> 1 */
#undef OFDM
#undef CCK
}

static void
run_set_txpreamble(struct run_softc *sc)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint32_t tmp;

	run_read(sc, RT2860_AUTO_RSP_CFG, &tmp);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RT2860_CCK_SHORT_EN;
	else
		tmp &= ~RT2860_CCK_SHORT_EN;
	run_write(sc, RT2860_AUTO_RSP_CFG, tmp);
}

static void
run_set_basicrates(struct run_softc *sc)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	/* set basic rates mask */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		run_write(sc, RT2860_LEGACY_BASIC_RATE, 0x003);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		run_write(sc, RT2860_LEGACY_BASIC_RATE, 0x150);
	else	/* 11g */
		run_write(sc, RT2860_LEGACY_BASIC_RATE, 0x15f);
}

static void
run_set_leds(struct run_softc *sc, uint16_t which)
{
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LEDS,
	    which | (sc->leds & 0x7f));
}

static void
run_set_bssid(struct run_softc *sc, const uint8_t *bssid)
{
	run_write(sc, RT2860_MAC_BSSID_DW0,
	    bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24);
	run_write(sc, RT2860_MAC_BSSID_DW1,
	    bssid[4] | bssid[5] << 8);
}

static void
run_set_macaddr(struct run_softc *sc, const uint8_t *addr)
{
	run_write(sc, RT2860_MAC_ADDR_DW0,
	    addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24);
	run_write(sc, RT2860_MAC_ADDR_DW1,
	    addr[4] | addr[5] << 8 | 0xff << 16);
}

/* ARGSUSED */
static void
run_updateslot(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t tmp;

	run_read(sc, RT2860_BKOFF_SLOT_CFG, &tmp);
	tmp &= ~0xff;
	tmp |= (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	run_write(sc, RT2860_BKOFF_SLOT_CFG, tmp);
}

static int8_t
run_rssi2dbm(struct run_softc *sc, uint8_t rssi, uint8_t rxchain)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211_channel *c = ic->ic_curchan;
	int delta;

	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		uint32_t chan = ieee80211_chan2ieee(ic, c);
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

	return -12 - delta - rssi;
}

static int
run_bbp_init(struct run_softc *sc)
{
	int i, error, ntries;
	uint8_t bbp0;

	/* wait for BBP to wake up */
	for (ntries = 0; ntries < 20; ntries++) {
		if ((error = run_bbp_read(sc, 0, &bbp0)) != 0)
			return error;
		if (bbp0 != 0 && bbp0 != 0xff)
			break;
	}
	if (ntries == 20)
		return ETIMEDOUT;

	/* initialize BBP registers to default values */
	for (i = 0; i < nitems(rt2860_def_bbp); i++) {
		run_bbp_write(sc, rt2860_def_bbp[i].reg,
		    rt2860_def_bbp[i].val);
	}

	/* fix BBP84 for RT2860E */
	if (sc->mac_ver == 0x2860 && sc->mac_rev != 0x0101)
		run_bbp_write(sc, 84, 0x19);

	if (sc->mac_ver >= 0x3070) {
		run_bbp_write(sc, 79, 0x13);
		run_bbp_write(sc, 80, 0x05);
		run_bbp_write(sc, 81, 0x33);
	} else if (sc->mac_ver == 0x2860 && sc->mac_rev == 0x0100) {
		run_bbp_write(sc, 69, 0x16);
		run_bbp_write(sc, 73, 0x12);
	}
	return 0;
}

static int
run_rt3070_rf_init(struct run_softc *sc)
{
	uint32_t tmp;
	uint8_t rf, target, bbp4;
	int i;

	run_rt3070_rf_read(sc, 30, &rf);
	/* toggle RF R30 bit 7 */
	run_rt3070_rf_write(sc, 30, rf | 0x80);
	run_delay(sc, 10);
	run_rt3070_rf_write(sc, 30, rf & ~0x80);

	/* initialize RF registers to default value */
	if (sc->mac_ver == 0x3572) {
		for (i = 0; i < nitems(rt3572_def_rf); i++) {
			run_rt3070_rf_write(sc, rt3572_def_rf[i].reg,
			    rt3572_def_rf[i].val);
		}
	} else {
		for (i = 0; i < nitems(rt3070_def_rf); i++) {
			run_rt3070_rf_write(sc, rt3070_def_rf[i].reg,
			    rt3070_def_rf[i].val);
		}
	}

	if (sc->mac_ver == 0x3070) {
		/* change voltage from 1.2V to 1.35V for RT3070 */
		run_read(sc, RT3070_LDO_CFG0, &tmp);
		tmp = (tmp & ~0x0f000000) | 0x0d000000;
		run_write(sc, RT3070_LDO_CFG0, tmp);

	} else if (sc->mac_ver == 0x3071) {
		run_rt3070_rf_read(sc, 6, &rf);
		run_rt3070_rf_write(sc, 6, rf | 0x40);
		run_rt3070_rf_write(sc, 31, 0x14);

		run_read(sc, RT3070_LDO_CFG0, &tmp);
		tmp &= ~0x1f000000;
		if (sc->mac_rev < 0x0211)
			tmp |= 0x0d000000;	/* 1.3V */
		else
			tmp |= 0x01000000;	/* 1.2V */
		run_write(sc, RT3070_LDO_CFG0, tmp);

		/* patch LNA_PE_G1 */
		run_read(sc, RT3070_GPIO_SWITCH, &tmp);
		run_write(sc, RT3070_GPIO_SWITCH, tmp & ~0x20);
	} else if(sc->mac_ver == 0x3572){
		run_rt3070_rf_read(sc, 6, &rf);
		run_rt3070_rf_write(sc, 6, rf | 0x40);

		if (sc->mac_rev < 0x0211){
			/* increase voltage from 1.2V to 1.35V */
			run_read(sc, RT3070_LDO_CFG0, &tmp);
			tmp = (tmp & ~0x0f000000) | 0x0d000000;
			run_write(sc, RT3070_LDO_CFG0, tmp);
		} else {
			/* increase voltage from 1.2V to 1.35V */
			run_read(sc, RT3070_LDO_CFG0, &tmp);
			tmp = (tmp & ~0x1f000000) | 0x0d000000;
			run_write(sc, RT3070_LDO_CFG0, tmp);

			run_delay(sc, 1);	/* wait for 1msec */

			/* decrease voltage back to 1.2V */
			tmp = (tmp & ~0x1f000000) | 0x01000000;
			run_write(sc, RT3070_LDO_CFG0, tmp);
		}
	}

	/* select 20MHz bandwidth */
	run_rt3070_rf_read(sc, 31, &rf);
	run_rt3070_rf_write(sc, 31, rf & ~0x20);

	/* calibrate filter for 20MHz bandwidth */
	sc->rf24_20mhz = 0x1f;	/* default value */
	target = (sc->mac_ver < 0x3071) ? 0x16 : 0x13;
	run_rt3070_filter_calib(sc, 0x07, target, &sc->rf24_20mhz);

	/* select 40MHz bandwidth */
	run_bbp_read(sc, 4, &bbp4);
	run_bbp_write(sc, 4, (bbp4 & ~0x08) | 0x10);
	run_rt3070_rf_read(sc, 31, &rf);
	run_rt3070_rf_write(sc, 31, rf | 0x20);

	/* calibrate filter for 40MHz bandwidth */
	sc->rf24_40mhz = 0x2f;	/* default value */
	target = (sc->mac_ver < 0x3071) ? 0x19 : 0x15;
	run_rt3070_filter_calib(sc, 0x27, target, &sc->rf24_40mhz);

	/* go back to 20MHz bandwidth */
	run_bbp_read(sc, 4, &bbp4);
	run_bbp_write(sc, 4, bbp4 & ~0x18);

	if (sc->mac_ver == 0x3572) {
		/* save default BBP registers 25 and 26 values */
		run_bbp_read(sc, 25, &sc->bbp25);
		run_bbp_read(sc, 26, &sc->bbp26);
	} else if (sc->mac_rev < 0x0211)
		run_rt3070_rf_write(sc, 27, 0x03);

	run_read(sc, RT3070_OPT_14, &tmp);
	run_write(sc, RT3070_OPT_14, tmp | 1);

	if (sc->mac_ver == 0x3070 || sc->mac_ver == 0x3071) {
		run_rt3070_rf_read(sc, 17, &rf);
		rf &= ~RT3070_TX_LO1;
		if ((sc->mac_ver == 0x3070 ||
		     (sc->mac_ver == 0x3071 && sc->mac_rev >= 0x0211)) &&
		    !sc->ext_2ghz_lna)
			rf |= 0x20;	/* fix for long range Rx issue */
		if (sc->txmixgain_2ghz >= 1)
			rf = (rf & ~0x7) | sc->txmixgain_2ghz;
		run_rt3070_rf_write(sc, 17, rf);
	}

	if (sc->mac_rev == 0x3071) {
		run_rt3070_rf_read(sc, 1, &rf);
		rf &= ~(RT3070_RX0_PD | RT3070_TX0_PD);
		rf |= RT3070_RF_BLOCK | RT3070_RX1_PD | RT3070_TX1_PD;
		run_rt3070_rf_write(sc, 1, rf);

		run_rt3070_rf_read(sc, 15, &rf);
		run_rt3070_rf_write(sc, 15, rf & ~RT3070_TX_LO2);

		run_rt3070_rf_read(sc, 20, &rf);
		run_rt3070_rf_write(sc, 20, rf & ~RT3070_RX_LO1);

		run_rt3070_rf_read(sc, 21, &rf);
		run_rt3070_rf_write(sc, 21, rf & ~RT3070_RX_LO2);
	}

	if (sc->mac_ver == 0x3070 || sc->mac_ver == 0x3071) {
		/* fix Tx to Rx IQ glitch by raising RF voltage */
		run_rt3070_rf_read(sc, 27, &rf);
		rf &= ~0x77;
		if (sc->mac_rev < 0x0211)
			rf |= 0x03;
		run_rt3070_rf_write(sc, 27, rf);
	}
	return 0;
}

static int
run_rt3070_filter_calib(struct run_softc *sc, uint8_t init, uint8_t target,
    uint8_t *val)
{
	uint8_t rf22, rf24;
	uint8_t bbp55_pb, bbp55_sb, delta;
	int ntries;

	/* program filter */
	run_rt3070_rf_read(sc, 24, &rf24);
	rf24 = (rf24 & 0xc0) | init;	/* initial filter value */
	run_rt3070_rf_write(sc, 24, rf24);

	/* enable baseband loopback mode */
	run_rt3070_rf_read(sc, 22, &rf22);
	run_rt3070_rf_write(sc, 22, rf22 | 0x01);

	/* set power and frequency of passband test tone */
	run_bbp_write(sc, 24, 0x00);
	for (ntries = 0; ntries < 100; ntries++) {
		/* transmit test tone */
		run_bbp_write(sc, 25, 0x90);
		run_delay(sc, 10);
		/* read received power */
		run_bbp_read(sc, 55, &bbp55_pb);
		if (bbp55_pb != 0)
			break;
	}
	if (ntries == 100)
		return ETIMEDOUT;

	/* set power and frequency of stopband test tone */
	run_bbp_write(sc, 24, 0x06);
	for (ntries = 0; ntries < 100; ntries++) {
		/* transmit test tone */
		run_bbp_write(sc, 25, 0x90);
		run_delay(sc, 10);
		/* read received power */
		run_bbp_read(sc, 55, &bbp55_sb);

		delta = bbp55_pb - bbp55_sb;
		if (delta > target)
			break;

		/* reprogram filter */
		rf24++;
		run_rt3070_rf_write(sc, 24, rf24);
	}
	if (ntries < 100) {
		if (rf24 != init)
			rf24--;	/* backtrack */
		*val = rf24;
		run_rt3070_rf_write(sc, 24, rf24);
	}

	/* restore initial state */
	run_bbp_write(sc, 24, 0x00);

	/* disable baseband loopback mode */
	run_rt3070_rf_read(sc, 22, &rf22);
	run_rt3070_rf_write(sc, 22, rf22 & ~0x01);

	return 0;
}

static void
run_rt3070_rf_setup(struct run_softc *sc)
{
	uint8_t bbp, rf;
	int i;

	if (sc->mac_ver == 0x3572) {
		/* enable DC filter */
		if (sc->mac_rev >= 0x0201)
			run_bbp_write(sc, 103, 0xc0);

		run_bbp_read(sc, 138, &bbp);
		if (sc->ntxchains == 1)
			bbp |= 0x20;	/* turn off DAC1 */
		if (sc->nrxchains == 1)
			bbp &= ~0x02;	/* turn off ADC1 */
		run_bbp_write(sc, 138, bbp);

		if (sc->mac_rev >= 0x0211) {
			/* improve power consumption */
			run_bbp_read(sc, 31, &bbp);
			run_bbp_write(sc, 31, bbp & ~0x03);
		}

		run_rt3070_rf_read(sc, 16, &rf);
		rf = (rf & ~0x07) | sc->txmixgain_2ghz;
		run_rt3070_rf_write(sc, 16, rf);

	} else if (sc->mac_ver == 0x3071) {
		/* enable DC filter */
		if (sc->mac_rev >= 0x0201)
			run_bbp_write(sc, 103, 0xc0);

		run_bbp_read(sc, 138, &bbp);
		if (sc->ntxchains == 1)
			bbp |= 0x20;	/* turn off DAC1 */
		if (sc->nrxchains == 1)
			bbp &= ~0x02;	/* turn off ADC1 */
		run_bbp_write(sc, 138, bbp);

		if (sc->mac_rev >= 0x0211) {
			/* improve power consumption */
			run_bbp_read(sc, 31, &bbp);
			run_bbp_write(sc, 31, bbp & ~0x03);
		}

		run_write(sc, RT2860_TX_SW_CFG1, 0);
		if (sc->mac_rev < 0x0211) {
			run_write(sc, RT2860_TX_SW_CFG2,
			    sc->patch_dac ? 0x2c : 0x0f);
		} else
			run_write(sc, RT2860_TX_SW_CFG2, 0);

	} else if (sc->mac_ver == 0x3070) {
		if (sc->mac_rev >= 0x0201) {
			/* enable DC filter */
			run_bbp_write(sc, 103, 0xc0);

			/* improve power consumption */
			run_bbp_read(sc, 31, &bbp);
			run_bbp_write(sc, 31, bbp & ~0x03);
		}

		if (sc->mac_rev < 0x0211) {
			run_write(sc, RT2860_TX_SW_CFG1, 0);
			run_write(sc, RT2860_TX_SW_CFG2, 0x2c);
		} else
			run_write(sc, RT2860_TX_SW_CFG2, 0);
	}

	/* initialize RF registers from ROM for >=RT3071*/
	if (sc->mac_ver >= 0x3071) {
		for (i = 0; i < 10; i++) {
			if (sc->rf[i].reg == 0 || sc->rf[i].reg == 0xff)
				continue;
			run_rt3070_rf_write(sc, sc->rf[i].reg, sc->rf[i].val);
		}
	}
}

static int
run_txrx_enable(struct run_softc *sc)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint32_t tmp;
	int error, ntries;

	run_write(sc, RT2860_MAC_SYS_CTRL, RT2860_MAC_TX_EN);
	for (ntries = 0; ntries < 200; ntries++) {
		if ((error = run_read(sc, RT2860_WPDMA_GLO_CFG, &tmp)) != 0)
			return error;
		if ((tmp & (RT2860_TX_DMA_BUSY | RT2860_RX_DMA_BUSY)) == 0)
			break;
		run_delay(sc, 50);
	}
	if (ntries == 200)
		return ETIMEDOUT;

	run_delay(sc, 50);

	tmp |= RT2860_RX_DMA_EN | RT2860_TX_DMA_EN | RT2860_TX_WB_DDONE;
	run_write(sc, RT2860_WPDMA_GLO_CFG, tmp);

	/* enable Rx bulk aggregation (set timeout and limit) */
	tmp = RT2860_USB_TX_EN | RT2860_USB_RX_EN | RT2860_USB_RX_AGG_EN |
	    RT2860_USB_RX_AGG_TO(128) | RT2860_USB_RX_AGG_LMT(2);
	run_write(sc, RT2860_USB_DMA_CFG, tmp);

	/* set Rx filter */
	tmp = RT2860_DROP_CRC_ERR | RT2860_DROP_PHY_ERR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2860_DROP_UC_NOME | RT2860_DROP_DUPL |
		    RT2860_DROP_CTS | RT2860_DROP_BA | RT2860_DROP_ACK |
		    RT2860_DROP_VER_ERR | RT2860_DROP_CTRL_RSV |
		    RT2860_DROP_CFACK | RT2860_DROP_CFEND;
		if (ic->ic_opmode == IEEE80211_M_STA)
			tmp |= RT2860_DROP_RTS | RT2860_DROP_PSPOLL;
	}
	run_write(sc, RT2860_RX_FILTR_CFG, tmp);

	run_write(sc, RT2860_MAC_SYS_CTRL,
	    RT2860_MAC_RX_EN | RT2860_MAC_TX_EN);

	return 0;
}

static void
run_init_locked(struct run_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t tmp;
	uint8_t bbp1, bbp3;
	int i;
	int ridx;
	int ntries;

	run_stop(sc);

	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_ASIC_VER_ID, &tmp) != 0)
			goto fail;
		if (tmp != 0 && tmp != 0xffffffff)
			break;
		run_delay(sc, 10);
	}
	if (ntries == 100)
		goto fail;

	for (i = 0; i != RUN_EP_QUEUES; i++)
		run_setup_tx_list(sc, &sc->sc_epq[i]);

	run_set_macaddr(sc, IF_LLADDR(ifp));

	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_WPDMA_GLO_CFG, &tmp) != 0)
			goto fail;
		if ((tmp & (RT2860_TX_DMA_BUSY | RT2860_RX_DMA_BUSY)) == 0)
			break;
		run_delay(sc, 10);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for DMA engine\n");
		goto fail;
	}
	tmp &= 0xff0;
	tmp |= RT2860_TX_WB_DDONE;
	run_write(sc, RT2860_WPDMA_GLO_CFG, tmp);

	/* turn off PME_OEN to solve high-current issue */
	run_read(sc, RT2860_SYS_CTRL, &tmp);
	run_write(sc, RT2860_SYS_CTRL, tmp & ~RT2860_PME_OEN);

	run_write(sc, RT2860_MAC_SYS_CTRL,
	    RT2860_BBP_HRST | RT2860_MAC_SRST);
	run_write(sc, RT2860_USB_DMA_CFG, 0);

	if (run_reset(sc) != 0) {
		device_printf(sc->sc_dev, "could not reset chipset\n");
		goto fail;
	}

	run_write(sc, RT2860_MAC_SYS_CTRL, 0);

	/* init Tx power for all Tx rates (from EEPROM) */
	for (ridx = 0; ridx < 5; ridx++) {
		if (sc->txpow20mhz[ridx] == 0xffffffff)
			continue;
		run_write(sc, RT2860_TX_PWR_CFG(ridx), sc->txpow20mhz[ridx]);
	}

	for (i = 0; i < nitems(rt2870_def_mac); i++)
		run_write(sc, rt2870_def_mac[i].reg, rt2870_def_mac[i].val);
	run_write(sc, RT2860_WMM_AIFSN_CFG, 0x00002273);
	run_write(sc, RT2860_WMM_CWMIN_CFG, 0x00002344);
	run_write(sc, RT2860_WMM_CWMAX_CFG, 0x000034aa);

	if (sc->mac_ver >= 0x3070) {
		/* set delay of PA_PE assertion to 1us (unit of 0.25us) */
		run_write(sc, RT2860_TX_SW_CFG0,
		    4 << RT2860_DLY_PAPE_EN_SHIFT);
	}

	/* wait while MAC is busy */
	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_MAC_STATUS_REG, &tmp) != 0)
			goto fail;
		if (!(tmp & (RT2860_RX_STATUS_BUSY | RT2860_TX_STATUS_BUSY)))
			break;
		run_delay(sc, 10);
	}
	if (ntries == 100)
		goto fail;

	/* clear Host to MCU mailbox */
	run_write(sc, RT2860_H2M_BBPAGENT, 0);
	run_write(sc, RT2860_H2M_MAILBOX, 0);
	run_delay(sc, 10);

	if (run_bbp_init(sc) != 0) {
		device_printf(sc->sc_dev, "could not initialize BBP\n");
		goto fail;
	}

	/* abort TSF synchronization */
	run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
	tmp &= ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN |
	    RT2860_TBTT_TIMER_EN);
	run_write(sc, RT2860_BCN_TIME_CFG, tmp);

	/* clear RX WCID search table */
	run_set_region_4(sc, RT2860_WCID_ENTRY(0), 0, 512);
	/* clear WCID attribute table */
	run_set_region_4(sc, RT2860_WCID_ATTR(0), 0, 8 * 32);
	/* clear shared key table */
	run_set_region_4(sc, RT2860_SKEY(0, 0), 0, 8 * 32);
	/* clear shared key mode */
	run_set_region_4(sc, RT2860_SKEY_MODE_0_7, 0, 4);

	run_read(sc, RT2860_US_CYC_CNT, &tmp);
	tmp = (tmp & ~0xff) | 0x1e;
	run_write(sc, RT2860_US_CYC_CNT, tmp);

	if (sc->mac_rev != 0x0101)
		run_write(sc, RT2860_TXOP_CTRL_CFG, 0x0000583f);

	run_write(sc, RT2860_WMM_TXOP0_CFG, 0);
	run_write(sc, RT2860_WMM_TXOP1_CFG, 48 << 16 | 96);

	/* write vendor-specific BBP values (from EEPROM) */
	for (i = 0; i < 8; i++) {
		if (sc->bbp[i].reg == 0 || sc->bbp[i].reg == 0xff)
			continue;
		run_bbp_write(sc, sc->bbp[i].reg, sc->bbp[i].val);
	}

	/* select Main antenna for 1T1R devices */
	if (sc->rf_rev == RT3070_RF_3020)
		run_set_rx_antenna(sc, 0);

	/* send LEDs operating mode to microcontroller */
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LED1, sc->led[0]);
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LED2, sc->led[1]);
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LED3, sc->led[2]);

	if (sc->mac_ver >= 0x3070)
		run_rt3070_rf_init(sc);

	/* disable non-existing Rx chains */
	run_bbp_read(sc, 3, &bbp3);
	bbp3 &= ~(1 << 3 | 1 << 4);
	if (sc->nrxchains == 2)
		bbp3 |= 1 << 3;
	else if (sc->nrxchains == 3)
		bbp3 |= 1 << 4;
	run_bbp_write(sc, 3, bbp3);

	/* disable non-existing Tx chains */
	run_bbp_read(sc, 1, &bbp1);
	if (sc->ntxchains == 1)
		bbp1 &= ~(1 << 3 | 1 << 4);
	run_bbp_write(sc, 1, bbp1);

	if (sc->mac_ver >= 0x3070)
		run_rt3070_rf_setup(sc);

	/* select default channel */
	run_set_chan(sc, ic->ic_curchan);

	/* setup initial protection mode */
	run_updateprot(ic);

	/* turn radio LED on */
	run_set_leds(sc, RT2860_LED_RADIO);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	for(i = 0; i != RUN_N_XFER; i++)
		usbd_xfer_set_stall(sc->sc_xfer[i]);

	usbd_transfer_start(sc->sc_xfer[RUN_BULK_RX]);

	if (run_txrx_enable(sc) != 0)
		goto fail;

	return;

fail:
	run_stop(sc);
}

static void
run_init(void *arg)
{
	struct run_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	RUN_LOCK(sc);
	run_init_locked(sc);
	RUN_UNLOCK(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
	        ieee80211_start_all(ic);
}

static void
run_stop(void *arg)
{
	struct run_softc *sc = (struct run_softc *)arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t tmp;
	int i;
	int ntries;

	RUN_LOCK_ASSERT(sc, MA_OWNED);

	if(sc->sc_rvp != NULL){
		sc->sc_rvp->ratectl_run = RUN_RATECTL_OFF;
		if (ic->ic_flags & IEEE80211_F_SCAN)
			ieee80211_cancel_scan(&sc->sc_rvp->vap);
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		run_set_leds(sc, 0);	/* turn all LEDs off */

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	RUN_UNLOCK(sc);

	for(i = 0; i < RUN_N_XFER; i++)
		usbd_transfer_drain(sc->sc_xfer[i]);

	RUN_LOCK(sc);

	if(sc->rx_m != NULL){
		m_free(sc->rx_m);
		sc->rx_m = NULL;
	}

	/* disable Tx/Rx */
	run_read(sc, RT2860_MAC_SYS_CTRL, &tmp);
	tmp &= ~(RT2860_MAC_RX_EN | RT2860_MAC_TX_EN);
	run_write(sc, RT2860_MAC_SYS_CTRL, tmp);

	/* wait for pending Tx to complete */
	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_TXRXQ_PCNT, &tmp) != 0){
			DPRINTF("Cannot read Tx queue count\n");
			break;
		}
		if ((tmp & RT2860_TX2Q_PCNT_MASK) == 0){
			DPRINTF("All Tx cleared\n");
			break;
		}
		run_delay(sc, 10);
	}
	if(ntries >= 100)
		DPRINTF("There are still pending Tx\n");
	run_delay(sc, 10);
	run_write(sc, RT2860_USB_DMA_CFG, 0);

	run_write(sc, RT2860_MAC_SYS_CTRL, RT2860_BBP_HRST | RT2860_MAC_SRST);
	run_write(sc, RT2860_MAC_SYS_CTRL, 0);

	for (i = 0; i != RUN_EP_QUEUES; i++)
		run_unsetup_tx_list(sc, &sc->sc_epq[i]);

	return;
}

static void
run_delay(struct run_softc *sc, unsigned int ms)
{
	usb_pause_mtx(mtx_owned(&sc->sc_mtx) ? 
	    &sc->sc_mtx : NULL, USB_MS_TO_TICKS(ms));
}

static device_method_t run_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		run_match),
	DEVMETHOD(device_attach,	run_attach),
	DEVMETHOD(device_detach,	run_detach),

	{ 0, 0 }
};

static driver_t run_driver = {
	"run",
	run_methods,
	sizeof(struct run_softc)
};

static devclass_t run_devclass;

DRIVER_MODULE(run, uhub, run_driver, run_devclass, NULL, 0);
