/* Check in revisions of RCS files from working files.  */

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
 * Revision 5.30  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.29  1995/06/01 16:23:43  eggert
 * (main): Add -kb.
 * Use `cmpdate', not `cmpnum', to compare dates.
 * This is for MKS RCS's incompatible 20th-century date format.
 * Don't worry about errno after ftruncate fails.
 * Fix input file rewinding bug when large_memory && !maps_memory
 * and checking in a branch tip.
 *
 * (fixwork): Fall back on chmod if fchmod fails, since it might be ENOSYS.
 *
 * Revision 5.28  1994/03/20 04:52:58  eggert
 * Do not generate a corrupted RCS file if the user modifies the working file
 * while `ci' is running.
 * Do not remove the lock when `ci -l' reverts.
 * Move buffer-flushes out of critical sections, since they aren't critical.
 * Use ORCSerror to clean up after a fatal error.
 * Specify subprocess input via file descriptor, not file name.
 *
 * Revision 5.27  1993/11/09 17:40:15  eggert
 * -V now prints version on stdout and exits.  Don't print usage twice.
 *
 * Revision 5.26  1993/11/03 17:42:27  eggert
 * Add -z.  Don't subtract from RCS file timestamp even if -T.
 * Scan for and use Name keyword if -k.
 * Don't discard ignored phrases.  Improve quality of diagnostics.
 *
 * Revision 5.25  1992/07/28  16:12:44  eggert
 * Add -i, -j, -V.  Check that working and RCS files are distinct.
 *
 * Revision 5.24  1992/02/17  23:02:06  eggert
 * `-rREV' now just specifies a revision REV; only bare `-r' reverts to default.
 * Add -T.
 *
 * Revision 5.23  1992/01/27  16:42:51  eggert
 * Always unlock branchpoint if caller has a lock.
 * Add support for bad_chmod_close, bad_creat0.  lint -> RCS_lint
 *
 * Revision 5.22  1992/01/06  02:42:34  eggert
 * Invoke utime() before chmod() to keep some buggy systems happy.
 *
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
static int addbranch P((struct hshentry*,struct buf*,int));
static int addelta P((void));
static int addsyms P((char const*));
static int fixwork P((mode_t,time_t));
static int removelock P((struct hshentry*));
static int xpandfile P((RILE*,struct hshentry const*,char const**,int));
static struct cbuf getlogmsg P((void));
static void cleanup P((void));
static void incnum P((char const*,struct buf*));
static void addassoclst P((int,char const*));

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
static struct Symrev *assoclst, **nextassoc;

mainProg(ciId, "ci", "$FreeBSD: src/gnu/usr.bin/rcs/ci/ci.c,v 1.7 1999/08/27 23:36:38 peter Exp $")
{
	static char const cmdusage[] =
		"\nci usage: ci -{fIklMqru}[rev] -d[date] -mmsg -{nN}name -sstate -ttext -T -Vn -wwho -xsuff -zzone file ...";
	static char const default_state[] = DEFAULTSTATE;

	char altdate[datesize];
	char olddate[datesize];
	char newdatebuf[datesize + zonelenmax];
	char targetdatebuf[datesize + zonelenmax];
	char *a, **newargv, *textfile;
	char const *author, *krev, *rev, *state;
	char const *diffname, *expname;
	char const *newworkname;
	int initflag, mustread;
	int lockflag, lockthis, mtimeflag, removedlock, Ttimeflag;
	int r;
	int changedRCS, changework, dolog, newhead;
	int usestatdate; /* Use mod time of file for -d.  */
	mode_t newworkmode; /* mode for working file */
	time_t mtime, wtime;
	struct hshentry *workdelta;

	setrid();

	author = rev = state = textfile = 0;
	initflag = lockflag = mustread = false;
	mtimeflag = false;
	Ttimeflag = false;
	altdate[0]= '\0'; /* empty alternate date for -d */
	usestatdate=false;
	suffixes = X_DEFAULT;
	nextassoc = &assoclst;

	argc = getRCSINIT(argc, argv, &newargv);
	argv = newargv;
	while (a = *++argv,  0<--argc && *a++=='-') {
		switch (*a++) {

                case 'r':
			if (*a)
				goto revno;
			keepworkingfile = lockflag = false;
			break;

		case 'l':
			keepworkingfile = lockflag = true;
		revno:
			if (*a) {
				if (rev) warn("redefinition of revision number");
				rev = a;
                        }
                        break;

                case 'u':
                        keepworkingfile=true; lockflag=false;
                        goto revno;

		case 'i':
			initflag = true;
			goto revno;

		case 'j':
			mustread = true;
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
				error("missing message for -m option");
                        break;

                case 'n':
			if (!*a) {
                                error("missing symbolic name after -n");
				break;
            		}
			checkssym(a);
			addassoclst(false, a);
		        break;

		case 'N':
			if (!*a) {
                                error("missing symbolic name after -N");
				break;
            		}
			checkssym(a);
			addassoclst(true, a);
		        break;

                case 's':
			if (*a) {
				if (state) redefined('s');
				checksid(a);
				state = a;
			} else
				error("missing state for -s option");
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
			altdate[0] = '\0';
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
				error("missing author for -w option");
                        break;

		case 'x':
			suffixes = a;
			break;

		case 'V':
			setRCSversion(*argv);
			break;

		case 'z':
			zone_set(a);
			break;

		case 'T':
			if (!*a) {
				Ttimeflag = true;
				break;
			}
			/* fall into */
                default:
			error("unknown option: %s%s", *argv, cmdusage);
                };
        }  /* end processing of options */

	/* Handle all pathnames.  */
	if (nerror) cleanup();
	else if (argc < 1) faterror("no input file%s", cmdusage);
	else for (;  0 < argc;  cleanup(), ++argv, --argc) {
	targetdelta = 0;
	ffree();

	switch (pairnames(argc, argv, rcswriteopen, mustread, false)) {

        case -1:                /* New RCS file */
#		if has_setuid && has_getuid
		    if (euid() != ruid()) {
			workerror("setuid initial checkin prohibited; use `rcs -i -a' first");
			continue;
		    }
#		endif
		rcsinitflag = true;
                break;

        case 0:                 /* Error */
                continue;

        case 1:                 /* Normal checkin with prev . RCS file */
		if (initflag) {
			rcserror("already exists");
			continue;
		}
		rcsinitflag = !Head;
        }

	/*
	 * RCSname contains the name of the RCS file, and
	 * workname contains the name of the working file.
	 * If the RCS file exists, finptr contains the file descriptor for the
	 * RCS file, and RCSstat is set. The admin node is initialized.
         */

	diagnose("%s  <--  %s\n", RCSname, workname);

	if (!(workptr = Iopen(workname, FOPEN_R_WORK, &workstat))) {
		eerror(workname);
		continue;
	}

	if (finptr) {
		if (same_file(RCSstat, workstat, 0)) {
			rcserror("RCS file is the same as working file %s.",
				workname
			);
			continue;
		}
		if (!checkaccesslist())
			continue;
	}

	krev = rev;
        if (keepflag) {
                /* get keyword values from working file */
		if (!getoldkeys(workptr)) continue;
		if (!rev  &&  !*(krev = prevrev.string)) {
			workerror("can't find a revision number");
                        continue;
                }
		if (!*prevdate.string && *altdate=='\0' && usestatdate==false)
			workwarn("can't find a date");
		if (!*prevauthor.string && !author)
			workwarn("can't find an author");
		if (!*prevstate.string && !state)
			workwarn("can't find a state");
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
	newdelta.branches = 0;
	newdelta.lockedby = 0; /* This might be changed by addlock().  */
	newdelta.selector = true;
	newdelta.name = 0;
	clear_buf(&newdelta.ig);
	clear_buf(&newdelta.igtext);
	/* set author */
	if (author)
		newdelta.author=author;     /* set author given by -w         */
	else if (keepflag && *prevauthor.string)
		newdelta.author=prevauthor.string; /* preserve old author if possible*/
	else    newdelta.author=getcaller();/* otherwise use caller's id      */
	newdelta.state = default_state;
	if (state)
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
	if (targetdelta &&
	    cmpdate(newdelta.date,targetdelta->date) < 0) {
		rcserror("Date %s precedes %s in revision %s.",
			date2str(newdelta.date, newdatebuf),
			date2str(targetdelta->date, targetdatebuf),
			targetdelta->num
		);
		continue;
	}


	if (lockflag  &&  addlock(&newdelta, true) < 0) continue;

	if (keepflag && *prevname.string)
	    if (addsymbol(newdelta.num, prevname.string, false)  <  0)
		continue;
	if (!addsyms(newdelta.num))
	    continue;


	putadmin();
        puttree(Head,frewrite);
	putdesc(false,textfile);

	changework = Expand < MIN_UNCHANGED_EXPAND;
	dolog = true;
	lockthis = lockflag;
	workdelta = &newdelta;

        /* build rest of file */
	if (rcsinitflag) {
		diagnose("initial revision: %s\n", newdelta.num);
                /* get logmessage */
                newdelta.log=getlogmsg();
		putdftext(&newdelta, workptr, frewrite, false);
		RCSstat.st_mode = workstat.st_mode;
		RCSstat.st_nlink = 0;
		changedRCS = true;
        } else {
		diffname = maketemp(0);
		newhead  =  Head == &newdelta;
		if (!newhead)
			foutptr = frewrite;
		expname = buildrevision(
			gendeltas, targetdelta, (FILE*)0, false
		);
		if (
		    !forceciflag  &&
		    strcmp(newdelta.state, targetdelta->state) == 0  &&
		    (changework = rcsfcmp(
			workptr, &workstat, expname, targetdelta
		    )) <= 0
		) {
		    diagnose("file is unchanged; reverting to previous revision %s\n",
			targetdelta->num
		    );
		    if (removedlock < lockflag) {
			diagnose("previous revision was not locked; ignoring -l option\n");
			lockthis = 0;
		    }
		    dolog = false;
		    if (! (changedRCS = lockflag<removedlock || assoclst))
			workdelta = targetdelta;
		    else {
			/*
			 * We have started to build the wrong new RCS file.
			 * Start over from the beginning.
			 */
			long hwm = ftell(frewrite);
			int bad_truncate;
			Orewind(frewrite);

			/*
			* Work around a common ftruncate() bug:
			* NFS won't let you truncate a file that you
			* currently lack permissions for, even if you
			* had permissions when you opened it.
			* Also, Posix 1003.1b-1993 sec 5.6.7.2 p 128 l 1022
			* says ftruncate might fail because it's not supported.
			*/
#			if !has_ftruncate
#			    undef ftruncate
#			    define ftruncate(fd,length) (-1)
#			endif
			bad_truncate = ftruncate(fileno(frewrite), (off_t)0);

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
			if (lockthis<removedlock && removelock(workdelta)<0)
			    continue;
			if (!addsyms(workdelta->num))
			    continue;
			if (dorewrite(true, true) != 0)
			    continue;
			fastcopy(finptr, frewrite);
			if (bad_truncate)
			    while (ftell(frewrite) < hwm)
				/* White out any earlier mistake with '\n's.  */
				/* This is unlikely.  */
				afputc('\n', frewrite);
		    }
		} else {
		    int wfd = Ifileno(workptr);
		    struct stat checkworkstat;
		    char const *diffv[6 + !!OPEN_O_BINARY], **diffp;
#		    if large_memory && !maps_memory
			FILE *wfile = workptr->stream;
			long wfile_off;
#		    endif
#		    if !has_fflush_input && !(large_memory && maps_memory)
		        off_t wfd_off;
#		    endif

		    diagnose("new revision: %s; previous revision: %s\n",
			newdelta.num, targetdelta->num
		    );
		    newdelta.log = getlogmsg();
#		    if !large_memory
			Irewind(workptr);
#			if has_fflush_input
			    if (fflush(workptr) != 0)
				Ierror();
#			endif
#		    else
#			if !maps_memory
			    if (
			    	(wfile_off = ftell(wfile)) == -1
			     ||	fseek(wfile, 0L, SEEK_SET) != 0
#			     if has_fflush_input
			     ||	fflush(wfile) != 0
#			     endif
			    )
				Ierror();
#			endif
#		    endif
#		    if !has_fflush_input && !(large_memory && maps_memory)
			wfd_off = lseek(wfd, (off_t)0, SEEK_CUR);
			if (wfd_off == -1
			    || (wfd_off != 0
				&& lseek(wfd, (off_t)0, SEEK_SET) != 0))
			    Ierror();
#		    endif
		    diffp = diffv;
		    *++diffp = DIFF;
		    *++diffp = DIFFFLAGS;
#		    if OPEN_O_BINARY
			if (Expand == BINARY_EXPAND)
			    *++diffp = "--binary";
#		    endif
		    *++diffp = newhead ? "-" : expname;
		    *++diffp = newhead ? expname : "-";
		    *++diffp = 0;
		    switch (runv(wfd, diffname, diffv)) {
			case DIFF_FAILURE: case DIFF_SUCCESS: break;
			default: rcsfaterror("diff failed");
		    }
#		    if !has_fflush_input && !(large_memory && maps_memory)
			if (lseek(wfd, wfd_off, SEEK_CUR) == -1)
			    Ierror();
#		    endif
#		    if large_memory && !maps_memory
			if (fseek(wfile, wfile_off, SEEK_SET) != 0)
			    Ierror();
#		    endif
		    if (newhead) {
			Irewind(workptr);
			putdftext(&newdelta, workptr, frewrite, false);
			if (!putdtext(targetdelta,diffname,frewrite,true)) continue;
		    } else
			if (!putdtext(&newdelta,diffname,frewrite,true)) continue;

		    /*
		    * Check whether the working file changed during checkin,
		    * to avoid producing an inconsistent RCS file.
		    */
		    if (
			fstat(wfd, &checkworkstat) != 0
		     ||	workstat.st_mtime != checkworkstat.st_mtime
		     ||	workstat.st_size != checkworkstat.st_size
		    ) {
			workerror("file changed during checkin");
			continue;
		    }

		    changedRCS = true;
                }
        }

	/* Deduce time_t of new revision if it is needed later.  */
	wtime = (time_t)-1;
	if (mtimeflag | Ttimeflag)
		wtime = date2time(workdelta->date);

	if (donerewrite(changedRCS,
		!Ttimeflag ? (time_t)-1
		: finptr && wtime < RCSstat.st_mtime ? RCSstat.st_mtime
		: wtime
	) != 0)
		continue;

        if (!keepworkingfile) {
		Izclose(&workptr);
		r = un_link(workname); /* Get rid of old file */
        } else {
		newworkmode = WORKMODE(RCSstat.st_mode,
			!   (Expand==VAL_EXPAND  ||  lockthis < StrictLocks)
		);
		mtime = mtimeflag ? wtime : (time_t)-1;

		/* Expand if it might change or if we can't fix mode, time.  */
		if (changework  ||  (r=fixwork(newworkmode,mtime)) != 0) {
		    Irewind(workptr);
		    /* Expand keywords in file.  */
		    locker_expansion = lockthis;
		    workdelta->name =
			namedrev(
				assoclst ? assoclst->ssymbol
				: keepflag && *prevname.string ? prevname.string
				: rev,
				workdelta
			);
		    switch (xpandfile(
			workptr, workdelta, &newworkname, dolog
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
			    Izclose(&workptr);
			    aflush(exfile);
			    ignoreints();
			    r = chnamemod(&exfile, newworkname,
				    workname, 1, newworkmode, mtime
			    );
			    keepdirtemp(newworkname);
			    restoreints();
		    }
		}
        }
	if (r != 0) {
	    eerror(workname);
	    continue;
	}
	diagnose("done\n");

	}

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
	ORCSclose();
	dirtempunlink();
}

#if RCS_lint
#	define exiterr ciExit
#endif
	void
exiterr()
{
	ORCSerror();
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
	register int i;
	int removedlock;
	int newdnumlength;  /* actual length of new rev. num. */

	newdnumlength = countnumflds(newdelnum.string);

	if (rcsinitflag) {
                /* this covers non-existing RCS file and a file initialized with rcs -i */
		if (newdnumlength==0 && Dbranch) {
			bufscpy(&newdelnum, Dbranch);
			newdnumlength = countnumflds(Dbranch);
		}
		if (newdnumlength==0) bufscpy(&newdelnum, "1.1");
		else if (newdnumlength==1) bufscat(&newdelnum, ".1");
		else if (newdnumlength>2) {
		    rcserror("Branch point doesn't exist for revision %s.",
			newdelnum.string
		    );
		    return -1;
                } /* newdnumlength == 2 is OK;  */
                Head = &newdelta;
		newdelta.next = 0;
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
			newdelta.next = 0;
                    } else {
                        /* middle revision; start a new branch */
			bufscpy(&newdelnum, "");
			return addbranch(targetdelta, &newdelnum, 1);
                    }
		    incnum(targetdelta->num, &newdelnum);
		    return 1; /* successful use of existing lock */

		  case 0:
                    /* no existing lock; try Dbranch */
                    /* update newdelnum */
		    if (StrictLocks || !myself(RCSstat.st_uid)) {
			rcserror("no lock set by %s", getcaller());
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
		    rcserror("revision %s too low; must be higher than %s",
			  newdelnum.string, Head->num
		    );
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
		for (i = newdnumlength - ((newdnumlength&1) ^ 1);  --i;  )
			while (*tp++ != '.')
				continue;
		*--tp = 0; /* Kill final dot to get old delta temporarily. */
		if (!(targetdelta=genrevs(newdelnum.string,(char*)0,(char*)0,(char*)0,&gendeltas)))
		    return -1;
		if (cmpnum(targetdelta->num, newdelnum.string) != 0) {
		    rcserror("can't find branch point %s", newdelnum.string);
		    return -1;
                }
		*tp = '.'; /* Restore final dot. */
		return addbranch(targetdelta, &newdelnum, 0);
        }
}



	static int
addbranch(branchpoint, num, removedlock)
	struct hshentry *branchpoint;
	struct buf *num;
	int removedlock;
/* adds a new branch and branch delta at branchpoint.
 * If num is the null string, appends the new branch, incrementing
 * the highest branch number (initially 1), and setting the level number to 1.
 * the new delta and branchhead are in globals newdelta and newbranch, resp.
 * the new number is placed into num.
 * Return -1 on error, 1 if a lock is removed, 0 otherwise.
 * If REMOVEDLOCK is 1, a lock was already removed.
 */
{
	struct branchhead *bhead, **btrail;
	struct buf branchnum;
	int result;
	int field, numlength;
	static struct branchhead newbranch;  /* new branch to be inserted */

	numlength = countnumflds(num->string);

	if (!branchpoint->branches) {
                /* start first branch */
                branchpoint->branches = &newbranch;
                if (numlength==0) {
			bufscpy(num, branchpoint->num);
			bufscat(num, ".1.1");
		} else if (numlength&1)
			bufscat(num, ".1");
		newbranch.nextbranch = 0;

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
		newbranch.nextbranch = 0;
        } else {
                /* place the branch properly */
		field = numlength - ((numlength&1) ^ 1);
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
			targetdelta = genrevs(
				branchnum.string, (char*)0, (char*)0, (char*)0,
				&gendeltas
			);
			bufautoend(&branchnum);
			if (!targetdelta)
			    return -1;
			if (cmpnum(num->string,targetdelta->num) <= 0) {
				rcserror("revision %s too low; must be higher than %s",
				      num->string, targetdelta->num
				);
				return -1;
                        }
			if (!removedlock
			    && 0 <= (removedlock = removelock(targetdelta))
			) {
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
	newdelta.next = 0;
	if (branchpoint->lockedby)
	    if (strcmp(branchpoint->lockedby, getcaller()) == 0)
		return removelock(branchpoint); /* This returns 1.  */
	return removedlock;
}

	static int
addsyms(num)
	char const *num;
{
	register struct Symrev *p;

	for (p = assoclst;  p;  p = p->nextsym)
		if (addsymbol(num, p->ssymbol, p->override)  <  0)
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
	register struct rcslock *next, **trail;
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
		    rcserror("revision %s locked by %s", num, next->login);
		    return -1;
                }
	if (!StrictLocks && myself(RCSstat.st_uid))
	    return 0;
	rcserror("no lock set by %s for revision %s", getcaller(), num);
	return -1;
}



	static char const *
getcurdate()
/* Return a pointer to the current date.  */
{
	static char buffer[datesize]; /* date buffer */

	if (!buffer[0])
		time2date(now(), buffer);
        return buffer;
}

	static int
#if has_prototypes
fixwork(mode_t newworkmode, time_t mtime)
  /* The `#if has_prototypes' is needed because mode_t might promote to int.  */
#else
  fixwork(newworkmode, mtime)
	mode_t newworkmode;
	time_t mtime;
#endif
{
	return
			1 < workstat.st_nlink
		    ||	(newworkmode&S_IWUSR && !myself(workstat.st_uid))
		    ||	setmtime(workname, mtime) != 0
		?   -1
	    :	workstat.st_mode == newworkmode  ?  0
#if has_fchmod
	    :	fchmod(Ifileno(workptr), newworkmode) == 0  ?  0
#endif
#if bad_chmod_close
	    :	-1
#else
	    :	chmod(workname, newworkmode)
#endif
	;
}

	static int
xpandfile(unexfile, delta, exname, dolog)
	RILE *unexfile;
	struct hshentry const *delta;
	char const **exname;
	int dolog;
/*
 * Read unexfile and copy it to a
 * file, performing keyword substitution with data from delta.
 * Return -1 if unsuccessful, 1 if expansion occurred, 0 otherwise.
 * If successful, stores the stream descriptor into *EXFILEP
 * and its name into *EXNAME.
 */
{
	char const *targetname;
	int e, r;

	targetname = makedirtemp(1);
	if (!(exfile = fopenSafer(targetname, FOPEN_W_WORK))) {
		eerror(targetname);
		workerror("can't build working file");
		return -1;
        }
	r = 0;
	if (MIN_UNEXPAND <= Expand)
		fastcopy(unexfile,exfile);
	else {
		for (;;) {
			e = expandline(
				unexfile, exfile, delta, false, (FILE*)0, dolog
			);
			if (e < 0)
				break;
			r |= e;
			if (e <= 1)
				break;
		}
	}
	*exname = targetname;
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
		bufalloc(&logbuf, i + datesize + zonelenmax);
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
	int flag;
	char const *sp;
{
        struct Symrev *pt;

	pt = talloc(struct Symrev);
	pt->ssymbol = sp;
	pt->override = flag;
	pt->nextsym = 0;
	*nextassoc = pt;
	nextassoc = &pt->nextsym;
}
