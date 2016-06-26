/*-
 * Copyright (c) 1999 FreeBSD(98) port team.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_kbd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/kbio.h>

#include <machine/resource.h>

#include <dev/kbd/kbdreg.h>

#include <pc98/cbus/cbus.h>
#include <isa/isavar.h>

#define DRIVER_NAME		"pckbd"

/* device configuration flags */
#define KB_CONF_FAIL_IF_NO_KBD	(1 << 0) /* don't install if no kbd is found */

typedef caddr_t		KBDC;

typedef struct pckbd_state {
	KBDC		kbdc;		/* keyboard controller */
	int		ks_mode;	/* input mode (K_XLATE,K_RAW,K_CODE) */
	int		ks_flags;	/* flags */
#define COMPOSE		(1 << 0)
	int		ks_state;	/* shift/lock key state */
	int		ks_accents;	/* accent key index (> 0) */
	u_int		ks_composed_char; /* composed char code (> 0) */
	struct callout	ks_timer;
} pckbd_state_t;

static devclass_t	pckbd_devclass;

static int		pckbdprobe(device_t dev);
static int		pckbdattach(device_t dev);
static int		pckbdresume(device_t dev);
static void		pckbd_isa_intr(void *arg);

static device_method_t pckbd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pckbdprobe),
	DEVMETHOD(device_attach,	pckbdattach),
	DEVMETHOD(device_resume,	pckbdresume),
	{ 0, 0 }
};

static driver_t pckbd_driver = {
	DRIVER_NAME,
	pckbd_methods,
	1,
};

DRIVER_MODULE(pckbd, isa, pckbd_driver, pckbd_devclass, 0, 0);

static bus_addr_t pckbd_iat[] = {0, 2};

static int		pckbd_probe_unit(device_t dev, int port, int irq,
					 int flags);
static int		pckbd_attach_unit(device_t dev, keyboard_t **kbd,
					  int port, int irq, int flags);
static timeout_t	pckbd_timeout;


static int
pckbdprobe(device_t dev)
{
	struct resource *res;
	int error, rid;

	/* Check isapnp ids */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	device_set_desc(dev, "PC-98 Keyboard");

	rid = 0;
	res = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, pckbd_iat, 2,
				  RF_ACTIVE);
	if (res == NULL)
		return ENXIO;
	isa_load_resourcev(res, pckbd_iat, 2);

	error = pckbd_probe_unit(dev,
				 isa_get_port(dev),
				 (1 << isa_get_irq(dev)),
				 device_get_flags(dev));

	bus_release_resource(dev, SYS_RES_IOPORT, rid, res);

	return (error);
}

static int
pckbdattach(device_t dev)
{
	keyboard_t	*kbd;
	void		*ih;
	struct resource	*res;
	int		error, rid;

	rid = 0;
	res = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, pckbd_iat, 2,
				  RF_ACTIVE);
	if (res == NULL)
		return ENXIO;
	isa_load_resourcev(res, pckbd_iat, 2);

	error = pckbd_attach_unit(dev, &kbd,
				  isa_get_port(dev),
				  (1 << isa_get_irq(dev)),
				  device_get_flags(dev));

	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (res == NULL)
		return ENXIO;
	bus_setup_intr(dev, res, INTR_TYPE_TTY, NULL, pckbd_isa_intr, kbd, &ih);

	return 0;
}

static int
pckbdresume(device_t dev)
{
	keyboard_t *kbd;

	kbd = kbd_get_keyboard(kbd_find_keyboard(DRIVER_NAME,
						 device_get_unit(dev)));
	if (kbd)
		kbdd_clear_state(kbd);

	return (0);
}

static void
pckbd_isa_intr(void *arg)
{
        keyboard_t	*kbd = arg;

	kbdd_intr(kbd, NULL);
}

static int
pckbd_probe_unit(device_t dev, int port, int irq, int flags)
{
	keyboard_switch_t *sw;
	int args[2];
	int error;

	sw = kbd_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return ENXIO;

	args[0] = port;
	args[1] = irq;
	error = (*sw->probe)(device_get_unit(dev), args, flags);
	if (error)
		return error;
	return 0;
}

static int
pckbd_attach_unit(device_t dev, keyboard_t **kbd, int port, int irq, int flags)
{
	keyboard_switch_t *sw;
	pckbd_state_t *state;
	int args[2];
	int error;
	int unit;

	sw = kbd_get_switch(DRIVER_NAME);
	if (sw == NULL)
		return ENXIO;

	/* reset, initialize and enable the device */
	unit = device_get_unit(dev);
	args[0] = port;
	args[1] = irq;
	*kbd = NULL;
	error = (*sw->probe)(unit, args, flags);
	if (error)
		return error;
	error = (*sw->init)(unit, kbd, args, flags);
	if (error)
		return error;
	(*sw->enable)(*kbd);

#ifdef KBD_INSTALL_CDEV
	/* attach a virtual keyboard cdev */
	error = kbd_attach(*kbd);
	if (error)
		return error;
#endif /* KBD_INSTALL_CDEV */

	/*
	 * This is a kludge to compensate for lost keyboard interrupts.
	 * A similar code used to be in syscons. See below. XXX
	 */
	state = (pckbd_state_t *)(*kbd)->kb_data;
	callout_init(&state->ks_timer, 0);
	pckbd_timeout(*kbd);

	if (bootverbose)
		(*sw->diag)(*kbd, bootverbose);

	return 0;
}

static void
pckbd_timeout(void *arg)
{
	pckbd_state_t *state;
	keyboard_t *kbd;
	int s;

	/* The following comments are extracted from syscons.c (1.287) */
	/* 
	 * With release 2.1 of the Xaccel server, the keyboard is left
	 * hanging pretty often. Apparently an interrupt from the
	 * keyboard is lost, and I don't know why (yet).
	 * This ugly hack calls scintr if input is ready for the keyboard
	 * and conveniently hides the problem.			XXX
	 */
	/*
	 * Try removing anything stuck in the keyboard controller; whether
	 * it's a keyboard scan code or mouse data. `scintr()' doesn't
	 * read the mouse data directly, but `kbdio' routines will, as a
	 * side effect.
	 */
	s = spltty();
	kbd = (keyboard_t *)arg;
	if (kbdd_lock(kbd, TRUE)) {
		/*
		 * We have seen the lock flag is not set. Let's reset
		 * the flag early, otherwise the LED update routine fails
		 * which may want the lock during the interrupt routine.
		 */
		kbdd_lock(kbd, FALSE);
		if (kbdd_check_char(kbd))
			kbdd_intr(kbd, NULL);
	}
	splx(s);
	state = (pckbd_state_t *)kbd->kb_data;
	callout_reset(&state->ks_timer, hz / 10, pckbd_timeout, arg);
}

/* LOW-LEVEL */

#include <sys/limits.h>

#define PC98KBD_DEFAULT	0

/* keyboard driver declaration */
static int		pckbd_configure(int flags);
static kbd_probe_t	pckbd_probe;
static kbd_init_t	pckbd_init;
static kbd_term_t	pckbd_term;
static kbd_intr_t	pckbd_intr;
static kbd_test_if_t	pckbd_test_if;
static kbd_enable_t	pckbd_enable;
static kbd_disable_t	pckbd_disable;
static kbd_read_t	pckbd_read;
static kbd_check_t	pckbd_check;
static kbd_read_char_t	pckbd_read_char;
static kbd_check_char_t	pckbd_check_char;
static kbd_ioctl_t	pckbd_ioctl;
static kbd_lock_t	pckbd_lock;
static kbd_clear_state_t pckbd_clear_state;
static kbd_get_state_t	pckbd_get_state;
static kbd_set_state_t	pckbd_set_state;
static kbd_poll_mode_t	pckbd_poll;

keyboard_switch_t pckbdsw = {
	pckbd_probe,
	pckbd_init,
	pckbd_term,
	pckbd_intr,
	pckbd_test_if,
	pckbd_enable,
	pckbd_disable,
	pckbd_read,
	pckbd_check,
	pckbd_read_char,
	pckbd_check_char,
	pckbd_ioctl,
	pckbd_lock,
	pckbd_clear_state,
	pckbd_get_state,
	pckbd_set_state,
	genkbd_get_fkeystr,
	pckbd_poll,
	genkbd_diag,
};

KEYBOARD_DRIVER(pckbd, pckbdsw, pckbd_configure);

struct kbdc_softc {
    int port;			/* base port address */
    int lock;			/* FIXME: XXX not quite a semaphore... */
}; 

/* local functions */
static int		probe_keyboard(KBDC kbdc, int flags);
static int		init_keyboard(KBDC kbdc, int *type, int flags);
static KBDC		kbdc_open(int port);
static int		kbdc_lock(KBDC kbdc, int lock);
static int		kbdc_data_ready(KBDC kbdc);
static int		read_kbd_data(KBDC kbdc);
static int		read_kbd_data_no_wait(KBDC kbdc);
static int		wait_for_kbd_data(struct kbdc_softc *kbdc);

/* local variables */

/* the initial key map, accent map and fkey strings */
#include <pc98/cbus/pckbdtables.h>

/* structures for the default keyboard */
static keyboard_t	default_kbd;
static pckbd_state_t	default_kbd_state;
static keymap_t		default_keymap;
static accentmap_t	default_accentmap;
static fkeytab_t	default_fkeytab[NUM_FKEYS];

/* 
 * The back door to the keyboard driver!
 * This function is called by the console driver, via the kbdio module,
 * to tickle keyboard drivers when the low-level console is being initialized.
 * Almost nothing in the kernel has been initialied yet.  Try to probe
 * keyboards if possible.
 * NOTE: because of the way the low-level conole is initialized, this routine
 * may be called more than once!!
 */
static int
pckbd_configure(int flags)
{
	keyboard_t *kbd;
	int arg[2];
	int i;

	/* XXX: a kludge to obtain the device configuration flags */
	if (resource_int_value(DRIVER_NAME, 0, "flags", &i) == 0) {
		flags |= i;
		/* if the driver is disabled, unregister the keyboard if any */
		if (resource_disabled(DRIVER_NAME, 0)) {
			i = kbd_find_keyboard(DRIVER_NAME, PC98KBD_DEFAULT);
			if (i >= 0) {
				kbd = kbd_get_keyboard(i);
				kbd_unregister(kbd);
				kbd->kb_flags &= ~KB_REGISTERED;
				return 0;
			}
		}
	}

	/* probe the default keyboard */
	arg[0] = -1;
	arg[1] = -1;
	kbd = NULL;
	if (pckbd_probe(PC98KBD_DEFAULT, arg, flags))
		return 0;
	if (pckbd_init(PC98KBD_DEFAULT, &kbd, arg, flags))
		return 0;

	/* return the number of found keyboards */
	return 1;
}

/* low-level functions */

/* detect a keyboard */
static int
pckbd_probe(int unit, void *arg, int flags)
{
	KBDC kbdc;
	int *data = (int *)arg;

	if (unit != PC98KBD_DEFAULT)
		return ENXIO;
	if (KBD_IS_PROBED(&default_kbd))
		return 0;

	kbdc = kbdc_open(data[0]);
	if (kbdc == NULL)
		return ENXIO;
	if (probe_keyboard(kbdc, flags)) {
		if (flags & KB_CONF_FAIL_IF_NO_KBD)
			return ENXIO;
	}
	return 0;
}

/* reset and initialize the device */
static int
pckbd_init(int unit, keyboard_t **kbdp, void *arg, int flags)
{
	keyboard_t *kbd;
	pckbd_state_t *state;
	keymap_t *keymap;
	accentmap_t *accmap;
	fkeytab_t *fkeymap;
	int fkeymap_size;
	int *data = (int *)arg;

	if (unit != PC98KBD_DEFAULT)			/* shouldn't happen */
		return ENXIO;

	*kbdp = kbd = &default_kbd;
	state = &default_kbd_state;
	if (!KBD_IS_PROBED(kbd)) {
		keymap = &default_keymap;
		accmap = &default_accentmap;
		fkeymap = default_fkeytab;
		fkeymap_size = nitems(default_fkeytab);

		state->kbdc = kbdc_open(data[0]);
		if (state->kbdc == NULL)
			return ENXIO;
		kbd_init_struct(kbd, DRIVER_NAME, KB_OTHER, unit, flags,
				data[0], IO_KBDSIZE);
		bcopy(&key_map, keymap, sizeof(key_map));
		bcopy(&accent_map, accmap, sizeof(accent_map));
		bcopy(fkey_tab, fkeymap,
		      imin(fkeymap_size*sizeof(fkeymap[0]), sizeof(fkey_tab)));
		kbd_set_maps(kbd, keymap, accmap, fkeymap, fkeymap_size);
		kbd->kb_data = (void *)state;

		if (probe_keyboard(state->kbdc, flags)) {/* shouldn't happen */
			if (flags & KB_CONF_FAIL_IF_NO_KBD)
				return ENXIO;
		} else {
			KBD_FOUND_DEVICE(kbd);
		}
		pckbd_clear_state(kbd);
		state->ks_mode = K_XLATE;
		KBD_PROBE_DONE(kbd);
	}
	if (!KBD_IS_INITIALIZED(kbd) && !(flags & KB_CONF_PROBE_ONLY)) {
		if (KBD_HAS_DEVICE(kbd)
		    && init_keyboard(state->kbdc, &kbd->kb_type, kbd->kb_config)
		    && (kbd->kb_config & KB_CONF_FAIL_IF_NO_KBD))
			return ENXIO;
		pckbd_ioctl(kbd, KDSETLED, (caddr_t)&state->ks_state);
		KBD_INIT_DONE(kbd);
	}
	if (!KBD_IS_CONFIGURED(kbd)) {
		if (kbd_register(kbd) < 0)
			return ENXIO;
		KBD_CONFIG_DONE(kbd);
	}

	return 0;
}

/* finish using this keyboard */
static int
pckbd_term(keyboard_t *kbd)
{
	pckbd_state_t *state = (pckbd_state_t *)kbd->kb_data;

	kbd_unregister(kbd);
	callout_drain(&state->ks_timer);
	return 0;
}

/* keyboard interrupt routine */
static int
pckbd_intr(keyboard_t *kbd, void *arg)
{
	int c;

	if (KBD_IS_ACTIVE(kbd) && KBD_IS_BUSY(kbd)) {
		/* let the callback function to process the input */
		(*kbd->kb_callback.kc_func)(kbd, KBDIO_KEYINPUT,
					    kbd->kb_callback.kc_arg);
	} else {
		/* read and discard the input; no one is waiting for input */
		do {
			c = pckbd_read_char(kbd, FALSE);
		} while (c != NOKEY);
	}
	return 0;
}

/* test the interface to the device */
static int
pckbd_test_if(keyboard_t *kbd)
{
	return 0;
}

/* 
 * Enable the access to the device; until this function is called,
 * the client cannot read from the keyboard.
 */
static int
pckbd_enable(keyboard_t *kbd)
{
	int s;

	s = spltty();
	KBD_ACTIVATE(kbd);
	splx(s);
	return 0;
}

/* disallow the access to the device */
static int
pckbd_disable(keyboard_t *kbd)
{
	int s;

	s = spltty();
	KBD_DEACTIVATE(kbd);
	splx(s);
	return 0;
}

/* read one byte from the keyboard if it's allowed */
static int
pckbd_read(keyboard_t *kbd, int wait)
{
	int c;

	if (wait)
		c = read_kbd_data(((pckbd_state_t *)kbd->kb_data)->kbdc);
	else
		c = read_kbd_data_no_wait(((pckbd_state_t *)kbd->kb_data)->kbdc);
	if (c != -1)
		++kbd->kb_count;
	return (KBD_IS_ACTIVE(kbd) ? c : -1);
}

/* check if data is waiting */
static int
pckbd_check(keyboard_t *kbd)
{
	if (!KBD_IS_ACTIVE(kbd))
		return FALSE;
	return kbdc_data_ready(((pckbd_state_t *)kbd->kb_data)->kbdc);
}

/* read char from the keyboard */
static u_int
pckbd_read_char(keyboard_t *kbd, int wait)
{
	pckbd_state_t *state;
	u_int action;
	int scancode;
	int keycode;

	state = (pckbd_state_t *)kbd->kb_data;
next_code:
	/* do we have a composed char to return? */
	if (!(state->ks_flags & COMPOSE) && (state->ks_composed_char > 0)) {
		action = state->ks_composed_char;
		state->ks_composed_char = 0;
		if (action > UCHAR_MAX)
			return ERRKEY;
		return action;
	}

	/* see if there is something in the keyboard port */
	if (wait) {
		do {
			scancode = read_kbd_data(state->kbdc);
		} while (scancode == -1);
	} else {
		scancode = read_kbd_data_no_wait(state->kbdc);
		if (scancode == -1)
			return NOKEY;
	}
	++kbd->kb_count;

#if 0
	printf("pckbd_read_char(): scancode:0x%x\n", scancode);
#endif

	/* return the byte as is for the K_RAW mode */
	if (state->ks_mode == K_RAW)
		return scancode;

	/* translate the scan code into a keycode */
	keycode = scancode & 0x7F;
	switch(scancode) {
	case 0xF3:	/* GRPH (compose key) released */
		if (state->ks_flags & COMPOSE) {
			state->ks_flags &= ~COMPOSE;
			if (state->ks_composed_char > UCHAR_MAX)
				state->ks_composed_char = 0;
		}
		break;
	case 0x73:	/* GRPH (compose key) pressed */
		if (!(state->ks_flags & COMPOSE)) {
			state->ks_flags |= COMPOSE;
			state->ks_composed_char = 0;
		}
		break;
	}

	/* return the key code in the K_CODE mode */
	if (state->ks_mode == K_CODE)
		return (keycode | (scancode & 0x80));

	/* compose a character code */
	if (state->ks_flags & COMPOSE) {
		switch (scancode) {
		/* key pressed, process it */
		case 0x42: case 0x43: case 0x44:	/* keypad 7,8,9 */
			state->ks_composed_char *= 10;
			state->ks_composed_char += scancode - 0x3B;
			if (state->ks_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;
		case 0x46: case 0x47: case 0x48:	/* keypad 4,5,6 */
			state->ks_composed_char *= 10;
			state->ks_composed_char += scancode - 0x42;
			if (state->ks_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;
		case 0x4A: case 0x4B: case 0x4C:	/* keypad 1,2,3 */
			state->ks_composed_char *= 10;
			state->ks_composed_char += scancode - 0x49;
			if (state->ks_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;
		case 0x4E:				/* keypad 0 */
			state->ks_composed_char *= 10;
			if (state->ks_composed_char > UCHAR_MAX)
				return ERRKEY;
			goto next_code;

		/* key released, no interest here */
		case 0xC2: case 0xC3: case 0xC4:	/* keypad 7,8,9 */
		case 0xC6: case 0xC7: case 0xC8:	/* keypad 4,5,6 */
		case 0xCA: case 0xCB: case 0xCC:	/* keypad 1,2,3 */
		case 0xCE:				/* keypad 0 */
			goto next_code;

		case 0x73:				/* GRPH key */
			break;

		default:
			if (state->ks_composed_char > 0) {
				state->ks_flags &= ~COMPOSE;
				state->ks_composed_char = 0;
				return ERRKEY;
			}
			break;
		}
	}

	/* keycode to key action */
	action = genkbd_keyaction(kbd, keycode, scancode & 0x80,
				  &state->ks_state, &state->ks_accents);
	if (action == NOKEY)
		goto next_code;
	else
		return action;
}

/* check if char is waiting */
static int
pckbd_check_char(keyboard_t *kbd)
{
	pckbd_state_t *state;

	if (!KBD_IS_ACTIVE(kbd))
		return FALSE;
	state = (pckbd_state_t *)kbd->kb_data;
	if (!(state->ks_flags & COMPOSE) && (state->ks_composed_char > 0))
		return TRUE;
	return kbdc_data_ready(state->kbdc);
}

/* some useful control functions */
static int
pckbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t arg)
{
	pckbd_state_t *state = kbd->kb_data;
	int s;
	int i;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	int ival;
#endif

	s = spltty();
	switch (cmd) {

	case KDGKBMODE:		/* get keyboard mode */
		*(int *)arg = state->ks_mode;
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
			if (state->ks_mode != K_XLATE) {
				/* make lock key state and LED state match */
				state->ks_state &= ~LOCK_MASK;
				state->ks_state |= KBD_LED_VAL(kbd);
			}
			/* FALLTHROUGH */
		case K_RAW:
		case K_CODE:
			if (state->ks_mode != *(int *)arg) {
				pckbd_clear_state(kbd);
				state->ks_mode = *(int *)arg;
			}
			break;
		default:
			splx(s);
			return EINVAL;
		}
		break;

	case KDGETLED:		/* get keyboard LED */
		*(int *)arg = KBD_LED_VAL(kbd);
		break;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('K', 66):
		ival = IOCPARM_IVAL(arg);
		arg = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case KDSETLED:		/* set keyboard LED */
		/* NOTE: lock key state in ks_state won't be changed */
		if (*(int *)arg & ~LOCK_MASK) {
			splx(s);
			return EINVAL;
		}
		i = *(int *)arg;
		/* replace CAPS LED with ALTGR LED for ALTGR keyboards */
		if (kbd->kb_keymap->n_keys > ALTGR_OFFSET) {
			if (i & ALKED)
				i |= CLKED;
			else
				i &= ~CLKED;
		}
		KBD_LED_VAL(kbd) = *(int *)arg;
		break;

	case KDGKBSTATE:	/* get lock key state */
		*(int *)arg = state->ks_state & LOCK_MASK;
		break;
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4) || defined(COMPAT_43)
	case _IO('K', 20):
		ival = IOCPARM_IVAL(arg);
		arg = (caddr_t)&ival;
		/* FALLTHROUGH */
#endif
	case KDSKBSTATE:	/* set lock key state */
		if (*(int *)arg & ~LOCK_MASK) {
			splx(s);
			return EINVAL;
		}
		state->ks_state &= ~LOCK_MASK;
		state->ks_state |= *(int *)arg;
		splx(s);
		/* set LEDs and quit */
		return pckbd_ioctl(kbd, KDSETLED, arg);

	case KDSETRAD:		/* set keyboard repeat rate (old interface)*/
		break;
	case KDSETREPEAT:	/* set keyboard repeat rate (new interface) */
		break;

	case PIO_KEYMAP:	/* set keyboard translation table */
	case OPIO_KEYMAP:	/* set keyboard translation table (compat) */
	case PIO_KEYMAPENT:	/* set keyboard translation table entry */
	case PIO_DEADKEYMAP:	/* set accent key translation table */
		state->ks_accents = 0;
		/* FALLTHROUGH */
	default:
		splx(s);
		return genkbd_commonioctl(kbd, cmd, arg);
	}

	splx(s);
	return 0;
}

/* lock the access to the keyboard */
static int
pckbd_lock(keyboard_t *kbd, int lock)
{
	return kbdc_lock(((pckbd_state_t *)kbd->kb_data)->kbdc, lock);
}

/* clear the internal state of the keyboard */
static void
pckbd_clear_state(keyboard_t *kbd)
{
	pckbd_state_t *state;

	state = (pckbd_state_t *)kbd->kb_data;
	state->ks_flags = 0;
	state->ks_state &= LOCK_MASK;	/* preserve locking key state */
	state->ks_accents = 0;
	state->ks_composed_char = 0;
}

/* save the internal state */
static int
pckbd_get_state(keyboard_t *kbd, void *buf, size_t len)
{
	if (len == 0)
		return sizeof(pckbd_state_t);
	if (len < sizeof(pckbd_state_t))
		return -1;
	bcopy(kbd->kb_data, buf, sizeof(pckbd_state_t));
	return 0;
}

/* set the internal state */
static int
pckbd_set_state(keyboard_t *kbd, void *buf, size_t len)
{
	if (len < sizeof(pckbd_state_t))
		return ENOMEM;
	if (((pckbd_state_t *)kbd->kb_data)->kbdc
		!= ((pckbd_state_t *)buf)->kbdc)
		return ENOMEM;
	bcopy(buf, kbd->kb_data, sizeof(pckbd_state_t));
	return 0;
}

/* set polling mode */
static int
pckbd_poll(keyboard_t *kbd, int on)
{
	return 0;
}

/* local functions */

static int
probe_keyboard(KBDC kbdc, int flags)
{
	return 0;
}

static int
init_keyboard(KBDC kbdc, int *type, int flags)
{
	*type = KB_OTHER;
	return 0;
}

/* keyboard I/O routines */

/* retry count */
#ifndef KBD_MAXRETRY
#define KBD_MAXRETRY	3
#endif

/* timing parameters */
#ifndef KBD_RESETDELAY
#define KBD_RESETDELAY  200     /* wait 200msec after kbd/mouse reset */
#endif
#ifndef KBD_MAXWAIT
#define KBD_MAXWAIT	5 	/* wait 5 times at most after reset */
#endif

/* I/O recovery time */
#define KBDC_DELAYTIME	37
#define KBDD_DELAYTIME	37

/* I/O ports */
#define KBD_STATUS_PORT 	2	/* status port, read */
#define KBD_DATA_PORT		0	/* data port, read */

/* status bits (KBD_STATUS_PORT) */
#define KBDS_BUFFER_FULL	0x0002

/* macros */

#define kbdcp(p)		((struct kbdc_softc *)(p))

/* local variables */

static struct kbdc_softc kbdc_softc[1] = { { 0 }, };

/* associate a port number with a KBDC */

static KBDC
kbdc_open(int port)
{
	if (port <= 0)
		port = IO_KBD;

	/* PC-98 has only one keyboard I/F */
	kbdc_softc[0].port = port;
	kbdc_softc[0].lock = FALSE;
	return (KBDC)&kbdc_softc[0];
}

/* set/reset polling lock */
static int 
kbdc_lock(KBDC p, int lock)
{
    int prevlock;

    prevlock = kbdcp(p)->lock;
    kbdcp(p)->lock = lock;

    return (prevlock != lock);
}

/* check if any data is waiting to be processed */
static int
kbdc_data_ready(KBDC p)
{
	return (inb(kbdcp(p)->port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL);
}

/* wait for data from the keyboard */
static int
wait_for_kbd_data(struct kbdc_softc *kbdc)
{
    /* CPU will stay inside the loop for 200msec at most */
    int retry = 10000;
    int port = kbdc->port;

    while (!(inb(port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)) {
	DELAY(KBDD_DELAYTIME);
	DELAY(KBDC_DELAYTIME);
        if (--retry < 0)
    	    return 0;
    }
    DELAY(KBDD_DELAYTIME);
    return 1;
}

/* read one byte from the keyboard */
static int
read_kbd_data(KBDC p)
{
    if (!wait_for_kbd_data(kbdcp(p)))
        return -1;		/* timeout */
    DELAY(KBDC_DELAYTIME);
    return inb(kbdcp(p)->port + KBD_DATA_PORT);
}

/* read one byte from the keyboard, but return immediately if 
 * no data is waiting
 */
static int
read_kbd_data_no_wait(KBDC p)
{
    if (inb(kbdcp(p)->port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL) {
        DELAY(KBDD_DELAYTIME);
        return inb(kbdcp(p)->port + KBD_DATA_PORT);
    }
    return -1;		/* no data */
}
