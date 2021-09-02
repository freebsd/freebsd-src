/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2019 Greg V <greg@unrelenting.technology>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Generic / MS Windows compatible HID pen tablet driver:
 * https://docs.microsoft.com/en-us/windows-hardware/design/component-guidelines/required-hid-top-level-collections
 *
 * Tested on: Wacom WCOM50C1 (Google Pixelbook "eve")
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidmap.h>
#include <dev/hid/hidrdesc.h>

#include "usbdevs.h"

static const uint8_t	hpen_graphire_report_descr[] =
			   { HID_GRAPHIRE_REPORT_DESCR() };
static const uint8_t	hpen_graphire3_4x5_report_descr[] =
			   { HID_GRAPHIRE3_4X5_REPORT_DESCR() };

static hidmap_cb_t	hpen_battery_strenght_cb;
static hidmap_cb_t	hpen_final_digi_cb;
static hidmap_cb_t	hpen_final_pen_cb;

#define HPEN_MAP_BUT(usage, code)	\
	HIDMAP_KEY(HUP_DIGITIZERS, HUD_##usage, code)
#define HPEN_MAP_ABS(usage, code)	\
	HIDMAP_ABS(HUP_DIGITIZERS, HUD_##usage, code)
#define HPEN_MAP_ABS_GD(usage, code)	\
	HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code)
#define HPEN_MAP_ABS_CB(usage, cb)	\
	HIDMAP_ABS_CB(HUP_DIGITIZERS, HUD_##usage, &cb)

/* Generic map digitizer page map according to hut1_12v2.pdf */
static const struct hidmap_item hpen_map_pen[] = {
    { HPEN_MAP_ABS_GD(X,		ABS_X),		  .required = true },
    { HPEN_MAP_ABS_GD(Y,		ABS_Y),		  .required = true },
    { HPEN_MAP_ABS(   TIP_PRESSURE,	ABS_PRESSURE) },
    { HPEN_MAP_ABS(   X_TILT,		ABS_TILT_X) },
    { HPEN_MAP_ABS(   Y_TILT,		ABS_TILT_Y) },
    { HPEN_MAP_ABS(   CONTACTID,	0), 		  .forbidden = true },
    { HPEN_MAP_ABS(   CONTACTCOUNT,	0), 		  .forbidden = true },
    { HPEN_MAP_ABS_CB(BATTERY_STRENGTH,	hpen_battery_strenght_cb) },
    { HPEN_MAP_BUT(   TOUCH,		BTN_TOUCH) },
    { HPEN_MAP_BUT(   TIP_SWITCH,	BTN_TOUCH) },
    { HPEN_MAP_BUT(   SEC_TIP_SWITCH,	BTN_TOUCH) },
    { HPEN_MAP_BUT(   BARREL_SWITCH,	BTN_STYLUS) },
    { HPEN_MAP_BUT(   INVERT,		BTN_TOOL_RUBBER) },
    { HPEN_MAP_BUT(   ERASER,		BTN_TOUCH) },
    { HPEN_MAP_BUT(   TABLET_PICK,	BTN_STYLUS2) },
    { HPEN_MAP_BUT(   SEC_BARREL_SWITCH,BTN_STYLUS2) },
    { HIDMAP_FINAL_CB(			&hpen_final_pen_cb) },
};

static const struct hidmap_item hpen_map_stylus[] = {
    { HPEN_MAP_BUT(   IN_RANGE,		BTN_TOOL_PEN) },
};
static const struct hidmap_item hpen_map_finger[] = {
    { HPEN_MAP_BUT(   IN_RANGE,		BTN_TOOL_FINGER) },
};

static const struct hid_device_id hpen_devs[] = {
	{ HID_TLC(HUP_DIGITIZERS, HUD_DIGITIZER) },
	{ HID_TLC(HUP_DIGITIZERS, HUD_PEN) },
	{ HID_TLC(HUP_DIGITIZERS, HUD_TOUCHSCREEN),
	  HID_BVP(BUS_USB, USB_VENDOR_EGALAX, USB_PRODUCT_EGALAX_TPANEL) },
};

/* Do not autoload legacy pen driver for all touchscreen */
static const struct hid_device_id hpen_devs_no_load[] = {
	{ HID_TLC(HUP_DIGITIZERS, HUD_TOUCHSCREEN) },
};

static int
hpen_battery_strenght_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	int32_t data;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		evdev_support_event(evdev, EV_PWR);
		/* TODO */
		break;
	case HIDMAP_CB_IS_RUNNING:
		data = ctx.data;
		/* TODO */
		break;
	default:
		break;
	}

	return (0);
}

static int
hpen_final_pen_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING) {
		if (hidbus_get_usage(HIDMAP_CB_GET_DEV()) ==
		    HID_USAGE2(HUP_DIGITIZERS, HUD_DIGITIZER))
			evdev_support_prop(evdev, INPUT_PROP_POINTER);
		else
			evdev_support_prop(evdev, INPUT_PROP_DIRECT);
	}

	/* Do not execute callback at interrupt handler and detach */
	return (ENOSYS);
}

static void
hpen_identify(driver_t *driver, device_t parent)
{
	const struct hid_device_info *hw = hid_get_device_info(parent);

	/* the report descriptor for the Wacom Graphire is broken */
	if (hw->idBus == BUS_USB && hw->idVendor == USB_VENDOR_WACOM) {
		switch (hw->idProduct) {
		case USB_PRODUCT_WACOM_GRAPHIRE:
			hid_set_report_descr(parent,
			    hpen_graphire_report_descr,
			    sizeof(hpen_graphire_report_descr));
			break;

		case USB_PRODUCT_WACOM_GRAPHIRE3_4X5:
			hid_set_report_descr(parent,
			    hpen_graphire3_4x5_report_descr,
			    sizeof(hpen_graphire3_4x5_report_descr));
			break;
		}
	}
}

static int
hpen_probe(device_t dev)
{
	struct hidmap *hm = device_get_softc(dev);
	const char *desc;
	void *d_ptr;
	hid_size_t d_len;
	int error;

	if (HIDBUS_LOOKUP_DRIVER_INFO(dev, hpen_devs_no_load) != 0) {
		error = HIDBUS_LOOKUP_DRIVER_INFO(dev, hpen_devs);
		if (error != 0)
			return (error);
	}

	hidmap_set_dev(hm, dev);

	/* Check if report descriptor belongs to a HID pen device */
	error = HIDMAP_ADD_MAP(hm, hpen_map_pen, NULL);
	if (error != 0)
		return (error);

	if (hid_get_report_descr(dev, &d_ptr, &d_len) != 0)
		return (ENXIO);

	if (hidbus_is_collection(d_ptr, d_len,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_FINGER), hidbus_get_index(dev))) {
		HIDMAP_ADD_MAP(hm, hpen_map_finger, NULL);
		desc = "TouchScreen";
	} else {
		HIDMAP_ADD_MAP(hm, hpen_map_stylus, NULL);
		desc = "Pen";
	}
	if (hidbus_get_usage(dev) == HID_USAGE2(HUP_DIGITIZERS, HUD_DIGITIZER))
		desc = "Digitizer";

	hidbus_set_desc(dev, desc);

	return (BUS_PROBE_DEFAULT);
}

static int
hpen_attach(device_t dev)
{
	const struct hid_device_info *hw = hid_get_device_info(dev);
	struct hidmap *hm = device_get_softc(dev);
	int error;

	if (hw->idBus == BUS_USB && hw->idVendor == USB_VENDOR_WACOM &&
	    hw->idProduct == USB_PRODUCT_WACOM_GRAPHIRE3_4X5) {
		/*
		 * The Graphire3 needs 0x0202 to be written to
		 * feature report ID 2 before it'll start
		 * returning digitizer data.
		 */
		static const uint8_t reportbuf[3] = {2, 2, 2};
		error = hid_set_report(dev, reportbuf, sizeof(reportbuf),
		    HID_FEATURE_REPORT, reportbuf[0]);
		if (error)
			device_printf(dev, "set feature report failed, "
			    "error=%d (ignored)\n", error);
	}

	return (hidmap_attach(hm));
}

static int
hpen_detach(device_t dev)
{
	return (hidmap_detach(device_get_softc(dev)));
}


static devclass_t hpen_devclass;
static device_method_t hpen_methods[] = {
	DEVMETHOD(device_identify,	hpen_identify),
	DEVMETHOD(device_probe,		hpen_probe),
	DEVMETHOD(device_attach,	hpen_attach),
	DEVMETHOD(device_detach,	hpen_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(hpen, hpen_driver, hpen_methods, sizeof(struct hidmap));
DRIVER_MODULE(hpen, hidbus, hpen_driver, hpen_devclass, NULL, 0);
MODULE_DEPEND(hpen, hid, 1, 1, 1);
MODULE_DEPEND(hpen, hidbus, 1, 1, 1);
MODULE_DEPEND(hpen, hidmap, 1, 1, 1);
MODULE_DEPEND(hpen, evdev, 1, 1, 1);
MODULE_VERSION(hpen, 1);
HID_PNP_INFO(hpen_devs);
