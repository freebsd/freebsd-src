/* Check out working files from revisions of RCS files.  */

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
 * Revision 5.18  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.17  1995/06/01 16:23:43  eggert
 * (main, preparejoin): Pass argument instead of using `join' static variable.
 * (main): Add -kb.
 *
 * Revision 5.16  1994/03/17 14:05:48  eggert
 * Move buffer-flushes out of critical sections, since they aren't critical.
 * Use ORCSerror to clean up after a fatal error.  Remove lint.
 * Specify subprocess input via file descriptor, not file name.
 *
 * Revision 5.15  1993/11/09 17:40:15  eggert
 * -V now prints version on stdout and exits.  Don't print usage twice.
 *
 * Revision 5.14  1993/11/03 17:42:27  eggert
 * Add -z.  Generate a value for the Name keyword.
 * Don't arbitrarily limit the number of joins.
 * Improve quality of diagnostics.
 *
 * Revision 5.13  1992/07/28  16:12:44  eggert
 * Add -V.  Check that working and RCS files are distinct.
 *
 * Revision 5.12  1992/02/17  23:02:08  eggert
 * Add -T.
 *
 * Revision 5.11  1992/01/24  18:44:19  eggert
 * Add support for bad_creat0.  lint -> RCS_lint
 *
 * Revision 5.10  1992/01/06  02:42:34  eggert
 * Update usage string.
 *
 * Revision 5.9  1991/10/07  17:32:46  eggert
 * -k affects just working file, not RCS file.
 *
 * Revision 5.8  1991/08/19  03:13:55  eggert
 * Warn before removing somebody else's file.
 * Add -M.  Fix co -j bugs.  Tune.
 *
 * Revision 5.7  1991/04/21  11:58:15  eggert
 * Ensure that working file is newer than RCS file after co -[lu].
 * Add -x, RCSINIT, MS-DOS support.
 *
 * Revision 5.6  1990/12/04  05:18:38  eggert
 * Don't checkaccesslist() unless necessary.
 * Use -I for prompts and -q for diagnostics.
 *
 * Revision 5.5  1990/11/01  05:03:26  eggert
 * Fix -j.  Add -I.
 *
 * Revision 5.4  1990/10/04  06:30:11  eggert
 * Accumulate exit status across files.
 *
 * Revision 5.3  1990/09/11  02:41:09  eggert
 * co -kv yields a readonly working file.
 *
 * Revision 5.2  1990/09/04  08:02:13  eggert
 * Standardize yes-or-no procedure.
 *
 * Revision 5.0  1990/08/22  08:10:02  eggert
 * Permit multiple locks by same user.  Add setuid support.
 * Remove compile-time limits; use malloc instead.
 * Permit dates past 1999/12/31.  Switch to GMT.
 * Make lock and temp files faster and safer.
 * Ansify and Posixate.  Add -k, -V.  Remove snooping.  Tune.
 *
 * Revision 4.7  89/05/01  15:11:41  narten
 * changed copyright header to reflect current distribution rules
 *
 * Revision 4.6  88/08/09  19:12:15  eggert
 * Fix "co -d" core dump; rawdate wasn't always initialized.
 * Use execv(), not system(); fix putchar('\0') and diagnose() botches; remove lint
 *
 * Revision 4.5  87/12/18  11:35:40  narten
 * lint cleanups (from Guy Harris)
 *
 * Revision 4.4  87/10/18  10:20:53  narten
 * Updating version numbers changes relative to 1.1, are actually
 * relative to 4.2
 *
 * Revision 1.3  87/09/24  13:58:30  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf
 * warnings)
 *
 * Revision 1.2  87/03/27  14:21:38  jenkins
 * Port to suns
 *
 * Revision 4.2  83/12/05  13:39:48  wft
 * made rewriteflag external.
 *
 * Revision 4.1  83/05/10  16:52:55  wft
 * Added option -u and -f.
 * Added handling of default branch.
 * Replaced getpwuid() with getcaller().
 * Removed calls to stat(); now done by pairfilenames().
 * Changed and renamed rmoldfile() to rmworkfile().
 * Replaced catchints() calls with restoreints(), unlink()--link() with rename();
 *
 * Revision 3.7  83/02/15  15:27:07  wft
 * Added call to fastcopy() to copy remainder of RCS file.
 *
 * Revision 3.6  83/01/15  14:37:50  wft
 * Added ignoring of interrupts while RCS file is renamed; this avoids
 * deletion of RCS files during the unlink/link window.
 *
 * Revision 3.5  82/12/08  21:40:11  wft
 * changed processing of -d to use DATEFORM; removed actual from
 * call to preparejoin; re-fixed printing of done at the end.
 *
 * Revision 3.4  82/12/04  18:40:00  wft
 * Replaced getdelta() with gettree(), SNOOPDIR with SNOOPFILE.
 * Fixed printing of "done".
 *
 * Revision 3.3  82/11/28  22:23:11  wft
 * Replaced getlogin() with getpwuid(), flcose() with ffclose(),
 * %02d with %.2d, mode generation for working file with WORKMODE.
 * Fixed nil printing. Fixed -j combined with -l and -p, and exit
 * for non-existing revisions in preparejoin().
 *
 * Revision 3.2  82/10/18  20:47:21  wft
 * Mode of working file is now maintained even for co -l, but write permission
 * is removed.
 * The working file inherits its mode from the RCS file, plus write permission
 * for the owner. The write permission is not given if locking is strict and
 * co does not lock.
 * An existing working file without write permission is deleted automatically.
 * Otherwise, co asks (empty answer: abort co).
 * Call to getfullRCSname() added, check for write error added, call
 * for getlogin() fixed.
 *
 * Revision 3.1  82/10/13  16:01:30  wft
 * fixed type of variables receiving from getc() (char -> int).
 * removed unused variables.
 */




#include "rcsbase.h"

static char *addjoin P((char*));
static char const *getancestor P((char const*,char const*));
static int buildjoin P((char const*));
static int preparejoin P((char*));
static int rmlock P((struct hshentry const*));
static int rmworkfile P((void));
static void cleanup P((void));

static char const quietarg[] = "-q";

static char const *expandarg, *suffixarg, *versionarg, *zonearg, *incexcarg;
static char const **joinlist; /* revisions to be joined */
static int joinlength;
static FILE *neworkptr;
static int exitstatus;
static int forceflag;
static int lastjoin;			/* index of last element in joinlist  */
static int lockflag; /* -1 -> unlock, 0 -> do nothing, 1 -> lock */
static int mtimeflag;
static struct hshentries *gendeltas;	/* deltas to be generated	*/
static struct hshentry *targetdelta;	/* final delta to be generated	*/
static struct stat workstat;

mainProg(coId, "co", "$FreeBSD$")
{
	static char const cmdusage[] =
		"\nco usage: co -{fIlMpqru}[rev] -ddate -jjoins -ksubst -sstate -T -w[who] -Vn -xsuff -zzone file ...";

	char *a, *joinflag, **newargv;
	char const *author, *date, *rev, *state;
	char const *joinname, *newdate, *neworkname;
	int changelock;  /* 1 if a lock has been changed, -1 if error */
	int expmode, r, tostdout, workstatstat;
	int Ttimeflag;
	struct buf numericrev;	/* expanded revision number	*/
	char finaldate[datesize];
#	if OPEN_O_BINARY
		int stdout_mode = 0;
#	endif

	setrid();
	author = date = rev = state = 0;
	joinflag = 0;
	bufautobegin(&numericrev);
	expmode = -1;
	suffixes = X_DEFAULT;
	tostdout = false;
	Ttimeflag = false;

	argc = getRCSINIT(argc, argv, &newargv);
	argv = newargv;
	while (a = *++argv,  0<--argc && *a++=='-') {
		switch (*a++) {

                case 'r':
		revno:
			if (*a) {
				if (rev) warn("redefinition of revision number");
				rev = a;
                        }
                        break;

		case 'f':
			forceflag=true;
			goto revno;

                case 'l':
			if (lockflag < 0) {
				warn("-u overridden by -l.");
                        }
			lockflag = 1;
                        goto revno;

                case 'u':
			if (0 < lockflag) {
				warn("-l overridden by -u.");
                        }
			lockflag = -1;
                        goto revno;

                case 'p':
			tostdout = true;
                        goto revno;

		case 'I':
			interactiveflag = true;
			goto revno;

                case 'q':
                        quietflag=true;
                        goto revno;

                case 'd':
			if (date)
				redefined('d');
			str2date(a, finaldate);
                        date=finaldate;
                        break;

                case 'j':
			if (*a) {
				if (joinflag) redefined('j');
				joinflag = a;
                        }
                        break;

		case 'M':
			mtimeflag = true;
			goto revno;

                case 's':
			if (*a) {
				if (state) redefined('s');
				state = a;
                        }
                        break;

		case 'T':
			if (*a)
				goto unknown;
			Ttimeflag = true;
			break;

                case 'w':
			if (author) redefined('w');
			if (*a)
				author = a;
			else
				author = getcaller();
                        break;

		case 'x':
			suffixarg = *argv;
			suffixes = a;
			break;

		case 'V':
			versionarg = *argv;
			setRCSversion(versionarg);
			break;

		case 'z':
			zonearg = *argv;
			zone_set(a);
			break;

		case 'K':    /*  set keyword inclusions/exclusions  */
			incexcarg = *argv;
			setIncExc(incexcarg);
			break;

		case 'k':    /*  set keyword expand mode  */
			expandarg = *argv;
			if (0 <= expmode) redefined('k');
			if (0 <= (expmode = str2expmode(a)))
			    break;
			/* fall into */
                default:
		unknown:
			error("unknown option: %s%s", *argv, cmdusage);

                };
        } /* end of option processing */

	/* Now handle all pathnames.  */
	if (nerror) cleanup();
	else if (argc < 1) faterror("no input file%s", cmdusage);
	else for (;  0 < argc;  cleanup(), ++argv, --argc) {
	ffree();

	if (pairnames(argc, argv, lockflag?rcswriteopen:rcsreadopen, true, false)  <=  0)
		continue;

	/*
	 * RCSname contains the name of the RCS file, and finptr
	 * points at it.  workname contains the name of the working file.
	 * Also, RCSstat has been set.
         */
	diagnose("%s  -->  %s\n", RCSname, tostdout?"standard output":workname);

	workstatstat = -1;
	if (tostdout) {
#		if OPEN_O_BINARY
		    int newmode = Expand==BINARY_EXPAND ? OPEN_O_BINARY : 0;
		    if (stdout_mode != newmode) {
			stdout_mode = newmode;
			oflush();
			VOID setmode(STDOUT_FILENO, newmode);
		    }
#		endif
		neworkname = 0;
		neworkptr = workstdout = stdout;
	} else {
		workstatstat = stat(workname, &workstat);
		if (workstatstat == 0  &&  same_file(RCSstat, workstat, 0)) {
			rcserror("RCS file is the same as working file %s.",
				workname
			);
			continue;
		}
		neworkname = makedirtemp(1);
		if (!(neworkptr = fopenSafer(neworkname, FOPEN_W_WORK))) {
			if (errno == EACCES)
			    workerror("permission denied on parent directory");
			else
			    eerror(neworkname);
			continue;
		}
	}

        gettree();  /* reads in the delta tree */

	if (!Head) {
                /* no revisions; create empty file */
		diagnose("no revisions present; generating empty revision 0.0\n");
		if (lockflag)
			warn(
				"no revisions, so nothing can be %slocked",
				lockflag < 0 ? "un" : ""
			);
		Ozclose(&fcopy);
		if (workstatstat == 0)
			if (!rmworkfile()) continue;
		changelock = 0;
		newdate = 0;
        } else {
		int locks = lockflag ? findlock(false, &targetdelta) : 0;
		if (rev) {
                        /* expand symbolic revision number */
			if (!expandsym(rev, &numericrev))
                                continue;
		} else {
			switch (locks) {
			    default:
				continue;
			    case 0:
				bufscpy(&numericrev, Dbranch?Dbranch:"");
				break;
			    case 1:
				bufscpy(&numericrev, targetdelta->num);
				break;
			}
		}
                /* get numbers of deltas to be generated */
		if (!(targetdelta=genrevs(numericrev.string,date,author,state,&gendeltas)))
                        continue;
                /* check reservations */
		changelock =
			lockflag < 0 ?
				rmlock(targetdelta)
			: lockflag == 0 ?
				0
			:
				addlock(targetdelta, true);

		if (
			changelock < 0
			|| (changelock && !checkaccesslist())
			|| dorewrite(lockflag, changelock) != 0
		)
			continue;

		if (0 <= expmode)
			Expand = expmode;
		if (0 < lockflag  &&  Expand == VAL_EXPAND) {
			rcserror("cannot combine -kv and -l");
			continue;
		}

		if (joinflag && !preparejoin(joinflag))
			continue;

		diagnose("revision %s%s\n",targetdelta->num,
			 0<lockflag ? " (locked)" :
			 lockflag<0 ? " (unlocked)" : "");

		/* Prepare to remove old working file if necessary.  */
		if (workstatstat == 0)
                        if (!rmworkfile()) continue;

                /* skip description */
                getdesc(false); /* don't echo*/

		locker_expansion = 0 < lockflag;
		targetdelta->name = namedrev(rev, targetdelta);
		joinname = buildrevision(
			gendeltas, targetdelta,
			joinflag&&tostdout ? (FILE*)0 : neworkptr,
			Expand < MIN_UNEXPAND
		);
#		if !large_memory
			if (fcopy == neworkptr)
				fcopy = 0;  /* Don't close it twice.  */
#		endif
		if_advise_access(changelock && gendeltas->first!=targetdelta,
			finptr, MADV_SEQUENTIAL
		);

		if (donerewrite(changelock,
			Ttimeflag ? RCSstat.st_mtime : (time_t)-1
		) != 0)
			continue;

		if (changelock) {
			locks += lockflag;
			if (1 < locks)
				rcswarn("You now have %d locks.", locks);
		}

		newdate = targetdelta->date;
		if (joinflag) {
			newdate = 0;
			if (!joinname) {
				aflush(neworkptr);
				joinname = neworkname;
			}
			if (Expand == BINARY_EXPAND)
				workerror("merging binary files");
			if (!buildjoin(joinname))
				continue;
		}
        }
	if (!tostdout) {
	    mode_t m = WORKMODE(RCSstat.st_mode,
		!  (Expand==VAL_EXPAND  ||  (lockflag<=0 && StrictLocks))
	    );
	    time_t t = mtimeflag&&newdate ? date2time(newdate) : (time_t)-1;
	    aflush(neworkptr);
	    ignoreints();
	    r = chnamemod(&neworkptr, neworkname, workname, 1, m, t);
	    keepdirtemp(neworkname);
	    restoreints();
	    if (r != 0) {
		eerror(workname);
		error("see %s", neworkname);
		continue;
	    }
	    diagnose("done\n");
	}
	}

	tempunlink();
	Ofclose(workstdout);
	exitmain(exitstatus);

}       /* end of main (co) */

	static void
cleanup()
{
	if (nerror) exitstatus = EXIT_FAILURE;
	Izclose(&finptr);
	ORCSclose();
#	if !large_memory
		if (fcopy!=workstdout) Ozclose(&fcopy);
#	endif
	if (neworkptr!=workstdout) Ozclose(&neworkptr);
	dirtempunlink();
}

#if RCS_lint
#	define exiterr coExit
#endif
	void
exiterr()
{
	ORCSerror();
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}


/*****************************************************************
 * The following routines are auxiliary routines
 *****************************************************************/

	static int
rmworkfile()
/*
 * Prepare to remove workname, if it exists, and if
 * it is read-only.
 * Otherwise (file writable):
 *   if !quietmode asks the user whether to really delete it (default: fail);
 *   otherwise failure.
 * Returns true if permission is gotten.
 */
{
	if (workstat.st_mode&(S_IWUSR|S_IWGRP|S_IWOTH) && !forceflag) {
	    /* File is writable */
	    if (!yesorno(false, "writable %s exists%s; remove it? [ny](n): ",
			workname,
			myself(workstat.st_uid) ? "" : ", and you do not own it"
	    )) {
		error(!quietflag && ttystdin()
			? "checkout aborted"
			: "writable %s exists; checkout aborted", workname);
		return false;
            }
        }
	/* Actual unlink is done later by caller. */
	return true;
}


	static int
rmlock(delta)
	struct hshentry const *delta;
/* Function: removes the lock held by caller on delta.
 * Returns -1 if someone else holds the lock,
 * 0 if there is no lock on delta,
 * and 1 if a lock was found and removed.
 */
{       register struct rcslock * next, * trail;
	char const *num;
	struct rcslock dummy;
        int whomatch, nummatch;

        num=delta->num;
        dummy.nextlock=next=Locks;
        trail = &dummy;
	while (next) {
		whomatch = strcmp(getcaller(), next->login);
                nummatch=strcmp(num,next->delta->num);
                if ((whomatch==0) && (nummatch==0)) break;
			/*found a lock on delta by caller*/
                if ((whomatch!=0)&&(nummatch==0)) {
                    rcserror("revision %s locked by %s; use co -r or rcs -u",
			num, next->login
		    );
                    return -1;
                }
                trail=next;
                next=next->nextlock;
        }
	if (next) {
                /*found one; delete it */
                trail->nextlock=next->nextlock;
                Locks=dummy.nextlock;
		next->delta->lockedby = 0;
                return 1; /*success*/
        } else  return 0; /*no lock on delta*/
}




/*****************************************************************
 * The rest of the routines are for handling joins
 *****************************************************************/


	static char *
addjoin(joinrev)
	char *joinrev;
/* Add joinrev's number to joinlist, yielding address of char past joinrev,
 * or 0 if no such revision exists.
 */
{
	register char *j;
	register struct hshentry *d;
	char terminator;
	struct buf numrev;
	struct hshentries *joindeltas;

	j = joinrev;
	for (;;) {
	    switch (*j++) {
		default:
		    continue;
		case 0:
		case ' ': case '\t': case '\n':
		case ':': case ',': case ';':
		    break;
	    }
	    break;
	}
	terminator = *--j;
	*j = 0;
	bufautobegin(&numrev);
	d = 0;
	if (expandsym(joinrev, &numrev))
	    d = genrevs(numrev.string,(char*)0,(char*)0,(char*)0,&joindeltas);
	bufautoend(&numrev);
	*j = terminator;
	if (d) {
		joinlist[++lastjoin] = d->num;
		return j;
	}
	return 0;
}

	static int
preparejoin(j)
	register char *j;
/* Parse join list J and place pointers to the
 * revision numbers into joinlist.
 */
{
        lastjoin= -1;
        for (;;) {
                while ((*j==' ')||(*j=='\t')||(*j==',')) j++;
                if (*j=='\0') break;
                if (lastjoin>=joinlength-2) {
		    joinlist =
			(joinlength *= 2) == 0
			? tnalloc(char const *, joinlength = 16)
			: trealloc(char const *, joinlist, joinlength);
                }
		if (!(j = addjoin(j))) return false;
                while ((*j==' ') || (*j=='\t')) j++;
                if (*j == ':') {
                        j++;
                        while((*j==' ') || (*j=='\t')) j++;
                        if (*j!='\0') {
				if (!(j = addjoin(j))) return false;
                        } else {
				rcsfaterror("join pair incomplete");
                        }
                } else {
                        if (lastjoin==0) { /* first pair */
                                /* common ancestor missing */
                                joinlist[1]=joinlist[0];
                                lastjoin=1;
                                /*derive common ancestor*/
				if (!(joinlist[0] = getancestor(targetdelta->num,joinlist[1])))
                                       return false;
                        } else {
				rcsfaterror("join pair incomplete");
                        }
                }
        }
	if (lastjoin < 1)
		rcsfaterror("empty join");
	return true;
}



	static char const *
getancestor(r1, r2)
	char const *r1, *r2;
/* Yield the common ancestor of r1 and r2 if successful, 0 otherwise.
 * Work reliably only if r1 and r2 are not branch numbers.
 */
{
	static struct buf t1, t2;

	int l1, l2, l3;
	char const *r;

	l1 = countnumflds(r1);
	l2 = countnumflds(r2);
	if ((2<l1 || 2<l2)  &&  cmpnum(r1,r2)!=0) {
	    /* not on main trunk or identical */
	    l3 = 0;
	    while (cmpnumfld(r1, r2, l3+1)==0 && cmpnumfld(r1, r2, l3+2)==0)
		l3 += 2;
	    /* This will terminate since r1 and r2 are not the same; see above. */
	    if (l3==0) {
		/* no common prefix; common ancestor on main trunk */
		VOID partialno(&t1, r1, l1>2 ? 2 : l1);
		VOID partialno(&t2, r2, l2>2 ? 2 : l2);
		r = cmpnum(t1.string,t2.string)<0 ? t1.string : t2.string;
		if (cmpnum(r,r1)!=0 && cmpnum(r,r2)!=0)
			return r;
	    } else if (cmpnumfld(r1, r2, l3+1)!=0)
			return partialno(&t1,r1,l3);
	}
	rcserror("common ancestor of %s and %s undefined", r1, r2);
	return 0;
}



	static int
buildjoin(initialfile)
	char const *initialfile;
/* Function: merge pairs of elements in joinlist into initialfile
 * If workstdout is set, copy result to stdout.
 * All unlinking of initialfile, rev2, and rev3 should be done by tempunlink().
 */
{
	struct buf commarg;
	struct buf subs;
	char const *rev2, *rev3;
        int i;
	char const *cov[10], *mergev[11];
	char const **p;

	bufautobegin(&commarg);
	bufautobegin(&subs);
	rev2 = maketemp(0);
	rev3 = maketemp(3); /* buildrevision() may use 1 and 2 */

	cov[1] = CO;
	/* cov[2] setup below */
	p = &cov[3];
	if (expandarg) *p++ = expandarg;
	if (suffixarg) *p++ = suffixarg;
	if (versionarg) *p++ = versionarg;
	if (zonearg) *p++ = zonearg;
	*p++ = quietarg;
	*p++ = RCSname;
	*p = 0;

	mergev[1] = MERGE;
	mergev[2] = mergev[4] = "-L";
	/* rest of mergev setup below */

        i=0;
        while (i<lastjoin) {
                /*prepare marker for merge*/
                if (i==0)
			bufscpy(&subs, targetdelta->num);
		else {
			bufscat(&subs, ",");
			bufscat(&subs, joinlist[i-2]);
			bufscat(&subs, ":");
			bufscat(&subs, joinlist[i-1]);
		}
		diagnose("revision %s\n",joinlist[i]);
		bufscpy(&commarg, "-p");
		bufscat(&commarg, joinlist[i]);
		cov[2] = commarg.string;
		if (runv(-1, rev2, cov))
			goto badmerge;
		diagnose("revision %s\n",joinlist[i+1]);
		bufscpy(&commarg, "-p");
		bufscat(&commarg, joinlist[i+1]);
		cov[2] = commarg.string;
		if (runv(-1, rev3, cov))
			goto badmerge;
		diagnose("merging...\n");
		mergev[3] = subs.string;
		mergev[5] = joinlist[i+1];
		p = &mergev[6];
		if (quietflag) *p++ = quietarg;
		if (lastjoin<=i+2 && workstdout) *p++ = "-p";
		*p++ = initialfile;
		*p++ = rev2;
		*p++ = rev3;
		*p = 0;
		switch (runv(-1, (char*)0, mergev)) {
		    case DIFF_FAILURE: case DIFF_SUCCESS:
			break;
		    default:
			goto badmerge;
		}
                i=i+2;
        }
	bufautoend(&commarg);
	bufautoend(&subs);
        return true;

    badmerge:
	nerror++;
	bufautoend(&commarg);
	bufautoend(&subs);
	return false;
}
