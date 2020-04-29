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

#ifndef LIB9P_SBUF_H
#define LIB9P_SBUF_H

#include <stdarg.h>

struct sbuf
{
	char *s_buf;
	int s_size;
	int s_capacity;
	int s_position;
};

struct sbuf *sbuf_new_auto(void);
int sbuf_cat(struct sbuf *s, const char *str);
int sbuf_printf(struct sbuf *s, const char *fmt, ...);
int sbuf_vprintf(struct sbuf *s, const char *fmt, va_list args);
int sbuf_done(struct sbuf *s);
void sbuf_delete(struct sbuf *s);
int sbuf_finish(struct sbuf *s);
char *sbuf_data(struct sbuf *s);

#endif /* LIB9P_SBUF_H */

