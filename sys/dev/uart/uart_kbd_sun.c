/*-
 * Copyright (c) 2003 Jake Burkholder.
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

#include "opt_kbd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kbio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ktr.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <dev/kbd/kbdreg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <dev/uart/uart_kbd_sun.h>
#include <dev/uart/uart_kbd_sun_tables.h>

#include "uart_if.h"

#define	SUNKBD_BUF_SIZE		128

#define	TODO	printf("%s: unimplemented", __func__)

struct sunkbd_softc {
	keyboard_t		sc_kbd;
	struct uart_softc	*sc_uart;
	struct uart_devinfo	*sc_sysdev;

	struct callout		sc_repeat_callout;
	int			sc_repeat_key;

	int			sc_accents;
	int			sc_mode;
	int			sc_polling;
	int			sc_repeating;
	int			sc_state;
};

static int sunkbd_configure(int flags);
static int sunkbd_probe_keyboard(struct uart_devinfo *di);

static int sunkbd_probe(int unit, void *arg, int flags);
static int sunkbd_init(int unit, keyboard_t **kbdp, void *arg, int flags);
static int sunkbd_term(keyboard_t *kbd);
static int sunkbd_intr(keyboard_t *kbd, void *arg);
static int sunkbd_test_if(keyboard_t *kbd);
static int sunkbd_enable(keyboard_t *kbd);
static int sunkbd_disable(keyboard_t *kbd);
static int sunkbd_read(keyboard_t *kbd, int wait);
static int sunkbd_check(keyboard_t *kbd);
static u_int sunkbd_read_char(keyboard_t *kbd, int wait);
static int sunkbd_check_char(keyboard_t *kbd);
static int sunkbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t data);
static int sunkbd_lock(keyboard_t *kbd, int lock);
static void sunkbd_clear_state(keyboard_t *kbd);
static int sunkbd_get_state(keyboard_t *kbd, void *buf, size_t len);
static int sunkbd_set_state(keyboard_t *kbd, void *buf, size_t len);
static int sunkbd_poll_mode(keyboard_t *kbd, int on);
static void sunkbd_diag(keyboard_t *kbd, int level);

static void sunkbd_repeat(void *v);

static keyboard_switch_t sunkbdsw = {
	sunkbd_probe,
	sunkbd_init,
	sunkbd_term,
	sunkbd_intr,
	sunkbd_test_if,
	sunkbd_enable,
	sunkbd_disable,
	sunkbd_read,
	sunkbd_check,
	sunkbd_read_char,
	sunkbd_check_char,
	sunkbd_ioctl,
	sunkbd_lock,
	sunkbd_clear_state,
	sunkbd_get_state,
	sunkbd_set_state,
	genkbd_get_fkeystr,
	sunkbd_poll_mode,
	sunkbd_diag
};

KEYBOARD_DRIVER(sunkbd, sunkbdsw, sunkbd_configure);

static struct sunkbd_softc sunkbd_softc;
static struct uart_devinfo uart_keyboard;

static fkeytab_t fkey_tab[96] = {
/* 01-04 */	{"\033[M", 3}, {"\033[N", 3}, {"\033[O", 3}, {"\033[P", 3},
/* 05-08 */	{"\033[Q", 3}, {"\033[R", 3}, {"\033[S", 3}, {"\033[T", 3},
/* 09-12 */	{"\033[U", 3}, {"\033[V", 3}, {"\033[W", 3}, {"\033[X", 3},
/* 13-16 */	{"\033[Y", 3}, {"\033[Z", 3}, {"\033[a", 3}, {"\033[b", 3},
/* 17-20 */	{"\033[c", 3}, {"\033[d", 3}, {"\033[e", 3}, {"\033[f", 3},
/* 21-24 */	{"\033[g", 3}, {"\033[h", 3}, {"\033[i", 3}, {"\033[j", 3},
/* 25-28 */	{"\033[k", 3}, {"\033[l", 3}, {"\033[m", 3}, {"\033[n", 3},
/* 29-32 */	{"\033[o", 3}, {"\033[p", 3}, {"\033[q", 3}, {"\033[r", 3},
/* 33-36 */	{"\033[s", 3}, {"\033[t", 3}, {"\033[u", 3}, {"\033[v", 3},
/* 37-40 */	{"\033[w", 3}, {"\033[x", 3}, {"\033[y", 3}, {"\033[z", 3},
/* 41-44 */	{"\033[@", 3}, {"\033[[", 3}, {"\033[\\",3}, {"\033[]", 3},
/* 45-48 */     {"\033[^", 3}, {"\033[_", 3}, {"\033[`", 3}, {"\033[{", 3},
/* 49-52 */	{"\033[H", 3}, {"\033[A", 3}, {"\033[I", 3}, {"-"     , 1},
/* 53-56 */	{"\033[D", 3}, {"\033[E", 3}, {"\033[C", 3}, {"+"     , 1},
/* 57-60 */	{"\033[F", 3}, {"\033[B", 3}, {"\033[G", 3}, {"\033[L", 3},
/* 61-64 */	{"\177", 1},   {"\033[J", 3}, {"\033[~", 3}, {"\033[}", 3},
/* 65-68 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 69-72 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 73-76 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 77-80 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 81-84 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 85-88 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 89-92 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}      ,
/* 93-96 */	{"", 0}      , {"", 0}      , {"", 0}      , {"", 0}
};

static int
sunkbd_probe_keyboard(struct uart_devinfo *di)
{
	int tries;

	for (tries = 5; tries != 0; tries--) {
		int ltries;

		uart_putc(di, SKBD_CMD_RESET);
		for (ltries = 1000; ltries != 0; ltries--) {
			if (uart_poll(di) == SKBD_RSP_RESET)
				break;
			DELAY(1000);
		}
		if (ltries == 0)
			continue;
		for (ltries = 1000; ltries != 0; ltries--) {
			if (uart_poll(di) == SKBD_RSP_IDLE)
				break;
			DELAY(1000);
		}
		if (ltries == 0)
			continue;
		uart_putc(di, SKBD_CMD_LAYOUT);
		if (uart_getc(di) != SKBD_RSP_LAYOUT)
			break;
		return (uart_getc(di));
	}
	return (-1);
}

static int sunkbd_attach(struct uart_softc *sc);
static void sunkbd_uart_intr(void *arg);

static int
sunkbd_configure(int flags)
{
	struct sunkbd_softc *sc;

	if (KBD_IS_CONFIGURED(&sunkbd_softc.sc_kbd))
		goto found;

	if (!KBD_IS_INITIALIZED(&sunkbd_softc.sc_kbd)) {
		if (uart_cpu_getdev(UART_DEV_KEYBOARD, &uart_keyboard))
			return (0);
		if (uart_probe(&uart_keyboard))
			return (0);
		uart_init(&uart_keyboard);

		uart_keyboard.type = UART_DEV_KEYBOARD;
		uart_keyboard.attach = sunkbd_attach;
		uart_add_sysdev(&uart_keyboard);
		KBD_INIT_DONE(&sunkbd_softc.sc_kbd);
	}

	if (sunkbd_probe_keyboard(&uart_keyboard) == -1)
		return (0);

	sc = &sunkbd_softc;
	callout_init(&sc->sc_repeat_callout, 0);
	sc->sc_repeat_key = -1;
	sc->sc_repeating = 0;

	kbd_init_struct(&sc->sc_kbd, "sunkbd", KB_OTHER, 0, 0, 0, 0);
	kbd_set_maps(&sc->sc_kbd, &keymap_sun_us_unix_kbd,
	    &accentmap_sun_us_unix_kbd, fkey_tab,
	    sizeof(fkey_tab) / sizeof(fkey_tab[0]));
	sc->sc_mode = K_XLATE;
	kbd_register(&sc->sc_kbd);

	sc->sc_sysdev = &uart_keyboard;
	KBD_CONFIG_DONE(&sc->sc_kbd);

 found:
	/* Return number of found keyboards. */
	return (1);
}

static int
sunkbd_attach(struct uart_softc *sc)
{

	/*
	 * Don't attach if we didn't probe the keyboard. Note that
	 * the UART is still marked as a system device in that case.
	 */
	if (sunkbd_softc.sc_sysdev == NULL) {
		device_printf(sc->sc_dev, "keyboard not present\n");
		return (0);
	}

	if (sc->sc_sysdev != NULL) {
		sunkbd_softc.sc_uart = sc;

#ifdef KBD_INSTALL_CDEV
		kbd_attach(&sunkbd_softc.sc_kbd);
#endif
		sunkbd_enable(&sunkbd_softc.sc_kbd);

		swi_add(&tty_ithd, uart_driver_name, sunkbd_uart_intr,
		    &sunkbd_softc, SWI_TTY, INTR_TYPE_TTY, &sc->sc_softih);

		sc->sc_opened = 1;
	}

	return (0);
}

static void
sunkbd_uart_intr(void *arg)
{
	struct sunkbd_softc *sc = arg;
	int pend;

	if (sc->sc_uart->sc_leaving)
		return;

	pend = atomic_readandclear_32(&sc->sc_uart->sc_ttypend);
	if (!(pend & UART_IPEND_MASK))
		return;

	if (pend & UART_IPEND_RXREADY) {
		if (KBD_IS_ACTIVE(&sc->sc_kbd) && KBD_IS_BUSY(&sc->sc_kbd)) {
			sc->sc_kbd.kb_callback.kc_func(&sc->sc_kbd,
			    KBDIO_KEYINPUT, sc->sc_kbd.kb_callback.kc_arg);
		}
	}

}

static int
sunkbd_probe(int unit, void *arg, int flags)
{
	TODO;
	return (0);
}

static int
sunkbd_init(int unit, keyboard_t **kbdp, void *arg, int flags)
{
	TODO;
	return (0);
}

static int
sunkbd_term(keyboard_t *kbd)
{
	TODO;
	return (0);
}

static int
sunkbd_intr(keyboard_t *kbd, void *arg)
{
	TODO;
	return (0);
}

static int
sunkbd_test_if(keyboard_t *kbd)
{
	TODO;
	return (0);
}

static int
sunkbd_enable(keyboard_t *kbd)
{
	KBD_ACTIVATE(kbd);
	return (0);
}

static int
sunkbd_disable(keyboard_t *kbd)
{
	KBD_DEACTIVATE(kbd);
	return (0);
}

static int
sunkbd_read(keyboard_t *kbd, int wait)
{
	TODO;
	return (0);
}

static int
sunkbd_check(keyboard_t *kbd)
{
	TODO;
	return (0);
}

static u_int
sunkbd_read_char(keyboard_t *kbd, int wait)
{
	struct sunkbd_softc *sc;
	int action;
	int key;

	sc = (struct sunkbd_softc *)kbd;
	if (sc->sc_repeating) {
		sc->sc_repeating = 0;
		callout_reset(&sc->sc_repeat_callout, hz / 10,
		    sunkbd_repeat, sc);
		key = sc->sc_repeat_key;
		if (sc->sc_mode == K_RAW)
			return (key);
		else
			return genkbd_keyaction(kbd, key & 0x7f, key & 0x80,
			    &sc->sc_state, &sc->sc_accents);
	}

	for (;;) {
		/* XXX compose */

		if (sc->sc_uart != NULL && !uart_rx_empty(sc->sc_uart)) {
			key = uart_rx_get(sc->sc_uart);
		} else if (sc->sc_polling != 0 && sc->sc_sysdev != NULL) {
			if (wait)
				key = uart_getc(sc->sc_sysdev);
			else if ((key = uart_poll(sc->sc_sysdev)) == -1)
				return (NOKEY);
		} else {
			return (NOKEY);
		}

		switch (key) {
		case SKBD_RSP_IDLE:
			break;
		default:
			++kbd->kb_count;

			if ((key & 0x80) == 0) {
				callout_reset(&sc->sc_repeat_callout, hz / 2,
				    sunkbd_repeat, sc);
				sc->sc_repeat_key = key;
			} else {
				if (sc->sc_repeat_key == (key & 0x7f)) {
					callout_stop(&sc->sc_repeat_callout);
					sc->sc_repeat_key = -1;
				}
			}

			if (sc->sc_mode == K_RAW)
				return (key);

			action = genkbd_keyaction(kbd, key & 0x7f, key & 0x80,
			    &sc->sc_state, &sc->sc_accents);
			if (action != NOKEY)
				return (action);
			break;
		}
	}
	return (0);
}

static int
sunkbd_check_char(keyboard_t *kbd)
{
	TODO;
	return (0);
}

static int
sunkbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t data)
{
	struct sunkbd_softc *sc;
	int error;

	sc = (struct sunkbd_softc *)kbd;
	error = 0;
	switch (cmd) {
	case KDGKBMODE:
		*(int *)data = sc->sc_mode;
		break;
	case KDSKBMODE:
		switch (*(int *)data) {
		case K_XLATE:
			if (sc->sc_mode != K_XLATE) {
				/* make lock key state and LED state match */
				sc->sc_state &= ~LOCK_MASK;
				sc->sc_state |= KBD_LED_VAL(kbd);
			}
			/* FALLTHROUGH */
		case K_RAW:
		case K_CODE:
			if (sc->sc_mode != *(int *)data) {
				sunkbd_clear_state(kbd);
				sc->sc_mode = *(int *)data;
			}
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case KDGETLED:
		*(int *)data = KBD_LED_VAL(kbd);
		break;
	case KDSETLED:
		if (*(int *)data & ~LOCK_MASK) {
			error = EINVAL;
			break;
		}
		if (sc->sc_uart == NULL)
			break;
		sc->sc_uart->sc_txdatasz = 2;
		sc->sc_uart->sc_txbuf[0] = SKBD_CMD_SETLED;
		sc->sc_uart->sc_txbuf[1] = 0;
		if (*(int *)data & CLKED)
			sc->sc_uart->sc_txbuf[1] |= SKBD_LED_CAPSLOCK;
		if (*(int *)data & NLKED)
			sc->sc_uart->sc_txbuf[1] |= SKBD_LED_NUMLOCK;
		if (*(int *)data & SLKED)
			sc->sc_uart->sc_txbuf[1] |= SKBD_LED_SCROLLLOCK;
		UART_TRANSMIT(sc->sc_uart);
		KBD_LED_VAL(kbd) = *(int *)data;
		break;
	case KDGKBSTATE:
		*(int *)data = sc->sc_state & LOCK_MASK;
		break;
	case KDSKBSTATE:
		if (*(int *)data & ~LOCK_MASK) {
			error = EINVAL;
			break;
		}
		sc->sc_state &= ~LOCK_MASK;
		sc->sc_state |= *(int *)data;
		break;
	case KDSETREPEAT:
	case KDSETRAD:
		break;
	case PIO_KEYMAP:
	case PIO_KEYMAPENT:
	case PIO_DEADKEYMAP:
	default:
		return (genkbd_commonioctl(kbd, cmd, data));
	}
	return (error);
}

static int
sunkbd_lock(keyboard_t *kbd, int lock)
{
	TODO;
	return (0);
}

static void
sunkbd_clear_state(keyboard_t *kbd)
{
	/* TODO; */
}

static int
sunkbd_get_state(keyboard_t *kbd, void *buf, size_t len)
{
	TODO;
	return (0);
}

static int
sunkbd_set_state(keyboard_t *kbd, void *buf, size_t len)
{
	TODO;
	return (0);
}

static int
sunkbd_poll_mode(keyboard_t *kbd, int on)
{
	struct sunkbd_softc *sc;

	sc = (struct sunkbd_softc *)kbd;
	if (on)
		sc->sc_polling++;
	else
		sc->sc_polling--;
	return (0);
}

static void
sunkbd_diag(keyboard_t *kbd, int level)
{
	TODO;
}

static void
sunkbd_repeat(void *v)
{
	struct sunkbd_softc *sc = v;

	if (sc->sc_repeat_key != -1) {
		sc->sc_repeating = 1;
		sc->sc_kbd.kb_callback.kc_func(&sc->sc_kbd,
		    KBDIO_KEYINPUT, sc->sc_kbd.kb_callback.kc_arg);
	}
}
