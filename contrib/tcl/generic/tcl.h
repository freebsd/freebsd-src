/*
 * tcl.h --
 *
 *	This header file describes the externally-visible facilities
 *	of the Tcl interpreter.
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tcl.h 1.269 96/06/13 16:36:48
 */

#ifndef _TCL
#define _TCL

/*
 * The following definitions set up the proper options for Windows
 * compilers.  We use this method because there is no autoconf equivalent.
 */

#ifndef __WIN32__
#   if defined(_WIN32) || defined(WIN32)
#	define __WIN32__
#   endif
#endif

#ifdef __WIN32__
#   ifndef USE_PROTOTYPE
#	define USE_PROTOTYPE 1
#   endif
#   ifndef HAS_STDARG
#	define HAS_STDARG 1
#   endif
#   ifndef USE_PROTOTYPE
#	define USE_PROTOTYPE 1
#   endif
#   ifndef USE_TCLALLOC
#	define USE_TCLALLOC 1
#   endif
#endif /* __WIN32__ */

#ifndef BUFSIZ
#include <stdio.h>
#endif

#define TCL_VERSION "7.5"
#define TCL_MAJOR_VERSION 7
#define TCL_MINOR_VERSION 5

/*
 * Definitions that allow Tcl functions with variable numbers of
 * arguments to be used with either varargs.h or stdarg.h.  TCL_VARARGS
 * is used in procedure prototypes.  TCL_VARARGS_DEF is used to declare
 * the arguments in a function definiton: it takes the type and name of
 * the first argument and supplies the appropriate argument declaration
 * string for use in the function definition.  TCL_VARARGS_START
 * initializes the va_list data structure and returns the first argument.
 */

#if defined(__STDC__) || defined(HAS_STDARG)
#   define TCL_VARARGS(type, name) (type name, ...)
#   define TCL_VARARGS_DEF(type, name) (type name, ...)
#   define TCL_VARARGS_START(type, name, list) (va_start(list, name), name)
#else
#   ifdef __cplusplus
#	define TCL_VARARGS(type, name) (type name, ...)
#	define TCL_VARARGS_DEF(type, name) (type va_alist, ...)
#   else
#	define TCL_VARARGS(type, name) ()
#	define TCL_VARARGS_DEF(type, name) (va_alist)
#   endif
#   define TCL_VARARGS_START(type, name, list) \
	(va_start(list), va_arg(list, type))
#endif

/*
 * Definitions that allow this header file to be used either with or
 * without ANSI C features like function prototypes.
 */

#undef _ANSI_ARGS_
#undef CONST

#if ((defined(__STDC__) || defined(SABER)) && !defined(NO_PROTOTYPE)) || defined(__cplusplus) || defined(USE_PROTOTYPE)
#   define _USING_PROTOTYPES_ 1
#   define _ANSI_ARGS_(x)	x
#   define CONST const
#else
#   define _ANSI_ARGS_(x)	()
#   define CONST
#endif

#ifdef __cplusplus
#   define EXTERN extern "C"
#else
#   define EXTERN extern
#endif

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.
 */
#ifndef __WIN32__
#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif
#else /* __WIN32__ */
/*
 * The following code is copied from winnt.h
 */
#ifndef VOID
#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif
#endif /* __WIN32__ */

/*
 * Miscellaneous declarations.
 */

#ifndef NULL
#define NULL 0
#endif

#ifndef _CLIENTDATA
#   if defined(__STDC__) || defined(__cplusplus)
    typedef void *ClientData;
#   else
    typedef int *ClientData;
#   endif /* __STDC__ */
#define _CLIENTDATA
#endif

/*
 * Data structures defined opaquely in this module.  The definitions
 * below just provide dummy types.  A few fields are made visible in
 * Tcl_Interp structures, namely those for returning string values.
 * Note:  any change to the Tcl_Interp definition below must be mirrored
 * in the "real" definition in tclInt.h.
 */

typedef struct Tcl_Interp{
    char *result;		/* Points to result string returned by last
				 * command. */
    void (*freeProc) _ANSI_ARGS_((char *blockPtr));
				/* Zero means result is statically allocated.
				 * TCL_DYNAMIC means result was allocated with
				 * ckalloc and should be freed with ckfree.
				 * Other values give address of procedure
				 * to invoke to free the result.  Must be
				 * freed by Tcl_Eval before executing next
				 * command. */
    int errorLine;		/* When TCL_ERROR is returned, this gives
				 * the line number within the command where
				 * the error occurred (1 means first line). */
} Tcl_Interp;

typedef struct Tcl_AsyncHandler_ *Tcl_AsyncHandler;
typedef struct Tcl_Command_ *Tcl_Command;
typedef struct Tcl_Event Tcl_Event;
typedef struct Tcl_File_ *Tcl_File;
typedef struct Tcl_Channel_ *Tcl_Channel;
typedef struct Tcl_RegExp_ *Tcl_RegExp;
typedef struct Tcl_TimerToken_ *Tcl_TimerToken;
typedef struct Tcl_Trace_ *Tcl_Trace;

/*
 * When a TCL command returns, the string pointer interp->result points to
 * a string containing return information from the command.  In addition,
 * the command procedure returns an integer value, which is one of the
 * following:
 *
 * TCL_OK		Command completed normally;  interp->result contains
 *			the command's result.
 * TCL_ERROR		The command couldn't be completed successfully;
 *			interp->result describes what went wrong.
 * TCL_RETURN		The command requests that the current procedure
 *			return;  interp->result contains the procedure's
 *			return value.
 * TCL_BREAK		The command requests that the innermost loop
 *			be exited;  interp->result is meaningless.
 * TCL_CONTINUE		Go on to the next iteration of the current loop;
 *			interp->result is meaningless.
 */

#define TCL_OK		0
#define TCL_ERROR	1
#define TCL_RETURN	2
#define TCL_BREAK	3
#define TCL_CONTINUE	4

#define TCL_RESULT_SIZE 200

/*
 * Argument descriptors for math function callbacks in expressions:
 */

typedef enum {TCL_INT, TCL_DOUBLE, TCL_EITHER} Tcl_ValueType;
typedef struct Tcl_Value {
    Tcl_ValueType type;		/* Indicates intValue or doubleValue is
				 * valid, or both. */
    long intValue;		/* Integer value. */
    double doubleValue;		/* Double-precision floating value. */
} Tcl_Value;

/*
 * Procedure types defined by Tcl:
 */

typedef int (Tcl_AppInitProc) _ANSI_ARGS_((Tcl_Interp *interp));
typedef int (Tcl_AsyncProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int code));
typedef void (Tcl_ChannelProc) _ANSI_ARGS_((ClientData clientData, int mask));
typedef void (Tcl_CloseProc) _ANSI_ARGS_((ClientData data));
typedef void (Tcl_CmdDeleteProc) _ANSI_ARGS_((ClientData clientData));
typedef int (Tcl_CmdProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int argc, char *argv[]));
typedef void (Tcl_CmdTraceProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int level, char *command, Tcl_CmdProc *proc,
	ClientData cmdClientData, int argc, char *argv[]));
typedef int (Tcl_EventProc) _ANSI_ARGS_((Tcl_Event *evPtr, int flags));
typedef void (Tcl_EventCheckProc) _ANSI_ARGS_((ClientData clientData,
	int flags));
typedef int (Tcl_EventDeleteProc) _ANSI_ARGS_((Tcl_Event *evPtr,
        ClientData clientData));
typedef void (Tcl_EventSetupProc) _ANSI_ARGS_((ClientData clientData,
	int flags));
typedef void (Tcl_ExitProc) _ANSI_ARGS_((ClientData clientData));
typedef void (Tcl_FileProc) _ANSI_ARGS_((ClientData clientData, int mask));
typedef void (Tcl_FileFreeProc) _ANSI_ARGS_((ClientData clientData));
typedef void (Tcl_FreeProc) _ANSI_ARGS_((char *blockPtr));
typedef void (Tcl_IdleProc) _ANSI_ARGS_((ClientData clientData));
typedef void (Tcl_InterpDeleteProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp));
typedef int (Tcl_MathProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tcl_Value *args, Tcl_Value *resultPtr));
typedef int (Tcl_PackageInitProc) _ANSI_ARGS_((Tcl_Interp *interp));
typedef void (Tcl_TcpAcceptProc) _ANSI_ARGS_((ClientData callbackData,
        Tcl_Channel chan, char *address, int port));
typedef void (Tcl_TimerProc) _ANSI_ARGS_((ClientData clientData));
typedef char *(Tcl_VarTraceProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *part1, char *part2, int flags));

/*
 * The structure returned by Tcl_GetCmdInfo and passed into
 * Tcl_SetCmdInfo:
 */

typedef struct Tcl_CmdInfo {
    Tcl_CmdProc *proc;			/* Procedure to implement command. */
    ClientData clientData;		/* ClientData passed to proc. */
    Tcl_CmdDeleteProc *deleteProc;	/* Procedure to call when command
					 * is deleted. */
    ClientData deleteData;		/* Value to pass to deleteProc (usually
					 * the same as clientData). */
} Tcl_CmdInfo;

/*
 * The structure defined below is used to hold dynamic strings.  The only
 * field that clients should use is the string field, and they should
 * never modify it.
 */

#define TCL_DSTRING_STATIC_SIZE 200
typedef struct Tcl_DString {
    char *string;		/* Points to beginning of string:  either
				 * staticSpace below or a malloc'ed array. */
    int length;			/* Number of non-NULL characters in the
				 * string. */
    int spaceAvl;		/* Total number of bytes available for the
				 * string and its terminating NULL char. */
    char staticSpace[TCL_DSTRING_STATIC_SIZE];
				/* Space to use in common case where string
				 * is small. */
} Tcl_DString;

#define Tcl_DStringLength(dsPtr) ((dsPtr)->length)
#define Tcl_DStringValue(dsPtr) ((dsPtr)->string)
#define Tcl_DStringTrunc Tcl_DStringSetLength

/*
 * Definitions for the maximum number of digits of precision that may
 * be specified in the "tcl_precision" variable, and the number of
 * characters of buffer space required by Tcl_PrintDouble.
 */

#define TCL_MAX_PREC 17
#define TCL_DOUBLE_SPACE (TCL_MAX_PREC+10)

/*
 * Flag that may be passed to Tcl_ConvertElement to force it not to
 * output braces (careful!  if you change this flag be sure to change
 * the definitions at the front of tclUtil.c).
 */

#define TCL_DONT_USE_BRACES	1

/*
 * Flag values passed to Tcl_RecordAndEval.
 * WARNING: these bit choices must not conflict with the bit choices
 * for evalFlag bits in tclInt.h!!
 */

#define TCL_NO_EVAL		0x10000
#define TCL_EVAL_GLOBAL		0x20000

/*
 * Special freeProc values that may be passed to Tcl_SetResult (see
 * the man page for details):
 */

#define TCL_VOLATILE	((Tcl_FreeProc *) 1)
#define TCL_STATIC	((Tcl_FreeProc *) 0)
#define TCL_DYNAMIC	((Tcl_FreeProc *) 3)

/*
 * Flag values passed to variable-related procedures.
 */

#define TCL_GLOBAL_ONLY		1
#define TCL_APPEND_VALUE	2
#define TCL_LIST_ELEMENT	4
#define TCL_TRACE_READS		0x10
#define TCL_TRACE_WRITES	0x20
#define TCL_TRACE_UNSETS	0x40
#define TCL_TRACE_DESTROYED	0x80
#define TCL_INTERP_DESTROYED	0x100
#define TCL_LEAVE_ERR_MSG	0x200

/*
 * Types for linked variables:
 */

#define TCL_LINK_INT		1
#define TCL_LINK_DOUBLE		2
#define TCL_LINK_BOOLEAN	3
#define TCL_LINK_STRING		4
#define TCL_LINK_READ_ONLY	0x80

/*
 * The following declarations either map ckalloc and ckfree to
 * malloc and free, or they map them to procedures with all sorts
 * of debugging hooks defined in tclCkalloc.c.
 */

EXTERN char *		Tcl_Alloc _ANSI_ARGS_((unsigned int size));
EXTERN void		Tcl_Free _ANSI_ARGS_((char *ptr));
EXTERN char *		Tcl_Realloc _ANSI_ARGS_((char *ptr,
			    unsigned int size));

#ifdef TCL_MEM_DEBUG

#  define Tcl_Alloc(x) Tcl_DbCkalloc(x, __FILE__, __LINE__)
#  define Tcl_Free(x)  Tcl_DbCkfree(x, __FILE__, __LINE__)
#  define Tcl_Realloc(x,y) Tcl_DbCkrealloc((x), (y),__FILE__, __LINE__)
#  define ckalloc(x) Tcl_DbCkalloc(x, __FILE__, __LINE__)
#  define ckfree(x)  Tcl_DbCkfree(x, __FILE__, __LINE__)
#  define ckrealloc(x,y) Tcl_DbCkrealloc((x), (y),__FILE__, __LINE__)

EXTERN int		Tcl_DumpActiveMemory _ANSI_ARGS_((char *fileName));
EXTERN void		Tcl_ValidateAllMemory _ANSI_ARGS_((char *file,
			    int line));

#else

#  if USE_TCLALLOC
#     define ckalloc(x) Tcl_Alloc(x)
#     define ckfree(x) Tcl_Free(x)
#     define ckrealloc(x,y) Tcl_Realloc(x,y)
#  else
#     define ckalloc(x) malloc(x)
#     define ckfree(x)  free(x)
#     define ckrealloc(x,y) realloc(x,y)
#  endif
#  define Tcl_DumpActiveMemory(x)
#  define Tcl_ValidateAllMemory(x,y)

#endif /* TCL_MEM_DEBUG */

/*
 * Macro to free result of interpreter.
 */

#define Tcl_FreeResult(interp)					\
    if ((interp)->freeProc != 0) {				\
	if (((interp)->freeProc == TCL_DYNAMIC)			\
	    || ((interp)->freeProc == (Tcl_FreeProc *) free)) {	\
	    ckfree((interp)->result);				\
	} else {						\
	    (*(interp)->freeProc)((interp)->result);		\
	}							\
	(interp)->freeProc = 0;					\
    }

/*
 * Forward declaration of Tcl_HashTable.  Needed by some C++ compilers
 * to prevent errors when the forward reference to Tcl_HashTable is
 * encountered in the Tcl_HashEntry structure.
 */

#ifdef __cplusplus
struct Tcl_HashTable;
#endif

/*
 * Structure definition for an entry in a hash table.  No-one outside
 * Tcl should access any of these fields directly;  use the macros
 * defined below.
 */

typedef struct Tcl_HashEntry {
    struct Tcl_HashEntry *nextPtr;	/* Pointer to next entry in this
					 * hash bucket, or NULL for end of
					 * chain. */
    struct Tcl_HashTable *tablePtr;	/* Pointer to table containing entry. */
    struct Tcl_HashEntry **bucketPtr;	/* Pointer to bucket that points to
					 * first entry in this entry's chain:
					 * used for deleting the entry. */
    ClientData clientData;		/* Application stores something here
					 * with Tcl_SetHashValue. */
    union {				/* Key has one of these forms: */
	char *oneWordValue;		/* One-word value for key. */
	int words[1];			/* Multiple integer words for key.
					 * The actual size will be as large
					 * as necessary for this table's
					 * keys. */
	char string[4];			/* String for key.  The actual size
					 * will be as large as needed to hold
					 * the key. */
    } key;				/* MUST BE LAST FIELD IN RECORD!! */
} Tcl_HashEntry;

/*
 * Structure definition for a hash table.  Must be in tcl.h so clients
 * can allocate space for these structures, but clients should never
 * access any fields in this structure.
 */

#define TCL_SMALL_HASH_TABLE 4
typedef struct Tcl_HashTable {
    Tcl_HashEntry **buckets;		/* Pointer to bucket array.  Each
					 * element points to first entry in
					 * bucket's hash chain, or NULL. */
    Tcl_HashEntry *staticBuckets[TCL_SMALL_HASH_TABLE];
					/* Bucket array used for small tables
					 * (to avoid mallocs and frees). */
    int numBuckets;			/* Total number of buckets allocated
					 * at **bucketPtr. */
    int numEntries;			/* Total number of entries present
					 * in table. */
    int rebuildSize;			/* Enlarge table when numEntries gets
					 * to be this large. */
    int downShift;			/* Shift count used in hashing
					 * function.  Designed to use high-
					 * order bits of randomized keys. */
    int mask;				/* Mask value used in hashing
					 * function. */
    int keyType;			/* Type of keys used in this table. 
					 * It's either TCL_STRING_KEYS,
					 * TCL_ONE_WORD_KEYS, or an integer
					 * giving the number of ints that
                                         * is the size of the key.
					 */
    Tcl_HashEntry *(*findProc) _ANSI_ARGS_((struct Tcl_HashTable *tablePtr,
	    char *key));
    Tcl_HashEntry *(*createProc) _ANSI_ARGS_((struct Tcl_HashTable *tablePtr,
	    char *key, int *newPtr));
} Tcl_HashTable;

/*
 * Structure definition for information used to keep track of searches
 * through hash tables:
 */

typedef struct Tcl_HashSearch {
    Tcl_HashTable *tablePtr;		/* Table being searched. */
    int nextIndex;			/* Index of next bucket to be
					 * enumerated after present one. */
    Tcl_HashEntry *nextEntryPtr;	/* Next entry to be enumerated in the
					 * the current bucket. */
} Tcl_HashSearch;

/*
 * Acceptable key types for hash tables:
 */

#define TCL_STRING_KEYS		0
#define TCL_ONE_WORD_KEYS	1

/*
 * Macros for clients to use to access fields of hash entries:
 */

#define Tcl_GetHashValue(h) ((h)->clientData)
#define Tcl_SetHashValue(h, value) ((h)->clientData = (ClientData) (value))
#define Tcl_GetHashKey(tablePtr, h) \
    ((char *) (((tablePtr)->keyType == TCL_ONE_WORD_KEYS) ? (h)->key.oneWordValue \
						: (h)->key.string))

/*
 * Macros to use for clients to use to invoke find and create procedures
 * for hash tables:
 */

#define Tcl_FindHashEntry(tablePtr, key) \
	(*((tablePtr)->findProc))(tablePtr, key)
#define Tcl_CreateHashEntry(tablePtr, key, newPtr) \
	(*((tablePtr)->createProc))(tablePtr, key, newPtr)

/*
 * Flag values to pass to Tcl_DoOneEvent to disable searches
 * for some kinds of events:
 */

#define TCL_DONT_WAIT		(1<<1)
#define TCL_WINDOW_EVENTS	(1<<2)
#define TCL_FILE_EVENTS		(1<<3)
#define TCL_TIMER_EVENTS	(1<<4)
#define TCL_IDLE_EVENTS		(1<<5)	/* WAS 0x10 ???? */
#define TCL_ALL_EVENTS		(~TCL_DONT_WAIT)

/*
 * The following structure defines a generic event for the Tcl event
 * system.  These are the things that are queued in calls to Tcl_QueueEvent
 * and serviced later by Tcl_DoOneEvent.  There can be many different
 * kinds of events with different fields, corresponding to window events,
 * timer events, etc.  The structure for a particular event consists of
 * a Tcl_Event header followed by additional information specific to that
 * event.
 */

struct Tcl_Event {
    Tcl_EventProc *proc;	/* Procedure to call to service this event. */
    struct Tcl_Event *nextPtr;	/* Next in list of pending events, or NULL. */
};

/*
 * Positions to pass to Tk_QueueEvent:
 */

typedef enum {
    TCL_QUEUE_TAIL, TCL_QUEUE_HEAD, TCL_QUEUE_MARK
} Tcl_QueuePosition;

/*
 * The following structure keeps is used to hold a time value, either as
 * an absolute time (the number of seconds from the epoch) or as an
 * elapsed time. On Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 * On Macintosh systems the epoch is Midnight Jan 1, 1904 GMT.
 */

typedef struct Tcl_Time {
    long sec;			/* Seconds. */
    long usec;			/* Microseconds. */
} Tcl_Time;

/*
 * Bits to pass to Tcl_CreateFileHandler and Tcl_CreateChannelHandler
 * to indicate what sorts of events are of interest:
 */

#define TCL_READABLE	(1<<1)
#define TCL_WRITABLE	(1<<2)
#define TCL_EXCEPTION	(1<<3)

/*
 * Flag values to pass to Tcl_OpenCommandChannel to indicate the
 * disposition of the stdio handles.  TCL_STDIN, TCL_STDOUT, TCL_STDERR,
 * are also used in Tcl_GetStdChannel.
 */

#define TCL_STDIN		(1<<1)	
#define TCL_STDOUT		(1<<2)
#define TCL_STDERR		(1<<3)
#define TCL_ENFORCE_MODE	(1<<4)

/*
 * Typedefs for the various operations in a channel type:
 */

typedef int	(Tcl_DriverBlockModeProc) _ANSI_ARGS_((ClientData instanceData,
		    Tcl_File inFile, Tcl_File outFile, int mode));
typedef int	(Tcl_DriverCloseProc) _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp, Tcl_File inFile, Tcl_File outFile));
typedef int	(Tcl_DriverInputProc) _ANSI_ARGS_((ClientData instanceData,
		    Tcl_File inFile, char *buf, int toRead,
		    int *errorCodePtr));
typedef int	(Tcl_DriverOutputProc) _ANSI_ARGS_((ClientData instanceData,
	            Tcl_File outFile, char *buf, int toWrite,
        	    int *errorCodePtr));
typedef int	(Tcl_DriverSeekProc) _ANSI_ARGS_((ClientData instanceData,
		    Tcl_File inFile, Tcl_File outFile, long offset, int mode,
		    int *errorCodePtr));
typedef int	(Tcl_DriverSetOptionProc) _ANSI_ARGS_((
		    ClientData instanceData, Tcl_Interp *interp,
                    char *optionName, char *value));
typedef int	(Tcl_DriverGetOptionProc) _ANSI_ARGS_((
		    ClientData instanceData, char *optionName,
                    Tcl_DString *dsPtr));

/*
 * Enum for different end of line translation and recognition modes.
 */

typedef enum Tcl_EolTranslation {
    TCL_TRANSLATE_AUTO,			/* Eol == \r, \n and \r\n. */
    TCL_TRANSLATE_CR,			/* Eol == \r. */
    TCL_TRANSLATE_LF,			/* Eol == \n. */
    TCL_TRANSLATE_CRLF			/* Eol == \r\n. */
} Tcl_EolTranslation;

/*
 * struct Tcl_ChannelType:
 *
 * One such structure exists for each type (kind) of channel.
 * It collects together in one place all the functions that are
 * part of the specific channel type.
 */

typedef struct Tcl_ChannelType {
    char *typeName;			/* The name of the channel type in Tcl
                                         * commands. This storage is owned by
                                         * channel type. */
    Tcl_DriverBlockModeProc *blockModeProc;
    					/* Set blocking mode for the
                                         * raw channel. May be NULL. */
    Tcl_DriverCloseProc *closeProc;	/* Procedure to call to close
                                         * the channel. */
    Tcl_DriverInputProc *inputProc;	/* Procedure to call for input
                                         * on channel. */
    Tcl_DriverOutputProc *outputProc;	/* Procedure to call for output
                                         * on channel. */
    Tcl_DriverSeekProc *seekProc;	/* Procedure to call to seek
                                         * on the channel. May be NULL. */
    Tcl_DriverSetOptionProc *setOptionProc;
    					/* Set an option on a channel. */
    Tcl_DriverGetOptionProc *getOptionProc;
    					/* Get an option from a channel. */
} Tcl_ChannelType;

/*
 * The following flags determine whether the blockModeProc above should
 * set the channel into blocking or nonblocking mode. They are passed
 * as arguments to the blockModeProc procedure in the above structure.
 */

#define TCL_MODE_BLOCKING 0		/* Put channel into blocking mode. */
#define TCL_MODE_NONBLOCKING 1		/* Put channel into nonblocking
					 * mode. */

/*
 * Types for file handles:
 */

#define TCL_UNIX_FD	1
#define TCL_MAC_FILE	2
#define TCL_MAC_SOCKET	3
#define TCL_WIN_PIPE	4
#define TCL_WIN_FILE	5
#define TCL_WIN_SOCKET	6
#define TCL_WIN_CONSOLE 7

/*
 * Enum for different types of file paths.
 */

typedef enum Tcl_PathType {
    TCL_PATH_ABSOLUTE,
    TCL_PATH_RELATIVE,
    TCL_PATH_VOLUME_RELATIVE
} Tcl_PathType;

/*
 * The following interface is exported for backwards compatibility, but
 * is only implemented on Unix.  Portable applications should use
 * Tcl_OpenCommandChannel, instead.
 */

EXTERN int		Tcl_CreatePipeline _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, int **pidArrayPtr,
			    int *inPipePtr, int *outPipePtr,
			    int *errFilePtr));

/*
 * Exported Tcl procedures:
 */

EXTERN void		Tcl_AddErrorInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *message));
EXTERN void		Tcl_AllowExceptions _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_AppendElement _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN void		Tcl_AppendResult _ANSI_ARGS_(
    			    TCL_VARARGS(Tcl_Interp *,interp));
EXTERN int		Tcl_AppInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tcl_AsyncHandler	Tcl_AsyncCreate _ANSI_ARGS_((Tcl_AsyncProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_AsyncDelete _ANSI_ARGS_((Tcl_AsyncHandler async));
EXTERN int		Tcl_AsyncInvoke _ANSI_ARGS_((Tcl_Interp *interp,
			    int code));
EXTERN void		Tcl_AsyncMark _ANSI_ARGS_((Tcl_AsyncHandler async));
EXTERN int		Tcl_AsyncReady _ANSI_ARGS_((void));
EXTERN void		Tcl_BackgroundError _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN char		Tcl_Backslash _ANSI_ARGS_((char *src,
			    int *readPtr));
EXTERN void		Tcl_CallWhenDeleted _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_InterpDeleteProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_CancelIdleCall _ANSI_ARGS_((Tcl_IdleProc *idleProc,
			    ClientData clientData));
#define Tcl_Ckalloc Tcl_Alloc
#define Tcl_Ckfree Tcl_Free
#define Tcl_Ckrealloc Tcl_Realloc
EXTERN int		Tcl_Close _ANSI_ARGS_((Tcl_Interp *interp,
        		    Tcl_Channel chan));
EXTERN int		Tcl_CommandComplete _ANSI_ARGS_((char *cmd));
EXTERN char *		Tcl_Concat _ANSI_ARGS_((int argc, char **argv));
EXTERN int		Tcl_ConvertElement _ANSI_ARGS_((char *src,
			    char *dst, int flags));
EXTERN int		Tcl_CreateAlias _ANSI_ARGS_((Tcl_Interp *slave,
			    char *slaveCmd, Tcl_Interp *target,
        		    char *targetCmd, int argc, char **argv));
EXTERN Tcl_Channel	Tcl_CreateChannel _ANSI_ARGS_((
    			    Tcl_ChannelType *typePtr, char *chanName,
                            Tcl_File inFile, Tcl_File outFile,
                            ClientData instanceData));
EXTERN void		Tcl_CreateChannelHandler _ANSI_ARGS_((
			    Tcl_Channel chan, int mask,
                            Tcl_ChannelProc *proc, ClientData clientData));
EXTERN void		Tcl_CreateCloseHandler _ANSI_ARGS_((
			    Tcl_Channel chan, Tcl_CloseProc *proc,
                            ClientData clientData));
EXTERN Tcl_Command	Tcl_CreateCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdProc *proc,
			    ClientData clientData,
			    Tcl_CmdDeleteProc *deleteProc));
EXTERN void		Tcl_CreateEventSource _ANSI_ARGS_((
			    Tcl_EventSetupProc *setupProc, Tcl_EventCheckProc
			    *checkProc, ClientData clientData));
EXTERN void		Tcl_CreateExitHandler _ANSI_ARGS_((Tcl_ExitProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_CreateFileHandler _ANSI_ARGS_((
    			    Tcl_File file, int mask, Tcl_FileProc *proc,
			    ClientData clientData));
EXTERN Tcl_Interp *	Tcl_CreateInterp _ANSI_ARGS_((void));
EXTERN void		Tcl_CreateMathFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int numArgs, Tcl_ValueType *argTypes,
			    Tcl_MathProc *proc, ClientData clientData));
EXTERN void		Tcl_CreateModalTimeout _ANSI_ARGS_((int milliseconds,
			    Tcl_TimerProc *proc, ClientData clientData));
EXTERN Tcl_Interp	*Tcl_CreateSlave _ANSI_ARGS_((Tcl_Interp *interp,
		            char *slaveName, int isSafe));
EXTERN Tcl_TimerToken	Tcl_CreateTimerHandler _ANSI_ARGS_((int milliseconds,
			    Tcl_TimerProc *proc, ClientData clientData));
EXTERN Tcl_Trace	Tcl_CreateTrace _ANSI_ARGS_((Tcl_Interp *interp,
			    int level, Tcl_CmdTraceProc *proc,
			    ClientData clientData));
EXTERN char *		Tcl_DbCkalloc _ANSI_ARGS_((unsigned int size,
			    char *file, int line));
EXTERN int		Tcl_DbCkfree _ANSI_ARGS_((char *ptr,
			    char *file, int line));
EXTERN char *		Tcl_DbCkrealloc _ANSI_ARGS_((char *ptr,
			    unsigned int size, char *file, int line));
EXTERN void		Tcl_DeleteAssocData _ANSI_ARGS_((Tcl_Interp *interp,
                            char *name));
EXTERN int		Tcl_DeleteCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName));
EXTERN void		Tcl_DeleteChannelHandler _ANSI_ARGS_((
    			    Tcl_Channel chan, Tcl_ChannelProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_DeleteCloseHandler _ANSI_ARGS_((
			    Tcl_Channel chan, Tcl_CloseProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_DeleteEventSource _ANSI_ARGS_((
			    Tcl_EventSetupProc *setupProc,
			    Tcl_EventCheckProc *checkProc,
			    ClientData clientData));
EXTERN void		Tcl_DeleteEvents _ANSI_ARGS_((
			    Tcl_EventDeleteProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_DeleteExitHandler _ANSI_ARGS_((Tcl_ExitProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_DeleteFileHandler _ANSI_ARGS_((
    			    Tcl_File file));
EXTERN void		Tcl_DeleteHashEntry _ANSI_ARGS_((
			    Tcl_HashEntry *entryPtr));
EXTERN void		Tcl_DeleteHashTable _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr));
EXTERN void		Tcl_DeleteInterp _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_DeleteModalTimeout _ANSI_ARGS_((
			    Tcl_TimerProc *proc, ClientData clientData));
EXTERN void		Tcl_DeleteTimerHandler _ANSI_ARGS_((
			    Tcl_TimerToken token));
EXTERN void		Tcl_DeleteTrace _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Trace trace));
EXTERN void		Tcl_DetachPids _ANSI_ARGS_((int numPids, int *pidPtr));
EXTERN void		Tcl_DontCallWhenDeleted _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_InterpDeleteProc *proc,
			    ClientData clientData));
EXTERN int		Tcl_DoOneEvent _ANSI_ARGS_((int flags));
EXTERN void		Tcl_DoWhenIdle _ANSI_ARGS_((Tcl_IdleProc *proc,
			    ClientData clientData));
EXTERN char *		Tcl_DStringAppend _ANSI_ARGS_((Tcl_DString *dsPtr,
			    char *string, int length));
EXTERN char *		Tcl_DStringAppendElement _ANSI_ARGS_((
			    Tcl_DString *dsPtr, char *string));
EXTERN void		Tcl_DStringEndSublist _ANSI_ARGS_((Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringFree _ANSI_ARGS_((Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringGetResult _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringInit _ANSI_ARGS_((Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringResult _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_DString *dsPtr));
EXTERN void		Tcl_DStringSetLength _ANSI_ARGS_((Tcl_DString *dsPtr,
			    int length));
EXTERN void		Tcl_DStringStartSublist _ANSI_ARGS_((
			    Tcl_DString *dsPtr));
EXTERN int		Tcl_Eof _ANSI_ARGS_((Tcl_Channel chan));
EXTERN char *		Tcl_ErrnoId _ANSI_ARGS_((void));
EXTERN char *		Tcl_ErrnoMsg _ANSI_ARGS_((int err));
EXTERN int		Tcl_Eval _ANSI_ARGS_((Tcl_Interp *interp, char *cmd));
EXTERN int		Tcl_EvalFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName));
EXTERN void		Tcl_EventuallyFree _ANSI_ARGS_((ClientData clientData,
			    Tcl_FreeProc *freeProc));
EXTERN void		Tcl_Exit _ANSI_ARGS_((int status));
EXTERN int		Tcl_ExprBoolean _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *ptr));
EXTERN int		Tcl_ExprDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *ptr));
EXTERN int		Tcl_ExprLong _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, long *ptr));
EXTERN int		Tcl_ExprString _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN int		Tcl_FileReady _ANSI_ARGS_((Tcl_File file,
			    int mask));
EXTERN void		Tcl_FindExecutable _ANSI_ARGS_((char *argv0));
EXTERN Tcl_HashEntry *	Tcl_FirstHashEntry _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr,
			    Tcl_HashSearch *searchPtr));
EXTERN int		Tcl_Flush _ANSI_ARGS_((Tcl_Channel chan));
EXTERN void 		Tcl_FreeFile _ANSI_ARGS_((
    			    Tcl_File file));
EXTERN int		Tcl_GetAlias _ANSI_ARGS_((Tcl_Interp *interp,
       			    char *slaveCmd, Tcl_Interp **targetInterpPtr,
                            char **targetCmdPtr, int *argcPtr,
			    char ***argvPtr));
EXTERN ClientData	Tcl_GetAssocData _ANSI_ARGS_((Tcl_Interp *interp,
                            char *name, Tcl_InterpDeleteProc **procPtr));
EXTERN int		Tcl_GetBoolean _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *boolPtr));
EXTERN Tcl_Channel	Tcl_GetChannel _ANSI_ARGS_((Tcl_Interp *interp,
	        	    char *chanName, int *modePtr));
EXTERN int		Tcl_GetChannelBufferSize _ANSI_ARGS_((
    			    Tcl_Channel chan));
EXTERN Tcl_File		Tcl_GetChannelFile _ANSI_ARGS_((Tcl_Channel chan,
	        	    int direction));
EXTERN ClientData	Tcl_GetChannelInstanceData _ANSI_ARGS_((
    			    Tcl_Channel chan));
EXTERN int		Tcl_GetChannelOption _ANSI_ARGS_((Tcl_Channel chan,
		            char *optionName, Tcl_DString *dsPtr));
EXTERN char *		Tcl_GetChannelName _ANSI_ARGS_((Tcl_Channel chan));
EXTERN Tcl_ChannelType * Tcl_GetChannelType _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_GetCommandInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdInfo *infoPtr));
EXTERN char *		Tcl_GetCommandName _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Command command));
EXTERN char *		Tcl_GetCwd _ANSI_ARGS_((char *buf, int len));
EXTERN int		Tcl_GetDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *doublePtr));
EXTERN int		Tcl_GetErrno _ANSI_ARGS_((void));
EXTERN Tcl_File		Tcl_GetFile _ANSI_ARGS_((ClientData fileData,
			    int type));
EXTERN ClientData	Tcl_GetFileInfo _ANSI_ARGS_((Tcl_File file,
			    int *typePtr));
EXTERN char *		Tcl_GetHostName _ANSI_ARGS_((void));
EXTERN int		Tcl_GetInt _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *intPtr));
EXTERN int		Tcl_GetInterpPath _ANSI_ARGS_((Tcl_Interp *askInterp,
			    Tcl_Interp *slaveInterp));
EXTERN Tcl_Interp	*Tcl_GetMaster _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN ClientData	Tcl_GetNotifierData _ANSI_ARGS_((Tcl_File file,
			    Tcl_FileFreeProc **freeProcPtr));
EXTERN int		Tcl_GetOpenFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int write, int checkUsage,
			    ClientData *filePtr));
EXTERN Tcl_PathType	Tcl_GetPathType _ANSI_ARGS_((char *path));
EXTERN int		Tcl_Gets _ANSI_ARGS_((Tcl_Channel chan,
        		    Tcl_DString *dsPtr));
EXTERN Tcl_Interp	*Tcl_GetSlave _ANSI_ARGS_((Tcl_Interp *interp,
			    char *slaveName));
EXTERN Tcl_Channel	Tcl_GetStdChannel _ANSI_ARGS_((int type));
EXTERN char *		Tcl_GetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags));
EXTERN char *		Tcl_GetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags));
EXTERN int		Tcl_GlobalEval _ANSI_ARGS_((Tcl_Interp *interp,
			    char *command));
EXTERN char *		Tcl_HashStats _ANSI_ARGS_((Tcl_HashTable *tablePtr));
EXTERN int		Tcl_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_InitHashTable _ANSI_ARGS_((Tcl_HashTable *tablePtr,
			    int keyType));
EXTERN void		Tcl_InitMemory _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_InputBlocked _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_InputBuffered _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_InterpDeleted _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_IsSafe _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN char *		Tcl_JoinPath _ANSI_ARGS_((int argc, char **argv,
			    Tcl_DString *resultPtr));
EXTERN int		Tcl_LinkVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, char *addr, int type));
EXTERN void		Tcl_Main _ANSI_ARGS_((int argc, char **argv,
			    Tcl_AppInitProc *appInitProc));
EXTERN Tcl_Channel	Tcl_MakeFileChannel _ANSI_ARGS_((ClientData inFile,
		            ClientData outFile, int mode));
EXTERN int		Tcl_MakeSafe _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tcl_Channel	Tcl_MakeTcpClientChannel _ANSI_ARGS_((
    			    ClientData tcpSocket));
EXTERN char *		Tcl_Merge _ANSI_ARGS_((int argc, char **argv));
EXTERN Tcl_HashEntry *	Tcl_NextHashEntry _ANSI_ARGS_((
			    Tcl_HashSearch *searchPtr));
EXTERN Tcl_Channel	Tcl_OpenCommandChannel _ANSI_ARGS_((
    			    Tcl_Interp *interp, int argc, char **argv,
			    int flags));
EXTERN Tcl_Channel	Tcl_OpenFileChannel _ANSI_ARGS_((Tcl_Interp *interp,
        		    char *fileName, char *modeString,
                            int permissions));
EXTERN Tcl_Channel	Tcl_OpenTcpClient _ANSI_ARGS_((Tcl_Interp *interp,
			    int port, char *address, char *myaddr,
		            int myport, int async));
EXTERN Tcl_Channel	Tcl_OpenTcpServer _ANSI_ARGS_((Tcl_Interp *interp,
		            int port, char *host,
        		    Tcl_TcpAcceptProc *acceptProc,
			    ClientData callbackData));
EXTERN char *		Tcl_ParseVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char **termPtr));
EXTERN int		Tcl_PkgProvide _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, char *version));
EXTERN char *		Tcl_PkgRequire _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, char *version, int exact));
EXTERN char *		Tcl_PosixError _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_Preserve _ANSI_ARGS_((ClientData data));
EXTERN void		Tcl_PrintDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    double value, char *dst));
EXTERN int		Tcl_PutEnv _ANSI_ARGS_((CONST char *string));
EXTERN void		Tcl_QueueEvent _ANSI_ARGS_((Tcl_Event *evPtr,
			    Tcl_QueuePosition position));
EXTERN int		Tcl_Read _ANSI_ARGS_((Tcl_Channel chan,
	        	    char *bufPtr, int toRead));
EXTERN void		Tcl_ReapDetachedProcs _ANSI_ARGS_((void));
EXTERN int		Tcl_RecordAndEval _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmd, int flags));
EXTERN Tcl_RegExp	Tcl_RegExpCompile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN int		Tcl_RegExpExec _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_RegExp regexp, char *string, char *start));
EXTERN int		Tcl_RegExpMatch _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *pattern));
EXTERN void		Tcl_RegExpRange _ANSI_ARGS_((Tcl_RegExp regexp,
			    int index, char **startPtr, char **endPtr));
EXTERN void		Tcl_RegisterChannel _ANSI_ARGS_((Tcl_Interp *interp,
	        	    Tcl_Channel chan));
EXTERN void		Tcl_Release _ANSI_ARGS_((ClientData clientData));
EXTERN void		Tcl_ResetResult _ANSI_ARGS_((Tcl_Interp *interp));
#define Tcl_Return Tcl_SetResult
EXTERN int		Tcl_ScanElement _ANSI_ARGS_((char *string,
			    int *flagPtr));
EXTERN int		Tcl_Seek _ANSI_ARGS_((Tcl_Channel chan,
        		    int offset, int mode));
EXTERN void		Tcl_SetAssocData _ANSI_ARGS_((Tcl_Interp *interp,
                            char *name, Tcl_InterpDeleteProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_SetChannelBufferSize _ANSI_ARGS_((
			    Tcl_Channel chan, int sz));
EXTERN int		Tcl_SetChannelOption _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Channel chan,
	        	    char *optionName, char *newValue));
EXTERN int		Tcl_SetCommandInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdInfo *infoPtr));
EXTERN void		Tcl_SetErrno _ANSI_ARGS_((int errno));
EXTERN void		Tcl_SetErrorCode _ANSI_ARGS_(
    			    TCL_VARARGS(Tcl_Interp *,interp));
EXTERN void		Tcl_SetMaxBlockTime _ANSI_ARGS_((Tcl_Time *timePtr));
EXTERN void		Tcl_SetNotifierData _ANSI_ARGS_((Tcl_File file,
			    Tcl_FileFreeProc *freeProcPtr, ClientData data));
EXTERN void		Tcl_SetPanicProc _ANSI_ARGS_((void (*proc)
			    _ANSI_ARGS_(TCL_VARARGS(char *, format))));
EXTERN int		Tcl_SetRecursionLimit _ANSI_ARGS_((Tcl_Interp *interp,
			    int depth));
EXTERN void		Tcl_SetResult _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Tcl_FreeProc *freeProc));
EXTERN void		Tcl_SetStdChannel _ANSI_ARGS_((Tcl_Channel channel,
			    int type));
EXTERN char *		Tcl_SetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, char *newValue, int flags));
EXTERN char *		Tcl_SetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, char *newValue,
			    int flags));
EXTERN char *		Tcl_SignalId _ANSI_ARGS_((int sig));
EXTERN char *		Tcl_SignalMsg _ANSI_ARGS_((int sig));
EXTERN void		Tcl_Sleep _ANSI_ARGS_((int ms));
EXTERN int		Tcl_SplitList _ANSI_ARGS_((Tcl_Interp *interp,
			    char *list, int *argcPtr, char ***argvPtr));
EXTERN void		Tcl_SplitPath _ANSI_ARGS_((char *path,
			    int *argcPtr, char ***argvPtr));
EXTERN void		Tcl_StaticPackage _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pkgName, Tcl_PackageInitProc *initProc,
			    Tcl_PackageInitProc *safeInitProc));
EXTERN int		Tcl_StringMatch _ANSI_ARGS_((char *string,
			    char *pattern));
EXTERN int		Tcl_Tell _ANSI_ARGS_((Tcl_Channel chan));
#define Tcl_TildeSubst Tcl_TranslateFileName
EXTERN int		Tcl_TraceVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags, Tcl_VarTraceProc *proc,
			    ClientData clientData));
EXTERN int		Tcl_TraceVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags,
			    Tcl_VarTraceProc *proc, ClientData clientData));
EXTERN char *		Tcl_TranslateFileName _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_DString *bufferPtr));
EXTERN void		Tcl_UnlinkVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName));
EXTERN int		Tcl_UnregisterChannel _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Channel chan));
EXTERN int		Tcl_UnsetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags));
EXTERN int		Tcl_UnsetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags));
EXTERN void		Tcl_UntraceVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags, Tcl_VarTraceProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_UntraceVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags,
			    Tcl_VarTraceProc *proc, ClientData clientData));
EXTERN void		Tcl_UpdateLinkedVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName));
EXTERN int		Tcl_UpVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *frameName, char *varName,
			    char *localName, int flags));
EXTERN int		Tcl_UpVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *frameName, char *part1, char *part2,
			    char *localName, int flags));
EXTERN int		Tcl_VarEval _ANSI_ARGS_(
    			    TCL_VARARGS(Tcl_Interp *,interp));
EXTERN ClientData	Tcl_VarTraceInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags,
			    Tcl_VarTraceProc *procPtr,
			    ClientData prevClientData));
EXTERN ClientData	Tcl_VarTraceInfo2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags,
			    Tcl_VarTraceProc *procPtr,
			    ClientData prevClientData));
EXTERN int		Tcl_WaitForEvent _ANSI_ARGS_((Tcl_Time *timePtr));
EXTERN int		Tcl_WaitPid _ANSI_ARGS_((int pid, int *statPtr,
                            int options));
EXTERN void		Tcl_WatchFile _ANSI_ARGS_((Tcl_File file,
			    int mask));
EXTERN int		Tcl_Write _ANSI_ARGS_((Tcl_Channel chan,
        		    char *s, int slen));

#endif /* _TCL */
