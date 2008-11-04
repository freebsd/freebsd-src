/*-
 * Copyright (c) 1997, 1998, 1999, 2000-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * ASIX Electronics AX88172/AX88178/AX88778 USB 2.0 ethernet driver. Used in the
 * LinkSys USB200M and various other adapters.
 *
 * Manuals available from:
 * http://www.asix.com.tw/datasheet/mac/Ax88172.PDF
 * Note: you need the manual for the AX88170 chip (USB 1.x ethernet
 * controller) to find the definitions for the RX control register.
 * http://www.asix.com.tw/datasheet/mac/Ax88170.PDF
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Engineer
 * Wind River Systems
 */

/*
 * The AX88172 provides USB ethernet supports at 10 and 100Mbps.
 * It uses an external PHY (reference designs use a RealTek chip),
 * and has a 64-bit multicast hash filter. There is some information
 * missing from the manual which one needs to know in order to make
 * the chip function:
 *
 * - You must set bit 7 in the RX control register, otherwise the
 *   chip won't receive any packets.
 * - You must initialize all 3 IPG registers, or you won't be able
 *   to send any packets.
 *
 * Note that this device appears to only support loading the station
 * address via autload from the EEPROM (i.e. there's no way to manaully
 * set it).
 *
 * (Adam Weinberger wanted me to name this driver if_gir.c.)
 */

/*
 * Ax88178 and Ax88772 support backported from the OpenBSD driver.
 * 2007/02/12, J.R. Oldroyd, fbsd@opal.com
 *
 * Manual here:
 * http://www.asix.com.tw/FrootAttach/datasheet/AX88178_datasheet_Rev10.pdf
 * http://www.asix.com.tw/FrootAttach/datasheet/AX88772_datasheet_Rev10.pdf
 */

/*
 * NOTE: all function names beginning like "axe_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc usb2_ether_cc
#define	usb2_config_td_softc axe_softc

#define	USB_DEBUG_VAR axe_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_axe2_reg.h>

MODULE_DEPEND(axe, usb2_ethernet, 1, 1, 1);
MODULE_DEPEND(axe, usb2_core, 1, 1, 1);
MODULE_DEPEND(axe, ether, 1, 1, 1);
MODULE_DEPEND(axe, miibus, 1, 1, 1);

#if USB_DEBUG
static int axe_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, axe, CTLFLAG_RW, 0, "USB axe");
SYSCTL_INT(_hw_usb2_axe, OID_AUTO, debug, CTLFLAG_RW, &axe_debug, 0,
    "Debug level");
#endif

/*
 * Various supported device vendors/products.
 */
static const struct usb2_device_id axe_devs[] = {
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_UF200, 0)},
	{USB_VPI(USB_VENDOR_ACERCM, USB_PRODUCT_ACERCM_EP1427X2, 0)},
	{USB_VPI(USB_VENDOR_APPLE, USB_PRODUCT_APPLE_ETHERNET, AXE_FLAG_772)},
	{USB_VPI(USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88172, 0)},
	{USB_VPI(USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88178, AXE_FLAG_178)},
	{USB_VPI(USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88772, AXE_FLAG_772)},
	{USB_VPI(USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC210T, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D5055, AXE_FLAG_178)},
	{USB_VPI(USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USB2AR, 0)},
	{USB_VPI(USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_USB200MV2, AXE_FLAG_772)},
	{USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB2_TX, 0)},
	{USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DUBE100, 0)},
	{USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DUBE100B1, AXE_FLAG_772)},
	{USB_VPI(USB_VENDOR_GOODWAY, USB_PRODUCT_GOODWAY_GWUSB2E, 0)},
	{USB_VPI(USB_VENDOR_IODATA, USB_PRODUCT_IODATA_ETGUS2, AXE_FLAG_178)},
	{USB_VPI(USB_VENDOR_JVC, USB_PRODUCT_JVC_MP_PRX1, 0)},
	{USB_VPI(USB_VENDOR_LINKSYS2, USB_PRODUCT_LINKSYS2_USB200M, 0)},
	{USB_VPI(USB_VENDOR_LINKSYS4, USB_PRODUCT_LINKSYS4_USB1000, AXE_FLAG_178)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUAU2KTX, 0)},
	{USB_VPI(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_FA120, 0)},
	{USB_VPI(USB_VENDOR_OQO, USB_PRODUCT_OQO_ETHER01PLUS, AXE_FLAG_772)},
	{USB_VPI(USB_VENDOR_PLANEX3, USB_PRODUCT_PLANEX3_GU1000T, AXE_FLAG_178)},
	{USB_VPI(USB_VENDOR_SITECOM, USB_PRODUCT_SITECOM_LN029, 0)},
	{USB_VPI(USB_VENDOR_SITECOMEU, USB_PRODUCT_SITECOMEU_LN028, AXE_FLAG_178)},
	{USB_VPI(USB_VENDOR_SYSTEMTALKS, USB_PRODUCT_SYSTEMTALKS_SGCX2UL, 0)},
};

static device_probe_t axe_probe;
static device_attach_t axe_attach;
static device_detach_t axe_detach;
static device_shutdown_t axe_shutdown;

static usb2_callback_t axe_intr_clear_stall_callback;
static usb2_callback_t axe_intr_callback;
static usb2_callback_t axe_bulk_read_clear_stall_callback;
static usb2_callback_t axe_bulk_read_callback;
static usb2_callback_t axe_bulk_write_clear_stall_callback;
static usb2_callback_t axe_bulk_write_callback;

static void axe_cfg_cmd(struct axe_softc *sc, uint16_t cmd, uint16_t index, uint16_t val, void *buf);

static miibus_readreg_t axe_cfg_miibus_readreg;
static miibus_writereg_t axe_cfg_miibus_writereg;
static miibus_statchg_t axe_cfg_miibus_statchg;

static usb2_config_td_command_t axe_cfg_ifmedia_upd;
static usb2_config_td_command_t axe_config_copy;
static usb2_config_td_command_t axe_cfg_setmulti;
static usb2_config_td_command_t axe_cfg_first_time_setup;
static usb2_config_td_command_t axe_cfg_tick;
static usb2_config_td_command_t axe_cfg_pre_init;
static usb2_config_td_command_t axe_cfg_init;
static usb2_config_td_command_t axe_cfg_promisc_upd;
static usb2_config_td_command_t axe_cfg_pre_stop;
static usb2_config_td_command_t axe_cfg_stop;

static int axe_ifmedia_upd_cb(struct ifnet *ifp);
static void axe_ifmedia_sts_cb(struct ifnet *ifp, struct ifmediareq *ifmr);
static void axe_cfg_reset(struct axe_softc *sc);
static void axe_start_cb(struct ifnet *ifp);
static void axe_start_transfers(struct axe_softc *sc);
static void axe_init_cb(void *arg);
static int axe_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data);
static void axe_watchdog(void *arg);
static void axe_cfg_ax88178_init(struct axe_softc *);
static void axe_cfg_ax88772_init(struct axe_softc *);

static const struct usb2_config axe_config[AXE_ENDPT_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = AXE_BULK_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &axe_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
#if (MCLBYTES < 2048)
#error "(MCLBYTES < 2048)"
#endif
		.mh.bufsize = MCLBYTES,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &axe_bulk_read_callback,
		.mh.timeout = 0,	/* no timeout */
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &axe_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &axe_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[4] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &axe_intr_callback,
	},

	[5] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &axe_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static device_method_t axe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, axe_probe),
	DEVMETHOD(device_attach, axe_attach),
	DEVMETHOD(device_detach, axe_detach),
	DEVMETHOD(device_shutdown, axe_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, axe_cfg_miibus_readreg),
	DEVMETHOD(miibus_writereg, axe_cfg_miibus_writereg),
	DEVMETHOD(miibus_statchg, axe_cfg_miibus_statchg),

	{0, 0}
};

static driver_t axe_driver = {
	.name = "axe",
	.methods = axe_methods,
	.size = sizeof(struct axe_softc),
};

static devclass_t axe_devclass;

DRIVER_MODULE(axe, ushub, axe_driver, axe_devclass, NULL, 0);
DRIVER_MODULE(miibus, axe, miibus_driver, miibus_devclass, 0, 0);

static void
axe_cfg_cmd(struct axe_softc *sc, uint16_t cmd, uint16_t index,
    uint16_t val, void *buf)
{
	struct usb2_device_request req;
	usb2_error_t err;
	uint16_t length = AXE_CMD_LEN(cmd);

	req.bmRequestType = (AXE_CMD_IS_WRITE(cmd) ?
	    UT_WRITE_VENDOR_DEVICE :
	    UT_READ_VENDOR_DEVICE);
	req.bRequest = AXE_CMD_CMD(cmd);
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, length);

	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		goto error;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &sc->sc_mtx, &req, buf, 0, NULL, 1000);

	if (err) {

		DPRINTFN(0, "device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));

error:

		if ((req.bmRequestType & UT_READ) && length) {
			bzero(buf, length);
		}
	}
	return;
}

static int
axe_cfg_miibus_readreg(device_t dev, int phy, int reg)
{
	struct axe_softc *sc = device_get_softc(dev);
	uint16_t val;
	uint8_t do_unlock;

	/* avoid recursive locking */
	if (mtx_owned(&sc->sc_mtx)) {
		do_unlock = 0;
	} else {
		mtx_lock(&sc->sc_mtx);
		do_unlock = 1;
	}

#if 0
	/*
	 * The chip tells us the MII address of any supported
	 * PHYs attached to the chip, so only read from those.
	 */

	if ((sc->sc_phyaddrs[0] != AXE_NOPHY) && (phy != sc->sc_phyaddrs[0])) {
		val = 0;
		goto done;
	}
	if ((sc->sc_phyaddrs[1] != AXE_NOPHY) && (phy != sc->sc_phyaddrs[1])) {
		val = 0;
		goto done;
	}
#endif
	if ((sc->sc_phyaddrs[0] != 0xFF) && (sc->sc_phyaddrs[0] != phy)) {
		val = 0;
		goto done;
	}
	axe_cfg_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	axe_cfg_cmd(sc, AXE_CMD_MII_READ_REG, reg, phy, &val);
	axe_cfg_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);

	val = le16toh(val);

	if (val && (val != 0xffff)) {
		sc->sc_phyaddrs[0] = phy;
	}
done:
	if (do_unlock) {
		mtx_unlock(&sc->sc_mtx);
	}
	return (val);
}

static int
axe_cfg_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct axe_softc *sc = device_get_softc(dev);
	uint8_t do_unlock;

	val = htole16(val);

	/* avoid recursive locking */
	if (mtx_owned(&sc->sc_mtx)) {
		do_unlock = 0;
	} else {
		mtx_lock(&sc->sc_mtx);
		do_unlock = 1;
	}

	axe_cfg_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	axe_cfg_cmd(sc, AXE_CMD_MII_WRITE_REG, reg, phy, &val);
	axe_cfg_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);

	if (do_unlock) {
		mtx_unlock(&sc->sc_mtx);
	}
	return (0);
}

static void
axe_cfg_miibus_statchg(device_t dev)
{
	struct axe_softc *sc = device_get_softc(dev);
	struct mii_data *mii = GET_MII(sc);
	uint16_t val;
	uint8_t do_unlock;

	/* avoid recursive locking */
	if (mtx_owned(&sc->sc_mtx)) {
		do_unlock = 0;
	} else {
		mtx_lock(&sc->sc_mtx);
		do_unlock = 1;
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		val = AXE_MEDIA_FULL_DUPLEX;
	else
		val = 0;

	if (sc->sc_flags & (AXE_FLAG_772 | AXE_FLAG_178)) {

		val |= (AXE_178_MEDIA_RX_EN | AXE_178_MEDIA_MAGIC);

		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_1000_T:
			val |= AXE_178_MEDIA_GMII | AXE_178_MEDIA_ENCK;
			break;
		case IFM_100_TX:
			val |= AXE_178_MEDIA_100TX;
			break;
		case IFM_10_T:
			/* doesn't need to be handled */
			break;
		}
	}
	axe_cfg_cmd(sc, AXE_CMD_WRITE_MEDIA, 0, val, NULL);
	if (do_unlock) {
		mtx_unlock(&sc->sc_mtx);
	}
	return;
}

/*
 * Set media options.
 */
static int
axe_ifmedia_upd_cb(struct ifnet *ifp)
{
	struct axe_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &axe_cfg_ifmedia_upd, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return (0);
}

static void
axe_cfg_ifmedia_upd(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mii_data *mii = GET_MII(sc);

	if ((ifp == NULL) ||
	    (mii == NULL)) {
		/* not ready */
		return;
	}
	sc->sc_flags |= AXE_FLAG_WAIT_LINK;

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
			mii_phy_reset(miisc);
		}
	}
	mii_mediachg(mii);

	return;
}

/*
 * Report current media status.
 */
static void
axe_ifmedia_sts_cb(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct axe_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	ifmr->ifm_active = sc->sc_media_active;
	ifmr->ifm_status = sc->sc_media_status;
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
axe_mchash(struct usb2_config_td_cc *cc, const uint8_t *ptr)
{
	uint8_t h;

	h = (ether_crc32_be(ptr, ETHER_ADDR_LEN) >> 26);
	cc->if_hash[(h >> 3)] |= (1 << (h & 7));
	return;
}

static void
axe_config_copy(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	bzero(cc, sizeof(*cc));
	usb2_ether_cc(sc->sc_ifp, &axe_mchash, cc);
	return;
}

static void
axe_cfg_setmulti(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint16_t rxmode;

	axe_cfg_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, &rxmode);

	rxmode = le16toh(rxmode);

	if (cc->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) {
		rxmode |= AXE_RXCMD_ALLMULTI;
		axe_cfg_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
		return;
	}
	rxmode &= ~AXE_RXCMD_ALLMULTI;

	axe_cfg_cmd(sc, AXE_CMD_WRITE_MCAST, 0, 0, cc->if_hash);
	axe_cfg_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	return;
}

static void
axe_cfg_reset(struct axe_softc *sc)
{
	struct usb2_config_descriptor *cd;
	usb2_error_t err;

	cd = usb2_get_config_descriptor(sc->sc_udev);

	err = usb2_req_set_config(sc->sc_udev, &sc->sc_mtx,
	    cd->bConfigurationValue);
	if (err) {
		DPRINTF("reset failed (ignored)\n");
	}
	/*
	 * wait a little while for the chip to get its brains in order:
	 */
	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 100);

	return;
}

/*
 * Probe for a AX88172 chip.
 */
static int
axe_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != AXE_CONFIG_IDX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != AXE_IFACE_IDX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(axe_devs, sizeof(axe_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
axe_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct axe_softc *sc = device_get_softc(dev);
	int32_t error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);
	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name), "%s",
	    device_get_nameunit(dev));

	mtx_init(&sc->sc_mtx, "axe lock", NULL, MTX_DEF | MTX_RECURSE);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	iface_index = AXE_IFACE_IDX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, axe_config, AXE_ENDPT_MAX,
	    sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB "
		    "transfers failed!\n");
		goto detach;
	}
	error = usb2_config_td_setup(&sc->sc_config_td, sc, &sc->sc_mtx,
	    NULL, sizeof(struct usb2_config_td_cc), 16);
	if (error) {
		device_printf(dev, "could not setup config "
		    "thread!\n");
		goto detach;
	}
	mtx_lock(&sc->sc_mtx);

	sc->sc_flags |= AXE_FLAG_WAIT_LINK;

	/* start setup */

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &axe_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	axe_watchdog(sc);

	return (0);			/* success */

detach:
	axe_detach(dev);
	return (ENXIO);			/* failure */
}

static void
axe_cfg_ax88178_init(struct axe_softc *sc)
{
	uint16_t eeprom;
	uint16_t phymode;
	uint16_t gpio0;
	uint8_t err;

	DPRINTF("\n");

	axe_cfg_cmd(sc, AXE_CMD_SROM_WR_ENABLE, 0, 0, NULL);
	/* XXX magic */
	axe_cfg_cmd(sc, AXE_CMD_SROM_READ, 0, 0x0017, &eeprom);
	axe_cfg_cmd(sc, AXE_CMD_SROM_WR_DISABLE, 0, 0, NULL);

	/* For big-endian machines: */
	eeprom = le16toh(eeprom);

	/* if EEPROM is invalid we have to use to GPIO0 */
	if (eeprom == 0xffff) {
		phymode = 0;
		gpio0 = 1;
	} else {
		phymode = (eeprom & 7);
		gpio0 = (eeprom & 0x80) ? 0 : 1;
	}

	axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x008c, NULL);
	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 16);

	if ((eeprom >> 8) != 0x01) {
		axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 32);

		axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x001c, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 3);

		axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 32);
	} else {
		axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x0004, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 32);

		axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x000c, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 32);
	}

	/* soft reset */
	axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_CLEAR, NULL);
	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 4);

	axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
	    AXE_SW_RESET_PRL | AXE_178_RESET_MAGIC, NULL);
	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 4);

	axe_cfg_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
	return;
}

static void
axe_cfg_ax88772_init(struct axe_softc *sc)
{
	uint8_t err;

	DPRINTF("\n");

	axe_cfg_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x00b0, NULL);
	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 16);

	if (sc->sc_phyaddrs[1] == AXE_INTPHY) {
		/* ask for the embedded PHY */
		axe_cfg_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x01, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 64);

		/* power down and reset state, pin reset state */
		axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_CLEAR, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 16);

		/* power down/reset state, pin operating state */
		axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 4);

		/* power up, reset */
		axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_PRL, NULL);

		/* power up, operating */
		axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPRL | AXE_SW_RESET_PRL, NULL);
	} else {
		/* ask for external PHY */
		axe_cfg_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x00, NULL);
		err = usb2_config_td_sleep(&sc->sc_config_td, hz / 64);

		/* power down internal PHY */
		axe_cfg_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
	}

	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 4);
	axe_cfg_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
	return;
}

static void
axe_cfg_first_time_setup(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp;
	int error;
	uint8_t eaddr[min(ETHER_ADDR_LEN, 6)];

	/* set default value */
	bzero(eaddr, sizeof(eaddr));

	/*
	 * Load PHY indexes first. Needed by axe_xxx_init().
	 */
	axe_cfg_cmd(sc, AXE_CMD_READ_PHYID, 0, 0, sc->sc_phyaddrs);

	if (sc->sc_flags & AXE_FLAG_178) {
		axe_cfg_ax88178_init(sc);
	} else if (sc->sc_flags & AXE_FLAG_772) {
		axe_cfg_ax88772_init(sc);
	}
	/*
	 * Get station address.
	 */
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772))
		axe_cfg_cmd(sc, AXE_178_CMD_READ_NODEID, 0, 0, eaddr);
	else
		axe_cfg_cmd(sc, AXE_172_CMD_READ_NODEID, 0, 0, eaddr);

	/*
	 * Fetch IPG values.
	 */
	axe_cfg_cmd(sc, AXE_CMD_READ_IPG012, 0, 0, sc->sc_ipgs);

	/*
	 * Work around broken adapters that appear to lie about
	 * their PHY addresses.
	 */
	sc->sc_phyaddrs[0] = sc->sc_phyaddrs[1] = 0xFF;

	mtx_unlock(&sc->sc_mtx);

	ifp = if_alloc(IFT_ETHER);

	mtx_lock(&sc->sc_mtx);

	if (ifp == NULL) {
		printf("%s: could not if_alloc()\n",
		    sc->sc_name);
		goto done;
	}
	sc->sc_evilhack = ifp;

	ifp->if_softc = sc;
	if_initname(ifp, "axe", sc->sc_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = axe_ioctl_cb;
	ifp->if_start = axe_start_cb;
	ifp->if_watchdog = NULL;
	ifp->if_init = axe_init_cb;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * XXX need Giant when accessing the device structures !
	 */

	mtx_unlock(&sc->sc_mtx);

	mtx_lock(&Giant);

	error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
	    &axe_ifmedia_upd_cb,
	    &axe_ifmedia_sts_cb);
	mtx_unlock(&Giant);

	mtx_lock(&sc->sc_mtx);

	if (error) {
		printf("%s: MII without any PHY!\n",
		    sc->sc_name);
		if_free(ifp);
		goto done;
	}
	sc->sc_ifp = ifp;

	mtx_unlock(&sc->sc_mtx);

	/*
	 * Call MI attach routine.
	 */

	ether_ifattach(ifp, eaddr);

	mtx_lock(&sc->sc_mtx);

done:
	return;
}

static int
axe_detach(device_t dev)
{
	struct axe_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	axe_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, AXE_ENDPT_MAX);

	/* get rid of any late children */
	bus_generic_detach(dev);

	if (ifp) {
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	usb2_config_td_unsetup(&sc->sc_config_td);

	usb2_callout_drain(&sc->sc_watchdog);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
axe_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct axe_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[4];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~AXE_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
axe_intr_callback(struct usb2_xfer *xfer)
{
	struct axe_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		/* do nothing */

	case USB_ST_SETUP:
		if (sc->sc_flags & AXE_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[5]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* start clear stall */
			sc->sc_flags |= AXE_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[5]);
		}
		return;
	}
}

static void
axe_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct axe_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~AXE_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

#if (AXE_BULK_BUF_SIZE >= 0x10000)
#error "Please update axe_bulk_read_callback()!"
#endif

static void
axe_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct axe_softc *sc = xfer->priv_sc;
	struct axe_sframe_hdr hdr;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct {			/* mini-queue */
		struct mbuf *ifq_head;
		struct mbuf *ifq_tail;
		uint16_t ifq_len;
	}      mq = {
		NULL, NULL, 0
	};
	uint16_t pos;
	uint16_t len;
	uint16_t adjust;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		pos = 0;

		while (1) {

			if (sc->sc_flags & (AXE_FLAG_772 | AXE_FLAG_178)) {

				if (xfer->actlen < sizeof(hdr)) {
					/* too little data */
					break;
				}
				usb2_copy_out(xfer->frbuffers, pos, &hdr, sizeof(hdr));

				if ((hdr.len ^ hdr.ilen) != 0xFFFF) {
					/* we lost sync */
					break;
				}
				xfer->actlen -= sizeof(hdr);
				pos += sizeof(hdr);

				len = le16toh(hdr.len);
				if (len > xfer->actlen) {
					/* invalid length */
					break;
				}
				adjust = (len & 1);

			} else {
				len = xfer->actlen;
				adjust = 0;
			}

			if (len < sizeof(struct ether_header)) {
				ifp->if_ierrors++;
				goto skip;
			}
			m = usb2_ether_get_mbuf();
			if (m == NULL) {
				/* we are out of memory */
				break;
			}
			if (m->m_len > len) {
				m->m_len = len;
			}
			usb2_copy_out(xfer->frbuffers, pos, m->m_data, m->m_len);

			ifp->if_ipackets++;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len;

			/* enqueue */
			_IF_ENQUEUE(&mq, m);

	skip:

			pos += len;
			xfer->actlen -= len;

			if (xfer->actlen <= adjust) {
				/* we are finished */
				goto tr_setup;
			}
			pos += adjust;
			xfer->actlen -= adjust;
		}

		/* count an error */
		ifp->if_ierrors++;

	case USB_ST_SETUP:
tr_setup:

		if (sc->sc_flags & AXE_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}

		/*
		 * At the end of a USB callback it is always safe to unlock
		 * the private mutex of a device! That is why we do the
		 * "if_input" here, and not some lines up!
		 */
		if (mq.ifq_head) {

			mtx_unlock(&sc->sc_mtx);

			while (1) {

				_IF_DEQUEUE(&mq, m);

				if (m == NULL)
					break;

				(ifp->if_input) (ifp, m);
			}

			mtx_lock(&sc->sc_mtx);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= AXE_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		DPRINTF("bulk read error, %s\n",
		    usb2_errstr(xfer->error));
		return;

	}
}

static void
axe_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct axe_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~AXE_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

#if ((AXE_BULK_BUF_SIZE >= 0x10000) || (AXE_BULK_BUF_SIZE < (MCLBYTES+4)))
#error "Please update axe_bulk_write_callback()!"
#endif

static void
axe_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct axe_softc *sc = xfer->priv_sc;
	struct axe_sframe_hdr hdr;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint16_t pos;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");

		ifp->if_opackets++;

	case USB_ST_SETUP:

		if (sc->sc_flags & AXE_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			goto done;
		}
		if (sc->sc_flags & AXE_FLAG_WAIT_LINK) {
			/*
			 * don't send anything if there is no link !
			 */
			goto done;
		}
		pos = 0;

		while (1) {

			IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

			if (m == NULL) {
				if (pos > 0)
					break;	/* send out data */
				else
					goto done;
			}
			if (m->m_pkthdr.len > MCLBYTES) {
				m->m_pkthdr.len = MCLBYTES;
			}
			if (sc->sc_flags & (AXE_FLAG_772 | AXE_FLAG_178)) {

				hdr.len = htole16(m->m_pkthdr.len);
				hdr.ilen = ~hdr.len;

				usb2_copy_in(xfer->frbuffers, pos, &hdr, sizeof(hdr));

				pos += sizeof(hdr);

				/*
				 * NOTE: Some drivers force a short packet
				 * by appending a dummy header with zero
				 * length at then end of the USB transfer.
				 * This driver uses the
				 * USB_FORCE_SHORT_XFER flag instead.
				 */
			}
			usb2_m_copy_in(xfer->frbuffers, pos,
			    m, 0, m->m_pkthdr.len);

			pos += m->m_pkthdr.len;

			/*
		         * if there's a BPF listener, bounce a copy
		         * of this frame to him:
		         */
			BPF_MTAP(ifp, m);

			m_freem(m);

			if (sc->sc_flags & (AXE_FLAG_772 | AXE_FLAG_178)) {
				if (pos > (AXE_BULK_BUF_SIZE - MCLBYTES - sizeof(hdr))) {
					/* send out frame(s) */
					break;
				}
			} else {
				/* send out frame */
				break;
			}
		}

		xfer->frlengths[0] = pos;
		usb2_start_hardware(xfer);

done:
		return;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= AXE_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		ifp->if_oerrors++;
		return;

	}
}

static void
axe_cfg_tick(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mii_data *mii = GET_MII(sc);

	if ((ifp == NULL) ||
	    (mii == NULL)) {
		/* not ready */
		return;
	}
	mii_tick(mii);

	mii_pollstat(mii);

	if ((sc->sc_flags & AXE_FLAG_WAIT_LINK) &&
	    (mii->mii_media_status & IFM_ACTIVE) &&
	    (IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)) {
		sc->sc_flags &= ~AXE_FLAG_WAIT_LINK;
	}
	sc->sc_media_active = mii->mii_media_active;
	sc->sc_media_status = mii->mii_media_status;

	/* start stopped transfers, if any */

	axe_start_transfers(sc);

	return;
}

static void
axe_start_cb(struct ifnet *ifp)
{
	struct axe_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	axe_start_transfers(sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
axe_start_transfers(struct axe_softc *sc)
{
	if ((sc->sc_flags & AXE_FLAG_LL_READY) &&
	    (sc->sc_flags & AXE_FLAG_HL_READY)) {

		/*
		 * start the USB transfers, if not already started:
		 */
		usb2_transfer_start(sc->sc_xfer[4]);
		usb2_transfer_start(sc->sc_xfer[1]);
		usb2_transfer_start(sc->sc_xfer[0]);
	}
	return;
}

static void
axe_init_cb(void *arg)
{
	struct axe_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &axe_cfg_pre_init, &axe_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
axe_cfg_pre_init(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* immediate configuration */

	axe_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= AXE_FLAG_HL_READY;
	return;
}

static void
axe_cfg_init(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct mii_data *mii = GET_MII(sc);
	uint16_t rxmode;

	/*
	 * Cancel pending I/O
	 */

	axe_cfg_stop(sc, cc, 0);

#if 0
	/* Set MAC address */
	axe_mac(sc, cc->if_lladdr);
#endif

	/* Set transmitter IPG values */
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772)) {
		axe_cfg_cmd(sc, AXE_178_CMD_WRITE_IPG012, sc->sc_ipgs[2],
		    (sc->sc_ipgs[1] << 8) | (sc->sc_ipgs[0]), NULL);
	} else {
		axe_cfg_cmd(sc, AXE_172_CMD_WRITE_IPG0, 0, sc->sc_ipgs[0], NULL);
		axe_cfg_cmd(sc, AXE_172_CMD_WRITE_IPG1, 0, sc->sc_ipgs[1], NULL);
		axe_cfg_cmd(sc, AXE_172_CMD_WRITE_IPG2, 0, sc->sc_ipgs[2], NULL);
	}

	/* Enable receiver, set RX mode */
	rxmode = (AXE_RXCMD_MULTICAST | AXE_RXCMD_ENABLE);
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772)) {
		rxmode |= AXE_178_RXCMD_MFB_2048;	/* chip default */
	} else {
		rxmode |= AXE_172_RXCMD_UNICAST;
	}

	/* If we want promiscuous mode, set the allframes bit. */
	if (cc->if_flags & IFF_PROMISC) {
		rxmode |= AXE_RXCMD_PROMISC;
	}
	if (cc->if_flags & IFF_BROADCAST) {
		rxmode |= AXE_RXCMD_BROADCAST;
	}
	axe_cfg_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	/* Load the multicast filter. */
	axe_cfg_setmulti(sc, cc, 0);

	mii_mediachg(mii);

	sc->sc_flags |= (AXE_FLAG_READ_STALL |
	    AXE_FLAG_WRITE_STALL |
	    AXE_FLAG_LL_READY);

	axe_start_transfers(sc);

	return;
}

static void
axe_cfg_promisc_upd(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint16_t rxmode;

	axe_cfg_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, &rxmode);

	rxmode = le16toh(rxmode);

	if (cc->if_flags & IFF_PROMISC) {
		rxmode |= AXE_RXCMD_PROMISC;
	} else {
		rxmode &= ~AXE_RXCMD_PROMISC;
	}

	axe_cfg_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	axe_cfg_setmulti(sc, cc, 0);

	return;
}

static int
axe_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct axe_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &axe_config_copy,
				    &axe_cfg_promisc_upd, 0, 0);
			} else {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &axe_cfg_pre_init,
				    &axe_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &axe_cfg_pre_stop,
				    &axe_cfg_stop, 0, 0);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mtx_lock(&sc->sc_mtx);
		usb2_config_td_queue_command
		    (&sc->sc_config_td, &axe_config_copy,
		    &axe_cfg_setmulti, 0, 0);
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		if (mii == NULL) {
			error = EINVAL;
		} else {
			error = ifmedia_ioctl
			    (ifp, (void *)data, &mii->mii_media, command);
		}
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static void
axe_watchdog(void *arg)
{
	struct axe_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &axe_cfg_tick, 0, 0);

	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &axe_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);
	return;
}

/*
 * NOTE: can be called when "ifp" is NULL
 */
static void
axe_cfg_pre_stop(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		axe_config_copy(sc, cc, refcount);
	}
	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(AXE_FLAG_HL_READY |
	    AXE_FLAG_LL_READY);

	sc->sc_flags |= AXE_FLAG_WAIT_LINK;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[0]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[4]);
	usb2_transfer_stop(sc->sc_xfer[5]);
	return;
}

static void
axe_cfg_stop(struct axe_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	axe_cfg_reset(sc);
	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
axe_shutdown(device_t dev)
{
	struct axe_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &axe_cfg_pre_stop,
	    &axe_cfg_stop, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return (0);
}
