/* 
 * tclBasic.c --
 *
 *	Contains the basic facilities for TCL command interpretation,
 *	including interpreter creation and deletion, command creation
 *	and deletion, and command parsing and execution.
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclBasic.c 1.280 97/05/20 19:09:26
 */

#include "tclInt.h"
#include "tclCompile.h"
#ifndef TCL_GENERIC_ONLY
#   include "tclPort.h"
#endif

/*
 * Static procedures in this file:
 */

static void		DeleteInterpProc _ANSI_ARGS_((Tcl_Interp *interp));
static void		HiddenCmdsDeleteProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));

/*
 * The following structure defines the commands in the Tcl core.
 */

typedef struct {
    char *name;			/* Name of object-based command. */
    Tcl_CmdProc *proc;		/* String-based procedure for command. */
    Tcl_ObjCmdProc *objProc;	/* Object-based procedure for command. */
    CompileProc *compileProc;	/* Procedure called to compile command. */
    int isSafe;			/* If non-zero, command will be present
                                 * in safe interpreter. Otherwise it will
                                 * be hidden. */
} CmdInfo;

/*
 * The built-in commands, and the procedures that implement them:
 */

static CmdInfo builtInCmds[] = {
    /*
     * Commands in the generic core. Note that at least one of the proc or
     * objProc members should be non-NULL. This avoids infinitely recursive
     * calls between TclInvokeObjectCommand and TclInvokeStringCommand if a
     * command name is computed at runtime and results in the name of a
     * compiled command.
     */

    {"append",		(Tcl_CmdProc *) NULL,	Tcl_AppendObjCmd,
        (CompileProc *) NULL,		1},
    {"array",		(Tcl_CmdProc *) NULL,	Tcl_ArrayObjCmd,
        (CompileProc *) NULL,		1},
    {"binary",		(Tcl_CmdProc *) NULL,	Tcl_BinaryObjCmd,
        (CompileProc *) NULL,		1},
    {"break",		Tcl_BreakCmd,		(Tcl_ObjCmdProc *) NULL,
        TclCompileBreakCmd,		1},
    {"case",		(Tcl_CmdProc *) NULL,	Tcl_CaseObjCmd,
        (CompileProc *) NULL,		1},
    {"catch",		(Tcl_CmdProc *) NULL,	Tcl_CatchObjCmd,	
        TclCompileCatchCmd,		1},
    {"clock",		(Tcl_CmdProc *) NULL,	Tcl_ClockObjCmd,
        (CompileProc *) NULL,		1},
    {"concat",		(Tcl_CmdProc *) NULL,	Tcl_ConcatObjCmd,
        (CompileProc *) NULL,		1},
    {"continue",	Tcl_ContinueCmd,	(Tcl_ObjCmdProc *) NULL,
        TclCompileContinueCmd,		1},
    {"error",		(Tcl_CmdProc *) NULL,	Tcl_ErrorObjCmd,
        (CompileProc *) NULL,		1},
    {"eval",		(Tcl_CmdProc *) NULL,	Tcl_EvalObjCmd,
        (CompileProc *) NULL,		1},
    {"exit",		(Tcl_CmdProc *) NULL,	Tcl_ExitObjCmd,
        (CompileProc *) NULL,		0},
    {"expr",		(Tcl_CmdProc *) NULL,	Tcl_ExprObjCmd,
        TclCompileExprCmd,		1},
    {"fcopy",		(Tcl_CmdProc *) NULL,	Tcl_FcopyObjCmd,
        (CompileProc *) NULL,		1},
    {"fileevent",	Tcl_FileEventCmd,	(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"for",		Tcl_ForCmd,		(Tcl_ObjCmdProc *) NULL,
        TclCompileForCmd,		1},
    {"foreach",		(Tcl_CmdProc *) NULL,	Tcl_ForeachObjCmd,
        TclCompileForeachCmd,		1},
    {"format",		Tcl_FormatCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"global",		(Tcl_CmdProc *) NULL,	Tcl_GlobalObjCmd,
        (CompileProc *) NULL,		1},
    {"history",		Tcl_HistoryCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"if",		Tcl_IfCmd,		(Tcl_ObjCmdProc *) NULL,
        TclCompileIfCmd,		1},
    {"incr",		Tcl_IncrCmd,		(Tcl_ObjCmdProc *) NULL,
        TclCompileIncrCmd,		1},
    {"info",		(Tcl_CmdProc *) NULL,	Tcl_InfoObjCmd,
        (CompileProc *) NULL,		1},
    {"interp",		(Tcl_CmdProc *) NULL,	Tcl_InterpObjCmd,
        (CompileProc *) NULL,		1},
    {"join",		(Tcl_CmdProc *) NULL,	Tcl_JoinObjCmd,
        (CompileProc *) NULL,		1},
    {"lappend",		(Tcl_CmdProc *) NULL,	Tcl_LappendObjCmd,
        (CompileProc *) NULL,		1},
    {"lindex",		(Tcl_CmdProc *) NULL,	Tcl_LindexObjCmd,
        (CompileProc *) NULL,		1},
    {"linsert",		(Tcl_CmdProc *) NULL,	Tcl_LinsertObjCmd,
        (CompileProc *) NULL,		1},
    {"list",		(Tcl_CmdProc *) NULL,	Tcl_ListObjCmd,
        (CompileProc *) NULL,		1},
    {"llength",		(Tcl_CmdProc *) NULL,	Tcl_LlengthObjCmd,
        (CompileProc *) NULL,		1},
    {"load",		Tcl_LoadCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"lrange",		(Tcl_CmdProc *) NULL,	Tcl_LrangeObjCmd,
        (CompileProc *) NULL,		1},
    {"lreplace",	(Tcl_CmdProc *) NULL,	Tcl_LreplaceObjCmd,
        (CompileProc *) NULL,		1},
    {"lsearch",		(Tcl_CmdProc *) NULL,	Tcl_LsearchObjCmd,
        (CompileProc *) NULL,		1},
    {"lsort",		(Tcl_CmdProc *) NULL,	Tcl_LsortObjCmd,
        (CompileProc *) NULL,		1},
    {"namespace",	(Tcl_CmdProc *) NULL,	Tcl_NamespaceObjCmd,
        (CompileProc *) NULL,		1},
    {"package",		Tcl_PackageCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"proc",		(Tcl_CmdProc *) NULL,	Tcl_ProcObjCmd,	
        (CompileProc *) NULL,		1},
    {"regexp",		Tcl_RegexpCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"regsub",		Tcl_RegsubCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"rename",		(Tcl_CmdProc *) NULL,	Tcl_RenameObjCmd,
        (CompileProc *) NULL,		1},
    {"return",		(Tcl_CmdProc *) NULL,	Tcl_ReturnObjCmd,	
        (CompileProc *) NULL,		1},
    {"scan",		Tcl_ScanCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"set",		Tcl_SetCmd,		(Tcl_ObjCmdProc *) NULL,    
        TclCompileSetCmd,		1},
    {"split",		Tcl_SplitCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"string",		(Tcl_CmdProc *) NULL,	Tcl_StringObjCmd,
        (CompileProc *) NULL,		1},
    {"subst",		Tcl_SubstCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"switch",		(Tcl_CmdProc *) NULL,	Tcl_SwitchObjCmd,	
        (CompileProc *) NULL,		1},
    {"trace",		Tcl_TraceCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"unset",		(Tcl_CmdProc *) NULL,	Tcl_UnsetObjCmd,	
        (CompileProc *) NULL,		1},
    {"uplevel",		(Tcl_CmdProc *) NULL,	Tcl_UplevelObjCmd,	
        (CompileProc *) NULL,		1},
    {"upvar",		(Tcl_CmdProc *) NULL,	Tcl_UpvarObjCmd,	
        (CompileProc *) NULL,		1},
    {"variable",	(Tcl_CmdProc *) NULL,	Tcl_VariableObjCmd,
        (CompileProc *) NULL,		1},
    {"while",		Tcl_WhileCmd,		(Tcl_ObjCmdProc *) NULL,    
        TclCompileWhileCmd,		1},

    /*
     * Commands in the UNIX core:
     */

#ifndef TCL_GENERIC_ONLY
    {"after",		Tcl_AfterCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"cd",		Tcl_CdCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"close",		Tcl_CloseCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"eof",		Tcl_EofCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"fblocked",	Tcl_FblockedCmd,	(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"fconfigure",	Tcl_FconfigureCmd,	(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"file",		(Tcl_CmdProc *) NULL,	Tcl_FileObjCmd,
        (CompileProc *) NULL,		0},
    {"flush",		(Tcl_CmdProc *) NULL,	Tcl_FlushObjCmd,
        (CompileProc *) NULL,		1},
    {"gets",		(Tcl_CmdProc *) NULL,	Tcl_GetsObjCmd,
        (CompileProc *) NULL,		1},
    {"glob",		Tcl_GlobCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"open",		Tcl_OpenCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"pid",		(Tcl_CmdProc *) NULL,	Tcl_PidObjCmd,
        (CompileProc *) NULL,		1},
    {"puts",		(Tcl_CmdProc *) NULL,	Tcl_PutsObjCmd,
        (CompileProc *) NULL,		1},
    {"pwd",		Tcl_PwdCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"read",		(Tcl_CmdProc *) NULL,	Tcl_ReadObjCmd,
        (CompileProc *) NULL,		1},
    {"seek",		Tcl_SeekCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"socket",		Tcl_SocketCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"tell",		Tcl_TellCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"time",		(Tcl_CmdProc *) NULL,	Tcl_TimeObjCmd,
        (CompileProc *) NULL,		1},
    {"update",		Tcl_UpdateCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		1},
    {"vwait",		Tcl_VwaitCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    
#ifdef MAC_TCL
    {"beep",		(Tcl_CmdProc *) NULL,	Tcl_BeepObjCmd,
        (CompileProc *) NULL,		0},
    {"echo",		Tcl_EchoCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"ls",		Tcl_LsCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"resource",	(Tcl_CmdProc *) NULL,	Tcl_ResourceObjCmd,
        (CompileProc *) NULL,		1},
    {"source",		(Tcl_CmdProc *) NULL,	Tcl_MacSourceObjCmd,
        (CompileProc *) NULL,		0},
#else
    {"exec",		Tcl_ExecCmd,		(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0},
    {"source",		(Tcl_CmdProc *) NULL,	Tcl_SourceObjCmd,
        (CompileProc *) NULL,		0},
#endif /* MAC_TCL */
    
#endif /* TCL_GENERIC_ONLY */
    {NULL,		(Tcl_CmdProc *) NULL,	(Tcl_ObjCmdProc *) NULL,
        (CompileProc *) NULL,		0}
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateInterp --
 *
 *	Create a new TCL command interpreter.
 *
 * Results:
 *	The return value is a token for the interpreter, which may be
 *	used in calls to procedures like Tcl_CreateCmd, Tcl_Eval, or
 *	Tcl_DeleteInterp.
 *
 * Side effects:
 *	The command interpreter is initialized with an empty variable
 *	table and the built-in commands.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_CreateInterp()
{
    register Interp *iPtr;
    register Command *cmdPtr;
    register CmdInfo *cmdInfoPtr;
    union {
	char c[sizeof(short)];
	short s;
    } order;
    int i;

    /*
     * Panic if someone updated the CallFrame structure without
     * also updating the Tcl_CallFrame structure (or vice versa).
     */  

    if (sizeof(Tcl_CallFrame) != sizeof(CallFrame)) {
        panic("Tcl_CallFrame and CallFrame are not the same size");
    }

    /*
     * Initialize support for namespaces and create the global namespace
     * (whose name is ""; an alias is "::"). This also initializes the
     * Tcl object type table and other object management code.
     */

    TclInitNamespaces();
    
    iPtr = (Interp *) ckalloc(sizeof(Interp));
    iPtr->result = iPtr->resultSpace;
    iPtr->freeProc = 0;
    iPtr->objResultPtr = Tcl_NewObj(); /* an empty object */
    Tcl_IncrRefCount(iPtr->objResultPtr);
    iPtr->errorLine = 0;
    Tcl_InitHashTable(&iPtr->mathFuncTable, TCL_STRING_KEYS);
    iPtr->numLevels = 0;
    iPtr->maxNestingDepth = 1000;
    iPtr->framePtr = NULL;
    iPtr->varFramePtr = NULL;
    iPtr->activeTracePtr = NULL;
    iPtr->returnCode = TCL_OK;
    iPtr->errorInfo = NULL;
    iPtr->errorCode = NULL;
    iPtr->numEvents = 0;
    iPtr->events = NULL;
    iPtr->curEvent = 0;
    iPtr->curEventNum = 0;
    iPtr->revPtr = NULL;
    iPtr->historyFirst = NULL;
    iPtr->revDisables = 1;
    iPtr->evalFirst = iPtr->evalLast = NULL;
    iPtr->appendResult = NULL;
    iPtr->appendAvl = 0;
    iPtr->appendUsed = 0;
    for (i = 0; i < NUM_REGEXPS; i++) {
	iPtr->patterns[i] = NULL;
	iPtr->patLengths[i] = -1;
	iPtr->regexps[i] = NULL;
    }
    Tcl_InitHashTable(&iPtr->packageTable, TCL_STRING_KEYS);
    iPtr->packageUnknown = NULL;
    iPtr->cmdCount = 0;
    iPtr->termOffset = 0;
    iPtr->compileEpoch = 0;
    iPtr->compiledProcPtr = NULL;
    iPtr->evalFlags = 0;
    iPtr->scriptFile = NULL;
    iPtr->flags = 0;
    iPtr->tracePtr = NULL;
    iPtr->assocData = (Tcl_HashTable *) NULL;
    iPtr->execEnvPtr = NULL;	      /* set after namespaces initialized */
    iPtr->emptyObjPtr = Tcl_NewObj(); /* another empty object */
    Tcl_IncrRefCount(iPtr->emptyObjPtr);
    iPtr->resultSpace[0] = 0;

    iPtr->globalNsPtr = NULL;	/* force creation of global ns below */
    iPtr->globalNsPtr = (Namespace *) Tcl_CreateNamespace(
	    (Tcl_Interp *) iPtr, "", (ClientData) NULL,
	    (Tcl_NamespaceDeleteProc *) NULL);
    if (iPtr->globalNsPtr == NULL) {
        panic("Tcl_CreateInterp: can't create global namespace");
    }

    /*
     * Initialize support for code compilation. Do this after initializing
     * namespaces since TclCreateExecEnv will try to reference a Tcl
     * variable (it links to the Tcl "tcl_traceExec" variable).
     */
    
    iPtr->execEnvPtr = TclCreateExecEnv((Tcl_Interp *) iPtr);

    /*
     * Create the core commands. Do it here, rather than calling
     * Tcl_CreateCommand, because it's faster (there's no need to check for
     * a pre-existing command by the same name). If a command has a
     * Tcl_CmdProc but no Tcl_ObjCmdProc, set the Tcl_ObjCmdProc to
     * TclInvokeStringCommand. This is an object-based wrapper procedure
     * that extracts strings, calls the string procedure, and creates an
     * object for the result. Similarly, if a command has a Tcl_ObjCmdProc
     * but no Tcl_CmdProc, set the Tcl_CmdProc to TclInvokeObjectCommand.
     */

    for (cmdInfoPtr = builtInCmds;  cmdInfoPtr->name != NULL;
	    cmdInfoPtr++) {
	int new;
	Tcl_HashEntry *hPtr;

	if ((cmdInfoPtr->proc == (Tcl_CmdProc *) NULL)
	        && (cmdInfoPtr->objProc == (Tcl_ObjCmdProc *) NULL)
	        && (cmdInfoPtr->compileProc == (CompileProc *) NULL)) {
	    panic("Tcl_CreateInterp: builtin command with NULL string and object command procs and a NULL compile proc\n");
	}
	
	hPtr = Tcl_CreateHashEntry(&iPtr->globalNsPtr->cmdTable,
	        cmdInfoPtr->name, &new);
	if (new) {
	    cmdPtr = (Command *) ckalloc(sizeof(Command));
	    cmdPtr->hPtr = hPtr;
	    cmdPtr->nsPtr = iPtr->globalNsPtr;
	    cmdPtr->refCount = 1;
	    cmdPtr->cmdEpoch = 0;
	    cmdPtr->compileProc = cmdInfoPtr->compileProc;
	    if (cmdInfoPtr->proc == (Tcl_CmdProc *) NULL) {
		cmdPtr->proc = TclInvokeObjectCommand;
		cmdPtr->clientData = (ClientData) cmdPtr;
	    } else {
		cmdPtr->proc = cmdInfoPtr->proc;
		cmdPtr->clientData = (ClientData) NULL;
	    }
	    if (cmdInfoPtr->objProc == (Tcl_ObjCmdProc *) NULL) {
		cmdPtr->objProc = TclInvokeStringCommand;
		cmdPtr->objClientData = (ClientData) cmdPtr;
	    } else {
		cmdPtr->objProc = cmdInfoPtr->objProc;
		cmdPtr->objClientData = (ClientData) NULL;
	    }
	    cmdPtr->deleteProc = NULL;
	    cmdPtr->deleteData = (ClientData) NULL;
	    cmdPtr->deleted = 0;
	    cmdPtr->importRefPtr = NULL;
	    Tcl_SetHashValue(hPtr, cmdPtr);
	}
    }

#ifndef TCL_GENERIC_ONLY
    TclSetupEnv((Tcl_Interp *) iPtr);
#endif

    /*
     * Do Safe-Tcl init stuff
     */

    (void) TclInterpInit((Tcl_Interp *)iPtr);

    /*
     * Set up variables such as tcl_library and tcl_precision.
     */

    TclPlatformInit((Tcl_Interp *)iPtr);
    Tcl_SetVar((Tcl_Interp *) iPtr, "tcl_patchLevel", TCL_PATCH_LEVEL,
	    TCL_GLOBAL_ONLY);
    Tcl_SetVar((Tcl_Interp *) iPtr, "tcl_version", TCL_VERSION,
	    TCL_GLOBAL_ONLY);

    /*
     * Compute the byte order of this machine.
     */

    order.s = 1;
    Tcl_SetVar2((Tcl_Interp *) iPtr, "tcl_platform", "byteOrder",
	    (order.c[0] == 1) ? "litteEndian" : "bigEndian",
	    TCL_GLOBAL_ONLY);

    /*
     * Register Tcl's version number.
     */

    Tcl_PkgProvide((Tcl_Interp *) iPtr, "Tcl", TCL_VERSION);
    
    return (Tcl_Interp *) iPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHideUnsafeCommands --
 *
 *	Hides base commands that are not marked as safe from this
 *	interpreter.
 *
 * Results:
 *	TCL_OK if it succeeds, TCL_ERROR else.
 *
 * Side effects:
 *	Hides functionality in an interpreter.
 *
 *----------------------------------------------------------------------
 */

int
TclHideUnsafeCommands(interp)
    Tcl_Interp *interp;		/* Hide commands in this interpreter. */
{
    register CmdInfo *cmdInfoPtr;

    if (interp == (Tcl_Interp *) NULL) {
        return TCL_ERROR;
    }
    for (cmdInfoPtr = builtInCmds; cmdInfoPtr->name != NULL; cmdInfoPtr++) {
        if (!cmdInfoPtr->isSafe) {
            Tcl_HideCommand(interp, cmdInfoPtr->name, cmdInfoPtr->name);
        }
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CallWhenDeleted --
 *
 *	Arrange for a procedure to be called before a given
 *	interpreter is deleted. The procedure is called as soon
 *	as Tcl_DeleteInterp is called; if Tcl_CallWhenDeleted is
 *	called on an interpreter that has already been deleted,
 *	the procedure will be called when the last Tcl_Release is
 *	done on the interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When Tcl_DeleteInterp is invoked to delete interp,
 *	proc will be invoked.  See the manual entry for
 *	details.
 *
 *--------------------------------------------------------------
 */

void
Tcl_CallWhenDeleted(interp, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter to watch. */
    Tcl_InterpDeleteProc *proc;	/* Procedure to call when interpreter
				 * is about to be deleted. */
    ClientData clientData;	/* One-word value to pass to proc. */
{
    Interp *iPtr = (Interp *) interp;
    static int assocDataCounter = 0;
    int new;
    char buffer[128];
    AssocData *dPtr = (AssocData *) ckalloc(sizeof(AssocData));
    Tcl_HashEntry *hPtr;

    sprintf(buffer, "Assoc Data Key #%d", assocDataCounter);
    assocDataCounter++;

    if (iPtr->assocData == (Tcl_HashTable *) NULL) {
        iPtr->assocData = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(iPtr->assocData, TCL_STRING_KEYS);
    }
    hPtr = Tcl_CreateHashEntry(iPtr->assocData, buffer, &new);
    dPtr->proc = proc;
    dPtr->clientData = clientData;
    Tcl_SetHashValue(hPtr, dPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DontCallWhenDeleted --
 *
 *	Cancel the arrangement for a procedure to be called when
 *	a given interpreter is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If proc and clientData were previously registered as a
 *	callback via Tcl_CallWhenDeleted, they are unregistered.
 *	If they weren't previously registered then nothing
 *	happens.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DontCallWhenDeleted(interp, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter to watch. */
    Tcl_InterpDeleteProc *proc;	/* Procedure to call when interpreter
				 * is about to be deleted. */
    ClientData clientData;	/* One-word value to pass to proc. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashTable *hTablePtr;
    Tcl_HashSearch hSearch;
    Tcl_HashEntry *hPtr;
    AssocData *dPtr;

    hTablePtr = iPtr->assocData;
    if (hTablePtr == (Tcl_HashTable *) NULL) {
        return;
    }
    for (hPtr = Tcl_FirstHashEntry(hTablePtr, &hSearch); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&hSearch)) {
        dPtr = (AssocData *) Tcl_GetHashValue(hPtr);
        if ((dPtr->proc == proc) && (dPtr->clientData == clientData)) {
            ckfree((char *) dPtr);
            Tcl_DeleteHashEntry(hPtr);
            return;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetAssocData --
 *
 *	Creates a named association between user-specified data, a delete
 *	function and this interpreter. If the association already exists
 *	the data is overwritten with the new data. The delete function will
 *	be invoked when the interpreter is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the associated data, creates the association if needed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetAssocData(interp, name, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter to associate with. */
    char *name;			/* Name for association. */
    Tcl_InterpDeleteProc *proc;	/* Proc to call when interpreter is
                                 * about to be deleted. */
    ClientData clientData;	/* One-word value to pass to proc. */
{
    Interp *iPtr = (Interp *) interp;
    AssocData *dPtr;
    Tcl_HashEntry *hPtr;
    int new;

    if (iPtr->assocData == (Tcl_HashTable *) NULL) {
        iPtr->assocData = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(iPtr->assocData, TCL_STRING_KEYS);
    }
    hPtr = Tcl_CreateHashEntry(iPtr->assocData, name, &new);
    if (new == 0) {
        dPtr = (AssocData *) Tcl_GetHashValue(hPtr);
    } else {
        dPtr = (AssocData *) ckalloc(sizeof(AssocData));
    }
    dPtr->proc = proc;
    dPtr->clientData = clientData;

    Tcl_SetHashValue(hPtr, dPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteAssocData --
 *
 *	Deletes a named association of user-specified data with
 *	the specified interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the association.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteAssocData(interp, name)
    Tcl_Interp *interp;			/* Interpreter to associate with. */
    char *name;				/* Name of association. */
{
    Interp *iPtr = (Interp *) interp;
    AssocData *dPtr;
    Tcl_HashEntry *hPtr;

    if (iPtr->assocData == (Tcl_HashTable *) NULL) {
        return;
    }
    hPtr = Tcl_FindHashEntry(iPtr->assocData, name);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        return;
    }
    dPtr = (AssocData *) Tcl_GetHashValue(hPtr);
    if (dPtr->proc != NULL) {
        (dPtr->proc) (dPtr->clientData, interp);
    }
    ckfree((char *) dPtr);
    Tcl_DeleteHashEntry(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAssocData --
 *
 *	Returns the client data associated with this name in the
 *	specified interpreter.
 *
 * Results:
 *	The client data in the AssocData record denoted by the named
 *	association, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_GetAssocData(interp, name, procPtr)
    Tcl_Interp *interp;			/* Interpreter associated with. */
    char *name;				/* Name of association. */
    Tcl_InterpDeleteProc **procPtr;	/* Pointer to place to store address
					 * of current deletion callback. */
{
    Interp *iPtr = (Interp *) interp;
    AssocData *dPtr;
    Tcl_HashEntry *hPtr;

    if (iPtr->assocData == (Tcl_HashTable *) NULL) {
        return (ClientData) NULL;
    }
    hPtr = Tcl_FindHashEntry(iPtr->assocData, name);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        return (ClientData) NULL;
    }
    dPtr = (AssocData *) Tcl_GetHashValue(hPtr);
    if (procPtr != (Tcl_InterpDeleteProc **) NULL) {
        *procPtr = dPtr->proc;
    }
    return dPtr->clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteInterpProc --
 *
 *	Helper procedure to delete an interpreter. This procedure is
 *	called when the last call to Tcl_Preserve on this interpreter
 *	is matched by a call to Tcl_Release. The procedure cleans up
 *	all resources used in the interpreter and calls all currently
 *	registered interpreter deletion callbacks.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Whatever the interpreter deletion callbacks do. Frees resources
 *	used by the interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteInterpProc(interp)
    Tcl_Interp *interp;			/* Interpreter to delete. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_HashTable *hTablePtr;
    AssocData *dPtr;
    int i;

    /*
     * Punt if there is an error in the Tcl_Release/Tcl_Preserve matchup.
     */
    
    if (iPtr->numLevels > 0) {
        panic("DeleteInterpProc called with active evals");
    }

    /*
     * The interpreter should already be marked deleted; otherwise how
     * did we get here?
     */

    if (!(iPtr->flags & DELETED)) {
        panic("DeleteInterpProc called on interpreter not marked deleted");
    }

    /*
     * Dismantle everything in the global namespace except for the
     * "errorInfo" and "errorCode" variables. These remain until the
     * namespace is actually destroyed, in case any errors occur.
     *   
     * Dismantle the namespace here, before we clear the assocData. If any
     * background errors occur here, they will be deleted below.
     */
    
    TclTeardownNamespace(iPtr->globalNsPtr);

    /*
     * Tear down the math function table.
     */

    for (hPtr = Tcl_FirstHashEntry(&iPtr->mathFuncTable, &search);
	     hPtr != NULL;
             hPtr = Tcl_NextHashEntry(&search)) {
	ckfree((char *) Tcl_GetHashValue(hPtr));
    }
    Tcl_DeleteHashTable(&iPtr->mathFuncTable);

    /*
     * Invoke deletion callbacks; note that a callback can create new
     * callbacks, so we iterate.
     */

    while (iPtr->assocData != (Tcl_HashTable *) NULL) {
        hTablePtr = iPtr->assocData;
        iPtr->assocData = (Tcl_HashTable *) NULL;
        for (hPtr = Tcl_FirstHashEntry(hTablePtr, &search);
                 hPtr != NULL;
                 hPtr = Tcl_FirstHashEntry(hTablePtr, &search)) {
            dPtr = (AssocData *) Tcl_GetHashValue(hPtr);
            Tcl_DeleteHashEntry(hPtr);
            if (dPtr->proc != NULL) {
                (*dPtr->proc)(dPtr->clientData, interp);
            }
            ckfree((char *) dPtr);
        }
        Tcl_DeleteHashTable(hTablePtr);
        ckfree((char *) hTablePtr);
    }

    /*
     * Finish deleting the global namespace.
     */
    
    Tcl_DeleteNamespace((Tcl_Namespace *) iPtr->globalNsPtr);

    /*
     * Free up the result *after* deleting variables, since variable
     * deletion could have transferred ownership of the result string
     * to Tcl.
     */

    Tcl_FreeResult(interp);
    interp->result = NULL;
    Tcl_DecrRefCount(iPtr->objResultPtr);
    iPtr->objResultPtr = NULL;
    if (iPtr->errorInfo != NULL) {
	ckfree(iPtr->errorInfo);
        iPtr->errorInfo = NULL;
    }
    if (iPtr->errorCode != NULL) {
	ckfree(iPtr->errorCode);
        iPtr->errorCode = NULL;
    }
    if (iPtr->events != NULL) {
	for (i = 0; i < iPtr->numEvents; i++) {
	    ckfree(iPtr->events[i].command);
	}
	ckfree((char *) iPtr->events);
        iPtr->events = NULL;
    }
    while (iPtr->revPtr != NULL) {
	HistoryRev *nextPtr = iPtr->revPtr->nextPtr;

	ckfree(iPtr->revPtr->newBytes);
	ckfree((char *) iPtr->revPtr);
	iPtr->revPtr = nextPtr;
    }
    if (iPtr->appendResult != NULL) {
	ckfree(iPtr->appendResult);
        iPtr->appendResult = NULL;
    }
    for (i = 0; i < NUM_REGEXPS; i++) {
	if (iPtr->patterns[i] == NULL) {
	    break;
	}
	ckfree(iPtr->patterns[i]);
	ckfree((char *) iPtr->regexps[i]);
        iPtr->regexps[i] = NULL;
    }
    TclFreePackageInfo(iPtr);
    while (iPtr->tracePtr != NULL) {
	Trace *nextPtr = iPtr->tracePtr->nextPtr;

	ckfree((char *) iPtr->tracePtr);
	iPtr->tracePtr = nextPtr;
    }
    if (iPtr->execEnvPtr != NULL) {
	TclDeleteExecEnv(iPtr->execEnvPtr);
    }
    Tcl_DecrRefCount(iPtr->emptyObjPtr);
    iPtr->emptyObjPtr = NULL;
    
    ckfree((char *) iPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InterpDeleted --
 *
 *	Returns nonzero if the interpreter has been deleted with a call
 *	to Tcl_DeleteInterp.
 *
 * Results:
 *	Nonzero if the interpreter is deleted, zero otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_InterpDeleted(interp)
    Tcl_Interp *interp;
{
    return (((Interp *) interp)->flags & DELETED) ? 1 : 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteInterp --
 *
 *	Ensures that the interpreter will be deleted eventually. If there
 *	are no Tcl_Preserve calls in effect for this interpreter, it is
 *	deleted immediately, otherwise the interpreter is deleted when
 *	the last Tcl_Preserve is matched by a call to Tcl_Release. In either
 *	case, the procedure runs the currently registered deletion callbacks. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The interpreter is marked as deleted. The caller may still use it
 *	safely if there are calls to Tcl_Preserve in effect for the
 *	interpreter, but further calls to Tcl_Eval etc in this interpreter
 *	will fail.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteInterp(interp)
    Tcl_Interp *interp;		/* Token for command interpreter (returned
				 * by a previous call to Tcl_CreateInterp). */
{
    Interp *iPtr = (Interp *) interp;

    /*
     * If the interpreter has already been marked deleted, just punt.
     */

    if (iPtr->flags & DELETED) {
        return;
    }
    
    /*
     * Mark the interpreter as deleted. No further evals will be allowed.
     */

    iPtr->flags |= DELETED;

    /*
     * Ensure that the interpreter is eventually deleted.
     */

    Tcl_EventuallyFree((ClientData) interp,
            (Tcl_FreeProc *) DeleteInterpProc);
}

/*
 *----------------------------------------------------------------------
 *
 * HiddenCmdsDeleteProc --
 *
 *	Called on interpreter deletion to delete all the hidden
 *	commands in an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees up memory.
 *
 *----------------------------------------------------------------------
 */

static void
HiddenCmdsDeleteProc(clientData, interp)
    ClientData clientData;		/* The hidden commands hash table. */
    Tcl_Interp *interp;			/* The interpreter being deleted. */
{
    Tcl_HashTable *hiddenCmdTblPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    Command *cmdPtr;

    hiddenCmdTblPtr = (Tcl_HashTable *) clientData;
    hPtr = Tcl_FindHashEntry(hiddenCmdTblPtr, "tkerror");
    if (hPtr != NULL) {
	Tcl_DeleteHashEntry(hPtr);
    }
    for (hPtr = Tcl_FirstHashEntry(hiddenCmdTblPtr, &hSearch);
	     hPtr != NULL;
             hPtr = Tcl_FirstHashEntry(hiddenCmdTblPtr, &hSearch)) {

        /*
         * Cannot use Tcl_DeleteCommand because (a) the command is not
         * in the command hash table, and (b) that table has already been
         * deleted above. Hence we emulate what it does, below.
         */
        
        cmdPtr = (Command *) Tcl_GetHashValue(hPtr);

	/*
         * The code here is tricky.  We can't delete the hash table entry
         * before invoking the deletion callback because there are cases
         * where the deletion callback needs to invoke the command (e.g.
         * object systems such as OTcl).  However, this means that the
         * callback could try to delete or rename the command.  The deleted
         * flag allows us to detect these cases and skip nested deletes.
         */

        if (cmdPtr->deleted) {

	    /*
             * Another deletion is already in progress.  Remove the hash
             * table entry now, but don't invoke a callback or free the
             * command structure.
             */

            Tcl_DeleteHashEntry(cmdPtr->hPtr);
            cmdPtr->hPtr = NULL;
            continue;
        }
        cmdPtr->deleted = 1;
        if (cmdPtr->deleteProc != NULL) {
            (*cmdPtr->deleteProc)(cmdPtr->deleteData);
        }

	/*
	 * Bump the command epoch counter. This will invalidate all cached
         * references that refer to this command.
	 */
	
        cmdPtr->cmdEpoch++;

	/*
         * Don't use hPtr to delete the hash entry here, because it's
         * possible that the deletion callback renamed the command.
         * Instead, use cmdPtr->hptr, and make sure that no-one else
         * has already deleted the hash entry.
         */

        if (cmdPtr->hPtr != NULL) {
            Tcl_DeleteHashEntry(cmdPtr->hPtr);
        }
        ckfree((char *) cmdPtr);
    }
    Tcl_DeleteHashTable(hiddenCmdTblPtr);
    ckfree((char *) hiddenCmdTblPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_HideCommand --
 *
 *	Makes a command hidden so that it cannot be invoked from within
 *	an interpreter, only from within an ancestor.
 *
 * Results:
 *	A standard Tcl result; also leaves a message in interp->result
 *	if an error occurs.
 *
 * Side effects:
 *	Moves a command from the command table to the hidden command
 *	table.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_HideCommand(interp, cmdName, hiddenCmdName)
    Tcl_Interp *interp;		/* Interpreter in which to hide command. */
    char *cmdName;		/* Name of hidden command. */
    char *hiddenCmdName;	/* Name of to-be-hidden command. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Command cmd;
    Command *cmdPtr;
    Tcl_HashTable *hTblPtr;
    Tcl_HashEntry *hPtr, *tkErrorHPtr;
    int isBgerror, new;

    if (iPtr->flags & DELETED) {

        /*
         * The interpreter is being deleted. Do not create any new
         * structures, because it is not safe to modify the interpreter.
         */
        
        return TCL_ERROR;
    }

    if (strstr(hiddenCmdName, "::") != NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "hidden command names can't have namespace qualifiers",
                (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * Find the command to hide. An error is returned if cmdName can't
     * be found.
     */

    cmd = Tcl_FindCommand(interp, cmdName, (Tcl_Namespace *) NULL,
	    /*flags*/ TCL_LEAVE_ERR_MSG);
    if (cmd == (Tcl_Command) NULL) {
	return TCL_ERROR;
    }
    cmdPtr = (Command *) cmd;

    /*
     * If this command is the "bgerror" command in the global namespace,
     * make note of it now. We'll need to know this later so that we can
     * handle its "tkerror" twin below.
     */
    
    isBgerror = 0;
    if (cmdPtr->hPtr != NULL) {
        char *tail = Tcl_GetHashKey(cmdPtr->hPtr->tablePtr, cmdPtr->hPtr);
        if ((*tail == 'b') && (strcmp(tail, "bgerror") == 0)
	        && (cmdPtr->nsPtr == iPtr->globalNsPtr)) {
            isBgerror = 1;
        }
    }
    
    /*
     * Initialize the hidden command table if necessary.
     */

    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclHiddenCmds",
            NULL);
    if (hTblPtr == (Tcl_HashTable *) NULL) {
        hTblPtr = (Tcl_HashTable *)
	        ckalloc((unsigned) sizeof(Tcl_HashTable));
        Tcl_InitHashTable(hTblPtr, TCL_STRING_KEYS);
        Tcl_SetAssocData(interp, "tclHiddenCmds", HiddenCmdsDeleteProc,
                (ClientData) hTblPtr);
    }

    /*
     * It is an error to move an exposed command to a hidden command with
     * hiddenCmdName if a hidden command with the name hiddenCmdName already
     * exists.
     */
    
    hPtr = Tcl_CreateHashEntry(hTblPtr, hiddenCmdName, &new);
    if (!new) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "hidden command named \"", hiddenCmdName, "\" already exists",
                (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * Remove the hash entry for the command from the interpreter command
     * table. This is like deleting the command, so bump its command epoch;
     * this invalidates any cached references that point to the command.
     */

    if (cmdPtr->hPtr != NULL) {
        Tcl_DeleteHashEntry(cmdPtr->hPtr);
        cmdPtr->hPtr = (Tcl_HashEntry *) NULL;
	cmdPtr->cmdEpoch++;
    }

    /*
     * If we are creating a hidden command named "bgerror", share the
     * command data structure with another command named "tkerror". This
     * code should eventually be removed.
     */

    if (isBgerror) {
        tkErrorHPtr = Tcl_CreateHashEntry(hTblPtr, "tkerror", &new);
        if (!new) {
            panic("Tcl_HideCommand: hiding bgerror while tkerror is already hidden!");
        }
        Tcl_SetHashValue(tkErrorHPtr, (ClientData) cmdPtr);
        tkErrorHPtr = Tcl_FindHashEntry(&(iPtr->globalNsPtr->cmdTable),
                "tkerror");
        if (tkErrorHPtr != (Tcl_HashEntry *) NULL) {
            Tcl_DeleteHashEntry(tkErrorHPtr);
        }
    }

    /*
     * Now link the hash table entry with the command structure. Keep the
     * containing namespace the same. After all, the command really
     * "belongs" to that namespace.
     */
    
    cmdPtr->hPtr = hPtr;
    Tcl_SetHashValue(hPtr, (ClientData) cmdPtr);

    /*
     * If the command being hidden has a compile procedure, increment the
     * interpreter's compileEpoch to invalidate its compiled code. This
     * makes sure that we don't later try to execute old code compiled with
     * command-specific (i.e., inline) bytecodes for the now-hidden
     * command. This field is checked in Tcl_EvalObj and ObjInterpProc,
     * and code whose compilation epoch doesn't match is recompiled.
     */

    if (cmdPtr->compileProc != NULL) {
	iPtr->compileEpoch++;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExposeCommand --
 *
 *	Makes a previously hidden command callable from inside the
 *	interpreter instead of only by its ancestors.
 *
 * Results:
 *	A standard Tcl result. If an error occurs, a message is left
 *	in interp->result.
 *
 * Side effects:
 *	Moves commands from one hash table to another.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ExposeCommand(interp, hiddenCmdName, cmdName)
    Tcl_Interp *interp;		/* Interpreter in which to make command
                                 * callable. */
    char *hiddenCmdName;	/* Name of hidden command. */
    char *cmdName;		/* Name of to-be-exposed command. */
{
    Interp *iPtr = (Interp *) interp;
    Command *cmdPtr;
    Namespace *nsPtr, *dummy1, *dummy2;
    Tcl_HashEntry *hPtr, *tkErrorHPtr;
    Tcl_HashTable *hTblPtr;
    char *tail;
    int new, result;

    if (iPtr->flags & DELETED) {
        /*
         * The interpreter is being deleted. Do not create any new
         * structures, because it is not safe to modify the interpreter.
         */
        
        return TCL_ERROR;
    }

    /*
     * Find the hash table for the hidden commands; error out if there
     * is none.
     */

    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclHiddenCmds",
            NULL);
    if (hTblPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "unknown hidden command \"", hiddenCmdName,
                "\"", (char *) NULL);
        return TCL_ERROR;
    }
        
    /*
     * Get the command from the hidden command table:
     */

    hPtr = Tcl_FindHashEntry(hTblPtr, hiddenCmdName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "unknown hidden command \"", hiddenCmdName,
                "\"", (char *) NULL);
        return TCL_ERROR;
    }
    cmdPtr = (Command *) Tcl_GetHashValue(hPtr);

    /*
     * Normally, the command will go right back into its containing
     * namespace. But if the exposed command name has "::" namespace
     * qualifiers, it is being moved to another context.
     */
    
    if (strstr(cmdName, "::") != NULL) {
	result = TclGetNamespaceForQualName(interp, cmdName,
		iPtr->globalNsPtr,
		(CREATE_NS_IF_UNKNOWN | TCL_LEAVE_ERR_MSG),
                &nsPtr, &dummy1, &dummy2, &tail);
        if (result != TCL_OK) {
            return result;
        }
        if ((nsPtr == NULL) || (tail == NULL)) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "bad command name \"", cmdName, "\"", (char *) NULL);
            return TCL_ERROR;
        }
    } else {
        nsPtr = cmdPtr->nsPtr;
        tail = cmdName;
    }

    /*
     * It is an error to overwrite an existing exposed command as a result
     * of exposing a previously hidden command.
     */

    hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, tail, &new);
    if (!new) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "exposed command \"", cmdName,
                "\" already exists", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * Remove the hash entry for the command from the interpreter hidden
     * command table.
     */

    if (cmdPtr->hPtr != NULL) {
        Tcl_DeleteHashEntry(cmdPtr->hPtr);
        cmdPtr->hPtr = NULL;
    }

    /*
     * If we are creating a command named "bgerror", share the command
     * data structure with another command named "tkerror". This code
     * should eventually be removed.
     */

    if ((*tail == 'b') && (strcmp(tail, "bgerror") == 0)
	    && (nsPtr == iPtr->globalNsPtr)) {
        tkErrorHPtr = Tcl_CreateHashEntry(&iPtr->globalNsPtr->cmdTable,
                "tkerror", &new);
        if (!new) {
            panic("Tcl_ExposeCommand: exposing bgerror while tkerror is already exposed!");
        }
        Tcl_SetHashValue(tkErrorHPtr, (ClientData) cmdPtr);
        tkErrorHPtr = Tcl_FindHashEntry(hTblPtr, "tkerror");
        if (tkErrorHPtr != NULL) {
            Tcl_DeleteHashEntry(tkErrorHPtr);
        }
    }

    /*
     * Now link the hash table entry with the command structure.
     * This is like creating a new command, so deal with any shadowing
     * of commands in the global namespace.
     */
    
    cmdPtr->hPtr = hPtr;
    cmdPtr->nsPtr = nsPtr;
    Tcl_SetHashValue(hPtr, (ClientData) cmdPtr);
    TclResetShadowedCmdRefs(interp, cmdPtr);

    /*
     * If the command being exposed has a compile procedure, increment
     * interpreter's compileEpoch to invalidate its compiled code. This
     * makes sure that we don't later try to execute old code compiled
     * assuming the command is hidden. This field is checked in Tcl_EvalObj
     * and ObjInterpProc, and code whose compilation epoch doesn't match is
     * recompiled.
     */

    if (cmdPtr->compileProc != NULL) {
	iPtr->compileEpoch++;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateCommand --
 *
 *	Define a new command in a command table.
 *
 * Results:
 *	The return value is a token for the command, which can
 *	be used in future calls to Tcl_GetCommandName.
 *
 * Side effects:
 *	If a command named cmdName already exists for interp, it is deleted.
 *	In the future, when cmdName is seen as the name of a command by
 *	Tcl_Eval, proc will be called. To support the bytecode interpreter,
 *	the command is created with a wrapper Tcl_ObjCmdProc
 *	(TclInvokeStringCommand) that eventially calls proc. When the
 *	command is deleted from the table, deleteProc will be called.
 *	See the manual entry for details on the calling sequence.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
Tcl_CreateCommand(interp, cmdName, proc, clientData, deleteProc)
    Tcl_Interp *interp;		/* Token for command interpreter returned by
				 * a previous call to Tcl_CreateInterp. */
    char *cmdName;		/* Name of command. If it contains namespace
				 * qualifiers, the new command is put in the
				 * specified namespace; otherwise it is put
				 * in the global namespace. */
    Tcl_CmdProc *proc;		/* Procedure to associate with cmdName. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* If not NULL, gives a procedure to call
				 * when this command is deleted. */
{
    Interp *iPtr = (Interp *) interp;
    Namespace *nsPtr, *dummy1, *dummy2;
    Command *cmdPtr;
    Tcl_HashEntry *hPtr;
    char *tail;
    int new, result;

    if (iPtr->flags & DELETED) {
	/*
	 * The interpreter is being deleted.  Don't create any new
	 * commands; it's not safe to muck with the interpreter anymore.
	 */

	return (Tcl_Command) NULL;
    }

    /*
     * Determine where the command should reside. If its name contains 
     * namespace qualifiers, we put it in the specified namespace; 
     * otherwise, we always put it in the global namespace.
     */

    if (strstr(cmdName, "::") != NULL) {
	result = TclGetNamespaceForQualName(interp, cmdName, 
                (Namespace *) NULL, CREATE_NS_IF_UNKNOWN, &nsPtr, 
                &dummy1, &dummy2, &tail);
	if ((result != TCL_OK) || (nsPtr == NULL) || (tail == NULL)) {
	    return (Tcl_Command) NULL;
	}
    } else {
	nsPtr = iPtr->globalNsPtr;
	tail = cmdName;
    }
    
    /*
     * The code below was added in 11/95 to preserve backwards compatibility
     * when "tkerror" was renamed "bgerror":  if anyone attempts to define
     * "tkerror" as a command, it is actually created as "bgerror".  This
     * code should eventually be removed.
     */

    if ((*tail == 't') && (strcmp(tail, "tkerror") == 0)
	    && (nsPtr == iPtr->globalNsPtr)) {
	tail = "bgerror";
    }

    hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, tail, &new);
    if (!new) {
	/*
	 * Command already exists. Delete the old one.
	 */

	cmdPtr = (Command *) Tcl_GetHashValue(hPtr);
	Tcl_DeleteCommandFromToken(interp, (Tcl_Command) cmdPtr);
	hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, tail, &new);
	if (!new) {
	    /*
	     * If the deletion callback recreated the command, just throw
             * away the new command (if we try to delete it again, we
             * could get stuck in an infinite loop).
	     */

	     ckfree((char*) cmdPtr);
	}
    }
    cmdPtr = (Command *) ckalloc(sizeof(Command));
    Tcl_SetHashValue(hPtr, cmdPtr);
    cmdPtr->hPtr = hPtr;
    cmdPtr->nsPtr = nsPtr;
    cmdPtr->refCount = 1;
    cmdPtr->cmdEpoch = 0;
    cmdPtr->compileProc = (CompileProc *) NULL;
    cmdPtr->objProc = TclInvokeStringCommand;
    cmdPtr->objClientData = (ClientData) cmdPtr;
    cmdPtr->proc = proc;
    cmdPtr->clientData = clientData;
    cmdPtr->deleteProc = deleteProc;
    cmdPtr->deleteData = clientData;
    cmdPtr->deleted = 0;
    cmdPtr->importRefPtr = NULL;

    /*
     * The code below provides more backwards compatibility for the
     * renaming of "tkerror" to "bgerror". Like the code above, this
     * code should eventually become unnecessary.
     */

    if ((*tail == 'b') && (strcmp(tail, "bgerror") == 0)
	    && (nsPtr == iPtr->globalNsPtr)) {
	/*
	 * We're currently creating the "bgerror" command; create
	 * a "tkerror" command that shares the same Command structure.
	 */

	hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, "tkerror", &new);
	Tcl_SetHashValue(hPtr, cmdPtr);
    }

    /*
     * We just created a command, so in its namespace and all of its parent
     * namespaces, it may shadow global commands with the same name. If any
     * shadowed commands are found, invalidate all cached command references
     * in the affected namespaces.
     */
    
    TclResetShadowedCmdRefs(interp, cmdPtr);
    return (Tcl_Command) cmdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateObjCommand --
 *
 *	Define a new object-based command in a command table.
 *
 * Results:
 *	The return value is a token for the command, which can
 *	be used in future calls to Tcl_NameOfCommand.
 *
 * Side effects:
 *	If no command named "cmdName" already exists for interp, one is
 *	created. Otherwise, if a command does exist, then if the
 *	object-based Tcl_ObjCmdProc is TclInvokeStringCommand, we assume
 *	Tcl_CreateCommand was called previously for the same command and
 *	just set its Tcl_ObjCmdProc to the argument "proc"; otherwise, we
 *	delete the old command.
 *
 *	In the future, during bytecode evaluation when "cmdName" is seen as
 *	the name of a command by Tcl_EvalObj or Tcl_Eval, the object-based
 *	Tcl_ObjCmdProc proc will be called. When the command is deleted from
 *	the table, deleteProc will be called. See the manual entry for
 *	details on the calling sequence.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
Tcl_CreateObjCommand(interp, cmdName, proc, clientData, deleteProc)
    Tcl_Interp *interp;		/* Token for command interpreter (returned
				 * by previous call to Tcl_CreateInterp). */
    char *cmdName;		/* Name of command. If it contains namespace
				 * qualifiers, the new command is put in the
				 * specified namespace; otherwise it is put
				 * in the global namespace. */
    Tcl_ObjCmdProc *proc;	/* Object-based procedure to associate with
				 * name. */
    ClientData clientData;	/* Arbitrary value to pass to object
    				 * procedure. */
    Tcl_CmdDeleteProc *deleteProc;
				/* If not NULL, gives a procedure to call
				 * when this command is deleted. */
{
    Interp *iPtr = (Interp *) interp;
    Namespace *nsPtr, *dummy1, *dummy2;
    Command *cmdPtr;
    Tcl_HashEntry *hPtr;
    char *tail;
    int new, result;

    if (iPtr->flags & DELETED) {
	/*
	 * The interpreter is being deleted.  Don't create any new
	 * commands;  it's not safe to muck with the interpreter anymore.
	 */

	return (Tcl_Command) NULL;
    }

    /*
     * Determine where the command should reside. If its name contains 
     * namespace qualifiers, we put it in the specified namespace; 
     * otherwise, we always put it in the global namespace.
     */

    if (strstr(cmdName, "::") != NULL) {
	result = TclGetNamespaceForQualName(interp, cmdName, 
                (Namespace *) NULL, CREATE_NS_IF_UNKNOWN, &nsPtr, 
                &dummy1, &dummy2, &tail);
	if ((result != TCL_OK) || (nsPtr == NULL) || (tail == NULL)) {
	    return (Tcl_Command) NULL;
	}
    } else {
	nsPtr = iPtr->globalNsPtr;
	tail = cmdName;
    }

    /*
     * The code below was added in 11/95 to preserve backwards compatibility
     * when "tkerror" was renamed "bgerror":  if anyone attempts to define
     * "tkerror" as a command, it is actually created as "bgerror".  This
     * code should eventually be removed.
     */

    if ((*tail == 't') && (strcmp(tail, "tkerror") == 0)
	    && (nsPtr == iPtr->globalNsPtr)) {
	tail = "bgerror";
    }

    hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, tail, &new);
    if (!new) {
	cmdPtr = (Command *) Tcl_GetHashValue(hPtr);

	/*
	 * Command already exists. If its object-based Tcl_ObjCmdProc is
	 * TclInvokeStringCommand, we just set its Tcl_ObjCmdProc to the
	 * argument "proc". Otherwise, we delete the old command. 
	 */

	if (cmdPtr->objProc == TclInvokeStringCommand) {
	    cmdPtr->objProc = proc;
	    cmdPtr->objClientData = clientData;
            cmdPtr->deleteProc = deleteProc;
            cmdPtr->deleteData = clientData;
	    goto checkForBgerror;
	}

	Tcl_DeleteCommandFromToken(interp, (Tcl_Command) cmdPtr);
	hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, tail, &new);
	if (!new) {
	    /*
	     * If the deletion callback recreated the command, just throw
	     * away the new command (if we try to delete it again, we
	     * could get stuck in an infinite loop).
	     */

	     ckfree((char *) Tcl_GetHashValue(hPtr));
	}
    }
    cmdPtr = (Command *) ckalloc(sizeof(Command));
    Tcl_SetHashValue(hPtr, cmdPtr);
    cmdPtr->hPtr = hPtr;
    cmdPtr->nsPtr = nsPtr;
    cmdPtr->refCount = 1;
    cmdPtr->cmdEpoch = 0;
    cmdPtr->compileProc = (CompileProc *) NULL;
    cmdPtr->objProc = proc;
    cmdPtr->objClientData = clientData;
    cmdPtr->proc = TclInvokeObjectCommand;
    cmdPtr->clientData = (ClientData) cmdPtr;
    cmdPtr->deleteProc = deleteProc;
    cmdPtr->deleteData = clientData;
    cmdPtr->deleted = 0;
    cmdPtr->importRefPtr = NULL;
    
    /*
     * The code below provides more backwards compatibility for the
     * renaming of "tkerror" to "bgerror".  Like the code above, this
     * code should eventually become unnecessary.
     */

    checkForBgerror:
    if ((*tail == 'b') && (strcmp(tail, "bgerror") == 0)
	    && (nsPtr == iPtr->globalNsPtr)) {
	/*
	 * We're currently creating the "bgerror" command; create
	 * a "tkerror" command that shares the same Command structure.
	 */

	hPtr = Tcl_CreateHashEntry(&nsPtr->cmdTable, "tkerror", &new);
	Tcl_SetHashValue(hPtr, cmdPtr);
    }
    return (Tcl_Command) cmdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInvokeStringCommand --
 *
 *	"Wrapper" Tcl_ObjCmdProc used to call an existing string-based
 *	Tcl_CmdProc if no object-based procedure exists for a command. A
 *	pointer to this procedure is stored as the Tcl_ObjCmdProc in a
 *	Command structure. It simply turns around and calls the string
 *	Tcl_CmdProc in the Command structure.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	Besides those side effects of the called Tcl_CmdProc,
 *	TclInvokeStringCommand allocates and frees storage.
 *
 *----------------------------------------------------------------------
 */

int
TclInvokeStringCommand(clientData, interp, objc, objv)
    ClientData clientData;	/* Points to command's Command structure. */
    Tcl_Interp *interp;		/* Current interpreter. */
    register int objc;		/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Command *cmdPtr = (Command *) clientData;
    register int i;
    int result;

    /*
     * This procedure generates an argv array for the string arguments. It
     * starts out with stack-allocated space but uses dynamically-allocated
     * storage if needed.
     */

#define NUM_ARGS 20
    char *(argStorage[NUM_ARGS]);
    char **argv = argStorage;

    /*
     * Create the string argument array "argv". Make sure argv is large
     * enough to hold the objc arguments plus 1 extra for the zero
     * end-of-argv word.
     * THIS FAILS IF ANY ARGUMENT OBJECT CONTAINS AN EMBEDDED NULL.
     */

    if ((objc + 1) > NUM_ARGS) {
	argv = (char **) ckalloc((unsigned)(objc + 1) * sizeof(char *));
    }

    for (i = 0;  i < objc;  i++) {
	argv[i] = Tcl_GetStringFromObj(objv[i], (int *) NULL);
    }
    argv[objc] = 0;

    /*
     * Invoke the command's string-based Tcl_CmdProc.
     */

    result = (*cmdPtr->proc)(cmdPtr->clientData, interp, objc, argv);

    /*
     * Free the argv array if malloc'ed storage was used.
     */

    if (argv != argStorage) {
	ckfree((char *) argv);
    }
    return result;
#undef NUM_ARGS
}

/*
 *----------------------------------------------------------------------
 *
 * TclInvokeObjectCommand --
 *
 *	"Wrapper" Tcl_CmdProc used to call an existing object-based
 *	Tcl_ObjCmdProc if no string-based procedure exists for a command.
 *	A pointer to this procedure is stored as the Tcl_CmdProc in a
 *	Command structure. It simply turns around and calls the object
 *	Tcl_ObjCmdProc in the Command structure.
 *
 * Results:
 *	A standard Tcl string result value.
 *
 * Side effects:
 *	Besides those side effects of the called Tcl_CmdProc,
 *	TclInvokeStringCommand allocates and frees storage.
 *
 *----------------------------------------------------------------------
 */

int
TclInvokeObjectCommand(clientData, interp, argc, argv)
    ClientData clientData;	/* Points to command's Command structure. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    register char **argv;	/* Argument strings. */
{
    Command *cmdPtr = (Command *) clientData;
    register Tcl_Obj *objPtr;
    register int i;
    int length, result;

    /*
     * This procedure generates an objv array for object arguments that hold
     * the argv strings. It starts out with stack-allocated space but uses
     * dynamically-allocated storage if needed.
     */

#define NUM_ARGS 20
    Tcl_Obj *(argStorage[NUM_ARGS]);
    register Tcl_Obj **objv = argStorage;

    /*
     * Create the object argument array "objv". Make sure objv is large
     * enough to hold the objc arguments plus 1 extra for the zero
     * end-of-objv word.
     */

    if ((argc + 1) > NUM_ARGS) {
	objv = (Tcl_Obj **)
	    ckalloc((unsigned)(argc + 1) * sizeof(Tcl_Obj *));
    }

    for (i = 0;  i < argc;  i++) {
	length = strlen(argv[i]);
	TclNewObj(objPtr);
	TclInitStringRep(objPtr, argv[i], length);
	Tcl_IncrRefCount(objPtr);
	objv[i] = objPtr;
    }
    objv[argc] = 0;

    /*
     * Invoke the command's object-based Tcl_ObjCmdProc.
     */

    result = (*cmdPtr->objProc)(cmdPtr->objClientData, interp, argc, objv);

    /*
     * Move the interpreter's object result to the string result, 
     * then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULL BYTES.
     */

    Tcl_SetResult(interp,
	    TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	    TCL_VOLATILE);
    
    /*
     * Decrement the ref counts for the argument objects created above,
     * then free the objv array if malloc'ed storage was used.
     */

    for (i = 0;  i < argc;  i++) {
	objPtr = objv[i];
	Tcl_DecrRefCount(objPtr);
    }
    if (objv != argStorage) {
	ckfree((char *) objv);
    }
    return result;
#undef NUM_ARGS
}

/*
 *----------------------------------------------------------------------
 *
 * TclRenameCommand --
 *
 *      Called to give an existing Tcl command a different name. Both the
 *      old command name and the new command name can have "::" namespace
 *      qualifiers. If the new command has a different namespace context,
 *      the command is automatically moved to that namespace.
 *
 *      If the new command name is NULL or the null string, the command is
 *      deleted.
 *
 * Results:
 *      Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *      If anything goes wrong, an error message is returned in the
 *      interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

int
TclRenameCommand(interp, oldName, newName)
    Tcl_Interp *interp;                 /* Current interpreter. */
    char *oldName;                      /* Existing command name. */
    char *newName;                      /* New command name. */
{
    Interp *iPtr = (Interp *) interp;
    char *cmdTail, *newTail;
    Namespace *cmdNsPtr, *newNsPtr, *dummy1, *dummy2;
    Tcl_Command cmd;
    Command *cmdPtr;
    Tcl_HashEntry *hPtr, *oldHPtr;
    int new, isSrcBgerror, isDestBgerror, result;

    /*
     * Find the existing command. An error is returned if cmdName can't
     * be found.
     */

    cmd = Tcl_FindCommand(interp, oldName, (Tcl_Namespace *) NULL,
	/*flags*/ 0);
    cmdPtr = (Command *) cmd;
    if (cmdPtr == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "can't ",
                ((newName == NULL) || (*newName == '\0'))? "delete":"rename",
                " \"", oldName, "\": command doesn't exist", (char *) NULL);
	return TCL_ERROR;
    }
    cmdTail = Tcl_GetHashKey(cmdPtr->hPtr->tablePtr, cmdPtr->hPtr);
    cmdNsPtr = cmdPtr->nsPtr;

    /*
     * If the new command name is NULL or empty, delete the command. Do this
     * with Tcl_DeleteCommandFromToken, since we already have the command.
     */
    
    if ((newName == NULL) || (*newName == '\0')) {
	Tcl_DeleteCommandFromToken(interp, cmd);
	return TCL_OK;
    }

    /*
     * Make sure that the destination command does not already exist.
     * The rename operation is like creating a command, so we should
     * automatically create the containing namespaces just like
     * Tcl_CreateCommand would.
     */

    result = TclGetNamespaceForQualName(interp, newName, (Namespace *) NULL,
            (CREATE_NS_IF_UNKNOWN | TCL_LEAVE_ERR_MSG),
            &newNsPtr, &dummy1, &dummy2, &newTail);
    if (result != TCL_OK) {
        return result;
    }
    if ((newNsPtr == NULL) || (newTail == NULL)) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		 "can't rename to \"", newName, "\": bad command name",
    	    	 (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_FindHashEntry(&newNsPtr->cmdTable, newTail) != NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		 "can't rename to \"", newName,
		 "\": command already exists", (char *) NULL);
        return TCL_ERROR;
    }

    /*
     * The code below was added in 11/95 to preserve backwards compatibility
     * when "tkerror" was renamed "bgerror":  we guarantee that the hash
     * table entries for both commands refer to a single shared Command
     * structure.  This code should eventually become unnecessary.
     */

    if ((*cmdTail == 't') && (strcmp(cmdTail, "tkerror") == 0)
	    && (cmdNsPtr == iPtr->globalNsPtr)) {
	cmdTail = "bgerror";
    }
    isSrcBgerror = ((*cmdTail == 'b') && (strcmp(cmdTail, "bgerror") == 0)
	    && (cmdNsPtr == iPtr->globalNsPtr));

    if ((*newTail == 't') && (strcmp(newTail, "tkerror") == 0)
	    && (newNsPtr == iPtr->globalNsPtr)) {
	newTail = "bgerror";
    }
    isDestBgerror = ((*newTail == 'b') && (strcmp(newTail, "bgerror") == 0)
            && (newNsPtr == iPtr->globalNsPtr));

    /*
     * Put the command in the new namespace, so we can check for an alias
     * loop. Since we are adding a new command to a namespace, we must
     * handle any shadowing of the global commands that this might create.
     * Note that the renamed command has a different hashtable pointer than
     * it used to have. This allows the command caching code in tclExecute.c
     * to recognize that a command pointer it has cached for this command is
     * now invalid.
     */
    
    oldHPtr = cmdPtr->hPtr;
    hPtr = Tcl_CreateHashEntry(&newNsPtr->cmdTable, newTail, &new);
    Tcl_SetHashValue(hPtr, (ClientData) cmdPtr);
    cmdPtr->hPtr = hPtr;
    cmdPtr->nsPtr = newNsPtr;
    TclResetShadowedCmdRefs(interp, cmdPtr);

    /*
     * Everything is in place so we can check for an alias loop. If we
     * detect one, put everything back the way it was and report the error.
     */

    result = TclPreventAliasLoop(interp, interp, (Tcl_Command) cmdPtr);
    if (result != TCL_OK) {
        Tcl_DeleteHashEntry(cmdPtr->hPtr);
        cmdPtr->hPtr = oldHPtr;
        cmdPtr->nsPtr = cmdNsPtr;
        return result;
    }

    /*
     * The new command name is okay, so remove the command from its
     * current namespace. This is like deleting the command, so bump
     * the cmdEpoch to invalidate any cached references to the command.
     */
    
    Tcl_DeleteHashEntry(oldHPtr);
    cmdPtr->cmdEpoch++;

    /*
     * If the command being renamed has a compile procedure, increment the
     * interpreter's compileEpoch to invalidate its compiled code. This
     * makes sure that we don't later try to execute old code compiled for
     * the now-renamed command.
     */

    if (cmdPtr->compileProc != NULL) {
	iPtr->compileEpoch++;
    }

    /*
     * The code below provides more backwards compatibility for the
     * "tkerror" => "bgerror" renaming. As with the other compatibility
     * code above, it should eventually be removed.
     */

    if (isSrcBgerror) {
	/*
	 * The source command is "bgerror": delete the hash table entry for
	 * "tkerror" if it exists.
	 */
	
	hPtr = Tcl_FindHashEntry(&cmdNsPtr->cmdTable, "tkerror");
        if (hPtr != NULL) {
	    Tcl_DeleteHashEntry(hPtr);
        }
    }
    if (isDestBgerror) {
	/*
	 * The destination command is "bgerror"; create a "tkerror"
	 * command that shares the same Command structure.
	 */
	
	hPtr = Tcl_CreateHashEntry(&newNsPtr->cmdTable, "tkerror", &new);
	Tcl_SetHashValue(hPtr, (ClientData) cmdPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetCommandInfo --
 *
 *	Modifies various information about a Tcl command. Note that
 *	this procedure will not change a command's namespace; use
 *	Tcl_RenameCommand to do that. Also, the isNativeObjectProc
 *	member of *infoPtr is ignored.
 *
 * Results:
 *	If cmdName exists in interp, then the information at *infoPtr
 *	is stored with the command in place of the current information
 *	and 1 is returned. If the command doesn't exist then 0 is
 *	returned. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetCommandInfo(interp, cmdName, infoPtr)
    Tcl_Interp *interp;			/* Interpreter in which to look
					 * for command. */
    char *cmdName;			/* Name of desired command. */
    Tcl_CmdInfo *infoPtr;		/* Where to store information about
					 * command. */
{
    Tcl_Command cmd;
    Command *cmdPtr;

    cmd = Tcl_FindCommand(interp, cmdName, (Tcl_Namespace *) NULL,
            /*flags*/ 0);
    if (cmd == (Tcl_Command) NULL) {
	return 0;
    }

    /*
     * The isNativeObjectProc and nsPtr members of *infoPtr are ignored.
     */
    
    cmdPtr = (Command *) cmd;
    cmdPtr->proc = infoPtr->proc;
    cmdPtr->clientData = infoPtr->clientData;
    if (infoPtr->objProc == (Tcl_ObjCmdProc *) NULL) {
	cmdPtr->objProc = TclInvokeStringCommand;
	cmdPtr->objClientData = (ClientData) cmdPtr;
    } else {
	cmdPtr->objProc = infoPtr->objProc;
	cmdPtr->objClientData = infoPtr->objClientData;
    }
    cmdPtr->deleteProc = infoPtr->deleteProc;
    cmdPtr->deleteData = infoPtr->deleteData;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCommandInfo --
 *
 *	Returns various information about a Tcl command.
 *
 * Results:
 *	If cmdName exists in interp, then *infoPtr is modified to
 *	hold information about cmdName and 1 is returned.  If the
 *	command doesn't exist then 0 is returned and *infoPtr isn't
 *	modified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetCommandInfo(interp, cmdName, infoPtr)
    Tcl_Interp *interp;			/* Interpreter in which to look
					 * for command. */
    char *cmdName;			/* Name of desired command. */
    Tcl_CmdInfo *infoPtr;		/* Where to store information about
					 * command. */
{
    Tcl_Command cmd;
    Command *cmdPtr;

    cmd = Tcl_FindCommand(interp, cmdName, (Tcl_Namespace *) NULL,
            /*flags*/ 0);
    if (cmd == (Tcl_Command) NULL) {
	return 0;
    }

    /*
     * Set isNativeObjectProc 1 if objProc was registered by a call to
     * Tcl_CreateObjCommand. Otherwise set it to 0.
     */

    cmdPtr = (Command *) cmd;
    infoPtr->isNativeObjectProc =
	    (cmdPtr->objProc != TclInvokeStringCommand);
    infoPtr->objProc = cmdPtr->objProc;
    infoPtr->objClientData = cmdPtr->objClientData;
    infoPtr->proc = cmdPtr->proc;
    infoPtr->clientData = cmdPtr->clientData;
    infoPtr->deleteProc = cmdPtr->deleteProc;
    infoPtr->deleteData = cmdPtr->deleteData;
    infoPtr->namespacePtr = (Tcl_Namespace *) cmdPtr->nsPtr;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCommandName --
 *
 *	Given a token returned by Tcl_CreateCommand, this procedure
 *	returns the current name of the command (which may have changed
 *	due to renaming).
 *
 * Results:
 *	The return value is the name of the given command.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetCommandName(interp, command)
    Tcl_Interp *interp;		/* Interpreter containing the command. */
    Tcl_Command command;	/* Token for command returned by a previous
				 * call to Tcl_CreateCommand. The command
				 * must not have been deleted. */
{
    Command *cmdPtr = (Command *) command;

    if ((cmdPtr == NULL) || (cmdPtr->hPtr == NULL)) {

	/*
	 * This should only happen if command was "created" after the
	 * interpreter began to be deleted, so there isn't really any
	 * command. Just return an empty string.
	 */

	return "";
    }
    return Tcl_GetHashKey(cmdPtr->hPtr->tablePtr, cmdPtr->hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCommandFullName --
 *
 *	Given a token returned by, e.g., Tcl_CreateCommand or
 *	Tcl_FindCommand, this procedure appends to an object the command's
 *	full name, qualified by a sequence of parent namespace names. The
 *	command's fully-qualified name may have changed due to renaming.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The command's fully-qualified name is appended to the string
 *	representation of objPtr. 
 *
 *----------------------------------------------------------------------
 */

void
Tcl_GetCommandFullName(interp, command, objPtr)
    Tcl_Interp *interp;		/* Interpreter containing the command. */
    Tcl_Command command;	/* Token for command returned by a previous
				 * call to Tcl_CreateCommand. The command
				 * must not have been deleted. */
    Tcl_Obj *objPtr;		/* Points to the object onto which the
				 * command's full name is appended. */

{
    Interp *iPtr = (Interp *) interp;
    register Command *cmdPtr = (Command *) command;
    char *name;

    /*
     * Add the full name of the containing namespace, followed by the "::"
     * separator, and the command name.
     */

    if (cmdPtr != NULL) {
	if (cmdPtr->nsPtr != NULL) {
	    Tcl_AppendToObj(objPtr, cmdPtr->nsPtr->fullName, -1);
	    if (cmdPtr->nsPtr != iPtr->globalNsPtr) {
		Tcl_AppendToObj(objPtr, "::", 2);
	    }
	}
	if (cmdPtr->hPtr != NULL) {
	    name = Tcl_GetHashKey(cmdPtr->hPtr->tablePtr, cmdPtr->hPtr);
	    Tcl_AppendToObj(objPtr, name, -1);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteCommand --
 *
 *	Remove the given command from the given interpreter.
 *
 * Results:
 *	0 is returned if the command was deleted successfully.
 *	-1 is returned if there didn't exist a command by that name.
 *
 * Side effects:
 *	cmdName will no longer be recognized as a valid command for
 *	interp.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DeleteCommand(interp, cmdName)
    Tcl_Interp *interp;		/* Token for command interpreter (returned
				 * by a previous Tcl_CreateInterp call). */
    char *cmdName;		/* Name of command to remove. */
{
    Tcl_Command cmd;

    /*
     *  Find the desired command and delete it.
     */

    cmd = Tcl_FindCommand(interp, cmdName, (Tcl_Namespace *) NULL,
            /*flags*/ 0);
    if (cmd == (Tcl_Command) NULL) {
	return -1;
    }
    return Tcl_DeleteCommandFromToken(interp, cmd);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteCommandFromToken --
 *
 *	Removes the given command from the given interpreter. This procedure
 *	resembles Tcl_DeleteCommand, but takes a Tcl_Command token instead
 *	of a command name for efficiency.
 *
 * Results:
 *	0 is returned if the command was deleted successfully.
 *	-1 is returned if there didn't exist a command by that name.
 *
 * Side effects:
 *	The command specified by "cmd" will no longer be recognized as a
 *	valid command for "interp".
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DeleteCommandFromToken(interp, cmd)
    Tcl_Interp *interp;		/* Token for command interpreter returned by
				 * a previous call to Tcl_CreateInterp. */
    Tcl_Command cmd;            /* Token for command to delete. */
{
    Interp *iPtr = (Interp *) interp;
    Command *cmdPtr = (Command *) cmd;
    char *cmdName;
    int isBgerror;
    ImportRef *refPtr, *nextRefPtr;
    Tcl_Command importCmd;
    Tcl_HashEntry *tkErrorHPtr;

    cmdName = Tcl_GetHashKey(cmdPtr->hPtr->tablePtr, cmdPtr->hPtr);
    isBgerror = ((*cmdName == 'b') && (strcmp(cmdName, "bgerror") == 0)
	    && (cmdPtr->nsPtr == iPtr->globalNsPtr));

    /*
     * The code here is tricky.  We can't delete the hash table entry
     * before invoking the deletion callback because there are cases
     * where the deletion callback needs to invoke the command (e.g.
     * object systems such as OTcl). However, this means that the
     * callback could try to delete or rename the command. The deleted
     * flag allows us to detect these cases and skip nested deletes.
     */

    if (cmdPtr->deleted) {
	/*
	 * Another deletion is already in progress.  Remove the hash
	 * table entry now, but don't invoke a callback or free the
	 * command structure.
	 */

        Tcl_DeleteHashEntry(cmdPtr->hPtr);
	cmdPtr->hPtr = NULL;
	return 0;
    }

    /*
     * If the command being deleted has a compile procedure, increment the
     * interpreter's compileEpoch to invalidate its compiled code. This
     * makes sure that we don't later try to execute old code compiled with
     * command-specific (i.e., inline) bytecodes for the now-deleted
     * command. This field is checked in Tcl_EvalObj and ObjInterpProc, and
     * code whose compilation epoch doesn't match is recompiled.
     */

    if (cmdPtr->compileProc != NULL) {
        iPtr->compileEpoch++;
    }

    cmdPtr->deleted = 1;
    if (cmdPtr->deleteProc != NULL) {
	/*
	 * Delete the command's client data. If this was an imported command
	 * created when a command was imported into a namespace, this client
	 * data will be a pointer to a ImportedCmdData structure describing
	 * the "real" command that this imported command refers to.
	 */
	
	(*cmdPtr->deleteProc)(cmdPtr->deleteData);
    }

    /*
     * Bump the command epoch counter. This will invalidate all cached
     * references that point to this command.
     */
    
    cmdPtr->cmdEpoch++;

    /*
     * If this command was imported into other namespaces, then imported
     * commands were created that refer back to this command. Delete these
     * imported commands now.
     */

    for (refPtr = cmdPtr->importRefPtr;  refPtr != NULL;
            refPtr = nextRefPtr) {
	nextRefPtr = refPtr->nextPtr;
	importCmd = (Tcl_Command) refPtr->importedCmdPtr;
        Tcl_DeleteCommandFromToken(interp, importCmd);
    }

    /*
     * The code below provides more backwards compatibility for the
     * renaming of "tkerror" to "bgerror". Like the code above, this
     * code should eventually become unnecessary.
     */

    if (isBgerror) {
	/*
	 * When the "bgerror" command is deleted, delete "tkerror"
	 * as well.  It shared the same Command structure as "bgerror",
	 * so all we have to do is throw away the hash table entry.
         * NOTE: we have to be careful since tkerror may already have
         * been deleted before bgerror.
	 */

        tkErrorHPtr = Tcl_FindHashEntry(cmdPtr->hPtr->tablePtr,
            "tkerror");

        if (tkErrorHPtr != (Tcl_HashEntry *) NULL) {
            Tcl_DeleteHashEntry(tkErrorHPtr);
        }
    }

    /*
     * Don't use hPtr to delete the hash entry here, because it's
     * possible that the deletion callback renamed the command.
     * Instead, use cmdPtr->hptr, and make sure that no-one else
     * has already deleted the hash entry.
     */

    if (cmdPtr->hPtr != NULL) {
	Tcl_DeleteHashEntry(cmdPtr->hPtr);
    }

    /*
     * Mark the Command structure as no longer valid. This allows
     * TclExecuteByteCode to recognize when a Command has logically been
     * deleted and a pointer to this Command structure cached in a CmdName
     * object is invalid. TclExecuteByteCode will look up the command again
     * in the interpreter's command hashtable.
     */

    cmdPtr->objProc = NULL;

    /*
     * Now free the Command structure, unless there is another reference to
     * it from a CmdName Tcl object in some ByteCode code sequence. In that
     * case, delay the cleanup until all references are either discarded
     * (when a ByteCode is freed) or replaced by a new reference (when a
     * cached CmdName Command reference is found to be invalid and
     * TclExecuteByteCode looks up the command in the command hashtable).
     */
    
    TclCleanupCommand(cmdPtr);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCleanupCommand --
 *
 *	This procedure frees up a Command structure unless it is still
 *	referenced from an interpreter's command hashtable or from a CmdName
 *	Tcl object representing the name of a command in a ByteCode
 *	instruction sequence. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed unless a reference to the Command structure still
 *	exists. In that case the cleanup is delayed until the command is
 *	deleted or when the last ByteCode referring to it is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclCleanupCommand(cmdPtr)
    register Command *cmdPtr;	/* Points to the Command structure to
				 * be freed. */
{
    cmdPtr->refCount--;
    if (cmdPtr->refCount <= 0) {
	ckfree((char *) cmdPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Eval --
 *
 *	Execute a Tcl command in a string.
 *
 * Results:
 *	The return value is one of the return codes defined in tcl.h
 *	(such as TCL_OK), and interp->result contains a string value
 *	to supplement the return code. The value of interp->result
 *	will persist only until the next call to Tcl_Eval or Tcl_EvalObj:
 *	you must copy it or lose it!
 *
 * Side effects:
 *	The string is compiled to produce a ByteCode object that holds the
 *	command's bytecode instructions. However, this ByteCode object is
 *	lost after executing the command. The command's execution will
 *	almost certainly have side effects. interp->termOffset is set to the
 *	offset of the character in "string" just after the last one
 *	successfully compiled or executed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Eval(interp, string)
    Tcl_Interp *interp;		/* Token for command interpreter (returned
				 * by previous call to Tcl_CreateInterp). */
    char *string;		/* Pointer to TCL command to execute. */
{
    register Tcl_Obj *cmdPtr;
    int length = strlen(string);
    int result;

    if (length > 0) {
	/*
	 * Initialize a Tcl object from the command string.
	 */

	TclNewObj(cmdPtr);
	TclInitStringRep(cmdPtr, string, length);
	Tcl_IncrRefCount(cmdPtr);

	/*
	 * Compile and execute the bytecodes.
	 */
    
	result = Tcl_EvalObj(interp, cmdPtr);

	/*
	 * Move the interpreter's object result to the string result, 
	 * then reset the object result.
	 * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
	 */

	Tcl_SetResult(interp,
	        TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	        TCL_VOLATILE);

	/*
	 * Discard the Tcl object created to hold the command and its code.
	 */
	
	Tcl_DecrRefCount(cmdPtr);	
    } else {
	/*
	 * An empty string. Just reset the interpreter's result.
	 */

	Tcl_ResetResult(interp);
	result = TCL_OK;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EvalObj --
 *
 *	Execute Tcl commands stored in a Tcl object. These commands are
 *	compiled into bytecodes if necessary.
 *
 * Results:
 *	The return value is one of the return codes defined in tcl.h
 *	(such as TCL_OK), and the interpreter's result contains a value
 *	to supplement the return code.
 *
 * Side effects:
 *	The object is converted, if necessary, to a ByteCode object that
 *	holds the bytecode instructions for the commands. Executing the
 *	commands will almost certainly have side effects that depend
 *	on those commands.
 *
 *	Just as in Tcl_Eval, interp->termOffset is set to the offset of the
 *	last character executed in the objPtr's string.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_EvalObj(interp, objPtr)
    Tcl_Interp *interp;			/* Token for command interpreter
					 * (returned by a previous call to
					 * Tcl_CreateInterp). */
    Tcl_Obj *objPtr;			/* Pointer to object containing
					 * commands to execute. */
{
    register Interp *iPtr = (Interp *) interp;
    int flags;				/* Interp->evalFlags value when the
					 * procedure was called. */
    register ByteCode* codePtr;		/* Tcl Internal type of bytecode. */
    int oldCount = iPtr->cmdCount;	/* Used to tell whether any commands
					 * at all were executed. */
    int numSrcChars;
    register int result;

    /*
     * Reset both the interpreter's string and object results and clear out
     * any error information. This makes sure that we return an empty
     * result if there are no commands in the command string.
     */

    Tcl_ResetResult(interp);

    /*
     * Check depth of nested calls to Tcl_Eval:  if this gets too large,
     * it's probably because of an infinite loop somewhere.
     */

    iPtr->numLevels++;
    if (iPtr->numLevels > iPtr->maxNestingDepth) {
	iPtr->numLevels--;
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
		"too many nested calls to Tcl_EvalObj (infinite loop?)", -1);
	return TCL_ERROR;
    }

    /*
     * If the interpreter has been deleted, return an error.
     */
    
    if (iPtr->flags & DELETED) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "attempt to call eval in deleted interpreter", -1);
	Tcl_SetErrorCode(interp, "CORE", "IDELETE",
	        "attempt to call eval in deleted interpreter", (char *) NULL);
	iPtr->numLevels--;
	return TCL_ERROR;
    }

    /*
     * Get the ByteCode from the object. If it exists, make sure it hasn't
     * been invalidated by, e.g., someone redefining a command with a
     * compile procedure (this might make the compiled code wrong). If
     * necessary, convert the object to be a ByteCode object and compile it.
     * Also, if the code was compiled in/for a different interpreter,
     * we recompile it.
     */

    if (objPtr->typePtr == &tclByteCodeType) {
	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	
	if ((codePtr->iPtr != iPtr)
	        || (codePtr->compileEpoch != iPtr->compileEpoch)) {
	    tclByteCodeType.freeIntRepProc(objPtr);
	}
    }
    if (objPtr->typePtr != &tclByteCodeType) {
	/*
	 * First reset any error line number information.
	 */
	
	iPtr->errorLine = 1;   /* no correct line # information yet */
	result = tclByteCodeType.setFromAnyProc(interp, objPtr);
	if (result != TCL_OK) {
	    iPtr->numLevels--;
	    return result;
	}
    }
    codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;

    /*
     * Extract then reset the compilation flags in the interpreter.
     * Resetting the flags must be done after any compilation.
     */

    flags = iPtr->evalFlags;
    iPtr->evalFlags = 0;

    /*
     * Save information for the history module, if needed.
     * BTL: setting these NULL disables history revisions.
     */

    if (flags & TCL_RECORD_BOUNDS) {
	iPtr->evalFirst = NULL;
	iPtr->evalLast = NULL;
    }

    /*
     * Execute the commands. If the code was compiled from an empty string,
     * don't bother executing the code.
     */

    numSrcChars = codePtr->numSrcChars;
    if (numSrcChars > 0) {
	/*
	 * Increment the code's ref count while it is being executed. If
	 * afterwards no references to it remain, free the code.
	 */
	
	codePtr->refCount++;
	result = TclExecuteByteCode(interp, codePtr);
	codePtr->refCount--;
	if (codePtr->refCount <= 0) {
	    TclCleanupByteCode(codePtr);
	}
    } else {
	Tcl_ResetResult(interp);
	result = TCL_OK;
    }

    /*
     * If no commands at all were executed, check for asynchronous
     * handlers so that they at least get one change to execute.
     * This is needed to handle event loops written in Tcl with
     * empty bodies.
     */

    if ((oldCount == iPtr->cmdCount) && (Tcl_AsyncReady())) {
	result = Tcl_AsyncInvoke(interp, result);
    }

    /*
     * Free up any extra resources that were allocated.
     */

    iPtr->numLevels--;
    if (iPtr->numLevels == 0) {
	if (result == TCL_RETURN) {
	    result = TclUpdateReturnInfo(iPtr);
	}
	if ((result != TCL_OK) && (result != TCL_ERROR)
		&& !(flags & TCL_ALLOW_EXCEPTIONS)) {
	    Tcl_ResetResult(interp);
	    if (result == TCL_BREAK) {
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "invoked \"break\" outside of a loop", -1);
	    } else if (result == TCL_CONTINUE) {
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "invoked \"continue\" outside of a loop", -1);
	    } else {
		char buf[50];
		sprintf(buf, "command returned bad code: %d", result);
		Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	    }
	    result = TCL_ERROR;
	}
    }

    /*
     * If an error occurred, record information about what was being
     * executed when the error occurred.
     */

    if ((result == TCL_ERROR) && !(iPtr->flags & ERR_ALREADY_LOGGED)) {
	char buf[200];
	char *ellipsis = "";
	char *bytes;
	int length;

	/*
	 * Compute the line number where the error occurred.
	 * BTL: no line # information yet.
	 */

	iPtr->errorLine = 1;
#ifdef NOT_YET
	for (p = cmd; p != cmdStart; p++) {
	    if (*p == '\n') {
		iPtr->errorLine++;
	    }
	}
	for ( ; isspace(UCHAR(*p)) || (*p == ';'); p++) {
	    if (*p == '\n') {
		iPtr->errorLine++;
	    }
	}
#endif
	
	/*
	 * Figure out how much of the command to print in the error
	 * message (up to a certain number of characters, or up to
	 * the first new-line).
	 * THIS FAILS IF THE OBJECT'S STRING REP CONTAINS A NULL.
	 */

	bytes = Tcl_GetStringFromObj(objPtr, &length);
	length = TclMin(numSrcChars, length);
	if (length > 150) {
	    length = 150;
	    ellipsis = " ...";
	}

	if (!(iPtr->flags & ERR_IN_PROGRESS)) {
	    sprintf(buf, "\n    while executing\n\"%.*s%s\"",
		    length, bytes, ellipsis);
	} else {
	    sprintf(buf, "\n    invoked from within\n\"%.*s%s\"",
		    length, bytes, ellipsis);
	}
	Tcl_AddObjErrorInfo(interp, buf, -1);
    }

    /*
     * Set the interpreter's termOffset member to the offset of the
     * character just after the last one executed. We approximate the offset
     * of the last character executed by using the number of characters
     * compiled.
     */

    iPtr->termOffset = numSrcChars;
    iPtr->flags &= ~ERR_ALREADY_LOGGED;
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_ExprLong, Tcl_ExprDouble, Tcl_ExprBoolean --
 *
 *	Procedures to evaluate an expression and return its value in a
 *	particular form.
 *
 * Results:
 *	Each of the procedures below returns a standard Tcl result. If an
 *	error occurs then an error message is left in interp->result.
 *	Otherwise the value of the expression, in the appropriate form, is
 *	stored at *ptr. If the expression had a result that was
 *	incompatible with the desired form then an error is returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tcl_ExprLong(interp, string, ptr)
    Tcl_Interp *interp;		/* Context in which to evaluate the
				 * expression. */
    char *string;		/* Expression to evaluate. */
    long *ptr;			/* Where to store result. */
{
    register Tcl_Obj *exprPtr;
    Tcl_Obj *resultPtr;
    int length = strlen(string);
    int result = TCL_OK;

    if (length > 0) {
	exprPtr = Tcl_NewStringObj(string, length);
	Tcl_IncrRefCount(exprPtr);
	
	result = Tcl_ExprObj(interp, exprPtr, &resultPtr);
	if (result == TCL_OK) {
	    /*
	     * Store an integer based on the expression result.
	     */
	    
	    if (resultPtr->typePtr == &tclIntType) {
		*ptr = resultPtr->internalRep.longValue;
	    } else if (resultPtr->typePtr == &tclDoubleType) {
		*ptr = (long) resultPtr->internalRep.doubleValue;
	    } else {
		Tcl_SetResult(interp,
		        "expression didn't have numeric value", TCL_STATIC);
		result = TCL_ERROR;
	    }
	    Tcl_DecrRefCount(resultPtr);  /* discard the result object */
	} else {
	    /*
	     * Move the interpreter's object result to the string result, 
	     * then reset the object result.
	     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION HAS NULLS.
	     */

	    Tcl_SetResult(interp,
	            TclGetStringFromObj(Tcl_GetObjResult(interp),
		            (int *) NULL),
	            TCL_VOLATILE);
	}
	Tcl_DecrRefCount(exprPtr);  /* discard the expression object */	
    } else {
	/*
	 * An empty string. Just set the result integer to 0.
	 */
	
	*ptr = 0;
    }
    return result;
}

int
Tcl_ExprDouble(interp, string, ptr)
    Tcl_Interp *interp;		/* Context in which to evaluate the
				 * expression. */
    char *string;		/* Expression to evaluate. */
    double *ptr;		/* Where to store result. */
{
    register Tcl_Obj *exprPtr;
    Tcl_Obj *resultPtr;
    int length = strlen(string);
    int result = TCL_OK;

    if (length > 0) {
	exprPtr = Tcl_NewStringObj(string, length);
	Tcl_IncrRefCount(exprPtr);
	
	result = Tcl_ExprObj(interp, exprPtr, &resultPtr);
	if (result == TCL_OK) {
	    /*
	     * Store a double  based on the expression result.
	     */
	    
	    if (resultPtr->typePtr == &tclIntType) {
		*ptr = (double) resultPtr->internalRep.longValue;
	    } else if (resultPtr->typePtr == &tclDoubleType) {
		*ptr = resultPtr->internalRep.doubleValue;
	    } else {
		Tcl_SetResult(interp,
		        "expression didn't have numeric value", TCL_STATIC);
		result = TCL_ERROR;
	    }
	    Tcl_DecrRefCount(resultPtr);  /* discard the result object */
	} else {
	    /*
	     * Move the interpreter's object result to the string result, 
	     * then reset the object result.
	     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION HAS NULLS.
	     */

	    Tcl_SetResult(interp,
	            TclGetStringFromObj(Tcl_GetObjResult(interp),
		            (int *) NULL),
	            TCL_VOLATILE);
	}
	Tcl_DecrRefCount(exprPtr);  /* discard the expression object */
    } else {
	/*
	 * An empty string. Just set the result double to 0.0.
	 */
	
	*ptr = 0.0;
    }
    return result;
}

int
Tcl_ExprBoolean(interp, string, ptr)
    Tcl_Interp *interp;		/* Context in which to evaluate the
			         * expression. */
    char *string;		/* Expression to evaluate. */
    int *ptr;			/* Where to store 0/1 result. */
{
    register Tcl_Obj *exprPtr;
    Tcl_Obj *resultPtr;
    int length = strlen(string);
    int result = TCL_OK;

    if (length > 0) {
	exprPtr = Tcl_NewStringObj(string, length);
	Tcl_IncrRefCount(exprPtr);
	
	result = Tcl_ExprObj(interp, exprPtr, &resultPtr);
	if (result == TCL_OK) {
	    /*
	     * Store a boolean based on the expression result.
	     */
	    
	    if (resultPtr->typePtr == &tclIntType) {
		*ptr = (resultPtr->internalRep.longValue != 0);
	    } else if (resultPtr->typePtr == &tclDoubleType) {
		*ptr = (resultPtr->internalRep.doubleValue != 0.0);
	    } else {
		result = Tcl_GetBooleanFromObj(interp, resultPtr, ptr);
	    }
	    Tcl_DecrRefCount(resultPtr);  /* discard the result object */
	}
	if (result != TCL_OK) {
	    /*
	     * Move the interpreter's object result to the string result, 
	     * then reset the object result.
	     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION HAS NULLS.
	     */

	    Tcl_SetResult(interp,
	            TclGetStringFromObj(Tcl_GetObjResult(interp),
		            (int *) NULL),
	            TCL_VOLATILE);
	}
	Tcl_DecrRefCount(exprPtr); /* discard the expression object */
    } else {
	/*
	 * An empty string. Just set the result boolean to 0 (false).
	 */
	
	*ptr = 0;
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_ExprLongObj, Tcl_ExprDoubleObj, Tcl_ExprBooleanObj --
 *
 *	Procedures to evaluate an expression in an object and return its
 *	value in a particular form.
 *
 * Results:
 *	Each of the procedures below returns a standard Tcl result
 *	object. If an error occurs then an error message is left in the
 *	interpreter's result. Otherwise the value of the expression, in the
 *	appropriate form, is stored at *ptr. If the expression had a result
 *	that was incompatible with the desired form then an error is
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tcl_ExprLongObj(interp, objPtr, ptr)
    Tcl_Interp *interp;			/* Context in which to evaluate the
					 * expression. */
    register Tcl_Obj *objPtr;		/* Expression to evaluate. */
    long *ptr;				/* Where to store long result. */
{
    Tcl_Obj *resultPtr;
    int result;

    result = Tcl_ExprObj(interp, objPtr, &resultPtr);
    if (result == TCL_OK) {
	if (resultPtr->typePtr == &tclIntType) {
	    *ptr = resultPtr->internalRep.longValue;
	} else if (resultPtr->typePtr == &tclDoubleType) {
	    *ptr = (long) resultPtr->internalRep.doubleValue;
	} else {
	    result = Tcl_GetLongFromObj(interp, resultPtr, ptr);
	    if (result != TCL_OK) {
		return result;
	    }
	}
	Tcl_DecrRefCount(resultPtr);  /* discard the result object */
    }
    return result;
}

int
Tcl_ExprDoubleObj(interp, objPtr, ptr)
    Tcl_Interp *interp;			/* Context in which to evaluate the
					 * expression. */
    register Tcl_Obj *objPtr;		/* Expression to evaluate. */
    double *ptr;			/* Where to store double result. */
{
    Tcl_Obj *resultPtr;
    int result;

    result = Tcl_ExprObj(interp, objPtr, &resultPtr);
    if (result == TCL_OK) {
	if (resultPtr->typePtr == &tclIntType) {
	    *ptr = (double) resultPtr->internalRep.longValue;
	} else if (resultPtr->typePtr == &tclDoubleType) {
	    *ptr = resultPtr->internalRep.doubleValue;
	} else {
	    result = Tcl_GetDoubleFromObj(interp, resultPtr, ptr);
	    if (result != TCL_OK) {
		return result;
	    }
	}
	Tcl_DecrRefCount(resultPtr);  /* discard the result object */
    }
    return result;
}

int
Tcl_ExprBooleanObj(interp, objPtr, ptr)
    Tcl_Interp *interp;			/* Context in which to evaluate the
					 * expression. */
    register Tcl_Obj *objPtr;		/* Expression to evaluate. */
    int *ptr;				/* Where to store 0/1 result. */
{
    Tcl_Obj *resultPtr;
    int result;

    result = Tcl_ExprObj(interp, objPtr, &resultPtr);
    if (result == TCL_OK) {
	if (resultPtr->typePtr == &tclIntType) {
	    *ptr = (resultPtr->internalRep.longValue != 0);
	} else if (resultPtr->typePtr == &tclDoubleType) {
	    *ptr = (resultPtr->internalRep.doubleValue != 0.0);
	} else {
	    result = Tcl_GetBooleanFromObj(interp, resultPtr, ptr);
	    if (result != TCL_OK) {
		return result;
	    }
	}
	Tcl_DecrRefCount(resultPtr);  /* discard the result object */
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInvoke --
 *
 *	Invokes a Tcl command, given an argv/argc, from either the
 *	exposed or the hidden sets of commands in the given interpreter.
 *	NOTE: The command is invoked in the current stack frame of
 *	the interpreter, thus it can modify local variables.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the command does.
 *
 *----------------------------------------------------------------------
 */

int
TclInvoke(interp, argc, argv, flags)
    Tcl_Interp *interp;		/* Where to invoke the command. */
    int argc;			/* Count of args. */
    register char **argv;	/* The arg strings; argv[0] is the name of
                                 * the command to invoke. */
    int flags;			/* Combination of flags controlling the
				 * call: TCL_INVOKE_HIDDEN and
				 * TCL_INVOKE_NO_UNKNOWN. */
{
    register Tcl_Obj *objPtr;
    register int i;
    int length, result;

    /*
     * This procedure generates an objv array for object arguments that hold
     * the argv strings. It starts out with stack-allocated space but uses
     * dynamically-allocated storage if needed.
     */

#define NUM_ARGS 20
    Tcl_Obj *(objStorage[NUM_ARGS]);
    register Tcl_Obj **objv = objStorage;

    /*
     * Create the object argument array "objv". Make sure objv is large
     * enough to hold the objc arguments plus 1 extra for the zero
     * end-of-objv word.
     */

    if ((argc + 1) > NUM_ARGS) {
	objv = (Tcl_Obj **)
	    ckalloc((unsigned)(argc + 1) * sizeof(Tcl_Obj *));
    }

    for (i = 0;  i < argc;  i++) {
	length = strlen(argv[i]);
	objv[i] = Tcl_NewStringObj(argv[i], length);
	Tcl_IncrRefCount(objv[i]);
    }
    objv[argc] = 0;

    /*
     * Use TclObjInterpProc to actually invoke the command.
     */

    result = TclObjInvoke(interp, argc, objv, flags);

    /*
     * Move the interpreter's object result to the string result, 
     * then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
     */
    
    Tcl_SetResult(interp,
	    TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	    TCL_VOLATILE);

    /*
     * Decrement the ref counts on the objv elements since we are done
     * with them.
     */

    for (i = 0;  i < argc;  i++) {
	objPtr = objv[i];
	Tcl_DecrRefCount(objPtr);
    }
    
    /*
     * Free the objv array if malloc'ed storage was used.
     */

    if (objv != objStorage) {
	ckfree((char *) objv);
    }
    return result;
#undef NUM_ARGS
}

/*
 *----------------------------------------------------------------------
 *
 * TclGlobalInvoke --
 *
 *	Invokes a Tcl command, given an argv/argc, from either the
 *	exposed or hidden sets of commands in the given interpreter.
 *	NOTE: The command is invoked in the global stack frame of
 *	the interpreter, thus it cannot see any current state on
 *	the stack for that interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the command does.
 *
 *----------------------------------------------------------------------
 */

int
TclGlobalInvoke(interp, argc, argv, flags)
    Tcl_Interp *interp;		/* Where to invoke the command. */
    int argc;			/* Count of args. */
    register char **argv;	/* The arg strings; argv[0] is the name of
                                 * the command to invoke. */
    int flags;			/* Combination of flags controlling the
				 * call: TCL_INVOKE_HIDDEN and
				 * TCL_INVOKE_NO_UNKNOWN. */
{
    register Interp *iPtr = (Interp *) interp;
    int result;
    CallFrame *savedVarFramePtr;

    savedVarFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = NULL;
    result = TclInvoke(interp, argc, argv, flags);
    iPtr->varFramePtr = savedVarFramePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjInvokeGlobal --
 *
 *	Object version: Invokes a Tcl command, given an objv/objc, from
 *	either the exposed or hidden set of commands in the given
 *	interpreter.
 *	NOTE: The command is invoked in the global stack frame of the
 *	interpreter, thus it cannot see any current state on the
 *	stack of that interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the command does.
 *
 *----------------------------------------------------------------------
 */

int
TclObjInvokeGlobal(interp, objc, objv, flags)
    Tcl_Interp *interp;		/* Interpreter in which command is
				 * to be invoked. */
    int objc;			/* Count of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument value objects; objv[0]
				 * points to the name of the
				 * command to invoke. */
    int flags;			/* Combination of flags controlling
                                 * the call: TCL_INVOKE_HIDDEN and
                                 * TCL_INVOKE_NO_UNKNOWN. */
{
    register Interp *iPtr = (Interp *) interp;
    int result;
    CallFrame *savedVarFramePtr;

    savedVarFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = NULL;
    result = TclObjInvoke(interp, objc, objv, flags);
    iPtr->varFramePtr = savedVarFramePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjInvoke --
 *
 *	Invokes a Tcl command, given an objv/objc, from either the
 *	exposed or the hidden sets of commands in the given interpreter.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Whatever the command does.
 *
 *----------------------------------------------------------------------
 */

int
TclObjInvoke(interp, objc, objv, flags)
    Tcl_Interp *interp;		/* Interpreter in which command is
				 * to be invoked. */
    int objc;			/* Count of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument value objects; objv[0]
				 * points to the name of the
				 * command to invoke. */
    int flags;			/* Combination of flags controlling
                                 * the call: TCL_INVOKE_HIDDEN and
                                 * TCL_INVOKE_NO_UNKNOWN. */
{
    register Interp *iPtr = (Interp *) interp;
    Tcl_HashTable *hTblPtr;	/* Table of hidden commands. */
    char *cmdName;		/* Name of the command from objv[0]. */
    register Tcl_HashEntry *hPtr;
    Tcl_Command cmd;
    Command *cmdPtr;
    int localObjc;		/* Used to invoke "unknown" if the */
    Tcl_Obj **localObjv = NULL;	/* command is not found. */
    register int i;
    int length, result;
    char *bytes;

    if (interp == (Tcl_Interp *) NULL) {
        return TCL_ERROR;
    }

    if ((objc < 1) || (objv == (Tcl_Obj **) NULL)) {
        Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "illegal argument vector", -1);
        return TCL_ERROR;
    }

    /*
     * THE FOLLOWING CODE FAILS IF THE STRING REP CONTAINS NULLS.
     */
    
    cmdName = Tcl_GetStringFromObj(objv[0], (int *) NULL);
    if (flags & TCL_INVOKE_HIDDEN) {
        /*
         * Find the table of hidden commands; error out if none.
         */

        hTblPtr = (Tcl_HashTable *)
	        Tcl_GetAssocData(interp, "tclHiddenCmds", NULL);
        if (hTblPtr == (Tcl_HashTable *) NULL) {
            badHiddenCmdName:
	    Tcl_ResetResult(interp);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		     "invalid hidden command name \"", cmdName, "\"",
		     (char *) NULL);
            return TCL_ERROR;
        }
        hPtr = Tcl_FindHashEntry(hTblPtr, cmdName);

        /*
         * We never invoke "unknown" for hidden commands.
         */
        
        if (hPtr == NULL) {
            goto badHiddenCmdName;
        }
	cmdPtr = (Command *) Tcl_GetHashValue(hPtr);
    } else {
	cmdPtr = NULL;
	cmd = Tcl_FindCommand(interp, cmdName,
	        (Tcl_Namespace *) NULL, /*flags*/ TCL_GLOBAL_ONLY);
        if (cmd != (Tcl_Command) NULL) {
	    cmdPtr = (Command *) cmd;
        }
	if (cmdPtr == NULL) {
            if (!(flags & TCL_INVOKE_NO_UNKNOWN)) {
		cmd = Tcl_FindCommand(interp, "unknown",
                        (Tcl_Namespace *) NULL, /*flags*/ TCL_GLOBAL_ONLY);
		if (cmd != (Tcl_Command) NULL) {
	            cmdPtr = (Command *) cmd;
                }
                if (cmdPtr != NULL) {
                    localObjc = (objc + 1);
                    localObjv = (Tcl_Obj **)
			ckalloc((unsigned) (sizeof(Tcl_Obj *) * localObjc));
		    localObjv[0] = Tcl_NewStringObj("unknown", -1);
		    Tcl_IncrRefCount(localObjv[0]);
                    for (i = 0;  i < objc;  i++) {
                        localObjv[i+1] = objv[i];
                    }
                    objc = localObjc;
                    objv = localObjv;
                }
            }

            /*
             * Check again if we found the command. If not, "unknown" is
             * not present and we cannot help, or the caller said not to
             * call "unknown" (they specified TCL_INVOKE_NO_UNKNOWN).
             */

            if (cmdPtr == NULL) {
		Tcl_ResetResult(interp);
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"invalid command name \"",  cmdName, "\"", 
			 (char *) NULL);
                return TCL_ERROR;
            }
        }
    }

    /*
     * Invoke the command procedure. First reset the interpreter's string
     * and object results to their default empty values since they could
     * have gotten changed by earlier invocations.
     */

    Tcl_ResetResult(interp);
    iPtr->cmdCount++;
    result = (*cmdPtr->objProc)(cmdPtr->objClientData, interp, objc, objv);

    /*
     * If an error occurred, record information about what was being
     * executed when the error occurred.
     */

    if ((result == TCL_ERROR) && !(iPtr->flags & ERR_ALREADY_LOGGED)) {
        Tcl_DString ds;
        
        Tcl_DStringInit(&ds);
        if (!(iPtr->flags & ERR_IN_PROGRESS)) {
            Tcl_DStringAppend(&ds, "\n    while invoking\n\"", -1);
        } else {
            Tcl_DStringAppend(&ds, "\n    invoked from within\n\"", -1);
        }
        for (i = 0;  i < objc;  i++) {
	    bytes = Tcl_GetStringFromObj(objv[i], &length);
            Tcl_DStringAppend(&ds, bytes, length);
            if (i < (objc - 1)) {
                Tcl_DStringAppend(&ds, " ", -1);
            } else if (Tcl_DStringLength(&ds) > 100) {
                Tcl_DStringSetLength(&ds, 100);
                Tcl_DStringAppend(&ds, "...", -1);
                break;
            }
        }
        
        Tcl_DStringAppend(&ds, "\"", -1);
        Tcl_AddObjErrorInfo(interp, Tcl_DStringValue(&ds), -1);
        Tcl_DStringFree(&ds);
	iPtr->flags &= ~ERR_ALREADY_LOGGED;
    }

    /*
     * Free any locally allocated storage used to call "unknown".
     */

    if (localObjv != (Tcl_Obj **) NULL) {
        ckfree((char *) localObjv);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_ExprString --
 *
 *	Evaluate an expression in a string and return its value in string
 *	form.
 *
 * Results:
 *	A standard Tcl result. If the result is TCL_OK, then the
 *	interpreter's result is set to the string value of the
 *	expression. If the result is TCL_OK, then interp->result
 *	contains an error message.
 *
 * Side effects:
 *	A Tcl object is allocated to hold a copy of the expression string.
 *	This expression object is passed to Tcl_ExprObj and then
 *	deallocated.
 *
 *--------------------------------------------------------------
 */

int
Tcl_ExprString(interp, string)
    Tcl_Interp *interp;		/* Context in which to evaluate the
				 * expression. */
    char *string;		/* Expression to evaluate. */
{
    register Tcl_Obj *exprPtr;
    Tcl_Obj *resultPtr;
    int length = strlen(string);
    char buf[100];
    int result = TCL_OK;

    if (length > 0) {
	TclNewObj(exprPtr);
	TclInitStringRep(exprPtr, string, length);
	Tcl_DecrRefCount(exprPtr);

	result = Tcl_ExprObj(interp, exprPtr, &resultPtr);
	if (result == TCL_OK) {
	    /*
	     * Set the interpreter's string result from the result object.
	     */
	    
	    if (resultPtr->typePtr == &tclIntType) {
		sprintf(buf, "%ld", resultPtr->internalRep.longValue);
		Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    } else if (resultPtr->typePtr == &tclDoubleType) {
		Tcl_PrintDouble((Tcl_Interp *) NULL,
		        resultPtr->internalRep.doubleValue, buf);
		Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    } else {
		/*
		 * Set interpreter's string result from the result object.
		 * FAILS IF OBJECT RESULT'S STRING REPRESENTATION HAS NULLS.
		 */
	    
		Tcl_SetResult(interp,
	                TclGetStringFromObj(resultPtr, (int *) NULL),
	                TCL_VOLATILE);
	    }
	    Tcl_DecrRefCount(resultPtr);  /* discard the result object */
	} else {
	    /*
	     * Move the interpreter's object result to the string result, 
	     * then reset the object result.
	     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION HAS NULLS.
	     */
	    
	    Tcl_SetResult(interp,
	            TclGetStringFromObj(Tcl_GetObjResult(interp),
			    (int *) NULL),
	            TCL_VOLATILE);
	}
	Tcl_DecrRefCount(exprPtr); /* discard the expression object */
    } else {
	/*
	 * An empty string. Just set the interpreter's result to 0.
	 */
	
	Tcl_SetResult(interp, "0", TCL_VOLATILE);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_ExprObj --
 *
 *	Evaluate an expression in a Tcl_Obj.
 *
 * Results:
 *	A standard Tcl object result. If the result is other than TCL_OK,
 *	then the interpreter's result contains an error message. If the
 *	result is TCL_OK, then a pointer to the expression's result value
 *	object is stored in resultPtrPtr. In that case, the object's ref
 *	count is incremented to reflect the reference returned to the
 *	caller; the caller is then responsible for the resulting object
 *	and must, for example, decrement the ref count when it is finished
 *	with the object.
 *
 * Side effects:
 *	Any side effects caused by subcommands in the expression, if any.
 *	The interpreter result is not modified unless there is an error.
 *
 *--------------------------------------------------------------
 */

int
Tcl_ExprObj(interp, objPtr, resultPtrPtr)
    Tcl_Interp *interp;		/* Context in which to evaluate the
				 * expression. */
    register Tcl_Obj *objPtr;	/* Points to Tcl object containing
				 * expression to evaluate. */
    Tcl_Obj **resultPtrPtr;	/* Where the Tcl_Obj* that is the expression
				 * result is stored if no errors occur. */
{
    Interp *iPtr = (Interp *) interp;
    CompileEnv compEnv;		/* Compilation environment structure
				 * allocated in frame. */
    register ByteCode *codePtr = NULL;
    				/* Tcl Internal type of bytecode.
				 * Initialized to avoid compiler warning. */
    AuxData *auxDataPtr;
    Interp dummy;
    Tcl_Obj *saveObjPtr;
    char *string;
    int result = TCL_OK;
    int i;

    /*
     * Get the ByteCode from the object. If it exists, make sure it hasn't
     * been invalidated by, e.g., someone redefining a command with a
     * compile procedure (this might make the compiled code wrong). If
     * necessary, convert the object to be a ByteCode object and compile it.
     * Also, if the code was compiled in/for a different interpreter, we
     * recompile it.
     * THIS FAILS IF THE OBJECT'S STRING REP HAS A NULL BYTE.
     */

    if (objPtr->typePtr == &tclByteCodeType) {
	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	if ((codePtr->iPtr != iPtr)
	        || (codePtr->compileEpoch != iPtr->compileEpoch)) {
	    tclByteCodeType.freeIntRepProc(objPtr);
	    objPtr->typePtr = (Tcl_ObjType *) NULL;
	}
    }
    if (objPtr->typePtr != &tclByteCodeType) {
	int length;
	string = Tcl_GetStringFromObj(objPtr, &length);
	TclInitCompileEnv(interp, &compEnv, string);
	result = TclCompileExpr(interp, string, string + length,
		/*flags*/ 0, &compEnv);
	if (result == TCL_OK) {
	    /*
	     * If the expression yielded no instructions (e.g., was empty),
	     * push an integer zero object as the expressions's result.
	     */
	    
	    if (compEnv.codeNext == NULL) {
		int objIndex = TclObjIndexForString("0", 0,
			/*allocStrRep*/ 0, /*inHeap*/ 0, &compEnv);
		Tcl_Obj *objPtr = compEnv.objArrayPtr[objIndex];

		Tcl_InvalidateStringRep(objPtr);
		objPtr->internalRep.longValue = 0;
		objPtr->typePtr = &tclIntType;
		
		TclEmitPush(objIndex, &compEnv);
	    }
	    
	    /*
	     * Add done instruction at the end of the instruction sequence.
	     */
	    
	    TclEmitOpcode(INST_DONE, &compEnv);
	    
	    TclInitByteCodeObj(objPtr, &compEnv);
	    codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	    if (tclTraceCompile == 2) {
		TclPrintByteCodeObj(interp, objPtr);
	    }
	    TclFreeCompileEnv(&compEnv);
	} else {
	    /*
	     * Compilation errors. Decrement the ref counts on any objects
	     * in the object array before freeing the compilation
	     * environment.
	     */
	    
	    for (i = 0;  i < compEnv.objArrayNext;  i++) {
		Tcl_Obj *elemPtr = compEnv.objArrayPtr[i];
		Tcl_DecrRefCount(elemPtr);
	    }

	    auxDataPtr = compEnv.auxDataArrayPtr;
	    for (i = 0;  i < compEnv.auxDataArrayNext;  i++) {
		if (auxDataPtr->freeProc != NULL) {
		    auxDataPtr->freeProc(auxDataPtr->clientData);
		}
		auxDataPtr++;
	    }
	    TclFreeCompileEnv(&compEnv);
	    return result;
	}
    }

    /*
     * Execute the expression after first saving the interpreter's result.
     */
    
    dummy.objResultPtr = Tcl_NewObj();
    Tcl_IncrRefCount(dummy.objResultPtr);
    if (interp->freeProc == 0) {
	dummy.freeProc = (Tcl_FreeProc *) 0;
	dummy.result = "";
	Tcl_SetResult((Tcl_Interp *) &dummy, interp->result,
	        TCL_VOLATILE);
    } else {
	dummy.freeProc = interp->freeProc;
	dummy.result = interp->result;
	interp->freeProc = (Tcl_FreeProc *) 0;
    }
    
    saveObjPtr = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(saveObjPtr);
    
    /*
     * Increment the code's ref count while it is being executed. If
     * afterwards no references to it remain, free the code.
     */
    
    codePtr->refCount++;
    result = TclExecuteByteCode(interp, codePtr);
    codePtr->refCount--;
    if (codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
    }
    
    /*
     * If the expression evaluated successfully, store a pointer to its
     * value object in resultPtrPtr then restore the old interpreter result.
     * We increment the object's ref count to reflect the reference that we
     * are returning to the caller. We also decrement the ref count of the
     * interpreter's result object after calling Tcl_SetResult since we
     * next store into that field directly.
     */
    
    if (result == TCL_OK) {
	*resultPtrPtr = iPtr->objResultPtr;
	Tcl_IncrRefCount(iPtr->objResultPtr);
	
	Tcl_SetResult(interp, dummy.result,
	        ((dummy.freeProc == 0) ? TCL_VOLATILE : dummy.freeProc));
	Tcl_DecrRefCount(iPtr->objResultPtr);
	iPtr->objResultPtr = saveObjPtr;
    } else {
	Tcl_DecrRefCount(saveObjPtr);
	Tcl_FreeResult((Tcl_Interp *) &dummy);
    }

    Tcl_DecrRefCount(dummy.objResultPtr);
    dummy.objResultPtr = NULL;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateTrace --
 *
 *	Arrange for a procedure to be called to trace command execution.
 *
 * Results:
 *	The return value is a token for the trace, which may be passed
 *	to Tcl_DeleteTrace to eliminate the trace.
 *
 * Side effects:
 *	From now on, proc will be called just before a command procedure
 *	is called to execute a Tcl command.  Calls to proc will have the
 *	following form:
 *
 *	void
 *	proc(clientData, interp, level, command, cmdProc, cmdClientData,
 *		argc, argv)
 *	    ClientData clientData;
 *	    Tcl_Interp *interp;
 *	    int level;
 *	    char *command;
 *	    int (*cmdProc)();
 *	    ClientData cmdClientData;
 *	    int argc;
 *	    char **argv;
 *	{
 *	}
 *
 *	The clientData and interp arguments to proc will be the same
 *	as the corresponding arguments to this procedure.  Level gives
 *	the nesting level of command interpretation for this interpreter
 *	(0 corresponds to top level).  Command gives the ASCII text of
 *	the raw command, cmdProc and cmdClientData give the procedure that
 *	will be called to process the command and the ClientData value it
 *	will receive, and argc and argv give the arguments to the
 *	command, after any argument parsing and substitution.  Proc
 *	does not return a value.
 *
 *----------------------------------------------------------------------
 */

Tcl_Trace
Tcl_CreateTrace(interp, level, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter in which to create trace. */
    int level;			/* Only call proc for commands at nesting
				 * level<=argument level (1=>top level). */
    Tcl_CmdTraceProc *proc;	/* Procedure to call before executing each
				 * command. */
    ClientData clientData;	/* Arbitrary value word to pass to proc. */
{
    register Trace *tracePtr;
    register Interp *iPtr = (Interp *) interp;

    /*
     * Invalidate existing compiled code for this interpreter and arrange
     * (by setting the DONT_COMPILE_CMDS_INLINE flag) that when compiling
     * new code, no commands will be compiled inline (i.e., into an inline
     * sequence of instructions). We do this because commands that were
     * compiled inline will never result in a command trace being called.
     */

    iPtr->compileEpoch++;
    iPtr->flags |= DONT_COMPILE_CMDS_INLINE;

    tracePtr = (Trace *) ckalloc(sizeof(Trace));
    tracePtr->level = level;
    tracePtr->proc = proc;
    tracePtr->clientData = clientData;
    tracePtr->nextPtr = iPtr->tracePtr;
    iPtr->tracePtr = tracePtr;

    return (Tcl_Trace) tracePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteTrace --
 *
 *	Remove a trace.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on there will be no more calls to the procedure given
 *	in trace.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteTrace(interp, trace)
    Tcl_Interp *interp;		/* Interpreter that contains trace. */
    Tcl_Trace trace;		/* Token for trace (returned previously by
				 * Tcl_CreateTrace). */
{
    register Interp *iPtr = (Interp *) interp;
    register Trace *tracePtr = (Trace *) trace;
    register Trace *tracePtr2;

    if (iPtr->tracePtr == tracePtr) {
	iPtr->tracePtr = tracePtr->nextPtr;
	ckfree((char *) tracePtr);
    } else {
	for (tracePtr2 = iPtr->tracePtr; tracePtr2 != NULL;
		tracePtr2 = tracePtr2->nextPtr) {
	    if (tracePtr2->nextPtr == tracePtr) {
		tracePtr2->nextPtr = tracePtr->nextPtr;
		ckfree((char *) tracePtr);
		break;
	    }
	}
    }

    if (iPtr->tracePtr == NULL) {
	/*
	 * When compiling new code, allow commands to be compiled inline.
	 */

	iPtr->flags &= ~DONT_COMPILE_CMDS_INLINE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AddErrorInfo --
 *
 *	Add information to the "errorInfo" variable that describes the
 *	current error.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of message are added to the "errorInfo" variable.
 *	If Tcl_Eval has been called since the current value of errorInfo
 *	was set, errorInfo is cleared before adding the new message.
 *	If we are just starting to log an error, errorInfo is initialized
 *	from the error message in the interpreter's result.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AddErrorInfo(interp, message)
    Tcl_Interp *interp;		/* Interpreter to which error information
				 * pertains. */
    char *message;		/* Message to record. */
{
    Tcl_AddObjErrorInfo(interp, message, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AddObjErrorInfo --
 *
 *	Add information to the "errorInfo" variable that describes the
 *	current error. This routine differs from Tcl_AddErrorInfo by
 *	taking a byte pointer and length.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"length" bytes from "message" are added to the "errorInfo" variable.
 *	If "length" is negative, use bytes up to the first NULL byte.
 *	If Tcl_EvalObj has been called since the current value of errorInfo
 *	was set, errorInfo is cleared before adding the new message.
 *	If we are just starting to log an error, errorInfo is initialized
 *	from the error message in the interpreter's result.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AddObjErrorInfo(interp, message, length)
    Tcl_Interp *interp;		/* Interpreter to which error information
				 * pertains. */
    char *message;		/* Points to the first byte of an array of
				 * bytes of the message. */
    register int length;	/* The number of bytes in the message.
				 * If < 0, then append all bytes up to a
				 * NULL byte. */
{
    register Interp *iPtr = (Interp *) interp;
    Tcl_Obj *namePtr, *messagePtr;
    
    /*
     * If we are just starting to log an error, errorInfo is initialized
     * from the error message in the interpreter's result.
     */

    namePtr = Tcl_NewStringObj("errorInfo", -1);
    Tcl_IncrRefCount(namePtr);
    
    if (!(iPtr->flags & ERR_IN_PROGRESS)) { /* just starting to log error */
	iPtr->flags |= ERR_IN_PROGRESS;

	if (iPtr->result[0] == 0) {
	    (void) Tcl_ObjSetVar2(interp, namePtr, (Tcl_Obj *) NULL,
	            iPtr->objResultPtr, TCL_GLOBAL_ONLY);
	} else {		/* use the string result */
	    Tcl_SetVar2(interp, "errorInfo", (char *) NULL, interp->result,
		    TCL_GLOBAL_ONLY);
	}

	/*
	 * If the errorCode variable wasn't set by the code that generated
	 * the error, set it to "NONE".
	 */

	if (!(iPtr->flags & ERROR_CODE_SET)) {
	    (void) Tcl_SetVar2(interp, "errorCode", (char *) NULL, "NONE",
		    TCL_GLOBAL_ONLY);
	}
    }

    /*
     * Now append "message" to the end of errorInfo.
     */

    messagePtr = Tcl_NewStringObj(message, length);
    Tcl_IncrRefCount(messagePtr);
    Tcl_ObjSetVar2(interp, namePtr, (Tcl_Obj *) NULL, messagePtr,
	    (TCL_GLOBAL_ONLY | TCL_APPEND_VALUE));
    Tcl_DecrRefCount(messagePtr); /* free msg object appended above */
    
    Tcl_DecrRefCount(namePtr);    /* free the name object */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VarEval --
 *
 *	Given a variable number of string arguments, concatenate them
 *	all together and execute the result as a Tcl command.
 *
 * Results:
 *	A standard Tcl return result.  An error message or other
 *	result may be left in interp->result.
 *
 * Side effects:
 *	Depends on what was done by the command.
 *
 *----------------------------------------------------------------------
 */
	/* VARARGS2 */ /* ARGSUSED */
int
Tcl_VarEval TCL_VARARGS_DEF(Tcl_Interp *,arg1)
{
    va_list argList;
    Tcl_DString buf;
    char *string;
    Tcl_Interp *interp;
    int result;

    /*
     * Copy the strings one after the other into a single larger
     * string.  Use stack-allocated space for small commands, but if
     * the command gets too large than call ckalloc to create the
     * space.
     */

    interp = TCL_VARARGS_START(Tcl_Interp *,arg1,argList);
    Tcl_DStringInit(&buf);
    while (1) {
	string = va_arg(argList, char *);
	if (string == NULL) {
	    break;
	}
	Tcl_DStringAppend(&buf, string, -1);
    }
    va_end(argList);

    result = Tcl_Eval(interp, Tcl_DStringValue(&buf));
    Tcl_DStringFree(&buf);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GlobalEval --
 *
 *	Evaluate a command at global level in an interpreter.
 *
 * Results:
 *	A standard Tcl result is returned, and interp->result is
 *	modified accordingly.
 *
 * Side effects:
 *	The command string is executed in interp, and the execution
 *	is carried out in the variable context of global level (no
 *	procedures active), just as if an "uplevel #0" command were
 *	being executed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GlobalEval(interp, command)
    Tcl_Interp *interp;		/* Interpreter in which to evaluate command. */
    char *command;		/* Command to evaluate. */
{
    register Interp *iPtr = (Interp *) interp;
    int result;
    CallFrame *savedVarFramePtr;

    savedVarFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = NULL;
    result = Tcl_Eval(interp, command);
    iPtr->varFramePtr = savedVarFramePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GlobalEvalObj --
 *
 *	Execute Tcl commands stored in a Tcl object at global level in
 *	an interpreter. These commands are compiled into bytecodes if
 *	necessary.
 *
 * Results:
 *	A standard Tcl result is returned, and the interpreter's result
 *	contains a Tcl object value to supplement the return code.
 *
 * Side effects:
 *	The object is converted, if necessary, to a ByteCode object that
 *	holds the bytecode instructions for the commands. Executing the
 *	commands will almost certainly have side effects that depend on
 *	those commands.
 *
 *	The commands are executed in interp, and the execution
 *	is carried out in the variable context of global level (no
 *	procedures active), just as if an "uplevel #0" command were
 *	being executed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GlobalEvalObj(interp, objPtr)
    Tcl_Interp *interp;		/* Interpreter in which to evaluate
				 * commands. */
    Tcl_Obj *objPtr;		/* Pointer to object containing commands
				 * to execute. */
{
    register Interp *iPtr = (Interp *) interp;
    int result;
    CallFrame *savedVarFramePtr;

    savedVarFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = NULL;
    result = Tcl_EvalObj(interp, objPtr);
    iPtr->varFramePtr = savedVarFramePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetRecursionLimit --
 *
 *	Set the maximum number of recursive calls that may be active
 *	for an interpreter at once.
 *
 * Results:
 *	The return value is the old limit on nesting for interp.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetRecursionLimit(interp, depth)
    Tcl_Interp *interp;			/* Interpreter whose nesting limit
					 * is to be set. */
    int depth;				/* New value for maximimum depth. */
{
    Interp *iPtr = (Interp *) interp;
    int old;

    old = iPtr->maxNestingDepth;
    if (depth > 0) {
	iPtr->maxNestingDepth = depth;
    }
    return old;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AllowExceptions --
 *
 *	Sets a flag in an interpreter so that exceptions can occur
 *	in the next call to Tcl_Eval without them being turned into
 *	errors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TCL_ALLOW_EXCEPTIONS flag gets set in the interpreter's
 *	evalFlags structure.  See the reference documentation for
 *	more details.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AllowExceptions(interp)
    Tcl_Interp *interp;		/* Interpreter in which to set flag. */
{
    Interp *iPtr = (Interp *) interp;

    iPtr->evalFlags |= TCL_ALLOW_EXCEPTIONS;
}

