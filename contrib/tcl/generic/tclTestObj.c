/* 
 * tclTestObj.c --
 *
 *	This file contains C command procedures for the additional Tcl
 *	commands that are used for testing implementations of the Tcl object
 *	types. These commands are not normally included in Tcl
 *	applications; they're only used for testing.
 *
 * Copyright (c) 1995, 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclTestObj.c 1.27 97/05/19 17:37:31
 */

#include "tclInt.h"

/*
 * An array of Tcl_Obj pointers used in the commands that operate on or get
 * the values of Tcl object-valued variables. varPtr[i] is the i-th
 * variable's Tcl_Obj *.
 */

#define NUMBER_OF_OBJECT_VARS 20
static Tcl_Obj *varPtr[NUMBER_OF_OBJECT_VARS];

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		CheckIfVarUnset _ANSI_ARGS_((Tcl_Interp *interp,
			    int varIndex));
static int		GetVariableIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int *indexPtr));
static void		SetVarToObj _ANSI_ARGS_((int varIndex,
			    Tcl_Obj *objPtr));
int			TclObjTest_Init _ANSI_ARGS_((Tcl_Interp *interp));
static int		TestbooleanobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		TestconvertobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		TestdoubleobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		TestindexobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		TestintobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		TestobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
static int		TeststringobjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));

/*
 *----------------------------------------------------------------------
 *
 * TclObjTest_Init --
 *
 *	This procedure creates additional commands that are used to test the
 *	Tcl object support.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Creates and registers several new testing commands.
 *
 *----------------------------------------------------------------------
 */

int
TclObjTest_Init(interp)
    Tcl_Interp *interp;
{
    register int i;
    
    for (i = 0;  i < NUMBER_OF_OBJECT_VARS;  i++) {
        varPtr[i] = NULL;
    }
	
    Tcl_CreateObjCommand(interp, "testbooleanobj", TestbooleanobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "testconvertobj", TestconvertobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "testdoubleobj", TestdoubleobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "testintobj", TestintobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "testindexobj", TestindexobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "testobj", TestobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "teststringobj", TeststringobjCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestbooleanobjCmd --
 *
 *	This procedure implements the "testbooleanobj" command.  It is used
 *	to test the boolean Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees boolean objects, and also converts objects to
 *	have boolean type.
 *
 *----------------------------------------------------------------------
 */

static int
TestbooleanobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int varIndex, boolValue, length;
    char *index, *subCmd;

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */

    index = Tcl_GetStringFromObj(objv[2], &length);
    if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    subCmd = Tcl_GetStringFromObj(objv[1], &length);
    if (strcmp(subCmd, "set") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetBooleanFromObj(interp, objv[3], &boolValue) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly. Otherwise, if RC>1 (i.e. the object is shared),
	 * we must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBooleanObj(varPtr[varIndex], boolValue);
	} else {
	    SetVarToObj(varIndex, Tcl_NewBooleanObj(boolValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "get") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "not") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBooleanFromObj(interp, varPtr[varIndex],
				  &boolValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBooleanObj(varPtr[varIndex], !boolValue);
	} else {
	    SetVarToObj(varIndex, Tcl_NewBooleanObj(!boolValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetStringFromObj(objv[1], (int *) NULL),
		"\": must be set, get, or not", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestconvertobjCmd --
 *
 *	This procedure implements the "testconvertobj" command. It is used
 *	to test converting objects to new types.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Converts objects to new types.
 *
 *----------------------------------------------------------------------
 */

static int
TestconvertobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int length;
    char *subCmd;
    char buf[20];

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */

    subCmd = Tcl_GetStringFromObj(objv[1], &length);
    if (strcmp(subCmd, "double") == 0) {
	double d;

	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetDoubleFromObj(interp, objv[2], &d) != TCL_OK) {
	    return TCL_ERROR;
	}
	sprintf(buf, "%f", d);
        Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetStringFromObj(objv[1], (int *) NULL),
		"\": must be double", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdoubleobjCmd --
 *
 *	This procedure implements the "testdoubleobj" command.  It is used
 *	to test the double-precision floating point Tcl object type
 *	implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees double objects, and also converts objects to
 *	have double type.
 *
 *----------------------------------------------------------------------
 */

static int
TestdoubleobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int varIndex, length;
    double doubleValue;
    char *index, *subCmd, *string;
	
    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */

    index = Tcl_GetStringFromObj(objv[2], &length);
    if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    subCmd = Tcl_GetStringFromObj(objv[1], &length);
    if (strcmp(subCmd, "set") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	string = Tcl_GetStringFromObj(objv[3], &length);
	if (Tcl_GetDouble(interp, string, &doubleValue) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly. Otherwise, if RC>1 (i.e. the object is shared),
	 * we must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetDoubleObj(varPtr[varIndex], doubleValue);
	} else {
	    SetVarToObj(varIndex, Tcl_NewDoubleObj(doubleValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "get") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "mult10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetDoubleFromObj(interp, varPtr[varIndex],
				 &doubleValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetDoubleObj(varPtr[varIndex], (doubleValue * 10.0));
	} else {
	    SetVarToObj(varIndex, Tcl_NewDoubleObj( (doubleValue * 10.0) ));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "div10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetDoubleFromObj(interp, varPtr[varIndex],
				 &doubleValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetDoubleObj(varPtr[varIndex], (doubleValue / 10.0));
	} else {
	    SetVarToObj(varIndex, Tcl_NewDoubleObj( (doubleValue / 10.0) ));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetStringFromObj(objv[1], (int *) NULL),
		"\": must be set, get, mult10, or div10", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestindexobjCmd --
 *
 *	This procedure implements the "testindexobj" command. It is used to
 *	test the index Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees int objects, and also converts objects to
 *	have int type.
 *
 *----------------------------------------------------------------------
 */

static int
TestindexobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int allowAbbrev, index, index2, setError, i, dummy, result;
    char **argv;
    static char *tablePtr[] = {"a", "b", "check", (char *) NULL};

    if ((objc == 3) && (strcmp(Tcl_GetStringFromObj(objv[1], &dummy),
	    "check") == 0)) {
	/*
	 * This code checks to be sure that the results of
	 * Tcl_GetIndexFromObj are properly cached in the object and
	 * returned on subsequent lookups.
	 */

	Tcl_GetIndexFromObj((Tcl_Interp *) NULL, objv[1], tablePtr,
		"token", 0, &index);
	if (Tcl_GetIntFromObj(interp, objv[2], &index2) != TCL_OK) {
	    return TCL_ERROR;
	}
	objv[1]->internalRep.twoPtrValue.ptr2 = (VOID *) index2;
	result = Tcl_GetIndexFromObj((Tcl_Interp *) NULL, objv[1],
		tablePtr, "token", 0, &index);
	if (result == TCL_OK) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), index);
	}
	return result;
    }

    if (objc < 5) {
	Tcl_AppendToObj(Tcl_GetObjResult(interp), "wrong # args", -1);
	return TCL_ERROR;
    }

    if (Tcl_GetBooleanFromObj(interp, objv[1], &setError) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[2], &allowAbbrev) != TCL_OK) {
	return TCL_ERROR;
    }
    argv = (char **) ckalloc((unsigned) ((objc-3) * sizeof(char *)));
    for (i = 4; i < objc; i++) {
	argv[i-4] = Tcl_GetStringFromObj(objv[i], &dummy);
    }
    argv[objc-4] = NULL;
    result = Tcl_GetIndexFromObj(setError ? interp : NULL, objv[3],
	    argv, "token", allowAbbrev ? 0 : TCL_EXACT, &index);
    ckfree((char *) argv);
    if (result == TCL_OK) {
	Tcl_SetIntObj(Tcl_GetObjResult(interp), index);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TestintobjCmd --
 *
 *	This procedure implements the "testintobj" command. It is used to
 *	test the int Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees int objects, and also converts objects to
 *	have int type.
 *
 *----------------------------------------------------------------------
 */

static int
TestintobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int intValue, varIndex, length, i;
    long longValue;
    char *index, *subCmd, *string;
	
    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */

    index = Tcl_GetStringFromObj(objv[2], &length);
    if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    subCmd = Tcl_GetStringFromObj(objv[1], &length);
    if (strcmp(subCmd, "set") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	string = Tcl_GetStringFromObj(objv[3], &length);
	if (Tcl_GetInt(interp, string, &i) != TCL_OK) {
	    return TCL_ERROR;
	}
	intValue = i;

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly. Otherwise, if RC>1 (i.e. the object is shared),
	 * we must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetIntObj(varPtr[varIndex], intValue);
	} else {
	    SetVarToObj(varIndex, Tcl_NewIntObj(intValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "set2") == 0) { /* doesn't set result */
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	string = Tcl_GetStringFromObj(objv[3], &length);
	if (Tcl_GetInt(interp, string, &i) != TCL_OK) {
	    return TCL_ERROR;
	}
	intValue = i;
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetIntObj(varPtr[varIndex], intValue);
	} else {
	    SetVarToObj(varIndex, Tcl_NewIntObj(intValue));
	}
    } else if (strcmp(subCmd, "setlong") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	string = Tcl_GetStringFromObj(objv[3], &length);
	if (Tcl_GetInt(interp, string, &i) != TCL_OK) {
	    return TCL_ERROR;
	}
	intValue = i;
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetLongObj(varPtr[varIndex], intValue);
	} else {
	    SetVarToObj(varIndex, Tcl_NewLongObj(intValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "setmaxlong") == 0) {
	long maxLong = LONG_MAX;
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetLongObj(varPtr[varIndex], maxLong);
	} else {
	    SetVarToObj(varIndex, Tcl_NewLongObj(maxLong));
	}
    } else if (strcmp(subCmd, "ismaxlong") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetLongFromObj(interp, varPtr[varIndex], &longValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        ((longValue == LONG_MAX)? "1" : "0"), -1);
    } else if (strcmp(subCmd, "get") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "inttoobigtest") == 0) {
	/*
	 * If long ints have more bits than ints on this platform, verify
	 * that Tcl_GetIntFromObj returns an error if the long int held
	 * in an integer object's internal representation is too large
	 * to fit in an int.
	 */
	
	long maxLong = LONG_MAX;
	
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (INT_MAX == LONG_MAX) { /* int is same size as long int */
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), "1", -1);
	} else {
	    if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
		Tcl_SetLongObj(varPtr[varIndex], maxLong);
	    } else {
		SetVarToObj(varIndex, Tcl_NewLongObj(maxLong));
	    }
	    if (Tcl_GetIntFromObj(interp, varPtr[varIndex], &i) != TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp), "1", -1);
		return TCL_OK;
	    }
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), "0", -1);
	}
    } else if (strcmp(subCmd, "mult10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, varPtr[varIndex],
			      &intValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetIntObj(varPtr[varIndex], (intValue * 10));
	} else {
	    SetVarToObj(varIndex, Tcl_NewIntObj( (intValue * 10) ));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "div10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, varPtr[varIndex],
			      &intValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetIntObj(varPtr[varIndex], (intValue / 10));
	} else {
	    SetVarToObj(varIndex, Tcl_NewIntObj( (intValue / 10) ));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetStringFromObj(objv[1], (int *) NULL),
		"\": must be set, get, mult10, or div10", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestobjCmd --
 *
 *	This procedure implements the "testobj" command. It is used to test
 *	the type-independent portions of the Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees objects.
 *
 *----------------------------------------------------------------------
 */

static int
TestobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int varIndex, destIndex, i;
    char *index, *subCmd, *string;
    Tcl_ObjType *targetType;
    char buf[20];
    int length;
	
    if (objc < 2) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * THIS FAILS IF AN OBJECT'S STRING REP HAS A NULL BYTE.
     */

    subCmd = Tcl_GetStringFromObj(objv[1], &length);
    if (strcmp(subCmd, "assign") == 0) {
        if (objc != 4) {
            goto wrongNumArgs;
        }
        index = Tcl_GetStringFromObj(objv[2], &length);
        if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	string = Tcl_GetStringFromObj(objv[3], &length);
        if (GetVariableIndex(interp, string, &destIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        SetVarToObj(destIndex, varPtr[varIndex]);
	Tcl_SetObjResult(interp, varPtr[destIndex]);
     } else if (strcmp(subCmd, "convert") == 0) {
        char *typeName;
        if (objc != 4) {
            goto wrongNumArgs;
        }
        index = Tcl_GetStringFromObj(objv[2], &length);
        if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
        typeName = Tcl_GetStringFromObj(objv[3], &length);
        if ((targetType = Tcl_GetObjType(typeName)) == NULL) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "no type ", typeName, " found", (char *) NULL);
            return TCL_ERROR;
        }
        if (Tcl_ConvertToType(interp, varPtr[varIndex], targetType)
            != TCL_OK) {
            return TCL_ERROR;
        }
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "duplicate") == 0) {
        if (objc != 4) {
            goto wrongNumArgs;
        }
        index = Tcl_GetStringFromObj(objv[2], &length);
        if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
	string = Tcl_GetStringFromObj(objv[3], &length);
        if (GetVariableIndex(interp, string, &destIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        SetVarToObj(destIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	Tcl_SetObjResult(interp, varPtr[destIndex]);
    } else if (strcmp(subCmd, "freeallvars") == 0) {
        if (objc != 2) {
            goto wrongNumArgs;
        }
        for (i = 0;  i < NUMBER_OF_OBJECT_VARS;  i++) {
            if (varPtr[i] != NULL) {
                Tcl_DecrRefCount(varPtr[i]);
                varPtr[i] = NULL;
            }
        }
    } else if (strcmp(subCmd, "newobj") == 0) {
        if (objc != 3) {
            goto wrongNumArgs;
        }
        index = Tcl_GetStringFromObj(objv[2], &length);
        if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        SetVarToObj(varIndex, Tcl_NewObj());
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "refcount") == 0) {
        if (objc != 3) {
            goto wrongNumArgs;
        }
        index = Tcl_GetStringFromObj(objv[2], &length);
        if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
        sprintf(buf, "%d", varPtr[varIndex]->refCount);
        Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
    } else if (strcmp(subCmd, "type") == 0) {
        if (objc != 3) {
            goto wrongNumArgs;
        }
        index = Tcl_GetStringFromObj(objv[2], &length);
        if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        if (CheckIfVarUnset(interp, varIndex)) {
	    return TCL_ERROR;
	}
        if (varPtr[varIndex]->typePtr == NULL) { /* a string! */
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), "string", -1);
        } else {
            Tcl_AppendToObj(Tcl_GetObjResult(interp),
                    varPtr[varIndex]->typePtr->name, -1);
        }
    } else if (strcmp(subCmd, "types") == 0) {
        if (objc != 2) {
            goto wrongNumArgs;
        }
	if (Tcl_AppendAllObjTypes(interp, Tcl_GetObjResult(interp)) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"",
		Tcl_GetStringFromObj(objv[1], (int *) NULL),
		"\": must be assign, convert, duplicate, freeallvars, ",
		"newobj, objcount, refcount, type, or types",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TeststringobjCmd --
 *
 *	This procedure implements the "teststringobj" command. It is used to
 *	test the string Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees string objects, and also converts objects to
 *	have string type.
 *
 *----------------------------------------------------------------------
 */

static int
TeststringobjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int varIndex, option, i, length;
#define MAX_STRINGS 10
    char *index, *string, *strings[MAX_STRINGS+1];
    static char *options[] = {
	"append", "appendstrings", "get", "length", "length2",
	"set", "set2", "setlength", (char *) NULL
    };

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    index = Tcl_GetStringFromObj(objv[2], (int *) NULL);
    if (GetVariableIndex(interp, index, &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0, &option)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    switch (option) {
	case 0:				/* append */
	    if (objc != 5) {
		goto wrongNumArgs;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[4], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (varPtr[varIndex] == NULL) {
		SetVarToObj(varIndex, Tcl_NewObj());
	    }
	    
	    /*
	     * If the object bound to variable "varIndex" is shared, we must
	     * "copy on write" and append to a copy of the object. 
	     */
	    
	    if (Tcl_IsShared(varPtr[varIndex])) {
		SetVarToObj(varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	    }
	    string = Tcl_GetStringFromObj(objv[3], (int *) NULL);
	    Tcl_AppendToObj(varPtr[varIndex], string, length);
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 1:				/* appendstrings */
	    if (objc > (MAX_STRINGS+3)) {
		goto wrongNumArgs;
	    }
	    if (varPtr[varIndex] == NULL) {
		SetVarToObj(varIndex, Tcl_NewObj());
	    }

	    /*
	     * If the object bound to variable "varIndex" is shared, we must
	     * "copy on write" and append to a copy of the object. 
	     */

	    if (Tcl_IsShared(varPtr[varIndex])) {
		SetVarToObj(varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	    }
	    for (i = 3;  i < objc;  i++) {
		strings[i-3] = Tcl_GetStringFromObj(objv[i], (int *) NULL);
	    }
	    strings[objc-3] = NULL;
	    Tcl_AppendStringsToObj(varPtr[varIndex], strings[0], strings[1],
		    strings[2], strings[3], strings[4], strings[5],
		    strings[6], strings[7], strings[8], strings[9],
		    strings[10], strings[11]);
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 2:				/* get */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    if (CheckIfVarUnset(interp, varIndex)) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 3:				/* length */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), (varPtr[varIndex] != NULL)
		    ? varPtr[varIndex]->length : -1);
	    break;
	case 4:				/* length2 */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), (varPtr[varIndex] != NULL)
		    ? (int) varPtr[varIndex]->internalRep.longValue : -1);
	    break;
	case 5:				/* set */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }

	    /*
	     * If the object currently bound to the variable with index
	     * varIndex has ref count 1 (i.e. the object is unshared) we
	     * can modify that object directly. Otherwise, if RC>1 (i.e.
	     * the object is shared), we must create a new object to
	     * modify/set and decrement the old formerly-shared object's
	     * ref count. This is "copy on write".
	     */
    
	    string = Tcl_GetStringFromObj(objv[3], &length);
	    if ((varPtr[varIndex] != NULL)
		    && !Tcl_IsShared(varPtr[varIndex])) {
		Tcl_SetStringObj(varPtr[varIndex], string, length);
	    } else {
		SetVarToObj(varIndex, Tcl_NewStringObj(string, length));
	    }
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 6:				/* set2 */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }
	    SetVarToObj(varIndex, objv[3]);
	    break;
	case 7:				/* setlength */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (varPtr[varIndex] != NULL) {
		Tcl_SetObjLength(varPtr[varIndex], length);
	    }
	    break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SetVarToObj --
 *
 *	Utility routine to assign a Tcl_Obj* to a test variable. The
 *	Tcl_Obj* can be NULL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This routine handles ref counting details for assignment:
 *	i.e. the old value's ref count must be decremented (if not NULL) and
 *	the new one incremented (also if not NULL).
 *
 *----------------------------------------------------------------------
 */

static void
SetVarToObj(varIndex, objPtr)
    int varIndex;		/* Designates the assignment variable. */
    Tcl_Obj *objPtr;		/* Points to object to assign to var. */
{
    if (varPtr[varIndex] != NULL) {
	Tcl_DecrRefCount(varPtr[varIndex]);
    }
    varPtr[varIndex] = objPtr;
    if (objPtr != NULL) {
	Tcl_IncrRefCount(objPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetVariableIndex --
 *
 *	Utility routine to get a test variable index from the command line.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetVariableIndex(interp, string, indexPtr)
    Tcl_Interp *interp;         /* Interpreter for error reporting. */
    char *string;               /* String containing a variable index
				 * specified as a nonnegative number less
				 * than NUMBER_OF_OBJECT_VARS. */
    int *indexPtr;              /* Place to store converted result. */
{
    int index;
    
    if (Tcl_GetInt(interp, string, &index) != TCL_OK) {
	return TCL_ERROR;
    }
    if (index < 0 || index >= NUMBER_OF_OBJECT_VARS) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), "bad variable index", -1);
	return TCL_ERROR;
    }

    *indexPtr = index;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CheckIfVarUnset --
 *
 *	Utility procedure that checks whether a test variable is readable:
 *	i.e., that varPtr[varIndex] is non-NULL.
 *
 * Results:
 *	1 if the test variable is unset (NULL); 0 otherwise.
 *
 * Side effects:
 *	Sets the interpreter result to an error message if the variable is
 *	unset (NULL).
 *
 *----------------------------------------------------------------------
 */

static int
CheckIfVarUnset(interp, varIndex)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    int varIndex;		/* Index of the test variable to check. */
{
    if (varPtr[varIndex] == NULL) {
	char buf[100];
	
	sprintf(buf, "variable %d is unset (NULL)", varIndex);
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	return 1;
    }
    return 0;
}
