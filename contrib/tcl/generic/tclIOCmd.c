/* 
 * tclIOCmd.c --
 *
 *	Contains the definitions of most of the Tcl commands relating to IO.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclIOCmd.c 1.96 96/05/10 15:20:56
 */

#include	"tclInt.h"
#include	"tclPort.h"

/*
 * Return at most this number of bytes in one call to Tcl_Read:
 */

#define	TCL_READ_CHUNK_SIZE	4096

/*
 * Callback structure for accept callback in a TCP server.
 */

typedef struct AcceptCallback {
    char *script;			/* Script to invoke. */
    Tcl_Interp *interp;			/* Interpreter in which to run it. */
} AcceptCallback;

/*
 * Static functions for this file:
 */

static void	AcceptCallbackProc _ANSI_ARGS_((ClientData callbackData,
	            Tcl_Channel chan, char *address, int port));
static void	RegisterTcpServerInterpCleanup _ANSI_ARGS_((Tcl_Interp *interp,
	            AcceptCallback *acceptCallbackPtr));
static void	TcpAcceptCallbacksDeleteProc _ANSI_ARGS_((
		    ClientData clientData, Tcl_Interp *interp));
static void	TcpServerCloseProc _ANSI_ARGS_((ClientData callbackData));
static void	UnregisterTcpServerInterpCleanupProc _ANSI_ARGS_((
		    Tcl_Interp *interp, AcceptCallback *acceptCallbackPtr));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PutsCmd --
 *
 *	This procedure is invoked to process the "puts" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Produces output on a channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_PutsCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to puts on. */
    int i;				/* Counter. */
    int newline;			/* Add a newline at end? */
    char *channelId;			/* Name of channel for puts. */
    int result;				/* Result of puts operation. */
    int mode;				/* Mode in which channel is opened. */

    i = 1;
    newline = 1;
    if ((argc >= 2) && (strcmp(argv[1], "-nonewline") == 0)) {
	newline = 0;
	i++;
    }
    if ((i < (argc-3)) || (i >= argc)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-nonewline? ?channelId? string\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below provides backwards compatibility with an old
     * form of the command that is no longer recommended or documented.
     */

    if (i == (argc-3)) {
	if (strncmp(argv[i+2], "nonewline", strlen(argv[i+2])) != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", argv[i+2],
		    "\": should be \"nonewline\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newline = 0;
    }
    if (i == (argc-1)) {
	channelId = "stdout";
    } else {
	channelId = argv[i];
	i++;
    }
    chan = Tcl_GetChannel(interp, channelId, &mode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((mode & TCL_WRITABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", channelId,
                "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    }
    
    result = Tcl_Write(chan, argv[i], -1);
    if (result < 0) {
        goto error;
    }
    if (newline != 0) {
        result = Tcl_Write(chan, "\n", 1);
        if (result < 0) {
            goto error;
        }
    }
    return TCL_OK;
error:
    Tcl_AppendResult(interp, "error writing \"", Tcl_GetChannelName(chan),
	    "\": ", Tcl_PosixError(interp), (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FlushCmd --
 *
 *	This procedure is called to process the Tcl "flush" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May cause output to appear on the specified channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_FlushCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to flush on. */
    int result;				/* Result of call to channel
                                         * level function. */
    int mode;				/* Mode in which channel is opened. */

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelId\"", (char *) NULL);
	return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], &mode);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    if ((mode & TCL_WRITABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", argv[1],
                "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    }
    
    result = Tcl_Flush(chan);
    if (result != TCL_OK) {
        Tcl_AppendResult(interp, "error flushing \"", Tcl_GetChannelName(chan),
		"\": ", Tcl_PosixError(interp), (char *) NULL);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetsCmd --
 *
 *	This procedure is called to process the Tcl "gets" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May consume input from channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_GetsCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to read from. */
    char *varName;			/* Assign to this variable? */
    char buf[128];			/* Buffer to store string
                                         * representation of how long
                                         * a line was read. */
    Tcl_DString ds;			/* Dynamic string to hold the
                                         * buffer for the line just read. */
    int lineLen;			/* Length of line just read. */
    int mode;				/* Mode in which channel is opened. */

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelId ?varName?\"", (char *) NULL);
	return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], &mode);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", argv[1],
                "\" wasn't opened for reading", (char *) NULL);
        return TCL_ERROR;
    }
    
    if (argc != 3) {
        varName = (char *) NULL;
    } else {
        varName = argv[2];
    }
    Tcl_DStringInit(&ds);
    lineLen = Tcl_Gets(chan, &ds);
    if (lineLen < 0) {
        if (!Tcl_Eof(chan) && !Tcl_InputBlocked(chan)) {
            Tcl_DStringFree(&ds);
            Tcl_AppendResult(interp, "error reading \"",
		    Tcl_GetChannelName(chan), "\": ", Tcl_PosixError(interp),
		    (char *) NULL);
            return TCL_ERROR;
        }
        lineLen = -1;
    }
    if (varName == (char *) NULL) {
        Tcl_DStringResult(interp, &ds);
    } else {
        if (Tcl_SetVar(interp, varName, Tcl_DStringValue(&ds),
                TCL_LEAVE_ERR_MSG) == NULL) {
            Tcl_DStringFree(&ds);
            return TCL_ERROR;
        }
        Tcl_ResetResult(interp);
        sprintf(buf, "%d", lineLen);
        Tcl_AppendResult(interp, buf, (char *) NULL);
    }
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ReadCmd --
 *
 *	This procedure is invoked to process the Tcl "read" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May consume input from channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ReadCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to read from. */
    int newline, i;			/* Discard newline at end? */
    int toRead;				/* How many bytes to read? */
    int toReadNow;			/* How many bytes to attempt to
                                         * read in the current iteration? */
    int charactersRead;			/* How many characters were read? */
    int charactersReadNow;		/* How many characters were read
                                         * in this iteration? */
    int mode;				/* Mode in which channel is opened. */
    Tcl_DString ds;			/* Used to accumulate the data
                                         * read by Tcl_Read. */
    int bufSize;			/* Channel buffer size; used to decide
                                         * in what chunk sizes to read from
                                         * the channel. */

    if ((argc != 2) && (argc != 3)) {
argerror:
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelId ?numBytes?\" or \"", argv[0],
		" ?-nonewline? channelId\"", (char *) NULL);
	return TCL_ERROR;
    }
    i = 1;
    newline = 0;
    if (strcmp(argv[i], "-nonewline") == 0) {
	newline = 1;
	i++;
    }

    if (i == argc) {
        goto argerror;
    }

    chan = Tcl_GetChannel(interp, argv[i], &mode);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", argv[i],
                "\" wasn't opened for reading", (char *) NULL);
        return TCL_ERROR;
    }
    
    i++;	/* Consumed channel name. */

    /*
     * Compute how many bytes to read, and see whether the final
     * newline should be dropped.
     */

    toRead = INT_MAX;
    if (i < argc) {
        if (isdigit((unsigned char) (argv[i][0]))) {
            if (Tcl_GetInt(interp, argv[i], &toRead) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(argv[i], "nonewline") == 0) {
            newline = 1;
        } else {
            Tcl_AppendResult(interp, "bad argument \"", argv[i],
                    "\": should be \"nonewline\"", (char *) NULL);
            return TCL_ERROR;
        }
    }

    bufSize = Tcl_GetChannelBufferSize(chan);
    Tcl_DStringInit(&ds);
    for (charactersRead = 0; charactersRead < toRead; ) {
        toReadNow = toRead - charactersRead;
        if (toReadNow > bufSize) {
            toReadNow = bufSize;
        }
        Tcl_DStringSetLength(&ds, charactersRead + toReadNow);
        charactersReadNow =
            Tcl_Read(chan, Tcl_DStringValue(&ds) + charactersRead, toReadNow);
        if (charactersReadNow < 0) {
            Tcl_DStringFree(&ds);
            Tcl_AppendResult(interp, "error reading \"",
		    Tcl_GetChannelName(chan), "\": ",
		    Tcl_PosixError(interp), (char *) NULL);
            return TCL_ERROR;
        }

        /*
         * If we had a short read it means that we have either EOF
         * or BLOCKED on the channel, so break out.
         */
        
        charactersRead += charactersReadNow;
        if (charactersReadNow < toReadNow) {
            break;	/* Out of "for" loop. */
        }
    }

    /*
     * Tcl_Read does not put a NULL at the end of the string, so we must
     * do it here.
     */
    
    Tcl_DStringSetLength(&ds, charactersRead);
    Tcl_DStringResult(interp, &ds);
    Tcl_DStringFree(&ds);

    /*
     * If requested, remove the last newline in the channel if at EOF.
     */
    
    if ((charactersRead > 0) && (newline) &&
            (interp->result[charactersRead-1] == '\n')) {
        interp->result[charactersRead-1] = '\0';
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclUnsupported0Cmd --
 *
 *	This procedure is invoked to process the Tcl "unsupported0" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May copy a chunk from one channel to another.
 *
 *----------------------------------------------------------------------
 */

int
TclUnsupported0Cmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Interpreter in which both channels
                                         * are defined. */
    int argc;				/* How many arguments? */
    char **argv;			/* The argument strings. */
{
    Tcl_Channel inChan, outChan;
    int requested;
    char *bufPtr;
    int actuallyRead, actuallyWritten, totalRead, toReadNow, mode;
    
    /*
     * Assume we want to copy the entire channel.
     */
    
    requested = INT_MAX;

    if ((argc < 3) || (argc > 4)) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
                argv[0], " inChanId outChanId ?chunkSize?\"", (char *) NULL);
        return TCL_ERROR;
    }
    inChan = Tcl_GetChannel(interp, argv[1], &mode);
    if (inChan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", argv[1],
                "\" wasn't opened for reading", (char *) NULL);
        return TCL_ERROR;
    }
    outChan = Tcl_GetChannel(interp, argv[2], &mode);
    if (outChan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((mode & TCL_WRITABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", argv[2],
                "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    }
    
    if (argc == 4) {
	if (Tcl_GetInt(interp, argv[3], &requested) != TCL_OK) {
	    return TCL_ERROR;
	}
        if (requested < 0) {
            requested = INT_MAX;
        }
    }

    bufPtr = ckalloc((unsigned) TCL_READ_CHUNK_SIZE);
    for (totalRead = 0;
            requested > 0;
            totalRead += actuallyRead, requested -= actuallyRead) {
        toReadNow = requested;
        if (toReadNow > TCL_READ_CHUNK_SIZE) {
            toReadNow = TCL_READ_CHUNK_SIZE;
        }
        actuallyRead = Tcl_Read(inChan, bufPtr, toReadNow);
        if (actuallyRead < 0) {
            ckfree(bufPtr);
	    Tcl_AppendResult(interp, argv[0], ": ", Tcl_GetChannelName(inChan),
		Tcl_PosixError(interp), (char *) NULL);
            return TCL_ERROR;
        }
        if (actuallyRead == 0) {
            ckfree(bufPtr);
            sprintf(interp->result, "%d", totalRead);
            return TCL_OK;
        }
        actuallyWritten = Tcl_Write(outChan, bufPtr, actuallyRead);
        if (actuallyWritten < 0) {
            ckfree(bufPtr);
	    Tcl_AppendResult(interp, argv[0], ": ", Tcl_GetChannelName(outChan),
		Tcl_PosixError(interp), (char *) NULL);
            return TCL_ERROR;
        }
    }
    ckfree(bufPtr);
    
    sprintf(interp->result, "%d", totalRead);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SeekCmd --
 *
 *	This procedure is invoked to process the Tcl "seek" command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Moves the position of the access point on the specified channel.
 *	May flush queued output.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_SeekCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to tell on. */
    int offset, mode;			/* Where to seek? */
    int result;				/* Of calling Tcl_Seek. */

    if ((argc != 3) && (argc != 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelId offset ?origin?\"", (char *) NULL);
	return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], NULL);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &offset) != TCL_OK) {
	return TCL_ERROR;
    }
    mode = SEEK_SET;
    if (argc == 4) {
	size_t length;
	int c;

	length = strlen(argv[3]);
	c = argv[3][0];
	if ((c == 's') && (strncmp(argv[3], "start", length) == 0)) {
	    mode = SEEK_SET;
	} else if ((c == 'c') && (strncmp(argv[3], "current", length) == 0)) {
	    mode = SEEK_CUR;
	} else if ((c == 'e') && (strncmp(argv[3], "end", length) == 0)) {
	    mode = SEEK_END;
	} else {
	    Tcl_AppendResult(interp, "bad origin \"", argv[3],
		    "\": should be start, current, or end", (char *) NULL);
	    return TCL_ERROR;
	}
    }

    result = Tcl_Seek(chan, offset, mode);
    if (result < 0) {
        Tcl_AppendResult(interp, "error during seek on \"", 
		Tcl_GetChannelName(chan), "\": ",
                Tcl_PosixError(interp), (char *) NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TellCmd --
 *
 *	This procedure is invoked to process the Tcl "tell" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_TellCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to tell on. */

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelId\"", (char *) NULL);
	return TCL_ERROR;
    }
    /*
     * Try to find a channel with the right name and permissions in
     * the IO channel table of this interpreter.
     */
    
    chan = Tcl_GetChannel(interp, argv[1], NULL);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    sprintf(interp->result, "%d", Tcl_Tell(chan));

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CloseCmd --
 *
 *	This procedure is invoked to process the Tcl "close" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May discard queued input; may flush queued output.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_CloseCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to close. */
    int len;				/* Length of error output. */

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelId\"", (char *) NULL);
	return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], NULL);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    if (Tcl_UnregisterChannel(interp, chan) != TCL_OK) {

        /*
         * If there is an error message and it ends with a newline, remove
         * the newline. This is done for command pipeline channels where the
         * error output from the subprocesses is stored in interp->result.
         *
         * NOTE: This is likely to not have any effect on regular error
         * messages produced by drivers during the closing of a channel,
         * because the Tcl convention is that such error messages do not
         * have a terminating newline.
         */

        len = strlen(interp->result);
        if ((len > 0) && (interp->result[len - 1] == '\n')) {
            interp->result[len - 1] = '\0';
        }
        
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FconfigureCmd --
 *
 *	This procedure is invoked to process the Tcl "fconfigure" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May modify the behavior of an IO channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_FconfigureCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to set a mode on. */
    int result;				/* Of Tcl_Set/GetChannelOption. */
    int i;				/* Iterate over arg-value pairs. */
    Tcl_DString ds;			/* DString to hold result of
                                         * calling Tcl_GetChannelOption. */

    if ((argc < 2) || (((argc % 2) == 1) && (argc != 3))) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " channelId ?optionName? ?value? ?optionName value?...\"",
                (char *) NULL);
        return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], NULL);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if (argc == 2) {
        Tcl_DStringInit(&ds);
        if (Tcl_GetChannelOption(chan, (char *) NULL, &ds) != TCL_OK) {
            Tcl_AppendResult(interp, "option retrieval failed",
                    (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_DStringResult(interp, &ds);
        Tcl_DStringFree(&ds);
        return TCL_OK;
    }
    if (argc == 3) {
        Tcl_DStringInit(&ds);
        if (Tcl_GetChannelOption(chan, argv[2], &ds) != TCL_OK) {
            Tcl_DStringFree(&ds);
            Tcl_AppendResult(interp, "bad option \"", argv[2],
		    "\": must be -blocking, -buffering, -buffersize, ",
                    "-eofchar, -translation, ",
                    "or a channel type specific option", (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_DStringResult(interp, &ds);
        Tcl_DStringFree(&ds);
        return TCL_OK;
    }
    for (i = 3; i < argc; i += 2) {
        result = Tcl_SetChannelOption(interp, chan, argv[i-1], argv[i]);
        if (result != TCL_OK) {
            return result;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EofCmd --
 *
 *	This procedure is invoked to process the Tcl "eof" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets interp->result to "0" or "1" depending on whether the
 *	specified channel has an EOF condition.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_EofCmd(unused, interp, argc, argv)
    ClientData unused;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to query for EOF. */
    int mode;				/* Mode in which channel is opened. */

    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " channelId\"", (char *) NULL);
        return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], &mode);
    if (chan == (Tcl_Channel) NULL) {
	return TCL_ERROR;
    }
    sprintf(interp->result, "%d", Tcl_Eof(chan) ? 1 : 0);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExecCmd --
 *
 *	This procedure is invoked to process the "exec" Tcl command.
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
Tcl_ExecCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#ifdef MAC_TCL
    Tcl_AppendResult(interp, "exec not implemented under Mac OS",
		(char *)NULL);
    return TCL_ERROR;
#else /* !MAC_TCL */
    int keepNewline, firstWord, background, length, result;
    Tcl_Channel chan;
    Tcl_DString ds;
    int readSoFar, readNow, bufSize;

    /*
     * Check for a leading "-keepnewline" argument.
     */

    keepNewline = 0;
    for (firstWord = 1; (firstWord < argc) && (argv[firstWord][0] == '-');
	  firstWord++) {
	if (strcmp(argv[firstWord], "-keepnewline") == 0) {
	    keepNewline = 1;
	} else if (strcmp(argv[firstWord], "--") == 0) {
	    firstWord++;
	    break;
	} else {
	    Tcl_AppendResult(interp, "bad switch \"", argv[firstWord],
		    "\": must be -keepnewline or --", (char *) NULL);
	    return TCL_ERROR;
	}
    }

    if (argc <= firstWord) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?switches? arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * See if the command is to be run in background.
     */

    background = 0;
    if ((argv[argc-1][0] == '&') && (argv[argc-1][1] == 0)) {
	argc--;
	argv[argc] = NULL;
        background = 1;
    }
    
    chan = Tcl_OpenCommandChannel(interp, argc-firstWord,
            argv+firstWord,
	    (background ? 0 : TCL_STDOUT | TCL_STDERR));

    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }

    if (background) {

        /*
         * Get the list of PIDs from the pipeline into interp->result and
         * detach the PIDs (instead of waiting for them).
         */

        TclGetAndDetachPids(interp, chan);
        
        if (Tcl_Close(interp, chan) != TCL_OK) {
            return TCL_ERROR;
        }
        return TCL_OK;
    }

    if (Tcl_GetChannelFile(chan, TCL_READABLE) != NULL) {
#define	EXEC_BUFFER_SIZE 4096

        Tcl_DStringInit(&ds);
        readSoFar = 0; bufSize = 0;
        while (1) {
            bufSize += EXEC_BUFFER_SIZE;
            Tcl_DStringSetLength(&ds, bufSize);
            readNow = Tcl_Read(chan, Tcl_DStringValue(&ds) + readSoFar,
                    EXEC_BUFFER_SIZE);
            if (readNow < 0) {
                Tcl_DStringFree(&ds);
		Tcl_AppendResult(interp,
			"error reading output from command: ",
			Tcl_PosixError(interp), (char *) NULL);
                return TCL_ERROR;
            }
            readSoFar += readNow;
            if (readNow < EXEC_BUFFER_SIZE) {
                break;	/* Out of "while (1)" loop. */
            }
        }
        Tcl_DStringSetLength(&ds, readSoFar);
        Tcl_DStringResult(interp, &ds);
        Tcl_DStringFree(&ds);
    }

    result = Tcl_Close(interp, chan);

    /*
     * If the last character of interp->result is a newline, then remove
     * the newline character (the newline would just confuse things).
     * Special hack: must replace the old terminating null character
     * as a signal to Tcl_AppendResult et al. that we've mucked with
     * the string.
     */
    
    length = strlen(interp->result);
    if (!keepNewline && (length > 0) &&
        (interp->result[length-1] == '\n')) {
        interp->result[length-1] = '\0';
        interp->result[length] = 'x';
    }

    return result;
#endif /* !MAC_TCL */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FblockedCmd --
 *
 *	This procedure is invoked to process the Tcl "fblocked" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets interp->result to "0" or "1" depending on whether the
 *	a preceding input operation on the channel would have blocked.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_FblockedCmd(unused, interp, argc, argv)
    ClientData unused;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to query for blocked. */
    int mode;				/* Mode in which channel was opened. */

    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " channelId\"", (char *) NULL);
        return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], &mode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", argv[1],
                "\" wasn't opened for reading", (char *) NULL);
        return TCL_ERROR;
    }
        
    sprintf(interp->result, "%d", Tcl_InputBlocked(chan) ? 1 : 0);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenCmd --
 *
 *	This procedure is invoked to process the "open" Tcl command.
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
Tcl_OpenCmd(notUsed, interp, argc, argv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int pipeline, prot;
    char *modeString;
    Tcl_Channel chan;

    if ((argc < 2) || (argc > 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" fileName ?access? ?permissions?\"", (char *) NULL);
	return TCL_ERROR;
    }
    prot = 0666;
    if (argc == 2) {
	modeString = "r";
    } else {
	modeString = argv[2];
	if (argc == 4) {
	    if (Tcl_GetInt(interp, argv[3], &prot) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }

    pipeline = 0;
    if (argv[1][0] == '|') {
	pipeline = 1;
    }

    /*
     * Open the file or create a process pipeline.
     */

    if (!pipeline) {
        chan = Tcl_OpenFileChannel(interp, argv[1], modeString, prot);
    } else {
	int mode, seekFlag, cmdArgc;
	char **cmdArgv;

        if (Tcl_SplitList(interp, argv[1]+1, &cmdArgc, &cmdArgv) != TCL_OK) {
            return TCL_ERROR;
        }

        mode = TclGetOpenMode(interp, modeString, &seekFlag);
        if (mode == -1) {
	    chan = NULL;
        } else {
	    int flags = TCL_STDERR | TCL_ENFORCE_MODE;
	    switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
		case O_RDONLY:
		    flags |= TCL_STDOUT;
		    break;
		case O_WRONLY:
		    flags |= TCL_STDIN;
		    break;
		case O_RDWR:
		    flags |= (TCL_STDIN | TCL_STDOUT);
		    break;
		default:
		    panic("Tcl_OpenCmd: invalid mode value");
		    break;
	    }
	    chan = Tcl_OpenCommandChannel(interp, cmdArgc, cmdArgv, flags);
	}
        ckfree((char *) cmdArgv);
    }
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    Tcl_RegisterChannel(interp, chan);
    Tcl_AppendResult(interp, Tcl_GetChannelName(chan), (char *) NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpAcceptCallbacksDeleteProc --
 *
 *	Assocdata cleanup routine called when an interpreter is being
 *	deleted to set the interp field of all the accept callback records
 *	registered with	the interpreter to NULL. This will prevent the
 *	interpreter from being used in the future to eval accept scripts.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates memory and sets the interp field of all the accept
 *	callback records to NULL to prevent this interpreter from being
 *	used subsequently to eval accept scripts.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TcpAcceptCallbacksDeleteProc(clientData, interp)
    ClientData clientData;	/* Data which was passed when the assocdata
                                 * was registered. */
    Tcl_Interp *interp;		/* Interpreter being deleted - not used. */
{
    Tcl_HashTable *hTblPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    AcceptCallback *acceptCallbackPtr;

    hTblPtr = (Tcl_HashTable *) clientData;
    for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
             hPtr != (Tcl_HashEntry *) NULL;
             hPtr = Tcl_NextHashEntry(&hSearch)) {
        acceptCallbackPtr = (AcceptCallback *) Tcl_GetHashValue(hPtr);
        acceptCallbackPtr->interp = (Tcl_Interp *) NULL;
    }
    Tcl_DeleteHashTable(hTblPtr);
    ckfree((char *) hTblPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RegisterTcpServerInterpCleanup --
 *
 *	Registers an accept callback record to have its interp
 *	field set to NULL when the interpreter is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When, in the future, the interpreter is deleted, the interp
 *	field of the accept callback data structure will be set to
 *	NULL. This will prevent attempts to eval the accept script
 *	in a deleted interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
RegisterTcpServerInterpCleanup(interp, acceptCallbackPtr)
    Tcl_Interp *interp;		/* Interpreter for which we want to be
                                 * informed of deletion. */
    AcceptCallback *acceptCallbackPtr;
    				/* The accept callback record whose
                                 * interp field we want set to NULL when
                                 * the interpreter is deleted. */
{
    Tcl_HashTable *hTblPtr;	/* Hash table for accept callback
                                 * records to smash when the interpreter
                                 * will be deleted. */
    Tcl_HashEntry *hPtr;	/* Entry for this record. */
    int new;			/* Is the entry new? */

    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp,
            "tclTCPAcceptCallbacks",
            NULL);
    if (hTblPtr == (Tcl_HashTable *) NULL) {
        hTblPtr = (Tcl_HashTable *) ckalloc((unsigned) sizeof(Tcl_HashTable));
        Tcl_InitHashTable(hTblPtr, TCL_ONE_WORD_KEYS);
        (void) Tcl_SetAssocData(interp, "tclTCPAcceptCallbacks",
                TcpAcceptCallbacksDeleteProc, (ClientData) hTblPtr);
    }
    hPtr = Tcl_CreateHashEntry(hTblPtr, (char *) acceptCallbackPtr, &new);
    if (!new) {
        panic("RegisterTcpServerCleanup: damaged accept record table");
    }
    Tcl_SetHashValue(hPtr, (ClientData) acceptCallbackPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * UnregisterTcpServerInterpCleanupProc --
 *
 *	Unregister a previously registered accept callback record. The
 *	interp field of this record will no longer be set to NULL in
 *	the future when the interpreter is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Prevents the interp field of the accept callback record from
 *	being set to NULL in the future when the interpreter is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
UnregisterTcpServerInterpCleanupProc(interp, acceptCallbackPtr)
    Tcl_Interp *interp;		/* Interpreter in which the accept callback
                                 * record was registered. */
    AcceptCallback *acceptCallbackPtr;
    				/* The record for which to delete the
                                 * registration. */
{
    Tcl_HashTable *hTblPtr;
    Tcl_HashEntry *hPtr;

    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp,
            "tclTCPAcceptCallbacks", NULL);
    if (hTblPtr == (Tcl_HashTable *) NULL) {
        return;
    }
    hPtr = Tcl_FindHashEntry(hTblPtr, (char *) acceptCallbackPtr);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        return;
    }
    Tcl_DeleteHashEntry(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AcceptCallbackProc --
 *
 *	This callback is invoked by the TCP channel driver when it
 *	accepts a new connection from a client on a server socket.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Whatever the script does.
 *
 *----------------------------------------------------------------------
 */

static void
AcceptCallbackProc(callbackData, chan, address, port)
    ClientData callbackData;		/* The data stored when the callback
                                         * was created in the call to
                                         * Tcl_OpenTcpServer. */
    Tcl_Channel chan;			/* Channel for the newly accepted
                                         * connection. */
    char *address;			/* Address of client that was
                                         * accepted. */
    int port;				/* Port of client that was accepted. */
{
    AcceptCallback *acceptCallbackPtr;
    Tcl_Interp *interp;
    char *script;
    char portBuf[10];
    int result;

    acceptCallbackPtr = (AcceptCallback *) callbackData;

    /*
     * Check if the callback is still valid; the interpreter may have gone
     * away, this is signalled by setting the interp field of the callback
     * data to NULL.
     */
    
    if (acceptCallbackPtr->interp != (Tcl_Interp *) NULL) {

        script = acceptCallbackPtr->script;
        interp = acceptCallbackPtr->interp;
        
        Tcl_Preserve((ClientData) script);
        Tcl_Preserve((ClientData) interp);

	sprintf(portBuf, "%d", port);
        Tcl_RegisterChannel(interp, chan);
        result = Tcl_VarEval(interp, script, " ", Tcl_GetChannelName(chan),
                " ", address, " ", portBuf, (char *) NULL);
        if (result != TCL_OK) {
            Tcl_BackgroundError(interp);
	    Tcl_UnregisterChannel(interp, chan);
        }
        Tcl_Release((ClientData) interp);
        Tcl_Release((ClientData) script);
    } else {

        /*
         * The interpreter has been deleted, so there is no useful
         * way to utilize the client socket - just close it.
         */

        Tcl_Close((Tcl_Interp *) NULL, chan);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TcpServerCloseProc --
 *
 *	This callback is called when the TCP server channel for which it
 *	was registered is being closed. It informs the interpreter in
 *	which the accept script is evaluated (if that interpreter still
 *	exists) that this channel no longer needs to be informed if the
 *	interpreter is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	In the future, if the interpreter is deleted this channel will
 *	no longer be informed.
 *
 *----------------------------------------------------------------------
 */

static void
TcpServerCloseProc(callbackData)
    ClientData callbackData;	/* The data passed in the call to
                                 * Tcl_CreateCloseHandler. */
{
    AcceptCallback *acceptCallbackPtr;
    				/* The actual data. */

    acceptCallbackPtr = (AcceptCallback *) callbackData;
    if (acceptCallbackPtr->interp != (Tcl_Interp *) NULL) {
        UnregisterTcpServerInterpCleanupProc(acceptCallbackPtr->interp,
                acceptCallbackPtr);
    }
    Tcl_EventuallyFree((ClientData) acceptCallbackPtr->script, TCL_DYNAMIC);
    ckfree((char *) acceptCallbackPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SocketCmd --
 *
 *	This procedure is invoked to process the "socket" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates a socket based channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SocketCmd(notUsed, interp, argc, argv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int a, server, port;
    char *arg, *copyScript, *host, *script;
    char *myaddr = NULL;
    int myport = 0;
    int async = 0;
    Tcl_Channel chan;
    AcceptCallback *acceptCallbackPtr;
    
    server = 0;
    script = NULL;

    if (TclHasSockets(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    for (a = 1; a < argc; a++) {
        arg = argv[a];
	if (arg[0] == '-') {
	    if (strcmp(arg, "-server") == 0) {
                if (async == 1) {
                    Tcl_AppendResult(interp,
                            "cannot set -async option for server sockets",
                            (char *) NULL);
                    return TCL_ERROR;
                }
		server = 1;
		a++;
		if (a >= argc) {
		    Tcl_AppendResult(interp,
			    "no argument given for -server option",
                            (char *) NULL);
		    return TCL_ERROR;
		}
                script = argv[a];
            } else if (strcmp(arg, "-myaddr") == 0) {
		a++;
                if (a >= argc) {
		    Tcl_AppendResult(interp,
			    "no argument given for -myaddr option",
                            (char *) NULL);
		    return TCL_ERROR;
		}
                myaddr = argv[a];
            } else if (strcmp(arg, "-myport") == 0) {
		a++;
                if (a >= argc) {
		    Tcl_AppendResult(interp,
			    "no argument given for -myport option",
                            (char *) NULL);
		    return TCL_ERROR;
		}
		if (TclSockGetPort(interp, argv[a], "tcp", &myport)
                    != TCL_OK) {
		    return TCL_ERROR;
		}
            } else if (strcmp(arg, "-async") == 0) {
                if (server == 1) {
                    Tcl_AppendResult(interp,
                            "cannot set -async option for server sockets",
                            (char *) NULL);
                    return TCL_ERROR;
                }
                async = 1;
	    } else {
		Tcl_AppendResult(interp, "bad option \"", arg,
                        "\", must be -async, -myaddr, -myport, or -server",
                        (char *) NULL);
		return TCL_ERROR;
	    }
	} else {
	    break;
	}
    }
    if (server) {
        host = myaddr;		/* NULL implies INADDR_ANY */
	if (myport != 0) {
	    Tcl_AppendResult(interp, "Option -myport is not valid for servers",
		    NULL);
	    return TCL_ERROR;
	}
    } else if (a < argc) {
	host = argv[a];
	a++;
    } else {
wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args: should be either:\n",
		argv[0],
                " ?-myaddr addr? ?-myport myport? ?-async? host port\n",
		argv[0],
                " -server command ?-myaddr addr? port",
                (char *) NULL);
        return TCL_ERROR;
    }

    if (a == argc-1) {
	if (TclSockGetPort(interp, argv[a], "tcp", &port) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	goto wrongNumArgs;
    }

    if (server) {
        acceptCallbackPtr = (AcceptCallback *) ckalloc((unsigned)
                sizeof(AcceptCallback));
        copyScript = ckalloc((unsigned) strlen(script) + 1);
        strcpy(copyScript, script);
        acceptCallbackPtr->script = copyScript;
        acceptCallbackPtr->interp = interp;
        chan = Tcl_OpenTcpServer(interp, port, host, AcceptCallbackProc,
                (ClientData) acceptCallbackPtr);
        if (chan == (Tcl_Channel) NULL) {
            ckfree(copyScript);
            ckfree((char *) acceptCallbackPtr);
            return TCL_ERROR;
        }

        /*
         * Register with the interpreter to let us know when the
         * interpreter is deleted (by having the callback set the
         * acceptCallbackPtr->interp field to NULL). This is to
         * avoid trying to eval the script in a deleted interpreter.
         */

        RegisterTcpServerInterpCleanup(interp, acceptCallbackPtr);
        
        /*
         * Register a close callback. This callback will inform the
         * interpreter (if it still exists) that this channel does not
         * need to be informed when the interpreter is deleted.
         */
        
        Tcl_CreateCloseHandler(chan, TcpServerCloseProc,
                (ClientData) acceptCallbackPtr);
    } else {
        chan = Tcl_OpenTcpClient(interp, port, host, myaddr, myport, async);
        if (chan == (Tcl_Channel) NULL) {
            return TCL_ERROR;
        }
    }
    Tcl_RegisterChannel(interp, chan);            
    Tcl_AppendResult(interp, Tcl_GetChannelName(chan), (char *) NULL);
    
    return TCL_OK;
}
