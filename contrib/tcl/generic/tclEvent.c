/* 
 * tclEvent.c --
 *
 *	This file implements some general event related interfaces including
 *	background errors, exit handlers, and the "vwait" and "update"
 *	command procedures. 
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclEvent.c 1.152 97/05/21 07:06:19
 */

#include "tclInt.h"
#include "tclPort.h"

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
 * The following variable is a "secret" indication to Tcl_Exit that
 * it should dump out the state of memory before exiting.  If the
 * value is non-NULL, it gives the name of the file in which to
 * dump memory usage information.
 */

char *tclMemDumpFileName = NULL;

/*
 * This variable is set to 1 when Tcl_Exit is called, and at the end of
 * its work, it is reset to 0. The variable is checked by TclInExit() to
 * allow different behavior for exit-time processing, e.g. in closing of
 * files and pipes.
 */

static int tclInExit = 0;

/*
 * Prototypes for procedures referenced only in this file:
 */

static void		BgErrorDeleteProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static void		HandleBgErrors _ANSI_ARGS_((ClientData clientData));
static char *		VwaitVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));

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
    char *errResult, *varValue;
    ErrAssocData *assocPtr;

    /*
     * The Tcl_AddErrorInfo call below (with an empty string) ensures that
     * errorInfo gets properly set.  It's needed in cases where the error
     * came from a utility procedure like Tcl_GetVar instead of Tcl_Eval;
     * in these cases errorInfo still won't have been set when this
     * procedure is called.
     */

    Tcl_AddErrorInfo(interp, "");

    errResult = TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL);
	
    errPtr = (BgError *) ckalloc(sizeof(BgError));
    errPtr->interp = interp;
    errPtr->errorMsg = (char *) ckalloc((unsigned) (strlen(errResult) + 1));
    strcpy(errPtr->errorMsg, errResult);
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

    Tcl_Preserve((ClientData) assocPtr);
    
    while (assocPtr->firstBgPtr != NULL) {
	interp = assocPtr->firstBgPtr->interp;
	if (interp == NULL) {
	    goto doneWithInterp;
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
             * If the interpreter is safe, we look for a hidden command
             * named "bgerror" and call that with the error information.
             * Otherwise, simply ignore the error. The rationale is that
             * this could be an error caused by a malicious applet trying
             * to cause an infinite barrage of error messages. The hidden
             * "bgerror" command can be used by a security policy to
             * interpose on such attacks and e.g. kill the applet after a
             * few attempts.
             */

            if (Tcl_IsSafe(interp)) {
                Tcl_HashTable *hTblPtr;
                Tcl_HashEntry *hPtr;

                hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp,
                        "tclHiddenCmds", NULL);
                if (hTblPtr == (Tcl_HashTable *) NULL) {
                    goto doneWithInterp;
                }
                hPtr = Tcl_FindHashEntry(hTblPtr, "bgerror");
                if (hPtr == (Tcl_HashEntry *) NULL) {
                    goto doneWithInterp;
                }

                /*
                 * OK, the hidden command "bgerror" exists, invoke it.
                 */

                argv[0] = "bgerror";
                argv[1] = ckalloc((unsigned)
                        strlen(assocPtr->firstBgPtr->errorMsg));
                strcpy(argv[1], assocPtr->firstBgPtr->errorMsg);
                (void) TclInvoke(interp, 2, argv, TCL_INVOKE_HIDDEN);
                ckfree(argv[1]);

                goto doneWithInterp;
            } 

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

	/*
	 * Discard the command and the information about the error report.
	 */

doneWithInterp:

	if (assocPtr->firstBgPtr) {
	    ckfree(assocPtr->firstBgPtr->errorMsg);
	    ckfree(assocPtr->firstBgPtr->errorInfo);
	    ckfree(assocPtr->firstBgPtr->errorCode);
	    errPtr = assocPtr->firstBgPtr->nextPtr;
	    ckfree((char *) assocPtr->firstBgPtr);
	    assocPtr->firstBgPtr = errPtr;
	}
        
        if (interp != NULL) {
            Tcl_Release((ClientData) interp);
        }
    }
    assocPtr->lastBgPtr = NULL;

    Tcl_Release((ClientData) assocPtr);
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
    Tcl_CancelIdleCall(HandleBgErrors, (ClientData) assocPtr);
    Tcl_EventuallyFree((ClientData) assocPtr, TCL_DYNAMIC);
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
    Tcl_Finalize();
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
 * Tcl_Finalize --
 *
 *	Runs the exit handlers to allow Tcl to clean up its state prior
 *	to being unloaded. Called by Tcl_Exit and when Tcl was dynamically
 *	loaded and is now being unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Whatever the exit handlers do. Also frees up storage associated
 *	with the Tcl object type table.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Finalize()
{
    ExitHandler *exitPtr;
    
    tclInExit = 1;
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

    /*
     * Uninitialize everything associated with the compile and execute
     * environment. This *must* be done at the latest possible time.
     */
    
    TclFinalizeCompExecEnv();
    firstExitPtr = NULL;
    tclInExit = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInExit --
 *
 *	Determines if we are in the middle of exit-time cleanup.
 *
 * Results:
 *	If we are in the middle of exiting, 1, otherwise 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclInExit()
{
    return tclInExit;
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
    if (Tcl_TraceVar(interp, argv[1],
	    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    VwaitVarProc, (ClientData) &done) != TCL_OK) {
	return TCL_ERROR;
    };
    done = 0;
    foundEvent = 1;
    while (!done && foundEvent) {
	foundEvent = Tcl_DoOneEvent(TCL_ALL_EVENTS);
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
    int flags;

    if (argc == 1) {
	flags = TCL_ALL_EVENTS|TCL_DONT_WAIT;
    } else if (argc == 2) {
	if (strncmp(argv[1], "idletasks", strlen(argv[1])) != 0) {
	    Tcl_AppendResult(interp, "bad option \"", argv[1],
		    "\": must be idletasks", (char *) NULL);
	    return TCL_ERROR;
	}
	flags = TCL_WINDOW_EVENTS|TCL_IDLE_EVENTS|TCL_DONT_WAIT;
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
