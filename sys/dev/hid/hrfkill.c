/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 * Copyright (c) 2026 Framework Computer Inc
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Daniel Schaefer
 * <git@danielschaefer.me> under sponsorship from Framework Computer Inc.
 */

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidmap.h>

/*
 * Reference document:
 * https://www.usb.org/sites/default/files/hutrr40radiohidusagesfinal_0.pdf
 */

static hidmap_cb_t	hrfkill_cb;

static const struct hidmap_item hrfkill_map[] = {
	{ HIDMAP_REL_CB(HUP_GENERIC_DESKTOP, HUG_RADIO_BUTTON, &hrfkill_cb) },
};

static const struct hid_device_id hrfkill_devs[] = {
	{ HID_TLC(HUP_GENERIC_DESKTOP, HUG_RADIO_CONTROL) },
};

/*
 * Synthesize key-down + key-up for the wireless radio button,
 * which only reports a relative pulse (value=1) on press.
 * The firmware latches value=1 and never resets it, so we
 * track the last value via HIDMAP_CB_UDATA64 and only fire
 * on the 0 -> non-zero transition.
 */
static int
hrfkill_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	int32_t last;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		evdev_support_event(evdev, EV_KEY);
		evdev_support_key(evdev, KEY_RFKILL);
		HIDMAP_CB_UDATA64 = 0;
		break;
	case HIDMAP_CB_IS_RUNNING:
		last = (int32_t)HIDMAP_CB_UDATA64;
		HIDMAP_CB_UDATA64 = (uint64_t)ctx.data;
		if (ctx.data == 0 || ctx.data == last)
			return (ENOMSG);
		evdev_push_key(evdev, KEY_RFKILL, 1);
		evdev_push_key(evdev, KEY_RFKILL, 0);
		break;
	default:
		break;
	}

	return (0);
}

static int
hrfkill_probe(device_t dev)
{
	return (HIDMAP_PROBE(device_get_softc(dev), dev,
	    hrfkill_devs, hrfkill_map, "RFKILL Switch"));
}

static int
hrfkill_attach(device_t dev)
{
	return (hidmap_attach(device_get_softc(dev)));
}

static int
hrfkill_detach(device_t dev)
{
	return (hidmap_detach(device_get_softc(dev)));
}

static device_method_t hrfkill_methods[] = {
	DEVMETHOD(device_probe,		hrfkill_probe),
	DEVMETHOD(device_attach,	hrfkill_attach),
	DEVMETHOD(device_detach,	hrfkill_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(hrfkill, hrfkill_driver, hrfkill_methods, sizeof(struct hidmap));
DRIVER_MODULE(hrfkill, hidbus, hrfkill_driver, NULL, NULL);
MODULE_DEPEND(hrfkill, hid, 1, 1, 1);
MODULE_DEPEND(hrfkill, hidbus, 1, 1, 1);
MODULE_DEPEND(hrfkill, hidmap, 1, 1, 1);
MODULE_DEPEND(hrfkill, evdev, 1, 1, 1);
MODULE_VERSION(hrfkill, 1);
HID_PNP_INFO(hrfkill_devs);
