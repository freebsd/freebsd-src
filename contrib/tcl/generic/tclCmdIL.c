/* 
 * tclCmdIL.c --
 *
 *	This file contains the top-level command routines for most of
 *	the Tcl built-in commands whose names begin with the letters
 *	I through L.  It contains only commands in the generic core
 *	(i.e. those that don't depend much upon UNIX facilities).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCmdIL.c 1.120 96/07/10 17:16:03
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
 * The variables below are used to implement the "lsort" command.
 * Unfortunately, this use of static variables prevents "lsort"
 * from being thread-safe, but there's no alternative given the
 * current implementation of qsort.  In a threaded environment
 * these variables should be made thread-local if possible, or else
 * "lsort" needs internal mutual exclusion.
 */

static Tcl_Interp *sortInterp = NULL;	/* Interpreter for "lsort" command. 
					 * NULL means no lsort is active. */
static enum {ASCII, INTEGER, REAL, COMMAND} sortMode;
					/* Mode for sorting: compare as strings,
					 * compare as numbers, or call
					 * user-defined command for
					 * comparison. */
static Tcl_DString sortCmd;		/* Holds command if mode is COMMAND.
					 * pre-initialized to hold base of
					 * command. */
static int sortIncreasing;		/* 0 means sort in decreasing order,
					 * 1 means increasing order. */
static int sortCode;			/* Anything other than TCL_OK means a
					 * problem occurred while sorting; this
					 * executing a comparison command, so
					 * the sort was aborted. */

/*
 * Forward declarations for procedures defined in this file:
 */

static int		SortCompareProc _ANSI_ARGS_((CONST VOID *first,
			    CONST VOID *second));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IfCmd --
 *
 *	This procedure is invoked to process the "if" Tcl command.
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
    sprintf(newString, "%d", value);
    result = Tcl_SetVar(interp, argv[1], newString, TCL_LEAVE_ERR_MSG);
    if (result == NULL) {
	return TCL_ERROR;
    }
    interp->result = result;
    return TCL_OK; 
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InfoCmd --
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
Tcl_InfoCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Interp *iPtr = (Interp *) interp;
    size_t length;
    int c;
    Arg *argPtr;
    Proc *procPtr;
    Var *varPtr;
    Command *cmdPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "args", length)) == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " args procname\"", (char *) NULL);
	    return TCL_ERROR;
	}
	procPtr = TclFindProc(iPtr, argv[2]);
	if (procPtr == NULL) {
	    infoNoSuchProc:
	    Tcl_AppendResult(interp, "\"", argv[2],
		    "\" isn't a procedure", (char *) NULL);
	    return TCL_ERROR;
	}
	for (argPtr = procPtr->argPtr; argPtr != NULL;
		argPtr = argPtr->nextPtr) {
	    Tcl_AppendElement(interp, argPtr->name);
	}
	return TCL_OK;
    } else if ((c == 'b') && (strncmp(argv[1], "body", length)) == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " body procname\"", (char *) NULL);
	    return TCL_ERROR;
	}
	procPtr = TclFindProc(iPtr, argv[2]);
	if (procPtr == NULL) {
	    goto infoNoSuchProc;
	}
	iPtr->result = procPtr->command;
	return TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "cmdcount", length) == 0)
	    && (length >= 2)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " cmdcount\"", (char *) NULL);
	    return TCL_ERROR;
	}
	sprintf(iPtr->result, "%d", iPtr->cmdCount);
	return TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "commands", length) == 0)
	    && (length >= 4)) {
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " commands ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&iPtr->commandTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    char *name = Tcl_GetHashKey(&iPtr->commandTable, hPtr);
	    if ((argc == 3) && !Tcl_StringMatch(name, argv[2])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	}
	return TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "complete", length) == 0)
	    && (length >= 4)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " complete command\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_CommandComplete(argv[2])) {
	    interp->result = "1";
	} else {
	    interp->result = "0";
	}
	return TCL_OK;
    } else if ((c == 'd') && (strncmp(argv[1], "default", length)) == 0) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " default procname arg varname\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	procPtr = TclFindProc(iPtr, argv[2]);
	if (procPtr == NULL) {
	    goto infoNoSuchProc;
	}
	for (argPtr = procPtr->argPtr; ; argPtr = argPtr->nextPtr) {
	    if (argPtr == NULL) {
		Tcl_AppendResult(interp, "procedure \"", argv[2],
			"\" doesn't have an argument \"", argv[3],
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    if (strcmp(argv[3], argPtr->name) == 0) {
		if (argPtr->defValue != NULL) {
		    if (Tcl_SetVar((Tcl_Interp *) iPtr, argv[4],
			    argPtr->defValue, 0) == NULL) {
			defStoreError:
			Tcl_AppendResult(interp,
				"couldn't store default value in variable \"",
				argv[4], "\"", (char *) NULL);
			return TCL_ERROR;
		    }
		    iPtr->result = "1";
		} else {
		    if (Tcl_SetVar((Tcl_Interp *) iPtr, argv[4], "", 0)
			    == NULL) {
			goto defStoreError;
		    }
		    iPtr->result = "0";
		}
		return TCL_OK;
	    }
	}
    } else if ((c == 'e') && (strncmp(argv[1], "exists", length) == 0)) {
	char *p;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " exists varName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	p = Tcl_GetVar((Tcl_Interp *) iPtr, argv[2], 0);

	/*
	 * The code below handles the special case where the name is for
	 * an array:  Tcl_GetVar will reject this since you can't read
	 * an array variable without an index.
	 */

	if (p == NULL) {
	    Tcl_HashEntry *hPtr;
	    Var *varPtr;

	    if (strchr(argv[2], '(') != NULL) {
		noVar:
		iPtr->result = "0";
		return TCL_OK;
	    }
	    if (iPtr->varFramePtr == NULL) {
		hPtr = Tcl_FindHashEntry(&iPtr->globalTable, argv[2]);
	    } else {
		hPtr = Tcl_FindHashEntry(&iPtr->varFramePtr->varTable, argv[2]);
	    }
	    if (hPtr == NULL) {
		goto noVar;
	    }
	    varPtr = (Var *) Tcl_GetHashValue(hPtr);
	    if (varPtr->flags & VAR_UPVAR) {
		varPtr = varPtr->value.upvarPtr;
	    }
	    if (!(varPtr->flags & VAR_ARRAY)) {
		goto noVar;
	    }
	}
	iPtr->result = "1";
	return TCL_OK;
    } else if ((c == 'g') && (strncmp(argv[1], "globals", length) == 0)) {
	char *name;

	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " globals ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&iPtr->globalTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    varPtr = (Var *) Tcl_GetHashValue(hPtr);
	    if (varPtr->flags & VAR_UNDEFINED) {
		continue;
	    }
	    name = Tcl_GetHashKey(&iPtr->globalTable, hPtr);
	    if ((argc == 3) && !Tcl_StringMatch(name, argv[2])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	}
	return TCL_OK;
    } else if ((c == 'h') && (strncmp(argv[1], "hostname", length) == 0)) {
	if (argc > 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " hostname\"", (char *) NULL);
	    return TCL_ERROR;
	}
        Tcl_AppendResult(interp, Tcl_GetHostName(), NULL);
        return TCL_OK;
    } else if ((c == 'l') && (strncmp(argv[1], "level", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    if (iPtr->varFramePtr == NULL) {
		iPtr->result = "0";
	    } else {
		sprintf(iPtr->result, "%d", iPtr->varFramePtr->level);
	    }
	    return TCL_OK;
	} else if (argc == 3) {
	    int level;
	    CallFrame *framePtr;

	    if (Tcl_GetInt(interp, argv[2], &level) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (level <= 0) {
		if (iPtr->varFramePtr == NULL) {
		    levelError:
		    Tcl_AppendResult(interp, "bad level \"", argv[2],
			    "\"", (char *) NULL);
		    return TCL_ERROR;
		}
		level += iPtr->varFramePtr->level;
	    }
	    for (framePtr = iPtr->varFramePtr; framePtr != NULL;
		    framePtr = framePtr->callerVarPtr) {
		if (framePtr->level == level) {
		    break;
		}
	    }
	    if (framePtr == NULL) {
		goto levelError;
	    }
	    iPtr->result = Tcl_Merge(framePtr->argc, framePtr->argv);
	    iPtr->freeProc = TCL_DYNAMIC;
	    return TCL_OK;
	}
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" level [number]\"", (char *) NULL);
	return TCL_ERROR;
    } else if ((c == 'l') && (strncmp(argv[1], "library", length) == 0)
	    && (length >= 2)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " library\"", (char *) NULL);
	    return TCL_ERROR;
	}
	interp->result = Tcl_GetVar(interp, "tcl_library", TCL_GLOBAL_ONLY);
	if (interp->result == NULL) {
	    interp->result = "no library has been specified for Tcl";
	    return TCL_ERROR;
	}
	return TCL_OK;
    } else if ((c == 'l') && (strncmp(argv[1], "loaded", length) == 0)
	    && (length >= 3)) {
	if ((argc != 2) && (argc != 3)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " loaded ?interp?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	return TclGetLoadedPackages(interp, argv[2]);
    } else if ((c == 'l') && (strncmp(argv[1], "locals", length) == 0)
	    && (length >= 3)) {
	char *name;

	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " locals ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (iPtr->varFramePtr == NULL) {
	    return TCL_OK;
	}
	for (hPtr = Tcl_FirstHashEntry(&iPtr->varFramePtr->varTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    varPtr = (Var *) Tcl_GetHashValue(hPtr);
	    if (varPtr->flags & (VAR_UNDEFINED|VAR_UPVAR)) {
		continue;
	    }
	    name = Tcl_GetHashKey(&iPtr->varFramePtr->varTable, hPtr);
	    if ((argc == 3) && !Tcl_StringMatch(name, argv[2])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	}
	return TCL_OK;
    } else if ((c == 'n') && (strncmp(argv[1], "nameofexecutable",
	    length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " nameofexecutable\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (tclExecutableName != NULL) {
	    interp->result = tclExecutableName;
	}
	return TCL_OK;
    } else if ((c == 'p') && (strncmp(argv[1], "patchlevel", length) == 0)
	    && (length >= 2)) {
	char *value;

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " patchlevel\"", (char *) NULL);
	    return TCL_ERROR;
	}
	value = Tcl_GetVar(interp, "tcl_patchLevel",
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	interp->result = value;
	return TCL_OK;
    } else if ((c == 'p') && (strncmp(argv[1], "procs", length) == 0)
	    && (length >= 2)) {
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " procs ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&iPtr->commandTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    char *name = Tcl_GetHashKey(&iPtr->commandTable, hPtr);

	    cmdPtr = (Command *) Tcl_GetHashValue(hPtr);
	    if (!TclIsProc(cmdPtr)) {
		continue;
	    }
	    if ((argc == 3) && !Tcl_StringMatch(name, argv[2])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	}
	return TCL_OK;
    } else if ((c == 's') && (strncmp(argv[1], "script", length) == 0)
	    && (length >= 2)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " script\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (iPtr->scriptFile != NULL) {
	    /*
	     * Can't depend on iPtr->scriptFile to be non-volatile:
	     * if this command is returned as the result of the script,
	     * then iPtr->scriptFile will go away.
	     */

	    Tcl_SetResult(interp, iPtr->scriptFile, TCL_VOLATILE);
	}
	return TCL_OK;
    } else if ((c == 's') && (strncmp(argv[1], "sharedlibextension",
	    length) == 0) && (length >= 2)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " sharedlibextension\"", (char *) NULL);
	    return TCL_ERROR;
	}
#ifdef TCL_SHLIB_EXT
	interp->result = TCL_SHLIB_EXT;
#endif
	return TCL_OK;
    } else if ((c == 't') && (strncmp(argv[1], "tclversion", length) == 0)) {
	char *value;

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " tclversion\"", (char *) NULL);
	    return TCL_ERROR;
	}
	value = Tcl_GetVar(interp, "tcl_version",
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	interp->result = value;
	return TCL_OK;
    } else if ((c == 'v') && (strncmp(argv[1], "vars", length)) == 0) {
	Tcl_HashTable *tablePtr;
	char *name;

	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " vars ?pattern?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (iPtr->varFramePtr == NULL) {
	    tablePtr = &iPtr->globalTable;
	} else {
	    tablePtr = &iPtr->varFramePtr->varTable;
	}
	for (hPtr = Tcl_FirstHashEntry(tablePtr, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    varPtr = (Var *) Tcl_GetHashValue(hPtr);
	    if (varPtr->flags & VAR_UNDEFINED) {
		continue;
	    }
	    name = Tcl_GetHashKey(tablePtr, hPtr);
	    if ((argc == 3) && !Tcl_StringMatch(name, argv[2])) {
		continue;
	    }
	    Tcl_AppendElement(interp, name);
	}
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be args, body, cmdcount, commands, ",
		"complete, default, ",
		"exists, globals, hostname, level, library, loaded, locals, ",
		"nameofexecutable, patchlevel, procs, script, ",
		"sharedlibextension, tclversion, or vars",
		(char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_JoinCmd --
 *
 *	This procedure is invoked to process the "join" Tcl command.
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
Tcl_JoinCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *joinString;
    char **listArgv;
    int listArgc, i;

    if (argc == 2) {
	joinString = " ";
    } else if (argc == 3) {
	joinString = argv[2];
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" list ?joinString?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (Tcl_SplitList(interp, argv[1], &listArgc, &listArgv) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 0; i < listArgc; i++) {
	if (i == 0) {
	    Tcl_AppendResult(interp, listArgv[0], (char *) NULL);
	} else  {
	    Tcl_AppendResult(interp, joinString, listArgv[i], (char *) NULL);
	}
    }
    ckfree((char *) listArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LindexCmd --
 *
 *	This procedure is invoked to process the "lindex" Tcl command.
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
Tcl_LindexCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *p, *element, *next;
    int index, size, parenthesized, result, returnLast;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" list index\"", (char *) NULL);
	return TCL_ERROR;
    }
    if ((*argv[2] == 'e') && (strncmp(argv[2], "end", strlen(argv[2])) == 0)) {
	returnLast = 1;
	index = INT_MAX;
    } else {
	returnLast = 0;
	if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (index < 0) {
	return TCL_OK;
    }
    for (p = argv[1] ; index >= 0; index--) {
	result = TclFindElement(interp, p, &element, &next, &size,
		&parenthesized);
	if (result != TCL_OK) {
	    return result;
	}
	if ((*next == 0) && returnLast) {
	    break;
	}
	p = next;
    }
    if (size == 0) {
	return TCL_OK;
    }
    if (size >= TCL_RESULT_SIZE) {
	interp->result = (char *) ckalloc((unsigned) size+1);
	interp->freeProc = TCL_DYNAMIC;
    }
    if (parenthesized) {
	memcpy((VOID *) interp->result, (VOID *) element, (size_t) size);
	interp->result[size] = 0;
    } else {
	TclCopyAndCollapse(size, element, interp->result);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinsertCmd --
 *
 *	This procedure is invoked to process the "linsert" Tcl command.
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
Tcl_LinsertCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *p, *element, savedChar;
    int i, index, count, result, size;

    if (argc < 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" list index element ?element ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if ((*argv[2] == 'e') && (strncmp(argv[2], "end", strlen(argv[2])) == 0)) {
	index = INT_MAX;
    } else if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Skip over the first "index" elements of the list, then add
     * all of those elements to the result.
     */

    size = 0;
    element = argv[1];
    for (count = 0, p = argv[1]; (count < index) && (*p != 0); count++) {
	result = TclFindElement(interp, p, &element, &p, &size, (int *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
    }
    if (*p == 0) {
	Tcl_AppendResult(interp, argv[1], (char *) NULL);
    } else {
	char *end;

	end = element+size;
	if (element != argv[1]) {
	    while ((*end != 0) && !isspace(UCHAR(*end))) {
		end++;
	    }
	}
	savedChar = *end;
	*end = 0;
	Tcl_AppendResult(interp, argv[1], (char *) NULL);
	*end = savedChar;
    }

    /*
     * Add the new list elements.
     */

    for (i = 3; i < argc; i++) {
	Tcl_AppendElement(interp, argv[i]);
    }

    /*
     * Append the remainder of the original list.
     */

    if (*p != 0) {
	Tcl_AppendResult(interp, " ", p, (char *) NULL);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListCmd --
 *
 *	This procedure is invoked to process the "list" Tcl command.
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
Tcl_ListCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc >= 2) {
	interp->result = Tcl_Merge(argc-1, argv+1);
	interp->freeProc = TCL_DYNAMIC;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LlengthCmd --
 *
 *	This procedure is invoked to process the "llength" Tcl command.
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
Tcl_LlengthCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int count, result;
    char *element, *p;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" list\"", (char *) NULL);
	return TCL_ERROR;
    }
    for (count = 0, p = argv[1]; *p != 0 ; count++) {
	result = TclFindElement(interp, p, &element, &p, (int *) NULL,
		(int *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
	if (*element == 0) {
	    break;
	}
    }
    sprintf(interp->result, "%d", count);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LrangeCmd --
 *
 *	This procedure is invoked to process the "lrange" Tcl command.
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
Tcl_LrangeCmd(notUsed, interp, argc, argv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int first, last, result;
    char *begin, *end, c, *dummy, *next;
    int count, firstIsEnd;

    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" list first last\"", (char *) NULL);
	return TCL_ERROR;
    }
    if ((*argv[2] == 'e') && (strncmp(argv[2], "end", strlen(argv[2])) == 0)) {
	firstIsEnd = 1;
	first = INT_MAX;
    } else {
	firstIsEnd = 0;
	if (Tcl_GetInt(interp, argv[2], &first) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (first < 0) {
	first = 0;
    }
    if ((*argv[3] == 'e') && (strncmp(argv[3], "end", strlen(argv[3])) == 0)) {
	last = INT_MAX;
    } else {
	if (Tcl_GetInt(interp, argv[3], &last) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "expected integer or \"end\" but got \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if ((first > last) && !firstIsEnd) {
	return TCL_OK;
    }

    /*
     * Extract a range of fields.
     */

    for (count = 0, begin = argv[1]; count < first; begin = next, count++) {
	result = TclFindElement(interp, begin, &dummy, &next, (int *) NULL,
		(int *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
	if (*next == 0) {
	    if (firstIsEnd) {
		first = count;
	    } else {
		begin = next;
	    }
	    break;
	}
    }
    for (count = first, end = begin; (count <= last) && (*end != 0);
	    count++) {
	result = TclFindElement(interp, end, &dummy, &end, (int *) NULL,
		(int *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
    }
    if (end == begin) {
	return TCL_OK;
    }

    /*
     * Chop off trailing spaces.
     */

    while ((end != begin) && (isspace(UCHAR(end[-1])))
	    && (((end-1) == begin) || (end[-2] != '\\'))) {
	end--;
    }
    c = *end;
    *end = 0;
    Tcl_SetResult(interp, begin, TCL_VOLATILE);
    *end = c;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LreplaceCmd --
 *
 *	This procedure is invoked to process the "lreplace" Tcl command.
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
Tcl_LreplaceCmd(notUsed, interp, argc, argv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *p1, *p2, *element, savedChar, *dummy, *next;
    int i, first, last, count, result, size, firstIsEnd;

    if (argc < 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" list first last ?element element ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if ((*argv[2] == 'e') && (strncmp(argv[2], "end", strlen(argv[2])) == 0)) {
	firstIsEnd = 1;
	first = INT_MAX;
    } else {
	firstIsEnd = 0;
	if (Tcl_GetInt(interp, argv[2], &first) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "bad index \"", argv[2],
		    "\": must be integer or \"end\"", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if ((*argv[3] == 'e') && (strncmp(argv[3], "end", strlen(argv[3])) == 0)) {
	last = INT_MAX;
    } else {
	if (Tcl_GetInt(interp, argv[3], &last) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "bad index \"", argv[3],
		    "\": must be integer or \"end\"", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if (first < 0) {
	first = 0;
    }

    /*
     * Skip over the elements of the list before "first".
     */

    size = 0;
    element = argv[1];
    for (count = 0, p1 = argv[1]; (count < first) && (*p1 != 0); count++) {
	result = TclFindElement(interp, p1, &element, &next, &size,
		(int *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
	if ((*next == 0) && firstIsEnd) {
	    break;
	}
	p1 = next;
    }
    if (*p1 == 0) {
	Tcl_AppendResult(interp, "list doesn't contain element ",
		argv[2], (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Skip over the elements of the list up through "last".
     */

    for (p2 = p1 ; (count <= last) && (*p2 != 0); count++) {
	result = TclFindElement(interp, p2, &dummy, &p2, (int *) NULL,
		(int *) NULL);
	if (result != TCL_OK) {
	    return result;
	}
    }

    /*
     * Add the elements before "first" to the result. Remove any
     * trailing white space, to make the result look as clean as
     * possible (this matters primarily if the replacement string is
     * empty).
     */

    while ((p1 != argv[1]) && (isspace(UCHAR(p1[-1])))
	    && (((p1-1) == argv[1]) || (p1[-2] != '\\'))) {
	p1--;
    }
    savedChar = *p1;
    *p1 = 0;
    Tcl_AppendResult(interp, argv[1], (char *) NULL);
    *p1 = savedChar;

    /*
     * Add the new list elements.
     */

    for (i = 4; i < argc; i++) {
	Tcl_AppendElement(interp, argv[i]);
    }

    /*
     * Append the remainder of the original list.
     */

    if (*p2 != 0) {
	if (*interp->result == 0) {
	    Tcl_SetResult(interp, p2, TCL_VOLATILE);
	} else {
	    Tcl_AppendResult(interp, " ", p2, (char *) NULL);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsearchCmd --
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

	/* ARGSUSED */
int
Tcl_LsearchCmd(notUsed, interp, argc, argv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#define EXACT	0
#define GLOB	1
#define REGEXP	2
    int listArgc;
    char **listArgv;
    int i, match, mode, index;

    mode = GLOB;
    if (argc == 4) {
	if (strcmp(argv[1], "-exact") == 0) {
	    mode = EXACT;
	} else if (strcmp(argv[1], "-glob") == 0) {
	    mode = GLOB;
	} else if (strcmp(argv[1], "-regexp") == 0) {
	    mode = REGEXP;
	} else {
	    Tcl_AppendResult(interp, "bad search mode \"", argv[1],
		    "\": must be -exact, -glob, or -regexp", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?mode? list pattern\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (Tcl_SplitList(interp, argv[argc-2], &listArgc, &listArgv) != TCL_OK) {
	return TCL_ERROR;
    }
    index = -1;
    for (i = 0; i < listArgc; i++) {
	match = 0;
	switch (mode) {
	    case EXACT:
		match = (strcmp(listArgv[i], argv[argc-1]) == 0);
		break;
	    case GLOB:
		match = Tcl_StringMatch(listArgv[i], argv[argc-1]);
		break;
	    case REGEXP:
		match = Tcl_RegExpMatch(interp, listArgv[i], argv[argc-1]);
		if (match < 0) {
		    ckfree((char *) listArgv);
		    return TCL_ERROR;
		}
		break;
	}
	if (match) {
	    index = i;
	    break;
	}
    }
    sprintf(interp->result, "%d", index);
    ckfree((char *) listArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsortCmd --
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

	/* ARGSUSED */
int
Tcl_LsortCmd(notUsed, interp, argc, argv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int listArgc, i, c;
    size_t length;
    char **listArgv;
    char *command = NULL;		/* Initialization needed only to
					 * prevent compiler warning. */

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-ascii? ?-integer? ?-real? ?-increasing? ?-decreasing?",
		" ?-command string? list\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (sortInterp != NULL) {
	interp->result = "can't invoke \"lsort\" recursively";
	return TCL_ERROR;
    }

    /*
     * Parse arguments to set up the mode for the sort.
     */

    sortInterp = interp;
    sortMode = ASCII;
    sortIncreasing = 1;
    sortCode = TCL_OK;
    for (i = 1; i < argc-1; i++) {
	length = strlen(argv[i]);
	if (length < 2) {
	    badSwitch:
	    Tcl_AppendResult(interp, "bad switch \"", argv[i],
		    "\": must be -ascii, -integer, -real, -increasing",
		    " -decreasing, or -command", (char *) NULL);
	    sortCode = TCL_ERROR;
	    goto done;
	}
	c = argv[i][1];
	if ((c == 'a') && (strncmp(argv[i], "-ascii", length) == 0)) {
	    sortMode = ASCII;
	} else if ((c == 'c') && (strncmp(argv[i], "-command", length) == 0)) {
	    if (i == argc-2) {
		Tcl_AppendResult(interp, "\"-command\" must be",
			" followed by comparison command", (char *) NULL);
		sortCode = TCL_ERROR;
		goto done;
	    }
	    sortMode = COMMAND;
	    command = argv[i+1];
	    i++;
	} else if ((c == 'd')
		&& (strncmp(argv[i], "-decreasing", length) == 0)) {
	    sortIncreasing = 0;
	} else if ((c == 'i') && (length >= 4)
		&& (strncmp(argv[i], "-increasing", length) == 0)) {
	    sortIncreasing = 1;
	} else if ((c == 'i') && (length >= 4)
		&& (strncmp(argv[i], "-integer", length) == 0)) {
	    sortMode = INTEGER;
	} else if ((c == 'r')
		&& (strncmp(argv[i], "-real", length) == 0)) {
	    sortMode = REAL;
	} else {
	    goto badSwitch;
	}
    }
    if (sortMode == COMMAND) {
	Tcl_DStringInit(&sortCmd);
	Tcl_DStringAppend(&sortCmd, command, -1);
    }

    if (Tcl_SplitList(interp, argv[argc-1], &listArgc, &listArgv) != TCL_OK) {
	sortCode = TCL_ERROR;
	goto done;
    }
    qsort((VOID *) listArgv, (size_t) listArgc, sizeof (char *),
	    SortCompareProc);
    if (sortCode == TCL_OK) {
	Tcl_ResetResult(interp);
	interp->result = Tcl_Merge(listArgc, listArgv);
	interp->freeProc = TCL_DYNAMIC;
    }
    if (sortMode == COMMAND) {
	Tcl_DStringFree(&sortCmd);
    }
    ckfree((char *) listArgv);

    done:
    sortInterp = NULL;
    return sortCode;
}

/*
 *----------------------------------------------------------------------
 *
 * SortCompareProc --
 *
 *	This procedure is invoked by qsort to determine the proper
 *	ordering between two elements.
 *
 * Results:
 *	< 0 means first is "smaller" than "second", > 0 means "first"
 *	is larger than "second", and 0 means they should be treated
 *	as equal.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something
 *	weird.
 *
 *----------------------------------------------------------------------
 */

static int
SortCompareProc(first, second)
    CONST VOID *first, *second;		/* Elements to be compared. */
{
    int order;
    char *firstString = *((char **) first);
    char *secondString = *((char **) second);

    order = 0;
    if (sortCode != TCL_OK) {
	/*
	 * Once an error has occurred, skip any future comparisons
	 * so as to preserve the error message in sortInterp->result.
	 */

	return order;
    }
    if (sortMode == ASCII) {
	order = strcmp(firstString, secondString);
    } else if (sortMode == INTEGER) {
	int a, b;

	if ((Tcl_GetInt(sortInterp, firstString, &a) != TCL_OK)
		|| (Tcl_GetInt(sortInterp, secondString, &b) != TCL_OK)) {
	    Tcl_AddErrorInfo(sortInterp,
		    "\n    (converting list element from string to integer)");
	    sortCode = TCL_ERROR;
	    return order;
	}
	if (a > b) {
	    order = 1;
	} else if (b > a) {
	    order = -1;
	}
    } else if (sortMode == REAL) {
	double a, b;

	if ((Tcl_GetDouble(sortInterp, firstString, &a) != TCL_OK)
		|| (Tcl_GetDouble(sortInterp, secondString, &b) != TCL_OK)) {
	    Tcl_AddErrorInfo(sortInterp,
		    "\n    (converting list element from string to real)");
	    sortCode = TCL_ERROR;
	    return order;
	}
	if (a > b) {
	    order = 1;
	} else if (b > a) {
	    order = -1;
	}
    } else {
	int oldLength;
	char *end;

	/*
	 * Generate and evaluate a command to determine which string comes
	 * first.
	 */

	oldLength = Tcl_DStringLength(&sortCmd);
	Tcl_DStringAppendElement(&sortCmd, firstString);
	Tcl_DStringAppendElement(&sortCmd, secondString);
	sortCode = Tcl_Eval(sortInterp, Tcl_DStringValue(&sortCmd));
	Tcl_DStringTrunc(&sortCmd, oldLength);
	if (sortCode != TCL_OK) {
	    Tcl_AddErrorInfo(sortInterp,
		    "\n    (user-defined comparison command)");
	    return order;
	}

	/*
	 * Parse the result of the command.
	 */

	order = strtol(sortInterp->result, &end, 0);
	if ((end == sortInterp->result) || (*end != 0)) {
	    Tcl_ResetResult(sortInterp);
	    Tcl_AppendResult(sortInterp,
		    "comparison command returned non-numeric result",
		    (char *) NULL);
	    sortCode = TCL_ERROR;
	    return order;
	}
    }
    if (!sortIncreasing) {
	order = -order;
    }
    return order;
}
