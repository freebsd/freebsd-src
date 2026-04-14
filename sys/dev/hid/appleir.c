/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 */

/*
 * Apple IR Remote Control Driver
 *
 * HID driver for Apple IR receivers (USB HID, vendor 0x05ac).
 * Supports Apple Remote and generic IR remotes using NEC protocol.
 *
 * The Apple Remote protocol was reverse-engineered by James McKenzie and
 * others; key codes and packet format constants are derived from that work
 * and are factual descriptions of the hardware protocol, not copied code.
 * Linux reference (GPL-2.0, no code copied): drivers/hid/hid-appleir.c
 *
 * Apple Remote Protocol (proprietary):
 *   Key down:    [0x25][0x87][0xee][remote_id][key_code]
 *   Key repeat:  [0x26][0x87][0xee][remote_id][key_code]
 *   Battery low: [0x25][0x87][0xe0][remote_id][0x00]
 *   Key decode:  (byte4 >> 1) & 0x0F -> keymap[index]
 *   Two-packet:  bit 6 of key_code (0x40) set -> store index, use on next keydown
 *
 * Generic IR Protocol (NEC-style):
 *   Format:     [0x26][0x7f][0x80][code][~code]
 *   Checksum:   code + ~code = 0xFF
 *
 * NO hardware key-up events -- synthesize via 125ms callout timer.
 */

#include <sys/cdefs.h>

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define HID_DEBUG_VAR	appleir_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include "usbdevs.h"

#ifdef HID_DEBUG
static int appleir_debug = 0;

static SYSCTL_NODE(_hw_hid, OID_AUTO, appleir, CTLFLAG_RW, 0,
    "Apple IR Remote Control");
SYSCTL_INT(_hw_hid_appleir, OID_AUTO, debug, CTLFLAG_RWTUN,
    &appleir_debug, 0, "Debug level");
#endif

/* Protocol constants */
#define	APPLEIR_REPORT_LEN	5
#define	APPLEIR_KEY_MASK	0x0F
#define	APPLEIR_TWO_PKT_FLAG	0x40	/* bit 6: two-packet command */
#define	APPLEIR_KEYUP_TICKS	MAX(1, hz / 8)	/* 125ms */
#define	APPLEIR_TWOPKT_TICKS	MAX(1, hz / 4)	/* 250ms */

/* Report type markers (byte 0) */
#define	APPLEIR_PKT_KEYDOWN	0x25	/* key down / battery low */
#define	APPLEIR_PKT_REPEAT	0x26	/* key repeat / NEC generic */

/* Apple Remote signature (bytes 1-2) */
#define	APPLEIR_SIG_HI		0x87
#define	APPLEIR_SIG_KEYLO	0xee	/* normal key event */
#define	APPLEIR_SIG_BATTLO	0xe0	/* battery low event */

/* Generic IR NEC signature (bytes 1-2) */
#define	APPLEIR_NEC_HI		0x7f
#define	APPLEIR_NEC_LO		0x80
#define	APPLEIR_NEC_CHECKSUM	0xFF	/* code + ~code must equal this */

/*
 * Apple IR keymap: 17 entries, index = (key_code >> 1) & 0x0F
 * Based on Linux driver (hid-appleir.c) keymap.
 */
static const uint16_t appleir_keymap[] = {
	KEY_RESERVED,		/* 0x00 */
	KEY_MENU,		/* 0x01 - menu */
	KEY_PLAYPAUSE,		/* 0x02 - play/pause */
	KEY_FORWARD,		/* 0x03 - >> */
	KEY_BACK,		/* 0x04 - << */
	KEY_VOLUMEUP,		/* 0x05 - + */
	KEY_VOLUMEDOWN,		/* 0x06 - - */
	KEY_RESERVED,		/* 0x07 */
	KEY_RESERVED,		/* 0x08 */
	KEY_RESERVED,		/* 0x09 */
	KEY_RESERVED,		/* 0x0A */
	KEY_RESERVED,		/* 0x0B */
	KEY_RESERVED,		/* 0x0C */
	KEY_RESERVED,		/* 0x0D */
	KEY_ENTER,		/* 0x0E - middle button (two-packet) */
	KEY_PLAYPAUSE,		/* 0x0F - play/pause (two-packet) */
	KEY_RESERVED,		/* 0x10 - out of range guard */
};
#define APPLEIR_NKEYS	(nitems(appleir_keymap))

/*
 * Generic IR keymap (NEC protocol codes).
 * Maps raw NEC codes to evdev KEY_* codes.
 */
struct generic_ir_map {
	uint8_t		code;		/* NEC IR code */
	uint16_t	key;		/* evdev KEY_* */
};

static const struct generic_ir_map generic_keymap[] = {
	{ 0xe1, KEY_VOLUMEUP },
	{ 0xe9, KEY_VOLUMEDOWN },
	{ 0xed, KEY_CHANNELUP },
	{ 0xf3, KEY_CHANNELDOWN },
	{ 0xf5, KEY_PLAYPAUSE },
	{ 0xf9, KEY_POWER },
	{ 0xfb, KEY_MUTE },
	{ 0xfe, KEY_OK },
};
#define GENERIC_NKEYS	(nitems(generic_keymap))

static uint16_t
generic_ir_lookup(uint8_t code)
{
	int i;

	for (i = 0; i < GENERIC_NKEYS; i++) {
		if (generic_keymap[i].code == code)
			return (generic_keymap[i].key);
	}
	return (KEY_RESERVED);
}

struct appleir_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;		/* protects below + callout */
	struct evdev_dev	*sc_evdev;
	struct callout		sc_co;		/* key-up timer */
	struct callout		sc_twoco;	/* two-packet timeout */
	uint16_t		sc_current_key;	/* evdev keycode (0=none) */
	int			sc_prev_key_idx;/* two-packet state (0=none) */
	bool			sc_batt_warned;
};


/*
 * Callout: synthesize key-up event (no hardware key-up from remote).
 * Runs with sc_mtx held (callout_init_mtx).
 */
static void
appleir_keyup(void *arg)
{
	struct appleir_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->sc_current_key != 0) {
		evdev_push_key(sc->sc_evdev, sc->sc_current_key, 0);
		evdev_sync(sc->sc_evdev);
		sc->sc_current_key = 0;
		sc->sc_prev_key_idx = 0;
	}
}

static void
appleir_twopacket_timeout(void *arg)
{
	struct appleir_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	sc->sc_prev_key_idx = 0;
}

/*
 * Process 5-byte HID interrupt report.
 * Called from hidbus interrupt context.
 */
static void
appleir_intr(void *context, void *data, hid_size_t len)
{
	struct appleir_softc *sc = context;
	uint8_t *buf = data;
	uint8_t report[APPLEIR_REPORT_LEN];
	int index;
	uint16_t new_key;

	if (len != APPLEIR_REPORT_LEN) {
		DPRINTFN(1, "bad report len: %zu\n", (size_t)len);
		return;
	}

	memcpy(report, buf, APPLEIR_REPORT_LEN);

	mtx_lock(&sc->sc_mtx);

	/* Battery low: [KEYDOWN][SIG_HI][SIG_BATTLO] -- log and ignore */
	if (report[0] == APPLEIR_PKT_KEYDOWN &&
	    report[1] == APPLEIR_SIG_HI && report[2] == APPLEIR_SIG_BATTLO) {
		if (!sc->sc_batt_warned) {
			device_printf(sc->sc_dev,
			    "remote battery may be low\n");
			sc->sc_batt_warned = true;
		}
		goto done;
	}

	/* Key down: [KEYDOWN][SIG_HI][SIG_KEYLO][remote_id][key_code] */
	if (report[0] == APPLEIR_PKT_KEYDOWN &&
	    report[1] == APPLEIR_SIG_HI && report[2] == APPLEIR_SIG_KEYLO) {
		/* Release previous key if held */
		if (sc->sc_current_key != 0) {
			evdev_push_key(sc->sc_evdev, sc->sc_current_key, 0);
			evdev_sync(sc->sc_evdev);
			sc->sc_current_key = 0;
		}

		if (sc->sc_prev_key_idx > 0) {
			/* Second packet of a two-packet command */
			index = sc->sc_prev_key_idx;
			sc->sc_prev_key_idx = 0;
			callout_stop(&sc->sc_twoco);
		} else if (report[4] & APPLEIR_TWO_PKT_FLAG) {
			/* First packet of a two-packet command -- wait for next */
			sc->sc_prev_key_idx = (report[4] >> 1) & APPLEIR_KEY_MASK;
			callout_reset(&sc->sc_twoco, APPLEIR_TWOPKT_TICKS,
			    appleir_twopacket_timeout, sc);
			goto done;
		} else {
			index = (report[4] >> 1) & APPLEIR_KEY_MASK;
		}

		new_key = (index < APPLEIR_NKEYS) ?
		    appleir_keymap[index] : KEY_RESERVED;
		if (new_key != KEY_RESERVED) {
			sc->sc_current_key = new_key;
			evdev_push_key(sc->sc_evdev, new_key, 1);
			evdev_sync(sc->sc_evdev);
			callout_reset(&sc->sc_co, APPLEIR_KEYUP_TICKS,
			    appleir_keyup, sc);
		}
		goto done;
	}

	/* Key repeat: [REPEAT][SIG_HI][SIG_KEYLO][remote_id][key_code] */
	if (report[0] == APPLEIR_PKT_REPEAT &&
	    report[1] == APPLEIR_SIG_HI && report[2] == APPLEIR_SIG_KEYLO) {
		uint16_t repeat_key;
		int repeat_idx;

		if (sc->sc_prev_key_idx > 0)
			goto done;
		if (report[4] & APPLEIR_TWO_PKT_FLAG)
			goto done;

		repeat_idx = (report[4] >> 1) & APPLEIR_KEY_MASK;
		repeat_key = (repeat_idx < APPLEIR_NKEYS) ?
		    appleir_keymap[repeat_idx] : KEY_RESERVED;
		if (repeat_key == KEY_RESERVED ||
		    repeat_key != sc->sc_current_key)
			goto done;

		evdev_push_key(sc->sc_evdev, repeat_key, 1);
		evdev_sync(sc->sc_evdev);
		callout_reset(&sc->sc_co, APPLEIR_KEYUP_TICKS,
		    appleir_keyup, sc);
		goto done;
	}

	/* Generic IR (NEC protocol): [REPEAT][NEC_HI][NEC_LO][code][~code] */
	if (report[0] == APPLEIR_PKT_REPEAT &&
	    report[1] == APPLEIR_NEC_HI && report[2] == APPLEIR_NEC_LO) {
		uint8_t code = report[3];
		uint8_t checksum = report[4];

		sc->sc_prev_key_idx = 0;
		callout_stop(&sc->sc_twoco);

		if ((uint8_t)(code + checksum) != APPLEIR_NEC_CHECKSUM) {
			DPRINTFN(1, "generic IR: bad checksum %02x+%02x\n",
			    code, checksum);
			goto done;
		}

		new_key = generic_ir_lookup(code);
		if (new_key == KEY_RESERVED)
			goto done;

		if (sc->sc_current_key != new_key) {
			if (sc->sc_current_key != 0)
				evdev_push_key(sc->sc_evdev,
				    sc->sc_current_key, 0);
			sc->sc_current_key = new_key;
			evdev_push_key(sc->sc_evdev, new_key, 1);
			evdev_sync(sc->sc_evdev);
		} else {
			evdev_push_key(sc->sc_evdev, new_key, 1);
			evdev_sync(sc->sc_evdev);
		}
		callout_reset(&sc->sc_co, APPLEIR_KEYUP_TICKS,
		    appleir_keyup, sc);
		goto done;
	}

	DPRINTFN(1, "unknown report: %02x %02x %02x\n",
	    report[0], report[1], report[2]);

done:
	mtx_unlock(&sc->sc_mtx);
}

/* Apple IR receiver device IDs */
static const struct hid_device_id appleir_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_APPLE, 0x8240) },
	{ HID_BVP(BUS_USB, USB_VENDOR_APPLE, 0x8241) },
	{ HID_BVP(BUS_USB, USB_VENDOR_APPLE, 0x8242) },
	{ HID_BVP(BUS_USB, USB_VENDOR_APPLE, 0x8243) },
	{ HID_BVP(BUS_USB, USB_VENDOR_APPLE, 0x1440) },
};

static int
appleir_probe(device_t dev)
{
	int error;

	error = HIDBUS_LOOKUP_DRIVER_INFO(dev, appleir_devs);
	if (error != 0)
		return (error);

	/* Only attach to first top-level collection (TLC index 0) */
	if (hidbus_get_index(dev) != 0)
		return (ENXIO);

	hidbus_set_desc(dev, "Apple IR Receiver");
	return (BUS_PROBE_DEFAULT);
}

static int
appleir_attach(device_t dev)
{
	struct appleir_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw;
	int i, error;

	sc->sc_dev = dev;
	hw = hid_get_device_info(dev);
	sc->sc_current_key = 0;
	sc->sc_prev_key_idx = 0;
	sc->sc_batt_warned = false;
	mtx_init(&sc->sc_mtx, "appleir", NULL, MTX_DEF);
	callout_init_mtx(&sc->sc_co, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_twoco, &sc->sc_mtx, 0);

	sc->sc_evdev = evdev_alloc();
	evdev_set_name(sc->sc_evdev, device_get_desc(dev));
	evdev_set_phys(sc->sc_evdev, device_get_nameunit(dev));
	evdev_set_id(sc->sc_evdev, hw->idBus, hw->idVendor, hw->idProduct,
	    hw->idVersion);
	evdev_set_serial(sc->sc_evdev, hw->serial);
	evdev_support_event(sc->sc_evdev, EV_SYN);
	evdev_support_event(sc->sc_evdev, EV_KEY);
	evdev_support_event(sc->sc_evdev, EV_REP);

	for (i = 0; i < APPLEIR_NKEYS; i++) {
		if (appleir_keymap[i] != KEY_RESERVED)
			evdev_support_key(sc->sc_evdev, appleir_keymap[i]);
	}
	for (i = 0; i < GENERIC_NKEYS; i++)
		evdev_support_key(sc->sc_evdev, generic_keymap[i].key);

	error = evdev_register_mtx(sc->sc_evdev, &sc->sc_mtx);
	if (error != 0) {
		device_printf(dev, "evdev_register_mtx failed: %d\n", error);
		goto fail;
	}

	hidbus_set_intr(dev, appleir_intr, sc);

	error = hid_intr_start(dev);
	if (error != 0) {
		device_printf(dev, "hid_intr_start failed: %d\n", error);
		goto fail;
	}

	return (0);

fail:
	if (sc->sc_evdev != NULL)
		evdev_free(sc->sc_evdev);
	callout_drain(&sc->sc_co);
	callout_drain(&sc->sc_twoco);
	mtx_destroy(&sc->sc_mtx);
	return (error);
}

static int
appleir_detach(device_t dev)
{
	struct appleir_softc *sc = device_get_softc(dev);
	int error;

	error = hid_intr_stop(dev);
	if (error != 0) {
		device_printf(dev, "hid_intr_stop failed: %d\n", error);
		return (error);
	}
	callout_drain(&sc->sc_co);
	callout_drain(&sc->sc_twoco);
	evdev_free(sc->sc_evdev);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static device_method_t appleir_methods[] = {
	DEVMETHOD(device_probe,		appleir_probe),
	DEVMETHOD(device_attach,	appleir_attach),
	DEVMETHOD(device_detach,	appleir_detach),
	DEVMETHOD_END
};

static driver_t appleir_driver = {
	"appleir",
	appleir_methods,
	sizeof(struct appleir_softc)
};

DRIVER_MODULE(appleir, hidbus, appleir_driver, NULL, NULL);
MODULE_DEPEND(appleir, hid, 1, 1, 1);
MODULE_DEPEND(appleir, hidbus, 1, 1, 1);
MODULE_DEPEND(appleir, evdev, 1, 1, 1);
MODULE_VERSION(appleir, 1);
HID_PNP_INFO(appleir_devs);
