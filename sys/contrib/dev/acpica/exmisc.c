
/******************************************************************************
 *
 * Module Name: exmisc - ACPI AML (p-code) execution - specific opcodes
 *              $Revision: 116 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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

#define __EXMISC_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exmisc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExGetObjectReference
 *
 * PARAMETERS:  ObjDesc             - Create a reference to this object
 *              ReturnDesc          - Where to store the reference
 *              WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtain and return a "reference" to the target object
 *              Common code for the RefOfOp and the CondRefOfOp.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExGetObjectReference (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ReturnDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ReferenceObj;
    ACPI_OPERAND_OBJECT     *ReferencedObj;


    ACPI_FUNCTION_TRACE_PTR ("ExGetObjectReference", ObjDesc);


    *ReturnDesc = NULL;

    switch (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc))
    {
    case ACPI_DESC_TYPE_OPERAND:

        if (ACPI_GET_OBJECT_TYPE (ObjDesc) != ACPI_TYPE_LOCAL_REFERENCE)
        {
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        /*
         * Must be a reference to a Local or Arg
         */
        switch (ObjDesc->Reference.Opcode)
        {
        case AML_LOCAL_OP:
        case AML_ARG_OP:

            /* The referenced object is the pseudo-node for the local/arg */

            ReferencedObj = ObjDesc->Reference.Object;
            break;

        default:

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown Reference subtype %X\n",
                ObjDesc->Reference.Opcode));
            return_ACPI_STATUS (AE_AML_INTERNAL);
        }
        break;


    case ACPI_DESC_TYPE_NAMED:

        /*
         * A named reference that has already been resolved to a Node
         */
        ReferencedObj = ObjDesc;
        break;


    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%p has invalid descriptor [%s]\n",
                ObjDesc, AcpiUtGetDescriptorName (ObjDesc)));
        return_ACPI_STATUS (AE_TYPE);
    }


    /* Create a new reference object */

    ReferenceObj = AcpiUtCreateInternalObject (ACPI_TYPE_LOCAL_REFERENCE);
    if (!ReferenceObj)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    ReferenceObj->Reference.Opcode = AML_REF_OF_OP;
    ReferenceObj->Reference.Object = ReferencedObj;
    *ReturnDesc = ReferenceObj;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Object %p Type [%s], returning Reference %p\n",
            ObjDesc, AcpiUtGetObjectTypeName (ObjDesc), *ReturnDesc));

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConcatTemplate
 *
 * PARAMETERS:  *ObjDesc            - Object to be converted.  Must be an
 *                                    Integer, Buffer, or String
 *              WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two resource templates
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConcatTemplate (
    ACPI_OPERAND_OBJECT     *ObjDesc1,
    ACPI_OPERAND_OBJECT     *ObjDesc2,
    ACPI_OPERAND_OBJECT     **ActualReturnDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    UINT8                   *NewBuf;
    UINT8                   *EndTag1;
    UINT8                   *EndTag2;
    ACPI_SIZE               Length1;
    ACPI_SIZE               Length2;


    ACPI_FUNCTION_TRACE ("ExConcatTemplate");


    /* Find the EndTags in each resource template */

    EndTag1 = AcpiUtGetResourceEndTag (ObjDesc1);
    EndTag2 = AcpiUtGetResourceEndTag (ObjDesc2);
    if (!EndTag1 || !EndTag2)
    {
        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    /* Compute the length of each part */

    Length1 = ACPI_PTR_DIFF (EndTag1, ObjDesc1->Buffer.Pointer);
    Length2 = ACPI_PTR_DIFF (EndTag2, ObjDesc2->Buffer.Pointer) +
                             2; /* Size of END_TAG */

    /* Create a new buffer object for the result */

    ReturnDesc = AcpiUtCreateBufferObject (Length1 + Length2);
    if (!ReturnDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Copy the templates to the new descriptor */

    NewBuf = ReturnDesc->Buffer.Pointer;
    ACPI_MEMCPY (NewBuf, ObjDesc1->Buffer.Pointer, Length1);
    ACPI_MEMCPY (NewBuf + Length1, ObjDesc2->Buffer.Pointer, Length2);

    /* Compute the new checksum */

    NewBuf[ReturnDesc->Buffer.Length - 1] =
            AcpiUtGenerateChecksum (ReturnDesc->Buffer.Pointer,
                                   (ReturnDesc->Buffer.Length - 1));

    /* Return the completed template descriptor */

    *ActualReturnDesc = ReturnDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDoConcatenate
 *
 * PARAMETERS:  ObjDesc1            - First source object
 *              ObjDesc2            - Second source object
 *              ActualReturnDesc    - Where to place the return object
 *              WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two objects OF THE SAME TYPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDoConcatenate (
    ACPI_OPERAND_OBJECT     *ObjDesc1,
    ACPI_OPERAND_OBJECT     *ObjDesc2,
    ACPI_OPERAND_OBJECT     **ActualReturnDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    UINT32                  i;
    ACPI_INTEGER            ThisInteger;
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    char                    *NewBuf;


    ACPI_FUNCTION_ENTRY ();


    /*
     * There are three cases to handle:
     *
     * 1) Two Integers concatenated to produce a new Buffer
     * 2) Two Strings concatenated to produce a new String
     * 3) Two Buffers concatenated to produce a new Buffer
     */
    switch (ACPI_GET_OBJECT_TYPE (ObjDesc1))
    {
    case ACPI_TYPE_INTEGER:

        /* Result of two Integers is a Buffer */
        /* Need enough buffer space for two integers */

        ReturnDesc = AcpiUtCreateBufferObject (AcpiGbl_IntegerByteWidth * 2);
        if (!ReturnDesc)
        {
            return (AE_NO_MEMORY);
        }

        NewBuf = (char *) ReturnDesc->Buffer.Pointer;

        /* Convert the first integer */

        ThisInteger = ObjDesc1->Integer.Value;
        for (i = 0; i < AcpiGbl_IntegerByteWidth; i++)
        {
            NewBuf[i] = (char) ThisInteger;
            ThisInteger >>= 8;
        }

        /* Convert the second integer */

        ThisInteger = ObjDesc2->Integer.Value;
        for (; i < (ACPI_MUL_2 (AcpiGbl_IntegerByteWidth)); i++)
        {
            NewBuf[i] = (char) ThisInteger;
            ThisInteger >>= 8;
        }

        break;


    case ACPI_TYPE_STRING:

        /* Result of two Strings is a String */

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_STRING);
        if (!ReturnDesc)
        {
            return (AE_NO_MEMORY);
        }

        /* Operand0 is string  */

        NewBuf = ACPI_MEM_CALLOCATE ((ACPI_SIZE) ObjDesc1->String.Length +
                                     (ACPI_SIZE) ObjDesc2->String.Length + 1);
        if (!NewBuf)
        {
            ACPI_REPORT_ERROR
                (("ExDoConcatenate: String allocation failure\n"));
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /* Concatenate the strings */

        ACPI_STRCPY (NewBuf, ObjDesc1->String.Pointer);
        ACPI_STRCPY (NewBuf + ObjDesc1->String.Length,
                              ObjDesc2->String.Pointer);

        /* Complete the String object initialization */

        ReturnDesc->String.Pointer = NewBuf;
        ReturnDesc->String.Length  = ObjDesc1->String.Length +
                                     ObjDesc2->String.Length;
        break;


    case ACPI_TYPE_BUFFER:

        /* Result of two Buffers is a Buffer */

        ReturnDesc = AcpiUtCreateBufferObject (
                            (ACPI_SIZE) ObjDesc1->Buffer.Length +
                            (ACPI_SIZE) ObjDesc2->Buffer.Length);
        if (!ReturnDesc)
        {
            return (AE_NO_MEMORY);
        }

        NewBuf = (char *) ReturnDesc->Buffer.Pointer;

        /* Concatenate the buffers */

        ACPI_MEMCPY (NewBuf, ObjDesc1->Buffer.Pointer,
                        ObjDesc1->Buffer.Length);
        ACPI_MEMCPY (NewBuf + ObjDesc1->Buffer.Length, ObjDesc2->Buffer.Pointer,
                         ObjDesc2->Buffer.Length);

        break;


    default:

        /* Invalid object type, should not happen here */

        Status = AE_AML_INTERNAL;
        ReturnDesc = NULL;
    }

    *ActualReturnDesc = ReturnDesc;
    return (AE_OK);


Cleanup:

    AcpiUtRemoveReference (ReturnDesc);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDoMathOp
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Operand0            - Integer operand #0
 *              Operand1            - Integer operand #1
 *
 * RETURN:      Integer result of the operation
 *
 * DESCRIPTION: Execute a math AML opcode. The purpose of having all of the
 *              math functions here is to prevent a lot of pointer dereferencing
 *              to obtain the operands.
 *
 ******************************************************************************/

ACPI_INTEGER
AcpiExDoMathOp (
    UINT16                  Opcode,
    ACPI_INTEGER            Operand0,
    ACPI_INTEGER            Operand1)
{


    switch (Opcode)
    {
    case AML_ADD_OP:                /* Add (Operand0, Operand1, Result) */

        return (Operand0 + Operand1);


    case AML_BIT_AND_OP:            /* And (Operand0, Operand1, Result) */

        return (Operand0 & Operand1);


    case AML_BIT_NAND_OP:           /* NAnd (Operand0, Operand1, Result) */

        return (~(Operand0 & Operand1));


    case AML_BIT_OR_OP:             /* Or (Operand0, Operand1, Result) */

        return (Operand0 | Operand1);


    case AML_BIT_NOR_OP:            /* NOr (Operand0, Operand1, Result) */

        return (~(Operand0 | Operand1));


    case AML_BIT_XOR_OP:            /* XOr (Operand0, Operand1, Result) */

        return (Operand0 ^ Operand1);


    case AML_MULTIPLY_OP:           /* Multiply (Operand0, Operand1, Result) */

        return (Operand0 * Operand1);


    case AML_SHIFT_LEFT_OP:         /* ShiftLeft (Operand, ShiftCount, Result) */

        return (Operand0 << Operand1);


    case AML_SHIFT_RIGHT_OP:        /* ShiftRight (Operand, ShiftCount, Result) */

        return (Operand0 >> Operand1);


    case AML_SUBTRACT_OP:           /* Subtract (Operand0, Operand1, Result) */

        return (Operand0 - Operand1);

    default:

        return (0);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDoLogicalOp
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Operand0            - Integer operand #0
 *              Operand1            - Integer operand #1
 *
 * RETURN:      TRUE/FALSE result of the operation
 *
 * DESCRIPTION: Execute a logical AML opcode. The purpose of having all of the
 *              functions here is to prevent a lot of pointer dereferencing
 *              to obtain the operands and to simplify the generation of the
 *              logical value.
 *
 *              Note: cleanest machine code seems to be produced by the code
 *              below, rather than using statements of the form:
 *                  Result = (Operand0 == Operand1);
 *
 ******************************************************************************/

BOOLEAN
AcpiExDoLogicalOp (
    UINT16                  Opcode,
    ACPI_INTEGER            Operand0,
    ACPI_INTEGER            Operand1)
{


    switch (Opcode)
    {

    case AML_LAND_OP:               /* LAnd (Operand0, Operand1) */

        if (Operand0 && Operand1)
        {
            return (TRUE);
        }
        break;


    case AML_LEQUAL_OP:             /* LEqual (Operand0, Operand1) */

        if (Operand0 == Operand1)
        {
            return (TRUE);
        }
        break;


    case AML_LGREATER_OP:           /* LGreater (Operand0, Operand1) */

        if (Operand0 > Operand1)
        {
            return (TRUE);
        }
        break;


    case AML_LLESS_OP:              /* LLess (Operand0, Operand1) */

        if (Operand0 < Operand1)
        {
            return (TRUE);
        }
        break;


    case AML_LOR_OP:                 /* LOr (Operand0, Operand1) */

        if (Operand0 || Operand1)
        {
            return (TRUE);
        }
        break;

    default:
        break;
    }

    return (FALSE);
}


