/* 
 * tclLoad.c --
 *
 *	This file provides the generic portion (those that are the same
 *	on all platforms) of Tcl's dynamic loading facilities.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoad.c 1.16 97/05/14 13:23:37
 */

#include "tclInt.h"

/*
 * The following structure describes a package that has been loaded
 * either dynamically (with the "load" command) or statically (as
 * indicated by a call to Tcl_PackageLoaded).  All such packages
 * are linked together into a single list for the process.  Packages
 * are never unloaded, so these structures are never freed.
 */

typedef struct LoadedPackage {
    char *fileName;		/* Name of the file from which the
				 * package was loaded.  An empty string
				 * means the package is loaded statically.
				 * Malloc-ed. */
    char *packageName;		/* Name of package prefix for the package,
				 * properly capitalized (first letter UC,
				 * others LC), no "_", as in "Net". 
				 * Malloc-ed. */
    Tcl_PackageInitProc *initProc;
				/* Initialization procedure to call to
				 * incorporate this package into a trusted
				 * interpreter. */
    Tcl_PackageInitProc *safeInitProc;
				/* Initialization procedure to call to
				 * incorporate this package into a safe
				 * interpreter (one that will execute
				 * untrusted scripts).   NULL means the
				 * package can't be used in unsafe
				 * interpreters. */
    struct LoadedPackage *nextPtr;
				/* Next in list of all packages loaded into
				 * this application process.  NULL means
				 * end of list. */
} LoadedPackage;

static LoadedPackage *firstPackagePtr = NULL;
				/* First in list of all packages loaded into
				 * this process. */

/*
 * The following structure represents a particular package that has
 * been incorporated into a particular interpreter (by calling its
 * initialization procedure).  There is a list of these structures for
 * each interpreter, with an AssocData value (key "load") for the
 * interpreter that points to the first package (if any).
 */

typedef struct InterpPackage {
    LoadedPackage *pkgPtr;	/* Points to detailed information about
				 * package. */
    struct InterpPackage *nextPtr;
				/* Next package in this interpreter, or
				 * NULL for end of list. */
} InterpPackage;

/*
 * Prototypes for procedures that are private to this file:
 */

static void		LoadCleanupProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static void		LoadExitProc _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LoadCmd --
 *
 *	This procedure is invoked to process the "load" Tcl command.
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
Tcl_LoadCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Interp *target;
    LoadedPackage *pkgPtr, *defaultPtr;
    Tcl_DString pkgName, initName, safeInitName, fileName;
    Tcl_PackageInitProc *initProc, *safeInitProc;
    InterpPackage *ipFirstPtr, *ipPtr;
    int code, c, gotPkgName, namesMatch, filesMatch;
    char *p, *fullFileName, *p1, *p2;

    if ((argc < 2) || (argc > 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" fileName ?packageName? ?interp?\"", (char *) NULL);
	return TCL_ERROR;
    }
    fullFileName = Tcl_TranslateFileName(interp, argv[1], &fileName);
    if (fullFileName == NULL) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&pkgName);
    Tcl_DStringInit(&initName);
    Tcl_DStringInit(&safeInitName);
    if ((argc >= 3) && (argv[2][0] != 0)) {
	gotPkgName = 1;
    } else {
	gotPkgName = 0;
    }
    if ((fullFileName[0] == 0) && !gotPkgName) {
	Tcl_SetResult(interp,
		"must specify either file name or package name",
		TCL_STATIC);
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Figure out which interpreter we're going to load the package into.
     */

    target = interp;
    if (argc == 4) {
	target = Tcl_GetSlave(interp, argv[3]);
	if (target == NULL) {
	    Tcl_AppendResult(interp, "couldn't find slave interpreter named \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * Scan through the packages that are currently loaded to see if the
     * package we want is already loaded.  We'll use a loaded package if
     * it meets any of the following conditions:
     *  - Its name and file match the once we're looking for.
     *  - Its file matches, and we weren't given a name.
     *  - Its name matches, the file name was specified as empty, and there
     *    is only no statically loaded package with the same name.
     */

    defaultPtr = NULL;
    for (pkgPtr = firstPackagePtr; pkgPtr != NULL; pkgPtr = pkgPtr->nextPtr) {
	if (!gotPkgName) {
	    namesMatch = 0;
	} else {
	    namesMatch = 1;
	    for (p1 = argv[2], p2 = pkgPtr->packageName; ; p1++, p2++) {
		if ((isupper(UCHAR(*p1)) ? tolower(UCHAR(*p1)) : *p1)
			!= (isupper(UCHAR(*p2)) ? tolower(UCHAR(*p2)) : *p2)) {
		    namesMatch = 0;
		    break;
		}
		if (*p1 == 0) {
		    break;
		}
	    }
	}
	filesMatch = (strcmp(pkgPtr->fileName, fullFileName) == 0);
	if (filesMatch && (namesMatch || !gotPkgName)) {
	    break;
	}
	if (namesMatch && (fullFileName[0] == 0)) {
	    defaultPtr = pkgPtr;
	}
	if (filesMatch && !namesMatch && (fullFileName[0] != 0)) {
	    /*
	     * Can't have two different packages loaded from the same
	     * file.
	     */

	    Tcl_AppendResult(interp, "file \"", fullFileName,
		    "\" is already loaded for package \"",
		    pkgPtr->packageName, "\"", (char *) NULL);
	    code = TCL_ERROR;
	    goto done;
	}
    }
    if (pkgPtr == NULL) {
	pkgPtr = defaultPtr;
    }

    /*
     * Scan through the list of packages already loaded in the target
     * interpreter.  If the package we want is already loaded there,
     * then there's nothing for us to to.
     */

    if (pkgPtr != NULL) {
	ipFirstPtr = (InterpPackage *) Tcl_GetAssocData(target, "tclLoad",
		(Tcl_InterpDeleteProc **) NULL);
	for (ipPtr = ipFirstPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->pkgPtr == pkgPtr) {
		code = TCL_OK;
		goto done;
	    }
	}
    }

    if (pkgPtr == NULL) {
	/*
	 * The desired file isn't currently loaded, so load it.  It's an
	 * error if the desired package is a static one.
	 */

	if (fullFileName[0] == 0) {
	    Tcl_AppendResult(interp, "package \"", argv[2],
		    "\" isn't loaded statically", (char *) NULL);
	    code = TCL_ERROR;
	    goto done;
	}

	/*
	 * Figure out the module name if it wasn't provided explicitly.
	 */

	if (gotPkgName) {
	    Tcl_DStringAppend(&pkgName, argv[2], -1);
	} else {
	    if (!TclGuessPackageName(fullFileName, &pkgName)) {
		int pargc;
		char **pargv, *pkgGuess;

		/*
		 * The platform-specific code couldn't figure out the
		 * module name.  Make a guess by taking the last element
		 * of the file name, stripping off any leading "lib",
		 * and then using all of the alphabetic and underline
		 * characters that follow that.
		 */

		Tcl_SplitPath(fullFileName, &pargc, &pargv);
		pkgGuess = pargv[pargc-1];
		if ((pkgGuess[0] == 'l') && (pkgGuess[1] == 'i')
			&& (pkgGuess[2] == 'b')) {
		    pkgGuess += 3;
		}
		for (p = pkgGuess; isalpha(UCHAR(*p)) || (*p == '_'); p++) {
		    /* Empty loop body. */
		}
		if (p == pkgGuess) {
		    ckfree((char *)pargv);
		    Tcl_AppendResult(interp,
			    "couldn't figure out package name for ",
			    fullFileName, (char *) NULL);
		    code = TCL_ERROR;
		    goto done;
		}
		Tcl_DStringAppend(&pkgName, pkgGuess, (p - pkgGuess));
		ckfree((char *)pargv);
	    }
	}

	/*
	 * Fix the capitalization in the package name so that the first
	 * character is in caps but the others are all lower-case.
	 */
    
	p = Tcl_DStringValue(&pkgName);
	c = UCHAR(*p);
	if (c != 0) {
	    if (islower(c)) {
		*p = (char) toupper(c);
	    }
	    p++;
	    while (1) {
		c = UCHAR(*p);
		if (c == 0) {
		    break;
		}
		if (isupper(c)) {
		    *p = (char) tolower(c);
		}
		p++;
	    }
	}

	/*
	 * Compute the names of the two initialization procedures,
	 * based on the package name.
	 */
    
	Tcl_DStringAppend(&initName, Tcl_DStringValue(&pkgName), -1);
	Tcl_DStringAppend(&initName, "_Init", 5);
	Tcl_DStringAppend(&safeInitName, Tcl_DStringValue(&pkgName), -1);
	Tcl_DStringAppend(&safeInitName, "_SafeInit", 9);
    
	/*
	 * Call platform-specific code to load the package and find the
	 * two initialization procedures.
	 */
    
	code = TclLoadFile(interp, fullFileName, Tcl_DStringValue(&initName),
		Tcl_DStringValue(&safeInitName), &initProc, &safeInitProc);
	if (code != TCL_OK) {
	    goto done;
	}
	if (initProc  == NULL) {
	    Tcl_AppendResult(interp, "couldn't find procedure ",
		    Tcl_DStringValue(&initName), (char *) NULL);
	    code = TCL_ERROR;
	    goto done;
	}

	/*
	 * Create a new record to describe this package.
	 */

	if (firstPackagePtr == NULL) {
	    Tcl_CreateExitHandler(LoadExitProc, (ClientData) NULL);
	}
	pkgPtr = (LoadedPackage *) ckalloc(sizeof(LoadedPackage));
	pkgPtr->fileName = (char *) ckalloc((unsigned)
		(strlen(fullFileName) + 1));
	strcpy(pkgPtr->fileName, fullFileName);
	pkgPtr->packageName = (char *) ckalloc((unsigned)
		(Tcl_DStringLength(&pkgName) + 1));
	strcpy(pkgPtr->packageName, Tcl_DStringValue(&pkgName));
	pkgPtr->initProc = initProc;
	pkgPtr->safeInitProc = safeInitProc;
	pkgPtr->nextPtr = firstPackagePtr;
	firstPackagePtr = pkgPtr;
    }

    /*
     * Invoke the package's initialization procedure (either the
     * normal one or the safe one, depending on whether or not the
     * interpreter is safe).
     */

    if (Tcl_IsSafe(target)) {
	if (pkgPtr->safeInitProc != NULL) {
	    code = (*pkgPtr->safeInitProc)(target);
	} else {
	    Tcl_AppendResult(interp,
		    "can't use package in a safe interpreter: ",
		    "no ", pkgPtr->packageName, "_SafeInit procedure",
		    (char *) NULL);
	    code = TCL_ERROR;
	    goto done;
	}
    } else {
	code = (*pkgPtr->initProc)(target);
    }
    if ((code == TCL_ERROR) && (target != interp)) {
	/*
	 * An error occurred, so transfer error information from the
	 * destination interpreter back to our interpreter.  Must clear
	 * interp's result before calling Tcl_AddErrorInfo, since
	 * Tcl_AddErrorInfo will store the interp's result in errorInfo
	 * before appending target's $errorInfo;  we've already got
	 * everything we need in target's $errorInfo.
	 */

	Tcl_ResetResult(interp);
	Tcl_AddErrorInfo(interp, Tcl_GetVar2(target,
		"errorInfo", (char *) NULL, TCL_GLOBAL_ONLY));
	Tcl_SetVar2(interp, "errorCode", (char *) NULL,
		Tcl_GetVar2(target, "errorCode", (char *) NULL,
		TCL_GLOBAL_ONLY), TCL_GLOBAL_ONLY);
	Tcl_SetResult(interp, target->result, TCL_VOLATILE);
    }

    /*
     * Record the fact that the package has been loaded in the
     * target interpreter.
     */

    if (code == TCL_OK) {
	/*
	 * Refetch ipFirstPtr: loading the package may have introduced
	 * additional static packages at the head of the linked list!
	 */

	ipFirstPtr = (InterpPackage *) Tcl_GetAssocData(target, "tclLoad",
		(Tcl_InterpDeleteProc **) NULL);
	ipPtr = (InterpPackage *) ckalloc(sizeof(InterpPackage));
	ipPtr->pkgPtr = pkgPtr;
	ipPtr->nextPtr = ipFirstPtr;
	Tcl_SetAssocData(target, "tclLoad", LoadCleanupProc,
		(ClientData) ipPtr);
    }

    done:
    Tcl_DStringFree(&pkgName);
    Tcl_DStringFree(&initName);
    Tcl_DStringFree(&safeInitName);
    Tcl_DStringFree(&fileName);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_StaticPackage --
 *
 *	This procedure is invoked to indicate that a particular
 *	package has been linked statically with an application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Once this procedure completes, the package becomes loadable
 *	via the "load" command with an empty file name.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_StaticPackage(interp, pkgName, initProc, safeInitProc)
    Tcl_Interp *interp;			/* If not NULL, it means that the
					 * package has already been loaded
					 * into the given interpreter by
					 * calling the appropriate init proc. */
    char *pkgName;			/* Name of package (must be properly
					 * capitalized: first letter upper
					 * case, others lower case). */
    Tcl_PackageInitProc *initProc;	/* Procedure to call to incorporate
					 * this package into a trusted
					 * interpreter. */
    Tcl_PackageInitProc *safeInitProc;	/* Procedure to call to incorporate
					 * this package into a safe interpreter
					 * (one that will execute untrusted
					 * scripts).   NULL means the package
					 * can't be used in safe
					 * interpreters. */
{
    LoadedPackage *pkgPtr;
    InterpPackage *ipPtr, *ipFirstPtr;

    /*
     * Check to see if someone else has already reported this package as
     * statically loaded.  If this call is redundant then just return.
     */

    for (pkgPtr = firstPackagePtr; pkgPtr != NULL; pkgPtr = pkgPtr->nextPtr) {
	if ((pkgPtr->initProc == initProc)
		&& (pkgPtr->safeInitProc == safeInitProc)
		&& (strcmp(pkgPtr->packageName, pkgName) == 0)) {
	    return;
	}
    }

    if (firstPackagePtr == NULL) {
	Tcl_CreateExitHandler(LoadExitProc, (ClientData) NULL);
    }
    pkgPtr = (LoadedPackage *) ckalloc(sizeof(LoadedPackage));
    pkgPtr->fileName = (char *) ckalloc((unsigned) 1);
    pkgPtr->fileName[0] = 0;
    pkgPtr->packageName = (char *) ckalloc((unsigned)
	    (strlen(pkgName) + 1));
    strcpy(pkgPtr->packageName, pkgName);
    pkgPtr->initProc = initProc;
    pkgPtr->safeInitProc = safeInitProc;
    pkgPtr->nextPtr = firstPackagePtr;
    firstPackagePtr = pkgPtr;

    if (interp != NULL) {
	ipFirstPtr = (InterpPackage *) Tcl_GetAssocData(interp, "tclLoad",
		(Tcl_InterpDeleteProc **) NULL);
	ipPtr = (InterpPackage *) ckalloc(sizeof(InterpPackage));
	ipPtr->pkgPtr = pkgPtr;
	ipPtr->nextPtr = ipFirstPtr;
	Tcl_SetAssocData(interp, "tclLoad", LoadCleanupProc,
		(ClientData) ipPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetLoadedPackages --
 *
 *	This procedure returns information about all of the files
 *	that are loaded (either in a particular intepreter, or
 *	for all interpreters).
 *
 * Results:
 *	The return value is a standard Tcl completion code.  If
 *	successful, a list of lists is placed in interp->result.
 *	Each sublist corresponds to one loaded file;  its first
 *	element is the name of the file (or an empty string for
 *	something that's statically loaded) and the second element
 *	is the name of the package in that file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetLoadedPackages(interp, targetName)
    Tcl_Interp *interp;		/* Interpreter in which to return
				 * information or error message. */
    char *targetName;		/* Name of target interpreter or NULL.
				 * If NULL, return info about all interps;
				 * otherwise, just return info about this
				 * interpreter. */
{
    Tcl_Interp *target;
    LoadedPackage *pkgPtr;
    InterpPackage *ipPtr;
    char *prefix;

    if (targetName == NULL) {
	/* 
	 * Return information about all of the available packages.
	 */

	prefix = "{";
	for (pkgPtr = firstPackagePtr; pkgPtr != NULL;
		pkgPtr = pkgPtr->nextPtr) {
	    Tcl_AppendResult(interp, prefix, (char *) NULL);
	    Tcl_AppendElement(interp, pkgPtr->fileName);
	    Tcl_AppendElement(interp, pkgPtr->packageName);
	    Tcl_AppendResult(interp, "}", (char *) NULL);
	    prefix = " {";
	}
	return TCL_OK;
    }

    /*
     * Return information about only the packages that are loaded in
     * a given interpreter.
     */

    target = Tcl_GetSlave(interp, targetName);
    if (target == NULL) {
	Tcl_AppendResult(interp, "couldn't find slave interpreter named \"",
		targetName, "\"", (char *) NULL);
	return TCL_ERROR;
    }
    ipPtr = (InterpPackage *) Tcl_GetAssocData(target, "tclLoad",
	    (Tcl_InterpDeleteProc **) NULL);
    prefix = "{";
    for ( ; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	pkgPtr = ipPtr->pkgPtr;
	Tcl_AppendResult(interp, prefix, (char *) NULL);
	Tcl_AppendElement(interp, pkgPtr->fileName);
	Tcl_AppendElement(interp, pkgPtr->packageName);
	Tcl_AppendResult(interp, "}", (char *) NULL);
	prefix = " {";
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LoadCleanupProc --
 *
 *	This procedure is called to delete all of the InterpPackage
 *	structures for an interpreter when the interpreter is deleted.
 *	It gets invoked via the Tcl AssocData mechanism.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for all of the InterpPackage procedures for interp
 *	get deleted.
 *
 *----------------------------------------------------------------------
 */

static void
LoadCleanupProc(clientData, interp)
    ClientData clientData;	/* Pointer to first InterpPackage structure
				 * for interp. */
    Tcl_Interp *interp;		/* Interpreter that is being deleted. */
{
    InterpPackage *ipPtr, *nextPtr;

    ipPtr = (InterpPackage *) clientData;
    while (ipPtr != NULL) {
	nextPtr = ipPtr->nextPtr;
	ckfree((char *) ipPtr);
	ipPtr = nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * LoadExitProc --
 *
 *	This procedure is invoked just before the application exits.
 *	It frees all of the LoadedPackage structures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

static void
LoadExitProc(clientData)
    ClientData clientData;		/* Not used. */
{
    LoadedPackage *pkgPtr;

    while (firstPackagePtr != NULL) {
	pkgPtr = firstPackagePtr;
	firstPackagePtr = pkgPtr->nextPtr;
	ckfree(pkgPtr->fileName);
	ckfree(pkgPtr->packageName);
	ckfree((char *) pkgPtr);
    }
}
