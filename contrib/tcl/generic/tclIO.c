/* 
 * tclIO.c --
 *
 *	This file provides the generic portions (those that are the same on
 *	all platforms and for all channel types) of Tcl's IO facilities.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclIO.c 1.227 96/07/30 09:26:30
 */

#include	"tclInt.h"
#include	"tclPort.h"

/*
 * Make sure that both EAGAIN and EWOULDBLOCK are defined. This does not
 * compile on systems where neither is defined. We want both defined so
 * that we can test safely for both. In the code we still have to test for
 * both because there may be systems on which both are defined and have
 * different values.
 */

#if ((!defined(EWOULDBLOCK)) && (defined(EAGAIN)))
#   define EWOULDBLOCK EAGAIN
#endif
#if ((!defined(EAGAIN)) && (defined(EWOULDBLOCK)))
#   define EAGAIN EWOULDBLOCK
#endif
#if ((!defined(EAGAIN)) && (!defined(EWOULDBLOCK)))
    error one of EWOULDBLOCK or EAGAIN must be defined
#endif

/*
 * struct ChannelBuffer:
 *
 * Buffers data being sent to or from a channel.
 */

typedef struct ChannelBuffer {
    int nextAdded;		/* The next position into which a character
                                 * will be put in the buffer. */
    int nextRemoved;		/* Position of next byte to be removed
                                 * from the buffer. */
    int bufSize;		/* How big is the buffer? */
    struct ChannelBuffer *nextPtr;
    				/* Next buffer in chain. */
    char buf[4];		/* Placeholder for real buffer. The real
                                 * buffer occuppies this space + bufSize-4
                                 * bytes. This must be the last field in
                                 * the structure. */
} ChannelBuffer;

#define CHANNELBUFFER_HEADER_SIZE	(sizeof(ChannelBuffer) - 4)

/*
 * The following defines the *default* buffer size for channels.
 */

#define CHANNELBUFFER_DEFAULT_SIZE	(1024 * 4)

/*
 * Structure to record a close callback. One such record exists for
 * each close callback registered for a channel.
 */

typedef struct CloseCallback {
    Tcl_CloseProc *proc;		/* The procedure to call. */
    ClientData clientData;		/* Arbitrary one-word data to pass
                                         * to the callback. */
    struct CloseCallback *nextPtr;	/* For chaining close callbacks. */
} CloseCallback;

/*
 * Forward declaration of Channel; being used in struct EventScriptRecord,
 * below.
 */

typedef struct Channel *ChanPtr;

/*
 * The following structure describes the information saved from a call to
 * "fileevent". This is used later when the event being waited for to
 * invoke the saved script in the interpreter designed in this record.
 */

typedef struct EventScriptRecord {
    struct Channel *chanPtr;	/* The channel for which this script is
                                 * registered. This is used only when an
                                 * error occurs during evaluation of the
                                 * script, to delete the handler. */
    char *script;		/* Script to invoke. */
    Tcl_Interp *interp;		/* In what interpreter to invoke script? */
    int mask;			/* Events must overlap current mask for the
                                 * stored script to be invoked. */
    struct EventScriptRecord *nextPtr;
    				/* Next in chain of records. */
} EventScriptRecord;

/*
 * Forward declaration of ChannelHandler; being used in struct Channel,
 * below.
 */

typedef struct ChannelHandler *ChannelHandlerPtr;

/*
 * struct Channel:
 *
 * One of these structures is allocated for each open channel. It contains data
 * specific to the channel but which belongs to the generic part of the Tcl
 * channel mechanism, and it points at an instance specific (and type
 * specific) * instance data, and at a channel type structure.
 */

typedef struct Channel {
    char *channelName;		/* The name of the channel instance in Tcl
                                 * commands. Storage is owned by the generic IO
                                 * code,  is dynamically allocated. */
    int	flags;			/* ORed combination of the flags defined
                                 * below. */
    Tcl_EolTranslation inputTranslation;
				/* What translation to apply for end of line
                                 * sequences on input? */    
    Tcl_EolTranslation outputTranslation;
    				/* What translation to use for generating
                                 * end of line sequences in output? */
    int inEofChar;		/* If nonzero, use this as a signal of EOF
                                 * on input. */
    int outEofChar;             /* If nonzero, append this to the channel
                                 * when it is closed if it is open for
                                 * writing. */
    int unreportedError;	/* Non-zero if an error report was deferred
                                 * because it happened in the background. The
                                 * value is the POSIX error code. */
    ClientData instanceData;	/* Instance specific data. */
    Tcl_File inFile;		/* File to use for input, or NULL. */
    Tcl_File outFile;		/* File to use for output, or NULL. */
    Tcl_ChannelType *typePtr;	/* Pointer to channel type structure. */
    int refCount;		/* How many interpreters hold references to
                                 * this IO channel? */
    CloseCallback *closeCbPtr;	/* Callbacks registered to be called when the
                                 * channel is closed. */
    ChannelBuffer *curOutPtr;	/* Current output buffer being filled. */
    ChannelBuffer *outQueueHead;/* Points at first buffer in output queue. */
    ChannelBuffer *outQueueTail;/* Points at last buffer in output queue. */

    ChannelBuffer *saveInBufPtr;/* Buffer saved for input queue - eliminates
                                 * need to allocate a new buffer for "gets"
                                 * that crosses buffer boundaries. */
    ChannelBuffer *inQueueHead;	/* Points at first buffer in input queue. */
    ChannelBuffer *inQueueTail;	/* Points at last buffer in input queue. */

    struct ChannelHandler *chPtr;/* List of channel handlers registered
                                  * for this channel. */
    int interestMask;		/* Mask of all events this channel has
                                 * handlers for. */
    struct Channel *nextChanPtr;/* Next in list of channels currently open. */
    EventScriptRecord *scriptRecordPtr;
    				/* Chain of all scripts registered for
                                 * event handlers ("fileevent") on this
                                 * channel. */
    int bufSize;		/* What size buffers to allocate? */
} Channel;
    
/*
 * Values for the flags field in Channel. Any ORed combination of the
 * following flags can be stored in the field. These flags record various
 * options and state bits about the channel. In addition to the flags below,
 * the channel can also have TCL_READABLE (1<<1) and TCL_WRITABLE (1<<2) set.
 */

#define CHANNEL_NONBLOCKING	(1<<3)	/* Channel is currently in
					 * nonblocking mode. */
#define CHANNEL_LINEBUFFERED	(1<<4)	/* Output to the channel must be
					 * flushed after every newline. */
#define CHANNEL_UNBUFFERED	(1<<5)	/* Output to the channel must always
					 * be flushed immediately. */
#define BUFFER_READY		(1<<6)	/* Current output buffer (the
					 * curOutPtr field in the
                                         * channel structure) should be
                                         * output as soon as possible event
                                         * though it may not be full. */
#define BG_FLUSH_SCHEDULED	(1<<7)	/* A background flush of the
					 * queued output buffers has been
                                         * scheduled. */
#define CHANNEL_CLOSED		(1<<8)	/* Channel has been closed. No
					 * further Tcl-level IO on the
                                         * channel is allowed. */
#define	CHANNEL_EOF		(1<<9)	/* EOF occurred on this channel.
					 * This bit is cleared before every
                                         * input operation. */
#define CHANNEL_STICKY_EOF	(1<<10)	/* EOF occurred on this channel because
					 * we saw the input eofChar. This bit
                                         * prevents clearing of the EOF bit
                                         * before every input operation. */
#define CHANNEL_BLOCKED		(1<<11)	/* EWOULDBLOCK or EAGAIN occurred
					 * on this channel. This bit is
                                         * cleared before every input or
                                         * output operation. */
#define INPUT_SAW_CR		(1<<12)	/* Channel is in CRLF eol input
					 * translation mode and the last
                                         * byte seen was a "\r". */
#define CHANNEL_DEAD		(1<<13)	/* The channel has been closed by
					 * the exit handler (on exit) but
                                         * not deallocated. When any IO
                                         * operation sees this flag on a
                                         * channel, it does not call driver
                                         * level functions to avoid referring
                                         * to deallocated data. */

/*
 * For each channel handler registered in a call to Tcl_CreateChannelHandler,
 * there is one record of the following type. All of records for a specific
 * channel are chained together in a singly linked list which is stored in
 * the channel structure.
 */

typedef struct ChannelHandler {
    Channel *chanPtr;		/* The channel structure for this channel. */
    int mask;			/* Mask of desired events. */
    Tcl_ChannelProc *proc;	/* Procedure to call in the type of
                                 * Tcl_CreateChannelHandler. */
    ClientData clientData;	/* Argument to pass to procedure. */
    struct ChannelHandler *nextPtr;
    				/* Next one in list of registered handlers. */
} ChannelHandler;

/*
 * This structure keeps track of the current ChannelHandler being invoked in
 * the current invocation of ChannelHandlerEventProc. There is a potential
 * problem if a ChannelHandler is deleted while it is the current one, since
 * ChannelHandlerEventProc needs to look at the nextPtr field. To handle this
 * problem, structures of the type below indicate the next handler to be
 * processed for any (recursively nested) dispatches in progress. The
 * nextHandlerPtr field is updated if the handler being pointed to is deleted.
 * The nextPtr field is used to chain together all recursive invocations, so
 * that Tcl_DeleteChannelHandler can find all the recursively nested
 * invocations of ChannelHandlerEventProc and compare the handler being
 * deleted against the NEXT handler to be invoked in that invocation; when it
 * finds such a situation, Tcl_DeleteChannelHandler updates the nextHandlerPtr
 * field of the structure to the next handler.
 */

typedef struct NextChannelHandler {
    ChannelHandler *nextHandlerPtr;	/* The next handler to be invoked in
                                         * this invocation. */
    struct NextChannelHandler *nestedHandlerPtr;
					/* Next nested invocation of
                                         * ChannelHandlerEventProc. */
} NextChannelHandler;

/*
 * This variable holds the list of nested ChannelHandlerEventProc invocations.
 */

static NextChannelHandler *nestedHandlerPtr = (NextChannelHandler *) NULL;

/*
 * List of all channels currently open.
 */

static Channel *firstChanPtr = (Channel *) NULL;

/*
 * Has a channel exit handler been created yet?
 */

static int channelExitHandlerCreated = 0;

/*
 * Has the channel event source been created and registered with the
 * notifier?
 */

static int channelEventSourceCreated = 0;

/*
 * The following structure describes the event that is added to the Tcl
 * event queue by the channel handler check procedure.
 */

typedef struct ChannelHandlerEvent {
    Tcl_Event header;		/* Standard header for all events. */
    Channel *chanPtr;		/* The channel that is ready. */
    int readyMask;		/* Events that have occurred. */
} ChannelHandlerEvent;

/*
 * Static variables to hold channels for stdin, stdout and stderr.
 */

static Tcl_Channel stdinChannel = NULL;
static int stdinInitialized = 0;
static Tcl_Channel stdoutChannel = NULL;
static int stdoutInitialized = 0;
static Tcl_Channel stderrChannel = NULL;
static int stderrInitialized = 0;

/*
 * Static functions in this file:
 */

static int		ChannelEventDeleteProc _ANSI_ARGS_((
			    Tcl_Event *evPtr, ClientData clientData));
static void		ChannelEventSourceExitProc _ANSI_ARGS_((
    			    ClientData data));
static int		ChannelHandlerEventProc _ANSI_ARGS_((
			    Tcl_Event *evPtr, int flags));
static void		ChannelHandlerCheckProc _ANSI_ARGS_((
			    ClientData clientData, int flags));
static void		ChannelHandlerSetupProc _ANSI_ARGS_((
			    ClientData clientData, int flags));
static void		ChannelEventScriptInvoker _ANSI_ARGS_((
			    ClientData clientData, int flags));
static void		CleanupChannelHandlers _ANSI_ARGS_((
			    Tcl_Interp *interp, Channel *chanPtr));
static int		CloseChannel _ANSI_ARGS_((Tcl_Interp *interp,
                            Channel *chanPtr, int errorCode));
static void		CloseChannelsOnExit _ANSI_ARGS_((ClientData data));
static int		CopyAndTranslateBuffer _ANSI_ARGS_((
			    Channel *chanPtr, char *result, int space));
static void		CreateScriptRecord _ANSI_ARGS_((
			    Tcl_Interp *interp, Channel *chanPtr,
                            int mask, char *script));
static void		DeleteChannelTable _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));
static void		DeleteScriptRecord _ANSI_ARGS_((Tcl_Interp *interp,
        		    Channel *chanPtr, int mask));
static void		DiscardInputQueued _ANSI_ARGS_((
			    Channel *chanPtr, int discardSavedBuffers));
static void		DiscardOutputQueued _ANSI_ARGS_((
    			    Channel *chanPtr));
static int		FlushChannel _ANSI_ARGS_((Tcl_Interp *interp,
                            Channel *chanPtr, int calledFromAsyncFlush));
static void		FlushEventProc _ANSI_ARGS_((ClientData clientData,
                            int mask));
static Tcl_HashTable	*GetChannelTable _ANSI_ARGS_((Tcl_Interp *interp));
static int		GetEOL _ANSI_ARGS_((Channel *chanPtr));
static int		GetInput _ANSI_ARGS_((Channel *chanPtr));
static void		RecycleBuffer _ANSI_ARGS_((Channel *chanPtr,
		            ChannelBuffer *bufPtr, int mustDiscard));
static void		ReturnScriptRecord _ANSI_ARGS_((Tcl_Interp *interp,
		            Channel *chanPtr, int mask));
static int		ScanBufferForEOL _ANSI_ARGS_((Channel *chanPtr,
                            ChannelBuffer *bufPtr,
                            Tcl_EolTranslation translation, int eofChar,
		            int *bytesToEOLPtr, int *crSeenPtr));
static int		ScanInputForEOL _ANSI_ARGS_((Channel *chanPtr,
		            int *bytesQueuedPtr));

/*
 *----------------------------------------------------------------------
 *
 * TclFindChannel --
 *
 *	Finds a channel given two Tcl_Files.
 *
 * Results:
 *	The Tcl_Channel found. Also returns nonzero in fileUsedPtr output
 *	parameter if it finds that the Tcl_File is already used in another
 *	channel.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclFindFileChannel(inFile, outFile, fileUsedPtr)
    Tcl_File inFile, outFile;		/* Channel has these Tcl_Files. */
    int *fileUsedPtr;
{
    Channel *chanPtr;
    
    *fileUsedPtr = 0;
    for (chanPtr = firstChanPtr;
             chanPtr != (Channel *) NULL;
             chanPtr = chanPtr->nextChanPtr) {
        if ((chanPtr->inFile == inFile) && (chanPtr->outFile == outFile)) {
            return (Tcl_Channel) chanPtr;
        }
        if ((inFile != (Tcl_File) NULL) && (chanPtr->inFile == inFile)) {
            *fileUsedPtr = 1;
            return (Tcl_Channel) NULL;
        }
        if ((outFile != (Tcl_File) NULL) && (chanPtr->outFile == outFile)) {
            *fileUsedPtr = 1;
            return (Tcl_Channel) NULL;
        }
    }
    return (Tcl_Channel) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetStdChannel --
 *
 *	This function is used to change the channels that are used
 *	for stdin/stdout/stderr in new interpreters.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetStdChannel(channel, type)
    Tcl_Channel channel;
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR. */
{
    switch (type) {
	case TCL_STDIN:
            stdinInitialized = 1;
	    stdinChannel = channel;
	    break;
	case TCL_STDOUT:
	    stdoutInitialized = 1;
	    stdoutChannel = channel;
	    break;
	case TCL_STDERR:
	    stderrInitialized = 1;
	    stderrChannel = channel;
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetStdChannel --
 *
 *	Returns the specified standard channel.
 *
 * Results:
 *	Returns the specified standard channel, or NULL.
 *
 * Side effects:
 *	May cause the creation of a standard channel and the underlying
 *	file.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_GetStdChannel(type)
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR. */
{
    Tcl_Channel channel = NULL;

    /*
     * If the channels were not created yet, create them now and
     * store them in the static variables.  Note that we need to set
     * stdinInitialized before calling TclGetDefaultStdChannel in order
     * to avoid recursive loops when TclGetDefaultStdChannel calls
     * Tcl_CreateChannel.
     */

    switch (type) {
	case TCL_STDIN:
	    if (!stdinInitialized) {
		stdinInitialized = 1;
		stdinChannel = TclGetDefaultStdChannel(TCL_STDIN);
	    }
	    channel = stdinChannel;
	    break;
	case TCL_STDOUT:
	    if (!stdoutInitialized) {
		stdoutInitialized = 1;
		stdoutChannel = TclGetDefaultStdChannel(TCL_STDOUT);
	    }
	    channel = stdoutChannel;
	    break;
	case TCL_STDERR:
	    if (!stderrInitialized) {
		stderrInitialized = 1;
		stderrChannel = TclGetDefaultStdChannel(TCL_STDERR);
	    }
	    channel = stderrChannel;
	    break;
    }
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateCloseHandler
 *
 *	Creates a close callback which will be called when the channel is
 *	closed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Causes the callback to be called in the future when the channel
 *	will be closed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateCloseHandler(chan, proc, clientData)
    Tcl_Channel chan;		/* The channel for which to create the
                                 * close callback. */
    Tcl_CloseProc *proc;	/* The callback routine to call when the
                                 * channel will be closed. */
    ClientData clientData;	/* Arbitrary data to pass to the
                                 * close callback. */
{
    Channel *chanPtr;
    CloseCallback *cbPtr;

    chanPtr = (Channel *) chan;

    cbPtr = (CloseCallback *) ckalloc((unsigned) sizeof(CloseCallback));
    cbPtr->proc = proc;
    cbPtr->clientData = clientData;

    cbPtr->nextPtr = chanPtr->closeCbPtr;
    chanPtr->closeCbPtr = cbPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteCloseHandler --
 *
 *	Removes a callback that would have been called on closing
 *	the channel. If there is no matching callback then this
 *	function has no effect.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The callback will not be called in the future when the channel
 *	is eventually closed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteCloseHandler(chan, proc, clientData)
    Tcl_Channel chan;		/* The channel for which to cancel the
                                 * close callback. */
    Tcl_CloseProc *proc;	/* The procedure for the callback to
                                 * remove. */
    ClientData clientData;	/* The callback data for the callback
                                 * to remove. */
{
    Channel *chanPtr;
    CloseCallback *cbPtr, *cbPrevPtr;

    chanPtr = (Channel *) chan;
    for (cbPtr = chanPtr->closeCbPtr, cbPrevPtr = (CloseCallback *) NULL;
             cbPtr != (CloseCallback *) NULL;
             cbPtr = cbPtr->nextPtr) {
        if ((cbPtr->proc == proc) && (cbPtr->clientData == clientData)) {
            if (cbPrevPtr == (CloseCallback *) NULL) {
                chanPtr->closeCbPtr = cbPtr->nextPtr;
            } else {
                cbPrevPtr = cbPtr->nextPtr;
            }
            ckfree((char *) cbPtr);
            break;
        } else {
            cbPrevPtr = cbPtr;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CloseChannelsOnExit --
 *
 *	Closes all the existing channels, on exit. This	routine is called
 *	during exit processing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Closes all channels.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
CloseChannelsOnExit(clientData)
    ClientData clientData;		/* NULL - unused. */
{
    Channel *chanPtr;			/* Iterates over open channels. */
    Channel *nextChanPtr;		/* Iterates over open channels. */


    for (chanPtr = firstChanPtr; chanPtr != (Channel *) NULL;
             chanPtr = nextChanPtr) {
        nextChanPtr = chanPtr->nextChanPtr;

        /*
         * Set the channel back into blocking mode to ensure that we wait
         * for all data to flush out.
         */
        
        (void) Tcl_SetChannelOption(NULL, (Tcl_Channel) chanPtr,
                "-blocking", "on");
    
        if (chanPtr->refCount <= 0) {

	    /*
             * Close it only if the refcount indicates that the channel is not
             * referenced from any interpreter. If it is, that interpreter will
             * close the channel when it gets destroyed.
             */

            Tcl_Close((Tcl_Interp *) NULL, (Tcl_Channel) chanPtr);
        } else {

            /*
             * The refcount is greater than zero, so flush the channel.
             */

            Tcl_Flush((Tcl_Channel) chanPtr);

            /*
             * And close the OS level handles using the driver function:
             */

            (chanPtr->typePtr->closeProc) (chanPtr->instanceData,
                    (Tcl_Interp *) NULL, chanPtr->inFile, chanPtr->outFile);

            /*
             * Finally, we clean up the fields in the channel data structure
             * since all of them have been deleted already. We mark the
             * channel with CHANNEL_DEAD to prevent any further IO operations
             * on it.
             */

            chanPtr->inFile = (Tcl_File) NULL;
            chanPtr->outFile = (Tcl_File) NULL;
            chanPtr->instanceData = (ClientData) NULL;
            chanPtr->flags |= CHANNEL_DEAD;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetChannelTable --
 *
 *	Gets and potentially initializes the channel table for an
 *	interpreter. If it is initializing the table it also inserts
 *	channels for stdin, stdout and stderr if the interpreter is
 *	trusted.
 *
 * Results:
 *	A pointer to the hash table created, for use by the caller.
 *
 * Side effects:
 *	Initializes the channel table for an interpreter. May create
 *	channels for stdin, stdout and stderr.
 *
 *----------------------------------------------------------------------
 */

static Tcl_HashTable *
GetChannelTable(interp)
    Tcl_Interp *interp;
{
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_Channel stdinChan, stdoutChan, stderrChan;

    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
    if (hTblPtr == (Tcl_HashTable *) NULL) {
        hTblPtr = (Tcl_HashTable *) ckalloc((unsigned) sizeof(Tcl_HashTable));
        Tcl_InitHashTable(hTblPtr, TCL_STRING_KEYS);

        (void) Tcl_SetAssocData(interp, "tclIO",
                (Tcl_InterpDeleteProc *) DeleteChannelTable,
                (ClientData) hTblPtr);

        /*
         * If the interpreter is trusted (not "safe"), insert channels
         * for stdin, stdout and stderr (possibly creating them in the
         * process).
         */

        if (Tcl_IsSafe(interp) == 0) {
            stdinChan = Tcl_GetStdChannel(TCL_STDIN);
            if (stdinChan != NULL) {
                Tcl_RegisterChannel(interp, stdinChan);
            }
            stdoutChan = Tcl_GetStdChannel(TCL_STDOUT);
            if (stdoutChan != NULL) {
                Tcl_RegisterChannel(interp, stdoutChan);
            }
            stderrChan = Tcl_GetStdChannel(TCL_STDERR);
            if (stderrChan != NULL) {
                Tcl_RegisterChannel(interp, stderrChan);
            }
        }

    }
    return hTblPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteChannelTable --
 *
 *	Deletes the channel table for an interpreter, closing any open
 *	channels whose refcount reaches zero. This procedure is invoked
 *	when an interpreter is deleted, via the AssocData cleanup
 *	mechanism.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the hash table of channels. May close channels. May flush
 *	output on closed channels. Removes any channeEvent handlers that were
 *	registered in this interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteChannelTable(clientData, interp)
    ClientData clientData;	/* The per-interpreter data structure. */
    Tcl_Interp *interp;		/* The interpreter being deleted. */
{
    Tcl_HashTable *hTblPtr;	/* The hash table. */
    Tcl_HashSearch hSearch;	/* Search variable. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;	/* Channel being deleted. */
    EventScriptRecord *sPtr, *prevPtr, *nextPtr;
    				/* Variables to loop over all channel events
                                 * registered, to delete the ones that refer
                                 * to the interpreter being deleted. */

    /*
     * Delete all the registered channels - this will close channels whose
     * refcount reaches zero.
     */
    
    hTblPtr = (Tcl_HashTable *) clientData;
    for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
             hPtr != (Tcl_HashEntry *) NULL;
             hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch)) {

        chanPtr = (Channel *) Tcl_GetHashValue(hPtr);

        /*
         * Remove any fileevents registered in this interpreter.
         */
        
        for (sPtr = chanPtr->scriptRecordPtr,
                 prevPtr = (EventScriptRecord *) NULL;
                 sPtr != (EventScriptRecord *) NULL;
                 sPtr = nextPtr) {
            nextPtr = sPtr->nextPtr;
            if (sPtr->interp == interp) {
                if (prevPtr == (EventScriptRecord *) NULL) {
                    chanPtr->scriptRecordPtr = nextPtr;
                } else {
                    prevPtr->nextPtr = nextPtr;
                }

                Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                        ChannelEventScriptInvoker, (ClientData) sPtr);

                Tcl_EventuallyFree((ClientData) sPtr->script, TCL_DYNAMIC);
                ckfree((char *) sPtr);
            } else {
                prevPtr = sPtr;
            }
        }

        /*
         * Cannot call Tcl_UnregisterChannel because that procedure calls
         * Tcl_GetAssocData to get the channel table, which might already
         * be inaccessible from the interpreter structure. Instead, we
         * emulate the behavior of Tcl_UnregisterChannel directly here.
         */

        Tcl_DeleteHashEntry(hPtr);
        chanPtr->refCount--;
        if (chanPtr->refCount <= 0) {
            chanPtr->flags |= CHANNEL_CLOSED;
            if (!(chanPtr->flags & BG_FLUSH_SCHEDULED)) {
                Tcl_Close(interp, (Tcl_Channel) chanPtr);
            }
        }
    }
    Tcl_DeleteHashTable(hTblPtr);
    ckfree((char *) hTblPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnregisterChannel --
 *
 *	Deletes the hash entry for a channel associated with an interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes the hash entry for a channel associated with an interpreter.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnregisterChannel(interp, chan)
    Tcl_Interp *interp;		/* Interpreter in which channel is defined. */
    Tcl_Channel chan;		/* Channel to delete. */
{
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;		/* The real IO channel. */

    chanPtr = (Channel *) chan;
    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
    if (hTblPtr == (Tcl_HashTable *) NULL) {
        return TCL_OK;
    }
    hPtr = Tcl_FindHashEntry(hTblPtr, chanPtr->channelName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        return TCL_OK;
    }
    if ((Channel *) Tcl_GetHashValue(hPtr) != chanPtr) {
        return TCL_OK;
    }
    Tcl_DeleteHashEntry(hPtr);

    /*
     * Remove channel handlers that refer to this interpreter, so that they
     * will not be present if the actual close is delayed and more events
     * happen on the channel. This may occur if the channel is shared between
     * several interpreters, or if the channel has async flushing active.
     */
    
    CleanupChannelHandlers(interp, chanPtr);

    chanPtr->refCount--;
    if (chanPtr->refCount <= 0) {

        /*
         * Ensure that if there is another buffer, it gets flushed
         * whether or not we are doing a background flush.
         */

        if ((chanPtr->curOutPtr != NULL) &&
                (chanPtr->curOutPtr->nextAdded >
                        chanPtr->curOutPtr->nextRemoved)) {
            chanPtr->flags |= BUFFER_READY;
        }
        chanPtr->flags |= CHANNEL_CLOSED;
        if (!(chanPtr->flags & BG_FLUSH_SCHEDULED)) {
            if (Tcl_Close(interp, chan) != TCL_OK) {
                return TCL_ERROR;
            }
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegisterChannel --
 *
 *	Adds an already-open channel to the channel table of an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May increment the reference count of a channel.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_RegisterChannel(interp, chan)
    Tcl_Interp *interp;		/* Interpreter in which to add the channel. */
    Tcl_Channel chan;		/* The channel to add to this interpreter
                                 * channel table. */
{
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    int new;			/* Is the hash entry new or does it exist? */
    Channel *chanPtr;		/* The actual channel. */

    chanPtr = (Channel *) chan;

    if (chanPtr->channelName == (char *) NULL) {
        panic("Tcl_RegisterChannel: channel without name");
    }
    hTblPtr = GetChannelTable(interp);
    hPtr = Tcl_CreateHashEntry(hTblPtr, chanPtr->channelName, &new);
    if (new == 0) {
        if (chan == (Tcl_Channel) Tcl_GetHashValue(hPtr)) {
            return;
        }
        panic("Tcl_RegisterChannel: duplicate channel names");
    }
    Tcl_SetHashValue(hPtr, (ClientData) chanPtr);
    chanPtr->refCount++;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannel --
 *
 *	Finds an existing Tcl_Channel structure by name in a given
 *	interpreter. This function is public because it is used by
 *	channel-type-specific functions.
 *
 * Results:
 *	A Tcl_Channel or NULL on failure. If failed, interp->result
 *	contains an error message. It also returns, in modePtr, the
 *	modes in which the channel is opened.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_GetChannel(interp, chanName, modePtr)
    Tcl_Interp *interp;		/* Interpreter in which to find or create
                                 * the channel. */
    char *chanName;		/* The name of the channel. */
    int *modePtr;		/* Where to store the mode in which the
                                 * channel was opened? Will contain an ORed
                                 * combination of TCL_READABLE and
                                 * TCL_WRITABLE, if non-NULL. */
{
    Channel *chanPtr;		/* The actual channel. */
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    char *name;			/* Translated name. */

    /*
     * Substitute "stdin", etc.  Note that even though we immediately
     * find the channel using Tcl_GetStdChannel, we still need to look
     * it up in the specified interpreter to ensure that it is present
     * in the channel table.  Otherwise, safe interpreters would always
     * have access to the standard channels.
     */

    name = chanName;
    if ((chanName[0] == 's') && (chanName[1] == 't')) {
	chanPtr = NULL;
	if (strcmp(chanName, "stdin") == 0) {
	    chanPtr = (Channel *)Tcl_GetStdChannel(TCL_STDIN);
	} else if (strcmp(chanName, "stdout") == 0) {
	    chanPtr = (Channel *)Tcl_GetStdChannel(TCL_STDOUT);
	} else if (strcmp(chanName, "stderr") == 0) {
	    chanPtr = (Channel *)Tcl_GetStdChannel(TCL_STDERR);
	}
	if (chanPtr != NULL) {
	    name = chanPtr->channelName;
	}
    }
    
    hTblPtr = GetChannelTable(interp);
    hPtr = Tcl_FindHashEntry(hTblPtr, name);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendResult(interp, "can not find channel named \"",
                chanName, "\"", (char *) NULL);
        return NULL;
    }

    chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
    if (modePtr != NULL) {
        *modePtr = (chanPtr->flags & (TCL_READABLE|TCL_WRITABLE));
    }
    
    return (Tcl_Channel) chanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateChannel --
 *
 *	Creates a new entry in the hash table for a Tcl_Channel
 *	record.
 *
 * Results:
 *	Returns the new Tcl_Channel.
 *
 * Side effects:
 *	Creates a new Tcl_Channel instance and inserts it into the
 *	hash table.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_CreateChannel(typePtr, chanName, inFile, outFile, instanceData)
    Tcl_ChannelType *typePtr;	/* The channel type record. */
    char *chanName;		/* Name of channel to record. */
    Tcl_File inFile;		/* File to use for input, or NULL. */
    Tcl_File outFile;		/* File to use for output, or NULL. */
    ClientData instanceData;	/* Instance specific data. */
{
    Channel *chanPtr;		/* The channel structure newly created. */

    chanPtr = (Channel *) ckalloc((unsigned) sizeof(Channel));
    
    if (chanName != (char *) NULL) {
        chanPtr->channelName = ckalloc((unsigned) (strlen(chanName) + 1));
        strcpy(chanPtr->channelName, chanName);
    } else {
        panic("Tcl_CreateChannel: NULL channel name");
    }

    chanPtr->flags = 0;
    if (inFile != (Tcl_File) NULL) {
        chanPtr->flags |= TCL_READABLE;
    }
    if (outFile != (Tcl_File) NULL) {
        chanPtr->flags |= TCL_WRITABLE;
    }

    /*
     * Set the channel up initially in AUTO input translation mode to
     * accept "\n", "\r" and "\r\n". Output translation mode is set to
     * a platform specific default value. The eofChar is set to 0 for both
     * input and output, so that Tcl does not look for an in-file EOF
     * indicator (e.g. ^Z) and does not append an EOF indicator to files.
     */

    chanPtr->inputTranslation = TCL_TRANSLATE_AUTO;
    chanPtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
    chanPtr->inEofChar = 0;
    chanPtr->outEofChar = 0;

    chanPtr->unreportedError = 0;
    chanPtr->instanceData = instanceData;
    chanPtr->inFile = inFile;
    chanPtr->outFile = outFile;
    chanPtr->typePtr = typePtr;
    chanPtr->refCount = 0;
    chanPtr->closeCbPtr = (CloseCallback *) NULL;
    chanPtr->curOutPtr = (ChannelBuffer *) NULL;
    chanPtr->outQueueHead = (ChannelBuffer *) NULL;
    chanPtr->outQueueTail = (ChannelBuffer *) NULL;
    chanPtr->saveInBufPtr = (ChannelBuffer *) NULL;
    chanPtr->inQueueHead = (ChannelBuffer *) NULL;
    chanPtr->inQueueTail = (ChannelBuffer *) NULL;
    chanPtr->chPtr = (ChannelHandler *) NULL;
    chanPtr->interestMask = 0;
    chanPtr->scriptRecordPtr = (EventScriptRecord *) NULL;
    chanPtr->bufSize = CHANNELBUFFER_DEFAULT_SIZE;

    /*
     * Link the channel into the list of all channels; create an on-exit
     * handler if there is not one already, to close off all the channels
     * in the list on exit.
     */

    chanPtr->nextChanPtr = firstChanPtr;
    firstChanPtr = chanPtr;

    if (!channelExitHandlerCreated) {
        channelExitHandlerCreated = 1;
        Tcl_CreateExitHandler(CloseChannelsOnExit, (ClientData) NULL);
    }
    
    /*
     * Install this channel in the first empty standard channel slot.
     */

    if (Tcl_GetStdChannel(TCL_STDIN) == NULL) {
	Tcl_SetStdChannel((Tcl_Channel)chanPtr, TCL_STDIN);
    } else if (Tcl_GetStdChannel(TCL_STDOUT) == NULL) {
	Tcl_SetStdChannel((Tcl_Channel)chanPtr, TCL_STDOUT);
    } else if (Tcl_GetStdChannel(TCL_STDERR) == NULL) {
	Tcl_SetStdChannel((Tcl_Channel)chanPtr, TCL_STDERR);
    } 

    return (Tcl_Channel) chanPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelName --
 *
 *	Returns the string identifying the channel name.
 *
 * Results:
 *	The string containing the channel name. This memory is
 *	owned by the generic layer and should not be modified by
 *	the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetChannelName(chan)
    Tcl_Channel chan;		/* The channel for which to return the name. */
{
    Channel *chanPtr;		/* The actual channel. */

    chanPtr = (Channel *) chan;
    return chanPtr->channelName;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelType --
 *
 *	Given a channel structure, returns the channel type structure.
 *
 * Results:
 *	Returns a pointer to the channel type structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_ChannelType *
Tcl_GetChannelType(chan)
    Tcl_Channel chan;		/* The channel to return type for. */
{
    Channel *chanPtr;		/* The actual channel. */

    chanPtr = (Channel *) chan;
    return chanPtr->typePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelFile --
 *
 *	Returns a file associated with a channel.
 *
 * Results:
 *	The file or NULL if failed (e.g. the channel is not open for the
 *	requested direction).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
Tcl_GetChannelFile(chan, direction)
    Tcl_Channel chan;		/* The channel to get file from. */
    int direction;		/* TCL_WRITABLE or TCL_READABLE. */
{
    Channel *chanPtr;		/* The actual channel. */

    chanPtr = (Channel *) chan;
    switch (direction) {
        case TCL_WRITABLE:
            return chanPtr->outFile;
        case TCL_READABLE:
            return chanPtr->inFile;
        default:
            return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelInstanceData --
 *
 *	Returns the client data associated with a channel.
 *
 * Results:
 *	The client data.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_GetChannelInstanceData(chan)
    Tcl_Channel chan;		/* Channel for which to return client data. */
{
    Channel *chanPtr;		/* The actual channel. */

    chanPtr = (Channel *) chan;
    return chanPtr->instanceData;
}

/*
 *----------------------------------------------------------------------
 *
 * RecycleBuffer --
 *
 *	Helper function to recycle input and output buffers. Ensures
 *	that two input buffers are saved (one in the input queue and
 *	another in the saveInBufPtr field) and that curOutPtr is set
 *	to a buffer. Only if these conditions are met is the buffer
 *	freed to the OS.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May free a buffer to the OS.
 *
 *----------------------------------------------------------------------
 */

static void
RecycleBuffer(chanPtr, bufPtr, mustDiscard)
    Channel *chanPtr;		/* Channel for which to recycle buffers. */
    ChannelBuffer *bufPtr;	/* The buffer to recycle. */
    int mustDiscard;		/* If nonzero, free the buffer to the
                                 * OS, always. */
{
    /*
     * Do we have to free the buffer to the OS?
     */

    if (mustDiscard) {
        ckfree((char *) bufPtr);
        return;
    }
    
    /*
     * Only save buffers for the input queue if the channel is readable.
     */
    
    if (chanPtr->flags & TCL_READABLE) {
        if (chanPtr->inQueueHead == (ChannelBuffer *) NULL) {
            chanPtr->inQueueHead = bufPtr;
            chanPtr->inQueueTail = bufPtr;
            goto keepit;
        }
        if (chanPtr->saveInBufPtr == (ChannelBuffer *) NULL) {
            chanPtr->saveInBufPtr = bufPtr;
            goto keepit;
        }
    }

    /*
     * Only save buffers for the output queue if the channel is writable.
     */

    if (chanPtr->flags & TCL_WRITABLE) {
        if (chanPtr->curOutPtr == (ChannelBuffer *) NULL) {
            chanPtr->curOutPtr = bufPtr;
            goto keepit;
        }
    }

    /*
     * If we reached this code we return the buffer to the OS.
     */

    ckfree((char *) bufPtr);
    return;

keepit:
    bufPtr->nextRemoved = 0;
    bufPtr->nextAdded = 0;
    bufPtr->nextPtr = (ChannelBuffer *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DiscardOutputQueued --
 *
 *	Discards all output queued in the output queue of a channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Recycles buffers.
 *
 *----------------------------------------------------------------------
 */

static void
DiscardOutputQueued(chanPtr)
    Channel *chanPtr;		/* The channel for which to discard output. */
{
    ChannelBuffer *bufPtr;
    
    while (chanPtr->outQueueHead != (ChannelBuffer *) NULL) {
        bufPtr = chanPtr->outQueueHead;
        chanPtr->outQueueHead = bufPtr->nextPtr;
        RecycleBuffer(chanPtr, bufPtr, 0);
    }
    chanPtr->outQueueHead = (ChannelBuffer *) NULL;
    chanPtr->outQueueTail = (ChannelBuffer *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * FlushChannel --
 *
 *	This function flushes as much of the queued output as is possible
 *	now. If calledFromAsyncFlush is nonzero, it is being called in an
 *	event handler to flush channel output asynchronously.
 *
 * Results:
 *	0 if successful, else the error code that was returned by the
 *	channel type operation.
 *
 * Side effects:
 *	May produce output on a channel. May block indefinitely if the
 *	channel is synchronous. May schedule an async flush on the channel.
 *	May recycle memory for buffers in the output queue.
 *
 *----------------------------------------------------------------------
 */

static int
FlushChannel(interp, chanPtr, calledFromAsyncFlush)
    Tcl_Interp *interp;			/* For error reporting during close. */
    Channel *chanPtr;			/* The channel to flush on. */
    int calledFromAsyncFlush;		/* If nonzero then we are being
                                         * called from an asynchronous
                                         * flush callback. */
{
    ChannelBuffer *bufPtr;		/* Iterates over buffered output
                                         * queue. */
    int toWrite;			/* Amount of output data in current
                                         * buffer available to be written. */
    int written;			/* Amount of output data actually
                                         * written in current round. */
    int errorCode;			/* Stores POSIX error codes from
                                         * channel driver operations. */

    errorCode = 0;

    /*
     * Prevent writing on a dead channel -- a channel that has been closed
     * but not yet deallocated. This can occur if the exit handler for the
     * channel deallocation runs before all channels are deregistered in
     * all interpreters.
     */
    
    if (chanPtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }
    
    /*
     * Loop over the queued buffers and attempt to flush as
     * much as possible of the queued output to the channel.
     */

    while (1) {

        /*
         * If the queue is empty and there is a ready current buffer, OR if
         * the current buffer is full, then move the current buffer to the
         * queue.
         */
        
        if (((chanPtr->curOutPtr != (ChannelBuffer *) NULL) &&
                (chanPtr->curOutPtr->nextAdded == chanPtr->curOutPtr->bufSize))
                || ((chanPtr->flags & BUFFER_READY) &&
                        (chanPtr->outQueueHead == (ChannelBuffer *) NULL))) {
            chanPtr->flags &= (~(BUFFER_READY));
            chanPtr->curOutPtr->nextPtr = (ChannelBuffer *) NULL;
            if (chanPtr->outQueueHead == (ChannelBuffer *) NULL) {
                chanPtr->outQueueHead = chanPtr->curOutPtr;
            } else {
                chanPtr->outQueueTail->nextPtr = chanPtr->curOutPtr;
            }
            chanPtr->outQueueTail = chanPtr->curOutPtr;
            chanPtr->curOutPtr = (ChannelBuffer *) NULL;
        }
        bufPtr = chanPtr->outQueueHead;

        /*
         * If we are not being called from an async flush and an async
         * flush is active, we just return without producing any output.
         */

        if ((!calledFromAsyncFlush) &&
                (chanPtr->flags & BG_FLUSH_SCHEDULED)) {
            return 0;
        }

        /*
         * If the output queue is still empty, break out of the while loop.
         */

        if (bufPtr == (ChannelBuffer *) NULL) {
            break;	/* Out of the "while (1)". */
        }

        /*
         * Produce the output on the channel.
         */
        
        toWrite = bufPtr->nextAdded - bufPtr->nextRemoved;
        written = (chanPtr->typePtr->outputProc) (chanPtr->instanceData,
                chanPtr->outFile, bufPtr->buf + bufPtr->nextRemoved,
                toWrite, &errorCode);
            
	/*
         * If the write failed completely attempt to start the asynchronous
         * flush mechanism and break out of this loop - do not attempt to
         * write any more output at this time.
         */

        if (written < 0) {
            
            /*
             * If the last attempt to write was interrupted, simply retry.
             */
            
            if (errorCode == EINTR) {
                errorCode = 0;
                continue;
            }

            /*
             * If we would have blocked, attempt to set up an asynchronous
             * background flushing for this channel if the channel is
             * nonblocking, or block until more output can be written if
             * the channel is blocking.
             */

            if ((errorCode == EWOULDBLOCK) || (errorCode == EAGAIN)) {
                if (chanPtr->flags & CHANNEL_NONBLOCKING) {
                    if (!(chanPtr->flags & BG_FLUSH_SCHEDULED)) {
                        Tcl_CreateFileHandler(chanPtr->outFile,
                                TCL_WRITABLE, FlushEventProc,
                                (ClientData) chanPtr);
                    }
                    chanPtr->flags |= BG_FLUSH_SCHEDULED;
                    errorCode = 0;
                    break;	/* Out of the "while (1)" loop. */
                } else {

                    /*
                     * If the device driver did not emulate blocking behavior
                     * then we must do it it here.
                     */
                    
                    TclWaitForFile(chanPtr->outFile, TCL_WRITABLE, -1);
                    errorCode = 0;
                    continue;
                }
            }

            /*
             * Decide whether to report the error upwards or defer it. If
             * we got an error during async flush we discard all queued
             * output.
             */

            if (calledFromAsyncFlush) {
                if (chanPtr->unreportedError == 0) {
                    chanPtr->unreportedError = errorCode;
                }
            } else {
                Tcl_SetErrno(errorCode);
            }

            /*
             * When we get an error we throw away all the output
             * currently queued.
             */

            DiscardOutputQueued(chanPtr);
            continue;
        }

        bufPtr->nextRemoved += written;

        /*
         * If this buffer is now empty, recycle it.
         */

        if (bufPtr->nextRemoved == bufPtr->nextAdded) {
            chanPtr->outQueueHead = bufPtr->nextPtr;
            if (chanPtr->outQueueHead == (ChannelBuffer *) NULL) {
                chanPtr->outQueueTail = (ChannelBuffer *) NULL;
            }
            RecycleBuffer(chanPtr, bufPtr, 0);
        }
    }	/* Closes "while (1)". */
    
    /*
     * If the queue became empty and we have an asynchronous flushing
     * mechanism active, cancel the asynchronous flushing.
     */

    if ((chanPtr->outQueueHead == (ChannelBuffer *) NULL) &&
            (chanPtr->flags & BG_FLUSH_SCHEDULED)) {
        chanPtr->flags &= (~(BG_FLUSH_SCHEDULED));
        if (chanPtr->outFile != (Tcl_File) NULL) {
            Tcl_DeleteFileHandler(chanPtr->outFile);
        }
    }

    /*
     * If the channel is flagged as closed, delete it when the refcount
     * drops to zero, the output queue is empty and there is no output
     * in the current output buffer.
     */

    if ((chanPtr->flags & CHANNEL_CLOSED) && (chanPtr->refCount <= 0) &&
            (chanPtr->outQueueHead == (ChannelBuffer *) NULL) &&
            ((chanPtr->curOutPtr == (ChannelBuffer *) NULL) ||
                    (chanPtr->curOutPtr->nextAdded ==
                            chanPtr->curOutPtr->nextRemoved))) {
        return CloseChannel(interp, chanPtr, errorCode);
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * CloseChannel --
 *
 *	Utility procedure to close a channel and free its associated
 *	resources.
 *
 * Results:
 *	0 on success or a POSIX error code if the operation failed.
 *
 * Side effects:
 *	May close the actual channel; may free memory.
 *
 *----------------------------------------------------------------------
 */

static int
CloseChannel(interp, chanPtr, errorCode)
    Tcl_Interp *interp;			/* For error reporting. */
    Channel *chanPtr;			/* The channel to close. */
    int errorCode;			/* Status of operation so far. */
{
    int result = 0;			/* Of calling driver close
                                         * operation. */
    Channel *prevChanPtr;		/* Preceding channel in list of
                                         * all channels - used to splice a
                                         * channel out of the list on close. */
    
        
    /*
     * Remove the channel from the standard channel table.
     */
    
    if (Tcl_GetStdChannel(TCL_STDIN) == (Tcl_Channel) chanPtr) {
	Tcl_SetStdChannel(NULL, TCL_STDIN);
    } else if (Tcl_GetStdChannel(TCL_STDOUT) == (Tcl_Channel) chanPtr) {
	Tcl_SetStdChannel(NULL, TCL_STDOUT);
    } else if (Tcl_GetStdChannel(TCL_STDERR) == (Tcl_Channel) chanPtr) {
	Tcl_SetStdChannel(NULL, TCL_STDERR);
    } 

    /*
     * No more input can be consumed so discard any leftover input.
     */

    DiscardInputQueued(chanPtr, 1);

    /*
     * Discard a leftover buffer in the current output buffer field.
     */

    if (chanPtr->curOutPtr != (ChannelBuffer *) NULL) {
        ckfree((char *) chanPtr->curOutPtr);
        chanPtr->curOutPtr = (ChannelBuffer *) NULL;
    }
    
    /*
     * The caller guarantees that there are no more buffers
     * queued for output.
     */

    if (chanPtr->outQueueHead != (ChannelBuffer *) NULL) {
        panic("TclFlush, closed channel: queued output left");
    }

    /*
     * If the EOF character is set in the channel, append that to the
     * output device.
     */

    if ((chanPtr->outEofChar != 0) && (chanPtr->outFile != NULL)) {
        int dummy;
        char c;

        c = (char) chanPtr->outEofChar;
        if (!(chanPtr->flags & CHANNEL_DEAD)) {
            (chanPtr->typePtr->outputProc) (chanPtr->instanceData,
                    chanPtr->outFile, &c, 1, &dummy);
        }
    }

    /*
     * Remove TCL_READABLE and TCL_WRITABLE from chanPtr->flags, so
     * that close callbacks can not do input or output (assuming they
     * squirreled the channel away in their clientData). This also
     * prevents infinite loops if the callback calls any C API that
     * could call FlushChannel.
     */

    chanPtr->flags &= (~(TCL_READABLE|TCL_WRITABLE));
        
    /*
     * Splice this channel out of the list of all channels.
     */

    if (chanPtr == firstChanPtr) {
        firstChanPtr = chanPtr->nextChanPtr;
    } else {
        for (prevChanPtr = firstChanPtr;
                 (prevChanPtr != (Channel *) NULL) &&
                     (prevChanPtr->nextChanPtr != chanPtr);
                 prevChanPtr = prevChanPtr->nextChanPtr) {
            /* Empty loop body. */
        }
        if (prevChanPtr == (Channel *) NULL) {
            panic("FlushChannel: damaged channel list");
        }
        prevChanPtr->nextChanPtr = chanPtr->nextChanPtr;
    }

    /*
     * OK, close the channel itself.
     */
        
    if (!(chanPtr->flags & CHANNEL_DEAD)) {
        result = (chanPtr->typePtr->closeProc) (chanPtr->instanceData, interp,
                chanPtr->inFile, chanPtr->outFile);
    }
    if (chanPtr->channelName != (char *) NULL) {
        ckfree(chanPtr->channelName);
    }
    
    /*
     * If we are being called synchronously, report either
     * any latent error on the channel or the current error.
     */
        
    if (chanPtr->unreportedError != 0) {
        errorCode = chanPtr->unreportedError;
    }
    if (errorCode == 0) {
        errorCode = result;
        if (errorCode != 0) {
            Tcl_SetErrno(errorCode);
        }
    }

    Tcl_EventuallyFree((ClientData) chanPtr, TCL_DYNAMIC);

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Close --
 *
 *	Closes a channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Closes the channel if this is the last reference.
 *
 * NOTE:
 *	Tcl_Close removes the channel as far as the user is concerned.
 *	However, it may continue to exist for a while longer if it has
 *	a background flush scheduled. The device itself is eventually
 *	closed and the channel record removed, in CloseChannel, above.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_Close(interp, chan)
    Tcl_Interp *interp;			/* Interpreter for errors. */
    Tcl_Channel chan;			/* The channel being closed. Must
                                         * not be referenced in any
                                         * interpreter. */
{
    ChannelHandler *chPtr, *chNext;	/* Iterate over channel handlers. */
    CloseCallback *cbPtr;		/* Iterate over close callbacks
                                         * for this channel. */
    EventScriptRecord *ePtr, *eNextPtr;	/* Iterate over eventscript records. */
    Channel *chanPtr;			/* The real IO channel. */
    int result;				/* Of calling FlushChannel. */

    chanPtr = (Channel *) chan;

    if (chanPtr->refCount > 0) {
        panic("called Tcl_Close on channel with refcount > 0");
    }

    /*
     * Remove all the channel handler records attached to the channel
     * itself.
     */
        
    for (chPtr = chanPtr->chPtr;
             chPtr != (ChannelHandler *) NULL;
             chPtr = chNext) {
        chNext = chPtr->nextPtr;
        ckfree((char *) chPtr);
    }
    chanPtr->chPtr = (ChannelHandler *) NULL;

    /*
     * Must set the interest mask now to 0, otherwise infinite loops
     * will occur if Tcl_DoOneEvent is called before the channel is
     * finally deleted in FlushChannel. This can happen if the channel
     * has a background flush active.
     */
        
    chanPtr->interestMask = 0;
    
    /*
     * Remove any EventScript records for this channel.
     */

    for (ePtr = chanPtr->scriptRecordPtr;
             ePtr != (EventScriptRecord *) NULL;
             ePtr = eNextPtr) {
        eNextPtr = ePtr->nextPtr;
        Tcl_EventuallyFree((ClientData)ePtr->script, TCL_DYNAMIC);
        ckfree((char *) ePtr);
    }
    chanPtr->scriptRecordPtr = (EventScriptRecord *) NULL;
        
    /*
     * Invoke the registered close callbacks and delete their records.
     */

    while (chanPtr->closeCbPtr != (CloseCallback *) NULL) {
        cbPtr = chanPtr->closeCbPtr;
        chanPtr->closeCbPtr = cbPtr->nextPtr;
        (cbPtr->proc) (cbPtr->clientData);
        ckfree((char *) cbPtr);
    }

    /*
     * And remove any events for this channel from the event queue.
     */

    Tcl_DeleteEvents(ChannelEventDeleteProc, (ClientData) chanPtr);

    /*
     * Ensure that the last output buffer will be flushed.
     */
    
    if ((chanPtr->curOutPtr != (ChannelBuffer *) NULL) &&
           (chanPtr->curOutPtr->nextAdded > chanPtr->curOutPtr->nextRemoved)) {
        chanPtr->flags |= BUFFER_READY;
    }

    /*
     * The call to FlushChannel will flush any queued output and invoke
     * the close function of the channel driver, or it will set up the
     * channel to be flushed and closed asynchronously.
     */
    
    chanPtr->flags |= CHANNEL_CLOSED;
    result = FlushChannel(interp, chanPtr, 0);
    if (result != 0) {
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelEventDeleteProc --
 *
 *	This procedure returns 1 if the event passed in is for the
 *	channel passed in as the second argument. This procedure is
 *	used as a filter for events to delete in a call to
 *	Tcl_DeleteEvents in CloseChannel.
 *
 * Results:
 *	1 if matching, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ChannelEventDeleteProc(evPtr, clientData)
    Tcl_Event *evPtr;		/* The event to check for a match. */
    ClientData clientData;	/* The channel to check for. */
{
    ChannelHandlerEvent *cEvPtr;
    Channel *chanPtr;

    if (evPtr->proc != ChannelHandlerEventProc) {
        return 0;
    }
    cEvPtr = (ChannelHandlerEvent *) evPtr;
    chanPtr = (Channel *) clientData;
    if (cEvPtr->chanPtr != chanPtr) {
        return 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Write --
 *
 *	Puts a sequence of characters into an output buffer, may queue the
 *	buffer for output if it gets full, and also remembers whether the
 *	current buffer is ready e.g. if it contains a newline and we are in
 *	line buffering mode.
 *
 * Results:
 *	The number of bytes written or -1 in case of error. If -1,
 *	Tcl_GetErrno will return the error code.
 *
 * Side effects:
 *	May buffer up output and may cause output to be produced on the
 *	channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Write(chan, srcPtr, slen)
    Tcl_Channel chan;			/* The channel to buffer output for. */
    char *srcPtr;			/* Output to buffer. */
    int slen;				/* Its length. Negative means
                                         * the output is null terminated
                                         * and we must compute its length. */
{
    Channel *chanPtr;			/* The actual channel. */
    ChannelBuffer *outBufPtr;		/* Current output buffer. */
    int foundNewline;			/* Did we find a newline in output? */
    char *dPtr, *sPtr;			/* Search variables for newline. */
    int crsent;				/* In CRLF eol translation mode,
                                         * remember the fact that a CR was
                                         * output to the channel without
                                         * its following NL. */
    int i;				/* Loop index for newline search. */
    int destCopied;			/* How many bytes were used in this
                                         * destination buffer to hold the
                                         * output? */
    int totalDestCopied;		/* How many bytes total were
                                         * copied to the channel buffer? */
    int srcCopied;			/* How many bytes were copied from
                                         * the source string? */
    char *destPtr;			/* Where in line to copy to? */

    chanPtr = (Channel *) chan;

    /*
     * Check for unreported error.
     */

    if (chanPtr->unreportedError != 0) {
        Tcl_SetErrno(chanPtr->unreportedError);
        chanPtr->unreportedError = 0;
        return -1;
    }
    
    /*
     * If the channel is not open for writing punt.
     */

    if (!(chanPtr->flags & TCL_WRITABLE)) {
        Tcl_SetErrno(EACCES);
        return -1;
    }
    
    /*
     * If length passed is negative, assume that the output is null terminated
     * and compute its length.
     */
    
    if (slen < 0) {
        slen = strlen(srcPtr);
    }
    
    /*
     * If we are in network (or windows) translation mode, record the fact
     * that we have not yet sent a CR to the channel.
     */

    crsent = 0;
    
    /*
     * Loop filling buffers and flushing them until all output has been
     * consumed.
     */

    srcCopied = 0;
    totalDestCopied = 0;

    while (slen > 0) {
        
        /*
         * Make sure there is a current output buffer to accept output.
         */

        if (chanPtr->curOutPtr == (ChannelBuffer *) NULL) {
            chanPtr->curOutPtr = (ChannelBuffer *) ckalloc((unsigned)
                    (CHANNELBUFFER_HEADER_SIZE + chanPtr->bufSize));
            chanPtr->curOutPtr->nextAdded = 0;
            chanPtr->curOutPtr->nextRemoved = 0;
            chanPtr->curOutPtr->bufSize = chanPtr->bufSize;
            chanPtr->curOutPtr->nextPtr = (ChannelBuffer *) NULL;
        }

        outBufPtr = chanPtr->curOutPtr;

        destCopied = outBufPtr->bufSize - outBufPtr->nextAdded;
        if (destCopied > slen) {
            destCopied = slen;
        }
        
        destPtr = outBufPtr->buf + outBufPtr->nextAdded;
        switch (chanPtr->outputTranslation) {
            case TCL_TRANSLATE_LF:
                srcCopied = destCopied;
                memcpy((VOID *) destPtr, (VOID *) srcPtr, (size_t) destCopied);
                break;
            case TCL_TRANSLATE_CR:
                srcCopied = destCopied;
                memcpy((VOID *) destPtr, (VOID *) srcPtr, (size_t) destCopied);
                for (dPtr = destPtr; dPtr < destPtr + destCopied; dPtr++) {
                    if (*dPtr == '\n') {
                        *dPtr = '\r';
                    }
                }
                break;
            case TCL_TRANSLATE_CRLF:
                for (srcCopied = 0, dPtr = destPtr, sPtr = srcPtr;
                     dPtr < destPtr + destCopied;
                     dPtr++, sPtr++, srcCopied++) {
                    if (*sPtr == '\n') {
                        if (crsent) {
                            *dPtr = '\n';
                            crsent = 0;
                        } else {
                            *dPtr = '\r';
                            crsent = 1;
                            sPtr--, srcCopied--;
                        }
                    } else {
                        *dPtr = *sPtr;
                    }
                }
                break;
            case TCL_TRANSLATE_AUTO:
                panic("Tcl_Write: AUTO output translation mode not supported");
            default:
                panic("Tcl_Write: unknown output translation mode");
        }

        /*
         * The current buffer is ready for output if it is full, or if it
         * contains a newline and this channel is line-buffered, or if it
         * contains any output and this channel is unbuffered.
         */

        outBufPtr->nextAdded += destCopied;
        if (!(chanPtr->flags & BUFFER_READY)) {
            if (outBufPtr->nextAdded == outBufPtr->bufSize) {
                chanPtr->flags |= BUFFER_READY;
            } else if (chanPtr->flags & CHANNEL_LINEBUFFERED) {
                for (sPtr = srcPtr, i = 0, foundNewline = 0;
                         (i < srcCopied) && (!foundNewline);
                         i++, sPtr++) {
                    if (*sPtr == '\n') {
                        foundNewline = 1;
                        break;
                    }
                }
                if (foundNewline) {
                    chanPtr->flags |= BUFFER_READY;
                }
            } else if (chanPtr->flags & CHANNEL_UNBUFFERED) {
                chanPtr->flags |= BUFFER_READY;
            }
        }
        
        totalDestCopied += srcCopied;
        srcPtr += srcCopied;
        slen -= srcCopied;

        if (chanPtr->flags & BUFFER_READY) {
            if (FlushChannel(NULL, chanPtr, 0) != 0) {
                return -1;
            }
        }
    } /* Closes "while" */

    return totalDestCopied;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Flush --
 *
 *	Flushes output data on a channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May flush output queued on this channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Flush(chan)
    Tcl_Channel chan;			/* The Channel to flush. */
{
    int result;				/* Of calling FlushChannel. */
    Channel *chanPtr;			/* The actual channel. */

    chanPtr = (Channel *) chan;

    /*
     * Check for unreported error.
     */

    if (chanPtr->unreportedError != 0) {
        Tcl_SetErrno(chanPtr->unreportedError);
        chanPtr->unreportedError = 0;
        return TCL_ERROR;
    }

    /*
     * If the channel is not open for writing punt.
     */

    if (!(chanPtr->flags & TCL_WRITABLE)) {
        Tcl_SetErrno(EACCES);
        return TCL_ERROR;
    }
    
    /*
     * Force current output buffer to be output also.
     */
    
    if ((chanPtr->curOutPtr != (ChannelBuffer *) NULL) &&
            (chanPtr->curOutPtr->nextAdded > 0)) {
        chanPtr->flags |= BUFFER_READY;
    }
    
    result = FlushChannel(NULL, chanPtr, 0);
    if (result != 0) {
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DiscardInputQueued --
 *
 *	Discards any input read from the channel but not yet consumed
 *	by Tcl reading commands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May discard input from the channel. If discardLastBuffer is zero,
 *	leaves one buffer in place for back-filling.
 *
 *----------------------------------------------------------------------
 */

static void
DiscardInputQueued(chanPtr, discardSavedBuffers)
    Channel *chanPtr;		/* Channel on which to discard
                                 * the queued input. */
    int discardSavedBuffers;	/* If non-zero, discard all buffers including
                                 * last one. */
{
    ChannelBuffer *bufPtr, *nxtPtr;	/* Loop variables. */

    bufPtr = chanPtr->inQueueHead;
    chanPtr->inQueueHead = (ChannelBuffer *) NULL;
    chanPtr->inQueueTail = (ChannelBuffer *) NULL;
    for (; bufPtr != (ChannelBuffer *) NULL; bufPtr = nxtPtr) {
        nxtPtr = bufPtr->nextPtr;
        RecycleBuffer(chanPtr, bufPtr, discardSavedBuffers);
    }

    /*
     * If discardSavedBuffers is nonzero, must also discard any previously
     * saved buffer in the saveInBufPtr field.
     */
    
    if (discardSavedBuffers) {
        if (chanPtr->saveInBufPtr != (ChannelBuffer *) NULL) {
            ckfree((char *) chanPtr->saveInBufPtr);
            chanPtr->saveInBufPtr = (ChannelBuffer *) NULL;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetInput --
 *
 *	Reads input data from a device or file into an input buffer.
 *
 * Results:
 *	A Posix error code or 0.
 *
 * Side effects:
 *	Reads from the underlying device.
 *
 *----------------------------------------------------------------------
 */

static int
GetInput(chanPtr)
    Channel *chanPtr;			/* Channel to read input from. */
{
    int toRead;				/* How much to read? */
    int result;				/* Of calling driver. */
    int nread;				/* How much was read from channel? */
    ChannelBuffer *bufPtr;		/* New buffer to add to input queue. */

    /*
     * Prevent reading from a dead channel -- a channel that has been closed
     * but not yet deallocated, which can happen if the exit handler for
     * channel cleanup has run but the channel is still registered in some
     * interpreter.
     */
    
    if (chanPtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }

    /*
     * See if we can fill an existing buffer. If we can, read only
     * as much as will fit in it. Otherwise allocate a new buffer,
     * add it to the input queue and attempt to fill it to the max.
     */

    if ((chanPtr->inQueueTail != (ChannelBuffer *) NULL) &&
           (chanPtr->inQueueTail->nextAdded < chanPtr->inQueueTail->bufSize)) {
        bufPtr = chanPtr->inQueueTail;
        toRead = bufPtr->bufSize - bufPtr->nextAdded;
    } else {
	if (chanPtr->saveInBufPtr != (ChannelBuffer *) NULL) {
	    bufPtr = chanPtr->saveInBufPtr;
	    chanPtr->saveInBufPtr = (ChannelBuffer *) NULL;
	} else {
	    bufPtr = (ChannelBuffer *) ckalloc(
		((unsigned) CHANNELBUFFER_HEADER_SIZE + chanPtr->bufSize));
	    bufPtr->bufSize = chanPtr->bufSize;
	}
	bufPtr->nextRemoved = 0;
	bufPtr->nextAdded = 0;
        toRead = bufPtr->bufSize;
        if (chanPtr->inQueueTail == (ChannelBuffer *) NULL) {
            chanPtr->inQueueHead = bufPtr;
        } else {
            chanPtr->inQueueTail->nextPtr = bufPtr;
        }
        chanPtr->inQueueTail = bufPtr;
        bufPtr->nextPtr = (ChannelBuffer *) NULL;
    }
      
    while (1) {
    
        /*
         * If EOF is set, we should avoid calling the driver because on some
         * platforms it is impossible to read from a device after EOF.
         */

        if (chanPtr->flags & CHANNEL_EOF) {
	    break;
        }
        nread = (chanPtr->typePtr->inputProc) (chanPtr->instanceData,
                chanPtr->inFile, bufPtr->buf + bufPtr->nextAdded,
                toRead, &result);
        if (nread == 0) {
            chanPtr->flags |= CHANNEL_EOF;
            break;
        } else if (nread < 0) {
            if ((result == EWOULDBLOCK) || (result == EAGAIN)) {
                chanPtr->flags |= CHANNEL_BLOCKED;
                result = EAGAIN;
                if (chanPtr->flags & CHANNEL_NONBLOCKING) {
                    Tcl_SetErrno(result);
                    return result;
                } else {

                    /*
                     * If the device driver did not emulate blocking behavior
                     * then we have to do it here.
                     */
                    
                    TclWaitForFile(chanPtr->inFile, TCL_READABLE, -1);
                }
            } else {
                Tcl_SetErrno(result);
                return result;
            }
        } else {
            bufPtr->nextAdded += nread;

            /*
             * If we get a short read, signal up that we may be BLOCKED. We
             * should avoid calling the driver because on some platforms we
             * will block in the low level reading code even though the
             * channel is set into nonblocking mode.
             */
            
            if (nread < toRead) {
                chanPtr->flags |= CHANNEL_BLOCKED;
            }
            break;
        }
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CopyAndTranslateBuffer --
 *
 *	Copy at most one buffer of input to the result space, doing
 *	eol translations according to mode in effect currently.
 *
 * Results:
 *	Number of characters (as opposed to bytes) copied. May return
 *	zero if no input is available to be translated.
 *
 * Side effects:
 *	Consumes buffered input. May deallocate one buffer.
 *
 *----------------------------------------------------------------------
 */

static int
CopyAndTranslateBuffer(chanPtr, result, space)
    Channel *chanPtr;		/* The channel from which to read input. */
    char *result;		/* Where to store the copied input. */
    int space;			/* How many bytes are available in result
                                 * to store the copied input? */
{
    int bytesInBuffer;		/* How many bytes are available to be
                                 * copied in the current input buffer? */
    int copied;			/* How many characters were already copied
                                 * into the destination space? */
    ChannelBuffer *bufPtr;	/* The buffer from which to copy bytes. */
    char curByte;		/* The byte we are currently translating. */
    int i;			/* Iterates over the copied input looking
                                 * for the input eofChar. */
    
    /*
     * If there is no input at all, return zero. The invariant is that either
     * there is no buffer in the queue, or if the first buffer is empty, it
     * is also the last buffer (and thus there is no input in the queue).
     * Note also that if the buffer is empty, we leave it in the queue.
     */
    
    if (chanPtr->inQueueHead == (ChannelBuffer *) NULL) {
        return 0;
    }
    bufPtr = chanPtr->inQueueHead;
    bytesInBuffer = bufPtr->nextAdded - bufPtr->nextRemoved;
    if (bytesInBuffer < space) {
        space = bytesInBuffer;
    }
    copied = 0;
    switch (chanPtr->inputTranslation) {
        case TCL_TRANSLATE_LF:

            if (space == 0) {
                return 0;
            }
            
	    /*
             * Copy the current chunk into the result buffer.
             */

            memcpy((VOID *) result,
                    (VOID *)(bufPtr->buf + bufPtr->nextRemoved),
                    (size_t) space);
            bufPtr->nextRemoved += space;
            copied = space;
            break;

        case TCL_TRANSLATE_CR:

            if (space == 0) {
                return 0;
            }

	    /*
             * Copy the current chunk into the result buffer, then
             * replace all \r with \n.
             */

            memcpy((VOID *) result,
                    (VOID *)(bufPtr->buf + bufPtr->nextRemoved),
                    (size_t) space);
            bufPtr->nextRemoved += space;
            for (copied = 0; copied < space; copied++) {
                if (result[copied] == '\r') {
                    result[copied] = '\n';
                }
            }
            break;

        case TCL_TRANSLATE_CRLF:

            /*
             * If there is a held-back "\r" at EOF, produce it now.
             */
            
            if (space == 0) {
                if ((chanPtr->flags & (INPUT_SAW_CR | CHANNEL_EOF)) ==
                        (INPUT_SAW_CR | CHANNEL_EOF)) {
                    result[0] = '\r';
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    return 1;
                }
                return 0;
            }

            /*
             * Copy the current chunk and replace "\r\n" with "\n"
             * (but not standalone "\r"!).
             */

            for (copied = 0;
                     (copied < space) &&
                         (bufPtr->nextRemoved < bufPtr->nextAdded);
                     copied++) {
                curByte = bufPtr->buf[bufPtr->nextRemoved];
                bufPtr->nextRemoved++;
                if (curByte == '\r') {
                    if (chanPtr->flags & INPUT_SAW_CR) {
                        result[copied] = '\r';
                    } else {
                        chanPtr->flags |= INPUT_SAW_CR;
                        copied--;
                    }
                } else if (curByte == '\n') {
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    result[copied] = '\n';
                } else {
                    if (chanPtr->flags & INPUT_SAW_CR) {
                        chanPtr->flags &= (~(INPUT_SAW_CR));
                        result[copied] = '\r';
                        copied++;
                    }
                    result[copied] = curByte;
                }
            }
            break;
                
        case TCL_TRANSLATE_AUTO:
            
            if (space == 0) {
                return 0;
            }

            /*
             * Loop over the current buffer, converting "\r" and "\r\n"
             * to "\n".
             */

            for (copied = 0;
                     (copied < space) &&
                         (bufPtr->nextRemoved < bufPtr->nextAdded); ) {
                curByte = bufPtr->buf[bufPtr->nextRemoved];
                bufPtr->nextRemoved++;
                if (curByte == '\r') {
                    result[copied] = '\n';
		    copied++;
                    if (bufPtr->nextRemoved < bufPtr->nextAdded) {
                        if (bufPtr->buf[bufPtr->nextRemoved] == '\n') {
                            bufPtr->nextRemoved++;
                        }
                        chanPtr->flags &= (~(INPUT_SAW_CR));
                    } else {
                        chanPtr->flags |= INPUT_SAW_CR;
                    }
                } else {
                    if (curByte == '\n') {
                        if (!(chanPtr->flags & INPUT_SAW_CR)) {
                            result[copied] = '\n';
			    copied++;
                        }
                    } else {
                        result[copied] = curByte;
			copied++;
                    }
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                }
            }
            break;

        default:
            panic("unknown eol translation mode");
    }

    /*
     * If an in-stream EOF character is set for this channel,, check that
     * the input we copied so far does not contain the EOF char. If it does,
     * copy only up to and excluding that character.
     */
    
    if (chanPtr->inEofChar != 0) {
        for (i = 0; i < copied; i++) {
            if (result[i] == (char) chanPtr->inEofChar) {
                break;
            }
        }
        if (i < copied) {

            /*
             * Set sticky EOF so that no further input is presented
             * to the caller.
             */
            
            chanPtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);

            /*
             * Reset the start of valid data in the input buffer to the
             * position of the eofChar, so that subsequent reads will
             * encounter it immediately. First we set it to the position
             * of the last byte consumed if all result bytes were the
             * product of one input byte; since it is possible that "\r\n"
             * contracted to "\n" in the result, we have to search back
             * from that position until we find the eofChar, because it
             * is possible that its actual position in the buffer is n
             * bytes further back (n is the number of "\r\n" sequences
             * that were contracted to "\n" in the result).
             */
                  
            bufPtr->nextRemoved -= (copied - i);
            while ((bufPtr->nextRemoved > 0) &&
                    (bufPtr->buf[bufPtr->nextRemoved] !=
                            (char) chanPtr->inEofChar)) {
                bufPtr->nextRemoved--;
            }
            copied = i;
        }
    }

    /*
     * If the current buffer is empty recycle it.
     */

    if (bufPtr->nextRemoved == bufPtr->nextAdded) {
        chanPtr->inQueueHead = bufPtr->nextPtr;
        if (chanPtr->inQueueHead == (ChannelBuffer *) NULL) {
            chanPtr->inQueueTail = (ChannelBuffer *) NULL;
        }
        RecycleBuffer(chanPtr, bufPtr, 0);
    }

    /*
     * Return the number of characters copied into the result buffer.
     * This may be different from the number of bytes consumed, because
     * of EOL translations.
     */

    return copied;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanBufferForEOL --
 *
 *	Scans one buffer for EOL according to the specified EOL
 *	translation mode. If it sees the input eofChar for the channel
 *	it stops also.
 *
 * Results:
 *	TRUE if EOL is found, FALSE otherwise. Also sets output parameter
 *	bytesToEOLPtr to the number of bytes so far to EOL, and crSeenPtr
 *	to whether a "\r" was seen.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ScanBufferForEOL(chanPtr, bufPtr, translation, eofChar, bytesToEOLPtr,
                 crSeenPtr)
    Channel *chanPtr;
    ChannelBuffer *bufPtr;		/* Buffer to scan for EOL. */
    Tcl_EolTranslation translation;	/* Translation mode to use. */
    int eofChar;			/* EOF char to look for. */
    int *bytesToEOLPtr;			/* Running counter. */
    int *crSeenPtr;			/* Has "\r" been seen? */
{
    char *rPtr;				/* Iterates over input string. */
    char *sPtr;				/* Where to stop search? */
    int EOLFound;
    int bytesToEOL;
    
    for (EOLFound = 0, rPtr = bufPtr->buf + bufPtr->nextRemoved,
             sPtr = bufPtr->buf + bufPtr->nextAdded,
             bytesToEOL = *bytesToEOLPtr;
             (!EOLFound) && (rPtr < sPtr);
             rPtr++) {
        switch (translation) {
            case TCL_TRANSLATE_AUTO:
                if ((*rPtr == (char) eofChar) && (eofChar != 0)) {
                    chanPtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
                    EOLFound = 1;
                } else if (*rPtr == '\n') {

		    /*
                     * CopyAndTranslateBuffer wants to know the length
                     * of the result, not the input. The input is one
                     * larger because "\r\n" shrinks to "\n".
                     */

                    if (!(*crSeenPtr)) {
                        bytesToEOL++;
			EOLFound = 1;
                    } else {

			/*
			 * This is a lf at the begining of a buffer
			 * where the previous buffer ended in a cr.
			 * Consume this lf because we've already emitted
			 * the newline for this crlf sequence. ALSO, if
                         * bytesToEOL is 0 (which means that we are at the
                         * first character of the scan), unset the
                         * INPUT_SAW_CR flag in the channel, because we
                         * already handled it; leaving it set would cause
                         * CopyAndTranslateBuffer to potentially consume
                         * another lf if one follows the current byte.
			 */

			bufPtr->nextRemoved++;
                        *crSeenPtr = 0;
                        chanPtr->flags &= (~(INPUT_SAW_CR));
		    }
                } else if (*rPtr == '\r') {
                    bytesToEOL++;
                    EOLFound = 1;
                } else {
                    *crSeenPtr = 0;
                    bytesToEOL++;
                }
                break;
            case TCL_TRANSLATE_LF:
                if ((*rPtr == (char) eofChar) && (eofChar != 0)) {
                    chanPtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
                    EOLFound = 1;
                } else {
                    if (*rPtr == '\n') {
                        EOLFound = 1;
                    }
                    bytesToEOL++;
                }
                break;
            case TCL_TRANSLATE_CR:
                if ((*rPtr == (char) eofChar) && (eofChar != 0)) {
                    chanPtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
                    EOLFound = 1;
                } else {
                    if (*rPtr == '\r') {
                        EOLFound = 1;
                    }
                    bytesToEOL++;
                }
                break;
            case TCL_TRANSLATE_CRLF:
                if ((*rPtr == (char) eofChar) && (eofChar != 0)) {
                    chanPtr->flags |= (CHANNEL_EOF | CHANNEL_STICKY_EOF);
                    EOLFound = 1;
                } else if (*rPtr == '\n') {

                    /*
                     * CopyAndTranslateBuffer wants to know the length
                     * of the result, not the input. The input is one
                     * larger because crlf shrinks to lf.
                     */

                    if (*crSeenPtr) {
                        EOLFound = 1;
                    } else {
                        bytesToEOL++;
                    }
                } else {
                    if (*rPtr == '\r') {
                        *crSeenPtr = 1;
                    } else {
                        *crSeenPtr = 0;
                    }
                    bytesToEOL++;
                }
                break;
            default:
                panic("unknown eol translation mode");
        }
    }

    *bytesToEOLPtr = bytesToEOL;
    return EOLFound;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanInputForEOL --
 *
 *	Scans queued input for chanPtr for an end of line (according to the
 *	current EOL translation mode) and returns the number of bytes
 *	upto and including the end of line, or -1 if none was found.
 *
 * Results:
 *	Count of bytes upto and including the end of line if one is present
 *	or -1 if none was found. Also returns in an output parameter the
 *	number of bytes queued if no end of line was found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ScanInputForEOL(chanPtr, bytesQueuedPtr)
    Channel *chanPtr;	/* Channel for which to scan queued
                                 * input for end of line. */
    int *bytesQueuedPtr;	/* Where to store the number of bytes
                                 * currently queued if no end of line
                                 * was found. */
{
    ChannelBuffer *bufPtr;	/* Iterates over queued buffers. */
    int bytesToEOL;		/* How many bytes to end of line? */
    int EOLFound;		/* Did we find an end of line? */
    int crSeen;			/* Did we see a "\r" in CRLF mode? */

    *bytesQueuedPtr = 0;
    bytesToEOL = 0;
    EOLFound = 0;
    for (bufPtr = chanPtr->inQueueHead,
             crSeen = (chanPtr->flags & INPUT_SAW_CR) ? 1 : 0;
            (!EOLFound) && (bufPtr != (ChannelBuffer *) NULL);
            bufPtr = bufPtr->nextPtr) {
        EOLFound = ScanBufferForEOL(chanPtr, bufPtr, chanPtr->inputTranslation,
                chanPtr->inEofChar, &bytesToEOL, &crSeen);
    }

    if (EOLFound == 0) {
        *bytesQueuedPtr = bytesToEOL;
        return -1;
    }
    return bytesToEOL;        
}

/*
 *----------------------------------------------------------------------
 *
 * GetEOL --
 *
 *	Accumulate input into the channel input buffer queue until an
 *	end of line has been seen.
 *
 * Results:
 *	Number of bytes buffered or -1 on failure.
 *
 * Side effects:
 *	Consumes input from the channel.
 *
 *----------------------------------------------------------------------
 */

static int
GetEOL(chanPtr)
    Channel *chanPtr;	/* Channel to queue input on. */
{
    int result;			/* Of getting another buffer from the
                                 * channel. */
    int bytesToEOL;		/* How many bytes in buffer up to and
                                 * including the end of line? */
    int bytesQueued;		/* How many bytes are queued currently
                                 * in the input chain of the channel? */

    while (1) {
        bytesToEOL = ScanInputForEOL(chanPtr, &bytesQueued);
        if (bytesToEOL > 0) {
            chanPtr->flags &= (~(CHANNEL_BLOCKED));
            return bytesToEOL;
        }
        if (chanPtr->flags & CHANNEL_EOF) {
	    /*
	     * Boundary case where cr was at the end of the previous buffer
	     * and this buffer just has a newline.  At EOF our caller wants
	     * to see -1 for the line length.
	     */
            return (bytesQueued == 0) ? -1 : bytesQueued ;
        }
        if (chanPtr->flags & CHANNEL_BLOCKED) {
            if (chanPtr->flags & CHANNEL_NONBLOCKING) {
                return -1;
            }
            chanPtr->flags &= (~(CHANNEL_BLOCKED));
        }
        result = GetInput(chanPtr);
        if (result != 0) {
            if (result == EAGAIN) {
                chanPtr->flags |= CHANNEL_BLOCKED;
            }
            return -1;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Read --
 *
 *	Reads a given number of characters from a channel.
 *
 * Results:
 *	The number of characters read, or -1 on error. Use Tcl_GetErrno()
 *	to retrieve the error code for the error that occurred.
 *
 * Side effects:
 *	May cause input to be buffered.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Read(chan, bufPtr, toRead)
    Tcl_Channel chan;		/* The channel from which to read. */
    char *bufPtr;		/* Where to store input read. */
    int toRead;			/* Maximum number of characters to read. */
{
    Channel *chanPtr;		/* The real IO channel. */
    int copied;			/* How many characters were copied into
                                 * the result string? */
    int copiedNow;		/* How many characters were copied from
                                 * the current input buffer? */
    int result;			/* Of calling GetInput. */
    
    chanPtr = (Channel *) chan;

    /*
     * Check for unreported error.
     */

    if (chanPtr->unreportedError != 0) {
        Tcl_SetErrno(chanPtr->unreportedError);
        chanPtr->unreportedError = 0;
        return -1;
    }

    /*
     * Punt if the channel is not opened for reading.
     */

    if (!(chanPtr->flags & TCL_READABLE)) {
        Tcl_SetErrno(EACCES);
        return -1;
    }
    
    /*
     * If we have not encountered a sticky EOF, clear the EOF bit. Either
     * way clear the BLOCKED bit. We want to discover these anew during
     * each operation.
     */

    if (!(chanPtr->flags & CHANNEL_STICKY_EOF)) {
        chanPtr->flags &= (~(CHANNEL_EOF));
    }
    chanPtr->flags &= (~(CHANNEL_BLOCKED));
    
    for (copied = 0; copied < toRead; copied += copiedNow) {
        copiedNow = CopyAndTranslateBuffer(chanPtr, bufPtr + copied,
                toRead - copied);
        if (copiedNow == 0) {
            if (chanPtr->flags & CHANNEL_EOF) {
                return copied;
            }
            if (chanPtr->flags & CHANNEL_BLOCKED) {
                if (chanPtr->flags & CHANNEL_NONBLOCKING) {
                    return copied;
                }
                chanPtr->flags &= (~(CHANNEL_BLOCKED));
            }
            result = GetInput(chanPtr);
            if (result != 0) {
                if (result == EAGAIN) {
                    return copied;
                }
                return -1;
            }
        }
    }
    chanPtr->flags &= (~(CHANNEL_BLOCKED));
    return copied;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Gets --
 *
 *	Reads a complete line of input from the channel.
 *
 * Results:
 *	Length of line read or -1 if error, EOF or blocked. If -1, use
 *	Tcl_GetErrno() to retrieve the POSIX error code for the
 *	error or condition that occurred.
 *
 * Side effects:
 *	May flush output on the channel. May cause input to be
 *	consumed from the channel.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Gets(chan, lineRead)
    Tcl_Channel chan;		/* Channel from which to read. */
    Tcl_DString *lineRead;	/* The characters of the line read
                                 * (excluding the terminating newline if
                                 * present) will be appended to this
                                 * DString. The caller must have initialized
                                 * it and is responsible for managing the
                                 * storage. */
{
    Channel *chanPtr;		/* The channel to read from. */
    char *buf;			/* Points into DString where data
                                 * will be stored. */
    int offset;			/* Offset from start of DString at
                                 * which to append the line just read. */
    int copiedTotal;		/* Accumulates total length of input copied. */
    int copiedNow;		/* How many bytes were copied from the
                                 * current input buffer? */
    int lineLen;		/* Length of line read, including the
                                 * translated newline. If this is zero
                                 * and neither EOF nor BLOCKED is set,
                                 * the current line is empty. */
    
    chanPtr = (Channel *) chan;

    /*
     * Check for unreported error.
     */

    if (chanPtr->unreportedError != 0) {
        Tcl_SetErrno(chanPtr->unreportedError);
        chanPtr->unreportedError = 0;
        return -1;
    }

    /*
     * Punt if the channel is not opened for reading.
     */

    if (!(chanPtr->flags & TCL_READABLE)) {
        Tcl_SetErrno(EACCES);
        return -1;
    }

    /*
     * If we have not encountered a sticky EOF, clear the EOF bit
     * (sticky EOF is set if we have seen the input eofChar, to prevent
     * reading beyond the eofChar). Also, always clear the BLOCKED bit.
     * We want to discover these conditions anew in each operation.
     */
    
    if (!(chanPtr->flags & CHANNEL_STICKY_EOF)) {
        chanPtr->flags &= (~(CHANNEL_EOF));
    }
    chanPtr->flags &= (~(CHANNEL_BLOCKED));
    lineLen = GetEOL(chanPtr);
    if (lineLen < 0) {
        return -1;
    }
    if (lineLen == 0) {
        if (chanPtr->flags & (CHANNEL_EOF | CHANNEL_BLOCKED)) {
            return -1;
        }
        return 0;
    }
    offset = Tcl_DStringLength(lineRead);
    Tcl_DStringSetLength(lineRead, lineLen + offset);
    buf = Tcl_DStringValue(lineRead) + offset;

    for (copiedTotal = 0; copiedTotal < lineLen; copiedTotal += copiedNow) {
        copiedNow = CopyAndTranslateBuffer(chanPtr, buf + copiedTotal,
                lineLen - copiedTotal);
    }
    if ((copiedTotal > 0) && (buf[copiedTotal - 1] == '\n')) {
        copiedTotal--;
    }
    Tcl_DStringSetLength(lineRead, copiedTotal + offset);
    return copiedTotal;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Seek --
 *
 *	Implements seeking on Tcl Channels. This is a public function
 *	so that other C facilities may be implemented on top of it.
 *
 * Results:
 *	The new access point or -1 on error. If error, use Tcl_GetErrno()
 *	to retrieve the POSIX error code for the error that occurred.
 *
 * Side effects:
 *	May flush output on the channel. May discard queued input.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Seek(chan, offset, mode)
    Tcl_Channel chan;		/* The channel on which to seek. */
    int offset;			/* Offset to seek to. */
    int mode;			/* Relative to which location to seek? */
{
    Channel *chanPtr;	/* The real IO channel. */
    ChannelBuffer *bufPtr;	/* Iterates over queued input
                                 * and output buffers. */
    int inputBuffered, outputBuffered;
    int result;			/* Of device driver operations. */
    int curPos;			/* Position on the device. */
    int wasAsync;		/* Was the channel nonblocking before the
                                 * seek operation? If so, must restore to
                                 * nonblocking mode after the seek. */

    chanPtr = (Channel *) chan;

    /*
     * Check for unreported error.
     */

    if (chanPtr->unreportedError != 0) {
        Tcl_SetErrno(chanPtr->unreportedError);
        chanPtr->unreportedError = 0;
        return -1;
    }

    /*
     * Disallow seek on channels that are open for neither writing nor
     * reading (e.g. socket server channels).
     */

    if (!(chanPtr->flags & (TCL_WRITABLE|TCL_READABLE))) {
        Tcl_SetErrno(EACCES);
        return -1;
    }

    /*
     * Disallow seek on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (chanPtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }
    
    /*
     * Disallow seek on channels whose type does not have a seek procedure
     * defined. This means that the channel does not support seeking.
     */

    if (chanPtr->typePtr->seekProc == (Tcl_DriverSeekProc *) NULL) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }

    /*
     * Compute how much input and output is buffered. If both input and
     * output is buffered, cannot compute the current position.
     */

    for (bufPtr = chanPtr->inQueueHead, inputBuffered = 0;
             bufPtr != (ChannelBuffer *) NULL;
             bufPtr = bufPtr->nextPtr) {
        inputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    for (bufPtr = chanPtr->outQueueHead, outputBuffered = 0;
             bufPtr != (ChannelBuffer *) NULL;
             bufPtr = bufPtr->nextPtr) {
        outputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    if ((chanPtr->curOutPtr != (ChannelBuffer *) NULL) &&
           (chanPtr->curOutPtr->nextAdded > chanPtr->curOutPtr->nextRemoved)) {
        chanPtr->flags |= BUFFER_READY;
        outputBuffered +=
            (chanPtr->curOutPtr->nextAdded - chanPtr->curOutPtr->nextRemoved);
    }
    if ((inputBuffered != 0) && (outputBuffered != 0)) {
        Tcl_SetErrno(EFAULT);
        return -1;
    }

    /*
     * If we are seeking relative to the current position, compute the
     * corrected offset taking into account the amount of unread input.
     */

    if (mode == SEEK_CUR) {
        offset -= inputBuffered;
    }

    /*
     * Discard any queued input - this input should not be read after
     * the seek.
     */

    DiscardInputQueued(chanPtr, 0);

    /*
     * Reset EOF and BLOCKED flags. We invalidate them by moving the
     * access point. Also clear CR related flags.
     */

    chanPtr->flags &=
        (~(CHANNEL_EOF | CHANNEL_STICKY_EOF | CHANNEL_BLOCKED | INPUT_SAW_CR));
    
    /*
     * If the channel is in asynchronous output mode, switch it back
     * to synchronous mode and cancel any async flush that may be
     * scheduled. After the flush, the channel will be put back into
     * asynchronous output mode.
     */

    wasAsync = 0;
    if (chanPtr->flags & CHANNEL_NONBLOCKING) {
        wasAsync = 1;
        result = 0;
        if (chanPtr->typePtr->blockModeProc != NULL) {
            result = (chanPtr->typePtr->blockModeProc) (chanPtr->instanceData,
                    chanPtr->inFile, chanPtr->outFile, TCL_MODE_BLOCKING);
        }
        if (result != 0) {
            Tcl_SetErrno(result);
            return -1;
        }
        chanPtr->flags &= (~(CHANNEL_NONBLOCKING));
        if (chanPtr->flags & BG_FLUSH_SCHEDULED) {
            Tcl_DeleteFileHandler(chanPtr->outFile);
            chanPtr->flags &= (~(BG_FLUSH_SCHEDULED));
        }
    }
    
    /*
     * If the flush fails we cannot recover the original position. In
     * that case the seek is not attempted because we do not know where
     * the access position is - instead we return the error. FlushChannel
     * has already called Tcl_SetErrno() to report the error upwards.
     * If the flush succeeds we do the seek also.
     */
    
    if (FlushChannel(NULL, chanPtr, 0) != 0) {
        curPos = -1;
    } else {

        /*
         * Now seek to the new position in the channel as requested by the
         * caller.
         */

        curPos = (chanPtr->typePtr->seekProc) (chanPtr->instanceData,
                chanPtr->inFile, chanPtr->outFile, (long) offset,
                mode, &result);
        if (curPos == -1) {
            Tcl_SetErrno(result);
        }
    }
    
    /*
     * Restore to nonblocking mode if that was the previous behavior.
     *
     * NOTE: Even if there was an async flush active we do not restore
     * it now because we already flushed all the queued output, above.
     */
    
    if (wasAsync) {
        chanPtr->flags |= CHANNEL_NONBLOCKING;
        result = 0;
        if (chanPtr->typePtr->blockModeProc != NULL) {
            result = (chanPtr->typePtr->blockModeProc) (chanPtr->instanceData,
                    chanPtr->inFile, chanPtr->outFile, TCL_MODE_NONBLOCKING);
        }
        if (result != 0) {
            Tcl_SetErrno(result);
            return -1;
        }
    }

    return curPos;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Tell --
 *
 *	Returns the position of the next character to be read/written on
 *	this channel.
 *
 * Results:
 *	A nonnegative integer on success, -1 on failure. If failed,
 *	use Tcl_GetErrno() to retrieve the POSIX error code for the
 *	error that occurred.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Tell(chan)
    Tcl_Channel chan;			/* The channel to return pos for. */
{
    Channel *chanPtr;		/* The actual channel to tell on. */
    ChannelBuffer *bufPtr;		/* Iterates over queued input
                                         * and output buffers. */
    int inputBuffered, outputBuffered;
    int result;				/* Of calling device driver. */
    int curPos;				/* Position on device. */

    chanPtr = (Channel *) chan;

    /*
     * Check for unreported error.
     */

    if (chanPtr->unreportedError != 0) {
        Tcl_SetErrno(chanPtr->unreportedError);
        chanPtr->unreportedError = 0;
        return -1;
    }

    /*
     * Disallow tell on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (chanPtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }

    /*
     * Disallow tell on channels that are open for neither
     * writing nor reading (e.g. socket server channels).
     */

    if (!(chanPtr->flags & (TCL_WRITABLE|TCL_READABLE))) {
        Tcl_SetErrno(EACCES);
        return -1;
    }

    /*
     * Disallow tell on channels whose type does not have a seek procedure
     * defined. This means that the channel does not support seeking.
     */

    if (chanPtr->typePtr->seekProc == (Tcl_DriverSeekProc *) NULL) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }

    /*
     * Compute how much input and output is buffered. If both input and
     * output is buffered, cannot compute the current position.
     */

    for (bufPtr = chanPtr->inQueueHead, inputBuffered = 0;
             bufPtr != (ChannelBuffer *) NULL;
             bufPtr = bufPtr->nextPtr) {
        inputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    for (bufPtr = chanPtr->outQueueHead, outputBuffered = 0;
             bufPtr != (ChannelBuffer *) NULL;
             bufPtr = bufPtr->nextPtr) {
        outputBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    if (chanPtr->curOutPtr != (ChannelBuffer *) NULL) {
        outputBuffered +=
            (chanPtr->curOutPtr->nextAdded - chanPtr->curOutPtr->nextRemoved);
    }
    if ((inputBuffered != 0) && (outputBuffered != 0)) {
        Tcl_SetErrno(EFAULT);
        return -1;
    }

    /*
     * Get the current position in the device and compute the position
     * where the next character will be read or written.
     */

    curPos = (chanPtr->typePtr->seekProc) (chanPtr->instanceData,
            chanPtr->inFile, chanPtr->outFile, (long) 0, SEEK_CUR, &result);
    if (curPos == -1) {
        Tcl_SetErrno(result);
        return -1;
    }
    if (inputBuffered != 0) {
        return (curPos - inputBuffered);
    }
    return (curPos + outputBuffered);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Eof --
 *
 *	Returns 1 if the channel is at EOF, 0 otherwise.
 *
 * Results:
 *	1 or 0, always.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Eof(chan)
    Tcl_Channel chan;			/* Does this channel have EOF? */
{
    Channel *chanPtr;		/* The real channel structure. */

    chanPtr = (Channel *) chan;
    return ((chanPtr->flags & CHANNEL_STICKY_EOF) ||
            ((chanPtr->flags & CHANNEL_EOF) && (Tcl_InputBuffered(chan) == 0)))
        ? 1 : 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InputBlocked --
 *
 *	Returns 1 if input is blocked on this channel, 0 otherwise.
 *
 * Results:
 *	0 or 1, always.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_InputBlocked(chan)
    Tcl_Channel chan;			/* Is this channel blocked? */
{
    Channel *chanPtr;		/* The real channel structure. */

    chanPtr = (Channel *) chan;
    return (chanPtr->flags & CHANNEL_BLOCKED) ? 1 : 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InputBuffered --
 *
 *	Returns the number of bytes of input currently buffered in the
 *	internal buffer of a channel.
 *
 * Results:
 *	The number of input bytes buffered, or zero if the channel is not
 *	open for reading.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_InputBuffered(chan)
    Tcl_Channel chan;			/* The channel to query. */
{
    Channel *chanPtr;
    int bytesBuffered;
    ChannelBuffer *bufPtr;

    chanPtr = (Channel *) chan;
    for (bytesBuffered = 0, bufPtr = chanPtr->inQueueHead;
             bufPtr != (ChannelBuffer *) NULL;
             bufPtr = bufPtr->nextPtr) {
        bytesBuffered += (bufPtr->nextAdded - bufPtr->nextRemoved);
    }
    return bytesBuffered;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetChannelBufferSize --
 *
 *	Sets the size of buffers to allocate to store input or output
 *	in the channel. The size must be between 10 bytes and 1 MByte.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the size of buffers subsequently allocated for this channel.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetChannelBufferSize(chan, sz)
    Tcl_Channel chan;			/* The channel whose buffer size
                                         * to set. */
    int sz;				/* The size to set. */
{
    Channel *chanPtr;
    
    if (sz < 10) {
        sz = CHANNELBUFFER_DEFAULT_SIZE;
    }

    /*
     * Allow only buffers that are smaller than one megabyte.
     */
    
    if (sz > (1024 * 1024)) {
        sz = CHANNELBUFFER_DEFAULT_SIZE;
    }

    chanPtr = (Channel *) chan;
    chanPtr->bufSize = sz;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelBufferSize --
 *
 *	Retrieves the size of buffers to allocate for this channel.
 *
 * Results:
 *	The size.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelBufferSize(chan)
    Tcl_Channel chan;		/* The channel for which to find the
                                 * buffer size. */
{
    Channel *chanPtr;

    chanPtr = (Channel *) chan;
    return chanPtr->bufSize;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChannelOption --
 *
 *	Gets a mode associated with an IO channel. If the optionName arg
 *	is non NULL, retrieves the value of that option. If the optionName
 *	arg is NULL, retrieves a list of alternating option names and
 *	values for the given channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the supplied DString to the
 *	string value of the option(s) returned.
 *
 * Side effects:
 *	The string returned by this function is in static storage and
 *	may be reused at any time subsequent to the call.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetChannelOption(chan, optionName, dsPtr)
    Tcl_Channel chan;		/* Channel on which to get option. */
    char *optionName;		/* Option to get. */
    Tcl_DString *dsPtr;		/* Where to store value(s). */
{
    Channel *chanPtr;		/* The real IO channel. */
    size_t len;			/* Length of optionName string. */
    char optionVal[128];	/* Buffer for sprintf. */

    chanPtr = (Channel *) chan;

    /*
     * Disallow options on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (chanPtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
        return TCL_ERROR;
    }

    /*
     * If the optionName is NULL it means that we want a list of all
     * options and values.
     */
    
    if (optionName == (char *) NULL) {
        len = 0;
    } else {
        len = strlen(optionName);
    }
    
    if ((len == 0) || ((len > 2) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-blocking", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-blocking");
        }
        Tcl_DStringAppendElement(dsPtr,
                (chanPtr->flags & CHANNEL_NONBLOCKING) ? "0" : "1");
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) || ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffering", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-buffering");
        }
        if (chanPtr->flags & CHANNEL_LINEBUFFERED) {
            Tcl_DStringAppendElement(dsPtr, "line");
        } else if (chanPtr->flags & CHANNEL_UNBUFFERED) {
            Tcl_DStringAppendElement(dsPtr, "none");
        } else {
            Tcl_DStringAppendElement(dsPtr, "full");
        }
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) || ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffersize", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-buffersize");
        }
        sprintf(optionVal, "%d", chanPtr->bufSize);
        Tcl_DStringAppendElement(dsPtr, optionVal);
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'e') &&
                    (strncmp(optionName, "-eofchar", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-eofchar");
        }
        if (((chanPtr->flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringStartSublist(dsPtr);
        }
        if (chanPtr->flags & TCL_READABLE) {
            if (chanPtr->inEofChar == 0) {
                Tcl_DStringAppendElement(dsPtr, "");
            } else {
                char buf[4];

                sprintf(buf, "%c", chanPtr->inEofChar);
                Tcl_DStringAppendElement(dsPtr, buf);
            }
        }
        if (chanPtr->flags & TCL_WRITABLE) {
            if (chanPtr->outEofChar == 0) {
                Tcl_DStringAppendElement(dsPtr, "");
            } else {
                char buf[4];

                sprintf(buf, "%c", chanPtr->outEofChar);
                Tcl_DStringAppendElement(dsPtr, buf);
            }
        }
        if (((chanPtr->flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringEndSublist(dsPtr);
        }
        if (len > 0) {
            return TCL_OK;
        }
    }
    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 't') &&
                    (strncmp(optionName, "-translation", len) == 0))) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-translation");
        }
        if (((chanPtr->flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringStartSublist(dsPtr);
        }
        if (chanPtr->flags & TCL_READABLE) {
            if (chanPtr->inputTranslation == TCL_TRANSLATE_AUTO) {
                Tcl_DStringAppendElement(dsPtr, "auto");
            } else if (chanPtr->inputTranslation == TCL_TRANSLATE_CR) {
                Tcl_DStringAppendElement(dsPtr, "cr");
            } else if (chanPtr->inputTranslation == TCL_TRANSLATE_CRLF) {
                Tcl_DStringAppendElement(dsPtr, "crlf");
            } else {
                Tcl_DStringAppendElement(dsPtr, "lf");
            }
        }
        if (chanPtr->flags & TCL_WRITABLE) {
            if (chanPtr->outputTranslation == TCL_TRANSLATE_AUTO) {
                Tcl_DStringAppendElement(dsPtr, "auto");
            } else if (chanPtr->outputTranslation == TCL_TRANSLATE_CR) {
                Tcl_DStringAppendElement(dsPtr, "cr");
            } else if (chanPtr->outputTranslation == TCL_TRANSLATE_CRLF) {
                Tcl_DStringAppendElement(dsPtr, "crlf");
            } else {
                Tcl_DStringAppendElement(dsPtr, "lf");
            }
        }
        if (((chanPtr->flags & (TCL_READABLE|TCL_WRITABLE)) ==
                (TCL_READABLE|TCL_WRITABLE)) && (len == 0)) {
            Tcl_DStringEndSublist(dsPtr);
        }
        if (len > 0) {
            return TCL_OK;
        }
    }
    if (chanPtr->typePtr->getOptionProc != (Tcl_DriverGetOptionProc *) NULL) {
        return (chanPtr->typePtr->getOptionProc) (chanPtr->instanceData,
                optionName, dsPtr);
    }
    if (len == 0) {
        return TCL_OK;
    }
    Tcl_SetErrno(EINVAL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetChannelOption --
 *
 *	Sets an option on a channel.
 *
 * Results:
 *	A standard Tcl result. Also sets interp->result on error if
 *	interp is not NULL.
 *
 * Side effects:
 *	May modify an option on a device.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetChannelOption(interp, chan, optionName, newValue)
    Tcl_Interp *interp;		/* For error reporting - can be NULL. */
    Tcl_Channel chan;		/* Channel on which to set mode. */
    char *optionName;		/* Which option to set? */
    char *newValue;		/* New value for option. */
{
    int result;			/* Result of channel type operation. */
    int newMode;		/* New (numeric) mode to sert. */
    Channel *chanPtr;	/* The real IO channel. */
    size_t len;			/* Length of optionName string. */
    int argc;
    char **argv;

    chanPtr = (Channel *) chan;

    /*
     * Disallow options on dead channels -- channels that have been closed but
     * not yet been deallocated. Such channels can be found if the exit
     * handler for channel cleanup has run but the channel is still
     * registered in an interpreter.
     */

    if (chanPtr->flags & CHANNEL_DEAD) {
        Tcl_SetErrno(EINVAL);
        return -1;
    }
    
    len = strlen(optionName);

    if ((len > 2) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-blocking", len) == 0)) {
        if (Tcl_GetBoolean(interp, newValue, &newMode) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (newMode) {
            newMode = TCL_MODE_BLOCKING;
        } else {
            newMode = TCL_MODE_NONBLOCKING;
        }
        result = 0;
        if (chanPtr->typePtr->blockModeProc != NULL) {
            result = (chanPtr->typePtr->blockModeProc) (chanPtr->instanceData,
                    chanPtr->inFile, chanPtr->outFile, newMode);
        }
        if (result != 0) {
            Tcl_SetErrno(result);
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "error setting blocking mode: ",
                        Tcl_PosixError(interp), (char *) NULL);
            }
            return TCL_ERROR;
        }
        if (newMode == TCL_MODE_BLOCKING) {
            chanPtr->flags &= (~(CHANNEL_NONBLOCKING));
            if (chanPtr->outFile != (Tcl_File) NULL) {
                Tcl_DeleteFileHandler(chanPtr->outFile);
                chanPtr->flags &= (~(BG_FLUSH_SCHEDULED));
            }
        } else {
            chanPtr->flags |= CHANNEL_NONBLOCKING;
        }
        return TCL_OK;
    }

    if ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffering", len) == 0)) {
        len = strlen(newValue);
        if ((newValue[0] == 'f') && (strncmp(newValue, "full", len) == 0)) {
            chanPtr->flags &=
                (~(CHANNEL_UNBUFFERED|CHANNEL_LINEBUFFERED));
        } else if ((newValue[0] == 'l') &&
                (strncmp(newValue, "line", len) == 0)) {
            chanPtr->flags &= (~(CHANNEL_UNBUFFERED));
            chanPtr->flags |= CHANNEL_LINEBUFFERED;
        } else if ((newValue[0] == 'n') &&
                (strncmp(newValue, "none", len) == 0)) {
            chanPtr->flags &= (~(CHANNEL_LINEBUFFERED));
            chanPtr->flags |= CHANNEL_UNBUFFERED;
        } else {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "bad value for -buffering: ",
                        "must be one of full, line, or none",
                        (char *) NULL);
                return TCL_ERROR;
            }
        }
        return TCL_OK;
    }

    if ((len > 7) && (optionName[1] == 'b') &&
            (strncmp(optionName, "-buffersize", len) == 0)) {
        chanPtr->bufSize = atoi(newValue);
        if ((chanPtr->bufSize < 10) || (chanPtr->bufSize > (1024 * 1024))) {
            chanPtr->bufSize = CHANNELBUFFER_DEFAULT_SIZE;
        }
        return TCL_OK;
    }
    
    if ((len > 1) && (optionName[1] == 'e') &&
            (strncmp(optionName, "-eofchar", len) == 0)) {
        if (Tcl_SplitList(interp, newValue, &argc, &argv) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (argc == 0) {
            chanPtr->inEofChar = 0;
            chanPtr->outEofChar = 0;
        } else if (argc == 1) {
            if (chanPtr->flags & TCL_WRITABLE) {
                chanPtr->outEofChar = (int) argv[0][0];
            }
            if (chanPtr->flags & TCL_READABLE) {
                chanPtr->inEofChar = (int) argv[0][0];
            }
        } else if (argc != 2) {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp,
                        "bad value for -eofchar: should be a list of one or",
                        " two elements", (char *) NULL);
            }
            ckfree((char *) argv);
            return TCL_ERROR;
        } else {
            if (chanPtr->flags & TCL_READABLE) {
                chanPtr->inEofChar = (int) argv[0][0];
            }
            if (chanPtr->flags & TCL_WRITABLE) {
                chanPtr->outEofChar = (int) argv[1][0];
            }
        }
        if (argv != (char **) NULL) {
            ckfree((char *) argv);
        }
        return TCL_OK;
    }

    if ((len > 1) && (optionName[1] == 't') &&
            (strncmp(optionName, "-translation", len) == 0)) {
        if (Tcl_SplitList(interp, newValue, &argc, &argv) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (argc == 1) {
            if (chanPtr->flags & TCL_READABLE) {
                chanPtr->flags &= (~(INPUT_SAW_CR));
                if (strcmp(argv[0], "auto") == 0) {
                    chanPtr->inputTranslation = TCL_TRANSLATE_AUTO;
                } else if (strcmp(argv[0], "binary") == 0) {
                    chanPtr->inEofChar = 0;
                    chanPtr->inputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[0], "lf") == 0) {
                    chanPtr->inputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[0], "cr") == 0) {
                    chanPtr->inputTranslation = TCL_TRANSLATE_CR;
                } else if (strcmp(argv[0], "crlf") == 0) {
                    chanPtr->inputTranslation = TCL_TRANSLATE_CRLF;
                } else if (strcmp(argv[0], "platform") == 0) {
                    chanPtr->inputTranslation = TCL_PLATFORM_TRANSLATION;
                } else {
                    if (interp != (Tcl_Interp *) NULL) {
                        Tcl_AppendResult(interp,
                                "bad value for -translation: ",
                                "must be one of auto, binary, cr, lf, crlf,",
                                " or platform", (char *) NULL);
                    }
                    ckfree((char *) argv);
                    return TCL_ERROR;
                }
            }
            if (chanPtr->flags & TCL_WRITABLE) {
                if (strcmp(argv[0], "auto") == 0) {
                    /*
                     * This is a hack to get TCP sockets to produce output
                     * in CRLF mode if they are being set into AUTO mode.
                     * A better solution for achieving this effect will be
                     * coded later.
                     */

                    if (strcmp(chanPtr->typePtr->typeName, "tcp") == 0) {
                        chanPtr->outputTranslation = TCL_TRANSLATE_CRLF;
                    } else {
                        chanPtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
                    }
                } else if (strcmp(argv[0], "binary") == 0) {
                    chanPtr->outEofChar = 0;
                    chanPtr->outputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[0], "lf") == 0) {
                    chanPtr->outputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[0], "cr") == 0) {
                    chanPtr->outputTranslation = TCL_TRANSLATE_CR;
                } else if (strcmp(argv[0], "crlf") == 0) {
                    chanPtr->outputTranslation = TCL_TRANSLATE_CRLF;
                } else if (strcmp(argv[0], "platform") == 0) {
                    chanPtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
                } else {
                    if (interp != (Tcl_Interp *) NULL) {
                        Tcl_AppendResult(interp,
                                "bad value for -translation: ",
                                "must be one of auto, binary, cr, lf, crlf,",
                                " or platform", (char *) NULL);
                    }
                    ckfree((char *) argv);
                    return TCL_ERROR;
                }
            }
        } else if (argc != 2) {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp,
                        "bad value for -translation: must be a one or two",
                        " element list", (char *) NULL);
            }
            ckfree((char *) argv);
            return TCL_ERROR;
        } else {
            if (chanPtr->flags & TCL_READABLE) {
                if (argv[0][0] == '\0') {
                    /* Empty body. */
                } else if (strcmp(argv[0], "auto") == 0) {
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    chanPtr->inputTranslation = TCL_TRANSLATE_AUTO;
                } else if (strcmp(argv[0], "binary") == 0) {
                    chanPtr->inEofChar = 0;
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    chanPtr->inputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[0], "lf") == 0) {
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    chanPtr->inputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[0], "cr") == 0) {
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    chanPtr->inputTranslation = TCL_TRANSLATE_CR;
                } else if (strcmp(argv[0], "crlf") == 0) {
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    chanPtr->inputTranslation = TCL_TRANSLATE_CRLF;
                } else if (strcmp(argv[0], "platform") == 0) {
                    chanPtr->flags &= (~(INPUT_SAW_CR));
                    chanPtr->inputTranslation = TCL_PLATFORM_TRANSLATION;
                } else {
                    if (interp != (Tcl_Interp *) NULL) {
                        Tcl_AppendResult(interp,
                                "bad value for -translation: ",
                                "must be one of auto, binary, cr, lf, crlf,",
                                " or platform", (char *) NULL);
                    }
                    ckfree((char *) argv);
                    return TCL_ERROR;
                }
            }
            if (chanPtr->flags & TCL_WRITABLE) {
                if (argv[1][0] == '\0') {
                    /* Empty body. */
                } else if (strcmp(argv[1], "auto") == 0) {
                    /*
                     * This is a hack to get TCP sockets to produce output
                     * in CRLF mode if they are being set into AUTO mode.
                     * A better solution for achieving this effect will be
                     * coded later.
                     */

                    if (strcmp(chanPtr->typePtr->typeName, "tcp") == 0) {
                        chanPtr->outputTranslation = TCL_TRANSLATE_CRLF;
                    } else {
                        chanPtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
                    }
                } else if (strcmp(argv[1], "binary") == 0) {
                    chanPtr->outEofChar = 0;
                    chanPtr->outputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[1], "lf") == 0) {
                    chanPtr->outputTranslation = TCL_TRANSLATE_LF;
                } else if (strcmp(argv[1], "cr") == 0) {
                    chanPtr->outputTranslation = TCL_TRANSLATE_CR;
                } else if (strcmp(argv[1], "crlf") == 0) {
                    chanPtr->outputTranslation = TCL_TRANSLATE_CRLF;
                } else if (strcmp(argv[1], "platform") == 0) {
                    chanPtr->outputTranslation = TCL_PLATFORM_TRANSLATION;
                } else {
                    if (interp != (Tcl_Interp *) NULL) {
                        Tcl_AppendResult(interp,
                                "bad value for -translation: ",
                                "must be one of auto, binary, cr, lf, crlf,",
                                " or platform", (char *) NULL);
                    }
                    ckfree((char *) argv);
                    return TCL_ERROR;
                }
            }
        }
        ckfree((char *) argv);            
        return TCL_OK;
    }
        
    if (chanPtr->typePtr->setOptionProc != (Tcl_DriverSetOptionProc *) NULL) {
        return (chanPtr->typePtr->setOptionProc) (chanPtr->instanceData,
                interp, optionName, newValue);
    }
    
    if (interp != (Tcl_Interp *) NULL) {
        Tcl_AppendResult(interp, "bad option \"", optionName,
                "\": should be -blocking, -buffering, -buffersize, ",
                "-eofchar, -translation, ",
                "or channel type specific option",
                (char *) NULL);
    }

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupChannelHandlers --
 *
 *	Removes channel handlers that refer to the supplied interpreter,
 *	so that if the actual channel is not closed now, these handlers
 *	will not run on subsequent events on the channel. This would be
 *	erroneous, because the interpreter no longer has a reference to
 *	this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes channel handlers.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupChannelHandlers(interp, chanPtr)
    Tcl_Interp *interp;
    Channel *chanPtr;
{
    EventScriptRecord *sPtr, *prevPtr, *nextPtr;

    /*
     * Remove fileevent records on this channel that refer to the
     * given interpreter.
     */
    
    for (sPtr = chanPtr->scriptRecordPtr,
             prevPtr = (EventScriptRecord *) NULL;
             sPtr != (EventScriptRecord *) NULL;
             sPtr = nextPtr) {
        nextPtr = sPtr->nextPtr;
        if (sPtr->interp == interp) {
            if (prevPtr == (EventScriptRecord *) NULL) {
                chanPtr->scriptRecordPtr = nextPtr;
            } else {
                prevPtr->nextPtr = nextPtr;
            }

            Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                    ChannelEventScriptInvoker, (ClientData) sPtr);

            Tcl_EventuallyFree((ClientData) sPtr->script, TCL_DYNAMIC);
            ckfree((char *) sPtr);
        } else {
            prevPtr = sPtr;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelEventSourceExitProc --
 *
 *	This procedure is called during exit cleanup to delete the channel
 *	event source. It deletes the event source for channels.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the channel event source.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
ChannelEventSourceExitProc(clientData)
    ClientData clientData;		/* Not used. */
{
    Tcl_DeleteEventSource(ChannelHandlerSetupProc, ChannelHandlerCheckProc,
            (ClientData) NULL);
    channelEventSourceCreated = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelHandlerSetupProc --
 *
 *	This procedure is part of the event source for channel handlers.
 *	It is invoked by Tcl_DoOneEvent before it waits for events. The
 *	job of this procedure is to provide information to Tcl_DoOneEvent
 *	on how to wait for events (what files to watch).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tells the notifier what channels to watch.
 *
 *----------------------------------------------------------------------
 */

static void
ChannelHandlerSetupProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include
					 * TCL_FILE_EVENTS then we do
					 * nothing. */
{
    Tcl_Time dontBlock;
    Channel *chanPtr, *nextChanPtr;

    if (!(flags & TCL_FILE_EVENTS)) {
        return;
    }

    dontBlock.sec = 0; dontBlock.usec = 0;
    
    for (chanPtr = firstChanPtr; chanPtr != (Channel *) NULL;
             chanPtr = nextChanPtr) {
        nextChanPtr = chanPtr->nextChanPtr;
        if (chanPtr->interestMask & TCL_READABLE) {
            if ((!(chanPtr->flags & CHANNEL_BLOCKED)) &&
                    (chanPtr->inQueueHead != (ChannelBuffer *) NULL) &&
                    (chanPtr->inQueueHead->nextRemoved <
                            chanPtr->inQueueHead->nextAdded)) {
                Tcl_SetMaxBlockTime(&dontBlock);
            } else if (chanPtr->inFile != (Tcl_File) NULL) {
                Tcl_WatchFile(chanPtr->inFile, TCL_READABLE);
            }
        }
        if (chanPtr->interestMask & TCL_WRITABLE) {
            if (chanPtr->outFile != (Tcl_File) NULL) {
                Tcl_WatchFile(chanPtr->outFile, TCL_WRITABLE);
            }
        }
        if (chanPtr->interestMask & TCL_EXCEPTION) {
            if (chanPtr->inFile != (Tcl_File) NULL) {
                Tcl_WatchFile(chanPtr->inFile, TCL_EXCEPTION);
            }
            if (chanPtr->outFile != (Tcl_File) NULL) {
                Tcl_WatchFile(chanPtr->outFile, TCL_EXCEPTION);
            }
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelHandlerCheckProc --
 *
 *	This procedure is the second part (of three) of the event source
 *	for channels. It is invoked by Tcl_DoOneEvent after the wait for
 *	events is over. The job of this procedure is to test each channel
 *	to see if it is ready now, and if so, to create events and put them
 *	on the Tcl event queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Makes entries on the Tcl event queue for each channel that is
 *	ready now.
 *
 *----------------------------------------------------------------------
 */

static void
ChannelHandlerCheckProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include 
					 * TCL_FILE_EVENTS then we do
					 * nothing. */
{
    Channel *chanPtr, *nextChanPtr;
    ChannelHandlerEvent *ePtr;
    int readyMask;
    
    if (!(flags & TCL_FILE_EVENTS)) {
        return;
    }

    for (chanPtr = firstChanPtr;
             chanPtr != (Channel *) NULL;
             chanPtr = nextChanPtr) {
        nextChanPtr = chanPtr->nextChanPtr;

        readyMask = 0;

        /*
         * Check for readability.
         */
        
        if (chanPtr->interestMask & TCL_READABLE) {

            /*
             * The channel is considered ready for reading if there is input
             * buffered AND the last attempt to read from the channel did not
             * return EWOULDBLOCK, OR if the underlying file is ready.
             *
             * NOTE that the input queue may contain empty buffers, hence the
             * special check to see if the first input buffer is empty. The
             * invariant is that if there is an empty buffer in the queue
             * there is only one buffer in the queue, hence an empty first
             * buffer indicates that there is no input queued.
             */
            
            if ((!(chanPtr->flags & CHANNEL_BLOCKED)) &&
                    ((chanPtr->inQueueHead != (ChannelBuffer *) NULL) &&
                            (chanPtr->inQueueHead->nextRemoved <
                                    chanPtr->inQueueHead->nextAdded))) {
                readyMask |= TCL_READABLE;
            } else if (chanPtr->inFile != (Tcl_File) NULL) {
                readyMask |=
                    Tcl_FileReady(chanPtr->inFile, TCL_READABLE);
            }
        }

        /*
         * Check for writability.
         */

        if (chanPtr->interestMask & TCL_WRITABLE) {

            /*
             * The channel is considered ready for writing if there is no
             * output buffered waiting to be written to the device, AND the
             * underlying file is ready.
             */
            
            if ((chanPtr->outQueueHead == (ChannelBuffer *) NULL) &&
                    (chanPtr->outFile != (Tcl_File) NULL)) {
                readyMask |=
                    Tcl_FileReady(chanPtr->outFile, TCL_WRITABLE);
            }
        }

        /*
         * Check for exceptions.
         */

        if (chanPtr->interestMask & TCL_EXCEPTION) {
            if (chanPtr->inFile != (Tcl_File) NULL) {
                readyMask |=
                    Tcl_FileReady(chanPtr->inFile, TCL_EXCEPTION);
            }
            if (chanPtr->outFile != (Tcl_File) NULL) {
                readyMask |=
                    Tcl_FileReady(chanPtr->outFile, TCL_EXCEPTION);
            }
        }
        
        /*
         * If there are any events for this channel, put a notice into the
         * Tcl event queue.
         */
        
        if (readyMask != 0) {
            ePtr = (ChannelHandlerEvent *) ckalloc((unsigned)
                    sizeof(ChannelHandlerEvent));
            ePtr->header.proc = ChannelHandlerEventProc;
            ePtr->chanPtr = chanPtr;
            ePtr->readyMask = readyMask;
            Tcl_QueueEvent((Tcl_Event *) ePtr, TCL_QUEUE_TAIL);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FlushEventProc --
 *
 *	This routine dispatches a background flush event.
 *
 *	Errors that occur during the write operation are stored
 *	inside the channel structure for future reporting by the next
 *	operation that uses this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Causes production of output on a channel.
 *
 *----------------------------------------------------------------------
 */

static void
FlushEventProc(clientData, mask)
    ClientData clientData;		/* Channel to produce output on. */
    int mask;				/* Not used. */
{
    (void) FlushChannel(NULL, (Channel *) clientData, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelHandlerEventProc --
 *
 *	This procedure is called by Tcl_DoOneEvent when a channel event
 *	reaches the front of the event queue. This procedure is responsible
 *	for actually handling the event by invoking the callback for the
 *	channel handler.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning that it should be
 *	removed from the queue. Returns 0 if the event was not handled
 *	meaning that it should stay in the queue. The only time the event
 *	will not be handled is if the TCL_FILE_EVENTS flag bit is not
 *	set in the flags passed.
 *
 *	NOTE: If the handler is deleted between the time the event is added
 *	to the queue and the time it reaches the head of the queue, the
 *	event is silently discarded (i.e. we return 1).
 *
 * Side effects:
 *	Whatever the channel handler callback procedure does.
 *
 *----------------------------------------------------------------------
 */

static int
ChannelHandlerEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
                                 * handle, such as TCL_FILE_EVENTS. */
{
    Channel *chanPtr;
    ChannelHandler *chPtr;
    ChannelHandlerEvent *ePtr;
    NextChannelHandler nh;

    if (!(flags & TCL_FILE_EVENTS)) {
        return 0;
    }

    ePtr = (ChannelHandlerEvent *) evPtr;
    chanPtr = ePtr->chanPtr;

    /*
     * Add this invocation to the list of recursive invocations of
     * ChannelHandlerEventProc.
     */
    
    nh.nextHandlerPtr = (ChannelHandler *) NULL;
    nh.nestedHandlerPtr = nestedHandlerPtr;
    nestedHandlerPtr = &nh;
    
    for (chPtr = chanPtr->chPtr; chPtr != (ChannelHandler *) NULL; ) {

        /*
         * If this channel handler is interested in any of the events that
         * have occurred on the channel, invoke its procedure.
         */
        
        if ((chPtr->mask & ePtr->readyMask) != 0) {
            nh.nextHandlerPtr = chPtr->nextPtr;
	    (*(chPtr->proc))(chPtr->clientData, ePtr->readyMask);
            chPtr = nh.nextHandlerPtr;
        } else {
            chPtr = chPtr->nextPtr;
	}
    }

    nestedHandlerPtr = nh.nestedHandlerPtr;
    
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateChannelHandler --
 *
 *	Arrange for a given procedure to be invoked whenever the
 *	channel indicated by the chanPtr arg becomes readable or
 *	writable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever the I/O channel given by chanPtr becomes
 *	ready in the way indicated by mask, proc will be invoked.
 *	See the manual entry for details on the calling sequence
 *	to proc.  If there is already an event handler for chan, proc
 *	and clientData, then the mask will be updated.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateChannelHandler(chan, mask, proc, clientData)
    Tcl_Channel chan;		/* The channel to create the handler for. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions under which
				 * proc should be called. Use 0 to
                                 * disable a registered handler. */
    Tcl_ChannelProc *proc;	/* Procedure to call for each
				 * selected event. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    ChannelHandler *chPtr;
    Channel *chanPtr;

    chanPtr = (Channel *) chan;
    
    /*
     * Ensure that the channel event source is registered with the Tcl
     * notification mechanism.
     */
    
    if (!channelEventSourceCreated) {
        channelEventSourceCreated = 1;
        Tcl_CreateEventSource(ChannelHandlerSetupProc, ChannelHandlerCheckProc,
                (ClientData) NULL);
        Tcl_CreateExitHandler(ChannelEventSourceExitProc, (ClientData) NULL);
    }

    /*
     * Check whether this channel handler is not already registered. If
     * it is not, create a new record, else reuse existing record (smash
     * current values).
     */

    for (chPtr = chanPtr->chPtr;
             chPtr != (ChannelHandler *) NULL;
             chPtr = chPtr->nextPtr) {
        if ((chPtr->chanPtr == chanPtr) && (chPtr->proc == proc) &&
                (chPtr->clientData == clientData)) {
            break;
        }
    }
    if (chPtr == (ChannelHandler *) NULL) {
        chPtr = (ChannelHandler *) ckalloc((unsigned) sizeof(ChannelHandler));
        chPtr->mask = 0;
        chPtr->proc = proc;
        chPtr->clientData = clientData;
        chPtr->chanPtr = chanPtr;
        chPtr->nextPtr = chanPtr->chPtr;
        chanPtr->chPtr = chPtr;
    }

    /*
     * The remainder of the initialization below is done regardless of
     * whether or not this is a new record or a modification of an old
     * one.
     */

    chPtr->mask = mask;

    /*
     * Recompute the interest mask for the channel - this call may actually
     * be disabling an existing handler..
     */
    
    chanPtr->interestMask = 0;
    for (chPtr = chanPtr->chPtr;
	     chPtr != (ChannelHandler *) NULL;
	     chPtr = chPtr->nextPtr) {
	chanPtr->interestMask |= chPtr->mask;
    }                                       
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteChannelHandler --
 *
 *	Cancel a previously arranged callback arrangement for an IO
 *	channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a callback was previously registered for this chan, proc and
 *	 clientData , it is removed and the callback will no longer be called
 *	when the channel becomes ready for IO.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteChannelHandler(chan, proc, clientData)
    Tcl_Channel chan;		/* The channel for which to remove the
                                 * callback. */
    Tcl_ChannelProc *proc;	/* The procedure in the callback to delete. */
    ClientData clientData;	/* The client data in the callback
                                 * to delete. */
    
{
    ChannelHandler *chPtr, *prevChPtr;
    Channel *chanPtr;
    NextChannelHandler *nhPtr;

    chanPtr = (Channel *) chan;

    /*
     * Find the entry and the previous one in the list.
     */

    for (prevChPtr = (ChannelHandler *) NULL, chPtr = chanPtr->chPtr;
             chPtr != (ChannelHandler *) NULL;
             chPtr = chPtr->nextPtr) {
        if ((chPtr->chanPtr == chanPtr) && (chPtr->clientData == clientData)
                && (chPtr->proc == proc)) {
            break;
        }
        prevChPtr = chPtr;
    }

    /*
     * If ChannelHandlerEventProc is about to process this handler, tell it to
     * process the next one instead - we are going to delete *this* one.
     */

    for (nhPtr = nestedHandlerPtr;
             nhPtr != (NextChannelHandler *) NULL;
             nhPtr = nhPtr->nestedHandlerPtr) {
        if (nhPtr->nextHandlerPtr == chPtr) {
            nhPtr->nextHandlerPtr = chPtr->nextPtr;
        }
    }
    
    /*
     * If found, splice the entry out of the list.
     */

    if (chPtr == (ChannelHandler *) NULL) {
        return;
    }

    if (prevChPtr == (ChannelHandler *) NULL) {
        chanPtr->chPtr = chPtr->nextPtr;
    } else {
        prevChPtr->nextPtr = chPtr->nextPtr;
    }
    ckfree((char *) chPtr);

    /*
     * Recompute the interest list for the channel, so that infinite loops
     * will not result if Tcl_DeleteChanelHandler is called inside an event.
     */

    chanPtr->interestMask = 0;
    for (chPtr = chanPtr->chPtr;
             chPtr != (ChannelHandler *) NULL;
             chPtr = chPtr->nextPtr) {
        chanPtr->interestMask |= chPtr->mask;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ReturnScriptRecord --
 *
 *	Get a script stored for this channel with this interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets interp->result to the script.
 *
 *----------------------------------------------------------------------
 */

static void
ReturnScriptRecord(interp, chanPtr, mask)
    Tcl_Interp *interp;		/* The interpreter in which the script
                                 * is to be executed. */
    Channel *chanPtr;		/* The channel for which the script is
                                 * stored. */
    int mask;			/* Events in mask must overlap with events
                                 * for which this script is stored. */
{
    EventScriptRecord *esPtr;
    
    for (esPtr = chanPtr->scriptRecordPtr;
             esPtr != (EventScriptRecord *) NULL;
             esPtr = esPtr->nextPtr) {
        if ((esPtr->interp == interp) && (esPtr->mask == mask)) {
            interp->result = esPtr->script;
            return;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteScriptRecord --
 *
 *	Delete a script record for this combination of channel, interp
 *	and mask.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes a script record and cancels a channel event handler.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteScriptRecord(interp, chanPtr, mask)
    Tcl_Interp *interp;		/* Interpreter in which script was to be
                                 * executed. */
    Channel *chanPtr;		/* The channel for which to delete the
                                 * script record (if any). */
    int mask;			/* Events in mask must exactly match mask
                                 * of script to delete. */
{
    EventScriptRecord *esPtr, *prevEsPtr;

    for (esPtr = chanPtr->scriptRecordPtr,
             prevEsPtr = (EventScriptRecord *) NULL;
             esPtr != (EventScriptRecord *) NULL;
             prevEsPtr = esPtr, esPtr = esPtr->nextPtr) {
        if ((esPtr->interp == interp) && (esPtr->mask == mask)) {
            if (esPtr == chanPtr->scriptRecordPtr) {
                chanPtr->scriptRecordPtr = esPtr->nextPtr;
            } else {
                prevEsPtr->nextPtr = esPtr->nextPtr;
            }

            Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                    ChannelEventScriptInvoker, (ClientData) esPtr);
            
            Tcl_EventuallyFree((ClientData)esPtr->script, TCL_DYNAMIC);
            ckfree((char *) esPtr);

            break;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CreateScriptRecord --
 *
 *	Creates a record to store a script to be executed when a specific
 *	event fires on a specific channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Causes the script to be stored for later execution.
 *
 *----------------------------------------------------------------------
 */

static void
CreateScriptRecord(interp, chanPtr, mask, script)
    Tcl_Interp *interp;			/* Interpreter in which to execute
                                         * the stored script. */
    Channel *chanPtr;			/* Channel for which script is to
                                         * be stored. */
    int mask;				/* Set of events for which script
                                         * will be invoked. */
    char *script;			/* A copy of this script is stored
                                         * in the newly created record. */
{
    EventScriptRecord *esPtr;

    for (esPtr = chanPtr->scriptRecordPtr;
             esPtr != (EventScriptRecord *) NULL;
             esPtr = esPtr->nextPtr) {
        if ((esPtr->interp == interp) && (esPtr->mask == mask)) {
            Tcl_EventuallyFree((ClientData)esPtr->script, TCL_DYNAMIC);
            esPtr->script = (char *) NULL;
            break;
        }
    }
    if (esPtr == (EventScriptRecord *) NULL) {
        esPtr = (EventScriptRecord *) ckalloc((unsigned)
                sizeof(EventScriptRecord));
        Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
                ChannelEventScriptInvoker, (ClientData) esPtr);
        esPtr->nextPtr = chanPtr->scriptRecordPtr;
        chanPtr->scriptRecordPtr = esPtr;
    }
    esPtr->chanPtr = chanPtr;
    esPtr->interp = interp;
    esPtr->mask = mask;
    esPtr->script = ckalloc((unsigned) (strlen(script) + 1));
    strcpy(esPtr->script, script);
}

/*
 *----------------------------------------------------------------------
 *
 * ChannelEventScriptInvoker --
 *
 *	Invokes a script scheduled by "fileevent" for when the channel
 *	becomes ready for IO. This function is invoked by the channel
 *	handler which was created by the Tcl "fileevent" command.
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
ChannelEventScriptInvoker(clientData, mask)
    ClientData clientData;	/* The script+interp record. */
    int mask;			/* Not used. */
{
    Tcl_Interp *interp;		/* Interpreter in which to eval the script. */
    Channel *chanPtr;		/* The channel for which this handler is
                                 * registered. */
    char *script;		/* Script to eval. */
    EventScriptRecord *esPtr;	/* The event script + interpreter to eval it
                                 * in. */
    int result;			/* Result of call to eval script. */

    esPtr = (EventScriptRecord *) clientData;

    chanPtr = esPtr->chanPtr;
    mask = esPtr->mask;
    interp = esPtr->interp;
    script = esPtr->script;

    /*
     * We must preserve the channel, script and interpreter because each of
     * these may be deleted in the evaluation. If an error later occurs, we
     * want to have the relevant data around for error reporting and so we
     * can safely delete it.
     */
    
    Tcl_Preserve((ClientData) chanPtr);
    Tcl_Preserve((ClientData) script);
    Tcl_Preserve((ClientData) interp);
    result = Tcl_GlobalEval(esPtr->interp, script);

    /*
     * On error, cause a background error and remove the channel handler
     * and the script record.
     *
     * NOTE: Must delete channel handler before causing the background error
     * because the background error may want to reinstall the handler.
     */
    
    if (result != TCL_OK) {
        DeleteScriptRecord(interp, chanPtr, mask);
        Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) chanPtr);
    Tcl_Release((ClientData) script);
    Tcl_Release((ClientData) interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileEventCmd --
 *
 *	This procedure implements the "fileevent" Tcl command. See the
 *	user documentation for details on what it does. This command is
 *	based on the Tk command "fileevent" which in turn is based on work
 *	contributed by Mark Diekhans.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May create a channel handler for the specified channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_FileEventCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Interpreter in which the channel
                                         * for which to create the handler
                                         * is found. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Channel *chanPtr;			/* The channel to create
                                         * the handler for. */
    Tcl_Channel chan;			/* The opaque type for the channel. */
    int c;				/* First char of mode argument. */
    int mask;				/* Mask for events of interest. */
    size_t length;			/* Length of mode argument. */

    /*
     * Parse arguments.
     */

    if ((argc != 3) && (argc != 4)) {
	Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" channelId event ?script?", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'r') && (strncmp(argv[2], "readable", length) == 0)) {
        mask = TCL_READABLE;
    } else if ((c == 'w') && (strncmp(argv[2], "writable", length) == 0)) {
        mask = TCL_WRITABLE;
    } else {
	Tcl_AppendResult(interp, "bad event name \"", argv[2],
		"\": must be readable or writable", (char *) NULL);
	return TCL_ERROR;
    }
    chan = Tcl_GetChannel(interp, argv[1], NULL);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    
    chanPtr = (Channel *) chan;
    if ((chanPtr->flags & mask) == 0) {
        Tcl_AppendResult(interp, "channel is not ",
                (mask == TCL_READABLE) ? "readable" : "writable",
                (char *) NULL);
        return TCL_ERROR;
    }
    
    /*
     * If we are supposed to return the script, do so.
     */

    if (argc == 3) {
        ReturnScriptRecord(interp, chanPtr, mask);
        return TCL_OK;
    }

    /*
     * If we are supposed to delete a stored script, do so.
     */

    if (argv[3][0] == 0) {
        DeleteScriptRecord(interp, chanPtr, mask);
        return TCL_OK;
    }

    /*
     * Make the script record that will link between the event and the
     * script to invoke. This also creates a channel event handler which
     * will evaluate the script in the supplied interpreter.
     */

    CreateScriptRecord(interp, chanPtr, mask, argv[3]);
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclTestChannelCmd --
 *
 *	Implements the Tcl "testchannel" debugging command and its
 *	subcommands. This is part of the testing environment but must be
 *	in this file instead of tclTest.c because it needs access to the
 *	fields of struct Channel.
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
TclTestChannelCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter for result. */
    int argc;			/* Count of additional args. */
    char **argv;		/* Additional arg strings. */
{
    char *cmdName;		/* Sub command. */
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashSearch hSearch;	/* Search variable. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;		/* The actual channel. */
    Tcl_Channel chan;		/* The opaque type. */
    size_t len;			/* Length of subcommand string. */
    int IOQueued;		/* How much IO is queued inside channel? */
    ChannelBuffer *bufPtr;	/* For iterating over queued IO. */
    char buf[128];		/* For sprintf. */
    
    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " subcommand ?additional args..?\"", (char *) NULL);
        return TCL_ERROR;
    }
    cmdName = argv[1];
    len = strlen(cmdName);

    chanPtr = (Channel *) NULL;
    if (argc > 2) {
        chan = Tcl_GetChannel(interp, argv[2], NULL);
        if (chan == (Tcl_Channel) NULL) {
            return TCL_ERROR;
        }
        chanPtr = (Channel *) chan;
    }
    
    if ((cmdName[0] == 'i') && (strncmp(cmdName, "info", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " info channelName\"", (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_AppendElement(interp, argv[2]);
        Tcl_AppendElement(interp, chanPtr->typePtr->typeName);
        if (chanPtr->flags & TCL_READABLE) {
            Tcl_AppendElement(interp, "read");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (chanPtr->flags & TCL_WRITABLE) {
            Tcl_AppendElement(interp, "write");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (chanPtr->flags & CHANNEL_NONBLOCKING) {
            Tcl_AppendElement(interp, "nonblocking");
        } else {
            Tcl_AppendElement(interp, "blocking");
        }
        if (chanPtr->flags & CHANNEL_LINEBUFFERED) {
            Tcl_AppendElement(interp, "line");
        } else if (chanPtr->flags & CHANNEL_UNBUFFERED) {
            Tcl_AppendElement(interp, "none");
        } else {
            Tcl_AppendElement(interp, "full");
        }
        if (chanPtr->flags & BG_FLUSH_SCHEDULED) {
            Tcl_AppendElement(interp, "async_flush");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (chanPtr->flags & CHANNEL_EOF) {
            Tcl_AppendElement(interp, "eof");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (chanPtr->flags & CHANNEL_BLOCKED) {
            Tcl_AppendElement(interp, "blocked");
        } else {
            Tcl_AppendElement(interp, "unblocked");
        }
        if (chanPtr->inputTranslation == TCL_TRANSLATE_AUTO) {
            Tcl_AppendElement(interp, "auto");
            if (chanPtr->flags & INPUT_SAW_CR) {
                Tcl_AppendElement(interp, "saw_cr");
            } else {
                Tcl_AppendElement(interp, "");
            }
        } else if (chanPtr->inputTranslation == TCL_TRANSLATE_LF) {
            Tcl_AppendElement(interp, "lf");
            Tcl_AppendElement(interp, "");
        } else if (chanPtr->inputTranslation == TCL_TRANSLATE_CR) {
            Tcl_AppendElement(interp, "cr");
            Tcl_AppendElement(interp, "");
        } else if (chanPtr->inputTranslation == TCL_TRANSLATE_CRLF) {
            Tcl_AppendElement(interp, "crlf");
            if (chanPtr->flags & INPUT_SAW_CR) {
                Tcl_AppendElement(interp, "queued_cr");
            } else {
                Tcl_AppendElement(interp, "");
            }
        }
        if (chanPtr->outputTranslation == TCL_TRANSLATE_AUTO) {
            Tcl_AppendElement(interp, "auto");
        } else if (chanPtr->outputTranslation == TCL_TRANSLATE_LF) {
            Tcl_AppendElement(interp, "lf");
        } else if (chanPtr->outputTranslation == TCL_TRANSLATE_CR) {
            Tcl_AppendElement(interp, "cr");
        } else if (chanPtr->outputTranslation == TCL_TRANSLATE_CRLF) {
            Tcl_AppendElement(interp, "crlf");
        }
        for (IOQueued = 0, bufPtr = chanPtr->inQueueHead;
                 bufPtr != (ChannelBuffer *) NULL;
                 bufPtr = bufPtr->nextPtr) {
            IOQueued += bufPtr->nextAdded - bufPtr->nextRemoved;
        }
        sprintf(buf, "%d", IOQueued);
        Tcl_AppendElement(interp, buf);
        
        IOQueued = 0;
        if (chanPtr->curOutPtr != (ChannelBuffer *) NULL) {
            IOQueued = chanPtr->curOutPtr->nextAdded -
                chanPtr->curOutPtr->nextRemoved;
        }
        for (bufPtr = chanPtr->outQueueHead;
                 bufPtr != (ChannelBuffer *) NULL;
                 bufPtr = bufPtr->nextPtr) {
            IOQueued += (bufPtr->nextAdded - bufPtr->nextRemoved);
        }
        sprintf(buf, "%d", IOQueued);
        Tcl_AppendElement(interp, buf);
        
        sprintf(buf, "%d", Tcl_Tell((Tcl_Channel) chanPtr));
        Tcl_AppendElement(interp, buf);

        sprintf(buf, "%d", chanPtr->refCount);
        Tcl_AppendElement(interp, buf);

        return TCL_OK;
    }

    if ((cmdName[0] == 'i') &&
            (strncmp(cmdName, "inputbuffered", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        for (IOQueued = 0, bufPtr = chanPtr->inQueueHead;
                 bufPtr != (ChannelBuffer *) NULL;
                 bufPtr = bufPtr->nextPtr) {
            IOQueued += bufPtr->nextAdded - bufPtr->nextRemoved;
        }
        sprintf(buf, "%d", IOQueued);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_OK;
    }
        
    if ((cmdName[0] == 'm') && (strncmp(cmdName, "mode", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        if (chanPtr->flags & TCL_READABLE) {
            Tcl_AppendElement(interp, "read");
        } else {
            Tcl_AppendElement(interp, "");
        }
        if (chanPtr->flags & TCL_WRITABLE) {
            Tcl_AppendElement(interp, "write");
        } else {
            Tcl_AppendElement(interp, "");
        }
        return TCL_OK;
    }
    
    if ((cmdName[0] == 'n') && (strncmp(cmdName, "name", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, chanPtr->channelName, (char *) NULL);
        return TCL_OK;
    }
    
    if ((cmdName[0] == 'o') && (strncmp(cmdName, "open", len) == 0)) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
                 hPtr != (Tcl_HashEntry *) NULL;
                 hPtr = Tcl_NextHashEntry(&hSearch)) {
            Tcl_AppendElement(interp, Tcl_GetHashKey(hTblPtr, hPtr));
        }
        return TCL_OK;
    }

    if ((cmdName[0] == 'o') &&
            (strncmp(cmdName, "outputbuffered", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        IOQueued = 0;
        if (chanPtr->curOutPtr != (ChannelBuffer *) NULL) {
            IOQueued = chanPtr->curOutPtr->nextAdded -
                chanPtr->curOutPtr->nextRemoved;
        }
        for (bufPtr = chanPtr->outQueueHead;
                 bufPtr != (ChannelBuffer *) NULL;
                 bufPtr = bufPtr->nextPtr) {
            IOQueued += (bufPtr->nextAdded - bufPtr->nextRemoved);
        }
        sprintf(buf, "%d", IOQueued);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_OK;
    }
        
    if ((cmdName[0] == 'q') &&
            (strncmp(cmdName, "queuedcr", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        Tcl_AppendResult(interp,
                (chanPtr->flags & INPUT_SAW_CR) ? "1" : "0",
                (char *) NULL);
        return TCL_OK;
    }
    
    if ((cmdName[0] == 'r') && (strncmp(cmdName, "readable", len) == 0)) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
                 hPtr != (Tcl_HashEntry *) NULL;
                 hPtr = Tcl_NextHashEntry(&hSearch)) {
            chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
            if (chanPtr->flags & TCL_READABLE) {
                Tcl_AppendElement(interp, Tcl_GetHashKey(hTblPtr, hPtr));
            }
        }
        return TCL_OK;
    }

    if ((cmdName[0] == 'r') && (strncmp(cmdName, "refcount", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        
        sprintf(buf, "%d", chanPtr->refCount);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_OK;
    }
    
    if ((cmdName[0] == 't') && (strncmp(cmdName, "type", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, chanPtr->typePtr->typeName, (char *) NULL);
        return TCL_OK;
    }
    
    if ((cmdName[0] == 'w') && (strncmp(cmdName, "writable", len) == 0)) {
        hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            return TCL_OK;
        }
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
                 hPtr != (Tcl_HashEntry *) NULL;
                 hPtr = Tcl_NextHashEntry(&hSearch)) {
            chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
            if (chanPtr->flags & TCL_WRITABLE) {
                Tcl_AppendElement(interp, Tcl_GetHashKey(hTblPtr, hPtr));
            }
        }
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad option \"", cmdName, "\": should be ",
            "info, open, readable, or writable",
            (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclTestChannelEventCmd --
 *
 *	This procedure implements the "testchannelevent" command. It is
 *	used to test the Tcl channel event mechanism. It is present in
 *	this file instead of tclTest.c because it needs access to the
 *	internal structure of the channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes and returns channel event handlers.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
TclTestChannelEventCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Channel *chanPtr;
    EventScriptRecord *esPtr, *prevEsPtr, *nextEsPtr;
    char *cmd;
    int index, i, mask, len;

    if ((argc < 3) || (argc > 5)) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " channelName cmd ?arg1? ?arg2?\"", (char *) NULL);
        return TCL_ERROR;
    }
    chanPtr = (Channel *) Tcl_GetChannel(interp, argv[1], NULL);
    if (chanPtr == (Channel *) NULL) {
        return TCL_ERROR;
    }
    cmd = argv[2];
    len = strlen(cmd);
    if ((cmd[0] == 'a') && (strncmp(cmd, "add", (unsigned) len) == 0)) {
        if (argc != 5) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName add eventSpec script\"", (char *) NULL);
            return TCL_ERROR;
        }
        if (strcmp(argv[3], "readable") == 0) {
            mask = TCL_READABLE;
        } else if (strcmp(argv[3], "writable") == 0) {
            mask = TCL_WRITABLE;
        } else {
            Tcl_AppendResult(interp, "bad event name \"", argv[3],
                    "\": must be readable or writable", (char *) NULL);
            return TCL_ERROR;
        }

        esPtr = (EventScriptRecord *) ckalloc((unsigned)
                sizeof(EventScriptRecord));
        esPtr->nextPtr = chanPtr->scriptRecordPtr;
        chanPtr->scriptRecordPtr = esPtr;
        
        esPtr->chanPtr = chanPtr;
        esPtr->interp = interp;
        esPtr->mask = mask;
        esPtr->script = ckalloc((unsigned) (strlen(argv[4]) + 1));
        strcpy(esPtr->script, argv[4]);

        Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
                ChannelEventScriptInvoker, (ClientData) esPtr);
        
        return TCL_OK;
    }

    if ((cmd[0] == 'd') && (strncmp(cmd, "delete", (unsigned) len) == 0)) {
        if (argc != 4) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName delete index\"", (char *) NULL);
            return TCL_ERROR;
        }
        if (Tcl_GetInt(interp, argv[3], &index) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (index < 0) {
            Tcl_AppendResult(interp, "bad event index: ", argv[3],
                    ": must be nonnegative", (char *) NULL);
            return TCL_ERROR;
        }
        for (i = 0, esPtr = chanPtr->scriptRecordPtr;
                 (i < index) && (esPtr != (EventScriptRecord *) NULL);
                 i++, esPtr = esPtr->nextPtr) {
	    /* Empty loop body. */
        }
        if (esPtr == (EventScriptRecord *) NULL) {
            Tcl_AppendResult(interp, "bad event index ", argv[3],
                    ": out of range", (char *) NULL);
            return TCL_ERROR;
        }
        if (esPtr == chanPtr->scriptRecordPtr) {
            chanPtr->scriptRecordPtr = esPtr->nextPtr;
        } else {
            for (prevEsPtr = chanPtr->scriptRecordPtr;
                     (prevEsPtr != (EventScriptRecord *) NULL) &&
                         (prevEsPtr->nextPtr != esPtr);
                     prevEsPtr = prevEsPtr->nextPtr) {
                /* Empty loop body. */
            }
            if (prevEsPtr == (EventScriptRecord *) NULL) {
                panic("TclTestChannelEventCmd: damaged event script list");
            }
            prevEsPtr->nextPtr = esPtr->nextPtr;
        }
        Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                ChannelEventScriptInvoker, (ClientData) esPtr);
        Tcl_EventuallyFree((ClientData)esPtr->script, TCL_DYNAMIC);
        ckfree((char *) esPtr);

        return TCL_OK;
    }

    if ((cmd[0] == 'l') && (strncmp(cmd, "list", (unsigned) len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName list\"", (char *) NULL);
            return TCL_ERROR;
        }
        for (esPtr = chanPtr->scriptRecordPtr;
                 esPtr != (EventScriptRecord *) NULL;
                 esPtr = esPtr->nextPtr) {
            Tcl_AppendElement(interp,
                    esPtr->mask == TCL_READABLE ? "readable" : "writable");
            Tcl_AppendElement(interp, esPtr->script);
        }
        return TCL_OK;
    }

    if ((cmd[0] == 'r') && (strncmp(cmd, "removeall", (unsigned) len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " channelName removeall\"", (char *) NULL);
            return TCL_ERROR;
        }
        for (esPtr = chanPtr->scriptRecordPtr;
                 esPtr != (EventScriptRecord *) NULL;
                 esPtr = nextEsPtr) {
            nextEsPtr = esPtr->nextPtr;
            Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
                    ChannelEventScriptInvoker, (ClientData) esPtr);
            Tcl_EventuallyFree((ClientData)esPtr->script, TCL_DYNAMIC);
            ckfree((char *) esPtr);
        }
        chanPtr->scriptRecordPtr = (EventScriptRecord *) NULL;
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad command ", cmd, ", must be one of ",
            "add, delete, list, or removeall", (char *) NULL);
    return TCL_ERROR;

}
