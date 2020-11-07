/*	$NetBSD: make.h,v 1.179 2020/11/01 17:47:26 rillig Exp $	*/

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
#define	MAKE_GNUC_PREREQ(x, y)						\
	((__GNUC__ == (x) && __GNUC_MINOR__ >= (y)) ||			\
	 (__GNUC__ > (x)))
#else /* defined(__GNUC__) */
#define	MAKE_GNUC_PREREQ(x, y)	0
#endif /* defined(__GNUC__) */

#if MAKE_GNUC_PREREQ(2, 7)
#define	MAKE_ATTR_UNUSED	__attribute__((__unused__))
#else
#define	MAKE_ATTR_UNUSED	/* delete */
#endif

#if MAKE_GNUC_PREREQ(2, 5)
#define	MAKE_ATTR_DEAD		__attribute__((__noreturn__))
#elif defined(__GNUC__)
#define	MAKE_ATTR_DEAD		__volatile
#else
#define	MAKE_ATTR_DEAD		/* delete */
#endif

#if MAKE_GNUC_PREREQ(2, 7)
#define MAKE_ATTR_PRINTFLIKE(fmtarg, firstvararg)	\
	    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define MAKE_ATTR_PRINTFLIKE(fmtarg, firstvararg)	/* delete */
#endif

/*
 * A boolean type is defined as an integer, not an enum, for historic reasons.
 * The only allowed values are the constants TRUE and FALSE (1 and 0).
 */

#ifdef USE_DOUBLE_BOOLEAN
/* During development, to find type mismatches in function declarations. */
typedef double Boolean;
#define TRUE 1.0
#define FALSE 0.0
#elif defined(USE_UCHAR_BOOLEAN)
/* During development, to find code that depends on the exact value of TRUE or
 * that stores other values in Boolean variables. */
typedef unsigned char Boolean;
#define TRUE ((unsigned char)0xFF)
#define FALSE ((unsigned char)0x00)
#elif defined(USE_CHAR_BOOLEAN)
/* During development, to find code that uses a boolean as array index, via
 * -Wchar-subscripts. */
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
#define POSIX_SIGNALS
#endif

typedef enum  {
    UNMADE,			/* Not examined yet */
    DEFERRED,			/* Examined once (building child) */
    REQUESTED,			/* on toBeMade list */
    BEINGMADE,			/* Target is already being made.
				 * Indicates a cycle in the graph. */
    MADE,			/* Was out-of-date and has been made */
    UPTODATE,			/* Was already up-to-date */
    ERROR,			/* An error occurred while it was being
				 * made (used only in compat mode) */
    ABORTED			/* The target was aborted due to an error
				 * making an inferior (compat). */
} GNodeMade;

/* The OP_ constants are used when parsing a dependency line as a way of
 * communicating to other parts of the program the way in which a target
 * should be made.
 *
 * Some of the OP_ constants can be combined, others cannot. */
typedef enum GNodeType {
    /* The dependency operator ':' is the most common one.  The commands of
     * this node are executed if any child is out-of-date. */
    OP_DEPENDS		= 1 << 0,
    /* The dependency operator '!' always executes its commands, even if
     * its children are up-to-date. */
    OP_FORCE		= 1 << 1,
    /* The dependency operator '::' behaves like ':', except that it allows
     * multiple dependency groups to be defined.  Each of these groups is
     * executed on its own, independently from the others. */
    OP_DOUBLEDEP	= 1 << 2,

    /* Matches the dependency operators ':', '!' and '::'. */
    OP_OPMASK		= OP_DEPENDS|OP_FORCE|OP_DOUBLEDEP,

    /* Don't care if the target doesn't exist and can't be created */
    OP_OPTIONAL		= 1 << 3,
    /* Use associated commands for parents */
    OP_USE		= 1 << 4,
    /* Target is never out of date, but always execute commands anyway.
     * Its time doesn't matter, so it has none...sort of */
    OP_EXEC		= 1 << 5,
    /* Ignore non-zero exit status from shell commands when creating the node */
    OP_IGNORE		= 1 << 6,
    /* Don't remove the target when interrupted */
    OP_PRECIOUS		= 1 << 7,
    /* Don't echo commands when executed */
    OP_SILENT		= 1 << 8,
    /* Target is a recursive make so its commands should always be executed
     * when it is out of date, regardless of the state of the -n or -t flags */
    OP_MAKE		= 1 << 9,
    /* Target is out-of-date only if any of its children was out-of-date */
    OP_JOIN		= 1 << 10,
    /* Assume the children of the node have been already made */
    OP_MADE		= 1 << 11,
    /* Special .BEGIN, .END, .INTERRUPT */
    OP_SPECIAL		= 1 << 12,
    /* Like .USE, only prepend commands */
    OP_USEBEFORE	= 1 << 13,
    /* The node is invisible to its parents. I.e. it doesn't show up in the
     * parents' local variables. */
    OP_INVISIBLE	= 1 << 14,
    /* The node is exempt from normal 'main target' processing in parse.c */
    OP_NOTMAIN		= 1 << 15,
    /* Not a file target; run always */
    OP_PHONY		= 1 << 16,
    /* Don't search for file in the path */
    OP_NOPATH		= 1 << 17,
    /* .WAIT phony node */
    OP_WAIT		= 1 << 18,
    /* .NOMETA do not create a .meta file */
    OP_NOMETA		= 1 << 19,
    /* .META we _do_ want a .meta file */
    OP_META		= 1 << 20,
    /* Do not compare commands in .meta file */
    OP_NOMETA_CMP	= 1 << 21,
    /* Possibly a submake node */
    OP_SUBMAKE		= 1 << 22,

    /* Attributes applied by PMake */

    /* The node is a transformation rule */
    OP_TRANSFORM	= 1 << 31,
    /* Target is a member of an archive */
    /* XXX: How does this differ from OP_ARCHV? */
    OP_MEMBER		= 1 << 30,
    /* The node is a library,
     * its name has the form "-l<libname>" */
    OP_LIB		= 1 << 29,
    /* The node is an archive member,
     * its name has the form "archive(member)" */
    /* XXX: How does this differ from OP_MEMBER? */
    OP_ARCHV		= 1 << 28,
    /* Target has all the commands it should. Used when parsing to catch
     * multiple command groups for a target.  Only applies to the dependency
     * operators ':' and '!', but not to '::'. */
    OP_HAS_COMMANDS	= 1 << 27,
    /* The special command "..." has been seen. All further commands from
     * this node will be saved on the .END node instead, to be executed at
     * the very end. */
    OP_SAVE_CMDS	= 1 << 26,
    /* Already processed by Suff_FindDeps */
    OP_DEPS_FOUND	= 1 << 25,
    /* Node found while expanding .ALLSRC */
    OP_MARK		= 1 << 24,

    OP_NOTARGET		= OP_NOTMAIN | OP_USE | OP_EXEC | OP_TRANSFORM
} GNodeType;

typedef enum GNodeFlags {
    REMAKE	= 0x0001,	/* this target needs to be (re)made */
    CHILDMADE	= 0x0002,	/* children of this target were made */
    FORCE	= 0x0004,	/* children don't exist, and we pretend made */
    DONE_WAIT	= 0x0008,	/* Set by Make_ProcessWait() */
    DONE_ORDER	= 0x0010,	/* Build requested by .ORDER processing */
    FROM_DEPEND	= 0x0020,	/* Node created from .depend */
    DONE_ALLSRC	= 0x0040,	/* We do it once only */
    CYCLE	= 0x1000,	/* Used by MakePrintStatus */
    DONECYCLE	= 0x2000,	/* Used by MakePrintStatus */
    INTERNAL	= 0x4000	/* Internal use only */
} GNodeFlags;

typedef struct List StringList;
typedef struct ListNode StringListNode;

typedef struct List GNodeList;
typedef struct ListNode GNodeListNode;

typedef struct List /* of CachedDir */ SearchPath;

/* A graph node represents a target that can possibly be made, including its
 * relation to other targets and a lot of other details. */
typedef struct GNode {
    /* The target's name, such as "clean" or "make.c" */
    char *name;
    /* The unexpanded name of a .USE node */
    char *uname;
    /* The full pathname of the file belonging to the target.
     * XXX: What about .PHONY targets? These don't have an associated path. */
    char *path;

    /* The type of operator used to define the sources (see the OP flags below).
     * XXX: This looks like a wild mixture of type and flags. */
    GNodeType type;
    GNodeFlags flags;

    /* The state of processing on this node */
    GNodeMade made;
    int unmade;			/* The number of unmade children */

    /* The modification time; 0 means the node does not have a corresponding
     * file; see Make_OODate. */
    time_t mtime;
    struct GNode *youngestChild;

    /* The GNodes for which this node is an implied source. May be empty.
     * For example, when there is an inference rule for .c.o, the node for
     * file.c has the node for file.o in this list. */
    GNodeList *implicitParents;

    /* Other nodes of the same name, for the '::' operator. */
    GNodeList *cohorts;

    /* The nodes that depend on this one, or in other words, the nodes for
     * which this is a source. */
    GNodeList *parents;
    /* The nodes on which this one depends. */
    GNodeList *children;

    /* .ORDER nodes we need made. The nodes that must be made (if they're
     * made) before this node can be made, but that do not enter into the
     * datedness of this node. */
    GNodeList *order_pred;
    /* .ORDER nodes who need us. The nodes that must be made (if they're made
     * at all) after this node is made, but that do not depend on this node,
     * in the normal sense. */
    GNodeList *order_succ;

    /* The "#n" suffix for this cohort, or "" for other nodes */
    char cohort_num[8];
    /* The number of unmade instances on the cohorts list */
    int unmade_cohorts;
    /* Pointer to the first instance of a '::' node; only set when on a
     * cohorts list */
    struct GNode *centurion;

    /* Last time (sequence number) we tried to make this node */
    unsigned int checked_seqno;

    /* The "local" variables that are specific to this target and this target
     * only, such as $@, $<, $?.
     *
     * Also used for the global variable scopes VAR_GLOBAL, VAR_CMDLINE,
     * VAR_INTERNAL, which contain variables with arbitrary names. */
    HashTable /* of Var pointer */ context;

    /* The commands to be given to a shell to create this target. */
    StringList *commands;

    /* Suffix for the node (determined by Suff_FindDeps and opaque to everyone
     * but the Suff module) */
    struct Suff *suffix;

    /* filename where the GNode got defined */
    const char *fname;
    /* line number where the GNode got defined */
    int lineno;
} GNode;

/*
 * Error levels for parsing. PARSE_FATAL means the process cannot continue
 * once the top-level makefile has been parsed. PARSE_WARNING and PARSE_INFO
 * mean it can.
 */
typedef enum ParseErrorLevel {
    PARSE_FATAL = 1,
    PARSE_WARNING,
    PARSE_INFO
} ParseErrorLevel;

/*
 * Values returned by Cond_EvalLine and Cond_EvalCondition.
 */
typedef enum CondEvalResult {
    COND_PARSE,			/* Parse the next lines */
    COND_SKIP,			/* Skip the next lines */
    COND_INVALID		/* Not a conditional statement */
} CondEvalResult;

/*
 * Definitions for the "local" variables. Used only for clarity.
 */
#define TARGET		"@"	/* Target of dependency */
#define OODATE		"?"	/* All out-of-date sources */
#define ALLSRC		">"	/* All sources */
#define IMPSRC		"<"	/* Source implied by transformation */
#define PREFIX		"*"	/* Common prefix */
#define ARCHIVE		"!"	/* Archive in "archive(member)" syntax */
#define MEMBER		"%"	/* Member in "archive(member)" syntax */

#define FTARGET		"@F"	/* file part of TARGET */
#define DTARGET		"@D"	/* directory part of TARGET */
#define FIMPSRC		"<F"	/* file part of IMPSRC */
#define DIMPSRC		"<D"	/* directory part of IMPSRC */
#define FPREFIX		"*F"	/* file part of PREFIX */
#define DPREFIX		"*D"	/* directory part of PREFIX */

/*
 * Global Variables
 */
extern SearchPath *dirSearchPath;
				/* The list of directories to search when
				 * looking for targets */
extern Boolean  allPrecious;	/* True if every target is precious */
extern Boolean  deleteOnError;	/* True if failed targets should be deleted */
extern Boolean	doing_depend;	/* TRUE if processing .depend */

extern GNode    *DEFAULT;	/* .DEFAULT rule */

extern GNode	*VAR_INTERNAL;	/* Variables defined internally by make
				 * which should not override those set by
				 * makefiles.
				 */
extern GNode    *VAR_GLOBAL;	/* Variables defined in a global context, e.g
				 * in the Makefile itself */
extern GNode    *VAR_CMDLINE;	/* Variables defined on the command line */
extern char	var_Error[];	/* Value returned by Var_Parse when an error
				 * is encountered. It actually points to
				 * an empty string, so naive callers needn't
				 * worry about it. */

extern time_t	now;		/* The time at the start of this whole
				 * process */

extern Boolean	oldVars;	/* Do old-style variable substitution */

extern SearchPath *sysIncPath;	/* The system include path. */
extern SearchPath *defSysIncPath; /* The default system include path. */

extern char	curdir[];	/* Startup directory */
extern char	*progname;	/* The program name */
extern char	*makeDependfile; /* .depend */
extern char	**savedEnv;	 /* if we replaced environ this will be non-NULL */

extern int	makelevel;

/*
 * We cannot vfork() in a child of vfork().
 * Most systems do not enforce this but some do.
 */
#define vFork() ((getpid() == myPid) ? vfork() : fork())
extern pid_t	myPid;

#define	MAKEFLAGS	".MAKEFLAGS"
#define	MAKEOVERRIDES	".MAKEOVERRIDES"
#define	MAKE_JOB_PREFIX	".MAKE.JOB.PREFIX" /* prefix for job target output */
#define	MAKE_EXPORTED	".MAKE.EXPORTED"   /* variables we export */
#define	MAKE_MAKEFILES	".MAKE.MAKEFILES"  /* all makefiles already loaded */
#define	MAKE_LEVEL	".MAKE.LEVEL"	   /* recursion level */
#define MAKEFILE_PREFERENCE ".MAKE.MAKEFILE_PREFERENCE"
#define MAKE_DEPENDFILE	".MAKE.DEPENDFILE" /* .depend */
#define MAKE_MODE	".MAKE.MODE"
#ifndef MAKE_LEVEL_ENV
# define MAKE_LEVEL_ENV	"MAKELEVEL"
#endif

typedef enum DebugFlags {
    DEBUG_ARCH		= 1 << 0,
    DEBUG_COND		= 1 << 1,
    DEBUG_DIR		= 1 << 2,
    DEBUG_GRAPH1	= 1 << 3,
    DEBUG_GRAPH2	= 1 << 4,
    DEBUG_JOB		= 1 << 5,
    DEBUG_MAKE		= 1 << 6,
    DEBUG_SUFF		= 1 << 7,
    DEBUG_TARG		= 1 << 8,
    DEBUG_VAR		= 1 << 9,
    DEBUG_FOR		= 1 << 10,
    DEBUG_SHELL		= 1 << 11,
    DEBUG_ERROR		= 1 << 12,
    DEBUG_LOUD		= 1 << 13,
    DEBUG_META		= 1 << 14,
    DEBUG_HASH		= 1 << 15,

    DEBUG_GRAPH3	= 1 << 16,
    DEBUG_SCRIPT	= 1 << 17,
    DEBUG_PARSE		= 1 << 18,
    DEBUG_CWD		= 1 << 19,

    DEBUG_LINT		= 1 << 20
} DebugFlags;

#define CONCAT(a,b)	a##b

#define	DEBUG(module)	(opts.debug & CONCAT(DEBUG_,module))

void debug_printf(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2);

#define DEBUG0(module, text) \
    if (!DEBUG(module)) (void)0; \
    else debug_printf("%s", text)

#define DEBUG1(module, fmt, arg1) \
    if (!DEBUG(module)) (void)0; \
    else debug_printf(fmt, arg1)

#define DEBUG2(module, fmt, arg1, arg2) \
    if (!DEBUG(module)) (void)0; \
    else debug_printf(fmt, arg1, arg2)

#define DEBUG3(module, fmt, arg1, arg2, arg3) \
    if (!DEBUG(module)) (void)0; \
    else debug_printf(fmt, arg1, arg2, arg3)

#define DEBUG4(module, fmt, arg1, arg2, arg3, arg4) \
    if (!DEBUG(module)) (void)0; \
    else debug_printf(fmt, arg1, arg2, arg3, arg4)

#define DEBUG5(module, fmt, arg1, arg2, arg3, arg4, arg5) \
    if (!DEBUG(module)) (void)0; \
    else debug_printf(fmt, arg1, arg2, arg3, arg4, arg5)

typedef enum PrintVarsMode {
    COMPAT_VARS = 1,
    EXPAND_VARS
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

    /* -dV: for the -V option, print unexpanded variable values */
    Boolean debugVflag;

    /* -e: check environment variables before global variables */
    Boolean checkEnvFirst;

    /* -f: the makefiles to read */
    StringList *makefiles;

    /* -i: if true, ignore all errors from shell commands */
    Boolean ignoreErrors;

    /* -j: the maximum number of jobs that can run in parallel;
     * this is coordinated with the submakes */
    int maxJobs;

    /* -k: if true, continue on unaffected portions of the graph when an
     * error occurs in one portion */
    Boolean keepgoing;

    /* -N: execute no commands from the targets */
    Boolean noRecursiveExecute;

    /* -n: execute almost no commands from the targets */
    Boolean noExecute;

    /* -q: if true, we aren't supposed to really make anything, just see if
     * the targets are out-of-date */
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
    StringList *variables;

    /* -W: if true, makefile parsing warnings are treated as errors */
    Boolean parseWarnFatal;

    /* -w: print Entering and Leaving for submakes */
    Boolean enterFlag;

    /* -X: if true, do not export variables set on the command line to the
     * environment. */
    Boolean varNoExportEnv;

    /* The target names specified on the command line.
     * Used to resolve .if make(...) statements. */
    StringList *create;

} CmdOpts;

extern CmdOpts opts;

#include "nonints.h"

void Make_TimeStamp(GNode *, GNode *);
Boolean Make_OODate(GNode *);
void Make_ExpandUse(GNodeList *);
time_t Make_Recheck(GNode *);
void Make_HandleUse(GNode *, GNode *);
void Make_Update(GNode *);
void Make_DoAllVar(GNode *);
Boolean Make_Run(GNodeList *);
int dieQuietly(GNode *, int);
void PrintOnError(GNode *, const char *);
void Main_ExportMAKEFLAGS(Boolean);
Boolean Main_SetObjdir(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2);
int mkTempFile(const char *, char **);
int str2Lst_Append(StringList *, char *, const char *);
void GNode_FprintDetails(FILE *, const char *, const GNode *, const char *);
Boolean GNode_ShouldExecute(GNode *gn);

/* See if the node was seen on the left-hand side of a dependency operator. */
static MAKE_ATTR_UNUSED Boolean
GNode_IsTarget(const GNode *gn)
{
    return (gn->type & OP_OPMASK) != 0;
}

static MAKE_ATTR_UNUSED const char *
GNode_Path(const GNode *gn)
{
    return gn->path != NULL ? gn->path : gn->name;
}

static MAKE_ATTR_UNUSED const char *
GNode_VarTarget(GNode *gn) { return Var_ValueDirect(TARGET, gn); }
static MAKE_ATTR_UNUSED const char *
GNode_VarOodate(GNode *gn) { return Var_ValueDirect(OODATE, gn); }
static MAKE_ATTR_UNUSED const char *
GNode_VarAllsrc(GNode *gn) { return Var_ValueDirect(ALLSRC, gn); }
static MAKE_ATTR_UNUSED const char *
GNode_VarImpsrc(GNode *gn) { return Var_ValueDirect(IMPSRC, gn); }
static MAKE_ATTR_UNUSED const char *
GNode_VarPrefix(GNode *gn) { return Var_ValueDirect(PREFIX, gn); }
static MAKE_ATTR_UNUSED const char *
GNode_VarArchive(GNode *gn) { return Var_ValueDirect(ARCHIVE, gn); }
static MAKE_ATTR_UNUSED const char *
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
#define KILLPG(pid, sig)	kill(-(pid), (sig))
#else
#define KILLPG(pid, sig)	killpg((pid), (sig))
#endif

static inline MAKE_ATTR_UNUSED Boolean ch_isalnum(char ch)
{ return isalnum((unsigned char)ch) != 0; }
static inline MAKE_ATTR_UNUSED Boolean ch_isalpha(char ch)
{ return isalpha((unsigned char)ch) != 0; }
static inline MAKE_ATTR_UNUSED Boolean ch_isdigit(char ch)
{ return isdigit((unsigned char)ch) != 0; }
static inline MAKE_ATTR_UNUSED Boolean ch_isspace(char ch)
{ return isspace((unsigned char)ch) != 0; }
static inline MAKE_ATTR_UNUSED Boolean ch_isupper(char ch)
{ return isupper((unsigned char)ch) != 0; }
static inline MAKE_ATTR_UNUSED char ch_tolower(char ch)
{ return (char)tolower((unsigned char)ch); }
static inline MAKE_ATTR_UNUSED char ch_toupper(char ch)
{ return (char)toupper((unsigned char)ch); }

static inline MAKE_ATTR_UNUSED void
cpp_skip_whitespace(const char **pp)
{
    while (ch_isspace(**pp))
	(*pp)++;
}

static inline MAKE_ATTR_UNUSED void
pp_skip_whitespace(char **pp)
{
    while (ch_isspace(**pp))
	(*pp)++;
}

#ifdef MAKE_NATIVE
#  include <sys/cdefs.h>
#  ifndef lint
#    define MAKE_RCSID(id) __RCSID(id)
#  endif
#else
#  define MAKE_RCSID(id) static volatile char rcsid[] = id
#endif

#endif /* MAKE_MAKE_H */
