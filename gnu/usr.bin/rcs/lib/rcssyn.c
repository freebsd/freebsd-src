/*
 *                     RCS file input
 */
/*********************************************************************************
 *                       Syntax Analysis.
 *                       Keyword table
 *                       Testprogram: define SYNTEST
 *                       Compatibility with Release 2: define COMPAT2=1
 *********************************************************************************
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


/* $Log: rcssyn.c,v $
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
/* version SYNTEST inputs a RCS file and then prints out its internal
 * data structures.
*/

#include "rcsbase.h"

libId(synId, "$Id: rcssyn.c,v 5.8 1991/08/19 03:13:55 eggert Exp $")

/* forward */
static char const *getkeyval P((char const*,enum tokens,int));
static int strn2expmode P((char const*,size_t));

/* keyword table */

char const
	Kdesc[]     = "desc",
	Klog[]      = "log",
	Ktext[]     = "text";

static char const
	Kaccess[]   = "access",
	Kauthor[]   = "author",
	Kbranch[]   = "branch",
	K_branches[]= "branches",
	Kcomment[]  = "comment",
	Kdate[]     = "date",
	Kexpand[]   = "expand",
	Khead[]     = "head",
	Klocks[]    = "locks",
	Knext[]     = "next",
	Kstate[]    = "state",
	Kstrict[]   = "strict",
#if COMPAT2
	Ksuffix[]   = "suffix",
#endif
	Ksymbols[]  = "symbols";

static struct buf Commleader;
static struct cbuf Ignored;
struct cbuf Comment;
struct access   * AccessList;
struct assoc    * Symbols;
struct lock     * Locks;
int		  Expand;
int               StrictLocks;
struct hshentry * Head;
char const      * Dbranch;
unsigned TotalDeltas;


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
        struct lock     * newlock;
        struct hshentry * delta;
	struct access **LastAccess;
	struct assoc **LastSymbol;
	struct lock **LastLock;
	struct buf b;
	struct cbuf cb;

        TotalDeltas=0;

	getkey(Khead);
	Head = getdnum();
	getsemi(Khead);

	Dbranch = nil;
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
        while (id=getid()) {
		newaccess = ftalloc(struct access);
                newaccess->login = id;
		*LastAccess = newaccess;
		LastAccess = &newaccess->nextaccess;
        }
	*LastAccess = nil;
	getsemi(Kaccess);

	getkey(Ksymbols);
	LastSymbol = &Symbols;
        while (id = getid()) {
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
	*LastSymbol = nil;
	getsemi(Ksymbols);

	getkey(Klocks);
	LastLock = &Locks;
        while (id = getid()) {
                if (!getlex(COLON))
			fatserror("missing ':' in lock");
		if (!(delta=getdnum())) {
			fatserror("missing number in lock");
                } else { /*add new pair to lock list*/
			newlock = ftalloc(struct lock);
                        newlock->login=id;
                        newlock->delta=delta;
			*LastLock = newlock;
			LastLock = &newlock->nextlock;
                }
        }
	*LastLock = nil;
	getsemi(Klocks);

	if ((StrictLocks = getkeyopt(Kstrict)))
		getsemi(Kstrict);

	Comment.size = 0;
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
	"kv","kvl","k","v","o",
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
ignorephrase()
/* Ignore a phrase introduced by a later version of RCS.  */
{
	warnignore();
	hshenter=false;
	for (;;) {
	    switch (nexttok) {
		case SEMI: hshenter=true; nextlex(); return;
		case ID:
		case NUM: ffree1(NextString); break;
		case STRING: readstring(); break;
		default: break;
	    }
	    nextlex();
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
	*LastBranch = nil;
	getsemi(K_branches);

	getkey(Knext);
	Delta->next = num = getdnum();
	getsemi(Knext);
	Delta->lockedby = nil;
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
	struct lock const *currlock;

        while (getdelta());
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
	register char const *val = nil;

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
putadmin(fout)
register FILE * fout;
/* Function: Print the <admin> node read with getadmin() to file fout.
 * Assumption: Variables AccessList, Symbols, Locks, StrictLocks,
 * and Head have been set.
 */
{
	struct assoc const *curassoc;
	struct lock const *curlock;
	struct access const *curaccess;

	aprintf(fout, "%s\t%s;\n", Khead, Head?Head->num:"");
	if (Dbranch && VERSION(4)<=RCSversion)
		aprintf(fout, "%s\t%s;\n", Kbranch, Dbranch);

	aputs(Kaccess, fout);
        curaccess = AccessList;
        while (curaccess) {
	       aprintf(fout, "\n\t%s", curaccess->login);
               curaccess = curaccess->nextaccess;
        }
	aprintf(fout, ";\n%s", Ksymbols);
        curassoc = Symbols;
        while (curassoc) {
	       aprintf(fout, "\n\t%s:%s", curassoc->symbol, curassoc->num);
               curassoc = curassoc->nextassoc;
        }
	aprintf(fout, ";\n%s", Klocks);
        curlock = Locks;
        while (curlock) {
	       aprintf(fout, "\n\t%s:%s", curlock->login, curlock->delta->num);
               curlock = curlock->nextlock;
        }
	if (StrictLocks) aprintf(fout, "; %s", Kstrict);
	aprintf(fout, ";\n");
	if (Comment.size) {
		aprintf(fout, "%s\t", Kcomment);
		putstring(fout, true, Comment, false);
		aprintf(fout, ";\n");
        }
	if (Expand != KEYVAL_EXPAND)
		aprintf(fout, "%s\t%c%s%c;\n",
			Kexpand, SDELIM, expand_names[Expand], SDELIM
		);
	awrite(Ignored.string, Ignored.size, fout);
	aputc('\n', fout);
}




	static void
putdelta(node,fout)
register struct hshentry const *node;
register FILE * fout;
/* Function: prints a <delta> node to fout;
 */
{
	struct branchhead const *nextbranch;

        if (node == nil) return;

	aprintf(fout, "\n%s\n%s\t%s;\t%s %s;\t%s %s;\nbranches",
		node->num,
		Kdate, node->date,
		Kauthor, node->author,
		Kstate, node->state?node->state:""
	);
        nextbranch = node->branches;
        while (nextbranch) {
	       aprintf(fout, "\n\t%s", nextbranch->hsh->num);
               nextbranch = nextbranch->nextbranch;
        }

	aprintf(fout, ";\n%s\t%s;\n", Knext, node->next?node->next->num:"");
	awrite(node->ig.string, node->ig.size, fout);
}




	void
puttree(root,fout)
struct hshentry const *root;
register FILE * fout;
/* Function: prints the delta tree in preorder to fout, starting with root.
 */
{
	struct branchhead const *nextbranch;

        if (root==nil) return;

	if (root->selector)
		putdelta(root,fout);

        puttree(root->next,fout);

        nextbranch = root->branches;
        while (nextbranch) {
             puttree(nextbranch->hsh,fout);
             nextbranch = nextbranch->nextbranch;
        }
}


	static exiting void
unexpected_EOF()
{
	faterror("unexpected EOF in diff output");
}

int putdtext(num,log,srcfilename,fout,diffmt)
	char const *num, *srcfilename;
	struct cbuf log;
	FILE *fout;
	int diffmt;
/* Function: write a deltatext-node to fout.
 * num points to the deltanumber, log to the logmessage, and
 * sourcefile contains the text. Doubles up all SDELIMs in both the
 * log and the text; Makes sure the log message ends in \n.
 * returns false on error.
 * If diffmt is true, also checks that text is valid diff -n output.
 */
{
	RILE *fin;
	int result;
	if (!(fin = Iopen(srcfilename, "r", (struct stat*)0))) {
		eerror(srcfilename);
		return false;
	}
	result = putdftext(num,log,fin,fout,diffmt);
	Ifclose(fin);
	return result;
}

	void
putstring(out, delim, s, log)
	register FILE *out;
	struct cbuf s;
	int delim, log;
/*
 * Output to OUT one SDELIM if DELIM, then the string S with SDELIMs doubled.
 * If LOG is set then S is a log string; append a newline if S is nonempty.
 */
{
	register char const *sp;
	register size_t ss;

	if (delim)
		aputc(SDELIM, out);
	sp = s.string;
	for (ss = s.size;  ss;  --ss) {
		if (*sp == SDELIM)
			aputc(SDELIM, out);
		aputc(*sp++, out);
	}
	if (s.size && log)
		aputc('\n', out);
	aputc(SDELIM, out);
}

	int
putdftext(num,log,finfile,foutfile,diffmt)
	char const *num;
	struct cbuf log;
	RILE *finfile;
	FILE *foutfile;
	int diffmt;
/* like putdtext(), except the source file is already open */
{
	declarecache;
	register FILE *fout;
	register int c;
	register RILE *fin;
	int ed;
	struct diffcmd dc;

	fout = foutfile;
	aprintf(fout,DELNUMFORM,num,Klog);
        /* put log */
	putstring(fout, true, log, true);
        /* put text */
	aprintf(fout, "\n%s\n%c", Ktext, SDELIM);
	fin = finfile;
	setupcache(fin);
	if (!diffmt) {
	    /* Copy the file */
	    cache(fin);
	    for (;;) {
		cachegeteof(c, break;);
		if (c==SDELIM) aputc(SDELIM,fout);   /*double up SDELIM*/
		aputc(c,fout);
	    }
	} else {
	    initdiffcmd(&dc);
	    while (0  <=  (ed = getdiffcmd(fin,false,fout,&dc)))
		if (ed) {
		    cache(fin);
		    while (dc.nlines--)
			do {
			    cachegeteof(c, { if (!dc.nlines) goto OK_EOF; unexpected_EOF(); });
			    if (c == SDELIM)
				aputc(SDELIM,fout);
			    aputc(c,fout);
			} while (c != '\n');
		    uncache(fin);
		}
	}
    OK_EOF:
	aprintf(fout, "%c\n", SDELIM);
	return true;
}

	void
initdiffcmd(dc)
	register struct diffcmd *dc;
/* Initialize *dc suitably for getdiffcmd(). */
{
	dc->adprev = 0;
	dc->dafter = 0;
}

	static exiting void
badDiffOutput(buf)
	char const *buf;
{
	faterror("bad diff output line: %s", buf);
}

	static exiting void
diffLineNumberTooLarge(buf)
	char const *buf;
{
	faterror("diff line number too large: %s", buf);
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
	unsigned long line1, nlines, t;
	char buf[BUFSIZ];

	fin = finfile;
	fout = foutfile;
	setupcache(fin); cache(fin);
	cachegeteof(c, { if (delimiter) unexpected_EOF(); return -1; } );
	if (delimiter) {
		if (c==SDELIM) {
			cacheget(c);
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
			faterror("diff output command line too long");
		}
		*p++ = c;
		cachegeteof(c, unexpected_EOF();) ;
	} while (c != '\n');
	uncache(fin);
	if (delimiter)
		++rcsline;
	*p = '\0';
	for (p = buf+1;  (c = *p++) == ' ';  )
		;
	line1 = 0;
	while (isdigit(c)) {
		t = line1 * 10;
		if (
			ULONG_MAX/10 < line1  ||
			(line1 = t + (c - '0'))  <  t
		)
			diffLineNumberTooLarge(buf);
		c = *p++;
	}
	while (c == ' ')
		c = *p++;
	nlines = 0;
	while (isdigit(c)) {
		t = nlines * 10;
		if (
			ULONG_MAX/10 < nlines  ||
			(nlines = t + (c - '0'))  <  t
		)
			diffLineNumberTooLarge(buf);
		c = *p++;
	}
	if (c || !nlines) {
		badDiffOutput(buf);
	}
	if (line1+nlines < line1)
		diffLineNumberTooLarge(buf);
	switch (buf[0]) {
	    case 'a':
		if (line1 < dc->adprev) {
			faterror("backward insertion in diff output: %s", buf);
		}
		dc->adprev = line1 + 1;
		break;
	    case 'd':
		if (line1 < dc->adprev  ||  line1 < dc->dafter) {
			faterror("backward deletion in diff output: %s", buf);
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
        putadmin(stdout);

        gettree();
        puttree(Head,stdout);

        getdesc(true);

	nextlex();

	if (!eoflex()) {
		fatserror("expecting EOF");
        }
	exitmain(EXIT_SUCCESS);
}


exiting void exiterr() { _exit(EXIT_FAILURE); }


#endif

