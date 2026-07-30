/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <ctype.h>
#include <stdlib.h>

#include "strfrom.h"

int
strfromd(char * __restrict s, size_t n, const char * __restrict fmt, double fp)
{
	char conv, lc, *digits, *dend;
	int prec, decpt, signflag, ret, mode, ndig_req;

	conv = __sf_parse_fmt(fmt, &prec);
	lc = tolower((unsigned char)conv);

	if (lc == 'a') {
		digits = __hdtoa(fp, __sf_xdigits(conv),
		    prec >= 0 ? prec + 1 : -1, &decpt, &signflag, &dend);
		ret = __sf_render_hex(s, n, conv, prec, digits, dend, decpt,
		    signflag);
		freedtoa(digits);
		return (ret);
	}

	if (prec < 0)
		prec = 6;

	__sf_decimal_mode(lc, prec, &mode, &ndig_req);

	digits = dtoa(fp, mode, ndig_req, &decpt, &signflag, &dend);

	ret = __sf_render_decimal(s, n, conv, lc, prec, digits, dend, decpt,
	    signflag, decpt == 9999);

	freedtoa(digits);

	return (ret);
}
