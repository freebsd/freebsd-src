/* 
 * tclInterp.c --
 *
 *	This file implements the "interp" command which allows creation
 *	and manipulation of Tcl interpreters from within Tcl scripts.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclInterp.c 1.73 96/06/11 18:14:22
 */

#include <stdio.h>
#include "tclInt.h"
#include "tclPort.h"

/*
 * Counter for how many aliases were created (global)
 */

static int aliasCounter = 0;

/*
 *
 * struct Slave:
 *
 * Used by the "interp" command to record and find information about slave
 * interpreters. Maps from a command name in the master to information about
 * a slave interpreter, e.g. what aliases are defined in it.
 */

typedef struct {
    Tcl_Interp *masterInterp;	/* Master interpreter for this slave. */
    Tcl_HashEntry *slaveEntry;	/* Hash entry in masters slave table for
                                 * this slave interpreter. Used to find
                                 * this record, and used when deleting the
                                 * slave interpreter to delete it from the
                                 * masters table. */
    Tcl_Interp	*slaveInterp;	/* The slave interpreter. */
    Tcl_Command interpCmd;	/* Interpreter object command. */
    Tcl_HashTable aliasTable;	/* Table which maps from names of commands
                                 * in slave interpreter to struct Alias
                                 * defined below. */
} Slave;

/*
 * struct Alias:
 *
 * Stores information about an alias. Is stored in the slave interpreter
 * and used by the source command to find the target command in the master
 * when the source command is invoked.
 */

typedef struct {
    char	*aliasName;	/* Name of alias command. */
    char	*targetName;	/* Name of target command in master interp. */
    Tcl_Interp	*targetInterp;	/* Master interpreter. */
    int		argc;		/* Count of additional args to pass. */
    char	**argv;		/* Actual additional args to pass. */    
    Tcl_HashEntry *aliasEntry;	/* Entry for the alias hash table in slave.
                                 * This is used by alias deletion to remove
                                 * the alias from the slave interpreter
                                 * alias table. */
    Tcl_HashEntry *targetEntry;	/* Entry for target command in master.
                                 * This is used in the master interpreter to
                                 * map back from the target command to aliases
                                 * redirecting to it. Random access to this
                                 * hash table is never required - we are using
                                 * a hash table only for convenience. */
    Tcl_Command slaveCmd;	/* Source command in slave interpreter. */
} Alias;

/*
 * struct Target:
 *
 * Maps from master interpreter commands back to the source commands in slave
 * interpreters. This is needed because aliases can be created between sibling
 * interpreters and must be deleted when the target interpreter is deleted. In
 * case they would not be deleted the source interpreter would be left with a
 * "dangling pointer". One such record is stored in the Master record of the
 * master interpreter (in the targetTable hashtable, see below) with the
 * master for each alias which directs to a command in the master. These
 * records are used to remove the source command for an from a slave if/when
 * the master is deleted.
 */

typedef struct {
    Tcl_Command	slaveCmd;	/* Command for alias in slave interp. */
    Tcl_Interp *slaveInterp;	/* Slave Interpreter. */
} Target;

/*
 * struct Master:
 *
 * This record is used for three purposes: First, slaveTable (a hashtable)
 * maps from names of commands to slave interpreters. This hashtable is
 * used to store information about slave interpreters of this interpreter,
 * to map over all slaves, etc. The second purpose is to store information
 * about all aliases in slaves (or siblings) which direct to target commands
 * in this interpreter (using the targetTable hashtable). The third field in
 * the record, isSafe, denotes whether the interpreter is safe or not. Safe
 * interpreters have restricted functionality, can only create safe slave
 * interpreters and can only load safe extensions.
 */

typedef struct {
    Tcl_HashTable slaveTable;	/* Hash table for slave interpreters.
                                 * Maps from command names to Slave records. */
    int isSafe;			/* Am I a "safe" interpreter? */
    Tcl_HashTable targetTable;	/* Hash table for Target Records. Contains
                                 * all Target records which denote aliases
                                 * from slaves or sibling interpreters that
                                 * direct to commands in this interpreter. This
                                 * table is used to remove dangling pointers
                                 * from the slave (or sibling) interpreters
                                 * when this interpreter is deleted. */
} Master;

/*
 * Prototypes for local static procedures:
 */

static int		AliasCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *currentInterp, int argc, char **argv));
static void		AliasCmdDeleteProc _ANSI_ARGS_((
			    ClientData clientData));
static int		AliasHelper _ANSI_ARGS_((Tcl_Interp *curInterp,
			    Tcl_Interp *slaveInterp, Tcl_Interp *masterInterp,
			    Master *masterPtr, char *aliasName,
			    char *targetName, int argc, char **argv));
static int		CreateInterpObject _ANSI_ARGS_((Tcl_Interp *interp,
			   int argc, char **argv));
static Tcl_Interp	*CreateSlave _ANSI_ARGS_((Tcl_Interp *interp,
			    char *slavePath, int safe));
static int		DeleteAlias _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Interp *slaveInterp, char *aliasName));
static int		DescribeAlias _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Interp *slaveInterp, char *aliasName));
static int		DeleteInterpObject _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv));
static int		DeleteOneInterpObject _ANSI_ARGS_((Tcl_Interp *interp,
		            char *path));
static Tcl_Interp	*GetInterp _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, char *path,
			    Master **masterPtrPtr));
static int		GetTarget _ANSI_ARGS_((Tcl_Interp *interp, char *path,
			    char *aliasName));
static void		MasterRecordDeleteProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));
static int		MakeSafe _ANSI_ARGS_((Tcl_Interp *interp));
static int		SlaveAliasHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv));
static int		SlaveObjectCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static void		SlaveObjectDeleteProc _ANSI_ARGS_((
			    ClientData clientData));
static void		SlaveRecordDeleteProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));

/*
 * These are all the Tcl core commands which are available in a safe
 * interpeter:
 */

static char *TclCommandsToKeep[] = {
    "after", "append", "array",
    "break",
    "case", "catch", "clock", "close", "concat", "continue",
    "eof", "error", "eval", "expr",
    "fblocked", "fileevent", "flush", "for", "foreach", "format",
    "gets", "global",
    "history",
    "if", "incr", "info", "interp",
    "join",
    "lappend", "lindex", "linsert", "list", "llength",
    "lower", "lrange", "lreplace", "lsearch", "lsort",
    "package", "pid", "proc", "puts",
    "read", "regexp", "regsub", "rename", "return",
    "scan", "seek", "set", "split", "string", "subst", "switch",
    "tell", "time", "trace",
    "unset", "unsupported0", "update", "uplevel", "upvar",
    "vwait",
    "while",
    NULL};
static int TclCommandsToKeepCt =
	(sizeof (TclCommandsToKeep) / sizeof (char *)) -1 ;

/*
 *----------------------------------------------------------------------
 *
 * TclPreventAliasLoop --
 *
 *	When defining an alias or renaming a command, prevent an alias
 *	loop from being formed.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	If TCL_ERROR is returned, the function also sets interp->result
 *	to an error message.
 *
 * NOTE:
 *	This function is public internal (instead of being static to
 *	this file) because it is also used from Tcl_RenameCmd.
 *
 *----------------------------------------------------------------------
 */

int
TclPreventAliasLoop(interp, cmdInterp, cmdName, proc, clientData)
    Tcl_Interp *interp;			/* Interp in which to report errors. */
    Tcl_Interp *cmdInterp;		/* Interp in which the command is
                                         * being defined. */
    char *cmdName;			/* Name of Tcl command we are
                                         * attempting to define. */
    Tcl_CmdProc *proc;			/* The command procedure for the
                                         * command being created. */
    ClientData clientData;		/* The client data associated with the
                                         * command to be created. */
{
    Alias *aliasPtr, *nextAliasPtr;
    Tcl_CmdInfo cmdInfo;
    
    /*
     * If we are not creating or renaming an alias, then it is
     * always OK to create or rename the command.
     */
    
    if (proc != AliasCmd) {
        return TCL_OK;
    }

    /*
     * OK, we are dealing with an alias, so traverse the chain of aliases.
     * If we encounter the alias we are defining (or renaming to) any in
     * the chain then we have a loop.
     */

    aliasPtr = (Alias *) clientData;
    nextAliasPtr = aliasPtr;
    while (1) {

        /*
         * If the target of the next alias in the chain is the same as the
         * source alias, we have a loop.
         */
        
        if ((strcmp(nextAliasPtr->targetName, cmdName) == 0) &&
                (nextAliasPtr->targetInterp == cmdInterp)) {
            Tcl_AppendResult(interp, "cannot define or rename alias \"",
                    aliasPtr->aliasName, "\": would create a loop",
                    (char *) NULL);
            return TCL_ERROR;
        }

        /*
         * Otherwise, follow the chain one step further. If the target
         * command is undefined then there is no loop.
         */
        
        if (Tcl_GetCommandInfo(nextAliasPtr->targetInterp,
                nextAliasPtr->targetName, &cmdInfo) == 0) {
            return TCL_OK;
        }

        /*
         * See if the target command is an alias - if so, follow the
         * loop to its target command. Otherwise we do not have a loop.
         */

        if (cmdInfo.proc != AliasCmd) {
            return TCL_OK;
        }
        nextAliasPtr = (Alias *) cmdInfo.clientData;
    }

    /* NOTREACHED */
}

/*
 *----------------------------------------------------------------------
 *
 * MakeSafe --
 *
 *	Makes its argument interpreter contain only functionality that is
 *	defined to be part of Safe Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes commands from its argument interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
MakeSafe(interp)
    Tcl_Interp *interp;		/* Interpreter to be made safe. */
{
    char **argv;				/* Args for Tcl_Eval. */
    int argc, keep, i, j;			/* Loop indices. */
    char *cmdGetGlobalCmds = "info commands";	/* What command to run. */
    char *cmdNoEnv = "unset env";		/* How to get rid of env. */
    Master *masterPtr;				/* Master record of interp
                                                 * to be made safe. */
    Tcl_Channel chan;				/* Channel to remove from
                                                 * safe interpreter. */

    /*
     * Below, Tcl_Eval sets interp->result, so we do not.
     */

    Tcl_ResetResult(interp);
    if ((Tcl_Eval(interp, cmdGetGlobalCmds) == TCL_ERROR) ||
            (Tcl_SplitList(interp, interp->result, &argc, &argv) != TCL_OK)) {
        return TCL_ERROR;
    }
    for (i = 0; i < argc; i++) {
        for (keep = 0, j = 0; j < TclCommandsToKeepCt; j++) {
            if (strcmp(TclCommandsToKeep[j], argv[i]) == 0) {
                keep = 1;
                break;
            }
        }
        if (keep == 0) {
            (void) Tcl_DeleteCommand(interp, argv[i]);
        }
    }
    ckfree((char *) argv);
    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord",
            NULL);
    if (masterPtr == (Master *) NULL) {
        panic("MakeSafe: could not find master record");
    }
    masterPtr->isSafe = 1;
    if (Tcl_Eval(interp, cmdNoEnv) == TCL_ERROR) {
        return TCL_ERROR;
    }

    /*
     * Remove the standard channels from the interpreter; safe interpreters
     * do not ordinarily have access to stdin, stdout and stderr.
     */

    chan = Tcl_GetStdChannel(TCL_STDIN);
    if (chan != (Tcl_Channel) NULL) {
        Tcl_UnregisterChannel(interp, chan);
    }
    chan = Tcl_GetStdChannel(TCL_STDOUT);
    if (chan != (Tcl_Channel) NULL) {
        Tcl_UnregisterChannel(interp, chan);
    }
    chan = Tcl_GetStdChannel(TCL_STDERR);
    if (chan != (Tcl_Channel) NULL) {
        Tcl_UnregisterChannel(interp, chan);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetInterp --
 *
 *	Helper function to find a slave interpreter given a pathname.
 *
 * Results:
 *	Returns the slave interpreter known by that name in the calling
 *	interpreter, or NULL if no interpreter known by that name exists. 
 *
 * Side effects:
 *	Assigns to the pointer variable passed in, if not NULL.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Interp *
GetInterp(interp, masterPtr, path, masterPtrPtr)
    Tcl_Interp *interp;		/* Interp. to start search from. */
    Master *masterPtr;		/* Its master record. */
    char *path;			/* The path (name) of interp. to be found. */
    Master **masterPtrPtr;	/* (Return) its master record. */
{
    Tcl_HashEntry *hPtr;	/* Search element. */
    Slave *slavePtr;		/* Interim slave record. */
    char **argv;		/* Split-up path (name) for interp to find. */
    int argc, i;		/* Loop indices. */
    Tcl_Interp *searchInterp;	/* Interim storage for interp. to find. */

    if (masterPtrPtr != (Master **) NULL) *masterPtrPtr = masterPtr;
    
    if (Tcl_SplitList(interp, path, &argc, &argv) != TCL_OK) {
        return (Tcl_Interp *) NULL;
    }

    for (searchInterp = interp, i = 0; i < argc; i++) {
        
        hPtr = Tcl_FindHashEntry(&(masterPtr->slaveTable), argv[i]);
        if (hPtr == (Tcl_HashEntry *) NULL) {
            ckfree((char *) argv);
            return (Tcl_Interp *) NULL;
        }
        slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
        searchInterp = slavePtr->slaveInterp;
        if (searchInterp == (Tcl_Interp *) NULL) {
            ckfree((char *) argv);
            return (Tcl_Interp *) NULL;
        }
        masterPtr = (Master *) Tcl_GetAssocData(searchInterp,
                "tclMasterRecord", NULL);
        if (masterPtrPtr != (Master **) NULL) *masterPtrPtr = masterPtr;
        if (masterPtr == (Master *) NULL) {
            ckfree((char *) argv);
            return (Tcl_Interp *) NULL;
        }
    }
    ckfree((char *) argv);
    return searchInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSlave --
 *
 *	Helper function to do the actual work of creating a slave interp
 *	and new object command. Also optionally makes the new slave
 *	interpreter "safe".
 *
 * Results:
 *	Returns the new Tcl_Interp * if successful or NULL if not. If failed,
 *	the result of the invoking interpreter contains an error message.
 *
 * Side effects:
 *	Creates a new slave interpreter and a new object command.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Interp *
CreateSlave(interp, slavePath, safe)
    Tcl_Interp *interp;			/* Interp. to start search from. */
    char *slavePath;			/* Path (name) of slave to create. */
    int safe;				/* Should we make it "safe"? */
{
    Master *masterPtr;			/* Master record. */
    Tcl_Interp *slaveInterp;		/* Ptr to slave interpreter. */
    Tcl_Interp *masterInterp;		/* Ptr to master interp for slave. */
    Slave *slavePtr;			/* Slave record. */
    Tcl_HashEntry *hPtr;		/* Entry into interp hashtable. */
    int new;				/* Indicates whether new entry. */
    int argc;				/* Count of elements in slavePath. */
    char **argv;			/* Elements in slavePath. */
    char *masterPath;			/* Path to its master. */

    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord",
            NULL); 
    if (masterPtr == (Master *) NULL) {
        panic("CreatSlave: could not find master record");
    }

    if (Tcl_SplitList(interp, slavePath, &argc, &argv) != TCL_OK) {
        return (Tcl_Interp *) NULL;
    }

    if (argc < 2) {
        masterInterp = interp;
        if (argc == 1) {
            slavePath = argv[0];
        }
    } else {
        masterPath = Tcl_Merge(argc-1, argv);
        masterInterp = GetInterp(interp, masterPtr, masterPath, &masterPtr);
        if (masterInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "interpreter named \"", masterPath,
                    "\" not found", (char *) NULL);
            ckfree((char *) argv);
            ckfree((char *) masterPath);
            return (Tcl_Interp *) NULL;
        }
        ckfree((char *) masterPath);
        slavePath = argv[argc-1];
        if (!safe) {
            safe = masterPtr->isSafe;
        }
    }
    hPtr = Tcl_CreateHashEntry(&(masterPtr->slaveTable), slavePath, &new);
    if (new == 0) {
        Tcl_AppendResult(interp, "interpreter named \"", slavePath,
                "\" already exists, cannot create", (char *) NULL);
        ckfree((char *) argv);
        return (Tcl_Interp *) NULL;
    }
    slaveInterp = Tcl_CreateInterp();
    if (slaveInterp == (Tcl_Interp *) NULL) {
        panic("CreateSlave: out of memory while creating a new interpreter");
    }
    slavePtr = (Slave *) ckalloc((unsigned) sizeof(Slave));
    slavePtr->masterInterp = masterInterp;
    slavePtr->slaveEntry = hPtr;
    slavePtr->slaveInterp = slaveInterp;
    slavePtr->interpCmd = Tcl_CreateCommand(masterInterp, slavePath,
            SlaveObjectCmd, (ClientData) slaveInterp, SlaveObjectDeleteProc);
    Tcl_InitHashTable(&(slavePtr->aliasTable), TCL_STRING_KEYS);
    (void) Tcl_SetAssocData(slaveInterp, "tclSlaveRecord",
            SlaveRecordDeleteProc, (ClientData) slavePtr);
    Tcl_SetHashValue(hPtr, (ClientData) slavePtr);
    Tcl_SetVar(slaveInterp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
    
    if (((safe) && (MakeSafe(slaveInterp) == TCL_ERROR)) ||
            ((!safe) && (Tcl_Init(slaveInterp) == TCL_ERROR))) {
        Tcl_ResetResult(interp);
        Tcl_AddErrorInfo(interp, Tcl_GetVar2(slaveInterp, "errorInfo", (char *)
                NULL, TCL_GLOBAL_ONLY));
        Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                Tcl_GetVar2(slaveInterp, "errorCode", (char *) NULL,
                        TCL_GLOBAL_ONLY),
                TCL_GLOBAL_ONLY);
        if (slaveInterp->freeProc != NULL) {
            interp->result = slaveInterp->result;
            interp->freeProc = slaveInterp->freeProc;
            slaveInterp->freeProc = 0;
        } else {
            Tcl_SetResult(interp, slaveInterp->result, TCL_VOLATILE);
        }
        Tcl_ResetResult(slaveInterp);
        (void) Tcl_DeleteCommand(masterInterp, slavePath);
        slaveInterp = (Tcl_Interp *) NULL;
    }
    ckfree((char *) argv);
    return slaveInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateInterpObject -
 *
 *	Helper function to do the actual work of creating a new interpreter
 *	and an object command. 
 *
 * Results:
 *	A Tcl result.
 *
 * Side effects:
 *	See user documentation for details.
 *
 *----------------------------------------------------------------------
 */

static int
CreateInterpObject(interp, argc, argv)
    Tcl_Interp *interp;			/* Invoking interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int safe;				/* Create a safe interpreter? */
    Master *masterPtr;			/* Master record. */
    int moreFlags;			/* Expecting more flag args? */
    char *slavePath;			/* Name of slave. */
    char localSlaveName[200];		/* Local area for creating names. */
    int i;				/* Loop counter. */
    size_t len;				/* Length of option argument. */
    static int interpCounter = 0;	/* Unique id for created names. */

    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL); 
    if (masterPtr == (Master *) NULL) {
        panic("CreateInterpObject: could not find master record");
    }
    moreFlags = 1;
    slavePath = NULL;
    safe = masterPtr->isSafe;
    
    if (argc < 2 || argc > 5) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " create ?-safe? ?--? ?path?\"", (char *) NULL);
        return TCL_ERROR;
    }
    for (i = 2; i < argc; i++) {
        len = strlen(argv[i]);
        if ((argv[i][0] == '-') && (moreFlags != 0)) {
            if ((argv[i][1] == 's') && (strncmp(argv[i], "-safe", len) == 0)
                && (len > 1)){
                safe = 1;
            } else if ((strncmp(argv[i], "--", len) == 0) && (len > 1)) {
                moreFlags = 0;
            } else {
                Tcl_AppendResult(interp, "bad option \"", argv[i],
                        "\": should be -safe", (char *) NULL);
                return TCL_ERROR;
            }
        } else {
            slavePath = argv[i];
        }
    }
    if (slavePath == (char *) NULL) {
        sprintf(localSlaveName, "interp%d", interpCounter);
        interpCounter++;
        slavePath = localSlaveName;
    }
    if (CreateSlave(interp, slavePath, safe) != NULL) {
        Tcl_AppendResult(interp, slavePath, (char *) NULL);
        return TCL_OK;
    } else {
        /*
         * CreateSlave already set interp->result if there was an error,
         * so we do not do it here.
         */
        return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOneInterpObject --
 *
 *	Helper function for DeleteInterpObject. It deals with deleting one
 *	interpreter at a time.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes an interpreter and its interpreter object command.
 *
 *----------------------------------------------------------------------
 */

static int
DeleteOneInterpObject(interp, path)
    Tcl_Interp *interp;			/* Interpreter for reporting errors. */
    char *path;				/* Path of interpreter to delete. */
{
    Master *masterPtr;			/* Interim storage for master record.*/
    Slave *slavePtr;			/* Interim storage for slave record. */
    Tcl_Interp *masterInterp;		/* Master of interp. to delete. */
    Tcl_HashEntry *hPtr;		/* Search element. */
    int localArgc;			/* Local copy of count of elements in
                                         * path (name) of interp. to delete. */
    char **localArgv;			/* Local copy of path. */
    char *slaveName;			/* Last component in path. */
    char *masterPath;			/* One-before-last component in path.*/

    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("DeleteInterpObject: could not find master record");
    }
    if (Tcl_SplitList(interp, path, &localArgc, &localArgv) != TCL_OK) {
        Tcl_AppendResult(interp, "bad interpreter path \"", path,
                "\"", (char *) NULL);
        return TCL_ERROR;
    }
    if (localArgc < 2) {
        masterInterp = interp;
        if (localArgc == 0) {
            slaveName = "";
        } else {
            slaveName = localArgv[0];
        }
    } else {
        masterPath = Tcl_Merge(localArgc-1, localArgv);
        masterInterp = GetInterp(interp, masterPtr, masterPath, &masterPtr);
        if (masterInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "interpreter named \"", masterPath,
                    "\" not found", (char *) NULL);
            ckfree((char *) localArgv);
            ckfree((char *) masterPath);
            return TCL_ERROR;
        }
        ckfree((char *) masterPath);
        slaveName = localArgv[localArgc-1];
    }
    hPtr = Tcl_FindHashEntry(&(masterPtr->slaveTable), slaveName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        ckfree((char *) localArgv);
        Tcl_AppendResult(interp, "interpreter named \"", path,
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
    slaveName = Tcl_GetCommandName(masterInterp, slavePtr->interpCmd);
    if (Tcl_DeleteCommand(masterInterp, slaveName) != 0) {
        ckfree((char *) localArgv);
        Tcl_AppendResult(interp, "interpreter named \"", path,
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    ckfree((char *) localArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteInterpObject --
 *
 *	Helper function to do the work of deleting zero or more
 *	interpreters and their interpreter object commands.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes interpreters and their interpreter object command.
 *
 *----------------------------------------------------------------------
 */

static int
DeleteInterpObject(interp, argc, argv)
    Tcl_Interp *interp;			/* Interpreter start search from. */
    int argc;				/* Number of arguments in vector. */
    char **argv;			/* Contains path to interps to
                                         * delete. */
{
    int i;
    
    for (i = 2; i < argc; i++) {
        if (DeleteOneInterpObject(interp, argv[i]) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasHelper --
 *
 *	Helper function to do the work to actually create an alias or
 *	delete an alias.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	An alias command is created and entered into the alias table
 *	for the slave interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
AliasHelper(curInterp, slaveInterp, masterInterp, masterPtr,
     aliasName, targetName, argc, argv)
    Tcl_Interp *curInterp;		/* Interp that invoked this proc. */
    Tcl_Interp *slaveInterp;		/* Interp where alias cmd will live
                                         * or from which alias will be
                                         * deleted. */
    Tcl_Interp *masterInterp;		/* Interp where target cmd will be. */
    Master *masterPtr;			/* Master record for target interp. */
    char *aliasName;			/* Name of alias cmd. */
    char *targetName;			/* Name of target cmd. */
    int argc;				/* Additional arguments to store */
    char **argv;			/* with alias. */
{
    Alias *aliasPtr;			/* Storage for alias data. */
    Alias *tmpAliasPtr;			/* Temp storage for alias to delete. */
    Tcl_HashEntry *hPtr;		/* Entry into interp hashtable. */
    int i;				/* Loop index. */
    int new;				/* Is it a new hash entry? */
    Target *targetPtr;			/* Maps from target command in master
                                         * to source command in slave. */
    Slave *slavePtr;			/* Maps from source command in slave
                                         * to target command in master. */

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord", NULL);

    /*
     * Fix it up if there is no slave record. This can happen if someone
     * uses "" as the source for an alias.
     */
    
    if (slavePtr == (Slave *) NULL) {
        slavePtr = (Slave *) ckalloc((unsigned) sizeof(Slave));
        slavePtr->masterInterp = (Tcl_Interp *) NULL;
        slavePtr->slaveEntry = (Tcl_HashEntry *) NULL;
        slavePtr->slaveInterp = slaveInterp;
        slavePtr->interpCmd = (Tcl_Command) NULL;
        Tcl_InitHashTable(&(slavePtr->aliasTable), TCL_STRING_KEYS);
        (void) Tcl_SetAssocData(slaveInterp, "tclSlaveRecord",
                SlaveRecordDeleteProc, (ClientData) slavePtr);
    }

    if ((targetName == (char *) NULL) || (targetName[0] == '\0')) {
        if (argc != 0) {
            Tcl_AppendResult(curInterp, "malformed command: should be",
                " \"alias ",  aliasName, " {}\"", (char *) NULL);
            return TCL_ERROR;
        }

        return DeleteAlias(curInterp, slaveInterp, aliasName);
    }
    
    aliasPtr = (Alias *) ckalloc((unsigned) sizeof(Alias));
    aliasPtr->aliasName = (char *) ckalloc((unsigned) strlen(aliasName)+1);
    aliasPtr->targetName = (char *) ckalloc((unsigned) strlen(targetName)+1);
    strcpy(aliasPtr->aliasName, aliasName);
    strcpy(aliasPtr->targetName, targetName);
    aliasPtr->targetInterp = masterInterp;

    aliasPtr->argv = (char **) NULL;
    aliasPtr->argc = argc;
    if (aliasPtr->argc > 0) {
        aliasPtr->argv = (char **) ckalloc((unsigned) sizeof(char *) *
                aliasPtr->argc);
        for (i = 0; i < argc; i++) {
            aliasPtr->argv[i] = (char *) ckalloc((unsigned) strlen(argv[i])+1);
            strcpy(aliasPtr->argv[i], argv[i]);
        }
    }

    if (TclPreventAliasLoop(curInterp, slaveInterp, aliasName, AliasCmd,
            (ClientData) aliasPtr) != TCL_OK) {
        for (i = 0; i < argc; i++) {
            ckfree(aliasPtr->argv[i]);
        }
        if (aliasPtr->argv != (char **) NULL) {
            ckfree((char *) aliasPtr->argv);
        }
        ckfree(aliasPtr->aliasName);
        ckfree(aliasPtr->targetName);
        ckfree((char *) aliasPtr);
        
        return TCL_ERROR;
    }
    
    aliasPtr->slaveCmd = Tcl_CreateCommand(slaveInterp, aliasName, AliasCmd,
            (ClientData) aliasPtr, AliasCmdDeleteProc);

    /*
     * Make an entry in the alias table. If it already exists delete
     * the alias command. Then retry.
     */

    do {
        hPtr = Tcl_CreateHashEntry(&(slavePtr->aliasTable), aliasName, &new);
        if (new == 0) {
            tmpAliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
            (void) Tcl_DeleteCommand(slaveInterp, tmpAliasPtr->aliasName);
            Tcl_DeleteHashEntry(hPtr);
        }
    } while (new == 0);
    aliasPtr->aliasEntry = hPtr;
    Tcl_SetHashValue(hPtr, (ClientData) aliasPtr);

    targetPtr = (Target *) ckalloc((unsigned) sizeof(Target));
    targetPtr->slaveCmd = aliasPtr->slaveCmd;
    targetPtr->slaveInterp = slaveInterp;

    do {
        hPtr = Tcl_CreateHashEntry(&(masterPtr->targetTable),
                (char *) aliasCounter, &new);
	aliasCounter++;
    } while (new == 0);

    Tcl_SetHashValue(hPtr, (ClientData) targetPtr);

    aliasPtr->targetEntry = hPtr;

    curInterp->result = aliasPtr->aliasName;
            
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveAliasHelper -
 *
 *	Handles the different forms of the "interp alias" command:
 *	- interp alias slavePath aliasName
 *		Describes an alias.
 *	- interp alias slavePath aliasName {}
 *		Deletes an alias.
 *	- interp alias slavePath srcCmd masterPath targetCmd args...
 *		Creates an alias.
 *
 * Results:
 *	A Tcl result.
 *
 * Side effects:
 *	See user documentation for details.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveAliasHelper(interp, argc, argv)
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Master *masterPtr;			/* Master record for current interp. */
    Tcl_Interp *slaveInterp,		/* Interpreters used when */
        *masterInterp;			/* creating an alias btn siblings. */
    Master *masterMasterPtr;		/* Master record for master interp. */

    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("SlaveAliasHelper: could not find master record");
    }
    if (argc < 4) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " alias slavePath slaveCmd masterPath masterCmd ?args ..?\"",
                (char *) NULL);
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr, argv[2], NULL);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendResult(interp, "could not find interpreter \"",
            argv[2], "\"", (char *) NULL);
        return TCL_ERROR;
    }
    if (argc == 4) {
        return DescribeAlias(interp, slaveInterp, argv[3]);
    }
    if (argc == 5 && strcmp(argv[4], "") == 0) {
        return DeleteAlias(interp, slaveInterp, argv[3]);
    }
    if (argc < 6) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " alias slavePath slaveCmd masterPath masterCmd ?args ..?\"",
                (char *) NULL);
        return TCL_ERROR;
    }
    masterInterp = GetInterp(interp, masterPtr, argv[4], &masterMasterPtr);
    if (masterInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendResult(interp, "could not find interpreter \"",
            argv[4], "\"", (char *) NULL);
        return TCL_ERROR;
    }
    return AliasHelper(interp, slaveInterp, masterInterp, masterMasterPtr,
            argv[3], argv[5], argc-6, argv+6);
}

/*
 *----------------------------------------------------------------------
 *
 * DescribeAlias --
 *
 *	Sets interp->result to a Tcl list describing the given alias in the
 *	given interpreter: its target command and the additional arguments
 *	to prepend to any invocation of the alias.
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
DescribeAlias(interp, slaveInterp, aliasName)
    Tcl_Interp *interp;		/* Interpreter for result and errors. */
    Tcl_Interp *slaveInterp;	/* Interpreter defining alias. */
    char *aliasName;		/* Name of alias to describe. */
{
    Slave *slavePtr;		/* Slave record for slave interpreter. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Alias *aliasPtr;		/* Structure describing alias. */
    int i;			/* Loop variable. */

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",
            NULL);
    if (slavePtr == (Slave *) NULL) {
        panic("DescribeAlias: could not find slave record");
    }
    hPtr = Tcl_FindHashEntry(&(slavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        return TCL_OK;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
    Tcl_AppendResult(interp, aliasPtr->targetName, (char *) NULL);
    for (i = 0; i < aliasPtr->argc; i++) {
        Tcl_AppendElement(interp, aliasPtr->argv[i]);
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteAlias --
 *
 *	Deletes the given alias from the slave interpreter given.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes the alias from the slave interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
DeleteAlias(interp, slaveInterp, aliasName)
    Tcl_Interp *interp;		/* Interpreter for result and errors. */
    Tcl_Interp *slaveInterp;	/* Interpreter defining alias. */
    char *aliasName;		/* Name of alias to delete. */
{
    Slave *slavePtr;		/* Slave record for slave interpreter. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Alias *aliasPtr;		/* Structure describing alias to delete. */

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",
            NULL);
    if (slavePtr == (Slave *) NULL) {
        panic("DeleteAlias: could not find slave record");
    }
    
    /*
     * Get the alias from the alias table, determine the current
     * true name of the alias (it may have been renamed!) and then
     * delete the true command name. The deleteProc on the alias
     * command will take care of removing the entry from the alias
     * table.
     */

    hPtr = Tcl_FindHashEntry(&(slavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendResult(interp, "alias \"", aliasName, "\" not found",
                (char *) NULL);
        return TCL_ERROR;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
    aliasName = Tcl_GetCommandName(slaveInterp, aliasPtr->slaveCmd);

    /*
     * NOTE: The deleteProc for this command will delete the
     * alias from the hash table. The deleteProc will also
     * delete the target information from the master interpreter
     * target table.
     */

    if (Tcl_DeleteCommand(slaveInterp, aliasName) != 0) {
        panic("DeleteAlias: did not find alias to be deleted");
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetInterpPath --
 *
 *	Sets the result of the asking interpreter to a proper Tcl list
 *	containing the names of interpreters between the asking and
 *	target interpreters. The target interpreter must be either the
 *	same as the asking interpreter or one of its slaves (including
 *	recursively).
 *
 * Results:
 *	TCL_OK if the target interpreter is the same as, or a descendant
 *	of, the asking interpreter; TCL_ERROR else. This way one can
 *	distinguish between the case where the asking and target interps
 *	are the same (an empty list is the result, and TCL_OK is returned)
 *	and when the target is not a descendant of the asking interpreter
 *	(in which case the Tcl result is an error message and the function
 *	returns TCL_ERROR).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetInterpPath(askingInterp, targetInterp)
    Tcl_Interp *askingInterp;	/* Interpreter to start search from. */
    Tcl_Interp *targetInterp;	/* Interpreter to find. */
{
    Master *masterPtr;		/* Interim storage for Master record. */
    Slave *slavePtr;		/* Interim storage for Slave record. */
    
    if (targetInterp == askingInterp) {
        return TCL_OK;
    }
    if (targetInterp == (Tcl_Interp *) NULL) {
        return TCL_ERROR;
    }
    slavePtr = (Slave *) Tcl_GetAssocData(targetInterp, "tclSlaveRecord",
            NULL);
    if (slavePtr == (Slave *) NULL) {
        return TCL_ERROR;
    }
    if (Tcl_GetInterpPath(askingInterp, slavePtr->masterInterp) == TCL_ERROR) {
        /*
         * AskingInterp->result was set by recursive call.
         */
        return TCL_ERROR;
    }
    masterPtr = (Master *) Tcl_GetAssocData(slavePtr->masterInterp,
            "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("Tcl_GetInterpPath: could not find master record");
    }
    Tcl_AppendElement(askingInterp, Tcl_GetHashKey(&(masterPtr->slaveTable),
            slavePtr->slaveEntry));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTarget --
 *
 *	Sets the result of the invoking interpreter to a path name for
 *	the target interpreter of an alias in one of the slaves.
 *
 * Results:
 *	TCL_OK if the target interpreter of the alias is a slave of the
 *	invoking interpreter, TCL_ERROR else.
 *
 * Side effects:
 *	Sets the result of the invoking interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
GetTarget(askingInterp, path, aliasName)
    Tcl_Interp *askingInterp;	/* Interpreter to start search from. */
    char *path;			/* The path of the interp to find. */
    char *aliasName;		/* The target of this allias. */
{
    Tcl_Interp *slaveInterp;	/* Interim storage for slave. */
    Slave *slaveSlavePtr;	/* Its Slave record. */
    Master *masterPtr;		/* Interim storage for Master record. */
    Tcl_HashEntry *hPtr;	/* Search element. */
    Alias *aliasPtr;		/* Data describing the alias. */

    Tcl_ResetResult(askingInterp);

    masterPtr = (Master *) Tcl_GetAssocData(askingInterp, "tclMasterRecord",
            NULL);
    if (masterPtr == (Master *) NULL) {
        panic("GetTarget: could not find master record");
    }
    slaveInterp = GetInterp(askingInterp, masterPtr, path, NULL);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendResult(askingInterp, "could not find interpreter \"",
            path, "\"", (char *) NULL);
        return TCL_ERROR;
    }
    slaveSlavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",
            NULL);
    if (slaveSlavePtr == (Slave *) NULL) {
        panic("GetTarget: could not find slave record");
    }
    hPtr = Tcl_FindHashEntry(&(slaveSlavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendResult(askingInterp, "alias \"", aliasName, "\" in path \"",
                path, "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
    if (aliasPtr == (Alias *) NULL) {
        panic("GetTarget: could not find alias record");
    }
    if (Tcl_GetInterpPath(askingInterp, aliasPtr->targetInterp) == TCL_ERROR) {
        Tcl_ResetResult(askingInterp);
        Tcl_AppendResult(askingInterp, "target interpreter for alias \"",
                aliasName, "\" in path \"", path, "\" is not my descendant",
                (char *) NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InterpCmd --
 *
 *	This procedure is invoked to process the "interp" Tcl command.
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
Tcl_InterpCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Unused. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    Tcl_Interp *masterInterp;		/* A master. */
    Master *masterPtr;			/* Master record for current interp. */
    Slave *slavePtr;			/* Record for slave interp. */
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    size_t len;				/* Length of command name. */
    int result;				/* Result of eval. */
    char *cmdName;			/* Name of sub command to do. */
    char *cmd;				/* Command to eval. */
    Tcl_Channel chan;			/* Channel to share or transfer. */

    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                " cmd ?arg ...?\"", (char *) NULL);
        return TCL_ERROR;
    }
    cmdName = argv[1];

    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("Tcl_InterpCmd: could not find master record");
    }

    len = strlen(cmdName);
    
    if (cmdName[0] == 'a') {
        if ((strncmp(cmdName, "alias", len) == 0) && (len <= 5)) {
            return SlaveAliasHelper(interp, argc, argv);
        }

        if (strcmp(cmdName, "aliases") == 0) {
            if (argc != 2 && argc != 3) {
                Tcl_AppendResult(interp, "wrong # args: should be \"",
                        argv[0], " aliases ?path?\"", (char *) NULL);
                return TCL_ERROR;
            }
            if (argc == 3) {
                slaveInterp = GetInterp(interp, masterPtr, argv[2], NULL);
                if (slaveInterp == (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp, "interpreter \"",
                            argv[2], "\" not found", (char *) NULL);
                    return TCL_ERROR;
                }
            } else {
                slaveInterp = interp;
            }
            slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp,
                    "tclSlaveRecord", NULL);
            if (slavePtr == (Slave *) NULL) {
                return TCL_OK;
            }
            for (hPtr = Tcl_FirstHashEntry(&(slavePtr->aliasTable), &hSearch);
                 hPtr != NULL;
                 hPtr = Tcl_NextHashEntry(&hSearch)) {
                Tcl_AppendElement(interp,
                        Tcl_GetHashKey(&(slavePtr->aliasTable), hPtr));
            }
            return TCL_OK;
        }
    }

    if ((cmdName[0] == 'c') && (strncmp(cmdName, "create", len) == 0)) {
        return CreateInterpObject(interp, argc, argv);
    }

    if ((cmdName[0] == 'd') && (strncmp(cmdName, "delete", len) == 0)) {
        return DeleteInterpObject(interp, argc, argv);
    }

    if (cmdName[0] == 'e') {
        if ((strncmp(cmdName, "exists", len) == 0) && (len > 1)) {
            if (argc > 3) {
                Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                        " exists ?path?\"", (char *) NULL);
                return TCL_ERROR;
            }
            if (argc == 3) {
                if (GetInterp(interp, masterPtr, argv[2], NULL) ==
                        (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp, "0", (char *) NULL);
                } else {
                    Tcl_AppendResult(interp, "1", (char *) NULL);
                }
            } else {
                Tcl_AppendResult(interp, "1", (char *) NULL);
            }
            return TCL_OK;
        }
        if ((strncmp(cmdName, "eval", len) == 0) && (len > 1)) {
            if (argc < 4) {
                Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                        " eval path arg ?arg ...?\"", (char *) NULL);
                return TCL_ERROR;
            }
            slaveInterp = GetInterp(interp, masterPtr, argv[2], NULL);
            if (slaveInterp == (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "interpreter named \"", argv[2],
                        "\" not found", (char *) NULL);
                return TCL_ERROR;
            }
            cmd = Tcl_Concat(argc-3, argv+3);
            Tcl_Preserve((ClientData) slaveInterp);
            result = Tcl_Eval(slaveInterp, cmd);
            ckfree((char *) cmd);

            /*
             * Now make the result and any error information accessible. We
             * have to be careful because the slave interpreter and the current
             * interpreter can be the same - do not destroy the result.. This
             * can happen if an interpreter contains an alias which is directed
             * at a target command in the same interpreter.
             */

            if (interp != slaveInterp) {
                if (result == TCL_ERROR) {

                    /*
                     * An error occurred, so transfer error information from
                     * the target interpreter back to our interpreter.  Must
                     * clear interp's result before calling Tcl_AddErrorInfo,
                     * since Tcl_AddErrorInfo will store the interp's result in
                     * errorInfo before appending slaveInterp's $errorInfo;
                     * we've already got everything we need in the slave
                     * interpreter's $errorInfo.
                     */

                    Tcl_ResetResult(interp);
                    Tcl_AddErrorInfo(interp, Tcl_GetVar2(slaveInterp,
                            "errorInfo", (char *) NULL, TCL_GLOBAL_ONLY));
                    Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                            Tcl_GetVar2(slaveInterp, "errorCode", (char *)
                                    NULL, TCL_GLOBAL_ONLY),
                            TCL_GLOBAL_ONLY);
                }
                if (slaveInterp->freeProc != NULL) {
                    interp->result = slaveInterp->result;
                    interp->freeProc = slaveInterp->freeProc;
                    slaveInterp->freeProc = 0;
                } else {
                    Tcl_SetResult(interp, slaveInterp->result, TCL_VOLATILE);
                }
                Tcl_ResetResult(slaveInterp);
            }
            Tcl_Release((ClientData) slaveInterp);
            return result;        
        }
    }

    if ((cmdName[0] == 'i') && (strncmp(cmdName, "issafe", len) == 0)) {
        if (argc > 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " issafe ?path?\"", (char *) NULL);
            return TCL_ERROR;
        }
        if (argc == 3) {
            slaveInterp = GetInterp(interp, masterPtr, argv[2], &masterPtr);
            if (slaveInterp == (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "interpreter \"", argv[2],
                        "\" not found", (char *) NULL);
                return TCL_ERROR;
            }
        }
        if (masterPtr->isSafe == 0) {
            Tcl_AppendResult(interp, "0", (char *) NULL);
        } else {
            Tcl_AppendResult(interp, "1", (char *) NULL);
        }
        return TCL_OK;
    }
    
    if (cmdName[0] == 's') {
        if ((strncmp(cmdName, "slaves", len) == 0) && (len > 1)) {
            if (argc != 2 && argc != 3) {
                Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                        " slaves ?path?\"", (char *) NULL);
                return TCL_ERROR;
            }
            if (argc == 3) {
                if (GetInterp(interp, masterPtr, argv[2], &masterPtr) ==
                        (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp, "interpreter \"", argv[2],
                            "\" not found", (char *) NULL);
                    return TCL_ERROR;
                }
            }
            for (hPtr = Tcl_FirstHashEntry(&(masterPtr->slaveTable), &hSearch);
                 hPtr != NULL;
                 hPtr = Tcl_NextHashEntry(&hSearch)) {
                Tcl_AppendElement(interp,
                        Tcl_GetHashKey(&(masterPtr->slaveTable), hPtr));
            }
            return TCL_OK;
        }
        if ((strncmp(cmdName, "share", len) == 0) && (len > 1)) {
            if (argc != 5) {
                Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                        " share srcPath channelId destPath\"", (char *) NULL);
                return TCL_ERROR;
            }
            masterInterp = GetInterp(interp, masterPtr, argv[2], NULL);
            if (masterInterp == (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "interpreter \"", argv[2],
                        "\" not found", (char *) NULL);
                return TCL_ERROR;
            }
            slaveInterp = GetInterp(interp, masterPtr, argv[4], NULL);
            if (slaveInterp == (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "interpreter \"", argv[4],
                        "\" not found", (char *) NULL);
                return TCL_ERROR;
            }
            chan = Tcl_GetChannel(masterInterp, argv[3], NULL);
            if (chan == (Tcl_Channel) NULL) {
                if (interp != masterInterp) {
                    Tcl_AppendResult(interp, masterInterp->result,
                            (char *) NULL);
                    Tcl_ResetResult(masterInterp);
                }
                return TCL_ERROR;
            }
            Tcl_RegisterChannel(slaveInterp, chan);
            return TCL_OK;
        }
    }

    if ((cmdName[0] == 't') && (strncmp(cmdName, "target", len) == 0)) {
        if (argc != 4) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " target path alias\"", (char *) NULL);
            return TCL_ERROR;
        }
        return GetTarget(interp, argv[2], argv[3]);
    }

    if ((cmdName[0] == 't') && (strncmp(cmdName, "transfer", len) == 0)) {
        if (argc != 5) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " transfer srcPath channelId destPath\"", (char *) NULL);
            return TCL_ERROR;
        }
        masterInterp = GetInterp(interp, masterPtr, argv[2], NULL);
        if (masterInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "interpreter \"", argv[2],
                    "\" not found", (char *) NULL);
            return TCL_ERROR;
        }
        slaveInterp = GetInterp(interp, masterPtr, argv[4], NULL);
        if (slaveInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "interpreter \"", argv[4],
                    "\" not found", (char *) NULL);
            return TCL_ERROR;
        }
        chan = Tcl_GetChannel(masterInterp, argv[3], NULL);
        if (chan == (Tcl_Channel) NULL) {
            if (interp != masterInterp) {
                Tcl_AppendResult(interp, masterInterp->result, (char *) NULL);
                Tcl_ResetResult(masterInterp);
            }
            return TCL_ERROR;
        }
        Tcl_RegisterChannel(slaveInterp, chan);
        if (Tcl_UnregisterChannel(masterInterp, chan) != TCL_OK) {
            if (interp != masterInterp) {
                Tcl_AppendResult(interp, masterInterp->result, (char *) NULL);
                Tcl_ResetResult(masterInterp);
            }
            return TCL_ERROR;
        }

        return TCL_OK;
    }
        
    Tcl_AppendResult(interp, "bad option \"", argv[1],
            "\": should be alias, aliases, create, delete, exists, eval, ",
            "issafe, share, slaves, target or transfer", (char *) NULL);
    return TCL_ERROR;    
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveObjectCmd --
 *
 *	Command to manipulate an interpreter, e.g. to send commands to it
 *	to be evaluated. One such command exists for each slave interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See user documentation for details.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveObjectCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Slave interpreter. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Master *masterPtr;			/* Master record for slave interp. */
    Slave *slavePtr;			/* Slave record. */
    Tcl_Interp *slaveInterp;		/* Slave interpreter. */
    char *cmdName;			/* Name of command to do. */
    char *cmd;				/* Command to evaluate in slave
                                         * interpreter. */
    Alias *aliasPtr;			/* Alias information. */
    Tcl_HashEntry *hPtr;		/* For local searches. */
    Tcl_HashSearch hSearch;		/* For local searches. */
    int result;				/* Loop counter, status return. */
    size_t len;				/* Length of command name. */
    
    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
            " cmd ?arg ...?\"", (char *) NULL);
        return TCL_ERROR;
    }

    slaveInterp = (Tcl_Interp *) clientData;
    if (slaveInterp == (Tcl_Interp *) NULL) {
	Tcl_AppendResult(interp, "interpreter ", argv[0], " has been deleted",
		(char *) NULL);
	return TCL_ERROR;
    }

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp,
            "tclSlaveRecord", NULL);
    if (slavePtr == (Slave *) NULL) {
        panic("SlaveObjectCmd: could not find slave record");
    }

    cmdName = argv[1];
    len = strlen(cmdName);

    if (cmdName[0] == 'a') {
        if (strncmp(cmdName, "alias", len) == 0) {
            switch (argc-2) {
                case 0:
                    Tcl_AppendResult(interp, "wrong # args: should be \"",
                            argv[0], " alias aliasName ?targetName? ?args..?",
                            (char *) NULL);
                    return TCL_ERROR;

                case 1:

                    /*
                     * Return the name of the command in the current
                     * interpreter for which the argument is an alias in the
                     * slave interpreter, and the list of saved arguments
                     */

                    return DescribeAlias(interp, slaveInterp, argv[2]);

                default:
                    masterPtr = (Master *) Tcl_GetAssocData(interp,
                            "tclMasterRecord", NULL);
                    if (masterPtr == (Master *) NULL) {
                        panic("SlaveObjectCmd: could not find master record");
                    }
                    return AliasHelper(interp, slaveInterp, interp, masterPtr,
                            argv[2], argv[3], argc-4, argv+4);
            }
        }

        if (strncmp(cmdName, "aliases", len) == 0) {

            /*
             * Return the names of all the aliases created in the
             * slave interpreter.
             */

            for (hPtr = Tcl_FirstHashEntry(&(slavePtr->aliasTable),
                    &hSearch);
                 hPtr != (Tcl_HashEntry *) NULL;
                 hPtr = Tcl_NextHashEntry(&hSearch)) {
                aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
                Tcl_AppendElement(interp, aliasPtr->aliasName);
            }
            return TCL_OK;
        }
    }
    

    if ((cmdName[0] == 'e') && (strncmp(cmdName, "eval", len) == 0)) {
        if (argc < 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " eval arg ?arg ...?\"", (char *) NULL);
            return TCL_ERROR;
        }

        cmd = Tcl_Concat(argc-2, argv+2);
        Tcl_Preserve((ClientData) slaveInterp);
        result = Tcl_Eval(slaveInterp, cmd);
        ckfree((char *) cmd);

        /*
         * Now make the result and any error information accessible. We have
         * to be careful because the slave interpreter and the current
         * interpreter can be the same - do not destroy the result.. This
         * can happen if an interpreter contains an alias which is directed
         * at a target command in the same interpreter.
         */

        if (interp != slaveInterp) {
            if (result == TCL_ERROR) {

               /*
                * An error occurred, so transfer error information from the
                * destination interpreter back to our interpreter.  Must clear
                * interp's result before calling Tcl_AddErrorInfo, since
                * Tcl_AddErrorInfo will store the interp's result in errorInfo
                * before appending slaveInterp's $errorInfo;
                * we've already got everything we need in the slave
                * interpreter's $errorInfo.
                */

                Tcl_ResetResult(interp);
                Tcl_AddErrorInfo(interp, Tcl_GetVar2(slaveInterp,
                        "errorInfo", (char *) NULL, TCL_GLOBAL_ONLY));
                Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                        Tcl_GetVar2(slaveInterp, "errorCode", (char *) NULL,
                                TCL_GLOBAL_ONLY), TCL_GLOBAL_ONLY);
            }
            if (slaveInterp->freeProc != NULL) {
                interp->result = slaveInterp->result;
                interp->freeProc = slaveInterp->freeProc;
                slaveInterp->freeProc = 0;
            } else {
                Tcl_SetResult(interp, slaveInterp->result, TCL_VOLATILE);
            }
            Tcl_ResetResult(slaveInterp);
        }
        Tcl_Release((ClientData) slaveInterp);
        return result;
    }

    if ((cmdName[0] == 'i') && (strncmp(cmdName, "issafe", len) == 0)) {
        if (argc > 2) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " issafe\"", (char *) NULL);
            return TCL_ERROR;
        }
        masterPtr = (Master *) Tcl_GetAssocData(slaveInterp,
                "tclMasterRecord", NULL);
        if (masterPtr == (Master *) NULL) {
            panic("SlaveObjectCmd: could not find master record");
        }
        if (masterPtr->isSafe == 1) {
            Tcl_AppendResult(interp, "1", (char *) NULL);
        } else {
            Tcl_AppendResult(interp, "0", (char *) NULL);
        }
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad option \"", argv[1],
            "\": should be alias, aliases, eval or issafe", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveObjectDeleteProc --
 *
 *	Invoked when an object command for a slave interpreter is deleted;
 *	cleans up all state associated with the slave interpreter and destroys
 *	the slave interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up all state associated with the slave interpreter and
 *	destroys the slave interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
SlaveObjectDeleteProc(clientData)
    ClientData clientData;		/* The SlaveRecord for the command. */
{
    Slave *slavePtr;			/* Interim storage for Slave record. */
    Tcl_Interp *slaveInterp;		/* And for a slave interp. */

    slaveInterp = (Tcl_Interp *) clientData;
    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",NULL); 
    if (slavePtr == (Slave *) NULL) {
        panic("SlaveObjectDeleteProc: could not find slave record");
    }

    /*
     * Delete the entry in the slave table in the master interpreter now.
     * This is to avoid an infinite loop in the Master hash table cleanup in
     * the master interpreter. This can happen if this slave is being deleted
     * because the master is being deleted and the slave deletion is deferred
     * because it is still active.
     */

    Tcl_DeleteHashEntry(slavePtr->slaveEntry);

    /*
     * Set to NULL so that when the slave record is cleaned up in the slave
     * it does not try to delete the command causing all sorts of grief.
     * See SlaveRecordDeleteProc().
     */

    slavePtr->interpCmd = NULL;

    /*
     * Destroy the interpreter - this will cause all the deleteProcs for
     * all commands (including aliases) to run.
     *
     * NOTE: WE ASSUME THAT THE INTERPRETER HAS NOT BEEN DELETED YET!!
     */

    Tcl_DeleteInterp(slavePtr->slaveInterp);
}

/*
 *----------------------------------------------------------------------
 *
 * AliasCmd --
 *
 *	This is the procedure that services invocations of aliases in a
 *	slave interpreter. One such command exists for each alias. When
 *	invoked, this procedure redirects the invocation to the target
 *	command in the master interpreter as designated by the Alias
 *	record associated with this command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Causes forwarding of the invocation; all possible side effects
 *	may occur as a result of invoking the command to which the
 *	invocation is forwarded.
 *
 *----------------------------------------------------------------------
 */

static int
AliasCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Alias record. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Alias *aliasPtr;			/* Describes the alias. */
    Tcl_CmdInfo cmdInfo;		/* Info about target command. */
    int result;				/* Result of execution. */
    int i, j, addArgc;			/* Loop counters. */
    int localArgc;			/* Local argument count. */
    char **localArgv;			/* Local argument vector. */
    Interp *iPtr;			/* The target interpreter. */
    
    aliasPtr = (Alias *) clientData;

    result = Tcl_GetCommandInfo(aliasPtr->targetInterp, aliasPtr->targetName,
            &cmdInfo);
    if (result == 0) {
        Tcl_AppendResult(interp, "aliased target \"", aliasPtr->targetName,
                "\" for \"", argv[0], "\" not found", (char *) NULL); 
        return TCL_ERROR;
    }
    if (aliasPtr->argc <= 0) {
        localArgv = argv;
        localArgc = argc;
    } else {
        addArgc = aliasPtr->argc;
        localArgc = argc + addArgc;
        localArgv = (char **) ckalloc((unsigned) sizeof(char *) * localArgc);
        localArgv[0] = argv[0];
        for (i = 0, j = 1; i < addArgc; i++, j++) {
            localArgv[j] = aliasPtr->argv[i];
        }
        for (i = 1; i < argc; i++, j++) {
            localArgv[j] = argv[i];
        }
    }

    /*
     * Invoke the redirected command in the target interpreter. Note
     * that we are not calling eval because of possible security holes with
     * $ substitution and bracketed command evaluation.
     *
     * We duplicate some code here from Tcl_Eval to implement recursion
     * level counting and correct deletion of the target interpreter if
     * that was requested but delayed because of in-progress evaluations.
     */

    iPtr = (Interp *) aliasPtr->targetInterp;
    iPtr->numLevels++;
    Tcl_Preserve((ClientData) iPtr);
    Tcl_ResetResult((Tcl_Interp *) iPtr);
    result = (cmdInfo.proc)(cmdInfo.clientData, (Tcl_Interp *) iPtr,
            localArgc, localArgv);
    iPtr->numLevels--;
    if (iPtr->numLevels == 0) {
	if (result == TCL_RETURN) {
	    result = TclUpdateReturnInfo(iPtr);
	}
	if ((result != TCL_OK) && (result != TCL_ERROR)) {
	    Tcl_ResetResult((Tcl_Interp *) iPtr);
	    if (result == TCL_BREAK) {
		iPtr->result = "invoked \"break\" outside of a loop";
	    } else if (result == TCL_CONTINUE) {
		iPtr->result = "invoked \"continue\" outside of a loop";
	    } else {
		iPtr->result = iPtr->resultSpace;
		sprintf(iPtr->resultSpace, "command returned bad code: %d",
			result);
	    }
	    result = TCL_ERROR;
	}
    }

    /*
     * Clean up any locally allocated argument vector structure.
     */
    
    if (localArgv != argv) {
        ckfree((char *) localArgv);
    }
    
    /*
     *
     * NOTE: Need to be careful if the target interpreter and the current
     * interpreter are the same - must not destroy result. This may happen
     * if an alias is created which redirects to a command in the same
     * interpreter as the one in which the source command will be defined.
     * Also: We cannot use aliasPtr any more because the alias may have
     * been deleted.
     */

    if (interp != (Tcl_Interp *) iPtr) {
        if (result == TCL_ERROR) {
	    /*
	     * An error occurred, so transfer error information from the
	     * destination interpreter back to our interpreter.  Some tricky
	     * points:
	     * 1. Must call Tcl_AddErrorInfo in destination interpreter to
	     *    make sure that the errorInfo variable has been initialized
	     *    (it's initialized lazily and might not have been initialized
	     *    yet).
	     * 2. Must clear interp's result before calling Tcl_AddErrorInfo,
	     *    since Tcl_AddErrorInfo will store the interp's result in
	     *    errorInfo before appending aliasPtr->interp's $errorInfo;
	     *    we've already got everything we need in the redirected
	     *    interpreter's $errorInfo.
	     */

	    if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
		Tcl_AddErrorInfo((Tcl_Interp *) iPtr, "");
	    }
	    iPtr->flags &= ~ERR_ALREADY_LOGGED;
            Tcl_ResetResult(interp);
            Tcl_AddErrorInfo(interp, Tcl_GetVar2((Tcl_Interp *) iPtr,
                    "errorInfo", (char *) NULL, TCL_GLOBAL_ONLY));
            Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                    Tcl_GetVar2((Tcl_Interp *) iPtr, "errorCode",
                    (char *) NULL, TCL_GLOBAL_ONLY), TCL_GLOBAL_ONLY);
        }
        if (iPtr->freeProc != NULL) {
            interp->result = iPtr->result;
            interp->freeProc = iPtr->freeProc;
            iPtr->freeProc = 0;
        } else {
            Tcl_SetResult(interp, iPtr->result, TCL_VOLATILE);
        }
        Tcl_ResetResult((Tcl_Interp *) iPtr);
    }
    Tcl_Release((ClientData) iPtr);
    return result;        
}

/*
 *----------------------------------------------------------------------
 *
 * AliasCmdDeleteProc --
 *
 *	Is invoked when an alias command is deleted in a slave. Cleans up
 *	all storage associated with this alias.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the alias record and its entry in the alias table for
 *	the interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
AliasCmdDeleteProc(clientData)
    ClientData clientData;		/* The alias record for this alias. */
{
    Alias *aliasPtr;			/* Alias record for alias to delete. */
    Target *targetPtr;			/* Record for target of this alias. */
    int i;				/* Loop counter. */

    aliasPtr = (Alias *) clientData;
    
    targetPtr = (Target *) Tcl_GetHashValue(aliasPtr->targetEntry);
    ckfree((char *) targetPtr);
    Tcl_DeleteHashEntry(aliasPtr->targetEntry);

    ckfree((char *) aliasPtr->targetName);
    ckfree((char *) aliasPtr->aliasName);
    for (i = 0; i < aliasPtr->argc; i++) {
        ckfree((char *) aliasPtr->argv[i]);
    }
    if (aliasPtr->argv != (char **) NULL) {
        ckfree((char *) aliasPtr->argv);
    }

    Tcl_DeleteHashEntry(aliasPtr->aliasEntry);

    ckfree((char *) aliasPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * MasterRecordDeleteProc -
 *
 *	Is invoked when an interpreter (which is using the "interp" facility)
 *	is deleted, and it cleans up the storage associated with the
 *	"tclMasterRecord" assoc-data entry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up storage.
 *
 *----------------------------------------------------------------------
 */

static void
MasterRecordDeleteProc(clientData, interp)
    ClientData	clientData;		/* Master record for deleted interp. */
    Tcl_Interp *interp;			/* Interpreter being deleted. */
{
    Target *targetPtr;			/* Loop variable. */
    Tcl_HashEntry *hPtr;		/* Search element. */
    Tcl_HashSearch hSearch;		/* Search record (internal). */
    Slave *slavePtr;			/* Loop variable. */
    char *cmdName;			/* Name of command to delete. */
    Master *masterPtr;			/* Interim storage. */

    masterPtr = (Master *) clientData;
    for (hPtr = Tcl_FirstHashEntry(&(masterPtr->slaveTable), &hSearch);
         hPtr != NULL;
         hPtr = Tcl_NextHashEntry(&hSearch)) {
        slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
        cmdName = Tcl_GetCommandName(interp, slavePtr->interpCmd);
        (void) Tcl_DeleteCommand(interp, cmdName);
    }
    Tcl_DeleteHashTable(&(masterPtr->slaveTable));

    for (hPtr = Tcl_FirstHashEntry(&(masterPtr->targetTable), &hSearch);
         hPtr != NULL;
         hPtr = Tcl_FirstHashEntry(&(masterPtr->targetTable), &hSearch)) {
        targetPtr = (Target *) Tcl_GetHashValue(hPtr);
        cmdName = Tcl_GetCommandName(targetPtr->slaveInterp,
            targetPtr->slaveCmd);
        (void) Tcl_DeleteCommand(targetPtr->slaveInterp, cmdName);
    }
    Tcl_DeleteHashTable(&(masterPtr->targetTable));

    ckfree((char *) masterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveRecordDeleteProc --
 *
 *	Is invoked when an interpreter (which is using the interp facility)
 *	is deleted, and it cleans up the storage associated with the
 *	tclSlaveRecord assoc-data entry.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Cleans up storage.
 *
 *----------------------------------------------------------------------
 */

static void
SlaveRecordDeleteProc(clientData, interp)
    ClientData	clientData;		/* Slave record for deleted interp. */
    Tcl_Interp *interp;			/* Interpreter being deleted. */
{
    Slave *slavePtr;			/* Interim storage. */
    Alias *aliasPtr;
    Tcl_HashTable *hTblPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    
    slavePtr = (Slave *) clientData;

    /*
     * In every case that we call SetAssocData on "tclSlaveRecord",
     * slavePtr is not NULL. Otherwise we panic.
     */

    if (slavePtr == NULL) {
	panic("SlaveRecordDeleteProc: NULL slavePtr");
    }

    if (slavePtr->interpCmd != (Tcl_Command) NULL) {
	Command *cmdPtr = (Command *) slavePtr->interpCmd;

	/*
	 * The interpCmd has not been deleted in the master yet,  since
	 * it's callback sets interpCmd to NULL.
	 *
	 * Probably Tcl_DeleteInterp() was called on this interpreter directly,
	 * rather than via "interp delete", or equivalent (deletion of the
	 * command in the master).
	 *
	 * Perform the cleanup done by SlaveObjectDeleteProc() directly,
	 * and turn off the callback now (since we are about to free slavePtr
	 * and this interpreter is going away, while the deletion of commands
	 * in the master may be deferred).
	 */

	Tcl_DeleteHashEntry(slavePtr->slaveEntry);
	cmdPtr->clientData = NULL;
	cmdPtr->deleteProc = NULL;
	cmdPtr->deleteData = NULL;

        /*
         * Get the command name from the master interpreter instead of
         * relying on the stored name; the command may have been renamed.
         */
        
	Tcl_DeleteCommand(slavePtr->masterInterp,
                Tcl_GetCommandName(slavePtr->masterInterp,
                        slavePtr->interpCmd));
    }

    /*
     * If there are any aliases, delete those now. This removes any
     * dependency on the order of deletion between commands and the
     * slave record.
     */

    hTblPtr = (Tcl_HashTable *) &(slavePtr->aliasTable);
    for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
             hPtr != (Tcl_HashEntry *) NULL;
             hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch)) {
        aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);

        /*
         * The call to Tcl_DeleteCommand will release the storage
         * occuppied by the hash entry and the alias record.
         * NOTE that we cannot use the alias name directly because its
         * storage will be deleted in the command deletion callback. Hence
         * we must use the name for the command as stored in the hash table.
         */

        Tcl_DeleteCommand(interp,
                Tcl_GetCommandName(interp, aliasPtr->slaveCmd));
    }
        
    /*
     * Finally dispose of the slave record itself.
     */
    
    ckfree((char *) slavePtr);    
}

/*
 *----------------------------------------------------------------------
 *
 * TclInterpInit --
 *
 *	Initializes the invoking interpreter for using the "interp"
 *	facility. This is called from inside Tcl_Init.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds the "interp" command to an interpreter and initializes several
 *	records in the associated data of the invoking interpreter.
 *
 *----------------------------------------------------------------------
 */

int
TclInterpInit(interp)
    Tcl_Interp *interp;			/* Interpreter to initialize. */
{
    Master *masterPtr;			/* Its Master record. */

    masterPtr = (Master *) ckalloc((unsigned) sizeof(Master));
    masterPtr->isSafe = 0;
    Tcl_InitHashTable(&(masterPtr->slaveTable), TCL_STRING_KEYS);
    Tcl_InitHashTable(&(masterPtr->targetTable), TCL_ONE_WORD_KEYS);

    (void) Tcl_SetAssocData(interp, "tclMasterRecord", MasterRecordDeleteProc,
            (ClientData) masterPtr);
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IsSafe --
 *
 *	Determines whether an interpreter is safe
 *
 * Results:
 *	1 if it is safe, 0 if it is not.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_IsSafe(interp)
    Tcl_Interp *interp;		/* Is this interpreter "safe" ? */
{
    Master *masterPtr;		/* Its master record. */

    if (interp == (Tcl_Interp *) NULL) {
        return 0;
    }
    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("Tcl_IsSafe: could not find master record");
    }
    return masterPtr->isSafe;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeSafe --
 *
 *	Makes an interpreter safe.
 *
 * Results:
 *	TCL_OK if it succeeds, TCL_ERROR else.
 *
 * Side effects:
 *	Removes functionality from an interpreter.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_MakeSafe(interp)
    Tcl_Interp *interp;		/* Make this interpreter "safe". */
{
    if (interp == (Tcl_Interp *) NULL) {
        return TCL_ERROR;
    }
    return MakeSafe(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateSlave --
 *
 *	Creates a slave interpreter. The slavePath argument denotes the
 *	name of the new slave relative to the current interpreter; the
 *	slave is a direct descendant of the one-before-last component of
 *	the path, e.g. it is a descendant of the current interpreter if
 *	the slavePath argument contains only one component. Optionally makes
 *	the slave interpreter safe.
 *
 * Results:
 *	Returns the interpreter structure created, or NULL if an error
 *	occurred.
 *
 * Side effects:
 *	Creates a new interpreter and a new interpreter object command in
 *	the interpreter indicated by the slavePath argument.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_CreateSlave(interp, slavePath, isSafe)
    Tcl_Interp *interp;		/* Interpreter to start search at. */
    char *slavePath;		/* Name of slave to create. */
    int isSafe;			/* Should new slave be "safe" ? */
{
    if ((interp == (Tcl_Interp *) NULL) || (slavePath == (char *) NULL)) {
        return NULL;
    }
    return CreateSlave(interp, slavePath, isSafe);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetSlave --
 *
 *	Finds a slave interpreter by its path name.
 *
 * Results:
 *	Returns a Tcl_Interp * for the named interpreter or NULL if not
 *	found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_GetSlave(interp, slavePath)
    Tcl_Interp *interp;		/* Interpreter to start search from. */
    char *slavePath;		/* Path of slave to find. */
{
    Master *masterPtr;		/* Interim storage for Master record. */

    if ((interp == (Tcl_Interp *) NULL) || (slavePath == (char *) NULL)) {
        return NULL;
    }
    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("Tcl_GetSlave: could not find master record");
    }
    return GetInterp(interp, masterPtr, slavePath, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetMaster --
 *
 *	Finds the master interpreter of a slave interpreter.
 *
 * Results:
 *	Returns a Tcl_Interp * for the master interpreter or NULL if none.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_GetMaster(interp)
    Tcl_Interp *interp;		/* Get the master of this interpreter. */
{
    Slave *slavePtr;		/* Slave record of this interpreter. */

    if (interp == (Tcl_Interp *) NULL) {
        return NULL;
    }
    slavePtr = (Slave *) Tcl_GetAssocData(interp, "tclSlaveRecord", NULL);
    if (slavePtr == (Slave *) NULL) {
        return NULL;
    }
    return slavePtr->masterInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateAlias --
 *
 *	Creates an alias between two interpreters.
 *
 * Results:
 *	TCL_OK if successful, TCL_ERROR if failed. If TCL_ERROR is returned
 *	the result of slaveInterp will contain an error message.
 *
 * Side effects:
 *	Creates a new alias, manipulates the result field of slaveInterp.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CreateAlias(slaveInterp, slaveCmd, targetInterp, targetCmd, argc, argv)
    Tcl_Interp *slaveInterp;		/* Interpreter for source command. */
    char *slaveCmd;			/* Command to install in slave. */
    Tcl_Interp *targetInterp;		/* Interpreter for target command. */
    char *targetCmd;			/* Name of target command. */
    int argc;				/* How many additional arguments? */
    char **argv;			/* These are the additional args. */
{
    Master *masterPtr;			/* Master record for target interp. */

    if ((slaveInterp == (Tcl_Interp *) NULL) ||
            (targetInterp == (Tcl_Interp *) NULL) ||
            (slaveCmd == (char *) NULL) ||
            (targetCmd == (char *) NULL)) {
        return TCL_ERROR;
    }
    masterPtr = (Master *) Tcl_GetAssocData(targetInterp, "tclMasterRecord",
            NULL);
    if (masterPtr == (Master *) NULL) {
        panic("Tcl_CreateAlias: could not find master record");
    }
    return AliasHelper(slaveInterp, slaveInterp, targetInterp, masterPtr,
            slaveCmd, targetCmd, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAlias --
 *
 *	Gets information about an alias.
 *
 * Results:
 *	TCL_OK if successful, TCL_ERROR else. If TCL_ERROR is returned, the
 *	result field of the interpreter given as argument will contain an
 *	error message.
 *
 * Side effects:
 *	Manipulates the result field of the interpreter given as argument.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetAlias(interp, aliasName, targetInterpPtr, targetNamePtr, argcPtr,
        argvPtr)
    Tcl_Interp *interp;			/* Interp to start search from. */
    char *aliasName;			/* Name of alias to find. */
    Tcl_Interp **targetInterpPtr;	/* (Return) target interpreter. */
    char **targetNamePtr;		/* (Return) name of target command. */
    int *argcPtr;			/* (Return) count of addnl args. */
    char ***argvPtr;			/* (Return) additional arguments. */
{
    Slave *slavePtr;			/* Slave record for slave interp. */
    Tcl_HashEntry *hPtr;		/* Search element. */
    Alias *aliasPtr;			/* Storage for alias found. */

    if ((interp == (Tcl_Interp *) NULL) || (aliasName == (char *) NULL)) {
        return TCL_ERROR;
    }
    slavePtr = (Slave *) Tcl_GetAssocData(interp, "tclSlaveRecord", NULL);
    if (slavePtr == (Slave *) NULL) {
        panic("Tcl_GetAlias: could not find slave record");
    }
    hPtr = Tcl_FindHashEntry(&(slavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendResult(interp, "alias \"", aliasName, "\" not found",
                (char *) NULL);
        return TCL_ERROR;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
    if (targetInterpPtr != (Tcl_Interp **) NULL) {
        *targetInterpPtr = aliasPtr->targetInterp;
    }
    if (targetNamePtr != (char **) NULL) {
        *targetNamePtr = aliasPtr->targetName;
    }
    if (argcPtr != (int *) NULL) {
        *argcPtr = aliasPtr->argc;
    }
    if (argvPtr != (char ***) NULL) {
        *argvPtr = aliasPtr->argv;
    }
    return TCL_OK;
}
