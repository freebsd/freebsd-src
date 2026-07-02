/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdlib.h>

#include "strfrom.h"

int
strfromf(char * __restrict s, size_t n, const char * __restrict fmt, float fp)
{
	char conv, lc, *digits, *dend;
	int prec, decpt, signflag, ret, mode, ndig_req;
	double dbl;

	dbl = (double)fp;
	conv = sf_parse_fmt(fmt, &prec);
	lc = conv | 0x20;

	if (lc == 'a') {
		digits = __hdtoa(dbl, sf_xdigits(conv),
		    prec >= 0 ? prec + 1 : -1, &decpt, &signflag, &dend);
		ret = sf_render_hex(s, n, conv, prec, digits, dend, decpt,
		    signflag);
		freedtoa(digits);
		return (ret);
	}

	if (prec < 0)
		prec = 6;

	sf_decimal_mode(lc, prec, &mode, &ndig_req);

	digits = dtoa(dbl, mode, ndig_req, &decpt, &signflag, &dend);

	ret = sf_render_decimal(s, n, conv, lc, prec, digits, dend, decpt,
	    signflag, decpt == 9999);

	freedtoa(digits);

	return (ret);
}
