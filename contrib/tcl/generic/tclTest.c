/* 
 * tclTest.c --
 *
 *	This file contains C command procedures for a bunch of additional
 *	Tcl commands that are used for testing out Tcl's C interfaces.
 *	These commands are not normally included in Tcl applications;
 *	they're only used for testing.
 *
 * Copyright (c) 1993-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclTest.c 1.78 96/04/11 14:50:51
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Declare external functions used in Windows tests.
 */

#if defined(__WIN32__)
extern TclPlatformType *	TclWinGetPlatform _ANSI_ARGS_((void));
#endif

/*
 * Dynamic string shared by TestdcallCmd and DelCallbackProc;  used
 * to collect the results of the various deletion callbacks.
 */

static Tcl_DString delString;
static Tcl_Interp *delInterp;

/*
 * One of the following structures exists for each asynchronous
 * handler created by the "testasync" command".
 */

typedef struct TestAsyncHandler {
    int id;				/* Identifier for this handler. */
    Tcl_AsyncHandler handler;		/* Tcl's token for the handler. */
    char *command;			/* Command to invoke when the
					 * handler is invoked. */
    struct TestAsyncHandler *nextPtr;	/* Next is list of handlers. */
} TestAsyncHandler;

static TestAsyncHandler *firstHandler = NULL;

/*
 * The dynamic string below is used by the "testdstring" command
 * to test the dynamic string facilities.
 */

static Tcl_DString dstring;

/*
 * One of the following structures exists for each command created
 * by TestdelCmd:
 */

typedef struct DelCmd {
    Tcl_Interp *interp;		/* Interpreter in which command exists. */
    char *deleteCmd;		/* Script to execute when command is
				 * deleted.  Malloc'ed. */
} DelCmd;

/*
 * The following structure is used to keep track of modal timeout
 * handlers created by the "testmodal" command.
 */

typedef struct Modal {
    Tcl_Interp *interp;		/* Interpreter in which to set variable
				 * "x" when timer fires. */
    char *key;			/* Null-terminated string to store in
				 * global variable "x" in interp when
				 * timer fires.  Malloc-ed. */
} Modal;

/*
 * Forward declarations for procedures defined later in this file:
 */

int			Tcltest_Init _ANSI_ARGS_((Tcl_Interp *interp));
static int		AsyncHandlerProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int code));
static void		CleanupTestSetassocdataTests _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));
static void		CmdDelProc1 _ANSI_ARGS_((ClientData clientData));
static void		CmdDelProc2 _ANSI_ARGS_((ClientData clientData));
static int		CmdProc1 _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		CmdProc2 _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		DelCallbackProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static int		DelCmdProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		DelDeleteProc _ANSI_ARGS_((ClientData clientData));
static void		ExitProcEven _ANSI_ARGS_((ClientData clientData));
static void		ExitProcOdd _ANSI_ARGS_((ClientData clientData));
static void		ModalTimeoutProc _ANSI_ARGS_((ClientData clientData));
static void		SpecialFree _ANSI_ARGS_((char *blockPtr));
static int		StaticInitProc _ANSI_ARGS_((Tcl_Interp *interp));
static int		TestasyncCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestcmdinfoCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestcmdtokenCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestdcallCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestdelCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestdelassocdataCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestdstringCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestexithandlerCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestfilewaitCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestgetassocdataCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestgetplatformCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestfhandleCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestinterpdeleteCmd _ANSI_ARGS_((ClientData dummy,
		            Tcl_Interp *interp, int argc, char **argv));
static int		TestlinkCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestMathFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Tcl_Value *args,
			    Tcl_Value *resultPtr));
static int		TestmodalCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestsetassocdataCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestsetplatformCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TeststaticpkgCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TesttranslatefilenameCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestupvarCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestwordendCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestfeventCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestPanicCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));

/*
 * External (platform specific) initialization routine:
 */

EXTERN int		TclplatformtestInit _ANSI_ARGS_((
			    Tcl_Interp *interp));

/*
 *----------------------------------------------------------------------
 *
 * Tcltest_Init --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcltest_Init(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_PkgProvide(interp, "Tcltest", "7.5") == TCL_ERROR) {
        return TCL_ERROR;
    }

    /*
     * Create additional commands and math functions for testing Tcl.
     */

    Tcl_CreateCommand(interp, "testasync", TestasyncCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testchannel", TclTestChannelCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testchannelevent", TclTestChannelEventCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testcmdtoken", TestcmdtokenCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testcmdinfo", TestcmdinfoCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testdcall", TestdcallCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testdel", TestdelCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testdelassocdata", TestdelassocdataCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_DStringInit(&dstring);
    Tcl_CreateCommand(interp, "testdstring", TestdstringCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testexithandler", TestexithandlerCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testfhandle", TestfhandleCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testfilewait", TestfilewaitCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testgetassocdata", TestgetassocdataCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testgetplatform", TestgetplatformCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testinterpdelete", TestinterpdeleteCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testlink", TestlinkCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testmodal", TestmodalCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testsetassocdata", TestsetassocdataCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testsetplatform", TestsetplatformCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "teststaticpkg", TeststaticpkgCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testtranslatefilename",
            TesttranslatefilenameCmd, (ClientData) 0,
            (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testupvar", TestupvarCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testwordend", TestwordendCmd, (ClientData) 0,
	    (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testfevent", TestfeventCmd, (ClientData) 0,
            (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testpanic", TestPanicCmd, (ClientData) 0,
            (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateMathFunc(interp, "T1", 0, (Tcl_ValueType *) NULL, TestMathFunc,
	    (ClientData) 123);
    Tcl_CreateMathFunc(interp, "T2", 0, (Tcl_ValueType *) NULL, TestMathFunc,
	    (ClientData) 345);

    /*
     * And finally add any platform specific test commands.
     */
    
    return TclplatformtestInit(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * TestasyncCmd --
 *
 *	This procedure implements the "testasync" command.  It is used
 *	to test the asynchronous handler facilities of Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes, and invokes handlers.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestasyncCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TestAsyncHandler *asyncPtr, *prevPtr;
    int id, code;
    static int nextId = 1;

    if (argc < 2) {
	wrongNumArgs:
	interp->result = "wrong # args";
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	asyncPtr = (TestAsyncHandler *) ckalloc(sizeof(TestAsyncHandler));
	asyncPtr->id = nextId;
	nextId++;
	asyncPtr->handler = Tcl_AsyncCreate(AsyncHandlerProc,
		(ClientData) asyncPtr);
	asyncPtr->command = (char *) ckalloc((unsigned) (strlen(argv[2]) + 1));
	strcpy(asyncPtr->command, argv[2]);
	asyncPtr->nextPtr = firstHandler;
	firstHandler = asyncPtr;
	sprintf(interp->result, "%d", asyncPtr->id);
    } else if (strcmp(argv[1], "delete") == 0) {
	if (argc == 2) {
	    while (firstHandler != NULL) {
		asyncPtr = firstHandler;
		firstHandler = asyncPtr->nextPtr;
		Tcl_AsyncDelete(asyncPtr->handler);
		ckfree(asyncPtr->command);
		ckfree((char *) asyncPtr);
	    }
	    return TCL_OK;
	}
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[2], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (prevPtr = NULL, asyncPtr = firstHandler; asyncPtr != NULL;
		prevPtr = asyncPtr, asyncPtr = asyncPtr->nextPtr) {
	    if (asyncPtr->id != id) {
		continue;
	    }
	    if (prevPtr == NULL) {
		firstHandler = asyncPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = asyncPtr->nextPtr;
	    }
	    Tcl_AsyncDelete(asyncPtr->handler);
	    ckfree(asyncPtr->command);
	    ckfree((char *) asyncPtr);
	    break;
	}
    } else if (strcmp(argv[1], "mark") == 0) {
	if (argc != 5) {
	    goto wrongNumArgs;
	}
	if ((Tcl_GetInt(interp, argv[2], &id) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &code) != TCL_OK)) {
	    return TCL_ERROR;
	}
	for (asyncPtr = firstHandler; asyncPtr != NULL;
		asyncPtr = asyncPtr->nextPtr) {
	    if (asyncPtr->id == id) {
		Tcl_AsyncMark(asyncPtr->handler);
		break;
	    }
	}
	Tcl_SetResult(interp, argv[3], TCL_VOLATILE);
	return code;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, int, or mark",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
AsyncHandlerProc(clientData, interp, code)
    ClientData clientData;	/* Pointer to TestAsyncHandler structure. */
    Tcl_Interp *interp;		/* Interpreter in which command was
				 * executed, or NULL. */
    int code;			/* Current return code from command. */
{
    TestAsyncHandler *asyncPtr = (TestAsyncHandler *) clientData;
    char *listArgv[4];
    char string[20], *cmd;

    sprintf(string, "%d", code);
    listArgv[0] = asyncPtr->command;
    listArgv[1] = interp->result;
    listArgv[2] = string;
    listArgv[3] = NULL;
    cmd = Tcl_Merge(3, listArgv);
    code = Tcl_Eval(interp, cmd);
    ckfree(cmd);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TestcmdinfoCmd --
 *
 *	This procedure implements the "testcmdinfo" command.  It is used
 *	to test Tcl_GetCommandInfo, Tcl_SetCommandInfo, and command creation
 *	and deletion.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes various commands and modifies their data.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestcmdinfoCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_CmdInfo info;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option cmdName\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	Tcl_CreateCommand(interp, argv[2], CmdProc1, (ClientData) "original",
		CmdDelProc1);
    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_DStringInit(&delString);
	Tcl_DeleteCommand(interp, argv[2]);
	Tcl_DStringResult(interp, &delString);
    } else if (strcmp(argv[1], "get") == 0) {
	if (Tcl_GetCommandInfo(interp, argv[2], &info) ==0) {
	    interp->result = "??";
	    return TCL_OK;
	}
	if (info.proc == CmdProc1) {
	    Tcl_AppendResult(interp, "CmdProc1", " ",
		    (char *) info.clientData, (char *) NULL);
	} else if (info.proc == CmdProc2) {
	    Tcl_AppendResult(interp, "CmdProc2", " ",
		    (char *) info.clientData, (char *) NULL);
	} else {
	    Tcl_AppendResult(interp, "unknown", (char *) NULL);
	}
	if (info.deleteProc == CmdDelProc1) {
	    Tcl_AppendResult(interp, " CmdDelProc1", " ",
		    (char *) info.deleteData, (char *) NULL);
	} else if (info.deleteProc == CmdDelProc2) {
	    Tcl_AppendResult(interp, " CmdDelProc2", " ",
		    (char *) info.deleteData, (char *) NULL);
	} else {
	    Tcl_AppendResult(interp, " unknown", (char *) NULL);
	}
    } else if (strcmp(argv[1], "modify") == 0) {
	info.proc = CmdProc2;
	info.clientData = (ClientData) "new_command_data";
	info.deleteProc = CmdDelProc2;
	info.deleteData = (ClientData) "new_delete_data";
	if (Tcl_SetCommandInfo(interp, argv[2], &info) == 0) {
	    interp->result = "0";
	} else {
	    interp->result = "1";
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, get, or modify",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

	/*ARGSUSED*/
static int
CmdProc1(clientData, interp, argc, argv)
    ClientData clientData;		/* String to return. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_AppendResult(interp, "CmdProc1 ", (char *) clientData,
	    (char *) NULL);
    return TCL_OK;
}

	/*ARGSUSED*/
static int
CmdProc2(clientData, interp, argc, argv)
    ClientData clientData;		/* String to return. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_AppendResult(interp, "CmdProc2 ", (char *) clientData,
	    (char *) NULL);
    return TCL_OK;
}

static void
CmdDelProc1(clientData)
    ClientData clientData;		/* String to save. */
{
    Tcl_DStringInit(&delString);
    Tcl_DStringAppend(&delString, "CmdDelProc1 ", -1);
    Tcl_DStringAppend(&delString, (char *) clientData, -1);
}

static void
CmdDelProc2(clientData)
    ClientData clientData;		/* String to save. */
{
    Tcl_DStringInit(&delString);
    Tcl_DStringAppend(&delString, "CmdDelProc2 ", -1);
    Tcl_DStringAppend(&delString, (char *) clientData, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * TestcmdtokenCmd --
 *
 *	This procedure implements the "testcmdtoken" command.  It is used
 *	to test Tcl_Command tokens and Tcl_GetCommandName.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes various commands and modifies their data.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestcmdtokenCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Command token;
    long int l;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option arg\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	token = Tcl_CreateCommand(interp, argv[2], CmdProc1,
		(ClientData) "original", (Tcl_CmdDeleteProc *) NULL);
	sprintf(interp->result, "%lx", (long int) token);
    } else if (strcmp(argv[1], "name") == 0) {
	if (sscanf(argv[2], "%lx", &l) != 1) {
	    Tcl_AppendResult(interp, "bad command token \"", argv[2],
		    "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	interp->result = Tcl_GetCommandName(interp, (Tcl_Command) l);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create or name", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdcallCmd --
 *
 *	This procedure implements the "testdcall" command.  It is used
 *	to test Tcl_CallWhenDeleted.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes interpreters.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestdcallCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int i, id;

    delInterp = Tcl_CreateInterp();
    Tcl_DStringInit(&delString);
    for (i = 1; i < argc; i++) {
	if (Tcl_GetInt(interp, argv[i], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (id < 0) {
	    Tcl_DontCallWhenDeleted(delInterp, DelCallbackProc,
		    (ClientData) (-id));
	} else {
	    Tcl_CallWhenDeleted(delInterp, DelCallbackProc,
		    (ClientData) id);
	}
    }
    Tcl_DeleteInterp(delInterp);
    Tcl_DStringResult(interp, &delString);
    return TCL_OK;
}

/*
 * The deletion callback used by TestdcallCmd:
 */

static void
DelCallbackProc(clientData, interp)
    ClientData clientData;		/* Numerical value to append to
					 * delString. */
    Tcl_Interp *interp;			/* Interpreter being deleted. */
{
    int id = (int) clientData;
    char buffer[10];

    sprintf(buffer, "%d", id);
    Tcl_DStringAppendElement(&delString, buffer);
    if (interp != delInterp) {
	Tcl_DStringAppendElement(&delString, "bogus interpreter argument!");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestdelCmd --
 *
 *	This procedure implements the "testdcall" command.  It is used
 *	to test Tcl_CallWhenDeleted.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes interpreters.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestdelCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    DelCmd *dPtr;
    Tcl_Interp *slave;

    if (argc != 4) {
	interp->result = "wrong # args";
	return TCL_ERROR;
    }

    slave = Tcl_GetSlave(interp, argv[1]);
    if (slave == NULL) {
	return TCL_ERROR;
    }

    dPtr = (DelCmd *) ckalloc(sizeof(DelCmd));
    dPtr->interp = interp;
    dPtr->deleteCmd = (char *) ckalloc((unsigned) (strlen(argv[3]) + 1));
    strcpy(dPtr->deleteCmd, argv[3]);

    Tcl_CreateCommand(slave, argv[2], DelCmdProc, (ClientData) dPtr,
	    DelDeleteProc);
    return TCL_OK;
}

static int
DelCmdProc(clientData, interp, argc, argv)
    ClientData clientData;		/* String result to return. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    DelCmd *dPtr = (DelCmd *) clientData;

    Tcl_AppendResult(interp, dPtr->deleteCmd, (char *) NULL);
    ckfree(dPtr->deleteCmd);
    ckfree((char *) dPtr);
    return TCL_OK;
}

static void
DelDeleteProc(clientData)
    ClientData clientData;		/* String command to evaluate. */
{
    DelCmd *dPtr = (DelCmd *) clientData;

    Tcl_Eval(dPtr->interp, dPtr->deleteCmd);
    Tcl_ResetResult(dPtr->interp);
    ckfree(dPtr->deleteCmd);
    ckfree((char *) dPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestdelassocdataCmd --
 *
 *	This procedure implements the "testdelassocdata" command. It is used
 *	to test Tcl_DeleteAssocData.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes an association between a key and associated data from an
 *	interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
TestdelassocdataCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " data_key\"", (char *) NULL);
        return TCL_ERROR;
    }
    Tcl_DeleteAssocData(interp, argv[1]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdstringCmd --
 *
 *	This procedure implements the "testdstring" command.  It is used
 *	to test the dynamic string facilities of Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes, and invokes handlers.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestdstringCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int count;

    if (argc < 2) {
	wrongNumArgs:
	interp->result = "wrong # args";
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "append") == 0) {
	if (argc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[3], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&dstring, argv[2], count);
    } else if (strcmp(argv[1], "element") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	Tcl_DStringAppendElement(&dstring, argv[2]);
    } else if (strcmp(argv[1], "end") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringEndSublist(&dstring);
    } else if (strcmp(argv[1], "free") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringFree(&dstring);
    } else if (strcmp(argv[1], "get") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	interp->result = Tcl_DStringValue(&dstring);
    } else if (strcmp(argv[1], "gresult") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (strcmp(argv[2], "staticsmall") == 0) {
	    interp->result = "short";
	} else if (strcmp(argv[2], "staticlarge") == 0) {
	    interp->result = "first0 first1 first2 first3 first4 first5 first6 first7 first8 first9\nsecond0 second1 second2 second3 second4 second5 second6 second7 second8 second9\nthird0 third1 third2 third3 third4 third5 third6 third7 third8 third9\nfourth0 fourth1 fourth2 fourth3 fourth4 fourth5 fourth6 fourth7 fourth8 fourth9\nfifth0 fifth1 fifth2 fifth3 fifth4 fifth5 fifth6 fifth7 fifth8 fifth9\nsixth0 sixth1 sixth2 sixth3 sixth4 sixth5 sixth6 sixth7 sixth8 sixth9\nseventh0 seventh1 seventh2 seventh3 seventh4 seventh5 seventh6 seventh7 seventh8 seventh9\n";
	} else if (strcmp(argv[2], "free") == 0) {
	    interp->result = (char *) ckalloc(100);
	    interp->freeProc = TCL_DYNAMIC;
	    strcpy(interp->result, "This is a malloc-ed string");
	} else if (strcmp(argv[2], "special") == 0) {
	    interp->result = (char *) ckalloc(100);
	    interp->result += 4;
	    interp->freeProc = SpecialFree;
	    strcpy(interp->result, "This is a specially-allocated string");
	} else {
	    Tcl_AppendResult(interp, "bad gresult option \"", argv[2],
		    "\": must be staticsmall, staticlarge, free, or special",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_DStringGetResult(interp, &dstring);
    } else if (strcmp(argv[1], "length") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	sprintf(interp->result, "%d", Tcl_DStringLength(&dstring));
    } else if (strcmp(argv[1], "result") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringResult(interp, &dstring);
    } else if (strcmp(argv[1], "trunc") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[2], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_DStringTrunc(&dstring, count);
    } else if (strcmp(argv[1], "start") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringStartSublist(&dstring);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be append, element, end, free, get, length, ",
		"result, trunc, or start", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * The procedure below is used as a special freeProc to test how well
 * Tcl_DStringGetResult handles freeProc's other than free.
 */

static void SpecialFree(blockPtr)
    char *blockPtr;			/* Block to free. */
{
    ckfree(blockPtr - 4);
}

/*
 *----------------------------------------------------------------------
 *
 * TestexithandlerCmd --
 *
 *	This procedure implements the "testexithandler" command. It is
 *	used to test Tcl_CreateExitHandler and Tcl_DeleteExitHandler.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexithandlerCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int value;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " create|delete value\"", (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &value) != TCL_OK) {
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	Tcl_CreateExitHandler((value & 1) ? ExitProcOdd : ExitProcEven,
		(ClientData) value);
    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_DeleteExitHandler((value & 1) ? ExitProcOdd : ExitProcEven,
		(ClientData) value);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create or delete", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void
ExitProcOdd(clientData)
    ClientData clientData;		/* Integer value to print. */
{
    char buf[100];

    sprintf(buf, "odd %d\n", (int) clientData);
    write(1, buf, strlen(buf));
}

static void
ExitProcEven(clientData)
    ClientData clientData;		/* Integer value to print. */
{
    char buf[100];

    sprintf(buf, "even %d\n", (int) clientData);
    write(1, buf, strlen(buf));
}

/*
 *----------------------------------------------------------------------
 *
 * TestfhandleCmd --
 *
 *	This procedure implements the "testfhandle" command.  It is
 *	used to test Tcl_GetFile, Tcl_FreeFile, and
 *	Tcl_GetFileInfo.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestfhandleCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#define MAX_FHANDLES 10
    static Tcl_File testHandles[MAX_FHANDLES];
    static initialized = 0;

    int i, index, type;
    ClientData data;

    if (!initialized) {
	for (i = 0; i < MAX_FHANDLES; i++) {
	    testHandles[i] = NULL;
	}
	initialized = 1;
    }
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " option ... \"", (char *) NULL);
        return TCL_ERROR;
    }
    index = -1;
    if (argc >= 3) {
	if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (index >= MAX_FHANDLES) {
	    Tcl_AppendResult(interp, "bad index ", argv[2], (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if (strcmp(argv[1], "compare") == 0) {
	int index2;
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		    argv[0], " index index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], (int *) &index2) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (testHandles[index] == testHandles[index2]) {
	    sprintf(interp->result, "equal");
	} else {
	    sprintf(interp->result, "notequal");
	}
    } else if (strcmp(argv[1], "get") == 0) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		    argv[0], " index data type\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], (int *) &data) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[4], &type) != TCL_OK) {
	    return TCL_ERROR;
	}
	testHandles[index] = Tcl_GetFile(data, type);
    } else if (strcmp(argv[1], "free") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		    argv[0], " index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_FreeFile(testHandles[index]);
    } else if (strcmp(argv[1], "info1") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		    argv[0], " index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	data = Tcl_GetFileInfo(testHandles[index], NULL);
	sprintf(interp->result, "%d", (int)data);
    } else if (strcmp(argv[1], "info2") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		    argv[0], " index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	data = Tcl_GetFileInfo(testHandles[index], &type);
	sprintf(interp->result, "%d %d", (int)data, type);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be compare, get, free, info1, or info2",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestfilewaitCmd --
 *
 *	This procedure implements the "testfilewait" command. It is
 *	used to test TclWaitForFile.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestfilewaitCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int mask, result, timeout;
    Tcl_Channel channel;
    Tcl_File file;

    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" file readable|writable|both timeout\"", (char *) NULL);
	return TCL_ERROR;
    }
    channel = Tcl_GetChannel(interp, argv[1], NULL);
    if (channel == NULL) {
	return TCL_ERROR;
    }
    if (strcmp(argv[2], "readable") == 0) {
	mask = TCL_READABLE;
    } else if (strcmp(argv[2], "writable") == 0){
	mask = TCL_WRITABLE;
    } else if (strcmp(argv[2], "both") == 0){
	mask = TCL_WRITABLE|TCL_READABLE;
    } else {
	Tcl_AppendResult(interp, "bad argument \"", argv[2],
		"\": must be readable, writable, or both", (char *) NULL);
	return TCL_ERROR;
    }
    file = Tcl_GetChannelFile(channel, 
	    (mask & TCL_READABLE) ? TCL_READABLE : TCL_WRITABLE);
    if (file == NULL) {
	interp->result = "couldn't get channel file";
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[3], &timeout) != TCL_OK) {
	return TCL_ERROR;
    }
    result = TclWaitForFile(file, mask, timeout);
    if (result & TCL_READABLE) {
	Tcl_AppendElement(interp, "readable");
    }
    if (result & TCL_WRITABLE) {
	Tcl_AppendElement(interp, "writable");
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetassocdataCmd --
 *
 *	This procedure implements the "testgetassocdata" command. It is
 *	used to test Tcl_GetAssocData.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetassocdataCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *res;
    
    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " data_key\"", (char *) NULL);
        return TCL_ERROR;
    }
    res = (char *) Tcl_GetAssocData(interp, argv[1], NULL);
    if (res != NULL) {
        Tcl_AppendResult(interp, res, NULL);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetplatformCmd --
 *
 *	This procedure implements the "testgetplatform" command. It is
 *	used to retrievel the value of the tclPlatform global variable.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetplatformCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    static char *platformStrings[] = { "unix", "mac", "windows" };
    TclPlatformType *platform;

#ifdef __WIN32__
    platform = TclWinGetPlatform();
#else
    platform = &tclPlatform;
#endif
    
    if (argc != 1) {
        Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		(char *) NULL);
        return TCL_ERROR;
    }

    Tcl_AppendResult(interp, platformStrings[*platform], NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestinterpdeleteCmd --
 *
 *	This procedure tests the code in tclInterp.c that deals with
 *	interpreter deletion. It deletes a user-specified interpreter
 *	from the hierarchy, and subsequent code checks integrity.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes one or more interpreters.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestinterpdeleteCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Interp *slaveToDelete;

    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " path\"", (char *) NULL);
        return TCL_ERROR;
    }
    if (argv[1][0] == '\0') {
        Tcl_AppendResult(interp, "cannot delete current interpreter",
                (char *) NULL);
        return TCL_ERROR;
    }
    slaveToDelete = Tcl_GetSlave(interp, argv[1]);
    if (slaveToDelete == (Tcl_Interp *) NULL) {
        Tcl_AppendResult(interp, "could not find interpreter \"",
                argv[1], "\"", (char *) NULL);
        return TCL_ERROR;
    }
    Tcl_DeleteInterp(slaveToDelete);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestlinkCmd --
 *
 *	This procedure implements the "testlink" command.  It is used
 *	to test Tcl_LinkVar and related library procedures.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes various variable links, plus returns
 *	values of the linked variables.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestlinkCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    static int intVar = 43;
    static int boolVar = 4;
    static double realVar = 1.23;
    static char *stringVar = NULL;
    static int created = 0;
    char buffer[TCL_DOUBLE_SPACE];
    int writable, flag;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	if (created) {
	    Tcl_UnlinkVar(interp, "int");
	    Tcl_UnlinkVar(interp, "real");
	    Tcl_UnlinkVar(interp, "bool");
	    Tcl_UnlinkVar(interp, "string");
	}
	created = 1;
	if (Tcl_GetBoolean(interp, argv[2], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = (writable != 0) ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "int", (char *) &intVar,
		TCL_LINK_INT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[3], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = (writable != 0) ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "real", (char *) &realVar,
		TCL_LINK_DOUBLE | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[4], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = (writable != 0) ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "bool", (char *) &boolVar,
		TCL_LINK_BOOLEAN | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[5], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = (writable != 0) ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "string", (char *) &stringVar,
		TCL_LINK_STRING | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_UnlinkVar(interp, "int");
	Tcl_UnlinkVar(interp, "real");
	Tcl_UnlinkVar(interp, "bool");
	Tcl_UnlinkVar(interp, "string");
	created = 0;
    } else if (strcmp(argv[1], "get") == 0) {
	sprintf(buffer, "%d", intVar);
	Tcl_AppendElement(interp, buffer);
	Tcl_PrintDouble(interp, realVar, buffer);
	Tcl_AppendElement(interp, buffer);
	sprintf(buffer, "%d", boolVar);
	Tcl_AppendElement(interp, buffer);
	Tcl_AppendElement(interp, (stringVar == NULL) ? "-" : stringVar);
    } else if (strcmp(argv[1], "set") == 0) {
	if (argc != 6) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ", argv[1],
		"intValue realValue boolValue stringValue\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argv[2][0] != 0) {
	    if (Tcl_GetInt(interp, argv[2], &intVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (argv[3][0] != 0) {
	    if (Tcl_GetDouble(interp, argv[3], &realVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (argv[4][0] != 0) {
	    if (Tcl_GetInt(interp, argv[4], &boolVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (argv[5][0] != 0) {
	    if (stringVar != NULL) {
		ckfree(stringVar);
	    }
	    if (strcmp(argv[5], "-") == 0) {
		stringVar = NULL;
	    } else {
		stringVar = (char *) ckalloc((unsigned) (strlen(argv[5]) + 1));
		strcpy(stringVar, argv[5]);
	    }
	}
    } else if (strcmp(argv[1], "update") == 0) {
	if (argc != 6) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ", argv[1],
		"intValue realValue boolValue stringValue\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argv[2][0] != 0) {
	    if (Tcl_GetInt(interp, argv[2], &intVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_UpdateLinkedVar(interp, "int");
	}
	if (argv[3][0] != 0) {
	    if (Tcl_GetDouble(interp, argv[3], &realVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_UpdateLinkedVar(interp, "real");
	}
	if (argv[4][0] != 0) {
	    if (Tcl_GetInt(interp, argv[4], &boolVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_UpdateLinkedVar(interp, "bool");
	}
	if (argv[5][0] != 0) {
	    if (stringVar != NULL) {
		ckfree(stringVar);
	    }
	    if (strcmp(argv[5], "-") == 0) {
		stringVar = NULL;
	    } else {
		stringVar = (char *) ckalloc((unsigned) (strlen(argv[5]) + 1));
		strcpy(stringVar, argv[5]);
	    }
	    Tcl_UpdateLinkedVar(interp, "string");
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be create, delete, get, set, or update",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestMathFunc --
 *
 *	This is a user-defined math procedure to test out math procedures
 *	with no arguments.
 *
 * Results:
 *	A normal Tcl completion code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestMathFunc(clientData, interp, args, resultPtr)
    ClientData clientData;		/* Integer value to return. */
    Tcl_Interp *interp;			/* Not used. */
    Tcl_Value *args;			/* Not used. */
    Tcl_Value *resultPtr;		/* Where to store result. */
{
    resultPtr->type = TCL_INT;
    resultPtr->intValue = (int) clientData;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupTestSetassocdataTests --
 *
 *	This function is called when an interpreter is deleted to clean
 *	up any data left over from running the testsetassocdata command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Releases storage.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
static void
CleanupTestSetassocdataTests(clientData, interp)
    ClientData clientData;		/* Data to be released. */
    Tcl_Interp *interp;			/* Interpreter being deleted. */
{
    ckfree((char *) clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * TestmodalCmd --
 *
 *	This procedure implements the "testmodal" command. It is used
 *	to test modal timeouts created by Tcl_CreateModalTimeout.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Modifies or creates an association between a key and associated
 *	data for this interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
TestmodalCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#define NUM_MODALS 10
    static Modal modals[NUM_MODALS];
    static int numModals = 0;
    int ms;

    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " option ?arg arg ...?\"", (char *) NULL);
        return TCL_ERROR;
    }

    if (strcmp(argv[1], "create") == 0) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		    " create ms key\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (numModals >= NUM_MODALS) {
	    interp->result = "too many modal timeouts";
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[2], &ms) != TCL_OK) {
	    return TCL_ERROR;
	}
	modals[numModals].interp = interp;
	modals[numModals].key = (char *) ckalloc((unsigned)
		(strlen(argv[3]) + 1));
	strcpy(modals[numModals].key, argv[3]);
	Tcl_CreateModalTimeout(ms, ModalTimeoutProc,
		(ClientData) &modals[numModals]);
	numModals += 1;
    } else if (strcmp(argv[1], "delete") == 0) {
	if (numModals == 0) {
	    interp->result = "no more modal timeouts";
	    return TCL_ERROR;
	}
	numModals -= 1;
	ckfree(modals[numModals].key);
	Tcl_DeleteModalTimeout(ModalTimeoutProc,
		(ClientData) &modals[numModals]);
    } else if (strcmp(argv[1], "event") == 0) {
	Tcl_DoOneEvent(TCL_TIMER_EVENTS|TCL_DONT_WAIT);
    } else if (strcmp(argv[1], "eventnotimers") == 0) {
	Tcl_DoOneEvent(0x100000|TCL_DONT_WAIT);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, event, or eventnotimers",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void
ModalTimeoutProc(clientData)
    ClientData clientData;	/* Pointer to Modal structure. */
{
    Modal *modalPtr = (Modal *) clientData;
    Tcl_SetVar(modalPtr->interp, "x", modalPtr->key,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetassocdataCmd --
 *
 *	This procedure implements the "testsetassocdata" command. It is used
 *	to test Tcl_SetAssocData.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Modifies or creates an association between a key and associated
 *	data for this interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetassocdataCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *buf;
    
    if (argc != 3) {
        Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " data_key data_item\"", (char *) NULL);
        return TCL_ERROR;
    }

    buf = ckalloc((unsigned) strlen(argv[2]) + 1);
    strcpy(buf, argv[2]);
    
    Tcl_SetAssocData(interp, argv[1], CleanupTestSetassocdataTests, 
	(ClientData) buf);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetplatformCmd --
 *
 *	This procedure implements the "testsetplatform" command. It is
 *	used to change the tclPlatform global variable so all file
 *	name conversions can be tested on a single platform.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets the tclPlatform global variable.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetplatformCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    size_t length;
    TclPlatformType *platform;

#ifdef __WIN32__
    platform = TclWinGetPlatform();
#else
    platform = &tclPlatform;
#endif
    
    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " platform\"", (char *) NULL);
        return TCL_ERROR;
    }

    length = strlen(argv[1]);
    if (strncmp(argv[1], "unix", length) == 0) {
	*platform = TCL_PLATFORM_UNIX;
    } else if (strncmp(argv[1], "mac", length) == 0) {
	*platform = TCL_PLATFORM_MAC;
    } else if (strncmp(argv[1], "windows", length) == 0) {
	*platform = TCL_PLATFORM_WINDOWS;
    } else {
        Tcl_AppendResult(interp, "unsupported platform: should be one of ",
		"unix, mac, or windows", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TeststaticpkgCmd --
 *
 *	This procedure implements the "teststaticpkg" command.
 *	It is used to test the procedure Tcl_StaticPackage.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	When the packge given by argv[1] is loaded into an interpeter,
 *	variable "x" in that interpreter is set to "loaded".
 *
 *----------------------------------------------------------------------
 */

static int
TeststaticpkgCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int safe, loaded;

    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " pkgName safe loaded\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &safe) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[3], &loaded) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage((loaded) ? interp : NULL, argv[1], StaticInitProc,
	    (safe) ? StaticInitProc : NULL);
    return TCL_OK;
}

static int
StaticInitProc(interp)
    Tcl_Interp *interp;			/* Interpreter in which package
					 * is supposedly being loaded. */
{
    Tcl_SetVar(interp, "x", "loaded", TCL_GLOBAL_ONLY);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TesttranslatefilenameCmd --
 *
 *	This procedure implements the "testtranslatefilename" command.
 *	It is used to test the Tcl_TranslateFileName command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TesttranslatefilenameCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_DString buffer;
    char *result;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " path\"", (char *) NULL);
	return TCL_ERROR;
    }
    result = Tcl_TranslateFileName(interp, argv[1], &buffer);
    if (result == NULL) {
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, result, NULL);
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestupvarCmd --
 *
 *	This procedure implements the "testupvar2" command.  It is used
 *	to test Tcl_UpVar and Tcl_UpVar2.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates or modifies an "upvar" reference.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestupvarCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if ((argc != 5) && (argc != 6)) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " level name ?name2? dest global\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (argc == 5) {
	return Tcl_UpVar(interp, argv[1], argv[2], argv[3],
		(strcmp(argv[4], "global") == 0) ? TCL_GLOBAL_ONLY : 0);
    } else {
	return Tcl_UpVar2(interp, argv[1], argv[2], 
		(argv[3][0] == 0) ? (char *) NULL : argv[3], argv[4],
		(strcmp(argv[5], "global") == 0) ? TCL_GLOBAL_ONLY : 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestwordendCmd --
 *
 *	This procedure implements the "testwordend" command.  It is used
 *	to test TclWordEnd.
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
static int
TestwordendCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " string\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, TclWordEnd(argv[1], 0, (int *) NULL), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestfeventCmd --
 *
 *	This procedure implements the "testfevent" command.  It is
 *	used for testing the "fileevent" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes interpreters.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestfeventCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    static Tcl_Interp *interp2 = NULL;
    int code;
    Tcl_Channel chan;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?", (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "cmd") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " cmd script", (char *) NULL);
	    return TCL_ERROR;
	}
        if (interp2 != (Tcl_Interp *) NULL) {
            code = Tcl_GlobalEval(interp2, argv[2]);
            interp->result = interp2->result;
            return code;
        } else {
            Tcl_AppendResult(interp,
                    "called \"testfevent code\" before \"testfevent create\"",
                    (char *) NULL);
            return TCL_ERROR;
        }
    } else if (strcmp(argv[1], "create") == 0) {
	if (interp2 != NULL) {
            Tcl_DeleteInterp(interp2);
	}
        interp2 = Tcl_CreateInterp();
	return TCL_OK;
    } else if (strcmp(argv[1], "delete") == 0) {
	if (interp2 != NULL) {
            Tcl_DeleteInterp(interp2);
	}
	interp2 = NULL;
    } else if (strcmp(argv[1], "share") == 0) {
        if (interp2 != NULL) {
            chan = Tcl_GetChannel(interp, argv[2], NULL);
            if (chan == (Tcl_Channel) NULL) {
                return TCL_ERROR;
            }
            Tcl_RegisterChannel(interp2, chan);
        }
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestPanicCmd --
 *
 *	Calls the panic routine.
 *
 * Results:
 *      Always returns TCL_OK. 
 *
 * Side effects:
 *	May exit application.
 *
 *----------------------------------------------------------------------
 */

static int
TestPanicCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *argString;
    
    /*
     *  Put the arguments into a var args structure
     *  Append all of the arguments together separated by spaces
     */

    argString = Tcl_Merge(argc-1, argv+1);
    panic(argString);
    ckfree(argString);
 
    return TCL_OK;
}
