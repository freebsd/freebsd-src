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
 * ASIX Electronics AX88172/AX88178/AX88778 USB 2.0 ethernet driver.
 * Used in the LinkSys USB200M and various other adapters.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR axe_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/net/usb_ethernet.h>
#include <dev/usb/net/if_axereg.h>

/*
 * AXE_178_MAX_FRAME_BURST
 * max frame burst size for Ax88178 and Ax88772
 *	0	2048 bytes
 *	1	4096 bytes
 *	2	8192 bytes
 *	3	16384 bytes
 * use the largest your system can handle without USB stalling.
 *
 * NB: 88772 parts appear to generate lots of input errors with
 * a 2K rx buffer and 8K is only slightly faster than 4K on an
 * EHCI port on a T42 so change at your own risk.
 */
#define AXE_178_MAX_FRAME_BURST	1

#if USB_DEBUG
static int axe_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, axe, CTLFLAG_RW, 0, "USB axe");
SYSCTL_INT(_hw_usb_axe, OID_AUTO, debug, CTLFLAG_RW, &axe_debug, 0,
    "Debug level");
#endif

/*
 * Various supported device vendors/products.
 */
static const struct usb_device_id axe_devs[] = {
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

static usb_callback_t axe_intr_callback;
static usb_callback_t axe_bulk_read_callback;
static usb_callback_t axe_bulk_write_callback;

static miibus_readreg_t axe_miibus_readreg;
static miibus_writereg_t axe_miibus_writereg;
static miibus_statchg_t axe_miibus_statchg;

static uether_fn_t axe_attach_post;
static uether_fn_t axe_init;
static uether_fn_t axe_stop;
static uether_fn_t axe_start;
static uether_fn_t axe_tick;
static uether_fn_t axe_setmulti;
static uether_fn_t axe_setpromisc;

static int	axe_ifmedia_upd(struct ifnet *);
static void	axe_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int	axe_cmd(struct axe_softc *, int, int, int, void *);
static void	axe_ax88178_init(struct axe_softc *);
static void	axe_ax88772_init(struct axe_softc *);
static int	axe_get_phyno(struct axe_softc *, int);

static const struct usb_config axe_config[AXE_N_TRANSFER] = {

	[AXE_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = AXE_BULK_BUF_SIZE,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = axe_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
	},

	[AXE_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 16384,	/* bytes */
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = axe_bulk_read_callback,
		.timeout = 0,	/* no timeout */
	},

	[AXE_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = axe_intr_callback,
	},
};

static device_method_t axe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, axe_probe),
	DEVMETHOD(device_attach, axe_attach),
	DEVMETHOD(device_detach, axe_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, axe_miibus_readreg),
	DEVMETHOD(miibus_writereg, axe_miibus_writereg),
	DEVMETHOD(miibus_statchg, axe_miibus_statchg),

	{0, 0}
};

static driver_t axe_driver = {
	.name = "axe",
	.methods = axe_methods,
	.size = sizeof(struct axe_softc),
};

static devclass_t axe_devclass;

DRIVER_MODULE(axe, uhub, axe_driver, axe_devclass, NULL, 0);
DRIVER_MODULE(miibus, axe, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(axe, uether, 1, 1, 1);
MODULE_DEPEND(axe, usb, 1, 1, 1);
MODULE_DEPEND(axe, ether, 1, 1, 1);
MODULE_DEPEND(axe, miibus, 1, 1, 1);

static const struct usb_ether_methods axe_ue_methods = {
	.ue_attach_post = axe_attach_post,
	.ue_start = axe_start,
	.ue_init = axe_init,
	.ue_stop = axe_stop,
	.ue_tick = axe_tick,
	.ue_setmulti = axe_setmulti,
	.ue_setpromisc = axe_setpromisc,
	.ue_mii_upd = axe_ifmedia_upd,
	.ue_mii_sts = axe_ifmedia_sts,
};

static int
axe_cmd(struct axe_softc *sc, int cmd, int index, int val, void *buf)
{
	struct usb_device_request req;
	usb_error_t err;

	AXE_LOCK_ASSERT(sc, MA_OWNED);

	req.bmRequestType = (AXE_CMD_IS_WRITE(cmd) ?
	    UT_WRITE_VENDOR_DEVICE :
	    UT_READ_VENDOR_DEVICE);
	req.bRequest = AXE_CMD_CMD(cmd);
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, AXE_CMD_LEN(cmd));

	err = uether_do_request(&sc->sc_ue, &req, buf, 1000);

	return (err);
}

static int
axe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct axe_softc *sc = device_get_softc(dev);
	uint16_t val;
	int locked;

	if (sc->sc_phyno != phy)
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AXE_LOCK(sc);

	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	axe_cmd(sc, AXE_CMD_MII_READ_REG, reg, phy, &val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);

	val = le16toh(val);
	if ((sc->sc_flags & AXE_FLAG_772) != 0 && reg == MII_BMSR) {
		/*
		 * BMSR of AX88772 indicates that it supports extended
		 * capability but the extended status register is
		 * revered for embedded ethernet PHY. So clear the
		 * extended capability bit of BMSR.
		 */
		val &= ~BMSR_EXTCAP;
	}

	if (!locked)
		AXE_UNLOCK(sc);
	return (val);
}

static int
axe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct axe_softc *sc = device_get_softc(dev);
	int locked;

	val = htole32(val);

	if (sc->sc_phyno != phy)
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AXE_LOCK(sc);

	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	axe_cmd(sc, AXE_CMD_MII_WRITE_REG, reg, phy, &val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);

	if (!locked)
		AXE_UNLOCK(sc);
	return (0);
}

static void
axe_miibus_statchg(device_t dev)
{
	struct axe_softc *sc = device_get_softc(dev);
	struct mii_data *mii = GET_MII(sc);
	struct ifnet *ifp;
	uint16_t val;
	int err, locked;

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AXE_LOCK(sc);

	ifp = uether_getifp(&sc->sc_ue);
	if (mii == NULL || ifp == NULL ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		goto done;
 
	sc->sc_flags &= ~AXE_FLAG_LINK;
	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			sc->sc_flags |= AXE_FLAG_LINK;
			break;
		case IFM_1000_T:
			if ((sc->sc_flags & AXE_FLAG_178) == 0)
				break;
			sc->sc_flags |= AXE_FLAG_LINK;
			break;
		default:
			break;
		}
	}
 
	/* Lost link, do nothing. */
	if ((sc->sc_flags & AXE_FLAG_LINK) == 0)
		goto done;
 
	val = 0;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0)
		val |= AXE_MEDIA_FULL_DUPLEX;
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772)) {
		val |= AXE_178_MEDIA_RX_EN | AXE_178_MEDIA_MAGIC;
		if ((sc->sc_flags & AXE_FLAG_178) != 0)
			val |= AXE_178_MEDIA_ENCK;
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
	err = axe_cmd(sc, AXE_CMD_WRITE_MEDIA, 0, val, NULL);
	if (err)
		device_printf(dev, "media change failed, error %d\n", err);
done:
	if (!locked)
		AXE_UNLOCK(sc);
}

/*
 * Set media options.
 */
static int
axe_ifmedia_upd(struct ifnet *ifp)
{
	struct axe_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);
	int error;

	AXE_LOCK_ASSERT(sc, MA_OWNED);

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);
	return (error);
}

/*
 * Report current media status.
 */
static void
axe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct axe_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	AXE_LOCK(sc);
	mii_pollstat(mii);
	AXE_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
axe_setmulti(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);
	struct ifmultiaddr *ifma;
	uint32_t h = 0;
	uint16_t rxmode;
	uint8_t hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	AXE_LOCK_ASSERT(sc, MA_OWNED);

	axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, &rxmode);
	rxmode = le16toh(rxmode);

	if (ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) {
		rxmode |= AXE_RXCMD_ALLMULTI;
		axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
		return;
	}
	rxmode &= ~AXE_RXCMD_ALLMULTI;

	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		hashtbl[h / 8] |= 1 << (h % 8);
	}
	if_maddr_runlock(ifp);

	axe_cmd(sc, AXE_CMD_WRITE_MCAST, 0, 0, (void *)&hashtbl);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
}

static int
axe_get_phyno(struct axe_softc *sc, int sel)
{
	int phyno;

	switch (AXE_PHY_TYPE(sc->sc_phyaddrs[sel])) {
	case PHY_TYPE_100_HOME:
	case PHY_TYPE_GIG:
		phyno = AXE_PHY_NO(sc->sc_phyaddrs[sel]);
		break;
	case PHY_TYPE_SPECIAL:
		/* FALLTHROUGH */
	case PHY_TYPE_RSVD:
		/* FALLTHROUGH */
	case PHY_TYPE_NON_SUP:
		/* FALLTHROUGH */
	default:
		phyno = -1;
		break;
	}

	return (phyno);
}

static void
axe_ax88178_init(struct axe_softc *sc)
{
	int gpio0 = 0, phymode = 0;
	uint16_t eeprom;

	axe_cmd(sc, AXE_CMD_SROM_WR_ENABLE, 0, 0, NULL);
	/* XXX magic */
	axe_cmd(sc, AXE_CMD_SROM_READ, 0, 0x0017, &eeprom);
	eeprom = le16toh(eeprom);
	axe_cmd(sc, AXE_CMD_SROM_WR_DISABLE, 0, 0, NULL);

	/* if EEPROM is invalid we have to use to GPIO0 */
	if (eeprom == 0xffff) {
		phymode = 0;
		gpio0 = 1;
	} else {
		phymode = eeprom & 7;
		gpio0 = (eeprom & 0x80) ? 0 : 1;
	}

	axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x008c, NULL);
	uether_pause(&sc->sc_ue, hz / 16);

	if ((eeprom >> 8) != 0x01) {
		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
		uether_pause(&sc->sc_ue, hz / 32);

		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x001c, NULL);
		uether_pause(&sc->sc_ue, hz / 3);

		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
		uether_pause(&sc->sc_ue, hz / 32);
	} else {
		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x0004, NULL);
		uether_pause(&sc->sc_ue, hz / 32);

		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x000c, NULL);
		uether_pause(&sc->sc_ue, hz / 32);
	}

	/* soft reset */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_CLEAR, NULL);
	uether_pause(&sc->sc_ue, hz / 4);

	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
	    AXE_SW_RESET_PRL | AXE_178_RESET_MAGIC, NULL);
	uether_pause(&sc->sc_ue, hz / 4);
	/* Enable MII/GMII/RGMII interface to work with external PHY. */
	axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0, NULL);
	uether_pause(&sc->sc_ue, hz / 4);

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

static void
axe_ax88772_init(struct axe_softc *sc)
{
	axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x00b0, NULL);
	uether_pause(&sc->sc_ue, hz / 16);

	if (sc->sc_phyno == AXE_772_PHY_NO_EPHY) {
		/* ask for the embedded PHY */
		axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x01, NULL);
		uether_pause(&sc->sc_ue, hz / 64);

		/* power down and reset state, pin reset state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_CLEAR, NULL);
		uether_pause(&sc->sc_ue, hz / 16);

		/* power down/reset state, pin operating state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
		uether_pause(&sc->sc_ue, hz / 4);

		/* power up, reset */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_PRL, NULL);

		/* power up, operating */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPRL | AXE_SW_RESET_PRL, NULL);
	} else {
		/* ask for external PHY */
		axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x00, NULL);
		uether_pause(&sc->sc_ue, hz / 64);

		/* power down internal PHY */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
	}

	uether_pause(&sc->sc_ue, hz / 4);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

static void
axe_reset(struct axe_softc *sc)
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
}

static void
axe_attach_post(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);

	/*
	 * Load PHY indexes first. Needed by axe_xxx_init().
	 */
	axe_cmd(sc, AXE_CMD_READ_PHYID, 0, 0, sc->sc_phyaddrs);
#if 1
	device_printf(sc->sc_ue.ue_dev, "PHYADDR 0x%02x:0x%02x\n",
	    sc->sc_phyaddrs[0], sc->sc_phyaddrs[1]);
#endif
	sc->sc_phyno = axe_get_phyno(sc, AXE_PHY_SEL_PRI);
	if (sc->sc_phyno == -1)
		sc->sc_phyno = axe_get_phyno(sc, AXE_PHY_SEL_SEC);
	if (sc->sc_phyno == -1) {
		device_printf(sc->sc_ue.ue_dev,
		    "no valid PHY address found, assuming PHY address 0\n");
		sc->sc_phyno = 0;
	}

	if (sc->sc_flags & AXE_FLAG_178)
		axe_ax88178_init(sc);
	else if (sc->sc_flags & AXE_FLAG_772)
		axe_ax88772_init(sc);

	/*
	 * Get station address.
	 */
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772))
		axe_cmd(sc, AXE_178_CMD_READ_NODEID, 0, 0, ue->ue_eaddr);
	else
		axe_cmd(sc, AXE_172_CMD_READ_NODEID, 0, 0, ue->ue_eaddr);

	/*
	 * Fetch IPG values.
	 */
	axe_cmd(sc, AXE_CMD_READ_IPG012, 0, 0, sc->sc_ipgs);
}

/*
 * Probe for a AX88172 chip.
 */
static int
axe_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != AXE_CONFIG_IDX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != AXE_IFACE_IDX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(axe_devs, sizeof(axe_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
axe_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct axe_softc *sc = device_get_softc(dev);
	struct usb_ether *ue = &sc->sc_ue;
	uint8_t iface_index;
	int error;

	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	device_set_usb_desc(dev);

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	iface_index = AXE_IFACE_IDX;
	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    axe_config, AXE_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB transfers failed!\n");
		goto detach;
	}

	ue->ue_sc = sc;
	ue->ue_dev = dev;
	ue->ue_udev = uaa->device;
	ue->ue_mtx = &sc->sc_mtx;
	ue->ue_methods = &axe_ue_methods;

	error = uether_ifattach(ue);
	if (error) {
		device_printf(dev, "could not attach interface\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	axe_detach(dev);
	return (ENXIO);			/* failure */
}

static int
axe_detach(device_t dev)
{
	struct axe_softc *sc = device_get_softc(dev);
	struct usb_ether *ue = &sc->sc_ue;

	usbd_transfer_unsetup(sc->sc_xfer, AXE_N_TRANSFER);
	uether_ifdetach(ue);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
axe_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		return;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

#if (AXE_BULK_BUF_SIZE >= 0x10000)
#error "Please update axe_bulk_read_callback()!"
#endif

static void
axe_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct axe_softc *sc = usbd_xfer_softc(xfer);
	struct usb_ether *ue = &sc->sc_ue;
	struct ifnet *ifp = uether_getifp(ue);
	struct axe_sframe_hdr hdr;
	struct usb_page_cache *pc;
	int err, pos, len;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pos = 0;
		len = 0;
		err = 0;

		pc = usbd_xfer_get_frame(xfer, 0);
		if (sc->sc_flags & (AXE_FLAG_772 | AXE_FLAG_178)) {
			while (pos < actlen) {
				if ((pos + sizeof(hdr)) > actlen) {
					/* too little data */
					err = EINVAL;
					break;
				}
				usbd_copy_out(pc, pos, &hdr, sizeof(hdr));

				if ((hdr.len ^ hdr.ilen) != 0xFFFF) {
					/* we lost sync */
					err = EINVAL;
					break;
				}
				pos += sizeof(hdr);

				len = le16toh(hdr.len);
				if ((pos + len) > actlen) {
					/* invalid length */
					err = EINVAL;
					break;
				}
				err = uether_rxbuf(ue, pc, pos, len);

				pos += len + (len % 2);
			}
		} else {
			err = uether_rxbuf(ue, pc, 0, actlen);
		}

		if (err != 0)
			ifp->if_ierrors++;

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		uether_rxflush(ue);
		return;

	default:			/* Error */
		DPRINTF("bulk read error, %s\n", usbd_errstr(error));

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;

	}
}

#if ((AXE_BULK_BUF_SIZE >= 0x10000) || (AXE_BULK_BUF_SIZE < (MCLBYTES+4)))
#error "Please update axe_bulk_write_callback()!"
#endif

static void
axe_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct axe_softc *sc = usbd_xfer_softc(xfer);
	struct axe_sframe_hdr hdr;
	struct ifnet *ifp = uether_getifp(&sc->sc_ue);
	struct usb_page_cache *pc;
	struct mbuf *m;
	int pos;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");
		ifp->if_opackets++;
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		if ((sc->sc_flags & AXE_FLAG_LINK) == 0) {
			/*
			 * don't send anything if there is no link !
			 */
			return;
		}
		pos = 0;
		pc = usbd_xfer_get_frame(xfer, 0);

		while (1) {

			IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

			if (m == NULL) {
				if (pos > 0)
					break;	/* send out data */
				return;
			}
			if (m->m_pkthdr.len > MCLBYTES) {
				m->m_pkthdr.len = MCLBYTES;
			}
			if (sc->sc_flags & (AXE_FLAG_772 | AXE_FLAG_178)) {

				hdr.len = htole16(m->m_pkthdr.len);
				hdr.ilen = ~hdr.len;

				usbd_copy_in(pc, pos, &hdr, sizeof(hdr));

				pos += sizeof(hdr);

				/*
				 * NOTE: Some drivers force a short packet
				 * by appending a dummy header with zero
				 * length at then end of the USB transfer.
				 * This driver uses the
				 * USB_FORCE_SHORT_XFER flag instead.
				 */
			}
			usbd_m_copy_in(pc, pos, m, 0, m->m_pkthdr.len);
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

		usbd_xfer_set_frame_len(xfer, 0, pos);
		usbd_transfer_submit(xfer);
		return;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usbd_errstr(error));

		ifp->if_oerrors++;

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;

	}
}

static void
axe_tick(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);
	struct mii_data *mii = GET_MII(sc);

	AXE_LOCK_ASSERT(sc, MA_OWNED);

	mii_tick(mii);
	if ((sc->sc_flags & AXE_FLAG_LINK) == 0) {
		axe_miibus_statchg(ue->ue_dev);
		if ((sc->sc_flags & AXE_FLAG_LINK) != 0)
			axe_start(ue);
	}
}

static void
axe_start(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);

	/*
	 * start the USB transfers, if not already started:
	 */
	usbd_transfer_start(sc->sc_xfer[AXE_INTR_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[AXE_BULK_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[AXE_BULK_DT_WR]);
}

static void
axe_init(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);
	uint16_t rxmode;

	AXE_LOCK_ASSERT(sc, MA_OWNED);

	/* Cancel pending I/O */
	axe_stop(ue);

#ifdef notdef
	/* Set MAC address */
	axe_mac(sc, IF_LLADDR(ifp), 1);
#endif

	/* Set transmitter IPG values */
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772)) {
		axe_cmd(sc, AXE_178_CMD_WRITE_IPG012, sc->sc_ipgs[2],
		    (sc->sc_ipgs[1] << 8) | (sc->sc_ipgs[0]), NULL);
	} else {
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG0, 0, sc->sc_ipgs[0], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG1, 0, sc->sc_ipgs[1], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG2, 0, sc->sc_ipgs[2], NULL);
	}

	/* Enable receiver, set RX mode */
	rxmode = (AXE_RXCMD_MULTICAST | AXE_RXCMD_ENABLE);
	if (sc->sc_flags & (AXE_FLAG_178 | AXE_FLAG_772)) {
#if 0
		rxmode |= AXE_178_RXCMD_MFB_2048;	/* chip default */
#else
		/*
		 * Default Rx buffer size is too small to get
		 * maximum performance.
		 */
		rxmode |= AXE_178_RXCMD_MFB_16384;
#endif
	} else {
		rxmode |= AXE_172_RXCMD_UNICAST;
	}

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxmode |= AXE_RXCMD_PROMISC;

	if (ifp->if_flags & IFF_BROADCAST)
		rxmode |= AXE_RXCMD_BROADCAST;

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	/* Load the multicast filter. */
	axe_setmulti(ue);

	usbd_xfer_set_stall(sc->sc_xfer[AXE_BULK_DT_WR]);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	axe_start(ue);
}

static void
axe_setpromisc(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);
	uint16_t rxmode;

	axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, &rxmode);

	rxmode = le16toh(rxmode);

	if (ifp->if_flags & IFF_PROMISC) {
		rxmode |= AXE_RXCMD_PROMISC;
	} else {
		rxmode &= ~AXE_RXCMD_PROMISC;
	}

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	axe_setmulti(ue);
}

static void
axe_stop(struct usb_ether *ue)
{
	struct axe_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);

	AXE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	sc->sc_flags &= ~AXE_FLAG_LINK;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usbd_transfer_stop(sc->sc_xfer[AXE_BULK_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[AXE_BULK_DT_RD]);
	usbd_transfer_stop(sc->sc_xfer[AXE_INTR_DT_RD]);

	axe_reset(sc);
}
