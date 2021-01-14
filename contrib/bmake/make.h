/*	$NetBSD: make.h,v 1.242 2021/01/10 21:20:46 rillig Exp $	*/

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
 *
 *	from: @(#)make.h	8.3 (Berkeley) 6/13/95
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
 *
 *	from: @(#)make.h	8.3 (Berkeley) 6/13/95
 */

/*-
 * make.h --
 *	The global definitions for pmake
 */

#ifndef MAKE_MAKE_H
#define MAKE_MAKE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <unistd.h>
#include <sys/cdefs.h>

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#if defined(__GNUC__)
#define MAKE_GNUC_PREREQ(x, y)						\
	((__GNUC__ == (x) && __GNUC_MINOR__ >= (y)) ||			\
	 (__GNUC__ > (x)))
#else /* defined(__GNUC__) */
#define MAKE_GNUC_PREREQ(x, y)	0
#endif /* defined(__GNUC__) */

#if MAKE_GNUC_PREREQ(2, 7)
#define MAKE_ATTR_UNUSED	__attribute__((__unused__))
#else
#define MAKE_ATTR_UNUSED	/* delete */
#endif

#if MAKE_GNUC_PREREQ(2, 5)
#define MAKE_ATTR_DEAD		__attribute__((__noreturn__))
#elif defined(__GNUC__)
#define MAKE_ATTR_DEAD		__volatile
#else
#define MAKE_ATTR_DEAD		/* delete */
#endif

#if MAKE_GNUC_PREREQ(2, 7)
#define MAKE_ATTR_PRINTFLIKE(fmtarg, firstvararg)	\
	    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define MAKE_ATTR_PRINTFLIKE(fmtarg, firstvararg)	/* delete */
#endif

#define MAKE_INLINE static inline MAKE_ATTR_UNUSED

/*
 * A boolean type is defined as an integer, not an enum, for historic reasons.
 * The only allowed values are the constants TRUE and FALSE (1 and 0).
 */
#if defined(lint) || defined(USE_C99_BOOLEAN)
#include <stdbool.h>
typedef bool Boolean;
#define FALSE false
#define TRUE true
#elif defined(USE_DOUBLE_BOOLEAN)
/* During development, to find type mismatches in function declarations. */
typedef double Boolean;
#define TRUE 1.0
#define FALSE 0.0
#elif defined(USE_UCHAR_BOOLEAN)
/*
 * During development, to find code that depends on the exact value of TRUE or
 * that stores other values in Boolean variables.
 */
typedef unsigned char Boolean;
#define TRUE ((unsigned char)0xFF)
#define FALSE ((unsigned char)0x00)
#elif defined(USE_CHAR_BOOLEAN)
/*
 * During development, to find code that uses a boolean as array index, via
 * -Wchar-subscripts.
 */
typedef char Boolean;
#define TRUE ((char)-1)
#define FALSE ((char)0x00)
#elif defined(USE_ENUM_BOOLEAN)
typedef enum Boolean { FALSE, TRUE } Boolean;
#else
typedef int Boolean;
#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif
#endif

#include "lst.h"
#include "enum.h"
#include "hash.h"
#include "make-conf.h"
#include "buf.h"
#include "make_malloc.h"

/*
 * some vendors don't have this --sjg
 */
#if defined(S_IFDIR) && !defined(S_ISDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# define POSIX_SIGNALS
#endif

/*
 * The typical flow of states is:
 *
 * The direct successful path:
 * UNMADE -> BEINGMADE -> MADE.
 *
 * The direct error path:
 * UNMADE -> BEINGMADE -> ERROR.
 *
 * The successful path when dependencies need to be made first:
 * UNMADE -> DEFERRED -> REQUESTED -> BEINGMADE -> MADE.
 *
 * A node that has dependencies, and one of the dependencies cannot be made:
 * UNMADE -> DEFERRED -> ABORTED.
 *
 * A node that turns out to be up-to-date:
 * UNMADE -> BEINGMADE -> UPTODATE.
 */
typedef enum GNodeMade {
	/* Not examined yet. */
	UNMADE,
	/* The node has been examined but is not yet ready since its
	 * dependencies have to be made first. */
	DEFERRED,

	/* The node is on the toBeMade list. */
	REQUESTED,

	/* The node is already being made. Trying to build a node in this
	 * state indicates a cycle in the graph. */
	BEINGMADE,

	/* Was out-of-date and has been made. */
	MADE,
	/* Was already up-to-date, does not need to be made. */
	UPTODATE,
	/* An error occurred while it was being made.
	 * Used only in compat mode. */
	ERROR,
	/* The target was aborted due to an error making a dependency.
	 * Used only in compat mode. */
	ABORTED
} GNodeMade;

/*
 * The OP_ constants are used when parsing a dependency line as a way of
 * communicating to other parts of the program the way in which a target
 * should be made.
 *
 * Some of the OP_ constants can be combined, others cannot.
 */
typedef enum GNodeType {
	OP_NONE		= 0,

	/* The dependency operator ':' is the most common one.  The commands
	 * of this node are executed if any child is out-of-date. */
	OP_DEPENDS	= 1 << 0,
	/* The dependency operator '!' always executes its commands, even if
	 * its children are up-to-date. */
	OP_FORCE	= 1 << 1,
	/* The dependency operator '::' behaves like ':', except that it
	 * allows multiple dependency groups to be defined.  Each of these
	 * groups is executed on its own, independently from the others.
	 * Each individual dependency group is called a cohort. */
	OP_DOUBLEDEP	= 1 << 2,

	/* Matches the dependency operators ':', '!' and '::'. */
	OP_OPMASK	= OP_DEPENDS | OP_FORCE | OP_DOUBLEDEP,

	/* Don't care if the target doesn't exist and can't be created. */
	OP_OPTIONAL	= 1 << 3,
	/* Use associated commands for parents. */
	OP_USE		= 1 << 4,
	/* Target is never out of date, but always execute commands anyway.
	 * Its time doesn't matter, so it has none...sort of. */
	OP_EXEC		= 1 << 5,
	/* Ignore non-zero exit status from shell commands when creating the
	 * node. */
	OP_IGNORE	= 1 << 6,
	/* Don't remove the target when interrupted. */
	OP_PRECIOUS	= 1 << 7,
	/* Don't echo commands when executed. */
	OP_SILENT	= 1 << 8,
	/* Target is a recursive make so its commands should always be
	 * executed when it is out of date, regardless of the state of the
	 * -n or -t flags. */
	OP_MAKE		= 1 << 9,
	/* Target is out-of-date only if any of its children was out-of-date. */
	OP_JOIN		= 1 << 10,
	/* Assume the children of the node have been already made. */
	OP_MADE		= 1 << 11,
	/* Special .BEGIN, .END or .INTERRUPT. */
	OP_SPECIAL	= 1 << 12,
	/* Like .USE, only prepend commands. */
	OP_USEBEFORE	= 1 << 13,
	/* The node is invisible to its parents. I.e. it doesn't show up in
	 * the parents' local variables (.IMPSRC, .ALLSRC). */
	OP_INVISIBLE	= 1 << 14,
	/* The node does not become the main target, even if it is the first
	 * target in the first makefile. */
	OP_NOTMAIN	= 1 << 15,
	/* Not a file target; run always. */
	OP_PHONY	= 1 << 16,
	/* Don't search for the file in the path. */
	OP_NOPATH	= 1 << 17,
	/* In a dependency line "target: source1 .WAIT source2", source1 is
	 * made first, including its children.  Once that is finished,
	 * source2 is made, including its children.  The .WAIT keyword may
	 * appear more than once in a single dependency declaration. */
	OP_WAIT		= 1 << 18,
	/* .NOMETA do not create a .meta file */
	OP_NOMETA	= 1 << 19,
	/* .META we _do_ want a .meta file */
	OP_META		= 1 << 20,
	/* Do not compare commands in .meta file */
	OP_NOMETA_CMP	= 1 << 21,
	/* Possibly a submake node */
	OP_SUBMAKE	= 1 << 22,

	/* Attributes applied by PMake */

	/* The node is a transformation rule, such as ".c.o". */
	OP_TRANSFORM	= 1 << 30,
	/* Target is a member of an archive */
	/* XXX: How does this differ from OP_ARCHV? */
	OP_MEMBER	= 1 << 29,
	/* The node is a library,
	 * its name has the form "-l<libname>" */
	OP_LIB		= 1 << 28,
	/* The node is an archive member,
	 * its name has the form "archive(member)" */
	/* XXX: How does this differ from OP_MEMBER? */
	OP_ARCHV	= 1 << 27,
	/* Target has all the commands it should. Used when parsing to catch
	 * multiple command groups for a target.  Only applies to the
	 * dependency operators ':' and '!', but not to '::'. */
	OP_HAS_COMMANDS	= 1 << 26,
	/* The special command "..." has been seen. All further commands from
	 * this node will be saved on the .END node instead, to be executed at
	 * the very end. */
	OP_SAVE_CMDS	= 1 << 25,
	/* Already processed by Suff_FindDeps, to find dependencies from
	 * suffix transformation rules. */
	OP_DEPS_FOUND	= 1 << 24,
	/* Node found while expanding .ALLSRC */
	OP_MARK		= 1 << 23,

	OP_NOTARGET	= OP_NOTMAIN | OP_USE | OP_EXEC | OP_TRANSFORM
} GNodeType;

typedef enum GNodeFlags {
	GNF_NONE	= 0,
	/* this target needs to be (re)made */
	REMAKE		= 0x0001,
	/* children of this target were made */
	CHILDMADE	= 0x0002,
	/* children don't exist, and we pretend made */
	FORCE		= 0x0004,
	/* Set by Make_ProcessWait() */
	DONE_WAIT	= 0x0008,
	/* Build requested by .ORDER processing */
	DONE_ORDER	= 0x0010,
	/* Node created from .depend */
	FROM_DEPEND	= 0x0020,
	/* We do it once only */
	DONE_ALLSRC	= 0x0040,
	/* Used by MakePrintStatus */
	CYCLE		= 0x1000,
	/* Used by MakePrintStatus */
	DONECYCLE	= 0x2000,
	/* Internal use only */
	INTERNAL	= 0x4000
} GNodeFlags;

typedef struct List StringList;
typedef struct ListNode StringListNode;

typedef struct List GNodeList;
typedef struct ListNode GNodeListNode;

typedef struct List /* of CachedDir */ SearchPath;

/*
 * A graph node represents a target that can possibly be made, including its
 * relation to other targets and a lot of other details.
 */
typedef struct GNode {
	/* The target's name, such as "clean" or "make.c" */
	char *name;
	/* The unexpanded name of a .USE node */
	char *uname;
	/* The full pathname of the file belonging to the target.
	 * XXX: What about .PHONY targets? These don't have an associated
	 * path. */
	char *path;

	/* The type of operator used to define the sources (see the OP flags
	 * below).
	 * XXX: This looks like a wild mixture of type and flags. */
	GNodeType type;
	GNodeFlags flags;

	/* The state of processing on this node */
	GNodeMade made;
	/* The number of unmade children */
	int unmade;

	/* The modification time; 0 means the node does not have a
	 * corresponding file; see GNode_IsOODate. */
	time_t mtime;
	struct GNode *youngestChild;

	/* The GNodes for which this node is an implied source. May be empty.
	 * For example, when there is an inference rule for .c.o, the node for
	 * file.c has the node for file.o in this list. */
	GNodeList implicitParents;

	/* The nodes that depend on this one, or in other words, the nodes for
	 * which this is a source. */
	GNodeList parents;
	/* The nodes on which this one depends. */
	GNodeList children;

	/* .ORDER nodes we need made. The nodes that must be made (if they're
	 * made) before this node can be made, but that do not enter into the
	 * datedness of this node. */
	GNodeList order_pred;
	/* .ORDER nodes who need us. The nodes that must be made (if they're
	 * made at all) after this node is made, but that do not depend on
	 * this node, in the normal sense. */
	GNodeList order_succ;

	/* Other nodes of the same name, for the '::' dependency operator. */
	GNodeList cohorts;
	/* The "#n" suffix for this cohort, or "" for other nodes */
	char cohort_num[8];
	/* The number of unmade instances on the cohorts list */
	int unmade_cohorts;
	/* Pointer to the first instance of a '::' node; only set when on a
	 * cohorts list */
	struct GNode *centurion;

	/* Last time (sequence number) we tried to make this node */
	unsigned int checked_seqno;

	/* The "local" variables that are specific to this target and this
	 * target only, such as $@, $<, $?.
	 *
	 * Also used for the global variable scopes VAR_GLOBAL, VAR_CMDLINE,
	 * VAR_INTERNAL, which contain variables with arbitrary names. */
	HashTable /* of Var pointer */ vars;

	/* The commands to be given to a shell to create this target. */
	StringList commands;

	/* Suffix for the node (determined by Suff_FindDeps and opaque to
	 * everyone but the Suff module) */
	struct Suffix *suffix;

	/* Filename where the GNode got defined */
	/* XXX: What is the lifetime of this string? */
	const char *fname;
	/* Line number where the GNode got defined */
	int lineno;
} GNode;

/* Error levels for diagnostics during parsing. */
typedef enum ParseErrorLevel {
	/* Exit when the current top-level makefile has been parsed
	 * completely. */
	PARSE_FATAL = 1,
	/* Print "warning"; may be upgraded to fatal by the -w option. */
	PARSE_WARNING,
	/* Informational, mainly used during development of makefiles. */
	PARSE_INFO
} ParseErrorLevel;

/*
 * Values returned by Cond_EvalLine and Cond_EvalCondition.
 */
typedef enum CondEvalResult {
	COND_PARSE,		/* Parse the next lines */
	COND_SKIP,		/* Skip the next lines */
	COND_INVALID		/* Not a conditional statement */
} CondEvalResult;

/* Names of the variables that are "local" to a specific target. */
#define TARGET	"@"	/* Target of dependency */
#define OODATE	"?"	/* All out-of-date sources */
#define ALLSRC	">"	/* All sources */
#define IMPSRC	"<"	/* Source implied by transformation */
#define PREFIX	"*"	/* Common prefix */
#define ARCHIVE	"!"	/* Archive in "archive(member)" syntax */
#define MEMBER	"%"	/* Member in "archive(member)" syntax */

/*
 * Global Variables
 */

/* True if every target is precious */
extern Boolean allPrecious;
/* True if failed targets should be deleted */
extern Boolean deleteOnError;
/* TRUE while processing .depend */
extern Boolean doing_depend;
/* .DEFAULT rule */
extern GNode *defaultNode;

/*
 * Variables defined internally by make which should not override those set
 * by makefiles.
 */
extern GNode *VAR_INTERNAL;
/* Variables defined in a global context, e.g in the Makefile itself. */
extern GNode *VAR_GLOBAL;
/* Variables defined on the command line. */
extern GNode *VAR_CMDLINE;

/*
 * Value returned by Var_Parse when an error is encountered. It actually
 * points to an empty string, so naive callers needn't worry about it.
 */
extern char var_Error[];

/* The time at the start of this whole process */
extern time_t now;

/*
 * The list of directories to search when looking for targets (set by the
 * special target .PATH).
 */
extern SearchPath dirSearchPath;
/* Used for .include "...". */
extern SearchPath *parseIncPath;
/*
 * Used for .include <...>, for the built-in sys.mk and makefiles from the
 * command line arguments.
 */
extern SearchPath *sysIncPath;
/* The default for sysIncPath. */
extern SearchPath *defSysIncPath;

/* Startup directory */
extern char curdir[];
/* The basename of the program name, suffixed with [n] for sub-makes.  */
extern const char *progname;
/* Name of the .depend makefile */
extern char *makeDependfile;
/* If we replaced environ, this will be non-NULL. */
extern char **savedEnv;

extern int makelevel;

/*
 * We cannot vfork() in a child of vfork().
 * Most systems do not enforce this but some do.
 */
#define vFork() ((getpid() == myPid) ? vfork() : fork())
extern pid_t myPid;

#define MAKEFLAGS	".MAKEFLAGS"
#define MAKEOVERRIDES	".MAKEOVERRIDES"
/* prefix when printing the target of a job */
#define MAKE_JOB_PREFIX	".MAKE.JOB.PREFIX"
#define MAKE_EXPORTED	".MAKE.EXPORTED"	/* exported variables */
#define MAKE_MAKEFILES	".MAKE.MAKEFILES"	/* all loaded makefiles */
#define MAKE_LEVEL	".MAKE.LEVEL"		/* recursion level */
#define MAKE_MAKEFILE_PREFERENCE ".MAKE.MAKEFILE_PREFERENCE"
#define MAKE_DEPENDFILE	".MAKE.DEPENDFILE"	/* .depend */
#define MAKE_MODE	".MAKE.MODE"
#ifndef MAKE_LEVEL_ENV
# define MAKE_LEVEL_ENV	"MAKELEVEL"
#endif

typedef enum DebugFlags {
	DEBUG_NONE	= 0,
	DEBUG_ARCH	= 1 << 0,
	DEBUG_COND	= 1 << 1,
	DEBUG_CWD	= 1 << 2,
	DEBUG_DIR	= 1 << 3,
	DEBUG_ERROR	= 1 << 4,
	DEBUG_FOR	= 1 << 5,
	DEBUG_GRAPH1	= 1 << 6,
	DEBUG_GRAPH2	= 1 << 7,
	DEBUG_GRAPH3	= 1 << 8,
	DEBUG_HASH	= 1 << 9,
	DEBUG_JOB	= 1 << 10,
	DEBUG_LOUD	= 1 << 11,
	DEBUG_MAKE	= 1 << 12,
	DEBUG_META	= 1 << 13,
	DEBUG_PARSE	= 1 << 14,
	DEBUG_SCRIPT	= 1 << 15,
	DEBUG_SHELL	= 1 << 16,
	DEBUG_SUFF	= 1 << 17,
	DEBUG_TARG	= 1 << 18,
	DEBUG_VAR	= 1 << 19,
	DEBUG_ALL	= (1 << 20) - 1
} DebugFlags;

#define CONCAT(a, b) a##b

#define DEBUG(module) ((opts.debug & CONCAT(DEBUG_, module)) != 0)

void debug_printf(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2);

#define DEBUG_IMPL(module, args) \
	do { \
		if (DEBUG(module)) \
			debug_printf args; \
	} while (/*CONSTCOND*/ 0)

#define DEBUG0(module, text) \
	DEBUG_IMPL(module, ("%s", text))
#define DEBUG1(module, fmt, arg1) \
	DEBUG_IMPL(module, (fmt, arg1))
#define DEBUG2(module, fmt, arg1, arg2) \
	DEBUG_IMPL(module, (fmt, arg1, arg2))
#define DEBUG3(module, fmt, arg1, arg2, arg3) \
	DEBUG_IMPL(module, (fmt, arg1, arg2, arg3))
#define DEBUG4(module, fmt, arg1, arg2, arg3, arg4) \
	DEBUG_IMPL(module, (fmt, arg1, arg2, arg3, arg4))
#define DEBUG5(module, fmt, arg1, arg2, arg3, arg4, arg5) \
	DEBUG_IMPL(module, (fmt, arg1, arg2, arg3, arg4, arg5))

typedef enum PrintVarsMode {
	PVM_NONE,
	PVM_UNEXPANDED,
	PVM_EXPANDED
} PrintVarsMode;

/* Command line options */
typedef struct CmdOpts {
	/* -B: whether we are make compatible */
	Boolean compatMake;

	/* -d: debug control: There is one bit per module.  It is up to the
	 * module what debug information to print. */
	DebugFlags debug;

	/* -df: debug output is written here - default stderr */
	FILE *debug_file;

	/* -dL: lint mode
	 *
	 * Runs make in strict mode, with additional checks and better error
	 * handling. */
	Boolean strict;

	/* -dV: for the -V option, print unexpanded variable values */
	Boolean debugVflag;

	/* -e: check environment variables before global variables */
	Boolean checkEnvFirst;

	/* -f: the makefiles to read */
	StringList makefiles;

	/* -i: if true, ignore all errors from shell commands */
	Boolean ignoreErrors;

	/* -j: the maximum number of jobs that can run in parallel;
	 * this is coordinated with the submakes */
	int maxJobs;

	/* -k: if true and an error occurs while making a node, continue
	 * making nodes that do not depend on the erroneous node */
	Boolean keepgoing;

	/* -N: execute no commands from the targets */
	Boolean noRecursiveExecute;

	/* -n: execute almost no commands from the targets */
	Boolean noExecute;

	/* -q: if true, we aren't supposed to really make anything, just see
	 * if the targets are out-of-date */
	Boolean queryFlag;

	/* -r: raw mode, without loading the builtin rules. */
	Boolean noBuiltins;

	/* -s: don't echo the shell commands before executing them */
	Boolean beSilent;

	/* -t: touch the targets if they are out-of-date, but don't actually
	 * make them */
	Boolean touchFlag;

	/* -[Vv]: print expanded or unexpanded selected variables */
	PrintVarsMode printVars;
	/* -[Vv]: the variables to print */
	StringList variables;

	/* -W: if true, makefile parsing warnings are treated as errors */
	Boolean parseWarnFatal;

	/* -w: print Entering and Leaving for submakes */
	Boolean enterFlag;

	/* -X: if true, do not export variables set on the command line to the
	 * environment. */
	Boolean varNoExportEnv;

	/* The target names specified on the command line.
	 * Used to resolve .if make(...) statements. */
	StringList create;

} CmdOpts;

extern CmdOpts opts;

#include "nonints.h"

void GNode_UpdateYoungestChild(GNode *, GNode *);
Boolean GNode_IsOODate(GNode *);
void Make_ExpandUse(GNodeList *);
time_t Make_Recheck(GNode *);
void Make_HandleUse(GNode *, GNode *);
void Make_Update(GNode *);
void Make_DoAllVar(GNode *);
Boolean Make_Run(GNodeList *);
Boolean shouldDieQuietly(GNode *, int);
void PrintOnError(GNode *, const char *);
void Main_ExportMAKEFLAGS(Boolean);
Boolean Main_SetObjdir(Boolean, const char *, ...) MAKE_ATTR_PRINTFLIKE(2, 3);
int mkTempFile(const char *, char **);
int str2Lst_Append(StringList *, char *);
void GNode_FprintDetails(FILE *, const char *, const GNode *, const char *);
Boolean GNode_ShouldExecute(GNode *gn);

/* See if the node was seen on the left-hand side of a dependency operator. */
MAKE_INLINE Boolean
GNode_IsTarget(const GNode *gn)
{
	return (gn->type & OP_OPMASK) != 0;
}

MAKE_INLINE const char *
GNode_Path(const GNode *gn)
{
	return gn->path != NULL ? gn->path : gn->name;
}

MAKE_INLINE Boolean
GNode_IsWaitingFor(const GNode *gn)
{
	return (gn->flags & REMAKE) && gn->made <= REQUESTED;
}

MAKE_INLINE Boolean
GNode_IsReady(const GNode *gn)
{
	return gn->made > DEFERRED;
}

MAKE_INLINE Boolean
GNode_IsDone(const GNode *gn)
{
	return gn->made >= MADE;
}

MAKE_INLINE Boolean
GNode_IsError(const GNode *gn)
{
	return gn->made == ERROR || gn->made == ABORTED;
}

MAKE_INLINE const char *
GNode_VarTarget(GNode *gn) { return Var_ValueDirect(TARGET, gn); }
MAKE_INLINE const char *
GNode_VarOodate(GNode *gn) { return Var_ValueDirect(OODATE, gn); }
MAKE_INLINE const char *
GNode_VarAllsrc(GNode *gn) { return Var_ValueDirect(ALLSRC, gn); }
MAKE_INLINE const char *
GNode_VarImpsrc(GNode *gn) { return Var_ValueDirect(IMPSRC, gn); }
MAKE_INLINE const char *
GNode_VarPrefix(GNode *gn) { return Var_ValueDirect(PREFIX, gn); }
MAKE_INLINE const char *
GNode_VarArchive(GNode *gn) { return Var_ValueDirect(ARCHIVE, gn); }
MAKE_INLINE const char *
GNode_VarMember(GNode *gn) { return Var_ValueDirect(MEMBER, gn); }

#ifdef __GNUC__
#define UNCONST(ptr)	({		\
    union __unconst {			\
	const void *__cp;		\
	void *__p;			\
    } __d;				\
    __d.__cp = ptr, __d.__p; })
#else
#define UNCONST(ptr)	(void *)(ptr)
#endif

/* At least GNU/Hurd systems lack hardcoded MAXPATHLEN/PATH_MAX */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN	BMAKE_PATH_MAX
#endif
#ifndef PATH_MAX
#define PATH_MAX	MAXPATHLEN
#endif

#if defined(SYSV)
#define KILLPG(pid, sig) kill(-(pid), (sig))
#else
#define KILLPG(pid, sig) killpg((pid), (sig))
#endif

MAKE_INLINE Boolean
ch_isalnum(char ch) { return isalnum((unsigned char)ch) != 0; }
MAKE_INLINE Boolean
ch_isalpha(char ch) { return isalpha((unsigned char)ch) != 0; }
MAKE_INLINE Boolean
ch_isdigit(char ch) { return isdigit((unsigned char)ch) != 0; }
MAKE_INLINE Boolean
ch_isspace(char ch) { return isspace((unsigned char)ch) != 0; }
MAKE_INLINE Boolean
ch_isupper(char ch) { return isupper((unsigned char)ch) != 0; }
MAKE_INLINE char
ch_tolower(char ch) { return (char)tolower((unsigned char)ch); }
MAKE_INLINE char
ch_toupper(char ch) { return (char)toupper((unsigned char)ch); }

MAKE_INLINE void
cpp_skip_whitespace(const char **pp)
{
	while (ch_isspace(**pp))
		(*pp)++;
}

MAKE_INLINE void
cpp_skip_hspace(const char **pp)
{
	while (**pp == ' ' || **pp == '\t')
		(*pp)++;
}

MAKE_INLINE void
pp_skip_whitespace(char **pp)
{
	while (ch_isspace(**pp))
		(*pp)++;
}

MAKE_INLINE void
pp_skip_hspace(char **pp)
{
	while (**pp == ' ' || **pp == '\t')
		(*pp)++;
}

#if defined(lint)
#  define MAKE_RCSID(id) extern void do_not_define_rcsid(void)
#elif defined(MAKE_NATIVE)
#  include <sys/cdefs.h>
#  define MAKE_RCSID(id) __RCSID(id)
#elif defined(MAKE_ALL_IN_ONE) && defined(__COUNTER__)
#  define MAKE_RCSID_CONCAT(x, y) CONCAT(x, y)
#  define MAKE_RCSID(id) static volatile char \
	MAKE_RCSID_CONCAT(rcsid_, __COUNTER__)[] = id
#elif defined(MAKE_ALL_IN_ONE)
#  define MAKE_RCSID(id) extern void do_not_define_rcsid(void)
#else
#  define MAKE_RCSID(id) static volatile char rcsid[] = id
#endif

#endif /* MAKE_MAKE_H */
