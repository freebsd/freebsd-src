/*
 * tclNamesp.c --
 *
 *      Contains support for namespaces, which provide a separate context of
 *      commands and global variables. The global :: namespace is the
 *      traditional Tcl "global" scope. Other namespaces are created as
 *      children of the global namespace. These other namespaces contain
 *      special-purpose commands and variables for packages.
 *
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * Originally implemented by
 *   Michael J. McLennan
 *   Bell Labs Innovations for Lucent Technologies
 *   mmclennan@lucent.com
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclNamesp.c 1.21 97/06/20 15:21:04
 */

#include "tclInt.h"

/*
 * Flag passed to TclGetNamespaceForQualName to indicate that it should
 * search for a namespace rather than a command or variable inside a
 * namespace. Note that this flag's value must not conflict with the values
 * of TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY, or CREATE_NS_IF_UNKNOWN.
 */

#define FIND_ONLY_NS	0x1000

/*
 * Count of the number of namespaces created. This value is used as a
 * unique id for each namespace.
 */

static long numNsCreated = 0; 

/*
 * Data structure used as the ClientData of imported commands: commands
 * created in an namespace when it imports a "real" command from another
 * namespace.
 */

typedef struct ImportedCmdData {
    Command *realCmdPtr;	/* "Real" command that this imported command
                                 * refers to. */
    Command *selfPtr;		/* Pointer to this imported command. Needed
				 * only when deleting it in order to remove
				 * it from the real command's linked list of
				 * imported commands that refer to it. */
} ImportedCmdData;

/*
 * This structure contains a cached pointer to a namespace that is the
 * result of resolving the namespace's name in some other namespace. It is
 * the internal representation for a nsName object. It contains the
 * pointer along with some information that is used to check the cached
 * pointer's validity.
 */

typedef struct ResolvedNsName {
    Namespace *nsPtr;		/* A cached namespace pointer. */
    long nsId;			/* nsPtr's unique namespace id. Used to
				 * verify that nsPtr is still valid
				 * (e.g., it's possible that the namespace
				 * was deleted and a new one created at
				 * the same address). */
    Namespace *refNsPtr;	/* Points to the namespace containing the
				 * reference (not the namespace that
				 * contains the referenced namespace). */
    int refCount;		/* Reference count: 1 for each nsName
				 * object that has a pointer to this
				 * ResolvedNsName structure as its internal
				 * rep. This structure can be freed when
				 * refCount becomes zero. */
} ResolvedNsName;

/*
 * Declarations for procedures local to this file:
 */

static void		DeleteImportedCmd _ANSI_ARGS_((
			    ClientData clientData));
static void		DupNsNameInternalRep _ANSI_ARGS_((Tcl_Obj *objPtr,
			    Tcl_Obj *copyPtr));
static void		FreeNsNameInternalRep _ANSI_ARGS_((
			    Tcl_Obj *objPtr));
static int		GetNamespaceFromObj _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr,
			    Tcl_Namespace **nsPtrPtr));
static int		InvokeImportedCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceChildrenCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceCodeCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceCurrentCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceDeleteCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceEvalCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceExportCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceForgetCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static void		NamespaceFree _ANSI_ARGS_((Namespace *nsPtr));
static int		NamespaceImportCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceInscopeCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceOriginCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceParentCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceQualifiersCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceTailCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		NamespaceWhichCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
static int		SetNsNameFromAny _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr));
static void		UpdateStringOfNsName _ANSI_ARGS_((Tcl_Obj *objPtr));

/*
 * This structure defines a Tcl object type that contains a
 * namespace reference.  It is used in commands that take the
 * name of a namespace as an argument.  The namespace reference
 * is resolved, and the result in cached in the object.
 */

Tcl_ObjType tclNsNameType = {
    "nsName",			/* the type's name */
    FreeNsNameInternalRep,	/* freeIntRepProc */
    DupNsNameInternalRep,	/* dupIntRepProc */
    UpdateStringOfNsName,	/* updateStringProc */
    SetNsNameFromAny		/* setFromAnyProc */
};

/*
 * Boolean flag indicating whether or not the namespName object
 * type has been registered with the Tcl compiler.
 */

static int nsInitialized = 0;

/*
 *----------------------------------------------------------------------
 *
 * TclInitNamespaces --
 *
 *	Called when any interpreter is created to make sure that
 *	things are properly set up for namespaces.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	On the first call, the namespName object type is registered
 *	with the Tcl compiler.
 *
 *----------------------------------------------------------------------
 */

void
TclInitNamespaces()
{
    if (!nsInitialized) {
        Tcl_RegisterObjType(&tclNsNameType);
        nsInitialized = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCurrentNamespace --
 *
 *	Returns a pointer to an interpreter's currently active namespace.
 *
 * Results:
 *	Returns a pointer to the interpreter's current namespace.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Namespace *
Tcl_GetCurrentNamespace(interp)
    register Tcl_Interp *interp; /* Interpreter whose current namespace is
				  * being queried. */
{
    register Interp *iPtr = (Interp *) interp;
    register Namespace *nsPtr;

    if (iPtr->varFramePtr != NULL) {
        nsPtr = iPtr->varFramePtr->nsPtr;
    } else {
        nsPtr = iPtr->globalNsPtr;
    }
    return (Tcl_Namespace *) nsPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetGlobalNamespace --
 *
 *	Returns a pointer to an interpreter's global :: namespace.
 *
 * Results:
 *	Returns a pointer to the specified interpreter's global namespace.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Namespace *
Tcl_GetGlobalNamespace(interp)
    register Tcl_Interp *interp; /* Interpreter whose global namespace 
				  * should be returned. */
{
    register Interp *iPtr = (Interp *) interp;
    
    return (Tcl_Namespace *) iPtr->globalNsPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PushCallFrame --
 *
 *	Pushes a new call frame onto the interpreter's Tcl call stack.
 *	Called when executing a Tcl procedure or a "namespace eval" or
 *	"namespace inscope" command. 
 *
 * Results:
 *	Returns TCL_OK if successful, or TCL_ERROR (along with an error
 *	message in the interpreter's result object) if something goes wrong.
 *
 * Side effects:
 *	Modifies the interpreter's Tcl call stack.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_PushCallFrame(interp, callFramePtr, namespacePtr, isProcCallFrame)
    Tcl_Interp *interp;		 /* Interpreter in which the new call frame
				  * is to be pushed. */
    Tcl_CallFrame *callFramePtr; /* Points to a call frame structure to
				  * push. Storage for this have already been
				  * allocated by the caller; typically this
				  * is the address of a CallFrame structure
				  * allocated on the caller's C stack.  The
				  * call frame will be initialized by this
				  * procedure. The caller can pop the frame
				  * later with Tcl_PopCallFrame, and it is
				  * responsible for freeing the frame's
				  * storage. */
    Tcl_Namespace *namespacePtr; /* Points to the namespace in which the
				  * frame will execute. If NULL, the
				  * interpreter's current namespace will
				  * be used. */
    int isProcCallFrame;	 /* If nonzero, the frame represents a
				  * called Tcl procedure and may have local
				  * vars. Vars will ordinarily be looked up
				  * in the frame. If new variables are
				  * created, they will be created in the
				  * frame. If 0, the frame is for a
				  * "namespace eval" or "namespace inscope"
				  * command and var references are treated
				  * as references to namespace variables. */
{
    Interp *iPtr = (Interp *) interp;
    register CallFrame *framePtr = (CallFrame *) callFramePtr;
    register Namespace *nsPtr;

    if (namespacePtr == NULL) {
	nsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    } else {
        nsPtr = (Namespace *) namespacePtr;
        if (nsPtr->flags & NS_DEAD) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "namespace \"",
		    nsPtr->fullName, "\" not found in context \"",
		    Tcl_GetCurrentNamespace(interp)->fullName, "\"",
		    (char *) NULL);
            return TCL_ERROR;
        }
    }

    nsPtr->activationCount++;
    framePtr->nsPtr = nsPtr;
    framePtr->isProcCallFrame = isProcCallFrame;
    framePtr->objc = 0;
    framePtr->objv = NULL;
    framePtr->callerPtr = iPtr->framePtr;
    framePtr->callerVarPtr = iPtr->varFramePtr;
    if (iPtr->varFramePtr != NULL) {
        framePtr->level = (iPtr->varFramePtr->level + 1);
    } else {
        framePtr->level = 1;
    }
    framePtr->procPtr = NULL; 	   /* no called procedure */
    framePtr->varTablePtr = NULL;  /* and no local variables */
    framePtr->numCompiledLocals = 0;
    framePtr->compiledLocals = NULL;

    /*
     * Push the new call frame onto the interpreter's stack of procedure
     * call frames making it the current frame.
     */

    iPtr->framePtr = framePtr;
    iPtr->varFramePtr = framePtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PopCallFrame --
 *
 *	Removes a call frame from the Tcl call stack for the interpreter.
 *	Called to remove a frame previously pushed by Tcl_PushCallFrame.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the call stack of the interpreter. Resets various fields of
 *	the popped call frame. If a namespace has been deleted and
 *	has no more activations on the call stack, the namespace is
 *	destroyed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_PopCallFrame(interp)
    Tcl_Interp* interp;		/* Interpreter with call frame to pop. */
{
    register Interp *iPtr = (Interp *) interp;
    register CallFrame *framePtr = iPtr->framePtr;
    int saveErrFlag;
    Namespace *nsPtr;

    /*
     * It's important to remove the call frame from the interpreter's stack
     * of call frames before deleting local variables, so that traces
     * invoked by the variable deletion don't see the partially-deleted
     * frame.
     */

    iPtr->framePtr = framePtr->callerPtr;
    iPtr->varFramePtr = framePtr->callerVarPtr;

    /*
     * Delete the local variables. As a hack, we save then restore the
     * ERR_IN_PROGRESS flag in the interpreter. The problem is that there
     * could be unset traces on the variables, which cause scripts to be
     * evaluated. This will clear the ERR_IN_PROGRESS flag, losing stack
     * trace information if the procedure was exiting with an error. The
     * code below preserves the flag. Unfortunately, that isn't really
     * enough: we really should preserve the errorInfo variable too
     * (otherwise a nested error in the trace script will trash errorInfo).
     * What's really needed is a general-purpose mechanism for saving and
     * restoring interpreter state.
     */

    saveErrFlag = (iPtr->flags & ERR_IN_PROGRESS);

    if (framePtr->varTablePtr != NULL) {
        TclDeleteVars(iPtr, framePtr->varTablePtr);
        ckfree((char *) framePtr->varTablePtr);
        framePtr->varTablePtr = NULL;
    }
    if (framePtr->numCompiledLocals > 0) {
        TclDeleteCompiledLocalVars(iPtr, framePtr);
    }

    iPtr->flags |= saveErrFlag;

    /*
     * Decrement the namespace's count of active call frames. If the
     * namespace is "dying" and there are no more active call frames,
     * call Tcl_DeleteNamespace to destroy it.
     */

    nsPtr = framePtr->nsPtr;
    nsPtr->activationCount--;
    if ((nsPtr->flags & NS_DYING)
	    && (nsPtr->activationCount == 0)) {
        Tcl_DeleteNamespace((Tcl_Namespace *) nsPtr);
    }
    framePtr->nsPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateNamespace --
 *
 *	Creates a new namespace with the given name. If there is no
 *	active namespace (i.e., the interpreter is being initialized),
 *	the global :: namespace is created and returned.
 *
 * Results:
 *	Returns a pointer to the new namespace if successful. If the
 *	namespace already exists or if another error occurs, this routine
 *	returns NULL, along with an error message in the interpreter's
 *	result object.
 *
 * Side effects:
 *	If the name contains "::" qualifiers and a parent namespace does
 *	not already exist, it is automatically created. 
 *
 *----------------------------------------------------------------------
 */

Tcl_Namespace *
Tcl_CreateNamespace(interp, name, clientData, deleteProc)
    Tcl_Interp *interp;             /* Interpreter in which a new namespace
				     * is being created. Also used for
				     * error reporting. */
    char *name;                     /* Name for the new namespace. May be a
				     * qualified name with names of ancestor
				     * namespaces separated by "::"s. */
    ClientData clientData;	    /* One-word value to store with
				     * namespace. */
    Tcl_NamespaceDeleteProc *deleteProc;
    				    /* Procedure called to delete client
				     * data when the namespace is deleted.
				     * NULL if no procedure should be
				     * called.*/
{
    Interp *iPtr = (Interp *) interp;
    register Namespace *nsPtr, *ancestorPtr;
    Namespace *parentPtr, *dummy1Ptr, *dummy2Ptr;
    Namespace *globalNsPtr = iPtr->globalNsPtr;
    Tcl_HashEntry *entryPtr;
    Tcl_DString buffer1, buffer2;
    int newEntry, result;

    /*
     * Check first if there is no active namespace. If so, we assume
     * the interpreter is being initialized. 
     */

    if ((globalNsPtr == NULL) && (iPtr->varFramePtr == NULL)) {
	/*
	 * Treat this namespace as the global namespace, and avoid
	 * looking for a parent.
	 */
	
        parentPtr = NULL;
        name = "";
    } else {
	/*
	 * There is no active namespace. Find the parent namespace that will
	 * contain the new namespace.
	 */

	result = TclGetNamespaceForQualName(interp, name,
		(Namespace *) NULL,
		/*flags*/ (CREATE_NS_IF_UNKNOWN | TCL_LEAVE_ERR_MSG),
		&parentPtr, &dummy1Ptr, &dummy2Ptr, &name);
        if (result != TCL_OK) {
            return NULL;
        }

        /*
         * Check for a bad namespace name and make sure that the name
	 * does not already exist in the parent namespace.
	 */

        if ((name == NULL) || (*name == '\0')) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "can't create namespace \"", name,
    	    	    "\": invalid name", (char *) NULL);
            return NULL;
        }
        if (Tcl_FindHashEntry(&parentPtr->childTable, name) != NULL) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "can't create namespace \"", name,
    	    	    "\": already exists", (char *) NULL);
            return NULL;
        }
    }

    /*
     * Create the new namespace and root it in its parent. Increment the
     * count of namespaces created.
     */

    numNsCreated++;

    nsPtr = (Namespace *) ckalloc(sizeof(Namespace));
    nsPtr->name            = (char *) ckalloc((unsigned) (strlen(name)+1));
    strcpy(nsPtr->name, name);
    nsPtr->fullName        = NULL;   /* set below */
    nsPtr->clientData      = clientData;
    nsPtr->deleteProc      = deleteProc;
    nsPtr->parentPtr       = parentPtr;
    Tcl_InitHashTable(&nsPtr->childTable, TCL_STRING_KEYS);
    nsPtr->nsId            = numNsCreated;
    nsPtr->interp          = interp;
    nsPtr->flags           = 0;
    nsPtr->activationCount = 0;
    nsPtr->refCount        = 0;
    Tcl_InitHashTable(&nsPtr->cmdTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&nsPtr->varTable, TCL_STRING_KEYS);
    nsPtr->exportArrayPtr  = NULL;
    nsPtr->numExportPatterns = 0;
    nsPtr->maxExportPatterns = 0;
    nsPtr->cmdRefEpoch     = 0;

    if (parentPtr != NULL) {
        entryPtr = Tcl_CreateHashEntry(&parentPtr->childTable, name,
	        &newEntry);
        Tcl_SetHashValue(entryPtr, (ClientData) nsPtr);
    }

    /*
     * Build the fully qualified name for this namespace.
     */

    Tcl_DStringInit(&buffer1);
    Tcl_DStringInit(&buffer2);
    for (ancestorPtr = nsPtr;  ancestorPtr != NULL;
	    ancestorPtr = ancestorPtr->parentPtr) {
        if (ancestorPtr != globalNsPtr) {
            Tcl_DStringAppend(&buffer1, "::", 2);
            Tcl_DStringAppend(&buffer1, ancestorPtr->name, -1);
        }
        Tcl_DStringAppend(&buffer1, Tcl_DStringValue(&buffer2), -1);

        Tcl_DStringSetLength(&buffer2, 0);
        Tcl_DStringAppend(&buffer2, Tcl_DStringValue(&buffer1), -1);
        Tcl_DStringSetLength(&buffer1, 0);
    }
    
    name = Tcl_DStringValue(&buffer2);
    nsPtr->fullName = (char *) ckalloc((unsigned) (strlen(name)+1));
    strcpy(nsPtr->fullName, name);

    Tcl_DStringFree(&buffer1);
    Tcl_DStringFree(&buffer2);

    /*
     * Return a pointer to the new namespace.
     */

    return (Tcl_Namespace *) nsPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteNamespace --
 *
 *	Deletes a namespace and all of the commands, variables, and other
 *	namespaces within it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When a namespace is deleted, it is automatically removed as a
 *	child of its parent namespace. Also, all its commands, variables
 *	and child namespaces are deleted.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteNamespace(namespacePtr)
    Tcl_Namespace *namespacePtr;   /* Points to the namespace to delete. */
{
    register Namespace *nsPtr = (Namespace *) namespacePtr;
    Interp *iPtr = (Interp *) nsPtr->interp;
    Namespace *globalNsPtr =
	    (Namespace *) Tcl_GetGlobalNamespace((Tcl_Interp *) iPtr);
    Tcl_HashEntry *entryPtr;

    /*
     * If the namespace is on the call frame stack, it is marked as "dying"
     * (NS_DYING is OR'd into its flags): the namespace can't be looked up
     * by name but its commands and variables are still usable by those
     * active call frames. When all active call frames referring to the
     * namespace have been popped from the Tcl stack, Tcl_PopCallFrame will
     * call this procedure again to delete everything in the namespace.
     * If no nsName objects refer to the namespace (i.e., if its refCount 
     * is zero), its commands and variables are deleted and the storage for
     * its namespace structure is freed. Otherwise, if its refCount is
     * nonzero, the namespace's commands and variables are deleted but the
     * structure isn't freed. Instead, NS_DEAD is OR'd into the structure's
     * flags to allow the namespace resolution code to recognize that the
     * namespace is "deleted". The structure's storage is freed by
     * FreeNsNameInternalRep when its refCount reaches 0.
     */

    if (nsPtr->activationCount > 0) {
        nsPtr->flags |= NS_DYING;
        if (nsPtr->parentPtr != NULL) {
            entryPtr = Tcl_FindHashEntry(&nsPtr->parentPtr->childTable,
		    nsPtr->name);
            if (entryPtr != NULL) {
                Tcl_DeleteHashEntry(entryPtr);
            }
        }
        nsPtr->parentPtr = NULL;
    } else {
	/*
	 * Delete the namespace and everything in it. If this is the global
	 * namespace, then clear it but don't free its storage unless the
	 * interpreter is being torn down.
	 */

        TclTeardownNamespace(nsPtr);

        if ((nsPtr != globalNsPtr) || (iPtr->flags & DELETED)) {
            /*
	     * If this is the global namespace, then it may have residual
             * "errorInfo" and "errorCode" variables for errors that
             * occurred while it was being torn down.  Try to clear the
             * variable list one last time.
	     */

            TclDeleteVars((Interp *) nsPtr->interp, &nsPtr->varTable);
	    
            Tcl_DeleteHashTable(&nsPtr->childTable);
            Tcl_DeleteHashTable(&nsPtr->cmdTable);

            /*
             * If the reference count is 0, then discard the namespace.
             * Otherwise, mark it as "dead" so that it can't be used.
             */

            if (nsPtr->refCount == 0) {
                NamespaceFree(nsPtr);
            } else {
                nsPtr->flags |= NS_DEAD;
            }
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclTeardownNamespace --
 *
 *	Used internally to dismantle and unlink a namespace when it is
 *	deleted. Divorces the namespace from its parent, and deletes all
 *	commands, variables, and child namespaces.
 *
 *	This is kept separate from Tcl_DeleteNamespace so that the global
 *	namespace can be handled specially. Global variables like
 *	"errorInfo" and "errorCode" need to remain intact while other
 *	namespaces and commands are torn down, in case any errors occur.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes this namespace from its parent's child namespace hashtable.
 *	Deletes all commands, variables and namespaces in this namespace.
 *	If this is the global namespace, the "errorInfo" and "errorCode"
 *	variables are left alone and deleted later.
 *
 *----------------------------------------------------------------------
 */

void
TclTeardownNamespace(nsPtr)
    register Namespace *nsPtr;	/* Points to the namespace to be dismantled
				 * and unlinked from its parent. */
{
    Interp *iPtr = (Interp *) nsPtr->interp;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Namespace *childNsPtr;
    Tcl_Command cmd;
    Namespace *globalNsPtr =
	    (Namespace *) Tcl_GetGlobalNamespace((Tcl_Interp *) iPtr);
    int i;

    /*
     * Start by destroying the namespace's variable table,
     * since variables might trigger traces.
     */

    if (nsPtr == globalNsPtr) {
	/*
	 * This is the global namespace, so be careful to preserve the
	 * "errorInfo" and "errorCode" variables. These might be needed
	 * later on if errors occur while deleting commands. We are careful
	 * to destroy and recreate the "errorInfo" and "errorCode"
	 * variables, in case they had any traces on them.
	 */
    
        char *str, *errorInfoStr, *errorCodeStr;

        str = Tcl_GetVar((Tcl_Interp *) iPtr, "errorInfo", TCL_GLOBAL_ONLY);
        if (str != NULL) {
            errorInfoStr = ckalloc((unsigned) (strlen(str)+1));
            strcpy(errorInfoStr, str);
        } else {
            errorInfoStr = NULL;
        }

        str = Tcl_GetVar((Tcl_Interp *) iPtr, "errorCode", TCL_GLOBAL_ONLY);
        if (str != NULL) {
            errorCodeStr = ckalloc((unsigned) (strlen(str)+1));
            strcpy(errorCodeStr, str);
        } else {
            errorCodeStr = NULL;
        }

        TclDeleteVars(iPtr, &nsPtr->varTable);
        Tcl_InitHashTable(&nsPtr->varTable, TCL_STRING_KEYS);

        if (errorInfoStr != NULL) {
            Tcl_SetVar((Tcl_Interp *) iPtr, "errorInfo", errorInfoStr,
                TCL_GLOBAL_ONLY);
            ckfree(errorInfoStr);
        }
        if (errorCodeStr != NULL) {
            Tcl_SetVar((Tcl_Interp *) iPtr, "errorCode", errorCodeStr,
                TCL_GLOBAL_ONLY);
            ckfree(errorCodeStr);
        }
    } else {
	/*
	 * Variable table should be cleared but not freed! TclDeleteVars
	 * frees it, so we reinitialize it afterwards.
	 */
    
        TclDeleteVars(iPtr, &nsPtr->varTable);
        Tcl_InitHashTable(&nsPtr->varTable, TCL_STRING_KEYS);
    }

    /*
     * Remove the namespace from its parent's child hashtable.
     */

    if (nsPtr->parentPtr != NULL) {
        entryPtr = Tcl_FindHashEntry(&nsPtr->parentPtr->childTable,
	        nsPtr->name);
        if (entryPtr != NULL) {
            Tcl_DeleteHashEntry(entryPtr);
        }
    }
    nsPtr->parentPtr = NULL;

    /*
     * Delete all the child namespaces.
     *
     * BE CAREFUL: When each child is deleted, it will divorce
     *    itself from its parent. You can't traverse a hash table
     *    properly if its elements are being deleted. We use only
     *    the Tcl_FirstHashEntry function to be safe.
     */

    for (entryPtr = Tcl_FirstHashEntry(&nsPtr->childTable, &search);
            entryPtr != NULL;
            entryPtr = Tcl_FirstHashEntry(&nsPtr->childTable, &search)) {
        childNsPtr = (Tcl_Namespace *) Tcl_GetHashValue(entryPtr);
        Tcl_DeleteNamespace(childNsPtr);
    }

    /*
     * Delete all commands in this namespace. Be careful when traversing the
     * hash table: when each command is deleted, it removes itself from the
     * command table. There's a special hack here because "tkerror" is just
     * a synonym for "bgerror" (they share a Command structure). Just
     * delete the hash table entry for "tkerror" without invoking its
     * callback or cleaning up its Command structure.
     */

    hPtr = Tcl_FindHashEntry(&nsPtr->cmdTable, "tkerror");
    if (hPtr != NULL) {
	Tcl_DeleteHashEntry(hPtr);
    }
    for (entryPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search);
            entryPtr != NULL;
            entryPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search)) {
        cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
        Tcl_DeleteCommandFromToken((Tcl_Interp *) iPtr, cmd);
    }
    Tcl_DeleteHashTable(&nsPtr->cmdTable);
    Tcl_InitHashTable(&nsPtr->cmdTable, TCL_STRING_KEYS);

    /*
     * Free the namespace's export pattern array.
     */

    if (nsPtr->exportArrayPtr != NULL) {
	for (i = 0;  i < nsPtr->numExportPatterns;  i++) {
	    ckfree(nsPtr->exportArrayPtr[i]);
	}
        ckfree((char *) nsPtr->exportArrayPtr);
	nsPtr->exportArrayPtr = NULL;
	nsPtr->numExportPatterns = 0;
	nsPtr->maxExportPatterns = 0;
    }

    /*
     * Free any client data associated with the namespace.
     */

    if (nsPtr->deleteProc != NULL) {
        (*nsPtr->deleteProc)(nsPtr->clientData);
    }
    nsPtr->deleteProc = NULL;
    nsPtr->clientData = NULL;

    /*
     * Reset the namespace's id field to ensure that this namespace won't
     * be interpreted as valid by, e.g., the cache validation code for
     * cached command references in Tcl_GetCommandFromObj.
     */

    nsPtr->nsId = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceFree --
 *
 *	Called after a namespace has been deleted, when its
 *	reference count reaches 0.  Frees the data structure
 *	representing the namespace.
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
NamespaceFree(nsPtr)
    register Namespace *nsPtr;	/* Points to the namespace to free. */
{
    /*
     * Most of the namespace's contents are freed when the namespace is
     * deleted by Tcl_DeleteNamespace. All that remains is to free its names
     * (for error messages), and the structure itself.
     */

    ckfree(nsPtr->name);
    ckfree(nsPtr->fullName);

    ckfree((char *) nsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Export --
 *
 *	Makes all the commands matching a pattern available to later ber
 *	imported from the namespace specified by contextNsPtr (or the
 *	current namespace if contextNsPtr is NULL). The specified pattern is
 *	appended onto the namespace's export pattern list, which is
 *	optionally cleared beforehand.
 *
 * Results:
 *	Returns TCL_OK if successful, or TCL_ERROR (along with an error
 *	message in the interpreter's result) if something goes wrong.
 *
 * Side effects:
 *	Appends the export pattern onto the namespace's export list.
 *	Optionally reset the namespace's export pattern list.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Export(interp, namespacePtr, pattern, resetListFirst)
    Tcl_Interp *interp;		 /* Current interpreter. */
    Tcl_Namespace *namespacePtr; /* Points to the namespace from which 
				  * commands are to be exported. NULL for
                                  * the current namespace. */
    char *pattern;               /* String pattern indicating which commands
                                  * to export. This pattern may not include
				  * any namespace qualifiers; only commands
				  * in the specified namespace may be
				  * exported. */
    int resetListFirst;		 /* If nonzero, resets the namespace's
				  * export list before appending 
				  * be overwritten by imported commands.
				  * If 0, return an error if an imported
				  * cmd conflicts with an existing one. */
{
#define INIT_EXPORT_PATTERNS 5    
    Namespace *nsPtr, *exportNsPtr, *altNsPtr, *dummyPtr;
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    char *simplePattern, *patternCpy;
    int neededElems, len, i, result;

    /*
     * If the specified namespace is NULL, use the current namespace.
     */

    if (namespacePtr == NULL) {
        nsPtr = (Namespace *) currNsPtr;
    } else {
        nsPtr = (Namespace *) namespacePtr;
    }

    /*
     * If resetListFirst is true (nonzero), clear the namespace's export
     * pattern list.
     */

    if (resetListFirst) {
	if (nsPtr->exportArrayPtr != NULL) {
	    for (i = 0;  i < nsPtr->numExportPatterns;  i++) {
		ckfree(nsPtr->exportArrayPtr[i]);
	    }
	    ckfree((char *) nsPtr->exportArrayPtr);
	    nsPtr->exportArrayPtr = NULL;
	    nsPtr->numExportPatterns = 0;
	    nsPtr->maxExportPatterns = 0;
	}
    }

    /*
     * Check that the pattern doesn't have namespace qualifiers.
     */

    result = TclGetNamespaceForQualName(interp, pattern, nsPtr,
	    /*flags*/ TCL_LEAVE_ERR_MSG, &exportNsPtr, &altNsPtr,
	    &dummyPtr, &simplePattern);
    if (result != TCL_OK) {
	return result;
    }
    if (exportNsPtr == NULL) {
	exportNsPtr = altNsPtr;
    }
    if ((exportNsPtr != currNsPtr)
	    || (strcmp(pattern, simplePattern) != 0)) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
	        "invalid export pattern \"", pattern,
		"\": pattern can't specify a namespace",
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Make sure there is room in the namespace's pattern array for the
     * new pattern.
     */

    neededElems = currNsPtr->numExportPatterns + 1;
    if (currNsPtr->exportArrayPtr == NULL) {
	currNsPtr->exportArrayPtr = (char **)
	        ckalloc((unsigned) (INIT_EXPORT_PATTERNS * sizeof(char *)));
	currNsPtr->numExportPatterns = 0;
	currNsPtr->maxExportPatterns = INIT_EXPORT_PATTERNS;
    } else if (neededElems > currNsPtr->maxExportPatterns) {
	int numNewElems = 2 * currNsPtr->maxExportPatterns;
	size_t currBytes = currNsPtr->numExportPatterns * sizeof(char *);
	size_t newBytes  = numNewElems * sizeof(char *);
	char **newPtr = (char **) ckalloc((unsigned) newBytes);

	memcpy((VOID *) newPtr, (VOID *) currNsPtr->exportArrayPtr,
	        currBytes);
	ckfree((char *) currNsPtr->exportArrayPtr);
	currNsPtr->exportArrayPtr = (char **) newPtr;
	currNsPtr->maxExportPatterns = numNewElems;
    }

    /*
     * Add the pattern to the namespace's array of export patterns.
     */

    len = strlen(pattern);
    patternCpy = (char *) ckalloc((unsigned) (len + 1));
    strcpy(patternCpy, pattern);
    
    currNsPtr->exportArrayPtr[currNsPtr->numExportPatterns] = patternCpy;
    currNsPtr->numExportPatterns++;
    return TCL_OK;
#undef INIT_EXPORT_PATTERNS
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendExportList --
 *
 *	Appends onto the argument object the list of export patterns for the
 *	specified namespace.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case the object
 *	referenced by objPtr has each export pattern appended to it. If an
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
Tcl_AppendExportList(interp, namespacePtr, objPtr)
    Tcl_Interp *interp;		 /* Interpreter used for error reporting. */
    Tcl_Namespace *namespacePtr; /* Points to the namespace whose export
				  * pattern list is appended onto objPtr.
				  * NULL for the current namespace. */
    Tcl_Obj *objPtr;		 /* Points to the Tcl object onto which the
				  * export pattern list is appended. */
{
    Namespace *nsPtr;
    int i, result;

    /*
     * If the specified namespace is NULL, use the current namespace.
     */

    if (namespacePtr == NULL) {
        nsPtr = (Namespace *) (Namespace *) Tcl_GetCurrentNamespace(interp);
    } else {
        nsPtr = (Namespace *) namespacePtr;
    }

    /*
     * Append the export pattern list onto objPtr.
     */

    for (i = 0;  i < nsPtr->numExportPatterns;  i++) {
	result = Tcl_ListObjAppendElement(interp, objPtr,
		Tcl_NewStringObj(nsPtr->exportArrayPtr[i], -1));
	if (result != TCL_OK) {
	    return result;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Import --
 *
 *	Imports all of the commands matching a pattern into the namespace
 *	specified by contextNsPtr (or the current namespace if contextNsPtr
 *	is NULL). This is done by creating a new command (the "imported
 *	command") that points to the real command in its original namespace.
 *
 * Results:
 *	Returns TCL_OK if successful, or TCL_ERROR (along with an error
 *	message in the interpreter's result) if something goes wrong.
 *
 * Side effects:
 *	Creates new commands in the importing namespace. These indirect
 *	calls back to the real command and are deleted if the real commands
 *	are deleted.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Import(interp, namespacePtr, pattern, allowOverwrite)
    Tcl_Interp *interp;		 /* Current interpreter. */
    Tcl_Namespace *namespacePtr; /* Points to the namespace into which the
				  * commands are to be imported. NULL for
                                  * the current namespace. */
    char *pattern;               /* String pattern indicating which commands
                                  * to import. This pattern should be
				  * qualified by the name of the namespace
				  * from which to import the command(s). */
    int allowOverwrite;		 /* If nonzero, allow existing commands to
				  * be overwritten by imported commands.
				  * If 0, return an error if an imported
				  * cmd conflicts with an existing one. */
{
    Interp *iPtr = (Interp *) interp;
    Namespace *nsPtr, *importNsPtr, *dummyPtr, *actualCtxPtr;
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    char *simplePattern, *cmdName;
    register Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Command *cmdPtr;
    ImportRef *refPtr;
    Tcl_Command importedCmd;
    ImportedCmdData *dataPtr;
    int wasExported, i, result;

    /*
     * If the specified namespace is NULL, use the current namespace.
     */

    if (namespacePtr == NULL) {
        nsPtr = (Namespace *) currNsPtr;
    } else {
        nsPtr = (Namespace *) namespacePtr;
    }

    /*
     * From the pattern, find the namespace from which we are importing
     * and get the simple pattern (no namespace qualifiers or ::'s) at
     * the end.
     */

    if (strlen(pattern) == 0) {
	Tcl_SetStringObj(Tcl_GetObjResult(interp),
	        "empty import pattern", -1);
        return TCL_ERROR;
    }
    result = TclGetNamespaceForQualName(interp, pattern, nsPtr,
	    /*flags*/ TCL_LEAVE_ERR_MSG, &importNsPtr, &dummyPtr,
	    &actualCtxPtr, &simplePattern);
    if (result != TCL_OK) {
        return TCL_ERROR;
    }
    if (importNsPtr == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"unknown namespace in import pattern \"",
		pattern, "\"", (char *) NULL);
        return TCL_ERROR;
    }
    if (importNsPtr == nsPtr) {
	if (pattern == simplePattern) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "no namespace specified in import pattern \"", pattern,
		    "\"", (char *) NULL);
	} else {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "import pattern \"", pattern,
		    "\" tries to import from namespace \"",
		    importNsPtr->name, "\" into itself", (char *) NULL);
	}
        return TCL_ERROR;
    }

    /*
     * Scan through the command table in the source namespace and look for
     * exported commands that match the string pattern. Create an "imported
     * command" in the current namespace for each imported command; these
     * commands redirect their invocations to the "real" command.
     */

    for (hPtr = Tcl_FirstHashEntry(&importNsPtr->cmdTable, &search);
	    (hPtr != NULL);
	    hPtr = Tcl_NextHashEntry(&search)) {
        cmdName = Tcl_GetHashKey(&importNsPtr->cmdTable, hPtr);
        if (Tcl_StringMatch(cmdName, simplePattern)) {
	    /*
	     * The command cmdName in the source namespace matches the
	     * pattern. Check whether it was exported. If it wasn't,
	     * we ignore it.
	     */

	    wasExported = 0;
	    for (i = 0;  i < importNsPtr->numExportPatterns;  i++) {
		if (Tcl_StringMatch(cmdName,
			importNsPtr->exportArrayPtr[i])) {
		    wasExported = 1;
		    break;
		}
	    }
	    if (!wasExported) {
		continue;
            }

	    /*
	     * Unless there is a name clash, create an imported command
	     * in the current namespace that refers to cmdPtr.
	     */
	    
            if ((Tcl_FindHashEntry(&nsPtr->cmdTable, cmdName) == NULL)
		    || allowOverwrite) {
		/*
		 * Create the imported command and its client data.
		 * To create the new command in the current namespace, 
		 * generate a fully qualified name for it.
		 */

		Tcl_DString ds;

		Tcl_DStringInit(&ds);
		Tcl_DStringAppend(&ds, currNsPtr->fullName, -1);
		if (currNsPtr != iPtr->globalNsPtr) {
		    Tcl_DStringAppend(&ds, "::", 2);
		}
		Tcl_DStringAppend(&ds, cmdName, -1);
		
		cmdPtr = (Command *) Tcl_GetHashValue(hPtr);
		dataPtr = (ImportedCmdData *)
		        ckalloc(sizeof(ImportedCmdData));
                importedCmd = Tcl_CreateObjCommand(interp, 
                        Tcl_DStringValue(&ds), InvokeImportedCmd,
                        (ClientData) dataPtr, DeleteImportedCmd);
		dataPtr->realCmdPtr = cmdPtr;
		dataPtr->selfPtr = (Command *) importedCmd;

		/*
		 * Create an ImportRef structure describing this new import
		 * command and add it to the import ref list in the "real"
		 * command.
		 */

                refPtr = (ImportRef *) ckalloc(sizeof(ImportRef));
                refPtr->importedCmdPtr = (Command *) importedCmd;
                refPtr->nextPtr = cmdPtr->importRefPtr;
                cmdPtr->importRefPtr = refPtr;
            } else {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		        "can't import command \"", cmdName,
			"\": already exists", (char *) NULL);
                return TCL_ERROR;
            }
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ForgetImport --
 *
 *	Deletes previously imported commands. Given a pattern that may
 *	include the name of an exporting namespace, this procedure first
 *	finds all matching exported commands. It then looks in the namespace
 *	specified by namespacePtr for any corresponding previously imported
 *	commands, which it deletes. If namespacePtr is NULL, commands are
 *	deleted from the current namespace.
 *
 * Results:
 *	Returns TCL_OK if successful. If there is an error, returns
 *	TCL_ERROR and puts an error message in the interpreter's result
 *	object.
 *
 * Side effects:
 *	May delete commands. 
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ForgetImport(interp, namespacePtr, pattern)
    Tcl_Interp *interp;		 /* Current interpreter. */
    Tcl_Namespace *namespacePtr; /* Points to the namespace from which
				  * previously imported commands should be
				  * removed. NULL for current namespace. */
    char *pattern;		 /* String pattern indicating which imported
				  * commands to remove. This pattern should
				  * be qualified by the name of the
				  * namespace from which the command(s) were
				  * imported. */
{
    Namespace *nsPtr, *importNsPtr, *dummyPtr, *actualCtxPtr;
    char *simplePattern, *cmdName;
    register Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Command *cmdPtr;
    int result;

    /*
     * If the specified namespace is NULL, use the current namespace.
     */

    if (namespacePtr == NULL) {
        nsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    } else {
        nsPtr = (Namespace *) namespacePtr;
    }

    /*
     * From the pattern, find the namespace from which we are importing
     * and get the simple pattern (no namespace qualifiers or ::'s) at
     * the end.
     */

    result = TclGetNamespaceForQualName(interp, pattern, nsPtr,
	    /*flags*/ TCL_LEAVE_ERR_MSG, &importNsPtr, &dummyPtr,
	    &actualCtxPtr, &simplePattern);
    if (result != TCL_OK) {
        return result;
    }
    if (importNsPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"unknown namespace in namespace forget pattern \"",
		pattern, "\"", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * Scan through the command table in the source namespace and look for
     * exported commands that match the string pattern. If the current
     * namespace has an imported command that refers to one of those real
     * commands, delete it.
     */

    for (hPtr = Tcl_FirstHashEntry(&importNsPtr->cmdTable, &search);
            (hPtr != NULL);
            hPtr = Tcl_NextHashEntry(&search)) {
        cmdName = Tcl_GetHashKey(&importNsPtr->cmdTable, hPtr);
        if (Tcl_StringMatch(cmdName, simplePattern)) {
            hPtr = Tcl_FindHashEntry(&nsPtr->cmdTable, cmdName);
            if (hPtr != NULL) {	/* cmd of same name in current namespace */
                cmdPtr = (Command *) Tcl_GetHashValue(hPtr);
                if (cmdPtr->deleteProc == DeleteImportedCmd) { 
                    Tcl_DeleteCommandFromToken(interp, (Tcl_Command) cmdPtr);
                }
            }
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetOriginalCommand --
 *
 *	An imported command is created in an namespace when it imports a
 *	"real" command from another namespace. If the specified command is a
 *	imported command, this procedure returns the original command it
 *	refers to.  
 *
 * Results:
 *	If the command was imported into a sequence of namespaces a, b,...,n
 *	where each successive namespace just imports the command from the
 *	previous namespace, this procedure returns the Tcl_Command token in
 *	the first namespace, a. Otherwise, if the specified command is not
 *	an imported command, the procedure returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclGetOriginalCommand(command)
    Tcl_Command command;	/* The command for which the original
				 * command should be returned. */
{
    register Command *cmdPtr = (Command *) command;
    ImportedCmdData *dataPtr;

    if (cmdPtr->deleteProc != DeleteImportedCmd) {
	return (Tcl_Command) NULL;
    }
    
    while (cmdPtr->deleteProc == DeleteImportedCmd) {
	dataPtr = (ImportedCmdData *) cmdPtr->objClientData;
	cmdPtr = dataPtr->realCmdPtr;
    }
    return (Tcl_Command) cmdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * InvokeImportedCmd --
 *
 *	Invoked by Tcl whenever the user calls an imported command that
 *	was created by Tcl_Import. Finds the "real" command (in another
 *	namespace), and passes control to it.
 *
 * Results:
 *	Returns TCL_OK if successful, and  TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result object is set to an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InvokeImportedCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Points to the imported command's
				 * ImportedCmdData structure. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* The argument objects. */
{
    register ImportedCmdData *dataPtr = (ImportedCmdData *) clientData;
    register Command *realCmdPtr = dataPtr->realCmdPtr;

    return (*realCmdPtr->objProc)(realCmdPtr->objClientData, interp,
            objc, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteImportedCmd --
 *
 *	Invoked by Tcl whenever an imported command is deleted. The "real"
 *	command keeps a list of all the imported commands that refer to it,
 *	so those imported commands can be deleted when the real command is
 *	deleted. This procedure removes the imported command reference from
 *	the real command's list, and frees up the memory associated with
 *	the imported command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the imported command from the real command's import list.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteImportedCmd(clientData)
    ClientData clientData;	/* Points to the imported command's
				 * ImportedCmdData structure. */
{
    ImportedCmdData *dataPtr = (ImportedCmdData *) clientData;
    Command *realCmdPtr = dataPtr->realCmdPtr;
    Command *selfPtr = dataPtr->selfPtr;
    register ImportRef *refPtr, *prevPtr;

    prevPtr = NULL;
    for (refPtr = realCmdPtr->importRefPtr;  refPtr != NULL;
            refPtr = refPtr->nextPtr) {
	if (refPtr->importedCmdPtr == selfPtr) {
	    /*
	     * Remove *refPtr from real command's list of imported commands
	     * that refer to it.
	     */
	    
	    if (prevPtr == NULL) { /* refPtr is first in list */
		realCmdPtr->importRefPtr = refPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = refPtr->nextPtr;
	    }
	    ckfree((char *) refPtr);
	    ckfree((char *) dataPtr);
	    return;
	}
	prevPtr = refPtr;
    }
	
    panic("DeleteImportedCmd: did not find cmd in real cmd's list of import references");
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetNamespaceForQualName --
 *
 *	Given a qualified name specifying a command, variable, or namespace,
 *	and a namespace in which to resolve the name, this procedure returns
 *	a pointer to the namespace that contains the item. A qualified name
 *	consists of the "simple" name of an item qualified by the names of
 *	an arbitrary number of containing namespace separated by "::"s. If
 *	the qualified name starts with "::", it is interpreted absolutely
 *	from the global namespace. Otherwise, it is interpreted relative to
 *	the namespace specified by cxtNsPtr if it is non-NULL. If cxtNsPtr
 *	is NULL, the name is interpreted relative to the current namespace.
 *
 *	A relative name like "foo::bar::x" can be found starting in either
 *	the current namespace or in the global namespace. So each search
 *	usually follows two tracks, and two possible namespaces are
 *	returned. If the procedure sets either *nsPtrPtr or *altNsPtrPtr to
 *	NULL, then that path failed.
 *
 *	If "flags" contains TCL_GLOBAL_ONLY, the relative qualified name is
 *	sought only in the global :: namespace. The alternate search
 *	(also) starting from the global namespace is ignored and
 *	*altNsPtrPtr is set NULL. 
 *
 *	If "flags" contains TCL_NAMESPACE_ONLY, the relative qualified
 *	name is sought only in the namespace specified by cxtNsPtr. The
 *	alternate search starting from the global namespace is ignored and
 *	*altNsPtrPtr is set NULL. If both TCL_GLOBAL_ONLY and
 *	TCL_NAMESPACE_ONLY are specified, TCL_GLOBAL_ONLY is ignored and
 *	the search starts from the namespace specified by cxtNsPtr.
 *
 *	If "flags" contains CREATE_NS_IF_UNKNOWN, all namespace
 *	components of the qualified name that cannot be found are
 *	automatically created within their specified parent. This makes sure
 *	that functions like Tcl_CreateCommand always succeed. There is no
 *	alternate search path, so *altNsPtrPtr is set NULL.
 *
 *	If "flags" contains FIND_ONLY_NS, the qualified name is treated as a
 *	reference to a namespace, and the entire qualified name is
 *	followed. If the name is relative, the namespace is looked up only
 *	in the current namespace. A pointer to the namespace is stored in
 *	*nsPtrPtr and NULL is stored in *simpleNamePtr. Otherwise, if
 *	FIND_ONLY_NS is not specified, only the leading components are
 *	treated as namespace names, and a pointer to the simple name of the
 *	final component is stored in *simpleNamePtr.
 *
 * Results:
 *	Ordinarily this procedure returns TCL_OK. It sets *nsPtrPtr and
 *	*altNsPtrPtr to point to the two possible namespaces which represent
 *	the last (containing) namespace in the qualified name. If the
 *	procedure sets either *nsPtrPtr or *altNsPtrPtr to NULL, then the
 *	search along that path failed. The procedure also stores a pointer
 *	to the simple name of the final component in *simpleNamePtr. If the
 *	qualified name is "::" or was treated as a namespace reference
 *	(FIND_ONLY_NS), the procedure stores a pointer to the
 *	namespace in *nsPtrPtr, NULL in *altNsPtrPtr, and sets
 *	*simpleNamePtr to point to an empty string.
 *
 *	If there is an error, this procedure returns TCL_ERROR. If "flags"
 *	contains TCL_LEAVE_ERR_MSG, an error message is returned in the
 *	interpreter's result object. Otherwise, the interpreter's result
 *	object is left unchanged.
 *
 *	*actualCxtPtrPtr is set to the actual context namespace. It is
 *	set to the input context namespace pointer in cxtNsPtr. If cxtNsPtr
 *	is NULL, it is set to the current namespace context.
 *
 * Side effects:
 *	If flags contains TCL_LEAVE_ERR_MSG and an error is encountered,
 *	the interpreter's result object will contain an error message.
 *
 *----------------------------------------------------------------------
 */

int
TclGetNamespaceForQualName(interp, qualName, cxtNsPtr, flags,
	nsPtrPtr, altNsPtrPtr, actualCxtPtrPtr, simpleNamePtr)
    Tcl_Interp *interp;		 /* Interpreter in which to find the
				  * namespace containing qualName. */
    register char *qualName;	 /* A namespace-qualified name of an
				  * command, variable, or namespace. */
    Namespace *cxtNsPtr;	 /* The namespace in which to start the
				  * search for qualName's namespace. If NULL
				  * start from the current namespace.
				  * Ignored if TCL_GLOBAL_ONLY or
				  * TCL_NAMESPACE_ONLY are set. */
    int flags;			 /* Flags controlling the search: an OR'd
				  * combination of TCL_GLOBAL_ONLY,
				  * TCL_NAMESPACE_ONLY,
				  * CREATE_NS_IF_UNKNOWN, and
				  * FIND_ONLY_NS. */
    Namespace **nsPtrPtr;	 /* Address where procedure stores a pointer
				  * to containing namespace if qualName is
				  * found starting from *cxtNsPtr or, if
				  * TCL_GLOBAL_ONLY is set, if qualName is
				  * found in the global :: namespace. NULL
				  * is stored otherwise. */
    Namespace **altNsPtrPtr;	 /* Address where procedure stores a pointer
				  * to containing namespace if qualName is
				  * found starting from the global ::
				  * namespace. NULL is stored if qualName
				  * isn't found starting from :: or if the
				  * TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY,
				  * CREATE_NS_IF_UNKNOWN, FIND_ONLY_NS flag
				  * is set. */
    Namespace **actualCxtPtrPtr; /* Address where procedure stores a pointer
				  * to the actual namespace from which the
				  * search started. This is either cxtNsPtr,
				  * the :: namespace if TCL_GLOBAL_ONLY was
				  * specified, or the current namespace if
				  * cxtNsPtr was NULL. */
    char **simpleNamePtr;	 /* Address where procedure stores the
				  * simple name at end of the qualName, or
				  * NULL if qualName is "::" or the flag
				  * FIND_ONLY_NS was specified. */
{
    Interp *iPtr = (Interp *) interp;
    Namespace *nsPtr = cxtNsPtr;
    Namespace *altNsPtr;
    Namespace *globalNsPtr = iPtr->globalNsPtr;
    register char *start, *end;
    char *nsName;
    Tcl_HashEntry *entryPtr;
    Tcl_DString buffer;
    int len, result;

    /*
     * Determine the context namespace nsPtr in which to start the primary
     * search. If TCL_NAMESPACE_ONLY or FIND_ONLY_NS was specified, search
     * from the current namespace. If the qualName name starts with a "::"
     * or TCL_GLOBAL_ONLY was specified, search from the global
     * namespace. Otherwise, use the given namespace given in cxtNsPtr, or
     * if that is NULL, use the current namespace context. Note that we
     * always treat two or more adjacent ":"s as a namespace separator.
     */

    if (flags & (TCL_NAMESPACE_ONLY | FIND_ONLY_NS)) {
	nsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    } else if (flags & TCL_GLOBAL_ONLY) {
	nsPtr = globalNsPtr;
    } else if (nsPtr == NULL) {
	nsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    }

    start = qualName;		/* pts to start of qualifying namespace */
    if ((*qualName == ':') && (*(qualName+1) == ':')) {
	start = qualName+2;	/* skip over the initial :: */
	while (*start == ':') {
            start++;		/* skip over a subsequent : */
	}
        nsPtr = globalNsPtr;
        if (*start == '\0') {	/* qualName is just two or more ":"s */
            *nsPtrPtr        = globalNsPtr;
            *altNsPtrPtr     = NULL;
	    *actualCxtPtrPtr = globalNsPtr;
            *simpleNamePtr   = start; /* points to empty string */
            return TCL_OK;
        }
    }
    *actualCxtPtrPtr = nsPtr;

    /*
     * Start an alternate search path starting with the global namespace.
     * However, if the starting context is the global namespace, or if the
     * flag is set to search only the namespace *cxtNsPtr, ignore the
     * alternate search path.
     */

    altNsPtr = globalNsPtr;
    if ((nsPtr == globalNsPtr)
	    || (flags & (TCL_NAMESPACE_ONLY | FIND_ONLY_NS))) {
        altNsPtr = NULL;
    }

    /*
     * Loop to resolve each namespace qualifier in qualName.
     */

    Tcl_DStringInit(&buffer);
    end = start;
    while (*start != '\0') {
        /*
         * Find the next namespace qualifier (i.e., a name ending in "::")
	 * or the end of the qualified name  (i.e., a name ending in "\0").
	 * Set len to the number of characters, starting from start,
	 * in the name; set end to point after the "::"s or at the "\0".
         */

	len = 0;
        for (end = start;  *end != '\0';  end++) {
	    if ((*end == ':') && (*(end+1) == ':')) {
		end += 2;	/* skip over the initial :: */
		while (*end == ':') {
		    end++;	/* skip over the subsequent : */
		}
		break;		/* exit for loop; end is after ::'s */
	    }
            len++;
	}

	if ((*end == '\0')
	        && !((len >= 2) && (*(end-1) == ':') && (*(end-2) == ':'))) {
	    /*
	     * qualName ended with a simple name at start. If FIND_ONLY_NS
	     * was specified, look this up as a namespace. Otherwise,
	     * start is the name of a cmd or var and we are done.
	     */
	    
	    if (flags & FIND_ONLY_NS) {
		nsName = start;
	    } else {
		*nsPtrPtr      = nsPtr;
		*altNsPtrPtr   = altNsPtr;
		*simpleNamePtr = start;
		Tcl_DStringFree(&buffer);
		return TCL_OK;
	    }
	} else {
	    /*
	     * start points to the beginning of a namespace qualifier ending
	     * in "::". end points to the start of a name in that namespace
	     * that might be empty. Copy the namespace qualifier to a
	     * buffer so it can be null terminated. We can't modify the
	     * incoming qualName since it may be a string constant.
	     */

	    Tcl_DStringSetLength(&buffer, 0);
            Tcl_DStringAppend(&buffer, start, len);
            nsName = Tcl_DStringValue(&buffer);
        }

        /*
	 * Look up the namespace qualifier nsName in the current namespace
         * context. If it isn't found but CREATE_NS_IF_UNKNOWN is set,
         * create that qualifying namespace. This is needed for procedures
         * like Tcl_CreateCommand that cannot fail.
	 */

        if (nsPtr != NULL) {
            entryPtr = Tcl_FindHashEntry(&nsPtr->childTable, nsName);
            if (entryPtr != NULL) {
                nsPtr = (Namespace *) Tcl_GetHashValue(entryPtr);
            } else if (flags & CREATE_NS_IF_UNKNOWN) {
		Tcl_CallFrame frame;
		
		result = Tcl_PushCallFrame(interp, &frame,
		        (Tcl_Namespace *) nsPtr, /*isProcCallFrame*/ 0);
                if (result != TCL_OK) {
                    Tcl_DStringFree(&buffer);
                    return result;
                }
                nsPtr = (Namespace *) Tcl_CreateNamespace(interp, nsName,
		        (ClientData) NULL, (Tcl_NamespaceDeleteProc *) NULL);
                Tcl_PopCallFrame(interp);
                if (nsPtr == NULL) {
                    Tcl_DStringFree(&buffer);
                    return TCL_ERROR;
                }
            } else {		/* namespace not found and wasn't created */
                nsPtr = NULL;
            }
        }

        /*
         * Look up the namespace qualifier in the alternate search path too.
         */

        if (altNsPtr != NULL) {
            entryPtr = Tcl_FindHashEntry(&altNsPtr->childTable, nsName);
            if (entryPtr != NULL) {
                altNsPtr = (Namespace *) Tcl_GetHashValue(entryPtr);
            } else {
                altNsPtr = NULL;
            }
        }

        /*
         * If both search paths have failed, return NULL results.
         */

        if ((nsPtr == NULL) && (altNsPtr == NULL)) {
            *nsPtrPtr      = NULL;
            *altNsPtrPtr   = NULL;
            *simpleNamePtr = NULL;
            Tcl_DStringFree(&buffer);
            return TCL_OK;
        }

	start = end;
    }

    /*
     * We ignore trailing "::"s in a namespace name, but in a command or
     * variable name, trailing "::"s refer to the cmd or var named {}.
     */

    if ((flags & FIND_ONLY_NS)
	    || ((end > start ) && (*(end-1) != ':'))) {
	*simpleNamePtr = NULL; /* found namespace name */
    } else {
	*simpleNamePtr = end;  /* found cmd/var: points to empty string */
    }

    /*
     * As a special case, if we are looking for a namespace and qualName
     * is "" and the current active namespace (nsPtr) is not the global
     * namespace, return NULL (no namespace was found). This is because
     * namespaces can not have empty names except for the global namespace.
     */

    if ((flags & FIND_ONLY_NS) && (*qualName == '\0')
	    && (nsPtr != globalNsPtr)) {
	nsPtr = NULL;
    }
    
    *nsPtrPtr    = nsPtr;
    *altNsPtrPtr = altNsPtr;
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindNamespace --
 *
 *	Searches for a namespace.
 *
 * Results:
 *	Returns a pointer to the namespace if it is found. Otherwise,
 *	returns NULL and leaves an error message in the interpreter's
 *	result object if "flags" contains TCL_LEAVE_ERR_MSG.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Namespace *
Tcl_FindNamespace(interp, name, contextNsPtr, flags)
    Tcl_Interp *interp;		 /* The interpreter in which to find the
				  * namespace. */
    char *name;			 /* Namespace name. If it starts with "::",
				  * will be looked up in global namespace.
				  * Else, looked up first in contextNsPtr
				  * (current namespace if contextNsPtr is
				  * NULL), then in global namespace. */
    Tcl_Namespace *contextNsPtr; /* Ignored if TCL_GLOBAL_ONLY flag is set
				  * or if the name starts with "::".
				  * Otherwise, points to namespace in which
				  * to resolve name; if NULL, look up name
				  * in the current namespace. */
    register int flags;		 /* Flags controlling namespace lookup: an
				  * OR'd combination of TCL_GLOBAL_ONLY and
				  * TCL_LEAVE_ERR_MSG flags. */
{
    Namespace *nsPtr, *dummy1Ptr, *dummy2Ptr;
    char *dummy;
    int result;

    /*
     * Find the namespace(s) that contain the specified namespace name.
     * Add the FIND_ONLY_NS flag to resolve the name all the way down
     * to its last component, a namespace.
     */

    result = TclGetNamespaceForQualName(interp, name,
	    (Namespace *) contextNsPtr, /*flags*/ (flags | FIND_ONLY_NS),
	    &nsPtr, &dummy1Ptr, &dummy2Ptr, &dummy);
    if (result != TCL_OK) {
        return NULL;
    }
    if (nsPtr != NULL) {
       return (Tcl_Namespace *) nsPtr;
    } else if (flags & TCL_LEAVE_ERR_MSG) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "unknown namespace \"", name, "\"", (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindCommand --
 *
 *	Searches for a command.
 *
 * Results:
 *	Returns a token for the command if it is found. Otherwise, if it
 *	can't be found or there is an error, returns NULL and leaves an
 *	error message in the interpreter's result object if "flags"
 *	contains TCL_LEAVE_ERR_MSG.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
Tcl_FindCommand(interp, name, contextNsPtr, flags)
    Tcl_Interp *interp;         /* The interpreter in which to find the
				  * command and to report errors. */
    char *name;		         /* Command's name. If it starts with "::",
				  * will be looked up in global namespace.
				  * Else, looked up first in contextNsPtr
				  * (current namespace if contextNsPtr is
				  * NULL), then in global namespace. */
    Tcl_Namespace *contextNsPtr; /* Ignored if TCL_GLOBAL_ONLY flag set.
				  * Otherwise, points to namespace in which
				  * to resolve name. If NULL, look up name
				  * in the current namespace. */
    int flags;                   /* An OR'd combination of flags:
				  * TCL_GLOBAL_ONLY (look up name only in
				  * global namespace), TCL_NAMESPACE_ONLY
				  * (look up only in contextNsPtr, or the
				  * current namespace if contextNsPtr is
				  * NULL), and TCL_LEAVE_ERR_MSG. If both
				  * TCL_GLOBAL_ONLY and TCL_NAMESPACE_ONLY
				  * are given, TCL_GLOBAL_ONLY is
				  * ignored. */
{
    Namespace *nsPtr[2], *cxtNsPtr;
    char *simpleName;
    register Tcl_HashEntry *entryPtr;
    register Command *cmdPtr;
    register int search;
    int result;

    /*
     * Find the namespace(s) that contain the command.
     */

    result = TclGetNamespaceForQualName(interp, name,
	    (Namespace *) contextNsPtr, flags, &nsPtr[0], &nsPtr[1],
	    &cxtNsPtr, &simpleName);
    if (result != TCL_OK) {
        return (Tcl_Command) NULL;
    }

    /*
     * Look for the command in the command table of its namespace.
     * Be sure to check both possible search paths: from the specified
     * namespace context and from the global namespace.
     */

    cmdPtr = NULL;
    for (search = 0;  (search < 2) && (cmdPtr == NULL);  search++) {
        if ((nsPtr[search] != NULL) && (simpleName != NULL)) {
	    entryPtr = Tcl_FindHashEntry(&nsPtr[search]->cmdTable,
		    simpleName);
            if (entryPtr != NULL) {
                cmdPtr = (Command *) Tcl_GetHashValue(entryPtr);
            }
        }
    }
    if (cmdPtr != NULL) {
        return (Tcl_Command) cmdPtr;
    } else if (flags & TCL_LEAVE_ERR_MSG) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "unknown command \"", name, "\"", (char *) NULL);
    }
    return (Tcl_Command) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindNamespaceVar --
 *
 *	Searches for a namespace variable, a variable not local to a
 *	procedure. The variable can be either a scalar or an array, but
 *	may not be an element of an array.
 *
 * Results:
 *	Returns a token for the variable if it is found. Otherwise, if it
 *	can't be found or there is an error, returns NULL and leaves an
 *	error message in the interpreter's result object if "flags"
 *	contains TCL_LEAVE_ERR_MSG.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Var
Tcl_FindNamespaceVar(interp, name, contextNsPtr, flags)
    Tcl_Interp *interp;		 /* The interpreter in which to find the
				  * variable. */
    char *name;			 /* Variable's name. If it starts with "::",
				  * will be looked up in global namespace.
				  * Else, looked up first in contextNsPtr
				  * (current namespace if contextNsPtr is
				  * NULL), then in global namespace. */
    Tcl_Namespace *contextNsPtr; /* Ignored if TCL_GLOBAL_ONLY flag set.
				  * Otherwise, points to namespace in which
				  * to resolve name. If NULL, look up name
				  * in the current namespace. */
    int flags;			 /* An OR'd combination of flags:
				  * TCL_GLOBAL_ONLY (look up name only in
				  * global namespace), TCL_NAMESPACE_ONLY
				  * (look up only in contextNsPtr, or the
				  * current namespace if contextNsPtr is
				  * NULL), and TCL_LEAVE_ERR_MSG. If both
				  * TCL_GLOBAL_ONLY and TCL_NAMESPACE_ONLY
				  * are given, TCL_GLOBAL_ONLY is
				  * ignored. */
{
    Namespace *nsPtr[2], *cxtNsPtr;
    char *simpleName;
    Tcl_HashEntry *entryPtr;
    Var *varPtr;
    register int search;
    int result;

    /*
     * Find the namespace(s) that contain the variable.
     */

    result = TclGetNamespaceForQualName(interp, name,
	    (Namespace *) contextNsPtr, flags, &nsPtr[0], &nsPtr[1],
	    &cxtNsPtr, &simpleName);
    if (result != TCL_OK) {
        return (Tcl_Var) NULL;
    }

    /*
     * Look for the variable in the variable table of its namespace.
     * Be sure to check both possible search paths: from the specified
     * namespace context and from the global namespace.
     */

    varPtr = NULL;
    for (search = 0;  (search < 2) && (varPtr == NULL);  search++) {
        if ((nsPtr[search] != NULL) && (simpleName != NULL)) {
            entryPtr = Tcl_FindHashEntry(&nsPtr[search]->varTable,
		    simpleName);
            if (entryPtr != NULL) {
                varPtr = (Var *) Tcl_GetHashValue(entryPtr);
            }
        }
    }
    if (varPtr != NULL) {
	return (Tcl_Var) varPtr;
    } else if (flags & TCL_LEAVE_ERR_MSG) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "unknown variable \"", name, "\"", (char *) NULL);
    }
    return (Tcl_Var) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclResetShadowedCmdRefs --
 *
 *	Called when a command is added to a namespace to check for existing
 *	command references that the new command may invalidate. Consider the
 *	following cases that could happen when you add a command "foo" to a
 *	namespace "b":
 *	   1. It could shadow a command named "foo" at the global scope.
 *	      If it does, all command references in the namespace "b" are
 *	      suspect.
 *	   2. Suppose the namespace "b" resides in a namespace "a".
 *	      Then to "a" the new command "b::foo" could shadow another
 *	      command "b::foo" in the global namespace. If so, then all
 *	      command references in "a" are suspect.
 *	The same checks are applied to all parent namespaces, until we
 *	reach the global :: namespace.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the new command shadows an existing command, the cmdRefEpoch
 *	counter is incremented in each namespace that sees the shadow.
 *	This invalidates all command references that were previously cached
 *	in that namespace. The next time the commands are used, they are
 *	resolved from scratch.
 *
 *----------------------------------------------------------------------
 */

void
TclResetShadowedCmdRefs(interp, newCmdPtr)
    Tcl_Interp *interp;	       /* Interpreter containing the new command. */
    Command *newCmdPtr;	       /* Points to the new command. */
{
    char *cmdName;
    Tcl_HashEntry *hPtr;
    register Namespace *nsPtr;
    Namespace *trailNsPtr, *shadowNsPtr;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    int found, i;

    /*
     * This procedure generates an array used to hold the trail list. This
     * starts out with stack-allocated space but uses dynamically-allocated
     * storage if needed.
     */

#define NUM_TRAIL_ELEMS 5
    Namespace *(trailStorage[NUM_TRAIL_ELEMS]);
    Namespace **trailPtr = trailStorage;
    int trailFront = -1;
    int trailSize = NUM_TRAIL_ELEMS;

    /*
     * Start at the namespace containing the new command, and work up
     * through the list of parents. Stop just before the global namespace,
     * since the global namespace can't "shadow" its own entries.
     *
     * The namespace "trail" list we build consists of the names of each
     * namespace that encloses the new command, in order from outermost to
     * innermost: for example, "a" then "b". Each iteration of this loop
     * eventually extends the trail upwards by one namespace, nsPtr. We use
     * this trail list to see if nsPtr (e.g. "a" in 2. above) could have
     * now-invalid cached command references. This will happen if nsPtr
     * (e.g. "a") contains a sequence of child namespaces (e.g. "b")
     * such that there is a identically-named sequence of child namespaces
     * starting from :: (e.g. "::b") whose tail namespace contains a command
     * also named cmdName.
     */

    cmdName = Tcl_GetHashKey(newCmdPtr->hPtr->tablePtr, newCmdPtr->hPtr);
    for (nsPtr = newCmdPtr->nsPtr;
	    (nsPtr != NULL) && (nsPtr != globalNsPtr);
            nsPtr = nsPtr->parentPtr) {
        /*
	 * Find the maximal sequence of child namespaces contained in nsPtr
	 * such that there is a identically-named sequence of child
	 * namespaces starting from ::. shadowNsPtr will be the tail of this
	 * sequence, or the deepest namespace under :: that might contain a
	 * command now shadowed by cmdName. We check below if shadowNsPtr
	 * actually contains a command cmdName.
	 */

        found = 1;
        shadowNsPtr = globalNsPtr;

        for (i = trailFront;  i >= 0;  i--) {
            trailNsPtr = trailPtr[i];
            hPtr = Tcl_FindHashEntry(&shadowNsPtr->childTable,
		    trailNsPtr->name);
            if (hPtr != NULL) {
                shadowNsPtr = (Namespace *) Tcl_GetHashValue(hPtr);
            } else {
                found = 0;
                break;
            }
        }

        /*
	 * If shadowNsPtr contains a command named cmdName, we invalidate
         * all of the command refs cached in nsPtr. As a boundary case,
	 * shadowNsPtr is initially :: and we check for case 1. above.
	 */

        if (found) {
            hPtr = Tcl_FindHashEntry(&shadowNsPtr->cmdTable, cmdName);
            if (hPtr != NULL) {
                nsPtr->cmdRefEpoch++;
            }
        }

        /*
	 * Insert nsPtr at the front of the trail list: i.e., at the end
	 * of the trailPtr array.
	 */

	trailFront++;
	if (trailFront == trailSize) {
	    size_t currBytes = trailSize * sizeof(Namespace *);
	    int newSize = 2*trailSize;
	    size_t newBytes = newSize * sizeof(Namespace *);
	    Namespace **newPtr =
		    (Namespace **) ckalloc((unsigned) newBytes);
	    
	    memcpy((VOID *) newPtr, (VOID *) trailPtr, currBytes);
	    if (trailPtr != trailStorage) {
		ckfree((char *) trailPtr);
	    }
	    trailPtr = newPtr;
	    trailSize = newSize;
	}
	trailPtr[trailFront] = nsPtr;
    }

    /*
     * Free any allocated storage.
     */
    
    if (trailPtr != trailStorage) {
	ckfree((char *) trailPtr);
    }
#undef NUM_TRAIL_ELEMS
}

/*
 *----------------------------------------------------------------------
 *
 * GetNamespaceFromObj --
 *
 *	Returns the namespace specified by the name in a Tcl_Obj.
 *
 * Results:
 *	Returns TCL_OK if the namespace was resolved successfully, and
 *	stores a pointer to the namespace in the location specified by
 *	nsPtrPtr. If the namespace can't be found, the procedure stores
 *	NULL in *nsPtrPtr and returns TCL_OK. If anything else goes wrong,
 *	this procedure returns TCL_ERROR.
 *
 * Side effects:
 *	May update the internal representation for the object, caching the
 *	namespace reference. The next time this procedure is called, the
 *	namespace value can be found quickly.
 *
 *	If anything goes wrong, an error message is left in the
 *	interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

static int
GetNamespaceFromObj(interp, objPtr, nsPtrPtr)
    Tcl_Interp *interp;		/* The current interpreter. */
    Tcl_Obj *objPtr;		/* The object to be resolved as the name
				 * of a namespace. */
    Tcl_Namespace **nsPtrPtr;	/* Result namespace pointer goes here. */
{
    register ResolvedNsName *resNamePtr;
    register Namespace *nsPtr;
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    int result;

    /*
     * Get the internal representation, converting to a namespace type if
     * needed. The internal representation is a ResolvedNsName that points
     * to the actual namespace.
     */

    if (objPtr->typePtr != &tclNsNameType) {
        result = tclNsNameType.setFromAnyProc(interp, objPtr);
        if (result != TCL_OK) {
            return TCL_ERROR;
        }
    }
    resNamePtr = (ResolvedNsName *) objPtr->internalRep.otherValuePtr;

    /*
     * Check the context namespace of the resolved symbol to make sure that
     * it is fresh. If not, then force another conversion to the namespace
     * type, to discard the old rep and create a new one. Note that we
     * verify that the namespace id of the cached namespace is the same as
     * the id when we cached it; this insures that the namespace wasn't
     * deleted and a new one created at the same address.
     */

    nsPtr = NULL;
    if ((resNamePtr != NULL)
	    && (resNamePtr->refNsPtr == currNsPtr)
	    && (resNamePtr->nsId == resNamePtr->nsPtr->nsId)) {
        nsPtr = resNamePtr->nsPtr;
	if (nsPtr->flags & NS_DEAD) {
	    nsPtr = NULL;
	}
    }
    if (nsPtr == NULL) {	/* try again */
        result = tclNsNameType.setFromAnyProc(interp, objPtr);
        if (result != TCL_OK) {
            return TCL_ERROR;
        }
        resNamePtr = (ResolvedNsName *) objPtr->internalRep.otherValuePtr;
        if (resNamePtr != NULL) {
            nsPtr = resNamePtr->nsPtr;
            if (nsPtr->flags & NS_DEAD) {
                nsPtr = NULL;
            }
        }
    }
    *nsPtrPtr = (Tcl_Namespace *) nsPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NamespaceObjCmd --
 *
 *	Invoked to implement the "namespace" command that creates, deletes,
 *	or manipulates Tcl namespaces. Handles the following syntax:
 *
 *	    namespace children ?name? ?pattern?
 *	    namespace code arg
 *	    namespace current
 *	    namespace delete ?name name...?
 *	    namespace eval name arg ?arg...?
 *	    namespace export ?-clear? ?pattern pattern...?
 *	    namespace forget ?pattern pattern...?
 *	    namespace import ?-force? ?pattern pattern...?
 *	    namespace inscope name arg ?arg...?
 *	    namespace origin name
 *	    namespace parent ?name?
 *	    namespace qualifiers string
 *	    namespace tail string
 *	    namespace which ?-command? ?-variable? name
 *
 * Results:
 *	Returns TCL_OK if the command is successful. Returns TCL_ERROR if
 *	anything goes wrong.
 *
 * Side effects:
 *	Based on the subcommand name (e.g., "import"), this procedure
 *	dispatches to a corresponding procedure NamespaceXXXCmd defined
 *	statically in this file. This procedure's side effects depend on
 *	whatever that subcommand procedure does. If there is an error, this
 *	procedure returns an error message in the interpreter's result
 *	object. Otherwise it may return a result in the interpreter's result
 *	object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_NamespaceObjCmd(clientData, interp, objc, objv)
    ClientData clientData;		/* Arbitrary value passed to cmd. */
    Tcl_Interp *interp;			/* Current interpreter. */
    register int objc;			/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    static char *subCmds[] = {
            "children", "code", "current", "delete",
	    "eval", "export", "forget", "import",
	    "inscope", "origin", "parent", "qualifiers",
	    "tail", "which", (char *) NULL};
    enum NSSubCmdIdx {
	    NSChildrenIdx, NSCodeIdx, NSCurrentIdx, NSDeleteIdx,
	    NSEvalIdx, NSExportIdx, NSForgetIdx, NSImportIdx,
	    NSInscopeIdx, NSOriginIdx, NSParentIdx, NSQualifiersIdx,
	    NSTailIdx, NSWhichIdx
    } index;
    int result;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
        return TCL_ERROR;
    }

    /*
     * Return an index reflecting the particular subcommand.
     */

    result = Tcl_GetIndexFromObj((Tcl_Interp *) NULL, objv[1], subCmds,
	    "subcommand", /*flags*/ 0, (int *) &index);
    if (result != TCL_OK) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad namespace subcommand \"",
		Tcl_GetStringFromObj(objv[1], (int *) NULL),
		"\": should be children, code, current, delete, eval, export, forget, import, inscope, origin, parent, qualifiers, tail, or which",
		(char *) NULL);
	return result;
    }
    
    switch (index) {
        case NSChildrenIdx:
	    result = NamespaceChildrenCmd(clientData, interp, objc, objv);
            break;
        case NSCodeIdx:
	    result = NamespaceCodeCmd(clientData, interp, objc, objv);
            break;
        case NSCurrentIdx:
	    result = NamespaceCurrentCmd(clientData, interp, objc, objv);
            break;
        case NSDeleteIdx:
	    result = NamespaceDeleteCmd(clientData, interp, objc, objv);
            break;
        case NSEvalIdx:
	    result = NamespaceEvalCmd(clientData, interp, objc, objv);
            break;
        case NSExportIdx:
	    result = NamespaceExportCmd(clientData, interp, objc, objv);
            break;
        case NSForgetIdx:
	    result = NamespaceForgetCmd(clientData, interp, objc, objv);
            break;
        case NSImportIdx:
	    result = NamespaceImportCmd(clientData, interp, objc, objv);
            break;
        case NSInscopeIdx:
	    result = NamespaceInscopeCmd(clientData, interp, objc, objv);
            break;
        case NSOriginIdx:
	    result = NamespaceOriginCmd(clientData, interp, objc, objv);
            break;
        case NSParentIdx:
	    result = NamespaceParentCmd(clientData, interp, objc, objv);
            break;
        case NSQualifiersIdx:
	    result = NamespaceQualifiersCmd(clientData, interp, objc, objv);
            break;
        case NSTailIdx:
	    result = NamespaceTailCmd(clientData, interp, objc, objv);
            break;
        case NSWhichIdx:
	    result = NamespaceWhichCmd(clientData, interp, objc, objv);
            break;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceChildrenCmd --
 *
 *	Invoked to implement the "namespace children" command that returns a
 *	list containing the fully-qualified names of the child namespaces of
 *	a given namespace. Handles the following syntax:
 *
 *	    namespace children ?name? ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceChildrenCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Namespace *namespacePtr;
    Namespace *nsPtr, *childNsPtr;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    char *pattern = NULL;
    Tcl_DString buffer;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tcl_Obj *listPtr, *elemPtr;

    /*
     * Get a pointer to the specified namespace, or the current namespace.
     */

    if (objc == 2) {
	nsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    } else if ((objc == 3) || (objc == 4)) {
        if (GetNamespaceFromObj(interp, objv[2], &namespacePtr) != TCL_OK) {
            return TCL_ERROR;
        }
        if (namespacePtr == NULL) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "unknown namespace \"",
		    Tcl_GetStringFromObj(objv[2], (int *) NULL),
		    "\" in namespace children command", (char *) NULL);
            return TCL_ERROR;
        }
        nsPtr = (Namespace *) namespacePtr;
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "children ?name? ?pattern?");
        return TCL_ERROR;
    }

    /*
     * Get the glob-style pattern, if any, used to narrow the search.
     */

    Tcl_DStringInit(&buffer);
    if (objc == 4) {
        char *name = Tcl_GetStringFromObj(objv[3], (int *) NULL);
	
        if ((*name == ':') && (*(name+1) == ':')) {
            pattern = name;
        } else {
            Tcl_DStringAppend(&buffer, nsPtr->fullName, -1);
            if (nsPtr != globalNsPtr) {
                Tcl_DStringAppend(&buffer, "::", 2);
            }
            Tcl_DStringAppend(&buffer, name, -1);
            pattern = Tcl_DStringValue(&buffer);
        }
    }

    /*
     * Create a list containing the full names of all child namespaces
     * whose names match the specified pattern, if any.
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    entryPtr = Tcl_FirstHashEntry(&nsPtr->childTable, &search);
    while (entryPtr != NULL) {
        childNsPtr = (Namespace *) Tcl_GetHashValue(entryPtr);
        if ((pattern == NULL)
	        || Tcl_StringMatch(childNsPtr->fullName, pattern)) {
            elemPtr = Tcl_NewStringObj(childNsPtr->fullName, -1);
            Tcl_ListObjAppendElement(interp, listPtr, elemPtr);
        }
        entryPtr = Tcl_NextHashEntry(&search);
    }

    Tcl_SetObjResult(interp, listPtr);
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceCodeCmd --
 *
 *	Invoked to implement the "namespace code" command to capture the
 *	namespace context of a command. Handles the following syntax:
 *
 *	    namespace code arg
 *
 *	Here "arg" can be a list. "namespace code arg" produces a result
 *	equivalent to that produced by the command
 *
 *	    list namespace inscope [namespace current] $arg
 *
 *	However, if "arg" is itself a scoped value starting with
 *	"namespace inscope", then the result is just "arg".
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	If anything goes wrong, this procedure returns an error
 *	message as the result in the interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceCodeCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Namespace *currNsPtr;
    Tcl_Obj *listPtr, *objPtr;
    register char *arg, *p;
    int length;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "code arg");
        return TCL_ERROR;
    }

    /*
     * If "arg" is already a scoped value, then return it directly.
     */

    arg = Tcl_GetStringFromObj(objv[2], &length);
    if ((*arg == 'n') && (length > 17)
	    && (strncmp(arg, "namespace", 9) == 0)) {
	for (p = (arg + 9);  (*p == ' ');  p++) {
	    /* empty body: skip over spaces */
	}
	if ((*p == 'i') && ((p + 7) <= (arg + length))
	        && (strncmp(p, "inscope", 7) == 0)) {
	    Tcl_SetObjResult(interp, objv[2]);
	    return TCL_OK;
	}
    }

    /*
     * Otherwise, construct a scoped command by building a list with
     * "namespace inscope", the full name of the current namespace, and 
     * the argument "arg". By constructing a list, we ensure that scoped
     * commands are interpreted properly when they are executed later,
     * by the "namespace inscope" command.
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    Tcl_ListObjAppendElement(interp, listPtr,
            Tcl_NewStringObj("namespace", -1));
    Tcl_ListObjAppendElement(interp, listPtr,
	    Tcl_NewStringObj("inscope", -1));

    currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    if (currNsPtr == (Namespace *) Tcl_GetGlobalNamespace(interp)) {
	objPtr = Tcl_NewStringObj("::", -1);
    } else {
	objPtr = Tcl_NewStringObj(currNsPtr->fullName, -1);
    }
    Tcl_ListObjAppendElement(interp, listPtr, objPtr);
    
    Tcl_ListObjAppendElement(interp, listPtr, objv[2]);

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceCurrentCmd --
 *
 *	Invoked to implement the "namespace current" command which returns
 *	the fully-qualified name of the current namespace. Handles the
 *	following syntax:
 *
 *	    namespace current
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceCurrentCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Namespace *currNsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "current");
        return TCL_ERROR;
    }

    /*
     * The "real" name of the global namespace ("::") is the null string,
     * but we return "::" for it as a convenience to programmers. Note that
     * "" and "::" are treated as synonyms by the namespace code so that it
     * is still easy to do things like:
     *
     *    namespace [namespace current]::bar { ... }
     */

    currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    if (currNsPtr == (Namespace *) Tcl_GetGlobalNamespace(interp)) {
        Tcl_AppendToObj(Tcl_GetObjResult(interp), "::", -1);
    } else {
	Tcl_AppendToObj(Tcl_GetObjResult(interp), currNsPtr->fullName, -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceDeleteCmd --
 *
 *	Invoked to implement the "namespace delete" command to delete
 *	namespace(s). Handles the following syntax:
 *
 *	    namespace delete ?name name...?
 *
 *	Each name identifies a namespace. It may include a sequence of
 *	namespace qualifiers separated by "::"s. If a namespace is found, it
 *	is deleted: all variables and procedures contained in that namespace
 *	are deleted. If that namespace is being used on the call stack, it
 *	is kept alive (but logically deleted) until it is removed from the
 *	call stack: that is, it can no longer be referenced by name but any
 *	currently executing procedure that refers to it is allowed to do so
 *	until the procedure returns. If the namespace can't be found, this
 *	procedure returns an error. If no namespaces are specified, this
 *	command does nothing.
 *
 * Results:
 *	Returns TCL_OK if successful, and  TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Deletes the specified namespaces. If anything goes wrong, this
 *	procedure returns an error message in the interpreter's
 *	result object.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceDeleteCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Namespace *namespacePtr;
    char *name;
    register int i;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "delete ?name name...?");
        return TCL_ERROR;
    }

    /*
     * Destroying one namespace may cause another to be destroyed. Break
     * this into two passes: first check to make sure that all namespaces on
     * the command line are valid, and report any errors.
     */

    for (i = 2;  i < objc;  i++) {
        name = Tcl_GetStringFromObj(objv[i], (int *) NULL);
	namespacePtr = Tcl_FindNamespace(interp, name,
		(Tcl_Namespace *) NULL, /*flags*/ 0);
        if (namespacePtr == NULL) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "unknown namespace \"",
		    Tcl_GetStringFromObj(objv[i], (int *) NULL),
		    "\" in namespace delete command", (char *) NULL);
            return TCL_ERROR;
        }
    }

    /*
     * Okay, now delete each namespace.
     */

    for (i = 2;  i < objc;  i++) {
        name = Tcl_GetStringFromObj(objv[i], (int *) NULL);
	namespacePtr = Tcl_FindNamespace(interp, name,
	    (Tcl_Namespace *) NULL, TCL_LEAVE_ERR_MSG);
	if (namespacePtr == NULL) {
            return TCL_ERROR;
        }
        Tcl_DeleteNamespace(namespacePtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceEvalCmd --
 *
 *	Invoked to implement the "namespace eval" command. Executes
 *	commands in a namespace. If the namespace does not already exist,
 *	it is created. Handles the following syntax:
 *
 *	    namespace eval name arg ?arg...?
 *
 *	If more than one arg argument is specified, the command that is
 *	executed is the result of concatenating the arguments together with
 *	a space between each argument.
 *
 * Results:
 *	Returns TCL_OK if the namespace is found and the commands are
 *	executed successfully. Returns TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns the result of the command in the interpreter's result
 *	object. If anything goes wrong, this procedure returns an error
 *	message as the result.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceEvalCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Namespace *namespacePtr;
    Tcl_CallFrame frame;
    Tcl_Obj *objPtr;
    char *name;
    int length, result;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "eval name arg ?arg...?");
        return TCL_ERROR;
    }

    /*
     * Try to resolve the namespace reference, caching the result in the
     * namespace object along the way.
     */

    result = GetNamespaceFromObj(interp, objv[2], &namespacePtr);
    if (result != TCL_OK) {
        return result;
    }

    /*
     * If the namespace wasn't found, try to create it.
     */
    
    if (namespacePtr == NULL) {
	name = Tcl_GetStringFromObj(objv[2], &length);
	namespacePtr = Tcl_CreateNamespace(interp, name, (ClientData) NULL, 
                (Tcl_NamespaceDeleteProc *) NULL);
	if (namespacePtr == NULL) {
	    return TCL_ERROR;
	}
    }

    /*
     * Make the specified namespace the current namespace and evaluate
     * the command(s).
     */

    result = Tcl_PushCallFrame(interp, &frame, namespacePtr,
	    /*isProcCallFrame*/ 0);
    if (result != TCL_OK) {
        return TCL_ERROR;
    }

    if (objc == 4) {
        result = Tcl_EvalObj(interp, objv[3]);
    } else {
        objPtr = Tcl_ConcatObj(objc-3, objv+3);
        result = Tcl_EvalObj(interp, objPtr);
        Tcl_DecrRefCount(objPtr);  /* we're done with the object */
    }
    if (result == TCL_ERROR) {
        char msg[256];
	
        sprintf(msg, "\n    (in namespace eval \"%.200s\" script line %d)",
            namespacePtr->fullName, interp->errorLine);
        Tcl_AddObjErrorInfo(interp, msg, -1);
    }

    /*
     * Restore the previous "current" namespace.
     */
    
    Tcl_PopCallFrame(interp);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceExportCmd --
 *
 *	Invoked to implement the "namespace export" command that specifies
 *	which commands are exported from a namespace. The exported commands
 *	are those that can be imported into another namespace using
 *	"namespace import". Both commands defined in a namespace and
 *	commands the namespace has imported can be exported by a
 *	namespace. This command has the following syntax:
 *
 *	    namespace export ?-clear? ?pattern pattern...?
 *
 *	Each pattern may contain "string match"-style pattern matching
 *	special characters, but the pattern may not include any namespace
 *	qualifiers: that is, the pattern must specify commands in the
 *	current (exporting) namespace. The specified patterns are appended
 *	onto the namespace's list of export patterns.
 *
 *	To reset the namespace's export pattern list, specify the "-clear"
 *	flag.
 *
 *	If there are no export patterns and the "-clear" flag isn't given,
 *	this command returns the namespace's current export list.
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceExportCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Namespace *currNsPtr = (Namespace*) Tcl_GetCurrentNamespace(interp);
    char *pattern, *string;
    int resetListFirst = 0;
    int firstArg, patternCt, i, result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv,
	        "export ?-clear? ?pattern pattern...?");
        return TCL_ERROR;
    }

    /*
     * Process the optional "-clear" argument.
     */

    firstArg = 2;
    if (firstArg < objc) {
	string = Tcl_GetStringFromObj(objv[firstArg], (int *) NULL);
	if (strcmp(string, "-clear") == 0) {
	    resetListFirst = 1;
	    firstArg++;
	}
    }

    /*
     * If no pattern arguments are given, and "-clear" isn't specified,
     * return the namespace's current export pattern list.
     */

    patternCt = (objc - firstArg);
    if (patternCt == 0) {
	if (firstArg > 2) {
	    return TCL_OK;
	} else {		/* create list with export patterns */
	    Tcl_Obj *listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	    result = Tcl_AppendExportList(interp,
		    (Tcl_Namespace *) currNsPtr, listPtr);
	    if (result != TCL_OK) {
		return result;
	    }
	    Tcl_SetObjResult(interp, listPtr);
	    return TCL_OK;
	}
    }

    /*
     * Add each pattern to the namespace's export pattern list.
     */
    
    for (i = firstArg;  i < objc;  i++) {
	pattern = Tcl_GetStringFromObj(objv[i], (int *) NULL);
	result = Tcl_Export(interp, (Tcl_Namespace *) currNsPtr, pattern,
		((i == firstArg)? resetListFirst : 0));
        if (result != TCL_OK) {
            return result;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceForgetCmd --
 *
 *	Invoked to implement the "namespace forget" command to remove
 *	imported commands from a namespace. Handles the following syntax:
 *
 *	    namespace forget ?pattern pattern...?
 *
 *	Each pattern is a name like "foo::*" or "a::b::x*". That is, the
 *	pattern may include the special pattern matching characters
 *	recognized by the "string match" command, but only in the command
 *	name at the end of the qualified name; the special pattern
 *	characters may not appear in a namespace name. All of the commands
 *	that match that pattern are checked to see if they have an imported
 *	command in the current namespace that refers to the matched
 *	command. If there is an alias, it is removed.
 *	
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Imported commands are removed from the current namespace. If
 *	anything goes wrong, this procedure returns an error message in the
 *	interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceForgetCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *pattern;
    register int i, result;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "forget ?pattern pattern...?");
        return TCL_ERROR;
    }

    for (i = 2;  i < objc;  i++) {
        pattern = Tcl_GetStringFromObj(objv[i], (int *) NULL);
	result = Tcl_ForgetImport(interp, (Tcl_Namespace *) NULL, pattern);
        if (result != TCL_OK) {
            return result;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceImportCmd --
 *
 *	Invoked to implement the "namespace import" command that imports
 *	commands into a namespace. Handles the following syntax:
 *
 *	    namespace import ?-force? ?pattern pattern...?
 *
 *	Each pattern is a namespace-qualified name like "foo::*",
 *	"a::b::x*", or "bar::p". That is, the pattern may include the
 *	special pattern matching characters recognized by the "string match"
 *	command, but only in the command name at the end of the qualified
 *	name; the special pattern characters may not appear in a namespace
 *	name. All of the commands that match the pattern and which are
 *	exported from their namespace are made accessible from the current
 *	namespace context. This is done by creating a new "imported command"
 *	in the current namespace that points to the real command in its
 *	original namespace; when the imported command is called, it invokes
 *	the real command.
 *
 *	If an imported command conflicts with an existing command, it is
 *	treated as an error. But if the "-force" option is included, then
 *	existing commands are overwritten by the imported commands.
 *	
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Adds imported commands to the current namespace. If anything goes
 *	wrong, this procedure returns an error message in the interpreter's
 *	result object.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceImportCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int allowOverwrite = 0;
    char *string, *pattern;
    register int i, result;
    int firstArg;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv,
	        "import ?-force? ?pattern pattern...?");
        return TCL_ERROR;
    }

    /*
     * Skip over the optional "-force" as the first argument.
     */

    firstArg = 2;
    if (firstArg < objc) {
	string = Tcl_GetStringFromObj(objv[firstArg], (int *) NULL);
	if ((*string == '-') && (strcmp(string, "-force") == 0)) {
	    allowOverwrite = 1;
	    firstArg++;
	}
    }

    /*
     * Handle the imports for each of the patterns.
     */

    for (i = firstArg;  i < objc;  i++) {
        pattern = Tcl_GetStringFromObj(objv[i], (int *) NULL);
	result = Tcl_Import(interp, (Tcl_Namespace *) NULL, pattern,
	        allowOverwrite);
        if (result != TCL_OK) {
            return result;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceInscopeCmd --
 *
 *	Invoked to implement the "namespace inscope" command that executes a
 *	script in the context of a particular namespace. This command is not
 *	expected to be used directly by programmers; calls to it are
 *	generated implicitly when programs use "namespace code" commands
 *	to register callback scripts. Handles the following syntax:
 *
 *	    namespace inscope name arg ?arg...?
 *
 *	The "namespace inscope" command is much like the "namespace eval"
 *	command except that it has lappend semantics and the namespace must
 *	already exist. It treats the first argument as a list, and appends
 *	any arguments after the first onto the end as proper list elements.
 *	For example,
 *
 *	    namespace inscope ::foo a b c d
 *
 *	is equivalent to
 *
 *	    namespace eval ::foo [concat a [list b c d]]
 *
 *	This lappend semantics is important because many callback scripts
 *	are actually prefixes.
 *
 * Results:
 *	Returns TCL_OK to indicate success, or TCL_ERROR to indicate
 *	failure.
 *
 * Side effects:
 *	Returns a result in the Tcl interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceInscopeCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Namespace *namespacePtr;
    Tcl_CallFrame frame;
    int i, result;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "inscope name arg ?arg...?");
        return TCL_ERROR;
    }

    /*
     * Resolve the namespace reference.
     */

    result = GetNamespaceFromObj(interp, objv[2], &namespacePtr);
    if (result != TCL_OK) {
        return result;
    }
    if (namespacePtr == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
	        "unknown namespace \"",
		Tcl_GetStringFromObj(objv[2], (int *) NULL),
		"\" in inscope namespace command", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * Make the specified namespace the current namespace.
     */

    result = Tcl_PushCallFrame(interp, &frame, namespacePtr,
	    /*isProcCallFrame*/ 0);
    if (result != TCL_OK) {
        return result;
    }

    /*
     * Execute the command. If there is just one argument, just treat it as
     * a script and evaluate it. Otherwise, create a list from the arguments
     * after the first one, then concatenate the first argument and the list
     * of extra arguments to form the command to evaluate.
     */

    if (objc == 4) {
        result = Tcl_EvalObj(interp, objv[3]);
    } else {
	Tcl_Obj *concatObjv[2];
	register Tcl_Obj *listPtr, *cmdObjPtr;
	
        listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
        for (i = 4;  i < objc;  i++) {
	    result = Tcl_ListObjAppendElement(interp, listPtr, objv[i]);
            if (result != TCL_OK) {
                Tcl_DecrRefCount(listPtr); /* free unneeded obj */
                return result;
            }
        }

	concatObjv[0] = objv[3];
	concatObjv[1] = listPtr;
	cmdObjPtr = Tcl_ConcatObj(2, concatObjv);
        result = Tcl_EvalObj(interp, cmdObjPtr);
	
	Tcl_DecrRefCount(cmdObjPtr);  /* we're done with the cmd object */
	Tcl_DecrRefCount(listPtr);    /* we're done with the list object */
    }
    if (result == TCL_ERROR) {
        char msg[256];
	
        sprintf(msg,
	    "\n    (in namespace inscope \"%.200s\" script line %d)",
            namespacePtr->fullName, interp->errorLine);
        Tcl_AddObjErrorInfo(interp, msg, -1);
    }

    /*
     * Restore the previous "current" namespace.
     */

    Tcl_PopCallFrame(interp);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceOriginCmd --
 *
 *	Invoked to implement the "namespace origin" command to return the
 *	fully-qualified name of the "real" command to which the specified
 *	"imported command" refers. Handles the following syntax:
 *
 *	    namespace origin name
 *
 * Results:
 *	An imported command is created in an namespace when that namespace
 *	imports a command from another namespace. If a command is imported
 *	into a sequence of namespaces a, b,...,n where each successive
 *	namespace just imports the command from the previous namespace, this
 *	command returns the fully-qualified name of the original command in
 *	the first namespace, a. If "name" does not refer to an alias, its
 *	fully-qualified name is returned. The returned name is stored in the
 *	interpreter's result object. This procedure returns TCL_OK if
 *	successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	If anything goes wrong, this procedure returns an error message in
 *	the interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceOriginCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Command command, origCommand;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "origin name");
        return TCL_ERROR;
    }

    command = Tcl_GetCommandFromObj(interp, objv[2]);
    if (command == (Tcl_Command) NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"invalid command name \"",
		Tcl_GetStringFromObj(objv[2], (int *) NULL),
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    origCommand = TclGetOriginalCommand(command);
    if (origCommand == (Tcl_Command) NULL) {
	/*
	 * The specified command isn't an imported command. Return the
	 * command's name qualified by the full name of the namespace it
	 * was defined in.
	 */
	
	Tcl_GetCommandFullName(interp, command, Tcl_GetObjResult(interp));
    } else {
	Tcl_GetCommandFullName(interp, origCommand, Tcl_GetObjResult(interp));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceParentCmd --
 *
 *	Invoked to implement the "namespace parent" command that returns the
 *	fully-qualified name of the parent namespace for a specified
 *	namespace. Handles the following syntax:
 *
 *	    namespace parent ?name?
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceParentCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Namespace *nsPtr;
    int result;

    if (objc == 2) {
        nsPtr = Tcl_GetCurrentNamespace(interp);
    } else if (objc == 3) {
	result = GetNamespaceFromObj(interp, objv[2], &nsPtr);
        if (result != TCL_OK) {
            return result;
        }
        if (nsPtr == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "unknown namespace \"",
		    Tcl_GetStringFromObj(objv[2], (int *) NULL),
		    "\" in namespace parent command", (char *) NULL);
            return TCL_ERROR;
        }
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "parent ?name?");
        return TCL_ERROR;
    }

    /*
     * Report the parent of the specified namespace.
     */

    if (nsPtr->parentPtr != NULL) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp),
	        nsPtr->parentPtr->fullName, -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceQualifiersCmd --
 *
 *	Invoked to implement the "namespace qualifiers" command that returns
 *	any leading namespace qualifiers in a string. These qualifiers are
 *	namespace names separated by "::"s. For example, for "::foo::p" this
 *	command returns "::foo", and for "::" it returns "". This command
 *	is the complement of the "namespace tail" command. Note that this
 *	command does not check whether the "namespace" names are, in fact,
 *	the names of currently defined namespaces. Handles the following
 *	syntax:
 *
 *	    namespace qualifiers string
 *
 * Results:
 *	Returns TCL_OK if successful, and  TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceQualifiersCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register char *name, *p;
    int length;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "qualifiers string");
        return TCL_ERROR;
    }

    /*
     * Find the end of the string, then work backward and find
     * the start of the last "::" qualifier.
     */

    name = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    for (p = name;  *p != '\0';  p++) {
	/* empty body */
    }
    while (--p >= name) {
        if ((*p == ':') && (p > name) && (*(p-1) == ':')) {
	    p -= 2;		/* back up over the :: */
	    while ((*p == ':') && (p >= name)) {
		p--;		/* back up over the preceeding : */
	    }
	    break;
        }
    }

    if (p >= name) {
        length = p-name+1;
        Tcl_AppendToObj(Tcl_GetObjResult(interp), name, length);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceTailCmd --
 *
 *	Invoked to implement the "namespace tail" command that returns the
 *	trailing name at the end of a string with "::" namespace
 *	qualifiers. These qualifiers are namespace names separated by
 *	"::"s. For example, for "::foo::p" this command returns "p", and for
 *	"::" it returns "". This command is the complement of the "namespace
 *	qualifiers" command. Note that this command does not check whether
 *	the "namespace" names are, in fact, the names of currently defined
 *	namespaces. Handles the following syntax:
 *
 *	    namespace tail string
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceTailCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register char *name, *p;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "tail string");
        return TCL_ERROR;
    }

    /*
     * Find the end of the string, then work backward and find the
     * last "::" qualifier.
     */

    name = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    for (p = name;  *p != '\0';  p++) {
	/* empty body */
    }
    while (--p > name) {
        if ((*p == ':') && (p > name) && (*(p-1) == ':')) {
            p++;		/* just after the last "::" */
            break;
        }
    }
    
    if (p >= name) {
        Tcl_AppendToObj(Tcl_GetObjResult(interp), p, -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamespaceWhichCmd --
 *
 *	Invoked to implement the "namespace which" command that returns the
 *	fully-qualified name of a command or variable. If the specified
 *	command or variable does not exist, it returns "". Handles the
 *	following syntax:
 *
 *	    namespace which ?-command? ?-variable? name
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If anything
 *	goes wrong, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
NamespaceWhichCmd(dummy, interp, objc, objv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int objc;                           /* Number of arguments. */
    Tcl_Obj *CONST objv[];              /* Argument objects. */
{
    register char *arg;
    Tcl_Command cmd;
    Tcl_Var variable;
    int argIndex, lookup;

    if (objc < 3) {
        badArgs:
        Tcl_WrongNumArgs(interp, 1, objv,
	        "which ?-command? ?-variable? name");
        return TCL_ERROR;
    }

    /*
     * Look for a flag controlling the lookup.
     */

    argIndex = 2;
    lookup = 0;			/* assume command lookup by default */
    arg = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    if (*arg == '-') {
	if (strncmp(arg, "-command", 8) == 0) {
	    lookup = 0;
	} else if (strncmp(arg, "-variable", 9) == 0) {
	    lookup = 1;
	} else {
	    goto badArgs;
	}
	argIndex = 3;
    }
    if (objc != (argIndex + 1)) {
	goto badArgs;
    }

    switch (lookup) {
    case 0:			/* -command */
	cmd = Tcl_GetCommandFromObj(interp, objv[argIndex]);
        if (cmd == (Tcl_Command) NULL) {	
            return TCL_OK;	/* cmd not found, just return (no error) */
        }
	Tcl_GetCommandFullName(interp, cmd, Tcl_GetObjResult(interp));
        break;

    case 1:			/* -variable */
        arg = Tcl_GetStringFromObj(objv[argIndex], (int *) NULL);
	variable = Tcl_FindNamespaceVar(interp, arg, (Tcl_Namespace *) NULL,
		/*flags*/ 0);
        if (variable != (Tcl_Var) NULL) {
            Tcl_GetVariableFullName(interp, variable, Tcl_GetObjResult(interp));
        }
        break;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeNsNameInternalRep --
 *
 *	Frees the resources associated with a nsName object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Decrements the ref count of any Namespace structure pointed
 *	to by the nsName's internal representation. If there are no more
 *	references to the namespace, it's structure will be freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeNsNameInternalRep(objPtr)
    register Tcl_Obj *objPtr;   /* nsName object with internal
                                 * representation to free */
{
    register ResolvedNsName *resNamePtr =
        (ResolvedNsName *) objPtr->internalRep.otherValuePtr;
    Namespace *nsPtr;

    /*
     * Decrement the reference count of the namespace. If there are no
     * more references, free it up.
     */

    if (resNamePtr != NULL) {
        resNamePtr->refCount--;
        if (resNamePtr->refCount == 0) {

            /*
	     * Decrement the reference count for the cached namespace.  If
	     * the namespace is dead, and there are no more references to
	     * it, free it.
	     */

            nsPtr = resNamePtr->nsPtr;
            nsPtr->refCount--;
            if ((nsPtr->refCount == 0) && (nsPtr->flags & NS_DEAD)) {
                NamespaceFree(nsPtr);
            }
            ckfree((char *) resNamePtr);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupNsNameInternalRep --
 *
 *	Initializes the internal representation of a nsName object to a copy
 *	of the internal representation of another nsName object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	copyPtr's internal rep is set to refer to the same namespace
 *	referenced by srcPtr's internal rep. Increments the ref count of
 *	the ResolvedNsName structure used to hold the namespace reference.
 *
 *----------------------------------------------------------------------
 */

static void
DupNsNameInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;                /* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr;      /* Object with internal rep to set. */
{
    register ResolvedNsName *resNamePtr =
        (ResolvedNsName *) srcPtr->internalRep.otherValuePtr;

    copyPtr->internalRep.otherValuePtr = (VOID *) resNamePtr;
    if (resNamePtr != NULL) {
        resNamePtr->refCount++;
    }
    copyPtr->typePtr = &tclNsNameType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetNsNameFromAny --
 *
 *	Attempt to generate a nsName internal representation for a
 *	Tcl object.
 *
 * Results:
 *	Returns TCL_OK if the value could be converted to a proper
 *	namespace reference. Otherwise, it returns TCL_ERROR, along
 *	with an error message in the interpreter's result object.
 *
 * Side effects:
 *	If successful, the object is made a nsName object. Its internal rep
 *	is set to point to a ResolvedNsName, which contains a cached pointer
 *	to the Namespace. Reference counts are kept on both the
 *	ResolvedNsName and the Namespace, so we can keep track of their
 *	usage and free them when appropriate.
 *
 *----------------------------------------------------------------------
 */

static int
SetNsNameFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Points to the namespace in which to
				 * resolve name. Also used for error
				 * reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *name, *dummy;
    Namespace *nsPtr, *dummy1Ptr, *dummy2Ptr;
    register ResolvedNsName *resNamePtr;
    int flags, result;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    name = objPtr->bytes;
    if (name == NULL) {
	name = Tcl_GetStringFromObj(objPtr, (int *) NULL);
    }

    /*
     * Look for the namespace "name" in the current namespace. If there is
     * an error parsing the (possibly qualified) name, return an error.
     * If the namespace isn't found, we convert the object to an nsName
     * object with a NULL ResolvedNsName* internal rep.
     */

    flags = ((interp != NULL)? TCL_LEAVE_ERR_MSG : 0) | FIND_ONLY_NS;
    result = TclGetNamespaceForQualName(interp, name, (Namespace *) NULL,
            flags, &nsPtr, &dummy1Ptr, &dummy2Ptr, &dummy);
    if (result != TCL_OK) {
        return result;
    }

    /*
     * If we found a namespace, then create a new ResolvedNsName structure
     * that holds a reference to it.
     */

    if (nsPtr != NULL) {
	Namespace *currNsPtr =
	        (Namespace *) Tcl_GetCurrentNamespace(interp);
	
        nsPtr->refCount++;
        resNamePtr = (ResolvedNsName *) ckalloc(sizeof(ResolvedNsName));
        resNamePtr->nsPtr = nsPtr;
        resNamePtr->nsId = nsPtr->nsId;
        resNamePtr->refNsPtr = currNsPtr;
        resNamePtr->refCount = 1;
    } else {
        resNamePtr = NULL;
    }

    /*
     * Free the old internalRep before setting the new one.
     * We do this as late as possible to allow the conversion code
     * (in particular, Tcl_GetStringFromObj) to use that old internalRep.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
        oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = (VOID *) resNamePtr;
    objPtr->typePtr = &tclNsNameType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfNsName --
 *
 *	Updates the string representation for a nsName object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a copy of the fully qualified
 *	namespace name.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfNsName(objPtr)
    register Tcl_Obj *objPtr; /* nsName object with string rep to update. */
{
    ResolvedNsName *resNamePtr =
        (ResolvedNsName *) objPtr->internalRep.otherValuePtr;
    register Namespace *nsPtr;
    char *name = "";
    int length;

    if ((resNamePtr != NULL)
	    && (resNamePtr->nsId == resNamePtr->nsPtr->nsId)) {
        nsPtr = resNamePtr->nsPtr;
        if (nsPtr->flags & NS_DEAD) {
            nsPtr = NULL;
        }
        if (nsPtr != NULL) {
            name = nsPtr->fullName;
        }
    }

    /*
     * The following sets the string rep to an empty string on the heap
     * if the internal rep is NULL.
     */

    length = strlen(name);
    if (length == 0) {
	objPtr->bytes = tclEmptyStringRep;
    } else {
	objPtr->bytes = (char *) ckalloc((unsigned) (length + 1));
	memcpy((VOID *) objPtr->bytes, (VOID *) name, (unsigned) length);
	objPtr->bytes[length] = '\0';
    }
    objPtr->length = length;
}
