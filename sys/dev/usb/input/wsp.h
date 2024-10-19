/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Huang Wen Hui
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

#ifndef _DEV_USB_INPUT_WSP_H_
#define _DEV_USB_INPUT_WSP_H_

#define WSP_DRIVER_NAME "wsp"
#define WSP_BUFFER_MAX  1024

#define USB_DEBUG_VAR wsp_debug
#include <dev/usb/usb_debug.h>

#include <sys/mouse.h>

#ifdef EVDEV_SUPPORT
#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>
#endif

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

/* button data structure */
struct bt_data {
	uint8_t	unknown1;		/* constant */
	uint8_t	button;			/* left button */
	uint8_t	rel_x;			/* relative x coordinate */
	uint8_t	rel_y;			/* relative y coordinate */
} __packed;

/* trackpad header types */
enum tp_type {
	TYPE1,			/* plain trackpad */
	TYPE2,			/* button integrated in trackpad */
	TYPE3,			/* additional header fields since June 2013 */
	TYPE4,                  /* additional header field for pressure data */
	TYPE_CNT
};

/* trackpad finger data offsets, le16-aligned */
#define	FINGER_TYPE1		(13 * 2)
#define	FINGER_TYPE2		(15 * 2)
#define	FINGER_TYPE3		(19 * 2)
#define	FINGER_TYPE4		(23 * 2)

/* trackpad button data offsets */
#define	BUTTON_TYPE2		15
#define	BUTTON_TYPE3		23
#define	BUTTON_TYPE4		31

/* list of device capability bits */
#define	HAS_INTEGRATED_BUTTON	1

/* trackpad finger data block size */
#define FSIZE_TYPE1             (14 * 2)
#define FSIZE_TYPE2             (14 * 2)
#define FSIZE_TYPE3             (14 * 2)
#define FSIZE_TYPE4             (15 * 2)

struct wsp_tp {
	uint8_t	caps;			/* device capability bitmask */
	uint8_t	button;			/* offset to button data */
	uint8_t	offset;			/* offset to trackpad finger data */
	uint8_t fsize;			/* bytes in single finger block */
	uint8_t delta;			/* offset from header to finger struct */
	uint8_t iface_index;
	uint8_t um_size;		/* usb control message length */
	uint8_t um_req_idx;		/* usb control message index */
	uint8_t um_switch_idx;		/* usb control message mode switch index */
	uint8_t um_switch_on;		/* usb control message mode switch on */
	uint8_t um_switch_off;		/* usb control message mode switch off */
} const static wsp_tp[TYPE_CNT] = {
	[TYPE1] = {
		.caps = 0,
		.button = 0,
		.offset = FINGER_TYPE1,
		.fsize = FSIZE_TYPE1,
		.delta = 0,
		.iface_index = 0,
		.um_size = 8,
		.um_req_idx = 0x00,
		.um_switch_idx = 0,
		.um_switch_on = 0x01,
		.um_switch_off = 0x08,
	},
	[TYPE2] = {
		.caps = HAS_INTEGRATED_BUTTON,
		.button = BUTTON_TYPE2,
		.offset = FINGER_TYPE2,
		.fsize = FSIZE_TYPE2,
		.delta = 0,
		.iface_index = 0,
		.um_size = 8,
		.um_req_idx = 0x00,
		.um_switch_idx = 0,
		.um_switch_on = 0x01,
		.um_switch_off = 0x08,
	},
	[TYPE3] = {
		.caps = HAS_INTEGRATED_BUTTON,
		.button = BUTTON_TYPE3,
		.offset = FINGER_TYPE3,
		.fsize = FSIZE_TYPE3,
		.delta = 0,
	},
	[TYPE4] = {
		.caps = HAS_INTEGRATED_BUTTON,
		.button = BUTTON_TYPE4,
		.offset = FINGER_TYPE4,
		.fsize = FSIZE_TYPE4,
		.delta = 2,
		.iface_index = 2,
		.um_size = 2,
		.um_req_idx = 0x02,
		.um_switch_idx = 1,
		.um_switch_on = 0x01,
		.um_switch_off = 0x00,
	},
};

/* trackpad finger header - little endian */
struct tp_header {
	uint8_t	flag;
	uint8_t	sn0;
	uint16_t wFixed0;
	uint32_t dwSn1;
	uint32_t dwFixed1;
	uint16_t wLength;
	uint8_t	nfinger;
	uint8_t	ibt;
	int16_t	wUnknown[6];
	uint8_t	q1;
	uint8_t	q2;
} __packed;

/* trackpad finger structure - little endian */
struct tp_finger {
	int16_t	origin;			/* zero when switching track finger */
	int16_t	abs_x;			/* absolute x coodinate */
	int16_t	abs_y;			/* absolute y coodinate */
	int16_t	rel_x;			/* relative x coodinate */
	int16_t	rel_y;			/* relative y coodinate */
	int16_t	tool_major;		/* tool area, major axis */
	int16_t	tool_minor;		/* tool area, minor axis */
	int16_t	orientation;		/* 16384 when point, else 15 bit angle */
	int16_t	touch_major;		/* touch area, major axis */
	int16_t	touch_minor;		/* touch area, minor axis */
	int16_t	unused[2];		/* zeros */
	int16_t pressure;		/* pressure on forcetouch touchpad */
	int16_t	multi;			/* one finger: varies, more fingers:
					 * constant */
} __packed;

/* trackpad finger data size, empirically at least ten fingers */
#ifdef EVDEV_SUPPORT
#define	MAX_FINGERS		MAX_MT_SLOTS
#else
#define	MAX_FINGERS		16
#endif
#define	SIZEOF_FINGER		sizeof(struct tp_finger)
#define	SIZEOF_ALL_FINGERS	(MAX_FINGERS * SIZEOF_FINGER)
#define	MAX_FINGER_ORIENTATION	16384

#if (WSP_BUFFER_MAX < ((MAX_FINGERS * FSIZE_TYPE4) + FINGER_TYPE4))
#error "WSP_BUFFER_MAX is too small"
#endif

enum {
	WSP_FLAG_WELLSPRING1,
	WSP_FLAG_WELLSPRING2,
	WSP_FLAG_WELLSPRING3,
	WSP_FLAG_WELLSPRING4,
	WSP_FLAG_WELLSPRING4A,
	WSP_FLAG_WELLSPRING5,
	WSP_FLAG_WELLSPRING6A,
	WSP_FLAG_WELLSPRING6,
	WSP_FLAG_WELLSPRING5A,
	WSP_FLAG_WELLSPRING7,
	WSP_FLAG_WELLSPRING7A,
	WSP_FLAG_WELLSPRING8,
	WSP_FLAG_WELLSPRING9,
	WSP_FLAG_MAX,
};

/* device-specific parameters */
struct wsp_param {
	int snratio;			/* signal-to-noise ratio */
	int min;			/* device minimum reading */
	int max;			/* device maximum reading */
	int size;			/* physical size, mm */
};

/* device-specific configuration */
struct wsp_dev_params {
	const struct wsp_tp* tp;
	struct wsp_param p;		/* finger pressure limits */
	struct wsp_param w;		/* finger width limits */
	struct wsp_param x;		/* horizontal limits */
	struct wsp_param y;		/* vertical limits */
	struct wsp_param o;		/* orientation limits */
};

/* logical signal quality */
#define	SN_PRESSURE	45		/* pressure signal-to-noise ratio */
#define	SN_WIDTH	25		/* width signal-to-noise ratio */
#define	SN_COORD	250		/* coordinate signal-to-noise ratio */
#define	SN_ORIENT	10		/* orientation signal-to-noise ratio */

static const struct wsp_dev_params wsp_dev_params[WSP_FLAG_MAX] = {
	[WSP_FLAG_WELLSPRING1] = {
		.tp = wsp_tp + TYPE1,
		.p = { SN_PRESSURE, 0, 256, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4824, 5342, 105 },
		.y = { SN_COORD, -172, 5820, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING2] = {
		.tp = wsp_tp + TYPE1,
		.p = { SN_PRESSURE, 0, 256, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4824, 4824, 105 },
		.y = { SN_COORD, -172, 4290, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING3] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4460, 5166, 105 },
		.y = { SN_COORD, -75, 6700, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING4] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING4A] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4616, 5112, 105 },
		.y = { SN_COORD, -142, 5234, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING5] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4415, 5050, 105 },
		.y = { SN_COORD, -55, 6680, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING6] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING5A] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4750, 5280, 105 },
		.y = { SN_COORD, -150, 6730, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING6A] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING7] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4750, 5280, 105 },
		.y = { SN_COORD, -150, 6730, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING7A] = {
		.tp = wsp_tp + TYPE2,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4750, 5280, 105 },
		.y = { SN_COORD, -150, 6730, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING8] = {
		.tp = wsp_tp + TYPE3,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4620, 5140, 105 },
		.y = { SN_COORD, -150, 6600, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
	[WSP_FLAG_WELLSPRING9] = {
		.tp = wsp_tp + TYPE4,
		.p = { SN_PRESSURE, 0, 300, 0 },
		.w = { SN_WIDTH, 0, 2048, 0 },
		.x = { SN_COORD, -4828, 5345, 105 },
		.y = { SN_COORD, -203, 6803, 75 },
		.o = { SN_ORIENT,
		    -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0 },
	},
};
#define	WSP_DEV(v,p,i) { USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, i) }

static const STRUCT_USB_HOST_ID wsp_devs[] = {
	/* MacbookAir1.1 */
	WSP_DEV(APPLE, WELLSPRING_ANSI, WSP_FLAG_WELLSPRING1),
	WSP_DEV(APPLE, WELLSPRING_ISO, WSP_FLAG_WELLSPRING1),
	WSP_DEV(APPLE, WELLSPRING_JIS, WSP_FLAG_WELLSPRING1),

	/* MacbookProPenryn, aka wellspring2 */
	WSP_DEV(APPLE, WELLSPRING2_ANSI, WSP_FLAG_WELLSPRING2),
	WSP_DEV(APPLE, WELLSPRING2_ISO, WSP_FLAG_WELLSPRING2),
	WSP_DEV(APPLE, WELLSPRING2_JIS, WSP_FLAG_WELLSPRING2),

	/* Macbook5,1 (unibody), aka wellspring3 */
	WSP_DEV(APPLE, WELLSPRING3_ANSI, WSP_FLAG_WELLSPRING3),
	WSP_DEV(APPLE, WELLSPRING3_ISO, WSP_FLAG_WELLSPRING3),
	WSP_DEV(APPLE, WELLSPRING3_JIS, WSP_FLAG_WELLSPRING3),

	/* MacbookAir3,2 (unibody), aka wellspring4 */
	WSP_DEV(APPLE, WELLSPRING4_ANSI, WSP_FLAG_WELLSPRING4),
	WSP_DEV(APPLE, WELLSPRING4_ISO, WSP_FLAG_WELLSPRING4),
	WSP_DEV(APPLE, WELLSPRING4_JIS, WSP_FLAG_WELLSPRING4),

	/* MacbookAir3,1 (unibody), aka wellspring4 */
	WSP_DEV(APPLE, WELLSPRING4A_ANSI, WSP_FLAG_WELLSPRING4A),
	WSP_DEV(APPLE, WELLSPRING4A_ISO, WSP_FLAG_WELLSPRING4A),
	WSP_DEV(APPLE, WELLSPRING4A_JIS, WSP_FLAG_WELLSPRING4A),

	/* Macbook8 (unibody, March 2011) */
	WSP_DEV(APPLE, WELLSPRING5_ANSI, WSP_FLAG_WELLSPRING5),
	WSP_DEV(APPLE, WELLSPRING5_ISO, WSP_FLAG_WELLSPRING5),
	WSP_DEV(APPLE, WELLSPRING5_JIS, WSP_FLAG_WELLSPRING5),

	/* MacbookAir4,1 (unibody, July 2011) */
	WSP_DEV(APPLE, WELLSPRING6A_ANSI, WSP_FLAG_WELLSPRING6A),
	WSP_DEV(APPLE, WELLSPRING6A_ISO, WSP_FLAG_WELLSPRING6A),
	WSP_DEV(APPLE, WELLSPRING6A_JIS, WSP_FLAG_WELLSPRING6A),

	/* MacbookAir4,2 (unibody, July 2011) */
	WSP_DEV(APPLE, WELLSPRING6_ANSI, WSP_FLAG_WELLSPRING6),
	WSP_DEV(APPLE, WELLSPRING6_ISO, WSP_FLAG_WELLSPRING6),
	WSP_DEV(APPLE, WELLSPRING6_JIS, WSP_FLAG_WELLSPRING6),

	/* Macbook8,2 (unibody) */
	WSP_DEV(APPLE, WELLSPRING5A_ANSI, WSP_FLAG_WELLSPRING5A),
	WSP_DEV(APPLE, WELLSPRING5A_ISO, WSP_FLAG_WELLSPRING5A),
	WSP_DEV(APPLE, WELLSPRING5A_JIS, WSP_FLAG_WELLSPRING5A),

	/* MacbookPro10,1 (unibody, June 2012) */
	/* MacbookPro11,1-3 (unibody, June 2013) */
	WSP_DEV(APPLE, WELLSPRING7_ANSI, WSP_FLAG_WELLSPRING7),
	WSP_DEV(APPLE, WELLSPRING7_ISO, WSP_FLAG_WELLSPRING7),
	WSP_DEV(APPLE, WELLSPRING7_JIS, WSP_FLAG_WELLSPRING7),

	/* MacbookPro10,2 (unibody, October 2012) */
	WSP_DEV(APPLE, WELLSPRING7A_ANSI, WSP_FLAG_WELLSPRING7A),
	WSP_DEV(APPLE, WELLSPRING7A_ISO, WSP_FLAG_WELLSPRING7A),
	WSP_DEV(APPLE, WELLSPRING7A_JIS, WSP_FLAG_WELLSPRING7A),

	/* MacbookAir6,2 (unibody, June 2013) */
	WSP_DEV(APPLE, WELLSPRING8_ANSI, WSP_FLAG_WELLSPRING8),
	WSP_DEV(APPLE, WELLSPRING8_ISO, WSP_FLAG_WELLSPRING8),
	WSP_DEV(APPLE, WELLSPRING8_JIS, WSP_FLAG_WELLSPRING8),

	/* MacbookPro12,1 MacbookPro11,4 */
	WSP_DEV(APPLE, WELLSPRING9_ANSI, WSP_FLAG_WELLSPRING9),
	WSP_DEV(APPLE, WELLSPRING9_ISO, WSP_FLAG_WELLSPRING9),
	WSP_DEV(APPLE, WELLSPRING9_JIS, WSP_FLAG_WELLSPRING9),
};

#define	WSP_FIFO_BUF_SIZE	 8	/* bytes */
#define	WSP_FIFO_QUEUE_MAXLEN	50	/* units */

enum {
	WSP_INTR_DT,
	WSP_N_TRANSFER,
};

struct wsp_softc {
	struct usb_device *sc_usb_device;
	struct mtx sc_mutex;		/* for synchronization */
	struct usb_xfer *sc_xfer[WSP_N_TRANSFER];
	struct usb_fifo_sc sc_fifo;

	const struct wsp_dev_params *sc_params;	/* device configuration */

#ifdef EVDEV_SUPPORT
	struct evdev_dev *sc_evdev;
#endif
	mousehw_t sc_hw;
	mousemode_t sc_mode;
	u_int	sc_pollrate;
	mousestatus_t sc_status;
	int	sc_fflags;
	u_int	sc_state;
#define	WSP_ENABLED		0x01
#define	WSP_EVDEV_OPENED	0x02

	struct tp_finger *index[MAX_FINGERS];	/* finger index data */
	int16_t	pos_x[MAX_FINGERS];	/* position array */
	int16_t	pos_y[MAX_FINGERS];	/* position array */
	int16_t pre_pos_x[MAX_FINGERS];	/* previous position array */
	int16_t pre_pos_y[MAX_FINGERS]; /* previous position array */
	u_int	sc_touch;		/* touch status */
#define	WSP_UNTOUCH		0x00
#define	WSP_FIRST_TOUCH		0x01
#define	WSP_SECOND_TOUCH	0x02
#define	WSP_TOUCHING		0x04
	int	dx_sum;			/* x axis cumulative movement */
	int	dy_sum;			/* y axis cumulative movement */
	int	dz_sum;			/* z axis cumulative movement */
	int	dz_count;
#define	WSP_DZ_MAX_COUNT	32
	int	dt_sum;			/* T-axis cumulative movement */
	int	rdx;			/* x axis remainder of divide by scale_factor */
	int	rdy;			/* y axis remainder of divide by scale_factor */
	int	rdz;			/* z axis remainder of divide by scale_factor */
	int	tp_datalen;
	uint8_t o_ntouch;		/* old touch finger status */
	uint8_t	finger;			/* 0 or 1 *, check which finger moving */
	uint16_t intr_count;
#define	WSP_TAP_THRESHOLD	3
#define	WSP_TAP_MAX_COUNT	20
	int	distance;		/* the distance of 2 fingers */
	uint8_t	ibtn;			/* button status in tapping */
	uint8_t	ntaps;			/* finger status in tapping */
	uint8_t	scr_mode;		/* scroll status in movement */
#define	WSP_SCR_NONE		0
#define	WSP_SCR_VER		1
#define	WSP_SCR_HOR		2
	uint8_t tp_data[WSP_BUFFER_MAX] __aligned(4);		/* trackpad transferred data */
};

#endif /* !_DEV_USB_INPUT_WSP_H_ */
