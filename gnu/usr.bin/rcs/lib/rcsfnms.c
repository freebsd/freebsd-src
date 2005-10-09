/* RCS filename and pathname handling */

/****************************************************************************
 *                     creation and deletion of /tmp temporaries
 *		       pairing of RCS pathnames and working pathnames.
 *                     Testprogram: define PAIRTEST
 ****************************************************************************
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
 * Revision 5.16  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.15  1995/06/01 16:23:43  eggert
 * (basefilename): Renamed from basename to avoid collisions.
 * (dirlen): Remove (for similar reasons).
 * (rcsreadopen): Open with FOPEN_RB.
 * (SLASHSLASH_is_SLASH): Default is 0.
 * (getcwd): Work around bad_wait_if_SIGCHLD_ignored bug.
 *
 * Revision 5.14  1994/03/17 14:05:48  eggert
 * Strip trailing SLASHes from TMPDIR; some systems need this.  Remove lint.
 *
 * Revision 5.13  1993/11/03 17:42:27  eggert
 * Determine whether a file name is too long indirectly,
 * by examining inode numbers, instead of trying to use operating system
 * primitives like pathconf, which are not trustworthy in general.
 * File names may now hold white space or $.
 * Do not flatten ../X in pathnames; that may yield wrong answer for symlinks.
 * Add getabsname hook.  Improve quality of diagnostics.
 *
 * Revision 5.12  1992/07/28  16:12:44  eggert
 * Add .sty.  .pl now implies Perl, not Prolog.  Fix fdlock initialization bug.
 * Check that $PWD is really ".".  Be consistent about pathnames vs filenames.
 *
 * Revision 5.11  1992/02/17  23:02:25  eggert
 * `a/RCS/b/c' is now an RCS file with an empty extension, not just `a/b/RCS/c'.
 *
 * Revision 5.10  1992/01/24  18:44:19  eggert
 * Fix bug: Expand and Ignored weren't reinitialized.
 * Avoid `char const c=ch;' compiler bug.
 * Add support for bad_creat0.
 *
 * Revision 5.9  1992/01/06  02:42:34  eggert
 * Shorten long (>31 chars) name.
 * while (E) ; -> while (E) continue;
 *
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
 * Modernize list of filename extensions.
 * Permit paths of arbitrary length.  Beware filenames beginning with "-".
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
 * Added check for KDELIM in filenames to pairfilenames().
 *
 * Revision 4.2  83/12/02  22:47:45  wft
 * Added csh, red, and sl filename suffixes.
 *
 * Revision 4.1  83/05/11  16:23:39  wft
 * Added initialization of Dbranch to InitAdmin(). Canged pairfilenames():
 * 1. added copying of path from workfile to RCS file, if RCS file is omitted;
 * 2. added getting the file status of RCS and working files;
 * 3. added ignoring of directories.
 *
 * Revision 3.7  83/05/11  15:01:58  wft
 * Added comtable[] which pairs filename suffixes with comment leaders;
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
 * Changed mktempfile() to store the generated filenames.
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

libId(fnmsId, "$FreeBSD$")

static char const *bindex P((char const*,int));
static int fin2open P((char const*, size_t, char const*, size_t, char const*, size_t, RILE*(*)P((struct buf*,struct stat*,int)), int));
static int finopen P((RILE*(*)P((struct buf*,struct stat*,int)), int));
static int suffix_matches P((char const*,char const*));
static size_t dir_useful_len P((char const*));
static size_t suffixlen P((char const*));
static void InitAdmin P((void));

char const *RCSname;
char *workname;
int fdlock;
FILE *workstdout;
struct stat RCSstat;
char const *suffixes;

static char const rcsdir[] = "RCS";
#define rcslen (sizeof(rcsdir)-1)

static struct buf RCSbuf, RCSb;
static int RCSerrno;


/* Temp names to be unlinked when done, if they are not 0.  */
#define TEMPNAMES 5 /* must be at least DIRTEMPNAMES (see rcsedit.c) */
static char *volatile tpnames[TEMPNAMES];


struct compair {
	char const *suffix, *comlead;
};

/*
* This table is present only for backwards compatibility.
* Normally we ignore this table, and use the prefix of the `$Log' line instead.
*/
static struct compair const comtable[] = {
	{ "a"	, "-- "	},	/* Ada */
	{ "ada"	, "-- "	},
	{ "adb"	, "-- "	},
	{ "ads"	, "-- "	},
	{ "asm"	, ";; "	},	/* assembler (MS-DOS) */
	{ "bat"	, ":: "	},	/* batch (MS-DOS) */
	{ "body", "-- "	},	/* Ada */
	{ "c"	, " * "	},	/* C */
	{ "c++"	, "// "	},	/* C++ in all its infinite guises */
	{ "cc"	, "// "	},
	{ "cpp"	, "// "	},
	{ "cxx"	, "// "	},
	{ "cl"	, ";;; "},	/* Common Lisp */
	{ "cmd"	, ":: "	},	/* command (OS/2) */
	{ "cmf"	, "c "	},	/* CM Fortran */
	{ "cs"	, " * "	},	/* C* */
	{ "el"	, "; "	},	/* Emacs Lisp */
	{ "f"	, "c "	},	/* Fortran */
	{ "for"	, "c "	},
	{ "h"	, " * "	},	/* C-header */
	{ "hpp"	, "// "	},	/* C++ header */
	{ "hxx"	, "// "	},
	{ "l"	, " * "	},	/* lex (NOTE: franzlisp disagrees) */
	{ "lisp", ";;; "},	/* Lucid Lisp */
	{ "lsp"	, ";; "	},	/* Microsoft Lisp */
	{ "m"   , "// " },	/* Objective C */
	{ "mac"	, ";; "	},	/* macro (DEC-10, MS-DOS, PDP-11, VMS, etc) */
	{ "me"	, ".\\\" "},	/* troff -me */
	{ "ml"	, "; "	},	/* mocklisp */
	{ "mm"	, ".\\\" "},	/* troff -mm */
	{ "ms"	, ".\\\" "},	/* troff -ms */
	{ "p"	, " * "	},	/* Pascal */
	{ "pas"	, " * "	},
	{ "ps"	, "% "	},	/* PostScript */
	{ "spec", "-- "	},	/* Ada */
	{ "sty"	, "% "	},	/* LaTeX style */
	{ "tex"	, "% "	},	/* TeX */
	{ "y"	, " * "	},	/* yacc */
	{ 0	, "# "	}	/* default for unknown suffix; must be last */
};

#if has_mktemp
	static char const *tmp P((void));
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
/* Create a unique pathname using n and the process id and store it
 * into the nth slot in tpnames.
 * Because of storage in tpnames, tempunlink() can unlink the file later.
 * Return a pointer to the pathname created.
 */
{
	char *p;
	char const *t = tpnames[n];
#	if has_mktemp
	int fd;
#	endif

	if (t)
		return t;

	catchints();
	{
#	if has_mktemp
	    char const *tp = tmp();
	    size_t tplen = dir_useful_len(tp);
	    p = testalloc(tplen + 10);
	    VOID sprintf(p, "%.*s%cT%cXXXXXX", (int)tplen, tp, SLASH, '0'+n);
	    fd = mkstemp(p);
	    if (fd < 0 || !*p)
		faterror("can't make temporary pathname `%.*s%cT%cXXXXXX'",
			(int)tplen, tp, SLASH, '0'+n
		);
	    close(fd);
#	else
	    static char tpnamebuf[TEMPNAMES][L_tmpnam];
	    p = tpnamebuf[n];
	    if (!tmpnam(p) || !*p)
#		ifdef P_tmpdir
		    faterror("can't make temporary pathname `%s...'",P_tmpdir);
#		else
		    faterror("can't make temporary pathname");
#		endif
#	endif
	}

	tpnames[n] = p;
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
	    if ((p = tpnames[i])) {
		VOID unlink(p);
		/*
		 * We would tfree(p) here,
		 * but this might dump core if we're handing a signal.
		 * We're about to exit anyway, so we won't bother.
		 */
		tpnames[i] = 0;
	    }
}


	static char const *
bindex(sp, c)
	register char const *sp;
	register int c;
/* Function: Finds the last occurrence of character c in string sp
 * and returns a pointer to the character just beyond it. If the
 * character doesn't occur in the string, sp is returned.
 */
{
	register char const *r;
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

	Head=0; Dbranch=0; AccessList=0; Symbols=0; Locks=0;
        StrictLocks=STRICT_LOCKING;

        /* guess the comment leader from the suffix*/
	Suffix = bindex(workname, '.');
	if (Suffix==workname) Suffix= ""; /* empty suffix; will get default*/
	for (i=0; !suffix_matches(Suffix,comtable[i].suffix); i++)
		continue;
	Comment.string = comtable[i].comlead;
	Comment.size = strlen(comtable[i].comlead);
	Expand = KEYVAL_EXPAND;
	clear_buf(&Ignored);
	Lexinit(); /* note: if !finptr, reads nothing; only initializes */
}



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
				continue;
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
basefilename(p)
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


	static size_t
suffixlen(x)
	char const *x;
/* Yield the length of X, an RCS pathname suffix.  */
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
/* Yield the suffix of NAME if it is an RCS pathname, 0 otherwise.  */
{
	char const *x, *p, *nz;
	size_t nl, xl;

	nl = strlen(name);
	nz = name + nl;
	x = suffixes;
	do {
	    if ((xl = suffixlen(x))) {
		if (xl <= nl  &&  memcmp(p = nz-xl, x, xl) == 0)
		    return p;
	    } else
		for (p = name;  p < nz - rcslen;  p++)
		    if (
			isSLASH(p[rcslen])
			&& (p==name || isSLASH(p[-1]))
			&& memcmp(p, rcsdir, rcslen) == 0
		    )
			return nz;
	    x += xl;
	} while (*x++);
	return 0;
}

	/*ARGSUSED*/ RILE *
rcsreadopen(RCSpath, status, mustread)
	struct buf *RCSpath;
	struct stat *status;
	int mustread;
/* Open RCSPATH for reading and yield its FILE* descriptor.
 * If successful, set *STATUS to its status.
 * Pass this routine to pairnames() for read-only access to the file.  */
{
	return Iopen(RCSpath->string, FOPEN_RB, status);
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
	preferold  =  RCSbuf.string[0] && (mustread||0<=fdlock);

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
 * X is an RCS pathname suffix with length XLEN.
 * Use RCSOPEN to open an RCS file; MUSTREAD is set if the file must be read.
 * Yield true if successful.
 * Try dRCS/basex first; if that fails and x is nonempty, try dbasex.
 * Put these potential names in RCSb.
 * Set RCSbuf to the best RCS name found so far, and RCSerrno to its errno.
 * Yield true if successful or if an unusual failure.
 */
{
	register char *p;

	bufalloc(&RCSb, dlen + rcslen + 1 + baselen + xlen + 1);

	/* Try dRCS/basex.  */
	VOID memcpy(p = RCSb.string, d, dlen);
	VOID memcpy(p += dlen, rcsdir, rcslen);
	p += rcslen;
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
pairnames(argc, argv, rcsopen, mustread, quiet)
	int argc;
	char **argv;
	RILE *(*rcsopen)P((struct buf*,struct stat*,int));
	int mustread, quiet;
/*
 * Pair the pathnames pointed to by argv; argc indicates
 * how many there are.
 * Place a pointer to the RCS pathname into RCSname,
 * and a pointer to the pathname of the working file into workname.
 * If both are given, and workstdout
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
	char const *base, *RCSbase, *x;
	int paired;
	size_t arglen, dlen, baselen, xlen;

	fdlock = -1;

	if (!(arg = *argv)) return 0; /* already paired pathname */
	if (*arg == '-') {
		error("%s option is ignored after pathnames", arg);
		return 0;
	}

	base = basefilename(arg);
	paired = false;

        /* first check suffix to see whether it is an RCS file or not */
	if ((x = rcssuffix(arg)))
	{
		/* RCS pathname given */
		RCS1 = arg;
		RCSbase = base;
		baselen = x - base;
		if (
		    1 < argc  &&
		    !rcssuffix(workname = p = argv[1])  &&
		    baselen <= (arglen = strlen(p))  &&
		    ((p+=arglen-baselen) == workname  ||  isSLASH(p[-1])) &&
		    memcmp(base, p, baselen) == 0
		) {
			argv[1] = 0;
			paired = true;
		} else {
			bufscpy(&tempbuf, base);
			workname = p = tempbuf.string;
			p[baselen] = 0;
		}
        } else {
                /* working file given; now try to find RCS file */
		workname = arg;
		baselen = strlen(base);
		/* Derive RCS pathname.  */
		if (
		    1 < argc  &&
		    (x = rcssuffix(RCS1 = argv[1]))  &&
		    baselen  <=  x - RCS1  &&
		    ((RCSbase=x-baselen)==RCS1 || isSLASH(RCSbase[-1])) &&
		    memcmp(base, RCSbase, baselen) == 0
		) {
			argv[1] = 0;
			paired = true;
		} else
			RCSbase = RCS1 = 0;
        }
	/* Now we have a (tentative) RCS pathname in RCS1 and workname.  */
        /* Second, try to find the right RCS file */
	if (RCSbase!=RCS1) {
                /* a path for RCSfile is given; single RCS file to look for */
		bufscpy(&RCSbuf, RCS1);
		finptr = (*rcsopen)(&RCSbuf, &RCSstat, mustread);
		RCSerrno = errno;
        } else {
		bufscpy(&RCSbuf, "");
		if (RCS1)
			/* RCS filename was given without path.  */
			VOID fin2open(arg, (size_t)0, RCSbase, baselen,
				x, strlen(x), rcsopen, mustread
			);
		else {
			/* No RCS pathname was given.  */
			/* Try each suffix in turn.  */
			dlen = base-arg;
			x = suffixes;
			while (! fin2open(arg, dlen, base, baselen,
					x, xlen=suffixlen(x), rcsopen, mustread
			)) {
				x += xlen;
				if (!*x++)
					break;
			}
		}
        }
	RCSname = p = RCSbuf.string;
	if (finptr) {
		if (!S_ISREG(RCSstat.st_mode)) {
			error("%s isn't a regular file -- ignored", p);
                        return 0;
                }
                Lexinit(); getadmin();
	} else {
		if (RCSerrno!=ENOENT || mustread || fdlock<0) {
			if (RCSerrno == EEXIST)
				error("RCS file %s is in use", p);
			else if (!quiet || RCSerrno!=ENOENT)
				enerror(RCSerrno, p);
			return 0;
		}
                InitAdmin();
        };

	if (paired && workstdout)
		workwarn("Working file ignored due to -p option");

	prevkeys = false;
	return finptr ? 1 : -1;
}


	char const *
getfullRCSname()
/*
 * Return a pointer to the full pathname of the RCS file.
 * Remove leading `./'.
 */
{
	if (ROOTPATH(RCSname)) {
	    return RCSname;
	} else {
	    static struct buf rcsbuf;
#	    if needs_getabsname
		bufalloc(&rcsbuf, SIZEABLE_PATH + 1);
		while (getabsname(RCSname, rcsbuf.string, rcsbuf.size) != 0)
		    if (errno == ERANGE)
			bufalloc(&rcsbuf, rcsbuf.size<<1);
		    else
			efaterror("getabsname");
#	    else
		static char const *wdptr;
		static struct buf wdbuf;
		static size_t wdlen;

		register char const *r;
		register size_t dlen;
		register char *d;
		register char const *wd;

		if (!(wd = wdptr)) {
		    /* Get working directory for the first time.  */
		    char *PWD = cgetenv("PWD");
		    struct stat PWDstat, dotstat;
		    if (! (
			(d = PWD) &&
			ROOTPATH(PWD) &&
			stat(PWD, &PWDstat) == 0 &&
			stat(".", &dotstat) == 0 &&
			same_file(PWDstat, dotstat, 1)
		    )) {
			bufalloc(&wdbuf, SIZEABLE_PATH + 1);
#			if has_getcwd || !has_getwd
			    while (!(d = getcwd(wdbuf.string, wdbuf.size)))
				if (errno == ERANGE)
				    bufalloc(&wdbuf, wdbuf.size<<1);
				else if ((d = PWD))
				    break;
				else
				    efaterror("getcwd");
#			else
			    d = getwd(wdbuf.string);
			    if (!d  &&  !(d = PWD))
				efaterror("getwd");
#			endif
		    }
		    wdlen = dir_useful_len(d);
		    d[wdlen] = 0;
		    wdptr = wd = d;
                }
		/*
		* Remove leading `./'s from RCSname.
		* Do not try to handle `../', since removing it may yield
		* the wrong answer in the presence of symbolic links.
		*/
		for (r = RCSname;  r[0]=='.' && isSLASH(r[1]);  r += 2)
		    /* `.////' is equivalent to `./'.  */
		    while (isSLASH(r[2]))
			r++;
		/* Build full pathname.  */
		dlen = wdlen;
		bufalloc(&rcsbuf, dlen + strlen(r) + 2);
		d = rcsbuf.string;
		VOID memcpy(d, wd, dlen);
		d += dlen;
		*d++ = SLASH;
		VOID strcpy(d, r);
#	    endif
	    return rcsbuf.string;
        }
}

/* Derived from code from the XFree86 project */
	char const *
getfullCVSname()
/* Function: returns a pointer to the path name of the RCS file with the
 * CVSROOT part stripped off, and with 'Attic/' stripped off (if present).
 */
{

#define ATTICDIR "/Attic"

	char const *namebuf = getfullRCSname();
	char *cvsroot = cgetenv("CVSROOT");
	int cvsrootlen;
	char *c = NULL;
	int alen = strlen(ATTICDIR);

	if ((c = strrchr(namebuf, '/')) != NULL) {
	    if (namebuf - c >= alen) {
		if (!strncmp(c - alen, ATTICDIR, alen)) {
		    while(*c != '\0') {
			*(c - alen) = *c;
			c++;
		    }
		    *(c - alen) = '\0';
		}
	    }
	}
	
	if (!cvsroot)
	    return(namebuf);
	else
	{
	    cvsrootlen = strlen(cvsroot);
	    if (!strncmp(namebuf, cvsroot, cvsrootlen) &&
	        namebuf[cvsrootlen] == '/')
		return(namebuf + cvsrootlen + 1);
	    else
		return(namebuf);
	}
}

	static size_t
dir_useful_len(d)
	char const *d;
/*
* D names a directory; yield the number of characters of D's useful part.
* To create a file in D, append a SLASH and a file name to D's useful part.
* Ignore trailing slashes if possible; not only are they ugly,
* but some non-Posix systems misbehave unless the slashes are omitted.
*/
{
#	ifndef SLASHSLASH_is_SLASH
#	define SLASHSLASH_is_SLASH 0
#	endif
	size_t dlen = strlen(d);
	if (!SLASHSLASH_is_SLASH && dlen==2 && isSLASH(d[0]) && isSLASH(d[1]))
	    --dlen;
	else
	    while (dlen && isSLASH(d[dlen-1]))
		--dlen;
	return dlen;
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

	if (!size) {
		errno = EINVAL;
		return 0;
	}
	if (pipe(fd) != 0)
		return 0;
#	if bad_wait_if_SIGCHLD_ignored
#		ifndef SIGCHLD
#		define SIGCHLD SIGCLD
#		endif
		VOID signal(SIGCHLD, SIG_DFL);
#	endif
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
			{
				pid_t w;
				do {
					if ((w = wait(&wstatus)) < 0) {
						wstatus = 1;
						break;
					}
				} while (w != child);
			}
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
/* test program for pairnames() and getfullRCSname() */

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
		RCSname = workname = 0;
		result = pairnames(argc,argv,rcsreadopen,!initflag,quietflag);
                if (result!=0) {
		    diagnose("RCS pathname: %s; working pathname: %s\nFull RCS pathname: %s\n",
			     RCSname, workname, getfullRCSname()
		    );
                }
                switch (result) {
                        case 0: continue; /* already paired file */

                        case 1: if (initflag) {
				    rcserror("already exists");
                                } else {
				    diagnose("RCS file %s exists\n", RCSname);
                                }
				Ifclose(finptr);
                                break;

			case -1:diagnose("RCS file doesn't exist\n");
                                break;
                }

        } while (++argv, --argc>=1);

}

	void
exiterr()
{
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}
#endif
