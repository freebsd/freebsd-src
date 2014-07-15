/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define	NONE	KEY_RESERVED

static uint16_t evdev_usb_scancodes[256] = {
	/* 0x00 - 0x27 */
	NONE,	NONE,	NONE,	NONE,	KEY_A,	KEY_B,	KEY_C,	KEY_D,
	KEY_E,	KEY_F,	KEY_G,	KEY_H,	KEY_I,	KEY_J,	KEY_K,	KEY_L,
	KEY_M,	KEY_N,	KEY_O,	KEY_P,	KEY_Q,	KEY_R,	KEY_S,	KEY_T,
	KEY_U,	KEY_V,	KEY_W,	KEY_X,	KEY_Y,	KEY_Z,	KEY_1,	KEY_2,
	KEY_3,	KEY_4,	KEY_5,	KEY_6,	KEY_7,	KEY_8,	KEY_9,	KEY_0,
	/* 0x28 - 0x3f */
	KEY_ENTER,	KEY_ESC,	KEY_BACKSPACE,	KEY_TAB, 
	KEY_SPACE,	KEY_MINUS,	KEY_EQUAL,	KEY_LEFTBRACE, 
	KEY_RIGHTBRACE,	KEY_BACKSLASH,	NONE,		KEY_SEMICOLON,
	KEY_APOSTROPHE,	KEY_GRAVE,	KEY_COMMA,	KEY_DOT, 
	KEY_SLASH,	KEY_CAPSLOCK,	KEY_F1,		KEY_F2,
	KEY_F3,		KEY_F4,		KEY_F5,		KEY_F6,
	/* 0x40 - 0x5f */
	KEY_F7, 	KEY_F8,		KEY_F9,		KEY_F10, 
	KEY_F11, 	KEY_F12,	KEY_SYSRQ,	KEY_SCROLLLOCK,
	KEY_PAUSE,	KEY_INSERT,	KEY_HOME,	KEY_PAGEUP,
	KEY_DELETE,	KEY_END,	KEY_PAGEDOWN,	KEY_RIGHT,
	KEY_LEFT,	KEY_DOWN,	KEY_UP,		KEY_NUMLOCK,
	KEY_SLASH,	KEY_KPASTERISK,	KEY_KPMINUS,	KEY_KPPLUS,
	KEY_KPENTER,	KEY_KP1,	KEY_KP2,	KEY_KP3,
	KEY_KP4,	NONE,		KEY_KP6,	KEY_KP7,
	/* 0x60 - 0x7f */
	KEY_KP8,	KEY_KP9,	KEY_KP0,	KEY_KPDOT,
	NONE,		NONE, /* XXX */	KEY_F13,	KEY_F14,
	KEY_F15,	KEY_F16,	KEY_F17,	KEY_F18,
	KEY_F19,	KEY_F20,	KEY_F21,	KEY_F22,
	KEY_F23,	KEY_F24,	KEY_HELP,	KEY_UNDO,
	KEY_CUT,	KEY_COPY,	KEY_PASTE,	KEY_MUTE,
	KEY_VOLUMEUP,	KEY_VOLUMEDOWN,	NONE,		KEY_KATAKANA,
	NONE,		NONE,		KEY_HIRAGANA,	NONE,
	/* 0x80 - 0x9f */
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		KEY_KP5,	NONE,
	NONE,		KEY_SYSRQ,	NONE,		KEY_CLEAR,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,	
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	/* 0xa0 - 0xbf */
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE, 		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	/* 0xc0 - 0xdf */
	NONE,		NONE,		NONE,		NONE,
	NONE, 		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	/* 0xe0 - 0xff */
	KEY_LEFTCTRL,	KEY_LEFTSHIFT,	KEY_LEFTALT,	KEY_LEFTMETA,
	KEY_RIGHTCTRL,	KEY_RIGHTSHIFT,	KEY_RIGHTALT,	KEY_RIGHTMETA,	
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE, 		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,
	NONE,		NONE,		NONE,		NONE,

};

static uint16_t evdev_at_set1_scancodes[] = {
	/* 0x00 - 0x1f */
	NONE,		KEY_ESC,	KEY_1,		KEY_2,
	KEY_3,		KEY_4,		KEY_5,		KEY_6,
	KEY_7,		KEY_8,		KEY_9,		KEY_0,
	KEY_MINUS,	KEY_EQUAL,	KEY_BACKSPACE,	KEY_TAB,
	KEY_Q,		KEY_W,		KEY_E,		KEY_R,
	KEY_T,		KEY_Y,		KEY_U,		KEY_I,
	KEY_O,		KEY_P,		KEY_LEFTBRACE,	KEY_RIGHTBRACE,
	KEY_ENTER,	KEY_LEFTCTRL,	KEY_A,		KEY_S,
	/* 0x20 - 0x3f */
	KEY_D,		KEY_F,		KEY_G,		KEY_H,
	KEY_J,		KEY_K,		KEY_L,		KEY_SEMICOLON,
	KEY_APOSTROPHE,	KEY_GRAVE,	KEY_LEFTSHIFT,	KEY_BACKSLASH,
	KEY_Z,		KEY_X,		KEY_C,		KEY_V,
	KEY_B,		KEY_N,		KEY_M,		KEY_COMMA,
	KEY_DOT,	KEY_SLASH,	KEY_RIGHTSHIFT,	NONE,
	KEY_LEFTALT,	KEY_SPACE,	KEY_CAPSLOCK,	KEY_F1,
	KEY_F2,		KEY_F3,		KEY_F4,		KEY_F5,
	/* 0x40 - 0x5f */
	KEY_F6,		KEY_F7,		KEY_F8,		KEY_F9,
	KEY_F10,	KEY_NUMLOCK,	KEY_SCROLLLOCK,	KEY_KP7,
	KEY_KP8,	KEY_KP9,	KEY_KPMINUS,	KEY_KP4,
	KEY_KP5,	KEY_KP6,	KEY_KPPLUS,	KEY_KP1,
	KEY_KP2,	KEY_KP3,	KEY_KP0,	KEY_KPDOT,
	NONE,		NONE,		NONE,		KEY_F11,
	KEY_F12,	NONE,		NONE,		NONE,
	NONE, 		NONE,		NONE,		NONE,
};


inline uint16_t
evdev_hid2key(int scancode)
{
	return evdev_usb_scancodes[scancode];
}

inline uint16_t
evdev_scancode2key(int *state, int scancode)
{
	uint16_t keycode;

	/* translate the scan code into a keycode */
	keycode = evdev_at_set1_scancodes[scancode & 0x7f];
	switch (*state) {
	case 0x00:	/* normal scancode */
		switch(scancode) {
		case 0xE0:
		case 0xE1:
			*state = scancode;
			return (NONE);
		}
		break;
	case 0xE0:		/* 0xE0 prefix */
		*state = 0;
		switch (scancode & 0x7f) {
		case 0x1C:	/* right enter key */
			keycode = KEY_KPENTER;
			break;
		case 0x1D:	/* right ctrl key */
			keycode = KEY_RIGHTCTRL;
			break;
		case 0x35:	/* keypad divide key */
			keycode = KEY_KPASTERISK;
			break;
		case 0x37:	/* print scrn key */
			keycode = KEY_SYSRQ;
			break;
		case 0x38:	/* right alt key (alt gr) */
			keycode = KEY_RIGHTALT;
			break;
		case 0x46:	/* ctrl-pause/break on AT 101 (see below) */
			keycode = KEY_PAUSE;
			break;
		case 0x47:	/* grey home key */
			keycode = KEY_HOME;
			break;
		case 0x48:	/* grey up arrow key */
			keycode = KEY_UP;
			break;
		case 0x49:	/* grey page up key */
			keycode = KEY_PAGEUP;
			break;
		case 0x4B:	/* grey left arrow key */
			keycode = KEY_LEFT;
			break;
		case 0x4D:	/* grey right arrow key */
			keycode = KEY_RIGHT;
			break;
		case 0x4F:	/* grey end key */
			keycode = KEY_END;
			break;
		case 0x50:	/* grey down arrow key */
			keycode = KEY_DOWN;
			break;
		case 0x51:	/* grey page down key */
			keycode = KEY_PAGEDOWN;
			break;
		case 0x52:	/* grey insert key */
			keycode = KEY_INSERT;
			break;
		case 0x53:	/* grey delete key */
			keycode = KEY_DELETE;
			break;
			/* the following 3 are only used on the MS "Natural" keyboard */
		case 0x5b:	/* left Window key */
			keycode = KEY_LEFTMETA;
			break;
		case 0x5c:	/* right Window key */
			keycode = KEY_RIGHTMETA;
			break;
		case 0x5d:	/* menu key */
			keycode = KEY_MENU;
			break;
		case 0x5e:	/* power key */
			keycode = KEY_POWER;
			break;
		case 0x5f:	/* sleep key */
			keycode = KEY_SLEEP;
			break;
		case 0x63:	/* wake key */
			keycode = KEY_WAKEUP;
			break;
		default:	/* ignore everything else */
			return (NONE);
		}
		break;
   	case 0xE1:	/* 0xE1 prefix */
		/* 
		 * The pause/break key on the 101 keyboard produces:
		 * E1-1D-45 E1-9D-C5
		 * Ctrl-pause/break produces:
		 * E0-46 E0-C6 (See above.)
		 */
		*state = 0;
		if ((scancode & 0x7f) == 0x1D)
			*state = 0x1D;
		return (NONE);
		/* NOT REACHED */
   	case 0x1D:	/* pause / break */
		*state = 0;
		if (scancode != 0x45)
			return (NONE);
		keycode = KEY_PAUSE;
		break;
	}

	return (keycode);
}
