/*
 * Copyright (c) 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "%W% (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

/*
 * Actual printf innards.
 *
 * This code is large and complicated...
 */

#include <sys/types.h>
#include "ioprivate.h"
#include <string.h>
#include <stdarg.h>

/*
 * Define FLOATING_POINT to get floating point.
 */
#ifndef	NO_FLOATING_POINT
#define	FLOATING_POINT
#endif

/* end of configuration stuff */


/*
 * Helper class and function for `fprintf to unbuffered': creates a
 * temporary buffer.  We only work on write-only files; this avoids
 * worries about ungetc buffers and so forth.
 */

class help_streambuf : public backupbuf {
  public:
    char *buffer;
    int buf_size;
    streambuf *sb;
    help_streambuf(streambuf *sbuf, char *buf, int n) {
	sb = sbuf; buffer = buf; buf_size = n;
	setp(buffer, buffer+buf_size); }
    ~help_streambuf();
    virtual int overflow(int c = EOF);
};

int help_streambuf::overflow(int c)
{
    int used = pptr() - pbase();
    if (used) {
	sb->sputn(pbase(), used);
	pbump(-used);
    }
    if (c == EOF || buf_size == 0)
	return sb->overflow(c);
    return sputc(c);
}
help_streambuf::~help_streambuf()
{
    int used = pptr() - pbase();
    if (used) {
	sb->sputn(pbase(), used);
	pbump(-used);
    }
}

int help_vform(streambuf *sb, char const *fmt0, va_list ap)
{
    char buf[_G_BUFSIZ];
    help_streambuf helper(sb, buf, _G_BUFSIZ);
    return helper.vform(fmt0, ap);
}

#ifdef FLOATING_POINT

#include "floatio.h"
#define	BUF		(MAXEXP+MAXFRACT+1)	/* + decimal point */
#define	DEFPREC		6
extern "C" double modf(double, double*);

#else /* no FLOATING_POINT */

#define	BUF		40

#endif /* FLOATING_POINT */


/*
 * Macros for converting digits to letters and vice versa
 */
#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit(c) <= 9)
#define	to_char(n)	((n) + '0')

/*
 * Flags used during conversion.
 */
#define	LONGINT		0x01		/* long integer */
#define	LONGDBL		0x02		/* long double; unimplemented */
#define	SHORTINT	0x04		/* short integer */
#define	ALT		0x08		/* alternate form */
#define	LADJUST		0x10		/* left adjustment */
#define	ZEROPAD		0x20		/* zero (as opposed to blank) pad */
#define	HEXPREFIX	0x40		/* add 0x or 0X prefix */

int streambuf::vform(char const *fmt0, _G_va_list ap)
{
	register const char *fmt; /* format string */
	register int ch;	/* character from fmt */
	register int n;		/* handy integer (short term usage) */
	register char *cp;	/* handy char pointer (short term usage) */
	const char *fmark;	/* for remembering a place in fmt */
	register int flags;	/* flags as above */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format (%.3d), or -1 */
	char sign;		/* sign prefix (' ', '+', '-', or \0) */
#ifdef FLOATING_POINT
	int softsign;		/* temporary negative sign for floats */
	double _double;		/* double precision arguments %[eEfgG] */
#ifndef USE_DTOA
	int fpprec;		/* `extra' floating precision in [eEfgG] */
#endif
#endif
	unsigned long _ulong;	/* integer arguments %[diouxX] */
	enum { OCT, DEC, HEX } base;/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int dpad;		/* extra 0 padding needed for integers */
	int fieldsz;		/* field size expanded by sign, dpad etc */
	// The initialization of 'size' is to suppress a warning that
	// 'size' might be used unitialized.  It seems gcc can't
	// quite grok this spaghetti code ...
	int size = 0;		/* size of converted field or string */
	char buf[BUF];		/* space for %c, %[diouxX], %[eEfgG] */
	char ox[2];		/* space for 0x hex-prefix */

	/*
	 * Choose PADSIZE to trade efficiency vs size.  If larger
	 * printf fields occur frequently, increase PADSIZE (and make
	 * the initialisers below longer).
	 */
#define	PADSIZE	16		/* pad chunk size */
	static char const blanks[PADSIZE] =
	 {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
	static char const zeroes[PADSIZE] =
	 {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};

	/*
	 * BEWARE, these `goto error' on error, and PAD uses `n'.
	 */
#define	PRINT(ptr, len) \
  do { if (sputn(ptr, len) != len) goto error; } while (0)
#if 1
#define PAD_SP(howmany) if (padn(' ', howmany) < 0) goto error;
#define PAD_0(howmany) if (padn('0', howmany) < 0) goto error;
#else
#define	PAD(howmany, with) { \
	if ((n = (howmany)) > 0) { \
		while (n > PADSIZE) { \
			PRINT(with, PADSIZE); \
			n -= PADSIZE; \
		} \
		PRINT(with, n); \
	} \
}
#define PAD_SP(howmany) PAD(howmany, blanks)
#define PAD_0(howmany) PAD(howmany, zeroes)
#endif

	/*
	 * To extend shorts properly, we need both signed and unsigned
	 * argument extraction methods.
	 */
#define	SARG() \
	(flags&LONGINT ? va_arg(ap, long) : \
	    flags&SHORTINT ? (long)(short)va_arg(ap, int) : \
	    (long)va_arg(ap, int))
#define	UARG() \
	(flags&LONGINT ? va_arg(ap, unsigned long) : \
	    flags&SHORTINT ? (unsigned long)(unsigned short)va_arg(ap, int) : \
	    (unsigned long)va_arg(ap, unsigned int))

	/* optimise cerr (and other unbuffered Unix files) */
	if (unbuffered())
	    return help_vform(this, fmt0, ap);

	fmt = fmt0;
	ret = 0;

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		for (fmark = fmt; (ch = *fmt) != '\0' && ch != '%'; fmt++)
			/* void */;
		if ((n = fmt - fmark) != 0) {
			PRINT(fmark, n);
			ret += n;
		}
		if (ch == '\0')
			goto done;
		fmt++;		/* skip over '%' */

		flags = 0;
		dprec = 0;
#if defined(FLOATING_POINT) && !defined (USE_DTOA)
		fpprec = 0;
#endif
		width = 0;
		prec = -1;
		sign = '\0';

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
			/*
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
			/*
			 * ``A negative field width argument is taken as a
			 * - flag followed by a positive field width.''
			 *	-- ANSI X3J11
			 * They don't exclude field widths read from args.
			 */
			if ((width = va_arg(ap, int)) >= 0)
				goto rflag;
			width = -width;
			/* FALLTHROUGH */
		case '-':
			flags |= LADJUST;
			flags &= ~ZEROPAD; /* '-' disables '0' */
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				n = va_arg(ap, int);
				prec = n < 0 ? -1 : n;
				goto rflag;
			}
			n = 0;
			while (is_digit(ch)) {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			}
			prec = n < 0 ? -1 : n;
			goto reswitch;
		case '0':
			/*
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			if (!(flags & LADJUST))
			    flags |= ZEROPAD; /* '-' disables '0' */
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			width = n;
			goto reswitch;
#ifdef FLOATING_POINT
		case 'L':
			flags |= LONGDBL;
			goto rflag;
#endif
		case 'h':
			flags |= SHORTINT;
			goto rflag;
		case 'l':
			flags |= LONGINT;
			goto rflag;
		case 'c':
			*(cp = buf) = va_arg(ap, int);
			size = 1;
			sign = '\0';
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			_ulong = SARG();
			if ((long)_ulong < 0) {
				_ulong = -_ulong;
				sign = '-';
			}
			base = DEC;
			goto number;
#ifdef FLOATING_POINT
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
			_double = va_arg(ap, double);
#ifdef USE_DTOA
			{
			    ios::fmtflags fmt_flags = 0;
			    int fill = ' ';
			    if (flags & ALT)
				fmt_flags |= ios::showpoint;
			    if (flags & LADJUST)
				fmt_flags |= ios::left;
			    else if (flags & ZEROPAD)
				fmt_flags |= ios::internal, fill = '0';
			    n = __outfloat(_double, this, ch, width,
					   prec < 0 ? DEFPREC : prec,
					   fmt_flags, sign, fill);
			    if (n < 0)
				goto error;
			    ret += n;
			}
			// CHECK ERROR!
			continue;
#else
			/*
			 * don't do unrealistic precision; just pad it with
			 * zeroes later, so buffer size stays rational.
			 */
			if (prec > MAXFRACT) {
				if ((ch != 'g' && ch != 'G') || (flags&ALT))
					fpprec = prec - MAXFRACT;
				prec = MAXFRACT;
			} else if (prec == -1)
				prec = DEFPREC;
			// __cvt_double may have to round up before the
			// "start" of its buffer, i.e.
			// ``intf("%.2f", (double)9.999);'';
			// if the first character is still NUL, it did.
			// softsign avoids negative 0 if _double < 0 but
			// no significant digits will be shown.
			cp = buf;
			*cp = '\0';
			size = __cvt_double(_double, prec, flags, &softsign,
					    ch, cp, buf + sizeof(buf));
			if (softsign)
				sign = '-';
			if (*cp == '\0')
				cp++;
			break;
#endif
#endif /* FLOATING_POINT */
		case 'n':
			if (flags & LONGINT)
				*va_arg(ap, long *) = ret;
			else if (flags & SHORTINT)
				*va_arg(ap, short *) = ret;
			else
				*va_arg(ap, int *) = ret;
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			_ulong = UARG();
			base = OCT;
			goto nosign;
		case 'p':
			/*
			 * ``The argument shall be a pointer to void.  The
			 * value of the pointer is converted to a sequence
			 * of printable characters, in an implementation-
			 * defined manner.''
			 *	-- ANSI X3J11
			 */
			/* NOSTRICT */
			_ulong = (unsigned long)va_arg(ap, void *);
			base = HEX;
			flags |= HEXPREFIX;
			ch = 'x';
			goto nosign;
		case 's':
			if ((cp = va_arg(ap, char *)) == NULL)
				cp = "(null)";
			if (prec >= 0) {
				/*
				 * can't use strlen; can only look for the
				 * NUL in the first `prec' characters, and
				 * strlen() will go further.
				 */
				char *p = (char*)memchr(cp, 0, prec);

				if (p != NULL) {
					size = p - cp;
					if (size > prec)
						size = prec;
				} else
					size = prec;
			} else
				size = strlen(cp);
			sign = '\0';
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
			_ulong = UARG();
			base = DEC;
			goto nosign;
		case 'X':
		case 'x':
			_ulong = UARG();
			base = HEX;
			/* leading 0x/X only if non-zero */
			if (flags & ALT && _ulong != 0)
				flags |= HEXPREFIX;

			/* unsigned conversions */
nosign:			sign = '\0';
			/*
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 */
			cp = buf + BUF;
			if (_ulong != 0 || prec != 0) {
			        char *xdigs; /* digits for [xX] conversion */
				/*
				 * unsigned mod is hard, and unsigned mod
				 * by a constant is easier than that by
				 * a variable; hence this switch.
				 */
				switch (base) {
				case OCT:
					do {
						*--cp = to_char(_ulong & 7);
						_ulong >>= 3;
					} while (_ulong);
					/* handle octal leading 0 */
					if (flags & ALT && *cp != '0')
						*--cp = '0';
					break;

				case DEC:
					/* many numbers are 1 digit */
					while (_ulong >= 10) {
						*--cp = to_char(_ulong % 10);
						_ulong /= 10;
					}
					*--cp = to_char(_ulong);
					break;

				case HEX:
					if (ch == 'X')
					    xdigs = "0123456789ABCDEF";
					else /* ch == 'x' || ch == 'p' */
					    xdigs = "0123456789abcdef";
					do {
						*--cp = xdigs[_ulong & 15];
						_ulong >>= 4;
					} while (_ulong);
					break;

				default:
					cp = "bug in vform: bad base";
					goto skipsize;
				}
			}
			size = buf + BUF - cp;
		skipsize:
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = ch;
			size = 1;
			sign = '\0';
			break;
		}

		/*
		 * All reasonable formats wind up here.  At this point,
		 * `cp' points to a string which (if not flags&LADJUST)
		 * should be padded out to `width' places.  If
		 * flags&ZEROPAD, it should first be prefixed by any
		 * sign or other prefix; otherwise, it should be blank
		 * padded before the prefix is emitted.  After any
		 * left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print
		 * the string proper, then emit zeroes required by any
		 * leftover floating precision; finally, if LADJUST,
		 * pad with blanks.
		 */

		/*
		 * compute actual size, so we know how much to pad.
		 */
#if defined(FLOATING_POINT) && !defined (USE_DTOA)
		fieldsz = size + fpprec;
#else
		fieldsz = size;
#endif
		dpad = dprec - size;
		if (dpad < 0)
		    dpad = 0;

		if (sign)
			fieldsz++;
		else if (flags & HEXPREFIX)
			fieldsz += 2;
		fieldsz += dpad;

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0)
			PAD_SP(width - fieldsz);

		/* prefix */
		if (sign) {
			PRINT(&sign, 1);
		} else if (flags & HEXPREFIX) {
			ox[0] = '0';
			ox[1] = ch;
			PRINT(ox, 2);
		}

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
			PAD_0(width - fieldsz);

		/* leading zeroes from decimal precision */
		PAD_0(dpad);

		/* the string or number proper */
		PRINT(cp, size);

#if defined(FLOATING_POINT) && !defined (USE_DTOA)
		/* trailing f.p. zeroes */
		PAD_0(fpprec);
#endif

		/* left-adjusting padding (always blank) */
		if (flags & LADJUST)
			PAD_SP(width - fieldsz);

		/* finally, adjust ret */
		ret += width > fieldsz ? width : fieldsz;

	}
done:
	return ret;
error:
	return EOF;
	/* NOTREACHED */
}

#if defined(FLOATING_POINT) && !defined(USE_DTOA)

static char *exponent(register char *p, register int exp, int fmtch)
{
	register char *t;
	char expbuf[MAXEXP];

	*p++ = fmtch;
	if (exp < 0) {
		exp = -exp;
		*p++ = '-';
	}
	else
		*p++ = '+';
	t = expbuf + MAXEXP;
	if (exp > 9) {
		do {
			*--t = to_char(exp % 10);
		} while ((exp /= 10) > 9);
		*--t = to_char(exp);
		for (; t < expbuf + MAXEXP; *p++ = *t++);
	}
	else {
		*p++ = '0';
		*p++ = to_char(exp);
	}
	return (p);
}

static char * round(double fract, int *exp,
		    register char *start, register char *end,
		    char ch, int *signp)
{
	double tmp;

	if (fract)
	(void)modf(fract * 10, &tmp);
	else
		tmp = to_digit(ch);
	if (tmp > 4)
		for (;; --end) {
			if (*end == '.')
				--end;
			if (++*end <= '9')
				break;
			*end = '0';
			if (end == start) {
				if (exp) {	/* e/E; increment exponent */
					*end = '1';
					++*exp;
				}
				else {		/* f; add extra digit */
				*--end = '1';
				--start;
				}
				break;
			}
		}
	/* ``"%.3f", (double)-0.0004'' gives you a negative 0. */
	else if (*signp == '-')
		for (;; --end) {
			if (*end == '.')
				--end;
			if (*end != '0')
				break;
			if (end == start)
				*signp = 0;
		}
	return (start);
}

int __cvt_double(double number, register int prec, int flags, int *signp,
		 int fmtch, char *startp, char *endp)
{
	register char *p, *t;
	register double fract;
	int dotrim = 0, expcnt, gformat = 0;
	double integer, tmp;

	expcnt = 0;
	if (number < 0) {
		number = -number;
		*signp = '-';
	} else
		*signp = 0;

	fract = modf(number, &integer);

	/* get an extra slot for rounding. */
	t = ++startp;

	/*
	 * get integer portion of number; put into the end of the buffer; the
	 * .01 is added for modf(356.0 / 10, &integer) returning .59999999...
	 */
	for (p = endp - 1; integer; ++expcnt) {
		tmp = modf(integer / 10, &integer);
		*p-- = to_char((int)((tmp + .01) * 10));
	}
	switch (fmtch) {
	case 'f':
	case 'F':
		/* reverse integer into beginning of buffer */
		if (expcnt)
			for (; ++p < endp; *t++ = *p);
		else
			*t++ = '0';
		/*
		 * if precision required or alternate flag set, add in a
		 * decimal point.
		 */
		if (prec || flags&ALT)
			*t++ = '.';
		/* if requires more precision and some fraction left */
		if (fract) {
			if (prec)
				do {
					fract = modf(fract * 10, &tmp);
					*t++ = to_char((int)tmp);
				} while (--prec && fract);
			if (fract)
				startp = round(fract, (int *)NULL, startp,
				    t - 1, (char)0, signp);
		}
		for (; prec--; *t++ = '0');
		break;
	case 'e':
	case 'E':
eformat:	if (expcnt) {
			*t++ = *++p;
			if (prec || flags&ALT)
				*t++ = '.';
			/* if requires more precision and some integer left */
			for (; prec && ++p < endp; --prec)
				*t++ = *p;
			/*
			 * if done precision and more of the integer component,
			 * round using it; adjust fract so we don't re-round
			 * later.
			 */
			if (!prec && ++p < endp) {
				fract = 0;
				startp = round((double)0, &expcnt, startp,
				    t - 1, *p, signp);
			}
			/* adjust expcnt for digit in front of decimal */
			--expcnt;
		}
		/* until first fractional digit, decrement exponent */
		else if (fract) {
			/* adjust expcnt for digit in front of decimal */
			for (expcnt = -1;; --expcnt) {
				fract = modf(fract * 10, &tmp);
				if (tmp)
					break;
			}
			*t++ = to_char((int)tmp);
			if (prec || flags&ALT)
				*t++ = '.';
		}
		else {
			*t++ = '0';
			if (prec || flags&ALT)
				*t++ = '.';
		}
		/* if requires more precision and some fraction left */
		if (fract) {
			if (prec)
				do {
					fract = modf(fract * 10, &tmp);
					*t++ = to_char((int)tmp);
				} while (--prec && fract);
			if (fract)
				startp = round(fract, &expcnt, startp,
				    t - 1, (char)0, signp);
		}
		/* if requires more precision */
		for (; prec--; *t++ = '0');

		/* unless alternate flag, trim any g/G format trailing 0's */
		if (gformat && !(flags&ALT)) {
			while (t > startp && *--t == '0');
			if (*t == '.')
				--t;
			++t;
		}
		t = exponent(t, expcnt, fmtch);
		break;
	case 'g':
	case 'G':
		/* a precision of 0 is treated as a precision of 1. */
		if (!prec)
			++prec;
		/*
		 * ``The style used depends on the value converted; style e
		 * will be used only if the exponent resulting from the
		 * conversion is less than -4 or greater than the precision.''
		 *	-- ANSI X3J11
		 */
		if (expcnt > prec || (!expcnt && fract && fract < .0001)) {
			/*
			 * g/G format counts "significant digits, not digits of
			 * precision; for the e/E format, this just causes an
			 * off-by-one problem, i.e. g/G considers the digit
			 * before the decimal point significant and e/E doesn't
			 * count it as precision.
			 */
			--prec;
			fmtch -= 2;		/* G->E, g->e */
			gformat = 1;
			goto eformat;
		}
		/*
		 * reverse integer into beginning of buffer,
		 * note, decrement precision
		 */
		if (expcnt)
			for (; ++p < endp; *t++ = *p, --prec);
		else
			*t++ = '0';
		/*
		 * if precision required or alternate flag set, add in a
		 * decimal point.  If no digits yet, add in leading 0.
		 */
		if (prec || flags&ALT) {
			dotrim = 1;
			*t++ = '.';
		}
		else
			dotrim = 0;
		/* if requires more precision and some fraction left */
		if (fract) {
			if (prec) {
				/* If no integer part, don't count initial
				 * zeros as significant digits. */
				do {
					fract = modf(fract * 10, &tmp);
					*t++ = to_char((int)tmp);
				} while(!tmp && !expcnt);
				while (--prec && fract) {
					fract = modf(fract * 10, &tmp);
					*t++ = to_char((int)tmp);
				}
			}
			if (fract)
				startp = round(fract, (int *)NULL, startp,
				    t - 1, (char)0, signp);
		}
		/* alternate format, adds 0's for precision, else trim 0's */
		if (flags&ALT)
			for (; prec--; *t++ = '0');
		else if (dotrim) {
			while (t > startp && *--t == '0');
			if (*t != '.')
				++t;
		}
	}
	return (t - startp);
}

#endif /* defined(FLOATING_POINT) && !defined(USE_DTOA) */

int streambuf::form(char const *format ...)
{
    va_list ap;
    va_start(ap, format);
    int count = vform(format, ap);
    va_end(ap);
    return count;
}
