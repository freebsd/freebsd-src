/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "log.h"

static const char *l9p_log_level_names[] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
};

void
l9p_logf(enum l9p_log_level level, const char *func, const char *fmt, ...)
{
	const char *dest = NULL;
	static FILE *stream = NULL;
	va_list ap;

	if (stream == NULL) {
		dest = getenv("LIB9P_LOGGING");
		if (dest == NULL)
			return;
		else if (!strcmp(dest, "stderr"))
			stream = stderr;
		else {
			stream = fopen(dest, "a");
			if (stream == NULL)
				return;
		}
	}

	va_start(ap, fmt);
	fprintf(stream, "[%s]\t %s: ", l9p_log_level_names[level], func);
	vfprintf(stream, fmt, ap);
	fprintf(stream, "\n");
	fflush(stream);
	va_end(ap);
}
