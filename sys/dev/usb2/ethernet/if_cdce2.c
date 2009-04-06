/*	$NetBSD: if_cdce.c,v 1.4 2004/10/24 12:50:54 augustss Exp $ */

/*-
 * Copyright (c) 1997, 1998, 1999, 2000-2003 Bill Paul <wpaul@windriver.com>
 * Copyright (c) 2003-2005 Craig Boston
 * Copyright (c) 2004 Daniel Hartmeier
 * Copyright (c) 2009 Hans Petter Selasky
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul, THE VOICES IN HIS HEAD OR
 * THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * USB Communication Device Class (Ethernet Networking Control Model)
 * http://www.usb.org/developers/devclass_docs/usbcdc11.pdf
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "usbdevs.h"
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR cdce_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_parse.h>
#include <dev/usb2/core/usb2_device.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_cdcereg.h>

static device_probe_t cdce_probe;
static device_attach_t cdce_attach;
static device_detach_t cdce_detach;
static device_shutdown_t cdce_shutdown;
static device_suspend_t cdce_suspend;
static device_resume_t cdce_resume;
static usb2_handle_request_t cdce_handle_request;

static usb2_callback_t cdce_bulk_write_callback;
static usb2_callback_t cdce_bulk_read_callback;
static usb2_callback_t cdce_intr_read_callback;
static usb2_callback_t cdce_intr_write_callback;

static usb2_ether_fn_t cdce_attach_post;
static usb2_ether_fn_t cdce_init;
static usb2_ether_fn_t cdce_stop;
static usb2_ether_fn_t cdce_start;
static usb2_ether_fn_t cdce_setmulti;
static usb2_ether_fn_t cdce_setpromisc;

static uint32_t	cdce_m_crc32(struct mbuf *, uint32_t, uint32_t);

#if USB_DEBUG
static int cdce_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, cdce, CTLFLAG_RW, 0, "USB CDC-Ethernet");
SYSCTL_INT(_hw_usb2_cdce, OID_AUTO, debug, CTLFLAG_RW, &cdce_debug, 0,
    "Debug level");
#endif

static const struct usb2_config cdce_config[CDCE_N_TRANSFER] = {

	[CDCE_BULK_A] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.if_index = 0,
		/* Host Mode */
		.mh.frames = CDCE_FRAMES_MAX,
		.mh.bufsize = (CDCE_FRAMES_MAX * MCLBYTES),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,.ext_buffer = 1,},
		.mh.callback = cdce_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
		/* Device Mode */
		.md.frames = CDCE_FRAMES_MAX,
		.md.bufsize = (CDCE_FRAMES_MAX * MCLBYTES),
		.md.flags = {.pipe_bof = 1,.short_frames_ok = 1,.short_xfer_ok = 1,.ext_buffer = 1,},
		.md.callback = cdce_bulk_read_callback,
		.md.timeout = 0,	/* no timeout */
	},

	[CDCE_BULK_B] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.if_index = 0,
		/* Host Mode */
		.mh.frames = CDCE_FRAMES_MAX,
		.mh.bufsize = (CDCE_FRAMES_MAX * MCLBYTES),
		.mh.flags = {.pipe_bof = 1,.short_frames_ok = 1,.short_xfer_ok = 1,.ext_buffer = 1,},
		.mh.callback = cdce_bulk_read_callback,
		.mh.timeout = 0,	/* no timeout */
		/* Device Mode */
		.md.frames = CDCE_FRAMES_MAX,
		.md.bufsize = (CDCE_FRAMES_MAX * MCLBYTES),
		.md.flags = {.pipe_bof = 1,.force_short_xfer = 1,.ext_buffer = 1,},
		.md.callback = cdce_bulk_write_callback,
		.md.timeout = 10000,	/* 10 seconds */
	},

	[CDCE_INTR] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.if_index = 1,
		/* Host Mode */
		.mh.bufsize = CDCE_IND_SIZE_MAX,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.no_pipe_ok = 1,},
		.mh.callback = cdce_intr_read_callback,
		.mh.timeout = 0,
		/* Device Mode */
		.md.bufsize = CDCE_IND_SIZE_MAX,
		.md.flags = {.pipe_bof = 1,.force_short_xfer = 1,.no_pipe_ok = 1,},
		.md.callback = cdce_intr_write_callback,
		.md.timeout = 10000,	/* 10 seconds */
	},
};

static device_method_t cdce_methods[] = {
	/* USB interface */
	DEVMETHOD(usb2_handle_request, cdce_handle_request),

	/* Device interface */
	DEVMETHOD(device_probe, cdce_probe),
	DEVMETHOD(device_attach, cdce_attach),
	DEVMETHOD(device_detach, cdce_detach),
	DEVMETHOD(device_suspend, cdce_suspend),
	DEVMETHOD(device_resume, cdce_resume),
	DEVMETHOD(device_shutdown, cdce_shutdown),

	{0, 0}
};

static driver_t cdce_driver = {
	.name = "cdce",
	.methods = cdce_methods,
	.size = sizeof(struct cdce_softc),
};

static devclass_t cdce_devclass;

DRIVER_MODULE(cdce, ushub, cdce_driver, cdce_devclass, NULL, 0);
MODULE_VERSION(cdce, 1);
MODULE_DEPEND(cdce, usb2_ethernet, 1, 1, 1);
MODULE_DEPEND(cdce, usb2_core, 1, 1, 1);
MODULE_DEPEND(cdce, ether, 1, 1, 1);

static const struct usb2_ether_methods cdce_ue_methods = {
	.ue_attach_post = cdce_attach_post,
	.ue_start = cdce_start,
	.ue_init = cdce_init,
	.ue_stop = cdce_stop,
	.ue_setmulti = cdce_setmulti,
	.ue_setpromisc = cdce_setpromisc,
};

static const struct usb2_device_id cdce_devs[] = {
	{USB_IF_CSI(UICLASS_CDC, UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL, 0)},
	{USB_IF_CSI(UICLASS_CDC, UISUBCLASS_MOBILE_DIRECT_LINE_MODEL, 0)},

	{USB_VPI(USB_VENDOR_ACERLABS, USB_PRODUCT_ACERLABS_M5632, CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_AMBIT, USB_PRODUCT_AMBIT_NTL_250, CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_COMPAQ, USB_PRODUCT_COMPAQ_IPAQLINUX, CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_GMATE, USB_PRODUCT_GMATE_YP3X00, CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_MOTOROLA2, USB_PRODUCT_MOTOROLA2_USBLAN, CDCE_FLAG_ZAURUS | CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_MOTOROLA2, USB_PRODUCT_MOTOROLA2_USBLAN2, CDCE_FLAG_ZAURUS | CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_NETCHIP, USB_PRODUCT_NETCHIP_ETHERNETGADGET, CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PL2501, CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_SHARP, USB_PRODUCT_SHARP_SL5500, CDCE_FLAG_ZAURUS)},
	{USB_VPI(USB_VENDOR_SHARP, USB_PRODUCT_SHARP_SL5600, CDCE_FLAG_ZAURUS | CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_SHARP, USB_PRODUCT_SHARP_SLA300, CDCE_FLAG_ZAURUS | CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_SHARP, USB_PRODUCT_SHARP_SLC700, CDCE_FLAG_ZAURUS | CDCE_FLAG_NO_UNION)},
	{USB_VPI(USB_VENDOR_SHARP, USB_PRODUCT_SHARP_SLC750, CDCE_FLAG_ZAURUS | CDCE_FLAG_NO_UNION)},
};

static int
cdce_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	return (usb2_lookup_id_by_uaa(cdce_devs, sizeof(cdce_devs), uaa));
}

static void
cdce_attach_post(struct usb2_ether *ue)
{
	/* no-op */
	return;
}

static int
cdce_attach(device_t dev)
{
	struct cdce_softc *sc = device_get_softc(dev);
	struct usb2_ether *ue = &sc->sc_ue;
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct usb2_interface *iface;
	const struct usb2_cdc_union_descriptor *ud;
	const struct usb2_interface_descriptor *id;
	const struct usb2_cdc_ethernet_descriptor *ued;
	int error;
	uint8_t i;
	char eaddr_str[5 * ETHER_ADDR_LEN];	/* approx */

	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (sc->sc_flags & CDCE_FLAG_NO_UNION) {
		sc->sc_ifaces_index[0] = uaa->info.bIfaceIndex;
		sc->sc_ifaces_index[1] = uaa->info.bIfaceIndex;
		sc->sc_data_iface_no = 0;	/* not used */
		goto alloc_transfers;
	}
	ud = usb2_find_descriptor
	    (uaa->device, NULL, uaa->info.bIfaceIndex,
	    UDESC_CS_INTERFACE, 0 - 1, UDESCSUB_CDC_UNION, 0 - 1);

	if ((ud == NULL) || (ud->bLength < sizeof(*ud))) {
		device_printf(dev, "no union descriptor!\n");
		goto detach;
	}
	sc->sc_data_iface_no = ud->bSlaveInterface[0];

	for (i = 0;; i++) {

		iface = usb2_get_iface(uaa->device, i);

		if (iface) {

			id = usb2_get_interface_descriptor(iface);

			if (id && (id->bInterfaceNumber ==
			    sc->sc_data_iface_no)) {
				sc->sc_ifaces_index[0] = i;
				sc->sc_ifaces_index[1] = uaa->info.bIfaceIndex;
				usb2_set_parent_iface(uaa->device, i, uaa->info.bIfaceIndex);
				break;
			}
		} else {
			device_printf(dev, "no data interface found!\n");
			goto detach;
		}
	}

	/*
	 * <quote>
	 *
	 *  The Data Class interface of a networking device shall have
	 *  a minimum of two interface settings. The first setting
	 *  (the default interface setting) includes no endpoints and
	 *  therefore no networking traffic is exchanged whenever the
	 *  default interface setting is selected. One or more
	 *  additional interface settings are used for normal
	 *  operation, and therefore each includes a pair of endpoints
	 *  (one IN, and one OUT) to exchange network traffic. Select
	 *  an alternate interface setting to initialize the network
	 *  aspects of the device and to enable the exchange of
	 *  network traffic.
	 *
	 * </quote>
	 *
	 * Some devices, most notably cable modems, include interface
	 * settings that have no IN or OUT endpoint, therefore loop
	 * through the list of all available interface settings
	 * looking for one with both IN and OUT endpoints.
	 */

alloc_transfers:

	for (i = 0; i != 32; i++) {

		error = usb2_set_alt_interface_index
		    (uaa->device, sc->sc_ifaces_index[0], i);

		if (error) {
			device_printf(dev, "no valid alternate "
			    "setting found!\n");
			goto detach;
		}
		error = usb2_transfer_setup
		    (uaa->device, sc->sc_ifaces_index,
		    sc->sc_xfer, cdce_config, CDCE_N_TRANSFER,
		    sc, &sc->sc_mtx);

		if (error == 0) {
			break;
		}
	}

	ued = usb2_find_descriptor
	    (uaa->device, NULL, uaa->info.bIfaceIndex,
	    UDESC_CS_INTERFACE, 0 - 1, UDESCSUB_CDC_ENF, 0 - 1);

	if ((ued == NULL) || (ued->bLength < sizeof(*ued))) {
		error = USB_ERR_INVAL;
	} else {
		error = usb2_req_get_string_any(uaa->device, NULL, 
		    eaddr_str, sizeof(eaddr_str), ued->iMacAddress);
	}

	if (error) {

		/* fake MAC address */

		device_printf(dev, "faking MAC address\n");
		sc->sc_ue.ue_eaddr[0] = 0x2a;
		memcpy(&sc->sc_ue.ue_eaddr[1], &ticks, sizeof(uint32_t));
		sc->sc_ue.ue_eaddr[5] = device_get_unit(dev);

	} else {

		bzero(sc->sc_ue.ue_eaddr, sizeof(sc->sc_ue.ue_eaddr));

		for (i = 0; i != (ETHER_ADDR_LEN * 2); i++) {

			char c = eaddr_str[i];

			if ('0' <= c && c <= '9')
				c -= '0';
			else if (c != 0)
				c -= 'A' - 10;
			else
				break;

			c &= 0xf;

			if ((i & 1) == 0)
				c <<= 4;
			sc->sc_ue.ue_eaddr[i / 2] |= c;
		}

		if (uaa->usb2_mode == USB_MODE_DEVICE) {
			/*
			 * Do not use the same MAC address like the peer !
			 */
			sc->sc_ue.ue_eaddr[5] ^= 0xFF;
		}
	}

	ue->ue_sc = sc;
	ue->ue_dev = dev;
	ue->ue_udev = uaa->device;
	ue->ue_mtx = &sc->sc_mtx;
	ue->ue_methods = &cdce_ue_methods;

	error = usb2_ether_ifattach(ue);
	if (error) {
		device_printf(dev, "could not attach interface\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	cdce_detach(dev);
	return (ENXIO);			/* failure */
}

static int
cdce_detach(device_t dev)
{
	struct cdce_softc *sc = device_get_softc(dev);
	struct usb2_ether *ue = &sc->sc_ue;

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, CDCE_N_TRANSFER);
	usb2_ether_ifdetach(ue);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
cdce_start(struct usb2_ether *ue)
{
	struct cdce_softc *sc = usb2_ether_getsc(ue);

	/*
	 * Start the USB transfers, if not already started:
	 */
	usb2_transfer_start(sc->sc_xfer[CDCE_BULK_B]);
	usb2_transfer_start(sc->sc_xfer[CDCE_BULK_A]);
}

static void
cdce_free_queue(struct mbuf **ppm, uint8_t n)
{
	uint8_t x;
	for (x = 0; x != n; x++) {
		if (ppm[x] != NULL) {
			m_freem(ppm[x]);
			ppm[x] = NULL;
		}
	}
}

static void
cdce_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = usb2_ether_getifp(&sc->sc_ue);
	struct mbuf *m;
	struct mbuf *mt;
	uint32_t crc;
	uint8_t x;

	DPRINTFN(1, "\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete: "
		    "%u bytes in %u frames\n", xfer->actlen,
		    xfer->aframes);

		ifp->if_opackets++;

		/* free all previous TX buffers */
		cdce_free_queue(sc->sc_tx_buf, CDCE_FRAMES_MAX);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		for (x = 0; x != CDCE_FRAMES_MAX; x++) {

			IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

			if (m == NULL)
				break;

			if (sc->sc_flags & CDCE_FLAG_ZAURUS) {
				/*
				 * Zaurus wants a 32-bit CRC appended
				 * to every frame
				 */

				crc = cdce_m_crc32(m, 0, m->m_pkthdr.len);
				crc = htole32(crc);

				if (!m_append(m, 4, (void *)&crc)) {
					m_freem(m);
					ifp->if_oerrors++;
					continue;
				}
			}
			if (m->m_len != m->m_pkthdr.len) {
				mt = m_defrag(m, M_DONTWAIT);
				if (mt == NULL) {
					m_freem(m);
					ifp->if_oerrors++;
					continue;
				}
				m = mt;
			}
			if (m->m_pkthdr.len > MCLBYTES) {
				m->m_pkthdr.len = MCLBYTES;
			}
			sc->sc_tx_buf[x] = m;
			xfer->frlengths[x] = m->m_len;
			usb2_set_frame_data(xfer, m->m_data, x);

			/*
			 * If there's a BPF listener, bounce a copy of
			 * this frame to him:
			 */
			BPF_MTAP(ifp, m);
		}
		if (x != 0) {
			xfer->nframes = x;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		/* free all previous TX buffers */
		cdce_free_queue(sc->sc_tx_buf, CDCE_FRAMES_MAX);

		/* count output errors */
		ifp->if_oerrors++;

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
}

static int32_t
cdce_m_crc32_cb(void *arg, void *src, uint32_t count)
{
	uint32_t *p_crc = arg;

	*p_crc = crc32_raw(src, count, *p_crc);
	return (0);
}

static uint32_t
cdce_m_crc32(struct mbuf *m, uint32_t src_offset, uint32_t src_len)
{
	uint32_t crc = 0xFFFFFFFF;
	int error;

	error = m_apply(m, src_offset, src_len, cdce_m_crc32_cb, &crc);
	return (crc ^ 0xFFFFFFFF);
}

static void
cdce_init(struct usb2_ether *ue)
{
	struct cdce_softc *sc = usb2_ether_getsc(ue);
	struct ifnet *ifp = usb2_ether_getifp(ue);

	CDCE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	/* start interrupt transfer */
	usb2_transfer_start(sc->sc_xfer[CDCE_INTR]);

	/* stall data write direction, which depends on USB mode */
	if (usb2_get_mode(sc->sc_ue.ue_udev) == USB_MODE_HOST)
		usb2_transfer_set_stall(sc->sc_xfer[CDCE_BULK_A]);
	else
		usb2_transfer_set_stall(sc->sc_xfer[CDCE_BULK_B]);

	/* start data transfers */
	cdce_start(ue);
}

static void
cdce_stop(struct usb2_ether *ue)
{
	struct cdce_softc *sc = usb2_ether_getsc(ue);
	struct ifnet *ifp = usb2_ether_getifp(ue);

	CDCE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[CDCE_BULK_A]);
	usb2_transfer_stop(sc->sc_xfer[CDCE_BULK_B]);
	usb2_transfer_stop(sc->sc_xfer[CDCE_INTR]);
}

static void
cdce_setmulti(struct usb2_ether *ue)
{
	/* no-op */
	return;
}

static void
cdce_setpromisc(struct usb2_ether *ue)
{
	/* no-op */
	return;
}

static int
cdce_shutdown(device_t dev)
{
	struct cdce_softc *sc = device_get_softc(dev);

	usb2_ether_ifshutdown(&sc->sc_ue);

	return (0);
}

static int
cdce_suspend(device_t dev)
{
	device_printf(dev, "Suspending\n");
	return (0);
}

static int
cdce_resume(device_t dev)
{
	device_printf(dev, "Resuming\n");
	return (0);
}

static void
cdce_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;
	struct mbuf *m;
	uint8_t x;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("received %u bytes in %u frames\n",
		    xfer->actlen, xfer->aframes);

		for (x = 0; x != xfer->aframes; x++) {

			m = sc->sc_rx_buf[x];
			sc->sc_rx_buf[x] = NULL;

			/* Strip off CRC added by Zaurus, if any */
			if ((sc->sc_flags & CDCE_FLAG_ZAURUS) &&
			    (xfer->frlengths[x] >= 14))
				xfer->frlengths[x] -= 4;

			if (xfer->frlengths[x] < sizeof(struct ether_header)) {
				m_freem(m);
				continue;
			}
			/* queue up mbuf */
			usb2_ether_rxmbuf(&sc->sc_ue, m, xfer->frlengths[x]);
		}

		/* FALLTHROUGH */
	case USB_ST_SETUP:
		/* 
		 * TODO: Implement support for multi frame transfers,
		 * when the USB hardware supports it.
		 */
		for (x = 0; x != 1; x++) {
			if (sc->sc_rx_buf[x] == NULL) {
				m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
				if (m == NULL)
					goto tr_stall;
				sc->sc_rx_buf[x] = m;
				/* adjust for ethernet */
				m->m_len = m->m_pkthdr.len = MCLBYTES;
				m_adj(m, ETHER_ALIGN);
			} else {
				m = sc->sc_rx_buf[x];
			}

			usb2_set_frame_data(xfer, m->m_data, x);
			xfer->frlengths[x] = m->m_len;
		}
		/* set number of frames and start hardware */
		xfer->nframes = x;
		usb2_start_hardware(xfer);
		/* flush any received frames */
		usb2_ether_rxflush(&sc->sc_ue);
		break;

	default:			/* Error */
		DPRINTF("error = %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
tr_stall:
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			xfer->nframes = 0;
			usb2_start_hardware(xfer);
			break;
		}

		/* need to free the RX-mbufs when we are cancelled */
		cdce_free_queue(sc->sc_rx_buf, CDCE_FRAMES_MAX);
		break;
	}
}

static void
cdce_intr_read_callback(struct usb2_xfer *xfer)
{
	;				/* style fix */
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("Received %d bytes\n",
		    xfer->actlen);

		/* TODO: decode some indications */

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		xfer->frlengths[0] = xfer->max_data_length;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* start clear stall */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
}

static void
cdce_intr_write_callback(struct usb2_xfer *xfer)
{
	;				/* style fix */
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("Transferred %d bytes\n", xfer->actlen);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
#if 0
		xfer->frlengths[0] = XXX;
		usb2_start_hardware(xfer);
#endif
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* start clear stall */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
}

static int
cdce_handle_request(device_t dev,
    const void *req, void **pptr, uint16_t *plen,
    uint16_t offset, uint8_t is_complete)
{
	return (ENXIO);			/* use builtin handler */
}
