/*
 * tcl.h --
 *
 *	This header file describes the externally-visible facilities
 *	of the Tcl interpreter.
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1993-1996 Lucent Technologies.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tcl.h 1.318 97/06/26 13:43:02
 */

#ifndef _TCL
#define _TCL

/*
 * When version numbers change here, must also go into the following files
 * and update the version numbers:
 *
 * library/init.tcl
 * unix/configure.in
 * unix/pkginfo
 * win/makefile.bc
 * win/makefile.vc
 *
 * The release level should be  0 for alpha, 1 for beta, and 2 for
 * final/patch.  The release serial value is the number that follows the
 * "a", "b", or "p" in the patch level; for example, if the patch level
 * is 7.6b2, TCL_RELEASE_SERIAL is 2.  It restarts at 1 whenever the
 * release level is changed, except for the final release which is 0
 * (the first patch will start at 1).
 */

#define TCL_MAJOR_VERSION   8
#define TCL_MINOR_VERSION   0
#define TCL_RELEASE_LEVEL   1
#define TCL_RELEASE_SERIAL  2

#define TCL_VERSION	    "8.0"
#define TCL_PATCH_LEVEL	    "8.0b2"

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
#   ifndef STRICT
#	define STRICT
#   endif
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
#   ifndef STRINGIFY
#	define STRINGIFY(x)	    STRINGIFY1(x)
#	define STRINGIFY1(x)	    #x
#   endif
#endif /* __WIN32__ */

/*
 * The following definitions set up the proper options for Macintosh
 * compilers.  We use this method because there is no autoconf equivalent.
 */

#ifdef MAC_TCL
#   ifndef HAS_STDARG
#	define HAS_STDARG 1
#   endif
#   ifndef USE_TCLALLOC
#	define USE_TCLALLOC 1
#   endif
#   ifndef NO_STRERROR
#	define NO_STRERROR 1
#   endif
#endif

/* 
 * A special definition used to allow this header file to be included 
 * in resource files so that they can get obtain version information from
 * this file.  Resource compilers don't like all the C stuff, like typedefs
 * and procedure declarations, that occur below.
 */

#ifndef RESOURCE_INCLUDED

#ifndef BUFSIZ
#include <stdio.h>
#endif

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
 * Data structures defined opaquely in this module. The definitions below
 * just provide dummy types. A few fields are made visible in Tcl_Interp
 * structures, namely those used for returning a string result from
 * commands. Direct access to the result field is discouraged in Tcl 8.0.
 * The interpreter result is either an object or a string, and the two
 * values are kept consistent unless some C code sets interp->result
 * directly. Programmers should use either the procedure Tcl_GetObjResult()
 * or Tcl_GetStringResult() to read the interpreter's result. See the
 * SetResult man page for details.
 * 
 * Note: any change to the Tcl_Interp definition below must be mirrored
 * in the "real" definition in tclInt.h.
 *
 * Note: Tcl_ObjCmdProc procedures do not directly set result and freeProc.
 * Instead, they set a Tcl_Obj member in the "real" structure that can be
 * accessed with Tcl_GetObjResult() and Tcl_SetObjResult().
 */

typedef struct Tcl_Interp {
    char *result;		/* If the last command returned a string
				 * result, this points to it. */
    void (*freeProc) _ANSI_ARGS_((char *blockPtr));
				/* Zero means the string result is
				 * statically allocated. TCL_DYNAMIC means
				 * it was allocated with ckalloc and should
				 * be freed with ckfree. Other values give
				 * the address of procedure to invoke to
				 * free the result. Tcl_Eval must free it
				 * before executing next command. */
    int errorLine;              /* When TCL_ERROR is returned, this gives
                                 * the line number within the command where
                                 * the error occurred (1 if first line). */
} Tcl_Interp;

typedef struct Tcl_AsyncHandler_ *Tcl_AsyncHandler;
typedef struct Tcl_Channel_ *Tcl_Channel;
typedef struct Tcl_Command_ *Tcl_Command;
typedef struct Tcl_Event Tcl_Event;
typedef struct Tcl_Pid_ *Tcl_Pid;
typedef struct Tcl_RegExp_ *Tcl_RegExp;
typedef struct Tcl_TimerToken_ *Tcl_TimerToken;
typedef struct Tcl_Trace_ *Tcl_Trace;
typedef struct Tcl_Var_ *Tcl_Var;

/*
 * When a TCL command returns, the interpreter contains a result from the
 * command. Programmers are strongly encouraged to use one of the
 * procedures Tcl_GetObjResult() or Tcl_GetStringResult() to read the
 * interpreter's result. See the SetResult man page for details. Besides
 * this result, the command procedure returns an integer code, which is 
 * one of the following:
 *
 * TCL_OK		Command completed normally; the interpreter's
 *			result contains	the command's result.
 * TCL_ERROR		The command couldn't be completed successfully;
 *			the interpreter's result describes what went wrong.
 * TCL_RETURN		The command requests that the current procedure
 *			return; the interpreter's result contains the
 *			procedure's return value.
 * TCL_BREAK		The command requests that the innermost loop
 *			be exited; the interpreter's result is meaningless.
 * TCL_CONTINUE		Go on to the next iteration of the current loop;
 *			the interpreter's result is meaningless.
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
 * Forward declaration of Tcl_Obj to prevent an error when the forward
 * reference to Tcl_Obj is encountered in the procedure types declared 
 * below.
 */

struct Tcl_Obj;

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
typedef void (Tcl_DupInternalRepProc) _ANSI_ARGS_((struct Tcl_Obj *srcPtr, 
        struct Tcl_Obj *dupPtr));
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
typedef void (Tcl_FreeInternalRepProc) _ANSI_ARGS_((struct Tcl_Obj *objPtr));
typedef void (Tcl_FreeProc) _ANSI_ARGS_((char *blockPtr));
typedef void (Tcl_IdleProc) _ANSI_ARGS_((ClientData clientData));
typedef void (Tcl_InterpDeleteProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp));
typedef int (Tcl_MathProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tcl_Value *args, Tcl_Value *resultPtr));
typedef void (Tcl_NamespaceDeleteProc) _ANSI_ARGS_((ClientData clientData));
typedef int (Tcl_ObjCmdProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int objc, struct Tcl_Obj * CONST objv[]));
typedef int (Tcl_PackageInitProc) _ANSI_ARGS_((Tcl_Interp *interp));
typedef void (Tcl_TcpAcceptProc) _ANSI_ARGS_((ClientData callbackData,
        Tcl_Channel chan, char *address, int port));
typedef void (Tcl_TimerProc) _ANSI_ARGS_((ClientData clientData));
typedef int (Tcl_SetFromAnyProc) _ANSI_ARGS_((Tcl_Interp *interp,
	struct Tcl_Obj *objPtr));
typedef void (Tcl_UpdateStringProc) _ANSI_ARGS_((struct Tcl_Obj *objPtr));
typedef char *(Tcl_VarTraceProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *part1, char *part2, int flags));

/*
 * The following structure represents a type of object, which is a
 * particular internal representation for an object plus a set of
 * procedures that provide standard operations on objects of that type.
 */

typedef struct Tcl_ObjType {
    char *name;			/* Name of the type, e.g. "int". */
    Tcl_FreeInternalRepProc *freeIntRepProc;
				/* Called to free any storage for the type's
				 * internal rep. NULL if the internal rep
				 * does not need freeing. */
    Tcl_DupInternalRepProc *dupIntRepProc;
    				/* Called to create a new object as a copy
				 * of an existing object. */
    Tcl_UpdateStringProc *updateStringProc;
    				/* Called to update the string rep from the
				 * type's internal representation. */
    Tcl_SetFromAnyProc *setFromAnyProc;
    				/* Called to convert the object's internal
				 * rep to this type. Frees the internal rep
				 * of the old type. Returns TCL_ERROR on
				 * failure. */
} Tcl_ObjType;

/*
 * One of the following structures exists for each object in the Tcl
 * system. An object stores a value as either a string, some internal
 * representation, or both.
 */

typedef struct Tcl_Obj {
    int refCount;		/* When 0 the object will be freed. */
    char *bytes;		/* This points to the first byte of the
				 * object's string representation. The array
				 * must be followed by a null byte (i.e., at
				 * offset length) but may also contain
				 * embedded null characters. The array's
				 * storage is allocated by ckalloc. NULL
				 * means the string rep is invalid and must
				 * be regenerated from the internal rep.
				 * Clients should use Tcl_GetStringFromObj
				 * to get a pointer to the byte array as a
				 * readonly value. */
    int length;			/* The number of bytes at *bytes, not
				 * including the terminating null. */
    Tcl_ObjType *typePtr;	/* Denotes the object's type. Always
				 * corresponds to the type of the object's
				 * internal rep. NULL indicates the object
				 * has no internal rep (has no type). */
    union {			/* The internal representation: */
	long longValue;		/*   - an long integer value */
	double doubleValue;	/*   - a double-precision floating value */
	VOID *otherValuePtr;	/*   - another, type-specific value */
	struct {		/*   - internal rep as two pointers */
	    VOID *ptr1;
	    VOID *ptr2;
	} twoPtrValue;
    } internalRep;
} Tcl_Obj;

/*
 * Macros to increment and decrement a Tcl_Obj's reference count, and to
 * test whether an object is shared (i.e. has reference count > 1).
 * Note: clients should use Tcl_DecrRefCount() when they are finished using
 * an object, and should never call TclFreeObj() directly. TclFreeObj() is
 * only defined and made public in tcl.h to support Tcl_DecrRefCount's macro
 * definition. Note also that Tcl_DecrRefCount() refers to the parameter
 * "obj" twice. This means that you should avoid calling it with an
 * expression that is expensive to compute or has side effects.
 */

#define Tcl_IncrRefCount(objPtr) \
    ++(objPtr)->refCount
#define Tcl_DecrRefCount(objPtr) \
    if (--(objPtr)->refCount <= 0) TclFreeObj(objPtr)
#define Tcl_IsShared(objPtr) \
    ((objPtr)->refCount > 1)

/*
 * Macros and definitions that help to debug the use of Tcl objects.
 * When TCL_MEM_DEBUG is defined, the Tcl_New* declarations are 
 * overridden to call debugging versions of the object creation procedures.
 */

EXTERN Tcl_Obj *	Tcl_NewBooleanObj _ANSI_ARGS_((int boolValue));
EXTERN Tcl_Obj *	Tcl_NewDoubleObj _ANSI_ARGS_((double doubleValue));
EXTERN Tcl_Obj *	Tcl_NewIntObj _ANSI_ARGS_((int intValue));
EXTERN Tcl_Obj *	Tcl_NewListObj _ANSI_ARGS_((int objc,
			    Tcl_Obj *CONST objv[]));
EXTERN Tcl_Obj *	Tcl_NewLongObj _ANSI_ARGS_((long longValue));
EXTERN Tcl_Obj *	Tcl_NewObj _ANSI_ARGS_((void));
EXTERN Tcl_Obj *	Tcl_NewStringObj _ANSI_ARGS_((char *bytes,
			    int length));

#ifdef TCL_MEM_DEBUG
#  define Tcl_NewBooleanObj(val) \
     Tcl_DbNewBooleanObj(val, __FILE__, __LINE__)
#  define Tcl_NewDoubleObj(val) \
     Tcl_DbNewDoubleObj(val, __FILE__, __LINE__)
#  define Tcl_NewIntObj(val) \
     Tcl_DbNewLongObj(val, __FILE__, __LINE__)
#  define Tcl_NewListObj(objc, objv) \
     Tcl_DbNewListObj(objc, objv, __FILE__, __LINE__)
#  define Tcl_NewLongObj(val) \
     Tcl_DbNewLongObj(val, __FILE__, __LINE__)
#  define Tcl_NewObj() \
     Tcl_DbNewObj(__FILE__, __LINE__)
#  define Tcl_NewStringObj(bytes, len) \
     Tcl_DbNewStringObj(bytes, len, __FILE__, __LINE__)
#endif /* TCL_MEM_DEBUG */

/*
 * The following definitions support Tcl's namespace facility.
 * Note: the first five fields must match exactly the fields in a
 * Namespace structure (see tcl.h). 
 */

typedef struct Tcl_Namespace {
    char *name;                 /* The namespace's name within its parent
				 * namespace. This contains no ::'s. The
				 * name of the global namespace is ""
				 * although "::" is an synonym. */
    char *fullName;             /* The namespace's fully qualified name.
				 * This starts with ::. */
    ClientData clientData;      /* Arbitrary value associated with this
				 * namespace. */
    Tcl_NamespaceDeleteProc* deleteProc;
                                /* Procedure invoked when deleting the
				 * namespace to, e.g., free clientData. */
    struct Tcl_Namespace* parentPtr;
                                /* Points to the namespace that contains
				 * this one. NULL if this is the global
				 * namespace. */
} Tcl_Namespace;

/*
 * The following structure represents a call frame, or activation record.
 * A call frame defines a naming context for a procedure call: its local
 * scope (for local variables) and its namespace scope (used for non-local
 * variables; often the global :: namespace). A call frame can also define
 * the naming context for a namespace eval or namespace inscope command:
 * the namespace in which the command's code should execute. The
 * Tcl_CallFrame structures exist only while procedures or namespace
 * eval/inscope's are being executed, and provide a Tcl call stack.
 * 
 * A call frame is initialized and pushed using Tcl_PushCallFrame and
 * popped using Tcl_PopCallFrame. Storage for a Tcl_CallFrame must be
 * provided by the Tcl_PushCallFrame caller, and callers typically allocate
 * them on the C call stack for efficiency. For this reason, Tcl_CallFrame
 * is defined as a structure and not as an opaque token. However, most
 * Tcl_CallFrame fields are hidden since applications should not access
 * them directly; others are declared as "dummyX".
 *
 * WARNING!! The structure definition must be kept consistent with the
 * CallFrame structure in tclInt.h. If you change one, change the other.
 */

typedef struct Tcl_CallFrame {
    Tcl_Namespace *nsPtr;
    int dummy1;
    int dummy2;
    char *dummy3;
    char *dummy4;
    char *dummy5;
    int dummy6;
    char *dummy7;
    char *dummy8;
    int dummy9;
    char* dummy10;
} Tcl_CallFrame;

/*
 * Information about commands that is returned by Tcl_GetCmdInfo and passed
 * to Tcl_SetCmdInfo. objProc is an objc/objv object-based command procedure
 * while proc is a traditional Tcl argc/argv string-based procedure.
 * Tcl_CreateObjCommand and Tcl_CreateCommand ensure that both objProc and
 * proc are non-NULL and can be called to execute the command. However,
 * it may be faster to call one instead of the other. The member
 * isNativeObjectProc is set to 1 if an object-based procedure was
 * registered by Tcl_CreateObjCommand, and to 0 if a string-based procedure
 * was registered by Tcl_CreateCommand. The other procedure is typically set
 * to a compatibility wrapper that does string-to-object or object-to-string
 * argument conversions then calls the other procedure.
 */
     
typedef struct Tcl_CmdInfo {
    int isNativeObjectProc;	 /* 1 if objProc was registered by a call to
				  * Tcl_CreateObjCommand; 0 otherwise.
				  * Tcl_SetCmdInfo does not modify this
				  * field. */
    Tcl_ObjCmdProc *objProc;	 /* Command's object-based procedure. */
    ClientData objClientData;	 /* ClientData for object proc. */
    Tcl_CmdProc *proc;		 /* Command's string-based procedure. */
    ClientData clientData;	 /* ClientData for string proc. */
    Tcl_CmdDeleteProc *deleteProc;
                                 /* Procedure to call when command is
                                  * deleted. */
    ClientData deleteData;	 /* Value to pass to deleteProc (usually
				  * the same as clientData). */
    Tcl_Namespace *namespacePtr; /* Points to the namespace that contains
				  * this command. Note that Tcl_SetCmdInfo
				  * will not change a command's namespace;
				  * use Tcl_RenameCommand to do that. */

} Tcl_CmdInfo;

/*
 * The structure defined below is used to hold dynamic strings.  The only
 * field that clients should use is the string field, and they should
 * never modify it.
 */

#define TCL_DSTRING_STATIC_SIZE 200
typedef struct Tcl_DString {
    char *string;		/* Points to beginning of string:  either
				 * staticSpace below or a malloced array. */
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
 * Flag that may be passed to Tcl_GetIndexFromObj to force it to disallow
 * abbreviated strings.
 */

#define TCL_EXACT	1

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

#define TCL_GLOBAL_ONLY		 1
#define TCL_NAMESPACE_ONLY	 2
#define TCL_APPEND_VALUE	 4
#define TCL_LIST_ELEMENT	 8
#define TCL_TRACE_READS		 0x10
#define TCL_TRACE_WRITES	 0x20
#define TCL_TRACE_UNSETS	 0x40
#define TCL_TRACE_DESTROYED	 0x80
#define TCL_INTERP_DESTROYED	 0x100
#define TCL_LEAVE_ERR_MSG	 0x200
#define TCL_PARSE_PART1		 0x400

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
	    CONST char *key));
    Tcl_HashEntry *(*createProc) _ANSI_ARGS_((struct Tcl_HashTable *tablePtr,
	    CONST char *key, int *newPtr));
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
 * Positions to pass to Tcl_QueueEvent:
 */

typedef enum {
    TCL_QUEUE_TAIL, TCL_QUEUE_HEAD, TCL_QUEUE_MARK
} Tcl_QueuePosition;

/*
 * Values to pass to Tcl_SetServiceMode to specify the behavior of notifier
 * event routines.
 */

#define TCL_SERVICE_NONE 0
#define TCL_SERVICE_ALL 1

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

typedef int	(Tcl_DriverBlockModeProc) _ANSI_ARGS_((
		    ClientData instanceData, int mode));
typedef int	(Tcl_DriverCloseProc) _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp));
typedef int	(Tcl_DriverInputProc) _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toRead, int *errorCodePtr));
typedef int	(Tcl_DriverOutputProc) _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toWrite, int *errorCodePtr));
typedef int	(Tcl_DriverSeekProc) _ANSI_ARGS_((ClientData instanceData,
		    long offset, int mode, int *errorCodePtr));
typedef int	(Tcl_DriverSetOptionProc) _ANSI_ARGS_((
		    ClientData instanceData, Tcl_Interp *interp,
	            char *optionName, char *value));
typedef int	(Tcl_DriverGetOptionProc) _ANSI_ARGS_((
		    ClientData instanceData, Tcl_Interp *interp,
		    char *optionName, Tcl_DString *dsPtr));
typedef void	(Tcl_DriverWatchProc) _ANSI_ARGS_((
		    ClientData instanceData, int mask));
typedef int	(Tcl_DriverGetHandleProc) _ANSI_ARGS_((
		    ClientData instanceData, int direction,
		    ClientData *handlePtr));

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
    Tcl_DriverWatchProc *watchProc;	/* Set up the notifier to watch
                                         * for events on this channel. */
    Tcl_DriverGetHandleProc *getHandleProc;
					/* Get an OS handle from the channel
                                         * or NULL if not supported. */
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
 * Enum for different types of file paths.
 */

typedef enum Tcl_PathType {
    TCL_PATH_ABSOLUTE,
    TCL_PATH_RELATIVE,
    TCL_PATH_VOLUME_RELATIVE
} Tcl_PathType;

/*
 * Exported Tcl procedures:
 */

EXTERN void		Tcl_AddErrorInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *message));
EXTERN void		Tcl_AddObjErrorInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *message, int length));
EXTERN void		Tcl_AllowExceptions _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_AppendAllObjTypes _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr));
EXTERN void		Tcl_AppendElement _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN void		Tcl_AppendResult _ANSI_ARGS_(
			    TCL_VARARGS(Tcl_Interp *,interp));
EXTERN void		Tcl_AppendToObj _ANSI_ARGS_((Tcl_Obj *objPtr,
			    char *bytes, int length));
EXTERN void		Tcl_AppendStringsToObj _ANSI_ARGS_(
			    TCL_VARARGS(Tcl_Obj *,interp));
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
EXTERN int		Tcl_BadChannelOption _ANSI_ARGS_((Tcl_Interp *interp,
			    char *optionName, char *optionList));
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
EXTERN Tcl_Obj *	Tcl_ConcatObj _ANSI_ARGS_((int objc,
			    Tcl_Obj *CONST objv[]));
EXTERN int		Tcl_ConvertCountedElement _ANSI_ARGS_((char *src,
			    int length, char *dst, int flags));
EXTERN int		Tcl_ConvertElement _ANSI_ARGS_((char *src,
			    char *dst, int flags));
EXTERN int		Tcl_ConvertToType _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Tcl_ObjType *typePtr));
EXTERN int		Tcl_CreateAlias _ANSI_ARGS_((Tcl_Interp *slave,
			    char *slaveCmd, Tcl_Interp *target,
        		    char *targetCmd, int argc, char **argv));
EXTERN int		Tcl_CreateAliasObj _ANSI_ARGS_((Tcl_Interp *slave,
			    char *slaveCmd, Tcl_Interp *target,
        		    char *targetCmd, int objc,
		            Tcl_Obj *CONST objv[]));
EXTERN Tcl_Channel	Tcl_CreateChannel _ANSI_ARGS_((
    			    Tcl_ChannelType *typePtr, char *chanName,
                            ClientData instanceData, int mask));
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
			    Tcl_EventSetupProc *setupProc,
			    Tcl_EventCheckProc *checkProc,
			    ClientData clientData));
EXTERN void		Tcl_CreateExitHandler _ANSI_ARGS_((Tcl_ExitProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_CreateFileHandler _ANSI_ARGS_((
    			    int fd, int mask, Tcl_FileProc *proc,
			    ClientData clientData));
EXTERN Tcl_Interp *	Tcl_CreateInterp _ANSI_ARGS_((void));
EXTERN void		Tcl_CreateMathFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int numArgs, Tcl_ValueType *argTypes,
			    Tcl_MathProc *proc, ClientData clientData));
EXTERN Tcl_Command	Tcl_CreateObjCommand _ANSI_ARGS_((
			    Tcl_Interp *interp, char *cmdName,
			    Tcl_ObjCmdProc *proc, ClientData clientData,
			    Tcl_CmdDeleteProc *deleteProc));
EXTERN Tcl_Interp *	Tcl_CreateSlave _ANSI_ARGS_((Tcl_Interp *interp,
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
EXTERN Tcl_Obj *	Tcl_DbNewBooleanObj _ANSI_ARGS_((int boolValue,
                            char *file, int line));
EXTERN Tcl_Obj *	Tcl_DbNewDoubleObj _ANSI_ARGS_((double doubleValue,
                            char *file, int line));
EXTERN Tcl_Obj *	Tcl_DbNewListObj _ANSI_ARGS_((int objc,
			    Tcl_Obj *CONST objv[], char *file, int line));
EXTERN Tcl_Obj *	Tcl_DbNewLongObj _ANSI_ARGS_((long longValue,
                            char *file, int line));
EXTERN Tcl_Obj *	Tcl_DbNewObj _ANSI_ARGS_((char *file, int line));
EXTERN Tcl_Obj *	Tcl_DbNewStringObj _ANSI_ARGS_((char *bytes,
			    int length, char *file, int line));
EXTERN void		Tcl_DeleteAssocData _ANSI_ARGS_((Tcl_Interp *interp,
                            char *name));
EXTERN int		Tcl_DeleteCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName));
EXTERN int		Tcl_DeleteCommandFromToken _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Command command));
EXTERN void		Tcl_DeleteChannelHandler _ANSI_ARGS_((
    			    Tcl_Channel chan, Tcl_ChannelProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_DeleteCloseHandler _ANSI_ARGS_((
			    Tcl_Channel chan, Tcl_CloseProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_DeleteEvents _ANSI_ARGS_((
			    Tcl_EventDeleteProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_DeleteEventSource _ANSI_ARGS_((
			    Tcl_EventSetupProc *setupProc,
			    Tcl_EventCheckProc *checkProc,
			    ClientData clientData));
EXTERN void		Tcl_DeleteExitHandler _ANSI_ARGS_((Tcl_ExitProc *proc,
			    ClientData clientData));
EXTERN void		Tcl_DeleteFileHandler _ANSI_ARGS_((int fd));
EXTERN void		Tcl_DeleteHashEntry _ANSI_ARGS_((
			    Tcl_HashEntry *entryPtr));
EXTERN void		Tcl_DeleteHashTable _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr));
EXTERN void		Tcl_DeleteInterp _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_DeleteTimerHandler _ANSI_ARGS_((
			    Tcl_TimerToken token));
EXTERN void		Tcl_DeleteTrace _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Trace trace));
EXTERN void		Tcl_DetachPids _ANSI_ARGS_((int numPids, Tcl_Pid *pidPtr));
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
EXTERN Tcl_Obj *	Tcl_DuplicateObj _ANSI_ARGS_((Tcl_Obj *objPtr));
EXTERN int		Tcl_Eof _ANSI_ARGS_((Tcl_Channel chan));
EXTERN char *		Tcl_ErrnoId _ANSI_ARGS_((void));
EXTERN char *		Tcl_ErrnoMsg _ANSI_ARGS_((int err));
EXTERN int		Tcl_Eval _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN int		Tcl_EvalFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName));
EXTERN void		Tcl_EventuallyFree _ANSI_ARGS_((ClientData clientData,
			    Tcl_FreeProc *freeProc));
EXTERN int		Tcl_EvalObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
EXTERN void		Tcl_Exit _ANSI_ARGS_((int status));
EXTERN int		Tcl_ExposeCommand _ANSI_ARGS_((Tcl_Interp *interp,
        		    char *hiddenCmdName, char *cmdName));
EXTERN int		Tcl_ExprBoolean _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *ptr));
EXTERN int		Tcl_ExprBooleanObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, int *ptr));
EXTERN int		Tcl_ExprDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *ptr));
EXTERN int		Tcl_ExprDoubleObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, double *ptr));
EXTERN int		Tcl_ExprLong _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, long *ptr));
EXTERN int		Tcl_ExprLongObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, long *ptr));
EXTERN int		Tcl_ExprObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Tcl_Obj **resultPtrPtr));
EXTERN int		Tcl_ExprString _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
EXTERN void		Tcl_Finalize _ANSI_ARGS_((void));
EXTERN void		Tcl_FindExecutable _ANSI_ARGS_((char *argv0));
EXTERN Tcl_HashEntry *	Tcl_FirstHashEntry _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr,
			    Tcl_HashSearch *searchPtr));
EXTERN int		Tcl_Flush _ANSI_ARGS_((Tcl_Channel chan));
EXTERN void		TclFreeObj _ANSI_ARGS_((Tcl_Obj *objPtr));
EXTERN void		Tcl_FreeResult _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_GetAlias _ANSI_ARGS_((Tcl_Interp *interp,
       			    char *slaveCmd, Tcl_Interp **targetInterpPtr,
                            char **targetCmdPtr, int *argcPtr,
			    char ***argvPtr));
EXTERN int		Tcl_GetAliasObj _ANSI_ARGS_((Tcl_Interp *interp,
       			    char *slaveCmd, Tcl_Interp **targetInterpPtr,
                            char **targetCmdPtr, int *objcPtr,
			    Tcl_Obj ***objv));
EXTERN ClientData	Tcl_GetAssocData _ANSI_ARGS_((Tcl_Interp *interp,
                            char *name, Tcl_InterpDeleteProc **procPtr));
EXTERN int		Tcl_GetBoolean _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *boolPtr));
EXTERN int		Tcl_GetBooleanFromObj _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr,
			    int *boolPtr));
EXTERN Tcl_Channel	Tcl_GetChannel _ANSI_ARGS_((Tcl_Interp *interp,
	        	    char *chanName, int *modePtr));
EXTERN int		Tcl_GetChannelBufferSize _ANSI_ARGS_((
    			    Tcl_Channel chan));
EXTERN int		Tcl_GetChannelHandle _ANSI_ARGS_((Tcl_Channel chan,
	        	    int direction, ClientData *handlePtr));
EXTERN ClientData	Tcl_GetChannelInstanceData _ANSI_ARGS_((
    			    Tcl_Channel chan));
EXTERN int		Tcl_GetChannelMode _ANSI_ARGS_((Tcl_Channel chan));
EXTERN char *		Tcl_GetChannelName _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_GetChannelOption _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Channel chan, char *optionName,
			    Tcl_DString *dsPtr));
EXTERN Tcl_ChannelType * Tcl_GetChannelType _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_GetCommandInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdInfo *infoPtr));
EXTERN char *		Tcl_GetCommandName _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Command command));
EXTERN char *		Tcl_GetCwd _ANSI_ARGS_((char *buf, int len));
EXTERN int		Tcl_GetDouble _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *doublePtr));
EXTERN int		Tcl_GetDoubleFromObj _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr,
			    double *doublePtr));
EXTERN int		Tcl_GetErrno _ANSI_ARGS_((void));
EXTERN int		Tcl_GetErrorLine _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN char *		Tcl_GetHostName _ANSI_ARGS_((void));
EXTERN int		Tcl_GetIndexFromObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, char **tablePtr, char *msg,
			    int flags, int *indexPtr));
EXTERN int		Tcl_GetInt _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *intPtr));
EXTERN int		Tcl_GetInterpPath _ANSI_ARGS_((Tcl_Interp *askInterp,
			    Tcl_Interp *slaveInterp));
EXTERN int		Tcl_GetIntFromObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, int *intPtr));
EXTERN int		Tcl_GetLongFromObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, long *longPtr));
EXTERN Tcl_Interp *	Tcl_GetMaster _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tcl_Obj *	Tcl_GetObjResult _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tcl_ObjType *	Tcl_GetObjType _ANSI_ARGS_((char *typeName));
EXTERN int		Tcl_GetOpenFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int write, int checkUsage,
			    ClientData *filePtr));
EXTERN Tcl_Command	Tcl_GetOriginalCommand _ANSI_ARGS_((
			    Tcl_Command command));
EXTERN Tcl_PathType	Tcl_GetPathType _ANSI_ARGS_((char *path));
EXTERN int		Tcl_Gets _ANSI_ARGS_((Tcl_Channel chan,
        		    Tcl_DString *dsPtr));
EXTERN int		Tcl_GetsObj _ANSI_ARGS_((Tcl_Channel chan,
        		    Tcl_Obj *objPtr));
EXTERN int		Tcl_GetServiceMode _ANSI_ARGS_((void));
EXTERN Tcl_Interp *	Tcl_GetSlave _ANSI_ARGS_((Tcl_Interp *interp,
			    char *slaveName));
EXTERN Tcl_Channel	Tcl_GetStdChannel _ANSI_ARGS_((int type));
EXTERN char *		Tcl_GetStringFromObj _ANSI_ARGS_((Tcl_Obj *objPtr,
			    int *lengthPtr));
EXTERN char *		Tcl_GetStringResult _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN char *		Tcl_GetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, int flags));
EXTERN char *		Tcl_GetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags));
EXTERN int		Tcl_GlobalEval _ANSI_ARGS_((Tcl_Interp *interp,
			    char *command));
EXTERN int		Tcl_GlobalEvalObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
EXTERN char *		Tcl_HashStats _ANSI_ARGS_((Tcl_HashTable *tablePtr));
EXTERN int		Tcl_HideCommand _ANSI_ARGS_((Tcl_Interp *interp,
		            char *cmdName, char *hiddenCmdName));
EXTERN int		Tcl_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_InitHashTable _ANSI_ARGS_((Tcl_HashTable *tablePtr,
			    int keyType));
EXTERN void		Tcl_InitMemory _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_InputBlocked _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_InputBuffered _ANSI_ARGS_((Tcl_Channel chan));
EXTERN int		Tcl_InterpDeleted _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		Tcl_IsSafe _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		Tcl_InvalidateStringRep _ANSI_ARGS_((
			    Tcl_Obj *objPtr));
EXTERN char *		Tcl_JoinPath _ANSI_ARGS_((int argc, char **argv,
			    Tcl_DString *resultPtr));
EXTERN int		Tcl_LinkVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, char *addr, int type));
EXTERN int		Tcl_ListObjAppendList _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *listPtr, 
			    Tcl_Obj *elemListPtr));
EXTERN int		Tcl_ListObjAppendElement _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *listPtr,
			    Tcl_Obj *objPtr));
EXTERN int		Tcl_ListObjGetElements _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *listPtr,
			    int *objcPtr, Tcl_Obj ***objvPtr));
EXTERN int		Tcl_ListObjIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *listPtr, int index, 
			    Tcl_Obj **objPtrPtr));
EXTERN int		Tcl_ListObjLength _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *listPtr, int *intPtr));
EXTERN int		Tcl_ListObjReplace _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *listPtr, int first, int count,
			    int objc, Tcl_Obj *CONST objv[]));
EXTERN void		Tcl_Main _ANSI_ARGS_((int argc, char **argv,
			    Tcl_AppInitProc *appInitProc));
EXTERN Tcl_Channel	Tcl_MakeFileChannel _ANSI_ARGS_((ClientData handle,
			    int mode));
EXTERN int		Tcl_MakeSafe _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tcl_Channel	Tcl_MakeTcpClientChannel _ANSI_ARGS_((
    			    ClientData tcpSocket));
EXTERN char *		Tcl_Merge _ANSI_ARGS_((int argc, char **argv));
EXTERN Tcl_HashEntry *	Tcl_NextHashEntry _ANSI_ARGS_((
			    Tcl_HashSearch *searchPtr));
EXTERN void		Tcl_NotifyChannel _ANSI_ARGS_((Tcl_Channel channel,
			    int mask));
EXTERN Tcl_Obj *	Tcl_ObjGetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
			    int flags));
EXTERN Tcl_Obj *	Tcl_ObjSetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
			    Tcl_Obj *newValuePtr, int flags));
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
EXTERN void		Tcl_RegisterObjType _ANSI_ARGS_((
			    Tcl_ObjType *typePtr));
EXTERN void		Tcl_Release _ANSI_ARGS_((ClientData clientData));
EXTERN void		Tcl_RestartIdleTimer _ANSI_ARGS_((void));
EXTERN void		Tcl_ResetResult _ANSI_ARGS_((Tcl_Interp *interp));
#define Tcl_Return Tcl_SetResult
EXTERN int		Tcl_ScanCountedElement _ANSI_ARGS_((char *string,
			    int length, int *flagPtr));
EXTERN int		Tcl_ScanElement _ANSI_ARGS_((char *string,
			    int *flagPtr));
EXTERN int		Tcl_Seek _ANSI_ARGS_((Tcl_Channel chan,
        		    int offset, int mode));
EXTERN int		Tcl_ServiceAll _ANSI_ARGS_((void));
EXTERN int		Tcl_ServiceEvent _ANSI_ARGS_((int flags));
EXTERN void		Tcl_SetAssocData _ANSI_ARGS_((Tcl_Interp *interp,
                            char *name, Tcl_InterpDeleteProc *proc,
                            ClientData clientData));
EXTERN void		Tcl_SetBooleanObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    int boolValue));
EXTERN void		Tcl_SetChannelBufferSize _ANSI_ARGS_((
			    Tcl_Channel chan, int sz));
EXTERN int		Tcl_SetChannelOption _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Channel chan,
	        	    char *optionName, char *newValue));
EXTERN int		Tcl_SetCommandInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *cmdName, Tcl_CmdInfo *infoPtr));
EXTERN void		Tcl_SetDoubleObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    double doubleValue));
EXTERN void		Tcl_SetErrno _ANSI_ARGS_((int err));
EXTERN void		Tcl_SetErrorCode _ANSI_ARGS_(
    			    TCL_VARARGS(Tcl_Interp *,arg1));
EXTERN void		Tcl_SetIntObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    int intValue));
EXTERN void		Tcl_SetListObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    int objc, Tcl_Obj *CONST objv[]));
EXTERN void		Tcl_SetLongObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    long longValue));
EXTERN void		Tcl_SetMaxBlockTime _ANSI_ARGS_((Tcl_Time *timePtr));
EXTERN void		Tcl_SetObjErrorCode _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *errorObjPtr));
EXTERN void		Tcl_SetObjLength _ANSI_ARGS_((Tcl_Obj *objPtr,
			    int length));
EXTERN void		Tcl_SetObjResult _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *resultObjPtr));
EXTERN void		Tcl_SetPanicProc _ANSI_ARGS_((void (*proc)
			    _ANSI_ARGS_(TCL_VARARGS(char *, format))));
EXTERN int		Tcl_SetRecursionLimit _ANSI_ARGS_((Tcl_Interp *interp,
			    int depth));
EXTERN void		Tcl_SetResult _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Tcl_FreeProc *freeProc));
EXTERN int		Tcl_SetServiceMode _ANSI_ARGS_((int mode));
EXTERN void		Tcl_SetStdChannel _ANSI_ARGS_((Tcl_Channel channel,
			    int type));
EXTERN void		Tcl_SetStringObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    char *bytes, int length));
EXTERN void		Tcl_SetTimer _ANSI_ARGS_((Tcl_Time *timePtr));
EXTERN char *		Tcl_SetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, char *newValue, int flags));
EXTERN char *		Tcl_SetVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, char *newValue,
			    int flags));
EXTERN char *		Tcl_SignalId _ANSI_ARGS_((int sig));
EXTERN char *		Tcl_SignalMsg _ANSI_ARGS_((int sig));
EXTERN void		Tcl_Sleep _ANSI_ARGS_((int ms));
EXTERN void		Tcl_SourceRCFile _ANSI_ARGS_((Tcl_Interp *interp));
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
EXTERN int		Tcl_Ungets _ANSI_ARGS_((Tcl_Channel chan, char *str,
			    int len, int atHead));
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
EXTERN Tcl_Pid		Tcl_WaitPid _ANSI_ARGS_((Tcl_Pid pid, int *statPtr, 
			    int options));
EXTERN int		Tcl_Write _ANSI_ARGS_((Tcl_Channel chan,
			    char *s, int slen));
EXTERN void		Tcl_WrongNumArgs _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[], char *message));

#endif /* RESOURCE_INCLUDED */
#endif /* _TCL */
