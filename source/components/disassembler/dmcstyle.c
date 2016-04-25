/*******************************************************************************
 *
 * Module Name: dmcstyle - Support for C-style operator disassembly
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpi.h"
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdebug.h"


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmcstyle")


/* Local prototypes */

static char *
AcpiDmGetCompoundSymbol (
   UINT16                   AslOpcode);

static void
AcpiDmPromoteTarget (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *Target);

static BOOLEAN
AcpiDmIsValidTarget (
    ACPI_PARSE_OBJECT       *Op);

static BOOLEAN
AcpiDmIsTargetAnOperand (
    ACPI_PARSE_OBJECT       *Target,
    ACPI_PARSE_OBJECT       *Operand,
    BOOLEAN                 TopLevel);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCheckForSymbolicOpcode
 *
 * PARAMETERS:  Op                  - Current parse object
 *              Walk                - Current parse tree walk info
 *
 * RETURN:      TRUE if opcode can be converted to symbolic, FALSE otherwise
 *
 * DESCRIPTION: This is the main code that implements disassembly of AML code
 *              to C-style operators. Called during descending phase of the
 *              parse tree walk.
 *
 ******************************************************************************/

BOOLEAN
AcpiDmCheckForSymbolicOpcode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OP_WALK_INFO       *Info)
{
    char                    *OperatorSymbol = NULL;
    ACPI_PARSE_OBJECT       *Child1;
    ACPI_PARSE_OBJECT       *Child2;
    ACPI_PARSE_OBJECT       *Target;


    /* Exit immediately if ASL+ not enabled */

    if (!AcpiGbl_CstyleDisassembly)
    {
        return (FALSE);
    }

    /* Get the first operand */

    Child1 = AcpiPsGetArg (Op, 0);
    if (!Child1)
    {
        return (FALSE);
    }

    /* Get the second operand */

    Child2 = Child1->Common.Next;

    /* Setup the operator string for this opcode */

    switch (Op->Common.AmlOpcode)
    {
    case AML_ADD_OP:
        OperatorSymbol = " + ";
        break;

    case AML_SUBTRACT_OP:
        OperatorSymbol = " - ";
        break;

    case AML_MULTIPLY_OP:
        OperatorSymbol = " * ";
        break;

    case AML_DIVIDE_OP:
        OperatorSymbol = " / ";
        break;

    case AML_MOD_OP:
        OperatorSymbol = " % ";
        break;

    case AML_SHIFT_LEFT_OP:
        OperatorSymbol = " << ";
        break;

    case AML_SHIFT_RIGHT_OP:
        OperatorSymbol = " >> ";
        break;

    case AML_BIT_AND_OP:
        OperatorSymbol = " & ";
        break;

    case AML_BIT_OR_OP:
        OperatorSymbol = " | ";
        break;

    case AML_BIT_XOR_OP:
        OperatorSymbol = " ^ ";
        break;

    /* Logical operators, no target */

    case AML_LAND_OP:
        OperatorSymbol = " && ";
        break;

    case AML_LEQUAL_OP:
        OperatorSymbol = " == ";
        break;

    case AML_LGREATER_OP:
        OperatorSymbol = " > ";
        break;

    case AML_LLESS_OP:
        OperatorSymbol = " < ";
        break;

    case AML_LOR_OP:
        OperatorSymbol = " || ";
        break;

    case AML_LNOT_OP:
        /*
         * Check for the LNOT sub-opcodes. These correspond to
         * LNotEqual, LLessEqual, and LGreaterEqual. There are
         * no actual AML opcodes for these operators.
         */
        switch (Child1->Common.AmlOpcode)
        {
        case AML_LEQUAL_OP:
            OperatorSymbol = " != ";
            break;

        case AML_LGREATER_OP:
            OperatorSymbol = " <= ";
            break;

        case AML_LLESS_OP:
            OperatorSymbol = " >= ";
            break;

        default:

            /* Unary LNOT case, emit "!" immediately */

            AcpiOsPrintf ("!");
            return (TRUE);
        }

        Child1->Common.DisasmOpcode = ACPI_DASM_LNOT_SUFFIX;
        Op->Common.DisasmOpcode = ACPI_DASM_LNOT_PREFIX;
        Op->Common.DisasmFlags |= ACPI_PARSEOP_COMPOUND_ASSIGNMENT;

        /* Save symbol string in the next child (not peer) */

        Child2 = AcpiPsGetArg (Child1, 0);
        if (!Child2)
        {
            return (FALSE);
        }

        Child2->Common.OperatorSymbol = OperatorSymbol;
        return (TRUE);

    case AML_INDEX_OP:
        /*
         * Check for constant source operand. Note: although technically
         * legal syntax, the iASL compiler does not support this with
         * the symbolic operators for Index(). It doesn't make sense to
         * use Index() with a constant anyway.
         */
        if ((Child1->Common.AmlOpcode == AML_STRING_OP)  ||
            (Child1->Common.AmlOpcode == AML_BUFFER_OP)  ||
            (Child1->Common.AmlOpcode == AML_PACKAGE_OP) ||
            (Child1->Common.AmlOpcode == AML_VAR_PACKAGE_OP))
        {
            Op->Common.DisasmFlags |= ACPI_PARSEOP_CLOSING_PAREN;
            return (FALSE);
        }

        /* Index operator is [] */

        Child1->Common.OperatorSymbol = " [";
        Child2->Common.OperatorSymbol = "]";
        break;

    /* Unary operators */

    case AML_DECREMENT_OP:
        OperatorSymbol = "--";
        break;

    case AML_INCREMENT_OP:
        OperatorSymbol = "++";
        break;

    case AML_BIT_NOT_OP:
    case AML_STORE_OP:
        OperatorSymbol = NULL;
        break;

    default:
        return (FALSE);
    }

    if (Child1->Common.DisasmOpcode == ACPI_DASM_LNOT_SUFFIX)
    {
        return (TRUE);
    }

    /*
     * This is the key to how the disassembly of the C-style operators
     * works. We save the operator symbol in the first child, thus
     * deferring symbol output until after the first operand has been
     * emitted.
     */
    if (!Child1->Common.OperatorSymbol)
    {
        Child1->Common.OperatorSymbol = OperatorSymbol;
    }

    /*
     * Check for a valid target as the 3rd (or sometimes 2nd) operand
     *
     * Compound assignment operator support:
     * Attempt to optimize constructs of the form:
     *      Add (Local1, 0xFF, Local1)
     * to:
     *      Local1 += 0xFF
     *
     * Only the math operators and Store() have a target.
     * Logicals have no target.
     */
    switch (Op->Common.AmlOpcode)
    {
    case AML_ADD_OP:
    case AML_SUBTRACT_OP:
    case AML_MULTIPLY_OP:
    case AML_DIVIDE_OP:
    case AML_MOD_OP:
    case AML_SHIFT_LEFT_OP:
    case AML_SHIFT_RIGHT_OP:
    case AML_BIT_AND_OP:
    case AML_BIT_OR_OP:
    case AML_BIT_XOR_OP:

        /* Target is 3rd operand */

        Target = Child2->Common.Next;
        if (Op->Common.AmlOpcode == AML_DIVIDE_OP)
        {
            /*
             * Divide has an extra target operand (Remainder).
             * If this extra target is specified, it cannot be converted
             * to a C-style operator
             */
            if (AcpiDmIsValidTarget (Target))
            {
                Child1->Common.OperatorSymbol = NULL;
                return (FALSE);
            }

            Target->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
            Target = Target->Common.Next;
        }

        /* Parser should ensure there is at least a placeholder target */

        if (!Target)
        {
            return (FALSE);
        }

        if (!AcpiDmIsValidTarget (Target))
        {
            /* Not a valid target (placeholder only, from parser) */
            break;
        }

        /*
         * Promote the target up to the first child in the parse
         * tree. This is done because the target will be output
         * first, in the form:
         *     <Target> = Operands...
         */
        AcpiDmPromoteTarget (Op, Target);

        /* Check operands for conversion to a "Compound Assignment" */

        switch (Op->Common.AmlOpcode)
        {
            /* Commutative operators */

        case AML_ADD_OP:
        case AML_MULTIPLY_OP:
        case AML_BIT_AND_OP:
        case AML_BIT_OR_OP:
        case AML_BIT_XOR_OP:
            /*
             * For the commutative operators, we can convert to a
             * compound statement only if at least one (either) operand
             * is the same as the target.
             *
             *      Add (A, B, A) --> A += B
             *      Add (B, A, A) --> A += B
             *      Add (B, C, A) --> A = (B + C)
             */
            if ((AcpiDmIsTargetAnOperand (Target, Child1, TRUE)) ||
                (AcpiDmIsTargetAnOperand (Target, Child2, TRUE)))
            {
                Target->Common.OperatorSymbol =
                    AcpiDmGetCompoundSymbol (Op->Common.AmlOpcode);

                /* Convert operator to compound assignment */

                Op->Common.DisasmFlags |= ACPI_PARSEOP_COMPOUND_ASSIGNMENT;
                Child1->Common.OperatorSymbol = NULL;
                return (TRUE);
            }
            break;

            /* Non-commutative operators */

        case AML_SUBTRACT_OP:
        case AML_DIVIDE_OP:
        case AML_MOD_OP:
        case AML_SHIFT_LEFT_OP:
        case AML_SHIFT_RIGHT_OP:
            /*
             * For the non-commutative operators, we can convert to a
             * compound statement only if the target is the same as the
             * first operand.
             *
             *      Subtract (A, B, A) --> A -= B
             *      Subtract (B, A, A) --> A = (B - A)
             */
            if ((AcpiDmIsTargetAnOperand (Target, Child1, TRUE)))
            {
                Target->Common.OperatorSymbol =
                    AcpiDmGetCompoundSymbol (Op->Common.AmlOpcode);

                /* Convert operator to compound assignment */

                Op->Common.DisasmFlags |= ACPI_PARSEOP_COMPOUND_ASSIGNMENT;
                Child1->Common.OperatorSymbol = NULL;
                return (TRUE);
            }
            break;

        default:
            break;
        }

        /*
         * If we are within a C-style expression, emit an extra open
         * paren. Implemented by examining the parent op.
         */
        switch (Op->Common.Parent->Common.AmlOpcode)
        {
        case AML_ADD_OP:
        case AML_SUBTRACT_OP:
        case AML_MULTIPLY_OP:
        case AML_DIVIDE_OP:
        case AML_MOD_OP:
        case AML_SHIFT_LEFT_OP:
        case AML_SHIFT_RIGHT_OP:
        case AML_BIT_AND_OP:
        case AML_BIT_OR_OP:
        case AML_BIT_XOR_OP:
        case AML_LAND_OP:
        case AML_LEQUAL_OP:
        case AML_LGREATER_OP:
        case AML_LLESS_OP:
        case AML_LOR_OP:

            Op->Common.DisasmFlags |= ACPI_PARSEOP_ASSIGNMENT;
            AcpiOsPrintf ("(");
            break;

        default:
            break;
        }

        /* Normal output for ASL/AML operators with a target operand */

        Target->Common.OperatorSymbol = " = (";
        return (TRUE);

    /* Binary operators, no parens */

    case AML_DECREMENT_OP:
    case AML_INCREMENT_OP:
        return (TRUE);

    case AML_INDEX_OP:

        /* Target is optional, 3rd operand */

        Target = Child2->Common.Next;
        if (AcpiDmIsValidTarget (Target))
        {
            AcpiDmPromoteTarget (Op, Target);

            if (!Target->Common.OperatorSymbol)
            {
                Target->Common.OperatorSymbol = " = ";
            }
        }
        return (TRUE);

    case AML_STORE_OP:
        /*
         * Target is the 2nd operand.
         * We know the target is valid, it is not optional.
         * In the parse tree, simply swap the target with the
         * source so that the target is processed first.
         */
        Target = Child1->Common.Next;
        if (!Target)
        {
            return (FALSE);
        }

        AcpiDmPromoteTarget (Op, Target);
        if (!Target->Common.OperatorSymbol)
        {
            Target->Common.OperatorSymbol = " = ";
        }
        return (TRUE);

    case AML_BIT_NOT_OP:

        /* Target is optional, 2nd operand */

        Target = Child1->Common.Next;
        if (!Target)
        {
            return (FALSE);
        }

        if (AcpiDmIsValidTarget (Target))
        {
            /* Valid target, not a placeholder */

            AcpiDmPromoteTarget (Op, Target);
            Target->Common.OperatorSymbol = " = ~";
        }
        else
        {
            /* No target. Emit this prefix operator immediately */

            AcpiOsPrintf ("~");
        }
        return (TRUE);

    default:
        break;
    }

    /*
     * Nodes marked with ACPI_PARSEOP_PARAMLIST don't need a parens
     * output here. We also need to check the parent to see if this op
     * is part of a compound test (!=, >=, <=).
     */
    if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) ||
       ((Op->Common.Parent->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) &&
        (Op->Common.DisasmOpcode == ACPI_DASM_LNOT_SUFFIX)))
    {
        /* Do Nothing. Paren already generated */
        return (TRUE);
    }

    /* All other operators, emit an open paren */

    AcpiOsPrintf ("(");
    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCloseOperator
 *
 * PARAMETERS:  Op                  - Current parse object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Closes an operator by adding a closing parentheses if and
 *              when necessary. Called during ascending phase of the
 *              parse tree walk.
 *
 ******************************************************************************/

void
AcpiDmCloseOperator (
    ACPI_PARSE_OBJECT       *Op)
{
    BOOLEAN                 IsCStyleOp = FALSE;

    /* Always emit paren if ASL+ disassembly disabled */

    if (!AcpiGbl_CstyleDisassembly)
    {
        AcpiOsPrintf (")");
        return;
    }

    /* Check if we need to add an additional closing paren */

    switch (Op->Common.AmlOpcode)
    {
    case AML_ADD_OP:
    case AML_SUBTRACT_OP:
    case AML_MULTIPLY_OP:
    case AML_DIVIDE_OP:
    case AML_MOD_OP:
    case AML_SHIFT_LEFT_OP:
    case AML_SHIFT_RIGHT_OP:
    case AML_BIT_AND_OP:
    case AML_BIT_OR_OP:
    case AML_BIT_XOR_OP:
    case AML_LAND_OP:
    case AML_LEQUAL_OP:
    case AML_LGREATER_OP:
    case AML_LLESS_OP:
    case AML_LOR_OP:

        /* Emit paren only if this is not a compound assignment */

        if (Op->Common.DisasmFlags & ACPI_PARSEOP_COMPOUND_ASSIGNMENT)
        {
            return;
        }

        /* Emit extra close paren for assignment within an expression */

        if (Op->Common.DisasmFlags & ACPI_PARSEOP_ASSIGNMENT)
        {
            AcpiOsPrintf (")");
        }

        IsCStyleOp = TRUE;
        break;

    case AML_INDEX_OP:

        /* This is case for unsupported Index() source constants */

        if (Op->Common.DisasmFlags & ACPI_PARSEOP_CLOSING_PAREN)
        {
            AcpiOsPrintf (")");
        }
        return;

    /* No need for parens for these */

    case AML_DECREMENT_OP:
    case AML_INCREMENT_OP:
    case AML_LNOT_OP:
    case AML_BIT_NOT_OP:
    case AML_STORE_OP:
        return;

    default:

        /* Always emit paren for non-ASL+ operators */
        break;
    }

    /*
     * Nodes marked with ACPI_PARSEOP_PARAMLIST don't need a parens
     * output here. We also need to check the parent to see if this op
     * is part of a compound test (!=, >=, <=).
     */
    if (IsCStyleOp &&
       ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) ||
       ((Op->Common.Parent->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) &&
        (Op->Common.DisasmOpcode == ACPI_DASM_LNOT_SUFFIX))))
    {
        return;
    }

    AcpiOsPrintf (")");
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmGetCompoundSymbol
 *
 * PARAMETERS:  AslOpcode
 *
 * RETURN:      String containing the compound assignment symbol
 *
 * DESCRIPTION: Detect opcodes that can be converted to compound assignment,
 *              return the appropriate operator string.
 *
 ******************************************************************************/

static char *
AcpiDmGetCompoundSymbol (
   UINT16                   AmlOpcode)
{
    char                    *Symbol;


    switch (AmlOpcode)
    {
    case AML_ADD_OP:
        Symbol = " += ";
        break;

    case AML_SUBTRACT_OP:
        Symbol = " -= ";
        break;

    case AML_MULTIPLY_OP:
        Symbol = " *= ";
        break;

    case AML_DIVIDE_OP:
        Symbol = " /= ";
        break;

    case AML_MOD_OP:
        Symbol = " %= ";
        break;

    case AML_SHIFT_LEFT_OP:
        Symbol = " <<= ";
        break;

    case AML_SHIFT_RIGHT_OP:
        Symbol = " >>= ";
        break;

    case AML_BIT_AND_OP:
        Symbol = " &= ";
        break;

    case AML_BIT_OR_OP:
        Symbol = " |= ";
        break;

    case AML_BIT_XOR_OP:
        Symbol = " ^= ";
        break;

    default:

        /* No operator string for all other opcodes */

        return (NULL);
    }

    return (Symbol);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmPromoteTarget
 *
 * PARAMETERS:  Op                  - Operator parse object
 *              Target              - Target associate with the Op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Transform the parse tree by moving the target up to the first
 *              child of the Op.
 *
 ******************************************************************************/

static void
AcpiDmPromoteTarget (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *Target)
{
    ACPI_PARSE_OBJECT       *Child;


    /* Link target directly to the Op as first child */

    Child = Op->Common.Value.Arg;
    Op->Common.Value.Arg = Target;
    Target->Common.Next = Child;

    /* Find the last peer, it is linked to the target. Unlink it. */

    while (Child->Common.Next != Target)
    {
        Child = Child->Common.Next;
    }

    Child->Common.Next = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsValidTarget
 *
 * PARAMETERS:  Target              - Target Op from the parse tree
 *
 * RETURN:      TRUE if the Target is real. FALSE if it is just a placeholder
 *              Op that was inserted by the parser.
 *
 * DESCRIPTION: Determine if a Target Op is a placeholder Op or a real Target.
 *              In other words, determine if the optional target is used or
 *              not. Note: If Target is NULL, something is seriously wrong,
 *              probably with the parse tree.
 *
 ******************************************************************************/

static BOOLEAN
AcpiDmIsValidTarget (
    ACPI_PARSE_OBJECT       *Target)
{

    if (!Target)
    {
        return (FALSE);
    }

    if ((Target->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
        (Target->Common.Value.Arg == NULL))
    {
        return (FALSE);
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsTargetAnOperand
 *
 * PARAMETERS:  Target              - Target associated with the expression
 *              Operand             - An operand associated with expression
 *
 * RETURN:      TRUE if expression can be converted to a compound assignment.
 *              FALSE otherwise.
 *
 * DESCRIPTION: Determine if the Target duplicates the operand, in order to
 *              detect if the expression can be converted to a compound
 *              assigment. (+=, *=, etc.)
 *
 ******************************************************************************/

static BOOLEAN
AcpiDmIsTargetAnOperand (
    ACPI_PARSE_OBJECT       *Target,
    ACPI_PARSE_OBJECT       *Operand,
    BOOLEAN                 TopLevel)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    BOOLEAN                 Same;


    /*
     * Opcodes must match. Note: ignoring the difference between nameseg
     * and namepath for now. May be needed later.
     */
    if (Target->Common.AmlOpcode != Operand->Common.AmlOpcode)
    {
        return (FALSE);
    }

    /* Nodes should match, even if they are NULL */

    if (Target->Common.Node != Operand->Common.Node)
    {
        return (FALSE);
    }

    /* Determine if a child exists */

    OpInfo = AcpiPsGetOpcodeInfo (Operand->Common.AmlOpcode);
    if (OpInfo->Flags & AML_HAS_ARGS)
    {
        Same = AcpiDmIsTargetAnOperand (Target->Common.Value.Arg,
            Operand->Common.Value.Arg, FALSE);
        if (!Same)
        {
            return (FALSE);
        }
    }

    /* Check the next peer, as long as we are not at the top level */

    if ((!TopLevel) &&
         Target->Common.Next)
    {
        Same = AcpiDmIsTargetAnOperand (Target->Common.Next,
            Operand->Common.Next, FALSE);
        if (!Same)
        {
            return (FALSE);
        }
    }

    /* Supress the duplicate operand at the top-level */

    if (TopLevel)
    {
        Operand->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
    }
    return (TRUE);
}
