/*
 * tclInt.h --
 *
 *	Declarations of things used internally by the Tcl interpreter.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1993-1997 Lucent Technologies.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclInt.h 1.277 97/06/20 15:19:00
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
 * Data structures related to namespaces.
 *----------------------------------------------------------------
 */

/*
 * The structure below defines a namespace.
 * Note: the first five fields must match exactly the fields in a
 * Tcl_Namespace structure (see tcl.h). If you change one, be sure to
 * change the other.
 */

typedef struct Namespace {
    char *name;			 /* The namespace's simple (unqualified)
				  * name. This contains no ::'s. The name of
				  * the global namespace is "" although "::"
				  * is an synonym. */
    char *fullName;		 /* The namespace's fully qualified name.
				  * This starts with ::. */
    ClientData clientData;	 /* An arbitrary value associated with this
				  * namespace. */
    Tcl_NamespaceDeleteProc *deleteProc;
				 /* Procedure invoked when deleting the
				  * namespace to, e.g., free clientData. */
    struct Namespace *parentPtr; /* Points to the namespace that contains
				  * this one. NULL if this is the global
				  * namespace. */
    Tcl_HashTable childTable;	 /* Contains any child namespaces. Indexed
                                  * by strings; values have type
				  * (Namespace *). */
    long nsId;			 /* Unique id for the namespace. */
    Tcl_Interp *interp;		 /* The interpreter containing this
				  * namespace. */
    int flags;			 /* OR-ed combination of the namespace
				  * status flags NS_DYING and NS_DEAD
				  * listed below. */
    int activationCount;	 /* Number of "activations" or active call
				  * frames for this namespace that are on
				  * the Tcl call stack. The namespace won't
				  * be freed until activationCount becomes
				  * zero. */
    int refCount;		 /* Count of references by namespaceName *
				  * objects. The namespace can't be freed
				  * until refCount becomes zero. */
    Tcl_HashTable cmdTable;	 /* Contains all the commands currently
                                  * registered in the namespace. Indexed by
                                  * strings; values have type (Command *).
				  * Commands imported by Tcl_Import have
				  * Command structures that point (via an
				  * ImportedCmdRef structure) to the
				  * Command structure in the source
				  * namespace's command table. */
    Tcl_HashTable varTable;	 /* Contains all the (global) variables
				  * currently in this namespace. Indexed
                                  * by strings; values have type (Var *). */
    char **exportArrayPtr;	 /* Points to an array of string patterns
				  * specifying which commands are exported.
				  * A pattern may include "string match"
				  * style wildcard characters to specify
				  * multiple commands; however, no namespace
				  * qualifiers are allowed. NULL if no
				  * export patterns are registered. */
    int numExportPatterns;	 /* Number of export patterns currently
				  * registered using "namespace export". */
    int maxExportPatterns;	 /* Mumber of export patterns for which
				  * space is currently allocated. */
    int cmdRefEpoch;		 /* Incremented if a newly added command
				  * shadows a command for which this
				  * namespace has already cached a Command *
				  * pointer; this causes all its cached
				  * Command* pointers to be invalidated. */
} Namespace;

/*
 * Flags used to represent the status of a namespace:
 *
 * NS_DYING -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace but there are still active call frames on the Tcl
 *		stack that refer to the namespace. When the last call frame
 *		referring to it has been popped, it's variables and command
 *		will be destroyed and it will be marked "dead" (NS_DEAD).
 *		The namespace can no longer be looked up by name.
 * NS_DEAD -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace and no call frames still refer to it. Its
 *		variables and command have already been destroyed. This bit
 *		allows the namespace resolution code to recognize that the
 *		namespace is "deleted". When the last namespaceName object
 *		in any byte code code unit that refers to the namespace has
 *		been freed (i.e., when the namespace's refCount is 0), the
 *		namespace's storage will be freed.
 */

#define NS_DYING  	0x01
#define NS_DEAD  	0x02

/*
 * Flag passed to TclGetNamespaceForQualName to have it create all namespace
 * components of a namespace-qualified name that cannot be found. The new
 * namespaces are created within their specified parent. Note that this
 * flag's value must not conflict with the values of the flags
 * TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY, and FIND_ONLY_NS (defined in
 * tclNamesp.c).
 */

#define CREATE_NS_IF_UNKNOWN 0x800

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
 * with a Tcl_Obj value. These structures are kept in procedure call frames
 * (for local variables recognized by the compiler) or in the heap (for
 * global variables and any variable not known to the compiler). For each
 * Var structure in the heap, a hash table entry holds the variable name and
 * a pointer to the Var structure.
 */

typedef struct Var {
    union {
	Tcl_Obj *objPtr;	/* The variable's object value. Used for 
				 * scalar variables and array elements. */
	Tcl_HashTable *tablePtr;/* For array variables, this points to
				 * information about the hash table used
				 * to implement the associative array. 
				 * Points to malloc-ed data. */
	struct Var *linkPtr;	/* If this is a global variable being
				 * referred to in a procedure, or a variable
				 * created by "upvar", this field points to
				 * the referenced variable's Var struct. */
    } value;
    char *name;			/* NULL if the variable is in a hashtable,
				 * otherwise points to the variable's
				 * name. It is used, e.g., by TclLookupVar
				 * and "info locals". The storage for the
				 * characters of the name is not owned by
				 * the Var and must not be freed when
				 * freeing the Var. */
    Namespace *nsPtr;		/* Points to the namespace that contains
				 * this variable or NULL if the variable is
				 * a local variable in a Tcl procedure. */
    Tcl_HashEntry *hPtr;	/* If variable is in a hashtable, either the
				 * hash table entry that refers to this
				 * variable or NULL if the variable has been
				 * detached from its hash table (e.g. an
				 * array is deleted, but some of its
				 * elements are still referred to in
				 * upvars). NULL if the variable is not in a
				 * hashtable. This is used to delete an
				 * variable from its hashtable if it is no
				 * longer needed. */
    int refCount;		/* Counts number of active uses of this
				 * variable, not including its entry in the
				 * call frame or the hash table: 1 for each
				 * additional variable whose linkPtr points
				 * here, 1 for each nested trace active on
				 * variable. This record can't be deleted
				 * until refCount becomes 0. */
    VarTrace *tracePtr;		/* First in list of all traces set for this
				 * variable. */
    ArraySearch *searchPtr;	/* First in list of all searches active
				 * for this variable, or NULL if none. */
    int flags;			/* Miscellaneous bits of information about
				 * variable. See below for definitions. */
} Var;

/*
 * Flag bits for variables. The first three (VAR_SCALAR, VAR_ARRAY, and
 * VAR_LINK) are mutually exclusive and give the "type" of the variable.
 * VAR_UNDEFINED is independent of the variable's type. 
 *
 * VAR_SCALAR -			1 means this is a scalar variable and not
 *				an array or link. The "objPtr" field points
 *				to the variable's value, a Tcl object.
 * VAR_ARRAY -			1 means this is an array variable rather
 *				than a scalar variable or link. The
 *				"tablePtr" field points to the array's
 *				hashtable for its elements.
 * VAR_LINK - 			1 means this Var structure contains a
 *				pointer to another Var structure that
 *				either has the real value or is itself
 *				another VAR_LINK pointer. Variables like
 *				this come about through "upvar" and "global"
 *				commands, or through references to variables
 *				in enclosing namespaces.
 * VAR_UNDEFINED -		1 means that the variable is in the process
 *				of being deleted. An undefined variable
 *				logically does not exist and survives only
 *				while it has a trace, or if it is a global
 *				variable currently being used by some
 *				procedure.
 * VAR_IN_HASHTABLE -		1 means this variable is in a hashtable and
 *				the Var structure is malloced. 0 if it is
 *				a local variable that was assigned a slot
 *				in a procedure frame by	the compiler so the
 *				Var storage is part of the call frame.
 * VAR_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a read or write access, so
 *				new read or write accesses should not cause
 *				trace procedures to be called and the
 *				variable can't be deleted.
 * VAR_ARRAY_ELEMENT -		1 means that this variable is an array
 *				element, so it is not legal for it to be
 *				an array itself (the VAR_ARRAY flag had
 *				better not be set).
 */

#define VAR_SCALAR		0x1
#define VAR_ARRAY		0x2
#define VAR_LINK		0x4
#define VAR_UNDEFINED	        0x8
#define VAR_IN_HASHTABLE	0x10
#define VAR_TRACE_ACTIVE	0x20
#define VAR_ARRAY_ELEMENT	0x40

/*
 * Macros to ensure that various flag bits are set properly for variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN void	TclSetVarScalar _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarArray _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarLink _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarArrayElement _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarUndefined _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclClearVarUndefined _ANSI_ARGS_((Var *varPtr));
 */

#define TclSetVarScalar(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_ARRAY|VAR_LINK)) | VAR_SCALAR

#define TclSetVarArray(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_SCALAR|VAR_LINK)) | VAR_ARRAY

#define TclSetVarLink(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_SCALAR|VAR_ARRAY)) | VAR_LINK

#define TclSetVarArrayElement(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~VAR_ARRAY) | VAR_ARRAY_ELEMENT

#define TclSetVarUndefined(varPtr) \
    (varPtr)->flags |= VAR_UNDEFINED

#define TclClearVarUndefined(varPtr) \
    (varPtr)->flags &= ~VAR_UNDEFINED

/*
 * Macros to read various flag bits of variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN int	TclIsVarScalar _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarLink _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarArray _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarUndefined _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarArrayElement _ANSI_ARGS_((Var *varPtr));
 */
    
#define TclIsVarScalar(varPtr) \
    ((varPtr)->flags & VAR_SCALAR)

#define TclIsVarLink(varPtr) \
    ((varPtr)->flags & VAR_LINK)

#define TclIsVarArray(varPtr) \
    ((varPtr)->flags & VAR_ARRAY)

#define TclIsVarUndefined(varPtr) \
    ((varPtr)->flags & VAR_UNDEFINED)

#define TclIsVarArrayElement(varPtr) \
    ((varPtr)->flags & VAR_ARRAY_ELEMENT)

/*
 *----------------------------------------------------------------
 * Data structures related to procedures.  These are used primarily
 * in tclProc.c, tclCompile.c, and tclExecute.c.
 *----------------------------------------------------------------
 */

/*
 * The variable-length structure below describes a local variable of a
 * procedure that was recognized by the compiler. These variables have a
 * name, an element in the array of compiler-assigned local variables in the
 * procedure's call frame, and various other items of information. If the
 * local variable is a formal argument, it may also have a default value.
 * The compiler can't recognize local variables whose names are
 * expressions (these names are only known at runtime when the expressions
 * are evaluated) or local variables that are created as a result of an
 * "upvar" or "uplevel" command. These other local variables are kept
 * separately in a hash table in the call frame.
 */

typedef struct CompiledLocal {
    struct CompiledLocal *nextPtr;
				/* Next compiler-recognized local variable
				 * for this procedure, or NULL if this is
				 * the last local. */
    int nameLength;		/* The number of characters in local
				 * variable's name. Used to speed up
				 * variable lookups. */
    int frameIndex;		/* Index in the array of compiler-assigned
				 * variables in the procedure call frame. */
    int isArg;			/* 1 if the local variable is a formal
				 * argument. */
    int isTemp;			/* 1 if the local variable is an anonymous
				 * temporary variable. Temporaries have
				 * a NULL name. */
    int flags;			/* Flag bits for the local variable. Same as
				 * the flags for the Var structure above,
				 * although only VAR_SCALAR, VAR_ARRAY, and
				 * VAR_LINK make sense. */
    Tcl_Obj *defValuePtr;	/* Pointer to the default value of an
				 * argument, if any. NULL if not an argument
				 * or, if an argument, no default value. */
    char name[4];		/* Name of the local variable starts here.
				 * If the name is NULL, this will just be
				 * '\0'. The actual size of this field will
				 * be large enough to hold the name. MUST
				 * BE THE LAST FIELD IN THE STRUCTURE! */
} CompiledLocal;

/*
 * The structure below defines a command procedure, which consists of a
 * collection of Tcl commands plus information about arguments and other
 * local variables recognized at compile time.
 */

typedef struct Proc {
    struct Interp *iPtr;	  /* Interpreter for which this command
				   * is defined. */
    int refCount;		  /* Reference count: 1 if still present
				   * in command table plus 1 for each call
				   * to the procedure that is currently
				   * active. This structure can be freed
				   * when refCount becomes zero. */
    Namespace *nsPtr;		  /* Points to the namespace that contains
				   * this procedure. */
    Tcl_Obj *bodyPtr;		  /* Points to the ByteCode object for
				   * procedure's body command. */
    int numArgs;		  /* Number of formal parameters. */
    int numCompiledLocals;	  /* Count of local variables recognized by
				   * the compiler including arguments and
				   * temporaries. */
    CompiledLocal *firstLocalPtr; /* Pointer to first of the procedure's
				   * compiler-allocated local variables, or
				   * NULL if none. The first numArgs entries
				   * in this list describe the procedure's
				   * formal arguments. */
    CompiledLocal *lastLocalPtr;  /* Pointer to the last allocated local
				   * variable or NULL if none. This has
				   * frame index (numCompiledLocals-1). */
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
 * The structure below defines a call frame. A call frame defines a naming
 * context for a procedure call: its local naming scope (for local
 * variables) and its global naming scope (a namespace, perhaps the global
 * :: namespace). A call frame can also define the naming context for a
 * namespace eval or namespace inscope command: the namespace in which the
 * command's code should execute. The Tcl_CallFrame structures exist only
 * while procedures or namespace eval/inscope's are being executed, and
 * provide a kind of Tcl call stack.
 * 
 * WARNING!! The structure definition must be kept consistent with the
 * Tcl_CallFrame structure in tcl.h. If you change one, change the other.
 */

typedef struct CallFrame {
    Namespace *nsPtr;		/* Points to the namespace used to resolve
				 * commands and global variables. */
    int isProcCallFrame;	/* If nonzero, the frame was pushed to
				 * execute a Tcl procedure and may have
				 * local vars. If 0, the frame was pushed
				 * to execute a namespace command and var
				 * references are treated as references to
				 * namespace vars; varTablePtr and
				 * compiledLocals are ignored. */
    int objc;			/* This and objv below describe the
				 * arguments for this procedure call. */
    Tcl_Obj *CONST *objv;	/* Array of argument objects. */
    struct CallFrame *callerPtr;
				/* Value of interp->framePtr when this
				 * procedure was invoked (i.e. next higher
				 * in stack of all active procedures). */
    struct CallFrame *callerVarPtr;
				/* Value of interp->varFramePtr when this
				 * procedure was invoked (i.e. determines
				 * variable scoping within caller). Same
				 * as callerPtr unless an "uplevel" command
				 * or something equivalent was active in
				 * the caller). */
    int level;			/* Level of this procedure, for "uplevel"
				 * purposes (i.e. corresponds to nesting of
				 * callerVarPtr's, not callerPtr's). 1 for
				 * outermost procedure, 0 for top-level. */
    Proc *procPtr;		/* Points to the structure defining the
				 * called procedure. Used to get information
				 * such as the number of compiled local
				 * variables (local variables assigned
				 * entries ["slots"] in the compiledLocals
				 * array below). */
    Tcl_HashTable *varTablePtr;	/* Hash table containing local variables not
				 * recognized by the compiler, or created at
				 * execution time through, e.g., upvar.
				 * Initially NULL and created if needed. */
    int numCompiledLocals;	/* Count of local variables recognized by
				 * the compiler including arguments. */
    Var* compiledLocals;	/* Points to the array of local variables
				 * recognized by the compiler. The compiler
				 * emits code that refers to these variables
				 * using an index into this array. */
} CallFrame;

/*
 *----------------------------------------------------------------
 * Data structures related to history.   These are used primarily
 * in tclHistory.c
 *----------------------------------------------------------------
 */

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
    int builtinFuncIndex;	/* If this is a builtin math function, its
				 * index in the array of builtin functions.
				 * (tclCompilation.h lists these indices.)
				 * The value is -1 if this is a new function
				 * defined by Tcl_CreateMathFunc. The value
				 * is also -1 if a builtin function is
				 * replaced by a Tcl_CreateMathFunc call. */
    int numArgs;		/* Number of arguments for function. */
    Tcl_ValueType argTypes[MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    Tcl_MathProc *proc;		/* Procedure that implements this function.
				 * NULL if isBuiltinFunc is 1. */
    ClientData clientData;	/* Additional argument to pass to the
				 * function when invoking it. NULL if
				 * isBuiltinFunc is 1. */
} MathFunc;

/*
 *----------------------------------------------------------------
 * Data structures related to bytecode compilation and execution.
 * These are used primarily in tclCompile.c, tclExecute.c, and
 * tclBasic.c.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent an error when the forward reference to
 * CompileEnv is encountered in the procedure type CompileProc declared
 * below.
 */

struct CompileEnv;

/*
 * The type of procedures called by the Tcl bytecode compiler to compile
 * commands. Pointers to these procedures are kept in the Command structure
 * describing each command. When a CompileProc returns, the interpreter's
 * result is set to error information, if any. In addition, the CompileProc
 * returns an integer value, which is one of the following:
 *
 * TCL_OK		Compilation completed normally.
 * TCL_ERROR		Compilation failed because of an error;
 *			the interpreter's result describes what went wrong.
 * TCL_OUT_LINE_COMPILE	Compilation failed because, e.g., the command is
 *			too complex for effective inline compilation. The
 *			CompileProc believes the command is legal but 
 *			should be compiled "out of line" by emitting code
 *			to invoke its command procedure at runtime.
 */

#define TCL_OUT_LINE_COMPILE	(TCL_CONTINUE + 1)

typedef int (CompileProc) _ANSI_ARGS_((Tcl_Interp *interp, char *string,
	char *lastChar, int compileFlags, struct CompileEnv *compEnvPtr));

/*
 * The data structure defining the execution environment for ByteCode's.
 * There is one ExecEnv structure per Tcl interpreter. It holds the
 * evaluation stack that holds command operands and results. The stack grows
 * towards increasing addresses. The "stackTop" member is cached by
 * TclExecuteByteCode in a local variable: it must be set before calling
 * TclExecuteByteCode and will be restored by TclExecuteByteCode before it
 * returns.
 */

typedef union StackItem {
    Tcl_Obj *o;			/* Stack item as a pointer to a Tcl_Obj. */
    int      i;			/* Stack item as an integer. */
    VOID    *p;			/* Stack item as an arbitrary pointer. */
} StackItem;

typedef struct ExecEnv {
    StackItem *stackPtr;	/* Points to the first item in the
				 * evaluation stack on the heap. */
    int stackTop;		/* Index of current top of stack; -1 when
				 * the stack is empty. */
    int stackEnd;		/* Index of last usable item in stack. */
} ExecEnv;

/*
 *----------------------------------------------------------------
 * Data structures related to commands.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent an error when the forward reference to
 * Command is encountered in the ImportRef type declared below.
 */

struct Command;

/*
 * An imported command is created in an namespace when it imports a "real"
 * command from another namespace. An imported command has a Command
 * structure that points (via its ClientData value) to the "real" Command
 * structure in the source namespace's command table. The real command
 * records all the imported commands that refer to it in a list of ImportRef
 * structures so that they can be deleted when the real command is deleted.  */

typedef struct ImportRef {
    struct Command *importedCmdPtr;
    				/* Points to the imported command created in
				 * an importing namespace; this command
				 * redirects its invocations to the "real"
				 * command. */
    struct ImportRef *nextPtr;	/* Next element on the linked list of
				 * imported commands that refer to the
				 * "real" command. The real command deletes
				 * these imported commands on this list when
				 * it is deleted. */
} ImportRef;

/*
 * A Command structure exists for each command in a namespace. The
 * Tcl_Command opaque type actually refers to these structures.
 */

typedef struct Command {
    Tcl_HashEntry *hPtr;	/* Pointer to the hash table entry that
				 * refers to this command. The hash table is
				 * either a namespace's command table or an
				 * interpreter's hidden command table. This
				 * pointer is used to get a command's name
				 * from its Tcl_Command handle. NULL means
				 * that the hash table entry has been
				 * removed already (this can happen if
				 * deleteProc causes the command to be
				 * deleted or recreated). */
    Namespace *nsPtr;		/* Points to the namespace containing this
				 * command. */
    int refCount;		/* 1 if in command hashtable plus 1 for each
				 * reference from a CmdName Tcl object
				 * representing a command's name in a
				 * ByteCode instruction sequence. This
				 * structure can be freed when refCount
				 * becomes zero. */
    int cmdEpoch;		/* Incremented to invalidate any references
                                 * that point to this command when it is
				 * renamed, deleted, hidden, or exposed. */
    CompileProc *compileProc;   /* Procedure called to compile command. NULL
				 * if no compile proc exists for command. */
    Tcl_ObjCmdProc *objProc;	/* Object-based command procedure. */
    ClientData objClientData;	/* Arbitrary value passed to object proc. */
    Tcl_CmdProc *proc;		/* String-based command procedure. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* Procedure invoked when deleting command
				 * to, e.g., free all client data. */
    ClientData deleteData;	/* Arbitrary value passed to deleteProc. */
    int deleted;		/* Means that the command is in the process
				 * of being deleted (its deleteProc is
				 * currently executing). Other attempts to
				 * delete the command should be ignored. */
    ImportRef *importRefPtr;	/* List of each imported Command created in
				 * another namespace when this command is
				 * imported. These imported commands
				 * redirect invocations back to this
				 * command. The list is used to remove all
				 * those imported commands when deleting
				 * this "real" command. */
} Command;

/*
 *----------------------------------------------------------------
 * This structure defines an interpreter, which is a collection of
 * commands plus other state information related to interpreting
 * commands, such as variable storage. Primary responsibility for
 * this data structure is in tclBasic.c, but almost every Tcl
 * source file uses something in here.
 *----------------------------------------------------------------
 */

typedef struct Interp {

    /*
     * Note:  the first three fields must match exactly the fields in
     * a Tcl_Interp struct (see tcl.h).  If you change one, be sure to
     * change the other.
     *
     * The interpreter's result is held in both the string and the
     * objResultPtr fields. These fields hold, respectively, the result's
     * string or object value. The interpreter's result is always in the
     * result field if that is non-empty, otherwise it is in objResultPtr.
     * The two fields are kept consistent unless some C code sets
     * interp->result directly. Programs should not access result and
     * objResultPtr directly; instead, they should always get and set the
     * result using procedures such as Tcl_SetObjResult, Tcl_GetObjResult,
     * and Tcl_GetStringResult. See the SetResult man page for details.
     */

    char *result;               /* If the last command returned a string
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Tcl_FreeProc *freeProc;     /* Zero means a string result is statically
                                 * allocated. TCL_DYNAMIC means string
                                 * result was allocated with ckalloc and
                                 * should be freed with ckfree. Other values
                                 * give address of procedure to invoke to
                                 * free the string result. Tcl_Eval must
                                 * free it before executing next command. */
    int errorLine;		/* When TCL_ERROR is returned, this gives
				 * the line number in the command where the
				 * error occurred (1 means first line). */
    Tcl_Obj *objResultPtr;	/* If the last command returned an object
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Namespace *globalNsPtr;     /* The interpreter's global namespace. */
    Tcl_HashTable mathFuncTable;/* Contains all the math functions currently
				 * defined for the interpreter.  Indexed by
				 * strings (function names); values have
				 * type (MathFunc *). */

    /*
     * Information related to procedures and variables. See tclProc.c
     * and tclvar.c for usage.
     */

    int numLevels;		/* Keeps track of how many nested calls to
				 * Tcl_Eval are in progress for this
				 * interpreter.  It's used to delay deletion
				 * of the table until all Tcl_Eval
				 * invocations are completed. */
    int maxNestingDepth;	/* If numLevels exceeds this value then Tcl
				 * assumes that infinite recursion has
				 * occurred and it generates an error. */
    CallFrame *framePtr;	/* Points to top-most in stack of all nested
				 * procedure invocations.  NULL means there
				 * are no active procedures. */
    CallFrame *varFramePtr;	/* Points to the call frame whose variables
				 * are currently in use (same as framePtr
				 * unless an "uplevel" command is
				 * executing). NULL means no procedure is
				 * active or "uplevel 0" is executing. */
    ActiveVarTrace *activeTracePtr;
				/* First in list of active traces for
				 * interp, or NULL if no active traces. */
    int returnCode;		/* Completion code to return if current
				 * procedure exits with TCL_RETURN code. */
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
    char *evalFirst;		/* If TCL_RECORD_BOUNDS Tcl_Eval and
				 * Tcl_EvalObj set this field to point to
				 * the first char. of text from which the
				 * current command came. Otherwise set to
				 * NULL. */
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
     * Miscellaneous information:
     */

    int cmdCount;		/* Total number of times a command procedure
				 * has been called for this interpreter. */
    int evalFlags;		/* Flags to control next call to Tcl_Eval.
				 * Normally zero, but may be set before
				 * calling Tcl_Eval.  See below for valid
				 * values. */
    int termOffset;		/* Offset of character just after last one
				 * compiled or executed by Tcl_EvalObj. */
    int compileEpoch;		/* Holds the current "compilation epoch"
				 * for this interpreter. This is
				 * incremented to invalidate existing
				 * ByteCodes when, e.g., a command with a
				 * compile procedure is redefined. */
    Proc *compiledProcPtr;	/* If a procedure is being compiled, a
				 * pointer to its Proc structure; otherwise,
				 * this is NULL. Set by ObjInterpProc in
				 * tclProc.c and used by tclCompile.c to
				 * process local variables appropriately. */
    char *scriptFile;		/* NULL means there is no nested source
				 * command active;  otherwise this points to
				 * the name of the file being sourced (it's
				 * not malloc-ed:  it points to an argument
				 * to Tcl_EvalFile. */
    int flags;			/* Various flag bits.  See below. */
    long randSeed;		/* Seed used for rand() function. */
    Trace *tracePtr;		/* List of traces for this interpreter. */
    Tcl_HashTable *assocData;	/* Hash table for associating data with
                                 * this interpreter. Cleaned up when
                                 * this interpreter is deleted. */
    struct ExecEnv *execEnvPtr;	/* Execution environment for Tcl bytecode
                                 * execution. Contains a pointer to the
				 * Tcl evaluation stack. */
    Tcl_Obj *emptyObjPtr;	/* Points to an object holding an empty
				 * string. Returned by Tcl_ObjSetVar2 when
				 * variable traces change a variable in a
				 * gross way. */
    char resultSpace[TCL_RESULT_SIZE+1];
				/* Static space holding small results. */
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

#define TCL_BRACKET_TERM	  1
#define TCL_RECORD_BOUNDS	  2
#define TCL_ALLOW_EXCEPTIONS	  4

/*
 * Flag bits for Interp structures:
 *
 * DELETED:		Non-zero means the interpreter has been deleted:
 *			don't process any more commands for it, and destroy
 *			the structure as soon as all nested invocations of
 *			Tcl_Eval are done.
 * ERR_IN_PROGRESS:	Non-zero means an error unwind is already in
 *			progress. Zero means a command proc has been
 *			invoked since last error occured.
 * ERR_ALREADY_LOGGED:	Non-zero means information has already been logged
 *			in $errorInfo for the current Tcl_Eval instance,
 *			so Tcl_Eval needn't log it (used to implement the
 *			"error message log" command).
 * ERROR_CODE_SET:	Non-zero means that Tcl_SetErrorCode has been
 *			called to record information for the current
 *			error.  Zero means Tcl_Eval must clear the
 *			errorCode variable if an error is returned.
 * EXPR_INITIALIZED:	Non-zero means initialization specific to
 *			expressions has	been carried out.
 * DONT_COMPILE_CMDS_INLINE: Non-zero means that the bytecode compiler
 *			should not compile any commands into an inline
 *			sequence of instructions. This is set 1, for
 *			example, when command traces are requested.
 * RAND_SEED_INITIALIZED: Non-zero means that the randSeed value of the
 *			interp has not be initialized.  This is set 1
 *			when we first use the rand() or srand() functions.
 */

#define DELETED			 1
#define ERR_IN_PROGRESS		 2
#define ERR_ALREADY_LOGGED	 4
#define ERROR_CODE_SET		 8
#define EXPR_INITIALIZED	 0x10
#define DONT_COMPILE_CMDS_INLINE 0x20
#define RAND_SEED_INITIALIZED	 0x40

/*
 *----------------------------------------------------------------
 * Data structures related to command parsing. These are used in
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
 * value.  The CHAR_TYPE macro takes a pointer to a signed character
 * and a pointer to the last character in the source string.  If the
 * src pointer is pointing at the terminating null of the string,
 * CHAR_TYPE returns TCL_COMMAND_END.
 */

extern unsigned char tclTypeTable[];
#define CHAR_TYPE(src,last) \
	(((src)==(last))?TCL_COMMAND_END:(tclTypeTable+128)[*(src)])

/*
 * Possible values returned by CHAR_TYPE. Note that except for TCL_DOLLAR,
 * these are all one byte values with a single bit set 1. This means these
 * values may be bit-or'ed together (except for TCL_DOLLAR) to quickly test
 * whether a character is one of several different kinds of characters.
 *
 * TCL_NORMAL -		All characters that don't have special significance
 *			to the Tcl language.
 * TCL_SPACE -		Character is space, tab, or return.
 * TCL_COMMAND_END -	Character is newline or semicolon or close-bracket
 *			or terminating null.
 * TCL_QUOTE -		Character is a double-quote.
 * TCL_OPEN_BRACKET -	Character is a "[".
 * TCL_OPEN_BRACE -	Character is a "{".
 * TCL_CLOSE_BRACE -	Character is a "}".
 * TCL_BACKSLASH -	Character is a "\".
 * TCL_DOLLAR - 	Character is a "$".
 */

#define TCL_NORMAL		0x01
#define TCL_SPACE		0x02
#define TCL_COMMAND_END		0x04
#define TCL_QUOTE		0x08
#define TCL_OPEN_BRACKET	0x10
#define TCL_OPEN_BRACE		0x20
#define TCL_CLOSE_BRACE		0x40
#define TCL_BACKSLASH		0x80
#define TCL_DOLLAR		0x00

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
 * This macro is used to determine the offset needed to safely allocate any
 * data structure in memory. Given a starting offset or size, it "rounds up"
 * or "aligns" the offset to the next 8-byte boundary so that any data
 * structure can be placed at the resulting offset without fear of an
 * alignment error.
 *
 * WARNING!! DO NOT USE THIS MACRO TO ALIGN POINTERS: it will produce
 * the wrong result on platforms that allocate addresses that are divisible
 * by 4 or 2. Only use it for offsets or sizes.
 */

#define TCL_ALIGN(x) (((int)(x) + 7) & ~7)

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
 * Flags for TclInvoke:
 *
 * TCL_INVOKE_HIDDEN		Invoke a hidden command; if not set,
 *				invokes an exposed command.
 * TCL_INVOKE_NO_UNKNOWN	If set, "unknown" is not invoked if
 *				the command to be invoked is not found.
 *				Only has an effect if invoking an exposed
 *				command, i.e. if TCL_INVOKE_HIDDEN is not
 *				also set.
 */

#define	TCL_INVOKE_HIDDEN	(1<<0)
#define TCL_INVOKE_NO_UNKNOWN	(1<<1)

/*
 * The structure used as the internal representation of Tcl list
 * objects. This is an array of pointers to the element objects. This array
 * is grown (reallocated and copied) as necessary to hold all the list's
 * element pointers. The array might contain more slots than currently used
 * to hold all element pointers. This is done to make append operations
 * faster.
 */

typedef struct List {
    int maxElemCount;		/* Total number of element array slots. */
    int elemCount;		/* Current number of list elements. */
    Tcl_Obj **elements;		/* Array of pointers to element objects. */
} List;

/*
 * The following types are used for getting and storing platform-specific
 * file attributes in tclFCmd.c and the various platform-versions of
 * that file. This is done to have as much common code as possible
 * in the file attributes code. For more information about the callbacks,
 * see TclFileAttrsCmd in tclFCmd.c.
 */

typedef int (TclGetFileAttrProc) _ANSI_ARGS_((Tcl_Interp *interp,
	int objIndex, char *fileName, 
	Tcl_Obj **attrObjPtrPtr));
typedef int (TclSetFileAttrProc) _ANSI_ARGS_((Tcl_Interp *interp,
	int objIndex, char *fileName, 
	Tcl_Obj *attrObjPtr));

typedef struct TclFileAttrProcs {
    TclGetFileAttrProc *getProc; 	/* The procedure for getting attrs. */
    TclSetFileAttrProc *setProc;	/* The procedure for setting attrs. */
} TclFileAttrProcs;

/*
 * Opaque handle used in pipeline routines to encapsulate platform-dependent
 * state. 
 */

typedef struct TclFile_ *TclFile;
    
/*
 *----------------------------------------------------------------
 * Variables shared among Tcl modules but not used by the outside world.
 *----------------------------------------------------------------
 */

extern Tcl_Time			tclBlockTime;
extern int			tclBlockTimeSet;
extern char *			tclExecutableName;
extern Tcl_ChannelType	 	tclFileChannelType;
extern char *			tclMemDumpFileName;
extern TclPlatformType		tclPlatform;
extern char *			tclpFileAttrStrings[];
extern CONST TclFileAttrProcs   tclpFileAttrProcs[];

/*
 * Variables denoting the Tcl object types defined in the core.
 */

extern Tcl_ObjType	tclBooleanType;
extern Tcl_ObjType	tclByteCodeType;
extern Tcl_ObjType	tclDoubleType;
extern Tcl_ObjType	tclIntType;
extern Tcl_ObjType	tclListType;
extern Tcl_ObjType	tclStringType;

/*
 * The head of the list of free Tcl objects, and the total number of Tcl
 * objects ever allocated and freed.
 */

extern Tcl_Obj *	tclFreeObjList;

#ifdef TCL_COMPILE_STATS
extern long 		tclObjsAlloced;
extern long 		tclObjsFreed;
#endif /* TCL_COMPILE_STATS */

/*
 * Pointer to a heap-allocated string of length zero that the Tcl core uses
 * as the value of an empty string representation for an object. This value
 * is shared by all new objects allocated by Tcl_NewObj.
 */

extern char *		tclEmptyStringRep;

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl modules but not used by the outside
 * world:
 *----------------------------------------------------------------
 */

EXTERN void		panic _ANSI_ARGS_(TCL_VARARGS(char *,format));
EXTERN void		TclAllocateFreeObjects _ANSI_ARGS_((void));
EXTERN int		TclChdir _ANSI_ARGS_((Tcl_Interp *interp,
			    char *dirName));
EXTERN int		TclCleanupChildren _ANSI_ARGS_((Tcl_Interp *interp,
		            int numPids, Tcl_Pid *pidPtr,
			    Tcl_Channel errorChan));
EXTERN void		TclCleanupCommand _ANSI_ARGS_((Command *cmdPtr));
EXTERN char *		TclConvertToNative _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_DString *bufferPtr));
EXTERN char *		TclConvertToNetwork _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_DString *bufferPtr));
EXTERN int		TclCopyAndCollapse _ANSI_ARGS_((int count,
			    char *src, char *dst));
EXTERN int		TclCopyChannel _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Channel inChan, Tcl_Channel outChan,
			    int toRead, Tcl_Obj *cmdPtr));
/*
 * TclCreatePipeline unofficially exported for use by BLT.
 */
EXTERN int		TclCreatePipeline _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, Tcl_Pid **pidArrayPtr,
			    TclFile *inPipePtr, TclFile *outPipePtr,
			    TclFile *errFilePtr));
EXTERN int		TclCreateProc _ANSI_ARGS_((Tcl_Interp *interp,
			    Namespace *nsPtr, char *procName,
			    Tcl_Obj *argsPtr, Tcl_Obj *bodyPtr,
			    Proc **procPtrPtr));
EXTERN void		TclDeleteCompiledLocalVars _ANSI_ARGS_((
    			    Interp *iPtr, CallFrame *framePtr));
EXTERN void		TclDeleteVars _ANSI_ARGS_((Interp *iPtr,
			    Tcl_HashTable *tablePtr));
EXTERN int		TclDoGlob _ANSI_ARGS_((Tcl_Interp *interp,
			    char *separators, Tcl_DString *headPtr,
			    char *tail));
EXTERN void		TclDumpMemoryInfo _ANSI_ARGS_((FILE *outFile));
EXTERN void		TclExpandParseValue _ANSI_ARGS_((ParseValue *pvPtr,
			    int needed));
EXTERN void		TclExprFloatError _ANSI_ARGS_((Tcl_Interp *interp,
			    double value));
EXTERN int		TclFileAttrsCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
EXTERN int		TclFileCopyCmd _ANSI_ARGS_((Tcl_Interp *interp, 
			    int argc, char **argv)) ;
EXTERN int 		TclFileDeleteCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv));
EXTERN int		TclFileMakeDirsCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv)) ;
EXTERN int		TclFileRenameCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv)) ;
EXTERN void		TclFinalizeCompExecEnv _ANSI_ARGS_((void));
EXTERN void		TclFinalizeExecEnv _ANSI_ARGS_((void));
EXTERN int		TclFindElement _ANSI_ARGS_((Tcl_Interp *interp,
			    char *list, int listLength, char **elementPtr,
			    char **nextPtr, int *sizePtr, int *bracePtr));
EXTERN Proc *		TclFindProc _ANSI_ARGS_((Interp *iPtr,
			    char *procName));
EXTERN int		TclFormatInt _ANSI_ARGS_((char *buffer, long n));
EXTERN void		TclFreePackageInfo _ANSI_ARGS_((Interp *iPtr));
EXTERN void		TclGetAndDetachPids _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Channel chan));
EXTERN char *		TclGetCwd _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclGetDate _ANSI_ARGS_((char *p,
			    unsigned long now, long zone,
			    unsigned long *timePtr));
EXTERN Tcl_Channel	TclGetDefaultStdChannel _ANSI_ARGS_((int type));
EXTERN Tcl_Obj *	TclGetElementOfIndexedArray _ANSI_ARGS_((
                            Tcl_Interp *interp, int localIndex,
			    Tcl_Obj *elemPtr, int leaveErrorMsg));
EXTERN char *		TclGetEnv _ANSI_ARGS_((char *name));
EXTERN char *		TclGetExtension _ANSI_ARGS_((char *name));
EXTERN int		TclGetFrame _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, CallFrame **framePtrPtr));
EXTERN int		TclGetIdleGeneration _ANSI_ARGS_((void));
EXTERN int		TclGetIntForIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, int endValue, int *indexPtr));
EXTERN Tcl_Obj *	TclGetIndexedScalar _ANSI_ARGS_((Tcl_Interp *interp,
			    int localIndex, int leaveErrorMsg));
EXTERN int		TclGetLong _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, long *longPtr));
EXTERN int		TclGetLoadedPackages _ANSI_ARGS_((
			    Tcl_Interp *interp, char *targetName));
EXTERN int		TclGetNamespaceForQualName _ANSI_ARGS_((
			    Tcl_Interp *interp, char *qualName,
			    Namespace *cxtNsPtr, int flags,
			    Namespace **nsPtrPtr, Namespace **altNsPtrPtr,
			    Namespace **actualCxtPtrPtr,
			    char **simpleNamePtr));
EXTERN int		TclGetOpenMode _ANSI_ARGS_((Tcl_Interp *interp,
        		    char *string, int *seekFlagPtr));
EXTERN Tcl_Command	TclGetOriginalCommand _ANSI_ARGS_((
			    Tcl_Command command));
EXTERN char *		TclGetUserHome _ANSI_ARGS_((char *name,
			    Tcl_DString *bufferPtr));
EXTERN int		TclGlobalInvoke _ANSI_ARGS_((Tcl_Interp *interp,
		            int argc, char **argv, int flags));
EXTERN int		TclGuessPackageName _ANSI_ARGS_((char *fileName,
			    Tcl_DString *bufPtr));
EXTERN int              TclHasPipes _ANSI_ARGS_((void));
EXTERN int		TclHasSockets _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclHideUnsafeCommands _ANSI_ARGS_((
    			    Tcl_Interp *interp));
EXTERN int		TclIdlePending _ANSI_ARGS_((void));
EXTERN int		TclInExit _ANSI_ARGS_((void));
EXTERN Tcl_Obj *	TclIncrElementOfIndexedArray _ANSI_ARGS_((
                            Tcl_Interp *interp, int localIndex,
			    Tcl_Obj *elemPtr, long incrAmount));
EXTERN Tcl_Obj *	TclIncrIndexedScalar _ANSI_ARGS_((
                            Tcl_Interp *interp, int localIndex,
			    long incrAmount));
EXTERN Tcl_Obj *	TclIncrVar2 _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
			    long incrAmount, int part1NotParsed));
EXTERN void		TclInitNamespaces _ANSI_ARGS_((void));
EXTERN int		TclInterpInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclInvoke _ANSI_ARGS_((Tcl_Interp *interp,
		            int argc, char **argv, int flags));
EXTERN int		TclInvokeObjectCommand _ANSI_ARGS_((
                            ClientData clientData, Tcl_Interp *interp,
                            int argc, char **argv));
EXTERN int		TclInvokeStringCommand _ANSI_ARGS_((
                            ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *CONST objv[]));
EXTERN Proc *		TclIsProc _ANSI_ARGS_((Command *cmdPtr));
EXTERN int		TclLoadFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName, char *sym1, char *sym2,
			    Tcl_PackageInitProc **proc1Ptr,
			    Tcl_PackageInitProc **proc2Ptr));
EXTERN int		TclLooksLikeInt _ANSI_ARGS_((char *p));
EXTERN Var *		TclLookupVar _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, int flags, char *msg,
			    int createPart1, int createPart2,
			    Var **arrayPtrPtr));
EXTERN int		TclMakeFileTable _ANSI_ARGS_((Tcl_Interp *interp,
                            int noStdio));
EXTERN int		TclMatchFiles _ANSI_ARGS_((Tcl_Interp *interp,
			    char *separators, Tcl_DString *dirPtr,
			    char *pattern, char *tail));
EXTERN int		TclNeedSpace _ANSI_ARGS_((char *start, char *end));
EXTERN int		TclObjInterpProc _ANSI_ARGS_((ClientData clientData,
		    	    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
EXTERN int		TclObjInvoke _ANSI_ARGS_((Tcl_Interp *interp,
		            int objc, Tcl_Obj *CONST objv[], int flags));
EXTERN int		TclObjInvokeGlobal _ANSI_ARGS_((Tcl_Interp *interp,
		            int objc, Tcl_Obj *CONST objv[], int flags));
EXTERN char *		TclpAlloc _ANSI_ARGS_((unsigned int size));
EXTERN int		TclpCloseFile _ANSI_ARGS_((TclFile file));
EXTERN int		TclpCopyFile _ANSI_ARGS_((char *source, char *dest));
EXTERN int              TclpCopyDirectory _ANSI_ARGS_((char *source,
			    char *dest, Tcl_DString *errorPtr));
EXTERN Tcl_Channel	TclpCreateCommandChannel _ANSI_ARGS_((
    			    TclFile readFile, TclFile writeFile,
			    TclFile errorFile, int numPids, Tcl_Pid *pidPtr));
EXTERN int              TclpCreateDirectory _ANSI_ARGS_((char *path));
EXTERN int              TclpCreatePipe _ANSI_ARGS_((TclFile *readPipe,
			    TclFile *writePipe));
EXTERN int		TclpCreateProcess _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, TclFile inputFile, 
			    TclFile outputFile, TclFile errorFile,
			    Tcl_Pid *pidPtr));
EXTERN TclFile		TclpCreateTempFile _ANSI_ARGS_((char *contents, 
			    Tcl_DString *namePtr));
EXTERN int              TclpDeleteFile _ANSI_ARGS_((char *path));
EXTERN void		TclpFree _ANSI_ARGS_((char *ptr));
EXTERN unsigned long	TclpGetClicks _ANSI_ARGS_((void));
EXTERN unsigned long	TclpGetSeconds _ANSI_ARGS_((void));
EXTERN void		TclpGetTime _ANSI_ARGS_((Tcl_Time *time));
EXTERN int		TclpGetTimeZone _ANSI_ARGS_((unsigned long time));
EXTERN char *		TclpGetTZName _ANSI_ARGS_((void));
EXTERN TclFile		TclpMakeFile _ANSI_ARGS_((Tcl_Channel channel,
			    int direction));
EXTERN TclFile		TclpOpenFile _ANSI_ARGS_((char *fname, int mode));
EXTERN int		TclpListVolumes _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN char *		TclpRealloc _ANSI_ARGS_((char *ptr,
			    unsigned int size));
EXTERN int              TclpRemoveDirectory _ANSI_ARGS_((char *path,
			    int recursive, Tcl_DString *errorPtr));
EXTERN int              TclpRenameFile _ANSI_ARGS_((char *source, char *dest));
EXTERN int		TclParseBraces _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char **termPtr, ParseValue *pvPtr));
EXTERN int		TclParseNestedCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int flags, char **termPtr,
			    ParseValue *pvPtr));
EXTERN int		TclParseQuotes _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int termChar, int flags,
			    char **termPtr, ParseValue *pvPtr));
EXTERN void		TclPlatformExit _ANSI_ARGS_((int status));
EXTERN void		TclPlatformInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclPreventAliasLoop _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *cmdInterp, Tcl_Command cmd));
EXTERN void		TclPrintByteCodeObj _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Obj *objPtr));
EXTERN int		TclRenameCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *oldName, char *newName)) ;
EXTERN void		TclResetShadowedCmdRefs _ANSI_ARGS_((
			    Tcl_Interp *interp, Command *newCmdPtr));
EXTERN int		TclServiceIdle _ANSI_ARGS_((void));
EXTERN Tcl_Obj *	TclSetElementOfIndexedArray _ANSI_ARGS_((
                            Tcl_Interp *interp, int localIndex,
			    Tcl_Obj *elemPtr, Tcl_Obj *objPtr,
			    int leaveErrorMsg));
EXTERN Tcl_Obj *	TclSetIndexedScalar _ANSI_ARGS_((Tcl_Interp *interp,
			    int localIndex, Tcl_Obj *objPtr,
			    int leaveErrorMsg));
EXTERN void		TclSetupEnv _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TclSockGetPort _ANSI_ARGS_((Tcl_Interp *interp,
		            char *string, char *proto, int *portPtr));
EXTERN int		TclSockMinimumBuffers _ANSI_ARGS_((int sock,
        		    int size));
EXTERN void		TclTeardownNamespace _ANSI_ARGS_((Namespace *nsPtr));
EXTERN int		TclTestChannelCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		TclTestChannelEventCmd _ANSI_ARGS_((
    			    ClientData clientData, Tcl_Interp *interp,
                            int argc, char **argv));
EXTERN int		TclUpdateReturnInfo _ANSI_ARGS_((Interp *iPtr));
EXTERN char *		TclWordEnd _ANSI_ARGS_((char *start, char *lastChar,
			    int nested, int *semiPtr));

/*
 *----------------------------------------------------------------
 * Command procedures in the generic core:
 *----------------------------------------------------------------
 */

EXTERN int	Tcl_AfterCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_AppendObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ArrayObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_BinaryObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_BreakCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_CaseObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CatchObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CdCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ClockObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CloseCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ConcatObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ContinueCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_EofCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ErrorObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_EvalObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ExecCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ExitObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ExprObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FblockedCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FconfigureCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FcopyObjCmd _ANSI_ARGS_((ClientData dummy,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FileObjCmd _ANSI_ARGS_((ClientData dummy,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FileEventCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_FlushObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ForCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ForeachObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FormatCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_GetsObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_GlobalObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_GlobCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_HistoryCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_IfCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_IncrCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_InfoObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_InterpObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_JoinObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LappendObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LindexObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LinsertObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LlengthObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ListObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LoadCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_LrangeObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LreplaceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LsearchObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LsortObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_NamespaceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_OpenCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_PackageCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_PidObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ProcObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_PutsObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_PwdCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_ReadObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_RegexpCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_RegsubCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_RenameObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ReturnObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
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
EXTERN int	Tcl_SourceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_StringObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SubstCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_SwitchObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_TellCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_TimeObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_TraceCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_UnsetObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_UpdateCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_UplevelObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_UpvarObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_VariableObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_VwaitCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int	Tcl_WhileCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

/*
 *----------------------------------------------------------------
 * Command procedures found only in the Mac version of the core:
 *----------------------------------------------------------------
 */

#ifdef MAC_TCL
EXTERN int 	Tcl_EchoCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_LsCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
EXTERN int 	Tcl_BeepObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int 	Tcl_MacSourceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ResourceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
#endif

/*
 *----------------------------------------------------------------
 * Compilation procedures for commands in the generic core:
 *----------------------------------------------------------------
 */

EXTERN int	TclCompileBreakCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileCatchCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileContinueCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileExprCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileForCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileForeachCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileIfCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileIncrCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileSetCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));
EXTERN int	TclCompileWhileCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    char *string, char *lastChar, int compileFlags,
		    struct CompileEnv *compileEnvPtr));

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to create and release Tcl objects.
 * TclNewObj(objPtr) creates a new object denoting an empty string.
 * TclDecrRefCount(objPtr) decrements the object's reference count,
 * and frees the object if its reference count is zero.
 * These macros are inline versions of Tcl_NewObj() and
 * Tcl_DecrRefCount(). Notice that the names differ in not having
 * a "_" after the "Tcl". Notice also that these macros reference
 * their argument more than once, so you should avoid calling them
 * with an expression that is expensive to compute or has
 * side effects. The ANSI C "prototypes" for these macros are:
 *
 * EXTERN void	TclNewObj _ANSI_ARGS_((Tcl_Obj *objPtr));
 * EXTERN void	TclDecrRefCount _ANSI_ARGS_((Tcl_Obj *objPtr));
 *----------------------------------------------------------------
 */

#ifdef TCL_COMPILE_STATS
#  define TclIncrObjsAllocated() \
    tclObjsAlloced++
#  define TclIncrObjsFreed() \
    tclObjsFreed++
#else
#  define TclIncrObjsAllocated()
#  define TclIncrObjsFreed()
#endif /* TCL_COMPILE_STATS */

#ifdef TCL_MEM_DEBUG
#  define TclNewObj(objPtr) \
    (objPtr) = (Tcl_Obj *) Tcl_DbCkalloc(sizeof(Tcl_Obj), __FILE__, __LINE__); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes    = tclEmptyStringRep; \
    (objPtr)->length   = 0; \
    (objPtr)->typePtr  = NULL; \
    TclIncrObjsAllocated()
#  define TclDbNewObj(objPtr, file, line) \
    (objPtr) = (Tcl_Obj *) Tcl_DbCkalloc(sizeof(Tcl_Obj), (file), (line)); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes    = tclEmptyStringRep; \
    (objPtr)->length   = 0; \
    (objPtr)->typePtr  = NULL; \
    TclIncrObjsAllocated()
#  define TclDecrRefCount(objPtr) \
    if (--(objPtr)->refCount <= 0) { \
 	if ((objPtr)->refCount < -1) \
            panic("Reference count for %lx was negative: %s line %d", \
		  (objPtr), __FILE__, __LINE__); \
        if (((objPtr)->bytes != NULL) \
	        && ((objPtr)->bytes != tclEmptyStringRep)) { \
	    ckfree((char *) (objPtr)->bytes); \
        } \
        if (((objPtr)->typePtr != NULL) \
	        && ((objPtr)->typePtr->freeIntRepProc != NULL)) { \
	    (objPtr)->typePtr->freeIntRepProc(objPtr); \
        } \
        ckfree((char *) (objPtr)); \
        TclIncrObjsFreed(); \
    }
#else /* not TCL_MEM_DEBUG */
#  define TclNewObj(objPtr) \
    if (tclFreeObjList == NULL) { \
	TclAllocateFreeObjects(); \
    } \
    (objPtr) = tclFreeObjList; \
    tclFreeObjList = (Tcl_Obj *) \
	tclFreeObjList->internalRep.otherValuePtr; \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes    = tclEmptyStringRep; \
    (objPtr)->length   = 0; \
    (objPtr)->typePtr  = NULL; \
    TclIncrObjsAllocated()
#  define TclDecrRefCount(objPtr) \
    if (--(objPtr)->refCount <= 0) { \
        if (((objPtr)->bytes != NULL) \
	        && ((objPtr)->bytes != tclEmptyStringRep)) { \
	    ckfree((char *) (objPtr)->bytes); \
        } \
        if (((objPtr)->typePtr != NULL) \
	        && ((objPtr)->typePtr->freeIntRepProc != NULL)) { \
	    (objPtr)->typePtr->freeIntRepProc(objPtr); \
        } \
        (objPtr)->internalRep.otherValuePtr = (VOID *) tclFreeObjList; \
        tclFreeObjList = (objPtr); \
        TclIncrObjsFreed(); \
    }
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to set a Tcl_Obj's string representation
 * to a copy of the "len" bytes starting at "bytePtr". This code
 * works even if the byte array contains NULLs as long as the length
 * is correct. Because "len" is referenced multiple times, it should
 * be as simple an expression as possible. The ANSI C "prototype" for
 * this macro is:
 *
 * EXTERN void	TclInitStringRep _ANSI_ARGS_((Tcl_Obj *objPtr,
 *		    char *bytePtr, int len));
 *----------------------------------------------------------------
 */

#define TclInitStringRep(objPtr, bytePtr, len) \
    if ((len) == 0) { \
        (objPtr)->bytes  = tclEmptyStringRep; \
	(objPtr)->length = 0; \
    } else { \
	(objPtr)->bytes = (char *) ckalloc((unsigned) ((len) + 1)); \
	memcpy((VOID *) (objPtr)->bytes, (VOID *) (bytePtr), \
	        (unsigned) (len)); \
	(objPtr)->bytes[len] = '\0'; \
	(objPtr)->length = (len); \
    }

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to get the string representation's
 * byte array pointer and length from a Tcl_Obj. This is an inline
 * version of Tcl_GetStringFromObj(). "lengthPtr" must be the
 * address of an integer variable or NULL; If non-NULL, that variable
 * will be set to the string rep's length. The macro's expression
 * result is the string rep's byte pointer which might be NULL.
 * Note that the bytes referenced by this pointer must not be modified
 * by the caller. The ANSI C "prototype" for this macro is:
 *
 * EXTERN char *  TclGetStringFromObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *		       int *lengthPtr));
 *----------------------------------------------------------------
 */

#define TclGetStringFromObj(objPtr, lengthPtr) \
    ((objPtr)->bytes? \
        ((lengthPtr)? \
	    ((*(lengthPtr) = (objPtr)->length), (objPtr)->bytes) : \
	    (objPtr)->bytes) : \
        Tcl_GetStringFromObj((objPtr), (lengthPtr)))

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to reset an interpreter's Tcl object
 * result to an unshared empty string object with ref count one.
 * This does not clear any error information for the interpreter.
 * The ANSI C "prototype" for this macro is:
 *
 * EXTERN void	TclResetObjResult _ANSI_ARGS_((Tcl_Interp *interp));
 *---------------------------------------------------------------
 */

#define TclResetObjResult(interp) \
    { \
        register Tcl_Obj *objResultPtr = ((Interp *) interp)->objResultPtr; \
        if (Tcl_IsShared(objResultPtr)) { \
	    TclDecrRefCount(objResultPtr); \
	    TclNewObj(objResultPtr); \
	    Tcl_IncrRefCount(objResultPtr); \
	    ((Interp *) interp)->objResultPtr = objResultPtr; \
        } else { \
	    if ((objResultPtr->bytes != NULL) \
		    && (objResultPtr->bytes != tclEmptyStringRep)) { \
	        ckfree((char *) objResultPtr->bytes); \
	    } \
	    objResultPtr->bytes  = tclEmptyStringRep; \
	    objResultPtr->length = 0; \
	    if ((objResultPtr->typePtr != NULL) \
	            && (objResultPtr->typePtr->freeIntRepProc != NULL)) { \
	        objResultPtr->typePtr->freeIntRepProc(objResultPtr); \
	    } \
	    objResultPtr->typePtr = (Tcl_ObjType *) NULL; \
        } \
    }

/*
 *----------------------------------------------------------------
 * Procedures used in conjunction with Tcl namespaces. They are
 * defined here instead of in tcl.h since they are not stable yet.
 *----------------------------------------------------------------
 */

EXTERN int		Tcl_AppendExportList _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Namespace *nsPtr,
			    Tcl_Obj *objPtr));
EXTERN Tcl_Namespace *	Tcl_CreateNamespace _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, ClientData clientData,
			    Tcl_NamespaceDeleteProc *deleteProc));
EXTERN void		Tcl_DeleteNamespace _ANSI_ARGS_((
			    Tcl_Namespace *nsPtr));
EXTERN int		Tcl_Export _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Namespace *nsPtr, char *pattern,
			    int resetListFirst));
EXTERN Tcl_Command	Tcl_FindCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_Namespace *contextNsPtr,
			    int flags));
EXTERN Tcl_Namespace *	Tcl_FindNamespace _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, Tcl_Namespace *contextNsPtr,
			    int flags));
EXTERN Tcl_Var		Tcl_FindNamespaceVar _ANSI_ARGS_((
			    Tcl_Interp *interp, char *name,
			    Tcl_Namespace *contextNsPtr, int flags));
EXTERN int		Tcl_ForgetImport _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Namespace *nsPtr, char *pattern));
EXTERN Tcl_Command	Tcl_GetCommandFromObj _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr));
EXTERN void		Tcl_GetCommandFullName _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Command command,
			    Tcl_Obj *objPtr));
EXTERN Tcl_Namespace *	Tcl_GetCurrentNamespace _ANSI_ARGS_((
			    Tcl_Interp *interp));
EXTERN Tcl_Namespace *	Tcl_GetGlobalNamespace _ANSI_ARGS_((
			    Tcl_Interp *interp));
EXTERN void		Tcl_GetVariableFullName _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Var variable,
			    Tcl_Obj *objPtr));
EXTERN int		Tcl_Import _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Namespace *nsPtr, char *pattern,
			    int allowOverwrite));
EXTERN void		Tcl_PopCallFrame _ANSI_ARGS_((Tcl_Interp* interp));
EXTERN int		Tcl_PushCallFrame _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_CallFrame *framePtr, Tcl_Namespace *nsPtr,
			    int isProcCallFrame)); 

#endif /* _TCLINT */

