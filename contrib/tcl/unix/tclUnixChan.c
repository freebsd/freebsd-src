/* 
 * tclUnixChan.c
 *
 *	Common channel driver for Unix channels based on files, command
 *	pipes and TCP sockets.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixChan.c 1.172 96/06/11 10:14:51
 */

#include	"tclInt.h"	/* Internal definitions for Tcl. */
#include	"tclPort.h"	/* Portability features for Tcl. */

/*
 * This structure describes per-instance state of a pipe based channel.
 */

typedef struct PipeState {
    Tcl_File readFile;	/* Output from pipe. */
    Tcl_File writeFile;	/* Input to pipe. */
    Tcl_File errorFile;	/* Error output from pipe. */
    int numPids;	/* How many processes are attached to this pipe? */
    int *pidPtr;	/* The process IDs themselves. Allocated by
                         * the creator of the pipe. */
    int isNonBlocking;	/* Nonzero when the pipe is in nonblocking mode.
                         * Used to decide whether to wait for the children
                         * at close time. */
} PipeState;

/*
 * This structure describes per-instance state of a tcp based channel.
 */

typedef struct TcpState {
    int flags;				/* ORed combination of the
                                         * bitfields defined below. */
    Tcl_File sock;			/* The socket itself. */
    Tcl_TcpAcceptProc *acceptProc;	/* Proc to call on accept. */
    ClientData acceptProcData;		/* The data for the accept proc. */
} TcpState;

/*
 * These bits may be ORed together into the "flags" field of a TcpState
 * structure.
 */

#define TCP_ASYNC_SOCKET	(1<<0)	/* Asynchronous socket. */
#define TCP_ASYNC_CONNECT	(1<<1)	/* Async connect in progress. */

/*
 * The following defines the maximum length of the listen queue. This is
 * the number of outstanding yet-to-be-serviced requests for a connection
 * on a server socket, more than this number of outstanding requests and
 * the connection request will fail.
 */

#define TCL_LISTEN_LIMIT	100

/*
 * The following defines how much buffer space the kernel should maintain
 * for a socket.
 */

#define SOCKET_BUFSIZE	4096

/*
 * Static routines for this file:
 */

static TcpState *	CreateSocket _ANSI_ARGS_((Tcl_Interp *interp,
			    int port, char *host, int server,
			    char *myaddr, int myport, int async));
static int		CreateSocketAddress _ANSI_ARGS_(
			    (struct sockaddr_in *sockaddrPtr,
			    char *host, int port));
static int		FileBlockModeProc _ANSI_ARGS_((
    			    ClientData instanceData, Tcl_File inFile,
                            Tcl_File outFile, int mode));
static int		FileCloseProc _ANSI_ARGS_((ClientData instanceData,
			    Tcl_Interp *interp, Tcl_File inFile,
                            Tcl_File outFile));
static int		FilePipeInputProc _ANSI_ARGS_((ClientData instanceData,
		            Tcl_File inFile, char *buf, int toRead,
		            int *errorCode));
static int		FilePipeOutputProc _ANSI_ARGS_((
    			    ClientData instanceData, Tcl_File outFile,
                            char *buf, int toWrite, int *errorCode));
static int		FileSeekProc _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, Tcl_File outFile, long offset,
			    int mode, int *errorCode));
static int		PipeBlockModeProc _ANSI_ARGS_((
    			    ClientData instanceData, Tcl_File inFile,
                            Tcl_File outFile, int mode));
static int		PipeCloseProc _ANSI_ARGS_((ClientData instanceData,
			    Tcl_Interp *interp, Tcl_File inFile,
                            Tcl_File outFile));
static void		TcpAccept _ANSI_ARGS_((ClientData data, int mask));
static int		TcpBlockModeProc _ANSI_ARGS_((ClientData data,
		            Tcl_File inFile, Tcl_File outFile, int mode));
static int		TcpCloseProc _ANSI_ARGS_((ClientData instanceData,
			    Tcl_Interp *interp, Tcl_File inFile,
                            Tcl_File outFile));
static int		TcpGetOptionProc _ANSI_ARGS_((ClientData instanceData,
                            char *optionName, Tcl_DString *dsPtr));
static int		TcpInputProc _ANSI_ARGS_((ClientData instanceData,
		            Tcl_File infile, char *buf, int toRead,
        		    int *errorCode));
static int		TcpOutputProc _ANSI_ARGS_((ClientData instanceData,
		            Tcl_File outFile, char *buf, int toWrite,
		            int *errorCode));
static int		WaitForConnect _ANSI_ARGS_((TcpState *statePtr,
		            Tcl_File fileToWaitFor, int *errorCodePtr));

/*
 * This structure describes the channel type structure for file based IO:
 */

static Tcl_ChannelType fileChannelType = {
    "file",				/* Type name. */
    FileBlockModeProc,			/* Set blocking/nonblocking mode.*/
    FileCloseProc,			/* Close proc. */
    FilePipeInputProc,			/* Input proc. */
    FilePipeOutputProc,			/* Output proc. */
    FileSeekProc,			/* Seek proc. */
    NULL,				/* Set option proc. */
    NULL,				/* Get option proc. */
};

/*
 * This structure describes the channel type structure for command pipe
 * based IO:
 */

static Tcl_ChannelType pipeChannelType = {
    "pipe",				/* Type name. */
    PipeBlockModeProc,			/* Set blocking/nonblocking mode.*/
    PipeCloseProc,			/* Close proc. */
    FilePipeInputProc,			/* Input proc. */
    FilePipeOutputProc,			/* Output proc. */
    NULL,				/* Seek proc. */
    NULL,				/* Set option proc. */
    NULL,				/* Get option proc. */
};

/*
 * This structure describes the channel type structure for TCP socket
 * based IO:
 */

static Tcl_ChannelType tcpChannelType = {
    "tcp",				/* Type name. */
    TcpBlockModeProc,			/* Set blocking/nonblocking mode.*/
    TcpCloseProc,			/* Close proc. */
    TcpInputProc,			/* Input proc. */
    TcpOutputProc,			/* Output proc. */
    NULL,				/* Seek proc. */
    NULL,				/* Set option proc. */
    TcpGetOptionProc,			/* Get option proc. */
};

/*
 *----------------------------------------------------------------------
 *
 * FileBlockModeProc --
 *
 *	Helper procedure to set blocking and nonblocking modes on a
 *	channel. Invoked either by generic IO level code or by other
 *	channel drivers after doing channel-type-specific inialization.
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
FileBlockModeProc(instanceData, inFile, outFile, mode)
    ClientData instanceData;		/* Unused. */
    Tcl_File inFile, outFile;		/* Input, output files for channel. */
    int mode;				/* The mode to set. Can be one of
                                         * TCL_MODE_BLOCKING or
                                         * TCL_MODE_NONBLOCKING. */
{
    int curStatus;
    int fd;

    if (inFile != NULL) {
        fd = (int) Tcl_GetFileInfo(inFile, NULL);
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
    if (outFile != NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
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

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeBlockModeProc --
 *
 *	Helper procedure to set blocking and nonblocking modes on a
 *	channel. Invoked either by generic IO level code or by other
 *	channel drivers after doing channel-type-specific inialization.
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
PipeBlockModeProc(instanceData, inFile, outFile, mode)
    ClientData instanceData;		/* The pipe state. */
    Tcl_File inFile, outFile;		/* Input, output files for channel. */
    int mode;				/* The mode to set. Can be one of
                                         * TCL_MODE_BLOCKING or
                                         * TCL_MODE_NONBLOCKING. */
{
    PipeState *pipePtr;
    int curStatus;
    int fd;

    if (inFile != NULL) {
        fd = (int) Tcl_GetFileInfo(inFile, NULL);
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
    if (outFile != NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
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

    pipePtr = (PipeState *) instanceData;
    pipePtr->isNonBlocking = (mode == TCL_MODE_NONBLOCKING) ? 1 : 0;
    
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FilePipeInputProc --
 *
 *	This procedure is invoked from the generic IO level to read
 *	input from a file or command pipeline channel.
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

	/* ARGSUSED */
static int
FilePipeInputProc(instanceData, inFile, buf, toRead, errorCodePtr)
    ClientData instanceData;		/* Unused. */
    Tcl_File inFile;			/* Input device for channel. */
    char *buf;				/* Where to store data read. */
    int toRead;				/* How much space is available
                                         * in the buffer? */
    int *errorCodePtr;			/* Where to store error code. */
{
    int fd;				/* The OS handle for reading. */
    int bytesRead;			/* How many bytes were actually
                                         * read from the input device? */

    *errorCodePtr = 0;
    fd = (int) Tcl_GetFileInfo(inFile, NULL);
    
    /*
     * Assume there is always enough input available. This will block
     * appropriately, and read will unblock as soon as a short read is
     * possible, if the channel is in blocking mode. If the channel is
     * nonblocking, the read will never block.
     */

    bytesRead = read(fd, buf, (size_t) toRead);
    if (bytesRead > -1) {
        return bytesRead;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FilePipeOutputProc--
 *
 *	This procedure is invoked from the generic IO level to write
 *	output to a file or command pipeline channel.
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

	/* ARGSUSED */
static int
FilePipeOutputProc(instanceData, outFile, buf, toWrite, errorCodePtr)
    ClientData instanceData;		/* Unused. */
    Tcl_File outFile;			/* Output device for channel. */
    char *buf;				/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCodePtr;			/* Where to store error code. */
{
    int written;
    int fd;

    *errorCodePtr = 0;
    fd = (int) Tcl_GetFileInfo(outFile, NULL);
    written = write(fd, buf, (size_t) toWrite);
    if (written > -1) {
        return written;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FileCloseProc --
 *
 *	This procedure is called from the generic IO level to perform
 *	channel-type-specific cleanup when a file based channel is closed.
 *
 * Results:
 *	0 if successful, errno if failed.
 *
 * Side effects:
 *	Closes the device of the channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
FileCloseProc(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* Unused. */
    Tcl_Interp *interp;		/* For error reporting - unused. */
    Tcl_File inFile;		/* Input file to close. */
    Tcl_File outFile;		/* Output file to close. */
{
    int fd, errorCode = 0;

    if (inFile != NULL) {

	/*
	 * Check for read/write file so we only close it once.
	 */

	if (inFile == outFile) {
	    outFile = NULL;
	}
        fd = (int) Tcl_GetFileInfo(inFile, NULL);
        Tcl_FreeFile(inFile);

        if (tclInInterpreterDeletion) {
            if ((fd != 0) && (fd != 1) && (fd != 2)) {
                if (close(fd) < 0) {
                    errorCode = errno;
                }
            }
        } else if (close(fd) < 0) {
            errorCode = errno;
        }
    }

    if (outFile != NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
        Tcl_FreeFile(outFile);        
        if (tclInInterpreterDeletion) {
            if ((fd != 0) && (fd != 1) && (fd != 2)) {
                if ((close(fd) < 0) && (errorCode == 0)) {
                    errorCode = errno;
                }
            }
        } else if ((close(fd) < 0) && (errorCode == 0)) {
            errorCode = errno;
        }
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * FileSeekProc --
 *
 *	This procedure is called by the generic IO level to move the
 *	access point in a file based channel.
 *
 * Results:
 *	-1 if failed, the new position if successful. An output
 *	argument contains the POSIX error code if an error occurred,
 *	or zero.
 *
 * Side effects:
 *	Moves the location at which the channel will be accessed in
 *	future operations.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
FileSeekProc(instanceData, inFile, outFile, offset, mode, errorCodePtr)
    ClientData instanceData;			/* Unused. */
    Tcl_File inFile, outFile;			/* Input and output
                                                 * files for channel. */
    long offset;				/* Offset to seek to. */
    int mode;					/* Relative to where
                                                 * should we seek? Can be
                                                 * one of SEEK_START,
                                                 * SEEK_SET or SEEK_END. */
    int *errorCodePtr;				/* To store error code. */
{
    int newLoc;
    int fd;

    *errorCodePtr = 0;
    if (inFile != (Tcl_File) NULL) {
        fd = (int) Tcl_GetFileInfo(inFile, NULL);
    } else if (outFile != (Tcl_File) NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
    } else {
        *errorCodePtr = EFAULT;
        return -1;
    }
    newLoc = lseek(fd, offset, mode);
    if (newLoc > -1) {
        return newLoc;
    }
    *errorCodePtr = errno;
    return -1;
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
        sprintf(buf, "%d", pipePtr->pidPtr[i]);
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
PipeCloseProc(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* The pipe to close. */
    Tcl_Interp *interp;		/* For error reporting. */
    Tcl_File inFile, outFile;	/* Unused. */
{
    PipeState *pipePtr;
    Tcl_Channel errChan;
    int fd, errorCode, result;

    errorCode = 0;
    result = 0;
    pipePtr = (PipeState *) instanceData;
    if (pipePtr->readFile != NULL) {
        fd = (int) Tcl_GetFileInfo(pipePtr->readFile, NULL);
        Tcl_FreeFile(pipePtr->readFile);
	if (close(fd) < 0) {
	    errorCode = errno;
	}
    }
    if (pipePtr->writeFile != NULL) {
        fd = (int) Tcl_GetFileInfo(pipePtr->writeFile, NULL);
        Tcl_FreeFile(pipePtr->writeFile);
	if ((close(fd) < 0) && (errorCode == 0)) {
	    errorCode = errno;
	}
    }

    if (pipePtr->isNonBlocking) {
    
	/*
         * If the channel is non-blocking, just detach the children PIDs
         * and discard the errorFile.
         */
        
        Tcl_DetachPids(pipePtr->numPids, pipePtr->pidPtr);
        if (pipePtr->errorFile != NULL) {
            Tcl_FreeFile(pipePtr->errorFile);
        }
    } else {
        
	/*
         * Wrap the error file into a channel and give it to the cleanup
         * routine.
         */

        if (pipePtr->errorFile != NULL) {
            errChan = Tcl_CreateChannel(&fileChannelType, "pipeError",
                    pipePtr->errorFile, NULL, NULL);
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
 * Tcl_OpenFileChannel --
 *
 *	Open an file based channel on Unix systems.
 *
 * Results:
 *	The new channel or NULL. If NULL, the output argument
 *	errorCodePtr is set to a POSIX error and an error message is
 *	left in interp->result if interp is not NULL.
 *
 * Side effects:
 *	May open the channel and may cause creation of a file on the
 *	file system.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenFileChannel(interp, fileName, modeString, permissions)
    Tcl_Interp *interp;			/* Interpreter for error reporting;
                                         * can be NULL. */
    char *fileName;			/* Name of file to open. */
    char *modeString;			/* A list of POSIX open modes or
                                         * a string such as "rw". */
    int permissions;			/* If the open involves creating a
                                         * file, with what modes to create
                                         * it? */
{
    int fd, seekFlag, mode, channelPermissions;
    Tcl_File file;
    Tcl_Channel chan;
    char *nativeName, channelName[20];
    Tcl_DString buffer;

    mode = TclGetOpenMode(interp, modeString, &seekFlag);
    if (mode == -1) {
        return NULL;
    }
    switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
	case O_RDONLY:
	    channelPermissions = TCL_READABLE;
	    break;
	case O_WRONLY:
	    channelPermissions = TCL_WRITABLE;
	    break;
	case O_RDWR:
	    channelPermissions = (TCL_READABLE | TCL_WRITABLE);
	    break;
	default:
            /*
             * This may occurr if modeString was "", for example.
             */
	    panic("Tcl_OpenFileChannel: invalid mode value");
	    return NULL;
    }

    nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (nativeName == NULL) {
	return NULL;
    }
    fd = open(nativeName, mode, permissions);

    /*
     * If nativeName is not NULL, the buffer is valid and we must free
     * the storage.
     */
    
    Tcl_DStringFree(&buffer);

    if (fd < 0) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "couldn't open \"", fileName, "\": ",
                    Tcl_PosixError(interp), (char *) NULL);
        }
        return NULL;
    }
    
    sprintf(channelName, "file%d", fd);
    file = Tcl_GetFile((ClientData) fd, TCL_UNIX_FD);
    
    chan = Tcl_CreateChannel(&fileChannelType, channelName,
            (channelPermissions & TCL_READABLE) ? file : NULL,
            (channelPermissions & TCL_WRITABLE) ? file : NULL,
            (ClientData) NULL);

    /*
     * The channel may not be open now, for example if we tried to
     * open a file with permissions that cannot be satisfied.
     */
    
    if (chan == (Tcl_Channel) NULL) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "couldn't create channel \"",
                    channelName, "\": ", Tcl_PosixError(interp),
                    (char *) NULL);
        }
        Tcl_FreeFile(file);
        close(fd);
        return NULL;
    }

    if (seekFlag) {
        if (Tcl_Seek(chan, 0, SEEK_END) < 0) {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "couldn't seek to end of file on \"",
                        channelName, "\": ", Tcl_PosixError(interp),
                        (char *) NULL);
            }
            Tcl_Close(NULL, chan);
            return NULL;
        }
    }
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeFileChannel --
 *
 *	Makes a Tcl_Channel from an existing OS level file handle.
 *
 * Results:
 *	The Tcl_Channel created around the preexisting OS level file handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeFileChannel(inFd, outFd, mode)
    ClientData inFd;		/* OS level handle used for input. */
    ClientData outFd;		/* OS level handle used for output. */
    int mode;			/* ORed combination of TCL_READABLE and
                                 * TCL_WRITABLE to indicate whether inFile
                                 * and/or outFile are valid. */
{
    Tcl_Channel chan;
    int fileUsed;
    Tcl_File inFile, outFile;
    char channelName[20];

    if (mode == 0) {
        return (Tcl_Channel) NULL;
    }
    
    inFile = (Tcl_File) NULL;
    outFile = (Tcl_File) NULL;
    
    if (mode & TCL_READABLE) {
	sprintf(channelName, "file%d", (int) inFd);
        inFile = Tcl_GetFile(inFd, TCL_UNIX_FD);
    }
    
    if (mode & TCL_WRITABLE) {
	sprintf(channelName, "file%d", (int) outFd);
        outFile = Tcl_GetFile(outFd, TCL_UNIX_FD);
    }

    /*
     * Look to see if a channel with those two Tcl_Files already exists.
     * If so, return it.
     */
    
    chan = TclFindFileChannel(inFile, outFile, &fileUsed);
    if (chan != (Tcl_Channel) NULL) {
        return chan;
    }

    /*
     * If one of the Tcl_Files is used in another channel, do not
     * create a new channel containing it; this avoids core dumps
     * later, when the Tcl_File would be freed twice.
     */
    
    if (fileUsed) {
        return (Tcl_Channel) NULL;
    }
    return Tcl_CreateChannel(&fileChannelType, channelName, inFile, outFile,
            (ClientData) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateCommandChannel --
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
TclCreateCommandChannel(readFile, writeFile, errorFile, numPids, pidPtr)
    Tcl_File readFile;		/* If non-null, gives the file for reading. */
    Tcl_File writeFile;		/* If non-null, gives the file for writing. */
    Tcl_File errorFile;		/* If non-null, gives the file where errors
				 * can be read. */
    int numPids;		/* The number of pids in the pid array. */
    int *pidPtr;		/* An array of process identifiers.
                                 * Allocated by the caller, freed when
                                 * the channel is closed or the processes
                                 * are detached (in a background exec). */
{
    Tcl_Channel channel;
    char channelName[20];
    int channelId;
    PipeState *statePtr = (PipeState *) ckalloc((unsigned) sizeof(PipeState));

    statePtr->readFile = readFile;
    statePtr->writeFile = writeFile;
    statePtr->errorFile = errorFile;
    statePtr->numPids = numPids;
    statePtr->pidPtr = pidPtr;
    statePtr->isNonBlocking = 0;

    /*
     * Use one of the fds associated with the channel as the
     * channel id.
     */

    if (readFile) {
	channelId = (int) Tcl_GetFileInfo(readFile, NULL);
    } else if (writeFile) {
	channelId = (int) Tcl_GetFileInfo(writeFile, NULL);
    } else if (errorFile) {
	channelId = (int) Tcl_GetFileInfo(errorFile, NULL);
    } else {
	channelId = 0;
    }

    /*
     * For backward compatibility with previous versions of Tcl, we
     * use "file%d" as the base name for pipes even though it would
     * be more natural to use "pipe%d".
     */

    sprintf(channelName, "file%d", channelId);
    channel = Tcl_CreateChannel(&pipeChannelType, channelName, readFile,
	    writeFile, (ClientData) statePtr);

    if (channel == NULL) {

        /*
         * pidPtr will be freed by the caller if the return value is NULL.
         */
        
	ckfree((char *)statePtr);
    }
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PidCmd --
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
Tcl_PidCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to get pids for. */
    Tcl_ChannelType *chanTypePtr;	/* The type of that channel. */
    PipeState *pipePtr;			/* The pipe state. */
    int i;				/* Loops over PIDs attached to the
                                         * pipe. */
    char string[50];			/* Temp buffer for string rep. of
                                         * PIDs attached to the pipe. */

    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ?channelId?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 1) {
	sprintf(interp->result, "%ld", (long) getpid());
    } else {
        chan = Tcl_GetChannel(interp, argv[1], NULL);
        if (chan == (Tcl_Channel) NULL) {
	    return TCL_ERROR;
	}
	chanTypePtr = Tcl_GetChannelType(chan);
	if (chanTypePtr != &pipeChannelType) {
	    return TCL_OK;
	}
        pipePtr = (PipeState *) Tcl_GetChannelInstanceData(chan);
        for (i = 0; i < pipePtr->numPids; i++) {
	    sprintf(string, "%d", pipePtr->pidPtr[i]);
	    Tcl_AppendElement(interp, string);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpBlockModeProc --
 *
 *	This procedure is invoked by the generic IO level to set blocking
 *	and nonblocking mode on a TCP socket based channel.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or nonblocking mode.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpBlockModeProc(instanceData, inFile, outFile, mode)
    ClientData instanceData;		/* Socket state. */
    Tcl_File inFile, outFile;		/* Input, output files for channel. */
    int mode;				/* The mode to set. Can be one of
                                         * TCL_MODE_BLOCKING or
                                         * TCL_MODE_NONBLOCKING. */
{
    TcpState *statePtr;
    
    statePtr = (TcpState *) instanceData;
    if (mode == TCL_MODE_BLOCKING) {
        statePtr->flags &= (~(TCP_ASYNC_SOCKET));
    } else {
        statePtr->flags |= TCP_ASYNC_SOCKET;
    }
    return FileBlockModeProc(instanceData, inFile, outFile, mode);
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForConnect --
 *
 *	Waits for a connection on an asynchronously opened socket to
 *	be completed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The socket is connected after this function returns.
 *
 *----------------------------------------------------------------------
 */

static int
WaitForConnect(statePtr, fileToWaitFor, errorCodePtr)
    TcpState *statePtr;		/* State of the socket. */
    Tcl_File fileToWaitFor;	/* File to wait on to become connected. */
    int *errorCodePtr;		/* Where to store errors? */
{
    int sock;			/* The socket itself. */
    int timeOut;		/* How long to wait. */
    int state;			/* Of calling TclWaitForFile. */
    int flags;			/* fcntl flags for the socket. */

    /*
     * If an asynchronous connect is in progress, attempt to wait for it
     * to complete before reading.
     */
    
    if (statePtr->flags & TCP_ASYNC_CONNECT) {
        if (statePtr->flags & TCP_ASYNC_SOCKET) {
            timeOut = 0;
        } else {
            timeOut = -1;
        }
        errno = 0;
        state = TclWaitForFile(fileToWaitFor, TCL_WRITABLE | TCL_EXCEPTION,
                timeOut);
        if (!(statePtr->flags & TCP_ASYNC_SOCKET)) {
            sock = (int) Tcl_GetFileInfo(statePtr->sock, NULL);
            flags = fcntl(sock, F_GETFL);
            flags &= (~(O_NONBLOCK));
            (void) fcntl(sock, F_SETFL, flags);
        }
        if (state & TCL_EXCEPTION) {
            return -1;
        }
        if (state & TCL_WRITABLE) {
            statePtr->flags &= (~(TCP_ASYNC_CONNECT));
        } else if (timeOut == 0) {
            *errorCodePtr = errno = EWOULDBLOCK;
            return -1;
        }
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpInputProc --
 *
 *	This procedure is invoked by the generic IO level to read input
 *	from a TCP socket based channel.
 *
 *	NOTE: We cannot share code with FilePipeInputProc because here
 *	we must use recv to obtain the input from the channel, not read.
 *
 * Results:
 *	The number of bytes read is returned or -1 on error. An output
 *	argument contains the POSIX error code on error, or zero if no
 *	error occurred.
 *
 * Side effects:
 *	Reads input from the input device of the channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpInputProc(instanceData, inFile, buf, bufSize, errorCodePtr)
    ClientData instanceData;		/* Socket state. */
    Tcl_File inFile;			/* Input device for channel. */
    char *buf;				/* Where to store data read. */
    int bufSize;			/* How much space is available
                                         * in the buffer? */
    int *errorCodePtr;			/* Where to store error code. */
{
    TcpState *statePtr;			/* The state of the socket. */
    int sock;				/* The OS handle. */
    int bytesRead;			/* How many bytes were read? */
    int state;				/* Of waiting for connection. */

    *errorCodePtr = 0;
    sock = (int) Tcl_GetFileInfo(inFile, NULL);
    statePtr = (TcpState *) instanceData;

    state = WaitForConnect(statePtr, inFile, errorCodePtr);
    if (state != 0) {
        return -1;
    }
    bytesRead = recv(sock, buf, bufSize, 0);
    if (bytesRead > -1) {
        return bytesRead;
    }
    if (errno == ECONNRESET) {

        /*
         * Turn ECONNRESET into a soft EOF condition.
         */
        
        return 0;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpOutputProc --
 *
 *	This procedure is invoked by the generic IO level to write output
 *	to a TCP socket based channel.
 *
 *	NOTE: We cannot share code with FilePipeOutputProc because here
 *	we must use send, not write, to get reliable error reporting.
 *
 * Results:
 *	The number of bytes written is returned. An output argument is
 *	set to a POSIX error code if an error occurred, or zero.
 *
 * Side effects:
 *	Writes output on the output device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
TcpOutputProc(instanceData, outFile, buf, toWrite, errorCodePtr)
    ClientData instanceData;		/* Socket state. */
    Tcl_File outFile;			/* Output device for channel. */
    char *buf;				/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCodePtr;			/* Where to store error code. */
{
    TcpState *statePtr;
    int written;
    int sock;				/* OS level socket. */
    int state;				/* Of waiting for connection. */

    *errorCodePtr = 0;
    sock = (int) Tcl_GetFileInfo(outFile, NULL);
    statePtr = (TcpState *) instanceData;
    state = WaitForConnect(statePtr, outFile, errorCodePtr);
    if (state != 0) {
        return -1;
    }
    written = send(sock, buf, toWrite, 0);
    if (written > -1) {
        return written;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpCloseProc --
 *
 *	This procedure is invoked by the generic IO level to perform
 *	channel-type-specific cleanup when a TCP socket based channel
 *	is closed.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the socket of the channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpCloseProc(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* The socket to close. */
    Tcl_Interp *interp;		/* For error reporting - unused. */
    Tcl_File inFile, outFile;	/* Unused. */
{
    TcpState *statePtr;
    Tcl_File sockFile;
    int sock;
    int errorCode = 0;

    statePtr = (TcpState *) instanceData;
    sockFile = statePtr->sock;
    sock = (int) Tcl_GetFileInfo(sockFile, NULL);
    
    /*
     * Delete a file handler that may be active for this socket if this
     * is a server socket - the file handler was created automatically
     * by Tcl as part of the mechanism to accept new client connections.
     * Channel handlers are already deleted in the generic IO channel
     * closing code that called this function, so we do not have to
     * delete them here.
     */
    
    Tcl_DeleteFileHandler(sockFile);

    ckfree((char *) statePtr);
    
    /*
     * We assume that inFile==outFile==sockFile and so
     * we only clean up sockFile.
     */

    Tcl_FreeFile(sockFile);

    if (close(sock) < 0) {
	errorCode = errno;
    }

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetOptionProc --
 *
 *	Computes an option value for a TCP socket based channel, or a
 *	list of all options and their values.
 *
 *	Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *	A standard Tcl result. The value of the specified option or a
 *	list of all options and	their values is returned in the
 *	supplied DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetOptionProc(instanceData, optionName, dsPtr)
    ClientData instanceData;		/* Socket state. */
    char *optionName;			/* Name of the option to
                                         * retrieve the value for, or
                                         * NULL to get all options and
                                         * their values. */
    Tcl_DString *dsPtr;			/* Where to store the computed
                                         * value; initialized by caller. */
{
    TcpState *statePtr;
    struct sockaddr_in sockname;
    struct sockaddr_in peername;
    struct hostent *hostEntPtr;
    int sock;
    int size = sizeof(struct sockaddr_in);
    size_t len = 0;
    char buf[128];

    statePtr = (TcpState *) instanceData;
    sock = (int) Tcl_GetFileInfo(statePtr->sock, NULL);
    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'p') &&
                    (strncmp(optionName, "-peername", len) == 0))) {
        if (getpeername(sock, (struct sockaddr *) &peername, &size) >= 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-peername");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr, inet_ntoa(peername.sin_addr));
            hostEntPtr = gethostbyaddr((char *) &(peername.sin_addr),
                    sizeof(peername.sin_addr), AF_INET);
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr, inet_ntoa(peername.sin_addr));
            }
            sprintf(buf, "%d", ntohs(peername.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        }
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 's') &&
                    (strncmp(optionName, "-sockname", len) == 0))) {
        if (getsockname(sock, (struct sockaddr *) &sockname, &size) >= 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-sockname");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr, inet_ntoa(sockname.sin_addr));
            hostEntPtr = gethostbyaddr((char *) &(sockname.sin_addr),
                    sizeof(peername.sin_addr), AF_INET);
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr, inet_ntoa(sockname.sin_addr));
            }
            sprintf(buf, "%d", ntohs(sockname.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        }
    }

    if (len > 0) {
        Tcl_SetErrno(EINVAL);
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocket --
 *
 *	This function opens a new socket in client or server mode
 *	and initializes the TcpState structure.
 *
 * Results:
 *	Returns a new TcpState, or NULL with an error in interp->result,
 *	if interp is not NULL.
 *
 * Side effects:
 *	Opens a socket.
 *
 *----------------------------------------------------------------------
 */

static TcpState *
CreateSocket(interp, port, host, server, myaddr, myport, async)
    Tcl_Interp *interp;		/* For error reporting; can be NULL. */
    int port;			/* Port number to open. */
    char *host;			/* Name of host on which to open port.
				 * NULL implies INADDR_ANY */
    int server;			/* 1 if socket should be a server socket,
				 * else 0 for a client socket. */
    char *myaddr;		/* Optional client-side address */
    int myport;			/* Optional client-side port */
    int async;			/* If nonzero and creating a client socket,
                                 * attempt to do an async connect. Otherwise
                                 * do a synchronous connect or bind. */
{
    int status, sock, asyncConnect, curState, origState;
    struct sockaddr_in sockaddr;	/* socket address */
    struct sockaddr_in mysockaddr;	/* Socket address for client */
    TcpState *statePtr;

    sock = -1;
    origState = 0;
    if (! CreateSocketAddress(&sockaddr, host, port)) {
	goto addressError;
    }
    if ((myaddr != NULL || myport != 0) &&
	    ! CreateSocketAddress(&mysockaddr, myaddr, myport)) {
	goto addressError;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	goto addressError;
    }

    /*
     * Set kernel space buffering
     */

    TclSockMinimumBuffers(sock, SOCKET_BUFSIZE);

    asyncConnect = 0;
    status = 0;
    if (server) {

	/*
	 * Set up to reuse server addresses automatically and bind to the
	 * specified port.
	 */
    
	status = 1;
	(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &status,
		sizeof(status));
	status = bind(sock, (struct sockaddr *) &sockaddr,
                sizeof(struct sockaddr));
	if (status != -1) {
	    status = listen(sock, TCL_LISTEN_LIMIT);
	} 
    } else {
	if (myaddr != NULL || myport != 0) { 
	    status = 1;
	    (void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &status,
		    sizeof(status));
	    status = bind(sock, (struct sockaddr *) &mysockaddr,
		    sizeof(struct sockaddr));
	    if (status < 0) {
		goto bindError;
	    }
	}

	/*
	 * Attempt to connect. The connect may fail at present with an
	 * EINPROGRESS but at a later time it will complete. The caller
	 * will set up a file handler on the socket if she is interested in
	 * being informed when the connect completes.
	 */

        if (async) {
            origState = fcntl(sock, F_GETFL);
            curState = origState | O_NONBLOCK;
            status = fcntl(sock, F_SETFL, curState);
        } else {
            status = 0;
        }
        if (status > -1) {
            status = connect(sock, (struct sockaddr *) &sockaddr,
                    sizeof(sockaddr));
            if (status < 0) {
                if (errno == EINPROGRESS) {
                    asyncConnect = 1;
                    status = 0;
                }
            }
        }
    }

bindError:
    if (status < 0) {
        if (interp != NULL) {
            Tcl_AppendResult(interp, "couldn't open socket: ",
                    Tcl_PosixError(interp), (char *) NULL);
        }
        if (sock != -1) {
            close(sock);
        }
        return NULL;
    }

    /*
     * Allocate a new TcpState for this socket.
     */

    statePtr = (TcpState *) ckalloc((unsigned) sizeof(TcpState));
    statePtr->flags = 0;
    if (asyncConnect) {
        statePtr->flags = TCP_ASYNC_CONNECT;
    }
    statePtr->sock = Tcl_GetFile((ClientData) sock, TCL_UNIX_FD);
    
    return statePtr;

addressError:
    if (sock != -1) {
        close(sock);
    }
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_PosixError(interp), (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a sockaddr structure for a host and port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to
 *	an IP address.
 *
 * Side effects:
 *	Fills in the *sockaddrPtr structure.
 *
 *----------------------------------------------------------------------
 */

static int
CreateSocketAddress(sockaddrPtr, host, port)
    struct sockaddr_in *sockaddrPtr;	/* Socket address */
    char *host;				/* Host.  NULL implies INADDR_ANY */
    int port;				/* Port number */
{
    struct hostent *hostent;		/* Host database entry */
    struct in_addr addr;		/* For 64/32 bit madness */

    (void) memset((VOID *) sockaddrPtr, '\0', sizeof(struct sockaddr_in));
    sockaddrPtr->sin_family = AF_INET;
    sockaddrPtr->sin_port = htons((unsigned short) (port & 0xFFFF));
    if (host == NULL) {
	addr.s_addr = INADDR_ANY;
    } else {
        addr.s_addr = inet_addr(host);
        if (addr.s_addr == -1) {
            hostent = gethostbyname(host);
            if (hostent != NULL) {
                memcpy((VOID *) &addr,
                        (VOID *) hostent->h_addr_list[0],
                        (size_t) hostent->h_length);
            } else {
#ifdef	EHOSTUNREACH
                errno = EHOSTUNREACH;
#else
#ifdef ENXIO
                errno = ENXIO;
#endif
#endif
                return 0;	/* error */
            }
        }
    }
        
    /*
     * NOTE: On 64 bit machines the assignment below is rumored to not
     * do the right thing. Please report errors related to this if you
     * observe incorrect behavior on 64 bit machines such as DEC Alphas.
     * Should we modify this code to do an explicit memcpy?
     */

    sockaddrPtr->sin_addr.s_addr = addr.s_addr;
    return 1;	/* Success. */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpClient --
 *
 *	Opens a TCP client socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a client socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpClient(interp, port, host, myaddr, myport, async)
    Tcl_Interp *interp;			/* For error reporting; can be NULL. */
    int port;				/* Port number to open. */
    char *host;				/* Host on which to open port. */
    char *myaddr;			/* Client-side address */
    int myport;				/* Client-side port */
    int async;				/* If nonzero, attempt to do an
                                         * asynchronous connect. Otherwise
                                         * we do a blocking connect. */
{
    Tcl_Channel chan;
    TcpState *statePtr;
    char channelName[20];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    statePtr = CreateSocket(interp, port, host, 0, myaddr, myport, async);
    if (statePtr == NULL) {
	return NULL;
    }

    statePtr->acceptProc = NULL;
    statePtr->acceptProcData = (ClientData) NULL;

    sprintf(channelName, "sock%d",
	    (int) Tcl_GetFileInfo(statePtr->sock, NULL));

    chan = Tcl_CreateChannel(&tcpChannelType, channelName, statePtr->sock,
	    statePtr->sock, (ClientData) statePtr);
    if (Tcl_SetChannelOption(interp, chan, "-translation", "auto crlf") ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return NULL;
    }
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeTcpClientChannel --
 *
 *	Creates a Tcl_Channel from an existing client TCP socket.
 *
 * Results:
 *	The Tcl_Channel wrapped around the preexisting TCP socket.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeTcpClientChannel(sock)
    ClientData sock;		/* The socket to wrap up into a channel. */
{
    TcpState *statePtr;
    Tcl_File sockFile;
    char channelName[20];
    Tcl_Channel chan;

    sockFile = Tcl_GetFile(sock, TCL_UNIX_FD);
    statePtr = (TcpState *) ckalloc((unsigned) sizeof(TcpState));
    statePtr->sock = sockFile;
    statePtr->acceptProc = NULL;
    statePtr->acceptProcData = (ClientData) NULL;

    sprintf(channelName, "sock%d", (int) sock);
    
    chan = Tcl_CreateChannel(&tcpChannelType, channelName, sockFile, sockFile,
            (ClientData) statePtr);
    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, chan, "-translation",
            "auto crlf") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return NULL;
    }
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpServer --
 *
 *	Opens a TCP server socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed. If an error occurred, an
 *	error message is left in interp->result if interp is
 *	not NULL.
 *
 * Side effects:
 *	Opens a server socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpServer(interp, port, myHost, acceptProc, acceptProcData)
    Tcl_Interp *interp;			/* For error reporting - may be
                                         * NULL. */
    int port;				/* Port number to open. */
    char *myHost;			/* Name of local host. */
    Tcl_TcpAcceptProc *acceptProc;	/* Callback for accepting connections
                                         * from new clients. */
    ClientData acceptProcData;		/* Data for the callback. */
{
    Tcl_Channel chan;
    TcpState *statePtr;
    char channelName[20];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    statePtr = CreateSocket(interp, port, myHost, 1, NULL, 0, 0);
    if (statePtr == NULL) {
	return NULL;
    }

    statePtr->acceptProc = acceptProc;
    statePtr->acceptProcData = acceptProcData;

    /*
     * Set up the callback mechanism for accepting connections
     * from new clients.
     */

    Tcl_CreateFileHandler(statePtr->sock, TCL_READABLE, TcpAccept,
            (ClientData) statePtr);
    sprintf(channelName, "sock%d",
	    (int) Tcl_GetFileInfo(statePtr->sock, NULL));
    chan = Tcl_CreateChannel(&tcpChannelType, channelName, NULL, NULL,
            (ClientData) statePtr);
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpAccept --
 *	Accept a TCP socket connection.  This is called by the event loop.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new connection socket. Calls the registered callback
 *	for the connection acceptance mechanism.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TcpAccept(data, mask)
    ClientData data;			/* Callback token. */
    int mask;				/* Not used. */
{
    TcpState *sockState;		/* Client data of server socket. */
    int newsock;			/* The new client socket */
    Tcl_File newFile;			/* Its file. */
    TcpState *newSockState;		/* State for new socket. */
    struct sockaddr_in addr;		/* The remote address */
    int len;				/* For accept interface */
    Tcl_Channel chan;			/* Channel instance created. */
    char channelName[20];

    sockState = (TcpState *) data;

    len = sizeof(struct sockaddr_in);
    newsock = accept((int) Tcl_GetFileInfo(sockState->sock, NULL),
	    (struct sockaddr *)&addr, &len);
    if (newsock < 0) {
        return;
    }
    
    newFile = Tcl_GetFile((ClientData) newsock, TCL_UNIX_FD);
    if (newFile) {
        newSockState = (TcpState *) ckalloc((unsigned) sizeof(TcpState));

        newSockState->flags = 0;
        newSockState->sock = newFile;
        newSockState->acceptProc = (Tcl_TcpAcceptProc *) NULL;
        newSockState->acceptProcData = (ClientData) NULL;
        
        sprintf(channelName, "sock%d", (int) newsock);
        chan = Tcl_CreateChannel(&tcpChannelType, channelName, newFile,
                newFile, (ClientData) newSockState);
        if (chan == (Tcl_Channel) NULL) {
            ckfree((char *) newSockState);
            close(newsock);
            Tcl_FreeFile(newFile);
	} else {
            if (Tcl_SetChannelOption((Tcl_Interp *) NULL, chan, "-translation",
                    "auto crlf") == TCL_ERROR) {
                Tcl_Close((Tcl_Interp *) NULL, chan);
            }
            if (sockState->acceptProc != (Tcl_TcpAcceptProc *) NULL) {
                (sockState->acceptProc) (sockState->acceptProcData, chan,
                        inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetDefaultStdChannel --
 *
 *	Creates channels for standard input, standard output or standard
 *	error output if they do not already exist.
 *
 * Results:
 *	Returns the specified default standard channel, or NULL.
 *
 * Side effects:
 *	May cause the creation of a standard channel and the underlying
 *	file.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclGetDefaultStdChannel(type)
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR. */
{
    Tcl_Channel channel = NULL;
    int fd = 0;			/* Initializations needed to prevent */
    int mode = 0;		/* compiler warning (used before set). */
    char *bufMode = NULL;

    switch (type) {
        case TCL_STDIN:
            if ((lseek(0, (off_t) 0, SEEK_CUR) == -1) &&
                    (errno == EBADF)) {
                return (Tcl_Channel) NULL;
            }
	    fd = 0;
	    mode = TCL_READABLE;
            bufMode = "line";
            break;
        case TCL_STDOUT:
            if ((lseek(1, (off_t) 0, SEEK_CUR) == -1) &&
                    (errno == EBADF)) {
                return (Tcl_Channel) NULL;
            }
	    fd = 1;
	    mode = TCL_WRITABLE;
            bufMode = "line";
            break;
        case TCL_STDERR:
            if ((lseek(2, (off_t) 0, SEEK_CUR) == -1) &&
                    (errno == EBADF)) {
                return (Tcl_Channel) NULL;
            }
	    fd = 2;
	    mode = TCL_WRITABLE;
	    bufMode = "none";
            break;
	default:
	    panic("TclGetDefaultStdChannel: Unexpected channel type");
	    break;
    }

    channel = Tcl_MakeFileChannel((ClientData) fd, (ClientData) fd, mode);

    /*
     * Set up the normal channel options for stdio handles.
     */

    if (Tcl_SetChannelOption(NULL, channel, "-translation", "auto") ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return NULL;
    }
    if (Tcl_SetChannelOption(NULL, channel, "-buffering", bufMode) ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return NULL;
    }
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TclClosePipeFile --
 *
 *	This function is a simple wrapper for close on a file or
 *	pipe handle. Called in the generic command pipeline cleanup
 *	code to do platform specific closing of the files associated
 *	with the command channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Closes the fd and frees the Tcl_File.
 *
 *----------------------------------------------------------------------
 */

void
TclClosePipeFile(file)
    Tcl_File file;
{
    int fd = (int) Tcl_GetFileInfo(file, NULL);
    close(fd);
    Tcl_FreeFile(file);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetOpenFile --
 *
 *	Given a name of a channel registered in the given interpreter,
 *	returns a FILE * for it.
 *
 * Results:
 *	A standard Tcl result. If the channel is registered in the given
 *	interpreter and it is managed by the "file" channel driver, and
 *	it is open for the requested mode, then the output parameter
 *	filePtr is set to a FILE * for the underlying file. On error, the
 *	filePtr is not set, TCL_ERROR is returned and an error message is
 *	left in interp->result.
 *
 * Side effects:
 *	May invoke fdopen to create the FILE * for the requested file.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetOpenFile(interp, string, forWriting, checkUsage, filePtr)
    Tcl_Interp *interp;		/* Interpreter in which to find file. */
    char *string;		/* String that identifies file. */
    int forWriting;		/* 1 means the file is going to be used
				 * for writing, 0 means for reading. */
    int checkUsage;		/* 1 means verify that the file was opened
				 * in a mode that allows the access specified
				 * by "forWriting". Ignored, we always
                                 * check that the channel is open for the
                                 * requested mode. */
    ClientData *filePtr;	/* Store pointer to FILE structure here. */
{
    Tcl_Channel chan;
    int chanMode;
    Tcl_ChannelType *chanTypePtr;
    Tcl_File tf;
    int fd;
    FILE *f;
    
    chan = Tcl_GetChannel(interp, string, &chanMode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((forWriting) && ((chanMode & TCL_WRITABLE) == 0)) {
        Tcl_AppendResult(interp,
                "\"", string, "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    } else if ((!(forWriting)) && ((chanMode & TCL_READABLE) == 0)) {
        Tcl_AppendResult(interp,
                "\"", string, "\" wasn't opened for reading", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * We allow creating a FILE * out of file based, pipe based and socket
     * based channels. We currently do not allow any other channel types,
     * because it is likely that stdio will not know what to do with them.
     */

    chanTypePtr = Tcl_GetChannelType(chan);
    if ((chanTypePtr == &fileChannelType) || (chanTypePtr == &pipeChannelType)
            || (chanTypePtr == &tcpChannelType)) {
        tf = Tcl_GetChannelFile(chan,
                (forWriting ? TCL_WRITABLE : TCL_READABLE));
        fd = (int) Tcl_GetFileInfo(tf, NULL);

        /*
         * The call to fdopen below is probably dangerous, since it will
         * truncate an existing file if the file is being opened
         * for writing....
         */
        
        f = fdopen(fd, (forWriting ? "w" : "r"));
        if (f == NULL) {
            Tcl_AppendResult(interp, "cannot get a FILE * for \"", string,
                    "\"", (char *) NULL);
            return TCL_ERROR;
        }
        *filePtr = (ClientData) f;
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "\"", string,
            "\" cannot be used to get a FILE * - unsupported type",
            (char *) NULL);
    return TCL_ERROR;        
}
