/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2005, 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
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
 * Ralink Technology RT2500USB chipset driver
 * http://www.ralinktech.com/
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
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/if_uralreg.h>
#include <dev/usb/if_uralvar.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (uraldebug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (uraldebug >= (n)) printf x; } while (0)
int uraldebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ural, CTLFLAG_RW, 0, "USB ural");
SYSCTL_INT(_hw_usb_ural, OID_AUTO, debug, CTLFLAG_RW, &uraldebug, 0,
    "ural debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

#define URAL_RSSI(rssi)					\
	((rssi) > (RAL_NOISE_FLOOR + RAL_RSSI_CORR) ?	\
	 ((rssi) - (RAL_NOISE_FLOOR + RAL_RSSI_CORR)) : 0)

/* various supported device vendors/products */
static const struct usb_devno ural_devs[] = {
	{ USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_WL167G },
	{ USB_VENDOR_ASUS,		USB_PRODUCT_RALINK_RT2570 },
	{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D7050 },
	{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D7051 },
	{ USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_C54RU },
	{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DWLG122 },
	{ USB_VENDOR_GIGABYTE,		USB_PRODUCT_GIGABYTE_GNWBKG },
	{ USB_VENDOR_GIGABYTE,		USB_PRODUCT_GIGABYTE_GN54G },
	{ USB_VENDOR_GUILLEMOT,		USB_PRODUCT_GUILLEMOT_HWGUSB254 },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_WUSB54G },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_WUSB54GP },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_HU200TS },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_KG54 },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_KG54AI },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_KG54YB },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_NINWIFI },
	{ USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT2570 },
	{ USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT2570_2 },
	{ USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT2570_3 },
	{ USB_VENDOR_NOVATECH,		USB_PRODUCT_NOVATECH_NV902 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2570 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2570_2 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2570_3 },
	{ USB_VENDOR_SIEMENS2,		USB_PRODUCT_SIEMENS2_WL54G },
	{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2862WG },
	{ USB_VENDOR_SPHAIRON,		USB_PRODUCT_SPHAIRON_UB801R},
	{ USB_VENDOR_SURECOM,		USB_PRODUCT_SURECOM_RT2570 },
	{ USB_VENDOR_VTECH,		USB_PRODUCT_VTECH_RT2570 },
	{ USB_VENDOR_ZINWELL,		USB_PRODUCT_ZINWELL_RT2570 }
};

MODULE_DEPEND(ural, wlan, 1, 1, 1);
MODULE_DEPEND(ural, wlan_amrr, 1, 1, 1);
MODULE_DEPEND(ural, usb, 1, 1, 1);

static struct ieee80211vap *ural_vap_create(struct ieee80211com *,
			    const char name[IFNAMSIZ], int unit, int opmode,
			    int flags, const uint8_t bssid[IEEE80211_ADDR_LEN],
			    const uint8_t mac[IEEE80211_ADDR_LEN]);
static void		ural_vap_delete(struct ieee80211vap *);
static int		ural_alloc_tx_list(struct ural_softc *);
static void		ural_free_tx_list(struct ural_softc *);
static int		ural_alloc_rx_list(struct ural_softc *);
static void		ural_free_rx_list(struct ural_softc *);
static void		ural_task(void *);
static void		ural_scantask(void *);
static int		ural_newstate(struct ieee80211vap *,
			    enum ieee80211_state, int);
static void		ural_txeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		ural_rxeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		ural_setup_tx_desc(struct ural_softc *,
			    struct ural_tx_desc *, uint32_t, int, int);
static int		ural_tx_bcn(struct ural_softc *, struct mbuf *,
			    struct ieee80211_node *);
static int		ural_tx_mgt(struct ural_softc *, struct mbuf *,
			    struct ieee80211_node *);
static int		ural_tx_data(struct ural_softc *, struct mbuf *,
			    struct ieee80211_node *);
static void		ural_start(struct ifnet *);
static void		ural_watchdog(void *);
static int		ural_ioctl(struct ifnet *, u_long, caddr_t);
static void		ural_set_testmode(struct ural_softc *);
static void		ural_eeprom_read(struct ural_softc *, uint16_t, void *,
			    int);
static uint16_t		ural_read(struct ural_softc *, uint16_t);
static void		ural_read_multi(struct ural_softc *, uint16_t, void *,
			    int);
static void		ural_write(struct ural_softc *, uint16_t, uint16_t);
static void		ural_write_multi(struct ural_softc *, uint16_t, void *,
			    int) __unused;
static void		ural_bbp_write(struct ural_softc *, uint8_t, uint8_t);
static uint8_t		ural_bbp_read(struct ural_softc *, uint8_t);
static void		ural_rf_write(struct ural_softc *, uint8_t, uint32_t);
static struct ieee80211_node *ural_node_alloc(struct ieee80211_node_table *);
static void		ural_newassoc(struct ieee80211_node *, int);
static void		ural_scan_start(struct ieee80211com *);
static void		ural_scan_end(struct ieee80211com *);
static void		ural_set_channel(struct ieee80211com *);
static void		ural_set_chan(struct ural_softc *,
			    struct ieee80211_channel *);
static void		ural_disable_rf_tune(struct ural_softc *);
static void		ural_enable_tsf_sync(struct ural_softc *);
static void		ural_update_slot(struct ifnet *);
static void		ural_set_txpreamble(struct ural_softc *);
static void		ural_set_basicrates(struct ural_softc *,
			    const struct ieee80211_channel *);
static void		ural_set_bssid(struct ural_softc *, const uint8_t *);
static void		ural_set_macaddr(struct ural_softc *, uint8_t *);
static void		ural_update_promisc(struct ural_softc *);
static const char	*ural_get_rf(int);
static void		ural_read_eeprom(struct ural_softc *);
static int		ural_bbp_init(struct ural_softc *);
static void		ural_set_txantenna(struct ural_softc *, int);
static void		ural_set_rxantenna(struct ural_softc *, int);
static void		ural_init_locked(struct ural_softc *);
static void		ural_init(void *);
static void		ural_stop(void *);
static int		ural_raw_xmit(struct ieee80211_node *, struct mbuf *,
			    const struct ieee80211_bpf_params *);
static void		ural_amrr_start(struct ural_softc *,
			    struct ieee80211_node *);
static void		ural_amrr_timeout(void *);
static void		ural_amrr_update(usbd_xfer_handle, usbd_private_handle,
			    usbd_status status);

/*
 * Default values for MAC registers; values taken from the reference driver.
 */
static const struct {
	uint16_t	reg;
	uint16_t	val;
} ural_def_mac[] = {
	{ RAL_TXRX_CSR5,  0x8c8d },
	{ RAL_TXRX_CSR6,  0x8b8a },
	{ RAL_TXRX_CSR7,  0x8687 },
	{ RAL_TXRX_CSR8,  0x0085 },
	{ RAL_MAC_CSR13,  0x1111 },
	{ RAL_MAC_CSR14,  0x1e11 },
	{ RAL_TXRX_CSR21, 0xe78f },
	{ RAL_MAC_CSR9,   0xff1d },
	{ RAL_MAC_CSR11,  0x0002 },
	{ RAL_MAC_CSR22,  0x0053 },
	{ RAL_MAC_CSR15,  0x0000 },
	{ RAL_MAC_CSR8,   0x0780 },
	{ RAL_TXRX_CSR19, 0x0000 },
	{ RAL_TXRX_CSR18, 0x005a },
	{ RAL_PHY_CSR2,   0x0000 },
	{ RAL_TXRX_CSR0,  0x1ec0 },
	{ RAL_PHY_CSR4,   0x000f }
};

/*
 * Default values for BBP registers; values taken from the reference driver.
 */
static const struct {
	uint8_t	reg;
	uint8_t	val;
} ural_def_bbp[] = {
	{  3, 0x02 },
	{  4, 0x19 },
	{ 14, 0x1c },
	{ 15, 0x30 },
	{ 16, 0xac },
	{ 17, 0x48 },
	{ 18, 0x18 },
	{ 19, 0xff },
	{ 20, 0x1e },
	{ 21, 0x08 },
	{ 22, 0x08 },
	{ 23, 0x08 },
	{ 24, 0x80 },
	{ 25, 0x50 },
	{ 26, 0x08 },
	{ 27, 0x23 },
	{ 30, 0x10 },
	{ 31, 0x2b },
	{ 32, 0xb9 },
	{ 34, 0x12 },
	{ 35, 0x50 },
	{ 39, 0xc4 },
	{ 40, 0x02 },
	{ 41, 0x60 },
	{ 53, 0x10 },
	{ 54, 0x18 },
	{ 56, 0x08 },
	{ 57, 0x10 },
	{ 58, 0x08 },
	{ 61, 0x60 },
	{ 62, 0x10 },
	{ 75, 0xff }
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
static const struct {
	uint8_t		chan;
	uint32_t	r1;
	uint32_t	r2;
	uint32_t	r4;
} ural_rf5222[] = {
	{   1, 0x08808, 0x0044d, 0x00282 },
	{   2, 0x08808, 0x0044e, 0x00282 },
	{   3, 0x08808, 0x0044f, 0x00282 },
	{   4, 0x08808, 0x00460, 0x00282 },
	{   5, 0x08808, 0x00461, 0x00282 },
	{   6, 0x08808, 0x00462, 0x00282 },
	{   7, 0x08808, 0x00463, 0x00282 },
	{   8, 0x08808, 0x00464, 0x00282 },
	{   9, 0x08808, 0x00465, 0x00282 },
	{  10, 0x08808, 0x00466, 0x00282 },
	{  11, 0x08808, 0x00467, 0x00282 },
	{  12, 0x08808, 0x00468, 0x00282 },
	{  13, 0x08808, 0x00469, 0x00282 },
	{  14, 0x08808, 0x0046b, 0x00286 },

	{  36, 0x08804, 0x06225, 0x00287 },
	{  40, 0x08804, 0x06226, 0x00287 },
	{  44, 0x08804, 0x06227, 0x00287 },
	{  48, 0x08804, 0x06228, 0x00287 },
	{  52, 0x08804, 0x06229, 0x00287 },
	{  56, 0x08804, 0x0622a, 0x00287 },
	{  60, 0x08804, 0x0622b, 0x00287 },
	{  64, 0x08804, 0x0622c, 0x00287 },

	{ 100, 0x08804, 0x02200, 0x00283 },
	{ 104, 0x08804, 0x02201, 0x00283 },
	{ 108, 0x08804, 0x02202, 0x00283 },
	{ 112, 0x08804, 0x02203, 0x00283 },
	{ 116, 0x08804, 0x02204, 0x00283 },
	{ 120, 0x08804, 0x02205, 0x00283 },
	{ 124, 0x08804, 0x02206, 0x00283 },
	{ 128, 0x08804, 0x02207, 0x00283 },
	{ 132, 0x08804, 0x02208, 0x00283 },
	{ 136, 0x08804, 0x02209, 0x00283 },
	{ 140, 0x08804, 0x0220a, 0x00283 },

	{ 149, 0x08808, 0x02429, 0x00281 },
	{ 153, 0x08808, 0x0242b, 0x00281 },
	{ 157, 0x08808, 0x0242d, 0x00281 },
	{ 161, 0x08808, 0x0242f, 0x00281 }
};

static device_probe_t ural_match;
static device_attach_t ural_attach;
static device_detach_t ural_detach;

static device_method_t ural_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ural_match),
	DEVMETHOD(device_attach,	ural_attach),
	DEVMETHOD(device_detach,	ural_detach),

	{ 0, 0 }
};

static driver_t ural_driver = {
	"ural",
	ural_methods,
	sizeof(struct ural_softc)
};

static devclass_t ural_devclass;

DRIVER_MODULE(ural, uhub, ural_driver, ural_devclass, usbd_driver_load, 0);

static int
ural_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(ural_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

static int
ural_attach(device_t self)
{
	struct ural_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct ifnet *ifp;
	struct ieee80211com *ic;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	int i;
	uint8_t bands;

	sc->sc_udev = uaa->device;
	sc->sc_dev = self;

	if (usbd_set_config_no(sc->sc_udev, RAL_CONFIG_NO, 0) != 0) {
		device_printf(self, "could not set configuration no\n");
		return ENXIO;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, RAL_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		device_printf(self, "could not get interface handle\n");
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
			device_printf(self, "no endpoint descriptor for %d\n",
			    i);
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
		device_printf(self, "missing endpoint\n");
		return ENXIO;
	}

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		return ENXIO;
	}
	ic = ifp->if_l2com;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	usb_init_task(&sc->sc_task, ural_task, sc);
	usb_init_task(&sc->sc_scantask, ural_scantask, sc);
	callout_init(&sc->watchdog_ch, 0);

	/* retrieve RT2570 rev. no */
	sc->asic_rev = ural_read(sc, RAL_MAC_CSR0);

	/* retrieve MAC address and various other things from EEPROM */
	ural_read_eeprom(sc);

	device_printf(sc->sc_dev, "MAC/BBP RT2570 (rev 0x%02x), RF %s\n",
	    sc->asic_rev, ural_get_rf(sc->rf_rev));

	ifp->if_softc = sc;
	if_initname(ifp, "ural", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT; /* USB stack is still under Giant lock */
	ifp->if_init = ural_init;
	ifp->if_ioctl = ural_ioctl;
	ifp->if_start = ural_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */

	/* set device capabilities */
	ic->ic_caps =
	      IEEE80211_C_IBSS		/* IBSS mode supported */
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
	if (sc->rf_rev == RAL_RF_5222)
		setbit(&bands, IEEE80211_MODE_11A);
	ieee80211_init_channels(ic, NULL, &bands);

	ieee80211_ifattach(ic);
	ic->ic_newassoc = ural_newassoc;
	ic->ic_raw_xmit = ural_raw_xmit;
	ic->ic_node_alloc = ural_node_alloc;
	ic->ic_scan_start = ural_scan_start;
	ic->ic_scan_end = ural_scan_end;
	ic->ic_set_channel = ural_set_channel;

	ic->ic_vap_create = ural_vap_create;
	ic->ic_vap_delete = ural_vap_delete;

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);

	bpfattach(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + sizeof(sc->sc_txtap));

	sc->sc_rxtap_len = sizeof sc->sc_rxtap;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RAL_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtap;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RAL_TX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;
}

static int
ural_detach(device_t self)
{
	struct ural_softc *sc = device_get_softc(self);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	ural_stop(sc);
	bpfdetach(ifp);
	ieee80211_ifdetach(ic);

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	usb_rem_task(sc->sc_udev, &sc->sc_scantask);
	callout_stop(&sc->watchdog_ch);

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

	ural_free_rx_list(sc);
	ural_free_tx_list(sc);

	if_free(ifp);
	mtx_destroy(&sc->sc_mtx);

	return 0;
}

static struct ieee80211vap *
ural_vap_create(struct ieee80211com *ic,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ural_vap *uvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return NULL;
	uvp = (struct ural_vap *) malloc(sizeof(struct ural_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (uvp == NULL)
		return NULL;
	vap = &uvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = ural_newstate;

	callout_init(&uvp->amrr_ch, 0);
	ieee80211_amrr_init(&uvp->amrr, vap,
	    IEEE80211_AMRR_MIN_SUCCESS_THRESHOLD,
	    IEEE80211_AMRR_MAX_SUCCESS_THRESHOLD,
	    1000 /* 1 sec */);

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change, ieee80211_media_status);
	ic->ic_opmode = opmode;
	return vap;
}

static void
ural_vap_delete(struct ieee80211vap *vap)
{
	struct ural_vap *uvp = URAL_VAP(vap);

	callout_stop(&uvp->amrr_ch);
	ieee80211_amrr_cleanup(&uvp->amrr);
	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

static int
ural_alloc_tx_list(struct ural_softc *sc)
{
	struct ural_tx_data *data;
	int i, error;

	sc->tx_queued = sc->tx_cur = 0;

	for (i = 0; i < RAL_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate tx xfer\n");
			error = ENOMEM;
			goto fail;
		}

		data->buf = usbd_alloc_buffer(data->xfer,
		    RAL_TX_DESC_SIZE + MCLBYTES);
		if (data->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate tx buffer\n");
			error = ENOMEM;
			goto fail;
		}
	}

	return 0;

fail:	ural_free_tx_list(sc);
	return error;
}

static void
ural_free_tx_list(struct ural_softc *sc)
{
	struct ural_tx_data *data;
	int i;

	for (i = 0; i < RAL_TX_LIST_COUNT; i++) {
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
ural_alloc_rx_list(struct ural_softc *sc)
{
	struct ural_rx_data *data;
	int i, error;

	for (i = 0; i < RAL_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx xfer\n");
			error = ENOMEM;
			goto fail;
		}

		if (usbd_alloc_buffer(data->xfer, MCLBYTES) == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx buffer\n");
			error = ENOMEM;
			goto fail;
		}

		data->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (data->m == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		data->buf = mtod(data->m, uint8_t *);
	}

	return 0;

fail:	ural_free_tx_list(sc);
	return error;
}

static void
ural_free_rx_list(struct ural_softc *sc)
{
	struct ural_rx_data *data;
	int i;

	for (i = 0; i < RAL_RX_LIST_COUNT; i++) {
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

static void
ural_task(void *xarg)
{
	struct ural_softc *sc = xarg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ural_vap *uvp = URAL_VAP(vap);
	const struct ieee80211_txparam *tp;
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	struct mbuf *m;

	ostate = vap->iv_state;

	RAL_LOCK(sc);
	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			ural_write(sc, RAL_TXRX_CSR19, 0);

			/* force tx led to stop blinking */
			ural_write(sc, RAL_MAC_CSR20, 0);
		}
		break;

	case IEEE80211_S_RUN:
		ni = vap->iv_bss;

		if (vap->iv_opmode != IEEE80211_M_MONITOR) {
			ural_update_slot(ic->ic_ifp);
			ural_set_txpreamble(sc);
			ural_set_basicrates(sc, ic->ic_bsschan);
			ural_set_bssid(sc, ni->ni_bssid);
		}

		if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
		    vap->iv_opmode == IEEE80211_M_IBSS) {
			m = ieee80211_beacon_alloc(ni, &uvp->bo);
			if (m == NULL) {
				device_printf(sc->sc_dev,
				    "could not allocate beacon\n");
				return;
			}

			if (ural_tx_bcn(sc, m, ni) != 0) {
				device_printf(sc->sc_dev,
				    "could not send beacon\n");
				return;
			}
		}

		/* make tx led blink on tx (controlled by ASIC) */
		ural_write(sc, RAL_MAC_CSR20, 1);

		if (vap->iv_opmode != IEEE80211_M_MONITOR)
			ural_enable_tsf_sync(sc);

		/* enable automatic rate adaptation */
		tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_bsschan)];
		if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE)
			ural_amrr_start(sc, ni);

		break;

	default:
		break;
	}

	RAL_UNLOCK(sc);

	IEEE80211_LOCK(ic);
	uvp->newstate(vap, sc->sc_state, sc->sc_arg);
	if (vap->iv_newstate_cb != NULL)
		vap->iv_newstate_cb(vap, sc->sc_state, sc->sc_arg);
	IEEE80211_UNLOCK(ic);
}

static void
ural_scantask(void *arg)
{
	struct ural_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	RAL_LOCK(sc);
	if (sc->sc_scan_action == URAL_SCAN_START) {
		/* abort TSF synchronization */
		ural_write(sc, RAL_TXRX_CSR19, 0);
		ural_set_bssid(sc, ifp->if_broadcastaddr);
	} else if (sc->sc_scan_action == URAL_SET_CHANNEL) {
		mtx_lock(&Giant);
		ural_set_chan(sc, ic->ic_curchan);
		mtx_unlock(&Giant);
	} else {
		ural_enable_tsf_sync(sc);
		/* XXX keep local copy */
		ural_set_bssid(sc, vap->iv_bss->ni_bssid);
	} 
	RAL_UNLOCK(sc);
}

static int
ural_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ural_vap *uvp = URAL_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct ural_softc *sc = ic->ic_ifp->if_softc;

	callout_stop(&uvp->amrr_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	if (nstate == IEEE80211_S_INIT) {
		uvp->newstate(vap, nstate, arg);
		return 0;
	} else {
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);
		return EINPROGRESS;
	}
}

#define RAL_RXTX_TURNAROUND	5	/* us */

static void
ural_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ural_tx_data *data = priv;
	struct ural_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;

	if (data->m->m_flags & M_TXCB)
		ieee80211_process_callback(data->ni, data->m,
			status == USBD_NORMAL_COMPLETION ? 0 : ETIMEDOUT);
	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		device_printf(sc->sc_dev, "could not transmit buffer: %s\n",
		    usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipeh);

		ifp->if_oerrors++;
		/* XXX mbuf leak? */
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
	ural_start(ifp);
}

static void
ural_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ural_rx_data *data = priv;
	struct ural_softc *sc = data->sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ural_rx_desc *desc;
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

	if (len < RAL_RX_DESC_SIZE + IEEE80211_MIN_LEN) {
		DPRINTF(("%s: xfer too short %d\n", device_get_nameunit(sc->sc_dev),
		    len));
		ifp->if_ierrors++;
		goto skip;
	}

	/* rx descriptor is located at the end */
	desc = (struct ural_rx_desc *)(data->buf + len - RAL_RX_DESC_SIZE);

	if ((le32toh(desc->flags) & RAL_RX_PHY_ERROR) ||
	    (le32toh(desc->flags) & RAL_RX_CRC_ERROR)) {
		/*
		 * This should not happen since we did not request to receive
		 * those frames when we filled RAL_TXRX_CSR2.
		 */
		DPRINTFN(5, ("PHY or CRC error\n"));
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
	m->m_pkthdr.len = m->m_len = (le32toh(desc->flags) >> 16) & 0xfff;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct ural_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;   
		tap->wr_rate = ieee80211_plcp2rate(desc->rate,
		    le32toh(desc->flags) & RAL_RX_OFDM);
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_antenna = sc->rx_ant;
		tap->wr_antsignal = URAL_RSSI(desc->rssi);

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
	}

	/* Strip trailing 802.11 MAC FCS. */
	m_adj(m, -IEEE80211_CRC_LEN);

	rssi = URAL_RSSI(desc->rssi);
	ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));
	if (ni != NULL) {
		(void) ieee80211_input(ni, m, rssi, RAL_NOISE_FLOOR, 0);
		ieee80211_free_node(ni);
	} else
		(void) ieee80211_input_all(ic, m, rssi, RAL_NOISE_FLOOR, 0);

	DPRINTFN(15, ("rx done\n"));

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->sc_rx_pipeh, data, data->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, ural_rxeof);
	usbd_transfer(xfer);
}

static void
ural_setup_tx_desc(struct ural_softc *sc, struct ural_tx_desc *desc,
    uint32_t flags, int len, int rate)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t plcp_length;
	int remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(RAL_TX_NEWSEQ);
	desc->flags |= htole32(len << 16);

	desc->wme = htole16(RAL_AIFSN(2) | RAL_LOGCWMIN(3) | RAL_LOGCWMAX(5));
	desc->wme |= htole16(RAL_IVOFFSET(sizeof (struct ieee80211_frame)));

	/* setup PLCP fields */
	desc->plcp_signal  = ieee80211_rate2plcp(rate);
	desc->plcp_service = 4;

	len += IEEE80211_CRC_LEN;
	if (ieee80211_rate2phytype(sc->sc_rates, rate) == IEEE80211_T_OFDM) {
		desc->flags |= htole32(RAL_TX_OFDM);

		plcp_length = len & 0xfff;
		desc->plcp_length_hi = plcp_length >> 6;
		desc->plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = (16 * len + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if (remainder != 0 && remainder < 7)
				desc->plcp_service |= RAL_PLCP_LENGEXT;
		}
		desc->plcp_length_hi = plcp_length >> 8;
		desc->plcp_length_lo = plcp_length & 0xff;

		if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			desc->plcp_signal |= 0x08;
	}

	desc->iv = 0;
	desc->eiv = 0;
}

#define RAL_TX_TIMEOUT	5000

static int
ural_tx_bcn(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_txparam *tp;
	struct ural_tx_desc *desc;
	usbd_xfer_handle xfer;
	uint8_t cmd;
	usbd_status error;
	uint8_t *buf;
	int xferlen;

	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_bsschan)];

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return ENOMEM;

	/* xfer length needs to be a multiple of two! */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	buf = usbd_alloc_buffer(xfer, xferlen);
	if (buf == NULL) {
		usbd_free_xfer(xfer);
		return ENOMEM;
	}

	cmd = 0;
	usbd_setup_xfer(xfer, sc->sc_tx_pipeh, NULL, &cmd, sizeof cmd,
	    USBD_FORCE_SHORT_XFER, RAL_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	if (error != 0) {
		usbd_free_xfer(xfer);
		return error;
	}

	desc = (struct ural_tx_desc *)buf;

	m_copydata(m0, 0, m0->m_pkthdr.len, buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, RAL_TX_IFS_NEWBACKOFF | RAL_TX_TIMESTAMP,
	    m0->m_pkthdr.len, tp->mgmtrate);

	DPRINTFN(10, ("sending beacon frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, tp->mgmtrate, xferlen));

	usbd_setup_xfer(xfer, sc->sc_tx_pipeh, NULL, buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	usbd_free_xfer(xfer);

	return error;
}

static int
ural_tx_mgt(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = sc->sc_ifp;
	const struct ieee80211_txparam *tp;
	struct ural_tx_desc *desc;
	struct ural_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	uint32_t flags;
	uint16_t dur;
	usbd_status error;
	int xferlen;

	data = &sc->tx_data[sc->tx_cur];
	desc = (struct ural_tx_desc *)data->buf;

	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];

	wh = mtod(m0, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}
		wh = mtod(m0, struct ieee80211_frame *);
	}

	data->m = m0;
	data->ni = ni;

	flags = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;

		dur = ieee80211_ack_duration(sc->sc_rates, tp->mgmtrate, 
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT &&
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= RAL_TX_TIMESTAMP;
	}

	if (bpf_peers_present(ifp->if_bpf)) {
		struct ural_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = tp->mgmtrate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, tp->mgmtrate);

	/* align end on a 2-bytes boundary */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending mgt frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, tp->mgmtrate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf,
	    xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT,
	    ural_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		data->m = NULL;
		data->ni = NULL;
		return error;
	}

	sc->tx_queued++;
	sc->tx_cur = (sc->tx_cur + 1) % RAL_TX_LIST_COUNT;

	return 0;
}

static int
ural_sendprot(struct ural_softc *sc,
    const struct mbuf *m, struct ieee80211_node *ni, int prot, int rate)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_frame *wh;
	struct ural_tx_desc *desc;
	struct ural_tx_data *data;
	struct mbuf *mprot;
	int protrate, ackrate, pktlen, flags, isshort;
	uint16_t dur;
	usbd_status error;

	KASSERT(prot == IEEE80211_PROT_RTSCTS || prot == IEEE80211_PROT_CTSONLY,
	    ("protection %d", prot));

	wh = mtod(m, const struct ieee80211_frame *);
	pktlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;

	protrate = ieee80211_ctl_rate(sc->sc_rates, rate);
	ackrate = ieee80211_ack_rate(sc->sc_rates, rate);

	isshort = (ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0;
	dur = ieee80211_compute_duration(sc->sc_rates, pktlen, rate, isshort);
	    + ieee80211_ack_duration(sc->sc_rates, rate, isshort);
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
		/* XXX stat + msg */
		return ENOBUFS;
	}
	data = &sc->tx_data[sc->tx_cur];
	desc = (struct ural_tx_desc *)data->buf;

	data->m = mprot;
	data->ni = ieee80211_ref_node(ni);
	m_copydata(mprot, 0, mprot->m_pkthdr.len, data->buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, flags, mprot->m_pkthdr.len, protrate);

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf,
	    /* NB: no roundup necessary */
	    RAL_TX_DESC_SIZE + mprot->m_pkthdr.len,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT, ural_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		data->m = NULL;
		data->ni = NULL;
		return error;
	}

	sc->tx_queued++;
	sc->tx_cur = (sc->tx_cur + 1) % RAL_TX_LIST_COUNT;

	return 0;
}

static int
ural_tx_raw(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    const struct ieee80211_bpf_params *params)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ural_tx_desc *desc;
	struct ural_tx_data *data;
	uint32_t flags;
	usbd_status error;
	int xferlen, rate;

	KASSERT(params != NULL, ("no raw xmit params"));

	data = &sc->tx_data[sc->tx_cur];
	desc = (struct ural_tx_desc *)data->buf;

	rate = params->ibp_rate0 & IEEE80211_RATE_VAL;
	/* XXX validate */
	if (rate == 0) {
		m_freem(m0);
		return EINVAL;
	}
	flags = 0;
	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0)
		flags |= RAL_TX_ACK;
	if (params->ibp_flags & (IEEE80211_BPF_RTS|IEEE80211_BPF_CTS)) {
		error = ural_sendprot(sc, m0, ni,
		    params->ibp_flags & IEEE80211_BPF_RTS ?
			 IEEE80211_PROT_RTSCTS : IEEE80211_PROT_CTSONLY,
		    rate);
		if (error) {
			m_freem(m0);
			return error;
		}
		flags |= RAL_TX_IFS_SIFS;
	}

	if (bpf_peers_present(ifp->if_bpf)) {
		struct ural_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RAL_TX_DESC_SIZE);
	/* XXX need to setup descriptor ourself */
	ural_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate);

	/* align end on a 2-bytes boundary */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending raw frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf,
	    xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT,
	    ural_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		data->m = NULL;
		data->ni = NULL;
		return error;
	}

	sc->tx_queued++;
	sc->tx_cur = (sc->tx_cur + 1) % RAL_TX_LIST_COUNT;

	return 0;
}

static int
ural_tx_data(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = sc->sc_ifp;
	struct ural_tx_desc *desc;
	struct ural_tx_data *data;
	struct ieee80211_frame *wh;
	const struct ieee80211_txparam *tp;
	struct ieee80211_key *k;
	uint32_t flags = 0;
	uint16_t dur;
	usbd_status error;
	int xferlen, rate;

	wh = mtod(m0, struct ieee80211_frame *);

	tp = &vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)];
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		rate = tp->mcastrate;
	else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rate = tp->ucastrate;
	else
		rate = ni->ni_txrate;

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}
		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		int prot = IEEE80211_PROT_NONE;
		if (m0->m_pkthdr.len + IEEE80211_CRC_LEN > vap->iv_rtsthreshold)
			prot = IEEE80211_PROT_RTSCTS;
		else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    ieee80211_rate2phytype(sc->sc_rates, rate) == IEEE80211_T_OFDM)
			prot = ic->ic_protmode;
		if (prot != IEEE80211_PROT_NONE) {
			error = ural_sendprot(sc, m0, ni, prot, rate);
			if (error) {
				m_freem(m0);
				return error;
			}
			flags |= RAL_TX_IFS_SIFS;
		}
	}

	data = &sc->tx_data[sc->tx_cur];
	desc = (struct ural_tx_desc *)data->buf;

	data->m = m0;
	data->ni = ni;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;
		flags |= RAL_TX_RETRY(7);

		dur = ieee80211_ack_duration(sc->sc_rates, rate, 
		    ic->ic_flags & IEEE80211_F_SHPREAMBLE);
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

	if (bpf_peers_present(ifp->if_bpf)) {
		struct ural_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate);

	/* align end on a 2-bytes boundary */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending data frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf,
	    xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT,
	    ural_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		data->m = NULL;
		data->ni = NULL;
		return error;
	}

	sc->tx_queued++;
	sc->tx_cur = (sc->tx_cur + 1) % RAL_TX_LIST_COUNT;

	return 0;
}

static void
ural_start(struct ifnet *ifp)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct mbuf *m;

	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		if (sc->tx_queued >= RAL_TX_LIST_COUNT-1) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
		m = ieee80211_encap(ni, m);
		if (m == NULL) {
			ieee80211_free_node(ni);
			continue;
		}
		if (ural_tx_data(sc, m, ni) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}
		sc->sc_tx_timer = 5;
		callout_reset(&sc->watchdog_ch, hz, ural_watchdog, sc);
	}
}

static void
ural_watchdog(void *arg)
{
	struct ural_softc *sc = (struct ural_softc *)arg;

	RAL_LOCK(sc);

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
			/*ural_init(sc); XXX needs a process context! */
			sc->sc_ifp->if_oerrors++;
			RAL_UNLOCK(sc);
			return;
		}
		callout_reset(&sc->watchdog_ch, hz, ural_watchdog, sc);
	}

	RAL_UNLOCK(sc);
}

static int
ural_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 1;

	RAL_LOCK(sc);
	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
				ural_init_locked(sc);
				startall = 1;
			} else
				ural_update_promisc(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ural_stop(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	RAL_UNLOCK(sc);

	if (startall)
		ieee80211_start_all(ic);
	return error;
}

static void
ural_set_testmode(struct ural_softc *sc)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_VENDOR_REQUEST;
	USETW(req.wValue, 4);
	USETW(req.wIndex, 1);
	USETW(req.wLength, 0);

	error = usbd_do_request(sc->sc_udev, &req, NULL);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not set test mode: %s\n",
		    usbd_errstr(error));
	}
}

static void
ural_eeprom_read(struct ural_softc *sc, uint16_t addr, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_EEPROM;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not read EEPROM: %s\n",
		    usbd_errstr(error));
	}
}

static uint16_t
ural_read(struct ural_softc *sc, uint16_t reg)
{
	usb_device_request_t req;
	usbd_status error;
	uint16_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, sizeof (uint16_t));

	error = usbd_do_request(sc->sc_udev, &req, &val);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not read MAC register: %s\n",
		    usbd_errstr(error));
		return 0;
	}

	return le16toh(val);
}

static void
ural_read_multi(struct ural_softc *sc, uint16_t reg, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not read MAC register: %s\n",
		    usbd_errstr(error));
	}
}

static void
ural_write(struct ural_softc *sc, uint16_t reg, uint16_t val)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_WRITE_MAC;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	error = usbd_do_request(sc->sc_udev, &req, NULL);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not write MAC register: %s\n",
		    usbd_errstr(error));
	}
}

static void
ural_write_multi(struct ural_softc *sc, uint16_t reg, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_WRITE_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not write MAC register: %s\n",
		    usbd_errstr(error));
	}
}

static void
ural_bbp_write(struct ural_softc *sc, uint8_t reg, uint8_t val)
{
	uint16_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(ural_read(sc, RAL_PHY_CSR8) & RAL_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		device_printf(sc->sc_dev, "could not write to BBP\n");
		return;
	}

	tmp = reg << 8 | val;
	ural_write(sc, RAL_PHY_CSR7, tmp);
}

static uint8_t
ural_bbp_read(struct ural_softc *sc, uint8_t reg)
{
	uint16_t val;
	int ntries;

	val = RAL_BBP_WRITE | reg << 8;
	ural_write(sc, RAL_PHY_CSR7, val);

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(ural_read(sc, RAL_PHY_CSR8) & RAL_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		device_printf(sc->sc_dev, "could not read BBP\n");
		return 0;
	}

	return ural_read(sc, RAL_PHY_CSR7) & 0xff;
}

static void
ural_rf_write(struct ural_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(ural_read(sc, RAL_PHY_CSR10) & RAL_RF_LOBUSY))
			break;
	}
	if (ntries == 5) {
		device_printf(sc->sc_dev, "could not write to RF\n");
		return;
	}

	tmp = RAL_RF_BUSY | RAL_RF_20BIT | (val & 0xfffff) << 2 | (reg & 0x3);
	ural_write(sc, RAL_PHY_CSR9,  tmp & 0xffff);
	ural_write(sc, RAL_PHY_CSR10, tmp >> 16);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 0x3, val & 0xfffff));
}

/* ARGUSED */
static struct ieee80211_node *
ural_node_alloc(struct ieee80211_node_table *nt __unused)
{
	struct ural_node *un;

	un = malloc(sizeof(struct ural_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	return un != NULL ? &un->ni : NULL;
}

static void
ural_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ieee80211_amrr_node_init(&URAL_VAP(vap)->amrr, &URAL_NODE(ni)->amn, ni);
}

static void
ural_scan_start(struct ieee80211com *ic)
{
	struct ural_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = URAL_SCAN_START;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);

}

static void
ural_scan_end(struct ieee80211com *ic)
{
	struct ural_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = URAL_SCAN_END;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);

}

static void
ural_set_channel(struct ieee80211com *ic)
{

	struct ural_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = URAL_SET_CHANNEL;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);

	sc->sc_rates = ieee80211_get_ratetable(ic->ic_curchan);
}

static void
ural_set_chan(struct ural_softc *sc, struct ieee80211_channel *c)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint8_t power, tmp;
	u_int i, chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		power = min(sc->txpow[chan - 1], 31);
	else
		power = 31;

	/* adjust txpower using ifconfig settings */
	power -= (100 - ic->ic_txpowlimit) / 8;

	DPRINTFN(2, ("setting channel to %u, txpower to %u\n", chan, power));

	switch (sc->rf_rev) {
	case RAL_RF_2522:
		ural_rf_write(sc, RAL_RF1, 0x00814);
		ural_rf_write(sc, RAL_RF2, ural_rf2522_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		break;

	case RAL_RF_2523:
		ural_rf_write(sc, RAL_RF1, 0x08804);
		ural_rf_write(sc, RAL_RF2, ural_rf2523_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x38044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2524:
		ural_rf_write(sc, RAL_RF1, 0x0c808);
		ural_rf_write(sc, RAL_RF2, ural_rf2524_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525:
		ural_rf_write(sc, RAL_RF1, 0x08808);
		ural_rf_write(sc, RAL_RF2, ural_rf2525_hi_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);

		ural_rf_write(sc, RAL_RF1, 0x08808);
		ural_rf_write(sc, RAL_RF2, ural_rf2525_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525E:
		ural_rf_write(sc, RAL_RF1, 0x08808);
		ural_rf_write(sc, RAL_RF2, ural_rf2525e_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00286 : 0x00282);
		break;

	case RAL_RF_2526:
		ural_rf_write(sc, RAL_RF2, ural_rf2526_hi_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		ural_rf_write(sc, RAL_RF1, 0x08804);

		ural_rf_write(sc, RAL_RF2, ural_rf2526_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		break;

	/* dual-band RF */
	case RAL_RF_5222:
		for (i = 0; ural_rf5222[i].chan != chan; i++);

		ural_rf_write(sc, RAL_RF1, ural_rf5222[i].r1);
		ural_rf_write(sc, RAL_RF2, ural_rf5222[i].r2);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		ural_rf_write(sc, RAL_RF4, ural_rf5222[i].r4);
		break;
	}

	if (ic->ic_opmode != IEEE80211_M_MONITOR &&
	    (ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		/* set Japan filter bit for channel 14 */
		tmp = ural_bbp_read(sc, 70);

		tmp &= ~RAL_JAPAN_FILTER;
		if (chan == 14)
			tmp |= RAL_JAPAN_FILTER;

		ural_bbp_write(sc, 70, tmp);

		/* clear CRC errors */
		ural_read(sc, RAL_STA_CSR0);

		DELAY(10000);
		ural_disable_rf_tune(sc);
	}

	/* XXX doesn't belong here */
	/* update basic rate set */
	ural_set_basicrates(sc, c);
}

/*
 * Disable RF auto-tuning.
 */
static void
ural_disable_rf_tune(struct ural_softc *sc)
{
	uint32_t tmp;

	if (sc->rf_rev != RAL_RF_2523) {
		tmp = sc->rf_regs[RAL_RF1] & ~RAL_RF1_AUTOTUNE;
		ural_rf_write(sc, RAL_RF1, tmp);
	}

	tmp = sc->rf_regs[RAL_RF3] & ~RAL_RF3_AUTOTUNE;
	ural_rf_write(sc, RAL_RF3, tmp);

	DPRINTFN(2, ("disabling RF autotune\n"));
}

/*
 * Refer to IEEE Std 802.11-1999 pp. 123 for more information on TSF
 * synchronization.
 */
static void
ural_enable_tsf_sync(struct ural_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint16_t logcwmin, preload, tmp;

	/* first, disable TSF synchronization */
	ural_write(sc, RAL_TXRX_CSR19, 0);

	tmp = (16 * vap->iv_bss->ni_intval) << 4;
	ural_write(sc, RAL_TXRX_CSR18, tmp);

	logcwmin = (ic->ic_opmode == IEEE80211_M_IBSS) ? 2 : 0;
	preload = (ic->ic_opmode == IEEE80211_M_IBSS) ? 320 : 6;
	tmp = logcwmin << 12 | preload;
	ural_write(sc, RAL_TXRX_CSR20, tmp);

	/* finally, enable TSF synchronization */
	tmp = RAL_ENABLE_TSF | RAL_ENABLE_TBCN;
	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= RAL_ENABLE_TSF_SYNC(1);
	else
		tmp |= RAL_ENABLE_TSF_SYNC(2) | RAL_ENABLE_BEACON_GENERATOR;
	ural_write(sc, RAL_TXRX_CSR19, tmp);

	DPRINTF(("enabling TSF synchronization\n"));
}

static void
ural_update_slot(struct ifnet *ifp)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t slottime, sifs, eifs;

	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	/*
	 * These settings may sound a bit inconsistent but this is what the
	 * reference driver does.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		sifs = 16 - RAL_RXTX_TURNAROUND;
		eifs = 364;
	} else {
		sifs = 10 - RAL_RXTX_TURNAROUND;
		eifs = 64;
	}

	ural_write(sc, RAL_MAC_CSR10, slottime);
	ural_write(sc, RAL_MAC_CSR11, sifs);
	ural_write(sc, RAL_MAC_CSR12, eifs);
}

static void
ural_set_txpreamble(struct ural_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t tmp;

	tmp = ural_read(sc, RAL_TXRX_CSR10);

	tmp &= ~RAL_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RAL_SHORT_PREAMBLE;

	ural_write(sc, RAL_TXRX_CSR10, tmp);
}

static void
ural_set_basicrates(struct ural_softc *sc, const struct ieee80211_channel *c)
{
	/* XXX wrong, take from rate set */
	/* update basic rate set */
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		/* 11a basic rates: 6, 12, 24Mbps */
		ural_write(sc, RAL_TXRX_CSR11, 0x150);
	} else if (IEEE80211_IS_CHAN_ANYG(c)) {
		/* 11g basic rates: 1, 2, 5.5, 11, 6, 12, 24Mbps */
		ural_write(sc, RAL_TXRX_CSR11, 0x15f);
	} else {
		/* 11b basic rates: 1, 2Mbps */
		ural_write(sc, RAL_TXRX_CSR11, 0x3);
	}
}

static void
ural_set_bssid(struct ural_softc *sc, const uint8_t *bssid)
{
	uint16_t tmp;

	tmp = bssid[0] | bssid[1] << 8;
	ural_write(sc, RAL_MAC_CSR5, tmp);

	tmp = bssid[2] | bssid[3] << 8;
	ural_write(sc, RAL_MAC_CSR6, tmp);

	tmp = bssid[4] | bssid[5] << 8;
	ural_write(sc, RAL_MAC_CSR7, tmp);

	DPRINTF(("setting BSSID to %6D\n", bssid, ":"));
}

static void
ural_set_macaddr(struct ural_softc *sc, uint8_t *addr)
{
	uint16_t tmp;

	tmp = addr[0] | addr[1] << 8;
	ural_write(sc, RAL_MAC_CSR2, tmp);

	tmp = addr[2] | addr[3] << 8;
	ural_write(sc, RAL_MAC_CSR3, tmp);

	tmp = addr[4] | addr[5] << 8;
	ural_write(sc, RAL_MAC_CSR4, tmp);

	DPRINTF(("setting MAC address to %6D\n", addr, ":"));
}

static void
ural_update_promisc(struct ural_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t tmp;

	tmp = ural_read(sc, RAL_TXRX_CSR2);

	tmp &= ~RAL_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RAL_DROP_NOT_TO_ME;

	ural_write(sc, RAL_TXRX_CSR2, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

static const char *
ural_get_rf(int rev)
{
	switch (rev) {
	case RAL_RF_2522:	return "RT2522";
	case RAL_RF_2523:	return "RT2523";
	case RAL_RF_2524:	return "RT2524";
	case RAL_RF_2525:	return "RT2525";
	case RAL_RF_2525E:	return "RT2525e";
	case RAL_RF_2526:	return "RT2526";
	case RAL_RF_5222:	return "RT5222";
	default:		return "unknown";
	}
}

static void
ural_read_eeprom(struct ural_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t val;

	ural_eeprom_read(sc, RAL_EEPROM_CONFIG0, &val, 2);
	val = le16toh(val);
	sc->rf_rev =   (val >> 11) & 0x7;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->led_mode = (val >> 6)  & 0x7;
	sc->rx_ant =   (val >> 4)  & 0x3;
	sc->tx_ant =   (val >> 2)  & 0x3;
	sc->nb_ant =   val & 0x3;

	/* read MAC address */
	ural_eeprom_read(sc, RAL_EEPROM_ADDRESS, ic->ic_myaddr, 6);

	/* read default values for BBP registers */
	ural_eeprom_read(sc, RAL_EEPROM_BBP_BASE, sc->bbp_prom, 2 * 16);

	/* read Tx power for all b/g channels */
	ural_eeprom_read(sc, RAL_EEPROM_TXPOWER, sc->txpow, 14);
}

static int
ural_bbp_init(struct ural_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		if (ural_bbp_read(sc, RAL_BBP_VERSION) != 0)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for BBP\n");
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(ural_def_bbp); i++)
		ural_bbp_write(sc, ural_def_bbp[i].reg, ural_def_bbp[i].val);

#if 0
	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0xff)
			continue;
		ural_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}
#endif

	return 0;
#undef N
}

static void
ural_set_txantenna(struct ural_softc *sc, int antenna)
{
	uint16_t tmp;
	uint8_t tx;

	tx = ural_bbp_read(sc, RAL_BBP_TX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		tx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		tx |= RAL_BBP_ANTB;
	else
		tx |= RAL_BBP_DIVERSITY;

	/* need to force I/Q flip for RF 2525e, 2526 and 5222 */
	if (sc->rf_rev == RAL_RF_2525E || sc->rf_rev == RAL_RF_2526 ||
	    sc->rf_rev == RAL_RF_5222)
		tx |= RAL_BBP_FLIPIQ;

	ural_bbp_write(sc, RAL_BBP_TX, tx);

	/* update values in PHY_CSR5 and PHY_CSR6 */
	tmp = ural_read(sc, RAL_PHY_CSR5) & ~0x7;
	ural_write(sc, RAL_PHY_CSR5, tmp | (tx & 0x7));

	tmp = ural_read(sc, RAL_PHY_CSR6) & ~0x7;
	ural_write(sc, RAL_PHY_CSR6, tmp | (tx & 0x7));
}

static void
ural_set_rxantenna(struct ural_softc *sc, int antenna)
{
	uint8_t rx;

	rx = ural_bbp_read(sc, RAL_BBP_RX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		rx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		rx |= RAL_BBP_ANTB;
	else
		rx |= RAL_BBP_DIVERSITY;

	/* need to force no I/Q flip for RF 2525e and 2526 */
	if (sc->rf_rev == RAL_RF_2525E || sc->rf_rev == RAL_RF_2526)
		rx &= ~RAL_BBP_FLIPIQ;

	ural_bbp_write(sc, RAL_BBP_RX, rx);
}

static void
ural_init_locked(struct ural_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ural_rx_data *data;
	uint16_t tmp;
	usbd_status error;
	int i, ntries;

	ural_set_testmode(sc);
	ural_write(sc, 0x308, 0x00f0);	/* XXX magic */

	ural_stop(sc);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(ural_def_mac); i++)
		ural_write(sc, ural_def_mac[i].reg, ural_def_mac[i].val);

	/* wait for BBP and RF to wake up (this can take a long time!) */
	for (ntries = 0; ntries < 100; ntries++) {
		tmp = ural_read(sc, RAL_MAC_CSR17);
		if ((tmp & (RAL_BBP_AWAKE | RAL_RF_AWAKE)) ==
		    (RAL_BBP_AWAKE | RAL_RF_AWAKE))
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "timeout waiting for BBP/RF to wakeup\n");
		goto fail;
	}

	/* we're ready! */
	ural_write(sc, RAL_MAC_CSR1, RAL_HOST_READY);

	/* set basic rate set (will be updated later) */
	ural_write(sc, RAL_TXRX_CSR11, 0x15f);

	if (ural_bbp_init(sc) != 0)
		goto fail;

	ural_set_chan(sc, ic->ic_curchan);

	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_read_multi(sc, RAL_STA_CSR0, sc->sta, sizeof sc->sta);

	ural_set_txantenna(sc, sc->tx_ant);
	ural_set_rxantenna(sc, sc->rx_ant);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	ural_set_macaddr(sc, ic->ic_myaddr);

	/*
	 * Allocate xfer for AMRR statistics requests.
	 */
	sc->amrr_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->amrr_xfer == NULL) {
		device_printf(sc->sc_dev, "could not allocate AMRR xfer\n");
		goto fail;
	}

	/*
	 * Open Tx and Rx USB bulk pipes.
	 */
	error = usbd_open_pipe(sc->sc_iface, sc->sc_tx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not open Tx pipe: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	error = usbd_open_pipe(sc->sc_iface, sc->sc_rx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_rx_pipeh);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not open Rx pipe: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	error = ural_alloc_tx_list(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Tx list\n");
		goto fail;
	}

	error = ural_alloc_rx_list(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate Rx list\n");
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < RAL_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->sc_rx_pipeh, data, data->buf,
		    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, ural_rxeof);
		usbd_transfer(data->xfer);
	}

	/* kick Rx */
	tmp = RAL_DROP_PHY | RAL_DROP_CRC;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RAL_DROP_CTL | RAL_DROP_BAD_VERSION;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RAL_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RAL_DROP_NOT_TO_ME;
	}
	ural_write(sc, RAL_TXRX_CSR2, tmp);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	return;

fail:	ural_stop(sc);
#undef N
}

static void
ural_init(void *priv)
{
	struct ural_softc *sc = priv;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	RAL_LOCK(sc);
	ural_init_locked(sc);
	RAL_UNLOCK(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ieee80211_start_all(ic);		/* start all vap's */
}

static void
ural_stop(void *priv)
{
	struct ural_softc *sc = priv;
	struct ifnet *ifp = sc->sc_ifp;

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* disable Rx */
	ural_write(sc, RAL_TXRX_CSR2, RAL_DISABLE_RX);

	/* reset ASIC and BBP (but won't reset MAC registers!) */
	ural_write(sc, RAL_MAC_CSR1, RAL_RESET_ASIC | RAL_RESET_BBP);
	ural_write(sc, RAL_MAC_CSR1, 0);

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

	ural_free_rx_list(sc);
	ural_free_tx_list(sc);
}

static int
ural_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ural_softc *sc = ifp->if_softc;

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}
	if (sc->tx_queued >= RAL_TX_LIST_COUNT) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		m_freem(m);
		ieee80211_free_node(ni);
		return EIO;
	}

	ifp->if_opackets++;

	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		if (ural_tx_mgt(sc, m, ni) != 0)
			goto bad;
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		if (ural_tx_raw(sc, m, ni, params) != 0)
			goto bad;
	}
	sc->sc_tx_timer = 5;
	callout_reset(&sc->watchdog_ch, hz, ural_watchdog, sc);

	return 0;
bad:
	ifp->if_oerrors++;
	ieee80211_free_node(ni);
	return EIO;		/* XXX */
}

static void
ural_amrr_start(struct ural_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ural_vap *uvp = URAL_VAP(vap);

	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_read_multi(sc, RAL_STA_CSR0, sc->sta, sizeof sc->sta);

	ieee80211_amrr_node_init(&uvp->amrr, &URAL_NODE(ni)->amn, ni);

	callout_reset(&uvp->amrr_ch, hz, ural_amrr_timeout, vap);
}

static void
ural_amrr_timeout(void *arg)
{
	struct ieee80211vap *vap = arg;
	struct ural_softc *sc = vap->iv_ic->ic_ifp->if_softc;
	usb_device_request_t req;

	/*
	 * Asynchronously read statistic registers (cleared by read).
	 */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, RAL_STA_CSR0);
	USETW(req.wLength, sizeof sc->sta);

	usbd_setup_default_xfer(sc->amrr_xfer, sc->sc_udev, vap,
	    USBD_DEFAULT_TIMEOUT, &req, sc->sta, sizeof sc->sta, 0,
	    ural_amrr_update);
	(void)usbd_transfer(sc->amrr_xfer);
}

static void
ural_amrr_update(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct ieee80211vap *vap = priv;
	struct ural_vap *uvp = URAL_VAP(vap);
	struct ifnet *ifp = vap->iv_ic->ic_ifp;
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni = vap->iv_bss;
	int ok, fail;

	if (status != USBD_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "could not retrieve Tx statistics - "
		    "cancelling automatic rate control\n");
		return;
	}

	ok = sc->sta[7] +		/* TX ok w/o retry */
	     sc->sta[8];		/* TX ok w/ retry */
	fail = sc->sta[9];		/* TX retry-fail count */

	ieee80211_amrr_tx_update(&URAL_NODE(ni)->amn,
	    ok+fail, ok, sc->sta[8] + fail);
	(void) ieee80211_amrr_choose(ni, &URAL_NODE(ni)->amn);

	ifp->if_oerrors += fail;	/* count TX retry-fail as Tx errors */

	callout_reset(&uvp->amrr_ch, hz, ural_amrr_timeout, vap);
}
