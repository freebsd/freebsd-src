/* 
 * tclEvent.c --
 *
 *	This file provides basic event-managing facilities for Tcl,
 *	including an event queue, and mechanisms for attaching
 *	callbacks to certain events.
 *
 *	It also contains the command procedures for the commands
 *	"after", "vwait", and "update".
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclEvent.c 1.128 96/07/23 16:12:34
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * For each file registered in a call to Tcl_CreateFileHandler,
 * there is one record of the following type.  All of these records
 * are chained together into a single list.
 */

typedef struct FileHandler {
    Tcl_File file;		/* Generic file handle for file. */
    int mask;			/* Mask of desired events: TCL_READABLE, etc. */
    int readyMask;		/* Events that were ready the last time that
				 * FileHandlerCheckProc checked this file. */
    Tcl_FileProc *proc;		/* Procedure to call, in the style of
				 * Tcl_CreateFileHandler.  This is NULL
				 * if the handler was created by
				 * Tcl_CreateFileHandler2. */
    ClientData clientData;	/* Argument to pass to proc. */
    struct FileHandler *nextPtr;/* Next in list of all files we care
				 * about (NULL for end of list). */
} FileHandler;

static FileHandler *firstFileHandlerPtr = (FileHandler *) NULL;
				/* List of all file handlers. */
static int fileEventSourceCreated = 0;
				/* Non-zero means that the file event source
				 * hasn't been registerd with the Tcl
				 * notifier yet. */

/*
 * The following structure is what is added to the Tcl event queue when
 * file handlers are ready to fire.
 */

typedef struct FileHandlerEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    Tcl_File file;		/* File descriptor that is ready.  Used
				 * to find the FileHandler structure for
				 * the file (can't point directly to the
				 * FileHandler structure because it could
				 * go away while the event is queued). */
} FileHandlerEvent;

/*
 * For each timer callback that's pending (either regular or "modal"),
 * there is one record of the following type.  The normal handlers
 * (created by Tcl_CreateTimerHandler) are chained together in a
 * list sorted by time (earliest event first).
 */

typedef struct TimerHandler {
    Tcl_Time time;			/* When timer is to fire. */
    Tcl_TimerProc *proc;		/* Procedure to call. */
    ClientData clientData;		/* Argument to pass to proc. */
    Tcl_TimerToken token;		/* Identifies event so it can be
					 * deleted.  Not used in modal
					 * timeouts. */
    struct TimerHandler *nextPtr;	/* Next event in queue, or NULL for
					 * end of queue. */
} TimerHandler;

static TimerHandler *firstTimerHandlerPtr = NULL;
					/* First event in queue. */
static int timerEventSourceCreated = 0;	/* 0 means that the timer event source
					 * hasn't yet been registered with the
					 * Tcl notifier. */

/*
 * The information below describes a stack of modal timeouts managed by
 * Tcl_CreateModalTimer and Tcl_DeleteModalTimer.  Only the first element
 * in the list is used at any given time.
 */

static TimerHandler *firstModalHandlerPtr = NULL;

/*
 * The following structure is what's added to the Tcl event queue when
 * timer handlers are ready to fire.
 */

typedef struct TimerEvent {
    Tcl_Event header;			/* Information that is standard for
					 * all events. */
    Tcl_Time time;			/* All timer events that specify this
					 * time or earlier are ready
                                         * to fire. */
} TimerEvent;

/*
 * There is one of the following structures for each of the
 * handlers declared in a call to Tcl_DoWhenIdle.  All of the
 * currently-active handlers are linked together into a list.
 */

typedef struct IdleHandler {
    Tcl_IdleProc (*proc);	/* Procedure to call. */
    ClientData clientData;	/* Value to pass to proc. */
    int generation;		/* Used to distinguish older handlers from
				 * recently-created ones. */
    struct IdleHandler *nextPtr;/* Next in list of active handlers. */
} IdleHandler;

static IdleHandler *idleList = NULL;
				/* First in list of all idle handlers. */
static IdleHandler *lastIdlePtr = NULL;
				/* Last in list (or NULL for empty list). */
static int idleGeneration = 0;	/* Used to fill in the "generation" fields
				 * of IdleHandler structures.  Increments
				 * each time Tcl_DoOneEvent starts calling
				 * idle handlers, so that all old handlers
				 * can be called without calling any of the
				 * new ones created by old ones. */

/*
 * The data structure below is used by the "after" command to remember
 * the command to be executed later.  All of the pending "after" commands
 * for an interpreter are linked together in a list.
 */

typedef struct AfterInfo {
    struct AfterAssocData *assocPtr;
				/* Pointer to the "tclAfter" assocData for
				 * the interp in which command will be
				 * executed. */
    char *command;		/* Command to execute.  Malloc'ed, so must
				 * be freed when structure is deallocated. */
    int id;			/* Integer identifier for command;  used to
				 * cancel it. */
    Tcl_TimerToken token;	/* Used to cancel the "after" command.  NULL
				 * means that the command is run as an
				 * idle handler rather than as a timer
				 * handler.  NULL means this is an "after
				 * idle" handler rather than a
                                 * timer handler. */
    struct AfterInfo *nextPtr;	/* Next in list of all "after" commands for
				 * this interpreter. */
} AfterInfo;

/*
 * One of the following structures is associated with each interpreter
 * for which an "after" command has ever been invoked.  A pointer to
 * this structure is stored in the AssocData for the "tclAfter" key.
 */

typedef struct AfterAssocData {
    Tcl_Interp *interp;		/* The interpreter for which this data is
				 * registered. */
    AfterInfo *firstAfterPtr;	/* First in list of all "after" commands
				 * still pending for this interpreter, or
				 * NULL if none. */
} AfterAssocData;

/*
 * The data structure below is used to report background errors.  One
 * such structure is allocated for each error;  it holds information
 * about the interpreter and the error until bgerror can be invoked
 * later as an idle handler.
 */

typedef struct BgError {
    Tcl_Interp *interp;		/* Interpreter in which error occurred.  NULL
				 * means this error report has been cancelled
				 * (a previous report generated a break). */
    char *errorMsg;		/* The error message (interp->result when
				 * the error occurred).  Malloc-ed. */
    char *errorInfo;		/* Value of the errorInfo variable
				 * (malloc-ed). */
    char *errorCode;		/* Value of the errorCode variable
				 * (malloc-ed). */
    struct BgError *nextPtr;	/* Next in list of all pending error
				 * reports for this interpreter, or NULL
				 * for end of list. */
} BgError;

/*
 * One of the structures below is associated with the "tclBgError"
 * assoc data for each interpreter.  It keeps track of the head and
 * tail of the list of pending background errors for the interpreter.
 */

typedef struct ErrAssocData {
    BgError *firstBgPtr;	/* First in list of all background errors
				 * waiting to be processed for this
				 * interpreter (NULL if none). */
    BgError *lastBgPtr;		/* Last in list of all background errors
				 * waiting to be processed for this
				 * interpreter (NULL if none). */
} ErrAssocData;

/*
 * For each exit handler created with a call to Tcl_CreateExitHandler
 * there is a structure of the following type:
 */

typedef struct ExitHandler {
    Tcl_ExitProc *proc;		/* Procedure to call when process exits. */
    ClientData clientData;	/* One word of information to pass to proc. */
    struct ExitHandler *nextPtr;/* Next in list of all exit handlers for
				 * this application, or NULL for end of list. */
} ExitHandler;

static ExitHandler *firstExitPtr = NULL;
				/* First in list of all exit handlers for
				 * application. */

/*
 * Structures of the following type are used during the execution
 * of Tcl_WaitForFile, to keep track of the file and timeout.
 */

typedef struct FileWait {
    Tcl_File file;		/* File to wait on. */
    int mask;			/* Conditions to wait for (TCL_READABLE,
				 * etc.) */
    int timeout;		/* Original "timeout" argument to
				 * Tcl_WaitForFile. */
    Tcl_Time abortTime;		/* Time at which to abort the wait. */
    int present;		/* Conditions present on the file during
				 * the last time through the event loop. */
    int done;			/* Non-zero means we're done:  either one of
				 * the desired conditions is present or the
				 * timeout period has elapsed. */
} FileWait;

/*
 * The following variable is a "secret" indication to Tcl_Exit that
 * it should dump out the state of memory before exiting.  If the
 * value is non-NULL, it gives the name of the file in which to
 * dump memory usage information.
 */

char *tclMemDumpFileName = NULL;

/*
 * Prototypes for procedures referenced only in this file:
 */

static void		AfterCleanupProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static void		AfterProc _ANSI_ARGS_((ClientData clientData));
static void		BgErrorDeleteProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static void		FileHandlerCheckProc _ANSI_ARGS_((
			    ClientData clientData, int flags));
static int		FileHandlerEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
			    int flags));
static void		FileHandlerExitProc _ANSI_ARGS_((ClientData data));
static void		FileHandlerSetupProc _ANSI_ARGS_((
			    ClientData clientData, int flags));
static void		FreeAfterPtr _ANSI_ARGS_((AfterInfo *afterPtr));
static AfterInfo *	GetAfterEvent _ANSI_ARGS_((AfterAssocData *assocPtr,
			    char *string));
static void		HandleBgErrors _ANSI_ARGS_((ClientData clientData));
static void		TimerHandlerCheckProc _ANSI_ARGS_((
			    ClientData clientData, int flags));
static int		TimerHandlerEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
			    int flags));
static void		TimerHandlerExitProc _ANSI_ARGS_((ClientData data));
static void		TimerHandlerSetupProc _ANSI_ARGS_((
			    ClientData clientData, int flags));
static char *		VwaitVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateFileHandler --
 *
 *	Arrange for a given procedure to be invoked whenever
 *	a given file becomes readable or writable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever the I/O channel given by file becomes
 *	ready in the way indicated by mask, proc will be invoked.
 *	See the manual entry for details on the calling sequence
 *	to proc.  If file is already registered then the old mask
 *	and proc and clientData values will be replaced with
 *	new ones.
 *
 *--------------------------------------------------------------
 */

void
Tcl_CreateFileHandler(file, mask, proc, clientData)
    Tcl_File file;		/* Handle of stream to watch. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions under which
				 * proc should be called. */
    Tcl_FileProc *proc;		/* Procedure to call for each
				 * selected event. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register FileHandler *filePtr;

    if (!fileEventSourceCreated) {
	fileEventSourceCreated = 1;
	Tcl_CreateEventSource(FileHandlerSetupProc, FileHandlerCheckProc,
		(ClientData) NULL);
        Tcl_CreateExitHandler(FileHandlerExitProc, (ClientData) NULL);
    }

    /*
     * Make sure the file isn't already registered.  Create a
     * new record in the normal case where there's no existing
     * record.
     */

    for (filePtr = firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->file == file) {
	    break;
	}
    }
    if (filePtr == NULL) {
	filePtr = (FileHandler *) ckalloc(sizeof(FileHandler));
	filePtr->file = file;
	filePtr->nextPtr = firstFileHandlerPtr;
	firstFileHandlerPtr = filePtr;
    }

    /*
     * The remainder of the initialization below is done regardless
     * of whether or not this is a new record or a modification of
     * an old one.
     */

    filePtr->mask = mask;
    filePtr->readyMask = 0;
    filePtr->proc = proc;
    filePtr->clientData = clientData;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DeleteFileHandler --
 *
 *	Cancel a previously-arranged callback arrangement for
 *	a file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a callback was previously registered on file, remove it.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DeleteFileHandler(file)
    Tcl_File file;		/* Stream id for which to remove
				 * callback procedure. */
{
    FileHandler *filePtr, *prevPtr;

    /*
     * Find the entry for the given file (and return if there
     * isn't one).
     */

    for (prevPtr = NULL, filePtr = firstFileHandlerPtr; ;
	    prevPtr = filePtr, filePtr = filePtr->nextPtr) {
	if (filePtr == NULL) {
	    return;
	}
	if (filePtr->file == file) {
	    break;
	}
    }

    /*
     * Clean up information in the callback record.
     */

    if (prevPtr == NULL) {
	firstFileHandlerPtr = filePtr->nextPtr;
    } else {
	prevPtr->nextPtr = filePtr->nextPtr;
    }
    ckfree((char *) filePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerExitProc --
 *
 *	Cleanup procedure to delete the file event source during exit
 *	cleanup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the file event source.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
FileHandlerExitProc(clientData)
    ClientData clientData;		/* Not used. */
{
    Tcl_DeleteEventSource(FileHandlerSetupProc, FileHandlerCheckProc,
            (ClientData) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerSetupProc --
 *
 *	This procedure is part of the "event source" for file handlers.
 *	It is invoked by Tcl_DoOneEvent before it calls select (or
 *	whatever it uses to wait).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tells the notifier which files should be waited for.
 *
 *----------------------------------------------------------------------
 */

static void
FileHandlerSetupProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include
					 * TCL_FILE_EVENTS then we do
					 * nothing. */
{
    FileHandler *filePtr;

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    for (filePtr = firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->mask != 0) {
	    Tcl_WatchFile(filePtr->file, filePtr->mask);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerCheckProc --
 *
 *	This procedure is the second part of the "event source" for
 *	file handlers.  It is invoked by Tcl_DoOneEvent after it calls
 *	select (or whatever it uses to wait for events).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Makes entries on the Tcl event queue for each file that is
 *	now ready.
 *
 *----------------------------------------------------------------------
 */

static void
FileHandlerCheckProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include 
					 * TCL_FILE_EVENTS then we do
					 * nothing. */
{
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr;

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    for (filePtr = firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->mask != 0) {
	    filePtr->readyMask = Tcl_FileReady(filePtr->file, filePtr->mask);
	    if (filePtr->readyMask != 0) {
		fileEvPtr = (FileHandlerEvent *) ckalloc(
			sizeof(FileHandlerEvent));
		fileEvPtr->header.proc = FileHandlerEventProc;
		fileEvPtr->file = filePtr->file;
		Tcl_QueueEvent((Tcl_Event *) fileEvPtr, TCL_QUEUE_TAIL);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerEventProc --
 *
 *	This procedure is called by Tcl_DoOneEvent when a file event
 *	reaches the front of the event queue.  This procedure is responsible
 *	for actually handling the event by invoking the callback for the
 *	file handler.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the file handler's callback procedure does
 *
 *----------------------------------------------------------------------
 */

static int
FileHandlerEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr = (FileHandlerEvent *) evPtr;
    int mask;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the file handlers to find the one whose handle matches
     * the event.  We do this rather than keeping a pointer to the file
     * handler directly in the event, so that the handler can be deleted
     * while the event is queued without leaving a dangling pointer.
     */

    for (filePtr = firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->file != fileEvPtr->file) {
	    continue;
	}

	/*
	 * The code is tricky for two reasons:
	 * 1. The file handler's desired events could have changed
	 *    since the time when the event was queued, so AND the
	 *    ready mask with the desired mask.
	 * 2. The file could have been closed and re-opened since
	 *    the time when the event was queued.  This is why the
	 *    ready mask is stored in the file handler rather than
	 *    the queued event:  it will be zeroed when a new
	 *    file handler is created for the newly opened file.
	 */

	mask = filePtr->readyMask & filePtr->mask;
	filePtr->readyMask = 0;
	if (mask != 0) {
	    (*filePtr->proc)(filePtr->clientData, mask);
	}
	break;
    }
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateTimerHandler --
 *
 *	Arrange for a given procedure to be invoked at a particular
 *	time in the future.
 *
 * Results:
 *	The return value is a token for the timer event, which
 *	may be used to delete the event before it fires.
 *
 * Side effects:
 *	When milliseconds have elapsed, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
Tcl_CreateTimerHandler(milliseconds, proc, clientData)
    int milliseconds;		/* How many milliseconds to wait
				 * before invoking proc. */
    Tcl_TimerProc *proc;	/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register TimerHandler *timerHandlerPtr, *tPtr2, *prevPtr;
    static int id = 0;

    if (!timerEventSourceCreated) {
	timerEventSourceCreated = 1;
	Tcl_CreateEventSource(TimerHandlerSetupProc, TimerHandlerCheckProc,
		(ClientData) NULL);
        Tcl_CreateExitHandler(TimerHandlerExitProc, (ClientData) NULL);
    }

    timerHandlerPtr = (TimerHandler *) ckalloc(sizeof(TimerHandler));

    /*
     * Compute when the event should fire.
     */

    TclpGetTime(&timerHandlerPtr->time);
    timerHandlerPtr->time.sec += milliseconds/1000;
    timerHandlerPtr->time.usec += (milliseconds%1000)*1000;
    if (timerHandlerPtr->time.usec >= 1000000) {
	timerHandlerPtr->time.usec -= 1000000;
	timerHandlerPtr->time.sec += 1;
    }
    
    /*
     * Fill in other fields for the event.
     */

    timerHandlerPtr->proc = proc;
    timerHandlerPtr->clientData = clientData;
    id++;
    timerHandlerPtr->token = (Tcl_TimerToken) id;

    /*
     * Add the event to the queue in the correct position
     * (ordered by event firing time).
     */

    for (tPtr2 = firstTimerHandlerPtr, prevPtr = NULL; tPtr2 != NULL;
	    prevPtr = tPtr2, tPtr2 = tPtr2->nextPtr) {
	if ((tPtr2->time.sec > timerHandlerPtr->time.sec)
		|| ((tPtr2->time.sec == timerHandlerPtr->time.sec)
		&& (tPtr2->time.usec > timerHandlerPtr->time.usec))) {
	    break;
	}
    }
    timerHandlerPtr->nextPtr = tPtr2;
    if (prevPtr == NULL) {
	firstTimerHandlerPtr = timerHandlerPtr;
    } else {
	prevPtr->nextPtr = timerHandlerPtr;
    }
    return timerHandlerPtr->token;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DeleteTimerHandler --
 *
 *	Delete a previously-registered timer handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroy the timer callback identified by TimerToken,
 *	so that its associated procedure will not be called.
 *	If the callback has already fired, or if the given
 *	token doesn't exist, then nothing happens.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DeleteTimerHandler(token)
    Tcl_TimerToken token;	/* Result previously returned by
				 * Tcl_DeleteTimerHandler. */
{
    register TimerHandler *timerHandlerPtr, *prevPtr;

    for (timerHandlerPtr = firstTimerHandlerPtr, prevPtr = NULL;
	    timerHandlerPtr != NULL; prevPtr = timerHandlerPtr,
	    timerHandlerPtr = timerHandlerPtr->nextPtr) {
	if (timerHandlerPtr->token != token) {
	    continue;
	}
	if (prevPtr == NULL) {
	    firstTimerHandlerPtr = timerHandlerPtr->nextPtr;
	} else {
	    prevPtr->nextPtr = timerHandlerPtr->nextPtr;
	}
	ckfree((char *) timerHandlerPtr);
	return;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateModalTimeout --
 *
 *	Arrange for a given procedure to be invoked at a particular
 *	time in the future, independently of all other timer events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When milliseconds have elapsed, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

void
Tcl_CreateModalTimeout(milliseconds, proc, clientData)
    int milliseconds;		/* How many milliseconds to wait
				 * before invoking proc. */
    Tcl_TimerProc *proc;	/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    TimerHandler *timerHandlerPtr;

    if (!timerEventSourceCreated) {
	timerEventSourceCreated = 1;
	Tcl_CreateEventSource(TimerHandlerSetupProc, TimerHandlerCheckProc,
		(ClientData) NULL);
        Tcl_CreateExitHandler(TimerHandlerExitProc, (ClientData) NULL);
    }

    timerHandlerPtr = (TimerHandler *) ckalloc(sizeof(TimerHandler));

    /*
     * Compute when the timeout should fire and fill in the other fields
     * of the handler.
     */

    TclpGetTime(&timerHandlerPtr->time);
    timerHandlerPtr->time.sec += milliseconds/1000;
    timerHandlerPtr->time.usec += (milliseconds%1000)*1000;
    if (timerHandlerPtr->time.usec >= 1000000) {
	timerHandlerPtr->time.usec -= 1000000;
	timerHandlerPtr->time.sec += 1;
    }
    timerHandlerPtr->proc = proc;
    timerHandlerPtr->clientData = clientData;

    /*
     * Push the handler on the top of the modal stack.
     */

    timerHandlerPtr->nextPtr = firstModalHandlerPtr;
    firstModalHandlerPtr = timerHandlerPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DeleteModalTimeout --
 *
 *	Remove the topmost modal timer handler from the stack of
 *	modal  handlers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the topmost modal timeout handler, which must
 *	match proc and clientData.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DeleteModalTimeout(proc, clientData)
    Tcl_TimerProc *proc;	/* Callback procedure for the timeout. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    TimerHandler *timerHandlerPtr;

    timerHandlerPtr = firstModalHandlerPtr;
    firstModalHandlerPtr = timerHandlerPtr->nextPtr;
    if ((timerHandlerPtr->proc != proc)
	    || (timerHandlerPtr->clientData != clientData)) {
	panic("Tcl_DeleteModalTimeout found timeout stack corrupted");
    }
    ckfree((char *) timerHandlerPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerHandlerSetupProc --
 *
 *	This procedure is part of the "event source" for timers.
 *	It is invoked by Tcl_DoOneEvent before it calls select (or
 *	whatever it uses to wait).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tells the notifier how long to sleep if it decides to block.
 *
 *----------------------------------------------------------------------
 */

static void
TimerHandlerSetupProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include
					 * TCL_TIMER_EVENTS then we only
					 * consider modal timers. */
{
    TimerHandler *timerHandlerPtr, *tPtr2;
    Tcl_Time blockTime;

    /*
     * Find the timer handler (regular or modal) that fires first.
     */

    timerHandlerPtr = firstTimerHandlerPtr;
    if (!(flags & TCL_TIMER_EVENTS)) {
	timerHandlerPtr = NULL;
    }
    if (timerHandlerPtr != NULL) {
	tPtr2 = firstModalHandlerPtr;
	if (tPtr2 != NULL) {
	    if ((timerHandlerPtr->time.sec > tPtr2->time.sec)
		    || ((timerHandlerPtr->time.sec == tPtr2->time.sec)
		    && (timerHandlerPtr->time.usec > tPtr2->time.usec))) {
		timerHandlerPtr = tPtr2;
	    }
	}
    } else {
	timerHandlerPtr = firstModalHandlerPtr;
    }
    if (timerHandlerPtr == NULL) {
	return;
    }

    TclpGetTime(&blockTime);
    blockTime.sec = timerHandlerPtr->time.sec - blockTime.sec;
    blockTime.usec = timerHandlerPtr->time.usec - blockTime.usec;
    if (blockTime.usec < 0) {
	blockTime.sec -= 1;
	blockTime.usec += 1000000;
    }
    if (blockTime.sec < 0) {
	blockTime.sec = 0;
	blockTime.usec = 0;
    }
    Tcl_SetMaxBlockTime(&blockTime);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerHandlerCheckProc --
 *
 *	This procedure is the second part of the "event source" for
 *	file handlers.  It is invoked by Tcl_DoOneEvent after it calls
 *	select (or whatever it uses to wait for events).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Makes entries on the Tcl event queue for each file that is
 *	now ready.
 *
 *----------------------------------------------------------------------
 */

static void
TimerHandlerCheckProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include 
					 * TCL_TIMER_EVENTS then we only
					 * consider modal timeouts. */
{
    TimerHandler *timerHandlerPtr;
    TimerEvent *timerEvPtr;
    int triggered, gotTime;
    Tcl_Time curTime;

    triggered = 0;
    gotTime = 0;
    timerHandlerPtr = firstTimerHandlerPtr;
    if ((flags & TCL_TIMER_EVENTS) && (timerHandlerPtr != NULL)) {
	TclpGetTime(&curTime);
	gotTime = 1;
	if ((timerHandlerPtr->time.sec < curTime.sec)
		|| ((timerHandlerPtr->time.sec == curTime.sec)
		&& (timerHandlerPtr->time.usec <= curTime.usec))) {
	    triggered = 1;
	}
    }
    timerHandlerPtr = firstModalHandlerPtr;
    if (timerHandlerPtr != NULL) {
	if (!gotTime) {
	    TclpGetTime(&curTime);
	}
	if ((timerHandlerPtr->time.sec < curTime.sec)
		|| ((timerHandlerPtr->time.sec == curTime.sec)
		&& (timerHandlerPtr->time.usec <= curTime.usec))) {
	    triggered = 1;
	}
    }
    if (triggered) {
	timerEvPtr = (TimerEvent *) ckalloc(sizeof(TimerEvent));
	timerEvPtr->header.proc = TimerHandlerEventProc;
	timerEvPtr->time.sec = curTime.sec;
	timerEvPtr->time.usec = curTime.usec;
	Tcl_QueueEvent((Tcl_Event *) timerEvPtr, TCL_QUEUE_TAIL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimerHandlerExitProc --
 *
 *	Callback invoked during exit cleanup to destroy the timer event
 *	source.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the timer event source.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TimerHandlerExitProc(clientData)
    ClientData clientData;		/* Not used. */
{
    Tcl_DeleteEventSource(TimerHandlerSetupProc, TimerHandlerCheckProc,
            (ClientData) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerHandlerEventProc --
 *
 *	This procedure is called by Tcl_DoOneEvent when a timer event
 *	reaches the front of the event queue.  This procedure handles
 *	the event by invoking the callbacks for all timers that are
 *	ready.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_TIMER_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the timer handler callback procedures do.
 *
 *----------------------------------------------------------------------
 */

static int
TimerHandlerEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    TimerHandler *timerHandlerPtr;
    TimerEvent *timerEvPtr = (TimerEvent *) evPtr;

    /*
     * Invoke the current modal timeout first, if there is one and
     * it has triggered.
     */

    timerHandlerPtr = firstModalHandlerPtr;
    if (firstModalHandlerPtr != NULL) {
	if ((timerHandlerPtr->time.sec < timerEvPtr->time.sec)
		|| ((timerHandlerPtr->time.sec == timerEvPtr->time.sec)
		&& (timerHandlerPtr->time.usec <= timerEvPtr->time.usec))) {
	    (*timerHandlerPtr->proc)(timerHandlerPtr->clientData);
	}
    }

    /*
     * Invoke any normal timers that have fired.
     */

    if (!(flags & TCL_TIMER_EVENTS)) {
	return 1;
    }

    while (1) {
	timerHandlerPtr = firstTimerHandlerPtr;
	if (timerHandlerPtr == NULL) {
	    break;
	}
	if ((timerHandlerPtr->time.sec > timerEvPtr->time.sec)
		|| ((timerHandlerPtr->time.sec == timerEvPtr->time.sec)
		&& (timerHandlerPtr->time.usec >= timerEvPtr->time.usec))) {
	    break;
	}

	/*
	 * Remove the handler from the queue before invoking it,
	 * to avoid potential reentrancy problems.
	 */

	firstTimerHandlerPtr = timerHandlerPtr->nextPtr;
	(*timerHandlerPtr->proc)(timerHandlerPtr->clientData);
	ckfree((char *) timerHandlerPtr);
    }
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DoWhenIdle --
 *
 *	Arrange for proc to be invoked the next time the system is
 *	idle (i.e., just before the next time that Tcl_DoOneEvent
 *	would have to wait for something to happen).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc will eventually be called, with clientData as argument.
 *	See the manual entry for details.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DoWhenIdle(proc, clientData)
    Tcl_IdleProc *proc;		/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr;

    idlePtr = (IdleHandler *) ckalloc(sizeof(IdleHandler));
    idlePtr->proc = proc;
    idlePtr->clientData = clientData;
    idlePtr->generation = idleGeneration;
    idlePtr->nextPtr = NULL;
    if (lastIdlePtr == NULL) {
	idleList = idlePtr;
    } else {
	lastIdlePtr->nextPtr = idlePtr;
    }
    lastIdlePtr = idlePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CancelIdleCall --
 *
 *	If there are any when-idle calls requested to a given procedure
 *	with given clientData, cancel all of them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the proc/clientData combination were on the when-idle list,
 *	they are removed so that they will never be called.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CancelIdleCall(proc, clientData)
    Tcl_IdleProc *proc;		/* Procedure that was previously registered. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr, *prevPtr;
    IdleHandler *nextPtr;

    for (prevPtr = NULL, idlePtr = idleList; idlePtr != NULL;
	    prevPtr = idlePtr, idlePtr = idlePtr->nextPtr) {
	while ((idlePtr->proc == proc)
		&& (idlePtr->clientData == clientData)) {
	    nextPtr = idlePtr->nextPtr;
	    ckfree((char *) idlePtr);
	    idlePtr = nextPtr;
	    if (prevPtr == NULL) {
		idleList = idlePtr;
	    } else {
		prevPtr->nextPtr = idlePtr;
	    }
	    if (idlePtr == NULL) {
		lastIdlePtr = prevPtr;
		return;
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclIdlePending --
 *
 *	This function is called by the notifier subsystem to determine
 *	whether there are any idle handlers currently scheduled.
 *
 * Results:
 *	Returns 0 if the idle list is empty, otherwise it returns 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclIdlePending()
{
    return (idleList == NULL) ? 0 : 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclServiceIdle --
 *
 *	This procedure is invoked by the notifier when it becomes idle.
 *
 * Results:
 *	The return value is 1 if the procedure actually found an idle
 *	handler to invoke.  If no handler was found then 0 is returned.
 *
 * Side effects:
 *	Invokes all pending idle handlers.
 *
 *----------------------------------------------------------------------
 */

int
TclServiceIdle()
{
    IdleHandler *idlePtr;
    int oldGeneration;
    int foundIdle;

    if (idleList == NULL) {
	return 0;
    }
    
    foundIdle = 0;
    oldGeneration = idleGeneration;
    idleGeneration++;

    /*
     * The code below is trickier than it may look, for the following
     * reasons:
     *
     * 1. New handlers can get added to the list while the current
     *    one is being processed.  If new ones get added, we don't
     *    want to process them during this pass through the list (want
     *    to check for other work to do first).  This is implemented
     *    using the generation number in the handler:  new handlers
     *    will have a different generation than any of the ones currently
     *    on the list.
     * 2. The handler can call Tcl_DoOneEvent, so we have to remove
     *    the handler from the list before calling it. Otherwise an
     *    infinite loop could result.
     * 3. Tcl_CancelIdleCall can be called to remove an element from
     *    the list while a handler is executing, so the list could
     *    change structure during the call.
     */

    for (idlePtr = idleList;
	    ((idlePtr != NULL)
		    && ((oldGeneration - idlePtr->generation) >= 0));
	    idlePtr = idleList) {
	idleList = idlePtr->nextPtr;
	if (idleList == NULL) {
	    lastIdlePtr = NULL;
	}
	foundIdle = 1;
	(*idlePtr->proc)(idlePtr->clientData);
	ckfree((char *) idlePtr);
    }

    return foundIdle;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BackgroundError --
 *
 *	This procedure is invoked to handle errors that occur in Tcl
 *	commands that are invoked in "background" (e.g. from event or
 *	timer bindings).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The command "bgerror" is invoked later as an idle handler to
 *	process the error, passing it the error message.  If that fails,
 *	then an error message is output on stderr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_BackgroundError(interp)
    Tcl_Interp *interp;		/* Interpreter in which an error has
				 * occurred. */
{
    BgError *errPtr;
    char *varValue;
    ErrAssocData *assocPtr;

    /*
     * The Tcl_AddErrorInfo call below (with an empty string) ensures that
     * errorInfo gets properly set.  It's needed in cases where the error
     * came from a utility procedure like Tcl_GetVar instead of Tcl_Eval;
     * in these cases errorInfo still won't have been set when this
     * procedure is called.
     */

    Tcl_AddErrorInfo(interp, "");
    errPtr = (BgError *) ckalloc(sizeof(BgError));
    errPtr->interp = interp;
    errPtr->errorMsg = (char *) ckalloc((unsigned) (strlen(interp->result)
	    + 1));
    strcpy(errPtr->errorMsg, interp->result);
    varValue = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
    if (varValue == NULL) {
	varValue = errPtr->errorMsg;
    }
    errPtr->errorInfo = (char *) ckalloc((unsigned) (strlen(varValue) + 1));
    strcpy(errPtr->errorInfo, varValue);
    varValue = Tcl_GetVar(interp, "errorCode", TCL_GLOBAL_ONLY);
    if (varValue == NULL) {
	varValue = "";
    }
    errPtr->errorCode = (char *) ckalloc((unsigned) (strlen(varValue) + 1));
    strcpy(errPtr->errorCode, varValue);
    errPtr->nextPtr = NULL;

    assocPtr = (ErrAssocData *) Tcl_GetAssocData(interp, "tclBgError",
	    (Tcl_InterpDeleteProc **) NULL);
    if (assocPtr == NULL) {

	/*
	 * This is the first time a background error has occurred in
	 * this interpreter.  Create associated data to keep track of
	 * pending error reports.
	 */

	assocPtr = (ErrAssocData *) ckalloc(sizeof(ErrAssocData));
	assocPtr->firstBgPtr = NULL;
	assocPtr->lastBgPtr = NULL;
	Tcl_SetAssocData(interp, "tclBgError", BgErrorDeleteProc,
		(ClientData) assocPtr);
    }
    if (assocPtr->firstBgPtr == NULL) {
	assocPtr->firstBgPtr = errPtr;
	Tcl_DoWhenIdle(HandleBgErrors, (ClientData) assocPtr);
    } else {
	assocPtr->lastBgPtr->nextPtr = errPtr;
    }
    assocPtr->lastBgPtr = errPtr;
    Tcl_ResetResult(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * HandleBgErrors --
 *
 *	This procedure is invoked as an idle handler to process all of
 *	the accumulated background errors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what actions "bgerror" takes for the errors.
 *
 *----------------------------------------------------------------------
 */

static void
HandleBgErrors(clientData)
    ClientData clientData;	/* Pointer to ErrAssocData structure. */
{
    Tcl_Interp *interp;
    char *command;
    char *argv[2];
    int code;
    BgError *errPtr;
    ErrAssocData *assocPtr = (ErrAssocData *) clientData;
    Tcl_Channel errChannel;

    while (assocPtr->firstBgPtr != NULL) {
	interp = assocPtr->firstBgPtr->interp;
	if (interp == NULL) {
	    goto doneWithReport;
	}

	/*
	 * Restore important state variables to what they were at
	 * the time the error occurred.
	 */

	Tcl_SetVar(interp, "errorInfo", assocPtr->firstBgPtr->errorInfo,
		TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "errorCode", assocPtr->firstBgPtr->errorCode,
		TCL_GLOBAL_ONLY);

	/*
	 * Create and invoke the bgerror command.
	 */

	argv[0] = "bgerror";
	argv[1] = assocPtr->firstBgPtr->errorMsg;
	command = Tcl_Merge(2, argv);
	Tcl_AllowExceptions(interp);
        Tcl_Preserve((ClientData) interp);
	code = Tcl_GlobalEval(interp, command);
	ckfree(command);
	if (code == TCL_ERROR) {

            /*
             * We have to get the error output channel at the latest possible
             * time, because the eval (above) might have changed the channel.
             */
            
            errChannel = Tcl_GetStdChannel(TCL_STDERR);
            if (errChannel != (Tcl_Channel) NULL) {
                if (strcmp(interp->result,
           "\"bgerror\" is an invalid command name or ambiguous abbreviation")
                        == 0) {
                    Tcl_Write(errChannel, assocPtr->firstBgPtr->errorInfo, -1);
                    Tcl_Write(errChannel, "\n", -1);
                } else {
                    Tcl_Write(errChannel,
                            "bgerror failed to handle background error.\n",
                            -1);
                    Tcl_Write(errChannel, "    Original error: ", -1);
                    Tcl_Write(errChannel, assocPtr->firstBgPtr->errorMsg,
                            -1);
                    Tcl_Write(errChannel, "\n", -1);
                    Tcl_Write(errChannel, "    Error in bgerror: ", -1);
                    Tcl_Write(errChannel, interp->result, -1);
                    Tcl_Write(errChannel, "\n", -1);
                }
                Tcl_Flush(errChannel);
            }
	} else if (code == TCL_BREAK) {

	    /*
	     * Break means cancel any remaining error reports for this
	     * interpreter.
	     */

	    for (errPtr = assocPtr->firstBgPtr; errPtr != NULL;
		    errPtr = errPtr->nextPtr) {
		if (errPtr->interp == interp) {
		    errPtr->interp = NULL;
		}
	    }
	}

        Tcl_Release((ClientData) interp);

	/*
	 * Discard the command and the information about the error report.
	 */

	doneWithReport:
	ckfree(assocPtr->firstBgPtr->errorMsg);
	ckfree(assocPtr->firstBgPtr->errorInfo);
	ckfree(assocPtr->firstBgPtr->errorCode);
	errPtr = assocPtr->firstBgPtr->nextPtr;
	ckfree((char *) assocPtr->firstBgPtr);
	assocPtr->firstBgPtr = errPtr;
    }
    assocPtr->lastBgPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * BgErrorDeleteProc --
 *
 *	This procedure is associated with the "tclBgError" assoc data
 *	for an interpreter;  it is invoked when the interpreter is
 *	deleted in order to free the information assoicated with any
 *	pending error reports.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Background error information is freed: if there were any
 *	pending error reports, they are cancelled.
 *
 *----------------------------------------------------------------------
 */

static void
BgErrorDeleteProc(clientData, interp)
    ClientData clientData;	/* Pointer to ErrAssocData structure. */
    Tcl_Interp *interp;		/* Interpreter being deleted. */
{
    ErrAssocData *assocPtr = (ErrAssocData *) clientData;
    BgError *errPtr;

    while (assocPtr->firstBgPtr != NULL) {
	errPtr = assocPtr->firstBgPtr;
	assocPtr->firstBgPtr = errPtr->nextPtr;
	ckfree(errPtr->errorMsg);
	ckfree(errPtr->errorInfo);
	ckfree(errPtr->errorCode);
	ckfree((char *) errPtr);
    }
    ckfree((char *) assocPtr);
    Tcl_CancelIdleCall(HandleBgErrors, (ClientData) assocPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateExitHandler --
 *
 *	Arrange for a given procedure to be invoked just before the
 *	application exits.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc will be invoked with clientData as argument when the
 *	application exits.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateExitHandler(proc, clientData)
    Tcl_ExitProc *proc;		/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    ExitHandler *exitPtr;

    exitPtr = (ExitHandler *) ckalloc(sizeof(ExitHandler));
    exitPtr->proc = proc;
    exitPtr->clientData = clientData;
    exitPtr->nextPtr = firstExitPtr;
    firstExitPtr = exitPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteExitHandler --
 *
 *	This procedure cancels an existing exit handler matching proc
 *	and clientData, if such a handler exits.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there is an exit handler corresponding to proc and clientData
 *	then it is cancelled;  if no such handler exists then nothing
 *	happens.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteExitHandler(proc, clientData)
    Tcl_ExitProc *proc;		/* Procedure that was previously registered. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    ExitHandler *exitPtr, *prevPtr;

    for (prevPtr = NULL, exitPtr = firstExitPtr; exitPtr != NULL;
	    prevPtr = exitPtr, exitPtr = exitPtr->nextPtr) {
	if ((exitPtr->proc == proc)
		&& (exitPtr->clientData == clientData)) {
	    if (prevPtr == NULL) {
		firstExitPtr = exitPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = exitPtr->nextPtr;
	    }
	    ckfree((char *) exitPtr);
	    return;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Exit --
 *
 *	This procedure is called to terminate the application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All existing exit handlers are invoked, then the application
 *	ends.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Exit(status)
    int status;			/* Exit status for application;  typically
				 * 0 for normal return, 1 for error return. */
{
    ExitHandler *exitPtr;

    for (exitPtr = firstExitPtr; exitPtr != NULL; exitPtr = firstExitPtr) {
	/*
	 * Be careful to remove the handler from the list before invoking
	 * its callback.  This protects us against double-freeing if the
	 * callback should call Tcl_DeleteExitHandler on itself.
	 */

	firstExitPtr = exitPtr->nextPtr;
	(*exitPtr->proc)(exitPtr->clientData);
	ckfree((char *) exitPtr);
    }
#ifdef TCL_MEM_DEBUG
    if (tclMemDumpFileName != NULL) {
	Tcl_DumpActiveMemory(tclMemDumpFileName);
    }
#endif
    
    TclPlatformExit(status);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AfterCmd --
 *
 *	This procedure is invoked to process the "after" Tcl command.
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
Tcl_AfterCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Points to the "tclAfter" assocData for
				 * this interpreter, or NULL if the assocData
				 * hasn't been created yet.*/
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    /*
     * The variable below is used to generate unique identifiers for
     * after commands.  This id can wrap around, which can potentially
     * cause problems.  However, there are not likely to be problems
     * in practice, because after commands can only be requested to
     * about a month in the future, and wrap-around is unlikely to
     * occur in less than about 1-10 years.  Thus it's unlikely that
     * any old ids will still be around when wrap-around occurs.
     */

    static int nextId = 1;
    int ms;
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr = (AfterAssocData *) clientData;
    Tcl_CmdInfo cmdInfo;
    size_t length;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create the "after" information associated for this interpreter,
     * if it doesn't already exist.  Associate it with the command too,
     * so that it will be passed in as the ClientData argument in the
     * future.
     */

    if (assocPtr == NULL) {
	assocPtr = (AfterAssocData *) ckalloc(sizeof(AfterAssocData));
	assocPtr->interp = interp;
	assocPtr->firstAfterPtr = NULL;
	Tcl_SetAssocData(interp, "tclAfter", AfterCleanupProc,
		(ClientData) assocPtr);
	cmdInfo.proc = Tcl_AfterCmd;
	cmdInfo.clientData = (ClientData) assocPtr;
	cmdInfo.deleteProc = NULL;
	cmdInfo.deleteData = (ClientData) assocPtr;
	Tcl_SetCommandInfo(interp, argv[0], &cmdInfo);
    }

    /*
     * Parse the command.
     */

    length = strlen(argv[1]);
    if (isdigit(UCHAR(argv[1][0]))) {
	if (Tcl_GetInt(interp, argv[1], &ms) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (ms < 0) {
	    ms = 0;
	}
	if (argc == 2) {
	    Tcl_Sleep(ms);
	    return TCL_OK;
	}
	afterPtr = (AfterInfo *) ckalloc((unsigned) (sizeof(AfterInfo)));
	afterPtr->assocPtr = assocPtr;
	if (argc == 3) {
	    afterPtr->command = (char *) ckalloc((unsigned)
		    (strlen(argv[2]) + 1));
	    strcpy(afterPtr->command, argv[2]);
	} else {
	    afterPtr->command = Tcl_Concat(argc-2, argv+2);
	}
	afterPtr->id = nextId;
	nextId += 1;
	afterPtr->token = Tcl_CreateTimerHandler(ms, AfterProc,
		(ClientData) afterPtr);
	afterPtr->nextPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr;
	sprintf(interp->result, "after#%d", afterPtr->id);
    } else if (strncmp(argv[1], "cancel", length) == 0) {
	char *arg;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cancel id|command\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    arg = argv[2];
	} else {
	    arg = Tcl_Concat(argc-2, argv+2);
	}
	for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
		afterPtr = afterPtr->nextPtr) {
	    if (strcmp(afterPtr->command, arg) == 0) {
		break;
	    }
	}
	if (afterPtr == NULL) {
	    afterPtr = GetAfterEvent(assocPtr, arg);
	}
	if (arg != argv[2]) {
	    ckfree(arg);
	}
	if (afterPtr != NULL) {
	    if (afterPtr->token != NULL) {
		Tcl_DeleteTimerHandler(afterPtr->token);
	    } else {
		Tcl_CancelIdleCall(AfterProc, (ClientData) afterPtr);
	    }
	    FreeAfterPtr(afterPtr);
	}
    } else if ((strncmp(argv[1], "idle", length) == 0)
	     && (length >= 2)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " idle script script ...\"", (char *) NULL);
	    return TCL_ERROR;
	}
	afterPtr = (AfterInfo *) ckalloc((unsigned) (sizeof(AfterInfo)));
	afterPtr->assocPtr = assocPtr;
	if (argc == 3) {
	    afterPtr->command = (char *) ckalloc((unsigned)
		    (strlen(argv[2]) + 1));
	    strcpy(afterPtr->command, argv[2]);
	} else {
	    afterPtr->command = Tcl_Concat(argc-2, argv+2);
	}
	afterPtr->id = nextId;
	nextId += 1;
	afterPtr->token = NULL;
	afterPtr->nextPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr;
	Tcl_DoWhenIdle(AfterProc, (ClientData) afterPtr);
	sprintf(interp->result, "after#%d", afterPtr->id);
    } else if ((strncmp(argv[1], "info", length) == 0)
	     && (length >= 2)) {
	if (argc == 2) {
	    char buffer[30];
	    
	    for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
		    afterPtr = afterPtr->nextPtr) {
		if (assocPtr->interp == interp) {
		    sprintf(buffer, "after#%d", afterPtr->id);
		    Tcl_AppendElement(interp, buffer);
		}
	    }
	    return TCL_OK;
	}
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " info ?id?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	afterPtr = GetAfterEvent(assocPtr, argv[2]);
	if (afterPtr == NULL) {
	    Tcl_AppendResult(interp, "event \"", argv[2],
		    "\" doesn't exist", (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, afterPtr->command);
	Tcl_AppendElement(interp,
		(afterPtr->token == NULL) ? "idle" : "timer");
    } else {
	Tcl_AppendResult(interp, "bad argument \"", argv[1],
		"\": must be cancel, idle, info, or a number",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAfterEvent --
 *
 *	This procedure parses an "after" id such as "after#4" and
 *	returns a pointer to the AfterInfo structure.
 *
 * Results:
 *	The return value is either a pointer to an AfterInfo structure,
 *	if one is found that corresponds to "string" and is for interp,
 *	or NULL if no corresponding after event can be found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static AfterInfo *
GetAfterEvent(assocPtr, string)
    AfterAssocData *assocPtr;	/* Points to "after"-related information for
				 * this interpreter. */
    char *string;		/* Textual identifier for after event, such
				 * as "after#6". */
{
    AfterInfo *afterPtr;
    int id;
    char *end;

    if (strncmp(string, "after#", 6) != 0) {
	return NULL;
    }
    string += 6;
    id = strtoul(string, &end, 10);
    if ((end == string) || (*end != 0)) {
	return NULL;
    }
    for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
	    afterPtr = afterPtr->nextPtr) {
	if (afterPtr->id == id) {
	    return afterPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * AfterProc --
 *
 *	Timer callback to execute commands registered with the
 *	"after" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Executes whatever command was specified.  If the command
 *	returns an error, then the command "bgerror" is invoked
 *	to process the error;  if bgerror fails then information
 *	about the error is output on stderr.
 *
 *----------------------------------------------------------------------
 */

static void
AfterProc(clientData)
    ClientData clientData;	/* Describes command to execute. */
{
    AfterInfo *afterPtr = (AfterInfo *) clientData;
    AfterAssocData *assocPtr = afterPtr->assocPtr;
    AfterInfo *prevPtr;
    int result;
    Tcl_Interp *interp;

    /*
     * First remove the callback from our list of callbacks;  otherwise
     * someone could delete the callback while it's being executed, which
     * could cause a core dump.
     */

    if (assocPtr->firstAfterPtr == afterPtr) {
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
    } else {
	for (prevPtr = assocPtr->firstAfterPtr; prevPtr->nextPtr != afterPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = afterPtr->nextPtr;
    }

    /*
     * Execute the callback.
     */

    interp = assocPtr->interp;
    Tcl_Preserve((ClientData) interp);
    result = Tcl_GlobalEval(interp, afterPtr->command);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (\"after\" script)");
	Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
    
    /*
     * Free the memory for the callback.
     */

    ckfree(afterPtr->command);
    ckfree((char *) afterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeAfterPtr --
 *
 *	This procedure removes an "after" command from the list of
 *	those that are pending and frees its resources.  This procedure
 *	does *not* cancel the timer handler;  if that's needed, the
 *	caller must do it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The memory associated with afterPtr is released.
 *
 *----------------------------------------------------------------------
 */

static void
FreeAfterPtr(afterPtr)
    AfterInfo *afterPtr;		/* Command to be deleted. */
{
    AfterInfo *prevPtr;
    AfterAssocData *assocPtr = afterPtr->assocPtr;

    if (assocPtr->firstAfterPtr == afterPtr) {
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
    } else {
	for (prevPtr = assocPtr->firstAfterPtr; prevPtr->nextPtr != afterPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = afterPtr->nextPtr;
    }
    ckfree(afterPtr->command);
    ckfree((char *) afterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AfterCleanupProc --
 *
 *	This procedure is invoked whenever an interpreter is deleted
 *	to cleanup the AssocData for "tclAfter".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After commands are removed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
AfterCleanupProc(clientData, interp)
    ClientData clientData;	/* Points to AfterAssocData for the
				 * interpreter. */
    Tcl_Interp *interp;		/* Interpreter that is being deleted. */
{
    AfterAssocData *assocPtr = (AfterAssocData *) clientData;
    AfterInfo *afterPtr;

    while (assocPtr->firstAfterPtr != NULL) {
	afterPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
	if (afterPtr->token != NULL) {
	    Tcl_DeleteTimerHandler(afterPtr->token);
	} else {
	    Tcl_CancelIdleCall(AfterProc, (ClientData) afterPtr);
	}
	ckfree(afterPtr->command);
	ckfree((char *) afterPtr);
    }
    ckfree((char *) assocPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VwaitCmd --
 *
 *	This procedure is invoked to process the "vwait" Tcl command.
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
Tcl_VwaitCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int done, foundEvent;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " name\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_TraceVar(interp, argv[1],
	    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    VwaitVarProc, (ClientData) &done);
    done = 0;
    foundEvent = 1;
    while (!done && foundEvent) {
	foundEvent = Tcl_DoOneEvent(0);
    }
    Tcl_UntraceVar(interp, argv[1],
	    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    VwaitVarProc, (ClientData) &done);

    /*
     * Clear out the interpreter's result, since it may have been set
     * by event handlers.
     */

    Tcl_ResetResult(interp);
    if (!foundEvent) {
	Tcl_AppendResult(interp, "can't wait for variable \"", argv[1],
		"\":  would wait forever", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

	/* ARGSUSED */
static char *
VwaitVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpdateCmd --
 *
 *	This procedure is invoked to process the "update" Tcl command.
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
Tcl_UpdateCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int flags = 0;		/* Initialization needed only to stop
				 * compiler warnings. */

    if (argc == 1) {
	flags = TCL_ALL_EVENTS|TCL_DONT_WAIT;
    } else if (argc == 2) {
	if (strncmp(argv[1], "idletasks", strlen(argv[1])) != 0) {
	    Tcl_AppendResult(interp, "bad option \"", argv[1],
		    "\": must be idletasks", (char *) NULL);
	    return TCL_ERROR;
	}
	flags = TCL_IDLE_EVENTS|TCL_DONT_WAIT;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ?idletasks?\"", (char *) NULL);
	return TCL_ERROR;
    }

    while (Tcl_DoOneEvent(flags) != 0) {
	/* Empty loop body */
    }

    /*
     * Must clear the interpreter's result because event handlers could
     * have executed commands.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWaitForFile --
 *
 *	This procedure waits synchronously for a file to become readable
 *	or writable, with an optional timeout.
 *
 * Results:
 *	The return value is an OR'ed combination of TCL_READABLE,
 *	TCL_WRITABLE, and TCL_EXCEPTION, indicating the conditions
 *	that are present on file at the time of the return.  This
 *	procedure will not return until either "timeout" milliseconds
 *	have elapsed or at least one of the conditions given by mask
 *	has occurred for file (a return value of 0 means that a timeout
 *	occurred).  No normal events will be serviced during the
 *	execution of this procedure.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

int
TclWaitForFile(file, mask, timeout)
    Tcl_File file;		/* Handle for file on which to wait. */
    int mask;			/* What to wait for: OR'ed combination of
				 * TCL_READABLE, TCL_WRITABLE, and
				 * TCL_EXCEPTION. */
    int timeout;		/* Maximum amount of time to wait for one
				 * of the conditions in mask to occur, in
				 * milliseconds.  A value of 0 means don't
				 * wait at all, and a value of -1 means
				 * wait forever. */
{
    Tcl_Time abortTime, now, blockTime;
    int present;

    /*
     * If there is a non-zero finite timeout, compute the time when
     * we give up.
     */

    if (timeout > 0) {
	TclpGetTime(&now);
	abortTime.sec = now.sec + timeout/1000;
	abortTime.usec = now.usec + (timeout%1000)*1000;
	if (abortTime.usec >= 1000000) {
	    abortTime.usec -= 1000000;
	    abortTime.sec += 1;
	}
    }

    /*
     * Loop in a mini-event loop of our own, waiting for either the
     * file to become ready or a timeout to occur.
     */

    while (1) {
	Tcl_WatchFile(file, mask);
	if (timeout > 0) {
	    blockTime.sec = abortTime.sec - now.sec;
	    blockTime.usec = abortTime.usec - now.usec;
	    if (blockTime.usec < 0) {
		blockTime.sec -= 1;
		blockTime.usec += 1000000;
	    }
	    if (blockTime.sec < 0) {
		blockTime.sec = 0;
		blockTime.usec = 0;
	    }
	    Tcl_WaitForEvent(&blockTime);
	} else if (timeout == 0) {
	    blockTime.sec = 0;
	    blockTime.usec = 0;
	    Tcl_WaitForEvent(&blockTime);
	} else {
	    Tcl_WaitForEvent((Tcl_Time *) NULL);
	}
	present = Tcl_FileReady(file, mask);
	if (present != 0) {
	    break;
	}
	if (timeout == 0) {
	    break;
	}
	TclpGetTime(&now);
	if ((abortTime.sec < now.sec)
		|| ((abortTime.sec == now.sec)
		&& (abortTime.usec <= now.usec))) {
	    break;
	}
    }
    return present;
}
