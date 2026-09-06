/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Derived from contrib/bearssl/tools/ for generating compile-time trust
 * anchors from CA certificates.
 */

#include <stdlib.h>
#include <string.h>

#include "brssl.h"

static int
is_ign(int c)
{
	if (c == 0)
		return 0;
	if (c <= 32 || c == '-' || c == '_' || c == '.' || c == '/' ||
	    c == '+' || c == ':')
		return 1;
	return 0;
}

static int
next_char(const char **ps, const char *limit)
{
	for (;;) {
		int c;

		if (*ps == limit)
			return 0;
		c = *(*ps)++;
		if (c == 0)
			return 0;
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		if (!is_ign(c))
			return c;
	}
}

static int
eqstr_chunk(const char *s1, size_t s1_len, const char *s2, size_t s2_len)
{
	const char *lim1, *lim2;

	lim1 = s1 + s1_len;
	lim2 = s2 + s2_len;
	for (;;) {
		int c1, c2;

		c1 = next_char(&s1, lim1);
		c2 = next_char(&s2, lim2);
		if (c1 != c2)
			return 0;
		if (c1 == 0)
			return 1;
	}
}

int
eqstr(const char *s1, const char *s2)
{
	return eqstr_chunk(s1, strlen(s1), s2, strlen(s2));
}

int
main(int argc, char *argv[])
{
	return (do_ta(argc - 1, argv + 1) < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
