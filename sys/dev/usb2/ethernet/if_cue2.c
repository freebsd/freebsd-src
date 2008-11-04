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
 * CATC USB-EL1210A USB to ethernet driver. Used in the CATC Netmate
 * adapters and others.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The CATC USB-EL1210A provides USB ethernet support at 10Mbps. The
 * RX filter uses a 512-bit multicast hash table, single perfect entry
 * for the station address, and promiscuous mode. Unlike the ADMtek
 * and KLSI chips, the CATC ASIC supports read and write combining
 * mode where multiple packets can be transfered using a single bulk
 * transaction, which helps performance a great deal.
 */

/*
 * NOTE: all function names beginning like "cue_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc usb2_ether_cc
#define	usb2_config_td_softc cue_softc

#define	USB_DEBUG_VAR cue_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_cue2_reg.h>

/*
 * Various supported device vendors/products.
 */

/* Belkin F5U111 adapter covered by NETMATE entry */

static const struct usb2_device_id cue_devs[] = {
	{USB_VPI(USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE, 0)},
	{USB_VPI(USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE2, 0)},
	{USB_VPI(USB_VENDOR_SMARTBRIDGES, USB_PRODUCT_SMARTBRIDGES_SMARTLINK, 0)},
};

/* prototypes */

static device_probe_t cue_probe;
static device_attach_t cue_attach;
static device_detach_t cue_detach;
static device_shutdown_t cue_shutdown;

static usb2_callback_t cue_bulk_read_clear_stall_callback;
static usb2_callback_t cue_bulk_read_callback;
static usb2_callback_t cue_bulk_write_clear_stall_callback;
static usb2_callback_t cue_bulk_write_callback;

static usb2_config_td_command_t cue_cfg_promisc_upd;
static usb2_config_td_command_t cue_config_copy;
static usb2_config_td_command_t cue_cfg_first_time_setup;
static usb2_config_td_command_t cue_cfg_tick;
static usb2_config_td_command_t cue_cfg_pre_init;
static usb2_config_td_command_t cue_cfg_init;
static usb2_config_td_command_t cue_cfg_pre_stop;
static usb2_config_td_command_t cue_cfg_stop;

static void cue_cfg_do_request(struct cue_softc *sc, struct usb2_device_request *req, void *data);
static uint8_t cue_cfg_csr_read_1(struct cue_softc *sc, uint16_t reg);
static uint16_t cue_cfg_csr_read_2(struct cue_softc *sc, uint8_t reg);
static void cue_cfg_csr_write_1(struct cue_softc *sc, uint16_t reg, uint16_t val);
static void cue_cfg_mem(struct cue_softc *sc, uint8_t cmd, uint16_t addr, void *buf, uint16_t len);
static void cue_cfg_getmac(struct cue_softc *sc, void *buf);
static void cue_mchash(struct usb2_config_td_cc *cc, const uint8_t *addr);
static void cue_cfg_reset(struct cue_softc *sc);
static void cue_start_cb(struct ifnet *ifp);
static void cue_start_transfers(struct cue_softc *sc);
static void cue_init_cb(void *arg);
static int cue_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data);
static void cue_watchdog(void *arg);

#if USB_DEBUG
static int cue_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, cue, CTLFLAG_RW, 0, "USB cue");
SYSCTL_INT(_hw_usb2_cue, OID_AUTO, debug, CTLFLAG_RW, &cue_debug, 0,
    "Debug level");
#endif

static const struct usb2_config cue_config[CUE_ENDPT_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = (MCLBYTES + 2),
		.mh.flags = {.pipe_bof = 1,},
		.mh.callback = &cue_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = (MCLBYTES + 2),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &cue_bulk_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &cue_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &cue_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static device_method_t cue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, cue_probe),
	DEVMETHOD(device_attach, cue_attach),
	DEVMETHOD(device_detach, cue_detach),
	DEVMETHOD(device_shutdown, cue_shutdown),

	{0, 0}
};

static driver_t cue_driver = {
	.name = "cue",
	.methods = cue_methods,
	.size = sizeof(struct cue_softc),
};

static devclass_t cue_devclass;

DRIVER_MODULE(cue, ushub, cue_driver, cue_devclass, NULL, 0);
MODULE_DEPEND(cue, usb2_ethernet, 1, 1, 1);
MODULE_DEPEND(cue, usb2_core, 1, 1, 1);
MODULE_DEPEND(cue, ether, 1, 1, 1);

static void
cue_cfg_do_request(struct cue_softc *sc, struct usb2_device_request *req,
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

#define	CUE_CFG_SETBIT(sc, reg, x)				\
	cue_cfg_csr_write_1(sc, reg, cue_cfg_csr_read_1(sc, reg) | (x))

#define	CUE_CFG_CLRBIT(sc, reg, x)				\
	cue_cfg_csr_write_1(sc, reg, cue_cfg_csr_read_1(sc, reg) & ~(x))

static uint8_t
cue_cfg_csr_read_1(struct cue_softc *sc, uint16_t reg)
{
	struct usb2_device_request req;
	uint8_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	cue_cfg_do_request(sc, &req, &val);
	return (val);
}

static uint16_t
cue_cfg_csr_read_2(struct cue_softc *sc, uint8_t reg)
{
	struct usb2_device_request req;
	uint16_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	cue_cfg_do_request(sc, &req, &val);
	return (le16toh(val));
}

static void
cue_cfg_csr_write_1(struct cue_softc *sc, uint16_t reg, uint16_t val)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	cue_cfg_do_request(sc, &req, NULL);
	return;
}

static void
cue_cfg_mem(struct cue_softc *sc, uint8_t cmd, uint16_t addr,
    void *buf, uint16_t len)
{
	struct usb2_device_request req;

	if (cmd == CUE_CMD_READSRAM) {
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	} else {
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	}
	req.bRequest = cmd;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	cue_cfg_do_request(sc, &req, buf);
	return;
}

static void
cue_cfg_getmac(struct cue_softc *sc, void *buf)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_GET_MACADDR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, ETHER_ADDR_LEN);

	cue_cfg_do_request(sc, &req, buf);
	return;
}

#define	CUE_BITS 9

static void
cue_mchash(struct usb2_config_td_cc *cc, const uint8_t *addr)
{
	uint16_t h;

	h = ether_crc32_le(addr, ETHER_ADDR_LEN) &
	    ((1 << CUE_BITS) - 1);
	cc->if_hash[h >> 3] |= 1 << (h & 0x7);
	return;
}

static void
cue_cfg_promisc_upd(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* if we want promiscuous mode, set the allframes bit */

	if (cc->if_flags & IFF_PROMISC) {
		CUE_CFG_SETBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
	} else {
		CUE_CFG_CLRBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
	}

	/* write multicast hash-bits */

	cue_cfg_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
	    cc->if_hash, CUE_MCAST_TABLE_LEN);
	return;
}

static void
cue_config_copy(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	bzero(cc, sizeof(*cc));
	usb2_ether_cc(sc->sc_ifp, &cue_mchash, cc);
	return;
}

static void
cue_cfg_reset(struct cue_softc *sc)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	cue_cfg_do_request(sc, &req, NULL);

	/*
	 * wait a little while for the chip to get its brains in order:
	 */

	(void)usb2_config_td_sleep(&sc->sc_config_td, hz / 100);
	return;
}

static int
cue_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != CUE_CONFIG_IDX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != CUE_IFACE_IDX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(cue_devs, sizeof(cue_devs), uaa));
}

static int
cue_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct cue_softc *sc = device_get_softc(dev);
	uint8_t iface_index;
	int32_t error;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "cue lock", NULL, MTX_DEF | MTX_RECURSE);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	iface_index = CUE_IFACE_IDX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, cue_config, CUE_ENDPT_MAX, sc, &sc->sc_mtx);
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

	/* start setup */

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &cue_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	cue_watchdog(sc);

	return (0);			/* success */

detach:
	cue_detach(dev);
	return (ENXIO);			/* failure */
}

static void
cue_cfg_first_time_setup(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint8_t eaddr[ETHER_ADDR_LEN];
	struct ifnet *ifp;

#if 0
	/* Reset the adapter. */
	cue_cfg_reset(sc);
#endif
	/*
	 * Get station address.
	 */
	cue_cfg_getmac(sc, eaddr);

	mtx_unlock(&sc->sc_mtx);

	ifp = if_alloc(IFT_ETHER);

	mtx_lock(&sc->sc_mtx);

	if (ifp == NULL) {
		printf("cue%d: could not if_alloc()\n",
		    sc->sc_unit);
		goto done;
	}
	sc->sc_evilhack = ifp;

	ifp->if_softc = sc;
	if_initname(ifp, "cue", sc->sc_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cue_ioctl_cb;
	ifp->if_start = cue_start_cb;
	ifp->if_watchdog = NULL;
	ifp->if_init = cue_init_cb;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	sc->sc_ifp = ifp;

	mtx_unlock(&sc->sc_mtx);

	ether_ifattach(ifp, eaddr);

	mtx_lock(&sc->sc_mtx);

done:
	return;
}

static int
cue_detach(device_t dev)
{
	struct cue_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	cue_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, CUE_ENDPT_MAX);

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
cue_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct cue_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~CUE_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
cue_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct cue_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m = NULL;
	uint8_t buf[2];
	uint16_t len;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen <= (2 + sizeof(struct ether_header))) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, 0, buf, 2);

		len = buf[0] | (buf[1] << 8);

		xfer->actlen -= 2;

		m = usb2_ether_get_mbuf();

		if (m == NULL) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		xfer->actlen = min(xfer->actlen, m->m_len);
		xfer->actlen = min(xfer->actlen, len);

		usb2_copy_out(xfer->frbuffers, 2, m->m_data, xfer->actlen);

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = xfer->actlen;

	case USB_ST_SETUP:
tr_setup:

		if (sc->sc_flags & CUE_FLAG_READ_STALL) {
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
			sc->sc_flags |= CUE_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		DPRINTF("bulk read error, %s\n",
		    usb2_errstr(xfer->error));
		return;

	}
}

static void
cue_cfg_tick(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if ((ifp == NULL)) {
		/* not ready */
		return;
	}
	ifp->if_collisions += cue_cfg_csr_read_2(sc, CUE_TX_SINGLECOLL);
	ifp->if_collisions += cue_cfg_csr_read_2(sc, CUE_TX_MULTICOLL);
	ifp->if_collisions += cue_cfg_csr_read_2(sc, CUE_TX_EXCESSCOLL);

	if (cue_cfg_csr_read_2(sc, CUE_RX_FRAMEERR)) {
		ifp->if_ierrors++;
	}
	/* start stopped transfers, if any */

	cue_start_transfers(sc);

	return;
}

static void
cue_start_cb(struct ifnet *ifp)
{
	struct cue_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	cue_start_transfers(sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
cue_start_transfers(struct cue_softc *sc)
{
	if ((sc->sc_flags & CUE_FLAG_LL_READY) &&
	    (sc->sc_flags & CUE_FLAG_HL_READY)) {

		/*
		 * start the USB transfers, if not already started:
		 */
		usb2_transfer_start(sc->sc_xfer[1]);
		usb2_transfer_start(sc->sc_xfer[0]);
	}
	return;
}

static void
cue_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct cue_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~CUE_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
cue_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct cue_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");

		ifp->if_opackets++;

	case USB_ST_SETUP:

		if (sc->sc_flags & CUE_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			goto done;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL) {
			goto done;
		}
		if (m->m_pkthdr.len > MCLBYTES) {
			m->m_pkthdr.len = MCLBYTES;
		}
		xfer->frlengths[0] = (m->m_pkthdr.len + 2);

		/* the first two bytes are the frame length */

		buf[0] = (uint8_t)(m->m_pkthdr.len);
		buf[1] = (uint8_t)(m->m_pkthdr.len >> 8);

		usb2_copy_in(xfer->frbuffers, 0, buf, 2);

		usb2_m_copy_in(xfer->frbuffers, 2,
		    m, 0, m->m_pkthdr.len);

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m);

		m_freem(m);

		usb2_start_hardware(xfer);

done:
		return;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= CUE_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		ifp->if_oerrors++;
		return;

	}
}

static void
cue_init_cb(void *arg)
{
	struct cue_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &cue_cfg_pre_init,
	    &cue_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
cue_cfg_pre_init(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* immediate configuration */

	cue_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= CUE_FLAG_HL_READY;

	return;
}

static void
cue_cfg_init(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	uint8_t i;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	cue_cfg_stop(sc, cc, 0);
#if 0
	cue_cfg_reset(sc);
#endif
	/* Set MAC address */

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		cue_cfg_csr_write_1(sc, CUE_PAR0 - i, cc->if_lladdr[i]);
	}

	/* Enable RX logic. */
	cue_cfg_csr_write_1(sc, CUE_ETHCTL, CUE_ETHCTL_RX_ON | CUE_ETHCTL_MCAST_ON);

	/* Load the multicast filter */
	cue_cfg_promisc_upd(sc, cc, 0);

	/*
	 * Set the number of RX and TX buffers that we want
	 * to reserve inside the ASIC.
	 */
	cue_cfg_csr_write_1(sc, CUE_RX_BUFPKTS, CUE_RX_FRAMES);
	cue_cfg_csr_write_1(sc, CUE_TX_BUFPKTS, CUE_TX_FRAMES);

	/* Set advanced operation modes. */
	cue_cfg_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN | 0x01);/* 1 wait state */

	/* Program the LED operation. */
	cue_cfg_csr_write_1(sc, CUE_LEDCTL, CUE_LEDCTL_FOLLOW_LINK);

	sc->sc_flags |= (CUE_FLAG_READ_STALL |
	    CUE_FLAG_WRITE_STALL |
	    CUE_FLAG_LL_READY);

	cue_start_transfers(sc);
	return;
}

static int
cue_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cue_softc *sc = ifp->if_softc;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &cue_config_copy,
				    &cue_cfg_promisc_upd, 0, 0);
			} else {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &cue_cfg_pre_init,
				    &cue_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &cue_cfg_pre_stop,
				    &cue_cfg_stop, 0, 0);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mtx_lock(&sc->sc_mtx);
		usb2_config_td_queue_command
		    (&sc->sc_config_td, &cue_config_copy,
		    &cue_cfg_promisc_upd, 0, 0);
		mtx_unlock(&sc->sc_mtx);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static void
cue_watchdog(void *arg)
{
	struct cue_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &cue_cfg_tick, 0, 0);

	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &cue_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);
	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
cue_cfg_pre_stop(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		cue_config_copy(sc, cc, refcount);
	}
	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(CUE_FLAG_HL_READY |
	    CUE_FLAG_LL_READY);

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[0]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[3]);
	return;
}

static void
cue_cfg_stop(struct cue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	cue_cfg_csr_write_1(sc, CUE_ETHCTL, 0);
	cue_cfg_reset(sc);
	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
cue_shutdown(device_t dev)
{
	struct cue_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &cue_cfg_pre_stop,
	    &cue_cfg_stop, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return (0);
}
