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

/*
 *                     RCS checkin operation
 */
/*******************************************************************
 *                       check revisions into RCS files
 *******************************************************************
 */



/* $Log: ci.c,v $
 * Revision 5.21  1991/11/20  17:58:07  eggert
 * Don't read the delta tree from a nonexistent RCS file.
 *
 * Revision 5.20  1991/10/07  17:32:46  eggert
 * Fix log bugs.  Remove lint.
 *
 * Revision 5.19  1991/09/26  23:10:30  eggert
 * Plug file descriptor leak.
 *
 * Revision 5.18  1991/09/18  07:29:10  eggert
 * Work around a common ftruncate() bug.
 *
 * Revision 5.17  1991/09/10  22:15:46  eggert
 * Fix test for redirected stdin.
 *
 * Revision 5.16  1991/08/19  23:17:54  eggert
 * When there are no changes, revert to previous revision instead of aborting.
 * Add piece tables, -M, -r$.  Tune.
 *
 * Revision 5.15  1991/04/21  11:58:14  eggert
 * Ensure that working file is newer than RCS file after ci -[lu].
 * Add -x, RCSINIT, MS-DOS support.
 *
 * Revision 5.14  1991/02/28  19:18:47  eggert
 * Don't let a setuid ci create a new RCS file; rcs -i -a must be run first.
 * Fix ci -ko -l mode bug.  Open work file at most once.
 *
 * Revision 5.13  1991/02/25  07:12:33  eggert
 * getdate -> getcurdate (SVR4 name clash)
 *
 * Revision 5.12  1990/12/31  01:00:12  eggert
 * Don't use uninitialized storage when handling -{N,n}.
 *
 * Revision 5.11  1990/12/04  05:18:36  eggert
 * Use -I for prompts and -q for diagnostics.
 *
 * Revision 5.10  1990/11/05  20:30:10  eggert
 * Don't remove working file when aborting due to no changes.
 *
 * Revision 5.9  1990/11/01  05:03:23  eggert
 * Add -I and new -t behavior.  Permit arbitrary data in logs.
 *
 * Revision 5.8  1990/10/04  06:30:09  eggert
 * Accumulate exit status across files.
 *
 * Revision 5.7  1990/09/25  20:11:46  hammer
 * fixed another small typo
 *
 * Revision 5.6  1990/09/24  21:48:50  hammer
 * added cleanups from Paul Eggert.
 *
 * Revision 5.5  1990/09/21  06:16:38  hammer
 * made it handle multiple -{N,n}'s.  Also, made it treat re-directed stdin
 * the same as the terminal
 *
 * Revision 5.4  1990/09/20  02:38:51  eggert
 * ci -k now checks dates more thoroughly.
 *
 * Revision 5.3  1990/09/11  02:41:07  eggert
 * Fix revision bug with `ci -k file1 file2'.
 *
 * Revision 5.2  1990/09/04  08:02:10  eggert
 * Permit adjacent revisions with identical time stamps (possible on fast hosts).
 * Improve incomplete line handling.  Standardize yes-or-no procedure.
 *
 * Revision 5.1  1990/08/29  07:13:44  eggert
 * Expand locker value like co.  Clean old log messages too.
 *
 * Revision 5.0  1990/08/22  08:10:00  eggert
 * Don't require a final newline.
 * Make lock and temp files faster and safer.
 * Remove compile-time limits; use malloc instead.
 * Permit dates past 1999/12/31.  Switch to GMT.
 * Add setuid support.  Don't pass +args to diff.  Check diff's output.
 * Ansify and Posixate.  Add -k, -V.  Remove snooping.  Tune.
 * Check diff's output.
 *
 * Revision 4.9  89/05/01  15:10:54  narten
 * changed copyright header to reflect current distribution rules
 * 
 * Revision 4.8  88/11/08  13:38:23  narten
 * changes from root@seismo.CSS.GOV (Super User)
 * -d with no arguments uses the mod time of the file it is checking in
 * 
 * Revision 4.7  88/08/09  19:12:07  eggert
 * Make sure workfile is a regular file; use its mode if RCSfile doesn't have one.
 * Use execv(), not system(); allow cc -R; remove lint.
 * isatty(fileno(stdin)) -> ttystdin()
 * 
 * Revision 4.6  87/12/18  11:34:41  narten
 * lint cleanups (from Guy Harris)
 * 
 * Revision 4.5  87/10/18  10:18:48  narten
 * Updating version numbers. Changes relative to revision 1.1 are actually
 * relative to 4.3
 * 
 * Revision 1.3  87/09/24  13:57:19  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf 
 * warnings)
 * 
 * Revision 1.2  87/03/27  14:21:33  jenkins
 * Port to suns
 * 
 * Revision 4.3  83/12/15  12:28:54  wft
 * ci -u and ci -l now set mode of working file properly.
 * 
 * Revision 4.2  83/12/05  13:40:54  wft
 * Merged with 3.9.1.1: added calls to clearerr(stdin).
 * made rewriteflag external.
 * 
 * Revision 4.1  83/05/10  17:03:06  wft
 * Added option -d and -w, and updated assingment of date, etc. to new delta.
 * Added handling of default branches.
 * Option -k generates std. log message; fixed undef. pointer in reading of log.
 * Replaced getlock() with findlock(), link--unlink with rename(),
 * getpwuid() with getcaller().
 * Moved all revision number generation to new routine addelta().
 * Removed calls to stat(); now done by pairfilenames().
 * Changed most calls to catchints() with restoreints().
 * Directed all interactive messages to stderr.
 * 
 * Revision 3.9.1.1  83/10/19  04:21:03  lepreau
 * Added clearerr(stdin) to getlogmsg() for re-reading stdin.
 * 
 * Revision 3.9  83/02/15  15:25:44  wft
 * 4.2 prerelease
 * 
 * Revision 3.9  83/02/15  15:25:44  wft
 * Added call to fastcopy() to copy remainder of RCS file.
 *
 * Revision 3.8  83/01/14  15:34:05  wft
 * Added ignoring of interrupts while new RCS file is renamed;
 * Avoids deletion of RCS files by interrupts.
 *
 * Revision 3.7  82/12/10  16:09:20  wft
 * Corrected checking of return code from diff.
 *
 * Revision 3.6  82/12/08  21:34:49  wft
 * Using DATEFORM to prepare date of checked-in revision;
 * Fixed return from addbranch().
 *
 * Revision 3.5  82/12/04  18:32:42  wft
 * Replaced getdelta() with gettree(), SNOOPDIR with SNOOPFILE. Updated
 * field lockedby in removelock(), moved getlogmsg() before calling diff.
 *
 * Revision 3.4  82/12/02  13:27:13  wft
 * added option -k.
 *
 * Revision 3.3  82/11/28  20:53:31  wft
 * Added mustcheckin() to check for redundant checkins.
 * Added xpandfile() to do keyword expansion for -u and -l;
 * -m appends linefeed to log message if necessary.
 * getlogmsg() suppresses prompt if stdin is not a terminal.
 * Replaced keeplock with lockflag, fclose() with ffclose(),
 * %02d with %.2d, getlogin() with getpwuid().
 *
 * Revision 3.2  82/10/18  20:57:23  wft
 * An RCS file inherits its mode during the first ci from the working file,
 * otherwise it stays the same, except that write permission is removed.
 * Fixed ci -l, added ci -u (both do an implicit co after the ci).
 * Fixed call to getlogin(), added call to getfullRCSname(), added check
 * for write error.
 * Changed conflicting identifiers.
 *
 * Revision 3.1  82/10/13  16:04:59  wft
 * fixed type of variables receiving from getc() (char -> int).
 * added include file dbm.h for getting BYTESIZ. This is used
 * to check the return code from diff portably.
 */

#include "rcsbase.h"

struct Symrev {
       char const *ssymbol;
       int override;
       struct Symrev * nextsym;
};

static char const *getcurdate P((void));
static int addbranch P((struct hshentry*,struct buf*));
static int addelta P((void));
static int addsyms P((char const*));
static int fixwork P((mode_t,char const*));
static int removelock P((struct hshentry*));
static int xpandfile P((RILE*,char const*,struct hshentry const*,char const**));
static struct cbuf getlogmsg P((void));
static void cleanup P((void));
static void incnum P((char const*,struct buf*));
static void addassoclst P((int, char *));

static FILE *exfile;
static RILE *workptr;			/* working file pointer		*/
static struct buf newdelnum;		/* new revision number		*/
static struct cbuf msg;
static int exitstatus;
static int forceciflag;			/* forces check in		*/
static int keepflag, keepworkingfile, rcsinitflag;
static struct hshentries *gendeltas;	/* deltas to be generated	*/
static struct hshentry *targetdelta;	/* old delta to be generated	*/
static struct hshentry newdelta;	/* new delta to be inserted	*/
static struct stat workstat;
static struct Symrev *assoclst, *lastassoc;

mainProg(ciId, "ci", "$Id: ci.c,v 5.21 1991/11/20 17:58:07 eggert Exp $")
{
	static char const cmdusage[] =
		"\nci usage: ci -{fklqru}[rev] -mmsg -{nN}name -sstate -t[textfile] -Vn file ...";
	static char const default_state[] = DEFAULTSTATE;

	char altdate[datesize];
	char olddate[datesize];
	char newdatebuf[datesize], targetdatebuf[datesize];
	char *a, **newargv, *textfile;
	char const *author, *krev, *rev, *state;
	char const *diffilename, *expfilename;
	char const *workdiffname, *newworkfilename;
	char const *mtime;
	int lockflag, lockthis, mtimeflag, removedlock;
	int r;
	int changedRCS, changework, newhead;
	int usestatdate; /* Use mod time of file for -d.  */
	mode_t newworkmode; /* mode for working file */
	struct hshentry *workdelta;
	
	setrid();

	author = rev = state = textfile = nil;
	lockflag = false;
	mtimeflag = false;
	altdate[0]= '\0'; /* empty alternate date for -d */
	usestatdate=false;
	suffixes = X_DEFAULT;

	argc = getRCSINIT(argc, argv, &newargv);
	argv = newargv;
	while (a = *++argv,  0<--argc && *a++=='-') {
		switch (*a++) {

                case 'r':
			keepworkingfile = lockflag = false;
		revno:
			if (*a) {
				if (rev) warn("redefinition of revision number");
				rev = a;
                        }
                        break;

                case 'l':
                        keepworkingfile=lockflag=true;
                        goto revno;

                case 'u':
                        keepworkingfile=true; lockflag=false;
                        goto revno;

		case 'I':
			interactiveflag = true;
			goto revno;

                case 'q':
                        quietflag=true;
                        goto revno;

                case 'f':
                        forceciflag=true;
                        goto revno;

                case 'k':
                        keepflag=true;
                        goto revno;

                case 'm':
			if (msg.size) redefined('m');
			msg = cleanlogmsg(a, strlen(a));
			if (!msg.size)
				warn("missing message for -m option");
                        break;

                case 'n':
			if (!*a) {
                                error("missing symbolic name after -n");
				break;
            		}
			checksid(a);
			addassoclst(false, a);
		        break;
		
		case 'N':
			if (!*a) {
                                error("missing symbolic name after -N");
				break;
            		}
			checksid(a);
			addassoclst(true, a);
		        break;

                case 's':
			if (*a) {
				if (state) redefined('s');
				checksid(a);
				state = a;
			} else
				warn("missing state for -s option");
                        break;

                case 't':
			if (*a) {
				if (textfile) redefined('t');
				textfile = a;
                        }
                        break;

		case 'd':
			if (altdate[0] || usestatdate)
				redefined('d');
			altdate[0] = 0;
			if (!(usestatdate = !*a))
				str2date(a, altdate);
                        break;

		case 'M':
			mtimeflag = true;
			goto revno;

		case 'w':
			if (*a) {
				if (author) redefined('w');
				checksid(a);
				author = a;
			} else
				warn("missing author for -w option");
                        break;

		case 'x':
			suffixes = a;
			break;

		case 'V':
			setRCSversion(*argv);
			break;



                default:
			faterror("unknown option: %s%s", *argv, cmdusage);
                };
        }  /* end processing of options */

	if (argc<1) faterror("no input file%s", cmdusage);

        /* now handle all filenames */
        do {
        targetdelta=nil;
	ffree();

	switch (pairfilenames(argc, argv, rcswriteopen, false, false)) {

        case -1:                /* New RCS file */
#		if has_setuid && has_getuid
		    if (euid() != ruid()) {
			error("setuid initial checkin prohibited; use `rcs -i -a' first");
			continue;
		    }
#		endif
		rcsinitflag = true;
                break;

        case 0:                 /* Error */
                continue;

        case 1:                 /* Normal checkin with prev . RCS file */
		rcsinitflag = !Head;
        }

        /* now RCSfilename contains the name of the RCS file, and
         * workfilename contains the name of the working file.
	 * If the RCS file exists, finptr contains the file descriptor for the
         * RCS file. The admin node is initialized.
	 * RCSstat is set.
         */

	diagnose("%s  <--  %s\n", RCSfilename,workfilename);

	if (!(workptr = Iopen(workfilename, FOPEN_R_WORK, &workstat))) {
		eerror(workfilename);
		continue;
	}
	if (finptr && !checkaccesslist()) continue; /* give up */

	krev = rev;
        if (keepflag) {
                /* get keyword values from working file */
		if (!getoldkeys(workptr)) continue;
		if (!rev  &&  !*(krev = prevrev.string)) {
			error("can't find a revision number in %s",workfilename);
                        continue;
                }
		if (!*prevdate.string && *altdate=='\0' && usestatdate==false)
			warn("can't find a date in %s", workfilename);
		if (!*prevauthor.string && !author)
			warn("can't find an author in %s", workfilename);
		if (!*prevstate.string && !state)
			warn("can't find a state in %s", workfilename);
        } /* end processing keepflag */

	/* Read the delta tree.  */
	if (finptr)
	    gettree();

        /* expand symbolic revision number */
	if (!fexpandsym(krev, &newdelnum, workptr))
	    continue;

        /* splice new delta into tree */
	if ((removedlock = addelta()) < 0)
	    continue;

	newdelta.num = newdelnum.string;
        newdelta.branches=nil;
        newdelta.lockedby=nil; /*might be changed by addlock() */
	newdelta.selector = true;
	/* set author */
	if (author!=nil)
		newdelta.author=author;     /* set author given by -w         */
	else if (keepflag && *prevauthor.string)
		newdelta.author=prevauthor.string; /* preserve old author if possible*/
	else    newdelta.author=getcaller();/* otherwise use caller's id      */
	newdelta.state = default_state;
	if (state!=nil)
		newdelta.state=state;       /* set state given by -s          */
	else if (keepflag && *prevstate.string)
		newdelta.state=prevstate.string;   /* preserve old state if possible */
	if (usestatdate) {
	    time2date(workstat.st_mtime, altdate);
	}
	if (*altdate!='\0')
		newdelta.date=altdate;      /* set date given by -d           */
	else if (keepflag && *prevdate.string) {
		/* Preserve old date if possible.  */
		str2date(prevdate.string, olddate);
		newdelta.date = olddate;
	} else
		newdelta.date = getcurdate();  /* use current date */
	/* now check validity of date -- needed because of -d and -k          */
	if (targetdelta!=nil &&
	    cmpnum(newdelta.date,targetdelta->date) < 0) {
		error("Date %s precedes %s in existing revision %s.",
			date2str(newdelta.date, newdatebuf),
			date2str(targetdelta->date, targetdatebuf),
			targetdelta->num
		);
		continue;
	}


	if (lockflag  &&  addlock(&newdelta) < 0) continue;
	if (!addsyms(newdelta.num))
	    continue;

    
        putadmin(frewrite);
        puttree(Head,frewrite);
	putdesc(false,textfile);

	changework = Expand != OLD_EXPAND;
	lockthis = lockflag;
	workdelta = &newdelta;

        /* build rest of file */
	if (rcsinitflag) {
		diagnose("initial revision: %s\n", newdelnum.string);
                /* get logmessage */
                newdelta.log=getlogmsg();
		if (!putdftext(newdelnum.string,newdelta.log,workptr,frewrite,false)) continue;
		RCSstat.st_mode = workstat.st_mode;
		changedRCS = true;
        } else {
		diffilename = maketemp(0);
		workdiffname = workfilename;
		if (workdiffname[0] == '+') {
		    /* Some diffs have options with leading '+'.  */
		    char *dp = ftnalloc(char, strlen(workfilename)+3);
		    workdiffname = dp;
		    *dp++ = '.';
		    *dp++ = SLASH;
		    VOID strcpy(dp, workfilename);
		}
		newhead  =  Head == &newdelta;
		if (!newhead)
			foutptr = frewrite;
		expfilename = buildrevision(
			gendeltas, targetdelta, (FILE*)0, false
		);
		if (
		    !forceciflag  &&
		    (changework = rcsfcmp(
			workptr, &workstat, expfilename, targetdelta
		    )) <= 0
		) {
		    diagnose("file is unchanged; reverting to previous revision %s\n",
			targetdelta->num
		    );
		    if (removedlock < lockflag) {
			diagnose("previous revision was not locked; ignoring -l option\n");
			lockthis = 0;
		    }
		    if (!(changedRCS  =
			    lockflag < removedlock
			||  assoclst
			||	newdelta.state != default_state
			    &&	strcmp(newdelta.state, targetdelta->state) != 0
		    ))
			workdelta = targetdelta;
		    else {
			/*
			 * We have started to build the wrong new RCS file.
			 * Start over from the beginning.
			 */
			long hwm = ftell(frewrite);
			int bad_truncate;
			if (fseek(frewrite, 0L, SEEK_SET) != 0)
			    Oerror();
#			if !has_ftruncate
			    bad_truncate = 1;
#			else
			    /*
			     * Work around a common ftruncate() bug.
			     * We can't rely on has_truncate, because we might
			     * be using a filesystem exported to us via NFS.
			     */
			    bad_truncate = ftruncate(fileno(frewrite),(off_t)0);
			    if (bad_truncate  &&  errno != EACCES)
				Oerror();
#			endif
			Irewind(finptr);
			Lexinit();
			getadmin();
			gettree();
			if (!(workdelta = genrevs(
			    targetdelta->num, (char*)0, (char*)0, (char*)0,
			    &gendeltas
			)))
			    continue;
			workdelta->log = targetdelta->log;
			if (newdelta.state != default_state)
			    workdelta->state = newdelta.state;
			if (removedlock && removelock(workdelta)<0)
			    continue;
			if (!addsyms(workdelta->num))
			    continue;
			if (!dorewrite(true, true))
			    continue;
			fastcopy(finptr, frewrite);
			if (bad_truncate)
			    while (ftell(frewrite) < hwm)
				/* White out any earlier mistake with '\n's.  */
				/* This is unlikely.  */
				afputc('\n', frewrite);
		    }
		} else {
		    diagnose("new revision: %s; previous revision: %s\n",
			newdelnum.string, targetdelta->num
		    );
		    newdelta.log = getlogmsg();
		    switch (run((char*)0, diffilename,
			DIFF DIFF_FLAGS,
			newhead ? workdiffname : expfilename,
			newhead ? expfilename : workdiffname,
			(char*)0
		    )) {
			case DIFF_FAILURE: case DIFF_SUCCESS: break;
			default: faterror("diff failed");
		    }
		    if (newhead) {
			Irewind(workptr);
			if (!putdftext(newdelnum.string,newdelta.log,workptr,frewrite,false)) continue;
			if (!putdtext(targetdelta->num,targetdelta->log,diffilename,frewrite,true)) continue;
		    } else
			if (!putdtext(newdelnum.string,newdelta.log,diffilename,frewrite,true)) continue;
		    changedRCS = true;
                }
        }
	if (!donerewrite(changedRCS))
		continue;

        if (!keepworkingfile) {
		Izclose(&workptr);
		r = un_link(workfilename); /* Get rid of old file */
        } else {
		newworkmode = WORKMODE(RCSstat.st_mode,
			!   (Expand==VAL_EXPAND  ||  lockthis < StrictLocks)
		);
		mtime = mtimeflag ? workdelta->date : (char const*)0;

		/* Expand if it might change or if we can't fix mode, time.  */
		if (changework  ||  (r=fixwork(newworkmode,mtime)) != 0) {
		    Irewind(workptr);
		    /* Expand keywords in file.  */
		    locker_expansion = lockthis;
		    switch (xpandfile(
			workptr, workfilename,
			workdelta, &newworkfilename
		    )) {
			default:
			    continue;

			case 0:
			    /*
			     * No expansion occurred; try to reuse working file
			     * unless we already tried and failed.
			     */
			    if (changework)
				if ((r=fixwork(newworkmode,mtime)) == 0)
				    break;
			    /* fall into */
			case 1:
			    if (!(r = setfiledate(newworkfilename,mtime))) {
				Izclose(&workptr);
				ignoreints();
				r = chnamemod(&exfile, newworkfilename, workfilename, newworkmode);
				keepdirtemp(newworkfilename);
				restoreints();
			    }
		    }
		}
        }
	if (r != 0) {
	    eerror(workfilename);
	    continue;
	}
	diagnose("done\n");

        } while (cleanup(),
                 ++argv, --argc >=1);

	tempunlink();
	exitmain(exitstatus);
}       /* end of main (ci) */

	static void
cleanup()
{
	if (nerror) exitstatus = EXIT_FAILURE;
	Izclose(&finptr);
	Izclose(&workptr);
	Ozclose(&exfile);
	Ozclose(&fcopy);
	Ozclose(&frewrite);
	dirtempunlink();
}

#if lint
#	define exiterr ciExit
#endif
	exiting void
exiterr()
{
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}

/*****************************************************************/
/* the rest are auxiliary routines                               */


	static int
addelta()
/* Function: Appends a delta to the delta tree, whose number is
 * given by newdelnum.  Updates Head, newdelnum, newdelnumlength,
 * and the links in newdelta.
 * Return -1 on error, 1 if a lock is removed, 0 otherwise.
 */
{
	register char *tp;
	register unsigned i;
	int removedlock;
	unsigned newdnumlength;  /* actual length of new rev. num. */

	newdnumlength = countnumflds(newdelnum.string);

	if (rcsinitflag) {
                /* this covers non-existing RCS file and a file initialized with rcs -i */
		if ((newdnumlength==0)&&(Dbranch!=nil)) {
			bufscpy(&newdelnum, Dbranch);
			newdnumlength = countnumflds(Dbranch);
		}
		if (newdnumlength==0) bufscpy(&newdelnum, "1.1");
		else if (newdnumlength==1) bufscat(&newdelnum, ".1");
		else if (newdnumlength>2) {
		    error("Branch point doesn't exist for %s.",newdelnum.string);
		    return -1;
                } /* newdnumlength == 2 is OK;  */
                Head = &newdelta;
                newdelta.next=nil;
		return 0;
        }
        if (newdnumlength==0) {
                /* derive new revision number from locks */
		switch (findlock(true, &targetdelta)) {

		  default:
		    /* found two or more old locks */
		    return -1;

		  case 1:
                    /* found an old lock */
                    /* check whether locked revision exists */
		    if (!genrevs(targetdelta->num,(char*)0,(char*)0,(char*)0,&gendeltas))
			return -1;
                    if (targetdelta==Head) {
                        /* make new head */
                        newdelta.next=Head;
                        Head= &newdelta;
		    } else if (!targetdelta->next && countnumflds(targetdelta->num)>2) {
                        /* new tip revision on side branch */
                        targetdelta->next= &newdelta;
                        newdelta.next = nil;
                    } else {
                        /* middle revision; start a new branch */
			bufscpy(&newdelnum, "");
			return addbranch(targetdelta,&newdelnum);
                    }
		    incnum(targetdelta->num, &newdelnum);
		    return 1; /* successful use of existing lock */

		  case 0:
                    /* no existing lock; try Dbranch */
                    /* update newdelnum */
		    if (StrictLocks || !myself(RCSstat.st_uid)) {
			error("no lock set by %s",getcaller());
			return -1;
                    }
                    if (Dbranch) {
			bufscpy(&newdelnum, Dbranch);
                    } else {
			incnum(Head->num, &newdelnum);
                    }
		    newdnumlength = countnumflds(newdelnum.string);
                    /* now fall into next statement */
                }
        }
        if (newdnumlength<=2) {
                /* add new head per given number */
                if(newdnumlength==1) {
                    /* make a two-field number out of it*/
		    if (cmpnumfld(newdelnum.string,Head->num,1)==0)
			incnum(Head->num, &newdelnum);
		    else
			bufscat(&newdelnum, ".1");
                }
		if (cmpnum(newdelnum.string,Head->num) <= 0) {
                    error("deltanumber %s too low; must be higher than %s",
			  newdelnum.string, Head->num);
		    return -1;
                }
		targetdelta = Head;
		if (0 <= (removedlock = removelock(Head))) {
		    if (!genrevs(Head->num,(char*)0,(char*)0,(char*)0,&gendeltas))
			return -1;
		    newdelta.next = Head;
		    Head = &newdelta;
		}
		return removedlock;
        } else {
                /* put new revision on side branch */
                /*first, get branch point */
		tp = newdelnum.string;
		for (i = newdnumlength - (newdnumlength&1 ^ 1);  (--i);  )
			while (*tp++ != '.')
				;
		*--tp = 0; /* Kill final dot to get old delta temporarily. */
		if (!(targetdelta=genrevs(newdelnum.string,(char*)nil,(char*)nil,(char*)nil,&gendeltas)))
		    return -1;
		if (cmpnum(targetdelta->num, newdelnum.string) != 0) {
		    error("can't find branchpoint %s", newdelnum.string);
		    return -1;
                }
		*tp = '.'; /* Restore final dot. */
		return addbranch(targetdelta,&newdelnum);
        }
}



	static int
addbranch(branchpoint,num)
	struct hshentry *branchpoint;
	struct buf *num;
/* adds a new branch and branch delta at branchpoint.
 * If num is the null string, appends the new branch, incrementing
 * the highest branch number (initially 1), and setting the level number to 1.
 * the new delta and branchhead are in globals newdelta and newbranch, resp.
 * the new number is placed into num.
 * Return -1 on error, 1 if a lock is removed, 0 otherwise.
 */
{
	struct branchhead *bhead, **btrail;
	struct buf branchnum;
	int removedlock, result;
	unsigned field, numlength;
	static struct branchhead newbranch;  /* new branch to be inserted */

	numlength = countnumflds(num->string);

        if (branchpoint->branches==nil) {
                /* start first branch */
                branchpoint->branches = &newbranch;
                if (numlength==0) {
			bufscpy(num, branchpoint->num);
			bufscat(num, ".1.1");
		} else if (numlength&1)
			bufscat(num, ".1");
                newbranch.nextbranch=nil;

	} else if (numlength==0) {
                /* append new branch to the end */
                bhead=branchpoint->branches;
                while (bhead->nextbranch) bhead=bhead->nextbranch;
                bhead->nextbranch = &newbranch;
		bufautobegin(&branchnum);
		getbranchno(bhead->hsh->num, &branchnum);
		incnum(branchnum.string, num);
		bufautoend(&branchnum);
		bufscat(num, ".1");
                newbranch.nextbranch=nil;
        } else {
                /* place the branch properly */
		field = numlength - (numlength&1 ^ 1);
                /* field of branch number */
		btrail = &branchpoint->branches;
		while (0 < (result=cmpnumfld(num->string,(*btrail)->hsh->num,field))) {
			btrail = &(*btrail)->nextbranch;
			if (!*btrail) {
				result = -1;
				break;
			}
                }
		if (result < 0) {
                        /* insert/append new branchhead */
			newbranch.nextbranch = *btrail;
			*btrail = &newbranch;
			if (numlength&1) bufscat(num, ".1");
                } else {
                        /* branch exists; append to end */
			bufautobegin(&branchnum);
			getbranchno(num->string, &branchnum);
			targetdelta=genrevs(branchnum.string,(char*)nil,
					    (char*)nil,(char*)nil,&gendeltas);
			bufautoend(&branchnum);
			if (!targetdelta)
			    return -1;
			if (cmpnum(num->string,targetdelta->num) <= 0) {
                                error("deltanumber %s too low; must be higher than %s",
				      num->string,targetdelta->num);
				return -1;
                        }
			if (0 <= (removedlock = removelock(targetdelta))) {
			    if (numlength&1)
				incnum(targetdelta->num,num);
			    targetdelta->next = &newdelta;
			    newdelta.next = 0;
			}
			return removedlock;
			/* Don't do anything to newbranch.  */
                }
        }
        newbranch.hsh = &newdelta;
        newdelta.next=nil;
	return 0;
}

	static int
addsyms(num)
	char const *num;
{
	register struct Symrev *p;

	for (p = assoclst;  p;  p = p->nextsym)
		if (!addsymbol(num, p->ssymbol, p->override))
			return false;
	return true;
}


	static void
incnum(onum,nnum)
	char const *onum;
	struct buf *nnum;
/* Increment the last field of revision number onum by one and
 * place the result into nnum.
 */
{
	register char *tp, *np;
	register size_t l;

	l = strlen(onum);
	bufalloc(nnum, l+2);
	np = tp = nnum->string;
	VOID strcpy(np, onum);
	for (tp = np + l;  np != tp;  )
		if (isdigit(*--tp)) {
			if (*tp != '9') {
				++*tp;
				return;
			}
			*tp = '0';
		} else {
			tp++;
			break;
		}
	/* We changed 999 to 000; now change it to 1000.  */
	*tp = '1';
	tp = np + l;
	*tp++ = '0';
	*tp = 0;
}



	static int
removelock(delta)
struct hshentry * delta;
/* function: Finds the lock held by caller on delta,
 * removes it, and returns nonzero if successful.
 * Print an error message and return -1 if there is no such lock.
 * An exception is if !StrictLocks, and caller is the owner of
 * the RCS file. If caller does not have a lock in this case,
 * return 0; return 1 if a lock is actually removed.
 */
{
	register struct lock *next, **trail;
	char const *num;

        num=delta->num;
	for (trail = &Locks;  (next = *trail);  trail = &next->nextlock)
	    if (next->delta == delta)
		if (strcmp(getcaller(), next->login) == 0) {
		    /* We found a lock on delta by caller; delete it.  */
		    *trail = next->nextlock;
		    delta->lockedby = 0;
		    return 1;
		} else {
                    error("revision %s locked by %s",num,next->login);
		    return -1;
                }
	if (!StrictLocks && myself(RCSstat.st_uid))
	    return 0;
	error("no lock set by %s for revision %s", getcaller(), num);
	return -1;
}



	static char const *
getcurdate()
/* Return a pointer to the current date.  */
{
	static char buffer[datesize]; /* date buffer */
	time_t t;

	if (!buffer[0]) {
		t = time((time_t *)0);
		if (t == -1)
			faterror("time not available");
		time2date(t, buffer);
	}
        return buffer;
}

	static int
#if has_prototypes
fixwork(mode_t newworkmode, char const *mtime)
  /* The `#if has_prototypes' is needed because mode_t might promote to int.  */
#else
  fixwork(newworkmode, mtime)
	mode_t newworkmode;
	char const *mtime;
#endif
{
	int r;
	return
			1 < workstat.st_nlink
		    ||	newworkmode&S_IWUSR && !myself(workstat.st_uid)
		?   -1
	    :
			workstat.st_mode != newworkmode
		    &&
			(r =
#			    if has_fchmod
				fchmod(Ifileno(workptr), newworkmode)
#			    else
				chmod(workfilename, newworkmode)
#			    endif
			) != 0
		?   r
	    :
		setfiledate(workfilename, mtime);
}

	static int
xpandfile(unexfile, dir, delta, exfilename)
	RILE *unexfile;
	char const *dir;
	struct hshentry const *delta;
	char const **exfilename;
/*
 * Read unexfile and copy it to a
 * file in dir, performing keyword substitution with data from delta.
 * Return -1 if unsuccessful, 1 if expansion occurred, 0 otherwise.
 * If successful, stores the stream descriptor into *EXFILEP
 * and its name into *EXFILENAME.
 */
{
	char const *targetfname;
	int e, r;

	targetfname = makedirtemp(dir, 1);
	if (!(exfile = fopen(targetfname, FOPEN_W_WORK))) {
		eerror(targetfname);
		error("can't expand working file");
		return -1;
        }
	r = 0;
	if (Expand == OLD_EXPAND)
		fastcopy(unexfile,exfile);
	else {
		for (;;) {
			e = expandline(unexfile,exfile,delta,false,(FILE*)nil);
			if (e < 0)
				break;
			r |= e;
			if (e <= 1)
				break;
		}
	}
	*exfilename = targetfname;
	aflush(exfile);
	return r & 1;
}




/* --------------------- G E T L O G M S G --------------------------------*/


	static struct cbuf
getlogmsg()
/* Obtain and yield a log message.
 * If a log message is given with -m, yield that message.
 * If this is the initial revision, yield a standard log message.
 * Otherwise, reads a character string from the terminal.
 * Stops after reading EOF or a single '.' on a
 * line. getlogmsg prompts the first time it is called for the
 * log message; during all later calls it asks whether the previous
 * log message can be reused.
 */
{
	static char const
		emptych[] = EMPTYLOG,
		initialch[] = "Initial revision";
	static struct cbuf const
		emptylog = { emptych, sizeof(emptych)-sizeof(char) },
		initiallog = { initialch, sizeof(initialch)-sizeof(char) };
	static struct buf logbuf;
	static struct cbuf logmsg;

	register char *tp;
	register size_t i;
	char const *caller;

	if (msg.size) return msg;

	if (keepflag) {
		/* generate std. log message */
		caller = getcaller();
		i = sizeof(ciklog)+strlen(caller)+3;
		bufalloc(&logbuf, i+datesize);
		tp = logbuf.string;
		VOID sprintf(tp, "%s%s at ", ciklog, caller);
		VOID date2str(getcurdate(), tp+i);
		logmsg.string = tp;
		logmsg.size = strlen(tp);
		return logmsg;
	}

	if (!targetdelta && (
		cmpnum(newdelnum.string,"1.1")==0 ||
		cmpnum(newdelnum.string,"1.0")==0
	))
		return initiallog;

	if (logmsg.size) {
                /*previous log available*/
	    if (yesorno(true, "reuse log message of previous file? [yn](y): "))
		return logmsg;
        }

        /* now read string from stdin */
	logmsg = getsstdin("m", "log message", "", &logbuf);

        /* now check whether the log message is not empty */
	if (logmsg.size)
		return logmsg;
	return emptylog;
}

/*  Make a linked list of Symbolic names  */

        static void
addassoclst(flag, sp)
int  flag;
char * sp;
{
        struct Symrev *pt;
	
	pt = talloc(struct Symrev);
	pt->ssymbol = sp;
	pt->override = flag;
	pt->nextsym = nil;
	if (lastassoc)
	        lastassoc->nextsym = pt;
	else
	        assoclst = pt;
	lastassoc = pt;
	return;
}
