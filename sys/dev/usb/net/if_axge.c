/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2014 Kevin Lo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ASIX Electronics AX88178A/AX88179/AX88179A USB 2.0/3.0 gigabit ethernet
 * driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR 	axge_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/net/usb_ethernet.h>
#include <dev/usb/net/if_axgereg.h>

#include "miibus_if.h"

/*
 * Various supported device vendors/products.
 */

static const STRUCT_USB_HOST_ID axge_devs[] = {
#define	AXGE_DEV(v,p,i,...)	\
	{ USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, i), __VA_ARGS__ }
	AXGE_DEV(ASIX, AX88178A, AXGE_FLAG_178A),
	AXGE_DEV(ASIX, AX88179, AXGE_FLAG_179, USB_DEV_BCD_LTEQ(0x0100)),
	AXGE_DEV(ASIX, AX88179, AXGE_FLAG_179A, USB_DEV_BCD_GTEQ(0x0200)),
	AXGE_DEV(BELKIN, B2B128, AXGE_FLAG_179),
	AXGE_DEV(DLINK, DUB1312, AXGE_FLAG_179),
	AXGE_DEV(LENOVO, GIGALAN, AXGE_FLAG_179),
	AXGE_DEV(SITECOMEU, LN032, AXGE_FLAG_179),
#undef AXGE_DEV
};

static const struct {
	uint8_t	ctrl;
	uint8_t timer_l;
	uint8_t	timer_h;
	uint8_t	size;
	uint8_t	ifg;
} __packed axge_bulk_size[] = {
	{ 7, 0x4f, 0x00, 0x12, 0xff },
	{ 7, 0x20, 0x03, 0x16, 0xff },
	{ 7, 0xae, 0x07, 0x18, 0xff },
	{ 7, 0xcc, 0x4c, 0x18, 0x08 }
};

/* prototypes */

static device_probe_t axge_probe;
static device_attach_t axge_attach;
static device_detach_t axge_detach;

static usb_callback_t axge_bulk_read_callback;
static usb_callback_t axge_bulk_write_callback;

static miibus_readreg_t axge_miibus_readreg;
static miibus_writereg_t axge_miibus_writereg;
static miibus_statchg_t axge_miibus_statchg;

static uether_fn_t axge_attach_post;
static uether_fn_t axge_init;
static uether_fn_t axge_stop;
static uether_fn_t axge_start;
static uether_fn_t axge_tick;
static uether_fn_t axge_rxfilter;

static int	axge_read_mem(struct axge_softc *, uint8_t, uint16_t,
		    uint16_t, void *, int);
static void	axge_write_mem(struct axge_softc *, uint8_t, uint16_t,
		    uint16_t, void *, int);
static uint8_t	axge_read_cmd_1(struct axge_softc *, uint8_t, uint16_t);
static uint16_t	axge_read_cmd_2(struct axge_softc *, uint8_t, uint16_t,
		    uint16_t);
static void	axge_write_cmd_1(struct axge_softc *, uint8_t, uint16_t,
		    uint8_t);
static void	axge_write_cmd_2(struct axge_softc *, uint8_t, uint16_t,
		    uint16_t, uint16_t);
static void	axge_chip_init(struct axge_softc *);
static void	axge_reset(struct axge_softc *);

static int	axge_attach_post_sub(struct usb_ether *);
static int	axge_ifmedia_upd(if_t);
static void	axge_ifmedia_sts(if_t, struct ifmediareq *);
static int	axge_ioctl(if_t, u_long, caddr_t);
static void	axge_rx_frame(struct usb_ether *, struct usb_page_cache *, int);
static void	axge_rxeof(struct usb_ether *, struct usb_page_cache *,
		    unsigned, unsigned, uint32_t);
static void	axge_csum_cfg(struct usb_ether *);

#define	AXGE_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)

#ifdef USB_DEBUG
static int axge_debug = 0;

static SYSCTL_NODE(_hw_usb, OID_AUTO, axge, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB axge");
SYSCTL_INT(_hw_usb_axge, OID_AUTO, debug, CTLFLAG_RWTUN, &axge_debug, 0,
    "Debug level");
#endif

static const struct usb_config axge_config[AXGE_N_TRANSFER] = {
	[AXGE_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.frames = AXGE_N_FRAMES,
		.bufsize = AXGE_N_FRAMES * MCLBYTES,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = axge_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
	},
	[AXGE_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 65536,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = axge_bulk_read_callback,
		.timeout = 0,		/* no timeout */
	},
};

static device_method_t axge_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		axge_probe),
	DEVMETHOD(device_attach,	axge_attach),
	DEVMETHOD(device_detach,	axge_detach),

	/* MII interface. */
	DEVMETHOD(miibus_readreg,	axge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	axge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	axge_miibus_statchg),

	DEVMETHOD_END
};

static driver_t axge_driver = {
	.name = "axge",
	.methods = axge_methods,
	.size = sizeof(struct axge_softc),
};

DRIVER_MODULE(axge, uhub, axge_driver, NULL, NULL);
DRIVER_MODULE(miibus, axge, miibus_driver, NULL, NULL);
MODULE_DEPEND(axge, uether, 1, 1, 1);
MODULE_DEPEND(axge, usb, 1, 1, 1);
MODULE_DEPEND(axge, ether, 1, 1, 1);
MODULE_DEPEND(axge, miibus, 1, 1, 1);
MODULE_VERSION(axge, 1);
USB_PNP_HOST_INFO(axge_devs);

static const struct usb_ether_methods axge_ue_methods = {
	.ue_attach_post = axge_attach_post,
	.ue_attach_post_sub = axge_attach_post_sub,
	.ue_start = axge_start,
	.ue_init = axge_init,
	.ue_stop = axge_stop,
	.ue_tick = axge_tick,
	.ue_setmulti = axge_rxfilter,
	.ue_setpromisc = axge_rxfilter,
	.ue_mii_upd = axge_ifmedia_upd,
	.ue_mii_sts = axge_ifmedia_sts,
};

static int
axge_read_mem(struct axge_softc *sc, uint8_t cmd, uint16_t index,
    uint16_t val, void *buf, int len)
{
	struct usb_device_request req;

	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = cmd;
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, len);

	return (uether_do_request(&sc->sc_ue, &req, buf, 1000));
}

static void
axge_write_mem(struct axge_softc *sc, uint8_t cmd, uint16_t index,
    uint16_t val, void *buf, int len)
{
	struct usb_device_request req;

	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = cmd;
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, len);

	if (uether_do_request(&sc->sc_ue, &req, buf, 1000)) {
		/* Error ignored. */
	}
}

static uint8_t
axge_read_cmd_1(struct axge_softc *sc, uint8_t cmd, uint16_t reg)
{
	uint8_t val;

	axge_read_mem(sc, cmd, 1, reg, &val, 1);
	return (val);
}

static uint16_t
axge_read_cmd_2(struct axge_softc *sc, uint8_t cmd, uint16_t index,
    uint16_t reg)
{
	uint8_t val[2];

	axge_read_mem(sc, cmd, index, reg, &val, 2);
	return (UGETW(val));
}

static void
axge_write_cmd_1(struct axge_softc *sc, uint8_t cmd, uint16_t reg, uint8_t val)
{
	axge_write_mem(sc, cmd, 1, reg, &val, 1);
}

static void
axge_write_cmd_2(struct axge_softc *sc, uint8_t cmd, uint16_t index,
    uint16_t reg, uint16_t val)
{
	uint8_t temp[2];

	USETW(temp, val);
	axge_write_mem(sc, cmd, index, reg, &temp, 2);
}

static int
axge_miibus_readreg(device_t dev, int phy, int reg)
{
	struct axge_softc *sc;
	uint16_t val;
	int locked;

	sc = device_get_softc(dev);
	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AXGE_LOCK(sc);

	val = axge_read_cmd_2(sc, AXGE_ACCESS_PHY, reg, phy);

	if (!locked)
		AXGE_UNLOCK(sc);

	return (val);
}

static int
axge_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct axge_softc *sc;
	int locked;

	sc = device_get_softc(dev);
	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AXGE_LOCK(sc);

	axge_write_cmd_2(sc, AXGE_ACCESS_PHY, reg, phy, val);

	if (!locked)
		AXGE_UNLOCK(sc);

	return (0);
}

static void
axge_miibus_statchg(device_t dev)
{
	struct axge_softc *sc;
	struct mii_data *mii;
	if_t ifp;
	uint8_t link_status, tmp[5];
	uint16_t val;
	int locked;

	sc = device_get_softc(dev);
	mii = GET_MII(sc);
	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AXGE_LOCK(sc);

	ifp = uether_getifp(&sc->sc_ue);
	if (mii == NULL || ifp == NULL ||
	    (if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
		goto done;

	sc->sc_flags &= ~AXGE_FLAG_LINK;
	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
		case IFM_1000_T:
			sc->sc_flags |= AXGE_FLAG_LINK;
			break;
		default:
			break;
		}
	}

	/* Lost link, do nothing. */
	if ((sc->sc_flags & AXGE_FLAG_LINK) == 0)
		goto done;

	link_status = axge_read_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_PLSR);

	val = 0;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		val |= MSR_FD;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
			val |= MSR_TFC;
		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
			val |= MSR_RFC;
	}
	val |=  MSR_RE;
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
		val |= MSR_GM | MSR_EN_125MHZ;
		if (link_status & PLSR_USB_SS)
			memcpy(tmp, &axge_bulk_size[0], 5);
		else if (link_status & PLSR_USB_HS)
			memcpy(tmp, &axge_bulk_size[1], 5);
		else
			memcpy(tmp, &axge_bulk_size[3], 5);
		break;
	case IFM_100_TX:
		val |= MSR_PS;
		if (link_status & (PLSR_USB_SS | PLSR_USB_HS))
			memcpy(tmp, &axge_bulk_size[2], 5);
		else
			memcpy(tmp, &axge_bulk_size[3], 5);
		break;
	case IFM_10_T:
		memcpy(tmp, &axge_bulk_size[3], 5);
		break;
	}
	/* Rx bulk configuration. */
	axge_write_mem(sc, AXGE_ACCESS_MAC, 5, AXGE_RX_BULKIN_QCTRL, tmp, 5);
	axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_MSR, val);
done:
	if (!locked)
		AXGE_UNLOCK(sc);
}

static void
axge_chip_init(struct axge_softc *sc)
{
	/* Power up ethernet PHY. */
	axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_EPPRCR, 0);
	axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_EPPRCR, EPPRCR_IPRL);
	uether_pause(&sc->sc_ue, hz / 4);
	axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_CLK_SELECT,
	    AXGE_CLK_SELECT_ACS | AXGE_CLK_SELECT_BCS);
	uether_pause(&sc->sc_ue, hz / 10);

	if ((sc->sc_flags & AXGE_FLAG_179A) != 0) {
		/*
		 * 179A chip has two firmware modes that each use different
		 * transfer layouts for Ethernet over USB. The newer fw mode has
		 * larger rx packet headers which seem to
		 * accomodate for ethernet frames up to 9K length and a VLAN
		 * field for hardware tagging, but is not backward compatible
		 * with 178A/179 bulk transfer code due to the change in size
		 * and field alignments. The other fw mode uses the same packet
		 * headers as the older 178A/179 chips, which this driver uses.
		 *
		 * As we do not currently have VLAN hw tagging or jumbo support
		 * in this driver anyway, we're ok forcing 179A into its compat
		 * mode by default.
		 */
		axge_write_cmd_1(sc, AXGE_FW_MODE, AXGE_FW_MODE_178A179, 0);
	}
}

static void
axge_reset(struct axge_softc *sc)
{
	struct usb_config_descriptor *cd;
	usb_error_t err;

	cd = usbd_get_config_descriptor(sc->sc_ue.ue_udev);

	err = usbd_req_set_config(sc->sc_ue.ue_udev, &sc->sc_mtx,
	    cd->bConfigurationValue);
	if (err)
		DPRINTF("reset failed (ignored)\n");

	/* Wait a little while for the chip to get its brains in order. */
	uether_pause(&sc->sc_ue, hz / 100);

	/* Reinitialize controller to achieve full reset. */
	axge_chip_init(sc);
}

static void
axge_attach_post(struct usb_ether *ue)
{
	struct axge_softc *sc;

	sc = uether_getsc(ue);

	/* Initialize controller and get station address. */
	axge_chip_init(sc);
	axge_read_mem(sc, AXGE_ACCESS_MAC, ETHER_ADDR_LEN, AXGE_NIDR,
	    ue->ue_eaddr, ETHER_ADDR_LEN);
}

static int
axge_attach_post_sub(struct usb_ether *ue)
{
	if_t ifp;
	int error;

	ifp = ue->ue_ifp;
	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(ifp, uether_start);
	if_setioctlfn(ifp, axge_ioctl);
	if_setinitfn(ifp, uether_init);
	if_setsendqlen(ifp, ifqmaxlen);
	if_setsendqready(ifp);

	if_setcapabilitiesbit(ifp, IFCAP_VLAN_MTU | IFCAP_TXCSUM | IFCAP_RXCSUM, 0);
	if_sethwassist(ifp, AXGE_CSUM_FEATURES);
	if_setcapenable(ifp, if_getcapabilities(ifp));

	bus_topo_lock();
	error = mii_attach(ue->ue_dev, &ue->ue_miibus, ifp,
	    uether_ifmedia_upd, ue->ue_methods->ue_mii_sts,
	    BMSR_DEFCAPMASK, AXGE_PHY_ADDR, MII_OFFSET_ANY, MIIF_DOPAUSE);
	bus_topo_unlock();

	return (error);
}

/*
 * Set media options.
 */
static int
axge_ifmedia_upd(if_t ifp)
{
	struct axge_softc *sc;
	struct mii_data *mii;
	struct mii_softc *miisc;
	int error;

	sc = if_getsoftc(ifp);
	mii = GET_MII(sc);
	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
	    PHY_RESET(miisc);
	error = mii_mediachg(mii);

	return (error);
}

/*
 * Report current media status.
 */
static void
axge_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct axge_softc *sc;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	mii = GET_MII(sc);
	AXGE_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	AXGE_UNLOCK(sc);
}

/*
 * Probe for a AX88179 chip.
 */
static int
axge_probe(device_t dev)
{
	struct usb_attach_arg *uaa;

	uaa = device_get_ivars(dev);
	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != AXGE_CONFIG_IDX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != AXGE_IFACE_IDX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(axge_devs, sizeof(axge_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
axge_attach(device_t dev)
{
	struct usb_attach_arg *uaa;
	struct axge_softc *sc;
	struct usb_ether *ue;
	uint8_t iface_index;
	int error;

	uaa = device_get_ivars(dev);
	sc = device_get_softc(dev);
	ue = &sc->sc_ue;

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	iface_index = AXGE_IFACE_IDX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, axge_config, AXGE_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB transfers failed\n");
		mtx_destroy(&sc->sc_mtx);
		return (ENXIO);
	}

	ue->ue_sc = sc;
	ue->ue_dev = dev;
	ue->ue_udev = uaa->device;
	ue->ue_mtx = &sc->sc_mtx;
	ue->ue_methods = &axge_ue_methods;

	error = uether_ifattach(ue);
	if (error) {
		device_printf(dev, "could not attach interface\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	axge_detach(dev);
	return (ENXIO);			/* failure */
}

static int
axge_detach(device_t dev)
{
	struct axge_softc *sc;
	struct usb_ether *ue;
	uint16_t val;

	sc = device_get_softc(dev);
	ue = &sc->sc_ue;
	if (device_is_attached(dev)) {
		/* wait for any post attach or other command to complete */
		usb_proc_drain(&ue->ue_tq);

		AXGE_LOCK(sc);
		/*
		 * XXX
		 * ether_ifdetach(9) should be called first.
		 */
		axge_stop(ue);
		/* Force bulk-in to return a zero-length USB packet. */
		val = axge_read_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_EPPRCR);
		val |= EPPRCR_BZ | EPPRCR_IPRL;
		axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_EPPRCR, val);
		/* Change clock. */
		axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_CLK_SELECT, 0);
		/* Disable MAC. */
		axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_RCR, 0);
		AXGE_UNLOCK(sc);
	}
	usbd_transfer_unsetup(sc->sc_xfer, AXGE_N_TRANSFER);
	uether_ifdetach(ue);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
axge_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct axge_softc *sc;
	struct usb_ether *ue;
	struct usb_page_cache *pc;
	int actlen;

	sc = usbd_xfer_softc(xfer);
	ue = &sc->sc_ue;
	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		axge_rx_frame(ue, pc, actlen);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		uether_rxflush(ue);
		break;

	default:
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
axge_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct axge_softc *sc;
	if_t ifp;
	struct usb_page_cache *pc;
	struct mbuf *m;
	struct axge_frame_txhdr txhdr;
	int nframes, pos;

	sc = usbd_xfer_softc(xfer);
	ifp = uether_getifp(&sc->sc_ue);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		if ((sc->sc_flags & AXGE_FLAG_LINK) == 0 ||
		    (if_getdrvflags(ifp) & IFF_DRV_OACTIVE) != 0) {
			/*
			 * Don't send anything if there is no link or
			 * controller is busy.
			 */
			return;
		}

		for (nframes = 0; nframes < AXGE_N_FRAMES &&
		    !if_sendq_empty(ifp); nframes++) {
			m = if_dequeue(ifp);
			if (m == NULL)
				break;
			usbd_xfer_set_frame_offset(xfer, nframes * MCLBYTES,
			    nframes);
			pc = usbd_xfer_get_frame(xfer, nframes);
			txhdr.mss = 0;
			txhdr.len = htole32(AXGE_TXBYTES(m->m_pkthdr.len));
			if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0 &&
			    (m->m_pkthdr.csum_flags & AXGE_CSUM_FEATURES) == 0)
				txhdr.len |= htole32(AXGE_CSUM_DISABLE);

			pos = 0;
			usbd_copy_in(pc, pos, &txhdr, sizeof(txhdr));
			pos += sizeof(txhdr);
			usbd_m_copy_in(pc, pos, m, 0, m->m_pkthdr.len);
			pos += m->m_pkthdr.len;

			/*
			 * if there's a BPF listener, bounce a copy
			 * of this frame to him:
			 */
			BPF_MTAP(ifp, m);

			m_freem(m);

			/* Set frame length. */
			usbd_xfer_set_frame_len(xfer, nframes, pos);
		}
		if (nframes != 0) {
			/*
			 * XXX
			 * Update TX packet counter here. This is not
			 * correct way but it seems that there is no way
			 * to know how many packets are sent at the end
			 * of transfer because controller combines
			 * multiple writes into single one if there is
			 * room in TX buffer of controller.
			 */
			if_inc_counter(ifp, IFCOUNTER_OPACKETS, nframes);
			usbd_xfer_set_frames(xfer, nframes);
			usbd_transfer_submit(xfer);
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
		}
		return;
		/* NOTREACHED */
	default:
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);

		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static void
axge_tick(struct usb_ether *ue)
{
	struct axge_softc *sc;
	struct mii_data *mii;

	sc = uether_getsc(ue);
	mii = GET_MII(sc);
	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	mii_tick(mii);
}

static u_int
axge_hash_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	uint8_t *hashtbl = arg;
	uint32_t h;

	h = ether_crc32_be(LLADDR(sdl), ETHER_ADDR_LEN) >> 26;
	hashtbl[h / 8] |= 1 << (h % 8);

	return (1);
}

static void
axge_rxfilter(struct usb_ether *ue)
{
	struct axge_softc *sc;
	if_t ifp;
	uint16_t rxmode;
	uint8_t hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	sc = uether_getsc(ue);
	ifp = uether_getifp(ue);
	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Configure RX settings.
	 * Don't set RCR_IPE(IP header alignment on 32bit boundary) to disable
	 * inserting extra padding bytes.  This wastes ethernet to USB host
	 * bandwidth as well as complicating RX handling logic.  Current USB
	 * framework requires copying RX frames to mbufs so there is no need
	 * to worry about alignment.
	 */
	rxmode = RCR_DROP_CRCERR | RCR_START;
	if (if_getflags(ifp) & IFF_BROADCAST)
		rxmode |= RCR_ACPT_BCAST;
	if (if_getflags(ifp) & (IFF_ALLMULTI | IFF_PROMISC)) {
		if (if_getflags(ifp) & IFF_PROMISC)
			rxmode |= RCR_PROMISC;
		rxmode |= RCR_ACPT_ALL_MCAST;
		axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_RCR, rxmode);
		return;
	}

	rxmode |= RCR_ACPT_MCAST;
	if_foreach_llmaddr(ifp, axge_hash_maddr, &hashtbl);

	axge_write_mem(sc, AXGE_ACCESS_MAC, 8, AXGE_MFA, (void *)&hashtbl, 8);
	axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_RCR, rxmode);
}

static void
axge_start(struct usb_ether *ue)
{
	struct axge_softc *sc;

	sc = uether_getsc(ue);
	/*
	 * Start the USB transfers, if not already started.
	 */
	usbd_transfer_start(sc->sc_xfer[AXGE_BULK_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[AXGE_BULK_DT_WR]);
}

static void
axge_init(struct usb_ether *ue)
{
	struct axge_softc *sc;
	if_t ifp;

	sc = uether_getsc(ue);
	ifp = uether_getifp(ue);
	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0)
		return;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	axge_stop(ue);

	axge_reset(sc);

	/* Set MAC address. */
	axge_write_mem(sc, AXGE_ACCESS_MAC, ETHER_ADDR_LEN, AXGE_NIDR,
	    if_getlladdr(ifp), ETHER_ADDR_LEN);

	axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_PWLLR, 0x34);
	axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_PWLHR, 0x52);

	/* Configure TX/RX checksum offloading. */
	axge_csum_cfg(ue);

	/*  Configure RX filters. */
	axge_rxfilter(ue);

	/*
	 * XXX
	 * Controller supports wakeup on link change detection,
	 * magic packet and wakeup frame recpetion.  But it seems
	 * there is no framework for USB ethernet suspend/wakeup.
	 * Disable all wakeup functions.
	 */
	axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_MMSR, 0);
	(void)axge_read_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_MMSR);

	/* Configure default medium type. */
	axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_MSR, MSR_GM | MSR_FD |
	    MSR_RFC | MSR_TFC | MSR_RE);

	usbd_xfer_set_stall(sc->sc_xfer[AXGE_BULK_DT_WR]);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);
	/* Switch to selected media. */
	axge_ifmedia_upd(ifp);
}

static void
axge_stop(struct usb_ether *ue)
{
	struct axge_softc *sc;
	if_t ifp;
	uint16_t val;

	sc = uether_getsc(ue);
	ifp = uether_getifp(ue);

	AXGE_LOCK_ASSERT(sc, MA_OWNED);

	val = axge_read_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_MSR);
	val &= ~MSR_RE;
	axge_write_cmd_2(sc, AXGE_ACCESS_MAC, 2, AXGE_MSR, val);

	if (ifp != NULL)
		if_setdrvflagbits(ifp, 0, (IFF_DRV_RUNNING | IFF_DRV_OACTIVE));
	sc->sc_flags &= ~AXGE_FLAG_LINK;

	/*
	 * Stop all the transfers, if not already stopped:
	 */
	usbd_transfer_stop(sc->sc_xfer[AXGE_BULK_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[AXGE_BULK_DT_RD]);
}

static int
axge_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct usb_ether *ue;
	struct axge_softc *sc;
	struct ifreq *ifr;
	int error, mask, reinit;

	ue = if_getsoftc(ifp);
	sc = uether_getsc(ue);
	ifr = (struct ifreq *)data;
	error = 0;
	reinit = 0;
	if (cmd == SIOCSIFCAP) {
		AXGE_LOCK(sc);
		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
		if ((mask & IFCAP_TXCSUM) != 0 &&
		    (if_getcapabilities(ifp) & IFCAP_TXCSUM) != 0) {
			if_togglecapenable(ifp, IFCAP_TXCSUM);
			if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0)
				if_sethwassistbits(ifp, AXGE_CSUM_FEATURES, 0);
			else
				if_sethwassistbits(ifp, 0, AXGE_CSUM_FEATURES);
			reinit++;
		}
		if ((mask & IFCAP_RXCSUM) != 0 &&
		    (if_getcapabilities(ifp) & IFCAP_RXCSUM) != 0) {
			if_togglecapenable(ifp, IFCAP_RXCSUM);
			reinit++;
		}
		if (reinit > 0 && if_getdrvflags(ifp) & IFF_DRV_RUNNING)
			if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
		else
			reinit = 0;
		AXGE_UNLOCK(sc);
		if (reinit > 0)
			uether_init(ue);
	} else
		error = uether_ioctl(ifp, cmd, data);

	return (error);
}

static void
axge_rx_frame(struct usb_ether *ue, struct usb_page_cache *pc, int actlen)
{
	struct axge_frame_rxhdr pkt_hdr;
	uint32_t rxhdr;
	uint32_t pos;
	uint32_t pkt_cnt, pkt_end;
	uint32_t hdr_off;
	uint32_t pktlen;

	/* verify we have enough data */
	if (actlen < (int)sizeof(rxhdr))
		return;

	pos = 0;

	usbd_copy_out(pc, actlen - sizeof(rxhdr), &rxhdr, sizeof(rxhdr));
	rxhdr = le32toh(rxhdr);

	pkt_cnt = rxhdr & 0xFFFF;
	hdr_off = pkt_end = (rxhdr >> 16) & 0xFFFF;

	/*
	 * On older firmware:
	 * <----------------------- actlen ------------------------>
	 * [frame #0]...[frame #N][pkt_hdr #0]...[pkt_hdr #N][rxhdr]
	 *
	 * On newer firmware:
	 * <----------------------- actlen -----------------
	 * [frame #0]...[frame #N][pkt_hdr #0][dummy_hdr]...
	 *                         -------------------------------->
	 *                         ...[pkt_hdr #N][dummy_hdr][rxhdr]
	 *
	 * Each RX frame would be aligned on 8 bytes boundary. If
	 * RCR_IPE bit is set in AXGE_RCR register, there would be 2
	 * padding bytes and 6 dummy bytes(as the padding also should
	 * be aligned on 8 bytes boundary) for each RX frame to align
	 * IP header on 32bits boundary.  Driver don't set RCR_IPE bit
	 * of AXGE_RCR register, so there should be no padding bytes
	 * which simplifies RX logic a lot.
	 *
	 * Further, newer firmware interweaves dummy headers that have
	 * pktlen == 0 and should be skipped without being seen as
	 * dropped frames.
	 */
	while (pkt_cnt--) {
		/* verify the header offset */
		if ((int)(hdr_off + sizeof(pkt_hdr)) > actlen) {
			DPRINTF("End of packet headers\n");
			break;
		}
		usbd_copy_out(pc, hdr_off, &pkt_hdr, sizeof(pkt_hdr));
		pkt_hdr.status = le32toh(pkt_hdr.status);
		pktlen = AXGE_RXBYTES(pkt_hdr.status);
		hdr_off += sizeof(pkt_hdr);

		/* Skip dummy packet header. */
		if (pktlen == 0)
			continue;

		if (pos + pktlen > pkt_end) {
			DPRINTF("Data position reached end\n");
			break;
		}

		if (AXGE_RX_ERR(pkt_hdr.status) != 0) {
			DPRINTF("Dropped a packet\n");
			if_inc_counter(ue->ue_ifp, IFCOUNTER_IERRORS, 1);
		} else
			axge_rxeof(ue, pc, pos, pktlen, pkt_hdr.status);
		pos += (pktlen + 7) & ~7;
	}
}

static void
axge_rxeof(struct usb_ether *ue, struct usb_page_cache *pc, unsigned offset,
    unsigned len, uint32_t status)
{
	if_t ifp;
	struct mbuf *m;

	ifp = ue->ue_ifp;
	if (len < ETHER_HDR_LEN || len > MCLBYTES - ETHER_ALIGN) {
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		return;
	}

	if (len > MHLEN - ETHER_ALIGN)
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	else
		m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
		return;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = len;
	m->m_data += ETHER_ALIGN;

	usbd_copy_out(pc, offset, mtod(m, uint8_t *), len);

	if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0) {
		if ((status & AXGE_RX_L3_CSUM_ERR) == 0 &&
		    (status & AXGE_RX_L3_TYPE_MASK) == AXGE_RX_L3_TYPE_IPV4)
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED |
			    CSUM_IP_VALID;
		if ((status & AXGE_RX_L4_CSUM_ERR) == 0 &&
		    ((status & AXGE_RX_L4_TYPE_MASK) == AXGE_RX_L4_TYPE_UDP ||
		    (status & AXGE_RX_L4_TYPE_MASK) == AXGE_RX_L4_TYPE_TCP)) {
			m->m_pkthdr.csum_flags |= CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}
	}
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

	(void)mbufq_enqueue(&ue->ue_rxq, m);
}

static void
axge_csum_cfg(struct usb_ether *ue)
{
	struct axge_softc *sc;
	if_t ifp;
	uint8_t csum;

	sc = uether_getsc(ue);
	AXGE_LOCK_ASSERT(sc, MA_OWNED);
	ifp = uether_getifp(ue);

	csum = 0;
	if ((if_getcapenable(ifp) & IFCAP_TXCSUM) != 0)
		csum |= CTCR_IP | CTCR_TCP | CTCR_UDP;
	axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_CTCR, csum);

	csum = 0;
	if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0)
		csum |= CRCR_IP | CRCR_TCP | CRCR_UDP;
	axge_write_cmd_1(sc, AXGE_ACCESS_MAC, AXGE_CRCR, csum);
}
