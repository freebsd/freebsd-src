/*	$NetBSD: main2.c,v 1.5 2001/11/21 19:14:26 wiz Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: main2.c,v 1.5 2001/11/21 19:14:26 wiz Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lint2.h"

/* warnings for symbols which are declared but not defined or used */
int	xflag;

/*
 * warnings for symbols which are used and not defined or defined
 * and not used
 */
int	uflag = 1;

/* Create a lint library in the current directory with name libname. */
int	Cflag;
const	char *libname;

int	pflag;

/*
 * warnings for (tentative) definitions of the same name in more than
 * one translation unit
 */
int	sflag;

int	tflag;

/*
 * If a complaint stems from a included file, print the name of the included
 * file instead of the name spezified at the command line followed by '?'
 */
int	Hflag;

int	hflag;

/* Print full path names, not only the last component */
int	Fflag;

/*
 * List of libraries (from -l flag). These libraries are read after all
 * other input files has been read and, for Cflag, after the new lint library
 * has been written.
 */
const	char	**libs;

static	void	usage(void);

int main(int, char *[]);

int
main(int argc, char *argv[])
{
	int	c, i;
	size_t	len;
	char	*lname;

	libs = xcalloc(1, sizeof (char *));

	opterr = 0;
	while ((c = getopt(argc, argv, "hpstxuC:HFl:")) != -1) {
		switch (c) {
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			uflag = 0;
			break;
		case 'x':
			xflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'C':
			len = strlen(optarg);
			lname = xmalloc(len + 10);
			(void)sprintf(lname, "llib-l%s.ln", optarg);
			libname = lname;
			Cflag = 1;
			uflag = 0;
			break;
		case 'H':
			Hflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'F':
			Fflag = 1;
			break;
		case 'l':
			for (i = 0; libs[i] != NULL; i++)
				continue;
			libs = xrealloc(libs, (i + 2) * sizeof (char *));
			libs[i] = xstrdup(optarg);
			libs[i + 1] = NULL;
			break;
		case '?':
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	initmem();

	/* initialize hash table */
	inithash();

	inittyp();

	for (i = 0; i < argc; i++)
		readfile(argv[i]);

	/* write the lint library */
	if (Cflag) {
		forall(mkstatic);
		outlib(libname);
	}

	/* read additional libraries */
	for (i = 0; libs[i] != NULL; i++)
		readfile(libs[i]);

	forall(mkstatic);

	mainused();

	/* perform all tests */
	forall(chkname);

	exit(0);
	/* NOTREACHED */
}

static void
usage(void)
{
	(void)fprintf(stderr,
		      "usage: lint2 -hpstxuHF -Clib -l lib ... src1 ...\n");
	exit(1);
}
