/* 
 * tclUnixPipe.c --
 *
 *	This file implements the UNIX-specific exec pipeline functions,
 *	the "pipe" channel driver, and the "pid" Tcl command.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixPipe.c 1.36 97/05/14 13:24:24
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following macros convert between TclFile's and fd's.  The conversion
 * simple involves shifting fd's up by one to ensure that no valid fd is ever
 * the same as NULL.
 */

#define MakeFile(fd) ((TclFile)((fd)+1))
#define GetFd(file) (((int)file)-1)

/*
 * This structure describes per-instance state of a pipe based channel.
 */

typedef struct PipeState {
    Tcl_Channel channel;/* Channel associated with this file. */
    TclFile inFile;	/* Output from pipe. */
    TclFile outFile;	/* Input to pipe. */
    TclFile errorFile;	/* Error output from pipe. */
    int numPids;	/* How many processes are attached to this pipe? */
    Tcl_Pid *pidPtr;	/* The process IDs themselves. Allocated by
                         * the creator of the pipe. */
    int isNonBlocking;	/* Nonzero when the pipe is in nonblocking mode.
                         * Used to decide whether to wait for the children
                         * at close time. */
} PipeState;

/*
 * Declarations for local procedures defined in this file:
 */

static int	PipeBlockModeProc _ANSI_ARGS_((ClientData instanceData,
		    int mode));
static int	PipeCloseProc _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp));
static int	PipeGetHandleProc _ANSI_ARGS_((ClientData instanceData,
		    int direction, ClientData *handlePtr));
static int	PipeInputProc _ANSI_ARGS_((ClientData instanceData,
		    char *buf, int toRead, int *errorCode));
static int	PipeOutputProc _ANSI_ARGS_((
		    ClientData instanceData, char *buf, int toWrite,
		    int *errorCode));
static void	PipeWatchProc _ANSI_ARGS_((ClientData instanceData, int mask));
static void	RestoreSignals _ANSI_ARGS_((void));
static int	SetupStdFile _ANSI_ARGS_((TclFile file, int type));

/*
 * This structure describes the channel type structure for command pipe
 * based IO:
 */

static Tcl_ChannelType pipeChannelType = {
    "pipe",				/* Type name. */
    PipeBlockModeProc,			/* Set blocking/nonblocking mode.*/
    PipeCloseProc,			/* Close proc. */
    PipeInputProc,			/* Input proc. */
    PipeOutputProc,			/* Output proc. */
    NULL,				/* Seek proc. */
    NULL,				/* Set option proc. */
    NULL,				/* Get option proc. */
    PipeWatchProc,			/* Initialize notifier. */
    PipeGetHandleProc,			/* Get OS handles out of channel. */
};

/*
 *----------------------------------------------------------------------
 *
 * TclpMakeFile --
 *
 *	Make a TclFile from a channel.
 *
 * Results:
 *	Returns a new TclFile or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpMakeFile(channel, direction)
    Tcl_Channel channel;	/* Channel to get file from. */
    int direction;		/* Either TCL_READABLE or TCL_WRITABLE. */
{
    int fd;

    if (Tcl_GetChannelHandle(channel, direction, (ClientData *) &fd)
	    == TCL_OK) {
	return MakeFile(fd);
    } else {
	return (TclFile) NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpOpenFile --
 *
 *	Open a file for use in a pipeline.  
 *
 * Results:
 *	Returns a new TclFile handle or NULL on failure.
 *
 * Side effects:
 *	May cause a file to be created on the file system.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpOpenFile(fname, mode)
    char *fname;			/* The name of the file to open. */
    int mode;				/* In what mode to open the file? */
{
    int fd;

    fd = open(fname, mode, 0666);
    if (fd != -1) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);

	/*
	 * If the file is being opened for writing, seek to the end
	 * so we can append to any data already in the file.
	 */

	if (mode & O_WRONLY) {
	    lseek(fd, 0, SEEK_END);
	}

	/*
	 * Increment the fd so it can't be 0, which would conflict with
	 * the NULL return for errors.
	 */

	return MakeFile(fd);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateTempFile --
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

TclFile
TclpCreateTempFile(contents, namePtr)
    char *contents;		/* String to write into temp file, or NULL. */
    Tcl_DString *namePtr;	/* If non-NULL, pointer to initialized 
				 * DString that is filled with the name of 
				 * the temp file that was created. */
{
    char fileName[L_tmpnam];
    TclFile file;
    size_t length = (contents == NULL) ? 0 : strlen(contents);

    tmpnam(fileName);
    file = TclpOpenFile(fileName, O_RDWR|O_CREAT|O_TRUNC);
    unlink(fileName);

    if ((file != NULL) && (length > 0)) {
	int fd = GetFd(file);
	while (1) {
	    if (write(fd, contents, length) != -1) {
		break;
	    } else if (errno != EINTR) {
		close(fd);
		return NULL;
	    }
	}
	lseek(fd, 0, SEEK_SET);
    }
    if (namePtr != NULL) {
	Tcl_DStringAppend(namePtr, fileName, -1);
    }
    return file;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreatePipe --
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
TclpCreatePipe(readPipe, writePipe)
    TclFile *readPipe;		/* Location to store file handle for
				 * read side of pipe. */
    TclFile *writePipe;		/* Location to store file handle for
				 * write side of pipe. */
{
    int pipeIds[2];

    if (pipe(pipeIds) != 0) {
	return 0;
    }

    fcntl(pipeIds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeIds[1], F_SETFD, FD_CLOEXEC);

    *readPipe = MakeFile(pipeIds[0]);
    *writePipe = MakeFile(pipeIds[1]);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCloseFile --
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
TclpCloseFile(file)
    TclFile file;	/* The file to close. */
{
    int fd = GetFd(file);

    /*
     * Refuse to close the fds for stdin, stdout and stderr.
     */
    
    if ((fd == 0) || (fd == 1) || (fd == 2)) {
        return 0;
    }
    
    Tcl_DeleteFileHandler(fd);
    return close(fd);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateProcess --
 *
 *	Create a child process that has the specified files as its 
 *	standard input, output, and error.  The child process runs
 *	asynchronously and runs with the same environment variables
 *	as the creating process.
 *
 *	The path is searched to find the specified executable.  
 *
 * Results:
 *	The return value is TCL_ERROR and an error message is left in
 *	interp->result if there was a problem creating the child 
 *	process.  Otherwise, the return value is TCL_OK and *pidPtr is
 *	filled with the process id of the child process.
 * 
 * Side effects:
 *	A process is created.
 *	
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
int
TclpCreateProcess(interp, argc, argv, inputFile, outputFile, errorFile, 
	pidPtr)
    Tcl_Interp *interp;		/* Interpreter in which to leave errors that
				 * occurred when creating the child process.
				 * Error messages from the child process
				 * itself are sent to errorFile. */
    int argc;			/* Number of arguments in following array. */
    char **argv;		/* Array of argument strings.  argv[0]
				 * contains the name of the executable
				 * converted to native format (using the
				 * Tcl_TranslateFileName call).  Additional
				 * arguments have not been converted. */
    TclFile inputFile;		/* If non-NULL, gives the file to use as
				 * input for the child process.  If inputFile
				 * file is not readable or is NULL, the child
				 * will receive no standard input. */
    TclFile outputFile;		/* If non-NULL, gives the file that
				 * receives output from the child process.  If
				 * outputFile file is not writeable or is
				 * NULL, output from the child will be
				 * discarded. */
    TclFile errorFile;		/* If non-NULL, gives the file that
				 * receives errors from the child process.  If
				 * errorFile file is not writeable or is NULL,
				 * errors from the child will be discarded.
				 * errorFile may be the same as outputFile. */
    Tcl_Pid *pidPtr;		/* If this procedure is successful, pidPtr
				 * is filled with the process id of the child
				 * process. */
{
    TclFile errPipeIn, errPipeOut;
    int joinThisError, count, status, fd;
    char errSpace[200];
    int pid;
    
    errPipeIn = NULL;
    errPipeOut = NULL;
    pid = -1;

    /*
     * Create a pipe that the child can use to return error
     * information if anything goes wrong.
     */

    if (TclpCreatePipe(&errPipeIn, &errPipeOut) == 0) {
	Tcl_AppendResult(interp, "couldn't create pipe: ",
		Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }

    joinThisError = (errorFile == outputFile);
    pid = vfork();
    if (pid == 0) {
	fd = GetFd(errPipeOut);

	/*
	 * Set up stdio file handles for the child process.
	 */

	if (!SetupStdFile(inputFile, TCL_STDIN)
		|| !SetupStdFile(outputFile, TCL_STDOUT)
		|| (!joinThisError && !SetupStdFile(errorFile, TCL_STDERR))
		|| (joinThisError &&
			((dup2(1,2) == -1) ||
			 (fcntl(2, F_SETFD, 0) != 0)))) {
	    sprintf(errSpace,
		    "%dforked process couldn't set up input/output: ",
		    errno);
	    write(fd, errSpace, (size_t) strlen(errSpace));
	    _exit(1);
	}

	/*
	 * Close the input side of the error pipe.
	 */

	RestoreSignals();
	execvp(argv[0], &argv[0]);
	sprintf(errSpace, "%dcouldn't execute \"%.150s\": ", errno,
		argv[0]);
	write(fd, errSpace, (size_t) strlen(errSpace));
	_exit(1);
    }
    if (pid == -1) {
	Tcl_AppendResult(interp, "couldn't fork child process: ",
		Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }

    /*
     * Read back from the error pipe to see if the child started
     * up OK.  The info in the pipe (if any) consists of a decimal
     * errno value followed by an error message.
     */

    TclpCloseFile(errPipeOut);
    errPipeOut = NULL;

    fd = GetFd(errPipeIn);
    count = read(fd, errSpace, (size_t) (sizeof(errSpace) - 1));
    if (count > 0) {
	char *end;
	errSpace[count] = 0;
	errno = strtol(errSpace, &end, 10);
	Tcl_AppendResult(interp, end, Tcl_PosixError(interp),
		(char *) NULL);
	goto error;
    }
    
    TclpCloseFile(errPipeIn);
    *pidPtr = (Tcl_Pid) pid;
    return TCL_OK;

    error:
    if (pid != -1) {
	/*
	 * Reap the child process now if an error occurred during its
	 * startup.
	 */

	Tcl_WaitPid((Tcl_Pid) pid, &status, WNOHANG);
    }
    
    if (errPipeIn) {
	TclpCloseFile(errPipeIn);
    }
    if (errPipeOut) {
	TclpCloseFile(errPipeOut);
    }
    return TCL_ERROR;
}

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
    TclFile file;		/* File to dup, or NULL. */
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
	    file = TclpMakeFile(channel, direction);
	}
    }
    if (file) {
	fd = GetFd(file);
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
 * TclpCreateCommandChannel --
 *
 *	This function is called by the generic IO level to perform
 *	the platform specific channel initialization for a command
 *	channel.
 *
 * Results:
 *	Returns a new channel or NULL on failure.
 *
 * Side effects:
 *	Allocates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpCreateCommandChannel(readFile, writeFile, errorFile, numPids, pidPtr)
    TclFile readFile;		/* If non-null, gives the file for reading. */
    TclFile writeFile;		/* If non-null, gives the file for writing. */
    TclFile errorFile;		/* If non-null, gives the file where errors
				 * can be read. */
    int numPids;		/* The number of pids in the pid array. */
    Tcl_Pid *pidPtr;		/* An array of process identifiers.
                                 * Allocated by the caller, freed when
                                 * the channel is closed or the processes
                                 * are detached (in a background exec). */
{
    char channelName[20];
    int channelId;
    PipeState *statePtr = (PipeState *) ckalloc((unsigned) sizeof(PipeState));
    int mode;

    statePtr->inFile = readFile;
    statePtr->outFile = writeFile;
    statePtr->errorFile = errorFile;
    statePtr->numPids = numPids;
    statePtr->pidPtr = pidPtr;
    statePtr->isNonBlocking = 0;

    mode = 0;
    if (readFile) {
        mode |= TCL_READABLE;
    }
    if (writeFile) {
        mode |= TCL_WRITABLE;
    }
    
    /*
     * Use one of the fds associated with the channel as the
     * channel id.
     */

    if (readFile) {
	channelId = GetFd(readFile);
    } else if (writeFile) {
	channelId = GetFd(writeFile);
    } else if (errorFile) {
	channelId = GetFd(errorFile);
    } else {
	channelId = 0;
    }

    /*
     * For backward compatibility with previous versions of Tcl, we
     * use "file%d" as the base name for pipes even though it would
     * be more natural to use "pipe%d".
     */

    sprintf(channelName, "file%d", channelId);
    statePtr->channel = Tcl_CreateChannel(&pipeChannelType, channelName,
            (ClientData) statePtr, mode);
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetAndDetachPids --
 *
 *	This procedure is invoked in the generic implementation of a
 *	background "exec" (An exec when invoked with a terminating "&")
 *	to store a list of the PIDs for processes in a command pipeline
 *	in interp->result and to detach the processes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies interp->result. Detaches processes.
 *
 *----------------------------------------------------------------------
 */

void
TclGetAndDetachPids(interp, chan)
    Tcl_Interp *interp;
    Tcl_Channel chan;
{
    PipeState *pipePtr;
    Tcl_ChannelType *chanTypePtr;
    int i;
    char buf[20];

    /*
     * Punt if the channel is not a command channel.
     */

    chanTypePtr = Tcl_GetChannelType(chan);
    if (chanTypePtr != &pipeChannelType) {
        return;
    }

    pipePtr = (PipeState *) Tcl_GetChannelInstanceData(chan);
    for (i = 0; i < pipePtr->numPids; i++) {
        sprintf(buf, "%ld", TclpGetPid(pipePtr->pidPtr[i]));
        Tcl_AppendElement(interp, buf);
        Tcl_DetachPids(1, &(pipePtr->pidPtr[i]));
    }
    if (pipePtr->numPids > 0) {
        ckfree((char *) pipePtr->pidPtr);
        pipePtr->numPids = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeBlockModeProc --
 *
 *	Helper procedure to set blocking and nonblocking modes on a
 *	pipe based channel. Invoked by generic IO level code.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
PipeBlockModeProc(instanceData, mode)
    ClientData instanceData;		/* Pipe state. */
    int mode;				/* The mode to set. Can be one of
                                         * TCL_MODE_BLOCKING or
                                         * TCL_MODE_NONBLOCKING. */
{
    PipeState *psPtr = (PipeState *) instanceData;
    int curStatus;
    int fd;

#ifndef	USE_FIONBIO    
    if (psPtr->inFile) {
        fd = GetFd(psPtr->inFile);
        curStatus = fcntl(fd, F_GETFL);
        if (mode == TCL_MODE_BLOCKING) {
            curStatus &= (~(O_NONBLOCK));
        } else {
            curStatus |= O_NONBLOCK;
        }
        if (fcntl(fd, F_SETFL, curStatus) < 0) {
            return errno;
        }
        curStatus = fcntl(fd, F_GETFL);
    }
    if (psPtr->outFile) {
        fd = GetFd(psPtr->outFile);
        curStatus = fcntl(fd, F_GETFL);
        if (mode == TCL_MODE_BLOCKING) {
            curStatus &= (~(O_NONBLOCK));
        } else {
            curStatus |= O_NONBLOCK;
        }
        if (fcntl(fd, F_SETFL, curStatus) < 0) {
            return errno;
        }
    }
#endif	/* !FIONBIO */

#ifdef	USE_FIONBIO
    if (psPtr->inFile) {
        fd = GetFd(psPtr->inFile);
        if (mode == TCL_MODE_BLOCKING) {
            curStatus = 0;
        } else {
            curStatus = 1;
        }
        if (ioctl(fd, (int) FIONBIO, &curStatus) < 0) {
            return errno;
        }
    }
    if (psPtr->outFile != NULL) {
        fd = GetFd(psPtr->outFile);
        if (mode == TCL_MODE_BLOCKING) {
            curStatus = 0;
        } else {
            curStatus = 1;
        }
        if (ioctl(fd, (int) FIONBIO,  &curStatus) < 0) {
            return errno;
        }
    }
#endif	/* USE_FIONBIO */
    
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeCloseProc --
 *
 *	This procedure is invoked by the generic IO level to perform
 *	channel-type-specific cleanup when a command pipeline channel
 *	is closed.
 *
 * Results:
 *	0 on success, errno otherwise.
 *
 * Side effects:
 *	Closes the command pipeline channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
PipeCloseProc(instanceData, interp)
    ClientData instanceData;	/* The pipe to close. */
    Tcl_Interp *interp;		/* For error reporting. */
{
    PipeState *pipePtr;
    Tcl_Channel errChan;
    int errorCode, result;

    errorCode = 0;
    result = 0;
    pipePtr = (PipeState *) instanceData;
    if (pipePtr->inFile) {
	if (TclpCloseFile(pipePtr->inFile) < 0) {
	    errorCode = errno;
	}
    }
    if (pipePtr->outFile) {
	if ((TclpCloseFile(pipePtr->outFile) < 0) && (errorCode == 0)) {
	    errorCode = errno;
	}
    }

    if (pipePtr->isNonBlocking || TclInExit()) {
    
	/*
         * If the channel is non-blocking or Tcl is being cleaned up, just
         * detach the children PIDs, reap them (important if we are in a
         * dynamic load module), and discard the errorFile.
         */
        
        Tcl_DetachPids(pipePtr->numPids, pipePtr->pidPtr);
        Tcl_ReapDetachedProcs();

        if (pipePtr->errorFile) {
	    TclpCloseFile(pipePtr->errorFile);
        }
    } else {
        
	/*
         * Wrap the error file into a channel and give it to the cleanup
         * routine.
         */

        if (pipePtr->errorFile) {
	    errChan = Tcl_MakeFileChannel(
		(ClientData) GetFd(pipePtr->errorFile), TCL_READABLE);
        } else {
            errChan = NULL;
        }
        result = TclCleanupChildren(interp, pipePtr->numPids, pipePtr->pidPtr,
                errChan);
    }

    if (pipePtr->numPids != 0) {
        ckfree((char *) pipePtr->pidPtr);
    }
    ckfree((char *) pipePtr);
    if (errorCode == 0) {
        return result;
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeInputProc --
 *
 *	This procedure is invoked from the generic IO level to read
 *	input from a command pipeline based channel.
 *
 * Results:
 *	The number of bytes read is returned or -1 on error. An output
 *	argument contains a POSIX error code if an error occurs, or zero.
 *
 * Side effects:
 *	Reads input from the input device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeInputProc(instanceData, buf, toRead, errorCodePtr)
    ClientData instanceData;		/* Pipe state. */
    char *buf;				/* Where to store data read. */
    int toRead;				/* How much space is available
                                         * in the buffer? */
    int *errorCodePtr;			/* Where to store error code. */
{
    PipeState *psPtr = (PipeState *) instanceData;
    int bytesRead;			/* How many bytes were actually
                                         * read from the input device? */

    *errorCodePtr = 0;
    
    /*
     * Assume there is always enough input available. This will block
     * appropriately, and read will unblock as soon as a short read is
     * possible, if the channel is in blocking mode. If the channel is
     * nonblocking, the read will never block.
     */

    bytesRead = read(GetFd(psPtr->inFile), buf, (size_t) toRead);
    if (bytesRead > -1) {
        return bytesRead;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeOutputProc--
 *
 *	This procedure is invoked from the generic IO level to write
 *	output to a command pipeline based channel.
 *
 * Results:
 *	The number of bytes written is returned or -1 on error. An
 *	output argument	contains a POSIX error code if an error occurred,
 *	or zero.
 *
 * Side effects:
 *	Writes output on the output device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeOutputProc(instanceData, buf, toWrite, errorCodePtr)
    ClientData instanceData;		/* Pipe state. */
    char *buf;				/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCodePtr;			/* Where to store error code. */
{
    PipeState *psPtr = (PipeState *) instanceData;
    int written;

    *errorCodePtr = 0;
    written = write(GetFd(psPtr->outFile), buf, (size_t) toWrite);
    if (written > -1) {
        return written;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeWatchProc --
 *
 *	Initialize the notifier to watch the fds from this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up the notifier so that a future event on the channel will
 *	be seen by Tcl.
 *
 *----------------------------------------------------------------------
 */

static void
PipeWatchProc(instanceData, mask)
    ClientData instanceData;		/* The pipe state. */
    int mask;				/* Events of interest; an OR-ed
                                         * combination of TCL_READABLE,
                                         * TCL_WRITABEL and TCL_EXCEPTION. */
{
    PipeState *psPtr = (PipeState *) instanceData;
    int newmask;

    if (psPtr->inFile) {
	newmask = mask & (TCL_READABLE | TCL_EXCEPTION);
	if (newmask) {
	    Tcl_CreateFileHandler(GetFd(psPtr->inFile), mask,
		    (Tcl_FileProc *) Tcl_NotifyChannel,
		    (ClientData) psPtr->channel);
	} else {
	    Tcl_DeleteFileHandler(GetFd(psPtr->inFile));
	}
    }
    if (psPtr->outFile) {
	newmask = mask & (TCL_WRITABLE | TCL_EXCEPTION);
	if (newmask) {
	    Tcl_CreateFileHandler(GetFd(psPtr->outFile), mask,
		    (Tcl_FileProc *) Tcl_NotifyChannel,
		    (ClientData) psPtr->channel);
	} else {
	    Tcl_DeleteFileHandler(GetFd(psPtr->outFile));
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from
 *	inside a command pipeline based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if
 *	there is no handle for the specified direction. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PipeGetHandleProc(instanceData, direction, handlePtr)
    ClientData instanceData;	/* The pipe state. */
    int direction;		/* TCL_READABLE or TCL_WRITABLE */
    ClientData *handlePtr;	/* Where to store the handle.  */
{
    PipeState *psPtr = (PipeState *) instanceData;

    if (direction == TCL_READABLE && psPtr->inFile) {
	*handlePtr = (ClientData) GetFd(psPtr->inFile);
	return TCL_OK;
    }
    if (direction == TCL_WRITABLE && psPtr->outFile) {
	*handlePtr = (ClientData) GetFd(psPtr->outFile);
	return TCL_OK;
    }
    return TCL_ERROR;
}

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

Tcl_Pid
Tcl_WaitPid(pid, statPtr, options)
    Tcl_Pid pid;
    int *statPtr;
    int options;
{
    int result;
    pid_t real_pid;

    real_pid = (pid_t) pid;
    while (1) {
	result = (int) waitpid(real_pid, statPtr, options);
	if ((result != -1) || (errno != EINTR)) {
	    return (Tcl_Pid) result;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PidObjCmd --
 *
 *	This procedure is invoked to process the "pid" Tcl command.
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
Tcl_PidObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST *objv;	/* Argument strings. */
{
    Tcl_Channel chan;
    Tcl_ChannelType *chanTypePtr;
    PipeState *pipePtr;
    int i;
    Tcl_Obj *resultPtr, *longObjPtr;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?channelId?");
	return TCL_ERROR;
    }
    if (objc == 1) {
	Tcl_SetLongObj(Tcl_GetObjResult(interp), (long) getpid());
    } else {
        chan = Tcl_GetChannel(interp, Tcl_GetStringFromObj(objv[1], NULL),
		NULL);
        if (chan == (Tcl_Channel) NULL) {
	    return TCL_ERROR;
	}
	chanTypePtr = Tcl_GetChannelType(chan);
	if (chanTypePtr != &pipeChannelType) {
	    return TCL_OK;
	}
        pipePtr = (PipeState *) Tcl_GetChannelInstanceData(chan);
	resultPtr = Tcl_GetObjResult(interp);
        for (i = 0; i < pipePtr->numPids; i++) {
	    longObjPtr = Tcl_NewLongObj((long) TclpGetPid(pipePtr->pidPtr[i]));
	    Tcl_ListObjAppendElement(NULL, resultPtr, longObjPtr);
	}
    }
    return TCL_OK;
}
