/* 
 * tclProc.c --
 *
 *	This file contains routines that implement Tcl procedures,
 *	including the "proc" and "uplevel" commands.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclProc.c 1.116 97/10/29 18:33:24
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Forward references to procedures defined later in this file:
 */

static void	CleanupProc _ANSI_ARGS_((Proc *procPtr));
static  int	InterpProc _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
static  void	ProcDeleteProc _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ProcObjCmd --
 *
 *	This object-based procedure is invoked to process the "proc" Tcl 
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	A new procedure gets created.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ProcObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    register Proc *procPtr;
    char *fullName, *procName, *args, *bytes, *p;
    char **argArray = NULL;
    Namespace *nsPtr, *altNsPtr, *cxtNsPtr;
    Tcl_Obj *defPtr, *bodyPtr;
    Tcl_Command cmd;
    Tcl_DString ds;
    int numArgs, length, result, i;
    register CompiledLocal *localPtr;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "name args body");
	return TCL_ERROR;
    }

    /*
     * Determine the namespace where the procedure should reside. Unless
     * the command name includes namespace qualifiers, this will be the
     * current namespace.
     */
    
    fullName = Tcl_GetStringFromObj(objv[1], (int *) NULL);
    result = TclGetNamespaceForQualName(interp, fullName,
	    (Namespace *) NULL, TCL_LEAVE_ERR_MSG,
            &nsPtr, &altNsPtr, &cxtNsPtr, &procName);
    if (result != TCL_OK) {
        return result;
    }
    if (nsPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"can't create procedure \"", fullName,
		"\": unknown namespace", (char *) NULL);
        return TCL_ERROR;
    }
    if (procName == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"can't create procedure \"", fullName,
		"\": bad procedure name", (char *) NULL);
        return TCL_ERROR;
    }
    if ((nsPtr != iPtr->globalNsPtr)
	    && (procName != NULL) && (procName[0] == ':')) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"can't create procedure \"", procName,
		"\" in non-global namespace with name starting with \":\"",
	        (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * If the procedure's body object is shared because its string value is
     * identical to, e.g., the body of another procedure, we must create a
     * private copy for this procedure to use. Such sharing of procedure
     * bodies is rare but can cause problems. A procedure body is compiled
     * in a context that includes the number of compiler-allocated "slots"
     * for local variables. Each formal parameter is given a local variable
     * slot (the "procPtr->numCompiledLocals = numArgs" assignment
     * below). This means that the same code can not be shared by two
     * procedures that have a different number of arguments, even if their
     * bodies are identical. Note that we don't use Tcl_DuplicateObj since
     * we would not want any bytecode internal representation.
     */

    bodyPtr = objv[3];
    if (Tcl_IsShared(bodyPtr)) {
	bytes = Tcl_GetStringFromObj(bodyPtr, &length);
	bodyPtr = Tcl_NewStringObj(bytes, length);
    }

    /*
     * Create and initialize a Proc structure for the procedure. Note that
     * we initialize its cmdPtr field below after we've created the command
     * for the procedure. We increment the ref count of the procedure's
     * body object since there will be a reference to it in the Proc
     * structure.
     */
    
    Tcl_IncrRefCount(bodyPtr);

    procPtr = (Proc *) ckalloc(sizeof(Proc));
    procPtr->iPtr = iPtr;
    procPtr->refCount = 1;
    procPtr->bodyPtr = bodyPtr;
    procPtr->numArgs  = 0;	/* actual argument count is set below. */
    procPtr->numCompiledLocals = 0;
    procPtr->firstLocalPtr = NULL;
    procPtr->lastLocalPtr = NULL;
    
    /*
     * Break up the argument list into argument specifiers, then process
     * each argument specifier.
     * THIS FAILS IF THE ARG LIST OBJECT'S STRING REP CONTAINS NULLS.
     */

    args = Tcl_GetStringFromObj(objv[2], &length);
    result = Tcl_SplitList(interp, args, &numArgs, &argArray);
    if (result != TCL_OK) {
	goto procError;
    }
    
    procPtr->numArgs = numArgs;
    procPtr->numCompiledLocals = numArgs;
    for (i = 0;  i < numArgs;  i++) {
	int fieldCount, nameLength, valueLength;
	char **fieldValues;

	/*
	 * Now divide the specifier up into name and default.
	 */

	result = Tcl_SplitList(interp, argArray[i], &fieldCount,
		&fieldValues);
	if (result != TCL_OK) {
	    goto procError;
	}
	if (fieldCount > 2) {
	    ckfree((char *) fieldValues);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "too many fields in argument specifier \"",
		    argArray[i], "\"", (char *) NULL);
	    goto procError;
	}
	if ((fieldCount == 0) || (*fieldValues[0] == 0)) {
	    ckfree((char *) fieldValues);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "procedure \"", fullName,
		    "\" has argument with no name", (char *) NULL);
	    goto procError;
	}
	
	nameLength = strlen(fieldValues[0]);
	if (fieldCount == 2) {
	    valueLength = strlen(fieldValues[1]);
	} else {
	    valueLength = 0;
	}

	/*
	 * Check that the formal parameter name is a scalar.
	 */

	p = fieldValues[0];
	while (*p != '\0') {
	    if (*p == '(') {
		char *q = p;
		do {
		    q++;
		} while (*q != '\0');
		q--;
		if (*q == ')') { /* we have an array element */
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		            "procedure \"", fullName,
		            "\" has formal parameter \"", fieldValues[0],
			    "\" that is an array element",
			    (char *) NULL);
		    ckfree((char *) fieldValues);
		    goto procError;
		}
	    }
	    p++;
	}

	/*
	 * Allocate an entry in the runtime procedure frame's array of local
	 * variables for the argument. 
	 */

	localPtr = (CompiledLocal *) ckalloc((unsigned) 
	        (sizeof(CompiledLocal) - sizeof(localPtr->name)
		+ nameLength+1));
	if (procPtr->firstLocalPtr == NULL) {
	    procPtr->firstLocalPtr = procPtr->lastLocalPtr = localPtr;
	} else {
	    procPtr->lastLocalPtr->nextPtr = localPtr;
	    procPtr->lastLocalPtr = localPtr;
	}
	localPtr->nextPtr = NULL;
	localPtr->nameLength = nameLength;
	localPtr->frameIndex = i;
	localPtr->isArg  = 1;
	localPtr->isTemp = 0;
	localPtr->flags = VAR_SCALAR;
	if (fieldCount == 2) {
	    localPtr->defValuePtr =
		    Tcl_NewStringObj(fieldValues[1], valueLength);
	    Tcl_IncrRefCount(localPtr->defValuePtr);
	} else {
	    localPtr->defValuePtr = NULL;
	}
	strcpy(localPtr->name, fieldValues[0]);
	
	ckfree((char *) fieldValues);
    }

    /*
     * Now create a command for the procedure. This will initially be in
     * the current namespace unless the procedure's name included namespace
     * qualifiers. To create the new command in the right namespace, we
     * generate a fully qualified name for it.
     */

    Tcl_DStringInit(&ds);
    if (nsPtr != iPtr->globalNsPtr) {
	Tcl_DStringAppend(&ds, nsPtr->fullName, -1);
	Tcl_DStringAppend(&ds, "::", 2);
    }
    Tcl_DStringAppend(&ds, procName, -1);
    
    Tcl_CreateCommand(interp, Tcl_DStringValue(&ds), InterpProc,
	    (ClientData) procPtr, ProcDeleteProc);
    cmd = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&ds),
	    TclObjInterpProc, (ClientData) procPtr, ProcDeleteProc);

    /*
     * Now initialize the new procedure's cmdPtr field. This will be used
     * later when the procedure is called to determine what namespace the
     * procedure will run in. This will be different than the current
     * namespace if the proc was renamed into a different namespace.
     */
    
    procPtr->cmdPtr = (Command *) cmd;
	
    ckfree((char *) argArray);
    return TCL_OK;

    procError:
    Tcl_DecrRefCount(bodyPtr);
    while (procPtr->firstLocalPtr != NULL) {
	localPtr = procPtr->firstLocalPtr;
	procPtr->firstLocalPtr = localPtr->nextPtr;
	
	defPtr = localPtr->defValuePtr;
	if (defPtr != NULL) {
	    Tcl_DecrRefCount(defPtr);
	}
	
	ckfree((char *) localPtr);
    }
    ckfree((char *) procPtr);
    if (argArray != NULL) {
	ckfree((char *) argArray);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetFrame --
 *
 *	Given a description of a procedure frame, such as the first
 *	argument to an "uplevel" or "upvar" command, locate the
 *	call frame for the appropriate level of procedure.
 *
 * Results:
 *	The return value is -1 if an error occurred in finding the
 *	frame (in this case an error message is left in interp->result).
 *	1 is returned if string was either a number or a number preceded
 *	by "#" and it specified a valid frame.  0 is returned if string
 *	isn't one of the two things above (in this case, the lookup
 *	acts as if string were "1").  The variable pointed to by
 *	framePtrPtr is filled in with the address of the desired frame
 *	(unless an error occurs, in which case it isn't modified).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetFrame(interp, string, framePtrPtr)
    Tcl_Interp *interp;		/* Interpreter in which to find frame. */
    char *string;		/* String describing frame. */
    CallFrame **framePtrPtr;	/* Store pointer to frame here (or NULL
				 * if global frame indicated). */
{
    register Interp *iPtr = (Interp *) interp;
    int curLevel, level, result;
    CallFrame *framePtr;

    /*
     * Parse string to figure out which level number to go to.
     */

    result = 1;
    curLevel = (iPtr->varFramePtr == NULL) ? 0 : iPtr->varFramePtr->level;
    if (*string == '#') {
	if (Tcl_GetInt(interp, string+1, &level) != TCL_OK) {
	    return -1;
	}
	if (level < 0) {
	    levelError:
	    Tcl_AppendResult(interp, "bad level \"", string, "\"",
		    (char *) NULL);
	    return -1;
	}
    } else if (isdigit(UCHAR(*string))) {
	if (Tcl_GetInt(interp, string, &level) != TCL_OK) {
	    return -1;
	}
	level = curLevel - level;
    } else {
	level = curLevel - 1;
	result = 0;
    }

    /*
     * Figure out which frame to use, and modify the interpreter so
     * its variables come from that frame.
     */

    if (level == 0) {
	framePtr = NULL;
    } else {
	for (framePtr = iPtr->varFramePtr; framePtr != NULL;
		framePtr = framePtr->callerVarPtr) {
	    if (framePtr->level == level) {
		break;
	    }
	}
	if (framePtr == NULL) {
	    goto levelError;
	}
    }
    *framePtrPtr = framePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UplevelObjCmd --
 *
 *	This object procedure is invoked to process the "uplevel" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_UplevelObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    char *optLevel;
    int length, result;
    CallFrame *savedVarFramePtr, *framePtr;

    if (objc < 2) {
	uplevelSyntax:
	Tcl_WrongNumArgs(interp, 1, objv, "?level? command ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Find the level to use for executing the command.
     * THIS FAILS IF THE OBJECT RESULT'S STRING REP CONTAINS A NULL.
     */

    optLevel = Tcl_GetStringFromObj(objv[1], &length);
    result = TclGetFrame(interp, optLevel, &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }
    objc -= (result+1);
    if (objc == 0) {
	goto uplevelSyntax;
    }
    objv += (result+1);

    /*
     * Modify the interpreter state to execute in the given frame.
     */

    savedVarFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = framePtr;

    /*
     * Execute the residual arguments as a command.
     */

    if (objc == 1) {
	result = Tcl_EvalObj(interp, objv[0]);
    } else {
	Tcl_Obj *cmdObjPtr = Tcl_ConcatObj(objc, objv);
	result = Tcl_EvalObj(interp, cmdObjPtr);
	Tcl_DecrRefCount(cmdObjPtr); /* done with object */
    }
    if (result == TCL_ERROR) {
	char msg[60];
	sprintf(msg, "\n    (\"uplevel\" body line %d)", interp->errorLine);
	Tcl_AddObjErrorInfo(interp, msg, -1);
    }

    /*
     * Restore the variable frame, and return.
     */

    iPtr->varFramePtr = savedVarFramePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFindProc --
 *
 *	Given the name of a procedure, return a pointer to the
 *	record describing the procedure.
 *
 * Results:
 *	NULL is returned if the name doesn't correspond to any
 *	procedure.  Otherwise the return value is a pointer to
 *	the procedure's record.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Proc *
TclFindProc(iPtr, procName)
    Interp *iPtr;		/* Interpreter in which to look. */
    char *procName;		/* Name of desired procedure. */
{
    Tcl_Command cmd;
    Command *cmdPtr;

    cmd = Tcl_FindCommand((Tcl_Interp *) iPtr, procName,
            (Tcl_Namespace *) NULL, /*flags*/ 0);
    if (cmd == (Tcl_Command) NULL) {
        return NULL;
    }
    cmdPtr = (Command *) cmd;
    if (cmdPtr->proc != InterpProc) {
	return NULL;
    }
    return (Proc *) cmdPtr->clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIsProc --
 *
 *	Tells whether a command is a Tcl procedure or not.
 *
 * Results:
 *	If the given command is actuall a Tcl procedure, the
 *	return value is the address of the record describing
 *	the procedure.  Otherwise the return value is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Proc *
TclIsProc(cmdPtr)
    Command *cmdPtr;		/* Command to test. */
{
    if (cmdPtr->proc == InterpProc) {
	return (Proc *) cmdPtr->clientData;
    }
    return (Proc *) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpProc --
 *
 *	When a Tcl procedure gets invoked with an argc/argv array of
 *	strings, this routine gets invoked to interpret the procedure.
 *
 * Results:
 *	A standard Tcl result value, usually TCL_OK.
 *
 * Side effects:
 *	Depends on the commands in the procedure.
 *
 *----------------------------------------------------------------------
 */

static int
InterpProc(clientData, interp, argc, argv)
    ClientData clientData;	/* Record describing procedure to be
				 * interpreted. */
    Tcl_Interp *interp;		/* Interpreter in which procedure was
				 * invoked. */
    int argc;			/* Count of number of arguments to this
				 * procedure. */
    register char **argv;	/* Argument values. */
{
    register Tcl_Obj *objPtr;
    register int i;
    int result;

    /*
     * This procedure generates an objv array for object arguments that hold
     * the argv strings. It starts out with stack-allocated space but uses
     * dynamically-allocated storage if needed.
     */

#define NUM_ARGS 20
    Tcl_Obj *(objStorage[NUM_ARGS]);
    register Tcl_Obj **objv = objStorage;

    /*
     * Create the object argument array "objv". Make sure objv is large
     * enough to hold the objc arguments plus 1 extra for the zero
     * end-of-objv word.
     */

    if ((argc + 1) > NUM_ARGS) {
	objv = (Tcl_Obj **)
	    ckalloc((unsigned)(argc + 1) * sizeof(Tcl_Obj *));
    }

    for (i = 0;  i < argc;  i++) {
	objv[i] = Tcl_NewStringObj(argv[i], -1);
	Tcl_IncrRefCount(objv[i]);
    }
    objv[argc] = 0;

    /*
     * Use TclObjInterpProc to actually interpret the procedure.
     */

    result = TclObjInterpProc(clientData, interp, argc, objv);

    /*
     * Move the interpreter's object result to the string result, 
     * then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
     */
    
    Tcl_SetResult(interp,
	    TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	    TCL_VOLATILE);

    /*
     * Decrement the ref counts on the objv elements since we are done
     * with them.
     */

    for (i = 0;  i < argc;  i++) {
	objPtr = objv[i];
	TclDecrRefCount(objPtr);
    }
    
    /*
     * Free the objv array if malloc'ed storage was used.
     */

    if (objv != objStorage) {
	ckfree((char *) objv);
    }
    return result;
#undef NUM_ARGS
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjInterpProc --
 *
 *	When a Tcl procedure gets invoked during bytecode evaluation, this 
 *	object-based routine gets invoked to interpret the procedure.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	Depends on the commands in the procedure.
 *
 *----------------------------------------------------------------------
 */

int
TclObjInterpProc(clientData, interp, objc, objv)
    ClientData clientData;	/* Record describing procedure to be
				 * interpreted. */
    Tcl_Interp *interp;		/* Interpreter in which procedure was
				 * invoked. */
    int objc;			/* Count of number of arguments to this
				 * procedure. */
    Tcl_Obj *CONST objv[];	/* Argument value objects. */
{
    Interp *iPtr = (Interp *) interp;
    Proc *procPtr = (Proc *) clientData;
    Tcl_Obj *bodyPtr = procPtr->bodyPtr;
    CallFrame frame;
    register CallFrame *framePtr = &frame;
    register Var *varPtr;
    register CompiledLocal *localPtr;
    Proc *saveProcPtr;
    char *procName, *bytes;
    int nameLen, localCt, numArgs, argCt, length, i, result;

    /*
     * This procedure generates an array "compiledLocals" that holds the
     * storage for local variables. It starts out with stack-allocated space
     * but uses dynamically-allocated storage if needed.
     */

#define NUM_LOCALS 20
    Var localStorage[NUM_LOCALS];
    Var *compiledLocals = localStorage;

    /*
     * Get the procedure's name.
     * THIS FAILS IF THE PROC NAME'S STRING REP HAS A NULL.
     */
    
    procName = Tcl_GetStringFromObj(objv[0], &nameLen);

    /*
     * If necessary, compile the procedure's body. The compiler will
     * allocate frame slots for the procedure's non-argument local
     * variables. If the ByteCode already exists, make sure it hasn't been
     * invalidated by someone redefining a core command (this might make the
     * compiled code wrong). Also, if the code was compiled in/for a
     * different interpreter, we recompile it. Note that compiling the body
     * might increase procPtr->numCompiledLocals if new local variables are
     * found while compiling.
     */

    if (bodyPtr->typePtr == &tclByteCodeType) {
	ByteCode *codePtr = (ByteCode *) bodyPtr->internalRep.otherValuePtr;
	
	if ((codePtr->iPtr != iPtr)
	        || (codePtr->compileEpoch != iPtr->compileEpoch)) {
	    tclByteCodeType.freeIntRepProc(bodyPtr);
	    bodyPtr->typePtr = (Tcl_ObjType *) NULL;
	}
    }
    if (bodyPtr->typePtr != &tclByteCodeType) {
	char buf[100];
	int numChars;
	char *ellipsis;
	
	if (tclTraceCompile >= 1) {
	    /*
	     * Display a line summarizing the top level command we
	     * are about to compile.
	     */

	    numChars = nameLen;
	    ellipsis = "";
	    if (numChars > 50) {
		numChars = 50;
		ellipsis = "...";
	    }
	    fprintf(stdout, "Compiling body of proc \"%.*s%s\"\n",
		    numChars, procName, ellipsis);
	}
	
	saveProcPtr = iPtr->compiledProcPtr;
	iPtr->compiledProcPtr = procPtr;
	result = tclByteCodeType.setFromAnyProc(interp, bodyPtr);
	iPtr->compiledProcPtr = saveProcPtr;
	
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		numChars = nameLen;
		ellipsis = "";
		if (numChars > 50) {
		    numChars = 50;
		    ellipsis = "...";
		}
		sprintf(buf, "\n    (compiling body of proc \"%.*s%s\", line %d)",
			numChars, procName, ellipsis, interp->errorLine);
		Tcl_AddObjErrorInfo(interp, buf, -1);
	    }
	    return result;
	}
    }

    /*
     * Create the "compiledLocals" array. Make sure it is large enough to
     * hold all the procedure's compiled local variables, including its
     * formal parameters.
     */

    localCt = procPtr->numCompiledLocals;
    if (localCt > NUM_LOCALS) {
	compiledLocals = (Var *) ckalloc((unsigned) localCt * sizeof(Var));
    }
    
    /*
     * Set up and push a new call frame for the new procedure invocation.
     * This call frame will execute in the proc's namespace, which might
     * be different than the current namespace. The proc's namespace is
     * that of its command, which can change if the command is renamed
     * from one namespace to another.
     */

    result = Tcl_PushCallFrame(interp, (Tcl_CallFrame *) framePtr,
            (Tcl_Namespace *) procPtr->cmdPtr->nsPtr,
	     /*isProcCallFrame*/ 1);
    if (result != TCL_OK) {
        return result;
    }

    framePtr->objc = objc;
    framePtr->objv = objv;  /* ref counts for args are incremented below */
    framePtr->procPtr = procPtr;
    framePtr->numCompiledLocals = localCt;
    framePtr->compiledLocals = compiledLocals;

    /*
     * Initialize the array of local variables stored in the call frame.
     */

    varPtr = framePtr->compiledLocals;
    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;
	    localPtr = localPtr->nextPtr) {
	varPtr->value.objPtr = NULL;
	varPtr->name = localPtr->name; /* will be just '\0' if temp var */
	varPtr->nsPtr = NULL;
	varPtr->hPtr = NULL;
	varPtr->refCount = 0;
	varPtr->tracePtr = NULL;
	varPtr->searchPtr = NULL;
	varPtr->flags = (localPtr->flags | VAR_UNDEFINED);
	varPtr++;
    }

    /*
     * Match and assign the call's actual parameters to the procedure's
     * formal arguments. The formal arguments are described by the first
     * numArgs entries in both the Proc structure's local variable list and
     * the call frame's local variable array.
     */

    numArgs = procPtr->numArgs;
    varPtr = framePtr->compiledLocals;
    localPtr = procPtr->firstLocalPtr;
    argCt = objc;
    for (i = 1, argCt -= 1;  i <= numArgs;  i++, argCt--) {
	if (!localPtr->isArg) {
	    panic("TclObjInterpProc: local variable %s is not argument but should be",
		  localPtr->name);
	    return TCL_ERROR;
	}
	if (localPtr->isTemp) {
	    panic("TclObjInterpProc: local variable %d is temporary but should be an argument", i);
	    return TCL_ERROR;
	}

	/*
	 * Handle the special case of the last formal being "args".  When
	 * it occurs, assign it a list consisting of all the remaining
	 * actual arguments.
	 */

	if ((i == numArgs) && ((localPtr->name[0] == 'a')
	        && (strcmp(localPtr->name, "args") == 0))) {
	    Tcl_Obj *listPtr = Tcl_NewListObj(argCt, &(objv[i]));
	    varPtr->value.objPtr = listPtr;
	    Tcl_IncrRefCount(listPtr); /* local var is a reference */
	    varPtr->flags &= ~VAR_UNDEFINED;
	    argCt = 0;
	    break;		/* done processing args */
	} else if (argCt > 0) {
	    Tcl_Obj *objPtr = objv[i];
	    varPtr->value.objPtr = objPtr;
	    varPtr->flags &= ~VAR_UNDEFINED;
	    Tcl_IncrRefCount(objPtr);  /* since the local variable now has
					* another reference to object. */
	} else if (localPtr->defValuePtr != NULL) {
	    Tcl_Obj *objPtr = localPtr->defValuePtr;
	    varPtr->value.objPtr = objPtr;
	    varPtr->flags &= ~VAR_UNDEFINED;
	    Tcl_IncrRefCount(objPtr);  /* since the local variable now has
					* another reference to object. */
	} else {
	    Tcl_ResetResult(interp);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "no value given for parameter \"", localPtr->name,
		    "\" to \"", Tcl_GetStringFromObj(objv[0], (int *) NULL),
		    "\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto procDone;
	}
	varPtr++;
	localPtr = localPtr->nextPtr;
    }
    if (argCt > 0) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"called \"", Tcl_GetStringFromObj(objv[0], (int *) NULL),
		"\" with too many arguments", (char *) NULL);
	result = TCL_ERROR;
	goto procDone;
    }

    /*
     * Invoke the commands in the procedure's body.
     */

    if (tclTraceExec >= 1) {
	fprintf(stdout, "Calling proc ");
	for (i = 0;  i < objc;  i++) {
	    bytes = Tcl_GetStringFromObj(objv[i], &length);
	    TclPrintSource(stdout, bytes, TclMin(length, 15));
	    fprintf(stdout, " ");
	}
	fprintf(stdout, "\n");
	fflush(stdout);
    }

    iPtr->returnCode = TCL_OK;
    procPtr->refCount++;
    result = Tcl_EvalObj(interp, procPtr->bodyPtr);
    procPtr->refCount--;
    if (procPtr->refCount <= 0) {
	CleanupProc(procPtr);
    }

    if (result != TCL_OK) {
	if (result == TCL_RETURN) {
	    result = TclUpdateReturnInfo(iPtr);
	} else if (result == TCL_ERROR) {
	    char msg[100];
	    sprintf(msg, "\n    (procedure \"%.50s\" line %d)",
		    procName, iPtr->errorLine);
	    Tcl_AddObjErrorInfo(interp, msg, -1);
	} else if (result == TCL_BREAK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "invoked \"break\" outside of a loop", -1);
	    result = TCL_ERROR;
	} else if (result == TCL_CONTINUE) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    "invoked \"continue\" outside of a loop", -1);
	    result = TCL_ERROR;
	}
    }
    
    procDone:

    /*
     * Pop and free the call frame for this procedure invocation.
     */
    
    Tcl_PopCallFrame(interp);
    
    /*
     * Free the compiledLocals array if malloc'ed storage was used.
     */

    if (compiledLocals != localStorage) {
	ckfree((char *) compiledLocals);
    }
    return result;
#undef NUM_LOCALS
}

/*
 *----------------------------------------------------------------------
 *
 * ProcDeleteProc --
 *
 *	This procedure is invoked just before a command procedure is
 *	removed from an interpreter.  Its job is to release all the
 *	resources allocated to the procedure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, unless the procedure is actively being
 *	executed.  In this case the cleanup is delayed until the
 *	last call to the current procedure completes.
 *
 *----------------------------------------------------------------------
 */

static void
ProcDeleteProc(clientData)
    ClientData clientData;		/* Procedure to be deleted. */
{
    Proc *procPtr = (Proc *) clientData;

    procPtr->refCount--;
    if (procPtr->refCount <= 0) {
	CleanupProc(procPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupProc --
 *
 *	This procedure does all the real work of freeing up a Proc
 *	structure.  It's called only when the structure's reference
 *	count becomes zero.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupProc(procPtr)
    register Proc *procPtr;		/* Procedure to be deleted. */
{
    register CompiledLocal *localPtr;
    Tcl_Obj *bodyPtr = procPtr->bodyPtr;
    Tcl_Obj *defPtr;

    if (bodyPtr != NULL) {
	Tcl_DecrRefCount(bodyPtr);
    }
    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;  ) {
	CompiledLocal *nextPtr = localPtr->nextPtr;

	if (localPtr->defValuePtr != NULL) {
	    defPtr = localPtr->defValuePtr;
	    Tcl_DecrRefCount(defPtr);
	}
	ckfree((char *) localPtr);
	localPtr = nextPtr;
    }
    ckfree((char *) procPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclUpdateReturnInfo --
 *
 *	This procedure is called when procedures return, and at other
 *	points where the TCL_RETURN code is used.  It examines fields
 *	such as iPtr->returnCode and iPtr->errorCode and modifies
 *	the real return status accordingly.
 *
 * Results:
 *	The return value is the true completion code to use for
 *	the procedure, instead of TCL_RETURN.
 *
 * Side effects:
 *	The errorInfo and errorCode variables may get modified.
 *
 *----------------------------------------------------------------------
 */

int
TclUpdateReturnInfo(iPtr)
    Interp *iPtr;		/* Interpreter for which TCL_RETURN
				 * exception is being processed. */
{
    int code;

    code = iPtr->returnCode;
    iPtr->returnCode = TCL_OK;
    if (code == TCL_ERROR) {
	Tcl_SetVar2((Tcl_Interp *) iPtr, "errorCode", (char *) NULL,
		(iPtr->errorCode != NULL) ? iPtr->errorCode : "NONE",
		TCL_GLOBAL_ONLY);
	iPtr->flags |= ERROR_CODE_SET;
	if (iPtr->errorInfo != NULL) {
	    Tcl_SetVar2((Tcl_Interp *) iPtr, "errorInfo", (char *) NULL,
		    iPtr->errorInfo, TCL_GLOBAL_ONLY);
	    iPtr->flags |= ERR_IN_PROGRESS;
	}
    }
    return code;
}
