/* three-way file merge internals */

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

libId(mergerId, "$FreeBSD$")

	static char const *normalize_arg P((char const*,char**));
	static char const *
normalize_arg(s, b)
	char const *s;
	char **b;
/*
 * If S looks like an option, prepend ./ to it.  Yield the result.
 * Set *B to the address of any storage that was allocated.
 */
{
	char *t;
	if (*s == '-') {
		*b = t = testalloc(strlen(s) + 3);
		VOID sprintf(t, ".%c%s", SLASH, s);
		return t;
	} else {
		*b = 0;
		return s;
	}
}

	int
merge(tostdout, edarg, label, argv)
	int tostdout;
	char const *edarg;
	char const *const label[3];
	char const *const argv[3];
/*
 * Do `merge [-p] EDARG -L l0 -L l1 -L l2 a0 a1 a2',
 * where TOSTDOUT specifies whether -p is present,
 * EDARG gives the editing type (e.g. "-A", or null for the default),
 * LABEL gives l0, l1 and l2, and ARGV gives a0, a1 and a2.
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

	if (!edarg)
		edarg = "-E";

#if DIFF3_BIN
	t = 0;
	if (!tostdout)
		t = maketemp(0);
	s = run(
		-1, t,
		DIFF3, edarg, "-am",
		"-L", label[0],
		"-L", label[1],
		"-L", label[2],
		a[0], a[1], a[2], (char*)0
	);
	switch (s) {
		case DIFF_SUCCESS:
			break;
		case DIFF_FAILURE:
			warn("conflicts during merge");
			break;
		default:
			exiterr();
	}
	if (t) {
		if (!(f = fopenSafer(argv[0], "w")))
			efaterror(argv[0]);
		if (!(rt = Iopen(t, "r", (struct stat*)0)))
			efaterror(t);
		fastcopy(rt, f);
		Ifclose(rt);
		Ofclose(f);
	}
#else
	for (i=0; i<2; i++)
		switch (run(
			-1, d[i]=maketemp(i),
			DIFF, a[i], a[2], (char*)0
		)) {
			case DIFF_FAILURE: case DIFF_SUCCESS: break;
			default: faterror("diff failed");
		}
	t = maketemp(2);
	s = run(
		-1, t,
		DIFF3, edarg, d[0], d[1], a[0], a[1], a[2],
		label[0], label[2], (char*)0
	);
	if (s != DIFF_SUCCESS) {
		s = DIFF_FAILURE;
		warn("overlaps or other problems during merge");
	}
	if (!(f = fopenSafer(t, "a+")))
		efaterror(t);
	aputs(tostdout ? "1,$p\n" : "w\n",  f);
	Orewind(f);
	aflush(f);
	if (run(fileno(f), (char*)0, ED, "-", a[0], (char*)0))
		exiterr();
	Ofclose(f);
#endif

	tempunlink();
	for (i=3; 0<=--i; )
		if (b[i])
			tfree(b[i]);
	return s;
}
