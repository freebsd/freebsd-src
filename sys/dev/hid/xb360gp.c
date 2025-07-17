/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2020 Val Packett <val@packett.cool>
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
/*
 * XBox 360 gamepad driver thanks to the custom descriptor in usbhid.
 *
 * Tested on: SVEN GC-5070 in both XInput (XBox 360) and DirectInput modes
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#include <dev/hid/hgame.h>
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidmap.h>
#include <dev/hid/hidquirk.h>
#include <dev/hid/hidrdesc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

static const uint8_t	xb360gp_rdesc[] = {HID_XB360GP_REPORT_DESCR()};

#define XB360GP_MAP_BUT(number, code)	\
	{ HIDMAP_KEY(HUP_BUTTON, number, code) }
#define XB360GP_MAP_ABS(usage, code)	\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code) }
#define XB360GP_MAP_ABS_FLT(usage, code)	\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code),	\
	    .fuzz = 16, .flat = 128 }
#define XB360GP_MAP_ABS_INV(usage, code)	\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code),	\
	    .fuzz = 16, .flat = 128, .invert_value = true }
#define XB360GP_MAP_CRG(usage_from, usage_to, callback)	\
	{ HIDMAP_ANY_CB_RANGE(HUP_GENERIC_DESKTOP,	\
	    HUG_##usage_from, HUG_##usage_to, callback) }
#define XB360GP_FINALCB(cb)		\
	{ HIDMAP_FINAL_CB(&cb) }

/* Customized to match usbhid's XBox 360 descriptor */
static const struct hidmap_item xb360gp_map[] = {
	XB360GP_MAP_BUT(1,		BTN_SOUTH),
	XB360GP_MAP_BUT(2,		BTN_EAST),
	XB360GP_MAP_BUT(3,		BTN_WEST),
	XB360GP_MAP_BUT(4,		BTN_NORTH),
	XB360GP_MAP_BUT(5,		BTN_TL),
	XB360GP_MAP_BUT(6,		BTN_TR),
	XB360GP_MAP_BUT(7,		BTN_SELECT),
	XB360GP_MAP_BUT(8,		BTN_START),
	XB360GP_MAP_BUT(9,		BTN_THUMBL),
	XB360GP_MAP_BUT(10,		BTN_THUMBR),
	XB360GP_MAP_BUT(11,		BTN_MODE),
	XB360GP_MAP_CRG(D_PAD_UP, D_PAD_LEFT, hgame_dpad_cb),
	XB360GP_MAP_ABS_FLT(X,		ABS_X),
	XB360GP_MAP_ABS_INV(Y,		ABS_Y),
	XB360GP_MAP_ABS(Z,		ABS_Z),
	XB360GP_MAP_ABS_FLT(RX,		ABS_RX),
	XB360GP_MAP_ABS_INV(RY,		ABS_RY),
	XB360GP_MAP_ABS(RZ,		ABS_RZ),
	XB360GP_FINALCB(		hgame_final_cb),
};

static const STRUCT_USB_HOST_ID xb360gp_devs[] = {
	/* the Xbox 360 gamepad doesn't use the HID class */
	{USB_IFACE_CLASS(UICLASS_VENDOR),
	 USB_IFACE_SUBCLASS(UISUBCLASS_XBOX360_CONTROLLER),
	 USB_IFACE_PROTOCOL(UIPROTO_XBOX360_GAMEPAD),},
};

static void
xb360gp_identify(driver_t *driver, device_t parent)
{
	const struct hid_device_info *hw = hid_get_device_info(parent);

	/* the Xbox 360 gamepad has no report descriptor */
	if (hid_test_quirk(hw, HQ_IS_XBOX360GP))
		hid_set_report_descr(parent, xb360gp_rdesc,
		    sizeof(xb360gp_rdesc));
}

static int
xb360gp_probe(device_t dev)
{
	struct hgame_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw = hid_get_device_info(dev);
	int error;

	if (!hid_test_quirk(hw, HQ_IS_XBOX360GP))
		return (ENXIO);

	hidmap_set_dev(&sc->hm, dev);

	error = HIDMAP_ADD_MAP(&sc->hm, xb360gp_map, NULL);
	if (error != 0)
		return (error);

	device_set_desc(dev, "XBox 360 Gamepad");

	return (BUS_PROBE_DEFAULT);
}

static int
xb360gp_attach(device_t dev)
{
	struct hgame_softc *sc = device_get_softc(dev);
	int error;

	/*
	 * Turn off the four LEDs on the gamepad which
	 * are blinking by default:
	 */
	static const uint8_t reportbuf[3] = {1, 3, 0};
	error = hid_set_report(dev, reportbuf, sizeof(reportbuf),
	    HID_OUTPUT_REPORT, 0);
	if (error)
		device_printf(dev, "set output report failed, error=%d "
		    "(ignored)\n", error);

	return (hidmap_attach(&sc->hm));
}

static int
xb360gp_detach(device_t dev)
{
	struct hgame_softc *sc = device_get_softc(dev);

	return (hidmap_detach(&sc->hm));
}

static device_method_t xb360gp_methods[] = {
	DEVMETHOD(device_identify,	xb360gp_identify),
	DEVMETHOD(device_probe,		xb360gp_probe),
	DEVMETHOD(device_attach,	xb360gp_attach),
	DEVMETHOD(device_detach,	xb360gp_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_0(xb360gp, xb360gp_driver, xb360gp_methods,
    sizeof(struct hgame_softc));
DRIVER_MODULE(xb360gp, hidbus, xb360gp_driver, NULL, NULL);
MODULE_DEPEND(xb360gp, hid, 1, 1, 1);
MODULE_DEPEND(xb360gp, hidbus, 1, 1, 1);
MODULE_DEPEND(xb360gp, hidmap, 1, 1, 1);
MODULE_DEPEND(xb360gp, hgame, 1, 1, 1);
MODULE_DEPEND(xb360gp, evdev, 1, 1, 1);
MODULE_VERSION(xb360gp, 1);
USB_PNP_HOST_INFO(xb360gp_devs);
