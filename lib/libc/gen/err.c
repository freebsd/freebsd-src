/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "namespace.h"
#include <sys/exterrvar.h>
#include <err.h>
#include <errno.h>
#include <exterr.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un-namespace.h"

#include "libc_private.h"

static FILE *err_file; /* file to use for error output */
static void (*err_exit)(int);

static void verrci(bool doexterr, int eval, int code, const char *fmt,
    va_list ap) __printf0like(4, 0) __dead2;
static void vwarnci(bool doexterr, int code, const char *fmt, va_list ap)
    __printf0like(3, 0);

/*
 * This is declared to take a `void *' so that the caller is not required
 * to include <stdio.h> first.  However, it is really a `FILE *', and the
 * manual page documents it as such.
 */
void
err_set_file(void *fp)
{
	if (fp)
		err_file = fp;
	else
		err_file = stderr;
}

void
err_set_exit(void (*ef)(int))
{
	err_exit = ef;
}

__weak_reference(_err, err);

void
_err(int eval, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verrci(true, eval, errno, fmt, ap);
	va_end(ap);
}

void
verr(int eval, const char *fmt, va_list ap)
{
	verrci(true, eval, errno, fmt, ap);
}

void
errc(int eval, int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verrci(false, eval, code, fmt, ap);
	va_end(ap);
}

void
verrc(int eval, int code, const char *fmt, va_list ap)
{
	verrci(false, eval, code, fmt, ap);
}

static void
vexterr(bool doexterr, int code, const char *fmt, va_list ap)
{
	char exterr[UEXTERROR_MAXLEN];	/* libc knows the buffer size */
	int extstatus;

	if (doexterr)
		extstatus = uexterr_gettext(exterr, sizeof(exterr));
	if (err_file == NULL)
		err_set_file(NULL);
	fprintf(err_file, "%s: ", _getprogname());
	if (fmt != NULL) {
		vfprintf(err_file, fmt, ap);
		fprintf(err_file, ": ");
	}
	fprintf(err_file, "%s", strerror(code));
	if (doexterr && extstatus == 0 && exterr[0] != '\0')
		fprintf(err_file, " (extended error %s)", exterr);
	fprintf(err_file, "\n");
}

static void
verrci(bool doexterr, int eval, int code, const char *fmt, va_list ap)
{
	vexterr(doexterr, code, fmt, ap);
	if (err_exit)
		err_exit(eval);
	exit(eval);
}

void
errx(int eval, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verrx(eval, fmt, ap);
	va_end(ap);
}

void
verrx(int eval, const char *fmt, va_list ap)
{
	if (err_file == NULL)
		err_set_file(NULL);
	fprintf(err_file, "%s: ", _getprogname());
	if (fmt != NULL)
		vfprintf(err_file, fmt, ap);
	fprintf(err_file, "\n");
	if (err_exit)
		err_exit(eval);
	exit(eval);
}

__weak_reference(_warn, warn);

void
_warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnc(errno, fmt, ap);
	va_end(ap);
}

void
vwarn(const char *fmt, va_list ap)
{
	vwarnc(errno, fmt, ap);
}

void
warnc(int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnc(code, fmt, ap);
	va_end(ap);
}

void
vwarnc(int code, const char *fmt, va_list ap)
{
	vwarnci(false, code, fmt, ap);
}

static void
vwarnci(bool doexterr, int code, const char *fmt, va_list ap)
{
	int saved_errno;

	saved_errno = errno;
	vexterr(doexterr, code, fmt, ap);
	errno = saved_errno;
}

void
warnx(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

void
vwarnx(const char *fmt, va_list ap)
{
	int saved_errno;

	saved_errno = errno;
	if (err_file == NULL)
		err_set_file(NULL);
	fprintf(err_file, "%s: ", _getprogname());
	if (fmt != NULL)
		vfprintf(err_file, fmt, ap);
	fprintf(err_file, "\n");
	errno = saved_errno;
}
