/* 
 * tclCompile.c --
 *
 *	This file contains procedures that compile Tcl commands or parts
 *	of commands (like quoted strings or nested sub-commands) into a
 *	sequence of instructions ("bytecodes"). 
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCompile.c 1.80 97/09/18 18:23:30
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Variable that controls whether compilation tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no compilation tracing
 *    1: summarize compilation of top level cmds and proc bodies
 *    2: display all instructions of each ByteCode compiled
 * This variable is linked to the Tcl variable "tcl_traceCompile".
 */

int tclTraceCompile = 0;
static int traceInitialized = 0;

/*
 * Count of the number of compilations and various other compilation-
 * related statistics.
 */

#ifdef TCL_COMPILE_STATS
long tclNumCompilations = 0;
double tclTotalSourceBytes = 0.0;
double tclTotalCodeBytes = 0.0;

double tclTotalInstBytes = 0.0;
double tclTotalObjBytes = 0.0;
double tclTotalExceptBytes = 0.0;
double tclTotalAuxBytes = 0.0;
double tclTotalCmdMapBytes = 0.0;

double tclCurrentSourceBytes = 0.0;
double tclCurrentCodeBytes = 0.0;

int tclSourceCount[32];
int tclByteCodeCount[32];
#endif /* TCL_COMPILE_STATS */

/*
 * A table describing the Tcl bytecode instructions. The entries in this
 * table must correspond to the list of instructions in tclInt.h. The names
 * "op1" and "op4" refer to an instruction's one or four byte first operand.
 * Similarly, "stktop" and "stknext" refer to the topmost and next to
 * topmost stack elements.
 *
 * Note that the load, store, and incr instructions do not distinguish local
 * from global variables; the bytecode interpreter at runtime uses the
 * existence of a procedure call frame to distinguish these.
 */

InstructionDesc instructionTable[] = {
   /* Name	      Bytes #Opnds Operand types        Stack top, next   */
    {"done",	          1,   0,   {OPERAND_NONE}},
        /* Finish ByteCode execution and return stktop (top stack item) */
    {"push1",	          2,   1,   {OPERAND_UINT1}},
        /* Push object at ByteCode objArray[op1] */
    {"push4",	          5,   1,   {OPERAND_UINT4}},
        /* Push object at ByteCode objArray[op4] */
    {"pop",	          1,   0,   {OPERAND_NONE}},
        /* Pop the topmost stack object */
    {"dup",	          1,   0,   {OPERAND_NONE}},
        /* Duplicate the topmost stack object and push the result */
    {"concat1",	          2,   1,   {OPERAND_UINT1}},
        /* Concatenate the top op1 items and push result */
    {"invokeStk1",        2,   1,   {OPERAND_UINT1}},
        /* Invoke command named objv[0]; <objc,objv> = <op1,top op1> */
    {"invokeStk4",        5,   1,   {OPERAND_UINT4}},
        /* Invoke command named objv[0]; <objc,objv> = <op4,top op4> */
    {"evalStk",           1,   0,   {OPERAND_NONE}},
        /* Evaluate command in stktop using Tcl_EvalObj. */
    {"exprStk",           1,   0,   {OPERAND_NONE}},
        /* Execute expression in stktop using Tcl_ExprStringObj. */
    
    {"loadScalar1",       2,   1,   {OPERAND_UINT1}},
        /* Load scalar variable at index op1 <= 255 in call frame */
    {"loadScalar4",       5,   1,   {OPERAND_UINT4}},
        /* Load scalar variable at index op1 >= 256 in call frame */
    {"loadScalarStk",     1,   0,   {OPERAND_NONE}},
        /* Load scalar variable; scalar's name is stktop */
    {"loadArray1",        2,   1,   {OPERAND_UINT1}},
        /* Load array element; array at slot op1<=255, element is stktop */
    {"loadArray4",        5,   1,   {OPERAND_UINT4}},
        /* Load array element; array at slot op1 > 255, element is stktop */
    {"loadArrayStk",      1,   0,   {OPERAND_NONE}},
        /* Load array element; element is stktop, array name is stknext */
    {"loadStk",           1,   0,   {OPERAND_NONE}},
        /* Load general variable; unparsed variable name is stktop */
    {"storeScalar1",      2,   1,   {OPERAND_UINT1}},
        /* Store scalar variable at op1<=255 in frame; value is stktop */
    {"storeScalar4",      5,   1,   {OPERAND_UINT4}},
        /* Store scalar variable at op1 > 255 in frame; value is stktop */
    {"storeScalarStk",    1,   0,   {OPERAND_NONE}},
        /* Store scalar; value is stktop, scalar name is stknext */
    {"storeArray1",       2,   1,   {OPERAND_UINT1}},
        /* Store array element; array at op1<=255, value is top then elem */
    {"storeArray4",       5,   1,   {OPERAND_UINT4}},
        /* Store array element; array at op1>=256, value is top then elem */
    {"storeArrayStk",     1,   0,   {OPERAND_NONE}},
        /* Store array element; value is stktop, then elem, array names */
    {"storeStk",          1,   0,   {OPERAND_NONE}},
        /* Store general variable; value is stktop, then unparsed name */
    
    {"incrScalar1",       2,   1,   {OPERAND_UINT1}},
        /* Incr scalar at index op1<=255 in frame; incr amount is stktop */
    {"incrScalarStk",     1,   0,   {OPERAND_NONE}},
        /* Incr scalar; incr amount is stktop, scalar's name is stknext */
    {"incrArray1",        2,   1,   {OPERAND_UINT1}},
        /* Incr array elem; arr at slot op1<=255, amount is top then elem */
    {"incrArrayStk",      1,   0,   {OPERAND_NONE}},
        /* Incr array element; amount is top then elem then array names */
    {"incrStk",           1,   0,   {OPERAND_NONE}},
        /* Incr general variable; amount is stktop then unparsed var name */
    {"incrScalar1Imm",    3,   2,   {OPERAND_UINT1, OPERAND_INT1}},
        /* Incr scalar at slot op1 <= 255; amount is 2nd operand byte */
    {"incrScalarStkImm",  2,   1,   {OPERAND_INT1}},
        /* Incr scalar; scalar name is stktop; incr amount is op1 */
    {"incrArray1Imm",     3,   2,   {OPERAND_UINT1, OPERAND_INT1}},
        /* Incr array elem; array at slot op1 <= 255, elem is stktop,
	 * amount is 2nd operand byte */
    {"incrArrayStkImm",   2,   1,   {OPERAND_INT1}},
        /* Incr array element; elem is top then array name, amount is op1 */
    {"incrStkImm",        2,   1,   {OPERAND_INT1}},
        /* Incr general variable; unparsed name is top, amount is op1 */
    
    {"jump1",             2,   1,   {OPERAND_INT1}},
        /* Jump relative to (pc + op1) */
    {"jump4",             5,   1,   {OPERAND_INT4}},
        /* Jump relative to (pc + op4) */
    {"jumpTrue1",         2,   1,   {OPERAND_INT1}},
        /* Jump relative to (pc + op1) if stktop expr object is true */
    {"jumpTrue4",         5,   1,   {OPERAND_INT4}},
        /* Jump relative to (pc + op4) if stktop expr object is true */
    {"jumpFalse1",        2,   1,   {OPERAND_INT1}},
        /* Jump relative to (pc + op1) if stktop expr object is false */
    {"jumpFalse4",        5,   1,   {OPERAND_INT4}},
        /* Jump relative to (pc + op4) if stktop expr object is false */

    {"lor",               1,   0,   {OPERAND_NONE}},
        /* Logical or:	push (stknext || stktop) */
    {"land",              1,   0,   {OPERAND_NONE}},
        /* Logical and:	push (stknext && stktop) */
    {"bitor",             1,   0,   {OPERAND_NONE}},
        /* Bitwise or:	push (stknext | stktop) */
    {"bitxor",            1,   0,   {OPERAND_NONE}},
        /* Bitwise xor	push (stknext ^ stktop) */
    {"bitand",            1,   0,   {OPERAND_NONE}},
        /* Bitwise and:	push (stknext & stktop) */
    {"eq",                1,   0,   {OPERAND_NONE}},
        /* Equal:	push (stknext == stktop) */
    {"neq",               1,   0,   {OPERAND_NONE}},
        /* Not equal:	push (stknext != stktop) */
    {"lt",                1,   0,   {OPERAND_NONE}},
        /* Less:	push (stknext < stktop) */
    {"gt",                1,   0,   {OPERAND_NONE}},
        /* Greater:	push (stknext || stktop) */
    {"le",                1,   0,   {OPERAND_NONE}},
        /* Logical or:	push (stknext || stktop) */
    {"ge",                1,   0,   {OPERAND_NONE}},
        /* Logical or:	push (stknext || stktop) */
    {"lshift",            1,   0,   {OPERAND_NONE}},
        /* Left shift:	push (stknext << stktop) */
    {"rshift",            1,   0,   {OPERAND_NONE}},
        /* Right shift:	push (stknext >> stktop) */
    {"add",               1,   0,   {OPERAND_NONE}},
        /* Add:		push (stknext + stktop) */
    {"sub",               1,   0,   {OPERAND_NONE}},
        /* Sub:		push (stkext - stktop) */
    {"mult",              1,   0,   {OPERAND_NONE}},
        /* Multiply:	push (stknext * stktop) */
    {"div",               1,   0,   {OPERAND_NONE}},
        /* Divide:	push (stknext / stktop) */
    {"mod",               1,   0,   {OPERAND_NONE}},
        /* Mod:		push (stknext % stktop) */
    {"uplus",             1,   0,   {OPERAND_NONE}},
        /* Unary plus:	push +stktop */
    {"uminus",            1,   0,   {OPERAND_NONE}},
        /* Unary minus:	push -stktop */
    {"bitnot",            1,   0,   {OPERAND_NONE}},
        /* Bitwise not:	push ~stktop */
    {"not",               1,   0,   {OPERAND_NONE}},
        /* Logical not:	push !stktop */
    {"callBuiltinFunc1",  2,   1,   {OPERAND_UINT1}},
        /* Call builtin math function with index op1; any args are on stk */
    {"callFunc1",         2,   1,   {OPERAND_UINT1}},
        /* Call non-builtin func objv[0]; <objc,objv>=<op1,top op1>  */
    {"tryCvtToNumeric",   1,   0,   {OPERAND_NONE}},
        /* Try converting stktop to first int then double if possible. */

    {"break",             1,   0,   {OPERAND_NONE}},
        /* Abort closest enclosing loop; if none, return TCL_BREAK code. */
    {"continue",          1,   0,   {OPERAND_NONE}},
        /* Skip to next iteration of closest enclosing loop; if none,
	 * return TCL_CONTINUE code. */

    {"foreach_start4",    5,   1,   {OPERAND_UINT4}},
        /* Initialize execution of a foreach loop. Operand is aux data index
	 * of the ForeachInfo structure for the foreach command. */
    {"foreach_step4",     5,   1,   {OPERAND_UINT4}},
        /* "Step" or begin next iteration of foreach loop. Push 0 if to
	 *  terminate loop, else push 1. */

    {"beginCatch4",	  5,   1,   {OPERAND_UINT4}},
        /* Record start of catch with the operand's exception range index.
	 * Push the current stack depth onto a special catch stack. */
    {"endCatch",	  1,   0,   {OPERAND_NONE}},
        /* End of last catch. Pop the bytecode interpreter's catch stack. */
    {"pushResult",	  1,   0,   {OPERAND_NONE}},
        /* Push the interpreter's object result onto the stack. */
    {"pushReturnCode",	  1,   0,   {OPERAND_NONE}},
        /* Push interpreter's return code (e.g. TCL_OK or TCL_ERROR) as
	 * a new object onto the stack. */
    {0}
};

/*
 * The following table assigns a type to each character. Only types
 * meaningful to Tcl parsing are represented here. The table is
 * designed to be referenced with either signed or unsigned characters,
 * so it has 384 entries. The first 128 entries correspond to negative
 * character values, the next 256 correspond to positive character
 * values. The last 128 entries are identical to the first 128. The
 * table is always indexed with a 128-byte offset (the 128th entry
 * corresponds to a 0 character value).
 */

unsigned char tclTypeTable[] = {
    /*
     * Negative character values, from -128 to -1:
     */

    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,

    /*
     * Positive character values, from 0-127:
     */

    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_SPACE,         TCL_COMMAND_END,   TCL_SPACE,
    TCL_SPACE,         TCL_SPACE,         TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_SPACE,         TCL_NORMAL,        TCL_QUOTE,         TCL_NORMAL,
    TCL_DOLLAR,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_COMMAND_END,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_OPEN_BRACKET,
    TCL_BACKSLASH,     TCL_COMMAND_END,   TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_OPEN_BRACE,
    TCL_NORMAL,        TCL_CLOSE_BRACE,   TCL_NORMAL,        TCL_NORMAL,

    /*
     * Large unsigned character values, from 128-255:
     */

    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
    TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,        TCL_NORMAL,
};

/*
 * Prototypes for procedures defined later in this file:
 */

static void		AdvanceToNextWord _ANSI_ARGS_((char *string,
			    CompileEnv *envPtr));
static int		CollectArgInfo _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *lastChar, int flags,
			    ArgInfo *argInfoPtr));
static int		CompileBraces _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *lastChar, int flags,
			    CompileEnv *envPtr));
static int		CompileCmdWordInline _ANSI_ARGS_((
    			    Tcl_Interp *interp, char *string,
			    char *lastChar, int flags, CompileEnv *envPtr));
static int		CompileExprWord _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *lastChar, int flags, 
			    CompileEnv *envPtr));
static int		CompileMultipartWord _ANSI_ARGS_((
    			    Tcl_Interp *interp, char *string,
			    char *lastChar, int flags, CompileEnv *envPtr));
static int		CompileWord _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, char *lastChar, int flags, 
			    CompileEnv *envPtr));
static int		CreateExceptionRange _ANSI_ARGS_((
			    ExceptionRangeType type, CompileEnv *envPtr));
static void		DupByteCodeInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static ClientData	DupForeachInfo _ANSI_ARGS_((ClientData clientData));
static unsigned char *	EncodeCmdLocMap _ANSI_ARGS_((
			    CompileEnv *envPtr, ByteCode *codePtr,
			    unsigned char *startPtr));
static void		EnterCmdExtentData _ANSI_ARGS_((
    			    CompileEnv *envPtr, int cmdNumber,
			    int numSrcChars, int numCodeBytes));
static void		EnterCmdStartData _ANSI_ARGS_((
    			    CompileEnv *envPtr, int cmdNumber,
			    int srcOffset, int codeOffset));
static void		ExpandObjectArray _ANSI_ARGS_((CompileEnv *envPtr));
static void		FreeForeachInfo _ANSI_ARGS_((
			    ClientData clientData));
static void		FreeByteCodeInternalRep _ANSI_ARGS_((
    			    Tcl_Obj *objPtr));
static void		FreeArgInfo _ANSI_ARGS_((ArgInfo *argInfoPtr));
static int		GetCmdLocEncodingSize _ANSI_ARGS_((
			    CompileEnv *envPtr));
static void		InitArgInfo _ANSI_ARGS_((ArgInfo *argInfoPtr));
static int		LookupCompiledLocal _ANSI_ARGS_((
        		    char *name, int nameChars, int createIfNew,
			    int flagsIfCreated, Proc *procPtr));
static int		SetByteCodeFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static void		UpdateStringOfByteCode _ANSI_ARGS_((Tcl_Obj *objPtr));

/*
 * The structure below defines the bytecode Tcl object type by
 * means of procedures that can be invoked by generic object code.
 */

Tcl_ObjType tclByteCodeType = {
    "bytecode",			/* name */
    FreeByteCodeInternalRep,	/* freeIntRepProc */
    DupByteCodeInternalRep,	/* dupIntRepProc */
    UpdateStringOfByteCode,	/* updateStringProc */
    SetByteCodeFromAny		/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * TclPrintByteCodeObj --
 *
 *	This procedure prints ("disassembles") the instructions of a
 *	bytecode object to stdout.
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
TclPrintByteCodeObj(interp, objPtr)
    Tcl_Interp *interp;		/* Used only for Tcl_GetStringFromObj. */
    Tcl_Obj *objPtr;		/* The bytecode object to disassemble. */
{
    ByteCode* codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
    unsigned char *codeStart, *codeLimit, *pc;
    unsigned char *codeDeltaNext, *codeLengthNext;
    unsigned char *srcDeltaNext, *srcLengthNext;
    int codeOffset, codeLen, srcOffset, srcLen;
    int numCmds, numObjs, delta, objBytes, i;

    if (codePtr->refCount <= 0) {
	return;			/* already freed */
    }

    codeStart = codePtr->codeStart;
    codeLimit = (codeStart + codePtr->numCodeBytes);
    numCmds = codePtr->numCommands;
    numObjs = codePtr->numObjects;

    objBytes = (numObjs * sizeof(Tcl_Obj));
    for (i = 0;  i < numObjs;  i++) {
	Tcl_Obj *litObjPtr = codePtr->objArrayPtr[i];
	if (litObjPtr->bytes != NULL) {
	    objBytes += litObjPtr->length;
	}
    }

    /*
     * Print header lines describing the ByteCode.
     */

    fprintf(stdout, "\nByteCode 0x%x, ref ct %u, epoch %u, interp 0x%x(epoch %u)\n",
	    (unsigned int) codePtr, codePtr->refCount,
	    codePtr->compileEpoch, (unsigned int) codePtr->iPtr,
	    codePtr->iPtr->compileEpoch);
    fprintf(stdout, "  Source ");
    TclPrintSource(stdout, codePtr->source,
	    TclMin(codePtr->numSrcChars, 70));
    fprintf(stdout, "\n  Cmds %d, chars %d, inst %d, objs %u, aux %d, stk depth %u, code/src %.2f\n",
	    numCmds, codePtr->numSrcChars, codePtr->numCodeBytes, numObjs,
	    codePtr->numAuxDataItems, codePtr->maxStackDepth,
	    (codePtr->numSrcChars?
	            ((float)codePtr->totalSize)/((float)codePtr->numSrcChars) : 0.0));
    fprintf(stdout, "  Code %d = %d(header)+%d(inst)+%d(objs)+%d(exc)+%d(aux)+%d(cmd map)\n",
	    codePtr->totalSize, sizeof(ByteCode), codePtr->numCodeBytes,
	    objBytes, (codePtr->numExcRanges * sizeof(ExceptionRange)),
	    (codePtr->numAuxDataItems * sizeof(AuxData)),
	    codePtr->numCmdLocBytes);

    /*
     * If the ByteCode is the compiled body of a Tcl procedure, print
     * information about that procedure. Note that we don't know the
     * procedure's name since ByteCode's can be shared among procedures.
     */
    
    if (codePtr->procPtr != NULL) {
	Proc *procPtr = codePtr->procPtr;
	int numCompiledLocals = procPtr->numCompiledLocals;
	fprintf(stdout,
	        "  Proc 0x%x, ref ct %d, args %d, compiled locals %d\n",
		(unsigned int) procPtr, procPtr->refCount, procPtr->numArgs,
		numCompiledLocals);
	if (numCompiledLocals > 0) {
	    CompiledLocal *localPtr = procPtr->firstLocalPtr;
	    for (i = 0;  i < numCompiledLocals;  i++) {
		fprintf(stdout, "      %d: slot %d%s%s%s%s%s",
			i, localPtr->frameIndex,
			((localPtr->flags & VAR_SCALAR)?  ", scalar"  : ""),
			((localPtr->flags & VAR_ARRAY)?  ", array"  : ""),
			((localPtr->flags & VAR_LINK)?  ", link"  : ""),
			(localPtr->isArg?  ", arg"  : ""),
			(localPtr->isTemp? ", temp" : ""));
		if (localPtr->isTemp) {
		    fprintf(stdout,	"\n");
		} else {
		    fprintf(stdout,	", name=\"%s\"\n", localPtr->name);
		}
		localPtr = localPtr->nextPtr;
	    }
	}
    }

    /*
     * Print the ExceptionRange array.
     */

    if (codePtr->numExcRanges > 0) {
	fprintf(stdout, "  Exception ranges %d, depth %d:\n",
	        codePtr->numExcRanges, codePtr->maxExcRangeDepth);
	for (i = 0;  i < codePtr->numExcRanges;  i++) {
	    ExceptionRange *rangePtr = &(codePtr->excRangeArrayPtr[i]);
	    fprintf(stdout, "      %d: level %d, %s, pc %d-%d, ",
		    i, rangePtr->nestingLevel,
		    ((rangePtr->type == LOOP_EXCEPTION_RANGE)? "loop":"catch"),
		    rangePtr->codeOffset,
		    (rangePtr->codeOffset + rangePtr->numCodeBytes - 1));
	    switch (rangePtr->type) {
	    case LOOP_EXCEPTION_RANGE:
		fprintf(stdout,	"continue %d, break %d\n",
		        rangePtr->continueOffset, rangePtr->breakOffset);
		break;
	    case CATCH_EXCEPTION_RANGE:
		fprintf(stdout,	"catch %d\n", rangePtr->catchOffset);
		break;
	    default:
		panic("TclPrintSource: unrecognized ExceptionRange type %d\n",
		        rangePtr->type);
	    }
	}
    }
    
    /*
     * If there were no commands (e.g., an expression or an empty string
     * was compiled), just print all instructions and return.
     */

    if (numCmds == 0) {
	pc = codeStart;
	while (pc < codeLimit) {
	    fprintf(stdout, "    ");
	    pc += TclPrintInstruction(codePtr, pc);
	}
	return;
    }
    
    /*
     * Print table showing the code offset, source offset, and source
     * length for each command. These are encoded as a sequence of bytes.
     */

    fprintf(stdout, "  Commands %d:", numCmds);
    codeDeltaNext = codePtr->codeDeltaStart;
    codeLengthNext = codePtr->codeLengthStart;
    srcDeltaNext  = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	if ((unsigned int) (*codeDeltaNext) == (unsigned int) 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if ((unsigned int) (*codeLengthNext) == (unsigned int) 0xFF) {
	    codeLengthNext++;
	    codeLen = TclGetInt4AtPtr(codeLengthNext);
	    codeLengthNext += 4;
	} else {
	    codeLen = TclGetInt1AtPtr(codeLengthNext);
	    codeLengthNext++;
	}
	
	if ((unsigned int) (*srcDeltaNext) == (unsigned int) 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if ((unsigned int) (*srcLengthNext) == (unsigned int) 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}
	
	fprintf(stdout,	"%s%4d: pc %d-%d, source %d-%d",
		((i % 2)? "	" : "\n   "),
		(i+1), codeOffset, (codeOffset + codeLen - 1),
		srcOffset, (srcOffset + srcLen - 1));
    }
    if ((numCmds > 0) && ((numCmds % 2) != 0)) {
	fprintf(stdout,	"\n");
    }
    
    /*
     * Print each instruction. If the instruction corresponds to the start
     * of a command, print the command's source. Note that we don't need
     * the code length here.
     */

    codeDeltaNext = codePtr->codeDeltaStart;
    srcDeltaNext  = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    pc = codeStart;
    for (i = 0;  i < numCmds;  i++) {
	if ((unsigned int) (*codeDeltaNext) == (unsigned int) 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if ((unsigned int) (*srcDeltaNext) == (unsigned int) 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if ((unsigned int) (*srcLengthNext) == (unsigned int) 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}

	/*
	 * Print instructions before command i.
	 */
	
	while ((pc-codeStart) < codeOffset) {
	    fprintf(stdout, "    ");
	    pc += TclPrintInstruction(codePtr, pc);
	}

	fprintf(stdout, "  Command %d: ", (i+1));
	TclPrintSource(stdout, (codePtr->source + srcOffset),
	        TclMin(srcLen, 70));
	fprintf(stdout, "\n");
    }
    if (pc < codeLimit) {
	/*
	 * Print instructions after the last command.
	 */

	while (pc < codeLimit) {
	    fprintf(stdout, "    ");
	    pc += TclPrintInstruction(codePtr, pc);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclPrintInstruction --
 *
 *	This procedure prints ("disassembles") one instruction from a
 *	bytecode object to stdout.
 *
 * Results:
 *	Returns the length in bytes of the current instruiction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclPrintInstruction(codePtr, pc)
    ByteCode* codePtr;		/* Bytecode containing the instruction. */
    unsigned char *pc;		/* Points to first byte of instruction. */
{
    Proc *procPtr = codePtr->procPtr;
    unsigned char opCode = *pc;
    register InstructionDesc *instDesc = &instructionTable[opCode];
    unsigned char *codeStart = codePtr->codeStart;
    unsigned int pcOffset = (pc - codeStart);
    int opnd, elemLen, i, j;
    Tcl_Obj *elemPtr;
    char *string;
    
    fprintf(stdout, "(%u) %s ", pcOffset, instDesc->name);
    for (i = 0;  i < instDesc->numOperands;  i++) {
	switch (instDesc->opTypes[i]) {
	case OPERAND_INT1:
	    opnd = TclGetInt1AtPtr(pc+1+i);
	    if ((i == 0) && ((opCode == INST_JUMP1)
			     || (opCode == INST_JUMP_TRUE1)
		             || (opCode == INST_JUMP_FALSE1))) {
		fprintf(stdout, "%d  	# pc %u", opnd, (pcOffset + opnd));
	    } else {
		fprintf(stdout, "%d", opnd);
	    }
	    break;
	case OPERAND_INT4:
	    opnd = TclGetInt4AtPtr(pc+1+i);
	    if ((i == 0) && ((opCode == INST_JUMP4)
			     || (opCode == INST_JUMP_TRUE4)
		             || (opCode == INST_JUMP_FALSE4))) {
		fprintf(stdout, "%d  	# pc %u", opnd, (pcOffset + opnd));
	    } else {
		fprintf(stdout, "%d", opnd);
	    }
	    break;
	case OPERAND_UINT1:
	    opnd = TclGetUInt1AtPtr(pc+1+i);
	    if ((i == 0) && (opCode == INST_PUSH1)) {
		elemPtr = codePtr->objArrayPtr[opnd];
		string = Tcl_GetStringFromObj(elemPtr, &elemLen);
		fprintf(stdout, "%u  	# ", (unsigned int) opnd);
		TclPrintSource(stdout, string, TclMin(elemLen, 40));
	    } else if ((i == 0) && ((opCode == INST_LOAD_SCALAR1)
				    || (opCode == INST_LOAD_ARRAY1)
				    || (opCode == INST_STORE_SCALAR1)
				    || (opCode == INST_STORE_ARRAY1))) {
		int localCt = procPtr->numCompiledLocals;
		CompiledLocal *localPtr = procPtr->firstLocalPtr;
		if (opnd >= localCt) {
		    panic("TclPrintInstruction: bad local var index %u (%u locals)\n",
			     (unsigned int) opnd, localCt);
		    return instDesc->numBytes;
		}
		for (j = 0;  j < opnd;  j++) {
		    localPtr = localPtr->nextPtr;
		}
		if (localPtr->isTemp) {
		    fprintf(stdout, "%u	# temp var %u",
			    (unsigned int) opnd, (unsigned int) opnd);
		} else {
		    fprintf(stdout, "%u	# var ", (unsigned int) opnd);
		    TclPrintSource(stdout, localPtr->name, 40);
		}
	    } else {
		fprintf(stdout, "%u ", (unsigned int) opnd);
	    }
	    break;
	case OPERAND_UINT4:
	    opnd = TclGetUInt4AtPtr(pc+1+i);
	    if (opCode == INST_PUSH4) {
		elemPtr = codePtr->objArrayPtr[opnd];
		string = Tcl_GetStringFromObj(elemPtr, &elemLen);
		fprintf(stdout, "%u  	# ", opnd);
		TclPrintSource(stdout, string, TclMin(elemLen, 40));
	    } else if ((i == 0) && ((opCode == INST_LOAD_SCALAR4)
				    || (opCode == INST_LOAD_ARRAY4)
				    || (opCode == INST_STORE_SCALAR4)
				    || (opCode == INST_STORE_ARRAY4))) {
		int localCt = procPtr->numCompiledLocals;
		CompiledLocal *localPtr = procPtr->firstLocalPtr;
		if (opnd >= localCt) {
		    panic("TclPrintInstruction: bad local var index %u (%u locals)\n",
			     (unsigned int) opnd, localCt);
		    return instDesc->numBytes;
		}
		for (j = 0;  j < opnd;  j++) {
		    localPtr = localPtr->nextPtr;
		}
		if (localPtr->isTemp) {
		    fprintf(stdout, "%u	# temp var %u",
			    (unsigned int) opnd, (unsigned int) opnd);
		} else {
		    fprintf(stdout, "%u	# var ", (unsigned int) opnd);
		    TclPrintSource(stdout, localPtr->name, 40);
		}
	    } else {
		fprintf(stdout, "%u ", (unsigned int) opnd);
	    }
	    break;
	case OPERAND_NONE:
	default:
	    break;
	}
    }
    fprintf(stdout, "\n");
    return instDesc->numBytes;
}

/*
 *----------------------------------------------------------------------
 *
 * TclPrintSource --
 *
 *	This procedure prints up to a specified number of characters from
 *	the argument string to a specified file. It tries to produce legible
 *	output by adding backslashes as necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Outputs characters to the specified file.
 *
 *----------------------------------------------------------------------
 */

void
TclPrintSource(outFile, string, maxChars)
    FILE *outFile;		/* The file to print the source to. */
    char *string;		/* The string to print. */
    int maxChars;		/* Maximum number of chars to print. */
{
    register char *p;
    register int i = 0;

    if (string == NULL) {
	fprintf(outFile, "\"\"");
	return;
    }

    fprintf(outFile, "\"");
    p = string;
    for (;  (*p != '\0') && (i < maxChars);  p++, i++) {
	switch (*p) {
	    case '"':
		fprintf(outFile, "\\\"");
		continue;
	    case '\f':
		fprintf(outFile, "\\f");
		continue;
	    case '\n':
		fprintf(outFile, "\\n");
		continue;
            case '\r':
		fprintf(outFile, "\\r");
		continue;
	    case '\t':
		fprintf(outFile, "\\t");
		continue;
            case '\v':
		fprintf(outFile, "\\v");
		continue;
	    default:
		fprintf(outFile, "%c", *p);
		continue;
	}
    }
    fprintf(outFile, "\"");
}

/*
 *----------------------------------------------------------------------
 *
 * FreeByteCodeInternalRep --
 *
 *	Part of the bytecode Tcl object type implementation. Frees the
 *	storage associated with a bytecode object's internal representation
 *	unless its code is actively being executed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The bytecode object's internal rep is marked invalid and its
 *	code gets freed unless the code is actively being executed.
 *	In that case the cleanup is delayed until the last execution
 *	of the code completes.
 *
 *----------------------------------------------------------------------
 */

static void
FreeByteCodeInternalRep(objPtr)
    register Tcl_Obj *objPtr;	/* Object whose internal rep to free. */
{
    register ByteCode *codePtr =
	    (ByteCode *) objPtr->internalRep.otherValuePtr;

    codePtr->refCount--;
    if (codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
    }
    objPtr->typePtr = NULL;
    objPtr->internalRep.otherValuePtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupByteCode --
 *
 *	This procedure does all the real work of freeing up a bytecode
 *	object's ByteCode structure. It's called only when the structure's
 *	reference count becomes zero.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees objPtr's bytecode internal representation and sets
 *	its type and objPtr->internalRep.otherValuePtr NULL. Also
 *	decrements the ref counts on each object in its object array,
 *	and frees its auxiliary data items.
 *
 *----------------------------------------------------------------------
 */

void
TclCleanupByteCode(codePtr)
    ByteCode *codePtr;		/* ByteCode to free. */
{
    Tcl_Obj **objArrayPtr = codePtr->objArrayPtr;
    int numObjects = codePtr->numObjects;
    int numAuxDataItems = codePtr->numAuxDataItems;
    register AuxData *auxDataPtr;
    register Tcl_Obj *elemPtr;
    register int i;

#ifdef TCL_COMPILE_STATS    
    tclCurrentSourceBytes -= (double) codePtr->numSrcChars;
    tclCurrentCodeBytes -= (double) codePtr->totalSize;
#endif /* TCL_COMPILE_STATS */

    /*
     * A single heap object holds the ByteCode structure and its code,
     * object, command location, and auxiliary data arrays. This means we
     * only need to 1) decrement the ref counts on the objects in its
     * object array, 2) call the free procs for the auxiliary data items,
     * and 3) free the ByteCode structure's heap object.
     */

    for (i = 0;  i < numObjects;  i++) {
	elemPtr = objArrayPtr[i];
	TclDecrRefCount(elemPtr);
    }

    auxDataPtr = codePtr->auxDataArrayPtr;
    for (i = 0;  i < numAuxDataItems;  i++) {
	if (auxDataPtr->freeProc != NULL) {
	    auxDataPtr->freeProc(auxDataPtr->clientData);
	}
	auxDataPtr++;
    }
    
    ckfree((char *) codePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DupByteCodeInternalRep --
 *
 *	Part of the bytecode Tcl object type implementation. However, it
 *	does not copy the internal representation of a bytecode Tcl_Obj, but
 *	instead leaves the new object untyped (with a NULL type pointer).
 *	Code will be compiled for the new object only if necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DupByteCodeInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    return;
}

/*
 *-----------------------------------------------------------------------
 *
 * SetByteCodeFromAny --
 *
 *	Part of the bytecode Tcl object type implementation. Attempts to
 *	generate an byte code internal form for the Tcl object "objPtr" by
 *	compiling its string representation.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during compilation, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	Frees the old internal representation. If no error occurs, then the
 *	compiled code is stored as "objPtr"s bytecode representation.
 *	Also, if debugging, initializes the "tcl_traceCompile" Tcl variable
 *	used to trace compilations.
 *
 *----------------------------------------------------------------------
 */

static int
SetByteCodeFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* The interpreter for which the code is
				 * compiled. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Interp *iPtr = (Interp *) interp;
    char *string;
    CompileEnv compEnv;		/* Compilation environment structure
				 * allocated in frame. */
    AuxData *auxDataPtr;
    register int i;
    int length, result;

    if (!traceInitialized) {
        if (Tcl_LinkVar(interp, "tcl_traceCompile",
	            (char *) &tclTraceCompile,  TCL_LINK_INT) != TCL_OK) {
            panic("SetByteCodeFromAny: unable to create link for tcl_traceCompile variable");
        }
        traceInitialized = 1;
    }
    
    string = Tcl_GetStringFromObj(objPtr, &length);
    TclInitCompileEnv(interp, &compEnv, string);
    result = TclCompileString(interp, string, string+length,
	    iPtr->evalFlags, &compEnv);
    if (result == TCL_OK) {
	/*
	 * Add a "done" instruction at the end of the instruction sequence.
	 */
    
	TclEmitOpcode(INST_DONE, &compEnv);
	
	/*
	 * Convert the object to a ByteCode object.
	 */

	TclInitByteCodeObj(objPtr, &compEnv);
    } else {
	/*
	 * Compilation errors. Decrement the ref counts on any objects in
	 * the object array and free any aux data items prior to freeing
	 * the compilation environment.
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
    }
    TclFreeCompileEnv(&compEnv);

    if (result == TCL_OK) {
	if (tclTraceCompile == 2) {
	    TclPrintByteCodeObj(interp, objPtr);
	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfByteCode --
 *
 *	Part of the bytecode Tcl object type implementation. Called to
 *	update the string representation for a byte code object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Generates a panic. 
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfByteCode(objPtr)
    register Tcl_Obj *objPtr;	/* ByteCode object with string rep that 
				 * needs updating. */
{
    /*
     * This procedure is never invoked since the internal representation of
     * a bytecode object is never modified.
     */

    panic("UpdateStringOfByteCode should never be called.");
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitCompileEnv --
 *
 *	Initializes a CompileEnv compilation environment structure for the
 *	compilation of a string in an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The CompileEnv structure is initialized.
 *
 *----------------------------------------------------------------------
 */

void
TclInitCompileEnv(interp, envPtr, string)
    Tcl_Interp *interp;		 /* The interpreter for which a CompileEnv
				  * structure is initialized. */
    register CompileEnv *envPtr; /* Points to the CompileEnv structure to
				  * initialize. */
    char *string;		 /* The source string to be compiled. */
{
    Interp *iPtr = (Interp *) interp;
    
    envPtr->iPtr = iPtr;
    envPtr->source = string;
    envPtr->procPtr = iPtr->compiledProcPtr;
    envPtr->numCommands = 0;
    envPtr->excRangeDepth = 0;
    envPtr->maxExcRangeDepth = 0;
    envPtr->maxStackDepth = 0;
    Tcl_InitHashTable(&(envPtr->objTable), TCL_STRING_KEYS);
    envPtr->pushSimpleWords = 1;
    envPtr->wordIsSimple = 0;
    envPtr->numSimpleWordChars = 0;
    envPtr->exprIsJustVarRef = 0;
    envPtr->exprIsComparison = 0;
    envPtr->termOffset = 0;

    envPtr->codeStart = envPtr->staticCodeSpace;
    envPtr->codeNext = envPtr->codeStart;
    envPtr->codeEnd = (envPtr->codeStart + COMPILEENV_INIT_CODE_BYTES);
    envPtr->mallocedCodeArray = 0;

    envPtr->objArrayPtr = envPtr->staticObjArraySpace;
    envPtr->objArrayNext = 0;
    envPtr->objArrayEnd = COMPILEENV_INIT_NUM_OBJECTS;
    envPtr->mallocedObjArray = 0;
    
    envPtr->excRangeArrayPtr = envPtr->staticExcRangeArraySpace;
    envPtr->excRangeArrayNext = 0;
    envPtr->excRangeArrayEnd = COMPILEENV_INIT_EXCEPT_RANGES;
    envPtr->mallocedExcRangeArray = 0;
    
    envPtr->cmdMapPtr = envPtr->staticCmdMapSpace;
    envPtr->cmdMapEnd = COMPILEENV_INIT_CMD_MAP_SIZE;
    envPtr->mallocedCmdMap = 0;
    
    envPtr->auxDataArrayPtr = envPtr->staticAuxDataArraySpace;
    envPtr->auxDataArrayNext = 0;
    envPtr->auxDataArrayEnd = COMPILEENV_INIT_AUX_DATA_SIZE;
    envPtr->mallocedAuxDataArray = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreeCompileEnv --
 *
 *	Free the storage allocated in a CompileEnv compilation environment
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocated storage in the CompileEnv structure is freed. Note that
 *	ref counts for Tcl objects in its object table are not decremented.
 *	In addition, any storage referenced by any auxiliary data items
 *	in the CompileEnv structure are not freed either. The expectation
 *	is that when compilation is successful, "ownership" (i.e., the
 *	pointers to) these objects and aux data items will just be handed
 *	over to the corresponding ByteCode structure.
 *
 *----------------------------------------------------------------------
 */

void
TclFreeCompileEnv(envPtr)
    register CompileEnv *envPtr; /* Points to the CompileEnv structure. */
{
    Tcl_DeleteHashTable(&(envPtr->objTable));
    if (envPtr->mallocedCodeArray) {
	ckfree((char *) envPtr->codeStart);
    }
    if (envPtr->mallocedObjArray) {
	ckfree((char *) envPtr->objArrayPtr);
    }
    if (envPtr->mallocedExcRangeArray) {
	ckfree((char *) envPtr->excRangeArrayPtr);
    }
    if (envPtr->mallocedCmdMap) {
	ckfree((char *) envPtr->cmdMapPtr);
    }
    if (envPtr->mallocedAuxDataArray) {
	ckfree((char *) envPtr->auxDataArrayPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitByteCodeObj --
 *
 *	Create a ByteCode structure and initialize it from a CompileEnv
 *	compilation environment structure. The ByteCode structure is
 *	smaller and contains just that information needed to execute
 *	the bytecode instructions resulting from compiling a Tcl script.
 *	The resulting structure is placed in the specified object.
 *
 * Results:
 *	A newly constructed ByteCode object is stored in the internal
 *	representation of the objPtr.
 *
 * Side effects:
 *	A single heap object is allocated to hold the new ByteCode structure
 *	and its code, object, command location, and aux data arrays. Note
 *	that "ownership" (i.e., the pointers to) the Tcl objects and aux
 *	data items will be handed over to the new ByteCode structure from
 *	the CompileEnv structure.
 *
 *----------------------------------------------------------------------
 */

void
TclInitByteCodeObj(objPtr, envPtr)
    Tcl_Obj *objPtr;		 /* Points object that should be
				  * initialized, and whose string rep
				  * contains the source code. */
    register CompileEnv *envPtr; /* Points to the CompileEnv structure from
				  * which to create a ByteCode structure. */
{
    register ByteCode *codePtr;
    size_t codeBytes, objArrayBytes, exceptArrayBytes, cmdLocBytes;
    size_t auxDataArrayBytes;
    register size_t size, objBytes, totalSize;
    register unsigned char *p;
    unsigned char *nextPtr;
    int srcLen = envPtr->termOffset;
    int numObjects, i;
#ifdef TCL_COMPILE_STATS
    int srcLenLog2, sizeLog2;
#endif /*TCL_COMPILE_STATS*/

    codeBytes = (envPtr->codeNext - envPtr->codeStart);
    numObjects = envPtr->objArrayNext;
    objArrayBytes = (envPtr->objArrayNext * sizeof(Tcl_Obj *));
    exceptArrayBytes = (envPtr->excRangeArrayNext * sizeof(ExceptionRange));
    auxDataArrayBytes = (envPtr->auxDataArrayNext * sizeof(AuxData));
    cmdLocBytes = GetCmdLocEncodingSize(envPtr);
    
    size = sizeof(ByteCode);
    size += TCL_ALIGN(codeBytes);       /* align object array */
    size += TCL_ALIGN(objArrayBytes);   /* align exception range array */
    size += TCL_ALIGN(exceptArrayBytes); /* align AuxData array */
    size += auxDataArrayBytes;
    size += cmdLocBytes;

    /*
     * Compute the total number of bytes needed for this bytecode
     * including the storage for the Tcl objects in its object array.
     */

    objBytes = (numObjects * sizeof(Tcl_Obj));
    for (i = 0;  i < numObjects;  i++) {
	Tcl_Obj *litObjPtr = envPtr->objArrayPtr[i];
	if (litObjPtr->bytes != NULL) {
	    objBytes += litObjPtr->length;
	}
    }
    totalSize = (size + objBytes);

#ifdef TCL_COMPILE_STATS
    tclNumCompilations++;
    tclTotalSourceBytes += (double) srcLen;
    tclTotalCodeBytes += (double) totalSize;
    
    tclTotalInstBytes += (double) codeBytes;
    tclTotalObjBytes += (double) objBytes;
    tclTotalExceptBytes += exceptArrayBytes;
    tclTotalAuxBytes += (double) auxDataArrayBytes;
    tclTotalCmdMapBytes += (double) cmdLocBytes;

    tclCurrentSourceBytes += (double) srcLen;
    tclCurrentCodeBytes += (double) totalSize;

    srcLenLog2 = TclLog2(srcLen);
    sizeLog2 = TclLog2((int) totalSize);
    if ((srcLenLog2 > 31) || (sizeLog2 > 31)) {
	panic("TclInitByteCodeObj: bad source or code sizes\n");
    }
    tclSourceCount[srcLenLog2]++;
    tclByteCodeCount[sizeLog2]++;
#endif /* TCL_COMPILE_STATS */    
    
    p = (unsigned char *) ckalloc(size);
    codePtr = (ByteCode *) p;
    codePtr->iPtr = envPtr->iPtr;
    codePtr->compileEpoch = envPtr->iPtr->compileEpoch;
    codePtr->refCount = 1;
    codePtr->source = envPtr->source;
    codePtr->procPtr = envPtr->procPtr;
    codePtr->totalSize = totalSize;
    codePtr->numCommands = envPtr->numCommands;
    codePtr->numSrcChars = srcLen;
    codePtr->numCodeBytes = codeBytes;
    codePtr->numObjects = numObjects;
    codePtr->numExcRanges = envPtr->excRangeArrayNext;
    codePtr->numAuxDataItems = envPtr->auxDataArrayNext;
    codePtr->auxDataArrayPtr = NULL;
    codePtr->numCmdLocBytes = cmdLocBytes;
    codePtr->maxExcRangeDepth = envPtr->maxExcRangeDepth;
    codePtr->maxStackDepth = envPtr->maxStackDepth;
    
    p += sizeof(ByteCode);
    codePtr->codeStart = p;
    memcpy((VOID *) p, (VOID *) envPtr->codeStart, codeBytes);
    
    p += TCL_ALIGN(codeBytes);	      /* align object array */
    codePtr->objArrayPtr = (Tcl_Obj **) p;
    memcpy((VOID *) p, (VOID *) envPtr->objArrayPtr, objArrayBytes);

    p += TCL_ALIGN(objArrayBytes);    /* align exception range array */
    if (exceptArrayBytes > 0) {
	codePtr->excRangeArrayPtr = (ExceptionRange *) p;
	memcpy((VOID *) p, (VOID *) envPtr->excRangeArrayPtr,
	        exceptArrayBytes);
    }
    
    p += TCL_ALIGN(exceptArrayBytes); /* align AuxData array */
    if (auxDataArrayBytes > 0) {
	codePtr->auxDataArrayPtr = (AuxData *) p;
	memcpy((VOID *) p, (VOID *) envPtr->auxDataArrayPtr,
	        auxDataArrayBytes);
    }

    p += auxDataArrayBytes;
    nextPtr = EncodeCmdLocMap(envPtr, codePtr, (unsigned char *) p);
    if (((size_t)(nextPtr - p)) != cmdLocBytes) {	
	panic("TclInitByteCodeObj: encoded cmd location bytes %d != expected size %d\n", (nextPtr - p), cmdLocBytes);
    }
    
    /*
     * Free the old internal rep then convert the object to a
     * bytecode object by making its internal rep point to the just
     * compiled ByteCode.
     */
	    
    if ((objPtr->typePtr != NULL) &&
	    (objPtr->typePtr->freeIntRepProc != NULL)) {
	objPtr->typePtr->freeIntRepProc(objPtr);
    }
    objPtr->internalRep.otherValuePtr = (VOID *) codePtr;
    objPtr->typePtr = &tclByteCodeType;
}

/*
 *----------------------------------------------------------------------
 *
 * GetCmdLocEncodingSize --
 *
 *	Computes the total number of bytes needed to encode the command
 *	location information for some compiled code.
 *
 * Results:
 *	The byte count needed to encode the compiled location information.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetCmdLocEncodingSize(envPtr)
     CompileEnv *envPtr;	/* Points to compilation environment
				 * structure containing the CmdLocation
				 * structure to encode. */
{
    register CmdLocation *mapPtr = envPtr->cmdMapPtr;
    int numCmds = envPtr->numCommands;
    int codeDelta, codeLen, srcDelta, srcLen;
    int codeDeltaNext, codeLengthNext, srcDeltaNext, srcLengthNext;
				/* The offsets in their respective byte
				 * sequences where the next encoded offset
				 * or length should go. */
    int prevCodeOffset, prevSrcOffset, i;

    codeDeltaNext = codeLengthNext = srcDeltaNext = srcLengthNext = 0;
    prevCodeOffset = prevSrcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	codeDelta = (mapPtr[i].codeOffset - prevCodeOffset);
	if (codeDelta < 0) {
	    panic("GetCmdLocEncodingSize: bad code offset");
	} else if (codeDelta <= 127) {
	    codeDeltaNext++;
	} else {
	    codeDeltaNext += 5;	 /* 1 byte for 0xFF, 4 for positive delta */
	}
	prevCodeOffset = mapPtr[i].codeOffset;

	codeLen = mapPtr[i].numCodeBytes;
	if (codeLen < 0) {
	    panic("GetCmdLocEncodingSize: bad code length");
	} else if (codeLen <= 127) {
	    codeLengthNext++;
	} else {
	    codeLengthNext += 5; /* 1 byte for 0xFF, 4 for length */
	}

	srcDelta = (mapPtr[i].srcOffset - prevSrcOffset);
	if ((-127 <= srcDelta) && (srcDelta <= 127)) {
	    srcDeltaNext++;
	} else {
	    srcDeltaNext += 5;	 /* 1 byte for 0xFF, 4 for delta */
	}
	prevSrcOffset = mapPtr[i].srcOffset;

	srcLen = mapPtr[i].numSrcChars;
	if (srcLen < 0) {
	    panic("GetCmdLocEncodingSize: bad source length");
	} else if (srcLen <= 127) {
	    srcLengthNext++;
	} else {
	    srcLengthNext += 5;	 /* 1 byte for 0xFF, 4 for length */
	}
    }

    return (codeDeltaNext + codeLengthNext + srcDeltaNext + srcLengthNext);
}

/*
 *----------------------------------------------------------------------
 *
 * EncodeCmdLocMap --
 *
 *	Encode the command location information for some compiled code into
 *	a ByteCode structure. The encoded command location map is stored as
 *	three adjacent byte sequences.
 *
 * Results:
 *	Pointer to the first byte after the encoded command location
 *	information.
 *
 * Side effects:
 *	The encoded information is stored into the block of memory headed
 *	by codePtr. Also records pointers to the start of the four byte
 *	sequences in fields in codePtr's ByteCode header structure.
 *
 *----------------------------------------------------------------------
 */

static unsigned char *
EncodeCmdLocMap(envPtr, codePtr, startPtr)
     CompileEnv *envPtr;	/* Points to compilation environment
				 * structure containing the CmdLocation
				 * structure to encode. */
     ByteCode *codePtr;		/* ByteCode in which to encode envPtr's
				 * command location information. */
     unsigned char *startPtr;	/* Points to the first byte in codePtr's
				 * memory block where the location
				 * information is to be stored. */
{
    register CmdLocation *mapPtr = envPtr->cmdMapPtr;
    int numCmds = envPtr->numCommands;
    register unsigned char *p = startPtr;
    int codeDelta, codeLen, srcDelta, srcLen, prevOffset;
    register int i;
    
    /*
     * Encode the code offset for each command as a sequence of deltas.
     */

    codePtr->codeDeltaStart = p;
    prevOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	codeDelta = (mapPtr[i].codeOffset - prevOffset);
	if (codeDelta < 0) {
	    panic("EncodeCmdLocMap: bad code offset");
	} else if (codeDelta <= 127) {
	    TclStoreInt1AtPtr(codeDelta, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(codeDelta, p);
	    p += 4;
	}
	prevOffset = mapPtr[i].codeOffset;
    }

    /*
     * Encode the code length for each command.
     */

    codePtr->codeLengthStart = p;
    for (i = 0;  i < numCmds;  i++) {
	codeLen = mapPtr[i].numCodeBytes;
	if (codeLen < 0) {
	    panic("EncodeCmdLocMap: bad code length");
	} else if (codeLen <= 127) {
	    TclStoreInt1AtPtr(codeLen, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(codeLen, p);
	    p += 4;
	}
    }

    /*
     * Encode the source offset for each command as a sequence of deltas.
     */

    codePtr->srcDeltaStart = p;
    prevOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	srcDelta = (mapPtr[i].srcOffset - prevOffset);
	if ((-127 <= srcDelta) && (srcDelta <= 127)) {
	    TclStoreInt1AtPtr(srcDelta, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(srcDelta, p);
	    p += 4;
	}
	prevOffset = mapPtr[i].srcOffset;
    }

    /*
     * Encode the source length for each command.
     */

    codePtr->srcLengthStart = p;
    for (i = 0;  i < numCmds;  i++) {
	srcLen = mapPtr[i].numSrcChars;
	if (srcLen < 0) {
	    panic("EncodeCmdLocMap: bad source length");
	} else if (srcLen <= 127) {
	    TclStoreInt1AtPtr(srcLen, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(srcLen, p);
	    p += 4;
	}
    }
    
    return p;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileString --
 *
 *	Compile a Tcl script in a null-terminated binary string.
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->termOffset and interp->termOffset are filled in with the
 *	offset of the character in the string just after the last one
 *	successfully processed; this might be the offset of the ']' (if
 *	flags & TCL_BRACKET_TERM), or the offset of the '\0' at the end of
 *	the string. Also updates envPtr->maxStackDepth with the maximum
 *	number of stack elements needed to execute the string's commands.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the string at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileString(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Interp *iPtr = (Interp *) interp;
    register char *src = string;/* Points to current source char. */
    register char c = *src;	/* The current char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    char termChar = (char)((flags & TCL_BRACKET_TERM)? ']' : '\0');
				/* Return when this character is found
				 * (either ']' or '\0'). Zero means newlines
				 * terminate cmds. */
    int isFirstCmd = 1;		/* 1 if compiling the first cmd. */
    char *cmdSrcStart = NULL;	/* Points to first non-blank char in each
 				 * command. Initialized to avoid compiler
 				 * warning. */
    int cmdIndex;		/* The index of the current command in the
 				 * compilation environment's command
 				 * location table. */
    int lastTopLevelCmdIndex = -1;
    				/* Index of most recent toplevel command in
 				 * the command location table. Initialized
				 * to avoid compiler warning. */
    int cmdCodeOffset = -1;	/* Offset of first byte of current command's
 				 * code. Initialized to avoid compiler
 				 * warning. */
    int cmdWords;		/* Number of words in current command. */
    Tcl_Command cmd;		/* Used to search for commands. */
    Command *cmdPtr;		/* Points to command's Command structure if
				 * first word is simple and command was
				 * found; else NULL. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute all cmds. */
    char *termPtr;		/* Points to char that terminated word. */
    char savedChar;		/* Holds the character from string
				 * termporarily replaced by a null character
				 * during processing of words. */
    int objIndex = -1;		/* The object array index for a pushed
 				 * object holding a word or word part
 				 * Initialized to avoid compiler warning. */
    unsigned char *entryCodeNext = envPtr->codeNext;
    				/* Value of envPtr's current instruction
				 * pointer at entry. Used to tell if any
				 * instructions generated. */
    char *ellipsis = "";	/* Used to set errorInfo variable; "..."
				 * indicates that not all of offending
				 * command is included in errorInfo. ""
				 * means that the command is all there. */
    Tcl_Obj *objPtr;
    int numChars;
    int result = TCL_OK;
    int savePushSimpleWords = envPtr->pushSimpleWords;

    /*
     * commands: command {(';' | '\n') command}
     */

    while ((src != lastChar) && (c != termChar)) {
	/*
	 * Skip white space, semicolons, backslash-newlines (treated as
	 * spaces), and comments before command.
	 */

	type = CHAR_TYPE(src, lastChar);
	while ((type & (TCL_SPACE | TCL_BACKSLASH))
	        || (c == '\n') || (c == ';')) {
	    if (type == TCL_BACKSLASH) {
		if (src[1] == '\n') {
		    src += 2;
		} else {
		    break;
		}
	    } else {
		src++;
	    }
	    c = *src;
	    type = CHAR_TYPE(src, lastChar);
	}

	if (c == '#') {
	    while (src != lastChar) {
		if (c == '\\') {
		    int numRead;
		    Tcl_Backslash(src, &numRead);
		    src += numRead;
		} else if (c == '\n') {
		    src++;
		    c = *src;
		    envPtr->termOffset = (src - string);
		    break;
		} else {
		    src++;
		}
		c = *src;
	    }
	    continue;	/* end of comment, restart outer command loop */
	}

	/*
	 * Compile one command: zero or more words terminated by a '\n',
	 * ';', ']' (if command is terminated by close bracket), or
	 * the end of string.
	 *
	 * command: word*
	 */

	type = CHAR_TYPE(src, lastChar);
	if ((type == TCL_COMMAND_END) 
	        && ((c != ']') || (flags & TCL_BRACKET_TERM))) {
	    continue;  /* empty command; restart outer cmd loop */
	}

	/*
	 * If not the first command, discard the previous command's result.
	 */
	
	if (!isFirstCmd) {
	    TclEmitOpcode(INST_POP, envPtr);
	    if (!(flags & TCL_BRACKET_TERM)) {
		/*
		 * We are compiling a top level command. Update the number
		 * of code bytes for the last command to account for the pop
		 * instruction.
		 */
		
	        (envPtr->cmdMapPtr[lastTopLevelCmdIndex]).numCodeBytes =
		    (envPtr->codeNext-envPtr->codeStart) - cmdCodeOffset;
	    }
	}

	/*
	 * Compile the words of the command. Process the first word
	 * specially, since it is the name of a command. If it is a "simple"
	 * string (just a sequence of characters), look it up in the table
	 * of compilation procedures. If a word other than the first is
	 * simple and represents an integer whose formatted representation
	 * is the same as the word, just push an integer object. Also record
	 * starting source and object information for the command.
	 */

	envPtr->numCommands++;
	cmdIndex = (envPtr->numCommands - 1);
	if (!(flags & TCL_BRACKET_TERM)) {
	    lastTopLevelCmdIndex = cmdIndex;
	}
	
	cmdSrcStart = src;
	cmdCodeOffset = (envPtr->codeNext - envPtr->codeStart);
	cmdWords = 0;
	EnterCmdStartData(envPtr, cmdIndex, src-envPtr->source,
		cmdCodeOffset);
	    
	if ((!(flags & TCL_BRACKET_TERM))
	        && (tclTraceCompile >= 1) && (envPtr->procPtr == NULL)) {
	    /*
	     * Display a line summarizing the top level command we are about
	     * to compile.
	     */
	    
	    char *p = cmdSrcStart;
	    int numChars, complete;
	    
	    while ((CHAR_TYPE(p, lastChar) != TCL_COMMAND_END)
		   || ((*p == ']') && !(flags & TCL_BRACKET_TERM))) {
		p++;
	    }
	    numChars = (p - cmdSrcStart);
	    complete = 1;
	    if (numChars > 60) {
		numChars = 60;
		complete = 0;
	    } else if ((numChars >= 2) && (*p == '\n') && (*(p-1) == '{')) {
		complete = 0;
	    }
	    fprintf(stdout, "Compiling: %.*s%s\n",
		    numChars, cmdSrcStart, (complete? "" : " ..."));
	}
	
	while ((type != TCL_COMMAND_END)
	        || ((c == ']') && !(flags & TCL_BRACKET_TERM))) {
	    /*
	     * Skip any leading white space at the start of a word. Note
	     * that a backslash-newline is treated as a space.
	     */

	    while (type & (TCL_SPACE | TCL_BACKSLASH)) {
		if (type == TCL_BACKSLASH) {
		    if (src[1] == '\n') {
			src += 2;
		    } else {
			break;
		    }
		} else {
		    src++;
		}
		c = *src;
		type = CHAR_TYPE(src, lastChar);
	    }
	    if ((type == TCL_COMMAND_END) 
	            && ((c != ']') || (flags & TCL_BRACKET_TERM))) {
		break;		/* no words remain for command. */
	    }

	    /*
	     * Compile one word. We use an inline version of CompileWord to
	     * avoid an extra procedure call.
	     */

	    envPtr->pushSimpleWords = 0;
	    if (type & (TCL_QUOTE | TCL_OPEN_BRACE)) {
		src++;
		if (type == TCL_QUOTE) {
		    result = TclCompileQuotes(interp, src, lastChar,
			    '"', flags, envPtr);
		} else {
		    result = CompileBraces(interp, src, lastChar,
			    flags, envPtr);
		}
		termPtr = (src + envPtr->termOffset);
		if (result != TCL_OK) {
		    src = termPtr;
		    goto done;
		}

		/*
		 * Make sure terminating character of the quoted or braced
		 * string is the end of word.
		 */
		
		c = *termPtr;
		if ((c == '\\') && (*(termPtr+1) == '\n')) {
		    /*
		     * Line is continued on next line; the backslash-
		     * newline turns into space, which terminates the word.
		     */
		} else {
		    type = CHAR_TYPE(termPtr, lastChar);
		    if ((type != TCL_SPACE) && (type != TCL_COMMAND_END)) {
			Tcl_ResetResult(interp);
			if (*(src-1) == '"') {
			    Tcl_AppendToObj(Tcl_GetObjResult(interp),
				    "extra characters after close-quote", -1);
			} else {
			    Tcl_AppendToObj(Tcl_GetObjResult(interp),
				    "extra characters after close-brace", -1);
			}
			result = TCL_ERROR;
		    }
		}
	    } else {
		result = CompileMultipartWord(interp, src, lastChar,
			flags, envPtr);
		termPtr = (src + envPtr->termOffset);
	    }
	    if (result != TCL_OK) {
		ellipsis = "...";
		src = termPtr;
		goto done;
	    }
	    
	    if (envPtr->wordIsSimple) {
		/*
		 * A simple word. Temporarily replace the terminating
		 * character with a null character.
		 */
		
		numChars = envPtr->numSimpleWordChars;
		savedChar = src[numChars];
		src[numChars] = '\0';

		if ((cmdWords == 0)
		        && (!(iPtr->flags & DONT_COMPILE_CMDS_INLINE))) {
		    /*
		     * The first word of a command and inline command
		     * compilation has not been disabled (e.g., by command
		     * traces). Look up the first word in the interpreter's
		     * hashtable of commands. If a compilation procedure is
		     * found, let it compile the command after resetting
		     * error logging information. Note that if we are
		     * compiling a procedure, we must look up the command
		     * in the procedure's namespace and not the current
		     * namespace.
		     */

		    Namespace *cmdNsPtr;

		    if (envPtr->procPtr != NULL) {
			cmdNsPtr = envPtr->procPtr->cmdPtr->nsPtr;
		    } else {
			cmdNsPtr = NULL;
		    }

		    cmdPtr = NULL;
		    cmd = Tcl_FindCommand(interp, src,
			    (Tcl_Namespace *) cmdNsPtr, /*flags*/ 0);
                    if (cmd != (Tcl_Command) NULL) {
                        cmdPtr = (Command *) cmd;
                    }
		    if ((cmdPtr != NULL) && (cmdPtr->compileProc != NULL)) {
			char *firstArg = termPtr;
			src[numChars] = savedChar;
			iPtr->flags &= ~(ERR_ALREADY_LOGGED | ERR_IN_PROGRESS
					 | ERROR_CODE_SET);
			result = (*(cmdPtr->compileProc))(interp,
				firstArg, lastChar, flags, envPtr);
			if (result == TCL_OK) {
			    src = (firstArg + envPtr->termOffset);
			    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
			    goto finishCommand;
			} else if (result == TCL_OUT_LINE_COMPILE) {
			    result = TCL_OK;
			    src[numChars] = '\0';
			} else {
			    src = firstArg;
			    goto done;           /* an error */
			}
		    }

		    /*
		     * No compile procedure was found for the command: push
		     * the word and continue to compile the remaining
		     * words. If a hashtable entry was found for the
		     * command, push a CmdName object instead to avoid
		     * runtime lookups. If necessary, convert the pushed
		     * object to be a CmdName object. If this is the first
		     * CmdName object in this code unit that refers to the
		     * command, increment the reference count in the
		     * Command structure to reflect the new reference from
		     * the CmdName object and, if the command is deleted
		     * later, to keep the Command structure from being freed
		     * until TclExecuteByteCode has a chance to recognize
		     * that the command was deleted.
		     */

		    objIndex = TclObjIndexForString(src, numChars,
			    /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		    if (cmdPtr != NULL) {
			objPtr = envPtr->objArrayPtr[objIndex];
			if ((objPtr->typePtr != &tclCmdNameType)
			        && (objPtr->bytes != NULL)) {
			    ResolvedCmdName *resPtr = (ResolvedCmdName *)
                                    ckalloc(sizeof(ResolvedCmdName));
                            Namespace *nsPtr = (Namespace *) 
				    Tcl_GetCurrentNamespace(interp);

                            resPtr->cmdPtr = cmdPtr;
                            resPtr->refNsPtr = nsPtr;
			    resPtr->refNsId = nsPtr->nsId;
                            resPtr->refNsCmdEpoch = nsPtr->cmdRefEpoch;
                            resPtr->cmdEpoch = cmdPtr->cmdEpoch;
                            resPtr->refCount = 1;
			    objPtr->internalRep.twoPtrValue.ptr1 =
				(VOID *) resPtr;
			    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
                            objPtr->typePtr = &tclCmdNameType;
			    cmdPtr->refCount++;
			}
		    }
		} else {
		    /*
		     * See if the word represents an integer whose formatted
		     * representation is the same as the word (e.g., this is
		     * true for 123 and -1 but not for 00005). If so, just
		     * push an integer object.
		     */

		    int isCompilableInt = 0;
		    long n;
		    char buf[40];
		    
		    if (TclLooksLikeInt(src)) {
			int code = TclGetLong(interp, src, &n);
			if (code == TCL_OK) {
			    TclFormatInt(buf, n);
			    if (strcmp(src, buf) == 0) {
				isCompilableInt = 1;
				objIndex = TclObjIndexForString(src,
					numChars, /*allocStrRep*/ 0,
					/*inHeap*/ 0, envPtr);
				objPtr = envPtr->objArrayPtr[objIndex];

				Tcl_InvalidateStringRep(objPtr);
				objPtr->internalRep.longValue = n;
				objPtr->typePtr = &tclIntType;
			    }
			} else {
			    Tcl_ResetResult(interp);
			}
		    }
		    if (!isCompilableInt) {
			objIndex = TclObjIndexForString(src, numChars,
			        /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		    }
		}
		src[numChars] = savedChar;
		TclEmitPush(objIndex, envPtr);
		maxDepth = TclMax((cmdWords + 1), maxDepth);
	    } else {		/* not a simple word */
		maxDepth = TclMax((cmdWords + envPtr->maxStackDepth),
			       maxDepth);
	    }
	    src = termPtr;
	    c = *src;
	    type = CHAR_TYPE(src, lastChar);
	    cmdWords++;
	}
	
	/*
	 * Emit an invoke instruction for the command. If a compile command
	 * was found for the command we called it and skipped this.
	 */

	if (cmdWords > 0) {
	    if (cmdWords <= 255) {
	        TclEmitInstUInt1(INST_INVOKE_STK1, cmdWords, envPtr);
            } else {
	        TclEmitInstUInt4(INST_INVOKE_STK4, cmdWords, envPtr);
            }
	}

	/*
	 * Update the compilation environment structure. Record
	 * source/object information for the command.
	 */

        finishCommand:
	EnterCmdExtentData(envPtr, cmdIndex, src-cmdSrcStart,
	        (envPtr->codeNext-envPtr->codeStart) - cmdCodeOffset);
	
	isFirstCmd = 0;
	envPtr->termOffset = (src - string);
	c = *src;
    }

    done:
    if (result == TCL_OK) {
	/*
	 * If the source string yielded no instructions (e.g., if it was
	 * empty), push an empty string object as the command's result.
	 */
    
	if (entryCodeNext == envPtr->codeNext) {
	    int objIndex = TclObjIndexForString("", 0, /*allocStrRep*/ 0,
                                                /*inHeap*/ 0, envPtr);
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = 1;
	}
    } else {
	/*
	 * Add additional error information. First compute the line number
	 * where the error occurred.
	 */

	register char *p;
	int numChars;
	char buf[200];

	iPtr->errorLine = 1;
	for (p = string;  p != cmdSrcStart;  p++) {
	    if (*p == '\n') {
		iPtr->errorLine++;
	    }
	}
	for (  ; isspace(UCHAR(*p)) || (*p == ';');  p++) {
	    if (*p == '\n') {
		iPtr->errorLine++;
	    }
	}

	/*
	 * Figure out how much of the command to print (up to a certain
	 * number of characters, or up to the end of the command).
	 */

	p = cmdSrcStart;
	while ((CHAR_TYPE(p, lastChar) != TCL_COMMAND_END)
		|| ((*p == ']') && !(flags & TCL_BRACKET_TERM))) {
	    p++;
	}
	numChars = (p - cmdSrcStart);
	if (numChars > 150) {
	    numChars = 150;
	    ellipsis = " ...";
	} else if ((numChars >= 2) && (*p == '\n') && (*(p-1) == '{')) {
	    ellipsis = " ...";
	}
	
	sprintf(buf, "\n    while compiling\n\"%.*s%s\"",
		numChars, cmdSrcStart, ellipsis);
	Tcl_AddObjErrorInfo(interp, buf, -1);
    } 
	
    envPtr->termOffset = (src - string);
    iPtr->termOffset = envPtr->termOffset;
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileWord --
 *
 *	This procedure compiles one word from a command string. It skips
 *	any leading white space.
 *
 *	Ordinarily, callers set envPtr->pushSimpleWords to 1 and this
 *	procedure emits push and other instructions to compute the
 *	word on the Tcl evaluation stack at execution time. If a caller sets
 *	envPtr->pushSimpleWords to 0, CompileWord will _not_ compile
 *	"simple" words: words that are just a sequence of characters without
 *	backslashes. It will leave their compilation up to the caller.
 *
 *	As an important special case, if the word is simple, this procedure
 *	sets envPtr->wordIsSimple to 1 and envPtr->numSimpleWordChars to the
 *	number of characters in the simple word. This allows the caller to
 *	process these words specially.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs, an
 *	error message is left in the interpreter's result.
 *	
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed in the last
 *	word. This is normally the character just after the last one in a
 *	word (perhaps the command terminator), or the vicinity of an error
 *	(if the result is not TCL_OK).
 *
 *	envPtr->wordIsSimple is set 1 if the word is simple: just a
 *	sequence of characters without backslashes. If so, the word's
 *	characters are the envPtr->numSimpleWordChars characters starting 
 *	at string.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to evaluate the word. This is not changed if
 *	the word is simple and envPtr->pushSimpleWords was 0 (false).
 *
 * Side effects:
 *	Instructions are added to envPtr to compute and push the word
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileWord(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Interpreter to use for nested command
				 * evaluations and error messages. */
    char *string;		/* First character of word. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;			/* Flags to control compilation (same values
				 * passed to Tcl_EvalObj). */
    CompileEnv *envPtr;		/* Holds the resulting instructions. */
{
    /*
     * Compile one word: approximately
     *
     * word:             quoted_string | braced_string | multipart_word
     * quoted_string:    '"' char* '"'
     * braced_string:    '{' char* '}'
     * multipart_word    (see CompileMultipartWord below)
     */
    
    register char *src = string; /* Points to current source char. */
    register int type = CHAR_TYPE(src, lastChar);
				 /* Current char's CHAR_TYPE type. */
    int maxDepth = 0;		 /* Maximum number of stack elements needed
				  * to compute and push the word. */
    char *termPtr = src;	 /* Points to the character that terminated
				  * the word. */
    int result = TCL_OK;

    /*
     * Skip any leading white space at the start of a word. Note that a
     * backslash-newline is treated as a space.
     */

    while (type & (TCL_SPACE | TCL_BACKSLASH)) {
	if (type == TCL_BACKSLASH) {
	    if (src[1] == '\n') {
		src += 2;
	    } else {
		break;		/* no longer white space */
	    }
	} else {
	    src++;
	}
	type = CHAR_TYPE(src, lastChar);
    }
    if (type == TCL_COMMAND_END) {
	goto done;
    }

    /*
     * Compile the word. Handle quoted and braced string words here in order
     * to avoid an extra procedure call.
     */

    if (type & (TCL_QUOTE | TCL_OPEN_BRACE)) {
	src++;
	if (type == TCL_QUOTE) {
	    result = TclCompileQuotes(interp, src, lastChar, '"', flags,
		    envPtr);
	} else {
	    result = CompileBraces(interp, src, lastChar, flags, envPtr);
	}
	termPtr = (src + envPtr->termOffset);
	if (result != TCL_OK) {
	    goto done;
	}
	
	/*
	 * Make sure terminating character of the quoted or braced string is
	 * the end of word.
	 */
	
	if ((*termPtr == '\\') && (*(termPtr+1) == '\n')) {
	    /*
	     * Line is continued on next line; the backslash-newline turns
	     * into space, which terminates the word.
	     */
	} else {
	    type = CHAR_TYPE(termPtr, lastChar);
	    if (!(type & (TCL_SPACE | TCL_COMMAND_END))) {
		Tcl_ResetResult(interp);
		if (*(src-1) == '"') {
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "extra characters after close-quote", -1);
		} else {
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
			    "extra characters after close-brace", -1);
		}
		result = TCL_ERROR;
		goto done;
	    }
	}
	maxDepth = envPtr->maxStackDepth;
    } else {
	result = CompileMultipartWord(interp, src, lastChar, flags, envPtr);
	termPtr = (src + envPtr->termOffset);
	maxDepth = envPtr->maxStackDepth;
    }

    /*
     * Done processing the word. The values of envPtr->wordIsSimple and
     * envPtr->numSimpleWordChars are left at the values returned by
     * TclCompileQuotes/Braces/MultipartWord.
     */
    
    done:
    envPtr->termOffset = (termPtr - string);
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileMultipartWord --
 *
 *	This procedure compiles one multipart word: a word comprised of some
 *	number of nested commands, variable references, or arbitrary
 *	characters. This procedure assumes that quoted string and braced
 *	string words and the end of command have already been handled by its
 *	caller. It also assumes that any leading white space has already
 *	been consumed.
 *
 *	Ordinarily, callers set envPtr->pushSimpleWords to 1 and this
 *	procedure emits push and other instructions to compute the word on
 *	the Tcl evaluation stack at execution time. If a caller sets
 *	envPtr->pushSimpleWords to 0, it will _not_ compile "simple" words:
 *	words that are just a sequence of characters without backslashes.
 *	It will leave their compilation up to the caller. This is done, for
 *	example, to provide special support for the first word of commands,
 *	which are almost always the (simple) name of a command.
 *
 *	As an important special case, if the word is simple, this procedure
 *	sets envPtr->wordIsSimple to 1 and envPtr->numSimpleWordChars to the
 *	number of characters in the simple word. This allows the caller to
 *	process these words specially.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs, an
 *	error message is left in the interpreter's result.
 *	
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed in the last
 *	word. This is normally the character just after the last one in a
 *	word (perhaps the command terminator), or the vicinity of an error
 *	(if the result is not TCL_OK).
 *
 *	envPtr->wordIsSimple is set 1 if the word is simple: just a
 *	sequence of characters without backslashes. If so, the word's
 *	characters are the envPtr->numSimpleWordChars characters starting 
 *	at string.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to evaluate the word. This is not changed if
 *	the word is simple and envPtr->pushSimpleWords was 0 (false).
 *
 * Side effects:
 *	Instructions are added to envPtr to compute and push the word
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileMultipartWord(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Interpreter to use for nested command
				 * evaluations and error messages. */
    char *string;		/* First character of word. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;			/* Flags to control compilation (same values
				 * passed to Tcl_EvalObj). */
    CompileEnv *envPtr;		/* Holds the resulting instructions. */
{
    /*
     * Compile one multi_part word:
     *
     * multi_part_word:  word_part+
     * word_part:        nested_cmd | var_reference | char+
     * nested_cmd:       '[' command ']'
     * var_reference:    '$' name | '$' name '(' index_string ')' |
     *                   '$' '{' braced_name '}')
     * name:             (letter | digit | underscore)+
     * braced_name:      (non_close_brace_char)*
     * index_string:     (non_close_paren_char)*
     */
    
    register char *src = string; /* Points to current source char. */
    register char c = *src;	/* The current char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int bracketNormal = !(flags & TCL_BRACKET_TERM);
    int simpleWord = 0;		/* Set 1 if word is simple. */
    int numParts = 0;		/* Count of word_part objs pushed. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to compute and push the word. */
    char *start;		/* Starting position of char+ word_part. */
    int hasBackslash;		/* Nonzero if '\' in char+ word_part. */
    int numChars;		/* Number of chars in char+ word_part. */
    char savedChar;		/* Holds the character from string
				 * termporarily replaced by a null character
				 * during word_part processing. */
    int objIndex;		/* The object array index for a pushed
				 * object holding a word_part. */
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int result = TCL_OK;
    int numRead;

    type = CHAR_TYPE(src, lastChar);
    while (1) {
	/*
	 * Process a word_part: a sequence of chars, a var reference, or
	 * a nested command.
	 */

	if ((type & (TCL_NORMAL | TCL_CLOSE_BRACE | TCL_BACKSLASH |
		     TCL_QUOTE | TCL_OPEN_BRACE)) ||
	    ((c == ']') && bracketNormal)) {
	    /*
	     * A char+ word part. Scan first looking for any backslashes.
	     * Note that a backslash-newline must be treated as a word
	     * separator, as if the backslash-newline had been collapsed
	     * before command parsing began.
	     */
	    
	    start = src;
	    hasBackslash = 0;
	    do {
		if (type == TCL_BACKSLASH) {
		    hasBackslash = 1;
		    Tcl_Backslash(src, &numRead);
		    if (src[1] == '\n') {
			src += numRead;
			type = TCL_SPACE; /* force word end */
			break;
		    }
		    src += numRead;
		} else {
		    src++;
		}
		c = *src;
		type = CHAR_TYPE(src, lastChar);
	    } while (type & (TCL_NORMAL | TCL_BACKSLASH | TCL_QUOTE |
			    TCL_OPEN_BRACE | TCL_CLOSE_BRACE)
			    || ((c == ']') && bracketNormal));

	    if ((numParts == 0) && !hasBackslash
		    && (type & (TCL_SPACE | TCL_COMMAND_END))) {
		/*
		 * The word is "simple": just a sequence of characters
		 * without backslashes terminated by a TCL_SPACE or
		 * TCL_COMMAND_END. Just return if we are not to compile
		 * simple words.
		 */

		simpleWord = 1;
		if (!envPtr->pushSimpleWords) {
		    envPtr->wordIsSimple = 1;
		    envPtr->numSimpleWordChars = (src - string);
		    envPtr->termOffset = envPtr->numSimpleWordChars;
		    envPtr->pushSimpleWords = savePushSimpleWords;
		    return TCL_OK;
		}
	    }

	    /*
	     * Create and push a string object for the char+ word_part,
	     * which starts at "start" and ends at the char just before
	     * src. If backslashes were found, copy the word_part's
	     * characters with substituted backslashes into a heap-allocated
	     * buffer and use it to create the string object. Temporarily
	     * replace the terminating character with a null character.
	     */

	    numChars = (src - start);
	    savedChar = start[numChars];
	    start[numChars] = '\0';
	    if ((numChars > 0) && (hasBackslash)) {
		char *buffer = ckalloc((unsigned) numChars + 1);
		register char *dst = buffer;
		register char *p = start;
		while (p < src) {
		    if (*p == '\\') {	
			*dst = Tcl_Backslash(p, &numRead);
			if (p[1] == '\n') {
			    break;
			}
			p += numRead;
			dst++;
		    } else {
			*dst++ = *p++;
		    }
		}
		*dst = '\0';
		objIndex = TclObjIndexForString(buffer, dst-buffer,
			/*allocStrRep*/ 1, /*inHeap*/ 1, envPtr);
	    } else {
		objIndex = TclObjIndexForString(start, numChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	    }
	    start[numChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = TclMax((numParts + 1), maxDepth);
	} else if (type == TCL_DOLLAR) {
	    result = TclCompileDollarVar(interp, src, lastChar,
		    flags, envPtr);
	    src += envPtr->termOffset;
	    if (result != TCL_OK) {
		goto done;
	    }
	    maxDepth = TclMax((numParts + envPtr->maxStackDepth), maxDepth);
	    c = *src;
	    type = CHAR_TYPE(src, lastChar);
	} else if (type == TCL_OPEN_BRACKET) {
	    char *termPtr;
	    envPtr->pushSimpleWords = 1;
	    src++;
	    result = TclCompileString(interp, src, lastChar,
				      (flags | TCL_BRACKET_TERM), envPtr);
	    termPtr = (src + envPtr->termOffset);
	    if (*termPtr == ']') {
		termPtr++;
	    } else if (*termPtr == '\0') {
		/*
		 * Missing ] at end of nested command.
		 */
		
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "missing close-bracket", -1);
		result = TCL_ERROR;
	    }
	    src = termPtr;
	    if (result != TCL_OK) {
		goto done;
	    }
	    maxDepth = TclMax((numParts + envPtr->maxStackDepth), maxDepth);
	    c = *src;
	    type = CHAR_TYPE(src, lastChar);
	} else if (type & (TCL_SPACE | TCL_COMMAND_END)) {
	    goto wordEnd;
	}
	numParts++;
    } /* end of infinite loop */

    wordEnd:
    /*
     * End of a non-simple word: TCL_SPACE, TCL_COMMAND_END, or
     * backslash-newline. Concatenate the word_parts if necessary.
     */

    while (numParts > 255) {
	TclEmitInstUInt1(INST_CONCAT1, 255, envPtr);
	numParts -= 254;  /* concat pushes 1 obj, the result */
    }
    if (numParts > 1) {
	TclEmitInstUInt1(INST_CONCAT1, numParts, envPtr);
    }

    done:
    if (simpleWord) {
	envPtr->wordIsSimple = 1;
	envPtr->numSimpleWordChars = (src - string);
    } else {
	envPtr->wordIsSimple = 0;
	envPtr->numSimpleWordChars = 0;
    }
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileQuotes --
 *
 *	This procedure compiles a double-quoted string such as a quoted Tcl
 *	command argument or a quoted value in a Tcl expression. This
 *	procedure is also used to compile array element names within
 *	parentheses (where the termChar will be ')' instead of '"'), or
 *	anything else that needs the substitutions that happen in quotes.
 *
 *	Ordinarily, callers set envPtr->pushSimpleWords to 1 and
 *	TclCompileQuotes always emits push and other instructions to compute
 *	the word on the Tcl evaluation stack at execution time. If a caller
 *	sets envPtr->pushSimpleWords to 0, TclCompileQuotes will not compile
 *	"simple" words: words that are just a sequence of characters without
 *	backslashes. It will leave their compilation up to the caller. This
 *	is done to provide special support for the first word of commands,
 *	which are almost always the (simple) name of a command.
 *
 *	As an important special case, if the word is simple, this procedure
 *	sets envPtr->wordIsSimple to 1 and envPtr->numSimpleWordChars to the
 *	number of characters in the simple word. This allows the caller to
 *	process these words specially.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing the quoted string. If an error
 *	occurs then the interpreter's result contains a standard error
 *	message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed; this is
 *	usually the character just after the matching close-quote.
 *
 *	envPtr->wordIsSimple is set 1 if the word is simple: just a
 *	sequence of characters without backslashes. If so, the word's
 *	characters are the envPtr->numSimpleWordChars characters starting 
 *	at string.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to evaluate the word. This is not changed if
 *	the word is simple and envPtr->pushSimpleWords was 0 (false).
 *
 * Side effects:
 *	Instructions are added to envPtr to push the quoted-string
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileQuotes(interp, string, lastChar, termChar, flags, envPtr)
    Tcl_Interp *interp;		 /* Interpreter to use for nested command
				  * evaluations and error messages. */
    char *string;		 /* Points to the character just after
				  * the opening '"' or '('. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int termChar;		 /* Character that terminates the "quoted"
				  * string (usually double-quote, but might
				  * be right-paren or something else). */
    int flags;			 /* Flags to control compilation (same 
				  * values passed to Tcl_Eval). */
    CompileEnv *envPtr;		 /* Holds the resulting instructions. */
{
    register char *src = string; /* Points to current source char. */
    register char c = *src;	 /* The current char. */
    int simpleWord = 0;		 /* Set 1 if a simple quoted string word. */
    char *start;		 /* Start position of char+ string_part. */
    int hasBackslash; 	         /* 1 if '\' found in char+ string_part. */
    int numRead;		 /* Count of chars read by Tcl_Backslash. */
    int numParts = 0;	         /* Count of string_part objs pushed. */
    int maxDepth = 0;		 /* Maximum number of stack elements needed
				  * to compute and push the string. */
    char savedChar;		 /* Holds the character from string
				  * termporarily replaced by a null 
				  * char during string_part processing. */
    int objIndex;		 /* The object array index for a pushed
				  * object holding a string_part. */
    int numChars;		 /* Number of chars in string_part. */
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int result = TCL_OK;
    
    /*
     * quoted_string: '"' string_part* '"'   (or termChar instead of ")
     * string_part:   var_reference | nested_cmd | char+
     */


    while ((src != lastChar) && (c != termChar)) {
	if (c == '$') {
	    result = TclCompileDollarVar(interp, src, lastChar, flags,
		    envPtr);
	    src += envPtr->termOffset;
	    if (result != TCL_OK) {
		goto done;
	    }
	    maxDepth = TclMax((numParts + envPtr->maxStackDepth), maxDepth);
	    c = *src;
        } else if (c == '[') {
	    char *termPtr;
	    envPtr->pushSimpleWords = 1;
	    src++;
	    result = TclCompileString(interp, src, lastChar,
				      (flags | TCL_BRACKET_TERM), envPtr);
	    termPtr = (src + envPtr->termOffset);
	    if (*termPtr == ']') {
		termPtr++;
	    }
	    src = termPtr;
	    if (result != TCL_OK) {
		goto done;
	    }
	    if (termPtr == lastChar) {
		/*
		 * Missing ] at end of nested command.
		 */
		
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "missing close-bracket", -1);
		result = TCL_ERROR;
		goto done;
	    }
	    maxDepth = TclMax((numParts + envPtr->maxStackDepth), maxDepth);
	    c = *src;
        } else {
	    /*
	     * Start of a char+ string_part. Scan first looking for any
	     * backslashes.
	     */

	    start = src;
	    hasBackslash = 0;
	    do {
		if (c == '\\') {
		    hasBackslash = 1;
		    Tcl_Backslash(src, &numRead);
		    src += numRead;
		} else {
		    src++;
		}
		c = *src;
            } while ((src != lastChar) && (c != '$') && (c != '[')
		    && (c != termChar));
	    
	    if ((numParts == 0) && !hasBackslash
		    && ((src == lastChar) && (c == termChar))) {
		/*
		 * The quoted string is "simple": just a sequence of
		 * characters without backslashes terminated by termChar or
		 * a null character. Just return if we are not to compile
		 * simple words.
		 */

		simpleWord = 1;
		if (!envPtr->pushSimpleWords) {
		    if ((src == lastChar) && (termChar != '\0')) {
			char buf[40];
			sprintf(buf, "missing %c", termChar);
			Tcl_ResetResult(interp);
			Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
			result = TCL_ERROR;
		    } else {
			src++;
		    }
		    envPtr->wordIsSimple = 1;
		    envPtr->numSimpleWordChars = (src - string - 1);
		    envPtr->termOffset = (src - string);
		    envPtr->pushSimpleWords = savePushSimpleWords;
		    return result;
		}
	    }

	    /*
	     * Create and push a string object for the char+ string_part
	     * that starts at "start" and ends at the char just before
	     * src. If backslashes were found, copy the string_part's
	     * characters with substituted backslashes into a heap-allocated
	     * buffer and use it to create the string object. Temporarily
	     * replace the terminating character with a null character.
	     */
	    
	    numChars = (src - start);
	    savedChar = start[numChars];
	    start[numChars] = '\0';
	    if ((numChars > 0) && (hasBackslash)) {
		char *buffer = ckalloc((unsigned) numChars + 1);
		register char *dst = buffer;
		register char *p = start;
		while (p < src) {
		    if (*p == '\\') {
			*dst++ = Tcl_Backslash(p, &numRead);
			p += numRead;
		    } else {
			*dst++ = *p++;
		    }
		}
		*dst = '\0';
		objIndex = TclObjIndexForString(buffer, (dst - buffer),
			/*allocStrRep*/ 1, /*inHeap*/ 1, envPtr);
	    } else {
		objIndex = TclObjIndexForString(start, numChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	    }
	    start[numChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = TclMax((numParts + 1), maxDepth);
        }
	numParts++;
    } 
	    
    /*
     * End of the quoted string: src points at termChar or '\0'. If
     * necessary, concatenate the string_part objects on the stack.
     */

    if ((src == lastChar) && (termChar != '\0')) {
	char buf[40];
	sprintf(buf, "missing %c", termChar);
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	result = TCL_ERROR;
	goto done;
    } else {
	src++;
    }

    if (numParts == 0) {
	/*
	 * The quoted string was empty. Push an empty string object.
	 */

	int objIndex = TclObjIndexForString("", 0, /*allocStrRep*/ 0,
                                            /*inHeap*/ 0, envPtr);
	TclEmitPush(objIndex, envPtr);
    } else {
	/*
	 * Emit any needed concat instructions.
	 */
	
	while (numParts > 255) {
	    TclEmitInstUInt1(INST_CONCAT1, 255, envPtr);
	    numParts -= 254;  /* concat pushes 1 obj, the result */
	}
	if (numParts > 1) {
	    TclEmitInstUInt1(INST_CONCAT1, numParts, envPtr);
	}
    }

    done:
    if (simpleWord) {
	envPtr->wordIsSimple = 1;
	envPtr->numSimpleWordChars = (src - string - 1);
    } else {
	envPtr->wordIsSimple = 0;
	envPtr->numSimpleWordChars = 0;
    }
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * CompileBraces --
 *
 *	This procedure compiles characters between matching curly braces.
 *
 *	Ordinarily, callers set envPtr->pushSimpleWords to 1 and
 *	CompileBraces always emits a push instruction to compute the word on
 *	the Tcl evaluation stack at execution time. However, if a caller
 *	sets envPtr->pushSimpleWords to 0, CompileBraces will _not_ compile
 *	"simple" words: words that are just a sequence of characters without
 *	backslash-newlines. It will leave their compilation up to the
 *	caller.
 *
 *	As an important special case, if the word is simple, this procedure
 *	sets envPtr->wordIsSimple to 1 and envPtr->numSimpleWordChars to the
 *	number of characters in the simple word. This allows the caller to
 *	process these words specially.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed. This is
 *	usually the character just after the matching close-brace.
 *
 *	envPtr->wordIsSimple is set 1 if the word is simple: just a
 *	sequence of characters without backslash-newlines. If so, the word's
 *	characters are the envPtr->numSimpleWordChars characters starting 
 *	at string.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to evaluate the word. This is not changed if
 *	the word is simple and envPtr->pushSimpleWords was 0 (false).
 *
 * Side effects:
 *	Instructions are added to envPtr to push the braced string
 *	at runtime.
 *
 *--------------------------------------------------------------
 */

static int
CompileBraces(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		 /* Interpreter to use for nested command
				  * evaluations and error messages. */
    char *string;		 /* Character just after opening bracket. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;			 /* Flags to control compilation (same 
				  * values passed to Tcl_Eval). */
    CompileEnv *envPtr;		 /* Holds the resulting instructions. */
{
    register char *src = string; /* Points to current source char. */
    register char c;		 /* The current char. */
    int simpleWord = 0;		 /* Set 1 if a simple braced string word. */
    int level = 1;		 /* {} nesting level. Initially 1 since {
				  * was parsed before we were called. */
    int hasBackslashNewline = 0; /* Nonzero if '\' found. */
    char *last;			 /* Points just before terminating '}'. */
    int numChars;		 /* Number of chars in braced string. */
    char savedChar;		 /* Holds the character from string
				  * termporarily replaced by a null 
				  * char during braced string processing. */
    int objIndex;		 /* The object array index for a pushed
				  * object holding a braced string. */
    int numRead;
    int result = TCL_OK;

    /*
     * Check for any backslash-newlines, since we must treat
     * backslash-newlines specially (they must be replaced by spaces).
     */

    while (1) {
	c = *src;
	if (src == lastChar) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    "missing close-brace", -1);
	    result = TCL_ERROR;
	    goto done;
	}
	if (CHAR_TYPE(src, lastChar) != TCL_NORMAL) {
	    if (c == '{') {
		level++;
	    } else if (c == '}') {
		--level;
		if (level == 0) {
		    src++;
		    last = (src - 2); /* point just before terminating } */
		    break;
		}
	    } else if (c == '\\') {
		if (*(src+1) == '\n') {
		    hasBackslashNewline = 1;
		}
		(void) Tcl_Backslash(src, &numRead);
		src += numRead - 1;
	    }
	}
	src++;
    }

    if (!hasBackslashNewline) {
	/*
	 * The braced word is "simple": just a sequence of characters
	 * without backslash-newlines. Just return if we are not to compile
	 * simple words.
	 */

	simpleWord = 1;
	if (!envPtr->pushSimpleWords) {
	    envPtr->wordIsSimple = 1;
	    envPtr->numSimpleWordChars = (src - string - 1);
	    envPtr->termOffset = (src - string);
	    return TCL_OK;
	}
    }

    /*
     * Create and push a string object for the braced string. This starts at
     * "string" and ends just after "last" (which points to the final
     * character before the terminating '}'). If backslash-newlines were
     * found, we copy characters one at a time into a heap-allocated buffer
     * and do backslash-newline substitutions.
     */

    numChars = (last - string + 1);
    savedChar = string[numChars];
    string[numChars] = '\0';
    if ((numChars > 0) && (hasBackslashNewline)) {
	char *buffer = ckalloc((unsigned) numChars + 1);
	register char *dst = buffer;
	register char *p = string;
	while (p <= last) {
	    c = *dst++ = *p++;
	    if (c == '\\') {
		if (*p == '\n') {
		    dst[-1] = Tcl_Backslash(p-1, &numRead);
		    p += numRead - 1;
		} else {
		    (void) Tcl_Backslash(p-1, &numRead);
		    while (numRead > 1) {
			*dst++ = *p++;
			numRead--;
		    }
		}
	    }
	}
	*dst = '\0';
	objIndex = TclObjIndexForString(buffer, (dst - buffer),
		/*allocStrRep*/ 1, /*inHeap*/ 1, envPtr);
    } else {
	objIndex = TclObjIndexForString(string, numChars, /*allocStrRep*/ 1,
                                        /*inHeap*/ 0, envPtr);
    }
    string[numChars] = savedChar;
    TclEmitPush(objIndex, envPtr);

    done:
    if (simpleWord) {
	envPtr->wordIsSimple = 1;
	envPtr->numSimpleWordChars = (src - string - 1);
    } else {
	envPtr->wordIsSimple = 0;
	envPtr->numSimpleWordChars = 0;
    }
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = 1;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileDollarVar --
 *
 *	Given a string starting with a $ sign, parse a variable name
 *	and compile instructions to push its value. If the variable
 *	reference is just a '$' (i.e. the '$' isn't followed by anything
 *	that could possibly be a variable name), just push a string object
 *	containing '$'.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs
 *	then an error message is left in the interpreter's result.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one in the variable reference.
 *
 *	envPtr->wordIsSimple is set 0 (false) because the word is not
 *	simple: it is not just a sequence of characters without backslashes.
 *	For the same reason, envPtr->numSimpleWordChars is set 0.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the string's commands.
 *
 * Side effects:
 *	Instructions are added to envPtr to look up the variable and
 *	push its value at runtime.
 *
 *----------------------------------------------------------------------
 */
    
int
TclCompileDollarVar(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		 /* Interpreter to use for nested command
				  * evaluations and error messages. */
    char *string;		 /* First char (i.e. $) of var reference. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;			 /* Flags to control compilation (same
				  * values passed to Tcl_Eval). */
    CompileEnv *envPtr;		 /* Holds the resulting instructions. */
{
    register char *src = string; /* Points to current source char. */
    register char c;		 /* The current char. */
    char *name;			 /* Start of 1st part of variable name. */
    int nameChars;		 /* Count of chars in name. */
    int nameHasNsSeparators = 0; /* Set 1 if name contains "::"s. */
    char savedChar;		 /* Holds the character from string
				  * termporarily replaced by a null 
				  * char during name processing. */
    int objIndex;		 /* The object array index for a pushed
				  * object holding a name part. */
    int isArrayRef = 0;		 /* 1 if reference to array element. */
    int localIndex = -1;	 /* Frame index of local if found.  */
    int maxDepth = 0;		 /* Maximum number of stack elements needed
				  * to push the variable. */
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int result = TCL_OK;

    /*
     * var_reference: '$' '{' braced_name '}' |
     *                '$' name ['(' index_string ')']
     *
     * There are three cases:
     * 1. The $ sign is followed by an open curly brace. Then the variable
     *    name is everything up to the next close curly brace, and the
     *    variable is a scalar variable.
     * 2. The $ sign is not followed by an open curly brace. Then the
     *    variable name is everything up to the next character that isn't
     *    a letter, digit, underscore, or a "::" namespace separator. If the
     *    following character is an open parenthesis, then the information
     *    between parentheses is the array element name, which can include
     *    any of the substitutions permissible between quotes.
     * 3. The $ sign is followed by something that isn't a letter, digit,
     *    underscore, or a "::" namespace separator: in this case,
     *    there is no variable name, and "$" is pushed.
     */

    src++;			/* advance over the '$'. */

    /*
     * Collect the first part of the variable's name into "name" and
     * determine if it is an array reference and if it contains any
     * namespace separator (::'s).
     */
    
    if (*src == '{') {
        /*
	 * A scalar name in braces.
	 */

	char *p;

	src++;
        name = src;
        c = *src;
	while (c != '}') {
	    if (src == lastChar) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
			"missing close-brace for variable name", -1);
		result = TCL_ERROR;
		goto done;
	    }
	    src++;
	    c = *src;
	}
	nameChars = (src - name);
	for (p = name;  p < src;  p++) {
	    if ((*p == ':') && (*(p+1) == ':')) {
		nameHasNsSeparators = 1;
		break;
	    }
	}
	src++;			/* advance over the '}'. */
    } else {
	/*
	 * Scalar name or array reference not in braces.
	 */
	
        name = src;
        c = *src;
        while (isalnum(UCHAR(c)) || (c == '_') || (c == ':')) {
	    if (c == ':') {
                if (*(src+1) == ':') {
		    nameHasNsSeparators = 1;
                    src += 2;
		    while (*src == ':') {
			src++;
		    }
                    c = *src;
                } else {
                    break;	/* : by itself */
                }
            } else {
                src++;
                c = *src;
            }
	}
	if (src == name) {
	    /*
	     * A '$' by itself, not a name reference. Push a "$" string.
	     */

	    objIndex = TclObjIndexForString("$", 1, /*allocStrRep*/ 1,
                                            /*inHeap*/ 0, envPtr);
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = 1;
	    goto done;
	}
	nameChars = (src - name);
	isArrayRef = (c == '(');
    }

    /*
     * Now emit instructions to load the variable. First either push the
     * name of the scalar or array, or determine its index in the array of
     * local variables in a procedure frame. Push the name if we are not
     * compiling a procedure body or if the name has namespace
     * qualifiers ("::"s).
     */
    
    if (!isArrayRef) {		/* scalar reference */
	if ((envPtr->procPtr == NULL) || nameHasNsSeparators) {
	    savedChar = name[nameChars];
	    name[nameChars] = '\0';
	    objIndex = TclObjIndexForString(name, nameChars,
		    /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	    name[nameChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    TclEmitOpcode(INST_LOAD_SCALAR_STK, envPtr);
	    maxDepth = 1;
	} else {
	    localIndex = LookupCompiledLocal(name, nameChars,
	            /*createIfNew*/ 0, /*flagsIfCreated*/ 0,
		    envPtr->procPtr);
	    if (localIndex >= 0) {
		if (localIndex <= 255) {
		    TclEmitInstUInt1(INST_LOAD_SCALAR1, localIndex, envPtr);
		} else {
		    TclEmitInstUInt4(INST_LOAD_SCALAR4, localIndex, envPtr);
		}
		maxDepth = 0;
	    } else {
		savedChar = name[nameChars];
		name[nameChars] = '\0';
		objIndex = TclObjIndexForString(name, nameChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		name[nameChars] = savedChar;
		TclEmitPush(objIndex, envPtr); 
		TclEmitOpcode(INST_LOAD_SCALAR_STK, envPtr);
		maxDepth = 1;
	    }
	}
    } else {			/* array reference */
	if ((envPtr->procPtr == NULL) || nameHasNsSeparators) {
	    savedChar = name[nameChars];
	    name[nameChars] = '\0';
	    objIndex = TclObjIndexForString(name, nameChars,
		    /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	    name[nameChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = 1;
	} else {
	    localIndex = LookupCompiledLocal(name, nameChars,
	            /*createIfNew*/ 0, /*flagsIfCreated*/ 0,
		    envPtr->procPtr);
	    if (localIndex < 0) {
		savedChar = name[nameChars];
		name[nameChars] = '\0';
		objIndex = TclObjIndexForString(name, nameChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		name[nameChars] = savedChar;
		TclEmitPush(objIndex, envPtr);
		maxDepth = 1;
	    }
	}

	/*
	 * Parse and push the array element. Perform substitutions on it,
	 * just as is done for quoted strings.
	 */

	src++;
	envPtr->pushSimpleWords = 1;
	result = TclCompileQuotes(interp, src, lastChar, ')', flags,
		envPtr);
	src += envPtr->termOffset;
	if (result != TCL_OK) {
	    char msg[200];
	    sprintf(msg, "\n    (parsing index for array \"%.*s\")",
		    (nameChars > 100? 100 : nameChars), name);
	    Tcl_AddObjErrorInfo(interp, msg, -1);
	    goto done;
	}
	maxDepth += envPtr->maxStackDepth;

	/*
	 * Now emit the appropriate load instruction for the array element.
	 */

	if (localIndex < 0) {	/* a global or an unknown local */
	    TclEmitOpcode(INST_LOAD_ARRAY_STK, envPtr);
	} else {
	    if (localIndex <= 255) {
		TclEmitInstUInt1(INST_LOAD_ARRAY1, localIndex, envPtr);
	    } else {
		TclEmitInstUInt4(INST_LOAD_ARRAY4, localIndex, envPtr);
	    }
	}
    }

    done:
    envPtr->termOffset = (src - string);
    envPtr->wordIsSimple = 0;
    envPtr->numSimpleWordChars = 0;
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileBreakCmd --
 *
 *	Procedure called to compile the "break" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "break" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileBreakCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    register char *src = string;/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int result = TCL_OK;
    
    /*
     * There should be no argument after the "break".
     */

    type = CHAR_TYPE(src, lastChar);
    if (type != TCL_COMMAND_END) {
	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if (type != TCL_COMMAND_END) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "wrong # args: should be \"break\"", -1);
	    result = TCL_ERROR;
	    goto done;
	}
    }

    /*
     * Emit a break instruction.
     */

    TclEmitOpcode(INST_BREAK, envPtr);

    done:
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = 0;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileCatchCmd --
 *
 *	Procedure called to compile the "catch" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK if
 *	compilation was successful. If an error occurs then the
 *	interpreter's result contains a standard error message and TCL_ERROR
 *	is returned. If compilation failed because the command is too
 *	complex for TclCompileCatchCmd, TCL_OUT_LINE_COMPILE is returned
 *	indicating that the catch command should be compiled "out of line"
 *	by emitting code to invoke its command procedure at runtime.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "catch" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileCatchCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Proc *procPtr = envPtr->procPtr;
    				/* Points to structure describing procedure
				 * containing the catch cmd, else NULL. */
    int maxDepth = 0;           /* Maximum number of stack elements needed
				 * to execute cmd. */
    ArgInfo argInfo;		/* Structure holding information about the
				 * start and end of each argument word. */
    int range = -1;		/* If we compile the catch command, the
				 * index for its catch range record in the
				 * ExceptionRange array. -1 if we are not
				 * compiling the command. */
    char *name;			/* If a var name appears for a scalar local
				 * to a procedure, this points to the name's
				 * 1st char and nameChars is its length. */
    int nameChars;		/* Length of the variable name, if any. */
    int localIndex = -1;        /* Index of the variable in the current
				 * procedure's array of local variables.
				 * Otherwise -1 if not in a procedure or
				 * the variable wasn't found. */
    char savedChar;		/* Holds the character from string
				 * termporarily replaced by a null character
				 * during processing of words. */
    JumpFixup jumpFixup;	/* Used to emit the jump after the "no
				 * errors" epilogue code. */
    int numWords, objIndex, jumpDist, result;
    char *bodyStart, *bodyEnd;
    Tcl_Obj *objPtr;
    int savePushSimpleWords = envPtr->pushSimpleWords;

    /*
     * Scan the words of the command and record the start and finish of
     * each argument word.
     */

    InitArgInfo(&argInfo);
    result = CollectArgInfo(interp, string, lastChar, flags, &argInfo);
    numWords = argInfo.numArgs;	  /* i.e., the # after the command name */
    if (result != TCL_OK) {
	goto done;
    }
    if ((numWords != 1) && (numWords != 2)) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"catch command ?varName?\"", -1);
        result = TCL_ERROR;
	goto done;
    }

    /*
     * If a variable was specified and the catch command is at global level
     * (not in a procedure), don't compile it inline: the payoff is
     * too small.
     */

    if ((numWords == 2) && (procPtr == NULL)) {
	result = TCL_OUT_LINE_COMPILE;
        goto done;
    }

    /*
     * Make sure the variable name, if any, has no substitutions and just
     * refers to a local scaler.
     */

    if (numWords == 2) {
	char *firstChar = argInfo.startArray[1];
	char *lastChar  = argInfo.endArray[1];
	
	if (*firstChar == '{') {
	    if (*lastChar != '}') {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "extra characters after close-brace", -1);
		result = TCL_ERROR;
		goto done;
	    }
	    firstChar++;
	    lastChar--;
	}

	nameChars = (lastChar - firstChar + 1);
	if (nameChars > 0) {
	    char *p = firstChar;
	    while (p != lastChar) {
		if (CHAR_TYPE(p, lastChar) != TCL_NORMAL) {
		    result = TCL_OUT_LINE_COMPILE;
		    goto done;
		}
		if (*p == '(') {
		    if (*lastChar == ')') { /* we have an array element */
			result = TCL_OUT_LINE_COMPILE;
			goto done; 
		    }
		}
		p++;
	    }
	}

	name = firstChar;
	localIndex = LookupCompiledLocal(name, nameChars,
                    /*createIfNew*/ 1, /*flagsIfCreated*/ VAR_SCALAR,
		    procPtr);
    }

    /*
     *==== At this point we believe we can compile the catch command ====
     */

    /*
     * Create and initialize a ExceptionRange record to hold information
     * about this catch command.
     */
    
    envPtr->excRangeDepth++;
    envPtr->maxExcRangeDepth =
	TclMax(envPtr->excRangeDepth, envPtr->maxExcRangeDepth);
    range = CreateExceptionRange(CATCH_EXCEPTION_RANGE, envPtr);

    /*
     * Emit the instruction to mark the start of the catch command.
     */
    
    TclEmitInstUInt4(INST_BEGIN_CATCH4, range, envPtr);
    
    /*
     * Inline compile the catch's body word: the command it controls. Also
     * register the body's starting PC offset and byte length in the
     * ExceptionRange record.
     */

    envPtr->excRangeArrayPtr[range].codeOffset = TclCurrCodeOffset();

    bodyStart = argInfo.startArray[0];
    bodyEnd   = argInfo.endArray[0];
    savedChar = *(bodyEnd+1);
    *(bodyEnd+1) = '\0';
    result = CompileCmdWordInline(interp, bodyStart, (bodyEnd+1),
	    flags, envPtr);
    *(bodyEnd+1) = savedChar;
    
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
	    char msg[60];
	    sprintf(msg, "\n    (\"catch\" body line %d)",
		    interp->errorLine);
            Tcl_AddObjErrorInfo(interp, msg, -1);
        }
	goto done;
    }
    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
    envPtr->excRangeArrayPtr[range].numCodeBytes =
	TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range].codeOffset;

    /*
     * Now emit the "no errors" epilogue code for the catch. First, if a
     * variable was specified, store the body's result into the
     * variable; otherwise, just discard the body's result. Then push
     * a "0" object as the catch command's "no error" TCL_OK result,
     * and jump around the "error case" epilogue code.
     */

    if (localIndex != -1) {
	if (localIndex <= 255) {
	    TclEmitInstUInt1(INST_STORE_SCALAR1, localIndex, envPtr);
	} else {
	    TclEmitInstUInt4(INST_STORE_SCALAR4, localIndex, envPtr);
	}
    }
    TclEmitOpcode(INST_POP, envPtr);

    objIndex = TclObjIndexForString("0", 1, /*allocStrRep*/ 0, /*inHeap*/ 0,
	    envPtr);
    objPtr = envPtr->objArrayPtr[objIndex];
    
    Tcl_InvalidateStringRep(objPtr);
    objPtr->internalRep.longValue = 0;
    objPtr->typePtr = &tclIntType;
    
    TclEmitPush(objIndex, envPtr);
    if (maxDepth == 0) {
	maxDepth = 1;	/* since we just pushed one object */
    }
    
    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);

    /*
     * Now emit the "error case" epilogue code. First, if a variable was
     * specified, emit instructions to push the interpreter's object result
     * and store it into the variable. Then emit an instruction to push the
     * nonzero error result. Note that the initial PC offset here is the
     * catch's error target.
     */

    envPtr->excRangeArrayPtr[range].catchOffset = TclCurrCodeOffset();
    if (localIndex != -1) {
	TclEmitOpcode(INST_PUSH_RESULT, envPtr);
	if (localIndex <= 255) {
	    TclEmitInstUInt1(INST_STORE_SCALAR1, localIndex, envPtr);
	} else {
	    TclEmitInstUInt4(INST_STORE_SCALAR4, localIndex, envPtr);
	}
	TclEmitOpcode(INST_POP, envPtr);
    }
    TclEmitOpcode(INST_PUSH_RETURN_CODE, envPtr);

    /*
     * Now that we know the target of the jump after the "no errors"
     * epilogue, update it with the correct distance. This is less
     * than 127 bytes.
     */

    jumpDist = (TclCurrCodeOffset() - jumpFixup.codeOffset);
    if (TclFixupForwardJump(envPtr, &jumpFixup, jumpDist, 127)) {
	panic("TclCompileCatchCmd: bad jump distance %d\n", jumpDist);
    }

    /*
     * Emit the instruction to mark the end of the catch command.
     */

    TclEmitOpcode(INST_END_CATCH, envPtr);

    done:
    if (numWords == 0) {
	envPtr->termOffset = 0;
    } else {
	envPtr->termOffset = (argInfo.endArray[numWords-1] + 1 - string);
    }
    if (range != -1) {		/* we compiled the catch command */
	envPtr->excRangeDepth--;
    }
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->maxStackDepth = maxDepth;
    FreeArgInfo(&argInfo);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileContinueCmd --
 *
 *	Procedure called to compile the "continue" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "continue" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileContinueCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    register char *src = string;/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int result = TCL_OK;
    
    /*
     * There should be no argument after the "continue".
     */

    type = CHAR_TYPE(src, lastChar);
    if (type != TCL_COMMAND_END) {
	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if (type != TCL_COMMAND_END) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "wrong # args: should be \"continue\"", -1);
	    result = TCL_ERROR;
	    goto done;
	}
    }

    /*
     * Emit a continue instruction.
     */

    TclEmitOpcode(INST_CONTINUE, envPtr);

    done:
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = 0;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileExprCmd --
 *
 *	Procedure called to compile the "expr" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK
 *	unless there was an error while parsing string. If an error occurs
 *	then the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the "expr" command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "expr" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileExprCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    ArgInfo argInfo;		/* Structure holding information about the
				 * start and end of each argument word. */
    Tcl_DString buffer;		/* Holds the concatenated expr command
				 * argument words. */
    int firstWord;		/* 1 if processing the first word; 0 if
				 * processing subsequent words. */
    char *first, *last;		/* Points to the first and last significant
				 * chars of the concatenated expression. */
    int inlineCode;		/* 1 if inline "optimistic" code is
				 * emitted for the expression; else 0. */
    int range = -1;		/* If we inline compile the concatenated
				 * expression, the index for its catch range
				 * record in the ExceptionRange array.
				 * Initialized to avoid compile warning. */
    JumpFixup jumpFixup;	/* Used to emit the "success" jump after
				 * the inline concat. expression's code. */
    char savedChar;		/* Holds the character termporarily replaced
				 * by a null character during compilation
				 * of the concatenated expression. */
    int numWords, objIndex, i, result;
    char *wordStart, *wordEnd, *p;
    char c;
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int saveExprIsJustVarRef = envPtr->exprIsJustVarRef;
    int saveExprIsComparison = envPtr->exprIsComparison;

    /*
     * Scan the words of the command and record the start and finish of
     * each argument word.
     */

    InitArgInfo(&argInfo);
    result = CollectArgInfo(interp, string, lastChar, flags, &argInfo);
    numWords = argInfo.numArgs;	  /* i.e., the # after the command name */
    if (result != TCL_OK) {
	goto done;
    }
    if (numWords == 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"expr arg ?arg ...?\"", -1);
        result = TCL_ERROR;
	goto done;
    }

    /*
     * If there is a single argument word and it is enclosed in {}s, we may
     * strip them off and safely compile the expr command into an inline
     * sequence of instructions using TclCompileExpr. We know these
     * instructions will have the right Tcl7.x expression semantics.
     *
     * Otherwise, if the word is not enclosed in {}s, or there are multiple
     * words, we may need to call the expr command (Tcl_ExprObjCmd) at
     * runtime. This recompiles the expression each time (typically) and so
     * is slow. However, there are some circumstances where we can still
     * compile inline instructions "optimistically" and check, during their
     * execution, for double substitutions (these appear as nonnumeric
     * operands). We check for any backslash or command substitutions. If
     * none appear, and only variable substitutions are found, we generate
     * inline instructions. If there is a compilation error, we must emit
     * instructions that return the error at runtime, since this is when
     * scripts in Tcl7.x would "see" the error.
     *
     * For now, if there are multiple words, or the single argument word is
     * not in {}s, we concatenate the argument words and strip off any
     * enclosing {}s or ""s. We call the expr command at runtime if
     * either command or backslash substitutions appear (but not if
     * only variable substitutions appear).
     */

    if (numWords == 1) {
	wordStart = argInfo.startArray[0]; /* start of 1st arg word */
	wordEnd   = argInfo.endArray[0];   /* last char of 1st arg word */
	if ((*wordStart == '{') && (*wordEnd == '}')) {
	    /*
	     * Simple case: a single argument word in {}'s. 
	     */

	    *wordEnd = '\0';
	    result = TclCompileExpr(interp, (wordStart + 1), wordEnd,
		    flags, envPtr);
	    *wordEnd = '}';
	    
	    envPtr->termOffset = (wordEnd + 1) - string;
	    envPtr->pushSimpleWords = savePushSimpleWords;
	    FreeArgInfo(&argInfo);
	    return result;
	}
    }
	
    /*
     * There are multiple words or no braces around the single word.
     * Concatenate the expression's argument words while stripping off
     * any enclosing {}s or ""s.
     */
    
    Tcl_DStringInit(&buffer);
    firstWord = 1;
    for (i = 0;  i < numWords;  i++) {
	wordStart = argInfo.startArray[i];
	wordEnd   = argInfo.endArray[i];
	if (((*wordStart == '{') && (*wordEnd == '}'))
	        || ((*wordStart == '"') && (*wordEnd == '"'))) {
	    wordStart++;
	    wordEnd--;
	}
	if (!firstWord) {
	    Tcl_DStringAppend(&buffer, " ", 1);
	}
	firstWord = 0;
	if (wordEnd >= wordStart) {
	    Tcl_DStringAppend(&buffer, wordStart, (wordEnd-wordStart+1));
	}
    }

    /*
     * Scan the concatenated expression's characters looking for any
     * '['s or (for now) '\'s. If any are found, just call the expr cmd
     * at runtime.
     */
    
    inlineCode = 1;
    first = Tcl_DStringValue(&buffer);
    last = first + (Tcl_DStringLength(&buffer) - 1);
    for (p = first;  p <= last;  p++) {
	c = *p;
	if ((c == '[') || (c == '\\')) {
	    inlineCode = 0;
	    break;
	}
    }

    if (inlineCode) {
	/*
	 * Inline compile the concatenated expression inside a "catch"
	 * so that a runtime error will back off to a (slow) call on expr.
	 */
	
	int startCodeOffset = (envPtr->codeNext - envPtr->codeStart);
	int startRangeNext = envPtr->excRangeArrayNext;
	
	/*
	 * Create a ExceptionRange record to hold information about the
	 * "catch" range for the expression's inline code. Also emit the
	 * instruction to mark the start of the range.
	 */
	
	envPtr->excRangeDepth++;
	envPtr->maxExcRangeDepth =
	        TclMax(envPtr->excRangeDepth, envPtr->maxExcRangeDepth);
	range = CreateExceptionRange(CATCH_EXCEPTION_RANGE, envPtr);
	TclEmitInstUInt4(INST_BEGIN_CATCH4, range, envPtr);
	
	/*
	 * Inline compile the concatenated expression.
	 */
	
	envPtr->excRangeArrayPtr[range].codeOffset = TclCurrCodeOffset();
	savedChar = *(last + 1);
	*(last + 1) = '\0';
	result = TclCompileExpr(interp, first, last + 1, flags, envPtr);
	*(last + 1) = savedChar;
	
	maxDepth = envPtr->maxStackDepth;
	envPtr->excRangeArrayPtr[range].numCodeBytes =
	        TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range].codeOffset;
	
	if ((result != TCL_OK) || (envPtr->exprIsJustVarRef)
	        || (envPtr->exprIsComparison)) {
	    /*
	     * We must call the expr command at runtime. Either there was a
	     * compilation error or the inline code might fail to give the
	     * correct 2 level substitution semantics.
	     *
	     * The latter can happen if the expression consisted of just a
	     * single variable reference or if the top-level operator in the
	     * expr is a comparison (which might operate on strings). In the
	     * latter case, the expression's code might execute (apparently)
	     * successfully but produce the wrong result. We depend on its
	     * execution failing if a second level of substitutions is
	     * required. This causes the "catch" code we generate around the
	     * inline code to back off to a call on the expr command at
	     * runtime, and this always gives the right 2 level substitution
	     * semantics.
	     *
	     * We delete the inline code by backing up the code pc and catch
	     * index. Note that if there was a compilation error, we can't
	     * report the error yet since the expression might be valid
	     * after the second round of substitutions.
	     */
	    
	    envPtr->codeNext = (envPtr->codeStart + startCodeOffset);
	    envPtr->excRangeArrayNext = startRangeNext;
	    inlineCode = 0;
	} else {
	    TclEmitOpcode(INST_END_CATCH, envPtr); /* for ok case */
	    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);
	    envPtr->excRangeArrayPtr[range].catchOffset = TclCurrCodeOffset();
	    TclEmitOpcode(INST_END_CATCH, envPtr); /* for error case */
	}
    }
	    
    /*
     * Emit code for the (slow) call on the expr command at runtime.
     * Generate code to concatenate the (already substituted once)
     * expression words with a space between each word.
     */
    
    for (i = 0;  i < numWords;  i++) {
	wordStart = argInfo.startArray[i];
	wordEnd   = argInfo.endArray[i];
	savedChar = *(wordEnd + 1);
	*(wordEnd + 1) = '\0';
	envPtr->pushSimpleWords = 1;
	result = CompileWord(interp, wordStart, wordEnd+1, flags, envPtr);
	*(wordEnd + 1) = savedChar;
	if (result != TCL_OK) {
	    break;
	}
	if (i != (numWords - 1)) {
	    objIndex = TclObjIndexForString(" ", 1, /*allocStrRep*/ 1,
					    /*inHeap*/ 0, envPtr);
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);
	} else {
	    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
	}
    }
    if (result == TCL_OK) {
	int concatItems = 2*numWords - 1;
	while (concatItems > 255) {
	    TclEmitInstUInt1(INST_CONCAT1, 255, envPtr);
	    concatItems -= 254;  /* concat pushes 1 obj, the result */
	}
	if (concatItems > 1) {
	    TclEmitInstUInt1(INST_CONCAT1, concatItems, envPtr);
	}
	TclEmitOpcode(INST_EXPR_STK, envPtr);
    }
    
    /*
     * If emitting inline code, update the target of the jump after
     * that inline code.
     */
    
    if (inlineCode) {
	int jumpDist = (TclCurrCodeOffset() - jumpFixup.codeOffset);
	if (TclFixupForwardJump(envPtr, &jumpFixup, jumpDist, 127)) {
	    /*
	     * Update the inline expression code's catch ExceptionRange
	     * target since it, being after the jump, also moved down.
	     */
	    
	    envPtr->excRangeArrayPtr[range].catchOffset += 3;
	}
    }
    Tcl_DStringFree(&buffer);
    
    done:
    if (numWords == 0) {
	envPtr->termOffset = 0;
    } else {
	envPtr->termOffset = (argInfo.endArray[numWords-1] + 1 - string);
    }
    if (range != -1) {		/* we inline compiled the expr */
	envPtr->excRangeDepth--;
    }
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->exprIsJustVarRef = saveExprIsJustVarRef;
    envPtr->exprIsComparison = saveExprIsComparison;
    envPtr->maxStackDepth = maxDepth;
    FreeArgInfo(&argInfo);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileForCmd --
 *
 *	Procedure called to compile the "for" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "for" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileForCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    ArgInfo argInfo;		/* Structure holding information about the
				 * start and end of each argument word. */
    int range1, range2;		/* Indexes in the ExceptionRange array of
				 * the loop ranges for this loop: one for
				 * its body and one for its "next" cmd. */
    JumpFixup jumpFalseFixup;	/* Used to update or replace the ifFalse
				 * jump after the "for" test when its target
				 * PC is determined. */
    int jumpBackDist, jumpBackOffset, testCodeOffset, jumpDist, objIndex;
    unsigned char *jumpPc;
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int numWords, result;

    /*
     * Scan the words of the command and record the start and finish of
     * each argument word.
     */

    InitArgInfo(&argInfo);
    result = CollectArgInfo(interp, string, lastChar, flags, &argInfo);
    numWords = argInfo.numArgs;	  /* i.e., the # after the command name */
    if (result != TCL_OK) {
	goto done;
    }
    if (numWords != 4) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"for start test next command\"", -1);
	result = TCL_ERROR;
	goto done;
    }

    /*
     * If the test expression is enclosed in quotes (""s), don't compile
     * the for inline. As a result of Tcl's two level substitution
     * semantics for expressions, the expression might have a constant
     * value that results in the loop never executing, or executing forever.
     * Consider "set x 0; for {} "$x > 5" {incr x} {}": the loop body 
     * should never be executed.
     */

    if (*(argInfo.startArray[1]) == '"') {
	result = TCL_OUT_LINE_COMPILE;
        goto done;
    }

    /*
     * Create a ExceptionRange record for the for loop's body. This is used
     * to implement break and continue commands inside the body.
     * Then create a second ExceptionRange record for the "next" command in 
     * order to implement break (but not continue) inside it. The second,
     * "next" ExceptionRange will always have a -1 continueOffset.
     */

    envPtr->excRangeDepth++;
    envPtr->maxExcRangeDepth =
	TclMax(envPtr->excRangeDepth, envPtr->maxExcRangeDepth);
    range1 = CreateExceptionRange(LOOP_EXCEPTION_RANGE, envPtr);
    range2 = CreateExceptionRange(LOOP_EXCEPTION_RANGE, envPtr);

    /*
     * Compile inline the next word: the initial command.
     */

    result = CompileCmdWordInline(interp, argInfo.startArray[0],
	    (argInfo.endArray[0] + 1), flags, envPtr);
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
            Tcl_AddObjErrorInfo(interp, "\n    (\"for\" initial command)", -1);
        }
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;

    /*
     * Discard the start command's result.
     */

    TclEmitOpcode(INST_POP, envPtr);

    /*
     * Compile the next word: the test expression.
     */

    testCodeOffset = TclCurrCodeOffset();
    envPtr->pushSimpleWords = 1;    /* process words normally */
    result = CompileExprWord(interp, argInfo.startArray[1],
	    (argInfo.endArray[1] + 1), flags, envPtr);
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
            Tcl_AddObjErrorInfo(interp, "\n    (\"for\" test expression)", -1);
        }
	goto done;
    }
    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);

    /*
     * Emit the jump that terminates the for command if the test was
     * false. We emit a one byte (relative) jump here, and replace it later
     * with a four byte jump if the jump target is > 127 bytes away.
     */

    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpFalseFixup);

    /*
     * Compile the loop body word inline. Also register the loop body's
     * starting PC offset and byte length in the its ExceptionRange record.
     */

    envPtr->excRangeArrayPtr[range1].codeOffset = TclCurrCodeOffset();
    result = CompileCmdWordInline(interp, argInfo.startArray[3],
	    (argInfo.endArray[3] + 1), flags, envPtr);
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
	    char msg[60];
	    sprintf(msg, "\n    (\"for\" body line %d)", interp->errorLine);
            Tcl_AddObjErrorInfo(interp, msg, -1);
        }
	goto done;
    }
    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
    envPtr->excRangeArrayPtr[range1].numCodeBytes =
	(TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range1].codeOffset);

    /*
     * Discard the loop body's result.
     */

    TclEmitOpcode(INST_POP, envPtr);

    /*
     * Finally, compile the "next" subcommand word inline.
     */

    envPtr->excRangeArrayPtr[range1].continueOffset = TclCurrCodeOffset();
    envPtr->excRangeArrayPtr[range2].codeOffset = TclCurrCodeOffset();
    result = CompileCmdWordInline(interp, argInfo.startArray[2],
	    (argInfo.endArray[2] + 1), flags, envPtr);
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
	    Tcl_AddObjErrorInfo(interp, "\n    (\"for\" loop-end command)", -1);
	}
	goto done;
    }
    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
    envPtr->excRangeArrayPtr[range2].numCodeBytes =
	TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range2].codeOffset;

    /*
     * Discard the "next" subcommand's result.
     */

    TclEmitOpcode(INST_POP, envPtr);
	
    /*
     * Emit the unconditional jump back to the test at the top of the for
     * loop. We generate a four byte jump if the distance to the test is
     * greater than 120 bytes. This is conservative, and ensures that we
     * won't have to replace this unconditional jump if we later need to
     * replace the ifFalse jump with a four-byte jump.
     */

    jumpBackOffset = TclCurrCodeOffset();
    jumpBackDist = (jumpBackOffset - testCodeOffset);
    if (jumpBackDist > 120) {
	TclEmitInstInt4(INST_JUMP4, /*offset*/ -jumpBackDist, envPtr);
    } else {
	TclEmitInstInt1(INST_JUMP1, /*offset*/ -jumpBackDist, envPtr);
    }

    /*
     * Now that we know the target of the jumpFalse after the test, update
     * it with the correct distance. If the distance is too great (more
     * than 127 bytes), replace that jump with a four byte instruction and
     * move the instructions after the jump down.
     */

    jumpDist = (TclCurrCodeOffset() - jumpFalseFixup.codeOffset);
    if (TclFixupForwardJump(envPtr, &jumpFalseFixup, jumpDist, 127)) {
	/*
	 * Update the loop body's ExceptionRange record since it moved down:
	 * i.e., increment both its start and continue PC offsets. Also,
	 * update the "next" command's start PC offset in its ExceptionRange
	 * record since it also moved down.
	 */

	envPtr->excRangeArrayPtr[range1].codeOffset += 3;
	envPtr->excRangeArrayPtr[range1].continueOffset += 3;
	envPtr->excRangeArrayPtr[range2].codeOffset += 3;

	/*
	 * Update the distance for the unconditional jump back to the test
	 * at the top of the loop since it moved down 3 bytes too.
	 */

	jumpBackOffset += 3;
	jumpPc = (envPtr->codeStart + jumpBackOffset);
	if (jumpBackDist > 120) {
	    jumpBackDist += 3;
	    TclUpdateInstInt4AtPc(INST_JUMP4, /*offset*/ -jumpBackDist,
				   jumpPc);
	} else {
	    jumpBackDist += 3;
	    TclUpdateInstInt1AtPc(INST_JUMP1, /*offset*/ -jumpBackDist,
				   jumpPc);
	}
    }
    
    /*
     * The current PC offset (after the loop's body and "next" subcommand)
     * is the loop's break target.
     */

    envPtr->excRangeArrayPtr[range1].breakOffset =
	envPtr->excRangeArrayPtr[range2].breakOffset = TclCurrCodeOffset();
    
    /*
     * Push an empty string object as the for command's result.
     */

    objIndex = TclObjIndexForString("", 0, /*allocStrRep*/ 0, /*inHeap*/ 0,
				    envPtr);
    TclEmitPush(objIndex, envPtr);
    if (maxDepth == 0) {
	maxDepth = 1;
    }

    done:
    if (numWords == 0) {
	envPtr->termOffset = 0;
    } else {
	envPtr->termOffset = (argInfo.endArray[numWords-1] + 1 - string);
    }
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->maxStackDepth = maxDepth;
    envPtr->excRangeDepth--;
    FreeArgInfo(&argInfo);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileForeachCmd --
 *
 *	Procedure called to compile the "foreach" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK if
 *	compilation was successful. If an error occurs then the
 *	interpreter's result contains a standard error message and TCL_ERROR
 *	is returned. If complation failed because the command is too complex
 *	for TclCompileForeachCmd, TCL_OUT_LINE_COMPILE is returned
 *	indicating that the foreach command should be compiled "out of line"
 *	by emitting code to invoke its command procedure at runtime.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the "while" command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "foreach" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileForeachCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Proc *procPtr = envPtr->procPtr;
    				/* Points to structure describing procedure
				 * containing foreach command, else NULL. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    ArgInfo argInfo;		/* Structure holding information about the
				 * start and end of each argument word. */
    int numLists = 0;		/* Count of variable (and value) lists. */
    int range;			/* Index in the ExceptionRange array of the
				 * ExceptionRange record for this loop. */
    ForeachInfo *infoPtr;	/* Points to the structure describing this
				 * foreach command. Stored in a AuxData
				 * record in the ByteCode. */
    JumpFixup jumpFalseFixup;	/* Used to update or replace the ifFalse
				 * jump after test when its target PC is
				 * determined. */
    char savedChar;		/* Holds the char from string termporarily
				 * replaced by a null character during
				 * processing of argument words. */
    int firstListTmp = -1;	/* If we decide to compile this foreach
				 * command, this is the index or "slot
				 * number" for the first temp var allocated
				 * in the proc frame that holds a pointer to
				 * a value list. Initialized to avoid a
				 * compiler warning. */
    int loopIterNumTmp;		/* If we decide to compile this foreach
				 * command, the index for the temp var that
				 * holds the current iteration count.  */
    char *varListStart, *varListEnd, *valueListStart, *bodyStart, *bodyEnd;
    unsigned char *jumpPc;
    int jumpDist, jumpBackDist, jumpBackOffset;
    int numWords, numVars, infoIndex, tmpIndex, objIndex, i, j, result;
    int savePushSimpleWords = envPtr->pushSimpleWords;

    /*
     * We parse the variable list argument words and create two arrays:
     *    varcList[i] gives the number of variables in the i-th var list
     *    varvList[i] points to an array of the names in the i-th var list
     * These are initially allocated on the stack, and are allocated on
     * the heap if necessary.
     */

#define STATIC_VAR_LIST_SIZE 4
    int varcListStaticSpace[STATIC_VAR_LIST_SIZE];
    char **varvListStaticSpace[STATIC_VAR_LIST_SIZE];

    int *varcList = varcListStaticSpace;
    char ***varvList = varvListStaticSpace;

    /*
     * If the foreach command is at global level (not in a procedure),
     * don't compile it inline: the payoff is too small.
     */

    if (procPtr == NULL) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Scan the words of the command and record the start and finish of
     * each argument word.
     */

    InitArgInfo(&argInfo);
    result = CollectArgInfo(interp, string, lastChar, flags, &argInfo);
    numWords = argInfo.numArgs;
    if (result != TCL_OK) {
	goto done;
    }
    if ((numWords < 3) || (numWords%2 != 1)) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"foreach varList list ?varList list ...? command\"", -1);
        result = TCL_ERROR;
	goto done;
    }

    /*
     * Initialize the varcList and varvList arrays; allocate heap storage,
     * if necessary, for them. Also make sure the variable names
     * have no substitutions: that they're just "var" or "var(elem)"
     */

    numLists = (numWords - 1)/2;
    if (numLists > STATIC_VAR_LIST_SIZE) {
        varcList = (int *) ckalloc(numLists * sizeof(int));
        varvList = (char ***) ckalloc(numLists * sizeof(char **));
    }
    for (i = 0;  i < numLists;  i++) {
        varcList[i] = 0;
        varvList[i] = (char **) NULL;
    }
    for (i = 0;  i < numLists;  i++) {
	/*
	 * Break each variable list into its component variables. If the
	 * lists is enclosed in {}s or ""s, strip them off first.
	 */

	varListStart = argInfo.startArray[i*2];
	varListEnd   = argInfo.endArray[i*2];
	if ((*varListStart == '{') || (*varListStart == '"')) {
	    if ((*varListEnd != '}') && (*varListEnd != '"')) {
		Tcl_ResetResult(interp);
		if (*varListStart == '"') {
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
			    "extra characters after close-quote", -1);
		} else {
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "extra characters after close-brace", -1);
		}
		result = TCL_ERROR;
		goto done;
	    }
	    varListStart++;
	    varListEnd--;
	}
	    
	/*
	 * NOTE: THIS NEEDS TO BE CONVERTED TO AN OBJECT LIST.
	 */

	savedChar = *(varListEnd+1);
	*(varListEnd+1) = '\0';
	result = Tcl_SplitList(interp, varListStart,
			       &varcList[i], &varvList[i]);
	*(varListEnd+1) = savedChar;
        if (result != TCL_OK) {
            goto done;
        }

	/*
	 * Check that each variable name has no substitutions and that
	 * it is a scalar name.
	 */

	numVars = varcList[i];
	for (j = 0;  j < numVars;  j++) {
	    char *varName = varvList[i][j];
	    char *p = varName;
	    while (*p != '\0') {
		if (CHAR_TYPE(p, p+1) != TCL_NORMAL) {
		    result = TCL_OUT_LINE_COMPILE;
		    goto done;
		}
		if (*p == '(') {
		    char *q = p;
		    do {
			q++;
		    } while (*q != '\0');
		    q--;
		    if (*q == ')') { /* we have an array element */
			result = TCL_OUT_LINE_COMPILE;
			goto done; 
		    }
		}
		p++;
	    }
	}
    }

    /*
     *==== At this point we believe we can compile the foreach command ====
     */

    /*
     * Create and initialize a ExceptionRange record to hold information
     * about this loop. This is used to implement break and continue.
     */
    
    envPtr->excRangeDepth++;
    envPtr->maxExcRangeDepth =
	TclMax(envPtr->excRangeDepth, envPtr->maxExcRangeDepth);
    range = CreateExceptionRange(LOOP_EXCEPTION_RANGE, envPtr);
    
    /*
     * Reserve (numLists + 1) temporary variables:
     *    - numLists temps for each value list
     *    - a temp for the "next value" index into each value list
     * At this time we don't try to reuse temporaries; if there are two
     * nonoverlapping foreach loops, they don't share any temps.
     */

    for (i = 0;  i < numLists;  i++) {
	tmpIndex = LookupCompiledLocal(NULL, /*nameChars*/ 0,
		/*createIfNew*/ 1, /*flagsIfCreated*/ VAR_SCALAR, procPtr);
	if (i == 0) {
	    firstListTmp = tmpIndex;
	}
    }
    loopIterNumTmp = LookupCompiledLocal(NULL, /*nameChars*/ 0,
	    /*createIfNew*/ 1, /*flagsIfCreated*/ VAR_SCALAR, procPtr);
    
    /*
     * Create and initialize the ForeachInfo and ForeachVarList data
     * structures describing this command. Then create a AuxData record
     * pointing to the ForeachInfo structure in the compilation environment.
     */

    infoPtr = (ForeachInfo *) ckalloc((unsigned)
	    (sizeof(ForeachInfo) + (numLists * sizeof(ForeachVarList *))));
    infoPtr->numLists = numLists;
    infoPtr->firstListTmp = firstListTmp;
    infoPtr->loopIterNumTmp = loopIterNumTmp;
    for (i = 0;  i < numLists;  i++) {
	ForeachVarList *varListPtr;
	numVars = varcList[i];
	varListPtr = (ForeachVarList *) ckalloc((unsigned)
	        sizeof(ForeachVarList) + numVars*sizeof(int));
	varListPtr->numVars = numVars;
	for (j = 0;  j < numVars;  j++) {
	    char *varName = varvList[i][j];
	    int nameChars = strlen(varName);
	    varListPtr->varIndexes[j] = LookupCompiledLocal(varName,
		    nameChars, /*createIfNew*/ 1,
                    /*flagsIfCreated*/ VAR_SCALAR, procPtr);
	}
	infoPtr->varLists[i] = varListPtr;
    }
    infoIndex = TclCreateAuxData((ClientData) infoPtr,
            DupForeachInfo, FreeForeachInfo, envPtr);

    /*
     * Emit code to store each value list into the associated temporary.
     */

    for (i = 0;  i < numLists;  i++) {
	valueListStart = argInfo.startArray[2*i + 1];
	envPtr->pushSimpleWords = 1;
	result = CompileWord(interp, valueListStart, lastChar, flags,
		envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);

	tmpIndex = (firstListTmp + i);
	if (tmpIndex <= 255) {
	    TclEmitInstUInt1(INST_STORE_SCALAR1, tmpIndex, envPtr);
	} else {
	    TclEmitInstUInt4(INST_STORE_SCALAR4, tmpIndex, envPtr);
	}
	TclEmitOpcode(INST_POP, envPtr);
    }

    /*
     * Emit the instruction to initialize the foreach loop's index temp var.
     */

    TclEmitInstUInt4(INST_FOREACH_START4, infoIndex, envPtr);
    
    /*
     * Emit the top of loop code that assigns each loop variable and checks
     * whether to terminate the loop.
     */

    envPtr->excRangeArrayPtr[range].continueOffset = TclCurrCodeOffset();
    TclEmitInstUInt4(INST_FOREACH_STEP4, infoIndex, envPtr);

    /*
     * Emit the ifFalse jump that terminates the foreach if all value lists
     * are exhausted. We emit a one byte (relative) jump here, and replace
     * it later with a four byte jump if the jump target is more than
     * 127 bytes away.
     */

    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpFalseFixup);
    
    /*
     * Compile the loop body word inline. Also register the loop body's
     * starting PC offset and byte length in the ExceptionRange record.
     */

    bodyStart = argInfo.startArray[numWords - 1];
    bodyEnd   = argInfo.endArray[numWords - 1];
    savedChar = *(bodyEnd+1);
    *(bodyEnd+1) = '\0';
    envPtr->excRangeArrayPtr[range].codeOffset = TclCurrCodeOffset();
    result = CompileCmdWordInline(interp, bodyStart, bodyEnd+1, flags,
	    envPtr);
    *(bodyEnd+1) = savedChar;
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
	    char msg[60];
	    sprintf(msg, "\n    (\"foreach\" body line %d)",
		    interp->errorLine);
            Tcl_AddObjErrorInfo(interp, msg, -1);
        }
	goto done;
    }
    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
    envPtr->excRangeArrayPtr[range].numCodeBytes =
	TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range].codeOffset;

    /*
     * Discard the loop body's result.
     */

    TclEmitOpcode(INST_POP, envPtr);
	
    /*
     * Emit the unconditional jump back to the test at the top of the
     * loop. We generate a four byte jump if the distance to the to of
     * the foreach is greater than 120 bytes. This is conservative and
     * ensures that we won't have to replace this unconditional jump if
     * we later need to replace the ifFalse jump with a four-byte jump.
     */

    jumpBackOffset = TclCurrCodeOffset();
    jumpBackDist =
	(jumpBackOffset - envPtr->excRangeArrayPtr[range].continueOffset);
    if (jumpBackDist > 120) {
	TclEmitInstInt4(INST_JUMP4, /*offset*/ -jumpBackDist, envPtr);
    } else {
	TclEmitInstInt1(INST_JUMP1, /*offset*/ -jumpBackDist, envPtr);
    }

    /*
     * Now that we know the target of the jumpFalse after the foreach_step
     * test, update it with the correct distance. If the distance is too
     * great (more than 127 bytes), replace that jump with a four byte
     * instruction and move the instructions after the jump down.
     */

    jumpDist = (TclCurrCodeOffset() - jumpFalseFixup.codeOffset);
    if (TclFixupForwardJump(envPtr, &jumpFalseFixup, jumpDist, 127)) {
	/*
	 * Update the loop body's starting PC offset since it moved down.
	 */

	envPtr->excRangeArrayPtr[range].codeOffset += 3;

	/*
	 * Update the distance for the unconditional jump back to the test
	 * at the top of the loop since it moved down 3 bytes too.
	 */

	jumpBackOffset += 3;
	jumpPc = (envPtr->codeStart + jumpBackOffset);
	if (jumpBackDist > 120) {
	    jumpBackDist += 3;
	    TclUpdateInstInt4AtPc(INST_JUMP4, /*offset*/ -jumpBackDist,
				   jumpPc);
	} else {
	    jumpBackDist += 3;
	    TclUpdateInstInt1AtPc(INST_JUMP1, /*offset*/ -jumpBackDist,
				   jumpPc);
	}
    }

    /*
     * The current PC offset (after the loop's body) is the loop's
     * break target.
     */

    envPtr->excRangeArrayPtr[range].breakOffset = TclCurrCodeOffset();
    
    /*
     * Push an empty string object as the foreach command's result.
     */

    objIndex = TclObjIndexForString("", 0, /*allocStrRep*/ 0, /*inHeap*/ 0,
				    envPtr);
    TclEmitPush(objIndex, envPtr);
    if (maxDepth == 0) {
	maxDepth = 1;
    }

    done:
    for (i = 0;  i < numLists;  i++) {
        if (varvList[i] != (char **) NULL) {
            ckfree((char *) varvList[i]);
        }
    }
    if (varcList != varcListStaticSpace) {
	ckfree((char *) varcList);
        ckfree((char *) varvList);
    }
    envPtr->termOffset = (argInfo.endArray[numWords-1] + 1 - string);
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->maxStackDepth = maxDepth;
    envPtr->excRangeDepth--;
    FreeArgInfo(&argInfo);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DupForeachInfo --
 *
 *	This procedure duplicates a ForeachInfo structure created as
 *	auxiliary data during the compilation of a foreach command.
 *
 * Results:
 *	A pointer to a newly allocated copy of the existing ForeachInfo
 *	structure is returned.
 *
 * Side effects:
 *	Storage for the copied ForeachInfo record is allocated. If the
 *	original ForeachInfo structure pointed to any ForeachVarList
 *	records, these structures are also copied and pointers to them
 *	are stored in the new ForeachInfo record.
 *
 *----------------------------------------------------------------------
 */

static ClientData
DupForeachInfo(clientData)
    ClientData clientData;	/* The foreach command's compilation
				 * auxiliary data to duplicate. */
{
    register ForeachInfo *srcPtr = (ForeachInfo *) clientData;
    ForeachInfo *dupPtr;
    register ForeachVarList *srcListPtr, *dupListPtr;
    int numLists = srcPtr->numLists;
    int numVars, i, j;
    
    dupPtr = (ForeachInfo *) ckalloc((unsigned)
	    (sizeof(ForeachInfo) + (numLists * sizeof(ForeachVarList *))));
    dupPtr->numLists = numLists;
    dupPtr->firstListTmp = srcPtr->firstListTmp;
    dupPtr->loopIterNumTmp = srcPtr->loopIterNumTmp;
    
    for (i = 0;  i < numLists;  i++) {
	srcListPtr = srcPtr->varLists[i];
	numVars = srcListPtr->numVars;
	dupListPtr = (ForeachVarList *) ckalloc((unsigned)
	        sizeof(ForeachVarList) + numVars*sizeof(int));
	dupListPtr->numVars = numVars;
	for (j = 0;  j < numVars;  j++) {
	    dupListPtr->varIndexes[j] =	srcListPtr->varIndexes[j];
	}
	dupPtr->varLists[i] = dupListPtr;
    }
    return (ClientData) dupPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeForeachInfo --
 *
 *	Procedure to free a ForeachInfo structure created as auxiliary data
 *	during the compilation of a foreach command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for the ForeachInfo structure pointed to by the ClientData
 *	argument is freed as is any ForeachVarList record pointed to by the
 *	ForeachInfo structure.
 *
 *----------------------------------------------------------------------
 */

static void
FreeForeachInfo(clientData)
    ClientData clientData;	/* The foreach command's compilation
				 * auxiliary data to free. */
{
    register ForeachInfo *infoPtr = (ForeachInfo *) clientData;
    register ForeachVarList *listPtr;
    int numLists = infoPtr->numLists;
    register int i;

    for (i = 0;  i < numLists;  i++) {
	listPtr = infoPtr->varLists[i];
	ckfree((char *) listPtr);
    }
    ckfree((char *) infoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileIfCmd --
 *
 *	Procedure called to compile the "if" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "if" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileIfCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    register char *src = string;/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    JumpFixupArray jumpFalseFixupArray;
    				/* Used to fix up the ifFalse jump after
				 * each "if"/"elseif" test when its target
				 * PC is determined. */
    JumpFixupArray jumpEndFixupArray;
				/* Used to fix up the unconditional jump
				 * after each "then" command to the end of
				 * the "if" when that PC is determined. */
    char *testSrcStart;
    int jumpDist, jumpFalseDist, jumpIndex, objIndex, j, result;
    unsigned char *ifFalsePc;
    unsigned char opCode;
    int savePushSimpleWords = envPtr->pushSimpleWords;

    /*
     * Loop compiling "expr then body" clauses after an "if" or "elseif".
     */

    TclInitJumpFixupArray(&jumpFalseFixupArray);
    TclInitJumpFixupArray(&jumpEndFixupArray);
    while (1) {	
	/*
	 * At this point in the loop, we have an expression to test, either
	 * the main expression or an expression following an "elseif".
	 * The arguments after the expression must be "then" (optional) and
	 * a script to execute if the expression is true.
	 */

	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if (type == TCL_COMMAND_END) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    "wrong # args: no expression after \"if\" argument", -1);
	    result = TCL_ERROR;
	    goto done;
	}

	/*
	 * Compile the "if"/"elseif" test expression.
	 */
	
	testSrcStart = src;
	envPtr->pushSimpleWords = 1;
	result = CompileExprWord(interp, src, lastChar, flags, envPtr);
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		Tcl_AddObjErrorInfo(interp,
		        "\n    (\"if\" test expression)", -1);
	    }
	    goto done;
	}
	maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
	src += envPtr->termOffset;

	/*
	 * Emit the ifFalse jump around the "then" part if the test was
	 * false. We emit a one byte (relative) jump here, and replace it
	 * later with a four byte jump if the jump target is more than 127
	 * bytes away. 
	 */

	if (jumpFalseFixupArray.next >= jumpFalseFixupArray.end) {
	    TclExpandJumpFixupArray(&jumpFalseFixupArray);
	}
	jumpIndex = jumpFalseFixupArray.next;
	jumpFalseFixupArray.next++;
	TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
		&(jumpFalseFixupArray.fixup[jumpIndex]));
	
	/*
	 * Skip over the optional "then" before the then clause.
	 */

	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if (type == TCL_COMMAND_END) {
	    char buf[100];
	    sprintf(buf, "wrong # args: no script following \"%.20s\" argument", testSrcStart);
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	    result = TCL_ERROR;
	    goto done;
	}
	if ((*src == 't') && (strncmp(src, "then", 4) == 0)) {
	    type = CHAR_TYPE(src+4, lastChar);
	    if ((type == TCL_SPACE) || (type == TCL_COMMAND_END)) {
		src += 4;
		AdvanceToNextWord(src, envPtr); 
		src += envPtr->termOffset;
		type = CHAR_TYPE(src, lastChar);
		if (type == TCL_COMMAND_END) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "wrong # args: no script following \"then\" argument", -1);
		    result = TCL_ERROR;
		    goto done;
		}
	    }
	}

	/*
	 * Compile the "then" command word inline.
	 */

	result = CompileCmdWordInline(interp, src, lastChar, flags, envPtr);
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		char msg[60];
		sprintf(msg, "\n    (\"if\" then script line %d)",
		        interp->errorLine);
		Tcl_AddObjErrorInfo(interp, msg, -1);
	    }
	    goto done;
	}
	maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
	src += envPtr->termOffset;

	/*
	 * Emit an unconditional jump to the end of the "if" command. We
	 * emit a one byte jump here, and replace it later with a four byte
	 * jump if the jump target is more than 127 bytes away. Note that
	 * both the jumpFalseFixupArray and the jumpEndFixupArray are
	 * indexed by the same index, "jumpIndex".
	 */

	if (jumpEndFixupArray.next >= jumpEndFixupArray.end) {
	    TclExpandJumpFixupArray(&jumpEndFixupArray);
	}
	jumpEndFixupArray.next++;
	TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP,
		&(jumpEndFixupArray.fixup[jumpIndex]));

 	/*
	 * Now that we know the target of the jumpFalse after the if test,
         * update it with the correct distance. We generate a four byte
	 * jump if the distance is greater than 120 bytes. This is
	 * conservative, and ensures that we won't have to replace this
	 * jump if we later also need to replace the preceeding
	 * unconditional jump to the end of the "if" with a four-byte jump.
         */

	jumpDist = (TclCurrCodeOffset() - jumpFalseFixupArray.fixup[jumpIndex].codeOffset);
	if (TclFixupForwardJump(envPtr,
	        &(jumpFalseFixupArray.fixup[jumpIndex]), jumpDist, 120)) {
	    /*
	     * Adjust the code offset for the unconditional jump at the end
	     * of the last "then" clause.
	     */

	    jumpEndFixupArray.fixup[jumpIndex].codeOffset += 3;
	}

	/*
	 * Check now for a "elseif" word. If we find one, keep looping.
	 */

	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if ((type != TCL_COMMAND_END)
	        && ((*src == 'e') && (strncmp(src, "elseif", 6) == 0))) {
	    type = CHAR_TYPE(src+6, lastChar);
	    if ((type == TCL_SPACE) || (type == TCL_COMMAND_END)) {
		src += 6;
		AdvanceToNextWord(src, envPtr); 
		src += envPtr->termOffset;
		type = CHAR_TYPE(src, lastChar);
		if (type == TCL_COMMAND_END) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "wrong # args: no expression after \"elseif\" argument", -1);
		    result = TCL_ERROR;
		    goto done;
		}
		continue;	  /* continue the "expr then body" loop */
	    }
	}
	break;
    } /* end of the "expr then body" loop */

    /*
     * No more "elseif expr then body" clauses. Check now for an "else"
     * clause. If there is another word, we are at its start.
     */

    if (type != TCL_COMMAND_END) {
	if ((*src == 'e') && (strncmp(src, "else", 4) == 0)) {
	    type = CHAR_TYPE(src+4, lastChar);
	    if ((type == TCL_SPACE) || (type == TCL_COMMAND_END)) {
		src += 4;
		AdvanceToNextWord(src, envPtr); 
		src += envPtr->termOffset;
		type = CHAR_TYPE(src, lastChar);
		if (type == TCL_COMMAND_END) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "wrong # args: no script following \"else\" argument", -1);
		    result = TCL_ERROR;
		    goto done;
		}
	    }
	}

	/*
	 * Compile the "else" command word inline.
	 */

	result = CompileCmdWordInline(interp, src, lastChar, flags, envPtr);
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		char msg[60];
		sprintf(msg, "\n    (\"if\" else script line %d)",
		        interp->errorLine);
		Tcl_AddObjErrorInfo(interp, msg, -1);
	    }
	    goto done;
	}
	maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
	src += envPtr->termOffset;
    
	/*
	 * Skip over white space until the end of the command.
	 */
	
	type = CHAR_TYPE(src, lastChar);
	if (type != TCL_COMMAND_END) {
	    AdvanceToNextWord(src, envPtr);
	    src += envPtr->termOffset;
	    type = CHAR_TYPE(src, lastChar);
	    if (type != TCL_COMMAND_END) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "wrong # args: extra words after \"else\" clause in \"if\" command", -1);
		result = TCL_ERROR;
		goto done;
	    }
	}
    } else {
	/*
	 * The "if" command has no "else" clause: push an empty string
	 * object as its result.
	 */

	objIndex = TclObjIndexForString("", 0, /*allocStrRep*/ 0,
		/*inHeap*/ 0, envPtr);
	TclEmitPush(objIndex, envPtr);
	maxDepth = TclMax(1, maxDepth);
    }

    /*
     * Now that we know the target of the unconditional jumps to the end of
     * the "if" command, update them with the correct distance. If the
     * distance is too great (> 127 bytes), replace the jump with a four
     * byte instruction and move instructions after the jump down.
     */
    
    for (j = jumpEndFixupArray.next;  j > 0;  j--) {
	jumpIndex = (j - 1);	/* i.e. process the closest jump first */
	jumpDist = (TclCurrCodeOffset() - jumpEndFixupArray.fixup[jumpIndex].codeOffset);
	if (TclFixupForwardJump(envPtr,
	        &(jumpEndFixupArray.fixup[jumpIndex]), jumpDist, 127)) {
	    /*
	     * Adjust the jump distance for the "ifFalse" jump that
	     * immediately preceeds this jump. We've moved it's target
	     * (just after this unconditional jump) three bytes down.
	     */

	    ifFalsePc = (envPtr->codeStart + jumpFalseFixupArray.fixup[jumpIndex].codeOffset);
	    opCode = *ifFalsePc;
	    if (opCode == INST_JUMP_FALSE1) {
		jumpFalseDist = TclGetInt1AtPtr(ifFalsePc + 1);
		jumpFalseDist += 3;
		TclStoreInt1AtPtr(jumpFalseDist, (ifFalsePc + 1));
	    } else if (opCode == INST_JUMP_FALSE4) {
		jumpFalseDist = TclGetInt4AtPtr(ifFalsePc + 1);
		jumpFalseDist += 3;
		TclStoreInt4AtPtr(jumpFalseDist, (ifFalsePc + 1));
	    } else {
		panic("TclCompileIfCmd: unexpected opcode updating ifFalse jump");
	    }
	}
    }
	
    /*
     * Free the jumpFixupArray array if malloc'ed storage was used.
     */

    done:
    TclFreeJumpFixupArray(&jumpFalseFixupArray);
    TclFreeJumpFixupArray(&jumpEndFixupArray);
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileIncrCmd --
 *
 *	Procedure called to compile the "incr" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while parsing string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the "incr" command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "incr" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileIncrCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Proc *procPtr = envPtr->procPtr;
    				/* Points to structure describing procedure
				 * containing incr command, else NULL. */
    register char *src = string;
    				/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int simpleVarName;		/* 1 if name is just sequence of chars with
                                 * an optional element name in parens. */
    char *name = NULL;		/* If simpleVarName, points to first char of
				 * variable name and nameChars is length.
				 * Otherwise NULL. */
    char *elName = NULL;	/* If simpleVarName, points to first char of
				 * element name and elNameChars is length.
				 * Otherwise NULL. */
    int nameChars = 0;		/* Length of the var name. Initialized to
				 * avoid a compiler warning. */
    int elNameChars = 0;	/* Length of array's element name, if any.
				 * Initialized to avoid a compiler
				 * warning. */
    int incrementGiven;		/* 1 if an increment amount was given. */
    int isImmIncrValue = 0;	/* 1 if increment amount is a literal
				 * integer in [-127..127]. */
    int immIncrValue = 0;	/* if isImmIncrValue is 1, the immediate
				 * integer value. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    int localIndex = -1;	/* Index of the variable in the current
				 * procedure's array of local variables.
				 * Otherwise -1 if not in a procedure or
				 * the variable wasn't found. */
    char savedChar;		/* Holds the character from string
				 * termporarily replaced by a null char
				 * during name processing. */
    int objIndex;		/* The object array index for a pushed
				 * object holding a name part. */
    int savePushSimpleWords = envPtr->pushSimpleWords;
    char *p;
    int i, result;

    /*
     * Parse the next word: the variable name. If it is "simple" (requires
     * no substitutions at runtime), divide it up into a simple "name" plus
     * an optional "elName". Otherwise, if not simple, just push the name.
     */

    AdvanceToNextWord(src, envPtr);
    src += envPtr->termOffset;
    type = CHAR_TYPE(src, lastChar);
    if (type == TCL_COMMAND_END) {
	badArgs:
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"incr varName ?increment?\"", -1);
	result = TCL_ERROR;
	goto done;
    }
    
    envPtr->pushSimpleWords = 0;
    result = CompileWord(interp, src, lastChar, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    simpleVarName = envPtr->wordIsSimple;
    if (simpleVarName) {
	name = src;
	nameChars = envPtr->numSimpleWordChars;
	if (type & (TCL_QUOTE | TCL_OPEN_BRACE)) {
	    name++;
	}
	elName = NULL;
	elNameChars = 0;
	p = name;
	for (i = 0;  i < nameChars;  i++) {
	    if (*p == '(') {
		char *openParen = p;
		p = (src + nameChars-1);	
		if (*p == ')') { /* last char is ')' => array reference */
		    nameChars = (openParen - name);
		    elName = openParen+1;
		    elNameChars = (p - elName);
		}
		break;
	    }
	    p++;
	}
    } else {
        maxDepth = envPtr->maxStackDepth;
    }
    src += envPtr->termOffset;

    /*
     * See if there is a next word. If so, we are incrementing the variable
     * by that value (which must be an integer).
     */

    incrementGiven = 0;
    type = CHAR_TYPE(src, lastChar);
    if (type != TCL_COMMAND_END) {
	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	incrementGiven = (type != TCL_COMMAND_END);
    }

    /*
     * Non-simple names have already been pushed. If this is a simple
     * variable, either push its name (if a global or an unknown local
     * variable) or look up the variable's local frame index. If a local is
     * not found, push its name and do the lookup at runtime. If this is an
     * array reference, also push the array element.
     */

    if (simpleVarName) {
	if (procPtr == NULL) {
	    savedChar = name[nameChars];
	    name[nameChars] = '\0';
	    objIndex = TclObjIndexForString(name, nameChars,
		    /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	    name[nameChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = 1;
	} else {
	    localIndex = LookupCompiledLocal(name, nameChars,
	            /*createIfNew*/ 0, /*flagsIfCreated*/ 0,
		    envPtr->procPtr);
	    if ((localIndex < 0) || (localIndex > 255)) {
		if (localIndex > 255) {	      /* we'll push the name */
		    localIndex = -1;
		}
		savedChar = name[nameChars];
		name[nameChars] = '\0';
		objIndex = TclObjIndexForString(name, nameChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		name[nameChars] = savedChar;
		TclEmitPush(objIndex, envPtr);
		maxDepth = 1;
	    } else {
		maxDepth = 0;
	    }
	}
	
	if (elName != NULL) {
	    /*
	     * Parse and push the array element's name. Perform
	     * substitutions on it, just as is done for quoted strings.
	     */

	    savedChar = elName[elNameChars];
	    elName[elNameChars] = '\0';
	    envPtr->pushSimpleWords = 1;
	    result = TclCompileQuotes(interp, elName, elName+elNameChars,
		    0, flags, envPtr);
	    elName[elNameChars] = savedChar;
	    if (result != TCL_OK) {
		char msg[200];
		sprintf(msg, "\n    (parsing index for array \"%.*s\")",
			TclMin(nameChars, 100), name);
		Tcl_AddObjErrorInfo(interp, msg, -1);
		goto done;
	    }
	    maxDepth += envPtr->maxStackDepth;
	}
    }

    /*
     * If an increment was given, push the new value.
     */
    
    if (incrementGiven) {
	type = CHAR_TYPE(src, lastChar);
	envPtr->pushSimpleWords = 0;
	result = CompileWord(interp, src, lastChar, flags, envPtr);
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		Tcl_AddObjErrorInfo(interp,
		        "\n    (increment expression)", -1);
	    }
	    goto done;
	}
	if (type & (TCL_QUOTE | TCL_OPEN_BRACE)) {
	    src++;
	}
	if (envPtr->wordIsSimple) {
	    /*
	     * See if the word represents an integer whose formatted
	     * representation is the same as the word (e.g., this is
	     * true for 123 and -1 but not for 00005). If so, just
	     * push an integer object.
	     */
	    
	    int isCompilableInt = 0;
	    int numChars = envPtr->numSimpleWordChars;
	    char savedChar = src[numChars];
	    char buf[40];
	    Tcl_Obj *objPtr;
	    long n;

	    src[numChars] = '\0';
	    if (TclLooksLikeInt(src)) {
		int code = TclGetLong(interp, src, &n);
		if (code == TCL_OK) {
		    if ((-127 <= n) && (n <= 127)) {
			isCompilableInt = 1;
			isImmIncrValue = 1;
			immIncrValue = n;
		    } else {
			TclFormatInt(buf, n);
			if (strcmp(src, buf) == 0) {
			    isCompilableInt = 1;
			    isImmIncrValue = 0;
			    objIndex = TclObjIndexForString(src, numChars,
                                /*allocStrRep*/ 0, /*inHeap*/ 0, envPtr);
			    objPtr = envPtr->objArrayPtr[objIndex];

			    Tcl_InvalidateStringRep(objPtr);
			    objPtr->internalRep.longValue = n;
			    objPtr->typePtr = &tclIntType;
			    
			    TclEmitPush(objIndex, envPtr);
			    maxDepth += 1;
			}
		    }
		} else {
		    Tcl_ResetResult(interp);
		}
	    }
	    if (!isCompilableInt) {
		objIndex = TclObjIndexForString(src, numChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		TclEmitPush(objIndex, envPtr);
		maxDepth += 1;
	    }
	    src[numChars] = savedChar;
	} else {
	    maxDepth += envPtr->maxStackDepth;
	}
	if (type & (TCL_QUOTE | TCL_OPEN_BRACE)) {
	    src += (envPtr->termOffset - 1); /* already advanced 1 above */
	} else {
	    src += envPtr->termOffset;
	}
    } else {			/* no incr amount given so use 1 */
	isImmIncrValue = 1;
	immIncrValue = 1;
    }
    
    /*
     * Now emit instructions to increment the variable.
     */

    if (simpleVarName) {
	if (elName == NULL) {  /* scalar */
	    if (localIndex >= 0) {
		if (isImmIncrValue) {
		    TclEmitInstUInt1(INST_INCR_SCALAR1_IMM, localIndex,
				    envPtr);
		    TclEmitInt1(immIncrValue, envPtr);
		} else {
		    TclEmitInstUInt1(INST_INCR_SCALAR1, localIndex, envPtr);
		}
	    } else {
		if (isImmIncrValue) {
		    TclEmitInstInt1(INST_INCR_SCALAR_STK_IMM, immIncrValue,
				   envPtr);
		} else {
		    TclEmitOpcode(INST_INCR_SCALAR_STK, envPtr);
		}
	    }
	} else {		/* array */
	    if (localIndex >= 0) {
		if (isImmIncrValue) {
		    TclEmitInstUInt1(INST_INCR_ARRAY1_IMM, localIndex,
				    envPtr);
		    TclEmitInt1(immIncrValue, envPtr);
		} else {
		    TclEmitInstUInt1(INST_INCR_ARRAY1, localIndex, envPtr);
		}
	    } else {
		if (isImmIncrValue) {
		    TclEmitInstInt1(INST_INCR_ARRAY_STK_IMM, immIncrValue,
				   envPtr);
		} else {
		    TclEmitOpcode(INST_INCR_ARRAY_STK, envPtr);
		}
	    }
	}
    } else {			/* non-simple variable name */
	if (isImmIncrValue) {
	    TclEmitInstInt1(INST_INCR_STK_IMM, immIncrValue, envPtr);
	} else {
	    TclEmitOpcode(INST_INCR_STK, envPtr);
	}
    }
	
    /*
     * Skip over white space until the end of the command.
     */

    type = CHAR_TYPE(src, lastChar);
    if (type != TCL_COMMAND_END) {
	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if (type != TCL_COMMAND_END) {
	    goto badArgs;
	}
    }

    done:
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileSetCmd --
 *
 *	Procedure called to compile the "set" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is normally TCL_OK
 *	unless there was an error while parsing string. If an error occurs
 *	then the interpreter's result contains a standard error message. If
 *	complation fails because the set command requires a second level of
 *	substitutions, TCL_OUT_LINE_COMPILE is returned indicating that the
 *	set command should be compiled "out of line" by emitting code to
 *	invoke its command procedure (Tcl_SetCmd) at runtime.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the incr command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "set" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileSetCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Proc *procPtr = envPtr->procPtr;
				/* Points to structure describing procedure
				 * containing the set command, else NULL. */
    ArgInfo argInfo;		/* Structure holding information about the
				 * start and end of each argument word. */
    int simpleVarName;		/* 1 if name is just sequence of chars with
                                 * an optional element name in parens. */
    char *elName = NULL;	/* If simpleVarName, points to first char of
				 * element name and elNameChars is length.
				 * Otherwise NULL. */
    int isAssignment;		/* 1 if assigning value to var, else 0. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    int localIndex = -1;	/* Index of the variable in the current
				 * procedure's array of local variables.
				 * Otherwise -1 if not in a procedure, the
				 * name contains "::"s, or the variable
				 * wasn't found. */
    char savedChar;		/* Holds the character from string
				 * termporarily replaced by a null char
				 * during name processing. */
    int objIndex = -1;		/* The object array index for a pushed
				 * object holding a name part. Initialized
				 * to avoid a compiler warning. */
    char *wordStart, *p;
    int numWords, isCompilableInt, i, result;
    Tcl_Obj *objPtr;
    int savePushSimpleWords = envPtr->pushSimpleWords;

    /*
     * Scan the words of the command and record the start and finish of
     * each argument word.
     */

    InitArgInfo(&argInfo);
    result = CollectArgInfo(interp, string, lastChar, flags, &argInfo);
    numWords = argInfo.numArgs;	  /* i.e., the # after the command name */
    if (result != TCL_OK) {
	goto done;
    }
    if ((numWords < 1) || (numWords > 2)) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"set varName ?newValue?\"", -1);
        result = TCL_ERROR;
	goto done;
    }
    isAssignment = (numWords == 2);

    /*
     * Parse the next word: the variable name. If the name is enclosed in
     * quotes or braces, we return TCL_OUT_LINE_COMPILE and call the set
     * command procedure at runtime since this makes sure that a second
     * round of substitutions is done properly. 
     */

    wordStart = argInfo.startArray[0]; /* start of 1st arg word: varname */
    if ((*wordStart == '{') || (*wordStart == '"')) {
	result = TCL_OUT_LINE_COMPILE;
	goto done;
    }

    /*
     * Check whether the name is "simple": requires no substitutions at
     * runtime.
     */
    
    envPtr->pushSimpleWords = 0;
    result = CompileWord(interp, wordStart, argInfo.endArray[0] + 1,
	    flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    simpleVarName = envPtr->wordIsSimple;
    
    if (!simpleVarName) {
	/*
	 * The name isn't simple. CompileWord already pushed it.
	 */
	
	maxDepth = envPtr->maxStackDepth;
    } else {
	char *name;		/* If simpleVarName, points to first char of
				 * variable name and nameChars is length.
				 * Otherwise NULL. */
	int nameChars;		/* Length of the var name. */
	int nameHasNsSeparators = 0;
				/* Set 1 if name contains "::"s. */
	int elNameChars;	/* Length of array's element name if any. */

	/*
	 * A simple name. First divide it up into "name" plus "elName"
	 * for an array element name, if any.
	 */
	
	name = wordStart;
	nameChars = envPtr->numSimpleWordChars;
	elName = NULL;
	elNameChars = 0;
	
	p = name;
	for (i = 0;  i < nameChars;  i++) {
	    if (*p == '(') {
		char *openParen = p;
		p = (name + nameChars-1);	
		if (*p == ')') { /* last char is ')' => array reference */
		    nameChars = (openParen - name);
		    elName = openParen+1;
		    elNameChars = (p - elName);
		}
		break;
	    }
	    p++;
	}

	/*
	 * Determine if name has any namespace separators (::'s).
	 */

	p = name;
	for (i = 0;  i < nameChars;  i++) {
	    if ((*p == ':') && ((i+1) < nameChars) && (*(p+1) == ':')) {
		nameHasNsSeparators = 1;
		break;
	    }
	    p++;
	}

	/*
	 * Now either push the name or determine its index in the array of
	 * local variables in a procedure frame. Note that if we are
	 * compiling a procedure the variable must be local unless its
	 * name has namespace separators ("::"s). Note also that global
	 * variables are implemented by a local variable that "points" to
	 * the real global. There are two cases:
	 *   1) We are not compiling a procedure body. Push the global
	 *      variable's name and do the lookup at runtime.
	 *   2) We are compiling a procedure and the name has "::"s.
	 *	Push the namespace variable's name and do the lookup at
	 *	runtime.
	 *   3) We are compiling a procedure and the name has no "::"s.
	 *	If the variable has already been allocated an local index,
	 *	just look it up. If the variable is unknown and we are
	 *	doing an assignment, allocate a new index. Otherwise,
	 *	push the name and try to do the lookup at runtime.
	 */

	if ((procPtr == NULL) || nameHasNsSeparators) {
	    savedChar = name[nameChars];
	    name[nameChars] = '\0';
	    objIndex = TclObjIndexForString(name, nameChars,
		    /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	    name[nameChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    maxDepth = 1;
	} else {
	    localIndex = LookupCompiledLocal(name, nameChars,
	            /*createIfNew*/ isAssignment,
                    /*flagsIfCreated*/
			((elName == NULL)? VAR_SCALAR : VAR_ARRAY),
		    envPtr->procPtr);
	    if (localIndex >= 0) {
		maxDepth = 0;
	    } else {
		savedChar = name[nameChars];
		name[nameChars] = '\0';
		objIndex = TclObjIndexForString(name, nameChars,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		name[nameChars] = savedChar;
		TclEmitPush(objIndex, envPtr);
		maxDepth = 1;
	    }
	}

	/*
	 * If we are dealing with a reference to an array element, push the
	 * array element. Perform substitutions on it, just as is done
	 * for quoted strings.
	 */
	
	if (elName != NULL) {
	    savedChar = elName[elNameChars];
	    elName[elNameChars] = '\0';
	    envPtr->pushSimpleWords = 1;
	    result = TclCompileQuotes(interp, elName, elName+elNameChars,
		    0, flags, envPtr);
	    elName[elNameChars] = savedChar;
	    if (result != TCL_OK) {
		char msg[200];
		sprintf(msg, "\n    (parsing index for array \"%.*s\")",
			TclMin(nameChars, 100), name);
		Tcl_AddObjErrorInfo(interp, msg, -1);
		goto done;
	    }
	    maxDepth += envPtr->maxStackDepth;
	}
    }

    /*
     * If we are doing an assignment, push the new value.
     */
    
    if (isAssignment) {
	wordStart = argInfo.startArray[1]; /* start of 2nd arg word */
	envPtr->pushSimpleWords = 0;       /* we will handle simple words */
	result = CompileWord(interp, wordStart,	argInfo.endArray[1] + 1,
		flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	if (!envPtr->wordIsSimple) {
	    /*
	     * The value isn't simple. CompileWord already pushed it.
	     */

	    maxDepth += envPtr->maxStackDepth;
	} else {
	    /*
	     * The value is simple. See if the word represents an integer
	     * whose formatted representation is the same as the word (e.g.,
	     * this is true for 123 and -1 but not for 00005). If so, just
	     * push an integer object.
	     */
	    
	    char buf[40];
	    long n;

	    p = wordStart;
	    if ((*wordStart == '"') || (*wordStart == '{')) {
		p++;
	    }
	    savedChar = p[envPtr->numSimpleWordChars];
	    p[envPtr->numSimpleWordChars] = '\0';
	    isCompilableInt = 0;
	    if (TclLooksLikeInt(p)) {
		int code = TclGetLong(interp, p, &n);
		if (code == TCL_OK) {
		    TclFormatInt(buf, n);
		    if (strcmp(p, buf) == 0) {
			isCompilableInt = 1;
			objIndex = TclObjIndexForString(p,
				envPtr->numSimpleWordChars,
                                /*allocStrRep*/ 0, /*inHeap*/ 0, envPtr);
			objPtr = envPtr->objArrayPtr[objIndex];

			Tcl_InvalidateStringRep(objPtr);
			objPtr->internalRep.longValue = n;
			objPtr->typePtr = &tclIntType;
		    }
		} else {
		    Tcl_ResetResult(interp);
		}
	    }
	    if (!isCompilableInt) {
		objIndex = TclObjIndexForString(p,
			envPtr->numSimpleWordChars, /*allocStrRep*/ 1,
			/*inHeap*/ 0, envPtr);
	    }
	    p[envPtr->numSimpleWordChars] = savedChar;
	    TclEmitPush(objIndex, envPtr);
	    maxDepth += 1;
	}
    }
    
    /*
     * Now emit instructions to set/retrieve the variable.
     */

    if (simpleVarName) {
	if (elName == NULL) {  /* scalar */
	    if (localIndex >= 0) {
		if (localIndex <= 255) {
		    TclEmitInstUInt1((isAssignment?
			     INST_STORE_SCALAR1 : INST_LOAD_SCALAR1),
			localIndex, envPtr);
		} else {
		    TclEmitInstUInt4((isAssignment?
			     INST_STORE_SCALAR4 : INST_LOAD_SCALAR4),
			localIndex, envPtr);
		}
	    } else {
		TclEmitOpcode((isAssignment?
			     INST_STORE_SCALAR_STK : INST_LOAD_SCALAR_STK),
			    envPtr);
	    }
	} else {		/* array */
	    if (localIndex >= 0) {
		if (localIndex <= 255) {
		    TclEmitInstUInt1((isAssignment?
			     INST_STORE_ARRAY1 : INST_LOAD_ARRAY1),
			localIndex, envPtr);
		} else {
		    TclEmitInstUInt4((isAssignment?
			     INST_STORE_ARRAY4 : INST_LOAD_ARRAY4),
			localIndex, envPtr);
		}
	    } else {
		TclEmitOpcode((isAssignment?
			     INST_STORE_ARRAY_STK : INST_LOAD_ARRAY_STK),
			    envPtr);
	    }
	}
    } else {			/* non-simple variable name */
	TclEmitOpcode((isAssignment? INST_STORE_STK : INST_LOAD_STK), envPtr);
    }
	
    done:
    if (numWords == 0) {
	envPtr->termOffset = 0;
    } else {
	envPtr->termOffset = (argInfo.endArray[numWords-1] + 1 - string);
    }
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->maxStackDepth = maxDepth;
    FreeArgInfo(&argInfo);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileWhileCmd --
 *
 *	Procedure called to compile the "while" command.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK if
 *	compilation was successful. If an error occurs then the
 *	interpreter's result contains a standard error message and TCL_ERROR
 *	is returned. If compilation failed because the command is too
 *	complex for TclCompileWhileCmd, TCL_OUT_LINE_COMPILE is returned
 *	indicating that the while command should be compiled "out of line"
 *	by emitting code to invoke its command procedure at runtime.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the "while" command.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the "while" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileWhileCmd(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    register char *src = string;/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    int range;			/* Index in the ExceptionRange array of the
				 * ExceptionRange record for this loop. */
    JumpFixup jumpFalseFixup;	/* Used to update or replace the ifFalse
				 * jump after test when its target PC is
				 * determined. */
    unsigned char *jumpPc;
    int jumpDist, jumpBackDist, jumpBackOffset, objIndex, result;
    int savePushSimpleWords = envPtr->pushSimpleWords;

    envPtr->excRangeDepth++;
    envPtr->maxExcRangeDepth =
	TclMax(envPtr->excRangeDepth, envPtr->maxExcRangeDepth);

    /*
     * Create and initialize a ExceptionRange record to hold information
     * about this loop. This is used to implement break and continue.
     */

    range = CreateExceptionRange(LOOP_EXCEPTION_RANGE, envPtr);
    envPtr->excRangeArrayPtr[range].continueOffset = TclCurrCodeOffset();

    AdvanceToNextWord(src, envPtr);
    src += envPtr->termOffset;
    type = CHAR_TYPE(src, lastChar);
    if (type == TCL_COMMAND_END) {
	badArgs:
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "wrong # args: should be \"while test command\"", -1);
	result = TCL_ERROR;
	goto done;
    }

    /*
     * If the test expression is enclosed in quotes (""s), don't compile
     * the while inline. As a result of Tcl's two level substitution
     * semantics for expressions, the expression might have a constant
     * value that results in the loop never executing, or executing forever.
     * Consider "set x 0; while "$x < 5" {incr x}": the loop body should
     * never be executed.
     */

    if (*src == '"') {
	result = TCL_OUT_LINE_COMPILE;
        goto done;
    }

    /*
     * Compile the next word: the test expression.
     */

    envPtr->pushSimpleWords = 1;
    result = CompileExprWord(interp, src, lastChar, flags, envPtr);
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
            Tcl_AddObjErrorInfo(interp, "\n    (\"while\" test expression)", -1);
        }
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;
    src += envPtr->termOffset;

    /*
     * Emit the ifFalse jump that terminates the while if the test was
     * false. We emit a one byte (relative) jump here, and replace it
     * later with a four byte jump if the jump target is more than
     * 127 bytes away.
     */

    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpFalseFixup);
    
    /*
     * Compile the loop body word inline. Also register the loop body's
     * starting PC offset and byte length in the its ExceptionRange record.
     */

    AdvanceToNextWord(src, envPtr);
    src += envPtr->termOffset;
    type = CHAR_TYPE(src, lastChar);
    if (type == TCL_COMMAND_END) {
	goto badArgs;
    }

    envPtr->excRangeArrayPtr[range].codeOffset = TclCurrCodeOffset();
    result = CompileCmdWordInline(interp, src, lastChar,
	    flags, envPtr);
    if (result != TCL_OK) {
	if (result == TCL_ERROR) {
	    char msg[60];
	    sprintf(msg, "\n    (\"while\" body line %d)", interp->errorLine);
            Tcl_AddObjErrorInfo(interp, msg, -1);
        }
	goto done;
    }
    maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
    src += envPtr->termOffset;
    envPtr->excRangeArrayPtr[range].numCodeBytes =
	(TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range].codeOffset);

    /*
     * Discard the loop body's result.
     */

    TclEmitOpcode(INST_POP, envPtr);
	
    /*
     * Emit the unconditional jump back to the test at the top of the
     * loop. We generate a four byte jump if the distance to the while's
     * test is greater than 120 bytes. This is conservative, and ensures
     * that we won't have to replace this unconditional jump if we later
     * need to replace the ifFalse jump with a four-byte jump.
     */

    jumpBackOffset = TclCurrCodeOffset();
    jumpBackDist =
	(jumpBackOffset - envPtr->excRangeArrayPtr[range].continueOffset);
    if (jumpBackDist > 120) {
	TclEmitInstInt4(INST_JUMP4, /*offset*/ -jumpBackDist, envPtr);
    } else {
	TclEmitInstInt1(INST_JUMP1, /*offset*/ -jumpBackDist, envPtr);
    }

    /*
     * Now that we know the target of the jumpFalse after the test, update
     * it with the correct distance. If the distance is too great (more
     * than 127 bytes), replace that jump with a four byte instruction and
     * move the instructions after the jump down. 
     */

    jumpDist = (TclCurrCodeOffset() - jumpFalseFixup.codeOffset);
    if (TclFixupForwardJump(envPtr, &jumpFalseFixup, jumpDist, 127)) {
	/*
	 * Update the loop body's starting PC offset since it moved down.
	 */

	envPtr->excRangeArrayPtr[range].codeOffset += 3;

	/*
	 * Update the distance for the unconditional jump back to the test
	 * at the top of the loop since it moved down 3 bytes too.
	 */

	jumpBackOffset += 3;
	jumpPc = (envPtr->codeStart + jumpBackOffset);
	if (jumpBackDist > 120) {
	    jumpBackDist += 3;
	    TclUpdateInstInt4AtPc(INST_JUMP4, /*offset*/ -jumpBackDist,
				   jumpPc);
	} else {
	    jumpBackDist += 3;
	    TclUpdateInstInt1AtPc(INST_JUMP1, /*offset*/ -jumpBackDist,
				   jumpPc);
	}
    }

    /*
     * The current PC offset (after the loop's body) is the loop's
     * break target.
     */

    envPtr->excRangeArrayPtr[range].breakOffset = TclCurrCodeOffset();
    
    /*
     * Push an empty string object as the while command's result.
     */

    objIndex = TclObjIndexForString("", 0, /*allocStrRep*/ 0, /*inHeap*/ 0,
				    envPtr);
    TclEmitPush(objIndex, envPtr);
    if (maxDepth == 0) {
	maxDepth = 1;
    }

    /*
     * Skip over white space until the end of the command.
     */

    type = CHAR_TYPE(src, lastChar);
    if (type != TCL_COMMAND_END) {
	AdvanceToNextWord(src, envPtr);
	src += envPtr->termOffset;
	type = CHAR_TYPE(src, lastChar);
	if (type != TCL_COMMAND_END) {
	    goto badArgs;
	}
    }

    done:
    envPtr->termOffset = (src - string);
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->maxStackDepth = maxDepth;
    envPtr->excRangeDepth--;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileExprWord --
 *
 *	Procedure that compiles a Tcl expression in a command word.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while compiling string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the "expr" word.
 *
 * Side effects:
 *	Instructions are added to envPtr to evaluate the expression word
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileExprWord(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    register char *src = string;/* Points to current source char. */
    register int type;          /* Current char's CHAR_TYPE type. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int nestedCmd = (flags & TCL_BRACKET_TERM);
				/* 1 if script being compiled is a nested
				 * command and is terminated by a ']';
				 * otherwise 0. */
    char *first, *last;		/* Points to the first and last significant
				 * characters of the word. */
    char savedChar;		/* Holds the character termporarily replaced
				 * by a null character during compilation
				 * of the expression. */
    int inlineCode;		/* 1 if inline "optimistic" code is
				 * emitted for the expression; else 0. */
    int range = -1;		/* If we inline compile an un-{}'d
				 * expression, the index for its catch range
				 * record in the ExceptionRange array.
				 * Initialized to avoid compile warning. */
    JumpFixup jumpFixup;	/* Used to emit the "success" jump after
				 * the inline expression code. */
    char *p;
    char c;
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int saveExprIsJustVarRef = envPtr->exprIsJustVarRef;
    int saveExprIsComparison = envPtr->exprIsComparison;
    int numChars, result;

    /*
     * Skip over leading white space.
     */

    AdvanceToNextWord(src, envPtr);
    src += envPtr->termOffset;
    type = CHAR_TYPE(src, lastChar);
    if (type == TCL_COMMAND_END) {
	badArgs:
	Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    "malformed expression word", -1);
	result = TCL_ERROR;
	goto done;
    }

    /*
     * If the word is enclosed in {}s, we may strip them off and safely
     * compile the expression into an inline sequence of instructions using
     * TclCompileExpr. We know these instructions will have the right Tcl7.x
     * expression semantics.
     *
     * Otherwise, if the word is not enclosed in {}s, we may need to call
     * the expr command (Tcl_ExprObjCmd) at runtime. This recompiles the
     * expression each time (typically) and so is slow. However, there are
     * some circumstances where we can still compile inline instructions
     * "optimistically" and check, during their execution, for double
     * substitutions (these appear as nonnumeric operands). We check for any
     * backslash or command substitutions. If none appear, and only variable
     * substitutions are found, we generate inline instructions.
     *
     * For now, if the expression is not enclosed in {}s, we call the expr
     * command at runtime if either command or backslash substitutions
     * appear (but not if only variable substitutions appear).
     */

    if (*src == '{') {
	/*
	 * Inline compile the expression inside {}s.
	 */
	
	first = src+1;
	src = TclWordEnd(src, lastChar, nestedCmd, NULL);
	if (*src == 0) {
	    goto badArgs;
	}
	if (*src != '}') {
	    goto badArgs;
	}
	last = (src-1);

	numChars = (last - first + 1);
	savedChar = first[numChars];
	first[numChars] = '\0';
	result = TclCompileExpr(interp, first, first+numChars,
		flags, envPtr);
	first[numChars] = savedChar;

	src++;
	maxDepth = envPtr->maxStackDepth;
    } else {
	/*
	 * No braces. If the expression is enclosed in '"'s, call the expr
	 * cmd at runtime. Otherwise, scan the word's characters looking for
	 * any '['s or (for now) '\'s. If any are found, just call expr cmd
	 * at runtime.
	 */

	first = src;
	last = TclWordEnd(first, lastChar, nestedCmd, NULL);
	if (*last == 0) {	/* word doesn't end properly. */
	    src = last;
	    goto badArgs;
	}

	inlineCode = 1;
	if ((*first == '"') && (*last == '"')) {
	    inlineCode = 0;
	} else {
	    for (p = first;  p <= last;  p++) {
		c = *p;
		if ((c == '[') || (c == '\\')) {
		    inlineCode = 0;
		    break;
		}
	    }
	}
	
	if (inlineCode) {
	    /*
	     * Inline compile the expression inside a "catch" so that a
	     * runtime error will back off to make a (slow) call on expr.
	     */

	    int startCodeOffset = (envPtr->codeNext - envPtr->codeStart);
	    int startRangeNext = envPtr->excRangeArrayNext;

	    /*
	     * Create a ExceptionRange record to hold information about
	     * the "catch" range for the expression's inline code. Also
	     * emit the instruction to mark the start of the range.
	     */

	    envPtr->excRangeDepth++;
	    envPtr->maxExcRangeDepth =
		TclMax(envPtr->excRangeDepth, envPtr->maxExcRangeDepth);
	    range = CreateExceptionRange(CATCH_EXCEPTION_RANGE, envPtr);
	    TclEmitInstUInt4(INST_BEGIN_CATCH4, range, envPtr);

	    /*
	     * Inline compile the expression.
	     */

	    envPtr->excRangeArrayPtr[range].codeOffset = TclCurrCodeOffset();
	    numChars = (last - first + 1);
	    savedChar = first[numChars];
	    first[numChars] = '\0';
	    result = TclCompileExpr(interp, first, first + numChars,
		    flags, envPtr);
	    first[numChars] = savedChar;
	    
	    envPtr->excRangeArrayPtr[range].numCodeBytes =
		TclCurrCodeOffset() - envPtr->excRangeArrayPtr[range].codeOffset;

	    if ((result != TCL_OK) || (envPtr->exprIsJustVarRef)
	            || (envPtr->exprIsComparison)) {
		/*
		 * We must call the expr command at runtime. Either there
		 * was a compilation error or the inline code might fail to
		 * give the correct 2 level substitution semantics.
		 *
		 * The latter can happen if the expression consisted of just
		 * a single variable reference or if the top-level operator
		 * in the expr is a comparison (which might operate on
		 * strings). In the latter case, the expression's code might
		 * execute (apparently) successfully but produce the wrong
		 * result. We depend on its execution failing if a second
		 * level of substitutions is required. This causes the
		 * "catch" code we generate around the inline code to back
		 * off to a call on the expr command at runtime, and this
		 * always gives the right 2 level substitution semantics.
		 *
		 * We delete the inline code by backing up the code pc and
		 * catch index. Note that if there was a compilation error,
		 * we can't report the error yet since the expression might
		 * be valid after the second round of substitutions.
		 */
		
		envPtr->codeNext = (envPtr->codeStart + startCodeOffset);
		envPtr->excRangeArrayNext = startRangeNext;
		inlineCode = 0;
	    } else {
		TclEmitOpcode(INST_END_CATCH, envPtr);
		TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);
		envPtr->excRangeArrayPtr[range].catchOffset = TclCurrCodeOffset();
	    }
	}
	    
	/*
	 * Arrange to call expr at runtime with the (already substituted
	 * once) expression word on the stack.
	 */

	envPtr->pushSimpleWords = 1;
	result = CompileWord(interp, first, lastChar, flags, envPtr);
	src += envPtr->termOffset;
	maxDepth = envPtr->maxStackDepth;
	if (result == TCL_OK) {
	    TclEmitOpcode(INST_EXPR_STK, envPtr);
	}

	/*
	 * If emitting inline code for this non-{}'d expression, update
	 * the target of the jump after that inline code.
	 */

	if (inlineCode) {
	    int jumpDist = (TclCurrCodeOffset() - jumpFixup.codeOffset);
	    if (TclFixupForwardJump(envPtr, &jumpFixup, jumpDist, 127)) {
		/*
		 * Update the inline expression code's catch ExceptionRange
		 * target since it, being after the jump, also moved down.
		 */

		envPtr->excRangeArrayPtr[range].catchOffset += 3;
	    }
	}
    } /* if expression isn't in {}s */
    
    done:
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    envPtr->exprIsJustVarRef = saveExprIsJustVarRef;
    envPtr->exprIsComparison = saveExprIsComparison;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileCmdWordInline --
 *
 *	Procedure that compiles a Tcl command word inline. If the word is
 *	enclosed in quotes or braces, we call TclCompileString to compile it
 *	after stripping them off. Otherwise, we normally push the word's
 *	value and call eval at runtime, but if the word is just a sequence
 *	of alphanumeric characters, we emit an invoke instruction
 *	directly. This procedure assumes that string points to the start of
 *	the word to compile.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while compiling string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the command.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the command word
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileCmdWordInline(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Interp *iPtr = (Interp *) interp;
    register char *src = string;/* Points to current source char. */
    register int type;          /* Current char's CHAR_TYPE type. */
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute cmd. */
    char *termPtr;		/* Points to char that terminated braced
				 * string. */
    char savedChar;		/* Holds the character termporarily replaced
				 * by a null character during compilation
				 * of the command. */
    int savePushSimpleWords = envPtr->pushSimpleWords;
    int objIndex;
    int result = TCL_OK;
    register char c;

    type = CHAR_TYPE(src, lastChar);
    if (type & (TCL_QUOTE | TCL_OPEN_BRACE)) {
	src++;
	envPtr->pushSimpleWords = 0;
	if (type == TCL_QUOTE) {
	    result = TclCompileQuotes(interp, src, lastChar,
		    '"', flags, envPtr);
	} else {
	    result = CompileBraces(interp, src, lastChar, flags, envPtr);
	}
	if (result != TCL_OK) {
	    goto done;
	}
	
	/*
	 * Make sure the terminating character is the end of word.
	 */
	
	termPtr = (src + envPtr->termOffset);
	c = *termPtr;
	if ((c == '\\') && (*(termPtr+1) == '\n')) {
	    /*
	     * Line is continued on next line; the backslash-newline turns
	     * into space, which terminates the word.
	     */
	} else {
	    type = CHAR_TYPE(termPtr, lastChar);
	    if ((type != TCL_SPACE) && (type != TCL_COMMAND_END)) {
		Tcl_ResetResult(interp);
		if (*(src-1) == '"') {
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
			    "extra characters after close-quote", -1);
		} else {
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "extra characters after close-brace", -1);
		}
		result = TCL_ERROR;
		goto done;
	    }
	}
	
	if (envPtr->wordIsSimple) {
	    /*
	     * A simple word enclosed in "" or {}s. Call TclCompileString to
	     * compile it inline. Add a null character after the end of the
	     * quoted or braced string: i.e., at the " or }. Turn the
	     * flag bit TCL_BRACKET_TERM off since the recursively
	     * compiled subcommand is now terminated by a null character.
	     */
	    char *closeCharPos = (termPtr - 1);
	    
	    savedChar = *closeCharPos;
	    *closeCharPos = '\0';
	    result = TclCompileString(interp, src, closeCharPos,
		    (flags & ~TCL_BRACKET_TERM), envPtr);
	    *closeCharPos = savedChar;
	    if (result != TCL_OK) {
		goto done;
	    }
	} else {
            /*
	     * The braced string contained a backslash-newline. Call eval
	     * at runtime.
	     */
	    TclEmitOpcode(INST_EVAL_STK, envPtr);
	}
	src = termPtr;
	maxDepth = envPtr->maxStackDepth;
    } else {
	/*
	 * Not a braced or quoted string. We normally push the word's
	 * value and call eval at runtime. However, if the word is just
	 * a sequence of alphanumeric characters, we call its compile
	 * procedure, if any, or otherwise just emit an invoke instruction.
	 */

	char *p = src;
	c = *p;
	while (isalnum(UCHAR(c)) || (c == '_')) {
            p++;
            c = *p;
        }
	type = CHAR_TYPE(p, lastChar);
        if ((p > src) && (type == TCL_COMMAND_END)) {
            /*
	     * Look for a compile procedure and call it. Otherwise emit an
	     * invoke instruction to call the command at runtime.
	     */

	    Tcl_Command cmd;
	    Command *cmdPtr = NULL;
	    int wasCompiled = 0;

	    savedChar = *p;
	    *p = '\0';

	    cmd = Tcl_FindCommand(interp, src, (Tcl_Namespace *) NULL,
		    /*flags*/ 0);
	    if (cmd != (Tcl_Command) NULL) {
                cmdPtr = (Command *) cmd;
            }
	    if (cmdPtr != NULL && cmdPtr->compileProc != NULL) {
		*p = savedChar;
		src = p;
		iPtr->flags &= ~(ERR_ALREADY_LOGGED | ERR_IN_PROGRESS
				 | ERROR_CODE_SET);
		result = (*(cmdPtr->compileProc))(interp, src, lastChar, flags, envPtr);
		if (result != TCL_OK) {
		    goto done;
		}
		wasCompiled = 1;
		src += envPtr->termOffset;
		maxDepth = envPtr->maxStackDepth;
	    }
	    if (!wasCompiled) {
		objIndex = TclObjIndexForString(src, p-src,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		*p = savedChar;
		TclEmitPush(objIndex, envPtr);
		TclEmitInstUInt1(INST_INVOKE_STK1, 1, envPtr);
		src = p;
		maxDepth = 1;
	    }
        } else {
	    /*
	     * Push the word and call eval at runtime.
	     */

	    envPtr->pushSimpleWords = 1;
	    result = CompileWord(interp, src, lastChar, flags, envPtr);
	    if (result != TCL_OK) {
		goto done;
	    }
	    TclEmitOpcode(INST_EVAL_STK, envPtr);
	    src += envPtr->termOffset;
	    maxDepth = envPtr->maxStackDepth;
	}
    }

    done:
    envPtr->termOffset = (src - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->pushSimpleWords = savePushSimpleWords;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupCompiledLocal --
 *
 *	This procedure is called at compile time to look up and optionally
 *	allocate an entry ("slot") for a variable in a procedure's array of
 *	local variables. If the variable's name is NULL, a new temporary
 *	variable is always created. (Such temporary variables can only be
 *	referenced using their slot index.)
 *
 * Results:
 *	If createIfNew is 0 (false) and the name is non-NULL, then if the
 *	variable is found, the index of its entry in the procedure's array
 *	of local variables is returned; otherwise -1 is returned.
 *	If name is NULL, the index of a new temporary variable is returned.
 *	Finally, if createIfNew is 1 and name is non-NULL, the index of a
 *	new entry is returned.
 *
 * Side effects:
 *	Creates and registers a new local variable if createIfNew is 1 and
 *	the variable is unknown, or if the name is NULL.
 *
 *----------------------------------------------------------------------
 */

static int
LookupCompiledLocal(name, nameChars, createIfNew, flagsIfCreated, procPtr)
    register char *name;	/* Points to first character of the name of
				 * a scalar or array variable. If NULL, a
				 * temporary var should be created. */
    int nameChars;		/* The length of the name excluding the
				 * terminating null character. */
    int createIfNew;		/* 1 to allocate a local frame entry for the
				 * variable if it is new. */
    int flagsIfCreated;		/* Flag bits for the compiled local if
				 * created. Only VAR_SCALAR, VAR_ARRAY, and
				 * VAR_LINK make sense. */
    register Proc *procPtr;	/* Points to structure describing procedure
				 * containing the variable reference. */
{
    register CompiledLocal *localPtr;
    int localIndex = -1;
    register int i;

    /*
     * If not creating a temporary, does a local variable of the specified
     * name already exist?
     */

    if (name != NULL) {	
	int localCt = procPtr->numCompiledLocals;
	localPtr = procPtr->firstLocalPtr;
	for (i = 0;  i < localCt;  i++) {
	    if (!localPtr->isTemp) {
		char *localName = localPtr->name;
		if ((name[0] == localName[0])
	                && (nameChars == localPtr->nameLength)
	                && (strncmp(name, localName, (unsigned) nameChars) == 0)) {
		    return i;
		}
	    }
	    localPtr = localPtr->nextPtr;
	}
    }

    /*
     * Create a new variable if appropriate.
     */
    
    if (createIfNew || (name == NULL)) {
	localIndex = procPtr->numCompiledLocals;
	localPtr = (CompiledLocal *) ckalloc((unsigned) 
	        (sizeof(CompiledLocal) - sizeof(localPtr->name)
		+ nameChars+1));
	if (procPtr->firstLocalPtr == NULL) {
	    procPtr->firstLocalPtr = procPtr->lastLocalPtr = localPtr;
	} else {
	    procPtr->lastLocalPtr->nextPtr = localPtr;
	    procPtr->lastLocalPtr = localPtr;
	}
	localPtr->nextPtr = NULL;
	localPtr->nameLength = nameChars;
	localPtr->frameIndex = localIndex;
	localPtr->isArg  = 0;
	localPtr->isTemp = (name == NULL);
	localPtr->flags = flagsIfCreated;
	localPtr->defValuePtr = NULL;
	if (name != NULL) {
	    memcpy((VOID *) localPtr->name, (VOID *) name, (size_t) nameChars);
	}
	localPtr->name[nameChars] = '\0';
	procPtr->numCompiledLocals++;
    }
    return localIndex;
}

/*
 *----------------------------------------------------------------------
 *
 * AdvanceToNextWord --
 *
 *	This procedure is called to skip over any leading white space at the
 *	start of a word. Note that a backslash-newline is treated as a
 *	space.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates envPtr->termOffset with the offset of the first
 *	character in "string" that was not white space or a
 *	backslash-newline. This might be the offset of the character that
 *	ends the command: a newline, null, semicolon, or close-bracket.
 *
 *----------------------------------------------------------------------
 */

static void
AdvanceToNextWord(string, envPtr)
    char *string;		/* The source string to compile. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    register char *src;		/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    
    src = string;
    type = CHAR_TYPE(src, src+1);
    while (type & (TCL_SPACE | TCL_BACKSLASH)) {
	if (type == TCL_BACKSLASH) {
	    if (src[1] == '\n') {
		src += 2;
	    } else {
		break;		/* exit loop; no longer white space */
	    }
	} else {
	    src++;
	}
	type = CHAR_TYPE(src, src+1);
    }
    envPtr->termOffset = (src - string);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Backslash --
 *
 *	Figure out how to handle a backslash sequence.
 *
 * Results:
 *	The return value is the character that should be substituted
 *	in place of the backslash sequence that starts at src.  If
 *	readPtr isn't NULL then it is filled in with a count of the
 *	number of characters in the backslash sequence.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char
Tcl_Backslash(src, readPtr)
    CONST char *src;		/* Points to the backslash character of
				 * a backslash sequence. */
    int *readPtr;		/* Fill in with number of characters read
				 * from src, unless NULL. */
{
    CONST char *p = src + 1;
    char result;
    int count;

    count = 2;

    switch (*p) {
	/*
         * Note: in the conversions below, use absolute values (e.g.,
         * 0xa) rather than symbolic values (e.g. \n) that get converted
         * by the compiler.  It's possible that compilers on some
         * platforms will do the symbolic conversions differently, which
         * could result in non-portable Tcl scripts.
         */

        case 'a':
            result = 0x7;
            break;
        case 'b':
            result = 0x8;
            break;
        case 'f':
            result = 0xc;
            break;
        case 'n':
            result = 0xa;
            break;
        case 'r':
            result = 0xd;
            break;
        case 't':
            result = 0x9;
            break;
        case 'v':
            result = 0xb;
            break;
        case 'x':
            if (isxdigit(UCHAR(p[1]))) {
                char *end;

                result = (char) strtoul(p+1, &end, 16);
                count = end - src;
            } else {
                count = 2;
                result = 'x';
            }
            break;
        case '\n':
            do {
                p++;
            } while ((*p == ' ') || (*p == '\t'));
            result = ' ';
            count = p - src;
            break;
        case 0:
            result = '\\';
            count = 1;
            break;
	default:
	    if (isdigit(UCHAR(*p))) {
		result = (char)(*p - '0');
		p++;
		if (!isdigit(UCHAR(*p))) {
		    break;
		}
		count = 3;
		result = (char)((result << 3) + (*p - '0'));
		p++;
		if (!isdigit(UCHAR(*p))) {
		    break;
		}
		count = 4;
		result = (char)((result << 3) + (*p - '0'));
		break;
	    }
	    result = *p;
	    count = 2;
	    break;
    }

    if (readPtr != NULL) {
	*readPtr = count;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjIndexForString --
 *
 *	Procedure to find, or if necessary create, an object in a
 *	CompileEnv's object array that has a string representation
 *	matching the argument string.
 *
 * Results:
 *	The index in the CompileEnv's object array of an object with a
 *	string representation matching the argument "string". The object is
 *	created if necessary. If inHeap is 1, then string is heap allocated
 *	and ownership of the string is passed to TclObjIndexForString;
 *	otherwise, the string is owned by the caller and must not be
 *	modified or freed by TclObjIndexForString. Typically, a caller sets
 *	inHeap 1 if string is an already heap-allocated buffer holding the
 *	result of backslash substitutions.
 *
 * Side effects:
 *	A new Tcl object will be created if no existing object matches the
 *	input string. If allocStrRep is 1 then if a new object is created,
 *	its string representation is allocated in the heap, else it is left
 *	NULL. If inHeap is 1, this procedure is given ownership of the
 * 	string: if an object is created and allocStrRep is 1 then its
 *	string representation is set directly from string, otherwise
 *	the string is freed.
 *
 *----------------------------------------------------------------------
 */

int
TclObjIndexForString(string, length, allocStrRep, inHeap, envPtr)
    register char *string;	/* Points to string for which an object is
				 * found or created in CompileEnv's object
				 * array. */
    int length;			/* Length of string. */
    int allocStrRep;		/* If 1 then the object's string rep should
				 * be allocated in the heap. */
    int inHeap;			/* If 1 then string is heap allocated and
				 * its ownership is passed to
				 * TclObjIndexForString. */
    CompileEnv *envPtr;		/* Points to the CompileEnv in whose object
				 * array an object is found or created. */
{
    register Tcl_Obj *objPtr;	/* Points to the object created for
				 * the string, if one was created. */
    int objIndex;		/* Index of matching object. */
    Tcl_HashEntry *hPtr;
    int strLength, new;
    
    /*
     * Look up the string in the code's object hashtable. If found, just
     * return the associated object array index.  Note that if the string
     * has embedded nulls, we don't create a hash table entry.  This
     * should be fixed, but we need to update hash tables, first.
     */

    strLength = strlen(string);
    if (length == -1) {
	length = strLength;
    }
    if (strLength != length) {
	hPtr = NULL;
    } else {
	hPtr = Tcl_CreateHashEntry(&envPtr->objTable, string, &new);
	if (!new) {		/* already in object table and array */
	    objIndex = (int) Tcl_GetHashValue(hPtr);
	    if (inHeap) {
		ckfree(string);
	    }
	    return objIndex;
	}
    }    

    /*
     * Create a new object holding the string, add it to the object array,
     * and register its index in the object hashtable.
     */

    objPtr = Tcl_NewObj();
    if (allocStrRep) {
	if (inHeap) {		/* use input string for obj's string rep */
	    objPtr->bytes = string;
	} else {
	    if (length > 0) {
		objPtr->bytes = ckalloc((unsigned) length + 1);
		memcpy((VOID *) objPtr->bytes, (VOID *) string,
			(size_t) length);
		objPtr->bytes[length] = '\0';
	    }
	}
	objPtr->length = length;
    } else {			/* leave the string rep NULL */
	if (inHeap) {
	    ckfree(string);
	}
    }

    if (envPtr->objArrayNext >= envPtr->objArrayEnd) {
        ExpandObjectArray(envPtr);
    }
    objIndex = envPtr->objArrayNext;
    envPtr->objArrayPtr[objIndex] = objPtr;
    Tcl_IncrRefCount(objPtr);
    envPtr->objArrayNext++;

    if (hPtr) {
	Tcl_SetHashValue(hPtr, objIndex);
    }
    return objIndex;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExpandCodeArray --
 *
 *	Procedure that uses malloc to allocate more storage for a
 *	CompileEnv's code array.
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	The byte code array in *envPtr is reallocated to a new array of
 *	double the size, and if envPtr->mallocedCodeArray is non-zero the
 *	old array is freed. Byte codes are copied from the old array to the
 *	new one.
 *
 *----------------------------------------------------------------------
 */

void
TclExpandCodeArray(envPtr)
    CompileEnv *envPtr;		/* Points to the CompileEnv whose code array
				 * must be enlarged. */
{
    /*
     * envPtr->codeNext is equal to envPtr->codeEnd. The currently defined
     * code bytes are stored between envPtr->codeStart and
     * (envPtr->codeNext - 1) [inclusive].
     */
    
    size_t currBytes = TclCurrCodeOffset();
    size_t newBytes  = 2*(envPtr->codeEnd  - envPtr->codeStart);
    unsigned char *newPtr = (unsigned char *) ckalloc((unsigned) newBytes);

    /*
     * Copy from old code array to new, free old code array if needed, and
     * mark new code array as malloced.
     */
 
    memcpy((VOID *) newPtr, (VOID *) envPtr->codeStart, currBytes);
    if (envPtr->mallocedCodeArray) {
        ckfree((char *) envPtr->codeStart);
    }
    envPtr->codeStart = newPtr;
    envPtr->codeNext = (newPtr + currBytes);
    envPtr->codeEnd  = (newPtr + newBytes);
    envPtr->mallocedCodeArray = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpandObjectArray --
 *
 *	Procedure that uses malloc to allocate more storage for a
 *	CompileEnv's object array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object array in *envPtr is reallocated to a new array of
 *	double the size, and if envPtr->mallocedObjArray is non-zero the
 *	old array is freed. Tcl_Obj pointers are copied from the old array
 *	to the new one.
 *
 *----------------------------------------------------------------------
 */

static void
ExpandObjectArray(envPtr)
    CompileEnv *envPtr;		/* Points to the CompileEnv whose object
				 * array must be enlarged. */
{
    /*
     * envPtr->objArrayNext is equal to envPtr->objArrayEnd. The currently
     * allocated Tcl_Obj pointers are stored between elements
     * 0 and (envPtr->objArrayNext - 1) [inclusive] in the object array
     * pointed to by objArrayPtr.
     */

    size_t currBytes = envPtr->objArrayNext * sizeof(Tcl_Obj *);
    int newElems = 2*envPtr->objArrayEnd;
    size_t newBytes = newElems * sizeof(Tcl_Obj *);
    Tcl_Obj **newPtr = (Tcl_Obj **) ckalloc((unsigned) newBytes);

    /*
     * Copy from old object array to new, free old object array if needed,
     * and mark new object array as malloced.
     */
 
    memcpy((VOID *) newPtr, (VOID *) envPtr->objArrayPtr, currBytes);
    if (envPtr->mallocedObjArray) {
	ckfree((char *) envPtr->objArrayPtr);
    }
    envPtr->objArrayPtr = (Tcl_Obj **) newPtr;
    envPtr->objArrayEnd = newElems;
    envPtr->mallocedObjArray = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * EnterCmdStartData --
 *
 *	Registers the starting source and bytecode location of a
 *	command. This information is used at runtime to map between
 *	instruction pc and source locations.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Inserts source and code location information into the compilation
 *	environment envPtr for the command at index cmdIndex. The
 *	compilation environment's CmdLocation array is grown if necessary.
 *
 *----------------------------------------------------------------------
 */

static void
EnterCmdStartData(envPtr, cmdIndex, srcOffset, codeOffset)
    CompileEnv *envPtr;		/* Points to the compilation environment
				 * structure in which to enter command
				 * location information. */
    int cmdIndex;		/* Index of the command whose start data
				 * is being set. */
    int srcOffset;		/* Offset of first char of the command. */
    int codeOffset;		/* Offset of first byte of command code. */
{
    CmdLocation *cmdLocPtr;
    
    if ((cmdIndex < 0) || (cmdIndex >= envPtr->numCommands)) {
	panic("EnterCmdStartData: bad command index %d\n", cmdIndex);
    }
    
    if (cmdIndex >= envPtr->cmdMapEnd) {
	/*
	 * Expand the command location array by allocating more storage from
	 * the heap. The currently allocated CmdLocation entries are stored
	 * from cmdMapPtr[0] up to cmdMapPtr[envPtr->cmdMapEnd] (inclusive).
	 */

	size_t currElems = envPtr->cmdMapEnd;
	size_t newElems  = 2*currElems;
	size_t currBytes = currElems * sizeof(CmdLocation);
	size_t newBytes  = newElems  * sizeof(CmdLocation);
	CmdLocation *newPtr = (CmdLocation *) ckalloc((unsigned) newBytes);
	
	/*
	 * Copy from old command location array to new, free old command
	 * location array if needed, and mark new array as malloced.
	 */
	
	memcpy((VOID *) newPtr, (VOID *) envPtr->cmdMapPtr, currBytes);
	if (envPtr->mallocedCmdMap) {
	    ckfree((char *) envPtr->cmdMapPtr);
	}
	envPtr->cmdMapPtr = (CmdLocation *) newPtr;
	envPtr->cmdMapEnd = newElems;
	envPtr->mallocedCmdMap = 1;
    }

    if (cmdIndex > 0) {
	if (codeOffset < envPtr->cmdMapPtr[cmdIndex-1].codeOffset) {
	    panic("EnterCmdStartData: cmd map table not sorted by code offset");
	}
    }

    cmdLocPtr = &(envPtr->cmdMapPtr[cmdIndex]);
    cmdLocPtr->codeOffset = codeOffset;
    cmdLocPtr->srcOffset = srcOffset;
    cmdLocPtr->numSrcChars = -1;
    cmdLocPtr->numCodeBytes = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * EnterCmdExtentData --
 *
 *	Registers the source and bytecode length for a command. This
 *	information is used at runtime to map between instruction pc and
 *	source locations.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Inserts source and code length information into the compilation
 *	environment envPtr for the command at index cmdIndex. Starting
 *	source and bytecode information for the command must already
 *	have been registered.
 *
 *----------------------------------------------------------------------
 */

static void
EnterCmdExtentData(envPtr, cmdIndex, numSrcChars, numCodeBytes)
    CompileEnv *envPtr;		/* Points to the compilation environment
				 * structure in which to enter command
				 * location information. */
    int cmdIndex;		/* Index of the command whose source and
				 * code length data is being set. */
    int numSrcChars;		/* Number of command source chars. */
    int numCodeBytes;		/* Offset of last byte of command code. */
{
    CmdLocation *cmdLocPtr;

    if ((cmdIndex < 0) || (cmdIndex >= envPtr->numCommands)) {
	panic("EnterCmdStartData: bad command index %d\n", cmdIndex);
    }
    
    if (cmdIndex > envPtr->cmdMapEnd) {
	panic("EnterCmdStartData: no start data registered for command with index %d\n", cmdIndex);
    }

    cmdLocPtr = &(envPtr->cmdMapPtr[cmdIndex]);
    cmdLocPtr->numSrcChars = numSrcChars;
    cmdLocPtr->numCodeBytes = numCodeBytes;
}

/*
 *----------------------------------------------------------------------
 *
 * InitArgInfo --
 *
 *	Initializes a ArgInfo structure to hold information about
 *	some number of argument words in a command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The ArgInfo structure is initialized.
 *
 *----------------------------------------------------------------------
 */

static void
InitArgInfo(argInfoPtr)
    register ArgInfo *argInfoPtr; /* Points to the ArgInfo structure
				   * to initialize. */
{
    argInfoPtr->numArgs = 0;
    argInfoPtr->startArray = argInfoPtr->staticStartSpace;
    argInfoPtr->endArray   = argInfoPtr->staticEndSpace;
    argInfoPtr->allocArgs = ARGINFO_INIT_ENTRIES;
    argInfoPtr->mallocedArrays = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CollectArgInfo --
 *
 *	Procedure to scan the argument words of a command and record the
 *	start and finish of each argument word in a ArgInfo structure.
 *
 * Results:
 *	The return value is a standard Tcl result, which is TCL_OK unless
 *	there was an error while scanning string. If an error occurs then
 *	the interpreter's result contains a standard error message.
 *
 * Side effects:
 *	If necessary, the argument start and end arrays in *argInfoPtr
 *	are grown and reallocated to a new arrays of double the size, and
 *	if argInfoPtr->mallocedArray is non-zero the old arrays are freed.
 *
 *----------------------------------------------------------------------
 */

static int
CollectArgInfo(interp, string, lastChar, flags, argInfoPtr)
    Tcl_Interp *interp;         /* Used for error reporting. */
    char *string;               /* The source command string to scan. */
    char *lastChar;		 /* Pointer to terminating character of
				  * string. */
    int flags;                  /* Flags to control compilation (same as
                                 * passed to Tcl_Eval). */
    register ArgInfo *argInfoPtr;
				/* Points to the ArgInfo structure in which
				 * to record the arg word information. */
{
    register char *src = string;/* Points to current source char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    int nestedCmd = (flags & TCL_BRACKET_TERM);
                                /* 1 if string being scanned is a nested
				 * command and is terminated by a ']';
				 * otherwise 0. */
    int scanningArgs;           /* 1 if still scanning argument words to
				 * determine their start and end. */
    char *wordStart, *wordEnd;  /* Points to the first and last significant
				 * characters of each word. */
    CompileEnv tempCompEnv;	/* Only used to hold the termOffset field
				 * updated by AdvanceToNextWord. */
    char *prev;

    argInfoPtr->numArgs = 0;
    scanningArgs = 1;
    while (scanningArgs) {
	AdvanceToNextWord(src, &tempCompEnv);
	src += tempCompEnv.termOffset;
	type = CHAR_TYPE(src, lastChar);

	if ((type == TCL_COMMAND_END) && ((*src != ']') || nestedCmd)) {
	    break;		    /* done collecting argument words */
	} else if (*src == '"') {
	    wordStart = src;
	    src = TclWordEnd(src, lastChar, nestedCmd, NULL);
	    if (src == lastChar) {
	        badStringTermination:
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
	                "quoted string doesn't terminate properly", -1);
		return TCL_ERROR;
	    }
	    prev = (src-1);
	    if (*src == '"') {
		wordEnd = src;
		src++;
	    } else if ((*src == ';') && (*prev == '"')) {
		scanningArgs = 0;
		wordEnd = prev;
	    } else {
		goto badStringTermination;
	    }
	} else if (*src == '{') {
	    wordStart = src;
	    src = TclWordEnd(src, lastChar, nestedCmd, NULL);
	    if (src == lastChar) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "missing close-brace", -1);
		return TCL_ERROR;
	    }
	    prev = (src-1);
	    if (*src == '}') {
		wordEnd = src;
		src++;
	    } else if ((*src == ';') && (*prev == '}')) {
		scanningArgs = 0;
		wordEnd = prev;
	    } else {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
	                "argument word in braces doesn't terminate properly", -1);
		return TCL_ERROR;
	    }
	} else {
	    wordStart = src;
	    src = TclWordEnd(src, lastChar, nestedCmd, NULL);
	    prev = (src-1);
	    if (src == lastChar) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "missing close-bracket or close-brace", -1);
		return TCL_ERROR;
	    } else if (*src == ';') {
		scanningArgs = 0;
		wordEnd = prev;
	    } else {
		wordEnd = src;
		src++;
		if ((src == lastChar) || (*src == '\n')
	                || ((*src == ']') && nestedCmd)) {
		    scanningArgs = 0;
		}
	    }
	} /* end of test on each kind of word */

	if (argInfoPtr->numArgs == argInfoPtr->allocArgs) {
	    int newArgs = 2*argInfoPtr->numArgs;
	    size_t currBytes = argInfoPtr->numArgs * sizeof(char *);
	    size_t newBytes  = newArgs * sizeof(char *);
	    char **newStartArrayPtr =
		    (char **) ckalloc((unsigned) newBytes);
	    char **newEndArrayPtr =
		    (char **) ckalloc((unsigned) newBytes);
	    
	    /*
	     * Copy from the old arrays to the new, free the old arrays if
	     * needed, and mark the new arrays as malloc'ed.
	     */
	    
	    memcpy((VOID *) newStartArrayPtr,
	            (VOID *) argInfoPtr->startArray, currBytes);
	    memcpy((VOID *) newEndArrayPtr,
		    (VOID *) argInfoPtr->endArray, currBytes);
	    if (argInfoPtr->mallocedArrays) {
		ckfree((char *) argInfoPtr->startArray);
		ckfree((char *) argInfoPtr->endArray);
	    }
	    argInfoPtr->startArray = newStartArrayPtr;
	    argInfoPtr->endArray   = newEndArrayPtr;
	    argInfoPtr->allocArgs = newArgs;
	    argInfoPtr->mallocedArrays = 1;
	}
	argInfoPtr->startArray[argInfoPtr->numArgs] = wordStart;
	argInfoPtr->endArray[argInfoPtr->numArgs]   = wordEnd;
	argInfoPtr->numArgs++;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeArgInfo --
 *
 *	Free any storage allocated in a ArgInfo structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocated storage in the ArgInfo structure is freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeArgInfo(argInfoPtr)
    register ArgInfo *argInfoPtr; /* Points to the ArgInfo structure
				   * to free. */
{
    if (argInfoPtr->mallocedArrays) {
	ckfree((char *) argInfoPtr->startArray);
	ckfree((char *) argInfoPtr->endArray);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CreateLoopExceptionRange --
 *
 *	Procedure that allocates and initializes a new ExceptionRange
 *	structure of the specified kind in a CompileEnv's ExceptionRange
 *	array.
 *
 * Results:
 *	Returns the index for the newly created ExceptionRange.
 *
 * Side effects:
 *	If there is not enough room in the CompileEnv's ExceptionRange
 *	array, the array in expanded: a new array of double the size is
 *	allocated, if envPtr->mallocedExcRangeArray is non-zero the old
 *	array is freed, and ExceptionRange entries are copied from the old
 *	array to the new one.
 *
 *----------------------------------------------------------------------
 */

static int
CreateExceptionRange(type, envPtr)
    ExceptionRangeType type;	/* The kind of ExceptionRange desired. */
    register CompileEnv *envPtr;/* Points to the CompileEnv for which a new
				 * loop ExceptionRange structure is to be
				 * allocated. */
{
    int index;			/* Index for the newly-allocated
				 * ExceptionRange structure. */
    register ExceptionRange *rangePtr;
    				/* Points to the new ExceptionRange
				 * structure */
    
    index = envPtr->excRangeArrayNext;
    if (index >= envPtr->excRangeArrayEnd) {
        /*
	 * Expand the ExceptionRange array. The currently allocated entries
	 * are stored between elements 0 and (envPtr->excRangeArrayNext - 1)
	 * [inclusive].
	 */
	
	size_t currBytes =
	        envPtr->excRangeArrayNext * sizeof(ExceptionRange);
	int newElems = 2*envPtr->excRangeArrayEnd;
	size_t newBytes = newElems * sizeof(ExceptionRange);
	ExceptionRange *newPtr = (ExceptionRange *)
	        ckalloc((unsigned) newBytes);
	
	/*
	 * Copy from old ExceptionRange array to new, free old
	 * ExceptionRange array if needed, and mark the new ExceptionRange
	 * array as malloced.
	 */
	
	memcpy((VOID *) newPtr, (VOID *) envPtr->excRangeArrayPtr,
	        currBytes);
	if (envPtr->mallocedExcRangeArray) {
	    ckfree((char *) envPtr->excRangeArrayPtr);
	}
	envPtr->excRangeArrayPtr = (ExceptionRange *) newPtr;
	envPtr->excRangeArrayEnd = newElems;
	envPtr->mallocedExcRangeArray = 1;
    }
    envPtr->excRangeArrayNext++;
    
    rangePtr = &(envPtr->excRangeArrayPtr[index]);
    rangePtr->type = type;
    rangePtr->nestingLevel = envPtr->excRangeDepth;
    rangePtr->codeOffset = -1;
    rangePtr->numCodeBytes = -1;
    rangePtr->breakOffset = -1;
    rangePtr->continueOffset = -1;
    rangePtr->catchOffset = -1;
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateAuxData --
 *
 *	Procedure that allocates and initializes a new AuxData structure in
 *	a CompileEnv's array of compilation auxiliary data records. These
 *	AuxData records hold information created during compilation by
 *	CompileProcs and used by instructions during execution.
 *
 * Results:
 *	Returns the index for the newly created AuxData structure.
 *
 * Side effects:
 *	If there is not enough room in the CompileEnv's AuxData array,
 *	the AuxData array in expanded: a new array of double the size
 *	is allocated, if envPtr->mallocedAuxDataArray is non-zero
 *	the old array is freed, and AuxData entries are copied from
 *	the old array to the new one.
 *
 *----------------------------------------------------------------------
 */

int
TclCreateAuxData(clientData, dupProc, freeProc, envPtr)
    ClientData clientData;	/* The compilation auxiliary data to store
				 * in the new aux data record. */
    AuxDataDupProc *dupProc;	/* Procedure to call to duplicate the
				 * compilation aux data when the containing
				 * ByteCode structure is duplicated. */
    AuxDataFreeProc *freeProc;	/* Procedure to call to free the
				 * compilation aux data when the containing
				 * ByteCode structure is freed.  */
    register CompileEnv *envPtr;/* Points to the CompileEnv for which a new
				 * aux data structure is to be allocated. */
{
    int index;			/* Index for the new AuxData structure. */
    register AuxData *auxDataPtr;
    				/* Points to the new AuxData structure */
    
    index = envPtr->auxDataArrayNext;
    if (index >= envPtr->auxDataArrayEnd) {
        /*
	 * Expand the AuxData array. The currently allocated entries are
	 * stored between elements 0 and (envPtr->auxDataArrayNext - 1)
	 * [inclusive].
	 */
	
	size_t currBytes = envPtr->auxDataArrayNext * sizeof(AuxData);
	int newElems = 2*envPtr->auxDataArrayEnd;
	size_t newBytes = newElems * sizeof(AuxData);
	AuxData *newPtr = (AuxData *) ckalloc((unsigned) newBytes);
	
	/*
	 * Copy from old AuxData array to new, free old AuxData array if
	 * needed, and mark the new AuxData array as malloced.
	 */
	
	memcpy((VOID *) newPtr, (VOID *) envPtr->auxDataArrayPtr,
	        currBytes);
	if (envPtr->mallocedAuxDataArray) {
	    ckfree((char *) envPtr->auxDataArrayPtr);
	}
	envPtr->auxDataArrayPtr = newPtr;
	envPtr->auxDataArrayEnd = newElems;
	envPtr->mallocedAuxDataArray = 1;
    }
    envPtr->auxDataArrayNext++;
    
    auxDataPtr = &(envPtr->auxDataArrayPtr[index]);
    auxDataPtr->clientData = clientData;
    auxDataPtr->dupProc  = dupProc;
    auxDataPtr->freeProc = freeProc;
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitJumpFixupArray --
 *
 *	Initializes a JumpFixupArray structure to hold some number of
 *	jump fixup entries.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The JumpFixupArray structure is initialized.
 *
 *----------------------------------------------------------------------
 */

void
TclInitJumpFixupArray(fixupArrayPtr)
    register JumpFixupArray *fixupArrayPtr;
				 /* Points to the JumpFixupArray structure
				  * to initialize. */
{
    fixupArrayPtr->fixup = fixupArrayPtr->staticFixupSpace;
    fixupArrayPtr->next = 0;
    fixupArrayPtr->end = (JUMPFIXUP_INIT_ENTRIES - 1);
    fixupArrayPtr->mallocedArray = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExpandJumpFixupArray --
 *
 *	Procedure that uses malloc to allocate more storage for a
 *      jump fixup array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The jump fixup array in *fixupArrayPtr is reallocated to a new array
 *	of double the size, and if fixupArrayPtr->mallocedArray is non-zero
 *	the old array is freed. Jump fixup structures are copied from the
 *	old array to the new one.
 *
 *----------------------------------------------------------------------
 */

void
TclExpandJumpFixupArray(fixupArrayPtr)
    register JumpFixupArray *fixupArrayPtr;
				 /* Points to the JumpFixupArray structure
				  * to enlarge. */
{
    /*
     * The currently allocated jump fixup entries are stored from fixup[0]
     * up to fixup[fixupArrayPtr->fixupNext] (*not* inclusive). We assume
     * fixupArrayPtr->fixupNext is equal to fixupArrayPtr->fixupEnd.
     */

    size_t currBytes = fixupArrayPtr->next * sizeof(JumpFixup);
    int newElems = 2*(fixupArrayPtr->end + 1);
    size_t newBytes = newElems * sizeof(JumpFixup);
    JumpFixup *newPtr = (JumpFixup *) ckalloc((unsigned) newBytes);

    /*
     * Copy from the old array to new, free the old array if needed,
     * and mark the new array as malloced.
     */
 
    memcpy((VOID *) newPtr, (VOID *) fixupArrayPtr->fixup, currBytes);
    if (fixupArrayPtr->mallocedArray) {
	ckfree((char *) fixupArrayPtr->fixup);
    }
    fixupArrayPtr->fixup = (JumpFixup *) newPtr;
    fixupArrayPtr->end = newElems;
    fixupArrayPtr->mallocedArray = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreeJumpFixupArray --
 *
 *	Free any storage allocated in a jump fixup array structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocated storage in the JumpFixupArray structure is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFreeJumpFixupArray(fixupArrayPtr)
    register JumpFixupArray *fixupArrayPtr;
				 /* Points to the JumpFixupArray structure
				  * to free. */
{
    if (fixupArrayPtr->mallocedArray) {
	ckfree((char *) fixupArrayPtr->fixup);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclEmitForwardJump --
 *
 *	Procedure to emit a two-byte forward jump of kind "jumpType". Since
 *	the jump may later have to be grown to five bytes if the jump target
 *	is more than, say, 127 bytes away, this procedure also initializes a
 *	JumpFixup record with information about the jump. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The JumpFixup record pointed to by "jumpFixupPtr" is initialized
 *	with information needed later if the jump is to be grown. Also,
 *	a two byte jump of the designated type is emitted at the current
 *	point in the bytecode stream.
 *
 *----------------------------------------------------------------------
 */

void
TclEmitForwardJump(envPtr, jumpType, jumpFixupPtr)
    CompileEnv *envPtr;		/* Points to the CompileEnv structure that
				 * holds the resulting instruction. */
    TclJumpType jumpType;	/* Indicates the kind of jump: if true or
				 * false or unconditional. */
    JumpFixup *jumpFixupPtr;	/* Points to the JumpFixup structure to
				 * initialize with information about this
				 * forward jump. */
{
    /*
     * Initialize the JumpFixup structure:
     *    - codeOffset is offset of first byte of jump below
     *    - cmdIndex is index of the command after the current one
     *    - excRangeIndex is the index of the first ExceptionRange after
     *      the current one.
     */
    
    jumpFixupPtr->jumpType = jumpType;
    jumpFixupPtr->codeOffset = TclCurrCodeOffset();
    jumpFixupPtr->cmdIndex = envPtr->numCommands;
    jumpFixupPtr->excRangeIndex = envPtr->excRangeArrayNext;
    
    switch (jumpType) {
    case TCL_UNCONDITIONAL_JUMP:
	TclEmitInstInt1(INST_JUMP1, /*offset*/ 0, envPtr);
	break;
    case TCL_TRUE_JUMP:
	TclEmitInstInt1(INST_JUMP_TRUE1, /*offset*/ 0, envPtr);
	break;
    default:
	TclEmitInstInt1(INST_JUMP_FALSE1, /*offset*/ 0, envPtr);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFixupForwardJump --
 *
 *	Procedure that updates a previously-emitted forward jump to jump
 *	a specified number of bytes, "jumpDist". If necessary, the jump is
 *      grown from two to five bytes; this is done if the jump distance is
 *	greater than "distThreshold" (normally 127 bytes). The jump is
 *	described by a JumpFixup record previously initialized by
 *	TclEmitForwardJump.
 *
 * Results:
 *	1 if the jump was grown and subsequent instructions had to be moved;
 *	otherwise 0. This result is returned to allow callers to update
 *	any additional code offsets they may hold.
 *
 * Side effects:
 *	The jump may be grown and subsequent instructions moved. If this
 *	happens, the code offsets for any commands and any ExceptionRange
 *	records	between the jump and the current code address will be
 *	updated to reflect the moved code. Also, the bytecode instruction
 *	array in the CompileEnv structure may be grown and reallocated.
 *
 *----------------------------------------------------------------------
 */

int
TclFixupForwardJump(envPtr, jumpFixupPtr, jumpDist, distThreshold)
    CompileEnv *envPtr;		/* Points to the CompileEnv structure that
				 * holds the resulting instruction. */
    JumpFixup *jumpFixupPtr;    /* Points to the JumpFixup structure that
				 * describes the forward jump. */
    int jumpDist;		/* Jump distance to set in jump
				 * instruction. */
    int distThreshold;		/* Maximum distance before the two byte
				 * jump is grown to five bytes. */
{
    unsigned char *jumpPc, *p;
    int firstCmd, lastCmd, firstRange, lastRange, k;
    unsigned int numBytes;
    
    if (jumpDist <= distThreshold) {
	jumpPc = (envPtr->codeStart + jumpFixupPtr->codeOffset);
	switch (jumpFixupPtr->jumpType) {
	case TCL_UNCONDITIONAL_JUMP:
	    TclUpdateInstInt1AtPc(INST_JUMP1, jumpDist, jumpPc);
	    break;
	case TCL_TRUE_JUMP:
	    TclUpdateInstInt1AtPc(INST_JUMP_TRUE1, jumpDist, jumpPc);
	    break;
	default:
	    TclUpdateInstInt1AtPc(INST_JUMP_FALSE1, jumpDist, jumpPc);
	    break;
	}
	return 0;
    }

    /*
     * We must grow the jump then move subsequent instructions down.
     */
    
    TclEnsureCodeSpace(3, envPtr);  /* NB: might change code addresses! */
    jumpPc = (envPtr->codeStart + jumpFixupPtr->codeOffset);
    for (numBytes = envPtr->codeNext-jumpPc-2, p = jumpPc+2+numBytes-1;
	    numBytes > 0;  numBytes--, p--) {
	p[3] = p[0];
    }
    envPtr->codeNext += 3;
    jumpDist += 3;
    switch (jumpFixupPtr->jumpType) {
    case TCL_UNCONDITIONAL_JUMP:
	TclUpdateInstInt4AtPc(INST_JUMP4, jumpDist, jumpPc);
	break;
    case TCL_TRUE_JUMP:
	TclUpdateInstInt4AtPc(INST_JUMP_TRUE4, jumpDist, jumpPc);
	break;
    default:
	TclUpdateInstInt4AtPc(INST_JUMP_FALSE4, jumpDist, jumpPc);
	break;
    }
    
    /*
     * Adjust the code offsets for any commands and any ExceptionRange
     * records between the jump and the current code address.
     */
    
    firstCmd = jumpFixupPtr->cmdIndex;
    lastCmd  = (envPtr->numCommands - 1);
    if (firstCmd < lastCmd) {
	for (k = firstCmd;  k <= lastCmd;  k++) {
	    (envPtr->cmdMapPtr[k]).codeOffset += 3;
	}
    }
    
    firstRange = jumpFixupPtr->excRangeIndex;
    lastRange  = (envPtr->excRangeArrayNext - 1);
    for (k = firstRange;  k <= lastRange;  k++) {
	ExceptionRange *rangePtr = &(envPtr->excRangeArrayPtr[k]);
	rangePtr->codeOffset += 3;
	
	switch (rangePtr->type) {
	case LOOP_EXCEPTION_RANGE:
	    rangePtr->breakOffset += 3;
	    if (rangePtr->continueOffset != -1) {
		rangePtr->continueOffset += 3;
	    }
	    break;
	case CATCH_EXCEPTION_RANGE:
	    rangePtr->catchOffset += 3;
	    break;
	default:
	    panic("TclFixupForwardJump: unrecognized ExceptionRange type %d\n", rangePtr->type);
	}
    }
    return 1;			/* the jump was grown */
}


