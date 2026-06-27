/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Internal declarations for strfrom* numeric conversion (C23 §7.24.1.3).
 *
 * Note: gdtoa dtoa() modes used:
 *   2 – ndigits significant digits      (%e, %g)
 *   3 – ndigits digits after '.'        (%f)
 */

#ifndef STRFROM_H
#define	STRFROM_H

#include <limits.h>
#include <stddef.h>

#define	dtoa		__dtoa
#define	freedtoa	__freedtoa

#include "../stdio/floatio.h"
#include "gdtoa.h"

char	__sf_parse_fmt(const char *, int *);
int	__sf_edoofus(char *, size_t);
const char *__sf_xdigits(char);
void	__sf_decimal_mode(char, int, int *, int *);
int	__sf_render_hex(char *, size_t, char, int, char *, char *, int, int);
int	__sf_render_decimal(char *, size_t, char, char, int, char *, char *,
	    int, int, int);

#endif /* STRFROM_H */
