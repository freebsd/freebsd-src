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

/*
 * Minimal libsbuf reimplementation for Mac OS X.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "sbuf.h"

#define	SBUF_INITIAL_SIZE	128

struct sbuf *
sbuf_new_auto()
{
	struct sbuf *s;

	s = malloc(sizeof(struct sbuf));
	s->s_buf = calloc(1, SBUF_INITIAL_SIZE + 1);
	s->s_capacity = s->s_buf != NULL ? SBUF_INITIAL_SIZE : 0;
	s->s_size = 0;

	return (s);
}

int
sbuf_cat(struct sbuf *s, const char *str)
{
	int req = (int)strlen(str);

	if (s->s_size + req >= s->s_capacity) {
		s->s_capacity = s->s_size + req + 1;
		s->s_buf = realloc(s->s_buf, (size_t)s->s_capacity);
	}
	if (s->s_buf == NULL)
		return (-1);

	strcpy(s->s_buf + s->s_size, str);
	s->s_size += req;

	return (0);
}

int
sbuf_printf(struct sbuf *s, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = sbuf_vprintf(s, fmt, ap);
	va_end(ap);

	return (ret);
}

int
sbuf_vprintf(struct sbuf *s, const char *fmt, va_list args)
{
	va_list copy;
	int req;

	va_copy(copy, args);
	req = vsnprintf(NULL, 0, fmt, copy);
	va_end(copy);

	if (s->s_size + req >= s->s_capacity) {
		s->s_capacity = s->s_size + req + 1;
		s->s_buf = realloc(s->s_buf, (size_t)s->s_capacity);
	}
	if (s->s_buf == NULL)
		return (-1);

	req = vsnprintf(s->s_buf + s->s_size, req + 1, fmt, args);
	s->s_size += req;

	return (0);
}

char *
sbuf_data(struct sbuf *s)
{
	return (s->s_buf);
}

int
sbuf_finish(struct sbuf *s)
{
	if (s->s_buf != NULL)
		s->s_buf[s->s_size] = '\0';
	return (0);
}

void
sbuf_delete(struct sbuf *s)
{
	free(s->s_buf);
	free(s);
}
