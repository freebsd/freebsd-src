/* 
 * tclLoadShl.c --
 *
 *	This procedure provides a version of the TclLoadFile that works
 *	with the "shl_load" and "shl_findsym" library procedures for
 *	dynamic loading (e.g. for HP machines).
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoadShl.c 1.5 96/03/15 15:01:44
 */

#include <dl.h>

/*
 * On some HP machines, dl.h defines EXTERN; remove that definition.
 */

#ifdef EXTERN
#   undef EXTERN
#endif

#include "tcl.h"

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
    shl_t handle;
    Tcl_DString newName;

    handle = shl_load(fileName, BIND_IMMEDIATE, 0L);
    if (handle == NULL) {
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Some versions of the HP system software still use "_" at the
     * beginning of exported symbols while others don't;  try both
     * forms of each name.
     */

    if (shl_findsym(&handle, sym1, (short) TYPE_PROCEDURE, (void *) proc1Ptr)
	    != 0) {
	Tcl_DStringInit(&newName);
	Tcl_DStringAppend(&newName, "_", 1);
	Tcl_DStringAppend(&newName, sym1, -1);
	if (shl_findsym(&handle, Tcl_DStringValue(&newName),
		(short) TYPE_PROCEDURE, (void *) proc1Ptr) != 0) {
	    *proc1Ptr = NULL;
	}
	Tcl_DStringFree(&newName);
    }
    if (shl_findsym(&handle, sym2, (short) TYPE_PROCEDURE, (void *) proc2Ptr)
	    != 0) {
	Tcl_DStringInit(&newName);
	Tcl_DStringAppend(&newName, "_", 1);
	Tcl_DStringAppend(&newName, sym2, -1);
	if (shl_findsym(&handle, Tcl_DStringValue(&newName),
		(short) TYPE_PROCEDURE, (void *) proc2Ptr) != 0) {
	    *proc2Ptr = NULL;
	}
	Tcl_DStringFree(&newName);
    }
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
