/* 
 * tclVar.c --
 *
 *	This file contains routines that implement Tcl variables
 *	(both scalars and arrays).
 *
 *	The implementation of arrays is modelled after an initial
 *	implementation by Mark Diekhans and Karl Lehenbauer.
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclVar.c 1.69 96/02/28 21:45:10
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The strings below are used to indicate what went wrong when a
 * variable access is denied.
 */

static char *noSuchVar =	"no such variable";
static char *isArray =		"variable is array";
static char *needArray =	"variable isn't array";
static char *noSuchElement =	"no such element in array";
static char *danglingUpvar =	"upvar refers to element in deleted array";

/*
 * Creation flag values passed in to LookupVar:
 *
 * CRT_PART1 -		1 means create hash table entry for part 1 of
 *			name, if it doesn't already exist.  0 means
 *			return an error if it doesn't exist.
 * CRT_PART2 -		1 means create hash table entry for part 2 of
 *			name, if it doesn't already exist.  0 means
 *			return an error if it doesn't exist.
 */

#define CRT_PART1	1
#define CRT_PART2	2

/*
 * The following additional flag is used internally and passed through
 * to LookupVar to indicate that a procedure like Tcl_GetVar was called
 * instead of Tcl_GetVar2 and the single name value hasn't yet been
 * parsed into an array name and index (if any).
 */

#define PART1_NOT_PARSED	0x10000

/*
 * Forward references to procedures defined later in this file:
 */

static  char *		CallTraces _ANSI_ARGS_((Interp *iPtr, Var *arrayPtr,
			    Var *varPtr, char *part1, char *part2,
			    int flags));
static void		CleanupVar _ANSI_ARGS_((Var *varPtr, Var *arrayPtr));
static void		DeleteSearches _ANSI_ARGS_((Var *arrayVarPtr));
static void		DeleteArray _ANSI_ARGS_((Interp *iPtr, char *arrayName,
			    Var *varPtr, int flags));
static Var *		LookupVar _ANSI_ARGS_((Tcl_Interp *interp, char *part1,
			    char *part2, int flags, char *msg, int create,
			    Var **arrayPtrPtr));
static int		MakeUpvar _ANSI_ARGS_((Interp *iPtr,
			    CallFrame *framePtr, char *otherP1,
			    char *otherP2, char *myName, int flags));
static Var *		NewVar _ANSI_ARGS_((void));
static ArraySearch *	ParseSearchId _ANSI_ARGS_((Tcl_Interp *interp,
			    Var *varPtr, char *varName, char *string));
static void		VarErrMsg _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, char *operation,
			    char *reason));

/*
 *----------------------------------------------------------------------
 *
 * LookupVar --
 *
 *	This procedure is used by virtually all of the variable
 *	code to locate a variable given its name(s).
 *
 * Results:
 *	The return value is a pointer to the variable indicated by
 *	part1 and part2, or NULL if the variable couldn't be found.
 *	If the variable is found, *arrayPtrPtr is filled in with
 *	the address of the array that contains the variable (or NULL
 *	if the variable is a scalar).  Note:  it's possible that the
 *	variable returned may be VAR_UNDEFINED, even if CRT_PART1 and
 *	CRT_PART2 are specified (these only cause the hash table entry
 *	and/or array to be created).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Var *
LookupVar(interp, part1, part2, flags, msg, create, arrayPtrPtr)
    Tcl_Interp *interp;		/* Interpreter to use for lookup. */
    char *part1;		/* If part2 isn't NULL, this is the name
				 * of an array.  Otherwise, if the
				 * PART1_NOT_PARSED flag bit is set this
				 * is a full variable name that could
				 * include a parenthesized array elemnt.
				 * If PART1_NOT_PARSED isn't present, then
				 * this is the name of a scalar variable. */
    char *part2;		/* Name of an element within array, or NULL. */
    int flags;			/* Only the TCL_GLOBAL_ONLY, TCL_LEAVE_ERR_MSG,
				 * and PART1_NOT_PARSED bits matter. */
    char *msg;			/* Verb to use in error messages, e.g.
				 * "read" or "set".  Only needed if
				 * TCL_LEAVE_ERR_MSG is set in flags. */
    int create;			/* OR'ed combination of CRT_PART1 and
				 * CRT_PART2.  Tells which entries to create
				 * if they don't already exist. */
    Var **arrayPtrPtr;		/* If the name refers to an element of an
				 * array, *arrayPtrPtr gets filled in with
				 * address of array variable.  Otherwise
				 * this is set to NULL. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashTable *tablePtr;
    Tcl_HashEntry *hPtr;
    Var *varPtr;
    int new;
    char *openParen, *closeParen;	/* If this procedure parses a name
					 * into array and index, these point
					 * to the parens around the index.
					 * Otherwise they are NULL.  These
					 * are needed to restore the parens
					 * after parsing the name. */
    char *elName;			/* Name of array element or NULL;
					 * may be same as part2, or may be
					 * openParen+1. */
    char *p;

    /*
     * If the name hasn't been parsed into array name and index yet,
     * do it now.
     */

    openParen = closeParen = NULL;
    elName = part2;
    if (flags & PART1_NOT_PARSED) {
	for (p = part1; ; p++) {
	    if (*p == 0) {
		elName = NULL;
		break;
	    }
	    if (*p == '(') {
		openParen = p;
		do {
		    p++;
		} while (*p != '\0');
		p--;
		if (*p == ')') {
		    closeParen = p;
		    *openParen = 0;
		    elName = openParen+1;
		} else {
		    openParen = NULL;
		    elName = NULL;
		}
		break;
	    }
	}
    }

    /*
     * Lookup part1.
     */

    *arrayPtrPtr = NULL;
    if ((flags & TCL_GLOBAL_ONLY) || (iPtr->varFramePtr == NULL)) {
	tablePtr = &iPtr->globalTable;
    } else {
	tablePtr = &iPtr->varFramePtr->varTable;
    }
    if (create & CRT_PART1) {
	hPtr = Tcl_CreateHashEntry(tablePtr, part1, &new);
	if (openParen != NULL) {
	    *openParen = '(';
	}
	if (new) {
	    varPtr = NewVar();
	    Tcl_SetHashValue(hPtr, varPtr);
	    varPtr->hPtr = hPtr;
	}
    } else {
	hPtr = Tcl_FindHashEntry(tablePtr, part1);
	if (openParen != NULL) {
	    *openParen = '(';
	}
	if (hPtr == NULL) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, msg, noSuchVar);
	    }
	    return NULL;
	}
    }
    varPtr = (Var *) Tcl_GetHashValue(hPtr);
    if (varPtr->flags & VAR_UPVAR) {
	varPtr = varPtr->value.upvarPtr;
    }

    if (elName == NULL) {
	return varPtr;
    }

    /*
     * We're dealing with an array element, so make sure the variable
     * is an array and lookup the element (create it if desired).
     */

    if (varPtr->flags & VAR_UNDEFINED) {
	if (!(create & CRT_PART1)) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, msg, noSuchVar);
	    }
	    return NULL;
	}
	varPtr->flags = VAR_ARRAY;
	varPtr->value.tablePtr = (Tcl_HashTable *)
		ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(varPtr->value.tablePtr, TCL_STRING_KEYS);
    } else if (!(varPtr->flags & VAR_ARRAY)) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, msg, needArray);
	}
	return NULL;
    }
    *arrayPtrPtr = varPtr;
    if (closeParen != NULL) {
	*closeParen = 0;
    }
    if (create & CRT_PART2) {
	hPtr = Tcl_CreateHashEntry(varPtr->value.tablePtr, elName, &new);
	if (closeParen != NULL) {
	    *closeParen = ')';
	}
	if (new) {
	    if (varPtr->searchPtr != NULL) {
		DeleteSearches(varPtr);
	    }
	    varPtr = NewVar();
	    Tcl_SetHashValue(hPtr, varPtr);
	    varPtr->hPtr = hPtr;
	}
    } else {
	hPtr = Tcl_FindHashEntry(varPtr->value.tablePtr, elName);
	if (closeParen != NULL) {
	    *closeParen = ')';
	}
	if (hPtr == NULL) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, msg, noSuchElement);
	    }
	    return NULL;
	}
    }
    return (Var *) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetVar --
 *
 *	Return the value of a Tcl variable.
 *
 * Results:
 *	The return value points to the current value of varName.  If
 *	the variable is not defined or can't be read because of a clash
 *	in array usage then a NULL pointer is returned and an error
 *	message is left in interp->result if the TCL_LEAVE_ERR_MSG
 *	flag is set.  Note:  the return value is only valid up until
 *	the next call to Tcl_SetVar or Tcl_SetVar2;  if you depend on
 *	the value lasting longer than that, then make yourself a private
 *	copy.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetVar(interp, varName, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *varName;		/* Name of a variable in interp. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY
				 * or TCL_LEAVE_ERR_MSG bits. */
{
    return Tcl_GetVar2(interp, varName, (char *) NULL,
	    flags | PART1_NOT_PARSED);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetVar2 --
 *
 *	Return the value of a Tcl variable, given a two-part name
 *	consisting of array name and element within array.
 *
 * Results:
 *	The return value points to the current value of the variable
 *	given by part1 and part2.  If the specified variable doesn't
 *	exist, or if there is a clash in array usage, then NULL is
 *	returned and a message will be left in interp->result if the
 *	TCL_LEAVE_ERR_MSG flag is set.  Note:  the return value is
 *	only valid up until the next call to Tcl_SetVar or Tcl_SetVar2;
 *	if you depend on the value lasting longer than that, then make
 *	yourself a private copy.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetVar2(interp, part1, part2, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    char *part1;		/* Name of array (if part2 is NULL) or
				 * name of variable. */
    char *part2;		/* If non-null, gives name of element in
				 * array. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY,
				 * TCL_LEAVE_ERR_MSG, and PART1_NOT_PARSED
				 * bits. */
{
    Var *varPtr, *arrayPtr;
    Interp *iPtr = (Interp *) interp;

    varPtr = LookupVar(interp, part1, part2, flags, "read", CRT_PART2,
	    &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }

    /*
     * Invoke any traces that have been set for the variable.
     */

    if ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	char *msg;

	msg = CallTraces(iPtr, arrayPtr, varPtr, part1, part2,
		(flags & (TCL_GLOBAL_ONLY|PART1_NOT_PARSED)) | TCL_TRACE_READS);
	if (msg != NULL) {
	    VarErrMsg(interp, part1, part2, "read", msg);
	    goto cleanup;
	}
    }
    if (!(varPtr->flags & (VAR_UNDEFINED|VAR_UPVAR|VAR_ARRAY))) {
	return varPtr->value.string;
    }
    if (flags & TCL_LEAVE_ERR_MSG) {
	char *msg;

	if ((varPtr->flags & VAR_UNDEFINED) && (arrayPtr != NULL)
		&& !(arrayPtr->flags & VAR_UNDEFINED)) {
	    msg = noSuchElement;
	} else if (varPtr->flags & VAR_ARRAY) {
	    msg = isArray;
	} else {
	    msg = noSuchVar;
	}
	VarErrMsg(interp, part1, part2, "read", msg);
    }

    /*
     * If the variable doesn't exist anymore and no-one's using it,
     * then free up the relevant structures and hash table entries.
     */

    cleanup:
    if (varPtr->flags & VAR_UNDEFINED) {
	CleanupVar(varPtr, arrayPtr);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetVar --
 *
 *	Change the value of a variable.
 *
 * Results:
 *	Returns a pointer to the malloc'ed string holding the new
 *	value of the variable.  The caller should not modify this
 *	string.  If the write operation was disallowed then NULL
 *	is returned;  if the TCL_LEAVE_ERR_MSG flag is set, then
 *	an explanatory message will be left in interp->result.
 *
 * Side effects:
 *	If varName is defined as a local or global variable in interp,
 *	its value is changed to newValue.  If varName isn't currently
 *	defined, then a new global variable by that name is created.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_SetVar(interp, varName, newValue, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *varName;		/* Name of a variable in interp. */
    char *newValue;		/* New value for varName. */
    int flags;			/* Various flags that tell how to set value:
				 * any of TCL_GLOBAL_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT, or TCL_LEAVE_ERR_MSG. */
{
    return Tcl_SetVar2(interp, varName, (char *) NULL, newValue,
	    flags | PART1_NOT_PARSED);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetVar2 --
 *
 *	Given a two-part variable name, which may refer either to a
 *	scalar variable or an element of an array, change the value
 *	of the variable.  If the named scalar or array or element
 *	doesn't exist then create one.
 *
 * Results:
 *	Returns a pointer to the malloc'ed string holding the new
 *	value of the variable.  The caller should not modify this
 *	string.  If the write operation was disallowed because an
 *	array was expected but not found (or vice versa), then NULL
 *	is returned;  if the TCL_LEAVE_ERR_MSG flag is set, then
 *	an explanatory message will be left in interp->result.
 *
 * Side effects:
 *	The value of the given variable is set.  If either the array
 *	or the entry didn't exist then a new one is created.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_SetVar2(interp, part1, part2, newValue, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    char *part1;		/* If part2 is NULL, this is name of scalar
				 * variable.  Otherwise it is name of array. */
    char *part2;		/* Name of an element within array, or NULL. */
    char *newValue;		/* New value for variable. */
    int flags;			/* Various flags that tell how to set value:
				 * any of TCL_GLOBAL_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT, TCL_LEAVE_ERR_MSG, or
				 * PART1_NOT_PARSED. */
{
    register Var *varPtr;
    register Interp *iPtr = (Interp *) interp;
    int length, listFlags;
    Var *arrayPtr;
    char *result;

    varPtr = LookupVar(interp, part1, part2, flags, "set", CRT_PART1|CRT_PART2,
	    &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }

    /*
     * If the variable's hPtr field is NULL, it means that this is an
     * upvar to an array element where the array was deleted, leaving
     * the element dangling at the end of the upvar.  Generate an error
     * (allowing the variable to be reset would screw up our storage
     * allocation and is meaningless anyway).
     */

    if (varPtr->hPtr == NULL) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, "set", danglingUpvar);
	}
	return NULL;
    }

    /*
     * Clear the variable's current value unless this is an
     * append operation.
     */

    if (varPtr->flags & VAR_ARRAY) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, "set", isArray);
	}
	return NULL;
    }
    if (!(flags & TCL_APPEND_VALUE) || (varPtr->flags & VAR_UNDEFINED)) {
	varPtr->valueLength = 0;
    }

    /*
     * Call read trace if variable is being appended to.
     */

    if ((flags & TCL_APPEND_VALUE) && ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL)))) {
	char *msg;
	msg = CallTraces(iPtr, arrayPtr, varPtr, part1, part2,
		(flags & (TCL_GLOBAL_ONLY|PART1_NOT_PARSED)) | TCL_TRACE_READS);
	if (msg != NULL) {
	    VarErrMsg(interp, part1, part2, "read", msg);
	    result = NULL;
	    goto cleanup;
	}
    } 

    /*
     * Compute how many total bytes will be needed for the variable's
     * new value (leave space for a separating space between list
     * elements).  Allocate new space for the value if needed.
     */

    if (flags & TCL_LIST_ELEMENT) {
	length = Tcl_ScanElement(newValue, &listFlags) + 1;
    } else {
	length = strlen(newValue);
    }
    length += varPtr->valueLength;
    if (length >= varPtr->valueSpace) {
	char *newValue;
	int newSize;

	newSize = 2*varPtr->valueSpace;
	if (newSize <= length) {
	    newSize = length + 1;
	}
	if (newSize < 24) {
	    /*
	     * Don't waste time with teensy-tiny variables;  we'll
	     * just end up expanding them later.
	     */

	    newSize = 24;
	}
	newValue = (char *) ckalloc((unsigned) newSize);
	if (varPtr->valueSpace > 0) {
	    strcpy(newValue, varPtr->value.string);
	    ckfree(varPtr->value.string);
	}
	varPtr->valueSpace = newSize;
	varPtr->value.string = newValue;
    }

    /*
     * Append the new value to the variable, either as a list
     * element or as a string.
     */

    if (flags & TCL_LIST_ELEMENT) {
	char *dst = varPtr->value.string + varPtr->valueLength;

	if (TclNeedSpace(varPtr->value.string, dst)) {
	    *dst = ' ';
	    dst++;
	    varPtr->valueLength++;
	}
	varPtr->valueLength += Tcl_ConvertElement(newValue, dst, listFlags);
    } else {
	strcpy(varPtr->value.string + varPtr->valueLength, newValue);
	varPtr->valueLength = length;
    }
    varPtr->flags &= ~VAR_UNDEFINED;

    /*
     * Invoke any write traces for the variable.
     */

    if ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	char *msg;

	msg = CallTraces(iPtr, arrayPtr, varPtr, part1, part2,
		(flags & (TCL_GLOBAL_ONLY|PART1_NOT_PARSED))
		| TCL_TRACE_WRITES);
	if (msg != NULL) {
	    VarErrMsg(interp, part1, part2, "set", msg);
	    result = NULL;
	    goto cleanup;
	}
    }

    /*
     * If the variable was changed in some gross way by a trace (e.g.
     * it was unset and then recreated as an array) then just return
     * an empty string;  otherwise return the variable's current
     * value.
     */

    if (!(varPtr->flags & (VAR_UNDEFINED|VAR_UPVAR|VAR_ARRAY))) {
	return varPtr->value.string;
    }
    result = "";

    /*
     * If the variable doesn't exist anymore and no-one's using it,
     * then free up the relevant structures and hash table entries.
     */

    cleanup:
    if (varPtr->flags & VAR_UNDEFINED) {
	CleanupVar(varPtr, arrayPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnsetVar --
 *
 *	Delete a variable, so that it may not be accessed anymore.
 *
 * Results:
 *	Returns TCL_OK if the variable was successfully deleted, TCL_ERROR
 *	if the variable can't be unset.  In the event of an error,
 *	if the TCL_LEAVE_ERR_MSG flag is set then an error message
 *	is left in interp->result.
 *
 * Side effects:
 *	If varName is defined as a local or global variable in interp,
 *	it is deleted.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnsetVar(interp, varName, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *varName;		/* Name of a variable in interp.  May be
				 * either a scalar name or an array name
				 * or an element in an array. */
    int flags;			/* OR-ed combination of any of
				 * TCL_GLOBAL_ONLY or TCL_LEAVE_ERR_MSG. */
{
    return Tcl_UnsetVar2(interp, varName, (char *) NULL,
	    flags | PART1_NOT_PARSED);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnsetVar2 --
 *
 *	Delete a variable, given a 2-part name.
 *
 * Results:
 *	Returns TCL_OK if the variable was successfully deleted, TCL_ERROR
 *	if the variable can't be unset.  In the event of an error,
 *	if the TCL_LEAVE_ERR_MSG flag is set then an error message
 *	is left in interp->result.
 *
 * Side effects:
 *	If part1 and part2 indicate a local or global variable in interp,
 *	it is deleted.  If part1 is an array name and part2 is NULL, then
 *	the whole array is deleted.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnsetVar2(interp, part1, part2, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *part1;		/* Name of variable or array. */
    char *part2;		/* Name of element within array or NULL. */
    int flags;			/* OR-ed combination of any of
				 * TCL_GLOBAL_ONLY, TCL_LEAVE_ERR_MSG,
				 * or PART1_NOT_PARSED. */
{
    Var *varPtr, dummyVar;
    Interp *iPtr = (Interp *) interp;
    Var *arrayPtr;
    ActiveVarTrace *activePtr;
    int result;

    varPtr = LookupVar(interp, part1, part2, flags, "unset", 0,  &arrayPtr);
    if (varPtr == NULL) {
	return TCL_ERROR;
    }
    result = (varPtr->flags & VAR_UNDEFINED) ? TCL_ERROR : TCL_OK;

    if ((arrayPtr != NULL) && (arrayPtr->searchPtr != NULL)) {
	DeleteSearches(arrayPtr);
    }

    /*
     * The code below is tricky, because of the possibility that
     * a trace procedure might try to access a variable being
     * deleted.  To handle this situation gracefully, do things
     * in three steps:
     * 1. Copy the contents of the variable to a dummy variable
     *    structure, and mark the original structure as undefined.
     * 2. Invoke traces and clean up the variable, using the copy.
     * 3. If at the end of this the original variable is still
     *    undefined and has no outstanding references, then delete
     *	  it (but it could have gotten recreated by a trace).
     */

    dummyVar = *varPtr;
    varPtr->valueSpace = 0;
    varPtr->flags = VAR_UNDEFINED;
    varPtr->tracePtr = NULL;
    varPtr->searchPtr = NULL;

    /*
     * Call trace procedures for the variable being deleted and delete
     * its traces.  Be sure to abort any other traces for the variable
     * that are still pending.  Special tricks:
     * 1. Increment varPtr's refCount around this:  CallTraces will
     *    use dummyVar so it won't increment varPtr's refCount.
     * 2. Turn off the VAR_TRACE_ACTIVE flag in dummyVar: we want to
     *    call unset traces even if other traces are pending.
     */

    if ((dummyVar.tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	varPtr->refCount++;
	dummyVar.flags &= ~VAR_TRACE_ACTIVE;
	(void) CallTraces(iPtr, arrayPtr, &dummyVar, part1, part2,
		(flags & (TCL_GLOBAL_ONLY|PART1_NOT_PARSED))
		| TCL_TRACE_UNSETS);
	while (dummyVar.tracePtr != NULL) {
	    VarTrace *tracePtr = dummyVar.tracePtr;
	    dummyVar.tracePtr = tracePtr->nextPtr;
	    ckfree((char *) tracePtr);
	}
	for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
		activePtr = activePtr->nextPtr) {
	    if (activePtr->varPtr == varPtr) {
		activePtr->nextTracePtr = NULL;
	    }
	}
	varPtr->refCount--;
    }

    /*
     * If the variable is an array, delete all of its elements.  This
     * must be done after calling the traces on the array, above (that's
     * the way traces are defined).
     */

    if (dummyVar.flags & VAR_ARRAY) {
	DeleteArray(iPtr, part1, &dummyVar,
	    (flags & TCL_GLOBAL_ONLY) | TCL_TRACE_UNSETS);
    }
    if (dummyVar.valueSpace > 0) {
	ckfree(dummyVar.value.string);
    }
    if (result == TCL_ERROR) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, "unset", 
		    (arrayPtr == NULL) ? noSuchVar : noSuchElement);
	}
    }

    /*
     * Finally, if the variable is truly not in use then free up its
     * record and remove it from the hash table.
     */

    CleanupVar(varPtr, arrayPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TraceVar --
 *
 *	Arrange for reads and/or writes to a variable to cause a
 *	procedure to be invoked, which can monitor the operations
 *	and/or change their actions.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	A trace is set up on the variable given by varName, such that
 *	future references to the variable will be intermediated by
 *	proc.  See the manual entry for complete details on the calling
 *	sequence for proc.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_TraceVar(interp, varName, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter in which variable is
				 * to be traced. */
    char *varName;		/* Name of variable;  may end with "(index)"
				 * to signify an array reference. */
    int flags;			/* OR-ed collection of bits, including any
				 * of TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, and TCL_GLOBAL_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure to call when specified ops are
				 * invoked upon varName. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    return Tcl_TraceVar2(interp, varName, (char *) NULL,
	    flags | PART1_NOT_PARSED, proc, clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TraceVar2 --
 *
 *	Arrange for reads and/or writes to a variable to cause a
 *	procedure to be invoked, which can monitor the operations
 *	and/or change their actions.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	A trace is set up on the variable given by part1 and part2, such
 *	that future references to the variable will be intermediated by
 *	proc.  See the manual entry for complete details on the calling
 *	sequence for proc.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_TraceVar2(interp, part1, part2, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter in which variable is
				 * to be traced. */
    char *part1;		/* Name of scalar variable or array. */
    char *part2;		/* Name of element within array;  NULL means
				 * trace applies to scalar variable or array
				 * as-a-whole. */
    int flags;			/* OR-ed collection of bits, including any
				 * of TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, TCL_GLOBAL_ONLY, and
				 * PART1_NOT_PARSED. */
    Tcl_VarTraceProc *proc;	/* Procedure to call when specified ops are
				 * invoked upon varName. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    Var *varPtr, *arrayPtr;
    register VarTrace *tracePtr;

    varPtr = LookupVar(interp, part1, part2, (flags | TCL_LEAVE_ERR_MSG),
	    "trace", CRT_PART1|CRT_PART2, &arrayPtr);
    if (varPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Set up trace information.
     */

    tracePtr = (VarTrace *) ckalloc(sizeof(VarTrace));
    tracePtr->traceProc = proc;
    tracePtr->clientData = clientData;
    tracePtr->flags = flags &
	    (TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS);
    tracePtr->nextPtr = varPtr->tracePtr;
    varPtr->tracePtr = tracePtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UntraceVar --
 *
 *	Remove a previously-created trace for a variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there exists a trace for the variable given by varName
 *	with the given flags, proc, and clientData, then that trace
 *	is removed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UntraceVar(interp, varName, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter containing traced variable. */
    char *varName;		/* Name of variable;  may end with "(index)"
				 * to signify an array reference. */
    int flags;			/* OR-ed collection of bits describing
				 * current trace, including any of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, and TCL_GLOBAL_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    Tcl_UntraceVar2(interp, varName, (char *) NULL, flags | PART1_NOT_PARSED,
	    proc, clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UntraceVar2 --
 *
 *	Remove a previously-created trace for a variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there exists a trace for the variable given by part1
 *	and part2 with the given flags, proc, and clientData, then
 *	that trace is removed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UntraceVar2(interp, part1, part2, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter containing traced variable. */
    char *part1;		/* Name of variable or array. */
    char *part2;		/* Name of element within array;  NULL means
				 * trace applies to scalar variable or array
				 * as-a-whole. */
    int flags;			/* OR-ed collection of bits describing
				 * current trace, including any of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, TCL_GLOBAL_ONLY, and
				 * PART1_NOT_PARSED. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    register VarTrace *tracePtr;
    VarTrace *prevPtr;
    Var *varPtr, *arrayPtr;
    Interp *iPtr = (Interp *) interp;
    ActiveVarTrace *activePtr;

    varPtr = LookupVar(interp, part1, part2,
	    flags & (TCL_GLOBAL_ONLY|PART1_NOT_PARSED), (char *) NULL, 0,
	    &arrayPtr);
    if (varPtr == NULL) {
	return;
    }

    flags &= (TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS);
    for (tracePtr = varPtr->tracePtr, prevPtr = NULL; ;
	    prevPtr = tracePtr, tracePtr = tracePtr->nextPtr) {
	if (tracePtr == NULL) {
	    return;
	}
	if ((tracePtr->traceProc == proc) && (tracePtr->flags == flags)
		&& (tracePtr->clientData == clientData)) {
	    break;
	}
    }

    /*
     * The code below makes it possible to delete traces while traces
     * are active:  it makes sure that the deleted trace won't be
     * processed by CallTraces.
     */

    for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
	    activePtr = activePtr->nextPtr) {
	if (activePtr->nextTracePtr == tracePtr) {
	    activePtr->nextTracePtr = tracePtr->nextPtr;
	}
    }
    if (prevPtr == NULL) {
	varPtr->tracePtr = tracePtr->nextPtr;
    } else {
	prevPtr->nextPtr = tracePtr->nextPtr;
    }
    ckfree((char *) tracePtr);

    /*
     * If this is the last trace on the variable, and the variable is
     * unset and unused, then free up the variable.
     */

    if (varPtr->flags & VAR_UNDEFINED) {
	CleanupVar(varPtr, (Var *) NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VarTraceInfo --
 *
 *	Return the clientData value associated with a trace on a
 *	variable.  This procedure can also be used to step through
 *	all of the traces on a particular variable that have the
 *	same trace procedure.
 *
 * Results:
 *	The return value is the clientData value associated with
 *	a trace on the given variable.  Information will only be
 *	returned for a trace with proc as trace procedure.  If
 *	the clientData argument is NULL then the first such trace is
 *	returned;  otherwise, the next relevant one after the one
 *	given by clientData will be returned.  If the variable
 *	doesn't exist, or if there are no (more) traces for it,
 *	then NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_VarTraceInfo(interp, varName, flags, proc, prevClientData)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *varName;		/* Name of variable;  may end with "(index)"
				 * to signify an array reference. */
    int flags;			/* 0 or TCL_GLOBAL_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData prevClientData;	/* If non-NULL, gives last value returned
				 * by this procedure, so this call will
				 * return the next trace after that one.
				 * If NULL, this call will return the
				 * first trace. */
{
    return Tcl_VarTraceInfo2(interp, varName, (char *) NULL,
	    flags | PART1_NOT_PARSED, proc, prevClientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VarTraceInfo2 --
 *
 *	Same as Tcl_VarTraceInfo, except takes name in two pieces
 *	instead of one.
 *
 * Results:
 *	Same as Tcl_VarTraceInfo.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_VarTraceInfo2(interp, part1, part2, flags, proc, prevClientData)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *part1;		/* Name of variable or array. */
    char *part2;		/* Name of element within array;  NULL means
				 * trace applies to scalar variable or array
				 * as-a-whole. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY and
				 * PART1_NOT_PARSED. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData prevClientData;	/* If non-NULL, gives last value returned
				 * by this procedure, so this call will
				 * return the next trace after that one.
				 * If NULL, this call will return the
				 * first trace. */
{
    register VarTrace *tracePtr;
    Var *varPtr, *arrayPtr;

    varPtr = LookupVar(interp, part1, part2,
	    flags & (TCL_GLOBAL_ONLY|PART1_NOT_PARSED), (char *) NULL, 0,
	    &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }

    /*
     * Find the relevant trace, if any, and return its clientData.
     */

    tracePtr = varPtr->tracePtr;
    if (prevClientData != NULL) {
	for ( ; tracePtr != NULL; tracePtr = tracePtr->nextPtr) {
	    if ((tracePtr->clientData == prevClientData)
		    && (tracePtr->traceProc == proc)) {
		tracePtr = tracePtr->nextPtr;
		break;
	    }
	}
    }
    for ( ; tracePtr != NULL; tracePtr = tracePtr->nextPtr) {
	if (tracePtr->traceProc == proc) {
	    return tracePtr->clientData;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetCmd --
 *
 *	This procedure is invoked to process the "set" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	A variable's value may be changed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_SetCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    register Tcl_Interp *interp;	/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc == 2) {
	char *value;

	value = Tcl_GetVar2(interp, argv[1], (char *) NULL,
		TCL_LEAVE_ERR_MSG|PART1_NOT_PARSED);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	interp->result = value;
	return TCL_OK;
    } else if (argc == 3) {
	char *result;

	result = Tcl_SetVar2(interp, argv[1], (char *) NULL, argv[2],
		TCL_LEAVE_ERR_MSG|PART1_NOT_PARSED);
	if (result == NULL) {
	    return TCL_ERROR;
	}
	interp->result = result;
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " varName ?newValue?\"", (char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnsetCmd --
 *
 *	This procedure is invoked to process the "unset" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_UnsetCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    register Tcl_Interp *interp;	/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " varName ?varName ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    for (i = 1; i < argc; i++) {
	if (Tcl_UnsetVar2(interp, argv[i], (char *) NULL,
		TCL_LEAVE_ERR_MSG|PART1_NOT_PARSED) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendCmd --
 *
 *	This procedure is invoked to process the "append" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	A variable's value may be changed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_AppendCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    register Tcl_Interp *interp;	/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i;
    char *result = NULL;		/* (Initialization only needed to keep
					 * the compiler from complaining) */

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " varName ?value value ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 2) {
	result = Tcl_GetVar2(interp, argv[1], (char *) NULL,
		TCL_LEAVE_ERR_MSG|PART1_NOT_PARSED);
	if (result == NULL) {
	    return TCL_ERROR;
	}
	interp->result = result;
	return TCL_OK;
    }

    for (i = 2; i < argc; i++) {
	result = Tcl_SetVar2(interp, argv[1], (char *) NULL, argv[i],
		TCL_APPEND_VALUE|TCL_LEAVE_ERR_MSG|PART1_NOT_PARSED);
	if (result == NULL) {
	    return TCL_ERROR;
	}
    }
    interp->result = result;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LappendCmd --
 *
 *	This procedure is invoked to process the "lappend" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	A variable's value may be changed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LappendCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    register Tcl_Interp *interp;	/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i;
    char *result = NULL;		/* (Initialization only needed to keep
					 * the compiler from complaining) */

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " varName ?value value ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 2) {
	result = Tcl_GetVar2(interp, argv[1], (char *) NULL,
		TCL_LEAVE_ERR_MSG|PART1_NOT_PARSED);
	if (result == NULL) {
	    return TCL_ERROR;
	}
	interp->result = result;
	return TCL_OK;
    }

    for (i = 2; i < argc; i++) {
	result = Tcl_SetVar2(interp, argv[1], (char *) NULL, argv[i],
		TCL_APPEND_VALUE|TCL_LIST_ELEMENT|TCL_LEAVE_ERR_MSG
		|PART1_NOT_PARSED);
	if (result == NULL) {
	    return TCL_ERROR;
	}
    }
    interp->result = result;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ArrayCmd --
 *
 *	This procedure is invoked to process the "array" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ArrayCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    register Tcl_Interp *interp;	/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int c, notArray;
    size_t length;
    Var *varPtr = NULL;		/* Initialization needed only to prevent
				 * compiler warning. */
    Tcl_HashEntry *hPtr;
    Interp *iPtr = (Interp *) interp;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option arrayName ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Locate the array variable (and it better be an array).
     */

    if (iPtr->varFramePtr == NULL) {
	hPtr = Tcl_FindHashEntry(&iPtr->globalTable, argv[2]);
    } else {
	hPtr = Tcl_FindHashEntry(&iPtr->varFramePtr->varTable, argv[2]);
    }
    notArray = 0;
    if (hPtr == NULL) {
	notArray = 1;
    } else {
	varPtr = (Var *) Tcl_GetHashValue(hPtr);
	if (varPtr->flags & VAR_UPVAR) {
	    varPtr = varPtr->value.upvarPtr;
	}
	if (!(varPtr->flags & VAR_ARRAY)) {
	    notArray = 1;
	}
    }

    /*
     * Dispatch based on the option.
     */

    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "anymore", length) == 0)) {
	ArraySearch *searchPtr;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " anymore arrayName searchId\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (notArray) {
	    goto error;
	}
	searchPtr = ParseSearchId(interp, varPtr, argv[2], argv[3]);
	if (searchPtr == NULL) {
	    return TCL_ERROR;
	}
	while (1) {
	    Var *varPtr2;

	    if (searchPtr->nextEntry != NULL) {
		varPtr2 = (Var *) Tcl_GetHashValue(searchPtr->nextEntry);
		if (!(varPtr2->flags & VAR_UNDEFINED)) {
		    break;
		}
	    }
	    searchPtr->nextEntry = Tcl_NextHashEntry(&searchPtr->search);
	    if (searchPtr->nextEntry == NULL) {
		interp->result = "0";
		return TCL_OK;
	    }
	}
	interp->result = "1";
	return TCL_OK;
    } else if ((c == 'd') && (strncmp(argv[1], "donesearch", length) == 0)) {
	ArraySearch *searchPtr, *prevPtr;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " donesearch arrayName searchId\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (notArray) {
	    goto error;
	}
	searchPtr = ParseSearchId(interp, varPtr, argv[2], argv[3]);
	if (searchPtr == NULL) {
	    return TCL_ERROR;
	}
	if (varPtr->searchPtr == searchPtr) {
	    varPtr->searchPtr = searchPtr->nextPtr;
	} else {
	    for (prevPtr = varPtr->searchPtr; ; prevPtr = prevPtr->nextPtr) {
		if (prevPtr->nextPtr == searchPtr) {
		    prevPtr->nextPtr = searchPtr->nextPtr;
		    break;
		}
	    }
	}
	ckfree((char *) searchPtr);
    } else if ((c == 'e') && (strncmp(argv[1], "exists", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " exists arrayName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	interp->result = (notArray) ? "0" : "1";
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	Tcl_HashSearch search;
	Var *varPtr2;
	char *name;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get arrayName ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (notArray) {
	    return TCL_OK;
	}
	for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
	    if (varPtr2->flags & VAR_UNDEFINED) {
		continue;
	    }
	    name = Tcl_GetHashKey(varPtr->value.tablePtr, hPtr);
	    if ((argc == 4) && !Tcl_StringMatch(name, argv[3])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	    Tcl_AppendElement(interp, varPtr2->value.string);
	}
    } else if ((c == 'n') && (strncmp(argv[1], "names", length) == 0)
	    && (length >= 2)) {
	Tcl_HashSearch search;
	Var *varPtr2;
	char *name;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " names arrayName ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (notArray) {
	    return TCL_OK;
	}
	for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
	    if (varPtr2->flags & VAR_UNDEFINED) {
		continue;
	    }
	    name = Tcl_GetHashKey(varPtr->value.tablePtr, hPtr);
	    if ((argc == 4) && !Tcl_StringMatch(name, argv[3])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	}
    } else if ((c == 'n') && (strncmp(argv[1], "nextelement", length) == 0)
	    && (length >= 2)) {
	ArraySearch *searchPtr;
	Tcl_HashEntry *hPtr;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " nextelement arrayName searchId\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (notArray) {
	    goto error;
	}
	searchPtr = ParseSearchId(interp, varPtr, argv[2], argv[3]);
	if (searchPtr == NULL) {
	    return TCL_ERROR;
	}
	while (1) {
	    Var *varPtr2;

	    hPtr = searchPtr->nextEntry;
	    if (hPtr == NULL) {
		hPtr = Tcl_NextHashEntry(&searchPtr->search);
		if (hPtr == NULL) {
		    return TCL_OK;
		}
	    } else {
		searchPtr->nextEntry = NULL;
	    }
	    varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
	    if (!(varPtr2->flags & VAR_UNDEFINED)) {
		break;
	    }
	}
	interp->result = Tcl_GetHashKey(varPtr->value.tablePtr, hPtr);
    } else if ((c == 's') && (strncmp(argv[1], "set", length) == 0)
	    && (length >= 2)) {
	char **valueArgv;
	int valueArgc, i, result;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " set arrayName list\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_SplitList(interp, argv[3], &valueArgc, &valueArgv) != TCL_OK) {
	    return TCL_ERROR;
	}
	result = TCL_OK;
	if (valueArgc & 1) {
	    interp->result = "list must have an even number of elements";
	    result = TCL_ERROR;
	    goto setDone;
	}
	for (i = 0; i < valueArgc; i += 2) {
	    if (Tcl_SetVar2(interp, argv[2], valueArgv[i], valueArgv[i+1],
		    TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
		break;
	    }
	}
	setDone:
	ckfree((char *) valueArgv);
	return result;
    } else if ((c == 's') && (strncmp(argv[1], "size", length) == 0)
	    && (length >= 2)) {
	Tcl_HashSearch search;
	Var *varPtr2;
	int size;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " size arrayName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	size = 0;
	if (!notArray) {
	    for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
		    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
		varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
		if (varPtr2->flags & VAR_UNDEFINED) {
		    continue;
		}
		size++;
	    }
	}
	sprintf(interp->result, "%d", size);
    } else if ((c == 's') && (strncmp(argv[1], "startsearch", length) == 0)
	    && (length >= 2)) {
	ArraySearch *searchPtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " startsearch arrayName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (notArray) {
	    goto error;
	}
	searchPtr = (ArraySearch *) ckalloc(sizeof(ArraySearch));
	if (varPtr->searchPtr == NULL) {
	    searchPtr->id = 1;
	    Tcl_AppendResult(interp, "s-1-", argv[2], (char *) NULL);
	} else {
	    char string[20];

	    searchPtr->id = varPtr->searchPtr->id + 1;
	    sprintf(string, "%d", searchPtr->id);
	    Tcl_AppendResult(interp, "s-", string, "-", argv[2],
		    (char *) NULL);
	}
	searchPtr->varPtr = varPtr;
	searchPtr->nextEntry = Tcl_FirstHashEntry(varPtr->value.tablePtr,
		&searchPtr->search);
	searchPtr->nextPtr = varPtr->searchPtr;
	varPtr->searchPtr = searchPtr;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be anymore, donesearch, exists, ",
		"get, names, nextelement, ",
		"set, size, or startsearch", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    error:
    Tcl_AppendResult(interp, "\"", argv[2], "\" isn't an array",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeUpvar --
 *
 *	This procedure does all of the work of the "global" and "upvar"
 *	commands.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs then an
 *	error message is left in iPtr->result.
 *
 * Side effects:
 *	The variable given by myName is linked to the variable in
 *	framePtr given by otherP1 and otherP2, so that references to
 *	myName are redirected to the other variable like a symbolic
*	link.
 *
 *----------------------------------------------------------------------
 */

static int
MakeUpvar(iPtr, framePtr, otherP1, otherP2, myName, flags)
    Interp *iPtr;		/* Interpreter containing variables.  Used
				 * for error messages, too. */
    CallFrame *framePtr;	/* Call frame containing "other" variable.
				 * NULL means use global context. */
    char *otherP1, *otherP2;	/* Two-part name of variable in framePtr. */
    char *myName;		/* Name of variable in local table, which
				 * will refer to otherP1/P2.  Must be a
				 * scalar. */
    int flags;			/* 0 or TCL_GLOBAL_ONLY: indicates scope of
				 * myName. */
{
    Tcl_HashEntry *hPtr;
    Var *otherPtr, *varPtr, *arrayPtr;
    CallFrame *savedFramePtr;
    int new;

    /*
     * In order to use LookupVar to find "other", temporarily replace
     * the current frame pointer in the interpreter.
     */

    savedFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = framePtr;
    otherPtr = LookupVar((Tcl_Interp *) iPtr, otherP1, otherP2,
	    TCL_LEAVE_ERR_MSG, "access", CRT_PART1|CRT_PART2, &arrayPtr);
    iPtr->varFramePtr = savedFramePtr;
    if (otherPtr == NULL) {
	return TCL_ERROR;
    }
    if ((flags & TCL_GLOBAL_ONLY) || (iPtr->varFramePtr == NULL)) {
	hPtr = Tcl_CreateHashEntry(&iPtr->globalTable, myName, &new);
    } else {
	hPtr = Tcl_CreateHashEntry(&iPtr->varFramePtr->varTable, myName, &new);
    }
    if (new) {
	varPtr = NewVar();
	Tcl_SetHashValue(hPtr, varPtr);
	varPtr->hPtr = hPtr;
    } else {
	/*
	 * The variable already exists.  Make sure that this variable
	 * isn't also "otherVar" (avoid circular links).  Also, if it's
	 * not an upvar then it's an error.  If it is an upvar, then
	 * just disconnect it from the thing it currently refers to.
	 */

	varPtr = (Var *) Tcl_GetHashValue(hPtr);
	if (varPtr == otherPtr) {
	    iPtr->result = "can't upvar from variable to itself";
	    return TCL_ERROR;
	}
	if (varPtr->flags & VAR_UPVAR) {
	    Var *upvarPtr;

	    upvarPtr = varPtr->value.upvarPtr;
	    if (upvarPtr == otherPtr) {
		return TCL_OK;
	    }
	    upvarPtr->refCount--;
	    if (upvarPtr->flags & VAR_UNDEFINED) {
		CleanupVar(upvarPtr, (Var *) NULL);
	    }
	} else if (!(varPtr->flags & VAR_UNDEFINED)) {
	    Tcl_AppendResult((Tcl_Interp *) iPtr, "variable \"", myName,
		"\" already exists", (char *) NULL);
	    return TCL_ERROR;
	} else if (varPtr->tracePtr != NULL) {
	    Tcl_AppendResult((Tcl_Interp *) iPtr, "variable \"", myName,
		"\" has traces: can't use for upvar", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    varPtr->flags = (varPtr->flags & ~VAR_UNDEFINED) | VAR_UPVAR;
    varPtr->value.upvarPtr = otherPtr;
    otherPtr->refCount++;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpVar --
 *
 *	Delete a variable, so that it may not be accessed anymore.
 *
 * Results:
 *	Returns TCL_OK if the variable was successfully deleted, TCL_ERROR
 *	if the variable can't be unset.  In the event of an error,
 *	if the TCL_LEAVE_ERR_MSG flag is set then an error message
 *	is left in interp->result.
 *
 * Side effects:
 *	If varName is defined as a local or global variable in interp,
 *	it is deleted.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UpVar(interp, frameName, varName, localName, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *frameName;		/* Name of the frame containing the source
				 * variable, such as "1" or "#0". */
    char *varName;		/* Name of a variable in interp.  May be
				 * either a scalar name or an element
				 * in an array. */
    char *localName;		/* Destination variable name. */
    int flags;			/* Either 0 or TCL_GLOBAL_ONLY;  indicates
				 * whether localName is local or global. */
{
    int result;
    CallFrame *framePtr;
    register char *p;

    result = TclGetFrame(interp, frameName, &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }

    /*
     * Figure out whether this is an array reference, then call
     * Tcl_UpVar2 to do all the real work.
     */

    for (p = varName; *p != '\0'; p++) {
	if (*p == '(') {
	    char *openParen = p;

	    do {
		p++;
	    } while (*p != '\0');
	    p--;
	    if (*p != ')') {
		goto scalar;
	    }
	    *openParen = '\0';
	    *p = '\0';
	    result = MakeUpvar((Interp *) interp, framePtr, varName,
		    openParen+1, localName, flags);
	    *openParen = '(';
	    *p = ')';
	    return result;
	}
    }

    scalar:
    return MakeUpvar((Interp *) interp, framePtr, varName, (char *) NULL,
	    localName, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpVar2 --
 *
 *	This procedure links one variable to another, just like
 *	the "upvar" command.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs then
 *	an error message is left in interp->result.
 *
 * Side effects:
 *	The variable in frameName whose name is given by part1 and
 *	part2 becomes accessible under the name newName, so that
 *	references to newName are redirected to the other variable
 *	like a symbolic link.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UpVar2(interp, frameName, part1, part2, localName, flags)
    Tcl_Interp *interp;		/* Interpreter containing variables.  Used
				 * for error messages too. */
    char *frameName;		/* Name of the frame containing the source
				 * variable, such as "1" or "#0". */
    char *part1, *part2;	/* Two parts of source variable name. */
    char *localName;		/* Destination variable name. */
    int flags;			/* TCL_GLOBAL_ONLY or 0. */
{
    int result;
    CallFrame *framePtr;

    result = TclGetFrame(interp, frameName, &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }
    return MakeUpvar((Interp *) interp, framePtr, part1, part2,
	    localName, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GlobalCmd --
 *
 *	This procedure is invoked to process the "global" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_GlobalCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Interp *iPtr = (Interp *) interp;

    if (argc < 2) {
	Tcl_AppendResult((Tcl_Interp *) iPtr, "wrong # args: should be \"",
		argv[0], " varName ?varName ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (iPtr->varFramePtr == NULL) {
	return TCL_OK;
    }

    for (argc--, argv++; argc > 0; argc--, argv++) {
	if (MakeUpvar(iPtr, (CallFrame *) NULL, *argv, (char *) NULL, *argv, 0)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpvarCmd --
 *
 *	This procedure is invoked to process the "upvar" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_UpvarCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Interp *iPtr = (Interp *) interp;
    int result;
    CallFrame *framePtr;
    register char *p;

    if (argc < 3) {
	upvarSyntax:
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?level? otherVar localVar ?otherVar localVar ...?\"",
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Find the hash table containing the variable being referenced.
     */

    result = TclGetFrame(interp, argv[1], &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }
    argc -= result+1;
    if ((argc & 1) != 0) {
	goto upvarSyntax;
    }
    argv += result+1;

    /*
     * Iterate over all the pairs of (other variable, local variable)
     * names.  For each pair, divide the other variable name into two
     * parts, then call MakeUpvar to do all the work of creating linking
     * it to the local variable.
     */

    for ( ; argc > 0; argc -= 2, argv += 2) {
	for (p = argv[0]; *p != 0; p++) {
	    if (*p == '(') {
		char *openParen = p;

		do {
		    p++;
		} while (*p != '\0');
		p--;
		if (*p != ')') {
		    goto scalar;
		}
		*openParen = '\0';
		*p = '\0';
		result = MakeUpvar(iPtr, framePtr, argv[0], openParen+1,
			argv[1], 0);
		*openParen = '(';
		*p = ')';
		goto checkResult;
	    }
	}
	scalar:
	result = MakeUpvar(iPtr, framePtr, argv[0], (char *) NULL, argv[1], 0);

	checkResult:
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CallTraces --
 *
 *	This procedure is invoked to find and invoke relevant
 *	trace procedures associated with a particular operation on
 *	a variable.  This procedure invokes traces both on the
 *	variable and on its containing array (where relevant).
 *
 * Results:
 *	The return value is NULL if no trace procedures were invoked, or
 *	if all the invoked trace procedures returned successfully.
 *	The return value is non-zero if a trace procedure returned an
 *	error (in this case no more trace procedures were invoked after
 *	the error was returned).  In this case the return value is a
 *	pointer to a static string describing the error.
 *
 * Side effects:
 *	Almost anything can happen, depending on trace;  this procedure
 *	itself doesn't have any side effects.
 *
 *----------------------------------------------------------------------
 */

static char *
CallTraces(iPtr, arrayPtr, varPtr, part1, part2, flags)
    Interp *iPtr;			/* Interpreter containing variable. */
    register Var *arrayPtr;		/* Pointer to array variable that
					 * contains the variable, or NULL if
					 * the variable isn't an element of an
					 * array. */
    Var *varPtr;			/* Variable whose traces are to be
					 * invoked. */
    char *part1, *part2;		/* Variable's two-part name. */
    int flags;				/* Flags to pass to trace procedures:
					 * indicates what's happening to
					 * variable, plus other stuff like
					 * TCL_GLOBAL_ONLY and
					 * TCL_INTERP_DESTROYED.   May also
					 * contain PART1_NOT_PARSEd, which
					 * should not be passed through
					 * to callbacks. */
{
    register VarTrace *tracePtr;
    ActiveVarTrace active;
    char *result, *openParen, *p;
    Tcl_DString nameCopy;
    int copiedName;

    /*
     * If there are already similar trace procedures active for the
     * variable, don't call them again.
     */

    if (varPtr->flags & VAR_TRACE_ACTIVE) {
	return NULL;
    }
    varPtr->flags |= VAR_TRACE_ACTIVE;
    varPtr->refCount++;

    /*
     * If the variable name hasn't been parsed into array name and
     * element, do it here.  If there really is an array element,
     * make a copy of the original name so that NULLs can be
     * inserted into it to separate the names (can't modify the name
     * string in place, because the string might get used by the
     * callbacks we invoke).
     */

    copiedName = 0;
    if (flags & PART1_NOT_PARSED) {
	for (p = part1; ; p++) {
	    if (*p == 0) {
		break;
	    }
	    if (*p == '(') {
		openParen = p;
		do {
		    p++;
		} while (*p != '\0');
		p--;
		if (*p == ')') {
		    Tcl_DStringInit(&nameCopy);
		    Tcl_DStringAppend(&nameCopy, part1, (p-part1));
		    part2 = Tcl_DStringValue(&nameCopy)
			    + (openParen + 1 - part1);
		    part2[-1] = 0;
		    part1 = Tcl_DStringValue(&nameCopy);
		    copiedName = 1;
		}
		break;
	    }
	}
    }
    flags &= ~PART1_NOT_PARSED;

    /*
     * Invoke traces on the array containing the variable, if relevant.
     */

    result = NULL;
    active.nextPtr = iPtr->activeTracePtr;
    iPtr->activeTracePtr = &active;
    if (arrayPtr != NULL) {
	arrayPtr->refCount++;
	active.varPtr = arrayPtr;
	for (tracePtr = arrayPtr->tracePtr;  tracePtr != NULL;
		tracePtr = active.nextTracePtr) {
	    active.nextTracePtr = tracePtr->nextPtr;
	    if (!(tracePtr->flags & flags)) {
		continue;
	    }
	    result = (*tracePtr->traceProc)(tracePtr->clientData,
		    (Tcl_Interp *) iPtr, part1, part2, flags);
	    if (result != NULL) {
		if (flags & TCL_TRACE_UNSETS) {
		    result = NULL;
		} else {
		    goto done;
		}
	    }
	}
    }

    /*
     * Invoke traces on the variable itself.
     */

    if (flags & TCL_TRACE_UNSETS) {
	flags |= TCL_TRACE_DESTROYED;
    }
    active.varPtr = varPtr;
    for (tracePtr = varPtr->tracePtr; tracePtr != NULL;
	    tracePtr = active.nextTracePtr) {
	active.nextTracePtr = tracePtr->nextPtr;
	if (!(tracePtr->flags & flags)) {
	    continue;
	}
	result = (*tracePtr->traceProc)(tracePtr->clientData,
		(Tcl_Interp *) iPtr, part1, part2, flags);
	if (result != NULL) {
	    if (flags & TCL_TRACE_UNSETS) {
		result = NULL;
	    } else {
		goto done;
	    }
	}
    }

    /*
     * Restore the variable's flags, remove the record of our active
     * traces, and then return.
     */

    done:
    if (arrayPtr != NULL) {
	arrayPtr->refCount--;
    }
    if (copiedName) {
	Tcl_DStringFree(&nameCopy);
    }
    varPtr->flags &= ~VAR_TRACE_ACTIVE;
    varPtr->refCount--;
    iPtr->activeTracePtr = active.nextPtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NewVar --
 *
 *	Create a new variable with a given amount of storage
 *	space.
 *
 * Results:
 *	The return value is a pointer to the new variable structure.
 *	The variable will not be part of any hash table yet.  Its
 *	initial value is empty.
 *
 * Side effects:
 *	Storage gets allocated.
 *
 *----------------------------------------------------------------------
 */

static Var *
NewVar()
{
    register Var *varPtr;

    varPtr = (Var *) ckalloc(sizeof(Var));
    varPtr->valueLength = 0;
    varPtr->valueSpace = 0;
    varPtr->value.string = NULL;
    varPtr->hPtr = NULL;
    varPtr->refCount = 0;
    varPtr->tracePtr = NULL;
    varPtr->searchPtr = NULL;
    varPtr->flags = VAR_UNDEFINED;
    return varPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseSearchId --
 *
 *	This procedure translates from a string to a pointer to an
 *	active array search (if there is one that matches the string).
 *
 * Results:
 *	The return value is a pointer to the array search indicated
 *	by string, or NULL if there isn't one.  If NULL is returned,
 *	interp->result contains an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ArraySearch *
ParseSearchId(interp, varPtr, varName, string)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    Var *varPtr;		/* Array variable search is for. */
    char *varName;		/* Name of array variable that search is
				 * supposed to be for. */
    char *string;		/* String containing id of search.  Must have
				 * form "search-num-var" where "num" is a
				 * decimal number and "var" is a variable
				 * name. */
{
    char *end;
    int id;
    ArraySearch *searchPtr;

    /*
     * Parse the id into the three parts separated by dashes.
     */

    if ((string[0] != 's') || (string[1] != '-')) {
	syntax:
	Tcl_AppendResult(interp, "illegal search identifier \"", string,
		"\"", (char *) NULL);
	return NULL;
    }
    id = strtoul(string+2, &end, 10);
    if ((end == (string+2)) || (*end != '-')) {
	goto syntax;
    }
    if (strcmp(end+1, varName) != 0) {
	Tcl_AppendResult(interp, "search identifier \"", string,
		"\" isn't for variable \"", varName, "\"", (char *) NULL);
	return NULL;
    }

    /*
     * Search through the list of active searches on the interpreter
     * to see if the desired one exists.
     */

    for (searchPtr = varPtr->searchPtr; searchPtr != NULL;
	    searchPtr = searchPtr->nextPtr) {
	if (searchPtr->id == id) {
	    return searchPtr;
	}
    }
    Tcl_AppendResult(interp, "couldn't find search \"", string, "\"",
	    (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteSearches --
 *
 *	This procedure is called to free up all of the searches
 *	associated with an array variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is released to the storage allocator.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteSearches(arrayVarPtr)
    register Var *arrayVarPtr;		/* Variable whose searches are
					 * to be deleted. */
{
    ArraySearch *searchPtr;

    while (arrayVarPtr->searchPtr != NULL) {
	searchPtr = arrayVarPtr->searchPtr;
	arrayVarPtr->searchPtr = searchPtr->nextPtr;
	ckfree((char *) searchPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclDeleteVars --
 *
 *	This procedure is called to recycle all the storage space
 *	associated with a table of variables.  For this procedure
 *	to work correctly, it must not be possible for any of the
 *	variable in the table to be accessed from Tcl commands
 *	(e.g. from trace procedures).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Variables are deleted and trace procedures are invoked, if
 *	any are declared.
 *
 *----------------------------------------------------------------------
 */

void
TclDeleteVars(iPtr, tablePtr)
    Interp *iPtr;		/* Interpreter to which variables belong. */
    Tcl_HashTable *tablePtr;	/* Hash table containing variables to
				 * delete. */
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    register Var *varPtr;
    Var *upvarPtr;
    int flags;
    ActiveVarTrace *activePtr;

    flags = TCL_TRACE_UNSETS;
    if (tablePtr == &iPtr->globalTable) {
	flags |= TCL_INTERP_DESTROYED | TCL_GLOBAL_ONLY;
    }
    for (hPtr = Tcl_FirstHashEntry(tablePtr, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	varPtr = (Var *) Tcl_GetHashValue(hPtr);

	/*
	 * For global/upvar variables referenced in procedures, decrement
	 * the reference count on the variable referred to, and free
	 * the referenced variable if it's no longer needed.  Don't delete
	 * the hash entry for the other variable if it's in the same table
	 * as us:  this will happen automatically later on.
	 */

	if (varPtr->flags & VAR_UPVAR) {
	    upvarPtr = varPtr->value.upvarPtr;
	    upvarPtr->refCount--;
	    if ((upvarPtr->refCount == 0) && (upvarPtr->flags & VAR_UNDEFINED)
		    && (upvarPtr->tracePtr == NULL)) {
		if (upvarPtr->hPtr == NULL) {
		    ckfree((char *) upvarPtr);
		} else if (upvarPtr->hPtr->tablePtr != tablePtr) {
		    Tcl_DeleteHashEntry(upvarPtr->hPtr);
		    ckfree((char *) upvarPtr);
		}
	    }
	}

	/*
	 * Invoke traces on the variable that is being deleted, then
	 * free up the variable's space (no need to free the hash entry
	 * here, unless we're dealing with a global variable:  the
	 * hash entries will be deleted automatically when the whole
	 * table is deleted).
	 */

	if (varPtr->tracePtr != NULL) {
	    (void) CallTraces(iPtr, (Var *) NULL, varPtr,
		    Tcl_GetHashKey(tablePtr, hPtr), (char *) NULL, flags);
	    while (varPtr->tracePtr != NULL) {
		VarTrace *tracePtr = varPtr->tracePtr;
		varPtr->tracePtr = tracePtr->nextPtr;
		ckfree((char *) tracePtr);
	    }
	    for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
		    activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == varPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}
	if (varPtr->flags & VAR_ARRAY) {
	    DeleteArray(iPtr, Tcl_GetHashKey(tablePtr, hPtr), varPtr, flags);
	}
	if (varPtr->valueSpace > 0) {
	    /*
	     * SPECIAL TRICK:  it's possible that the interpreter's result
	     * currently points to this variable (for example, a "set" or
	     * "lappend" command was the last command in a procedure that's
	     * being returned from).  If this is the case, then just pass
	     * ownership of the value string to the Tcl interpreter.
	     */

	    if (iPtr->result == varPtr->value.string) {
		iPtr->freeProc = TCL_DYNAMIC;
	    } else {
		ckfree(varPtr->value.string);
	    }
	    varPtr->valueSpace = 0;
	}
	varPtr->hPtr = NULL;
	varPtr->tracePtr = NULL;
	varPtr->flags = VAR_UNDEFINED;

	/*
	 * Recycle the variable's memory space if there aren't any upvar's
	 * pointing to it.  If there are upvars, then the variable will
	 * get freed when the last upvar goes away.
	 */

	if (varPtr->refCount == 0) {
	    ckfree((char *) varPtr);
	}
    }
    Tcl_DeleteHashTable(tablePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteArray --
 *
 *	This procedure is called to free up everything in an array
 *	variable.  It's the caller's responsibility to make sure
 *	that the array is no longer accessible before this procedure
 *	is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All storage associated with varPtr's array elements is deleted
 *	(including the hash table).  Delete trace procedures for
 *	array elements are invoked.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteArray(iPtr, arrayName, varPtr, flags)
    Interp *iPtr;			/* Interpreter containing array. */
    char *arrayName;			/* Name of array (used for trace
					 * callbacks). */
    Var *varPtr;			/* Pointer to variable structure. */
    int flags;				/* Flags to pass to CallTraces:
					 * TCL_TRACE_UNSETS and sometimes
					 * TCL_INTERP_DESTROYED and/or
					 * TCL_GLOBAL_ONLY. */
{
    Tcl_HashSearch search;
    register Tcl_HashEntry *hPtr;
    register Var *elPtr;
    ActiveVarTrace *activePtr;

    DeleteSearches(varPtr);
    for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	elPtr = (Var *) Tcl_GetHashValue(hPtr);
	if (elPtr->valueSpace != 0) {
	    /*
	     * SPECIAL TRICK:  it's possible that the interpreter's result
	     * currently points to this element (for example, a "set" or
	     * "lappend" command was the last command in a procedure that's
	     * being returned from).  If this is the case, then just pass
	     * ownership of the value string to the Tcl interpreter.
	     */

	    if (iPtr->result == elPtr->value.string) {
		iPtr->freeProc = TCL_DYNAMIC;
	    } else {
		ckfree(elPtr->value.string);
	    }
	    elPtr->valueSpace = 0;
	}
	elPtr->hPtr = NULL;
	if (elPtr->tracePtr != NULL) {
	    elPtr->flags &= ~VAR_TRACE_ACTIVE;
	    (void) CallTraces(iPtr, (Var *) NULL, elPtr, arrayName,
		    Tcl_GetHashKey(varPtr->value.tablePtr, hPtr), flags);
	    while (elPtr->tracePtr != NULL) {
		VarTrace *tracePtr = elPtr->tracePtr;
		elPtr->tracePtr = tracePtr->nextPtr;
		ckfree((char *) tracePtr);
	    }
	    for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
		    activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == elPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}
	elPtr->flags = VAR_UNDEFINED;
	if (elPtr->refCount == 0) {
	    ckfree((char *) elPtr);
	}
    }
    Tcl_DeleteHashTable(varPtr->value.tablePtr);
    ckfree((char *) varPtr->value.tablePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupVar --
 *
 *	This procedure is called when it looks like it may be OK
 *	to free up the variable's record and hash table entry, and
 *	those of its containing parent.  It's called, for example,
 *	when a trace on a variable deletes the variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the variable (or its containing array) really is dead then
 *	its record, and possibly its hash table entry, gets freed up.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupVar(varPtr, arrayPtr)
    Var *varPtr;		/* Pointer to variable that may be a
				 * candidate for being expunged. */
    Var *arrayPtr;		/* Array that contains the variable, or
				 * NULL if this variable isn't an array
				 * element. */
{
    if ((varPtr->flags & VAR_UNDEFINED) && (varPtr->refCount == 0)
	    && (varPtr->tracePtr == NULL)) {
	if (varPtr->hPtr != NULL) {
	    Tcl_DeleteHashEntry(varPtr->hPtr);
	}
	ckfree((char *) varPtr);
    }
    if (arrayPtr != NULL) {
	if ((arrayPtr->flags & VAR_UNDEFINED) && (arrayPtr->refCount == 0)
		&& (arrayPtr->tracePtr == NULL)) {
	    if (arrayPtr->hPtr != NULL) {
		Tcl_DeleteHashEntry(arrayPtr->hPtr);
	    }
	    ckfree((char *) arrayPtr);
	}
    }
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * VarErrMsg --
 *
 *	Generate a reasonable error message describing why a variable
 *	operation failed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Interp->result is reset to hold a message identifying the
 *	variable given by part1 and part2 and describing why the
 *	variable operation failed.
 *
 *----------------------------------------------------------------------
 */

static void
VarErrMsg(interp, part1, part2, operation, reason)
    Tcl_Interp *interp;		/* Interpreter in which to record message. */
    char *part1, *part2;	/* Variable's two-part name. */
    char *operation;		/* String describing operation that failed,
				 * e.g. "read", "set", or "unset". */
    char *reason;		/* String describing why operation failed. */
{
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "can't ", operation, " \"", part1, (char *) NULL);
    if (part2 != NULL) {
	Tcl_AppendResult(interp, "(", part2, ")", (char *) NULL);
    }
    Tcl_AppendResult(interp, "\": ", reason, (char *) NULL);
}
