
/******************************************************************************
 *
 * Module Name: exoparg1 - AML execution - opcodes with 1 argument
 *              $Revision: 146 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#define __EXOPARG1_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exoparg1")


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
 * FUNCTION:    AcpiExOpcode_1A_0T_0R
 *
 * PARAMETERS:  WalkState           - Current state (contains AML opcode)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 1 monadic operator with numeric operand on
 *              object stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_1A_0T_0R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_STR ("ExOpcode_1A_0T_0R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /* Examine the AML opcode */

    switch (WalkState->Opcode)
    {
    case AML_RELEASE_OP:    /*  Release (MutexObject) */

        Status = AcpiExReleaseMutex (Operand[0], WalkState);
        break;


    case AML_RESET_OP:      /*  Reset (EventObject) */

        Status = AcpiExSystemResetEvent (Operand[0]);
        break;


    case AML_SIGNAL_OP:     /*  Signal (EventObject) */

        Status = AcpiExSystemSignalEvent (Operand[0]);
        break;


    case AML_SLEEP_OP:      /*  Sleep (MsecTime) */

        Status = AcpiExSystemDoSuspend ((UINT32) Operand[0]->Integer.Value);
        break;


    case AML_STALL_OP:      /*  Stall (UsecTime) */

        Status = AcpiExSystemDoStall ((UINT32) Operand[0]->Integer.Value);
        break;


    case AML_UNLOAD_OP:     /*  Unload (Handle) */

        Status = AcpiExUnloadTable (Operand[0]);
        break;


    default:                /*  Unknown opcode  */

        ACPI_REPORT_ERROR (("AcpiExOpcode_1A_0T_0R: Unknown opcode %X\n",
            WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_1A_1T_0R
 *
 * PARAMETERS:  WalkState           - Current state (contains AML opcode)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with one argument, one target, and no
 *              return value.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_1A_1T_0R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];


    ACPI_FUNCTION_TRACE_STR ("ExOpcode_1A_1T_0R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /* Examine the AML opcode */

    switch (WalkState->Opcode)
    {
    case AML_LOAD_OP:

        Status = AcpiExLoadOp (Operand[0], Operand[1], WalkState);
        break;

    default:                        /* Unknown opcode */

        ACPI_REPORT_ERROR (("AcpiExOpcode_1A_1T_0R: Unknown opcode %X\n",
            WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


Cleanup:

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_1A_1T_1R
 *
 * PARAMETERS:  WalkState           - Current state (contains AML opcode)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with one argument, one target, and a
 *              return value.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_1A_1T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ReturnDesc = NULL;
    ACPI_OPERAND_OBJECT     *ReturnDesc2 = NULL;
    UINT32                  Temp32;
    UINT32                  i;
    UINT32                  j;
    ACPI_INTEGER            Digit;


    ACPI_FUNCTION_TRACE_STR ("ExOpcode_1A_1T_1R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /* Examine the AML opcode */

    switch (WalkState->Opcode)
    {
    case AML_BIT_NOT_OP:
    case AML_FIND_SET_LEFT_BIT_OP:
    case AML_FIND_SET_RIGHT_BIT_OP:
    case AML_FROM_BCD_OP:
    case AML_TO_BCD_OP:
    case AML_COND_REF_OF_OP:

        /* Create a return object of type Integer for these opcodes */

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        switch (WalkState->Opcode)
        {
        case AML_BIT_NOT_OP:            /* Not (Operand, Result)  */

            ReturnDesc->Integer.Value = ~Operand[0]->Integer.Value;
            break;


        case AML_FIND_SET_LEFT_BIT_OP:  /* FindSetLeftBit (Operand, Result) */

            ReturnDesc->Integer.Value = Operand[0]->Integer.Value;

            /*
             * Acpi specification describes Integer type as a little
             * endian unsigned value, so this boundary condition is valid.
             */
            for (Temp32 = 0; ReturnDesc->Integer.Value && Temp32 < ACPI_INTEGER_BIT_SIZE; ++Temp32)
            {
                ReturnDesc->Integer.Value >>= 1;
            }

            ReturnDesc->Integer.Value = Temp32;
            break;


        case AML_FIND_SET_RIGHT_BIT_OP: /* FindSetRightBit (Operand, Result)  */

            ReturnDesc->Integer.Value = Operand[0]->Integer.Value;

            /*
             * The Acpi specification describes Integer type as a little
             * endian unsigned value, so this boundary condition is valid.
             */
            for (Temp32 = 0; ReturnDesc->Integer.Value && Temp32 < ACPI_INTEGER_BIT_SIZE; ++Temp32)
            {
                ReturnDesc->Integer.Value <<= 1;
            }

            /* Since the bit position is one-based, subtract from 33 (65) */

            ReturnDesc->Integer.Value = Temp32 == 0 ? 0 : (ACPI_INTEGER_BIT_SIZE + 1) - Temp32;
            break;


        case AML_FROM_BCD_OP:           /* FromBcd (BCDValue, Result)  */

            /*
             * The 64-bit ACPI integer can hold 16 4-bit BCD integers
             */
            ReturnDesc->Integer.Value = 0;
            for (i = 0; i < ACPI_MAX_BCD_DIGITS; i++)
            {
                /* Get one BCD digit */

                Digit = (ACPI_INTEGER) ((Operand[0]->Integer.Value >> (i * 4)) & 0xF);

                /* Check the range of the digit */

                if (Digit > 9)
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "BCD digit too large: %d\n",
                        (UINT32) Digit));
                    Status = AE_AML_NUMERIC_OVERFLOW;
                    goto Cleanup;
                }

                if (Digit > 0)
                {
                    /* Sum into the result with the appropriate power of 10 */

                    for (j = 0; j < i; j++)
                    {
                        Digit *= 10;
                    }

                    ReturnDesc->Integer.Value += Digit;
                }
            }
            break;


        case AML_TO_BCD_OP:             /* ToBcd (Operand, Result)  */

            if (Operand[0]->Integer.Value > ACPI_MAX_BCD_VALUE)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "BCD overflow: %8.8X%8.8X\n",
                    ACPI_HIDWORD(Operand[0]->Integer.Value),
                    ACPI_LODWORD(Operand[0]->Integer.Value)));
                Status = AE_AML_NUMERIC_OVERFLOW;
                goto Cleanup;
            }

            ReturnDesc->Integer.Value = 0;
            for (i = 0; i < ACPI_MAX_BCD_DIGITS; i++)
            {
                /* Divide by nth factor of 10 */

                Temp32 = 0;
                Digit = Operand[0]->Integer.Value;
                for (j = 0; j < i; j++)
                {
                    (void) AcpiUtShortDivide (&Digit, 10, &Digit, &Temp32);
                }

                /* Create the BCD digit from the remainder above */

                if (Digit > 0)
                {
                    ReturnDesc->Integer.Value += ((ACPI_INTEGER) Temp32 << (i * 4));
                }
            }
            break;


        case AML_COND_REF_OF_OP:        /* CondRefOf (SourceObject, Result)  */

            /*
             * This op is a little strange because the internal return value is
             * different than the return value stored in the result descriptor
             * (There are really two return values)
             */
            if ((ACPI_NAMESPACE_NODE *) Operand[0] == AcpiGbl_RootNode)
            {
                /*
                 * This means that the object does not exist in the namespace,
                 * return FALSE
                 */
                ReturnDesc->Integer.Value = 0;
                goto Cleanup;
            }

            /* Get the object reference, store it, and remove our reference */

            Status = AcpiExGetObjectReference (Operand[0], &ReturnDesc2, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            Status = AcpiExStore (ReturnDesc2, Operand[1], WalkState);
            AcpiUtRemoveReference (ReturnDesc2);

            /* The object exists in the namespace, return TRUE */

            ReturnDesc->Integer.Value = ACPI_INTEGER_MAX;
            goto Cleanup;


        default:
            /* No other opcodes get here */
            break;
        }
        break;


    case AML_STORE_OP:              /* Store (Source, Target) */

        /*
         * A store operand is typically a number, string, buffer or lvalue
         * Be careful about deleting the source object,
         * since the object itself may have been stored.
         */
        Status = AcpiExStore (Operand[0], Operand[1], WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* It is possible that the Store already produced a return object */

        if (!WalkState->ResultObj)
        {
            /*
             * Normally, we would remove a reference on the Operand[0] parameter;
             * But since it is being used as the internal return object
             * (meaning we would normally increment it), the two cancel out,
             * and we simply don't do anything.
             */
            WalkState->ResultObj = Operand[0];
            WalkState->Operands[0] = NULL;  /* Prevent deletion */
        }
        return_ACPI_STATUS (Status);


    /*
     * ACPI 2.0 Opcodes
     */
    case AML_COPY_OP:               /* Copy (Source, Target) */

        Status = AcpiUtCopyIobjectToIobject (Operand[0], &ReturnDesc, WalkState);
        break;


    case AML_TO_DECSTRING_OP:       /* ToDecimalString (Data, Result) */

        Status = AcpiExConvertToString (Operand[0], &ReturnDesc, 10, ACPI_UINT32_MAX, WalkState);
        break;


    case AML_TO_HEXSTRING_OP:       /* ToHexString (Data, Result) */

        Status = AcpiExConvertToString (Operand[0], &ReturnDesc, 16, ACPI_UINT32_MAX, WalkState);
        break;


    case AML_TO_BUFFER_OP:          /* ToBuffer (Data, Result) */

        Status = AcpiExConvertToBuffer (Operand[0], &ReturnDesc, WalkState);
        break;


    case AML_TO_INTEGER_OP:         /* ToInteger (Data, Result) */

        Status = AcpiExConvertToInteger (Operand[0], &ReturnDesc, WalkState);
        break;


    case AML_SHIFT_LEFT_BIT_OP:     /*  ShiftLeftBit (Source, BitNum)  */
    case AML_SHIFT_RIGHT_BIT_OP:    /*  ShiftRightBit (Source, BitNum) */

        /*
         * These are two obsolete opcodes
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%s is obsolete and not implemented\n",
                        AcpiPsGetOpcodeName (WalkState->Opcode)));
        Status = AE_SUPPORT;
        goto Cleanup;


    default:                        /* Unknown opcode */

        ACPI_REPORT_ERROR (("AcpiExOpcode_1A_1T_1R: Unknown opcode %X\n",
            WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }

    /*
     * Store the return value computed above into the target object
     */
    Status = AcpiExStore (ReturnDesc, Operand[1], WalkState);


Cleanup:

    if (!WalkState->ResultObj)
    {
        WalkState->ResultObj = ReturnDesc;
    }

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ReturnDesc);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_1A_0T_1R
 *
 * PARAMETERS:  WalkState           - Current state (contains AML opcode)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with one argument, no target, and a return value
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_1A_0T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *TempDesc;
    ACPI_OPERAND_OBJECT     *ReturnDesc = NULL;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Type;
    ACPI_INTEGER            Value;


    ACPI_FUNCTION_TRACE_STR ("ExOpcode_1A_0T_0R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /* Examine the AML opcode */

    switch (WalkState->Opcode)
    {
    case AML_LNOT_OP:               /* LNot (Operand) */

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        ReturnDesc->Integer.Value = !Operand[0]->Integer.Value;
        break;


    case AML_DECREMENT_OP:          /* Decrement (Operand)  */
    case AML_INCREMENT_OP:          /* Increment (Operand)  */

        /*
         * Since we are expecting a Reference operand, it
         * can be either a NS Node or an internal object.
         */
        ReturnDesc = Operand[0];
        if (ACPI_GET_DESCRIPTOR_TYPE (Operand[0]) == ACPI_DESC_TYPE_OPERAND)
        {
            /* Internal reference object - prevent deletion */

            AcpiUtAddReference (ReturnDesc);
        }

        /*
         * Convert the ReturnDesc Reference to a Number
         * (This removes a reference on the ReturnDesc object)
         */
        Status = AcpiExResolveOperands (AML_LNOT_OP, &ReturnDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%s: bad operand(s) %s\n",
                AcpiPsGetOpcodeName (WalkState->Opcode), AcpiFormatException(Status)));

            goto Cleanup;
        }

        /*
         * ReturnDesc is now guaranteed to be an Integer object
         * Do the actual increment or decrement
         */
        if (AML_INCREMENT_OP == WalkState->Opcode)
        {
            ReturnDesc->Integer.Value++;
        }
        else
        {
            ReturnDesc->Integer.Value--;
        }

        /* Store the result back in the original descriptor */

        Status = AcpiExStore (ReturnDesc, Operand[0], WalkState);
        break;


    case AML_TYPE_OP:               /* ObjectType (SourceObject) */

        /* Get the type of the base object */

        Status = AcpiExResolveMultiple (WalkState, Operand[0], &Type, NULL);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        /* Allocate a descriptor to hold the type. */

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        ReturnDesc->Integer.Value = Type;
        break;


    case AML_SIZE_OF_OP:            /* SizeOf (SourceObject)  */

        /* Get the base object */

        Status = AcpiExResolveMultiple (WalkState, Operand[0], &Type, &TempDesc);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        /*
         * Type is guaranteed to be a buffer, string, or package at this
         * point (even if the original operand was an object reference, it
         * will be resolved and typechecked during operand resolution.)
         */
        switch (Type)
        {
        case ACPI_TYPE_BUFFER:
            Value = TempDesc->Buffer.Length;
            break;

        case ACPI_TYPE_STRING:
            Value = TempDesc->String.Length;
            break;

        case ACPI_TYPE_PACKAGE:
            Value = TempDesc->Package.Count;
            break;

        default:
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "SizeOf, Not Buf/Str/Pkg - found type %s\n",
                AcpiUtGetTypeName (Type)));
            Status = AE_AML_OPERAND_TYPE;
            goto Cleanup;
        }

        /*
         * Now that we have the size of the object, create a result
         * object to hold the value
         */
        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        ReturnDesc->Integer.Value = Value;
        break;


    case AML_REF_OF_OP:             /* RefOf (SourceObject) */

        Status = AcpiExGetObjectReference (Operand[0], &ReturnDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
        break;


    case AML_DEREF_OF_OP:           /* DerefOf (ObjReference | String) */

        /* Check for a method local or argument, or standalone String */

        if (ACPI_GET_DESCRIPTOR_TYPE (Operand[0]) != ACPI_DESC_TYPE_NAMED)
        {
            switch (ACPI_GET_OBJECT_TYPE (Operand[0]))
            {
            case ACPI_TYPE_LOCAL_REFERENCE:
                /*
                 * This is a DerefOf (LocalX | ArgX)
                 *
                 * Must resolve/dereference the local/arg reference first
                 */
                switch (Operand[0]->Reference.Opcode)
                {
                case AML_LOCAL_OP:
                case AML_ARG_OP:

                    /* Set Operand[0] to the value of the local/arg */

                    Status = AcpiDsMethodDataGetValue (Operand[0]->Reference.Opcode,
                                Operand[0]->Reference.Offset, WalkState, &TempDesc);
                    if (ACPI_FAILURE (Status))
                    {
                        goto Cleanup;
                    }

                    /*
                     * Delete our reference to the input object and
                     * point to the object just retrieved
                     */
                    AcpiUtRemoveReference (Operand[0]);
                    Operand[0] = TempDesc;
                    break;

                case AML_REF_OF_OP:

                    /* Get the object to which the reference refers */

                    TempDesc = Operand[0]->Reference.Object;
                    AcpiUtRemoveReference (Operand[0]);
                    Operand[0] = TempDesc;
                    break;

                default:

                    /* Must be an Index op - handled below */
                    break;
                }
                break;


            case ACPI_TYPE_STRING:

                /*
                 * This is a DerefOf (String).  The string is a reference to a named ACPI object.
                 *
                 * 1) Find the owning Node
                 * 2) Dereference the node to an actual object.  Could be a Field, so we nee
                 *    to resolve the node to a value.
                 */
                Status = AcpiNsGetNodeByPath (Operand[0]->String.Pointer,
                                WalkState->ScopeInfo->Scope.Node, ACPI_NS_SEARCH_PARENT,
                                ACPI_CAST_INDIRECT_PTR (ACPI_NAMESPACE_NODE, &ReturnDesc));
                if (ACPI_FAILURE (Status))
                {
                    goto Cleanup;
                }

                Status = AcpiExResolveNodeToValue (
                                ACPI_CAST_INDIRECT_PTR (ACPI_NAMESPACE_NODE, &ReturnDesc), WalkState);
                goto Cleanup;


            default:

                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
        }

        /* Operand[0] may have changed from the code above */

        if (ACPI_GET_DESCRIPTOR_TYPE (Operand[0]) == ACPI_DESC_TYPE_NAMED)
        {
            /*
             * This is a DerefOf (ObjectReference)
             * Get the actual object from the Node (This is the dereference).
             * -- This case may only happen when a LocalX or ArgX is dereferenced above.
             */
            ReturnDesc = AcpiNsGetAttachedObject ((ACPI_NAMESPACE_NODE *) Operand[0]);
        }
        else
        {
            /*
             * This must be a reference object produced by either the Index() or
             * RefOf() operator
             */
            switch (Operand[0]->Reference.Opcode)
            {
            case AML_INDEX_OP:

                /*
                 * The target type for the Index operator must be
                 * either a Buffer or a Package
                 */
                switch (Operand[0]->Reference.TargetType)
                {
                case ACPI_TYPE_BUFFER_FIELD:

                    TempDesc = Operand[0]->Reference.Object;

                    /*
                     * Create a new object that contains one element of the
                     * buffer -- the element pointed to by the index.
                     *
                     * NOTE: index into a buffer is NOT a pointer to a
                     * sub-buffer of the main buffer, it is only a pointer to a
                     * single element (byte) of the buffer!
                     */
                    ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
                    if (!ReturnDesc)
                    {
                        Status = AE_NO_MEMORY;
                        goto Cleanup;
                    }

                    /*
                     * Since we are returning the value of the buffer at the
                     * indexed location, we don't need to add an additional
                     * reference to the buffer itself.
                     */
                    ReturnDesc->Integer.Value =
                        TempDesc->Buffer.Pointer[Operand[0]->Reference.Offset];
                    break;


                case ACPI_TYPE_PACKAGE:

                    /*
                     * Return the referenced element of the package.  We must add
                     * another reference to the referenced object, however.
                     */
                    ReturnDesc = *(Operand[0]->Reference.Where);
                    if (!ReturnDesc)
                    {
                        /*
                         * We can't return a NULL dereferenced value.  This is
                         * an uninitialized package element and is thus a
                         * severe error.
                         */
                        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "NULL package element obj %p\n",
                            Operand[0]));
                        Status = AE_AML_UNINITIALIZED_ELEMENT;
                        goto Cleanup;
                    }

                    AcpiUtAddReference (ReturnDesc);
                    break;


                default:

                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown Index TargetType %X in obj %p\n",
                        Operand[0]->Reference.TargetType, Operand[0]));
                    Status = AE_AML_OPERAND_TYPE;
                    goto Cleanup;
                }
                break;


            case AML_REF_OF_OP:

                ReturnDesc = Operand[0]->Reference.Object;

                if (ACPI_GET_DESCRIPTOR_TYPE (ReturnDesc) == ACPI_DESC_TYPE_NAMED)
                {

                    ReturnDesc = AcpiNsGetAttachedObject ((ACPI_NAMESPACE_NODE *) ReturnDesc);
                }

                /* Add another reference to the object! */

                AcpiUtAddReference (ReturnDesc);
                break;


            default:
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown opcode in ref(%p) - %X\n",
                    Operand[0], Operand[0]->Reference.Opcode));

                Status = AE_TYPE;
                goto Cleanup;
            }
        }
        break;


    default:

        ACPI_REPORT_ERROR (("AcpiExOpcode_1A_0T_1R: Unknown opcode %X\n",
            WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


Cleanup:

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ReturnDesc);
    }

    WalkState->ResultObj = ReturnDesc;
    return_ACPI_STATUS (Status);
}

