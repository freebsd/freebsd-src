
/******************************************************************************
 *
 * Module Name: ammonad - ACPI AML (p-code) execution for monadic operators
 *              $Revision: 81 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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

#define __AMMONAD_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("ammonad")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlGetObjectReference
 *
 * PARAMETERS:  ObjDesc         - Create a reference to this object
 *              RetDesc         - Where to store the reference
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtain and return a "reference" to the target object
 *              Common code for the RefOfOp and the CondRefOfOp.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlGetObjectReference (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **RetDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("AmlGetObjectReference", ObjDesc);


    if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_INTERNAL))
    {
        if (ObjDesc->Common.Type != INTERNAL_TYPE_REFERENCE)
        {
            *RetDesc = NULL;
            Status = AE_TYPE;
            goto Cleanup;
        }

        /*
         * Not a Name -- an indirect name pointer would have
         * been converted to a direct name pointer in AcpiAmlResolveOperands
         */
        switch (ObjDesc->Reference.OpCode)
        {
        case AML_LOCAL_OP:

            *RetDesc = (void *) AcpiDsMethodDataGetNte (MTH_TYPE_LOCAL,
                                        (ObjDesc->Reference.Offset), WalkState);
            break;


        case AML_ARG_OP:

            *RetDesc = (void *) AcpiDsMethodDataGetNte (MTH_TYPE_ARG,
                                        (ObjDesc->Reference.Offset), WalkState);
            break;


        default:

            DEBUG_PRINT (ACPI_ERROR,
                ("AmlGetObjectReference: (Internal) Unknown Ref subtype %02x\n",
                ObjDesc->Reference.OpCode));
            *RetDesc = NULL;
            Status = AE_AML_INTERNAL;
            goto Cleanup;
        }

    }

    else if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))
    {
        /* Must be a named object;  Just return the Node */

        *RetDesc = ObjDesc;
    }

    else
    {
        *RetDesc = NULL;
        Status = AE_TYPE;
    }


Cleanup:

    DEBUG_PRINT (TRACE_EXEC,
        ("AmlGetObjectReference: Obj=%p Ref=%p\n", ObjDesc, *RetDesc));
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlExecMonadic1
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 1 monadic operator with numeric operand on
 *              object stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlExecMonadic1 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("AmlExecMonadic1", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE,
                    AcpiPsGetOpcodeName (Opcode),
                    1, "after AcpiAmlResolveOperands");

    /* Get all operands */

    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecMonadic1/%s: bad operand(s) (0x%X)\n",
            AcpiPsGetOpcodeName (Opcode), Status));

        goto Cleanup;
    }


    /* Examine the opcode */

    switch (Opcode)
    {

    /*  DefRelease  :=  ReleaseOp   MutexObject */

    case AML_RELEASE_OP:

        Status = AcpiAmlSystemReleaseMutex (ObjDesc);
        break;


    /*  DefReset    :=  ResetOp     AcpiEventObject */

    case AML_RESET_OP:

        Status = AcpiAmlSystemResetEvent (ObjDesc);
        break;


    /*  DefSignal   :=  SignalOp    AcpiEventObject */

    case AML_SIGNAL_OP:

        Status = AcpiAmlSystemSignalEvent (ObjDesc);
        break;


    /*  DefSleep    :=  SleepOp     MsecTime    */

    case AML_SLEEP_OP:

        AcpiAmlSystemDoSuspend ((UINT32) ObjDesc->Number.Value);
        break;


    /*  DefStall    :=  StallOp     UsecTime    */

    case AML_STALL_OP:

        AcpiAmlSystemDoStall ((UINT32) ObjDesc->Number.Value);
        break;


    /*  Unknown opcode  */

    default:

        REPORT_ERROR (("AcpiAmlExecMonadic1: Unknown monadic opcode %X\n",
            Opcode));
        Status = AE_AML_BAD_OPCODE;
        break;

    } /* switch */


Cleanup:

    /* Always delete the operand */

    AcpiCmRemoveReference (ObjDesc);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlExecMonadic2R
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 2 monadic operator with numeric operand and
 *              result operand on operand stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlExecMonadic2R (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *ResDesc;
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_OPERAND_OBJECT     *RetDesc2 = NULL;
    UINT32                  ResVal;
    ACPI_STATUS             Status;
    UINT32                  d0;
    UINT32                  d1;
    UINT32                  d2;
    UINT32                  d3;


    FUNCTION_TRACE_PTR ("AmlExecMonadic2R", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE,
                    AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiAmlResolveOperands");

    /* Get all operands */

    Status |= AcpiDsObjStackPopObject (&ResDesc, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecMonadic2R/%s: bad operand(s) (0x%X)\n",
            AcpiPsGetOpcodeName (Opcode), Status));

        goto Cleanup;
    }


    /* Create a return object of type NUMBER for most opcodes */

    switch (Opcode)
    {
    case AML_BIT_NOT_OP:
    case AML_FIND_SET_LEFT_BIT_OP:
    case AML_FIND_SET_RIGHT_BIT_OP:
    case AML_FROM_BCD_OP:
    case AML_TO_BCD_OP:
    case AML_COND_REF_OF_OP:

        RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        break;
    }


    switch (Opcode)
    {
    /*  DefNot  :=  NotOp   Operand Result  */

    case AML_BIT_NOT_OP:

        RetDesc->Number.Value = ~ObjDesc->Number.Value;
        break;


    /*  DefFindSetLeftBit   :=  FindSetLeftBitOp    Operand Result  */

    case AML_FIND_SET_LEFT_BIT_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value;

        /*
         * Acpi specification describes Integer type as a little
         * endian unsigned value, so this boundry condition is valid.
         */
        for (ResVal = 0; RetDesc->Number.Value && ResVal < ACPI_INTEGER_BIT_SIZE; ++ResVal)
        {
            RetDesc->Number.Value >>= 1;
        }

        RetDesc->Number.Value = ResVal;
        break;


    /*  DefFindSetRightBit  :=  FindSetRightBitOp   Operand Result  */

    case AML_FIND_SET_RIGHT_BIT_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value;

        /*
         * Acpi specification describes Integer type as a little
         * endian unsigned value, so this boundry condition is valid.
         */
        for (ResVal = 0; RetDesc->Number.Value && ResVal < ACPI_INTEGER_BIT_SIZE; ++ResVal)
        {
            RetDesc->Number.Value <<= 1;
        }

        /* Since returns must be 1-based, subtract from 33 (65) */

        RetDesc->Number.Value = ResVal == 0 ? 0 : (ACPI_INTEGER_BIT_SIZE + 1) - ResVal;
        break;


    /*  DefFromBDC  :=  FromBCDOp   BCDValue    Result  */

    case AML_FROM_BCD_OP:

        d0 = (UINT32) (ObjDesc->Number.Value & 15);
        d1 = (UINT32) (ObjDesc->Number.Value >> 4 & 15);
        d2 = (UINT32) (ObjDesc->Number.Value >> 8 & 15);
        d3 = (UINT32) (ObjDesc->Number.Value >> 12 & 15);

        if (d0 > 9 || d1 > 9 || d2 > 9 || d3 > 9)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("Monadic2R/FromBCDOp: BCD digit too large %d %d %d %d\n",
                d3, d2, d1, d0));
            Status = AE_AML_NUMERIC_OVERFLOW;
            goto Cleanup;
        }

        RetDesc->Number.Value = d0 + d1 * 10 + d2 * 100 + d3 * 1000;
        break;


    /*  DefToBDC    :=  ToBCDOp Operand Result  */

    case AML_TO_BCD_OP:


        if (ObjDesc->Number.Value > 9999)
        {
            DEBUG_PRINT (ACPI_ERROR, ("Monadic2R/ToBCDOp: BCD overflow: %d\n",
                ObjDesc->Number.Value));
            Status = AE_AML_NUMERIC_OVERFLOW;
            goto Cleanup;
        }

        RetDesc->Number.Value
            = ObjDesc->Number.Value % 10
            + (ObjDesc->Number.Value / 10 % 10 << 4)
            + (ObjDesc->Number.Value / 100 % 10 << 8)
            + (ObjDesc->Number.Value / 1000 % 10 << 12);

        break;


    /*  DefCondRefOf        :=  CondRefOfOp     SourceObject    Result  */

    case AML_COND_REF_OF_OP:

        /*
         * This op is a little strange because the internal return value is
         * different than the return value stored in the result descriptor
         * (There are really two return values)
         */

        if ((ACPI_NAMESPACE_NODE *) ObjDesc == AcpiGbl_RootNode)
        {
            /*
             * This means that the object does not exist in the namespace,
             * return FALSE
             */

            RetDesc->Number.Value = 0;

            /*
             * Must delete the result descriptor since there is no reference
             * being returned
             */

            AcpiCmRemoveReference (ResDesc);
            goto Cleanup;
        }

        /* Get the object reference and store it */

        Status = AcpiAmlGetObjectReference (ObjDesc, &RetDesc2, WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        Status = AcpiAmlExecStore (RetDesc2, ResDesc, WalkState);

        /* The object exists in the namespace, return TRUE */

        RetDesc->Number.Value = ACPI_INTEGER_MAX
        goto Cleanup;
        break;


    case AML_STORE_OP:

        /*
         * A store operand is typically a number, string, buffer or lvalue
         * TBD: [Unhandled] What about a store to a package?
         */

        /*
         * Do the store, and be careful about deleting the source object,
         * since the object itself may have been stored.
         */

        Status = AcpiAmlExecStore (ObjDesc, ResDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            /* On failure, just delete the ObjDesc */

            AcpiCmRemoveReference (ObjDesc);
        }

        else
        {
            /*
             * Normally, we would remove a reference on the ObjDesc parameter;
             * But since it is being used as the internal return object
             * (meaning we would normally increment it), the two cancel out,
             * and we simply don't do anything.
             */
            *ReturnDesc = ObjDesc;
        }

        ObjDesc = NULL;
        return_ACPI_STATUS (Status);

        break;


    case AML_DEBUG_OP:

        /* Reference, returning an Reference */

        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecMonadic2R: DebugOp should never get here!\n"));
        return_ACPI_STATUS (AE_OK);
        break;


    /*
     * These are obsolete opcodes
     */

    /*  DefShiftLeftBit     :=  ShiftLeftBitOp      Source          BitNum  */
    /*  DefShiftRightBit    :=  ShiftRightBitOp     Source          BitNum  */

    case AML_SHIFT_LEFT_BIT_OP:
    case AML_SHIFT_RIGHT_BIT_OP:

        DEBUG_PRINT (ACPI_ERROR, ("AmlExecMonadic2R: %s unimplemented\n",
                        AcpiPsGetOpcodeName (Opcode)));
        Status = AE_SUPPORT;
        goto Cleanup;
        break;


    default:

        REPORT_ERROR (("AcpiAmlExecMonadic2R: Unknown monadic opcode %X\n",
            Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


    Status = AcpiAmlExecStore (RetDesc, ResDesc, WalkState);


Cleanup:
    /* Always delete the operand object */

    AcpiCmRemoveReference (ObjDesc);

    /* Delete return object(s) on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiCmRemoveReference (ResDesc);     /* Result descriptor */
        if (RetDesc)
        {
            AcpiCmRemoveReference (RetDesc);
            RetDesc = NULL;
        }
    }

    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlExecMonadic2
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 2 monadic operator with numeric operand:
 *              DerefOfOp, RefOfOp, SizeOfOp, TypeOp, IncrementOp,
 *              DecrementOp, LNotOp,
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlExecMonadic2 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *TmpDesc;
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_STATUS             ResolveStatus;
    ACPI_STATUS             Status;
    UINT32                  Type;
    ACPI_INTEGER            Value;


    FUNCTION_TRACE_PTR ("AmlExecMonadic2", WALK_OPERANDS);


    /* Attempt to resolve the operands */

    ResolveStatus = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE,
                    AcpiPsGetOpcodeName (Opcode),
                    1, "after AcpiAmlResolveOperands");

    /* Always get all operands */

    Status = AcpiDsObjStackPopObject (&ObjDesc, WalkState);


    /* Now we can check the status codes */

    if (ACPI_FAILURE (ResolveStatus))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecMonadic2[%s]: Could not resolve operands, %s\n",
            AcpiPsGetOpcodeName (Opcode), AcpiCmFormatException (ResolveStatus)));

        goto Cleanup;
    }

    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecMonadic2[%s]: Bad operand(s), %s\n",
            AcpiPsGetOpcodeName (Opcode), AcpiCmFormatException (Status)));

        goto Cleanup;
    }


    /* Get the operand and decode the opcode */


    switch (Opcode)
    {

    /*  DefLNot :=  LNotOp  Operand */

    case AML_LNOT_OP:

        RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        RetDesc->Number.Value = !ObjDesc->Number.Value;
        break;


    /*  DefDecrement    :=  DecrementOp Target  */
    /*  DefIncrement    :=  IncrementOp Target  */

    case AML_DECREMENT_OP:
    case AML_INCREMENT_OP:

        /*
         * Since we are expecting an Reference on the top of the stack, it
         * can be either an Node or an internal object.
         *
         * TBD: [Future] This may be the prototype code for all cases where
         * an Reference is expected!! 10/99
         */

       if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))
       {
           RetDesc = ObjDesc;
       }

       else
       {
            /*
             * Duplicate the Reference in a new object so that we can resolve it
             * without destroying the original Reference object
             */

            RetDesc = AcpiCmCreateInternalObject (INTERNAL_TYPE_REFERENCE);
            if (!RetDesc)
            {
              Status = AE_NO_MEMORY;
               goto Cleanup;
            }

            RetDesc->Reference.OpCode = ObjDesc->Reference.OpCode;
            RetDesc->Reference.Offset = ObjDesc->Reference.Offset;
            RetDesc->Reference.Object = ObjDesc->Reference.Object;
        }


        /*
         * Convert the RetDesc Reference to a Number
         * (This deletes the original RetDesc)
         */

        Status = AcpiAmlResolveOperands (AML_LNOT_OP, &RetDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("ExecMonadic2/%s: bad operand(s) (0x%X)\n",
                AcpiPsGetOpcodeName (Opcode), Status));

            goto Cleanup;
        }

        /* Do the actual increment or decrement */

        if (AML_INCREMENT_OP == Opcode)
        {
            RetDesc->Number.Value++;
        }
        else
        {
            RetDesc->Number.Value--;
        }

        /* Store the result back in the original descriptor */

        Status = AcpiAmlExecStore (RetDesc, ObjDesc, WalkState);

        /* Objdesc was just deleted (because it is an Reference) */

        ObjDesc = NULL;

        break;


    /*  DefObjectType   :=  ObjectTypeOp    SourceObject    */

    case AML_TYPE_OP:

        if (INTERNAL_TYPE_REFERENCE == ObjDesc->Common.Type)
        {
            /*
             * Not a Name -- an indirect name pointer would have
             * been converted to a direct name pointer in ResolveOperands
             */
            switch (ObjDesc->Reference.OpCode)
            {
            case AML_ZERO_OP:
            case AML_ONE_OP:
            case AML_ONES_OP:

                /* Constants are of type Number */

                Type = ACPI_TYPE_NUMBER;
                break;


            case AML_DEBUG_OP:

                /* Per 1.0b spec, Debug object is of type DebugObject */

                Type = ACPI_TYPE_DEBUG_OBJECT;
                break;


            case AML_INDEX_OP:

                /* Get the type of this reference (index into another object) */

                Type = ObjDesc->Reference.TargetType;
                if (Type == ACPI_TYPE_PACKAGE)
                {
                    /*
                     * The main object is a package, we want to get the type
                     * of the individual package element that is referenced by
                     * the index.
                     */
                    Type = (*(ObjDesc->Reference.Where))->Common.Type;
                }

                break;


            case AML_LOCAL_OP:

                Type = AcpiDsMethodDataGetType (MTH_TYPE_LOCAL,
                                (ObjDesc->Reference.Offset), WalkState);
                break;


            case AML_ARG_OP:

                Type = AcpiDsMethodDataGetType (MTH_TYPE_ARG,
                                (ObjDesc->Reference.Offset), WalkState);
                break;


            default:

                REPORT_ERROR (("AcpiAmlExecMonadic2/TypeOp: Internal error - Unknown Reference subtype %X\n",
                    ObjDesc->Reference.OpCode));
                Status = AE_AML_INTERNAL;
                goto Cleanup;
            }
        }

        else
        {
            /*
             * It's not a Reference, so it must be a direct name pointer.
             */
            Type = AcpiNsGetType ((ACPI_HANDLE) ObjDesc);
        }

        /* Allocate a descriptor to hold the type. */

        RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        RetDesc->Number.Value = Type;
        break;


    /*  DefSizeOf   :=  SizeOfOp    SourceObject    */

    case AML_SIZE_OF_OP:

        if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))
        {
            ObjDesc = AcpiNsGetAttachedObject (ObjDesc);
        }

        if (!ObjDesc)
        {
            Value = 0;
        }

        else
        {
            switch (ObjDesc->Common.Type)
            {

            case ACPI_TYPE_BUFFER:

                Value = ObjDesc->Buffer.Length;
                break;


            case ACPI_TYPE_STRING:

                Value = ObjDesc->String.Length;
                break;


            case ACPI_TYPE_PACKAGE:

                Value = ObjDesc->Package.Count;
                break;

            case INTERNAL_TYPE_REFERENCE:

                Value = 4;
                break;

            default:

                DEBUG_PRINT (ACPI_ERROR,
                    ("AmlExecMonadic2: Not Buf/Str/Pkg - found type 0x%X\n",
                    ObjDesc->Common.Type));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
        }

        /*
         * Now that we have the size of the object, create a result
         * object to hold the value
         */

        RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        RetDesc->Number.Value = Value;
        break;


    /*  DefRefOf    :=  RefOfOp     SourceObject    */

    case AML_REF_OF_OP:

        Status = AcpiAmlGetObjectReference (ObjDesc, &RetDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
        break;


    /*  DefDerefOf  :=  DerefOfOp   ObjReference    */

    case AML_DEREF_OF_OP:


        /* Check for a method local or argument */

        if (!VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))
        {
            /*
             * Must resolve/dereference the local/arg reference first
             */
            switch (ObjDesc->Reference.OpCode)
            {
            /* Set ObjDesc to the value of the local/arg */

            case AML_LOCAL_OP:

                AcpiDsMethodDataGetValue (MTH_TYPE_LOCAL,
                        (ObjDesc->Reference.Offset), WalkState, &TmpDesc);

                /*
                 * Delete our reference to the input object and
                 * point to the object just retrieved
                 */
                AcpiCmRemoveReference (ObjDesc);
                ObjDesc = TmpDesc;
                break;


            case AML_ARG_OP:

                AcpiDsMethodDataGetValue (MTH_TYPE_ARG,
                        (ObjDesc->Reference.Offset), WalkState, &TmpDesc);

                /*
                 * Delete our reference to the input object and
                 * point to the object just retrieved
                 */
                AcpiCmRemoveReference (ObjDesc);
                ObjDesc = TmpDesc;
                break;

            default:

                /* Index op - handled below */
                break;
            }
        }


        /* ObjDesc may have changed from the code above */

        if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))
        {
            /* Get the actual object from the Node (This is the dereference) */

            RetDesc = ((ACPI_NAMESPACE_NODE *) ObjDesc)->Object;

            /* Returning a pointer to the object, add another reference! */

            AcpiCmAddReference (RetDesc);
        }

        else
        {
            /*
             * This must be a reference object produced by the Index
             * ASL operation -- check internal opcode
             */

            if ((ObjDesc->Reference.OpCode != AML_INDEX_OP) &&
                (ObjDesc->Reference.OpCode != AML_REF_OF_OP))
            {
                DEBUG_PRINT (ACPI_ERROR,
                    ("AmlExecMonadic2: DerefOf, invalid obj ref %p\n",
                    ObjDesc));

                Status = AE_TYPE;
                goto Cleanup;
            }


            switch (ObjDesc->Reference.OpCode)
            {
            case AML_INDEX_OP:

                /*
                 * Supported target types for the Index operator are
                 * 1) A Buffer
                 * 2) A Package
                 */

                if (ObjDesc->Reference.TargetType == ACPI_TYPE_BUFFER_FIELD)
                {
                    /*
                     * The target is a buffer, we must create a new object that
                     * contains one element of the buffer, the element pointed
                     * to by the index.
                     *
                     * NOTE: index into a buffer is NOT a pointer to a
                     * sub-buffer of the main buffer, it is only a pointer to a
                     * single element (byte) of the buffer!
                     */
                    RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
                    if (!RetDesc)
                    {
                        Status = AE_NO_MEMORY;
                        goto Cleanup;
                    }

                    TmpDesc = ObjDesc->Reference.Object;
                    RetDesc->Number.Value =
                        TmpDesc->Buffer.Pointer[ObjDesc->Reference.Offset];

                    /* TBD: [Investigate] (see below) Don't add an additional
                     * ref!
                     */
                }

                else if (ObjDesc->Reference.TargetType == ACPI_TYPE_PACKAGE)
                {
                    /*
                     * The target is a package, we want to return the referenced
                     * element of the package.  We must add another reference to
                     * this object, however.
                     */

                    RetDesc = *(ObjDesc->Reference.Where);
                    if (!RetDesc)
                    {
                        /*
                         * We can't return a NULL dereferenced value.  This is
                         * an uninitialized package element and is thus a
                         * severe error.
                         */

                        DEBUG_PRINT (ACPI_ERROR,
                            ("AmlExecMonadic2: DerefOf, NULL package element obj %p\n",
                            ObjDesc));
                        Status = AE_AML_UNINITIALIZED_ELEMENT;
                        goto Cleanup;
                    }

                    AcpiCmAddReference (RetDesc);
                }

                else
                {
                    DEBUG_PRINT (ACPI_ERROR,
                        ("AmlExecMonadic2: DerefOf, Unknown TargetType %X in obj %p\n",
                        ObjDesc->Reference.TargetType, ObjDesc));
                    Status = AE_AML_OPERAND_TYPE;
                    goto Cleanup;
                }

                break;


            case AML_REF_OF_OP:

                RetDesc = ObjDesc->Reference.Object;

                /* Add another reference to the object! */

                AcpiCmAddReference (RetDesc);
                break;
            }
        }

        break;


    default:

        REPORT_ERROR (("AcpiAmlExecMonadic2: Unknown monadic opcode %X\n",
            Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


Cleanup:

    if (ObjDesc)
    {
        AcpiCmRemoveReference (ObjDesc);
    }

    /* Delete return object on error */

    if (ACPI_FAILURE (Status) &&
        (RetDesc))
    {
        AcpiCmRemoveReference (RetDesc);
        RetDesc = NULL;
    }

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}

