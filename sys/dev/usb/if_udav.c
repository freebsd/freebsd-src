/*	$NetBSD: if_udav.c,v 1.2 2003/09/04 15:17:38 tsutsui Exp $	*/
/*	$nabe: if_udav.c,v 1.3 2003/08/21 16:57:19 nabe Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/if_udav.c,v 1.33.6.1 2008/11/25 02:59:29 kensmith Exp $	*/
/*-
 * Copyright (c) 2003
 *     Shingo WATANABE <nabe@nabechan.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 */

/*
 * DM9601(DAVICOM USB to Ethernet MAC Controller with Integrated 10/100 PHY)
 * The spec can be found at the following url.
 *   http://www.davicom.com.tw/big5/download/Data%20Sheet/DM9601-DS-P01-930914.pdf
 */

/*
 * TODO:
 *	Interrupt Endpoint support
 *	External PHYs
 *	powerhook() support?
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/usb/if_udav.c,v 1.33.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_inet.h"
#if defined(__NetBSD__)
#include "opt_ns.h"
#endif
#if defined(__NetBSD__)
#include "bpfilter.h"
#endif
#if defined(__FreeBSD__)
#define NBPFILTER	1
#endif
#if defined(__NetBSD__)
#include "rnd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/lockmgr.h>
#include <sys/sockio.h>
#endif

#if defined(__NetBSD__)
#include <sys/device.h>
#endif

#if defined(NRND) && NRND > 0
#include <sys/rnd.h>
#endif

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#if defined(__NetBSD__)
#ifndef BPF_MTAP
#define	BPF_MTAP(_ifp, _m)	do {			\
	if ((_ifp)->if_bpf)) {				\
		bpf_mtap((_ifp)->if_bpf, (_m)) ;	\
	}						\
} while (0)
#endif
#endif

#if defined(__NetBSD__)
#include <net/if_ether.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif /* INET */
#elif defined(__FreeBSD__) /* defined(__NetBSD__) */
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif /* defined(__FreeBSD__) */

#if defined(__NetBSD__)
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif
#endif /* defined (__NetBSD__) */

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb_port.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_ethersubr.h>

#include <dev/usb/if_udavreg.h>

#if defined(__FreeBSD__)
MODULE_DEPEND(udav, usb, 1, 1, 1);
MODULE_DEPEND(udav, ether, 1, 1, 1);
MODULE_DEPEND(udav, miibus, 1, 1, 1);
#endif

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#if !defined(__FreeBSD__)
/* Function declarations */
USB_DECLARE_DRIVER(udav);
#endif

#if defined(__FreeBSD__)
static device_probe_t udav_match;
static device_attach_t udav_attach;
static device_detach_t udav_detach;
static device_shutdown_t udav_shutdown;
static miibus_readreg_t udav_miibus_readreg;
static miibus_writereg_t udav_miibus_writereg;
static miibus_statchg_t udav_miibus_statchg;
#endif

static int udav_openpipes(struct udav_softc *);
static void udav_start(struct ifnet *);
static int udav_send(struct udav_softc *, struct mbuf *, int);
static void udav_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
#if defined(__FreeBSD__)
static void udav_rxstart(struct ifnet *ifp);
#endif
static void udav_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void udav_tick(void *);
static void udav_tick_task(void *);
static int udav_ioctl(struct ifnet *, u_long, caddr_t);
static void udav_stop_task(struct udav_softc *);
static void udav_stop(struct ifnet *, int);
static void udav_watchdog(struct ifnet *);
static int udav_ifmedia_change(struct ifnet *);
static void udav_ifmedia_status(struct ifnet *, struct ifmediareq *);
static void udav_lock_mii(struct udav_softc *);
static void udav_unlock_mii(struct udav_softc *);
#if defined(__NetBSD__)
static int udav_miibus_readreg(device_t, int, int);
static void udav_miibus_writereg(device_t, int, int, int);
static void udav_miibus_statchg(device_t);
static int udav_init(struct ifnet *);
#elif defined(__FreeBSD__)
static void udav_init(void *);
#endif
static void udav_setmulti(struct udav_softc *);
static void udav_reset(struct udav_softc *);

static int udav_csr_read(struct udav_softc *, int, void *, int);
static int udav_csr_write(struct udav_softc *, int, void *, int);
static int udav_csr_read1(struct udav_softc *, int);
static int udav_csr_write1(struct udav_softc *, int, unsigned char);

#if 0
static int udav_mem_read(struct udav_softc *, int, void *, int);
static int udav_mem_write(struct udav_softc *, int, void *, int);
static int udav_mem_write1(struct udav_softc *, int, unsigned char);
#endif

#if defined(__FreeBSD__)
static device_method_t udav_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		udav_match),
	DEVMETHOD(device_attach,	udav_attach),
	DEVMETHOD(device_detach,	udav_detach),
	DEVMETHOD(device_shutdown,	udav_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	udav_miibus_readreg),
	DEVMETHOD(miibus_writereg,	udav_miibus_writereg),
	DEVMETHOD(miibus_statchg,	udav_miibus_statchg),

	{ 0, 0 }
};

static driver_t udav_driver = {
	"udav",
	udav_methods,
	sizeof(struct udav_softc)
};

static devclass_t udav_devclass;

DRIVER_MODULE(udav, uhub, udav_driver, udav_devclass, usbd_driver_load, 0);
DRIVER_MODULE(miibus, udav, miibus_driver, miibus_devclass, 0, 0);

#endif /* defined(__FreeBSD__) */

/* Macros */
#ifdef UDAV_DEBUG
#define DPRINTF(x)	if (udavdebug) printf x
#define DPRINTFN(n,x)	if (udavdebug >= (n)) printf x
int udavdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define delay(d)	DELAY(d)

#define	UDAV_SETBIT(sc, reg, x)	\
	udav_csr_write1(sc, reg, udav_csr_read1(sc, reg) | (x))

#define	UDAV_CLRBIT(sc, reg, x)	\
	udav_csr_write1(sc, reg, udav_csr_read1(sc, reg) & ~(x))

static const struct udav_type {
	struct usb_devno udav_dev;
	u_int16_t udav_flags;
#define UDAV_EXT_PHY	0x0001
} udav_devs [] = {
	/* Corega USB-TXC */
	{{ USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TXC }, 0},
	/* ShanTou ST268 USB NIC */
	{{ USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_ST268 }, 0},
#if 0
	/* DAVICOM DM9601 Generic? */
	/*  XXX: The following ids was obtained from the data sheet. */
	{{ 0x0a46, 0x9601 }, 0},
#endif
};
#define udav_lookup(v, p) ((const struct udav_type *)usb_lookup(udav_devs, v, p))


/* Probe */
static int
udav_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (udav_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/* Attach */
static int
udav_attach(device_t self)
{
	USB_ATTACH_START(udav, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usbd_status err;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	const char *devname ;
	struct ifnet *ifp;
#if defined(__NetBSD__)
	struct mii_data *mii;
#endif
	u_char eaddr[ETHER_ADDR_LEN];
	int i;
#if defined(__NetBSD__)
	int s;
#endif

	sc->sc_dev = self;
	devname = device_get_nameunit(self);
	/* Move the device into the configured state. */
	err = usbd_set_config_no(dev, UDAV_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed\n", devname);
		goto bad;
	}

	usb_init_task(&sc->sc_tick_task, udav_tick_task, sc);
	lockinit(&sc->sc_mii_lock, PZERO, "udavmii", 0, 0);
	usb_init_task(&sc->sc_stop_task, (void (*)(void *)) udav_stop_task, sc);

	/* get control interface */
	err = usbd_device2interface_handle(dev, UDAV_IFACE_INDEX, &iface);
	if (err) {
		printf("%s: failed to get interface, err=%s\n", devname,
		       usbd_errstr(err));
		goto bad;
	}

	sc->sc_udev = dev;
	sc->sc_ctl_iface = iface;
	sc->sc_flags = udav_lookup(uaa->vendor, uaa->product)->udav_flags;

	/* get interface descriptor */
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);

	/* find endpoints */
	sc->sc_bulkin_no = sc->sc_bulkout_no = sc->sc_intrin_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_ctl_iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get endpoint %d\n", devname, i);
			goto bad;
		}
		if ((ed->bmAttributes & UE_XFERTYPE) == UE_BULK &&
		    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
			sc->sc_bulkin_no = ed->bEndpointAddress; /* RX */
		else if ((ed->bmAttributes & UE_XFERTYPE) == UE_BULK &&
			 UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT)
			sc->sc_bulkout_no = ed->bEndpointAddress; /* TX */
		else if ((ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT &&
			 UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
			sc->sc_intrin_no = ed->bEndpointAddress; /* Status */
	}

	if (sc->sc_bulkin_no == -1 || sc->sc_bulkout_no == -1 ||
	    sc->sc_intrin_no == -1) {
		printf("%s: missing endpoint\n", devname);
		goto bad;
	}

#if defined(__FreeBSD__)
	mtx_init(&sc->sc_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
#endif
#if defined(__NetBSD__)
	s = splnet();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

	/* reset the adapter */
	udav_reset(sc);

	/* Get Ethernet Address */
	err = udav_csr_read(sc, UDAV_PAR, (void *)eaddr, ETHER_ADDR_LEN);
	if (err) {
		printf("%s: read MAC address failed\n", devname);
#if defined(__NetBSD__)
		splx(s);
#elif defined(__FreeBSD__)
                UDAV_UNLOCK(sc);
		mtx_destroy(&sc->sc_mtx);
#endif
		goto bad;
	}

	/* Print Ethernet Address */
	printf("%s: Ethernet address %s\n", devname, ether_sprintf(eaddr));

	/* initialize interface infomation */
#if defined(__FreeBSD__)
	ifp = GET_IFP(sc) = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("%s: can not if_alloc\n", devname);
                UDAV_UNLOCK(sc);
		mtx_destroy(&sc->sc_mtx);
		goto bad;
	}
#else
	ifp = GET_IFP(sc);
#endif
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
#if defined(__NetBSD__)
	strncpy(ifp->if_xname, devname, IFNAMSIZ);
#elif defined(__FreeBSD__)
	if_initname(ifp, "udav",  device_get_unit(self));
#endif
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_start = udav_start;
	ifp->if_ioctl = udav_ioctl;
	ifp->if_watchdog = udav_watchdog;
	ifp->if_init = udav_init;
#if defined(__NetBSD__)
	ifp->if_stop = udav_stop;
#endif
#if defined(__FreeBSD__)
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#endif
#if defined(__NetBSD__)
	IFQ_SET_READY(&ifp->if_snd);
#endif


#if defined(__NetBSD__)
	/*
	 * Do ifmedia setup.
	 */
	mii = &sc->sc_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = udav_miibus_readreg;
	mii->mii_writereg = udav_miibus_writereg;
	mii->mii_statchg = udav_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;
	ifmedia_init(&mii->mii_media, 0,
		     udav_ifmedia_change, udav_ifmedia_status);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	/* attach the interface */
	if_attach(ifp);
	Ether_ifattach(ifp, eaddr);
#elif defined(__FreeBSD__)
	if (mii_phy_probe(self, &sc->sc_miibus,
	    udav_ifmedia_change, udav_ifmedia_status)) {
		printf("%s: MII without any PHY!\n", device_get_nameunit(sc->sc_dev));
		if_free(ifp);
		UDAV_UNLOCK(sc);
		mtx_destroy(&sc->sc_mtx);
		return ENXIO;
	}

	sc->sc_qdat.ifp = ifp;
	sc->sc_qdat.if_rxstart = udav_rxstart;

	/*
	 * Call MI attach routine.
	 */

	ether_ifattach(ifp, eaddr);
#endif

#if defined(NRND) && NRND > 0
	rnd_attach_source(&sc->rnd_source, devname, RND_TYPE_NET, 0);
#endif

	usb_callout_init(sc->sc_stat_ch);
#if defined(__FreeBSD__)
	usb_register_netisr();
#endif
	sc->sc_attached = 1;
#if defined(__NetBSD__)
        splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, dev, sc->sc_dev);

	return 0;

 bad:
	sc->sc_dying = 1;
	return ENXIO;
}

/* detach */
static int
udav_detach(device_t self)
{
	USB_DETACH_START(udav, sc);
	struct ifnet *ifp = GET_IFP(sc);
#if defined(__NetBSD__)
	int s;
#endif

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	/* Detached before attached finished */
	if (!sc->sc_attached)
		return (0);

	UDAV_LOCK(sc);

	usb_uncallout(sc->sc_stat_ch, udav_tick, sc);

	/* Remove any pending tasks */
	usb_rem_task(sc->sc_udev, &sc->sc_tick_task);
	usb_rem_task(sc->sc_udev, &sc->sc_stop_task);

#if defined(__NetBSD__)
	s = splusb();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away */
		usb_detach_wait(sc->sc_dev);
	}
#if defined(__FreeBSD__)
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
#else
	if (ifp->if_flags & IFF_RUNNING)
#endif
		udav_stop(GET_IFP(sc), 1);

#if defined(NRND) && NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
#if defined(__NetBSD__)
	mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);
#endif
	ether_ifdetach(ifp);
#if defined(__NetBSD__)
	if_detach(ifp);
#endif
#if defined(__FreeBSD__)
	if_free(ifp);
#endif

#ifdef DIAGNOSTIC
	if (sc->sc_pipe_tx != NULL)
		printf("%s: detach has active tx endpoint.\n",
		       device_get_nameunit(sc->sc_dev));
	if (sc->sc_pipe_rx != NULL)
		printf("%s: detach has active rx endpoint.\n",
		       device_get_nameunit(sc->sc_dev));
	if (sc->sc_pipe_intr != NULL)
		printf("%s: detach has active intr endpoint.\n",
		       device_get_nameunit(sc->sc_dev));
#endif
	sc->sc_attached = 0;

#if defined(__NetBSD__)
        splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif

#if defined(__FreeBSD__)
	mtx_destroy(&sc->sc_mtx);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);
	return (0);
}

#if 0
/* read memory */
static int
udav_mem_read(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xffff;
	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: read failed. off=%04x, err=%d\n",
			 device_get_nameunit(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

/* write memory */
static int
udav_mem_write(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xffff;
	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_get_nameunit(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

/* write memory */
static int
udav_mem_write1(struct udav_softc *sc, int offset, unsigned char ch)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xffff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_get_nameunit(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}
#endif

/* read register(s) */
static int
udav_csr_read(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xff;
	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: read failed. off=%04x, err=%d\n",
			 device_get_nameunit(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

/* write register(s) */
static int
udav_csr_write(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xff;
	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_get_nameunit(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

static int
udav_csr_read1(struct udav_softc *sc, int offset)
{
	u_int8_t val = 0;
	
	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	return (udav_csr_read(sc, offset, &val, 1) ? 0 : val);
}

/* write a register */
static int
udav_csr_write1(struct udav_softc *sc, int offset, unsigned char ch)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_get_nameunit(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

#if defined(__NetBSD__)
static int
udav_init(struct ifnet *ifp)
#elif defined(__FreeBSD__)
static void
udav_init(void *xsc)
#endif
{
#if defined(__NetBSD__)
	struct udav_softc *sc = ifp->if_softc;
#elif defined(__FreeBSD__)
	struct udav_softc *sc = (struct udav_softc *)xsc;
	struct ifnet	*ifp = GET_IFP(sc);
#endif
	struct mii_data *mii = GET_MII(sc);
	u_char *eaddr;
#if defined(__NetBSD__)
	int s;
#endif

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
#if defined(__NetBSD__)
		return (EIO);
#elif defined(__FreeBSD__)
		return ;
#endif

#if defined(__NetBSD__)
	s = splnet();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

	/* Cancel pending I/O and free all TX/RX buffers */
	udav_stop(ifp, 1);

#if defined(__NetBSD__)
	eaddr = LLADDR(ifp->if_sadl);
#elif defined(__FreeBSD__)
	eaddr = IF_LLADDR(ifp);
#endif
	udav_csr_write(sc, UDAV_PAR, eaddr, ETHER_ADDR_LEN);

	/* Initialize network control register */
	/*  Disable loopback  */
	UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_LBK0 | UDAV_NCR_LBK1);

	/* Initialize RX control register */
	UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_DIS_LONG | UDAV_RCR_DIS_CRC);

	/* If we want promiscuous mode, accept all physical frames. */
	if (ifp->if_flags & IFF_PROMISC)
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);
	else
		UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);

	/* Initialize transmit ring */
	if (usb_ether_tx_list_init(sc, &sc->sc_cdata,
	    sc->sc_udev) == ENOBUFS) {
		printf("%s: tx list init failed\n", device_get_nameunit(sc->sc_dev));
#if defined(__NetBSD__)
		splx(s);
		return (EIO);
#elif defined(__FreeBSD__)
                UDAV_UNLOCK(sc);
		return ;
#endif

	}

	/* Initialize receive ring */
	if (usb_ether_rx_list_init(sc, &sc->sc_cdata,
	    sc->sc_udev) == ENOBUFS) {
		printf("%s: rx list init failed\n", device_get_nameunit(sc->sc_dev));
#if defined(__NetBSD__)
		splx(s);
		return (EIO);
#elif defined(__FreeBSD__)
                UDAV_UNLOCK(sc);
		return ;
#endif
	}

	/* Load the multicast filter */
	udav_setmulti(sc);

	/* Enable RX */
	UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_RXEN);

	/* clear POWER_DOWN state of internal PHY */
	UDAV_SETBIT(sc, UDAV_GPCR, UDAV_GPCR_GEP_CNTL0);
	UDAV_CLRBIT(sc, UDAV_GPR, UDAV_GPR_GEPIO0);

	mii_mediachg(mii);

	if (sc->sc_pipe_tx == NULL || sc->sc_pipe_rx == NULL) {
		if (udav_openpipes(sc)) {
#if defined(__NetBSD__)
                        splx(s);
                        return (EIO);
#elif defined(__FreeBSD__)
                        UDAV_UNLOCK(sc);
                        return ;
#endif
		}
	}

#if defined(__FreeBSD__)
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
#else
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
#endif

#if defined(__NetBSD__)
	splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif

	usb_callout(sc->sc_stat_ch, hz, udav_tick, sc);

#if defined(__NetBSD__)
	return (0);
#elif defined(__FreeBSD__)
	return ;
#endif
}

static void
udav_reset(struct udav_softc *sc)
{
	int i;

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return;

	/* Select PHY */
#if 1
	/*
	 * XXX: force select internal phy.
	 *	external phy routines are not tested.
	 */
	UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
#else
	if (sc->sc_flags & UDAV_EXT_PHY) {
		UDAV_SETBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	} else {
		UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	}
#endif

	UDAV_SETBIT(sc, UDAV_NCR, UDAV_NCR_RST);

	for (i = 0; i < UDAV_TX_TIMEOUT; i++) {
		if (!(udav_csr_read1(sc, UDAV_NCR) & UDAV_NCR_RST))
			break;
		delay(10);	/* XXX */
	}
	delay(10000);		/* XXX */
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
udav_activate(device_t self, enum devact act)
{
	struct udav_softc *sc = (struct udav_softc *)self;

	DPRINTF(("%s: %s: enter, act=%d\n", device_get_nameunit(sc->sc_dev),
		 __func__, act));
	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		sc->sc_dying = 1;
		break;
	}
	return (0);
}
#endif

#define UDAV_BITS	6

#define UDAV_CALCHASH(addr) \
	(ether_crc32_le((addr), ETHER_ADDR_LEN) & ((1 << UDAV_BITS) - 1))

static void
udav_setmulti(struct udav_softc *sc)
{
	struct ifnet *ifp;
#if defined(__NetBSD__)
	struct ether_multi *enm;
	struct ether_multistep step;
#elif defined(__FreeBSD__)
	struct ifmultiaddr *ifma;
#endif
	u_int8_t hashes[8];
	int h = 0;

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return;

	ifp = GET_IFP(sc);

	if (ifp->if_flags & IFF_PROMISC) {
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);
		return;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
#if defined(__NetBSD__)
	allmulti:
#endif
		ifp->if_flags |= IFF_ALLMULTI;
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL);
		UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_PRMSC);
		return;
	}

	/* first, zot all the existing hash bits */
	memset(hashes, 0x00, sizeof(hashes));
	hashes[7] |= 0x80;	/* broadcast address */
	udav_csr_write(sc, UDAV_MAR, hashes, sizeof(hashes));

	/* now program new ones */
#if defined(__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			   ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = UDAV_CALCHASH(enm->enm_addrlo);
		hashes[h>>3] |= 1 << (h & 0x7);
		ETHER_NEXT_MULTI(step, enm);
	}
#elif defined(__FreeBSD__)
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = UDAV_CALCHASH(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr));
		hashes[h>>3] |= 1 << (h & 0x7);
	}
	IF_ADDR_UNLOCK(ifp);
#endif

	/* disable all multicast */
	ifp->if_flags &= ~IFF_ALLMULTI;
	UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_ALL);

	/* write hash value to the register */
	udav_csr_write(sc, UDAV_MAR, hashes, sizeof(hashes));
}

static int
udav_openpipes(struct udav_softc *sc)
{
	struct ue_chain *c;
	usbd_status err;
	int i;
	int error = 0;

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt++;

	/* Open RX pipe */
	err = usbd_open_pipe(sc->sc_ctl_iface, sc->sc_bulkin_no,
			     USBD_EXCLUSIVE_USE, &sc->sc_pipe_rx);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		error = EIO;
		goto done;
	}

	/* Open TX pipe */
	err = usbd_open_pipe(sc->sc_ctl_iface, sc->sc_bulkout_no,
			     USBD_EXCLUSIVE_USE, &sc->sc_pipe_tx);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		error = EIO;
		goto done;
	}

#if 0
	/* XXX: interrupt endpoint is not yet supported */
	/* Open Interrupt pipe */
	err = usbd_open_pipe_intr(sc->sc_ctl_iface, sc->sc_intrin_no,
				  USBD_EXCLUSIVE_USE, &sc->sc_pipe_intr, sc,
				  &sc->sc_cdata.ue_ibuf, UDAV_INTR_PKGLEN,
				  udav_intr, UDAV_INTR_INTERVAL);
	if (err) {
		printf("%s: open intr pipe failed: %s\n",
		       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		error = EIO;
		goto done;
	}
#endif


	/* Start up the receive pipe. */
	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		c = &sc->sc_cdata.ue_rx_chain[i];
		usbd_setup_xfer(c->ue_xfer, sc->sc_pipe_rx,
				c, c->ue_buf, UE_BUFSZ,
				USBD_SHORT_XFER_OK | USBD_NO_COPY,
				USBD_NO_TIMEOUT, udav_rxeof);
		(void)usbd_transfer(c->ue_xfer);
		DPRINTF(("%s: %s: start read\n", device_get_nameunit(sc->sc_dev),
			 __func__));
	}

 done:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);

	return (error);
}

static void
udav_start(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mbuf *m_head = NULL;

	DPRINTF(("%s: %s: enter, link=%d\n", device_get_nameunit(sc->sc_dev),
		 __func__, sc->sc_link));

	if (sc->sc_dying)
		return;

	if (!sc->sc_link)
		return;

#if defined(__FreeBSD__)
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
#else
	if (ifp->if_flags & IFF_OACTIVE)
#endif
		return;
#if defined(__NetBSD__)
	IFQ_POLL(&ifp->if_snd, m_head);
#elif defined(__FreeBSD__)
	IF_DEQUEUE(&ifp->if_snd, m_head);
#endif
	if (m_head == NULL)
		return;

	if (udav_send(sc, m_head, 0)) {
#if defined(__FreeBSD__)
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
#else
		ifp->if_flags |= IFF_OACTIVE;
#endif
		return;
	}

#if defined(__NetBSD__)
	IFQ_DEQUEUE(&ifp->if_snd, m_head);
#endif

#if NBPFILTER > 0
	BPF_MTAP(ifp, m_head);
#endif

#if defined(__FreeBSD__)
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
#else
	ifp->if_flags |= IFF_OACTIVE;
#endif

	/* Set a timeout in case the chip goes out to lunch. */
	ifp->if_timer = 5;
}

static int
udav_send(struct udav_softc *sc, struct mbuf *m, int idx)
{
	int total_len;
	struct ue_chain *c;
	usbd_status err;

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev),__func__));

	c = &sc->sc_cdata.ue_tx_chain[idx];

	/* Copy the mbuf data into a contiguous buffer */
	/*  first 2 bytes are packet length */
	m_copydata(m, 0, m->m_pkthdr.len, c->ue_buf + 2);
	c->ue_mbuf = m;
	total_len = m->m_pkthdr.len;
	if (total_len < UDAV_MIN_FRAME_LEN) {
		memset(c->ue_buf + 2 + total_len, 0,
		    UDAV_MIN_FRAME_LEN - total_len);
		total_len = UDAV_MIN_FRAME_LEN;
	}

	/* Frame length is specified in the first 2bytes of the buffer */
	c->ue_buf[0] = (u_int8_t)total_len;
	c->ue_buf[1] = (u_int8_t)(total_len >> 8);
	total_len += 2;

	usbd_setup_xfer(c->ue_xfer, sc->sc_pipe_tx, c, c->ue_buf, total_len,
			USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
			UDAV_TX_TIMEOUT, udav_txeof);

	/* Transmit */
	sc->sc_refcnt++;
	err = usbd_transfer(c->ue_xfer);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	if (err != USBD_IN_PROGRESS) {
		printf("%s: udav_send error=%s\n", device_get_nameunit(sc->sc_dev),
		       usbd_errstr(err));
		/* Stop the interface */
		usb_add_task(sc->sc_udev, &sc->sc_stop_task, USB_TASKQ_DRIVER);
		return (EIO);
	}

	DPRINTF(("%s: %s: send %d bytes\n", device_get_nameunit(sc->sc_dev),
		 __func__, total_len));

	sc->sc_cdata.ue_tx_cnt++;

	return (0);
}

static void
udav_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain *c = priv;
	struct udav_softc *sc = c->ue_sc;
	struct ifnet *ifp = GET_IFP(sc);
#if defined(__NetBSD__)
	int s;
#endif

	if (sc->sc_dying)
		return;

#if defined(__NetBSD__)
	s = splnet();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	ifp->if_timer = 0;
#if defined(__FreeBSD__)
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
#else
	ifp->if_flags &= ~IFF_OACTIVE;
#endif

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
#if defined(__NetBSD__)
			splx(s);
#elif defined(__FreeBSD__)
                        UDAV_UNLOCK(sc);
#endif
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", device_get_nameunit(sc->sc_dev),
		       usbd_errstr(status));
		if (status == USBD_STALLED) {
			sc->sc_refcnt++;
			usbd_clear_endpoint_stall(sc->sc_pipe_tx);
			if (--sc->sc_refcnt < 0)
				usb_detach_wakeup(sc->sc_dev);
		}
#if defined(__NetBSD__)
		splx(s);
#elif defined(__FreeBSD__)
                UDAV_UNLOCK(sc);
#endif
		return;
	}

	ifp->if_opackets++;

	m_freem(c->ue_mbuf);
	c->ue_mbuf = NULL;

#if defined(__NetBSD__)
	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
#elif defined(__FreeBSD__)
	if ( ifp->if_snd.ifq_head != NULL )
#endif
		udav_start(ifp);

#if defined(__NetBSD__)
	splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif
}

static void
udav_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain *c = priv;
	struct udav_softc *sc = c->ue_sc;
	struct ifnet *ifp = GET_IFP(sc);
	struct mbuf *m;
	u_int32_t total_len;
	u_int8_t *pktstat;
#if defined(__NetBSD__)
	int s;
#endif

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev),__func__));

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->sc_rx_errs++;
		if (usbd_ratecheck(&sc->sc_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			       device_get_nameunit(sc->sc_dev), sc->sc_rx_errs,
			       usbd_errstr(status));
			sc->sc_rx_errs = 0;
		}
		if (status == USBD_STALLED) {
			sc->sc_refcnt++;
			usbd_clear_endpoint_stall(sc->sc_pipe_rx);
			if (--sc->sc_refcnt < 0)
				usb_detach_wakeup(sc->sc_dev);
		}
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	/* copy data to mbuf */
	m = c->ue_mbuf;
	memcpy(mtod(m, char *), c->ue_buf, total_len);

	/* first byte in received data */
	pktstat = mtod(m, u_int8_t *);
	m_adj(m, sizeof(u_int8_t));
	DPRINTF(("%s: RX Status: 0x%02x\n", device_get_nameunit(sc->sc_dev), *pktstat));

	total_len = UGETW(mtod(m, u_int8_t *));
	m_adj(m, sizeof(u_int16_t));

	if (*pktstat & UDAV_RSR_LCS) {
		ifp->if_collisions++;
		goto done;
	}

	if (total_len < sizeof(struct ether_header) ||
	    *pktstat & UDAV_RSR_ERR) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	total_len -= ETHER_CRC_LEN;

	m->m_pkthdr.len = m->m_len = total_len;
#if defined(__NetBSD__)
	m->m_pkthdr.rcvif = ifp;
#elif defined(__FreeBSD__)
	m->m_pkthdr.rcvif = (struct ifnet *)&sc->sc_qdat;
#endif

#if defined(__NetBSD__)
	s = splnet();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

#if defined(__NetBSD__)
	c->ue_mbuf = usb_ether_newbuf();
	if (c->ue_mbuf == NULL) {
		printf("%s: no memory for rx list "
		    "-- packet dropped!\n", device_get_nameunit(sc->sc_dev));
		ifp->if_ierrors++;
		goto done1;
	}
#endif

#if NBPFILTER > 0
	BPF_MTAP(ifp, m);
#endif

	DPRINTF(("%s: %s: deliver %d\n", device_get_nameunit(sc->sc_dev),
		 __func__, m->m_len));
#if defined(__NetBSD__)
	IF_INPUT(ifp, m);
#endif
#if defined(__FreeBSD__)
	usb_ether_input(m);
        UDAV_UNLOCK(sc);
        return ;
#endif

#if defined(__NetBSD__)
 done1:
	splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif
 done:
	/* Setup new transfer */
	usbd_setup_xfer(xfer, sc->sc_pipe_rx, c, c->ue_buf, UE_BUFSZ,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, udav_rxeof);
	sc->sc_refcnt++;
	usbd_transfer(xfer);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);

	DPRINTF(("%s: %s: start rx\n", device_get_nameunit(sc->sc_dev), __func__));
}

#if 0
static void udav_intr()
{
}
#endif

static int
udav_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct udav_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
#if defined(__NetBSD__)
	int s;
#endif
	int error = 0;

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (EIO);

#if defined(__NetBSD__)
	s = splnet();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

	switch (cmd) {
#if defined(__FreeBSD__)
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC) {
				UDAV_SETBIT(sc, UDAV_RCR,
					    UDAV_RCR_ALL|UDAV_RCR_PRMSC);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
				   !(ifp->if_flags & IFF_PROMISC)) {
				if (ifp->if_flags & IFF_ALLMULTI)
					UDAV_CLRBIT(sc, UDAV_RCR,
 						    UDAV_RCR_PRMSC);
 				else
 					UDAV_CLRBIT(sc, UDAV_RCR,
 						    UDAV_RCR_ALL|UDAV_RCR_PRMSC);
 			} else if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
 				udav_init(sc);
 		} else {
 			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
 				udav_stop(ifp, 1);
 		}
 		error = 0;
 		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		udav_setmulti(sc);
		error = 0;
		break;
#endif
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
#if defined(__NetBSD__)
		if (error == ENETRESET) {
			udav_setmulti(sc);
			error = 0;
		}
#endif
		break;
	}

#if defined(__NetBSD__)
	splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif

	return (error);
}

static void
udav_watchdog(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct ue_chain *c;
	usbd_status stat;
#if defined(__NetBSD__)
	int s;
#endif

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", device_get_nameunit(sc->sc_dev));

#if defined(__NetBSD__)
	s = splusb();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc)
#endif
	c = &sc->sc_cdata.ue_tx_chain[0];
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &stat);
	udav_txeof(c->ue_xfer, c, stat);

#if defined(__NetBSD__)
	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
#elif defined(__FreeBSD__)
	if ( ifp->if_snd.ifq_head != NULL )
#endif
		udav_start(ifp);
#if defined(__NetBSD__)
	splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif
}

static void
udav_stop_task(struct udav_softc *sc)
{
	udav_stop(GET_IFP(sc), 1);
}

/* Stop the adapter and free any mbufs allocated to the RX and TX lists. */
static void
udav_stop(struct ifnet *ifp, int disable)
{
	struct udav_softc *sc = ifp->if_softc;
	usbd_status err;

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	ifp->if_timer = 0;

	udav_reset(sc);

	usb_uncallout(sc->sc_stat_ch, udav_tick, sc);

	/* Stop transfers */
	/* RX endpoint */
	if (sc->sc_pipe_rx != NULL) {
		err = usbd_abort_pipe(sc->sc_pipe_rx);
		if (err)
			printf("%s: abort rx pipe failed: %s\n",
			       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_pipe_rx);
		if (err)
			printf("%s: close rx pipe failed: %s\n",
			       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		sc->sc_pipe_rx = NULL;
	}

	/* TX endpoint */
	if (sc->sc_pipe_tx != NULL) {
		err = usbd_abort_pipe(sc->sc_pipe_tx);
		if (err)
			printf("%s: abort tx pipe failed: %s\n",
			       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_pipe_tx);
		if (err)
			printf("%s: close tx pipe failed: %s\n",
			       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		sc->sc_pipe_tx = NULL;
	}

#if 0
	/* XXX: Interrupt endpoint is not yet supported!! */
	/* Interrupt endpoint */
	if (sc->sc_pipe_intr != NULL) {
		err = usbd_abort_pipe(sc->sc_pipe_intr);
		if (err)
			printf("%s: abort intr pipe failed: %s\n",
			       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_pipe_intr);
		if (err)
			printf("%s: close intr pipe failed: %s\n",
			       device_get_nameunit(sc->sc_dev), usbd_errstr(err));
		sc->sc_pipe_intr = NULL;
	}
#endif

	/* Free RX resources. */
	usb_ether_rx_list_free(&sc->sc_cdata);
	/* Free TX resources. */
	usb_ether_tx_list_free(&sc->sc_cdata);

	sc->sc_link = 0;
#if defined(__FreeBSD__)
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
#else
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#endif
}

/* Set media options */
static int
udav_ifmedia_change(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	sc->sc_link = 0;
	if (mii->mii_instance) {
		struct mii_softc *miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		     miisc = LIST_NEXT(miisc, mii_list))
			mii_phy_reset(miisc);
	}

	return (mii_mediachg(mii));
}

/* Report current media status. */
static void
udav_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return;

#if defined(__FreeBSD__)
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
#else
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
#endif
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
udav_tick(void *xsc)
{
	struct udav_softc *sc = xsc;

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", device_get_nameunit(sc->sc_dev),
			__func__));

	if (sc->sc_dying)
		return;

	/* Perform periodic stuff in process context */
	usb_add_task(sc->sc_udev, &sc->sc_tick_task, USB_TASKQ_DRIVER);
}

static void
udav_tick_task(void *xsc)
{
	struct udav_softc *sc = xsc;
	struct ifnet *ifp;
	struct mii_data *mii;
#if defined(__NetBSD__)
	int s;
#endif

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", device_get_nameunit(sc->sc_dev),
			__func__));

	if (sc->sc_dying)
		return;

	ifp = GET_IFP(sc);
	mii = GET_MII(sc);

	if (mii == NULL)
		return;

#if defined(__NetBSD__)
	s = splnet();
#elif defined(__FreeBSD__)
        UDAV_LOCK(sc);
#endif

	mii_tick(mii);
	if (!sc->sc_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			DPRINTF(("%s: %s: got link\n",
				 device_get_nameunit(sc->sc_dev), __func__));
			sc->sc_link++;
#if defined(__NetBSD__)
			if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
#elif defined(__FreeBSD__)
			if ( ifp->if_snd.ifq_head != NULL )
#endif
				   udav_start(ifp);
		}
	}

	usb_callout(sc->sc_stat_ch, hz, udav_tick, sc);

#if defined(__NetBSD__)
	splx(s);
#elif defined(__FreeBSD__)
        UDAV_UNLOCK(sc);
#endif
}

/* Get exclusive access to the MII registers */
static void
udav_lock_mii(struct udav_softc *sc)
{
	DPRINTFN(0xff, ("%s: %s: enter\n", device_get_nameunit(sc->sc_dev),
			__func__));

	sc->sc_refcnt++;
#if defined(__NetBSD__)
	lockmgr(&sc->sc_mii_lock, LK_EXCLUSIVE, NULL);
#elif defined(__FreeBSD__)
	lockmgr(&sc->sc_mii_lock, LK_EXCLUSIVE, NULL, NULL);
#endif
}

static void
udav_unlock_mii(struct udav_softc *sc)
{
	DPRINTFN(0xff, ("%s: %s: enter\n", device_get_nameunit(sc->sc_dev),
		       __func__));

#if defined(__NetBSD__)
	lockmgr(&sc->sc_mii_lock, LK_RELEASE, NULL);
#elif defined(__FreeBSD__)
	lockmgr(&sc->sc_mii_lock, LK_RELEASE, NULL, NULL);
#endif
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
}

static int
udav_miibus_readreg(device_t dev, int phy, int reg)
{
	struct udav_softc *sc;
	u_int8_t val[2];
	u_int16_t data16;

	if (dev == NULL)
		return (0);

	sc = USBGETSOFTC(dev);

	DPRINTFN(0xff, ("%s: %s: enter, phy=%d reg=0x%04x\n",
		 device_get_nameunit(sc->sc_dev), __func__, phy, reg));

	if (sc->sc_dying) {
#ifdef DIAGNOSTIC
		printf("%s: %s: dying\n", device_get_nameunit(sc->sc_dev),
		       __func__);
#endif
		return (0);
	}

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0) {
		DPRINTFN(0xff, ("%s: %s: phy=%d is not supported\n",
			 device_get_nameunit(sc->sc_dev), __func__, phy));
		return (0);
	}

	udav_lock_mii(sc);

	/* select internal PHY and set PHY register address */
	udav_csr_write1(sc, UDAV_EPAR,
			UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* select PHY operation and start read command */
	udav_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRR);

	/* XXX: should be wait? */

	/* end read command */
	UDAV_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRR);

	/* retrieve the result from data registers */
	udav_csr_read(sc, UDAV_EPDRL, val, 2);

	udav_unlock_mii(sc);

	data16 = val[0] | (val[1] << 8);

	DPRINTFN(0xff, ("%s: %s: phy=%d reg=0x%04x => 0x%04x\n",
		 device_get_nameunit(sc->sc_dev), __func__, phy, reg, data16));

	return (data16);
}

static int
udav_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct udav_softc *sc;
	u_int8_t val[2];

	if (dev == NULL)
		return (0);	/* XXX real error? */

	sc = USBGETSOFTC(dev);

	DPRINTFN(0xff, ("%s: %s: enter, phy=%d reg=0x%04x data=0x%04x\n",
		 device_get_nameunit(sc->sc_dev), __func__, phy, reg, data));

	if (sc->sc_dying) {
#ifdef DIAGNOSTIC
		printf("%s: %s: dying\n", device_get_nameunit(sc->sc_dev),
		       __func__);
#endif
		return (0);	/* XXX real error? */
	}

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0) {
		DPRINTFN(0xff, ("%s: %s: phy=%d is not supported\n",
			 device_get_nameunit(sc->sc_dev), __func__, phy));
		return (0);	/* XXX real error? */
	}

	udav_lock_mii(sc);

	/* select internal PHY and set PHY register address */
	udav_csr_write1(sc, UDAV_EPAR,
			UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* put the value to the data registers */
	val[0] = data & 0xff;
	val[1] = (data >> 8) & 0xff;
	udav_csr_write(sc, UDAV_EPDRL, val, 2);

	/* select PHY operation and start write command */
	udav_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRW);

	/* XXX: should be wait? */

	/* end write command */
	UDAV_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRW);

	udav_unlock_mii(sc);

	return (0);
}

static void
udav_miibus_statchg(device_t dev)
{
#ifdef UDAV_DEBUG
	struct udav_softc *sc;

	if (dev == NULL)
		return;

	sc = USBGETSOFTC(dev);
	DPRINTF(("%s: %s: enter\n", device_get_nameunit(sc->sc_dev), __func__));
#endif
	/* Nothing to do */
}

#if defined(__FreeBSD__)
/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
udav_shutdown(device_t dev)
{
	struct udav_softc	*sc;

	sc = device_get_softc(dev);

	udav_stop_task(sc);

	return (0);
}

static void
udav_rxstart(struct ifnet *ifp)
{
	struct udav_softc	*sc;
	struct ue_chain	*c;

	sc = ifp->if_softc;
	UDAV_LOCK(sc);
	c = &sc->sc_cdata.ue_rx_chain[sc->sc_cdata.ue_rx_prod];

	c->ue_mbuf = usb_ether_newbuf();
	if (c->ue_mbuf == NULL) {
		printf("%s: no memory for rx list "
		    "-- packet dropped!\n", device_get_nameunit(sc->sc_dev));
		ifp->if_ierrors++;
		UDAV_UNLOCK(sc);
		return;
	}

	/* Setup new transfer. */
        usbd_setup_xfer(c->ue_xfer, sc->sc_pipe_rx,
                        c, c->ue_buf, UE_BUFSZ,
                        USBD_SHORT_XFER_OK | USBD_NO_COPY,
                        USBD_NO_TIMEOUT, udav_rxeof);
	usbd_transfer(c->ue_xfer);

	UDAV_UNLOCK(sc);
	return;
}
#endif
