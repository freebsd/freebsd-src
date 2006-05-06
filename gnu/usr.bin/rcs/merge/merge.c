/* merge - three-way file merge */

/* Copyright 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

#include "rcsbase.h"

static void badoption P((char const*));

static char const usage[] =
 "\nmerge: usage: merge [-AeEpqxX3] [-L lab [-L lab [-L lab]]] file1 file2 file3";

	static void
badoption(a)
	char const *a;
{
	error("unknown option: %s%s", a, usage);
}


mainProg(mergeId, "merge", "$FreeBSD: src/gnu/usr.bin/rcs/merge/merge.c,v 1.5 1999/08/27 23:36:51 peter Exp $")
{
	register char const *a;
	char const *arg[3], *label[3], *edarg = 0;
	int labels, tostdout;

	labels = 0;
	tostdout = false;

	for (;  (a = *++argv)  &&  *a++ == '-';  --argc) {
		switch (*a++) {
			case 'A': case 'E': case 'e':
				if (edarg  &&  edarg[1] != (*argv)[1])
					error("%s and %s are incompatible",
						edarg, *argv
					);
				edarg = *argv;
				break;

			case 'p': tostdout = true; break;
			case 'q': quietflag = true; break;

			case 'L':
				if (3 <= labels)
					faterror("too many -L options");
				if (!(label[labels++] = *++argv))
					faterror("-L needs following argument");
				--argc;
				break;

			case 'V':
				printf("RCS version %s\n", RCS_version_string);
				exitmain(0);

			default:
				badoption(a - 2);
				continue;
		}
		if (*a)
			badoption(a - 2);
	}

	if (argc != 4)
		faterror("%s arguments%s",
			argc<4 ? "not enough" : "too many",  usage
		);

	/* This copy keeps us `const'-clean.  */
	arg[0] = argv[0];
	arg[1] = argv[1];
	arg[2] = argv[2];

	for (;  labels < 3;  labels++)
		label[labels] = arg[labels];

	if (nerror)
		exiterr();
	exitmain(merge(tostdout, edarg, label, arg));
}


#if RCS_lint
#	define exiterr mergeExit
#endif
	void
exiterr()
{
	tempunlink();
	_exit(DIFF_TROUBLE);
}
