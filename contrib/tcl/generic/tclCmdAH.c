/* 
 * tclCmdAH.c --
 *
 *	This file contains the top-level command routines for most of
 *	the Tcl built-in commands whose names begin with the letters
 *	A to H.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCmdAH.c 1.107 96/04/09 17:14:39
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
 * Tcl_CaseCmd --
 *
 *	This procedure is invoked to process the "case" Tcl command.
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
Tcl_CaseCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i, result;
    int body;
    char *string;
    int caseArgc, splitArgs;
    char **caseArgv;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " string ?in? patList body ... ?default body?\"",
		(char *) NULL);
	return TCL_ERROR;
    }
    string = argv[1];
    body = -1;
    if (strcmp(argv[2], "in") == 0) {
	i = 3;
    } else {
	i = 2;
    }
    caseArgc = argc - i;
    caseArgv = argv + i;

    /*
     * If all of the pattern/command pairs are lumped into a single
     * argument, split them out again.
     */

    splitArgs = 0;
    if (caseArgc == 1) {
	result = Tcl_SplitList(interp, caseArgv[0], &caseArgc, &caseArgv);
	if (result != TCL_OK) {
	    return result;
	}
	splitArgs = 1;
    }

    for (i = 0; i < caseArgc; i += 2) {
	int patArgc, j;
	char **patArgv;
	register char *p;

	if (i == (caseArgc-1)) {
	    interp->result = "extra case pattern with no body";
	    result = TCL_ERROR;
	    goto cleanup;
	}

	/*
	 * Check for special case of single pattern (no list) with
	 * no backslash sequences.
	 */

	for (p = caseArgv[i]; *p != 0; p++) {
	    if (isspace(UCHAR(*p)) || (*p == '\\')) {
		break;
	    }
	}
	if (*p == 0) {
	    if ((*caseArgv[i] == 'd')
		    && (strcmp(caseArgv[i], "default") == 0)) {
		body = i+1;
	    }
	    if (Tcl_StringMatch(string, caseArgv[i])) {
		body = i+1;
		goto match;
	    }
	    continue;
	}

	/*
	 * Break up pattern lists, then check each of the patterns
	 * in the list.
	 */

	result = Tcl_SplitList(interp, caseArgv[i], &patArgc, &patArgv);
	if (result != TCL_OK) {
	    goto cleanup;
	}
	for (j = 0; j < patArgc; j++) {
	    if (Tcl_StringMatch(string, patArgv[j])) {
		body = i+1;
		break;
	    }
	}
	ckfree((char *) patArgv);
	if (j < patArgc) {
	    break;
	}
    }

    match:
    if (body != -1) {
	result = Tcl_Eval(interp, caseArgv[body]);
	if (result == TCL_ERROR) {
	    char msg[100];
	    sprintf(msg, "\n    (\"%.50s\" arm line %d)", caseArgv[body-1],
		    interp->errorLine);
	    Tcl_AddErrorInfo(interp, msg);
	}
	goto cleanup;
    }

    /*
     * Nothing matched:  return nothing.
     */

    result = TCL_OK;

    cleanup:
    if (splitArgs) {
	ckfree((char *) caseArgv);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CatchCmd --
 *
 *	This procedure is invoked to process the "catch" Tcl command.
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
Tcl_CatchCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int result;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " command ?varName?\"", (char *) NULL);
	return TCL_ERROR;
    }
    result = Tcl_Eval(interp, argv[1]);
    if (argc == 3) {
	if (Tcl_SetVar(interp, argv[2], interp->result, 0) == NULL) {
	    Tcl_SetResult(interp, "couldn't save command result in variable",
		    TCL_STATIC);
	    return TCL_ERROR;
	}
    }
    Tcl_ResetResult(interp);
    sprintf(interp->result, "%d", result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CdCmd --
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
Tcl_CdCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *dirName;
    Tcl_DString buffer;
    int result;

    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" dirName\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
	dirName = argv[1];
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
 * Tcl_ConcatCmd --
 *
 *	This procedure is invoked to process the "concat" Tcl command.
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
Tcl_ConcatCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc >= 2) {
	interp->result = Tcl_Concat(argc-1, argv+1);
	interp->freeProc = TCL_DYNAMIC;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ContinueCmd --
 *
 *	This procedure is invoked to process the "continue" Tcl command.
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
 * Tcl_ErrorCmd --
 *
 *	This procedure is invoked to process the "error" Tcl command.
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
Tcl_ErrorCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Interp *iPtr = (Interp *) interp;

    if ((argc < 2) || (argc > 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" message ?errorInfo? ?errorCode?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if ((argc >= 3) && (argv[2][0] != 0)) {
	Tcl_AddErrorInfo(interp, argv[2]);
	iPtr->flags |= ERR_ALREADY_LOGGED;
    }
    if (argc == 4) {
	Tcl_SetVar2(interp, "errorCode", (char *) NULL, argv[3],
		TCL_GLOBAL_ONLY);
	iPtr->flags |= ERROR_CODE_SET;
    }
    Tcl_SetResult(interp, argv[1], TCL_VOLATILE);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EvalCmd --
 *
 *	This procedure is invoked to process the "eval" Tcl command.
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
Tcl_EvalCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int result;
    char *cmd;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 2) {
	result = Tcl_Eval(interp, argv[1]);
    } else {
    
	/*
	 * More than one argument:  concatenate them together with spaces
	 * between, then evaluate the result.
	 */
    
	cmd = Tcl_Concat(argc-1, argv+1);
	result = Tcl_Eval(interp, cmd);
	ckfree(cmd);
    }
    if (result == TCL_ERROR) {
	char msg[60];
	sprintf(msg, "\n    (\"eval\" body line %d)", interp->errorLine);
	Tcl_AddErrorInfo(interp, msg);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExitCmd --
 *
 *	This procedure is invoked to process the "exit" Tcl command.
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
Tcl_ExitCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int value;

    if ((argc != 1) && (argc != 2)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?returnCode?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 1) {
	value = 0;
    } else if (Tcl_GetInt(interp, argv[1], &value) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_Exit(value);
    /*NOTREACHED*/
    return TCL_OK;			/* Better not ever reach this! */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExprCmd --
 *
 *	This procedure is invoked to process the "expr" Tcl command.
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
Tcl_ExprCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_DString buffer;
    int i, result;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
	return Tcl_ExprString(interp, argv[1]);
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, argv[1], -1);
    for (i = 2; i < argc; i++) {
	Tcl_DStringAppend(&buffer, " ", 1);
	Tcl_DStringAppend(&buffer, argv[i], -1);
    }
    result = Tcl_ExprString(interp, buffer.string);
    Tcl_DStringFree(&buffer);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileCmd --
 *
 *	This procedure is invoked to process the "file" Tcl command.
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
Tcl_FileCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *fileName, *extension;
    int c, statOp, result;
    size_t length;
    int mode = 0;			/* Initialized only to prevent
					 * compiler warning message. */
    struct stat statBuf;
    Tcl_DString buffer;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option name ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    result = TCL_OK;
    Tcl_DStringInit(&buffer);

    /*
     * First handle operations on the file name.
     */

    if ((c == 'd') && (strncmp(argv[1], "dirname", length) == 0)) {
	int pargc;
	char **pargv;

	if (argc != 3) {
	    argv[1] = "dirname";
	    goto not3Args;
	}

	fileName = argv[2];

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
	    Tcl_DStringResult(interp, &buffer);
	} else if ((pargc == 0)
		|| (Tcl_GetPathType(pargv[0]) == TCL_PATH_RELATIVE)) {
	    Tcl_SetResult(interp,
		    (tclPlatform == TCL_PLATFORM_MAC) ? ":" : ".", TCL_STATIC);
	} else {
	    Tcl_SetResult(interp, pargv[0], TCL_VOLATILE);
	}
	ckfree((char *)pargv);
	goto done;

    } else if ((c == 't') && (strncmp(argv[1], "tail", length) == 0)
	&& (length >= 2)) {
	int pargc;
	char **pargv;

	if (argc != 3) {
	    argv[1] = "tail";
	    goto not3Args;
	}

	Tcl_SplitPath(argv[2], &pargc, &pargv);
	if (pargc > 0) {
	    if ((pargc > 1)
		    || (Tcl_GetPathType(pargv[0]) == TCL_PATH_RELATIVE)) {
		Tcl_SetResult(interp, pargv[pargc-1], TCL_VOLATILE);
	    }
	}
	ckfree((char *)pargv);
	goto done;

    } else if ((c == 'r') && (strncmp(argv[1], "rootname", length) == 0)
	    && (length >= 2)) {
	char tmp;
	if (argc != 3) {
	    argv[1] = "rootname";
	    goto not3Args;
	}
	extension = TclGetExtension(argv[2]);
	if (extension == NULL) {
	    Tcl_SetResult(interp, argv[2], TCL_VOLATILE);
	} else {
	    tmp = *extension;
	    *extension = 0;
	    Tcl_SetResult(interp, argv[2], TCL_VOLATILE);
	    *extension = tmp;
	}
	goto done;
    } else if ((c == 'e') && (strncmp(argv[1], "extension", length) == 0)
	    && (length >= 3)) {
	if (argc != 3) {
	    argv[1] = "extension";
	    goto not3Args;
	}
	extension = TclGetExtension(argv[2]);

	if (extension != NULL) {
	    Tcl_SetResult(interp, extension, TCL_VOLATILE);
	}
	goto done;
    } else if ((c == 'p') && (strncmp(argv[1], "pathtype", length) == 0)) {
	if (argc != 3) {
	    argv[1] = "pathtype";
	    goto not3Args;
	}
	switch (Tcl_GetPathType(argv[2])) {
	    case TCL_PATH_ABSOLUTE:
		Tcl_SetResult(interp, "absolute", TCL_STATIC);
		break;
	    case TCL_PATH_RELATIVE:
		Tcl_SetResult(interp, "relative", TCL_STATIC);
		break;
	    case TCL_PATH_VOLUME_RELATIVE:
		Tcl_SetResult(interp, "volumerelative", TCL_STATIC);
		break;
	}
	goto done;
    } else if ((c == 's') && (strncmp(argv[1], "split", length) == 0)
	&& (length >= 2)) {
	int pargc, i;
	char **pargvList;

	if (argc != 3) {
	    argv[1] = "split";
	    goto not3Args;
	}

	Tcl_SplitPath(argv[2], &pargc, &pargvList);
	for (i = 0; i < pargc; i++) {
	    Tcl_AppendElement(interp, pargvList[i]);
	}
	ckfree((char *) pargvList);
	goto done;
    } else if ((c == 'j') && (strncmp(argv[1], "join", length) == 0)) {
	Tcl_JoinPath(argc-2, argv+2, &buffer);
	Tcl_DStringResult(interp, &buffer);
	goto done;
    }

    /*
     * Next, handle operations that can be satisfied with the "access"
     * kernel call.
     */

    fileName = Tcl_TranslateFileName(interp, argv[2], &buffer);
    if (fileName == NULL) {
	result = TCL_ERROR;
	goto done;
    }
    if ((c == 'r') && (strncmp(argv[1], "readable", length) == 0)
	    && (length >= 5)) {
	if (argc != 3) {
	    argv[1] = "readable";
	    goto not3Args;
	}
	mode = R_OK;
	checkAccess:
	if (access(fileName, mode) == -1) {
	    interp->result = "0";
	} else {
	    interp->result = "1";
	}
	goto done;
    } else if ((c == 'w') && (strncmp(argv[1], "writable", length) == 0)) {
	if (argc != 3) {
	    argv[1] = "writable";
	    goto not3Args;
	}
	mode = W_OK;
	goto checkAccess;
    } else if ((c == 'e') && (strncmp(argv[1], "executable", length) == 0)
	    && (length >= 3)) {
	if (argc != 3) {
	    argv[1] = "executable";
	    goto not3Args;
	}
	mode = X_OK;
	goto checkAccess;
    } else if ((c == 'e') && (strncmp(argv[1], "exists", length) == 0)
	    && (length >= 3)) {
	if (argc != 3) {
	    argv[1] = "exists";
	    goto not3Args;
	}
	mode = F_OK;
	goto checkAccess;
    }

    /*
     * Lastly, check stuff that requires the file to be stat-ed.
     */

    if ((c == 'a') && (strncmp(argv[1], "atime", length) == 0)) {
	if (argc != 3) {
	    argv[1] = "atime";
	    goto not3Args;
	}
	if (stat(fileName, &statBuf) == -1) {
	    goto badStat;
	}
	sprintf(interp->result, "%ld", (long) statBuf.st_atime);
	goto done;
    } else if ((c == 'i') && (strncmp(argv[1], "isdirectory", length) == 0)
	    && (length >= 3)) {
	if (argc != 3) {
	    argv[1] = "isdirectory";
	    goto not3Args;
	}
	statOp = 2;
    } else if ((c == 'i') && (strncmp(argv[1], "isfile", length) == 0)
	    && (length >= 3)) {
	if (argc != 3) {
	    argv[1] = "isfile";
	    goto not3Args;
	}
	statOp = 1;
    } else if ((c == 'l') && (strncmp(argv[1], "lstat", length) == 0)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " lstat name varName\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}

	if (lstat(fileName, &statBuf) == -1) {
	    Tcl_AppendResult(interp, "couldn't lstat \"", argv[2],
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	result = StoreStatData(interp, argv[3], &statBuf);
	goto done;
    } else if ((c == 'm') && (strncmp(argv[1], "mtime", length) == 0)) {
	if (argc != 3) {
	    argv[1] = "mtime";
	    goto not3Args;
	}
	if (stat(fileName, &statBuf) == -1) {
	    goto badStat;
	}
	sprintf(interp->result, "%ld", (long) statBuf.st_mtime);
	goto done;
    } else if ((c == 'o') && (strncmp(argv[1], "owned", length) == 0)) {
	if (argc != 3) {
	    argv[1] = "owned";
	    goto not3Args;
	}
	statOp = 0;
    } else if ((c == 'r') && (strncmp(argv[1], "readlink", length) == 0)
	    && (length >= 5)) {
	char linkValue[MAXPATHLEN+1];
	int linkLength;

	if (argc != 3) {
	    argv[1] = "readlink";
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
	    Tcl_AppendResult(interp, "couldn't readlink \"", argv[2],
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	linkValue[linkLength] = 0;
	Tcl_SetResult(interp, linkValue, TCL_VOLATILE);
	goto done;
    } else if ((c == 's') && (strncmp(argv[1], "size", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    argv[1] = "size";
	    goto not3Args;
	}
	if (stat(fileName, &statBuf) == -1) {
	    goto badStat;
	}
	sprintf(interp->result, "%lu", (unsigned long) statBuf.st_size);
	goto done;
    } else if ((c == 's') && (strncmp(argv[1], "stat", length) == 0)
	    && (length >= 2)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " stat name varName\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}

	if (stat(fileName, &statBuf) == -1) {
	    badStat:
	    Tcl_AppendResult(interp, "couldn't stat \"", argv[2],
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	result = StoreStatData(interp, argv[3], &statBuf);
	goto done;
    } else if ((c == 't') && (strncmp(argv[1], "type", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    argv[1] = "type";
	    goto not3Args;
	}
	if (lstat(fileName, &statBuf) == -1) {
	    goto badStat;
	}
	interp->result = GetTypeFromMode((int) statBuf.st_mode);
	goto done;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be atime, dirname, executable, exists, ",
		"extension, isdirectory, isfile, join, ",
		"lstat, mtime, owned, pathtype, readable, readlink, ",
		"root, size, split, stat, tail, type, ",
		"or writable",
		(char *) NULL);
	result = TCL_ERROR;
	goto done;
    }
    if (stat(fileName, &statBuf) == -1) {
	interp->result = "0";
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
    if (mode) {
	interp->result = "1";
    } else {
	interp->result = "0";
    }

    done:
    Tcl_DStringFree(&buffer);
    return result;

    not3Args:
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " ", argv[1], " name\"", (char *) NULL);
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
	    GetTypeFromMode((int) statPtr->st_mode), TCL_LEAVE_ERR_MSG) == NULL) {
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
    } else if (S_ISLNK(mode)) {
	return "link";
    } else if (S_ISSOCK(mode)) {
	return "socket";
    }
    return "unknown";
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ForCmd --
 *
 *	This procedure is invoked to process the "for" Tcl command.
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
Tcl_ForCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
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
		sprintf(msg, "\n    (\"for\" body line %d)", interp->errorLine);
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
 * Tcl_ForeachCmd --
 *
 *	This procedure is invoked to process the "foreach" Tcl command.
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
Tcl_ForeachCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int result = TCL_OK;
    int i;			/* i selects a value list */
    int j, maxj;		/* Number of loop iterations */
    int v;			/* v selects a loop variable */
    int numLists;		/* Count of value lists */
#define STATIC_SIZE 4
    int indexArray[STATIC_SIZE];	/* Array of value list indices */
    int varcListArray[STATIC_SIZE];	/* Number of loop variables per list */
    char **varvListArray[STATIC_SIZE];	/* Array of variable name lists */
    int argcListArray[STATIC_SIZE];	/* Array of value list sizes */
    char **argvListArray[STATIC_SIZE];	/* Array of value lists */

    int *index = indexArray;
    int *varcList = varcListArray;
    char ***varvList = varvListArray;
    int *argcList = argcListArray;
    char ***argvList = argvListArray;

    if (argc < 4 || (argc%2 != 0)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" varList list ?varList list ...? command\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Manage numList parallel value lists.
     * argvList[i] is a value list counted by argcList[i]
     * varvList[i] is the list of variables associated with the value list
     * varcList[i] is the number of variables associated with the value list
     * index[i] is the current pointer into the value list argvList[i]
     */

    numLists = (argc-2)/2;
    if (numLists > STATIC_SIZE) {
	index = (int *) ckalloc(numLists * sizeof(int));
	varcList = (int *) ckalloc(numLists * sizeof(int));
	varvList = (char ***) ckalloc(numLists * sizeof(char **));
	argcList = (int *) ckalloc(numLists * sizeof(int));
	argvList = (char ***) ckalloc(numLists * sizeof(char **));
    }
    for (i=0 ; i<numLists ; i++) {
	index[i] = 0;
	varcList[i] = 0;
	varvList[i] = (char **)NULL;
	argcList[i] = 0;
	argvList[i] = (char **)NULL;
    }

    /*
     * Break up the value lists and variable lists into elements
     */

    maxj = 0;
    for (i=0 ; i<numLists ; i++) {
	result = Tcl_SplitList(interp, argv[1+i*2], &varcList[i], &varvList[i]);
	if (result != TCL_OK) {
	    goto errorReturn;
	}
	result = Tcl_SplitList(interp, argv[2+i*2], &argcList[i], &argvList[i]);
	if (result != TCL_OK) {
	    goto errorReturn;
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
    for (j = 0; j < maxj; j++) {
	for (i=0 ; i<numLists ; i++) {
	    for (v=0 ; v<varcList[i] ; v++) {
		int k = index[i]++;
		char *value = "";
		if (k < argcList[i]) {
		    value = argvList[i][k];
		}
		if (Tcl_SetVar(interp, varvList[i][v], value, 0) == NULL) {
		    Tcl_AppendResult(interp, "couldn't set loop variable: \"",
			    varvList[i][v], "\"", (char *)NULL);
		    result = TCL_ERROR;
		    goto errorReturn;
		}
	    }
	}

	result = Tcl_Eval(interp, argv[argc-1]);
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
		Tcl_AddErrorInfo(interp, msg);
		break;
	    } else {
		break;
	    }
	}
    }
    if (result == TCL_OK) {
	Tcl_ResetResult(interp);
    }
errorReturn:
    for (i=0 ; i<numLists ; i++) {
	if (argvList[i] != (char **)NULL) {
	    ckfree((char *) argvList[i]);
	}
	if (varvList[i] != (char **)NULL) {
	    ckfree((char *) varvList[i]);
	}
    }
    if (numLists > STATIC_SIZE) {
	ckfree((char *) index);
	ckfree((char *) varcList);
	ckfree((char *) argcList);
	ckfree((char *) varvList);
	ckfree((char *) argvList);
    }
#undef STATIC_SIZE
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FormatCmd --
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
Tcl_FormatCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register char *format;	/* Used to read characters from the format
				 * string. */
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
    char *dst = interp->result;	/* Where result is stored.  Starts off at
				 * interp->resultSpace, but may get dynamically
				 * re-allocated if this isn't enough. */
    int dstSize = 0;		/* Number of non-null characters currently
				 * stored at dst. */
    int dstSpace = TCL_RESULT_SIZE;
				/* Total amount of storage space available
				 * in dst (not including null terminator. */
    int noPercent;		/* Special case for speed:  indicates there's
				 * no field specifier, just a string to copy. */
    int argIndex;		/* Index of argument to substitute next. */
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
     * 2. there's no way to move the arguments from argv to the call
     *    to sprintf in a reasonable way.  This is particularly nasty
     *    because some of the arguments may be two-word values (doubles).
     * So, what happens here is to scan the format string one % group
     * at a time, making many individual calls to sprintf.
     */

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" formatString ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    argIndex = 2;
    for (format = argv[1]; *format != 0; ) {
	register char *newPtr = newFormat;

	width = precision = noPercent = useShort = 0;
	whichValue = PTR_VALUE;

	/*
	 * Get rid of any characters before the next field specifier.
	 */

	if (*format != '%') {
	    register char *p;

	    ptrValue = p = format;
	    while ((*format != '%') && (*format != 0)) {
		*p = *format;
		p++;
		format++;
	    }
	    size = p - ptrValue;
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
	    argIndex = tmp+1;
	    if ((argIndex < 2) || (argIndex >= argc)) {
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
	    if (argIndex >= argc) {
		goto badIndex;
	    }
	    if (Tcl_GetInt(interp, argv[argIndex], &width) != TCL_OK) {
		goto fmtError;
	    }
	    argIndex++;
	    format++;
	}
	if (width > 1000) {
	    /*
	     * Don't allow arbitrarily large widths:  could cause core
	     * dump when we try to allocate a zillion bytes of memory
	     * below.
	     */

	    width = 1000;
	} else if (width < 0) {
	    width = 0;
	}
	if (width != 0) {
	    sprintf(newPtr, "%d", width);
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
	    if (argIndex >= argc) {
		goto badIndex;
	    }
	    if (Tcl_GetInt(interp, argv[argIndex], &precision) != TCL_OK) {
		goto fmtError;
	    }
	    argIndex++;
	    format++;
	}
	if (precision != 0) {
	    sprintf(newPtr, "%d", precision);
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
	if (argIndex >= argc) {
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
		if (Tcl_GetInt(interp, argv[argIndex], (int *) &intValue)
			!= TCL_OK) {
		    goto fmtError;
		}
		whichValue = INT_VALUE;
		size = 40 + precision;
		break;
	    case 's':
		ptrValue = argv[argIndex];
		size = strlen(argv[argIndex]);
		break;
	    case 'c':
		if (Tcl_GetInt(interp, argv[argIndex], (int *) &intValue)
			!= TCL_OK) {
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
		if (Tcl_GetDouble(interp, argv[argIndex], &doubleValue)
			!= TCL_OK) {
		    goto fmtError;
		}
		whichValue = DOUBLE_VALUE;
		size = 320;
		if (precision > 10) {
		    size += precision;
		}
		break;
	    case 0:
		interp->result =
			"format string ended in middle of field specifier";
		goto fmtError;
	    default:
		sprintf(interp->result, "bad field specifier \"%c\"", *format);
		goto fmtError;
	}
	argIndex++;
	format++;

	/*
	 * Make sure that there's enough space to hold the formatted
	 * result, then format it.
	 */

	doField:
	if (width > size) {
	    size = width;
	}
	if ((dstSize + size) > dstSpace) {
	    char *newDst;
	    int newSpace;

	    newSpace = 2*(dstSize + size);
	    newDst = (char *) ckalloc((unsigned) newSpace+1);
	    if (dstSize != 0) {
		memcpy((VOID *) newDst, (VOID *) dst, (size_t) dstSize);
	    }
	    if (dstSpace != TCL_RESULT_SIZE) {
		ckfree(dst);
	    }
	    dst = newDst;
	    dstSpace = newSpace;
	}
	if (noPercent) {
	    memcpy((VOID *) (dst+dstSize), (VOID *) ptrValue, (size_t) size);
	    dstSize += size;
	    dst[dstSize] = 0;
	} else {
	    if (whichValue == DOUBLE_VALUE) {
		sprintf(dst+dstSize, newFormat, doubleValue);
	    } else if (whichValue == INT_VALUE) {
		if (useShort) {
		    sprintf(dst+dstSize, newFormat, (short) intValue);
		} else {
		    sprintf(dst+dstSize, newFormat, intValue);
		}
	    } else {
		sprintf(dst+dstSize, newFormat, ptrValue);
	    }
	    dstSize += strlen(dst+dstSize);
	}
    }

    interp->result = dst;
    if (dstSpace != TCL_RESULT_SIZE) {
	interp->freeProc = TCL_DYNAMIC;
    } else {
	interp->freeProc = 0;
    }
    return TCL_OK;

    mixedXPG:
    interp->result = "cannot mix \"%\" and \"%n$\" conversion specifiers";
    goto fmtError;

    badIndex:
    if (gotXpg) {
	interp->result = "\"%n$\" argument index out of range";
    } else {
	interp->result = "not enough arguments for all format specifiers";
    }

    fmtError:
    if (dstSpace != TCL_RESULT_SIZE) {
	ckfree(dst);
    }
    return TCL_ERROR;
}
