/*-
 * Copyright (c) 2002 Jonathan Mini <mini@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "thr_private.h"

static void	pchar(int fd, char c);
static void	pstr(int fd, const char *s);

/*
 * Write formatted output to stdout, in a thread-safe manner.
 *
 * Recognises the following conversions:
 *	%c	-> char
 *	%d	-> signed int (base 10)
 *	%s	-> string
 *	%u	-> unsigned int (base 10)
 *	%x	-> unsigned int (base 16)
 *	%p	-> unsigned int (base 16)
 */
void
_thread_printf(int fd, const char *fmt, ...)
{
	static const char digits[16] = "0123456789abcdef";
	va_list	 ap;
	char buf[10];
	char *s;
	unsigned r, u;
	int c, d;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				pchar(fd, va_arg(ap, int));
				continue;
			case 's':
				pstr(fd, va_arg(ap, char *));
				continue;
			case 'd':
			case 'u':
			case 'p':
			case 'x':
				r = ((c == 'u') || (c == 'd')) ? 10 : 16;
				if (c == 'd') {
					d = va_arg(ap, unsigned);
					if (d < 0) {
						pchar(fd, '-');
						u = (unsigned)(d * -1);
					} else
						u = (unsigned)d;
				} else
					u = va_arg(ap, unsigned);
				s = buf;
				do {
					*s++ = digits[u % r];
				} while (u /= r);
				while (--s >= buf)
					pchar(fd, *s);
				continue;
			}
		}
		pchar(fd, c);
	}
	va_end(ap);
}

/*
 * Write a single character to stdout, in a thread-safe manner.
 */
static void
pchar(int fd, char c)
{

	__sys_write(fd, &c, 1);
}

/*
 * Write a string to stdout, in a thread-safe manner.
 */
static void
pstr(int fd, const char *s)
{

	__sys_write(fd, s, strlen(s));
}

