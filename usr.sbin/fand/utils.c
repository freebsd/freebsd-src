/*-
 * Copyright (c) 2026 Stéphane Rochoy <sheda@FreeBSD.org>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

static int
strtouint(const char *str, unsigned int *value, const char **errstr)
{
	const char *es;
	long long   v;

	v = strtonum(str, 0, UINT_MAX, &es);
	if (es != NULL) {
		if (errstr != NULL)
			*errstr = es;
		return (-1);
	}
	if (value != NULL)
		*value = (unsigned int)v;
	return (0);
}

/*
 * Convert the given string to duty cycle. On success, the duty cycle is stored
 * in *value.
 * When percent is non-NULL:
 * - the duty cycle can be expressed as a percentage
 * - and *percent tells if *value is a percentage or not.
 *
 * WARNING: str may be modified.
 */
int
strtoduty(char *str, unsigned int *value, bool *percent, const char **errstr)
{
	unsigned int v;
	bool         p = false;

	if (percent != NULL) {
		size_t len;

		len = strlen(str);
		if (str[len - 1] == '%') {
			str[len - 1] = '\0';
			p = true;
		}
	}
	if (strtouint(str, &v, errstr) == -1)
		return (-1);
	if (percent != NULL) {
		if (p && v > 100)
			return (-1);
		*percent = p;
	}
	if (value != NULL)
		*value = v;
	return (0);
}
