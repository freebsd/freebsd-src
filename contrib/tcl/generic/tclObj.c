/* 
 * tclObj.c --
 *
 *	This file contains Tcl object-related procedures that are used by
 * 	many Tcl commands.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclObj.c 1.44 97/06/20 15:19:32
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Table of all object types.
 */

static Tcl_HashTable typeTable;
static int typeTableInitialized = 0;    /* 0 means not yet initialized. */

/*
 * Head of the list of free Tcl_Objs we maintain.
 */

Tcl_Obj *tclFreeObjList = NULL;

/*
 * Pointer to a heap-allocated string of length zero that the Tcl core uses
 * as the value of an empty string representation for an object. This value
 * is shared by all new objects allocated by Tcl_NewObj.
 */

char *tclEmptyStringRep = NULL;

/*
 * Count of the number of Tcl objects every allocated (by Tcl_NewObj) and
 * freed (by TclFreeObj).
 */

#ifdef TCL_COMPILE_STATS
long tclObjsAlloced = 0;
long tclObjsFreed = 0;
#endif /* TCL_COMPILE_STATS */

/*
 * Prototypes for procedures defined later in this file:
 */

static void		DupBooleanInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		DupDoubleInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		DupIntInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		FinalizeTypeTable _ANSI_ARGS_((void));
static void		FinalizeFreeObjList _ANSI_ARGS_((void));
static void		InitTypeTable _ANSI_ARGS_((void));
static int		SetBooleanFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static int		SetDoubleFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static int		SetIntFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static void		UpdateStringOfBoolean _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		UpdateStringOfDouble _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		UpdateStringOfInt _ANSI_ARGS_((Tcl_Obj *objPtr));

/*
 * The structures below defines the Tcl object types defined in this file by
 * means of procedures that can be invoked by generic object code. See also
 * tclStringObj.c, tclListObj.c, tclByteCode.c for other type manager
 * implementations.
 */

Tcl_ObjType tclBooleanType = {
    "boolean",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,   /* freeIntRepProc */
    DupBooleanInternalRep,		/* dupIntRepProc */
    UpdateStringOfBoolean,		/* updateStringProc */
    SetBooleanFromAny			/* setFromAnyProc */
};

Tcl_ObjType tclDoubleType = {
    "double",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,   /* freeIntRepProc */
    DupDoubleInternalRep,		/* dupIntRepProc */
    UpdateStringOfDouble,		/* updateStringProc */
    SetDoubleFromAny			/* setFromAnyProc */
};

Tcl_ObjType tclIntType = {
    "int",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,   /* freeIntRepProc */
    DupIntInternalRep,		        /* dupIntRepProc */
    UpdateStringOfInt,			/* updateStringProc */
    SetIntFromAny			/* setFromAnyProc */
};

/*
 *--------------------------------------------------------------
 *
 * InitTypeTable --
 *
 *	This procedure is invoked to perform once-only initialization of
 *	the type table. It also registers the object types defined in 
 *	this file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the table of defined object types "typeTable" with
 *	builtin object types defined in this file. It also initializes the
 *	value of tclEmptyStringRep, which points to the heap-allocated
 *	string of length zero used as the string representation for
 *	newly-created objects.
 *
 *--------------------------------------------------------------
 */

static void
InitTypeTable()
{
    typeTableInitialized = 1;

    Tcl_InitHashTable(&typeTable, TCL_STRING_KEYS);
    Tcl_RegisterObjType(&tclBooleanType);
    Tcl_RegisterObjType(&tclDoubleType);
    Tcl_RegisterObjType(&tclIntType);
    Tcl_RegisterObjType(&tclStringType);
    Tcl_RegisterObjType(&tclListType);
    Tcl_RegisterObjType(&tclByteCodeType);

    tclEmptyStringRep = (char *) ckalloc((unsigned) 1);
    tclEmptyStringRep[0] = '\0';
}

/*
 *----------------------------------------------------------------------
 *
 * FinalizeTypeTable --
 *
 *	This procedure is called by Tcl_Finalize after all exit handlers
 *	have been run to free up storage associated with the table of Tcl
 *	object types.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes all entries in the hash table of object types, "typeTable".
 *	Then sets "typeTableInitialized" to 0 so that the Tcl type system
 *	will be properly reinitialized if Tcl is restarted. Also deallocates
 *	the storage for tclEmptyStringRep.
 *
 *----------------------------------------------------------------------
 */

static void
FinalizeTypeTable()
{
    if (typeTableInitialized) {
        Tcl_DeleteHashTable(&typeTable);
	ckfree(tclEmptyStringRep);
        typeTableInitialized = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FinalizeFreeObjList --
 *
 *	Resets the free object list so it can later be reinitialized.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resets the value of tclFreeObjList.
 *
 *----------------------------------------------------------------------
 */

static void
FinalizeFreeObjList()
{
    tclFreeObjList = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeCompExecEnv --
 *
 *	Clean up the compiler execution environment so it can later be
 *	properly reinitialized.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up the execution environment
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeCompExecEnv()
{
    FinalizeTypeTable();
    FinalizeFreeObjList();
    TclFinalizeExecEnv();
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_RegisterObjType --
 *
 *	This procedure is called to register a new Tcl object type
 *	in the table of all object types supported by Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The type is registered in the Tcl type table. If there was already
 *	a type with the same name as in typePtr, it is replaced with the
 *	new type.
 *
 *--------------------------------------------------------------
 */

void
Tcl_RegisterObjType(typePtr)
    Tcl_ObjType *typePtr;	/* Information about object type;
				 * storage must be statically
				 * allocated (must live forever). */
{
    register Tcl_HashEntry *hPtr;
    int new;

    if (!typeTableInitialized) {
	InitTypeTable();
    }

    /*
     * If there's already an object type with the given name, remove it.
     */

    hPtr = Tcl_FindHashEntry(&typeTable, typePtr->name);
    if (hPtr != (Tcl_HashEntry *) NULL) {
        Tcl_DeleteHashEntry(hPtr);
    }

    /*
     * Now insert the new object type.
     */

    hPtr = Tcl_CreateHashEntry(&typeTable, typePtr->name, &new);
    if (new) {
	Tcl_SetHashValue(hPtr, typePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendAllObjTypes --
 *
 *	This procedure appends onto the argument object the name of each
 *	object type as a list element. This includes the builtin object
 *	types (e.g. int, list) as well as those added using
 *	Tcl_CreateObjType. These names can be used, for example, with
 *	Tcl_GetObjType to get pointers to the corresponding Tcl_ObjType
 *	structures.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case the object
 *	referenced by objPtr has each type name appended to it. If an
 *	error occurs, TCL_ERROR is returned and the interpreter's result
 *	holds an error message.
 *
 * Side effects:
 *	If necessary, the object referenced by objPtr is converted into
 *	a list object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppendAllObjTypes(interp, objPtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting. */
    Tcl_Obj *objPtr;		/* Points to the Tcl object onto which the
				 * name of each registered type is appended
				 * as a list element. */
{
    register Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_ObjType *typePtr;
    int result;
 
    if (!typeTableInitialized) {
	InitTypeTable();
    }

    /*
     * This code assumes that types names do not contain embedded NULLs.
     */

    for (hPtr = Tcl_FirstHashEntry(&typeTable, &search);
	    hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
        typePtr = (Tcl_ObjType *) Tcl_GetHashValue(hPtr);
	result = Tcl_ListObjAppendElement(interp, objPtr,
	        Tcl_NewStringObj(typePtr->name, -1));
	if (result == TCL_ERROR) {
	    return result;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetObjType --
 *
 *	This procedure looks up an object type by name.
 *
 * Results:
 *	If an object type with name matching "typeName" is found, a pointer
 *	to its Tcl_ObjType structure is returned; otherwise, NULL is
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_ObjType *
Tcl_GetObjType(typeName)
    char *typeName;		/* Name of Tcl object type to look up. */
{
    register Tcl_HashEntry *hPtr;
    Tcl_ObjType *typePtr;

    if (!typeTableInitialized) {
	InitTypeTable();
    }

    hPtr = Tcl_FindHashEntry(&typeTable, typeName);
    if (hPtr != (Tcl_HashEntry *) NULL) {
        typePtr = (Tcl_ObjType *) Tcl_GetHashValue(hPtr);
	return typePtr;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConvertToType --
 *
 *	Convert the Tcl object "objPtr" to have type "typePtr" if possible.
 *
 * Results:
 *	The return value is TCL_OK on success and TCL_ERROR on failure. If
 *	TCL_ERROR is returned, then the interpreter's result contains an
 *	error message unless "interp" is NULL. Passing a NULL "interp"
 *	allows this procedure to be used as a test whether the conversion
 *	could be done (and in fact was done).
 *
 * Side effects:
 *	Any internal representation for the old type is freed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ConvertToType(interp, objPtr, typePtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
    Tcl_ObjType *typePtr;	/* The target type. */
{
    if (objPtr->typePtr == typePtr) {
	return TCL_OK;
    }

    /*
     * Use the target type's Tcl_SetFromAnyProc to set "objPtr"s internal
     * form as appropriate for the target type. This frees the old internal
     * representation.
     */

    return typePtr->setFromAnyProc(interp, objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewObj --
 *
 *	This procedure is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates new Tcl objects that denote
 *	the empty string. These objects have a NULL object type and NULL
 *	string representation byte pointer. Type managers call this routine
 *	to allocate new objects that they further initialize.
 *
 *	When TCL_MEM_DEBUG is defined, this procedure just returns the
 *	result of calling the debugging version Tcl_DbNewObj.
 *
 * Results:
 *	The result is a newly allocated object that represents the empty
 *	string. The new object's typePtr is set NULL and its ref count
 *	is set to 0.
 *
 * Side effects:
 *	If compiling with TCL_COMPILE_STATS, this procedure increments
 *	the global count of allocated objects (tclObjsAlloced).
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewObj

Tcl_Obj *
Tcl_NewObj()
{
    return Tcl_DbNewObj("unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewObj()
{
    register Tcl_Obj *objPtr;

    /*
     * Allocate the object using the list of free Tcl_Objs we maintain.
     */

    if (tclFreeObjList == NULL) {
	TclAllocateFreeObjects();
    }
    objPtr = tclFreeObjList;
    tclFreeObjList = (Tcl_Obj *) tclFreeObjList->internalRep.otherValuePtr;
    
    objPtr->refCount = 0;
    objPtr->bytes    = tclEmptyStringRep;
    objPtr->length   = 0;
    objPtr->typePtr  = NULL;
#ifdef TCL_COMPILE_STATS
    tclObjsAlloced++;
#endif /* TCL_COMPILE_STATS */
    return objPtr;
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewObj --
 *
 *	This procedure is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new Tcl objects that denote the
 *	empty string. It is the same as the Tcl_NewObj procedure above
 *	except that it calls Tcl_DbCkalloc directly with the file name and
 *	line number from its caller. This simplifies debugging since then
 *	the checkmem command will report the correct file name and line
 *	number when reporting objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this procedure just returns the
 *	result of calling Tcl_NewObj.
 *
 * Results:
 *	The result is a newly allocated that represents the empty string.
 *	The new object's typePtr is set NULL and its ref count is set to 0.
 *
 * Side effects:
 *	If compiling with TCL_COMPILE_STATS, this procedure increments
 *	the global count of allocated objects (tclObjsAlloced).
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewObj(file, line)
    register char *file;	/* The name of the source file calling this
				 * procedure; used for debugging. */
    register int line;		/* Line number in the source file; used
				 * for debugging. */
{
    register Tcl_Obj *objPtr;

    /*
     * If debugging Tcl's memory usage, allocate the object using ckalloc.
     * Otherwise, allocate it using the list of free Tcl_Objs we maintain.
     */

    objPtr = (Tcl_Obj *) Tcl_DbCkalloc(sizeof(Tcl_Obj), file, line);
    objPtr->refCount = 0;
    objPtr->bytes    = tclEmptyStringRep;
    objPtr->length   = 0;
    objPtr->typePtr  = NULL;
#ifdef TCL_COMPILE_STATS
    tclObjsAlloced++;
#endif /* TCL_COMPILE_STATS */
    return objPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewObj(file, line)
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    return Tcl_NewObj();
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * TclAllocateFreeObjects --
 *
 *	Procedure to allocate a number of free Tcl_Objs. This is done using
 *	a single ckalloc to reduce the overhead for Tcl_Obj allocation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	tclFreeObjList, the head of the list of free Tcl_Objs, is set to the
 *	first of a number of free Tcl_Obj's linked together by their
 *	internalRep.otherValuePtrs.
 *
 *----------------------------------------------------------------------
 */

#define OBJS_TO_ALLOC_EACH_TIME 100

void
TclAllocateFreeObjects()
{
    Tcl_Obj tmp[2];
    size_t objSizePlusPadding =	/* NB: this assumes byte addressing. */
	((int)(&(tmp[1])) - (int)(&(tmp[0])));
    size_t bytesToAlloc = (OBJS_TO_ALLOC_EACH_TIME * objSizePlusPadding);
    char *basePtr;
    register Tcl_Obj *prevPtr, *objPtr;
    register int i;

    basePtr = (char *) ckalloc(bytesToAlloc);
    memset(basePtr, 0, bytesToAlloc);

    prevPtr = NULL;
    objPtr = (Tcl_Obj *) basePtr;
    for (i = 0;  i < OBJS_TO_ALLOC_EACH_TIME;  i++) {
	objPtr->internalRep.otherValuePtr = (VOID *) prevPtr;
	prevPtr = objPtr;
	objPtr = (Tcl_Obj *) (((char *)objPtr) + objSizePlusPadding);
    }
    tclFreeObjList = prevPtr;
}
#undef OBJS_TO_ALLOC_EACH_TIME

/*
 *----------------------------------------------------------------------
 *
 * TclFreeObj --
 *
 *	This procedure frees the memory associated with the argument
 *	object. It is called by the tcl.h macro Tcl_DecrRefCount when an
 *	object's ref count is zero. It is only "public" since it must
 *	be callable by that macro wherever the macro is used. It should not
 *	be directly called by clients.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates the storage for the object's Tcl_Obj structure
 *	after deallocating the string representation and calling the
 *	type-specific Tcl_FreeInternalRepProc to deallocate the object's
 *	internal representation. If compiling with TCL_COMPILE_STATS,
 *	this procedure increments the global count of freed objects
 *	(tclObjsFreed).
 *
 *----------------------------------------------------------------------
 */

void
TclFreeObj(objPtr)
    register Tcl_Obj *objPtr;	/* The object to be freed. */
{
    register Tcl_ObjType *typePtr = objPtr->typePtr;
    
#ifdef TCL_MEM_DEBUG
    if ((objPtr)->refCount < -1) {
	panic("Reference count for %lx was negative", objPtr);
    }
#endif /* TCL_MEM_DEBUG */

    Tcl_InvalidateStringRep(objPtr);
    if ((typePtr != NULL) && (typePtr->freeIntRepProc != NULL)) {
	typePtr->freeIntRepProc(objPtr);
    }

    /*
     * If debugging Tcl's memory usage, deallocate the object using ckfree.
     * Otherwise, deallocate it by adding it onto the list of free
     * Tcl_Objs we maintain.
     */
    
#ifdef TCL_MEM_DEBUG
    ckfree((char *) objPtr);
#else
    objPtr->internalRep.otherValuePtr = (VOID *) tclFreeObjList;
    tclFreeObjList = objPtr;
#endif /* TCL_MEM_DEBUG */

#ifdef TCL_COMPILE_STATS    
    tclObjsFreed++;
#endif /* TCL_COMPILE_STATS */    
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DuplicateObj --
 *
 *	Create and return a new object that is a duplicate of the argument
 *	object.
 *
 * Results:
 *	The return value is a pointer to a newly created Tcl_Obj. This
 *	object has reference count 0 and the same type, if any, as the
 *	source object objPtr. Also:
 *	  1) If the source object has a valid string rep, we copy it;
 *	     otherwise, the duplicate's string rep is set NULL to mark
 *	     it invalid.
 *	  2) If the source object has an internal representation (i.e. its
 *	     typePtr is non-NULL), the new object's internal rep is set to
 *	     a copy; otherwise the new internal rep is marked invalid.
 *
 * Side effects:
 *      What constitutes "copying" the internal representation depends on
 *	the type. For example, if the argument object is a list,
 *	the element objects it points to will not actually be copied but
 *	will be shared with the duplicate list. That is, the ref counts of
 *	the element objects will be incremented.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_DuplicateObj(objPtr)
    register Tcl_Obj *objPtr;		/* The object to duplicate. */
{
    register Tcl_ObjType *typePtr = objPtr->typePtr;
    register Tcl_Obj *dupPtr;

    TclNewObj(dupPtr);

    if (objPtr->bytes == NULL) {
	dupPtr->bytes = NULL;
    } else if (objPtr->bytes != tclEmptyStringRep) {
	int len = objPtr->length;
	
	dupPtr->bytes = (char *) ckalloc((unsigned) len+1);
	if (len > 0) {
	    memcpy((VOID *) dupPtr->bytes, (VOID *) objPtr->bytes,
		   (unsigned) len);
	}
	dupPtr->bytes[len] = '\0';
	dupPtr->length = len;
    }
    
    if (typePtr != NULL) {
	typePtr->dupIntRepProc(objPtr, dupPtr);
    }
    return dupPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetStringFromObj --
 *
 *	Returns the string representation's byte array pointer and length
 *	for an object.
 *
 * Results:
 *	Returns a pointer to the string representation of objPtr. If
 *	lengthPtr isn't NULL, the length of the string representation is
 *	stored at *lengthPtr. The byte array referenced by the returned
 *	pointer must not be modified by the caller. Furthermore, the
 *	caller must copy the bytes if they need to retain them since the
 *	object's string rep can change as a result of other operations.
 *
 * Side effects:
 *	May call the object's updateStringProc to update the string
 *	representation from the internal representation.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetStringFromObj(objPtr, lengthPtr)
    register Tcl_Obj *objPtr;	/* Object whose string rep byte pointer
				 * should be returned. */
    register int *lengthPtr;	/* If non-NULL, the location where the
				 * string rep's byte array length should be
				 * stored. If NULL, no length is stored. */
{
    if (objPtr->bytes != NULL) {
	if (lengthPtr != NULL) {
	    *lengthPtr = objPtr->length;
	}
	return objPtr->bytes;
    }

    objPtr->typePtr->updateStringProc(objPtr);
    if (lengthPtr != NULL) {
	*lengthPtr = objPtr->length;
    }
    return objPtr->bytes;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InvalidateStringRep --
 *
 *	This procedure is called to invalidate an object's string
 *	representation. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates the storage for any old string representation, then
 *	sets the string representation NULL to mark it invalid.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_InvalidateStringRep(objPtr)
     register Tcl_Obj *objPtr;	/* Object whose string rep byte pointer
				 * should be freed. */
{
    if (objPtr->bytes != NULL) {
	if (objPtr->bytes != tclEmptyStringRep) {
	    ckfree((char *) objPtr->bytes);
	}
	objPtr->bytes = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewBooleanObj --
 *
 *	This procedure is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new boolean object and
 *	initializes it from the argument boolean value. A nonzero
 *	"boolValue" is coerced to 1.
 *
 *	When TCL_MEM_DEBUG is defined, this procedure just returns the
 *	result of calling the debugging version Tcl_DbNewBooleanObj.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewBooleanObj

Tcl_Obj *
Tcl_NewBooleanObj(boolValue)
    register int boolValue;	/* Boolean used to initialize new object. */
{
    return Tcl_DbNewBooleanObj(boolValue, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewBooleanObj(boolValue)
    register int boolValue;	/* Boolean used to initialize new object. */
{
    register Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.longValue = (boolValue? 1 : 0);
    objPtr->typePtr = &tclBooleanType;
    return objPtr;
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewBooleanObj --
 *
 *	This procedure is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new boolean objects. It is the
 *	same as the Tcl_NewBooleanObj procedure above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the checkmem command
 *	will report the correct file name and line number when reporting
 *	objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this procedure just returns the
 *	result of calling Tcl_NewBooleanObj.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewBooleanObj(boolValue, file, line)
    register int boolValue;	/* Boolean used to initialize new object. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    register Tcl_Obj *objPtr;

    TclDbNewObj(objPtr, file, line);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.longValue = (boolValue? 1 : 0);
    objPtr->typePtr = &tclBooleanType;
    return objPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewBooleanObj(boolValue, file, line)
    register int boolValue;	/* Boolean used to initialize new object. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    return Tcl_NewBooleanObj(boolValue);
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetBooleanObj --
 *
 *	Modify an object to be a boolean object and to have the specified
 *	boolean value. A nonzero "boolValue" is coerced to 1.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetBooleanObj(objPtr, boolValue)
    register Tcl_Obj *objPtr;	/* Object whose internal rep to init. */
    register int boolValue;	/* Boolean used to set object's value. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetBooleanObj called with shared object");
    }
    
    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = (boolValue? 1 : 0);
    objPtr->typePtr = &tclBooleanType;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetBooleanFromObj --
 *
 *	Attempt to return a boolean from the Tcl object "objPtr". If the
 *	object is not already a boolean, an attempt will be made to convert
 *	it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already a boolean, the conversion will free
 *	any old internal representation. 
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetBooleanFromObj(interp, objPtr, boolPtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object from which to get boolean. */
    register int *boolPtr;	/* Place to store resulting boolean. */
{
    register int result;

    result = SetBooleanFromAny(interp, objPtr);
    if (result == TCL_OK) {
	*boolPtr = (int) objPtr->internalRep.longValue;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DupBooleanInternalRep --
 *
 *	Initialize the internal representation of a boolean Tcl_Obj to a
 *	copy of the internal representation of an existing boolean object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to the boolean (an integer)
 *	corresponding to "srcPtr"s internal rep.
 *
 *----------------------------------------------------------------------
 */

static void
DupBooleanInternalRep(srcPtr, copyPtr)
    register Tcl_Obj *srcPtr;	/* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr;	/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
    copyPtr->typePtr = &tclBooleanType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetBooleanFromAny --
 *
 *	Attempt to generate a boolean internal form for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an integer 1 or 0 is stored as "objPtr"s
 *	internal representation and the type of "objPtr" is set to boolean.
 *
 *----------------------------------------------------------------------
 */

static int
SetBooleanFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *end;
    register char c;
    char lowerCase[10];
    int newBool, length;
    register int i;
    double dbl;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = TclGetStringFromObj(objPtr, &length);

    /*
     * Copy the string converting its characters to lower case.
     */

    for (i = 0;  (i < 9) && (i < length);  i++) {
	c = string[i];
	if (isupper(UCHAR(c))) {
	    c = (char) tolower(UCHAR(c));
	}
	lowerCase[i] = c;
    }
    lowerCase[i] = 0;

    /*
     * Parse the string as a boolean. We use an implementation here that
     * doesn't report errors in interp if interp is NULL.
     */

    c = lowerCase[0];
    if ((c == '0') && (lowerCase[1] == '\0')) {
	newBool = 0;
    } else if ((c == '1') && (lowerCase[1] == '\0')) {
	newBool = 1;
    } else if ((c == 'y') && (strncmp(lowerCase, "yes", (size_t) length) == 0)) {
	newBool = 1;
    } else if ((c == 'n') && (strncmp(lowerCase, "no", (size_t) length) == 0)) {
	newBool = 0;
    } else if ((c == 't') && (strncmp(lowerCase, "true", (size_t) length) == 0)) {
	newBool = 1;
    } else if ((c == 'f') && (strncmp(lowerCase, "false", (size_t) length) == 0)) {
	newBool = 0;
    } else if ((c == 'o') && (length >= 2)) {
	if (strncmp(lowerCase, "on", (size_t) length) == 0) {
	    newBool = 1;
	} else if (strncmp(lowerCase, "off", (size_t) length) == 0) {
	    newBool = 0;
	} else {
	    goto badBoolean;
	}
    } else {
        /*
         * Still might be a string containing the characters representing an
         * int or double that wasn't handled above. This would be a string
         * like "27" or "1.0" that is non-zero and not "1". Such a string
         * whould result in the boolean value true. We try converting to
         * double. If that succeeds and the resulting double is non-zero, we
         * have a "true". Note that numbers can't have embedded NULLs.
	 */

	dbl = strtod(string, &end);
	if (end == string) {
	    goto badBoolean;
	}

	/*
	 * Make sure the string has no garbage after the end of the double.
	 */
	
	while ((end < (string+length)) && isspace(UCHAR(*end))) {
	    end++;
	}
	if (end != (string+length)) {
	    goto badBoolean;
	}
	newBool = (dbl != 0.0);
    }

    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    if ((oldTypePtr != NULL) &&	(oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.longValue = newBool;
    objPtr->typePtr = &tclBooleanType;
    return TCL_OK;

    badBoolean:
    if (interp != NULL) {
	/*
	 * Must copy string before resetting the result in case a caller
	 * is trying to convert the interpreter's result to a boolean.
	 */
	
	char buf[100];
	sprintf(buf, "expected boolean value but got \"%.50s\"", string);
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfBoolean --
 *
 *	Update the string representation for a boolean object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the boolean-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfBoolean(objPtr)
    register Tcl_Obj *objPtr;	/* Int object whose string rep to update. */
{
    char *s = ckalloc((unsigned) 2);
    
    s[0] = (char) (objPtr->internalRep.longValue? '1' : '0');
    s[1] = '\0';
    objPtr->bytes = s;
    objPtr->length = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewDoubleObj --
 *
 *	This procedure is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new double object and
 *	initializes it from the argument double value.
 *
 *	When TCL_MEM_DEBUG is defined, this procedure just returns the
 *	result of calling the debugging version Tcl_DbNewDoubleObj.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewDoubleObj

Tcl_Obj *
Tcl_NewDoubleObj(dblValue)
    register double dblValue;	/* Double used to initialize the object. */
{
    return Tcl_DbNewDoubleObj(dblValue, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewDoubleObj(dblValue)
    register double dblValue;	/* Double used to initialize the object. */
{
    register Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.doubleValue = dblValue;
    objPtr->typePtr = &tclDoubleType;
    return objPtr;
}
#endif /* if TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewDoubleObj --
 *
 *	This procedure is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new double objects. It is the
 *	same as the Tcl_NewDoubleObj procedure above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the checkmem command
 *	will report the correct file name and line number when reporting
 *	objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this procedure just returns the
 *	result of calling Tcl_NewDoubleObj.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewDoubleObj(dblValue, file, line)
    register double dblValue;	/* Double used to initialize the object. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    register Tcl_Obj *objPtr;

    TclDbNewObj(objPtr, file, line);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.doubleValue = dblValue;
    objPtr->typePtr = &tclDoubleType;
    return objPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewDoubleObj(dblValue, file, line)
    register double dblValue;	/* Double used to initialize the object. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    return Tcl_NewDoubleObj(dblValue);
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetDoubleObj --
 *
 *	Modify an object to be a double object and to have the specified
 *	double value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetDoubleObj(objPtr, dblValue)
    register Tcl_Obj *objPtr;	/* Object whose internal rep to init. */
    register double dblValue;	/* Double used to set the object's value. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetDoubleObj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.doubleValue = dblValue;
    objPtr->typePtr = &tclDoubleType;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetDoubleFromObj --
 *
 *	Attempt to return a double from the Tcl object "objPtr". If the
 *	object is not already a double, an attempt will be made to convert
 *	it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already a double, the conversion will free
 *	any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetDoubleFromObj(interp, objPtr, dblPtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object from which to get a double. */
    register double *dblPtr;	/* Place to store resulting double. */
{
    register int result;
    
    if (objPtr->typePtr == &tclDoubleType) {
	*dblPtr = objPtr->internalRep.doubleValue;
	return TCL_OK;
    }

    result = SetDoubleFromAny(interp, objPtr);
    if (result == TCL_OK) {
	*dblPtr = objPtr->internalRep.doubleValue;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DupDoubleInternalRep --
 *
 *	Initialize the internal representation of a double Tcl_Obj to a
 *	copy of the internal representation of an existing double object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to the double precision floating 
 *	point number corresponding to "srcPtr"s internal rep.
 *
 *----------------------------------------------------------------------
 */

static void
DupDoubleInternalRep(srcPtr, copyPtr)
    register Tcl_Obj *srcPtr;	/* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr;	/* Object with internal rep to set. */
{
    copyPtr->internalRep.doubleValue = srcPtr->internalRep.doubleValue;
    copyPtr->typePtr = &tclDoubleType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetDoubleFromAny --
 *
 *	Attempt to generate an double-precision floating point internal form
 *	for the Tcl object "objPtr".
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a double is stored as "objPtr"s internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetDoubleFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *end;
    double newDouble;
    int length;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = TclGetStringFromObj(objPtr, &length);

    /*
     * Now parse "objPtr"s string as an double. Numbers can't have embedded
     * NULLs. We use an implementation here that doesn't report errors in
     * interp if interp is NULL.
     */

    errno = 0;
    newDouble = strtod(string, &end);
    if (end == string) {
	badDouble:
	if (interp != NULL) {
	    /*
	     * Must copy string before resetting the result in case a caller
	     * is trying to convert the interpreter's result to an int.
	     */
	    
	    char buf[100];
	    sprintf(buf, "expected floating-point number but got \"%.50s\"",
	            string);
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	}
	return TCL_ERROR;
    }
    if (errno != 0) {
	if (interp != NULL) {
	    TclExprFloatError(interp, newDouble);
	}
	return TCL_ERROR;
    }

    /*
     * Make sure that the string has no garbage after the end of the double.
     */
    
    while ((end < (string+length)) && isspace(UCHAR(*end))) {
	end++;
    }
    if (end != (string+length)) {
	goto badDouble;
    }
    
    /*
     * The conversion to double succeeded. Free the old internalRep before
     * setting the new one. We do this as late as possible to allow the
     * conversion code, in particular Tcl_GetStringFromObj, to use that old
     * internalRep.
     */
    
    if ((oldTypePtr != NULL) &&	(oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.doubleValue = newDouble;
    objPtr->typePtr = &tclDoubleType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfDouble --
 *
 *	Update the string representation for a double-precision floating
 *	point object. This must obey the current tcl_precision value for
 *	double-to-string conversions. Note: This procedure does not free an
 *	existing old string rep so storage will be lost if this has not
 *	already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the double-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfDouble(objPtr)
    register Tcl_Obj *objPtr;	/* Double obj with string rep to update. */
{
    char buffer[TCL_DOUBLE_SPACE];
    register int len;
    
    Tcl_PrintDouble((Tcl_Interp *) NULL, objPtr->internalRep.doubleValue,
	    buffer);
    len = strlen(buffer);
    
    objPtr->bytes = (char *) ckalloc((unsigned) len + 1);
    strcpy(objPtr->bytes, buffer);
    objPtr->length = len;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewIntObj --
 *
 *	If a client is compiled with TCL_MEM_DEBUG defined, calls to
 *	Tcl_NewIntObj to create a new integer object end up calling the
 *	debugging procedure Tcl_DbNewLongObj instead.
 *
 *	Otherwise, if the client is compiled without TCL_MEM_DEBUG defined,
 *	calls to Tcl_NewIntObj result in a call to one of the two
 *	Tcl_NewIntObj implementations below. We provide two implementations
 *	so that the Tcl core can be compiled to do memory debugging of the 
 *	core even if a client does not request it for itself.
 *
 *	Integer and long integer objects share the same "integer" type
 *	implementation. We store all integers as longs and Tcl_GetIntFromObj
 *	checks whether the current value of the long can be represented by
 *	an int.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewIntObj

Tcl_Obj *
Tcl_NewIntObj(intValue)
    register int intValue;	/* Int used to initialize the new object. */
{
    return Tcl_DbNewLongObj((long)intValue, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewIntObj(intValue)
    register int intValue;	/* Int used to initialize the new object. */
{
    register Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.longValue = (long)intValue;
    objPtr->typePtr = &tclIntType;
    return objPtr;
}
#endif /* if TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetIntObj --
 *
 *	Modify an object to be an integer and to have the specified integer
 *	value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed. 
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetIntObj(objPtr, intValue)
    register Tcl_Obj *objPtr;	/* Object whose internal rep to init. */
    register int intValue;	/* Integer used to set object's value. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetIntObj called with shared object");
    }
    
    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = (long) intValue;
    objPtr->typePtr = &tclIntType;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetIntFromObj --
 *
 *	Attempt to return an int from the Tcl object "objPtr". If the object
 *	is not already an int, an attempt will be made to convert it to one.
 *
 *	Integer and long integer objects share the same "integer" type
 *	implementation. We store all integers as longs and Tcl_GetIntFromObj
 *	checks whether the current value of the long can be represented by
 *	an int.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion or if the long integer held by the object
 *	can not be represented by an int, an error message is left in
 *	the interpreter's result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already an int, the conversion will free
 *	any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetIntFromObj(interp, objPtr, intPtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object from which to get a int. */
    register int *intPtr;	/* Place to store resulting int. */
{
    register long l;
    int result;
    
    if (objPtr->typePtr != &tclIntType) {
	result = SetIntFromAny(interp, objPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    l = objPtr->internalRep.longValue;
    if (((long)((int)l)) == l) {
	*intPtr = (int)objPtr->internalRep.longValue;
	return TCL_OK;
    }
    if (interp != NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
		"integer value too large to represent as non-long integer", -1);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DupIntInternalRep --
 *
 *	Initialize the internal representation of an int Tcl_Obj to a
 *	copy of the internal representation of an existing int object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to the integer corresponding to
 *	"srcPtr"s internal rep.
 *
 *----------------------------------------------------------------------
 */

static void
DupIntInternalRep(srcPtr, copyPtr)
    register Tcl_Obj *srcPtr;	/* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr;	/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
    copyPtr->typePtr = &tclIntType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetIntFromAny --
 *
 *	Attempt to generate an integer internal form for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is a standard object Tcl result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an int is stored as "objPtr"s internal
 *	representation. 
 *
 *----------------------------------------------------------------------
 */

static int
SetIntFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *end;
    int length;
    register char *p;
    long newLong;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = TclGetStringFromObj(objPtr, &length);

    /*
     * Now parse "objPtr"s string as an int. We use an implementation here
     * that doesn't report errors in interp if interp is NULL. Note: use
     * strtoul instead of strtol for integer conversions to allow full-size
     * unsigned numbers, but don't depend on strtoul to handle sign
     * characters; it won't in some implementations.
     */

    errno = 0;
    for (p = string;  isspace(UCHAR(*p));  p++) {
	/* Empty loop body. */
    }
    if (*p == '-') {
	p++;
	newLong = -((long)strtoul(p, &end, 0));
    } else if (*p == '+') {
	p++;
	newLong = strtoul(p, &end, 0);
    } else {
	newLong = strtoul(p, &end, 0);
    }
    if (end == p) {
	badInteger:
	if (interp != NULL) {
	    /*
	     * Must copy string before resetting the result in case a caller
	     * is trying to convert the interpreter's result to an int.
	     */
	    
	    char buf[100];
	    sprintf(buf, "expected integer but got \"%.50s\"", string);
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	}
	return TCL_ERROR;
    }
    if (errno == ERANGE) {
	if (interp != NULL) {
	    char *s = "integer value too large to represent";
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
	    Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW", s, (char *) NULL);
	}
	return TCL_ERROR;
    }

    /*
     * Make sure that the string has no garbage after the end of the int.
     */
    
    while ((end < (string+length)) && isspace(UCHAR(*end))) {
	end++;
    }
    if (end != (string+length)) {
	goto badInteger;
    }

    /*
     * The conversion to int succeeded. Free the old internalRep before
     * setting the new one. We do this as late as possible to allow the
     * conversion code, in particular Tcl_GetStringFromObj, to use that old
     * internalRep.
     */

    if ((oldTypePtr != NULL) &&	(oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = newLong;
    objPtr->typePtr = &tclIntType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfInt --
 *
 *	Update the string representation for an integer object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the int-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfInt(objPtr)
    register Tcl_Obj *objPtr;	/* Int object whose string rep to update. */
{
    char buffer[TCL_DOUBLE_SPACE];
    register int len;
    
    len = TclFormatInt(buffer, objPtr->internalRep.longValue);
    
    objPtr->bytes = ckalloc((unsigned) len + 1);
    strcpy(objPtr->bytes, buffer);
    objPtr->length = len;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewLongObj --
 *
 *	If a client is compiled with TCL_MEM_DEBUG defined, calls to
 *	Tcl_NewLongObj to create a new long integer object end up calling
 *	the debugging procedure Tcl_DbNewLongObj instead.
 *
 *	Otherwise, if the client is compiled without TCL_MEM_DEBUG defined,
 *	calls to Tcl_NewLongObj result in a call to one of the two
 *	Tcl_NewLongObj implementations below. We provide two implementations
 *	so that the Tcl core can be compiled to do memory debugging of the 
 *	core even if a client does not request it for itself.
 *
 *	Integer and long integer objects share the same "integer" type
 *	implementation. We store all integers as longs and Tcl_GetIntFromObj
 *	checks whether the current value of the long can be represented by
 *	an int.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
#undef Tcl_NewLongObj

Tcl_Obj *
Tcl_NewLongObj(longValue)
    register long longValue;	/* Long integer used to initialize the
				 * new object. */
{
    return Tcl_DbNewLongObj(longValue, "unknown", 0);
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_NewLongObj(longValue)
    register long longValue;	/* Long integer used to initialize the
				 * new object. */
{
    register Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.longValue = longValue;
    objPtr->typePtr = &tclIntType;
    return objPtr;
}
#endif /* if TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewLongObj --
 *
 *	If a client is compiled with TCL_MEM_DEBUG defined, calls to
 *	Tcl_NewIntObj and Tcl_NewLongObj to create new integer or
 *	long integer objects end up calling the debugging procedure
 *	Tcl_DbNewLongObj instead. We provide two implementations of
 *	Tcl_DbNewLongObj so that whether the Tcl core is compiled to do
 *	memory debugging of the core is independent of whether a client
 *	requests debugging for itself.
 *
 *	When the core is compiled with TCL_MEM_DEBUG defined,
 *	Tcl_DbNewLongObj calls Tcl_DbCkalloc directly with the file name and
 *	line number from its caller. This simplifies debugging since then
 *	the checkmem command will report the caller's file name and line
 *	number when reporting objects that haven't been freed.
 *
 *	Otherwise, when the core is compiled without TCL_MEM_DEBUG defined,
 *	this procedure just returns the result of calling Tcl_NewLongObj.
 *
 * Results:
 *	The newly created long integer object is returned. This object
 *	will have an invalid string representation. The returned object has
 *	ref count 0.
 *
 * Side effects:
 *	Allocates memory.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG

Tcl_Obj *
Tcl_DbNewLongObj(longValue, file, line)
    register long longValue;	/* Long integer used to initialize the
				 * new object. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    register Tcl_Obj *objPtr;

    TclDbNewObj(objPtr, file, line);
    objPtr->bytes = NULL;
    
    objPtr->internalRep.longValue = longValue;
    objPtr->typePtr = &tclIntType;
    return objPtr;
}

#else /* if not TCL_MEM_DEBUG */

Tcl_Obj *
Tcl_DbNewLongObj(longValue, file, line)
    register long longValue;	/* Long integer used to initialize the
				 * new object. */
    char *file;			/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line;			/* Line number in the source file; used
				 * for debugging. */
{
    return Tcl_NewLongObj(longValue);
}
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetLongObj --
 *
 *	Modify an object to be an integer object and to have the specified
 *	long integer value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed. 
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetLongObj(objPtr, longValue)
    register Tcl_Obj *objPtr;	/* Object whose internal rep to init. */
    register long longValue;	/* Long integer used to initialize the
				 * object's value. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	panic("Tcl_SetLongObj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = longValue;
    objPtr->typePtr = &tclIntType;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetLongFromObj --
 *
 *	Attempt to return an long integer from the Tcl object "objPtr". If
 *	the object is not already an int object, an attempt will be made to
 *	convert it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already an int object, the conversion will free
 *	any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetLongFromObj(interp, objPtr, longPtr)
    Tcl_Interp *interp; 	/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object from which to get a long. */
    register long *longPtr;	/* Place to store resulting long. */
{
    register int result;
    
    if (objPtr->typePtr == &tclIntType) {
	*longPtr = objPtr->internalRep.longValue;
	return TCL_OK;
    }
    result = SetIntFromAny(interp, objPtr);
    if (result == TCL_OK) {
	*longPtr = objPtr->internalRep.longValue;
    }
    return result;
}
