/* merge - three-way file merge */

/* Copyright 1991 by Paul Eggert
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
along with RCS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

#include "rcsbase.h"


static char const usage[] =
 "\nmerge: usage: merge [-p] [-q] [-L label1 [-L label3]] file1 file2 file3\n";

	static exiting void
badoption(a)
	char const *a;
{
	faterror("unknown option: %s%s", a-2, usage);
}


mainProg(mergeId, "merge", "$Id: merge.c,v 1.2 1991/08/19 03:13:55 eggert Exp $")
{
	register char const *a;
	char const *label[2], *arg[3];
	int labels, tostdout;

	labels = 0;
	tostdout = false;

	while ((a = *++argv)  &&  *a++ == '-') {
		switch (*a++) {
			case 'p': tostdout = true; break;
			case 'q': quietflag = true; break;
			case 'L':
				if (1<labels)
					faterror("too many -L options");
				if (!(label[labels++] = *++argv))
					faterror("-L needs following argument");
				--argc;
				break;
			default:
				badoption(a);
		}
		if (*a)
			badoption(a);
		--argc;
	}

	if (argc != 4)
		faterror("%s arguments%s",
			argc<4 ? "not enough" : "too many",  usage
		);

	/* This copy keeps us `const'-clean.  */
	arg[0] = argv[0];
	arg[1] = argv[1];
	arg[2] = argv[2];

	switch (labels) {
		case 0: label[0] = arg[0]; /* fall into */
		case 1: label[1] = arg[2];
	}

	exitmain(merge(tostdout, label, arg));
}


#if lint
#	define exiterr mergeExit
#endif
	exiting void
exiterr()
{
	tempunlink();
	_exit(DIFF_TROUBLE);
}
