/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Eamonn McManus of Trinity College Dublin.
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
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)arithmetic.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/arithmetic/arithmetic.c,v 1.10 1999/12/12 06:40:28 billf Exp $";
#endif /* not lint */

/*
 * By Eamonn McManus, Trinity College Dublin <emcmanus@cs.tcd.ie>.
 *
 * The operation of this program mimics that of the standard Unix game
 * `arithmetic'.  I've made it as close as I could manage without examining
 * the source code.  The principal differences are:
 *
 * The method of biasing towards numbers that had wrong answers in the past
 * is different; original `arithmetic' seems to retain the bias forever,
 * whereas this program lets the bias gradually decay as it is used.
 *
 * Original `arithmetic' delays for some period (3 seconds?) after printing
 * the score.  I saw no reason for this delay, so I scrapped it.
 *
 * There is no longer a limitation on the maximum range that can be supplied
 * to the program.  The original program required it to be less than 100.
 * Anomalous results may occur with this program if ranges big enough to
 * allow overflow are given.
 *
 * I have obviously not attempted to duplicate bugs in the original.  It
 * would go into an infinite loop if invoked as `arithmetic / 0'.  It also
 * did not recognise an EOF in its input, and would continue trying to read
 * after it.  It did not check that the input was a valid number, treating any
 * garbage as 0.  Finally, it did not flush stdout after printing its prompt,
 * so in the unlikely event that stdout was not a terminal, it would not work
 * properly.
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

const char keylist[] = "+-x/";
const char defaultkeys[] = "+-";
const char *keys = defaultkeys;
int nkeys = sizeof(defaultkeys) - 1;
int rangemax = 10;
int nright, nwrong;
time_t qtime;
#define	NQUESTS	20

static void usage __P((void));
int getrandom __P((int, int, int));
void intr __P((int));
int opnum __P((int));
void penalise __P((int, int, int));
int problem __P((void));
void showstats __P((void));

/*
 * Select keys from +-x/ to be asked addition, subtraction, multiplication,
 * and division problems.  More than one key may be given.  The default is
 * +-.  Specify a range to confine the operands to 0 - range.  Default upper
 * bound is 10.  After every NQUESTS questions, statistics on the performance
 * so far are printed.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, cnt;

	/* Revoke setgid privileges */
	setgid(getgid());

	while ((ch = getopt(argc, argv, "r:o:")) != -1)
		switch(ch) {
		case 'o': {
			const char *p;

			for (p = keys = optarg; *p; ++p)
				if (!index(keylist, *p)) {
					(void)fprintf(stderr,
					    "arithmetic: unknown key.\n");
					exit(1);
				}
			nkeys = p - optarg;
			break;
		}
		case 'r':
			if ((rangemax = atoi(optarg)) <= 0) {
				(void)fprintf(stderr,
				    "arithmetic: invalid range.\n");
				exit(1);
			}
			break;
		case '?':
		default:
			usage();
		}
	if (argc -= optind)
		usage();

	/* Seed the random-number generator. */
	srandomdev();

	(void)signal(SIGINT, intr);

	/* Now ask the questions. */
	for (;;) {
		for (cnt = NQUESTS; cnt--;)
			if (problem() == EOF)
				exit(0);
		showstats();
	}
	/* NOTREACHED */
}

/* Handle interrupt character.  Print score and exit. */
void
intr(sig)
	int sig;
{
	showstats();
	exit(0);
}

/* Print score.  Original `arithmetic' had a delay after printing it. */
void
showstats()
{
	if (nright + nwrong > 0) {
		(void)printf("\n\nRights %d; Wrongs %d; Score %d%%",
		    nright, nwrong, (int)(100L * nright / (nright + nwrong)));
		if (nright > 0)
	(void)printf("\nTotal time %ld seconds; %.1f seconds per problem\n\n",
			    (long)qtime, (float)qtime / nright);
	}
	(void)printf("\n");
}

/*
 * Pick a problem and ask it.  Keeps asking the same problem until supplied
 * with the correct answer, or until EOF or interrupt is typed.  Problems are
 * selected such that the right operand and either the left operand (for +, x)
 * or the correct result (for -, /) are in the range 0 to rangemax.  Each wrong
 * answer causes the numbers in the problem to be penalised, so that they are
 * more likely to appear in subsequent problems.
 */
int
problem()
{
	char *p;
	time_t start, finish;
	int left, op, right, result;
	char line[80];

	left = 0;
	right = 0;
	result = 0;
	op = keys[random() % nkeys];
	if (op != '/')
		right = getrandom(rangemax + 1, op, 1);
retry:
	/* Get the operands. */
	switch (op) {
	case '+':
		left = getrandom(rangemax + 1, op, 0);
		result = left + right;
		break;
	case '-':
		result = getrandom(rangemax + 1, op, 0);
		left = right + result;
		break;
	case 'x':
		left = getrandom(rangemax + 1, op, 0);
		result = left * right;
		break;
	case '/':
		right = getrandom(rangemax, op, 1) + 1;
		result = getrandom(rangemax + 1, op, 0);
		left = right * result + random() % right;
		break;
	}

	/*
	 * A very big maxrange could cause negative values to pop
	 * up, owing to overflow.
	 */
	if (result < 0 || left < 0)
		goto retry;

	(void)printf("%d %c %d =   ", left, op, right);
	(void)fflush(stdout);
	(void)time(&start);

	/*
	 * Keep looping until the correct answer is given, or until EOF or
	 * interrupt is typed.
	 */
	for (;;) {
		if (!fgets(line, sizeof(line), stdin)) {
			(void)printf("\n");
			return(EOF);
		}
		for (p = line; *p && isspace(*p); ++p);
		if (!isdigit(*p)) {
			(void)printf("Please type a number.\n");
			continue;
		}
		if (atoi(p) == result) {
			(void)printf("Right!\n");
			++nright;
			break;
		}
		/* Wrong answer; penalise and ask again. */
		(void)printf("What?\n");
		++nwrong;
		penalise(right, op, 1);
		if (op == 'x' || op == '+')
			penalise(left, op, 0);
		else
			penalise(result, op, 0);
	}

	/*
	 * Accumulate the time taken.  Obviously rounding errors happen here;
	 * however they should cancel out, because some of the time you are
	 * charged for a partially elapsed second at the start, and some of
	 * the time you are not charged for a partially elapsed second at the
	 * end.
	 */
	(void)time(&finish);
	qtime += finish - start;
	return(0);
}

/*
 * Here is the code for accumulating penalties against the numbers for which
 * a wrong answer was given.  The right operand and either the left operand
 * (for +, x) or the result (for -, /) are stored in a list for the particular
 * operation, and each becomes more likely to appear again in that operation.
 * Initially, each number is charged a penalty of WRONGPENALTY, giving it that
 * many extra chances of appearing.  Each time it is selected because of this,
 * its penalty is decreased by one; it is removed when it reaches 0.
 *
 * The penalty[] array gives the sum of all penalties in the list for
 * each operation and each operand.  The penlist[] array has the lists of
 * penalties themselves.
 */

int penalty[sizeof(keylist) - 1][2];
struct penalty {
	int value, penalty;	/* Penalised value and its penalty. */
	struct penalty *next;
} *penlist[sizeof(keylist) - 1][2];

#define	WRONGPENALTY	5	/* Perhaps this should depend on maxrange. */

/*
 * Add a penalty for the number `value' to the list for operation `op',
 * operand number `operand' (0 or 1).  If we run out of memory, we just
 * forget about the penalty (how likely is this, anyway?).
 */
void
penalise(value, op, operand)
	int value, op, operand;
{
	struct penalty *p;

	op = opnum(op);
	if ((p = (struct penalty *)malloc((u_int)sizeof(*p))) == NULL)
		return;
	p->next = penlist[op][operand];
	penlist[op][operand] = p;
	penalty[op][operand] += p->penalty = WRONGPENALTY;
	p->value = value;
}

/*
 * Select a random value from 0 to maxval - 1 for operand `operand' (0 or 1)
 * of operation `op'.  The random number we generate is either used directly
 * as a value, or represents a position in the penalty list.  If the latter,
 * we find the corresponding value and return that, decreasing its penalty.
 */
int
getrandom(maxval, op, operand)
	int maxval, op, operand;
{
	int value;
	struct penalty **pp, *p;

	op = opnum(op);
	value = random() % (maxval + penalty[op][operand]);

	/*
	 * 0 to maxval - 1 is a number to be used directly; bigger values
	 * are positions to be located in the penalty list.
	 */
	if (value < maxval)
		return(value);
	value -= maxval;

	/*
	 * Find the penalty at position `value'; decrement its penalty and
	 * delete it if it reaches 0; return the corresponding value.
	 */
	for (pp = &penlist[op][operand]; (p = *pp) != NULL; pp = &p->next) {
		if (p->penalty > value) {
			value = p->value;
			penalty[op][operand]--;
			if (--(p->penalty) <= 0) {
				p = p->next;
				(void)free((char *)*pp);
				*pp = p;
			}
			return(value);
		}
		value -= p->penalty;
	}
	/*
	 * We can only get here if the value from the penalty[] array doesn't
	 * correspond to the actual sum of penalties in the list.  Provide an
	 * obscure message.
	 */
	(void)fprintf(stderr, "arithmetic: bug: inconsistent penalties\n");
	exit(1);
	/* NOTREACHED */
}

/* Return an index for the character op, which is one of [+-x/]. */
int
opnum(op)
	int op;
{
	char *p;

	if (op == 0 || (p = index(keylist, op)) == NULL) {
		(void)fprintf(stderr,
		    "arithmetic: bug: op %c not in keylist %s\n", op, keylist);
		exit(1);
	}
	return(p - keylist);
}

/* Print usage message and quit. */
static void
usage()
{
	(void)fprintf(stderr, "usage: arithmetic [-o +-x/] [-r range]\n");
	exit(1);
}
