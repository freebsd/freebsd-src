/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "autoconf.h"
#if HAVE_TCL_H
#include <tcl.h>
#elif HAVE_TCL_TCL_H
#include <tcl/tcl.h>
#endif
#include "tcl_kadm5.h"

#define _TCL_MAIN ((TCL_MAJOR_VERSION * 100 + TCL_MINOR_VERSION) >= 704)

#if _TCL_MAIN
int
main(argc, argv)
    int argc;                   /* Number of command-line arguments. */
    char **argv;                /* Values of command-line arguments. */
{
    Tcl_Main(argc, argv, Tcl_AppInit);
    return 0;                   /* Needed only to prevent compiler warning. */
}
#else
/*
 * The following variable is a special hack that allows applications
 * to be linked using the procedure "main" from the Tcl library.  The
 * variable generates a reference to "main", which causes main to
 * be brought in from the library (and all of Tcl with it).
 */

extern int main();
int *tclDummyMainPtr = (int *) main;
#endif

int Tcl_AppInit(Tcl_Interp *interp)
{
    Tcl_kadm5_init(interp);

    return(TCL_OK);
}
