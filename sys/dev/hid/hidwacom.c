/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Wacom ExpressKey Remote Driver
 *
 * HID driver for Wacom ExpressKey Remote (USB HID, vendor 0x056a).
 * The device uses a vendor-specific HID usage page (0xFF0C) with
 * proprietary report formats.  Only a single remote is supported;
 * multi-remote pairing (up to 5) is not implemented.
 *
 * Protocol decoded from USB traffic capture (HydraUSB3 analyser).
 * Report structure cross-referenced with the HID report descriptor
 * and publicly available Wacom protocol documentation.
 *
 *   Report ID 0x10 (DEVICE_LIST): pairing status (ignored)
 *   Report ID 0x11 (REMOTE): button and touch ring events
 *     buf[0]:     report ID (0x11)
 *     buf[3..5]:  serial number (LE 24-bit)
 *     buf[7]:     battery (bit 7 = charging, bits 0-6 = percent 0-100)
 *     buf[9]:     buttons 0-7
 *     buf[10]:    buttons 8-15
 *     buf[11]:    bits 0-1 = buttons 16-17, bits 6-7 = touch ring mode
 *     buf[12]:    touch ring (bit 7 = active, bits 0-6 = position 1-72,
 *                 where raw value 1 maps to position 0 and 72 maps to 71)
 */

#include <sys/cdefs.h>

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define HID_DEBUG_VAR	hidwacom_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include "usbdevs.h"

#ifdef HID_DEBUG
static int hidwacom_debug = 0;

static SYSCTL_NODE(_hw_hid, OID_AUTO, hidwacom, CTLFLAG_RW, 0,
    "Wacom ExpressKey Remote");
SYSCTL_INT(_hw_hid_hidwacom, OID_AUTO, debug, CTLFLAG_RWTUN,
    &hidwacom_debug, 0, "Debug level");
#endif

/* Report IDs */
#define	WACOM_REPORT_DEVICE_LIST	0x10
#define	WACOM_REPORT_REMOTE		0x11

/* Minimum report length (report ID + 12 data bytes for ring at buf[12]) */
#define	WACOM_REMOTE_MIN_LEN		13

/* Touch ring modes (buf[11] bits 6-7) */
#define	WACOM_RING_MODE_MASK		0xc0
#define	WACOM_RING_MODE_SHIFT		6

/* Touch ring */
#define	WACOM_RING_ACTIVE		0x80
#define	WACOM_RING_POSITION_MASK	0x7f
#define	WACOM_RING_MAX			71

/* Battery: bit 7 = charging flag, bits 0-6 = percent */
#define	WACOM_BATTERY_CHARGING		0x80
#define	WACOM_BATTERY_PERCENT_MASK	0x7f

/* Pad device ID for ABS_MISC activity marker (matches Linux PAD_DEVICE_ID) */
#define	WACOM_PAD_DEVICE_ID		0x0f

#define	HIDWACOM_DEV_NAME		"Wacom ExpressKey Remote"

/* Button evdev keycodes */
static const uint16_t hidwacom_buttons[] = {
	BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7,  /* buf[9] */
	BTN_8, BTN_9, BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z,  /* buf[10] */
	BTN_BASE, BTN_BASE2,				/* buf[11] */
};

struct hidwacom_softc {
	struct mtx		sc_mtx;
	struct evdev_dev	*sc_evdev;
};

static void
hidwacom_intr(void *context, void *data, hid_size_t len)
{
	struct hidwacom_softc *sc = context;
	uint8_t *buf = data;
	uint8_t report_id;
	uint8_t b9, b10, b11, ring, battery;
	uint32_t serial;
	int i, pos;

	if (len < 1)
		return;

	report_id = buf[0];

	if (report_id == WACOM_REPORT_DEVICE_LIST) {
		DPRINTFN(2, "device list report (pairing)\n");
		return;
	}

	if (report_id != WACOM_REPORT_REMOTE) {
		DPRINTFN(1, "unknown report id: 0x%02x\n", report_id);
		return;
	}

	if (len < WACOM_REMOTE_MIN_LEN) {
		DPRINTFN(1, "short remote report: %zu\n", (size_t)len);
		return;
	}

	mtx_lock(&sc->sc_mtx);

	/* Serial number (LE 24-bit) */
	serial = buf[3] + ((uint32_t)buf[4] << 8) + ((uint32_t)buf[5] << 16);

	/* Battery: percent in bits 0-6, charging flag in bit 7 */
	battery = buf[7] & WACOM_BATTERY_PERCENT_MASK;

	/* Extract button bytes */
	b9 = buf[9];
	b10 = buf[10];
	b11 = buf[11];

	/* Buttons 0-7 from buf[9] */
	for (i = 0; i < 8; i++)
		evdev_push_key(sc->sc_evdev, hidwacom_buttons[i],
		    (b9 >> i) & 1);

	/* Buttons 8-15 from buf[10] */
	for (i = 0; i < 8; i++)
		evdev_push_key(sc->sc_evdev, hidwacom_buttons[8 + i],
		    (b10 >> i) & 1);

	/* Buttons 16-17 from buf[11] bits 0-1 */
	evdev_push_key(sc->sc_evdev, BTN_BASE, b11 & 0x01);
	evdev_push_key(sc->sc_evdev, BTN_BASE2, b11 & 0x02);

	/* Touch ring: raw position is 1-based (1..72); subtract 1 for 0..71 */
	ring = buf[12];
	if (ring & WACOM_RING_ACTIVE) {
		pos = (ring & WACOM_RING_POSITION_MASK) - 1;
		if (pos < 0)
			pos = 0;
		if (pos > WACOM_RING_MAX)
			pos = WACOM_RING_MAX;
		evdev_push_abs(sc->sc_evdev, ABS_WHEEL, pos);
		evdev_push_abs(sc->sc_evdev, ABS_MISC,
		    (b11 >> WACOM_RING_MODE_SHIFT) & 0x03);
	} else {
		evdev_push_abs(sc->sc_evdev, ABS_WHEEL, 0);
		evdev_push_abs(sc->sc_evdev, ABS_MISC, 0);
	}

	/* Battery percent */
	evdev_push_event(sc->sc_evdev, EV_MSC, MSC_PULSELED, battery);

	/* Remote serial number */
	evdev_push_event(sc->sc_evdev, EV_MSC, MSC_SERIAL, serial);

	evdev_sync(sc->sc_evdev);

	mtx_unlock(&sc->sc_mtx);
}

static const struct hid_device_id hidwacom_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_WACOM,
	    USB_PRODUCT_WACOM_EXPRESSKEY_REMOTE) },
};

static int
hidwacom_probe(device_t dev)
{
	int error;

	error = HIDBUS_LOOKUP_DRIVER_INFO(dev, hidwacom_devs);
	if (error != 0)
		return (error);

	if (hidbus_get_index(dev) != 0)
		return (ENXIO);

	hidbus_set_desc(dev, HIDWACOM_DEV_NAME);
	return (BUS_PROBE_DEFAULT);
}

static int
hidwacom_attach(device_t dev)
{
	struct hidwacom_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw;
	int i, error;

	hw = hid_get_device_info(dev);
	mtx_init(&sc->sc_mtx, "hidwacom", NULL, MTX_DEF);

	sc->sc_evdev = evdev_alloc();
	evdev_set_name(sc->sc_evdev, HIDWACOM_DEV_NAME);
	evdev_set_phys(sc->sc_evdev, device_get_nameunit(dev));
	evdev_set_id(sc->sc_evdev, hw->idBus, hw->idVendor, hw->idProduct,
	    hw->idVersion);
	evdev_set_serial(sc->sc_evdev, hw->serial);

	evdev_support_event(sc->sc_evdev, EV_SYN);
	evdev_support_event(sc->sc_evdev, EV_KEY);
	evdev_support_event(sc->sc_evdev, EV_ABS);
	evdev_support_event(sc->sc_evdev, EV_MSC);

	for (i = 0; i < (int)nitems(hidwacom_buttons); i++)
		evdev_support_key(sc->sc_evdev, hidwacom_buttons[i]);

	evdev_support_abs(sc->sc_evdev, ABS_WHEEL, 0, WACOM_RING_MAX,
	    0, 0, 0);
	evdev_support_abs(sc->sc_evdev, ABS_MISC, 0, 3, 0, 0, 0);
	evdev_support_msc(sc->sc_evdev, MSC_SERIAL);
	evdev_support_msc(sc->sc_evdev, MSC_PULSELED);

	error = evdev_register_mtx(sc->sc_evdev, &sc->sc_mtx);
	if (error != 0) {
		device_printf(dev, "evdev_register_mtx failed: %d\n", error);
		goto fail;
	}

	hidbus_set_intr(dev, hidwacom_intr, sc);

	error = hid_intr_start(dev);
	if (error != 0) {
		device_printf(dev, "hid_intr_start failed: %d\n", error);
		hid_intr_stop(dev);
		goto fail;
	}

	return (0);

fail:
	if (sc->sc_evdev != NULL)
		evdev_free(sc->sc_evdev);
	mtx_destroy(&sc->sc_mtx);
	return (error);
}

static int
hidwacom_detach(device_t dev)
{
	struct hidwacom_softc *sc = device_get_softc(dev);
	int error;

	error = hid_intr_stop(dev);
	if (error != 0) {
		device_printf(dev, "hid_intr_stop failed: %d\n", error);
		return (error);
	}
	evdev_free(sc->sc_evdev);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static device_method_t hidwacom_methods[] = {
	DEVMETHOD(device_probe,		hidwacom_probe),
	DEVMETHOD(device_attach,	hidwacom_attach),
	DEVMETHOD(device_detach,	hidwacom_detach),
	DEVMETHOD_END
};

static driver_t hidwacom_driver = {
	"hidwacom",
	hidwacom_methods,
	sizeof(struct hidwacom_softc)
};

DRIVER_MODULE(hidwacom, hidbus, hidwacom_driver, NULL, NULL);
MODULE_DEPEND(hidwacom, hid, 1, 1, 1);
MODULE_DEPEND(hidwacom, hidbus, 1, 1, 1);
MODULE_DEPEND(hidwacom, evdev, 1, 1, 1);
MODULE_VERSION(hidwacom, 1);
HID_PNP_INFO(hidwacom_devs);
