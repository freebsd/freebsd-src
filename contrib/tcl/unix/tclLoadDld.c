/* 
 * tclLoadDld.c --
 *
 *	This procedure provides a version of the TclLoadFile that
 *	works with the "dld_link" and "dld_get_func" library procedures
 *	for dynamic loading.  It has been tested on Linux 1.1.95 and
 *	dld-3.2.7.  This file probably isn't needed anymore, since it
 *	makes more sense to use "dl_open" etc.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoadDld.c 1.5 97/05/14 13:24:22
 */

#include "tclInt.h"
#include "dld.h"

/*
 * In some systems, like SunOS 4.1.3, the RTLD_NOW flag isn't defined
 * and this argument to dlopen must always be 1.
 */

#ifndef RTLD_NOW
#   define RTLD_NOW 1
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclLoadFile --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	the addresses of two procedures within that file, if they
 *	are defined.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs, an error
 *	message is left in interp->result.  *proc1Ptr and *proc2Ptr
 *	are filled in with the addresses of the symbols given by
 *	*sym1 and *sym2, or NULL if those symbols can't be found.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclLoadFile(interp, fileName, sym1, sym2, proc1Ptr, proc2Ptr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *fileName;		/* Name of the file containing the desired
				 * code. */
    char *sym1, *sym2;		/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
{
    static int firstTime = 1;
    int returnCode;

    /*
     *  The dld package needs to know the pathname to the tcl binary.
     *  If that's not know, return an error.
     */

    if (firstTime) {
	if (tclExecutableName == NULL) {
	    Tcl_SetResult(interp,
		    "don't know name of application binary file, so can't initialize dynamic loader",
		    TCL_STATIC);
	    return TCL_ERROR;
	}
	returnCode = dld_init(tclExecutableName);
	if (returnCode != 0) {
	    Tcl_AppendResult(interp,
		    "initialization failed for dynamic loader: ",
		    dld_strerror(returnCode), (char *) NULL);
	    return TCL_ERROR;
	}
	firstTime = 0;
    }

    if ((returnCode = dld_link(fileName)) != 0) {
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
	    "\": ", dld_strerror(returnCode), (char *) NULL);
	return TCL_ERROR;
    }
    *proc1Ptr = (Tcl_PackageInitProc *) dld_get_func(sym1);
    *proc2Ptr = (Tcl_PackageInitProc *) dld_get_func(sym2);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *	If the "load" command is invoked without providing a package
 *	name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *	Always returns 0 to indicate that we couldn't figure out a
 *	package name;  generic code will then try to guess the package
 *	from the file name.  A return value of 1 would have meant that
 *	we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(fileName, bufPtr)
    char *fileName;		/* Name of file containing package (already
				 * translated to local form if needed). */
    Tcl_DString *bufPtr;	/* Initialized empty dstring.  Append
				 * package name to this if possible. */
{
    return 0;
}
