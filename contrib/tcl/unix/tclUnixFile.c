/* 
 * tclUnixFile.c --
 *
 *      This file contains wrappers around UNIX file handling functions.
 *      These wrappers mask differences between Windows and UNIX.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixFile.c 1.38 96/04/18 08:43:51
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The variable below caches the name of the current working directory
 * in order to avoid repeated calls to getcwd.  The string is malloc-ed.
 * NULL means the cache needs to be refreshed.
 */

static char *currentDir =  NULL;
static int currentDirExitHandlerSet = 0;

/*
 * The variable below is set if the exit routine for deleting the string
 * containing the executable name has been registered.
 */

static int executableNameExitHandlerSet = 0;

extern pid_t waitpid _ANSI_ARGS_((pid_t pid, int *stat_loc, int options));

/*
 * Static routines for this file:
 */

static void	FreeCurrentDir _ANSI_ARGS_((ClientData clientData));
static void	FreeExecutableName _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Implements the waitpid system call on Unix systems.
 *
 * Results:
 *	Result of calling waitpid.
 *
 * Side effects:
 *	Waits for a process to terminate.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitPid(pid, statPtr, options)
    int pid;
    int *statPtr;
    int options;
{
    int result;
    pid_t real_pid;

    real_pid = (pid_t) pid;
    while (1) {
	result = (int) waitpid(real_pid, statPtr, options);
	if ((result != -1) || (errno != EINTR)) {
	    return result;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeCurrentDir --
 *
 *	Frees the string stored in the currentDir variable. This routine
 *	is registered as an exit handler and will be called during shutdown.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the memory occuppied by the currentDir value.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
FreeCurrentDir(clientData)
    ClientData clientData;	/* Not used. */
{
    if (currentDir != (char *) NULL) {
        ckfree(currentDir);
        currentDir = (char *) NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeExecutableName --
 *
 *	Frees the string stored in the tclExecutableName variable. This
 *	routine is registered as an exit handler and will be called
 *	during shutdown.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the memory occuppied by the tclExecutableName value.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
FreeExecutableName(clientData)
    ClientData clientData;	/* Not used. */
{
    if (tclExecutableName != (char *) NULL) {
        ckfree(tclExecutableName);
        tclExecutableName = (char *) NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclChdir --
 *
 *	Change the current working directory.
 *
 * Results:
 *	The result is a standard Tcl result.  If an error occurs and 
 *	interp isn't NULL, an error message is left in interp->result.
 *
 * Side effects:
 *	The working directory for this application is changed.  Also
 *	the cache maintained used by TclGetCwd is deallocated and
 *	set to NULL.
 *
 *----------------------------------------------------------------------
 */

int
TclChdir(interp, dirName)
    Tcl_Interp *interp;		/* If non NULL, used for error reporting. */
    char *dirName;     		/* Path to new working directory. */
{
    if (currentDir != NULL) {
	ckfree(currentDir);
	currentDir = NULL;
    }
    if (chdir(dirName) != 0) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "couldn't change working directory to \"",
		    dirName, "\": ", Tcl_PosixError(interp), (char *) NULL);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetCwd --
 *
 *	Return the path name of the current working directory.
 *
 * Results:
 *	The result is the full path name of the current working
 *	directory, or NULL if an error occurred while figuring it out.
 *	The returned string is owned by the TclGetCwd routine and must
 *	not be freed by the caller.  If an error occurs and interp
 *	isn't NULL, an error message is left in interp->result.
 *
 * Side effects:
 *	The path name is cached to avoid having to recompute it
 *	on future calls;  if it is already cached, the cached
 *	value is returned.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetCwd(interp)
    Tcl_Interp *interp;		/* If non NULL, used for error reporting. */
{
    char buffer[MAXPATHLEN+1];

    if (currentDir == NULL) {
        if (!currentDirExitHandlerSet) {
            currentDirExitHandlerSet = 1;
            Tcl_CreateExitHandler(FreeCurrentDir, (ClientData) NULL);
        }
	if (getcwd(buffer, MAXPATHLEN+1) == NULL) {
	    if (interp != NULL) {
		if (errno == ERANGE) {
		    interp->result = "working directory name is too long";
		} else {
		    Tcl_AppendResult(interp,
			    "error getting working directory name: ",
			    Tcl_PosixError(interp), (char *) NULL);
		}
	    }
	    return NULL;
	}
	currentDir = (char *) ckalloc((unsigned) (strlen(buffer) + 1));
	strcpy(currentDir, buffer);
    }
    return currentDir;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFile --
 *
 *	Implements a mechanism to open files on Unix systems.
 *
 * Results:
 *	The opened file.
 *
 * Side effects:
 *	May cause a file to be created on the file system.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclOpenFile(fname, mode)
    char *fname;			/* The name of the file to open. */
    int mode;				/* In what mode to open the file? */
{
    int fd;

    fd = open(fname, mode, 0600);
    if (fd != -1) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        return Tcl_GetFile((ClientData)fd, TCL_UNIX_FD);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCloseFile --
 *
 *	Implements a mechanism to close a UNIX file.
 *
 * Results:
 *	Returns 0 on success, or -1 on error, setting errno.
 *
 * Side effects:
 *	The file is closed.
 *
 *----------------------------------------------------------------------
 */

int
TclCloseFile(file)
    Tcl_File file;	/* The file to close. */
{
    int type;
    int fd;
    int result;

    fd = (int) Tcl_GetFileInfo(file, &type);
    if (type != TCL_UNIX_FD) {
	panic("Tcl_CloseFile: unexpected file type");
    }

    /*
     * Refuse to close the fds for stdin, stdout and stderr.
     */
    
    if ((fd == 0) || (fd == 1) || (fd == 2)) {
        return 0;
    }
    
    result = close(fd);
    Tcl_DeleteFileHandler(file);
    Tcl_FreeFile(file);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclReadFile --
 *
 *	Implements a mechanism to read from files on Unix systems. Also
 *	simulates blocking behavior on non-blocking files when asked to.
 *
 * Results:
 *	The number of characters read from the specified file.
 *
 * Side effects:
 *	May consume characters from the file.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
int
TclReadFile(file, shouldBlock, buf, toRead)
    Tcl_File file;	/* The file to read from. */
    int shouldBlock;		/* Not used. */
    char *buf;			/* The buffer to store input in. */
    int toRead;			/* Number of characters to read. */
{
    int type, fd;

    fd = (int) Tcl_GetFileInfo(file, &type);
    if (type != TCL_UNIX_FD) {
	panic("Tcl_ReadFile: unexpected file type");
    }

    return read(fd, buf, (size_t) toRead);
}

/*
 *----------------------------------------------------------------------
 *
 * TclWriteFile --
 *
 *	Implements a mechanism to write to files on Unix systems.
 *
 * Results:
 *	The number of characters written to the specified file.
 *
 * Side effects:
 *	May produce characters on the file.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
TclWriteFile(file, shouldBlock, buf, toWrite)
    Tcl_File file;	/* The file to write to. */
    int shouldBlock;		/* Not used. */
    char *buf;			/* Where output is stored. */
    int toWrite;		/* Number of characters to write. */
{
    int type, fd;

    fd = (int) Tcl_GetFileInfo(file, &type);
    if (type != TCL_UNIX_FD) {
	panic("Tcl_WriteFile: unexpected file type");
    }
    return write(fd, buf, (size_t) toWrite);
}

/*
 *----------------------------------------------------------------------
 *
 * TclSeekFile --
 *
 *	Sets the file pointer on the indicated UNIX file.
 *
 * Results:
 *	The new position at which the file will be accessed, or -1 on
 *	failure.
 *
 * Side effects:
 *	May change the position at which subsequent operations access the
 *	file designated by the file.
 *
 *----------------------------------------------------------------------
 */

int
TclSeekFile(file, offset, whence)
    Tcl_File file;	/* The file to seek on. */
    int offset;			/* How far to seek? */
    int whence;			/* And from where to seek? */
{
    int type, fd;

    fd = (int) Tcl_GetFileInfo(file, &type);
    if (type != TCL_UNIX_FD) {
	panic("Tcl_SeekFile: unexpected file type");
    }

    return lseek(fd, offset, whence);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateTempFile --
 *
 *	This function creates a temporary file initialized with an
 *	optional string, and returns a file handle with the file pointer
 *	at the beginning of the file.
 *
 * Results:
 *	A handle to a file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclCreateTempFile(contents)
    char *contents;		/* String to write into temp file, or NULL. */
{
    char fileName[L_tmpnam];
    Tcl_File file;
    size_t length = (contents == NULL) ? 0 : strlen(contents);

    tmpnam(fileName);
    file = TclOpenFile(fileName, O_RDWR|O_CREAT|O_TRUNC);
    unlink(fileName);

    if ((file != NULL) && (length > 0)) {
	int fd = (int)Tcl_GetFileInfo(file, NULL);
	while (1) {
	    if (write(fd, contents, length) != -1) {
		break;
	    } else if (errno != EINTR) {
		close(fd);
		Tcl_FreeFile(file);
		return NULL;
	    }
	}
	lseek(fd, 0, SEEK_SET);
    }
    return file;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindExecutable --
 *
 *	This procedure computes the absolute path name of the current
 *	application, given its argv[0] value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The variable tclExecutableName gets filled in with the file
 *	name for the application, if we figured it out.  If we couldn't
 *	figure it out, Tcl_FindExecutable is set to NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FindExecutable(argv0)
    char *argv0;		/* The value of the application's argv[0]. */
{
    char *name, *p, *cwd;
    Tcl_DString buffer;
    int length;

    Tcl_DStringInit(&buffer);
    if (tclExecutableName != NULL) {
	ckfree(tclExecutableName);
	tclExecutableName = NULL;
    }

    name = argv0;
    for (p = name; *p != 0; p++) {
	if (*p == '/') {
	    /*
	     * The name contains a slash, so use the name directly
	     * without doing a path search.
	     */

	    goto gotName;
	}
    }

    p = getenv("PATH");
    if (p == NULL) {
	/*
	 * There's no PATH environment variable; use the default that
	 * is used by sh.
	 */

	p = ":/bin:/usr/bin";
    }

    /*
     * Search through all the directories named in the PATH variable
     * to see if argv[0] is in one of them.  If so, use that file
     * name.
     */

    while (*p != 0) {
	while (isspace(UCHAR(*p))) {
	    p++;
	}
	name = p;
	while ((*p != ':') && (*p != 0)) {
	    p++;
	}
	Tcl_DStringSetLength(&buffer, 0);
	if (p != name) {
	    Tcl_DStringAppend(&buffer, name, p-name);
	    if (p[-1] != '/') {
		Tcl_DStringAppend(&buffer, "/", 1);
	    }
	}
	Tcl_DStringAppend(&buffer, argv0, -1);
	if (access(Tcl_DStringValue(&buffer), X_OK) == 0) {
	    name = Tcl_DStringValue(&buffer);
	    goto gotName;
	}
	p++;
    }
    goto done;

    /*
     * If the name starts with "/" then just copy it to tclExecutableName.
     */

    gotName:
    if (name[0] == '/')  {
	tclExecutableName = (char *) ckalloc((unsigned) (strlen(name) + 1));
	strcpy(tclExecutableName, name);
	goto done;
    }

    /*
     * The name is relative to the current working directory.  First
     * strip off a leading "./", if any, then add the full path name of
     * the current working directory.
     */

    if ((name[0] == '.') && (name[1] == '/')) {
	name += 2;
    }
    cwd = TclGetCwd((Tcl_Interp *) NULL);
    if (cwd == NULL) {
	tclExecutableName = NULL;
	goto done;
    }
    length = strlen(cwd);
    tclExecutableName = (char *) ckalloc((unsigned)
	    (length + strlen(name) + 2));
    strcpy(tclExecutableName, cwd);
    tclExecutableName[length] = '/';
    strcpy(tclExecutableName + length + 1, name);

    done:
    Tcl_DStringFree(&buffer);

    if (!executableNameExitHandlerSet) {
        executableNameExitHandlerSet = 1;
        Tcl_CreateExitHandler(FreeExecutableName, (ClientData) NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetUserHome --
 *
 *	This function takes the passed in user name and finds the
 *	corresponding home directory specified in the password file.
 *
 * Results:
 *	The result is a pointer to a static string containing
 *	the new name.  If there was an error in processing the
 *	user name then the return value is NULL.  Otherwise the
 *	result is stored in bufferPtr, and the caller must call
 *	Tcl_DStringFree(bufferPtr) to free the result.
 *
 * Side effects:
 *	Information may be left in bufferPtr.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetUserHome(name, bufferPtr)
    char *name;			/* User name to use to find home directory. */
    Tcl_DString *bufferPtr;	/* May be used to hold result.  Must not hold
				 * anything at the time of the call, and need
				 * not even be initialized. */
{
    struct passwd *pwPtr;

    pwPtr = getpwnam(name);
    if (pwPtr == NULL) {
	endpwent();
	return NULL;
    }
    Tcl_DStringInit(bufferPtr);
    Tcl_DStringAppend(bufferPtr, pwPtr->pw_dir, -1);
    endpwent();
    return bufferPtr->string;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMatchFiles --
 *
 *	This routine is used by the globbing code to search a
 *	directory for all files which match a given pattern.
 *
 * Results: 
 *	If the tail argument is NULL, then the matching files are
 *	added to the interp->result.  Otherwise, TclDoGlob is called
 *	recursively for each matching subdirectory.  The return value
 *	is a standard Tcl result indicating whether an error occurred
 *	in globbing.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

int
TclMatchFiles(interp, separators, dirPtr, pattern, tail)
    Tcl_Interp *interp;		/* Interpreter to receive results. */
    char *separators;		/* Path separators to pass to TclDoGlob. */
    Tcl_DString *dirPtr;	/* Contains path to directory to search. */
    char *pattern;		/* Pattern to match against. */
    char *tail;			/* Pointer to end of pattern. */
{
    char *dirName, *patternEnd = tail;
    char savedChar = 0;		/* Initialization needed only to prevent
				 * compiler warning from gcc. */
    DIR *d;
    struct stat statBuf;
    struct dirent *entryPtr;
    int matchHidden;
    int result = TCL_OK;
    int baseLength = Tcl_DStringLength(dirPtr);

    /*
     * Make sure that the directory part of the name really is a
     * directory.  If the directory name is "", use the name "."
     * instead, because some UNIX systems don't treat "" like "."
     * automatically.  Keep the "" for use in generating file names,
     * otherwise "glob foo.c" would return "./foo.c".
     */

    if (dirPtr->string[0] == '\0') {
	dirName = ".";
    } else {
	dirName = dirPtr->string;
    }
    if ((stat(dirName, &statBuf) != 0) || !S_ISDIR(statBuf.st_mode)) {
	return TCL_OK;
    }

    /*
     * Check to see if the pattern needs to compare with hidden files.
     */

    if ((pattern[0] == '.')
	    || ((pattern[0] == '\\') && (pattern[1] == '.'))) {
	matchHidden = 1;
    } else {
	matchHidden = 0;
    }

    /*
     * Now open the directory for reading and iterate over the contents.
     */

    d = opendir(dirName);
    if (d == NULL) {
	Tcl_ResetResult(interp);

	/*
	 * Strip off a trailing '/' if necessary, before reporting the error.
	 */

	if (baseLength > 0) {
	    savedChar = dirPtr->string[baseLength-1];
	    if (savedChar == '/') {
		dirPtr->string[baseLength-1] = '\0';
	    }
	}
	Tcl_AppendResult(interp, "couldn't read directory \"",
		dirPtr->string, "\": ", Tcl_PosixError(interp), (char *) NULL);
	if (baseLength > 0) {
	    dirPtr->string[baseLength-1] = savedChar;
	}
	return TCL_ERROR;
    }

    /*
     * Clean up the end of the pattern and the tail pointer.  Leave
     * the tail pointing to the first character after the path separator
     * following the pattern, or NULL.  Also, ensure that the pattern
     * is null-terminated.
     */

    if (*tail == '\\') {
	tail++;
    }
    if (*tail == '\0') {
	tail = NULL;
    } else {
	tail++;
    }
    savedChar = *patternEnd;
    *patternEnd = '\0';

    while (1) {
	entryPtr = readdir(d);
	if (entryPtr == NULL) {
	    break;
	}

	/*
	 * Don't match names starting with "." unless the "." is
	 * present in the pattern.
	 */

	if (!matchHidden && (*entryPtr->d_name == '.')) {
	    continue;
	}

	/*
	 * Now check to see if the file matches.  If there are more
	 * characters to be processed, then ensure matching files are
	 * directories before calling TclDoGlob. Otherwise, just add
	 * the file to the result.
	 */

	if (Tcl_StringMatch(entryPtr->d_name, pattern)) {
	    Tcl_DStringSetLength(dirPtr, baseLength);
	    Tcl_DStringAppend(dirPtr, entryPtr->d_name, -1);
	    if (tail == NULL) {
		Tcl_AppendElement(interp, dirPtr->string);
	    } else if ((stat(dirPtr->string, &statBuf) == 0)
		    && S_ISDIR(statBuf.st_mode)) {
		Tcl_DStringAppend(dirPtr, "/", 1);
		result = TclDoGlob(interp, separators, dirPtr, tail);
		if (result != TCL_OK) {
		    break;
		}
	    }
	}
    }
    *patternEnd = savedChar;

    closedir(d);
    return result;
}
