/*-
 * Copyright (c) 2014-2017 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * MS Windows 7/8/10 compatible USB HID Multi-touch Device driver.
 * https://msdn.microsoft.com/en-us/library/windows/hardware/jj151569(v=vs.85).aspx
 * https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/stddef.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/hid/hid.h>

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/quirk/usb_quirk.h>

#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>

#define	USB_DEBUG_VAR wmt_debug
#include <dev/usb/usb_debug.h>

static SYSCTL_NODE(_hw_usb, OID_AUTO, wmt, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB MSWindows 7/8/10 compatible Multi-touch Device");
#ifdef USB_DEBUG
static int wmt_debug = 0;
SYSCTL_INT(_hw_usb_wmt, OID_AUTO, debug, CTLFLAG_RWTUN,
    &wmt_debug, 1, "Debug level");
#endif
static bool wmt_timestamps = 0;
SYSCTL_BOOL(_hw_usb_wmt, OID_AUTO, timestamps, CTLFLAG_RDTUN,
    &wmt_timestamps, 1, "Enable hardware timestamp reporting");

#define	WMT_BSIZE	1024	/* bytes, buffer size */
#define	WMT_BTN_MAX	8	/* Number of buttons supported */

enum {
	WMT_INTR_DT,
	WMT_N_TRANSFER,
};

enum wmt_type {
	WMT_TYPE_UNKNOWN = 0,	/* HID report descriptor is not probed */
	WMT_TYPE_UNSUPPORTED,	/* Repdescr does not belong to MT device */
	WMT_TYPE_TOUCHPAD,
	WMT_TYPE_TOUCHSCREEN,
};

enum wmt_input_mode {
	WMT_INPUT_MODE_MOUSE =		0x0,
	WMT_INPUT_MODE_MT_TOUCHSCREEN =	0x2,
	WMT_INPUT_MODE_MT_TOUCHPAD =	0x3,
};

enum {
	WMT_TIP_SWITCH,
#define	WMT_SLOT	WMT_TIP_SWITCH
	WMT_WIDTH,
#define	WMT_MAJOR	WMT_WIDTH
	WMT_HEIGHT,
#define WMT_MINOR	WMT_HEIGHT
	WMT_ORIENTATION,
	WMT_X,
	WMT_Y,
	WMT_CONTACTID,
	WMT_PRESSURE,
	WMT_IN_RANGE,
	WMT_CONFIDENCE,
	WMT_TOOL_X,
	WMT_TOOL_Y,
	WMT_N_USAGES,
};

#define	WMT_NO_CODE	(ABS_MAX + 10)
#define	WMT_NO_USAGE	-1

struct wmt_hid_map_item {
	char		name[5];
	int32_t 	usage;		/* HID usage */
	uint32_t	code;		/* Evdev event code */
	bool		required;	/* Required for MT Digitizers */
};

static const struct wmt_hid_map_item wmt_hid_map[WMT_N_USAGES] = {
	[WMT_TIP_SWITCH] = {	/* WMT_SLOT */
		.name = "TIP",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH),
		.code = ABS_MT_SLOT,
		.required = true,
	},
	[WMT_WIDTH] = {		/* WMT_MAJOR */
		.name = "WDTH",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_WIDTH),
		.code = ABS_MT_TOUCH_MAJOR,
		.required = false,
	},
	[WMT_HEIGHT] = {	/* WMT_MINOR */
		.name = "HGHT",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_HEIGHT),
		.code = ABS_MT_TOUCH_MINOR,
		.required = false,
	},
	[WMT_ORIENTATION] = {
		.name = "ORIE",
		.usage = WMT_NO_USAGE,
		.code = ABS_MT_ORIENTATION,
		.required = false,
	},
	[WMT_X] = {
		.name = "X",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		.code = ABS_MT_POSITION_X,
		.required = true,
	},
	[WMT_Y] = {
		.name = "Y",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		.code = ABS_MT_POSITION_Y,
		.required = true,
	},
	[WMT_CONTACTID] = {
		.name = "C_ID",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACTID),
		.code = ABS_MT_TRACKING_ID,
		.required = true,
	},
	[WMT_PRESSURE] = {
		.name = "PRES",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_PRESSURE),
		.code = ABS_MT_PRESSURE,
		.required = false,
	},
	[WMT_IN_RANGE] = {
		.name = "RANG",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_IN_RANGE),
		.code = ABS_MT_DISTANCE,
		.required = false,
	},
	[WMT_CONFIDENCE] = {
		.name = "CONF",
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIDENCE),
		.code = WMT_NO_CODE,
		.required = false,
	},
	[WMT_TOOL_X] = {	/* Shares HID usage with WMT_X */
		.name = "TL_X",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		.code = ABS_MT_TOOL_X,
		.required = false,
	},
	[WMT_TOOL_Y] = {	/* Shares HID usage with WMT_Y */
		.name = "TL_Y",
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		.code = ABS_MT_TOOL_Y,
		.required = false,
	},
};

struct wmt_absinfo {
	int32_t			min;
	int32_t			max;
	int32_t			res;
};

struct wmt_softc {
	device_t		dev;
	enum wmt_type		type;

	struct mtx		mtx;
	struct wmt_absinfo	ai[WMT_N_USAGES];
	struct hid_location	locs[MAX_MT_SLOTS][WMT_N_USAGES];
	struct hid_location	cont_count_loc;
	struct hid_location	btn_loc[WMT_BTN_MAX];
	struct hid_location	int_btn_loc;
	struct hid_location	scan_time_loc;
	int32_t			scan_time_max;
	int32_t			scan_time;
	int32_t			timestamp;
	bool			touch;
	bool			prev_touch;

	struct usb_xfer		*xfer[WMT_N_TRANSFER];
	struct evdev_dev	*evdev;

	uint32_t		slot_data[WMT_N_USAGES];
	uint8_t			caps[howmany(WMT_N_USAGES, 8)];
	uint8_t			buttons[howmany(WMT_BTN_MAX, 8)];
	uint32_t		isize;
	uint32_t		nconts_per_report;
	uint32_t		nconts_todo;
	uint32_t		report_len;
	uint8_t			report_id;
	uint32_t		max_button;
	bool			has_int_button;
	bool			is_clickpad;
	bool			do_timestamps;

	struct hid_location	cont_max_loc;
	uint32_t		cont_max_rlen;
	uint8_t			cont_max_rid;
	struct hid_location	btn_type_loc;
	uint32_t		btn_type_rlen;
	uint8_t			btn_type_rid;
	uint32_t		thqa_cert_rlen;
	uint8_t			thqa_cert_rid;
	struct hid_location	input_mode_loc;
	uint32_t		input_mode_rlen;
	uint8_t			input_mode_rid;

	uint8_t			buf[WMT_BSIZE] __aligned(4);
};

#define	WMT_FOREACH_USAGE(caps, usage)			\
	for ((usage) = 0; (usage) < WMT_N_USAGES; ++(usage))	\
		if (isset((caps), (usage)))

static enum wmt_type wmt_hid_parse(struct wmt_softc *, const void *, uint16_t);
static int wmt_set_input_mode(struct wmt_softc *, enum wmt_input_mode);

static usb_callback_t	wmt_intr_callback;

static device_probe_t	wmt_probe;
static device_attach_t	wmt_attach;
static device_detach_t	wmt_detach;

#if __FreeBSD_version >= 1200077
static evdev_open_t	wmt_ev_open;
static evdev_close_t	wmt_ev_close;
#else
static evdev_open_t	wmt_ev_open_11;
static evdev_close_t	wmt_ev_close_11;
#endif

static const struct evdev_methods wmt_evdev_methods = {
#if __FreeBSD_version >= 1200077
	.ev_open = &wmt_ev_open,
	.ev_close = &wmt_ev_close,
#else
	.ev_open = &wmt_ev_open_11,
	.ev_close = &wmt_ev_close_11,
#endif
};

static const struct usb_config wmt_config[WMT_N_TRANSFER] = {
	[WMT_INTR_DT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1 },
		.bufsize = WMT_BSIZE,
		.callback = &wmt_intr_callback,
	},
};

static int
wmt_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct wmt_softc *sc = device_get_softc(dev);
	void *d_ptr;
	uint16_t d_len;
	int err;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bInterfaceClass != UICLASS_HID)
		return (ENXIO);

	if (usb_test_quirk(uaa, UQ_WMT_IGNORE))
		return (ENXIO);

	err = usbd_req_get_hid_desc(uaa->device, NULL,
	    &d_ptr, &d_len, M_TEMP, uaa->info.bIfaceIndex);
	if (err)
		return (ENXIO);

	/* Check if report descriptor belongs to a HID multitouch device */
	if (sc->type == WMT_TYPE_UNKNOWN)
		sc->type = wmt_hid_parse(sc, d_ptr, d_len);
	if (sc->type != WMT_TYPE_UNSUPPORTED)
		err = BUS_PROBE_DEFAULT;
	else
		err = ENXIO;

	/* Check HID report length */
	if (sc->type != WMT_TYPE_UNSUPPORTED &&
	    (sc->isize <= 0 || sc->isize > WMT_BSIZE)) {
		DPRINTF("Input size invalid or too large: %d\n", sc->isize);
		err = ENXIO;
	}

	free(d_ptr, M_TEMP);
	return (err);
}

static int
wmt_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct wmt_softc *sc = device_get_softc(dev);
	uint32_t cont_count_max;
	int nbuttons, btn;
	size_t i;
	int err;

	device_set_usb_desc(dev);
	sc->dev = dev;

	/* Fetch and parse "Contact count maximum" feature report */
	if (sc->cont_max_rlen > 0 && sc->cont_max_rlen <= WMT_BSIZE) {
		err = usbd_req_get_report(uaa->device, NULL, sc->buf,
		    sc->cont_max_rlen, uaa->info.bIfaceIndex,
		    UHID_FEATURE_REPORT, sc->cont_max_rid);
		if (err == USB_ERR_NORMAL_COMPLETION) {
			cont_count_max = hid_get_udata(sc->buf + 1,
			    sc->cont_max_rlen - 1, &sc->cont_max_loc);
			/*
			 * Feature report is a primary source of
			 * 'Contact Count Maximum'
			 */
			if (cont_count_max > 0)
				sc->ai[WMT_SLOT].max = cont_count_max - 1;
		} else
			DPRINTF("usbd_req_get_report error=(%s)\n",
			    usbd_errstr(err));
	} else
		DPRINTF("Feature report %hhu size invalid or too large: %u\n",
		    sc->cont_max_rid, sc->cont_max_rlen);

	/* Fetch and parse "Button type" feature report */
	if (sc->btn_type_rlen > 1 && sc->btn_type_rlen <= WMT_BSIZE &&
	    sc->btn_type_rid != sc->cont_max_rid) {
		bzero(sc->buf, sc->btn_type_rlen);
		err = usbd_req_get_report(uaa->device, NULL, sc->buf,
		    sc->btn_type_rlen, uaa->info.bIfaceIndex,
		    UHID_FEATURE_REPORT, sc->btn_type_rid);
	}
	if (sc->btn_type_rlen > 1) {
		if (err == 0)
			sc->is_clickpad = hid_get_udata(sc->buf + 1,
			    sc->btn_type_rlen - 1, &sc->btn_type_loc) == 0;
		else
			DPRINTF("usbd_req_get_report error=%d\n", err);
	}

	/* Fetch THQA certificate to enable some devices like WaveShare */
	if (sc->thqa_cert_rlen > 0 && sc->thqa_cert_rlen <= WMT_BSIZE &&
	    sc->thqa_cert_rid != sc->cont_max_rid)
		(void)usbd_req_get_report(uaa->device, NULL, sc->buf,
		    sc->thqa_cert_rlen, uaa->info.bIfaceIndex,
		    UHID_FEATURE_REPORT, sc->thqa_cert_rid);

	/* Switch touchpad in to absolute multitouch mode */
	if (sc->type == WMT_TYPE_TOUCHPAD) {
		err = wmt_set_input_mode(sc, WMT_INPUT_MODE_MT_TOUCHPAD);
		if (err != 0)
			DPRINTF("Failed to set input mode: %d\n", err);
	}

	/* Cap contact count maximum to MAX_MT_SLOTS */
	if (sc->ai[WMT_SLOT].max >= MAX_MT_SLOTS) {
		DPRINTF("Hardware reported %d contacts while only %d is "
		    "supported\n", (int)sc->ai[WMT_SLOT].max+1, MAX_MT_SLOTS);
		sc->ai[WMT_SLOT].max = MAX_MT_SLOTS - 1;
	}

	if (/*usb_test_quirk(hw, UQ_MT_TIMESTAMP) ||*/ wmt_timestamps)
		sc->do_timestamps = true;

	mtx_init(&sc->mtx, "wmt lock", NULL, MTX_DEF);

	err = usbd_transfer_setup(uaa->device, &uaa->info.bIfaceIndex,
	    sc->xfer, wmt_config, WMT_N_TRANSFER, sc, &sc->mtx);
	if (err != USB_ERR_NORMAL_COMPLETION) {
		DPRINTF("usbd_transfer_setup error=%s\n", usbd_errstr(err));
		goto detach;
	}

	sc->evdev = evdev_alloc();
	evdev_set_name(sc->evdev, device_get_desc(dev));
	evdev_set_phys(sc->evdev, device_get_nameunit(dev));
	evdev_set_id(sc->evdev, BUS_USB, uaa->info.idVendor,
	    uaa->info.idProduct, 0);
	evdev_set_serial(sc->evdev, usb_get_serial(uaa->device));
	evdev_set_methods(sc->evdev, sc, &wmt_evdev_methods);
	evdev_set_flag(sc->evdev, EVDEV_FLAG_MT_STCOMPAT);
	switch (sc->type) {
	case WMT_TYPE_TOUCHSCREEN:
		evdev_support_prop(sc->evdev, INPUT_PROP_DIRECT);
		break;
	case WMT_TYPE_TOUCHPAD:
		evdev_support_prop(sc->evdev, INPUT_PROP_POINTER);
		if (sc->is_clickpad)
			evdev_support_prop(sc->evdev, INPUT_PROP_BUTTONPAD);
		break;
	default:
		KASSERT(0, ("wmt_attach: unsupported touch device type"));
	}
	evdev_support_event(sc->evdev, EV_SYN);
	evdev_support_event(sc->evdev, EV_ABS);
	if (sc->do_timestamps) {
		evdev_support_event(sc->evdev, EV_MSC);
		evdev_support_msc(sc->evdev, MSC_TIMESTAMP);
	}
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
	WMT_FOREACH_USAGE(sc->caps, i) {
		if (wmt_hid_map[i].code != WMT_NO_CODE)
			evdev_support_abs(sc->evdev, wmt_hid_map[i].code,
			    sc->ai[i].min, sc->ai[i].max, 0, 0, sc->ai[i].res);
	}

	err = evdev_register_mtx(sc->evdev, &sc->mtx);
	if (err)
		goto detach;

	/* Announce information about the touch device */
	device_printf(sc->dev, "Multitouch %s with %d external button%s%s\n",
	    sc->type == WMT_TYPE_TOUCHSCREEN ? "touchscreen" : "touchpad",
	    nbuttons, nbuttons != 1 ? "s" : "",
	    sc->is_clickpad ? ", click-pad" : "");
	device_printf(sc->dev,
	    "%d contacts and [%s%s%s%s%s]. Report range [%d:%d] - [%d:%d]\n",
	    (int)sc->ai[WMT_SLOT].max + 1,
	    isset(sc->caps, WMT_IN_RANGE) ? "R" : "",
	    isset(sc->caps, WMT_CONFIDENCE) ? "C" : "",
	    isset(sc->caps, WMT_WIDTH) ? "W" : "",
	    isset(sc->caps, WMT_HEIGHT) ? "H" : "",
	    isset(sc->caps, WMT_PRESSURE) ? "P" : "",
	    (int)sc->ai[WMT_X].min, (int)sc->ai[WMT_Y].min,
	    (int)sc->ai[WMT_X].max, (int)sc->ai[WMT_Y].max);

	return (0);

detach:
	wmt_detach(dev);
	return (ENXIO);
}

static int
wmt_detach(device_t dev)
{
	struct wmt_softc *sc = device_get_softc(dev);

	evdev_free(sc->evdev);
	usbd_transfer_unsetup(sc->xfer, WMT_N_TRANSFER);
	mtx_destroy(&sc->mtx);
	return (0);
}

static void
wmt_process_report(struct wmt_softc *sc, uint8_t *buf, int len)
{
	size_t usage;
	uint32_t *slot_data = sc->slot_data;
	uint32_t cont, btn;
	uint32_t cont_count;
	uint32_t width;
	uint32_t height;
	uint32_t int_btn = 0;
	uint32_t left_btn = 0;
	int32_t slot;
	uint32_t scan_time;
	int32_t delta;

	/*
	 * "In Parallel mode, devices report all contact information in a
	 * single packet. Each physical contact is represented by a logical
	 * collection that is embedded in the top-level collection."
	 *
	 * Since additional contacts that were not present will still be in the
	 * report with contactid=0 but contactids are zero-based, find
	 * contactcount first.
	 */
	cont_count = hid_get_udata(buf, len, &sc->cont_count_loc);
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

#ifdef USB_DEBUG
	DPRINTFN(6, "cont_count:%2u", (unsigned)cont_count);
	if (wmt_debug >= 6) {
		WMT_FOREACH_USAGE(sc->caps, usage) {
			if (wmt_hid_map[usage].usage != WMT_NO_USAGE)
				printf(" %-4s", wmt_hid_map[usage].name);
		}
		printf("\n");
	}
#endif

	/* Find the number of contacts reported in current report */
	cont_count = MIN(sc->nconts_todo, sc->nconts_per_report);

	/* Use protocol Type B for reporting events */
	for (cont = 0; cont < cont_count; cont++) {
		bzero(slot_data, sizeof(sc->slot_data));
		WMT_FOREACH_USAGE(sc->caps, usage) {
			if (sc->locs[cont][usage].size > 0)
				slot_data[usage] = hid_get_udata(
				    buf, len, &sc->locs[cont][usage]);
		}

		slot = evdev_get_mt_slot_by_tracking_id(sc->evdev,
		    slot_data[WMT_CONTACTID]);

#ifdef USB_DEBUG
		DPRINTFN(6, "cont%01x: data = ", cont);
		if (wmt_debug >= 6) {
			WMT_FOREACH_USAGE(sc->caps, usage) {
				if (wmt_hid_map[usage].usage != WMT_NO_USAGE)
					printf("%04x ", slot_data[usage]);
			}
			printf("slot = %d\n", (int)slot);
		}
#endif

		if (slot == -1) {
			DPRINTF("Slot overflow for contact_id %u\n",
			    (unsigned)slot_data[WMT_CONTACTID]);
			continue;
		}

		if (slot_data[WMT_TIP_SWITCH] != 0 &&
		    !(isset(sc->caps, WMT_CONFIDENCE) &&
		      slot_data[WMT_CONFIDENCE] == 0)) {
			/* This finger is in proximity of the sensor */
			sc->touch = true;
			slot_data[WMT_SLOT] = slot;
			slot_data[WMT_IN_RANGE] = !slot_data[WMT_IN_RANGE];
			/* Divided by two to match visual scale of touch */
			width = slot_data[WMT_WIDTH] >> 1;
			height = slot_data[WMT_HEIGHT] >> 1;
			slot_data[WMT_ORIENTATION] = width > height;
			slot_data[WMT_MAJOR] = MAX(width, height);
			slot_data[WMT_MINOR] = MIN(width, height);

			WMT_FOREACH_USAGE(sc->caps, usage)
				if (wmt_hid_map[usage].code != WMT_NO_CODE)
					evdev_push_abs(sc->evdev,
					    wmt_hid_map[usage].code,
					    slot_data[usage]);
		} else {
			evdev_push_abs(sc->evdev, ABS_MT_SLOT, slot);
			evdev_push_abs(sc->evdev, ABS_MT_TRACKING_ID, -1);
		}
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

static void
wmt_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct wmt_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint8_t *buf = sc->buf;
	int len;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);

		DPRINTFN(6, "sc=%p actlen=%d\n", sc, len);

		if (len >= (int)sc->report_len ||
		    (len > 0 && sc->report_id != 0)) {
			/* Limit report length to the maximum */
			if (len > (int)sc->report_len)
				len = sc->report_len;

			usbd_copy_out(pc, 0, buf, len);

			/* Ignore irrelevant reports */
			if (sc->report_id && *buf != sc->report_id)
				goto tr_ignore;

			/* Make sure we don't process old data */
			if (len < sc->report_len)
				bzero(buf + len, sc->report_len - len);

			/* Strip leading "report ID" byte */
			if (sc->report_id) {
				len--;
				buf++;
			}

			wmt_process_report(sc, buf, len);
		} else {
tr_ignore:
			DPRINTF("Ignored transfer, %d bytes\n", len);
		}

	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, sc->isize);
		usbd_transfer_submit(xfer);
		break;
	default:
		if (error != USB_ERR_CANCELLED) {
			/* Try clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
wmt_ev_close_11(struct evdev_dev *evdev, void *ev_softc)
{
	struct wmt_softc *sc = ev_softc;

	mtx_assert(&sc->mtx, MA_OWNED);
	usbd_transfer_stop(sc->xfer[WMT_INTR_DT]);
}

static int
wmt_ev_open_11(struct evdev_dev *evdev, void *ev_softc)
{
	struct wmt_softc *sc = ev_softc;

	mtx_assert(&sc->mtx, MA_OWNED);
	usbd_transfer_start(sc->xfer[WMT_INTR_DT]);

	return (0);
}

#if __FreeBSD_version >= 1200077
static int
wmt_ev_close(struct evdev_dev *evdev)
{
	struct wmt_softc *sc = evdev_get_softc(evdev);

	wmt_ev_close_11(evdev, sc);

	return (0);
}

static int
wmt_ev_open(struct evdev_dev *evdev)
{
	struct wmt_softc *sc = evdev_get_softc(evdev);

	return (wmt_ev_open_11(evdev, sc));

}
#endif

static enum wmt_type
wmt_hid_parse(struct wmt_softc *sc, const void *d_ptr, uint16_t d_len)
{
	struct hid_item hi;
	struct hid_data *hd;
	size_t i;
	size_t cont = 0;
	enum wmt_type type = WMT_TYPE_UNSUPPORTED;
	uint32_t left_btn, btn;
	int32_t cont_count_max = 0;
	uint8_t report_id = 0;
	bool touch_coll = false;
	bool finger_coll = false;
	bool cont_count_found = false;
	bool scan_time_found = false;
	bool has_int_button = false;

#define WMT_HI_ABSOLUTE(hi)	\
	(((hi).flags & (HIO_CONST|HIO_VARIABLE|HIO_RELATIVE)) == HIO_VARIABLE)
#define	HUMS_THQA_CERT	0xC5

	/* Parse features for maximum contact count */
	hd = hid_start_parse(d_ptr, d_len, 1 << hid_feature);
	while (hid_get_item(hd, &hi)) {
		switch (hi.kind) {
		case hid_collection:
			if (hi.collevel == 1 && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN)) {
				touch_coll = true;
				type = WMT_TYPE_TOUCHSCREEN;
				left_btn = 1;
				break;
			}
			if (hi.collevel == 1 && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHPAD)) {
				touch_coll = true;
				type = WMT_TYPE_TOUCHPAD;
				left_btn = 2;
			}
			break;
		case hid_endcollection:
			if (hi.collevel == 0 && touch_coll)
				touch_coll = false;
			break;
		case hid_feature:
			if (hi.collevel == 1 && touch_coll && hi.usage ==
			      HID_USAGE2(HUP_MICROSOFT, HUMS_THQA_CERT)) {
				sc->thqa_cert_rid = hi.report_ID;
				break;
			}
			if (hi.collevel == 1 && touch_coll && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACT_MAX)) {
				cont_count_max = hi.logical_maximum;
				sc->cont_max_rid = hi.report_ID;
				sc->cont_max_loc = hi.loc;
				break;
			}
			if (hi.collevel == 1 && touch_coll && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_BUTTON_TYPE)) {
				sc->btn_type_rid = hi.report_ID;
				sc->btn_type_loc = hi.loc;
			}
			break;
		default:
			break;
		}
	}
	hid_end_parse(hd);

	if (type == WMT_TYPE_UNSUPPORTED)
		return (WMT_TYPE_UNSUPPORTED);
	/* Maximum contact count is required usage */
	if (sc->cont_max_rid == 0)
		return (WMT_TYPE_UNSUPPORTED);

	touch_coll = false;

	/* Parse input for other parameters */
	hd = hid_start_parse(d_ptr, d_len, 1 << hid_input);
	while (hid_get_item(hd, &hi)) {
		switch (hi.kind) {
		case hid_collection:
			if (hi.collevel == 1 && hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN))
				touch_coll = true;
			else if (touch_coll && hi.collevel == 2 &&
			    (report_id == 0 || report_id == hi.report_ID) &&
			    hi.usage == HID_USAGE2(HUP_DIGITIZERS, HUD_FINGER))
				finger_coll = true;
			break;
		case hid_endcollection:
			if (hi.collevel == 1 && finger_coll) {
				finger_coll = false;
				cont++;
			} else if (hi.collevel == 0 && touch_coll)
				touch_coll = false;
			break;
		case hid_input:
			/*
			 * Ensure that all usages are located within the same
			 * report and proper collection.
			 */
			if (WMT_HI_ABSOLUTE(hi) && touch_coll &&
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
			    hi.usage <= HID_USAGE2(HUP_BUTTON, WMT_BTN_MAX)) {
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
			/* Scan time is required but clobbered by evdev */
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

			for (i = 0; i < WMT_N_USAGES; i++) {
				if (hi.usage == wmt_hid_map[i].usage) {
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
					sc->ai[i] = (struct wmt_absinfo) {
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
	if (!cont_count_found || !scan_time_found || cont == 0)
		return (WMT_TYPE_UNSUPPORTED);
	for (i = 0; i < WMT_N_USAGES; i++) {
		if (wmt_hid_map[i].required && isclr(sc->caps, i))
			return (WMT_TYPE_UNSUPPORTED);
	}

	/* Touchpads must have at least one button */
	if (type == WMT_TYPE_TOUCHPAD && !sc->max_button && !has_int_button)
		return (WMT_TYPE_UNSUPPORTED);

	/*
	 * According to specifications 'Contact Count Maximum' should be read
	 * from Feature Report rather than from HID descriptor. Set sane
	 * default value now to handle the case of 'Get Report' request failure
	 */
	if (cont_count_max < 1)
		cont_count_max = cont;

	/* Set number of MT protocol type B slots */
	sc->ai[WMT_SLOT] = (struct wmt_absinfo) {
		.min = 0,
		.max = cont_count_max - 1,
		.res = 0,
	};

	/* Report touch orientation if both width and height are supported */
	if (isset(sc->caps, WMT_WIDTH) && isset(sc->caps, WMT_HEIGHT)) {
		setbit(sc->caps, WMT_ORIENTATION);
		sc->ai[WMT_ORIENTATION].max = 1;
	}

	sc->isize = hid_report_size_max(d_ptr, d_len, hid_input, NULL);
	sc->report_len = hid_report_size(d_ptr, d_len, hid_input,
	    report_id);
	sc->cont_max_rlen = hid_report_size(d_ptr, d_len, hid_feature,
	    sc->cont_max_rid);
	if (sc->btn_type_rid > 0)
		sc->btn_type_rlen = hid_report_size(d_ptr, d_len,
		    hid_feature, sc->btn_type_rid);
	if (sc->thqa_cert_rid > 0)
		sc->thqa_cert_rlen = hid_report_size(d_ptr, d_len,
		    hid_feature, sc->thqa_cert_rid);

	sc->report_id = report_id;
	sc->nconts_per_report = cont;
	sc->has_int_button = has_int_button;

	return (type);
}

static int
wmt_set_input_mode(struct wmt_softc *sc, enum wmt_input_mode mode)
{
	struct usb_attach_arg *uaa = device_get_ivars(sc->dev);
	int err;

	if (sc->input_mode_rlen < 3 || sc->input_mode_rlen > WMT_BSIZE) {
		DPRINTF("Feature report %hhu size invalid or too large: %u\n",
		    sc->input_mode_rid, sc->input_mode_rlen);
		return (USB_ERR_BAD_BUFSIZE);
	}

	/* Input Mode report is not strictly required to be readable */
	err = usbd_req_get_report(uaa->device, NULL, sc->buf,
	    sc->input_mode_rlen, uaa->info.bIfaceIndex,
	    UHID_FEATURE_REPORT, sc->input_mode_rid);
	if (err != USB_ERR_NORMAL_COMPLETION)
		bzero(sc->buf + 1, sc->input_mode_rlen - 1);

	sc->buf[0] = sc->input_mode_rid;
	hid_put_udata(sc->buf + 1, sc->input_mode_rlen - 1,
	    &sc->input_mode_loc, mode);
	err = usbd_req_set_report(uaa->device, NULL, sc->buf,
	    sc->input_mode_rlen, uaa->info.bIfaceIndex,
	    UHID_FEATURE_REPORT, sc->input_mode_rid);

	return (err);
}

#ifndef USBHID_ENABLED
static const STRUCT_USB_HOST_ID wmt_devs[] = {
	/* generic HID class w/o boot interface */
	{USB_IFACE_CLASS(UICLASS_HID),
	 USB_IFACE_SUBCLASS(0),},
};
#endif

static devclass_t wmt_devclass;

static device_method_t wmt_methods[] = {
	DEVMETHOD(device_probe, wmt_probe),
	DEVMETHOD(device_attach, wmt_attach),
	DEVMETHOD(device_detach, wmt_detach),

	DEVMETHOD_END
};

static driver_t wmt_driver = {
	.name = "wmt",
	.methods = wmt_methods,
	.size = sizeof(struct wmt_softc),
};

DRIVER_MODULE(wmt, uhub, wmt_driver, wmt_devclass, NULL, 0);
MODULE_DEPEND(wmt, usb, 1, 1, 1);
MODULE_DEPEND(wmt, hid, 1, 1, 1);
MODULE_DEPEND(wmt, evdev, 1, 1, 1);
MODULE_VERSION(wmt, 1);
#ifndef USBHID_ENABLED
USB_PNP_HOST_INFO(wmt_devs);
#endif
