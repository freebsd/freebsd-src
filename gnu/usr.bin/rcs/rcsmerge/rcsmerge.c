/* Merge RCS revisions.  */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
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

/*
 * $Log: rcsmerge.c,v $
 * Revision 5.15  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.14  1995/06/01 16:23:43  eggert
 * (main): Report an error if -kb, so don't worry about binary stdout.
 * Punctuate messages properly.  Rewrite to avoid `goto end'.
 *
 * Revision 5.13  1994/03/17 14:05:48  eggert
 * Specify subprocess input via file descriptor, not file name.  Remove lint.
 *
 * Revision 5.12  1993/11/09 17:40:15  eggert
 * -V now prints version on stdout and exits.  Don't print usage twice.
 *
 * Revision 5.11  1993/11/03 17:42:27  eggert
 * Add -A, -E, -e, -z.  Ignore -T.  Allow up to three file labels.
 * Pass -Vn to `co'.  Pass unexpanded revision name to `co', so that Name works.
 *
 * Revision 5.10  1992/07/28  16:12:44  eggert
 * Add -V.
 *
 * Revision 5.9  1992/01/24  18:44:19  eggert
 * lint -> RCS_lint
 *
 * Revision 5.8  1992/01/06  02:42:34  eggert
 * Update usage string.
 *
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

mainProg(rcsmergeId, "rcsmerge", "$Id: rcsmerge.c,v 5.15 1995/06/16 06:19:24 eggert Exp $")
{
	static char const cmdusage[] =
		"\nrcsmerge usage: rcsmerge -rrev1 [-rrev2] -ksubst -{pq}[rev] -Vn -xsuff -zzone file";
	static char const quietarg[] = "-q";

	register int i;
	char *a, **newargv;
	char const *arg[3];
	char const *rev[3], *xrev[3]; /*revision numbers*/
	char const *edarg, *expandarg, *suffixarg, *versionarg, *zonearg;
        int tostdout;
	int status;
	RILE *workptr;
	struct buf commarg;
	struct buf numericrev; /* holds expanded revision number */
	struct hshentries *gendeltas; /* deltas to be generated */
        struct hshentry * target;

	bufautobegin(&commarg);
	bufautobegin(&numericrev);
	edarg = rev[1] = rev[2] = 0;
	status = 0; /* Keep lint happy.  */
	tostdout = false;
	expandarg = suffixarg = versionarg = zonearg = quietarg; /* no-op */
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
			if (!rev[1])
				rev[1] = a;
			else if (!rev[2])
				rev[2] = a;
			else
				error("too many revision numbers");
                        break;

		case 'A': case 'E': case 'e':
			if (*a)
				goto unknown;
			edarg = *argv;
			break;

		case 'x':
			suffixarg = *argv;
			suffixes = a;
			break;
		case 'z':
			zonearg = *argv;
			zone_set(a);
			break;
		case 'T':
			/* Ignore -T, so that RCSINIT can contain -T.  */
			if (*a)
				goto unknown;
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
		unknown:
			error("unknown option: %s%s", *argv, cmdusage);
                };
        } /* end of option processing */

	if (!rev[1]) faterror("no base revision number given");

	/* Now handle all pathnames.  */

	if (!nerror) {
	    if (argc < 1)
		faterror("no input file%s", cmdusage);
	    if (0 < pairnames(argc, argv, rcsreadopen, true, false)) {

                if (argc>2  ||  (argc==2 && argv[1]))
			warn("excess arguments ignored");
		if (Expand == BINARY_EXPAND)
			workerror("merging binary files");
		diagnose("RCS file: %s\n", RCSname);
		if (!(workptr = Iopen(workname, FOPEN_R_WORK, (struct stat*)0)))
			efaterror(workname);

                gettree();  /* reads in the delta tree */

		if (!Head) rcsfaterror("no revisions present");

		if (!*rev[1])
			rev[1]  =  Dbranch ? Dbranch : Head->num;
		if (fexpandsym(rev[1], &numericrev, workptr)
		    && (target=genrevs(numericrev.string, (char *)0, (char *)0, (char*)0, &gendeltas))
		) {
		  xrev[1] = target->num;
		  if (!rev[2] || !*rev[2])
			rev[2]  =  Dbranch ? Dbranch : Head->num;
		  if (fexpandsym(rev[2], &numericrev, workptr)
		      && (target=genrevs(numericrev.string, (char *)0, (char *)0, (char *)0, &gendeltas))
		  ) {
		    xrev[2] = target->num;

		    if (strcmp(xrev[1],xrev[2]) == 0) {
		      if (tostdout) {
			fastcopy(workptr, stdout);
			Ofclose(stdout);
		      }
		    } else {
		      Izclose(&workptr);

		      for (i=1; i<=2; i++) {
			diagnose("retrieving revision %s\n", xrev[i]);
			bufscpy(&commarg, "-p");
			bufscat(&commarg, rev[i]); /* not xrev[i], for $Name's sake */
			if (run(
				-1,
				/* Do not collide with merger.c maketemp().  */
				arg[i] = maketemp(i+2),
				co, quietarg, commarg.string,
				expandarg, suffixarg, versionarg, zonearg,
				RCSname, (char*)0
			))
				rcsfaterror("co failed");
		      }
		      diagnose("Merging differences between %s and %s into %s%s\n",
			       xrev[1], xrev[2], workname,
			       tostdout?"; result to stdout":"");

		      arg[0] = xrev[0] = workname;
		      status = merge(tostdout, edarg, xrev, arg);
		    }
		  }
		}

		Izclose(&workptr);
	    }
        }
	tempunlink();
	exitmain(nerror ? DIFF_TROUBLE : status);
}

#if RCS_lint
#	define exiterr rmergeExit
#endif
	void
exiterr()
{
	tempunlink();
	_exit(DIFF_TROUBLE);
}
