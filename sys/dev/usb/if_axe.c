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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"
#include <dev/usb/usb_ethersubr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#include <dev/usb/if_axereg.h>

/*
 * Various supported device vendors/products.
 */
const struct axe_type axe_devs[] = {
        { { USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_UF200}, 0 },
        { { USB_VENDOR_ACERCM, USB_PRODUCT_ACERCM_EP1427X2}, 0 },
        { { USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88172}, 0 },
        { { USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88772}, AX772 },
        { { USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88178}, AX178 },
        { { USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC210T}, 0 },
        { { USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5D5055 }, AX178 },
        { { USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USB2AR}, 0},
        { { USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_USB200MV2}, AX772 },
        { { USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB2_TX }, 0},
        { { USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DUBE100}, 0 },
        { { USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DUBE100B1 }, AX772 },
        { { USB_VENDOR_GOODWAY, USB_PRODUCT_GOODWAY_GWUSB2E}, 0 },
        { { USB_VENDOR_IODATA, USB_PRODUCT_IODATA_ETGUS2 }, AX178 },
        { { USB_VENDOR_JVC, USB_PRODUCT_JVC_MP_PRX1}, 0 },
        { { USB_VENDOR_LINKSYS2, USB_PRODUCT_LINKSYS2_USB200M}, 0 },
        { { USB_VENDOR_LINKSYS4, USB_PRODUCT_LINKSYS4_USB1000 }, AX178 },
        { { USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUAU2KTX}, 0 },
        { { USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_FA120}, 0 },
        { { USB_VENDOR_OQO, USB_PRODUCT_OQO_ETHER01PLUS }, AX772 },
        { { USB_VENDOR_PLANEX3, USB_PRODUCT_PLANEX3_GU1000T }, AX178 },
        { { USB_VENDOR_SYSTEMTALKS, USB_PRODUCT_SYSTEMTALKS_SGCX2UL}, 0 },
        { { USB_VENDOR_SITECOM, USB_PRODUCT_SITECOM_LN029}, 0 },
        { { USB_VENDOR_SITECOMEU, USB_PRODUCT_SITECOMEU_LN028 }, AX178 }
};

#define axe_lookup(v, p) ((const struct axe_type *)usb_lookup(axe_devs, v, p))

static device_probe_t axe_match;
static device_attach_t axe_attach;
static device_detach_t axe_detach;
static device_shutdown_t axe_shutdown;
static miibus_readreg_t axe_miibus_readreg;
static miibus_writereg_t axe_miibus_writereg;
static miibus_statchg_t axe_miibus_statchg;

static int axe_encap(struct axe_softc *, struct mbuf *, int);
static void axe_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void axe_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void axe_tick(void *);
static void axe_tick_task(void *);
static void axe_rxstart(struct ifnet *);
static void axe_start(struct ifnet *);
static int axe_ioctl(struct ifnet *, u_long, caddr_t);
static void axe_init(void *);
static void axe_stop(struct axe_softc *);
static void axe_watchdog(struct ifnet *);
static int axe_cmd(struct axe_softc *, int, int, int, void *);
static int axe_ifmedia_upd(struct ifnet *);
static void axe_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void axe_setmulti(struct axe_softc *);

static device_method_t axe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		axe_match),
	DEVMETHOD(device_attach,	axe_attach),
	DEVMETHOD(device_detach,	axe_detach),
	DEVMETHOD(device_shutdown,	axe_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	axe_miibus_readreg),
	DEVMETHOD(miibus_writereg,	axe_miibus_writereg),
	DEVMETHOD(miibus_statchg,	axe_miibus_statchg),

	{ 0, 0 }
};

static driver_t axe_driver = {
	"axe",
	axe_methods,
	sizeof(struct axe_softc)
};

static devclass_t axe_devclass;

DRIVER_MODULE(axe, uhub, axe_driver, axe_devclass, usbd_driver_load, 0);
DRIVER_MODULE(miibus, axe, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(axe, usb, 1, 1, 1);
MODULE_DEPEND(axe, miibus, 1, 1, 1);

static int
axe_cmd(struct axe_softc *sc, int cmd, int index, int val, void *buf)
{
	usb_device_request_t	req;
	usbd_status		err;

	AXE_SLEEPLOCKASSERT(sc);
	if (sc->axe_dying)
		return(0);

	if (AXE_CMD_DIR(cmd))
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AXE_CMD_CMD(cmd);
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, AXE_CMD_LEN(cmd));

	err = usbd_do_request(sc->axe_udev, &req, buf);

	if (err)
		return(-1);

	return(0);
}

static int
axe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct axe_softc	*sc = device_get_softc(dev);
	usbd_status		err;
	u_int16_t		val;

	if (sc->axe_dying)
		return(0);

	AXE_SLEEPLOCKASSERT(sc);
#ifdef notdef
	/*
	 * The chip tells us the MII address of any supported
	 * PHYs attached to the chip, so only read from those.
	 */

	if (sc->axe_phyaddrs[0] != AXE_NOPHY && phy != sc->axe_phyaddrs[0])
		return (0);

	if (sc->axe_phyaddrs[1] != AXE_NOPHY && phy != sc->axe_phyaddrs[1])
		return (0);
#endif
	if (sc->axe_phyaddrs[0] != 0xFF && sc->axe_phyaddrs[0] != phy)
		return (0);

	AXE_LOCK(sc);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_READ_REG, reg, phy, (void *)&val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	AXE_UNLOCK(sc);

	if (err) {
		device_printf(sc->axe_dev, "read PHY failed\n");
		return(-1);
	}

	if (val)
		sc->axe_phyaddrs[0] = phy;

	return (val);
}

static int
axe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct axe_softc	*sc = device_get_softc(dev);
	usbd_status		err;

	if (sc->axe_dying)
		return(0);

	AXE_SLEEPLOCKASSERT(sc);
	AXE_LOCK(sc);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_WRITE_REG, reg, phy, (void *)&val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	AXE_UNLOCK(sc);

	if (err) {
		device_printf(sc->axe_dev, "write PHY failed\n");
		return(-1);
	}

	return (0);
}

static void
axe_miibus_statchg(device_t dev)
{
	struct axe_softc	*sc = device_get_softc(dev);
	struct mii_data		*mii = GET_MII(sc);
	int			val, err;

	val = (mii->mii_media_active & IFM_GMASK) == IFM_FDX ?
	    AXE_MEDIA_FULL_DUPLEX : 0;
	if (sc->axe_flags & (AX178|AX772)) {
		val |= AXE_178_MEDIA_RX_EN | AXE_178_MEDIA_MAGIC;

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
}

/*
 * Set media options.
 */
static int
axe_ifmedia_upd(struct ifnet *ifp)
{
        struct axe_softc        *sc = ifp->if_softc;
        struct mii_data         *mii = GET_MII(sc);

        sc->axe_link = 0;
        if (mii->mii_instance) {
                struct mii_softc        *miisc;
                LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
                         mii_phy_reset(miisc);
        }
        mii_mediachg(mii);

        return (0);
}

/*
 * Report current media status.
 */
static void
axe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
        struct axe_softc        *sc = ifp->if_softc;
        struct mii_data         *mii = GET_MII(sc);

        mii_pollstat(mii);
        ifmr->ifm_active = mii->mii_media_active;
        ifmr->ifm_status = mii->mii_media_status;

        return;
}

static void
axe_setmulti(struct axe_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0;
	u_int16_t		rxmode;
	u_int8_t		hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	ifp = sc->axe_ifp;

	AXE_LOCK(sc);
	axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, (void *)&rxmode);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxmode |= AXE_RXCMD_ALLMULTI;
		axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
		AXE_UNLOCK(sc);
		return;
	} else
		rxmode &= ~AXE_RXCMD_ALLMULTI;

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		hashtbl[h / 8] |= 1 << (h % 8);
	}
	IF_ADDR_UNLOCK(ifp);

	axe_cmd(sc, AXE_CMD_WRITE_MCAST, 0, 0, (void *)&hashtbl);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
	AXE_UNLOCK(sc);

	return;
}

static void
axe_ax88178_init(struct axe_softc *sc)
{
	int gpio0 = 0, phymode = 0;
	u_int16_t eeprom;

	axe_cmd(sc, AXE_CMD_SROM_WR_ENABLE, 0, 0, NULL);
	/* XXX magic */
	axe_cmd(sc, AXE_CMD_SROM_READ, 0, 0x0017, &eeprom);
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
	usbd_delay_ms(sc->axe_udev, 40);
	if ((eeprom >> 8) != 1) {
		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
		usbd_delay_ms(sc->axe_udev, 30);

		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x001c, NULL);
		usbd_delay_ms(sc->axe_udev, 300);

		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
		usbd_delay_ms(sc->axe_udev, 30);
	} else {
		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x0004, NULL);
		usbd_delay_ms(sc->axe_udev, 30);
		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x000c, NULL);
		usbd_delay_ms(sc->axe_udev, 30);
	}

	/* soft reset */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, 0, NULL);
	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
	    AXE_SW_RESET_PRL | AXE_178_RESET_MAGIC, NULL);
	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

static void
axe_ax88772_init(struct axe_softc *sc)
{
	axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x00b0, NULL);
	usbd_delay_ms(sc->axe_udev, 40);

	if (sc->axe_phyaddrs[1] == AXE_INTPHY) {
		/* ask for embedded PHY */
		axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x01, NULL);
		usbd_delay_ms(sc->axe_udev, 10);

		/* power down and reset state, pin reset state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_CLEAR, NULL);
		usbd_delay_ms(sc->axe_udev, 60);

		/* power down/reset state, pin operating state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
		usbd_delay_ms(sc->axe_udev, 150);

		/* power up, reset */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_PRL, NULL);

		/* power up, operating */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPRL | AXE_SW_RESET_PRL, NULL);
	} else {
		/* ask for external PHY */
		axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x00, NULL);
		usbd_delay_ms(sc->axe_udev, 10);

		/* power down/reset state, pin operating state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
	}

	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

static void
axe_reset(struct axe_softc *sc)
{
	if (sc->axe_dying)
		return;

	if (usbd_set_config_no(sc->axe_udev, AXE_CONFIG_NO, 1) ||
	    usbd_device2interface_handle(sc->axe_udev, AXE_IFACE_IDX,
	    &sc->axe_iface)) {
		device_printf(sc->axe_dev, "getting interface handle failed\n");
	}

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
	return;
}

/*
 * Probe for a AX88172 chip.
 */
static int
axe_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (!uaa->iface)
		return(UMATCH_NONE);
	return (axe_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
axe_attach(device_t self)
{
	struct axe_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	const struct axe_type *type;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	sc->axe_udev = uaa->device;
	sc->axe_dev = self;
	type = axe_lookup(uaa->vendor, uaa->product);
	if (type != NULL)
		sc->axe_flags = type->axe_flags;

	if (usbd_set_config_no(sc->axe_udev, AXE_CONFIG_NO, 1)) {
		device_printf(sc->axe_dev, "getting interface handle failed\n");
		return ENXIO;
	}

	usb_init_task(&sc->axe_tick_task, axe_tick_task, sc);

	if (usbd_device2interface_handle(uaa->device,
	    AXE_IFACE_IDX, &sc->axe_iface)) {
		device_printf(sc->axe_dev, "getting interface handle failed\n");
		return ENXIO;
	}

	sc->axe_boundary = 64;
#if 0
	if (sc->axe_flags & (AX178|AX772)) {
		if (sc->axe_udev->speed == USB_SPEED_HIGH) {
			sc->axe_bufsz = AXE_178_MAX_BUFSZ;
			sc->axe_boundary = 512;
		} else
			sc->axe_bufsz = AXE_178_MIN_BUFSZ;
	} else
		sc->axe_bufsz = AXE_172_BUFSZ
#else
	sc->axe_bufsz = AXE_172_BUFSZ;
#endif
{ /* XXX debug */
device_printf(sc->axe_dev, "%s, bufsz %d, boundary %d\n",
	sc->axe_flags & AX178 ? "AX88178" :
	sc->axe_flags & AX772 ? "AX88772" : "AX88172",
	sc->axe_bufsz, sc->axe_boundary);
}

	id = usbd_get_interface_descriptor(sc->axe_iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->axe_iface, i);
		if (!ed) {
			device_printf(sc->axe_dev, "couldn't get ep %d\n", i);
			return ENXIO;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->axe_ed[AXE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	mtx_init(&sc->axe_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	sx_init(&sc->axe_sleeplock, device_get_nameunit(self));
	AXE_SLEEPLOCK(sc);
	AXE_LOCK(sc);

	/* We need the PHYID for the init dance in some cases */
	axe_cmd(sc, AXE_CMD_READ_PHYID, 0, 0, (void *)&sc->axe_phyaddrs);

	if (sc->axe_flags & AX178)
		axe_ax88178_init(sc);
	else if (sc->axe_flags & AX772)
		axe_ax88772_init(sc);

	/*
	 * Get station address.
	 */
	if (sc->axe_flags & (AX178|AX772))
		axe_cmd(sc, AXE_178_CMD_READ_NODEID, 0, 0, &eaddr);
	else
		axe_cmd(sc, AXE_172_CMD_READ_NODEID, 0, 0, &eaddr);

	/*
	 * Fetch IPG values.
	 */
	axe_cmd(sc, AXE_CMD_READ_IPG012, 0, 0, (void *)&sc->axe_ipgs);

	/*
	 * Work around broken adapters that appear to lie about
	 * their PHY addresses.
	 */
	sc->axe_phyaddrs[0] = sc->axe_phyaddrs[1] = 0xFF;

	ifp = sc->axe_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->axe_dev, "can not if_alloc()\n");
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		sx_destroy(&sc->axe_sleeplock);
		mtx_destroy(&sc->axe_mtx);
		return ENXIO;
	}
	ifp->if_softc = sc;
	if_initname(ifp, "axe", device_get_unit(sc->axe_dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_ioctl = axe_ioctl;
	ifp->if_start = axe_start;
	ifp->if_watchdog = axe_watchdog;
	ifp->if_init = axe_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	sc->axe_qdat.ifp = ifp;
	sc->axe_qdat.if_rxstart = axe_rxstart;

	if (mii_phy_probe(self, &sc->axe_miibus,
	    axe_ifmedia_upd, axe_ifmedia_sts)) {
		device_printf(sc->axe_dev, "MII without any PHY!\n");
		if_free(ifp);
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		sx_destroy(&sc->axe_sleeplock);
		mtx_destroy(&sc->axe_mtx);
		return ENXIO;
	}

	/*
	 * Call MI attach routine.
	 */

	ether_ifattach(ifp, eaddr);
	callout_handle_init(&sc->axe_stat_ch);
	usb_register_netisr();

	sc->axe_dying = 0;

	AXE_UNLOCK(sc);
	AXE_SLEEPUNLOCK(sc);

	return 0;
}

static int
axe_detach(device_t dev)
{
	struct axe_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	AXE_LOCK(sc);
	ifp = sc->axe_ifp;

	sc->axe_dying = 1;
	untimeout(axe_tick, sc, sc->axe_stat_ch);
	usb_rem_task(sc->axe_udev, &sc->axe_tick_task);

	ether_ifdetach(ifp);
	if_free(ifp);

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);

	AXE_UNLOCK(sc);
	sx_destroy(&sc->axe_sleeplock);
	mtx_destroy(&sc->axe_mtx);

	return(0);
}

static void
axe_rxstart(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct ue_chain	*c;

	sc = ifp->if_softc;
	AXE_LOCK(sc);
	c = &sc->axe_cdata.ue_rx_chain[sc->axe_cdata.ue_rx_prod];

	c->ue_mbuf = usb_ether_newbuf();
	if (c->ue_mbuf == NULL) {
		device_printf(sc->axe_dev, "no memory for rx list "
		    "-- packet dropped!\n");
		ifp->if_ierrors++;
		AXE_UNLOCK(sc);
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(c->ue_xfer);
	AXE_UNLOCK(sc);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
axe_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc	*sc;
	struct ue_chain	*c;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct axe_sframe_hdr	hdr;
	int			total_len = 0, pktlen;

	c = priv;
	sc = c->ue_sc;
	AXE_LOCK(sc);
	ifp = sc->axe_ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		AXE_UNLOCK(sc);
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			AXE_UNLOCK(sc);
			return;
		}
		if (usbd_ratecheck(&sc->axe_rx_notice))
			device_printf(sc->axe_dev, "usb error on rx: %s\n",
			    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->axe_ep[AXE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	m = c->ue_mbuf;
	/* XXX don't handle multiple packets in one transfer */
	if (sc->axe_flags & (AX178|AX772)) {
		if (total_len < sizeof(hdr)) {
			ifp->if_ierrors++;
			goto done;
		}
		m_copydata(m, 0, sizeof(hdr), (caddr_t) &hdr);
		total_len -= sizeof(hdr);

		if ((hdr.len ^ hdr.ilen) != 0xffff) {
			ifp->if_ierrors++;
			goto done;
		}
		pktlen = le16toh(hdr.len);
		if (pktlen > total_len) {
			ifp->if_ierrors++;
			goto done;
		}
		m_adj(m, sizeof(hdr));
	} else {
		if (total_len < sizeof(struct ether_header)) {
			ifp->if_ierrors++;
			goto done;
		}
		pktlen = total_len;
	}

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = (void *)&sc->axe_qdat;
	m->m_pkthdr.len = m->m_len = pktlen;

	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);
	AXE_UNLOCK(sc);

	return;
done:
	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(c->ue_xfer);
	AXE_UNLOCK(sc);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
axe_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc	*sc;
	struct ue_chain	*c;
	struct ifnet		*ifp;
	usbd_status		err;

	c = priv;
	sc = c->ue_sc;
	AXE_LOCK(sc);
	ifp = sc->axe_ifp;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			AXE_UNLOCK(sc);
			return;
		}
		device_printf(sc->axe_dev, "usb error on tx: %s\n",
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->axe_ep[AXE_ENDPT_TX]);
		AXE_UNLOCK(sc);
		return;
	}

	ifp->if_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &err);

	if (c->ue_mbuf != NULL) {
		c->ue_mbuf->m_pkthdr.rcvif = ifp;
		usb_tx_done(c->ue_mbuf);
		c->ue_mbuf = NULL;
	}

	if (err)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	AXE_UNLOCK(sc);

	return;
}

static void
axe_tick(void *xsc)
{
	struct axe_softc *sc = xsc;

	if (sc == NULL)
		return;
	if (sc->axe_dying)
		return;

	/* Perform periodic stuff in process context */
	usb_add_task(sc->axe_udev, &sc->axe_tick_task, USB_TASKQ_DRIVER);
}

static void
axe_tick_task(void *xsc)
{
	struct axe_softc	*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	sc = xsc;

	if (sc == NULL)
		return;

	AXE_SLEEPLOCK(sc);
	AXE_LOCK(sc);

	ifp = sc->axe_ifp;
	mii = GET_MII(sc);
	if (mii == NULL) {
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		return;
	}

	mii_tick(mii);
	if (!sc->axe_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->axe_link++;
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			axe_start(ifp);
	}

	sc->axe_stat_ch = timeout(axe_tick, sc, hz);

	AXE_UNLOCK(sc);
	AXE_SLEEPUNLOCK(sc);

	return;
}

static int
axe_encap(struct axe_softc *sc, struct mbuf *m, int idx)
{
	struct ue_chain	*c;
	usbd_status		err;
	struct axe_sframe_hdr	hdr;
	int			length;

	c = &sc->axe_cdata.ue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	if (sc->axe_flags & (AX178|AX772)) {
		hdr.len = htole16(m->m_pkthdr.len);
		hdr.ilen = ~hdr.len;

		memcpy(c->ue_buf, &hdr, sizeof(hdr));
		length = sizeof(hdr);

		m_copydata(m, 0, m->m_pkthdr.len, c->ue_buf + sizeof(hdr));
		length += m->m_pkthdr.len;

		if ((length % sc->axe_boundary) == 0) {
			hdr.len = 0;
			hdr.ilen = 0xffff;
			memcpy(c->ue_buf + length, &hdr, sizeof(hdr));
			length += sizeof(hdr);
		}
	} else {
		m_copydata(m, 0, m->m_pkthdr.len, c->ue_buf);
		length = m->m_pkthdr.len;
	}
	c->ue_mbuf = m;

	usbd_setup_xfer(c->ue_xfer, sc->axe_ep[AXE_ENDPT_TX],
	    c, c->ue_buf, length, USBD_FORCE_SHORT_XFER, 10000, axe_txeof);

	/* Transmit */
	err = usbd_transfer(c->ue_xfer);
	if (err != USBD_IN_PROGRESS) {
		/* XXX probably don't want to sleep here */
		AXE_SLEEPLOCK(sc);
		axe_stop(sc);
		AXE_SLEEPUNLOCK(sc);
		return(EIO);
	}

	sc->axe_cdata.ue_tx_cnt++;

	return(0);
}

static void
axe_start(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;
	AXE_LOCK(sc);

	if (!sc->axe_link) {
		AXE_UNLOCK(sc);
		return;
	}

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		AXE_UNLOCK(sc);
		return;
	}

	IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		AXE_UNLOCK(sc);
		return;
	}

	if (axe_encap(sc, m_head, 0)) {
		IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		AXE_UNLOCK(sc);
		return;
	}

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	BPF_MTAP(ifp, m_head);

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
	AXE_UNLOCK(sc);

	return;
}

static void
axe_init(void *xsc)
{
	struct axe_softc	*sc = xsc;
	struct ifnet		*ifp = sc->axe_ifp;
	struct ue_chain	*c;
	usbd_status		err;
	int			i;
	int			rxmode;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	AXE_SLEEPLOCK(sc);
	AXE_LOCK(sc);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */

	axe_reset(sc);

#ifdef notdef
	/* Set MAC address */
	axe_mac(sc, IF_LLADDR(sc->axe_ifp), 1);
#endif

	/* Enable RX logic. */

	/* Init TX ring. */
	if (usb_ether_tx_list_init(sc, &sc->axe_cdata,
	    sc->axe_udev) == ENOBUFS) {
		device_printf(sc->axe_dev, "tx list init failed\n");
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		return;
	}

	/* Init RX ring. */
	if (usb_ether_rx_list_init(sc, &sc->axe_cdata,
	    sc->axe_udev) == ENOBUFS) {
		device_printf(sc->axe_dev, "rx list init failed\n");
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		return;
	}

	/* Set transmitter IPG values */
	if (sc->axe_flags & (AX178|AX772)) {
		axe_cmd(sc, AXE_178_CMD_WRITE_IPG012, sc->axe_ipgs[2],
		    (sc->axe_ipgs[1]<<8) | sc->axe_ipgs[0], NULL);
	} else {
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG0, 0, sc->axe_ipgs[0], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG1, 0, sc->axe_ipgs[1], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG2, 0, sc->axe_ipgs[2], NULL);
	}

	/* Enable receiver, set RX mode */
	rxmode = AXE_RXCMD_MULTICAST|AXE_RXCMD_ENABLE;
	if (sc->axe_flags & (AX178|AX772)) {
		if (sc->axe_bufsz == AXE_178_MAX_BUFSZ)
			rxmode |= AXE_178_RXCMD_MFB_16384;
	} else
		rxmode |= AXE_172_RXCMD_UNICAST;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxmode |= AXE_RXCMD_PROMISC;

	if (ifp->if_flags & IFF_BROADCAST)
		rxmode |= AXE_RXCMD_BROADCAST;

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	/* Load the multicast filter. */
	axe_setmulti(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_RX]);
	if (err) {
		device_printf(sc->axe_dev, "open rx pipe failed: %s\n",
		    usbd_errstr(err));
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		return;
	}

	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_TX]);
	if (err) {
		device_printf(sc->axe_dev, "open tx pipe failed: %s\n",
		    usbd_errstr(err));
		AXE_UNLOCK(sc);
		AXE_SLEEPUNLOCK(sc);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		c = &sc->axe_cdata.ue_rx_chain[i];
		usbd_setup_xfer(c->ue_xfer, sc->axe_ep[AXE_ENDPT_RX],
		    c, mtod(c->ue_mbuf, char *), UE_BUFSZ,
		    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, axe_rxeof);
		usbd_transfer(c->ue_xfer);
	}

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	AXE_UNLOCK(sc);
	AXE_SLEEPUNLOCK(sc);

	sc->axe_stat_ch = timeout(axe_tick, sc, hz);

	return;
}

static int
axe_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct axe_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct mii_data		*mii;
	u_int16_t		rxmode;
	int			error = 0;

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->axe_if_flags & IFF_PROMISC)) {
				AXE_SLEEPLOCK(sc);
				AXE_LOCK(sc);
				axe_cmd(sc, AXE_CMD_RXCTL_READ,
					0, 0, (void *)&rxmode);
				rxmode |= AXE_RXCMD_PROMISC;
				axe_cmd(sc, AXE_CMD_RXCTL_WRITE,
					0, rxmode, NULL);
				AXE_UNLOCK(sc);
				axe_setmulti(sc);
				AXE_SLEEPUNLOCK(sc);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->axe_if_flags & IFF_PROMISC) {
				AXE_SLEEPLOCK(sc);
				AXE_LOCK(sc);
				axe_cmd(sc, AXE_CMD_RXCTL_READ,
					0, 0, (void *)&rxmode);
				rxmode &= ~AXE_RXCMD_PROMISC;
				axe_cmd(sc, AXE_CMD_RXCTL_WRITE,
					0, rxmode, NULL);
				AXE_UNLOCK(sc);
				axe_setmulti(sc);
				AXE_SLEEPUNLOCK(sc);
			} else if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				axe_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				AXE_SLEEPLOCK(sc);
				axe_stop(sc);
				AXE_SLEEPUNLOCK(sc);
			}
		}
		sc->axe_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		AXE_SLEEPLOCK(sc);
		axe_setmulti(sc);
		AXE_SLEEPUNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		AXE_SLEEPLOCK(sc);
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		AXE_SLEEPUNLOCK(sc);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return(error);
}

static void
axe_watchdog(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct ue_chain	*c;
	usbd_status		stat;

	sc = ifp->if_softc;
	AXE_LOCK(sc);

	ifp->if_oerrors++;
	device_printf(sc->axe_dev, "watchdog timeout\n");

	c = &sc->axe_cdata.ue_tx_chain[0];
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &stat);
	axe_txeof(c->ue_xfer, c, stat);

	AXE_UNLOCK(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		axe_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
axe_stop(struct axe_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;

	AXE_SLEEPLOCKASSERT(sc);
	AXE_LOCK(sc);

	ifp = sc->axe_ifp;
	ifp->if_timer = 0;

	untimeout(axe_tick, sc, sc->axe_stat_ch);

	/* Stop transfers. */
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			device_printf(sc->axe_dev, "abort rx pipe failed: %s\n",
			    usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			device_printf(sc->axe_dev, "close rx pipe failed: %s\n",
			    usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_RX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			device_printf(sc->axe_dev, "abort tx pipe failed: %s\n",
			    usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			device_printf(sc->axe_dev, "close tx pipe failed: %s\n",
			    usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_TX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			device_printf(sc->axe_dev,
			    "abort intr pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			device_printf(sc->axe_dev,
			    "close intr pipe failed: %s\n", usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_INTR] = NULL;
	}

	axe_reset(sc);

	/* Free RX resources. */
	usb_ether_rx_list_free(&sc->axe_cdata);
	/* Free TX resources. */
	usb_ether_tx_list_free(&sc->axe_cdata);

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
        sc->axe_link = 0;
	AXE_UNLOCK(sc);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
axe_shutdown(device_t dev)
{
	struct axe_softc	*sc;

	sc = device_get_softc(dev);

	AXE_SLEEPLOCK(sc);
	axe_stop(sc);
	AXE_SLEEPUNLOCK(sc);

	return (0);
}
