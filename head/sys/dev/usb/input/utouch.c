/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 *
 * $FreeBSD$
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
#include <sys/sbuf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR utouch_debug
#include <dev/usb/usb_debug.h>

#include <dev/usb/quirk/usb_quirk.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/tty.h>

enum {
	UTOUCH_INTR_DT,
	UTOUCH_N_TRANSFER,
};

struct utouch_softc
{
	device_t sc_dev;
	struct evdev_dev *sc_evdev;
	struct mtx sc_mtx;
	struct usb_xfer *sc_xfer[UTOUCH_N_TRANSFER];
	struct hid_location sc_loc_x;
	struct hid_location sc_loc_y;
	struct hid_location sc_loc_z;
#define	UTOUCH_BUTTON_MAX	8
	struct hid_location sc_loc_btn[UTOUCH_BUTTON_MAX];
	uint8_t sc_iid;
	uint8_t	sc_iid_x;
	uint8_t	sc_iid_y;
	uint8_t	sc_iid_z;
	uint8_t	sc_iid_btn[UTOUCH_BUTTON_MAX];
	uint8_t	sc_nbuttons;
	uint32_t sc_flags;
#define	UTOUCH_FLAG_X_AXIS	0x0001
#define	UTOUCH_FLAG_Y_AXIS	0x0002
#define	UTOUCH_FLAG_Z_AXIS	0x0004
#define	UTOUCH_FLAG_OPENED	0x0008


	int sc_oldx;
	int sc_oldy;
	int sc_oldz;
	int sc_oldbuttons;

	uint8_t	sc_temp[64];
};



static usb_callback_t utouch_intr_callback;

static device_probe_t utouch_probe;
static device_attach_t utouch_attach;
static device_detach_t utouch_detach;

static evdev_open_t utouch_ev_open;
static evdev_close_t utouch_ev_close;

static int utouch_hid_test(void *, uint16_t);
static void utouch_hid_parse(struct utouch_softc *, const void *, uint16_t);
static void utouch_report_event(struct utouch_softc *, int, int, int, int);

static struct evdev_methods utouch_evdev_methods = {
	.ev_open = &utouch_ev_open,
	.ev_close = &utouch_ev_close,
};

static const struct usb_config utouch_config[UTOUCH_N_TRANSFER] = {

	[UTOUCH_INTR_DT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = &utouch_intr_callback,
	},
};

static int
utouch_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	void *d_ptr;
	uint16_t d_len;
	int err;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bInterfaceClass != UICLASS_HID)
		return (ENXIO);

	err = usbd_req_get_hid_desc(uaa->device, NULL,
	    &d_ptr, &d_len, M_TEMP, uaa->info.bIfaceIndex);

	if (err)
		return (ENXIO);

	if (utouch_hid_test(d_ptr, d_len))
		err = BUS_PROBE_DEFAULT;
	else
		err = ENXIO;

	free(d_ptr, M_TEMP);
	return (err);
}

static int
utouch_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct utouch_softc *sc = device_get_softc(dev);
	struct input_absinfo absinfo = { 0 };
	void *d_ptr = NULL;
	uint16_t d_len;
	int isize, i, err;

	device_set_usb_desc(dev);
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "utouch lock", NULL, MTX_DEF | MTX_RECURSE);

	err = usbd_req_set_protocol(uaa->device, NULL,
	    uaa->info.bIfaceIndex, 1);

	err = usbd_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, utouch_config,
	    UTOUCH_N_TRANSFER, sc, &sc->sc_mtx);
	if (err) 
		goto detach;

	err = usbd_req_get_hid_desc(uaa->device, NULL, &d_ptr,
	    &d_len, M_TEMP, uaa->info.bIfaceIndex);
	if (err)
		goto detach;

	isize = hid_report_size(d_ptr, d_len, hid_input, &sc->sc_iid);

	utouch_hid_parse(sc, d_ptr, d_len);

	sc->sc_evdev = evdev_alloc();
	evdev_set_name(sc->sc_evdev, device_get_desc(dev));
	evdev_set_serial(sc->sc_evdev, "0");
	evdev_set_softc(sc->sc_evdev, sc);
	evdev_set_methods(sc->sc_evdev, &utouch_evdev_methods);
	evdev_support_event(sc->sc_evdev, EV_SYN);
	evdev_support_event(sc->sc_evdev, EV_ABS);
	evdev_support_event(sc->sc_evdev, EV_REL);
	evdev_support_event(sc->sc_evdev, EV_KEY);

	if (sc->sc_flags & UTOUCH_FLAG_X_AXIS)
		evdev_support_abs(sc->sc_evdev, ABS_X);

	if (sc->sc_flags & UTOUCH_FLAG_Y_AXIS)
		evdev_support_abs(sc->sc_evdev, ABS_Y);

	if (sc->sc_flags & UTOUCH_FLAG_Z_AXIS)
		evdev_support_rel(sc->sc_evdev, REL_WHEEL);

	for (i = 0; i < sc->sc_nbuttons; i++)
		evdev_support_key(sc->sc_evdev, BTN_MOUSE + i);

	/* Report absolute axes information */
	absinfo.minimum = 0;
	absinfo.maximum = 0x7fff; /* XXX should read from HID descriptor */
	evdev_set_absinfo(sc->sc_evdev, ABS_X, &absinfo);
	evdev_set_absinfo(sc->sc_evdev, ABS_Y, &absinfo);

	err = evdev_register(dev, sc->sc_evdev);
	if (err)
		goto detach;

	return (0);

detach:
	if (d_ptr)
		free(d_ptr, M_TEMP);

	utouch_detach(dev);
	return (ENXIO);
}

static int
utouch_detach(device_t dev)
{
	struct utouch_softc *sc = device_get_softc(dev);
	
	/* Stop intr transfer if running */
	utouch_ev_close(sc->sc_evdev, sc);

	evdev_unregister(dev, sc->sc_evdev);
	usbd_transfer_unsetup(sc->sc_xfer, UTOUCH_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);
	return (0);
}

static void
utouch_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct utouch_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	void *buf = sc->sc_temp;
	int x, y, dz, buttons = 0, changed = 0;
	int len, i;
	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (len == 0)
			goto tr_setup;
			
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, buf, len);

		if (sc->sc_flags & UTOUCH_FLAG_X_AXIS) {
			x = hid_get_data(buf, len, &sc->sc_loc_x);
			changed += x != sc->sc_oldx;
		}

		if (sc->sc_flags & UTOUCH_FLAG_Y_AXIS) {
			y = hid_get_data(buf, len, &sc->sc_loc_y);
			changed += y != sc->sc_oldy;
		}

		if (sc->sc_flags & UTOUCH_FLAG_Z_AXIS) {
			dz = hid_get_data(buf, len, &sc->sc_loc_z);
			changed += dz != 0;
		}

		for (i = 0; i < sc->sc_nbuttons; i++) {
			if (hid_get_data(buf, len, &sc->sc_loc_btn[i]))
				buttons |= (1 << i);
		}

		changed += buttons != sc->sc_oldbuttons;

		if (changed) {
			utouch_report_event(sc, x, y, dz, buttons);
			sc->sc_oldx = x;
			sc->sc_oldy = y;
			sc->sc_oldbuttons = buttons;
		}

	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default:
		if (error != USB_ERR_CANCELLED) {
			/* try clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
utouch_ev_close(struct evdev_dev *evdev, void *ev_softc)
{
	struct utouch_softc *sc = (struct utouch_softc *)ev_softc;

	mtx_lock(&sc->sc_mtx);
	usbd_transfer_stop(sc->sc_xfer[UTOUCH_INTR_DT]);
	mtx_unlock(&sc->sc_mtx);
}

static int
utouch_ev_open(struct evdev_dev *evdev, void *ev_softc)
{
	struct utouch_softc *sc = (struct utouch_softc *)ev_softc;

	mtx_lock(&sc->sc_mtx);

	usbd_transfer_stop(sc->sc_xfer[UTOUCH_INTR_DT]);
	usbd_xfer_set_interval(sc->sc_xfer[UTOUCH_INTR_DT], 100);
	usbd_transfer_start(sc->sc_xfer[UTOUCH_INTR_DT]);

	mtx_unlock(&sc->sc_mtx);

	return (0);
}

static void
utouch_report_event(struct utouch_softc *sc, int x, int y, int dz, int buttons)
{
	int i;

	if (x != sc->sc_oldx || y != sc->sc_oldy) {
		if (sc->sc_flags & UTOUCH_FLAG_X_AXIS)
			evdev_push_event(sc->sc_evdev, EV_ABS, ABS_X, x);

		if (sc->sc_flags & UTOUCH_FLAG_Y_AXIS)
			evdev_push_event(sc->sc_evdev, EV_ABS, ABS_Y, y);
	}

	if (sc->sc_flags & UTOUCH_FLAG_Z_AXIS && dz != 0)
		evdev_push_event(sc->sc_evdev, EV_REL, REL_WHEEL, dz);

	if (buttons != sc->sc_oldbuttons) {
		for (i = 0; i < sc->sc_nbuttons; i++) {
			if (((buttons & (1 << i)) ^
			    (sc->sc_oldbuttons & (1 << i))) == 0)
				continue;

			evdev_push_event(sc->sc_evdev, EV_KEY,
			    BTN_MOUSE + i, !!(buttons & (1 << i)));
		}
	}

	evdev_sync(sc->sc_evdev);

}

static int
utouch_hid_test(void *d_ptr, uint16_t d_len)
{
	struct hid_data *hd;
	struct hid_item hi;
	int mdepth;
	int found;

	hd = hid_start_parse(d_ptr, d_len, 1 << hid_input);
	if (hd == NULL)
		return (0);

	mdepth = 0;
	found = 0;

	while (hid_get_item(hd, &hi)) {
		switch (hi.kind) {
		case hid_collection:
			if (mdepth != 0)
				mdepth++;
			else if (hi.collection == 1 &&
			     hi.usage ==
			      HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_POINTER))
				mdepth++;
			break;
		case hid_endcollection:
			if (mdepth != 0)
				mdepth--;
			break;
		case hid_input:
			if (mdepth == 0)
				break;
			if (hi.usage ==
			     HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X) &&
			    (hi.flags & (HIO_CONST|HIO_VARIABLE|HIO_RELATIVE)) == HIO_VARIABLE)
				found++;
			if (hi.usage ==
			     HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y) &&
			    (hi.flags & (HIO_CONST|HIO_VARIABLE|HIO_RELATIVE)) == HIO_VARIABLE)
				found++;
			break;
		default:
			break;
		}
	}
	hid_end_parse(hd);
	return (found);
}

static void
utouch_hid_parse(struct utouch_softc *sc, const void *buf, uint16_t len)
{
	uint32_t flags;
	uint8_t i;

	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
	    hid_input, 0, &sc->sc_loc_x, &flags, &sc->sc_iid_x)) {

		if (flags & HIO_VARIABLE) {
			sc->sc_flags |= UTOUCH_FLAG_X_AXIS;
		}
	}
	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
	    hid_input, 0, &sc->sc_loc_y, &flags, &sc->sc_iid_y)) {

		if (flags & HIO_VARIABLE) {
			sc->sc_flags |= UTOUCH_FLAG_Y_AXIS;
		}
	}
	/* Try the wheel first as the Z activator since it's tradition. */
	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
	    HUG_WHEEL), hid_input, 0, &sc->sc_loc_z, &flags,
	    &sc->sc_iid_z) ||
	    hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
	    HUG_TWHEEL), hid_input, 0, &sc->sc_loc_z, &flags,
	    &sc->sc_iid_z)) {
		if (flags & HIO_VARIABLE)
			sc->sc_flags |= UTOUCH_FLAG_Z_AXIS;
	}	

	/* figure out the number of buttons */
	for (i = 0; i < UTOUCH_BUTTON_MAX; i++) {
		if (!hid_locate(buf, len, HID_USAGE2(HUP_BUTTON, (i + 1)),
		    hid_input, 0, &sc->sc_loc_btn[i], NULL, 
		    &sc->sc_iid_btn[i])) {
			break;
		}
	}

	sc->sc_nbuttons = i;

	if (i > sc->sc_nbuttons)
		sc->sc_nbuttons = i;

	if (sc->sc_flags == 0)
		return;

	/* announce information about the mouse */
	device_printf(sc->sc_dev, "%d buttons and [%s%s%s] axes\n",
	    (sc->sc_nbuttons),
	    (sc->sc_flags & UTOUCH_FLAG_X_AXIS) ? "X" : "",
	    (sc->sc_flags & UTOUCH_FLAG_Y_AXIS) ? "Y" : "",
	    (sc->sc_flags & UTOUCH_FLAG_Z_AXIS) ? "Z" : "");
}

static devclass_t utouch_devclass;

static device_method_t utouch_methods[] = {
	DEVMETHOD(device_probe, utouch_probe),
	DEVMETHOD(device_attach, utouch_attach),
	DEVMETHOD(device_detach, utouch_detach),

	DEVMETHOD_END
};

static driver_t utouch_driver = {
	.name = "utouch",
	.methods = utouch_methods,
	.size = sizeof(struct utouch_softc),
};

DRIVER_MODULE(utouch, uhub, utouch_driver, utouch_devclass, NULL, 0);
MODULE_DEPEND(utouch, usb, 1, 1, 1);
MODULE_VERSION(utouch, 1);
