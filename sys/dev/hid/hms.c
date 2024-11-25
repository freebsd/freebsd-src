/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 * HID spec: https://www.usb.org/sites/default/files/documents/hid1_11.pdf
 */

#include "opt_hid.h"

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
#include <dev/hid/hidquirk.h>
#include <dev/hid/hidrdesc.h>

static const uint8_t hms_boot_desc[] = { HID_MOUSE_BOOTPROTO_DESCR() };

enum {
	HMS_REL_X,
	HMS_REL_Y,
	HMS_REL_Z,
	HMS_ABS_X,
	HMS_ABS_Y,
	HMS_ABS_Z,
	HMS_HWHEEL,
	HMS_BTN,
	HMS_FINAL_CB,
};

static hidmap_cb_t	hms_final_cb;
#ifdef IICHID_SAMPLING
static hid_intr_t	hms_intr;
#endif

#define HMS_MAP_BUT_RG(usage_from, usage_to, code)	\
	{ HIDMAP_KEY_RANGE(HUP_BUTTON, usage_from, usage_to, code) }
#define HMS_MAP_BUT_MS(usage, code)	\
	{ HIDMAP_KEY(HUP_MICROSOFT, usage, code) }
#define HMS_MAP_ABS(usage, code)	\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, usage, code) }
#define HMS_MAP_REL(usage, code)	\
	{ HIDMAP_REL(HUP_GENERIC_DESKTOP, usage, code) }
#define HMS_MAP_REL_REV(usage, code)	\
	{ HIDMAP_REL(HUP_GENERIC_DESKTOP, usage, code), .invert_value = true }
#define HMS_MAP_REL_CN(usage, code)	\
	{ HIDMAP_REL(HUP_CONSUMER, usage, code) }
#define	HMS_FINAL_CB(cb)		\
	{ HIDMAP_FINAL_CB(&cb) }

static const struct hidmap_item hms_map[] = {
	[HMS_REL_X]	= HMS_MAP_REL(HUG_X,		REL_X),
	[HMS_REL_Y]	= HMS_MAP_REL(HUG_Y,		REL_Y),
	[HMS_REL_Z]	= HMS_MAP_REL(HUG_Z,		REL_Z),
	[HMS_ABS_X]	= HMS_MAP_ABS(HUG_X,		ABS_X),
	[HMS_ABS_Y]	= HMS_MAP_ABS(HUG_Y,		ABS_Y),
	[HMS_ABS_Z]	= HMS_MAP_ABS(HUG_Z,		ABS_Z),
	[HMS_HWHEEL]	= HMS_MAP_REL_CN(HUC_AC_PAN,	REL_HWHEEL),
	[HMS_BTN]	= HMS_MAP_BUT_RG(1, 16,		BTN_MOUSE),
	[HMS_FINAL_CB]	= HMS_FINAL_CB(hms_final_cb),
};

static const struct hidmap_item hms_map_wheel[] = {
	HMS_MAP_REL(HUG_WHEEL,		REL_WHEEL),
};
static const struct hidmap_item hms_map_wheel_rev[] = {
	HMS_MAP_REL_REV(HUG_WHEEL,	REL_WHEEL),
};

static const struct hidmap_item hms_map_kensington_slimblade[] = {
	HMS_MAP_BUT_MS(1,	BTN_RIGHT),
	HMS_MAP_BUT_MS(2,	BTN_MIDDLE),
};

/* A match on these entries will load hms */
static const struct hid_device_id hms_devs[] = {
	{ HID_TLC(HUP_GENERIC_DESKTOP, HUG_POINTER) },
	{ HID_TLC(HUP_GENERIC_DESKTOP, HUG_MOUSE) },
};

struct hms_softc {
	struct hidmap		hm;
	HIDMAP_CAPS(caps, hms_map);
#ifdef IICHID_SAMPLING
	bool			iichid_sampling;
	void			*last_ir;
	hid_size_t		last_irsize;
	hid_size_t		isize;
	uint32_t		drift_cnt;
	uint32_t		drift_thresh;
	struct hid_location	wheel_loc;
#endif
};

#ifdef IICHID_SAMPLING
static void
hms_intr(void *context, void *buf, hid_size_t len)
{
	struct hidmap *hm = context;
	struct hms_softc *sc = device_get_softc(hm->dev);
	int32_t wheel;

	if (len > sc->isize)
		len = sc->isize;

	/*
	 * Many I2C "compatibility" mouse devices found on touchpads continue
	 * to return last report data in sampling mode even after touch has
	 * been ended.  That results in cursor drift.  Filter out such a
	 * reports through comparing with previous one.
	 *
	 * Except this results in dropping consecutive mouse wheel events,
	 * because differently from cursor movement they always move by the
	 * same amount.  So, don't do it when there's mouse wheel movement.
	 */
	if (sc->wheel_loc.size != 0)
		wheel = hid_get_data(buf, len, &sc->wheel_loc);
	else
		wheel = 0;

	if (len == sc->last_irsize && memcmp(buf, sc->last_ir, len) == 0 &&
	    wheel == 0) {
		sc->drift_cnt++;
		if (sc->drift_thresh != 0 && sc->drift_cnt >= sc->drift_thresh)
			return;
	} else {
		sc->drift_cnt = 0;
		sc->last_irsize = len;
		bcopy(buf, sc->last_ir, len);
	}

	hidmap_intr(context, buf, len);
}
#endif

static int
hms_final_cb(HIDMAP_CB_ARGS)
{
	struct hms_softc *sc = HIDMAP_CB_GET_SOFTC();
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING) {
		if (hidmap_test_cap(sc->caps, HMS_ABS_X) ||
		    hidmap_test_cap(sc->caps, HMS_ABS_Y))
			evdev_support_prop(evdev, INPUT_PROP_DIRECT);
		else
			evdev_support_prop(evdev, INPUT_PROP_POINTER);
#ifdef IICHID_SAMPLING
		/* Overload interrupt handler to skip identical reports */
		if (sc->iichid_sampling)
			hidbus_set_intr(sc->hm.dev, hms_intr, &sc->hm);
#endif
	}

	/* Do not execute callback at interrupt handler and detach */
	return (ENOSYS);
}

static void
hms_identify(driver_t *driver, device_t parent)
{
	const struct hid_device_info *hw = hid_get_device_info(parent);
	void *d_ptr;
	hid_size_t d_len;
	int error;

	/*
	 * If device claimed boot protocol support but do not have report
	 * descriptor, load one defined in "Appendix B.2" of HID1_11.pdf
	 */
	error = hid_get_report_descr(parent, &d_ptr, &d_len);
	if ((error != 0 && hid_test_quirk(hw, HQ_HAS_MS_BOOTPROTO)) ||
	    (error == 0 && hid_test_quirk(hw, HQ_MS_BOOTPROTO) &&
	     hid_is_mouse(d_ptr, d_len)))
		(void)hid_set_report_descr(parent, hms_boot_desc,
		    sizeof(hms_boot_desc));
}

static int
hms_probe(device_t dev)
{
	struct hms_softc *sc = device_get_softc(dev);
	int error;

	error = HIDBUS_LOOKUP_DRIVER_INFO(dev, hms_devs);
	if (error != 0)
		return (error);

	hidmap_set_dev(&sc->hm, dev);

	/* Check if report descriptor belongs to mouse */
	error = HIDMAP_ADD_MAP(&sc->hm, hms_map, sc->caps);
	if (error != 0)
		return (error);

	/* There should be at least one X or Y axis */
	if (!hidmap_test_cap(sc->caps, HMS_REL_X) &&
	    !hidmap_test_cap(sc->caps, HMS_REL_Y) &&
	    !hidmap_test_cap(sc->caps, HMS_ABS_X) &&
	    !hidmap_test_cap(sc->caps, HMS_ABS_Y))
		return (ENXIO);

	if (hidmap_test_cap(sc->caps, HMS_ABS_X) ||
	    hidmap_test_cap(sc->caps, HMS_ABS_Y))
		hidbus_set_desc(dev, "Tablet");
	else
		hidbus_set_desc(dev, "Mouse");

	return (BUS_PROBE_GENERIC);
}

static int
hms_attach(device_t dev)
{
	struct hms_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw = hid_get_device_info(dev);
	struct hidmap_hid_item *hi;
	HIDMAP_CAPS(cap_wheel, hms_map_wheel);
	void *d_ptr;
	hid_size_t d_len;
	bool set_report_proto;
	int error, nbuttons = 0;

	/*
	 * Set the report (non-boot) protocol if report descriptor has not been
	 * overloaded with boot protocol report descriptor.
	 *
	 * Mice without boot protocol support may choose not to implement
	 * Set_Protocol at all; Ignore any error.
	 */
	error = hid_get_report_descr(dev, &d_ptr, &d_len);
	set_report_proto = !(error == 0 && d_len == sizeof(hms_boot_desc) &&
	    memcmp(d_ptr, hms_boot_desc, sizeof(hms_boot_desc)) == 0);
	(void)hid_set_protocol(dev, set_report_proto ? 1 : 0);

	if (hid_test_quirk(hw, HQ_MS_REVZ))
		HIDMAP_ADD_MAP(&sc->hm, hms_map_wheel_rev, cap_wheel);
	else
		HIDMAP_ADD_MAP(&sc->hm, hms_map_wheel, cap_wheel);

	if (hid_test_quirk(hw, HQ_MS_VENDOR_BTN))
		HIDMAP_ADD_MAP(&sc->hm, hms_map_kensington_slimblade, NULL);

#ifdef IICHID_SAMPLING
	if (hid_test_quirk(hw, HQ_IICHID_SAMPLING) &&
	    hidmap_test_cap(sc->caps, HMS_REL_X) &&
	    hidmap_test_cap(sc->caps, HMS_REL_Y)) {
		sc->iichid_sampling = true;
		sc->isize = hid_report_size_max(d_ptr, d_len, hid_input, NULL);
		sc->last_ir = malloc(sc->isize, M_DEVBUF, M_WAITOK | M_ZERO);
		sc->drift_thresh = 2;
		SYSCTL_ADD_U32(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    "drift_thresh", CTLFLAG_RW, &sc->drift_thresh, 0,
		    "drift detection threshold");
	}
#endif

	error = hidmap_attach(&sc->hm);
	if (error)
		return (error);

	/* Count number of input usages of variable type mapped to buttons */
	for (hi = sc->hm.hid_items;
	     hi < sc->hm.hid_items + sc->hm.nhid_items;
	     hi++) {
		if (hi->type == HIDMAP_TYPE_VARIABLE && hi->evtype == EV_KEY)
			nbuttons++;
#ifdef IICHID_SAMPLING
		/*
		 * Make note of which part of the report descriptor is the wheel.
		 */
		if (hi->type == HIDMAP_TYPE_VARIABLE &&
		    hi->evtype == EV_REL && hi->code == REL_WHEEL) {
			sc->wheel_loc = hi->loc;
			/*
			 * Account for the leading Report ID byte
			 * if it is a multi-report device.
			 */
			if (hi->id != 0)
				sc->wheel_loc.pos += 8;
		}
#endif
	}

	/* announce information about the mouse */
	device_printf(dev, "%d buttons and [%s%s%s%s%s] coordinates ID=%u\n",
	    nbuttons,
	    (hidmap_test_cap(sc->caps, HMS_REL_X) ||
	     hidmap_test_cap(sc->caps, HMS_ABS_X)) ? "X" : "",
	    (hidmap_test_cap(sc->caps, HMS_REL_Y) ||
	     hidmap_test_cap(sc->caps, HMS_ABS_Y)) ? "Y" : "",
	    (hidmap_test_cap(sc->caps, HMS_REL_Z) ||
	     hidmap_test_cap(sc->caps, HMS_ABS_Z)) ? "Z" : "",
	    hidmap_test_cap(cap_wheel, 0) ? "W" : "",
	    hidmap_test_cap(sc->caps, HMS_HWHEEL) ? "H" : "",
	    sc->hm.hid_items[0].id);

	return (0);
}

static int
hms_detach(device_t dev)
{
	struct hms_softc *sc = device_get_softc(dev);
	int error;

	error = hidmap_detach(&sc->hm);
#ifdef IICHID_SAMPLING
	if (error == 0)
		free(sc->last_ir, M_DEVBUF);
#endif
	return (error);
}

static device_method_t hms_methods[] = {
	DEVMETHOD(device_identify,	hms_identify),
	DEVMETHOD(device_probe,		hms_probe),
	DEVMETHOD(device_attach,	hms_attach),
	DEVMETHOD(device_detach,	hms_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(hms, hms_driver, hms_methods, sizeof(struct hms_softc));
DRIVER_MODULE(hms, hidbus, hms_driver, NULL, NULL);
MODULE_DEPEND(hms, hid, 1, 1, 1);
MODULE_DEPEND(hms, hidbus, 1, 1, 1);
MODULE_DEPEND(hms, hidmap, 1, 1, 1);
MODULE_DEPEND(hms, evdev, 1, 1, 1);
MODULE_VERSION(hms, 1);
HID_PNP_INFO(hms_devs);
