/* 
 * tclCmdAH.c --
 *
 *	This file contains the top-level command routines for most of
 *	the Tcl built-in commands whose names begin with the letters
 *	A to H.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCmdAH.c 1.159 97/10/31 13:06:07
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Prototypes for local procedures defined in this file:
 */

static char *		GetTypeFromMode _ANSI_ARGS_((int mode));
static int		StoreStatData _ANSI_ARGS_((Tcl_Interp *interp,
			    char *varName, struct stat *statPtr));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BreakCmd --
 *
 *	This procedure is invoked to process the "break" Tcl command.
 *	See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when
 *	a command name is computed at runtime, and is "break" or the name
 *	to which "break" was renamed: e.g., "set z break; $z"
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
Tcl_BreakCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc != 1) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_BREAK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CaseObjCmd --
 *
 *	This procedure is invoked to process the "case" Tcl command.
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
Tcl_CaseObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register int i;
    int body, result;
    char *string, *arg;
    int argLen, caseObjc;
    Tcl_Obj *CONST *caseObjv;
    Tcl_Obj *armPtr;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"string ?in? patList body ... ?default body?");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */
    
    string = Tcl_GetStringFromObj(objv[1], &argLen);
    body = -1;

    arg = Tcl_GetStringFromObj(objv[2], &argLen);
    if (strcmp(arg, "in") == 0) {
	i = 3;
    } else {
	i = 2;
    }
    caseObjc = objc - i;
    caseObjv = objv + i;

    /*
     * If all of the pattern/command pairs are lumped into a single
     * argument, split them out again.
     * THIS FAILS IF THE ARG'S STRING REP CONTAINS A NULL
     */

    if (caseObjc == 1) {
	Tcl_Obj **newObjv;
	
	Tcl_ListObjGetElements(interp, caseObjv[0], &caseObjc, &newObjv);
	caseObjv = newObjv;
    }

    for (i = 0;  i < caseObjc;  i += 2) {
	int patObjc, j;
	char **patObjv;
	char *pat;
	register char *p;

	if (i == (caseObjc-1)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "extra case pattern with no body", -1);
	    return TCL_ERROR;
	}

	/*
	 * Check for special case of single pattern (no list) with
	 * no backslash sequences.
	 */

	pat = Tcl_GetStringFromObj(caseObjv[i], &argLen);
	for (p = pat;  *p != 0;  p++) {	/* FAILS IF NULL BYTE */
	    if (isspace(UCHAR(*p)) || (*p == '\\')) {
		break;
	    }
	}
	if (*p == 0) {
	    if ((*pat == 'd') && (strcmp(pat, "default") == 0)) {
		body = i+1;
	    }
	    if (Tcl_StringMatch(string, pat)) {
		body = i+1;
		goto match;
	    }
	    continue;
	}


	/*
	 * Break up pattern lists, then check each of the patterns
	 * in the list.
	 */

	result = Tcl_SplitList(interp, pat, &patObjc, &patObjv);
	if (result != TCL_OK) {
	    return result;
	}
	for (j = 0; j < patObjc; j++) {
	    if (Tcl_StringMatch(string, patObjv[j])) {
		body = i+1;
		break;
	    }
	}
	ckfree((char *) patObjv);
	if (j < patObjc) {
	    break;
	}
    }

    match:
    if (body != -1) {
	armPtr = caseObjv[body-1];
	result = Tcl_EvalObj(interp, caseObjv[body]);
	if (result == TCL_ERROR) {
	    char msg[100];
	    
	    arg = Tcl_GetStringFromObj(armPtr, &argLen);
	    sprintf(msg, "\n    (\"%.*s\" arm line %d)", argLen, arg,
	            interp->errorLine);
	    Tcl_AddObjErrorInfo(interp, msg, -1);
	}
	return result;
    }

    /*
     * Nothing matched: return nothing.
     */

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CatchObjCmd --
 *
 *	This object-based procedure is invoked to process the "catch" Tcl 
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
Tcl_CatchObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *varNamePtr = NULL;
    int result;

    if ((objc != 2) && (objc != 3)) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?varName?");
	return TCL_ERROR;
    }

    /*
     * Save a pointer to the variable name object, if any, in case the
     * Tcl_EvalObj reallocates the bytecode interpreter's evaluation
     * stack rendering objv invalid.
     */
    
    if (objc == 3) {
	varNamePtr = objv[2];
    }
    
    result = Tcl_EvalObj(interp, objv[1]);
    
    if (objc == 3) {
	if (Tcl_ObjSetVar2(interp, varNamePtr, NULL,
		    Tcl_GetObjResult(interp), TCL_PARSE_PART1) == NULL) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),  
	            "couldn't save command result in variable", -1);
	    return TCL_ERROR;
	}
    }

    /*
     * Set the interpreter's object result to an integer object holding the
     * integer Tcl_EvalObj result. Note that we don't bother generating a
     * string representation. We reset the interpreter's object result
     * to an unshared empty object and then set it to be an integer object.
     */

    Tcl_ResetResult(interp);
    Tcl_SetIntObj(Tcl_GetObjResult(interp), result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CdObjCmd --
 *
 *	This procedure is invoked to process the "cd" Tcl command.
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
Tcl_CdObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *dirName;
    int dirLength;
    Tcl_DString buffer;
    int result;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "dirName");
	return TCL_ERROR;
    }

    if (objc == 2) {
	dirName = Tcl_GetStringFromObj(objv[1], &dirLength);
    } else {
	dirName = "~";
    }
    dirName = Tcl_TranslateFileName(interp, dirName, &buffer);
    if (dirName == NULL) {
	return TCL_ERROR;
    }
    result = TclChdir(interp, dirName);
    Tcl_DStringFree(&buffer);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConcatObjCmd --
 *
 *	This object-based procedure is invoked to process the "concat" Tcl
 *	command. See the user documentation for details on what it does/
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
Tcl_ConcatObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc >= 2) {
	Tcl_SetObjResult(interp, Tcl_ConcatObj(objc-1, objv+1));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ContinueCmd -
 *
 *	This procedure is invoked to process the "continue" Tcl command.
 *	See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when
 *	a command name is computed at runtime, and is "continue" or the name
 *	to which "continue" was renamed: e.g., "set z continue; $z"
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
Tcl_ContinueCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc != 1) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_CONTINUE;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ErrorObjCmd --
 *
 *	This procedure is invoked to process the "error" Tcl command.
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
Tcl_ErrorObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    register Tcl_Obj *namePtr;
    char *info;
    int infoLen;

    if ((objc < 2) || (objc > 4)) {
	Tcl_WrongNumArgs(interp, 1, objv, "message ?errorInfo? ?errorCode?");
	return TCL_ERROR;
    }
    
    if (objc >= 3) {		/* process the optional info argument */
	info = Tcl_GetStringFromObj(objv[2], &infoLen);
	if (*info != 0) {
	    Tcl_AddObjErrorInfo(interp, info, infoLen);
	    iPtr->flags |= ERR_ALREADY_LOGGED;
	}
    }
    
    if (objc == 4) {
	namePtr = Tcl_NewStringObj("errorCode", -1);
	Tcl_ObjSetVar2(interp, namePtr, (Tcl_Obj *) NULL, objv[3],
		TCL_GLOBAL_ONLY);
	iPtr->flags |= ERROR_CODE_SET;
	Tcl_DecrRefCount(namePtr); /* we're done with name object */
    }
    
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EvalObjCmd --
 *
 *	This object-based procedure is invoked to process the "eval" Tcl 
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
Tcl_EvalObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int result;
    register Tcl_Obj *objPtr;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "arg ?arg ...?");
	return TCL_ERROR;
    }
    
    if (objc == 2) {
	result = Tcl_EvalObj(interp, objv[1]);
    } else {
	/*
	 * More than one argument: concatenate them together with spaces
	 * between, then evaluate the result.
	 */
    
	objPtr = Tcl_ConcatObj(objc-1, objv+1);
	result = Tcl_EvalObj(interp, objPtr);
	Tcl_DecrRefCount(objPtr);  /* we're done with the object */
    }
    if (result == TCL_ERROR) {
	char msg[60];
	sprintf(msg, "\n    (\"eval\" body line %d)", interp->errorLine);
	Tcl_AddObjErrorInfo(interp, msg, -1);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExitObjCmd --
 *
 *	This procedure is invoked to process the "exit" Tcl command.
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
Tcl_ExitObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int value;

    if ((objc != 1) && (objc != 2)) {
	Tcl_WrongNumArgs(interp, 1, objv, "?returnCode?");
	return TCL_ERROR;
    }
    
    if (objc == 1) {
	value = 0;
    } else if (Tcl_GetIntFromObj(interp, objv[1], &value) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_Exit(value);
    /*NOTREACHED*/
    return TCL_OK;			/* Better not ever reach this! */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExprObjCmd --
 *
 *	This object-based procedure is invoked to process the "expr" Tcl
 *	command. See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is called in two
 *	circumstances: 1) to execute expr commands that are too complicated
 *	or too unsafe to try compiling directly into an inline sequence of
 *	instructions, and 2) to execute commands where the command name is
 *	computed at runtime and is "expr" or the name to which "expr" was
 *	renamed (e.g., "set z expr; $z 2+3")
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
Tcl_ExprObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Tcl_Obj *objPtr;
    Tcl_Obj *resultPtr;
    register char *bytes;
    int length, i, result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "arg ?arg ...?");
	return TCL_ERROR;
    }

    if (objc == 2) {
	result = Tcl_ExprObj(interp, objv[1], &resultPtr);
	if (result == TCL_OK) {
	    Tcl_SetObjResult(interp, resultPtr);
	    Tcl_DecrRefCount(resultPtr);  /* done with the result object */
	}
	return result;
    }

    /*
     * Create a new object holding the concatenated argument strings.
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */

    bytes = Tcl_GetStringFromObj(objv[1], &length);
    objPtr = Tcl_NewStringObj(bytes, length);
    Tcl_IncrRefCount(objPtr);
    for (i = 2;  i < objc;  i++) {
	Tcl_AppendToObj(objPtr, " ", 1);
	bytes = Tcl_GetStringFromObj(objv[i], &length);
	Tcl_AppendToObj(objPtr, bytes, length);
    }

    /*
     * Evaluate the concatenated string object.
     */

    result = Tcl_ExprObj(interp, objPtr, &resultPtr);
    if (result == TCL_OK) {
	Tcl_SetObjResult(interp, resultPtr);
	Tcl_DecrRefCount(resultPtr);  /* done with the result object */
    }

    /*
     * Free allocated resources.
     */
    
    Tcl_DecrRefCount(objPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileObjCmd --
 *
 *	This procedure is invoked to process the "file" Tcl command.
 *	See the user documentation for details on what it does.
 *	PLEASE NOTE THAT THIS FAILS WITH FILENAMES AND PATHS WITH
 *	EMBEDDED NULLS, WHICH COULD THEORETICALLY HAPPEN ON A MAC.
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
Tcl_FileObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *fileName, *extension, *errorString;
    int statOp = 0;		/* Init. to avoid compiler warning. */
    int length;
    int mode = 0;			/* Initialized only to prevent
					 * compiler warning message. */
    struct stat statBuf;
    Tcl_DString buffer;
    Tcl_Obj *resultPtr;
    int index, result;

/*
 * This list of constants should match the fileOption string array below.
 */

enum {FILE_ATIME, FILE_ATTRIBUTES, FILE_COPY, FILE_DELETE, FILE_DIRNAME,
	FILE_EXECUTABLE, FILE_EXISTS, FILE_EXTENSION, FILE_ISDIRECTORY,
	FILE_ISFILE, FILE_JOIN, FILE_LSTAT, FILE_MTIME, FILE_MKDIR,
	FILE_NATIVENAME, FILE_OWNED, FILE_PATHTYPE, FILE_READABLE,
	FILE_READLINK, FILE_RENAME, FILE_ROOTNAME, FILE_SIZE, FILE_SPLIT,
	FILE_STAT, FILE_TAIL, FILE_TYPE, FILE_VOLUMES, FILE_WRITABLE};


    static char *fileOptions[] = {"atime", "attributes", "copy", "delete", 
    	    "dirname", "executable", "exists", "extension", "isdirectory", 
    	    "isfile", "join", "lstat", "mtime", "mkdir", "nativename", 
    	    "owned", "pathtype", "readable", "readlink", "rename",
    	    "rootname", "size", "split", "stat", "tail", "type", "volumes", 
    	    "writable", (char *) NULL};

    if (objc < 2) {
    	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], fileOptions, "option", 0, &index)
	    != TCL_OK) {
    	return TCL_ERROR;
    }
    
    result = TCL_OK;
    /* 
     * First, do the volumes command, since it is the only one that
     * has objc == 2.
     */
	
    if ( index == FILE_VOLUMES) {
        if ( objc != 2 ) {
	    Tcl_WrongNumArgs(interp, 1, objv, "volumes");
	    return TCL_ERROR;
	}
	result = TclpListVolumes(interp);
	return result;
    }
    
    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "name ?arg ...?");
	return TCL_ERROR;
    }

    Tcl_DStringInit(&buffer);
    resultPtr = Tcl_GetObjResult(interp);
    

    /*
     * Handle operations on the file name.
     */
    
    switch (index) {
        case FILE_ATTRIBUTES:
            result = TclFileAttrsCmd(interp, objc - 2, objv + 2);
    	    goto done;
    	case FILE_DIRNAME:	{
    	    int pargc;
	    char **pargv;

	    if (objc != 3) {
	    	errorString = "dirname name";
	    	goto not3Args;
	    }

	    fileName = Tcl_GetStringFromObj(objv[2], &length);

	    /*
	     * If there is only one element, and it starts with a tilde,
	     * perform tilde substitution and resplit the path.
	     */

	    Tcl_SplitPath(fileName, &pargc, &pargv);
	    if ((pargc == 1) && (*fileName == '~')) {
	        ckfree((char*) pargv);
	        fileName = Tcl_TranslateFileName(interp, fileName, &buffer);
	        if (fileName == NULL) {
		    result = TCL_ERROR;
		    goto done;
	        }
	        Tcl_SplitPath(fileName, &pargc, &pargv);
	        Tcl_DStringSetLength(&buffer, 0);
	    }

	    /*
	     * Return all but the last component.  If there is only one
	     * component, return it if the path was non-relative, otherwise
	     * return the current directory.
	     */

	    if (pargc > 1) {
	    	Tcl_JoinPath(pargc-1, pargv, &buffer);
	    	Tcl_SetStringObj(resultPtr, Tcl_DStringValue(&buffer),
	    		buffer.length);
	    } else if ((pargc == 0)
		    || (Tcl_GetPathType(pargv[0]) == TCL_PATH_RELATIVE)) {
		Tcl_SetStringObj(resultPtr, (tclPlatform == TCL_PLATFORM_MAC)
			? ":" : ".", 1);
	    } else {
	    	Tcl_SetStringObj(resultPtr, pargv[0], -1);	    }
	    ckfree((char *)pargv);
	    goto done;
	}
    	case FILE_TAIL: {
	    int pargc;
	    char **pargv;

	    if (objc != 3) {
	    	errorString = "tail name";
	    	goto not3Args;
	    }
	    
	    fileName = Tcl_GetStringFromObj(objv[2], &length);

	    /*
	     * If there is only one element, and it starts with a tilde,
	     * perform tilde substitution and resplit the path.
	     */

	    Tcl_SplitPath(fileName, &pargc, &pargv);
	    if ((pargc == 1) && (*fileName == '~')) {
	    	ckfree((char*) pargv);
	    	fileName = Tcl_TranslateFileName(interp, fileName, &buffer);
	    	if (fileName == NULL) {
		    result = TCL_ERROR;
		    goto done;
	        }
	        Tcl_SplitPath(fileName, &pargc, &pargv);
	        Tcl_DStringSetLength(&buffer, 0);
	    }

	    /*
	     * Return the last component, unless it is the only component,
	     * and it is the root of an absolute path.
	     */

	    if (pargc > 0) {
	    	if ((pargc > 1)
		    	|| (Tcl_GetPathType(pargv[0]) == TCL_PATH_RELATIVE)) {
		    Tcl_SetStringObj(resultPtr, pargv[pargc - 1], -1);
	    	}
	    }
	    ckfree((char *)pargv);
	    goto done;
	}
	case FILE_ROOTNAME: {
	    char *fileName;
	    
	    if (objc != 3) {
	    	errorString = "rootname name";
	    	goto not3Args;
	    }
	    
	    fileName = Tcl_GetStringFromObj(objv[2], &length);
	    extension = TclGetExtension(fileName);
	    if (extension == NULL) {
	    	Tcl_SetObjResult(interp, objv[2]);
	    } else {
	        Tcl_SetStringObj(resultPtr, fileName,
			(int) (length - strlen(extension)));
	    }
	    goto done;
	}
	case FILE_EXTENSION:
	    if (objc != 3) {
	    	errorString = "extension name";
	    	goto not3Args;
	    }
	    extension = TclGetExtension(Tcl_GetStringFromObj(objv[2],&length));

	    if (extension != NULL) {
	    	Tcl_SetStringObj(resultPtr, extension, (int)strlen(extension));
	    }
	    goto done;
	case FILE_PATHTYPE:
	    if (objc != 3) {
	    	errorString = "pathtype name";
	    	goto not3Args;
	    }
	    switch (Tcl_GetPathType(Tcl_GetStringFromObj(objv[2], &length))) {
	    	case TCL_PATH_ABSOLUTE:
	    	    Tcl_SetStringObj(resultPtr, "absolute", -1);
		    break;
	    	case TCL_PATH_RELATIVE:
	    	    Tcl_SetStringObj(resultPtr, "relative", -1);
	    	    break;
	    	case TCL_PATH_VOLUME_RELATIVE:
		    Tcl_SetStringObj(resultPtr, "volumerelative", -1);
		    break;
	    }
	    goto done;
	case FILE_SPLIT: {
	    int pargc, i;
	    char **pargvList;
	    Tcl_Obj *listObjPtr;
		
	    if (objc != 3) {
    	    	errorString = "split name";
	    	goto not3Args;
	    }
		
	    Tcl_SplitPath(Tcl_GetStringFromObj(objv[2], &length), &pargc,
	    	    &pargvList);
	    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	    for (i = 0; i < pargc; i++) {
	    	Tcl_ListObjAppendElement(interp, listObjPtr,
			Tcl_NewStringObj(pargvList[i], -1));
	    }
	    ckfree((char *) pargvList);
	    Tcl_SetObjResult(interp, listObjPtr);
	    goto done;
	}
	case FILE_JOIN: {
	    char **pargv = (char **) ckalloc((objc - 2) * sizeof(char *));
	    int i;
	    
	    for (i = 2; i < objc; i++) {
	    	pargv[i - 2] = Tcl_GetStringFromObj(objv[i], &length);
	    }
	    Tcl_JoinPath(objc - 2, pargv, &buffer);
	    Tcl_SetStringObj(resultPtr, Tcl_DStringValue(&buffer), 
                    buffer.length);
	    ckfree((char *) pargv);
	    Tcl_DStringFree(&buffer);
	    goto done;
	}
	case FILE_RENAME: {
	    char **pargv = (char **) ckalloc(objc * sizeof(char *));
	    int i;
	    
	    for (i = 0; i < objc; i++) {
	    	pargv[i] = Tcl_GetStringFromObj(objv[i], &length);
	    }
	    result = TclFileRenameCmd(interp, objc, pargv);
	    ckfree((char *) pargv);
	    goto done;
	}
	case FILE_MKDIR: {
	    char **pargv = (char **) ckalloc(objc * sizeof(char *));
	    int i;
	    
	    for (i = 0; i < objc; i++) {
	    	pargv[i] = Tcl_GetStringFromObj(objv[i], &length);
	    }
	    result = TclFileMakeDirsCmd(interp, objc, pargv);
	    ckfree((char *) pargv);
	    goto done;
	}
	case FILE_DELETE: {
	    char **pargv = (char **) ckalloc(objc * sizeof(char *));
	    int i;
	    
	    for (i = 0; i < objc; i++) {
	    	pargv[i] = Tcl_GetStringFromObj(objv[i], &length);
	    }
	    result = TclFileDeleteCmd(interp, objc, pargv);
	    ckfree((char *) pargv);
	    goto done;
	}
	case FILE_COPY: {
	    char **pargv = (char **) ckalloc(objc * sizeof(char *));
	    int i;
	    
	    for (i = 0; i < objc; i++) {
	    	pargv[i] = Tcl_GetStringFromObj(objv[i], &length);
	    }
	    result = TclFileCopyCmd(interp, objc, pargv);
	    ckfree((char *) pargv);
	    goto done;
	}
	case FILE_NATIVENAME:
	    fileName = Tcl_TranslateFileName(interp,
	    	    Tcl_GetStringFromObj(objv[2], &length), &buffer);
	    if (fileName == NULL) {
		result = TCL_ERROR ;
	    } else {
		Tcl_SetStringObj(resultPtr, fileName, -1);
	    }
	    goto done;
    }

    /*
     * Next, handle operations that can be satisfied with the "access"
     * kernel call.
     */

    fileName = Tcl_TranslateFileName(interp,
	    Tcl_GetStringFromObj(objv[2], &length), &buffer);
	
    switch (index) {
    	case FILE_READABLE:
    	    if (objc != 3) {
	    	errorString = "readable name";
	    	goto not3Args;
	    }
	    mode = R_OK;
checkAccess:
	    /*
	     * The result might have been set within Tcl_TranslateFileName
	     * (like no such user "blah" for file exists ~blah)
	     * but we don't want to flag an error in that case.
	     */
	    if (fileName == NULL) {
		Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
	    } else {
		Tcl_SetBooleanObj(resultPtr, (access(fileName, mode) != -1));
	    }
	    goto done;
	  case FILE_WRITABLE:
	    if (objc != 3) {
	    	errorString = "writable name";
	    	goto not3Args;
	    }
	    mode = W_OK;
	    goto checkAccess;
	  case FILE_EXECUTABLE:
	    if (objc != 3) {
	    	errorString = "executable name";
	    	goto not3Args;
	    }
	    mode = X_OK;
	    goto checkAccess;
	  case FILE_EXISTS:
	    if (objc != 3) {
	    	errorString = "exists name";
	    	goto not3Args;
	    }
	    mode = F_OK;
	    goto checkAccess;
    }

	
    /*
     * Lastly, check stuff that requires the file to be stat-ed.
     */

    if (fileName == NULL) {
	result = TCL_ERROR;
	goto done;
    }
    
    switch (index) {
    	case FILE_ATIME:
    	    if (objc != 3) {
	    	errorString = "atime name";
	    	goto not3Args;
	    }
	    
	    if (stat(fileName, &statBuf) == -1) {
	    	goto badStat;
	    }
	    Tcl_SetLongObj(resultPtr, (long) statBuf.st_atime);
	    goto done;
    	case FILE_ISDIRECTORY:
    	    if (objc != 3) {
    	    	errorString = "isdirectory name";
    	    	goto not3Args;
    	    }
    	    statOp = 2;
    	    break;
    	case FILE_ISFILE:
    	    if (objc != 3) {
    	    	errorString = "isfile name";
    	    	goto not3Args;
    	    }
    	    statOp = 1;
    	    break;
    	case FILE_LSTAT:
    	    if (objc != 4) {
    	    	Tcl_WrongNumArgs(interp, 1, objv, "lstat name varName");
    	    	result = TCL_ERROR;
    	    	goto done;
    	    }
    	    
    	    if (lstat(fileName, &statBuf) == -1) {
    	    	Tcl_AppendStringsToObj(resultPtr, "couldn't lstat \"",
    	    		Tcl_GetStringFromObj(objv[2], &length), "\": ",
    	    		Tcl_PosixError(interp), (char *) NULL);
    	    	result = TCL_ERROR;
    	    	goto done;
    	    }
    	    result = StoreStatData(interp, Tcl_GetStringFromObj(objv[3],
    	    	    &length), &statBuf);
    	    goto done;
	case FILE_MTIME:
	    if (objc != 3) {
	    	errorString = "mtime name";
	    	goto not3Args;
	    }
	    if (stat(fileName, &statBuf) == -1) {
	    	goto badStat;
	    }
	    Tcl_SetLongObj(resultPtr, (long) statBuf.st_mtime);
	    goto done;
	case FILE_OWNED:
	    if (objc != 3) {
	    	errorString = "owned name";
	    	goto not3Args;
	    }	        	    
    	    statOp = 0;
    	    break;
	case FILE_READLINK: {
	    char linkValue[MAXPATHLEN + 1];
	    int linkLength;
		
	    if (objc != 3) {
	    	errorString = "readlink name";
	        goto not3Args;
	    }

	    /*
	     * If S_IFLNK isn't defined it means that the machine doesn't
	     * support symbolic links, so the file can't possibly be a
	     * symbolic link.  Generate an EINVAL error, which is what
	     * happens on machines that do support symbolic links when
	     * you invoke readlink on a file that isn't a symbolic link.
	     */

#ifndef S_IFLNK
	    linkLength = -1;
	    errno = EINVAL;
#else
	    linkLength = readlink(fileName, linkValue, sizeof(linkValue) - 1);
#endif /* S_IFLNK */
	    if (linkLength == -1) {
	    	Tcl_AppendStringsToObj(resultPtr, "couldn't readlink \"", 
	    		Tcl_GetStringFromObj(objv[2], &length), "\": ", 
	    		Tcl_PosixError(interp), (char *) NULL);
	    	result = TCL_ERROR;
	    	goto done;
	    }
	    linkValue[linkLength] = 0;
	    Tcl_SetStringObj(resultPtr, linkValue, linkLength);
	    goto done;
	}
	case FILE_SIZE:
	    if (objc != 3) {
	    	errorString = "size name";
	    	goto not3Args;
	    }
	    if (stat(fileName, &statBuf) == -1) {
	    	goto badStat;
	    }
	    Tcl_SetLongObj(resultPtr, (long) statBuf.st_size);
	    goto done;
	case FILE_STAT:
	    if (objc != 4) {
	    	Tcl_WrongNumArgs(interp, 1, objv, "stat name varName");
	    	result = TCL_ERROR;
	    	goto done;
	    }

	    if (stat(fileName, &statBuf) == -1) {
badStat:
		Tcl_AppendStringsToObj(resultPtr, "couldn't stat \"", 
			Tcl_GetStringFromObj(objv[2], &length),
		    	"\": ", Tcl_PosixError(interp), (char *) NULL);
	    	result = TCL_ERROR;
	    	goto done;
	    }
	    result = StoreStatData(interp, Tcl_GetStringFromObj(objv[3],
	    	    &length), &statBuf);
	    goto done;
	case FILE_TYPE:
	    if (objc != 3) {
	    	errorString = "type name";
	    	goto not3Args;
	    }
	    if (lstat(fileName, &statBuf) == -1) {
	    	goto badStat;
	    }
	    errorString = GetTypeFromMode((int) statBuf.st_mode);
	    Tcl_SetStringObj(resultPtr, errorString, -1);
	    goto done;
    }

    if (stat(fileName, &statBuf) == -1) {
    	Tcl_SetBooleanObj(resultPtr, 0);
	goto done;
    }
    switch (statOp) {
	case 0:
	    /*
	     * For Windows and Macintosh, there are no user ids 
	     * associated with a file, so we always return 1.
	     */

#if (defined(__WIN32__) || defined(MAC_TCL))
	    mode = 1;
#else
	    mode = (geteuid() == statBuf.st_uid);
#endif
	    break;
	case 1:
	    mode = S_ISREG(statBuf.st_mode);
	    break;
	case 2:
	    mode = S_ISDIR(statBuf.st_mode);
	    break;
    }
    Tcl_SetBooleanObj(resultPtr, mode);

done:
    Tcl_DStringFree(&buffer);
    return result;

not3Args:
    Tcl_WrongNumArgs(interp, 1, objv, errorString);
    result = TCL_ERROR;
    goto done;
}

/*
 *----------------------------------------------------------------------
 *
 * StoreStatData --
 *
 *	This is a utility procedure that breaks out the fields of a
 *	"stat" structure and stores them in textual form into the
 *	elements of an associative array.
 *
 * Results:
 *	Returns a standard Tcl return value.  If an error occurs then
 *	a message is left in interp->result.
 *
 * Side effects:
 *	Elements of the associative array given by "varName" are modified.
 *
 *----------------------------------------------------------------------
 */

static int
StoreStatData(interp, varName, statPtr)
    Tcl_Interp *interp;			/* Interpreter for error reports. */
    char *varName;			/* Name of associative array variable
					 * in which to store stat results. */
    struct stat *statPtr;		/* Pointer to buffer containing
					 * stat data to store in varName. */
{
    char string[30];

    sprintf(string, "%ld", (long) statPtr->st_dev);
    if (Tcl_SetVar2(interp, varName, "dev", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_ino);
    if (Tcl_SetVar2(interp, varName, "ino", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_mode);
    if (Tcl_SetVar2(interp, varName, "mode", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_nlink);
    if (Tcl_SetVar2(interp, varName, "nlink", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_uid);
    if (Tcl_SetVar2(interp, varName, "uid", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_gid);
    if (Tcl_SetVar2(interp, varName, "gid", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%lu", (unsigned long) statPtr->st_size);
    if (Tcl_SetVar2(interp, varName, "size", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_atime);
    if (Tcl_SetVar2(interp, varName, "atime", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_mtime);
    if (Tcl_SetVar2(interp, varName, "mtime", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    sprintf(string, "%ld", (long) statPtr->st_ctime);
    if (Tcl_SetVar2(interp, varName, "ctime", string, TCL_LEAVE_ERR_MSG)
	    == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_SetVar2(interp, varName, "type",
	    GetTypeFromMode((int) statPtr->st_mode), TCL_LEAVE_ERR_MSG) 
            == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTypeFromMode --
 *
 *	Given a mode word, returns a string identifying the type of a
 *	file.
 *
 * Results:
 *	A static text string giving the file type from mode.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetTypeFromMode(mode)
    int mode;
{
    if (S_ISREG(mode)) {
	return "file";
    } else if (S_ISDIR(mode)) {
	return "directory";
    } else if (S_ISCHR(mode)) {
	return "characterSpecial";
    } else if (S_ISBLK(mode)) {
	return "blockSpecial";
    } else if (S_ISFIFO(mode)) {
	return "fifo";
#ifdef S_ISLNK
    } else if (S_ISLNK(mode)) {
	return "link";
#endif
#ifdef S_ISSOCK
    } else if (S_ISSOCK(mode)) {
	return "socket";
#endif
    }
    return "unknown";
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ForCmd --
 *
 *      This procedure is invoked to process the "for" Tcl command.
 *      See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when
 *	a command name is computed at runtime, and is "for" or the name
 *	to which "for" was renamed: e.g.,
 *	"set z for; $z {set i 0} {$i<100} {incr i} {puts $i}"
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      See the user documentation.
 *
 *----------------------------------------------------------------------
 */

        /* ARGSUSED */
int
Tcl_ForCmd(dummy, interp, argc, argv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int argc;                           /* Number of arguments. */
    char **argv;                        /* Argument strings. */
{
    int result, value;

    if (argc != 5) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " start test next command\"", (char *) NULL);
        return TCL_ERROR;
    }

    result = Tcl_Eval(interp, argv[1]);
    if (result != TCL_OK) {
        if (result == TCL_ERROR) {
            Tcl_AddErrorInfo(interp, "\n    (\"for\" initial command)");
        }
        return result;
    }
    while (1) {
        result = Tcl_ExprBoolean(interp, argv[2], &value);
        if (result != TCL_OK) {
            return result;
        }
        if (!value) {
            break;
        }
        result = Tcl_Eval(interp, argv[4]);
        if ((result != TCL_OK) && (result != TCL_CONTINUE)) {
            if (result == TCL_ERROR) {
                char msg[60];
                sprintf(msg, "\n    (\"for\" body line %d)",interp->errorLine);
                Tcl_AddErrorInfo(interp, msg);
            }
            break;
        }
        result = Tcl_Eval(interp, argv[3]);
	if (result == TCL_BREAK) {
            break;
        } else if (result != TCL_OK) {
            if (result == TCL_ERROR) {
                Tcl_AddErrorInfo(interp, "\n    (\"for\" loop-end command)");
            }
            return result;
        }
    }
    if (result == TCL_BREAK) {
        result = TCL_OK;
    }
    if (result == TCL_OK) {
        Tcl_ResetResult(interp);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ForeachObjCmd --
 *
 *	This object-based procedure is invoked to process the "foreach" Tcl
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
Tcl_ForeachObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int result = TCL_OK;
    int i;			/* i selects a value list */
    int j, maxj;		/* Number of loop iterations */
    int v;			/* v selects a loop variable */
    int numLists;		/* Count of value lists */
    Tcl_Obj *bodyPtr;

    /*
     * We copy the argument object pointers into a local array to avoid
     * the problem that "objv" might become invalid. It is a pointer into
     * the evaluation stack and that stack might be grown and reallocated
     * if the loop body requires a large amount of stack space.
     */
    
#define NUM_ARGS 9
    Tcl_Obj *(argObjStorage[NUM_ARGS]);
    Tcl_Obj **argObjv = argObjStorage;
    
#define STATIC_LIST_SIZE 4
    int indexArray[STATIC_LIST_SIZE];	  /* Array of value list indices */
    int varcListArray[STATIC_LIST_SIZE];  /* # loop variables per list */
    Tcl_Obj **varvListArray[STATIC_LIST_SIZE]; /* Array of var name lists */
    int argcListArray[STATIC_LIST_SIZE];  /* Array of value list sizes */
    Tcl_Obj **argvListArray[STATIC_LIST_SIZE]; /* Array of value lists */

    int *index = indexArray;
    int *varcList = varcListArray;
    Tcl_Obj ***varvList = varvListArray;
    int *argcList = argcListArray;
    Tcl_Obj ***argvList = argvListArray;

    if (objc < 4 || (objc%2 != 0)) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"varList list ?varList list ...? command");
	return TCL_ERROR;
    }

    /*
     * Create the object argument array "argObjv". Make sure argObjv is
     * large enough to hold the objc arguments.
     */

    if (objc > NUM_ARGS) {
	argObjv = (Tcl_Obj **) ckalloc(objc * sizeof(Tcl_Obj *));
    }
    for (i = 0;  i < objc;  i++) {
	argObjv[i] = objv[i];
    }

    /*
     * Manage numList parallel value lists.
     * argvList[i] is a value list counted by argcList[i]
     * varvList[i] is the list of variables associated with the value list
     * varcList[i] is the number of variables associated with the value list
     * index[i] is the current pointer into the value list argvList[i]
     */

    numLists = (objc-2)/2;
    if (numLists > STATIC_LIST_SIZE) {
	index = (int *) ckalloc(numLists * sizeof(int));
	varcList = (int *) ckalloc(numLists * sizeof(int));
	varvList = (Tcl_Obj ***) ckalloc(numLists * sizeof(Tcl_Obj **));
	argcList = (int *) ckalloc(numLists * sizeof(int));
	argvList = (Tcl_Obj ***) ckalloc(numLists * sizeof(Tcl_Obj **));
    }
    for (i = 0;  i < numLists;  i++) {
	index[i] = 0;
	varcList[i] = 0;
	varvList[i] = (Tcl_Obj **) NULL;
	argcList[i] = 0;
	argvList[i] = (Tcl_Obj **) NULL;
    }

    /*
     * Break up the value lists and variable lists into elements
     * THIS FAILS IF THE OBJECT'S STRING REP HAS A NULL BYTE.
     */

    maxj = 0;
    for (i = 0;  i < numLists;  i++) {
	result = Tcl_ListObjGetElements(interp, argObjv[1+i*2],
	        &varcList[i], &varvList[i]);
	if (result != TCL_OK) {
	    goto done;
	}
	if (varcList[i] < 1) {
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "foreach varlist is empty", -1);
	    result = TCL_ERROR;
	    goto done;
	}
	
	result = Tcl_ListObjGetElements(interp, argObjv[2+i*2],
	        &argcList[i], &argvList[i]);
	if (result != TCL_OK) {
	    goto done;
	}
	
	j = argcList[i] / varcList[i];
	if ((argcList[i] % varcList[i]) != 0) {
	    j++;
	}
	if (j > maxj) {
	    maxj = j;
	}
    }

    /*
     * Iterate maxj times through the lists in parallel
     * If some value lists run out of values, set loop vars to ""
     */
    
    bodyPtr = argObjv[objc-1];
    for (j = 0;  j < maxj;  j++) {
	for (i = 0;  i < numLists;  i++) {
	    /*
	     * If a variable or value list object has been converted to
	     * another kind of Tcl object, convert it back to a list object
	     * and refetch the pointer to its element array.
	     */

	    if (argObjv[1+i*2]->typePtr != &tclListType) {
		result = Tcl_ListObjGetElements(interp, argObjv[1+i*2],
		        &varcList[i], &varvList[i]);
		if (result != TCL_OK) {
		    panic("Tcl_ForeachObjCmd: could not reconvert variable list %d to a list object\n", i);
		}
	    }
	    if (argObjv[2+i*2]->typePtr != &tclListType) {
		result = Tcl_ListObjGetElements(interp, argObjv[2+i*2],
	                &argcList[i], &argvList[i]);
		if (result != TCL_OK) {
		    panic("Tcl_ForeachObjCmd: could not reconvert value list %d to a list object\n", i);
		}
	    }
	    
	    for (v = 0;  v < varcList[i];  v++) {
		int k = index[i]++;
		Tcl_Obj *valuePtr, *varValuePtr;
		int isEmptyObj = 0;
		
		if (k < argcList[i]) {
		    valuePtr = argvList[i][k];
		} else {
		    valuePtr = Tcl_NewObj(); /* empty string */
		    isEmptyObj = 1;
		}
		varValuePtr = Tcl_ObjSetVar2(interp, varvList[i][v], NULL,
			valuePtr, TCL_PARSE_PART1);
		if (varValuePtr == NULL) {
		    if (isEmptyObj) {
			Tcl_DecrRefCount(valuePtr);
		    }
		    Tcl_ResetResult(interp);
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"couldn't set loop variable: \"",
			Tcl_GetStringFromObj(varvList[i][v], (int *) NULL),
			"\"", (char *) NULL);
		    result = TCL_ERROR;
		    goto done;
		}

	    }
	}

	result = Tcl_EvalObj(interp, bodyPtr);
	if (result != TCL_OK) {
	    if (result == TCL_CONTINUE) {
		result = TCL_OK;
	    } else if (result == TCL_BREAK) {
		result = TCL_OK;
		break;
	    } else if (result == TCL_ERROR) {
		char msg[100];
		sprintf(msg, "\n    (\"foreach\" body line %d)",
			interp->errorLine);
		Tcl_AddObjErrorInfo(interp, msg, -1);
		break;
	    } else {
		break;
	    }
	}
    }
    if (result == TCL_OK) {
	Tcl_ResetResult(interp);
    }

    done:
    if (numLists > STATIC_LIST_SIZE) {
	ckfree((char *) index);
	ckfree((char *) varcList);
	ckfree((char *) argcList);
	ckfree((char *) varvList);
	ckfree((char *) argvList);
    }
    if (argObjv != argObjStorage) {
	ckfree((char *) argObjv);
    }
    return result;
#undef STATIC_LIST_SIZE
#undef NUM_ARGS
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FormatObjCmd --
 *
 *	This procedure is invoked to process the "format" Tcl command.
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
Tcl_FormatObjCmd(dummy, interp, objc, objv)
    ClientData dummy;    	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register char *format;	/* Used to read characters from the format
				 * string. */
    int formatLen;              /* The length of the format string */
    char *endPtr;               /* Points to the last char in format array */
    char newFormat[40];		/* A new format specifier is generated here. */
    int width;			/* Field width from field specifier, or 0 if
				 * no width given. */
    int precision;		/* Field precision from field specifier, or 0
				 * if no precision given. */
    int size;			/* Number of bytes needed for result of
				 * conversion, based on type of conversion
				 * ("e", "s", etc.), width, and precision. */
    int intValue;		/* Used to hold value to pass to sprintf, if
				 * it's a one-word integer or char value */
    char *ptrValue = NULL;	/* Used to hold value to pass to sprintf, if
				 * it's a one-word value. */
    double doubleValue;		/* Used to hold value to pass to sprintf if
				 * it's a double value. */
    int whichValue;		/* Indicates which of intValue, ptrValue,
				 * or doubleValue has the value to pass to
				 * sprintf, according to the following
				 * definitions: */
#   define INT_VALUE 0
#   define PTR_VALUE 1
#   define DOUBLE_VALUE 2
#   define MAX_FLOAT_SIZE 320
    
    Tcl_Obj *resultPtr;  	/* Where result is stored finally. */
    char staticBuf[MAX_FLOAT_SIZE + 1];
                                /* A static buffer to copy the format results 
				 * into */
    char *dst = staticBuf;      /* The buffer that sprintf writes into each
				 * time the format processes a specifier */
    int dstSize = MAX_FLOAT_SIZE;
                                /* The size of the dst buffer */
    int noPercent;		/* Special case for speed:  indicates there's
				 * no field specifier, just a string to copy.*/
    int objIndex;		/* Index of argument to substitute next. */
    int gotXpg = 0;		/* Non-zero means that an XPG3 %n$-style
				 * specifier has been seen. */
    int gotSequential = 0;	/* Non-zero means that a regular sequential
				 * (non-XPG3) conversion specifier has been
				 * seen. */
    int useShort;		/* Value to be printed is short (half word). */
    char *end;			/* Used to locate end of numerical fields. */

    /*
     * This procedure is a bit nasty.  The goal is to use sprintf to
     * do most of the dirty work.  There are several problems:
     * 1. this procedure can't trust its arguments.
     * 2. we must be able to provide a large enough result area to hold
     *    whatever's generated.  This is hard to estimate.
     * 2. there's no way to move the arguments from objv to the call
     *    to sprintf in a reasonable way.  This is particularly nasty
     *    because some of the arguments may be two-word values (doubles).
     * So, what happens here is to scan the format string one % group
     * at a time, making many individual calls to sprintf.
     */

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv,
		"formatString ?arg arg ...?");
	return TCL_ERROR;
    }

    format = Tcl_GetStringFromObj(objv[1], &formatLen);
    endPtr = format + formatLen;
    resultPtr = Tcl_NewObj();
    objIndex = 2;

    while (format < endPtr) {
	register char *newPtr = newFormat;

	width = precision = noPercent = useShort = 0;
	whichValue = PTR_VALUE;

	/*
	 * Get rid of any characters before the next field specifier.
	 */
	if (*format != '%') {
	    ptrValue = format;
	    while ((*format != '%') && (format < endPtr)) {
		format++;
	    }
	    size = format - ptrValue;
	    noPercent = 1;
	    goto doField;
	}

	if (format[1] == '%') {
	    ptrValue = format;
	    size = 1;
	    noPercent = 1;
	    format += 2;
	    goto doField;
	}

	/*
	 * Parse off a field specifier, compute how many characters
	 * will be needed to store the result, and substitute for
	 * "*" size specifiers.
	 */
	*newPtr = '%';
	newPtr++;
	format++;
	if (isdigit(UCHAR(*format))) {
	    int tmp;

	    /*
	     * Check for an XPG3-style %n$ specification.  Note: there
	     * must not be a mixture of XPG3 specs and non-XPG3 specs
	     * in the same format string.
	     */

	    tmp = strtoul(format, &end, 10);
	    if (*end != '$') {
		goto notXpg;
	    }
	    format = end+1;
	    gotXpg = 1;
	    if (gotSequential) {
		goto mixedXPG;
	    }
	    objIndex = tmp+1;
	    if ((objIndex < 2) || (objIndex >= objc)) {
		goto badIndex;
	    }
	    goto xpgCheckDone;
	}

	notXpg:
	gotSequential = 1;
	if (gotXpg) {
	    goto mixedXPG;
	}

	xpgCheckDone:
	while ((*format == '-') || (*format == '#') || (*format == '0')
		|| (*format == ' ') || (*format == '+')) {
	    *newPtr = *format;
	    newPtr++;
	    format++;
	}
	if (isdigit(UCHAR(*format))) {
	    width = strtoul(format, &end, 10);
	    format = end;
	} else if (*format == '*') {
	    if (objIndex >= objc) {
		goto badIndex;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[objIndex], 
                    &width) != TCL_OK) {
		goto fmtError;
	    }
	    objIndex++;
	    format++;
	}
	if (width > 100000) {
	    /*
	     * Don't allow arbitrarily large widths:  could cause core
	     * dump when we try to allocate a zillion bytes of memory
	     * below.
	     */

	    width = 100000;
	} else if (width < 0) {
	    width = 0;
	}
	if (width != 0) {
	    TclFormatInt(newPtr, width);
	    while (*newPtr != 0) {
		newPtr++;
	    }
	}
	if (*format == '.') {
	    *newPtr = '.';
	    newPtr++;
	    format++;
	}
	if (isdigit(UCHAR(*format))) {
	    precision = strtoul(format, &end, 10);
	    format = end;
	} else if (*format == '*') {
	    if (objIndex >= objc) {
		goto badIndex;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[objIndex], 
                    &precision) != TCL_OK) {
		goto fmtError;
	    }
	    objIndex++;
	    format++;
	}
	if (precision != 0) {
	    TclFormatInt(newPtr, precision);
	    while (*newPtr != 0) {
		newPtr++;
	    }
	}
	if (*format == 'l') {
	    format++;
	} else if (*format == 'h') {
	    useShort = 1;
	    *newPtr = 'h';
	    newPtr++;
	    format++;
	}
	*newPtr = *format;
	newPtr++;
	*newPtr = 0;
	if (objIndex >= objc) {
	    goto badIndex;
	}
	switch (*format) {
	    case 'i':
		newPtr[-1] = 'd';
	    case 'd':
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
		if (Tcl_GetIntFromObj(interp, objv[objIndex], 
		        (int *) &intValue) != TCL_OK) {
		    goto fmtError;
		}
		whichValue = INT_VALUE;
		size = 40 + precision;
		break;
	    case 's':
		ptrValue = Tcl_GetStringFromObj(objv[objIndex], &size);
		break;
	    case 'c':
		if (Tcl_GetIntFromObj(interp, objv[objIndex], 
                        (int *) &intValue) != TCL_OK) {
		    goto fmtError;
		}
		whichValue = INT_VALUE;
		size = 1;
		break;
	    case 'e':
	    case 'E':
	    case 'f':
	    case 'g':
	    case 'G':
		if (Tcl_GetDoubleFromObj(interp, objv[objIndex], 
			&doubleValue) != TCL_OK) {
		    goto fmtError;
		}
		whichValue = DOUBLE_VALUE;
		size = MAX_FLOAT_SIZE;
		if (precision > 10) {
		    size += precision;
		}
		break;
	    case 0:
		Tcl_SetResult(interp,
		        "format string ended in middle of field specifier",
			TCL_STATIC);
		goto fmtError;
	    default:
		{
		    char buf[40];
		    sprintf(buf, "bad field specifier \"%c\"", *format);
		    Tcl_SetResult(interp, buf, TCL_VOLATILE);
		    goto fmtError;
		}
	}
	objIndex++;
	format++;

	/*
	 * Make sure that there's enough space to hold the formatted
	 * result, then format it.
	 */

	doField:
	if (width > size) {
	    size = width;
	}
	if (noPercent) {
	    Tcl_AppendToObj(resultPtr, ptrValue, size);
	} else {
	    if (size > dstSize) {
	        if (dst != staticBuf) {
		    ckfree(dst);
		}
		dst = (char *) ckalloc((unsigned) (size + 1));
		dstSize = size;
	    }

	    if (whichValue == DOUBLE_VALUE) {
	        sprintf(dst, newFormat, doubleValue);
	    } else if (whichValue == INT_VALUE) {
		if (useShort) {
		    sprintf(dst, newFormat, (short) intValue);
		} else {
		    sprintf(dst, newFormat, intValue);
		}
	    } else {
	        sprintf(dst, newFormat, ptrValue);
	    }
	    Tcl_AppendToObj(resultPtr, dst, -1);
	}
    }

    Tcl_SetObjResult(interp, resultPtr);
    if(dst != staticBuf) {
        ckfree(dst);
    }
    return TCL_OK;

    mixedXPG:
    Tcl_SetResult(interp, 
            "cannot mix \"%\" and \"%n$\" conversion specifiers", TCL_STATIC);
    goto fmtError;

    badIndex:
    if (gotXpg) {
        Tcl_SetResult(interp, 
                "\"%n$\" argument index out of range", TCL_STATIC);
    } else {
        Tcl_SetResult(interp, 
                "not enough arguments for all format specifiers", TCL_STATIC);
    }

    fmtError:
    if(dst != staticBuf) {
        ckfree(dst);
    }
    Tcl_DecrRefCount(resultPtr);
    return TCL_ERROR;
}
