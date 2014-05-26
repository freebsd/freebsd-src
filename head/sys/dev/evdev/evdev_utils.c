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

static uint16_t evdev_at_set1_scancodes[256] = {
	NONE,
};
	

inline uint16_t
evdev_hid2key(int scancode)
{
	return evdev_usb_scancodes[scancode];
}

inline uint16_t
evdev_at2key(int scancode)
{
	return evdev_at_set1_scancodes[scancode];
}
