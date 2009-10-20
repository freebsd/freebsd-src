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
#include "usbdevs.h"

#include <dev/usb/wlan/if_upgtvar.h>

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
static int	upgt_device_reset(struct upgt_softc *);
static void	upgt_bulk_tx(struct upgt_softc *, struct upgt_data *);
static int	upgt_fw_verify(struct upgt_softc *);
static int	upgt_mem_init(struct upgt_softc *);
static int	upgt_fw_load(struct upgt_softc *);
static int	upgt_fw_copy(const uint8_t *, char *, int);
static uint32_t	upgt_crc32_le(const void *, size_t);
static struct mbuf *
		upgt_rxeof(struct usb_xfer *, struct upgt_data *, int *);
static struct mbuf *
		upgt_rx(struct upgt_softc *, uint8_t *, int, int *);
static void	upgt_txeof(struct usb_xfer *, struct upgt_data *);
static int	upgt_eeprom_read(struct upgt_softc *);
static int	upgt_eeprom_parse(struct upgt_softc *);
static void	upgt_eeprom_parse_hwrx(struct upgt_softc *, uint8_t *);
static void	upgt_eeprom_parse_freq3(struct upgt_softc *, uint8_t *, int);
static void	upgt_eeprom_parse_freq4(struct upgt_softc *, uint8_t *, int);
static void	upgt_eeprom_parse_freq6(struct upgt_softc *, uint8_t *, int);
static uint32_t	upgt_chksum_le(const uint32_t *, size_t);
static void	upgt_tx_done(struct upgt_softc *, uint8_t *);
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
static void	upgt_stop(struct upgt_softc *);
static void	upgt_setup_rates(struct ieee80211vap *, struct ieee80211com *);
static int	upgt_set_macfilter(struct upgt_softc *, uint8_t);
static int	upgt_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	upgt_set_chan(struct upgt_softc *, struct ieee80211_channel *);
static void	upgt_set_led(struct upgt_softc *, int);
static void	upgt_set_led_blink(void *);
static void	upgt_get_stats(struct upgt_softc *);
static void	upgt_mem_free(struct upgt_softc *, uint32_t);
static uint32_t	upgt_mem_alloc(struct upgt_softc *);
static void	upgt_free_tx(struct upgt_softc *);
static void	upgt_free_rx(struct upgt_softc *);
static void	upgt_watchdog(void *);
static void	upgt_abort_xfers(struct upgt_softc *);
static void	upgt_abort_xfers_locked(struct upgt_softc *);
static void	upgt_sysctl_node(struct upgt_softc *);
static struct upgt_data *
		upgt_getbuf(struct upgt_softc *);
static struct upgt_data *
		upgt_gettxbuf(struct upgt_softc *);
static int	upgt_tx_start(struct upgt_softc *, struct mbuf *,
		    struct ieee80211_node *, struct upgt_data *);

static const char *upgt_fwname = "upgt-gw3887";

static const struct usb_device_id upgt_devs_2[] = {
#define	UPGT_DEV(v,p) { USB_VP(USB_VENDOR_##v, USB_PRODUCT_##v##_##p) }
	/* version 2 devices */
	UPGT_DEV(ACCTON,	PRISM_GT),
	UPGT_DEV(BELKIN,	F5D7050),
	UPGT_DEV(CISCOLINKSYS,	WUSB54AG),
	UPGT_DEV(CONCEPTRONIC,	PRISM_GT),
	UPGT_DEV(DELL,		PRISM_GT_1),
	UPGT_DEV(DELL,		PRISM_GT_2),
	UPGT_DEV(FSC,		E5400),
	UPGT_DEV(GLOBESPAN,	PRISM_GT_1),
	UPGT_DEV(GLOBESPAN,	PRISM_GT_2),
	UPGT_DEV(INTERSIL,	PRISM_GT),
	UPGT_DEV(SMC,		2862WG),
	UPGT_DEV(WISTRONNEWEB,	UR045G),
	UPGT_DEV(XYRATEX,	PRISM_GT_1),
	UPGT_DEV(XYRATEX,	PRISM_GT_2),
	UPGT_DEV(ZCOM,		XG703A),
	UPGT_DEV(ZCOM,		XM142)
};

static usb_callback_t upgt_bulk_rx_callback;
static usb_callback_t upgt_bulk_tx_callback;

static const struct usb_config upgt_config[UPGT_N_XFERS] = {
	[UPGT_BULK_TX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MCLBYTES,
		.flags = {
			.ext_buffer = 1,
			.force_short_xfer = 1,
			.pipe_bof = 1
		},
		.callback = upgt_bulk_tx_callback,
		.timeout = UPGT_USB_TIMEOUT,	/* ms */
	},
	[UPGT_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = MCLBYTES,
		.flags = {
			.ext_buffer = 1,
			.pipe_bof = 1,
			.short_xfer_ok = 1
		},
		.callback = upgt_bulk_rx_callback,
	},
};

static int
upgt_match(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != UPGT_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != UPGT_IFACE_INDEX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(upgt_devs_2, sizeof(upgt_devs_2), uaa));
}

static int
upgt_attach(device_t dev)
{
	int error;
	struct ieee80211com *ic;
	struct ifnet *ifp;
	struct upgt_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	uint8_t bands, iface_index = UPGT_IFACE_INDEX;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
#ifdef UPGT_DEBUG
	sc->sc_debug = upgt_debug;
#endif
	device_set_usb_desc(dev);

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init(&sc->sc_led_ch, 0);
	callout_init(&sc->sc_watchdog_ch, 0);

	/* Allocate TX and RX xfers.  */
	error = upgt_alloc_tx(sc);
	if (error)
		goto fail1;
	error = upgt_alloc_rx(sc);
	if (error)
		goto fail2;

	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    upgt_config, UPGT_N_XFERS, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "could not allocate USB transfers, "
		    "err=%s\n", usbd_errstr(error));
		goto fail3;
	}

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		goto fail4;
	}

	/* Initialize the device.  */
	error = upgt_device_reset(sc);
	if (error)
		goto fail5;
	/* Verify the firmware.  */
	error = upgt_fw_verify(sc);
	if (error)
		goto fail5;
	/* Calculate device memory space.  */
	if (sc->sc_memaddr_frame_start == 0 || sc->sc_memaddr_frame_end == 0) {
		device_printf(dev,
		    "could not find memory space addresses on FW!\n");
		error = EIO;
		goto fail5;
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
	error = upgt_fw_load(sc);
	if (error)
		goto fail5;

	/* Read the whole EEPROM content and parse it.  */
	error = upgt_eeprom_read(sc);
	if (error)
		goto fail5;
	error = upgt_eeprom_parse(sc);
	if (error)
		goto fail5;

	/* all works related with the device have done here. */
	upgt_abort_xfers(sc);

	/* Setup the 802.11 device.  */
	ifp->if_softc = sc;
	if_initname(ifp, "upgt", device_get_unit(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
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

	ieee80211_ifattach(ic, sc->sc_myaddr);
	ic->ic_raw_xmit = upgt_raw_xmit;
	ic->ic_scan_start = upgt_scan_start;
	ic->ic_scan_end = upgt_scan_end;
	ic->ic_set_channel = upgt_set_channel;

	ic->ic_vap_create = upgt_vap_create;
	ic->ic_vap_delete = upgt_vap_delete;
	ic->ic_update_mcast = upgt_update_mcast;

	ieee80211_radiotap_attach(ic,
	    &sc->sc_txtap.wt_ihdr, sizeof(sc->sc_txtap),
		UPGT_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
		UPGT_RX_RADIOTAP_PRESENT);

	upgt_sysctl_node(sc);

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

fail5:	if_free(ifp);
fail4:	usbd_transfer_unsetup(sc->sc_xfer, UPGT_N_XFERS);
fail3:	upgt_free_rx(sc);
fail2:	upgt_free_tx(sc);
fail1:	mtx_destroy(&sc->sc_mtx);

	return (error);
}

static void
upgt_txeof(struct usb_xfer *xfer, struct upgt_data *data)
{
	struct upgt_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	UPGT_ASSERT_LOCKED(sc);

	/*
	 * Do any tx complete callback.  Note this must be done before releasing
	 * the node reference.
	 */
	if (data->m) {
		m = data->m;
		if (m->m_flags & M_TXCB) {
			/* XXX status? */
			ieee80211_process_callback(data->ni, m, 0);
		}
		m_freem(m);
		data->m = NULL;
	}
	if (data->ni) {
		ieee80211_free_node(data->ni);
		data->ni = NULL;
	}
	ifp->if_opackets++;
}

static void
upgt_get_stats(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_stats *stats;

	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		device_printf(sc->sc_dev, "%s: out of buffer.\n", __func__);
		return;
	}

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

	data_cmd->buflen = sizeof(*mem) + sizeof(*stats);

	mem->chksum = upgt_chksum_le((uint32_t *)stats,
	    data_cmd->buflen - sizeof(*mem));

	upgt_bulk_tx(sc, data_cmd);
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
				upgt_stop(sc);
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
upgt_stop_locked(struct upgt_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	UPGT_ASSERT_LOCKED(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		upgt_set_macfilter(sc, IEEE80211_S_INIT);
	upgt_abort_xfers_locked(sc);
}

static void
upgt_stop(struct upgt_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	UPGT_LOCK(sc);
	upgt_stop_locked(sc);
	UPGT_UNLOCK(sc);

	/* device down */
	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->sc_flags &= ~UPGT_FLAG_INITDONE;
}

static void
upgt_set_led(struct upgt_softc *sc, int action)
{
	struct upgt_data *data_cmd;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_led *led;

	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		device_printf(sc->sc_dev, "%s: out of buffers.\n", __func__);
		return;
	}

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
		if (sc->sc_state != IEEE80211_S_RUN) {
			STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data_cmd, next);
			return;
		}
		if (sc->sc_led_blink) {
			/* previous blink was not finished */
			STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data_cmd, next);
			return;
		}
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = htole16(UPGT_LED_ACTION_OFF);
		led->action_tmp = htole16(UPGT_LED_ACTION_ON);
		led->action_tmp_dur = htole16(UPGT_LED_ACTION_TMP_DUR);
		/* lock blink */
		sc->sc_led_blink = 1;
		callout_reset(&sc->sc_led_ch, hz, upgt_set_led_blink, sc);
		break;
	default:
		STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data_cmd, next);
		return;
	}

	data_cmd->buflen = sizeof(*mem) + sizeof(*led);

	mem->chksum = upgt_chksum_le((uint32_t *)led,
	    data_cmd->buflen - sizeof(*mem));

	upgt_bulk_tx(sc, data_cmd);
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

	UPGT_ASSERT_LOCKED(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		upgt_stop_locked(sc);

	usbd_transfer_start(sc->sc_xfer[UPGT_BULK_RX]);

	(void)upgt_set_macfilter(sc, IEEE80211_S_SCAN);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->sc_flags |= UPGT_FLAG_INITDONE;

	callout_reset(&sc->sc_watchdog_ch, hz, upgt_watchdog, sc);
}

static int
upgt_set_macfilter(struct upgt_softc *sc, uint8_t state)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni = vap->iv_bss;
	struct upgt_data *data_cmd;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_filter *filter;
	uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	UPGT_ASSERT_LOCKED(sc);

	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		device_printf(sc->sc_dev, "out of TX buffers.\n");
		return (ENOBUFS);
	}

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
		IEEE80211_ADDR_COPY(filter->dst, sc->sc_myaddr);
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
			IEEE80211_ADDR_COPY(filter->dst, sc->sc_myaddr);
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
			IEEE80211_ADDR_COPY(filter->dst, sc->sc_myaddr);
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

	data_cmd->buflen = sizeof(*mem) + sizeof(*filter);

	mem->chksum = upgt_chksum_le((uint32_t *)filter,
	    data_cmd->buflen - sizeof(*mem));

	upgt_bulk_tx(sc, data_cmd);

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
	ifp->if_flags |= IFF_ALLMULTI;
}

static void
upgt_start(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct upgt_data *data_tx;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	UPGT_LOCK(sc);
	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		data_tx = upgt_gettxbuf(sc);
		if (data_tx == NULL) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			break;
		}

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;

		if (upgt_tx_start(sc, m, ni, data_tx) != 0) {
			STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, data_tx, next);
			UPGT_STAT_INC(sc, st_tx_inactive);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}
		sc->sc_tx_timer = 5;
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

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}

	UPGT_LOCK(sc);
	data_tx = upgt_gettxbuf(sc);
	if (data_tx == NULL) {
		ieee80211_free_node(ni);
		m_freem(m);
		UPGT_UNLOCK(sc);
		return (ENOBUFS);
	}

	if (upgt_tx_start(sc, m, ni, data_tx) != 0) {
		STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, data_tx, next);
		UPGT_STAT_INC(sc, st_tx_inactive);
		ieee80211_free_node(ni);
		ifp->if_oerrors++;
		UPGT_UNLOCK(sc);
		return (EIO);
	}
	UPGT_UNLOCK(sc);

	sc->sc_tx_timer = 5;
	return (0);
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

	UPGT_LOCK(sc);
	upgt_set_chan(sc, ic->ic_curchan);
	UPGT_UNLOCK(sc);
}

static void
upgt_set_chan(struct upgt_softc *sc, struct ieee80211_channel *c)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct upgt_data *data_cmd;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_channel *chan;
	int channel;

	UPGT_ASSERT_LOCKED(sc);

	channel = ieee80211_chan2ieee(ic, c);
	if (channel == 0 || channel == IEEE80211_CHAN_ANY) {
		/* XXX should NEVER happen */
		device_printf(sc->sc_dev,
		    "%s: invalid channel %x\n", __func__, channel);
		return;
	}
	
	DPRINTF(sc, UPGT_DEBUG_STATE, "%s: channel %d\n", __func__, channel);

	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		device_printf(sc->sc_dev, "%s: out of buffers.\n", __func__);
		return;
	}
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

	data_cmd->buflen = sizeof(*mem) + sizeof(*chan);

	mem->chksum = upgt_chksum_le((uint32_t *)chan,
	    data_cmd->buflen - sizeof(*mem));

	upgt_bulk_tx(sc, data_cmd);
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

	/* do it in a process context */
	sc->sc_state = nstate;

	IEEE80211_UNLOCK(ic);
	UPGT_LOCK(sc);
	callout_stop(&sc->sc_led_ch);
	callout_stop(&sc->sc_watchdog_ch);

	switch (nstate) {
	case IEEE80211_S_INIT:
		/* do not accept any frames if the device is down */
		(void)upgt_set_macfilter(sc, sc->sc_state);
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
		upgt_set_macfilter(sc, sc->sc_state);
		upgt_set_led(sc, UPGT_LED_ON);
		break;
	default:
		break;
	}
	UPGT_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return (uvp->newstate(vap, nstate, arg));
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

	upgt_set_multi(sc);
}

static int
upgt_eeprom_parse(struct upgt_softc *sc)
{
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

			IEEE80211_ADDR_COPY(sc->sc_myaddr, eeprom_option->data);
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
	struct upgt_data *data_cmd;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_eeprom	*eeprom;
	int block, error, offset;

	UPGT_LOCK(sc);
	usb_pause_mtx(&sc->sc_mtx, 100);

	offset = 0;
	block = UPGT_EEPROM_BLOCK_SIZE;
	while (offset < UPGT_EEPROM_SIZE) {
		DPRINTF(sc, UPGT_DEBUG_FW,
		    "request EEPROM block (offset=%d, len=%d)\n", offset, block);

		data_cmd = upgt_getbuf(sc);
		if (data_cmd == NULL) {
			UPGT_UNLOCK(sc);
			return (ENOBUFS);
		}

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

		data_cmd->buflen = sizeof(*mem) + sizeof(*eeprom) + block;

		mem->chksum = upgt_chksum_le((uint32_t *)eeprom,
		    data_cmd->buflen - sizeof(*mem));
		upgt_bulk_tx(sc, data_cmd);

		error = mtx_sleep(sc, &sc->sc_mtx, 0, "eeprom_request", hz);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "timeout while waiting for EEPROM data!\n");
			UPGT_UNLOCK(sc);
			return (EIO);
		}

		offset += block;
		if (UPGT_EEPROM_SIZE - offset < block)
			block = UPGT_EEPROM_SIZE - offset;
	}

	UPGT_UNLOCK(sc);
	return (0);
}

/*
 * When a rx data came in the function returns a mbuf and a rssi values.
 */
static struct mbuf *
upgt_rxeof(struct usb_xfer *xfer, struct upgt_data *data, int *rssi)
{
	struct mbuf *m = NULL;
	struct upgt_softc *sc = usbd_xfer_softc(xfer);
	struct upgt_lmac_header *header;
	struct upgt_lmac_eeprom *eeprom;
	uint8_t h1_type;
	uint16_t h2_type;
	int actlen, sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	UPGT_ASSERT_LOCKED(sc);

	if (actlen < 1)
		return (NULL);

	/* Check only at the very beginning.  */
	if (!(sc->sc_flags & UPGT_FLAG_FWLOADED) &&
	    (memcmp(data->buf, "OK", 2) == 0)) {
		sc->sc_flags |= UPGT_FLAG_FWLOADED;
		wakeup_one(sc);
		return (NULL);
	}

	if (actlen < UPGT_RX_MINSZ)
		return (NULL);

	/*
	 * Check what type of frame came in.
	 */
	header = (struct upgt_lmac_header *)(data->buf + 4);

	h1_type = header->header1.type;
	h2_type = le16toh(header->header2.type);

	if (h1_type == UPGT_H1_TYPE_CTRL && h2_type == UPGT_H2_TYPE_EEPROM) {
		eeprom = (struct upgt_lmac_eeprom *)(data->buf + 4);
		uint16_t eeprom_offset = le16toh(eeprom->offset);
		uint16_t eeprom_len = le16toh(eeprom->len);

		DPRINTF(sc, UPGT_DEBUG_FW,
		    "received EEPROM block (offset=%d, len=%d)\n",
		    eeprom_offset, eeprom_len);

		bcopy(data->buf + sizeof(struct upgt_lmac_eeprom) + 4,
			sc->sc_eeprom + eeprom_offset, eeprom_len);

		/* EEPROM data has arrived in time, wakeup.  */
		wakeup(sc);
	} else if (h1_type == UPGT_H1_TYPE_CTRL &&
	    h2_type == UPGT_H2_TYPE_TX_DONE) {
		DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: received 802.11 TX done\n",
		    __func__);
		upgt_tx_done(sc, data->buf + 4);
	} else if (h1_type == UPGT_H1_TYPE_RX_DATA ||
	    h1_type == UPGT_H1_TYPE_RX_DATA_MGMT) {
		DPRINTF(sc, UPGT_DEBUG_RECV, "%s: received 802.11 RX data\n",
		    __func__);
		m = upgt_rx(sc, data->buf + 4, le16toh(header->header1.len),
		    rssi);
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
	return (m);
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

static struct mbuf *
upgt_rx(struct upgt_softc *sc, uint8_t *data, int pkglen, int *rssi)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct upgt_lmac_rx_desc *rxdesc;
	struct mbuf *m;

	/*
	 * don't pass packets to the ieee80211 framework if the driver isn't
	 * RUNNING.
	 */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return (NULL);

	/* access RX packet descriptor */
	rxdesc = (struct upgt_lmac_rx_desc *)data;

	/* create mbuf which is suitable for strict alignment archs */
	KASSERT((pkglen + ETHER_ALIGN) < MCLBYTES,
	    ("A current mbuf storage is small (%d)", pkglen + ETHER_ALIGN));
	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		device_printf(sc->sc_dev, "could not create RX mbuf!\n");
		return (NULL);
	}
	m_adj(m, ETHER_ALIGN);
	bcopy(rxdesc->data, mtod(m, char *), pkglen);
	/* trim FCS */
	m->m_len = m->m_pkthdr.len = pkglen - IEEE80211_CRC_LEN;
	m->m_pkthdr.rcvif = ifp;

	if (ieee80211_radiotap_active(ic)) {
		struct upgt_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = upgt_rx_rate(sc, rxdesc->rate);
		tap->wr_antsignal = rxdesc->rssi;
	}
	ifp->if_ipackets++;

	DPRINTF(sc, UPGT_DEBUG_RX_PROC, "%s: RX done\n", __func__);
	*rssi = rxdesc->rssi;
	return (m);
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
	int i, freed = 0;

	UPGT_ASSERT_LOCKED(sc);

	desc = (struct upgt_lmac_tx_done_desc *)data;

	for (i = 0; i < UPGT_TX_MAXCOUNT; i++) {
		struct upgt_data *data_tx = &sc->sc_tx_data[i];

		if (data_tx->addr == le32toh(desc->header2.reqid)) {
			upgt_mem_free(sc, data_tx->addr);
			data_tx->ni = NULL;
			data_tx->addr = 0;
			data_tx->m = NULL;
			data_tx->use = 0;

			DPRINTF(sc, UPGT_DEBUG_TX_PROC,
			    "TX done: memaddr=0x%08x, status=0x%04x, rssi=%d, ",
			    le32toh(desc->header2.reqid),
			    le16toh(desc->status), le16toh(desc->rssi));
			DPRINTF(sc, UPGT_DEBUG_TX_PROC, "seq=%d\n",
			    le16toh(desc->seq));

			freed++;
		}
	}

	if (freed != 0) {
		sc->sc_tx_timer = 0;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		UPGT_UNLOCK(sc);
		upgt_start(ifp);
		UPGT_LOCK(sc);
	}
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
	struct upgt_data *data_cmd;
	struct upgt_fw_x2_header *x2;
	char start_fwload_cmd[] = { 0x3c, 0x0d };
	int error = 0, offset, bsize, n;
	uint32_t crc32;

	fw = firmware_get(upgt_fwname);
	if (fw == NULL) {
		device_printf(sc->sc_dev, "could not read microcode %s!\n",
		    upgt_fwname);
		return (EIO);
	}

	UPGT_LOCK(sc);

	/* send firmware start load command */
	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		error = ENOBUFS;
		goto fail;
	}
	data_cmd->buflen = sizeof(start_fwload_cmd);
	bcopy(start_fwload_cmd, data_cmd->buf, data_cmd->buflen);
	upgt_bulk_tx(sc, data_cmd);

	/* send X2 header */
	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		error = ENOBUFS;
		goto fail;
	}
	data_cmd->buflen = sizeof(struct upgt_fw_x2_header);
	x2 = (struct upgt_fw_x2_header *)data_cmd->buf;
	bcopy(UPGT_X2_SIGNATURE, x2->signature, UPGT_X2_SIGNATURE_SIZE);
	x2->startaddr = htole32(UPGT_MEMADDR_FIRMWARE_START);
	x2->len = htole32(fw->datasize);
	x2->crc = upgt_crc32_le((uint8_t *)data_cmd->buf +
	    UPGT_X2_SIGNATURE_SIZE,
	    sizeof(struct upgt_fw_x2_header) - UPGT_X2_SIGNATURE_SIZE -
	    sizeof(uint32_t));
	upgt_bulk_tx(sc, data_cmd);

	/* download firmware */
	for (offset = 0; offset < fw->datasize; offset += bsize) {
		if (fw->datasize - offset > UPGT_FW_BLOCK_SIZE)
			bsize = UPGT_FW_BLOCK_SIZE;
		else
			bsize = fw->datasize - offset;

		data_cmd = upgt_getbuf(sc);
		if (data_cmd == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		n = upgt_fw_copy((const uint8_t *)fw->data + offset,
		    data_cmd->buf, bsize);
		data_cmd->buflen = bsize;
		upgt_bulk_tx(sc, data_cmd);

		DPRINTF(sc, UPGT_DEBUG_FW, "FW offset=%d, read=%d, sent=%d\n",
		    offset, n, bsize);
		bsize = n;
	}
	DPRINTF(sc, UPGT_DEBUG_FW, "%s: firmware downloaded\n", __func__);

	/* load firmware */
	data_cmd = upgt_getbuf(sc);
	if (data_cmd == NULL) {
		error = ENOBUFS;
		goto fail;
	}
	crc32 = upgt_crc32_le(fw->data, fw->datasize);
	*((uint32_t *)(data_cmd->buf)    ) = crc32;
	*((uint8_t  *)(data_cmd->buf) + 4) = 'g';
	*((uint8_t  *)(data_cmd->buf) + 5) = '\r';
	data_cmd->buflen = 6;
	upgt_bulk_tx(sc, data_cmd);

	/* waiting 'OK' response.  */
	usbd_transfer_start(sc->sc_xfer[UPGT_BULK_RX]);
	error = mtx_sleep(sc, &sc->sc_mtx, 0, "upgtfw", 2 * hz);
	if (error != 0) {
		device_printf(sc->sc_dev, "firmware load failed!\n");
		error = EIO;
	}

	DPRINTF(sc, UPGT_DEBUG_FW, "%s: firmware loaded\n", __func__);
fail:
	UPGT_UNLOCK(sc);
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

static void
upgt_bulk_tx(struct upgt_softc *sc, struct upgt_data *data)
{

	UPGT_ASSERT_LOCKED(sc);

	STAILQ_INSERT_TAIL(&sc->sc_tx_pending, data, next);
	UPGT_STAT_INC(sc, st_tx_pending);
	usbd_transfer_start(sc->sc_xfer[UPGT_BULK_TX]);
}

static int
upgt_device_reset(struct upgt_softc *sc)
{
	struct upgt_data *data;
	char init_cmd[] = { 0x7e, 0x7e, 0x7e, 0x7e };

	UPGT_LOCK(sc);

	data = upgt_getbuf(sc);
	if (data == NULL) {
		UPGT_UNLOCK(sc);
		return (ENOBUFS);
	}
	bcopy(init_cmd, data->buf, sizeof(init_cmd));
	data->buflen = sizeof(init_cmd);
	upgt_bulk_tx(sc, data);
	usb_pause_mtx(&sc->sc_mtx, 100);

	UPGT_UNLOCK(sc);
	DPRINTF(sc, UPGT_DEBUG_FW, "%s: device initialized\n", __func__);
	return (0);
}

static int
upgt_alloc_tx(struct upgt_softc *sc)
{
	int i;

	STAILQ_INIT(&sc->sc_tx_active);
	STAILQ_INIT(&sc->sc_tx_inactive);
	STAILQ_INIT(&sc->sc_tx_pending);

	for (i = 0; i < UPGT_TX_MAXCOUNT; i++) {
		struct upgt_data *data = &sc->sc_tx_data[i];

		data->buf = malloc(MCLBYTES, M_USBDEV, M_NOWAIT | M_ZERO);
		if (data->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate TX buffer!\n");
			return (ENOMEM);
		}
		STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data, next);
		UPGT_STAT_INC(sc, st_tx_inactive);
	}

	return (0);
}

static int
upgt_alloc_rx(struct upgt_softc *sc)
{
	int i;

	STAILQ_INIT(&sc->sc_rx_active);
	STAILQ_INIT(&sc->sc_rx_inactive);

	for (i = 0; i < UPGT_RX_MAXCOUNT; i++) {
		struct upgt_data *data = &sc->sc_rx_data[i];

		data->buf = malloc(MCLBYTES, M_USBDEV, M_NOWAIT | M_ZERO);
		if (data->buf == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate RX buffer!\n");
			return (ENOMEM);
		}
		STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
	}

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

	upgt_stop(sc);

	callout_drain(&sc->sc_led_ch);
	callout_drain(&sc->sc_watchdog_ch);

	usbd_transfer_unsetup(sc->sc_xfer, UPGT_N_XFERS);
	ieee80211_ifdetach(ic);
	upgt_free_rx(sc);
	upgt_free_tx(sc);

	if_free(ifp);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
upgt_free_rx(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < UPGT_RX_MAXCOUNT; i++) {
		struct upgt_data *data = &sc->sc_rx_data[i];

		free(data->buf, M_USBDEV);
		data->ni = NULL;
	}
}

static void
upgt_free_tx(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < UPGT_TX_MAXCOUNT; i++) {
		struct upgt_data *data = &sc->sc_tx_data[i];

		free(data->buf, M_USBDEV);
		data->ni = NULL;
	}
}

static void
upgt_abort_xfers_locked(struct upgt_softc *sc)
{
	int i;

	UPGT_ASSERT_LOCKED(sc);
	/* abort any pending transfers */
	for (i = 0; i < UPGT_N_XFERS; i++)
		usbd_transfer_stop(sc->sc_xfer[i]);
}

static void
upgt_abort_xfers(struct upgt_softc *sc)
{

	UPGT_LOCK(sc);
	upgt_abort_xfers_locked(sc);
	UPGT_UNLOCK(sc);
}

#define	UPGT_SYSCTL_STAT_ADD32(c, h, n, p, d)	\
	    SYSCTL_ADD_UINT(c, h, OID_AUTO, n, CTLFLAG_RD, p, 0, d)

static void
upgt_sysctl_node(struct upgt_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *child;
	struct sysctl_oid *tree;
	struct upgt_stat *stats;

	stats = &sc->sc_stat;
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev));

	tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "stats", CTLFLAG_RD,
	    NULL, "UPGT statistics");
	child = SYSCTL_CHILDREN(tree);
	UPGT_SYSCTL_STAT_ADD32(ctx, child, "tx_active",
	    &stats->st_tx_active, "Active numbers in TX queue");
	UPGT_SYSCTL_STAT_ADD32(ctx, child, "tx_inactive",
	    &stats->st_tx_inactive, "Inactive numbers in TX queue");
	UPGT_SYSCTL_STAT_ADD32(ctx, child, "tx_pending",
	    &stats->st_tx_pending, "Pending numbers in TX queue");
}

#undef UPGT_SYSCTL_STAT_ADD32

static struct upgt_data *
_upgt_getbuf(struct upgt_softc *sc)
{
	struct upgt_data *bf;

	bf = STAILQ_FIRST(&sc->sc_tx_inactive);
	if (bf != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_tx_inactive, next);
		UPGT_STAT_DEC(sc, st_tx_inactive);
	} else
		bf = NULL;
	if (bf == NULL)
		DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: %s\n", __func__,
		    "out of xmit buffers");
	return (bf);
}

static struct upgt_data *
upgt_getbuf(struct upgt_softc *sc)
{
	struct upgt_data *bf;

	UPGT_ASSERT_LOCKED(sc);

	bf = _upgt_getbuf(sc);
	if (bf == NULL) {
		struct ifnet *ifp = sc->sc_ifp;

		DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: stop queue\n", __func__);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	}

	return (bf);
}

static struct upgt_data *
upgt_gettxbuf(struct upgt_softc *sc)
{
	struct upgt_data *bf;

	UPGT_ASSERT_LOCKED(sc);

	bf = upgt_getbuf(sc);
	if (bf == NULL)
		return (NULL);

	bf->addr = upgt_mem_alloc(sc);
	if (bf->addr == 0) {
		struct ifnet *ifp = sc->sc_ifp;

		DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: no free prism memory!\n",
		    __func__);
		STAILQ_INSERT_HEAD(&sc->sc_tx_inactive, bf, next);
		UPGT_STAT_INC(sc, st_tx_inactive);
		if (!(ifp->if_drv_flags & IFF_DRV_OACTIVE))
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		return (NULL);
	}
	return (bf);
}

static int
upgt_tx_start(struct upgt_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
    struct upgt_data *data)
{
	struct ieee80211vap *vap = ni->ni_vap;
	int error = 0, len;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct ifnet *ifp = sc->sc_ifp;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_tx_desc *txdesc;

	UPGT_ASSERT_LOCKED(sc);

	upgt_set_led(sc, UPGT_LED_BLINK);

	/*
	 * Software crypto.
	 */
	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			device_printf(sc->sc_dev,
			    "ieee80211_crypto_encap returns NULL.\n");
			error = EIO;
			goto done;
		}

		/* in case packet header moved, reset pointer */
		wh = mtod(m, struct ieee80211_frame *);
	}

	/* Transmit the URB containing the TX data.  */
	bzero(data->buf, MCLBYTES);
	mem = (struct upgt_lmac_mem *)data->buf;
	mem->addr = htole32(data->addr);
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
		bcopy(sc->sc_cur_rateset, txdesc->rates, sizeof(txdesc->rates));
	}
	txdesc->header1.type = UPGT_H1_TYPE_TX_DATA;
	txdesc->header1.len = htole16(m->m_pkthdr.len);
	txdesc->header2.reqid = htole32(data->addr);
	txdesc->header2.type = htole16(UPGT_H2_TYPE_TX_ACK_YES);
	txdesc->header2.flags = htole16(UPGT_H2_FLAGS_TX_ACK_YES);
	txdesc->type = htole32(UPGT_TX_DESC_TYPE_DATA);
	txdesc->pad3[0] = UPGT_TX_DESC_PAD3_SIZE;

	if (ieee80211_radiotap_active_vap(vap)) {
		struct upgt_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = 0;	/* XXX where to get from? */

		ieee80211_radiotap_tx(vap, m);
	}

	/* copy frame below our TX descriptor header */
	m_copydata(m, 0, m->m_pkthdr.len,
	    data->buf + (sizeof(*mem) + sizeof(*txdesc)));
	/* calculate frame size */
	len = sizeof(*mem) + sizeof(*txdesc) + m->m_pkthdr.len;
	/* we need to align the frame to a 4 byte boundary */
	len = (len + 3) & ~3;
	/* calculate frame checksum */
	mem->chksum = upgt_chksum_le((uint32_t *)txdesc, len - sizeof(*mem));
	data->ni = ni;
	data->m = m;
	data->buflen = len;

	DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: TX start data sending (%d bytes)\n",
	    __func__, len);
	KASSERT(len <= MCLBYTES, ("mbuf is small for saving data"));

	upgt_bulk_tx(sc, data);
done:
	/*
	 * If we don't regulary read the device statistics, the RX queue
	 * will stall.  It's strange, but it works, so we keep reading
	 * the statistics here.  *shrug*
	 */
	if (!(ifp->if_opackets % UPGT_TX_STAT_INTERVAL))
		upgt_get_stats(sc);

	return (error);
}

static void
upgt_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct upgt_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m = NULL;
	struct upgt_data *data;
	int8_t nf;
	int rssi = -1;

	UPGT_ASSERT_LOCKED(sc);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_rx_active);
		if (data == NULL)
			goto setup;
		STAILQ_REMOVE_HEAD(&sc->sc_rx_active, next);
		m = upgt_rxeof(xfer, data, &rssi);
		STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
setup:
		data = STAILQ_FIRST(&sc->sc_rx_inactive);
		if (data == NULL)
			return;
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
		UPGT_UNLOCK(sc);
		if (m != NULL) {
			wh = mtod(m, struct ieee80211_frame *);
			ni = ieee80211_find_rxnode(ic,
			    (struct ieee80211_frame_min *)wh);
			nf = -95;	/* XXX */
			if (ni != NULL) {
				(void) ieee80211_input(ni, m, rssi, nf);
				/* node is no longer needed */
				ieee80211_free_node(ni);
			} else
				(void) ieee80211_input_all(ic, m, rssi, nf);
			m = NULL;
		}
		if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0 &&
		    !IFQ_IS_EMPTY(&ifp->if_snd))
			upgt_start(ifp);
		UPGT_LOCK(sc);
		break;
	default:
		/* needs it to the inactive queue due to a error.  */
		data = STAILQ_FIRST(&sc->sc_rx_active);
		if (data != NULL) {
			STAILQ_REMOVE_HEAD(&sc->sc_rx_active, next);
			STAILQ_INSERT_TAIL(&sc->sc_rx_inactive, data, next);
		}
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			ifp->if_ierrors++;
			goto setup;
		}
		break;
	}
}

static void
upgt_bulk_tx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct upgt_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct upgt_data *data;

	UPGT_ASSERT_LOCKED(sc);
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&sc->sc_tx_active);
		if (data == NULL)
			goto setup;
		STAILQ_REMOVE_HEAD(&sc->sc_tx_active, next);
		UPGT_STAT_DEC(sc, st_tx_active);
		upgt_txeof(xfer, data);
		STAILQ_INSERT_TAIL(&sc->sc_tx_inactive, data, next);
		UPGT_STAT_INC(sc, st_tx_inactive);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
setup:
		data = STAILQ_FIRST(&sc->sc_tx_pending);
		if (data == NULL) {
			DPRINTF(sc, UPGT_DEBUG_XMIT, "%s: empty pending queue\n",
			    __func__);
			return;
		}
		STAILQ_REMOVE_HEAD(&sc->sc_tx_pending, next);
		UPGT_STAT_DEC(sc, st_tx_pending);
		STAILQ_INSERT_TAIL(&sc->sc_tx_active, data, next);
		UPGT_STAT_INC(sc, st_tx_active);

		usbd_xfer_set_frame_data(xfer, 0, data->buf, data->buflen);
		usbd_transfer_submit(xfer);
		UPGT_UNLOCK(sc);
		upgt_start(ifp);
		UPGT_LOCK(sc);
		break;
	default:
		data = STAILQ_FIRST(&sc->sc_tx_active);
		if (data == NULL)
			goto setup;
		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
			ifp->if_oerrors++;
		}
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto setup;
		}
		break;
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

DRIVER_MODULE(if_upgt, uhub, upgt_driver, upgt_devclass, NULL, 0);
MODULE_VERSION(if_upgt, 1);
MODULE_DEPEND(if_upgt, usb, 1, 1, 1);
MODULE_DEPEND(if_upgt, wlan, 1, 1, 1);
MODULE_DEPEND(if_upgt, upgtfw_fw, 1, 1, 1);
