/* 
 * tclLink.c --
 *
 *	This file implements linked variables (a C variable that is
 *	tied to a Tcl variable).  The idea of linked variables was
 *	first suggested by Andreas Stolcke and this implementation is
 *	based heavily on a prototype implementation provided by
 *	him.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLink.c 1.12 96/02/15 11:50:26
 */

#include "tclInt.h"

/*
 * For each linked variable there is a data structure of the following
 * type, which describes the link and is the clientData for the trace
 * set on the Tcl variable.
 */

typedef struct Link {
    Tcl_Interp *interp;		/* Interpreter containing Tcl variable. */
    char *varName;		/* Name of variable (must be global).  This
				 * is needed during trace callbacks, since
				 * the actual variable may be aliased at
				 * that time via upvar. */
    char *addr;			/* Location of C variable. */
    int type;			/* Type of link (TCL_LINK_INT, etc.). */
    int writable;		/* Zero means Tcl variable is read-only. */
    union {
	int i;
	double d;
    } lastValue;		/* Last known value of C variable;  used to
				 * avoid string conversions. */
} Link;

/*
 * Forward references to procedures defined later in this file:
 */

static char *		LinkTraceProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static char *		StringValue _ANSI_ARGS_((Link *linkPtr,
			    char *buffer));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinkVar --
 *
 *	Link a C variable to a Tcl variable so that changes to either
 *	one causes the other to change.
 *
 * Results:
 *	The return value is TCL_OK if everything went well or TCL_ERROR
 *	if an error occurred (interp->result is also set after errors).
 *
 * Side effects:
 *	The value at *addr is linked to the Tcl variable "varName",
 *	using "type" to convert between string values for Tcl and
 *	binary values for *addr.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LinkVar(interp, varName, addr, type)
    Tcl_Interp *interp;		/* Interpreter in which varName exists. */
    char *varName;		/* Name of a global variable in interp. */
    char *addr;			/* Address of a C variable to be linked
				 * to varName. */
    int type;			/* Type of C variable: TCL_LINK_INT, etc. 
				 * Also may have TCL_LINK_READ_ONLY
				 * OR'ed in. */
{
    Link *linkPtr;
    char buffer[TCL_DOUBLE_SPACE];
    int code;

    linkPtr = (Link *) ckalloc(sizeof(Link));
    linkPtr->interp = interp;
    linkPtr->varName = (char *) ckalloc((unsigned) (strlen(varName) + 1));
    strcpy(linkPtr->varName, varName);
    linkPtr->addr = addr;
    linkPtr->type = type & ~TCL_LINK_READ_ONLY;
    linkPtr->writable = (type & TCL_LINK_READ_ONLY) == 0;
    if (Tcl_SetVar(interp, varName, StringValue(linkPtr, buffer),
	    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	ckfree(linkPtr->varName);
	ckfree((char *) linkPtr);
	return TCL_ERROR;
    }
    code = Tcl_TraceVar(interp, varName, TCL_GLOBAL_ONLY|TCL_TRACE_READS
	    |TCL_TRACE_WRITES|TCL_TRACE_UNSETS, LinkTraceProc,
	    (ClientData) linkPtr);
    if (code != TCL_OK) {
	ckfree(linkPtr->varName);
	ckfree((char *) linkPtr);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnlinkVar --
 *
 *	Destroy the link between a Tcl variable and a C variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If "varName" was previously linked to a C variable, the link
 *	is broken to make the variable independent.  If there was no
 *	previous link for "varName" then nothing happens.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UnlinkVar(interp, varName)
    Tcl_Interp *interp;		/* Interpreter containing variable to unlink. */
    char *varName;		/* Global variable in interp to unlink. */
{
    Link *linkPtr;

    linkPtr = (Link *) Tcl_VarTraceInfo(interp, varName, TCL_GLOBAL_ONLY,
	    LinkTraceProc, (ClientData) NULL);
    if (linkPtr == NULL) {
	return;
    }
    Tcl_UntraceVar(interp, varName,
	    TCL_GLOBAL_ONLY|TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    LinkTraceProc, (ClientData) linkPtr);
    ckfree(linkPtr->varName);
    ckfree((char *) linkPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpdateLinkedVar --
 *
 *	This procedure is invoked after a linked variable has been
 *	changed by C code.  It updates the Tcl variable so that
 *	traces on the variable will trigger.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Tcl variable "varName" is updated from its C value,
 *	causing traces on the variable to trigger.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UpdateLinkedVar(interp, varName)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *varName;		/* Name of global variable that is linked. */
{
    Link *linkPtr;
    char buffer[TCL_DOUBLE_SPACE];

    linkPtr = (Link *) Tcl_VarTraceInfo(interp, varName, TCL_GLOBAL_ONLY,
	    LinkTraceProc, (ClientData) NULL);
    if (linkPtr == NULL) {
	return;
    }
    Tcl_SetVar(interp, linkPtr->varName, StringValue(linkPtr, buffer),
	    TCL_GLOBAL_ONLY);
}

/*
 *----------------------------------------------------------------------
 *
 * LinkTraceProc --
 *
 *	This procedure is invoked when a linked Tcl variable is read,
 *	written, or unset from Tcl.  It's responsible for keeping the
 *	C variable in sync with the Tcl variable.
 *
 * Results:
 *	If all goes well, NULL is returned; otherwise an error message
 *	is returned.
 *
 * Side effects:
 *	The C variable may be updated to make it consistent with the
 *	Tcl variable, or the Tcl variable may be overwritten to reject
 *	a modification.
 *
 *----------------------------------------------------------------------
 */

static char *
LinkTraceProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Contains information about the link. */
    Tcl_Interp *interp;		/* Interpreter containing Tcl variable. */
    char *name1;		/* First part of variable name. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Miscellaneous additional information. */
{
    Link *linkPtr = (Link *) clientData;
    int changed;
    char buffer[TCL_DOUBLE_SPACE];
    char *value, **pp;
    Tcl_DString savedResult;

    /*
     * If the variable is being unset, then just re-create it (with a
     * trace) unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if (flags & TCL_INTERP_DESTROYED) {
	    ckfree(linkPtr->varName);
	    ckfree((char *) linkPtr);
	} else if (flags & TCL_TRACE_DESTROYED) {
	    Tcl_SetVar(interp, linkPtr->varName, StringValue(linkPtr, buffer),
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, linkPtr->varName, TCL_GLOBAL_ONLY
		    |TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    LinkTraceProc, (ClientData) linkPtr);
	}
	return NULL;
    }

    /*
     * For read accesses, update the Tcl variable if the C variable
     * has changed since the last time we updated the Tcl variable.
     */

    if (flags & TCL_TRACE_READS) {
	switch (linkPtr->type) {
	    case TCL_LINK_INT:
	    case TCL_LINK_BOOLEAN:
		changed = *(int *)(linkPtr->addr) != linkPtr->lastValue.i;
		break;
	    case TCL_LINK_DOUBLE:
		changed = *(double *)(linkPtr->addr) != linkPtr->lastValue.d;
		break;
	    case TCL_LINK_STRING:
		changed = 1;
		break;
	    default:
		return "internal error: bad linked variable type";
	}
	if (changed) {
	    Tcl_SetVar(interp, linkPtr->varName, StringValue(linkPtr, buffer),
		    TCL_GLOBAL_ONLY);
	}
	return NULL;
    }

    /*
     * For writes, first make sure that the variable is writable.  Then
     * convert the Tcl value to C if possible.  If the variable isn't
     * writable or can't be converted, then restore the varaible's old
     * value and return an error.  Another tricky thing: we have to save
     * and restore the interpreter's result, since the variable access
     * could occur when the result has been partially set.
     */

    if (!linkPtr->writable) {
	Tcl_SetVar(interp, linkPtr->varName, StringValue(linkPtr, buffer),
		TCL_GLOBAL_ONLY);
	return "linked variable is read-only";
    }
    value = Tcl_GetVar(interp, linkPtr->varName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	/*
	 * This shouldn't ever happen.
	 */
	return "internal error: linked variable couldn't be read";
    }
    Tcl_DStringInit(&savedResult);
    Tcl_DStringAppend(&savedResult, interp->result, -1);
    Tcl_ResetResult(interp);
    switch (linkPtr->type) {
	case TCL_LINK_INT:
	    if (Tcl_GetInt(interp, value, &linkPtr->lastValue.i) != TCL_OK) {
		Tcl_DStringResult(interp, &savedResult);
		Tcl_SetVar(interp, linkPtr->varName,
			StringValue(linkPtr, buffer), TCL_GLOBAL_ONLY);
		return "variable must have integer value";
	    }
	    *(int *)(linkPtr->addr) = linkPtr->lastValue.i;
	    break;
	case TCL_LINK_DOUBLE:
	    if (Tcl_GetDouble(interp, value, &linkPtr->lastValue.d)
		    != TCL_OK) {
		Tcl_DStringResult(interp, &savedResult);
		Tcl_SetVar(interp, linkPtr->varName,
			StringValue(linkPtr, buffer), TCL_GLOBAL_ONLY);
		return "variable must have real value";
	    }
	    *(double *)(linkPtr->addr) = linkPtr->lastValue.d;
	    break;
	case TCL_LINK_BOOLEAN:
	    if (Tcl_GetBoolean(interp, value, &linkPtr->lastValue.i)
		    != TCL_OK) {
		Tcl_DStringResult(interp, &savedResult);
		Tcl_SetVar(interp, linkPtr->varName,
			StringValue(linkPtr, buffer), TCL_GLOBAL_ONLY);
		return "variable must have boolean value";
	    }
	    *(int *)(linkPtr->addr) = linkPtr->lastValue.i;
	    break;
	case TCL_LINK_STRING:
	    pp = (char **)(linkPtr->addr);
	    if (*pp != NULL) {
		ckfree(*pp);
	    }
	    *pp = (char *) ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(*pp, value);
	    break;
	default:
	    return "internal error: bad linked variable type";
    }
    Tcl_DStringResult(interp, &savedResult);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * StringValue --
 *
 *	Converts the value of a C variable to a string for use in a
 *	Tcl variable to which it is linked.
 *
 * Results:
 *	The return value is a pointer
 to a string that represents
 *	the value of the C variable given by linkPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
StringValue(linkPtr, buffer)
    Link *linkPtr;		/* Structure describing linked variable. */
    char *buffer;		/* Small buffer to use for converting
				 * values.  Must have TCL_DOUBLE_SPACE
				 * bytes or more. */
{
    char *p;

    switch (linkPtr->type) {
	case TCL_LINK_INT:
	    linkPtr->lastValue.i = *(int *)(linkPtr->addr);
	    sprintf(buffer, "%d", linkPtr->lastValue.i);
	    return buffer;
	case TCL_LINK_DOUBLE:
	    linkPtr->lastValue.d = *(double *)(linkPtr->addr);
	    Tcl_PrintDouble(linkPtr->interp, linkPtr->lastValue.d, buffer);
	    return buffer;
	case TCL_LINK_BOOLEAN:
	    linkPtr->lastValue.i = *(int *)(linkPtr->addr);
	    if (linkPtr->lastValue.i != 0) {
		return "1";
	    }
	    return "0";
	case TCL_LINK_STRING:
	    p = *(char **)(linkPtr->addr);
	    if (p == NULL) {
		return "NULL";
	    }
	    return p;
    }

    /*
     * This code only gets executed if the link type is unknown
     * (shouldn't ever happen).
     */

    return "??";
}
