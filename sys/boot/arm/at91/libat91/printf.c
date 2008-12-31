/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2006 M. Warner Losh
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
 *
 * $FreeBSD: src/sys/boot/arm/at91/libat91/printf.c,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <stdarg.h>
#include "lib.h"

void
printf(const char *fmt,...)
{
	va_list ap;
	const char *hex = "0123456789abcdef";
	char buf[10];
	char *s;
	unsigned u;
	int c;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				xputchar(va_arg(ap, int));
				continue;
			case 's':
				for (s = va_arg(ap, char *); *s; s++)
					xputchar(*s);
				continue;
			case 'd':	/* A lie, always prints unsigned */
			case 'u':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = '0' + u % 10U;
				while (u /= 10U);
			dumpbuf:;
				while (--s >= buf)
					xputchar(*s);
				continue;
			case 'x':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = hex[u & 0xfu];
				while (u >>= 4);
				goto dumpbuf;
			}
		}
		xputchar(c);
	}
	va_end(ap);

	return;
}
