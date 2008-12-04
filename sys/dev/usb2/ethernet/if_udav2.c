/*	$NetBSD: if_udav.c,v 1.2 2003/09/04 15:17:38 tsutsui Exp $	*/
/*	$nabe: if_udav.c,v 1.3 2003/08/21 16:57:19 nabe Exp $	*/
/*	$FreeBSD$	*/
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
 * NOTE: all function names beginning like "udav_cfg_" can only
 * be called from within the config thread function !
 */

/*
 * TODO:
 *	Interrupt Endpoint support
 *	External PHYs
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc usb2_ether_cc
#define	usb2_config_td_softc udav_softc

#define	USB_DEBUG_VAR udav_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_udav2_reg.h>

/* prototypes */

static device_probe_t udav_probe;
static device_attach_t udav_attach;
static device_detach_t udav_detach;
static device_shutdown_t udav_shutdown;

static usb2_callback_t udav_bulk_write_clear_stall_callback;
static usb2_callback_t udav_bulk_write_callback;
static usb2_callback_t udav_bulk_read_clear_stall_callback;
static usb2_callback_t udav_bulk_read_callback;
static usb2_callback_t udav_intr_clear_stall_callback;
static usb2_callback_t udav_intr_callback;

static usb2_config_td_command_t udav_cfg_first_time_setup;
static usb2_config_td_command_t udav_cfg_pre_init;
static usb2_config_td_command_t udav_cfg_init;
static usb2_config_td_command_t udav_config_copy;
static usb2_config_td_command_t udav_cfg_promisc_upd;
static usb2_config_td_command_t udav_cfg_pre_stop;
static usb2_config_td_command_t udav_cfg_stop;
static usb2_config_td_command_t udav_cfg_ifmedia_change;
static usb2_config_td_command_t udav_cfg_tick;

static void udav_cfg_do_request(struct udav_softc *sc, struct usb2_device_request *req, void *data);
static void udav_cfg_csr_read(struct udav_softc *sc, uint16_t offset, void *buf, uint16_t len);
static void udav_cfg_csr_write(struct udav_softc *sc, uint16_t offset, void *buf, uint16_t len);
static uint8_t udav_cfg_csr_read1(struct udav_softc *sc, uint16_t offset);
static void udav_cfg_csr_write1(struct udav_softc *sc, uint16_t offset, uint8_t ch);
static void udav_init_cb(void *arg);
static void udav_cfg_reset(struct udav_softc *sc);
static void udav_start_cb(struct ifnet *ifp);
static void udav_start_transfers(struct udav_softc *sc);
static int udav_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data);
static void udav_watchdog(void *arg);
static int udav_ifmedia_change_cb(struct ifnet *ifp);
static void udav_ifmedia_status_cb(struct ifnet *ifp, struct ifmediareq *ifmr);

static miibus_readreg_t udav_cfg_miibus_readreg;
static miibus_writereg_t udav_cfg_miibus_writereg;
static miibus_statchg_t udav_cfg_miibus_statchg;

static const struct usb2_config udav_config[UDAV_ENDPT_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = (MCLBYTES + 2),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &udav_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = (MCLBYTES + 3),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &udav_bulk_read_callback,
		.mh.timeout = 0,	/* no timeout */
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &udav_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &udav_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[4] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &udav_intr_callback,
	},

	[5] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &udav_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static device_method_t udav_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, udav_probe),
	DEVMETHOD(device_attach, udav_attach),
	DEVMETHOD(device_detach, udav_detach),
	DEVMETHOD(device_shutdown, udav_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, udav_cfg_miibus_readreg),
	DEVMETHOD(miibus_writereg, udav_cfg_miibus_writereg),
	DEVMETHOD(miibus_statchg, udav_cfg_miibus_statchg),

	{0, 0}
};

static driver_t udav_driver = {
	.name = "udav",
	.methods = udav_methods,
	.size = sizeof(struct udav_softc),
};

static devclass_t udav_devclass;

DRIVER_MODULE(udav, ushub, udav_driver, udav_devclass, NULL, 0);
DRIVER_MODULE(miibus, udav, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(udav, usb2_ethernet, 1, 1, 1);
MODULE_DEPEND(udav, usb2_core, 1, 1, 1);
MODULE_DEPEND(udav, ether, 1, 1, 1);
MODULE_DEPEND(udav, miibus, 1, 1, 1);

#if USB_DEBUG
static int udav_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, udav, CTLFLAG_RW, 0, "USB udav");
SYSCTL_INT(_hw_usb2_udav, OID_AUTO, debug, CTLFLAG_RW, &udav_debug, 0,
    "Debug level");
#endif

#define	UDAV_CFG_SETBIT(sc, reg, x)	\
	udav_cfg_csr_write1(sc, reg, udav_cfg_csr_read1(sc, reg) | (x))

#define	UDAV_CFG_CLRBIT(sc, reg, x)	\
	udav_cfg_csr_write1(sc, reg, udav_cfg_csr_read1(sc, reg) & ~(x))

static const struct usb2_device_id udav_devs[] = {
	/* ShanTou DM9601 USB NIC */
	{USB_VPI(USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_DM9601, 0)},

	/* ShanTou ST268 USB NIC */
	{USB_VPI(USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_ST268, 0)},

	/* Corega USB-TXC */
	{USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TXC, 0)},
};

static int
udav_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UDAV_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UDAV_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(udav_devs, sizeof(udav_devs), uaa));
}

static int
udav_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct udav_softc *sc = device_get_softc(dev);
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

	mtx_init(&sc->sc_mtx, "udav lock", NULL, MTX_DEF | MTX_RECURSE);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	iface_index = UDAV_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, udav_config, UDAV_ENDPT_MAX, sc, &sc->sc_mtx);
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

	sc->sc_flags |= UDAV_FLAG_WAIT_LINK;

	/* start setup */

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &udav_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	udav_watchdog(sc);

	return (0);			/* success */

detach:
	udav_detach(dev);
	return (ENXIO);			/* failure */
}

static void
udav_cfg_first_time_setup(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp;
	int error;
	uint8_t eaddr[min(ETHER_ADDR_LEN, 6)];

	/* reset the adapter */

	udav_cfg_reset(sc);

	/* get Ethernet Address */

	udav_cfg_csr_read(sc, UDAV_PAR, eaddr, ETHER_ADDR_LEN);

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
	if_initname(ifp, "udav", sc->sc_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = udav_start_cb;
	ifp->if_ioctl = udav_ioctl_cb;
	ifp->if_watchdog = NULL;
	ifp->if_init = udav_init_cb;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * XXX need Giant when accessing the device structures !
	 */

	mtx_unlock(&sc->sc_mtx);

	mtx_lock(&Giant);

	error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
	    &udav_ifmedia_change_cb,
	    &udav_ifmedia_status_cb);
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
udav_detach(device_t dev)
{
	struct udav_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	udav_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, UDAV_ENDPT_MAX);

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
udav_cfg_do_request(struct udav_softc *sc, struct usb2_device_request *req,
    void *data)
{
	uint16_t length;
	usb2_error_t err;

	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		goto error;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &sc->sc_mtx, req, data, 0, NULL, 1000);

	if (err) {

		DPRINTF("device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));

error:
		length = UGETW(req->wLength);

		if ((req->bmRequestType & UT_READ) && length) {
			bzero(data, length);
		}
	}
	return;
}

#if 0
static void
udav_cfg_mem_read(struct udav_softc *sc, uint16_t offset, void *buf,
    uint16_t len)
{
	struct usb2_device_request req;

	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	udav_cfg_do_request(sc, &req, buf);
	return;
}

static void
udav_cfg_mem_write(struct udav_softc *sc, uint16_t offset, void *buf,
    uint16_t len)
{
	struct usb2_device_request req;

	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	udav_cfg_do_request(sc, &req, buf);
	return;
}

static void
udav_cfg_mem_write1(struct udav_softc *sc, uint16_t offset,
    uint8_t ch)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	udav_cfg_do_request(sc, &req, NULL);
	return;
}

#endif

static void
udav_cfg_csr_read(struct udav_softc *sc, uint16_t offset, void *buf,
    uint16_t len)
{
	struct usb2_device_request req;

	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	udav_cfg_do_request(sc, &req, buf);
	return;
}

static void
udav_cfg_csr_write(struct udav_softc *sc, uint16_t offset, void *buf,
    uint16_t len)
{
	struct usb2_device_request req;

	offset &= 0xff;
	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	udav_cfg_do_request(sc, &req, buf);
	return;
}

static uint8_t
udav_cfg_csr_read1(struct udav_softc *sc, uint16_t offset)
{
	uint8_t val;

	udav_cfg_csr_read(sc, offset, &val, 1);
	return (val);
}

static void
udav_cfg_csr_write1(struct udav_softc *sc, uint16_t offset,
    uint8_t ch)
{
	struct usb2_device_request req;

	offset &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	udav_cfg_do_request(sc, &req, NULL);
	return;
}

static void
udav_init_cb(void *arg)
{
	struct udav_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &udav_cfg_pre_init,
	    &udav_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
udav_cfg_pre_init(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* immediate configuration */

	udav_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= UDAV_FLAG_HL_READY;

	return;
}

static void
udav_cfg_init(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct mii_data *mii = GET_MII(sc);

	/*
	 * Cancel pending I/O
	 */

	udav_cfg_stop(sc, cc, 0);

	/* set MAC address */

	udav_cfg_csr_write(sc, UDAV_PAR, cc->if_lladdr, ETHER_ADDR_LEN);

	/* initialize network control register */

	/* disable loopback  */

	UDAV_CFG_CLRBIT(sc, UDAV_NCR, UDAV_NCR_LBK0 | UDAV_NCR_LBK1);

	/* Initialize RX control register */
	UDAV_CFG_SETBIT(sc, UDAV_RCR, UDAV_RCR_DIS_LONG | UDAV_RCR_DIS_CRC);

	/* load multicast filter and update promiscious mode bit */
	udav_cfg_promisc_upd(sc, cc, 0);

	/* enable RX */
	UDAV_CFG_SETBIT(sc, UDAV_RCR, UDAV_RCR_RXEN);

	/* clear POWER_DOWN state of internal PHY */
	UDAV_CFG_SETBIT(sc, UDAV_GPCR, UDAV_GPCR_GEP_CNTL0);
	UDAV_CFG_CLRBIT(sc, UDAV_GPR, UDAV_GPR_GEPIO0);

	mii_mediachg(mii);

	sc->sc_flags |= (UDAV_FLAG_READ_STALL |
	    UDAV_FLAG_WRITE_STALL |
	    UDAV_FLAG_LL_READY);

	udav_start_transfers(sc);

	return;
}

static void
udav_cfg_reset(struct udav_softc *sc)
{
	usb2_error_t err;
	uint16_t to;

	/* Select PHY */
#if 1
	/*
	 * XXX: force select internal phy.
	 *	external phy routines are not tested.
	 */
	UDAV_CFG_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
#else
	if (sc->sc_flags & UDAV_EXT_PHY) {
		UDAV_CFG_SETBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	} else {
		UDAV_CFG_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	}
#endif

	UDAV_CFG_SETBIT(sc, UDAV_NCR, UDAV_NCR_RST);

	for (to = 0;; to++) {

		if (to < 100) {

			err = usb2_config_td_sleep(&sc->sc_config_td, hz / 100);

			if (err) {
				break;
			}
			if (!(udav_cfg_csr_read1(sc, UDAV_NCR) & UDAV_NCR_RST)) {
				break;
			}
		} else {
			printf("%s: reset timeout!\n",
			    sc->sc_name);
			break;
		}
	}

	err = usb2_config_td_sleep(&sc->sc_config_td, hz / 100);

	return;
}

#define	UDAV_BITS	6

static void
udav_mchash(struct usb2_config_td_cc *cc, const uint8_t *ptr)
{
	uint8_t h;

	h = ether_crc32_le(ptr, ETHER_ADDR_LEN) &
	    ((1 << UDAV_BITS) - 1);
	cc->if_hash[h >> 3] |= 1 << (h & 0x7);
	return;
}

static void
udav_config_copy(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	bzero(cc, sizeof(*cc));
	usb2_ether_cc(sc->sc_ifp, &udav_mchash, cc);
	return;
}

static void
udav_cfg_promisc_upd(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint8_t rxmode;

	rxmode = udav_cfg_csr_read1(sc, UDAV_RCR);

	rxmode &= ~(UDAV_RCR_ALL | UDAV_RCR_PRMSC);

	if (cc->if_flags & IFF_PROMISC) {

		rxmode |= UDAV_RCR_ALL | UDAV_RCR_PRMSC;

	} else if (cc->if_flags & IFF_ALLMULTI) {

		rxmode |= UDAV_RCR_ALL;
	}
	/* write hash value to the register */
	udav_cfg_csr_write(sc, UDAV_MAR, cc->if_hash, 8);

	/* write new mode bits */
	udav_cfg_csr_write1(sc, UDAV_RCR, rxmode);

	return;
}

static void
udav_start_cb(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	udav_start_transfers(sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
udav_start_transfers(struct udav_softc *sc)
{
	if ((sc->sc_flags & UDAV_FLAG_LL_READY) &&
	    (sc->sc_flags & UDAV_FLAG_HL_READY)) {

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
udav_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct udav_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UDAV_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
udav_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct udav_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint32_t extra_len;
	uint32_t temp_len;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");

		ifp->if_opackets++;

	case USB_ST_SETUP:

		if (sc->sc_flags & UDAV_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			goto done;
		}
		if (sc->sc_flags & UDAV_FLAG_WAIT_LINK) {
			/*
			 * don't send anything if there is no link !
			 */
			goto done;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL) {
			goto done;
		}
		if (m->m_pkthdr.len > MCLBYTES) {
			m->m_pkthdr.len = MCLBYTES;
		}
		if (m->m_pkthdr.len < UDAV_MIN_FRAME_LEN) {
			extra_len = UDAV_MIN_FRAME_LEN - m->m_pkthdr.len;
		} else {
			extra_len = 0;
		}

		temp_len = (m->m_pkthdr.len + extra_len);

		/*
		 * the frame length is specified in the first 2 bytes of the
		 * buffer
		 */
		buf[0] = (uint8_t)(temp_len);
		buf[1] = (uint8_t)(temp_len >> 8);

		temp_len += 2;

		usb2_copy_in(xfer->frbuffers, 0, buf, 2);

		usb2_m_copy_in(xfer->frbuffers, 2,
		    m, 0, m->m_pkthdr.len);

		if (extra_len) {
			usb2_bzero(xfer->frbuffers, temp_len - extra_len,
			    extra_len);
		}
		/*
		 * if there's a BPF listener, bounce a copy
		 * of this frame to him:
		 */
		BPF_MTAP(ifp, m);

		m_freem(m);

		xfer->frlengths[0] = temp_len;
		usb2_start_hardware(xfer);

done:
		return;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UDAV_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		ifp->if_oerrors++;
		return;

	}
}

static void
udav_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct udav_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UDAV_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
udav_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct udav_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t status;
	uint16_t total_len;
	struct mbuf *m = NULL;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen < 1) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		xfer->actlen -= 1;

		usb2_copy_out(xfer->frbuffers, 0, &status, 1);

		if (status & UDAV_RSR_LCS) {
			ifp->if_collisions++;
			goto tr_setup;
		}
		if ((status & UDAV_RSR_ERR) || (xfer->actlen < 2)) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, 1, &total_len, 2);

		total_len = le16toh(total_len);

		xfer->actlen -= 2;

		xfer->actlen = min(xfer->actlen, total_len);

		if (xfer->actlen < (sizeof(struct ether_header) + ETHER_CRC_LEN)) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		xfer->actlen -= ETHER_CRC_LEN;

		m = usb2_ether_get_mbuf();

		if (m == NULL) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		xfer->actlen = min(xfer->actlen, m->m_len);

		usb2_copy_out(xfer->frbuffers, 3, m->m_data, xfer->actlen);

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = xfer->actlen;

	case USB_ST_SETUP:
tr_setup:

		if (sc->sc_flags & UDAV_FLAG_READ_STALL) {
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
		if (m) {
			mtx_unlock(&sc->sc_mtx);
			(ifp->if_input) (ifp, m);
			mtx_lock(&sc->sc_mtx);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UDAV_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		DPRINTF("bulk read error, %s\n",
		    usb2_errstr(xfer->error));
		return;

	}
}

static void
udav_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct udav_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[4];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UDAV_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
udav_intr_callback(struct usb2_xfer *xfer)
{
	struct udav_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

	case USB_ST_SETUP:
		if (sc->sc_flags & UDAV_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[5]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* start clear stall */
			sc->sc_flags |= UDAV_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[5]);
		}
		return;
	}
}

static int
udav_ioctl_cb(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &udav_config_copy,
				    &udav_cfg_promisc_upd, 0, 0);
			} else {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &udav_cfg_pre_init,
				    &udav_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &udav_cfg_pre_stop,
				    &udav_cfg_stop, 0, 0);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mtx_lock(&sc->sc_mtx);
		usb2_config_td_queue_command
		    (&sc->sc_config_td, &udav_config_copy,
		    &udav_cfg_promisc_upd, 0, 0);
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		if (mii == NULL) {
			error = EINVAL;
		} else {
			error = ifmedia_ioctl
			    (ifp, (void *)data, &mii->mii_media, cmd);
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
udav_watchdog(void *arg)
{
	struct udav_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &udav_cfg_tick, 0, 0);

	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &udav_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);
	return;
}

/*
 * NOTE: can be called when "ifp" is NULL
 */
static void
udav_cfg_pre_stop(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		udav_config_copy(sc, cc, refcount);
	}
	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(UDAV_FLAG_HL_READY |
	    UDAV_FLAG_LL_READY);

	sc->sc_flags |= UDAV_FLAG_WAIT_LINK;

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

/*
 * NOTE: can be called when "ifp" is NULL
 */
static void
udav_cfg_stop(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	udav_cfg_reset(sc);
	return;
}

static int
udav_ifmedia_change_cb(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL,
	    &udav_cfg_ifmedia_change, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return (0);
}

static void
udav_cfg_ifmedia_change(struct udav_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mii_data *mii = GET_MII(sc);

	if ((ifp == NULL) ||
	    (mii == NULL)) {
		/* not ready */
		return;
	}
	sc->sc_flags |= UDAV_FLAG_WAIT_LINK;

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
			mii_phy_reset(miisc);
		}
	}
	mii_mediachg(mii);

	return;
}

static void
udav_ifmedia_status_cb(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct udav_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ifmr->ifm_active = sc->sc_media_active;
		ifmr->ifm_status = sc->sc_media_status;
	} else {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
	}

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
udav_cfg_tick(struct udav_softc *sc,
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

	if ((sc->sc_flags & UDAV_FLAG_WAIT_LINK) &&
	    (mii->mii_media_status & IFM_ACTIVE) &&
	    (IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)) {
		sc->sc_flags &= ~UDAV_FLAG_WAIT_LINK;
	}
	sc->sc_media_active = mii->mii_media_active;
	sc->sc_media_status = mii->mii_media_status;

	/* start stopped transfers, if any */

	udav_start_transfers(sc);

	return;
}

static int
udav_cfg_miibus_readreg(device_t dev, int phy, int reg)
{
	struct udav_softc *sc = device_get_softc(dev);
	uint16_t data16;
	uint8_t val[2];
	uint8_t do_unlock;

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0) {
		return (0);
	}
	/* avoid recursive locking */
	if (mtx_owned(&sc->sc_mtx)) {
		do_unlock = 0;
	} else {
		mtx_lock(&sc->sc_mtx);
		do_unlock = 1;
	}

	/* select internal PHY and set PHY register address */
	udav_cfg_csr_write1(sc, UDAV_EPAR,
	    UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* select PHY operation and start read command */
	udav_cfg_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRR);

	/* XXX: should we wait? */

	/* end read command */
	UDAV_CFG_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRR);

	/* retrieve the result from data registers */
	udav_cfg_csr_read(sc, UDAV_EPDRL, val, 2);

	if (do_unlock) {
		mtx_unlock(&sc->sc_mtx);
	}
	data16 = (val[0] | (val[1] << 8));

	DPRINTFN(11, "phy=%d reg=0x%04x => 0x%04x\n",
	    phy, reg, data16);

	return (data16);
}

static int
udav_cfg_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct udav_softc *sc = device_get_softc(dev);
	uint8_t val[2];
	uint8_t do_unlock;

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0) {
		return (0);
	}
	/* avoid recursive locking */
	if (mtx_owned(&sc->sc_mtx)) {
		do_unlock = 0;
	} else {
		mtx_lock(&sc->sc_mtx);
		do_unlock = 1;
	}

	/* select internal PHY and set PHY register address */
	udav_cfg_csr_write1(sc, UDAV_EPAR,
	    UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* put the value to the data registers */
	val[0] = (data & 0xff);
	val[1] = (data >> 8) & 0xff;
	udav_cfg_csr_write(sc, UDAV_EPDRL, val, 2);

	/* select PHY operation and start write command */
	udav_cfg_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRW);

	/* XXX: should we wait? */

	/* end write command */
	UDAV_CFG_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRW);

	if (do_unlock) {
		mtx_unlock(&sc->sc_mtx);
	}
	return (0);
}

static void
udav_cfg_miibus_statchg(device_t dev)
{
	/* nothing to do */
	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
udav_shutdown(device_t dev)
{
	struct udav_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &udav_cfg_pre_stop,
	    &udav_cfg_stop, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return (0);
}
