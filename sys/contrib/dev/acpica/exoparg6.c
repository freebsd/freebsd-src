
/******************************************************************************
 *
 * Module Name: exoparg6 - AML execution - opcodes with 6 arguments
 *              $Revision: 4 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY

 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#define __EXOPARG6_C__

#include "acpi.h"
#include "acinterp.h"
#include "acparser.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exoparg6")



/*!
 * Naming convention for AML interpreter execution routines.
 *
 * The routines that begin execution of AML opcodes are named with a common
 * convention based upon the number of arguments, the number of target operands,
 * and whether or not a value is returned:
 *
 *      AcpiExOpcode_xA_yT_zR
 *
 * Where:  
 *
 * xA - ARGUMENTS:    The number of arguments (input operands) that are 
 *                    required for this opcode type (1 through 6 args).
 * yT - TARGETS:      The number of targets (output operands) that are required 
 *                    for this opcode type (0, 1, or 2 targets).
 * zR - RETURN VALUE: Indicates whether this opcode type returns a value 
 *                    as the function return (0 or 1).
 *
 * The AcpiExOpcode* functions are called via the Dispatcher component with 
 * fully resolved operands.
!*/



/*******************************************************************************
 *
 * FUNCTION:    AcpiExDoMatch
 *
 * PARAMETERS:  MatchOp         - The AML match operand
 *              PackageValue    - Value from the target package
 *              MatchValue      - Value to be matched
 *
 * RETURN:      TRUE if the match is successful, FALSE otherwise
 *
 * DESCRIPTION: Implements the low-level match for the ASL Match operator
 *
 ******************************************************************************/

BOOLEAN
AcpiExDoMatch (
    UINT32                  MatchOp,
    ACPI_INTEGER            PackageValue,
    ACPI_INTEGER            MatchValue)
{

    switch (MatchOp)
    {
    case MATCH_MTR:   /* always true */

        break;


    case MATCH_MEQ:   /* true if equal   */

        if (PackageValue != MatchValue)
        {
            return (FALSE);
        }
        break;


    case MATCH_MLE:   /* true if less than or equal  */

        if (PackageValue > MatchValue)
        {
            return (FALSE);
        }
        break;


    case MATCH_MLT:   /* true if less than   */

        if (PackageValue >= MatchValue)
        {
            return (FALSE);
        }
        break;


    case MATCH_MGE:   /* true if greater than or equal   */

        if (PackageValue < MatchValue)
        {
            return (FALSE);
        }
        break;


    case MATCH_MGT:   /* true if greater than    */

        if (PackageValue <= MatchValue)
        {
            return (FALSE);
        }
        break;


    default:    /* undefined   */

        return (FALSE);
    }


    return TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_6A_0T_1R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with 6 arguments, no target, and a return value
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_6A_0T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ReturnDesc = NULL;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Index;
    ACPI_OPERAND_OBJECT     *ThisElement;


    FUNCTION_TRACE_STR ("ExOpcode_6A_0T_1R", AcpiPsGetOpcodeName (WalkState->Opcode));


    switch (WalkState->Opcode)
    {
    case AML_MATCH_OP:  
        /* 
         * Match (SearchPackage[0], MatchOp1[1], MatchObject1[2], 
         *                          MatchOp2[3], MatchObject2[4], StartIndex[5])
         */

        /* Validate match comparison sub-opcodes */

        if ((Operand[1]->Integer.Value > MAX_MATCH_OPERATOR) ||
            (Operand[3]->Integer.Value > MAX_MATCH_OPERATOR))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "operation encoding out of range\n"));
            Status = AE_AML_OPERAND_VALUE;
            goto Cleanup;
        }

        Index = (UINT32) Operand[5]->Integer.Value;
        if (Index >= (UINT32) Operand[0]->Package.Count)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Index beyond package end\n"));
            Status = AE_AML_PACKAGE_LIMIT;
            goto Cleanup;
        }

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;

        }

        /* Default return value if no match found */

        ReturnDesc->Integer.Value = ACPI_INTEGER_MAX;

        /*
         * Examine each element until a match is found.  Within the loop,
         * "continue" signifies that the current element does not match
         * and the next should be examined.
         * Upon finding a match, the loop will terminate via "break" at
         * the bottom.  If it terminates "normally", MatchValue will be -1
         * (its initial value) indicating that no match was found.  When
         * returned as a Number, this will produce the Ones value as specified.
         */
        for ( ; Index < Operand[0]->Package.Count; Index++)
        {
            ThisElement = Operand[0]->Package.Elements[Index];

            /*
             * Treat any NULL or non-numeric elements as non-matching.
             * TBD [Unhandled] - if an element is a Name,
             *      should we examine its value?
             */
            if (!ThisElement ||
                ThisElement->Common.Type != ACPI_TYPE_INTEGER)
            {
                continue;
            }


            /*
             * Within these switch statements:
             *      "break" (exit from the switch) signifies a match;
             *      "continue" (proceed to next iteration of enclosing
             *          "for" loop) signifies a non-match.
             */
            if (!AcpiExDoMatch ((UINT32) Operand[1]->Integer.Value, 
                                ThisElement->Integer.Value, Operand[2]->Integer.Value))
            {
                continue;
            }


            if (!AcpiExDoMatch ((UINT32) Operand[3]->Integer.Value, 
                                ThisElement->Integer.Value, Operand[4]->Integer.Value))
            {
                continue;
            }

            /* Match found: Index is the return value */

            ReturnDesc->Integer.Value = Index;
            break;
        }

        break;


    case AML_LOAD_TABLE_OP:

        Status = AE_NOT_IMPLEMENTED;
        goto Cleanup;
        break;


    default:

        REPORT_ERROR (("AcpiExOpcode_3A_0T_0R: Unknown opcode %X\n",
                WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
        break;
    }


    WalkState->ResultObj = ReturnDesc;


Cleanup:

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ReturnDesc);
    }

    return_ACPI_STATUS (Status);
}
