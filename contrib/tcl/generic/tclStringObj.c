/* 
 * tclStringObj.c --
 *
 *	This file contains procedures that implement string operations
 *	on Tcl objects.  To do this efficiently (i.e. to allow many
 *	appends to be done to an object without constantly reallocating
 *	the space for the string representation) we overallocate the
 *	space for the string and use the internal representation to keep
 *	track of the extra space.  Objects with this internal
 *	representation are called "expandable string objects".
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclStringObj.c 1.31 97/10/30 13:56:35
 */

#include "tclInt.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static void		ConvertToStringType _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		DupStringInternalRep _ANSI_ARGS_((Tcl_Obj *objPtr,
			    Tcl_Obj *copyPtr));
static int		SetStringFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static void		UpdateStringOfString _ANSI_ARGS_((Tcl_Obj *objPtr));

/*
 * The structure below defines the string Tcl object type by means of
 * procedures that can be invoked by generic object code.
 */

Tcl_ObjType tclStringType = {
    "string",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,	/* freeIntRepProc */
    DupStringInternalRep,		/* dupIntRepProc */
    UpdateStringOfString,		/* updateStringProc */
    SetStringFromAny			/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewStringObj --
 *
 *	This procedure is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new string object and
 *	initializes it from the byte pointer and length arguments.
 *
 *	When TCL_MEM_DEBUG is defined, this procedure just returns the
 *	result of calling the debugging version Tcl_DbNewStringObj.
 *
 * Results:
 *	A newly created string object is returned that has ref count zero.
 *
 * Side effects:
 *	The new object's internal string representation will be set to a
 *	copy of the length bytes starting at "bytes". If "length" is
 *	negative, use bytes up to the first NULL byte; i.e., assume "bytes"
 *	points to a C-style NULL-terminated string. The object's type is set
 *	to NULL. An extra NULL is added to the end of the new object's byte
 *	array.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewStringObj

Tcl_Obj *
Tcl_NewStringObj(bytes, length)
    register char *bytes;	/* Points to the first of the length bytes
				 * used to initialize the new object. */
    register int length;	/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If 
				 * negative, use bytes up to the first
				 * NULL byte. */
{
    return Tcl_DbNewStringObj(bytes, length, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewStringObj(bytes, length)
    register char *bytes;	/* Points to the first of the length bytes
				 * used to initialize the new object. */
    register int length;	/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If 
				 * negative, use bytes up to the first
				 * NULL byte. */
{
    register Tcl_Obj *objPtr;

    if (length < 0) {
	length = (bytes? strlen(bytes) : 0);
    }
    TclNewObj(objPtr);
    TclInitStringRep(objPtr, bytes, length);
    return objPtr;
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewStringObj --
 *
 *	This procedure is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new string objects. It is the
 *	same as the Tcl_NewStringObj procedure above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the checkmem command
 *	will report the correct file name and line number when reporting
 *	objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this procedure just returns the
 *	result of calling Tcl_NewStringObj.
 *
 * Results:
 *	A newly created string object is returned that has ref count zero.
 *
 * Side effects:
 *	The new object's internal string representation will be set to a
 *	copy of the length bytes starting at "bytes". If "length" is
 *	negative, use bytes up to the first NULL byte; i.e., assume "bytes"
 *	points to a C-style NULL-terminated string. The object's type is set
 *	to NULL. An extra NULL is added to the end of the new object's byte
 *	array.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewStringObj(bytes, length, file, line)
    register char *bytes;	/* Points to the first of the length bytes
				 * used to initialize the new object. */
    register int length;	/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If 
				 * negative, use bytes up to the first
				 * NULL byte. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    register Tcl_Obj *objPtr;

    if (length < 0) {
	length = (bytes? strlen(bytes) : 0);
    }
    TclDbNewObj(objPtr, file, line);
    TclInitStringRep(objPtr, bytes, length);
    return objPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewStringObj(bytes, length, file, line)
    register char *bytes;	/* Points to the first of the length bytes
				 * used to initialize the new object. */
    register int length;	/* The number of bytes to copy from "bytes"
				 * when initializing the new object. If 
				 * negative, use bytes up to the first
				 * NULL byte. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    return Tcl_NewStringObj(bytes, length);
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetStringObj --
 *
 *	Modify an object to hold a string that is a copy of the bytes
 *	indicated by the byte pointer and length arguments. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string representation will be set to a copy of
 *	the "length" bytes starting at "bytes". If "length" is negative, use
 *	bytes up to the first NULL byte; i.e., assume "bytes" points to a
 *	C-style NULL-terminated string. The object's old string and internal
 *	representations are freed and the object's type is set NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetStringObj(objPtr, bytes, length)
    register Tcl_Obj *objPtr;	/* Object whose internal rep to init. */
    char *bytes;		/* Points to the first of the length bytes
				 * used to initialize the object. */
    register int length;	/* The number of bytes to copy from "bytes"
				 * when initializing the object. If 
				 * negative, use bytes up to the first
				 * NULL byte.*/
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    /*
     * Free any old string rep, then set the string rep to a copy of
     * the length bytes starting at "bytes".
     */

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetStringObj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if (length < 0) {
	length = strlen(bytes);
    }
    TclInitStringRep(objPtr, bytes, length);
        
    /*
     * Set the type to NULL and free any internal rep for the old type.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    objPtr->typePtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetObjLength --
 *
 *	This procedure changes the length of the string representation
 *	of an object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the size of objPtr's string representation is greater than
 *	length, then it is reduced to length and a new terminating null
 *	byte is stored in the strength.  If the length of the string
 *	representation is greater than length, the storage space is
 *	reallocated to the given length; a null byte is stored at the
 *	end, but other bytes past the end of the original string
 *	representation are undefined.  The object's internal
 *	representation is changed to "expendable string".
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetObjLength(objPtr, length)
    register Tcl_Obj *objPtr;	/* Pointer to object.  This object must
				 * not currently be shared. */
    register int length;	/* Number of bytes desired for string
				 * representation of object, not including
				 * terminating null byte. */
{
    char *new;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetObjLength called with shared object");
    }
    if (objPtr->typePtr != &tclStringType) {
	ConvertToStringType(objPtr);
    }
    
    if ((long)length > objPtr->internalRep.longValue) {
	/*
	 * Not enough space in current string. Reallocate the string
	 * space and free the old string.
	 */

	new = (char *) ckalloc((unsigned) (length+1));
	if (objPtr->bytes != NULL) {
	    memcpy((VOID *) new, (VOID *) objPtr->bytes,
		    (size_t) objPtr->length);
	    Tcl_InvalidateStringRep(objPtr);
	}
	objPtr->bytes = new;
	objPtr->internalRep.longValue = (long) length;
    }
    objPtr->length = length;
    if ((objPtr->bytes != NULL) && (objPtr->bytes != tclEmptyStringRep)) {
	objPtr->bytes[length] = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendToObj --
 *
 *	This procedure appends a sequence of bytes to an object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The bytes at *bytes are appended to the string representation
 *	of objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendToObj(objPtr, bytes, length)
    register Tcl_Obj *objPtr;	/* Points to the object to append to. */
    char *bytes;		/* Points to the bytes to append to the
				 * object. */
    register int length;	/* The number of bytes to append from
				 * "bytes". If < 0, then append all bytes
				 * up to NULL byte. */
{
    int newLength, oldLength;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_AppendToObj called with shared object");
    }
    if (objPtr->typePtr != &tclStringType) {
	ConvertToStringType(objPtr);
    }
    if (length < 0) {
	length = strlen(bytes);
    }
    if (length == 0) {
	return;
    }
    oldLength = objPtr->length;
    newLength = length + oldLength;
    if ((long)newLength > objPtr->internalRep.longValue) {
	/*
	 * There isn't currently enough space in the string
	 * representation so allocate additional space.  In fact,
	 * overallocate so that there is room for future growth without
	 * having to reallocate again.
	 */

	Tcl_SetObjLength(objPtr, 2*newLength);
    }
    if (length > 0) {
	memcpy((VOID *) (objPtr->bytes + oldLength), (VOID *) bytes,
	       (size_t) length);
	objPtr->length = newLength;
	objPtr->bytes[objPtr->length] = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendStringsToObj --
 *
 *	This procedure appends one or more null-terminated strings
 *	to an object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of all the string arguments are appended to the
 *	string representation of objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendStringsToObj TCL_VARARGS_DEF(Tcl_Obj *,arg1)
{
    va_list argList;
    register Tcl_Obj *objPtr;
    int newLength, oldLength;
    register char *string, *dst;

    objPtr = (Tcl_Obj *) TCL_VARARGS_START(Tcl_Obj *,arg1,argList);
    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_AppendStringsToObj called with shared object");
    }
    if (objPtr->typePtr != &tclStringType) {
	ConvertToStringType(objPtr);
    }

    /*
     * Figure out how much space is needed for all the strings, and
     * expand the string representation if it isn't big enough. If no
     * bytes would be appended, just return.
     */

    newLength = oldLength = objPtr->length;
    while (1) {
	string = va_arg(argList, char *);
	if (string == NULL) {
	    break;
	}
	newLength += strlen(string);
    }
    if (newLength == oldLength) {
	return;
    }

    if ((long)newLength > objPtr->internalRep.longValue) {
	/*
	 * There isn't currently enough space in the string
	 * representation so allocate additional space.  If the current
	 * string representation isn't empty (i.e. it looks like we're
	 * doing a series of appends) then overallocate the space so
	 * that we won't have to do as much reallocation in the future.
	 */

	Tcl_SetObjLength(objPtr,
		(objPtr->length == 0) ? newLength : 2*newLength);
    }

    /*
     * Make a second pass through the arguments, appending all the
     * strings to the object.
     */

    TCL_VARARGS_START(Tcl_Obj *,arg1,argList);
    dst = objPtr->bytes + oldLength;
    while (1) {
	string = va_arg(argList, char *);
	if (string == NULL) {
	    break;
	}
	while (*string != 0) {
	    *dst = *string;
	    dst++;
	    string++;
	}
    }

    /*
     * Add a null byte to terminate the string.  However, be careful:
     * it's possible that the object is totally empty (if it was empty
     * originally and there was nothing to append).  In this case dst is
     * NULL; just leave everything alone.
     */

    if (dst != NULL) {
	*dst = 0;
    }
    objPtr->length = newLength;
    va_end(argList);
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertToStringType --
 *
 *	This procedure converts the internal representation of an object
 *	to "expandable string" type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any old internal reputation for objPtr is freed and the
 *	internal representation is set to that for an expandable string
 *	(the field internalRep.longValue holds 1 less than the allocated
 *	length of objPtr's string representation).
 *
 *----------------------------------------------------------------------
 */

static void
ConvertToStringType(objPtr)
    register Tcl_Obj *objPtr;	/* Pointer to object.  Must have a
				 * typePtr that isn't &tclStringType. */
{
    if (objPtr->typePtr != NULL) {
	if (objPtr->bytes == NULL) {
	    objPtr->typePtr->updateStringProc(objPtr);
	}
	if (objPtr->typePtr->freeIntRepProc != NULL) {
	    objPtr->typePtr->freeIntRepProc(objPtr);
	}
    }
    objPtr->typePtr = &tclStringType;
    if (objPtr->bytes != NULL) {
	objPtr->internalRep.longValue = (long)objPtr->length;
    } else {
	objPtr->internalRep.longValue = 0;
	objPtr->length = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupStringInternalRep --
 *
 *	Initialize the internal representation of a new Tcl_Obj to a
 *	copy of the internal representation of an existing string object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	copyPtr's internal rep is set to a copy of srcPtr's internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static void
DupStringInternalRep(srcPtr, copyPtr)
    register Tcl_Obj *srcPtr;	/* Object with internal rep to copy.  Must
				 * have an internal representation of type
				 * "expandable string". */
    register Tcl_Obj *copyPtr;	/* Object with internal rep to set.  Must
				 * not currently have an internal rep.*/
{
    /*
     * Tricky point: the string value was copied by generic object
     * management code, so it doesn't contain any extra bytes that
     * might exist in the source object.
     */

    copyPtr->internalRep.longValue = (long)copyPtr->length;
    copyPtr->typePtr = &tclStringType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetStringFromAny --
 *
 *	Create an internal representation of type "expandable string"
 *	for an object.
 *
 * Results:
 *	This operation always succeeds and returns TCL_OK.
 *
 * Side effects:
 *	This procedure does nothing; there is no advantage in converting
 *	the internal representation now, so we just defer it.
 *
 *----------------------------------------------------------------------
 */

static int
SetStringFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfString --
 *
 *	Update the string representation for an object whose internal
 *	representation is "expandable string".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfString(objPtr)
    Tcl_Obj *objPtr;		/* Object with string rep to update. */
{
    /*
     * The string is almost always valid already, in which case there's
     * nothing for us to do. The only case we have to worry about is if
     * the object is totally null. In this case, set the string rep to
     * an empty string.
     */

    if (objPtr->bytes == NULL) {
	objPtr->bytes = tclEmptyStringRep;
	objPtr->length = 0;
    }
    return;
}
