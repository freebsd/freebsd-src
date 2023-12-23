/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 *
 * Copyright (c) 2023 Dag-Erling Smørgrav
 *
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "namespace.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "un-namespace.h"

#include "collate.h"
#include "libc_private.h"
#include "local.h"
#include "xlocale_private.h"

#ifndef NO_FLOATING_POINT
#include <locale.h>
#endif

#define	BUF		513	/* Maximum length of numeric string. */

/*
 * Flags used during conversion.
 */
#define	LONG		0x01	/* l: long or double */
#define	LONGDBL		0x02	/* L: long double */
#define	SHORT		0x04	/* h: short */
#define	SUPPRESS	0x08	/* *: suppress assignment */
#define	POINTER		0x10	/* p: void * (as hex) */
#define	NOSKIP		0x20	/* [ or c: do not skip blanks */
#define FASTINT		0x200	/* wfN: int_fastN_t */
#define	LONGLONG	0x400	/* ll: long long (+ deprecated q: quad) */
#define	INTMAXT		0x800	/* j: intmax_t */
#define	PTRDIFFT	0x1000	/* t: ptrdiff_t */
#define	SIZET		0x2000	/* z: size_t */
#define	SHORTSHORT	0x4000	/* hh: char */
#define	UNSIGNED	0x8000	/* %[oupxX] conversions */

/*
 * Conversion types.
 */
#define	CT_CHAR		0	/* %c conversion */
#define	CT_CCL		1	/* %[...] conversion */
#define	CT_STRING	2	/* %s conversion */
#define	CT_INT		3	/* %[dioupxX] conversion */
#define	CT_FLOAT	4	/* %[efgEFG] conversion */

static const u_char *__sccl(char *, const u_char *);
#ifndef NO_FLOATING_POINT
static int parsefloat(FILE *, char *, char *, locale_t);
#endif

__weak_reference(__vfscanf, vfscanf);

/*
 * Conversion functions are passed a pointer to this object instead of
 * a real parameter to indicate that the assignment-suppression (*)
 * flag was specified.  We could use a NULL pointer to indicate this,
 * but that would mask bugs in applications that call scanf() with a
 * NULL pointer.
 */
static const int suppress;
#define	SUPPRESS_PTR	((void *)&suppress)

static const mbstate_t initial_mbs;

/*
 * The following conversion functions return the number of characters consumed,
 * or -1 on input failure.  Character class conversion returns 0 on match
 * failure.
 */

static __inline int
convert_char(FILE *fp, char * p, int width)
{
	int n;

	if (p == SUPPRESS_PTR) {
		size_t sum = 0;
		for (;;) {
			if ((n = fp->_r) < width) {
				sum += n;
				width -= n;
				fp->_p += n;
				if (__srefill(fp)) {
					if (sum == 0)
						return (-1);
					break;
				}
			} else {
				sum += width;
				fp->_r -= width;
				fp->_p += width;
				break;
			}
		}
		return (sum);
	} else {
		size_t r = __fread(p, 1, width, fp);

		if (r == 0)
			return (-1);
		return (r);
	}
}

static __inline int
convert_wchar(FILE *fp, wchar_t *wcp, int width, locale_t locale)
{
	mbstate_t mbs;
	int n, nread;
	wint_t wi;

	mbs = initial_mbs;
	n = 0;
	while (width-- != 0 &&
	    (wi = __fgetwc_mbs(fp, &mbs, &nread, locale)) != WEOF) {
		if (wcp != SUPPRESS_PTR)
			*wcp++ = (wchar_t)wi;
		n += nread;
	}
	if (n == 0)
		return (-1);
	return (n);
}

static __inline int
convert_ccl(FILE *fp, char * p, int width, const char *ccltab)
{
	char *p0;
	int n;

	if (p == SUPPRESS_PTR) {
		n = 0;
		while (ccltab[*fp->_p]) {
			n++, fp->_r--, fp->_p++;
			if (--width == 0)
				break;
			if (fp->_r <= 0 && __srefill(fp)) {
				if (n == 0)
					return (-1);
				break;
			}
		}
	} else {
		p0 = p;
		while (ccltab[*fp->_p]) {
			fp->_r--;
			*p++ = *fp->_p++;
			if (--width == 0)
				break;
			if (fp->_r <= 0 && __srefill(fp)) {
				if (p == p0)
					return (-1);
				break;
			}
		}
		n = p - p0;
		if (n == 0)
			return (0);
		*p = 0;
	}
	return (n);
}

static __inline int
convert_wccl(FILE *fp, wchar_t *wcp, int width, const char *ccltab,
    locale_t locale)
{
	mbstate_t mbs;
	wint_t wi;
	int n, nread;

	mbs = initial_mbs;
	n = 0;
	if (wcp == SUPPRESS_PTR) {
		while ((wi = __fgetwc_mbs(fp, &mbs, &nread, locale)) != WEOF &&
		    width-- != 0 && ccltab[wctob(wi)])
			n += nread;
		if (wi != WEOF)
			__ungetwc(wi, fp, __get_locale());
	} else {
		while ((wi = __fgetwc_mbs(fp, &mbs, &nread, locale)) != WEOF &&
		    width-- != 0 && ccltab[wctob(wi)]) {
			*wcp++ = (wchar_t)wi;
			n += nread;
		}
		if (wi != WEOF)
			__ungetwc(wi, fp, __get_locale());
		if (n == 0)
			return (0);
		*wcp = 0;
	}
	return (n);
}

static __inline int
convert_string(FILE *fp, char * p, int width)
{
	char *p0;
	int n;

	if (p == SUPPRESS_PTR) {
		n = 0;
		while (!isspace(*fp->_p)) {
			n++, fp->_r--, fp->_p++;
			if (--width == 0)
				break;
			if (fp->_r <= 0 && __srefill(fp))
				break;
		}
	} else {
		p0 = p;
		while (!isspace(*fp->_p)) {
			fp->_r--;
			*p++ = *fp->_p++;
			if (--width == 0)
				break;
			if (fp->_r <= 0 && __srefill(fp))
				break;
		}
		*p = 0;
		n = p - p0;
	}
	return (n);
}

static __inline int
convert_wstring(FILE *fp, wchar_t *wcp, int width, locale_t locale)
{
	mbstate_t mbs;
	wint_t wi;
	int n, nread;

	mbs = initial_mbs;
	n = 0;
	if (wcp == SUPPRESS_PTR) {
		while ((wi = __fgetwc_mbs(fp, &mbs, &nread, locale)) != WEOF &&
		    width-- != 0 && !iswspace(wi))
			n += nread;
		if (wi != WEOF)
			__ungetwc(wi, fp, __get_locale());
	} else {
		while ((wi = __fgetwc_mbs(fp, &mbs, &nread, locale)) != WEOF &&
		    width-- != 0 && !iswspace(wi)) {
			*wcp++ = (wchar_t)wi;
			n += nread;
		}
		if (wi != WEOF)
			__ungetwc(wi, fp, __get_locale());
		*wcp = '\0';
	}
	return (n);
}

enum parseint_state {
	begin,
	havesign,
	havezero,
	haveprefix,
	any,
};

static __inline int
parseint_fsm(int c, enum parseint_state *state, int *base)
{
	switch (c) {
	case '+':
	case '-':
		if (*state == begin) {
			*state = havesign;
			return 1;
		}
		break;
	case '0':
		if (*state == begin || *state == havesign) {
			*state = havezero;
		} else {
			*state = any;
		}
		return 1;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		if (*state == havezero && *base == 0) {
			*base = 8;
		}
		/* FALL THROUGH */
	case '8':
	case '9':
		if (*state == begin ||
		    *state == havesign) {
			if (*base == 0) {
				*base = 10;
			}
		}
		if (*state == begin ||
		    *state == havesign ||
		    *state == havezero ||
		    *state == haveprefix ||
		    *state == any) {
			if (*base > c - '0') {
				*state = any;
				return 1;
			}
		}
		break;
	case 'b':
		if (*state == havezero) {
			if (*base == 0 || *base == 2) {
				*state = haveprefix;
				*base = 2;
				return 1;
			}
		}
		/* FALL THROUGH */
	case 'a':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		if (*state == begin ||
		    *state == havesign ||
		    *state == havezero ||
		    *state == haveprefix ||
		    *state == any) {
			if (*base > c - 'a' + 10) {
				*state = any;
				return 1;
			}
		}
		break;
	case 'B':
		if (*state == havezero) {
			if (*base == 0 || *base == 2) {
				*state = haveprefix;
				*base = 2;
				return 1;
			}
		}
		/* FALL THROUGH */
	case 'A':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		if (*state == begin ||
		    *state == havesign ||
		    *state == havezero ||
		    *state == haveprefix ||
		    *state == any) {
			if (*base > c - 'A' + 10) {
				*state = any;
				return 1;
			}
		}
		break;
	case 'x':
	case 'X':
		if (*state == havezero) {
			if (*base == 0 || *base == 16) {
				*state = haveprefix;
				*base = 16;
				return 1;
			}
		}
		break;
	}
	return 0;
}

/*
 * Read an integer, storing it in buf.
 *
 * Return 0 on a match failure, and the number of characters read
 * otherwise.
 */
static __inline int
parseint(FILE *fp, char * __restrict buf, int width, int base)
{
	enum parseint_state state = begin;
	char *p;
	int c;

	for (p = buf; width; width--) {
		c = __sgetc(fp);
		if (c == EOF)
			break;
		if (!parseint_fsm(c, &state, &base))
			break;
		*p++ = c;
	}
	/*
	 * If we only had a sign, push it back.  If we only had a 0b or 0x
	 * prefix (possibly preceded by a sign), we view it as "0" and
	 * push back the letter.  In all other cases, if we stopped
	 * because we read a non-number character, push it back.
	 */
	if (state == havesign) {
		p--;
		(void) __ungetc(*(u_char *)p, fp);
	} else if (state == haveprefix) {
		p--;
		(void) __ungetc(c, fp);
	} else if (width && c != EOF) {
		(void) __ungetc(c, fp);
	}
	return (p - buf);
}

/*
 * __vfscanf - MT-safe version
 */
int
__vfscanf(FILE *fp, char const *fmt0, va_list ap)
{
	int ret;

	FLOCKFILE_CANCELSAFE(fp);
	ret = __svfscanf(fp, __get_locale(), fmt0, ap);
	FUNLOCKFILE_CANCELSAFE();
	return (ret);
}
int
vfscanf_l(FILE *fp, locale_t locale, char const *fmt0, va_list ap)
{
	int ret;
	FIX_LOCALE(locale);

	FLOCKFILE_CANCELSAFE(fp);
	ret = __svfscanf(fp, locale, fmt0, ap);
	FUNLOCKFILE_CANCELSAFE();
	return (ret);
}

/*
 * __svfscanf - non-MT-safe version of __vfscanf
 */
int
__svfscanf(FILE *fp, locale_t locale, const char *fmt0, va_list ap)
{
#define	GETARG(type)	((flags & SUPPRESS) ? SUPPRESS_PTR : va_arg(ap, type))
	const u_char *fmt = (const u_char *)fmt0;
	int c;			/* character from format, or conversion */
	size_t width;		/* field width, or 0 */
	int flags;		/* flags as defined above */
	int nassigned;		/* number of fields assigned */
	int nconversions;	/* number of conversions */
	int nr;			/* characters read by the current conversion */
	int nread;		/* number of characters consumed from fp */
	int base;		/* base argument to conversion function */
	char ccltab[256];	/* character class table for %[...] */
	char buf[BUF];		/* buffer for numeric conversions */

	ORIENT(fp, -1);

	nassigned = 0;
	nconversions = 0;
	nread = 0;
	for (;;) {
		c = *fmt++;
		if (c == 0)
			return (nassigned);
		if (isspace(c)) {
			while ((fp->_r > 0 || __srefill(fp) == 0) && isspace(*fp->_p))
				nread++, fp->_r--, fp->_p++;
			continue;
		}
		if (c != '%')
			goto literal;
		width = 0;
		flags = 0;
		/*
		 * switch on the format.  continue if done;
		 * break once format type is derived.
		 */
again:		c = *fmt++;
		switch (c) {
		case '%':
literal:
			if (fp->_r <= 0 && __srefill(fp))
				goto input_failure;
			if (*fp->_p != c)
				goto match_failure;
			fp->_r--, fp->_p++;
			nread++;
			continue;

		case '*':
			flags |= SUPPRESS;
			goto again;
		case 'j':
			flags |= INTMAXT;
			goto again;
		case 'l':
			if (flags & LONG) {
				flags &= ~LONG;
				flags |= LONGLONG;
			} else
				flags |= LONG;
			goto again;
		case 'q':
			flags |= LONGLONG;	/* not quite */
			goto again;
		case 't':
			flags |= PTRDIFFT;
			goto again;
		case 'w':
			/*
			 * Fixed-width integer types.  On all platforms we
			 * support, int8_t is equivalent to char, int16_t
			 * is equivalent to short, int32_t is equivalent
			 * to int, int64_t is equivalent to long long int.
			 * Furthermore, int_fast8_t, int_fast16_t and
			 * int_fast32_t are equivalent to int, and
			 * int_fast64_t is equivalent to long long int.
			 */
			flags &= ~(SHORTSHORT|SHORT|LONG|LONGLONG|SIZET|INTMAXT|PTRDIFFT);
			if (fmt[0] == 'f') {
				flags |= FASTINT;
				fmt++;
			} else {
				flags &= ~FASTINT;
			}
			if (fmt[0] == '8') {
				if (!(flags & FASTINT))
					flags |= SHORTSHORT;
				else
					/* no flag set = 32 */ ;
				fmt += 1;
			} else if (fmt[0] == '1' && fmt[1] == '6') {
				if (!(flags & FASTINT))
					flags |= SHORT;
				else
					/* no flag set = 32 */ ;
				fmt += 2;
			} else if (fmt[0] == '3' && fmt[1] == '2') {
				/* no flag set = 32 */ ;
				fmt += 2;
			} else if (fmt[0] == '6' && fmt[1] == '4') {
				flags |= LONGLONG;
				fmt += 2;
			} else {
				goto match_failure;
			}
			goto again;
		case 'z':
			flags |= SIZET;
			goto again;
		case 'L':
			flags |= LONGDBL;
			goto again;
		case 'h':
			if (flags & SHORT) {
				flags &= ~SHORT;
				flags |= SHORTSHORT;
			} else
				flags |= SHORT;
			goto again;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			width = width * 10 + c - '0';
			goto again;

		/*
		 * Conversions.
		 */
		case 'B':
		case 'b':
			c = CT_INT;
			flags |= UNSIGNED;
			base = 2;
			break;

		case 'd':
			c = CT_INT;
			base = 10;
			break;

		case 'i':
			c = CT_INT;
			base = 0;
			break;

		case 'o':
			c = CT_INT;
			flags |= UNSIGNED;
			base = 8;
			break;

		case 'u':
			c = CT_INT;
			flags |= UNSIGNED;
			base = 10;
			break;

		case 'X':
		case 'x':
			c = CT_INT;
			flags |= UNSIGNED;
			base = 16;
			break;

#ifndef NO_FLOATING_POINT
		case 'A': case 'E': case 'F': case 'G':
		case 'a': case 'e': case 'f': case 'g':
			c = CT_FLOAT;
			break;
#endif

		case 'S':
			flags |= LONG;
			/* FALLTHROUGH */
		case 's':
			c = CT_STRING;
			break;

		case '[':
			fmt = __sccl(ccltab, fmt);
			flags |= NOSKIP;
			c = CT_CCL;
			break;

		case 'C':
			flags |= LONG;
			/* FALLTHROUGH */
		case 'c':
			flags |= NOSKIP;
			c = CT_CHAR;
			break;

		case 'p':	/* pointer format is like hex */
			flags |= POINTER;
			c = CT_INT;		/* assumes sizeof(uintmax_t) */
			flags |= UNSIGNED;	/*      >= sizeof(uintptr_t) */
			base = 16;
			break;

		case 'n':
			if (flags & SUPPRESS)	/* ??? */
				continue;
			if (flags & SHORTSHORT)
				*va_arg(ap, char *) = nread;
			else if (flags & SHORT)
				*va_arg(ap, short *) = nread;
			else if (flags & LONG)
				*va_arg(ap, long *) = nread;
			else if (flags & LONGLONG)
				*va_arg(ap, long long *) = nread;
			else if (flags & INTMAXT)
				*va_arg(ap, intmax_t *) = nread;
			else if (flags & SIZET)
				*va_arg(ap, size_t *) = nread;
			else if (flags & PTRDIFFT)
				*va_arg(ap, ptrdiff_t *) = nread;
			else
				*va_arg(ap, int *) = nread;
			continue;

		default:
			goto match_failure;

		/*
		 * Disgusting backwards compatibility hack.	XXX
		 */
		case '\0':	/* compat */
			return (EOF);
		}

		/*
		 * We have a conversion that requires input.
		 */
		if (fp->_r <= 0 && __srefill(fp))
			goto input_failure;

		/*
		 * Consume leading white space, except for formats
		 * that suppress this.
		 */
		if ((flags & NOSKIP) == 0) {
			while (isspace(*fp->_p)) {
				nread++;
				if (--fp->_r > 0)
					fp->_p++;
				else if (__srefill(fp))
					goto input_failure;
			}
			/*
			 * Note that there is at least one character in
			 * the buffer, so conversions that do not set NOSKIP
			 * ca no longer result in an input failure.
			 */
		}

		/*
		 * Do the conversion.
		 */
		switch (c) {

		case CT_CHAR:
			/* scan arbitrary characters (sets NOSKIP) */
			if (width == 0)
				width = 1;
			if (flags & LONG) {
				nr = convert_wchar(fp, GETARG(wchar_t *),
				    width, locale);
			} else {
				nr = convert_char(fp, GETARG(char *), width);
			}
			if (nr < 0)
				goto input_failure;
			break;

		case CT_CCL:
			/* scan a (nonempty) character class (sets NOSKIP) */
			if (width == 0)
				width = (size_t)~0;	/* `infinity' */
			if (flags & LONG) {
				nr = convert_wccl(fp, GETARG(wchar_t *), width,
				    ccltab, locale);
			} else {
				nr = convert_ccl(fp, GETARG(char *), width,
				    ccltab);
			}
			if (nr <= 0) {
				if (nr < 0)
					goto input_failure;
				else /* nr == 0 */
					goto match_failure;
			}
			break;

		case CT_STRING:
			/* like CCL, but zero-length string OK, & no NOSKIP */
			if (width == 0)
				width = (size_t)~0;
			if (flags & LONG) {
				nr = convert_wstring(fp, GETARG(wchar_t *),
				    width, locale);
			} else {
				nr = convert_string(fp, GETARG(char *), width);
			}
			if (nr < 0)
				goto input_failure;
			break;

		case CT_INT:
			/* scan an integer as if by the conversion function */
#ifdef hardway
			if (width == 0 || width > sizeof(buf) - 1)
				width = sizeof(buf) - 1;
#else
			/* size_t is unsigned, hence this optimisation */
			if (--width > sizeof(buf) - 2)
				width = sizeof(buf) - 2;
			width++;
#endif
			nr = parseint(fp, buf, width, base);
			if (nr == 0)
				goto match_failure;
			if ((flags & SUPPRESS) == 0) {
				uintmax_t res;

				buf[nr] = '\0';
				if ((flags & UNSIGNED) == 0)
				    res = strtoimax_l(buf, (char **)NULL, base, locale);
				else
				    res = strtoumax_l(buf, (char **)NULL, base, locale);
				if (flags & POINTER)
					*va_arg(ap, void **) =
							(void *)(uintptr_t)res;
				else if (flags & SHORTSHORT)
					*va_arg(ap, char *) = res;
				else if (flags & SHORT)
					*va_arg(ap, short *) = res;
				else if (flags & LONG)
					*va_arg(ap, long *) = res;
				else if (flags & LONGLONG)
					*va_arg(ap, long long *) = res;
				else if (flags & INTMAXT)
					*va_arg(ap, intmax_t *) = res;
				else if (flags & PTRDIFFT)
					*va_arg(ap, ptrdiff_t *) = res;
				else if (flags & SIZET)
					*va_arg(ap, size_t *) = res;
				else
					*va_arg(ap, int *) = res;
			}
			break;

#ifndef NO_FLOATING_POINT
		case CT_FLOAT:
			/* scan a floating point number as if by strtod */
			if (width == 0 || width > sizeof(buf) - 1)
				width = sizeof(buf) - 1;
			nr = parsefloat(fp, buf, buf + width, locale);
			if (nr == 0)
				goto match_failure;
			if ((flags & SUPPRESS) == 0) {
				if (flags & LONGDBL) {
					long double res = strtold_l(buf, NULL,
					    locale);
					*va_arg(ap, long double *) = res;
				} else if (flags & LONG) {
					double res = strtod_l(buf, NULL,
					    locale);
					*va_arg(ap, double *) = res;
				} else {
					float res = strtof_l(buf, NULL, locale);
					*va_arg(ap, float *) = res;
				}
			}
			break;
#endif /* !NO_FLOATING_POINT */
		}
		if (!(flags & SUPPRESS))
			nassigned++;
		nread += nr;
		nconversions++;
	}
input_failure:
	return (nconversions != 0 ? nassigned : EOF);
match_failure:
	return (nassigned);
}

/*
 * Fill in the given table from the scanset at the given format
 * (just after `[').  Return a pointer to the character past the
 * closing `]'.  The table has a 1 wherever characters should be
 * considered part of the scanset.
 */
static const u_char *
__sccl(char *tab, const u_char *fmt)
{
	int c, n, v, i;
	struct xlocale_collate *table =
		(struct xlocale_collate*)__get_locale()->components[XLC_COLLATE];

	/* first `clear' the whole table */
	c = *fmt++;		/* first char hat => negated scanset */
	if (c == '^') {
		v = 1;		/* default => accept */
		c = *fmt++;	/* get new first char */
	} else
		v = 0;		/* default => reject */

	/* XXX: Will not work if sizeof(tab*) > sizeof(char) */
	(void) memset(tab, v, 256);

	if (c == 0)
		return (fmt - 1);/* format ended before closing ] */

	/*
	 * Now set the entries corresponding to the actual scanset
	 * to the opposite of the above.
	 *
	 * The first character may be ']' (or '-') without being special;
	 * the last character may be '-'.
	 */
	v = 1 - v;
	for (;;) {
		tab[c] = v;		/* take character c */
doswitch:
		n = *fmt++;		/* and examine the next */
		switch (n) {

		case 0:			/* format ended too soon */
			return (fmt - 1);

		case '-':
			/*
			 * A scanset of the form
			 *	[01+-]
			 * is defined as `the digit 0, the digit 1,
			 * the character +, the character -', but
			 * the effect of a scanset such as
			 *	[a-zA-Z0-9]
			 * is implementation defined.  The V7 Unix
			 * scanf treats `a-z' as `the letters a through
			 * z', but treats `a-a' as `the letter a, the
			 * character -, and the letter a'.
			 *
			 * For compatibility, the `-' is not considered
			 * to define a range if the character following
			 * it is either a close bracket (required by ANSI)
			 * or is not numerically greater than the character
			 * we just stored in the table (c).
			 */
			n = *fmt;
			if (n == ']'
			    || (table->__collate_load_error ? n < c :
				__collate_range_cmp(n, c) < 0
			       )
			   ) {
				c = '-';
				break;	/* resume the for(;;) */
			}
			fmt++;
			/* fill in the range */
			if (table->__collate_load_error) {
				do {
					tab[++c] = v;
				} while (c < n);
			} else {
				for (i = 0; i < 256; i ++)
					if (__collate_range_cmp(c, i) <= 0 &&
					    __collate_range_cmp(i, n) <= 0
					   )
						tab[i] = v;
			}
#if 1	/* XXX another disgusting compatibility hack */
			c = n;
			/*
			 * Alas, the V7 Unix scanf also treats formats
			 * such as [a-c-e] as `the letters a through e'.
			 * This too is permitted by the standard....
			 */
			goto doswitch;
#else
			c = *fmt++;
			if (c == 0)
				return (fmt - 1);
			if (c == ']')
				return (fmt);
#endif
			break;

		case ']':		/* end of scanset */
			return (fmt);

		default:		/* just another character */
			c = n;
			break;
		}
	}
	/* NOTREACHED */
}

#ifndef NO_FLOATING_POINT
static int
parsefloat(FILE *fp, char *buf, char *end, locale_t locale)
{
	char *commit, *p;
	int infnanpos = 0, decptpos = 0;
	enum {
		S_START, S_GOTSIGN, S_INF, S_NAN, S_DONE, S_MAYBEHEX,
		S_DIGITS, S_DECPT, S_FRAC, S_EXP, S_EXPDIGITS
	} state = S_START;
	unsigned char c;
	const char *decpt = localeconv_l(locale)->decimal_point;
	_Bool gotmantdig = 0, ishex = 0;

	/*
	 * We set commit = p whenever the string we have read so far
	 * constitutes a valid representation of a floating point
	 * number by itself.  At some point, the parse will complete
	 * or fail, and we will ungetc() back to the last commit point.
	 * To ensure that the file offset gets updated properly, it is
	 * always necessary to read at least one character that doesn't
	 * match; thus, we can't short-circuit "infinity" or "nan(...)".
	 */
	commit = buf - 1;
	for (p = buf; p < end; ) {
		c = *fp->_p;
reswitch:
		switch (state) {
		case S_START:
			state = S_GOTSIGN;
			if (c == '-' || c == '+')
				break;
			else
				goto reswitch;
		case S_GOTSIGN:
			switch (c) {
			case '0':
				state = S_MAYBEHEX;
				commit = p;
				break;
			case 'I':
			case 'i':
				state = S_INF;
				break;
			case 'N':
			case 'n':
				state = S_NAN;
				break;
			default:
				state = S_DIGITS;
				goto reswitch;
			}
			break;
		case S_INF:
			if (infnanpos > 6 ||
			    (c != "nfinity"[infnanpos] &&
			     c != "NFINITY"[infnanpos]))
				goto parsedone;
			if (infnanpos == 1 || infnanpos == 6)
				commit = p;	/* inf or infinity */
			infnanpos++;
			break;
		case S_NAN:
			switch (infnanpos) {
			case 0:
				if (c != 'A' && c != 'a')
					goto parsedone;
				break;
			case 1:
				if (c != 'N' && c != 'n')
					goto parsedone;
				else
					commit = p;
				break;
			case 2:
				if (c != '(')
					goto parsedone;
				break;
			default:
				if (c == ')') {
					commit = p;
					state = S_DONE;
				} else if (!isalnum(c) && c != '_')
					goto parsedone;
				break;
			}
			infnanpos++;
			break;
		case S_DONE:
			goto parsedone;
		case S_MAYBEHEX:
			state = S_DIGITS;
			if (c == 'X' || c == 'x') {
				ishex = 1;
				break;
			} else {	/* we saw a '0', but no 'x' */
				gotmantdig = 1;
				goto reswitch;
			}
		case S_DIGITS:
			if ((ishex && isxdigit(c)) || isdigit(c)) {
				gotmantdig = 1;
				commit = p;
				break;
			} else {
				state = S_DECPT;
				goto reswitch;
			}
		case S_DECPT:
			if (c == decpt[decptpos]) {
				if (decpt[++decptpos] == '\0') {
					/* We read the complete decpt seq. */
					state = S_FRAC;
					if (gotmantdig)
						commit = p;
				}
				break;
			} else if (!decptpos) {
				/* We didn't read any decpt characters. */
				state = S_FRAC;
				goto reswitch;
			} else {
				/*
				 * We read part of a multibyte decimal point,
				 * but the rest is invalid, so bail.
				 */
				goto parsedone;
			}
		case S_FRAC:
			if (((c == 'E' || c == 'e') && !ishex) ||
			    ((c == 'P' || c == 'p') && ishex)) {
				if (!gotmantdig)
					goto parsedone;
				else
					state = S_EXP;
			} else if ((ishex && isxdigit(c)) || isdigit(c)) {
				commit = p;
				gotmantdig = 1;
			} else
				goto parsedone;
			break;
		case S_EXP:
			state = S_EXPDIGITS;
			if (c == '-' || c == '+')
				break;
			else
				goto reswitch;
		case S_EXPDIGITS:
			if (isdigit(c))
				commit = p;
			else
				goto parsedone;
			break;
		default:
			abort();
		}
		*p++ = c;
		if (--fp->_r > 0)
			fp->_p++;
		else if (__srefill(fp))
			break;	/* EOF */
	}

parsedone:
	while (commit < --p)
		__ungetc(*(u_char *)p, fp);
	*++commit = '\0';
	return (commit - buf);
}
#endif
