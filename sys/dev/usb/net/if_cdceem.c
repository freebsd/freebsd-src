/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012 Ben Gray <bgray@freebsd.org>.
 * Copyright (C) 2018 The FreeBSD Foundation.
 * Copyright (c) 2019 Edward Tomasz Napierala <trasz@FreeBSD.org>
 *
 * This software was developed by Arshan Khanifar <arshankhanifar@gmail.com>
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * Universal Serial Bus Communications Class Subclass Specification
 * for Ethernet Emulation Model Devices:
 *
 * https://usb.org/sites/default/files/CDC_EEM10.pdf
 */

#include <sys/gsb_crc32.h>
#include <sys/eventhandler.h>
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/bus.h>
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

#include <net/if.h>
#include <net/if_var.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_cdc.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR cdceem_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_msctest.h>
#include "usb_if.h"

#include <dev/usb/net/usb_ethernet.h>

#define	CDCEEM_FRAMES_MAX	1
#define	CDCEEM_ECHO_MAX		1024

#define	CDCEEM_ECHO_PAYLOAD	\
    "ICH DALEKOPIS FALSZUJE GDY PROBY XQV NIE WYTRZYMUJE 1234567890"

enum {
	CDCEEM_BULK_RX,
	CDCEEM_BULK_TX,
	CDCEEM_N_TRANSFER,
};

struct cdceem_softc {
	struct usb_ether	sc_ue;
	struct mtx		sc_mtx;
	int			sc_flags;
	struct usb_xfer		*sc_xfer[CDCEEM_N_TRANSFER];
	size_t			sc_echo_len;
	char			sc_echo_buffer[CDCEEM_ECHO_MAX];
};

#define	CDCEEM_SC_FLAGS_ECHO_RESPONSE_PENDING	0x1
#define	CDCEEM_SC_FLAGS_ECHO_PENDING		0x2

static SYSCTL_NODE(_hw_usb, OID_AUTO, cdceem, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB CDC EEM");
static int cdceem_debug = 1;
SYSCTL_INT(_hw_usb_cdceem, OID_AUTO, debug, CTLFLAG_RWTUN,
    &cdceem_debug, 0, "Debug level");
static int cdceem_send_echoes = 0;
SYSCTL_INT(_hw_usb_cdceem, OID_AUTO, send_echoes, CTLFLAG_RWTUN,
    &cdceem_send_echoes, 0, "Send an Echo command");
static int cdceem_send_fake_crc = 0;
SYSCTL_INT(_hw_usb_cdceem, OID_AUTO, send_fake_crc, CTLFLAG_RWTUN,
    &cdceem_send_fake_crc, 0, "Use 0xdeadbeef instead of CRC");

#define	CDCEEM_DEBUG(S, X, ...)						\
	do {								\
		if (cdceem_debug > 1) {					\
			device_printf(S->sc_ue.ue_dev, "%s: " X "\n",	\
			    __func__, ## __VA_ARGS__);			\
		}							\
	} while (0)

#define	CDCEEM_WARN(S, X, ...)						\
	do {								\
		if (cdceem_debug > 0) {					\
			device_printf(S->sc_ue.ue_dev,			\
			    "WARNING: %s: " X "\n",			\
			    __func__, ## __VA_ARGS__);			\
		}							\
	} while (0)

#define	CDCEEM_LOCK(X)				mtx_lock(&(X)->sc_mtx)
#define	CDCEEM_UNLOCK(X)			mtx_unlock(&(X)->sc_mtx)

#define	CDCEEM_TYPE_CMD				(0x1 << 15)

#define	CDCEEM_CMD_MASK				(0x7 << 11)

#define	CDCEEM_CMD_ECHO				(0x0 << 11)
#define	CDCEEM_CMD_ECHO_RESPONSE		(0x1 << 11)
#define	CDCEEM_CMD_SUSPEND_HINT			(0x2 << 11)
#define	CDCEEM_CMD_RESPONSE_HINT		(0x3 << 11)
#define	CDCEEM_CMD_RESPONSE_COMPLETE_HINT	(0x4 << 11)
#define	CDCEEM_CMD_TICKLE			(0x5 << 11)

#define	CDCEEM_CMD_RESERVED			(0x1 << 14)

#define	CDCEEM_ECHO_LEN_MASK			0x3ff

#define	CDCEEM_DATA_CRC				(0x1 << 14)
#define	CDCEEM_DATA_LEN_MASK			0x3fff

static device_probe_t cdceem_probe;
static device_attach_t cdceem_attach;
static device_detach_t cdceem_detach;
static device_suspend_t cdceem_suspend;
static device_resume_t cdceem_resume;

static usb_callback_t cdceem_bulk_write_callback;
static usb_callback_t cdceem_bulk_read_callback;

static uether_fn_t cdceem_attach_post;
static uether_fn_t cdceem_init;
static uether_fn_t cdceem_stop;
static uether_fn_t cdceem_start;
static uether_fn_t cdceem_setmulti;
static uether_fn_t cdceem_setpromisc;

static uint32_t	cdceem_m_crc32(struct mbuf *, uint32_t, uint32_t);

static const struct usb_config cdceem_config[CDCEEM_N_TRANSFER] = {
	[CDCEEM_BULK_TX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_TX,
		.bufsize = 16 * (MCLBYTES + 16),
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = cdceem_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
		.usb_mode = USB_MODE_DUAL,
	},

	[CDCEEM_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_RX,
		.bufsize = 20480,	/* bytes */
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = cdceem_bulk_read_callback,
		.timeout = 0,	/* no timeout */
		.usb_mode = USB_MODE_DUAL,
	},
};

static device_method_t cdceem_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, cdceem_probe),
	DEVMETHOD(device_attach, cdceem_attach),
	DEVMETHOD(device_detach, cdceem_detach),
	DEVMETHOD(device_suspend, cdceem_suspend),
	DEVMETHOD(device_resume, cdceem_resume),

	DEVMETHOD_END
};

static driver_t cdceem_driver = {
	.name = "cdceem",
	.methods = cdceem_methods,
	.size = sizeof(struct cdceem_softc),
};

static const STRUCT_USB_DUAL_ID cdceem_dual_devs[] = {
	{USB_IFACE_CLASS(UICLASS_CDC),
		USB_IFACE_SUBCLASS(UISUBCLASS_ETHERNET_EMULATION_MODEL),
		0},
};

DRIVER_MODULE(cdceem, uhub, cdceem_driver, NULL, NULL);
MODULE_VERSION(cdceem, 1);
MODULE_DEPEND(cdceem, uether, 1, 1, 1);
MODULE_DEPEND(cdceem, usb, 1, 1, 1);
MODULE_DEPEND(cdceem, ether, 1, 1, 1);
USB_PNP_DUAL_INFO(cdceem_dual_devs);

static const struct usb_ether_methods cdceem_ue_methods = {
	.ue_attach_post = cdceem_attach_post,
	.ue_start = cdceem_start,
	.ue_init = cdceem_init,
	.ue_stop = cdceem_stop,
	.ue_setmulti = cdceem_setmulti,
	.ue_setpromisc = cdceem_setpromisc,
};

static int
cdceem_probe(device_t dev)
{
	struct usb_attach_arg *uaa;
	int error;

	uaa = device_get_ivars(dev);
	error = usbd_lookup_id_by_uaa(cdceem_dual_devs,
	    sizeof(cdceem_dual_devs), uaa);

	return (error);
}

static void
cdceem_attach_post(struct usb_ether *ue)
{

	return;
}

static int
cdceem_attach(device_t dev)
{
	struct cdceem_softc *sc;
	struct usb_ether *ue;
	struct usb_attach_arg *uaa;
	int error;
	uint8_t iface_index;

	sc = device_get_softc(dev);
	ue = &sc->sc_ue;
	uaa = device_get_ivars(dev);

	device_set_usb_desc(dev);

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Setup the endpoints. */
	iface_index = 0;
	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    cdceem_config, CDCEEM_N_TRANSFER, sc, &sc->sc_mtx);
	if (error != 0) {
		CDCEEM_WARN(sc,
		    "allocating USB transfers failed, error %d", error);
		mtx_destroy(&sc->sc_mtx);
		return (error);
	}

	/* Random MAC address. */
	arc4rand(ue->ue_eaddr, ETHER_ADDR_LEN, 0);
	ue->ue_eaddr[0] &= ~0x01;	/* unicast */
	ue->ue_eaddr[0] |= 0x02;	/* locally administered */

	ue->ue_sc = sc;
	ue->ue_dev = dev;
	ue->ue_udev = uaa->device;
	ue->ue_mtx = &sc->sc_mtx;
	ue->ue_methods = &cdceem_ue_methods;

	error = uether_ifattach(ue);
	if (error != 0) {
		CDCEEM_WARN(sc, "could not attach interface, error %d", error);
		usbd_transfer_unsetup(sc->sc_xfer, CDCEEM_N_TRANSFER);
		mtx_destroy(&sc->sc_mtx);
		return (error);
	}

	return (0);
}

static int
cdceem_detach(device_t dev)
{
	struct cdceem_softc *sc = device_get_softc(dev);
	struct usb_ether *ue = &sc->sc_ue;

	/* Stop all USB transfers first. */
	usbd_transfer_unsetup(sc->sc_xfer, CDCEEM_N_TRANSFER);
	uether_ifdetach(ue);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
cdceem_handle_cmd(struct usb_xfer *xfer, uint16_t hdr, int *offp)
{
	struct cdceem_softc *sc;
	struct usb_page_cache *pc;
	int actlen, off;
	uint16_t pktlen;

	off = *offp;
	sc = usbd_xfer_softc(xfer);
	pc = usbd_xfer_get_frame(xfer, 0);
	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	if (hdr & CDCEEM_CMD_RESERVED) {
		CDCEEM_WARN(sc, "received command header %#x "
		    "with Reserved bit set; ignoring", hdr);
		return;
	}

	switch (hdr & CDCEEM_CMD_MASK) {
	case CDCEEM_CMD_ECHO:
		pktlen = hdr & CDCEEM_ECHO_LEN_MASK;
		CDCEEM_DEBUG(sc, "received Echo, length %d", pktlen);

		if (pktlen > (actlen - off)) {
			CDCEEM_WARN(sc,
			    "bad Echo length %d, should be at most %d",
			    pktlen, actlen - off);
			break;
		}

		if (pktlen > sizeof(sc->sc_echo_buffer)) {
			CDCEEM_WARN(sc,
			    "Echo length %u too big, must be less than %zd",
			    pktlen, sizeof(sc->sc_echo_buffer));
			break;
		}

		sc->sc_flags |= CDCEEM_SC_FLAGS_ECHO_RESPONSE_PENDING;
		sc->sc_echo_len = pktlen;
		usbd_copy_out(pc, off, sc->sc_echo_buffer, pktlen);
		off += pktlen;
		break;

	case CDCEEM_CMD_ECHO_RESPONSE:
		pktlen = hdr & CDCEEM_ECHO_LEN_MASK;
		CDCEEM_DEBUG(sc, "received Echo Response, length %d", pktlen);

		if (pktlen > (actlen - off)) {
			CDCEEM_WARN(sc,
			    "bad Echo Response length %d, "
			    "should be at most %d",
			    pktlen, actlen - off);
			break;
		}

		if (pktlen != sizeof(CDCEEM_ECHO_PAYLOAD)) {
			CDCEEM_WARN(sc, "received Echo Response with bad "
			    "length %hu, should be %zd",
			    pktlen, sizeof(CDCEEM_ECHO_PAYLOAD));
			break;
		}

		usbd_copy_out(pc, off, sc->sc_echo_buffer, pktlen);
		off += pktlen;

		if (memcmp(sc->sc_echo_buffer, CDCEEM_ECHO_PAYLOAD,
		    sizeof(CDCEEM_ECHO_PAYLOAD)) != 0) {
			CDCEEM_WARN(sc,
			    "received Echo Response payload does not match");
		} else {
			CDCEEM_DEBUG(sc, "received Echo Response is valid");
		}
		break;

	case CDCEEM_CMD_SUSPEND_HINT:
		CDCEEM_DEBUG(sc, "received SuspendHint; ignoring");
		break;

	case CDCEEM_CMD_RESPONSE_HINT:
		CDCEEM_DEBUG(sc, "received ResponseHint; ignoring");
		break;

	case CDCEEM_CMD_RESPONSE_COMPLETE_HINT:
		CDCEEM_DEBUG(sc, "received ResponseCompleteHint; ignoring");
		break;

	case CDCEEM_CMD_TICKLE:
		CDCEEM_DEBUG(sc, "received Tickle; ignoring");
		break;

	default:
		CDCEEM_WARN(sc,
		    "received unknown command %u, header %#x; ignoring",
		    (hdr & CDCEEM_CMD_MASK >> 11), hdr);
		break;
	}

	*offp = off;
}

static void
cdceem_handle_data(struct usb_xfer *xfer, uint16_t hdr, int *offp)
{
	struct cdceem_softc *sc;
	struct usb_page_cache *pc;
	struct usb_ether *ue;
	if_t ifp;
	struct mbuf *m;
	uint32_t computed_crc, received_crc;
	int pktlen;
	int actlen;
	int off;

	off = *offp;
	sc = usbd_xfer_softc(xfer);
	pc = usbd_xfer_get_frame(xfer, 0);
	ue = &sc->sc_ue;
	ifp = uether_getifp(ue);
	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	pktlen = hdr & CDCEEM_DATA_LEN_MASK;
	CDCEEM_DEBUG(sc, "received Data, CRC %s, length %d",
	    (hdr & CDCEEM_DATA_CRC) ? "valid" : "absent",
	    pktlen);

	if (pktlen < (ETHER_HDR_LEN + 4)) {
		CDCEEM_WARN(sc,
		    "bad ethernet frame length %d, should be at least %d",
		    pktlen, ETHER_HDR_LEN);
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		return;
	}

	if (pktlen > (actlen - off)) {
		CDCEEM_WARN(sc,
		    "bad ethernet frame length %d, should be at most %d",
		    pktlen, actlen - off);
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		return;
	}

	m = uether_newbuf();
	if (m == NULL) {
		CDCEEM_WARN(sc, "uether_newbuf() failed");
		if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
		return;
	}

	pktlen -= 4; /* Subtract the CRC. */

	if (pktlen > m->m_len) {
		CDCEEM_WARN(sc, "buffer too small %d vs %d bytes",
		    pktlen, m->m_len);
		if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
		m_freem(m);
		return;
	}
	usbd_copy_out(pc, off, mtod(m, uint8_t *), pktlen);
	off += pktlen;

	usbd_copy_out(pc, off, &received_crc, sizeof(received_crc));
	off += sizeof(received_crc);

	if (hdr & CDCEEM_DATA_CRC) {
		computed_crc = cdceem_m_crc32(m, 0, pktlen);
	} else {
		computed_crc = be32toh(0xdeadbeef);
	}

	if (received_crc != computed_crc) {
		CDCEEM_WARN(sc,
		    "received Data packet with wrong CRC %#x, expected %#x",
		    received_crc, computed_crc);
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		m_freem(m);
		return;
	} else {
		CDCEEM_DEBUG(sc, "received correct CRC %#x", received_crc);
	}

	uether_rxmbuf(ue, m, pktlen);
	*offp = off;
}

static void
cdceem_bulk_read_callback(struct usb_xfer *xfer, usb_error_t usb_error)
{
	struct cdceem_softc *sc;
	struct usb_page_cache *pc;
	int actlen, aframes, off;
	uint16_t hdr;

	sc = usbd_xfer_softc(xfer);
	usbd_xfer_status(xfer, &actlen, NULL, &aframes, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		CDCEEM_DEBUG(sc,
		    "received %u bytes in %u frames", actlen, aframes);

		pc = usbd_xfer_get_frame(xfer, 0);
		off = 0;

		while ((off + sizeof(hdr)) <= actlen) {
			usbd_copy_out(pc, off, &hdr, sizeof(hdr));
			CDCEEM_DEBUG(sc, "hdr = %#x", hdr);
			off += sizeof(hdr);

			if (hdr == 0) {
				CDCEEM_DEBUG(sc, "received Zero Length EEM");
				continue;
			}

			hdr = le16toh(hdr);

			if ((hdr & CDCEEM_TYPE_CMD) != 0) {
				cdceem_handle_cmd(xfer, hdr, &off);
			} else {
				cdceem_handle_data(xfer, hdr, &off);
			}

			KASSERT(off <= actlen,
			    ("%s: went past the buffer, off %d, actlen %d",
			     __func__, off, actlen));
		}

		/* FALLTHROUGH */
	case USB_ST_SETUP:
		CDCEEM_DEBUG(sc, "setup");
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		uether_rxflush(&sc->sc_ue);
		break;

	default:
		CDCEEM_WARN(sc, "USB_ST_ERROR: %s", usbd_errstr(usb_error));

		if (usb_error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
cdceem_send_echo(struct usb_xfer *xfer, int *offp)
{
	struct cdceem_softc *sc;
	struct usb_page_cache *pc;
	int maxlen __diagused, off;
	uint16_t hdr;

	off = *offp;
	sc = usbd_xfer_softc(xfer);
	pc = usbd_xfer_get_frame(xfer, 0);
	maxlen = usbd_xfer_max_len(xfer);

	CDCEEM_DEBUG(sc, "sending Echo, length %zd",
	    sizeof(CDCEEM_ECHO_PAYLOAD));

	KASSERT(off + sizeof(hdr) + sizeof(CDCEEM_ECHO_PAYLOAD) < maxlen,
	    ("%s: out of space; have %d, need %zd", __func__, maxlen,
	    off + sizeof(hdr) + sizeof(CDCEEM_ECHO_PAYLOAD)));

	hdr = 0;
	hdr |= CDCEEM_TYPE_CMD;
	hdr |= CDCEEM_CMD_ECHO;
	hdr |= sizeof(CDCEEM_ECHO_PAYLOAD);
	CDCEEM_DEBUG(sc, "hdr = %#x", hdr);
	hdr = htole16(hdr);

	usbd_copy_in(pc, off, &hdr, sizeof(hdr));
	off += sizeof(hdr);

	usbd_copy_in(pc, off, CDCEEM_ECHO_PAYLOAD,
	    sizeof(CDCEEM_ECHO_PAYLOAD));
	off += sizeof(CDCEEM_ECHO_PAYLOAD);

	sc->sc_flags &= ~CDCEEM_SC_FLAGS_ECHO_PENDING;

	*offp = off;
}

static void
cdceem_send_echo_response(struct usb_xfer *xfer, int *offp)
{
	struct cdceem_softc *sc;
	struct usb_page_cache *pc;
	int maxlen __diagused, off;
	uint16_t hdr;

	off = *offp;
	sc = usbd_xfer_softc(xfer);
	pc = usbd_xfer_get_frame(xfer, 0);
	maxlen = usbd_xfer_max_len(xfer);

	KASSERT(off + sizeof(hdr) + sc->sc_echo_len < maxlen,
	    ("%s: out of space; have %d, need %zd", __func__, maxlen,
	    off + sizeof(hdr) + sc->sc_echo_len));

	CDCEEM_DEBUG(sc, "sending Echo Response, length %zd", sc->sc_echo_len);

	hdr = 0;
	hdr |= CDCEEM_TYPE_CMD;
	hdr |= CDCEEM_CMD_ECHO_RESPONSE;
	hdr |= sc->sc_echo_len;
	CDCEEM_DEBUG(sc, "hdr = %#x", hdr);
	hdr = htole16(hdr);

	usbd_copy_in(pc, off, &hdr, sizeof(hdr));
	off += sizeof(hdr);

	usbd_copy_in(pc, off, sc->sc_echo_buffer, sc->sc_echo_len);
	off += sc->sc_echo_len;

	sc->sc_flags &= ~CDCEEM_SC_FLAGS_ECHO_RESPONSE_PENDING;
	sc->sc_echo_len = 0;

	*offp = off;
}

static void
cdceem_send_data(struct usb_xfer *xfer, int *offp)
{
	struct cdceem_softc *sc;
	struct usb_page_cache *pc;
	if_t ifp;
	struct mbuf *m;
	int maxlen __diagused, off;
	uint32_t crc;
	uint16_t hdr;

	off = *offp;
	sc = usbd_xfer_softc(xfer);
	pc = usbd_xfer_get_frame(xfer, 0);
	ifp = uether_getifp(&sc->sc_ue);
	maxlen = usbd_xfer_max_len(xfer);

	m = if_dequeue(ifp);
	if (m == NULL) {
		CDCEEM_DEBUG(sc, "no Data packets to send");
		return;
	}

	KASSERT((m->m_pkthdr.len & CDCEEM_DATA_LEN_MASK) == m->m_pkthdr.len,
	    ("%s: packet too long: %d, should be %d\n", __func__,
	     m->m_pkthdr.len, m->m_pkthdr.len & CDCEEM_DATA_LEN_MASK));
	KASSERT(off + sizeof(hdr) + m->m_pkthdr.len + 4 < maxlen,
	    ("%s: out of space; have %d, need %zd", __func__, maxlen,
	    off + sizeof(hdr) + m->m_pkthdr.len + 4));

	CDCEEM_DEBUG(sc, "sending Data, length %d + 4", m->m_pkthdr.len);

	hdr = 0;
	if (!cdceem_send_fake_crc)
		hdr |= CDCEEM_DATA_CRC;
	hdr |= (m->m_pkthdr.len + 4); /* +4 for CRC */
	CDCEEM_DEBUG(sc, "hdr = %#x", hdr);
	hdr = htole16(hdr);

	usbd_copy_in(pc, off, &hdr, sizeof(hdr));
	off += sizeof(hdr);

	usbd_m_copy_in(pc, off, m, 0, m->m_pkthdr.len);
	off += m->m_pkthdr.len;

	if (cdceem_send_fake_crc) {
		crc = htobe32(0xdeadbeef);
	} else {
		crc = cdceem_m_crc32(m, 0, m->m_pkthdr.len);
	}
	CDCEEM_DEBUG(sc, "CRC = %#x", crc);

	usbd_copy_in(pc, off, &crc, sizeof(crc));
	off += sizeof(crc);

	if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);

	/*
	 * If there's a BPF listener, bounce a copy of this frame to it.
	 */
	BPF_MTAP(ifp, m);
	m_freem(m);

	*offp = off;
}

static void
cdceem_bulk_write_callback(struct usb_xfer *xfer, usb_error_t usb_error)
{
	struct cdceem_softc *sc;
	if_t ifp;
	int actlen, aframes, maxlen __diagused, off;

	sc = usbd_xfer_softc(xfer);
	maxlen = usbd_xfer_max_len(xfer);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usbd_xfer_status(xfer, &actlen, NULL, &aframes, NULL);
		CDCEEM_DEBUG(sc, "transferred %u bytes in %u frames",
		    actlen, aframes);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
		CDCEEM_DEBUG(sc, "setup");
tr_setup:

		off = 0;
		usbd_xfer_set_frame_offset(xfer, 0, 0);

		if (sc->sc_flags & CDCEEM_SC_FLAGS_ECHO_PENDING) {
			cdceem_send_echo(xfer, &off);
		} else if (sc->sc_flags & CDCEEM_SC_FLAGS_ECHO_RESPONSE_PENDING) {
			cdceem_send_echo_response(xfer, &off);
		} else {
			cdceem_send_data(xfer, &off);
		}

		KASSERT(off <= maxlen,
		    ("%s: went past the buffer, off %d, maxlen %d",
		     __func__, off, maxlen));

		if (off > 0) {
			CDCEEM_DEBUG(sc, "starting transfer, length %d", off);
			usbd_xfer_set_frame_len(xfer, 0, off);
			usbd_transfer_submit(xfer);
		} else {
			CDCEEM_DEBUG(sc, "nothing to transfer");
		}

		break;

	default:
		CDCEEM_WARN(sc, "USB_ST_ERROR: %s", usbd_errstr(usb_error));

		ifp = uether_getifp(&sc->sc_ue);
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);

		if (usb_error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static int32_t
cdceem_m_crc32_cb(void *arg, void *src, uint32_t count)
{
	uint32_t *p_crc = arg;

	*p_crc = crc32_raw(src, count, *p_crc);
	return (0);
}

static uint32_t
cdceem_m_crc32(struct mbuf *m, uint32_t src_offset, uint32_t src_len)
{
	uint32_t crc = 0xFFFFFFFF;

	m_apply(m, src_offset, src_len, cdceem_m_crc32_cb, &crc);
	return (crc ^ 0xFFFFFFFF);
}

static void
cdceem_start(struct usb_ether *ue)
{
	struct cdceem_softc *sc;

	sc = uether_getsc(ue);

	/*
	 * Start the USB transfers, if not already started.
	 */
	usbd_transfer_start(sc->sc_xfer[CDCEEM_BULK_RX]);
	usbd_transfer_start(sc->sc_xfer[CDCEEM_BULK_TX]);
}

static void
cdceem_init(struct usb_ether *ue)
{
	struct cdceem_softc *sc;
	if_t ifp;

	sc = uether_getsc(ue);
	ifp = uether_getifp(ue);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);

	if (cdceem_send_echoes)
		sc->sc_flags = CDCEEM_SC_FLAGS_ECHO_PENDING;
	else
		sc->sc_flags = 0;

	/*
	 * Stall data write direction, which depends on USB mode.
	 *
	 * Some USB host stacks (e.g. Mac OS X) don't clears stall
	 * bit as it should, so set it in our host mode only.
	 */
	if (usbd_get_mode(sc->sc_ue.ue_udev) == USB_MODE_HOST)
		usbd_xfer_set_stall(sc->sc_xfer[CDCEEM_BULK_TX]);

	cdceem_start(ue);
}

static void
cdceem_stop(struct usb_ether *ue)
{
	struct cdceem_softc *sc;
	if_t ifp;

	sc = uether_getsc(ue);
	ifp = uether_getifp(ue);

	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);

	usbd_transfer_stop(sc->sc_xfer[CDCEEM_BULK_RX]);
	usbd_transfer_stop(sc->sc_xfer[CDCEEM_BULK_TX]);
}

static void
cdceem_setmulti(struct usb_ether *ue)
{
	/* no-op */
	return;
}

static void
cdceem_setpromisc(struct usb_ether *ue)
{
	/* no-op */
	return;
}

static int
cdceem_suspend(device_t dev)
{
	struct cdceem_softc *sc = device_get_softc(dev);

	CDCEEM_DEBUG(sc, "go");
	return (0);
}

static int
cdceem_resume(device_t dev)
{
	struct cdceem_softc *sc = device_get_softc(dev);

	CDCEEM_DEBUG(sc, "go");
	return (0);
}
