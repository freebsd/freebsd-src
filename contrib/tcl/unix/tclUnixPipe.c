/* 
 * tclUnixPipe.c -- This file implements the UNIX-specific exec pipeline 
 *                  functions.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixPipe.c 1.29 96/04/18 15:56:26
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Declarations for local procedures defined in this file:
 */

static void             RestoreSignals _ANSI_ARGS_((void));
static int		SetupStdFile _ANSI_ARGS_((Tcl_File file, int type));

/*
 *----------------------------------------------------------------------
 *
 * RestoreSignals --
 *
 *      This procedure is invoked in a forked child process just before
 *      exec-ing a new program to restore all signals to their default
 *      settings.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Signal settings get changed.
 *
 *----------------------------------------------------------------------
 */
 
static void
RestoreSignals()
{
#ifdef SIGABRT
    signal(SIGABRT, SIG_DFL);
#endif
#ifdef SIGALRM
    signal(SIGALRM, SIG_DFL);
#endif
#ifdef SIGFPE
    signal(SIGFPE, SIG_DFL);
#endif
#ifdef SIGHUP
    signal(SIGHUP, SIG_DFL);
#endif
#ifdef SIGILL
    signal(SIGILL, SIG_DFL);
#endif
#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_DFL);
#endif
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_DFL);
#endif
#ifdef SIGSEGV
    signal(SIGSEGV, SIG_DFL);
#endif
#ifdef SIGTERM
    signal(SIGTERM, SIG_DFL);
#endif
#ifdef SIGUSR1
    signal(SIGUSR1, SIG_DFL);
#endif
#ifdef SIGUSR2
    signal(SIGUSR2, SIG_DFL);
#endif
#ifdef SIGCHLD
    signal(SIGCHLD, SIG_DFL);
#endif
#ifdef SIGCONT
    signal(SIGCONT, SIG_DFL);
#endif
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif
#ifdef SIGTTIN
    signal(SIGTTIN, SIG_DFL);
#endif
#ifdef SIGTTOU
    signal(SIGTTOU, SIG_DFL);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * SetupStdFile --
 *
 *	Set up stdio file handles for the child process, using the
 *	current standard channels if no other files are specified.
 *	If no standard channel is defined, or if no file is associated
 *	with the channel, then the corresponding standard fd is closed.
 *
 * Results:
 *	Returns 1 on success, or 0 on failure.
 *
 * Side effects:
 *	Replaces stdio fds.
 *
 *----------------------------------------------------------------------
 */

static int
SetupStdFile(file, type)
    Tcl_File file;		/* File to dup, or NULL. */
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR */
{
    Tcl_Channel channel;
    int fd;
    int targetFd = 0;		/* Initializations here needed only to */
    int direction = 0;		/* prevent warnings about using uninitialized
				 * variables. */

    switch (type) {
	case TCL_STDIN:
	    targetFd = 0;
	    direction = TCL_READABLE;
	    break;
	case TCL_STDOUT:
	    targetFd = 1;
	    direction = TCL_WRITABLE;
	    break;
	case TCL_STDERR:
	    targetFd = 2;
	    direction = TCL_WRITABLE;
	    break;
    }

    if (!file) {
	channel = Tcl_GetStdChannel(type);
	if (channel) {
	    file = Tcl_GetChannelFile(channel, direction);
	}
    }
    if (file) {
	fd = (int)Tcl_GetFileInfo(file, NULL);
	if (fd != targetFd) {
	    if (dup2(fd, targetFd) == -1) {
		return 0;
	    }

            /*
             * Must clear the close-on-exec flag for the target FD, since
             * some systems (e.g. Ultrix) do not clear the CLOEXEC flag on
             * the target FD.
             */
            
            fcntl(targetFd, F_SETFD, 0);
	} else {
	    int result;

	    /*
	     * Since we aren't dup'ing the file, we need to explicitly clear
	     * the close-on-exec flag.
	     */

	    result = fcntl(fd, F_SETFD, 0);
	}
    } else {
	close(targetFd);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSpawnPipeline --
 *
 *      Given an argc/argv array, instantiate a pipeline of processes
 *      as described by the argv.
 *
 * Results:
 *      The return value is 1 on success, 0 on error
 *
 * Side effects:
 *      Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */
int
TclSpawnPipeline(interp, pidPtr, numPids, argc, argv, inputFile,
	outputFile, errorFile, intIn, finalOut)
    Tcl_Interp *interp;		/* Interpreter in which to process pipeline. */
    int *pidPtr;		/* Array of pids which are created. */
    int *numPids;		/* Number of pids created. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Array of strings describing commands in
				 * pipeline plus I/O redirection with <,
				 * <<, >, etc. argv[argc] must be NULL. */
    Tcl_File inputFile;	/* If >=0, gives file id to use as input for
				 * first process in pipeline (specified via <
				 * or <@). */
    Tcl_File outputFile;	/* Writable file id for output from last
				 * command in pipeline (could be file or
				 * pipe). NULL means use stdout. */
    Tcl_File errorFile;	/* Writable file id for error output from all
				 * commands in the pipeline. NULL means use
				 * stderr */
    char *intIn;		/* File name for initial input (for Win32s). */
    char *finalOut;		/* File name for final output (for Win32s). */
{
    int firstArg, lastArg;
    int pid, count;
    Tcl_DString buffer;
    char *execName;
    char errSpace[200];
    Tcl_File pipeIn, errPipeIn, errPipeOut;
    int joinThisError;
    Tcl_File curOutFile = NULL, curInFile;
    
    Tcl_DStringInit(&buffer);
    pipeIn = errPipeIn = errPipeOut = NULL;

    curInFile = inputFile;

    for (firstArg = 0; firstArg < argc; firstArg = lastArg+1) {

	/*
	 * Convert the program name into native form.
	 */

	Tcl_DStringFree(&buffer);
	execName = Tcl_TranslateFileName(interp, argv[firstArg], &buffer);
	if (execName == NULL) {
	    goto error;
	}

	/*
	 * Find the end of the current segment of the pipeline.
	 */

	joinThisError = 0;
	for (lastArg = firstArg; lastArg < argc; lastArg++) {
	    if (argv[lastArg][0] == '|') {
		if (argv[lastArg][1] == 0) {
		    break;
		}
		if ((argv[lastArg][1] == '&') && (argv[lastArg][2] == 0)) {
		    joinThisError = 1;
		    break;
		}
	    }
	}
	argv[lastArg] = NULL;

	/*
	 * If this is the last segment, use the specified outputFile.
	 * Otherwise create an intermediate pipe.
	 */

	if (lastArg == argc) {
	    curOutFile = outputFile;
	} else {
	    if (TclCreatePipe(&pipeIn, &curOutFile) == 0) {
		Tcl_AppendResult(interp, "couldn't create pipe: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}

	/*
	 * Create a pipe that the child can use to return error
	 * information if anything goes wrong.
	 */

	if (TclCreatePipe(&errPipeIn, &errPipeOut) == 0) {
	    Tcl_AppendResult(interp, "couldn't create pipe: ",
		    Tcl_PosixError(interp), (char *) NULL);
	    goto error;
	}

	pid = vfork();
	if (pid == 0) {

	    /*
	     * Set up stdio file handles for the child process.
	     */

	    if (!SetupStdFile(curInFile, TCL_STDIN)
		    || !SetupStdFile(curOutFile, TCL_STDOUT)
		    || (!joinThisError && !SetupStdFile(errorFile, TCL_STDERR))
		    || (joinThisError &&
                            ((dup2(1,2) == -1) ||
                             (fcntl(2, F_SETFD, 0) != 0)))) {
		sprintf(errSpace,
			"%dforked process couldn't set up input/output: ",
			errno);
		TclWriteFile(errPipeOut, 1, errSpace, (int) strlen(errSpace));
		_exit(1);
	    }

            /*
             * Close the input side of the error pipe.
             */

	    RestoreSignals();
	    execvp(execName, &argv[firstArg]);
	    sprintf(errSpace, "%dcouldn't execute \"%.150s\": ", errno,
		    argv[firstArg]);
	    TclWriteFile(errPipeOut, 1, errSpace, (int) strlen(errSpace));
	    _exit(1);
	}
	Tcl_DStringFree(&buffer);
	if (pid == -1) {
	    Tcl_AppendResult(interp, "couldn't fork child process: ",
		    Tcl_PosixError(interp), (char *) NULL);
	    goto error;
	}

	/*
	 * Add the child process to the list of those to be reaped.
	 * Note: must do it now, so that the process will be reaped even if
	 * an error occurs during its startup.
	 */
	
	pidPtr[*numPids] = pid;
	(*numPids)++;
	
	/*
	 * Read back from the error pipe to see if the child startup
	 * up OK.  The info in the pipe (if any) consists of a decimal
	 * errno value followed by an error message.
	 */

	TclCloseFile(errPipeOut);
	errPipeOut = NULL;

	count = TclReadFile(errPipeIn, 1, errSpace,
		(size_t) (sizeof(errSpace) - 1));
	if (count > 0) {
	    char *end;
	    errSpace[count] = 0;
	    errno = strtol(errSpace, &end, 10);
	    Tcl_AppendResult(interp, end, Tcl_PosixError(interp),
		    (char *) NULL);
	    goto error;
	}
	TclCloseFile(errPipeIn);
	errPipeIn = NULL;

	/*
	 * Close off our copies of file descriptors that were set up for
	 * this child, then set up the input for the next child.
	 */

	if (curInFile && (curInFile != inputFile)) {
	    TclCloseFile(curInFile);
	}
	curInFile = pipeIn;
	pipeIn = NULL;

	if (curOutFile && (curOutFile != outputFile)) {
	    TclCloseFile(curOutFile);
	}
	curOutFile = NULL;
    }
    return 1;

    /*
     * An error occured, so we need to clean up any open pipes.
     */

error:
    Tcl_DStringFree(&buffer);
    if (errPipeIn) {
	TclCloseFile(errPipeIn);
    }
    if (errPipeOut) {
	TclCloseFile(errPipeOut);
    }
    if (pipeIn) {
	TclCloseFile(pipeIn);
    }
    if (curOutFile && (curOutFile != outputFile)) {
	TclCloseFile(curOutFile);
    }
    if (curInFile && (curInFile != inputFile)) {
	TclCloseFile(curInFile);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreatePipe --
 *
 *      Creates a pipe - simply calls the pipe() function.
 *
 * Results:
 *      Returns 1 on success, 0 on failure. 
 *
 * Side effects:
 *      Creates a pipe.
 *
 *----------------------------------------------------------------------
 */
int
TclCreatePipe(readPipe, writePipe)
    Tcl_File *readPipe;	/* Location to store file handle for
				 * read side of pipe. */
    Tcl_File *writePipe;	/* Location to store file handle for
				 * write side of pipe. */
{
    int pipeIds[2];

    if (pipe(pipeIds) != 0) {
	return 0;
    }

    fcntl(pipeIds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeIds[1], F_SETFD, FD_CLOEXEC);

    *readPipe = Tcl_GetFile((ClientData)pipeIds[0], TCL_UNIX_FD);
    *writePipe = Tcl_GetFile((ClientData)pipeIds[1], TCL_UNIX_FD);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreatePipeline --
 *
 *	This function is a compatibility wrapper for TclCreatePipeline.
 *	It is only available under Unix, and may be removed from later
 *	versions.
 *
 * Results:
 *	Same as TclCreatePipeline.
 *
 * Side effects:
 *	Same as TclCreatePipeline.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CreatePipeline(interp, argc, argv, pidArrayPtr, inPipePtr,
	outPipePtr, errFilePtr)
    Tcl_Interp *interp;
    int argc;
    char **argv;
    int **pidArrayPtr;
    int *inPipePtr;
    int *outPipePtr;
    int *errFilePtr;
{
    Tcl_File inFile, outFile, errFile;
    int result;

    result = TclCreatePipeline(interp, argc, argv, pidArrayPtr,
	    (inPipePtr ? &inFile : NULL),
	    (outPipePtr ? &outFile : NULL),
	    (errFilePtr ? &errFile : NULL));

    if (inPipePtr) {
	if (inFile) {
	    *inPipePtr = (int) Tcl_GetFileInfo(inFile, NULL);
	    Tcl_FreeFile(inFile);
	} else {
	    *inPipePtr = -1;
	}
    }
    if (outPipePtr) {
	if (outFile) {
	    *outPipePtr = (int) Tcl_GetFileInfo(outFile, NULL);
	    Tcl_FreeFile(outFile);
	} else {
	    *outPipePtr = -1;
	}
    }
    if (errFilePtr) {
	if (errFile) {
	    *errFilePtr = (int) Tcl_GetFileInfo(errFile, NULL);
	    Tcl_FreeFile(errFile);
	} else {
	    *errFilePtr = -1;
	}
    }
    return result;
}
