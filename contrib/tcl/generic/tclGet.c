/* 
 * tclGet.c --
 *
 *	This file contains procedures to convert strings into
 *	other forms, like integers or floating-point numbers or
 *	booleans, doing syntax checking along the way.
 *
 * Copyright (c) 1990-1993 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclGet.c 1.24 96/02/15 11:42:47
 */

#include "tclInt.h"
#include "tclPort.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetInt --
 *
 *	Given a string, produce the corresponding integer value.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *intPtr
 *	will be set to the integer value equivalent to string.  If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetInt(interp, string, intPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    char *string;		/* String containing a (possibly signed)
				 * integer in a form acceptable to strtol. */
    int *intPtr;		/* Place to store converted result. */
{
    char *end, *p;
    int i;

    /*
     * Note: use strtoul instead of strtol for integer conversions
     * to allow full-size unsigned numbers, but don't depend on strtoul
     * to handle sign characters;  it won't in some implementations.
     */

    errno = 0;
    for (p = string; isspace(UCHAR(*p)); p++) {
	/* Empty loop body. */
    }
    if (*p == '-') {
	p++;
	i = -(int)strtoul(p, &end, 0);
    } else if (*p == '+') {
	p++;
	i = strtoul(p, &end, 0);
    } else {
	i = strtoul(p, &end, 0);
    }
    if (end == p) {
	badInteger:
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "expected integer but got \"", string,
                    "\"", (char *) NULL);
        }
	return TCL_ERROR;
    }
    if (errno == ERANGE) {
        if (interp != (Tcl_Interp *) NULL) {
            interp->result = "integer value too large to represent";
            Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
                    interp->result, (char *) NULL);
        }
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto badInteger;
    }
    *intPtr = i;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetDouble --
 *
 *	Given a string, produce the corresponding double-precision
 *	floating-point value.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *doublePtr
 *	will be set to the double-precision value equivalent to string.
 *	If string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetDouble(interp, string, doublePtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    char *string;		/* String containing a floating-point number
				 * in a form acceptable to strtod. */
    double *doublePtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    errno = 0;
    d = strtod(string, &end);
    if (end == string) {
	badDouble:
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp,
                    "expected floating-point number but got \"",
                    string, "\"", (char *) NULL);
        }
	return TCL_ERROR;
    }
    if (errno != 0) {
        if (interp != (Tcl_Interp *) NULL) {
            TclExprFloatError(interp, d);
        }
	return TCL_ERROR;
    }
    while ((*end != 0) && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto badDouble;
    }
    *doublePtr = d;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetBoolean --
 *
 *	Given a string, return a 0/1 boolean value corresponding
 *	to the string.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *boolPtr
 *	will be set to the 0/1 value equivalent to string.  If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetBoolean(interp, string, boolPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    char *string;		/* String containing a boolean number
				 * specified either as 1/0 or true/false or
				 * yes/no. */
    int *boolPtr;		/* Place to store converted result, which
				 * will be 0 or 1. */
{
    int i;
    char lowerCase[10], c;
    size_t length;

    /*
     * Convert the input string to all lower-case.
     */

    for (i = 0; i < 9; i++) {
	c = string[i];
	if (c == 0) {
	    break;
	}
	if ((c >= 'A') && (c <= 'Z')) {
	    c += (char) ('a' - 'A');
	}
	lowerCase[i] = c;
    }
    lowerCase[i] = 0;

    length = strlen(lowerCase);
    c = lowerCase[0];
    if ((c == '0') && (lowerCase[1] == '\0')) {
	*boolPtr = 0;
    } else if ((c == '1') && (lowerCase[1] == '\0')) {
	*boolPtr = 1;
    } else if ((c == 'y') && (strncmp(lowerCase, "yes", length) == 0)) {
	*boolPtr = 1;
    } else if ((c == 'n') && (strncmp(lowerCase, "no", length) == 0)) {
	*boolPtr = 0;
    } else if ((c == 't') && (strncmp(lowerCase, "true", length) == 0)) {
	*boolPtr = 1;
    } else if ((c == 'f') && (strncmp(lowerCase, "false", length) == 0)) {
	*boolPtr = 0;
    } else if ((c == 'o') && (length >= 2)) {
	if (strncmp(lowerCase, "on", length) == 0) {
	    *boolPtr = 1;
	} else if (strncmp(lowerCase, "off", length) == 0) {
	    *boolPtr = 0;
	} else {
	    goto badBoolean;
	}
    } else {
	badBoolean:
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "expected boolean value but got \"",
                    string, "\"", (char *) NULL);
        }
	return TCL_ERROR;
    }
    return TCL_OK;
}
