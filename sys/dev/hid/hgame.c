/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2020 Greg V <greg@unrelenting.technology>
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
 * Generic HID game controller (joystick/gamepad) driver,
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
#include <dev/hid/hidquirk.h>
#include <dev/hid/hidmap.h>

#define HGAME_MAP_BRG(number_from, number_to, code)	\
	{ HIDMAP_KEY_RANGE(HUP_BUTTON, number_from, number_to, code) }
#define HGAME_MAP_ABS(usage, code)	\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code) }
#define HGAME_MAP_GCB(usage, callback)	\
	{ HIDMAP_ANY_CB(HUP_GENERIC_DESKTOP, HUG_##usage, callback) }
#define HGAME_MAP_CRG(usage_from, usage_to, callback)	\
	{ HIDMAP_ANY_CB_RANGE(HUP_GENERIC_DESKTOP,	\
	    HUG_##usage_from, HUG_##usage_to, callback) }
#define HGAME_FINALCB(cb)	\
	{ HIDMAP_FINAL_CB(&cb) }

static const struct hidmap_item hgame_map[] = {
	HGAME_MAP_BRG(1, 16,		BTN_TRIGGER),
	HGAME_MAP_ABS(X,		ABS_X),
	HGAME_MAP_ABS(Y,		ABS_Y),
	HGAME_MAP_ABS(Z,		ABS_Z),
	HGAME_MAP_ABS(RX,		ABS_RX),
	HGAME_MAP_ABS(RY,		ABS_RY),
	HGAME_MAP_ABS(RZ,		ABS_RZ),
	HGAME_MAP_GCB(HAT_SWITCH,	hgame_hat_switch_cb),
	HGAME_MAP_CRG(D_PAD_UP, D_PAD_LEFT, hgame_dpad_cb),
	HGAME_MAP_BRG(17, 57,		BTN_TRIGGER_HAPPY),
	HGAME_FINALCB(			hgame_final_cb),
};

static const struct hid_device_id hgame_devs[] = {
	{ HID_TLC(HUP_GENERIC_DESKTOP, HUG_JOYSTICK),
	  HID_DRIVER_INFO(HUG_JOYSTICK) },
	{ HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD),
	  HID_DRIVER_INFO(HUG_GAME_PAD) },
};

int
hgame_hat_switch_cb(HIDMAP_CB_ARGS)
{
	static const struct { int32_t x; int32_t y; } hat_switch_map[] = {
	    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0},
	    {-1, -1},{0, 0}
	};
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	u_int idx;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		evdev_support_event(evdev, EV_ABS);
		evdev_support_abs(evdev, ABS_HAT0X, -1, 1, 0, 0, 0);
		evdev_support_abs(evdev, ABS_HAT0Y, -1, 1, 0, 0, 0);
		break;

	case HIDMAP_CB_IS_RUNNING:
		idx = MIN(nitems(hat_switch_map) - 1, (u_int)ctx.data);
		evdev_push_abs(evdev, ABS_HAT0X, hat_switch_map[idx].x);
		evdev_push_abs(evdev, ABS_HAT0Y, hat_switch_map[idx].y);
		break;

	default:
		break;
	}

	return (0);
}

/*
 * Emulate the hat switch report via the D-pad usages
 * found on XInput/XBox style devices
 */
int
hgame_dpad_cb(HIDMAP_CB_ARGS)
{
	struct hgame_softc *sc = HIDMAP_CB_GET_SOFTC();
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	int32_t data;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		HIDMAP_CB_UDATA64 = HID_GET_USAGE(ctx.hi->usage);
		evdev_support_event(evdev, EV_ABS);
		evdev_support_abs(evdev, ABS_HAT0X, -1, 1, 0, 0, 0);
		evdev_support_abs(evdev, ABS_HAT0Y, -1, 1, 0, 0, 0);
		break;

	case HIDMAP_CB_IS_RUNNING:
		data = ctx.data;
		switch (HIDMAP_CB_UDATA64) {
		case HUG_D_PAD_UP:
			if (sc->dpad_down)
				return (ENOMSG);
			evdev_push_abs(evdev, ABS_HAT0Y, (data == 0) ? 0 : -1);
			sc->dpad_up = (data != 0);
			break;
		case HUG_D_PAD_DOWN:
			if (sc->dpad_up)
				return (ENOMSG);
			evdev_push_abs(evdev, ABS_HAT0Y, (data == 0) ? 0 : 1);
			sc->dpad_down = (data != 0);
			break;
		case HUG_D_PAD_RIGHT:
			if (sc->dpad_left)
				return (ENOMSG);
			evdev_push_abs(evdev, ABS_HAT0X, (data == 0) ? 0 : 1);
			sc->dpad_right = (data != 0);
			break;
		case HUG_D_PAD_LEFT:
			if (sc->dpad_right)
				return (ENOMSG);
			evdev_push_abs(evdev, ABS_HAT0X, (data == 0) ? 0 : -1);
			sc->dpad_left = (data != 0);
			break;
		}
		break;

	default:
		break;
	}

	return (0);
}

int
hgame_final_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING)
		evdev_support_prop(evdev, INPUT_PROP_DIRECT);

	/* Do not execute callback at interrupt handler and detach */
	return (ENOSYS);
}

static int
hgame_probe(device_t dev)
{
	const struct hid_device_info *hw = hid_get_device_info(dev);
	struct hgame_softc *sc = device_get_softc(dev);
	int error;

	if (hid_test_quirk(hw, HQ_IS_XBOX360GP))
		return(ENXIO);

	error = HIDMAP_PROBE(&sc->hm, dev, hgame_devs, hgame_map, NULL);
	if (error > 0)
		return (error);

	hidbus_set_desc(dev, hidbus_get_driver_info(dev) == HUG_GAME_PAD ?
	    "Gamepad" : "Joystick");

	return (BUS_PROBE_GENERIC);
}



static int
hgame_attach(device_t dev)
{
	struct hgame_softc *sc = device_get_softc(dev);

	return (hidmap_attach(&sc->hm));
}

static int
hgame_detach(device_t dev)
{
	struct hgame_softc *sc = device_get_softc(dev);

	return (hidmap_detach(&sc->hm));
}

static device_method_t hgame_methods[] = {
	DEVMETHOD(device_probe,		hgame_probe),
	DEVMETHOD(device_attach,	hgame_attach),
	DEVMETHOD(device_detach,	hgame_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(hgame, hgame_driver, hgame_methods, sizeof(struct hgame_softc));
DRIVER_MODULE(hgame, hidbus, hgame_driver, NULL, NULL);
MODULE_DEPEND(hgame, hid, 1, 1, 1);
MODULE_DEPEND(hgame, hidbus, 1, 1, 1);
MODULE_DEPEND(hgame, hidmap, 1, 1, 1);
MODULE_DEPEND(hgame, evdev, 1, 1, 1);
MODULE_VERSION(hgame, 1);
HID_PNP_INFO(hgame_devs);
