/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Landon Curt Noll.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)factor.c	4.4 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * factor - factor a number into primes
 *
 * By: Landon Curt Noll   chongo@toad.com,   ...!{sun,tolsoft}!hoptoad!chongo
 *
 *   chongo <for a good prime call: 391581 * 2^216193 - 1> /\oo/\
 *
 * usage:
 *	factor [number] ...
 *
 * The form of the output is:
 *
 *	number: factor1 factor1 factor2 factor3 factor3 factor3 ...
 *
 * where factor1 < factor2 < factor3 < ...
 *
 * If no args are given, the list of numbers are read from stdin.
 */

#include <stdio.h>
#include <ctype.h>
#include "primes.h"

/*
 * prime[i] is the (i-1)th prime.
 *
 * We are able to sieve 2^32-1 because this byte table yields all primes 
 * up to 65537 and 65537^2 > 2^32-1.
 */
extern ubig prime[];
extern ubig *pr_limit;	/* largest prime in the prime array */

#define MAX_LINE 255	/* max line allowed on stdin */

void pr_fact();		/* print factors of a value */
long small_fact();	/* find smallest factor of a value */
char *read_num_buf();	/* read a number buffer */
char *program;		/* name of this program */

main(argc, argv)
	int argc;	/* arg count */
	char *argv[];	/* the args */
{
	int arg;	/* which arg to factor */
	long val;	/* the value to factor */
	char buf[MAX_LINE+1];	/* input buffer */

	/* parse args */
	program = argv[0];
	if (argc >= 2) {

		/* factor each arg */
		for (arg=1; arg < argc; ++arg) {

			/* process the buffer */
			if (read_num_buf(NULL, argv[arg]) == NULL) {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);
			}

			/* factor the argument */
			if (sscanf(argv[arg], "%ld", &val) == 1) {
				pr_fact(val);
			} else {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);
			}
		}

	/* no args supplied, read numbers from stdin */
	} else {
		/*
		 * read asciii numbers from input
		 */
		while (read_num_buf(stdin, buf) != NULL) {

			/* factor the argument */
			if (sscanf(buf, "%ld", &val) == 1) {
				pr_fact(val);
			}
		}
	}
	exit(0);
}

/*
 * read_num_buf - read a number buffer from a stream
 *
 * Read a number on a line of the form:
 *
 *	^[ \t]*\([+-]?[0-9][0-9]\)*.*$
 *
 * where ? is a 1-or-0 operator and the number is within \( \).
 *
 * If does not match the above pattern, it is ignored and a new
 * line is read.  If the number is too large or small, we will
 * print ouch and read a new line.
 *
 * We have to be very careful on how we check the magnitude of the
 * input.  We can not use numeric checks because of the need to
 * check values against maximum numeric values.
 *
 * This routine will return a line containing a ascii number between
 * NEG_SEMIBIG and SEMIBIG, or it will return NULL.
 *
 * If the stream is NULL then buf will be processed as if were
 * a single line stream.
 *
 * returns:
 *	char *	pointer to leading digit, + or -
 *	NULL	EOF or error
 */
char *
read_num_buf(input, buf)
	FILE *input;		/* input stream or NULL */
	char *buf;		/* input buffer */
{
	static char limit[MAX_LINE+1];	/* ascii value of SEMIBIG */
	static int limit_len;		/* digit count of limit */
	static char neg_limit[MAX_LINE+1];	/* value of NEG_SEMIBIG */
	static int neg_limit_len;		/* digit count of neg_limit */
	int len;			/* digits in input (excluding +/-) */
	char *s;	/* line start marker */
	char *d;	/* first digit, skip +/- */
	char *p;	/* scan pointer */
	char *z;	/* zero scan pointer */

	/* form the ascii value of SEMIBIG if needed */
	if (!isascii(limit[0]) || !isdigit(limit[0])) {
		sprintf(limit, "%ld", SEMIBIG);
		limit_len = strlen(limit);
		sprintf(neg_limit, "%ld", NEG_SEMIBIG);
		neg_limit_len = strlen(neg_limit)-1;	/* exclude - */
	}
	
	/*
	 * the search for a good line
	 */
	if (input != NULL && fgets(buf, MAX_LINE, input) == NULL) {
		/* error or EOF */
		return NULL;
	}
	do {

		/* ignore leading whitespace */
		for (s=buf; *s && s < buf+MAX_LINE; ++s) {
			if (!isascii(*s) || !isspace(*s)) {
				break;
			}
		}

		/* skip over any leading + or - */
		if (*s == '+' || *s == '-') {
			d = s+1;
		} else {
			d = s;
		}

		/* note leading zeros */
		for (z=d; *z && z < buf+MAX_LINE; ++z) {
			if (*z != '0') {
				break;
			}
		}

		/* scan for the first non-digit */
		for (p=d; *p && p < buf+MAX_LINE; ++p) {
			if (!isascii(*p) || !isdigit(*p)) {
				break;
			}
		}

		/* ignore empty lines */
		if (p == d) {
			continue;
		}
		*p = '\0';

		/* object if too many digits */
		len = strlen(z);
		len = (len<=0) ? 1 : len;
		if (*s == '-') {
			/* accept if digit count is below limit */
			if (len < neg_limit_len) {
				/* we have good input */
				return s;

			/* reject very large numbers */
			} else if (len > neg_limit_len) {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);

			/* carefully check against near limit numbers */
			} else if (strcmp(z, neg_limit+1) > 0) {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);
			}
			/* number is near limit, but is under it */
			return s;
		
		} else {
			/* accept if digit count is below limit */
			if (len < limit_len) {
				/* we have good input */
				return s;

			/* reject very large numbers */
			} else if (len > limit_len) {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);

			/* carefully check against near limit numbers */
			} else if (strcmp(z, limit) > 0) {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);
			}
			/* number is near limit, but is under it */
			return s;
		}
	} while (input != NULL && fgets(buf, MAX_LINE, input) != NULL);

	/* error or EOF */
	return NULL;
}


/*
 * pr_fact - print the factors of a number
 *
 * If the number is 0 or 1, then print the number and return.
 * If the number is < 0, print -1, negate the number and continue
 * processing.
 *
 * Print the factors of the number, from the lowest to the highest.
 * A factor will be printed numtiple times if it divides the value
 * multiple times.
 *
 * Factors are printed with leading tabs.
 */
void
pr_fact(val)
	long val;	/* factor this value */
{
	ubig *fact;	/* the factor found */

	/* firewall - catch 0 and 1 */
	switch (val) {
	case -2147483648:
		/* avoid negation problems */
		puts("-2147483648: -1 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2\n");
		return;
	case -1:
		puts("-1: -1\n");
		return;
	case 0:
		exit(0);
	case 1:
		puts("1: 1\n");
		return;
	default:
		if (val < 0) {
			val = -val;
			printf("%ld: -1", val);
		} else {
			printf("%ld:", val);
		}
		fflush(stdout);
		break;
	}

	/*
	 * factor value
	 */
	fact = &prime[0];
	while (val > 1) {

		/* look for the smallest factor */
		do {
			if (val%(long)*fact == 0) {
				break;
			}
		} while (++fact <= pr_limit);

		/* watch for primes larger than the table */
		if (fact > pr_limit) {
			printf(" %ld\n", val);
			return;
		}

		/* divide factor out until none are left */
		do {
			printf(" %ld", *fact);
			val /= (long)*fact;
		} while ((val % (long)*fact) == 0);
		fflush(stdout);
		++fact;
	}
	putchar('\n');
	return;
}
