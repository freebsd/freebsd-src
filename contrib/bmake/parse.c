/*	$NetBSD: parse.c,v 1.526 2021/01/10 21:20:46 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Parsing of makefiles.
 *
 * Parse_File is the main entry point and controls most of the other
 * functions in this module.
 *
 * The directories for the .include "..." directive are kept in
 * 'parseIncPath', while those for .include <...> are kept in 'sysIncPath'.
 * The targets currently being defined are kept in 'targets'.
 *
 * Interface:
 *	Parse_Init	Initialize the module
 *
 *	Parse_End	Clean up the module
 *
 *	Parse_File	Parse a top-level makefile.  Included files are
 *			handled by Parse_include_file though.
 *
 *	Parse_IsVar	Return TRUE if the given line is a variable
 *			assignment. Used by MainParseArgs to determine if
 *			an argument is a target or a variable assignment.
 *			Used internally for pretty much the same thing.
 *
 *	Parse_Error	Report a parse error, a warning or an informational
 *			message.
 *
 *	Parse_MainName	Returns a list of the main target to create.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

#include "make.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>

#ifndef MAP_COPY
#define MAP_COPY MAP_PRIVATE
#endif
#ifndef MAP_FILE
#define MAP_FILE 0
#endif
#endif

#include "dir.h"
#include "job.h"
#include "pathnames.h"

/*	"@(#)parse.c	8.3 (Berkeley) 3/19/94"	*/
MAKE_RCSID("$NetBSD: parse.c,v 1.526 2021/01/10 21:20:46 rillig Exp $");

/* types and constants */

/*
 * Structure for a file being read ("included file")
 */
typedef struct IFile {
	char *fname;		/* name of file (relative? absolute?) */
	Boolean fromForLoop;	/* simulated .include by the .for loop */
	int lineno;		/* current line number in file */
	int first_lineno;	/* line number of start of text */
	unsigned int cond_depth; /* 'if' nesting when file opened */
	Boolean depending;	/* state of doing_depend on EOF */

	/* The buffer from which the file's content is read. */
	char *buf_freeIt;
	char *buf_ptr;		/* next char to be read */
	char *buf_end;

	/* Function to read more data, with a single opaque argument. */
	ReadMoreProc readMore;
	void *readMoreArg;

	struct loadedfile *lf;	/* loadedfile object, if any */
} IFile;

/*
 * Tokens for target attributes
 */
typedef enum ParseSpecial {
	SP_ATTRIBUTE,	/* Generic attribute */
	SP_BEGIN,	/* .BEGIN */
	SP_DEFAULT,	/* .DEFAULT */
	SP_DELETE_ON_ERROR, /* .DELETE_ON_ERROR */
	SP_END,		/* .END */
	SP_ERROR,	/* .ERROR */
	SP_IGNORE,	/* .IGNORE */
	SP_INCLUDES,	/* .INCLUDES; not mentioned in the manual page */
	SP_INTERRUPT,	/* .INTERRUPT */
	SP_LIBS,	/* .LIBS; not mentioned in the manual page */
	/* .MAIN and we don't have anything user-specified to make */
	SP_MAIN,
	SP_META,	/* .META */
	SP_MFLAGS,	/* .MFLAGS or .MAKEFLAGS */
	SP_NOMETA,	/* .NOMETA */
	SP_NOMETA_CMP,	/* .NOMETA_CMP */
	SP_NOPATH,	/* .NOPATH */
	SP_NOT,		/* Not special */
	SP_NOTPARALLEL,	/* .NOTPARALLEL or .NO_PARALLEL */
	SP_NULL,	/* .NULL; not mentioned in the manual page */
	SP_OBJDIR,	/* .OBJDIR */
	SP_ORDER,	/* .ORDER */
	SP_PARALLEL,	/* .PARALLEL; not mentioned in the manual page */
	SP_PATH,	/* .PATH or .PATH.suffix */
	SP_PHONY,	/* .PHONY */
#ifdef POSIX
	SP_POSIX,	/* .POSIX; not mentioned in the manual page */
#endif
	SP_PRECIOUS,	/* .PRECIOUS */
	SP_SHELL,	/* .SHELL */
	SP_SILENT,	/* .SILENT */
	SP_SINGLESHELL,	/* .SINGLESHELL; not mentioned in the manual page */
	SP_STALE,	/* .STALE */
	SP_SUFFIXES,	/* .SUFFIXES */
	SP_WAIT		/* .WAIT */
} ParseSpecial;

typedef List SearchPathList;
typedef ListNode SearchPathListNode;

/* result data */

/*
 * The main target to create. This is the first target on the first
 * dependency line in the first makefile.
 */
static GNode *mainNode;

/* eval state */

/*
 * During parsing, the targets from the left-hand side of the currently
 * active dependency line, or NULL if the current line does not belong to a
 * dependency line, for example because it is a variable assignment.
 *
 * See unit-tests/deptgt.mk, keyword "parse.c:targets".
 */
static GNodeList *targets;

#ifdef CLEANUP
/*
 * All shell commands for all targets, in no particular order and possibly
 * with duplicates.  Kept in a separate list since the commands from .USE or
 * .USEBEFORE nodes are shared with other GNodes, thereby giving up the
 * easily understandable ownership over the allocated strings.
 */
static StringList targCmds = LST_INIT;
#endif

/*
 * Predecessor node for handling .ORDER. Initialized to NULL when .ORDER
 * seen, then set to each successive source on the line.
 */
static GNode *order_pred;

/* parser state */

/* number of fatal errors */
static int fatals = 0;

/*
 * Variables for doing includes
 */

/*
 * The include chain of makefiles.  At the bottom is the top-level makefile
 * from the command line, and on top of that, there are the included files or
 * .for loops, up to and including the current file.
 *
 * This data could be used to print stack traces on parse errors.  As of
 * 2020-09-14, this is not done though.  It seems quite simple to print the
 * tuples (fname:lineno:fromForLoop), from top to bottom.  This simple idea is
 * made complicated by the fact that the .for loops also use this stack for
 * storing information.
 *
 * The lineno fields of the IFiles with fromForLoop == TRUE look confusing,
 * which is demonstrated by the test 'include-main.mk'.  They seem sorted
 * backwards since they tell the number of completely parsed lines, which for
 * a .for loop is right after the terminating .endfor.  To compensate for this
 * confusion, there is another field first_lineno pointing at the start of the
 * .for loop, 1-based for human consumption.
 *
 * To make the stack trace intuitive, the entry below the first .for loop must
 * be ignored completely since neither its lineno nor its first_lineno is
 * useful.  Instead, the topmost of each chain of .for loop needs to be
 * printed twice, once with its first_lineno and once with its lineno.
 *
 * As of 2020-10-28, using the above rules, the stack trace for the .info line
 * in include-subsub.mk would be:
 *
 *	includes[5]:	include-subsub.mk:4
 *			(lineno, from an .include)
 *	includes[4]:	include-sub.mk:32
 *			(lineno, from a .for loop below an .include)
 *	includes[4]:	include-sub.mk:31
 *			(first_lineno, from a .for loop, lineno == 32)
 *	includes[3]:	include-sub.mk:30
 *			(first_lineno, from a .for loop, lineno == 33)
 *	includes[2]:	include-sub.mk:29
 *			(first_lineno, from a .for loop, lineno == 34)
 *	includes[1]:	include-sub.mk:35
 *			(not printed since it is below a .for loop)
 *	includes[0]:	include-main.mk:27
 */
static Vector /* of IFile */ includes;

static IFile *
GetInclude(size_t i)
{
	return Vector_Get(&includes, i);
}

/* The file that is currently being read. */
static IFile *
CurFile(void)
{
	return GetInclude(includes.len - 1);
}

/* include paths */
SearchPath *parseIncPath;	/* dirs for "..." includes */
SearchPath *sysIncPath;		/* dirs for <...> includes */
SearchPath *defSysIncPath;	/* default for sysIncPath */

/* parser tables */

/*
 * The parseKeywords table is searched using binary search when deciding
 * if a target or source is special. The 'spec' field is the ParseSpecial
 * type of the keyword (SP_NOT if the keyword isn't special as a target) while
 * the 'op' field is the operator to apply to the list of targets if the
 * keyword is used as a source ("0" if the keyword isn't special as a source)
 */
static const struct {
	const char *name;	/* Name of keyword */
	ParseSpecial spec;	/* Type when used as a target */
	GNodeType op;		/* Operator when used as a source */
} parseKeywords[] = {
    { ".BEGIN",		SP_BEGIN,	OP_NONE },
    { ".DEFAULT",	SP_DEFAULT,	OP_NONE },
    { ".DELETE_ON_ERROR", SP_DELETE_ON_ERROR, OP_NONE },
    { ".END",		SP_END,		OP_NONE },
    { ".ERROR",		SP_ERROR,	OP_NONE },
    { ".EXEC",		SP_ATTRIBUTE,	OP_EXEC },
    { ".IGNORE",	SP_IGNORE,	OP_IGNORE },
    { ".INCLUDES",	SP_INCLUDES,	OP_NONE },
    { ".INTERRUPT",	SP_INTERRUPT,	OP_NONE },
    { ".INVISIBLE",	SP_ATTRIBUTE,	OP_INVISIBLE },
    { ".JOIN",		SP_ATTRIBUTE,	OP_JOIN },
    { ".LIBS",		SP_LIBS,	OP_NONE },
    { ".MADE",		SP_ATTRIBUTE,	OP_MADE },
    { ".MAIN",		SP_MAIN,	OP_NONE },
    { ".MAKE",		SP_ATTRIBUTE,	OP_MAKE },
    { ".MAKEFLAGS",	SP_MFLAGS,	OP_NONE },
    { ".META",		SP_META,	OP_META },
    { ".MFLAGS",	SP_MFLAGS,	OP_NONE },
    { ".NOMETA",	SP_NOMETA,	OP_NOMETA },
    { ".NOMETA_CMP",	SP_NOMETA_CMP,	OP_NOMETA_CMP },
    { ".NOPATH",	SP_NOPATH,	OP_NOPATH },
    { ".NOTMAIN",	SP_ATTRIBUTE,	OP_NOTMAIN },
    { ".NOTPARALLEL",	SP_NOTPARALLEL,	OP_NONE },
    { ".NO_PARALLEL",	SP_NOTPARALLEL,	OP_NONE },
    { ".NULL",		SP_NULL,	OP_NONE },
    { ".OBJDIR",	SP_OBJDIR,	OP_NONE },
    { ".OPTIONAL",	SP_ATTRIBUTE,	OP_OPTIONAL },
    { ".ORDER",		SP_ORDER,	OP_NONE },
    { ".PARALLEL",	SP_PARALLEL,	OP_NONE },
    { ".PATH",		SP_PATH,	OP_NONE },
    { ".PHONY",		SP_PHONY,	OP_PHONY },
#ifdef POSIX
    { ".POSIX",		SP_POSIX,	OP_NONE },
#endif
    { ".PRECIOUS",	SP_PRECIOUS,	OP_PRECIOUS },
    { ".RECURSIVE",	SP_ATTRIBUTE,	OP_MAKE },
    { ".SHELL",		SP_SHELL,	OP_NONE },
    { ".SILENT",	SP_SILENT,	OP_SILENT },
    { ".SINGLESHELL",	SP_SINGLESHELL,	OP_NONE },
    { ".STALE",		SP_STALE,	OP_NONE },
    { ".SUFFIXES",	SP_SUFFIXES,	OP_NONE },
    { ".USE",		SP_ATTRIBUTE,	OP_USE },
    { ".USEBEFORE",	SP_ATTRIBUTE,	OP_USEBEFORE },
    { ".WAIT",		SP_WAIT,	OP_NONE },
};

/* file loader */

struct loadedfile {
	/* XXX: What is the lifetime of this path? Who manages the memory? */
	const char *path;	/* name, for error reports */
	char *buf;		/* contents buffer */
	size_t len;		/* length of contents */
	Boolean used;		/* XXX: have we used the data yet */
};

/* XXX: What is the lifetime of the path? Who manages the memory? */
static struct loadedfile *
loadedfile_create(const char *path, char *buf, size_t buflen)
{
	struct loadedfile *lf;

	lf = bmake_malloc(sizeof *lf);
	lf->path = path == NULL ? "(stdin)" : path;
	lf->buf = buf;
	lf->len = buflen;
	lf->used = FALSE;
	return lf;
}

static void
loadedfile_destroy(struct loadedfile *lf)
{
	free(lf->buf);
	free(lf);
}

/*
 * readMore() operation for loadedfile, as needed by the weird and twisted
 * logic below. Once that's cleaned up, we can get rid of lf->used.
 */
static char *
loadedfile_readMore(void *x, size_t *len)
{
	struct loadedfile *lf = x;

	if (lf->used)
		return NULL;

	lf->used = TRUE;
	*len = lf->len;
	return lf->buf;
}

/*
 * Try to get the size of a file.
 */
static Boolean
load_getsize(int fd, size_t *ret)
{
	struct stat st;

	if (fstat(fd, &st) < 0)
		return FALSE;

	if (!S_ISREG(st.st_mode))
		return FALSE;

	/*
	 * st_size is an off_t, which is 64 bits signed; *ret is
	 * size_t, which might be 32 bits unsigned or 64 bits
	 * unsigned. Rather than being elaborate, just punt on
	 * files that are more than 1 GiB. We should never
	 * see a makefile that size in practice.
	 *
	 * While we're at it reject negative sizes too, just in case.
	 */
	if (st.st_size < 0 || st.st_size > 0x3fffffff)
		return FALSE;

	*ret = (size_t)st.st_size;
	return TRUE;
}

/*
 * Read in a file.
 *
 * Until the path search logic can be moved under here instead of
 * being in the caller in another source file, we need to have the fd
 * passed in already open. Bleh.
 *
 * If the path is NULL, use stdin.
 */
static struct loadedfile *
loadfile(const char *path, int fd)
{
	ssize_t n;
	Buffer buf;
	size_t filesize;


	if (path == NULL) {
		assert(fd == -1);
		fd = STDIN_FILENO;
	}

	if (load_getsize(fd, &filesize)) {
		/*
		 * Avoid resizing the buffer later for no reason.
		 *
		 * At the same time leave space for adding a final '\n',
		 * just in case it is missing in the file.
		 */
		filesize++;
	} else
		filesize = 1024;
	Buf_InitSize(&buf, filesize);

	for (;;) {
		assert(buf.len <= buf.cap);
		if (buf.len == buf.cap) {
			if (buf.cap > 0x1fffffff) {
				errno = EFBIG;
				Error("%s: file too large", path);
				exit(2); /* Not 1 so -q can distinguish error */
			}
			Buf_Expand(&buf);
		}
		assert(buf.len < buf.cap);
		n = read(fd, buf.data + buf.len, buf.cap - buf.len);
		if (n < 0) {
			Error("%s: read error: %s", path, strerror(errno));
			exit(2);	/* Not 1 so -q can distinguish error */
		}
		if (n == 0)
			break;

		buf.len += (size_t)n;
	}
	assert(buf.len <= buf.cap);

	if (!Buf_EndsWith(&buf, '\n'))
		Buf_AddByte(&buf, '\n');

	if (path != NULL)
		close(fd);

	{
		struct loadedfile *lf = loadedfile_create(path,
		    buf.data, buf.len);
		Buf_Destroy(&buf, FALSE);
		return lf;
	}
}

/* old code */

/* Check if the current character is escaped on the current line. */
static Boolean
ParseIsEscaped(const char *line, const char *c)
{
	Boolean active = FALSE;
	for (;;) {
		if (line == c)
			return active;
		if (*--c != '\\')
			return active;
		active = !active;
	}
}

/*
 * Add the filename and lineno to the GNode so that we remember where it
 * was first defined.
 */
static void
ParseMark(GNode *gn)
{
	IFile *curFile = CurFile();
	gn->fname = curFile->fname;
	gn->lineno = curFile->lineno;
}

/*
 * Look in the table of keywords for one matching the given string.
 * Return the index of the keyword, or -1 if it isn't there.
 */
static int
ParseFindKeyword(const char *str)
{
	int start = 0;
	int end = sizeof parseKeywords / sizeof parseKeywords[0] - 1;

	do {
		int curr = start + (end - start) / 2;
		int diff = strcmp(str, parseKeywords[curr].name);

		if (diff == 0)
			return curr;
		if (diff < 0)
			end = curr - 1;
		else
			start = curr + 1;
	} while (start <= end);

	return -1;
}

static void
PrintLocation(FILE *f, const char *fname, size_t lineno)
{
	char dirbuf[MAXPATHLEN + 1];
	FStr dir, base;

	if (*fname == '/' || strcmp(fname, "(stdin)") == 0) {
		(void)fprintf(f, "\"%s\" line %u: ", fname, (unsigned)lineno);
		return;
	}

	/* Find out which makefile is the culprit.
	 * We try ${.PARSEDIR} and apply realpath(3) if not absolute. */

	dir = Var_Value(".PARSEDIR", VAR_GLOBAL);
	if (dir.str == NULL)
		dir.str = ".";
	if (dir.str[0] != '/')
		dir.str = realpath(dir.str, dirbuf);

	base = Var_Value(".PARSEFILE", VAR_GLOBAL);
	if (base.str == NULL)
		base.str = str_basename(fname);

	(void)fprintf(f, "\"%s/%s\" line %u: ",
	    dir.str, base.str, (unsigned)lineno);

	FStr_Done(&base);
	FStr_Done(&dir);
}

static void
ParseVErrorInternal(FILE *f, const char *fname, size_t lineno,
		    ParseErrorLevel type, const char *fmt, va_list ap)
{
	static Boolean fatal_warning_error_printed = FALSE;

	(void)fprintf(f, "%s: ", progname);

	if (fname != NULL)
		PrintLocation(f, fname, lineno);
	if (type == PARSE_WARNING)
		(void)fprintf(f, "warning: ");
	(void)vfprintf(f, fmt, ap);
	(void)fprintf(f, "\n");
	(void)fflush(f);

	if (type == PARSE_INFO)
		return;
	if (type == PARSE_FATAL || opts.parseWarnFatal)
		fatals++;
	if (opts.parseWarnFatal && !fatal_warning_error_printed) {
		Error("parsing warnings being treated as errors");
		fatal_warning_error_printed = TRUE;
	}
}

static void
ParseErrorInternal(const char *fname, size_t lineno,
		   ParseErrorLevel type, const char *fmt, ...)
{
	va_list ap;

	(void)fflush(stdout);
	va_start(ap, fmt);
	ParseVErrorInternal(stderr, fname, lineno, type, fmt, ap);
	va_end(ap);

	if (opts.debug_file != stderr && opts.debug_file != stdout) {
		va_start(ap, fmt);
		ParseVErrorInternal(opts.debug_file, fname, lineno, type,
		    fmt, ap);
		va_end(ap);
	}
}

/*
 * Print a parse error message, including location information.
 *
 * If the level is PARSE_FATAL, continue parsing until the end of the
 * current top-level makefile, then exit (see Parse_File).
 *
 * Fmt is given without a trailing newline.
 */
void
Parse_Error(ParseErrorLevel type, const char *fmt, ...)
{
	va_list ap;
	const char *fname;
	size_t lineno;

	if (includes.len == 0) {
		fname = NULL;
		lineno = 0;
	} else {
		IFile *curFile = CurFile();
		fname = curFile->fname;
		lineno = (size_t)curFile->lineno;
	}

	va_start(ap, fmt);
	(void)fflush(stdout);
	ParseVErrorInternal(stderr, fname, lineno, type, fmt, ap);
	va_end(ap);

	if (opts.debug_file != stderr && opts.debug_file != stdout) {
		va_start(ap, fmt);
		ParseVErrorInternal(opts.debug_file, fname, lineno, type,
		    fmt, ap);
		va_end(ap);
	}
}


/*
 * Parse and handle a .info, .warning or .error directive.
 * For an .error directive, immediately exit.
 */
static void
ParseMessage(ParseErrorLevel level, const char *levelName, const char *umsg)
{
	char *xmsg;

	if (umsg[0] == '\0') {
		Parse_Error(PARSE_FATAL, "Missing argument for \".%s\"",
		    levelName);
		return;
	}

	(void)Var_Subst(umsg, VAR_CMDLINE, VARE_WANTRES, &xmsg);
	/* TODO: handle errors */

	Parse_Error(level, "%s", xmsg);
	free(xmsg);

	if (level == PARSE_FATAL) {
		PrintOnError(NULL, NULL);
		exit(1);
	}
}

/*
 * Add the child to the parent's children.
 *
 * Additionally, add the parent to the child's parents, but only if the
 * target is not special.  An example for such a special target is .END,
 * which does not need to be informed once the child target has been made.
 */
static void
LinkSource(GNode *pgn, GNode *cgn, Boolean isSpecial)
{
	if ((pgn->type & OP_DOUBLEDEP) && !Lst_IsEmpty(&pgn->cohorts))
		pgn = pgn->cohorts.last->datum;

	Lst_Append(&pgn->children, cgn);
	pgn->unmade++;

	/* Special targets like .END don't need any children. */
	if (!isSpecial)
		Lst_Append(&cgn->parents, pgn);

	if (DEBUG(PARSE)) {
		debug_printf("# %s: added child %s - %s\n",
		    __func__, pgn->name, cgn->name);
		Targ_PrintNode(pgn, 0);
		Targ_PrintNode(cgn, 0);
	}
}

/* Add the node to each target from the current dependency group. */
static void
LinkToTargets(GNode *gn, Boolean isSpecial)
{
	GNodeListNode *ln;

	for (ln = targets->first; ln != NULL; ln = ln->next)
		LinkSource(ln->datum, gn, isSpecial);
}

static Boolean
TryApplyDependencyOperator(GNode *gn, GNodeType op)
{
	/*
	 * If the node occurred on the left-hand side of a dependency and the
	 * operator also defines a dependency, they must match.
	 */
	if ((op & OP_OPMASK) && (gn->type & OP_OPMASK) &&
	    ((op & OP_OPMASK) != (gn->type & OP_OPMASK))) {
		Parse_Error(PARSE_FATAL, "Inconsistent operator for %s",
		    gn->name);
		return FALSE;
	}

	if (op == OP_DOUBLEDEP && (gn->type & OP_OPMASK) == OP_DOUBLEDEP) {
		/*
		 * If the node was of the left-hand side of a '::' operator,
		 * we need to create a new instance of it for the children
		 * and commands on this dependency line since each of these
		 * dependency groups has its own attributes and commands,
		 * separate from the others.
		 *
		 * The new instance is placed on the 'cohorts' list of the
		 * initial one (note the initial one is not on its own
		 * cohorts list) and the new instance is linked to all
		 * parents of the initial instance.
		 */
		GNode *cohort;

		/*
		 * Propagate copied bits to the initial node.  They'll be
		 * propagated back to the rest of the cohorts later.
		 */
		gn->type |= op & ~OP_OPMASK;

		cohort = Targ_NewInternalNode(gn->name);
		if (doing_depend)
			ParseMark(cohort);
		/*
		 * Make the cohort invisible as well to avoid duplicating it
		 * into other variables. True, parents of this target won't
		 * tend to do anything with their local variables, but better
		 * safe than sorry.
		 *
		 * (I think this is pointless now, since the relevant list
		 * traversals will no longer see this node anyway. -mycroft)
		 */
		cohort->type = op | OP_INVISIBLE;
		Lst_Append(&gn->cohorts, cohort);
		cohort->centurion = gn;
		gn->unmade_cohorts++;
		snprintf(cohort->cohort_num, sizeof cohort->cohort_num, "#%d",
		    (unsigned int)gn->unmade_cohorts % 1000000);
	} else {
		/*
		 * We don't want to nuke any previous flags (whatever they
		 * were) so we just OR the new operator into the old.
		 */
		gn->type |= op;
	}

	return TRUE;
}

static void
ApplyDependencyOperator(GNodeType op)
{
	GNodeListNode *ln;

	for (ln = targets->first; ln != NULL; ln = ln->next)
		if (!TryApplyDependencyOperator(ln->datum, op))
			break;
}

/*
 * We add a .WAIT node in the dependency list. After any dynamic dependencies
 * (and filename globbing) have happened, it is given a dependency on each
 * previous child, back until the previous .WAIT node. The next child won't
 * be scheduled until the .WAIT node is built.
 *
 * We give each .WAIT node a unique name (mainly for diagnostics).
 */
static void
ParseDependencySourceWait(Boolean isSpecial)
{
	static int wait_number = 0;
	char wait_src[16];
	GNode *gn;

	snprintf(wait_src, sizeof wait_src, ".WAIT_%u", ++wait_number);
	gn = Targ_NewInternalNode(wait_src);
	if (doing_depend)
		ParseMark(gn);
	gn->type = OP_WAIT | OP_PHONY | OP_DEPENDS | OP_NOTMAIN;
	LinkToTargets(gn, isSpecial);

}

static Boolean
ParseDependencySourceKeyword(const char *src, ParseSpecial specType)
{
	int keywd;
	GNodeType op;

	if (*src != '.' || !ch_isupper(src[1]))
		return FALSE;

	keywd = ParseFindKeyword(src);
	if (keywd == -1)
		return FALSE;

	op = parseKeywords[keywd].op;
	if (op != OP_NONE) {
		ApplyDependencyOperator(op);
		return TRUE;
	}
	if (parseKeywords[keywd].spec == SP_WAIT) {
		ParseDependencySourceWait(specType != SP_NOT);
		return TRUE;
	}
	return FALSE;
}

static void
ParseDependencySourceMain(const char *src)
{
	/*
	 * In a line like ".MAIN: source1 source2", it means we need to add
	 * the sources of said target to the list of things to create.
	 *
	 * Note that this will only be invoked if the user didn't specify a
	 * target on the command line and the .MAIN occurs for the first time.
	 *
	 * See ParseDoDependencyTargetSpecial, branch SP_MAIN.
	 * See unit-tests/cond-func-make-main.mk.
	 */
	Lst_Append(&opts.create, bmake_strdup(src));
	/*
	 * Add the name to the .TARGETS variable as well, so the user can
	 * employ that, if desired.
	 */
	Var_Append(".TARGETS", src, VAR_GLOBAL);
}

static void
ParseDependencySourceOrder(const char *src)
{
	GNode *gn;
	/*
	 * Create proper predecessor/successor links between the previous
	 * source and the current one.
	 */
	gn = Targ_GetNode(src);
	if (doing_depend)
		ParseMark(gn);
	if (order_pred != NULL) {
		Lst_Append(&order_pred->order_succ, gn);
		Lst_Append(&gn->order_pred, order_pred);
		if (DEBUG(PARSE)) {
			debug_printf("# %s: added Order dependency %s - %s\n",
			    __func__, order_pred->name, gn->name);
			Targ_PrintNode(order_pred, 0);
			Targ_PrintNode(gn, 0);
		}
	}
	/*
	 * The current source now becomes the predecessor for the next one.
	 */
	order_pred = gn;
}

static void
ParseDependencySourceOther(const char *src, GNodeType tOp,
			   ParseSpecial specType)
{
	GNode *gn;

	/*
	 * If the source is not an attribute, we need to find/create
	 * a node for it. After that we can apply any operator to it
	 * from a special target or link it to its parents, as
	 * appropriate.
	 *
	 * In the case of a source that was the object of a :: operator,
	 * the attribute is applied to all of its instances (as kept in
	 * the 'cohorts' list of the node) or all the cohorts are linked
	 * to all the targets.
	 */

	/* Find/create the 'src' node and attach to all targets */
	gn = Targ_GetNode(src);
	if (doing_depend)
		ParseMark(gn);
	if (tOp != OP_NONE)
		gn->type |= tOp;
	else
		LinkToTargets(gn, specType != SP_NOT);
}

/*
 * Given the name of a source in a dependency line, figure out if it is an
 * attribute (such as .SILENT) and apply it to the targets if it is. Else
 * decide if there is some attribute which should be applied *to* the source
 * because of some special target (such as .PHONY) and apply it if so.
 * Otherwise, make the source a child of the targets in the list 'targets'.
 *
 * Input:
 *	tOp		operator (if any) from special targets
 *	src		name of the source to handle
 */
static void
ParseDependencySource(GNodeType tOp, const char *src, ParseSpecial specType)
{
	if (ParseDependencySourceKeyword(src, specType))
		return;

	if (specType == SP_MAIN)
		ParseDependencySourceMain(src);
	else if (specType == SP_ORDER)
		ParseDependencySourceOrder(src);
	else
		ParseDependencySourceOther(src, tOp, specType);
}

/*
 * If we have yet to decide on a main target to make, in the absence of any
 * user input, we want the first target on the first dependency line that is
 * actually a real target (i.e. isn't a .USE or .EXEC rule) to be made.
 */
static void
FindMainTarget(void)
{
	GNodeListNode *ln;

	if (mainNode != NULL)
		return;

	for (ln = targets->first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		if (!(gn->type & OP_NOTARGET)) {
			DEBUG1(MAKE, "Setting main node to \"%s\"\n", gn->name);
			mainNode = gn;
			Targ_SetMain(gn);
			return;
		}
	}
}

/*
 * We got to the end of the line while we were still looking at targets.
 *
 * Ending a dependency line without an operator is a Bozo no-no.  As a
 * heuristic, this is also often triggered by undetected conflicts from
 * cvs/rcs merges.
 */
static void
ParseErrorNoDependency(const char *lstart)
{
	if ((strncmp(lstart, "<<<<<<", 6) == 0) ||
	    (strncmp(lstart, "======", 6) == 0) ||
	    (strncmp(lstart, ">>>>>>", 6) == 0))
		Parse_Error(PARSE_FATAL,
		    "Makefile appears to contain unresolved cvs/rcs/??? merge conflicts");
	else if (lstart[0] == '.') {
		const char *dirstart = lstart + 1;
		const char *dirend;
		cpp_skip_whitespace(&dirstart);
		dirend = dirstart;
		while (ch_isalnum(*dirend) || *dirend == '-')
			dirend++;
		Parse_Error(PARSE_FATAL, "Unknown directive \"%.*s\"",
		    (int)(dirend - dirstart), dirstart);
	} else
		Parse_Error(PARSE_FATAL, "Need an operator");
}

static void
ParseDependencyTargetWord(const char **pp, const char *lstart)
{
	const char *cp = *pp;

	while (*cp != '\0') {
		if ((ch_isspace(*cp) || *cp == '!' || *cp == ':' ||
		     *cp == '(') &&
		    !ParseIsEscaped(lstart, cp))
			break;

		if (*cp == '$') {
			/*
			 * Must be a dynamic source (would have been expanded
			 * otherwise), so call the Var module to parse the
			 * puppy so we can safely advance beyond it.
			 *
			 * There should be no errors in this, as they would
			 * have been discovered in the initial Var_Subst and
			 * we wouldn't be here.
			 */
			const char *nested_p = cp;
			FStr nested_val;

			(void)Var_Parse(&nested_p, VAR_CMDLINE, VARE_NONE,
			    &nested_val);
			/* TODO: handle errors */
			FStr_Done(&nested_val);
			cp += nested_p - cp;
		} else
			cp++;
	}

	*pp = cp;
}

/* Handle special targets like .PATH, .DEFAULT, .BEGIN, .ORDER. */
static void
ParseDoDependencyTargetSpecial(ParseSpecial *inout_specType,
			       const char *line, /* XXX: bad name */
			       SearchPathList **inout_paths)
{
	switch (*inout_specType) {
	case SP_PATH:
		if (*inout_paths == NULL)
			*inout_paths = Lst_New();
		Lst_Append(*inout_paths, &dirSearchPath);
		break;
	case SP_MAIN:
		/*
		 * Allow targets from the command line to override the
		 * .MAIN node.
		 */
		if (!Lst_IsEmpty(&opts.create))
			*inout_specType = SP_NOT;
		break;
	case SP_BEGIN:
	case SP_END:
	case SP_STALE:
	case SP_ERROR:
	case SP_INTERRUPT: {
		GNode *gn = Targ_GetNode(line);
		if (doing_depend)
			ParseMark(gn);
		gn->type |= OP_NOTMAIN | OP_SPECIAL;
		Lst_Append(targets, gn);
		break;
	}
	case SP_DEFAULT: {
		/*
		 * Need to create a node to hang commands on, but we don't
		 * want it in the graph, nor do we want it to be the Main
		 * Target. We claim the node is a transformation rule to make
		 * life easier later, when we'll use Make_HandleUse to
		 * actually apply the .DEFAULT commands.
		 */
		GNode *gn = GNode_New(".DEFAULT");
		gn->type |= OP_NOTMAIN | OP_TRANSFORM;
		Lst_Append(targets, gn);
		defaultNode = gn;
		break;
	}
	case SP_DELETE_ON_ERROR:
		deleteOnError = TRUE;
		break;
	case SP_NOTPARALLEL:
		opts.maxJobs = 1;
		break;
	case SP_SINGLESHELL:
		opts.compatMake = TRUE;
		break;
	case SP_ORDER:
		order_pred = NULL;
		break;
	default:
		break;
	}
}

/*
 * .PATH<suffix> has to be handled specially.
 * Call on the suffix module to give us a path to modify.
 */
static Boolean
ParseDoDependencyTargetPath(const char *line, /* XXX: bad name */
			    SearchPathList **inout_paths)
{
	SearchPath *path;

	path = Suff_GetPath(&line[5]);
	if (path == NULL) {
		Parse_Error(PARSE_FATAL,
		    "Suffix '%s' not defined (yet)", &line[5]);
		return FALSE;
	}

	if (*inout_paths == NULL)
		*inout_paths = Lst_New();
	Lst_Append(*inout_paths, path);

	return TRUE;
}

/*
 * See if it's a special target and if so set specType to match it.
 */
static Boolean
ParseDoDependencyTarget(const char *line, /* XXX: bad name */
			ParseSpecial *inout_specType,
			GNodeType *out_tOp, SearchPathList **inout_paths)
{
	int keywd;

	if (!(line[0] == '.' && ch_isupper(line[1])))
		return TRUE;

	/*
	 * See if the target is a special target that must have it
	 * or its sources handled specially.
	 */
	keywd = ParseFindKeyword(line);
	if (keywd != -1) {
		if (*inout_specType == SP_PATH &&
		    parseKeywords[keywd].spec != SP_PATH) {
			Parse_Error(PARSE_FATAL, "Mismatched special targets");
			return FALSE;
		}

		*inout_specType = parseKeywords[keywd].spec;
		*out_tOp = parseKeywords[keywd].op;

		ParseDoDependencyTargetSpecial(inout_specType, line,
		    inout_paths);

	} else if (strncmp(line, ".PATH", 5) == 0) {
		*inout_specType = SP_PATH;
		if (!ParseDoDependencyTargetPath(line, inout_paths))
			return FALSE;
	}
	return TRUE;
}

static void
ParseDoDependencyTargetMundane(char *line, /* XXX: bad name */
			       StringList *curTargs)
{
	if (Dir_HasWildcards(line)) {
		/*
		 * Targets are to be sought only in the current directory,
		 * so create an empty path for the thing. Note we need to
		 * use Dir_Destroy in the destruction of the path as the
		 * Dir module could have added a directory to the path...
		 */
		SearchPath *emptyPath = SearchPath_New();

		Dir_Expand(line, emptyPath, curTargs);

		SearchPath_Free(emptyPath);
	} else {
		/*
		 * No wildcards, but we want to avoid code duplication,
		 * so create a list with the word on it.
		 */
		Lst_Append(curTargs, line);
	}

	/* Apply the targets. */

	while (!Lst_IsEmpty(curTargs)) {
		char *targName = Lst_Dequeue(curTargs);
		GNode *gn = Suff_IsTransform(targName)
		    ? Suff_AddTransform(targName)
		    : Targ_GetNode(targName);
		if (doing_depend)
			ParseMark(gn);

		Lst_Append(targets, gn);
	}
}

static void
ParseDoDependencyTargetExtraWarn(char **pp, const char *lstart)
{
	Boolean warning = FALSE;
	char *cp = *pp;

	while (*cp != '\0') {
		if (!ParseIsEscaped(lstart, cp) && (*cp == '!' || *cp == ':'))
			break;
		if (ParseIsEscaped(lstart, cp) || (*cp != ' ' && *cp != '\t'))
			warning = TRUE;
		cp++;
	}
	if (warning)
		Parse_Error(PARSE_WARNING, "Extra target ignored");

	*pp = cp;
}

static void
ParseDoDependencyCheckSpec(ParseSpecial specType)
{
	switch (specType) {
	default:
		Parse_Error(PARSE_WARNING,
		    "Special and mundane targets don't mix. "
		    "Mundane ones ignored");
		break;
	case SP_DEFAULT:
	case SP_STALE:
	case SP_BEGIN:
	case SP_END:
	case SP_ERROR:
	case SP_INTERRUPT:
		/*
		 * These create nodes on which to hang commands, so targets
		 * shouldn't be empty.
		 */
	case SP_NOT:
		/* Nothing special here -- targets can be empty if it wants. */
		break;
	}
}

static Boolean
ParseDoDependencyParseOp(char **pp, const char *lstart, GNodeType *out_op)
{
	const char *cp = *pp;

	if (*cp == '!') {
		*out_op = OP_FORCE;
		(*pp)++;
		return TRUE;
	}

	if (*cp == ':') {
		if (cp[1] == ':') {
			*out_op = OP_DOUBLEDEP;
			(*pp) += 2;
		} else {
			*out_op = OP_DEPENDS;
			(*pp)++;
		}
		return TRUE;
	}

	{
		const char *msg = lstart[0] == '.'
		    ? "Unknown directive" : "Missing dependency operator";
		Parse_Error(PARSE_FATAL, "%s", msg);
		return FALSE;
	}
}

static void
ClearPaths(SearchPathList *paths)
{
	if (paths != NULL) {
		SearchPathListNode *ln;
		for (ln = paths->first; ln != NULL; ln = ln->next)
			SearchPath_Clear(ln->datum);
	}

	Dir_SetPATH();
}

static void
ParseDoDependencySourcesEmpty(ParseSpecial specType, SearchPathList *paths)
{
	switch (specType) {
	case SP_SUFFIXES:
		Suff_ClearSuffixes();
		break;
	case SP_PRECIOUS:
		allPrecious = TRUE;
		break;
	case SP_IGNORE:
		opts.ignoreErrors = TRUE;
		break;
	case SP_SILENT:
		opts.beSilent = TRUE;
		break;
	case SP_PATH:
		ClearPaths(paths);
		break;
#ifdef POSIX
	case SP_POSIX:
		Var_Set("%POSIX", "1003.2", VAR_GLOBAL);
		break;
#endif
	default:
		break;
	}
}

static void
AddToPaths(const char *dir, SearchPathList *paths)
{
	if (paths != NULL) {
		SearchPathListNode *ln;
		for (ln = paths->first; ln != NULL; ln = ln->next)
			(void)Dir_AddDir(ln->datum, dir);
	}
}

/*
 * If the target was one that doesn't take files as its sources
 * but takes something like suffixes, we take each
 * space-separated word on the line as a something and deal
 * with it accordingly.
 *
 * If the target was .SUFFIXES, we take each source as a
 * suffix and add it to the list of suffixes maintained by the
 * Suff module.
 *
 * If the target was a .PATH, we add the source as a directory
 * to search on the search path.
 *
 * If it was .INCLUDES, the source is taken to be the suffix of
 * files which will be #included and whose search path should
 * be present in the .INCLUDES variable.
 *
 * If it was .LIBS, the source is taken to be the suffix of
 * files which are considered libraries and whose search path
 * should be present in the .LIBS variable.
 *
 * If it was .NULL, the source is the suffix to use when a file
 * has no valid suffix.
 *
 * If it was .OBJDIR, the source is a new definition for .OBJDIR,
 * and will cause make to do a new chdir to that path.
 */
static void
ParseDoDependencySourceSpecial(ParseSpecial specType, char *word,
			       SearchPathList *paths)
{
	switch (specType) {
	case SP_SUFFIXES:
		Suff_AddSuffix(word, &mainNode);
		break;
	case SP_PATH:
		AddToPaths(word, paths);
		break;
	case SP_INCLUDES:
		Suff_AddInclude(word);
		break;
	case SP_LIBS:
		Suff_AddLib(word);
		break;
	case SP_NULL:
		Suff_SetNull(word);
		break;
	case SP_OBJDIR:
		Main_SetObjdir(FALSE, "%s", word);
		break;
	default:
		break;
	}
}

static Boolean
ParseDoDependencyTargets(char **inout_cp,
			 char **inout_line,
			 const char *lstart,
			 ParseSpecial *inout_specType,
			 GNodeType *inout_tOp,
			 SearchPathList **inout_paths,
			 StringList *curTargs)
{
	char *cp;
	char *tgt = *inout_line;
	char savec;
	const char *p;

	for (;;) {
		/*
		 * Here LINE points to the beginning of the next word, and
		 * LSTART points to the actual beginning of the line.
		 */

		/* Find the end of the next word. */
		cp = tgt;
		p = cp;
		ParseDependencyTargetWord(&p, lstart);
		cp += p - cp;

		/*
		 * If the word is followed by a left parenthesis, it's the
		 * name of an object file inside an archive (ar file).
		 */
		if (!ParseIsEscaped(lstart, cp) && *cp == '(') {
			/*
			 * Archives must be handled specially to make sure the
			 * OP_ARCHV flag is set in their 'type' field, for one
			 * thing, and because things like "archive(file1.o
			 * file2.o file3.o)" are permissible.
			 *
			 * Arch_ParseArchive will set 'line' to be the first
			 * non-blank after the archive-spec. It creates/finds
			 * nodes for the members and places them on the given
			 * list, returning TRUE if all went well and FALSE if
			 * there was an error in the specification. On error,
			 * line should remain untouched.
			 */
			if (!Arch_ParseArchive(&tgt, targets, VAR_CMDLINE)) {
				Parse_Error(PARSE_FATAL,
				    "Error in archive specification: \"%s\"",
				    tgt);
				return FALSE;
			}

			cp = tgt;
			continue;
		}

		if (*cp == '\0') {
			ParseErrorNoDependency(lstart);
			return FALSE;
		}

		/* Insert a null terminator. */
		savec = *cp;
		*cp = '\0';

		if (!ParseDoDependencyTarget(tgt, inout_specType, inout_tOp,
		    inout_paths))
			return FALSE;

		/*
		 * Have word in line. Get or create its node and stick it at
		 * the end of the targets list
		 */
		if (*inout_specType == SP_NOT && *tgt != '\0')
			ParseDoDependencyTargetMundane(tgt, curTargs);
		else if (*inout_specType == SP_PATH && *tgt != '.' &&
			 *tgt != '\0')
			Parse_Error(PARSE_WARNING, "Extra target (%s) ignored",
			    tgt);

		/* Don't need the inserted null terminator any more. */
		*cp = savec;

		/*
		 * If it is a special type and not .PATH, it's the only target
		 * we allow on this line.
		 */
		if (*inout_specType != SP_NOT && *inout_specType != SP_PATH)
			ParseDoDependencyTargetExtraWarn(&cp, lstart);
		else
			pp_skip_whitespace(&cp);

		tgt = cp;
		if (*tgt == '\0')
			break;
		if ((*tgt == '!' || *tgt == ':') &&
		    !ParseIsEscaped(lstart, tgt))
			break;
	}

	*inout_cp = cp;
	*inout_line = tgt;
	return TRUE;
}

static void
ParseDoDependencySourcesSpecial(char *start, char *end,
				ParseSpecial specType, SearchPathList *paths)
{
	char savec;

	while (*start != '\0') {
		while (*end != '\0' && !ch_isspace(*end))
			end++;
		savec = *end;
		*end = '\0';
		ParseDoDependencySourceSpecial(specType, start, paths);
		*end = savec;
		if (savec != '\0')
			end++;
		pp_skip_whitespace(&end);
		start = end;
	}
}

static Boolean
ParseDoDependencySourcesMundane(char *start, char *end,
				ParseSpecial specType, GNodeType tOp)
{
	while (*start != '\0') {
		/*
		 * The targets take real sources, so we must beware of archive
		 * specifications (i.e. things with left parentheses in them)
		 * and handle them accordingly.
		 */
		for (; *end != '\0' && !ch_isspace(*end); end++) {
			if (*end == '(' && end > start && end[-1] != '$') {
				/*
				 * Only stop for a left parenthesis if it
				 * isn't at the start of a word (that'll be
				 * for variable changes later) and isn't
				 * preceded by a dollar sign (a dynamic
				 * source).
				 */
				break;
			}
		}

		if (*end == '(') {
			GNodeList sources = LST_INIT;
			if (!Arch_ParseArchive(&start, &sources, VAR_CMDLINE)) {
				Parse_Error(PARSE_FATAL,
				    "Error in source archive spec \"%s\"",
				    start);
				return FALSE;
			}

			while (!Lst_IsEmpty(&sources)) {
				GNode *gn = Lst_Dequeue(&sources);
				ParseDependencySource(tOp, gn->name, specType);
			}
			Lst_Done(&sources);
			end = start;
		} else {
			if (*end != '\0') {
				*end = '\0';
				end++;
			}

			ParseDependencySource(tOp, start, specType);
		}
		pp_skip_whitespace(&end);
		start = end;
	}
	return TRUE;
}

/*
 * Parse a dependency line consisting of targets, followed by a dependency
 * operator, optionally followed by sources.
 *
 * The nodes of the sources are linked as children to the nodes of the
 * targets. Nodes are created as necessary.
 *
 * The operator is applied to each node in the global 'targets' list,
 * which is where the nodes found for the targets are kept, by means of
 * the ParseDoOp function.
 *
 * The sources are parsed in much the same way as the targets, except
 * that they are expanded using the wildcarding scheme of the C-Shell,
 * and a target is created for each expanded word. Each of the resulting
 * nodes is then linked to each of the targets as one of its children.
 *
 * Certain targets and sources such as .PHONY or .PRECIOUS are handled
 * specially. These are the ones detailed by the specType variable.
 *
 * The storing of transformation rules such as '.c.o' is also taken care of
 * here. A target is recognized as a transformation rule by calling
 * Suff_IsTransform. If it is a transformation rule, its node is gotten
 * from the suffix module via Suff_AddTransform rather than the standard
 * Targ_FindNode in the target module.
 *
 * Upon return, the value of the line is unspecified.
 */
static void
ParseDoDependency(char *line)
{
	char *cp;		/* our current position */
	GNodeType op;		/* the operator on the line */
	SearchPathList *paths;	/* search paths to alter when parsing
				 * a list of .PATH targets */
	GNodeType tOp;		/* operator from special target */
	/* target names to be found and added to the targets list */
	StringList curTargs = LST_INIT;
	char *lstart = line;

	/*
	 * specType contains the SPECial TYPE of the current target. It is
	 * SP_NOT if the target is unspecial. If it *is* special, however, the
	 * children are linked as children of the parent but not vice versa.
	 */
	ParseSpecial specType = SP_NOT;

	DEBUG1(PARSE, "ParseDoDependency(%s)\n", line);
	tOp = OP_NONE;

	paths = NULL;

	/*
	 * First, grind through the targets.
	 */
	/* XXX: don't use line as an iterator variable */
	if (!ParseDoDependencyTargets(&cp, &line, lstart, &specType, &tOp,
	    &paths, &curTargs))
		goto out;

	/*
	 * Don't need the list of target names anymore.
	 * The targets themselves are now in the global variable 'targets'.
	 */
	Lst_Done(&curTargs);
	Lst_Init(&curTargs);

	if (!Lst_IsEmpty(targets))
		ParseDoDependencyCheckSpec(specType);

	/*
	 * Have now parsed all the target names. Must parse the operator next.
	 */
	if (!ParseDoDependencyParseOp(&cp, lstart, &op))
		goto out;

	/*
	 * Apply the operator to the target. This is how we remember which
	 * operator a target was defined with. It fails if the operator
	 * used isn't consistent across all references.
	 */
	ApplyDependencyOperator(op);

	/*
	 * Onward to the sources.
	 *
	 * LINE will now point to the first source word, if any, or the
	 * end of the string if not.
	 */
	pp_skip_whitespace(&cp);
	line = cp;		/* XXX: 'line' is an inappropriate name */

	/*
	 * Several special targets take different actions if present with no
	 * sources:
	 *	a .SUFFIXES line with no sources clears out all old suffixes
	 *	a .PRECIOUS line makes all targets precious
	 *	a .IGNORE line ignores errors for all targets
	 *	a .SILENT line creates silence when making all targets
	 *	a .PATH removes all directories from the search path(s).
	 */
	if (line[0] == '\0') {
		ParseDoDependencySourcesEmpty(specType, paths);
	} else if (specType == SP_MFLAGS) {
		/*
		 * Call on functions in main.c to deal with these arguments and
		 * set the initial character to a null-character so the loop to
		 * get sources won't get anything
		 */
		Main_ParseArgLine(line);
		*line = '\0';
	} else if (specType == SP_SHELL) {
		if (!Job_ParseShell(line)) {
			Parse_Error(PARSE_FATAL,
			    "improper shell specification");
			goto out;
		}
		*line = '\0';
	} else if (specType == SP_NOTPARALLEL || specType == SP_SINGLESHELL ||
		   specType == SP_DELETE_ON_ERROR) {
		*line = '\0';
	}

	/* Now go for the sources. */
	if (specType == SP_SUFFIXES || specType == SP_PATH ||
	    specType == SP_INCLUDES || specType == SP_LIBS ||
	    specType == SP_NULL || specType == SP_OBJDIR) {
		ParseDoDependencySourcesSpecial(line, cp, specType, paths);
		if (paths != NULL) {
			Lst_Free(paths);
			paths = NULL;
		}
		if (specType == SP_PATH)
			Dir_SetPATH();
	} else {
		assert(paths == NULL);
		if (!ParseDoDependencySourcesMundane(line, cp, specType, tOp))
			goto out;
	}

	FindMainTarget();

out:
	if (paths != NULL)
		Lst_Free(paths);
	Lst_Done(&curTargs);
}

typedef struct VarAssignParsed {
	const char *nameStart;	/* unexpanded */
	const char *nameEnd;	/* before operator adjustment */
	const char *eq;		/* the '=' of the assignment operator */
} VarAssignParsed;

/*
 * Determine the assignment operator and adjust the end of the variable
 * name accordingly.
 */
static void
AdjustVarassignOp(const VarAssignParsed *pvar, const char *value,
		  VarAssign *out_var)
{
	const char *op = pvar->eq;
	const char *const name = pvar->nameStart;
	VarAssignOp type;

	if (op > name && op[-1] == '+') {
		type = VAR_APPEND;
		op--;

	} else if (op > name && op[-1] == '?') {
		op--;
		type = VAR_DEFAULT;

	} else if (op > name && op[-1] == ':') {
		op--;
		type = VAR_SUBST;

	} else if (op > name && op[-1] == '!') {
		op--;
		type = VAR_SHELL;

	} else {
		type = VAR_NORMAL;
#ifdef SUNSHCMD
		while (op > name && ch_isspace(op[-1]))
			op--;

		if (op >= name + 3 && op[-3] == ':' && op[-2] == 's' &&
		    op[-1] == 'h') {
			type = VAR_SHELL;
			op -= 3;
		}
#endif
	}

	{
		const char *nameEnd = pvar->nameEnd < op ? pvar->nameEnd : op;
		out_var->varname = bmake_strsedup(pvar->nameStart, nameEnd);
		out_var->op = type;
		out_var->value = value;
	}
}

/*
 * Parse a variable assignment, consisting of a single-word variable name,
 * optional whitespace, an assignment operator, optional whitespace and the
 * variable value.
 *
 * Note: There is a lexical ambiguity with assignment modifier characters
 * in variable names. This routine interprets the character before the =
 * as a modifier. Therefore, an assignment like
 *	C++=/usr/bin/CC
 * is interpreted as "C+ +=" instead of "C++ =".
 *
 * Used for both lines in a file and command line arguments.
 */
Boolean
Parse_IsVar(const char *p, VarAssign *out_var)
{
	VarAssignParsed pvar;
	const char *firstSpace = NULL;
	int level = 0;

	cpp_skip_hspace(&p);	/* Skip to variable name */

	/*
	 * During parsing, the '+' of the '+=' operator is initially parsed
	 * as part of the variable name.  It is later corrected, as is the
	 * ':sh' modifier. Of these two (nameEnd and op), the earlier one
	 * determines the actual end of the variable name.
	 */
	pvar.nameStart = p;
#ifdef CLEANUP
	pvar.nameEnd = NULL;
	pvar.eq = NULL;
#endif

	/*
	 * Scan for one of the assignment operators outside a variable
	 * expansion.
	 */
	while (*p != '\0') {
		char ch = *p++;
		if (ch == '(' || ch == '{') {
			level++;
			continue;
		}
		if (ch == ')' || ch == '}') {
			level--;
			continue;
		}

		if (level != 0)
			continue;

		if (ch == ' ' || ch == '\t')
			if (firstSpace == NULL)
				firstSpace = p - 1;
		while (ch == ' ' || ch == '\t')
			ch = *p++;

#ifdef SUNSHCMD
		if (ch == ':' && p[0] == 's' && p[1] == 'h') {
			p += 2;
			continue;
		}
#endif
		if (ch == '=') {
			pvar.eq = p - 1;
			pvar.nameEnd = firstSpace != NULL ? firstSpace : p - 1;
			cpp_skip_whitespace(&p);
			AdjustVarassignOp(&pvar, p, out_var);
			return TRUE;
		}
		if (*p == '=' &&
		    (ch == '+' || ch == ':' || ch == '?' || ch == '!')) {
			pvar.eq = p;
			pvar.nameEnd = firstSpace != NULL ? firstSpace : p;
			p++;
			cpp_skip_whitespace(&p);
			AdjustVarassignOp(&pvar, p, out_var);
			return TRUE;
		}
		if (firstSpace != NULL)
			return FALSE;
	}

	return FALSE;
}

/*
 * Check for syntax errors such as unclosed expressions or unknown modifiers.
 */
static void
VarCheckSyntax(VarAssignOp type, const char *uvalue, GNode *ctxt)
{
	if (opts.strict) {
		if (type != VAR_SUBST && strchr(uvalue, '$') != NULL) {
			char *expandedValue;

			(void)Var_Subst(uvalue, ctxt, VARE_NONE,
			    &expandedValue);
			/* TODO: handle errors */
			free(expandedValue);
		}
	}
}

static void
VarAssign_EvalSubst(const char *name, const char *uvalue, GNode *ctxt,
		    FStr *out_avalue)
{
	const char *avalue;
	char *evalue;

	/*
	 * make sure that we set the variable the first time to nothing
	 * so that it gets substituted!
	 */
	if (!Var_Exists(name, ctxt))
		Var_Set(name, "", ctxt);

	(void)Var_Subst(uvalue, ctxt,
	    VARE_WANTRES | VARE_KEEP_DOLLAR | VARE_KEEP_UNDEF, &evalue);
	/* TODO: handle errors */

	avalue = evalue;
	Var_Set(name, avalue, ctxt);

	*out_avalue = (FStr){ avalue, evalue };
}

static void
VarAssign_EvalShell(const char *name, const char *uvalue, GNode *ctxt,
		    FStr *out_avalue)
{
	FStr cmd;
	const char *errfmt;
	char *cmdOut;

	cmd = FStr_InitRefer(uvalue);
	if (strchr(cmd.str, '$') != NULL) {
		char *expanded;
		(void)Var_Subst(cmd.str, VAR_CMDLINE,
		    VARE_WANTRES | VARE_UNDEFERR, &expanded);
		/* TODO: handle errors */
		cmd = FStr_InitOwn(expanded);
	}

	cmdOut = Cmd_Exec(cmd.str, &errfmt);
	Var_Set(name, cmdOut, ctxt);
	*out_avalue = FStr_InitOwn(cmdOut);

	if (errfmt != NULL)
		Parse_Error(PARSE_WARNING, errfmt, cmd.str);

	FStr_Done(&cmd);
}

/*
 * Perform a variable assignment.
 *
 * The actual value of the variable is returned in *out_avalue and
 * *out_avalue_freeIt.  Especially for VAR_SUBST and VAR_SHELL this can differ
 * from the literal value.
 *
 * Return whether the assignment was actually done.  The assignment is only
 * skipped if the operator is '?=' and the variable already exists.
 */
static Boolean
VarAssign_Eval(const char *name, VarAssignOp op, const char *uvalue,
	       GNode *ctxt, FStr *out_TRUE_avalue)
{
	FStr avalue = FStr_InitRefer(uvalue);

	if (op == VAR_APPEND)
		Var_Append(name, uvalue, ctxt);
	else if (op == VAR_SUBST)
		VarAssign_EvalSubst(name, uvalue, ctxt, &avalue);
	else if (op == VAR_SHELL)
		VarAssign_EvalShell(name, uvalue, ctxt, &avalue);
	else {
		if (op == VAR_DEFAULT && Var_Exists(name, ctxt))
			return FALSE;

		/* Normal assignment -- just do it. */
		Var_Set(name, uvalue, ctxt);
	}

	*out_TRUE_avalue = avalue;
	return TRUE;
}

static void
VarAssignSpecial(const char *name, const char *avalue)
{
	if (strcmp(name, MAKEOVERRIDES) == 0)
		Main_ExportMAKEFLAGS(FALSE); /* re-export MAKEFLAGS */
	else if (strcmp(name, ".CURDIR") == 0) {
		/*
		 * Someone is being (too?) clever...
		 * Let's pretend they know what they are doing and
		 * re-initialize the 'cur' CachedDir.
		 */
		Dir_InitCur(avalue);
		Dir_SetPATH();
	} else if (strcmp(name, MAKE_JOB_PREFIX) == 0)
		Job_SetPrefix();
	else if (strcmp(name, MAKE_EXPORTED) == 0)
		Var_ExportVars(avalue);
}

/* Perform the variable variable assignment in the given context. */
void
Parse_DoVar(VarAssign *var, GNode *ctxt)
{
	FStr avalue;	/* actual value (maybe expanded) */

	VarCheckSyntax(var->op, var->value, ctxt);
	if (VarAssign_Eval(var->varname, var->op, var->value, ctxt, &avalue)) {
		VarAssignSpecial(var->varname, avalue.str);
		FStr_Done(&avalue);
	}

	free(var->varname);
}


/*
 * See if the command possibly calls a sub-make by using the variable
 * expressions ${.MAKE}, ${MAKE} or the plain word "make".
 */
static Boolean
MaybeSubMake(const char *cmd)
{
	const char *start;

	for (start = cmd; *start != '\0'; start++) {
		const char *p = start;
		char endc;

		/* XXX: What if progname != "make"? */
		if (p[0] == 'm' && p[1] == 'a' && p[2] == 'k' && p[3] == 'e')
			if (start == cmd || !ch_isalnum(p[-1]))
				if (!ch_isalnum(p[4]))
					return TRUE;

		if (*p != '$')
			continue;
		p++;

		if (*p == '{')
			endc = '}';
		else if (*p == '(')
			endc = ')';
		else
			continue;
		p++;

		if (*p == '.')	/* Accept either ${.MAKE} or ${MAKE}. */
			p++;

		if (p[0] == 'M' && p[1] == 'A' && p[2] == 'K' && p[3] == 'E')
			if (p[4] == endc)
				return TRUE;
	}
	return FALSE;
}

/*
 * Append the command to the target node.
 *
 * The node may be marked as a submake node if the command is determined to
 * be that.
 */
static void
ParseAddCmd(GNode *gn, char *cmd)
{
	/* Add to last (ie current) cohort for :: targets */
	if ((gn->type & OP_DOUBLEDEP) && gn->cohorts.last != NULL)
		gn = gn->cohorts.last->datum;

	/* if target already supplied, ignore commands */
	if (!(gn->type & OP_HAS_COMMANDS)) {
		Lst_Append(&gn->commands, cmd);
		if (MaybeSubMake(cmd))
			gn->type |= OP_SUBMAKE;
		ParseMark(gn);
	} else {
#if 0
		/* XXX: We cannot do this until we fix the tree */
		Lst_Append(&gn->commands, cmd);
		Parse_Error(PARSE_WARNING,
		    "overriding commands for target \"%s\"; "
		    "previous commands defined at %s: %d ignored",
		    gn->name, gn->fname, gn->lineno);
#else
		Parse_Error(PARSE_WARNING,
		    "duplicate script for target \"%s\" ignored",
		    gn->name);
		ParseErrorInternal(gn->fname, (size_t)gn->lineno, PARSE_WARNING,
		    "using previous script for \"%s\" defined here",
		    gn->name);
#endif
	}
}

/*
 * Add a directory to the path searched for included makefiles bracketed
 * by double-quotes.
 */
void
Parse_AddIncludeDir(const char *dir)
{
	(void)Dir_AddDir(parseIncPath, dir);
}

/*
 * Handle one of the .[-ds]include directives by remembering the current file
 * and pushing the included file on the stack.  After the included file has
 * finished, parsing continues with the including file; see Parse_SetInput
 * and ParseEOF.
 *
 * System includes are looked up in sysIncPath, any other includes are looked
 * up in the parsedir and then in the directories specified by the -I command
 * line options.
 */
static void
Parse_include_file(char *file, Boolean isSystem, Boolean depinc, Boolean silent)
{
	struct loadedfile *lf;
	char *fullname;		/* full pathname of file */
	char *newName;
	char *slash, *incdir;
	int fd;
	int i;

	fullname = file[0] == '/' ? bmake_strdup(file) : NULL;

	if (fullname == NULL && !isSystem) {
		/*
		 * Include files contained in double-quotes are first searched
		 * relative to the including file's location. We don't want to
		 * cd there, of course, so we just tack on the old file's
		 * leading path components and call Dir_FindFile to see if
		 * we can locate the file.
		 */

		incdir = bmake_strdup(CurFile()->fname);
		slash = strrchr(incdir, '/');
		if (slash != NULL) {
			*slash = '\0';
			/*
			 * Now do lexical processing of leading "../" on the
			 * filename.
			 */
			for (i = 0; strncmp(file + i, "../", 3) == 0; i += 3) {
				slash = strrchr(incdir + 1, '/');
				if (slash == NULL || strcmp(slash, "/..") == 0)
					break;
				*slash = '\0';
			}
			newName = str_concat3(incdir, "/", file + i);
			fullname = Dir_FindFile(newName, parseIncPath);
			if (fullname == NULL)
				fullname = Dir_FindFile(newName,
				    &dirSearchPath);
			free(newName);
		}
		free(incdir);

		if (fullname == NULL) {
			/*
			 * Makefile wasn't found in same directory as included
			 * makefile.
			 *
			 * Search for it first on the -I search path, then on
			 * the .PATH search path, if not found in a -I
			 * directory. If we have a suffix-specific path, we
			 * should use that.
			 */
			const char *suff;
			SearchPath *suffPath = NULL;

			if ((suff = strrchr(file, '.')) != NULL) {
				suffPath = Suff_GetPath(suff);
				if (suffPath != NULL)
					fullname = Dir_FindFile(file, suffPath);
			}
			if (fullname == NULL) {
				fullname = Dir_FindFile(file, parseIncPath);
				if (fullname == NULL)
					fullname = Dir_FindFile(file,
					    &dirSearchPath);
			}
		}
	}

	/* Looking for a system file or file still not found */
	if (fullname == NULL) {
		/*
		 * Look for it on the system path
		 */
		SearchPath *path = Lst_IsEmpty(sysIncPath) ? defSysIncPath
		    : sysIncPath;
		fullname = Dir_FindFile(file, path);
	}

	if (fullname == NULL) {
		if (!silent)
			Parse_Error(PARSE_FATAL, "Could not find %s", file);
		return;
	}

	/* Actually open the file... */
	fd = open(fullname, O_RDONLY);
	if (fd == -1) {
		if (!silent)
			Parse_Error(PARSE_FATAL, "Cannot open %s", fullname);
		free(fullname);
		return;
	}

	/* load it */
	lf = loadfile(fullname, fd);

	/* Start reading from this file next */
	Parse_SetInput(fullname, 0, -1, loadedfile_readMore, lf);
	CurFile()->lf = lf;
	if (depinc)
		doing_depend = depinc;	/* only turn it on */
}

static void
ParseDoInclude(char *line /* XXX: bad name */)
{
	char endc;		/* the character which ends the file spec */
	char *cp;		/* current position in file spec */
	Boolean silent = line[0] != 'i';
	char *file = line + (silent ? 8 : 7);

	/* Skip to delimiter character so we know where to look */
	pp_skip_hspace(&file);

	if (*file != '"' && *file != '<') {
		Parse_Error(PARSE_FATAL,
		    ".include filename must be delimited by '\"' or '<'");
		return;
	}

	/*
	 * Set the search path on which to find the include file based on the
	 * characters which bracket its name. Angle-brackets imply it's
	 * a system Makefile while double-quotes imply it's a user makefile
	 */
	if (*file == '<')
		endc = '>';
	else
		endc = '"';

	/* Skip to matching delimiter */
	for (cp = ++file; *cp != '\0' && *cp != endc; cp++)
		continue;

	if (*cp != endc) {
		Parse_Error(PARSE_FATAL,
		    "Unclosed .include filename. '%c' expected", endc);
		return;
	}

	*cp = '\0';

	/*
	 * Substitute for any variables in the filename before trying to
	 * find the file.
	 */
	(void)Var_Subst(file, VAR_CMDLINE, VARE_WANTRES, &file);
	/* TODO: handle errors */

	Parse_include_file(file, endc == '>', line[0] == 'd', silent);
	free(file);
}

/*
 * Split filename into dirname + basename, then assign these to the
 * given variables.
 */
static void
SetFilenameVars(const char *filename, const char *dirvar, const char *filevar)
{
	const char *slash, *dirname, *basename;
	void *freeIt;

	slash = strrchr(filename, '/');
	if (slash == NULL) {
		dirname = curdir;
		basename = filename;
		freeIt = NULL;
	} else {
		dirname = freeIt = bmake_strsedup(filename, slash);
		basename = slash + 1;
	}

	Var_Set(dirvar, dirname, VAR_GLOBAL);
	Var_Set(filevar, basename, VAR_GLOBAL);

	DEBUG5(PARSE, "%s: ${%s} = `%s' ${%s} = `%s'\n",
	    __func__, dirvar, dirname, filevar, basename);
	free(freeIt);
}

/*
 * Return the immediately including file.
 *
 * This is made complicated since the .for loop is implemented as a special
 * kind of .include; see For_Run.
 */
static const char *
GetActuallyIncludingFile(void)
{
	size_t i;
	const IFile *incs = GetInclude(0);

	for (i = includes.len; i >= 2; i--)
		if (!incs[i - 1].fromForLoop)
			return incs[i - 2].fname;
	return NULL;
}

/* Set .PARSEDIR, .PARSEFILE, .INCLUDEDFROMDIR and .INCLUDEDFROMFILE. */
static void
ParseSetParseFile(const char *filename)
{
	const char *including;

	SetFilenameVars(filename, ".PARSEDIR", ".PARSEFILE");

	including = GetActuallyIncludingFile();
	if (including != NULL) {
		SetFilenameVars(including,
		    ".INCLUDEDFROMDIR", ".INCLUDEDFROMFILE");
	} else {
		Var_Delete(".INCLUDEDFROMDIR", VAR_GLOBAL);
		Var_Delete(".INCLUDEDFROMFILE", VAR_GLOBAL);
	}
}

static Boolean
StrContainsWord(const char *str, const char *word)
{
	size_t strLen = strlen(str);
	size_t wordLen = strlen(word);
	const char *p, *end;

	if (strLen < wordLen)
		return FALSE;	/* str is too short to contain word */

	end = str + strLen - wordLen;
	for (p = str; p != NULL; p = strchr(p, ' ')) {
		if (*p == ' ')
			p++;
		if (p > end)
			return FALSE;	/* cannot contain word */

		if (memcmp(p, word, wordLen) == 0 &&
		    (p[wordLen] == '\0' || p[wordLen] == ' '))
			return TRUE;
	}
	return FALSE;
}

/*
 * XXX: Searching through a set of words with this linear search is
 * inefficient for variables that contain thousands of words.
 *
 * XXX: The paths in this list don't seem to be normalized in any way.
 */
static Boolean
VarContainsWord(const char *varname, const char *word)
{
	FStr val = Var_Value(varname, VAR_GLOBAL);
	Boolean found = val.str != NULL && StrContainsWord(val.str, word);
	FStr_Done(&val);
	return found;
}

/*
 * Track the makefiles we read - so makefiles can set dependencies on them.
 * Avoid adding anything more than once.
 *
 * Time complexity: O(n) per call, in total O(n^2), where n is the number
 * of makefiles that have been loaded.
 */
static void
ParseTrackInput(const char *name)
{
	if (!VarContainsWord(MAKE_MAKEFILES, name))
		Var_Append(MAKE_MAKEFILES, name, VAR_GLOBAL);
}


/*
 * Start parsing from the given source.
 *
 * The given file is added to the includes stack.
 */
void
Parse_SetInput(const char *name, int lineno, int fd,
	       ReadMoreProc readMore, void *readMoreArg)
{
	IFile *curFile;
	char *buf;
	size_t len;
	Boolean fromForLoop = name == NULL;

	if (fromForLoop)
		name = CurFile()->fname;
	else
		ParseTrackInput(name);

	DEBUG3(PARSE, "Parse_SetInput: %s %s, line %d\n",
	    readMore == loadedfile_readMore ? "file" : ".for loop in",
	    name, lineno);

	if (fd == -1 && readMore == NULL)
		/* sanity */
		return;

	curFile = Vector_Push(&includes);
	curFile->fname = bmake_strdup(name);
	curFile->fromForLoop = fromForLoop;
	curFile->lineno = lineno;
	curFile->first_lineno = lineno;
	curFile->readMore = readMore;
	curFile->readMoreArg = readMoreArg;
	curFile->lf = NULL;
	curFile->depending = doing_depend;	/* restore this on EOF */

	assert(readMore != NULL);

	/* Get first block of input data */
	buf = curFile->readMore(curFile->readMoreArg, &len);
	if (buf == NULL) {
		/* Was all a waste of time ... */
		if (curFile->fname != NULL)
			free(curFile->fname);
		free(curFile);
		return;
	}
	curFile->buf_freeIt = buf;
	curFile->buf_ptr = buf;
	curFile->buf_end = buf + len;

	curFile->cond_depth = Cond_save_depth();
	ParseSetParseFile(name);
}

/* Check if the directive is an include directive. */
static Boolean
IsInclude(const char *dir, Boolean sysv)
{
	if (dir[0] == 's' || dir[0] == '-' || (dir[0] == 'd' && !sysv))
		dir++;

	if (strncmp(dir, "include", 7) != 0)
		return FALSE;

	/* Space is not mandatory for BSD .include */
	return !sysv || ch_isspace(dir[7]);
}


#ifdef SYSVINCLUDE
/* Check if the line is a SYSV include directive. */
static Boolean
IsSysVInclude(const char *line)
{
	const char *p;

	if (!IsInclude(line, TRUE))
		return FALSE;

	/* Avoid interpreting a dependency line as an include */
	for (p = line; (p = strchr(p, ':')) != NULL;) {

		/* end of line -> it's a dependency */
		if (*++p == '\0')
			return FALSE;

		/* '::' operator or ': ' -> it's a dependency */
		if (*p == ':' || ch_isspace(*p))
			return FALSE;
	}
	return TRUE;
}

/* Push to another file.  The line points to the word "include". */
static void
ParseTraditionalInclude(char *line)
{
	char *cp;		/* current position in file spec */
	Boolean done = FALSE;
	Boolean silent = line[0] != 'i';
	char *file = line + (silent ? 8 : 7);
	char *all_files;

	DEBUG2(PARSE, "%s: %s\n", __func__, file);

	pp_skip_whitespace(&file);

	/*
	 * Substitute for any variables in the file name before trying to
	 * find the thing.
	 */
	(void)Var_Subst(file, VAR_CMDLINE, VARE_WANTRES, &all_files);
	/* TODO: handle errors */

	if (*file == '\0') {
		Parse_Error(PARSE_FATAL, "Filename missing from \"include\"");
		goto out;
	}

	for (file = all_files; !done; file = cp + 1) {
		/* Skip to end of line or next whitespace */
		for (cp = file; *cp != '\0' && !ch_isspace(*cp); cp++)
			continue;

		if (*cp != '\0')
			*cp = '\0';
		else
			done = TRUE;

		Parse_include_file(file, FALSE, FALSE, silent);
	}
out:
	free(all_files);
}
#endif

#ifdef GMAKEEXPORT
/* Parse "export <variable>=<value>", and actually export it. */
static void
ParseGmakeExport(char *line)
{
	char *variable = line + 6;
	char *value;

	DEBUG2(PARSE, "%s: %s\n", __func__, variable);

	pp_skip_whitespace(&variable);

	for (value = variable; *value != '\0' && *value != '='; value++)
		continue;

	if (*value != '=') {
		Parse_Error(PARSE_FATAL,
		    "Variable/Value missing from \"export\"");
		return;
	}
	*value++ = '\0';	/* terminate variable */

	/*
	 * Expand the value before putting it in the environment.
	 */
	(void)Var_Subst(value, VAR_CMDLINE, VARE_WANTRES, &value);
	/* TODO: handle errors */

	setenv(variable, value, 1);
	free(value);
}
#endif

/*
 * Called when EOF is reached in the current file. If we were reading an
 * include file or a .for loop, the includes stack is popped and things set
 * up to go back to reading the previous file at the previous location.
 *
 * Results:
 *	TRUE to continue parsing, i.e. it had only reached the end of an
 *	included file, FALSE if the main file has been parsed completely.
 */
static Boolean
ParseEOF(void)
{
	char *ptr;
	size_t len;
	IFile *curFile = CurFile();

	assert(curFile->readMore != NULL);

	doing_depend = curFile->depending;	/* restore this */
	/* get next input buffer, if any */
	ptr = curFile->readMore(curFile->readMoreArg, &len);
	curFile->buf_ptr = ptr;
	curFile->buf_freeIt = ptr;
	curFile->buf_end = ptr == NULL ? NULL : ptr + len;
	curFile->lineno = curFile->first_lineno;
	if (ptr != NULL)
		return TRUE;	/* Iterate again */

	/* Ensure the makefile (or loop) didn't have mismatched conditionals */
	Cond_restore_depth(curFile->cond_depth);

	if (curFile->lf != NULL) {
		loadedfile_destroy(curFile->lf);
		curFile->lf = NULL;
	}

	/* Dispose of curFile info */
	/* Leak curFile->fname because all the gnodes have pointers to it. */
	free(curFile->buf_freeIt);
	Vector_Pop(&includes);

	if (includes.len == 0) {
		/* We've run out of input */
		Var_Delete(".PARSEDIR", VAR_GLOBAL);
		Var_Delete(".PARSEFILE", VAR_GLOBAL);
		Var_Delete(".INCLUDEDFROMDIR", VAR_GLOBAL);
		Var_Delete(".INCLUDEDFROMFILE", VAR_GLOBAL);
		return FALSE;
	}

	curFile = CurFile();
	DEBUG2(PARSE, "ParseEOF: returning to file %s, line %d\n",
	    curFile->fname, curFile->lineno);

	ParseSetParseFile(curFile->fname);
	return TRUE;
}

typedef enum ParseRawLineResult {
	PRLR_LINE,
	PRLR_EOF,
	PRLR_ERROR
} ParseRawLineResult;

/*
 * Parse until the end of a line, taking into account lines that end with
 * backslash-newline.
 */
static ParseRawLineResult
ParseRawLine(IFile *curFile, char **out_line, char **out_line_end,
	     char **out_firstBackslash, char **out_firstComment)
{
	char *line = curFile->buf_ptr;
	char *p = line;
	char *line_end = line;
	char *firstBackslash = NULL;
	char *firstComment = NULL;
	ParseRawLineResult res = PRLR_LINE;

	curFile->lineno++;

	for (;;) {
		char ch;

		if (p == curFile->buf_end) {
			res = PRLR_EOF;
			break;
		}

		ch = *p;
		if (ch == '\0' ||
		    (ch == '\\' && p + 1 < curFile->buf_end && p[1] == '\0')) {
			Parse_Error(PARSE_FATAL, "Zero byte read from file");
			return PRLR_ERROR;
		}

		/* Treat next character after '\' as literal. */
		if (ch == '\\') {
			if (firstBackslash == NULL)
				firstBackslash = p;
			if (p[1] == '\n') {
				curFile->lineno++;
				if (p + 2 == curFile->buf_end) {
					line_end = p;
					*line_end = '\n';
					p += 2;
					continue;
				}
			}
			p += 2;
			line_end = p;
			assert(p <= curFile->buf_end);
			continue;
		}

		/*
		 * Remember the first '#' for comment stripping, unless
		 * the previous char was '[', as in the modifier ':[#]'.
		 */
		if (ch == '#' && firstComment == NULL &&
		    !(p > line && p[-1] == '['))
			firstComment = line_end;

		p++;
		if (ch == '\n')
			break;

		/* We are not interested in trailing whitespace. */
		if (!ch_isspace(ch))
			line_end = p;
	}

	*out_line = line;
	curFile->buf_ptr = p;
	*out_line_end = line_end;
	*out_firstBackslash = firstBackslash;
	*out_firstComment = firstComment;
	return res;
}

/*
 * Beginning at start, unescape '\#' to '#' and replace backslash-newline
 * with a single space.
 */
static void
UnescapeBackslash(char *line, char *start)
{
	char *src = start;
	char *dst = start;
	char *spaceStart = line;

	for (;;) {
		char ch = *src++;
		if (ch != '\\') {
			if (ch == '\0')
				break;
			*dst++ = ch;
			continue;
		}

		ch = *src++;
		if (ch == '\0') {
			/* Delete '\\' at end of buffer */
			dst--;
			break;
		}

		/* Delete '\\' from before '#' on non-command lines */
		if (ch == '#' && line[0] != '\t') {
			*dst++ = ch;
			continue;
		}

		if (ch != '\n') {
			/* Leave '\\' in buffer for later */
			*dst++ = '\\';
			/*
			 * Make sure we don't delete an escaped ' ' from the
			 * line end.
			 */
			spaceStart = dst + 1;
			*dst++ = ch;
			continue;
		}

		/*
		 * Escaped '\n' -- replace following whitespace with a single
		 * ' '.
		 */
		pp_skip_hspace(&src);
		*dst++ = ' ';
	}

	/* Delete any trailing spaces - eg from empty continuations */
	while (dst > spaceStart && ch_isspace(dst[-1]))
		dst--;
	*dst = '\0';
}

typedef enum GetLineMode {
	/*
	 * Return the next line that is neither empty nor a comment.
	 * Backslash line continuations are folded into a single space.
	 * A trailing comment, if any, is discarded.
	 */
	GLM_NONEMPTY,

	/*
	 * Return the next line, even if it is empty or a comment.
	 * Preserve backslash-newline to keep the line numbers correct.
	 *
	 * Used in .for loops to collect the body of the loop while waiting
	 * for the corresponding .endfor.
	 */
	GLM_FOR_BODY,

	/*
	 * Return the next line that starts with a dot.
	 * Backslash line continuations are folded into a single space.
	 * A trailing comment, if any, is discarded.
	 *
	 * Used in .if directives to skip over irrelevant branches while
	 * waiting for the corresponding .endif.
	 */
	GLM_DOT
} GetLineMode;

/* Return the next "interesting" logical line from the current file. */
static char *
ParseGetLine(GetLineMode mode)
{
	IFile *curFile = CurFile();
	char *line;
	char *line_end;
	char *firstBackslash;
	char *firstComment;

	/* Loop through blank lines and comment lines */
	for (;;) {
		ParseRawLineResult res = ParseRawLine(curFile,
		    &line, &line_end, &firstBackslash, &firstComment);
		if (res == PRLR_ERROR)
			return NULL;

		if (line_end == line || firstComment == line) {
			if (res == PRLR_EOF)
				return NULL;
			if (mode != GLM_FOR_BODY)
				continue;
		}

		/* We now have a line of data */
		assert(ch_isspace(*line_end));
		*line_end = '\0';

		if (mode == GLM_FOR_BODY)
			return line;	/* Don't join the physical lines. */

		if (mode == GLM_DOT && line[0] != '.')
			continue;
		break;
	}

	/* Brutally ignore anything after a non-escaped '#' in non-commands. */
	if (firstComment != NULL && line[0] != '\t')
		*firstComment = '\0';

	/* If we didn't see a '\\' then the in-situ data is fine. */
	if (firstBackslash == NULL)
		return line;

	/* Remove escapes from '\n' and '#' */
	UnescapeBackslash(line, firstBackslash);

	return line;
}

static Boolean
ParseSkippedBranches(void)
{
	char *line;

	while ((line = ParseGetLine(GLM_DOT)) != NULL) {
		if (Cond_EvalLine(line) == COND_PARSE)
			break;
		/*
		 * TODO: Check for typos in .elif directives
		 * such as .elsif or .elseif.
		 *
		 * This check will probably duplicate some of
		 * the code in ParseLine.  Most of the code
		 * there cannot apply, only ParseVarassign and
		 * ParseDependency can, and to prevent code
		 * duplication, these would need to be called
		 * with a flag called onlyCheckSyntax.
		 *
		 * See directive-elif.mk for details.
		 */
	}

	return line != NULL;
}

static Boolean
ParseForLoop(const char *line)
{
	int rval;
	int firstLineno;

	rval = For_Eval(line);
	if (rval == 0)
		return FALSE;	/* Not a .for line */
	if (rval < 0)
		return TRUE;	/* Syntax error - error printed, ignore line */

	firstLineno = CurFile()->lineno;

	/* Accumulate loop lines until matching .endfor */
	do {
		line = ParseGetLine(GLM_FOR_BODY);
		if (line == NULL) {
			Parse_Error(PARSE_FATAL,
			    "Unexpected end of file in for loop.");
			break;
		}
	} while (For_Accum(line));

	For_Run(firstLineno);	/* Stash each iteration as a new 'input file' */

	return TRUE;		/* Read next line from for-loop buffer */
}

/*
 * Read an entire line from the input file.
 *
 * Empty lines, .if and .for are completely handled by this function,
 * leaving only variable assignments, other directives, dependency lines
 * and shell commands to the caller.
 *
 * Results:
 *	A line without its newline and without any trailing whitespace,
 *	or NULL.
 */
static char *
ParseReadLine(void)
{
	char *line;

	for (;;) {
		line = ParseGetLine(GLM_NONEMPTY);
		if (line == NULL)
			return NULL;

		if (line[0] != '.')
			return line;

		/*
		 * The line might be a conditional. Ask the conditional module
		 * about it and act accordingly
		 */
		switch (Cond_EvalLine(line)) {
		case COND_SKIP:
			if (!ParseSkippedBranches())
				return NULL;
			continue;
		case COND_PARSE:
			continue;
		case COND_INVALID:	/* Not a conditional line */
			if (ParseForLoop(line))
				continue;
			break;
		}
		return line;
	}
}

static void
FinishDependencyGroup(void)
{
	GNodeListNode *ln;

	if (targets == NULL)
		return;

	for (ln = targets->first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;

		Suff_EndTransform(gn);

		/*
		 * Mark the target as already having commands if it does, to
		 * keep from having shell commands on multiple dependency
		 * lines.
		 */
		if (!Lst_IsEmpty(&gn->commands))
			gn->type |= OP_HAS_COMMANDS;
	}

	Lst_Free(targets);
	targets = NULL;
}

/* Add the command to each target from the current dependency spec. */
static void
ParseLine_ShellCommand(const char *p)
{
	cpp_skip_whitespace(&p);
	if (*p == '\0')
		return;		/* skip empty commands */

	if (targets == NULL) {
		Parse_Error(PARSE_FATAL,
		    "Unassociated shell command \"%s\"", p);
		return;
	}

	{
		char *cmd = bmake_strdup(p);
		GNodeListNode *ln;

		for (ln = targets->first; ln != NULL; ln = ln->next) {
			GNode *gn = ln->datum;
			ParseAddCmd(gn, cmd);
		}
#ifdef CLEANUP
		Lst_Append(&targCmds, cmd);
#endif
	}
}

MAKE_INLINE Boolean
IsDirective(const char *dir, size_t dirlen, const char *name)
{
	return dirlen == strlen(name) && memcmp(dir, name, dirlen) == 0;
}

/*
 * See if the line starts with one of the known directives, and if so, handle
 * the directive.
 */
static Boolean
ParseDirective(char *line)
{
	char *cp = line + 1;
	const char *dir, *arg;
	size_t dirlen;

	pp_skip_whitespace(&cp);
	if (IsInclude(cp, FALSE)) {
		ParseDoInclude(cp);
		return TRUE;
	}

	dir = cp;
	while (ch_isalpha(*cp) || *cp == '-')
		cp++;
	dirlen = (size_t)(cp - dir);

	if (*cp != '\0' && !ch_isspace(*cp))
		return FALSE;

	pp_skip_whitespace(&cp);
	arg = cp;

	if (IsDirective(dir, dirlen, "undef")) {
		Var_Undef(cp);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "export")) {
		Var_Export(VEM_PLAIN, arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "export-env")) {
		Var_Export(VEM_ENV, arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "export-literal")) {
		Var_Export(VEM_LITERAL, arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "unexport")) {
		Var_UnExport(FALSE, arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "unexport-env")) {
		Var_UnExport(TRUE, arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "info")) {
		ParseMessage(PARSE_INFO, "info", arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "warning")) {
		ParseMessage(PARSE_WARNING, "warning", arg);
		return TRUE;
	} else if (IsDirective(dir, dirlen, "error")) {
		ParseMessage(PARSE_FATAL, "error", arg);
		return TRUE;
	}
	return FALSE;
}

static Boolean
ParseVarassign(const char *line)
{
	VarAssign var;

	if (!Parse_IsVar(line, &var))
		return FALSE;

	FinishDependencyGroup();
	Parse_DoVar(&var, VAR_GLOBAL);
	return TRUE;
}

static char *
FindSemicolon(char *p)
{
	int level = 0;

	for (; *p != '\0'; p++) {
		if (*p == '\\' && p[1] != '\0') {
			p++;
			continue;
		}

		if (*p == '$' && (p[1] == '(' || p[1] == '{'))
			level++;
		else if (level > 0 && (*p == ')' || *p == '}'))
			level--;
		else if (level == 0 && *p == ';')
			break;
	}
	return p;
}

/*
 * dependency	-> target... op [source...]
 * op		-> ':' | '::' | '!'
 */
static void
ParseDependency(char *line)
{
	VarEvalFlags eflags;
	char *expanded_line;
	const char *shellcmd = NULL;

	/*
	 * For some reason - probably to make the parser impossible -
	 * a ';' can be used to separate commands from dependencies.
	 * Attempt to avoid ';' inside substitution patterns.
	 */
	{
		char *semicolon = FindSemicolon(line);
		if (*semicolon != '\0') {
			/* Terminate the dependency list at the ';' */
			*semicolon = '\0';
			shellcmd = semicolon + 1;
		}
	}

	/*
	 * We now know it's a dependency line so it needs to have all
	 * variables expanded before being parsed.
	 *
	 * XXX: Ideally the dependency line would first be split into
	 * its left-hand side, dependency operator and right-hand side,
	 * and then each side would be expanded on its own.  This would
	 * allow for the left-hand side to allow only defined variables
	 * and to allow variables on the right-hand side to be undefined
	 * as well.
	 *
	 * Parsing the line first would also prevent that targets
	 * generated from variable expressions are interpreted as the
	 * dependency operator, such as in "target${:U\:} middle: source",
	 * in which the middle is interpreted as a source, not a target.
	 */

	/* In lint mode, allow undefined variables to appear in
	 * dependency lines.
	 *
	 * Ideally, only the right-hand side would allow undefined
	 * variables since it is common to have optional dependencies.
	 * Having undefined variables on the left-hand side is more
	 * unusual though.  Since both sides are expanded in a single
	 * pass, there is not much choice what to do here.
	 *
	 * In normal mode, it does not matter whether undefined
	 * variables are allowed or not since as of 2020-09-14,
	 * Var_Parse does not print any parse errors in such a case.
	 * It simply returns the special empty string var_Error,
	 * which cannot be detected in the result of Var_Subst. */
	eflags = opts.strict ? VARE_WANTRES : VARE_WANTRES | VARE_UNDEFERR;
	(void)Var_Subst(line, VAR_CMDLINE, eflags, &expanded_line);
	/* TODO: handle errors */

	/* Need a fresh list for the target nodes */
	if (targets != NULL)
		Lst_Free(targets);
	targets = Lst_New();

	ParseDoDependency(expanded_line);
	free(expanded_line);

	if (shellcmd != NULL)
		ParseLine_ShellCommand(shellcmd);
}

static void
ParseLine(char *line)
{
	/*
	 * Lines that begin with '.' can be pretty much anything:
	 *	- directives like '.include' or '.if',
	 *	- suffix rules like '.c.o:',
	 *	- dependencies for filenames that start with '.',
	 *	- variable assignments like '.tmp=value'.
	 */
	if (line[0] == '.' && ParseDirective(line))
		return;

	if (line[0] == '\t') {
		ParseLine_ShellCommand(line + 1);
		return;
	}

#ifdef SYSVINCLUDE
	if (IsSysVInclude(line)) {
		/*
		 * It's an S3/S5-style "include".
		 */
		ParseTraditionalInclude(line);
		return;
	}
#endif

#ifdef GMAKEEXPORT
	if (strncmp(line, "export", 6) == 0 && ch_isspace(line[6]) &&
	    strchr(line, ':') == NULL) {
		/*
		 * It's a Gmake "export".
		 */
		ParseGmakeExport(line);
		return;
	}
#endif

	if (ParseVarassign(line))
		return;

	FinishDependencyGroup();

	ParseDependency(line);
}

/*
 * Parse a top-level makefile, incorporating its content into the global
 * dependency graph.
 *
 * Input:
 *	name		The name of the file being read
 *	fd		The open file to parse; will be closed at the end
 */
void
Parse_File(const char *name, int fd)
{
	char *line;		/* the line we're working on */
	struct loadedfile *lf;

	lf = loadfile(name, fd);

	assert(targets == NULL);

	if (name == NULL)
		name = "(stdin)";

	Parse_SetInput(name, 0, -1, loadedfile_readMore, lf);
	CurFile()->lf = lf;

	do {
		while ((line = ParseReadLine()) != NULL) {
			DEBUG2(PARSE, "ParseReadLine (%d): '%s'\n",
			    CurFile()->lineno, line);
			ParseLine(line);
		}
		/* Reached EOF, but it may be just EOF of an include file. */
	} while (ParseEOF());

	FinishDependencyGroup();

	if (fatals != 0) {
		(void)fflush(stdout);
		(void)fprintf(stderr,
		    "%s: Fatal errors encountered -- cannot continue",
		    progname);
		PrintOnError(NULL, NULL);
		exit(1);
	}
}

/* Initialize the parsing module. */
void
Parse_Init(void)
{
	mainNode = NULL;
	parseIncPath = SearchPath_New();
	sysIncPath = SearchPath_New();
	defSysIncPath = SearchPath_New();
	Vector_Init(&includes, sizeof(IFile));
}

/* Clean up the parsing module. */
void
Parse_End(void)
{
#ifdef CLEANUP
	Lst_DoneCall(&targCmds, free);
	assert(targets == NULL);
	SearchPath_Free(defSysIncPath);
	SearchPath_Free(sysIncPath);
	SearchPath_Free(parseIncPath);
	assert(includes.len == 0);
	Vector_Done(&includes);
#endif
}


/*
 * Return a list containing the single main target to create.
 * If no such target exists, we Punt with an obnoxious error message.
 */
void
Parse_MainName(GNodeList *mainList)
{
	if (mainNode == NULL)
		Punt("no target to make.");

	if (mainNode->type & OP_DOUBLEDEP) {
		Lst_Append(mainList, mainNode);
		Lst_AppendAll(mainList, &mainNode->cohorts);
	} else
		Lst_Append(mainList, mainNode);

	Var_Append(".TARGETS", mainNode->name, VAR_GLOBAL);
}

int
Parse_GetFatals(void)
{
	return fatals;
}
