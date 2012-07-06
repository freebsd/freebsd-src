/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if !defined(HAVE_GETOPT) || defined(WANT_GETOPT_LONG) || defined(BROKEN_GETOPT)

#if defined(LIBC_SCCS) && !defined(lint)
/* static char sccsid[] = "from: @(#)getopt.c	8.2 (Berkeley) 4/2/94"; */
static char *rcsid = "$Id: getopt.c,v 1.3 1999/01/08 02:14:18 sjg Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define	BADCH	(int)'?'
#define	BADARG	(int)':'
#define	EMSG	""

int	opterr = 1,		/* if error message should be printed */
	optind = 1,		/* index into parent argv vector */
	optopt = BADCH,		/* character checked for validity */
	optreset;		/* reset getopt */
char	*optarg;		/* argument associated with option */

/*
 * getopt --
 *	Parse argc/argv argument vector.
 */
int
getopt(nargc, nargv, ostr)
	int nargc;
	char * const *nargv;
	const char *ostr;
{
	extern char *__progname;
	static char *place = EMSG;		/* option letter processing */
	char *oli;				/* option letter list index */

#ifndef BSD4_4
	if (!__progname) {
		if (__progname = strrchr(nargv[0], '/'))
			++__progname;
		else
			__progname = nargv[0];
	}
#endif
	
	if (optreset || !*place) {		/* update scanning pointer */
		optreset = 0;
		if (optind >= nargc || *(place = nargv[optind]) != '-') {
			place = EMSG;
			return (-1);
		}
		if (place[1] && *++place == '-'	/* found "--" */
		    && !place[1]) {		/* and not "--foo" */
			++optind;
			place = EMSG;
			return (-1);
		}
	}					/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' ||
	    !(oli = strchr(ostr, optopt))) {
		/*
		 * if the user didn't specify '-' as an option,
		 * assume it means -1.
		 */
		if (optopt == (int)'-')
			return (-1);
		if (!*place)
			++optind;
		if (opterr && *ostr != ':')
			(void)fprintf(stderr,
			    "%s: illegal option -- %c\n", __progname, optopt);
		return (BADCH);
	}
	if (*++oli != ':') {			/* don't need argument */
		optarg = NULL;
		if (!*place)
			++optind;
	}
	else {					/* need an argument */
		if (*place)			/* no white space */
			optarg = place;
		else if (nargc <= ++optind) {	/* no arg */
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (opterr)
				(void)fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    __progname, optopt);
			return (BADCH);
		}
	 	else				/* white space */
			optarg = nargv[optind];
		place = EMSG;
		++optind;
	}
	return (optopt);			/* dump back option letter */
}
#endif
#ifdef MAIN
#ifndef BSD4_4
char *__progname;
#endif

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	char *opts = argv[1];

	--argc;
	++argv;
	
	while ((c = getopt(argc, argv, opts)) != EOF) {
		switch (c) {
		case '-':
			if (optarg)
				printf("--%s ", optarg);
			break;
		case '?':
			exit(1);
			break;
		default:
			if (optarg)
				printf("-%c %s ", c, optarg);
			else
				printf("-%c ", c);
			break;
		}
	}

	if (optind < argc) {
		printf("-- ");
		for (; optind < argc; ++optind) {
			printf("%s ", argv[optind]);
		}
	}
	printf("\n");
	exit(0);
}
#endif
