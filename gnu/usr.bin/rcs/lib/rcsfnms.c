/*
 *                     RCS file name handling
 */
/****************************************************************************
 *                     creation and deletion of /tmp temporaries
 *                     pairing of RCS file names and working file names.
 *                     Testprogram: define PAIRTEST
 ****************************************************************************
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




/* $Log: rcsfnms.c,v $
 * Revision 5.8  1991/09/24  00:28:40  eggert
 * Don't export bindex().
 *
 * Revision 5.7  1991/08/19  03:13:55  eggert
 * Fix messages when rcswriteopen fails.
 * Look in $TMP and $TEMP if $TMPDIR isn't set.  Tune.
 *
 * Revision 5.6  1991/04/21  11:58:23  eggert
 * Fix errno bugs.  Add -x, RCSINIT, MS-DOS support.
 *
 * Revision 5.5  1991/02/26  17:48:38  eggert
 * Fix setuid bug.  Support new link behavior.
 * Define more portable getcwd().
 *
 * Revision 5.4  1990/11/01  05:03:43  eggert
 * Permit arbitrary data in comment leaders.
 *
 * Revision 5.3  1990/09/14  22:56:16  hammer
 * added more filename extensions and their comment leaders
 *
 * Revision 5.2  1990/09/04  08:02:23  eggert
 * Fix typo when !RCSSEP.
 *
 * Revision 5.1  1990/08/29  07:13:59  eggert
 * Work around buggy compilers with defective argument promotion.
 *
 * Revision 5.0  1990/08/22  08:12:50  eggert
 * Ignore signals when manipulating the semaphore file.
 * Modernize list of file name extensions.
 * Permit paths of arbitrary length.  Beware file names beginning with "-".
 * Remove compile-time limits; use malloc instead.
 * Permit dates past 1999/12/31.  Make lock and temp files faster and safer.
 * Ansify and Posixate.
 * Don't use access().  Fix test for non-regular files.  Tune.
 *
 * Revision 4.8  89/05/01  15:09:41  narten
 * changed getwd to not stat empty directories.
 * 
 * Revision 4.7  88/08/09  19:12:53  eggert
 * Fix troff macro comment leader bug; add Prolog; allow cc -R; remove lint.
 * 
 * Revision 4.6  87/12/18  11:40:23  narten
 * additional file types added from 4.3 BSD version, and SPARC assembler
 * comment character added. Also, more lint cleanups. (Guy Harris)
 * 
 * Revision 4.5  87/10/18  10:34:16  narten
 * Updating version numbers. Changes relative to 1.1 actually relative
 * to verion 4.3
 * 
 * Revision 1.3  87/03/27  14:22:21  jenkins
 * Port to suns
 * 
 * Revision 1.2  85/06/26  07:34:28  svb
 * Comment leader '% ' for '*.tex' files added.
 * 
 * Revision 4.3  83/12/15  12:26:48  wft
 * Added check for KDELIM in file names to pairfilenames().
 * 
 * Revision 4.2  83/12/02  22:47:45  wft
 * Added csh, red, and sl file name suffixes.
 * 
 * Revision 4.1  83/05/11  16:23:39  wft
 * Added initialization of Dbranch to InitAdmin(). Canged pairfilenames():
 * 1. added copying of path from workfile to RCS file, if RCS file is omitted;
 * 2. added getting the file status of RCS and working files;
 * 3. added ignoring of directories.
 * 
 * Revision 3.7  83/05/11  15:01:58  wft
 * Added comtable[] which pairs file name suffixes with comment leaders;
 * updated InitAdmin() accordingly.
 * 
 * Revision 3.6  83/04/05  14:47:36  wft
 * fixed Suffix in InitAdmin().
 * 
 * Revision 3.5  83/01/17  18:01:04  wft
 * Added getwd() and rename(); these can be removed by defining
 * V4_2BSD, since they are not needed in 4.2 bsd.
 * Changed sys/param.h to sys/types.h.
 *
 * Revision 3.4  82/12/08  21:55:20  wft
 * removed unused variable.
 *
 * Revision 3.3  82/11/28  20:31:37  wft
 * Changed mktempfile() to store the generated file names.
 * Changed getfullRCSname() to store the file and pathname, and to
 * delete leading "../" and "./".
 *
 * Revision 3.2  82/11/12  14:29:40  wft
 * changed pairfilenames() to handle file.sfx,v; also deleted checkpathnosfx(),
 * checksuffix(), checkfullpath(). Semaphore name generation updated.
 * mktempfile() now checks for nil path; freefilename initialized properly.
 * Added Suffix .h to InitAdmin. Added testprogram PAIRTEST.
 * Moved rmsema, trysema, trydiraccess, getfullRCSname from rcsutil.c to here.
 *
 * Revision 3.1  82/10/18  14:51:28  wft
 * InitAdmin() now initializes StrictLocks=STRICT_LOCKING (def. in rcsbase.h).
 * renamed checkpath() to checkfullpath().
 */


#include "rcsbase.h"

libId(fnmsId, "$Id: rcsfnms.c,v 5.8 1991/09/24 00:28:40 eggert Exp $")

char const *RCSfilename;
char *workfilename;
FILE *workstdout;
struct stat RCSstat;
char const *suffixes;

static char const rcsdir[] = "RCS";
#define rcsdirlen (sizeof(rcsdir)-1)

static struct buf RCSbuf, RCSb;
static int RCSerrno;


/* Temp file names to be unlinked when done, if they are not nil.  */
#define TEMPNAMES 5 /* must be at least DIRTEMPNAMES (see rcsedit.c) */
static char *volatile tfnames[TEMPNAMES];


struct compair {
	char const *suffix, *comlead;
};

static struct compair const comtable[] = {
/* comtable pairs each filename suffix with a comment leader. The comment   */
/* leader is placed before each line generated by the $Log keyword. This    */
/* table is used to guess the proper comment leader from the working file's */
/* suffix during initial ci (see InitAdmin()). Comment leaders are needed   */
/* for languages without multiline comments; for others they are optional.  */
	"a",   "-- ",   /* Ada         */
	"ada", "-- ",
	"asm", ";; ",	/* assembler (MS-DOS) */
	"bat", ":: ",	/* batch (MS-DOS) */
        "c",   " * ",   /* C           */
	"c++", "// ",	/* C++ in all its infinite guises */
	"cc",  "// ",
	"cpp", "// ",
	"cxx", "// ",
	"cl",  ";;; ",  /* Common Lisp */
	"cmd", ":: ",	/* command (OS/2) */
	"cmf", "c ",	/* CM Fortran  */
	"cs",  " * ",	/* C*          */
	"el",  "; ",    /* Emacs Lisp  */
	"f",   "c ",    /* Fortran     */
	"for", "c ",
        "h",   " * ",   /* C-header    */
	"hpp", "// ",	/* C++ header  */
	"hxx", "// ",
        "l",   " * ",   /* lex      NOTE: conflict between lex and franzlisp */
	"lisp",";;; ",	/* Lucid Lisp  */
	"lsp", ";; ",	/* Microsoft Lisp */
	"mac", ";; ",	/* macro (DEC-10, MS-DOS, PDP-11, VMS, etc) */
	"me",  ".\\\" ",/* me-macros   t/nroff*/
	"ml",  "; ",    /* mocklisp    */
	"mm",  ".\\\" ",/* mm-macros   t/nroff*/
	"ms",  ".\\\" ",/* ms-macros   t/nroff*/
	"p",   " * ",   /* Pascal      */
	"pas", " * ",
	"pl",  "% ",	/* Prolog      */
	"tex", "% ",	/* TeX	       */
        "y",   " * ",   /* yacc        */
	nil,   "# "     /* default for unknown suffix; must always be last */
};

#if has_mktemp
	static char const *
tmp()
/* Yield the name of the tmp directory.  */
{
	static char const *s;
	if (!s
		&&  !(s = cgetenv("TMPDIR"))	/* Unix tradition */
		&&  !(s = cgetenv("TMP"))	/* DOS tradition */
		&&  !(s = cgetenv("TEMP"))	/* another DOS tradition */
	)
		s = TMPDIR;
	return s;
}
#endif

	char const *
maketemp(n)
	int n;
/* Create a unique filename using n and the process id and store it
 * into the nth slot in tfnames.
 * Because of storage in tfnames, tempunlink() can unlink the file later.
 * Returns a pointer to the filename created.
 */
{
	char *p;
	char const *t = tfnames[n];

	if (t)
		return t;

	catchints();
	{
#	if has_mktemp
	    char const *tp = tmp();
	    p = testalloc(strlen(tp) + 10);
	    VOID sprintf(p, "%s%cT%cXXXXXX", tp, SLASH, '0'+n);
	    if (!mktemp(p) || !*p)
		faterror("can't make temporary file name `%s%cT%cXXXXXX'",
			tp, SLASH, '0'+n
		);
#	else
	    static char tfnamebuf[TEMPNAMES][L_tmpnam];
	    p = tfnamebuf[n];
	    if (!tmpnam(p) || !*p)
#		ifdef P_tmpdir
		    faterror("can't make temporary file name `%s...'",P_tmpdir);
#		else
		    faterror("can't make temporary file name");
#		endif
#	endif
	}

	tfnames[n] = p;
	return p;
}

	void
tempunlink()
/* Clean up maketemp() files.  May be invoked by signal handler.
 */
{
	register int i;
	register char *p;

	for (i = TEMPNAMES;  0 <= --i;  )
	    if ((p = tfnames[i])) {
		VOID unlink(p);
		/*
		 * We would tfree(p) here,
		 * but this might dump core if we're handing a signal.
		 * We're about to exit anyway, so we won't bother.
		 */
		tfnames[i] = 0;
	    }
}


	static char const *
bindex(sp,ch)
	register char const *sp;
	int ch;
/* Function: Finds the last occurrence of character c in string sp
 * and returns a pointer to the character just beyond it. If the
 * character doesn't occur in the string, sp is returned.
 */
{
	register char const c=ch, *r;
        r = sp;
        while (*sp) {
                if (*sp++ == c) r=sp;
        }
        return r;
}



	static int
suffix_matches(suffix, pattern)
	register char const *suffix, *pattern;
{
	register int c;
	if (!pattern)
		return true;
	for (;;)
		switch (*suffix++ - (c = *pattern++)) {
		    case 0:
			if (!c)
				return true;
			break;

		    case 'A'-'a':
			if (ctab[c] == Letter)
				break;
			/* fall into */
		    default:
			return false;
		}
}


	static void
InitAdmin()
/* function: initializes an admin node */
{
	register char const *Suffix;
        register int i;

	Head=nil; Dbranch=nil; AccessList=nil; Symbols=nil; Locks=nil;
        StrictLocks=STRICT_LOCKING;

        /* guess the comment leader from the suffix*/
        Suffix=bindex(workfilename, '.');
        if (Suffix==workfilename) Suffix= ""; /* empty suffix; will get default*/
	for (i=0; !suffix_matches(Suffix,comtable[i].suffix); i++)
		;
	Comment.string = comtable[i].comlead;
	Comment.size = strlen(comtable[i].comlead);
	Lexinit(); /* note: if !finptr, reads nothing; only initializes */
}


/* 'cpp' does not like this line. It seems to be the leading '_' in the */
/* second occurence of '_POSIX_NO_TRUNC'.  It evaluates correctly with  */
/* just the first term so lets just do that for now.                    */
/*#if defined(_POSIX_NO_TRUNC) && _POSIX_NO_TRUNC!=-1*/
#if defined(_POSIX_NO_TRUNC)
#	define LONG_NAMES_MAY_BE_SILENTLY_TRUNCATED 0
#else
#	define LONG_NAMES_MAY_BE_SILENTLY_TRUNCATED 1
#endif

#if LONG_NAMES_MAY_BE_SILENTLY_TRUNCATED
#ifdef NAME_MAX
#	define filenametoolong(path) (NAME_MAX < strlen(basename(path)))
#else
	static int
filenametoolong(path)
	char *path;
/* Yield true if the last file name in PATH is too long. */
{
	static unsigned long dot_namemax;

	register size_t namelen;
	register char *base;
	register unsigned long namemax;

	base = path + dirlen(path);
	namelen = strlen(base);
	if (namelen <= _POSIX_NAME_MAX) /* fast check for shorties */
		return false;
	if (base != path) {
		*--base = 0;
		namemax = pathconf(path, _PC_NAME_MAX);
		*base = SLASH;
	} else {
		/* Cache the results for the working directory, for speed. */
		if (!dot_namemax)
			dot_namemax = pathconf(".", _PC_NAME_MAX);
		namemax = dot_namemax;
	}
	/* If pathconf() yielded -1, namemax is now ULONG_MAX.  */
	return namemax<namelen;
}
#endif
#endif

	void
bufalloc(b, size)
	register struct buf *b;
	size_t size;
/* Ensure *B is a name buffer of at least SIZE bytes.
 * *B's old contents can be freed; *B's new contents are undefined.
 */
{
	if (b->size < size) {
		if (b->size)
			tfree(b->string);
		else
			b->size = sizeof(malloc_type);
		while (b->size < size)
			b->size <<= 1;
		b->string = tnalloc(char, b->size);
	}
}

	void
bufrealloc(b, size)
	register struct buf *b;
	size_t size;
/* like bufalloc, except *B's old contents, if any, are preserved */
{
	if (b->size < size) {
		if (!b->size)
			bufalloc(b, size);
		else {
			while ((b->size <<= 1)  <  size)
				;
			b->string = trealloc(char, b->string, b->size);
		}
	}
}

	void
bufautoend(b)
	struct buf *b;
/* Free an auto buffer at block exit. */
{
	if (b->size)
		tfree(b->string);
}

	struct cbuf
bufremember(b, s)
	struct buf *b;
	size_t s;
/*
 * Free the buffer B with used size S.
 * Yield a cbuf with identical contents.
 * The cbuf will be reclaimed when this input file is finished.
 */
{
	struct cbuf cb;

	if ((cb.size = s))
		cb.string = fremember(trealloc(char, b->string, s));
	else {
		bufautoend(b); /* not really auto */
		cb.string = "";
	}
	return cb;
}

	char *
bufenlarge(b, alim)
	register struct buf *b;
	char const **alim;
/* Make *B larger.  Set *ALIM to its new limit, and yield the relocated value
 * of its old limit.
 */
{
	size_t s = b->size;
	bufrealloc(b, s + 1);
	*alim = b->string + b->size;
	return b->string + s;
}

	void
bufscat(b, s)
	struct buf *b;
	char const *s;
/* Concatenate S to B's end. */
{
	size_t blen  =  b->string ? strlen(b->string) : 0;
	bufrealloc(b, blen+strlen(s)+1);
	VOID strcpy(b->string+blen, s);
}

	void
bufscpy(b, s)
	struct buf *b;
	char const *s;
/* Copy S into B. */
{
	bufalloc(b, strlen(s)+1);
	VOID strcpy(b->string, s);
}


	char const *
basename(p)
	char const *p;
/* Yield the address of the base filename of the pathname P.  */
{
	register char const *b = p, *q = p;
	for (;;)
	    switch (*q++) {
		case SLASHes: b = q; break;
		case 0: return b;
	    }
}

	size_t
dirlen(p)
	char const *p;
/* Yield the length of P's directory, including its trailing SLASH.  */
{
	return basename(p) - p;
}


	static size_t
suffixlen(x)
	char const *x;
/* Yield the length of X, an RCS filename suffix.  */
{
	register char const *p;

	p = x;
	for (;;)
	    switch (*p) {
		case 0: case SLASHes:
		    return p - x;

		default:
		    ++p;
		    continue;
	    }
}

	char const *
rcssuffix(name)
	char const *name;
/* Yield the suffix of NAME if it is an RCS filename, 0 otherwise.  */
{
	char const *x, *p, *nz;
	size_t dl, nl, xl;

	nl = strlen(name);
	nz = name + nl;
	x = suffixes;
	do {
	    if ((xl = suffixlen(x))) {
		if (xl <= nl  &&  memcmp(p = nz-xl, x, xl) == 0)
		    return p;
	    } else {
		dl = dirlen(name);
		if (
		    rcsdirlen < dl  &&
		    !memcmp(p = name+(dl-=rcsdirlen+1), rcsdir, rcsdirlen) &&
		    (!dl  ||  isSLASH(*--p))
		)
		    return nz;
	    }
	    x += xl;
	} while (*x++);
	return 0;
}

	/*ARGSUSED*/ RILE *
rcsreadopen(RCSname, status, mustread)
	struct buf *RCSname;
	struct stat *status;
	int mustread;
/* Open RCSNAME for reading and yield its FILE* descriptor.
 * If successful, set *STATUS to its status.
 * Pass this routine to pairfilenames() for read-only access to the file.  */
{
	return Iopen(RCSname->string, FOPEN_R, status);
}

	static int
finopen(rcsopen, mustread)
	RILE *(*rcsopen)P((struct buf*,struct stat*,int));
	int mustread;
/*
 * Use RCSOPEN to open an RCS file; MUSTREAD is set if the file must be read.
 * Set finptr to the result and yield true if successful.
 * RCSb holds the file's name.
 * Set RCSbuf to the best RCS name found so far, and RCSerrno to its errno.
 * Yield true if successful or if an unusual failure.
 */
{
	int interesting, preferold;

	/*
	 * We prefer an old name to that of a nonexisting new RCS file,
	 * unless we tried locking the old name and failed.
	 */
	preferold  =  RCSbuf.string[0] && (mustread||frewrite);

	finptr = (*rcsopen)(&RCSb, &RCSstat, mustread);
	interesting = finptr || errno!=ENOENT;
	if (interesting || !preferold) {
		/* Use the new name.  */
		RCSerrno = errno;
		bufscpy(&RCSbuf, RCSb.string);
	}
	return interesting;
}

	static int
fin2open(d, dlen, base, baselen, x, xlen, rcsopen, mustread)
	char const *d, *base, *x;
	size_t dlen, baselen, xlen;
	RILE *(*rcsopen)P((struct buf*,struct stat*,int));
	int mustread;
/*
 * D is a directory name with length DLEN (including trailing slash).
 * BASE is a filename with length BASELEN.
 * X is an RCS filename suffix with length XLEN.
 * Use RCSOPEN to open an RCS file; MUSTREAD is set if the file must be read.
 * Yield true if successful.
 * Try dRCS/basex first; if that fails and x is nonempty, try dbasex.
 * Put these potential names in RCSb.
 * Set RCSbuf to the best RCS name found so far, and RCSerrno to its errno.
 * Yield true if successful or if an unusual failure.
 */
{
	register char *p;

	bufalloc(&RCSb, dlen + rcsdirlen + 1 + baselen + xlen + 1);

	/* Try dRCS/basex.  */
	VOID memcpy(p = RCSb.string, d, dlen);
	VOID memcpy(p += dlen, rcsdir, rcsdirlen);
	p += rcsdirlen;
	*p++ = SLASH;
	VOID memcpy(p, base, baselen);
	VOID memcpy(p += baselen, x, xlen);
	p[xlen] = 0;
	if (xlen) {
	    if (finopen(rcsopen, mustread))
		return true;

	    /* Try dbasex.  */
	    /* Start from scratch, because finopen() may have changed RCSb.  */
	    VOID memcpy(p = RCSb.string, d, dlen);
	    VOID memcpy(p += dlen, base, baselen);
	    VOID memcpy(p += baselen, x, xlen);
	    p[xlen] = 0;
	}
	return finopen(rcsopen, mustread);
}

	int
pairfilenames(argc, argv, rcsopen, mustread, quiet)
	int argc;
	char **argv;
	RILE *(*rcsopen)P((struct buf*,struct stat*,int));
	int mustread, quiet;
/* Function: Pairs the filenames pointed to by argv; argc indicates
 * how many there are.
 * Places a pointer to the RCS filename into RCSfilename,
 * and a pointer to the name of the working file into workfilename.
 * If both the workfilename and the RCS filename are given, and workstdout
 * is set, a warning is printed.
 *
 * If the RCS file exists, places its status into RCSstat.
 *
 * If the RCS file exists, it is RCSOPENed for reading, the file pointer
 * is placed into finptr, and the admin-node is read in; returns 1.
 * If the RCS file does not exist and MUSTREAD,
 * print an error unless QUIET and return 0.
 * Otherwise, initialize the admin node and return -1.
 *
 * 0 is returned on all errors, e.g. files that are not regular files.
 */
{
	static struct buf tempbuf;

	register char *p, *arg, *RCS1;
	char const *purefname, *pureRCSname, *x;
	int paired;
	size_t arglen, dlen, baselen, xlen;

	if (!(arg = *argv)) return 0; /* already paired filename */
	if (*arg == '-') {
		error("%s option is ignored after file names", arg);
		return 0;
	}

	purefname = basename(arg);

	/* Allocate buffer temporary to hold the default paired file name. */
	p = arg;
	for (;;) {
		switch (*p++) {
		    /* Beware characters that cause havoc with ci -k. */
		    case KDELIM:
			error("RCS file name `%s' contains %c", arg, KDELIM);
			return 0;
		    case ' ': case '\n': case '\t':
			error("RCS file name `%s' contains white space", arg);
			return 0;
		    default:
			continue;
		    case 0:
			break;
		}
		break;
	}

	paired = false;

        /* first check suffix to see whether it is an RCS file or not */
	if ((x = rcssuffix(arg)))
	{
                /* RCS file name given*/
		RCS1 = arg;
		pureRCSname = purefname;
		baselen = x - purefname;
		if (
		    1 < argc  &&
		    !rcssuffix(workfilename = p = argv[1])  &&
		    baselen <= (arglen = strlen(p))  &&
		    ((p+=arglen-baselen) == workfilename  ||  isSLASH(p[-1])) &&
		    memcmp(purefname, p, baselen) == 0
		) {
			argv[1] = 0;
			paired = true;
		} else {
			bufscpy(&tempbuf, purefname);
			workfilename = p = tempbuf.string;
			p[baselen] = 0;
		}
        } else {
                /* working file given; now try to find RCS file */
		workfilename = arg;
		baselen = p - purefname - 1;
                /* derive RCS file name*/
		if (
		    1 < argc  &&
		    (x = rcssuffix(RCS1 = argv[1]))  &&
		    baselen  <=  x - RCS1  &&
		    ((pureRCSname=x-baselen)==RCS1 || isSLASH(pureRCSname[-1])) &&
		    memcmp(purefname, pureRCSname, baselen) == 0
		) {
			argv[1] = 0;
			paired = true;
		} else
			pureRCSname = RCS1 = 0;
        }
        /* now we have a (tentative) RCS filename in RCS1 and workfilename  */
        /* Second, try to find the right RCS file */
        if (pureRCSname!=RCS1) {
                /* a path for RCSfile is given; single RCS file to look for */
		bufscpy(&RCSbuf, RCS1);
		finptr = (*rcsopen)(&RCSbuf, &RCSstat, mustread);
		RCSerrno = errno;
        } else {
		bufscpy(&RCSbuf, "");
		if (RCS1)
			/* RCS file name was given without path.  */
			VOID fin2open(arg, (size_t)0, pureRCSname, baselen,
				x, strlen(x), rcsopen, mustread
			);
		else {
			/* No RCS file name was given.  */
			/* Try each suffix in turn.  */
			dlen = purefname-arg;
			x = suffixes;
			while (! fin2open(arg, dlen, purefname, baselen,
					x, xlen=suffixlen(x), rcsopen, mustread
			)) {
				x += xlen;
				if (!*x++)
					break;
			}
		}
        }
	RCSfilename = p = RCSbuf.string;
	if (finptr) {
		if (!S_ISREG(RCSstat.st_mode)) {
			error("%s isn't a regular file -- ignored", p);
                        return 0;
                }
                Lexinit(); getadmin();
	} else {
		if (RCSerrno!=ENOENT || mustread || !frewrite) {
			if (RCSerrno == EEXIST)
				error("RCS file %s is in use", p);
			else if (!quiet || RCSerrno!=ENOENT)
				enerror(RCSerrno, p);
			return 0;
		}
                InitAdmin();
        };
#	if LONG_NAMES_MAY_BE_SILENTLY_TRUNCATED
	    if (filenametoolong(p)) {
		error("RCS file name %s is too long", p);
		return 0;
	    }
#	    ifndef NAME_MAX
		/*
		 * Check workfilename too, even though it cannot be longer,
		 * because it may reside on a different filesystem.
		 */
		if (filenametoolong(workfilename)) {
		    error("working file name %s is too long", workfilename);
		    return 0;
		}
#	    endif
#	endif

	if (paired && workstdout)
                warn("Option -p is set; ignoring output file %s",workfilename);

	prevkeys = false;
	return finptr ? 1 : -1;
}


	char const *
getfullRCSname()
/* Function: returns a pointer to the full path name of the RCS file.
 * Gets the working directory's name at most once.
 * Removes leading "../" and "./".
 */
{
	static char const *wdptr;
	static struct buf rcsbuf, wdbuf;
	static size_t pathlength;

	register char const *realname;
	register size_t parentdirlength;
	register unsigned dotdotcounter;
	register char *d;
	register char const *wd;

	if (ROOTPATH(RCSfilename)) {
                return(RCSfilename);
        } else {
		if (!(wd = wdptr)) {
		    /* Get working directory for the first time.  */
		    if (!(d = cgetenv("PWD"))) {
			bufalloc(&wdbuf, SIZEABLE_PATH + 1);
#			if !has_getcwd && has_getwd
			    d = getwd(wdbuf.string);
#			else
			    while (
				    !(d = getcwd(wdbuf.string, wdbuf.size))
				&&  errno==ERANGE
			    )
				bufalloc(&wdbuf, wdbuf.size<<1);
#			endif
			if (!d)
			    efaterror("working directory");
		    }
		    parentdirlength = strlen(d);
		    while (parentdirlength && isSLASH(d[parentdirlength-1])) {
			d[--parentdirlength] = 0;
                        /* Check needed because some getwd implementations */
                        /* generate "/" for the root.                      */
                    }
		    wdptr = wd = d;
		    pathlength = parentdirlength;
                }
                /*the following must be redone since RCSfilename may change*/
		/* Find how many `../'s to remove from RCSfilename.  */
                dotdotcounter =0;
                realname = RCSfilename;
		while (realname[0]=='.') {
			if (isSLASH(realname[1])) {
                            /* drop leading ./ */
                            realname += 2;
			} else if (realname[1]=='.' && isSLASH(realname[2])) {
                            /* drop leading ../ and remember */
                            dotdotcounter++;
                            realname += 3;
			} else
			    break;
                }
		/* Now remove dotdotcounter trailing directories from wd. */
		parentdirlength = pathlength;
		while (dotdotcounter && parentdirlength) {
                    /* move pointer backwards over trailing directory */
		    if (isSLASH(wd[--parentdirlength])) {
                        dotdotcounter--;
                    }
                }
		/* build full path name */
		bufalloc(&rcsbuf, parentdirlength+strlen(realname)+2);
		d = rcsbuf.string;
		VOID memcpy(d, wd, parentdirlength);
		d += parentdirlength;
		*d++ = SLASH;
		VOID strcpy(d, realname);
		return rcsbuf.string;
        }
}

#ifndef isSLASH
	int
isSLASH(c)
	int c;
{
	switch (c) {
	    case SLASHes:
		return true;
	    default:
		return false;
	}
}
#endif


#if !has_getcwd && !has_getwd

	char *
getcwd(path, size)
	char *path;
	size_t size;
{
	static char const usrbinpwd[] = "/usr/bin/pwd";
#	define binpwd (usrbinpwd+4)

	register FILE *fp;
	register int c;
	register char *p, *lim;
	int closeerrno, closeerror, e, fd[2], readerror, toolong, wstatus;
	pid_t child;
#	if !has_waitpid
		pid_t w;
#	endif

	if (!size) {
		errno = EINVAL;
		return 0;
	}
	if (pipe(fd) != 0)
		return 0;
	if (!(child = vfork())) {
		if (
			close(fd[0]) == 0 &&
			(fd[1] == STDOUT_FILENO ||
#				ifdef F_DUPFD
					(VOID close(STDOUT_FILENO),
					fcntl(fd[1], F_DUPFD, STDOUT_FILENO))
#				else
					dup2(fd[1], STDOUT_FILENO)
#				endif
				== STDOUT_FILENO &&
				close(fd[1]) == 0
			)
		) {
			VOID close(STDERR_FILENO);
			VOID execl(binpwd, binpwd, (char *)0);
			VOID execl(usrbinpwd, usrbinpwd, (char *)0);
		}
		_exit(EXIT_FAILURE);
	}
	e = errno;
	closeerror = close(fd[1]);
	closeerrno = errno;
	fp = 0;
	readerror = toolong = wstatus = 0;
	p = path;
	if (0 <= child) {
		fp = fdopen(fd[0], "r");
		e = errno;
		if (fp) {
			lim = p + size;
			for (p = path;  ;  *p++ = c) {
				if ((c=getc(fp)) < 0) {
					if (feof(fp))
						break;
					if (ferror(fp)) {
						readerror = 1;
						e = errno;
						break;
					}
				}
				if (p == lim) {
					toolong = 1;
					break;
				}
			}
		}
#		if has_waitpid
			if (waitpid(child, &wstatus, 0) < 0)
				wstatus = 1;
#		else
			do {
				if ((w = wait(&wstatus)) < 0) {
					wstatus = 1;
					break;
				}
			} while (w != child);
#		endif
	}
	if (!fp) {
		VOID close(fd[0]);
		errno = e;
		return 0;
	}
	if (fclose(fp) != 0)
		return 0;
	if (readerror) {
		errno = e;
		return 0;
	}
	if (closeerror) {
		errno = closeerrno;
		return 0;
	}
	if (toolong) {
		errno = ERANGE;
		return 0;
	}
	if (wstatus  ||  p == path  ||  *--p != '\n') {
		errno = EACCES;
		return 0;
	}
	*p = '\0';
	return path;
}
#endif


#ifdef PAIRTEST
/* test program for pairfilenames() and getfullRCSname() */

char const cmdid[] = "pair";

main(argc, argv)
int argc; char *argv[];
{
        int result;
	int initflag;
	quietflag = initflag = false;

        while(--argc, ++argv, argc>=1 && ((*argv)[0] == '-')) {
                switch ((*argv)[1]) {

		case 'p':       workstdout = stdout;
                                break;
                case 'i':       initflag=true;
                                break;
                case 'q':       quietflag=true;
                                break;
                default:        error("unknown option: %s", *argv);
                                break;
                }
        }

        do {
                RCSfilename=workfilename=nil;
		result = pairfilenames(argc,argv,rcsreadopen,!initflag,quietflag);
                if (result!=0) {
		    diagnose("RCS file: %s; working file: %s\nFull RCS file name: %s\n",
			     RCSfilename,workfilename,getfullRCSname()
		    );
                }
                switch (result) {
                        case 0: continue; /* already paired file */

                        case 1: if (initflag) {
                                    error("RCS file %s exists already",RCSfilename);
                                } else {
				    diagnose("RCS file %s exists\n",RCSfilename);
                                }
				Ifclose(finptr);
                                break;

			case -1:diagnose("RCS file doesn't exist\n");
                                break;
                }

        } while (++argv, --argc>=1);

}

	exiting void
exiterr()
{
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}
#endif
