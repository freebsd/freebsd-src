/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)primes.c	8.5 (Berkeley) 5/10/95";
#endif /* not lint */

/*
 * primes - generate a table of primes between two values
 *
 * By: Landon Curt Noll chongo@toad.com, ...!{sun,tolsoft}!hoptoad!chongo
 *
 * chongo <for a good prime call: 391581 * 2^216193 - 1> /\oo/\
 *
 * usage:
 *	primes [start [stop]]
 *
 *	Print primes >= start and < stop.  If stop is omitted,
 *	the value 4294967295 (2^32-1) is assumed.  If start is
 *	omitted, start is read from standard input.
 *
 * validation check: there are 664579 primes between 0 and 10^7
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

void	primes __P((ubig, ubig));
ubig	read_num_buf __P((void));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	ubig start;		/* where to start generating */
	ubig stop;		/* don't generate at or above this value */
	int ch;
	char *p;

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch (ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	start = 0;
	stop = BIG;

	/*
	 * Convert low and high args.  Strtoul(3) sets errno to
	 * ERANGE if the number is too large, but, if there's
	 * a leading minus sign it returns the negation of the
	 * result of the conversion, which we'd rather disallow.
	 */
	switch (argc) {
	case 2:
		/* Start and stop supplied on the command line. */
		if (argv[0][0] == '-' || argv[1][0] == '-')
			errx(1, "negative numbers aren't permitted.");

		errno = 0;
		start = strtoul(argv[0], &p, 10);
		if (errno)
			err(1, "%s", argv[0]);
		if (*p != '\0')
			errx(1, "%s: illegal numeric format.", argv[0]);

		errno = 0;
		stop = strtoul(argv[1], &p, 10);
		if (errno)
			err(1, "%s", argv[1]);
		if (*p != '\0')
			errx(1, "%s: illegal numeric format.", argv[1]);
		break;
	case 1:
		/* Start on the command line. */
		if (argv[0][0] == '-')
			errx(1, "negative numbers aren't permitted.");

		errno = 0;
		start = strtoul(argv[0], &p, 10);
		if (errno)
			err(1, "%s", argv[0]);
		if (*p != '\0')
			errx(1, "%s: illegal numeric format.", argv[0]);
		break;
	case 0:
		start = read_num_buf();
		break;
	default:
		usage();
	}

	if (start > stop)
		errx(1, "start value must be less than stop value.");
	primes(start, stop);
	exit(0);
}

/*
 * read_num_buf --
 *	This routine returns a number n, where 0 <= n && n <= BIG.
 */
ubig
read_num_buf()
{
	ubig val;
	char *p, buf[100];		/* > max number of digits. */

	for (;;) {
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			if (ferror(stdin))
				err(1, "stdin");
			exit(0);
		}
		for (p = buf; isblank(*p); ++p);
		if (*p == '\n' || *p == '\0')
			continue;
		if (*p == '-')
			errx(1, "negative numbers aren't permitted.");
		errno = 0;
		val = strtoul(buf, &p, 10);
		if (errno)
			err(1, "%s", buf);
		if (*p != '\n')
			errx(1, "%s: illegal numeric format.", buf);
		return (val);
	}
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
	 * A number of systems can not convert double values into unsigned
	 * longs when the values are larger than the largest signed value.
	 * We don't have this problem, so we can go all the way to BIG.
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
		    factor < stop && p <= pr_limit; factor = *(++p)) {
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
		    fact_lim+pattern_size<=TABSIZE; fact_lim+=pattern_size) {
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

void
usage()
{
	(void)fprintf(stderr, "usage: primes [start [stop]]\n");
	exit(1);
}
