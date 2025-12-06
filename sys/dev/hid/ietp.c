/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020, 2022 Vladimir Kondratyev <wulf@FreeBSD.org>
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

/*
 * Elan I2C Touchpad driver. Based on Linux driver.
 * https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/drivers/input/mouse/elan_i2c_core.c
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>

#include <dev/iicbus/iic.h>
#include <dev/iicbus/iicbus.h>

#define HID_DEBUG_VAR   ietp_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>

#ifdef HID_DEBUG
static SYSCTL_NODE(_hw_hid, OID_AUTO, ietp, CTLFLAG_RW, 0,
    "Elantech Touchpad");
static int ietp_debug = 1;
SYSCTL_INT(_hw_hid_ietp, OID_AUTO, debug, CTLFLAG_RWTUN,
    &ietp_debug, 1, "Debug level");
#endif

#define	IETP_PATTERN		0x0100
#define	IETP_UNIQUEID		0x0101
#define	IETP_FW_VERSION		0x0102
#define	IETP_IC_TYPE		0x0103
#define	IETP_OSM_VERSION	0x0103
#define	IETP_NSM_VERSION	0x0104
#define	IETP_TRACENUM		0x0105
#define	IETP_MAX_X_AXIS		0x0106
#define	IETP_MAX_Y_AXIS		0x0107
#define	IETP_RESOLUTION		0x0108
#define	IETP_PRESSURE		0x010A

#define	IETP_CONTROL		0x0300
#define	IETP_CTRL_ABSOLUTE	0x0001
#define	IETP_CTRL_STANDARD	0x0000

#define	IETP_REPORT_LEN_LO	32
#define	IETP_REPORT_LEN_HI	37
#define	IETP_MAX_FINGERS	5

#define	IETP_REPORT_ID_LO	0x5D
#define	IETP_REPORT_ID_HI	0x60

#define	IETP_TOUCH_INFO		1
#define	IETP_FINGER_DATA	2
#define	IETP_FINGER_DATA_LEN	5
#define	IETP_HOVER_INFO		28
#define	IETP_WH_DATA		31

#define	IETP_TOUCH_LMB		(1 << 0)
#define	IETP_TOUCH_RMB		(1 << 1)
#define	IETP_TOUCH_MMB		(1 << 2)

#define	IETP_MAX_PRESSURE	255
#define	IETP_FWIDTH_REDUCE	90
#define	IETP_FINGER_MAX_WIDTH	15
#define	IETP_PRESSURE_BASE	25

struct ietp_softc {
	device_t		dev;

	struct evdev_dev	*evdev;
	bool			open;
	uint8_t			report_id;
	hid_size_t		report_len;

	uint16_t		product_id;
	uint16_t		ic_type;

	int32_t			pressure_base;
	uint16_t		max_x;
	uint16_t		max_y;
	uint16_t		trace_x;
	uint16_t		trace_y;
	uint16_t		res_x;		/* dots per mm */
	uint16_t		res_y;
	bool			hi_precision;
	bool			is_clickpad;
	bool			has_3buttons;
};

static evdev_open_t	ietp_ev_open;
static evdev_close_t	ietp_ev_close;
static hid_intr_t	ietp_intr;

static int		ietp_probe(struct ietp_softc *);
static int		ietp_attach(struct ietp_softc *);
static int		ietp_detach(struct ietp_softc *);
static int32_t		ietp_res2dpmm(uint8_t, bool);

static device_identify_t ietp_iic_identify;
static device_probe_t	ietp_iic_probe;
static device_attach_t	ietp_iic_attach;
static device_detach_t	ietp_iic_detach;
static device_resume_t	ietp_iic_resume;

static int		ietp_iic_read_reg(device_t, uint16_t, size_t, void *);
static int		ietp_iic_write_reg(device_t, uint16_t, uint16_t);
static int		ietp_iic_set_absolute_mode(device_t, bool);

#define	IETP_IIC_DEV(pnp) \
    { HID_TLC(HUP_GENERIC_DESKTOP, HUG_MOUSE), HID_BUS(BUS_I2C), HID_PNP(pnp) }

static const struct hid_device_id ietp_iic_devs[] = {
	IETP_IIC_DEV("ELAN0000"),
	IETP_IIC_DEV("ELAN0100"),
	IETP_IIC_DEV("ELAN0600"),
	IETP_IIC_DEV("ELAN0601"),
	IETP_IIC_DEV("ELAN0602"),
	IETP_IIC_DEV("ELAN0603"),
	IETP_IIC_DEV("ELAN0604"),
	IETP_IIC_DEV("ELAN0605"),
	IETP_IIC_DEV("ELAN0606"),
	IETP_IIC_DEV("ELAN0607"),
	IETP_IIC_DEV("ELAN0608"),
	IETP_IIC_DEV("ELAN0609"),
	IETP_IIC_DEV("ELAN060B"),
	IETP_IIC_DEV("ELAN060C"),
	IETP_IIC_DEV("ELAN060F"),
	IETP_IIC_DEV("ELAN0610"),
	IETP_IIC_DEV("ELAN0611"),
	IETP_IIC_DEV("ELAN0612"),
	IETP_IIC_DEV("ELAN0615"),
	IETP_IIC_DEV("ELAN0616"),
	IETP_IIC_DEV("ELAN0617"),
	IETP_IIC_DEV("ELAN0618"),
	IETP_IIC_DEV("ELAN0619"),
	IETP_IIC_DEV("ELAN061A"),
	IETP_IIC_DEV("ELAN061B"),
	IETP_IIC_DEV("ELAN061C"),
	IETP_IIC_DEV("ELAN061D"),
	IETP_IIC_DEV("ELAN061E"),
	IETP_IIC_DEV("ELAN061F"),
	IETP_IIC_DEV("ELAN0620"),
	IETP_IIC_DEV("ELAN0621"),
	IETP_IIC_DEV("ELAN0622"),
	IETP_IIC_DEV("ELAN0623"),
	IETP_IIC_DEV("ELAN0624"),
	IETP_IIC_DEV("ELAN0625"),
	IETP_IIC_DEV("ELAN0626"),
	IETP_IIC_DEV("ELAN0627"),
	IETP_IIC_DEV("ELAN0628"),
	IETP_IIC_DEV("ELAN0629"),
	IETP_IIC_DEV("ELAN062A"),
	IETP_IIC_DEV("ELAN062B"),
	IETP_IIC_DEV("ELAN062C"),
	IETP_IIC_DEV("ELAN062D"),
	IETP_IIC_DEV("ELAN062E"),	/* Lenovo V340 Whiskey Lake U */
	IETP_IIC_DEV("ELAN062F"),	/* Lenovo V340 Comet Lake U */
	IETP_IIC_DEV("ELAN0631"),
	IETP_IIC_DEV("ELAN0632"),
	IETP_IIC_DEV("ELAN0633"),	/* Lenovo S145 */
	IETP_IIC_DEV("ELAN0634"),	/* Lenovo V340 Ice lake */
	IETP_IIC_DEV("ELAN0635"),	/* Lenovo V1415-IIL */
	IETP_IIC_DEV("ELAN0636"),	/* Lenovo V1415-Dali */
	IETP_IIC_DEV("ELAN0637"),	/* Lenovo V1415-IGLR */
	IETP_IIC_DEV("ELAN1000"),
};

static uint8_t const ietp_dummy_rdesc_lo[] = {
	0x05, HUP_GENERIC_DESKTOP,	/* Usage Page (Generic Desktop Ctrls)	*/
	0x09, HUG_MOUSE,		/* Usage (Mouse)			*/
	0xA1, 0x01,			/* Collection (Application)		*/
	0x09, 0x01,			/*   Usage (0x01)			*/
	0x15, 0x00,			/*   Logical Minimum (0)                */
	0x26, 0xFF, 0x00,		/*   Logical Maximum (255)              */
	0x95, IETP_REPORT_LEN_LO,	/*   Report Count (IETP_REPORT_LEN_LO)	*/
	0x75, 0x08,			/*   Report Size (8)			*/
	0x81, 0x02,			/*   Input (Data,Var,Abs)		*/
	0xC0,				/* End Collection			*/
};

static uint8_t const ietp_dummy_rdesc_hi[] = {
	0x05, HUP_GENERIC_DESKTOP,	/* Usage Page (Generic Desktop Ctrls)	*/
	0x09, HUG_MOUSE,		/* Usage (Mouse)			*/
	0xA1, 0x01,			/* Collection (Application)		*/
	0x09, 0x01,			/*   Usage (0x01)			*/
	0x15, 0x00,			/*   Logical Minimum (0)                */
	0x26, 0xFF, 0x00,		/*   Logical Maximum (255)              */
	0x95, IETP_REPORT_LEN_HI,	/*   Report Count (IETP_REPORT_LEN_HI)	*/
	0x75, 0x08,			/*   Report Size (8)			*/
	0x81, 0x02,			/*   Input (Data,Var,Abs)		*/
	0xC0,				/* End Collection			*/
};

static const struct evdev_methods ietp_evdev_methods = {
	.ev_open = &ietp_ev_open,
	.ev_close = &ietp_ev_close,
};

static int
ietp_ev_open(struct evdev_dev *evdev)
{
	struct ietp_softc *sc = evdev_get_softc(evdev);
	int error;

	error = hid_intr_start(sc->dev);
	if (error == 0)
		sc->open = true;
	return (error);
}

static int
ietp_ev_close(struct evdev_dev *evdev)
{
	struct ietp_softc *sc = evdev_get_softc(evdev);
	int error;

	error = hid_intr_stop(sc->dev);
	if (error == 0)
		sc->open = false;
	return (error);
}

static int
ietp_probe(struct ietp_softc *sc)
{
	if (hidbus_find_child(device_get_parent(sc->dev),
	    HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHPAD)) != NULL) {
		DPRINTFN(5, "Ignore HID-compatible touchpad on %s\n",
		    device_get_nameunit(device_get_parent(sc->dev)));
		return (ENXIO);
	}

	device_set_desc(sc->dev, "Elan Touchpad");

	return (BUS_PROBE_DEFAULT);
}

static int
ietp_attach(struct ietp_softc *sc)
{
	const struct hid_device_info *hw = hid_get_device_info(sc->dev);
	void *d_ptr;
	hid_size_t d_len;
	int32_t minor, major;
	int error;

	sc->report_id = sc->hi_precision ?
	    IETP_REPORT_ID_HI : IETP_REPORT_ID_LO;
	sc->report_len = sc->hi_precision ?
	    IETP_REPORT_LEN_HI : IETP_REPORT_LEN_LO;

	/* Try to detect 3-rd button by relative mouse TLC */
	if (!sc->is_clickpad) {
		error = hid_get_report_descr(sc->dev, &d_ptr, &d_len);
		if (error != 0) {
			device_printf(sc->dev, "could not retrieve report "
			    "descriptor from device: %d\n", error);
			return (ENXIO);
		}
		if (hidbus_locate(d_ptr, d_len, HID_USAGE2(HUP_BUTTON, 3),
		    hid_input, hidbus_get_index(sc->dev), 0, NULL, NULL, NULL,
		    NULL))
			sc->has_3buttons = true;
	}

	sc->evdev = evdev_alloc();
	evdev_set_name(sc->evdev, device_get_desc(sc->dev));
	evdev_set_phys(sc->evdev, device_get_nameunit(sc->dev));
	evdev_set_id(sc->evdev, hw->idBus, hw->idVendor, hw->idProduct,
	    hw->idVersion);
	evdev_set_serial(sc->evdev, hw->serial);
	evdev_set_methods(sc->evdev, sc, &ietp_evdev_methods);
	evdev_set_flag(sc->evdev, EVDEV_FLAG_MT_STCOMPAT);
	evdev_set_flag(sc->evdev, EVDEV_FLAG_EXT_EPOCH); /* hidbus child */

	evdev_support_event(sc->evdev, EV_SYN);
	evdev_support_event(sc->evdev, EV_ABS);
	evdev_support_event(sc->evdev, EV_KEY);
	evdev_support_prop(sc->evdev, INPUT_PROP_POINTER);
	evdev_support_key(sc->evdev, BTN_LEFT);
	if (sc->is_clickpad) {
		evdev_support_prop(sc->evdev, INPUT_PROP_BUTTONPAD);
	} else {
		evdev_support_key(sc->evdev, BTN_RIGHT);
		if (sc->has_3buttons)
			evdev_support_key(sc->evdev, BTN_MIDDLE);
	}

	major = IETP_FINGER_MAX_WIDTH * MAX(sc->trace_x, sc->trace_y);
	minor = IETP_FINGER_MAX_WIDTH * MIN(sc->trace_x, sc->trace_y);

	evdev_support_abs(sc->evdev, ABS_MT_SLOT,
	    0, IETP_MAX_FINGERS - 1, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_TRACKING_ID,
	    -1, IETP_MAX_FINGERS - 1, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_POSITION_X,
	    0, sc->max_x, 0, 0, sc->res_x);
	evdev_support_abs(sc->evdev, ABS_MT_POSITION_Y,
	    0, sc->max_y, 0, 0, sc->res_y);
	evdev_support_abs(sc->evdev, ABS_MT_PRESSURE,
	    0, IETP_MAX_PRESSURE, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_ORIENTATION, 0, 1, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_TOUCH_MAJOR, 0, major, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_TOUCH_MINOR, 0, minor, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_DISTANCE, 0, 1, 0, 0, 0);

	error = evdev_register(sc->evdev);
	if (error != 0) {
		ietp_detach(sc);
		return (ENOMEM);
	}

	hidbus_set_intr(sc->dev, ietp_intr, sc);

	device_printf(sc->dev, "[%d:%d], %s\n", sc->max_x, sc->max_y,
	    sc->is_clickpad ? "clickpad" :
	    sc->has_3buttons ? "3 buttons" : "2 buttons");

	return (0);
}

static int
ietp_detach(struct ietp_softc *sc)
{
	evdev_free(sc->evdev);

	return (0);
}

static void
ietp_intr(void *context, void *buf, hid_size_t len)
{
	struct ietp_softc *sc = context;
	union evdev_mt_slot slot_data;
	uint8_t *report, *fdata;
	int32_t finger;
	int32_t x, y, w, h, wh;

	/* we seem to get 0 length reports sometimes, ignore them */
	if (len == 0)
		return;
	if (len != sc->report_len) {
		DPRINTF("wrong report length (%d vs %d expected)", len, sc->report_len);
		return;
	}

	report = buf;
	if (*report != sc->report_id)
		return;

	evdev_push_key(sc->evdev, BTN_LEFT,
	    report[IETP_TOUCH_INFO] & IETP_TOUCH_LMB);
	evdev_push_key(sc->evdev, BTN_MIDDLE,
	    report[IETP_TOUCH_INFO] & IETP_TOUCH_MMB);
	evdev_push_key(sc->evdev, BTN_RIGHT,
	    report[IETP_TOUCH_INFO] & IETP_TOUCH_RMB);
	evdev_push_abs(sc->evdev, ABS_DISTANCE,
	    (report[IETP_HOVER_INFO] & 0x40) >> 6);

	for (finger = 0, fdata = report + IETP_FINGER_DATA;
	     finger < IETP_MAX_FINGERS;
	     finger++, fdata += IETP_FINGER_DATA_LEN) {
		if ((report[IETP_TOUCH_INFO] & (1 << (finger + 3))) != 0) {
			if (sc->hi_precision) {
				x = fdata[0] << 8 | fdata[1];
				y = fdata[2] << 8 | fdata[3];
				wh = report[IETP_WH_DATA + finger];
			} else {
				x = (fdata[0] & 0xf0) << 4 | fdata[1];
				y = (fdata[0] & 0x0f) << 8 | fdata[2];
				wh = fdata[3];
			}

			if (x > sc->max_x || y > sc->max_y) {
				DPRINTF("[%d] x=%d y=%d over max (%d, %d)",
				    finger, x, y, sc->max_x, sc->max_y);
				continue;
			}

			/* Reduce trace size to not treat large finger as palm */
			w = (wh & 0x0F) * (sc->trace_x - IETP_FWIDTH_REDUCE);
			h = (wh >> 4) * (sc->trace_y - IETP_FWIDTH_REDUCE);

			slot_data = (union evdev_mt_slot) {
				.id = finger,
				.x = x,
				.y = sc->max_y - y,
				.p = MIN((int32_t)fdata[4] + sc->pressure_base,
				    IETP_MAX_PRESSURE),
				.ori = w > h ? 1 : 0,
				.maj = MAX(w, h),
				.min = MIN(w, h),
			};
			evdev_mt_push_slot(sc->evdev, finger, &slot_data);
		} else {
			evdev_push_abs(sc->evdev, ABS_MT_SLOT, finger);
			evdev_push_abs(sc->evdev, ABS_MT_TRACKING_ID, -1);
		}
	}

	evdev_sync(sc->evdev);
}

static int32_t
ietp_res2dpmm(uint8_t res, bool hi_precision)
{
	int32_t dpi;

	dpi = hi_precision ? 300 + res * 100 : 790 + res * 10;

	return (dpi * 10 /254);
}

static void
ietp_iic_identify(driver_t *driver, device_t parent)
{
	device_t iichid = device_get_parent(parent);
	static const uint16_t reg = IETP_PATTERN;
	uint16_t addr = iicbus_get_addr(iichid) << 1;
	uint8_t resp[2];
	uint8_t cmd[2] = { reg & 0xff, (reg >> 8) & 0xff };
	struct iic_msg msgs[2] = {
	    { addr, IIC_M_WR | IIC_M_NOSTOP,  sizeof(cmd), cmd },
	    { addr, IIC_M_RD, sizeof(resp), resp },
	};
	struct iic_rdwr_data ird = { msgs, nitems(msgs) };
	uint8_t pattern;

	if (HIDBUS_LOOKUP_ID(parent, ietp_iic_devs) == NULL)
		return;

	if (device_get_devclass(iichid) != devclass_find("iichid"))
		return;

	DPRINTF("Read reg 0x%04x with size %zu\n", reg, sizeof(resp));

	if (hid_ioctl(parent, I2CRDWR, (uintptr_t)&ird) != 0)
		return;

	DPRINTF("Response: %*D\n", (int)size(resp), resp, " ");

	pattern = (resp[0] == 0xFF && resp[1] == 0xFF) ? 0 : resp[1];
	if (pattern >= 0x02)
		hid_set_report_descr(parent, ietp_dummy_rdesc_hi,
		    sizeof(ietp_dummy_rdesc_hi));
	else
		hid_set_report_descr(parent, ietp_dummy_rdesc_lo,
		    sizeof(ietp_dummy_rdesc_lo));
}

static int
ietp_iic_probe(device_t dev)
{
	struct ietp_softc *sc = device_get_softc(dev);
	device_t iichid;
	int error;

	error = HIDBUS_LOOKUP_DRIVER_INFO(dev, ietp_iic_devs);
	if (error != 0)
		return (error);

	iichid = device_get_parent(device_get_parent(dev));
	if (device_get_devclass(iichid) != devclass_find("iichid"))
		return (ENXIO);

	sc->dev = dev;

	return (ietp_probe(sc));
}

static int
ietp_iic_attach(device_t dev)
{
	struct ietp_softc *sc = device_get_softc(dev);
	uint16_t buf, reg;
	uint8_t *buf8;
	uint8_t pattern;

	buf8 = (uint8_t *)&buf;

	if (ietp_iic_read_reg(dev, IETP_UNIQUEID, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading product ID\n");
		return (EIO);
	}
	sc->product_id = le16toh(buf);

	if (ietp_iic_read_reg(dev, IETP_PATTERN, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading pattern\n");
		return (EIO);
	}
	pattern = buf == 0xFFFF ? 0 : buf8[1];
	sc->hi_precision = pattern >= 0x02;

	reg = pattern >= 0x01 ? IETP_IC_TYPE : IETP_OSM_VERSION;
	if (ietp_iic_read_reg(dev, reg, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading IC type\n");
		return (EIO);
	}
	sc->ic_type = pattern >= 0x01 ? be16toh(buf) : buf8[1];

	if (ietp_iic_read_reg(dev, IETP_NSM_VERSION, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading SM version\n");
		return (EIO);
	}
	sc->is_clickpad = (buf8[0] & 0x10) != 0;

	if (ietp_iic_set_absolute_mode(dev, true) != 0) {
		device_printf(sc->dev, "failed to set absolute mode\n");
		return (EIO);
	}

	if (ietp_iic_read_reg(dev, IETP_MAX_X_AXIS, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading max x\n");
		return (EIO);
	}
	sc->max_x = le16toh(buf);

	if (ietp_iic_read_reg(dev, IETP_MAX_Y_AXIS, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading max y\n");
		return (EIO);
	}
	sc->max_y = le16toh(buf);

	if (ietp_iic_read_reg(dev, IETP_TRACENUM, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading trace info\n");
		return (EIO);
	}
	sc->trace_x = sc->max_x / buf8[0];
	sc->trace_y = sc->max_y / buf8[1];

	if (ietp_iic_read_reg(dev, IETP_PRESSURE, sizeof(buf), &buf) != 0) {
		device_printf(sc->dev, "failed reading pressure format\n");
		return (EIO);
	}
	sc->pressure_base = (buf8[0] & 0x10) ? 0 : IETP_PRESSURE_BASE;

	if (ietp_iic_read_reg(dev, IETP_RESOLUTION, sizeof(buf), &buf)  != 0) {
		device_printf(sc->dev, "failed reading resolution\n");
		return (EIO);
	}
	/* Conversion from internal format to dot per mm */
	sc->res_x = ietp_res2dpmm(buf8[0], sc->hi_precision);
	sc->res_y = ietp_res2dpmm(buf8[1], sc->hi_precision);

	return (ietp_attach(sc));
}

static int
ietp_iic_detach(device_t dev)
{
	struct ietp_softc *sc = device_get_softc(dev);

	if (ietp_iic_set_absolute_mode(dev, false) != 0)
		device_printf(dev, "failed setting standard mode\n");

	return (ietp_detach(sc));
}

static int
ietp_iic_resume(device_t dev)
{
	if (ietp_iic_set_absolute_mode(dev, true) != 0) {
		device_printf(dev, "reset when resuming failed: \n");
		return (EIO);
	}

	return (0);
}

static int
ietp_iic_set_absolute_mode(device_t dev, bool enable)
{
	struct ietp_softc *sc = device_get_softc(dev);
	static const struct {
		uint16_t	ic_type;
		uint16_t	product_id;
	} special_fw[] = {
	    { 0x0E, 0x05 }, { 0x0E, 0x06 }, { 0x0E, 0x07 }, { 0x0E, 0x09 },
	    { 0x0E, 0x13 }, { 0x08, 0x26 },
	};
	uint16_t val;
	int i, error;
	bool require_wakeup;

	error = 0;

	/*
	 * Some ASUS touchpads need to be powered on to enter absolute mode.
	 */
	require_wakeup = false;
	if (!sc->open) {
		for (i = 0; i < nitems(special_fw); i++) {
			if (sc->ic_type == special_fw[i].ic_type &&
			    sc->product_id == special_fw[i].product_id) {
				require_wakeup = true;
				break;
			}
		}
	}

	if (require_wakeup && hid_intr_start(dev) != 0) {
		device_printf(dev, "failed writing poweron command\n");
		return (EIO);
	}

	val = enable ? IETP_CTRL_ABSOLUTE : IETP_CTRL_STANDARD;
	if (ietp_iic_write_reg(dev, IETP_CONTROL, val) != 0) {
		device_printf(dev, "failed setting absolute mode\n");
		error = EIO;
	}

	if (require_wakeup && hid_intr_stop(dev) != 0) {
		device_printf(dev, "failed writing poweroff command\n");
		error = EIO;
	}

	return (error);
}

static int
ietp_iic_read_reg(device_t dev, uint16_t reg, size_t len, void *val)
{
	device_t iichid = device_get_parent(device_get_parent(dev));
	uint16_t addr = iicbus_get_addr(iichid) << 1;
	uint8_t cmd[2] = { reg & 0xff, (reg >> 8) & 0xff };
	struct iic_msg msgs[2] = {
	    { addr, IIC_M_WR | IIC_M_NOSTOP,  sizeof(cmd), cmd },
	    { addr, IIC_M_RD, len, val },
	};
	struct iic_rdwr_data ird = { msgs, nitems(msgs) };
	int error;

	DPRINTF("Read reg 0x%04x with size %zu\n", reg, len);

	error = hid_ioctl(dev, I2CRDWR, (uintptr_t)&ird);
	if (error != 0)
		return (error);

	DPRINTF("Response: %*D\n", (int)len, val, " ");

	return (0);
}

static int
ietp_iic_write_reg(device_t dev, uint16_t reg, uint16_t val)
{
	device_t iichid = device_get_parent(device_get_parent(dev));
	uint16_t addr = iicbus_get_addr(iichid) << 1;
	uint8_t cmd[4] = { reg & 0xff, (reg >> 8) & 0xff,
			   val & 0xff, (val >> 8) & 0xff };
	struct iic_msg msgs[1] = {
	    { addr, IIC_M_WR, sizeof(cmd), cmd },
	};
	struct iic_rdwr_data ird = { msgs, nitems(msgs) };

	DPRINTF("Write reg 0x%04x with value 0x%04x\n", reg, val);

	return (hid_ioctl(dev, I2CRDWR, (uintptr_t)&ird));
}

static device_method_t ietp_methods[] = {
	DEVMETHOD(device_identify,	ietp_iic_identify),
	DEVMETHOD(device_probe,		ietp_iic_probe),
	DEVMETHOD(device_attach,	ietp_iic_attach),
	DEVMETHOD(device_detach,	ietp_iic_detach),
	DEVMETHOD(device_resume,	ietp_iic_resume),
	DEVMETHOD_END
};

static driver_t ietp_driver = {
	.name = "ietp",
	.methods = ietp_methods,
	.size = sizeof(struct ietp_softc),
};

DRIVER_MODULE(ietp, hidbus, ietp_driver, NULL, NULL);
MODULE_DEPEND(ietp, hidbus, 1, 1, 1);
MODULE_DEPEND(ietp, hid, 1, 1, 1);
MODULE_DEPEND(ietp, evdev, 1, 1, 1);
MODULE_VERSION(ietp, 1);
HID_PNP_INFO(ietp_iic_devs);
