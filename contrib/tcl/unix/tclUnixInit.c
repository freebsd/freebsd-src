/* 
 * tclUnixInit.c --
 *
 *	Contains the Unix-specific interpreter initialization functions.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixInit.c 1.10 96/03/12 09:05:59
 */

#include "tclInt.h"
#include "tclPort.h"
#ifndef NO_UNAME
#   include <sys/utsname.h>
#endif
#if defined(__FreeBSD__)
#include <floatingpoint.h>
#endif

/*
 * Default directory in which to look for libraries:
 */

static char defaultLibraryDir[200] = TCL_LIBRARY;

/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in several different directories
 * for a script "init.tcl" that is compatible with this version
 * of Tcl.  The init.tcl script does all of the real work of
 * initialization.
 */

static char *initScript =
"proc init {} {\n\
    global tcl_library tcl_version tcl_patchLevel env\n\
    rename init {}\n\
    set dirs {}\n\
    if [info exists env(TCL_LIBRARY)] {\n\
	lappend dirs $env(TCL_LIBRARY)\n\
    }\n\
    lappend dirs [info library]\n\
    lappend dirs [file dirname [file dirname [info nameofexecutable]]]/lib/tcl$tcl_version\n\
    if [string match {*[ab]*} $tcl_patchLevel] {\n\
	set lib tcl$tcl_patchLevel\n\
    } else {\n\
	set lib tcl$tcl_version\n\
    }\n\
    lappend dirs [file dirname [file dirname [pwd]]]/$lib/library\n\
    lappend dirs [file dirname [pwd]]/library\n\
    foreach i $dirs {\n\
	set tcl_library $i\n\
	if ![catch {uplevel #0 source $i/init.tcl}] {\n\
	    return\n\
	}\n\
    }\n\
    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\"\n\
    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n\
    error $msg\n\
}\n\
init";

/*
 *----------------------------------------------------------------------
 *
 * TclPlatformInit --
 *
 *	Performs Unix-specific interpreter initialization related to the
 *      tcl_library and tcl_platform variables, and other platform-
 *	specific things.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets "tcl_library" and "tcl_platform" Tcl variables.
 *
 *----------------------------------------------------------------------
 */

void
TclPlatformInit(interp)
    Tcl_Interp *interp;
{
#ifndef NO_UNAME
    struct utsname name;
#endif
    int unameOK;
    static int initialized = 0;

    tclPlatform = TCL_PLATFORM_UNIX;
    Tcl_SetVar(interp, "tcl_library", defaultLibraryDir, TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "tcl_platform", "platform", "unix", TCL_GLOBAL_ONLY);
    unameOK = 0;
#ifndef NO_UNAME
    if (uname(&name) >= 0) {
	unameOK = 1;
	Tcl_SetVar2(interp, "tcl_platform", "os", name.sysname,
		TCL_GLOBAL_ONLY);
	Tcl_SetVar2(interp, "tcl_platform", "osVersion", name.release,
		TCL_GLOBAL_ONLY);
	Tcl_SetVar2(interp, "tcl_platform", "machine", name.machine,
		TCL_GLOBAL_ONLY);
    }
#endif
    if (!unameOK) {
	Tcl_SetVar2(interp, "tcl_platform", "os", "", TCL_GLOBAL_ONLY);
	Tcl_SetVar2(interp, "tcl_platform", "osVersion", "", TCL_GLOBAL_ONLY);
	Tcl_SetVar2(interp, "tcl_platform", "machine", "", TCL_GLOBAL_ONLY);
    }

    if (!initialized) {
	/*
	 * The code below causes SIGPIPE (broken pipe) errors to
	 * be ignored.  This is needed so that Tcl processes don't
	 * die if they create child processes (e.g. using "exec" or
	 * "open") that terminate prematurely.  The signal handler
	 * is only set up when the first interpreter is created;
	 * after this the application can override the handler with
	 * a different one of its own, if it wants.
	 */
    
#ifdef SIGPIPE
	(void) signal(SIGPIPE, SIG_IGN);
#endif /* SIGPIPE */

#ifdef __FreeBSD__
	fpsetround(FP_RN);
	fpsetmask(0L);
#endif
	initialized = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Init --
 *
 *	This procedure is typically invoked by Tcl_AppInit procedures
 *	to perform additional initialization for a Tcl interpreter,
 *	such as sourcing the "init.tcl" script.
 *
 * Results:
 *	Returns a standard Tcl completion code and sets interp->result
 *	if there is an error.
 *
 * Side effects:
 *	Depends on what's in the init.tcl script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Init(interp)
    Tcl_Interp *interp;		/* Interpreter to initialize. */
{
    return Tcl_Eval(interp, initScript);
}
