/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

/*
 * This uses 'printf' family functions, which can cause issues
 * for size-critical applications.  I've separated it out to make
 * this issue clear.  (Currently, it is called directly from within
 * the core code, so it cannot easily be omitted.)
 */

#include <stdio.h>

#include "archive_string.h"

/*
 * Like 'vsprintf', but ensures the target is big enough, resizing if
 * necessary.
 */
void
__archive_string_vsprintf(struct archive_string *as, const char *fmt,
    va_list ap)
{
	size_t l;
	va_list ap1;

	if (fmt == NULL) {
		as->s[0] = 0;
		return;
	}

	va_copy(ap1, ap);
	l = vsnprintf(as->s, as->buffer_length, fmt, ap);
	/* If output is bigger than the buffer, resize and try again. */
	if (l+1 >= as->buffer_length) {
		__archive_string_ensure(as, l + 1);
		l = vsnprintf(as->s, as->buffer_length, fmt, ap1);
	}
	as->length = l;
	va_end(ap1);
}

/*
 * Corresponding 'sprintf' interface.
 */
void
__archive_string_sprintf(struct archive_string *as, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	__archive_string_vsprintf(as, fmt, ap);
	va_end(ap);
}
