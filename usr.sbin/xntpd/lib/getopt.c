/* getopt.c,v 3.1 1993/07/06 01:08:18 jbj Exp
 * getopt - get option letter from argv
 *
 * This is a version of the public domain getopt() implementation by
 * Henry Spencer, changed for 4.3BSD compatibility (in addition to System V).
 * It allows rescanning of an option list by setting optind to 0 before
 * calling.  Thanks to Dennis Ferguson for the appropriate modifications.
 *
 * This file is in the Public Domain.
 */

/*LINTLIBRARY*/

#include <stdio.h>

#include "ntp_stdlib.h"

#ifdef	lint
#undef	putc
#define	putc	fputc
#endif	/* lint */

char	*optarg;	/* Global argument pointer. */
#ifndef	__convex__
int	optind = 0;	/* Global argv index. */
#else	/* __convex__ */
extern	int	optind;	/* Global argv index. */
#endif	/* __convex__ */

/*
 * N.B. use following at own risk
 */
#ifndef	__convex__
int	opterr = 1;	/* for compatibility, should error be printed? */
#else	/* __convex__ */
extern	int	opterr;	/* for compatibility, should error be printed? */
#endif	/* __convex__ */
int	optopt;		/* for compatibility, option character checked */

static char	*scan = NULL;	/* Private scan pointer. */

/*
 * Print message about a bad option.  Watch this definition, it's
 * not a single statement.
 */
#define	BADOPT(mess, ch)	if (opterr) { \
					fputs(argv[0], stderr); \
					fputs(mess, stderr); \
					(void) putc(ch, stderr); \
					(void) putc('\n', stderr); \
				} \
				return('?')

int
getopt_l(argc, argv, optstring)
	int argc;
	char *argv[];
	char *optstring;
{
	register char c;
	register char *place;

	optarg = NULL;

	if (optind == 0) {
		scan = NULL;
		optind++;
	}
	
	if (scan == NULL || *scan == '\0') {
		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
			return EOF;
		if (argv[optind][1] == '-' && argv[optind][2] == '\0') {
			optind++;
			return EOF;
		}
	
		scan = argv[optind]+1;
		optind++;
	}

	c = *scan++;
	optopt = c & 0377;
	for (place = optstring; place != NULL && *place != '\0'; ++place)
		if (*place == c)
			break;

	if (place == NULL || *place == '\0' || c == ':' || c == '?') {
		BADOPT(": unknown option -", c);
	}

	place++;
	if (*place == ':') {
		if (*scan != '\0') {
			optarg = scan;
			scan = NULL;
		} else if (optind >= argc) {
			BADOPT(": option requires argument -", c);
		} else {
			optarg = argv[optind];
			optind++;
		}
	}

	return c&0377;
}
