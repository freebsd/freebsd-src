/*	$NetBSD: parse.c,v 1.704 2023/06/23 06:08:56 rillig Exp $	*/

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
 * Interface:
 *	Parse_Init	Initialize the module
 *
 *	Parse_End	Clean up the module
 *
 *	Parse_File	Parse a top-level makefile.  Included files are
 *			handled by IncludeFile instead.
 *
 *	Parse_VarAssign
 *			Try to parse the given line as a variable assignment.
 *			Used by MainParseArgs to determine if an argument is
 *			a target or a variable assignment.  Used internally
 *			for pretty much the same thing.
 *
 *	Parse_Error	Report a parse error, a warning or an informational
 *			message.
 *
 *	Parse_MainName	Populate the list of targets to create.
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
MAKE_RCSID("$NetBSD: parse.c,v 1.704 2023/06/23 06:08:56 rillig Exp $");

/* Detects a multiple-inclusion guard in a makefile. */
typedef enum {
	GS_START,		/* at the beginning of the file */
	GS_COND,		/* after the guard condition */
	GS_DONE,		/* after the closing .endif */
	GS_NO			/* the file is not guarded */
} GuardState;

/*
 * A file being read.
 */
typedef struct IncludedFile {
	FStr name;		/* absolute or relative to the cwd */
	unsigned lineno;	/* 1-based */
	unsigned readLines;	/* the number of physical lines that have
				 * been read from the file */
	unsigned forHeadLineno;	/* 1-based */
	unsigned forBodyReadLines; /* the number of physical lines that have
				 * been read from the file above the body of
				 * the .for loop */
	unsigned int condMinDepth; /* depth of nested 'if' directives, at the
				 * beginning of the file */
	bool depending;		/* state of doing_depend on EOF */

	Buffer buf;		/* the file's content or the body of the .for
				 * loop; either empty or ends with '\n' */
	char *buf_ptr;		/* next char to be read from buf */
	char *buf_end;		/* buf_end[-1] == '\n' */

	GuardState guardState;
	Guard *guard;

	struct ForLoop *forLoop;
} IncludedFile;

/* Special attributes for target nodes. */
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
	SP_MAIN,	/* .MAIN and no user-specified targets to make */
	SP_META,	/* .META */
	SP_MFLAGS,	/* .MFLAGS or .MAKEFLAGS */
	SP_NOMETA,	/* .NOMETA */
	SP_NOMETA_CMP,	/* .NOMETA_CMP */
	SP_NOPATH,	/* .NOPATH */
	SP_NOREADONLY,	/* .NOREADONLY */
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
	SP_READONLY,	/* .READONLY */
	SP_SHELL,	/* .SHELL */
	SP_SILENT,	/* .SILENT */
	SP_SINGLESHELL,	/* .SINGLESHELL; not mentioned in the manual page */
	SP_STALE,	/* .STALE */
	SP_SUFFIXES,	/* .SUFFIXES */
	SP_SYSPATH,	/* .SYSPATH */
	SP_WAIT		/* .WAIT */
} ParseSpecial;

typedef List SearchPathList;
typedef ListNode SearchPathListNode;


typedef enum VarAssignOp {
	VAR_NORMAL,		/* = */
	VAR_APPEND,		/* += */
	VAR_DEFAULT,		/* ?= */
	VAR_SUBST,		/* := */
	VAR_SHELL		/* != or :sh= */
} VarAssignOp;

typedef struct VarAssign {
	char *varname;		/* unexpanded */
	VarAssignOp op;
	const char *value;	/* unexpanded */
} VarAssign;

static bool Parse_IsVar(const char *, VarAssign *);
static void Parse_Var(VarAssign *, GNode *);

/*
 * The target to be made if no targets are specified in the command line.
 * This is the first target defined in any of the makefiles.
 */
GNode *mainNode;

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
 * is seen, then set to each successive source on the line.
 */
static GNode *order_pred;

static int parseErrors = 0;

/*
 * The include chain of makefiles.  At index 0 is the top-level makefile from
 * the command line, followed by the included files or .for loops, up to and
 * including the current file.
 *
 * See PrintStackTrace for how to interpret the data.
 */
static Vector /* of IncludedFile */ includes;

SearchPath *parseIncPath;	/* directories for "..." includes */
SearchPath *sysIncPath;		/* directories for <...> includes */
SearchPath *defSysIncPath;	/* default for sysIncPath */

/*
 * The parseKeywords table is searched using binary search when deciding
 * if a target or source is special.
 */
static const struct {
	const char name[17];
	ParseSpecial special;	/* when used as a target */
	GNodeType targetAttr;	/* when used as a source */
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
    { ".NOREADONLY",	SP_NOREADONLY,	OP_NONE },
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
    { ".READONLY",	SP_READONLY,	OP_NONE },
    { ".RECURSIVE",	SP_ATTRIBUTE,	OP_MAKE },
    { ".SHELL",		SP_SHELL,	OP_NONE },
    { ".SILENT",	SP_SILENT,	OP_SILENT },
    { ".SINGLESHELL",	SP_SINGLESHELL,	OP_NONE },
    { ".STALE",		SP_STALE,	OP_NONE },
    { ".SUFFIXES",	SP_SUFFIXES,	OP_NONE },
    { ".SYSPATH",	SP_SYSPATH,	OP_NONE },
    { ".USE",		SP_ATTRIBUTE,	OP_USE },
    { ".USEBEFORE",	SP_ATTRIBUTE,	OP_USEBEFORE },
    { ".WAIT",		SP_WAIT,	OP_NONE },
};

enum PosixState posix_state = PS_NOT_YET;

static HashTable /* full file name -> Guard */ guards;

static IncludedFile *
GetInclude(size_t i)
{
	assert(i < includes.len);
	return Vector_Get(&includes, i);
}

/* The makefile that is currently being read. */
static IncludedFile *
CurFile(void)
{
	return GetInclude(includes.len - 1);
}

unsigned int
CurFile_CondMinDepth(void)
{
	return CurFile()->condMinDepth;
}

static Buffer
LoadFile(const char *path, int fd)
{
	ssize_t n;
	Buffer buf;
	size_t bufSize;
	struct stat st;

	bufSize = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
		  st.st_size > 0 && st.st_size < 1024 * 1024 * 1024
	    ? (size_t)st.st_size : 1024;
	Buf_InitSize(&buf, bufSize);

	for (;;) {
		if (buf.len == buf.cap) {
			if (buf.cap >= 512 * 1024 * 1024) {
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

	return buf;		/* may not be null-terminated */
}

/*
 * Print the current chain of .include and .for directives.  In Parse_Fatal
 * or other functions that already print the location, includingInnermost
 * would be redundant, but in other cases like Error or Fatal it needs to be
 * included.
 */
void
PrintStackTrace(bool includingInnermost)
{
	const IncludedFile *entries;
	size_t i, n;

	n = includes.len;
	if (n == 0)
		return;

	entries = GetInclude(0);
	if (!includingInnermost && entries[n - 1].forLoop == NULL)
		n--;		/* already in the diagnostic */

	for (i = n; i-- > 0;) {
		const IncludedFile *entry = entries + i;
		const char *fname = entry->name.str;
		char dirbuf[MAXPATHLEN + 1];

		if (fname[0] != '/' && strcmp(fname, "(stdin)") != 0) {
			const char *realPath = realpath(fname, dirbuf);
			if (realPath != NULL)
				fname = realPath;
		}

		if (entry->forLoop != NULL) {
			char *details = ForLoop_Details(entry->forLoop);
			debug_printf("\tin .for loop from %s:%u with %s\n",
			    fname, entry->forHeadLineno, details);
			free(details);
		} else if (i + 1 < n && entries[i + 1].forLoop != NULL) {
			/* entry->lineno is not a useful line number */
		} else
			debug_printf("\tin %s:%u\n", fname, entry->lineno);
	}
}

/* Check if the current character is escaped on the current line. */
static bool
IsEscaped(const char *line, const char *p)
{
	bool escaped = false;
	while (p > line && *--p == '\\')
		escaped = !escaped;
	return escaped;
}

/*
 * Add the filename and lineno to the GNode so that we remember where its
 * last command was added or where it was mentioned in a .depend file.
 */
static void
RememberLocation(GNode *gn)
{
	IncludedFile *curFile = CurFile();
	gn->fname = Str_Intern(curFile->name.str);
	gn->lineno = curFile->lineno;
}

/*
 * Look in the table of keywords for one matching the given string.
 * Return the index of the keyword, or -1 if it isn't there.
 */
static int
FindKeyword(const char *str)
{
	int start = 0;
	int end = sizeof parseKeywords / sizeof parseKeywords[0] - 1;

	while (start <= end) {
		int curr = start + (end - start) / 2;
		int diff = strcmp(str, parseKeywords[curr].name);

		if (diff == 0)
			return curr;
		if (diff < 0)
			end = curr - 1;
		else
			start = curr + 1;
	}

	return -1;
}

void
PrintLocation(FILE *f, bool useVars, const GNode *gn)
{
	char dirbuf[MAXPATHLEN + 1];
	FStr dir, base;
	const char *fname;
	unsigned lineno;

	if (gn != NULL) {
		fname = gn->fname;
		lineno = gn->lineno;
	} else if (includes.len > 0) {
		IncludedFile *curFile = CurFile();
		fname = curFile->name.str;
		lineno = curFile->lineno;
	} else
		return;

	if (!useVars || fname[0] == '/' || strcmp(fname, "(stdin)") == 0) {
		(void)fprintf(f, "\"%s\" line %u: ", fname, lineno);
		return;
	}

	dir = Var_Value(SCOPE_GLOBAL, ".PARSEDIR");
	if (dir.str == NULL)
		dir.str = ".";
	if (dir.str[0] != '/')
		dir.str = realpath(dir.str, dirbuf);

	base = Var_Value(SCOPE_GLOBAL, ".PARSEFILE");
	if (base.str == NULL)
		base.str = str_basename(fname);

	(void)fprintf(f, "\"%s/%s\" line %u: ", dir.str, base.str, lineno);

	FStr_Done(&base);
	FStr_Done(&dir);
}

static void MAKE_ATTR_PRINTFLIKE(5, 0)
ParseVErrorInternal(FILE *f, bool useVars, const GNode *gn,
		    ParseErrorLevel level, const char *fmt, va_list ap)
{
	static bool fatal_warning_error_printed = false;

	(void)fprintf(f, "%s: ", progname);

	PrintLocation(f, useVars, gn);
	if (level == PARSE_WARNING)
		(void)fprintf(f, "warning: ");
	(void)vfprintf(f, fmt, ap);
	(void)fprintf(f, "\n");
	(void)fflush(f);

	if (level == PARSE_FATAL)
		parseErrors++;
	if (level == PARSE_WARNING && opts.parseWarnFatal) {
		if (!fatal_warning_error_printed) {
			Error("parsing warnings being treated as errors");
			fatal_warning_error_printed = true;
		}
		parseErrors++;
	}

	if (DEBUG(PARSE))
		PrintStackTrace(false);
}

static void MAKE_ATTR_PRINTFLIKE(3, 4)
ParseErrorInternal(const GNode *gn,
		   ParseErrorLevel level, const char *fmt, ...)
{
	va_list ap;

	(void)fflush(stdout);
	va_start(ap, fmt);
	ParseVErrorInternal(stderr, false, gn, level, fmt, ap);
	va_end(ap);

	if (opts.debug_file != stdout && opts.debug_file != stderr) {
		va_start(ap, fmt);
		ParseVErrorInternal(opts.debug_file, false, gn,
		    level, fmt, ap);
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
Parse_Error(ParseErrorLevel level, const char *fmt, ...)
{
	va_list ap;

	(void)fflush(stdout);
	va_start(ap, fmt);
	ParseVErrorInternal(stderr, true, NULL, level, fmt, ap);
	va_end(ap);

	if (opts.debug_file != stdout && opts.debug_file != stderr) {
		va_start(ap, fmt);
		ParseVErrorInternal(opts.debug_file, true, NULL,
		    level, fmt, ap);
		va_end(ap);
	}
}


/*
 * Handle an .info, .warning or .error directive.  For an .error directive,
 * exit immediately.
 */
static void
HandleMessage(ParseErrorLevel level, const char *levelName, const char *umsg)
{
	char *xmsg;

	if (umsg[0] == '\0') {
		Parse_Error(PARSE_FATAL, "Missing argument for \".%s\"",
		    levelName);
		return;
	}

	xmsg = Var_Subst(umsg, SCOPE_CMDLINE, VARE_WANTRES);
	/* TODO: handle errors */

	Parse_Error(level, "%s", xmsg);
	free(xmsg);

	if (level == PARSE_FATAL) {
		PrintOnError(NULL, "\n");
		exit(1);
	}
}

/*
 * Add the child to the parent's children, and for non-special targets, vice
 * versa.  Special targets such as .END do not need to be informed once the
 * child target has been made.
 */
static void
LinkSource(GNode *pgn, GNode *cgn, bool isSpecial)
{
	if ((pgn->type & OP_DOUBLEDEP) && !Lst_IsEmpty(&pgn->cohorts))
		pgn = pgn->cohorts.last->datum;

	Lst_Append(&pgn->children, cgn);
	pgn->unmade++;

	/* Special targets like .END don't need any children. */
	if (!isSpecial)
		Lst_Append(&cgn->parents, pgn);

	if (DEBUG(PARSE)) {
		debug_printf("# LinkSource: added child %s - %s\n",
		    pgn->name, cgn->name);
		Targ_PrintNode(pgn, 0);
		Targ_PrintNode(cgn, 0);
	}
}

/* Add the node to each target from the current dependency group. */
static void
LinkToTargets(GNode *gn, bool isSpecial)
{
	GNodeListNode *ln;

	for (ln = targets->first; ln != NULL; ln = ln->next)
		LinkSource(ln->datum, gn, isSpecial);
}

static bool
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
		return false;
	}

	if (op == OP_DOUBLEDEP && (gn->type & OP_OPMASK) == OP_DOUBLEDEP) {
		/*
		 * If the node was on the left-hand side of a '::' operator,
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
		gn->type |= op & (unsigned)~OP_OPMASK;

		cohort = Targ_NewInternalNode(gn->name);
		if (doing_depend)
			RememberLocation(cohort);
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

	return true;
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
ApplyDependencySourceWait(bool isSpecial)
{
	static unsigned wait_number = 0;
	char name[6 + 10 + 1];
	GNode *gn;

	snprintf(name, sizeof name, ".WAIT_%u", ++wait_number);
	gn = Targ_NewInternalNode(name);
	if (doing_depend)
		RememberLocation(gn);
	gn->type = OP_WAIT | OP_PHONY | OP_DEPENDS | OP_NOTMAIN;
	LinkToTargets(gn, isSpecial);
}

static bool
ApplyDependencySourceKeyword(const char *src, ParseSpecial special)
{
	int keywd;
	GNodeType targetAttr;

	if (*src != '.' || !ch_isupper(src[1]))
		return false;

	keywd = FindKeyword(src);
	if (keywd == -1)
		return false;

	targetAttr = parseKeywords[keywd].targetAttr;
	if (targetAttr != OP_NONE) {
		ApplyDependencyOperator(targetAttr);
		return true;
	}
	if (parseKeywords[keywd].special == SP_WAIT) {
		ApplyDependencySourceWait(special != SP_NOT);
		return true;
	}
	return false;
}

/*
 * In a line like ".MAIN: source1 source2", add all sources to the list of
 * things to create, but only if the user didn't specify a target on the
 * command line and .MAIN occurs for the first time.
 *
 * See HandleDependencyTargetSpecial, branch SP_MAIN.
 * See unit-tests/cond-func-make-main.mk.
 */
static void
ApplyDependencySourceMain(const char *src)
{
	Lst_Append(&opts.create, bmake_strdup(src));
	/*
	 * Add the name to the .TARGETS variable as well, so the user can
	 * employ that, if desired.
	 */
	Global_Append(".TARGETS", src);
}

/*
 * For the sources of a .ORDER target, create predecessor/successor links
 * between the previous source and the current one.
 */
static void
ApplyDependencySourceOrder(const char *src)
{
	GNode *gn;

	gn = Targ_GetNode(src);
	if (doing_depend)
		RememberLocation(gn);
	if (order_pred != NULL) {
		Lst_Append(&order_pred->order_succ, gn);
		Lst_Append(&gn->order_pred, order_pred);
		if (DEBUG(PARSE)) {
			debug_printf(
			    "# .ORDER forces '%s' to be made before '%s'\n",
			    order_pred->name, gn->name);
			Targ_PrintNode(order_pred, 0);
			Targ_PrintNode(gn, 0);
		}
	}
	/*
	 * The current source now becomes the predecessor for the next one.
	 */
	order_pred = gn;
}

/* The source is not an attribute, so find/create a node for it. */
static void
ApplyDependencySourceOther(const char *src, GNodeType targetAttr,
			   ParseSpecial special)
{
	GNode *gn;

	gn = Targ_GetNode(src);
	if (doing_depend)
		RememberLocation(gn);
	if (targetAttr != OP_NONE)
		gn->type |= targetAttr;
	else
		LinkToTargets(gn, special != SP_NOT);
}

/*
 * Given the name of a source in a dependency line, figure out if it is an
 * attribute (such as .SILENT) and if so, apply it to all targets. Otherwise
 * decide if there is some attribute which should be applied *to* the source
 * because of some special target (such as .PHONY) and apply it if so.
 * Otherwise, make the source a child of the targets.
 */
static void
ApplyDependencySource(GNodeType targetAttr, const char *src,
		      ParseSpecial special)
{
	if (ApplyDependencySourceKeyword(src, special))
		return;

	if (special == SP_MAIN)
		ApplyDependencySourceMain(src);
	else if (special == SP_ORDER)
		ApplyDependencySourceOrder(src);
	else
		ApplyDependencySourceOther(src, targetAttr, special);
}

/*
 * If we have yet to decide on a main target to make, in the absence of any
 * user input, we want the first target on the first dependency line that is
 * actually a real target (i.e. isn't a .USE or .EXEC rule) to be made.
 */
static void
MaybeUpdateMainTarget(void)
{
	GNodeListNode *ln;

	if (mainNode != NULL)
		return;

	for (ln = targets->first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		if (GNode_IsMainCandidate(gn)) {
			DEBUG1(MAKE, "Setting main node to \"%s\"\n", gn->name);
			mainNode = gn;
			return;
		}
	}
}

static void
InvalidLineType(const char *line)
{
	if (strncmp(line, "<<<<<<", 6) == 0 ||
	    strncmp(line, ">>>>>>", 6) == 0)
		Parse_Error(PARSE_FATAL,
		    "Makefile appears to contain unresolved CVS/RCS/??? merge conflicts");
	else if (line[0] == '.') {
		const char *dirstart = line + 1;
		const char *dirend;
		cpp_skip_whitespace(&dirstart);
		dirend = dirstart;
		while (ch_isalnum(*dirend) || *dirend == '-')
			dirend++;
		Parse_Error(PARSE_FATAL, "Unknown directive \"%.*s\"",
		    (int)(dirend - dirstart), dirstart);
	} else
		Parse_Error(PARSE_FATAL, "Invalid line type");
}

static void
ParseDependencyTargetWord(char **pp, const char *lstart)
{
	const char *cp = *pp;

	while (*cp != '\0') {
		if ((ch_isspace(*cp) || *cp == '!' || *cp == ':' ||
		     *cp == '(') &&
		    !IsEscaped(lstart, cp))
			break;

		if (*cp == '$') {
			/*
			 * Must be a dynamic source (would have been expanded
			 * otherwise).
			 *
			 * There should be no errors in this, as they would
			 * have been discovered in the initial Var_Subst and
			 * we wouldn't be here.
			 */
			FStr val = Var_Parse(&cp, SCOPE_CMDLINE,
			    VARE_PARSE_ONLY);
			FStr_Done(&val);
		} else
			cp++;
	}

	*pp += cp - *pp;
}

/*
 * Handle special targets like .PATH, .DEFAULT, .BEGIN, .ORDER.
 *
 * See the tests deptgt-*.mk.
 */
static void
HandleDependencyTargetSpecial(const char *targetName,
			      ParseSpecial *inout_special,
			      SearchPathList **inout_paths)
{
	switch (*inout_special) {
	case SP_PATH:
		if (*inout_paths == NULL)
			*inout_paths = Lst_New();
		Lst_Append(*inout_paths, &dirSearchPath);
		break;
	case SP_SYSPATH:
		if (*inout_paths == NULL)
			*inout_paths = Lst_New();
		Lst_Append(*inout_paths, sysIncPath);
		break;
	case SP_MAIN:
		/*
		 * Allow targets from the command line to override the
		 * .MAIN node.
		 */
		if (!Lst_IsEmpty(&opts.create))
			*inout_special = SP_NOT;
		break;
	case SP_BEGIN:
	case SP_END:
	case SP_STALE:
	case SP_ERROR:
	case SP_INTERRUPT: {
		GNode *gn = Targ_GetNode(targetName);
		if (doing_depend)
			RememberLocation(gn);
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
		deleteOnError = true;
		break;
	case SP_NOTPARALLEL:
		opts.maxJobs = 1;
		break;
	case SP_SINGLESHELL:
		opts.compatMake = true;
		break;
	case SP_ORDER:
		order_pred = NULL;
		break;
	default:
		break;
	}
}

static bool
HandleDependencyTargetPath(const char *suffixName,
			   SearchPathList **inout_paths)
{
	SearchPath *path;

	path = Suff_GetPath(suffixName);
	if (path == NULL) {
		Parse_Error(PARSE_FATAL,
		    "Suffix '%s' not defined (yet)", suffixName);
		return false;
	}

	if (*inout_paths == NULL)
		*inout_paths = Lst_New();
	Lst_Append(*inout_paths, path);

	return true;
}

/* See if it's a special target and if so set inout_special to match it. */
static bool
HandleDependencyTarget(const char *targetName,
		       ParseSpecial *inout_special,
		       GNodeType *inout_targetAttr,
		       SearchPathList **inout_paths)
{
	int keywd;

	if (!(targetName[0] == '.' && ch_isupper(targetName[1])))
		return true;

	/*
	 * See if the target is a special target that must have it
	 * or its sources handled specially.
	 */
	keywd = FindKeyword(targetName);
	if (keywd != -1) {
		if (*inout_special == SP_PATH &&
		    parseKeywords[keywd].special != SP_PATH) {
			Parse_Error(PARSE_FATAL, "Mismatched special targets");
			return false;
		}

		*inout_special = parseKeywords[keywd].special;
		*inout_targetAttr = parseKeywords[keywd].targetAttr;

		HandleDependencyTargetSpecial(targetName, inout_special,
		    inout_paths);

	} else if (strncmp(targetName, ".PATH", 5) == 0) {
		*inout_special = SP_PATH;
		if (!HandleDependencyTargetPath(targetName + 5, inout_paths))
			return false;
	}
	return true;
}

static void
HandleSingleDependencyTargetMundane(const char *name)
{
	GNode *gn = Suff_IsTransform(name)
	    ? Suff_AddTransform(name)
	    : Targ_GetNode(name);
	if (doing_depend)
		RememberLocation(gn);

	Lst_Append(targets, gn);
}

static void
HandleDependencyTargetMundane(const char *targetName)
{
	if (Dir_HasWildcards(targetName)) {
		StringList targetNames = LST_INIT;

		SearchPath *emptyPath = SearchPath_New();
		SearchPath_Expand(emptyPath, targetName, &targetNames);
		SearchPath_Free(emptyPath);

		while (!Lst_IsEmpty(&targetNames)) {
			char *targName = Lst_Dequeue(&targetNames);
			HandleSingleDependencyTargetMundane(targName);
			free(targName);
		}
	} else
		HandleSingleDependencyTargetMundane(targetName);
}

static void
SkipExtraTargets(char **pp, const char *lstart)
{
	bool warning = false;
	const char *p = *pp;

	while (*p != '\0') {
		if (!IsEscaped(lstart, p) && (*p == '!' || *p == ':'))
			break;
		if (IsEscaped(lstart, p) || (*p != ' ' && *p != '\t'))
			warning = true;
		p++;
	}
	if (warning) {
		const char *start = *pp;
		cpp_skip_whitespace(&start);
		Parse_Error(PARSE_WARNING, "Extra target '%.*s' ignored",
		    (int)(p - start), start);
	}

	*pp += p - *pp;
}

static void
CheckSpecialMundaneMixture(ParseSpecial special)
{
	switch (special) {
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
		/* Nothing special here -- targets may be empty. */
		break;
	default:
		Parse_Error(PARSE_WARNING,
		    "Special and mundane targets don't mix. "
		    "Mundane ones ignored");
		break;
	}
}

/*
 * In a dependency line like 'targets: sources' or 'targets! sources', parse
 * the operator ':', '::' or '!' from between the targets and the sources.
 */
static GNodeType
ParseDependencyOp(char **pp)
{
	if (**pp == '!')
		return (*pp)++, OP_FORCE;
	if (**pp == ':' && (*pp)[1] == ':')
		return *pp += 2, OP_DOUBLEDEP;
	else if (**pp == ':')
		return (*pp)++, OP_DEPENDS;
	else
		return OP_NONE;
}

static void
ClearPaths(ParseSpecial special, SearchPathList *paths)
{
	if (paths != NULL) {
		SearchPathListNode *ln;
		for (ln = paths->first; ln != NULL; ln = ln->next)
			SearchPath_Clear(ln->datum);
	}
	if (special == SP_SYSPATH)
		Dir_SetSYSPATH();
	else
		Dir_SetPATH();
}

static char *
FindInDirOfIncludingFile(const char *file)
{
	char *fullname, *incdir, *slash, *newName;
	int i;

	fullname = NULL;
	incdir = bmake_strdup(CurFile()->name.str);
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
			fullname = Dir_FindFile(newName, &dirSearchPath);
		free(newName);
	}
	free(incdir);
	return fullname;
}

static char *
FindInQuotPath(const char *file)
{
	const char *suff;
	SearchPath *suffPath;
	char *fullname;

	fullname = FindInDirOfIncludingFile(file);
	if (fullname == NULL &&
	    (suff = strrchr(file, '.')) != NULL &&
	    (suffPath = Suff_GetPath(suff)) != NULL)
		fullname = Dir_FindFile(file, suffPath);
	if (fullname == NULL)
		fullname = Dir_FindFile(file, parseIncPath);
	if (fullname == NULL)
		fullname = Dir_FindFile(file, &dirSearchPath);
	return fullname;
}

static bool
SkipGuarded(const char *fullname)
{
	Guard *guard = HashTable_FindValue(&guards, fullname);
	if (guard != NULL && guard->kind == GK_VARIABLE
	    && GNode_ValueDirect(SCOPE_GLOBAL, guard->name) != NULL)
		goto skip;
	if (guard != NULL && guard->kind == GK_TARGET
	    && Targ_FindNode(guard->name) != NULL)
		goto skip;
	return false;

skip:
	DEBUG2(PARSE, "Skipping '%s' because '%s' is defined\n",
	    fullname, guard->name);
	return true;
}

/*
 * Handle one of the .[-ds]include directives by remembering the current file
 * and pushing the included file on the stack.  After the included file has
 * finished, parsing continues with the including file; see Parse_PushInput
 * and ParseEOF.
 *
 * System includes are looked up in sysIncPath, any other includes are looked
 * up in the parsedir and then in the directories specified by the -I command
 * line options.
 */
static void
IncludeFile(const char *file, bool isSystem, bool depinc, bool silent)
{
	Buffer buf;
	char *fullname;		/* full pathname of file */
	int fd;

	fullname = file[0] == '/' ? bmake_strdup(file) : NULL;

	if (fullname == NULL && !isSystem)
		fullname = FindInQuotPath(file);

	if (fullname == NULL) {
		SearchPath *path = Lst_IsEmpty(&sysIncPath->dirs)
		    ? defSysIncPath : sysIncPath;
		fullname = Dir_FindFile(file, path);
	}

	if (fullname == NULL) {
		if (!silent)
			Parse_Error(PARSE_FATAL, "Could not find %s", file);
		return;
	}

	if (SkipGuarded(fullname))
		return;

	if ((fd = open(fullname, O_RDONLY)) == -1) {
		if (!silent)
			Parse_Error(PARSE_FATAL, "Cannot open %s", fullname);
		free(fullname);
		return;
	}

	buf = LoadFile(fullname, fd);
	(void)close(fd);

	Parse_PushInput(fullname, 1, 0, buf, NULL);
	if (depinc)
		doing_depend = depinc;	/* only turn it on */
	free(fullname);
}

/* Handle a "dependency" line like '.SPECIAL:' without any sources. */
static void
HandleDependencySourcesEmpty(ParseSpecial special, SearchPathList *paths)
{
	switch (special) {
	case SP_SUFFIXES:
		Suff_ClearSuffixes();
		break;
	case SP_PRECIOUS:
		allPrecious = true;
		break;
	case SP_IGNORE:
		opts.ignoreErrors = true;
		break;
	case SP_SILENT:
		opts.silent = true;
		break;
	case SP_PATH:
	case SP_SYSPATH:
		ClearPaths(special, paths);
		break;
#ifdef POSIX
	case SP_POSIX:
		if (posix_state == PS_NOW_OR_NEVER) {
			/*
			 * With '-r', 'posix.mk' (if it exists)
			 * can effectively substitute for 'sys.mk',
			 * otherwise it is an extension.
			 */
			Global_Set("%POSIX", "1003.2");
			IncludeFile("posix.mk", true, false, true);
		}
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
			(void)SearchPath_Add(ln->datum, dir);
	}
}

/*
 * If the target was one that doesn't take files as its sources but takes
 * something like suffixes, we take each space-separated word on the line as
 * a something and deal with it accordingly.
 */
static void
ParseDependencySourceSpecial(ParseSpecial special, const char *word,
			     SearchPathList *paths)
{
	switch (special) {
	case SP_SUFFIXES:
		Suff_AddSuffix(word);
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
	case SP_NOREADONLY:
		Var_ReadOnly(word, false);
		break;
	case SP_NULL:
		Suff_SetNull(word);
		break;
	case SP_OBJDIR:
		Main_SetObjdir(false, "%s", word);
		break;
	case SP_READONLY:
		Var_ReadOnly(word, true);
		break;
	case SP_SYSPATH:
		AddToPaths(word, paths);
		break;
	default:
		break;
	}
}

static bool
ApplyDependencyTarget(char *name, char *nameEnd, ParseSpecial *inout_special,
		      GNodeType *inout_targetAttr,
		      SearchPathList **inout_paths)
{
	char savec = *nameEnd;
	*nameEnd = '\0';

	if (!HandleDependencyTarget(name, inout_special,
	    inout_targetAttr, inout_paths))
		return false;

	if (*inout_special == SP_NOT && *name != '\0')
		HandleDependencyTargetMundane(name);
	else if (*inout_special == SP_PATH && *name != '.' && *name != '\0')
		Parse_Error(PARSE_WARNING, "Extra target (%s) ignored", name);

	*nameEnd = savec;
	return true;
}

static bool
ParseDependencyTargets(char **pp,
		       const char *lstart,
		       ParseSpecial *inout_special,
		       GNodeType *inout_targetAttr,
		       SearchPathList **inout_paths)
{
	char *p = *pp;

	for (;;) {
		char *tgt = p;

		ParseDependencyTargetWord(&p, lstart);

		/*
		 * If the word is followed by a left parenthesis, it's the
		 * name of one or more files inside an archive.
		 */
		if (!IsEscaped(lstart, p) && *p == '(') {
			p = tgt;
			if (!Arch_ParseArchive(&p, targets, SCOPE_CMDLINE)) {
				Parse_Error(PARSE_FATAL,
				    "Error in archive specification: \"%s\"",
				    tgt);
				return false;
			}
			continue;
		}

		if (*p == '\0') {
			InvalidLineType(lstart);
			return false;
		}

		if (!ApplyDependencyTarget(tgt, p, inout_special,
		    inout_targetAttr, inout_paths))
			return false;

		if (*inout_special != SP_NOT && *inout_special != SP_PATH)
			SkipExtraTargets(&p, lstart);
		else
			pp_skip_whitespace(&p);

		if (*p == '\0')
			break;
		if ((*p == '!' || *p == ':') && !IsEscaped(lstart, p))
			break;
	}

	*pp = p;
	return true;
}

static void
ParseDependencySourcesSpecial(char *start,
			      ParseSpecial special, SearchPathList *paths)
{
	char savec;

	while (*start != '\0') {
		char *end = start;
		while (*end != '\0' && !ch_isspace(*end))
			end++;
		savec = *end;
		*end = '\0';
		ParseDependencySourceSpecial(special, start, paths);
		*end = savec;
		if (savec != '\0')
			end++;
		pp_skip_whitespace(&end);
		start = end;
	}
}

static void
LinkVarToTargets(VarAssign *var)
{
	GNodeListNode *ln;

	for (ln = targets->first; ln != NULL; ln = ln->next)
		Parse_Var(var, ln->datum);
}

static bool
ParseDependencySourcesMundane(char *start,
			      ParseSpecial special, GNodeType targetAttr)
{
	while (*start != '\0') {
		char *end = start;
		VarAssign var;

		/*
		 * Check for local variable assignment,
		 * rest of the line is the value.
		 */
		if (Parse_IsVar(start, &var)) {
			/*
			 * Check if this makefile has disabled
			 * setting local variables.
			 */
			bool target_vars = GetBooleanExpr(
			    "${.MAKE.TARGET_LOCAL_VARIABLES}", true);

			if (target_vars)
				LinkVarToTargets(&var);
			free(var.varname);
			if (target_vars)
				return true;
		}

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
			if (!Arch_ParseArchive(&start, &sources,
			    SCOPE_CMDLINE)) {
				Parse_Error(PARSE_FATAL,
				    "Error in source archive spec \"%s\"",
				    start);
				return false;
			}

			while (!Lst_IsEmpty(&sources)) {
				GNode *gn = Lst_Dequeue(&sources);
				ApplyDependencySource(targetAttr, gn->name,
				    special);
			}
			Lst_Done(&sources);
			end = start;
		} else {
			if (*end != '\0') {
				*end = '\0';
				end++;
			}

			ApplyDependencySource(targetAttr, start, special);
		}
		pp_skip_whitespace(&end);
		start = end;
	}
	return true;
}

/*
 * From a dependency line like 'targets: sources', parse the sources.
 *
 * See the tests depsrc-*.mk.
 */
static void
ParseDependencySources(char *p, GNodeType targetAttr,
		       ParseSpecial special, SearchPathList **inout_paths)
{
	if (*p == '\0') {
		HandleDependencySourcesEmpty(special, *inout_paths);
	} else if (special == SP_MFLAGS) {
		Main_ParseArgLine(p);
		return;
	} else if (special == SP_SHELL) {
		if (!Job_ParseShell(p)) {
			Parse_Error(PARSE_FATAL,
			    "improper shell specification");
			return;
		}
		return;
	} else if (special == SP_NOTPARALLEL || special == SP_SINGLESHELL ||
		   special == SP_DELETE_ON_ERROR) {
		return;
	}

	/* Now go for the sources. */
	switch (special) {
	case SP_INCLUDES:
	case SP_LIBS:
	case SP_NOREADONLY:
	case SP_NULL:
	case SP_OBJDIR:
	case SP_PATH:
	case SP_READONLY:
	case SP_SUFFIXES:
	case SP_SYSPATH:
		ParseDependencySourcesSpecial(p, special, *inout_paths);
		if (*inout_paths != NULL) {
			Lst_Free(*inout_paths);
			*inout_paths = NULL;
		}
		if (special == SP_PATH)
			Dir_SetPATH();
		if (special == SP_SYSPATH)
			Dir_SetSYSPATH();
		break;
	default:
		assert(*inout_paths == NULL);
		if (!ParseDependencySourcesMundane(p, special, targetAttr))
			return;
		break;
	}

	MaybeUpdateMainTarget();
}

/*
 * Parse a dependency line consisting of targets, followed by a dependency
 * operator, optionally followed by sources.
 *
 * The nodes of the sources are linked as children to the nodes of the
 * targets. Nodes are created as necessary.
 *
 * The operator is applied to each node in the global 'targets' list,
 * which is where the nodes found for the targets are kept.
 *
 * The sources are parsed in much the same way as the targets, except
 * that they are expanded using the wildcarding scheme of the C-Shell,
 * and a target is created for each expanded word. Each of the resulting
 * nodes is then linked to each of the targets as one of its children.
 *
 * Certain targets and sources such as .PHONY or .PRECIOUS are handled
 * specially, see ParseSpecial.
 *
 * Transformation rules such as '.c.o' are also handled here, see
 * Suff_AddTransform.
 *
 * Upon return, the value of the line is unspecified.
 */
static void
ParseDependency(char *line)
{
	char *p;
	SearchPathList *paths;	/* search paths to alter when parsing a list
				 * of .PATH targets */
	GNodeType targetAttr;	/* from special sources */
	ParseSpecial special;	/* in special targets, the children are
				 * linked as children of the parent but not
				 * vice versa */
	GNodeType op;

	DEBUG1(PARSE, "ParseDependency(%s)\n", line);
	p = line;
	paths = NULL;
	targetAttr = OP_NONE;
	special = SP_NOT;

	if (!ParseDependencyTargets(&p, line, &special, &targetAttr, &paths))
		goto out;

	if (!Lst_IsEmpty(targets))
		CheckSpecialMundaneMixture(special);

	op = ParseDependencyOp(&p);
	if (op == OP_NONE) {
		InvalidLineType(line);
		goto out;
	}
	ApplyDependencyOperator(op);

	pp_skip_whitespace(&p);

	ParseDependencySources(p, targetAttr, special, &paths);

out:
	if (paths != NULL)
		Lst_Free(paths);
}

/*
 * Determine the assignment operator and adjust the end of the variable
 * name accordingly.
 */
static VarAssign
AdjustVarassignOp(const char *name, const char *nameEnd, const char *op,
		  const char *value)
{
	VarAssignOp type;
	VarAssign va;

	if (op > name && op[-1] == '+') {
		op--;
		type = VAR_APPEND;

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

		if (op - name >= 3 && memcmp(op - 3, ":sh", 3) == 0) {
			op -= 3;
			type = VAR_SHELL;
		}
#endif
	}

	va.varname = bmake_strsedup(name, nameEnd < op ? nameEnd : op);
	va.op = type;
	va.value = value;
	return va;
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
static bool
Parse_IsVar(const char *p, VarAssign *out_var)
{
	const char *nameStart, *nameEnd, *firstSpace, *eq;
	int level = 0;

	cpp_skip_hspace(&p);	/* Skip to variable name */

	/*
	 * During parsing, the '+' of the operator '+=' is initially parsed
	 * as part of the variable name.  It is later corrected, as is the
	 * ':sh' modifier. Of these two (nameEnd and eq), the earlier one
	 * determines the actual end of the variable name.
	 */

	nameStart = p;
	firstSpace = NULL;

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

		if ((ch == ' ' || ch == '\t') && firstSpace == NULL)
			firstSpace = p - 1;
		while (ch == ' ' || ch == '\t')
			ch = *p++;

		if (ch == '\0')
			return false;
#ifdef SUNSHCMD
		if (ch == ':' && p[0] == 's' && p[1] == 'h') {
			p += 2;
			continue;
		}
#endif
		if (ch == '=')
			eq = p - 1;
		else if (*p == '=' &&
		    (ch == '+' || ch == ':' || ch == '?' || ch == '!'))
			eq = p;
		else if (firstSpace != NULL)
			return false;
		else
			continue;

		nameEnd = firstSpace != NULL ? firstSpace : eq;
		p = eq + 1;
		cpp_skip_whitespace(&p);
		*out_var = AdjustVarassignOp(nameStart, nameEnd, eq, p);
		return true;
	}

	return false;
}

/*
 * Check for syntax errors such as unclosed expressions or unknown modifiers.
 */
static void
VarCheckSyntax(VarAssignOp type, const char *uvalue, GNode *scope)
{
	if (opts.strict) {
		if (type != VAR_SUBST && strchr(uvalue, '$') != NULL) {
			char *expandedValue = Var_Subst(uvalue,
			    scope, VARE_PARSE_ONLY);
			/* TODO: handle errors */
			free(expandedValue);
		}
	}
}

/* Perform a variable assignment that uses the operator ':='. */
static void
VarAssign_EvalSubst(GNode *scope, const char *name, const char *uvalue,
		    FStr *out_avalue)
{
	char *evalue;

	/*
	 * make sure that we set the variable the first time to nothing
	 * so that it gets substituted.
	 *
	 * TODO: Add a test that demonstrates why this code is needed,
	 *  apart from making the debug log longer.
	 *
	 * XXX: The variable name is expanded up to 3 times.
	 */
	if (!Var_ExistsExpand(scope, name))
		Var_SetExpand(scope, name, "");

	evalue = Var_Subst(uvalue, scope, VARE_KEEP_DOLLAR_UNDEF);
	/* TODO: handle errors */

	Var_SetExpand(scope, name, evalue);

	*out_avalue = FStr_InitOwn(evalue);
}

/* Perform a variable assignment that uses the operator '!='. */
static void
VarAssign_EvalShell(const char *name, const char *uvalue, GNode *scope,
		    FStr *out_avalue)
{
	FStr cmd;
	char *output, *error;

	cmd = FStr_InitRefer(uvalue);
	Var_Expand(&cmd, SCOPE_CMDLINE, VARE_UNDEFERR);

	output = Cmd_Exec(cmd.str, &error);
	Var_SetExpand(scope, name, output);
	*out_avalue = FStr_InitOwn(output);
	if (error != NULL) {
		Parse_Error(PARSE_WARNING, "%s", error);
		free(error);
	}

	FStr_Done(&cmd);
}

/*
 * Perform a variable assignment.
 *
 * The actual value of the variable is returned in *out_true_avalue.
 * Especially for VAR_SUBST and VAR_SHELL this can differ from the literal
 * value.
 *
 * Return whether the assignment was actually performed, which is usually
 * the case.  It is only skipped if the operator is '?=' and the variable
 * already exists.
 */
static bool
VarAssign_Eval(const char *name, VarAssignOp op, const char *uvalue,
	       GNode *scope, FStr *out_true_avalue)
{
	FStr avalue = FStr_InitRefer(uvalue);

	if (op == VAR_APPEND)
		Var_AppendExpand(scope, name, uvalue);
	else if (op == VAR_SUBST)
		VarAssign_EvalSubst(scope, name, uvalue, &avalue);
	else if (op == VAR_SHELL)
		VarAssign_EvalShell(name, uvalue, scope, &avalue);
	else {
		/* XXX: The variable name is expanded up to 2 times. */
		if (op == VAR_DEFAULT && Var_ExistsExpand(scope, name))
			return false;

		/* Normal assignment -- just do it. */
		Var_SetExpand(scope, name, uvalue);
	}

	*out_true_avalue = avalue;
	return true;
}

static void
VarAssignSpecial(const char *name, const char *avalue)
{
	if (strcmp(name, ".MAKEOVERRIDES") == 0)
		Main_ExportMAKEFLAGS(false);	/* re-export MAKEFLAGS */
	else if (strcmp(name, ".CURDIR") == 0) {
		/*
		 * Someone is being (too?) clever...
		 * Let's pretend they know what they are doing and
		 * re-initialize the 'cur' CachedDir.
		 */
		Dir_InitCur(avalue);
		Dir_SetPATH();
	} else if (strcmp(name, ".MAKE.JOB.PREFIX") == 0)
		Job_SetPrefix();
	else if (strcmp(name, ".MAKE.EXPORTED") == 0)
		Var_ExportVars(avalue);
}

/* Perform the variable assignment in the given scope. */
static void
Parse_Var(VarAssign *var, GNode *scope)
{
	FStr avalue;		/* actual value (maybe expanded) */

	VarCheckSyntax(var->op, var->value, scope);
	if (VarAssign_Eval(var->varname, var->op, var->value, scope, &avalue)) {
		VarAssignSpecial(var->varname, avalue.str);
		FStr_Done(&avalue);
	}
}


/*
 * See if the command possibly calls a sub-make by using the variable
 * expressions ${.MAKE}, ${MAKE} or the plain word "make".
 */
static bool
MaybeSubMake(const char *cmd)
{
	const char *start;

	for (start = cmd; *start != '\0'; start++) {
		const char *p = start;
		char endc;

		/* XXX: What if progname != "make"? */
		if (strncmp(p, "make", 4) == 0)
			if (start == cmd || !ch_isalnum(p[-1]))
				if (!ch_isalnum(p[4]))
					return true;

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

		if (strncmp(p, "MAKE", 4) == 0 && p[4] == endc)
			return true;
	}
	return false;
}

/*
 * Append the command to the target node.
 *
 * The node may be marked as a submake node if the command is determined to
 * be that.
 */
static void
GNode_AddCommand(GNode *gn, char *cmd)
{
	/* Add to last (ie current) cohort for :: targets */
	if ((gn->type & OP_DOUBLEDEP) && gn->cohorts.last != NULL)
		gn = gn->cohorts.last->datum;

	/* if target already supplied, ignore commands */
	if (!(gn->type & OP_HAS_COMMANDS)) {
		Lst_Append(&gn->commands, cmd);
		if (MaybeSubMake(cmd))
			gn->type |= OP_SUBMAKE;
		RememberLocation(gn);
	} else {
#if 0
		/* XXX: We cannot do this until we fix the tree */
		Lst_Append(&gn->commands, cmd);
		Parse_Error(PARSE_WARNING,
		    "overriding commands for target \"%s\"; "
		    "previous commands defined at %s: %u ignored",
		    gn->name, gn->fname, gn->lineno);
#else
		Parse_Error(PARSE_WARNING,
		    "duplicate script for target \"%s\" ignored",
		    gn->name);
		ParseErrorInternal(gn, PARSE_WARNING,
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
	(void)SearchPath_Add(parseIncPath, dir);
}


/*
 * Parse a directive like '.include' or '.-include'.
 *
 * .include "user-makefile.mk"
 * .include <system-makefile.mk>
 */
static void
ParseInclude(char *directive)
{
	char endc;		/* '>' or '"' */
	char *p;
	bool silent = directive[0] != 'i';
	FStr file;

	p = directive + (silent ? 8 : 7);
	pp_skip_hspace(&p);

	if (*p != '"' && *p != '<') {
		Parse_Error(PARSE_FATAL,
		    ".include filename must be delimited by '\"' or '<'");
		return;
	}

	if (*p++ == '<')
		endc = '>';
	else
		endc = '"';
	file = FStr_InitRefer(p);

	/* Skip to matching delimiter */
	while (*p != '\0' && *p != endc)
		p++;

	if (*p != endc) {
		Parse_Error(PARSE_FATAL,
		    "Unclosed .include filename. '%c' expected", endc);
		return;
	}

	*p = '\0';

	Var_Expand(&file, SCOPE_CMDLINE, VARE_WANTRES);
	IncludeFile(file.str, endc == '>', directive[0] == 'd', silent);
	FStr_Done(&file);
}

/*
 * Split filename into dirname + basename, then assign these to the
 * given variables.
 */
static void
SetFilenameVars(const char *filename, const char *dirvar, const char *filevar)
{
	const char *slash, *basename;
	FStr dirname;

	slash = strrchr(filename, '/');
	if (slash == NULL) {
		dirname = FStr_InitRefer(curdir);
		basename = filename;
	} else {
		dirname = FStr_InitOwn(bmake_strsedup(filename, slash));
		basename = slash + 1;
	}

	Global_Set(dirvar, dirname.str);
	Global_Set(filevar, basename);

	DEBUG4(PARSE, "SetFilenameVars: ${%s} = `%s' ${%s} = `%s'\n",
	    dirvar, dirname.str, filevar, basename);
	FStr_Done(&dirname);
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
	const IncludedFile *incs = GetInclude(0);

	for (i = includes.len; i >= 2; i--)
		if (incs[i - 1].forLoop == NULL)
			return incs[i - 2].name.str;
	return NULL;
}

/* Set .PARSEDIR, .PARSEFILE, .INCLUDEDFROMDIR and .INCLUDEDFROMFILE. */
static void
SetParseFile(const char *filename)
{
	const char *including;

	SetFilenameVars(filename, ".PARSEDIR", ".PARSEFILE");

	including = GetActuallyIncludingFile();
	if (including != NULL) {
		SetFilenameVars(including,
		    ".INCLUDEDFROMDIR", ".INCLUDEDFROMFILE");
	} else {
		Global_Delete(".INCLUDEDFROMDIR");
		Global_Delete(".INCLUDEDFROMFILE");
	}
}

static bool
StrContainsWord(const char *str, const char *word)
{
	size_t strLen = strlen(str);
	size_t wordLen = strlen(word);
	const char *p;

	if (strLen < wordLen)
		return false;

	for (p = str; p != NULL; p = strchr(p, ' ')) {
		if (*p == ' ')
			p++;
		if (p > str + strLen - wordLen)
			return false;

		if (memcmp(p, word, wordLen) == 0 &&
		    (p[wordLen] == '\0' || p[wordLen] == ' '))
			return true;
	}
	return false;
}

/*
 * XXX: Searching through a set of words with this linear search is
 * inefficient for variables that contain thousands of words.
 *
 * XXX: The paths in this list don't seem to be normalized in any way.
 */
static bool
VarContainsWord(const char *varname, const char *word)
{
	FStr val = Var_Value(SCOPE_GLOBAL, varname);
	bool found = val.str != NULL && StrContainsWord(val.str, word);
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
TrackInput(const char *name)
{
	if (!VarContainsWord(".MAKE.MAKEFILES", name))
		Global_Append(".MAKE.MAKEFILES", name);
}


/* Parse from the given buffer, later return to the current file. */
void
Parse_PushInput(const char *name, unsigned lineno, unsigned readLines,
		Buffer buf, struct ForLoop *forLoop)
{
	IncludedFile *curFile;

	if (forLoop != NULL)
		name = CurFile()->name.str;
	else
		TrackInput(name);

	DEBUG3(PARSE, "Parse_PushInput: %s %s, line %u\n",
	    forLoop != NULL ? ".for loop in": "file", name, lineno);

	curFile = Vector_Push(&includes);
	curFile->name = FStr_InitOwn(bmake_strdup(name));
	curFile->lineno = lineno;
	curFile->readLines = readLines;
	curFile->forHeadLineno = lineno;
	curFile->forBodyReadLines = readLines;
	curFile->buf = buf;
	curFile->depending = doing_depend;	/* restore this on EOF */
	curFile->guardState = forLoop == NULL ? GS_START : GS_NO;
	curFile->guard = NULL;
	curFile->forLoop = forLoop;

	if (forLoop != NULL && !For_NextIteration(forLoop, &curFile->buf))
		abort();	/* see For_Run */

	curFile->buf_ptr = curFile->buf.data;
	curFile->buf_end = curFile->buf.data + curFile->buf.len;
	curFile->condMinDepth = cond_depth;
	SetParseFile(name);
}

/* Check if the directive is an include directive. */
static bool
IsInclude(const char *dir, bool sysv)
{
	if (dir[0] == 's' || dir[0] == '-' || (dir[0] == 'd' && !sysv))
		dir++;

	if (strncmp(dir, "include", 7) != 0)
		return false;

	/* Space is not mandatory for BSD .include */
	return !sysv || ch_isspace(dir[7]);
}


#ifdef SYSVINCLUDE
/* Check if the line is a SYSV include directive. */
static bool
IsSysVInclude(const char *line)
{
	const char *p;

	if (!IsInclude(line, true))
		return false;

	/* Avoid interpreting a dependency line as an include */
	for (p = line; (p = strchr(p, ':')) != NULL;) {

		/* end of line -> it's a dependency */
		if (*++p == '\0')
			return false;

		/* '::' operator or ': ' -> it's a dependency */
		if (*p == ':' || ch_isspace(*p))
			return false;
	}
	return true;
}

/* Push to another file.  The line points to the word "include". */
static void
ParseTraditionalInclude(char *line)
{
	char *cp;		/* current position in file spec */
	bool done = false;
	bool silent = line[0] != 'i';
	char *file = line + (silent ? 8 : 7);
	char *all_files;

	DEBUG1(PARSE, "ParseTraditionalInclude: %s\n", file);

	pp_skip_whitespace(&file);

	all_files = Var_Subst(file, SCOPE_CMDLINE, VARE_WANTRES);
	/* TODO: handle errors */

	for (file = all_files; !done; file = cp + 1) {
		/* Skip to end of line or next whitespace */
		for (cp = file; *cp != '\0' && !ch_isspace(*cp); cp++)
			continue;

		if (*cp != '\0')
			*cp = '\0';
		else
			done = true;

		IncludeFile(file, false, false, silent);
	}

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

	DEBUG1(PARSE, "ParseGmakeExport: %s\n", variable);

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
	value = Var_Subst(value, SCOPE_CMDLINE, VARE_WANTRES);
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
 *	true to continue parsing, i.e. it had only reached the end of an
 *	included file, false if the main file has been parsed completely.
 */
static bool
ParseEOF(void)
{
	IncludedFile *curFile = CurFile();

	doing_depend = curFile->depending;
	if (curFile->forLoop != NULL &&
	    For_NextIteration(curFile->forLoop, &curFile->buf)) {
		curFile->buf_ptr = curFile->buf.data;
		curFile->buf_end = curFile->buf.data + curFile->buf.len;
		curFile->readLines = curFile->forBodyReadLines;
		return true;
	}

	Cond_EndFile();

	if (curFile->guardState == GS_DONE)
		HashTable_Set(&guards, curFile->name.str, curFile->guard);
	else if (curFile->guard != NULL) {
		free(curFile->guard->name);
		free(curFile->guard);
	}

	FStr_Done(&curFile->name);
	Buf_Done(&curFile->buf);
	if (curFile->forLoop != NULL)
		ForLoop_Free(curFile->forLoop);
	Vector_Pop(&includes);

	if (includes.len == 0) {
		/* We've run out of input */
		Global_Delete(".PARSEDIR");
		Global_Delete(".PARSEFILE");
		Global_Delete(".INCLUDEDFROMDIR");
		Global_Delete(".INCLUDEDFROMFILE");
		return false;
	}

	curFile = CurFile();
	DEBUG2(PARSE, "ParseEOF: returning to file %s, line %u\n",
	    curFile->name.str, curFile->readLines + 1);

	SetParseFile(curFile->name.str);
	return true;
}

typedef enum ParseRawLineResult {
	PRLR_LINE,
	PRLR_EOF,
	PRLR_ERROR
} ParseRawLineResult;

/*
 * Parse until the end of a line, taking into account lines that end with
 * backslash-newline.  The resulting line goes from out_line to out_line_end;
 * the line is not null-terminated.
 */
static ParseRawLineResult
ParseRawLine(IncludedFile *curFile, char **out_line, char **out_line_end,
	     char **out_firstBackslash, char **out_commentLineEnd)
{
	char *line = curFile->buf_ptr;
	char *buf_end = curFile->buf_end;
	char *p = line;
	char *line_end = line;
	char *firstBackslash = NULL;
	char *commentLineEnd = NULL;
	ParseRawLineResult res = PRLR_LINE;

	curFile->readLines++;

	for (;;) {
		char ch;

		if (p == buf_end) {
			res = PRLR_EOF;
			break;
		}

		ch = *p;
		if (ch == '\0' || (ch == '\\' && p[1] == '\0')) {
			Parse_Error(PARSE_FATAL, "Zero byte read from file");
			return PRLR_ERROR;
		}

		/* Treat next character after '\' as literal. */
		if (ch == '\\') {
			if (firstBackslash == NULL)
				firstBackslash = p;
			if (p[1] == '\n') {
				curFile->readLines++;
				if (p + 2 == buf_end) {
					line_end = p;
					*line_end = '\n';
					p += 2;
					continue;
				}
			}
			p += 2;
			line_end = p;
			assert(p <= buf_end);
			continue;
		}

		/*
		 * Remember the first '#' for comment stripping, unless
		 * the previous char was '[', as in the modifier ':[#]'.
		 */
		if (ch == '#' && commentLineEnd == NULL &&
		    !(p > line && p[-1] == '['))
			commentLineEnd = line_end;

		p++;
		if (ch == '\n')
			break;

		/* We are not interested in trailing whitespace. */
		if (!ch_isspace(ch))
			line_end = p;
	}

	curFile->buf_ptr = p;
	*out_line = line;
	*out_line_end = line_end;
	*out_firstBackslash = firstBackslash;
	*out_commentLineEnd = commentLineEnd;
	return res;
}

/*
 * Beginning at start, unescape '\#' to '#' and replace backslash-newline
 * with a single space.
 */
static void
UnescapeBackslash(char *line, char *start)
{
	const char *src = start;
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
			/* Delete '\\' at the end of the buffer. */
			dst--;
			break;
		}

		/* Delete '\\' from before '#' on non-command lines. */
		if (ch == '#' && line[0] != '\t')
			*dst++ = ch;
		else if (ch == '\n') {
			cpp_skip_hspace(&src);
			*dst++ = ' ';
		} else {
			/* Leave '\\' in the buffer for later. */
			*dst++ = '\\';
			*dst++ = ch;
			/* Keep an escaped ' ' at the line end. */
			spaceStart = dst;
		}
	}

	/* Delete any trailing spaces - eg from empty continuations */
	while (dst > spaceStart && ch_isspace(dst[-1]))
		dst--;
	*dst = '\0';
}

typedef enum LineKind {
	/*
	 * Return the next line that is neither empty nor a comment.
	 * Backslash line continuations are folded into a single space.
	 * A trailing comment, if any, is discarded.
	 */
	LK_NONEMPTY,

	/*
	 * Return the next line, even if it is empty or a comment.
	 * Preserve backslash-newline to keep the line numbers correct.
	 *
	 * Used in .for loops to collect the body of the loop while waiting
	 * for the corresponding .endfor.
	 */
	LK_FOR_BODY,

	/*
	 * Return the next line that starts with a dot.
	 * Backslash line continuations are folded into a single space.
	 * A trailing comment, if any, is discarded.
	 *
	 * Used in .if directives to skip over irrelevant branches while
	 * waiting for the corresponding .endif.
	 */
	LK_DOT
} LineKind;

/*
 * Return the next "interesting" logical line from the current file.  The
 * returned string will be freed at the end of including the file.
 */
static char *
ReadLowLevelLine(LineKind kind)
{
	IncludedFile *curFile = CurFile();
	ParseRawLineResult res;
	char *line;
	char *line_end;
	char *firstBackslash;
	char *commentLineEnd;

	for (;;) {
		curFile->lineno = curFile->readLines + 1;
		res = ParseRawLine(curFile,
		    &line, &line_end, &firstBackslash, &commentLineEnd);
		if (res == PRLR_ERROR)
			return NULL;

		if (line == line_end || line == commentLineEnd) {
			if (res == PRLR_EOF)
				return NULL;
			if (kind != LK_FOR_BODY)
				continue;
		}

		/* We now have a line of data */
		assert(ch_isspace(*line_end));
		*line_end = '\0';

		if (kind == LK_FOR_BODY)
			return line;	/* Don't join the physical lines. */

		if (kind == LK_DOT && line[0] != '.')
			continue;
		break;
	}

	if (commentLineEnd != NULL && line[0] != '\t')
		*commentLineEnd = '\0';
	if (firstBackslash != NULL)
		UnescapeBackslash(line, firstBackslash);
	return line;
}

static bool
SkipIrrelevantBranches(void)
{
	const char *line;

	while ((line = ReadLowLevelLine(LK_DOT)) != NULL) {
		if (Cond_EvalLine(line) == CR_TRUE)
			return true;
		/*
		 * TODO: Check for typos in .elif directives such as .elsif
		 * or .elseif.
		 *
		 * This check will probably duplicate some of the code in
		 * ParseLine.  Most of the code there cannot apply, only
		 * ParseVarassign and ParseDependencyLine can, and to prevent
		 * code duplication, these would need to be called with a
		 * flag called onlyCheckSyntax.
		 *
		 * See directive-elif.mk for details.
		 */
	}

	return false;
}

static bool
ParseForLoop(const char *line)
{
	int rval;
	unsigned forHeadLineno;
	unsigned bodyReadLines;
	int forLevel;

	rval = For_Eval(line);
	if (rval == 0)
		return false;	/* Not a .for line */
	if (rval < 0)
		return true;	/* Syntax error - error printed, ignore line */

	forHeadLineno = CurFile()->lineno;
	bodyReadLines = CurFile()->readLines;

	/* Accumulate the loop body until the matching '.endfor'. */
	forLevel = 1;
	do {
		line = ReadLowLevelLine(LK_FOR_BODY);
		if (line == NULL) {
			Parse_Error(PARSE_FATAL,
			    "Unexpected end of file in .for loop");
			break;
		}
	} while (For_Accum(line, &forLevel));

	For_Run(forHeadLineno, bodyReadLines);
	return true;
}

/*
 * Read an entire line from the input file.
 *
 * Empty lines, .if and .for are completely handled by this function,
 * leaving only variable assignments, other directives, dependency lines
 * and shell commands to the caller.
 *
 * Return a line without trailing whitespace, or NULL for EOF.  The returned
 * string will be freed at the end of including the file.
 */
static char *
ReadHighLevelLine(void)
{
	char *line;
	CondResult condResult;

	for (;;) {
		IncludedFile *curFile = CurFile();
		line = ReadLowLevelLine(LK_NONEMPTY);
		if (posix_state == PS_MAYBE_NEXT_LINE)
			posix_state = PS_NOW_OR_NEVER;
		else
			posix_state = PS_TOO_LATE;
		if (line == NULL)
			return NULL;

		if (curFile->guardState != GS_NO
		    && ((curFile->guardState == GS_START && line[0] != '.')
			|| curFile->guardState == GS_DONE))
			curFile->guardState = GS_NO;
		if (line[0] != '.')
			return line;

		condResult = Cond_EvalLine(line);
		if (curFile->guardState == GS_START) {
			Guard *guard;
			if (condResult != CR_ERROR
			    && (guard = Cond_ExtractGuard(line)) != NULL) {
				curFile->guardState = GS_COND;
				curFile->guard = guard;
			} else
				curFile->guardState = GS_NO;
		}
		switch (condResult) {
		case CR_FALSE:	/* May also mean a syntax error. */
			if (!SkipIrrelevantBranches())
				return NULL;
			continue;
		case CR_TRUE:
			continue;
		case CR_ERROR:	/* Not a conditional line */
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
			GNode_AddCommand(gn, cmd);
		}
#ifdef CLEANUP
		Lst_Append(&targCmds, cmd);
#endif
	}
}

static void
HandleBreak(const char *arg)
{
	IncludedFile *curFile = CurFile();

	if (arg[0] != '\0')
		Parse_Error(PARSE_FATAL,
		    "The .break directive does not take arguments");

	if (curFile->forLoop != NULL) {
		/* pretend we reached EOF */
		For_Break(curFile->forLoop);
		cond_depth = CurFile_CondMinDepth();
		ParseEOF();
	} else
		Parse_Error(PARSE_FATAL, "break outside of for loop");
}

/*
 * See if the line starts with one of the known directives, and if so, handle
 * the directive.
 */
static bool
ParseDirective(char *line)
{
	char *cp = line + 1;
	const char *arg;
	Substring dir;

	pp_skip_whitespace(&cp);
	if (IsInclude(cp, false)) {
		ParseInclude(cp);
		return true;
	}

	dir.start = cp;
	while (ch_islower(*cp) || *cp == '-')
		cp++;
	dir.end = cp;

	if (*cp != '\0' && !ch_isspace(*cp))
		return false;

	pp_skip_whitespace(&cp);
	arg = cp;

	if (Substring_Equals(dir, "break"))
		HandleBreak(arg);
	else if (Substring_Equals(dir, "undef"))
		Var_Undef(arg);
	else if (Substring_Equals(dir, "export"))
		Var_Export(VEM_PLAIN, arg);
	else if (Substring_Equals(dir, "export-env"))
		Var_Export(VEM_ENV, arg);
	else if (Substring_Equals(dir, "export-literal"))
		Var_Export(VEM_LITERAL, arg);
	else if (Substring_Equals(dir, "unexport"))
		Var_UnExport(false, arg);
	else if (Substring_Equals(dir, "unexport-env"))
		Var_UnExport(true, arg);
	else if (Substring_Equals(dir, "info"))
		HandleMessage(PARSE_INFO, "info", arg);
	else if (Substring_Equals(dir, "warning"))
		HandleMessage(PARSE_WARNING, "warning", arg);
	else if (Substring_Equals(dir, "error"))
		HandleMessage(PARSE_FATAL, "error", arg);
	else
		return false;
	return true;
}

bool
Parse_VarAssign(const char *line, bool finishDependencyGroup, GNode *scope)
{
	VarAssign var;

	if (!Parse_IsVar(line, &var))
		return false;
	if (finishDependencyGroup)
		FinishDependencyGroup();
	Parse_Var(&var, scope);
	free(var.varname);
	return true;
}

void
Parse_GuardElse(void)
{
	IncludedFile *curFile = CurFile();
	if (cond_depth == curFile->condMinDepth + 1)
		curFile->guardState = GS_NO;
}

void
Parse_GuardEndif(void)
{
	IncludedFile *curFile = CurFile();
	if (cond_depth == curFile->condMinDepth
	    && curFile->guardState == GS_COND)
		curFile->guardState = GS_DONE;
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
 * dependency	-> [target...] op [source...] [';' command]
 * op		-> ':' | '::' | '!'
 */
static void
ParseDependencyLine(char *line)
{
	VarEvalMode emode;
	char *expanded_line;
	const char *shellcmd = NULL;

	/*
	 * For some reason - probably to make the parser impossible -
	 * a ';' can be used to separate commands from dependencies.
	 * Attempt to skip over ';' inside substitution patterns.
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

	/*
	 * In lint mode, allow undefined variables to appear in dependency
	 * lines.
	 *
	 * Ideally, only the right-hand side would allow undefined variables
	 * since it is common to have optional dependencies. Having undefined
	 * variables on the left-hand side is more unusual though.  Since
	 * both sides are expanded in a single pass, there is not much choice
	 * what to do here.
	 *
	 * In normal mode, it does not matter whether undefined variables are
	 * allowed or not since as of 2020-09-14, Var_Parse does not print
	 * any parse errors in such a case. It simply returns the special
	 * empty string var_Error, which cannot be detected in the result of
	 * Var_Subst.
	 */
	emode = opts.strict ? VARE_WANTRES : VARE_UNDEFERR;
	expanded_line = Var_Subst(line, SCOPE_CMDLINE, emode);
	/* TODO: handle errors */

	/* Need a fresh list for the target nodes */
	if (targets != NULL)
		Lst_Free(targets);
	targets = Lst_New();

	ParseDependency(expanded_line);
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

	if (Parse_VarAssign(line, true, SCOPE_GLOBAL))
		return;

	FinishDependencyGroup();

	ParseDependencyLine(line);
}

/*
 * Parse a top-level makefile, incorporating its content into the global
 * dependency graph.
 */
void
Parse_File(const char *name, int fd)
{
	char *line;
	Buffer buf;

	buf = LoadFile(name, fd != -1 ? fd : STDIN_FILENO);
	if (fd != -1)
		(void)close(fd);

	assert(targets == NULL);

	Parse_PushInput(name, 1, 0, buf, NULL);

	do {
		while ((line = ReadHighLevelLine()) != NULL) {
			DEBUG2(PARSE, "Parsing line %u: %s\n",
			    CurFile()->lineno, line);
			ParseLine(line);
		}
		/* Reached EOF, but it may be just EOF of an include file. */
	} while (ParseEOF());

	FinishDependencyGroup();

	if (parseErrors != 0) {
		(void)fflush(stdout);
		(void)fprintf(stderr,
		    "%s: Fatal errors encountered -- cannot continue\n",
		    progname);
		PrintOnError(NULL, "");
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
	Vector_Init(&includes, sizeof(IncludedFile));
	HashTable_Init(&guards);
}

/* Clean up the parsing module. */
void
Parse_End(void)
{
#ifdef CLEANUP
	HashIter hi;

	Lst_DoneCall(&targCmds, free);
	assert(targets == NULL);
	SearchPath_Free(defSysIncPath);
	SearchPath_Free(sysIncPath);
	SearchPath_Free(parseIncPath);
	assert(includes.len == 0);
	Vector_Done(&includes);
	HashIter_Init(&hi, &guards);
	while (HashIter_Next(&hi) != NULL) {
		Guard *guard = hi.entry->value;
		free(guard->name);
		free(guard);
	}
	HashTable_Done(&guards);
#endif
}


/* Populate the list with the single main target to create, or error out. */
void
Parse_MainName(GNodeList *mainList)
{
	if (mainNode == NULL)
		Punt("no target to make.");

	Lst_Append(mainList, mainNode);
	if (mainNode->type & OP_DOUBLEDEP)
		Lst_AppendAll(mainList, &mainNode->cohorts);

	Global_Append(".TARGETS", mainNode->name);
}

int
Parse_NumErrors(void)
{
	return parseErrors;
}
