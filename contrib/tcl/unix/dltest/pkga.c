/* 
 * pkga.c --
 *
 *	This file contains a simple Tcl package "pkga" that is intended
 *	for testing the Tcl dynamic loading facilities.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) pkga.c 1.4 96/02/15 12:30:35
 */
#include "tcl.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static int	Pkga_EqCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
static int	Pkga_QuoteCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

/*
 *----------------------------------------------------------------------
 *
 * Pkga_EqCmd --
 *
 *	This procedure is invoked to process the "pkga_eq" Tcl command.
 *	It expects two arguments and returns 1 if they are the same,
 *	0 if they are different.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
Pkga_EqCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" string1 string2\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1], argv[2]) == 0) {
	interp->result = "1";
    } else {
	interp->result = "0";
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Pkga_quoteCmd --
 *
 *	This procedure is invoked to process the "pkga_quote" Tcl command.
 *	It expects one argument, which it returns as result.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
Pkga_QuoteCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" value\"", (char *) NULL);
	return TCL_ERROR;
    }
    strcpy(interp->result, argv[1]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Pkga_Init --
 *
 *	This is a package initialization procedure, which is called
 *	by Tcl when this package is to be added to an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Pkga_Init(interp)
    Tcl_Interp *interp;		/* Interpreter in which the package is
				 * to be made available. */
{
    int code;

    code = Tcl_PkgProvide(interp, "Pkga", "1.0");
    if (code != TCL_OK) {
	return code;
    }
    Tcl_CreateCommand(interp, "pkga_eq", Pkga_EqCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "pkga_quote", Pkga_QuoteCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    return TCL_OK;
}
