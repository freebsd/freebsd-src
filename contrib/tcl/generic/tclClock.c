/* 
 * tclClock.c --
 *
 *	Contains the time and date related commands.  This code
 *	is derived from the time and date facilities of TclX,
 *	by Mark Diekhans and Karl Lehenbauer.
 *
 * Copyright 1991-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclClock.c 1.20 96/07/23 16:14:45
 */

#include "tcl.h"
#include "tclInt.h"
#include "tclPort.h"

/*
 * Function prototypes for local procedures in this file:
 */

static int		FormatClock _ANSI_ARGS_((Tcl_Interp *interp,
			    unsigned long clockVal, int useGMT,
			    char *format));
static int		ParseTime _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, unsigned long *timePtr));

/*
 *-----------------------------------------------------------------------------
 *
 * Tcl_ClockCmd --
 *
 *	This procedure is invoked to process the "clock" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *-----------------------------------------------------------------------------
 */

int
Tcl_ClockCmd (dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int c;
    size_t length;
    char **argPtr;
    int useGMT = 0;
    unsigned long clockVal;
    
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "clicks", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " clicks\"", (char *) NULL);
	    return TCL_ERROR;
	}
	sprintf(interp->result, "%lu", TclpGetClicks());
	return TCL_OK;
    } else if ((c == 'f') && (strncmp(argv[1], "format", length) == 0)) {
	char *format = "%a %b %d %X %Z %Y";
	
	if ((argc < 3) || (argc > 7)) {
	    wrongFmtArgs:
	    Tcl_AppendResult(interp, "wrong # args: ", argv [0], 
		    " format clockval ?-format string? ?-gmt boolean?",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	if (ParseTime(interp, argv[2], &clockVal) != TCL_OK) {
	    return TCL_ERROR;
	}

	argPtr = argv+3;
	argc -= 3;
	while ((argc > 1) && (argPtr[0][0] == '-')) {
	    if (strcmp(argPtr[0], "-format") == 0) {
	        format = argPtr[1];
	    } else if (strcmp(argPtr[0], "-gmt") == 0) {
		if (Tcl_GetBoolean(interp, argPtr[1], &useGMT) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		Tcl_AppendResult(interp, "bad option \"", argPtr[0],
			"\": must be -format or -gmt", (char *) NULL);
		return TCL_ERROR;
	    }
	    argPtr += 2;
	    argc -= 2;
	}
	if (argc != 0) {
	    goto wrongFmtArgs;
	}

	return FormatClock(interp, clockVal, useGMT, format);
    } else if ((c == 's') && (strncmp(argv[1], "scan", length) == 0)) {
	unsigned long baseClock;
	long zone;
	char * baseStr = NULL;

	if ((argc < 3) || (argc > 7)) {
	    wrongScanArgs:
	    Tcl_AppendResult (interp, "wrong # args: ", argv [0], 
		    " scan dateString ?-base clockValue? ?-gmt boolean?",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	argPtr = argv+3;
	argc -= 3;
	while ((argc > 1) && (argPtr[0][0] == '-')) {
	    if (strcmp(argPtr[0], "-base") == 0) {
	        baseStr = argPtr[1];
	    } else if (strcmp(argPtr[0], "-gmt") == 0) {
		if (Tcl_GetBoolean(interp, argPtr[1], &useGMT) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		Tcl_AppendResult(interp, "bad option \"", argPtr[0],
			"\": must be -base or -gmt", (char *) NULL);
		return TCL_ERROR;
	    }
	    argPtr += 2;
	    argc -= 2;
	}
	if (argc != 0) {
	    goto wrongScanArgs;
	}
	
	if (baseStr != NULL) {
	    if (ParseTime(interp, baseStr, &baseClock) != TCL_OK)
		return TCL_ERROR;
	} else {
	    baseClock = TclpGetSeconds();
	}

	if (useGMT) {
	    zone = -50000; /* Force GMT */
	} else {
	    zone = TclpGetTimeZone(baseClock);
	}

	if (TclGetDate(argv[2], baseClock, zone, &clockVal) < 0) {
	    Tcl_AppendResult(interp, "unable to convert date-time string \"",
		    argv[2], "\"", (char *) NULL);
	    return TCL_ERROR;
	}

	sprintf(interp->result, "%lu", (long) clockVal);
	return TCL_OK;
    } else if ((c == 's') && (strncmp(argv[1], "seconds", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " seconds\"", (char *) NULL);
	    return TCL_ERROR;
	}
	sprintf(interp->result, "%lu", TclpGetSeconds());
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "unknown option \"", argv[1],
		"\": must be clicks, format, scan, or seconds",
		(char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *-----------------------------------------------------------------------------
 *
 * ParseTime --
 *
 *      Given a string, produce the corresponding time_t value.
 *
 * Results:
 *      The return value is normally TCL_OK;  in this case *timePtr
 *      will be set to the integer value equivalent to string.  If
 *      string is improperly formed then TCL_ERROR is returned and
 *      an error message will be left in interp->result.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
ParseTime(interp, string, timePtr)
    Tcl_Interp *interp;
    char *string;
    unsigned long *timePtr;
{
    char *end, *p;
    unsigned long  i;

    /*
     * Since some strtoul functions don't detect negative numbers, check
     * in advance.
     */
    errno = 0;
    for (p = (char *) string; isspace(UCHAR(*p)); p++) {
        /* Empty loop body. */
    }
    if (*p == '+') {
        p++;
    }
    i = strtoul(p, &end, 0);
    if (end == p) {
        goto badTime;
    }
    if (errno == ERANGE) {
	interp->result = "integer value too large to represent";
	Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
		interp->result, (char *) NULL);
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
        end++;
    }
    if (*end != '\0') {
        goto badTime;
    }

    *timePtr = (time_t) i;
    if (*timePtr != i) {
        goto badTime;
    }
    return TCL_OK;

  badTime:
    Tcl_AppendResult (interp, "expected unsigned time but got \"", 
                      string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *-----------------------------------------------------------------------------
 *
 * FormatClock --
 *
 *      Formats a time value based on seconds into a human readable
 *	string.
 *
 * Results:
 *      Standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FormatClock(interp, clockVal, useGMT, format)
    Tcl_Interp *interp;			/* Current interpreter. */
    unsigned long clockVal;	       	/* Time in seconds. */
    int useGMT;				/* Boolean */
    char *format;			/* Format string */
{
    struct tm *timeDataPtr;
    Tcl_DString buffer;
    int bufSize;
    char *p;
#ifdef TCL_USE_TIMEZONE_VAR
    int savedTimeZone;
    char *savedTZEnv;
#endif

#ifdef HAVE_TZSET
    /*
     * Some systems forgot to call tzset in localtime, make sure its done.
     */
    static int  calledTzset = 0;

    if (!calledTzset) {
        tzset();
        calledTzset = 1;
    }
#endif

#ifdef TCL_USE_TIMEZONE_VAR
    /*
     * This is a horrible kludge for systems not having the timezone in
     * struct tm.  No matter what was specified, they use the global time
     * zone.  (Thanks Solaris).
     */
    if (useGMT) {
        char *varValue;

        varValue = Tcl_GetVar2(interp, "env", "TZ", TCL_GLOBAL_ONLY);
        if (varValue != NULL) {
	    savedTZEnv = strcpy(ckalloc(strlen(varValue) + 1), varValue);
        } else {
            savedTZEnv = NULL;
	}
        Tcl_SetVar2(interp, "env", "TZ", "GMT", TCL_GLOBAL_ONLY);
        savedTimeZone = timezone;
        timezone = 0;
        tzset();
    }
#endif

    timeDataPtr = TclpGetDate((time_t *) &clockVal, useGMT);
    
    /*
     * Make a guess at the upper limit on the substituted string size
     * based on the number of percents in the string.
     */

    for (bufSize = 0, p = format; *p != '\0'; p++) {
	if (*p == '%') {
	    bufSize += 40;
	} else {
	    bufSize++;
	}
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringSetLength(&buffer, bufSize);

    if (TclStrftime(buffer.string, (unsigned int) bufSize, format,
	    timeDataPtr) == 0) {
	Tcl_DStringFree(&buffer);
	Tcl_AppendResult(interp, "bad format string", (char *)NULL);
	return TCL_ERROR;
    }

#ifdef TCL_USE_TIMEZONE_VAR
    if (useGMT) {
        if (savedTZEnv != NULL) {
            Tcl_SetVar2(interp, "env", "TZ", savedTZEnv, TCL_GLOBAL_ONLY);
            ckfree(savedTZEnv);
        } else {
            Tcl_UnsetVar2(interp, "env", "TZ", TCL_GLOBAL_ONLY);
        }
        timezone = savedTimeZone;
        tzset();
    }
#endif

    Tcl_DStringResult(interp, &buffer);
    return TCL_OK;
}

