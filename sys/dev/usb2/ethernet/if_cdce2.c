/*	$NetBSD: if_cdce.c,v 1.4 2004/10/24 12:50:54 augustss Exp $ */

/*-
 * Copyright (c) 1997, 1998, 1999, 2000-2003 Bill Paul <wpaul@windriver.com>
 * Copyright (c) 2003-2005 Craig Boston
 * Copyright (c) 2004 Daniel Hartmeier
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

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>
#include <dev/usb2/include/usb2_defs.h>

#define	usb2_config_td_cc usb2_ether_cc
#define	usb2_config_td_softc cdce_softc

#define	USB_DEBUG_VAR cdce_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_parse.h>
#include <dev/usb2/core/usb2_device.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_cdce2_reg.h>

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

static void cdce_start_cb(struct ifnet *ifp);
static void cdce_start_transfers(struct cdce_softc *sc);
static uint32_t cdce_m_crc32(struct mbuf *m, uint32_t src_offset, uint32_t src_len);
static void cdce_stop(struct cdce_softc *sc);
static int cdce_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data);
static void cdce_init_cb(void *arg);
static int cdce_ifmedia_upd_cb(struct ifnet *ifp);
static void cdce_ifmedia_sts_cb(struct ifnet *const ifp, struct ifmediareq *req);

#if USB_DEBUG
static int cdce_debug = 0;
static int cdce_force_512x4 = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, cdce, CTLFLAG_RW, 0, "USB cdce");
SYSCTL_INT(_hw_usb2_cdce, OID_AUTO, debug, CTLFLAG_RW, &cdce_debug, 0,
    "cdce debug level");
SYSCTL_INT(_hw_usb2_cdce, OID_AUTO, force_512x4, CTLFLAG_RW,
    &cdce_force_512x4, 0, "cdce force 512x4 protocol");
#endif

static const struct usb2_config cdce_config[CDCE_N_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.if_index = 0,
		/* Host Mode */
		.mh.frames = CDCE_512X4_FRAGS_MAX + 1,
		.mh.bufsize = (CDCE_512X4_FRAMES_MAX * MCLBYTES) + sizeof(struct usb2_cdc_mf_eth_512x4_header),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,.ext_buffer = 1,},
		.mh.callback = &cdce_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
		/* Device Mode */
		.md.frames = CDCE_512X4_FRAGS_MAX + 1,
		.md.bufsize = (CDCE_512X4_FRAMES_MAX * MCLBYTES) + sizeof(struct usb2_cdc_mf_eth_512x4_header),
		.md.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.ext_buffer = 1,},
		.md.callback = &cdce_bulk_read_callback,
		.md.timeout = 0,	/* no timeout */
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.if_index = 0,
		/* Host Mode */
		.mh.frames = CDCE_512X4_FRAGS_MAX + 1,
		.mh.bufsize = (CDCE_512X4_FRAMES_MAX * MCLBYTES) + sizeof(struct usb2_cdc_mf_eth_512x4_header),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.ext_buffer = 1,},
		.mh.callback = &cdce_bulk_read_callback,
		.mh.timeout = 0,	/* no timeout */
		/* Device Mode */
		.md.frames = CDCE_512X4_FRAGS_MAX + 1,
		.md.bufsize = (CDCE_512X4_FRAMES_MAX * MCLBYTES) + sizeof(struct usb2_cdc_mf_eth_512x4_header),
		.md.flags = {.pipe_bof = 1,.force_short_xfer = 1,.ext_buffer = 1,},
		.md.callback = &cdce_bulk_write_callback,
		.md.timeout = 10000,	/* 10 seconds */
	},

	[2] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.if_index = 1,
		/* Host Mode */
		.mh.bufsize = CDCE_IND_SIZE_MAX,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.no_pipe_ok = 1,},
		.mh.callback = &cdce_intr_read_callback,
		.mh.timeout = 0,
		/* Device Mode */
		.md.bufsize = CDCE_IND_SIZE_MAX,
		.md.flags = {.pipe_bof = 1,.force_short_xfer = 1,.no_pipe_ok = 1,},
		.md.callback = &cdce_intr_write_callback,
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

static int
cdce_attach(device_t dev)
{
	struct cdce_softc *sc = device_get_softc(dev);
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct usb2_interface *iface;
	const struct usb2_cdc_union_descriptor *ud;
	const struct usb2_cdc_ethernet_descriptor *ue;
	const struct usb2_interface_descriptor *id;
	struct ifnet *ifp;
	int error;
	uint8_t alt_index;
	uint8_t i;
	uint8_t eaddr[ETHER_ADDR_LEN];
	char eaddr_str[5 * ETHER_ADDR_LEN];	/* approx */

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);
	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	/* search for alternate settings */
	if (uaa->usb2_mode == USB_MODE_HOST) {

		struct usb2_descriptor *desc;
		struct usb2_config_descriptor *cd;

		cd = usb2_get_config_descriptor(uaa->device);
		desc = (void *)(uaa->iface->idesc);
		id = (void *)desc;
		i = id->bInterfaceNumber;
		alt_index = 0;
		while ((desc = usb2_desc_foreach(cd, desc))) {
			id = (void *)desc;
			if ((id->bDescriptorType == UDESC_INTERFACE) &&
			    (id->bLength >= sizeof(*id))) {
				if (id->bInterfaceNumber != i) {
					alt_index = 0;
					break;
				}
				if ((id->bInterfaceClass == UICLASS_CDC) &&
				    (id->bInterfaceSubClass ==
				    UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL) &&
				    (id->bInterfaceProtocol == UIPROTO_CDC_ETH_512X4)) {

					alt_index = id->bAlternateSetting;
					/*
					 * We want this alt setting hence
					 * the protocol supports multi
					 * sub-framing !
					 */
					break;
				}
			}
		}

		if (alt_index > 0) {

			error = usb2_set_alt_interface_index(uaa->device,
			    uaa->info.bIfaceIndex, alt_index);
			if (error) {
				device_printf(dev, "Could not set alternate "
				    "setting, error = %s\n", usb2_errstr(error));
				return (EINVAL);
			}
		}
	}
	/* get the interface subclass we are using */
	sc->sc_iface_protocol = uaa->iface->idesc->bInterfaceProtocol;
#if USB_DEBUG
	if (cdce_force_512x4) {
		sc->sc_iface_protocol = UIPROTO_CDC_ETH_512X4;
	}
#endif
	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name), "%s",
	    device_get_nameunit(dev));

	mtx_init(&sc->sc_mtx, "cdce lock", NULL, MTX_DEF | MTX_RECURSE);

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

	for (i = 0; i < 32; i++) {

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

	ifmedia_init(&sc->sc_ifmedia, 0,
	    &cdce_ifmedia_upd_cb,
	    &cdce_ifmedia_sts_cb);

	ue = usb2_find_descriptor
	    (uaa->device, NULL, uaa->info.bIfaceIndex,
	    UDESC_CS_INTERFACE, 0 - 1, UDESCSUB_CDC_ENF, 0 - 1);

	if ((ue == NULL) || (ue->bLength < sizeof(*ue))) {
		error = USB_ERR_INVAL;
	} else {
		error = usb2_req_get_string_any
		    (uaa->device, &Giant, eaddr_str,
		    sizeof(eaddr_str), ue->iMacAddress);
	}

	if (error) {

		/* fake MAC address */

		device_printf(dev, "faking MAC address\n");
		eaddr[0] = 0x2a;
		memcpy(&eaddr[1], &ticks, sizeof(uint32_t));
		eaddr[5] = sc->sc_unit;

	} else {

		bzero(eaddr, sizeof(eaddr));

		for (i = 0; i < (ETHER_ADDR_LEN * 2); i++) {

			char c = eaddr_str[i];

			if ((c >= '0') && (c <= '9')) {
				c -= '0';
			} else if (c != 0) {
				c -= 'A' - 10;
			} else {
				break;
			}

			c &= 0xf;

			if ((i & 1) == 0) {
				c <<= 4;
			}
			eaddr[i / 2] |= c;
		}

		if (uaa->usb2_mode == USB_MODE_DEVICE) {
			/*
			 * Do not use the same MAC address like the peer !
			 */
			eaddr[5] ^= 0xFF;
		}
	}

	ifp = if_alloc(IFT_ETHER);

	if (ifp == NULL) {
		device_printf(dev, "cannot if_alloc()\n");
		goto detach;
	}
	sc->sc_evilhack = ifp;

	ifp->if_softc = sc;
	if_initname(ifp, "cdce", sc->sc_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cdce_ioctl_cb;
	ifp->if_output = ether_output;
	ifp->if_start = cdce_start_cb;
	ifp->if_init = cdce_init_cb;
	ifp->if_baudrate = 11000000;
	if (sc->sc_iface_protocol == UIPROTO_CDC_ETH_512X4) {
		IFQ_SET_MAXLEN(&ifp->if_snd, CDCE_512X4_IFQ_MAXLEN);
		ifp->if_snd.ifq_drv_maxlen = CDCE_512X4_IFQ_MAXLEN;
	} else {
		IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
		ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	}
	IFQ_SET_READY(&ifp->if_snd);

	/* no IFM type for 11Mbps USB, so go with 10baseT */
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_10_T, 0, 0);
	ifmedia_set(&sc->sc_ifmedia, IFM_ETHER | IFM_10_T);

	sc->sc_ifp = ifp;

	ether_ifattach(ifp, eaddr);

	/* start the interrupt transfer, if any */
	mtx_lock(&sc->sc_mtx);
	usb2_transfer_start(sc->sc_xfer[2]);
	mtx_unlock(&sc->sc_mtx);

	return (0);			/* success */

detach:
	cdce_detach(dev);
	return (ENXIO);			/* failure */
}

static int
cdce_detach(device_t dev)
{
	struct cdce_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;

	mtx_lock(&sc->sc_mtx);

	cdce_stop(sc);

	ifp = sc->sc_ifp;

	mtx_unlock(&sc->sc_mtx);

	/* stop all USB transfers first */
	usb2_transfer_unsetup(sc->sc_xfer, CDCE_N_TRANSFER);

	/* get rid of any late children */
	bus_generic_detach(dev);

	if (ifp) {
		ether_ifdetach(ifp);
		if_free(ifp);
		ifmedia_removeall(&sc->sc_ifmedia);
	}
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
cdce_start_cb(struct ifnet *ifp)
{
	struct cdce_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);

	cdce_start_transfers(sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
cdce_start_transfers(struct cdce_softc *sc)
{
	if ((sc->sc_flags & CDCE_FLAG_LL_READY) &&
	    (sc->sc_flags & CDCE_FLAG_HL_READY)) {

		/*
		 * start the USB transfers, if not already started:
		 */
		usb2_transfer_start(sc->sc_xfer[1]);
		usb2_transfer_start(sc->sc_xfer[0]);
	}
	return;
}

static uint32_t
cdce_m_frags(struct mbuf *m)
{
	uint32_t temp = 1;

	while ((m = m->m_next)) {
		temp++;
	}
	return (temp);
}

static void
cdce_fwd_mq(struct cdce_softc *sc, struct cdce_mq *mq)
{
	struct mbuf *m;
	struct ifnet *ifp = sc->sc_ifp;

	if (mq->ifq_head) {

		mtx_unlock(&sc->sc_mtx);

		while (1) {

			_IF_DEQUEUE(mq, m);

			if (m == NULL)
				break;

			(ifp->if_input) (ifp, m);
		}

		mtx_lock(&sc->sc_mtx);
	}
	return;
}

static void
cdce_free_mq(struct cdce_mq *mq)
{
	struct mbuf *m;

	if (mq->ifq_head) {

		while (1) {

			_IF_DEQUEUE(mq, m);

			if (m == NULL)
				break;

			m_freem(m);
		}
	}
	return;
}

static void
cdce_bulk_write_512x4_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct mbuf *mt;
	uint16_t x;
	uint16_t y;
	uint16_t flen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete: "
		    "%u bytes in %u fragments and %u frames\n",
		    xfer->actlen, xfer->nframes, sc->sc_tx_mq.ifq_len);

		/* update packet counter */
		ifp->if_opackets += sc->sc_tx_mq.ifq_len;

		/* free all previous mbufs */
		cdce_free_mq(&sc->sc_tx_mq);

	case USB_ST_SETUP:
tr_setup:
		x = 0;			/* number of frames */
		y = 1;			/* number of fragments */

		while (x != CDCE_512X4_FRAMES_MAX) {

			IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

			if (m == NULL) {
				break;
			}
			if (m->m_pkthdr.len > MCLBYTES) {
				m_freem(m);
				ifp->if_oerrors++;
				continue;
			}
			if (cdce_m_frags(m) > CDCE_512X4_FRAME_FRAG_MAX) {
				mt = m_defrag(m, M_DONTWAIT);
				if (mt == NULL) {
					m_freem(m);
					ifp->if_oerrors++;
					continue;
				}
				m = mt;
			}
			_IF_ENQUEUE(&sc->sc_tx_mq, m);

			/*
			 * if there's a BPF listener, bounce a copy
			 * of this frame to him:
			 */
			BPF_MTAP(ifp, m);

#if (CDCE_512X4_FRAG_LENGTH_MASK < MCLBYTES)
#error "(CDCE_512X4_FRAG_LENGTH_MASK < MCLBYTES)"
#endif
			do {

				flen = m->m_len & CDCE_512X4_FRAG_LENGTH_MASK;
				xfer->frlengths[y] = m->m_len;
				usb2_set_frame_data(xfer, m->m_data, y);

				if (m->m_next == NULL) {
					flen |= CDCE_512X4_FRAG_LAST_MASK;
				}
				USETW(sc->sc_tx.hdr.wFragLength[y - 1], flen);

				y++;

			} while ((m = m->m_next));

			x++;
		}

		if (y == 1) {
			/* no data to transmit */
			break;
		}
		/* fill in Signature */
		sc->sc_tx.hdr.bSig[0] = 'F';
		sc->sc_tx.hdr.bSig[1] = 'L';

		/*
		 * We ensure that the header results in a short packet by
		 * making the length odd !
		 */
		USETW(sc->sc_tx.hdr.wFragLength[y - 1], 0);
		xfer->frlengths[0] = CDCE_512X4_FRAG_LENGTH_OFFSET + ((y - 1) * 2) + 1;
		usb2_set_frame_data(xfer, &sc->sc_tx.hdr, 0);
		xfer->nframes = y;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		/* update error counter */
		ifp->if_oerrors += sc->sc_tx_mq.ifq_len;

		/* free all previous mbufs */
		cdce_free_mq(&sc->sc_tx_mq);

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
	return;
}

static void
cdce_bulk_write_std_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct mbuf *mt;
	uint32_t crc;

	DPRINTFN(1, "\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete: "
		    "%u bytes in %u frames\n", xfer->actlen,
		    xfer->aframes);

		ifp->if_opackets++;

		/* free all previous mbufs */
		cdce_free_mq(&sc->sc_tx_mq);

	case USB_ST_SETUP:
tr_setup:
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL) {
			break;
		}
		if (sc->sc_flags & CDCE_FLAG_ZAURUS) {
			/*
			 * Zaurus wants a 32-bit CRC appended to
			 * every frame
			 */

			crc = cdce_m_crc32(m, 0, m->m_pkthdr.len);
			crc = htole32(crc);

			if (!m_append(m, 4, (void *)&crc)) {
				m_freem(m);
				ifp->if_oerrors++;
				goto tr_setup;
			}
		}
		if (m->m_len != m->m_pkthdr.len) {
			mt = m_defrag(m, M_DONTWAIT);
			if (mt == NULL) {
				m_freem(m);
				ifp->if_oerrors++;
				goto tr_setup;
			}
			m = mt;
		}
		if (m->m_pkthdr.len > MCLBYTES) {
			m->m_pkthdr.len = MCLBYTES;
		}
		_IF_ENQUEUE(&sc->sc_tx_mq, m);

		xfer->frlengths[0] = m->m_len;
		usb2_set_frame_data(xfer, m->m_data, 0);
		xfer->nframes = 1;

		/*
		 * if there's a BPF listener, bounce a copy
		 * of this frame to him:
		 */
		BPF_MTAP(ifp, m);

		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		/* free all previous mbufs */
		cdce_free_mq(&sc->sc_tx_mq);
		ifp->if_oerrors++;

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
	return;
}

static void
cdce_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;

	/* first call - set the correct callback */
	if (sc->sc_iface_protocol == UIPROTO_CDC_ETH_512X4) {
		xfer->flags.force_short_xfer = 0;
		xfer->callback = &cdce_bulk_write_512x4_callback;
	} else {
		xfer->callback = &cdce_bulk_write_std_callback;
	}
	(xfer->callback) (xfer);
	return;
}

static int32_t
#ifdef __FreeBSD__
cdce_m_crc32_cb(void *arg, void *src, uint32_t count)
#else
cdce_m_crc32_cb(void *arg, caddr_t src, uint32_t count)
#endif
{
	register uint32_t *p_crc = arg;

	*p_crc = crc32_raw(src, count, *p_crc);
	return (0);
}

static uint32_t
cdce_m_crc32(struct mbuf *m, uint32_t src_offset, uint32_t src_len)
{
	register int error;
	uint32_t crc = 0xFFFFFFFF;

	error = m_apply(m, src_offset, src_len, &cdce_m_crc32_cb, &crc);
	return (crc ^ 0xFFFFFFFF);
}

static void
cdce_stop(struct cdce_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* immediate configuration */

	if (ifp) {
		/* clear flags */
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
	sc->sc_flags &= ~(CDCE_FLAG_HL_READY |
	    CDCE_FLAG_LL_READY);

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[0]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static int
cdce_shutdown(device_t dev)
{
	struct cdce_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);

	cdce_stop(sc);

	mtx_unlock(&sc->sc_mtx);

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

static int
cdce_ioctl_cb(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cdce_softc *sc = ifp->if_softc;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				cdce_init_cb(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				cdce_stop(sc);
			}
		}
		mtx_unlock(&sc->sc_mtx);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, (void *)data,
		    &sc->sc_ifmedia, command);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static void
cdce_init_cb(void *arg)
{
	struct cdce_softc *sc = arg;
	struct ifnet *ifp;

	mtx_lock(&sc->sc_mtx);

	ifp = sc->sc_ifp;

	/* immediate configuration */

	cdce_stop(sc);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	sc->sc_flags |= (
	    CDCE_FLAG_LL_READY |
	    CDCE_FLAG_HL_READY);

	usb2_transfer_set_stall(sc->sc_xfer[0]);
	usb2_transfer_set_stall(sc->sc_xfer[1]);

	cdce_start_transfers(sc);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
cdce_bulk_read_512x4_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	void *data_ptr;
	uint32_t offset;
	uint16_t x;
	uint16_t y;
	uint16_t z;
	uint16_t rx_frags;
	uint16_t flen;
	uint8_t fwd_mq;
	uint8_t free_mq;

	fwd_mq = 0;
	free_mq = 0;
	rx_frags = 0;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("received %u bytes in %u frames\n",
		    xfer->actlen, xfer->aframes);

		/* check state */
		if (!(sc->sc_flags & CDCE_FLAG_RX_DATA)) {

			/* verify the header */
			if ((xfer->actlen < CDCE_512X4_FRAG_LENGTH_OFFSET) ||
			    (sc->sc_rx.hdr.bSig[0] != 'F') ||
			    (sc->sc_rx.hdr.bSig[1] != 'L')) {
				/* try to clear stall first */
				xfer->flags.stall_pipe = 1;
				goto tr_setup;
			}
			rx_frags = (xfer->actlen -
			    CDCE_512X4_FRAG_LENGTH_OFFSET) / 2;
			if (rx_frags != 0) {
				/* start receiving data */
				sc->sc_flags |= CDCE_FLAG_RX_DATA;
			}
			DPRINTF("doing %u fragments\n", rx_frags);

		} else {
			/* we are done receiving data */
			sc->sc_flags &= ~CDCE_FLAG_RX_DATA;
			fwd_mq = 1;
		}

	case USB_ST_SETUP:
tr_setup:
		if (xfer->flags.stall_pipe) {
			/* we are done */
			sc->sc_flags &= ~CDCE_FLAG_RX_DATA;
		}
		/* we expect a Multi Frame Ethernet Header */
		if (!(sc->sc_flags & CDCE_FLAG_RX_DATA)) {
			DPRINTF("expecting length header\n");
			usb2_set_frame_data(xfer, &sc->sc_rx.hdr, 0);
			xfer->frlengths[0] = sizeof(sc->sc_rx.hdr);
			xfer->nframes = 1;
			xfer->flags.short_xfer_ok = 1;
			usb2_start_hardware(xfer);
			free_mq = 1;
			break;
		}
		/* verify number of fragments */
		if (rx_frags > CDCE_512X4_FRAGS_MAX) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		/* check if the last fragment does not complete a frame */
		x = rx_frags - 1;
		flen = UGETW(sc->sc_rx.hdr.wFragLength[x]);
		if (!(flen & CDCE_512X4_FRAG_LAST_MASK)) {
			DPRINTF("no last frag mask\n");
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		/*
		 * Setup a new USB transfer chain to receive all the
		 * IP-frame fragments, automagically defragged :
		 */
		x = 0;
		y = 0;
		while (1) {

			z = x;
			offset = 0;

			/* precompute the frame length */
			while (1) {
				flen = UGETW(sc->sc_rx.hdr.wFragLength[z]);
				offset += (flen & CDCE_512X4_FRAG_LENGTH_MASK);
				if (flen & CDCE_512X4_FRAG_LAST_MASK) {
					break;
				}
				z++;
			}

			if (offset >= sizeof(struct ether_header)) {
				/*
				 * allocate a suitable memory buffer, if
				 * possible
				 */
				if (offset > (MCLBYTES - ETHER_ALIGN)) {
					/* try to clear stall first */
					xfer->flags.stall_pipe = 1;
					goto tr_setup;
				} if (offset > (MHLEN - ETHER_ALIGN)) {
					m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
				} else {
					m = m_gethdr(M_DONTWAIT, MT_DATA);
				}
			} else {
				m = NULL;	/* dump it */
			}

			DPRINTFN(17, "frame %u, length = %u \n", y, offset);

			/* check if we have a buffer */
			if (m) {
				m->m_data = USB_ADD_BYTES(m->m_data, ETHER_ALIGN);
				m->m_pkthdr.rcvif = ifp;
				m->m_pkthdr.len = m->m_len = offset;

				/* enqueue */
				_IF_ENQUEUE(&sc->sc_rx_mq, m);

				data_ptr = m->m_data;
				ifp->if_ipackets++;
			} else {
				data_ptr = sc->sc_rx.data;
				ifp->if_ierrors++;
			}

			/* setup the RX chain */
			offset = 0;
			while (1) {

				flen = UGETW(sc->sc_rx.hdr.wFragLength[x]);

				usb2_set_frame_data(xfer,
				    USB_ADD_BYTES(data_ptr, offset), x);

				xfer->frlengths[x] =
				    (flen & CDCE_512X4_FRAG_LENGTH_MASK);

				DPRINTFN(17, "length[%u] = %u\n",
				    x, xfer->frlengths[x]);

				offset += xfer->frlengths[x];

				x++;

				if (flen & CDCE_512X4_FRAG_LAST_MASK) {
					break;
				}
			}

			y++;

			if (x == rx_frags) {
				break;
			}
			if (y == CDCE_512X4_FRAMES_MAX) {
				/* try to clear stall first */
				xfer->flags.stall_pipe = 1;
				goto tr_setup;
			}
		}

		DPRINTF("nframes = %u\n", x);

		xfer->nframes = x;
		xfer->flags.short_xfer_ok = 0;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTF("error = %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		free_mq = 1;
		break;
	}

	/*
	 * At the end of a USB callback it is always safe to unlock
	 * the private mutex of a device!
	 *
	 *
	 * By safe we mean that if "usb2_transfer_stop()" is called,
	 * we will get a callback having the error code
	 * USB_ERR_CANCELLED.
	 */
	if (fwd_mq) {
		cdce_fwd_mq(sc, &sc->sc_rx_mq);
	}
	if (free_mq) {
		cdce_free_mq(&sc->sc_rx_mq);
	}
	return;
}

static void
cdce_bulk_read_std_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct mbuf *m_rx = NULL;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("received %u bytes in %u frames\n",
		    xfer->actlen, xfer->aframes);

		if (sc->sc_flags & CDCE_FLAG_ZAURUS) {

			/* Strip off CRC added by Zaurus */
			if (xfer->frlengths[0] >= MAX(4, 14)) {
				xfer->frlengths[0] -= 4;
			}
		}
		_IF_DEQUEUE(&sc->sc_rx_mq, m);

		if (m) {

			if (xfer->frlengths[0] < sizeof(struct ether_header)) {
				m_freem(m);
				goto tr_setup;
			}
			ifp->if_ipackets++;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = xfer->frlengths[0];
			m_rx = m;
		}
	case USB_ST_SETUP:
tr_setup:
		m = usb2_ether_get_mbuf();
		if (m == NULL) {

			/*
			 * We are out of mbufs and need to dump all the
			 * received data !
			 */
			usb2_set_frame_data(xfer, &sc->sc_rx.data, 0);
			xfer->frlengths[0] = sizeof(sc->sc_rx.data);

		} else {
			usb2_set_frame_data(xfer, m->m_data, 0);
			xfer->frlengths[0] = m->m_len;
			_IF_ENQUEUE(&sc->sc_rx_mq, m);
		}
		xfer->nframes = 1;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTF("error = %s\n",
		    usb2_errstr(xfer->error));

		/* free all mbufs */
		cdce_free_mq(&sc->sc_rx_mq);

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		return;
	}

	/*
	 * At the end of a USB callback it is always safe to unlock
	 * the private mutex of a device! That is why we do the
	 * "if_input" here, and not some lines up!
	 *
	 * By safe we mean that if "usb2_transfer_stop()" is called,
	 * we will get a callback having the error code
	 * USB_ERR_CANCELLED.
	 */
	if (m_rx) {
		mtx_unlock(&sc->sc_mtx);
		(ifp->if_input) (ifp, m_rx);
		mtx_lock(&sc->sc_mtx);
	}
	return;
}

static void
cdce_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct cdce_softc *sc = xfer->priv_sc;

	/* first call - set the correct callback */
	if (sc->sc_iface_protocol == UIPROTO_CDC_ETH_512X4) {
		xfer->callback = &cdce_bulk_read_512x4_callback;
	} else {
		xfer->callback = &cdce_bulk_read_std_callback;
	}
	(xfer->callback) (xfer);
	return;
}

static int
cdce_ifmedia_upd_cb(struct ifnet *ifp)
{
	/* no-op, cdce has only 1 possible media type */
	return (0);
}

static void
cdce_ifmedia_sts_cb(struct ifnet *const ifp, struct ifmediareq *req)
{

	req->ifm_status = IFM_AVALID | IFM_ACTIVE;
	req->ifm_active = IFM_ETHER | IFM_10_T;
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
	return;
}

static void
cdce_intr_write_callback(struct usb2_xfer *xfer)
{
	;				/* style fix */
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("Transferred %d bytes\n", xfer->actlen);

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
	return;
}

static int
cdce_handle_request(device_t dev,
    const void *req, void **pptr, uint16_t *plen,
    uint16_t offset, uint8_t is_complete)
{
	return (ENXIO);			/* use builtin handler */
}
