/* 
 * tclInterp.c --
 *
 *	This file implements the "interp" command which allows creation
 *	and manipulation of Tcl interpreters from within Tcl scripts.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclInterp.c 1.128 97/11/05 09:35:12
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
    int		objc;		/* Count of additional args to pass. */
    Tcl_Obj	**objv;		/* Actual additional args to pass. */
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
 * This record is used for two purposes: First, slaveTable (a hashtable)
 * maps from names of commands to slave interpreters. This hashtable is
 * used to store information about slave interpreters of this interpreter,
 * to map over all slaves, etc. The second purpose is to store information
 * about all aliases in slaves (or siblings) which direct to target commands
 * in this interpreter (using the targetTable hashtable).
 * 
 * NB: the flags field in the interp structure, used with SAFE_INTERP
 * mask denotes whether the interpreter is safe or not. Safe
 * interpreters have restricted functionality, can only create safe slave
 * interpreters and can only load safe extensions.
 */

typedef struct {
    Tcl_HashTable slaveTable;	/* Hash table for slave interpreters.
                                 * Maps from command names to Slave records. */
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
			    Tcl_Interp *currentInterp, int objc,
		            Tcl_Obj *CONST objv[]));
static void		AliasCmdDeleteProc _ANSI_ARGS_((
			    ClientData clientData));
static int		AliasCreationHelper _ANSI_ARGS_((Tcl_Interp *curInterp,
			    Tcl_Interp *slaveInterp, Tcl_Interp *masterInterp,
			    Master *masterPtr, char *aliasName,
			    char *targetName, int objc,
			    Tcl_Obj *CONST objv[]));
static int		CreateInterpObject _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static Tcl_Interp	*CreateSlave _ANSI_ARGS_((Tcl_Interp *interp,
		            Master *masterPtr, char *slavePath, int safe));
static int		DeleteAlias _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Interp *slaveInterp, char *aliasName));
static int		DescribeAlias _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Interp *slaveInterp, char *aliasName));
static int		DeleteInterpObject _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		DeleteOneInterpObject _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, char *path));
static Tcl_Interp	*GetInterp _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, char *path,
			    Master **masterPtrPtr));
static int		GetTarget _ANSI_ARGS_((Tcl_Interp *interp, char *path,
			    char *aliasName));
static int		InterpAliasHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpAliasesHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpExistsHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpEvalHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpExposeHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpIsSafeHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpHideHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpHiddenHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpInvokeHiddenHelper _ANSI_ARGS_((
    			    Tcl_Interp *interp, Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpMarkTrustedHelper _ANSI_ARGS_((
    			    Tcl_Interp *interp, Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpSlavesHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpShareHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpTargetHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		InterpTransferHelper _ANSI_ARGS_((Tcl_Interp *interp,
			    Master *masterPtr, int objc,
        		    Tcl_Obj *CONST objv[]));
static int		MarkTrusted _ANSI_ARGS_((Tcl_Interp *interp));
static void		MasterRecordDeleteProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));
static int		SlaveAliasHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveAliasesHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveEvalHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveExposeHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveHideHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveHiddenHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveIsSafeHelper _ANSI_ARGS_((
    			    Tcl_Interp *interp, Tcl_Interp *slaveInterp,
                            Slave *slavePtr, int objc, Tcl_Obj *CONST objv[]));
static int		SlaveInvokeHiddenHelper _ANSI_ARGS_((
    			    Tcl_Interp *interp, Tcl_Interp *slaveInterp,
                            Slave *slavePtr, int objc, Tcl_Obj *CONST objv[]));
static int		SlaveMarkTrustedHelper _ANSI_ARGS_((Tcl_Interp *interp,
		            Tcl_Interp *slaveInterp, Slave *slavePtr,
		            int objc, Tcl_Obj *CONST objv[]));
static int		SlaveObjectCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static void		SlaveObjectDeleteProc _ANSI_ARGS_((
			    ClientData clientData));
static void		SlaveRecordDeleteProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp));

/*
 *----------------------------------------------------------------------
 *
 * TclPreventAliasLoop --
 *
 *	When defining an alias or renaming a command, prevent an alias
 *	loop from being formed.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	If TCL_ERROR is returned, the function also stores an error message
 *	in the interpreter's result object.
 *
 * NOTE:
 *	This function is public internal (instead of being static to
 *	this file) because it is also used from TclRenameCommand.
 *
 *----------------------------------------------------------------------
 */

int
TclPreventAliasLoop(interp, cmdInterp, cmd)
    Tcl_Interp *interp;			/* Interp in which to report errors. */
    Tcl_Interp *cmdInterp;		/* Interp in which the command is
                                         * being defined. */
    Tcl_Command cmd;                    /* Tcl command we are attempting
                                         * to define. */
{
    Command *cmdPtr = (Command *) cmd;
    Alias *aliasPtr, *nextAliasPtr;
    Tcl_Command aliasCmd;
    Command *aliasCmdPtr;
    
    /*
     * If we are not creating or renaming an alias, then it is
     * always OK to create or rename the command.
     */
    
    if (cmdPtr->objProc != AliasCmd) {
        return TCL_OK;
    }

    /*
     * OK, we are dealing with an alias, so traverse the chain of aliases.
     * If we encounter the alias we are defining (or renaming to) any in
     * the chain then we have a loop.
     */

    aliasPtr = (Alias *) cmdPtr->objClientData;
    nextAliasPtr = aliasPtr;
    while (1) {

        /*
         * If the target of the next alias in the chain is the same as
         * the source alias, we have a loop.
	 */

	aliasCmd = Tcl_FindCommand(nextAliasPtr->targetInterp,
                nextAliasPtr->targetName,
		Tcl_GetGlobalNamespace(nextAliasPtr->targetInterp),
		/*flags*/ 0);
        if (aliasCmd == (Tcl_Command) NULL) {
            return TCL_OK;
        }
	aliasCmdPtr = (Command *) aliasCmd;
        if (aliasCmdPtr == cmdPtr) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"cannot define or rename alias \"", aliasPtr->aliasName,
		"\": would create a loop", (char *) NULL);
            return TCL_ERROR;
        }

        /*
	 * Otherwise, follow the chain one step further. See if the target
         * command is an alias - if so, follow the loop to its target
         * command. Otherwise we do not have a loop.
	 */

        if (aliasCmdPtr->objProc != AliasCmd) {
            return TCL_OK;
        }
        nextAliasPtr = (Alias *) aliasCmdPtr->objClientData;
    }

    /* NOTREACHED */
}

/*
 *----------------------------------------------------------------------
 *
 * MarkTrusted --
 *
 *	Mark an interpreter as unsafe (i.e. remove the "safe" mark).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Removes the "safe" mark from an interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
MarkTrusted(interp)
    Tcl_Interp *interp;		/* Interpreter to be marked unsafe. */
{
    Interp *iPtr = (Interp *) interp;

    iPtr->flags &= ~SAFE_INTERP;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeSafe --
 *
 *	Makes its argument interpreter contain only functionality that is
 *	defined to be part of Safe Tcl. Unsafe commands are hidden, the
 *	env array is unset, and the standard channels are removed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Hides commands in its argument interpreter, and removes settings
 *	and channels.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_MakeSafe(interp)
    Tcl_Interp *interp;		/* Interpreter to be made safe. */
{
    Tcl_Channel chan;				/* Channel to remove from
                                                 * safe interpreter. */
    Interp *iPtr = (Interp *) interp;

    TclHideUnsafeCommands(interp);
    
    iPtr->flags |= SAFE_INTERP;

    /*
     *  Unsetting variables : (which should not have been set 
     *  in the first place, but...)
     */

    /*
     * No env array in a safe slave.
     */

    Tcl_UnsetVar(interp, "env", TCL_GLOBAL_ONLY);

    /* 
     * Remove unsafe parts of tcl_platform
     */

    Tcl_UnsetVar2(interp, "tcl_platform", "os", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "tcl_platform", "osVersion", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "tcl_platform", "machine", TCL_GLOBAL_ONLY);

    /*
     * Unset path informations variables
     * (the only one remaining is [info nameofexecutable])
     */

    Tcl_UnsetVar(interp, "tcl_library", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar(interp, "tcl_pkgPath", TCL_GLOBAL_ONLY);
    
    /*
     * Remove the standard channels from the interpreter; safe interpreters
     * do not ordinarily have access to stdin, stdout and stderr.
     *
     * NOTE: These channels are not added to the interpreter by the
     * Tcl_CreateInterp call, but may be added later, by another I/O
     * operation. We want to ensure that the interpreter does not have
     * these channels even if it is being made safe after being used for
     * some time..
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

    if (masterPtrPtr != (Master **) NULL) {
        *masterPtrPtr = masterPtr;
    }
    
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
CreateSlave(interp, masterPtr, slavePath, safe)
    Tcl_Interp *interp;			/* Interp. to start search from. */
    Master *masterPtr;			/* Master record. */
    char *slavePath;			/* Path (name) of slave to create. */
    int safe;				/* Should we make it "safe"? */
{
    Tcl_Interp *slaveInterp;		/* Ptr to slave interpreter. */
    Tcl_Interp *masterInterp;		/* Ptr to master interp for slave. */
    Slave *slavePtr;			/* Slave record. */
    Tcl_HashEntry *hPtr;		/* Entry into interp hashtable. */
    int new;				/* Indicates whether new entry. */
    int argc;				/* Count of elements in slavePath. */
    char **argv;			/* Elements in slavePath. */
    char *masterPath;			/* Path to its master. */

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
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "interpreter named \"", masterPath,
                    "\" not found", (char *) NULL);
            ckfree((char *) argv);
            ckfree((char *) masterPath);
            return (Tcl_Interp *) NULL;
        }
        ckfree((char *) masterPath);
        slavePath = argv[argc-1];
        if (!safe) {
            safe = Tcl_IsSafe(masterInterp);
        }
    }
    hPtr = Tcl_CreateHashEntry(&(masterPtr->slaveTable), slavePath, &new);
    if (new == 0) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter named \"", slavePath,
                "\" already exists, cannot create", (char *) NULL);
        ckfree((char *) argv);
        return (Tcl_Interp *) NULL;
    }
    slaveInterp = Tcl_CreateInterp();
    if (slaveInterp == (Tcl_Interp *) NULL) {
        panic("CreateSlave: out of memory while creating a new interpreter");
    }
    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord", NULL);
    slavePtr->masterInterp = masterInterp;
    slavePtr->slaveEntry = hPtr;
    slavePtr->slaveInterp = slaveInterp;
    slavePtr->interpCmd = Tcl_CreateObjCommand(masterInterp, slavePath,
            SlaveObjectCmd, (ClientData) slaveInterp, SlaveObjectDeleteProc);
    Tcl_InitHashTable(&(slavePtr->aliasTable), TCL_STRING_KEYS);
    (void) Tcl_SetAssocData(slaveInterp, "tclSlaveRecord",
            SlaveRecordDeleteProc, (ClientData) slavePtr);
    Tcl_SetHashValue(hPtr, (ClientData) slavePtr);
    Tcl_SetVar(slaveInterp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
    
    /*
     * Inherit the recursion limit.
     */
    ((Interp *)slaveInterp)->maxNestingDepth =
	((Interp *)masterInterp)->maxNestingDepth ;

    if (safe) {
        if (Tcl_MakeSafe(slaveInterp) == TCL_ERROR) {
            goto error;
        }
    } else {
        if (Tcl_Init(slaveInterp) == TCL_ERROR) {
            goto error;
        }
    }

    ckfree((char *) argv);
    return slaveInterp;

error:

    Tcl_AddErrorInfo(interp, Tcl_GetVar2(slaveInterp, "errorInfo", (char *)
            NULL, TCL_GLOBAL_ONLY));
    Tcl_SetVar2(interp, "errorCode", (char *) NULL,
            Tcl_GetVar2(slaveInterp, "errorCode", (char *) NULL,
                    TCL_GLOBAL_ONLY),
            TCL_GLOBAL_ONLY);

    Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
    Tcl_ResetResult(slaveInterp);

    (void) Tcl_DeleteCommand(masterInterp, slavePath);

    ckfree((char *) argv);
    return (Tcl_Interp *) NULL;
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
CreateInterpObject(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Invoking interpreter. */
    Master *masterPtr;			/* Master record for same. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* with alias. */
{
    int safe;				/* Create a safe interpreter? */
    int moreFlags;			/* Expecting more flag args? */
    char *string;			/* Local pointer to object string. */
    char *slavePath;			/* Name of slave. */
    char localSlaveName[200];		/* Local area for creating names. */
    int i;				/* Loop counter. */
    int len;				/* Length of option argument. */
    static int interpCounter = 0;	/* Unique id for created names. */

    moreFlags = 1;
    slavePath = NULL;
    safe = Tcl_IsSafe(interp);
    
    if ((objc < 2) || (objc > 5)) {
        Tcl_WrongNumArgs(interp, 2, objv, "?-safe? ?--? ?path?");
        return TCL_ERROR;
    }
    for (i = 2; i < objc; i++) {
        string = Tcl_GetStringFromObj(objv[i], &len);
        if ((string[0] == '-') && (moreFlags != 0)) {
            if ((string[1] == 's') &&
                (strncmp(string, "-safe", (size_t) len) == 0) &&
                (len > 1)){
                safe = 1;
            } else if ((strncmp(string, "--", (size_t) len) == 0) &&
                       (len > 1)) {
                moreFlags = 0;
            } else {
                Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                        "bad option \"", string, "\": should be -safe",
                        (char *) NULL);
                return TCL_ERROR;
            }
        } else {
            slavePath = string;
        }
    }
    if (slavePath == (char *) NULL) {

        /*
         * Create an anonymous interpreter -- we choose its name and
         * the name of the command. We check that the command name that
         * we use for the interpreter does not collide with an existing
         * command in the master interpreter.
         */
        
        while (1) {
            Tcl_CmdInfo cmdInfo;
            
            sprintf(localSlaveName, "interp%d", interpCounter);
            interpCounter++;
            if (!(Tcl_GetCommandInfo(interp, localSlaveName, &cmdInfo))) {
                break;
            }
        }
        slavePath = localSlaveName;
    }
    if (CreateSlave(interp, masterPtr, slavePath, safe) != NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(slavePath, -1));
        return TCL_OK;
    } else {
        /*
         * CreateSlave already set the result if there was an error,
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
DeleteOneInterpObject(interp, masterPtr, path)
    Tcl_Interp *interp;			/* Interpreter for reporting errors. */
    Master *masterPtr;			/* Interim storage for master record.*/
    char *path;				/* Path of interpreter to delete. */
{
    Slave *slavePtr;			/* Interim storage for slave record. */
    Tcl_Interp *masterInterp;		/* Master of interp. to delete. */
    Tcl_HashEntry *hPtr;		/* Search element. */
    int localArgc;			/* Local copy of count of elements in
                                         * path (name) of interp. to delete. */
    char **localArgv;			/* Local copy of path. */
    char *slaveName;			/* Last component in path. */
    char *masterPath;			/* One-before-last component in path.*/

    if (Tcl_SplitList(interp, path, &localArgc, &localArgv) != TCL_OK) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad interpreter path \"", path, "\"", (char *) NULL);
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
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "interpreter named \"", masterPath, "\" not found",
                    (char *) NULL);
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
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter named \"", path, "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
    if (Tcl_DeleteCommandFromToken(masterInterp, slavePtr->interpCmd) != 0) {
        ckfree((char *) localArgv);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter named \"", path, "\" not found", (char *) NULL);
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
DeleteInterpObject(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Interpreter start search from. */
    Master *masterPtr;			/* Interim storage for master record.*/
    int objc;				/* Number of arguments in vector. */
    Tcl_Obj *CONST objv[];		/* with alias. */
{
    int i;
    int len;
    
    for (i = 2; i < objc; i++) {
        if (DeleteOneInterpObject(interp, masterPtr,
                Tcl_GetStringFromObj(objv[i], &len))
                != TCL_OK) {
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasCreationHelper --
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
AliasCreationHelper(curInterp, slaveInterp, masterInterp, masterPtr,
     aliasName, targetName, objc, objv)
    Tcl_Interp *curInterp;		/* Interp that invoked this proc. */
    Tcl_Interp *slaveInterp;		/* Interp where alias cmd will live
                                         * or from which alias will be
                                         * deleted. */
    Tcl_Interp *masterInterp;		/* Interp where target cmd will be. */
    Master *masterPtr;			/* Master record for target interp. */
    char *aliasName;			/* Name of alias cmd. */
    char *targetName;			/* Name of target cmd. */
    int objc;				/* Additional arguments to store */
    Tcl_Obj *CONST objv[];		/* with alias. */
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
     * Slave record should be always present because it is created when
     * the interpreter is created.
     */
    
    if (slavePtr == (Slave *) NULL) {
        panic("AliasCreationHelper: could not find slave record");
    }

    if ((targetName == (char *) NULL) || (targetName[0] == '\0')) {
        if (objc != 0) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(curInterp),
                    "malformed command: should be",
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

    aliasPtr->objv = NULL;
    aliasPtr->objc = objc;

    if (aliasPtr->objc > 0) {
        aliasPtr->objv =
            (Tcl_Obj **) ckalloc((unsigned) sizeof(Tcl_Obj *) *
                    aliasPtr->objc);
        for (i = 0; i < objc; i++) {
            aliasPtr->objv[i] = objv[i];
            Tcl_IncrRefCount(objv[i]);
        }
    }

    aliasPtr->slaveCmd = Tcl_CreateObjCommand(slaveInterp, aliasName,
            AliasCmd, (ClientData) aliasPtr, AliasCmdDeleteProc);

    if (TclPreventAliasLoop(curInterp, slaveInterp, 
            aliasPtr->slaveCmd) != TCL_OK) {

	/*
         *  Found an alias loop!  The last call to Tcl_CreateObjCommand
         *  made the alias point to itself.  Delete the command and
         *  its alias record.  Be careful to wipe out its client data
         *  first, so the command doesn't try to delete itself.
         */
	
        Command *cmdPtr = (Command*) aliasPtr->slaveCmd;
        cmdPtr->clientData = NULL;
        cmdPtr->deleteProc = NULL;
        cmdPtr->deleteData = NULL;
        Tcl_DeleteCommandFromToken(slaveInterp, aliasPtr->slaveCmd);

        for (i = 0; i < objc; i++) {
            Tcl_DecrRefCount(aliasPtr->objv[i]);
        }
        if (aliasPtr->objv != (Tcl_Obj *CONST *) NULL) {
            ckfree((char *) aliasPtr->objv);
        }
        ckfree(aliasPtr->aliasName);
        ckfree(aliasPtr->targetName);
        ckfree((char *) aliasPtr);

        /*
         * The result was already set by TclPreventAliasLoop.
         */

        return TCL_ERROR;
    }
    
    /*
     * Make an entry in the alias table. If it already exists delete
     * the alias command. Then retry.
     */

    do {
        hPtr = Tcl_CreateHashEntry(&(slavePtr->aliasTable), aliasName, &new);
        if (!new) {
            tmpAliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
            (void) Tcl_DeleteCommandFromToken(slaveInterp,
	            tmpAliasPtr->slaveCmd);

            /*
             * The hash entry should be deleted by the Tcl_DeleteCommand
             * above, in its command deletion callback (most likely this
             * will be AliasCmdDeleteProc, which does the deletion).
             */
        }
    } while (new == 0);
    aliasPtr->aliasEntry = hPtr;
    Tcl_SetHashValue(hPtr, (ClientData) aliasPtr);
    
    /*
     * Create the new command. We must do it after deleting any old command,
     * because the alias may be pointing at a renamed alias, as in:
     *
     * interp alias {} foo {} bar		# Create an alias "foo"
     * rename foo zop				# Now rename the alias
     * interp alias {} foo {} zop		# Now recreate "foo"...
     */

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

    /*
     * Make sure we clear out the object result when setting the string
     * result.
     */

    Tcl_SetObjResult(curInterp, Tcl_NewStringObj(aliasPtr->aliasName, -1));
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpAliasesHelper --
 *
 *	Computes a list of aliases defined in an interpreter.
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
InterpAliasesHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Invoking interpreter. */
    Master *masterPtr;			/* Master record for current interp. */
    int objc;				/* How many arguments? */
    Tcl_Obj *CONST objv[];		/* Actual arguments. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    Slave *slavePtr;			/* Record for slave interp. */
    Tcl_HashEntry *hPtr;		/* Search variable. */
    Tcl_HashSearch hSearch;		/* Iteration variable. */
    int len;				/* Dummy length variable. */
    Tcl_Obj *listObjPtr, *elemObjPtr;	/* Local object pointers. */
    
    if ((objc != 2) && (objc != 3)) {
        Tcl_WrongNumArgs(interp, 2, objv, "?path?");
        return TCL_ERROR;
    }
    if (objc == 3) {
        slaveInterp = GetInterp(interp, masterPtr,
                Tcl_GetStringFromObj(objv[2], &len), NULL);
        if (slaveInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                    "\" not found", (char *) NULL);
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

    /*
     * Build a list to return the aliases:
     */
            
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Tcl_FirstHashEntry(&(slavePtr->aliasTable), &hSearch);
         hPtr != NULL;
         hPtr = Tcl_NextHashEntry(&hSearch)) {

        elemObjPtr = Tcl_NewStringObj(
            Tcl_GetHashKey(&(slavePtr->aliasTable), hPtr), -1);
        Tcl_ListObjAppendElement(interp, listObjPtr, elemObjPtr);
    }
    Tcl_SetObjResult(interp, listObjPtr);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpAliasHelper -
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
InterpAliasHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for current interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp,		/* Interpreters used when */
        *masterInterp;			/* creating an alias btn siblings. */
    Master *masterMasterPtr;		/* Master record for master interp. */
    int len;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "slavePath slaveCmd masterPath masterCmd ?args ..?");
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), NULL);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "could not find interpreter \"",
                Tcl_GetStringFromObj(objv[2], &len), "\"",
                (char *) NULL);
        return TCL_ERROR;
    }
    if (objc == 4) {
        return DescribeAlias(interp, slaveInterp,
                Tcl_GetStringFromObj(objv[3], &len));
    }
    if (objc == 5 && strcmp(Tcl_GetStringFromObj(objv[4], &len), "") == 0) {
        return DeleteAlias(interp, slaveInterp,
                Tcl_GetStringFromObj(objv[3], &len));
    }
    if (objc < 6) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "slavePath slaveCmd masterPath masterCmd ?args ..?");
        return TCL_ERROR;
    }
    masterInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[4], &len), &masterMasterPtr);
    if (masterInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "could not find interpreter \"",
                Tcl_GetStringFromObj(objv[4], &len), "\"", (char *) NULL);
        return TCL_ERROR;
    }
    return AliasCreationHelper(interp, slaveInterp, masterInterp,
            masterMasterPtr, Tcl_GetStringFromObj(objv[3], &len),
            Tcl_GetStringFromObj(objv[5], &len),
            objc-6, objv+6);
}

/*
 *----------------------------------------------------------------------
 *
 * InterpExistsHelper --
 *
 *	Computes whether a named interpreter exists or not.
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
InterpExistsHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for current interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Obj *objPtr;
    int len;

    if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?path?");
        return TCL_ERROR;
    }
    if (objc == 3) {
        if (GetInterp(interp, masterPtr,
                Tcl_GetStringFromObj(objv[2], &len), NULL) ==
                (Tcl_Interp *) NULL) {
            objPtr = Tcl_NewIntObj(0);
        } else {
            objPtr = Tcl_NewIntObj(1);
        }
    } else {
        objPtr = Tcl_NewIntObj(1);
    }
    Tcl_SetObjResult(interp, objPtr);
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpEvalHelper --
 *
 *	Helper function to handle all the details of evaluating a
 *	command in another interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the command itself does.
 *
 *----------------------------------------------------------------------
 */

static int
InterpEvalHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for current interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    Interp *iPtr;			/* Internal data type for slave. */
    int len;				/* Dummy length variable. */
    int result;
    Tcl_Obj *namePtr, *objPtr;		/* Local object pointer. */
    char *string;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "path arg ?arg ...?");
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), NULL);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter named \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    objPtr = Tcl_ConcatObj(objc-3, objv+3);
    Tcl_IncrRefCount(objPtr);
    
    Tcl_Preserve((ClientData) slaveInterp);
    result = Tcl_EvalObj(slaveInterp, objPtr);

    Tcl_DecrRefCount(objPtr);

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
             * the target interpreter back to our interpreter.
             */

            iPtr = (Interp *) slaveInterp;
            if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
                Tcl_AddErrorInfo(slaveInterp, "");
            }
            iPtr->flags &= (~(ERR_ALREADY_LOGGED));
            
            Tcl_ResetResult(interp);
            namePtr = Tcl_NewStringObj("errorInfo", -1);
            objPtr = Tcl_ObjGetVar2(slaveInterp, namePtr,
                    (Tcl_Obj *) NULL, TCL_GLOBAL_ONLY);
            string = Tcl_GetStringFromObj(objPtr, &len);
            Tcl_AddObjErrorInfo(interp, string, len);
            Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                    Tcl_GetVar2(slaveInterp, "errorCode", (char *)
                            NULL, TCL_GLOBAL_ONLY),
                    TCL_GLOBAL_ONLY);
            Tcl_DecrRefCount(namePtr);
        }

	/*
         * Move the result object from one interpreter to the
         * other.
         */
                
        Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
        Tcl_ResetResult(slaveInterp);

    }
    Tcl_Release((ClientData) slaveInterp);
    return result;        
}

/*
 *----------------------------------------------------------------------
 *
 * InterpExposeHelper --
 *
 *	Helper function to handle the details of exposing a command in
 *	another interpreter.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Exposes a command. From now on the command can be called by scripts
 *	in the interpreter in which it was exposed.
 *
 *----------------------------------------------------------------------
 */

static int
InterpExposeHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for current interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    int len;				/* Dummy length variable. */

    if ((objc != 4) && (objc != 5)) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "path hiddenCmdName ?cmdName?");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "permission denied: safe interpreter cannot expose commands",
                (char *) NULL);
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), &masterPtr);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_ExposeCommand(slaveInterp,
            Tcl_GetStringFromObj(objv[3], &len),
                (objc == 5 ?
                        Tcl_GetStringFromObj(objv[4], &len) :
                        Tcl_GetStringFromObj(objv[3], &len)))
            == TCL_ERROR) {
        if (interp != slaveInterp) {
            Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
            Tcl_ResetResult(slaveInterp);
        }
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpHideHelper --
 *
 *	Helper function that handles the details of hiding a command in
 *	another interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Hides a command. From now on the command cannot be called by
 *	scripts in that interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
InterpHideHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    int len;				/* Dummy length variable. */

    if ((objc != 4) && (objc != 5)) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "path cmdName ?hiddenCmdName?");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "permission denied: safe interpreter cannot hide commands",
                (char *) NULL);
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), &masterPtr);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_HideCommand(slaveInterp, Tcl_GetStringFromObj(objv[3], &len),
            (objc == 5 ?
                    Tcl_GetStringFromObj(objv[4], &len) :
                    Tcl_GetStringFromObj(objv[3], &len)))
            == TCL_ERROR) {
        if (interp != slaveInterp) {
            Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
            Tcl_ResetResult(slaveInterp);
        }
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpHiddenHelper --
 *
 *	Computes the list of hidden commands in a named interpreter.
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
InterpHiddenHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    int len;
    Tcl_HashTable *hTblPtr;		/* Hidden command table. */
    Tcl_HashEntry *hPtr;		/* Search variable. */
    Tcl_HashSearch hSearch;		/* Iteration variable. */
    Tcl_Obj *listObjPtr;		/* Local object pointer. */

    if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?path?");
        return TCL_ERROR;
    }
    if (objc == 3) {
        slaveInterp = GetInterp(interp, masterPtr,
                Tcl_GetStringFromObj(objv[2], &len),
                &masterPtr);
        if (slaveInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                    "\" not found", (char *) NULL);
            return TCL_ERROR;
        }
    } else {
        slaveInterp = interp;
    }
            
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(slaveInterp,
            "tclHiddenCmds", NULL);
    if (hTblPtr != (Tcl_HashTable *) NULL) {
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
             hPtr != (Tcl_HashEntry *) NULL;
             hPtr = Tcl_NextHashEntry(&hSearch)) {

            Tcl_ListObjAppendElement(interp, listObjPtr,
                    Tcl_NewStringObj(Tcl_GetHashKey(hTblPtr, hPtr), -1));
        }
    }
    Tcl_SetObjResult(interp, listObjPtr);
            
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpInvokeHiddenHelper --
 *
 *	Helper routine to handle the details of invoking a hidden
 *	command in another interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the hidden command does.
 *
 *----------------------------------------------------------------------
 */

static int
InterpInvokeHiddenHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    int doGlobal = 0;
    int len;
    int result;
    Tcl_Obj *namePtr, *objPtr;
    Tcl_Interp *slaveInterp;
    Interp *iPtr;
    char *string;
            
    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "path ?-global? cmd ?arg ..?");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "not allowed to invoke hidden commands from safe interpreter",
                (char *) NULL);
        return TCL_ERROR;
    }
    if (strcmp(Tcl_GetStringFromObj(objv[3], &len), "-global") == 0) {
        doGlobal = 1;
        if (objc < 5) {
            Tcl_WrongNumArgs(interp, 2, objv,
                    "path ?-global? cmd ?arg ..?");
            return TCL_ERROR;
        }
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), &masterPtr);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) slaveInterp);
    if (doGlobal) {
        result = TclObjInvokeGlobal(slaveInterp, objc-4, objv+4,
                TCL_INVOKE_HIDDEN);
    } else {
        result = TclObjInvoke(slaveInterp, objc-3, objv+3, TCL_INVOKE_HIDDEN);
    }

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
             * the target interpreter back to our interpreter.
             */

            iPtr = (Interp *) slaveInterp;
            if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
                Tcl_AddErrorInfo(slaveInterp, "");
            }
            iPtr->flags &= (~(ERR_ALREADY_LOGGED));

            Tcl_ResetResult(interp);
            namePtr = Tcl_NewStringObj("errorInfo", -1);
            objPtr = Tcl_ObjGetVar2(slaveInterp, namePtr,
                    (Tcl_Obj *) NULL, TCL_GLOBAL_ONLY);
            Tcl_DecrRefCount(namePtr);
            string = Tcl_GetStringFromObj(objPtr, &len);
            Tcl_AddObjErrorInfo(interp, string, len);
            Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                    Tcl_GetVar2(slaveInterp, "errorCode", (char *)
                            NULL, TCL_GLOBAL_ONLY),
                    TCL_GLOBAL_ONLY);
        }

	/*
         * Move the result object from the slave to the master.
         */
                
        Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
        Tcl_ResetResult(slaveInterp);
    }
    Tcl_Release((ClientData) slaveInterp);
    return result;        
}

/*
 *----------------------------------------------------------------------
 *
 * InterpMarkTrustedHelper --
 *
 *	Helper function to handle the details of marking another
 *	interpreter as trusted (unsafe).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Henceforth the hard-wired checks for safety will not prevent
 *	this interpreter from performing certain operations.
 *
 *----------------------------------------------------------------------
 */

static int
InterpMarkTrustedHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    int len;				/* Dummy length variable. */

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "path");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "\"", Tcl_GetStringFromObj(objv[0], &len),
                " marktrusted\" can only",
                " be invoked from a trusted interpreter",
                (char *) NULL);
        return TCL_ERROR;
    }

    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), &masterPtr);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    return MarkTrusted(slaveInterp);
}

/*
 *----------------------------------------------------------------------
 *
 * InterpIsSafeHelper --
 *
 *	Computes whether a named interpreter is safe.
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
InterpIsSafeHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    int len;				/* Dummy length variable. */
    Tcl_Obj *objPtr;			/* Local object pointer. */

    if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?path?");
        return TCL_ERROR;
    }
    if (objc == 3) {
        slaveInterp = GetInterp(interp, masterPtr,
                Tcl_GetStringFromObj(objv[2], &len), &masterPtr);
        if (slaveInterp == (Tcl_Interp *) NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "interpreter \"",
                    Tcl_GetStringFromObj(objv[2], &len), "\" not found",
                    (char *) NULL);
            return TCL_ERROR;
        }
	objPtr = Tcl_NewIntObj(Tcl_IsSafe(slaveInterp));
    } else {
	objPtr = Tcl_NewIntObj(Tcl_IsSafe(interp));
    }
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpSlavesHelper --
 *
 *	Computes a list of slave interpreters of a named interpreter.
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
InterpSlavesHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    int len;
    Tcl_HashEntry *hPtr;		/* Search variable. */
    Tcl_HashSearch hSearch;		/* Iteration variable. */
    Tcl_Obj *listObjPtr;		/* Local object pointers. */

    if ((objc != 2) && (objc != 3)) {
        Tcl_WrongNumArgs(interp, 2, objv, "?path?");
        return TCL_ERROR;
    }
    if (objc == 3) {
        if (GetInterp(interp, masterPtr,
                Tcl_GetStringFromObj(objv[2], &len), &masterPtr) ==
                (Tcl_Interp *) NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                    "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                    "\" not found", (char *) NULL);
            return TCL_ERROR;
        }
    }

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Tcl_FirstHashEntry(&(masterPtr->slaveTable), &hSearch);
         hPtr != NULL;
         hPtr = Tcl_NextHashEntry(&hSearch)) {

        Tcl_ListObjAppendElement(interp, listObjPtr,
                Tcl_NewStringObj(
                    Tcl_GetHashKey(&(masterPtr->slaveTable), hPtr), -1));
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpShareHelper --
 *
 *	Helper function to handle the details of sharing a channel between
 *	interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call the named channel will be shared between the
 *	interpreters named in the arguments.
 *
 *----------------------------------------------------------------------
 */

static int
InterpShareHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    Tcl_Interp *masterInterp;		/* Its master. */
    int len;
    Tcl_Channel chan;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "srcPath channelId destPath");
        return TCL_ERROR;
    }
    masterInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), NULL);
    if (masterInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[4], &len), NULL);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[4], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    chan = Tcl_GetChannel(masterInterp, Tcl_GetStringFromObj(objv[3], &len),
            NULL);
    if (chan == (Tcl_Channel) NULL) {
        if (interp != masterInterp) {
            Tcl_SetObjResult(interp, Tcl_GetObjResult(masterInterp));
            Tcl_ResetResult(masterInterp);
        }
        return TCL_ERROR;
    }
    Tcl_RegisterChannel(slaveInterp, chan);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InterpTargetHelper --
 *
 *	Helper function to compute the target of an alias.
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
InterpTargetHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    int len;
    
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "path alias");
        return TCL_ERROR;
    }
    return GetTarget(interp,
            Tcl_GetStringFromObj(objv[2], &len),
            Tcl_GetStringFromObj(objv[3], &len));
}

/*
 *----------------------------------------------------------------------
 *
 * InterpTransferHelper --
 *
 *	Helper function to handle the details of transferring ownership
 *	of a channel between interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After the call, the named channel will be registered in the target
 *	interpreter and no longer available for use in the source interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
InterpTransferHelper(interp, masterPtr, objc, objv)
    Tcl_Interp *interp;			/* Current interpreter. */
    Master *masterPtr;			/* Master record for interp. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Interp *slaveInterp;		/* A slave. */
    Tcl_Interp *masterInterp;		/* Its master. */
    int len;
    Tcl_Channel chan;
            
    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "srcPath channelId destPath");
        return TCL_ERROR;
    }
    masterInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[2], &len), NULL);
    if (masterInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[2], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    slaveInterp = GetInterp(interp, masterPtr,
            Tcl_GetStringFromObj(objv[4], &len), NULL);
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter \"", Tcl_GetStringFromObj(objv[4], &len),
                "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    chan = Tcl_GetChannel(masterInterp,
            Tcl_GetStringFromObj(objv[3], &len), NULL);
    if (chan == (Tcl_Channel) NULL) {
        if (interp != masterInterp) {

            /*
             * After fixing objresult, this code will change to:
             * Tcl_SetObjResult(interp, Tcl_GetObjResult(masterInterp));
             */
            
            Tcl_SetObjResult(interp, Tcl_GetObjResult(masterInterp));
            Tcl_ResetResult(masterInterp);
        }
        return TCL_ERROR;
    }
    Tcl_RegisterChannel(slaveInterp, chan);
    if (Tcl_UnregisterChannel(masterInterp, chan) != TCL_OK) {
        if (interp != masterInterp) {
            Tcl_SetObjResult(interp, Tcl_GetObjResult(masterInterp));
            Tcl_ResetResult(masterInterp);
        }
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DescribeAlias --
 *
 *	Sets the interpreter's result object to a Tcl list describing
 *	the given alias in the given interpreter: its target command
 *	and the additional arguments to prepend to any invocation
 *	of the alias.
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
    Tcl_Interp *interp;			/* Interpreter for result & errors. */
    Tcl_Interp *slaveInterp;		/* Interpreter defining alias. */
    char *aliasName;			/* Name of alias to describe. */
{
    Slave *slavePtr;			/* Slave interp slave record. */
    Tcl_HashEntry *hPtr;		/* Search variable. */
    Alias *aliasPtr;			/* Structure describing alias. */
    int i;				/* Loop variable. */
    Tcl_Obj *listObjPtr;		/* Local object pointer. */

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",
            NULL);

    /*
     * The slave record should always be present because it is created
     * by Tcl_CreateInterp.
     */
    
    if (slavePtr == (Slave *) NULL) {
        panic("DescribeAlias: could not find slave record");
    }
    hPtr = Tcl_FindHashEntry(&(slavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        return TCL_OK;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    Tcl_ListObjAppendElement(interp, listObjPtr,
            Tcl_NewStringObj(aliasPtr->targetName, -1));
    for (i = 0; i < aliasPtr->objc; i++) {
        Tcl_ListObjAppendElement(interp, listObjPtr, aliasPtr->objv[i]);
    }
    Tcl_SetObjResult(interp, listObjPtr);
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
    Alias *aliasPtr;		/* Points at alias structure to delete. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    char *tmpPtr, *namePtr;	/* Local pointers to name of command to
                                 * be deleted. */

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",
            NULL);
    if (slavePtr == (Slave *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "alias \"", aliasName, "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    
    /*
     * Get the alias from the alias table, then delete the command. The
     * deleteProc on the alias command will take care of removing the entry
     * from the alias table.
     */

    hPtr = Tcl_FindHashEntry(&(slavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "alias \"", aliasName, "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);

    /*
     * Get a copy of the real name of the command -- it might have
     * been renamed, and we want to delete the renamed command, not
     * the current command (if any) by the name of the original alias.
     * We need the local copy because the name may get smashed when the
     * command to delete is exposed, if it was hidden.
     */

    tmpPtr = Tcl_GetCommandName(slaveInterp, aliasPtr->slaveCmd);
    namePtr = (char *) ckalloc((unsigned) strlen(tmpPtr) + 1);
    strcpy(namePtr, tmpPtr);

    /*
     * NOTE: The deleteProc for this command will delete the
     * alias from the hash table. The deleteProc will also
     * delete the target information from the master interpreter
     * target table.
     */

    if (Tcl_DeleteCommand(slaveInterp, namePtr) != 0) {
        if (Tcl_ExposeCommand(slaveInterp, namePtr, namePtr) != TCL_OK) {
            panic("DeleteAlias: did not find alias to be deleted");
        }
        if (Tcl_DeleteCommand(slaveInterp, namePtr) != 0) {
            panic("DeleteAlias: did not find alias to be deleted");
        }
    }
    ckfree(namePtr);

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
         * The result of askingInterp was set by recursive call.
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
        Tcl_AppendStringsToObj(Tcl_GetObjResult(askingInterp),
                "could not find interpreter \"", path, "\"", (char *) NULL);
        return TCL_ERROR;
    }
    slaveSlavePtr = (Slave *) Tcl_GetAssocData(slaveInterp, "tclSlaveRecord",
            NULL);
    if (slaveSlavePtr == (Slave *) NULL) {
        panic("GetTarget: could not find slave record");
    }
    hPtr = Tcl_FindHashEntry(&(slaveSlavePtr->aliasTable), aliasName);
    if (hPtr == (Tcl_HashEntry *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(askingInterp),
                "alias \"", aliasName, "\" in path \"", path, "\" not found",
                (char *) NULL);
        return TCL_ERROR;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
    if (aliasPtr == (Alias *) NULL) {
        panic("GetTarget: could not find alias record");
    }
    
    if (Tcl_GetInterpPath(askingInterp, aliasPtr->targetInterp) == TCL_ERROR) {
        Tcl_ResetResult(askingInterp);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(askingInterp),
                "target interpreter for alias \"",
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
Tcl_InterpObjCmd(clientData, interp, objc, objv)
    ClientData clientData;		/* Unused. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Master *masterPtr;			/* Master record for current interp. */
    int result;				/* Local result variable. */

    /*
     * These are all the different subcommands for this command:
     */
    
    static char *subCmds[] = {
        "alias", "aliases", "create", "delete", "eval", "exists",
	"expose", "hide", "hidden", "issafe", "invokehidden",
        "marktrusted", "slaves", "share", "target", "transfer",
        (char *) NULL};
    enum ISubCmdIdx {
        IAliasIdx, IAliasesIdx, ICreateIdx, IDeleteIdx, IEvalIdx,
	IExistsIdx, IExposeIdx, IHideIdx, IHiddenIdx, IIsSafeIdx,
	IInvokeHiddenIdx, IMarkTrustedIdx, ISlavesIdx, IShareIdx,
        ITargetIdx, ITransferIdx
    } index;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "cmd ?arg ...?");
        return TCL_ERROR;
    }

    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("Tcl_InterpCmd: could not find master record");
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], subCmds, "option",
            0, (int *) &index);
    if (result != TCL_OK) {
        return result;
    }
    
    switch (index) {
        case IAliasIdx:
            return InterpAliasHelper(interp, masterPtr, objc, objv);
        case IAliasesIdx:
            return InterpAliasesHelper(interp, masterPtr, objc, objv);
        case ICreateIdx:
            return CreateInterpObject(interp, masterPtr, objc, objv);
        case IDeleteIdx:
            return DeleteInterpObject(interp, masterPtr, objc, objv);
        case IEvalIdx:
            return InterpEvalHelper(interp, masterPtr, objc, objv);
        case IExistsIdx:
            return InterpExistsHelper(interp, masterPtr, objc, objv);
        case IExposeIdx:
            return InterpExposeHelper(interp, masterPtr, objc, objv);
        case IHideIdx:
            return InterpHideHelper(interp, masterPtr, objc, objv);
        case IHiddenIdx:
            return InterpHiddenHelper(interp, masterPtr, objc, objv);
        case IIsSafeIdx:
            return InterpIsSafeHelper(interp, masterPtr, objc, objv);
        case IInvokeHiddenIdx:
            return InterpInvokeHiddenHelper(interp, masterPtr, objc, objv);
        case IMarkTrustedIdx:
            return InterpMarkTrustedHelper(interp, masterPtr, objc, objv);
        case ISlavesIdx:
            return InterpSlavesHelper(interp, masterPtr, objc, objv);
        case IShareIdx:
            return InterpShareHelper(interp, masterPtr, objc, objv);
        case ITargetIdx:
            return InterpTargetHelper(interp, masterPtr, objc, objv);
        case ITransferIdx:
            return InterpTransferHelper(interp, masterPtr, objc, objv);
    }

    return TCL_ERROR;    
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveAliasHelper --
 *
 *	Helper function to construct or query an alias for a slave
 *	interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Potentially creates a new alias.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveAliasHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    Master *masterPtr;
    int len;

    switch (objc-2) {
        case 0:
            Tcl_WrongNumArgs(interp, 2, objv,
                    "aliasName ?targetName? ?args..?");
            return TCL_ERROR;

        case 1:

            /*
             * Return the name of the command in the current
             * interpreter for which the argument is an alias in the
             * slave interpreter, and the list of saved arguments
             */

            return DescribeAlias(interp, slaveInterp,
                    Tcl_GetStringFromObj(objv[2], &len));

        default:
            masterPtr = (Master *) Tcl_GetAssocData(interp,
                    "tclMasterRecord", NULL);
            if (masterPtr == (Master *) NULL) {
                panic("SlaveObjectCmd: could not find master record");
            }
            return AliasCreationHelper(interp, slaveInterp, interp,
                    masterPtr,
                    Tcl_GetStringFromObj(objv[2], &len),
                    Tcl_GetStringFromObj(objv[3], &len),
                    objc-4, objv+4);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveAliasesHelper --
 *
 *	Computes a list of aliases defined in a slave interpreter.
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
SlaveAliasesHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    Tcl_HashEntry *hPtr;		/* For local searches. */
    Tcl_HashSearch hSearch;		/* For local searches. */
    Tcl_Obj *listObjPtr;		/* Local object pointer. */
    Alias *aliasPtr;			/* Alias information. */

    /*
     * Return the names of all the aliases created in the
     * slave interpreter.
     */

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (hPtr = Tcl_FirstHashEntry(&(slavePtr->aliasTable),
            &hSearch);
         hPtr != (Tcl_HashEntry *) NULL;
         hPtr = Tcl_NextHashEntry(&hSearch)) {
        aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
        Tcl_ListObjAppendElement(interp, listObjPtr,
                Tcl_NewStringObj(aliasPtr->aliasName, -1));
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveEvalHelper --
 *
 *	Helper function to evaluate a command in a slave interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the command does.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveEvalHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    Interp *iPtr;			/* Internal data type for slave. */
    Tcl_Obj *objPtr;			/* Local object pointer. */
    Tcl_Obj *namePtr;			/* Local object pointer. */
    int len;
    char *string;
    int result;
    
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "arg ?arg ...?");
        return TCL_ERROR;
    }

    objPtr = Tcl_ConcatObj(objc-2, objv+2);
    Tcl_IncrRefCount(objPtr);
    
    Tcl_Preserve((ClientData) slaveInterp);
    result = Tcl_EvalObj(slaveInterp, objPtr);

    Tcl_DecrRefCount(objPtr);

    /*
     * Make the result and any error information accessible. We have
     * to be careful because the slave interpreter and the current
     * interpreter can be the same - do not destroy the result.. This
     * can happen if an interpreter contains an alias which is directed
     * at a target command in the same interpreter.
     */

    if (interp != slaveInterp) {
        if (result == TCL_ERROR) {

            /*
             * An error occurred, so transfer error information from the
             * destination interpreter back to our interpreter. 
             */

            iPtr = (Interp *) slaveInterp;
            if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
                Tcl_AddErrorInfo(slaveInterp, "");
            }
            iPtr->flags &= (~(ERR_ALREADY_LOGGED));

            Tcl_ResetResult(interp);
            namePtr = Tcl_NewStringObj("errorInfo", -1);
            objPtr = Tcl_ObjGetVar2(slaveInterp, namePtr,
                    (Tcl_Obj *) NULL, TCL_GLOBAL_ONLY);
            string = Tcl_GetStringFromObj(objPtr, &len);
            Tcl_AddObjErrorInfo(interp, string, len);
            Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                    Tcl_GetVar2(slaveInterp, "errorCode", (char *)
                            NULL, TCL_GLOBAL_ONLY),
                    TCL_GLOBAL_ONLY);
            Tcl_DecrRefCount(namePtr);
        }

	/*
         * Move the result object from one interpreter to the
         * other.
         */
                
        Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
        Tcl_ResetResult(slaveInterp);
    }
    Tcl_Release((ClientData) slaveInterp);
    return result;        
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveExposeHelper --
 *
 *	Helper function to expose a command in a slave interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call scripts in the slave will be able to invoke
 *	the newly exposed command.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveExposeHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    int len;
    
    if ((objc != 3) && (objc != 4)) {
        Tcl_WrongNumArgs(interp, 2, objv, "hiddenCmdName ?cmdName?");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "permission denied: safe interpreter cannot expose commands",
                (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_ExposeCommand(slaveInterp, Tcl_GetStringFromObj(objv[2], &len),
            (objc == 4 ?
                    Tcl_GetStringFromObj(objv[3], &len) :
                    Tcl_GetStringFromObj(objv[2], &len)))
            == TCL_ERROR) {
        Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
        Tcl_ResetResult(slaveInterp);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveHideHelper --
 *
 *	Helper function to hide a command in a slave interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call scripts in the slave will no longer be able
 *	to invoke the named command.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveHideHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    int len;

    if ((objc != 3) && (objc != 4)) {
        Tcl_WrongNumArgs(interp, 2, objv, "cmdName ?hiddenCmdName?");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "permission denied: safe interpreter cannot hide commands",
                (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_HideCommand(slaveInterp, Tcl_GetStringFromObj(objv[2], &len),
            (objc == 4 ?
                    Tcl_GetStringFromObj(objv[3], &len) :
                    Tcl_GetStringFromObj(objv[2], &len)))
            == TCL_ERROR) {
        Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
        Tcl_ResetResult(slaveInterp);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveHiddenHelper --
 *
 *	Helper function to compute list of hidden commands in a slave
 *	interpreter.
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
SlaveHiddenHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    Tcl_Obj *listObjPtr;		/* Local object pointer. */
    Tcl_HashTable *hTblPtr;		/* For local searches. */
    Tcl_HashEntry *hPtr;		/* For local searches. */
    Tcl_HashSearch hSearch;		/* For local searches. */
    
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(slaveInterp,
            "tclHiddenCmds", NULL);
    if (hTblPtr != (Tcl_HashTable *) NULL) {
        for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
             hPtr != (Tcl_HashEntry *) NULL;
             hPtr = Tcl_NextHashEntry(&hSearch)) {
            Tcl_ListObjAppendElement(interp, listObjPtr,
                    Tcl_NewStringObj(Tcl_GetHashKey(hTblPtr, hPtr), -1));
        }
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveIsSafeHelper --
 *
 *	Helper function to compute whether a slave interpreter is safe.
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
SlaveIsSafeHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    Tcl_Obj *resultPtr;			/* Local object pointer. */

    if (objc > 2) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }
    resultPtr = Tcl_NewIntObj(Tcl_IsSafe(slaveInterp));

    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveInvokeHiddenHelper --
 *
 *	Helper function to invoke a hidden command in a slave interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the hidden command does.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveInvokeHiddenHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    Interp *iPtr;
    Master *masterPtr;
    int doGlobal = 0;
    int result;
    int len;
    char *string;
    Tcl_Obj *namePtr, *objPtr;
            
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv,
                "?-global? cmd ?arg ..?");
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "not allowed to invoke hidden commands from safe interpreter",
                (char *) NULL);
        return TCL_ERROR;
    }
    if (strcmp(Tcl_GetStringFromObj(objv[2], &len), "-global") == 0) {
        doGlobal = 1;
        if (objc < 4) {
            Tcl_WrongNumArgs(interp, 2, objv,
                    "path ?-global? cmd ?arg ..?");
            return TCL_ERROR;
        }
    }
    masterPtr = (Master *) Tcl_GetAssocData(slaveInterp,
            "tclMasterRecord", NULL);
    if (masterPtr == (Master *) NULL) {
        panic("SlaveObjectCmd: could not find master record");
    }
    Tcl_Preserve((ClientData) slaveInterp);
    if (doGlobal) {
        result = TclObjInvokeGlobal(slaveInterp, objc-3, objv+3,
                TCL_INVOKE_HIDDEN);
    } else {
        result = TclObjInvoke(slaveInterp, objc-2, objv+2,
                TCL_INVOKE_HIDDEN);
    }

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
             * the target interpreter back to our interpreter.
             */

            iPtr = (Interp *) slaveInterp;
            if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
                Tcl_AddErrorInfo(slaveInterp, "");
            }
            iPtr->flags &= (~(ERR_ALREADY_LOGGED));

            Tcl_ResetResult(interp);
            namePtr = Tcl_NewStringObj("errorInfo", -1);
            objPtr = Tcl_ObjGetVar2(slaveInterp, namePtr,
                    (Tcl_Obj *) NULL, TCL_GLOBAL_ONLY);
            string = Tcl_GetStringFromObj(objPtr, &len);
            Tcl_AddObjErrorInfo(interp, string, len);
            Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                    Tcl_GetVar2(slaveInterp, "errorCode", (char *)
                            NULL, TCL_GLOBAL_ONLY),
                    TCL_GLOBAL_ONLY);
            Tcl_DecrRefCount(namePtr);
        }

	/*
         * Move the result object from the slave to the master.
         */
                
        Tcl_SetObjResult(interp, Tcl_GetObjResult(slaveInterp));
        Tcl_ResetResult(slaveInterp);
    }
    Tcl_Release((ClientData) slaveInterp);
    return result;        
}

/*
 *----------------------------------------------------------------------
 *
 * SlaveMarkTrustedHelper --
 *
 *	Helper function to mark a slave interpreter as trusted (unsafe).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call the hard-wired security checks in the core no
 *	longer prevent the slave from performing certain operations.
 *
 *----------------------------------------------------------------------
 */

static int
SlaveMarkTrustedHelper(interp, slaveInterp, slavePtr, objc, objv)
    Tcl_Interp	*interp;		/* Current interpreter. */
    Tcl_Interp	*slaveInterp;		/* The slave interpreter. */
    Slave *slavePtr;			/* Its slave record. */
    int objc;				/* Count of arguments. */
    Tcl_Obj *CONST objv[];		/* Vector of arguments. */
{
    int len;
    
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "\"", Tcl_GetStringFromObj(objv[0], &len), " marktrusted\"",
                " can only be invoked from a trusted interpreter",
                (char *) NULL);
        return TCL_ERROR;
    }
    return MarkTrusted(slaveInterp);
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
SlaveObjectCmd(clientData, interp, objc, objv)
    ClientData clientData;		/* Slave interpreter. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* The argument vector. */
{
    Slave *slavePtr;			/* Slave record. */
    Tcl_Interp *slaveInterp;		/* Slave interpreter. */
    int result;				/* Loop counter, status return. */
    int len;				/* Length of command name. */

    /*
     * These are all the different subcommands for this command:
     */
    
    static char *subCmds[] = {
        "alias", "aliases",
        "eval", "expose",
        "hide", "hidden",
        "issafe", "invokehidden",
        "marktrusted",
        (char *) NULL};
    enum ISubCmdIdx {
        IAliasIdx, IAliasesIdx,
        IEvalIdx, IExposeIdx,
        IHideIdx, IHiddenIdx,
        IIsSafeIdx, IInvokeHiddenIdx,
        IMarkTrustedIdx
    } index;
    
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "cmd ?arg ...?");
        return TCL_ERROR;
    }

    slaveInterp = (Tcl_Interp *) clientData;
    if (slaveInterp == (Tcl_Interp *) NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "interpreter ", Tcl_GetStringFromObj(objv[0], &len),
                " has been deleted", (char *) NULL);
	return TCL_ERROR;
    }

    slavePtr = (Slave *) Tcl_GetAssocData(slaveInterp,
            "tclSlaveRecord", NULL);
    if (slavePtr == (Slave *) NULL) {
        panic("SlaveObjectCmd: could not find slave record");
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], subCmds, "option",
            0, (int *) &index);
    if (result != TCL_OK) {
        return result;
    }

    switch (index) {
        case IAliasIdx:
            return SlaveAliasHelper(interp, slaveInterp, slavePtr, objc, objv);
        case IAliasesIdx:
            return SlaveAliasesHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
        case IEvalIdx:
            return SlaveEvalHelper(interp, slaveInterp, slavePtr, objc, objv);
        case IExposeIdx:
            return SlaveExposeHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
        case IHideIdx:
            return SlaveHideHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
        case IHiddenIdx:
            return SlaveHiddenHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
        case IIsSafeIdx:
            return SlaveIsSafeHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
        case IInvokeHiddenIdx:
            return SlaveInvokeHiddenHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
        case IMarkTrustedIdx:
            return SlaveMarkTrustedHelper(interp, slaveInterp, slavePtr,
                    objc, objv);
    }

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
AliasCmd(clientData, interp, objc, objv)
    ClientData clientData;		/* Alias record. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument vector. */	
{
    Tcl_Interp *targetInterp;		/* Target for alias exec. */
    Interp *iPtr;			/* Internal type of target. */
    Alias *aliasPtr;			/* Describes the alias. */
    Tcl_Command cmd;			/* The target command. */
    Command *cmdPtr;			/* Points to target command. */
    Tcl_Namespace *targetNsPtr;	        /* Target command's namespace. */
    int result;				/* Result of execution. */
    int i, j, addObjc;			/* Loop counters. */
    int localObjc;			/* Local argument count. */
    Tcl_Obj **localObjv;		/* Local argument vector. */
    Tcl_Obj *namePtr, *objPtr;		/* Local object pointers. */
    char *string;			/* Local object string rep. */
    int len;				/* Dummy length arg. */
    
    aliasPtr = (Alias *) clientData;
    targetInterp = aliasPtr->targetInterp;

    /*
     * Look for the target command in the global namespace of the target
     * interpreter.
     */

    cmdPtr = NULL;
    targetNsPtr = Tcl_GetGlobalNamespace(aliasPtr->targetInterp);
    cmd = Tcl_FindCommand(targetInterp, aliasPtr->targetName,
            targetNsPtr, /*flags*/ 0);
    if (cmd != (Tcl_Command) NULL) {
        cmdPtr = (Command *) cmd;
    }

    iPtr = (Interp *) targetInterp;

    /*
     * If the command does not exist, invoke "unknown" in the master.
     */
    
    if (cmdPtr == NULL) {
        addObjc = aliasPtr->objc;
        localObjc = addObjc + objc + 1;
        localObjv = (Tcl_Obj **) ckalloc((unsigned) sizeof(Tcl_Obj *)
                * localObjc);
        
        localObjv[0] = Tcl_NewStringObj("unknown", -1);
        localObjv[1] = Tcl_NewStringObj(aliasPtr->targetName, -1);
        Tcl_IncrRefCount(localObjv[0]);
        Tcl_IncrRefCount(localObjv[1]);
        
        for (i = 0, j = 2; i < addObjc; i++, j++) {
            localObjv[j] = aliasPtr->objv[i];
        }
        for (i = 1; i < objc; i++, j++) {
            localObjv[j] = objv[i];
        }
        Tcl_Preserve((ClientData) targetInterp);
        result = TclObjInvoke(targetInterp, localObjc, localObjv, 0);

        Tcl_DecrRefCount(localObjv[0]);
        Tcl_DecrRefCount(localObjv[1]);
        
        ckfree((char *) localObjv);
        
        if (targetInterp != interp) {
            if (result == TCL_ERROR) {
                
                /*
                 * An error occurred, so transfer error information from
                 * the target interpreter back to our interpreter.
                 */

                if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
                    Tcl_AddErrorInfo((Tcl_Interp *) iPtr, "");
                }
                iPtr->flags &= (~(ERR_ALREADY_LOGGED));

                Tcl_ResetResult(interp);
                namePtr = Tcl_NewStringObj("errorInfo", -1);
                objPtr = Tcl_ObjGetVar2(targetInterp, namePtr,
                        (Tcl_Obj *) NULL, TCL_GLOBAL_ONLY);
                string = Tcl_GetStringFromObj(objPtr, &len);
                Tcl_AddObjErrorInfo(interp, string, len);
                Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                        Tcl_GetVar2(targetInterp, "errorCode", (char *)
                                NULL, TCL_GLOBAL_ONLY),
                        TCL_GLOBAL_ONLY);
                Tcl_DecrRefCount(namePtr);
            }

            /*
             * Transfer the result from the target interpreter to the
             * calling interpreter.
             */
            
            Tcl_SetObjResult(interp, Tcl_GetObjResult(targetInterp));
            Tcl_ResetResult(targetInterp);
        }

	Tcl_Release((ClientData) targetInterp);
        return result;
    }

    /*
     * Otherwise invoke the regular target command.
     */
    
    if (aliasPtr->objc <= 0) {
        localObjv = (Tcl_Obj **) objv;
        localObjc = objc;
    } else {
        addObjc = aliasPtr->objc;
        localObjc = objc + addObjc;
        localObjv =
            (Tcl_Obj **) ckalloc((unsigned) sizeof(Tcl_Obj *) * localObjc);
        localObjv[0] = objv[0];
        for (i = 0, j = 1; i < addObjc; i++, j++) {
            localObjv[j] = aliasPtr->objv[i];
        }
        for (i = 1; i < objc; i++, j++) {
            localObjv[j] = objv[i];
        }
    }

    iPtr->numLevels++;
    Tcl_Preserve((ClientData) targetInterp);

    /*
     * Reset the interpreter to its clean state; we do not know what state
     * it is in now..
     */
    
    Tcl_ResetResult(targetInterp);
    result = (cmdPtr->objProc)(cmdPtr->objClientData, targetInterp,
            localObjc, localObjv);

    iPtr->numLevels--;
    
    /*
     * Check if we are at the bottom of the stack for the target interpreter.
     * If so, check for special return codes.
     */
    
    if (iPtr->numLevels == 0) {
	if (result == TCL_RETURN) {
	    result = TclUpdateReturnInfo(iPtr);
	}
	if ((result != TCL_OK) && (result != TCL_ERROR)) {
	    Tcl_ResetResult(targetInterp);
	    if (result == TCL_BREAK) {
                Tcl_SetObjResult(targetInterp,
                        Tcl_NewStringObj("invoked \"break\" outside of a loop",
                                -1));
	    } else if (result == TCL_CONTINUE) {
                Tcl_SetObjResult(targetInterp,
                        Tcl_NewStringObj(
                            "invoked \"continue\" outside of a loop",
                            -1));
	    } else {
                char buf[128];

                sprintf(buf, "command returned bad code: %d", result);
                Tcl_SetObjResult(targetInterp, Tcl_NewStringObj(buf, -1));
	    }
	    result = TCL_ERROR;
	}
    }

    /*
     * Clean up any locally allocated argument vector structure.
     */
    
    if (localObjv != objv) {
        ckfree((char *) localObjv);
    }
    
    /*
     * Move the result from the target interpreter to the invoking
     * interpreter if they are different.
     *
     * Note: We cannot use aliasPtr any more because the alias may have
     * been deleted.
     */

    if (interp != targetInterp) {
        if (result == TCL_ERROR) {

            /*
             * An error occurred, so transfer the error information from
             * the target interpreter back to our interpreter.
             */

            if (!(iPtr->flags & ERR_ALREADY_LOGGED)) {
                Tcl_AddErrorInfo(targetInterp, "");
            }
            iPtr->flags &= (~(ERR_ALREADY_LOGGED));
            
            Tcl_ResetResult(interp);
            namePtr = Tcl_NewStringObj("errorInfo", -1);
            objPtr = Tcl_ObjGetVar2(targetInterp, namePtr, (Tcl_Obj *) NULL,
                    TCL_GLOBAL_ONLY);
            string = Tcl_GetStringFromObj(objPtr, &len);
            Tcl_AddObjErrorInfo(interp, string, len);
            Tcl_SetVar2(interp, "errorCode", (char *) NULL,
                    Tcl_GetVar2(targetInterp, "errorCode", (char *) NULL,
                            TCL_GLOBAL_ONLY),
                    TCL_GLOBAL_ONLY);
            Tcl_DecrRefCount(namePtr);
        }

	/*
         * Move the result object from one interpreter to the
         * other.
         */
                
        Tcl_SetObjResult(interp, Tcl_GetObjResult(targetInterp));
        Tcl_ResetResult(targetInterp);
    }
    Tcl_Release((ClientData) targetInterp);
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
    for (i = 0; i < aliasPtr->objc; i++) {
        Tcl_DecrRefCount(aliasPtr->objv[i]);
    }
    if (aliasPtr->objv != (Tcl_Obj **) NULL) {
        ckfree((char *) aliasPtr->objv);
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
    Master *masterPtr;			/* Interim storage. */

    masterPtr = (Master *) clientData;
    for (hPtr = Tcl_FirstHashEntry(&(masterPtr->slaveTable), &hSearch);
         hPtr != NULL;
         hPtr = Tcl_NextHashEntry(&hSearch)) {
        slavePtr = (Slave *) Tcl_GetHashValue(hPtr);
        (void) Tcl_DeleteCommandFromToken(interp, slavePtr->interpCmd);
    }
    Tcl_DeleteHashTable(&(masterPtr->slaveTable));

    for (hPtr = Tcl_FirstHashEntry(&(masterPtr->targetTable), &hSearch);
         hPtr != NULL;
         hPtr = Tcl_FirstHashEntry(&(masterPtr->targetTable), &hSearch)) {
        targetPtr = (Target *) Tcl_GetHashValue(hPtr);
        (void) Tcl_DeleteCommandFromToken(targetPtr->slaveInterp,
	        targetPtr->slaveCmd);
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

        Tcl_DeleteCommandFromToken(slavePtr->masterInterp,
	        slavePtr->interpCmd);
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
         * occupied by the hash entry and the alias record.
         */

        Tcl_DeleteCommandFromToken(interp, aliasPtr->slaveCmd);
    }
        
    /*
     * Finally dispose of the hash table and the slave record.
     */

    Tcl_DeleteHashTable(hTblPtr);
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
    Slave *slavePtr;			/* And its slave record. */

    masterPtr = (Master *) ckalloc((unsigned) sizeof(Master));

    Tcl_InitHashTable(&(masterPtr->slaveTable), TCL_STRING_KEYS);
    Tcl_InitHashTable(&(masterPtr->targetTable), TCL_ONE_WORD_KEYS);

    (void) Tcl_SetAssocData(interp, "tclMasterRecord", MasterRecordDeleteProc,
            (ClientData) masterPtr);

    slavePtr = (Slave *) ckalloc((unsigned) sizeof(Slave));

    slavePtr->masterInterp = (Tcl_Interp *) NULL;
    slavePtr->slaveEntry = (Tcl_HashEntry *) NULL;
    slavePtr->slaveInterp = interp;
    slavePtr->interpCmd = (Tcl_Command) NULL;
    Tcl_InitHashTable(&(slavePtr->aliasTable), TCL_STRING_KEYS);

    (void) Tcl_SetAssocData(interp, "tclSlaveRecord", SlaveRecordDeleteProc,
            (ClientData) slavePtr);
    
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
    Interp *iPtr;

    if (interp == (Tcl_Interp *) NULL) {
        return 0;
    }
    iPtr = (Interp *) interp;

    return ( (iPtr->flags) & SAFE_INTERP ) ? 1 : 0 ;
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
    Master *masterPtr;			/* Master record for same. */

    if ((interp == (Tcl_Interp *) NULL) || (slavePath == (char *) NULL)) {
        return NULL;
    }
    masterPtr = (Master *) Tcl_GetAssocData(interp, "tclMasterRecord",
            NULL); 
    if (masterPtr == (Master *) NULL) {
        panic("CreatSlave: could not find master record");
    }
    return CreateSlave(interp, masterPtr, slavePath, isSafe);
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
 *	A standard Tcl result.
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
    Tcl_Obj **objv;
    int i;
    int result;
    
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
    objv = (Tcl_Obj **) ckalloc((unsigned) sizeof(Tcl_Obj *) * argc);
    for (i = 0; i < argc; i++) {
        objv[i] = Tcl_NewStringObj(argv[i], -1);
        Tcl_IncrRefCount(objv[i]);
    }
    
    result = AliasCreationHelper(slaveInterp, slaveInterp, targetInterp,
            masterPtr, slaveCmd, targetCmd, argc, objv);

    ckfree((char *) objv);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateAliasObj --
 *
 *	Object version: Creates an alias between two interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates a new alias.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CreateAliasObj(slaveInterp, slaveCmd, targetInterp, targetCmd, objc, objv)
    Tcl_Interp *slaveInterp;		/* Interpreter for source command. */
    char *slaveCmd;			/* Command to install in slave. */
    Tcl_Interp *targetInterp;		/* Interpreter for target command. */
    char *targetCmd;			/* Name of target command. */
    int objc;				/* How many additional arguments? */
    Tcl_Obj *CONST objv[];		/* Argument vector. */
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
    return AliasCreationHelper(slaveInterp, slaveInterp, targetInterp,
            masterPtr, slaveCmd, targetCmd, objc, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAlias --
 *
 *	Gets information about an alias.
 *
 * Results:
 *	A standard Tcl result. 
 *
 * Side effects:
 *	None.
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
    int len;
    int i;

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
        *argcPtr = aliasPtr->objc;
    }
    if (argvPtr != (char ***) NULL) {
        *argvPtr = (char **) ckalloc((unsigned) sizeof(char *) *
                aliasPtr->objc);
        for (i = 0; i < aliasPtr->objc; i++) {
            *argvPtr[i] = Tcl_GetStringFromObj(aliasPtr->objv[i], &len);
        }
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ObjGetAlias --
 *
 *	Object version: Gets information about an alias.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetAliasObj(interp, aliasName, targetInterpPtr, targetNamePtr, objcPtr,
        objvPtr)
    Tcl_Interp *interp;			/* Interp to start search from. */
    char *aliasName;			/* Name of alias to find. */
    Tcl_Interp **targetInterpPtr;	/* (Return) target interpreter. */
    char **targetNamePtr;		/* (Return) name of target command. */
    int *objcPtr;			/* (Return) count of addnl args. */
    Tcl_Obj ***objvPtr;			/* (Return) additional args. */
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
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "alias \"", aliasName, "\" not found", (char *) NULL);
        return TCL_ERROR;
    }
    aliasPtr = (Alias *) Tcl_GetHashValue(hPtr);
    if (targetInterpPtr != (Tcl_Interp **) NULL) {
        *targetInterpPtr = aliasPtr->targetInterp;
    }
    if (targetNamePtr != (char **) NULL) {
        *targetNamePtr = aliasPtr->targetName;
    }
    if (objcPtr != (int *) NULL) {
        *objcPtr = aliasPtr->objc;
    }
    if (objvPtr != (Tcl_Obj ***) NULL) {
        *objvPtr = aliasPtr->objv;
    }
    return TCL_OK;
}
