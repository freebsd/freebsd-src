/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#if 0
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)vfscanf.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
__FBSDID("FreeBSD: src/lib/libc/stdio/vfscanf.c,v 1.24 2002/08/13 09:30:41 tjr Exp ");
#endif
__FBSDID("$FreeBSD$");

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

#include "libc_private.h"
#include "local.h"

#define FLOATING_POINT

#ifdef FLOATING_POINT
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
#define	LONGLONG	0x400	/* ll: long long (+ deprecated q: quad) */
#define	INTMAXT		0x800	/* j: intmax_t */
#define	PTRDIFFT	0x1000	/* t: ptrdiff_t */
#define	SIZET		0x2000	/* z: size_t */
#define	SHORTSHORT	0x4000	/* hh: char */
#define	UNSIGNED	0x8000	/* %[oupxX] conversions */

/*
 * The following are used in numeric conversions only:
 * SIGNOK, NDIGITS, DPTOK, and EXPOK are for floating point;
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS are for integral.
 */
#define	SIGNOK		0x40	/* +/- is (still) legal */
#define	NDIGITS		0x80	/* no digits detected */

#define	DPTOK		0x100	/* (float) decimal point is still legal */
#define	EXPOK		0x200	/* (float) exponent (e+3, etc) still legal */

#define	PFXOK		0x100	/* 0x prefix is (still) legal */
#define	NZDIGITS	0x200	/* no zero digits detected */

/*
 * Conversion types.
 */
#define	CT_CHAR		0	/* %c conversion */
#define	CT_CCL		1	/* %[...] conversion */
#define	CT_STRING	2	/* %s conversion */
#define	CT_INT		3	/* %[dioupxX] conversion */
#define	CT_FLOAT	4	/* %[efgEFG] conversion */

#define	INCCL(_c)	\
	(cclcompl ? (wmemchr(ccls, (_c), ccle - ccls) == NULL) : \
	(wmemchr(ccls, (_c), ccle - ccls) != NULL))

/*
 * MT-safe version.
 */
int
vfwscanf(FILE * __restrict fp, const wchar_t * __restrict fmt, va_list ap)
{
	int ret;

	FLOCKFILE(fp);
	ORIENT(fp, 1);
	ret = __vfwscanf(fp, fmt, ap);
	FUNLOCKFILE(fp);
	return (ret);
}

/*
 * Non-MT-safe version.
 */
int
__vfwscanf(FILE * __restrict fp, const wchar_t * __restrict fmt, va_list ap)
{
	wint_t c;		/* character from format, or conversion */
	size_t width;		/* field width, or 0 */
	wchar_t *p;		/* points into all kinds of strings */
	int n;			/* handy integer */
	int flags;		/* flags as defined above */
	wchar_t *p0;		/* saves original value of p when necessary */
	int nassigned;		/* number of fields assigned */
	int nconversions;	/* number of conversions */
	int nread;		/* number of characters consumed from fp */
	int base;		/* base argument to conversion function */
	wchar_t buf[BUF];	/* buffer for numeric conversions */
	const wchar_t *ccls;	/* character class start */
	const wchar_t *ccle;	/* character class end */
	int cclcompl;		/* ccl is complemented? */
	wint_t wi;		/* handy wint_t */
	char *mbp;		/* multibyte string pointer for %c %s %[ */
	size_t nconv;		/* number of bytes in mb. conversion */
	mbstate_t mbs;		/* multibyte state */
	char mbbuf[MB_LEN_MAX];	/* temporary mb. character buffer */

	/* `basefix' is used to avoid `if' tests in the integer scanner */
	static short basefix[17] =
		{ 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
#ifdef FLOATING_POINT
	char decimal_point = localeconv()->decimal_point[0];
#endif

	nassigned = 0;
	nconversions = 0;
	nread = 0;
	ccls = ccle = NULL;
	for (;;) {
		c = *fmt++;
		if (c == 0)
			return (nassigned);
		if (iswspace(c)) {
			while ((c = __fgetwc(fp)) != WEOF &&
			    iswspace(c))
				;
			if (c != WEOF)
				__ungetwc(c, fp);
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
			if ((wi = __fgetwc(fp)) == WEOF)
				goto input_failure;
			if (wi != c) {
				__ungetwc(wi, fp);
				goto input_failure;
			}
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
			flags |= PFXOK;	/* enable 0x prefixing */
			c = CT_INT;
			flags |= UNSIGNED;
			base = 16;
			break;

#ifdef FLOATING_POINT
		case 'E': case 'F': case 'G':
		case 'e': case 'f': case 'g':
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
			ccls = fmt;
			if (*fmt == '^') {
				cclcompl = 1;
				fmt++;
			} else
				cclcompl = 0;
			if (*fmt == ']')
				fmt++;
			while (*fmt != '\0' && *fmt != ']')
				fmt++;
			ccle = fmt;
			fmt++;
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
			flags |= POINTER | PFXOK;
			c = CT_INT;		/* assumes sizeof(uintmax_t) */
			flags |= UNSIGNED;	/*      >= sizeof(uintptr_t) */
			base = 16;
			break;

		case 'n':
			nconversions++;
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
		 * Consume leading white space, except for formats
		 * that suppress this.
		 */
		if ((flags & NOSKIP) == 0) {
			while ((wi = __fgetwc(fp)) != WEOF && iswspace(wi))
				nread++;
			if (wi == WEOF)
				goto input_failure;
			__ungetwc(wi, fp);
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
				if (!(flags & SUPPRESS))
					p = va_arg(ap, wchar_t *);
				n = 0;
				while (width-- != 0 &&
				    (wi = __fgetwc(fp)) != WEOF) {
					if (!(flags & SUPPRESS))
						*p++ = (wchar_t)wi;
					n++;
				}
				if (n == 0)
					goto input_failure;
				nread += n;
				if (!(flags & SUPPRESS))
					nassigned++;
			} else {
				if (!(flags & SUPPRESS))
					mbp = va_arg(ap, char *);
				n = 0;
				memset(&mbs, 0, sizeof(mbs));
				while (width != 0 &&
				    (wi = __fgetwc(fp)) != WEOF) {
					if (width >= MB_CUR_MAX &&
					    !(flags & SUPPRESS)) {
						nconv = wcrtomb(mbp, wi, &mbs);
						if (nconv == (size_t)-1)
							goto input_failure;
					} else {
						nconv = wcrtomb(mbbuf, wi,
						    &mbs);
						if (nconv == (size_t)-1)
							goto input_failure;
						if (nconv > width) {
							__ungetwc(wi, fp);
							break;
						}
						if (!(flags & SUPPRESS))
							memcpy(mbp, mbbuf,
							    nconv);
					}
					if (!(flags & SUPPRESS))
						mbp += nconv;
					width -= nconv;
					n++;
				}
				if (n == 0)
					goto input_failure;
				nread += n;
				if (!(flags & SUPPRESS))
					nassigned++;
			}
			nconversions++;
			break;

		case CT_CCL:
			/* scan a (nonempty) character class (sets NOSKIP) */
			if (width == 0)
				width = (size_t)~0;	/* `infinity' */
			/* take only those things in the class */
			if ((flags & SUPPRESS) && (flags & LONG)) {
				n = 0;
				while ((wi = __fgetwc(fp)) != WEOF &&
				    width-- != 0 && INCCL(wi))
					n++;
				if (wi != WEOF)
					__ungetwc(wi, fp);
				if (n == 0)
					goto match_failure;
			} else if (flags & LONG) {
				p0 = p = va_arg(ap, wchar_t *);
				while ((wi = __fgetwc(fp)) != WEOF &&
				    width-- != 0 && INCCL(wi))
					*p++ = (wchar_t)wi;
				if (wi != WEOF)
					__ungetwc(wi, fp);
				n = p - p0;
				if (n == 0)
					goto match_failure;
				*p = 0;
				nassigned++;
			} else {
				if (!(flags & SUPPRESS))
					mbp = va_arg(ap, char *);
				n = 0;
				memset(&mbs, 0, sizeof(mbs));
				while ((wi = __fgetwc(fp)) != WEOF &&
				    width != 0 && INCCL(wi)) {
					if (width >= MB_CUR_MAX &&
					   !(flags & SUPPRESS)) {
						nconv = wcrtomb(mbp, wi, &mbs);
						if (nconv == (size_t)-1)
							goto input_failure;
					} else {
						nconv = wcrtomb(mbbuf, wi,
						    &mbs);
						if (nconv == (size_t)-1)
							goto input_failure;
						if (nconv > width)
							break;
						if (!(flags & SUPPRESS))
							memcpy(mbp, mbbuf,
							    nconv);
					}
					if (!(flags & SUPPRESS))
						mbp += nconv;
					width -= nconv;
					n++;
				}
				if (wi != WEOF)
					__ungetwc(wi, fp);
				if (!(flags & SUPPRESS)) {
					*mbp = 0;
					nassigned++;
				}
			}
			nread += n;
			nconversions++;
			break;

		case CT_STRING:
			/* like CCL, but zero-length string OK, & no NOSKIP */
			if (width == 0)
				width = (size_t)~0;
			if ((flags & SUPPRESS) && (flags & LONG)) {
				while ((wi = __fgetwc(fp)) != WEOF &&
				    width-- != 0 &&
				    !iswspace(wi))
					nread++;
				if (wi != WEOF)
					__ungetwc(wi, fp);
			} else if (flags & LONG) {
				p0 = p = va_arg(ap, wchar_t *);
				while ((wi = __fgetwc(fp)) != WEOF &&
				    width-- != 0 &&
				    !iswspace(wi)) {
					*p++ = (wchar_t)wi;
					nread++;
				}
				if (wi != WEOF)
					__ungetwc(wi, fp);
				*p = '\0';
				nassigned++;
			} else {
				if (!(flags & SUPPRESS))
					mbp = va_arg(ap, char *);
				memset(&mbs, 0, sizeof(mbs));
				while ((wi = __fgetwc(fp)) != WEOF &&
				    width != 0 &&
				    !iswspace(wi)) {
					if (width >= MB_CUR_MAX &&
					    !(flags & SUPPRESS)) {
						nconv = wcrtomb(mbp, wi, &mbs);
						if (nconv == (size_t)-1)
							goto input_failure;
					} else {
						nconv = wcrtomb(mbbuf, wi,
						    &mbs);
						if (nconv == (size_t)-1)
							goto input_failure;
						if (nconv > width)
							break;
						if (!(flags & SUPPRESS))
							memcpy(mbp, mbbuf,
							    nconv);
					}
					if (!(flags & SUPPRESS))
						mbp += nconv;
					width -= nconv;
					nread++;
				}
				if (wi != WEOF)
					__ungetwc(wi, fp);
				if (!(flags & SUPPRESS)) {
					*mbp = 0;
					nassigned++;
				}
			}
			nconversions++;
			continue;

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
			flags |= SIGNOK | NDIGITS | NZDIGITS;
			for (p = buf; width; width--) {
				c = __fgetwc(fp);
				/*
				 * Switch on the character; `goto ok'
				 * if we accept it as a part of number.
				 */
				switch (c) {

				/*
				 * The digit 0 is always legal, but is
				 * special.  For %i conversions, if no
				 * digits (zero or nonzero) have been
				 * scanned (only signs), we will have
				 * base==0.  In that case, we should set
				 * it to 8 and enable 0x prefixing.
				 * Also, if we have not scanned zero digits
				 * before this, do not turn off prefixing
				 * (someone else will turn it off if we
				 * have scanned any nonzero digits).
				 */
				case '0':
					if (base == 0) {
						base = 8;
						flags |= PFXOK;
					}
					if (flags & NZDIGITS)
					    flags &= ~(SIGNOK|NZDIGITS|NDIGITS);
					else
					    flags &= ~(SIGNOK|PFXOK|NDIGITS);
					goto ok;

				/* 1 through 7 always legal */
				case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					base = basefix[base];
					flags &= ~(SIGNOK | PFXOK | NDIGITS);
					goto ok;

				/* digits 8 and 9 ok iff decimal or hex */
				case '8': case '9':
					base = basefix[base];
					if (base <= 8)
						break;	/* not legal here */
					flags &= ~(SIGNOK | PFXOK | NDIGITS);
					goto ok;

				/* letters ok iff hex */
				case 'A': case 'B': case 'C':
				case 'D': case 'E': case 'F':
				case 'a': case 'b': case 'c':
				case 'd': case 'e': case 'f':
					/* no need to fix base here */
					if (base <= 10)
						break;	/* not legal here */
					flags &= ~(SIGNOK | PFXOK | NDIGITS);
					goto ok;

				/* sign ok only as first character */
				case '+': case '-':
					if (flags & SIGNOK) {
						flags &= ~SIGNOK;
						goto ok;
					}
					break;

				/* x ok iff flag still set & 2nd char */
				case 'x': case 'X':
					if (flags & PFXOK && p == buf + 1) {
						base = 16;	/* if %i */
						flags &= ~PFXOK;
						goto ok;
					}
					break;
				}

				/*
				 * If we got here, c is not a legal character
				 * for a number.  Stop accumulating digits.
				 */
				if (c != WEOF)
					__ungetwc(c, fp);
				break;
		ok:
				/*
				 * c is legal: store it and look at the next.
				 */
				*p++ = (wchar_t)c;
			}
			/*
			 * If we had only a sign, it is no good; push
			 * back the sign.  If the number ends in `x',
			 * it was [sign] '0' 'x', so push back the x
			 * and treat it as [sign] '0'.
			 */
			if (flags & NDIGITS) {
				if (p > buf)
					__ungetwc(*--p, fp);
				goto match_failure;
			}
			c = p[-1];
			if (c == 'x' || c == 'X') {
				--p;
				__ungetwc(c, fp);
			}
			if ((flags & SUPPRESS) == 0) {
				uintmax_t res;

				*p = 0;
				if ((flags & UNSIGNED) == 0)
				    res = wcstoimax(buf, NULL, base);
				else
				    res = wcstoumax(buf, NULL, base);
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
				nassigned++;
			}
			nread += p - buf;
			nconversions++;
			break;

#ifdef FLOATING_POINT
		case CT_FLOAT:
			/* scan a floating point number as if by strtod */
#ifdef hardway
			if (width == 0 || width > sizeof(buf) - 1)
				width = sizeof(buf) - 1;
#else
			/* size_t is unsigned, hence this optimisation */
			if (--width > sizeof(buf) - 2)
				width = sizeof(buf) - 2;
			width++;
#endif
			flags |= SIGNOK | NDIGITS | DPTOK | EXPOK;
			for (p = buf; width; width--) {
				c = __fgetwc(fp);
				/*
				 * This code mimicks the integer conversion
				 * code, but is much simpler.
				 */
				switch (c) {

				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				case '8': case '9':
					flags &= ~(SIGNOK | NDIGITS);
					goto fok;

				case '+': case '-':
					if (flags & SIGNOK) {
						flags &= ~SIGNOK;
						goto fok;
					}
					break;
				case 'e': case 'E':
					/* no exponent without some digits */
					if ((flags&(NDIGITS|EXPOK)) == EXPOK) {
						flags =
						    (flags & ~(EXPOK|DPTOK)) |
						    SIGNOK | NDIGITS;
						goto fok;
					}
					break;
				default:
					if (c == (wchar_t)decimal_point &&
					    (flags & DPTOK)) {
						flags &= ~(SIGNOK | DPTOK);
						goto fok;
					}
					break;
				}
				if (c != WEOF)
					__ungetwc(c, fp);
				break;
		fok:
				*p++ = c;
			}
			/*
			 * If no digits, might be missing exponent digits
			 * (just give back the exponent) or might be missing
			 * regular digits, but had sign and/or decimal point.
			 */
			if (flags & NDIGITS) {
				if (flags & EXPOK) {
					/* no digits at all */
					while (p > buf)
						__ungetwc(*--p, fp);
					goto match_failure;
				}
				/* just a bad exponent (e and maybe sign) */
				c = *--p;
				if (c != 'e' && c != 'E') {
					__ungetwc(c, fp);/* sign */
					c = *--p;
				}
				__ungetwc(c, fp);
			}
			if ((flags & SUPPRESS) == 0) {
				double res;

				*p = 0;
				/* XXX this loses precision for long doubles. */
				res = wcstod(buf, NULL);
				if (flags & LONGDBL)
					*va_arg(ap, long double *) = res;
				else if (flags & LONG)
					*va_arg(ap, double *) = res;
				else
					*va_arg(ap, float *) = res;
				nassigned++;
			}
			nread += p - buf;
			nconversions++;
			break;
#endif /* FLOATING_POINT */
		}
	}
input_failure:
	return (nconversions != 0 ? nassigned : EOF);
match_failure:
	return (nassigned);
}
