/* 
 * tclIndexObj.c --
 *
 *	This file implements objects of type "index".  This object type
 *	is used to lookup a keyword in a table of valid values and cache
 *	the index of the matching entry.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclIndexObj.c 1.4 97/02/11 13:30:01
 */

#include "tclInt.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static void		DupIndexInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static int		SetIndexFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static void		UpdateStringOfIndex _ANSI_ARGS_((Tcl_Obj *listPtr));

/*
 * The structure below defines the index Tcl object type by means of
 * procedures that can be invoked by generic object code.
 */

Tcl_ObjType tclIndexType = {
    "index",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,	/* freeIntRepProc */
    DupIndexInternalRep,	        /* dupIntRepProc */
    UpdateStringOfIndex,		/* updateStringProc */
    SetIndexFromAny			/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetIndexFromObj --
 *
 *	This procedure looks up an object's value in a table of strings
 *	and returns the index of the matching string, if any.
 *
 * Results:

 *	If the value of objPtr is identical to or a unique abbreviation
 *	for one of the entries in objPtr, then the return value is
 *	TCL_OK and the index of the matching entry is stored at
 *	*indexPtr.  If there isn't a proper match, then TCL_ERROR is
 *	returned and an error message is left in interp's result (unless
 *	interp is NULL).  The msg argument is used in the error
 *	message; for example, if msg has the value "option" then the
 *	error message will say something flag 'bad option "foo": must be
 *	...'
 *
 * Side effects:
 *	The result of the lookup is cached as the internal rep of
 *	objPtr, so that repeated lookups can be done quickly.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetIndexFromObj(interp, objPtr, tablePtr, msg, flags, indexPtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* Object containing the string to lookup. */
    char **tablePtr;		/* Array of strings to compare against the
				 * value of objPtr; last entry must be NULL
				 * and there must not be duplicate entries. */
    char *msg;			/* Identifying word to use in error messages. */
    int flags;			/* 0 or TCL_EXACT */
    int *indexPtr;		/* Place to store resulting integer index. */
{
    int index, length, i, numAbbrev;
    char *key, *p1, *p2, **entryPtr;
    Tcl_Obj *resultPtr;

    /*
     * See if there is a valid cached result from a previous lookup.
     */

    if ((objPtr->typePtr == &tclIndexType)
	    && (objPtr->internalRep.twoPtrValue.ptr1 == (VOID *) tablePtr)) {
	*indexPtr = (int) objPtr->internalRep.twoPtrValue.ptr2;
	return TCL_OK;
    }

    /*
     * Lookup the value of the object in the table.  Accept unique
     * abbreviations unless TCL_EXACT is set in flags.
     */

    key = Tcl_GetStringFromObj(objPtr, &length);
    index = -1;
    numAbbrev = 0;
    for (entryPtr = tablePtr, i = 0; *entryPtr != NULL; entryPtr++, i++) {
	for (p1 = key, p2 = *entryPtr; *p1 == *p2; p1++, p2++) {
	    if (*p1 == 0) {
		index = i;
		goto done;
	    }
	}
	if (*p1 == 0) {
	    /*
	     * The value is an abbreviation for this entry.  Continue
	     * checking other entries to make sure it's unique.  If we
	     * get more than one unique abbreviation, keep searching to
	     * see if there is an exact match, but remember the number
	     * of unique abbreviations and don't allow either.
	     */

	    numAbbrev++;
	    index = i;
	}
    }
    if ((flags & TCL_EXACT) || (numAbbrev != 1)) {
	goto error;
    }

    done:
    if ((objPtr->typePtr != NULL)
	    && (objPtr->typePtr->freeIntRepProc != NULL)) {
	objPtr->typePtr->freeIntRepProc(objPtr);
    }
    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) tablePtr;
    objPtr->internalRep.twoPtrValue.ptr2 = (VOID *) index;
    objPtr->typePtr = &tclIndexType;
    *indexPtr = index;
    return TCL_OK;

    error:
    if (interp != NULL) {
	resultPtr = Tcl_GetObjResult(interp);
	Tcl_AppendStringsToObj(resultPtr,
		(numAbbrev > 1) ? "ambiguous " : "bad ", msg, " \"",
		key, "\": must be ", *tablePtr, (char *) NULL);
	for (entryPtr = tablePtr+1; *entryPtr != NULL; entryPtr++) {
	    if (entryPtr[1] == NULL) {
		Tcl_AppendStringsToObj(resultPtr, ", or ", *entryPtr,
			(char *) NULL);
	    } else {
		Tcl_AppendStringsToObj(resultPtr, ", ", *entryPtr,
			(char *) NULL);
	    }
	}
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DupIndexInternalRep --
 *
 *	Copy the internal representation of an index Tcl_Obj from one
 *	object to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to same value as "srcPtr"s
 *	internal rep.
 *
 *----------------------------------------------------------------------
 */

static void
DupIndexInternalRep(srcPtr, copyPtr)
    register Tcl_Obj *srcPtr;	/* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr;	/* Object with internal rep to set. */
{
    copyPtr->internalRep.twoPtrValue.ptr1
	    = srcPtr->internalRep.twoPtrValue.ptr1;
    copyPtr->internalRep.twoPtrValue.ptr2
	    = srcPtr->internalRep.twoPtrValue.ptr2;
    copyPtr->typePtr = &tclIndexType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetIndexFromAny --
 *
 *	This procedure is called to convert a Tcl object to index
 *	internal form. However, this doesn't make sense (need to have a
 *	table of keywords in order to do the conversion) so the
 *	procedure always generates an error.
 *
 * Results:
 *	The return value is always TCL_ERROR, and an error message is
 *	left in interp's result if interp isn't NULL. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SetIndexFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	    "can't convert value to index except via Tcl_GetIndexFromObj API",
	    -1);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfIndex --
 *
 *	This procedure is called to update the string representation for
 *	an index object.  It should never be called, because we never
 *	invalidate the string representation for an index object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A panic is added
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfIndex(objPtr)
    register Tcl_Obj *objPtr;	/* Int object whose string rep to update. */
{
    panic("UpdateStringOfIndex should never be invoked");
}
