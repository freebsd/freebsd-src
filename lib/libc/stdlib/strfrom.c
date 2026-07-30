/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Shared helpers for strfromd, strfromf, and strfroml (C23 §7.24.1.3).
 */

#include <ctype.h>
#include <locale.h>
#include <stdlib.h>

#include "strfrom.h"

struct sf_buf {
	char	*s;
	size_t	 n;
	int	 pos;
};

/*
 * Write c to b->s[b->pos] if within the n-1 writable bytes; always advance
 * b->pos.  n is the total buffer capacity, including the null slot.
 */
static void
sf_putc(struct sf_buf *b, char c)
{
	if (b->n > 0 && (size_t)b->pos < b->n - 1)
		b->s[b->pos] = c;
	b->pos++;
}

/*
 * Write len bytes from src.
 */
static void
sf_write(struct sf_buf *b, const char *src, int len)
{
	int i;

	for (i = 0; i < len; i++)
		sf_putc(b, src[i]);
}

/*
 * Write a null terminated string.
 */
static void
sf_puts(struct sf_buf *b, const char *src)
{
	while (*src != '\0')
		sf_putc(b, *src++);
}

/*
 * Write count copies of c.
 */
static void
sf_padc(struct sf_buf *b, char c, int count)
{
	int i;

	for (i = 0; i < count; i++)
		sf_putc(b, c);
}

/*
 * Seal the buffer: null terminate at min(pos, n-1).
 */
static void
sf_seal(struct sf_buf *b)
{
	if (b->n > 0)
		b->s[(size_t)b->pos < b->n ? b->pos : b->n - 1] = '\0';
}

/*
 * Emit the radix.  Interfaces are defined as snprintf(3) (C23 §7.24.1.3p2),
 * whose '.' is the LC_NUMERIC decimal point; localeconv() resolves the same
 * per-thread locale as vfprintf(3).  The radix may be multibyte.
 */
static void
sf_putdp(struct sf_buf *b)
{
	sf_puts(b, localeconv()->decimal_point);
}

/*
 * Emit "[eEpP][+-]ddd": exponent sign and decimal magnitude, zero-padded to at
 * least mindig digits (2 for %e/%E and %g/%G, 1 for %a/%A per C23 §7.23.6.1).
 *
 * Handles the full exponent range of every supported type.
 */
static void
sf_emit_exp(struct sf_buf *b, char ec, int exp, int mindig)
{
	char buf[16];
	int len;

	sf_putc(b, ec);
	if (exp < 0) {
		sf_putc(b, '-');
		exp = -exp;
	} else {
		sf_putc(b, '+');
	}

	len = 0;
	do {
		buf[len++] = '0' + exp % 10;
		exp /= 10;
	} while (exp != 0);
	while (len < mindig)
		buf[len++] = '0';
	while (len > 0)
		sf_putc(b, buf[--len]);
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
char
__sf_parse_fmt(const char *fmt, int *prec)
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
 * Render Inf or NaN into b->s[0..n-1].
 *
 * is_nan: non-zero if digits[0] == 'N' (NaN), zero for Infinity.
 */
static int
sf_special(struct sf_buf *b, char conv, int signflag, int is_nan)
{
	int upper;

	b->pos = 0;
	upper = isupper((unsigned char)conv);
	if (!is_nan && signflag)
		sf_putc(b, '-');
	sf_puts(b, is_nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf"));
	sf_seal(b);
	return (b->pos);
}

/*
 * Render %e / %E.
 *
 * dtoa was called with mode=2, ndigits=prec+1.  decpt is the position of the
 * first significant digit relative to the decimal point (= exponent + 1), as
 * returned by dtoa.
 */
static int
sf_efmt(struct sf_buf *b, int prec, char conv,
    const char *digits, int ndig, int decpt, int signflag)
{
	int avail, copy;

	b->pos = 0;
	if (signflag)
		sf_putc(b, '-');

	/*
	 * Leading significant digit.
	 */
	sf_putc(b, ndig > 0 ? digits[0] : '0');

	if (prec > 0) {
		sf_putdp(b);
		avail = ndig > 1 ? ndig - 1 : 0;
		copy = avail < prec ? avail : prec;
		sf_write(b, digits + 1, copy);
		sf_padc(b, '0', prec - copy);
	}
	sf_emit_exp(b, isupper((unsigned char)conv) ? 'E' : 'e', decpt - 1, 2);

	sf_seal(b);
	return (b->pos);
}

/*
 * Render %f / %F.
 *
 * dtoa was called with mode=3, ndigits=prec.  decpt gives the number of digits
 * before the decimal point (may be <= 0).
 */
static int
sf_ffmt(struct sf_buf *b, int prec, int signflag,
    const char *digits, int ndig, int decpt)
{
	int avail, copy, rem, zc;

	b->pos = 0;
	if (signflag)
		sf_putc(b, '-');

	if (decpt <= 0) {
		sf_putc(b, '0');
		if (prec > 0) {
			sf_putdp(b);
			zc = -decpt < prec ? -decpt : prec;
			rem = prec - zc;
			copy = ndig < rem ? ndig : rem;
			sf_padc(b, '0', zc);
			sf_write(b, digits, copy);
			sf_padc(b, '0', rem - copy);
		}
	} else {
		/*
		 * decpt digits (or zeros) before the point.
		 */
		copy = ndig < decpt ? ndig : decpt;
		sf_write(b, digits, copy);
		sf_padc(b, '0', decpt - copy);
		if (prec > 0) {
			sf_putdp(b);
			avail = ndig - decpt;
			if (avail < 0)
				avail = 0;
			copy = avail < prec ? avail : prec;
			sf_write(b, digits + decpt, copy);
			sf_padc(b, '0', prec - copy);
		}
	}

	sf_seal(b);
	return (b->pos);
}

/*
 * Render %g / %G.
 *
 * dtoa was called with mode=2, ndigits=max(1, prec).
 */
static int
sf_gfmt(struct sf_buf *b, int prec, char conv,
    const char *digits, int ndig, int decpt, int signflag)
{
	int ep, copy, frac;

	b->pos = 0;
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
		sf_putc(b, '-');

	/*
	 * Standard says −4 ≤ exponent < P test: exponent = decpt − 1
	 */
	if (decpt > -4 && decpt <= ep) {
		/*
		 * %f style, per C23 §7.23.6.1.
		 */
		if (decpt <= 0) {
			sf_putc(b, '0');
			if (ndig > 0) {
				sf_putdp(b);
				sf_padc(b, '0', -decpt);
				sf_write(b, digits, ndig);
			}
		} else {
			copy = ndig < decpt ? ndig : decpt;
			sf_write(b, digits, copy);
			sf_padc(b, '0', decpt - copy);
			frac = ndig - decpt;
			if (frac > 0) {
				sf_putdp(b);
				sf_write(b, digits + decpt, frac);
			}
		}
	} else {
		/*
		 * %e style, per C23 §7.23.6.1.
		 */
		sf_putc(b, ndig > 0 ? digits[0] : '0');
		if (ndig > 1) {
			sf_putdp(b);
			sf_write(b, digits + 1, ndig - 1);
		}
		sf_emit_exp(b, isupper((unsigned char)conv) ? 'E' : 'e',
		    decpt - 1, 2);
	}

	sf_seal(b);
	return (b->pos);
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
sf_afmt(struct sf_buf *b, int user_prec, char conv,
    const char *digits, int ndig, int decpt, int signflag)
{
	int after, copy;

	b->pos = 0;
	if (signflag)
		sf_putc(b, '-');
	sf_putc(b, '0');
	sf_putc(b, isupper((unsigned char)conv) ? 'X' : 'x');

	/*
	 * Integer part of the significand.
	 */
	sf_putc(b, ndig > 0 ? digits[0] : '0');

	after = ndig - 1;
	if (user_prec < 0) {
		/*
		 * Shortest-exact: emit only the non-zero significant tail.
		 */
		if (after > 0) {
			sf_putdp(b);
			sf_write(b, digits + 1, after);
		}
	} else if (user_prec > 0) {
		sf_putdp(b);
		copy = after < user_prec ? after : user_prec;
		sf_write(b, digits + 1, copy);
		sf_padc(b, '0', user_prec - copy);
	}
	sf_emit_exp(b, isupper((unsigned char)conv) ? 'P' : 'p', decpt - 1, 1);

	sf_seal(b);
	return (b->pos);
}

/*
 * Hex digit table for %a / %A, selected by specifier case.
 */
const char *
__sf_xdigits(char conv)
{
	return (isupper((unsigned char)conv) ? "0123456789ABCDEF" :
	    "0123456789abcdef");
}

/*
 * Map a decimal specifier (lowercased) to its gdtoa mode and digit count.
 */
void
__sf_decimal_mode(char lc, int prec, int *mode, int *ndig_req)
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
int
__sf_render_hex(char *s, size_t n, char conv, int prec,
    char *digits, char *dend, int decpt, int signflag)
{
	struct sf_buf b;
	int ndig, usprec;

	b.s = s;
	b.n = n;
	if (decpt == INT_MAX)
		return (sf_special(&b, conv, signflag, digits[0] == 'N'));
	ndig = (int)(dend - digits);
	usprec = prec >= 0 ? prec : ndig - 1;
	return (sf_afmt(&b, usprec, conv, digits, ndig, decpt, signflag));
}

/*
 * Render a finished %e/%f/%g conversion.  is_special is set when decpt holds
 * the caller's Inf/NaN sentinel (dtoa: 9999, __ldtoa: INT_MAX).
 */
int
__sf_render_decimal(char *s, size_t n, char conv, char lc, int prec,
    char *digits, char *dend, int decpt, int signflag, int is_special)
{
	struct sf_buf b;
	int ndig;

	b.s = s;
	b.n = n;
	if (is_special)
		return (sf_special(&b, conv, signflag, digits[0] == 'N'));
	ndig = (int)(dend - digits);
	switch (lc) {
	case 'e':
		return (sf_efmt(&b, prec, conv, digits, ndig, decpt, signflag));
	case 'f':
		return (sf_ffmt(&b, prec, signflag, digits, ndig, decpt));
	case 'g':
		return (sf_gfmt(&b, prec, conv, digits, ndig, decpt, signflag));
	default:
		__unreachable();
	}
}
