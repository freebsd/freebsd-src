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
static char sccsid[] = "@(#)primes.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * primes - generate a table of primes between two values
 *
 * By: Landon Curt Noll   chongo@toad.com,   ...!{sun,tolsoft}!hoptoad!chongo
 *
 *   chongo <for a good prime call: 391581 * 2^216193 - 1> /\oo/\
 *
 * usage:
 *	primes [start [stop]]
 *
 *	Print primes >= start and < stop.  If stop is omitted,
 *	the value 4294967295 (2^32-1) is assumed.  If start is
 *	omitted, start is read from standard input.
 *
 *	Prints "ouch" if start or stop is bogus.
 *
 * validation check: there are 664579 primes between 0 and 10^7
 */

#include <stdio.h>
#include <math.h>
#include <memory.h>
#include <ctype.h>
#include "primes.h"

/*
 * Eratosthenes sieve table
 *
 * We only sieve the odd numbers.  The base of our sieve windows are always
 * odd.  If the base of table is 1, table[i] represents 2*i-1.  After the
 * sieve, table[i] == 1 if and only iff 2*i-1 is prime.
 *
 * We make TABSIZE large to reduce the overhead of inner loop setup.
 */
char table[TABSIZE];	 /* Eratosthenes sieve of odd numbers */

/*
 * prime[i] is the (i-1)th prime.
 *
 * We are able to sieve 2^32-1 because this byte table yields all primes 
 * up to 65537 and 65537^2 > 2^32-1.
 */
extern ubig prime[];
extern ubig *pr_limit;		/* largest prime in the prime array */

/*
 * To avoid excessive sieves for small factors, we use the table below to 
 * setup our sieve blocks.  Each element represents a odd number starting 
 * with 1.  All non-zero elements are factors of 3, 5, 7, 11 and 13.
 */
extern char pattern[];
extern int pattern_size;	/* length of pattern array */

#define MAX_LINE 255    /* max line allowed on stdin */

char *read_num_buf();	 /* read a number buffer */
void primes();		 /* print the primes in range */
char *program;		 /* our name */

main(argc, argv)
	int argc;	/* arg count */
	char *argv[];	/* args */
{
	char buf[MAX_LINE+1];   /* input buffer */
	char *ret;	/* return result */
	ubig start;	/* where to start generating */
	ubig stop;	/* don't generate at or above this value */

	/*
	 * parse args
	 */
	program = argv[0];
	start = 0;
	stop = BIG;
	if (argc == 3) {
		/* convert low and high args */
		if (read_num_buf(NULL, argv[1]) == NULL) {
			fprintf(stderr, "%s: ouch\n", program);
			exit(1);
		}
		if (read_num_buf(NULL, argv[2]) == NULL) {
			fprintf(stderr, "%s: ouch\n", program);
			exit(1);
		}
		if (sscanf(argv[1], "%ld", &start) != 1) {
			fprintf(stderr, "%s: ouch\n", program);
			exit(1);
		}
		if (sscanf(argv[2], "%ld", &stop) != 1) {
			fprintf(stderr, "%s: ouch\n", program);
			exit(1);
		}

	} else if (argc == 2) {
		/* convert low arg */
		if (read_num_buf(NULL, argv[1]) == NULL) {
			fprintf(stderr, "%s: ouch\n", program);
			exit(1);
		}
		if (sscanf(argv[1], "%ld", &start) != 1) {
			fprintf(stderr, "%s: ouch\n", program);
			exit(1);
		}

	} else {
		/* read input until we get a good line */
		if (read_num_buf(stdin, buf) != NULL) {

			/* convert the buffer */
			if (sscanf(buf, "%ld", &start) != 1) {
				fprintf(stderr, "%s: ouch\n", program);
				exit(1);
			}
		} else {
			exit(0);
		}
	}
	if (start > stop) {
		fprintf(stderr, "%s: ouch\n", program);
		exit(1);
	}
	primes(start, stop);
	exit(0);
}

/*
 * read_num_buf - read a number buffer from a stream
 *
 * Read a number on a line of the form:
 *
 *	^[ \t]*\(+?[0-9][0-9]\)*.*$
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
 * 0 and BIG, or it will return NULL.
 *
 * If the stream is NULL then buf will be processed as if were
 * a single line stream.
 *
 * returns:
 *	char *	pointer to leading digit or +
 *	NULL	EOF or error
 */
char *
read_num_buf(input, buf)
	FILE *input;		/* input stream or NULL */
	char *buf;		/* input buffer */
{
	static char limit[MAX_LINE+1];	/* ascii value of BIG */
	static int limit_len;		/* digit count of limit */
	int len;			/* digits in input (excluding +/-) */
	char *s;	/* line start marker */
	char *d;	/* first digit, skip +/- */
	char *p;	/* scan pointer */
	char *z;	/* zero scan pointer */

	/* form the ascii value of SEMIBIG if needed */
	if (!isascii(limit[0]) || !isdigit(limit[0])) {
		sprintf(limit, "%ld", SEMIBIG);
		limit_len = strlen(limit);
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

		/* object if - */
		if (*s == '-') {
			fprintf(stderr, "%s: ouch\n", program);
			continue;
		}

		/* skip over any leading + */
		if (*s == '+') {
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

		/* scan for the first non-digit/non-plus/non-minus */
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
		/* accept if digit count is below limit */
		if (len < limit_len) {
			/* we have good input */
			return s;

		/* reject very large numbers */
		} else if (len > limit_len) {
			fprintf(stderr, "%s: ouch\n", program);
			continue;

		/* carefully check against near limit numbers */
		} else if (strcmp(z, limit) > 0) {
			fprintf(stderr, "%s: ouch\n", program);
			continue;
		}
		/* number is near limit, but is under it */
		return s;
	} while (input != NULL && fgets(buf, MAX_LINE, input) != NULL);

	/* error or EOF */
	return NULL;
}

/*
 * primes - sieve and print primes from start up to and but not including stop
 */
void
primes(start, stop)
	ubig start;	/* where to start generating */
	ubig stop;	/* don't generate at or above this value */
{
	register char *q;		/* sieve spot */
	register ubig factor;		/* index and factor */
	register char *tab_lim;		/* the limit to sieve on the table */
	register ubig *p;		/* prime table pointer */
	register ubig fact_lim;		/* highest prime for current block */

	/*
	 * A number of systems can not convert double values 
	 * into unsigned longs when the values are larger than
	 * the largest signed value.  Thus we take case when
	 * the double is larger than the value SEMIBIG. *sigh*
	 */
	if (start < 3) {
		start = (ubig)2;
	}
	if (stop < 3) {
		stop = (ubig)2;
	}
	if (stop <= start) {
		return;
	}

	/*
	 * be sure that the values are odd, or 2
	 */
	if (start != 2 && (start&0x1) == 0) {
		++start;
	}
	if (stop != 2 && (stop&0x1) == 0) {
		++stop;
	}

	/*
	 * quick list of primes <= pr_limit
	 */
	if (start <= *pr_limit) {
		/* skip primes up to the start value */
		for (p = &prime[0], factor = prime[0];
		     factor < stop && p <= pr_limit; 
		     factor = *(++p)) {
			if (factor >= start) {
				printf("%u\n", factor);
			}
		}
		/* return early if we are done */
		if (p <= pr_limit) {
			return;
		}
		start = *pr_limit+2;
	}

	/*
	 * we shall sieve a bytemap window, note primes and move the window
	 * upward until we pass the stop point
	 */
	while (start < stop) {
		/*
		 * factor out 3, 5, 7, 11 and 13
		 */
		/* initial pattern copy */
		factor = (start%(2*3*5*7*11*13))/2; /* starting copy spot */
		memcpy(table, &pattern[factor], pattern_size-factor);
		/* main block pattern copies */
		for (fact_lim=pattern_size-factor;
		     fact_lim+pattern_size<=TABSIZE;
		     fact_lim+=pattern_size) {
			memcpy(&table[fact_lim], pattern, pattern_size);
		}
		/* final block pattern copy */
		memcpy(&table[fact_lim], pattern, TABSIZE-fact_lim);

		/*
		 * sieve for primes 17 and higher
		 */
		/* note highest useful factor and sieve spot */
		if (stop-start > TABSIZE+TABSIZE) {
			tab_lim = &table[TABSIZE]; /* sieve it all */
			fact_lim = (int)sqrt(
					(double)(start)+TABSIZE+TABSIZE+1.0);
		} else {
			tab_lim = &table[(stop-start)/2]; /* partial sieve */
			fact_lim = (int)sqrt((double)(stop)+1.0);
		}
		/* sieve for factors >= 17 */
		factor = 17;	/* 17 is first prime to use */
		p = &prime[7];	/* 19 is next prime, pi(19)=7 */
		do {
			/* determine the factor's initial sieve point */
			q = (char *)(start%factor); /* temp storage for mod */
			if ((int)q & 0x1) {
				q = &table[(factor-(int)q)/2];
			} else {
				q = &table[q ? factor-((int)q/2) : 0];
			}
			/* sive for our current factor */
			for ( ; q < tab_lim; q += factor) {
				*q = '\0'; /* sieve out a spot */
			}
		} while ((factor=(ubig)(*(p++))) <= fact_lim);

		/*
		 * print generated primes
		 */
		for (q = table; q < tab_lim; ++q, start+=2) {
			if (*q) {
				printf("%u\n", start);
			}
		}
	}
}
