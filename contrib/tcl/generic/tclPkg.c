/* 
 * tclPkg.c --
 *
 *	This file implements package and version control for Tcl via
 *	the "package" command and a few C APIs.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclPkg.c 1.9 97/05/14 13:23:51
 */

#include "tclInt.h"

/*
 * Each invocation of the "package ifneeded" command creates a structure
 * of the following type, which is used to load the package into the
 * interpreter if it is requested with a "package require" command.
 */

typedef struct PkgAvail {
    char *version;		/* Version string; malloc'ed. */
    char *script;		/* Script to invoke to provide this version
				 * of the package.  Malloc'ed and protected
				 * by Tcl_Preserve and Tcl_Release. */
    struct PkgAvail *nextPtr;	/* Next in list of available versions of
				 * the same package. */
} PkgAvail;

/*
 * For each package that is known in any way to an interpreter, there
 * is one record of the following type.  These records are stored in
 * the "packageTable" hash table in the interpreter, keyed by
 * package name such as "Tk" (no version number).
 */

typedef struct Package {
    char *version;		/* Version that has been supplied in this
				 * interpreter via "package provide"
				 * (malloc'ed).  NULL means the package doesn't
				 * exist in this interpreter yet. */
    PkgAvail *availPtr;		/* First in list of all available versions
				 * of this package. */
} Package;

/*
 * Prototypes for procedures defined in this file:
 */

static int		CheckVersion _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string));
static int		ComparePkgVersions _ANSI_ARGS_((char *v1, char *v2,
			    int *satPtr));
static Package *	FindPackage _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PkgProvide --
 *
 *	This procedure is invoked to declare that a particular version
 *	of a particular package is now present in an interpreter.  There
 *	must not be any other version of this package already
 *	provided in the interpreter.
 *
 * Results:
 *	Normally returns TCL_OK;  if there is already another version
 *	of the package loaded then TCL_ERROR is returned and an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	The interpreter remembers that this package is available,
 *	so that no other version of the package may be provided for
 *	the interpreter.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_PkgProvide(interp, name, version)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    char *name;			/* Name of package. */
    char *version;		/* Version string for package. */
{
    Package *pkgPtr;

    pkgPtr = FindPackage(interp, name);
    if (pkgPtr->version == NULL) {
	pkgPtr->version = ckalloc((unsigned) (strlen(version) + 1));
	strcpy(pkgPtr->version, version);
	return TCL_OK;
    }
    if (ComparePkgVersions(pkgPtr->version, version, (int *) NULL) == 0) {
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "conflicting versions provided for package \"",
	    name, "\": ", pkgPtr->version, ", then ", version, (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PkgRequire --
 *
 *	This procedure is called by code that depends on a particular
 *	version of a particular package.  If the package is not already
 *	provided in the interpreter, this procedure invokes a Tcl script
 *	to provide it.  If the package is already provided, this
 *	procedure makes sure that the caller's needs don't conflict with
 *	the version that is present.
 *
 * Results:
 *	If successful, returns the version string for the currently
 *	provided version of the package, which may be different from
 *	the "version" argument.  If the caller's requirements
 *	cannot be met (e.g. the version requested conflicts with
 *	a currently provided version, or the required version cannot
 *	be found, or the script to provide the required version
 *	generates an error), NULL is returned and an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	The script from some previous "package ifneeded" command may
 *	be invoked to provide the package.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_PkgRequire(interp, name, version, exact)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    char *name;			/* Name of desired package. */
    char *version;		/* Version string for desired version;
				 * NULL means use the latest version
				 * available. */
    int exact;			/* Non-zero means that only the particular
				 * version given is acceptable. Zero means
				 * use the latest compatible version. */
{
    Package *pkgPtr;
    PkgAvail *availPtr, *bestPtr;
    char *script;
    int code, satisfies, result, pass;
    Tcl_DString command;

    /*
     * It can take up to three passes to find the package:  one pass to
     * run the "package unknown" script, one to run the "package ifneeded"
     * script for a specific version, and a final pass to lookup the
     * package loaded by the "package ifneeded" script.
     */

    for (pass = 1; ; pass++) {
	pkgPtr = FindPackage(interp, name);
	if (pkgPtr->version != NULL) {
	    break;
	}

	/*
	 * The package isn't yet present.  Search the list of available
	 * versions and invoke the script for the best available version.
	 */
    
	bestPtr = NULL;
	for (availPtr = pkgPtr->availPtr; availPtr != NULL;
		availPtr = availPtr->nextPtr) {
	    if ((bestPtr != NULL) && (ComparePkgVersions(availPtr->version,
		    bestPtr->version, (int *) NULL) <= 0)) {
		continue;
	    }
	    if (version != NULL) {
		result = ComparePkgVersions(availPtr->version, version,
			&satisfies);
		if ((result != 0) && exact) {
		    continue;
		}
		if (!satisfies) {
		    continue;
		}
	    }
	    bestPtr = availPtr;
	}
	if (bestPtr != NULL) {
	    /*
	     * We found an ifneeded script for the package.  Be careful while
	     * executing it:  this could cause reentrancy, so (a) protect the
	     * script itself from deletion and (b) don't assume that bestPtr
	     * will still exist when the script completes.
	     */
	
	    script = bestPtr->script;
	    Tcl_Preserve((ClientData) script);
	    code = Tcl_GlobalEval(interp, script);
	    Tcl_Release((ClientData) script);
	    if (code != TCL_OK) {
		if (code == TCL_ERROR) {
		    Tcl_AddErrorInfo(interp,
			    "\n    (\"package ifneeded\" script)");
		}
		return NULL;
	    }
	    Tcl_ResetResult(interp);
	    pkgPtr = FindPackage(interp, name);
	    break;
	}

	/*
	 * Package not in the database.  If there is a "package unknown"
	 * command, invoke it (but only on the first pass;  after that,
	 * we should not get here in the first place).
	 */

	if (pass > 1) {
	    break;
	}
	script = ((Interp *) interp)->packageUnknown;
	if (script != NULL) {
	    Tcl_DStringInit(&command);
	    Tcl_DStringAppend(&command, script, -1);
	    Tcl_DStringAppendElement(&command, name);
	    Tcl_DStringAppend(&command, " ", 1);
	    Tcl_DStringAppend(&command, (version != NULL) ? version : "{}",
		    -1);
	    if (exact) {
		Tcl_DStringAppend(&command, " -exact", 7);
	    }
	    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&command));
	    Tcl_DStringFree(&command);
	    if (code != TCL_OK) {
		if (code == TCL_ERROR) {
		    Tcl_AddErrorInfo(interp,
			    "\n    (\"package unknown\" script)");
		}
		return NULL;
	    }
	    Tcl_ResetResult(interp);
	}
    }

    if (pkgPtr->version == NULL) {
	Tcl_AppendResult(interp, "can't find package ", name,
		(char *) NULL);
	if (version != NULL) {
	    Tcl_AppendResult(interp, " ", version, (char *) NULL);
	}
	return NULL;
    }

    /*
     * At this point we now that the package is present.  Make sure that the
     * provided version meets the current requirement.
     */

    if (version == NULL) {
	return pkgPtr->version;
    }
    result = ComparePkgVersions(pkgPtr->version, version, &satisfies);
    if ((satisfies && !exact) || (result == 0)) {
	return pkgPtr->version;
    }
    Tcl_AppendResult(interp, "version conflict for package \"",
	    name, "\": have ", pkgPtr->version, ", need ", version,
	    (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PackageCmd --
 *
 *	This procedure is invoked to process the "package" Tcl command.
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
Tcl_PackageCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Interp *iPtr = (Interp *) interp;
    size_t length;
    int c, exact, i, satisfies;
    PkgAvail *availPtr, *prevPtr;
    Package *pkgPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_HashTable *tablePtr;
    char *version;
    char buf[30];

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'f') && (strncmp(argv[1], "forget", length) == 0)) {
	for (i = 2; i < argc; i++) {
	    hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv[i]);
	    if (hPtr == NULL) {
		return TCL_OK;
	    }
	    pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	    Tcl_DeleteHashEntry(hPtr);
	    if (pkgPtr->version != NULL) {
		ckfree(pkgPtr->version);
	    }
	    while (pkgPtr->availPtr != NULL) {
		availPtr = pkgPtr->availPtr;
		pkgPtr->availPtr = availPtr->nextPtr;
		ckfree(availPtr->version);
		Tcl_EventuallyFree((ClientData)availPtr->script, TCL_DYNAMIC);
		ckfree((char *) availPtr);
	    }
	    ckfree((char *) pkgPtr);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "ifneeded", length) == 0)) {
	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " ifneeded package version ?script?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (CheckVersion(interp, argv[3]) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (argc == 4) {
	    hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv[2]);
	    if (hPtr == NULL) {
		return TCL_OK;
	    }
	    pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	} else {
	    pkgPtr = FindPackage(interp, argv[2]);
	}
	for (availPtr = pkgPtr->availPtr, prevPtr = NULL; availPtr != NULL;
		prevPtr = availPtr, availPtr = availPtr->nextPtr) {
	    if (ComparePkgVersions(availPtr->version, argv[3], (int *) NULL)
		    == 0) {
		if (argc == 4) {
		    Tcl_SetResult(interp, availPtr->script, TCL_VOLATILE);
		    return TCL_OK;
		}
		Tcl_EventuallyFree((ClientData)availPtr->script, TCL_DYNAMIC);
		break;
	    }
	}
	if (argc == 4) {
	    return TCL_OK;
	}
	if (availPtr == NULL) {
	    availPtr = (PkgAvail *) ckalloc(sizeof(PkgAvail));
	    availPtr->version = ckalloc((unsigned) (strlen(argv[3]) + 1));
	    strcpy(availPtr->version, argv[3]);
	    if (prevPtr == NULL) {
		availPtr->nextPtr = pkgPtr->availPtr;
		pkgPtr->availPtr = availPtr;
	    } else {
		availPtr->nextPtr = prevPtr->nextPtr;
		prevPtr->nextPtr = availPtr;
	    }
	}
	availPtr->script = ckalloc((unsigned) (strlen(argv[4]) + 1));
	strcpy(availPtr->script, argv[4]);
    } else if ((c == 'n') && (strncmp(argv[1], "names", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " names\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tablePtr = &iPtr->packageTable;
	for (hPtr = Tcl_FirstHashEntry(tablePtr, &search); hPtr != NULL;
		hPtr = Tcl_NextHashEntry(&search)) {
	    pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	    if ((pkgPtr->version != NULL) || (pkgPtr->availPtr != NULL)) {
		Tcl_AppendElement(interp, Tcl_GetHashKey(tablePtr, hPtr));
	    }
	}
    } else if ((c == 'p') && (strncmp(argv[1], "provide", length) == 0)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " provide package ?version?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv[2]);
	    if (hPtr != NULL) {
		pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
		if (pkgPtr->version != NULL) {
		    Tcl_SetResult(interp, pkgPtr->version, TCL_VOLATILE);
		}
	    }
	    return TCL_OK;
	}
	if (CheckVersion(interp, argv[3]) != TCL_OK) {
	    return TCL_ERROR;
	}
	return Tcl_PkgProvide(interp, argv[2], argv[3]);
    } else if ((c == 'r') && (strncmp(argv[1], "require", length) == 0)) {
	if (argc < 3) {
	    requireSyntax:
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " require ?-exact? package ?version?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((argv[2][0] == '-') && (strcmp(argv[2], "-exact") == 0)) {
	    exact = 1;
	} else {
	    exact = 0;
	}
	version = NULL;
	if (argc == (4+exact)) {
	    version = argv[3+exact];
	    if (CheckVersion(interp, version) != TCL_OK) {
		return TCL_ERROR;
	    }
	} else if ((argc != 3) || exact) {
	    goto requireSyntax;
	}
	version = Tcl_PkgRequire(interp, argv[2+exact], version, exact);
	if (version == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, version, TCL_VOLATILE);
    } else if ((c == 'u') && (strncmp(argv[1], "unknown", length) == 0)) {
	if (argc == 2) {
	    if (iPtr->packageUnknown != NULL) {
		Tcl_SetResult(interp, iPtr->packageUnknown, TCL_VOLATILE);
	    }
	} else if (argc == 3) {
	    if (iPtr->packageUnknown != NULL) {
		ckfree(iPtr->packageUnknown);
	    }
	    if (argv[2][0] == 0) {
		iPtr->packageUnknown = NULL;
	    } else {
		iPtr->packageUnknown = (char *) ckalloc((unsigned)
			(strlen(argv[2]) + 1));
		strcpy(iPtr->packageUnknown, argv[2]);
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " unknown ?command?\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'v') && (strncmp(argv[1], "vcompare", length) == 0)
	    && (length >= 2)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " vcompare version1 version2\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((CheckVersion(interp, argv[2]) != TCL_OK)
		|| (CheckVersion(interp, argv[3]) != TCL_OK)) {
	    return TCL_ERROR;
	}
	TclFormatInt(buf, ComparePkgVersions(argv[2], argv[3], (int *) NULL));
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    } else if ((c == 'v') && (strncmp(argv[1], "versions", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " versions package\"", (char *) NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv[2]);
	if (hPtr != NULL) {
	    pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	    for (availPtr = pkgPtr->availPtr; availPtr != NULL;
		    availPtr = availPtr->nextPtr) {
		Tcl_AppendElement(interp, availPtr->version);
	    }
	}
    } else if ((c == 'v') && (strncmp(argv[1], "vsatisfies", length) == 0)
	    && (length >= 2)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " vsatisfies version1 version2\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((CheckVersion(interp, argv[2]) != TCL_OK)
		|| (CheckVersion(interp, argv[3]) != TCL_OK)) {
	    return TCL_ERROR;
	}
	ComparePkgVersions(argv[2], argv[3], &satisfies);
	TclFormatInt(buf, satisfies);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be forget, ifneeded, names, ",
		"provide, require, unknown, vcompare, ",
		"versions, or vsatisfies", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindPackage --
 *
 *	This procedure finds the Package record for a particular package
 *	in a particular interpreter, creating a record if one doesn't
 *	already exist.
 *
 * Results:
 *	The return value is a pointer to the Package record for the
 *	package.
 *
 * Side effects:
 *	A new Package record may be created.
 *
 *----------------------------------------------------------------------
 */

static Package *
FindPackage(interp, name)
    Tcl_Interp *interp;		/* Interpreter to use for package lookup. */
    char *name;			/* Name of package to fine. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hPtr;
    int new;
    Package *pkgPtr;

    hPtr = Tcl_CreateHashEntry(&iPtr->packageTable, name, &new);
    if (new) {
	pkgPtr = (Package *) ckalloc(sizeof(Package));
	pkgPtr->version = NULL;
	pkgPtr->availPtr = NULL;
	Tcl_SetHashValue(hPtr, pkgPtr);
    } else {
	pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
    }
    return pkgPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreePackageInfo --
 *
 *	This procedure is called during interpreter deletion to
 *	free all of the package-related information for the
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFreePackageInfo(iPtr)
    Interp *iPtr;		/* Interpereter that is being deleted. */
{
    Package *pkgPtr;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    PkgAvail *availPtr;

    for (hPtr = Tcl_FirstHashEntry(&iPtr->packageTable, &search);
	    hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
	pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	if (pkgPtr->version != NULL) {
	    ckfree(pkgPtr->version);
	}
	while (pkgPtr->availPtr != NULL) {
	    availPtr = pkgPtr->availPtr;
	    pkgPtr->availPtr = availPtr->nextPtr;
	    ckfree(availPtr->version);
	    Tcl_EventuallyFree((ClientData)availPtr->script, TCL_DYNAMIC);
	    ckfree((char *) availPtr);
	}
	ckfree((char *) pkgPtr);
    }
    Tcl_DeleteHashTable(&iPtr->packageTable);
    if (iPtr->packageUnknown != NULL) {
	ckfree(iPtr->packageUnknown);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CheckVersion --
 *
 *	This procedure checks to see whether a version number has
 *	valid syntax.
 *
 * Results:
 *	If string is a properly formed version number the TCL_OK
 *	is returned.  Otherwise TCL_ERROR is returned and an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
CheckVersion(interp, string)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* Supposedly a version number, which is
				 * groups of decimal digits separated
				 * by dots. */
{
    char *p = string;

    if (!isdigit(UCHAR(*p))) {
	goto error;
    }
    for (p++; *p != 0; p++) {
	if (!isdigit(UCHAR(*p)) && (*p != '.')) {
	    goto error;
	}
    }
    if (p[-1] != '.') {
	return TCL_OK;
    }

    error:
    Tcl_AppendResult(interp, "expected version number but got \"",
	    string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ComparePkgVersions --
 *
 *	This procedure compares two version numbers.
 *
 * Results:
 *	The return value is -1 if v1 is less than v2, 0 if the two
 *	version numbers are the same, and 1 if v1 is greater than v2.
 *	If *satPtr is non-NULL, the word it points to is filled in
 *	with 1 if v2 >= v1 and both numbers have the same major number
 *	or 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ComparePkgVersions(v1, v2, satPtr)
    char *v1, *v2;		/* Versions strings, of form 2.1.3 (any
				 * number of version numbers). */
    int *satPtr;		/* If non-null, the word pointed to is
				 * filled in with a 0/1 value.  1 means
				 * v1 "satisfies" v2:  v1 is greater than
				 * or equal to v2 and both version numbers
				 * have the same major number. */
{
    int thisIsMajor, n1, n2;

    /*
     * Each iteration of the following loop processes one number from
     * each string, terminated by a ".".  If those numbers don't match
     * then the comparison is over;  otherwise, we loop back for the
     * next number.
     */

    thisIsMajor = 1;
    while (1) {
	/*
	 * Parse one decimal number from the front of each string.
	 */

	n1 = n2 = 0;
	while ((*v1 != 0) && (*v1 != '.')) {
	    n1 = 10*n1 + (*v1 - '0');
	    v1++;
	}
	while ((*v2 != 0) && (*v2 != '.')) {
	    n2 = 10*n2 + (*v2 - '0');
	    v2++;
	}

	/*
	 * Compare and go on to the next version number if the
	 * current numbers match.
	 */

	if (n1 != n2) {
	    break;
	}
	if (*v1 != 0) {
	    v1++;
	} else if (*v2 == 0) {
	    break;
	}
	if (*v2 != 0) {
	    v2++;
	}
	thisIsMajor = 0;
    }
    if (satPtr != NULL) {
	*satPtr = (n1 == n2) || ((n1 > n2) && !thisIsMajor);
    }
    if (n1 > n2) {
	return 1;
    } else if (n1 == n2) {
	return 0;
    } else {
	return -1;
    }
}
