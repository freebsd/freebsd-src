/*	$NetBSD: seq.c,v 1.5 2008/07/21 14:19:26 lukem Exp $	*/
/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brian Ginsbach.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ZERO	'0'
#define SPACE	' '

#define MAX(a, b)	(((a) < (b))? (b) : (a))
#define ISSIGN(c)	((int)(c) == '-' || (int)(c) == '+')
#define ISEXP(c)	((int)(c) == 'e' || (int)(c) == 'E')
#define ISODIGIT(c)	((int)(c) >= '0' && (int)(c) <= '7')

/* Globals */

static const char *decimal_point = ".";	/* default */
static char default_format[] = { "%g" };	/* default */

/* Prototypes */

static double e_atof(const char *);

static int decimal_places(const char *);
static int numeric(const char *);
static int valid_format(const char *);

static char *generate_format(double, double, double, int, char);
static char *unescape(char *);

/*
 * The seq command will print out a numeric sequence from 1, the default,
 * to a user specified upper limit by 1.  The lower bound and increment
 * maybe indicated by the user on the command line.  The sequence can
 * be either whole, the default, or decimal numbers.
 */
int
main(int argc, char *argv[])
{
	int c = 0, errflg = 0;
	int equalize = 0;
	double first = 1.0;
	double last = 0.0;
	double incr = 0.0;
	struct lconv *locale;
	char *fmt = NULL;
	const char *sep = "\n";
	const char *term = NULL;
	char pad = ZERO;

	/* Determine the locale's decimal point. */
	locale = localeconv();
	if (locale && locale->decimal_point && locale->decimal_point[0] != '\0')
		decimal_point = locale->decimal_point;

	/*
         * Process options, but handle negative numbers separately
         * least they trip up getopt(3).
         */
	while ((optind < argc) && !numeric(argv[optind]) &&
	    (c = getopt(argc, argv, "f:hs:t:w")) != -1) {

		switch (c) {
		case 'f':	/* format (plan9) */
			fmt = optarg;
			equalize = 0;
			break;
		case 's':	/* separator (GNU) */
			sep = unescape(optarg);
			break;
		case 't':	/* terminator (new) */
			term = unescape(optarg);
			break;
		case 'w':	/* equal width (plan9) */
			if (!fmt)
				if (equalize++)
					pad = SPACE;
			break;
		case 'h':	/* help (GNU) */
		default:
			errflg++;
			break;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1 || argc > 3)
		errflg++;

	if (errflg) {
		fprintf(stderr,
		    "usage: %s [-w] [-f format] [-s string] [-t string] [first [incr]] last\n",
		    getprogname());
		exit(1);
	}

	last = e_atof(argv[argc - 1]);

	if (argc > 1)
		first = e_atof(argv[0]);
	
	if (argc > 2) {
		incr = e_atof(argv[1]);
		/* Plan 9/GNU don't do zero */
		if (incr == 0.0)
			errx(1, "zero %screment", (first < last)? "in" : "de");
	}

	/* default is one for Plan 9/GNU work alike */
	if (incr == 0.0)
		incr = (first < last) ? 1.0 : -1.0;

	if (incr <= 0.0 && first < last)
		errx(1, "needs positive increment");

	if (incr >= 0.0 && first > last)
		errx(1, "needs negative decrement");

	if (fmt != NULL) {
		if (!valid_format(fmt))
			errx(1, "invalid format string: `%s'", fmt);
		fmt = unescape(fmt);
		/*
	         * XXX to be bug for bug compatible with Plan 9 add a
		 * newline if none found at the end of the format string.
		 */
	} else
		fmt = generate_format(first, incr, last, equalize, pad);

	if (incr > 0) {
		for (; first <= last; first += incr) {
			printf(fmt, first);
			fputs(sep, stdout);
		}
	} else {
		for (; first >= last; first += incr) {
			printf(fmt, first);
			fputs(sep, stdout);
		}
	}
	if (term != NULL)
		fputs(term, stdout);

	return (0);
}

/*
 * numeric - verify that string is numeric
 */
static int
numeric(const char *s)
{
	int seen_decimal_pt, decimal_pt_len;

	/* skip any sign */
	if (ISSIGN((unsigned char)*s))
		s++;

	seen_decimal_pt = 0;
	decimal_pt_len = strlen(decimal_point);
	while (*s) {
		if (!isdigit((unsigned char)*s)) {
			if (!seen_decimal_pt &&
			    strncmp(s, decimal_point, decimal_pt_len) == 0) {
				s += decimal_pt_len;
				seen_decimal_pt = 1;
				continue;
			}
			if (ISEXP((unsigned char)*s)) {
				s++;
				if (ISSIGN((unsigned char)*s) ||
				    isdigit((unsigned char)*s)) {
					s++;
					continue;
				}
			}
			break;
		}
		s++;
	}
	return (*s == '\0');
}

/*
 * valid_format - validate user specified format string
 */
static int
valid_format(const char *fmt)
{
	int conversions = 0;

	while (*fmt != '\0') {
		/* scan for conversions */
		if (*fmt != '\0' && *fmt != '%') {
			do {
				fmt++;
			} while (*fmt != '\0' && *fmt != '%');
		}
		/* scan a conversion */
		if (*fmt != '\0') {
			do {
				fmt++;

				/* ok %% */
				if (*fmt == '%') {
					fmt++;
					break;
				}
				/* valid conversions */
				if (strchr("eEfgG", *fmt) &&
				    conversions++ < 1) {
					fmt++;
					break;
				}
				/* flags, width and precision */
				if (isdigit((unsigned char)*fmt) ||
				    strchr("+- 0#.", *fmt))
					continue;

				/* oops! bad conversion format! */
				return (0);
			} while (*fmt != '\0');
		}
	}

	return (conversions <= 1);
}

/*
 * unescape - handle C escapes in a string
 */
static char *
unescape(char *orig)
{
	char c, *cp, *new = orig;
	int i;

	for (cp = orig; (*orig = *cp); cp++, orig++) {
		if (*cp != '\\')
			continue;

		switch (*++cp) {
		case 'a':	/* alert (bell) */
			*orig = '\a';
			continue;
		case 'b':	/* backspace */
			*orig = '\b';
			continue;
		case 'e':	/* escape */
			*orig = '\e';
			continue;
		case 'f':	/* formfeed */
			*orig = '\f';
			continue;
		case 'n':	/* newline */
			*orig = '\n';
			continue;
		case 'r':	/* carriage return */
			*orig = '\r';
			continue;
		case 't':	/* horizontal tab */
			*orig = '\t';
			continue;
		case 'v':	/* vertical tab */
			*orig = '\v';
			continue;
		case '\\':	/* backslash */
			*orig = '\\';
			continue;
		case '\'':	/* single quote */
			*orig = '\'';
			continue;
		case '\"':	/* double quote */
			*orig = '"';
			continue;
		case '0':
		case '1':
		case '2':
		case '3':	/* octal */
		case '4':
		case '5':
		case '6':
		case '7':	/* number */
			for (i = 0, c = 0;
			     ISODIGIT((unsigned char)*cp) && i < 3;
			     i++, cp++) {
				c <<= 3;
				c |= (*cp - '0');
			}
			*orig = c;
			--cp;
			continue;
		case 'x':	/* hexadecimal number */
			cp++;	/* skip 'x' */
			for (i = 0, c = 0;
			     isxdigit((unsigned char)*cp) && i < 2;
			     i++, cp++) {
				c <<= 4;
				if (isdigit((unsigned char)*cp))
					c |= (*cp - '0');
				else
					c |= ((toupper((unsigned char)*cp) -
					    'A') + 10);
			}
			*orig = c;
			--cp;
			continue;
		default:
			--cp;
			break;
		}
	}

	return (new);
}

/*
 * e_atof - convert an ASCII string to a double
 *	exit if string is not a valid double, or if converted value would
 *	cause overflow or underflow
 */
static double
e_atof(const char *num)
{
	char *endp;
	double dbl;

	errno = 0;
	dbl = strtod(num, &endp);

	if (errno == ERANGE)
		/* under or overflow */
		err(2, "%s", num);
	else if (*endp != '\0')
		/* "junk" left in number */
		errx(2, "invalid floating point argument: %s", num);

	/* zero shall have no sign */
	if (dbl == -0.0)
		dbl = 0;
	return (dbl);
}

/*
 * decimal_places - count decimal places in a number (string)
 */
static int
decimal_places(const char *number)
{
	int places = 0;
	char *dp;

	/* look for a decimal point */
	if ((dp = strstr(number, decimal_point))) {
		dp += strlen(decimal_point);

		while (isdigit((unsigned char)*dp++))
			places++;
	}
	return (places);
}

/*
 * generate_format - create a format string
 *
 * XXX to be bug for bug compatible with Plan9 and GNU return "%g"
 * when "%g" prints as "%e" (this way no width adjustments are made)
 */
static char *
generate_format(double first, double incr, double last, int equalize, char pad)
{
	static char buf[256];
	char cc = '\0';
	int precision, width1, width2, places;

	if (equalize == 0)
		return (default_format);

	/* figure out "last" value printed */
	if (first > last)
		last = first - incr * floor((first - last) / incr);
	else
		last = first + incr * floor((last - first) / incr);

	sprintf(buf, "%g", incr);
	if (strchr(buf, 'e'))
		cc = 'e';
	precision = decimal_places(buf);

	width1 = sprintf(buf, "%g", first);
	if (strchr(buf, 'e'))
		cc = 'e';
	if ((places = decimal_places(buf)))
		width1 -= (places + strlen(decimal_point));

	precision = MAX(places, precision);

	width2 = sprintf(buf, "%g", last);
	if (strchr(buf, 'e'))
		cc = 'e';
	if ((places = decimal_places(buf)))
		width2 -= (places + strlen(decimal_point));

	if (precision) {
		sprintf(buf, "%%%c%d.%d%c", pad,
		    MAX(width1, width2) + (int) strlen(decimal_point) +
		    precision, precision, (cc) ? cc : 'f');
	} else {
		sprintf(buf, "%%%c%d%c", pad, MAX(width1, width2),
		    (cc) ? cc : 'g');
	}

	return (buf);
}
