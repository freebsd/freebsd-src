/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
strfroml(char * __restrict s, size_t n, const char * __restrict fmt,
    long double fp)
{
	size_t i;

	/*
	 * Advance past '%' to locate the conversion specifier, then past the
	 * optional '.' and any decimal digits.  This scan is not validation,
	 * so use of an invalid format string is UB per C23 §7.24.1.3 and no
	 * error is signalled.
	 */
	i = 1;
	if (fmt[i] == '.')
		for (++i; (unsigned)(fmt[i] - '0') <= 9u; ++i)
			continue;

	/*
	 * Insert 'L' before the conversion specifier so snprintf treats the
	 * argument as long double as the caller's format intentionally omits
	 * the length modifier per C23 §7.24.1.3.
	 */
	char ff[i + 3];
	memcpy(ff, fmt, i);
	ff[i] = 'L';
	ff[i + 1] = fmt[i];
	ff[i + 2] = '\0';

	return (snprintf(s, n, ff, fp));
}
