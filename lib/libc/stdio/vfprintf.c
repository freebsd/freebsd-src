/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 *
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * Actual printf innards.
 *
 * This code is large and complicated...
 */

#include "namespace.h"
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <printf.h>

#include <stdarg.h>
#include "xlocale_private.h"
#include "un-namespace.h"

#include "libc_private.h"
#include "local.h"
#include "fvwrite.h"
#include "printflocal.h"

static int	__sprint(FILE *, struct __suio *, locale_t);
static int	__sbprintf(FILE *, locale_t, int, const char *, va_list)
	__printflike(4, 0)
	__noinline;
static char	*__wcsconv(wchar_t *, int);

#define	CHAR	char
#include "printfcommon.h"

struct grouping_state {
	char *thousands_sep;	/* locale-specific thousands separator */
	int thousep_len;	/* length of thousands_sep */
	const char *grouping;	/* locale-specific numeric grouping rules */
	int lead;		/* sig figs before decimal or group sep */
	int nseps;		/* number of group separators with ' */
	int nrepeats;		/* number of repeats of the last group */
};

/*
 * Initialize the thousands' grouping state in preparation to print a
 * number with ndigits digits. This routine returns the total number
 * of bytes that will be needed.
 */
static int
grouping_init(struct grouping_state *gs, int ndigits, locale_t loc)
{
	struct lconv *locale;

	locale = localeconv_l(loc);
	gs->grouping = locale->grouping;
	gs->thousands_sep = locale->thousands_sep;
	gs->thousep_len = strlen(gs->thousands_sep);

	gs->nseps = gs->nrepeats = 0;
	gs->lead = ndigits;
	while (*gs->grouping != CHAR_MAX) {
		if (gs->lead <= *gs->grouping)
			break;
		gs->lead -= *gs->grouping;
		if (*(gs->grouping+1)) {
			gs->nseps++;
			gs->grouping++;
		} else
			gs->nrepeats++;
	}
	return ((gs->nseps + gs->nrepeats) * gs->thousep_len);
}

/*
 * Print a number with thousands' separators.
 */
static int
grouping_print(struct grouping_state *gs, struct io_state *iop,
	       const CHAR *cp, const CHAR *ep, locale_t locale)
{
	const CHAR *cp0 = cp;

	if (io_printandpad(iop, cp, ep, gs->lead, zeroes, locale))
		return (-1);
	cp += gs->lead;
	while (gs->nseps > 0 || gs->nrepeats > 0) {
		if (gs->nrepeats > 0)
			gs->nrepeats--;
		else {
			gs->grouping--;
			gs->nseps--;
		}
		if (io_print(iop, gs->thousands_sep, gs->thousep_len, locale))
			return (-1);
		if (io_printandpad(iop, cp, ep, *gs->grouping, zeroes, locale))
			return (-1);
		cp += *gs->grouping;
	}
	if (cp > ep)
		cp = ep;
	return (cp - cp0);
}

/*
 * Flush out all the vectors defined by the given uio,
 * then reset it so that it can be reused.
 */
static int
__sprint(FILE *fp, struct __suio *uio, locale_t locale)
{
	int err;

	if (uio->uio_resid == 0) {
		uio->uio_iovcnt = 0;
		return (0);
	}
	err = __sfvwrite(fp, uio);
	uio->uio_resid = 0;
	uio->uio_iovcnt = 0;
	return (err);
}

/*
 * Helper function for `fprintf to unbuffered unix file': creates a
 * temporary buffer.  We only work on write-only files; this avoids
 * worries about ungetc buffers and so forth.
 */
static int
__sbprintf(FILE *fp, locale_t locale, int serrno, const char *fmt, va_list ap)
{
	int ret;
	FILE fake = FAKE_FILE;
	unsigned char buf[BUFSIZ];

	/* XXX This is probably not needed. */
	if (prepwrite(fp) != 0)
		return (EOF);

	/* copy the important variables */
	fake._flags = fp->_flags & ~__SNBF;
	fake._file = fp->_file;
	fake._cookie = fp->_cookie;
	fake._write = fp->_write;
	fake._orientation = fp->_orientation;
	fake._mbstate = fp->_mbstate;

	/* set up the buffer */
	fake._bf._base = fake._p = buf;
	fake._bf._size = fake._w = sizeof(buf);
	fake._lbfsize = 0;	/* not actually used, but Just In Case */

	/* do the work, then copy any error status */
	ret = __vfprintf(&fake, locale, serrno, fmt, ap);
	if (ret >= 0 && __fflush(&fake))
		ret = EOF;
	if (fake._flags & __SERR)
		fp->_flags |= __SERR;
	return (ret);
}

/*
 * Convert a wide character string argument for the %ls format to a multibyte
 * string representation. If not -1, prec specifies the maximum number of
 * bytes to output, and also means that we can't assume that the wide char.
 * string ends is null-terminated.
 */
static char *
__wcsconv(wchar_t *wcsarg, int prec)
{
	static const mbstate_t initial;
	mbstate_t mbs;
	char buf[MB_LEN_MAX];
	wchar_t *p;
	char *convbuf;
	size_t clen, nbytes;

	/* Allocate space for the maximum number of bytes we could output. */
	if (prec < 0) {
		p = wcsarg;
		mbs = initial;
		nbytes = wcsrtombs(NULL, (const wchar_t **)&p, 0, &mbs);
		if (nbytes == (size_t)-1)
			return (NULL);
	} else {
		/*
		 * Optimisation: if the output precision is small enough,
		 * just allocate enough memory for the maximum instead of
		 * scanning the string.
		 */
		if (prec < 128)
			nbytes = prec;
		else {
			nbytes = 0;
			p = wcsarg;
			mbs = initial;
			for (;;) {
				clen = wcrtomb(buf, *p++, &mbs);
				if (clen == 0 || clen == (size_t)-1 ||
				    nbytes + clen > prec)
					break;
				nbytes += clen;
			}
		}
	}
	if ((convbuf = malloc(nbytes + 1)) == NULL)
		return (NULL);

	/* Fill the output buffer. */
	p = wcsarg;
	mbs = initial;
	if ((nbytes = wcsrtombs(convbuf, (const wchar_t **)&p,
	    nbytes, &mbs)) == (size_t)-1) {
		free(convbuf);
		return (NULL);
	}
	convbuf[nbytes] = '\0';
	return (convbuf);
}

/*
 * MT-safe version
 */
int
vfprintf_l(FILE * __restrict fp, locale_t locale, const char * __restrict fmt0,
    va_list ap)
{
	int serrno = errno;
	int ret;
	FIX_LOCALE(locale);

	FLOCKFILE_CANCELSAFE(fp);
	/* optimise fprintf(stderr) (and other unbuffered Unix files) */
	if ((fp->_flags & (__SNBF|__SWR|__SRW)) == (__SNBF|__SWR) &&
	    fp->_file >= 0)
		ret = __sbprintf(fp, locale, serrno, fmt0, ap);
	else
		ret = __vfprintf(fp, locale, serrno, fmt0, ap);
	FUNLOCKFILE_CANCELSAFE();
	return (ret);
}
int
vfprintf(FILE * __restrict fp, const char * __restrict fmt0, va_list ap)
{
	return vfprintf_l(fp, __get_locale(), fmt0, ap);
}

/*
 * The size of the buffer we use as scratch space for integer
 * conversions, among other things.  We need enough space to
 * write a uintmax_t in binary.
 */
#define BUF	(sizeof(uintmax_t) * CHAR_BIT)

/*
 * Non-MT-safe version
 */
int
__vfprintf(FILE *fp, locale_t locale, int serrno, const char *fmt0, va_list ap)
{
	char *fmt;		/* format string */
	int ch;			/* character from fmt */
	int n, n2;		/* handy integer (short term usage) */
	char *cp;		/* handy char pointer (short term usage) */
	int flags;		/* flags as above */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format; <0 for N/A */
	int error;
	char errnomsg[NL_TEXTMAX];
	char sign;		/* sign prefix (' ', '+', '-', or \0) */
	struct grouping_state gs; /* thousands' grouping info */

#ifndef NO_FLOATING_POINT
	/*
	 * We can decompose the printed representation of floating
	 * point numbers into several parts, some of which may be empty:
	 *
	 * [+|-| ] [0x|0X] MMM . NNN [e|E|p|P] [+|-] ZZ
	 *    A       B     ---C---      D       E   F
	 *
	 * A:	'sign' holds this value if present; '\0' otherwise
	 * B:	ox[1] holds the 'x' or 'X'; '\0' if not hexadecimal
	 * C:	cp points to the string MMMNNN.  Leading and trailing
	 *	zeros are not in the string and must be added.
	 * D:	expchar holds this character; '\0' if no exponent, e.g. %f
	 * F:	at least two digits for decimal, at least one digit for hex
	 */
	char *decimal_point;	/* locale specific decimal point */
	int decpt_len;		/* length of decimal_point */
	int signflag;		/* true if float is negative */
	union {			/* floating point arguments %[aAeEfFgG] */
		double dbl;
		long double ldbl;
	} fparg;
	int expt;		/* integer value of exponent */
	char expchar;		/* exponent character: [eEpP\0] */
	char *dtoaend;		/* pointer to end of converted digits */
	int expsize;		/* character count for expstr */
	int ndig;		/* actual number of digits returned by dtoa */
	char expstr[MAXEXPDIG+2];	/* buffer for exponent string: e+ZZZ */
	char *dtoaresult;	/* buffer allocated by dtoa */
#endif
	u_long	ulval;		/* integer arguments %[diouxX] */
	uintmax_t ujval;	/* %j, %ll, %q, %t, %z integers */
	int base;		/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int realsz;		/* field size expanded by dprec, sign, etc */
	int size;		/* size of converted field or string */
	int prsize;             /* max size of printed field */
	const char *xdigs;     	/* digits for %[xX] conversion */
	struct io_state io;	/* I/O buffering state */
	char buf[BUF];		/* buffer with space for digits of uintmax_t */
	char ox[2];		/* space for 0x; ox[1] is either x, X, or \0 */
	union arg *argtable;    /* args, built due to positional arg */
	union arg statargtable [STATIC_ARG_TBL_SIZE];
	int nextarg;            /* 1-based argument index */
	va_list orgap;          /* original argument pointer */
	char *convbuf;		/* wide to multibyte conversion result */
	int savserr;

	static const char xdigs_lower[16] = "0123456789abcdef";
	static const char xdigs_upper[16] = "0123456789ABCDEF";

	/* BEWARE, these `goto error' on error. */
#define	PRINT(ptr, len) { \
	if (io_print(&io, (ptr), (len), locale))	\
		goto error; \
}
#define	PAD(howmany, with) { \
	if (io_pad(&io, (howmany), (with), locale)) \
		goto error; \
}
#define	PRINTANDPAD(p, ep, len, with) {	\
	if (io_printandpad(&io, (p), (ep), (len), (with), locale)) \
		goto error; \
}
#define	FLUSH() { \
	if (io_flush(&io, locale)) \
		goto error; \
}

	/*
	 * Get the argument indexed by nextarg.   If the argument table is
	 * built, use it to get the argument.  If its not, get the next
	 * argument (and arguments must be gotten sequentially).
	 */
#define GETARG(type) \
	((argtable != NULL) ? *((type*)(&argtable[nextarg++])) : \
	    (nextarg++, va_arg(ap, type)))

	/*
	 * To extend shorts properly, we need both signed and unsigned
	 * argument extraction methods.
	 */
#define	SARG() \
	(flags&LONGINT ? GETARG(long) : \
	    flags&SHORTINT ? (long)(short)GETARG(int) : \
	    flags&CHARINT ? (long)(signed char)GETARG(int) : \
	    (long)GETARG(int))
#define	UARG() \
	(flags&LONGINT ? GETARG(u_long) : \
	    flags&SHORTINT ? (u_long)(u_short)GETARG(int) : \
	    flags&CHARINT ? (u_long)(u_char)GETARG(int) : \
	    (u_long)GETARG(u_int))
#define	INTMAX_SIZE	(INTMAXT|SIZET|PTRDIFFT|LLONGINT)
#define SJARG() \
	(flags&INTMAXT ? GETARG(intmax_t) : \
	    flags&SIZET ? (intmax_t)GETARG(ssize_t) : \
	    flags&PTRDIFFT ? (intmax_t)GETARG(ptrdiff_t) : \
	    (intmax_t)GETARG(long long))
#define	UJARG() \
	(flags&INTMAXT ? GETARG(uintmax_t) : \
	    flags&SIZET ? (uintmax_t)GETARG(size_t) : \
	    flags&PTRDIFFT ? (uintmax_t)GETARG(ptrdiff_t) : \
	    (uintmax_t)GETARG(unsigned long long))

	/*
	 * Get * arguments, including the form *nn$.  Preserve the nextarg
	 * that the argument can be gotten once the type is determined.
	 */
#define GETASTER(val) \
	n2 = 0; \
	cp = fmt; \
	while (is_digit(*cp)) { \
		n2 = 10 * n2 + to_digit(*cp); \
		cp++; \
	} \
	if (*cp == '$') { \
		int hold = nextarg; \
		if (argtable == NULL) { \
			argtable = statargtable; \
			if (__find_arguments (fmt0, orgap, &argtable)) { \
				ret = EOF; \
				goto error; \
			} \
		} \
		nextarg = n2; \
		val = GETARG (int); \
		nextarg = hold; \
		fmt = ++cp; \
	} else { \
		val = GETARG (int); \
	}

	if (__use_xprintf > 0)
		return (__xvprintf(fp, fmt0, ap));

	/* sorry, fprintf(read_only_file, "") returns EOF, not 0 */
	if (prepwrite(fp) != 0) {
		errno = EBADF;
		return (EOF);
	}

	savserr = fp->_flags & __SERR;
	fp->_flags &= ~__SERR;

	convbuf = NULL;
	fmt = (char *)fmt0;
	argtable = NULL;
	nextarg = 1;
	va_copy(orgap, ap);
	io_init(&io, fp);
	ret = 0;
#ifndef NO_FLOATING_POINT
	dtoaresult = NULL;
	decimal_point = localeconv_l(locale)->decimal_point;
	/* The overwhelmingly common case is decpt_len == 1. */
	decpt_len = (decimal_point[1] == '\0' ? 1 : strlen(decimal_point));
#endif

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		for (cp = fmt; (ch = *fmt) != '\0' && ch != '%'; fmt++)
			/* void */;
		if ((n = fmt - cp) != 0) {
			if ((unsigned)ret + n > INT_MAX) {
				ret = EOF;
				errno = EOVERFLOW;
				goto error;
			}
			PRINT(cp, n);
			ret += n;
		}
		if (ch == '\0')
			goto done;
		fmt++;		/* skip over '%' */

		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		gs.grouping = NULL;
		sign = '\0';
		ox[1] = '\0';

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
			/*-
			 * ``If the space and + flags both appear, the space
			 * flag will be ignored.''
			 *	-- ANSI X3J11
			 */
			if (!sign)
				sign = ' ';
			goto rflag;
		case '#':
			flags |= ALT;
			goto rflag;
		case '*':
			/*-
			 * ``A negative field width argument is taken as a
			 * - flag followed by a positive field width.''
			 *	-- ANSI X3J11
			 * They don't exclude field widths read from args.
			 */
			GETASTER (width);
			if (width >= 0)
				goto rflag;
			width = -width;
			/* FALLTHROUGH */
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '\'':
			flags |= GROUPING;
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				GETASTER (prec);
				goto rflag;
			}
			prec = 0;
			while (is_digit(ch)) {
				prec = 10 * prec + to_digit(ch);
				ch = *fmt++;
			}
			goto reswitch;
		case '0':
			/*-
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			if (ch == '$') {
				nextarg = n;
				if (argtable == NULL) {
					argtable = statargtable;
					if (__find_arguments (fmt0, orgap,
							      &argtable)) {
						ret = EOF;
						goto error;
					}
				}
				goto rflag;
			}
			width = n;
			goto reswitch;
#ifndef NO_FLOATING_POINT
		case 'L':
			flags |= LONGDBL;
			goto rflag;
#endif
		case 'h':
			if (flags & SHORTINT) {
				flags &= ~SHORTINT;
				flags |= CHARINT;
			} else
				flags |= SHORTINT;
			goto rflag;
		case 'j':
			flags |= INTMAXT;
			goto rflag;
		case 'l':
			if (flags & LONGINT) {
				flags &= ~LONGINT;
				flags |= LLONGINT;
			} else
				flags |= LONGINT;
			goto rflag;
		case 'q':
			flags |= LLONGINT;	/* not necessarily */
			goto rflag;
		case 't':
			flags |= PTRDIFFT;
			goto rflag;
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
			flags &= ~(CHARINT|SHORTINT|LONGINT|LLONGINT|INTMAXT);
			if (fmt[0] == 'f') {
				flags |= FASTINT;
				fmt++;
			} else {
				flags &= ~FASTINT;
			}
			if (fmt[0] == '8') {
				if (!(flags & FASTINT))
					flags |= CHARINT;
				else
					/* no flag set = 32 */ ;
				fmt += 1;
			} else if (fmt[0] == '1' && fmt[1] == '6') {
				if (!(flags & FASTINT))
					flags |= SHORTINT;
				else
					/* no flag set = 32 */ ;
				fmt += 2;
			} else if (fmt[0] == '3' && fmt[1] == '2') {
				/* no flag set = 32 */ ;
				fmt += 2;
			} else if (fmt[0] == '6' && fmt[1] == '4') {
				flags |= LLONGINT;
				fmt += 2;
			} else {
				if (flags & FASTINT) {
					flags &= ~FASTINT;
					fmt--;
				}
				goto invalid;
			}
			goto rflag;
		case 'z':
			flags |= SIZET;
			goto rflag;
		case 'B':
		case 'b':
			if (flags & INTMAX_SIZE)
				ujval = UJARG();
			else
				ulval = UARG();
			base = 2;
			/* leading 0b/B only if non-zero */
			if (flags & ALT &&
			    (flags & INTMAX_SIZE ? ujval != 0 : ulval != 0))
				ox[1] = ch;
			goto nosign;
			break;
		case 'C':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'c':
			if (flags & LONGINT) {
				static const mbstate_t initial;
				mbstate_t mbs;
				size_t mbseqlen;

				mbs = initial;
				mbseqlen = wcrtomb(cp = buf,
				    (wchar_t)GETARG(wint_t), &mbs);
				if (mbseqlen == (size_t)-1) {
					fp->_flags |= __SERR;
					goto error;
				}
				size = (int)mbseqlen;
			} else {
				*(cp = buf) = GETARG(int);
				size = 1;
			}
			sign = '\0';
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			if (flags & INTMAX_SIZE) {
				ujval = SJARG();
				if ((intmax_t)ujval < 0) {
					ujval = -ujval;
					sign = '-';
				}
			} else {
				ulval = SARG();
				if ((long)ulval < 0) {
					ulval = -ulval;
					sign = '-';
				}
			}
			base = 10;
			goto number;
#ifndef NO_FLOATING_POINT
		case 'a':
		case 'A':
			if (ch == 'a') {
				ox[1] = 'x';
				xdigs = xdigs_lower;
				expchar = 'p';
			} else {
				ox[1] = 'X';
				xdigs = xdigs_upper;
				expchar = 'P';
			}
			if (prec >= 0)
				prec++;
			if (dtoaresult != NULL)
				freedtoa(dtoaresult);
			if (flags & LONGDBL) {
				fparg.ldbl = GETARG(long double);
				dtoaresult = cp =
				    __hldtoa(fparg.ldbl, xdigs, prec,
				    &expt, &signflag, &dtoaend);
			} else {
				fparg.dbl = GETARG(double);
				dtoaresult = cp =
				    __hdtoa(fparg.dbl, xdigs, prec,
				    &expt, &signflag, &dtoaend);
			}
			if (prec < 0)
				prec = dtoaend - cp;
			if (expt == INT_MAX)
				ox[1] = '\0';
			goto fp_common;
		case 'e':
		case 'E':
			expchar = ch;
			if (prec < 0)	/* account for digit before decpt */
				prec = DEFPREC + 1;
			else
				prec++;
			goto fp_begin;
		case 'f':
		case 'F':
			expchar = '\0';
			goto fp_begin;
		case 'g':
		case 'G':
			expchar = ch - ('g' - 'e');
			if (prec == 0)
				prec = 1;
fp_begin:
			if (prec < 0)
				prec = DEFPREC;
			if (dtoaresult != NULL)
				freedtoa(dtoaresult);
			if (flags & LONGDBL) {
				fparg.ldbl = GETARG(long double);
				dtoaresult = cp =
				    __ldtoa(&fparg.ldbl, expchar ? 2 : 3, prec,
				    &expt, &signflag, &dtoaend);
			} else {
				fparg.dbl = GETARG(double);
				dtoaresult = cp =
				    dtoa(fparg.dbl, expchar ? 2 : 3, prec,
				    &expt, &signflag, &dtoaend);
				if (expt == 9999)
					expt = INT_MAX;
			}
fp_common:
			if (signflag)
				sign = '-';
			if (expt == INT_MAX) {	/* inf or nan */
				if (*cp == 'N') {
					cp = (ch >= 'a') ? "nan" : "NAN";
					sign = '\0';
				} else
					cp = (ch >= 'a') ? "inf" : "INF";
				size = 3;
				flags &= ~ZEROPAD;
				break;
			}
			flags |= FPT;
			ndig = dtoaend - cp;
			if (ch == 'g' || ch == 'G') {
				if (expt > -4 && expt <= prec) {
					/* Make %[gG] smell like %[fF] */
					expchar = '\0';
					if (flags & ALT)
						prec -= expt;
					else
						prec = ndig - expt;
					if (prec < 0)
						prec = 0;
				} else {
					/*
					 * Make %[gG] smell like %[eE], but
					 * trim trailing zeroes if no # flag.
					 */
					if (!(flags & ALT))
						prec = ndig;
				}
			}
			if (expchar) {
				expsize = exponent(expstr, expt - 1, expchar);
				size = expsize + prec;
				if (prec > 1 || flags & ALT)
					size += decpt_len;
			} else {
				/* space for digits before decimal point */
				if (expt > 0)
					size = expt;
				else	/* "0" */
					size = 1;
				/* space for decimal pt and following digits */
				if (prec || flags & ALT)
					size += prec + decpt_len;
				if ((flags & GROUPING) && expt > 0)
					size += grouping_init(&gs, expt, locale);
			}
			break;
#endif /* !NO_FLOATING_POINT */
		case 'm':
			error = __strerror_rl(serrno, errnomsg,
			    sizeof(errnomsg), locale);
			cp = error == 0 ? errnomsg : "<strerror failure>";
			size = (prec >= 0) ? strnlen(cp, prec) : strlen(cp);
			sign = '\0';
			break;
		case 'n':
			/*
			 * Assignment-like behavior is specified if the
			 * value overflows or is otherwise unrepresentable.
			 * C99 says to use `signed char' for %hhn conversions.
			 */
			if (flags & LLONGINT)
				*GETARG(long long *) = ret;
			else if (flags & SIZET)
				*GETARG(ssize_t *) = (ssize_t)ret;
			else if (flags & PTRDIFFT)
				*GETARG(ptrdiff_t *) = ret;
			else if (flags & INTMAXT)
				*GETARG(intmax_t *) = ret;
			else if (flags & LONGINT)
				*GETARG(long *) = ret;
			else if (flags & SHORTINT)
				*GETARG(short *) = ret;
			else if (flags & CHARINT)
				*GETARG(signed char *) = ret;
			else
				*GETARG(int *) = ret;
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			if (flags & INTMAX_SIZE)
				ujval = UJARG();
			else
				ulval = UARG();
			base = 8;
			goto nosign;
		case 'p':
			/*-
			 * ``The argument shall be a pointer to void.  The
			 * value of the pointer is converted to a sequence
			 * of printable characters, in an implementation-
			 * defined manner.''
			 *	-- ANSI X3J11
			 */
			ujval = (uintmax_t)(uintptr_t)GETARG(void *);
			base = 16;
			xdigs = xdigs_lower;
			flags = flags | INTMAXT;
			ox[1] = 'x';
			goto nosign;
		case 'S':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 's':
			if (flags & LONGINT) {
				wchar_t *wcp;

				if (convbuf != NULL)
					free(convbuf);
				if ((wcp = GETARG(wchar_t *)) == NULL)
					cp = "(null)";
				else {
					convbuf = __wcsconv(wcp, prec);
					if (convbuf == NULL) {
						fp->_flags |= __SERR;
						goto error;
					}
					cp = convbuf;
				}
			} else if ((cp = GETARG(char *)) == NULL)
				cp = "(null)";
			size = (prec >= 0) ? strnlen(cp, prec) : strlen(cp);
			sign = '\0';
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
			if (flags & INTMAX_SIZE)
				ujval = UJARG();
			else
				ulval = UARG();
			base = 10;
			goto nosign;
		case 'X':
			xdigs = xdigs_upper;
			goto hex;
		case 'x':
			xdigs = xdigs_lower;
hex:
			if (flags & INTMAX_SIZE)
				ujval = UJARG();
			else
				ulval = UARG();
			base = 16;
			/* leading 0x/X only if non-zero */
			if (flags & ALT &&
			    (flags & INTMAX_SIZE ? ujval != 0 : ulval != 0))
				ox[1] = ch;

			flags &= ~GROUPING;
			/* unsigned conversions */
nosign:			sign = '\0';
			/*-
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*-
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 *
			 * ``The C Standard is clear enough as is.  The call
			 * printf("%#.0o", 0) should print 0.''
			 *	-- Defect Report #151
			 */
			cp = buf + BUF;
			if (flags & INTMAX_SIZE) {
				if (ujval != 0 || prec != 0 ||
				    (flags & ALT && base == 8))
					cp = __ujtoa(ujval, cp, base,
					    flags & ALT, xdigs);
			} else {
				if (ulval != 0 || prec != 0 ||
				    (flags & ALT && base == 8))
					cp = __ultoa(ulval, cp, base,
					    flags & ALT, xdigs);
			}
			size = buf + BUF - cp;
			if (size > BUF)	/* should never happen */
				abort();
			if ((flags & GROUPING) && size != 0)
				size += grouping_init(&gs, size, locale);
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
invalid:
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = ch;
			size = 1;
			sign = '\0';
			break;
		}

		/*
		 * All reasonable formats wind up here.  At this point, `cp'
		 * points to a string which (if not flags&LADJUST) should be
		 * padded out to `width' places.  If flags&ZEROPAD, it should
		 * first be prefixed by any sign or other prefix; otherwise,
		 * it should be blank padded before the prefix is emitted.
		 * After any left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print the
		 * string proper, then emit zeroes required by any leftover
		 * floating precision; finally, if LADJUST, pad with blanks.
		 *
		 * Compute actual size, so we know how much to pad.
		 * size excludes decimal prec; realsz includes it.
		 */
		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;
		if (ox[1])
			realsz += 2;

		prsize = width > realsz ? width : realsz;
		if ((unsigned)ret + prsize > INT_MAX) {
			ret = EOF;
			errno = EOVERFLOW;
			goto error;
		}

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0)
			PAD(width - realsz, blanks);

		/* prefix */
		if (sign)
			PRINT(&sign, 1);

		if (ox[1]) {	/* ox[1] is either x, X, or \0 */
			ox[0] = '0';
			PRINT(ox, 2);
		}

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
			PAD(width - realsz, zeroes);

		/* the string or number proper */
#ifndef NO_FLOATING_POINT
		if ((flags & FPT) == 0) {
#endif
			/* leading zeroes from decimal precision */
			PAD(dprec - size, zeroes);
			if (gs.grouping) {
				if (grouping_print(&gs, &io, cp, buf+BUF, locale) < 0)
					goto error;
			} else {
				PRINT(cp, size);
			}
#ifndef NO_FLOATING_POINT
		} else {	/* glue together f_p fragments */
			if (!expchar) {	/* %[fF] or sufficiently short %[gG] */
				if (expt <= 0) {
					PRINT(zeroes, 1);
					if (prec || flags & ALT)
						PRINT(decimal_point,decpt_len);
					PAD(-expt, zeroes);
					/* already handled initial 0's */
					prec += expt;
				} else {
					if (gs.grouping) {
						n = grouping_print(&gs, &io,
						    cp, dtoaend, locale);
						if (n < 0)
							goto error;
						cp += n;
					} else {
						PRINTANDPAD(cp, dtoaend,
						    expt, zeroes);
						cp += expt;
					}
					if (prec || flags & ALT)
						PRINT(decimal_point,decpt_len);
				}
				PRINTANDPAD(cp, dtoaend, prec, zeroes);
			} else {	/* %[eE] or sufficiently long %[gG] */
				if (prec > 1 || flags & ALT) {
					PRINT(cp++, 1);
					PRINT(decimal_point, decpt_len);
					PRINT(cp, ndig-1);
					PAD(prec - ndig, zeroes);
				} else	/* XeYYY */
					PRINT(cp, 1);
				PRINT(expstr, expsize);
			}
		}
#endif
		/* left-adjusting padding (always blank) */
		if (flags & LADJUST)
			PAD(width - realsz, blanks);

		/* finally, adjust ret */
		ret += prsize;

		FLUSH();	/* copy out the I/O vectors */
	}
done:
	FLUSH();
error:
	va_end(orgap);
#ifndef NO_FLOATING_POINT
	if (dtoaresult != NULL)
		freedtoa(dtoaresult);
#endif
	if (convbuf != NULL)
		free(convbuf);
	if (__sferror(fp))
		ret = EOF;
	else
		fp->_flags |= savserr;
	if ((argtable != NULL) && (argtable != statargtable))
		free (argtable);
	return (ret);
	/* NOTREACHED */
}

