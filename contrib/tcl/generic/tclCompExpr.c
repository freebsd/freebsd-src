/* 
 * tclCompExpr.c --
 *
 *	This file contains the code to compile Tcl expressions.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclCompExpr.c 1.30 97/06/13 18:17:20
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * The stuff below is a bit of a hack so that this file can be used in
 * environments that include no UNIX, i.e. no errno: just arrange to use
 * the errno from tclExecute.c here.
 */

#ifndef TCL_GENERIC_ONLY
#include "tclPort.h"
#else
#define NO_ERRNO_H
#endif

#ifdef NO_ERRNO_H
extern int errno;			/* Use errno from tclExecute.c. */
#define ERANGE 34
#endif

/*
 * Boolean variable that controls whether expression compilation tracing
 * is enabled.
 */

#ifdef TCL_COMPILE_DEBUG
static int traceCompileExpr = 0;
#endif /* TCL_COMPILE_DEBUG */

/*
 * The ExprInfo structure describes the state of compiling an expression.
 * A pointer to an ExprInfo record is passed among the routines in
 * this module.
 */

typedef struct ExprInfo {
    int token;			/* Type of the last token parsed in expr.
				 * See below for definitions. Corresponds
				 * to the characters just before next. */
    int objIndex;		/* If token is a literal value, the index of
				 * an object holding the value in the code's
				 * object table; otherwise is NULL. */
    char *funcName;		/* If the token is FUNC_NAME, points to the
				 * first character of the math function's
				 * name; otherwise is NULL. */
    char *next;			/* Position of the next character to be
				 * scanned in the expression string. */
    char *originalExpr;		/* The entire expression that was originally
				 * passed to Tcl_ExprString et al. */
    char *lastChar;		/* Pointer to terminating null in
				 * originalExpr. */
    int hasOperators;		/* Set 1 if the expr has operators; 0 if
				 * expr is only a primary. If 1 after
				 * compiling an expr, a tryCvtToNumeric
				 * instruction is emitted to convert the
				 * primary to a number if possible. */
    int exprIsJustVarRef;	/* Set 1 if the expr consists of just a
				 * variable reference as in the expression
				 * of "if $b then...". Otherwise 0. Used
				 * to implement expr's 2 level substitution
				 * semantics properly. */
} ExprInfo;

/*
 * Definitions of the different tokens that appear in expressions. The order
 * of these must match the corresponding entries in the operatorStrings
 * array below.
 */

#define LITERAL		0
#define FUNC_NAME	(LITERAL + 1)
#define OPEN_BRACKET	(LITERAL + 2)
#define CLOSE_BRACKET	(LITERAL + 3)
#define OPEN_PAREN	(LITERAL + 4)
#define CLOSE_PAREN	(LITERAL + 5)
#define DOLLAR		(LITERAL + 6)
#define QUOTE		(LITERAL + 7)
#define COMMA		(LITERAL + 8)
#define END		(LITERAL + 9)
#define UNKNOWN		(LITERAL + 10)

/*
 * Binary operators:
 */

#define MULT		(UNKNOWN + 1)
#define DIVIDE		(MULT + 1)
#define MOD		(MULT + 2)
#define PLUS		(MULT + 3)
#define MINUS		(MULT + 4)
#define LEFT_SHIFT	(MULT + 5)
#define RIGHT_SHIFT	(MULT + 6)
#define LESS		(MULT + 7)
#define GREATER		(MULT + 8)
#define LEQ		(MULT + 9)
#define GEQ		(MULT + 10)
#define EQUAL		(MULT + 11)
#define NEQ		(MULT + 12)
#define BIT_AND		(MULT + 13)
#define BIT_XOR		(MULT + 14)
#define BIT_OR		(MULT + 15)
#define AND		(MULT + 16)
#define OR		(MULT + 17)
#define QUESTY		(MULT + 18)
#define COLON		(MULT + 19)

/*
 * Unary operators. Unary minus and plus are represented by the (binary)
 * tokens MINUS and PLUS.
 */

#define NOT		(COLON + 1)
#define BIT_NOT		(NOT + 1)

/*
 * Mapping from tokens to strings; used for debugging messages. These
 * entries must match the order and number of the token definitions above.
 */

#ifdef TCL_COMPILE_DEBUG
static char *tokenStrings[] = {
    "LITERAL", "FUNCNAME",
    "[", "]", "(", ")", "$", "\"", ",", "END", "UNKNOWN",
    "*", "/", "%", "+", "-",
    "<<", ">>", "<", ">", "<=", ">=", "==", "!=",
    "&", "^", "|", "&&", "||", "?", ":",
    "!", "~"
};
#endif /* TCL_COMPILE_DEBUG */

/*
 * Declarations for local procedures to this file:
 */

static int		CompileAddExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileBitAndExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileBitOrExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileBitXorExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileCondExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileEqualityExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileLandExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileLorExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileMathFuncCall _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileMultiplyExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompilePrimaryExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileRelationalExpr _ANSI_ARGS_((
    			    Tcl_Interp *interp, ExprInfo *infoPtr,
			    int flags, CompileEnv *envPtr));
static int		CompileShiftExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		CompileUnaryExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int flags,
			    CompileEnv *envPtr));
static int		GetToken _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, CompileEnv *envPtr));

/*
 * Macro used to debug the execution of the recursive descent parser used
 * to compile expressions.
 */

#ifdef TCL_COMPILE_DEBUG
#define HERE(production, level) \
    if (traceCompileExpr) { \
	fprintf(stderr, "%*s%s: token=%s, next=\"%.20s\"\n", \
		(level), " ", (production), tokenStrings[infoPtr->token], \
		infoPtr->next); \
    }
#else
#define HERE(production, level)
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * TclCompileExpr --
 *
 *	This procedure compiles a string containing a Tcl expression into
 *	Tcl bytecodes. This procedure is the top-level interface to the
 *	the expression compilation module, and is used by such public
 *	procedures as Tcl_ExprString, Tcl_ExprStringObj, Tcl_ExprLong,
 *	Tcl_ExprDouble, Tcl_ExprBoolean, and Tcl_ExprBooleanObj.
 *
 *	Note that the topmost recursive-descent parsing routine used by
 *	TclCompileExpr to compile expressions is called "CompileCondExpr"
 *	and not, e.g., "CompileExpr". This is done to avoid an extra
 *	procedure call since such a procedure would only return the result
 *	of calling CompileCondExpr. Other recursive-descent procedures
 *	that need to parse expressions also call CompileCondExpr.
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->termOffset is filled in with the offset of the character in
 *	"string" just after the last one successfully processed; this might
 *	be the offset of the ']' (if flags & TCL_BRACKET_TERM), or the
 *	offset of the '\0' at the end of the string.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 *	envPtr->exprIsJustVarRef is set 1 if the expression consisted of
 *	a single variable reference as in the expression of "if $b then...".
 *	Otherwise it is set 0. This is used to implement Tcl's two level
 *	expression substitution semantics properly.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileExpr(interp, string, lastChar, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* The source string to compile. */
    char *lastChar;		/* Pointer to terminating character of
				 * string. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Interp *iPtr = (Interp *) interp;
    ExprInfo info;
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int result;

#ifdef TCL_COMPILE_DEBUG
    if (traceCompileExpr) {
	fprintf(stderr, "expr: string=\"%.30s\"\n", string);
    }
#endif /* TCL_COMPILE_DEBUG */

    /*
     * Register the builtin math functions the first time an expression is
     * compiled.
     */

    if (!(iPtr->flags & EXPR_INITIALIZED)) {
	BuiltinFunc *funcPtr;
	Tcl_HashEntry *hPtr;
	MathFunc *mathFuncPtr;
	int i;

	iPtr->flags |= EXPR_INITIALIZED;
	i = 0;
	for (funcPtr = builtinFuncTable; funcPtr->name != NULL; funcPtr++) {
	    Tcl_CreateMathFunc(interp, funcPtr->name,
		    funcPtr->numArgs, funcPtr->argTypes,
		    (Tcl_MathProc *) NULL, (ClientData) 0);
	    
	    hPtr = Tcl_FindHashEntry(&iPtr->mathFuncTable, funcPtr->name);
	    if (hPtr == NULL) {
		panic("TclCompileExpr: Tcl_CreateMathFunc incorrectly registered '%s'", funcPtr->name);
		return TCL_ERROR;
	    }
	    mathFuncPtr = (MathFunc *) Tcl_GetHashValue(hPtr);
	    mathFuncPtr->builtinFuncIndex = i;
	    i++;
	}
    }

    info.token = UNKNOWN;
    info.objIndex = -1;
    info.funcName = NULL;
    info.next = string;
    info.originalExpr = string;
    info.lastChar = lastChar;
    info.hasOperators = 0;
    info.exprIsJustVarRef = 1;	/* will be set 0 if anything else is seen */

    /*
     * Get the first token then compile an expression.
     */

    result = GetToken(interp, &info, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    
    result = CompileCondExpr(interp, &info, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    if (info.token != END) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"syntax error in expression \"", string, "\"", (char *) NULL);
	result = TCL_ERROR;
	goto done;
    }
    if (!info.hasOperators) {
	/*
	 * Attempt to convert the primary's object to an int or double.
	 * This is done in order to support Tcl's policy of interpreting
	 * operands if at all possible as first integers, else
	 * floating-point numbers.
	 */
	
	TclEmitOpcode(INST_TRY_CVT_TO_NUMERIC, envPtr);
    }
    maxDepth = envPtr->maxStackDepth;

    done:
    envPtr->termOffset = (info.next - string);
    envPtr->maxStackDepth = maxDepth;
    envPtr->exprIsJustVarRef = info.exprIsJustVarRef;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileCondExpr --
 *
 *	This procedure compiles a Tcl conditional expression:
 *	condExpr ::= lorExpr ['?' condExpr ':' condExpr]
 *
 *	Note that this is the topmost recursive-descent parsing routine used
 *	by TclCompileExpr to compile expressions. It does not call an
 *	separate, higher-level "CompileExpr" procedure. This avoids an extra
 *	procedure call since such a procedure would only return the result
 *	of calling CompileCondExpr. Other recursive-descent procedures that
 *	need to parse expressions also call CompileCondExpr.
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileCondExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    JumpFixup jumpAroundThenFixup, jumpAroundElseFixup;
				/* Used to update or replace one-byte jumps
				 * around the then and else expressions when
				 * their target PCs are determined. */
    int elseCodeOffset, currCodeOffset, jumpDist, result;
    
    HERE("condExpr", 1);
    result = CompileLorExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;
    
    if (infoPtr->token == QUESTY) {
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '?' */
	if (result != TCL_OK) {
	    goto done;
	}

	/*
	 * Emit the jump around the "then" clause to the "else" condExpr if
	 * the test was false. We emit a one byte (relative) jump here, and
	 * replace it later with a four byte jump if the jump target is more
	 * than 127 bytes away.
	 */

	TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpAroundThenFixup);

	/*
	 * Compile the "then" expression. Note that if a subexpression
	 * is only a primary, we need to try to convert it to numeric.
	 * This is done in order to support Tcl's policy of interpreting
	 * operands if at all possible as first integers, else
	 * floating-point numbers.
	 */

	infoPtr->hasOperators = 0;
	infoPtr->exprIsJustVarRef = 0;
	result = CompileCondExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
	if (infoPtr->token != COLON) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "syntax error in expression \"", infoPtr->originalExpr,
		    "\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
	if (!infoPtr->hasOperators) {
	    TclEmitOpcode(INST_TRY_CVT_TO_NUMERIC, envPtr);
	}
	result = GetToken(interp, infoPtr, envPtr); /* skip over the ':' */
	if (result != TCL_OK) {
	    goto done;
	}

	/*
	 * Emit an unconditional jump around the "else" condExpr.
	 */

	TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP,
	        &jumpAroundElseFixup);

	/*
	 * Compile the "else" expression.
	 */

	infoPtr->hasOperators = 0;
	elseCodeOffset = TclCurrCodeOffset();
	result = CompileCondExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax(envPtr->maxStackDepth, maxDepth);
	if (!infoPtr->hasOperators) {
	    TclEmitOpcode(INST_TRY_CVT_TO_NUMERIC, envPtr);
	}

	/*
	 * Fix up the second jump: the unconditional jump around the "else"
	 * expression. If the distance is too great (> 127 bytes), replace
	 * it with a four byte instruction and move the instructions after
	 * the jump down.
	 */

	currCodeOffset = TclCurrCodeOffset();
	jumpDist = (currCodeOffset - jumpAroundElseFixup.codeOffset);
	if (TclFixupForwardJump(envPtr, &jumpAroundElseFixup, jumpDist, 127)) {
	    /*
	     * Update the else expression's starting code offset since it
	     * moved down 3 bytes too.
	     */
	    
	    elseCodeOffset += 3;
	}
	
	/*
	 * Now fix up the first branch: the jumpFalse after the test. If the
	 * distance is too great, replace it with a four byte instruction
	 * and update the code offsets for the commands in both the "then"
	 * and "else" expressions.
	 */

	jumpDist = (elseCodeOffset - jumpAroundThenFixup.codeOffset);
	TclFixupForwardJump(envPtr, &jumpAroundThenFixup, jumpDist, 127);

	infoPtr->hasOperators = 1;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileLorExpr --
 *
 *	This procedure compiles a Tcl logical or expression:
 *	lorExpr ::= landExpr {'||' landExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileLorExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    JumpFixupArray jumpFixupArray;
				/* Used to fix up the forward "short
				 * circuit" jump after each or-ed
				 * subexpression to just after the last
				 * subexpression. */
    JumpFixup jumpTrueFixup, jumpFixup;
    				/* Used to emit the jumps in the code to
				 * convert the first operand to a 0 or 1. */
    int fixupIndex, jumpDist, currCodeOffset, objIndex, j, result;
    Tcl_Obj *objPtr;
    
    HERE("lorExpr", 2);
    result = CompileLandExpr(interp, infoPtr, flags, envPtr);
    if ((result != TCL_OK) || (infoPtr->token != OR)) {
	return result;		/* envPtr->maxStackDepth is already set */
    }

    infoPtr->hasOperators = 1;
    infoPtr->exprIsJustVarRef = 0;
    maxDepth = envPtr->maxStackDepth;
    TclInitJumpFixupArray(&jumpFixupArray);
    while (infoPtr->token == OR) {
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '||' */
	if (result != TCL_OK) {
	    goto done;
	}

	if (jumpFixupArray.next == 0) {
	    /*
	     * Just the first "lor" operand is on the stack. The following
	     * is slightly ugly: we need to convert that first "lor" operand
	     * to a "0" or "1" to get the correct result if it is nonzero.
	     * Eventually we'll use a new instruction for this.
	     */

	    TclEmitForwardJump(envPtr, TCL_TRUE_JUMP, &jumpTrueFixup);
	    
	    objIndex = TclObjIndexForString("0", 1, /*allocStrRep*/ 0,
					    /*inHeap*/ 0, envPtr);
	    objPtr = envPtr->objArrayPtr[objIndex];

	    Tcl_InvalidateStringRep(objPtr);
	    objPtr->internalRep.longValue = 0;
	    objPtr->typePtr = &tclIntType;
	    
	    TclEmitPush(objIndex, envPtr);
	    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);

	    jumpDist = (TclCurrCodeOffset() - jumpTrueFixup.codeOffset);
	    if (TclFixupForwardJump(envPtr, &jumpTrueFixup, jumpDist, 127)) {
		panic("CompileLorExpr: bad jump distance %d\n", jumpDist);
	    }
	    objIndex = TclObjIndexForString("1", 1, /*allocStrRep*/ 0,
				            /*inHeap*/ 0, envPtr);
	    objPtr = envPtr->objArrayPtr[objIndex];

	    Tcl_InvalidateStringRep(objPtr);
	    objPtr->internalRep.longValue = 1;
	    objPtr->typePtr = &tclIntType;
	    
	    TclEmitPush(objIndex, envPtr);

	    jumpDist = (TclCurrCodeOffset() - jumpFixup.codeOffset);
	    if (TclFixupForwardJump(envPtr, &jumpFixup, jumpDist, 127)) {
		panic("CompileLorExpr: bad jump distance %d\n", jumpDist);
	    }
	}

	/*
	 * Duplicate the value on top of the stack to prevent the jump from
	 * consuming it.
	 */

	TclEmitOpcode(INST_DUP, envPtr);

	/*
	 * Emit the "short circuit" jump around the rest of the lorExp if
	 * the previous expression was true. We emit a one byte (relative)
	 * jump here, and replace it later with a four byte jump if the jump
	 * target is more than 127 bytes away.
	 */

	if (jumpFixupArray.next == jumpFixupArray.end) {
	    TclExpandJumpFixupArray(&jumpFixupArray);
	}
	fixupIndex = jumpFixupArray.next;
	jumpFixupArray.next++;
	TclEmitForwardJump(envPtr, TCL_TRUE_JUMP,
	        &(jumpFixupArray.fixup[fixupIndex]));
	
	/*
	 * Compile the subexpression.
	 */

	result = CompileLandExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	/*
	 * Emit a "logical or" instruction. This does not try to "short-
	 * circuit" the evaluation of both operands of a Tcl "||" operator,
	 * but instead ensures that we either have a "1" or a "0" result.
	 */

	TclEmitOpcode(INST_LOR, envPtr);
    }

    /*
     * Now that we know the target of the forward jumps, update the jumps
     * with the correct distance. Also, if the distance is too great (> 127
     * bytes), replace the jump with a four byte instruction and move the
     * instructions after the jump down.
     */
    
    for (j = jumpFixupArray.next;  j > 0;  j--) {
	fixupIndex = (j - 1);	/* process closest jump first */
	currCodeOffset = TclCurrCodeOffset();
	jumpDist = (currCodeOffset - jumpFixupArray.fixup[fixupIndex].codeOffset);
	TclFixupForwardJump(envPtr, &(jumpFixupArray.fixup[fixupIndex]), jumpDist, 127);
    }

    done:
    TclFreeJumpFixupArray(&jumpFixupArray);
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileLandExpr --
 *
 *	This procedure compiles a Tcl logical and expression:
 *	landExpr ::= bitOrExpr {'&&' bitOrExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileLandExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    JumpFixupArray jumpFixupArray;
				/* Used to fix up the forward "short
				 * circuit" jump after each and-ed
				 * subexpression to just after the last
				 * subexpression. */
    JumpFixup jumpTrueFixup, jumpFixup;
    				/* Used to emit the jumps in the code to
				 * convert the first operand to a 0 or 1. */
    int fixupIndex, jumpDist, currCodeOffset, objIndex, j, result;
    Tcl_Obj *objPtr;

    HERE("landExpr", 3);
    result = CompileBitOrExpr(interp, infoPtr, flags, envPtr);
    if ((result != TCL_OK) || (infoPtr->token != AND)) {
	return result;		/* envPtr->maxStackDepth is already set */
    }

    infoPtr->hasOperators = 1;
    infoPtr->exprIsJustVarRef = 0;
    maxDepth = envPtr->maxStackDepth;
    TclInitJumpFixupArray(&jumpFixupArray);
    while (infoPtr->token == AND) {
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '&&' */
	if (result != TCL_OK) {
	    goto done;
	}

	if (jumpFixupArray.next == 0) {
	    /*
	     * Just the first "land" operand is on the stack. The following
	     * is slightly ugly: we need to convert the first "land" operand
	     * to a "0" or "1" to get the correct result if it is
	     * nonzero. Eventually we'll use a new instruction.
	     */

	    TclEmitForwardJump(envPtr, TCL_TRUE_JUMP, &jumpTrueFixup);
	     
	    objIndex = TclObjIndexForString("0", 1, /*allocStrRep*/ 0,
				            /*inHeap*/ 0, envPtr);
	    objPtr = envPtr->objArrayPtr[objIndex];

	    Tcl_InvalidateStringRep(objPtr);
	    objPtr->internalRep.longValue = 0;
	    objPtr->typePtr = &tclIntType;
	    
	    TclEmitPush(objIndex, envPtr);
	    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);

	    jumpDist = (TclCurrCodeOffset() - jumpTrueFixup.codeOffset);
	    if (TclFixupForwardJump(envPtr, &jumpTrueFixup, jumpDist, 127)) {
		panic("CompileLandExpr: bad jump distance %d\n", jumpDist);
	    }
	    objIndex = TclObjIndexForString("1", 1, /*allocStrRep*/ 0,
				            /*inHeap*/ 0, envPtr);
	    objPtr = envPtr->objArrayPtr[objIndex];

	    Tcl_InvalidateStringRep(objPtr);
	    objPtr->internalRep.longValue = 1;
	    objPtr->typePtr = &tclIntType;
	    
	    TclEmitPush(objIndex, envPtr);

	    jumpDist = (TclCurrCodeOffset() - jumpFixup.codeOffset);
	    if (TclFixupForwardJump(envPtr, &jumpFixup, jumpDist, 127)) {
		panic("CompileLandExpr: bad jump distance %d\n", jumpDist);
	    }
	}

	/*
	 * Duplicate the value on top of the stack to prevent the jump from
	 * consuming it.
	 */

	TclEmitOpcode(INST_DUP, envPtr);

	/*
	 * Emit the "short circuit" jump around the rest of the landExp if
	 * the previous expression was false. We emit a one byte (relative)
	 * jump here, and replace it later with a four byte jump if the jump
	 * target is more than 127 bytes away.
	 */

	if (jumpFixupArray.next == jumpFixupArray.end) {
	    TclExpandJumpFixupArray(&jumpFixupArray);
	}
	fixupIndex = jumpFixupArray.next;
	jumpFixupArray.next++;
	TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
		&(jumpFixupArray.fixup[fixupIndex]));
	
	/*
	 * Compile the subexpression.
	 */

	result = CompileBitOrExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	/*
	 * Emit a "logical and" instruction. This does not try to "short-
	 * circuit" the evaluation of both operands of a Tcl "&&" operator,
	 * but instead ensures that we either have a "1" or a "0" result.
	 */

	TclEmitOpcode(INST_LAND, envPtr);
    }

    /*
     * Now that we know the target of the forward jumps, update the jumps
     * with the correct distance. Also, if the distance is too great (> 127
     * bytes), replace the jump with a four byte instruction and move the
     * instructions after the jump down.
     */
    
    for (j = jumpFixupArray.next;  j > 0;  j--) {
	fixupIndex = (j - 1);	/* process closest jump first */
	currCodeOffset = TclCurrCodeOffset();
	jumpDist = (currCodeOffset - jumpFixupArray.fixup[fixupIndex].codeOffset);
	TclFixupForwardJump(envPtr, &(jumpFixupArray.fixup[fixupIndex]), jumpDist, 127);
    }

    done:
    TclFreeJumpFixupArray(&jumpFixupArray);
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileBitOrExpr --
 *
 *	This procedure compiles a Tcl bitwise or expression:
 *	bitOrExpr ::= bitXorExpr {'|' bitXorExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileBitOrExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int result;

    HERE("bitOrExpr", 4);
    result = CompileBitXorExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;
    
    while (infoPtr->token == BIT_OR) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '|' */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileBitXorExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);
	
	TclEmitOpcode(INST_BITOR, envPtr);
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileBitXorExpr --
 *
 *	This procedure compiles a Tcl bitwise exclusive or expression:
 *	bitXorExpr ::= bitAndExpr {'^' bitAndExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileBitXorExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int result;

    HERE("bitXorExpr", 5);
    result = CompileBitAndExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;
    
    while (infoPtr->token == BIT_XOR) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '^' */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileBitAndExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);
	
	TclEmitOpcode(INST_BITXOR, envPtr);
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileBitAndExpr --
 *
 *	This procedure compiles a Tcl bitwise and expression:
 *	bitAndExpr ::= equalityExpr {'&' equalityExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileBitAndExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int result;

    HERE("bitAndExpr", 6);
    result = CompileEqualityExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;
    
    while (infoPtr->token == BIT_AND) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '&' */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileEqualityExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);
	
	TclEmitOpcode(INST_BITAND, envPtr);
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileEqualityExpr --
 *
 *	This procedure compiles a Tcl equality (inequality) expression:
 *	equalityExpr ::= relationalExpr {('==' | '!=') relationalExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileEqualityExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int op, result;

    HERE("equalityExpr", 7);
    result = CompileRelationalExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;

    op = infoPtr->token;
    while ((op == EQUAL) || (op == NEQ)) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over == or != */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileRelationalExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	if (op == EQUAL) {
	    TclEmitOpcode(INST_EQ, envPtr);
	} else {
	    TclEmitOpcode(INST_NEQ, envPtr);
	}
	
	op = infoPtr->token;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileRelationalExpr --
 *
 *	This procedure compiles a Tcl relational expression:
 *	relationalExpr ::= shiftExpr {('<' | '>' | '<=' | '>=') shiftExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileRelationalExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int op, result;

    HERE("relationalExpr", 8);
    result = CompileShiftExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;

    op = infoPtr->token;
    while ((op == LESS) || (op == GREATER) || (op == LEQ) || (op == GEQ)) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over the op */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileShiftExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	switch (op) {
	case LESS:
	    TclEmitOpcode(INST_LT, envPtr);
	    break;
	case GREATER:
	    TclEmitOpcode(INST_GT, envPtr);
	    break;
	case LEQ:
	    TclEmitOpcode(INST_LE, envPtr);
	    break;
	case GEQ:
	    TclEmitOpcode(INST_GE, envPtr);
	    break;
	}

	op = infoPtr->token;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileShiftExpr --
 *
 *	This procedure compiles a Tcl shift expression:
 *	shiftExpr ::= addExpr {('<<' | '>>') addExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileShiftExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int op, result;

    HERE("shiftExpr", 9);
    result = CompileAddExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;

    op = infoPtr->token;
    while ((op == LEFT_SHIFT) || (op == RIGHT_SHIFT)) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over << or >> */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileAddExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	if (op == LEFT_SHIFT) {
	    TclEmitOpcode(INST_LSHIFT, envPtr);
	} else {
	    TclEmitOpcode(INST_RSHIFT, envPtr);
	}

	op = infoPtr->token;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileAddExpr --
 *
 *	This procedure compiles a Tcl addition expression:
 *	addExpr ::= multiplyExpr {('+' | '-') multiplyExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileAddExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int op, result;

    HERE("addExpr", 10);
    result = CompileMultiplyExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;

    op = infoPtr->token;
    while ((op == PLUS) || (op == MINUS)) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over + or - */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileMultiplyExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	if (op == PLUS) {
	    TclEmitOpcode(INST_ADD, envPtr);
	} else {
	    TclEmitOpcode(INST_SUB, envPtr);
	}

	op = infoPtr->token;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileMultiplyExpr --
 *
 *	This procedure compiles a Tcl multiply expression:
 *	multiplyExpr ::= unaryExpr {('*' | '/' | '%') unaryExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileMultiplyExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int op, result;

    HERE("multiplyExpr", 11);
    result = CompileUnaryExpr(interp, infoPtr, flags, envPtr);
    if (result != TCL_OK) {
	goto done;
    }
    maxDepth = envPtr->maxStackDepth;

    op = infoPtr->token;
    while ((op == MULT) || (op == DIVIDE) || (op == MOD)) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over * or / */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileUnaryExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = TclMax((envPtr->maxStackDepth + 1), maxDepth);

	if (op == MULT) {
	    TclEmitOpcode(INST_MULT, envPtr);
	} else if (op == DIVIDE) {
	    TclEmitOpcode(INST_DIV, envPtr);
	} else {
	    TclEmitOpcode(INST_MOD, envPtr);
	}

	op = infoPtr->token;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileUnaryExpr --
 *
 *	This procedure compiles a Tcl unary expression:
 *	unaryExpr ::= ('+' | '-' | '~' | '!') unaryExpr | primaryExpr
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileUnaryExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int op, result;

    HERE("unaryExpr", 12);
    op = infoPtr->token;
    if ((op == PLUS) || (op == MINUS) || (op == BIT_NOT) || (op == NOT)) {
	infoPtr->hasOperators = 1;
	infoPtr->exprIsJustVarRef = 0;
	result = GetToken(interp, infoPtr, envPtr); /* skip over the op */
	if (result != TCL_OK) {
	    goto done;
	}

	result = CompileUnaryExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = envPtr->maxStackDepth;

	switch (op) {
	case PLUS:
	    TclEmitOpcode(INST_UPLUS, envPtr);
	    break;
	case MINUS:
	    TclEmitOpcode(INST_UMINUS, envPtr);
	    break;
	case BIT_NOT:
	    TclEmitOpcode(INST_BITNOT, envPtr);
	    break;
	case NOT:
	    TclEmitOpcode(INST_LNOT, envPtr);
	    break;
	}
    } else {			/* must be a primaryExpr */
	result = CompilePrimaryExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = envPtr->maxStackDepth;
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompilePrimaryExpr --
 *
 *	This procedure compiles a Tcl primary expression:
 *	primaryExpr ::= literal | varReference | quotedString |
 *			'[' command ']' | mathFuncCall | '(' condExpr ')'
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the expression.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompilePrimaryExpr(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    int theToken;
    char *dollarPtr, *quotePtr, *cmdPtr, *termPtr;
    int result = TCL_OK;

    /*
     * We emit tryCvtToNumeric instructions after most of these primary
     * expressions in order to support Tcl's policy of interpreting operands
     * as first integers if possible, otherwise floating-point numbers if
     * possible.
     */

    HERE("primaryExpr", 13);
    theToken = infoPtr->token;

    if (theToken != DOLLAR) {
	infoPtr->exprIsJustVarRef = 0;
    }
    switch (theToken) {
    case LITERAL:		/* int, double, or string in braces */
	TclEmitPush(infoPtr->objIndex, envPtr);
	maxDepth = 1;
	break;
	
    case DOLLAR:		/* $var variable reference */
	dollarPtr = (infoPtr->next - 1);
	envPtr->pushSimpleWords = 1;
	result = TclCompileDollarVar(interp, dollarPtr,
		infoPtr->lastChar, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = envPtr->maxStackDepth;
	infoPtr->next = (dollarPtr + envPtr->termOffset);
	break;
	
    case QUOTE:			/* quotedString */
	quotePtr = infoPtr->next;
	envPtr->pushSimpleWords = 1;
	result = TclCompileQuotes(interp, quotePtr,
		infoPtr->lastChar, '"', flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = envPtr->maxStackDepth;
	infoPtr->next = (quotePtr + envPtr->termOffset);
	break;
	
    case OPEN_BRACKET:		/* '[' command ']' */
	cmdPtr = infoPtr->next;
	envPtr->pushSimpleWords = 1;
	result = TclCompileString(interp, cmdPtr,
		infoPtr->lastChar, (flags | TCL_BRACKET_TERM), envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	termPtr = (cmdPtr + envPtr->termOffset);
	if (*termPtr == ']') {
	    infoPtr->next = (termPtr + 1); /* advance over the ']'. */
	} else if (termPtr == infoPtr->lastChar) {
	    /*
	     * Missing ] at end of nested command.
	     */
	    
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	            "missing close-bracket", -1);
	    result = TCL_ERROR;
	    goto done;
	} else {
	    panic("CompilePrimaryExpr: unexpected termination char '%c' for nested command\n", *termPtr);
	}
	maxDepth = envPtr->maxStackDepth;
	break;
	
    case FUNC_NAME:
	result = CompileMathFuncCall(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = envPtr->maxStackDepth;
	break;
	
    case OPEN_PAREN:
	result = GetToken(interp, infoPtr, envPtr); /* skip over the '(' */
	if (result != TCL_OK) {
	    goto done;
	}
	result = CompileCondExpr(interp, infoPtr, flags, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
	maxDepth = envPtr->maxStackDepth;
	if (infoPtr->token != CLOSE_PAREN) {
	    goto syntaxError;
	}
	break;
	
    default:
	goto syntaxError;
    }

    if (theToken != FUNC_NAME) {
	/*
	 * Advance to the next token before returning.
	 */
	
	result = GetToken(interp, infoPtr, envPtr);
	if (result != TCL_OK) {
	    goto done;
	}
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;

    syntaxError:
    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
	    "syntax error in expression \"", infoPtr->originalExpr,
	    "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileMathFuncCall --
 *
 *	This procedure compiles a call on a math function in an expression:
 *	mathFuncCall ::= funcName '(' [condExpr {',' condExpr}] ')'
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 *	envPtr->maxStackDepth is updated with the maximum number of stack
 *	elements needed to execute the function.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the math function at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileMathFuncCall(interp, infoPtr, flags, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    ExprInfo *infoPtr;		/* Describes the compilation state for the
				 * expression being compiled. */
    int flags;			/* Flags to control compilation (same as
				 * passed to Tcl_Eval). */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Interp *iPtr = (Interp *) interp;
    int maxDepth = 0;		/* Maximum number of stack elements needed
				 * to execute the expression. */
    MathFunc *mathFuncPtr;	/* Info about math function. */
    int objIndex;		/* The object array index for an object
				 * holding the function name if it is not
				 * builtin. */
    Tcl_HashEntry *hPtr;
    char *p, *funcName;
    char savedChar;
    int result, i;

    /*
     * infoPtr->funcName points to the first character of the math
     * function's name. Look for the end of its name and look up the
     * MathFunc record for the function.
     */

    funcName = p = infoPtr->funcName;
    while (isalnum(UCHAR(*p)) || (*p == '_')) {
	p++;
    }
    infoPtr->next = p;
    
    result = GetToken(interp, infoPtr, envPtr); /* skip over func name */
    if (result != TCL_OK) {
	goto done;
    }
    if (infoPtr->token != OPEN_PAREN) {
	goto syntaxError;
    }
    result = GetToken(interp, infoPtr, envPtr); /* skip over '(' */
    if (result != TCL_OK) {
	goto done;
    }
    
    savedChar = *p;
    *p = 0;
    hPtr = Tcl_FindHashEntry(&iPtr->mathFuncTable, funcName);
    if (hPtr == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"unknown math function \"", funcName, "\"", (char *) NULL);
	result = TCL_ERROR;
	*p = savedChar;
	goto done;
    }
    mathFuncPtr = (MathFunc *) Tcl_GetHashValue(hPtr);

    /*
     * If not a builtin function, push an object with the function's name.
     */

    if (mathFuncPtr->builtinFuncIndex < 0) {   /* not builtin */
	objIndex = TclObjIndexForString(funcName, -1, /*allocStrRep*/ 1,
				        /*inHeap*/ 0, envPtr);
	TclEmitPush(objIndex, envPtr);
	maxDepth = 1;
    }

    /*
     * Restore the saved character after the function name.
     */

    *p = savedChar;

    /*
     * Compile the arguments for the function, if there are any.
     */

    if (mathFuncPtr->numArgs > 0) {
	for (i = 0;  ;  i++) {
	    result = CompileCondExpr(interp, infoPtr, flags, envPtr);
	    if (result != TCL_OK) {
		goto done;
	    }
    
	    /*
	     * Check for a ',' between arguments or a ')' ending the
	     * argument list.
	     */
    
	    if (i == (mathFuncPtr->numArgs-1)) {
		if (infoPtr->token == CLOSE_PAREN) {
		    break;	/* exit the argument parsing loop */
		} else if (infoPtr->token == COMMA) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "too many arguments for math function", -1);
		    result = TCL_ERROR;
		    goto done;
		} else {
		    goto syntaxError;
		}
	    }
	    if (infoPtr->token != COMMA) {
		if (infoPtr->token == CLOSE_PAREN) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		            "too few arguments for math function", -1);
		    result = TCL_ERROR;
		    goto done;
		} else {
		    goto syntaxError;
		}
	    }
	    result = GetToken(interp, infoPtr, envPtr); /* skip over , */
	    if (result != TCL_OK) {
		goto done;
	    }
	    maxDepth++;
	}
    }

    if (infoPtr->token != CLOSE_PAREN) {
	goto syntaxError;
    }
    result = GetToken(interp, infoPtr, envPtr); /* skip over ')' */
    if (result != TCL_OK) {
	goto done;
    }
    
    /*
     * Compile the call on the math function. Note that the "objc" argument
     * count for non-builtin functions is incremented by 1 to include the
     * the function name itself.
     */

    if (mathFuncPtr->builtinFuncIndex >= 0) { /* a builtin function */
	TclEmitInstUInt1(INST_CALL_BUILTIN_FUNC1,
			mathFuncPtr->builtinFuncIndex, envPtr);
    } else {
	TclEmitInstUInt1(INST_CALL_FUNC1, (mathFuncPtr->numArgs+1), envPtr);
    }

    done:
    envPtr->maxStackDepth = maxDepth;
    return result;

    syntaxError:
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"syntax error in expression \"", infoPtr->originalExpr,
		"\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetToken --
 *
 *	Lexical scanner used to compile expressions: parses a single 
 *	operator or other syntactic element from an expression string.
 *
 * Results:
 *	TCL_OK is returned unless an error occurred. In that case a standard
 *	Tcl error is returned, using the interpreter's result to hold an
 *	error message. TCL_ERROR is returned if an integer overflow, or a
 *	floating-point overflow or underflow occurred while reading in a
 *	number. If the lexical analysis is successful, infoPtr->token refers
 *	to the next symbol in the expression string, and infoPtr->next is
 *	advanced past the token. Also, if the token is a integer, double, or
 *	string literal, then infoPtr->objIndex the index of an object
 *	holding the value in the code's object table; otherwise is NULL.
 *
 * Side effects:
 *	Object are added to envPtr to hold the values of scanned literal
 *	integers, doubles, or strings.
 *
 *----------------------------------------------------------------------
 */

static int
GetToken(interp, infoPtr, envPtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting. */
    register ExprInfo *infoPtr;         /* Describes the state of the
					 * compiling the expression,
					 * including the resulting token. */
    CompileEnv *envPtr;			/* Holds objects that store literal
					 * values that are scanned. */
{
    register char *src;		/* Points to current source char. */
    register char c;		/* The current char. */
    register int type;		/* Current char's CHAR_TYPE type. */
    char *termPtr;		/* Points to char terminating a literal. */
    char savedChar;		/* Holds the character termporarily replaced
				 * by a null character during processing of
				 * literal tokens. */
    int objIndex;		/* The object array index for an object
				 * holding a scanned literal. */
    long longValue;		/* Value of a scanned integer literal. */
    double doubleValue;		/* Value of a scanned double literal. */
    Tcl_Obj *objPtr;

    /*
     * First initialize the scanner's "result" fields to default values.
     */
    
    infoPtr->token = UNKNOWN;
    infoPtr->objIndex = -1;
    infoPtr->funcName = NULL;

    /*
     * Scan over leading white space at the start of a token. Note that a
     * backslash-newline is treated as a space.
     */

    src = infoPtr->next;
    c = *src;
    type = CHAR_TYPE(src, infoPtr->lastChar);
    while ((type & (TCL_SPACE | TCL_BACKSLASH)) || (c == '\n')) {
	if (type == TCL_BACKSLASH) {
	    if (src[1] == '\n') {
		src += 2;
	    } else {
		break;	/* no longer white space */
	    }
	} else {
	    src++;
	}
	c = *src;
	type = CHAR_TYPE(src, infoPtr->lastChar);
    }
    if (src == infoPtr->lastChar) {
	infoPtr->token = END;
	infoPtr->next = src;
	return TCL_OK;
    }

    /*
     * Try to parse the token first as an integer or floating-point
     * number. Don't check for a number if the first character is "+" or
     * "-". If we did, we might treat a binary operator as unary by mistake,
     * which would eventually cause a syntax error.
     */

    if ((*src != '+') && (*src != '-')) {
	int startsWithDigit = isdigit(UCHAR(*src));
	
	if (startsWithDigit && TclLooksLikeInt(src)) {
	    errno = 0;
	    longValue = strtoul(src, &termPtr, 0);
	    if (errno == ERANGE) {
		char *s = "integer value too large to represent";
		
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
		Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW", s,
			(char *) NULL);
		return TCL_ERROR;
	    }

	    /*
	     * Find/create an object in envPtr's object array that contains
	     * the integer.
	     */
	    
	    savedChar = *termPtr;
	    *termPtr = '\0';
	    objIndex = TclObjIndexForString(src, termPtr - src,
		    /*allocStrRep*/ 0, /*inHeap*/ 0, envPtr);
	    *termPtr = savedChar;  /* restore the saved char */

	    objPtr = envPtr->objArrayPtr[objIndex];
	    Tcl_InvalidateStringRep(objPtr);
	    objPtr->internalRep.longValue = longValue;
	    objPtr->typePtr = &tclIntType;
	    
	    infoPtr->token = LITERAL;
	    infoPtr->objIndex = objIndex;
	    infoPtr->next = termPtr;
	    return TCL_OK;
	} else if (startsWithDigit || (*src == '.')
	        || (*src == 'n') || (*src == 'N')) {
	    errno = 0;
	    doubleValue = strtod(src, &termPtr);
	    if (termPtr != src) {
		if (errno != 0) {
		    TclExprFloatError(interp, doubleValue);
		    return TCL_ERROR;
		}

		/*
		 * Find/create an object in the object array containing the
		 * double.
		 */
		
		savedChar = *termPtr;
		*termPtr = '\0';
		objIndex = TclObjIndexForString(src, termPtr - src,
			/*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
		*termPtr = savedChar;  /* restore the saved char */
		
		objPtr = envPtr->objArrayPtr[objIndex];
		objPtr->internalRep.doubleValue = doubleValue;
		objPtr->typePtr = &tclDoubleType;
		
		infoPtr->token = LITERAL;
		infoPtr->objIndex = objIndex;
		infoPtr->next = termPtr;
		return TCL_OK;
	    }
	}
    }

    /*
     * Not an integer or double literal. Check next for a string literal
     * in braces.
     */

    if (*src == '{') {
	int level = 0;		 /* The {} nesting level. */
	int hasBackslashNL = 0;  /* Nonzero if '\newline' was found. */
	char *string = src+1;	 /* Points just after the starting '{'. */
	char *last;		 /* Points just before terminating '}'. */
	int numChars;		 /* Number of chars in braced string. */
	char savedChar;		 /* Holds the character from string
				  * termporarily replaced by a null char
				  * during braced string processing. */
	int numRead;

	/*
	 * Check first for any backslash-newlines, since we must treat
	 * backslash-newlines specially (they must be replaced by spaces).
	 */
	
	while (1) {
	    if (src == infoPtr->lastChar) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "missing close-brace", -1);
		return TCL_ERROR;
	    } else if (CHAR_TYPE(src, infoPtr->lastChar) == TCL_NORMAL) {
		src++;
		continue;
	    }
	    c = *src++;
	    if (c == '{') {
		level++;
	    } else if (c == '}') {
		--level;
		if (level == 0) {
		    last = (src - 2); /* i.e. just before terminating } */
		    break;
		}
	    } else if (c == '\\') {
		if (*src == '\n') {
		    hasBackslashNL = 1;
		}
		(void) Tcl_Backslash(src-1, &numRead);
		src += numRead - 1;
	    }
	}

	/*
	 * Create a string object for the braced string. This starts at
	 * "string" and ends just after "last" (which points to the final
	 * character before the terminating '}'). If backslash-newlines were
	 * found, we copy characters one at a time into a heap-allocated
	 * buffer and do backslash-newline substitutions.
	 */
	
	numChars = (last - string + 1);
	savedChar = string[numChars];
	string[numChars] = '\0';
	if (hasBackslashNL && (numChars > 0)) {
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
	    objIndex = TclObjIndexForString(buffer, dst - buffer,
		    /*allocStrRep*/ 1, /*inHeap*/ 1, envPtr);
	} else {
	    objIndex = TclObjIndexForString(string, numChars,
		    /*allocStrRep*/ 1, /*inHeap*/ 0, envPtr);
	}
	string[numChars] = savedChar;   /* restore the saved char */

	infoPtr->token = LITERAL;
	infoPtr->objIndex = objIndex;
	infoPtr->next = src;
	return TCL_OK;
    }

    /*
     * Not an literal value.
     */

    infoPtr->next = src+1;   /* assume a 1 char token and advance over it */
    switch (*src) {
	case '[':
	    infoPtr->token = OPEN_BRACKET;
	    return TCL_OK;

	case ']':
	    infoPtr->token = CLOSE_BRACKET;
	    return TCL_OK;

	case '(':
	    infoPtr->token = OPEN_PAREN;
	    return TCL_OK;

	case ')':
	    infoPtr->token = CLOSE_PAREN;
	    return TCL_OK;

	case '$':
	    infoPtr->token = DOLLAR;
	    return TCL_OK;

	case '"':
	    infoPtr->token = QUOTE;
	    return TCL_OK;

	case ',':
	    infoPtr->token = COMMA;
	    return TCL_OK;

	case '*':
	    infoPtr->token = MULT;
	    return TCL_OK;

	case '/':
	    infoPtr->token = DIVIDE;
	    return TCL_OK;

	case '%':
	    infoPtr->token = MOD;
	    return TCL_OK;

	case '+':
	    infoPtr->token = PLUS;
	    return TCL_OK;

	case '-':
	    infoPtr->token = MINUS;
	    return TCL_OK;

	case '?':
	    infoPtr->token = QUESTY;
	    return TCL_OK;

	case ':':
	    infoPtr->token = COLON;
	    return TCL_OK;

	case '<':
	    switch (src[1]) {
		case '<':
		    infoPtr->next = src+2;
		    infoPtr->token = LEFT_SHIFT;
		    break;
		case '=':
		    infoPtr->next = src+2;
		    infoPtr->token = LEQ;
		    break;
		default:
		    infoPtr->token = LESS;
		    break;
	    }
	    return TCL_OK;

	case '>':
	    switch (src[1]) {
		case '>':
		    infoPtr->next = src+2;
		    infoPtr->token = RIGHT_SHIFT;
		    break;
		case '=':
		    infoPtr->next = src+2;
		    infoPtr->token = GEQ;
		    break;
		default:
		    infoPtr->token = GREATER;
		    break;
	    }
	    return TCL_OK;

	case '=':
	    if (src[1] == '=') {
		infoPtr->next = src+2;
		infoPtr->token = EQUAL;
	    } else {
		infoPtr->token = UNKNOWN;
	    }
	    return TCL_OK;

	case '!':
	    if (src[1] == '=') {
		infoPtr->next = src+2;
		infoPtr->token = NEQ;
	    } else {
		infoPtr->token = NOT;
	    }
	    return TCL_OK;

	case '&':
	    if (src[1] == '&') {
		infoPtr->next = src+2;
		infoPtr->token = AND;
	    } else {
		infoPtr->token = BIT_AND;
	    }
	    return TCL_OK;

	case '^':
	    infoPtr->token = BIT_XOR;
	    return TCL_OK;

	case '|':
	    if (src[1] == '|') {
		infoPtr->next = src+2;
		infoPtr->token = OR;
	    } else {
		infoPtr->token = BIT_OR;
	    }
	    return TCL_OK;

	case '~':
	    infoPtr->token = BIT_NOT;
	    return TCL_OK;

	default:
	    if (isalpha(UCHAR(*src))) {
		infoPtr->token = FUNC_NAME;
		infoPtr->funcName = src;
		while (isalnum(UCHAR(*src)) || (*src == '_')) {
		    src++;
		}
		infoPtr->next = src;
		return TCL_OK;
	    }
	    infoPtr->next = src+1;
	    infoPtr->token = UNKNOWN;
	    return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateMathFunc --
 *
 *	Creates a new math function for expressions in a given
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The function defined by "name" is created or redefined. If the
 *	function already exists then its definition is replaced; this
 *	includes the builtin functions. Redefining a builtin function forces
 *	all existing code to be invalidated since that code may be compiled
 *	using an instruction specific to the replaced function. In addition,
 *	redefioning a non-builtin function will force existing code to be
 *	invalidated if the number of arguments has changed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateMathFunc(interp, name, numArgs, argTypes, proc, clientData)
    Tcl_Interp *interp;			/* Interpreter in which function is
					 * to be available. */
    char *name;				/* Name of function (e.g. "sin"). */
    int numArgs;			/* Nnumber of arguments required by
					 * function. */
    Tcl_ValueType *argTypes;		/* Array of types acceptable for
					 * each argument. */
    Tcl_MathProc *proc;			/* Procedure that implements the
					 * math function. */
    ClientData clientData;		/* Additional value to pass to the
					 * function. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hPtr;
    MathFunc *mathFuncPtr;
    int new, i;

    hPtr = Tcl_CreateHashEntry(&iPtr->mathFuncTable, name, &new);
    if (new) {
	Tcl_SetHashValue(hPtr, ckalloc(sizeof(MathFunc)));
    }
    mathFuncPtr = (MathFunc *) Tcl_GetHashValue(hPtr);

    if (!new) {	
	if (mathFuncPtr->builtinFuncIndex >= 0) {
	    /*
	     * We are redefining a builtin math function. Invalidate the
             * interpreter's existing code by incrementing its
             * compileEpoch member. This field is checked in Tcl_EvalObj
             * and ObjInterpProc, and code whose compilation epoch doesn't
             * match is recompiled. Newly compiled code will no longer
             * treat the function as builtin.
	     */

	    iPtr->compileEpoch++;
	} else {
	    /*
	     * A non-builtin function is being redefined. We must invalidate
             * existing code if the number of arguments has changed. This
	     * is because existing code was compiled assuming that number.
	     */

	    if (numArgs != mathFuncPtr->numArgs) {
		iPtr->compileEpoch++;
	    }
	}
    }
    
    mathFuncPtr->builtinFuncIndex = -1;	/* can't be a builtin function */
    if (numArgs > MAX_MATH_ARGS) {
	numArgs = MAX_MATH_ARGS;
    }
    mathFuncPtr->numArgs = numArgs;
    for (i = 0;  i < numArgs;  i++) {
	mathFuncPtr->argTypes[i] = argTypes[i];
    }
    mathFuncPtr->proc = proc;
    mathFuncPtr->clientData = clientData;
}
