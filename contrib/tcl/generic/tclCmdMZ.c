/* 
 * tclCmdMZ.c --
 *
 *	This file contains the top-level command routines for most of
 *	the Tcl built-in commands whose names begin with the letters
 *	M to Z.  It contains only commands in the generic core (i.e.
 *	those that don't depend much upon UNIX facilities).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCmdMZ.c 1.104 97/10/31 13:06:19
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclCompile.h"

/*
 * Structure used to hold information about variable traces:
 */

typedef struct {
    int flags;			/* Operations for which Tcl command is
				 * to be invoked. */
    char *errMsg;		/* Error message returned from Tcl command,
				 * or NULL.  Malloc'ed. */
    int length;			/* Number of non-NULL chars. in command. */
    char command[4];		/* Space for Tcl command to invoke.  Actual
				 * size will be as large as necessary to
				 * hold command.  This field must be the
				 * last in the structure, so that it can
				 * be larger than 4 bytes. */
} TraceVarInfo;

/*
 * Forward declarations for procedures defined in this file:
 */

static char *		TraceVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PwdCmd --
 *
 *	This procedure is invoked to process the "pwd" Tcl command.
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
Tcl_PwdCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *dirName;

    if (argc != 1) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], "\"", (char *) NULL);
	return TCL_ERROR;
    }

    dirName = TclGetCwd(interp);
    if (dirName == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, dirName, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegexpCmd --
 *
 *	This procedure is invoked to process the "regexp" Tcl command.
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
Tcl_RegexpCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int noCase = 0;
    int indices = 0;
    Tcl_RegExp regExpr;
    char **argPtr, *string, *pattern, *start, *end;
    int match = 0;			/* Initialization needed only to
					 * prevent compiler warning. */
    int i;
    Tcl_DString stringDString, patternDString;

    if (argc < 3) {
	wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?switches? exp string ?matchVar? ?subMatchVar ",
		"subMatchVar ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    argPtr = argv+1;
    argc--;
    while ((argc > 0) && (argPtr[0][0] == '-')) {
	if (strcmp(argPtr[0], "-indices") == 0) {
	    indices = 1;
	} else if (strcmp(argPtr[0], "-nocase") == 0) {
	    noCase = 1;
	} else if (strcmp(argPtr[0], "--") == 0) {
	    argPtr++;
	    argc--;
	    break;
	} else {
	    Tcl_AppendResult(interp, "bad switch \"", argPtr[0],
		    "\": must be -indices, -nocase, or --", (char *) NULL);
	    return TCL_ERROR;
	}
	argPtr++;
	argc--;
    }
    if (argc < 2) {
	goto wrongNumArgs;
    }

    /*
     * Convert the string and pattern to lower case, if desired, and
     * perform the matching operation.
     */

    if (noCase) {
	register char *p;

	Tcl_DStringInit(&patternDString);
	Tcl_DStringAppend(&patternDString, argPtr[0], -1);
	pattern = Tcl_DStringValue(&patternDString);
	for (p = pattern; *p != 0; p++) {
	    if (isupper(UCHAR(*p))) {
		*p = (char)tolower(UCHAR(*p));
	    }
	}
	Tcl_DStringInit(&stringDString);
	Tcl_DStringAppend(&stringDString, argPtr[1], -1);
	string = Tcl_DStringValue(&stringDString);
	for (p = string; *p != 0; p++) {
	    if (isupper(UCHAR(*p))) {
		*p = (char)tolower(UCHAR(*p));
	    }
	}
    } else {
	pattern = argPtr[0];
	string = argPtr[1];
    }
    regExpr = Tcl_RegExpCompile(interp, pattern);
    if (regExpr != NULL) {
	match = Tcl_RegExpExec(interp, regExpr, string, string);
    }
    if (noCase) {
	Tcl_DStringFree(&stringDString);
	Tcl_DStringFree(&patternDString);
    }
    if (regExpr == NULL) {
	return TCL_ERROR;
    }
    if (match < 0) {
	return TCL_ERROR;
    }
    if (!match) {
	Tcl_SetResult(interp, "0", TCL_STATIC);
	return TCL_OK;
    }

    /*
     * If additional variable names have been specified, return
     * index information in those variables.
     */

    argc -= 2;
    for (i = 0; i < argc; i++) {
	char *result, info[50];

	Tcl_RegExpRange(regExpr, i, &start, &end);
	if (start == NULL) {
	    if (indices) {
		result = Tcl_SetVar(interp, argPtr[i+2], "-1 -1", 0);
	    } else {
		result = Tcl_SetVar(interp, argPtr[i+2], "", 0);
	    }
	} else {
	    if (indices) {
		sprintf(info, "%d %d", (int)(start - string),
			(int)(end - string - 1));
		result = Tcl_SetVar(interp, argPtr[i+2], info, 0);
	    } else {
		char savedChar, *first, *last;

		first = argPtr[1] + (start - string);
		last = argPtr[1] + (end - string);
		if (first == last) { /* don't modify argument */
		    result = Tcl_SetVar(interp, argPtr[i+2], "", 0);
		} else {
		    savedChar = *last;
		    *last = 0;
		    result = Tcl_SetVar(interp, argPtr[i+2], first, 0);
		    *last = savedChar;
		}
	    }
	}
	if (result == NULL) {
	    Tcl_AppendResult(interp, "couldn't set variable \"",
		    argPtr[i+2], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    Tcl_SetResult(interp, "1", TCL_STATIC);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegsubCmd --
 *
 *	This procedure is invoked to process the "regsub" Tcl command.
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
Tcl_RegsubCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int noCase = 0, all = 0;
    Tcl_RegExp regExpr;
    char *string, *pattern, *p, *firstChar, **argPtr;
    int match, code, numMatches;
    char *start, *end, *subStart, *subEnd;
    register char *src, c;
    Tcl_DString stringDString, patternDString, resultDString;

    if (argc < 5) {
	wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?switches? exp string subSpec varName\"", (char *) NULL);
	return TCL_ERROR;
    }
    argPtr = argv+1;
    argc--;
    while (argPtr[0][0] == '-') {
	if (strcmp(argPtr[0], "-nocase") == 0) {
	    noCase = 1;
	} else if (strcmp(argPtr[0], "-all") == 0) {
	    all = 1;
	} else if (strcmp(argPtr[0], "--") == 0) {
	    argPtr++;
	    argc--;
	    break;
	} else {
	    Tcl_AppendResult(interp, "bad switch \"", argPtr[0],
		    "\": must be -all, -nocase, or --", (char *) NULL);
	    return TCL_ERROR;
	}
	argPtr++;
	argc--;
    }
    if (argc != 4) {
	goto wrongNumArgs;
    }

    /*
     * Convert the string and pattern to lower case, if desired.
     */

    if (noCase) {
	Tcl_DStringInit(&patternDString);
	Tcl_DStringAppend(&patternDString, argPtr[0], -1);
	pattern = Tcl_DStringValue(&patternDString);
	for (p = pattern; *p != 0; p++) {
	    if (isupper(UCHAR(*p))) {
		*p = (char)tolower(UCHAR(*p));
	    }
	}
	Tcl_DStringInit(&stringDString);
	Tcl_DStringAppend(&stringDString, argPtr[1], -1);
	string = Tcl_DStringValue(&stringDString);
	for (p = string; *p != 0; p++) {
	    if (isupper(UCHAR(*p))) {
		*p = (char)tolower(UCHAR(*p));
	    }
	}
    } else {
	pattern = argPtr[0];
	string = argPtr[1];
    }
    Tcl_DStringInit(&resultDString);
    regExpr = Tcl_RegExpCompile(interp, pattern);
    if (regExpr == NULL) {
	code = TCL_ERROR;
	goto done;
    }

    /*
     * The following loop is to handle multiple matches within the
     * same source string;  each iteration handles one match and its
     * corresponding substitution.  If "-all" hasn't been specified
     * then the loop body only gets executed once.
     */

    numMatches = 0;
    for (p = string; *p != 0; ) {
	match = Tcl_RegExpExec(interp, regExpr, p, string);
	if (match < 0) {
	    code = TCL_ERROR;
	    goto done;
	}
	if (!match) {
	    break;
	}
	numMatches += 1;

	/*
	 * Copy the portion of the source string before the match to the
	 * result variable.
	 */

	Tcl_RegExpRange(regExpr, 0, &start, &end);
	Tcl_DStringAppend(&resultDString, argPtr[1] + (p - string), start - p);
    
	/*
	 * Append the subSpec argument to the variable, making appropriate
	 * substitutions.  This code is a bit hairy because of the backslash
	 * conventions and because the code saves up ranges of characters in
	 * subSpec to reduce the number of calls to Tcl_SetVar.
	 */
    
	for (src = firstChar = argPtr[2], c = *src; c != 0; src++, c = *src) {
	    int index;
    
	    if (c == '&') {
		index = 0;
	    } else if (c == '\\') {
		c = src[1];
		if ((c >= '0') && (c <= '9')) {
		    index = c - '0';
		} else if ((c == '\\') || (c == '&')) {
		    *src = c;
		    src[1] = 0;
		    Tcl_DStringAppend(&resultDString, firstChar, -1);
		    *src = '\\';
		    src[1] = c;
		    firstChar = src+2;
		    src++;
		    continue;
		} else {
		    continue;
		}
	    } else {
		continue;
	    }
	    if (firstChar != src) {
		c = *src;
		*src = 0;
		Tcl_DStringAppend(&resultDString, firstChar, -1);
		*src = c;
	    }
	    Tcl_RegExpRange(regExpr, index, &subStart, &subEnd);
	    if ((subStart != NULL) && (subEnd != NULL)) {
		char *first, *last, saved;
    
		first = argPtr[1] + (subStart - string);
		last = argPtr[1] + (subEnd - string);
		saved = *last;
		*last = 0;
		Tcl_DStringAppend(&resultDString, first, -1);
		*last = saved;
	    }
	    if (*src == '\\') {
		src++;
	    }
	    firstChar = src+1;
	}
	if (firstChar != src) {
	    Tcl_DStringAppend(&resultDString, firstChar, -1);
	}
	if (end == p) {

	    /*
	     * Always consume at least one character of the input string
	     * in order to prevent infinite loops.
	     */

	    Tcl_DStringAppend(&resultDString, argPtr[1] + (p - string), 1);
	    p = end + 1;
	} else {
	    p = end;
	}
	if (!all) {
	    break;
	}
    }

    /*
     * Copy the portion of the source string after the last match to the
     * result variable.
     */

    if ((*p != 0) || (numMatches == 0)) {
	Tcl_DStringAppend(&resultDString, argPtr[1] + (p - string), -1);
    }
    if (Tcl_SetVar(interp, argPtr[3], Tcl_DStringValue(&resultDString), 0)
	     == NULL) {
	Tcl_AppendResult(interp,
		"couldn't set variable \"", argPtr[3], "\"",
		(char *) NULL);
	code = TCL_ERROR;
    } else {
	char buf[40];
	
	TclFormatInt(buf, numMatches);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	code = TCL_OK;
    }

    done:
    if (noCase) {
	Tcl_DStringFree(&stringDString);
	Tcl_DStringFree(&patternDString);
    }
    Tcl_DStringFree(&resultDString);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RenameObjCmd --
 *
 *	This procedure is invoked to process the "rename" Tcl command.
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
Tcl_RenameObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Arbitrary value passed to the command. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *oldName, *newName;
    
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "oldName newName");
	return TCL_ERROR;
    }

    oldName = Tcl_GetStringFromObj(objv[1], (int *) NULL);
    newName = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    return TclRenameCommand(interp, oldName, newName);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ReturnObjCmd --
 *
 *	This object-based procedure is invoked to process the "return" Tcl
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
Tcl_ReturnObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    int optionLen, argLen, code, result;

    if (iPtr->errorInfo != NULL) {
	ckfree(iPtr->errorInfo);
	iPtr->errorInfo = NULL;
    }
    if (iPtr->errorCode != NULL) {
	ckfree(iPtr->errorCode);
	iPtr->errorCode = NULL;
    }
    code = TCL_OK;

   /*
    * THIS FAILS IF AN OBJECT CONTAINS AN EMBEDDED NULL.
    */
    
    for (objv++, objc--;  objc > 1;  objv += 2, objc -= 2) {
	char *option = Tcl_GetStringFromObj(objv[0], &optionLen);
	char *arg = Tcl_GetStringFromObj(objv[1], &argLen);
    	
	if (strcmp(option, "-code") == 0) {
	    register int c = arg[0];
	    if ((c == 'o') && (strcmp(arg, "ok") == 0)) {
		code = TCL_OK;
	    } else if ((c == 'e') && (strcmp(arg, "error") == 0)) {
		code = TCL_ERROR;
	    } else if ((c == 'r') && (strcmp(arg, "return") == 0)) {
		code = TCL_RETURN;
	    } else if ((c == 'b') && (strcmp(arg, "break") == 0)) {
		code = TCL_BREAK;
	    } else if ((c == 'c') && (strcmp(arg, "continue") == 0)) {
		code = TCL_CONTINUE;
	    } else {
		result = Tcl_GetIntFromObj((Tcl_Interp *) NULL, objv[1],
		        &code);
		if (result != TCL_OK) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			    "bad completion code \"",
			    Tcl_GetStringFromObj(objv[1], (int *) NULL),
			    "\": must be ok, error, return, break, ",
			    "continue, or an integer", (char *) NULL);
		    return result;
		}
	    }
	} else if (strcmp(option, "-errorinfo") == 0) {
	    iPtr->errorInfo =
		(char *) ckalloc((unsigned) (strlen(arg) + 1));
	    strcpy(iPtr->errorInfo, arg);
	} else if (strcmp(option, "-errorcode") == 0) {
	    iPtr->errorCode =
		(char *) ckalloc((unsigned) (strlen(arg) + 1));
	    strcpy(iPtr->errorCode, arg);
	} else {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "bad option \"", option,
		    "\": must be -code, -errorcode, or -errorinfo",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    }
    
    if (objc == 1) {
	/*
	 * Set the interpreter's object result. An inline version of
	 * Tcl_SetObjResult.
	 */

	Tcl_SetObjResult(interp, objv[0]);
    }
    iPtr->returnCode = code;
    return TCL_RETURN;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ScanCmd --
 *
 *	This procedure is invoked to process the "scan" Tcl command.
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
Tcl_ScanCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#   define MAX_FIELDS 20
    typedef struct {
	char fmt;			/* Format for field. */
	int size;			/* How many bytes to allow for
					 * field. */
	char *location;			/* Where field will be stored. */
    } Field;
    Field fields[MAX_FIELDS];		/* Info about all the fields in the
					 * format string. */
    register Field *curField;
    int numFields = 0;			/* Number of fields actually
					 * specified. */
    int suppress;			/* Current field is assignment-
					 * suppressed. */
    int totalSize = 0;			/* Number of bytes needed to store
					 * all results combined. */
    char *results;			/* Where scanned output goes.
					 * Malloced; NULL means not allocated
					 * yet. */
    int numScanned;			/* sscanf's result. */
    register char *fmt;
    int i, widthSpecified, length, code;
    char buf[40];

    /*
     * The variables below are used to hold a copy of the format
     * string, so that we can replace format specifiers like "%f"
     * and "%F" with specifiers like "%lf"
     */

#   define STATIC_SIZE 5
    char copyBuf[STATIC_SIZE], *fmtCopy;
    register char *dst;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" string format ?varName varName ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * This procedure operates in four stages:
     * 1. Scan the format string, collecting information about each field.
     * 2. Allocate an array to hold all of the scanned fields.
     * 3. Call sscanf to do all the dirty work, and have it store the
     *    parsed fields in the array.
     * 4. Pick off the fields from the array and assign them to variables.
     */

    code = TCL_OK;
    results = NULL;
    length = strlen(argv[2]) * 2 + 1;
    if (length < STATIC_SIZE) {
	fmtCopy = copyBuf;
    } else {
	fmtCopy = (char *) ckalloc((unsigned) length);
    }
    dst = fmtCopy;
    for (fmt = argv[2]; *fmt != 0; fmt++) {
	*dst = *fmt;
	dst++;
	if (*fmt != '%') {
	    continue;
	}
	fmt++;
	if (*fmt == '%') {
	    *dst = *fmt;
	    dst++;
	    continue;
	}
	if (*fmt == '*') {
	    suppress = 1;
	    *dst = *fmt;
	    dst++;
	    fmt++;
	} else {
	    suppress = 0;
	}
	widthSpecified = 0;
	while (isdigit(UCHAR(*fmt))) {
	    widthSpecified = 1;
	    *dst = *fmt;
	    dst++;
	    fmt++;
	}
	if ((*fmt == 'l') || (*fmt == 'h') || (*fmt == 'L')) {
	    fmt++;
	}
	*dst = *fmt;
	dst++;
	if (suppress) {
	    continue;
	}
	if (numFields == MAX_FIELDS) {
	    Tcl_SetResult(interp, "too many fields to scan", TCL_STATIC);
	    code = TCL_ERROR;
	    goto done;
	}
	curField = &fields[numFields];
	numFields++;
	switch (*fmt) {
	    case 'd':
	    case 'i':
	    case 'o':
	    case 'x':
		curField->fmt = 'd';
		curField->size = sizeof(int);
		break;

	    case 'u':
		curField->fmt = 'u';
		curField->size = sizeof(int);
		break;

	    case 's':
		curField->fmt = 's';
		curField->size = strlen(argv[1]) + 1;
		break;

	    case 'c':
                if (widthSpecified) {
		    Tcl_SetResult(interp,
		            "field width may not be specified in %c conversion",
			    TCL_STATIC);
		    code = TCL_ERROR;
		    goto done;
                }
		curField->fmt = 'c';
		curField->size = sizeof(int);
		break;

	    case 'e':
	    case 'f':
	    case 'g':
		dst[-1] = 'l';
		dst[0] = 'f';
		dst++;
		curField->fmt = 'f';
		curField->size = sizeof(double);
		break;

	    case '[':
		curField->fmt = 's';
		curField->size = strlen(argv[1]) + 1;
		do {
		    fmt++;
		    if (*fmt == 0) {
			Tcl_SetResult(interp,
			        "unmatched [ in format string", TCL_STATIC);
			code = TCL_ERROR;
			goto done;
		    }
		    *dst = *fmt;
		    dst++;
		} while (*fmt != ']');
		break;

	    default:
		{
		    char buf[50];

		    sprintf(buf, "bad scan conversion character \"%c\"", *fmt);
		    Tcl_SetResult(interp, buf, TCL_VOLATILE);
		    code = TCL_ERROR;
		    goto done;
		}
	}
	curField->size = TCL_ALIGN(curField->size);
	totalSize += curField->size;
    }
    *dst = 0;

    if (numFields != (argc-3)) {
	Tcl_SetResult(interp,
		"different numbers of variable names and field specifiers",
		TCL_STATIC);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Step 2:
     */

    results = (char *) ckalloc((unsigned) totalSize);
    for (i = 0, totalSize = 0, curField = fields;
	    i < numFields; i++, curField++) {
	curField->location = results + totalSize;
	totalSize += curField->size;
    }

    /*
     * Fill in the remaining fields with NULL;  the only purpose of
     * this is to keep some memory analyzers, like Purify, from
     * complaining.
     */

    for ( ; i < MAX_FIELDS; i++, curField++) {
	curField->location = NULL;
    }

    /*
     * Step 3:
     */

    numScanned = sscanf(argv[1], fmtCopy,
	    fields[0].location, fields[1].location, fields[2].location,
	    fields[3].location, fields[4].location, fields[5].location,
	    fields[6].location, fields[7].location, fields[8].location,
	    fields[9].location, fields[10].location, fields[11].location,
	    fields[12].location, fields[13].location, fields[14].location,
	    fields[15].location, fields[16].location, fields[17].location,
	    fields[18].location, fields[19].location);

    /*
     * Step 4:
     */

    if (numScanned < numFields) {
	numFields = numScanned;
    }
    for (i = 0, curField = fields; i < numFields; i++, curField++) {
	switch (curField->fmt) {
	    char string[TCL_DOUBLE_SPACE];

	    case 'd':
		TclFormatInt(string, *((int *) curField->location));
		if (Tcl_SetVar(interp, argv[i+3], string, 0) == NULL) {
		    storeError:
		    Tcl_AppendResult(interp,
			    "couldn't set variable \"", argv[i+3], "\"",
			    (char *) NULL);
		    code = TCL_ERROR;
		    goto done;
		}
		break;

	    case 'u':
		sprintf(string, "%u", *((int *) curField->location));
		if (Tcl_SetVar(interp, argv[i+3], string, 0) == NULL) {
		    goto storeError;
		}
		break;

	    case 'c':
		TclFormatInt(string, *((char *) curField->location) & 0xff);
		if (Tcl_SetVar(interp, argv[i+3], string, 0) == NULL) {
		    goto storeError;
		}
		break;

	    case 's':
		if (Tcl_SetVar(interp, argv[i+3], curField->location, 0)
			== NULL) {
		    goto storeError;
		}
		break;

	    case 'f':
		Tcl_PrintDouble((Tcl_Interp *) NULL,
			*((double *) curField->location), string);
		if (Tcl_SetVar(interp, argv[i+3], string, 0) == NULL) {
		    goto storeError;
		}
		break;
	}
    }
    TclFormatInt(buf, numScanned);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    done:
    if (results != NULL) {
	ckfree(results);
    }
    if (fmtCopy != copyBuf) {
	ckfree(fmtCopy);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SourceObjCmd --
 *
 *	This procedure is invoked to process the "source" Tcl command.
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
Tcl_SourceObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *bytes;
    int result;
    
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "fileName");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF THE OBJECT'S STRING REP CONTAINS A NULL.
     */

    bytes = Tcl_GetStringFromObj(objv[1], (int *) NULL);
    result = Tcl_EvalFile(interp, bytes);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SplitObjCmd --
 *
 *	This procedure is invoked to process the "split" Tcl command.
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
Tcl_SplitObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register char *p, *p2;
    char *splitChars, *string, *elementStart;
    int splitCharLen, stringLen, i, j;
    Tcl_Obj *listPtr;

    if (objc == 2) {
	splitChars = " \n\t\r";
	splitCharLen = 4;
    } else if (objc == 3) {
	splitChars = Tcl_GetStringFromObj(objv[2], &splitCharLen);
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "string ?splitChars?");
	return TCL_ERROR;
    }

    string = Tcl_GetStringFromObj(objv[1], &stringLen);
    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    
    /*
     * Handle the special case of splitting on every character.
     */

    if (splitCharLen == 0) {
	for (i = 0, p = string;  i < stringLen;  i++, p++) {
	    Tcl_ListObjAppendElement(interp, listPtr,
                    Tcl_NewStringObj(p, 1));
	}
    } else {
	/*
	 * Normal case: split on any of a given set of characters.
	 * Discard instances of the split characters.
	 */

	for (i = 0, p = elementStart = string;  i < stringLen;  i++, p++) {
	    for (j = 0, p2 = splitChars;  j < splitCharLen;  j++, p2++) {
		if (*p2 == *p) {
		    Tcl_ListObjAppendElement(interp, listPtr,
                            Tcl_NewStringObj(elementStart, (p-elementStart)));
		    elementStart = p+1;
		    break;
		}
	    }
	}
	if (p != string) {
	    int remainingChars = stringLen - (elementStart-string);
	    Tcl_ListObjAppendElement(interp, listPtr,
                    Tcl_NewStringObj(elementStart, remainingChars));
	}
    }

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_StringObjCmd --
 *
 *	This procedure is invoked to process the "string" Tcl command.
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
Tcl_StringObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int index, left, right;
    Tcl_Obj *resultPtr;
    char *string1, *string2;
    int length1, length2;
    static char *options[] = {
	"compare",	"first",	"index",	"last",
	"length",	"match",	"range",	"tolower",
	"toupper",	"trim",		"trimleft",	"trimright",
	"wordend",	"wordstart",	NULL
    };
    enum options {
	STR_COMPARE,	STR_FIRST,	STR_INDEX,	STR_LAST,
	STR_LENGTH,	STR_MATCH,	STR_RANGE,	STR_TOLOWER,
	STR_TOUPPER,	STR_TRIM,	STR_TRIMLEFT,	STR_TRIMRIGHT,
	STR_WORDEND,	STR_WORDSTART
    };	  
	    
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }
    
    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    resultPtr = Tcl_GetObjResult(interp);
    switch ((enum options) index) {
	case STR_COMPARE: {
	    int match, length;

	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string1 string2");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    string2 = Tcl_GetStringFromObj(objv[3], &length2);

	    length = (length1 < length2) ? length1 : length2;
	    match = memcmp(string1, string2, (unsigned) length);
	    if (match == 0) {
	        match = length1 - length2;
	    }
	    Tcl_SetIntObj(resultPtr, (match > 0) ? 1 : (match < 0) ? -1 : 0);
	    break;
	}
	case STR_FIRST: {
	    register char *p, *end;
	    int match;

	    if (objc != 4) {
	        badFirstLastArgs:
	        Tcl_WrongNumArgs(interp, 2, objv, "string1 string2");
		return TCL_ERROR;
	    }

	    match = -1;
	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    string2 = Tcl_GetStringFromObj(objv[3], &length2);
	    if (length1 > 0) {
		end = string2 + length2 - length1 + 1;
		for (p = string2;  p < end;  p++) {
		  /*
		   * Scan forward to find the first character.
		   */
		    
		  p = memchr(p, *string1, (unsigned) (end - p));
		  if (p == NULL) {
		      break;
		  }
		  if (memcmp(string1, p, (unsigned) length1) == 0) {
		      match = p - string2;
		      break;
		  }
		}
	    }
	    Tcl_SetIntObj(resultPtr, match);
	    break;
	}
	case STR_INDEX: {
	    int index;

	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string charIndex");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    if (Tcl_GetIntFromObj(interp, objv[3], &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((index >= 0) && (index < length1)) {
	        Tcl_SetStringObj(resultPtr, string1 + index, 1);
	    }
	    break;
	}
	case STR_LAST: {
	    register char *p;
	    int match;

	    if (objc != 4) {
	        goto badFirstLastArgs;
	    }

	    match = -1;
	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    string2 = Tcl_GetStringFromObj(objv[3], &length2);
	    if (length1 > 0) {
		for (p = string2 + length2 - length1;  p >= string2;  p--) {
		    /*
		     * Scan backwards to find the first character.
		     */
		    
		    while ((p != string2) && (*p != *string1)) {
			p--;
		    }
		    if (memcmp(string1, p, (unsigned) length1) == 0) {
			match = p - string2;
			break;
		    }
		}
	    }
	    Tcl_SetIntObj(resultPtr, match);
	    break;
	}
	case STR_LENGTH: {
	    if (objc != 3) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string");
		return TCL_ERROR;
	    }

	    (void) Tcl_GetStringFromObj(objv[2], &length1);
	    Tcl_SetIntObj(resultPtr, length1);
	    break;
	}
	case STR_MATCH: {
	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, "pattern string");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    string2 = Tcl_GetStringFromObj(objv[3], &length2);
	    Tcl_SetBooleanObj(resultPtr, Tcl_StringMatch(string2, string1));
	    break;
	}
	case STR_RANGE: {
	    int first, last;

	    if (objc != 5) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string first last");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    if (TclGetIntForIndex(interp, objv[3], length1 - 1,
		    &first) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (TclGetIntForIndex(interp, objv[4], length1 - 1,
		    &last) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (first < 0) {
		first = 0;
	    }
	    if (last >= length1 - 1) {
		last = length1 - 1;
	    }
	    if (last >= first) {
	        Tcl_SetStringObj(resultPtr, string1 + first, last - first + 1);
	    }
	    break;
	}
	case STR_TOLOWER: {
	    register char *p, *end;

	    if (objc != 3) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);

	    /*
	     * Since I know resultPtr is not a shared object, I can reach
	     * in and diddle the bytes in its string rep to convert them in
	     * place to lower case.
	     */

	    Tcl_SetStringObj(resultPtr, string1, length1);
	    string1 = Tcl_GetStringFromObj(resultPtr, &length1);
	    end = string1 + length1;
	    for (p = string1; p < end; p++) {
		if (isupper(UCHAR(*p))) {
		    *p = (char) tolower(UCHAR(*p));
		}
	    }
	    break;
	}
	case STR_TOUPPER: {
	    register char *p, *end;

	    if (objc != 3) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);

	    /*
	     * Since I know resultPtr is not a shared object, I can reach
	     * in and diddle the bytes in its string rep to convert them in
	     * place to upper case.
	     */

	    Tcl_SetStringObj(resultPtr, string1, length1);
	    string1 = Tcl_GetStringFromObj(resultPtr, &length1);
	    end = string1 + length1;
	    for (p = string1; p < end; p++) {
		if (islower(UCHAR(*p))) {
		    *p = (char) toupper(UCHAR(*p));
		}
	    }
	    break;
	}
	case STR_TRIM: {
	    char ch;
	    register char *p, *end;
	    char *check, *checkEnd;

	    left = 1;
	    right = 1;

	    trim:
	    if (objc == 4) {
		string2 = Tcl_GetStringFromObj(objv[3], &length2);
	    } else if (objc == 3) {
		string2 = " \t\n\r";
		length2 = strlen(string2);
	    } else {
	        Tcl_WrongNumArgs(interp, 2, objv, "string ?chars?");
		return TCL_ERROR;
	    }
	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    checkEnd = string2 + length2;

	    if (left) {
		end = string1 + length1;
		for (p = string1; p < end; p++) {
		    ch = *p;
		    for (check = string2; ; check++) {
			if (check >= checkEnd) {
			    p = end;
			    break;
			}
			if (ch == *check) {
			    length1--;
			    string1++;
			    break;
			}
		    }
		}
	    }
	    if (right) {
	        end = string1;
		for (p = string1 + length1; p > end; ) {
		    p--;
		    ch = *p;
		    for (check = string2; ; check++) {
		        if (check >= checkEnd) {
			    p = end;
			    break;
			}
			if (ch == *check) {
			    length1--;
			    break;
			}
		    }
		}
	    }
	    Tcl_SetStringObj(resultPtr, string1, length1);
	    break;
	}
	case STR_TRIMLEFT: {
	    left = 1;
	    right = 0;
	    goto trim;
	}
	case STR_TRIMRIGHT: {
	    left = 0;
	    right = 1;
	    goto trim;
	}
	case STR_WORDEND: {
	    int cur, c;
	    
	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string index");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    if (Tcl_GetIntFromObj(interp, objv[3], &index) != TCL_OK) {
	        return TCL_ERROR;
	    }
	    if (index < 0) {
		index = 0;
	    }
	    cur = length1;
	    if (index < length1) {
		for (cur = index; cur < length1; cur++) {
		    c = UCHAR(string1[cur]);
		    if (!isalnum(c) && (c != '_')) {
			break;
		    }
		}
		if (cur == index) {
		    cur = index + 1;
		}
	    }
	    Tcl_SetIntObj(resultPtr, cur);
	    break;
	}
	case STR_WORDSTART: {
	    int cur, c;
	    
	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, "string index");
		return TCL_ERROR;
	    }

	    string1 = Tcl_GetStringFromObj(objv[2], &length1);
	    if (Tcl_GetIntFromObj(interp, objv[3], &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (index >= length1) {
		index = length1 - 1;
	    }
	    cur = 0;
	    if (index > 0) {
	        for (cur = index; cur >= 0; cur--) {
		    c = UCHAR(string1[cur]);
		    if (!isalnum(c) && (c != '_')) {
			break;
		    }
		}
		if (cur != index) {
		    cur += 1;
		}
	    }
	    Tcl_SetIntObj(resultPtr, cur);
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SubstCmd --
 *
 *	This procedure is invoked to process the "subst" Tcl command.
 *	See the user documentation for details on what it does.  This
 *	command is an almost direct copy of an implementation by
 *	Andrew Payne.
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
Tcl_SubstCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_DString result;
    char *p, *old, *value;
    int code, count, doVars, doCmds, doBackslashes, i;
    size_t length;
    char c;

    /*
     * Parse command-line options.
     */

    doVars = doCmds = doBackslashes = 1;
    for (i = 1; i < (argc-1); i++) {
	p = argv[i];
	if (*p != '-') {
	    break;
	}
	length = strlen(p);
	if (length < 4) {
	    badSwitch:
	    Tcl_AppendResult(interp, "bad switch \"", p,
		    "\": must be -nobackslashes, -nocommands, ",
		    "or -novariables", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((p[3] == 'b') && (strncmp(p, "-nobackslashes", length) == 0)) {
	    doBackslashes = 0;
	} else if ((p[3] == 'c') && (strncmp(p, "-nocommands", length) == 0)) {
	    doCmds = 0;
	} else if ((p[3] == 'v') && (strncmp(p, "-novariables", length) == 0)) {
	    doVars = 0;
	} else {
	    goto badSwitch;
	}
    }
    if (i != (argc-1)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-nobackslashes? ?-nocommands? ?-novariables? string\"",
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Scan through the string one character at a time, performing
     * command, variable, and backslash substitutions.
     */

    Tcl_DStringInit(&result);
    old = p = argv[i];
    while (*p != 0) {
	switch (*p) {
	    case '\\':
		if (doBackslashes) {
		    if (p != old) {
			Tcl_DStringAppend(&result, old, p-old);
		    }
		    c = Tcl_Backslash(p, &count);
		    Tcl_DStringAppend(&result, &c, 1);
		    p += count;
		    old = p;
		} else {
		    p++;
		}
		break;

	    case '$':
		if (doVars) {
		    if (p != old) {
			Tcl_DStringAppend(&result, old, p-old);
		    }
		    value = Tcl_ParseVar(interp, p, &p);
		    if (value == NULL) {
			Tcl_DStringFree(&result);
			return TCL_ERROR;
		    }
		    Tcl_DStringAppend(&result, value, -1);
		    old = p;
		} else {
		    p++;
		}
		break;

	    case '[':
		if (doCmds) {
		    if (p != old) {
			Tcl_DStringAppend(&result, old, p-old);
		    }
		    iPtr->evalFlags = TCL_BRACKET_TERM;
		    code = Tcl_Eval(interp, p+1);
		    if (code == TCL_ERROR) {
			Tcl_DStringFree(&result);
			return code;
		    }
		    old = p = (p+1 + iPtr->termOffset+1);
		    Tcl_DStringAppend(&result, iPtr->result, -1);
		    Tcl_ResetResult(interp);
		} else {
		    p++;
		}
		break;

	    default:
		p++;
		break;
	}
    }
    if (p != old) {
	Tcl_DStringAppend(&result, old, p-old);
    }
    Tcl_DStringResult(interp, &result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SwitchObjCmd --
 *
 *	This object-based procedure is invoked to process the "switch" Tcl
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
Tcl_SwitchObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
#define EXACT	0
#define GLOB	1
#define REGEXP	2
    int switchObjc, index;
    Tcl_Obj *CONST *switchObjv;
    Tcl_Obj *patternObj, *bodyObj;
    char *string, *pattern, *body;
    int splitObjs, length, patternLen, i, code, mode, matched, bodyIdx;
    static char *switches[] =
	    {"-exact", "-glob", "-regexp", "--", (char *) NULL};

    switchObjc = objc-1;
    switchObjv = objv+1;
    mode = EXACT;

    while (switchObjc > 0) {
	string = Tcl_GetStringFromObj(switchObjv[0], &length);
	if (*string != '-') {
	    break;
	}
	if (Tcl_GetIndexFromObj(interp, switchObjv[0], switches,
		"option", 0, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	    case 0:			/* -exact */
		mode = EXACT;
		break;
	    case 1:			/* -glob */
		mode = GLOB;
		break;
	    case 2:			/* -regexp */
		mode = REGEXP;
		break;
	    case 3:			/* -- */
		switchObjc--;
		switchObjv++;
		goto doneWithSwitches;
	}
	switchObjc--;
	switchObjv++;
    }

    doneWithSwitches:
    if (switchObjc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"?switches? string pattern body ... ?default body?");
	return TCL_ERROR;
    }
    
    string = Tcl_GetStringFromObj(switchObjv[0], &length);
    switchObjc--;
    switchObjv++;

    /*
     * If all of the pattern/command pairs are lumped into a single
     * argument, split them out again.
     */

    splitObjs = 0;
    if (switchObjc == 1) {
	code = Tcl_ListObjLength(interp, switchObjv[0], &switchObjc);
	if (code != TCL_OK) {
	    return code;
	}
	splitObjs = 1;
    }

    for (i = 0;  i < switchObjc;  i += 2) {
	if (i == (switchObjc-1)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "extra switch pattern with no body", -1);
	    code = TCL_ERROR;
	    goto done;
	}

	/*
	 * See if the pattern matches the string.
	 */

	if (splitObjs) {
	    code = Tcl_ListObjIndex(interp, switchObjv[0], i, &patternObj);
	    if (code != TCL_OK) {
		return code;
	    }
	    pattern = Tcl_GetStringFromObj(patternObj, &patternLen);
	} else {
	    pattern = Tcl_GetStringFromObj(switchObjv[i], &patternLen);
	}

	matched = 0;
	if ((*pattern == 'd') && (i == switchObjc-2)
		&& (strcmp(pattern, "default") == 0)) {
	    matched = 1;
	} else {
	    /*
	     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL.
	     */
	    switch (mode) {
		case EXACT:
		    matched = (strcmp(string, pattern) == 0);
		    break;
		case GLOB:
		    matched = Tcl_StringMatch(string, pattern);
		    break;
		case REGEXP:
		    matched = Tcl_RegExpMatch(interp, string, pattern);
		    if (matched < 0) {
			code = TCL_ERROR;
			goto done;
		    }
		    break;
	    }
	}
	if (!matched) {
	    continue;
	}

	/*
	 * We've got a match. Find a body to execute, skipping bodies
	 * that are "-".
	 */

	for (bodyIdx = i+1;  ;  bodyIdx += 2) {
	    if (bodyIdx >= switchObjc) {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"no body specified for pattern \"", pattern,
			"\"", (char *) NULL);
		code = TCL_ERROR;
		goto done;
	    }
	    
	    if (splitObjs) {
		code = Tcl_ListObjIndex(interp, switchObjv[0], bodyIdx,
		        &bodyObj);
		if (code != TCL_OK) {
		    return code;
		}
	    } else {
		bodyObj = switchObjv[bodyIdx];
	    }
	    /*
	     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL.
	     */
	    body = Tcl_GetStringFromObj(bodyObj, &length);
	    if ((length != 1) || (body[0] != '-')) {
		break;
	    }
	}
	code = Tcl_EvalObj(interp, bodyObj);
	if (code == TCL_ERROR) {
	    char msg[100];
	    sprintf(msg, "\n    (\"%.50s\" arm line %d)", pattern,
		    interp->errorLine);
	    Tcl_AddObjErrorInfo(interp, msg, -1);
	}
	goto done;
    }

    /*
     * Nothing matched:  return nothing.
     */

    code = TCL_OK;

    done:
    return code;
#undef EXACT
#undef GLOB
#undef REGEXP
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TimeObjCmd --
 *
 *	This object-based procedure is invoked to process the "time" Tcl
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
Tcl_TimeObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Tcl_Obj *objPtr;
    register int i, result;
    int count;
    double totalMicroSec;
    Tcl_Time start, stop;
    char buf[100];

    if (objc == 2) {
	count = 1;
    } else if (objc == 3) {
	result = Tcl_GetIntFromObj(interp, objv[2], &count);
	if (result != TCL_OK) {
	    return result;
	}
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?count?");
	return TCL_ERROR;
    }
    
    objPtr = objv[1];
    i = count;
    TclpGetTime(&start);
    while (i-- > 0) {
	result = Tcl_EvalObj(interp, objPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    TclpGetTime(&stop);
    
    totalMicroSec =
	(stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    sprintf(buf, "%.0f microseconds per iteration",
	((count <= 0) ? 0 : totalMicroSec/count));
    Tcl_ResetResult(interp);
    Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TraceCmd --
 *
 *	This procedure is invoked to process the "trace" Tcl command.
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
Tcl_TraceCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int c;
    size_t length;

    if (argc < 2) {
	Tcl_AppendResult(interp, "too few args: should be \"",
		argv[0], " option [arg arg ...]\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][1];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "variable", length) == 0)
	    && (length >= 2)) {
	char *p;
	int flags, length;
	TraceVarInfo *tvarPtr;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " variable name ops command\"", (char *) NULL);
	    return TCL_ERROR;
	}

	flags = 0;
	for (p = argv[3] ; *p != 0; p++) {
	    if (*p == 'r') {
		flags |= TCL_TRACE_READS;
	    } else if (*p == 'w') {
		flags |= TCL_TRACE_WRITES;
	    } else if (*p == 'u') {
		flags |= TCL_TRACE_UNSETS;
	    } else {
		goto badOps;
	    }
	}
	if (flags == 0) {
	    goto badOps;
	}

	length = strlen(argv[4]);
	tvarPtr = (TraceVarInfo *) ckalloc((unsigned)
		(sizeof(TraceVarInfo) - sizeof(tvarPtr->command) + length + 1));
	tvarPtr->flags = flags;
	tvarPtr->errMsg = NULL;
	tvarPtr->length = length;
	flags |= TCL_TRACE_UNSETS;
	strcpy(tvarPtr->command, argv[4]);
	if (Tcl_TraceVar(interp, argv[2], flags, TraceVarProc,
		(ClientData) tvarPtr) != TCL_OK) {
	    ckfree((char *) tvarPtr);
	    return TCL_ERROR;
	}
    } else if ((c == 'd') && (strncmp(argv[1], "vdelete", length)
	    && (length >= 2)) == 0) {
	char *p;
	int flags, length;
	TraceVarInfo *tvarPtr;
	ClientData clientData;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " vdelete name ops command\"", (char *) NULL);
	    return TCL_ERROR;
	}

	flags = 0;
	for (p = argv[3] ; *p != 0; p++) {
	    if (*p == 'r') {
		flags |= TCL_TRACE_READS;
	    } else if (*p == 'w') {
		flags |= TCL_TRACE_WRITES;
	    } else if (*p == 'u') {
		flags |= TCL_TRACE_UNSETS;
	    } else {
		goto badOps;
	    }
	}
	if (flags == 0) {
	    goto badOps;
	}

	/*
	 * Search through all of our traces on this variable to
	 * see if there's one with the given command.  If so, then
	 * delete the first one that matches.
	 */

	length = strlen(argv[4]);
	clientData = 0;
	while ((clientData = Tcl_VarTraceInfo(interp, argv[2], 0,
		TraceVarProc, clientData)) != 0) {
	    tvarPtr = (TraceVarInfo *) clientData;
	    if ((tvarPtr->length == length) && (tvarPtr->flags == flags)
		    && (strncmp(argv[4], tvarPtr->command,
		    (size_t) length) == 0)) {
		Tcl_UntraceVar(interp, argv[2], flags | TCL_TRACE_UNSETS,
			TraceVarProc, clientData);
		if (tvarPtr->errMsg != NULL) {
		    ckfree(tvarPtr->errMsg);
		}
		ckfree((char *) tvarPtr);
		break;
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "vinfo", length) == 0)
	    && (length >= 2)) {
	ClientData clientData;
	char ops[4], *p;
	char *prefix = "{";

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " vinfo name\"", (char *) NULL);
	    return TCL_ERROR;
	}
	clientData = 0;
	while ((clientData = Tcl_VarTraceInfo(interp, argv[2], 0,
		TraceVarProc, clientData)) != 0) {
	    TraceVarInfo *tvarPtr = (TraceVarInfo *) clientData;
	    p = ops;
	    if (tvarPtr->flags & TCL_TRACE_READS) {
		*p = 'r';
		p++;
	    }
	    if (tvarPtr->flags & TCL_TRACE_WRITES) {
		*p = 'w';
		p++;
	    }
	    if (tvarPtr->flags & TCL_TRACE_UNSETS) {
		*p = 'u';
		p++;
	    }
	    *p = '\0';
	    Tcl_AppendResult(interp, prefix, (char *) NULL);
	    Tcl_AppendElement(interp, ops);
	    Tcl_AppendElement(interp, tvarPtr->command);
	    Tcl_AppendResult(interp, "}", (char *) NULL);
	    prefix = " {";
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be variable, vdelete, or vinfo",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    badOps:
    Tcl_AppendResult(interp, "bad operations \"", argv[3],
	    "\": should be one or more of rwu", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TraceVarProc --
 *
 *	This procedure is called to handle variable accesses that have
 *	been traced using the "trace" command.
 *
 * Results:
 *	Normally returns NULL.  If the trace command returns an error,
 *	then this procedure returns an error string.
 *
 * Side effects:
 *	Depends on the command associated with the trace.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
TraceVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about the variable trace. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable or array. */
    char *name2;		/* Name of element within array;  NULL means
				 * scalar variable is being referenced. */
    int flags;			/* OR-ed bits giving operation and other
				 * information. */
{
    Interp *iPtr = (Interp *) interp;
    TraceVarInfo *tvarPtr = (TraceVarInfo *) clientData;
    char *result;
    int code;
    Interp dummy;
    Tcl_DString cmd;
    Tcl_Obj *saveObjPtr, *oldObjResultPtr;

    result = NULL;
    if (tvarPtr->errMsg != NULL) {
	ckfree(tvarPtr->errMsg);
	tvarPtr->errMsg = NULL;
    }
    if ((tvarPtr->flags & flags) && !(flags & TCL_INTERP_DESTROYED)) {

	/*
	 * Generate a command to execute by appending list elements
	 * for the two variable names and the operation.  The five
	 * extra characters are for three space, the opcode character,
	 * and the terminating null.
	 */

	if (name2 == NULL) {
	    name2 = "";
	}
	Tcl_DStringInit(&cmd);
	Tcl_DStringAppend(&cmd, tvarPtr->command, tvarPtr->length);
	Tcl_DStringAppendElement(&cmd, name1);
	Tcl_DStringAppendElement(&cmd, name2);
	if (flags & TCL_TRACE_READS) {
	    Tcl_DStringAppend(&cmd, " r", 2);
	} else if (flags & TCL_TRACE_WRITES) {
	    Tcl_DStringAppend(&cmd, " w", 2);
	} else if (flags & TCL_TRACE_UNSETS) {
	    Tcl_DStringAppend(&cmd, " u", 2);
	}

	/*
	 * Execute the command.  Be careful to save and restore both the
	 * string and object results from the interpreter used for
	 * the command. We discard any object result the command returns.
	 */

	dummy.objResultPtr = Tcl_NewObj();
	Tcl_IncrRefCount(dummy.objResultPtr);
	if (interp->freeProc == 0) {
	    dummy.freeProc = (Tcl_FreeProc *) 0;
	    dummy.result = "";
	    Tcl_SetResult((Tcl_Interp *) &dummy, interp->result,
		    TCL_VOLATILE);
	} else {
	    dummy.freeProc = interp->freeProc;
	    dummy.result = interp->result;
	    interp->freeProc = (Tcl_FreeProc *) 0;
	}
	
	saveObjPtr = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(saveObjPtr);
	
	code = Tcl_Eval(interp, Tcl_DStringValue(&cmd));
	if (code != TCL_OK) {	     /* copy error msg to result */
	    tvarPtr->errMsg = (char *)
		    ckalloc((unsigned) (strlen(interp->result) + 1));
	    strcpy(tvarPtr->errMsg, interp->result);
	    result = tvarPtr->errMsg;
	    Tcl_ResetResult(interp); /* must clear error state. */
	}

	/*
	 * Restore the interpreter's string result.
	 */
	
	Tcl_SetResult(interp, dummy.result,
		(dummy.freeProc == 0) ? TCL_VOLATILE : dummy.freeProc);

	/*
	 * Restore the interpreter's object result from saveObjPtr.
	 */

	oldObjResultPtr = iPtr->objResultPtr;
	iPtr->objResultPtr = saveObjPtr;  /* was incremented above */
	Tcl_DecrRefCount(oldObjResultPtr);

	Tcl_DecrRefCount(dummy.objResultPtr);
	dummy.objResultPtr = NULL;
	Tcl_DStringFree(&cmd);
    }
    if (flags & TCL_TRACE_DESTROYED) {
	result = NULL;
	if (tvarPtr->errMsg != NULL) {
	    ckfree(tvarPtr->errMsg);
	}
	ckfree((char *) tvarPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WhileCmd --
 *
 *      This procedure is invoked to process the "while" Tcl command.
 *      See the user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when
 *	a command name is computed at runtime, and is "while" or the name
 *	to which "while" was renamed: e.g., "set z while; $z {$i<100} {}"
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
Tcl_WhileCmd(dummy, interp, argc, argv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int argc;                           /* Number of arguments. */
    char **argv;                        /* Argument strings. */
{
    int result, value;

    if (argc != 3) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
                argv[0], " test command\"", (char *) NULL);
        return TCL_ERROR;
    }

    while (1) {
        result = Tcl_ExprBoolean(interp, argv[1], &value);
        if (result != TCL_OK) {
            return result;
        }
        if (!value) {
            break;
        }
        result = Tcl_Eval(interp, argv[2]);
        if ((result != TCL_OK) && (result != TCL_CONTINUE)) {
            if (result == TCL_ERROR) {
                char msg[60];
                sprintf(msg, "\n    (\"while\" body line %d)",
                        interp->errorLine);
                Tcl_AddErrorInfo(interp, msg);
            }
            break;
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

