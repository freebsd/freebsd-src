/* 
 * tclNotify.c --
 *
 *	This file provides the parts of the Tcl event notifier that are
 *	the same on all platforms, plus a few other parts that are used
 *	on more than one platform but not all.
 *
 *	The notifier is the lowest-level part of the event system.  It
 *	manages an event queue that holds Tcl_Event structures and a list
 *	of event sources that can add events to the queue.  It also
 *	contains the procedure Tcl_DoOneEvent that invokes the event
 *	sources and blocks to wait for new events, but Tcl_DoOneEvent
 *	is in the platform-specific part of the notifier (in files like
 *	tclUnixNotify.c).
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclNotify.c 1.6 96/02/29 09:20:10
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following variable records the address of the first event
 * source in the list of all event sources for the application.
 * This variable is accessed by the notifier to traverse the list
 * and invoke each event source.
 */

TclEventSource *tclFirstEventSourcePtr = NULL;

/*
 * The following variables indicate how long to block in the event
 * notifier the next time it blocks (default:  block forever).
 */

static int blockTimeSet = 0;	/* 0 means there is no maximum block
				 * time:  block forever. */
static Tcl_Time blockTime;	/* If blockTimeSet is 1, gives the
				 * maximum elapsed time for the next block. */

/*
 * The following variables keep track of the event queue.  In addition
 * to the first (next to be serviced) and last events in the queue,
 * we keep track of a "marker" event.  This provides a simple priority
 * mechanism whereby events can be inserted at the front of the queue
 * but behind all other high-priority events already in the queue (this
 * is used for things like a sequence of Enter and Leave events generated
 * during a grab in Tk).
 */

static Tcl_Event *firstEventPtr = NULL;
				/* First pending event, or NULL if none. */
static Tcl_Event *lastEventPtr = NULL;
				/* Last pending event, or NULL if none. */
static Tcl_Event *markerEventPtr = NULL;
				/* Last high-priority event in queue, or
				 * NULL if none. */

/*
 * Prototypes for procedures used only in this file:
 */

static int		ServiceEvent _ANSI_ARGS_((int flags));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateEventSource --
 *
 *	This procedure is invoked to create a new source of events.
 *	The source is identified by a procedure that gets invoked
 *	during Tcl_DoOneEvent to check for events on that source
 *	and queue them.
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	SetupProc and checkProc will be invoked each time that Tcl_DoOneEvent
 *	runs out of things to do.  SetupProc will be invoked before
 *	Tcl_DoOneEvent calls select or whatever else it uses to wait
 *	for events.  SetupProc typically calls functions like Tcl_WatchFile
 *	or Tcl_SetMaxBlockTime to indicate what to wait for.
 *
 *	CheckProc is called after select or whatever operation was actually
 *	used to wait.  It figures out whether anything interesting actually
 *	happened (e.g. by calling Tcl_FileReady), and then calls
 *	Tcl_QueueEvent to queue any events that are ready.
 *
 *	Each of these procedures is passed two arguments, e.g.
 *		(*checkProc)(ClientData clientData, int flags));
 *	ClientData is the same as the clientData argument here, and flags
 *	is a combination of things like TCL_FILE_EVENTS that indicates
 *	what events are of interest:  setupProc and checkProc use flags
 *	to figure out whether their events are relevant or not.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateEventSource(setupProc, checkProc, clientData)
    Tcl_EventSetupProc *setupProc;	/* Procedure to invoke to figure out
					 * what to wait for. */
    Tcl_EventCheckProc *checkProc;	/* Procedure to call after waiting
					 * to see what happened. */
    ClientData clientData;		/* One-word argument to pass to
					 * setupProc and checkProc. */
{
    TclEventSource *sourcePtr;

    sourcePtr = (TclEventSource *) ckalloc(sizeof(TclEventSource));
    sourcePtr->setupProc = setupProc;
    sourcePtr->checkProc = checkProc;
    sourcePtr->clientData = clientData;
    sourcePtr->nextPtr = tclFirstEventSourcePtr;
    tclFirstEventSourcePtr = sourcePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteEventSource --
 *
 *	This procedure is invoked to delete the source of events
 *	given by proc and clientData.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given event source is cancelled, so its procedure will
 *	never again be called.  If no such source exists, nothing
 *	happens.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteEventSource(setupProc, checkProc, clientData)
    Tcl_EventSetupProc *setupProc;	/* Procedure to invoke to figure out
					 * what to wait for. */
    Tcl_EventCheckProc *checkProc;	/* Procedure to call after waiting
					 * to see what happened. */
    ClientData clientData;		/* One-word argument to pass to
					 * setupProc and checkProc. */
{
    TclEventSource *sourcePtr, *prevPtr;

    for (sourcePtr = tclFirstEventSourcePtr, prevPtr = NULL;
	    sourcePtr != NULL;
	    prevPtr = sourcePtr, sourcePtr = sourcePtr->nextPtr) {
	if ((sourcePtr->setupProc != setupProc)
		|| (sourcePtr->checkProc != checkProc)
		|| (sourcePtr->clientData != clientData)) {
	    continue;
	}
	if (prevPtr == NULL) {
	    tclFirstEventSourcePtr = sourcePtr->nextPtr;
	} else {
	    prevPtr->nextPtr = sourcePtr->nextPtr;
	}
	ckfree((char *) sourcePtr);
	return;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_QueueEvent --
 *
 *	Insert an event into the Tk event queue at one of three
 *	positions: the head, the tail, or before a floating marker.
 *	Events inserted before the marker will be processed in
 *	first-in-first-out order, but before any events inserted at
 *	the tail of the queue.  Events inserted at the head of the
 *	queue will be processed in last-in-first-out order.
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
Tcl_QueueEvent(evPtr, position)
    Tcl_Event* evPtr;		/* Event to add to queue.  The storage
				 * space must have been allocated the caller
				 * with malloc (ckalloc), and it becomes
				 * the property of the event queue.  It
				 * will be freed after the event has been
				 * handled. */
    Tcl_QueuePosition position;	/* One of TCL_QUEUE_TAIL, TCL_QUEUE_HEAD,
				 * TCL_QUEUE_MARK. */
{
    if (position == TCL_QUEUE_TAIL) {
	/*
	 * Append the event on the end of the queue.
	 */

	evPtr->nextPtr = NULL;
	if (firstEventPtr == NULL) {
	    firstEventPtr = evPtr;
	} else {
	    lastEventPtr->nextPtr = evPtr;
	}
	lastEventPtr = evPtr;
    } else if (position == TCL_QUEUE_HEAD) {
	/*
	 * Push the event on the head of the queue.
	 */

	evPtr->nextPtr = firstEventPtr;
	if (firstEventPtr == NULL) {
	    lastEventPtr = evPtr;
	}	    
	firstEventPtr = evPtr;
    } else if (position == TCL_QUEUE_MARK) {
	/*
	 * Insert the event after the current marker event and advance
	 * the marker to the new event.
	 */

	if (markerEventPtr == NULL) {
	    evPtr->nextPtr = firstEventPtr;
	    firstEventPtr = evPtr;
	} else {
	    evPtr->nextPtr = markerEventPtr->nextPtr;
	    markerEventPtr->nextPtr = evPtr;
	}
	markerEventPtr = evPtr;
	if (evPtr->nextPtr == NULL) {
	    lastEventPtr = evPtr;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteEvents --
 *
 *	Calls a procedure for each event in the queue and deletes those
 *	for which the procedure returns 1. Events for which the
 *	procedure returns 0 are left in the queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Potentially removes one or more events from the event queue.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteEvents(proc, clientData)
    Tcl_EventDeleteProc *proc;		/* The procedure to call. */
    ClientData clientData;    		/* type-specific data. */
{
    Tcl_Event *evPtr, *prevPtr, *hold;

    for (prevPtr = (Tcl_Event *) NULL, evPtr = firstEventPtr;
             evPtr != (Tcl_Event *) NULL;
             ) {
        if ((*proc) (evPtr, clientData) == 1) {
            if (firstEventPtr == evPtr) {
                firstEventPtr = evPtr->nextPtr;
                if (evPtr->nextPtr == (Tcl_Event *) NULL) {
                    lastEventPtr = (Tcl_Event *) NULL;
                }
            } else {
                prevPtr->nextPtr = evPtr->nextPtr;
            }
            hold = evPtr;
            evPtr = evPtr->nextPtr;
            ckfree((char *) hold);
        } else {
            prevPtr = evPtr;
            evPtr = evPtr->nextPtr;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ServiceEvent --
 *
 *	Process one event from the event queue.  This routine is called
 *	by the notifier whenever it wants Tk to process an event.  
 *
 * Results:
 *	The return value is 1 if the procedure actually found an event
 *	to process.  If no processing occurred, then 0 is returned.
 *
 * Side effects:
 *	Invokes all of the event handlers for the highest priority
 *	event in the event queue.  May collapse some events into a
 *	single event or discard stale events.
 *
 *----------------------------------------------------------------------
 */

static int
ServiceEvent(flags)
    int flags;			/* Indicates what events should be processed.
				 * May be any combination of TCL_WINDOW_EVENTS
				 * TCL_FILE_EVENTS, TCL_TIMER_EVENTS, or other
				 * flags defined elsewhere.  Events not
				 * matching this will be skipped for processing
				 * later. */
{
    Tcl_Event *evPtr, *prevPtr;
    Tcl_EventProc *proc;

    /*
     * No event flags is equivalent to TCL_ALL_EVENTS.
     */
    
    if ((flags & TCL_ALL_EVENTS) == 0) {
	flags |= TCL_ALL_EVENTS;
    }

    /*
     * Loop through all the events in the queue until we find one
     * that can actually be handled.
     */

    for (evPtr = firstEventPtr; evPtr != NULL; evPtr = evPtr->nextPtr) {
	/*
	 * Call the handler for the event.  If it actually handles the
	 * event then free the storage for the event.  There are two
	 * tricky things here, but stemming from the fact that the event
	 * code may be re-entered while servicing the event:
	 *
	 * 1. Set the "proc" field to NULL.  This is a signal to ourselves
	 *    that we shouldn't reexecute the handler if the event loop
	 *    is re-entered.
	 * 2. When freeing the event, must search the queue again from the
	 *    front to find it.  This is because the event queue could
	 *    change almost arbitrarily while handling the event, so we
	 *    can't depend on pointers found now still being valid when
	 *    the handler returns.
	 */

	proc = evPtr->proc;
	evPtr->proc = NULL;
	if ((proc != NULL) && (*proc)(evPtr, flags)) {
	    if (firstEventPtr == evPtr) {
		firstEventPtr = evPtr->nextPtr;
		if (evPtr->nextPtr == NULL) {
		    lastEventPtr = NULL;
		}
	    } else {
		for (prevPtr = firstEventPtr; prevPtr->nextPtr != evPtr;
			prevPtr = prevPtr->nextPtr) {
		    /* Empty loop body. */
		}
		prevPtr->nextPtr = evPtr->nextPtr;
		if (evPtr->nextPtr == NULL) {
		    lastEventPtr = prevPtr;
		}
	    }
	    if (markerEventPtr == evPtr) {
		markerEventPtr = NULL;
	    }
	    ckfree((char *) evPtr);
	    return 1;
	} else {
	    /*
	     * The event wasn't actually handled, so we have to restore
	     * the proc field to allow the event to be attempted again.
	     */

	    evPtr->proc = proc;
	}

	/*
	 * The handler for this event asked to defer it.  Just go on to
	 * the next event.
	 */

	continue;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetMaxBlockTime --
 *
 *	This procedure is invoked by event sources to tell the notifier
 *	how long it may block the next time it blocks.  The timePtr
 *	argument gives a maximum time;  the actual time may be less if
 *	some other event source requested a smaller time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May reduce the length of the next sleep in the notifier.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetMaxBlockTime(timePtr)
    Tcl_Time *timePtr;		/* Specifies a maximum elapsed time for
				 * the next blocking operation in the
				 * event notifier. */
{
    if (!blockTimeSet || (timePtr->sec < blockTime.sec)
	    || ((timePtr->sec == blockTime.sec)
	    && (timePtr->usec < blockTime.usec))) {
	blockTime = *timePtr;
	blockTimeSet = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DoOneEvent --
 *
 *	Process a single event of some sort.  If there's no work to
 *	do, wait for an event to occur, then process it.
 *
 * Results:
 *	The return value is 1 if the procedure actually found an event
 *	to process.  If no processing occurred, then 0 is returned (this
 *	can happen if the TCL_DONT_WAIT flag is set or if there are no
 *	event handlers to wait for in the set specified by flags).
 *
 * Side effects:
 *	May delay execution of process while waiting for an event,
 *	unless TCL_DONT_WAIT is set in the flags argument.  Event
 *	sources are invoked to check for and queue events.  Event
 *	handlers may produce arbitrary side effects.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DoOneEvent(flags)
    int flags;			/* Miscellaneous flag values:  may be any
				 * combination of TCL_DONT_WAIT,
				 * TCL_WINDOW_EVENTS, TCL_FILE_EVENTS,
				 * TCL_TIMER_EVENTS, TCL_IDLE_EVENTS, or
				 * others defined by event sources. */
{
    TclEventSource *sourcePtr;
    Tcl_Time *timePtr;

    /*
     * No event flags is equivalent to TCL_ALL_EVENTS.
     */
    
    if ((flags & TCL_ALL_EVENTS) == 0) {
	flags |= TCL_ALL_EVENTS;
    }

    /*
     * The core of this procedure is an infinite loop, even though
     * we only service one event.  The reason for this is that we
     * might think we have an event ready (e.g. the connection to
     * the server becomes readable), but then we might discover that
     * there's nothing interesting on that connection, so no event
     * was serviced.  Or, the select operation could return prematurely
     * due to a signal.  The easiest thing in both these cases is
     * just to loop back and try again.
     */

    while (1) {

	/*
	 * The first thing we do is to service any asynchronous event
	 * handlers.
	 */
    
	if (Tcl_AsyncReady()) {
	    (void) Tcl_AsyncInvoke((Tcl_Interp *) NULL, 0);
	    return 1;
	}

	/*
	 * If idle events are the only things to service, skip the
	 * main part of the loop and go directly to handle idle
	 * events (i.e. don't wait even if TCL_DONT_WAIT isn't set.
	 */

	if (flags == TCL_IDLE_EVENTS) {
	    flags = TCL_IDLE_EVENTS|TCL_DONT_WAIT;
	    goto idleEvents;
	}

	/*
	 * Ask Tk to service a queued event, if there are any.
	 */

	if (ServiceEvent(flags)) {
	    return 1;
	}

	/*
	 * There are no events already queued.  Invoke all of the
	 * event sources to give them a chance to setup for the wait.
	 */

	blockTimeSet = 0;
	for (sourcePtr = tclFirstEventSourcePtr; sourcePtr != NULL;
		sourcePtr = sourcePtr->nextPtr) {
	    (*sourcePtr->setupProc)(sourcePtr->clientData, flags);
	}
	if ((flags & TCL_DONT_WAIT) ||
		((flags & TCL_IDLE_EVENTS) && TclIdlePending())) {
	    /*
	     * Don't block:  there are idle events waiting, or we don't
	     * care about idle events anyway, or the caller asked us not
	     * to block.
	     */

	    blockTime.sec = 0;
	    blockTime.usec = 0;
	    timePtr = &blockTime;
	} else if (blockTimeSet) {
	    timePtr = &blockTime;
	} else {
	    timePtr = NULL;
	}

	/*
	 * Wait until an event occurs or the timer expires.
	 */

	if (Tcl_WaitForEvent(timePtr) == TCL_ERROR) {
	    return 0;
	}

	/*
	 * Give each of the event sources a chance to queue events,
	 * then call ServiceEvent and give it another chance to
	 * service events.
	 */

	for (sourcePtr = tclFirstEventSourcePtr; sourcePtr != NULL;
		sourcePtr = sourcePtr->nextPtr) {
	    (*sourcePtr->checkProc)(sourcePtr->clientData, flags);
	}
	if (ServiceEvent(flags)) {
	    return 1;
	}

	/*
	 * We've tried everything at this point, but nobody had anything
	 * to do.  Check for idle events.  If none, either quit or go back
	 * to the top and try again.
	 */

	idleEvents:
	if ((flags & TCL_IDLE_EVENTS) && TclServiceIdle()) {
	    return 1;
	}
	if (flags & TCL_DONT_WAIT) {
	    return 0;
	}
    }
}
