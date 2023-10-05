/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2015 Nahanni Systems Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
#include <sys/types.h>
#include <sys/stat.h>

#include <machine/vmm_snapshot.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <pthread_np.h>
#include <unistd.h>
#include <fcntl.h>

#include "atkbdc.h"
#include "bhyverun.h"
#include "config.h"
#include "console.h"
#include "debug.h"
#include "ps2kbd.h"

/* keyboard device commands */
#define	PS2KC_RESET_DEV		0xff
#define	PS2KC_DISABLE		0xf5
#define	PS2KC_ENABLE		0xf4
#define	PS2KC_SET_TYPEMATIC	0xf3
#define	PS2KC_SEND_DEV_ID	0xf2
#define	PS2KC_SET_SCANCODE_SET	0xf0
#define	PS2KC_ECHO		0xee
#define	PS2KC_SET_LEDS		0xed

#define	PS2KC_BAT_SUCCESS	0xaa
#define	PS2KC_ACK		0xfa

#define	PS2KBD_FIFOSZ		16

#define	PS2KBD_LAYOUT_BASEDIR	"/usr/share/bhyve/kbdlayout/"

#define	MAX_PATHNAME		256

struct fifo {
	uint8_t	buf[PS2KBD_FIFOSZ];
	int	rindex;		/* index to read from */
	int	windex;		/* index to write to */
	int	num;		/* number of bytes in the fifo */
	int	size;		/* size of the fifo */
};

struct ps2kbd_softc {
	struct atkbdc_softc	*atkbdc_sc;
	pthread_mutex_t		mtx;

	bool			enabled;
	struct fifo		fifo;

	uint8_t			curcmd;	/* current command for next byte */
};

#define SCANCODE_E0_PREFIX 1
struct extended_translation {
	uint32_t keysym;
	uint8_t scancode;
	int flags;
};

/*
 * FIXME: Pause/break and Print Screen/SysRq require special handling.
 */
static struct extended_translation extended_translations[128] = {
		{0xff08, 0x66, 0},		/* Back space */
		{0xff09, 0x0d, 0},		/* Tab */
		{0xff0d, 0x5a, 0},		/* Return */
		{0xff1b, 0x76, 0},		/* Escape */
		{0xff50, 0x6c, SCANCODE_E0_PREFIX}, 	/* Home */
		{0xff51, 0x6b, SCANCODE_E0_PREFIX}, 	/* Left arrow */
		{0xff52, 0x75, SCANCODE_E0_PREFIX}, 	/* Up arrow */
		{0xff53, 0x74, SCANCODE_E0_PREFIX}, 	/* Right arrow */
		{0xff54, 0x72, SCANCODE_E0_PREFIX}, 	/* Down arrow */
		{0xff55, 0x7d, SCANCODE_E0_PREFIX}, 	/* PgUp */
		{0xff56, 0x7a, SCANCODE_E0_PREFIX}, 	/* PgDown */
		{0xff57, 0x69, SCANCODE_E0_PREFIX}, 	/* End */
		{0xff63, 0x70, SCANCODE_E0_PREFIX}, 	/* Ins */
		{0xff8d, 0x5a, SCANCODE_E0_PREFIX}, 	/* Keypad Enter */
		{0xffe1, 0x12, 0},		/* Left shift */
		{0xffe2, 0x59, 0},		/* Right shift */
		{0xffe3, 0x14, 0},		/* Left control */
		{0xffe4, 0x14, SCANCODE_E0_PREFIX}, 	/* Right control */
		/* {0xffe7, XXX}, Left meta */
		/* {0xffe8, XXX}, Right meta */
		{0xffe9, 0x11, 0},		/* Left alt */
		{0xfe03, 0x11, SCANCODE_E0_PREFIX}, 	/* AltGr */
		{0xffea, 0x11, SCANCODE_E0_PREFIX}, 	/* Right alt */
		{0xffeb, 0x1f, SCANCODE_E0_PREFIX}, 	/* Left Windows */
		{0xffec, 0x27, SCANCODE_E0_PREFIX}, 	/* Right Windows */
		{0xffbe, 0x05, 0},		/* F1 */
		{0xffbf, 0x06, 0},		/* F2 */
		{0xffc0, 0x04, 0},		/* F3 */
		{0xffc1, 0x0c, 0},		/* F4 */
		{0xffc2, 0x03, 0},		/* F5 */
		{0xffc3, 0x0b, 0},		/* F6 */
		{0xffc4, 0x83, 0},		/* F7 */
		{0xffc5, 0x0a, 0},		/* F8 */
		{0xffc6, 0x01, 0},		/* F9 */
		{0xffc7, 0x09, 0},		/* F10 */
		{0xffc8, 0x78, 0},		/* F11 */
		{0xffc9, 0x07, 0},		/* F12 */
		{0xffff, 0x71, SCANCODE_E0_PREFIX},	/* Del */
		{0xff14, 0x7e, 0},		/* ScrollLock */
		/* NumLock and Keypads*/
		{0xff7f, 0x77, 0}, 	/* NumLock */
		{0xffaf, 0x4a, SCANCODE_E0_PREFIX}, 	/* Keypad slash */
		{0xffaa, 0x7c, 0}, 	/* Keypad asterisk */
		{0xffad, 0x7b, 0}, 	/* Keypad minus */
		{0xffab, 0x79, 0}, 	/* Keypad plus */
		{0xffb7, 0x6c, 0}, 	/* Keypad 7 */
		{0xff95, 0x6c, 0}, 	/* Keypad home */
		{0xffb8, 0x75, 0}, 	/* Keypad 8 */
		{0xff97, 0x75, 0}, 	/* Keypad up arrow */
		{0xffb9, 0x7d, 0}, 	/* Keypad 9 */
		{0xff9a, 0x7d, 0}, 	/* Keypad PgUp */
		{0xffb4, 0x6b, 0}, 	/* Keypad 4 */
		{0xff96, 0x6b, 0}, 	/* Keypad left arrow */
		{0xffb5, 0x73, 0}, 	/* Keypad 5 */
		{0xff9d, 0x73, 0}, 	/* Keypad empty */
		{0xffb6, 0x74, 0}, 	/* Keypad 6 */
		{0xff98, 0x74, 0}, 	/* Keypad right arrow */
		{0xffb1, 0x69, 0}, 	/* Keypad 1 */
		{0xff9c, 0x69, 0}, 	/* Keypad end */
		{0xffb2, 0x72, 0}, 	/* Keypad 2 */
		{0xff99, 0x72, 0}, 	/* Keypad down arrow */
		{0xffb3, 0x7a, 0}, 	/* Keypad 3 */
		{0xff9b, 0x7a, 0}, 	/* Keypad PgDown */
		{0xffb0, 0x70, 0}, 	/* Keypad 0 */
		{0xff9e, 0x70, 0}, 	/* Keypad ins */
		{0xffae, 0x71, 0}, 	/* Keypad . */
		{0xff9f, 0x71, 0}, 	/* Keypad del */
		{0, 0, 0} 		/* Terminator */
};

/* ASCII to type 2 scancode lookup table */
static uint8_t ascii_translations[128] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x29, 0x16, 0x52, 0x26, 0x25, 0x2e, 0x3d, 0x52,
		0x46, 0x45, 0x3e, 0x55, 0x41, 0x4e, 0x49, 0x4a,
		0x45, 0x16, 0x1e, 0x26, 0x25, 0x2e, 0x36, 0x3d,
		0x3e, 0x46, 0x4c, 0x4c, 0x41, 0x55, 0x49, 0x4a,
		0x1e, 0x1c, 0x32, 0x21, 0x23, 0x24, 0x2b, 0x34,
		0x33, 0x43, 0x3b, 0x42, 0x4b, 0x3a, 0x31, 0x44,
		0x4d, 0x15, 0x2d, 0x1b, 0x2c, 0x3c, 0x2a, 0x1d,
		0x22, 0x35, 0x1a, 0x54, 0x5d, 0x5b, 0x36, 0x4e,
		0x0e, 0x1c, 0x32, 0x21, 0x23, 0x24, 0x2b, 0x34,
		0x33, 0x43, 0x3b, 0x42, 0x4b, 0x3a, 0x31, 0x44,
		0x4d, 0x15, 0x2d, 0x1b, 0x2c, 0x3c, 0x2a, 0x1d,
		0x22, 0x35, 0x1a, 0x54, 0x5d, 0x5b, 0x0e, 0x00,
};

/* ScanCode set1 to set2 lookup table */
static const uint8_t keyset1to2_translations[128] = {
		   0, 0x76, 0x16, 0x1E, 0x26, 0x25, 0x2e, 0x36,
		0x3d, 0x3e, 0x46, 0x45, 0x4e, 0x55, 0x66, 0x0d,
		0x15, 0x1d, 0x24, 0x2d, 0x2c, 0x35, 0x3c, 0x43,
		0x44, 0x4d, 0x54, 0x5b, 0x5a, 0x14, 0x1c, 0x1b,
		0x23, 0x2b, 0x34, 0x33, 0x3b, 0x42, 0x4b, 0x4c,
		0x52, 0x0e, 0x12, 0x5d, 0x1a, 0x22, 0x21, 0x2a,
		0x32, 0x31, 0x3a, 0x41, 0x49, 0x4a, 0x59, 0x7c,
		0x11, 0x29, 0x58, 0x05, 0x06, 0x04, 0x0c, 0x03,
		0x0b, 0x83, 0x0a, 0x01, 0x09, 0x77, 0x7e, 0x6c,
		0x75, 0x7d, 0x7b, 0x6b, 0x73, 0x74, 0x79, 0x69,
		0x72, 0x7a, 0x70, 0x71, 0x84, 0x60, 0x61, 0x78,
		0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0x3f,
		0x47, 0x4f, 0x56, 0x5e, 0x08, 0x10, 0x18, 0x20,
		0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x57, 0x6f,
		0x13, 0x19, 0x39, 0x51, 0x53, 0x5c, 0x5f, 0x62,
		0x63, 0x64, 0x65, 0x67, 0x68, 0x6a, 0x6d, 0x6e,
};

static void
fifo_init(struct ps2kbd_softc *sc)
{
	struct fifo *fifo;

	fifo = &sc->fifo;
	fifo->size = sizeof(((struct fifo *)0)->buf);
}

static void
fifo_reset(struct ps2kbd_softc *sc)
{
	struct fifo *fifo;

	fifo = &sc->fifo;
	bzero(fifo, sizeof(struct fifo));
	fifo->size = sizeof(((struct fifo *)0)->buf);
}

static void
fifo_put(struct ps2kbd_softc *sc, uint8_t val)
{
	struct fifo *fifo;

	fifo = &sc->fifo;
	if (fifo->num < fifo->size) {
		fifo->buf[fifo->windex] = val;
		fifo->windex = (fifo->windex + 1) % fifo->size;
		fifo->num++;
	}
}

static int
fifo_get(struct ps2kbd_softc *sc, uint8_t *val)
{
	struct fifo *fifo;

	fifo = &sc->fifo;
	if (fifo->num > 0) {
		*val = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->num--;
		return (0);
	}

	return (-1);
}

int
ps2kbd_read(struct ps2kbd_softc *sc, uint8_t *val)
{
	int retval;

	pthread_mutex_lock(&sc->mtx);
	retval = fifo_get(sc, val);
	pthread_mutex_unlock(&sc->mtx);

	return (retval);
}

void
ps2kbd_write(struct ps2kbd_softc *sc, uint8_t val)
{
	pthread_mutex_lock(&sc->mtx);
	if (sc->curcmd) {
		switch (sc->curcmd) {
		case PS2KC_SET_TYPEMATIC:
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_SET_SCANCODE_SET:
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_SET_LEDS:
			fifo_put(sc, PS2KC_ACK);
			break;
		default:
			EPRINTLN("Unhandled ps2 keyboard current "
			    "command byte 0x%02x", val);
			break;
		}
		sc->curcmd = 0;
	} else {
		switch (val) {
		case 0x00:
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_RESET_DEV:
			fifo_reset(sc);
			fifo_put(sc, PS2KC_ACK);
			fifo_put(sc, PS2KC_BAT_SUCCESS);
			break;
		case PS2KC_DISABLE:
			sc->enabled = false;
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_ENABLE:
			sc->enabled = true;
			fifo_reset(sc);
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_SET_TYPEMATIC:
			sc->curcmd = val;
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_SEND_DEV_ID:
			fifo_put(sc, PS2KC_ACK);
			fifo_put(sc, 0xab);
			fifo_put(sc, 0x83);
			break;
		case PS2KC_SET_SCANCODE_SET:
			sc->curcmd = val;
			fifo_put(sc, PS2KC_ACK);
			break;
		case PS2KC_ECHO:
			fifo_put(sc, PS2KC_ECHO);
			break;
		case PS2KC_SET_LEDS:
			sc->curcmd = val;
			fifo_put(sc, PS2KC_ACK);
			break;
		default:
			EPRINTLN("Unhandled ps2 keyboard command "
			    "0x%02x", val);
			break;
		}
	}
	pthread_mutex_unlock(&sc->mtx);
}

/*
 * Translate keysym to type 2 scancode and insert into keyboard buffer.
 */
static void
ps2kbd_keysym_queue(struct ps2kbd_softc *sc,
    int down, uint32_t keysym, uint32_t keycode)
{
	const struct extended_translation *trans;
	int e0_prefix, found;
	uint8_t code;

	assert(pthread_mutex_isowned_np(&sc->mtx));

	if (keycode) {
		code =  keyset1to2_translations[(uint8_t)(keycode & 0x7f)];
		e0_prefix = ((keycode & 0x80) ?  SCANCODE_E0_PREFIX : 0);
		found = 1;
	} else {
		found = 0;
		if (keysym < 0x80) {
			code = ascii_translations[keysym];
			e0_prefix = 0;
			found = 1;
		} else {
			for (trans = &extended_translations[0];
			    trans->keysym != 0; trans++) {
				if (keysym == trans->keysym) {
					code = trans->scancode;
					e0_prefix = trans->flags & SCANCODE_E0_PREFIX;
					found = 1;
					break;
				}
			}
		}
	}

	if (!found) {
		EPRINTLN("Unhandled ps2 keyboard keysym 0x%x", keysym);
		return;
	}

	if (e0_prefix)
		fifo_put(sc, 0xe0);
	if (!down)
		fifo_put(sc, 0xf0);
	fifo_put(sc, code);
}

static void
ps2kbd_event(int down, uint32_t keysym, uint32_t keycode, void *arg)
{
	struct ps2kbd_softc *sc = arg;
	int fifo_full;

	pthread_mutex_lock(&sc->mtx);
	if (!sc->enabled) {
		pthread_mutex_unlock(&sc->mtx);
		return;
	}
	fifo_full = sc->fifo.num == PS2KBD_FIFOSZ;
	ps2kbd_keysym_queue(sc, down, keysym, keycode);
	pthread_mutex_unlock(&sc->mtx);

	if (!fifo_full)
		atkbdc_event(sc->atkbdc_sc, 1);
}

static void
ps2kbd_update_extended_translation(uint32_t keycode, uint32_t scancode, uint32_t prefix)
{
	int i = 0;

	do {
		if (extended_translations[i].keysym == keycode)
			break;
	} while (extended_translations[++i].keysym);

	if (i == (sizeof(extended_translations) / sizeof(struct extended_translation) - 1))
		return;

	if (!extended_translations[i].keysym)	{
		extended_translations[i].keysym = keycode;

		extended_translations[i+1].keysym = 0;
		extended_translations[i+1].scancode = 0;
		extended_translations[i+1].flags = 0;
	}

	extended_translations[i].scancode = (uint8_t)(scancode & 0xff);
	extended_translations[i].flags = (prefix ? SCANCODE_E0_PREFIX : 0);
}

static void
ps2kbd_setkbdlayout(void)
{
	int err;
	int fd;
	char path[MAX_PATHNAME];
	char *buf, *next, *line;
	struct stat sb;
	ssize_t sz;
	uint8_t ascii;
	uint32_t keycode, scancode, prefix;

	snprintf(path, MAX_PATHNAME, PS2KBD_LAYOUT_BASEDIR"%s", get_config_value("keyboard.layout") );

	err = stat(path, &sb);
	if (err)
		return;

	buf = (char *)malloc(sizeof(char) * sb.st_size);
	if (buf == NULL)
		return;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		goto out;

	sz = read(fd, buf, sb.st_size);

	close(fd);

	if (sz < 0 || sz != sb.st_size)
		goto out;

	next = buf;
	while ((line = strsep(&next, "\n")) != NULL)	{
		if (sscanf(line, "'%c',%x;", &ascii, &scancode) == 2)	{
			if (ascii < 0x80)
				ascii_translations[ascii] = (uint8_t)(scancode & 0xff);
		} else if (sscanf(line, "%x,%x,%x;", &keycode, &scancode, &prefix) == 3 )	{
			ps2kbd_update_extended_translation(keycode, scancode, prefix);
		} else if (sscanf(line, "%x,%x;", &keycode, &scancode) == 2)	{
			if (keycode < 0x80)
				ascii_translations[(uint8_t)(keycode & 0xff)] = (uint8_t)(scancode & 0xff);
			else
				ps2kbd_update_extended_translation(keycode, scancode, 0);
		}
	}

out:
	free(buf);
}

struct ps2kbd_softc *
ps2kbd_init(struct atkbdc_softc *atkbdc_sc)
{
	struct ps2kbd_softc *sc;

	if (get_config_value("keyboard.layout") != NULL)
		ps2kbd_setkbdlayout();

	sc = calloc(1, sizeof (struct ps2kbd_softc));
	pthread_mutex_init(&sc->mtx, NULL);
	fifo_init(sc);
	sc->atkbdc_sc = atkbdc_sc;

	console_kbd_register(ps2kbd_event, sc, 1);

	return (sc);
}

#ifdef BHYVE_SNAPSHOT
int
ps2kbd_snapshot(struct ps2kbd_softc *sc, struct vm_snapshot_meta *meta)
{
	int ret;

	SNAPSHOT_VAR_OR_LEAVE(sc->enabled, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->curcmd, meta, ret, done);

done:
	return (ret);
}
#endif

