/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2014-2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 * MS Windows 7/8/10 compatible HID Multi-touch Device driver.
 * https://msdn.microsoft.com/en-us/library/windows/hardware/jj151569(v=vs.85).aspx
 * http://download.microsoft.com/download/7/d/d/7dd44bb7-2a7a-4505-ac1c-7227d3d96d5b/hid-over-i2c-protocol-spec-v1-0.docx
 * https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt
 */

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>

#define	HID_DEBUG_VAR	hmt_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>

#include <dev/hid/hconf.h>

static SYSCTL_NODE(_hw_hid, OID_AUTO, hmt, CTLFLAG_RW, 0,
    "MSWindows 7/8/10 compatible HID Multi-touch Device");
#ifdef HID_DEBUG
static int hmt_debug = 0;
SYSCTL_INT(_hw_hid_hmt, OID_AUTO, debug, CTLFLAG_RWTUN,
    &hmt_debug, 1, "Debug level");
#endif
static bool hmt_timestamps = 0;
SYSCTL_BOOL(_hw_hid_hmt, OID_AUTO, timestamps, CTLFLAG_RDTUN,
    &hmt_timestamps, 1, "Enable hardware timestamp reporting");

#define	HMT_BTN_MAX	8	/* Number of buttons supported */

enum hmt_type {
	HMT_TYPE_UNKNOWN = 0,	/* HID report descriptor is not probed */
	HMT_TYPE_UNSUPPORTED,	/* Repdescr does not belong to MT device */
	HMT_TYPE_TOUCHPAD,
	HMT_TYPE_TOUCHSCREEN,
};

enum {
	HMT_TIP_SWITCH =	ABS_MT_INDEX(ABS_MT_TOOL_TYPE),
	HMT_WIDTH =		ABS_MT_INDEX(ABS_MT_TOUCH_MAJOR),
	HMT_HEIGHT =		ABS_MT_INDEX(ABS_MT_TOUCH_MINOR),
	HMT_ORIENTATION = 	ABS_MT_INDEX(ABS_MT_ORIENTATION),
	HMT_X =			ABS_MT_INDEX(ABS_MT_POSITION_X),
	HMT_Y =			ABS_MT_INDEX(ABS_MT_POSITION_Y),
	HMT_CONTACTID = 	ABS_MT_INDEX(ABS_MT_TRACKING_ID),
	HMT_PRESSURE =		ABS_MT_INDEX(ABS_MT_PRESSURE),
	HMT_IN_RANGE = 		ABS_MT_INDEX(ABS_MT_DISTANCE),
	HMT_CONFIDENCE = 	ABS_MT_INDEX(ABS_MT_BLOB_ID),
	HMT_TOOL_X =		ABS_MT_INDEX(ABS_MT_TOOL_X),
	HMT_TOOL_Y = 		ABS_MT_INDEX(ABS_MT_TOOL_Y),
};

#define	HMT_N_USAGES	MT_CNT
#define	HMT_NO_USAGE	-1

struct hmt_hid_map_item {
	char		name[5];
	int32_t 	usage;		/* HID usage */
	bool		reported;	/* Item value is passed to evdev */
	bool		required;	/* Required for MT Digitizers */
};

static const struct hmt_hid_map_item hmt_hid_map[HMT_N_USAGES] = {
	[HMT_TIP_SWITCH] = {
		.name = "TIP",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH),
		.reported = false,
		.required = true,
	},
	[HMT_WIDTH] = {
		.name = "WDTH",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_WIDTH),
		.reported = true,
		.required = false,
	},
	[HMT_HEIGHT] = {
		.name = "HGHT",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_HEIGHT),
		.reported = true,
		.required = false,
	},
	[HMT_ORIENTATION] = {
		.name = "ORIE",
		.usage = HMT_NO_USAGE,
		.reported = true,
		.required = false,
	},
	[HMT_X] = {
		.name = "X",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		.reported = true,
		.required = true,
	},
	[HMT_Y] = {
		.name = "Y",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		.reported = true,
		.required = true,
	},
	[HMT_CONTACTID] = {
		.name = "C_ID",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTID),
		.reported = true,
		.required = true,
	},
	[HMT_PRESSURE] = {
		.name = "PRES",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_PRESSURE),
		.reported = true,
		.required = false,
	},
	[HMT_IN_RANGE] = {
		.name = "RANG",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_IN_RANGE),
		.reported = true,
		.required = false,
	},
	[HMT_CONFIDENCE] = {
		.name = "CONF",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIDENCE),
		.reported = false,
		.required = false,
	},
	[HMT_TOOL_X] = { /* Shares HID usage with POS_X */
		.name = "TL_X",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		.reported = true,
		.required = false,
	},
	[HMT_TOOL_Y] = { /* Shares HID usage with POS_Y */
		.name = "TL_Y",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		.reported = true,
		.required = false,
	},
};

struct hmt_softc {
	device_t		dev;
	enum hmt_type		type;

	int32_t			cont_count_max;
	struct hid_absinfo	ai[HMT_N_USAGES];
	struct hid_location	locs[MAX_MT_SLOTS][HMT_N_USAGES];
	struct hid_location	cont_count_loc;
	struct hid_location	btn_loc[HMT_BTN_MAX];
	struct hid_location	int_btn_loc;
	struct hid_location	scan_time_loc;
	int32_t			scan_time_max;
	int32_t			scan_time;
	int32_t			timestamp;
	bool			touch;
	bool			prev_touch;

	struct evdev_dev	*evdev;

	union evdev_mt_slot	slot_data;
	uint8_t			caps[howmany(HMT_N_USAGES, 8)];
	uint8_t			buttons[howmany(HMT_BTN_MAX, 8)];
	uint32_t		nconts_per_report;
	uint32_t		nconts_todo;
	uint8_t			report_id;
	uint32_t		max_button;
	bool			has_int_button;
	bool			has_cont_count;
	bool			has_scan_time;
	bool			is_clickpad;
	bool			do_timestamps;
#ifdef IICHID_SAMPLING
	bool			iichid_sampling;
#endif

	struct hid_location	cont_max_loc;
	uint32_t		cont_max_rlen;
	uint8_t			cont_max_rid;
	struct hid_location	btn_type_loc;
	uint32_t		btn_type_rlen;
	uint8_t			btn_type_rid;
	uint32_t		thqa_cert_rlen;
	uint8_t			thqa_cert_rid;
};

#define	HMT_FOREACH_USAGE(caps, usage)			\
	for ((usage) = 0; (usage) < HMT_N_USAGES; ++(usage))	\
		if (isset((caps), (usage)))

static enum hmt_type hmt_hid_parse(struct hmt_softc *, const void *,
    hid_size_t, uint32_t, uint8_t);
static int hmt_set_input_mode(struct hmt_softc *, enum hconf_input_mode);

static hid_intr_t	hmt_intr;

static device_probe_t	hmt_probe;
static device_attach_t	hmt_attach;
static device_detach_t	hmt_detach;

static evdev_open_t	hmt_ev_open;
static evdev_close_t	hmt_ev_close;

static const struct evdev_methods hmt_evdev_methods = {
	.ev_open = &hmt_ev_open,
	.ev_close = &hmt_ev_close,
};

static const struct hid_device_id hmt_devs[] = {
	{ HID_TLC(HUP_DIGITIZERS, HUD_TOUCHSCREEN) },
	{ HID_TLC(HUP_DIGITIZERS, HUD_TOUCHPAD) },
};

static int
hmt_ev_close(struct evdev_dev *evdev)
{
	return (hidbus_intr_stop(evdev_get_softc(evdev)));
}

static int
hmt_ev_open(struct evdev_dev *evdev)
{
	return (hidbus_intr_start(evdev_get_softc(evdev)));
}

static int
hmt_probe(device_t dev)
{
	struct hmt_softc *sc = device_get_softc(dev);
	void *d_ptr;
	hid_size_t d_len;
	int err;

	err = HIDBUS_LOOKUP_DRIVER_INFO(dev, hmt_devs);
	if (err != 0)
		return (err);

	err = hid_get_report_descr(dev, &d_ptr, &d_len);
	if (err != 0) {
		device_printf(dev, "could not retrieve report descriptor from "
		     "device: %d\n", err);
		return (ENXIO);
	}

	/* Check if report descriptor belongs to a HID multitouch device */
	if (sc->type == HMT_TYPE_UNKNOWN)
		sc->type = hmt_hid_parse(sc, d_ptr, d_len,
		    hidbus_get_usage(dev), hidbus_get_index(dev));
	if (sc->type == HMT_TYPE_UNSUPPORTED)
		return (ENXIO);

	hidbus_set_desc(dev,
	    sc->type == HMT_TYPE_TOUCHPAD ? "TouchPad" : "TouchScreen");

	return (BUS_PROBE_DEFAULT);
}

static int
hmt_attach(device_t dev)
{
	struct hmt_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw = hid_get_device_info(dev);
	void *d_ptr;
	uint8_t *fbuf = NULL;
	hid_size_t d_len, fsize, rsize;
	uint32_t cont_count_max;
	int nbuttons, btn;
	size_t i;
	int err;

	err = hid_get_report_descr(dev, &d_ptr, &d_len);
	if (err != 0) {
		device_printf(dev, "could not retrieve report descriptor from "
		    "device: %d\n", err);
		return (ENXIO);
	}

	sc->dev = dev;

	fsize = hid_report_size_max(d_ptr, d_len, hid_feature, NULL);
	if (fsize != 0)
		fbuf = malloc(fsize, M_TEMP, M_WAITOK | M_ZERO);

	/* Fetch and parse "Contact count maximum" feature report */
	if (sc->cont_max_rlen > 1) {
		err = hid_get_report(dev, fbuf, sc->cont_max_rlen, &rsize,
		    HID_FEATURE_REPORT, sc->cont_max_rid);
		if (err == 0 && (rsize - 1) * 8 >=
		    sc->cont_max_loc.pos + sc->cont_max_loc.size) {
			cont_count_max = hid_get_udata(fbuf + 1,
			    sc->cont_max_rlen - 1, &sc->cont_max_loc);
			/*
			 * Feature report is a primary source of
			 * 'Contact Count Maximum'
			 */
			if (cont_count_max > 0)
				sc->cont_count_max = cont_count_max;
		} else
			DPRINTF("hid_get_report error=%d\n", err);
	}
	if (sc->cont_count_max == 0)
		sc->cont_count_max = sc->type == HMT_TYPE_TOUCHSCREEN ? 10 : 5;

	/* Fetch and parse "Button type" feature report */
	if (sc->btn_type_rlen > 1 && sc->btn_type_rid != sc->cont_max_rid) {
		bzero(fbuf, fsize);
		err = hid_get_report(dev, fbuf, sc->btn_type_rlen, &rsize,
		    HID_FEATURE_REPORT, sc->btn_type_rid);
		if (err != 0)
			DPRINTF("hid_get_report error=%d\n", err);
	}
	if (sc->btn_type_rlen > 1 && err == 0 && (rsize - 1) * 8 >=
	    sc->btn_type_loc.pos + sc->btn_type_loc.size)
		sc->is_clickpad = hid_get_udata(fbuf + 1, sc->btn_type_rlen - 1,
		    &sc->btn_type_loc) == 0;
	else
		sc->is_clickpad = sc->max_button == 0 && sc->has_int_button;

	/* Fetch THQA certificate to enable some devices like WaveShare */
	if (sc->thqa_cert_rlen > 1 && sc->thqa_cert_rid != sc->cont_max_rid)
		(void)hid_get_report(dev, fbuf, sc->thqa_cert_rlen, NULL,
		    HID_FEATURE_REPORT, sc->thqa_cert_rid);

	free(fbuf, M_TEMP);

	/* Switch touchpad in to absolute multitouch mode */
	if (sc->type == HMT_TYPE_TOUCHPAD) {
		err = hmt_set_input_mode(sc, HCONF_INPUT_MODE_MT_TOUCHPAD);
		if (err != 0)
			DPRINTF("Failed to set input mode: %d\n", err);
	}

	/* Cap contact count maximum to MAX_MT_SLOTS */
	if (sc->cont_count_max > MAX_MT_SLOTS) {
		DPRINTF("Hardware reported %d contacts while only %d is "
		    "supported\n", sc->cont_count_max, MAX_MT_SLOTS);
		sc->cont_count_max = MAX_MT_SLOTS;
	}

	if (sc->has_scan_time &&
	    (hid_test_quirk(hw, HQ_MT_TIMESTAMP) || hmt_timestamps))
		sc->do_timestamps = true;
#ifdef IICHID_SAMPLING
	if (hid_test_quirk(hw, HQ_IICHID_SAMPLING))
		sc->iichid_sampling = true;
#endif

	hidbus_set_intr(dev, hmt_intr, sc);

	sc->evdev = evdev_alloc();
	evdev_set_name(sc->evdev, device_get_desc(dev));
	evdev_set_phys(sc->evdev, device_get_nameunit(dev));
	evdev_set_id(sc->evdev, hw->idBus, hw->idVendor, hw->idProduct,
	    hw->idVersion);
	evdev_set_serial(sc->evdev, hw->serial);
	evdev_set_methods(sc->evdev, dev, &hmt_evdev_methods);
	evdev_set_flag(sc->evdev, EVDEV_FLAG_MT_STCOMPAT);
	evdev_set_flag(sc->evdev, EVDEV_FLAG_EXT_EPOCH); /* hidbus child */
	switch (sc->type) {
	case HMT_TYPE_TOUCHSCREEN:
		evdev_support_prop(sc->evdev, INPUT_PROP_DIRECT);
		break;
	case HMT_TYPE_TOUCHPAD:
		evdev_support_prop(sc->evdev, INPUT_PROP_POINTER);
		if (sc->is_clickpad)
			evdev_support_prop(sc->evdev, INPUT_PROP_BUTTONPAD);
		break;
	default:
		KASSERT(0, ("hmt_attach: unsupported touch device type"));
	}
	evdev_support_event(sc->evdev, EV_SYN);
	evdev_support_event(sc->evdev, EV_ABS);
	if (sc->do_timestamps) {
		evdev_support_event(sc->evdev, EV_MSC);
		evdev_support_msc(sc->evdev, MSC_TIMESTAMP);
	}
#ifdef IICHID_SAMPLING
	if (sc->iichid_sampling)
		evdev_set_flag(sc->evdev, EVDEV_FLAG_MT_AUTOREL);
#endif
	nbuttons = 0;
	if (sc->max_button != 0 || sc->has_int_button) {
		evdev_support_event(sc->evdev, EV_KEY);
		if (sc->has_int_button)
			evdev_support_key(sc->evdev, BTN_LEFT);
		for (btn = 0; btn < sc->max_button; ++btn) {
			if (isset(sc->buttons, btn)) {
				evdev_support_key(sc->evdev, BTN_MOUSE + btn);
				nbuttons++;
			}
		}
	}
	evdev_support_abs(sc->evdev,
	    ABS_MT_SLOT, 0, sc->cont_count_max - 1, 0, 0, 0);
	HMT_FOREACH_USAGE(sc->caps, i) {
		if (hmt_hid_map[i].reported)
			evdev_support_abs(sc->evdev, ABS_MT_FIRST + i,
			    sc->ai[i].min, sc->ai[i].max, 0, 0, sc->ai[i].res);
	}

	err = evdev_register(sc->evdev);
	if (err) {
		hmt_detach(dev);
		return (ENXIO);
	}

	/* Announce information about the touch device */
	device_printf(sc->dev, "%s %s with %d external button%s%s\n",
	    sc->cont_count_max > 1 ? "Multitouch" : "Singletouch",
	    sc->type == HMT_TYPE_TOUCHSCREEN ? "touchscreen" : "touchpad",
	    nbuttons, nbuttons != 1 ? "s" : "",
	    sc->is_clickpad ? ", click-pad" : "");
	device_printf(sc->dev,
	    "%d contact%s with [%s%s%s%s%s] properties. Report range [%d:%d] - [%d:%d]\n",
	    (int)sc->cont_count_max, sc->cont_count_max != 1 ? "s" : "",
	    isset(sc->caps, HMT_IN_RANGE) ? "R" : "",
	    isset(sc->caps, HMT_CONFIDENCE) ? "C" : "",
	    isset(sc->caps, HMT_WIDTH) ? "W" : "",
	    isset(sc->caps, HMT_HEIGHT) ? "H" : "",
	    isset(sc->caps, HMT_PRESSURE) ? "P" : "",
	    (int)sc->ai[HMT_X].min, (int)sc->ai[HMT_Y].min,
	    (int)sc->ai[HMT_X].max, (int)sc->ai[HMT_Y].max);

	return (0);
}

static int
hmt_detach(device_t dev)
{
	struct hmt_softc *sc = device_get_softc(dev);

	evdev_free(sc->evdev);

	return (0);
}

static void
hmt_intr(void *context, void *buf, hid_size_t len)
{
	struct hmt_softc *sc = context;
	size_t usage;
	union evdev_mt_slot *slot_data;
	uint32_t cont, btn;
	uint32_t cont_count;
	uint32_t width;
	uint32_t height;
	uint32_t int_btn = 0;
	uint32_t left_btn = 0;
	int slot;
	uint32_t scan_time;
	int32_t delta;
	uint8_t id;

#ifdef IICHID_SAMPLING
	/*
	 * Special packet of zero length is generated by iichid driver running
	 * in polling mode at the start of inactivity period to workaround
	 * "stuck touch" problem caused by miss of finger release events.
	 * This snippet is to be removed after GPIO interrupt support is added.
	 */
	if (sc->iichid_sampling && len == 0) {
		sc->prev_touch = false;
		sc->timestamp = 0;
		/* EVDEV_FLAG_MT_AUTOREL releases all touches for us */
		evdev_sync(sc->evdev);
		return;
	}
#endif

	/* Ignore irrelevant reports */
	id = sc->report_id != 0 ? *(uint8_t *)buf : 0;
	if (sc->report_id != id) {
		DPRINTF("Skip report with unexpected ID: %hhu\n", id);
		return;
	}

	/* Strip leading "report ID" byte */
	if (sc->report_id != 0) {
		len--;
		buf = (uint8_t *)buf + 1;
	}

	/*
	 * "In Serial mode, each packet contains information that describes a
	 * single physical contact point. Multiple contacts are streamed
	 * serially. In this mode, devices report all contact information in a
	 * series of packets. The device sends a separate packet for each
	 * concurrent contact."
	 *
	 * "In Parallel mode, devices report all contact information in a
	 * single packet. Each physical contact is represented by a logical
	 * collection that is embedded in the top-level collection."
	 *
	 * Since additional contacts that were not present will still be in the
	 * report with contactid=0 but contactids are zero-based, find
	 * contactcount first.
	 */
	if (sc->has_cont_count)
		cont_count = hid_get_udata(buf, len, &sc->cont_count_loc);
	else
		cont_count = 1;
	/*
	 * "In Hybrid mode, the number of contacts that can be reported in one
	 * report is less than the maximum number of contacts that the device
	 * supports. For example, a device that supports a maximum of
	 * 4 concurrent physical contacts, can set up its top-level collection
	 * to deliver a maximum of two contacts in one report. If four contact
	 * points are present, the device can break these up into two serial
	 * reports that deliver two contacts each.
	 *
	 * "When a device delivers data in this manner, the Contact Count usage
	 * value in the first report should reflect the total number of
	 * contacts that are being delivered in the hybrid reports. The other
	 * serial reports should have a contact count of zero (0)."
	 */
	if (cont_count != 0)
		sc->nconts_todo = cont_count;

#ifdef HID_DEBUG
	DPRINTFN(6, "cont_count:%2u", (unsigned)cont_count);
	if (hmt_debug >= 6) {
		HMT_FOREACH_USAGE(sc->caps, usage) {
			if (hmt_hid_map[usage].usage != HMT_NO_USAGE)
				printf(" %-4s", hmt_hid_map[usage].name);
		}
		printf("\n");
	}
#endif

	/* Find the number of contacts reported in current report */
	cont_count = MIN(sc->nconts_todo, sc->nconts_per_report);

	/* Use protocol Type B for reporting events */
	for (cont = 0; cont < cont_count; cont++) {
		slot_data = &sc->slot_data;
		bzero(slot_data, sizeof(sc->slot_data));
		HMT_FOREACH_USAGE(sc->caps, usage) {
			if (sc->locs[cont][usage].size > 0)
				slot_data->val[usage] = hid_get_udata(
				    buf, len, &sc->locs[cont][usage]);
		}

		slot = evdev_mt_id_to_slot(sc->evdev, slot_data->id);

#ifdef HID_DEBUG
		DPRINTFN(6, "cont%01x: data = ", cont);
		if (hmt_debug >= 6) {
			HMT_FOREACH_USAGE(sc->caps, usage) {
				if (hmt_hid_map[usage].usage != HMT_NO_USAGE)
					printf("%04x ", slot_data->val[usage]);
			}
			printf("slot = %d\n", slot);
		}
#endif

		if (slot == -1) {
			DPRINTF("Slot overflow for contact_id %u\n",
			    (unsigned)slot_data->id);
			continue;
		}

		if (slot_data->val[HMT_TIP_SWITCH] != 0 &&
		    !(isset(sc->caps, HMT_CONFIDENCE) &&
		      slot_data->val[HMT_CONFIDENCE] == 0)) {
			/* This finger is in proximity of the sensor */
			sc->touch = true;
			slot_data->dist = !slot_data->val[HMT_IN_RANGE];
			/* Divided by two to match visual scale of touch */
			width = slot_data->val[HMT_WIDTH] >> 1;
			height = slot_data->val[HMT_HEIGHT] >> 1;
			slot_data->ori = width > height;
			slot_data->maj = MAX(width, height);
			slot_data->min = MIN(width, height);
		} else
			slot_data = NULL;

		evdev_mt_push_slot(sc->evdev, slot, slot_data);
	}

	sc->nconts_todo -= cont_count;
	if (sc->do_timestamps && sc->nconts_todo == 0) {
		/* HUD_SCAN_TIME is measured in 100us, convert to us. */
		scan_time = hid_get_udata(buf, len, &sc->scan_time_loc);
		if (sc->prev_touch) {
			delta = scan_time - sc->scan_time;
			if (delta < 0)
				delta += sc->scan_time_max;
		} else
			delta = 0;
		sc->scan_time = scan_time;
		sc->timestamp += delta * 100;
		evdev_push_msc(sc->evdev, MSC_TIMESTAMP, sc->timestamp);
		sc->prev_touch = sc->touch;
		sc->touch = false;
		if (!sc->prev_touch)
			sc->timestamp = 0;
	}
	if (sc->nconts_todo == 0) {
		/* Report both the click and external left btns as BTN_LEFT */
		if (sc->has_int_button)
			int_btn = hid_get_data(buf, len, &sc->int_btn_loc);
		if (isset(sc->buttons, 0))
			left_btn = hid_get_data(buf, len, &sc->btn_loc[0]);
		if (sc->has_int_button || isset(sc->buttons, 0))
			evdev_push_key(sc->evdev, BTN_LEFT,
			    (int_btn != 0) | (left_btn != 0));
		for (btn = 1; btn < sc->max_button; ++btn) {
			if (isset(sc->buttons, btn))
				evdev_push_key(sc->evdev, BTN_MOUSE + btn,
				    hid_get_data(buf,
						 len,
						 &sc->btn_loc[btn]) != 0);
		}
		evdev_sync(sc->evdev);
	}
}

static enum hmt_type
hmt_hid_parse(struct hmt_softc *sc, const void *d_ptr, hid_size_t d_len,
    uint32_t tlc_usage, uint8_t tlc_index)
{
	struct hid_absinfo ai;
	struct hid_item hi;
	struct hid_data *hd;
	uint32_t flags;
	size_t i;
	size_t cont = 0;
	enum hmt_type type;
	uint32_t left_btn, btn;
	int32_t cont_count_max = 0;
	uint8_t report_id = 0;
	bool finger_coll = false;
	bool cont_count_found = false;
	bool scan_time_found = false;
	bool has_int_button = false;

#define HMT_HI_ABSOLUTE(hi)	((hi).nusages != 0 &&	\
	((hi).flags & (HIO_VARIABLE | HIO_RELATIVE)) == HIO_VARIABLE)
#define	HUMS_THQA_CERT	0xC5

	/*
	 * Get left button usage taking in account MS Precision Touchpad specs.
	 * For Windows PTP report descriptor assigns buttons in following way:
	 * Button 1 - Indicates Button State for touchpad button integrated
	 *            with digitizer.
	 * Button 2 - Indicates Button State for external button for primary
	 *            (default left) clicking.
	 * Button 3 - Indicates Button State for external button for secondary
	 *            (default right) clicking.
	 * If a device only supports external buttons, it must still use
	 * Button 2 and Button 3 to reference the external buttons.
	 */
	switch (tlc_usage) {
	case HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN):
		type = HMT_TYPE_TOUCHSCREEN;
		left_btn = 1;
		break;
	case HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHPAD):
		type = HMT_TYPE_TOUCHPAD;
		left_btn = 2;
		break;
	default:
		return (HMT_TYPE_UNSUPPORTED);
	}

	/* Parse features for mandatory maximum contact count usage */
	if (!hidbus_locate(d_ptr, d_len,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACT_MAX), hid_feature,
	    tlc_index, 0, &sc->cont_max_loc, &flags, &sc->cont_max_rid, &ai) ||
	    (flags & (HIO_VARIABLE | HIO_RELATIVE)) != HIO_VARIABLE)
		return (HMT_TYPE_UNSUPPORTED);

	cont_count_max = ai.max;

	/* Parse features for button type usage */
	if (hidbus_locate(d_ptr, d_len,
	    HID_USAGE2(HUP_DIGITIZERS, HUD_BUTTON_TYPE), hid_feature,
	    tlc_index, 0, &sc->btn_type_loc, &flags, &sc->btn_type_rid, NULL)
	    && (flags & (HIO_VARIABLE | HIO_RELATIVE)) != HIO_VARIABLE)
		sc->btn_type_rid = 0;

	/* Parse features for THQA certificate report ID */
	hidbus_locate(d_ptr, d_len, HID_USAGE2(HUP_MICROSOFT, HUMS_THQA_CERT),
	    hid_feature, tlc_index, 0, NULL, NULL, &sc->thqa_cert_rid, NULL);

	/* Parse input for other parameters */
	hd = hid_start_parse(d_ptr, d_len, 1 << hid_input);
	HIDBUS_FOREACH_ITEM(hd, &hi, tlc_index) {
		switch (hi.kind) {
		case hid_collection:
			if (hi.collevel == 2 &&
			    hi.usage == HID_USAGE2(HUP_DIGITIZERS, HUD_FINGER))
				finger_coll = true;
			break;
		case hid_endcollection:
			if (hi.collevel == 1 && finger_coll) {
				finger_coll = false;
				cont++;
			}
			break;
		case hid_input:
			/*
			 * Ensure that all usages belong to the same report
			 */
			if (HMT_HI_ABSOLUTE(hi) &&
			    (report_id == 0 || report_id == hi.report_ID))
				report_id = hi.report_ID;
			else
				break;

			if (hi.collevel == 1 && left_btn == 2 &&
			    hi.usage == HID_USAGE2(HUP_BUTTON, 1)) {
				has_int_button = true;
				sc->int_btn_loc = hi.loc;
				break;
			}
			if (hi.collevel == 1 &&
			    hi.usage >= HID_USAGE2(HUP_BUTTON, left_btn) &&
			    hi.usage <= HID_USAGE2(HUP_BUTTON, HMT_BTN_MAX)) {
				btn = (hi.usage & 0xFFFF) - left_btn;
				setbit(sc->buttons, btn);
				sc->btn_loc[btn] = hi.loc;
				if (btn >= sc->max_button)
					sc->max_button = btn + 1;
				break;
			}
			if (hi.collevel == 1 && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTCOUNT)) {
				cont_count_found = true;
				sc->cont_count_loc = hi.loc;
				break;
			}
			if (hi.collevel == 1 && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_SCAN_TIME)) {
				scan_time_found = true;
				sc->scan_time_loc = hi.loc;
				sc->scan_time_max = hi.logical_maximum;
				break;
			}

			if (!finger_coll || hi.collevel != 2)
				break;
			if (cont >= MAX_MT_SLOTS) {
				DPRINTF("Finger %zu ignored\n", cont);
				break;
			}

			for (i = 0; i < HMT_N_USAGES; i++) {
				if (hi.usage == hmt_hid_map[i].usage) {
					/*
					 * HUG_X usage is an array mapped to
					 * both ABS_MT_POSITION and ABS_MT_TOOL
					 * events. So don`t stop search if we
					 * already have HUG_X mapping done.
					 */
					if (sc->locs[cont][i].size)
						continue;
					sc->locs[cont][i] = hi.loc;
					/*
					 * Hid parser returns valid logical and
					 * physical sizes for first finger only
					 * at least on ElanTS 0x04f3:0x0012.
					 */
					if (cont > 0)
						break;
					setbit(sc->caps, i);
					sc->ai[i] = (struct hid_absinfo) {
					    .max = hi.logical_maximum,
					    .min = hi.logical_minimum,
					    .res = hid_item_resolution(&hi),
					};
					break;
				}
			}
			break;
		default:
			break;
		}
	}
	hid_end_parse(hd);

	/* Check for required HID Usages */
	if ((!cont_count_found && cont != 1) || cont == 0)
		return (HMT_TYPE_UNSUPPORTED);
	for (i = 0; i < HMT_N_USAGES; i++) {
		if (hmt_hid_map[i].required && isclr(sc->caps, i))
			return (HMT_TYPE_UNSUPPORTED);
	}

	/* Touchpads must have at least one button */
	if (type == HMT_TYPE_TOUCHPAD && !sc->max_button && !has_int_button)
		return (HMT_TYPE_UNSUPPORTED);

	/*
	 * According to specifications 'Contact Count Maximum' should be read
	 * from Feature Report rather than from HID descriptor. Set sane
	 * default value now to handle the case of 'Get Report' request failure
	 */
	if (cont_count_max < 1)
		cont_count_max = cont;

	/* Report touch orientation if both width and height are supported */
	if (isset(sc->caps, HMT_WIDTH) && isset(sc->caps, HMT_HEIGHT)) {
		setbit(sc->caps, HMT_ORIENTATION);
		sc->ai[HMT_ORIENTATION].max = 1;
	}

	sc->cont_max_rlen = hid_report_size(d_ptr, d_len, hid_feature,
	    sc->cont_max_rid);
	if (sc->btn_type_rid > 0)
		sc->btn_type_rlen = hid_report_size(d_ptr, d_len,
		    hid_feature, sc->btn_type_rid);
	if (sc->thqa_cert_rid > 0)
		sc->thqa_cert_rlen = hid_report_size(d_ptr, d_len,
		    hid_feature, sc->thqa_cert_rid);

	sc->report_id = report_id;
	sc->cont_count_max = cont_count_max;
	sc->nconts_per_report = cont;
	sc->has_int_button = has_int_button;
	sc->has_cont_count = cont_count_found;
	sc->has_scan_time = scan_time_found;

	return (type);
}

static int
hmt_set_input_mode(struct hmt_softc *sc, enum hconf_input_mode mode)
{
	devclass_t hconf_devclass;
	device_t hconf;
	int  err;

	bus_topo_assert();

	/* Find touchpad's configuration TLC */
	hconf = hidbus_find_child(device_get_parent(sc->dev),
	    HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIG));
	if (hconf == NULL)
		return (ENXIO);

	/* Ensure that hconf driver is attached to configuration TLC */
	if (device_is_alive(hconf) == 0)
		device_probe_and_attach(hconf);
	if (device_is_attached(hconf) == 0)
		return (ENXIO);
	hconf_devclass = devclass_find("hconf");
	if (device_get_devclass(hconf) != hconf_devclass)
		return (ENXIO);

	/* hconf_set_input_mode can drop the topo lock while sleeping */
	device_busy(hconf);
	err = hconf_set_input_mode(hconf, mode);
	device_unbusy(hconf);

	return (err);
}

static device_method_t hmt_methods[] = {
	DEVMETHOD(device_probe,		hmt_probe),
	DEVMETHOD(device_attach,	hmt_attach),
	DEVMETHOD(device_detach,	hmt_detach),

	DEVMETHOD_END
};

static driver_t hmt_driver = {
	.name = "hmt",
	.methods = hmt_methods,
	.size = sizeof(struct hmt_softc),
};

DRIVER_MODULE(hmt, hidbus, hmt_driver, NULL, NULL);
MODULE_DEPEND(hmt, hidbus, 1, 1, 1);
MODULE_DEPEND(hmt, hid, 1, 1, 1);
MODULE_DEPEND(hmt, hconf, 1, 1, 1);
MODULE_DEPEND(hmt, evdev, 1, 1, 1);
MODULE_VERSION(hmt, 1);
HID_PNP_INFO(hmt_devs);
