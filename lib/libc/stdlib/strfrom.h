/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Common routines for strfrom* numeric conversion functions, as defined in
 * C23 §7.24.1.3.
 *
 * Note: gdtoa dtoa() modes used:
 *   2 – ndigits significant digits      (%e, %g)
 *   3 – ndigits digits after '.'        (%f)
 */

#ifndef STRFROM_H
#define	STRFROM_H

#include <float.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define	dtoa		__dtoa
#define	freedtoa	__freedtoa

#include "../stdio/floatio.h"
#include "gdtoa.h"

/*
 * Write c to s[*pos] if within the n-1 writable bytes; always advance *pos.
 * n is the total buffer capacity, including the null slot.
 */
static inline void
sf_putc(char *s, size_t n, int *pos, char c)
{
	if (n > 0 && (size_t)*pos < n - 1)
		s[*pos] = c;
	(*pos)++;
}

/*
 * Write len bytes from src.
 */
static inline void
sf_write(char *s, size_t n, int *pos, const char *src, int len)
{
	for (int i = 0; i < len; i++)
		sf_putc(s, n, pos, src[i]);
}

/*
 * Write count copies of c.
 */
static inline void
sf_padc(char *s, size_t n, int *pos, char c, int count)
{
	for (int i = 0; i < count; i++)
		sf_putc(s, n, pos, c);
}

/*
 * Seal the buffer: null terminate at min(pos, n-1).
 */
static inline void
sf_seal(char *s, size_t n, int pos)
{
	if (n > 0)
		s[(size_t)pos < n ? pos : n - 1] = '\0';
}

/*
 * Emit the radix.  Interfaces are defined as snprintf(3) (C23 §7.24.1.3p2),
 * whose '.' is the LC_NUMERIC decimal point; localeconv() resolves the same
 * per-thread locale as vfprintf(3).  The radix may be multibyte.
 */
static inline void
sf_putdp(char *s, size_t n, int *pos)
{
	const char *dp;

	dp = localeconv()->decimal_point;
	sf_write(s, n, pos, dp, dp[1] == '\0' ? 1 : (int)strlen(dp));
}

/*
 * Emit "[eEpP][+-]ddd": exponent sign and decimal magnitude, zero-padded to at
 * least mindig digits (2 for %e/%E and %g/%G, 1 for %a/%A per C23 §7.23.6.1).
 *
 * Handles the full exponent range of every supported type.
 */
static void
sf_emit_exp(char *s, size_t n, int *pos, char ec, int exp, int mindig)
{
	char buf[16];
	int len;

	sf_putc(s, n, pos, ec);
	if (exp < 0) {
		sf_putc(s, n, pos, '-');
		exp = -exp;
	} else {
		sf_putc(s, n, pos, '+');
	}

	len = 0;
	do {
		buf[len++] = '0' + exp % 10;
		exp /= 10;
	} while (exp != 0);
	while (len < mindig)
		buf[len++] = '0';
	while (len > 0)
		sf_putc(s, n, pos, buf[--len]);
}

/*
 * Parse "%[.prec]conv" per C23 §7.24.1.3.
 *
 * Returns the conversion specifier character; *prec is -1 if absent.
 *
 * C23 §7.24.1.3p2 makes any other format string undefined behaviour.
 * Per §3.5.3, terminating execution is an explicitly valid response to an
 * undefined behaviour, and abort() here makes the default __unreachable()
 * branch in any other caller literally unreachable rather than merely a
 * contract annotation.
 */
static char
sf_parse_fmt(const char *fmt, int *prec)
{
	const char *p;
	char c;

	if (*fmt != '%')
		abort();
	p = fmt + 1;

	*prec = -1;
	if (*p == '.') {
		*prec = 0;
		while (*++p >= '0' && *p <= '9')
			*prec = *prec * 10 + (*p - '0');
	}
	c = *p;

	switch (c) {
	case 'a': case 'A':
	case 'e': case 'E':
	case 'f': case 'F':
	case 'g': case 'G':
		break;
	default:
		abort();
	}
	return (c);
}

/*
 * Render Inf or NaN into s[0..n-1].
 *
 * is_nan: non-zero if digits[0] == 'N' (NaN), zero for Infinity.
 */
static int
sf_special(char *s, size_t n, char conv, int signflag, int is_nan)
{
	int pos;
	int upper;

	pos = 0;
	upper = (conv >= 'A' && conv <= 'Z');
	if (!is_nan && signflag)
		sf_putc(s, n, &pos, '-');
	sf_write(s, n, &pos,
	    is_nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf"), 3);
	sf_seal(s, n, pos);
	return (pos);
}

/*
 * Render %e / %E.
 *
 * dtoa was called with mode=2, ndigits=prec+1.  decpt is the position of the
 * first significant digit relative to the decimal point (= exponent + 1), as
 * returned by dtoa.
 */
static int
sf_efmt(char *s, size_t n, int prec, char conv,
    const char *digits, int ndig, int decpt, int signflag)
{
	int pos;
	int avail, copy;

	pos = 0;
	if (signflag)
		sf_putc(s, n, &pos, '-');

	/*
	 * Leading significant digit.
	 */
	sf_putc(s, n, &pos, ndig > 0 ? digits[0] : '0');

	if (prec > 0) {
		sf_putdp(s, n, &pos);
		avail = ndig > 1 ? ndig - 1 : 0;
		copy = avail < prec ? avail : prec;
		sf_write(s, n, &pos, digits + 1, copy);
		sf_padc(s, n, &pos, '0', prec - copy);
	}
	sf_emit_exp(s, n, &pos, conv == 'e' ? 'e' : 'E', decpt - 1, 2);

	sf_seal(s, n, pos);
	return (pos);
}

/*
 * Render %f / %F.
 *
 * dtoa was called with mode=3, ndigits=prec.  decpt gives the number of digits
 * before the decimal point (may be <= 0).
 */
static int
sf_ffmt(char *s, size_t n, int prec, int signflag,
    const char *digits, int ndig, int decpt)
{
	int pos;
	int avail, copy, rem, zc;

	pos = 0;
	if (signflag)
		sf_putc(s, n, &pos, '-');

	if (decpt <= 0) {
		sf_putc(s, n, &pos, '0');
		if (prec > 0) {
			sf_putdp(s, n, &pos);
			zc = -decpt < prec ? -decpt : prec;
			rem = prec - zc;
			copy = ndig < rem ? ndig : rem;
			sf_padc(s, n, &pos, '0', zc);
			sf_write(s, n, &pos, digits, copy);
			sf_padc(s, n, &pos, '0', rem - copy);
		}
	} else {
		/*
		 * decpt digits (or zeros) before the point.
		 */
		copy = ndig < decpt ? ndig : decpt;
		sf_write(s, n, &pos, digits, copy);
		sf_padc(s, n, &pos, '0', decpt - copy);
		if (prec > 0) {
			sf_putdp(s, n, &pos);
			avail = ndig - decpt;
			if (avail < 0)
				avail = 0;
			copy = avail < prec ? avail : prec;
			sf_write(s, n, &pos, digits + decpt, copy);
			sf_padc(s, n, &pos, '0', prec - copy);
		}
	}

	sf_seal(s, n, pos);
	return (pos);
}

/*
 * Render %g / %G.
 *
 * dtoa was called with mode=2, ndigits=max(1, prec).
 */
static int
sf_gfmt(char *s, size_t n, int prec, char conv,
    const char *digits, int ndig, int decpt, int signflag)
{
	int pos;
	int ep, copy, frac;

	pos = 0;
	/*
	 * Precision 0 is treated as 1 per C23 §7.23.6.1.
	 */
	ep = prec == 0 ? 1 : prec;

	/*
	 * Strip trailing zeros (no ALT flag is accepted).
	 */
	while (ndig > 1 && digits[ndig - 1] == '0')
		ndig--;

	if (signflag)
		sf_putc(s, n, &pos, '-');

	/*
	 * Standard says −4 ≤ exponent < P test: exponent = decpt − 1
	 */
	if (decpt > -4 && decpt <= ep) {
		/*
		 * %f style, per C23 §7.23.6.1.
		 */
		if (decpt <= 0) {
			sf_putc(s, n, &pos, '0');
			if (ndig > 0) {
				sf_putdp(s, n, &pos);
				sf_padc(s, n, &pos, '0', -decpt);
				sf_write(s, n, &pos, digits, ndig);
			}
		} else {
			copy = ndig < decpt ? ndig : decpt;
			sf_write(s, n, &pos, digits, copy);
			sf_padc(s, n, &pos, '0', decpt - copy);
			frac = ndig - decpt;
			if (frac > 0) {
				sf_putdp(s, n, &pos);
				sf_write(s, n, &pos, digits + decpt, frac);
			}
		}
	} else {
		/*
		 * %e style, per C23 §7.23.6.1.
		 */
		sf_putc(s, n, &pos, ndig > 0 ? digits[0] : '0');
		if (ndig > 1) {
			sf_putdp(s, n, &pos);
			sf_write(s, n, &pos, digits + 1, ndig - 1);
		}
		sf_emit_exp(s, n, &pos, conv == 'g' ? 'e' : 'E', decpt - 1, 2);
	}

	sf_seal(s, n, pos);
	return (pos);
}

/*
 * Render %a / %A.
 *
 * digits / ndig: hex significand digits from __hdtoa / __hldtoa; the first
 * digit represents the integer part of the mantissa (normally '1').
 *
 * decpt: binary exponent such that p-exponent = decpt - 1.
 *
 * user_prec: digits after the hex point (-1 for shortest-exact).
 */
static int
sf_afmt(char *s, size_t n, int user_prec, char conv,
    const char *digits, int ndig, int decpt, int signflag)
{
	int pos;
	int after, copy;

	pos = 0;
	if (signflag)
		sf_putc(s, n, &pos, '-');
	sf_putc(s, n, &pos, '0');
	sf_putc(s, n, &pos, conv == 'a' ? 'x' : 'X');

	/*
	 * Integer part of the significand.
	 */
	sf_putc(s, n, &pos, ndig > 0 ? digits[0] : '0');

	after = ndig - 1;
	if (user_prec < 0) {
		/*
		 * Shortest-exact: emit only the non-zero significant tail.
		 */
		if (after > 0) {
			sf_putdp(s, n, &pos);
			sf_write(s, n, &pos, digits + 1, after);
		}
	} else if (user_prec > 0) {
		sf_putdp(s, n, &pos);
		copy = after < user_prec ? after : user_prec;
		sf_write(s, n, &pos, digits + 1, copy);
		sf_padc(s, n, &pos, '0', user_prec - copy);
	}
	sf_emit_exp(s, n, &pos, conv == 'a' ? 'p' : 'P', decpt - 1, 1);

	sf_seal(s, n, pos);
	return (pos);
}

/*
 * Hex digit table for %a / %A, selected by specifier case.
 */
static inline const char *
sf_xdigits(char conv)
{

	return (conv == 'a' ? "0123456789abcdef" : "0123456789ABCDEF");
}

/*
 * Map a decimal specifier (lowercased) to its gdtoa mode and digit count.
 */
static inline void
sf_decimal_mode(char lc, int prec, int *mode, int *ndig_req)
{

	switch (lc) {
	case 'e':
		*mode = 2;
		*ndig_req = prec + 1;
		break;
	case 'f':
		*mode = 3;
		*ndig_req = prec;
		break;
	case 'g':
		*mode = 2;
		*ndig_req = prec == 0 ? 1 : prec;
		break;
	default:
		__unreachable();
	}
}

/*
 * Render a finished %a / %A conversion.  __hdtoa / __hldtoa always flag
 * Inf/NaN with decpt == INT_MAX.
 */
static int
sf_render_hex(char *s, size_t n, char conv, int prec,
    char *digits, char *dend, int decpt, int signflag)
{
	int ndig, usprec;

	if (decpt == INT_MAX)
		return (sf_special(s, n, conv, signflag, digits[0] == 'N'));
	ndig = (int)(dend - digits);
	usprec = prec >= 0 ? prec : ndig - 1;
	return (sf_afmt(s, n, usprec, conv, digits, ndig, decpt, signflag));
}

/*
 * Render a finished %e/%f/%g conversion.  is_special is set when decpt holds
 * the caller's Inf/NaN sentinel (dtoa: 9999, __ldtoa: INT_MAX).
 */
static int
sf_render_decimal(char *s, size_t n, char conv, char lc, int prec,
    char *digits, char *dend, int decpt, int signflag, int is_special)
{
	int ndig;

	if (is_special)
		return (sf_special(s, n, conv, signflag, digits[0] == 'N'));
	ndig = (int)(dend - digits);
	switch (lc) {
	case 'e':
		return (sf_efmt(s, n, prec, conv, digits, ndig, decpt,
		    signflag));
	case 'f':
		return (sf_ffmt(s, n, prec, signflag, digits, ndig, decpt));
	case 'g':
		return (sf_gfmt(s, n, prec, conv, digits, ndig, decpt,
		    signflag));
	default:
		__unreachable();
	}
}

#endif /* STRFROM_H */
