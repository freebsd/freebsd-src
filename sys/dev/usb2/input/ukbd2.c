#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include "opt_compat.h"
#include "opt_kbd.h"
#include "opt_ukbd.h"

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_hid.h>

#define	USB_DEBUG_VAR ukbd_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_hid.h>

#include <dev/usb2/input/usb2_input.h>

#include <dev/usb2/quirk/usb2_quirk.h>

#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/tty.h>
#include <sys/kbio.h>

#include <dev/kbd/kbdreg.h>

/* the initial key map, accent map and fkey strings */
#if defined(UKBD_DFLT_KEYMAP) && !defined(KLD_MODULE)
#define	KBD_DFLT_KEYMAP
#include "ukbdmap.h"
#endif

/* the following file must be included after "ukbdmap.h" */
#include <dev/kbd/kbdtables.h>

#if USB_DEBUG
static int ukbd_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ukbd, CTLFLAG_RW, 0, "USB ukbd");
SYSCTL_INT(_hw_usb2_ukbd, OID_AUTO, debug, CTLFLAG_RW,
    &ukbd_debug, 0, "Debug level");
#endif

#define	UPROTO_BOOT_KEYBOARD 1

#define	UKBD_EMULATE_ATSCANCODE	       1
#define	UKBD_DRIVER_NAME          "ukbd"
#define	UKBD_NMOD                     8	/* units */
#define	UKBD_NKEYCODE                 6	/* units */
#define	UKBD_N_TRANSFER               3	/* units */
#define	UKBD_IN_BUF_SIZE  (2*(UKBD_NMOD + (2*UKBD_NKEYCODE)))	/* bytes */
#define	UKBD_IN_BUF_FULL  (UKBD_IN_BUF_SIZE / 2)	/* bytes */
#define	UKBD_NFKEY        (sizeof(fkey_tab)/sizeof(fkey_tab[0]))	/* units */

struct ukbd_data {
	uint8_t	modifiers;
#define	MOD_CONTROL_L	0x01
#define	MOD_CONTROL_R	0x10
#define	MOD_SHIFT_L	0x02
#define	MOD_SHIFT_R	0x20
#define	MOD_ALT_L	0x04
#define	MOD_ALT_R	0x40
#define	MOD_WIN_L	0x08
#define	MOD_WIN_R	0x80
	uint8_t	reserved;
	uint8_t	keycode[UKBD_NKEYCODE];
} __packed;

struct ukbd_softc {
	keyboard_t sc_kbd;
	keymap_t sc_keymap;
	accentmap_t sc_accmap;
	fkeytab_t sc_fkeymap[UKBD_NFKEY];
	struct usb2_callout sc_callout;
	struct ukbd_data sc_ndata;
	struct ukbd_data sc_odata;

	struct usb2_device *sc_udev;
	struct usb2_interface *sc_iface;
	struct usb2_xfer *sc_xfer[UKBD_N_TRANSFER];

	uint32_t sc_ntime[UKBD_NKEYCODE];
	uint32_t sc_otime[UKBD_NKEYCODE];
	uint32_t sc_input[UKBD_IN_BUF_SIZE];	/* input buffer */
	uint32_t sc_time_ms;
	uint32_t sc_composed_char;	/* composed char code, if non-zero */
#ifdef UKBD_EMULATE_ATSCANCODE
	uint32_t sc_buffered_char[2];
#endif
	uint32_t sc_flags;		/* flags */
#define	UKBD_FLAG_COMPOSE    0x0001
#define	UKBD_FLAG_POLLING    0x0002
#define	UKBD_FLAG_SET_LEDS   0x0004
#define	UKBD_FLAG_INTR_STALL 0x0008
#define	UKBD_FLAG_ATTACHED   0x0010
#define	UKBD_FLAG_GONE       0x0020

	int32_t	sc_mode;		/* input mode (K_XLATE,K_RAW,K_CODE) */
	int32_t	sc_state;		/* shift/lock key state */
	int32_t	sc_accents;		/* accent key index (> 0) */

	uint16_t sc_inputs;
	uint16_t sc_inputhead;
	uint16_t sc_inputtail;

	uint8_t	sc_leds;		/* store for async led requests */
	uint8_t	sc_iface_index;
	uint8_t	sc_iface_no;
};

#define	KEY_ERROR	  0x01

#define	KEY_PRESS	  0
#define	KEY_RELEASE	  0x400
#define	KEY_INDEX(c)	  ((c) & 0xFF)

#define	SCAN_PRESS	  0
#define	SCAN_RELEASE	  0x80
#define	SCAN_PREFIX_E0	  0x100
#define	SCAN_PREFIX_E1	  0x200
#define	SCAN_PREFIX_CTL	  0x400
#define	SCAN_PREFIX_SHIFT 0x800
#define	SCAN_PREFIX	(SCAN_PREFIX_E0  | SCAN_PREFIX_E1 | \
			 SCAN_PREFIX_CTL | SCAN_PREFIX_SHIFT)
#define	SCAN_CHAR(c)	((c) & 0x7f)

struct ukbd_mods {
	uint32_t mask, key;
};

static const struct ukbd_mods ukbd_mods[UKBD_NMOD] = {
	{MOD_CONTROL_L, 0xe0},
	{MOD_CONTROL_R, 0xe4},
	{MOD_SHIFT_L, 0xe1},
	{MOD_SHIFT_R, 0xe5},
	{MOD_ALT_L, 0xe2},
	{MOD_ALT_R, 0xe6},
	{MOD_WIN_L, 0xe3},
	{MOD_WIN_R, 0xe7},
};

#define	NN 0				/* no translation */
/*
 * Translate USB keycodes to AT keyboard scancodes.
 */
/*
 * FIXME: Mac USB keyboard generates:
 * 0x53: keypad NumLock/Clear
 * 0x66: Power
 * 0x67: keypad =
 * 0x68: F13
 * 0x69: F14
 * 0x6a: F15
 */
static const uint8_t ukbd_trtab[256] = {
	0, 0, 0, 0, 30, 48, 46, 32,	/* 00 - 07 */
	18, 33, 34, 35, 23, 36, 37, 38,	/* 08 - 0F */
	50, 49, 24, 25, 16, 19, 31, 20,	/* 10 - 17 */
	22, 47, 17, 45, 21, 44, 2, 3,	/* 18 - 1F */
	4, 5, 6, 7, 8, 9, 10, 11,	/* 20 - 27 */
	28, 1, 14, 15, 57, 12, 13, 26,	/* 28 - 2F */
	27, 43, 43, 39, 40, 41, 51, 52,	/* 30 - 37 */
	53, 58, 59, 60, 61, 62, 63, 64,	/* 38 - 3F */
	65, 66, 67, 68, 87, 88, 92, 70,	/* 40 - 47 */
	104, 102, 94, 96, 103, 99, 101, 98,	/* 48 - 4F */
	97, 100, 95, 69, 91, 55, 74, 78,/* 50 - 57 */
	89, 79, 80, 81, 75, 76, 77, 71,	/* 58 - 5F */
	72, 73, 82, 83, 86, 107, 122, NN,	/* 60 - 67 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* 68 - 6F */
	NN, NN, NN, NN, 115, 108, 111, 113,	/* 70 - 77 */
	109, 110, 112, 118, 114, 116, 117, 119,	/* 78 - 7F */
	121, 120, NN, NN, NN, NN, NN, 115,	/* 80 - 87 */
	112, 125, 121, 123, NN, NN, NN, NN,	/* 88 - 8F */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* 90 - 97 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* 98 - 9F */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* A0 - A7 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* A8 - AF */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* B0 - B7 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* B8 - BF */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* C0 - C7 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* C8 - CF */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* D0 - D7 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* D8 - DF */
	29, 42, 56, 105, 90, 54, 93, 106,	/* E0 - E7 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* E8 - EF */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* F0 - F7 */
	NN, NN, NN, NN, NN, NN, NN, NN,	/* F8 - FF */
};

/* prototypes */
static void ukbd_timeout(void *arg);
static void ukbd_set_leds(struct ukbd_softc *sc, uint8_t leds);
static int ukbd_set_typematic(keyboard_t *kbd, int code);

#ifdef UKBD_EMULATE_ATSCANCODE
static int
ukbd_key2scan(struct ukbd_softc *sc, int keycode,
    int shift, int up);

#endif
static uint32_t ukbd_read_char(keyboard_t *kbd, int wait);
static void ukbd_clear_state(keyboard_t *kbd);
static int ukbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t arg);
static int ukbd_enable(keyboard_t *kbd);
static int ukbd_disable(keyboard_t *kbd);
static void ukbd_interrupt(struct ukbd_softc *sc);

static device_probe_t ukbd_probe;
static device_attach_t ukbd_attach;
static device_detach_t ukbd_detach;
static device_resume_t ukbd_resume;

static void
ukbd_put_key(struct ukbd_softc *sc, uint32_t key)
{
	mtx_assert(&Giant, MA_OWNED);

	DPRINTF("0x%02x (%d) %s\n", key, key,
	    (key & KEY_RELEASE) ? "released" : "pressed");

	if (sc->sc_inputs < UKBD_IN_BUF_SIZE) {
		sc->sc_input[sc->sc_inputtail] = key;
		++(sc->sc_inputs);
		++(sc->sc_inputtail);
		if (sc->sc_inputtail >= UKBD_IN_BUF_SIZE) {
			sc->sc_inputtail = 0;
		}
	} else {
		DPRINTF("input buffer is full\n");
	}
	return;
}

static int32_t
ukbd_get_key(struct ukbd_softc *sc, uint8_t wait)
{
	int32_t c;

	mtx_assert(&Giant, MA_OWNED);

	if (sc->sc_inputs == 0) {
		/* start transfer, if not already started */
		usb2_transfer_start(sc->sc_xfer[0]);
	}
	if (sc->sc_flags & UKBD_FLAG_POLLING) {
		DPRINTFN(2, "polling\n");

		while (sc->sc_inputs == 0) {

			usb2_do_poll(sc->sc_xfer, UKBD_N_TRANSFER);

			DELAY(1000);	/* delay 1 ms */

			sc->sc_time_ms++;

			/* support repetition of keys: */

			ukbd_interrupt(sc);

			if (!wait) {
				break;
			}
		}
	}
	if (sc->sc_inputs == 0) {
		c = -1;
	} else {
		c = sc->sc_input[sc->sc_inputhead];
		--(sc->sc_inputs);
		++(sc->sc_inputhead);
		if (sc->sc_inputhead >= UKBD_IN_BUF_SIZE) {
			sc->sc_inputhead = 0;
		}
	}
	return (c);
}

static void
ukbd_interrupt(struct ukbd_softc *sc)
{
	uint32_t n_mod;
	uint32_t o_mod;
	uint32_t now = sc->sc_time_ms;
	uint32_t dtime;
	uint32_t c;
	uint8_t key;
	uint8_t i;
	uint8_t j;

	if (sc->sc_ndata.keycode[0] == KEY_ERROR) {
		goto done;
	}
	n_mod = sc->sc_ndata.modifiers;
	o_mod = sc->sc_odata.modifiers;
	if (n_mod != o_mod) {
		for (i = 0; i < UKBD_NMOD; i++) {
			if ((n_mod & ukbd_mods[i].mask) !=
			    (o_mod & ukbd_mods[i].mask)) {
				ukbd_put_key(sc, ukbd_mods[i].key |
				    ((n_mod & ukbd_mods[i].mask) ?
				    KEY_PRESS : KEY_RELEASE));
			}
		}
	}
	/* Check for released keys. */
	for (i = 0; i < UKBD_NKEYCODE; i++) {
		key = sc->sc_odata.keycode[i];
		if (key == 0) {
			continue;
		}
		for (j = 0; j < UKBD_NKEYCODE; j++) {
			if (sc->sc_ndata.keycode[j] == 0) {
				continue;
			}
			if (key == sc->sc_ndata.keycode[j]) {
				goto rfound;
			}
		}
		ukbd_put_key(sc, key | KEY_RELEASE);
rfound:	;
	}

	/* Check for pressed keys. */
	for (i = 0; i < UKBD_NKEYCODE; i++) {
		key = sc->sc_ndata.keycode[i];
		if (key == 0) {
			continue;
		}
		sc->sc_ntime[i] = now + sc->sc_kbd.kb_delay1;
		for (j = 0; j < UKBD_NKEYCODE; j++) {
			if (sc->sc_odata.keycode[j] == 0) {
				continue;
			}
			if (key == sc->sc_odata.keycode[j]) {

				/* key is still pressed */

				sc->sc_ntime[i] = sc->sc_otime[j];
				dtime = (sc->sc_otime[j] - now);

				if (!(dtime & 0x80000000)) {
					/* time has not elapsed */
					goto pfound;
				}
				sc->sc_ntime[i] = now + sc->sc_kbd.kb_delay2;
				break;
			}
		}
		ukbd_put_key(sc, key | KEY_PRESS);

		/*
                 * If any other key is presently down, force its repeat to be
                 * well in the future (100s).  This makes the last key to be
                 * pressed do the autorepeat.
                 */
		for (j = 0; j != UKBD_NKEYCODE; j++) {
			if (j != i)
				sc->sc_ntime[j] = now + (100 * 1000);
		}
pfound:	;
	}

	sc->sc_odata = sc->sc_ndata;

	bcopy(sc->sc_ntime, sc->sc_otime, sizeof(sc->sc_otime));

	if (sc->sc_inputs == 0) {
		goto done;
	}
	if (sc->sc_flags & UKBD_FLAG_POLLING) {
		goto done;
	}
	if (KBD_IS_ACTIVE(&sc->sc_kbd) &&
	    KBD_IS_BUSY(&sc->sc_kbd)) {
		/* let the callback function process the input */
		(sc->sc_kbd.kb_callback.kc_func) (&sc->sc_kbd, KBDIO_KEYINPUT,
		    sc->sc_kbd.kb_callback.kc_arg);
	} else {
		/* read and discard the input, no one is waiting for it */
		do {
			c = ukbd_read_char(&sc->sc_kbd, 0);
		} while (c != NOKEY);
	}
done:
	return;
}

static void
ukbd_timeout(void *arg)
{
	struct ukbd_softc *sc = arg;

	mtx_assert(&Giant, MA_OWNED);

	if (!(sc->sc_flags & UKBD_FLAG_POLLING)) {
		sc->sc_time_ms += 25;	/* milliseconds */
	}
	ukbd_interrupt(sc);

	usb2_callout_reset(&sc->sc_callout, hz / 40, &ukbd_timeout, sc);

	mtx_unlock(&Giant);

	return;
}

static void
ukbd_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ukbd_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UKBD_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ukbd_intr_callback(struct usb2_xfer *xfer)
{
	struct ukbd_softc *sc = xfer->priv_sc;
	uint16_t len = xfer->actlen;
	uint8_t i;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("actlen=%d bytes\n", len);

		if (len > sizeof(sc->sc_ndata)) {
			len = sizeof(sc->sc_ndata);
		}
		if (len) {
			bzero(&sc->sc_ndata, sizeof(sc->sc_ndata));
			usb2_copy_out(xfer->frbuffers, 0, &sc->sc_ndata, len);
#if USB_DEBUG
			if (sc->sc_ndata.modifiers) {
				DPRINTF("mod: 0x%04x\n", sc->sc_ndata.modifiers);
			}
			for (i = 0; i < UKBD_NKEYCODE; i++) {
				if (sc->sc_ndata.keycode[i]) {
					DPRINTF("[%d] = %d\n", i, sc->sc_ndata.keycode[i]);
				}
			}
#endif					/* USB_DEBUG */
			ukbd_interrupt(sc);
		}
	case USB_ST_SETUP:
		if (sc->sc_flags & UKBD_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[1]);
			return;
		}
		if (sc->sc_inputs < UKBD_IN_BUF_FULL) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		} else {
			DPRINTF("input queue is full!\n");
		}
		return;

	default:			/* Error */
		DPRINTF("error=%s\n", usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UKBD_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[1]);
		}
		return;
	}
}

static void
ukbd_set_leds_callback(struct usb2_xfer *xfer)
{
	struct usb2_device_request req;
	uint8_t buf[1];
	struct ukbd_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
		if (sc->sc_flags & UKBD_FLAG_SET_LEDS) {
			sc->sc_flags &= ~UKBD_FLAG_SET_LEDS;

			req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
			req.bRequest = UR_SET_REPORT;
			USETW2(req.wValue, UHID_OUTPUT_REPORT, 0);
			req.wIndex[0] = sc->sc_iface_no;
			req.wIndex[1] = 0;
			USETW(req.wLength, 1);

			buf[0] = sc->sc_leds;

			usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));
			usb2_copy_in(xfer->frbuffers + 1, 0, buf, sizeof(buf));

			xfer->frlengths[0] = sizeof(req);
			xfer->frlengths[1] = sizeof(buf);
			xfer->nframes = 2;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		DPRINTFN(0, "error=%s\n", usb2_errstr(xfer->error));
		return;
	}
}

static const struct usb2_config ukbd_config[UKBD_N_TRANSFER] = {

	[0] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &ukbd_intr_callback,
	},

	[1] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ukbd_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request) + 1,
		.mh.callback = &ukbd_set_leds_callback,
		.mh.timeout = 1000,	/* 1 second */
	},
};

static int
ukbd_probe(device_t dev)
{
	keyboard_switch_t *sw = kbd_get_switch(UKBD_DRIVER_NAME);
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	DPRINTFN(11, "\n");

	if (sw == NULL) {
		return (ENXIO);
	}
	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	/* check that the keyboard speaks the boot protocol: */
	if ((uaa->info.bInterfaceClass == UICLASS_HID)
	    && (uaa->info.bInterfaceSubClass == UISUBCLASS_BOOT)
	    && (uaa->info.bInterfaceProtocol == UPROTO_BOOT_KEYBOARD)) {
		if (usb2_test_quirk(uaa, UQ_KBD_IGNORE))
			return (ENXIO);
		else
			return (0);
	}
	return (ENXIO);
}

static int
ukbd_attach(device_t dev)
{
	struct ukbd_softc *sc = device_get_softc(dev);
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	int32_t unit = device_get_unit(dev);
	keyboard_t *kbd = &sc->sc_kbd;
	usb2_error_t err;
	uint16_t n;

	if (sc == NULL) {
		return (ENOMEM);
	}
	mtx_assert(&Giant, MA_OWNED);

	kbd_init_struct(kbd, UKBD_DRIVER_NAME, KB_OTHER, unit, 0, 0, 0);

	kbd->kb_data = (void *)sc;

	device_set_usb2_desc(dev);

	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;
	sc->sc_iface_index = uaa->info.bIfaceIndex;
	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_mode = K_XLATE;
	sc->sc_iface = uaa->iface;

	usb2_callout_init_mtx(&sc->sc_callout, &Giant,
	    CALLOUT_RETURNUNLOCKED);

	err = usb2_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, ukbd_config,
	    UKBD_N_TRANSFER, sc, &Giant);

	if (err) {
		DPRINTF("error=%s\n", usb2_errstr(err));
		goto detach;
	}
	/* setup default keyboard maps */

	sc->sc_keymap = key_map;
	sc->sc_accmap = accent_map;
	for (n = 0; n < UKBD_NFKEY; n++) {
		sc->sc_fkeymap[n] = fkey_tab[n];
	}

	kbd_set_maps(kbd, &sc->sc_keymap, &sc->sc_accmap,
	    sc->sc_fkeymap, UKBD_NFKEY);

	KBD_FOUND_DEVICE(kbd);

	ukbd_clear_state(kbd);

	/*
	 * FIXME: set the initial value for lock keys in "sc_state"
	 * according to the BIOS data?
	 */
	KBD_PROBE_DONE(kbd);

	/* ignore if SETIDLE fails, hence it is not crucial */
	err = usb2_req_set_idle(sc->sc_udev, &Giant, sc->sc_iface_index, 0, 0);

	ukbd_ioctl(kbd, KDSETLED, (caddr_t)&sc->sc_state);

	KBD_INIT_DONE(kbd);

	if (kbd_register(kbd) < 0) {
		goto detach;
	}
	KBD_CONFIG_DONE(kbd);

	ukbd_enable(kbd);

#ifdef KBD_INSTALL_CDEV
	if (kbd_attach(kbd)) {
		goto detach;
	}
#endif
	sc->sc_flags |= UKBD_FLAG_ATTACHED;

	if (bootverbose) {
		genkbd_diag(kbd, bootverbose);
	}
	/* lock keyboard mutex */

	mtx_lock(&Giant);

	/* start the keyboard */

	usb2_transfer_start(sc->sc_xfer[0]);

	/* start the timer */

	ukbd_timeout(sc);		/* will unlock mutex */

	return (0);			/* success */

detach:
	ukbd_detach(dev);
	return (ENXIO);			/* error */
}

int
ukbd_detach(device_t dev)
{
	struct ukbd_softc *sc = device_get_softc(dev);
	int error;

	mtx_assert(&Giant, MA_OWNED);

	DPRINTF("\n");

	if (sc->sc_flags & UKBD_FLAG_POLLING) {
		panic("cannot detach polled keyboard!\n");
	}
	sc->sc_flags |= UKBD_FLAG_GONE;

	usb2_callout_stop(&sc->sc_callout);

	ukbd_disable(&sc->sc_kbd);

#ifdef KBD_INSTALL_CDEV
	if (sc->sc_flags & UKBD_FLAG_ATTACHED) {
		error = kbd_detach(&sc->sc_kbd);
		if (error) {
			/* usb attach cannot return an error */
			device_printf(dev, "WARNING: kbd_detach() "
			    "returned non-zero! (ignored)\n");
		}
	}
#endif
	if (KBD_IS_CONFIGURED(&sc->sc_kbd)) {
		error = kbd_unregister(&sc->sc_kbd);
		if (error) {
			/* usb attach cannot return an error */
			device_printf(dev, "WARNING: kbd_unregister() "
			    "returned non-zero! (ignored)\n");
		}
	}
	sc->sc_kbd.kb_flags = 0;

	usb2_transfer_unsetup(sc->sc_xfer, UKBD_N_TRANSFER);

	usb2_callout_drain(&sc->sc_callout);

	DPRINTF("%s: disconnected\n",
	    device_get_nameunit(dev));

	return (0);
}

static int
ukbd_resume(device_t dev)
{
	struct ukbd_softc *sc = device_get_softc(dev);

	mtx_assert(&Giant, MA_OWNED);

	ukbd_clear_state(&sc->sc_kbd);

	return (0);
}

/* early keyboard probe, not supported */
static int
ukbd_configure(int flags)
{
	return (0);
}

/* detect a keyboard, not used */
static int
ukbd__probe(int unit, void *arg, int flags)
{
	mtx_assert(&Giant, MA_OWNED);
	return (ENXIO);
}

/* reset and initialize the device, not used */
static int
ukbd_init(int unit, keyboard_t **kbdp, void *arg, int flags)
{
	mtx_assert(&Giant, MA_OWNED);
	return (ENXIO);
}

/* test the interface to the device, not used */
static int
ukbd_test_if(keyboard_t *kbd)
{
	mtx_assert(&Giant, MA_OWNED);
	return (0);
}

/* finish using this keyboard, not used */
static int
ukbd_term(keyboard_t *kbd)
{
	mtx_assert(&Giant, MA_OWNED);
	return (ENXIO);
}

/* keyboard interrupt routine, not used */
static int
ukbd_intr(keyboard_t *kbd, void *arg)
{
	mtx_assert(&Giant, MA_OWNED);
	return (0);
}

/* lock the access to the keyboard, not used */
static int
ukbd_lock(keyboard_t *kbd, int lock)
{
	mtx_assert(&Giant, MA_OWNED);
	return (1);
}

/*
 * Enable the access to the device; until this function is called,
 * the client cannot read from the keyboard.
 */
static int
ukbd_enable(keyboard_t *kbd)
{
	mtx_assert(&Giant, MA_OWNED);
	KBD_ACTIVATE(kbd);
	return (0);
}

/* disallow the access to the device */
static int
ukbd_disable(keyboard_t *kbd)
{
	mtx_assert(&Giant, MA_OWNED);
	KBD_DEACTIVATE(kbd);
	return (0);
}

/* check if data is waiting */
static int
ukbd_check(keyboard_t *kbd)
{
	struct ukbd_softc *sc = kbd->kb_data;

	if (!mtx_owned(&Giant)) {
		return (0);		/* XXX */
	}
	mtx_assert(&Giant, MA_OWNED);

	if (!KBD_IS_ACTIVE(kbd)) {
		return (0);
	}
#ifdef UKBD_EMULATE_ATSCANCODE
	if (sc->sc_buffered_char[0]) {
		return (1);
	}
#endif
	if (sc->sc_inputs > 0) {
		return (1);
	}
	return (0);
}

/* check if char is waiting */
static int
ukbd_check_char(keyboard_t *kbd)
{
	struct ukbd_softc *sc = kbd->kb_data;

	if (!mtx_owned(&Giant)) {
		return (0);		/* XXX */
	}
	mtx_assert(&Giant, MA_OWNED);

	if (!KBD_IS_ACTIVE(kbd)) {
		return (0);
	}
	if ((sc->sc_composed_char > 0) &&
	    (!(sc->sc_flags & UKBD_FLAG_COMPOSE))) {
		return (1);
	}
	return (ukbd_check(kbd));
}


/* read one byte from the keyboard if it's allowed */
static int
ukbd_read(keyboard_t *kbd, int wait)
{
	struct ukbd_softc *sc = kbd->kb_data;
	int32_t usbcode;

#ifdef UKBD_EMULATE_ATSCANCODE
	uint32_t keycode;
	uint32_t scancode;

#endif

	if (!mtx_owned(&Giant)) {
		return -1;		/* XXX */
	}
	mtx_assert(&Giant, MA_OWNED);

#ifdef UKBD_EMULATE_ATSCANCODE
	if (sc->sc_buffered_char[0]) {
		scancode = sc->sc_buffered_char[0];
		if (scancode & SCAN_PREFIX) {
			sc->sc_buffered_char[0] &= ~SCAN_PREFIX;
			return ((scancode & SCAN_PREFIX_E0) ? 0xe0 : 0xe1);
		}
		sc->sc_buffered_char[0] = sc->sc_buffered_char[1];
		sc->sc_buffered_char[1] = 0;
		return (scancode);
	}
#endif					/* UKBD_EMULATE_ATSCANCODE */

	/* XXX */
	usbcode = ukbd_get_key(sc, (wait == FALSE) ? 0 : 1);
	if (!KBD_IS_ACTIVE(kbd) || (usbcode == -1)) {
		return -1;
	}
	++(kbd->kb_count);

#ifdef UKBD_EMULATE_ATSCANCODE
	keycode = ukbd_trtab[KEY_INDEX(usbcode)];
	if (keycode == NN) {
		return -1;
	}
	return (ukbd_key2scan(sc, keycode, sc->sc_ndata.modifiers,
	    (usbcode & KEY_RELEASE)));
#else					/* !UKBD_EMULATE_ATSCANCODE */
	return (usbcode);
#endif					/* UKBD_EMULATE_ATSCANCODE */
}

/* read char from the keyboard */
static uint32_t
ukbd_read_char(keyboard_t *kbd, int wait)
{
	struct ukbd_softc *sc = kbd->kb_data;
	uint32_t action;
	uint32_t keycode;
	int32_t usbcode;

#ifdef UKBD_EMULATE_ATSCANCODE
	uint32_t scancode;

#endif
	if (!mtx_owned(&Giant)) {
		return (NOKEY);		/* XXX */
	}
	mtx_assert(&Giant, MA_OWNED);

next_code:

	/* do we have a composed char to return ? */

	if ((sc->sc_composed_char > 0) &&
	    (!(sc->sc_flags & UKBD_FLAG_COMPOSE))) {

		action = sc->sc_composed_char;
		sc->sc_composed_char = 0;

		if (action > 0xFF) {
			goto errkey;
		}
		goto done;
	}
#ifdef UKBD_EMULATE_ATSCANCODE

	/* do we have a pending raw scan code? */

	if (sc->sc_mode == K_RAW) {
		scancode = sc->sc_buffered_char[0];
		if (scancode) {
			if (scancode & SCAN_PREFIX) {
				sc->sc_buffered_char[0] = (scancode & ~SCAN_PREFIX);
				return ((scancode & SCAN_PREFIX_E0) ? 0xe0 : 0xe1);
			}
			sc->sc_buffered_char[0] = sc->sc_buffered_char[1];
			sc->sc_buffered_char[1] = 0;
			return (scancode);
		}
	}
#endif					/* UKBD_EMULATE_ATSCANCODE */

	/* see if there is something in the keyboard port */
	/* XXX */
	usbcode = ukbd_get_key(sc, (wait == FALSE) ? 0 : 1);
	if (usbcode == -1) {
		return (NOKEY);
	}
	++kbd->kb_count;

#ifdef UKBD_EMULATE_ATSCANCODE
	/* USB key index -> key code -> AT scan code */
	keycode = ukbd_trtab[KEY_INDEX(usbcode)];
	if (keycode == NN) {
		return (NOKEY);
	}
	/* return an AT scan code for the K_RAW mode */
	if (sc->sc_mode == K_RAW) {
		return (ukbd_key2scan(sc, keycode, sc->sc_ndata.modifiers,
		    (usbcode & KEY_RELEASE)));
	}
#else					/* !UKBD_EMULATE_ATSCANCODE */

	/* return the byte as is for the K_RAW mode */
	if (sc->sc_mode == K_RAW) {
		return (usbcode);
	}
	/* USB key index -> key code */
	keycode = ukbd_trtab[KEY_INDEX(usbcode)];
	if (keycode == NN) {
		return (NOKEY);
	}
#endif					/* UKBD_EMULATE_ATSCANCODE */

	switch (keycode) {
	case 0x38:			/* left alt (compose key) */
		if (usbcode & KEY_RELEASE) {
			if (sc->sc_flags & UKBD_FLAG_COMPOSE) {
				sc->sc_flags &= ~UKBD_FLAG_COMPOSE;

				if (sc->sc_composed_char > 0xFF) {
					sc->sc_composed_char = 0;
				}
			}
		} else {
			if (!(sc->sc_flags & UKBD_FLAG_COMPOSE)) {
				sc->sc_flags |= UKBD_FLAG_COMPOSE;
				sc->sc_composed_char = 0;
			}
		}
		break;
		/* XXX: I don't like these... */
	case 0x5c:			/* print screen */
		if (sc->sc_flags & ALTS) {
			keycode = 0x54;	/* sysrq */
		}
		break;
	case 0x68:			/* pause/break */
		if (sc->sc_flags & CTLS) {
			keycode = 0x6c;	/* break */
		}
		break;
	}

	/* return the key code in the K_CODE mode */
	if (usbcode & KEY_RELEASE) {
		keycode |= SCAN_RELEASE;
	}
	if (sc->sc_mode == K_CODE) {
		return (keycode);
	}
	/* compose a character code */
	if (sc->sc_flags & UKBD_FLAG_COMPOSE) {
		switch (keycode) {
			/* key pressed, process it */
		case 0x47:
		case 0x48:
		case 0x49:		/* keypad 7,8,9 */
			sc->sc_composed_char *= 10;
			sc->sc_composed_char += keycode - 0x40;
			goto check_composed;

		case 0x4B:
		case 0x4C:
		case 0x4D:		/* keypad 4,5,6 */
			sc->sc_composed_char *= 10;
			sc->sc_composed_char += keycode - 0x47;
			goto check_composed;

		case 0x4F:
		case 0x50:
		case 0x51:		/* keypad 1,2,3 */
			sc->sc_composed_char *= 10;
			sc->sc_composed_char += keycode - 0x4E;
			goto check_composed;

		case 0x52:		/* keypad 0 */
			sc->sc_composed_char *= 10;
			goto check_composed;

			/* key released, no interest here */
		case SCAN_RELEASE | 0x47:
		case SCAN_RELEASE | 0x48:
		case SCAN_RELEASE | 0x49:	/* keypad 7,8,9 */
		case SCAN_RELEASE | 0x4B:
		case SCAN_RELEASE | 0x4C:
		case SCAN_RELEASE | 0x4D:	/* keypad 4,5,6 */
		case SCAN_RELEASE | 0x4F:
		case SCAN_RELEASE | 0x50:
		case SCAN_RELEASE | 0x51:	/* keypad 1,2,3 */
		case SCAN_RELEASE | 0x52:	/* keypad 0 */
			goto next_code;

		case 0x38:		/* left alt key */
			break;

		default:
			if (sc->sc_composed_char > 0) {
				sc->sc_flags &= ~UKBD_FLAG_COMPOSE;
				sc->sc_composed_char = 0;
				goto errkey;
			}
			break;
		}
	}
	/* keycode to key action */
	action = genkbd_keyaction(kbd, SCAN_CHAR(keycode),
	    (keycode & SCAN_RELEASE),
	    &sc->sc_state, &sc->sc_accents);
	if (action == NOKEY) {
		goto next_code;
	}
done:
	return (action);

check_composed:
	if (sc->sc_composed_char <= 0xFF) {
		goto next_code;
	}
errkey:
	return (ERRKEY);
}

/* some useful control functions */
static int
ukbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t arg)
{
	/* translate LED_XXX bits into the device specific bits */
	static const uint8_t ledmap[8] = {
		0, 2, 1, 3, 4, 6, 5, 7,
	};
	struct ukbd_softc *sc = kbd->kb_data;
	int i;

#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	int ival;

#endif
	if (!mtx_owned(&Giant)) {
		/*
		 * XXX big problem: If scroll lock is pressed and "printf()"
		 * is called, the CPU will get here, to un-scroll lock the
		 * keyboard. But if "printf()" acquires the "Giant" lock,
		 * there will be a locking order reversal problem, so the
		 * keyboard system must get out of "Giant" first, before the
		 * CPU can proceed here ...
		 */
		return (EINVAL);
	}
	mtx_assert(&Giant, MA_OWNED);

	switch (cmd) {
	case KDGKBMODE:		/* get keyboard mode */
		*(int *)arg = sc->sc_mode;
		break;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('K', 7):
		ival = IOCPARM_IVAL(arg);
		arg = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case KDSKBMODE:		/* set keyboard mode */
		switch (*(int *)arg) {
		case K_XLATE:
			if (sc->sc_mode != K_XLATE) {
				/* make lock key state and LED state match */
				sc->sc_state &= ~LOCK_MASK;
				sc->sc_state |= KBD_LED_VAL(kbd);
			}
			/* FALLTHROUGH */
		case K_RAW:
		case K_CODE:
			if (sc->sc_mode != *(int *)arg) {
				ukbd_clear_state(kbd);
				sc->sc_mode = *(int *)arg;
			}
			break;
		default:
			return (EINVAL);
		}
		break;

	case KDGETLED:			/* get keyboard LED */
		*(int *)arg = KBD_LED_VAL(kbd);
		break;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('K', 66):
		ival = IOCPARM_IVAL(arg);
		arg = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case KDSETLED:			/* set keyboard LED */
		/* NOTE: lock key state in "sc_state" won't be changed */
		if (*(int *)arg & ~LOCK_MASK) {
			return (EINVAL);
		}
		i = *(int *)arg;
		/* replace CAPS LED with ALTGR LED for ALTGR keyboards */
		if (sc->sc_mode == K_XLATE &&
		    kbd->kb_keymap->n_keys > ALTGR_OFFSET) {
			if (i & ALKED)
				i |= CLKED;
			else
				i &= ~CLKED;
		}
		if (KBD_HAS_DEVICE(kbd)) {
			ukbd_set_leds(sc, ledmap[i & LED_MASK]);
		}
		KBD_LED_VAL(kbd) = *(int *)arg;
		break;
	case KDGKBSTATE:		/* get lock key state */
		*(int *)arg = sc->sc_state & LOCK_MASK;
		break;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('K', 20):
		ival = IOCPARM_IVAL(arg);
		arg = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case KDSKBSTATE:		/* set lock key state */
		if (*(int *)arg & ~LOCK_MASK) {
			return (EINVAL);
		}
		sc->sc_state &= ~LOCK_MASK;
		sc->sc_state |= *(int *)arg;

		/* set LEDs and quit */
		return (ukbd_ioctl(kbd, KDSETLED, arg));

	case KDSETREPEAT:		/* set keyboard repeat rate (new
					 * interface) */
		if (!KBD_HAS_DEVICE(kbd)) {
			return (0);
		}
		if (((int *)arg)[1] < 0) {
			return (EINVAL);
		}
		if (((int *)arg)[0] < 0) {
			return (EINVAL);
		}
		if (((int *)arg)[0] < 200)	/* fastest possible value */
			kbd->kb_delay1 = 200;
		else
			kbd->kb_delay1 = ((int *)arg)[0];
		kbd->kb_delay2 = ((int *)arg)[1];
		return (0);

#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('K', 67):
		ival = IOCPARM_IVAL(arg);
		arg = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case KDSETRAD:			/* set keyboard repeat rate (old
					 * interface) */
		return (ukbd_set_typematic(kbd, *(int *)arg));

	case PIO_KEYMAP:		/* set keyboard translation table */
	case PIO_KEYMAPENT:		/* set keyboard translation table
					 * entry */
	case PIO_DEADKEYMAP:		/* set accent key translation table */
		sc->sc_accents = 0;
		/* FALLTHROUGH */
	default:
		return (genkbd_commonioctl(kbd, cmd, arg));
	}

	return (0);
}

/* clear the internal state of the keyboard */
static void
ukbd_clear_state(keyboard_t *kbd)
{
	struct ukbd_softc *sc = kbd->kb_data;

	if (!mtx_owned(&Giant)) {
		return;			/* XXX */
	}
	mtx_assert(&Giant, MA_OWNED);

	sc->sc_flags &= ~(UKBD_FLAG_COMPOSE | UKBD_FLAG_POLLING);
	sc->sc_state &= LOCK_MASK;	/* preserve locking key state */
	sc->sc_accents = 0;
	sc->sc_composed_char = 0;
#ifdef UKBD_EMULATE_ATSCANCODE
	sc->sc_buffered_char[0] = 0;
	sc->sc_buffered_char[1] = 0;
#endif
	bzero(&sc->sc_ndata, sizeof(sc->sc_ndata));
	bzero(&sc->sc_odata, sizeof(sc->sc_odata));
	bzero(&sc->sc_ntime, sizeof(sc->sc_ntime));
	bzero(&sc->sc_otime, sizeof(sc->sc_otime));
	return;
}

/* save the internal state, not used */
static int
ukbd_get_state(keyboard_t *kbd, void *buf, size_t len)
{
	mtx_assert(&Giant, MA_OWNED);
	return (len == 0) ? 1 : -1;
}

/* set the internal state, not used */
static int
ukbd_set_state(keyboard_t *kbd, void *buf, size_t len)
{
	mtx_assert(&Giant, MA_OWNED);
	return (EINVAL);
}

static int
ukbd_poll(keyboard_t *kbd, int on)
{
	struct ukbd_softc *sc = kbd->kb_data;

	if (!mtx_owned(&Giant)) {
		return (0);		/* XXX */
	}
	mtx_assert(&Giant, MA_OWNED);

	if (on) {
		sc->sc_flags |= UKBD_FLAG_POLLING;
	} else {
		sc->sc_flags &= ~UKBD_FLAG_POLLING;
	}
	return (0);
}

/* local functions */

static void
ukbd_set_leds(struct ukbd_softc *sc, uint8_t leds)
{
	DPRINTF("leds=0x%02x\n", leds);

	sc->sc_leds = leds;
	sc->sc_flags |= UKBD_FLAG_SET_LEDS;

	/* start transfer, if not already started */

	usb2_transfer_start(sc->sc_xfer[2]);

	return;
}

static int
ukbd_set_typematic(keyboard_t *kbd, int code)
{
	static const int delays[] = {250, 500, 750, 1000};
	static const int rates[] = {34, 38, 42, 46, 50, 55, 59, 63,
		68, 76, 84, 92, 100, 110, 118, 126,
		136, 152, 168, 184, 200, 220, 236, 252,
	272, 304, 336, 368, 400, 440, 472, 504};

	if (code & ~0x7f) {
		return (EINVAL);
	}
	kbd->kb_delay1 = delays[(code >> 5) & 3];
	kbd->kb_delay2 = rates[code & 0x1f];
	return (0);
}

#ifdef UKBD_EMULATE_ATSCANCODE
static int
ukbd_key2scan(struct ukbd_softc *sc, int code, int shift, int up)
{
	static const int scan[] = {
		0x1c, 0x1d, 0x35,
		0x37 | SCAN_PREFIX_SHIFT,	/* PrintScreen */
		0x38, 0x47, 0x48, 0x49, 0x4b, 0x4d, 0x4f,
		0x50, 0x51, 0x52, 0x53,
		0x46,			/* XXX Pause/Break */
		0x5b, 0x5c, 0x5d,
		/* SUN TYPE 6 USB KEYBOARD */
		0x68, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x25, 0x1f, 0x1e,
		0x20,
	};

	if ((code >= 89) && (code < (89 + (sizeof(scan) / sizeof(scan[0]))))) {
		code = scan[code - 89] | SCAN_PREFIX_E0;
	}
	/* Pause/Break */
	if ((code == 104) && (!(shift & (MOD_CONTROL_L | MOD_CONTROL_R)))) {
		code = (0x45 | SCAN_PREFIX_E1 | SCAN_PREFIX_CTL);
	}
	if (shift & (MOD_SHIFT_L | MOD_SHIFT_R)) {
		code &= ~SCAN_PREFIX_SHIFT;
	}
	code |= (up ? SCAN_RELEASE : SCAN_PRESS);

	if (code & SCAN_PREFIX) {
		if (code & SCAN_PREFIX_CTL) {
			/* Ctrl */
			sc->sc_buffered_char[0] = (0x1d | (code & SCAN_RELEASE));
			sc->sc_buffered_char[1] = (code & ~SCAN_PREFIX);
		} else if (code & SCAN_PREFIX_SHIFT) {
			/* Shift */
			sc->sc_buffered_char[0] = (0x2a | (code & SCAN_RELEASE));
			sc->sc_buffered_char[1] = (code & ~SCAN_PREFIX_SHIFT);
		} else {
			sc->sc_buffered_char[0] = (code & ~SCAN_PREFIX);
			sc->sc_buffered_char[1] = 0;
		}
		return ((code & SCAN_PREFIX_E0) ? 0xe0 : 0xe1);
	}
	return (code);

}

#endif					/* UKBD_EMULATE_ATSCANCODE */

keyboard_switch_t ukbdsw = {
	.probe = &ukbd__probe,
	.init = &ukbd_init,
	.term = &ukbd_term,
	.intr = &ukbd_intr,
	.test_if = &ukbd_test_if,
	.enable = &ukbd_enable,
	.disable = &ukbd_disable,
	.read = &ukbd_read,
	.check = &ukbd_check,
	.read_char = &ukbd_read_char,
	.check_char = &ukbd_check_char,
	.ioctl = &ukbd_ioctl,
	.lock = &ukbd_lock,
	.clear_state = &ukbd_clear_state,
	.get_state = &ukbd_get_state,
	.set_state = &ukbd_set_state,
	.get_fkeystr = &genkbd_get_fkeystr,
	.poll = &ukbd_poll,
	.diag = &genkbd_diag,
};

KEYBOARD_DRIVER(ukbd, ukbdsw, ukbd_configure);

static int
ukbd_driver_load(module_t mod, int what, void *arg)
{
	switch (what) {
		case MOD_LOAD:
		kbd_add_driver(&ukbd_kbd_driver);
		break;
	case MOD_UNLOAD:
		kbd_delete_driver(&ukbd_kbd_driver);
		break;
	}
	return (0);
}

static devclass_t ukbd_devclass;

static device_method_t ukbd_methods[] = {
	DEVMETHOD(device_probe, ukbd_probe),
	DEVMETHOD(device_attach, ukbd_attach),
	DEVMETHOD(device_detach, ukbd_detach),
	DEVMETHOD(device_resume, ukbd_resume),
	{0, 0}
};

static driver_t ukbd_driver = {
	.name = "ukbd",
	.methods = ukbd_methods,
	.size = sizeof(struct ukbd_softc),
};

DRIVER_MODULE(ukbd, ushub, ukbd_driver, ukbd_devclass, ukbd_driver_load, 0);
MODULE_DEPEND(ukbd, usb2_input, 1, 1, 1);
MODULE_DEPEND(ukbd, usb2_core, 1, 1, 1);
