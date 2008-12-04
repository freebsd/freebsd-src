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
 * Kawasaki LSI KL5KUSB101B USB to ethernet adapter driver.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The KLSI USB to ethernet adapter chip contains an USB serial interface,
 * ethernet MAC and embedded microcontroller (called the QT Engine).
 * The chip must have firmware loaded into it before it will operate.
 * Packets are passed between the chip and host via bulk transfers.
 * There is an interrupt endpoint mentioned in the software spec, however
 * it's currently unused. This device is 10Mbps half-duplex only, hence
 * there is no media selection logic. The MAC supports a 128 entry
 * multicast filter, though the exact size of the filter can depend
 * on the firmware. Curiously, while the software spec describes various
 * ethernet statistics counters, my sample adapter and firmware combination
 * claims not to support any statistics counters at all.
 *
 * Note that once we load the firmware in the device, we have to be
 * careful not to load it again: if you restart your computer but
 * leave the adapter attached to the USB controller, it may remain
 * powered on and retain its firmware. In this case, we don't need
 * to load the firmware a second time.
 *
 * Special thanks to Rob Furr for providing an ADS Technologies
 * adapter for development and testing. No monkeys were harmed during
 * the development of this driver.
 */

/*
 * NOTE: all function names beginning like "kue_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	usb2_config_td_cc usb2_ether_cc
#define	usb2_config_td_softc kue_softc

#define	USB_DEBUG_VAR kue_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_kue2_reg.h>
#include <dev/usb2/ethernet/if_kue2_fw.h>

/*
 * Various supported device vendors/products.
 */
static const struct usb2_device_id kue_devs[] = {
	{USB_VPI(USB_VENDOR_3COM, USB_PRODUCT_3COM_3C19250, 0)},
	{USB_VPI(USB_VENDOR_3COM, USB_PRODUCT_3COM_3C460, 0)},
	{USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_URE450, 0)},
	{USB_VPI(USB_VENDOR_ADS, USB_PRODUCT_ADS_UBS10BT, 0)},
	{USB_VPI(USB_VENDOR_ADS, USB_PRODUCT_ADS_UBS10BTX, 0)},
	{USB_VPI(USB_VENDOR_AOX, USB_PRODUCT_AOX_USB101, 0)},
	{USB_VPI(USB_VENDOR_ASANTE, USB_PRODUCT_ASANTE_EA, 0)},
	{USB_VPI(USB_VENDOR_ATEN, USB_PRODUCT_ATEN_DSB650C, 0)},
	{USB_VPI(USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC10T, 0)},
	{USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_ETHER_USB_T, 0)},
	{USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650C, 0)},
	{USB_VPI(USB_VENDOR_ENTREGA, USB_PRODUCT_ENTREGA_E45, 0)},
	{USB_VPI(USB_VENDOR_ENTREGA, USB_PRODUCT_ENTREGA_XX1, 0)},
	{USB_VPI(USB_VENDOR_ENTREGA, USB_PRODUCT_ENTREGA_XX2, 0)},
	{USB_VPI(USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBETT, 0)},
	{USB_VPI(USB_VENDOR_JATON, USB_PRODUCT_JATON_EDA, 0)},
	{USB_VPI(USB_VENDOR_KINGSTON, USB_PRODUCT_KINGSTON_XX1, 0)},
	{USB_VPI(USB_VENDOR_KLSI, USB_PRODUCT_AOX_USB101, 0)},
	{USB_VPI(USB_VENDOR_KLSI, USB_PRODUCT_KLSI_DUH3E10BT, 0)},
	{USB_VPI(USB_VENDOR_KLSI, USB_PRODUCT_KLSI_DUH3E10BTN, 0)},
	{USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10T, 0)},
	{USB_VPI(USB_VENDOR_MOBILITY, USB_PRODUCT_MOBILITY_EA, 0)},
	{USB_VPI(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_EA101, 0)},
	{USB_VPI(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_EA101X, 0)},
	{USB_VPI(USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET, 0)},
	{USB_VPI(USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET2, 0)},
	{USB_VPI(USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET3, 0)},
	{USB_VPI(USB_VENDOR_PORTGEAR, USB_PRODUCT_PORTGEAR_EA8, 0)},
	{USB_VPI(USB_VENDOR_PORTGEAR, USB_PRODUCT_PORTGEAR_EA9, 0)},
	{USB_VPI(USB_VENDOR_PORTSMITH, USB_PRODUCT_PORTSMITH_EEA, 0)},
	{USB_VPI(USB_VENDOR_SHARK, USB_PRODUCT_SHARK_PA, 0)},
	{USB_VPI(USB_VENDOR_SILICOM, USB_PRODUCT_SILICOM_GPE, 0)},
	{USB_VPI(USB_VENDOR_SILICOM, USB_PRODUCT_SILICOM_U2E, 0)},
	{USB_VPI(USB_VENDOR_SMC, USB_PRODUCT_SMC_2102USB, 0)},
};

/* prototypes */

static device_probe_t kue_probe;
static device_attach_t kue_attach;
static device_detach_t kue_detach;
static device_shutdown_t kue_shutdown;

static usb2_callback_t kue_bulk_read_clear_stall_callback;
static usb2_callback_t kue_bulk_read_callback;
static usb2_callback_t kue_bulk_write_clear_stall_callback;
static usb2_callback_t kue_bulk_write_callback;

static usb2_config_td_command_t kue_cfg_promisc_upd;
static usb2_config_td_command_t kue_config_copy;
static usb2_config_td_command_t kue_cfg_first_time_setup;
static usb2_config_td_command_t kue_cfg_pre_init;
static usb2_config_td_command_t kue_cfg_init;
static usb2_config_td_command_t kue_cfg_tick;
static usb2_config_td_command_t kue_cfg_pre_stop;
static usb2_config_td_command_t kue_cfg_stop;

static void kue_cfg_do_request(struct kue_softc *sc, struct usb2_device_request *req, void *data);
static void kue_cfg_setword(struct kue_softc *sc, uint8_t breq, uint16_t word);
static void kue_cfg_ctl(struct kue_softc *sc, uint8_t rw, uint8_t breq, uint16_t val, void *data, uint16_t len);
static void kue_cfg_load_fw(struct kue_softc *sc);
static void kue_cfg_reset(struct kue_softc *sc);
static void kue_start_cb(struct ifnet *ifp);
static void kue_start_transfers(struct kue_softc *sc);
static void kue_init_cb(void *arg);
static int kue_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data);
static void kue_watchdog(void *arg);

#if USB_DEBUG
static int kue_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, kue, CTLFLAG_RW, 0, "USB kue");
SYSCTL_INT(_hw_usb2_kue, OID_AUTO, debug, CTLFLAG_RW, &kue_debug, 0,
    "Debug level");
#endif

static const struct usb2_config kue_config[KUE_ENDPT_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = (MCLBYTES + 2 + 64),
		.mh.flags = {.pipe_bof = 1,},
		.mh.callback = &kue_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = (MCLBYTES + 2),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &kue_bulk_read_callback,
		.mh.timeout = 0,	/* no timeout */
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &kue_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &kue_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static device_method_t kue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, kue_probe),
	DEVMETHOD(device_attach, kue_attach),
	DEVMETHOD(device_detach, kue_detach),
	DEVMETHOD(device_shutdown, kue_shutdown),

	{0, 0}
};

static driver_t kue_driver = {
	.name = "kue",
	.methods = kue_methods,
	.size = sizeof(struct kue_softc),
};

static devclass_t kue_devclass;

DRIVER_MODULE(kue, ushub, kue_driver, kue_devclass, NULL, 0);
MODULE_DEPEND(kue, usb2_ethernet, 1, 1, 1);
MODULE_DEPEND(kue, usb2_core, 1, 1, 1);
MODULE_DEPEND(kue, ether, 1, 1, 1);

/*
 * We have a custom do_request function which is almost like the
 * regular do_request function, except it has a much longer timeout.
 * Why? Because we need to make requests over the control endpoint
 * to download the firmware to the device, which can take longer
 * than the default timeout.
 */
static void
kue_cfg_do_request(struct kue_softc *sc, struct usb2_device_request *req,
    void *data)
{
	uint16_t length;
	usb2_error_t err;

	if (usb2_config_td_is_gone(&sc->sc_config_td)) {
		goto error;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &sc->sc_mtx, req, data, 0, NULL, 60000);

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

static void
kue_cfg_setword(struct kue_softc *sc, uint8_t breq, uint16_t word)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = breq;
	USETW(req.wValue, word);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	kue_cfg_do_request(sc, &req, NULL);
	return;
}

static void
kue_cfg_ctl(struct kue_softc *sc, uint8_t rw, uint8_t breq,
    uint16_t val, void *data, uint16_t len)
{
	struct usb2_device_request req;

	if (rw == KUE_CTL_WRITE) {
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	} else {
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	}

	req.bRequest = breq;
	USETW(req.wValue, val);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	kue_cfg_do_request(sc, &req, data);
	return;
}

static void
kue_cfg_load_fw(struct kue_softc *sc)
{
	struct usb2_device_descriptor *dd;
	uint16_t hwrev;

	dd = usb2_get_device_descriptor(sc->sc_udev);
	hwrev = UGETW(dd->bcdDevice);

	/*
	 * First, check if we even need to load the firmware.
	 * If the device was still attached when the system was
	 * rebooted, it may already have firmware loaded in it.
	 * If this is the case, we don't need to do it again.
	 * And in fact, if we try to load it again, we'll hang,
	 * so we have to avoid this condition if we don't want
	 * to look stupid.
	 *
	 * We can test this quickly by checking the bcdRevision
	 * code. The NIC will return a different revision code if
	 * it's probed while the firmware is still loaded and
	 * running.
	 */
	if (hwrev == 0x0202) {
		return;
	}
	/* load code segment */
	kue_cfg_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_code_seg, sizeof(kue_code_seg));

	/* load fixup segment */
	kue_cfg_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_fix_seg, sizeof(kue_fix_seg));

	/* send trigger command */
	kue_cfg_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_trig_seg, sizeof(kue_trig_seg));
	return;
}

static void
kue_cfg_promisc_upd(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	kue_cfg_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SET_MCAST_FILTERS,
	    cc->if_nhash, cc->if_hash, cc->if_nhash * ETHER_ADDR_LEN);

	kue_cfg_setword(sc, KUE_CMD_SET_PKT_FILTER, cc->if_rxfilt);

	return;
}

static void
kue_mchash(struct usb2_config_td_cc *cc, const uint8_t *ptr)
{
	uint8_t i;

	i = cc->if_nhash;

	/*
	 * If there are too many addresses for the internal filter,
	 * switch over to allmulti mode.
	 */
	if (i == cc->if_mhash) {
		cc->if_rxfilt |= KUE_RXFILT_ALLMULTI;
	} else {
		bcopy(ptr, cc->if_hash + (i * ETHER_ADDR_LEN), ETHER_ADDR_LEN);
		cc->if_nhash++;
	}
	return;
}

static void
kue_config_copy(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	bzero(cc, sizeof(*cc));

	cc->if_mhash = sc->sc_mcfilt_max;
	cc->if_rxfilt = (KUE_RXFILT_UNICAST | KUE_RXFILT_BROADCAST);

	usb2_ether_cc(sc->sc_ifp, &kue_mchash, cc);

	/*
	 * If we want promiscuous mode, set the all-frames bit:
	 */
	if (cc->if_flags & IFF_PROMISC) {
		cc->if_rxfilt |= KUE_RXFILT_PROMISC;
	}
	if ((cc->if_flags & IFF_ALLMULTI) ||
	    (cc->if_flags & IFF_PROMISC)) {
		cc->if_rxfilt |= KUE_RXFILT_ALLMULTI;
	} else if (cc->if_nhash) {
		cc->if_rxfilt |= KUE_RXFILT_MULTICAST;
	}
	return;
}

/*
 * Issue a SET_CONFIGURATION command to reset the MAC. This should be
 * done after the firmware is loaded into the adapter in order to
 * bring it into proper operation.
 */
static void
kue_cfg_reset(struct kue_softc *sc)
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
 * Probe for a KLSI chip.
 */
static int
kue_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != KUE_CONFIG_IDX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != KUE_IFACE_IDX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(kue_devs, sizeof(kue_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do
 * setup and ethernet/BPF attach.
 */
static int
kue_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct kue_softc *sc = device_get_softc(dev);
	int32_t error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "kue lock", NULL, MTX_DEF | MTX_RECURSE);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	iface_index = KUE_IFACE_IDX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, kue_config, KUE_ENDPT_MAX, sc, &sc->sc_mtx);
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
	    (&sc->sc_config_td, NULL, &kue_cfg_first_time_setup, 0, 0);

	/* start watchdog (will exit mutex) */

	kue_watchdog(sc);

	return (0);			/* success */

detach:
	kue_detach(dev);
	return (ENXIO);			/* failure */
}

static void
kue_cfg_first_time_setup(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp;

	/* load the firmware into the NIC */

	kue_cfg_load_fw(sc);

	/* reset the adapter */

	kue_cfg_reset(sc);

	/* read ethernet descriptor */
	kue_cfg_ctl(sc, KUE_CTL_READ, KUE_CMD_GET_ETHER_DESCRIPTOR,
	    0, &sc->sc_desc, sizeof(sc->sc_desc));

	sc->sc_mcfilt_max = KUE_MCFILTCNT(sc);
	if (sc->sc_mcfilt_max > KUE_MCFILT_MAX) {
		sc->sc_mcfilt_max = KUE_MCFILT_MAX;
	}
	mtx_unlock(&sc->sc_mtx);

	ifp = if_alloc(IFT_ETHER);

	mtx_lock(&sc->sc_mtx);

	if (ifp == NULL) {
		printf("kue%d: could not if_alloc()\n",
		    sc->sc_unit);
		goto done;
	}
	sc->sc_evilhack = ifp;

	ifp->if_softc = sc;
	if_initname(ifp, "kue", sc->sc_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = kue_ioctl_cb;
	ifp->if_start = kue_start_cb;
	ifp->if_watchdog = NULL;
	ifp->if_init = kue_init_cb;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	sc->sc_ifp = ifp;

	mtx_unlock(&sc->sc_mtx);

	ether_ifattach(ifp, sc->sc_desc.kue_macaddr);

	mtx_lock(&sc->sc_mtx);
done:
	return;
}

static int
kue_detach(device_t dev)
{
	struct kue_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;

	usb2_config_td_drain(&sc->sc_config_td);

	mtx_lock(&sc->sc_mtx);

	usb2_callout_stop(&sc->sc_watchdog);

	kue_cfg_pre_stop(sc, NULL, 0);

	ifp = sc->sc_ifp;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, KUE_ENDPT_MAX);

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

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
kue_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct kue_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~KUE_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
kue_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct kue_softc *sc = xfer->priv_sc;
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

		if (sc->sc_flags & KUE_FLAG_READ_STALL) {
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
			sc->sc_flags |= KUE_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		DPRINTF("bulk read error, %s\n",
		    usb2_errstr(xfer->error));
		return;

	}
}

static void
kue_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct kue_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~KUE_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
kue_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct kue_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint32_t total_len;
	uint32_t temp_len;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");

		ifp->if_opackets++;

	case USB_ST_SETUP:

		if (sc->sc_flags & KUE_FLAG_WRITE_STALL) {
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
		temp_len = (m->m_pkthdr.len + 2);
		total_len = (temp_len + (64 - (temp_len % 64)));

		/* the first two bytes are the frame length */

		buf[0] = (uint8_t)(m->m_pkthdr.len);
		buf[1] = (uint8_t)(m->m_pkthdr.len >> 8);

		usb2_copy_in(xfer->frbuffers, 0, buf, 2);

		usb2_m_copy_in(xfer->frbuffers, 2,
		    m, 0, m->m_pkthdr.len);

		usb2_bzero(xfer->frbuffers, temp_len,
		    total_len - temp_len);

		xfer->frlengths[0] = total_len;

		/*
		 * if there's a BPF listener, bounce a copy
		 * of this frame to him:
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
			sc->sc_flags |= KUE_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		ifp->if_oerrors++;
		return;

	}
}

static void
kue_start_cb(struct ifnet *ifp)
{
	struct kue_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	kue_start_transfers(sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
kue_start_transfers(struct kue_softc *sc)
{
	if ((sc->sc_flags & KUE_FLAG_LL_READY) &&
	    (sc->sc_flags & KUE_FLAG_HL_READY)) {

		/*
		 * start the USB transfers, if not already started:
		 */
		usb2_transfer_start(sc->sc_xfer[1]);
		usb2_transfer_start(sc->sc_xfer[0]);
	}
	return;
}

static void
kue_init_cb(void *arg)
{
	struct kue_softc *sc = arg;

	mtx_lock(&sc->sc_mtx);
	usb2_config_td_queue_command
	    (&sc->sc_config_td, &kue_cfg_pre_init,
	    &kue_cfg_init, 0, 0);
	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
kue_cfg_pre_init(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* immediate configuration */

	kue_cfg_pre_stop(sc, cc, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= KUE_FLAG_HL_READY;

	return;
}

static void
kue_cfg_init(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	/* set MAC address */
	kue_cfg_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SET_MAC,
	    0, cc->if_lladdr, ETHER_ADDR_LEN);

	/* I'm not sure how to tune these. */
#if 0
	/*
	 * Leave this one alone for now; setting it
	 * wrong causes lockups on some machines/controllers.
	 */
	kue_cfg_setword(sc, KUE_CMD_SET_SOFS, 1);
#endif
	kue_cfg_setword(sc, KUE_CMD_SET_URB_SIZE, 64);

	/* load the multicast filter */
	kue_cfg_promisc_upd(sc, cc, 0);

	sc->sc_flags |= (KUE_FLAG_READ_STALL |
	    KUE_FLAG_WRITE_STALL |
	    KUE_FLAG_LL_READY);

	kue_start_transfers(sc);
	return;
}

static int
kue_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct kue_softc *sc = ifp->if_softc;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &kue_config_copy,
				    &kue_cfg_promisc_upd, 0, 0);
			} else {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &kue_cfg_pre_init,
				    &kue_cfg_init, 0, 0);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				usb2_config_td_queue_command
				    (&sc->sc_config_td, &kue_cfg_pre_stop,
				    &kue_cfg_stop, 0, 0);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mtx_lock(&sc->sc_mtx);
		usb2_config_td_queue_command
		    (&sc->sc_config_td, &kue_config_copy,
		    &kue_cfg_promisc_upd, 0, 0);
		mtx_unlock(&sc->sc_mtx);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static void
kue_cfg_tick(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if ((ifp == NULL)) {
		/* not ready */
		return;
	}
	/* start stopped transfers, if any */

	kue_start_transfers(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
kue_watchdog(void *arg)
{
	struct kue_softc *sc = arg;

	usb2_config_td_queue_command
	    (&sc->sc_config_td, NULL, &kue_cfg_tick, 0, 0);

	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &kue_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);
	return;
}

static void
kue_cfg_pre_stop(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (cc) {
		/* copy the needed configuration */
		kue_config_copy(sc, cc, refcount);
	}
	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(KUE_FLAG_HL_READY |
	    KUE_FLAG_LL_READY);

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
kue_cfg_stop(struct kue_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t refcount)
{
	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
kue_shutdown(device_t dev)
{
	struct kue_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	usb2_config_td_queue_command
	    (&sc->sc_config_td, &kue_cfg_pre_stop,
	    &kue_cfg_stop, 0, 0);

	mtx_unlock(&sc->sc_mtx);

	return (0);
}
