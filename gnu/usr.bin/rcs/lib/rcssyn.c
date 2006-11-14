/* RCS file syntactic analysis */

/******************************************************************************
 *                       Syntax Analysis.
 *                       Keyword table
 *                       Testprogram: define SYNTEST
 *                       Compatibility with Release 2: define COMPAT2=1
 ******************************************************************************
 */

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
 * Revision 5.15  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.14  1995/06/01 16:23:43  eggert
 * (expand_names): Add "b" for -kb.
 * (getdelta): Don't strip leading "19" from MKS RCS dates; see cmpdate.
 *
 * Revision 5.13  1994/03/20 04:52:58  eggert
 * Remove lint.
 *
 * Revision 5.12  1993/11/03 17:42:27  eggert
 * Parse MKS RCS dates; ignore \r in diff control lines.
 * Don't discard ignored phrases.  Improve quality of diagnostics.
 *
 * Revision 5.11  1992/07/28  16:12:44  eggert
 * Avoid `unsigned'.  Statement macro names now end in _.
 *
 * Revision 5.10  1992/01/24  18:44:19  eggert
 * Move put routines to rcsgen.c.
 *
 * Revision 5.9  1992/01/06  02:42:34  eggert
 * ULONG_MAX/10 -> ULONG_MAX_OVER_10
 * while (E) ; -> while (E) continue;
 *
 * Revision 5.8  1991/08/19  03:13:55  eggert
 * Tune.
 *
 * Revision 5.7  1991/04/21  11:58:29  eggert
 * Disambiguate names on shortname hosts.
 * Fix errno bug.  Add MS-DOS support.
 *
 * Revision 5.6  1991/02/28  19:18:51  eggert
 * Fix null termination bug in reporting keyword expansion.
 *
 * Revision 5.5  1991/02/25  07:12:44  eggert
 * Check diff output more carefully; avoid overflow.
 *
 * Revision 5.4  1990/11/01  05:28:48  eggert
 * When ignoring unknown phrases, copy them to the output RCS file.
 * Permit arbitrary data in logs and comment leaders.
 * Don't check for nontext on initial checkin.
 *
 * Revision 5.3  1990/09/20  07:58:32  eggert
 * Remove the test for non-text bytes; it caused more pain than it cured.
 *
 * Revision 5.2  1990/09/04  08:02:30  eggert
 * Parse RCS files with no revisions.
 * Don't strip leading white space from diff commands.  Count RCS lines better.
 *
 * Revision 5.1  1990/08/29  07:14:06  eggert
 * Add -kkvl.  Clean old log messages too.
 *
 * Revision 5.0  1990/08/22  08:13:44  eggert
 * Try to parse future RCS formats without barfing.
 * Add -k.  Don't require final newline.
 * Remove compile-time limits; use malloc instead.
 * Don't output branch keyword if there's no default branch,
 * because RCS version 3 doesn't understand it.
 * Tune.  Remove lint.
 * Add support for ISO 8859.  Ansify and Posixate.
 * Check that a newly checked-in file is acceptable as input to 'diff'.
 * Check diff's output.
 *
 * Revision 4.6  89/05/01  15:13:32  narten
 * changed copyright header to reflect current distribution rules
 *
 * Revision 4.5  88/08/09  19:13:21  eggert
 * Allow cc -R; remove lint.
 *
 * Revision 4.4  87/12/18  11:46:16  narten
 * more lint cleanups (Guy Harris)
 *
 * Revision 4.3  87/10/18  10:39:36  narten
 * Updating version numbers. Changes relative to 1.1 actually relative to
 * 4.1
 *
 * Revision 1.3  87/09/24  14:00:49  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf
 * warnings)
 *
 * Revision 1.2  87/03/27  14:22:40  jenkins
 * Port to suns
 *
 * Revision 4.1  83/03/28  11:38:49  wft
 * Added parsing and printing of default branch.
 *
 * Revision 3.6  83/01/15  17:46:50  wft
 * Changed readdelta() to initialize selector and log-pointer.
 * Changed puttree to check for selector==DELETE; putdtext() uses DELNUMFORM.
 *
 * Revision 3.5  82/12/08  21:58:58  wft
 * renamed Commentleader to Commleader.
 *
 * Revision 3.4  82/12/04  13:24:40  wft
 * Added routine gettree(), which updates keeplock after reading the
 * delta tree.
 *
 * Revision 3.3  82/11/28  21:30:11  wft
 * Reading and printing of Suffix removed; version COMPAT2 skips the
 * Suffix for files of release 2 format. Fixed problems with printing nil.
 *
 * Revision 3.2  82/10/18  21:18:25  wft
 * renamed putdeltatext to putdtext.
 *
 * Revision 3.1  82/10/11  19:45:11  wft
 * made sure getc() returns into an integer.
 */



/* version COMPAT2 reads files of the format of release 2 and 3, but
 * generates files of release 3 format. Need not be defined if no
 * old RCS files generated with release 2 exist.
 */

#include "rcsbase.h"

libId(synId, "$FreeBSD$")

static char const *getkeyval P((char const*,enum tokens,int));
static int getdelta P((void));
static int strn2expmode P((char const*,size_t));
static struct hshentry *getdnum P((void));
static void badDiffOutput P((char const*)) exiting;
static void diffLineNumberTooLarge P((char const*)) exiting;
static void getsemi P((char const*));

/* keyword table */

char const
	Kaccess[]   = "access",
	Kauthor[]   = "author",
	Kbranch[]   = "branch",
	Kcomment[]  = "comment",
	Kdate[]     = "date",
	Kdesc[]     = "desc",
	Kexpand[]   = "expand",
	Khead[]     = "head",
	Klocks[]    = "locks",
	Klog[]      = "log",
	Knext[]     = "next",
	Kstate[]    = "state",
	Kstrict[]   = "strict",
	Ksymbols[]  = "symbols",
	Ktext[]     = "text";

static char const
#if COMPAT2
	Ksuffix[]   = "suffix",
#endif
	K_branches[]= "branches";

static struct buf Commleader;
struct cbuf Comment;
struct cbuf Ignored;
struct access   * AccessList;
struct assoc    * Symbols;
struct rcslock *Locks;
int		  Expand;
int               StrictLocks;
struct hshentry * Head;
char const      * Dbranch;
int TotalDeltas;


	static void
getsemi(key)
	char const *key;
/* Get a semicolon to finish off a phrase started by KEY.  */
{
	if (!getlex(SEMI))
		fatserror("missing ';' after '%s'", key);
}

	static struct hshentry *
getdnum()
/* Get a delta number.  */
{
	register struct hshentry *delta = getnum();
	if (delta && countnumflds(delta->num)&1)
		fatserror("%s isn't a delta number", delta->num);
	return delta;
}


	void
getadmin()
/* Read an <admin> and initialize the appropriate global variables.  */
{
	register char const *id;
        struct access   * newaccess;
        struct assoc    * newassoc;
	struct rcslock *newlock;
        struct hshentry * delta;
	struct access **LastAccess;
	struct assoc **LastSymbol;
	struct rcslock **LastLock;
	struct buf b;
	struct cbuf cb;

        TotalDeltas=0;

	getkey(Khead);
	Head = getdnum();
	getsemi(Khead);

	Dbranch = 0;
	if (getkeyopt(Kbranch)) {
		if ((delta = getnum()))
			Dbranch = delta->num;
		getsemi(Kbranch);
        }


#if COMPAT2
        /* read suffix. Only in release 2 format */
	if (getkeyopt(Ksuffix)) {
                if (nexttok==STRING) {
			readstring(); nextlex(); /* Throw away the suffix.  */
		} else if (nexttok==ID) {
                        nextlex();
                }
		getsemi(Ksuffix);
        }
#endif

	getkey(Kaccess);
	LastAccess = &AccessList;
	while ((id = getid())) {
		newaccess = ftalloc(struct access);
                newaccess->login = id;
		*LastAccess = newaccess;
		LastAccess = &newaccess->nextaccess;
        }
	*LastAccess = 0;
	getsemi(Kaccess);

	getkey(Ksymbols);
	LastSymbol = &Symbols;
        while ((id = getid())) {
                if (!getlex(COLON))
			fatserror("missing ':' in symbolic name definition");
                if (!(delta=getnum())) {
			fatserror("missing number in symbolic name definition");
                } else { /*add new pair to association list*/
			newassoc = ftalloc(struct assoc);
                        newassoc->symbol=id;
			newassoc->num = delta->num;
			*LastSymbol = newassoc;
			LastSymbol = &newassoc->nextassoc;
                }
        }
	*LastSymbol = 0;
	getsemi(Ksymbols);

	getkey(Klocks);
	LastLock = &Locks;
        while ((id = getid())) {
                if (!getlex(COLON))
			fatserror("missing ':' in lock");
		if (!(delta=getdnum())) {
			fatserror("missing number in lock");
                } else { /*add new pair to lock list*/
			newlock = ftalloc(struct rcslock);
                        newlock->login=id;
                        newlock->delta=delta;
			*LastLock = newlock;
			LastLock = &newlock->nextlock;
                }
        }
	*LastLock = 0;
	getsemi(Klocks);

	if ((StrictLocks = getkeyopt(Kstrict)))
		getsemi(Kstrict);

	clear_buf(&Comment);
	if (getkeyopt(Kcomment)) {
		if (nexttok==STRING) {
			Comment = savestring(&Commleader);
			nextlex();
		}
		getsemi(Kcomment);
        }

	Expand = KEYVAL_EXPAND;
	if (getkeyopt(Kexpand)) {
		if (nexttok==STRING) {
			bufautobegin(&b);
			cb = savestring(&b);
			if ((Expand = strn2expmode(cb.string,cb.size)) < 0)
			    fatserror("unknown expand mode %.*s",
				(int)cb.size, cb.string
			    );
			bufautoend(&b);
			nextlex();
		}
		getsemi(Kexpand);
        }
	Ignored = getphrases(Kdesc);
}

char const *const expand_names[] = {
	/* These must agree with *_EXPAND in rcsbase.h.  */
	"kv", "kvl", "k", "v", "o", "b",
	0
};

	int
str2expmode(s)
	char const *s;
/* Yield expand mode corresponding to S, or -1 if bad.  */
{
	return strn2expmode(s, strlen(s));
}

	static int
strn2expmode(s, n)
	char const *s;
	size_t n;
{
	char const *const *p;

	for (p = expand_names;  *p;  ++p)
		if (memcmp(*p,s,n) == 0  &&  !(*p)[n])
			return p - expand_names;
	return -1;
}


	void
ignorephrases(key)
	const char *key;
/*
* Ignore a series of phrases that do not start with KEY.
* Stop when the next phrase starts with a token that is not an identifier,
* or is KEY.
*/
{
	for (;;) {
		nextlex();
		if (nexttok != ID  ||  strcmp(NextString,key) == 0)
			break;
		warnignore();
		hshenter=false;
		for (;; nextlex()) {
			switch (nexttok) {
				case SEMI: hshenter=true; break;
				case ID:
				case NUM: ffree1(NextString); continue;
				case STRING: readstring(); continue;
				default: continue;
			}
			break;
		}
	}
}


	static int
getdelta()
/* Function: reads a delta block.
 * returns false if the current block does not start with a number.
 */
{
        register struct hshentry * Delta, * num;
	struct branchhead **LastBranch, *NewBranch;

	if (!(Delta = getdnum()))
		return false;

        hshenter = false; /*Don't enter dates into hashtable*/
	Delta->date = getkeyval(Kdate, NUM, false);
        hshenter=true;    /*reset hshenter for revision numbers.*/

        Delta->author = getkeyval(Kauthor, ID, false);

        Delta->state = getkeyval(Kstate, ID, true);

	getkey(K_branches);
	LastBranch = &Delta->branches;
	while ((num = getdnum())) {
		NewBranch = ftalloc(struct branchhead);
                NewBranch->hsh = num;
		*LastBranch = NewBranch;
		LastBranch = &NewBranch->nextbranch;
        }
	*LastBranch = 0;
	getsemi(K_branches);

	getkey(Knext);
	Delta->next = num = getdnum();
	getsemi(Knext);
	Delta->lockedby = 0;
	Delta->log.string = 0;
	Delta->selector = true;
	Delta->ig = getphrases(Kdesc);
        TotalDeltas++;
        return (true);
}


	void
gettree()
/* Function: Reads in the delta tree with getdelta(), then
 * updates the lockedby fields.
 */
{
	struct rcslock const *currlock;

	while (getdelta())
		continue;
        currlock=Locks;
        while (currlock) {
                currlock->delta->lockedby = currlock->login;
                currlock = currlock->nextlock;
        }
}


	void
getdesc(prdesc)
int  prdesc;
/* Function: read in descriptive text
 * nexttok is not advanced afterwards.
 * If prdesc is set, the text is printed to stdout.
 */
{

	getkeystring(Kdesc);
        if (prdesc)
                printstring();  /*echo string*/
        else    readstring();   /*skip string*/
}






	static char const *
getkeyval(keyword, token, optional)
	char const *keyword;
	enum tokens token;
	int optional;
/* reads a pair of the form
 * <keyword> <token> ;
 * where token is one of <id> or <num>. optional indicates whether
 * <token> is optional. A pointer to
 * the actual character string of <id> or <num> is returned.
 */
{
	register char const *val = 0;

	getkey(keyword);
        if (nexttok==token) {
                val = NextString;
                nextlex();
        } else {
		if (!optional)
			fatserror("missing %s", keyword);
        }
	getsemi(keyword);
        return(val);
}


	void
unexpected_EOF()
{
	rcsfaterror("unexpected EOF in diff output");
}

	void
initdiffcmd(dc)
	register struct diffcmd *dc;
/* Initialize *dc suitably for getdiffcmd(). */
{
	dc->adprev = 0;
	dc->dafter = 0;
}

	static void
badDiffOutput(buf)
	char const *buf;
{
	rcsfaterror("bad diff output line: %s", buf);
}

	static void
diffLineNumberTooLarge(buf)
	char const *buf;
{
	rcsfaterror("diff line number too large: %s", buf);
}

	int
getdiffcmd(finfile, delimiter, foutfile, dc)
	RILE *finfile;
	FILE *foutfile;
	int delimiter;
	struct diffcmd *dc;
/* Get a editing command output by 'diff -n' from fin.
 * The input is delimited by SDELIM if delimiter is set, EOF otherwise.
 * Copy a clean version of the command to fout (if nonnull).
 * Yield 0 for 'd', 1 for 'a', and -1 for EOF.
 * Store the command's line number and length into dc->line1 and dc->nlines.
 * Keep dc->adprev and dc->dafter up to date.
 */
{
	register int c;
	declarecache;
	register FILE *fout;
	register char *p;
	register RILE *fin;
	long line1, nlines, t;
	char buf[BUFSIZ];

	fin = finfile;
	fout = foutfile;
	setupcache(fin); cache(fin);
	cachegeteof_(c, { if (delimiter) unexpected_EOF(); return -1; } )
	if (delimiter) {
		if (c==SDELIM) {
			cacheget_(c)
			if (c==SDELIM) {
				buf[0] = c;
				buf[1] = 0;
				badDiffOutput(buf);
			}
			uncache(fin);
			nextc = c;
			if (fout)
				aprintf(fout, "%c%c", SDELIM, c);
			return -1;
		}
	}
	p = buf;
	do {
		if (buf+BUFSIZ-2 <= p) {
			rcsfaterror("diff output command line too long");
		}
		*p++ = c;
		cachegeteof_(c, unexpected_EOF();)
	} while (c != '\n');
	uncache(fin);
	if (delimiter)
		++rcsline;
	*p = '\0';
	for (p = buf+1;  (c = *p++) == ' ';  )
		continue;
	line1 = 0;
	while (isdigit(c)) {
		if (
			LONG_MAX/10 < line1  ||
			(t = line1 * 10,   (line1 = t + (c - '0'))  <  t)
		)
			diffLineNumberTooLarge(buf);
		c = *p++;
	}
	while (c == ' ')
		c = *p++;
	nlines = 0;
	while (isdigit(c)) {
		if (
			LONG_MAX/10 < nlines  ||
			(t = nlines * 10,   (nlines = t + (c - '0'))  <  t)
		)
			diffLineNumberTooLarge(buf);
		c = *p++;
	}
	if (c == '\r')
		c = *p++;
	if (c || !nlines) {
		badDiffOutput(buf);
	}
	if (line1+nlines < line1)
		diffLineNumberTooLarge(buf);
	switch (buf[0]) {
	    case 'a':
		if (line1 < dc->adprev) {
		    rcsfaterror("backward insertion in diff output: %s", buf);
		}
		dc->adprev = line1 + 1;
		break;
	    case 'd':
		if (line1 < dc->adprev  ||  line1 < dc->dafter) {
		    rcsfaterror("backward deletion in diff output: %s", buf);
		}
		dc->adprev = line1;
		dc->dafter = line1 + nlines;
		break;
	    default:
		badDiffOutput(buf);
	}
	if (fout) {
		aprintf(fout, "%s\n", buf);
	}
	dc->line1 = line1;
	dc->nlines = nlines;
	return buf[0] == 'a';
}



#ifdef SYNTEST

/* Input an RCS file and print its internal data structures.  */

char const cmdid[] = "syntest";

	int
main(argc,argv)
int argc; char * argv[];
{

        if (argc<2) {
		aputs("No input file\n",stderr);
		exitmain(EXIT_FAILURE);
        }
	if (!(finptr = Iopen(argv[1], FOPEN_R, (struct stat*)0))) {
		faterror("can't open input file %s", argv[1]);
        }
        Lexinit();
        getadmin();
	fdlock = STDOUT_FILENO;
	putadmin();

        gettree();

        getdesc(true);

	nextlex();

	if (!eoflex()) {
		fatserror("expecting EOF");
        }
	exitmain(EXIT_SUCCESS);
}

void exiterr() { _exit(EXIT_FAILURE); }

#endif
