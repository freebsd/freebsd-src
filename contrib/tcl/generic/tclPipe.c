/* 
 * tclPipe.c --
 *
 *	This file contains the generic portion of the command channel
 *	driver as well as various utility routines used in managing
 *	subprocesses.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclPipe.c 1.8 97/06/20 13:26:45
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
    Tcl_Pid pid;			/* Id of process that's been detached
					 * but isn't known to have exited. */
    struct Detached *nextPtr;		/* Next in list of all detached
					 * processes. */
} Detached;

static Detached *detList = NULL;	/* List of all detached proceses. */

/*
 * Declarations for local procedures defined in this file:
 */

static TclFile	FileForRedirect _ANSI_ARGS_((Tcl_Interp *interp,
	            char *spec, int atOk, char *arg, char *nextArg, 
		    int flags, int *skipPtr, int *closePtr, int *releasePtr));

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

static TclFile
FileForRedirect(interp, spec, atOK, arg, nextArg, flags, skipPtr, closePtr,
	releasePtr)
    Tcl_Interp *interp;		/* Intepreter to use for error reporting. */
    char *spec;			/* Points to character just after
				 * redirection character. */
    char *arg;			/* Pointer to entire argument containing 
				 * spec:  used for error reporting. */
    int atOK;			/* Non-zero means that '@' notation can be 
				 * used to specify a channel, zero means that
				 * it isn't. */
    char *nextArg;		/* Next argument in argc/argv array, if needed 
				 * for file name or channel name.  May be 
				 * NULL. */
    int flags;			/* Flags to use for opening file or to 
				 * specify mode for channel. */
    int *skipPtr;		/* Filled with 1 if redirection target was
				 * in spec, 2 if it was in nextArg. */
    int *closePtr;		/* Filled with one if the caller should 
				 * close the file when done with it, zero
				 * otherwise. */
    int *releasePtr;
{
    int writing = (flags & O_WRONLY);
    Tcl_Channel chan;
    TclFile file;

    *skipPtr = 1;
    if ((atOK != 0)  && (*spec == '@')) {
	spec++;
	if (*spec == '\0') {
	    spec = nextArg;
	    if (spec == NULL) {
		goto badLastArg;
	    }
	    *skipPtr = 2;
	}
        chan = Tcl_GetChannel(interp, spec, NULL);
        if (chan == (Tcl_Channel) NULL) {
            return NULL;
        }
	file = TclpMakeFile(chan, writing ? TCL_WRITABLE : TCL_READABLE);
        if (file == NULL) {
            Tcl_AppendResult(interp, "channel \"", Tcl_GetChannelName(chan),
                    "\" wasn't opened for ",
                    ((writing) ? "writing" : "reading"), (char *) NULL);
            return NULL;
        }
	*releasePtr = 1;
	if (writing) {

	    /*
	     * Be sure to flush output to the file, so that anything
	     * written by the child appears after stuff we've already
	     * written.
	     */

            Tcl_Flush(chan);
	}
    } else {
	char *name;
	Tcl_DString nameString;

	if (*spec == '\0') {
	    spec = nextArg;
	    if (spec == NULL) {
		goto badLastArg;
	    }
	    *skipPtr = 2;
	}
	name = Tcl_TranslateFileName(interp, spec, &nameString);
	if (name != NULL) {
	    file = TclpOpenFile(name, flags);
	} else {
	    file = NULL;
	}
	Tcl_DStringFree(&nameString);
	if (file == NULL) {
	    Tcl_AppendResult(interp, "couldn't ",
		    ((writing) ? "write" : "read"), " file \"", spec, "\": ",
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
    Tcl_Pid *pidPtr;		/* Array of pids to detach. */
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
    Tcl_Pid pid;

    for (detPtr = detList, prevPtr = NULL; detPtr != NULL; ) {
	pid = Tcl_WaitPid(detPtr->pid, &status, WNOHANG);
	if ((pid == 0) || ((pid == (Tcl_Pid) -1) && (errno != ECHILD))) {
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
    Tcl_Pid *pidPtr;		/* Array of process ids of children. */
    Tcl_Channel errorChan;	/* Channel for file containing stderr output
				 * from pipeline.  NULL means there isn't any
				 * stderr output. */
{
    int result = TCL_OK;
    int i, abnormalExit, anyErrorInfo;
    Tcl_Pid pid;
    WAIT_STATUS_TYPE waitStatus;
    char *msg;

    abnormalExit = 0;
    for (i = 0; i < numPids; i++) {
        pid = Tcl_WaitPid(pidPtr[i], (int *) &waitStatus, 0);
	if (pid == (Tcl_Pid) -1) {
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
	    sprintf(msg1, "%ld", TclpGetPid(pid));
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
        
	Tcl_Close((Tcl_Interp *) NULL, errorChan);
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
 *	This procedure is unofficially exported for use by BLT.
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
    Tcl_Pid **pidArrayPtr;	/* Word at *pidArrayPtr gets filled in with
				 * address of array of pids for processes
				 * in pipeline (first pid is first process
				 * in pipeline). */
    TclFile *inPipePtr;		/* If non-NULL, input to the pipeline comes
				 * from a pipe (unless overridden by
				 * redirection in the command).  The file
				 * id with which to write to this pipe is
				 * stored at *inPipePtr.  NULL means command
				 * specified its own input source. */
    TclFile *outPipePtr;	/* If non-NULL, output to the pipeline goes
				 * to a pipe, unless overriden by redirection
				 * in the command.  The file id with which to
				 * read frome this pipe is stored at
				 * *outPipePtr.  NULL means command specified
				 * its own output sink. */
    TclFile *errFilePtr;	/* If non-NULL, all stderr output from the
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
    Tcl_Pid *pidPtr = NULL;	/* Points to malloc-ed array holding all
				 * the pids of child processes. */
    int numPids;		/* Actual number of processes that exist
				 * at *pidPtr right now. */
    int cmdCount;		/* Count of number of distinct commands
				 * found in argc/argv. */
    char *inputLiteral = NULL;	/* If non-null, then this points to a
				 * string containing input data (specified
				 * via <<) to be piped to the first process
				 * in the pipeline. */
    TclFile inputFile = NULL;	/* If != NULL, gives file to use as input for
				 * first process in pipeline (specified via <
				 * or <@). */
    int inputClose = 0;		/* If non-zero, then inputFile should be 
    				 * closed when cleaning up. */
    int inputRelease = 0;
    TclFile outputFile = NULL;	/* Writable file for output from last command
				 * in pipeline (could be file or pipe).  NULL
				 * means use stdout. */
    int outputClose = 0;	/* If non-zero, then outputFile should be 
    				 * closed when cleaning up. */
    int outputRelease = 0;
    TclFile errorFile = NULL;	/* Writable file for error output from all
				 * commands in pipeline.  NULL means use
				 * stderr. */
    int errorClose = 0;		/* If non-zero, then errorFile should be 
    				 * closed when cleaning up. */
    int errorRelease = 0;
    char *p;
    int skip, lastBar, lastArg, i, j, atOK, flags, errorToOutput;
    Tcl_DString execBuffer;
    TclFile pipeIn;
    TclFile curInFile, curOutFile, curErrFile;
    Tcl_Channel channel;

    if (inPipePtr != NULL) {
	*inPipePtr = NULL;
    }
    if (outPipePtr != NULL) {
	*outPipePtr = NULL;
    }
    if (errFilePtr != NULL) {
	*errFilePtr = NULL;
    }

    Tcl_DStringInit(&execBuffer);
    
    pipeIn = NULL;
    curInFile = NULL;
    curOutFile = NULL;
    numPids = 0;

    /*
     * First, scan through all the arguments to figure out the structure
     * of the pipeline.  Process all of the input and output redirection
     * arguments and remove them from the argument list in the pipeline.
     * Count the number of distinct processes (it's the number of "|"
     * arguments plus one) but don't remove the "|" arguments because 
     * they'll be used in the second pass to seperate the individual 
     * child processes.  Cannot start the child processes in this pass 
     * because the redirection symbols may appear anywhere in the 
     * command line -- e.g., the '<' that specifies the input to the 
     * entire pipe may appear at the very end of the argument list.
     */

    lastBar = -1;
    cmdCount = 1;
    for (i = 0; i < argc; i++) {
        skip = 0;
	p = argv[i];
	switch (*p++) {
	case '|':
	    if (*p == '&') {
		p++;
	    }
	    if (*p == '\0') {
		if ((i == (lastBar + 1)) || (i == (argc - 1))) {
		    Tcl_SetResult(interp,
			    "illegal use of | or |& in command",
			    TCL_STATIC);
		    goto error;
		}
	    }
	    lastBar = i;
	    cmdCount++;
	    break;

	case '<':
	    if (inputClose != 0) {
		inputClose = 0;
		TclpCloseFile(inputFile);
	    }
	    if (inputRelease != 0) {
		inputRelease = 0;
		TclpReleaseFile(inputFile);
	    }
	    if (*p == '<') {
		inputFile = NULL;
		inputLiteral = p + 1;
		skip = 1;
		if (*inputLiteral == '\0') {
		    inputLiteral = argv[i + 1];
		    if (inputLiteral == NULL) {
			Tcl_AppendResult(interp, "can't specify \"", argv[i],
				"\" as last word in command", (char *) NULL);
			goto error;
		    }
		    skip = 2;
		}
	    } else {
		inputLiteral = NULL;
		inputFile = FileForRedirect(interp, p, 1, argv[i], 
			argv[i + 1], O_RDONLY, &skip, &inputClose, &inputRelease);
		if (inputFile == NULL) {
		    goto error;
		}
	    }
	    break;

	case '>':
	    atOK = 1;
	    flags = O_WRONLY | O_CREAT | O_TRUNC;
	    errorToOutput = 0;
	    if (*p == '>') {
		p++;
		atOK = 0;
		flags = O_WRONLY | O_CREAT;
	    }
	    if (*p == '&') {
		if (errorClose != 0) {
		    errorClose = 0;
		    TclpCloseFile(errorFile);
		}
		errorToOutput = 1;
		p++;
	    }

	    /*
	     * Close the old output file, but only if the error file is
	     * not also using it.
	     */

	    if (outputClose != 0) {
		outputClose = 0;
		if (errorFile == outputFile) {
		    errorClose = 1;
		} else {
		    TclpCloseFile(outputFile);
		}
	    }
	    if (outputRelease != 0) {
		outputRelease = 0;
		if (errorFile == outputFile) {
		    errorRelease = 1;
		} else {
		    TclpReleaseFile(outputFile);
		}
	    }
	    outputFile = FileForRedirect(interp, p, atOK, argv[i], 
		    argv[i + 1], flags, &skip, &outputClose, &outputRelease);
	    if (outputFile == NULL) {
		goto error;
	    }
	    if (errorToOutput) {
		if (errorClose != 0) {
		    errorClose = 0;
		    TclpCloseFile(errorFile);
		}
		if (errorRelease != 0) {
		    errorRelease = 0;
		    TclpReleaseFile(errorFile);
		}
		errorFile = outputFile;
	    }
	    break;

	case '2':
	    if (*p != '>') {
		break;
	    }
	    p++;
	    atOK = 1;
	    flags = O_WRONLY | O_CREAT | O_TRUNC;
	    if (*p == '>') {
		p++;
		atOK = 0;
		flags = O_WRONLY | O_CREAT;
	    }
	    if (errorClose != 0) {
		errorClose = 0;
		TclpCloseFile(errorFile);
	    }
	    if (errorRelease != 0) {
		errorRelease = 0;
		TclpReleaseFile(errorFile);
	    }
	    errorFile = FileForRedirect(interp, p, atOK, argv[i], 
		    argv[i + 1], flags, &skip, &errorClose, &errorRelease);
	    if (errorFile == NULL) {
		goto error;
	    }
	    break;
	}

	if (skip != 0) {
	    for (j = i + skip; j < argc; j++) {
		argv[j - skip] = argv[j];
	    }
	    argc -= skip;
	    i -= 1;
	}
    }

    if (inputFile == NULL) {
	if (inputLiteral != NULL) {
	    /*
	     * The input for the first process is immediate data coming from
	     * Tcl.  Create a temporary file for it and put the data into the
	     * file.
	     */
	    inputFile = TclpCreateTempFile(inputLiteral, NULL);
	    if (inputFile == NULL) {
		Tcl_AppendResult(interp,
			"couldn't create input file for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	    inputClose = 1;
	} else if (inPipePtr != NULL) {
	    /*
	     * The input for the first process in the pipeline is to
	     * come from a pipe that can be written from by the caller.
	     */

	    if (TclpCreatePipe(&inputFile, inPipePtr) == 0) {
		Tcl_AppendResult(interp, 
			"couldn't create input pipe for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	    inputClose = 1;
	} else {
	    /*
	     * The input for the first process comes from stdin.
	     */

	    channel = Tcl_GetStdChannel(TCL_STDIN);
	    if (channel != NULL) {
		inputFile = TclpMakeFile(channel, TCL_READABLE);
		if (inputFile != NULL) {
		    inputRelease = 1;
		}
	    }
	}
    }

    if (outputFile == NULL) {
	if (outPipePtr != NULL) {
	    /*
	     * Output from the last process in the pipeline is to go to a
	     * pipe that can be read by the caller.
	     */

	    if (TclpCreatePipe(outPipePtr, &outputFile) == 0) {
		Tcl_AppendResult(interp, 
			"couldn't create output pipe for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	    outputClose = 1;
	} else {
	    /*
	     * The output for the last process goes to stdout.
	     */

	    channel = Tcl_GetStdChannel(TCL_STDOUT);
	    if (channel) {
		outputFile = TclpMakeFile(channel, TCL_WRITABLE);
		if (outputFile != NULL) {
		    outputRelease = 1;
		}
	    }
	}
    }

    if (errorFile == NULL) {
	if (errFilePtr != NULL) {
	    /*
	     * Set up the standard error output sink for the pipeline, if
	     * requested.  Use a temporary file which is opened, then deleted.
	     * Could potentially just use pipe, but if it filled up it could
	     * cause the pipeline to deadlock:  we'd be waiting for processes
	     * to complete before reading stderr, and processes couldn't 
	     * complete because stderr was backed up.
	     */

	    errorFile = TclpCreateTempFile(NULL, NULL);
	    if (errorFile == NULL) {
		Tcl_AppendResult(interp,
			"couldn't create error file for command: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	    *errFilePtr = errorFile;
	} else {
	    /*
	     * Errors from the pipeline go to stderr.
	     */

	    channel = Tcl_GetStdChannel(TCL_STDERR);
	    if (channel) {
		errorFile = TclpMakeFile(channel, TCL_WRITABLE);
		if (errorFile != NULL) {
		    errorRelease = 1;
		}
	    }
	}
    }
	
    /*
     * Scan through the argc array, creating a process for each
     * group of arguments between the "|" characters.
     */

    Tcl_ReapDetachedProcs();
    pidPtr = (Tcl_Pid *) ckalloc((unsigned) (cmdCount * sizeof(Tcl_Pid)));

    curInFile = inputFile;

    for (i = 0; i < argc; i = lastArg + 1) { 
	int joinThisError;
	Tcl_Pid pid;

	/*
	 * Convert the program name into native form. 
	 */

	argv[i] = Tcl_TranslateFileName(interp, argv[i], &execBuffer);
	if (argv[i] == NULL) {
	    goto error;
	}

	/*
	 * Find the end of the current segment of the pipeline.
	 */

	joinThisError = 0;
	for (lastArg = i; lastArg < argc; lastArg++) {
	    if (argv[lastArg][0] == '|') { 
		if (argv[lastArg][1] == '\0') { 
		    break;
		}
		if ((argv[lastArg][1] == '&') && (argv[lastArg][2] == '\0')) {
		    joinThisError = 1;
		    break;
		}
	    }
	}
	argv[lastArg] = NULL;

	/*
	 * If this is the last segment, use the specified outputFile.
	 * Otherwise create an intermediate pipe.  pipeIn will become the
	 * curInFile for the next segment of the pipe.
	 */

	if (lastArg == argc) { 
	    curOutFile = outputFile;
	} else {
	    if (TclpCreatePipe(&pipeIn, &curOutFile) == 0) {
		Tcl_AppendResult(interp, "couldn't create pipe: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}

	if (joinThisError != 0) {
	    curErrFile = curOutFile;
	} else {
	    curErrFile = errorFile;
	}

	if (TclpCreateProcess(interp, lastArg - i, argv + i,
		curInFile, curOutFile, curErrFile, &pid) != TCL_OK) {
	    goto error;
	}
	Tcl_DStringFree(&execBuffer);

	pidPtr[numPids] = pid;
	numPids++;

	/*
	 * Close off our copies of file descriptors that were set up for
	 * this child, then set up the input for the next child.
	 */

	if ((curInFile != NULL) && (curInFile != inputFile)) {
	    TclpCloseFile(curInFile);
	}
	curInFile = pipeIn;
	pipeIn = NULL;

	if ((curOutFile != NULL) && (curOutFile != outputFile)) {
	    TclpCloseFile(curOutFile);
	}
	curOutFile = NULL;
    }

    *pidArrayPtr = pidPtr;

    /*
     * All done.  Cleanup open files lying around and then return.
     */

cleanup:
    Tcl_DStringFree(&execBuffer);

    if (inputClose) {
	TclpCloseFile(inputFile);
    } else if (inputRelease) {
	TclpReleaseFile(inputFile);
    }
    if (outputClose) {
	TclpCloseFile(outputFile);
    } else if (outputRelease) {
	TclpReleaseFile(outputFile);
    }
    if (errorClose) {
	TclpCloseFile(errorFile);
    } else if (errorRelease) {
	TclpReleaseFile(errorFile);
    }
    return numPids;

    /*
     * An error occurred.  There could have been extra files open, such
     * as pipes between children.  Clean them all up.  Detach any child
     * processes that have been created.
     */

error:
    if (pipeIn != NULL) {
	TclpCloseFile(pipeIn);
    }
    if ((curOutFile != NULL) && (curOutFile != outputFile)) {
	TclpCloseFile(curOutFile);
    }
    if ((curInFile != NULL) && (curInFile != inputFile)) {
	TclpCloseFile(curInFile);
    }
    if ((inPipePtr != NULL) && (*inPipePtr != NULL)) {
	TclpCloseFile(*inPipePtr);
	*inPipePtr = NULL;
    }
    if ((outPipePtr != NULL) && (*outPipePtr != NULL)) {
	TclpCloseFile(*outPipePtr);
	*outPipePtr = NULL;
    }
    if ((errFilePtr != NULL) && (*errFilePtr != NULL)) {
	TclpCloseFile(*errFilePtr);
	*errFilePtr = NULL;
    }
    if (pidPtr != NULL) {
	for (i = 0; i < numPids; i++) {
	    if (pidPtr[i] != (Tcl_Pid) -1) {
		Tcl_DetachPids(1, &pidPtr[i]);
	    }
	}
	ckfree((char *) pidPtr);
    }
    numPids = -1;
    goto cleanup;
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
    TclFile *inPipePtr, *outPipePtr, *errFilePtr;
    TclFile inPipe, outPipe, errFile;
    int numPids;
    Tcl_Pid *pidPtr;
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
    
    channel = TclpCreateCommandChannel(outPipe, inPipe, errFile,
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
	TclpCloseFile(inPipe);
    }
    if (outPipe != NULL) {
	TclpCloseFile(outPipe);
    }
    if (errFile != NULL) {
	TclpCloseFile(errFile);
    }
    return NULL;
}
