/* 
 * tclCmdIL.c --
 *
 *	This file contains the top-level command routines for most of
 *	the Tcl built-in commands whose names begin with the letters
 *	I through L.  It contains only commands in the generic core
 *	(i.e. those that don't depend much upon UNIX facilities).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCmdIL.c 1.163 97/06/13 18:16:52
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following variable holds the full path name of the binary
 * from which this application was executed, or NULL if it isn't
 * know.  The value of the variable is set by the procedure
 * Tcl_FindExecutable.  The storage space is dynamically allocated.
 */

char *tclExecutableName = NULL;

/*
 * During execution of the "lsort" command, structures of the following
 * type are used to arrange the objects being sorted into a collection
 * of linked lists.
 */

typedef struct SortElement {
    Tcl_Obj *objPtr;			/* Object being sorted. */
    struct SortElement *nextPtr;        /* Next element in the list, or
					 * NULL for end of list. */
} SortElement;

/*
 * The "lsort" command needs to pass certain information down to the
 * function that compares two list elements, and the comparison function
 * needs to pass success or failure information back up to the top-level
 * "lsort" command.  The following structure is used to pass this
 * information.
 */

typedef struct SortInfo {
    int isIncreasing;		/* Nonzero means sort in increasing order. */
    int sortMode;		/* The sort mode.  One of SORTMODE_*
				 * values defined below */
    Tcl_DString compareCmd;	/* The Tcl comparison command when sortMode
				 * is SORTMODE_COMMAND.  Pre-initialized to
				 * hold base of command.*/
    long index;			/* If the -index option was specified, this
				 * holds the index of the list element
				 * to extract for comparison.  If -index
				 * wasn't specified, this is -1. */
    Tcl_Interp *interp;		/* The interpreter in which the sortis
				 * being done. */
    int resultCode;		/* Completion code for the lsort command.
				 * If an error occurs during the sort this
				 * is changed from TCL_OK to  TCL_ERROR. */
} SortInfo;

/*
 * The "sortMode" field of the SortInfo structure can take on any of the
 * following values.
 */

#define SORTMODE_ASCII      0
#define SORTMODE_INTEGER    1
#define SORTMODE_REAL       2
#define SORTMODE_COMMAND    3
#define SORTMODE_DICTIONARY 4

/*
 * Forward declarations for procedures defined in this file:
 */

static int		DictionaryCompare _ANSI_ARGS_((char *left,
			    char *right));
static int		InfoArgsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoBodyCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoCmdCountCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoCommandsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoCompleteCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoDefaultCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoExistsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoGlobalsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoHostnameCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoLevelCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoLibraryCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoLoadedCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoLocalsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoNameOfExecutableCmd _ANSI_ARGS_((
			    ClientData dummy, Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoPatchLevelCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoProcsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoScriptCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoSharedlibCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoTclVersionCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		InfoVarsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static SortElement *    MergeSort _ANSI_ARGS_((SortElement *headPt,
			    SortInfo *infoPtr));
static SortElement *    MergeLists _ANSI_ARGS_((SortElement *leftPtr,
			    SortElement *rightPtr, SortInfo *infoPtr));
static int		SortCompare _ANSI_ARGS_((Tcl_Obj *firstPtr,
			    Tcl_Obj *second, SortInfo *infoPtr));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IfCmd --
 *
 *	This procedure is invoked to process the "if" Tcl command.
 *	See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when
 *	a command name is computed at runtime, and is "if" or the name
 *	to which "if" was renamed: e.g., "set z if; $z 1 {puts foo}"
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_IfCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i, result, value;

    i = 1;
    while (1) {
	/*
	 * At this point in the loop, argv and argc refer to an expression
	 * to test, either for the main expression or an expression
	 * following an "elseif".  The arguments after the expression must
	 * be "then" (optional) and a script to execute if the expression is
	 * true.
	 */

	if (i >= argc) {
	    Tcl_AppendResult(interp, "wrong # args: no expression after \"",
		    argv[i-1], "\" argument", (char *) NULL);
	    return TCL_ERROR;
	}
	result = Tcl_ExprBoolean(interp, argv[i], &value);
	if (result != TCL_OK) {
	    return result;
	}
	i++;
	if ((i < argc) && (strcmp(argv[i], "then") == 0)) {
	    i++;
	}
	if (i >= argc) {
	    Tcl_AppendResult(interp, "wrong # args: no script following \"",
		    argv[i-1], "\" argument", (char *) NULL);
	    return TCL_ERROR;
	}
	if (value) {
	    return Tcl_Eval(interp, argv[i]);
	}
	
	/*
	 * The expression evaluated to false.  Skip the command, then
	 * see if there is an "else" or "elseif" clause.
	 */

	i++;
	if (i >= argc) {
	    return TCL_OK;
	}
	if ((argv[i][0] == 'e') && (strcmp(argv[i], "elseif") == 0)) {
	    i++;
	    continue;
	}
	break;
    }

    /*
     * Couldn't find a "then" or "elseif" clause to execute.  Check now
     * for an "else" clause.  We know that there's at least one more
     * argument when we get here.
     */

    if (strcmp(argv[i], "else") == 0) {
	i++;
	if (i >= argc) {
	    Tcl_AppendResult(interp,
		    "wrong # args: no script following \"else\" argument",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    }
    return Tcl_Eval(interp, argv[i]);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IncrCmd --
 *
 *	This procedure is invoked to process the "incr" Tcl command.
 *	See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when
 *	a command name is computed at runtime, and is "incr" or the name
 *	to which "incr" was renamed: e.g., "set z incr; $z i -1"
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
int
Tcl_IncrCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int value;
    char *oldString, *result;
    char newString[30];

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" varName ?increment?\"", (char *) NULL);
	return TCL_ERROR;
    }

    oldString = Tcl_GetVar(interp, argv[1], TCL_LEAVE_ERR_MSG);
    if (oldString == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, oldString, &value) != TCL_OK) {
	Tcl_AddErrorInfo(interp,
		"\n    (reading value of variable to increment)");
	return TCL_ERROR;
    }
    if (argc == 2) {
	value += 1;
    } else {
	int increment;

	if (Tcl_GetInt(interp, argv[2], &increment) != TCL_OK) {
	    Tcl_AddErrorInfo(interp,
		    "\n    (reading increment)");
	    return TCL_ERROR;
	}
	value += increment;
    }
    TclFormatInt(newString, value);
    result = Tcl_SetVar(interp, argv[1], newString, TCL_LEAVE_ERR_MSG);
    if (result == NULL) {
	return TCL_ERROR;
    }

    /*
     * Copy the result since the variable's value might change.
     */
    
    Tcl_SetResult(interp, result, TCL_VOLATILE);
    return TCL_OK; 
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InfoObjCmd --
 *
 *	This procedure is invoked to process the "info" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_InfoObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Arbitrary value passed to the command. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    static char *subCmds[] = {
            "args", "body", "cmdcount", "commands",
	     "complete", "default", "exists", "globals",
	     "hostname", "level", "library", "loaded",
	     "locals", "nameofexecutable", "patchlevel", "procs",
	     "script", "sharedlibextension", "tclversion", "vars",
	     (char *) NULL};
    enum ISubCmdIdx {
	    IArgsIdx, IBodyIdx, ICmdCountIdx, ICommandsIdx,
	    ICompleteIdx, IDefaultIdx, IExistsIdx, IGlobalsIdx,
	    IHostnameIdx, ILevelIdx, ILibraryIdx, ILoadedIdx,
	    ILocalsIdx, INameOfExecutableIdx, IPatchLevelIdx, IProcsIdx,
	    IScriptIdx, ISharedLibExtensionIdx, ITclVersionIdx, IVarsIdx
    } index;
    int result;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
        return TCL_ERROR;
    }
    
    result = Tcl_GetIndexFromObj(interp, objv[1], subCmds, "option", 0,
	    (int *) &index);
    if (result != TCL_OK) {
	return result;
    }

    switch (index) {
        case IArgsIdx:
	    result = InfoArgsCmd(clientData, interp, objc, objv);
            break;
	case IBodyIdx:
	    result = InfoBodyCmd(clientData, interp, objc, objv);
	    break;
	case ICmdCountIdx:
	    result = InfoCmdCountCmd(clientData, interp, objc, objv);
	    break;
        case ICommandsIdx:
	    result = InfoCommandsCmd(clientData, interp, objc, objv);
	    break;
        case ICompleteIdx:
	    result = InfoCompleteCmd(clientData, interp, objc, objv);
	    break;
	case IDefaultIdx:
	    result = InfoDefaultCmd(clientData, interp, objc, objv);
	    break;
	case IExistsIdx:
	    result = InfoExistsCmd(clientData, interp, objc, objv);
	    break;
        case IGlobalsIdx:
	    result = InfoGlobalsCmd(clientData, interp, objc, objv);
	    break;
        case IHostnameIdx:
	    result = InfoHostnameCmd(clientData, interp, objc, objv);
	    break;
	case ILevelIdx:
	    result = InfoLevelCmd(clientData, interp, objc, objv);
	    break;
	case ILibraryIdx:
	    result = InfoLibraryCmd(clientData, interp, objc, objv);
	    break;
        case ILoadedIdx:
	    result = InfoLoadedCmd(clientData, interp, objc, objv);
	    break;
        case ILocalsIdx:
	    result = InfoLocalsCmd(clientData, interp, objc, objv);
	    break;
	case INameOfExecutableIdx:
	    result = InfoNameOfExecutableCmd(clientData, interp, objc, objv);
	    break;
	case IPatchLevelIdx:
	    result = InfoPatchLevelCmd(clientData, interp, objc, objv);
	    break;
        case IProcsIdx:
	    result = InfoProcsCmd(clientData, interp, objc, objv);
	    break;
        case IScriptIdx:
	    result = InfoScriptCmd(clientData, interp, objc, objv);
	    break;
	case ISharedLibExtensionIdx:
	    result = InfoSharedlibCmd(clientData, interp, objc, objv);
	    break;
	case ITclVersionIdx:
	    result = InfoTclVersionCmd(clientData, interp, objc, objv);
	    break;
	case IVarsIdx:
	    result = InfoVarsCmd(clientData, interp, objc, objv);
	    break;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoArgsCmd --
 *
 *      Called to implement the "info args" command that returns the
 *      argument list for a procedure. Handles the following syntax:
 *
 *          info args procName
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoArgsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    char *name;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *listObjPtr;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "args procname");
        return TCL_ERROR;
    }

    name = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    procPtr = TclFindProc(iPtr, name);
    if (procPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "\"", name, "\" isn't a procedure", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * Build a return list containing the arguments.
     */
    
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;
            localPtr = localPtr->nextPtr) {
        if (localPtr->isArg) {
            Tcl_ListObjAppendElement(interp, listObjPtr,
		    Tcl_NewStringObj(localPtr->name, -1));
        }
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoBodyCmd --
 *
 *      Called to implement the "info body" command that returns the body
 *      for a procedure. Handles the following syntax:
 *
 *          info body procName
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoBodyCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    char *name;
    Proc *procPtr;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "body procname");
        return TCL_ERROR;
    }

    name = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    procPtr = TclFindProc(iPtr, name);
    if (procPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"\"", name, "\" isn't a procedure", (char *) NULL);
        return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, procPtr->bodyPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoCmdCountCmd --
 *
 *      Called to implement the "info cmdcount" command that returns the
 *      number of commands that have been executed. Handles the following
 *      syntax:
 *
 *          info cmdcount
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoCmdCountCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "cmdcount");
        return TCL_ERROR;
    }

    Tcl_SetIntObj(Tcl_GetObjResult(interp), iPtr->cmdCount);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoCommandsCmd --
 *
 *	Called to implement the "info commands" command that returns the
 *	list of commands in the interpreter that match an optional pattern.
 *	The pattern, if any, consists of an optional sequence of namespace
 *	names separated by "::" qualifiers, which is followed by a
 *	glob-style pattern that restricts which commands are returned.
 *	Handles the following syntax:
 *
 *          info commands ?pattern?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoCommandsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *cmdName, *pattern, *simplePattern;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Namespace *nsPtr;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    Namespace *currNsPtr   = (Namespace *) Tcl_GetCurrentNamespace(interp);
    Tcl_Obj *listPtr, *elemObjPtr;
    int specificNsInPattern = 0;  /* Init. to avoid compiler warning. */
    Tcl_Command cmd;
    int result;

    /*
     * Get the pattern and find the "effective namespace" in which to
     * list commands.
     */

    if (objc == 2) {
        simplePattern = NULL;
	nsPtr = currNsPtr;
	specificNsInPattern = 0;
    } else if (objc == 3) {
	/*
	 * From the pattern, get the effective namespace and the simple
	 * pattern (no namespace qualifiers or ::'s) at the end. If an
	 * error was found while parsing the pattern, return it. Otherwise,
	 * if the namespace wasn't found, just leave nsPtr NULL: we will
	 * return an empty list since no commands there can be found.
	 */

	Namespace *dummy1NsPtr, *dummy2NsPtr;
	
        pattern = Tcl_GetStringFromObj(objv[2], (int *) NULL);
	result = TclGetNamespaceForQualName(interp, pattern,
		(Namespace *) NULL, /*flags*/ TCL_LEAVE_ERR_MSG,
		&nsPtr, &dummy1NsPtr, &dummy2NsPtr, &simplePattern);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nsPtr != NULL) {	/* we successfully found the pattern's ns */
	    specificNsInPattern = (strcmp(simplePattern, pattern) != 0);
	}
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "commands ?pattern?");
        return TCL_ERROR;
    }

    /*
     * Scan through the effective namespace's command table and create a
     * list with all commands that match the pattern. If a specific
     * namespace was requested in the pattern, qualify the command names
     * with the namespace name.
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

    if (nsPtr != NULL) {
	entryPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search);
	while (entryPtr != NULL) {
	    cmdName = Tcl_GetHashKey(&nsPtr->cmdTable, entryPtr);
	    if ((simplePattern == NULL)
	            || Tcl_StringMatch(cmdName, simplePattern)) {
		if (specificNsInPattern) {
		    cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
		    elemObjPtr = Tcl_NewObj();
		    Tcl_GetCommandFullName(interp, cmd, elemObjPtr);
		} else {
		    elemObjPtr = Tcl_NewStringObj(cmdName, -1);
		}
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	    entryPtr = Tcl_NextHashEntry(&search);
	}

	/*
	 * If the effective namespace isn't the global :: namespace, and a
	 * specific namespace wasn't requested in the pattern, then add in
	 * all global :: commands that match the simple pattern. Of course,
	 * we add in only those commands that aren't hidden by a command in
	 * the effective namespace.
	 */
	
	if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
	    entryPtr = Tcl_FirstHashEntry(&globalNsPtr->cmdTable, &search);
	    while (entryPtr != NULL) {
		cmdName = Tcl_GetHashKey(&globalNsPtr->cmdTable, entryPtr);
		if ((simplePattern == NULL)
	                || Tcl_StringMatch(cmdName, simplePattern)) {
		    if (Tcl_FindHashEntry(&nsPtr->cmdTable, cmdName) == NULL) {
			Tcl_ListObjAppendElement(interp, listPtr,
				Tcl_NewStringObj(cmdName, -1));
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }
	}
    }
    
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoCompleteCmd --
 *
 *      Called to implement the "info complete" command that determines
 *      whether a string is a complete Tcl command. Handles the following
 *      syntax:
 *
 *          info complete command
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoCompleteCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *command;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "complete command");
        return TCL_ERROR;
    }

    command = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    if (Tcl_CommandComplete(command)) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
    } else {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoDefaultCmd --
 *
 *      Called to implement the "info default" command that returns the
 *      default value for a procedure argument. Handles the following
 *      syntax:
 *
 *          info default procName arg varName
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoDefaultCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    char *procName, *argName, *varName;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *valueObjPtr;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 1, objv, "default procname arg varname");
        return TCL_ERROR;
    }

    procName = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    argName = Tcl_GetStringFromObj(objv[3], (int *) NULL);

    procPtr = TclFindProc(iPtr, procName);
    if (procPtr == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"\"", procName, "\" isn't a procedure", (char *) NULL);
        return TCL_ERROR;
    }

    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;
            localPtr = localPtr->nextPtr) {
        if ((localPtr->isArg) && (strcmp(argName, localPtr->name) == 0)) {
            if (localPtr->defValuePtr != NULL) {
		valueObjPtr = Tcl_ObjSetVar2(interp, objv[4], NULL,
                        localPtr->defValuePtr, 0);
                if (valueObjPtr == NULL) {
                    defStoreError:
		    varName = Tcl_GetStringFromObj(objv[4], (int *) NULL);
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
	                    "couldn't store default value in variable \"",
			    varName, "\"", (char *) NULL);
                    return TCL_ERROR;
                }
		Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
            } else {
                Tcl_Obj *nullObjPtr = Tcl_NewObj();
                valueObjPtr = Tcl_ObjSetVar2(interp, objv[4], NULL,
                    nullObjPtr, 0);
                if (valueObjPtr == NULL) {
                    Tcl_DecrRefCount(nullObjPtr); /* free unneeded obj */
                    goto defStoreError;
                }
		Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
            }
            return TCL_OK;
        }
    }

    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
	    "procedure \"", procName, "\" doesn't have an argument \"",
	    argName, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoExistsCmd --
 *
 *      Called to implement the "info exists" command that determines
 *      whether a variable exists. Handles the following syntax:
 *
 *          info exists varName
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoExistsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *varName;
    Var *varPtr, *arrayPtr;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "exists varName");
        return TCL_ERROR;
    }

    varName = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    varPtr = TclLookupVar(interp, varName, (char *) NULL,
            TCL_PARSE_PART1, "access",
            /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);
    if ((varPtr != NULL) && !TclIsVarUndefined(varPtr)) {
        Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
    } else {
        Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoGlobalsCmd --
 *
 *      Called to implement the "info globals" command that returns the list
 *      of global variables matching an optional pattern. Handles the
 *      following syntax:
 *
 *          info globals ?pattern?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoGlobalsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *varName, *pattern;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Var *varPtr;
    Tcl_Obj *listPtr;

    if (objc == 2) {
        pattern = NULL;
    } else if (objc == 3) {
        pattern = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "globals ?pattern?");
        return TCL_ERROR;
    }

    /*
     * Scan through the global :: namespace's variable table and create a
     * list of all global variables that match the pattern.
     */
    
    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (entryPtr = Tcl_FirstHashEntry(&globalNsPtr->varTable, &search);
            entryPtr != NULL;
            entryPtr = Tcl_NextHashEntry(&search)) {
        varPtr = (Var *) Tcl_GetHashValue(entryPtr);
        if (TclIsVarUndefined(varPtr)) {
            continue;
        }
        varName = Tcl_GetHashKey(&globalNsPtr->varTable, entryPtr);
        if ((pattern == NULL) || Tcl_StringMatch(varName, pattern)) {
            Tcl_ListObjAppendElement(interp, listPtr,
		    Tcl_NewStringObj(varName, -1));
        }
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoHostnameCmd --
 *
 *      Called to implement the "info hostname" command that returns the
 *      host name. Handles the following syntax:
 *
 *          info hostname
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoHostnameCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "hostname");
        return TCL_ERROR;
    }

    Tcl_SetStringObj(Tcl_GetObjResult(interp), Tcl_GetHostName(), -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLevelCmd --
 *
 *      Called to implement the "info level" command that returns
 *      information about the call stack. Handles the following syntax:
 *
 *          info level ?number?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLevelCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    int level;
    CallFrame *framePtr;
    Tcl_Obj *listPtr;

    if (objc == 2) {		/* just "info level" */
        if (iPtr->varFramePtr == NULL) {
            Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
        } else {
            Tcl_SetIntObj(Tcl_GetObjResult(interp), iPtr->varFramePtr->level);
        }
        return TCL_OK;
    } else if (objc == 3) {
        if (Tcl_GetIntFromObj(interp, objv[2], &level) != TCL_OK) {
            return TCL_ERROR;
        }
        if (level <= 0) {
            if (iPtr->varFramePtr == NULL) {
                levelError:
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"bad level \"",
			Tcl_GetStringFromObj(objv[2], (int *) NULL),
			"\"", (char *) NULL);
                return TCL_ERROR;
            }
            level += iPtr->varFramePtr->level;
        }
        for (framePtr = iPtr->varFramePtr;  framePtr != NULL;
                framePtr = framePtr->callerVarPtr) {
            if (framePtr->level == level) {
                break;
            }
        }
        if (framePtr == NULL) {
            goto levelError;
        }

        listPtr = Tcl_NewListObj(framePtr->objc, framePtr->objv);
        Tcl_SetObjResult(interp, listPtr);
        return TCL_OK;
    }

    Tcl_WrongNumArgs(interp, 1, objv, "level ?number?");
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLibraryCmd --
 *
 *      Called to implement the "info library" command that returns the
 *      library directory for the Tcl installation. Handles the following
 *      syntax:
 *
 *          info library
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLibraryCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *libDirName;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "library");
        return TCL_ERROR;
    }

    libDirName = Tcl_GetVar(interp, "tcl_library", TCL_GLOBAL_ONLY);
    if (libDirName != NULL) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp), libDirName, -1);
        return TCL_OK;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), 
            "no library has been specified for Tcl", -1);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLoadedCmd --
 *
 *      Called to implement the "info loaded" command that returns the
 *      packages that have been loaded into an interpreter. Handles the
 *      following syntax:
 *
 *          info loaded ?interp?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLoadedCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *interpName;
    int result;

    if ((objc != 2) && (objc != 3)) {
        Tcl_WrongNumArgs(interp, 1, objv, "loaded ?interp?");
        return TCL_ERROR;
    }

    if (objc == 2) {		/* get loaded pkgs in all interpreters */
	interpName = NULL;
    } else {			/* get pkgs just in specified interp */
	interpName = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    }
    result = TclGetLoadedPackages(interp, interpName);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLocalsCmd --
 *
 *      Called to implement the "info locals" command to return a list of
 *      local variables that match an optional pattern. Handles the
 *      following syntax:
 *
 *          info locals ?pattern?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLocalsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    Var *varPtr;
    char *varName, *pattern;
    int i, localVarCt;
    Tcl_HashTable *localVarTablePtr;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tcl_Obj *listPtr;

    if (objc == 2) {
        pattern = NULL;
    } else if (objc == 3) {
        pattern = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "locals ?pattern?");
        return TCL_ERROR;
    }
    
    if (iPtr->varFramePtr == NULL) {
        return TCL_OK;
    }
    localVarTablePtr = iPtr->varFramePtr->varTablePtr;

    /*
     * Return a list containing names of first the compiled locals (i.e. the
     * ones stored in the call frame), then the variables in the local hash
     * table (if one exists).
     */
    
    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    
    localVarCt = iPtr->varFramePtr->numCompiledLocals;
    for (i = 0, varPtr = iPtr->varFramePtr->compiledLocals;
            i < localVarCt;
	    i++, varPtr++) {
	if (!TclIsVarUndefined(varPtr)) {
	    varName = varPtr->name;
	    if ((pattern == NULL) || Tcl_StringMatch(varName, pattern)) {
		Tcl_ListObjAppendElement(interp, listPtr,
		        Tcl_NewStringObj(varName, -1));
	    }
        }
    }
    
    if (localVarTablePtr != NULL) {
	for (entryPtr = Tcl_FirstHashEntry(localVarTablePtr, &search);
	        entryPtr != NULL;
                entryPtr = Tcl_NextHashEntry(&search)) {
	    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
	    if (!TclIsVarUndefined(varPtr) && !TclIsVarLink(varPtr)) {
		varName = Tcl_GetHashKey(localVarTablePtr, entryPtr);
		if ((pattern == NULL)
		        || Tcl_StringMatch(varName, pattern)) {
		    Tcl_ListObjAppendElement(interp, listPtr,
			    Tcl_NewStringObj(varName, -1));
		}
	    }
	}
    }
    
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoNameOfExecutableCmd --
 *
 *      Called to implement the "info nameofexecutable" command that returns
 *      the name of the binary file running this application. Handles the
 *      following syntax:
 *
 *          info nameofexecutable
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoNameOfExecutableCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "nameofexecutable");
        return TCL_ERROR;
    }
    
    if (tclExecutableName != NULL) {
	Tcl_SetStringObj(Tcl_GetObjResult(interp), tclExecutableName, -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoPatchLevelCmd --
 *
 *      Called to implement the "info patchlevel" command that returns the
 *      default value for an argument to a procedure. Handles the following
 *      syntax:
 *
 *          info patchlevel
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoPatchLevelCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *patchlevel;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "patchlevel");
        return TCL_ERROR;
    }

    patchlevel = Tcl_GetVar(interp, "tcl_patchLevel",
            (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG));
    if (patchlevel != NULL) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp), patchlevel, -1);
        return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoProcsCmd --
 *
 *      Called to implement the "info procs" command that returns the
 *      procedures in the current namespace that match an optional pattern.
 *      Handles the following syntax:
 *
 *          info procs ?pattern?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoProcsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *cmdName, *pattern;
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Command *cmdPtr;
    Tcl_Obj *listPtr;

    if (objc == 2) {
        pattern = NULL;
    } else if (objc == 3) {
        pattern = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "procs ?pattern?");
        return TCL_ERROR;
    }

    /*
     * Scan through the current namespace's command table and return a list
     * of all procs that match the pattern.
     */
    
    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (entryPtr = Tcl_FirstHashEntry(&currNsPtr->cmdTable, &search);
            entryPtr != NULL;
            entryPtr = Tcl_NextHashEntry(&search)) {
        cmdName = Tcl_GetHashKey(&currNsPtr->cmdTable, entryPtr);
        cmdPtr = (Command *) Tcl_GetHashValue(entryPtr);
        if (TclIsProc(cmdPtr)) {
            if ((pattern == NULL) || Tcl_StringMatch(cmdName, pattern)) {
                Tcl_ListObjAppendElement(interp, listPtr,
		        Tcl_NewStringObj(cmdName, -1));
            }
        }
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoScriptCmd --
 *
 *      Called to implement the "info script" command that returns the
 *      script file that is currently being evaluated. Handles the
 *      following syntax:
 *
 *          info script
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoScriptCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "script");
        return TCL_ERROR;
    }

    if (iPtr->scriptFile != NULL) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp), iPtr->scriptFile, -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoSharedlibCmd --
 *
 *      Called to implement the "info sharedlibextension" command that
 *      returns the file extension used for shared libraries. Handles the
 *      following syntax:
 *
 *          info sharedlibextension
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoSharedlibCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "sharedlibextension");
        return TCL_ERROR;
    }
    
#ifdef TCL_SHLIB_EXT
    Tcl_SetStringObj(Tcl_GetObjResult(interp), TCL_SHLIB_EXT, -1);
#endif
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoTclVersionCmd --
 *
 *      Called to implement the "info tclversion" command that returns the
 *      version number for this Tcl library. Handles the following syntax:
 *
 *          info tclversion
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoTclVersionCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *version;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "tclversion");
        return TCL_ERROR;
    }

    version = Tcl_GetVar(interp, "tcl_version",
        (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG));
    if (version != NULL) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp), version, -1);
        return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoVarsCmd --
 *
 *	Called to implement the "info vars" command that returns the
 *	list of variables in the interpreter that match an optional pattern.
 *	The pattern, if any, consists of an optional sequence of namespace
 *	names separated by "::" qualifiers, which is followed by a
 *	glob-style pattern that restricts which variables are returned.
 *	Handles the following syntax:
 *
 *          info vars ?pattern?
 *
 * Results:
 *      Returns TCL_OK is successful and TCL_ERROR is there is an error.
 *
 * Side effects:
 *      Returns a result in the interpreter's result object. If there is
 *	an error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoVarsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    char *varName, *pattern, *simplePattern;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Var *varPtr, *localVarPtr;
    Namespace *nsPtr;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    Namespace *currNsPtr   = (Namespace *) Tcl_GetCurrentNamespace(interp);
    Tcl_Obj *listPtr, *elemObjPtr;
    int specificNsInPattern = 0;  /* Init. to avoid compiler warning. */
    int i, result;

    /*
     * Get the pattern and find the "effective namespace" in which to
     * list variables. We only use this effective namespace if there's
     * no active Tcl procedure frame.
     */

    if (objc == 2) {
        simplePattern = NULL;
	nsPtr = currNsPtr;
	specificNsInPattern = 0;
    } else if (objc == 3) {
	/*
	 * From the pattern, get the effective namespace and the simple
	 * pattern (no namespace qualifiers or ::'s) at the end. If an
	 * error was found while parsing the pattern, return it. Otherwise,
	 * if the namespace wasn't found, just leave nsPtr NULL: we will
	 * return an empty list since no variables there can be found.
	 */

	Namespace *dummy1NsPtr, *dummy2NsPtr;

        pattern = Tcl_GetStringFromObj(objv[2], (int *) NULL);
	result = TclGetNamespaceForQualName(interp, pattern,
		(Namespace *) NULL, /*flags*/ TCL_LEAVE_ERR_MSG,
		&nsPtr, &dummy1NsPtr, &dummy2NsPtr, &simplePattern);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nsPtr != NULL) {	/* we successfully found the pattern's ns */
	    specificNsInPattern = (strcmp(simplePattern, pattern) != 0);
	}
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "vars ?pattern?");
        return TCL_ERROR;
    }

    /*
     * If the namespace specified in the pattern wasn't found, just return.
     */

    if (nsPtr == NULL) {
	return TCL_OK;
    }
    
    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    
    if ((iPtr->varFramePtr == NULL)
	    || !iPtr->varFramePtr->isProcCallFrame
	    || specificNsInPattern) {
	/*
	 * There is no frame pointer, the frame pointer was pushed only
	 * to activate a namespace, or we are in a procedure call frame
	 * but a specific namespace was specified. Create a list containing
	 * only the variables in the effective namespace's variable table.
	 */
	
	entryPtr = Tcl_FirstHashEntry(&nsPtr->varTable, &search);
	while (entryPtr != NULL) {
	    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
	    if (!TclIsVarUndefined(varPtr)) {
		varName = Tcl_GetHashKey(&nsPtr->varTable, entryPtr);
		if ((simplePattern == NULL)
	                || Tcl_StringMatch(varName, simplePattern)) {
		    if (specificNsInPattern) {
			elemObjPtr = Tcl_NewObj();
			Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr,
			        elemObjPtr);
		    } else {
			elemObjPtr = Tcl_NewStringObj(varName, -1);
		    }
		    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		}
	    }
	    entryPtr = Tcl_NextHashEntry(&search);
	}

	/*
	 * If the effective namespace isn't the global :: namespace, and a
	 * specific namespace wasn't requested in the pattern (i.e., the
	 * pattern only specifies variable names), then add in all global ::
	 * variables that match the simple pattern. Of course, add in only
	 * those variables that aren't hidden by a variable in the effective
	 * namespace.
	 */

	if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
	    entryPtr = Tcl_FirstHashEntry(&globalNsPtr->varTable, &search);
	    while (entryPtr != NULL) {
		varPtr = (Var *) Tcl_GetHashValue(entryPtr);
		if (!TclIsVarUndefined(varPtr)) {
		    varName = Tcl_GetHashKey(&globalNsPtr->varTable,
			    entryPtr);
		    if ((simplePattern == NULL)
	                    || Tcl_StringMatch(varName, simplePattern)) {
			if (Tcl_FindHashEntry(&nsPtr->varTable, varName) == NULL) {
			    Tcl_ListObjAppendElement(interp, listPtr,
			            Tcl_NewStringObj(varName, -1));
			}
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }
	}
    } else {
	/*
	 * We're in a local call frame and no specific namespace was
	 * specific. Create a list that starts with the compiled locals
	 * (i.e. the ones stored in the call frame).
	 */

	CallFrame *varFramePtr = iPtr->varFramePtr;
        int localVarCt = varFramePtr->numCompiledLocals;
	Tcl_HashTable *varTablePtr = varFramePtr->varTablePtr;
	
        for (i = 0, localVarPtr = iPtr->varFramePtr->compiledLocals;
                i < localVarCt;
                i++, localVarPtr++) {
            if (!TclIsVarUndefined(localVarPtr)) {
                varName = localVarPtr->name;
                if ((simplePattern == NULL)
		        || Tcl_StringMatch(varName, simplePattern)) {
                    Tcl_ListObjAppendElement(interp, listPtr,
			    Tcl_NewStringObj(varName, -1));
                }
            }
        }

	/*
	 * Now add in the variables in the call frame's variable hash
	 * table (if one exists).
	 */

	if (varTablePtr != NULL) {
	    for (entryPtr = Tcl_FirstHashEntry(varTablePtr, &search);
		    entryPtr != NULL;
		    entryPtr = Tcl_NextHashEntry(&search)) {
		varPtr = (Var *) Tcl_GetHashValue(entryPtr);
		if (!TclIsVarUndefined(varPtr)) {
		    varName = Tcl_GetHashKey(varTablePtr, entryPtr);
		    if ((simplePattern == NULL)
		            || Tcl_StringMatch(varName, simplePattern)) {
			Tcl_ListObjAppendElement(interp, listPtr,
				Tcl_NewStringObj(varName, -1));
		    }
		}
	    }
	}
    }
    
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_JoinObjCmd --
 *
 *	This procedure is invoked to process the "join" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_JoinObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* The argument objects. */
{
    char *joinString, *bytes;
    int joinLength, listLen, length, i, result;
    Tcl_Obj **elemPtrs;

    if (objc == 2) {
	joinString = " ";
	joinLength = 1;
    } else if (objc == 3) {
	joinString = Tcl_GetStringFromObj(objv[2], &joinLength);
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "list ?joinString?");
	return TCL_ERROR;
    }

    /*
     * Make sure the list argument is a list object and get its length and
     * a pointer to its array of element pointers.
     */

    result = Tcl_ListObjGetElements(interp, objv[1], &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Now concatenate strings to form the "joined" result. We append
     * directly into the interpreter's result object.
     */

    for (i = 0;  i < listLen;  i++) {
	bytes = Tcl_GetStringFromObj(elemPtrs[i], &length);
	if (i > 0) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), joinString,
		    bytes, (char *) NULL);
	} else {
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), bytes, length);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LindexObjCmd --
 *
 *	This object-based procedure is invoked to process the "lindex" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
int
Tcl_LindexObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *listPtr;
    Tcl_Obj **elemPtrs;
    int listLen, index, result;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "list index");
	return TCL_ERROR;
    }

    /*
     * Convert the first argument to a list if necessary.
     */

    listPtr = objv[1];
    result = Tcl_ListObjGetElements(interp, listPtr, &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Get the index from objv[2].
     */

    result = TclGetIntForIndex(interp, objv[2], /*endValue*/ (listLen - 1),
	    &index);
    if (result != TCL_OK) {
	return result;
    }
    if ((index < 0) || (index >= listLen)) {
	/*
	 * The index is out of range: the result is an empty string object.
	 */
	
	return TCL_OK;
    }

    /*
     * Make sure listPtr still refers to a list object. It might have been
     * converted to an int above if the argument objects were shared.
     */

    if (listPtr->typePtr != &tclListType) {
	result = Tcl_ListObjGetElements(interp, listPtr, &listLen,
	        &elemPtrs);
	if (result != TCL_OK) {
	    return result;
	}
    }

    /*
     * Set the interpreter's object result to the index-th list element.
     */

    Tcl_SetObjResult(interp, elemPtrs[index]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinsertObjCmd --
 *
 *	This object-based procedure is invoked to process the "linsert" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A new Tcl list object formed by inserting zero or more elements 
 *	into a list.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LinsertObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    register int objc;		/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *listPtr, *resultPtr;
    int index, isDuplicate;
    int result;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "list index element ?element ...?");
	return TCL_ERROR;
    }

    /*
     * Get the index first since, if a conversion to int is needed, it
     * will invalidate the list's internal representation.
     */

    result = TclGetIntForIndex(interp, objv[2], /*endValue*/ INT_MAX,
	    &index);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * If the list object is unshared we can modify it directly. Otherwise
     * we create a copy to modify: this is "copy on write". We create the
     * duplicate directly in the interpreter's object result.
     */
    
    listPtr = objv[1];
    isDuplicate = 0;
    if (Tcl_IsShared(listPtr)) {
	Tcl_ResetResult(interp);
	resultPtr = Tcl_GetObjResult(interp);
	if (listPtr->typePtr != NULL) {
	    Tcl_InvalidateStringRep(resultPtr);
	    listPtr->typePtr->dupIntRepProc(listPtr, resultPtr);
	} else if (listPtr->bytes != NULL) {
	    int len = listPtr->length;
	    
	    TclInitStringRep(resultPtr, listPtr->bytes, len);
	}
	listPtr = resultPtr;
	isDuplicate = 1;
    }
    
    if ((objc == 4) && (index == INT_MAX)) {
	/*
	 * Special case: insert one element at the end of the list.
	 */

	result = Tcl_ListObjAppendElement(interp, listPtr, objv[3]);
    } else if (objc > 3) {
	result = Tcl_ListObjReplace(interp, listPtr, index, 0,
				    (objc-3), &(objv[3]));
    }
    if (result != TCL_OK) {
	return result;
    }
    
    /*
     * Set the interpreter's object result.
     */

    if (!isDuplicate) {
	Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjCmd --
 *
 *	This procedure is invoked to process the "list" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ListObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    register int objc;			/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* The argument objects. */
{
    /*
     * If there are no list elements, the result is an empty object.
     * Otherwise modify the interpreter's result object to be a list object.
     */
    
    if (objc > 1) {
	Tcl_SetListObj(Tcl_GetObjResult(interp), (objc-1), &(objv[1]));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LlengthObjCmd --
 *
 *	This object-based procedure is invoked to process the "llength" Tcl
 *	command.  See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LlengthObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int listLen, result;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "list");
	return TCL_ERROR;
    }

    result = Tcl_ListObjLength(interp, objv[1], &listLen);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Set the interpreter's object result to an integer object holding the
     * length. 
     */

    Tcl_SetIntObj(Tcl_GetObjResult(interp), listLen);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LrangeObjCmd --
 *
 *	This procedure is invoked to process the "lrange" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LrangeObjCmd(notUsed, interp, objc, objv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *listPtr;
    Tcl_Obj **elemPtrs;
    int listLen, first, last, numElems, result;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "list first last");
	return TCL_ERROR;
    }

    /*
     * Make sure the list argument is a list object and get its length and
     * a pointer to its array of element pointers.
     */

    listPtr = objv[1];
    result = Tcl_ListObjGetElements(interp, listPtr, &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Get the first and last indexes.
     */

    result = TclGetIntForIndex(interp, objv[2], /*endValue*/ (listLen - 1),
	    &first);
    if (result != TCL_OK) {
	return result;
    }
    if (first < 0) {
	first = 0;
    }

    result = TclGetIntForIndex(interp, objv[3], /*endValue*/ (listLen - 1),
	    &last);
    if (result != TCL_OK) {
	return result;
    }
    if (last >= listLen) {
	last = (listLen - 1);
    }
    
    if (first > last) {
	return TCL_OK;		/* the result is an empty object */
    }

    /*
     * Make sure listPtr still refers to a list object. It might have been
     * converted to an int above if the argument objects were shared.
     */  

    if (listPtr->typePtr != &tclListType) {
        result = Tcl_ListObjGetElements(interp, listPtr, &listLen,
                &elemPtrs);
        if (result != TCL_OK) {
            return result;
        }
    }

    /*
     * Extract a range of fields. We modify the interpreter's result object
     * to be a list object containing the specified elements.
     */

    numElems = (last - first + 1);
    Tcl_SetListObj(Tcl_GetObjResult(interp), numElems, &(elemPtrs[first]));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LreplaceObjCmd --
 *
 *	This object-based procedure is invoked to process the "lreplace" 
 *	Tcl command. See the user documentation for details on what it does.
 *
 * Results:
 *	A new Tcl list object formed by replacing zero or more elements of
 *	a list.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LreplaceObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Tcl_Obj *listPtr;
    int createdNewObj, first, last, listLen, numToDelete, result;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"list first last ?element element ...?");
	return TCL_ERROR;
    }

    /*
     * If the list object is unshared we can modify it directly, otherwise
     * we create a copy to modify: this is "copy on write".
     */
    
    listPtr = objv[1];
    createdNewObj = 0;
    if (Tcl_IsShared(listPtr)) {
	listPtr = Tcl_DuplicateObj(listPtr);
	createdNewObj = 1;
    }
    result = Tcl_ListObjLength(interp, listPtr, &listLen);
    if (result != TCL_OK) {
        errorReturn:
	if (createdNewObj) {
	    Tcl_DecrRefCount(listPtr); /* free unneeded obj */
	}
	return result;
    }

    /*
     * Get the first and last indexes.
     */

    result = TclGetIntForIndex(interp, objv[2], /*endValue*/ (listLen - 1),
	    &first);
    if (result != TCL_OK) {
	goto errorReturn;
    }

    result = TclGetIntForIndex(interp, objv[3], /*endValue*/ (listLen - 1),
	    &last);
    if (result != TCL_OK) {
	goto errorReturn;
    }

    if (first < 0)  {
    	first = 0;
    }
    if (first >= listLen) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"list doesn't contain element ",
		Tcl_GetStringFromObj(objv[2], (int *) NULL), (int *) NULL);
	result = TCL_ERROR;
	goto errorReturn;
    }
    if (last >= listLen) {
    	last = (listLen - 1);
    }
    if (first <= last) {
	numToDelete = (last - first + 1);
    } else {
	numToDelete = 0;
    }

    if (objc > 4) {
	result = Tcl_ListObjReplace(interp, listPtr, first, numToDelete,
	        (objc-4), &(objv[4]));
    } else {
	result = Tcl_ListObjReplace(interp, listPtr, first, numToDelete,
		0, NULL);
    }
    if (result != TCL_OK) {
	goto errorReturn;
    }

    /*
     * Set the interpreter's object result. 
     */

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsearchObjCmd --
 *
 *	This procedure is invoked to process the "lsearch" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LsearchObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument values. */
{
#define EXACT	0
#define GLOB	1
#define REGEXP	2
    char *bytes, *patternBytes;
    int i, match, mode, index, result, listLen, length, elemLen;
    Tcl_Obj **elemPtrs;
    static char *switches[] =
	    {"-exact", "-glob", "-regexp", (char *) NULL};

    mode = GLOB;
    if (objc == 4) {
	if (Tcl_GetIndexFromObj(interp, objv[1], switches,
		"search mode", 0, &mode) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?mode? list pattern");
	return TCL_ERROR;
    }

    /*
     * Make sure the list argument is a list object and get its length and
     * a pointer to its array of element pointers.
     */

    result = Tcl_ListObjGetElements(interp, objv[objc-2], &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    patternBytes = Tcl_GetStringFromObj(objv[objc-1], &length);
 
    index = -1;
    for (i = 0; i < listLen; i++) {
	match = 0;
	bytes = Tcl_GetStringFromObj(elemPtrs[i], &elemLen);
	switch (mode) {
	    case EXACT:
		if (length == elemLen) {
		    match = (memcmp(bytes, patternBytes,
			    (size_t) length) == 0);
		}
		break;
	    case GLOB:
		/*
		 * WARNING: will not work with data containing NULLs.
		 */
		match = Tcl_StringMatch(bytes, patternBytes);
		break;
	    case REGEXP:
		/*
		 * WARNING: will not work with data containing NULLs.
		 */
		match = Tcl_RegExpMatch(interp, bytes, patternBytes);
		if (match < 0) {
		    return TCL_ERROR;
		}
		break;
	}
	if (match) {
	    index = i;
	    break;
	}
    }

    Tcl_SetIntObj(Tcl_GetObjResult(interp), index);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsortObjCmd --
 *
 *	This procedure is invoked to process the "lsort" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LsortObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument values. */
{
    int i, index, dummy;
    Tcl_Obj *resultPtr;
    int length;
    Tcl_Obj *cmdPtr, **listObjPtrs;
    SortElement *elementArray;
    SortElement *elementPtr;        
    SortInfo sortInfo;                  /* Information about this sort that
                                         * needs to be passed to the 
                                         * comparison function */
    static char *switches[] =
	    {"-ascii", "-command", "-decreasing", "-dictionary",
	    "-increasing", "-index", "-integer", "-real", (char *) NULL};

    resultPtr = Tcl_GetObjResult(interp);
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? list");
	return TCL_ERROR;
    }

    /*
     * Parse arguments to set up the mode for the sort.
     */

    sortInfo.isIncreasing = 1;
    sortInfo.sortMode = SORTMODE_ASCII;
    sortInfo.index = -1;
    sortInfo.interp = interp;
    sortInfo.resultCode = TCL_OK;
    cmdPtr = NULL;
    for (i = 1; i < objc-1; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], switches, "option", 0, &index)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	    case 0:			/* -ascii */
		sortInfo.sortMode = SORTMODE_ASCII;
		break;
	    case 1:			/* -command */
		if (i == (objc-2)) {
		    Tcl_AppendToObj(resultPtr,
			    "\"-command\" option must be followed by comparison command",
			    -1);
		    return TCL_ERROR;
		}
		sortInfo.sortMode = SORTMODE_COMMAND;
		cmdPtr = objv[i+1];
		i++;
		break;
	    case 2:			/* -decreasing */
		sortInfo.isIncreasing = 0;
		break;
	    case 3:			/* -dictionary */
		sortInfo.sortMode = SORTMODE_DICTIONARY;
		break;
	    case 4:			/* -increasing */
		sortInfo.isIncreasing = 1;
		break;
	    case 5:			/* -index */
		if (i == (objc-2)) {
		    Tcl_AppendToObj(resultPtr,
			    "\"-index\" option must be followed by list index",
			    -1);
		    return TCL_ERROR;
		}
		if (Tcl_GetLongFromObj(interp, objv[i+1], &sortInfo.index)
			!= TCL_OK) {
		    if (strcmp("end", Tcl_GetStringFromObj(objv[i+1], &dummy))
			    == 0) {
			sortInfo.index = -2;
		    } else {
			return TCL_ERROR;
		    }
		}
		cmdPtr = objv[i+1];
		i++;
		break;
	    case 6:			/* -integer */
		sortInfo.sortMode = SORTMODE_INTEGER;
		break;
	    case 7:			/* -real */
		sortInfo.sortMode = SORTMODE_REAL;
		break;
	}
    }
    if (sortInfo.sortMode == SORTMODE_COMMAND) {
	Tcl_DStringInit(&sortInfo.compareCmd);
	Tcl_DStringAppend(&sortInfo.compareCmd,
		Tcl_GetStringFromObj(cmdPtr, &dummy), -1);
    }

    sortInfo.resultCode = Tcl_ListObjGetElements(interp, objv[objc-1],
	    &length, &listObjPtrs);
    if (sortInfo.resultCode != TCL_OK) {
	goto done;
    }
    if (length <= 0) {
        return TCL_OK;
    }
    elementArray = (SortElement *) ckalloc(length * sizeof(SortElement));
    for (i=0; i < length; i++){
	elementArray[i].objPtr = listObjPtrs[i];
	elementArray[i].nextPtr = &elementArray[i+1];
    }
    elementArray[length-1].nextPtr = NULL;
    elementPtr = MergeSort(elementArray, &sortInfo);
    if (sortInfo.resultCode == TCL_OK) {
	/*
	 * Note: must clear the interpreter's result object: it could
	 * have been set by the -command script.
	 */

	Tcl_ResetResult(interp);
	resultPtr = Tcl_GetObjResult(interp);
	for (; elementPtr != NULL; elementPtr = elementPtr->nextPtr){
	    Tcl_ListObjAppendElement(interp, resultPtr, elementPtr->objPtr);
	}
    }
    ckfree((char*) elementArray);

    done:
    if (sortInfo.sortMode == SORTMODE_COMMAND) {
	Tcl_DStringFree(&sortInfo.compareCmd);
    }
    return sortInfo.resultCode;
}

/*
 *----------------------------------------------------------------------
 *
 * MergeSort -
 *
 *	This procedure sorts a linked list of SortElement structures
 *	use the merge-sort algorithm.
 *
 * Results:
 *      A pointer to the head of the list after sorting is returned.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */

static SortElement *
MergeSort(headPtr, infoPtr)
    SortElement *headPtr;               /* First element on the list */
    SortInfo *infoPtr;                  /* Information needed by the
                                         * comparison operator */
{
    /*
     * The subList array below holds pointers to temporary lists built
     * during the merge sort.  Element i of the array holds a list of
     * length 2**i.
     */

#   define NUM_LISTS 30
    SortElement *subList[NUM_LISTS];
    SortElement *elementPtr;
    int i;

    for(i = 0; i < NUM_LISTS; i++){
        subList[i] = NULL;
    }
    while (headPtr != NULL) {
	elementPtr = headPtr;
	headPtr = headPtr->nextPtr;
	elementPtr->nextPtr = 0;
	for (i = 0; (i < NUM_LISTS) && (subList[i] != NULL); i++){
	    elementPtr = MergeLists(subList[i], elementPtr, infoPtr);
	    subList[i] = NULL;
	}
	if (i >= NUM_LISTS) {
	    i = NUM_LISTS-1;
	}
	subList[i] = elementPtr;
    }
    elementPtr = NULL;
    for (i = 0; i < NUM_LISTS; i++){
        elementPtr = MergeLists(subList[i], elementPtr, infoPtr);
    }
    return elementPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MergeLists -
 *
 *	This procedure combines two sorted lists of SortElement structures
 *	into a single sorted list.
 *
 * Results:
 *      The unified list of SortElement structures.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */

static SortElement *
MergeLists(leftPtr, rightPtr, infoPtr)
    SortElement *leftPtr;               /* First list to be merged; may be
					 * NULL. */
    SortElement *rightPtr;              /* Second list to be merged; may be
					 * NULL. */
    SortInfo *infoPtr;                  /* Information needed by the
                                         * comparison operator. */
{
    SortElement *headPtr;
    SortElement *tailPtr;

    if (leftPtr == NULL) {
        return rightPtr;
    }
    if (rightPtr == NULL) {
        return leftPtr;
    }
    if (SortCompare(leftPtr->objPtr, rightPtr->objPtr, infoPtr) > 0) {
	tailPtr = rightPtr;
	rightPtr = rightPtr->nextPtr;
    } else {
	tailPtr = leftPtr;
	leftPtr = leftPtr->nextPtr;
    }
    headPtr = tailPtr;
    while ((leftPtr != NULL) && (rightPtr != NULL)) {
	if (SortCompare(leftPtr->objPtr, rightPtr->objPtr, infoPtr) > 0) {
	    tailPtr->nextPtr = rightPtr;
	    tailPtr = rightPtr;
	    rightPtr = rightPtr->nextPtr;
	} else {
	    tailPtr->nextPtr = leftPtr;
	    tailPtr = leftPtr;
	    leftPtr = leftPtr->nextPtr;
	}
    }
    if (leftPtr != NULL) {
       tailPtr->nextPtr = leftPtr;
    } else {
       tailPtr->nextPtr = rightPtr;
    }
    return headPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SortCompare --
 *
 *	This procedure is invoked by MergeLists to determine the proper
 *	ordering between two elements.
 *
 * Results:
 *      A negative results means the the first element comes before the
 *      second, and a positive results means that the second element
 *      should come first.  A result of zero means the two elements
 *      are equal and it doesn't matter which comes first.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */

static int
SortCompare(objPtr1, objPtr2, infoPtr)
    Tcl_Obj *objPtr1, *objPtr2;		/* Values to be compared. */
    SortInfo *infoPtr;                  /* Information passed from the
                                         * top-level "lsort" command */
{
    int order, dummy, listLen, index;
    Tcl_Obj *objPtr;
    char buffer[30];

    order = 0;
    if (infoPtr->resultCode != TCL_OK) {
	/*
	 * Once an error has occurred, skip any future comparisons
	 * so as to preserve the error message in sortInterp->result.
	 */

	return order;
    }
    if (infoPtr->index != -1) {
	/*
	 * The "-index" option was specified.  Treat each object as a
	 * list, extract the requested element from each list, and
	 * compare the elements, not the lists.  The special index "end"
	 * is signaled here with a large negative index.
	 */

	if (Tcl_ListObjLength(infoPtr->interp, objPtr1, &listLen) != TCL_OK) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (infoPtr->index < -1) {
	    index = listLen - 1;
	} else {
	    index = infoPtr->index;
	}

	if (Tcl_ListObjIndex(infoPtr->interp, objPtr1, index, &objPtr)
		!= TCL_OK) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (objPtr == NULL) {
	    objPtr = objPtr1;
	    missingElement:
	    sprintf(buffer, "%ld", infoPtr->index);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(infoPtr->interp),
			"element ", buffer, " missing from sublist \"",
			Tcl_GetStringFromObj(objPtr, (int *) NULL),
			"\"", (char *) NULL);
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	objPtr1 = objPtr;

	if (Tcl_ListObjLength(infoPtr->interp, objPtr2, &listLen) != TCL_OK) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (infoPtr->index < -1) {
	    index = listLen - 1;
	} else {
	    index = infoPtr->index;
	}

	if (Tcl_ListObjIndex(infoPtr->interp, objPtr2, index, &objPtr)
		!= TCL_OK) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (objPtr == NULL) {
	    objPtr = objPtr2;
	    goto missingElement;
	}
	objPtr2 = objPtr;
    }
    if (infoPtr->sortMode == SORTMODE_ASCII) {
	order = strcmp(Tcl_GetStringFromObj(objPtr1, &dummy),
		Tcl_GetStringFromObj(objPtr2, &dummy));
    } else if (infoPtr->sortMode == SORTMODE_DICTIONARY) {
	order = DictionaryCompare(
		Tcl_GetStringFromObj(objPtr1, &dummy),
		Tcl_GetStringFromObj(objPtr2, &dummy));
    } else if (infoPtr->sortMode == SORTMODE_INTEGER) {
	int a, b;

	if ((Tcl_GetIntFromObj(infoPtr->interp, objPtr1, &a) != TCL_OK)
		|| (Tcl_GetIntFromObj(infoPtr->interp, objPtr2, &b)
		!= TCL_OK)) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (a > b) {
	    order = 1;
	} else if (b > a) {
	    order = -1;
	}
    } else if (infoPtr->sortMode == SORTMODE_REAL) {
	double a, b;

	if ((Tcl_GetDoubleFromObj(infoPtr->interp, objPtr1, &a) != TCL_OK)
	      || (Tcl_GetDoubleFromObj(infoPtr->interp, objPtr2, &b)
		      != TCL_OK)) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (a > b) {
	    order = 1;
	} else if (b > a) {
	    order = -1;
	}
    } else {
	int oldLength;

	/*
	 * Generate and evaluate a command to determine which string comes
	 * first.
	 */

	oldLength = Tcl_DStringLength(&infoPtr->compareCmd);
	Tcl_DStringAppendElement(&infoPtr->compareCmd,
		Tcl_GetStringFromObj(objPtr1, &dummy));
	Tcl_DStringAppendElement(&infoPtr->compareCmd,
		Tcl_GetStringFromObj(objPtr2, &dummy));
	infoPtr->resultCode = Tcl_Eval(infoPtr->interp, 
		Tcl_DStringValue(&infoPtr->compareCmd));
	Tcl_DStringTrunc(&infoPtr->compareCmd, oldLength);
	if (infoPtr->resultCode != TCL_OK) {
	    Tcl_AddErrorInfo(infoPtr->interp,
		    "\n    (-compare command)");
	    return order;
	}

	/*
	 * Parse the result of the command.
	 */

	if (Tcl_GetIntFromObj(infoPtr->interp,
		Tcl_GetObjResult(infoPtr->interp), &order) != TCL_OK) {
	    Tcl_ResetResult(infoPtr->interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(infoPtr->interp),
		    "-compare command returned non-numeric result", -1);
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
    }
    if (!infoPtr->isIncreasing) {
	order = -order;
    }
    return order;
}

/*
 *----------------------------------------------------------------------
 *
 * DictionaryCompare
 *
 *	This function compares two strings as if they were being used in
 *	an index or card catalog.  The case of alphabetic characters is
 *	ignored, except to break ties.  Thus "B" comes before "b" but
 *	after "a".  Also, integers embedded in the strings compare in
 *	numerical order.  In other words, "x10y" comes after "x9y", not
 *      before it as it would when using strcmp().
 *
 * Results:
 *      A negative result means that the first element comes before the
 *      second, and a positive result means that the second element
 *      should come first.  A result of zero means the two elements
 *      are equal and it doesn't matter which comes first.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DictionaryCompare(left, right)
    char *left, *right;          /* The strings to compare */
{
    int diff, zeros;
    int secondaryDiff = 0;

    while (1) {
	if (isdigit(UCHAR(*right)) && isdigit(UCHAR(*left))) {
	    /*
	     * There are decimal numbers embedded in the two
	     * strings.  Compare them as numbers, rather than
	     * strings.  If one number has more leading zeros than
	     * the other, the number with more leading zeros sorts
	     * later, but only as a secondary choice.
	     */

	    zeros = 0;
	    while (*right == '0') {
		right++;
		zeros--;
	    }
	    while (*left == '0') {
		left++;
		zeros++;
	    }
	    if (secondaryDiff == 0) {
		secondaryDiff = zeros;
	    }

	    /*
	     * The code below compares the numbers in the two
	     * strings without ever converting them to integers.  It
	     * does this by first comparing the lengths of the
	     * numbers and then comparing the digit values.
	     */

	    diff = 0;
	    while (1) {
		if (diff == 0) {
		    diff = *left - *right;
		}
		right++;
		left++;
		if (!isdigit(UCHAR(*right))) {
		    if (isdigit(UCHAR(*left))) {
			return 1;
		    } else {
			/*
			 * The two numbers have the same length. See
			 * if their values are different.
			 */

			if (diff != 0) {
			    return diff;
			}
			break;
		    }
		} else if (!isdigit(UCHAR(*left))) {
		    return -1;
		}
	    }
	    continue;
	}
        diff = *left - *right;
        if (diff) {
            if (isupper(UCHAR(*left)) && islower(UCHAR(*right))) {
                diff = tolower(*left) - *right;
                if (diff) {
		    return diff;
                } else if (secondaryDiff == 0) {
		    secondaryDiff = -1;
                }
            } else if (isupper(UCHAR(*right)) && islower(UCHAR(*left))) {
                diff = *left - tolower(UCHAR(*right));
                if (diff) {
		    return diff;
                } else if (secondaryDiff == 0) {
		    secondaryDiff = 1;
                }
            } else {
                return diff;
            }
        }
        if (*left == 0) {
	    break;
	}
        left++;
        right++;
    }
    if (diff == 0) {
	diff = secondaryDiff;
    }
    return diff;
}
