/*-
 * Copyright (c) 2001-2003, Shunsuke Akiyama <akiyama@FreeBSD.org>.
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
/*-
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
 * RealTek RTL8150 USB to fast ethernet controller driver.
 * Datasheet is available from
 * ftp://ftp.realtek.com.tw/lancard/data_sheet/8150/.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <machine/bus.h>
#if __FreeBSD_version < 500000
#include <machine/clock.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"
#include <dev/usb/usb_ethersubr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/if_ruereg.h>

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#ifdef USB_DEBUG
Static int	ruedebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, rue, CTLFLAG_RW, 0, "USB rue");
SYSCTL_INT(_hw_usb_rue, OID_AUTO, debug, CTLFLAG_RW,
	   &ruedebug, 0, "rue debug level");

#define DPRINTFN(n, x)	do { \
				if (ruedebug > (n)) \
					logprintf x; \
			} while (0);
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x)	DPRINTFN(0, x)

/*
 * Various supported device vendors/products.
 */

Static struct rue_type rue_devs[] = {
	{ USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUAKTX },
	{ USB_VENDOR_REALTEK, USB_PRODUCT_REALTEK_USBKR100 },
	{ 0, 0 }
};

Static int rue_match(device_ptr_t);
Static int rue_attach(device_ptr_t);
Static int rue_detach(device_ptr_t);

Static int rue_encap(struct rue_softc *, struct mbuf *, int);
#ifdef RUE_INTR_PIPE
Static void rue_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
#endif
Static void rue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void rue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void rue_tick(void *);
Static void rue_rxstart(struct ifnet *);
Static void rue_start(struct ifnet *);
Static int rue_ioctl(struct ifnet *, u_long, caddr_t);
Static void rue_init(void *);
Static void rue_stop(struct rue_softc *);
Static void rue_watchdog(struct ifnet *);
Static void rue_shutdown(device_ptr_t);
Static int rue_ifmedia_upd(struct ifnet *);
Static void rue_ifmedia_sts(struct ifnet *, struct ifmediareq *);

Static int rue_miibus_readreg(device_ptr_t, int, int);
Static int rue_miibus_writereg(device_ptr_t, int, int, int);
Static void rue_miibus_statchg(device_ptr_t);

Static void rue_setmulti(struct rue_softc *);
Static void rue_reset(struct rue_softc *);

Static int rue_read_mem(struct rue_softc *, u_int16_t, void *, u_int16_t);
Static int rue_write_mem(struct rue_softc *, u_int16_t, void *, u_int16_t);
Static int rue_csr_read_1(struct rue_softc *, int);
Static int rue_csr_write_1(struct rue_softc *, int, u_int8_t);
Static int rue_csr_read_2(struct rue_softc *, int);
Static int rue_csr_write_2(struct rue_softc *, int, u_int16_t);
Static int rue_csr_write_4(struct rue_softc *, int, u_int32_t);

Static device_method_t rue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, rue_match),
	DEVMETHOD(device_attach, rue_attach),
	DEVMETHOD(device_detach, rue_detach),
	DEVMETHOD(device_shutdown, rue_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, rue_miibus_readreg),
	DEVMETHOD(miibus_writereg, rue_miibus_writereg),
	DEVMETHOD(miibus_statchg, rue_miibus_statchg),

	{ 0, 0 }
};

Static driver_t rue_driver = {
	"rue",
	rue_methods,
	sizeof(struct rue_softc)
};

Static devclass_t rue_devclass;

DRIVER_MODULE(rue, uhub, rue_driver, rue_devclass, usbd_driver_load, 0);
DRIVER_MODULE(miibus, rue, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(rue, usb, 1, 1, 1);
MODULE_DEPEND(rue, ether, 1, 1, 1);
MODULE_DEPEND(rue, miibus, 1, 1, 1);

#define RUE_SETBIT(sc, reg, x) \
	rue_csr_write_1(sc, reg, rue_csr_read_1(sc, reg) | (x))

#define RUE_CLRBIT(sc, reg, x) \
	rue_csr_write_1(sc, reg, rue_csr_read_1(sc, reg) & ~(x))

#define RUE_SETBIT_2(sc, reg, x) \
	rue_csr_write_2(sc, reg, rue_csr_read_2(sc, reg) | (x))

#define RUE_CLRBIT_2(sc, reg, x) \
	rue_csr_write_2(sc, reg, rue_csr_read_2(sc, reg) & ~(x))

Static int
rue_read_mem(struct rue_softc *sc, u_int16_t addr, void *buf, u_int16_t len)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->rue_dying)
		return (0);

	RUE_LOCK(sc);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	err = usbd_do_request(sc->rue_udev, &req, buf);

	RUE_UNLOCK(sc);

	if (err) {
		printf("rue%d: control pipe read failed: %s\n",
		       sc->rue_unit, usbd_errstr(err));
		return (-1);
	}

	return (0);
}

Static int
rue_write_mem(struct rue_softc *sc, u_int16_t addr, void *buf, u_int16_t len)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->rue_dying)
		return (0);

	RUE_LOCK(sc);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	err = usbd_do_request(sc->rue_udev, &req, buf);

	RUE_UNLOCK(sc);

	if (err) {
		printf("rue%d: control pipe write failed: %s\n",
		       sc->rue_unit, usbd_errstr(err));
		return (-1);
	}

	return (0);
}

Static int
rue_csr_read_1(struct rue_softc *sc, int reg)
{
	int		err;
	u_int8_t	val = 0;

	err = rue_read_mem(sc, reg, &val, 1);

	if (err)
		return (0);

	return (val);
}

Static int
rue_csr_read_2(struct rue_softc *sc, int reg)
{
	int		err;
	u_int16_t	val = 0;
	uWord		w;

	USETW(w, val);
	err = rue_read_mem(sc, reg, &w, 2);
	val = UGETW(w);

	if (err)
		return (0);

	return (val);
}

Static int
rue_csr_write_1(struct rue_softc *sc, int reg, u_int8_t val)
{
	int	err;

	err = rue_write_mem(sc, reg, &val, 1);

	if (err)
		return (-1);

	return (0);
}

Static int
rue_csr_write_2(struct rue_softc *sc, int reg, u_int16_t val)
{
	int	err;
	uWord	w;

	USETW(w, val);
	err = rue_write_mem(sc, reg, &w, 2);

	if (err)
		return (-1);

	return (0);
}

Static int
rue_csr_write_4(struct rue_softc *sc, int reg, u_int32_t val)
{
	int	err;
	uDWord	dw;

	USETDW(dw, val);
	err = rue_write_mem(sc, reg, &dw, 4);

	if (err)
		return (-1);

	return (0);
}

Static int
rue_miibus_readreg(device_ptr_t dev, int phy, int reg)
{
	struct rue_softc	*sc = USBGETSOFTC(dev);
	int			rval;
	int			ruereg;

	if (phy != 0)		/* RTL8150 supports PHY == 0, only */
		return (0);

	switch (reg) {
	case MII_BMCR:
		ruereg = RUE_BMCR;
		break;
	case MII_BMSR:
		ruereg = RUE_BMSR;
		break;
	case MII_ANAR:
		ruereg = RUE_ANAR;
		break;
	case MII_ANER:
		ruereg = RUE_AER;
		break;
	case MII_ANLPAR:
		ruereg = RUE_ANLP;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		return (0);
		break;
	default:
		if (RUE_REG_MIN <= reg && reg <= RUE_REG_MAX) {
			rval = rue_csr_read_1(sc, reg);
			return (rval);
		}
		printf("rue%d: bad phy register\n", sc->rue_unit);
		return (0);
	}

	rval = rue_csr_read_2(sc, ruereg);

	return (rval);
}

Static int
rue_miibus_writereg(device_ptr_t dev, int phy, int reg, int data)
{
	struct rue_softc	*sc = USBGETSOFTC(dev);
	int			ruereg;

	if (phy != 0)		/* RTL8150 supports PHY == 0, only */
		return (0);

	switch (reg) {
	case MII_BMCR:
		ruereg = RUE_BMCR;
		break;
	case MII_BMSR:
		ruereg = RUE_BMSR;
		break;
	case MII_ANAR:
		ruereg = RUE_ANAR;
		break;
	case MII_ANER:
		ruereg = RUE_AER;
		break;
	case MII_ANLPAR:
		ruereg = RUE_ANLP;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		return (0);
		break;
	default:
		if (RUE_REG_MIN <= reg && reg <= RUE_REG_MAX) {
			rue_csr_write_1(sc, reg, data);
			return (0);
		}
		printf("rue%d: bad phy register\n", sc->rue_unit);
		return (0);
	}
	rue_csr_write_2(sc, ruereg, data);

	return (0);
}

Static void
rue_miibus_statchg(device_ptr_t dev)
{
	/*
	 * When the code below is enabled the card starts doing weird
	 * things after link going from UP to DOWN and back UP.
	 *
	 * Looks like some of register writes below messes up PHY
	 * interface.
	 *
	 * No visible regressions were found after commenting this code
	 * out, so that disable it for good.
	 */
#if 0
	struct rue_softc	*sc = USBGETSOFTC(dev);
	struct mii_data		*mii = GET_MII(sc);
	int			bmcr;

	RUE_CLRBIT(sc, RUE_CR, (RUE_CR_RE | RUE_CR_TE));

	bmcr = rue_csr_read_2(sc, RUE_BMCR);

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX)
		bmcr |= RUE_BMCR_SPD_SET;
	else
		bmcr &= ~RUE_BMCR_SPD_SET;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		bmcr |= RUE_BMCR_DUPLEX;
	else
		bmcr &= ~RUE_BMCR_DUPLEX;

	rue_csr_write_2(sc, RUE_BMCR, bmcr);

	RUE_SETBIT(sc, RUE_CR, (RUE_CR_RE | RUE_CR_TE));
#endif
}

/*
 * Program the 64-bit multicast hash filter.
 */

Static void
rue_setmulti(struct rue_softc *sc)
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxcfg;
	int			mcnt = 0;

	ifp = sc->rue_ifp;

	rxcfg = rue_csr_read_2(sc, RUE_RCR);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxcfg |= (RUE_RCR_AAM | RUE_RCR_AAP);
		rxcfg &= ~RUE_RCR_AM;
		rue_csr_write_2(sc, RUE_RCR, rxcfg);
		rue_csr_write_4(sc, RUE_MAR0, 0xFFFFFFFF);
		rue_csr_write_4(sc, RUE_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	rue_csr_write_4(sc, RUE_MAR0, 0);
	rue_csr_write_4(sc, RUE_MAR4, 0);

	/* now program new ones */
	IF_ADDR_LOCK(ifp);
#if __FreeBSD_version >= 500000
	TAILQ_FOREACH (ifma, &ifp->if_multiaddrs, ifma_link)
#else
	LIST_FOREACH (ifma, &ifp->if_multiaddrs, ifma_link)
#endif
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}
	IF_ADDR_UNLOCK(ifp);

	if (mcnt)
		rxcfg |= RUE_RCR_AM;
	else
		rxcfg &= ~RUE_RCR_AM;

	rxcfg &= ~(RUE_RCR_AAM | RUE_RCR_AAP);

	rue_csr_write_2(sc, RUE_RCR, rxcfg);
	rue_csr_write_4(sc, RUE_MAR0, hashes[0]);
	rue_csr_write_4(sc, RUE_MAR4, hashes[1]);
}

Static void
rue_reset(struct rue_softc *sc)
{
	int	i;

	rue_csr_write_1(sc, RUE_CR, RUE_CR_SOFT_RST);

	for (i = 0; i < RUE_TIMEOUT; i++) {
		DELAY(500);
		if (!(rue_csr_read_1(sc, RUE_CR) & RUE_CR_SOFT_RST))
			break;
	}
	if (i == RUE_TIMEOUT)
		printf("rue%d: reset never completed!\n", sc->rue_unit);

	DELAY(10000);
}

/*
 * Probe for a RTL8150 chip.
 */

USB_MATCH(rue)
{
	USB_MATCH_START(rue, uaa);
	struct rue_type	*t;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	t = rue_devs;
	while (t->rue_vid) {
		if (uaa->vendor == t->rue_vid &&
		    uaa->product == t->rue_did) {
			return (UMATCH_VENDOR_PRODUCT);
		}
		t++;
	}

	return (UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */

USB_ATTACH(rue)
{
	USB_ATTACH_START(rue, sc, uaa);
	char				*devinfo;
	u_char				eaddr[ETHER_ADDR_LEN];
	struct ifnet			*ifp;
	usbd_interface_handle		iface;
	usbd_status			err;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int				i;
	struct rue_type			*t;

	devinfo = malloc(1024, M_USBDEV, M_WAITOK);

	bzero(sc, sizeof (struct rue_softc));
	usbd_devinfo(uaa->device, 0, devinfo);

	sc->rue_dev = self;
	sc->rue_udev = uaa->device;
	sc->rue_unit = device_get_unit(self);

	if (usbd_set_config_no(sc->rue_udev, RUE_CONFIG_NO, 0)) {
		printf("rue%d: getting interface handle failed\n",
			sc->rue_unit);
		goto error;
	}

	err = usbd_device2interface_handle(uaa->device, RUE_IFACE_IDX, &iface);
	if (err) {
		printf("rue%d: getting interface handle failed\n",
		       sc->rue_unit);
		goto error;
	}

	sc->rue_iface = iface;

	t = rue_devs;
	while (t->rue_vid) {
		if (uaa->vendor == t->rue_vid &&
		    uaa->product == t->rue_did) {
			sc->rue_info = t;
			break;
		}
		t++;
	}

	id = usbd_get_interface_descriptor(sc->rue_iface);

	usbd_devinfo(uaa->device, 0, devinfo);
	device_set_desc_copy(self, devinfo);
	printf("%s: %s\n", USBDEVNAME(self), devinfo);

	/* Find endpoints */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("rue%d: couldn't get ep %d\n", sc->rue_unit, i);
			goto error;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->rue_ed[RUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->rue_ed[RUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->rue_ed[RUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

#if __FreeBSD_version >= 500000
	mtx_init(&sc->rue_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
		 MTX_DEF | MTX_RECURSE);
#endif
	RUE_LOCK(sc);

	/* Reset the adapter */
	rue_reset(sc);

	/* Get station address from the EEPROM */
	err = rue_read_mem(sc, RUE_EEPROM_IDR0,
			   (caddr_t)&eaddr, ETHER_ADDR_LEN);
	if (err) {
		printf("rue%d: couldn't get station address\n", sc->rue_unit);
		goto error1;
	}

	ifp = sc->rue_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("rue%d: can not if_alloc()\n", sc->rue_unit);
		goto error1;
	}
	ifp->if_softc = sc;
	if_initname(ifp, "rue", sc->rue_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_ioctl = rue_ioctl;
	ifp->if_start = rue_start;
	ifp->if_watchdog = rue_watchdog;
	ifp->if_init = rue_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	/* MII setup */
	if (mii_phy_probe(self, &sc->rue_miibus,
			  rue_ifmedia_upd, rue_ifmedia_sts)) {
		printf("rue%d: MII without any PHY!\n", sc->rue_unit);
		goto error2;
	}

	sc->rue_qdat.ifp = ifp;
	sc->rue_qdat.if_rxstart = rue_rxstart;

	/* Call MI attach routine */
#if __FreeBSD_version >= 500000
	ether_ifattach(ifp, eaddr);
#else
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
#endif
	callout_handle_init(&sc->rue_stat_ch);
	usb_register_netisr();
	sc->rue_dying = 0;

	RUE_UNLOCK(sc);
	free(devinfo, M_USBDEV);
	USB_ATTACH_SUCCESS_RETURN;

    error2:
	if_free(ifp);
    error1:
	RUE_UNLOCK(sc);
#if __FreeBSD_version >= 500000
	mtx_destroy(&sc->rue_mtx);
#endif
    error:
	free(devinfo, M_USBDEV);
	USB_ATTACH_ERROR_RETURN;
}

Static int
rue_detach(device_ptr_t dev)
{
	struct rue_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	RUE_LOCK(sc);
	ifp = sc->rue_ifp;

	sc->rue_dying = 1;
	untimeout(rue_tick, sc, sc->rue_stat_ch);
#if __FreeBSD_version >= 500000
	ether_ifdetach(ifp);
#else
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
#endif

	if (sc->rue_ep[RUE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->rue_ep[RUE_ENDPT_TX]);
	if (sc->rue_ep[RUE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->rue_ep[RUE_ENDPT_RX]);
#ifdef RUE_INTR_PIPE
	if (sc->rue_ep[RUE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->rue_ep[RUE_ENDPT_INTR]);
#endif

	RUE_UNLOCK(sc);
#if __FreeBSD_version >= 500000
	mtx_destroy(&sc->rue_mtx);
#endif

	return (0);
}

#ifdef RUE_INTR_PIPE
Static void
rue_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct rue_softc	*sc = priv;
	struct ifnet		*ifp;
	struct rue_intrpkt	*p;

	RUE_LOCK(sc);
	ifp = sc->rue_ifp;

	if (!(ifp->if_flags & IFF_RUNNING)) {
		RUE_UNLOCK(sc);
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			RUE_UNLOCK(sc);
			return;
		}
		printf("rue%d: usb error on intr: %s\n", sc->rue_unit,
			usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->rue_ep[RUE_ENDPT_INTR]);
		RUE_UNLOCK(sc);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&p, NULL, NULL);

	ifp->if_ierrors += p->rue_rxlost_cnt;
	ifp->if_ierrors += p->rue_crcerr_cnt;
	ifp->if_collisions += p->rue_col_cnt;

	RUE_UNLOCK(sc);
}
#endif

Static void
rue_rxstart(struct ifnet *ifp)
{
	struct rue_softc	*sc;
	struct ue_chain	*c;

	sc = ifp->if_softc;
	RUE_LOCK(sc);
	c = &sc->rue_cdata.ue_rx_chain[sc->rue_cdata.ue_rx_prod];

	c->ue_mbuf = usb_ether_newbuf();
	if (c->ue_mbuf == NULL) {
		printf("%s: no memory for rx list "
		    "-- packet dropped!\n", USBDEVNAME(sc->rue_dev));
		ifp->if_ierrors++;
		RUE_UNLOCK(sc);
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->rue_ep[RUE_ENDPT_RX],
		c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
		USBD_NO_TIMEOUT, rue_rxeof);
	usbd_transfer(c->ue_xfer);

	RUE_UNLOCK(sc);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */

Static void
rue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain	*c = priv;
	struct rue_softc	*sc = c->ue_sc;
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			total_len = 0;
	struct rue_rxpkt	r;

	if (sc->rue_dying)
		return;
	RUE_LOCK(sc);
	ifp = sc->rue_ifp;

	if (!(ifp->if_flags & IFF_RUNNING)) {
		RUE_UNLOCK(sc);
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			RUE_UNLOCK(sc);
			return;
		}
		if (usbd_ratecheck(&sc->rue_rx_notice))
			printf("rue%d: usb error on rx: %s\n", sc->rue_unit,
			       usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->rue_ep[RUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	if (total_len <= ETHER_CRC_LEN) {
		ifp->if_ierrors++;
		goto done;
	}

	m = c->ue_mbuf;
	bcopy(mtod(m, char *) + total_len - 4, (char *)&r, sizeof (r));

	/* Check recieve packet was valid or not */
	if ((r.rue_rxstat & RUE_RXSTAT_VALID) == 0) {
		ifp->if_ierrors++;
		goto done;
	}

	/* No errors; receive the packet. */
	total_len -= ETHER_CRC_LEN;

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = (void *)&sc->rue_qdat;
	m->m_pkthdr.len = m->m_len = total_len;

	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);

	RUE_UNLOCK(sc);
	return;

    done:
	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->rue_ep[RUE_ENDPT_RX],
			c, mtod(c->ue_mbuf, char *), UE_BUFSZ,
			USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rue_rxeof);
	usbd_transfer(xfer);
	RUE_UNLOCK(sc);
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
rue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain	*c = priv;
	struct rue_softc	*sc = c->ue_sc;
	struct ifnet		*ifp;
	usbd_status		err;

	RUE_LOCK(sc);

	ifp = sc->rue_ifp;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			RUE_UNLOCK(sc);
			return;
		}
		printf("rue%d: usb error on tx: %s\n", sc->rue_unit,
			usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->rue_ep[RUE_ENDPT_TX]);
		RUE_UNLOCK(sc);
		return;
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
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

	RUE_UNLOCK(sc);
}

Static void
rue_tick(void *xsc)
{
	struct rue_softc	*sc = xsc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	if (sc == NULL)
		return;

	RUE_LOCK(sc);

	ifp = sc->rue_ifp;
	mii = GET_MII(sc);
	if (mii == NULL) {
		RUE_UNLOCK(sc);
		return;
	}

	mii_tick(mii);
	if (!sc->rue_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->rue_link++;
		if (ifp->if_snd.ifq_head != NULL)
			rue_start(ifp);
	}

	sc->rue_stat_ch = timeout(rue_tick, sc, hz);

	RUE_UNLOCK(sc);
}

Static int
rue_encap(struct rue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct ue_chain	*c;
	usbd_status		err;

	c = &sc->rue_cdata.ue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->ue_buf);
	c->ue_mbuf = m;

	total_len = m->m_pkthdr.len;

	/*
	 * This is an undocumented behavior.
	 * RTL8150 chip doesn't send frame length smaller than
	 * RUE_MIN_FRAMELEN (60) byte packet.
	 */
	if (total_len < RUE_MIN_FRAMELEN)
		total_len = RUE_MIN_FRAMELEN;

	usbd_setup_xfer(c->ue_xfer, sc->rue_ep[RUE_ENDPT_TX],
			c, c->ue_buf, total_len, USBD_FORCE_SHORT_XFER,
			10000, rue_txeof);

	/* Transmit */
	err = usbd_transfer(c->ue_xfer);
	if (err != USBD_IN_PROGRESS) {
		rue_stop(sc);
		return (EIO);
	}

	sc->rue_cdata.ue_tx_cnt++;

	return (0);
}

Static void
rue_start(struct ifnet *ifp)
{
	struct rue_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	RUE_LOCK(sc);

	if (!sc->rue_link) {
		RUE_UNLOCK(sc);
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
		RUE_UNLOCK(sc);
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		RUE_UNLOCK(sc);
		return;
	}

	if (rue_encap(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_flags |= IFF_OACTIVE;
		RUE_UNLOCK(sc);
		return;
	}

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	BPF_MTAP(ifp, m_head);

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	RUE_UNLOCK(sc);
}

Static void
rue_init(void *xsc)
{
	struct rue_softc	*sc = xsc;
	struct ifnet		*ifp = sc->rue_ifp;
	struct mii_data		*mii = GET_MII(sc);
	struct ue_chain	*c;
	usbd_status		err;
	int			i;
	int			rxcfg;

	RUE_LOCK(sc);

	if (ifp->if_flags & IFF_RUNNING) {
		RUE_UNLOCK(sc);
		return;
	}

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	rue_reset(sc);

	/* Set MAC address */
	rue_write_mem(sc, RUE_IDR0, IFP2ENADDR(sc->rue_ifp),
	    ETHER_ADDR_LEN);

	/* Init TX ring. */
	if (usb_ether_tx_list_init(sc, &sc->rue_cdata,
	    sc->rue_udev) == ENOBUFS) {
		printf("rue%d: tx list init failed\n", sc->rue_unit);
		RUE_UNLOCK(sc);
		return;
	}

	/* Init RX ring. */
	if (usb_ether_rx_list_init(sc, &sc->rue_cdata,
	    sc->rue_udev) == ENOBUFS) {
		printf("rue%d: rx list init failed\n", sc->rue_unit);
		RUE_UNLOCK(sc);
		return;
	}

#ifdef RUE_INTR_PIPE
	sc->rue_cdata.ue_ibuf = malloc(RUE_INTR_PKTLEN, M_USBDEV, M_NOWAIT);
#endif

	/*
	 * Set the initial TX and RX configuration.
	 */
	rue_csr_write_1(sc, RUE_TCR, RUE_TCR_CONFIG);

	rxcfg = RUE_RCR_CONFIG;

	/* Set capture broadcast bit to capture broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST)
		rxcfg |= RUE_RCR_AB;
	else
		rxcfg &= ~RUE_RCR_AB;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxcfg |= RUE_RCR_AAP;
	else
		rxcfg &= ~RUE_RCR_AAP;

	rue_csr_write_2(sc, RUE_RCR, rxcfg);

	/* Load the multicast filter. */
	rue_setmulti(sc);

	/* Enable RX and TX */
	rue_csr_write_1(sc, RUE_CR, (RUE_CR_TE | RUE_CR_RE | RUE_CR_EP3CLREN));

	mii_mediachg(mii);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->rue_iface, sc->rue_ed[RUE_ENDPT_RX],
			     USBD_EXCLUSIVE_USE, &sc->rue_ep[RUE_ENDPT_RX]);
	if (err) {
		printf("rue%d: open rx pipe failed: %s\n",
			sc->rue_unit, usbd_errstr(err));
		RUE_UNLOCK(sc);
		return;
	}
	err = usbd_open_pipe(sc->rue_iface, sc->rue_ed[RUE_ENDPT_TX],
			     USBD_EXCLUSIVE_USE, &sc->rue_ep[RUE_ENDPT_TX]);
	if (err) {
		printf("rue%d: open tx pipe failed: %s\n",
			sc->rue_unit, usbd_errstr(err));
		RUE_UNLOCK(sc);
		return;
	}

#ifdef RUE_INTR_PIPE
	err = usbd_open_pipe_intr(sc->rue_iface, sc->rue_ed[RUE_ENDPT_INTR],
				  USBD_SHORT_XFER_OK,
				  &sc->rue_ep[RUE_ENDPT_INTR], sc,
				  sc->rue_cdata.ue_ibuf, RUE_INTR_PKTLEN,
				  rue_intr, RUE_INTR_INTERVAL);
	if (err) {
		printf("rue%d: open intr pipe failed: %s\n",
			sc->rue_unit, usbd_errstr(err));
		RUE_UNLOCK(sc);
		return;
	}
#endif

	/* Start up the receive pipe. */
	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		c = &sc->rue_cdata.ue_rx_chain[i];
		usbd_setup_xfer(c->ue_xfer, sc->rue_ep[RUE_ENDPT_RX],
				c, mtod(c->ue_mbuf, char *), UE_BUFSZ,
				USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rue_rxeof);
		usbd_transfer(c->ue_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->rue_stat_ch = timeout(rue_tick, sc, hz);

	RUE_UNLOCK(sc);
}

/*
 * Set media options.
 */

Static int
rue_ifmedia_upd(struct ifnet *ifp)
{
	struct rue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	sc->rue_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		LIST_FOREACH (miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return (0);
}

/*
 * Report current media status.
 */

Static void
rue_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct rue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

Static int
rue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct rue_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct mii_data		*mii;
	int			error = 0;

	RUE_LOCK(sc);

	switch (command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->rue_if_flags & IFF_PROMISC)) {
				RUE_SETBIT_2(sc, RUE_RCR,
					     (RUE_RCR_AAM | RUE_RCR_AAP));
				rue_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
				   !(ifp->if_flags & IFF_PROMISC) &&
				   sc->rue_if_flags & IFF_PROMISC) {
				RUE_CLRBIT_2(sc, RUE_RCR,
					     (RUE_RCR_AAM | RUE_RCR_AAP));
				rue_setmulti(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				rue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rue_stop(sc);
		}
		sc->rue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		rue_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	RUE_UNLOCK(sc);

	return (error);
}

Static void
rue_watchdog(struct ifnet *ifp)
{
	struct rue_softc	*sc = ifp->if_softc;
	struct ue_chain	*c;
	usbd_status		stat;

	RUE_LOCK(sc);

	ifp->if_oerrors++;
	printf("rue%d: watchdog timeout\n", sc->rue_unit);

	c = &sc->rue_cdata.ue_tx_chain[0];
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &stat);
	rue_txeof(c->ue_xfer, c, stat);

	if (ifp->if_snd.ifq_head != NULL)
		rue_start(ifp);

	RUE_UNLOCK(sc);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */

Static void
rue_stop(struct rue_softc *sc)
{
	usbd_status	err;
	struct ifnet	*ifp;

	RUE_LOCK(sc);

	ifp = sc->rue_ifp;
	ifp->if_timer = 0;

	rue_csr_write_1(sc, RUE_CR, 0x00);
	rue_reset(sc);

	untimeout(rue_tick, sc, sc->rue_stat_ch);

	/* Stop transfers. */
	if (sc->rue_ep[RUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->rue_ep[RUE_ENDPT_RX]);
		if (err) {
			printf("rue%d: abort rx pipe failed: %s\n",
			       sc->rue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->rue_ep[RUE_ENDPT_RX]);
		if (err) {
			printf("rue%d: close rx pipe failed: %s\n",
			       sc->rue_unit, usbd_errstr(err));
		}
		sc->rue_ep[RUE_ENDPT_RX] = NULL;
	}

	if (sc->rue_ep[RUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->rue_ep[RUE_ENDPT_TX]);
		if (err) {
			printf("rue%d: abort tx pipe failed: %s\n",
			       sc->rue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->rue_ep[RUE_ENDPT_TX]);
		if (err) {
			printf("rue%d: close tx pipe failed: %s\n",
			       sc->rue_unit, usbd_errstr(err));
		}
		sc->rue_ep[RUE_ENDPT_TX] = NULL;
	}

#ifdef RUE_INTR_PIPE
	if (sc->rue_ep[RUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->rue_ep[RUE_ENDPT_INTR]);
		if (err) {
			printf("rue%d: abort intr pipe failed: %s\n",
			       sc->rue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->rue_ep[RUE_ENDPT_INTR]);
		if (err) {
			printf("rue%d: close intr pipe failed: %s\n",
			       sc->rue_unit, usbd_errstr(err));
		}
		sc->rue_ep[RUE_ENDPT_INTR] = NULL;
	}
#endif

	/* Free RX resources. */
	usb_ether_rx_list_free(&sc->rue_cdata);
	/* Free TX resources. */
	usb_ether_tx_list_free(&sc->rue_cdata);

#ifdef RUE_INTR_PIPE
	free(sc->rue_cdata.ue_ibuf, M_USBDEV);
	sc->rue_cdata.ue_ibuf = NULL;
#endif

	sc->rue_link = 0;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	RUE_UNLOCK(sc);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */

Static void
rue_shutdown(device_ptr_t dev)
{
	struct rue_softc	*sc;

	sc = device_get_softc(dev);

	sc->rue_dying++;
	RUE_LOCK(sc);
	rue_reset(sc);
	rue_stop(sc);
	RUE_UNLOCK(sc);
}
