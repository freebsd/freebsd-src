/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * HID spec: https://www.usb.org/sites/default/files/documents/hid1_11.pdf
 */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
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
#include <sys/conf.h>
#include <sys/fcntl.h>

#include <dev/evdev/input.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidquirk.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usb_core.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usb_util.h>

#define	USB_DEBUG_VAR usbhid_debug
#include <dev/usb/usb_debug.h>

#include <dev/usb/quirk/usb_quirk.h>

#include "hid_if.h"

static SYSCTL_NODE(_hw_usb, OID_AUTO, usbhid, CTLFLAG_RW, 0, "USB usbhid");
static int usbhid_enable = 0;
SYSCTL_INT(_hw_usb_usbhid, OID_AUTO, enable, CTLFLAG_RWTUN,
    &usbhid_enable, 0, "Enable usbhid and prefer it to other USB HID drivers");
#ifdef USB_DEBUG
static int usbhid_debug = 0;
SYSCTL_INT(_hw_usb_usbhid, OID_AUTO, debug, CTLFLAG_RWTUN,
    &usbhid_debug, 0, "Debug level");
#endif

/* Second set of USB transfers for polling mode */
#define	POLL_XFER(xfer)	((xfer) + USBHID_N_TRANSFER)
enum {
	USBHID_INTR_OUT_DT,
	USBHID_INTR_IN_DT,
	USBHID_CTRL_DT,
	USBHID_N_TRANSFER,
};

struct usbhid_xfer_ctx;
typedef int usbhid_callback_t(struct usbhid_xfer_ctx *xfer_ctx);

union usbhid_device_request {
	struct {			/* INTR xfers */
		uint16_t maxlen;
		uint16_t actlen;
	} intr;
	struct usb_device_request ctrl;	/* CTRL xfers */
};

/* Syncronous USB transfer context */
struct usbhid_xfer_ctx {
	union usbhid_device_request req;
	uint8_t *buf;
	int error;
	usbhid_callback_t *cb;
	void *cb_ctx;
	int waiters;
	bool influx;
};

struct usbhid_softc {
	hid_intr_t *sc_intr_handler;
	void *sc_intr_ctx;
	void *sc_intr_buf;

	struct hid_device_info sc_hw;

	struct mtx sc_mtx;
	struct usb_config sc_config[USBHID_N_TRANSFER];
	struct usb_xfer *sc_xfer[POLL_XFER(USBHID_N_TRANSFER)];
	struct usbhid_xfer_ctx sc_xfer_ctx[POLL_XFER(USBHID_N_TRANSFER)];
	bool sc_can_poll;

	struct usb_device *sc_udev;
	uint8_t	sc_iface_no;
	uint8_t	sc_iface_index;
};

/* prototypes */

static device_probe_t usbhid_probe;
static device_attach_t usbhid_attach;
static device_detach_t usbhid_detach;

static usb_callback_t usbhid_intr_out_callback;
static usb_callback_t usbhid_intr_in_callback;
static usb_callback_t usbhid_ctrl_callback;

static usbhid_callback_t usbhid_intr_handler_cb;
static usbhid_callback_t usbhid_sync_wakeup_cb;

static void
usbhid_intr_out_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usbhid_xfer_ctx *xfer_ctx = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int len;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		len = xfer_ctx->req.intr.maxlen;
		if (len == 0) {
			if (USB_IN_POLLING_MODE_FUNC())
				xfer_ctx->error = 0;
			return;
		}
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, xfer_ctx->buf, len);
		usbd_xfer_set_frame_len(xfer, 0, len);
		usbd_transfer_submit(xfer);
		xfer_ctx->req.intr.maxlen = 0;
		if (USB_IN_POLLING_MODE_FUNC())
			return;
		xfer_ctx->error = 0;
		goto tr_exit;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		xfer_ctx->error = EIO;
tr_exit:
		(void)xfer_ctx->cb(xfer_ctx);
		return;
	}
}

static void
usbhid_intr_in_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usbhid_xfer_ctx *xfer_ctx = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("transferred!\n");

		usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, xfer_ctx->buf, actlen);
		xfer_ctx->req.intr.actlen = actlen;
		if (xfer_ctx->cb(xfer_ctx) != 0)
			return;

	case USB_ST_SETUP:
re_submit:
		usbd_xfer_set_frame_len(xfer, 0, xfer_ctx->req.intr.maxlen);
		usbd_transfer_submit(xfer);
		return;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto re_submit;
		}
		return;
	}
}

static void
usbhid_ctrl_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usbhid_xfer_ctx *xfer_ctx = usbd_xfer_softc(xfer);
	struct usb_device_request *req = &xfer_ctx->req.ctrl;
	struct usb_page_cache *pc;
	int len = UGETW(req->wLength);
	bool is_rd = (req->bmRequestType & UT_READ) != 0;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		if (!is_rd && len != 0) {
			pc = usbd_xfer_get_frame(xfer, 1);
			usbd_copy_in(pc, 0, xfer_ctx->buf, len);
		}

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, req, sizeof(*req));
		usbd_xfer_set_frame_len(xfer, 0, sizeof(*req));
		if (len != 0)
			usbd_xfer_set_frame_len(xfer, 1, len);
		usbd_xfer_set_frames(xfer, len != 0 ? 2 : 1);
		usbd_transfer_submit(xfer);
		return;

	case USB_ST_TRANSFERRED:
		if (is_rd && len != 0) {
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, sizeof(*req), xfer_ctx->buf, len);
		}
		xfer_ctx->error = 0;
		goto tr_exit;

	default:			/* Error */
		/* bomb out */
		DPRINTFN(1, "error=%s\n", usbd_errstr(error));
		xfer_ctx->error = EIO;
tr_exit:
		(void)xfer_ctx->cb(xfer_ctx);
		return;
	}
}

static int
usbhid_intr_handler_cb(struct usbhid_xfer_ctx *xfer_ctx)
{
	struct usbhid_softc *sc = xfer_ctx->cb_ctx;

	sc->sc_intr_handler(sc->sc_intr_ctx, xfer_ctx->buf,
	    xfer_ctx->req.intr.actlen);

	return (0);
}

static int
usbhid_sync_wakeup_cb(struct usbhid_xfer_ctx *xfer_ctx)
{

	if (!USB_IN_POLLING_MODE_FUNC())
		wakeup(xfer_ctx->cb_ctx);

	return (ECANCELED);
}

static const struct usb_config usbhid_config[USBHID_N_TRANSFER] = {

	[USBHID_INTR_OUT_DT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.flags = {.pipe_bof = 1,.proxy_buffer = 1},
		.callback = &usbhid_intr_out_callback,
	},
	[USBHID_INTR_IN_DT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.proxy_buffer = 1},
		.callback = &usbhid_intr_in_callback,
	},
	[USBHID_CTRL_DT] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.flags = {.proxy_buffer = 1},
		.callback = &usbhid_ctrl_callback,
		.timeout = 1000,	/* 1 second */
	},
};

static inline usb_frlength_t
usbhid_xfer_max_len(struct usb_xfer *xfer)
{
	return (xfer == NULL ? 0 : usbd_xfer_max_len(xfer));
}

static inline int
usbhid_xfer_check_len(struct usbhid_softc* sc, int xfer_idx, hid_size_t len)
{
	if (USB_IN_POLLING_MODE_FUNC())
		xfer_idx = POLL_XFER(xfer_idx);
	if (sc->sc_xfer[xfer_idx] == NULL)
		return (ENODEV);
	if (len > usbd_xfer_max_len(sc->sc_xfer[xfer_idx]))
		return (ENOBUFS);
	return (0);
}

static void
usbhid_intr_setup(device_t dev, device_t child __unused, hid_intr_t intr,
    void *context, struct hid_rdesc_info *rdesc)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	uint16_t n;
	bool nowrite;
	int error;

	nowrite = hid_test_quirk(&sc->sc_hw, HQ_NOWRITE);

	/*
	 * Setup the USB transfers one by one, so they are memory independent
	 * which allows for handling panics triggered by the HID drivers
	 * itself, typically by hkbd via CTRL+ALT+ESC sequences. Or if the HID
	 * keyboard driver was processing a key at the moment of panic.
	 */
	if (intr == NULL) {
		if (sc->sc_can_poll)
			return;
		for (n = 0; n != USBHID_N_TRANSFER; n++) {
			if (nowrite && n == USBHID_INTR_OUT_DT)
				continue;
			error = usbd_transfer_setup(sc->sc_udev,
			    &sc->sc_iface_index, sc->sc_xfer + POLL_XFER(n),
			    sc->sc_config + n, 1,
			    (void *)(sc->sc_xfer_ctx + POLL_XFER(n)),
			    &sc->sc_mtx);
			if (error)
				DPRINTF("xfer %d setup error=%s\n", n,
				    usbd_errstr(error));
		}
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_xfer[USBHID_INTR_IN_DT] != NULL &&
		    sc->sc_xfer[USBHID_INTR_IN_DT]->flags_int.started)
			usbd_transfer_start(
			    sc->sc_xfer[POLL_XFER(USBHID_INTR_IN_DT)]);
		mtx_unlock(&sc->sc_mtx);
		sc->sc_can_poll = true;
		return;
	}

	sc->sc_intr_handler = intr;
	sc->sc_intr_ctx = context;
	bcopy(usbhid_config, sc->sc_config, sizeof(usbhid_config));
	bzero(sc->sc_xfer, sizeof(sc->sc_xfer));

	/* Set buffer sizes to match HID report sizes */
	sc->sc_config[USBHID_INTR_OUT_DT].bufsize = rdesc->osize;
	sc->sc_config[USBHID_INTR_IN_DT].bufsize = rdesc->isize;
	sc->sc_config[USBHID_CTRL_DT].bufsize =
	    MAX(rdesc->isize, MAX(rdesc->osize, rdesc->fsize));

	for (n = 0; n != USBHID_N_TRANSFER; n++) {
		if (nowrite && n == USBHID_INTR_OUT_DT)
			continue;
		error = usbd_transfer_setup(sc->sc_udev, &sc->sc_iface_index,
		    sc->sc_xfer + n, sc->sc_config + n, 1,
		    (void *)(sc->sc_xfer_ctx + n), &sc->sc_mtx);
		if (error)
			DPRINTF("xfer %d setup error=%s\n", n,
			    usbd_errstr(error));
	}

	rdesc->rdsize = usbhid_xfer_max_len(sc->sc_xfer[USBHID_INTR_IN_DT]);
	rdesc->grsize = usbhid_xfer_max_len(sc->sc_xfer[USBHID_CTRL_DT]);
	rdesc->srsize = rdesc->grsize;
	rdesc->wrsize = nowrite ? rdesc->srsize :
	    usbhid_xfer_max_len(sc->sc_xfer[USBHID_INTR_OUT_DT]);

	sc->sc_intr_buf = malloc(rdesc->rdsize, M_USBDEV, M_ZERO | M_WAITOK);
}

static void
usbhid_intr_unsetup(device_t dev, device_t child __unused)
{
	struct usbhid_softc* sc = device_get_softc(dev);

	usbd_transfer_unsetup(sc->sc_xfer, USBHID_N_TRANSFER);
	if (sc->sc_can_poll)
		usbd_transfer_unsetup(
		    sc->sc_xfer, POLL_XFER(USBHID_N_TRANSFER));
	sc->sc_can_poll = false;
	free(sc->sc_intr_buf, M_USBDEV);
}

static int
usbhid_intr_start(device_t dev, device_t child __unused)
{
	struct usbhid_softc* sc = device_get_softc(dev);

	if (sc->sc_xfer[USBHID_INTR_IN_DT] == NULL)
		return (ENODEV);

	mtx_lock(&sc->sc_mtx);
	sc->sc_xfer_ctx[USBHID_INTR_IN_DT] = (struct usbhid_xfer_ctx) {
		.req.intr.maxlen =
		    usbd_xfer_max_len(sc->sc_xfer[USBHID_INTR_IN_DT]),
		.cb = usbhid_intr_handler_cb,
		.cb_ctx = sc,
		.buf = sc->sc_intr_buf,
	};
	sc->sc_xfer_ctx[POLL_XFER(USBHID_INTR_IN_DT)] = (struct usbhid_xfer_ctx) {
		.req.intr.maxlen =
		    usbd_xfer_max_len(sc->sc_xfer[USBHID_INTR_IN_DT]),
		.cb = usbhid_intr_handler_cb,
		.cb_ctx = sc,
		.buf = sc->sc_intr_buf,
	};
	usbd_transfer_start(sc->sc_xfer[USBHID_INTR_IN_DT]);
	if (sc->sc_can_poll)
		usbd_transfer_start(sc->sc_xfer[POLL_XFER(USBHID_INTR_IN_DT)]);
	mtx_unlock(&sc->sc_mtx);

	return (0);
}

static int
usbhid_intr_stop(device_t dev, device_t child __unused)
{
	struct usbhid_softc* sc = device_get_softc(dev);

	usbd_transfer_drain(sc->sc_xfer[USBHID_INTR_IN_DT]);
	usbd_transfer_drain(sc->sc_xfer[USBHID_INTR_OUT_DT]);
	if (sc->sc_can_poll)
		usbd_transfer_drain(sc->sc_xfer[POLL_XFER(USBHID_INTR_IN_DT)]);

	return (0);
}

static void
usbhid_intr_poll(device_t dev, device_t child __unused)
{
	struct usbhid_softc* sc = device_get_softc(dev);

	MPASS(sc->sc_can_poll);
	usbd_transfer_poll(sc->sc_xfer + USBHID_INTR_IN_DT, 1);
	usbd_transfer_poll(sc->sc_xfer + POLL_XFER(USBHID_INTR_IN_DT), 1);
}

/*
 * HID interface
 */
static int
usbhid_sync_xfer(struct usbhid_softc* sc, int xfer_idx,
    union usbhid_device_request *req, void *buf)
{
	int error, timeout;
	struct usbhid_xfer_ctx *xfer_ctx;

	xfer_ctx = sc->sc_xfer_ctx + xfer_idx;

	if (USB_IN_POLLING_MODE_FUNC()) {
		xfer_ctx = POLL_XFER(xfer_ctx);
		xfer_idx = POLL_XFER(xfer_idx);
	} else {
		mtx_lock(&sc->sc_mtx);
		++xfer_ctx->waiters;
		while (xfer_ctx->influx)
			mtx_sleep(&xfer_ctx->waiters, &sc->sc_mtx, 0,
			    "usbhid wt", 0);
		--xfer_ctx->waiters;
		xfer_ctx->influx = true;
	}

	xfer_ctx->buf = buf;
	xfer_ctx->req = *req;
	xfer_ctx->error = ETIMEDOUT;
	xfer_ctx->cb = &usbhid_sync_wakeup_cb;
	xfer_ctx->cb_ctx = xfer_ctx;
	timeout = USB_DEFAULT_TIMEOUT;
	usbd_transfer_start(sc->sc_xfer[xfer_idx]);

	if (USB_IN_POLLING_MODE_FUNC())
		while (timeout > 0 && xfer_ctx->error == ETIMEDOUT) {
			usbd_transfer_poll(sc->sc_xfer + xfer_idx, 1);
			DELAY(1000);
			timeout--;
		}
	 else
		msleep_sbt(xfer_ctx, &sc->sc_mtx, 0, "usbhid io",
		    SBT_1MS * timeout, 0, C_HARDCLOCK);

	/* Perform usbhid_write() asyncronously to improve pipelining */
	if (USB_IN_POLLING_MODE_FUNC() || xfer_ctx->error != 0 ||
	    sc->sc_config[xfer_idx].type != UE_INTERRUPT ||
	    sc->sc_config[xfer_idx].direction != UE_DIR_OUT)
		usbd_transfer_stop(sc->sc_xfer[xfer_idx]);
	error = xfer_ctx->error;
	if (error == 0)
		*req = xfer_ctx->req;

	if (!USB_IN_POLLING_MODE_FUNC()) {
		xfer_ctx->influx = false;
		if (xfer_ctx->waiters != 0)
			wakeup_one(&xfer_ctx->waiters);
		mtx_unlock(&sc->sc_mtx);
	}

	if (error)
		DPRINTF("USB IO error:%d\n", error);

	return (error);
}

static int
usbhid_get_rdesc(device_t dev, device_t child __unused, void *buf,
    hid_size_t len)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	int error;

	error = usbd_req_get_report_descriptor(sc->sc_udev, NULL,
	    buf, len, sc->sc_iface_index);

	if (error)
		DPRINTF("no report descriptor: %s\n", usbd_errstr(error));

	return (error == 0 ? 0 : ENXIO);
}

static int
usbhid_get_report(device_t dev, device_t child __unused, void *buf,
    hid_size_t maxlen, hid_size_t *actlen, uint8_t type, uint8_t id)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	union usbhid_device_request req;
	int error;

	error = usbhid_xfer_check_len(sc, USBHID_CTRL_DT, maxlen);
	if (error)
		return (error);

	req.ctrl.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.ctrl.bRequest = UR_GET_REPORT;
	USETW2(req.ctrl.wValue, type, id);
	req.ctrl.wIndex[0] = sc->sc_iface_no;
	req.ctrl.wIndex[1] = 0;
	USETW(req.ctrl.wLength, maxlen);

	error = usbhid_sync_xfer(sc, USBHID_CTRL_DT, &req, buf);
	if (!error && actlen != NULL)
		*actlen = maxlen;

	return (error);
}

static int
usbhid_set_report(device_t dev, device_t child __unused, const void *buf,
    hid_size_t len, uint8_t type, uint8_t id)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	union usbhid_device_request req;
	int error;

	error = usbhid_xfer_check_len(sc, USBHID_CTRL_DT, len);
	if (error)
		return (error);

	req.ctrl.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.ctrl.bRequest = UR_SET_REPORT;
	USETW2(req.ctrl.wValue, type, id);
	req.ctrl.wIndex[0] = sc->sc_iface_no;
	req.ctrl.wIndex[1] = 0;
	USETW(req.ctrl.wLength, len);

	return (usbhid_sync_xfer(sc, USBHID_CTRL_DT, &req,
	    __DECONST(void *, buf)));
}

static int
usbhid_read(device_t dev, device_t child __unused, void *buf,
    hid_size_t maxlen, hid_size_t *actlen)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	union usbhid_device_request req;
	int error;

	error = usbhid_xfer_check_len(sc, USBHID_INTR_IN_DT, maxlen);
	if (error)
		return (error);

	req.intr.maxlen = maxlen;
	error = usbhid_sync_xfer(sc, USBHID_INTR_IN_DT, &req, buf);
	if (error == 0 && actlen != NULL)
		*actlen = req.intr.actlen;

	return (error);
}

static int
usbhid_write(device_t dev, device_t child __unused, const void *buf,
    hid_size_t len)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	union usbhid_device_request req;
	int error;

	error = usbhid_xfer_check_len(sc, USBHID_INTR_OUT_DT, len);
	if (error)
		return (error);

	req.intr.maxlen = len;
	return (usbhid_sync_xfer(sc, USBHID_INTR_OUT_DT, &req,
	    __DECONST(void *, buf)));
}

static int
usbhid_set_idle(device_t dev, device_t child __unused, uint16_t duration,
    uint8_t id)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	union usbhid_device_request req;
	int error;

	error = usbhid_xfer_check_len(sc, USBHID_CTRL_DT, 0);
	if (error)
		return (error);

	/* Duration is measured in 4 milliseconds per unit. */
	req.ctrl.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.ctrl.bRequest = UR_SET_IDLE;
	USETW2(req.ctrl.wValue, (duration + 3) / 4, id);
	req.ctrl.wIndex[0] = sc->sc_iface_no;
	req.ctrl.wIndex[1] = 0;
	USETW(req.ctrl.wLength, 0);

	return (usbhid_sync_xfer(sc, USBHID_CTRL_DT, &req, NULL));
}

static int
usbhid_set_protocol(device_t dev, device_t child __unused, uint16_t protocol)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	union usbhid_device_request req;
	int error;

	error = usbhid_xfer_check_len(sc, USBHID_CTRL_DT, 0);
	if (error)
		return (error);

	req.ctrl.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.ctrl.bRequest = UR_SET_PROTOCOL;
	USETW(req.ctrl.wValue, protocol);
	req.ctrl.wIndex[0] = sc->sc_iface_no;
	req.ctrl.wIndex[1] = 0;
	USETW(req.ctrl.wLength, 0);

	return (usbhid_sync_xfer(sc, USBHID_CTRL_DT, &req, NULL));
}

static int
usbhid_ioctl(device_t dev, device_t child __unused, unsigned long cmd,
    uintptr_t data)
{
	struct usbhid_softc* sc = device_get_softc(dev);
	struct usb_ctl_request *ucr;
	union usbhid_device_request req;
	int error;

	switch (cmd) {
	case USB_REQUEST:
		ucr = (struct usb_ctl_request *)data;
		req.ctrl = ucr->ucr_request;
		error = usbhid_xfer_check_len(
		    sc, USBHID_CTRL_DT, UGETW(req.ctrl.wLength));
		if (error)
			break;
		error = usb_check_request(sc->sc_udev, &req.ctrl);
		if (error)
			break;
		error = usbhid_sync_xfer(
		    sc, USBHID_CTRL_DT, &req, ucr->ucr_data);
		if (error == 0)
			ucr->ucr_actlen = UGETW(req.ctrl.wLength);
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

static void
usbhid_init_device_info(struct usb_attach_arg *uaa, struct hid_device_info *hw)
{

	hw->idBus = BUS_USB;
	hw->idVendor = uaa->info.idVendor;
	hw->idProduct = uaa->info.idProduct;
	hw->idVersion = uaa->info.bcdDevice;

	/* Set various quirks based on usb_attach_arg */
	hid_add_dynamic_quirk(hw, USB_GET_DRIVER_INFO(uaa));
}

static void
usbhid_fill_device_info(struct usb_attach_arg *uaa, struct hid_device_info *hw)
{
	struct usb_device *udev = uaa->device;
	struct usb_interface *iface = uaa->iface;
	struct usb_hid_descriptor *hid;
	struct usb_endpoint *ep;

	snprintf(hw->name, sizeof(hw->name), "%s %s",
	    usb_get_manufacturer(udev), usb_get_product(udev));
	strlcpy(hw->serial, usb_get_serial(udev), sizeof(hw->serial));

	if (uaa->info.bInterfaceClass == UICLASS_HID &&
	    iface != NULL && iface->idesc != NULL) {
		hid = hid_get_descriptor_from_usb(
		    usbd_get_config_descriptor(udev), iface->idesc);
		if (hid != NULL)
			hw->rdescsize =
			    UGETW(hid->descrs[0].wDescriptorLength);
	}

	/* See if there is a interrupt out endpoint. */
	ep = usbd_get_endpoint(udev, uaa->info.bIfaceIndex,
	    usbhid_config + USBHID_INTR_OUT_DT);
	if (ep == NULL || ep->methods == NULL)
		hid_add_dynamic_quirk(hw, HQ_NOWRITE);
}

static const STRUCT_USB_HOST_ID usbhid_devs[] = {
	/* the Xbox 360 gamepad doesn't use the HID class */
	{USB_IFACE_CLASS(UICLASS_VENDOR),
	 USB_IFACE_SUBCLASS(UISUBCLASS_XBOX360_CONTROLLER),
	 USB_IFACE_PROTOCOL(UIPROTO_XBOX360_GAMEPAD),
	 USB_DRIVER_INFO(HQ_IS_XBOX360GP)},
	/* HID keyboard with boot protocol support */
	{USB_IFACE_CLASS(UICLASS_HID),
	 USB_IFACE_SUBCLASS(UISUBCLASS_BOOT),
	 USB_IFACE_PROTOCOL(UIPROTO_BOOT_KEYBOARD),
	 USB_DRIVER_INFO(HQ_HAS_KBD_BOOTPROTO)},
	/* HID mouse with boot protocol support */
	{USB_IFACE_CLASS(UICLASS_HID),
	 USB_IFACE_SUBCLASS(UISUBCLASS_BOOT),
	 USB_IFACE_PROTOCOL(UIPROTO_MOUSE),
	 USB_DRIVER_INFO(HQ_HAS_MS_BOOTPROTO)},
	/* generic HID class */
	{USB_IFACE_CLASS(UICLASS_HID), USB_DRIVER_INFO(HQ_NONE)},
};

static int
usbhid_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usbhid_softc *sc = device_get_softc(dev);
	int error;

	DPRINTFN(11, "\n");

	if (usbhid_enable == 0)
		return (ENXIO);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	error = usbd_lookup_id_by_uaa(usbhid_devs, sizeof(usbhid_devs), uaa);
	if (error)
		return (error);

	if (usb_test_quirk(uaa, UQ_HID_IGNORE))
		return (ENXIO);

	/*
	 * Setup temporary hid_device_info so that we can figure out some
	 * basic quirks for this device.
	 */
	usbhid_init_device_info(uaa, &sc->sc_hw);

	if (hid_test_quirk(&sc->sc_hw, HQ_HID_IGNORE))
		return (ENXIO);

	return (BUS_PROBE_DEFAULT + 1);
}

static int
usbhid_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usbhid_softc *sc = device_get_softc(dev);
	device_t child;
	int error = 0;

	DPRINTFN(10, "sc=%p\n", sc);

	device_set_usb_desc(dev);

	sc->sc_udev = uaa->device;
	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	usbhid_fill_device_info(uaa, &sc->sc_hw);

	error = usbd_req_set_idle(uaa->device, NULL,
	    uaa->info.bIfaceIndex, 0, 0);
	if (error)
		DPRINTF("set idle failed, error=%s (ignored)\n",
		    usbd_errstr(error));

	mtx_init(&sc->sc_mtx, "usbhid lock", NULL, MTX_DEF);

	child = device_add_child(dev, "hidbus", -1);
	if (child == NULL) {
		device_printf(dev, "Could not add hidbus device\n");
		usbhid_detach(dev);
		return (ENOMEM);
	}

	device_set_ivars(child, &sc->sc_hw);
	error = bus_generic_attach(dev);
	if (error) {
		device_printf(dev, "failed to attach child: %d\n", error);
		usbhid_detach(dev);
		return (error);
	}

	return (0);			/* success */
}

static int
usbhid_detach(device_t dev)
{
	struct usbhid_softc *sc = device_get_softc(dev);

	device_delete_children(dev);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static device_method_t usbhid_methods[] = {
	DEVMETHOD(device_probe,		usbhid_probe),
	DEVMETHOD(device_attach,	usbhid_attach),
	DEVMETHOD(device_detach,	usbhid_detach),

	DEVMETHOD(hid_intr_setup,	usbhid_intr_setup),
	DEVMETHOD(hid_intr_unsetup,	usbhid_intr_unsetup),
	DEVMETHOD(hid_intr_start,	usbhid_intr_start),
	DEVMETHOD(hid_intr_stop,	usbhid_intr_stop),
	DEVMETHOD(hid_intr_poll,	usbhid_intr_poll),

	/* HID interface */
	DEVMETHOD(hid_get_rdesc,	usbhid_get_rdesc),
	DEVMETHOD(hid_read,		usbhid_read),
	DEVMETHOD(hid_write,		usbhid_write),
	DEVMETHOD(hid_get_report,	usbhid_get_report),
	DEVMETHOD(hid_set_report,	usbhid_set_report),
	DEVMETHOD(hid_set_idle,		usbhid_set_idle),
	DEVMETHOD(hid_set_protocol,	usbhid_set_protocol),
	DEVMETHOD(hid_ioctl,		usbhid_ioctl),

	DEVMETHOD_END
};

static driver_t usbhid_driver = {
	.name = "usbhid",
	.methods = usbhid_methods,
	.size = sizeof(struct usbhid_softc),
};

DRIVER_MODULE(usbhid, uhub, usbhid_driver, NULL, NULL);
MODULE_DEPEND(usbhid, usb, 1, 1, 1);
MODULE_DEPEND(usbhid, hid, 1, 1, 1);
MODULE_DEPEND(usbhid, hidbus, 1, 1, 1);
MODULE_VERSION(usbhid, 1);
USB_PNP_HOST_INFO(usbhid_devs);
