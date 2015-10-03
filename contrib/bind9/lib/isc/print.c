/*
 * Copyright (C) 2004-2008, 2010, 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <stdio.h>		/* for sprintf() */
#include <string.h>		/* for strlen() */
#include <assert.h>		/* for assert() */

#define	ISC__PRINT_SOURCE	/* Used to get the isc_print_* prototypes. */

#include <isc/assertions.h>
#include <isc/int.h>
#include <isc/msgs.h>
#include <isc/print.h>
#include <isc/stdlib.h>
#include <isc/util.h>

/*
 * We use the system's sprintf so we undef it here.
 */
#undef sprintf

static int
isc__print_printf(void (*emit)(char, void *), void *arg,
		  const char *format, va_list ap);

static void
file_emit(char c, void *arg) {
	FILE *fp = arg;
	int i = c & 0xff;

	putc(i, fp);
}

#if 0
static int
isc_print_vfprintf(FILE *fp, const char *format, va_list ap) {
	assert(fp != NULL);
	assert(format != NULL);

	return (isc__print_printf(file_emit, fp, format, ap));
}
#endif

int
isc_print_printf(const char *format, ...) {
	va_list ap;
	int n;

	assert(format != NULL);

	va_start(ap, format);
	n = isc__print_printf(file_emit, stdout, format, ap);
	va_end(ap);
	return (n);
}

int
isc_print_fprintf(FILE *fp, const char *format, ...) {
	va_list ap;
	int n;

	assert(fp != NULL);
	assert(format != NULL);

	va_start(ap, format);
	n = isc__print_printf(file_emit, fp, format, ap);
	va_end(ap);
	return (n);
}

static void
nocheck_emit(char c, void *arg) {
	struct { char *str; } *a = arg;

	*(a->str)++ = c;
}

int
isc_print_sprintf(char *str, const char *format, ...) {
	struct { char *str; } arg;
	int n;
	va_list ap;

	arg.str = str;

	va_start(ap, format);
	n = isc__print_printf(nocheck_emit, &arg, format, ap);
	va_end(ap);
	return (n);
}

/*!
 * Return length of string that would have been written if not truncated.
 */

int
isc_print_snprintf(char *str, size_t size, const char *format, ...) {
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = isc_print_vsnprintf(str, size, format, ap);
	va_end(ap);
	return (ret);

}

/*!
 * Return length of string that would have been written if not truncated.
 */

static void
string_emit(char c, void *arg) {
	struct { char *str; size_t size; } *p = arg;

	if (p->size > 0U) {
		*(p->str)++ = c;
		p->size--;
	}
}

int
isc_print_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	struct { char *str; size_t size; } arg;
	int n;

	assert(str != NULL);
	assert(format != NULL);

	arg.str = str;
	arg.size = size;

	n = isc__print_printf(string_emit, &arg, format, ap);
	if (arg.size > 0U)
		*arg.str = '\0';
	return (n);
}

static int
isc__print_printf(void (*emit)(char, void *), void *arg,
		  const char *format, va_list ap)
{
	int h;
	int l;
	int z;
	int q;
	int alt;
	int zero;
	int left;
	int plus;
	int space;
	int neg;
	isc_int64_t tmpi;
	isc_uint64_t tmpui;
	unsigned long width;
	unsigned long precision;
	unsigned int length;
	char buf[1024];
	char c;
	void *v;
	const char *cp;
	const char *head;
	int count = 0;
	int pad;
	int zeropad;
	int dot;
	double dbl;
	isc_boolean_t precision_set;
#ifdef HAVE_LONG_DOUBLE
	long double ldbl;
#endif
	char fmt[32];

	assert(emit != NULL);
	assert(arg != NULL);
	assert(format != NULL);

	while (*format != '\0') {
		if (*format != '%') {
			emit(*format++, arg);
			count++;
			continue;
		}
		format++;

		/*
		 * Reset flags.
		 */
		dot = neg = space = plus = left = zero = alt = h = l = q = z = 0;
		width = precision = 0;
		head = "";
		pad = zeropad = 0;
		precision_set = ISC_FALSE;

		do {
			if (*format == '#') {
				alt = 1;
				format++;
			} else if (*format == '-') {
				left = 1;
				zero = 0;
				format++;
			} else if (*format == ' ') {
				if (!plus)
					space = 1;
				format++;
			} else if (*format == '+') {
				plus = 1;
				space = 0;
				format++;
			} else if (*format == '0') {
				if (!left)
					zero = 1;
				format++;
			} else
				break;
		} while (1);

		/*
		 * Width.
		 */
		if (*format == '*') {
			width = va_arg(ap, int);
			format++;
		} else if (isdigit((unsigned char)*format)) {
			char *e;
			width = strtoul(format, &e, 10);
			format = e;
		}

		/*
		 * Precision.
		 */
		if (*format == '.') {
			format++;
			dot = 1;
			if (*format == '*') {
				precision = va_arg(ap, int);
				precision_set = ISC_TRUE;
				format++;
			} else if (isdigit((unsigned char)*format)) {
				char *e;
				precision = strtoul(format, &e, 10);
				precision_set = ISC_TRUE;
				format = e;
			}
		}

		switch (*format) {
		case '\0':
			continue;
		case '%':
			emit(*format, arg);
			count++;
			break;
		case 'q':
			q = 1;
			format++;
			goto doint;
		case 'h':
			h = 1;
			format++;
			goto doint;
		case 'l':
			l = 1;
			format++;
			if (*format == 'l') {
				q = 1;
				format++;
			}
			goto doint;
		case 'z':
			z = 1;
			format++;
			goto doint;
		case 'n':
		case 'i':
		case 'd':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
		doint:
			if (precision != 0U)
				zero = 0;
			switch (*format) {
			case 'n':
				if (h) {
					short int *p;
					p = va_arg(ap, short *);
					assert(p != NULL);
					*p = count;
				} else if (l) {
					long int *p;
					p = va_arg(ap, long *);
					assert(p != NULL);
					*p = count;
				} else if (z) {
					size_t *p;
					p = va_arg(ap, size_t *);
					assert(p != NULL);
					*p = count;
				} else {
					int *p;
					p = va_arg(ap, int *);
					assert(p != NULL);
					*p = count;
				}
				break;
			case 'i':
			case 'd':
				if (q)
					tmpi = va_arg(ap, isc_int64_t);
				else if (l)
					tmpi = va_arg(ap, long int);
				else if (z)
					tmpi = va_arg(ap, ssize_t);
				else
					tmpi = va_arg(ap, int);
				if (tmpi < 0) {
					head = "-";
					tmpui = -tmpi;
				} else {
					if (plus)
						head = "+";
					else if (space)
						head = " ";
					else
						head = "";
					tmpui = tmpi;
				}
				if (tmpui <= 0xffffffffU)
					sprintf(buf, "%lu",
						(unsigned long)tmpui);
				else {
					unsigned long mid;
					unsigned long lo;
					unsigned long hi;
					lo = tmpui % 1000000000;
					tmpui /= 1000000000;
					mid = tmpui % 1000000000;
					hi = tmpui / 1000000000;
					if (hi != 0U) {
						sprintf(buf, "%lu", hi);
						sprintf(buf + strlen(buf),
							"%09lu", mid);
					} else
						sprintf(buf, "%lu", mid);
					sprintf(buf + strlen(buf), "%09lu",
						lo);
				}
				goto printint;
			case 'o':
				if (q)
					tmpui = va_arg(ap, isc_uint64_t);
				else if (l)
					tmpui = va_arg(ap, long int);
				else if (z)
					tmpui = va_arg(ap, size_t);
				else
					tmpui = va_arg(ap, int);
				if (tmpui <= 0xffffffffU)
					sprintf(buf, alt ?  "%#lo" : "%lo",
						(unsigned long)tmpui);
				else {
					unsigned long mid;
					unsigned long lo;
					unsigned long hi;
					lo = tmpui % 010000000000;
					tmpui /= 010000000000;
					mid = tmpui % 010000000000;
					hi = tmpui / 010000000000;
					if (hi != 0U) {
						sprintf(buf,
							alt ?  "%#lo" : "%lo",
							hi);
						sprintf(buf + strlen(buf),
							"%09lo", mid);
					} else
						sprintf(buf,
							alt ?  "%#lo" : "%lo",
							mid);
					sprintf(buf + strlen(buf), "%09lo", lo);
				}
				goto printint;
			case 'u':
				if (q)
					tmpui = va_arg(ap, isc_uint64_t);
				else if (l)
					tmpui = va_arg(ap, unsigned long int);
				else if (z)
					tmpui = va_arg(ap, size_t);
				else
					tmpui = va_arg(ap, unsigned int);
				if (tmpui <= 0xffffffffU)
					sprintf(buf, "%lu",
						(unsigned long)tmpui);
				else {
					unsigned long mid;
					unsigned long lo;
					unsigned long hi;
					lo = tmpui % 1000000000;
					tmpui /= 1000000000;
					mid = tmpui % 1000000000;
					hi = tmpui / 1000000000;
					if (hi != 0U) {
						sprintf(buf, "%lu", hi);
						sprintf(buf + strlen(buf),
							"%09lu", mid);
					 } else
						sprintf(buf, "%lu", mid);
					sprintf(buf + strlen(buf), "%09lu",
						lo);
				}
				goto printint;
			case 'x':
				if (q)
					tmpui = va_arg(ap, isc_uint64_t);
				else if (l)
					tmpui = va_arg(ap, unsigned long int);
				else if (z)
					tmpui = va_arg(ap, size_t);
				else
					tmpui = va_arg(ap, unsigned int);
				if (alt) {
					head = "0x";
					if (precision > 2U)
						precision -= 2;
				}
				if (tmpui <= 0xffffffffU)
					sprintf(buf, "%lx",
						(unsigned long)tmpui);
				else {
					unsigned long hi = tmpui>>32;
					unsigned long lo = tmpui & 0xffffffff;
					sprintf(buf, "%lx", hi);
					sprintf(buf + strlen(buf), "%08lx", lo);
				}
				goto printint;
			case 'X':
				if (q)
					tmpui = va_arg(ap, isc_uint64_t);
				else if (l)
					tmpui = va_arg(ap, unsigned long int);
				else if (z)
					tmpui = va_arg(ap, size_t);
				else
					tmpui = va_arg(ap, unsigned int);
				if (alt) {
					head = "0X";
					if (precision > 2U)
						precision -= 2;
				}
				if (tmpui <= 0xffffffffU)
					sprintf(buf, "%lX",
						(unsigned long)tmpui);
				else  {
					unsigned long hi = tmpui>>32;
					unsigned long lo = tmpui & 0xffffffff;
					sprintf(buf, "%lX", hi);
					sprintf(buf + strlen(buf), "%08lX", lo);
				}
				goto printint;
			printint:
				if (precision_set || width != 0U) {
					length = strlen(buf);
					if (length < precision)
						zeropad = precision - length;
					else if (length < width && zero)
						zeropad = width - length;
					if (width != 0U) {
						pad = width - length -
						      zeropad - strlen(head);
						if (pad < 0)
							pad = 0;
					}
				}
				count += strlen(head) + strlen(buf) + pad +
					 zeropad;
				if (!left) {
					while (pad > 0) {
						emit(' ', arg);
						pad--;
					}
				}
				cp = head;
				while (*cp != '\0')
					emit(*cp++, arg);
				while (zeropad > 0) {
					emit('0', arg);
					zeropad--;
				}
				cp = buf;
				while (*cp != '\0')
					emit(*cp++, arg);
				while (pad > 0) {
					emit(' ', arg);
					pad--;
				}
				break;
			default:
				break;
			}
			break;
		case 's':
			cp = va_arg(ap, char *);

			if (precision_set) {
				/*
				 * cp need not be NULL terminated.
				 */
				const char *tp;
				unsigned long n;

				if (precision != 0U)
					assert(cp != NULL);
				n = precision;
				tp = cp;
				while (n != 0U && *tp != '\0')
					n--, tp++;
				length = precision - n;
			} else {
				assert(cp != NULL);
				length = strlen(cp);
			}
			if (width != 0U) {
				pad = width - length;
				if (pad < 0)
					pad = 0;
			}
			count += pad + length;
			if (!left)
				while (pad > 0) {
					emit(' ', arg);
					pad--;
				}
			if (precision_set)
				while (precision > 0U && *cp != '\0') {
					emit(*cp++, arg);
					precision--;
				}
			else
				while (*cp != '\0')
					emit(*cp++, arg);
			while (pad > 0) {
				emit(' ', arg);
				pad--;
			}
			break;
		case 'c':
			c = va_arg(ap, int);
			if (width > 0U) {
				count += width;
				width--;
				if (left)
					emit(c, arg);
				while (width-- > 0U)
					emit(' ', arg);
				if (!left)
					emit(c, arg);
			} else {
				count++;
				emit(c, arg);
			}
			break;
		case 'p':
			v = va_arg(ap, void *);
			sprintf(buf, "%p", v);
			length = strlen(buf);
			if (precision > length)
				zeropad = precision - length;
			if (width > 0U) {
				pad = width - length - zeropad;
				if (pad < 0)
					pad = 0;
			}
			count += length + pad + zeropad;
			if (!left)
				while (pad > 0) {
					emit(' ', arg);
					pad--;
				}
			cp = buf;
			if (zeropad > 0 && buf[0] == '0' &&
			    (buf[1] == 'x' || buf[1] == 'X')) {
				emit(*cp++, arg);
				emit(*cp++, arg);
				while (zeropad > 0) {
					emit('0', arg);
					zeropad--;
				}
			}
			while (*cp != '\0')
				emit(*cp++, arg);
			while (pad > 0) {
				emit(' ', arg);
				pad--;
			}
			break;
		case 'D':	/*deprecated*/
			assert("use %ld instead of %D" == NULL);
		case 'O':	/*deprecated*/
			assert("use %lo instead of %O" == NULL);
		case 'U':	/*deprecated*/
			assert("use %lu instead of %U" == NULL);

		case 'L':
#ifdef HAVE_LONG_DOUBLE
			l = 1;
#else
			assert("long doubles are not supported" == NULL);
#endif
			/*FALLTHROUGH*/
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
			if (!dot)
				precision = 6;
			/*
			 * IEEE floating point.
			 * MIN 2.2250738585072014E-308
			 * MAX 1.7976931348623157E+308
			 * VAX floating point has a smaller range than IEEE.
			 *
			 * precisions > 324 don't make much sense.
			 * if we cap the precision at 512 we will not
			 * overflow buf.
			 */
			if (precision > 512U)
				precision = 512;
			sprintf(fmt, "%%%s%s.%lu%s%c", alt ? "#" : "",
				plus ? "+" : space ? " " : "",
				precision, l ? "L" : "", *format);
			switch (*format) {
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':
#ifdef HAVE_LONG_DOUBLE
				if (l) {
					ldbl = va_arg(ap, long double);
					sprintf(buf, fmt, ldbl);
				} else
#endif
				{
					dbl = va_arg(ap, double);
					sprintf(buf, fmt, dbl);
				}
				length = strlen(buf);
				if (width > 0U) {
					pad = width - length;
					if (pad < 0)
						pad = 0;
				}
				count += length + pad;
				if (!left)
					while (pad > 0) {
						emit(' ', arg);
						pad--;
					}
				cp = buf;
				while (*cp != ' ')
					emit(*cp++, arg);
				while (pad > 0) {
					emit(' ', arg);
					pad--;
				}
				break;
			default:
				continue;
			}
			break;
		default:
			continue;
		}
		format++;
	}
	return (count);
}
