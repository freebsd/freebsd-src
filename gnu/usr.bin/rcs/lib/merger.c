/* merger - three-way file merge internals */

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

libId(mergerId, "$Id: merger.c,v 1.3 1991/08/20 23:05:00 eggert Exp $")

	static char const *
normalize_arg(s, b)
	char const *s;
	char **b;
/*
 * If S looks like an option, prepend ./ to it.  Yield the result.
 * Set *B to the address of any storage that was allocated..
 */
{
	char *t;
	switch (*s) {
		case '-': case '+':
			*b = t = testalloc(strlen(s) + 3);
			VOID sprintf(t, ".%c%s", SLASH, s);
			return t;
		default:
			*b = 0;
			return s;
	}
}

	int
merge(tostdout, label, argv)
	int tostdout;
	char const *const label[2];
	char const *const argv[3];
/*
 * Do `merge [-p] -L l0 -L l1 a0 a1 a2',
 * where TOSTDOUT specifies whether -p is present,
 * LABEL gives l0 and l1, and ARGV gives a0, a1, and a2.
 * Yield DIFF_SUCCESS or DIFF_FAILURE.
 */
{
	register int i;
	FILE *f;
	RILE *rt;
	char const *a[3], *t;
	char *b[3];
	int s;
#if !DIFF3_BIN
	char const *d[2];
#endif

	for (i=3; 0<=--i; )
		a[i] = normalize_arg(argv[i], &b[i]);

#if DIFF3_BIN
	t = 0;
	if (!tostdout)
		t = maketemp(0);
	s = run(
		(char*)0, t,
		DIFF3, "-am", "-L", label[0], "-L", label[1],
		a[0], a[1], a[2], (char*)0
	);
	switch (s) {
		case DIFF_SUCCESS:
			break;
		case DIFF_FAILURE:
			if (!quietflag)
				warn("overlaps during merge");
			break;
		default:
			exiterr();
	}
	if (t) {
		if (!(f = fopen(argv[0], FOPEN_W)))
			efaterror(argv[0]);
		if (!(rt = Iopen(t, FOPEN_R, (struct stat*)0)))
			efaterror(t);
		fastcopy(rt, f);
		Ifclose(rt);
		Ofclose(f);
	}
#else
	for (i=0; i<2; i++)
		switch (run(
			(char*)0, d[i]=maketemp(i),
			DIFF, a[i], a[2], (char*)0
		)) {
			case DIFF_FAILURE: case DIFF_SUCCESS: break;
			default: exiterr();
		}
	t = maketemp(2);
	s = run(
		(char*)0, t,
		DIFF3, "-E", d[0], d[1], a[0], a[1], a[2],
		label[0], label[1], (char*)0
	);
	if (s != DIFF_SUCCESS) {
		s = DIFF_FAILURE;
		if (!quietflag)
			warn("overlaps or other problems during merge");
	}
	if (!(f = fopen(t, "a")))
		efaterror(t);
	aputs(tostdout ? "1,$p\n" : "w\n",  f);
	Ofclose(f);
	if (run(t, (char*)0, ED, "-", a[0], (char*)0))
		exiterr();
#endif

	tempunlink();
	for (i=3; 0<=--i; )
		if (b[i])
			tfree(b[i]);
	return s;
}
