/*-
 * Copyright (c) 2017 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_evdev.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/taskqueue.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/callout.h>

#include <sys/kbio.h>
#include <dev/kbd/kbdreg.h>
#include <dev/kbd/kbdtables.h>

#ifdef EVDEV_SUPPORT
#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>
#endif

#include "dev/hyperv/input/hv_kbdc.h"

#define HVKBD_MTX_LOCK(_m) do {		\
	mtx_lock(_m);			\
} while (0)

#define HVKBD_MTX_UNLOCK(_m) do {	\
	mtx_unlock(_m);			\
} while (0)

#define HVKBD_MTX_ASSERT(_m, _t) do {	\
	mtx_assert(_m, _t);		\
} while (0)

#define	HVKBD_LOCK()		HVKBD_MTX_LOCK(&Giant)
#define	HVKBD_UNLOCK()		HVKBD_MTX_UNLOCK(&Giant)
#define	HVKBD_LOCK_ASSERT()	HVKBD_MTX_ASSERT(&Giant, MA_OWNED)

#define	HVKBD_FLAG_COMPOSE	0x00000001	/* compose char flag */
#define HVKBD_FLAG_POLLING	0x00000002

#ifdef EVDEV_SUPPORT
static evdev_event_t hvkbd_ev_event;

static const struct evdev_methods hvkbd_evdev_methods = {
	.ev_event = hvkbd_ev_event,
};
#endif

/* early keyboard probe, not supported */
static int
hvkbd_configure(int flags)
{
	return (0);
}

/* detect a keyboard, not used */
static int
hvkbd_probe(int unit, void *arg, int flags)
{
	return (ENXIO);
}

/* reset and initialize the device, not used */
static int
hvkbd_init(int unit, keyboard_t **kbdp, void *arg, int flags)
{
	DEBUG_HVKBD(*kbdp, "%s\n", __func__);
	return (ENXIO);
}

/* test the interface to the device, not used */
static int
hvkbd_test_if(keyboard_t *kbd)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	return (0);
}

/* finish using this keyboard, not used */
static int
hvkbd_term(keyboard_t *kbd)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	return (ENXIO);
}

/* keyboard interrupt routine, not used */
static int
hvkbd_intr(keyboard_t *kbd, void *arg)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	return (0);
}

/* lock the access to the keyboard, not used */
static int
hvkbd_lock(keyboard_t *kbd, int lock)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	return (1);
}

/* save the internal state, not used */
static int
hvkbd_get_state(keyboard_t *kbd, void *buf, size_t len)
{
	DEBUG_HVKBD(kbd,"%s\n",  __func__);
	return (len == 0) ? 1 : -1;
}

/* set the internal state, not used */
static int
hvkbd_set_state(keyboard_t *kbd, void *buf, size_t len)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	return (EINVAL);
}

static int
hvkbd_poll(keyboard_t *kbd, int on)
{
	hv_kbd_sc *sc = kbd->kb_data;

	HVKBD_LOCK();
	/*
	 * Keep a reference count on polling to allow recursive
	 * cngrab() during a panic for example.
	 */
	if (on)
		sc->sc_polling++;
	else if (sc->sc_polling > 0)
		sc->sc_polling--;

	if (sc->sc_polling != 0) {
		sc->sc_flags |= HVKBD_FLAG_POLLING;
	} else {
		sc->sc_flags &= ~HVKBD_FLAG_POLLING;
	}
	HVKBD_UNLOCK();
	return (0);
}

/*
 * Enable the access to the device; until this function is called,
 * the client cannot read from the keyboard.
 */
static int
hvkbd_enable(keyboard_t *kbd)
{
	HVKBD_LOCK();
	KBD_ACTIVATE(kbd);
	HVKBD_UNLOCK();
	return (0);
}

/* disallow the access to the device */
static int
hvkbd_disable(keyboard_t *kbd)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	HVKBD_LOCK();
	KBD_DEACTIVATE(kbd);
	HVKBD_UNLOCK();
	return (0);
}

static void
hvkbd_do_poll(hv_kbd_sc *sc, uint8_t wait)
{
	while (!hv_kbd_prod_is_ready(sc)) {
		hv_kbd_read_channel(sc->hs_chan, sc);
		if (!wait)
			break;
	}
}

/* check if data is waiting */
/* Currently unused. */
static int
hvkbd_check(keyboard_t *kbd)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	return (0);
}

/* check if char is waiting */
static int
hvkbd_check_char_locked(keyboard_t *kbd)
{
	HVKBD_LOCK_ASSERT();
	if (!KBD_IS_ACTIVE(kbd))
		return (FALSE);

	hv_kbd_sc *sc = kbd->kb_data;
	if (!(sc->sc_flags & HVKBD_FLAG_COMPOSE) && sc->sc_composed_char != 0)
		return (TRUE);
	if (sc->sc_flags & HVKBD_FLAG_POLLING)
		hvkbd_do_poll(sc, 0);
	if (hv_kbd_prod_is_ready(sc)) {
		return (TRUE);
	}
	return (FALSE);
}

static int
hvkbd_check_char(keyboard_t *kbd)
{
	int result;

	HVKBD_LOCK();
	result = hvkbd_check_char_locked(kbd);
	HVKBD_UNLOCK();

	return (result);
}

/* read char from the keyboard */
static uint32_t
hvkbd_read_char_locked(keyboard_t *kbd, int wait)
{
	uint32_t scancode = NOKEY;
	uint32_t action;
	keystroke ks;
	hv_kbd_sc *sc = kbd->kb_data;
	int keycode;

	HVKBD_LOCK_ASSERT();

	if (!KBD_IS_ACTIVE(kbd) || !hv_kbd_prod_is_ready(sc))
		return (NOKEY);

next_code:

	/* do we have a composed char to return? */
	if (!(sc->sc_flags & HVKBD_FLAG_COMPOSE) && sc->sc_composed_char > 0) {
		action = sc->sc_composed_char;
		sc->sc_composed_char = 0;
		if (action > UCHAR_MAX) {
			return (ERRKEY);
		}
		return (action);
	}

	if (hv_kbd_fetch_top(sc, &ks)) {
		return (NOKEY);
	}
	if ((ks.info & IS_E0) || (ks.info & IS_E1)) {
		/**
		 * Emulate the generation of E0 or E1 scancode,
		 * the real scancode will be consumed next time.
		 */
		if (ks.info & IS_E0) {
			scancode = XTKBD_EMUL0;
			ks.info &= ~IS_E0;
		} else if (ks.info & IS_E1) {
			scancode = XTKBD_EMUL1;
			ks.info &= ~IS_E1;
		}
		/**
		 * Change the top item to avoid encountering
		 * E0 or E1 twice.
		 */
		hv_kbd_modify_top(sc, &ks);
	} else if (ks.info & IS_UNICODE) {
		/**
		 * XXX: Hyperv host send unicode to VM through
		 * 'Type clipboard text', the mapping from
		 * unicode to scancode depends on the keymap.
		 * It is so complicated that we do not plan to
		 * support it yet.
		 */
		if (bootverbose)
			device_printf(sc->dev, "Unsupported unicode\n");
		hv_kbd_remove_top(sc);
		return (NOKEY);
	} else {
		scancode = ks.makecode;
		if (ks.info & IS_BREAK) {
			scancode |= XTKBD_RELEASE;
		}
		hv_kbd_remove_top(sc);
	}
#ifdef EVDEV_SUPPORT
	/* push evdev event */
	if (evdev_rcpt_mask & EVDEV_RCPT_HW_KBD &&
	    sc->ks_evdev != NULL) {
		keycode = evdev_scancode2key(&sc->ks_evdev_state,
		    scancode);

		if (keycode != KEY_RESERVED) {
			evdev_push_event(sc->ks_evdev, EV_KEY,
			    (uint16_t)keycode, scancode & 0x80 ? 0 : 1);
			evdev_sync(sc->ks_evdev);
		}
	}
	if (sc->ks_evdev != NULL && evdev_is_grabbed(sc->ks_evdev))
		return (NOKEY);
#endif
	++kbd->kb_count;
	DEBUG_HVKBD(kbd, "read scan: 0x%x\n", scancode);

	/* return the byte as is for the K_RAW mode */
	if (sc->sc_mode == K_RAW)
		return scancode;

	/* translate the scan code into a keycode */
	keycode = scancode & 0x7F;
	switch (sc->sc_prefix) {
	case 0x00:      /* normal scancode */
		switch(scancode) {
		case 0xB8:      /* left alt (compose key) released */
			if (sc->sc_flags & HVKBD_FLAG_COMPOSE) {
				sc->sc_flags &= ~HVKBD_FLAG_COMPOSE;
				if (sc->sc_composed_char > UCHAR_MAX)
					sc->sc_composed_char = 0;
			}
			break;
		case 0x38:      /* left alt (compose key) pressed */
			if (!(sc->sc_flags & HVKBD_FLAG_COMPOSE)) {
				sc->sc_flags |= HVKBD_FLAG_COMPOSE;
				sc->sc_composed_char = 0;
			}
			break;
		case 0xE0:
		case 0xE1:
			sc->sc_prefix = scancode;
			goto next_code;
		}
		break;
	case 0xE0:		/* 0xE0 prefix */
		sc->sc_prefix = 0;
		switch (keycode) {
		case 0x1C:	/* right enter key */
			keycode = 0x59;
			break;
		case 0x1D:	/* right ctrl key */
			keycode = 0x5A;
			break;
		case 0x35:	/* keypad divide key */
			keycode = 0x5B;
			break;
		case 0x37:	/* print scrn key */
			keycode = 0x5C;
			break;
		case 0x38:	/* right alt key (alt gr) */
			keycode = 0x5D;
			break;
		case 0x46:	/* ctrl-pause/break on AT 101 (see below) */
			keycode = 0x68;
			break;
		case 0x47:	/* grey home key */
			keycode = 0x5E;
			break;
		case 0x48:	/* grey up arrow key */
			keycode = 0x5F;
			break;
		case 0x49:	/* grey page up key */
			keycode = 0x60;
			break;
		case 0x4B:	/* grey left arrow key */
			keycode = 0x61;
			break;
		case 0x4D:	/* grey right arrow key */
			keycode = 0x62;
			break;
		case 0x4F:	/* grey end key */
			keycode = 0x63;
			break;
		case 0x50:	/* grey down arrow key */
			keycode = 0x64;
			break;
		case 0x51:	/* grey page down key */
			keycode = 0x65;
			break;
		case 0x52:	/* grey insert key */
			keycode = 0x66;
			break;
		case 0x53:	/* grey delete key */
			keycode = 0x67;
			break;
			/* the following 3 are only used on the MS "Natural" keyboard */
		case 0x5b:	/* left Window key */
			keycode = 0x69;
			break;
		case 0x5c:	/* right Window key */
			keycode = 0x6a;
			break;
		case 0x5d:	/* menu key */
			keycode = 0x6b;
			break;
		case 0x5e:	/* power key */
			keycode = 0x6d;
			break;
		case 0x5f:	/* sleep key */
			keycode = 0x6e;
			break;
		case 0x63:	/* wake key */
			keycode = 0x6f;
			break;
		default:	/* ignore everything else */
			goto next_code;
		}
		break;
	case 0xE1:	/* 0xE1 prefix */
		/*
		 * The pause/break key on the 101 keyboard produces:
		 * E1-1D-45 E1-9D-C5
		 * Ctrl-pause/break produces:
		 * E0-46 E0-C6 (See above.)
		 */
		sc->sc_prefix = 0;
		if (keycode == 0x1D)
			sc->sc_prefix = 0x1D;
		goto next_code;
		/* NOT REACHED */
	case 0x1D:	/* pause / break */
		sc->sc_prefix = 0;
		if (keycode != 0x45)
			goto next_code;
		keycode = 0x68;
		break;
	}

	/* XXX assume 101/102 keys AT keyboard */
	switch (keycode) {
	case 0x5c:      /* print screen */
		if (sc->sc_flags & ALTS)
			keycode = 0x54; /* sysrq */
		break;
	case 0x68:      /* pause/break */
		if (sc->sc_flags & CTLS)
			keycode = 0x6c; /* break */
		break;
	}

	/* return the key code in the K_CODE mode */
	if (sc->sc_mode == K_CODE)
		return (keycode | (scancode & 0x80));

	/* compose a character code */
	if (sc->sc_flags &  HVKBD_FLAG_COMPOSE) {
		switch (keycode | (scancode & 0x80)) {
		/* key pressed, process it */
		case 0x47: case 0x48: case 0x49:	/* keypad 7,8,9 */
			sc->sc_composed_char *= 10;
			sc->sc_composed_char += keycode - 0x40;
			if (sc->sc_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;
		case 0x4B: case 0x4C: case 0x4D:	/* keypad 4,5,6 */
			sc->sc_composed_char *= 10;
			sc->sc_composed_char += keycode - 0x47;
			if (sc->sc_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;
		case 0x4F: case 0x50: case 0x51:	/* keypad 1,2,3 */
			sc->sc_composed_char *= 10;
			sc->sc_composed_char += keycode - 0x4E;
			if (sc->sc_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;
		case 0x52:				/* keypad 0 */
			sc->sc_composed_char *= 10;
			if (sc->sc_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;

		/* key released, no interest here */
		case 0xC7: case 0xC8: case 0xC9:	/* keypad 7,8,9 */
		case 0xCB: case 0xCC: case 0xCD:	/* keypad 4,5,6 */
		case 0xCF: case 0xD0: case 0xD1:	/* keypad 1,2,3 */
		case 0xD2:				/* keypad 0 */
			goto next_code;

		case 0x38:				/* left alt key */
			break;

		default:
			if (sc->sc_composed_char > 0) {
				sc->sc_flags &= ~HVKBD_FLAG_COMPOSE;
				sc->sc_composed_char = 0;
				return (ERRKEY);
			}
			break;
		}
	}

	/* keycode to key action */
	action = genkbd_keyaction(kbd, keycode, scancode & 0x80,
				  &sc->sc_state, &sc->sc_accents);
	if (action == NOKEY)
		goto next_code;
	else
		return (action);
}

/* Currently wait is always false. */
static uint32_t
hvkbd_read_char(keyboard_t *kbd, int wait)
{
	uint32_t keycode;

	HVKBD_LOCK();
	keycode = hvkbd_read_char_locked(kbd, wait);
	HVKBD_UNLOCK();

	return (keycode);
}

/* clear the internal state of the keyboard */
static void
hvkbd_clear_state(keyboard_t *kbd)
{
	hv_kbd_sc *sc = kbd->kb_data;
	sc->sc_state &= LOCK_MASK;	/* preserve locking key state */
	sc->sc_flags &= ~(HVKBD_FLAG_POLLING | HVKBD_FLAG_COMPOSE);
	sc->sc_accents = 0;
	sc->sc_composed_char = 0;
}

static int
hvkbd_ioctl_locked(keyboard_t *kbd, u_long cmd, caddr_t arg)
{
	int i;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
        int ival;
#endif
	hv_kbd_sc *sc = kbd->kb_data;
	switch (cmd) {
	case KDGKBMODE:
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
		DEBUG_HVKBD(kbd, "expected mode: %x\n", *(int *)arg);
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
				DEBUG_HVKBD(kbd, "mod changed to %x\n", *(int *)arg);
				if ((sc->sc_flags & HVKBD_FLAG_POLLING) == 0)
					hvkbd_clear_state(kbd);
				sc->sc_mode = *(int *)arg;
			}
			break;
		default:
			return (EINVAL);
		}
		break;
	case KDGKBSTATE:	/* get lock key state */
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
		return hvkbd_ioctl_locked(kbd, KDSETLED, arg);
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
		if (*(int *)arg & ~LOCK_MASK)
			return (EINVAL);

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
			DEBUG_HVSC(sc, "setled 0x%x\n", *(int *)arg);
		}

#ifdef EVDEV_SUPPORT
		/* push LED states to evdev */
		if (sc->ks_evdev != NULL &&
		    evdev_rcpt_mask & EVDEV_RCPT_HW_KBD)
			evdev_push_leds(sc->ks_evdev, *(int *)arg);
#endif
		KBD_LED_VAL(kbd) = *(int *)arg;
		break;
	case PIO_KEYMAP:	/* set keyboard translation table */
	case PIO_KEYMAPENT:	/* set keyboard translation table entry */
	case PIO_DEADKEYMAP:	/* set accent key translation table */
#ifdef COMPAT_FREEBSD13
	case OPIO_KEYMAP:	/* set keyboard translation table (compat) */
	case OPIO_DEADKEYMAP:	/* set accent key translation table (compat) */
#endif /* COMPAT_FREEBSD13 */
		sc->sc_accents = 0;
		/* FALLTHROUGH */
	default:
		return (genkbd_commonioctl(kbd, cmd, arg));
	}
	return (0);
}

/* some useful control functions */
static int
hvkbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t arg)
{
	DEBUG_HVKBD(kbd, "%s: %lx start\n", __func__, cmd);
	HVKBD_LOCK();
	int ret = hvkbd_ioctl_locked(kbd, cmd, arg);
	HVKBD_UNLOCK();
	DEBUG_HVKBD(kbd, "%s: %lx end %d\n", __func__, cmd, ret);
	return (ret);
}

/* read one byte from the keyboard if it's allowed */
/* Currently unused. */
static int
hvkbd_read(keyboard_t *kbd, int wait)
{
	DEBUG_HVKBD(kbd, "%s\n", __func__);
	HVKBD_LOCK_ASSERT();
	if (!KBD_IS_ACTIVE(kbd))
		return (-1);
	return hvkbd_read_char_locked(kbd, wait);
}

#ifdef EVDEV_SUPPORT
static void
hvkbd_ev_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{
	keyboard_t *kbd = evdev_get_softc(evdev);

	if (evdev_rcpt_mask & EVDEV_RCPT_HW_KBD &&
	(type == EV_LED || type == EV_REP)) {
		mtx_lock(&Giant);
		kbd_ev_event(kbd, type, code, value);
		mtx_unlock(&Giant);
	}
}
#endif

static keyboard_switch_t hvkbdsw = {
	.probe =	hvkbd_probe,		/* not used */
	.init =		hvkbd_init,
	.term =		hvkbd_term,		/* not used */
	.intr =		hvkbd_intr,		/* not used */
	.test_if =	hvkbd_test_if,		/* not used */
	.enable =	hvkbd_enable,
	.disable =	hvkbd_disable,
	.read =		hvkbd_read,
	.check =	hvkbd_check,
	.read_char =	hvkbd_read_char,
	.check_char =	hvkbd_check_char,
	.ioctl =	hvkbd_ioctl,
	.lock =		hvkbd_lock,		/* not used */
	.clear_state =	hvkbd_clear_state,
	.get_state =	hvkbd_get_state,	/* not used */
	.set_state =	hvkbd_set_state,	/* not used */
	.poll =		hvkbd_poll,
};

KEYBOARD_DRIVER(hvkbd, hvkbdsw, hvkbd_configure);

void
hv_kbd_intr(hv_kbd_sc *sc)
{
	uint32_t c;
	if ((sc->sc_flags & HVKBD_FLAG_POLLING) != 0)
		return;

	if (KBD_IS_ACTIVE(&sc->sc_kbd) &&
	    KBD_IS_BUSY(&sc->sc_kbd)) {
		/* let the callback function process the input */
		(sc->sc_kbd.kb_callback.kc_func) (&sc->sc_kbd, KBDIO_KEYINPUT,
		    sc->sc_kbd.kb_callback.kc_arg);
	} else {
		/* read and discard the input, no one is waiting for it */
		do {
			c = hvkbd_read_char(&sc->sc_kbd, 0);
		} while (c != NOKEY);
	}
}

int
hvkbd_driver_load(module_t mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		kbd_add_driver(&hvkbd_kbd_driver);
		break;
	case MOD_UNLOAD:
		kbd_delete_driver(&hvkbd_kbd_driver);
		break;
	}
	return (0);
}

int
hv_kbd_drv_attach(device_t dev)
{
	hv_kbd_sc *sc = device_get_softc(dev);
	int unit = device_get_unit(dev);
	keyboard_t *kbd = &sc->sc_kbd;
	keyboard_switch_t *sw;
#ifdef EVDEV_SUPPORT
	struct evdev_dev *evdev;
#endif

	sw = kbd_get_switch(HVKBD_DRIVER_NAME);
	if (sw == NULL) {
		return (ENXIO);
	}

	kbd_init_struct(kbd, HVKBD_DRIVER_NAME, KB_OTHER, unit, 0, 0, 0);
	kbd->kb_data = (void *)sc;
	kbd_set_maps(kbd, &key_map, &accent_map, fkey_tab, nitems(fkey_tab));
	KBD_FOUND_DEVICE(kbd);
	hvkbd_clear_state(kbd);
	KBD_PROBE_DONE(kbd);
	KBD_INIT_DONE(kbd);
	sc->sc_mode = K_XLATE;
	(*sw->enable)(kbd);

#ifdef EVDEV_SUPPORT
	evdev = evdev_alloc();
	evdev_set_name(evdev, "Hyper-V keyboard");
	evdev_set_phys(evdev, device_get_nameunit(dev));
	evdev_set_id(evdev, BUS_VIRTUAL, 0, 0, 0);
	evdev_set_methods(evdev, kbd, &hvkbd_evdev_methods);
	evdev_support_event(evdev, EV_SYN);
	evdev_support_event(evdev, EV_KEY);
	evdev_support_event(evdev, EV_LED);
	evdev_support_event(evdev, EV_REP);
	evdev_support_all_known_keys(evdev);
	evdev_support_led(evdev, LED_NUML);
	evdev_support_led(evdev, LED_CAPSL);
	evdev_support_led(evdev, LED_SCROLLL);
	if (evdev_register_mtx(evdev, &Giant))
		evdev_free(evdev);
	else
		sc->ks_evdev = evdev;
	sc->ks_evdev_state = 0;
#endif

	if (kbd_register(kbd) < 0) {
		goto detach;
	}
	KBD_CONFIG_DONE(kbd);
#ifdef KBD_INSTALL_CDEV
        if (kbd_attach(kbd)) {
		goto detach;
	}
#endif
	if (bootverbose) {
		kbdd_diag(kbd, bootverbose);
	}
	return (0);
detach:
	hv_kbd_drv_detach(dev);
	return (ENXIO);
}

int
hv_kbd_drv_detach(device_t dev)
{
	int error = 0;
	hv_kbd_sc *sc = device_get_softc(dev);
	hvkbd_disable(&sc->sc_kbd);
#ifdef EVDEV_SUPPORT
	evdev_free(sc->ks_evdev);
#endif
	if (KBD_IS_CONFIGURED(&sc->sc_kbd)) {
		error = kbd_unregister(&sc->sc_kbd);
		if (error) {
			device_printf(dev, "WARNING: kbd_unregister() "
			    "returned non-zero! (ignored)\n");
		}
	}
#ifdef KBD_INSTALL_CDEV
	error = kbd_detach(&sc->sc_kbd);
#endif
	return (error);
}

