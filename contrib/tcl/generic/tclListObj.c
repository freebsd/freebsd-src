/* 
 * tclListObj.c --
 *
 *	This file contains procedures that implement the Tcl list object
 *	type.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclListObj.c 1.44 97/06/13 18:25:32
 */

#include "tclInt.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static void		DupListInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		FreeListInternalRep _ANSI_ARGS_((Tcl_Obj *listPtr));
static int		SetListFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static void		UpdateStringOfList _ANSI_ARGS_((Tcl_Obj *listPtr));

/*
 * The structure below defines the list Tcl object type by means of
 * procedures that can be invoked by generic object code.
 */

Tcl_ObjType tclListType = {
    "list",				/* name */
    FreeListInternalRep,		/* freeIntRepProc */
    DupListInternalRep,		        /* dupIntRepProc */
    UpdateStringOfList,			/* updateStringProc */
    SetListFromAny			/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewListObj --
 *
 *	This procedure is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new list object from an
 *	(objc,objv) array: that is, each of the objc elements of the array
 *	referenced by objv is inserted as an element into a new Tcl object.
 *
 *	When TCL_MEM_DEBUG is defined, this procedure just returns the
 *	result of calling the debugging version Tcl_DbNewListObj.
 *
 * Results:
 *	A new list object is returned that is initialized from the object
 *	pointers in objv. If objc is less than or equal to zero, an empty
 *	object is returned. The new object's string representation
 *	is left NULL. The resulting new list object has ref count 0.
 *
 * Side effects:
 *	The ref counts of the elements in objv are incremented since the
 *	resulting list now refers to them.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewListObj

Tcl_Obj *
Tcl_NewListObj(objc, objv)
    int objc;			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[];	/* An array of pointers to Tcl objects. */
{
    return Tcl_DbNewListObj(objc, objv, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewListObj(objc, objv)
    int objc;			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[];	/* An array of pointers to Tcl objects. */
{
    register Tcl_Obj *listPtr;
    register Tcl_Obj **elemPtrs;
    register List *listRepPtr;
    int i;
    
    TclNewObj(listPtr);
    
    if (objc > 0) {
	Tcl_InvalidateStringRep(listPtr);
	
	elemPtrs = (Tcl_Obj **)
	    ckalloc((unsigned) (objc * sizeof(Tcl_Obj *)));
	for (i = 0;  i < objc;  i++) {
	    elemPtrs[i] = objv[i];
	    Tcl_IncrRefCount(elemPtrs[i]);
	}
	
	listRepPtr = (List *) ckalloc(sizeof(List));
	listRepPtr->maxElemCount = objc;
	listRepPtr->elemCount    = objc;
	listRepPtr->elements     = elemPtrs;
	
	listPtr->internalRep.otherValuePtr = (VOID *) listRepPtr;
	listPtr->typePtr = &tclListType;
    }
    return listPtr;
}
#endif /* if TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewListObj --
 *
 *	This procedure is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new list objects. It is the
 *	same as the Tcl_NewListObj procedure above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the checkmem command
 *	will report the correct file name and line number when reporting
 *	objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this procedure just returns the
 *	result of calling Tcl_NewListObj.
 *
 * Results:
 *	A new list object is returned that is initialized from the object
 *	pointers in objv. If objc is less than or equal to zero, an empty
 *	object is returned. The new object's string representation
 *	is left NULL. The new list object has ref count 0.
 *
 * Side effects:
 *	The ref counts of the elements in objv are incremented since the
 *	resulting list now refers to them.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewListObj(objc, objv, file, line)
    int objc;			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[];	/* An array of pointers to Tcl objects. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    register Tcl_Obj *listPtr;
    register Tcl_Obj **elemPtrs;
    register List *listRepPtr;
    int i;
    
    TclDbNewObj(listPtr, file, line);
    
    if (objc > 0) {
	Tcl_InvalidateStringRep(listPtr);
	
	elemPtrs = (Tcl_Obj **)
	    ckalloc((unsigned) (objc * sizeof(Tcl_Obj *)));
	for (i = 0;  i < objc;  i++) {
	    elemPtrs[i] = objv[i];
	    Tcl_IncrRefCount(elemPtrs[i]);
	}
	
	listRepPtr = (List *) ckalloc(sizeof(List));
	listRepPtr->maxElemCount = objc;
	listRepPtr->elemCount    = objc;
	listRepPtr->elements     = elemPtrs;
	
	listPtr->internalRep.otherValuePtr = (VOID *) listRepPtr;
	listPtr->typePtr = &tclListType;
    }
    return listPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewListObj(objc, objv, file, line)
    int objc;			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[];	/* An array of pointers to Tcl objects. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    return Tcl_NewListObj(objc, objv);
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetListObj --
 *
 *	Modify an object to be a list containing each of the objc elements
 *	of the object array referenced by objv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object is made a list object and is initialized from the object
 *	pointers in objv. If objc is less than or equal to zero, an empty
 *	object is returned. The new object's string representation
 *	is left NULL. The ref counts of the elements in objv are incremented
 *	since the list now refers to them. The object's old string and
 *	internal representations are freed and its type is set NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetListObj(objPtr, objc, objv)
    Tcl_Obj *objPtr;		/* Object whose internal rep to init. */
    int objc;			/* Count of objects referenced by objv. */
    Tcl_Obj *CONST objv[];	/* An array of pointers to Tcl objects. */
{
    register Tcl_Obj **elemPtrs;
    register List *listRepPtr;
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    int i;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetListObj called with shared object");
    }
    
    /*
     * Free any old string rep and any internal rep for the old type.
     */

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
	objPtr->typePtr = NULL;
    }
        
    /*
     * Set the object's type to "list" and initialize the internal rep.
     */

    if (objc > 0) {
	elemPtrs = (Tcl_Obj **)
	    ckalloc((unsigned) (objc * sizeof(Tcl_Obj *)));
	for (i = 0;  i < objc;  i++) {
	    elemPtrs[i] = objv[i];
	    Tcl_IncrRefCount(elemPtrs[i]);
	}
	
	listRepPtr = (List *) ckalloc(sizeof(List));
	listRepPtr->maxElemCount = objc;
	listRepPtr->elemCount    = objc;
	listRepPtr->elements     = elemPtrs;
	
	objPtr->internalRep.otherValuePtr = (VOID *) listRepPtr;
	objPtr->typePtr = &tclListType;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjGetElements --
 *
 *	This procedure returns an (objc,objv) array of the elements in a
 *	list object.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *objcPtr is set to
 *	the count of list elements and *objvPtr is set to a pointer to an
 *	array of (*objcPtr) pointers to each list element. If listPtr does
 *	not refer to a list object and the object can not be converted to
 *	one, TCL_ERROR is returned and an error message will be left in
 *	the interpreter's result if interp is not NULL.
 *
 *	The objects referenced by the returned array should be treated as
 *	readonly and their ref counts are _not_ incremented; the caller must
 *	do that if it holds on to a reference. Furthermore, the pointer
 *	and length returned by this procedure may change as soon as any
 *	procedure is called on the list object; be careful about retaining
 *	the pointer in a local data structure.
 *
 * Side effects:
 *	The possible conversion of the object referenced by listPtr
 *	to a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjGetElements(interp, listPtr, objcPtr, objvPtr)
    Tcl_Interp *interp;		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr;	/* List object for which an element array
				 * is to be returned. */
    int *objcPtr;		/* Where to store the count of objects
				 * referenced by objv. */
    Tcl_Obj ***objvPtr;		/* Where to store the pointer to an array
				 * of pointers to the list's objects. */
{
    register List *listRepPtr;

    if (listPtr->typePtr != &tclListType) {
	int result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    *objcPtr = listRepPtr->elemCount;
    *objvPtr = listRepPtr->elements;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjAppendList --
 *
 *	This procedure appends the objects in the list referenced by
 *	elemListPtr to the list object referenced by listPtr. If listPtr is
 *	not already a list object, an attempt will be made to convert it to
 *	one.
 *
 * Results:
 *	The return value is normally TCL_OK. If listPtr or elemListPtr do
 *	not refer to list objects and they can not be converted to one,
 *	TCL_ERROR is returned and an error message is left in
 *	the interpreter's result if interp is not NULL.
 *
 * Side effects:
 *	The reference counts of the elements in elemListPtr are incremented
 *	since the list now refers to them. listPtr and elemListPtr are
 *	converted, if necessary, to list objects. Also, appending the
 *	new elements may cause listObj's array of element pointers to grow.
 *	listPtr's old string representation, if any, is invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjAppendList(interp, listPtr, elemListPtr)
    Tcl_Interp *interp;		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr;	/* List object to append elements to. */
    Tcl_Obj *elemListPtr;	/* List obj with elements to append. */
{
    register List *listRepPtr;
    int listLen, objc, result;
    Tcl_Obj **objv;

    if (Tcl_IsShared(listPtr)) {
	panic("Tcl_ListObjAppendList called with shared object");
    }
    if (listPtr->typePtr != &tclListType) {
	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    listLen = listRepPtr->elemCount;

    result = Tcl_ListObjGetElements(interp, elemListPtr, &objc, &objv);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Insert objc new elements starting after the lists's last element.
     * Delete zero existing elements.
     */
    
    return Tcl_ListObjReplace(interp, listPtr, listLen, 0, objc, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjAppendElement --
 *
 *	This procedure is a special purpose version of
 *	Tcl_ListObjAppendList: it appends a single object referenced by
 *	objPtr to the list object referenced by listPtr. If listPtr is not
 *	already a list object, an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case objPtr is added
 *	to the end of listPtr's list. If listPtr does not refer to a list
 *	object and the object can not be converted to one, TCL_ERROR is
 *	returned and an error message will be left in the interpreter's
 *	result if interp is not NULL.
 *
 * Side effects:
 *	The ref count of objPtr is incremented since the list now refers 
 *	to it. listPtr will be converted, if necessary, to a list object.
 *	Also, appending the new element may cause listObj's array of element
 *	pointers to grow. listPtr's old string representation, if any,
 *	is invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjAppendElement(interp, listPtr, objPtr)
    Tcl_Interp *interp;		/* Used to report errors if not NULL. */
    Tcl_Obj *listPtr;		/* List object to append objPtr to. */
    Tcl_Obj *objPtr;		/* Object to append to listPtr's list. */
{
    register List *listRepPtr;
    register Tcl_Obj **elemPtrs;
    int numElems;
    
    if (Tcl_IsShared(listPtr)) {
	panic("Tcl_ListObjAppendElement called with shared object");
    }
    if (listPtr->typePtr != &tclListType) {
	int result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    elemPtrs = listRepPtr->elements;
    numElems = listRepPtr->elemCount;
    
    /*
     * If there is no room in the current array of element pointers,
     * allocate a new, larger array and copy the pointers to it.
     */

    if (numElems >= listRepPtr->maxElemCount) {
	int numRequired = (numElems + 1);
	int newMax = (2 * numRequired);
	Tcl_Obj **newElemPtrs = (Tcl_Obj **)
	    ckalloc((unsigned) (newMax * sizeof(Tcl_Obj *)));
	
	memcpy((VOID *) newElemPtrs, (VOID *) elemPtrs,
	       (size_t) (numElems * sizeof(Tcl_Obj *)));

	listRepPtr->maxElemCount = newMax;
	listRepPtr->elements = newElemPtrs;
	ckfree((char *) elemPtrs);
	elemPtrs = newElemPtrs;
    }

    /*
     * Add objPtr to the end of listPtr's array of element
     * pointers. Increment the ref count for the (now shared) objPtr.
     */

    elemPtrs[numElems] = objPtr;
    Tcl_IncrRefCount(objPtr);
    listRepPtr->elemCount++;

    /*
     * Invalidate any old string representation since the list's internal
     * representation has changed.
     */

    Tcl_InvalidateStringRep(listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjIndex --
 *
 *	This procedure returns a pointer to the index'th object from the
 *	list referenced by listPtr. The first element has index 0. If index
 *	is negative or greater than or equal to the number of elements in
 *	the list, a NULL is returned. If listPtr is not a list object, an
 *	attempt will be made to convert it to a list.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case objPtrPtr is set
 *	to the Tcl_Obj pointer for the index'th list element or NULL if
 *	index is out of range. This object should be treated as readonly and
 *	its ref count is _not_ incremented; the caller must do that if it
 *	holds on to the reference. If listPtr does not refer to a list and
 *	can't be converted to one, TCL_ERROR is returned and an error
 *	message is left in the interpreter's result if interp is not NULL.
 *
 * Side effects:
 *	listPtr will be converted, if necessary, to a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjIndex(interp, listPtr, index, objPtrPtr)
    Tcl_Interp *interp;		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr;	/* List object to index into. */
    register int index;		/* Index of element to return. */
    Tcl_Obj **objPtrPtr;	/* The resulting Tcl_Obj* is stored here. */
{
    register List *listRepPtr;
    
    if (listPtr->typePtr != &tclListType) {
	int result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    if ((index < 0) || (index >= listRepPtr->elemCount)) {
	*objPtrPtr = NULL;
    } else {
	*objPtrPtr = listRepPtr->elements[index];
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjLength --
 *
 *	This procedure returns the number of elements in a list object. If
 *	the object is not already a list object, an attempt will be made to
 *	convert it to one.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *intPtr will be
 *	set to the integer count of list elements. If listPtr does not refer
 *	to a list object and the object can not be converted to one,
 *	TCL_ERROR is returned and an error message will be left in
 *	the interpreter's result if interp is not NULL.
 *
 * Side effects:
 *	The possible conversion of the argument object to a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjLength(interp, listPtr, intPtr)
    Tcl_Interp *interp;		/* Used to report errors if not NULL. */
    register Tcl_Obj *listPtr;	/* List object whose #elements to return. */
    register int *intPtr;	/* The resulting int is stored here. */
{
    register List *listRepPtr;
    
    if (listPtr->typePtr != &tclListType) {
	int result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    *intPtr = listRepPtr->elemCount;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjReplace --
 * 
 *	This procedure replaces zero or more elements of the list referenced
 *	by listPtr with the objects from an (objc,objv) array. 
 *	The objc elements of the array referenced by objv replace the
 *	count elements in listPtr starting at first.
 *
 *	If the argument first is zero or negative, it refers to the first
 *	element. If first is greater than or equal to the number of elements
 *	in the list, then no elements are deleted; the new elements are
 *	appended to the list. Count gives the number of elements to
 *	replace. If count is zero or negative then no elements are deleted;
 *	the new elements are simply inserted before first.
 *
 *	The argument objv refers to an array of objc pointers to the new
 *	elements to be added to listPtr in place of those that were
 *	deleted. If objv is NULL, no new elements are added. If listPtr is
 *	not a list object, an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is normally TCL_OK. If listPtr does
 *	not refer to a list object and can not be converted to one,
 *	TCL_ERROR is returned and an error message will be left in
 *	the interpreter's result if interp is not NULL.
 *
 * Side effects:
 *	The ref counts of the objc elements in objv are incremented since
 *	the resulting list now refers to them. Similarly, the ref counts for
 *	replaced objects are decremented. listPtr is converted, if
 *	necessary, to a list object. listPtr's old string representation, if
 *	any, is freed. 
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ListObjReplace(interp, listPtr, first, count, objc, objv)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *listPtr;		/* List object whose elements to replace. */
    int first;			/* Index of first element to replace. */
    int count;			/* Number of elements to replace. */
    int objc;			/* Number of objects to insert. */
    Tcl_Obj *CONST objv[];	/* An array of objc pointers to Tcl objects
				 * to insert. */
{
    List *listRepPtr;
    register Tcl_Obj **elemPtrs, **newPtrs;
    Tcl_Obj *victimPtr;
    int numElems, numRequired, numAfterLast;
    int start, shift, newMax, i, j, result;
     
    if (Tcl_IsShared(listPtr)) {
	panic("Tcl_ListObjReplace called with shared object");
    }
    if (listPtr->typePtr != &tclListType) {
	result = SetListFromAny(interp, listPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    elemPtrs = listRepPtr->elements;
    numElems = listRepPtr->elemCount;

    if (first < 0)  {
    	first = 0;
    }
    if (first >= numElems) {
	first = numElems;	/* so we'll insert after last element */
    }
    if (count < 0) {
	count = 0;
    }
    
    numRequired = (numElems - count + objc);
    if (numRequired < listRepPtr->maxElemCount) {
	/*
	 * Enough room in the current array. First "delete" count
	 * elements starting at first.
	 */

	for (i = 0, j = first;  i < count;  i++, j++) {
	    victimPtr = elemPtrs[j];
	    TclDecrRefCount(victimPtr);
	}

	/*
	 * Shift the elements after the last one removed to their
	 * new locations.
	 */

	start = (first + count);
	numAfterLast = (numElems - start);
	shift = (objc - count);	/* numNewElems - numDeleted */
	if ((numAfterLast > 0) && (shift != 0)) {
	    Tcl_Obj **src, **dst;

	    if (shift < 0) {
		for (src = elemPtrs + start, dst = src + shift;
			numAfterLast > 0; numAfterLast--, src++, dst++) {
		    *dst = *src;
		}
	    } else {
		for (src = elemPtrs + numElems - 1, dst = src + shift;
			numAfterLast > 0; numAfterLast--, src--, dst--) {
		    *dst = *src;
		}
	    }
	}

	/*
	 * Insert the new elements into elemPtrs before "first".
	 */

	for (i = 0, j = first;  i < objc;  i++, j++) {
            elemPtrs[j] = objv[i];
            Tcl_IncrRefCount(objv[i]);
        }

	/*
	 * Update the count of elements.
	 */

	listRepPtr->elemCount = numRequired;
    } else {
	/*
	 * Not enough room in the current array. Allocate a larger array and
	 * insert elements into it. 
	 */

	newMax = (2 * numRequired);
	newPtrs = (Tcl_Obj **)
	    ckalloc((unsigned) (newMax * sizeof(Tcl_Obj *)));

	/*
	 * Copy over the elements before "first".
	 */

	if (first > 0) {
	    memcpy((VOID *) newPtrs, (VOID *) elemPtrs,
		    (size_t) (first * sizeof(Tcl_Obj *)));
	}

	/*
	 * "Delete" count elements starting at first.
	 */

	for (i = 0, j = first;  i < count;  i++, j++) {
	    victimPtr = elemPtrs[j];
	    TclDecrRefCount(victimPtr);
	}

	/*
	 * Copy the elements after the last one removed, shifted to
	 * their new locations.
	 */

	start = (first + count);
	numAfterLast = (numElems - start);
	if (numAfterLast > 0) {
	    memcpy((VOID *) &(newPtrs[first + objc]),
		    (VOID *) &(elemPtrs[start]),
		    (size_t) (numAfterLast * sizeof(Tcl_Obj *)));
	}
	
	/*
	 * Insert the new elements before "first" and update the
	 * count of elements.
	 */

	for (i = 0, j = first;  i < objc;  i++, j++) {
	    newPtrs[j] = objv[i];
	    Tcl_IncrRefCount(objv[i]);
	}

	listRepPtr->elemCount = numRequired;
	listRepPtr->maxElemCount = newMax;
	listRepPtr->elements = newPtrs;
	ckfree((char *) elemPtrs);
    }
    
    /*
     * Invalidate and free any old string representation since it no longer
     * reflects the list's internal representation.
     */

    Tcl_InvalidateStringRep(listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeListInternalRep --
 *
 *	Deallocate the storage associated with a list object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees listPtr's List* internal representation and sets listPtr's
 *	internalRep.otherValuePtr to NULL. Decrements the ref counts
 *	of all element objects, which may free them.
 *
 *----------------------------------------------------------------------
 */

static void
FreeListInternalRep(listPtr)
    Tcl_Obj *listPtr;		/* List object with internal rep to free. */
{
    register List *listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    register Tcl_Obj **elemPtrs = listRepPtr->elements;
    register Tcl_Obj *objPtr;
    int numElems = listRepPtr->elemCount;
    int i;
    
    for (i = 0;  i < numElems;  i++) {
	objPtr = elemPtrs[i];
	Tcl_DecrRefCount(objPtr);
    }
    ckfree((char *) elemPtrs);
    ckfree((char *) listRepPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DupListInternalRep --
 *
 *	Initialize the internal representation of a list Tcl_Obj to a
 *	copy of the internal representation of an existing list object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"srcPtr"s list internal rep pointer should not be NULL and we assume
 *	it is not NULL. We set "copyPtr"s internal rep to a pointer to a
 *	newly allocated List structure that, in turn, points to "srcPtr"s
 *	element objects. Those element objects are not actually copied but
 *	are shared between "srcPtr" and "copyPtr". The ref count of each
 *	element object is incremented.
 *
 *----------------------------------------------------------------------
 */

static void
DupListInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    List *srcListRepPtr = (List *) srcPtr->internalRep.otherValuePtr;
    int numElems = srcListRepPtr->elemCount;
    int maxElems = srcListRepPtr->maxElemCount;
    register Tcl_Obj **srcElemPtrs = srcListRepPtr->elements;
    register Tcl_Obj **copyElemPtrs;
    register List *copyListRepPtr;
    int i;

    /*
     * Allocate a new List structure that points to "srcPtr"s element
     * objects. Increment the ref counts for those (now shared) element
     * objects.
     */
    
    copyElemPtrs = (Tcl_Obj **)
	ckalloc((unsigned) maxElems * sizeof(Tcl_Obj *));
    for (i = 0;  i < numElems;  i++) {
	copyElemPtrs[i] = srcElemPtrs[i];
	Tcl_IncrRefCount(copyElemPtrs[i]);
    }
    
    copyListRepPtr = (List *) ckalloc(sizeof(List));
    copyListRepPtr->maxElemCount = maxElems;
    copyListRepPtr->elemCount    = numElems;
    copyListRepPtr->elements     = copyElemPtrs;
    
    copyPtr->internalRep.otherValuePtr = (VOID *) copyListRepPtr;
    copyPtr->typePtr = &tclListType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetListFromAny --
 *
 *	Attempt to generate a list internal form for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a list is stored as "objPtr"s internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetListFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *elemStart, *nextElem, *s;
    int lenRemain, length, estCount, elemSize, hasBrace, i, j, result;
    char *limit;		/* Points just after string's last byte. */
    register char *p;
    register Tcl_Obj **elemPtrs;
    register Tcl_Obj *elemPtr;
    List *listRepPtr;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = TclGetStringFromObj(objPtr, &length);

    /*
     * Parse the string into separate string objects, and create a List
     * structure that points to the element string objects. We use a
     * modified version of Tcl_SplitList's implementation to avoid one
     * malloc and a string copy for each list element. First, estimate the
     * number of elements by counting the number of space characters in the
     * list.
     */

    limit = (string + length);
    estCount = 1;
    for (p = string;  p < limit;  p++) {
	if (isspace(UCHAR(*p))) {
	    estCount++;
	}
    }

    /*
     * Allocate a new List structure with enough room for "estCount"
     * elements. Each element is a pointer to a Tcl_Obj with the appropriate
     * string rep. The initial "estCount" elements are set using the
     * corresponding "argv" strings.
     */

    elemPtrs = (Tcl_Obj **)
	    ckalloc((unsigned) (estCount * sizeof(Tcl_Obj *)));
    for (p = string, lenRemain = length, i = 0;
	    lenRemain > 0;
	    p = nextElem, lenRemain = (limit - nextElem), i++) {
	result = TclFindElement(interp, p, lenRemain, &elemStart, &nextElem,
				&elemSize, &hasBrace);
	if (result != TCL_OK) {
	    for (j = 0;  j < i;  j++) {
		elemPtr = elemPtrs[j];
		Tcl_DecrRefCount(elemPtr);
	    }
	    ckfree((char *) elemPtrs);
	    return result;
	}
	if (elemStart >= limit) {
	    break;
	}
	if (i > estCount) {
	    panic("SetListFromAny: bad size estimate for list");
	}

	/*
	 * Allocate a Tcl object for the element and initialize it from the
	 * "elemSize" bytes starting at "elemStart".
	 */

	s = ckalloc((unsigned) elemSize + 1);
	if (hasBrace) {
	    strncpy(s, elemStart, (size_t) elemSize);
	    s[elemSize] = 0;
	} else {
	    elemSize = TclCopyAndCollapse(elemSize, elemStart, s);
	}
	
	TclNewObj(elemPtr);
        elemPtr->bytes  = s;
        elemPtr->length = elemSize;
        elemPtrs[i] = elemPtr;
	Tcl_IncrRefCount(elemPtr); /* since list now holds ref to it */
    }

    listRepPtr = (List *) ckalloc(sizeof(List));
    listRepPtr->maxElemCount = estCount;
    listRepPtr->elemCount    = i;
    listRepPtr->elements     = elemPtrs;

    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = (VOID *) listRepPtr;
    objPtr->typePtr = &tclListType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfList --
 *
 *	Update the string representation for a list object.
 *	Note: This procedure does not invalidate an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the list-to-string conversion. This string will be empty if the
 *	list has no elements. The list internal representation
 *	should not be NULL and we assume it is not NULL.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfList(listPtr)
    Tcl_Obj *listPtr;		/* List object with string rep to update. */
{
#   define LOCAL_SIZE 20
    int localFlags[LOCAL_SIZE], *flagPtr;
    List *listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
    int numElems = listRepPtr->elemCount;
    register int i;
    char *elem, *dst;
    int length;

    /*
     * Convert each element of the list to string form and then convert it
     * to proper list element form, adding it to the result buffer.
     */

    /*
     * Pass 1: estimate space, gather flags.
     */

    if (numElems <= LOCAL_SIZE) {
	flagPtr = localFlags;
    } else {
	flagPtr = (int *) ckalloc((unsigned) numElems*sizeof(int));
    }
    listPtr->length = 1;
    for (i = 0; i < numElems; i++) {
	elem = Tcl_GetStringFromObj(listRepPtr->elements[i], &length);
	listPtr->length += Tcl_ScanCountedElement(elem, length,
		&flagPtr[i]) + 1;
    }

    /*
     * Pass 2: copy into string rep buffer.
     */

    listPtr->bytes = ckalloc((unsigned) listPtr->length);
    dst = listPtr->bytes;
    for (i = 0; i < numElems; i++) {
	elem = Tcl_GetStringFromObj(listRepPtr->elements[i], &length);
	dst += Tcl_ConvertCountedElement(elem, length, dst, flagPtr[i]);
	*dst = ' ';
	dst++;
    }
    if (flagPtr != localFlags) {
	ckfree((char *) flagPtr);
    }
    if (dst == listPtr->bytes) {
	*dst = 0;
    } else {
	dst--;
	*dst = 0;
    }
    listPtr->length = dst - listPtr->bytes;
}
