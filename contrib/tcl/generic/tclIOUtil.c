/* 
 * tclIOUtil.c --
 *
 *	This file contains a collection of utility procedures that
 *	are shared by the platform specific IO drivers.
 *
 *	Parts of this file are based on code contributed by Karl
 *	Lehenbauer, Mark Diekhans and Peter da Silva.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclIOUtil.c 1.122 96/04/02 18:46:40
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * A linked list of the following structures is used to keep track
 * of child processes that have been detached but haven't exited
 * yet, so we can make sure that they're properly "reaped" (officially
 * waited for) and don't lie around as zombies cluttering the
 * system.
 */

typedef struct Detached {
    int pid;				/* Id of process that's been detached
					 * but isn't known to have exited. */
    struct Detached *nextPtr;		/* Next in list of all detached
					 * processes. */
} Detached;

static Detached *detList = NULL;	/* List of all detached proceses. */

/*
 * Declarations for local procedures defined in this file:
 */

static Tcl_File	FileForRedirect _ANSI_ARGS_((Tcl_Interp *interp,
	                    char *spec, int atOk, char *arg, int flags,
	                    char *nextArg, int *skipPtr, int *closePtr));

/*
 *----------------------------------------------------------------------
 *
 * FileForRedirect --
 *
 *	This procedure does much of the work of parsing redirection
 *	operators.  It handles "@" if specified and allowed, and a file
 *	name, and opens the file if necessary.
 *
 * Results:
 *	The return value is the descriptor number for the file.  If an
 *	error occurs then NULL is returned and an error message is left
 *	in interp->result.  Several arguments are side-effected; see
 *	the argument list below for details.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_File
FileForRedirect(interp, spec, atOk, arg, flags, nextArg, skipPtr, closePtr)
    Tcl_Interp *interp;			/* Intepreter to use for error
					 * reporting. */
    register char *spec;			/* Points to character just after
					 * redirection character. */
    int atOk;				/* Non-zero means '@' notation is
					 * OK, zero means it isn't. */
    char *arg;				/* Pointer to entire argument
					 * containing spec:  used for error
					 * reporting. */
    int flags;				/* Flags to use for opening file. */
    char *nextArg;			/* Next argument in argc/argv
					 * array, if needed for file name.
					 * May be NULL. */
    int *skipPtr;			/* This value is incremented if
					 * nextArg is used for redirection
					 * spec. */
    int *closePtr;			/* This value is set to 1 if the file
					 * that's returned must be closed, 0
					 * if it was specified with "@" so
					 * it must be left open. */
{
    int writing = (flags & O_WRONLY);
    Tcl_Channel chan;
    Tcl_File file;

    if (atOk && (*spec == '@')) {
	spec++;
	if (*spec == 0) {
	    spec = nextArg;
	    if (spec == NULL) {
		goto badLastArg;
	    }
	    *skipPtr += 1;
	}
        chan = Tcl_GetChannel(interp, spec, NULL);
        if (chan == (Tcl_Channel) NULL) {
            return NULL;
        }
	*closePtr = 0;
        file = Tcl_GetChannelFile(chan, writing ? TCL_WRITABLE : TCL_READABLE);
        if (file == NULL) {
            Tcl_AppendResult(interp,
                    "channel \"",
                    Tcl_GetChannelName(chan),
                    "\" wasn't opened for ",
                    writing ? "writing" : "reading", (char *) NULL);
            return NULL;
        }
	if (writing) {

	    /*
	     * Be sure to flush output to the file, so that anything
	     * written by the child appears after stuff we've already
	     * written.
	     */

            Tcl_Flush(chan);
	}
    } else {
	Tcl_DString buffer;
	char *name;

	if (*spec == 0) {
	    spec = nextArg;
	    if (spec == NULL) {
		goto badLastArg;
	    }
	    *skipPtr += 1;
	}
	name = Tcl_TranslateFileName(interp, spec, &buffer);
	if (name) {
	    file = TclOpenFile(name, flags);
	} else {
	    file = NULL;
	}
	Tcl_DStringFree(&buffer);
	if (file == NULL) {
	    Tcl_AppendResult(interp, "couldn't ",
		    (writing) ? "write" : "read", " file \"", spec, "\": ",
		    Tcl_PosixError(interp), (char *) NULL);
	    return NULL;
	}
	*closePtr = 1;
    }
    return file;

    badLastArg:
    Tcl_AppendResult(interp, "can't specify \"", arg,
	    "\" as last word in command", (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetOpenMode --
 *
 * Description:
 *	Computes a POSIX mode mask for opening a file, from a given string,
 *	and also sets a flag to indicate whether the caller should seek to
 *	EOF after opening the file.
 *
 * Results:
 *	On success, returns mode to pass to "open". If an error occurs, the
 *	returns -1 and if interp is not NULL, sets interp->result to an
 *	error message.
 *
 * Side effects:
 *	Sets the integer referenced by seekFlagPtr to 1 to tell the caller
 *	to seek to EOF after opening the file.
 *
 * Special note:
 *	This code is based on a prototype implementation contributed
 *	by Mark Diekhans.
 *
 *----------------------------------------------------------------------
 */

int
TclGetOpenMode(interp, string, seekFlagPtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting - may be NULL. */
    char *string;			/* Mode string, e.g. "r+" or
					 * "RDONLY CREAT". */
    int *seekFlagPtr;			/* Set this to 1 if the caller
                                         * should seek to EOF during the
                                         * opening of the file. */
{
    int mode, modeArgc, c, i, gotRW;
    char **modeArgv, *flag;
#define RW_MODES (O_RDONLY|O_WRONLY|O_RDWR)

    /*
     * Check for the simpler fopen-like access modes (e.g. "r").  They
     * are distinguished from the POSIX access modes by the presence
     * of a lower-case first letter.
     */

    *seekFlagPtr = 0;
    mode = 0;
    if (islower(UCHAR(string[0]))) {
	switch (string[0]) {
	    case 'r':
		mode = O_RDONLY;
		break;
	    case 'w':
		mode = O_WRONLY|O_CREAT|O_TRUNC;
		break;
	    case 'a':
		mode = O_WRONLY|O_CREAT;
                *seekFlagPtr = 1;
		break;
	    default:
		error:
                if (interp != (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp,
                            "illegal access mode \"", string, "\"",
                            (char *) NULL);
                }
		return -1;
	}
	if (string[1] == '+') {
	    mode &= ~(O_RDONLY|O_WRONLY);
	    mode |= O_RDWR;
	    if (string[2] != 0) {
		goto error;
	    }
	} else if (string[1] != 0) {
	    goto error;
	}
        return mode;
    }

    /*
     * The access modes are specified using a list of POSIX modes
     * such as O_CREAT.
     *
     * IMPORTANT NOTE: We rely on Tcl_SplitList working correctly when
     * a NULL interpreter is passed in.
     */

    if (Tcl_SplitList(interp, string, &modeArgc, &modeArgv) != TCL_OK) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AddErrorInfo(interp,
                    "\n    while processing open access modes \"");
            Tcl_AddErrorInfo(interp, string);
            Tcl_AddErrorInfo(interp, "\"");
        }
        return -1;
    }
    
    gotRW = 0;
    for (i = 0; i < modeArgc; i++) {
	flag = modeArgv[i];
	c = flag[0];
	if ((c == 'R') && (strcmp(flag, "RDONLY") == 0)) {
	    mode = (mode & ~RW_MODES) | O_RDONLY;
	    gotRW = 1;
	} else if ((c == 'W') && (strcmp(flag, "WRONLY") == 0)) {
	    mode = (mode & ~RW_MODES) | O_WRONLY;
	    gotRW = 1;
	} else if ((c == 'R') && (strcmp(flag, "RDWR") == 0)) {
	    mode = (mode & ~RW_MODES) | O_RDWR;
	    gotRW = 1;
	} else if ((c == 'A') && (strcmp(flag, "APPEND") == 0)) {
	    mode |= O_APPEND;
            *seekFlagPtr = 1;
	} else if ((c == 'C') && (strcmp(flag, "CREAT") == 0)) {
	    mode |= O_CREAT;
	} else if ((c == 'E') && (strcmp(flag, "EXCL") == 0)) {
	    mode |= O_EXCL;
	} else if ((c == 'N') && (strcmp(flag, "NOCTTY") == 0)) {
#ifdef O_NOCTTY
	    mode |= O_NOCTTY;
#else
	    if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "access mode \"", flag,
                        "\" not supported by this system", (char *) NULL);
            }
            ckfree((char *) modeArgv);
	    return -1;
#endif
	} else if ((c == 'N') && (strcmp(flag, "NONBLOCK") == 0)) {
#if defined(O_NDELAY) || defined(O_NONBLOCK)
#   ifdef O_NONBLOCK
	    mode |= O_NONBLOCK;
#   else
	    mode |= O_NDELAY;
#   endif
#else
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "access mode \"", flag,
                        "\" not supported by this system", (char *) NULL);
            }
            ckfree((char *) modeArgv);
	    return -1;
#endif
	} else if ((c == 'T') && (strcmp(flag, "TRUNC") == 0)) {
	    mode |= O_TRUNC;
	} else {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "invalid access mode \"", flag,
                        "\": must be RDONLY, WRONLY, RDWR, APPEND, CREAT",
                        " EXCL, NOCTTY, NONBLOCK, or TRUNC", (char *) NULL);
            }
	    ckfree((char *) modeArgv);
	    return -1;
	}
    }
    ckfree((char *) modeArgv);
    if (!gotRW) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "access mode must include either",
                    " RDONLY, WRONLY, or RDWR", (char *) NULL);
        }
	return -1;
    }
    return mode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EvalFile --
 *
 *	Read in a file and process the entire file as one gigantic
 *	Tcl command.
 *
 * Results:
 *	A standard Tcl result, which is either the result of executing
 *	the file or an error indicating why the file couldn't be read.
 *
 * Side effects:
 *	Depends on the commands in the file.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_EvalFile(interp, fileName)
    Tcl_Interp *interp;		/* Interpreter in which to process file. */
    char *fileName;		/* Name of file to process.  Tilde-substitution
				 * will be performed on this name. */
{
    int result;
    struct stat statBuf;
    char *cmdBuffer = (char *) NULL;
    char *oldScriptFile = (char *) NULL;
    Interp *iPtr = (Interp *) interp;
    Tcl_DString buffer;
    char *nativeName = (char *) NULL;
    Tcl_Channel chan = (Tcl_Channel) NULL;

    Tcl_ResetResult(interp);
    oldScriptFile = iPtr->scriptFile;
    iPtr->scriptFile = fileName;
    Tcl_DStringInit(&buffer);
    nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (nativeName == NULL) {
	goto error;
    }

    /*
     * If Tcl_TranslateFileName didn't already copy the file name, do it
     * here.  This way we don't depend on fileName staying constant
     * throughout the execution of the script (e.g., what if it happens
     * to point to a Tcl variable that the script could change?).
     */

    if (nativeName != Tcl_DStringValue(&buffer)) {
	Tcl_DStringSetLength(&buffer, 0);
	Tcl_DStringAppend(&buffer, nativeName, -1);
	nativeName = Tcl_DStringValue(&buffer);
    }
    if (stat(nativeName, &statBuf) == -1) {
        Tcl_SetErrno(errno);
	Tcl_AppendResult(interp, "couldn't read file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    chan = Tcl_OpenFileChannel(interp, nativeName, "r", 0644);
    if (chan == (Tcl_Channel) NULL) {
        Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "couldn't read file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    cmdBuffer = (char *) ckalloc((unsigned) statBuf.st_size+1);
    result = Tcl_Read(chan, cmdBuffer, statBuf.st_size);
    if (result < 0) {
        Tcl_Close(interp, chan);
	Tcl_AppendResult(interp, "couldn't read file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    cmdBuffer[result] = 0;
    if (Tcl_Close(interp, chan) != TCL_OK) {
        goto error;
    }

    result = Tcl_Eval(interp, cmdBuffer);
    if (result == TCL_RETURN) {
	result = TclUpdateReturnInfo(iPtr);
    } else if (result == TCL_ERROR) {
	char msg[200];

	/*
	 * Record information telling where the error occurred.
	 */

	sprintf(msg, "\n    (file \"%.150s\" line %d)", fileName,
		interp->errorLine);
	Tcl_AddErrorInfo(interp, msg);
    }
    iPtr->scriptFile = oldScriptFile;
    ckfree(cmdBuffer);
    Tcl_DStringFree(&buffer);
    return result;

error:
    if (cmdBuffer != (char *) NULL) {
        ckfree(cmdBuffer);
    }
    iPtr->scriptFile = oldScriptFile;
    Tcl_DStringFree(&buffer);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DetachPids --
 *
 *	This procedure is called to indicate that one or more child
 *	processes have been placed in background and will never be
 *	waited for;  they should eventually be reaped by
 *	Tcl_ReapDetachedProcs.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DetachPids(numPids, pidPtr)
    int numPids;		/* Number of pids to detach:  gives size
				 * of array pointed to by pidPtr. */
    int *pidPtr;		/* Array of pids to detach. */
{
    register Detached *detPtr;
    int i;

    for (i = 0; i < numPids; i++) {
	detPtr = (Detached *) ckalloc(sizeof(Detached));
	detPtr->pid = pidPtr[i];
	detPtr->nextPtr = detList;
	detList = detPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ReapDetachedProcs --
 *
 *	This procedure checks to see if any detached processes have
 *	exited and, if so, it "reaps" them by officially waiting on
 *	them.  It should be called "occasionally" to make sure that
 *	all detached processes are eventually reaped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Processes are waited on, so that they can be reaped by the
 *	system.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ReapDetachedProcs()
{
    register Detached *detPtr;
    Detached *nextPtr, *prevPtr;
    int status;
    pid_t pid;

    for (detPtr = detList, prevPtr = NULL; detPtr != NULL; ) {
	pid = Tcl_WaitPid(detPtr->pid, &status, WNOHANG);
	if ((pid == 0) || ((pid == -1) && (errno != ECHILD))) {
	    prevPtr = detPtr;
	    detPtr = detPtr->nextPtr;
	    continue;
	}
	nextPtr = detPtr->nextPtr;
	if (prevPtr == NULL) {
	    detList = detPtr->nextPtr;
	} else {
	    prevPtr->nextPtr = detPtr->nextPtr;
	}
	ckfree((char *) detPtr);
	detPtr = nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclCleanupChildren --
 *
 *	This is a utility procedure used to wait for child processes
 *	to exit, record information about abnormal exits, and then
 *	collect any stderr output generated by them.
 *
 * Results:
 *	The return value is a standard Tcl result.  If anything at
 *	weird happened with the child processes, TCL_ERROR is returned
 *	and a message is left in interp->result.
 *
 * Side effects:
 *	If the last character of interp->result is a newline, then it
 *	is removed unless keepNewline is non-zero.  File errorId gets
 *	closed, and pidPtr is freed back to the storage allocator.
 *
 *----------------------------------------------------------------------
 */

int
TclCleanupChildren(interp, numPids, pidPtr, errorChan)
    Tcl_Interp *interp;		/* Used for error messages. */
    int numPids;		/* Number of entries in pidPtr array. */
    int *pidPtr;		/* Array of process ids of children. */
    Tcl_Channel errorChan;	/* Channel for file containing stderr output
				 * from pipeline.  NULL means there isn't any
				 * stderr output. */
{
    int result = TCL_OK;
    int i, pid, abnormalExit, anyErrorInfo;
    WAIT_STATUS_TYPE waitStatus;
    char *msg;

    abnormalExit = 0;
    for (i = 0; i < numPids; i++) {
        pid = (int) Tcl_WaitPid(pidPtr[i], (int *) &waitStatus, 0);
	if (pid == -1) {
	    result = TCL_ERROR;
            if (interp != (Tcl_Interp *) NULL) {
                msg = Tcl_PosixError(interp);
                if (errno == ECHILD) {
		    /*
                     * This changeup in message suggested by Mark Diekhans
                     * to remind people that ECHILD errors can occur on
                     * some systems if SIGCHLD isn't in its default state.
                     */

                    msg =
                        "child process lost (is SIGCHLD ignored or trapped?)";
                }
                Tcl_AppendResult(interp, "error waiting for process to exit: ",
                        msg, (char *) NULL);
            }
	    continue;
	}

	/*
	 * Create error messages for unusual process exits.  An
	 * extra newline gets appended to each error message, but
	 * it gets removed below (in the same fashion that an
	 * extra newline in the command's output is removed).
	 */

	if (!WIFEXITED(waitStatus) || (WEXITSTATUS(waitStatus) != 0)) {
	    char msg1[20], msg2[20];

	    result = TCL_ERROR;
	    sprintf(msg1, "%d", pid);
	    if (WIFEXITED(waitStatus)) {
                if (interp != (Tcl_Interp *) NULL) {
                    sprintf(msg2, "%d", WEXITSTATUS(waitStatus));
                    Tcl_SetErrorCode(interp, "CHILDSTATUS", msg1, msg2,
                            (char *) NULL);
                }
		abnormalExit = 1;
	    } else if (WIFSIGNALED(waitStatus)) {
                if (interp != (Tcl_Interp *) NULL) {
                    char *p;
                    
                    p = Tcl_SignalMsg((int) (WTERMSIG(waitStatus)));
                    Tcl_SetErrorCode(interp, "CHILDKILLED", msg1,
                            Tcl_SignalId((int) (WTERMSIG(waitStatus))), p,
                            (char *) NULL);
                    Tcl_AppendResult(interp, "child killed: ", p, "\n",
                            (char *) NULL);
                }
	    } else if (WIFSTOPPED(waitStatus)) {
                if (interp != (Tcl_Interp *) NULL) {
                    char *p;

                    p = Tcl_SignalMsg((int) (WSTOPSIG(waitStatus)));
                    Tcl_SetErrorCode(interp, "CHILDSUSP", msg1,
                            Tcl_SignalId((int) (WSTOPSIG(waitStatus))),
                            p, (char *) NULL);
                    Tcl_AppendResult(interp, "child suspended: ", p, "\n",
                            (char *) NULL);
                }
	    } else {
                if (interp != (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp,
                            "child wait status didn't make sense\n",
                            (char *) NULL);
                }
	    }
	}
    }

    /*
     * Read the standard error file.  If there's anything there,
     * then return an error and add the file's contents to the result
     * string.
     */

    anyErrorInfo = 0;
    if (errorChan != NULL) {

	/*
	 * Make sure we start at the beginning of the file.
	 */

	Tcl_Seek(errorChan, 0L, SEEK_SET);

        if (interp != (Tcl_Interp *) NULL) {
            while (1) {
#define BUFFER_SIZE 1000
                char buffer[BUFFER_SIZE+1];
                int count;
    
                count = Tcl_Read(errorChan, buffer, BUFFER_SIZE);
                if (count == 0) {
                    break;
                }
                result = TCL_ERROR;
                if (count < 0) {
                    Tcl_AppendResult(interp,
                            "error reading stderr output file: ",
                            Tcl_PosixError(interp), (char *) NULL);
                    break;	/* out of the "while (1)" loop. */
                }
                buffer[count] = 0;
                Tcl_AppendResult(interp, buffer, (char *) NULL);
                anyErrorInfo = 1;
            }
        }
        
	Tcl_Close(NULL, errorChan);
    }

    /*
     * If a child exited abnormally but didn't output any error information
     * at all, generate an error message here.
     */

    if (abnormalExit && !anyErrorInfo && (interp != (Tcl_Interp *) NULL)) {
	Tcl_AppendResult(interp, "child process exited abnormally",
		(char *) NULL);
    }
    
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreatePipeline --
 *
 *	Given an argc/argv array, instantiate a pipeline of processes
 *	as described by the argv.
 *
 * Results:
 *	The return value is a count of the number of new processes
 *	created, or -1 if an error occurred while creating the pipeline.
 *	*pidArrayPtr is filled in with the address of a dynamically
 *	allocated array giving the ids of all of the processes.  It
 *	is up to the caller to free this array when it isn't needed
 *	anymore.  If inPipePtr is non-NULL, *inPipePtr is filled in
 *	with the file id for the input pipe for the pipeline (if any):
 *	the caller must eventually close this file.  If outPipePtr
 *	isn't NULL, then *outPipePtr is filled in with the file id
 *	for the output pipe from the pipeline:  the caller must close
 *	this file.  If errFilePtr isn't NULL, then *errFilePtr is filled
 *	with a file id that may be used to read error output after the
 *	pipeline completes.
 *
 * Side effects:
 *	Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */

int
TclCreatePipeline(interp, argc, argv, pidArrayPtr, inPipePtr,
	outPipePtr, errFilePtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Array of strings describing commands in
				 * pipeline plus I/O redirection with <,
				 * <<,  >, etc.  Argv[argc] must be NULL. */
    int **pidArrayPtr;		/* Word at *pidArrayPtr gets filled in with
				 * address of array of pids for processes
				 * in pipeline (first pid is first process
				 * in pipeline). */
    Tcl_File *inPipePtr;	/* If non-NULL, input to the pipeline comes
				 * from a pipe (unless overridden by
				 * redirection in the command).  The file
				 * id with which to write to this pipe is
				 * stored at *inPipePtr.  NULL means command
				 * specified its own input source. */
    Tcl_File *outPipePtr;	/* If non-NULL, output to the pipeline goes
				 * to a pipe, unless overriden by redirection
				 * in the command.  The file id with which to
				 * read frome this pipe is stored at
				 * *outPipePtr.  NULL means command specified
				 * its own output sink. */
    Tcl_File *errFilePtr;	/* If non-NULL, all stderr output from the
				 * pipeline will go to a temporary file
				 * created here, and a descriptor to read
				 * the file will be left at *errFilePtr.
				 * The file will be removed already, so
				 * closing this descriptor will be the end
				 * of the file.  If this is NULL, then
				 * all stderr output goes to our stderr.
				 * If the pipeline specifies redirection
				 * then the file will still be created
				 * but it will never get any data. */
{
#if defined( MAC_TCL )
    Tcl_AppendResult(interp,
	    "command pipelines not supported on Macintosh OS", NULL);
    return -1;
#else /* !MAC_TCL */
    int *pidPtr = NULL;		/* Points to malloc-ed array holding all
				 * the pids of child processes. */
    int numPids = 0;		/* Actual number of processes that exist
				 * at *pidPtr right now. */
    int cmdCount;		/* Count of number of distinct commands
				 * found in argc/argv. */
    char *input = NULL;		/* If non-null, then this points to a
				 * string containing input data (specified
				 * via <<) to be piped to the first process
				 * in the pipeline. */
    Tcl_File inputFile = NULL;
				/* If != NULL, gives file to use as input for
				 * first process in pipeline (specified via <
				 * or <@). */
    int closeInput = 0;		/* If non-zero, then must close inputId
				 * when cleaning up (zero means the file needs
				 * to stay open for some other reason). */
    Tcl_File outputFile = NULL;
				/* Writable file for output from last command
				 * in pipeline (could be file or pipe).  NULL
				 * means use stdout. */
    int closeOutput = 0;	/* Non-zero means must close outputId when
				 * cleaning up (similar to closeInput). */
    Tcl_File errorFile = NULL;
				/* Writable file for error output from all
				 * commands in pipeline.  NULL means use
				 * stderr. */
    int closeError = 0;		/* Non-zero means must close errorId when
				 * cleaning up. */
    int skip;			/* Number of arguments to skip (because they
				 * specify redirection). */
    int lastBar;
    int i, j;
    char *p;
    int hasPipes = TclHasPipes();
    char finalOut[L_tmpnam];
    char intIn[L_tmpnam];

    finalOut[0]  = '\0';
    intIn[0] = '\0';
    
    if (inPipePtr != NULL) {
	*inPipePtr = NULL;
    }
    if (outPipePtr != NULL) {
	*outPipePtr = NULL;
    }
    if (errFilePtr != NULL) {
	*errFilePtr = NULL;
    }

    /*
     * First, scan through all the arguments to figure out the structure
     * of the pipeline.  Process all of the input and output redirection
     * arguments and remove them from the argument list in the pipeline.
     * Count the number of distinct processes (it's the number of "|"
     * arguments plus one) but don't remove the "|" arguments.
     */

    cmdCount = 1;
    lastBar = -1;
    for (i = 0; i < argc; i++) {
	if ((argv[i][0] == '|') && (((argv[i][1] == 0))
		|| ((argv[i][1] == '&') && (argv[i][2] == 0)))) {
	    if ((i == (lastBar+1)) || (i == (argc-1))) {
		interp->result = "illegal use of | or |& in command";
		return -1;
	    }
	    lastBar = i;
	    cmdCount++;
	    continue;
	} else if (argv[i][0] == '<') {
	    if ((inputFile != NULL) && closeInput) {
		TclCloseFile(inputFile);
	    }
	    inputFile = NULL;
	    skip = 1;
	    if (argv[i][1] == '<') {
		input = argv[i]+2;
		if (*input == 0) {
		    input = argv[i+1];
		    if (input == 0) {
			Tcl_AppendResult(interp, "can't specify \"", argv[i],
				"\" as last word in command", (char *) NULL);
			goto error;
		    }
		    skip = 2;
		}
	    } else {
		input = 0;
		inputFile = FileForRedirect(interp, argv[i]+1, 1, argv[i],
			O_RDONLY, argv[i+1], &skip, &closeInput);
		if (inputFile == NULL) {
		    goto error;
		}

		/* When Win32s dies out, this code can be removed */
		if (!hasPipes) {
		    if (!closeInput) {
			Tcl_AppendResult(interp, "redirection with '@'",
				" notation is not supported on this system",
				(char *) NULL);
			goto error;
		    }
		    strcpy(intIn, skip == 1 ? argv[i]+1 : argv[i+1]);
		}
	    }
	} else if (argv[i][0] == '>') {
	    int append, useForStdErr, useForStdOut, mustClose, atOk, flags;
	    Tcl_File file;

	    skip = atOk = 1;
	    append = useForStdErr = 0;
	    useForStdOut = 1;
	    if (argv[i][1] == '>') {
		p = argv[i] + 2;
		append = 1;
		atOk = 0;
		flags = O_WRONLY|O_CREAT;
	    } else {
		p = argv[i] + 1;
		flags = O_WRONLY|O_CREAT|O_TRUNC;
	    }
	    if (*p == '&') {
		useForStdErr = 1;
		p++;
	    }
	    file = FileForRedirect(interp, p, atOk, argv[i], flags, argv[i+1],
		    &skip, &mustClose);
	    if (file == NULL) {
		goto error;
	    }

	    /* When Win32s dies out, this code can be removed */
	    if (!hasPipes) {
		if (!mustClose) {
		    Tcl_AppendResult(interp, "redirection with '@'",
			    " notation is not supported on this system",
			    (char *) NULL);
		    goto error;
		}
		strcpy(finalOut, skip == 1 ? p : argv[i+1]);
	    }

	    if (hasPipes && append) {
		TclSeekFile(file, 0L, 2);
	    }

	    /*
	     * Got the file descriptor.  Now use it for standard output,
	     * standard error, or both, depending on the redirection.
	     */

	    if (useForStdOut) {
		if ((outputFile != NULL) && closeOutput) {
		    TclCloseFile(outputFile);
		}
		outputFile = file;
		closeOutput = mustClose;
	    }
	    if (useForStdErr) {
		if ((errorFile != NULL) && closeError) {
		    TclCloseFile(errorFile);
		}
		errorFile = file;
		closeError = (useForStdOut) ? 0 : mustClose;
	    }
	} else if ((argv[i][0] == '2') && (argv[i][1] == '>')) {
	    int append, atOk, flags;

	    if ((errorFile != NULL) && closeError) {
		TclCloseFile(errorFile);
	    }
	    skip = 1;
	    p = argv[i] + 2;
	    if (*p == '>') {
		p++;
		append = 1;
		atOk = 0;
		flags = O_WRONLY|O_CREAT;
	    } else {
		append = 0;
		atOk = 1;
		flags = O_WRONLY|O_CREAT|O_TRUNC;
	    }
	    errorFile = FileForRedirect(interp, p, atOk, argv[i], flags,
		    argv[i+1], &skip, &closeError);
	    if (errorFile == NULL) {
		goto error;
	    }
	    if (hasPipes && append) {
		TclSeekFile(errorFile, 0L, 2);
	    }
	} else {
	    continue;
	}
	for (j = i+skip; j < argc; j++) {
	    argv[j-skip] = argv[j];
	}
	argc -= skip;
	i -= 1;			/* Process next arg from same position. */
    }
    if (argc == 0) {
	interp->result =  "didn't specify command to execute";
	return -1;
    }

    if ((hasPipes && inputFile == NULL) || (!hasPipes && intIn[0] == '\0')) {
	if (input != NULL) {

	    /*
	     * The input for the first process is immediate data coming from
	     * Tcl.  Create a temporary file for it and put the data into the
	     * file.
	     */
	    
	    inputFile = TclCreateTempFile(input);
	    closeInput = 1;
	    if (inputFile == NULL) {
		Tcl_AppendResult(interp,
			"couldn't create input file for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	} else if (inPipePtr != NULL) {
	    Tcl_File inPipe, outPipe;
	    /*
	     * The input for the first process in the pipeline is to
	     * come from a pipe that can be written from this end.
	     */

	    if (!hasPipes || TclCreatePipe(&inPipe, &outPipe) == 0) {
		Tcl_AppendResult(interp,
			"couldn't create input pipe for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	    inputFile = inPipe;
	    closeInput = 1;
	    *inPipePtr = outPipe;
	}
    }

    /*
     * Set up a pipe to receive output from the pipeline, if no other
     * output sink has been specified.
     */

    if ((outputFile == NULL) && (outPipePtr != NULL)) {
	if (!hasPipes) {
	    tmpnam(finalOut);
	} else {
	    Tcl_File inPipe, outPipe;
	    if (TclCreatePipe(&inPipe, &outPipe) == 0) {
		Tcl_AppendResult(interp,
			"couldn't create output pipe for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	    outputFile = outPipe;
	    closeOutput = 1;
	    *outPipePtr = inPipe;
	}
    }

    /*
     * Set up the standard error output sink for the pipeline, if
     * requested.  Use a temporary file which is opened, then deleted.
     * Could potentially just use pipe, but if it filled up it could
     * cause the pipeline to deadlock:  we'd be waiting for processes
     * to complete before reading stderr, and processes couldn't complete
     * because stderr was backed up.
     */

    if (errFilePtr && !errorFile) {
	*errFilePtr = TclCreateTempFile(NULL);
	if (*errFilePtr == NULL) {
	    Tcl_AppendResult(interp,
		    "couldn't create error file for command: ",
		    Tcl_PosixError(interp), (char *) NULL);
	    goto error;
	}
	errorFile = *errFilePtr;
	closeError = 0;
    }
	
    /*
     * Scan through the argc array, forking off a process for each
     * group of arguments between "|" arguments.
     */

    pidPtr = (int *) ckalloc((unsigned) (cmdCount * sizeof(int)));
    Tcl_ReapDetachedProcs();

    if (TclSpawnPipeline(interp, pidPtr, &numPids, argc, argv, 
	    inputFile, outputFile, errorFile, intIn, finalOut) == 0) {
	goto error;
    }
    *pidArrayPtr = pidPtr;

    /*
     * All done.  Cleanup open files lying around and then return.
     */

cleanup:
    if ((inputFile != NULL) && closeInput) {
	TclCloseFile(inputFile);
    }
    if ((outputFile != NULL) && closeOutput) {
	TclCloseFile(outputFile);
    }
    if ((errorFile != NULL) && closeError) {
	TclCloseFile(errorFile);
    }
    return numPids;

    /*
     * An error occurred.  There could have been extra files open, such
     * as pipes between children.  Clean them all up.  Detach any child
     * processes that have been created.
     */

error:
    if ((inPipePtr != NULL) && (*inPipePtr != NULL)) {
	TclCloseFile(*inPipePtr);
	*inPipePtr = NULL;
    }
    if ((outPipePtr != NULL) && (*outPipePtr != NULL)) {
	TclCloseFile(*outPipePtr);
	*outPipePtr = NULL;
    }
    if ((errFilePtr != NULL) && (*errFilePtr != NULL)) {
	TclCloseFile(*errFilePtr);
	*errFilePtr = NULL;
    }
    if (pidPtr != NULL) {
	for (i = 0; i < numPids; i++) {
	    if (pidPtr[i] != -1) {
		Tcl_DetachPids(1, &pidPtr[i]);
	    }
	}
	ckfree((char *) pidPtr);
    }
    numPids = -1;
    goto cleanup;
#endif /* !MAC_TCL */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetErrno --
 *
 *	Gets the current value of the Tcl error code variable. This is
 *	currently the global variable "errno" but could in the future
 *	change to something else.
 *
 * Results:
 *	The value of the Tcl error code variable.
 *
 * Side effects:
 *	None. Note that the value of the Tcl error code variable is
 *	UNDEFINED if a call to Tcl_SetErrno did not precede this call.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetErrno()
{
    return errno;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetErrno --
 *
 *	Sets the Tcl error code variable to the supplied value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the value of the Tcl error code variable.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetErrno(err)
    int err;			/* The new value. */
{
    errno = err;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PosixError --
 *
 *	This procedure is typically called after UNIX kernel calls
 *	return errors.  It stores machine-readable information about
 *	the error in $errorCode returns an information string for
 *	the caller's use.
 *
 * Results:
 *	The return value is a human-readable string describing the
 *	error.
 *
 * Side effects:
 *	The global variable $errorCode is reset.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_PosixError(interp)
    Tcl_Interp *interp;		/* Interpreter whose $errorCode variable
				 * is to be changed. */
{
    char *id, *msg;

    msg = Tcl_ErrnoMsg(errno);
    id = Tcl_ErrnoId();
    Tcl_SetErrorCode(interp, "POSIX", id, msg, (char *) NULL);
    return msg;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenCommandChannel --
 *
 *	Opens an I/O channel to one or more subprocesses specified
 *	by argc and argv.  The flags argument determines the
 *	disposition of the stdio handles.  If the TCL_STDIN flag is
 *	set then the standard input for the first subprocess will
 *	be tied to the channel:  writing to the channel will provide
 *	input to the subprocess.  If TCL_STDIN is not set, then
 *	standard input for the first subprocess will be the same as
 *	this application's standard input.  If TCL_STDOUT is set then
 *	standard output from the last subprocess can be read from the
 *	channel;  otherwise it goes to this application's standard
 *	output.  If TCL_STDERR is set, standard error output for all
 *	subprocesses is returned to the channel and results in an error
 *	when the channel is closed;  otherwise it goes to this
 *	application's standard error.  If TCL_ENFORCE_MODE is not set,
 *	then argc and argv can redirect the stdio handles to override
 *	TCL_STDIN, TCL_STDOUT, and TCL_STDERR;  if it is set, then it 
 *	is an error for argc and argv to override stdio channels for
 *	which TCL_STDIN, TCL_STDOUT, and TCL_STDERR have been set.
 *
 * Results:
 *	A new command channel, or NULL on failure with an error
 *	message left in interp.
 *
 * Side effects:
 *	Creates processes, opens pipes.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenCommandChannel(interp, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. Can
                                 * NOT be NULL. */
    int argc;			/* How many arguments. */
    char **argv;		/* Array of arguments for command pipe. */
    int flags;			/* Or'ed combination of TCL_STDIN, TCL_STDOUT,
				 * TCL_STDERR, and TCL_ENFORCE_MODE. */
{
    Tcl_File *inPipePtr, *outPipePtr, *errFilePtr;
    Tcl_File inPipe, outPipe, errFile;
    int numPids, *pidPtr;
    Tcl_Channel channel;

    inPipe = outPipe = errFile = NULL;

    inPipePtr = (flags & TCL_STDIN) ? &inPipe : NULL;
    outPipePtr = (flags & TCL_STDOUT) ? &outPipe : NULL;
    errFilePtr = (flags & TCL_STDERR) ? &errFile : NULL;
    
    numPids = TclCreatePipeline(interp, argc, argv, &pidPtr, inPipePtr,
            outPipePtr, errFilePtr);

    if (numPids < 0) {
	goto error;
    }

    /*
     * Verify that the pipes that were created satisfy the
     * readable/writable constraints. 
     */

    if (flags & TCL_ENFORCE_MODE) {
	if ((flags & TCL_STDOUT) && (outPipe == NULL)) {
	    Tcl_AppendResult(interp, "can't read output from command:",
		    " standard output was redirected", (char *) NULL);
	    goto error;
	}
	if ((flags & TCL_STDIN) && (inPipe == NULL)) {
	    Tcl_AppendResult(interp, "can't write input to command:",
		    " standard input was redirected", (char *) NULL);
	    goto error;
	}
    }
    
    channel = TclCreateCommandChannel(outPipe, inPipe, errFile,
	    numPids, pidPtr);

    if (channel == (Tcl_Channel) NULL) {
        Tcl_AppendResult(interp, "pipe for command could not be created",
                (char *) NULL);
	goto error;
    }
    return channel;

error:
    if (numPids > 0) {
	Tcl_DetachPids(numPids, pidPtr);
	ckfree((char *) pidPtr);
    }
    if (inPipe != NULL) {
	TclClosePipeFile(inPipe);
    }
    if (outPipe != NULL) {
	TclClosePipeFile(outPipe);
    }
    if (errFile != NULL) {
	TclClosePipeFile(errFile);
    }
    return NULL;
}
