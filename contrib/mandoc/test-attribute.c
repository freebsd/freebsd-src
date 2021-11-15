/* $Id: test-attribute.c,v 1.1 2020/06/22 20:00:38 schwarze Exp $ */
/*
 * Copyright (c) 2020 Ingo Schwarze <schwarze@openbsd.org>
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
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void	 var_arg(const char *, ...)
		__attribute__((__format__ (__printf__, 1, 2)));
void	 no_ret(int)
		__attribute__((__noreturn__));

void
var_arg(const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void
no_ret(int i)
{
	exit(i);
}

int
main(void)
{
	var_arg("Test output: %d\n", 42);
	no_ret(0);
}
