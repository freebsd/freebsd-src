/* $Id: compat_vasprintf.c,v 1.4 2020/06/15 01:37:15 schwarze Exp $ */
/*
 * Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * This fallback implementation is not efficient:
 * It does the formatting twice.
 * Short of fiddling with the unknown internals of the system's
 * printf(3) or completely reimplementing printf(3), i can't think
 * of another portable solution.
 */
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int
vasprintf(char **ret, const char *format, va_list ap)
{
	char	 buf[2];
	va_list	 ap2;
	int	 sz;

	va_copy(ap2, ap);
	sz = vsnprintf(buf, sizeof(buf), format, ap2);
	va_end(ap2);

	if (sz != -1 && (*ret = malloc(sz + 1)) != NULL) {
		if (vsnprintf(*ret, sz + 1, format, ap) == sz)
			return sz;
		free(*ret);
	}
	*ret = NULL;
	return -1;
}
