/* 
 * tclFileName.c --
 *
 *	This file contains routines for converting file names betwen
 *	native and network form.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclFileName.c 1.32 97/08/19 18:44:03
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclRegexp.h"

/*
 * This variable indicates whether the cleanup procedure has been
 * registered for this file yet.
 */

static int initialized = 0;

/*
 * The following regular expression matches the root portion of a Windows
 * absolute or volume relative path.  It will match both UNC and drive relative
 * paths.
 */

#define WIN_ROOT_PATTERN "^(([a-zA-Z]:)|[/\\][/\\]+([^/\\]+)[/\\]+([^/\\]+)|([/\\]))([/\\])*"

/*
 * The following regular expression matches the root portion of a Macintosh
 * absolute path.  It will match degenerate Unix-style paths, tilde paths,
 * Unix-style paths, and Mac paths.
 */

#define MAC_ROOT_PATTERN "^((/+([.][.]?/+)*([.][.]?)?)|(~[^:/]*)(/[^:]*)?|(~[^:]*)(:.*)?|/+([.][.]?/+)*([^:/]+)(/[^:]*)?|([^:]+):.*)$"

/*
 * The following variables are used to hold precompiled regular expressions
 * for use in filename matching.
 */

static regexp *winRootPatternPtr = NULL;
static regexp *macRootPatternPtr = NULL;

/*
 * The following variable is set in the TclPlatformInit call to one
 * of: TCL_PLATFORM_UNIX, TCL_PLATFORM_MAC, or TCL_PLATFORM_WINDOWS.
 */

TclPlatformType tclPlatform = TCL_PLATFORM_UNIX;

/*
 * Prototypes for local procedures defined in this file:
 */

static char *		DoTildeSubst _ANSI_ARGS_((Tcl_Interp *interp,
			    char *user, Tcl_DString *resultPtr));
static char *		ExtractWinRoot _ANSI_ARGS_((char *path,
			    Tcl_DString *resultPtr, int offset));
static void		FileNameCleanup _ANSI_ARGS_((ClientData clientData));
static int		SkipToChar _ANSI_ARGS_((char **stringPtr,
			    char *match));
static char *		SplitMacPath _ANSI_ARGS_((char *path,
			    Tcl_DString *bufPtr));
static char *		SplitWinPath _ANSI_ARGS_((char *path,
			    Tcl_DString *bufPtr));
static char *		SplitUnixPath _ANSI_ARGS_((char *path,
			    Tcl_DString *bufPtr));

/*
 *----------------------------------------------------------------------
 *
 * FileNameCleanup --
 *
 *	This procedure is a Tcl_ExitProc used to clean up the static
 *	data structures used in this file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates storage used by the procedures in this file.
 *
 *----------------------------------------------------------------------
 */

static void
FileNameCleanup(clientData)
    ClientData clientData;	/* Not used. */
{
    if (winRootPatternPtr != NULL) {
	ckfree((char *)winRootPatternPtr);
        winRootPatternPtr = (regexp *) NULL;
    }
    if (macRootPatternPtr != NULL) {
	ckfree((char *)macRootPatternPtr);
        macRootPatternPtr = (regexp *) NULL;
    }
    initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractWinRoot --
 *
 *	Matches the root portion of a Windows path and appends it
 *	to the specified Tcl_DString.
 *	
 * Results:
 *	Returns the position in the path immediately after the root
 *	including any trailing slashes.
 *	Appends a cleaned up version of the root to the Tcl_DString
 *	at the specified offest.
 *
 * Side effects:
 *	Modifies the specified Tcl_DString.
 *
 *----------------------------------------------------------------------
 */

static char *
ExtractWinRoot(path, resultPtr, offset)
    char *path;			/* Path to parse. */
    Tcl_DString *resultPtr;	/* Buffer to hold result. */
    int offset;			/* Offset in buffer where result should be
				 * stored. */
{
    int length;

    /*
     * Initialize the path name parser for Windows path names.
     */

    if (winRootPatternPtr == NULL) {
	winRootPatternPtr = TclRegComp(WIN_ROOT_PATTERN);
	if (!initialized) {
	    Tcl_CreateExitHandler(FileNameCleanup, NULL);
	    initialized = 1;
	}
    }

    /*
     * Match the root portion of a Windows path name.
     */

    if (!TclRegExec(winRootPatternPtr, path, path)) {
	return path;
    }

    Tcl_DStringSetLength(resultPtr, offset);

    if (winRootPatternPtr->startp[2] != NULL) {
	Tcl_DStringAppend(resultPtr, winRootPatternPtr->startp[2], 2);
	if (winRootPatternPtr->startp[6] != NULL) {
	    Tcl_DStringAppend(resultPtr, "/", 1);
	}
    } else if (winRootPatternPtr->startp[4] != NULL) {
	Tcl_DStringAppend(resultPtr, "//", 2);
	length = winRootPatternPtr->endp[3]
	    - winRootPatternPtr->startp[3];
	Tcl_DStringAppend(resultPtr, winRootPatternPtr->startp[3], length);
	Tcl_DStringAppend(resultPtr, "/", 1);
	length = winRootPatternPtr->endp[4]
	    - winRootPatternPtr->startp[4];
	Tcl_DStringAppend(resultPtr, winRootPatternPtr->startp[4], length);
    } else {
	Tcl_DStringAppend(resultPtr, "/", 1);
    }
    return winRootPatternPtr->endp[0];
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetPathType --
 *
 *	Determines whether a given path is relative to the current
 *	directory, relative to the current volume, or absolute.
 *
 * Results:
 *	Returns one of TCL_PATH_ABSOLUTE, TCL_PATH_RELATIVE, or
 *	TCL_PATH_VOLUME_RELATIVE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_PathType
Tcl_GetPathType(path)
    char *path;
{
    Tcl_PathType type = TCL_PATH_ABSOLUTE;

    switch (tclPlatform) {
   	case TCL_PLATFORM_UNIX:
	    /*
	     * Paths that begin with / or ~ are absolute.
	     */

	    if ((path[0] != '/') && (path[0] != '~')) {
		type = TCL_PATH_RELATIVE;
	    }
	    break;

	case TCL_PLATFORM_MAC:
	    if (path[0] == ':') {
		type = TCL_PATH_RELATIVE;
	    } else if (path[0] != '~') {

		/*
		 * Since we have eliminated the easy cases, use the
		 * root pattern to look for the other types.
		 */

		if (!macRootPatternPtr) {
		    macRootPatternPtr = TclRegComp(MAC_ROOT_PATTERN);
		    if (!initialized) {
			Tcl_CreateExitHandler(FileNameCleanup, NULL);
			initialized = 1;
		    }
		}
		if (!TclRegExec(macRootPatternPtr, path, path)
			|| (macRootPatternPtr->startp[2] != NULL)) {
		    type = TCL_PATH_RELATIVE;
		}
	    }
	    break;
	
	case TCL_PLATFORM_WINDOWS:
	    if (path[0] != '~') {

		/*
		 * Since we have eliminated the easy cases, check for
		 * drive relative paths using the regular expression.
		 */

		if (!winRootPatternPtr) {
		    winRootPatternPtr = TclRegComp(WIN_ROOT_PATTERN);
		    if (!initialized) {
			Tcl_CreateExitHandler(FileNameCleanup, NULL);
			initialized = 1;
		    }
		}
		if (TclRegExec(winRootPatternPtr, path, path)) {
		    if (winRootPatternPtr->startp[5]
			    || (winRootPatternPtr->startp[2]
				    && !(winRootPatternPtr->startp[6]))) {
			type = TCL_PATH_VOLUME_RELATIVE;
		    }
		} else {
		    type = TCL_PATH_RELATIVE;
		}
	    }
	    break;
    }
    return type;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SplitPath --
 *
 *	Split a path into a list of path components.  The first element
 *	of the list will have the same path type as the original path.
 *
 * Results:
 *	Returns a standard Tcl result.  The interpreter result contains
 *	a list of path components.
 *	*argvPtr will be filled in with the address of an array
 *	whose elements point to the elements of path, in order.
 *	*argcPtr will get filled in with the number of valid elements
 *	in the array.  A single block of memory is dynamically allocated
 *	to hold both the argv array and a copy of the path elements.
 *	The caller must eventually free this memory by calling ckfree()
 *	on *argvPtr.  Note:  *argvPtr and *argcPtr are only modified
 *	if the procedure returns normally.
 *
 * Side effects:
 *	Allocates memory.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SplitPath(path, argcPtr, argvPtr)
    char *path;			/* Pointer to string containing a path. */
    int *argcPtr;		/* Pointer to location to fill in with
				 * the number of elements in the path. */
    char ***argvPtr;		/* Pointer to place to store pointer to array
				 * of pointers to path elements. */
{
    int i, size;
    char *p;
    Tcl_DString buffer;
    Tcl_DStringInit(&buffer);

    /*
     * Perform platform specific splitting.  These routines will leave the
     * result in the specified buffer.  Individual elements are terminated
     * with a null character.
     */

    p = NULL;			/* Needed only to prevent gcc warnings. */
    switch (tclPlatform) {
   	case TCL_PLATFORM_UNIX:
	    p = SplitUnixPath(path, &buffer);
	    break;

	case TCL_PLATFORM_WINDOWS:
	    p = SplitWinPath(path, &buffer);
	    break;
	    
	case TCL_PLATFORM_MAC:
	    p = SplitMacPath(path, &buffer);
	    break;
    }

    /*
     * Compute the number of elements in the result.
     */

    size = Tcl_DStringLength(&buffer);
    *argcPtr = 0;
    for (i = 0; i < size; i++) {
	if (p[i] == '\0') {
	    (*argcPtr)++;
	}
    }
    
    /*
     * Allocate a buffer large enough to hold the contents of the
     * DString plus the argv pointers and the terminating NULL pointer.
     */

    *argvPtr = (char **) ckalloc((unsigned)
	    ((((*argcPtr) + 1) * sizeof(char *)) + size));

    /*
     * Position p after the last argv pointer and copy the contents of
     * the DString.
     */

    p = (char *) &(*argvPtr)[(*argcPtr) + 1];
    memcpy((VOID *) p, (VOID *) Tcl_DStringValue(&buffer), (size_t) size);

    /*
     * Now set up the argv pointers.
     */

    for (i = 0; i < *argcPtr; i++) {
	(*argvPtr)[i] = p;
	while ((*p++) != '\0') {}
    }
    (*argvPtr)[i] = NULL;

    Tcl_DStringFree(&buffer);
}

/*
 *----------------------------------------------------------------------
 *
 * SplitUnixPath --
 *
 *	This routine is used by Tcl_SplitPath to handle splitting
 *	Unix paths.
 *
 * Results:
 *	Stores a null separated array of strings in the specified
 *	Tcl_DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
SplitUnixPath(path, bufPtr)
    char *path;			/* Pointer to string containing a path. */
    Tcl_DString *bufPtr;	/* Pointer to DString to use for the result. */
{
    int length;
    char *p, *elementStart;

    /*
     * Deal with the root directory as a special case.
     */

    if (path[0] == '/') {
	Tcl_DStringAppend(bufPtr, "/", 2);
	p = path+1;
    } else {
	p = path;
    }

    /*
     * Split on slashes.  Embedded elements that start with tilde will be
     * prefixed with "./" so they are not affected by tilde substitution.
     */

    for (;;) {
	elementStart = p;
	while ((*p != '\0') && (*p != '/')) {
	    p++;
	}
	length = p - elementStart;
	if (length > 0) {
	    if ((elementStart[0] == '~') && (elementStart != path)) {
		Tcl_DStringAppend(bufPtr, "./", 2);
	    }
	    Tcl_DStringAppend(bufPtr, elementStart, length);
	    Tcl_DStringAppend(bufPtr, "", 1);
	}
	if (*p++ == '\0') {
	    break;
	}
    }
    return Tcl_DStringValue(bufPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SplitWinPath --
 *
 *	This routine is used by Tcl_SplitPath to handle splitting
 *	Windows paths.
 *
 * Results:
 *	Stores a null separated array of strings in the specified
 *	Tcl_DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
SplitWinPath(path, bufPtr)
    char *path;			/* Pointer to string containing a path. */
    Tcl_DString *bufPtr;	/* Pointer to DString to use for the result. */
{
    int length;
    char *p, *elementStart;

    p = ExtractWinRoot(path, bufPtr, 0);

    /*
     * Terminate the root portion, if we matched something.
     */

    if (p != path) {
	Tcl_DStringAppend(bufPtr, "", 1);
    }

    /*
     * Split on slashes.  Embedded elements that start with tilde will be
     * prefixed with "./" so they are not affected by tilde substitution.
     */

    do {
	elementStart = p;
	while ((*p != '\0') && (*p != '/') && (*p != '\\')) {
	    p++;
	}
	length = p - elementStart;
	if (length > 0) {
	    if ((elementStart[0] == '~') && (elementStart != path)) {
		Tcl_DStringAppend(bufPtr, "./", 2);
	    }
	    Tcl_DStringAppend(bufPtr, elementStart, length);
	    Tcl_DStringAppend(bufPtr, "", 1);
	}
    } while (*p++ != '\0');

    return Tcl_DStringValue(bufPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SplitMacPath --
 *
 *	This routine is used by Tcl_SplitPath to handle splitting
 *	Macintosh paths.
 *
 * Results:
 *	Returns a newly allocated argv array.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
SplitMacPath(path, bufPtr)
    char *path;			/* Pointer to string containing a path. */
    Tcl_DString *bufPtr;	/* Pointer to DString to use for the result. */
{
    int isMac = 0;		/* 1 if is Mac-style, 0 if Unix-style path. */
    int i, length;
    char *p, *elementStart;

    /*
     * Initialize the path name parser for Macintosh path names.
     */

    if (macRootPatternPtr == NULL) {
	macRootPatternPtr = TclRegComp(MAC_ROOT_PATTERN);
	if (!initialized) {
	    Tcl_CreateExitHandler(FileNameCleanup, NULL);
	    initialized = 1;
	}
    }

    /*
     * Match the root portion of a Mac path name.
     */

    i = 0;			/* Needed only to prevent gcc warnings. */
    if (TclRegExec(macRootPatternPtr, path, path) == 1) {
	/*
	 * Treat degenerate absolute paths like / and /../.. as
	 * Mac relative file names for lack of anything else to do.
	 */

	if (macRootPatternPtr->startp[2] != NULL) {
	    Tcl_DStringAppend(bufPtr, ":", 1);
	    Tcl_DStringAppend(bufPtr, path, macRootPatternPtr->endp[0]
		    - macRootPatternPtr->startp[0] + 1);
	    return Tcl_DStringValue(bufPtr);
	}

	if (macRootPatternPtr->startp[5] != NULL) {

	    /*
	     * Unix-style tilde prefixed paths.
	     */

	    isMac = 0;
	    i = 5;
	} else if (macRootPatternPtr->startp[7] != NULL) {

	    /*
	     * Mac-style tilde prefixed paths.
	     */

	    isMac = 1;
	    i = 7;
	} else if (macRootPatternPtr->startp[10] != NULL) {

	    /*
	     * Normal Unix style paths.
	     */

	    isMac = 0;
	    i = 10;
	} else if (macRootPatternPtr->startp[12] != NULL) {

	    /*
	     * Normal Mac style paths.
	     */

	    isMac = 1;
	    i = 12;
	}

	length = macRootPatternPtr->endp[i]
	    - macRootPatternPtr->startp[i];

	/*
	 * Append the element and terminate it with a : and a null.  Note that
	 * we are forcing the DString to contain an extra null at the end.
	 */

	Tcl_DStringAppend(bufPtr, macRootPatternPtr->startp[i], length);
	Tcl_DStringAppend(bufPtr, ":", 2);
	p = macRootPatternPtr->endp[i];
    } else {
	isMac = (strchr(path, ':') != NULL);
	p = path;
    }
    
    if (isMac) {

	/*
	 * p is pointing at the first colon in the path.  There
	 * will always be one, since this is a Mac-style path.
	 */

	elementStart = p++;
	while ((p = strchr(p, ':')) != NULL) {
	    length = p - elementStart;
	    if (length == 1) {
		while (*p == ':') {
		    Tcl_DStringAppend(bufPtr, "::", 3);
		    elementStart = p++;
		}
	    } else {
		/*
		 * If this is a simple component, drop the leading colon.
		 */

		if ((elementStart[1] != '~')
			&& (strchr(elementStart+1, '/') == NULL)) {
		    elementStart++;
		    length--;
		}
		Tcl_DStringAppend(bufPtr, elementStart, length);
		Tcl_DStringAppend(bufPtr, "", 1);
		elementStart = p++;
	    }
	}
	if (elementStart[1] != '\0' || elementStart == path) {
	    if ((elementStart[1] != '~') && (elementStart[1] != '\0')
			&& (strchr(elementStart+1, '/') == NULL)) {
		    elementStart++;
	    }
	    Tcl_DStringAppend(bufPtr, elementStart, -1);
	    Tcl_DStringAppend(bufPtr, "", 1);
	}
    } else {

	/*
	 * Split on slashes, suppress extra /'s, and convert .. to ::. 
	 */

	for (;;) {
	    elementStart = p;
	    while ((*p != '\0') && (*p != '/')) {
		p++;
	    }
	    length = p - elementStart;
	    if (length > 0) {
		if ((length == 1) && (elementStart[0] == '.')) {
		    Tcl_DStringAppend(bufPtr, ":", 2);
		} else if ((length == 2) && (elementStart[0] == '.')
			&& (elementStart[1] == '.')) {
		    Tcl_DStringAppend(bufPtr, "::", 3);
		} else {
		    if (*elementStart == '~') {
			Tcl_DStringAppend(bufPtr, ":", 1);
		    }
		    Tcl_DStringAppend(bufPtr, elementStart, length);
		    Tcl_DStringAppend(bufPtr, "", 1);
		}
	    }
	    if (*p++ == '\0') {
		break;
	    }
	}
    }
    return Tcl_DStringValue(bufPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_JoinPath --
 *
 *	Combine a list of paths in a platform specific manner.
 *
 * Results:
 *	Appends the joined path to the end of the specified
 *	returning a pointer to the resulting string.  Note that
 *	the Tcl_DString must already be initialized.
 *
 * Side effects:
 *	Modifies the Tcl_DString.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_JoinPath(argc, argv, resultPtr)
    int argc;
    char **argv;
    Tcl_DString *resultPtr;	/* Pointer to previously initialized DString. */
{
    int oldLength, length, i, needsSep;
    Tcl_DString buffer;
    char *p, c, *dest;

    Tcl_DStringInit(&buffer);
    oldLength = Tcl_DStringLength(resultPtr);

    switch (tclPlatform) {
   	case TCL_PLATFORM_UNIX:
	    for (i = 0; i < argc; i++) {
		p = argv[i];
		/*
		 * If the path is absolute, reset the result buffer.
		 * Consume any duplicate leading slashes or a ./ in
		 * front of a tilde prefixed path that isn't at the
		 * beginning of the path.
		 */

		if (*p == '/') {
		    Tcl_DStringSetLength(resultPtr, oldLength);
		    Tcl_DStringAppend(resultPtr, "/", 1);
		    while (*p == '/') {
			p++;
		    }
		} else if (*p == '~') {
		    Tcl_DStringSetLength(resultPtr, oldLength);
		} else if ((Tcl_DStringLength(resultPtr) != oldLength)
			&& (p[0] == '.') && (p[1] == '/')
			&& (p[2] == '~')) {
		    p += 2;
		}

		if (*p == '\0') {
		    continue;
		}

		/*
		 * Append a separator if needed.
		 */

		length = Tcl_DStringLength(resultPtr);
		if ((length != oldLength)
			&& (Tcl_DStringValue(resultPtr)[length-1] != '/')) {
		    Tcl_DStringAppend(resultPtr, "/", 1);
		    length++;
		}

		/*
		 * Append the element, eliminating duplicate and trailing
		 * slashes.
		 */

		Tcl_DStringSetLength(resultPtr, (int) (length + strlen(p)));
		dest = Tcl_DStringValue(resultPtr) + length;
		for (; *p != '\0'; p++) {
		    if (*p == '/') {
			while (p[1] == '/') {
			    p++;
			}
			if (p[1] != '\0') {
			    *dest++ = '/';
			}
		    } else {
			*dest++ = *p;
		    }
		}
		length = dest - Tcl_DStringValue(resultPtr);
		Tcl_DStringSetLength(resultPtr, length);
	    }
	    break;

	case TCL_PLATFORM_WINDOWS:
	    /*
	     * Iterate over all of the components.  If a component is
	     * absolute, then reset the result and start building the
	     * path from the current component on.
	     */

	    for (i = 0; i < argc; i++) {
		p = ExtractWinRoot(argv[i], resultPtr, oldLength);
		length = Tcl_DStringLength(resultPtr);
		
		/*
		 * If the pointer didn't move, then this is a relative path
		 * or a tilde prefixed path.
		 */

		if (p == argv[i]) {
		    /*
		     * Remove the ./ from tilde prefixed elements unless
		     * it is the first component.
		     */

		    if ((length != oldLength)
			    && (p[0] == '.')
			    && ((p[1] == '/') || (p[1] == '\\'))
			    && (p[2] == '~')) {
			p += 2;
		    } else if (*p == '~') {
			Tcl_DStringSetLength(resultPtr, oldLength);
			length = oldLength;
		    }
		}

		if (*p != '\0') {
		    /*
		     * Check to see if we need to append a separator.
		     */

		    
		    if (length != oldLength) {
			c = Tcl_DStringValue(resultPtr)[length-1];
			if ((c != '/') && (c != ':')) {
			    Tcl_DStringAppend(resultPtr, "/", 1);
			}
		    }

		    /*
		     * Append the element, eliminating duplicate and
		     * trailing slashes.
		     */

		    length = Tcl_DStringLength(resultPtr);
		    Tcl_DStringSetLength(resultPtr, (int) (length + strlen(p)));
		    dest = Tcl_DStringValue(resultPtr) + length;
		    for (; *p != '\0'; p++) {
			if ((*p == '/') || (*p == '\\')) {
			    while ((p[1] == '/') || (p[1] == '\\')) {
				p++;
			    }
			    if (p[1] != '\0') {
				*dest++ = '/';
			    }
			} else {
			    *dest++ = *p;
			}
		    }
		    length = dest - Tcl_DStringValue(resultPtr);
		    Tcl_DStringSetLength(resultPtr, length);
		}
	    }
	    break;

	case TCL_PLATFORM_MAC:
	    needsSep = 1;
	    for (i = 0; i < argc; i++) {
		Tcl_DStringSetLength(&buffer, 0);
		p = SplitMacPath(argv[i], &buffer);
		if ((*p != ':') && (*p != '\0')
			&& (strchr(p, ':') != NULL)) {
		    Tcl_DStringSetLength(resultPtr, oldLength);
		    length = strlen(p);
		    Tcl_DStringAppend(resultPtr, p, length);
		    needsSep = 0;
		    p += length+1;
		}

		/*
		 * Now append the rest of the path elements, skipping
		 * : unless it is the first element of the path, and
		 * watching out for :: et al. so we don't end up with
		 * too many colons in the result.
		 */

		for (; *p != '\0'; p += length+1) {
		    if (p[0] == ':' && p[1] == '\0') {
			if (Tcl_DStringLength(resultPtr) != oldLength) {
			    p++;
			} else {
			    needsSep = 0;
			}
		    } else {
			c = p[1];
			if (*p == ':') {
			    if (!needsSep) {
				p++;
			    }
			} else {
			    if (needsSep) {
				Tcl_DStringAppend(resultPtr, ":", 1);
			    }
			}
			needsSep = (c == ':') ? 0 : 1;
		    }
		    length = strlen(p);
		    Tcl_DStringAppend(resultPtr, p, length);
		}
	    }
	    break;
			       
    }
    Tcl_DStringFree(&buffer);
    return Tcl_DStringValue(resultPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TranslateFileName --
 *
 *	Converts a file name into a form usable by the native system
 *	interfaces.  If the name starts with a tilde, it will produce
 *	a name where the tilde and following characters have been
 *	replaced by the home directory location for the named user.
 *
 * Results:
 *	The result is a pointer to a static string containing
 *	the new name.  If there was an error in processing the
 *	name, then an error message is left in interp->result
 *	and the return value is NULL.  The result will be stored
 *	in bufferPtr; the caller must call Tcl_DStringFree(bufferPtr)
 *	to free the name if the return value was not NULL.
 *
 * Side effects:
 *	Information may be left in bufferPtr.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_TranslateFileName(interp, name, bufferPtr)
    Tcl_Interp *interp;		/* Interpreter in which to store error
				 * message (if necessary). */
    char *name;			/* File name, which may begin with "~"
				 * (to indicate current user's home directory)
				 * or "~<user>" (to indicate any user's
				 * home directory). */
    Tcl_DString *bufferPtr;	/* May be used to hold result.  Must not hold
				 * anything at the time of the call, and need
				 * not even be initialized. */
{
    register char *p;

    /*
     * Handle tilde substitutions, if needed.
     */

    if (name[0] == '~') {
	int argc, length;
	char **argv;
	Tcl_DString temp;

	Tcl_SplitPath(name, &argc, &argv);
	
	/*
	 * Strip the trailing ':' off of a Mac path
	 * before passing the user name to DoTildeSubst.
	 */

	if (tclPlatform == TCL_PLATFORM_MAC) {
	    length = strlen(argv[0]);
	    argv[0][length-1] = '\0';
	}
	
	Tcl_DStringInit(&temp);
	argv[0] = DoTildeSubst(interp, argv[0]+1, &temp);
	if (argv[0] == NULL) {
	    Tcl_DStringFree(&temp);
	    ckfree((char *)argv);
	    return NULL;
	}
	Tcl_DStringInit(bufferPtr);
	Tcl_JoinPath(argc, argv, bufferPtr);
	Tcl_DStringFree(&temp);
	ckfree((char*)argv);
    } else {
	Tcl_DStringInit(bufferPtr);
	Tcl_JoinPath(1, &name, bufferPtr);
    }

    /*
     * Convert forward slashes to backslashes in Windows paths because
     * some system interfaces don't accept forward slashes.
     */

    if (tclPlatform == TCL_PLATFORM_WINDOWS) {
	for (p = Tcl_DStringValue(bufferPtr); *p != '\0'; p++) {
	    if (*p == '/') {
		*p = '\\';
	    }
	}
    }
    return Tcl_DStringValue(bufferPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetExtension --
 *
 *	This function returns a pointer to the beginning of the
 *	extension part of a file name.
 *
 * Results:
 *	Returns a pointer into name which indicates where the extension
 *	starts.  If there is no extension, returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetExtension(name)
    char *name;			/* File name to parse. */
{
    char *p, *lastSep;

    /*
     * First find the last directory separator.
     */

    lastSep = NULL;		/* Needed only to prevent gcc warnings. */
    switch (tclPlatform) {
	case TCL_PLATFORM_UNIX:
	    lastSep = strrchr(name, '/');
	    break;

	case TCL_PLATFORM_MAC:
	    if (strchr(name, ':') == NULL) {
		lastSep = strrchr(name, '/');
	    } else {
		lastSep = strrchr(name, ':');
	    }
	    break;

	case TCL_PLATFORM_WINDOWS:
	    lastSep = NULL;
	    for (p = name; *p != '\0'; p++) {
		if (strchr("/\\:", *p) != NULL) {
		    lastSep = p;
		}
	    }
	    break;
    }
    p = strrchr(name, '.');
    if ((p != NULL) && (lastSep != NULL)
	    && (lastSep > p)) {
	p = NULL;
    }

    /*
     * Back up to the first period in a series of contiguous dots.
     * This is needed so foo..o will be split on the first dot.
     */

    if (p != NULL) {
	while ((p > name) && *(p-1) == '.') {
	    p--;
	}
    }
    return p;
}

/*
 *----------------------------------------------------------------------
 *
 * DoTildeSubst --
 *
 *	Given a string following a tilde, this routine returns the
 *	corresponding home directory.
 *
 * Results:
 *	The result is a pointer to a static string containing the home
 *	directory in native format.  If there was an error in processing
 *	the substitution, then an error message is left in interp->result
 *	and the return value is NULL.  On success, the results are appended
 * 	to resultPtr, and the contents of resultPtr are returned.
 *
 * Side effects:
 *	Information may be left in resultPtr.
 *
 *----------------------------------------------------------------------
 */

static char *
DoTildeSubst(interp, user, resultPtr)
    Tcl_Interp *interp;		/* Interpreter in which to store error
				 * message (if necessary). */
    char *user;			/* Name of user whose home directory should be
				 * substituted, or "" for current user. */
    Tcl_DString *resultPtr;	/* May be used to hold result.  Must not hold
				 * anything at the time of the call, and need
				 * not even be initialized. */
{
    char *dir;

    if (*user == '\0') {
	dir = TclGetEnv("HOME");
	if (dir == NULL) {
	    if (interp) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "couldn't find HOME environment ",
			"variable to expand path", (char *) NULL);
	    }
	    return NULL;
	}
	Tcl_JoinPath(1, &dir, resultPtr);
    } else {
	
	/* lint, TclGetuserHome() always NULL under windows. */
	if (TclGetUserHome(user, resultPtr) == NULL) {	
	    if (interp) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "user \"", user, "\" doesn't exist",
			(char *) NULL);
	    }
	    return NULL;
	}
    }
    return resultPtr->string;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GlobCmd --
 *
 *	This procedure is invoked to process the "glob" Tcl command.
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
Tcl_GlobCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i, noComplain, firstArg;
    char c;
    int result = TCL_OK;
    Tcl_DString buffer;
    char *separators, *head, *tail;

    noComplain = 0;
    for (firstArg = 1; (firstArg < argc) && (argv[firstArg][0] == '-');
	    firstArg++) {
	if (strcmp(argv[firstArg], "-nocomplain") == 0) {
	    noComplain = 1;
	} else if (strcmp(argv[firstArg], "--") == 0) {
	    firstArg++;
	    break;
	} else {
	    Tcl_AppendResult(interp, "bad switch \"", argv[firstArg],
		    "\": must be -nocomplain or --", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if (firstArg >= argc) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?switches? name ?name ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    Tcl_DStringInit(&buffer);
    separators = NULL;		/* Needed only to prevent gcc warnings. */
    for (i = firstArg; i < argc; i++) {
	switch (tclPlatform) {
	case TCL_PLATFORM_UNIX:
	    separators = "/";
	    break;
	case TCL_PLATFORM_WINDOWS:
	    separators = "/\\:";
	    break;
	case TCL_PLATFORM_MAC:
	    separators = (strchr(argv[i], ':') == NULL) ? "/" : ":";
	    break;
	}

	Tcl_DStringSetLength(&buffer, 0);

	/*
	 * Perform tilde substitution, if needed.
	 */

	if (argv[i][0] == '~') {
	    char *p;

	    /*
	     * Find the first path separator after the tilde.
	     */

	    for (tail = argv[i]; *tail != '\0'; tail++) {
		if (*tail == '\\') {
		    if (strchr(separators, tail[1]) != NULL) {
			break;
		    }
		} else if (strchr(separators, *tail) != NULL) {
		    break;
		}
	    }

	    /*
	     * Determine the home directory for the specified user.  Note that
	     * we don't allow special characters in the user name.
	     */

	    c = *tail;
	    *tail = '\0';
	    p = strpbrk(argv[i]+1, "\\[]*?{}");
	    if (p == NULL) {
		head = DoTildeSubst(interp, argv[i]+1, &buffer);
	    } else {
		if (!noComplain) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "globbing characters not ",
			    "supported in user names", (char *) NULL);
		}
		head = NULL;
	    }
	    *tail = c;
	    if (head == NULL) {
		if (noComplain) {
		    Tcl_ResetResult(interp);
		    continue;
		} else {
		    result = TCL_ERROR;
		    goto done;
		}
	    }
	    if (head != Tcl_DStringValue(&buffer)) {
		Tcl_DStringAppend(&buffer, head, -1);
	    }
	} else {
	    tail = argv[i];
	}

	result = TclDoGlob(interp, separators, &buffer, tail);
	if (result != TCL_OK) {
	    if (noComplain) {
		/*
		 * We should in fact pass down the nocomplain flag 
		 * or save the interp result or use another mecanism
		 * so the interp result is not mangled on errors in that case.
		 * but that would a bigger change than reasonable for a patch
		 * release.
		 * (see fileName.test 15.2-15.4 for expected behaviour)
		 */
		Tcl_ResetResult(interp);
		result = TCL_OK;
		continue;
	    } else {
		goto done;
	    }
	}
    }

    if ((*interp->result == 0) && !noComplain) {
	char *sep = "";

	Tcl_AppendResult(interp, "no files matched glob pattern",
		(argc == 2) ? " \"" : "s \"", (char *) NULL);
	for (i = firstArg; i < argc; i++) {
	    Tcl_AppendResult(interp, sep, argv[i], (char *) NULL);
	    sep = " ";
	}
	Tcl_AppendResult(interp, "\"", (char *) NULL);
	result = TCL_ERROR;
    }
done:
    Tcl_DStringFree(&buffer);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SkipToChar --
 *
 *	This function traverses a glob pattern looking for the next
 *	unquoted occurance of the specified character at the same braces
 *	nesting level.
 *
 * Results:
 *	Updates stringPtr to point to the matching character, or to
 *	the end of the string if nothing matched.  The return value
 *	is 1 if a match was found at the top level, otherwise it is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SkipToChar(stringPtr, match)
    char **stringPtr;			/* Pointer string to check. */
    char *match;			/* Pointer to character to find. */
{
    int quoted, level;
    register char *p;

    quoted = 0;
    level = 0;

    for (p = *stringPtr; *p != '\0'; p++) {
	if (quoted) {
	    quoted = 0;
	    continue;
	}
	if ((level == 0) && (*p == *match)) {
	    *stringPtr = p;
	    return 1;
	}
	if (*p == '{') {
	    level++;
	} else if (*p == '}') {
	    level--;
	} else if (*p == '\\') {
	    quoted = 1;
	}
    }
    *stringPtr = p;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDoGlob --
 *
 *	This recursive procedure forms the heart of the globbing
 *	code.  It performs a depth-first traversal of the tree
 *	given by the path name to be globbed.  The directory and
 *	remainder are assumed to be native format paths.
 *
 * Results:
 *	The return value is a standard Tcl result indicating whether
 *	an error occurred in globbing.  After a normal return the
 *	result in interp will be set to hold all of the file names
 *	given by the dir and rem arguments.  After an error the
 *	result in interp will hold an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclDoGlob(interp, separators, headPtr, tail)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting
				 * (e.g. unmatched brace). */
    char *separators;		/* String containing separator characters
				 * that should be used to identify globbing
				 * boundaries. */
    Tcl_DString *headPtr;	/* Completely expanded prefix. */
    char *tail;			/* The unexpanded remainder of the path. */
{
    int baseLength, quoted, count;
    int result = TCL_OK;
    char *p, *openBrace, *closeBrace, *name, *firstSpecialChar, savedChar;
    char lastChar = 0;
    int length = Tcl_DStringLength(headPtr);

    if (length > 0) {
	lastChar = Tcl_DStringValue(headPtr)[length-1];
    }

    /*
     * Consume any leading directory separators, leaving tail pointing
     * just past the last initial separator.
     */

    count = 0;
    name = tail;
    for (; *tail != '\0'; tail++) {
	if ((*tail == '\\') && (strchr(separators, tail[1]) != NULL)) {
	    tail++;
	} else if (strchr(separators, *tail) == NULL) {
	    break;
	}
	count++;
    }

    /*
     * Deal with path separators.  On the Mac, we have to watch out
     * for multiple separators, since they are special in Mac-style
     * paths.
     */

    switch (tclPlatform) {
	case TCL_PLATFORM_MAC:
	    if (*separators == '/') {
		if (((length == 0) && (count == 0))
			|| ((length > 0) && (lastChar != ':'))) {
		    Tcl_DStringAppend(headPtr, ":", 1);
		}
	    } else {
		if (count == 0) {
		    if ((length > 0) && (lastChar != ':')) {
			Tcl_DStringAppend(headPtr, ":", 1);
		    }
		} else {
		    if (lastChar == ':') {
			count--;
		    }
		    while (count-- > 0) {
			Tcl_DStringAppend(headPtr, ":", 1);
		    }
		}
	    }
	    break;
	case TCL_PLATFORM_WINDOWS:
	    /*
	     * If this is a drive relative path, add the colon and the
	     * trailing slash if needed.  Otherwise add the slash if
	     * this is the first absolute element, or a later relative
	     * element.  Add an extra slash if this is a UNC path.
	     */

	    if (*name == ':') {
		Tcl_DStringAppend(headPtr, ":", 1);
		if (count > 1) {
		    Tcl_DStringAppend(headPtr, "/", 1);
		}
	    } else if ((*tail != '\0')
		    && (((length > 0)
			    && (strchr(separators, lastChar) == NULL))
			    || ((length == 0) && (count > 0)))) {
		Tcl_DStringAppend(headPtr, "/", 1);
		if ((length == 0) && (count > 1)) {
		    Tcl_DStringAppend(headPtr, "/", 1);
		}
	    }
	    
	    break;
	case TCL_PLATFORM_UNIX:
	    /*
	     * Add a separator if this is the first absolute element, or
	     * a later relative element.
	     */

	    if ((*tail != '\0')
		    && (((length > 0)
			    && (strchr(separators, lastChar) == NULL))
			    || ((length == 0) && (count > 0)))) {
		Tcl_DStringAppend(headPtr, "/", 1);
	    }
	    break;
    }

    /*
     * Look for the first matching pair of braces or the first
     * directory separator that is not inside a pair of braces.
     */

    openBrace = closeBrace = NULL;
    quoted = 0;
    for (p = tail; *p != '\0'; p++) {
	if (quoted) {
	    quoted = 0;
	} else if (*p == '\\') {
	    quoted = 1;
	    if (strchr(separators, p[1]) != NULL) {
		break;			/* Quoted directory separator. */
	    }
	} else if (strchr(separators, *p) != NULL) {
	    break;			/* Unquoted directory separator. */
	} else if (*p == '{') {
	    openBrace = p;
	    p++;
	    if (SkipToChar(&p, "}")) {
		closeBrace = p;		/* Balanced braces. */
		break;
	    }
	    Tcl_SetResult(interp, "unmatched open-brace in file name",
		    TCL_STATIC);
	    return TCL_ERROR;
	} else if (*p == '}') {
	    Tcl_SetResult(interp, "unmatched close-brace in file name",
		    TCL_STATIC);
	    return TCL_ERROR;
	}
    }

    /*
     * Substitute the alternate patterns from the braces and recurse.
     */

    if (openBrace != NULL) {
	char *element;
	Tcl_DString newName;
	Tcl_DStringInit(&newName);

	/*
	 * For each element within in the outermost pair of braces,
	 * append the element and the remainder to the fixed portion
	 * before the first brace and recursively call TclDoGlob.
	 */

	Tcl_DStringAppend(&newName, tail, openBrace-tail);
	baseLength = Tcl_DStringLength(&newName);
	length = Tcl_DStringLength(headPtr);
	*closeBrace = '\0';
	for (p = openBrace; p != closeBrace; ) {
	    p++;
	    element = p;
	    SkipToChar(&p, ",");
	    Tcl_DStringSetLength(headPtr, length);
	    Tcl_DStringSetLength(&newName, baseLength);
	    Tcl_DStringAppend(&newName, element, p-element);
	    Tcl_DStringAppend(&newName, closeBrace+1, -1);
	    result = TclDoGlob(interp, separators,
		    headPtr, Tcl_DStringValue(&newName));
	    if (result != TCL_OK) {
		break;
	    }
	}
	*closeBrace = '}';
	Tcl_DStringFree(&newName);
	return result;
    }

    /*
     * At this point, there are no more brace substitutions to perform on
     * this path component.  The variable p is pointing at a quoted or
     * unquoted directory separator or the end of the string.  So we need
     * to check for special globbing characters in the current pattern.
     * We avoid modifying tail if p is pointing at the end of the string.
     */

    if (*p != '\0') {
	 savedChar = *p;
	 *p = '\0';
	 firstSpecialChar = strpbrk(tail, "*[]?\\");
	 *p = savedChar;
    } else {
	firstSpecialChar = strpbrk(tail, "*[]?\\");
    }

    if (firstSpecialChar != NULL) {
	/*
	 * Look for matching files in the current directory.  The
	 * implementation of this function is platform specific, but may
	 * recursively call TclDoGlob.  For each file that matches, it will
	 * add the match onto the interp->result, or call TclDoGlob if there
	 * are more characters to be processed.
	 */

	return TclMatchFiles(interp, separators, headPtr, tail, p);
    }
    Tcl_DStringAppend(headPtr, tail, p-tail);
    if (*p != '\0') {
	return TclDoGlob(interp, separators, headPtr, p);
    }

    /*
     * There are no more wildcards in the pattern and no more unprocessed
     * characters in the tail, so now we can construct the path and verify
     * the existence of the file.
     */

    switch (tclPlatform) {
	case TCL_PLATFORM_MAC:
	    if (strchr(Tcl_DStringValue(headPtr), ':') == NULL) {
		Tcl_DStringAppend(headPtr, ":", 1);
	    }
	    name = Tcl_DStringValue(headPtr);
	    if (access(name, F_OK) == 0) {
		if ((name[1] != '\0') && (strchr(name+1, ':') == NULL)) {
		    Tcl_AppendElement(interp, name+1);
		} else {
		    Tcl_AppendElement(interp, name);
		}
	    }
	    break;
	case TCL_PLATFORM_WINDOWS: {
	    int exists;
	    /*
	     * We need to convert slashes to backslashes before checking
	     * for the existence of the file.  Once we are done, we need
	     * to convert the slashes back.
	     */

	    if (Tcl_DStringLength(headPtr) == 0) {
		if (((*name == '\\') && (name[1] == '/' || name[1] == '\\'))
			|| (*name == '/')) {
		    Tcl_DStringAppend(headPtr, "\\", 1);
		} else {
		    Tcl_DStringAppend(headPtr, ".", 1);
		}
	    } else {
		for (p = Tcl_DStringValue(headPtr); *p != '\0'; p++) {
		    if (*p == '/') {
			*p = '\\';
		    }
		}
	    }
	    name = Tcl_DStringValue(headPtr);
	    exists = (access(name, F_OK) == 0);
	    for (p = name; *p != '\0'; p++) {
		if (*p == '\\') {
		    *p = '/';
		}
	    }
	    if (exists) {
		Tcl_AppendElement(interp, name);
	    }
	    break;
	}
	case TCL_PLATFORM_UNIX:
	    if (Tcl_DStringLength(headPtr) == 0) {
		if ((*name == '\\' && name[1] == '/') || (*name == '/')) {
		    Tcl_DStringAppend(headPtr, "/", 1);
		} else {
		    Tcl_DStringAppend(headPtr, ".", 1);
		}
	    }
	    name = Tcl_DStringValue(headPtr);
	    if (access(name, F_OK) == 0) {
		Tcl_AppendElement(interp, name);
	    }
	    break;
    }

    return TCL_OK;
}
