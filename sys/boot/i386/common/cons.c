/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <machine/psl.h>

#include <btxv86.h>

#include "lib.h"
#include "rbx.h"
#include "util.h"
#include "cons.h"

#define SECOND		18	/* Circa that many ticks in a second. */

uint8_t ioctrl = IO_KEYBOARD;

void
putc(int c)
{

	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0xe00 | (c & 0xff);
	v86.ebx = 0x7;
	v86int();
}

void
xputc(int c)
{

	if (ioctrl & IO_KEYBOARD)
		putc(c);
	if (ioctrl & IO_SERIAL)
		sio_putc(c);
}

void
putchar(int c)
{

	if (c == '\n')
		xputc('\r');
	xputc(c);
}

int
getc(int fn)
{

	/*
	 * The extra comparison against zero is an attempt to work around
	 * what appears to be a bug in QEMU and Bochs. Both emulators
	 * sometimes report a key-press with scancode one and ascii zero
	 * when no such key is pressed in reality. As far as I can tell,
	 * this only happens shortly after a reboot.
	 */
	v86.ctl = V86_FLAGS;
	v86.addr = 0x16;
	v86.eax = fn << 8;
	v86int();
	return fn == 0 ? v86.eax & 0xff : (!V86_ZR(v86.efl) && (v86.eax & 0xff));
}

int
xgetc(int fn)
{

	if (OPT_CHECK(RBX_NOINTR))
		return (0);
	for (;;) {
		if (ioctrl & IO_KEYBOARD && getc(1))
			return (fn ? 1 : getc(0));
		if (ioctrl & IO_SERIAL && sio_ischar())
			return (fn ? 1 : sio_getc());
		if (fn)
			return (0);
	}
	/* NOTREACHED */
}

int
keyhit(unsigned int secs)
{
	uint32_t t0, t1;

	if (OPT_CHECK(RBX_NOINTR))
		return (0);
	secs *= SECOND;
	t0 = 0;
	for (;;) {
		if (xgetc(1))
			return (1);
		if (secs > 0) {
			t1 = *(uint32_t *)PTOV(0x46c);
			if (!t0)
				t0 = t1;
			if (t1 < t0 || t1 >= t0 + secs)
				return (0);
		}
	}
	/* NOTREACHED */
}

void
getstr(char *cmdstr, size_t cmdstrsize)
{
	char *s;
	int c;

	s = cmdstr;
	for (;;) {
		switch (c = xgetc(0)) {
		case 0:
			break;
		case '\177':
		case '\b':
			if (s > cmdstr) {
				s--;
				printf("\b \b");
			}
			break;
		case '\n':
		case '\r':
			*s = 0;
			return;
		default:
			if (s - cmdstr < cmdstrsize - 1)
				*s++ = c;
			putchar(c);
			break;
		}
	}
}
