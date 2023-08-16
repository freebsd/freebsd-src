/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Huang Wen Hui
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define HID_DEBUG_VAR   bcm5974_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usb_ioctl.h>

#include "usbdevs.h"

#define	BCM5974_BUFFER_MAX	(248 * 4)	/* 4 Type4 SPI frames */
#define	BCM5974_TLC_PAGE	HUP_GENERIC_DESKTOP
#define	BCM5974_TLC_USAGE	HUG_MOUSE

/* magic to switch device from HID (default) mode into raw */
/* Type1 & Type2 trackpads */
#define	BCM5974_USB_IFACE_INDEX	0
#define	BCM5974_USB_REPORT_LEN	8
#define	BCM5974_USB_REPORT_ID	0
#define	BCM5974_USB_MODE_RAW	0x01
#define	BCM5974_USB_MODE_HID	0x08
/* Type4 trackpads */
#define	BCM5974_HID_REPORT_LEN	2
#define	BCM5974_HID_REPORT_ID	2
#define	BCM5974_HID_MODE_RAW	0x01
#define	BCM5974_HID_MODE_HID	0x00

/* Tunables */
static	SYSCTL_NODE(_hw_hid, OID_AUTO, bcm5974, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "HID wellspring touchpad");

#ifdef HID_DEBUG
enum wsp_log_level {
	BCM5974_LLEVEL_DISABLED = 0,
	BCM5974_LLEVEL_ERROR,
	BCM5974_LLEVEL_DEBUG,		/* for troubleshooting */
	BCM5974_LLEVEL_INFO,		/* for diagnostics */
};
/* the default is to only log errors */
static int bcm5974_debug = BCM5974_LLEVEL_ERROR;

SYSCTL_INT(_hw_hid_bcm5974, OID_AUTO, debug, CTLFLAG_RWTUN,
    &bcm5974_debug, BCM5974_LLEVEL_ERROR, "BCM5974 debug level");
#endif					/* HID_DEBUG */

/*
 * Some tables, structures, definitions and constant values for the
 * touchpad protocol has been copied from Linux's
 * "drivers/input/mouse/bcm5974.c" which has the following copyright
 * holders under GPLv2. All device specific code in this driver has
 * been written from scratch. The decoding algorithm is based on
 * output from FreeBSD's usbdump.
 *
 * Copyright (C) 2008      Henrik Rydberg (rydberg@euromail.se)
 * Copyright (C) 2008      Scott Shawcroft (scott.shawcroft@gmail.com)
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2005      Johannes Berg (johannes@sipsolutions.net)
 * Copyright (C) 2005      Stelian Pop (stelian@popies.net)
 * Copyright (C) 2005      Frank Arnold (frank@scirocco-5v-turbo.de)
 * Copyright (C) 2005      Peter Osterlund (petero2@telia.com)
 * Copyright (C) 2005      Michael Hanselmann (linux-kernel@hansmi.ch)
 * Copyright (C) 2006      Nicolas Boichat (nicolas@boichat.ch)
 */

/* trackpad header types */
enum tp_type {
	TYPE1,			/* plain trackpad */
	TYPE2,			/* button integrated in trackpad */
	TYPE3,			/* additional header fields since June 2013 */
	TYPE4,                  /* additional header field for pressure data */
	TYPE_MT2U,			/* Magic Trackpad 2 USB */
	TYPE_CNT
};

/* list of device capability bits */
#define	HAS_INTEGRATED_BUTTON	1
#define	USES_COMPACT_REPORT	2

struct tp_type_params {
	uint8_t	caps;		/* device capability bitmask */
	uint8_t	button;		/* offset to button data */
	uint8_t	offset;		/* offset to trackpad finger data */
	uint8_t delta;		/* offset from header to finger struct */
} const static tp[TYPE_CNT] = {
	[TYPE1] = {
		.caps = 0,
		.button = 0,
		.offset = 13 * 2,
		.delta = 0,
	},
	[TYPE2] = {
		.caps = HAS_INTEGRATED_BUTTON,
		.button = 15,
		.offset = 15 * 2,
		.delta = 0,
	},
	[TYPE3] = {
		.caps = HAS_INTEGRATED_BUTTON,
		.button = 23,
		.offset = 19 * 2,
		.delta = 0,
	},
	[TYPE4] = {
		.caps = HAS_INTEGRATED_BUTTON,
		.button = 31,
		.offset = 23 * 2,
		.delta = 2,
	},
	[TYPE_MT2U] = {
		.caps = HAS_INTEGRATED_BUTTON | USES_COMPACT_REPORT,
		.button = 1,
		.offset = 12,
		.delta = 0,
	},
};

/* trackpad finger structure - compact version for external "Magic" devices */
struct tp_finger_compact {
	uint32_t coords; /* not struct directly due to endian conversion */
	uint8_t touch_major;
	uint8_t touch_minor;
	uint8_t size;
	uint8_t pressure;
	uint8_t id_ori;
} __packed;

_Static_assert((sizeof(struct tp_finger_compact) == 9), "tp_finger struct size must be 9");

/* trackpad finger structure - little endian */
struct tp_finger {
	uint16_t	origin;		/* zero when switching track finger */
	uint16_t	abs_x;		/* absolute x coodinate */
	uint16_t	abs_y;		/* absolute y coodinate */
	uint16_t	rel_x;		/* relative x coodinate */
	uint16_t	rel_y;		/* relative y coodinate */
	uint16_t	tool_major;	/* tool area, major axis */
	uint16_t	tool_minor;	/* tool area, minor axis */
	uint16_t	orientation;	/* 16384 when point, else 15 bit angle */
	uint16_t	touch_major;	/* touch area, major axis */
	uint16_t	touch_minor;	/* touch area, minor axis */
	uint16_t	unused[2];	/* zeros */
	uint16_t	pressure;	/* pressure on forcetouch touchpad */
	uint16_t	multi;		/* one finger: varies, more fingers:
					 * constant */
} __packed;

#define BCM5974_LE2H(x) ((int32_t)(int16_t)le16toh(x))

/* trackpad finger data size, empirically at least ten fingers */
#define	MAX_FINGERS		MAX_MT_SLOTS

#define	MAX_FINGER_ORIENTATION	16384

enum {
	BCM5974_FLAG_WELLSPRING1,
	BCM5974_FLAG_WELLSPRING2,
	BCM5974_FLAG_WELLSPRING3,
	BCM5974_FLAG_WELLSPRING4,
	BCM5974_FLAG_WELLSPRING4A,
	BCM5974_FLAG_WELLSPRING5,
	BCM5974_FLAG_WELLSPRING6A,
	BCM5974_FLAG_WELLSPRING6,
	BCM5974_FLAG_WELLSPRING5A,
	BCM5974_FLAG_WELLSPRING7,
	BCM5974_FLAG_WELLSPRING7A,
	BCM5974_FLAG_WELLSPRING8,
	BCM5974_FLAG_WELLSPRING9,
	BCM5974_FLAG_MAGIC_TRACKPAD2_USB,
	BCM5974_FLAG_MAX,
};

/* device-specific parameters */
struct bcm5974_axis {
	int snratio;			/* signal-to-noise ratio */
	int min;			/* device minimum reading */
	int max;			/* device maximum reading */
	int size;			/* physical size, mm */
};

/* device-specific configuration */
struct bcm5974_dev_params {
	const struct tp_type_params* tp;
	struct bcm5974_axis p;		/* finger pressure limits */
	struct bcm5974_axis w;		/* finger width limits */
	struct bcm5974_axis x;		/* horizontal limits */
	struct bcm5974_axis y;		/* vertical limits */
	struct bcm5974_axis o;		/* orientation limits */
};

/* logical signal quality */
#define	SN_PRESSURE	45		/* pressure signal-to-noise ratio */
#define	SN_WIDTH	25		/* width signal-to-noise ratio */
#define	SN_COORD	250		/* coordinate signal-to-noise ratio */
#define	SN_ORIENT	10		/* orientation signal-to-noise ratio */

static const struct bcm5974_dev_params bcm5974_dev_params[BCM5974_FLAG_MAX] = {
	[BCM5974_FLAG_WELLSPRING1] = {
		.tp = tp + TYPE1,
		.p = { SN_PRESSURE, 0, 256, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4824, 5342, 105 },
		.y = { SN_COORD, -172, 5820, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING2] = {
		.tp = tp + TYPE1,
		.p = { SN_PRESSURE, 0, 256, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4824, 4824, 105 },
		.y = { SN_COORD, -172, 4290, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING3] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4460, 5166, 105 },
		.y = { SN_COORD, -75, 6700, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING4] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING4A] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4616, 5112, 105 },
		.y = { SN_COORD, -142, 5234, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING5] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4415, 5050, 105 },
		.y = { SN_COORD, -55, 6680, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING6] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING5A] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4750, 5280, 105 },
		.y = { SN_COORD, -150, 6730, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING6A] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING7] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4750, 5280, 105 },
		.y = { SN_COORD, -150, 6730, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING7A] = {
		.tp = tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4750, 5280, 105 },
		.y = { SN_COORD, -150, 6730, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_WELLSPRING8] = {
		.tp = tp + TYPE3,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	/*
	 * NOTE: Actually force-sensitive. Pressure has a "size" equal to the max
	 * so that the "resolution" is 1 (i.e. values will be interpreted as grams).
	 * No scientific measurements have been done :) but a really hard press
	 * results in a value around 3500 on model 4.
	 */
	[BCM5974_FLAG_WELLSPRING9] = {
		.tp = tp + TYPE4,
		.p = { SN_PRESSURE, 0, 4096, 4096 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4828, 5345, 105 },
		.y = { SN_COORD, -203, 6803, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[BCM5974_FLAG_MAGIC_TRACKPAD2_USB] = {
		.tp = tp + TYPE_MT2U,
		.p = { SN_PRESSURE, 0, 256, 256 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -3678, 3934, 48 },
		.y = { SN_COORD, -2478, 2587, 44 },
		.o = { SN_ORIENT, -3, 4, 0 },
	},
};

#define	BCM5974_DEV(v,p,i)	{					\
	HID_BVPI(BUS_USB, USB_VENDOR_##v, USB_PRODUCT_##v##_##p, i),	\
	HID_TLC(BCM5974_TLC_PAGE, BCM5974_TLC_USAGE),			\
}

static const struct hid_device_id bcm5974_devs[] = {
	/* MacbookAir1.1 */
	BCM5974_DEV(APPLE, WELLSPRING_ANSI, BCM5974_FLAG_WELLSPRING1),
	BCM5974_DEV(APPLE, WELLSPRING_ISO, BCM5974_FLAG_WELLSPRING1),
	BCM5974_DEV(APPLE, WELLSPRING_JIS, BCM5974_FLAG_WELLSPRING1),

	/* MacbookProPenryn, aka wellspring2 */
	BCM5974_DEV(APPLE, WELLSPRING2_ANSI, BCM5974_FLAG_WELLSPRING2),
	BCM5974_DEV(APPLE, WELLSPRING2_ISO, BCM5974_FLAG_WELLSPRING2),
	BCM5974_DEV(APPLE, WELLSPRING2_JIS, BCM5974_FLAG_WELLSPRING2),

	/* Macbook5,1 (unibody), aka wellspring3 */
	BCM5974_DEV(APPLE, WELLSPRING3_ANSI, BCM5974_FLAG_WELLSPRING3),
        BCM5974_DEV(APPLE, WELLSPRING3_ISO, BCM5974_FLAG_WELLSPRING3),
	BCM5974_DEV(APPLE, WELLSPRING3_JIS, BCM5974_FLAG_WELLSPRING3),

	/* MacbookAir3,2 (unibody), aka wellspring4 */
	BCM5974_DEV(APPLE, WELLSPRING4_ANSI, BCM5974_FLAG_WELLSPRING4),
	BCM5974_DEV(APPLE, WELLSPRING4_ISO, BCM5974_FLAG_WELLSPRING4),
	BCM5974_DEV(APPLE, WELLSPRING4_JIS, BCM5974_FLAG_WELLSPRING4),

	/* MacbookAir3,1 (unibody), aka wellspring4 */
	BCM5974_DEV(APPLE, WELLSPRING4A_ANSI, BCM5974_FLAG_WELLSPRING4A),
	BCM5974_DEV(APPLE, WELLSPRING4A_ISO, BCM5974_FLAG_WELLSPRING4A),
	BCM5974_DEV(APPLE, WELLSPRING4A_JIS, BCM5974_FLAG_WELLSPRING4A),

	/* Macbook8 (unibody, March 2011) */
	BCM5974_DEV(APPLE, WELLSPRING5_ANSI, BCM5974_FLAG_WELLSPRING5),
	BCM5974_DEV(APPLE, WELLSPRING5_ISO, BCM5974_FLAG_WELLSPRING5),
	BCM5974_DEV(APPLE, WELLSPRING5_JIS, BCM5974_FLAG_WELLSPRING5),

	/* MacbookAir4,1 (unibody, July 2011) */
	BCM5974_DEV(APPLE, WELLSPRING6A_ANSI, BCM5974_FLAG_WELLSPRING6A),
	BCM5974_DEV(APPLE, WELLSPRING6A_ISO, BCM5974_FLAG_WELLSPRING6A),
	BCM5974_DEV(APPLE, WELLSPRING6A_JIS, BCM5974_FLAG_WELLSPRING6A),

	/* MacbookAir4,2 (unibody, July 2011) */
	BCM5974_DEV(APPLE, WELLSPRING6_ANSI, BCM5974_FLAG_WELLSPRING6),
	BCM5974_DEV(APPLE, WELLSPRING6_ISO, BCM5974_FLAG_WELLSPRING6),
	BCM5974_DEV(APPLE, WELLSPRING6_JIS, BCM5974_FLAG_WELLSPRING6),

	/* Macbook8,2 (unibody) */
	BCM5974_DEV(APPLE, WELLSPRING5A_ANSI, BCM5974_FLAG_WELLSPRING5A),
	BCM5974_DEV(APPLE, WELLSPRING5A_ISO, BCM5974_FLAG_WELLSPRING5A),
	BCM5974_DEV(APPLE, WELLSPRING5A_JIS, BCM5974_FLAG_WELLSPRING5A),

	/* MacbookPro10,1 (unibody, June 2012) */
	/* MacbookPro11,1-3 (unibody, June 2013) */
	BCM5974_DEV(APPLE, WELLSPRING7_ANSI, BCM5974_FLAG_WELLSPRING7),
	BCM5974_DEV(APPLE, WELLSPRING7_ISO, BCM5974_FLAG_WELLSPRING7),
        BCM5974_DEV(APPLE, WELLSPRING7_JIS, BCM5974_FLAG_WELLSPRING7),

        /* MacbookPro10,2 (unibody, October 2012) */
        BCM5974_DEV(APPLE, WELLSPRING7A_ANSI, BCM5974_FLAG_WELLSPRING7A),
        BCM5974_DEV(APPLE, WELLSPRING7A_ISO, BCM5974_FLAG_WELLSPRING7A),
        BCM5974_DEV(APPLE, WELLSPRING7A_JIS, BCM5974_FLAG_WELLSPRING7A),

	/* MacbookAir6,2 (unibody, June 2013) */
	BCM5974_DEV(APPLE, WELLSPRING8_ANSI, BCM5974_FLAG_WELLSPRING8),
	BCM5974_DEV(APPLE, WELLSPRING8_ISO, BCM5974_FLAG_WELLSPRING8),
	BCM5974_DEV(APPLE, WELLSPRING8_JIS, BCM5974_FLAG_WELLSPRING8),

	/* MacbookPro12,1 MacbookPro11,4 */
	BCM5974_DEV(APPLE, WELLSPRING9_ANSI, BCM5974_FLAG_WELLSPRING9),
	BCM5974_DEV(APPLE, WELLSPRING9_ISO, BCM5974_FLAG_WELLSPRING9),
	BCM5974_DEV(APPLE, WELLSPRING9_JIS, BCM5974_FLAG_WELLSPRING9),

	/* External "Magic" devices */
	BCM5974_DEV(APPLE, MAGIC_TRACKPAD2, BCM5974_FLAG_MAGIC_TRACKPAD2_USB),
};

struct bcm5974_softc {
	device_t sc_dev;
	struct evdev_dev *sc_evdev;
	/* device configuration */
	const struct bcm5974_dev_params *sc_params;
	bool sc_saved_mode;
};

static const uint8_t bcm5974_rdesc[] = {
	0x05, BCM5974_TLC_PAGE,	/* Usage Page (BCM5974_TLC_PAGE)	*/
	0x09, BCM5974_TLC_USAGE,/* Usage (BCM5974_TLC_USAGE)		*/
	0xA1, 0x01,		/* Collection (Application)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x01,		/*   Usage (0x01)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x96,			/*   Report Count (BCM5974_BUFFER_MAX)	*/
	BCM5974_BUFFER_MAX & 0xFF,
	BCM5974_BUFFER_MAX >> 8 & 0xFF,
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0xC0,			/* End Collection			*/
};

/*
 * function prototypes
 */
static evdev_open_t	bcm5974_ev_open;
static evdev_close_t	bcm5974_ev_close;
static const struct evdev_methods bcm5974_evdev_methods = {
	.ev_open =	&bcm5974_ev_open,
	.ev_close =	&bcm5974_ev_close,
};
static hid_intr_t	bcm5974_intr;

/* Device methods. */
static device_identify_t bcm5974_identify;
static device_probe_t	bcm5974_probe;
static device_attach_t	bcm5974_attach;
static device_detach_t	bcm5974_detach;

/*
 * Type1 and Type2 touchpads use keyboard USB interface to switch from HID to
 * RAW mode. Although it is possible to extend hkbd driver to support such a
 * mode change requests, it's not wanted due to cross device tree dependencies.
 * So, find lowest common denominator (struct usb_device of grandparent usbhid
 * driver) of touchpad and keyboard drivers and issue direct USB requests.
 */
static int
bcm5974_set_device_mode_usb(struct bcm5974_softc *sc, bool on)
{
	uint8_t mode_bytes[BCM5974_USB_REPORT_LEN];
	struct usb_ctl_request ucr;
	int err;

	ucr.ucr_request.bmRequestType = UT_READ_CLASS_INTERFACE;
	ucr.ucr_request.bRequest = UR_GET_REPORT;
	USETW2(ucr.ucr_request.wValue,
	    UHID_FEATURE_REPORT, BCM5974_USB_REPORT_ID);
	ucr.ucr_request.wIndex[0] = BCM5974_USB_IFACE_INDEX;
	ucr.ucr_request.wIndex[1] = 0;
	USETW(ucr.ucr_request.wLength, BCM5974_USB_REPORT_LEN);
	ucr.ucr_data = mode_bytes;

	err = hid_ioctl(sc->sc_dev, USB_REQUEST, (uintptr_t)&ucr);
	if (err != 0) {
		DPRINTF("Failed to read device mode (%d)\n", err);
		return (EIO);
	}
#if 0
	/*
	 * XXX Need to wait at least 250ms for hardware to get
	 * ready. The device mode handling appears to be handled
	 * asynchronously and we should not issue these commands too
	 * quickly.
	 */
	pause("WHW", hz / 4);
#endif
	mode_bytes[0] = on ? BCM5974_USB_MODE_RAW : BCM5974_USB_MODE_HID;
	ucr.ucr_request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	ucr.ucr_request.bRequest = UR_SET_REPORT;

	err = hid_ioctl(sc->sc_dev, USB_REQUEST, (uintptr_t)&ucr);
	if (err != 0) {
		DPRINTF("Failed to write device mode (%d)\n", err);
		return (EIO);
	}

	return (0);
}

static int
bcm5974_set_device_mode_hid(struct bcm5974_softc *sc, bool on)
{
	uint8_t	mode_bytes[BCM5974_HID_REPORT_LEN] = {
		BCM5974_HID_REPORT_ID,
		on ? BCM5974_HID_MODE_RAW : BCM5974_HID_MODE_HID,
	};
#if 0
	int err;

	err = hid_get_report(sc->sc_dev, mode_bytes, BCM5974_HID_REPORT_LEN,
	    NULL, HID_FEATURE_REPORT, BCM5974_HID_REPORT_ID);
	if (err != 0) {
		DPRINTF("Failed to read device mode (%d)\n", err);
		return (err);
	}
	/*
	 * XXX Need to wait at least 250ms for hardware to get
	 * ready. The device mode handling appears to be handled
	 * asynchronously and we should not issue these commands too
	 * quickly.
	 */
	pause("WHW", hz / 4);
	mode_bytes[1] = on ? BCM5974_HID_MODE_RAW : BCM5974_HID_MODE_HID;
#endif
	return (hid_set_report(sc->sc_dev, mode_bytes, BCM5974_HID_REPORT_LEN,
	    HID_FEATURE_REPORT, BCM5974_HID_REPORT_ID));
}

static int
bcm5974_set_device_mode(struct bcm5974_softc *sc, bool on)
{
	int err = 0;

	switch (sc->sc_params->tp - tp) {
	case TYPE1:
	case TYPE2:
		err = bcm5974_set_device_mode_usb(sc, on);
		break;
	case TYPE3:	/* Type 3 does not require a mode switch */
		break;
	case TYPE4:
	case TYPE_MT2U:
		err = bcm5974_set_device_mode_hid(sc, on);
		break;
	default:
		KASSERT(0 == 1, ("Unknown trackpad type"));
	}

	if (!err)
		sc->sc_saved_mode = on;

	return (err);
}

static void
bcm5974_identify(driver_t *driver, device_t parent)
{
	void *d_ptr;
	hid_size_t d_len;

	/*
	 * The bcm5974 touchpad has no stable RAW mode TLC in its report
	 * descriptor.  So replace existing HID mode mouse TLC with dummy one
	 * to set proper transport layer buffer sizes, make driver probe
	 * simpler and prevent unwanted hms driver attachment.
	 */
	if (HIDBUS_LOOKUP_ID(parent, bcm5974_devs) != NULL &&
	    hid_get_report_descr(parent, &d_ptr, &d_len) == 0 &&
	    hid_is_mouse(d_ptr, d_len))
		hid_set_report_descr(parent, bcm5974_rdesc,
		    sizeof(bcm5974_rdesc));
}

static int
bcm5974_probe(device_t dev)
{
	int err;

	err = HIDBUS_LOOKUP_DRIVER_INFO(dev, bcm5974_devs);
	if (err != 0)
		return (err);

	hidbus_set_desc(dev, "Touchpad");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm5974_attach(device_t dev)
{
	struct bcm5974_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw = hid_get_device_info(dev);
	int err;

	DPRINTFN(BCM5974_LLEVEL_INFO, "sc=%p\n", sc);

	sc->sc_dev = dev;

	/* get device specific configuration */
	sc->sc_params = bcm5974_dev_params + hidbus_get_driver_info(dev);

	sc->sc_evdev = evdev_alloc();
	evdev_set_name(sc->sc_evdev, device_get_desc(dev));
	evdev_set_phys(sc->sc_evdev, device_get_nameunit(dev));
	evdev_set_id(sc->sc_evdev, hw->idBus, hw->idVendor, hw->idProduct,
	    hw->idVersion);
	evdev_set_serial(sc->sc_evdev, hw->serial);
	evdev_set_methods(sc->sc_evdev, sc, &bcm5974_evdev_methods);
	evdev_support_prop(sc->sc_evdev, INPUT_PROP_POINTER);
	evdev_support_event(sc->sc_evdev, EV_SYN);
	evdev_support_event(sc->sc_evdev, EV_ABS);
	evdev_support_event(sc->sc_evdev, EV_KEY);
	evdev_set_flag(sc->sc_evdev, EVDEV_FLAG_EXT_EPOCH); /* hidbus child */

#define BCM5974_ABS(evdev, code, param)					\
	evdev_support_abs((evdev), (code), (param).min, (param).max,	\
	((param).max - (param).min) / (param).snratio, 0,		\
	(param).size != 0 ? ((param).max - (param).min) / (param).size : 0);

	/* finger position */
	BCM5974_ABS(sc->sc_evdev, ABS_MT_POSITION_X, sc->sc_params->x);
	BCM5974_ABS(sc->sc_evdev, ABS_MT_POSITION_Y, sc->sc_params->y);
	/* finger pressure */
	BCM5974_ABS(sc->sc_evdev, ABS_MT_PRESSURE, sc->sc_params->p);
	/* finger touch area */
	BCM5974_ABS(sc->sc_evdev, ABS_MT_TOUCH_MAJOR, sc->sc_params->w);
	BCM5974_ABS(sc->sc_evdev, ABS_MT_TOUCH_MINOR, sc->sc_params->w);
	/* finger approach area */
	if ((sc->sc_params->tp->caps & USES_COMPACT_REPORT) == 0) {
		BCM5974_ABS(sc->sc_evdev, ABS_MT_WIDTH_MAJOR, sc->sc_params->w);
		BCM5974_ABS(sc->sc_evdev, ABS_MT_WIDTH_MINOR, sc->sc_params->w);
	}
	/* finger orientation */
	BCM5974_ABS(sc->sc_evdev, ABS_MT_ORIENTATION, sc->sc_params->o);
	/* button properties */
	evdev_support_key(sc->sc_evdev, BTN_LEFT);
	if ((sc->sc_params->tp->caps & HAS_INTEGRATED_BUTTON) != 0)
		evdev_support_prop(sc->sc_evdev, INPUT_PROP_BUTTONPAD);
	/* Enable automatic touch assignment for type B MT protocol */
	evdev_support_abs(sc->sc_evdev, ABS_MT_SLOT,
	    0, MAX_FINGERS - 1, 0, 0, 0);
	evdev_support_abs(sc->sc_evdev, ABS_MT_TRACKING_ID,
	    -1, MAX_FINGERS - 1, 0, 0, 0);
	if ((sc->sc_params->tp->caps & USES_COMPACT_REPORT) == 0)
		evdev_set_flag(sc->sc_evdev, EVDEV_FLAG_MT_TRACK);
	evdev_set_flag(sc->sc_evdev, EVDEV_FLAG_MT_AUTOREL);
	/* Synaptics compatibility events */
	evdev_set_flag(sc->sc_evdev, EVDEV_FLAG_MT_STCOMPAT);

	err = evdev_register(sc->sc_evdev);
	if (err)
		goto detach;

	hidbus_set_intr(dev, bcm5974_intr, sc);

	return (0);

detach:
	bcm5974_detach(dev);
	return (ENOMEM);
}

static int
bcm5974_detach(device_t dev)
{
	struct bcm5974_softc *sc = device_get_softc(dev);

	evdev_free(sc->sc_evdev);

	return (0);
}

static int
bcm5974_resume(device_t dev)
{
	struct bcm5974_softc *sc = device_get_softc(dev);

	bcm5974_set_device_mode(sc, sc->sc_saved_mode);

	return (0);
}

static void
bcm5974_intr(void *context, void *data, hid_size_t len)
{
	struct bcm5974_softc *sc = context;
	const struct bcm5974_dev_params *params = sc->sc_params;
	union evdev_mt_slot slot_data;
	struct tp_finger *f;
	struct tp_finger_compact *fc;
	int coords;
	int ntouch;			/* the finger number in touch */
	int ibt;			/* button status */
	int i;
	int slot;
	uint8_t fsize = sizeof(struct tp_finger) + params->tp->delta;

	if ((params->tp->caps & USES_COMPACT_REPORT) != 0)
		fsize = sizeof(struct tp_finger_compact) + params->tp->delta;

	if ((len < params->tp->offset + fsize) ||
	    ((len - params->tp->offset) % fsize) != 0) {
		DPRINTFN(BCM5974_LLEVEL_INFO, "Invalid length: %d, %x, %x\n",
		    len, sc->tp_data[0], sc->tp_data[1]);
		return;
	}

	ibt = ((uint8_t *)data)[params->tp->button];
	ntouch = (len - params->tp->offset) / fsize;

	for (i = 0, slot = 0; i != ntouch; i++) {
		if ((params->tp->caps & USES_COMPACT_REPORT) != 0) {
			fc = (struct tp_finger_compact *)(((uint8_t *)data) +
			     params->tp->offset + params->tp->delta + i * fsize);
			coords = (int)le32toh(fc->coords);
			DPRINTFN(BCM5974_LLEVEL_INFO,
			    "[%d]ibt=%d, taps=%d, x=%5d, y=%5d, state=%4d, "
			    "tchmaj=%4d, tchmin=%4d, size=%4d, pressure=%4d, "
			    "ot=%4x, id=%4x\n",
			    i, ibt, ntouch, coords << 19 >> 19,
			    coords << 6 >> 19, (u_int)coords >> 30,
			    fc->touch_major, fc->touch_minor, fc->size,
			    fc->pressure, fc->id_ori >> 5, fc->id_ori & 0x0f);
			if (fc->touch_major == 0)
				continue;
			slot_data = (union evdev_mt_slot) {
				.id = fc->id_ori & 0x0f,
				.x = coords << 19 >> 19,
				.y = params->y.min + params->y.max -
				    ((coords << 6) >> 19),
				.p = fc->pressure,
				.maj = fc->touch_major << 2,
				.min = fc->touch_minor << 2,
				.ori = (int)(fc->id_ori >> 5) - 4,
			};
			evdev_mt_push_slot(sc->sc_evdev, slot, &slot_data);
			slot++;
			continue;
		}
		f = (struct tp_finger *)(((uint8_t *)data) +
		    params->tp->offset + params->tp->delta + i * fsize);
		DPRINTFN(BCM5974_LLEVEL_INFO,
		    "[%d]ibt=%d, taps=%d, o=%4d, ax=%5d, ay=%5d, "
		    "rx=%5d, ry=%5d, tlmaj=%4d, tlmin=%4d, ot=%4x, "
		    "tchmaj=%4d, tchmin=%4d, pressure=%4d, m=%4x\n",
		    i, ibt, ntouch, BCM5974_LE2H(f->origin),
		    BCM5974_LE2H(f->abs_x), BCM5974_LE2H(f->abs_y),
		    BCM5974_LE2H(f->rel_x), BCM5974_LE2H(f->rel_y),
		    BCM5974_LE2H(f->tool_major), BCM5974_LE2H(f->tool_minor),
		    BCM5974_LE2H(f->orientation), BCM5974_LE2H(f->touch_major),
		    BCM5974_LE2H(f->touch_minor), BCM5974_LE2H(f->pressure),
		    BCM5974_LE2H(f->multi));

		if (BCM5974_LE2H(f->touch_major) == 0)
			continue;
		slot_data = (union evdev_mt_slot) {
			.id = slot,
			.x = BCM5974_LE2H(f->abs_x),
			.y = params->y.min + params->y.max -
			     BCM5974_LE2H(f->abs_y),
			.p = BCM5974_LE2H(f->pressure),
			.maj = BCM5974_LE2H(f->touch_major) << 1,
			.min = BCM5974_LE2H(f->touch_minor) << 1,
			.w_maj = BCM5974_LE2H(f->tool_major) << 1,
			.w_min = BCM5974_LE2H(f->tool_minor) << 1,
			.ori = params->o.max - BCM5974_LE2H(f->orientation),
		};
		evdev_mt_push_slot(sc->sc_evdev, slot, &slot_data);
		slot++;
	}

	evdev_push_key(sc->sc_evdev, BTN_LEFT, ibt);
	evdev_sync(sc->sc_evdev);
}

static int
bcm5974_ev_open(struct evdev_dev *evdev)
{
	struct bcm5974_softc *sc = evdev_get_softc(evdev);
	int err;

	/*
	 * By default the touchpad behaves like a HID device, sending
	 * packets with reportID = 8. Such reports contain only
	 * limited information. They encode movement deltas and button
	 * events, but do not include data from the pressure
	 * sensors. The device input mode can be switched from HID
	 * reports to raw sensor data using vendor-specific USB
	 * control commands:
	 */
	err = bcm5974_set_device_mode(sc, true);
	if (err != 0) {
		DPRINTF("failed to set mode to RAW MODE (%d)\n", err);
		return (err);
	}

	return (hid_intr_start(sc->sc_dev));
}

static int
bcm5974_ev_close(struct evdev_dev *evdev)
{
	struct bcm5974_softc *sc = evdev_get_softc(evdev);
	int err;

	err = hid_intr_stop(sc->sc_dev);
	if (err != 0)
		return (err);

	/*
	 * During re-enumeration of the device we need to force the
	 * device back into HID mode before switching it to RAW
	 * mode. Else the device does not work like expected.
	 */
	err = bcm5974_set_device_mode(sc, false);
	if (err != 0)
		DPRINTF("Failed to set mode to HID MODE (%d)\n", err);

	return (err);
}

static device_method_t bcm5974_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	bcm5974_identify),
	DEVMETHOD(device_probe,		bcm5974_probe),
	DEVMETHOD(device_attach,	bcm5974_attach),
	DEVMETHOD(device_detach,	bcm5974_detach),
	DEVMETHOD(device_resume,	bcm5974_resume),
	DEVMETHOD_END
};

static driver_t bcm5974_driver = {
	.name = "bcm5974",
	.methods = bcm5974_methods,
	.size = sizeof(struct bcm5974_softc)
};

DRIVER_MODULE(bcm5974, hidbus, bcm5974_driver, NULL, NULL);
MODULE_DEPEND(bcm5974, hidbus, 1, 1, 1);
MODULE_DEPEND(bcm5974, hid, 1, 1, 1);
MODULE_DEPEND(bcm5974, evdev, 1, 1, 1);
MODULE_VERSION(bcm5974, 1);
HID_PNP_INFO(bcm5974_devs);
