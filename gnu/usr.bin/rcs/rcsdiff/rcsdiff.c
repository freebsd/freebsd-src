/* Compare RCS revisions.  */

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
 * Revision 5.19  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.18  1995/06/01 16:23:43  eggert
 * (main): Pass "--binary" if -kb and if --binary makes a difference.
 * Don't treat + options specially.
 *
 * Revision 5.17  1994/03/17 14:05:48  eggert
 * Specify subprocess input via file descriptor, not file name.  Remove lint.
 *
 * Revision 5.16  1993/11/09 17:40:15  eggert
 * -V now prints version on stdout and exits.  Don't print usage twice.
 *
 * Revision 5.15  1993/11/03 17:42:27  eggert
 * Add -z.  Ignore -T.  Pass -Vn to `co'.  Add Name keyword.
 * Put revision numbers in -c output.  Improve quality of diagnostics.
 *
 * Revision 5.14  1992/07/28  16:12:44  eggert
 * Add -V.  Use co -M for better dates with traditional diff -c.
 *
 * Revision 5.13  1992/02/17  23:02:23  eggert
 * Output more readable context diff headers.
 * Suppress needless checkout and comparison of identical revisions.
 *
 * Revision 5.12  1992/01/24  18:44:19  eggert
 * Add GNU diff 1.15.2's new options.  lint -> RCS_lint
 *
 * Revision 5.11  1992/01/06  02:42:34  eggert
 * Update usage string.
 *
 * Revision 5.10  1991/10/07  17:32:46  eggert
 * Remove lint.
 *
 * Revision 5.9  1991/08/19  03:13:55  eggert
 * Add RCSINIT, -r$.  Tune.
 *
 * Revision 5.8  1991/04/21  11:58:21  eggert
 * Add -x, RCSINIT, MS-DOS support.
 *
 * Revision 5.7  1990/12/13  06:54:07  eggert
 * GNU diff 1.15 has -u.
 *
 * Revision 5.6  1990/11/01  05:03:39  eggert
 * Remove unneeded setid check.
 *
 * Revision 5.5  1990/10/04  06:30:19  eggert
 * Accumulate exit status across files.
 *
 * Revision 5.4  1990/09/27  01:31:43  eggert
 * Yield 1, not EXIT_FAILURE, when diffs are found.
 *
 * Revision 5.3  1990/09/11  02:41:11  eggert
 * Simplify -kkvl test.
 *
 * Revision 5.2  1990/09/04  17:07:19  eggert
 * Diff's argv was too small by 1.
 *
 * Revision 5.1  1990/08/29  07:13:55  eggert
 * Add -kkvl.
 *
 * Revision 5.0  1990/08/22  08:12:46  eggert
 * Add -k, -V.  Don't use access().  Add setuid support.
 * Remove compile-time limits; use malloc instead.
 * Don't pass arguments with leading '+' to diff; GNU DIFF treats them as options.
 * Add GNU diff's flags.  Make lock and temp files faster and safer.
 * Ansify and Posixate.
 *
 * Revision 4.6  89/05/01  15:12:27  narten
 * changed copyright header to reflect current distribution rules
 *
 * Revision 4.5  88/08/09  19:12:41  eggert
 * Use execv(), not system(); yield exit status like diff(1)s; allow cc -R.
 *
 * Revision 4.4  87/12/18  11:37:46  narten
 * changes Jay Lepreau made in the 4.3 BSD version, to add support for
 * "-i", "-w", and "-t" flags and to permit flags to be bundled together,
 * merged in.
 *
 * Revision 4.3  87/10/18  10:31:42  narten
 * Updating version numbers. Changes relative to 1.1 actually
 * relative to 4.1
 *
 * Revision 1.3  87/09/24  13:59:21  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf
 * warnings)
 *
 * Revision 1.2  87/03/27  14:22:15  jenkins
 * Port to suns
 *
 * Revision 4.1  83/05/03  22:13:19  wft
 * Added default branch, option -q, exit status like diff.
 * Added fterror() to replace faterror().
 *
 * Revision 3.6  83/01/15  17:52:40  wft
 * Expanded mainprogram to handle multiple RCS files.
 *
 * Revision 3.5  83/01/06  09:33:45  wft
 * Fixed passing of -c (context) option to diff.
 *
 * Revision 3.4  82/12/24  15:28:38  wft
 * Added call to catchsig().
 *
 * Revision 3.3  82/12/10  16:08:17  wft
 * Corrected checking of return code from diff; improved error msgs.
 *
 * Revision 3.2  82/12/04  13:20:09  wft
 * replaced getdelta() with gettree(). Changed diagnostics.
 *
 * Revision 3.1  82/11/28  19:25:04  wft
 * Initial revision.
 *
 */
#include "rcsbase.h"

#if DIFF_L
static char const *setup_label P((struct buf*,char const*,char const[datesize]));
#endif
static void cleanup P((void));

static int exitstatus;
static RILE *workptr;
static struct stat workstat;

mainProg(rcsdiffId, "rcsdiff", "$Id: rcsdiff.c,v 1.4 1995/10/29 22:06:42 peter Exp $")
{
    static char const cmdusage[] =
	    "\nrcsdiff usage: rcsdiff -ksubst -q -rrev1 [-rrev2] -Vn -xsuff -zzone [diff options] file ...";

    int  revnums;                 /* counter for revision numbers given */
    char const *rev1, *rev2;	/* revision numbers from command line */
    char const *xrev1, *xrev2;	/* expanded revision numbers */
    char const *expandarg, *lexpandarg, *suffixarg, *versionarg, *zonearg;
#if DIFF_L
    static struct buf labelbuf[2];
    int file_labels;
    char const **diff_label1, **diff_label2;
    char date2[datesize];
#endif
    char const *cov[10 + !DIFF_L];
    char const **diffv, **diffp, **diffpend;	/* argv for subsidiary diff */
    char const **pp, *p, *diffvstr;
    struct buf commarg;
    struct buf numericrev;	/* expanded revision number */
    struct hshentries *gendeltas;	/* deltas to be generated */
    struct hshentry * target;
    char *a, *dcp, **newargv;
    int no_diff_means_no_output;
    register c;

    exitstatus = DIFF_SUCCESS;

    bufautobegin(&commarg);
    bufautobegin(&numericrev);
    revnums = 0;
    rev1 = rev2 = xrev2 = 0;
#if DIFF_L
    file_labels = 0;
#endif
    expandarg = suffixarg = versionarg = zonearg = 0;
    no_diff_means_no_output = true;
    suffixes = X_DEFAULT;

    /*
    * Room for runv extra + args [+ --binary] [+ 2 labels]
    * + 1 file + 1 trailing null.
    */
    diffv = tnalloc(char const*, 1 + argc + !!OPEN_O_BINARY + 2*DIFF_L + 2);
    diffp = diffv + 1;
    *diffp++ = DIFF;

    argc = getRCSINIT(argc, argv, &newargv);
    argv = newargv;
    while (a = *++argv,  0<--argc && *a++=='-') {
	dcp = a;
	while ((c = *a++)) switch (c) {
	    case 'r':
		    switch (++revnums) {
			case 1: rev1=a; break;
			case 2: rev2=a; break;
			default: error("too many revision numbers");
		    }
		    goto option_handled;
	    case '-': case 'D':
		    no_diff_means_no_output = false;
		    /* fall into */
	    case 'C': case 'F': case 'I': case 'L': case 'W':
#if DIFF_L
		    if (c == 'L'  &&  file_labels++ == 2)
			faterror("too many -L options");
#endif
		    *dcp++ = c;
		    if (*a)
			do *dcp++ = *a++;
			while (*a);
		    else {
			if (!--argc)
			    faterror("-%c needs following argument%s",
				    c, cmdusage
			    );
			*diffp++ = *argv++;
		    }
		    break;
	    case 'y':
		    no_diff_means_no_output = false;
		    /* fall into */
	    case 'B': case 'H':
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    case 'h': case 'i': case 'n': case 'p':
	    case 't': case 'u': case 'w':
		    *dcp++ = c;
		    break;
	    case 'q':
		    quietflag=true;
		    break;
	    case 'x':
		    suffixarg = *argv;
		    suffixes = *argv + 2;
		    goto option_handled;
	    case 'z':
		    zonearg = *argv;
		    zone_set(*argv + 2);
		    goto option_handled;
	    case 'T':
		    /* Ignore -T, so that RCSINIT can contain -T.  */
		    if (*a)
			    goto unknown;
		    break;
	    case 'V':
		    versionarg = *argv;
		    setRCSversion(versionarg);
		    goto option_handled;
	    case 'k':
		    expandarg = *argv;
		    if (0 <= str2expmode(expandarg+2))
			goto option_handled;
		    /* fall into */
	    default:
	    unknown:
		    error("unknown option: %s%s", *argv, cmdusage);
	    };
      option_handled:
	if (dcp != *argv+1) {
	    *dcp = 0;
	    *diffp++ = *argv;
	}
    } /* end of option processing */

    for (pp = diffv+2, c = 0;  pp<diffp;  )
	    c += strlen(*pp++) + 1;
    diffvstr = a = tnalloc(char, c + 1);
    for (pp = diffv+2;  pp<diffp;  ) {
	    p = *pp++;
	    *a++ = ' ';
	    while ((*a = *p++))
		    a++;
    }
    *a = 0;

#if DIFF_L
    diff_label1 = diff_label2 = 0;
    if (file_labels < 2) {
	    if (!file_labels)
		    diff_label1 = diffp++;
	    diff_label2 = diffp++;
    }
#endif
    diffpend = diffp;

    cov[1] = CO;
    cov[2] = "-q";
#   if !DIFF_L
	cov[3] = "-M";
#   endif

    /* Now handle all pathnames.  */
    if (nerror)
	cleanup();
    else if (argc < 1)
	faterror("no input file%s", cmdusage);
    else
	for (;  0 < argc;  cleanup(), ++argv, --argc) {
	    ffree();

	    if (pairnames(argc, argv, rcsreadopen, true, false)  <=  0)
		    continue;
	    diagnose("===================================================================\nRCS file: %s\n",RCSname);
	    if (!rev2) {
		/* Make sure work file is readable, and get its status.  */
		if (!(workptr = Iopen(workname, FOPEN_R_WORK, &workstat))) {
		    eerror(workname);
		    continue;
		}
	    }


	    gettree(); /* reads in the delta tree */

	    if (!Head) {
		    rcserror("no revisions present");
		    continue;
	    }
	    if (revnums==0  ||  !*rev1)
		    rev1  =  Dbranch ? Dbranch : Head->num;

	    if (!fexpandsym(rev1, &numericrev, workptr)) continue;
	    if (!(target=genrevs(numericrev.string,(char *)0,(char *)0,(char *)0,&gendeltas))) continue;
	    xrev1=target->num;
#if DIFF_L
	    if (diff_label1)
		*diff_label1 = setup_label(&labelbuf[0], target->num, target->date);
#endif

	    lexpandarg = expandarg;
	    if (revnums==2) {
		    if (!fexpandsym(
			    *rev2 ? rev2  : Dbranch ? Dbranch  : Head->num,
			    &numericrev,
			    workptr
		    ))
			continue;
		    if (!(target=genrevs(numericrev.string,(char *)0,(char *)0,(char *)0,&gendeltas))) continue;
		    xrev2=target->num;
		    if (no_diff_means_no_output  &&  xrev1 == xrev2)
			continue;
	    } else if (
			target->lockedby
		&&	!lexpandarg
		&&	Expand == KEYVAL_EXPAND
		&&	WORKMODE(RCSstat.st_mode,true) == workstat.st_mode
	    )
		    lexpandarg = "-kkvl";
	    Izclose(&workptr);
#if DIFF_L
	    if (diff_label2)
		if (revnums == 2)
		    *diff_label2 = setup_label(&labelbuf[1], target->num, target->date);
		else {
		    time2date(workstat.st_mtime, date2);
		    *diff_label2 = setup_label(&labelbuf[1], (char*)0, date2);
		}
#endif

	    diagnose("retrieving revision %s\n", xrev1);
	    bufscpy(&commarg, "-p");
	    bufscat(&commarg, rev1); /* not xrev1, for $Name's sake */

	    pp = &cov[3 + !DIFF_L];
	    *pp++ = commarg.string;
	    if (lexpandarg) *pp++ = lexpandarg;
	    if (suffixarg) *pp++ = suffixarg;
	    if (versionarg) *pp++ = versionarg;
	    if (zonearg) *pp++ = zonearg;
	    *pp++ = RCSname;
	    *pp = 0;

	    diffp = diffpend;
#	    if OPEN_O_BINARY
		    if (Expand == BINARY_EXPAND)
			    *diffp++ = "--binary";
#	    endif
	    diffp[0] = maketemp(0);
	    if (runv(-1, diffp[0], cov)) {
		    rcserror("co failed");
		    continue;
	    }
	    if (!rev2) {
		    diffp[1] = workname;
		    if (*workname == '-') {
			char *dp = ftnalloc(char, strlen(workname)+3);
			diffp[1] = dp;
			*dp++ = '.';
			*dp++ = SLASH;
			VOID strcpy(dp, workname);
		    }
	    } else {
		    diagnose("retrieving revision %s\n",xrev2);
		    bufscpy(&commarg, "-p");
		    bufscat(&commarg, rev2); /* not xrev2, for $Name's sake */
		    cov[3 + !DIFF_L] = commarg.string;
		    diffp[1] = maketemp(1);
		    if (runv(-1, diffp[1], cov)) {
			    rcserror("co failed");
			    continue;
		    }
	    }
	    if (!rev2)
		    diagnose("diff%s -r%s %s\n", diffvstr, xrev1, workname);
	    else
		    diagnose("diff%s -r%s -r%s\n", diffvstr, xrev1, xrev2);

	    diffp[2] = 0;
	    switch (runv(-1, (char*)0, diffv)) {
		    case DIFF_SUCCESS:
			    break;
		    case DIFF_FAILURE:
			    if (exitstatus == DIFF_SUCCESS)
				    exitstatus = DIFF_FAILURE;
			    break;
		    default:
			    workerror("diff failed");
	    }
	}

    tempunlink();
    exitmain(exitstatus);
}

    static void
cleanup()
{
    if (nerror) exitstatus = DIFF_TROUBLE;
    Izclose(&finptr);
    Izclose(&workptr);
}

#if RCS_lint
#	define exiterr rdiffExit
#endif
    void
exiterr()
{
    tempunlink();
    _exit(DIFF_TROUBLE);
}

#if DIFF_L
	static char const *
setup_label(b, num, date)
	struct buf *b;
	char const *num;
	char const date[datesize];
{
	char *p;
	char datestr[datesize + zonelenmax];
	VOID date2str(date, datestr);
	bufalloc(b,
		strlen(workname)
		+ sizeof datestr + 4
		+ (num ? strlen(num) : 0)
	);
	p = b->string;
	if (num)
		VOID sprintf(p, "-L%s\t%s\t%s", workname, datestr, num);
	else
		VOID sprintf(p, "-L%s\t%s", workname, datestr);
	return p;
}
#endif
