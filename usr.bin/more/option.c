/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)option.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: option.c,v 1.1.1.1.8.1 1997/07/30 06:43:57 charnier Exp $";
#endif /* not lint */

#include <stdio.h>
#include <less.h>

int top_scroll;			/* Repaint screen from top */
int bs_mode;			/* How to process backspaces */
int caseless;			/* Do "caseless" searches */
int cbufs = 10;			/* Current number of buffers */
int linenums = 1;		/* Use line numbers */
int quit_at_eof;
int squeeze;			/* Squeeze multiple blank lines into one */
int tabstop = 8;		/* Tab settings */
int tagoption;

char *firstsearch;
extern int sc_height;

static void usage __P((void));

option(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	static int sc_window_set = 0;
	int ch;
	char *p;

	/* backward compatible processing for "+/search" */
	char **a;
	for (a = argv; *a; ++a)
		if ((*a)[0] == '+' && (*a)[1] == '/')
			(*a)[0] = '-';

	optind = 1;		/* called twice, re-init getopt. */
	while ((ch = getopt(argc, argv, "0123456789/:ceinst:ux:f")) !=  -1)
		switch((char)ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: more was originally designed to take
			 * a number after a dash.
			 */
			if (!sc_window_set) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					sc_height = atoi(++p);
				else
					sc_height = atoi(argv[optind] + 1);
				sc_window_set = 1;
			}
			break;
		case '/':
			firstsearch = optarg;
			break;
		case 'c':
			top_scroll = 1;
			break;
		case 'e':
			quit_at_eof = 1;
			break;
		case 'i':
			caseless = 1;
			break;
		case 'n':
			linenums = 0;
			break;
		case 's':
			squeeze = 1;
			break;
		case 't':
			tagoption = 1;
			findtag(optarg);
			break;
		case 'u':
			bs_mode = 1;
			break;
		case 'x':
			tabstop = atoi(optarg);
			if (tabstop <= 0)
				tabstop = 8;
			break;
		case 'f':	/* ignore -f, compatability with old more */
			break;
		case '?':
		default:
			usage();
		}
	return(optind);
}

static void
usage()
{
	fprintf(stderr,
	"usage: more [-ceinus] [-t tag] [-x tabs] [-/ pattern] [-#] [file ...]\n");
	exit(1);
}
