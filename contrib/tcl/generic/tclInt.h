/*
 * tclInt.h --
 *
 *	Declarations of things used internally by the Tcl interpreter.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclInt.h 1.200 96/04/11 17:24:12
 */

#ifndef _TCLINT
#define _TCLINT

/*
 * Common include files needed by most of the Tcl source files are
 * included here, so that system-dependent personalizations for the
 * include files only have to be made in once place.  This results
 * in a few extra includes, but greater modularity.  The order of
 * the three groups of #includes is important.  For example, stdio.h
 * is needed by tcl.h, and the _ANSI_ARGS_ declaration in tcl.h is
 * needed by stdlib.h in some configurations.
 */

#include <stdio.h>

#ifndef _TCL
#include "tcl.h"
#endif
#ifndef _REGEXP
#include "tclRegexp.h"
#endif

#include <ctype.h>
#ifdef NO_LIMITS_H
#   include "../compat/limits.h"
#else
#   include <limits.h>
#endif
#ifdef NO_STDLIB_H
#   include "../compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#ifdef NO_STRING_H
#include "../compat/string.h"
#else
#include <string.h>
#endif
#if defined(__STDC__) || defined(HAS_STDARG)
#   include <stdarg.h>
#else
#   include <varargs.h>
#endif

/*
 *----------------------------------------------------------------
 * Data structures related to variables.   These are used primarily
 * in tclVar.c
 *----------------------------------------------------------------
 */

/*
 * The following structure defines a variable trace, which is used to
 * invoke a specific C procedure whenever certain operations are performed
 * on a variable.
 */

typedef struct VarTrace {
    Tcl_VarTraceProc *traceProc;/* Procedure to call when operations given
				 * by flags are performed on variable. */
    ClientData clientData;	/* Argument to pass to proc. */
    int flags;			/* What events the trace procedure is
				 * interested in:  OR-ed combination of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES, and
				 * TCL_TRACE_UNSETS. */
    struct VarTrace *nextPtr;	/* Next in list of traces associated with
				 * a particular variable. */
} VarTrace;

/*
 * When a variable trace is active (i.e. its associated procedure is
 * executing), one of the following structures is linked into a list
 * associated with the variable's interpreter.  The information in
 * the structure is needed in order for Tcl to behave reasonably
 * if traces are deleted while traces are active.
 */

typedef struct ActiveVarTrace {
    struct Var *varPtr;		/* Variable that's being traced. */
    struct ActiveVarTrace *nextPtr;
				/* Next in list of all active variable
				 * traces for the interpreter, or NULL
				 * if no more. */
    VarTrace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveVarTrace;

/*
 * The following structure describes an enumerative search in progress on
 * an array variable;  this are invoked with options to the "array"
 * command.
 */

typedef struct ArraySearch {
    int id;			/* Integer id used to distinguish among
				 * multiple concurrent searches for the
				 * same array. */
    struct Var *varPtr;		/* Pointer to array variable that's being
				 * searched. */
    Tcl_HashSearch search;	/* Info kept by the hash module about
				 * progress through the array. */
    Tcl_HashEntry *nextEntry;	/* Non-null means this is the next element
				 * to be enumerated (it's leftover from
				 * the Tcl_FirstHashEntry call or from
				 * an "array anymore" command).  NULL
				 * means must call Tcl_NextHashEntry
				 * to get value to return. */
    struct ArraySearch *nextPtr;/* Next in list of all active searches
				 * for this variable, or NULL if this is
				 * the last one. */
} ArraySearch;

/*
 * The structure below defines a variable, which associates a string name
 * with a string value.  Pointers to these structures are kept as the
 * values of hash table entries, and the name of each variable is stored
 * in the hash entry.
 */

typedef struct Var {
    int valueLength;		/* Holds the number of non-null bytes
				 * actually occupied by the variable's
				 * current value in value.string (extra
				 * space is sometimes left for expansion).
				 * For array and global variables this is
				 * meaningless. */
    int valueSpace;		/* Total number of bytes of space allocated
				 * at value.string.  0 means there is no
				 * space allocated. */
    union {
	char *string;		/* String value of variable, used for scalar
				 * variables and array elements.  Malloc-ed. */
	Tcl_HashTable *tablePtr;/* For array variables, this points to
				 * information about the hash table used
				 * to implement the associative array. 
				 * Points to malloc-ed data. */
	struct Var *upvarPtr;	/* If this is a global variable being
				 * referred to in a procedure, or a variable
				 * created by "upvar", this field points to
				 * the record for the higher-level variable. */
    } value;
    Tcl_HashEntry *hPtr;	/* Hash table entry that refers to this
				 * variable, or NULL if the variable has
				 * been detached from its hash table (e.g.
				 * an array is deleted, but some of its
				 * elements are still referred to in upvars). */
    int refCount;		/* Counts number of active uses of this
				 * variable, not including its main hash
				 * table entry: 1 for each additional variable
				 * whose upVarPtr points here, 1 for each
				 * nested trace active on variable.  This
				 * record can't be deleted until refCount
				 * becomes 0. */
    VarTrace *tracePtr;		/* First in list of all traces set for this
				 * variable. */
    ArraySearch *searchPtr;	/* First in list of all searches active
				 * for this variable, or NULL if none. */
    int flags;			/* Miscellaneous bits of information about
				 * variable.  See below for definitions. */
} Var;

/*
 * Flag bits for variables:
 *
 * VAR_ARRAY	-		1 means this is an array variable rather
 *				than a scalar variable.
 * VAR_UPVAR - 			1 means this variable just contains a
 *				pointer to another variable that has the
 *				real value.  Variables like this come
 *				about through the "upvar" and "global"
 *				commands.
 * VAR_UNDEFINED -		1 means that the variable is currently
 *				undefined.  Undefined variables usually
 *				go away completely, but if an undefined
 *				variable has a trace on it, or if it is
 *				a global variable being used by a procedure,
 *				then it stays around even when undefined.
 * VAR_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a read or write access, so
 *				new read or write accesses should not cause
 *				trace procedures to be called and the
 *				variable can't be deleted.
 */

#define VAR_ARRAY		1
#define VAR_UPVAR		2
#define VAR_UNDEFINED		4
#define VAR_TRACE_ACTIVE	0x10

/*
 *----------------------------------------------------------------
 * Data structures related to procedures.   These are used primarily
 * in tclProc.c
 *----------------------------------------------------------------
 */

/*
 * The structure below defines an argument to a procedure, which
 * consists of a name and an (optional) default value.
 */

typedef struct Arg {
    struct Arg *nextPtr;	/* Next argument for this procedure,
				 * or NULL if this is the last argument. */
    char *defValue;		/* Pointer to arg's default value, or NULL
				 * if no default value. */
    char name[4];		/* Name of argument starts here.  The name
				 * is followed by space for the default,
				 * if there is one.  The actual size of this
				 * field will be as large as necessary to
				 * hold both name and default value.  THIS
				 * MUST BE THE LAST FIELD IN THE STRUCTURE!! */
} Arg;

/*
 * The structure below defines a command procedure, which consists of
 * a collection of Tcl commands plus information about arguments and
 * variables.
 */

typedef struct Proc {
    struct Interp *iPtr;	/* Interpreter for which this command
				 * is defined. */
    int refCount;		/* Reference count:  1 if still present
				 * in command table plus 1 for each call
				 * to the procedure that is currently
				 * active.  This structure can be freed
				 * when refCount becomes zero. */
    char *command;		/* Command that constitutes the body of
				 * the procedure (dynamically allocated). */
    Arg *argPtr;		/* Pointer to first of procedure's formal
				 * arguments, or NULL if none. */
} Proc;

/*
 * The structure below defines a command trace.  This is used to allow Tcl
 * clients to find out whenever a command is about to be executed.
 */

typedef struct Trace {
    int level;			/* Only trace commands at nesting level
				 * less than or equal to this. */
    Tcl_CmdTraceProc *proc;	/* Procedure to call to trace command. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
    struct Trace *nextPtr;	/* Next in list of traces for this interp. */
} Trace;

/*
 * The structure below defines an entry in the assocData hash table which
 * is associated with an interpreter. The entry contains a pointer to a
 * function to call when the interpreter is deleted, and a pointer to
 * a user-defined piece of data.
 */

typedef struct AssocData {
    Tcl_InterpDeleteProc *proc;	/* Proc to call when deleting. */
    ClientData clientData;	/* Value to pass to proc. */
} AssocData;    

/*
 * The structure below defines a frame, which is a procedure invocation.
 * These structures exist only while procedures are being executed, and
 * provide a sort of call stack.
 */

typedef struct CallFrame {
    Tcl_HashTable varTable;	/* Hash table containing all of procedure's
				 * local variables. */
    int level;			/* Level of this procedure, for "uplevel"
				 * purposes (i.e. corresponds to nesting of
				 * callerVarPtr's, not callerPtr's).  1 means
				 * outer-most procedure, 0 means top-level. */
    int argc;			/* This and argv below describe name and
				 * arguments for this procedure invocation. */
    char **argv;		/* Array of arguments. */
    struct CallFrame *callerPtr;
				/* Value of interp->framePtr when this
				 * procedure was invoked (i.e. next in
				 * stack of all active procedures). */
    struct CallFrame *callerVarPtr;
				/* Value of interp->varFramePtr when this
				 * procedure was invoked (i.e. determines
				 * variable scoping within caller;  same
				 * as callerPtr unless an "uplevel" command
				 * or something equivalent was active in
				 * the caller). */
} CallFrame;

/*
 * The structure below defines one history event (a previously-executed
 * command that can be re-executed in whole or in part).
 */

typedef struct {
    char *command;		/* String containing previously-executed
				 * command. */
    int bytesAvl;		/* Total # of bytes available at *event (not
				 * all are necessarily in use now). */
} HistoryEvent;

/*
 *----------------------------------------------------------------
 * Data structures related to history.   These are used primarily
 * in tclHistory.c
 *----------------------------------------------------------------
 */

/*
 * The structure below defines a pending revision to the most recent
 * history event.  Changes are linked together into a list and applied
 * during the next call to Tcl_RecordHistory.  See the comments at the
 * beginning of tclHistory.c for information on revisions.
 */

typedef struct HistoryRev {
    int firstIndex;		/* Index of the first byte to replace in
				 * current history event. */
    int lastIndex;		/* Index of last byte to replace in
				 * current history event. */
    int newSize;		/* Number of bytes in newBytes. */
    char *newBytes;		/* Replacement for the range given by
				 * firstIndex and lastIndex (malloced). */
    struct HistoryRev *nextPtr;	/* Next in chain of revisions to apply, or
				 * NULL for end of list. */
} HistoryRev;

/*
 *----------------------------------------------------------------
 * Data structures related to expressions.  These are used only in
 * tclExpr.c.
 *----------------------------------------------------------------
 */

/*
 * The data structure below defines a math function (e.g. sin or hypot)
 * for use in Tcl expressions.
 */

#define MAX_MATH_ARGS 5
typedef struct MathFunc {
    int numArgs;		/* Number of arguments for function. */
    Tcl_ValueType argTypes[MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    Tcl_MathProc *proc;		/* Procedure that implements this function. */
    ClientData clientData;	/* Additional argument to pass to the function
				 * when invoking it. */
} MathFunc;

/*
 *----------------------------------------------------------------
 * One of the following structures exists for each command in
 * an interpreter.  The Tcl_Command opaque type actually refers
 * to these structures.
 *----------------------------------------------------------------
 */

typedef struct Command {
    Tcl_HashEntry *hPtr;	/* Pointer to the hash table entry in
				 * interp->commandTable that refers to
				 * this command.  Used to get a command's
				 * name from its Tcl_Command handle.  NULL
				 * means that the hash table entry has
				 * been removed already (this can happen
				 * if deleteProc causes the command to be
				 * deleted or recreated). */
    Tcl_CmdProc *proc;		/* Procedure to process command. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* Procedure to invoke when deleting
				 * command. */
    ClientData deleteData;	/* Arbitrary value to pass to deleteProc
				 * (usually the same as clientData). */
    int deleted;		/* Means that the command is in the process
				 * of being deleted (its deleteProc is
				 * currently executing).  Any other attempts
				 * to delete the command should be ignored. */
} Command;

/*
 *----------------------------------------------------------------
 * This structure defines an interpreter, which is a collection of
 * commands plus other state information related to interpreting
 * commands, such as variable storage.  Primary responsibility for
 * this data structure is in tclBasic.c, but almost every Tcl
 * source file uses something in here.
 *----------------------------------------------------------------
 */

typedef struct Interp {

    /*
     * Note:  the first three fields must match exactly the fields in
     * a Tcl_Interp struct (see tcl.h).  If you change one, be sure to
     * change the other.
     */

    char *result;		/* Points to result returned by last
				 * command. */
    Tcl_FreeProc *freeProc;	/* Zero means result is statically allocated.
				 * TCL_DYNAMIC means result was allocated with
				 * ckalloc and should be freed with ckfree.
				 * Other values give address of procedure
				 * to invoke to free the result.  Must be
				 * freed by Tcl_Eval before executing next
				 * command. */
    int errorLine;		/* When TCL_ERROR is returned, this gives
				 * the line number within the command where
				 * the error occurred (1 means first line). */
    Tcl_HashTable commandTable;	/* Contains all of the commands currently
				 * registered in this interpreter.  Indexed
				 * by strings; values have type (Command *). */
    Tcl_HashTable mathFuncTable;/* Contains all of the math functions currently
				 * defined for the interpreter.  Indexed by
				 * strings (function names);  values have
				 * type (MathFunc *). */

    /*
     * Information related to procedures and variables.  See tclProc.c
     * and tclvar.c for usage.
     */

    Tcl_HashTable globalTable;	/* Contains all global variables for
				 * interpreter. */
    int numLevels;		/* Keeps track of how many nested calls to
				 * Tcl_Eval are in progress for this
				 * interpreter.  It's used to delay deletion
				 * of the table until all Tcl_Eval invocations
				 * are completed. */
    int maxNestingDepth;	/* If numLevels exceeds this value then Tcl
				 * assumes that infinite recursion has
				 * occurred and it generates an error. */
    CallFrame *framePtr;	/* Points to top-most in stack of all nested
				 * procedure invocations.  NULL means there
				 * are no active procedures. */
    CallFrame *varFramePtr;	/* Points to the call frame whose variables
				 * are currently in use (same as framePtr
				 * unless an "uplevel" command is being
				 * executed).  NULL means no procedure is
				 * active or "uplevel 0" is being exec'ed. */
    ActiveVarTrace *activeTracePtr;
				/* First in list of active traces for interp,
				 * or NULL if no active traces. */
    int returnCode;		/* Completion code to return if current
				 * procedure exits with a TCL_RETURN code. */
    char *errorInfo;		/* Value to store in errorInfo if returnCode
				 * is TCL_ERROR.  Malloc'ed, may be NULL */
    char *errorCode;		/* Value to store in errorCode if returnCode
				 * is TCL_ERROR.  Malloc'ed, may be NULL */

    /*
     * Information related to history:
     */

    int numEvents;		/* Number of previously-executed commands
				 * to retain. */
    HistoryEvent *events;	/* Array containing numEvents entries
				 * (dynamically allocated). */
    int curEvent;		/* Index into events of place where current
				 * (or most recent) command is recorded. */
    int curEventNum;		/* Event number associated with the slot
				 * given by curEvent. */
    HistoryRev *revPtr;		/* First in list of pending revisions. */
    char *historyFirst;		/* First char. of current command executed
				 * from history module or NULL if none. */
    int revDisables;		/* 0 means history revision OK;  > 0 gives
				 * a count of number of times revision has
				 * been disabled. */
    char *evalFirst;		/* If TCL_RECORD_BOUNDS flag set, Tcl_Eval
				 * sets this field to point to the first
				 * char. of text from which the current
				 * command came.  Otherwise Tcl_Eval sets
				 * this to NULL. */
    char *evalLast;		/* Similar to evalFirst, except points to
				 * last character of current command. */

    /*
     * Information used by Tcl_AppendResult to keep track of partial
     * results.  See Tcl_AppendResult code for details.
     */

    char *appendResult;		/* Storage space for results generated
				 * by Tcl_AppendResult.  Malloc-ed.  NULL
				 * means not yet allocated. */
    int appendAvl;		/* Total amount of space available at
				 * partialResult. */
    int appendUsed;		/* Number of non-null bytes currently
				 * stored at partialResult. */

    /*
     * A cache of compiled regular expressions.  See Tcl_RegExpCompile
     * in tclUtil.c for details.
     */

#define NUM_REGEXPS 5
    char *patterns[NUM_REGEXPS];/* Strings corresponding to compiled
				 * regular expression patterns.  NULL
				 * means that this slot isn't used.
				 * Malloc-ed. */
    int patLengths[NUM_REGEXPS];/* Number of non-null characters in
				 * corresponding entry in patterns.
				 * -1 means entry isn't used. */
    regexp *regexps[NUM_REGEXPS];
				/* Compiled forms of above strings.  Also
				 * malloc-ed, or NULL if not in use yet. */

    /*
     * Information about packages.  Used only in tclPkg.c.
     */

    Tcl_HashTable packageTable;	/* Describes all of the packages loaded
				 * in or available to this interpreter.
				 * Keys are package names, values are
				 * (Package *) pointers. */
    char *packageUnknown;	/* Command to invoke during "package
				 * require" commands for packages that
				 * aren't described in packageTable. 
				 * Malloc'ed, may be NULL. */

    /*
     * Information used by Tcl_PrintDouble:
     */

    char pdFormat[10];		/* Format string used by Tcl_PrintDouble. */
    int pdPrec;			/* Current precision (used to restore the
				 * the tcl_precision variable after a bogus
				 * value has been put into it). */

    /*
     * Miscellaneous information:
     */

    int cmdCount;		/* Total number of times a command procedure
				 * has been called for this interpreter. */
    int noEval;			/* Non-zero means no commands should actually
				 * be executed:  just parse only.  Used in
				 * expressions when the result is already
				 * determined. */
    int evalFlags;		/* Flags to control next call to Tcl_Eval.
				 * Normally zero, but may be set before
				 * calling Tcl_Eval.  See below for valid
				 * values. */
    char *termPtr;		/* Character just after the last one in
				 * a command.  Set by Tcl_Eval before
				 * returning. */
    char *scriptFile;		/* NULL means there is no nested source
				 * command active;  otherwise this points to
				 * the name of the file being sourced (it's
				 * not malloc-ed:  it points to an argument
				 * to Tcl_EvalFile. */
    int flags;			/* Various flag bits.  See below. */
    Trace *tracePtr;		/* List of traces for this interpreter. */
    Tcl_HashTable *assocData;	/* Hash table for associating data with
                                 * this interpreter. Cleaned up when
                                 * this interpreter is deleted. */
    char resultSpace[TCL_RESULT_SIZE+1];
				/* Static space for storing small results. */
} Interp;

/*
 * EvalFlag bits for Interp structures:
 *
 * TCL_BRACKET_TERM	1 means that the current script is terminated by
 *			a close bracket rather than the end of the string.
 * TCL_RECORD_BOUNDS	Tells Tcl_Eval to record information in the
 *			evalFirst and evalLast fields for each command
 *			executed directly from the string (top-level
 *			commands and those from command substitution).
 * TCL_ALLOW_EXCEPTIONS	1 means it's OK for the script to terminate with
 *			a code other than TCL_OK or TCL_ERROR;  0 means
 *			codes other than these should be turned into errors.
 */

#define TCL_BRACKET_TERM	1
#define TCL_RECORD_BOUNDS	2
#define TCL_ALLOW_EXCEPTIONS	4

/*
 * Flag bits for Interp structures:
 *
 * DELETED:		Non-zero means the interpreter has been deleted:
 *			don't process any more commands for it, and destroy
 *			the structure as soon as all nested invocations of
 *			Tcl_Eval are done.
 * ERR_IN_PROGRESS:	Non-zero means an error unwind is already in progress.
 *			Zero means a command proc has been invoked since last
 *			error occured.
 * ERR_ALREADY_LOGGED:	Non-zero means information has already been logged
 *			in $errorInfo for the current Tcl_Eval instance,
 *			so Tcl_Eval needn't log it (used to implement the
 *			"error message log" command).
 * ERROR_CODE_SET:	Non-zero means that Tcl_SetErrorCode has been
 *			called to record information for the current
 *			error.  Zero means Tcl_Eval must clear the
 *			errorCode variable if an error is returned.
 * EXPR_INITIALIZED:	1 means initialization specific to expressions has
 *			been carried out.
 */

#define DELETED			1
#define ERR_IN_PROGRESS		2
#define ERR_ALREADY_LOGGED	4
#define ERROR_CODE_SET		8
#define EXPR_INITIALIZED	0x10

/*
 * Default value for the pdPrec and pdFormat fields of interpreters:
 */

#define DEFAULT_PD_PREC 6
#define DEFAULT_PD_FORMAT "%g"

/*
 *----------------------------------------------------------------
 * Data structures related to command parsing.   These are used in
 * tclParse.c and its clients.
 *----------------------------------------------------------------
 */

/*
 * The following data structure is used by various parsing procedures
 * to hold information about where to store the results of parsing
 * (e.g. the substituted contents of a quoted argument, or the result
 * of a nested command).  At any given time, the space available
 * for output is fixed, but a procedure may be called to expand the
 * space available if the current space runs out.
 */

typedef struct ParseValue {
    char *buffer;		/* Address of first character in
				 * output buffer. */
    char *next;			/* Place to store next character in
				 * output buffer. */
    char *end;			/* Address of the last usable character
				 * in the buffer. */
    void (*expandProc) _ANSI_ARGS_((struct ParseValue *pvPtr, int needed));
				/* Procedure to call when space runs out;
				 * it will make more space. */
    ClientData clientData;	/* Arbitrary information for use of
				 * expandProc. */
} ParseValue;

/*
 * A table used to classify input characters to assist in parsing
 * Tcl commands.  The table should be indexed with a signed character
 * using the CHAR_TYPE macro.  The character may have a negative
 * value.
 */

extern char tclTypeTable[];
#define CHAR_TYPE(c) (tclTypeTable+128)[c]

/*
 * Possible values returned by CHAR_TYPE:
 *
 * TCL_NORMAL -		All characters that don't have special significance
 *			to the Tcl language.
 * TCL_SPACE -		Character is space, tab, or return.
 * TCL_COMMAND_END -	Character is newline or null or semicolon or
 *			close-bracket.
 * TCL_QUOTE -		Character is a double-quote.
 * TCL_OPEN_BRACKET -	Character is a "[".
 * TCL_OPEN_BRACE -	Character is a "{".
 * TCL_CLOSE_BRACE -	Character is a "}".
 * TCL_BACKSLASH -	Character is a "\".
 * TCL_DOLLAR -		Character is a "$".
 */

#define TCL_NORMAL		0
#define TCL_SPACE		1
#define TCL_COMMAND_END		2
#define TCL_QUOTE		3
#define TCL_OPEN_BRACKET	4
#define TCL_OPEN_BRACE		5
#define TCL_CLOSE_BRACE		6
#define TCL_BACKSLASH		7
#define TCL_DOLLAR		8

/*
 * Maximum number of levels of nesting permitted in Tcl commands (used
 * to catch infinite recursion).
 */

#define MAX_NESTING_DEPTH	1000

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */

#define UCHAR(c) ((unsigned char) (c))

/*
 * Given a size or address, the macro below "aligns" it to the machine's
 * memory unit size (e.g. an 8-byte boundary) so that anything can be
 * placed at the aligned address without fear of an alignment error.
 */

#define TCL_ALIGN(x) ((x + 7) & ~7)

/*
 * For each event source (created with Tcl_CreateEventSource) there
 * is a structure of the following type:
 */

typedef struct TclEventSource {
    Tcl_EventSetupProc *setupProc;	/* This procedure is called by
					 * Tcl_DoOneEvent to set up information
					 * for the wait operation, such as
					 * files to wait for or maximum
					 * timeout. */
    Tcl_EventCheckProc *checkProc;	/* This procedure is called by
					 * Tcl_DoOneEvent after its wait
					 * operation to see what events
					 * are ready and queue them. */
    ClientData clientData;		/* Arbitrary one-word argument to pass
					 * to setupProc and checkProc. */
    struct TclEventSource *nextPtr;	/* Next in list of all event sources
					 * defined for applicaton. */
} TclEventSource;

/*
 * The following macros are used to specify the runtime platform
 * setting of the tclPlatform variable.
 */

typedef enum {
    TCL_PLATFORM_UNIX,		/* Any Unix-like OS. */
    TCL_PLATFORM_MAC,		/* MacOS. */
    TCL_PLATFORM_WINDOWS	/* Any Microsoft Windows OS. */
} TclPlatformType;

/*
 *----------------------------------------------------------------
 * Variables shared among Tcl modules but not used by the outside
 * world:
 *----------------------------------------------------------------
 */

extern Tcl_Time			tclBlockTime;
extern int			tclBlockTimeSet;
extern char *			tclExecutableName;
extern TclEventSource *		tclFirstEventSourcePtr;
extern Tcl_ChannelType	 	tclFileChannelType;
extern char *			tclMemDumpFileName;
extern TclPlatformType		tclPlatform;

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl modules but not used by the outside
 * world:
 *----------------------------------------------------------------
 */

EXTERN void		panic();
EXTERN int		TclCleanupChildren _ANSI_ARGS_((Tcl_Interp *interp,
		            int numPids, int *pidPtr, Tcl_Channel errorChan));
EXTERN int		TclCloseFile _ANSI_ARGS_((Tcl_File file));
EXTERN char *		TclConvertToNative _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_DString *bufferPtr));
EXTERN char *		TclConvertToNetwork _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_DString *bufferPtr));
EXTERN void		TclCopyAndCollapse _ANSI_ARGS_((int count, char *src,
			    char *dst));
EXTERN int		TclChdir _ANSI_ARGS_((Tcl_Interp *interp,
			    char *dirName));
EXTERN void		TclClosePipeFile _ANSI_ARGS_((Tcl_File file));
EXTERN Tcl_Channel	TclCreateCommandChannel _ANSI_ARGS_((
    			    Tcl_File readFile, Tcl_File writeFile,
			    Tcl_File errorFile, int numPids, int *pidPtr));
EXTERN int              TclCreatePipe _ANSI_ARGS_((Tcl_File *readPipe,
			    Tcl_File *writePipe));
EXTERN int		TclCreatePipeline _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, int **pidArrayPtr,
			    Tcl_File *inPipePtr,
			    Tcl_File *outPipePtr,
			    Tcl_File *errFilePtr));
EXTERN Tcl_File		TclCreateTempFile _ANSI_ARGS_((char *contents));
EXTERN void		TclDeleteVars _ANSI_ARGS_((Interp *iPtr,
			    Tcl_HashTable *tablePtr));
EXTERN int		TclDoGlob _ANSI_ARGS_((Tcl_Interp *interp,
			    char *separators, Tcl_DString *headPtr,
			    char *tail));
EXTERN void		TclExpandParseValue _ANSI_ARGS_((ParseValue *pvPtr,
			    int needed));
EXTERN void		TclExprFloatError _ANSI_ARGS_((Tcl_Interp *interp,
			    double value));
EXTERN int		TclFindElement _ANSI_ARGS_((Tcl_Interp *interp,
			    char *list, char **elementPtr, char **nextPtr,
			    int *sizePtr, int *bracePtr));
EXTERN Proc *		TclFindProc _ANSI_ARGS_((Interp *iPtr,
			    char *procName));
EXTERN void		TclFreePackageInfo _ANSI_ARGS_((Interp *iPtr));
EXTERN char *		TclGetCwd _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN unsigned long	TclGetClicks _ANSI_ARGS_((void));
EXTERN char *		TclGetExtension _ANSI_ARGS_((char *name));
EXTERN void		TclGetAndDetachPids _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Channel chan));
EXTERN int		TclGetDate _ANSI_ARGS_((char *p,
			    unsigned long now, long zone,
			    unsigned long *timePtr));
EXTERN Tcl_Channel	TclGetDefaultStdChannel _ANSI_ARGS_((int type));
EXTERN char *		TclGetEnv _ANSI_ARGS_((char *name));
EXTERN int		TclGetFrame _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, CallFrame **framePtrPtr));
EXTERN int		TclGetOpenMode _ANSI_ARGS_((Tcl_Interp *interp,
        		    char *string, int *seekFlagPtr));
EXTERN unsigned long	TclGetSeconds _ANSI_ARGS_((void));
EXTERN void		TclGetTime _ANSI_ARGS_((Tcl_Time *time));
EXTERN int		TclGetTimeZone _ANSI_ARGS_((unsigned long time));
EXTERN char *		TclGetUserHome _ANSI_ARGS_((char *name,
			    Tcl_DString *bufferPtr));
EXTERN int		TclGetListIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *indexPtr));
EXTERN int		TclGetLoadedPackages _ANSI_ARGS_((Tcl_Interp *interp,
			    char *targetName));
EXTERN char *		TclGetUserHome _ANSI_ARGS_((char *name,
			    Tcl_DString *bufferPtr));
EXTERN int		TclGuessPackageName _ANSI_ARGS_((char *fileName,
			    Tcl_DString *bufPtr));
EXTERN int              TclHasPipes _ANSI_ARGS_((void));
EXTERN int		TclHasSockets _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclIdlePending _ANSI_ARGS_((void));
EXTERN int		TclInterpInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Proc *		TclIsProc _ANSI_ARGS_((Command *cmdPtr));
EXTERN int		TclLoadFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName, char *sym1, char *sym2,
			    Tcl_PackageInitProc **proc1Ptr,
			    Tcl_PackageInitProc **proc2Ptr));
EXTERN int		TclMakeFileTable _ANSI_ARGS_((Tcl_Interp *interp,
                            int noStdio));
EXTERN int		TclMatchFiles _ANSI_ARGS_((Tcl_Interp *interp,
			    char *separators, Tcl_DString *dirPtr,
			    char *pattern, char *tail));
EXTERN int		TclNeedSpace _ANSI_ARGS_((char *start, char *end));
EXTERN Tcl_File		TclOpenFile _ANSI_ARGS_((char *fname, int mode));
EXTERN int		TclParseBraces _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char **termPtr, ParseValue *pvPtr));
EXTERN int		TclParseNestedCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int flags, char **termPtr,
			    ParseValue *pvPtr));
EXTERN int		TclParseQuotes _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int termChar, int flags,
			    char **termPtr, ParseValue *pvPtr));
EXTERN int		TclParseWords _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int flags, int maxWords,
			    char **termPtr, int *argcPtr, char **argv,
			    ParseValue *pvPtr));
EXTERN void		TclPlatformExit _ANSI_ARGS_((int status));
EXTERN void		TclPlatformInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN char *		TclPrecTraceProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
EXTERN int		TclPreventAliasLoop _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *cmdInterp, char *cmdName,
                            Tcl_CmdProc *proc, ClientData clientData));
EXTERN int		TclReadFile _ANSI_ARGS_((Tcl_File file,
			    int shouldBlock, char *buf, int toRead));
EXTERN int		TclSeekFile _ANSI_ARGS_((Tcl_File file,
			    int offset, int whence));
EXTERN int		TclServiceIdle _ANSI_ARGS_((void));
EXTERN void		TclSetupEnv _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclSockGetPort _ANSI_ARGS_((Tcl_Interp *interp,
		            char *string, char *proto, int *portPtr));
EXTERN int		TclSockMinimumBuffers _ANSI_ARGS_((int sock,
        		    int size));
EXTERN int              TclSpawnPipeline _ANSI_ARGS_((Tcl_Interp *interp,
	                    int *pidPtr, int *numPids, int argc, char **argv,
			    Tcl_File inputFile,
			    Tcl_File outputFile,
	                    Tcl_File errorFile,
	                    char *intIn, char *finalOut));
EXTERN int		TclTestChannelCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		TclTestChannelEventCmd _ANSI_ARGS_((
    			    ClientData clientData, Tcl_Interp *interp,
                            int argc, char **argv));
EXTERN int		TclUpdateReturnInfo _ANSI_ARGS_((Interp *iPtr));
EXTERN int		TclWaitForFile _ANSI_ARGS_((Tcl_File file,
			    int mask, int timeout));
EXTERN char *		TclWordEnd _ANSI_ARGS_((char *start, int nested,
			    int *semiPtr));
EXTERN int		TclWriteFile _ANSI_ARGS_((Tcl_File file,
			    int shouldBlock, char *buf, int toWrite));

/*
 *----------------------------------------------------------------
 * Command procedures in the generic core:
 *----------------------------------------------------------------
 */

EXTERN int	Tcl_AfterCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_AppendCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ArrayCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_BreakCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_CaseCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_CatchCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_CdCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ClockCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_CloseCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ConcatCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ContinueCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_CpCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_EchoCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_EofCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ErrorCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_EvalCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ExecCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ExitCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ExprCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FblockedCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FconfigureCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FileCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FileEventCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FlushCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ForCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ForeachCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FormatCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_GetsCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_GlobalCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_GlobCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_HistoryCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_IfCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_IncrCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_InfoCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_InterpCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_JoinCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LappendCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LindexCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LinsertCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LlengthCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ListCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LoadCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LrangeCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LreplaceCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_LsCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LsearchCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LsortCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_MacBeepCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_MacSourceCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_MkdirCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_MvCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_OpenCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_PackageCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_PidCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ProcCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_PutsCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_PwdCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ReadCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_RegexpCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_RegsubCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_RenameCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ReturnCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_RmCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_RmdirCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ScanCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SeekCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SplitCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SocketCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SourceCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_StringCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SubstCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SwitchCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_TellCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_TimeCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_TraceCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_UnsetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_UpdateCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_UplevelCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_UpvarCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_VwaitCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_WhileCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	TclUnsupported0Cmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

#endif /* _TCLINT */
