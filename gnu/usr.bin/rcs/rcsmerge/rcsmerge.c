/*
 *                       rcsmerge operation
 */
/*****************************************************************************
 *                       join 2 revisions with respect to a third
 *****************************************************************************
 */

/* Copyright (C) 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991 by Paul Eggert
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



/* $Log: rcsmerge.c,v $
 * Revision 5.7  1991/11/20  17:58:09  eggert
 * Don't Iopen(f, "r+"); it's not portable.
 *
 * Revision 5.6  1991/08/19  03:13:55  eggert
 * Add -r$.  Tune.
 *
 * Revision 5.5  1991/04/21  11:58:27  eggert
 * Add -x, RCSINIT, MS-DOS support.
 *
 * Revision 5.4  1991/02/25  07:12:43  eggert
 * Merging a revision to itself is no longer an error.
 *
 * Revision 5.3  1990/11/01  05:03:50  eggert
 * Remove unneeded setid check.
 *
 * Revision 5.2  1990/09/04  08:02:28  eggert
 * Check for I/O error when reading working file.
 *
 * Revision 5.1  1990/08/29  07:14:04  eggert
 * Add -q.  Pass -L options to merge.
 *
 * Revision 5.0  1990/08/22  08:13:41  eggert
 * Propagate merge's exit status.
 * Remove compile-time limits; use malloc instead.
 * Make lock and temp files faster and safer.  Ansify and Posixate.  Add -V.
 * Don't use access().  Tune.
 *
 * Revision 4.5  89/05/01  15:13:16  narten
 * changed copyright header to reflect current distribution rules
 * 
 * Revision 4.4  88/08/09  19:13:13  eggert
 * Beware merging into a readonly file.
 * Beware merging a revision to itself (no change).
 * Use execv(), not system(); yield exit status like diff(1)'s.
 * 
 * Revision 4.3  87/10/18  10:38:02  narten
 * Updating version numbers. Changes relative to version 1.1 
 * actually relative to 4.1
 * 
 * Revision 1.3  87/09/24  14:00:31  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf 
 * warnings)
 * 
 * Revision 1.2  87/03/27  14:22:36  jenkins
 * Port to suns
 * 
 * Revision 4.1  83/03/28  11:14:57  wft
 * Added handling of default branch.
 * 
 * Revision 3.3  82/12/24  15:29:00  wft
 * Added call to catchsig().
 *
 * Revision 3.2  82/12/10  21:32:02  wft
 * Replaced getdelta() with gettree(); improved error messages.
 *
 * Revision 3.1  82/11/28  19:27:44  wft
 * Initial revision.
 *
 */
#include "rcsbase.h"

static char const co[] = CO;

mainProg(rcsmergeId, "rcsmerge", "$Id: rcsmerge.c,v 5.7 1991/11/20 17:58:09 eggert Exp $")
{
	static char const cmdusage[] =
		"\nrcsmerge usage: rcsmerge -rrev1 [-rrev2] [-p] [-Vn] file";
	static char const quietarg[] = "-q";

	register int i;
	char *a, **newargv;
	char const *arg[3];
	char const *rev[2]; /*revision numbers*/
	char const *expandarg, *versionarg;
        int tostdout;
	int status;
	RILE *workptr;
	struct buf commarg;
	struct buf numericrev; /* holds expanded revision number */
	struct hshentries *gendeltas; /* deltas to be generated */
        struct hshentry * target;

	bufautobegin(&commarg);
	bufautobegin(&numericrev);
	rev[0] = rev[1] = nil;
	status = 0; /* Keep lint happy.  */
	tostdout = false;
	expandarg = versionarg = quietarg; /* i.e. a no-op */
	suffixes = X_DEFAULT;

	argc = getRCSINIT(argc, argv, &newargv);
	argv = newargv;
	while (a = *++argv,  0<--argc && *a++=='-') {
		switch (*a++) {
                case 'p':
                        tostdout=true;
			goto revno;

		case 'q':
			quietflag = true;
		revno:
			if (!*a)
				break;
                        /* falls into -r */
                case 'r':
			if (!rev[0])
				rev[0] = a;
			else if (!rev[1])
				rev[1] = a;
			else
				faterror("too many revision numbers");
                        break;
		case 'x':
			suffixes = a;
			break;
		case 'V':
			versionarg = *argv;
			setRCSversion(versionarg);
			break;

		case 'k':
			expandarg = *argv;
			if (0 <= str2expmode(expandarg+2))
			    break;
			/* fall into */
                default:
			faterror("unknown option: %s%s", *argv, cmdusage);
                };
        } /* end of option processing */

	if (argc<1) faterror("no input file%s", cmdusage);
	if (!rev[0]) faterror("no base revision number given");

        /* now handle all filenames */

	if (0  <  pairfilenames(argc, argv, rcsreadopen, true, false)) {

                if (argc>2 || (argc==2&&argv[1]!=nil))
                        warn("too many arguments");
		diagnose("RCS file: %s\n", RCSfilename);
		if (!(workptr = Iopen(workfilename,
			FOPEN_R_WORK,
			(struct stat*)0
		)))
			efaterror(workfilename);

                gettree();  /* reads in the delta tree */

                if (Head==nil) faterror("no revisions present");

		if (!*rev[0])
			rev[0]  =  Dbranch ? Dbranch : Head->num;
		if (!fexpandsym(rev[0], &numericrev, workptr))
			goto end;
		if (!(target=genrevs(numericrev.string, (char *)nil, (char *)nil, (char *)nil,&gendeltas))) goto end;
		rev[0] = target->num;
		if (!rev[1] || !*rev[1])
			rev[1]  =  Dbranch ? Dbranch : Head->num;
		if (!fexpandsym(rev[1], &numericrev, workptr))
			goto end;
		if (!(target=genrevs(numericrev.string, (char *)nil, (char *)nil, (char *)nil,&gendeltas))) goto end;
		rev[1] = target->num;

		if (strcmp(rev[0],rev[1]) == 0) {
			if (tostdout) {
				FILE *o;
#				if text_equals_binary_stdio || text_work_stdio
				    o = stdout;
#				else
				    if (!(o=fdopen(STDOUT_FILENO,FOPEN_W_WORK)))
					efaterror("stdout");
#				endif
				fastcopy(workptr,o);
				Ofclose(o);
			}
			goto end;
		}
		Izclose(&workptr);

		for (i=0; i<2; i++) {
			diagnose("retrieving revision %s\n", rev[i]);
			bufscpy(&commarg, "-p");
			bufscat(&commarg, rev[i]);
			if (run(
				(char*)0,
				/* Do not collide with merger.c maketemp().  */
				arg[i+1] = maketemp(i+3),
				co, quietarg, commarg.string, expandarg,
				versionarg, RCSfilename, (char*)0
			))
				faterror("co failed");
		}
		diagnose("Merging differences between %s and %s into %s%s\n",
			 rev[0], rev[1], workfilename,
                         tostdout?"; result to stdout":"");

		arg[0] = rev[0] = workfilename;
		status = merge(tostdout, rev, arg);
        }

end:
	Izclose(&workptr);
	tempunlink();
	exitmain(nerror ? DIFF_TROUBLE : status);
}

#if lint
#	define exiterr rmergeExit
#endif
	exiting void
exiterr()
{
	tempunlink();
	_exit(DIFF_TROUBLE);
}
