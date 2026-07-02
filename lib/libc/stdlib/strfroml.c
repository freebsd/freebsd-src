/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdlib.h>

#include "strfrom.h"

int
strfroml(char * __restrict s, size_t n, const char * __restrict fmt,
    long double fp)
{
	char conv, lc, *digits, *dend;
	int prec, decpt, signflag, ret, mode, ndig_req;

	conv = sf_parse_fmt(fmt, &prec);
	lc = conv | 0x20;

	if (lc == 'a') {
		digits = __hldtoa(fp, sf_xdigits(conv),
		    prec >= 0 ? prec + 1 : -1, &decpt, &signflag, &dend);
		ret = sf_render_hex(s, n, conv, prec, digits, dend, decpt,
		    signflag);
		freedtoa(digits);
		return (ret);
	}

	if (prec < 0)
		prec = 6;

	sf_decimal_mode(lc, prec, &mode, &ndig_req);

	digits = __ldtoa(&fp, mode, ndig_req, &decpt, &signflag, &dend);

	ret = sf_render_decimal(s, n, conv, lc, prec, digits, dend, decpt,
	    signflag, decpt == INT_MAX);

	freedtoa(digits);

	return (ret);
}
