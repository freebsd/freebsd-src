/*	$OpenBSD: if_upgt.c,v 1.35 2008/04/16 18:32:15 damien Exp $ */
/*	$FreeBSD$ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/firmware.h>
#include <sys/linker.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>

#include <net/bpf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"
#include <dev/usb/usb_ethersubr.h>

#include <dev/usb/if_upgtvar.h>

/*
 * Driver for the USB PrismGT devices.
 *
 * For now just USB 2.0 devices with the GW3887 chipset are supported.
 * The driver has been written based on the firmware version 2.13.1.0_LM87.
 *
 * TODO's:
 * - MONITOR mode test.
 * - Add HOSTAP mode.
 * - Add IBSS mode.
 * - Support the USB 1.0 devices (NET2280, ISL3880, ISL3886 chipsets).
 *
 * Parts of this driver has been influenced by reading the p54u driver
 * written by Jean-Baptiste Note <jean-baptiste.note@m4x.org> and
 * Sebastien Bourdeauducq <lekernel@prism54.org>.
 */

SYSCTL_NODE(_hw, OID_AUTO, upgt, CTLFLAG_RD, 0,
    "USB PrismGT GW3887 driver parameters");

/*
 * NB: normally `upgt_txbuf' value can be increased to maximum 6, mininum 1.
 * However, we're using just 2 txbufs to protect packet losses in some cases
 * so the performance was sacrificed that with this value its speed is about
 * 2.1Mb/s.
 *
 * With setting txbuf value as 6, you can get full speed, 3.0Mb/s, of this
 * device but sometimes you'd meet some packet losses then retransmision.
 */
static	int upgt_txbuf = UPGT_TX_COUNT;		/* # tx buffers to allocate */
SYSCTL_INT(_hw_upgt, OID_AUTO, txbuf, CTLFLAG_RW, &upgt_txbuf,
    0, "tx buffers allocated");
TUNABLE_INT("hw.upgt.txbuf", &upgt_txbuf);

#ifdef UPGT_DEBUG
int upgt_debug = 0;
SYSCTL_INT(_hw_upgt, OID_AUTO, debug, CTLFLAG_RW, &upgt_debug,
	    0, "control debugging printfs");
TUNABLE_INT("hw.upgt.debug", &upgt_debug);
enum {
	UPGT_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	UPGT_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	UPGT_DEBUG_RESET	= 0x00000004,	/* reset processing */
	UPGT_DEBUG_INTR		= 0x00000008,	/* INTR */
	UPGT_DEBUG_TX_PROC	= 0x00000010,	/* tx ISR proc */
	UPGT_DEBUG_RX_PROC	= 0x00000020,	/* rx ISR proc */
	UPGT_DEBUG_STATE	= 0x00000040,	/* 802.11 state transitions */
	UPGT_DEBUG_STAT		= 0x00000080,	/* statistic */
	UPGT_DEBUG_FW		= 0x00000100,	/* firmware */
	UPGT_DEBUG_ANY		= 0xffffffff
};
#define	DPRINTF(sc, m, fmt, ...) do {				\
	if (sc->sc_debug & (m))					\
		printf(fmt, __VA_ARGS__);			\
} while (0)
#else
#define	DPRINTF(sc, m, fmt, ...) do {				\
	(void) sc;						\
} while (0)
#endif

/*
 * Prototypes.
 */
static device_probe_t upgt_match;
static device_attach_t upgt_attach;
static device_detach_t upgt_detach;
static int	upgt_alloc_tx(struct upgt_softc *);
static int	upgt_alloc_rx(struct upgt_softc *);
static int	upgt_alloc_cmd(struct upgt_softc *);
static int	upgt_attach_hook(device_t);
static int	upgt_device_reset(struct upgt_softc *);
static int	upgt_bulk_xmit(struct upgt_softc *, struct upgt_data *,
		    usbd_pipe_handle, uint32_t *, int);
static int	upgt_fw_verify(struct upgt_softc *);
static int	upgt_mem_init(struct upgt_softc *);
static int	upgt_fw_load(struct upgt_softc *);
static int	upgt_fw_copy(const uint8_t *, char *, int);
static uint32_t	upgt_crc32_le(const void *, size_t);
static void	upgt_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	upgt_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static int	upgt_eeprom_read(struct upgt_softc *);
static int	upgt_eeprom_parse(struct upgt_softc *);
static void	upgt_eeprom_parse_hwrx(struct upgt_softc *, uint8_t *);
static void	upgt_eeprom_parse_freq3(struct upgt_softc *, uint8_t *, int);
static void	upgt_eeprom_parse_freq4(struct upgt_softc *, uint8_t *, int);
static void	upgt_eeprom_parse_freq6(struct upgt_softc *, uint8_t *, int);
static uint32_t	upgt_chksum_le(const uint32_t *, size_t);
static void	upgt_tx_done(struct upgt_softc *, uint8_t *);
static void	upgt_rx(struct upgt_softc *, uint8_t *, int);
static void	upgt_init(void *);
static void	upgt_init_locked(struct upgt_softc *);
static int	upgt_ioctl(struct ifnet *, u_long, caddr_t);
static void	upgt_start(struct ifnet *);
static int	upgt_raw_xmit(struct ieee80211_node *, struct mbuf *,
		    const struct ieee80211_bpf_params *);
static void	upgt_scan_start(struct ieee80211com *);
static void	upgt_scan_end(struct ieee80211com *);
static void	upgt_set_channel(struct ieee80211com *);
static struct ieee80211vap *upgt_vap_create(struct ieee80211com *,
		    const char name[IFNAMSIZ], int unit, int opmode,
		    int flags, const uint8_t bssid[IEEE80211_ADDR_LEN],
		    const uint8_t mac[IEEE80211_ADDR_LEN]);
static void	upgt_vap_delete(struct ieee80211vap *);
static void	upgt_update_mcast(struct ifnet *);
static uint8_t	upgt_rx_rate(struct upgt_softc *, const int);
static void	upgt_set_multi(void *);
static void	upgt_stop(struct upgt_softc *, int);
static void	upgt_setup_rates(struct ieee80211vap *, struct ieee80211com *);
static int	upgt_set_macfilter(struct upgt_softc *, uint8_t);
static int	upgt_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	upgt_task(void *);
static void	upgt_scantask(void *);
static void	upgt_set_chan(struct upgt_softc *, struct ieee80211_channel *);
static void	upgt_set_led(struct upgt_softc *, int);
static void	upgt_set_led_blink(void *);
static void	upgt_tx_task(void *);
static int	upgt_get_stats(struct upgt_softc *);
static void	upgt_mem_free(struct upgt_softc *, uint32_t);
static uint32_t	upgt_mem_alloc(struct upgt_softc *);
static void	upgt_free_tx(struct upgt_softc *);
static void	upgt_free_rx(struct upgt_softc *);
static void	upgt_free_cmd(struct upgt_softc *);
static void	upgt_watchdog(void *);

static const char *upgt_fwname = "upgt-gw3887";

static const struct usb_devno upgt_devs_2[] = {
	/* version 2 devices */
	{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_PRISM_GT },
	{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D7050 },
	{ USB_VENDOR_CONCEPTRONIC,	USB_PRODUCT_CONCEPTRONIC_PRISM_GT },
	{ USB_VENDOR_DELL,		USB_PRODUCT_DELL_PRISM_GT_1 },
	{ USB_VENDOR_DELL,		USB_PRODUCT_DELL_PRISM_GT_2 },
	{ USB_VENDOR_FSC,		USB_PRODUCT_FSC_E5400 },
	{ USB_VENDOR_GLOBESPAN,		USB_PRODUCT_GLOBESPAN_PRISM_GT_1 },
	{ USB_VENDOR_GLOBESPAN,		USB_PRODUCT_GLOBESPAN_PRISM_GT_2 },
	{ USB_VENDOR_INTERSIL,		USB_PRODUCT_INTERSIL_PRISM_GT },
	{ USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_WG111V2_2 },
	{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2862WG },
	{ USB_VENDOR_WISTRONNEWEB,	USB_PRODUCT_WISTRONNEWEB_UR045G },
	{ USB_VENDOR_XYRATEX,		USB_PRODUCT_XYRATEX_PRISM_GT_1 },
	{ USB_VENDOR_XYRATEX,		USB_PRODUCT_XYRATEX_PRISM_GT_2 },
	{ USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_XG703A }
};

static int
upgt_match(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (!uaa->iface)
		return UMATCH_NONE;

	if (usb_lookup(upgt_devs_2, uaa->vendor, uaa->product) != NULL)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

static int
upgt_attach(device_t dev)
{
	int i;
	struct upgt_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usb_endpoint_descriptor_t *ed;
	usb_interface_descriptor_t *id;
	usbd_status error;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
#ifdef UPGT_DEBUG
	sc->sc_debug = upgt_debug;
#endif

	/* set configuration number */
	if (usbd_set_config_no(sc->sc_udev, UPGT_CONFIG_NO, 0) != 0) {
		device_printf(dev, "could not set configuration no!\n");
		return ENXIO;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, UPGT_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		device_printf(dev, "could not get interface handle!\n");
		return ENXIO;
	}

	/* find endpoints */
	id = usbd_get_interface_descriptor(sc->sc_iface);
	sc->sc_rx_no = sc->sc_tx_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			device_printf(dev,
			    "no endpoint descriptor for iface %d!\n", i);
			return ENXIO;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_tx_no = ed->bEndpointAddress;
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_rx_no = ed->bEndpointAddress;

		/*
		 * 0x01 TX pipe
		 * 0x81 RX pipe
		 *
		 * Deprecated scheme (not used with fw version >2.5.6.x):
		 * 0x02 TX MGMT pipe
		 * 0x82 TX MGMT pipe
		 */
		if (sc->sc_tx_no != -1 && sc->sc_rx_no != -1)
			break;
	}
	if (sc->sc_rx_no == -1 || sc->sc_tx_no == -1) {
		device_printf(dev, "missing endpoint!\n");
		return ENXIO;
	}

	/*
	 * Open TX and RX USB bulk pipes.
	 */
	error = usbd_open_pipe(sc->sc_iface, sc->sc_tx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != 0) {
		device_printf(dev, "could not open TX pipe: %s!\n",
		    usbd_errstr(error));
		goto fail;
	}
	error = usbd_open_pipe(sc->sc_iface, sc->sc_rx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_rx_pipeh);
	if (error != 0) {
		device_printf(dev, "could not open RX pipe: %s!\n",
		    usbd_errstr(error));
		goto fail;
	}

	/* Allocate TX, RX, and CMD xfers.  */
	if (upgt_alloc_tx(sc) != 0)
		goto fail;
	if (upgt_alloc_rx(sc) != 0)
		goto fail;
	if (upgt_alloc_cmd(sc) != 0)
		goto fail;

	/* We need the firmware loaded to complete the attach.  */
	return upgt_attach_hook(dev);

fail:
	device_printf(dev, "%s failed!\n", __func__);
	return ENXIO;
}

static int
upgt_attach_hook(device_t dev)
{
	struct ieee80211com *ic;
	struct ifnet *ifp;
	struct upgt_softc *sc = device_get_softc(dev);
	struct upgt_data *data_rx = &sc->rx_data;
	uint8_t bands;
	usbd_status error;

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		return ENXIO;
	}

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	usb_init_task(&sc->sc_mcasttask, upgt_set_multi, sc);
	usb_init_task(&sc->sc_scantask, upgt_scantask, sc);
	usb_init_task(&sc->sc_task, upgt_task, sc);
	usb_init_task(&sc->sc_task_tx, upgt_tx_task, sc);
	callout_init(&sc->sc_led_ch, 0);
	callout_init(&sc->sc_watchdog_ch, 0);

	/* Initialize the device.  */
	if (upgt_device_reset(sc) != 0)
		goto fail;

	/* Verify the firmware.  */
	if (upgt_fw_verify(sc) != 0)
		goto fail;

	/* Calculate device memory space.  */
	if (sc->sc_memaddr_frame_start == 0 || sc->sc_memaddr_frame_end == 0) {
		device_printf(dev,
		    "could not find memory space addresses on FW!\n");
		goto fail;
	}
	sc->sc_memaddr_frame_end -= UPGT_MEMSIZE_RX + 1;
	sc->sc_memaddr_rx_start = sc->sc_memaddr_frame_end + 1;

	DPRINTF(sc, UPGT_DEBUG_FW, "memory address frame start=0x%08x\n",
	    sc->sc_memaddr_frame_start);
	DPRINTF(sc, UPGT_DEBUG_FW, "memory address frame end=0x%08x\n",
	    sc->sc_memaddr_frame_end);
	DPRINTF(sc, UPGT_DEBUG_FW, "memory address rx start=0x%08x\n",
	    sc->sc_memaddr_rx_start);

	upgt_mem_init(sc);

	/* Load the firmware.  */
	if (upgt_fw_load(sc) != 0)
		goto fail;

	/* Startup the RX pipe.  */
	usbd_setup_xfer(data_rx->xfer, sc->sc_rx_pipeh, data_rx, data_rx->buf,
	    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, upgt_rxeof);
	error = usbd_transfer(data_rx->xfer);
	if (error != 0 && error != USBD_IN_PROGRESS) {
		device_printf(dev, "could not queue RX transfer!\n");
		goto fail;
	}
	usbd_delay_ms(sc->sc_udev, 100);

	/* Read the whole EEPROM content and parse it.  */
	if (upgt_eeprom_read(sc) != 0)
		goto fail;
	if (upgt_eeprom_parse(sc) != 0)
		goto fail;

	/* Setup the 802.11 device.  */
	ifp->if_softc = sc;
	if_initname(ifp, "upgt", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT; /* USB stack is still under Giant lock */
	ifp->if_init = upgt_init;
	ifp->if_ioctl = upgt_ioctl;
	ifp->if_start = upgt_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	ic = ifp->if_l2com;
	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;
	/* set device capabilities */
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	        | IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
	        | IEEE80211_C_WPA		/* 802.11i */
		;

	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	ieee80211_init_channels(ic, NULL, &bands);

	ieee80211_ifattach(ic);
	ic->ic_raw_xmit = upgt_raw_xmit;
	ic->ic_scan_start = upgt_scan_start;
	ic->ic_scan_end = upgt_scan_end;
	ic->ic_set_channel = upgt_set_channel;

	ic->ic_vap_create = upgt_vap_create;
	ic->ic_vap_delete = upgt_vap_delete;
	ic->ic_update_mcast = upgt_update_mcast;

	bpfattach(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + sizeof(sc->sc_txtap));
	sc->sc_rxtap_len = sizeof(sc->sc_rxtap);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(UPGT_RX_RADIOTAP_PRESENT);
	sc->sc_txtap_len = sizeof(sc->sc_txtap);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(UPGT_TX_RADIOTAP_PRESENT);

	if (bootverbose)
		ieee80211_announce(ic);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
	return 0;
fail:
	device_printf(dev, "%s failed!\n", __func__);
	mtx_destroy(&sc->sc_mtx);
	if_free(ifp);
	return ENXIO;
}

static void
upgt_tx_task(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct upgt_data *data_tx;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_tx_desc *txdesc;
	struct mbuf *m;
	uint32_t addr;
	int len, i;
	usbd_status error;

	upgt_set_led(sc, UPGT_LED_BLINK);

	UPGT_LOCK(sc);
	for (i = 0; i < upgt_txbuf; i++) {
		data_tx = &sc->tx_data[i];
		if (data_tx->m == NULL)
			continue;

		m = data_tx->m;
		addr = data_tx->addr + UPGT_MEMSIZE_FRAME_HEAD;

		/*
		 * Software crypto.
		 */
		wh = mtod(m, struct ieee80211_frame *);
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			k = ieee80211_crypto_encap(data_tx->ni, m);
			if (k == NULL) {
				device_printf(sc->sc_dev,
				    "ieee80211_crypto_encap returns NULL.\n");
				goto done;
			}

			/* in case packet header moved, reset pointer */
			wh = mtod(m, struct ieee80211_frame *);
		}

		/*
		 * Transmit the URB containing the TX data.
		 */
		bzero(data_tx->buf, MCLBYTES);

		mem = (struct upgt_lmac_mem *)data_tx->buf;
		mem->addr = htole32(addr);

		txdesc = (struct upgt_lmac_tx_desc *)(mem + 1);

		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT) {
			/* mgmt frames  */
			txdesc->header1.flags = UPGT_H1_FLAGS_TX_MGMT;
			/* always send mgmt frames at lowest rate (DS1) */
			memset(txdesc->rates, 0x10, sizeof(txdesc->rates));
		} else {
			/* data frames  */
			txdesc->header1.flags = UPGT_H1_FLAGS_TX_DATA;
			bcopy(sc->sc_cur_rateset, txdesc->rates,
			    sizeof(txdesc->rates));
		}
		txdesc->header1.type = UPGT_H1_TYPE_TX_DATA;
		txdesc->header1.len = htole16(m->m_pkthdr.len);
		txdesc->header2.reqid = htole32(data_tx->addr);
		txdesc->header2.type = htole16(UPGT_H2_TYPE_TX_ACK_YES);
		txdesc->header2.flags = htole16(UPGT_H2_FLAGS_TX_ACK_YES);
		txdesc->type = htole32(UPGT_TX_DESC_TYPE_DATA);
		txdesc->pad3[0] = UPGT_TX_DESC_PAD3_SIZE;

		if (bpf_peers_present(ifp->if_bpf)) {
			struct upgt_tx_radiotap_header *tap = &sc->sc_txtap;

			tap->wt_flags = 0;
			tap->wt_rate = 0;	/* XXX where to get from? */
			tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
			tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

			bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m);
		}

		/* copy frame below our TX descriptor header */
		m_copydata(m, 0, m->m_pkthdr.len,
		    data_tx->buf + (sizeof(*mem) + sizeof(*txdesc)));
		/* calculate frame size */
		len = sizeof(*mem) + sizeof(*txdesc) + m->m_pkthdr.len;
		/* we need to align the frame to a 4 byte boundary */
		len = (len + 3) & ~3;
		/* calculate frame checksum */
		mem->chksum = upgt_chksum_le((uint32_t *)txdesc,
		    len - sizeof(*mem));
		/* we do not need the mbuf anymore */
		m_freem(m);
		data_tx->m = NULL;

		DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: TX start data sending\n",
		    __func__);
		KASSERT(len <= MCLBYTES, ("mbuf is small for saving data"));

		usbd_setup_xfer(data_tx->xfer, sc->sc_tx_pipeh, data_tx,
		    data_tx->buf, len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
		    UPGT_USB_TIMEOUT, upgt_txeof);
		UPGT_UNLOCK(sc);
		mtx_lock(&Giant);
		error = usbd_transfer(data_tx->xfer);
		mtx_unlock(&Giant);
		UPGT_LOCK(sc);
		if (error != 0 && error != USBD_IN_PROGRESS) {
			device_printf(sc->sc_dev,
			    "could not transmit TX data URB!\n");
			goto done;
		}

		DPRINTF(sc, UPGT_DEBUG_XMIT, "TX sent (%d bytes)\n", len);
	}
done:
	UPGT_UNLOCK(sc);
	/*
	 * If we don't regulary read the device statistics, the RX queue
	 * will stall.  It's strange, but it works, so we keep reading
	 * the statistics here.  *shrug*
	 */
	(void)upgt_get_stats(sc);
}

static void
upgt_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct upgt_data *data_tx = priv;
	struct upgt_softc *sc = data_tx->sc;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipeh);
			return;
		}

		device_printf(sc->sc_dev, "TX warning(%s)\n",
		    usbd_errstr(status));
	}
}

static int
upgt_get_stats(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_stats *stats;
	int len;

	/*
	 * Transmit the URB containing the CMD data.
	 */
	bzero(data_cmd->buf, MCLBYTES);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	stats = (struct upgt_lmac_stats *)(mem + 1);

	stats->header1.flags = 0;
	stats->header1.type = UPGT_H1_TYPE_CTRL;
	stats->header1.len = htole16(
	    sizeof(struct upgt_lmac_stats) - sizeof(struct upgt_lmac_header));

	stats->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	stats->header2.type = htole16(UPGT_H2_TYPE_STATS);
	stats->header2.flags = 0;

	len = sizeof(*mem) + sizeof(*stats);

	mem->chksum = upgt_chksum_le((uint32_t *)stats,
	    len - sizeof(*mem));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		device_printf(sc->sc_dev,
		    "could not transmit statistics CMD data URB!\n");
		return (EIO);
	}

	return (0);
}

static int
upgt_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		mtx_lock(&Giant);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->sc_if_flags) &
				    (IFF_ALLMULTI | IFF_PROMISC))
					upgt_set_multi(sc);
			} else {
				upgt_init(sc);
				startall = 1;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				upgt_stop(sc, 1);
		}
		sc->sc_if_flags = ifp->if_flags;
		if (startall)
			ieee80211_start_all(ic);
		mtx_unlock(&Giant);
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
	return error;
}

static void
upgt_stop(struct upgt_softc *sc, int disable)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* abort and close TX / RX pipes */
	if (sc->sc_tx_pipeh != NULL)
		usbd_abort_pipe(sc->sc_tx_pipeh);
	if (sc->sc_rx_pipeh != NULL)
		usbd_abort_pipe(sc->sc_rx_pipeh);

	/* device down */
	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}

static void
upgt_task(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct upgt_vap *uvp = UPGT_VAP(vap);

	DPRINTF(sc, UPGT_DEBUG_STATE, "%s: %s -> %s\n", __func__,
	    ieee80211_state_name[vap->iv_state],
	    ieee80211_state_name[sc->sc_state]);
	
	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		/* do not accept any frames if the device is down */
		UPGT_LOCK(sc);
		upgt_set_macfilter(sc, sc->sc_state);
		UPGT_UNLOCK(sc);
		upgt_set_led(sc, UPGT_LED_OFF);
		break;
	case IEEE80211_S_SCAN:
		upgt_set_chan(sc, ic->ic_curchan);
		break;
	case IEEE80211_S_AUTH:
		upgt_set_chan(sc, ic->ic_curchan);
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		UPGT_LOCK(sc);
		upgt_set_macfilter(sc, sc->sc_state);
		UPGT_UNLOCK(sc);
		upgt_set_led(sc, UPGT_LED_ON);
		break;
	default:
		break;
	}

	IEEE80211_LOCK(ic);
	uvp->newstate(vap, sc->sc_state, sc->sc_arg);
	if (vap->iv_newstate_cb != NULL)
		vap->iv_newstate_cb(vap, sc->sc_state, sc->sc_arg);
	IEEE80211_UNLOCK(ic);
}

static void
upgt_set_led(struct upgt_softc *sc, int action)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_led *led;
	int len;

	/*
	 * Transmit the URB containing the CMD data.
	 */
	bzero(data_cmd->buf, MCLBYTES);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	led = (struct upgt_lmac_led *)(mem + 1);

	led->header1.flags = UPGT_H1_FLAGS_TX_NO_CALLBACK;
	led->header1.type = UPGT_H1_TYPE_CTRL;
	led->header1.len = htole16(
	    sizeof(struct upgt_lmac_led) -
	    sizeof(struct upgt_lmac_header));

	led->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	led->header2.type = htole16(UPGT_H2_TYPE_LED);
	led->header2.flags = 0;

	switch (action) {
	case UPGT_LED_OFF:
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = 0;
		led->action_tmp = htole16(UPGT_LED_ACTION_OFF);
		led->action_tmp_dur = 0;
		break;
	case UPGT_LED_ON:
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = 0;
		led->action_tmp = htole16(UPGT_LED_ACTION_ON);
		led->action_tmp_dur = 0;
		break;
	case UPGT_LED_BLINK:
		if (sc->sc_state != IEEE80211_S_RUN)
			return;
		if (sc->sc_led_blink)
			/* previous blink was not finished */
			return;
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = htole16(UPGT_LED_ACTION_OFF);
		led->action_tmp = htole16(UPGT_LED_ACTION_ON);
		led->action_tmp_dur = htole16(UPGT_LED_ACTION_TMP_DUR);
		/* lock blink */
		sc->sc_led_blink = 1;
		callout_reset(&sc->sc_led_ch, hz, upgt_set_led_blink, sc);
		break;
	default:
		return;
	}

	len = sizeof(*mem) + sizeof(*led);

	mem->chksum = upgt_chksum_le((uint32_t *)led,
	    len - sizeof(*mem));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0)
		device_printf(sc->sc_dev, "could not transmit led CMD URB!\n");
}

static void
upgt_set_led_blink(void *arg)
{
	struct upgt_softc *sc = arg;

	/* blink finished, we are ready for a next one */
	sc->sc_led_blink = 0;
}

static void
upgt_init(void *priv)
{
	struct upgt_softc *sc = priv;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	UPGT_LOCK(sc);
	upgt_init_locked(sc);
	UPGT_UNLOCK(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ieee80211_start_all(ic);		/* start all vap's */
}

static void
upgt_init_locked(struct upgt_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	DPRINTF(sc, UPGT_DEBUG_RESET, "setting MAC address to %s\n",
	    ether_sprintf(ic->ic_myaddr));

	upgt_set_macfilter(sc, IEEE80211_S_SCAN);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
}

static int
upgt_set_macfilter(struct upgt_softc *sc, uint8_t state)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = vap->iv_bss;
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_filter *filter;
	int len;
	uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	/*
	 * Transmit the URB containing the CMD data.
	 */
	bzero(data_cmd->buf, MCLBYTES);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	filter = (struct upgt_lmac_filter *)(mem + 1);

	filter->header1.flags = UPGT_H1_FLAGS_TX_NO_CALLBACK;
	filter->header1.type = UPGT_H1_TYPE_CTRL;
	filter->header1.len = htole16(
	    sizeof(struct upgt_lmac_filter) -
	    sizeof(struct upgt_lmac_header));

	filter->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	filter->header2.type = htole16(UPGT_H2_TYPE_MACFILTER);
	filter->header2.flags = 0;

	switch (state) {
	case IEEE80211_S_INIT:
		DPRINTF(sc, UPGT_DEBUG_STATE, "%s: set MAC filter to INIT\n",
		    __func__);
		filter->type = htole16(UPGT_FILTER_TYPE_RESET);
		break;
	case IEEE80211_S_SCAN:
		DPRINTF(sc, UPGT_DEBUG_STATE,
		    "set MAC filter to SCAN (bssid %s)\n",
		    ether_sprintf(broadcast));
		filter->type = htole16(UPGT_FILTER_TYPE_NONE);
		IEEE80211_ADDR_COPY(filter->dst, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(filter->src, broadcast);
		filter->unknown1 = htole16(UPGT_FILTER_UNKNOWN1);
		filter->rxaddr = htole32(sc->sc_memaddr_rx_start);
		filter->unknown2 = htole16(UPGT_FILTER_UNKNOWN2);
		filter->rxhw = htole32(sc->sc_eeprom_hwrx);
		filter->unknown3 = htole16(UPGT_FILTER_UNKNOWN3);
		break;
	case IEEE80211_S_RUN:
		/* XXX monitor mode isn't tested yet.  */
		if (vap->iv_opmode == IEEE80211_M_MONITOR) {
			filter->type = htole16(UPGT_FILTER_TYPE_MONITOR);
			IEEE80211_ADDR_COPY(filter->dst, ic->ic_myaddr);
			IEEE80211_ADDR_COPY(filter->src, ni->ni_bssid);
			filter->unknown1 = htole16(UPGT_FILTER_MONITOR_UNKNOWN1);
			filter->rxaddr = htole32(sc->sc_memaddr_rx_start);
			filter->unknown2 = htole16(UPGT_FILTER_MONITOR_UNKNOWN2);
			filter->rxhw = htole32(sc->sc_eeprom_hwrx);
			filter->unknown3 = htole16(UPGT_FILTER_MONITOR_UNKNOWN3);
		} else {
			DPRINTF(sc, UPGT_DEBUG_STATE,
			    "set MAC filter to RUN (bssid %s)\n",
			    ether_sprintf(ni->ni_bssid));
			filter->type = htole16(UPGT_FILTER_TYPE_STA);
			IEEE80211_ADDR_COPY(filter->dst, ic->ic_myaddr);
			IEEE80211_ADDR_COPY(filter->src, ni->ni_bssid);
			filter->unknown1 = htole16(UPGT_FILTER_UNKNOWN1);
			filter->rxaddr = htole32(sc->sc_memaddr_rx_start);
			filter->unknown2 = htole16(UPGT_FILTER_UNKNOWN2);
			filter->rxhw = htole32(sc->sc_eeprom_hwrx);
			filter->unknown3 = htole16(UPGT_FILTER_UNKNOWN3);
		}
		break;
	default:
		device_printf(sc->sc_dev,
		    "MAC filter does not know that state!\n");
		break;
	}

	len = sizeof(*mem) + sizeof(*filter);

	mem->chksum = upgt_chksum_le((uint32_t *)filter,
	    len - sizeof(*mem));

	UPGT_UNLOCK(sc);
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		device_printf(sc->sc_dev,
		    "could not transmit macfilter CMD data URB!\n");
		UPGT_LOCK(sc);
		return (EIO);
	}
	UPGT_LOCK(sc);

	return (0);
}

static void
upgt_setup_rates(struct ieee80211vap *vap, struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct upgt_softc *sc = ifp->if_softc;
	const struct ieee80211_txparam *tp;

	/*
	 * 0x01 = OFMD6   0x10 = DS1
	 * 0x04 = OFDM9   0x11 = DS2
	 * 0x06 = OFDM12  0x12 = DS5
	 * 0x07 = OFDM18  0x13 = DS11
	 * 0x08 = OFDM24
	 * 0x09 = OFDM36
	 * 0x0a = OFDM48
	 * 0x0b = OFDM54
	 */
	const uint8_t rateset_auto_11b[] =
	    { 0x13, 0x13, 0x12, 0x11, 0x11, 0x10, 0x10, 0x10 };
	const uint8_t rateset_auto_11g[] =
	    { 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x04, 0x01 };
	const uint8_t rateset_fix_11bg[] =
	    { 0x10, 0x11, 0x12, 0x13, 0x01, 0x04, 0x06, 0x07,
	      0x08, 0x09, 0x0a, 0x0b };

	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];

	/* XXX */
	if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE) {
		/*
		 * Automatic rate control is done by the device.
		 * We just pass the rateset from which the device
		 * will pickup a rate.
		 */
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			bcopy(rateset_auto_11b, sc->sc_cur_rateset,
			    sizeof(sc->sc_cur_rateset));
		if (ic->ic_curmode == IEEE80211_MODE_11G ||
		    ic->ic_curmode == IEEE80211_MODE_AUTO)
			bcopy(rateset_auto_11g, sc->sc_cur_rateset,
			    sizeof(sc->sc_cur_rateset));
	} else {
		/* set a fixed rate */
		memset(sc->sc_cur_rateset, rateset_fix_11bg[tp->ucastrate],
		    sizeof(sc->sc_cur_rateset));
	}
}

static void
upgt_set_multi(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (!(ifp->if_flags & IFF_UP))
		return;

	/*
	 * XXX don't know how to set a device.  Lack of docs.  Just try to set
	 * IFF_ALLMULTI flag here.
	 */
	IF_ADDR_LOCK(ifp);
	ifp->if_flags |= IFF_ALLMULTI;
	IF_ADDR_UNLOCK(ifp);
}

static void
upgt_start(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct upgt_data *data_tx;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int i;

	UPGT_LOCK(sc);
	for (i = 0; i < upgt_txbuf; i++) {
		data_tx = &sc->tx_data[i];
		if (data_tx->use == 1)
			continue;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m = ieee80211_encap(ni, m);
		if (m == NULL) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}
		
		if ((data_tx->addr = upgt_mem_alloc(sc)) == 0) {
			device_printf(sc->sc_dev, "no free prism memory!\n");
			UPGT_UNLOCK(sc);
			return;
		}
		data_tx->ni = ni;
		data_tx->m = m;
		data_tx->use = 1;
		sc->tx_queued++;
	}

	if (sc->tx_queued > 0) {
		DPRINTF(sc, UPGT_DEBUG_XMIT, "tx_queued=%d\n", sc->tx_queued);

		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		sc->sc_tx_timer = 5;
		callout_reset(&sc->sc_watchdog_ch, hz, upgt_watchdog, sc);
		/* process the TX queue in process context */
		usb_rem_task(sc->sc_udev, &sc->sc_task_tx);
		usb_add_task(sc->sc_udev, &sc->sc_task_tx, USB_TASKQ_DRIVER);
	}
	UPGT_UNLOCK(sc);
}

static int
upgt_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct upgt_softc *sc = ifp->if_softc;
	struct upgt_data *data_tx = NULL;
	int i;

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}

	UPGT_LOCK(sc);
	if (sc->tx_queued >= upgt_txbuf) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		m_freem(m);
		ieee80211_free_node(ni);
		UPGT_UNLOCK(sc);
		return ENOBUFS;		/* XXX */
	}

	ifp->if_opackets++;

	/* choose a unused buffer.  */
	for (i = 0; i < upgt_txbuf; i++) {
		data_tx = &sc->tx_data[i];
		if (data_tx->use == 0)
			break;
	}
	KASSERT(data_tx != NULL, ("data_tx is NULL"));
	KASSERT(data_tx->use == 0, ("no empty TX queue"));
	if ((data_tx->addr = upgt_mem_alloc(sc)) == 0) {
		device_printf(sc->sc_dev, "no free prism memory!\n");
		UPGT_UNLOCK(sc);
		return ENOBUFS;
	}

	if (bpf_peers_present(ifp->if_bpf)) {
		struct upgt_tx_radiotap_header *tap = &sc->sc_txtap;
		
		tap->wt_flags = 0;
		tap->wt_rate = 0;	/* TODO: where to get from? */
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		
		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m);
	}

	data_tx->ni = ni;
	data_tx->m = m;
	data_tx->use = 1;
	sc->tx_queued++;
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	UPGT_UNLOCK(sc);

	sc->sc_tx_timer = 5;
	callout_reset(&sc->sc_watchdog_ch, hz, upgt_watchdog, sc);
	usb_rem_task(sc->sc_udev, &sc->sc_task_tx);
	usb_add_task(sc->sc_udev, &sc->sc_task_tx, USB_TASKQ_DRIVER);

	return 0;
}

static void
upgt_watchdog(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev, "watchdog timeout\n");
			/* upgt_init(ifp); XXX needs a process context ? */
			ifp->if_oerrors++;
			return;
		}
		callout_reset(&sc->sc_watchdog_ch, hz, upgt_watchdog, sc);
	}
}

static uint32_t
upgt_mem_alloc(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < sc->sc_memory.pages; i++) {
		if (sc->sc_memory.page[i].used == 0) {
			sc->sc_memory.page[i].used = 1;
			return (sc->sc_memory.page[i].addr);
		}
	}

	return (0);
}

static void
upgt_scantask(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	switch (sc->sc_scan_action) {
        case UPGT_SET_CHANNEL:
                upgt_set_chan(sc, ic->ic_curchan);
                break;
        default:
                device_printf(sc->sc_dev, "unknown scan action %d\n",
		    sc->sc_scan_action);
                break;
        }
}

static void
upgt_scan_start(struct ieee80211com *ic)
{
	/* do nothing.  */
}

static void
upgt_scan_end(struct ieee80211com *ic)
{
	/* do nothing.  */
}

static void
upgt_set_channel(struct ieee80211com *ic)
{
	struct upgt_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_scantask);

	/* do it in a process context */
	sc->sc_scan_action = UPGT_SET_CHANNEL;
	usb_add_task(sc->sc_udev, &sc->sc_scantask, USB_TASKQ_DRIVER);
}

static void
upgt_set_chan(struct upgt_softc *sc, struct ieee80211_channel *c)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_channel *chan;
	int len, channel;

	channel = ieee80211_chan2ieee(ic, c);
	if (channel == 0 || channel == IEEE80211_CHAN_ANY) {
		/* XXX should NEVER happen */
		device_printf(sc->sc_dev,
		    "%s: invalid channel %x\n", __func__, channel);
		return;
	}
	
	DPRINTF(sc, UPGT_DEBUG_STATE, "%s: channel %d\n", __func__, channel);

	/*
	 * Transmit the URB containing the CMD data.
	 */
	bzero(data_cmd->buf, MCLBYTES);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	chan = (struct upgt_lmac_channel *)(mem + 1);

	chan->header1.flags = UPGT_H1_FLAGS_TX_NO_CALLBACK;
	chan->header1.type = UPGT_H1_TYPE_CTRL;
	chan->header1.len = htole16(
	    sizeof(struct upgt_lmac_channel) - sizeof(struct upgt_lmac_header));

	chan->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	chan->header2.type = htole16(UPGT_H2_TYPE_CHANNEL);
	chan->header2.flags = 0;

	chan->unknown1 = htole16(UPGT_CHANNEL_UNKNOWN1);
	chan->unknown2 = htole16(UPGT_CHANNEL_UNKNOWN2);
	chan->freq6 = sc->sc_eeprom_freq6[channel];
	chan->settings = sc->sc_eeprom_freq6_settings;
	chan->unknown3 = UPGT_CHANNEL_UNKNOWN3;

	bcopy(&sc->sc_eeprom_freq3[channel].data, chan->freq3_1,
	    sizeof(chan->freq3_1));
	bcopy(&sc->sc_eeprom_freq4[channel], chan->freq4,
	    sizeof(sc->sc_eeprom_freq4[channel]));
	bcopy(&sc->sc_eeprom_freq3[channel].data, chan->freq3_2,
	    sizeof(chan->freq3_2));

	len = sizeof(*mem) + sizeof(*chan);

	mem->chksum = upgt_chksum_le((uint32_t *)chan,
	    len - sizeof(*mem));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0)
		device_printf(sc->sc_dev,
		    "could not transmit channel CMD data URB!\n");
}

static struct ieee80211vap *
upgt_vap_create(struct ieee80211com *ic,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct upgt_vap *uvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return NULL;
	uvp = (struct upgt_vap *) malloc(sizeof(struct upgt_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (uvp == NULL)
		return NULL;
	vap = &uvp->vap;
	/* enable s/w bmiss handling for sta mode */
	ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags | IEEE80211_CLONE_NOBEACONS, bssid, mac);

	/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = upgt_newstate;

	/* setup device rates */
	upgt_setup_rates(vap, ic);

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status);
	ic->ic_opmode = opmode;
	return vap;
}

static int
upgt_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct upgt_vap *uvp = UPGT_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct upgt_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_task);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;

	if (nstate == IEEE80211_S_INIT) {
		uvp->newstate(vap, nstate, arg);
		return 0;
	} else {
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);
		return EINPROGRESS;
	}
}

static void
upgt_vap_delete(struct ieee80211vap *vap)
{
	struct upgt_vap *uvp = UPGT_VAP(vap);

	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

static void
upgt_update_mcast(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;

	usb_add_task(sc->sc_udev, &sc->sc_mcasttask, USB_TASKQ_DRIVER);
}

static int
upgt_eeprom_parse(struct upgt_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct upgt_eeprom_header *eeprom_header;
	struct upgt_eeprom_option *eeprom_option;
	uint16_t option_len;
	uint16_t option_type;
	uint16_t preamble_len;
	int option_end = 0;

	/* calculate eeprom options start offset */
	eeprom_header = (struct upgt_eeprom_header *)sc->sc_eeprom;
	preamble_len = le16toh(eeprom_header->preamble_len);
	eeprom_option = (struct upgt_eeprom_option *)(sc->sc_eeprom +
	    (sizeof(struct upgt_eeprom_header) + preamble_len));

	while (!option_end) {
		/* the eeprom option length is stored in words */
		option_len =
		    (le16toh(eeprom_option->len) - 1) * sizeof(uint16_t);
		option_type =
		    le16toh(eeprom_option->type);

		switch (option_type) {
		case UPGT_EEPROM_TYPE_NAME:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM name len=%d\n", option_len);
			break;
		case UPGT_EEPROM_TYPE_SERIAL:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM serial len=%d\n", option_len);
			break;
		case UPGT_EEPROM_TYPE_MAC:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM mac len=%d\n", option_len);

			IEEE80211_ADDR_COPY(ic->ic_myaddr, eeprom_option->data);
			break;
		case UPGT_EEPROM_TYPE_HWRX:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM hwrx len=%d\n", option_len);

			upgt_eeprom_parse_hwrx(sc, eeprom_option->data);
			break;
		case UPGT_EEPROM_TYPE_CHIP:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM chip len=%d\n", option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ3:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM freq3 len=%d\n", option_len);

			upgt_eeprom_parse_freq3(sc, eeprom_option->data,
			    option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ4:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM freq4 len=%d\n", option_len);

			upgt_eeprom_parse_freq4(sc, eeprom_option->data,
			    option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ5:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM freq5 len=%d\n", option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ6:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM freq6 len=%d\n", option_len);

			upgt_eeprom_parse_freq6(sc, eeprom_option->data,
			    option_len);
			break;
		case UPGT_EEPROM_TYPE_END:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM end len=%d\n", option_len);
			option_end = 1;
			break;
		case UPGT_EEPROM_TYPE_OFF:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "%s: EEPROM off without end option!\n", __func__);
			return (EIO);
		default:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "EEPROM unknown type 0x%04x len=%d\n",
			    option_type, option_len);
			break;
		}

		/* jump to next EEPROM option */
		eeprom_option = (struct upgt_eeprom_option *)
		    (eeprom_option->data + option_len);
	}

	return (0);
}

static void
upgt_eeprom_parse_freq3(struct upgt_softc *sc, uint8_t *data, int len)
{
	struct upgt_eeprom_freq3_header *freq3_header;
	struct upgt_lmac_freq3 *freq3;
	int i, elements, flags;
	unsigned channel;

	freq3_header = (struct upgt_eeprom_freq3_header *)data;
	freq3 = (struct upgt_lmac_freq3 *)(freq3_header + 1);

	flags = freq3_header->flags;
	elements = freq3_header->elements;

	DPRINTF(sc, UPGT_DEBUG_FW, "flags=0x%02x elements=%d\n",
	    flags, elements);

	for (i = 0; i < elements; i++) {
		channel = ieee80211_mhz2ieee(le16toh(freq3[i].freq), 0);
		if (!(channel >= 0 && channel < IEEE80211_CHAN_MAX))
			continue;

		sc->sc_eeprom_freq3[channel] = freq3[i];

		DPRINTF(sc, UPGT_DEBUG_FW, "frequence=%d, channel=%d\n",
		    le16toh(sc->sc_eeprom_freq3[channel].freq), channel);
	}
}

void
upgt_eeprom_parse_freq4(struct upgt_softc *sc, uint8_t *data, int len)
{
	struct upgt_eeprom_freq4_header *freq4_header;
	struct upgt_eeprom_freq4_1 *freq4_1;
	struct upgt_eeprom_freq4_2 *freq4_2;
	int i, j, elements, settings, flags;
	unsigned channel;

	freq4_header = (struct upgt_eeprom_freq4_header *)data;
	freq4_1 = (struct upgt_eeprom_freq4_1 *)(freq4_header + 1);
	flags = freq4_header->flags;
	elements = freq4_header->elements;
	settings = freq4_header->settings;

	/* we need this value later */
	sc->sc_eeprom_freq6_settings = freq4_header->settings;

	DPRINTF(sc, UPGT_DEBUG_FW, "flags=0x%02x elements=%d settings=%d\n",
	    flags, elements, settings);

	for (i = 0; i < elements; i++) {
		channel = ieee80211_mhz2ieee(le16toh(freq4_1[i].freq), 0);
		if (!(channel >= 0 && channel < IEEE80211_CHAN_MAX))
			continue;

		freq4_2 = (struct upgt_eeprom_freq4_2 *)freq4_1[i].data;
		for (j = 0; j < settings; j++) {
			sc->sc_eeprom_freq4[channel][j].cmd = freq4_2[j];
			sc->sc_eeprom_freq4[channel][j].pad = 0;
		}

		DPRINTF(sc, UPGT_DEBUG_FW, "frequence=%d, channel=%d\n",
		    le16toh(freq4_1[i].freq), channel);
	}
}

void
upgt_eeprom_parse_freq6(struct upgt_softc *sc, uint8_t *data, int len)
{
	struct upgt_lmac_freq6 *freq6;
	int i, elements;
	unsigned channel;

	freq6 = (struct upgt_lmac_freq6 *)data;
	elements = len / sizeof(struct upgt_lmac_freq6);

	DPRINTF(sc, UPGT_DEBUG_FW, "elements=%d\n", elements);

	for (i = 0; i < elements; i++) {
		channel = ieee80211_mhz2ieee(le16toh(freq6[i].freq), 0);
		if (!(channel >= 0 && channel < IEEE80211_CHAN_MAX))
			continue;

		sc->sc_eeprom_freq6[channel] = freq6[i];

		DPRINTF(sc, UPGT_DEBUG_FW, "frequence=%d, channel=%d\n",
		    le16toh(sc->sc_eeprom_freq6[channel].freq), channel);
	}
}

static void
upgt_eeprom_parse_hwrx(struct upgt_softc *sc, uint8_t *data)
{
	struct upgt_eeprom_option_hwrx *option_hwrx;

	option_hwrx = (struct upgt_eeprom_option_hwrx *)data;

	sc->sc_eeprom_hwrx = option_hwrx->rxfilter - UPGT_EEPROM_RX_CONST;

	DPRINTF(sc, UPGT_DEBUG_FW, "hwrx option value=0x%04x\n",
	    sc->sc_eeprom_hwrx);
}

static int
upgt_eeprom_read(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_eeprom	*eeprom;
	int offset, block, len;

	offset = 0;
	block = UPGT_EEPROM_BLOCK_SIZE;
	while (offset < UPGT_EEPROM_SIZE) {
		DPRINTF(sc, UPGT_DEBUG_FW,
		    "request EEPROM block (offset=%d, len=%d)\n", offset, block);

		/*
		 * Transmit the URB containing the CMD data.
		 */
		bzero(data_cmd->buf, MCLBYTES);

		mem = (struct upgt_lmac_mem *)data_cmd->buf;
		mem->addr = htole32(sc->sc_memaddr_frame_start +
		    UPGT_MEMSIZE_FRAME_HEAD);

		eeprom = (struct upgt_lmac_eeprom *)(mem + 1);
		eeprom->header1.flags = 0;
		eeprom->header1.type = UPGT_H1_TYPE_CTRL;
		eeprom->header1.len = htole16((
		    sizeof(struct upgt_lmac_eeprom) -
		    sizeof(struct upgt_lmac_header)) + block);

		eeprom->header2.reqid = htole32(sc->sc_memaddr_frame_start);
		eeprom->header2.type = htole16(UPGT_H2_TYPE_EEPROM);
		eeprom->header2.flags = 0;

		eeprom->offset = htole16(offset);
		eeprom->len = htole16(block);

		len = sizeof(*mem) + sizeof(*eeprom) + block;

		mem->chksum = upgt_chksum_le((uint32_t *)eeprom,
		    len - sizeof(*mem));

		if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len,
		    USBD_FORCE_SHORT_XFER) != 0) {
			device_printf(sc->sc_dev,
			    "could not transmit EEPROM data URB!\n");
			return (EIO);
		}
		if (tsleep(sc, 0, "eeprom_request", UPGT_USB_TIMEOUT)) {
			device_printf(sc->sc_dev,
			    "timeout while waiting for EEPROM data!\n");
			return (EIO);
		}

		offset += block;
		if (UPGT_EEPROM_SIZE - offset < block)
			block = UPGT_EEPROM_SIZE - offset;
	}

	return (0);
}

/*
 * The firmware awaits a checksum for each frame we send to it.
 * The algorithm used therefor is uncommon but somehow similar to CRC32.
 */
static uint32_t
upgt_chksum_le(const uint32_t *buf, size_t size)
{
	int i;
	uint32_t crc = 0;

	for (i = 0; i < size; i += sizeof(uint32_t)) {
		crc = htole32(crc ^ *buf++);
		crc = htole32((crc >> 5) ^ (crc << 3));
	}

	return (crc);
}

static void
upgt_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct upgt_data *data_rx = priv;
	struct upgt_softc *sc = data_rx->sc;
	int len;
	struct upgt_lmac_header *header;
	struct upgt_lmac_eeprom *eeprom;
	uint8_t h1_type;
	uint16_t h2_type;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipeh);
		goto skip;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	/*
	 * Check what type of frame came in.
	 */
	header = (struct upgt_lmac_header *)(data_rx->buf + 4);

	h1_type = header->header1.type;
	h2_type = le16toh(header->header2.type);

	if (h1_type == UPGT_H1_TYPE_CTRL && h2_type == UPGT_H2_TYPE_EEPROM) {
		eeprom = (struct upgt_lmac_eeprom *)(data_rx->buf + 4);
		uint16_t eeprom_offset = le16toh(eeprom->offset);
		uint16_t eeprom_len = le16toh(eeprom->len);

		DPRINTF(sc, UPGT_DEBUG_FW,
		    "received EEPROM block (offset=%d, len=%d)\n",
		    eeprom_offset, eeprom_len);

		bcopy(data_rx->buf + sizeof(struct upgt_lmac_eeprom) + 4,
			sc->sc_eeprom + eeprom_offset, eeprom_len);

		/* EEPROM data has arrived in time, wakeup tsleep() */
		wakeup(sc);
	} else if (h1_type == UPGT_H1_TYPE_CTRL &&
	    h2_type == UPGT_H2_TYPE_TX_DONE) {
		DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: received 802.11 TX done\n",
		    __func__);
		upgt_tx_done(sc, data_rx->buf + 4);
	} else if (h1_type == UPGT_H1_TYPE_RX_DATA ||
	    h1_type == UPGT_H1_TYPE_RX_DATA_MGMT) {
		DPRINTF(sc, UPGT_DEBUG_RECV, "%s: received 802.11 RX data\n",
		    __func__);
		upgt_rx(sc, data_rx->buf + 4, le16toh(header->header1.len));
	} else if (h1_type == UPGT_H1_TYPE_CTRL &&
	    h2_type == UPGT_H2_TYPE_STATS) {
		DPRINTF(sc, UPGT_DEBUG_STAT, "%s: received statistic data\n",
		    __func__);
		/* TODO: what could we do with the statistic data? */
	} else {
		/* ignore unknown frame types */
		DPRINTF(sc, UPGT_DEBUG_INTR,
		    "received unknown frame type 0x%02x\n",
		    header->header1.type);
	}

skip:	/* setup new transfer */
	usbd_setup_xfer(xfer, sc->sc_rx_pipeh, data_rx, data_rx->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, upgt_rxeof);
	(void)usbd_transfer(xfer);
}

static void
upgt_rx(struct upgt_softc *sc, uint8_t *data, int pkglen)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct upgt_lmac_rx_desc *rxdesc;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int nf;

	/*
	 * don't pass packets to the ieee80211 framework if the driver isn't
	 * RUNNING.
	 */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	/* access RX packet descriptor */
	rxdesc = (struct upgt_lmac_rx_desc *)data;

	/* create mbuf which is suitable for strict alignment archs */
	KASSERT((pkglen + ETHER_ALIGN) < MCLBYTES,
	    ("A current mbuf storage is small (%d)", pkglen + ETHER_ALIGN));
	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		device_printf(sc->sc_dev, "could not create RX mbuf!\n");
		return;
	}
	m_adj(m, ETHER_ALIGN);
	bcopy(rxdesc->data, mtod(m, char *), pkglen);
	/* trim FCS */
	m->m_len = m->m_pkthdr.len = pkglen - IEEE80211_CRC_LEN;
	m->m_pkthdr.rcvif = ifp;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct upgt_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = upgt_rx_rate(sc, rxdesc->rate);
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_antsignal = rxdesc->rssi;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
	}
	ifp->if_ipackets++;

	nf = -95;	/* XXX */
	ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));
	if (ni != NULL) {
		(void)ieee80211_input(ni, m, rxdesc->rssi, nf, 0);
		ieee80211_free_node(ni);
	} else
		(void)ieee80211_input_all(ic, m, rxdesc->rssi, nf, 0);

	DPRINTF(sc, UPGT_DEBUG_RX_PROC, "%s: RX done\n", __func__);
}

static uint8_t
upgt_rx_rate(struct upgt_softc *sc, const int rate)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static const uint8_t cck_upgt2rate[4] = { 2, 4, 11, 22 };
	static const uint8_t ofdm_upgt2rate[12] =
	    { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
	
	if (ic->ic_curmode == IEEE80211_MODE_11B &&
	    !(rate < 0 || rate > 3))
		return cck_upgt2rate[rate & 0xf];

	if (ic->ic_curmode == IEEE80211_MODE_11G &&
	    !(rate < 0 || rate > 11))
		return ofdm_upgt2rate[rate & 0xf];

	return (0);
}

static void
upgt_tx_done(struct upgt_softc *sc, uint8_t *data)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct upgt_lmac_tx_done_desc *desc;
	int i;

	desc = (struct upgt_lmac_tx_done_desc *)data;

	UPGT_LOCK(sc);
	for (i = 0; i < upgt_txbuf; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		if (data_tx->addr == le32toh(desc->header2.reqid)) {
			upgt_mem_free(sc, data_tx->addr);
			ieee80211_free_node(data_tx->ni);
			data_tx->ni = NULL;
			data_tx->addr = 0;
			data_tx->m = NULL;
			data_tx->use = 0;

			sc->tx_queued--;
			ifp->if_opackets++;

			DPRINTF(sc, UPGT_DEBUG_TX_PROC,
			    "TX done: memaddr=0x%08x, status=0x%04x, rssi=%d, ",
			    le32toh(desc->header2.reqid),
			    le16toh(desc->status), le16toh(desc->rssi));
			DPRINTF(sc, UPGT_DEBUG_TX_PROC, "seq=%d\n",
			    le16toh(desc->seq));
			break;
		}
	}
	if (sc->tx_queued == 0) {
		/* TX queued was processed, continue */
		sc->sc_tx_timer = 0;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		UPGT_UNLOCK(sc);
		upgt_start(ifp);
		return;
	}
	UPGT_UNLOCK(sc);
}

static void
upgt_mem_free(struct upgt_softc *sc, uint32_t addr)
{
	int i;

	for (i = 0; i < sc->sc_memory.pages; i++) {
		if (sc->sc_memory.page[i].addr == addr) {
			sc->sc_memory.page[i].used = 0;
			return;
		}
	}

	device_printf(sc->sc_dev,
	    "could not free memory address 0x%08x!\n", addr);
}

static int
upgt_fw_load(struct upgt_softc *sc)
{
	const struct firmware *fw;
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_data *data_rx = &sc->rx_data;
	struct upgt_fw_x2_header *x2;
	char start_fwload_cmd[] = { 0x3c, 0x0d };
	int error = 0, offset, bsize, n, i, len;
	uint32_t crc32;

	fw = firmware_get(upgt_fwname);
	if (fw == NULL) {
		device_printf(sc->sc_dev, "could not read microcode %s!\n",
		    upgt_fwname);
		return EIO;
	}

	/* send firmware start load command */
	len = sizeof(start_fwload_cmd);
	bcopy(start_fwload_cmd, data_cmd->buf, len);
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		device_printf(sc->sc_dev,
		    "could not send start_firmware_load command!\n");
		error = EIO;
		goto fail;
	}

	/* send X2 header */
	len = sizeof(struct upgt_fw_x2_header);
	x2 = (struct upgt_fw_x2_header *)data_cmd->buf;
	bcopy(UPGT_X2_SIGNATURE, x2->signature, UPGT_X2_SIGNATURE_SIZE);
	x2->startaddr = htole32(UPGT_MEMADDR_FIRMWARE_START);
	x2->len = htole32(fw->datasize);
	x2->crc = upgt_crc32_le((uint8_t *)data_cmd->buf +
	    UPGT_X2_SIGNATURE_SIZE,
	    sizeof(struct upgt_fw_x2_header) - UPGT_X2_SIGNATURE_SIZE -
	    sizeof(uint32_t));
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		device_printf(sc->sc_dev,
		    "could not send firmware X2 header!\n");
		error = EIO;
		goto fail;
	}

	/* download firmware */
	for (offset = 0; offset < fw->datasize; offset += bsize) {
		if (fw->datasize - offset > UPGT_FW_BLOCK_SIZE)
			bsize = UPGT_FW_BLOCK_SIZE;
		else
			bsize = fw->datasize - offset;

		n = upgt_fw_copy((const uint8_t *)fw->data + offset,
		    data_cmd->buf, bsize);

		DPRINTF(sc, UPGT_DEBUG_FW, "FW offset=%d, read=%d, sent=%d\n",
		    offset, n, bsize);

		if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &bsize, 0)
		    != 0) {
			device_printf(sc->sc_dev,
			    "error while downloading firmware block!\n");
			error = EIO;
			goto fail;
		}

		bsize = n;
	}
	DPRINTF(sc, UPGT_DEBUG_FW, "%s: firmware downloaded\n", __func__);

	/* load firmware */
	crc32 = upgt_crc32_le(fw->data, fw->datasize);
	*((uint32_t *)(data_cmd->buf)    ) = crc32;
	*((uint8_t  *)(data_cmd->buf) + 4) = 'g';
	*((uint8_t  *)(data_cmd->buf) + 5) = '\r';
	len = 6;
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		device_printf(sc->sc_dev,
		    "could not send load_firmware command!\n");
		error = EIO;
		goto fail;
	}

	for (i = 0; i < UPGT_FIRMWARE_TIMEOUT; i++) {
		len = UPGT_FW_BLOCK_SIZE;
		bzero(data_rx->buf, MCLBYTES);
		if (upgt_bulk_xmit(sc, data_rx, sc->sc_rx_pipeh, &len,
		    USBD_SHORT_XFER_OK) != 0) {
			device_printf(sc->sc_dev,
			    "could not read firmware response!\n");
			error = EIO;
			goto fail;
		}

		if (memcmp(data_rx->buf, "OK", 2) == 0)
			break;	/* firmware load was successful */
	}
	if (i == UPGT_FIRMWARE_TIMEOUT) {
		device_printf(sc->sc_dev, "firmware load failed!\n");
		error = EIO;
	}

	DPRINTF(sc, UPGT_DEBUG_FW, "%s: firmware loaded\n", __func__);
fail:
	firmware_put(fw, FIRMWARE_UNLOAD);
	return (error);
}

static uint32_t
upgt_crc32_le(const void *buf, size_t size)
{
	uint32_t crc;

	crc = ether_crc32_le(buf, size);

	/* apply final XOR value as common for CRC-32 */
	crc = htole32(crc ^ 0xffffffffU);

	return (crc);
}

/*
 * While copying the version 2 firmware, we need to replace two characters:
 *
 * 0x7e -> 0x7d 0x5e
 * 0x7d -> 0x7d 0x5d
 */
static int
upgt_fw_copy(const uint8_t *src, char *dst, int size)
{
	int i, j;
	
	for (i = 0, j = 0; i < size && j < size; i++) {
		switch (src[i]) {
		case 0x7e:
			dst[j] = 0x7d;
			j++;
			dst[j] = 0x5e;
			j++;
			break;
		case 0x7d:
			dst[j] = 0x7d;
			j++;
			dst[j] = 0x5d;
			j++;
			break;
		default:
			dst[j] = src[i];
			j++;
			break;
		}
	}

	return (i);
}

static int
upgt_mem_init(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < UPGT_MEMORY_MAX_PAGES; i++) {
		sc->sc_memory.page[i].used = 0;

		if (i == 0) {
			/*
			 * The first memory page is always reserved for
			 * command data.
			 */
			sc->sc_memory.page[i].addr =
			    sc->sc_memaddr_frame_start + MCLBYTES;
		} else {
			sc->sc_memory.page[i].addr =
			    sc->sc_memory.page[i - 1].addr + MCLBYTES;
		}

		if (sc->sc_memory.page[i].addr + MCLBYTES >=
		    sc->sc_memaddr_frame_end)
			break;

		DPRINTF(sc, UPGT_DEBUG_FW, "memory address page %d=0x%08x\n",
		    i, sc->sc_memory.page[i].addr);
	}

	sc->sc_memory.pages = i;
	if (upgt_txbuf > sc->sc_memory.pages)

	DPRINTF(sc, UPGT_DEBUG_FW, "memory pages=%d\n", sc->sc_memory.pages);
	return (0);
}

static int
upgt_fw_verify(struct upgt_softc *sc)
{
	const struct firmware *fw;
	const struct upgt_fw_bra_option *bra_opt;
	const struct upgt_fw_bra_descr *descr;
	const uint8_t *p;
	const uint32_t *uc;
	uint32_t bra_option_type, bra_option_len;
	int offset, bra_end = 0, error = 0;

	fw = firmware_get(upgt_fwname);
	if (fw == NULL) {
		device_printf(sc->sc_dev, "could not read microcode %s!\n",
		    upgt_fwname);
		return EIO;
	}

	/*
	 * Seek to beginning of Boot Record Area (BRA).
	 */
	for (offset = 0; offset < fw->datasize; offset += sizeof(*uc)) {
		uc = (const uint32_t *)((const uint8_t *)fw->data + offset);
		if (*uc == 0)
			break;
	}
	for (; offset < fw->datasize; offset += sizeof(*uc)) {
		uc = (const uint32_t *)((const uint8_t *)fw->data + offset);
		if (*uc != 0)
			break;
	}
	if (offset == fw->datasize) { 
		device_printf(sc->sc_dev,
		    "firmware Boot Record Area not found!\n");
		error = EIO;
		goto fail;
	}

	DPRINTF(sc, UPGT_DEBUG_FW,
	    "firmware Boot Record Area found at offset %d\n", offset);

	/*
	 * Parse Boot Record Area (BRA) options.
	 */
	while (offset < fw->datasize && bra_end == 0) {
		/* get current BRA option */
		p = (const uint8_t *)fw->data + offset;
		bra_opt = (const struct upgt_fw_bra_option *)p;
		bra_option_type = le32toh(bra_opt->type);
		bra_option_len = le32toh(bra_opt->len) * sizeof(*uc);

		switch (bra_option_type) {
		case UPGT_BRA_TYPE_FW:
			DPRINTF(sc, UPGT_DEBUG_FW, "UPGT_BRA_TYPE_FW len=%d\n",
			    bra_option_len);

			if (bra_option_len != UPGT_BRA_FWTYPE_SIZE) {
				device_printf(sc->sc_dev,
				    "wrong UPGT_BRA_TYPE_FW len!\n");
				error = EIO;
				goto fail;
			}
			if (memcmp(UPGT_BRA_FWTYPE_LM86, bra_opt->data,
			    bra_option_len) == 0) {
				sc->sc_fw_type = UPGT_FWTYPE_LM86;
				break;
			}
			if (memcmp(UPGT_BRA_FWTYPE_LM87, bra_opt->data,
			    bra_option_len) == 0) {
				sc->sc_fw_type = UPGT_FWTYPE_LM87;
				break;
			}
			device_printf(sc->sc_dev,
			    "unsupported firmware type!\n");
			error = EIO;
			goto fail;
		case UPGT_BRA_TYPE_VERSION:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "UPGT_BRA_TYPE_VERSION len=%d\n", bra_option_len);
			break;
		case UPGT_BRA_TYPE_DEPIF:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "UPGT_BRA_TYPE_DEPIF len=%d\n", bra_option_len);
			break;
		case UPGT_BRA_TYPE_EXPIF:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "UPGT_BRA_TYPE_EXPIF len=%d\n", bra_option_len);
			break;
		case UPGT_BRA_TYPE_DESCR:
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "UPGT_BRA_TYPE_DESCR len=%d\n", bra_option_len);

			descr = (const struct upgt_fw_bra_descr *)bra_opt->data;

			sc->sc_memaddr_frame_start =
			    le32toh(descr->memaddr_space_start);
			sc->sc_memaddr_frame_end =
			    le32toh(descr->memaddr_space_end);

			DPRINTF(sc, UPGT_DEBUG_FW,
			    "memory address space start=0x%08x\n",
			    sc->sc_memaddr_frame_start);
			DPRINTF(sc, UPGT_DEBUG_FW,
			    "memory address space end=0x%08x\n",
			    sc->sc_memaddr_frame_end);
			break;
		case UPGT_BRA_TYPE_END:
			DPRINTF(sc, UPGT_DEBUG_FW, "UPGT_BRA_TYPE_END len=%d\n",
			    bra_option_len);
			bra_end = 1;
			break;
		default:
			DPRINTF(sc, UPGT_DEBUG_FW, "unknown BRA option len=%d\n",
			    bra_option_len);
			error = EIO;
			goto fail;
		}

		/* jump to next BRA option */
		offset += sizeof(struct upgt_fw_bra_option) + bra_option_len;
	}

	DPRINTF(sc, UPGT_DEBUG_FW, "%s: firmware verified", __func__);
fail:
	firmware_put(fw, FIRMWARE_UNLOAD);
	return (error);
}

static int
upgt_bulk_xmit(struct upgt_softc *sc, struct upgt_data *data,
    usbd_pipe_handle pipeh, uint32_t *size, int flags)
{
        usbd_status status;

	mtx_lock(&Giant);
	status = usbd_bulk_transfer(data->xfer, pipeh,
	    USBD_NO_COPY | flags, UPGT_USB_TIMEOUT, data->buf, size,
	    "upgt_bulk_xmit");
	if (status != USBD_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "%s: error %s!\n",
		    __func__, usbd_errstr(status));
		mtx_unlock(&Giant);
		return (EIO);
	}
	mtx_unlock(&Giant);

	return (0);
}

static int
upgt_device_reset(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	char init_cmd[] = { 0x7e, 0x7e, 0x7e, 0x7e };
	int len;

	len = sizeof(init_cmd);
	bcopy(init_cmd, data_cmd->buf, len);
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		device_printf(sc->sc_dev,
		    "could not send device init string!\n");
		return (EIO);
	}
	usbd_delay_ms(sc->sc_udev, 100);

	DPRINTF(sc, UPGT_DEBUG_FW, "%s: device initialized\n", __func__);
	return (0);
}

static int
upgt_alloc_tx(struct upgt_softc *sc)
{
	int i;

	sc->tx_queued = 0;

	for (i = 0; i < upgt_txbuf; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		data_tx->sc = sc;
		data_tx->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data_tx->xfer == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate TX xfer!\n");
			return (ENOMEM);
		}

		data_tx->buf = usbd_alloc_buffer(data_tx->xfer, MCLBYTES);
		if (data_tx->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate TX buffer!\n");
			return (ENOMEM);
		}

		bzero(data_tx->buf, MCLBYTES);
	}

	return (0);
}

static int
upgt_alloc_rx(struct upgt_softc *sc)
{
	struct upgt_data *data_rx = &sc->rx_data;

	data_rx->sc = sc;
	data_rx->xfer = usbd_alloc_xfer(sc->sc_udev);
	if (data_rx->xfer == NULL) {
		device_printf(sc->sc_dev, "could not allocate RX xfer!\n");
		return (ENOMEM);
	}

	data_rx->buf = usbd_alloc_buffer(data_rx->xfer, MCLBYTES);
	if (data_rx->buf == NULL) {
		device_printf(sc->sc_dev, "could not allocate RX buffer!\n");
		return (ENOMEM);
	}

	bzero(data_rx->buf, MCLBYTES);

	return (0);
}

static int
upgt_alloc_cmd(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;

	data_cmd->sc = sc;
	data_cmd->xfer = usbd_alloc_xfer(sc->sc_udev);
	if (data_cmd->xfer == NULL) {
		device_printf(sc->sc_dev, "could not allocate RX xfer!\n");
		return (ENOMEM);
	}

	data_cmd->buf = usbd_alloc_buffer(data_cmd->xfer, MCLBYTES);
	if (data_cmd->buf == NULL) {
		device_printf(sc->sc_dev, "could not allocate RX buffer!\n");
		return (ENOMEM);
	}

	bzero(data_cmd->buf, MCLBYTES);

	return (0);
}

static int
upgt_detach(device_t dev)
{
	struct upgt_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	if (!device_is_attached(dev))
		return 0;

	upgt_stop(sc, 1);

	/* abort and close TX / RX pipes */
	if (sc->sc_tx_pipeh != NULL)
		usbd_close_pipe(sc->sc_tx_pipeh);
	if (sc->sc_rx_pipeh != NULL)
		usbd_close_pipe(sc->sc_rx_pipeh);

	mtx_destroy(&sc->sc_mtx);
	usb_rem_task(sc->sc_udev, &sc->sc_mcasttask);
	usb_rem_task(sc->sc_udev, &sc->sc_scantask);
	usb_rem_task(sc->sc_udev, &sc->sc_task);
	usb_rem_task(sc->sc_udev, &sc->sc_task_tx);
	callout_stop(&sc->sc_led_ch);
	callout_stop(&sc->sc_watchdog_ch);

	/* free xfers */
	upgt_free_tx(sc);
	upgt_free_rx(sc);
	upgt_free_cmd(sc);

	bpfdetach(ifp);
	ieee80211_ifdetach(ic);
	if_free(ifp);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return 0;
}

static void
upgt_free_rx(struct upgt_softc *sc)
{
	struct upgt_data *data_rx = &sc->rx_data;

	if (data_rx->xfer != NULL) {
		usbd_free_xfer(data_rx->xfer);
		data_rx->xfer = NULL;
	}

	data_rx->ni = NULL;
}

static void
upgt_free_tx(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < upgt_txbuf; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		if (data_tx->xfer != NULL) {
			usbd_free_xfer(data_tx->xfer);
			data_tx->xfer = NULL;
		}

		data_tx->ni = NULL;
	}
}

static void
upgt_free_cmd(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;

	if (data_cmd->xfer != NULL) {
		usbd_free_xfer(data_cmd->xfer);
		data_cmd->xfer = NULL;
	}
}

static device_method_t upgt_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe, upgt_match),
        DEVMETHOD(device_attach, upgt_attach),
        DEVMETHOD(device_detach, upgt_detach),
	
	{ 0, 0 }
};

static driver_t upgt_driver = {
        "upgt",
        upgt_methods,
        sizeof(struct upgt_softc)
};

static devclass_t upgt_devclass;

DRIVER_MODULE(if_upgt, uhub, upgt_driver, upgt_devclass, usbd_driver_load, 0);
MODULE_VERSION(if_upgt, 1);
MODULE_DEPEND(if_upgt, usb, 1, 1, 1);
MODULE_DEPEND(if_upgt, wlan, 1, 1, 1);
MODULE_DEPEND(if_upgt, upgtfw_fw, 1, 1, 1);
