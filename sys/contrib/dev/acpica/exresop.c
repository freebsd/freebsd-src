
/******************************************************************************
 *
 * Module Name: exresop - AML Interpreter operand/object resolution
 *              $Revision: 55 $
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

#define __EXRESOP_C__

#include "acpi.h"
#include "amlcode.h"
#include "acparser.h"
#include "acinterp.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exresop")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExCheckObjectType
 *
 * PARAMETERS:  TypeNeeded          Object type needed
 *              ThisType            Actual object type
 *              Object              Object pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check required type against actual type
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExCheckObjectType (
    ACPI_OBJECT_TYPE        TypeNeeded,
    ACPI_OBJECT_TYPE        ThisType,
    void                    *Object)
{
    ACPI_FUNCTION_NAME ("ExCheckObjectType");


    if (TypeNeeded == ACPI_TYPE_ANY)
    {
        /* All types OK, so we don't perform any typechecks */

        return (AE_OK);
    }

    if (TypeNeeded == INTERNAL_TYPE_REFERENCE)
    {
        /*
         * Allow the AML "Constant" opcodes (Zero, One, etc.) to be reference
         * objects and thus allow them to be targets.  (As per the ACPI
         * specification, a store to a constant is a noop.)
         */
        if ((ThisType == ACPI_TYPE_INTEGER) &&
            (((ACPI_OPERAND_OBJECT *) Object)->Common.Flags & AOPOBJ_AML_CONSTANT))
        {
            return (AE_OK);
        }
    }

    if (TypeNeeded != ThisType)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Needed [%s], found [%s] %p\n",
            AcpiUtGetTypeName (TypeNeeded),
            AcpiUtGetTypeName (ThisType), Object));

        return (AE_AML_OPERAND_TYPE);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExResolveOperands
 *
 * PARAMETERS:  Opcode              - Opcode being interpreted
 *              StackPtr            - Pointer to the operand stack to be
 *                                    resolved
 *              WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert multiple input operands to the types required by the
 *              target operator.
 *
 *      Each 5-bit group in ArgTypes represents one required
 *      operand and indicates the required Type. The corresponding operand
 *      will be converted to the required type if possible, otherwise we
 *      abort with an exception.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExResolveOperands (
    UINT16                  Opcode,
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status = AE_OK;
    UINT8                   ObjectType;
    void                    *TempNode;
    UINT32                  ArgTypes;
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  ThisArgType;
    ACPI_OBJECT_TYPE        TypeNeeded;


    ACPI_FUNCTION_TRACE_U32 ("ExResolveOperands", Opcode);


    OpInfo = AcpiPsGetOpcodeInfo (Opcode);
    if (OpInfo->Class == AML_CLASS_UNKNOWN)
    {
        return_ACPI_STATUS (AE_AML_BAD_OPCODE);
    }

    ArgTypes = OpInfo->RuntimeArgs;
    if (ArgTypes == ARGI_INVALID_OPCODE)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - %X is not a valid AML opcode\n",
            Opcode));

        return_ACPI_STATUS (AE_AML_INTERNAL);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Opcode %X [%s] OperandTypes=%X \n",
        Opcode, OpInfo->Name, ArgTypes));

    /*
     * Normal exit is with (ArgTypes == 0) at end of argument list.
     * Function will return an exception from within the loop upon
     * finding an entry which is not (or cannot be converted
     * to) the required type; if stack underflows; or upon
     * finding a NULL stack entry (which should not happen).
     */
    while (GET_CURRENT_ARG_TYPE (ArgTypes))
    {
        if (!StackPtr || !*StackPtr)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Internal - null stack entry at %p\n",
                StackPtr));

            return_ACPI_STATUS (AE_AML_INTERNAL);
        }

        /* Extract useful items */

        ObjDesc = *StackPtr;

        /* Decode the descriptor type */

        switch (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc))
        {
        case ACPI_DESC_TYPE_NAMED:

            /* Node */

            ObjectType = ((ACPI_NAMESPACE_NODE *) ObjDesc)->Type;
            break;


        case ACPI_DESC_TYPE_OPERAND:

            /* ACPI internal object */

            ObjectType = ACPI_GET_OBJECT_TYPE (ObjDesc);

            /* Check for bad ACPI_OBJECT_TYPE */

            if (!AcpiExValidateObjectType (ObjectType))
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Bad operand object type [%X]\n",
                    ObjectType));

                return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
            }

            if (ObjectType == (UINT8) INTERNAL_TYPE_REFERENCE)
            {
                /*
                 * Decode the Reference
                 */
                OpInfo = AcpiPsGetOpcodeInfo (Opcode);
                if (OpInfo->Class == AML_CLASS_UNKNOWN)
                {
                    return_ACPI_STATUS (AE_AML_BAD_OPCODE);
                }

                switch (ObjDesc->Reference.Opcode)
                {
                case AML_DEBUG_OP:
                case AML_NAME_OP:
                case AML_INDEX_OP:
                case AML_REF_OF_OP:
                case AML_ARG_OP:
                case AML_LOCAL_OP:

                    ACPI_DEBUG_ONLY_MEMBERS (ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
                        "Reference Opcode: %s\n", OpInfo->Name)));
                    break;

                default:
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Unknown Reference Opcode %X\n",
                        ObjDesc->Reference.Opcode));

                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }
            }
            break;


        default:

            /* Invalid descriptor */

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Bad descriptor type %X in Obj %p\n",
                ACPI_GET_DESCRIPTOR_TYPE (ObjDesc), ObjDesc));

            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }


        /*
         * Get one argument type, point to the next
         */
        ThisArgType = GET_CURRENT_ARG_TYPE (ArgTypes);
        INCREMENT_ARG_LIST (ArgTypes);

        /*
         * Handle cases where the object does not need to be
         * resolved to a value
         */
        switch (ThisArgType)
        {
        case ARGI_REF_OR_STRING:        /* Can be a String or Reference */

            if ((ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) == ACPI_DESC_TYPE_OPERAND) &&
                (ACPI_GET_OBJECT_TYPE (ObjDesc) == ACPI_TYPE_STRING))
            {
                /*
                 * String found - the string references a named object and must be
                 * resolved to a node
                 */
                goto NextOperand;
            }

            /* Else not a string - fall through to the normal Reference case below */
            /*lint -fallthrough */

        case ARGI_REFERENCE:            /* References: */
        case ARGI_INTEGER_REF:
        case ARGI_OBJECT_REF:
        case ARGI_DEVICE_REF:
        case ARGI_TARGETREF:            /* Allows implicit conversion rules before store */
        case ARGI_FIXED_TARGET:         /* No implicit conversion before store to target */
        case ARGI_SIMPLE_TARGET:        /* Name, Local, or Arg - no implicit conversion  */

            /* Need an operand of type INTERNAL_TYPE_REFERENCE */

            if (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) == ACPI_DESC_TYPE_NAMED) /* Node (name) ptr OK as-is */
            {
                goto NextOperand;
            }

            Status = AcpiExCheckObjectType (INTERNAL_TYPE_REFERENCE,
                            ObjectType, ObjDesc);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            if (AML_NAME_OP == ObjDesc->Reference.Opcode)
            {
                /*
                 * Convert an indirect name ptr to direct name ptr and put
                 * it on the stack
                 */
                TempNode = ObjDesc->Reference.Object;
                AcpiUtRemoveReference (ObjDesc);
                (*StackPtr) = TempNode;
            }
            goto NextOperand;


        case ARGI_ANYTYPE:

            /*
             * We don't want to resolve IndexOp reference objects during
             * a store because this would be an implicit DeRefOf operation.
             * Instead, we just want to store the reference object.
             * -- All others must be resolved below.
             */
            if ((Opcode == AML_STORE_OP) &&
                (ACPI_GET_OBJECT_TYPE (*StackPtr) == INTERNAL_TYPE_REFERENCE) &&
                ((*StackPtr)->Reference.Opcode == AML_INDEX_OP))
            {
                goto NextOperand;
            }
            break;

        default:
            /* All cases covered above */
            break;
        }


        /*
         * Resolve this object to a value
         */
        Status = AcpiExResolveToValue (StackPtr, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * Check the resulting object (value) type
         */
        switch (ThisArgType)
        {
        /*
         * For the simple cases, only one type of resolved object
         * is allowed
         */
        case ARGI_MUTEX:

            /* Need an operand of type ACPI_TYPE_MUTEX */

            TypeNeeded = ACPI_TYPE_MUTEX;
            break;

        case ARGI_EVENT:

            /* Need an operand of type ACPI_TYPE_EVENT */

            TypeNeeded = ACPI_TYPE_EVENT;
            break;

        case ARGI_REGION:

            /* Need an operand of type ACPI_TYPE_REGION */

            TypeNeeded = ACPI_TYPE_REGION;
            break;

        case ARGI_IF:   /* If */

            /* Need an operand of type INTERNAL_TYPE_IF */

            TypeNeeded = INTERNAL_TYPE_IF;
            break;

        case ARGI_PACKAGE:   /* Package */

            /* Need an operand of type ACPI_TYPE_PACKAGE */

            TypeNeeded = ACPI_TYPE_PACKAGE;
            break;

        case ARGI_ANYTYPE:

            /* Any operand type will do */

            TypeNeeded = ACPI_TYPE_ANY;
            break;


        /*
         * The more complex cases allow multiple resolved object types
         */
        case ARGI_INTEGER:   /* Number */

            /*
             * Need an operand of type ACPI_TYPE_INTEGER,
             * But we can implicitly convert from a STRING or BUFFER
             */
            Status = AcpiExConvertToInteger (*StackPtr, StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_TYPE)
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Needed [Integer/String/Buffer], found [%s] %p\n",
                        AcpiUtGetObjectTypeName (*StackPtr), *StackPtr));

                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }

                return_ACPI_STATUS (Status);
            }
            goto NextOperand;


        case ARGI_BUFFER:

            /*
             * Need an operand of type ACPI_TYPE_BUFFER,
             * But we can implicitly convert from a STRING or INTEGER
             */
            Status = AcpiExConvertToBuffer (*StackPtr, StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_TYPE)
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Needed [Integer/String/Buffer], found [%s] %p\n",
                        AcpiUtGetObjectTypeName (*StackPtr), *StackPtr));

                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }

                return_ACPI_STATUS (Status);
            }
            goto NextOperand;


        case ARGI_STRING:

            /*
             * Need an operand of type ACPI_TYPE_STRING,
             * But we can implicitly convert from a BUFFER or INTEGER
             */
            Status = AcpiExConvertToString (*StackPtr, StackPtr, 16, ACPI_UINT32_MAX, WalkState);
            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_TYPE)
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Needed [Integer/String/Buffer], found [%s] %p\n",
                        AcpiUtGetObjectTypeName (*StackPtr), *StackPtr));

                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }

                return_ACPI_STATUS (Status);
            }
            goto NextOperand;


        case ARGI_COMPUTEDATA:

            /* Need an operand of type INTEGER, STRING or BUFFER */

            switch (ACPI_GET_OBJECT_TYPE (*StackPtr))
            {
            case ACPI_TYPE_INTEGER:
            case ACPI_TYPE_STRING:
            case ACPI_TYPE_BUFFER:

                /* Valid operand */
               break;

            default:
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Needed [Integer/String/Buffer], found [%s] %p\n",
                    AcpiUtGetObjectTypeName (*StackPtr), *StackPtr));

                return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
            }
            goto NextOperand;


        case ARGI_DATAOBJECT:
            /*
             * ARGI_DATAOBJECT is only used by the SizeOf operator.
             * Need a buffer, string, package, or Node reference.
             *
             * The only reference allowed here is a direct reference to
             * a namespace node.
             */
            if (ACPI_GET_OBJECT_TYPE (*StackPtr) == INTERNAL_TYPE_REFERENCE)
            {
                if (!(*StackPtr)->Reference.Node)
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Needed [Node Reference], found [%p]\n",
                        *StackPtr));

                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }

                /* Get the object attached to the node */

                TempNode = AcpiNsGetAttachedObject ((*StackPtr)->Reference.Node);
                if (!TempNode)
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Node [%p] has no attached object\n",
                        (*StackPtr)->Reference.Node));

                    return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
                }

                /*
                 * Swap the reference object with the node's object.  Must add
                 * a reference to the node object, and remove a reference from
                 * the original reference object.
                 */
                AcpiUtAddReference (TempNode);
                AcpiUtRemoveReference (*StackPtr);
                (*StackPtr) = TempNode;
            }

            /* Need a buffer, string, package */

            switch (ACPI_GET_OBJECT_TYPE (*StackPtr))
            {
            case ACPI_TYPE_PACKAGE:
            case ACPI_TYPE_STRING:
            case ACPI_TYPE_BUFFER:

                /* Valid operand */
                break;

            default:
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Needed [Buf/Str/Pkg], found [%s] %p\n",
                    AcpiUtGetObjectTypeName (*StackPtr), *StackPtr));

                return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
            }
            goto NextOperand;


        case ARGI_COMPLEXOBJ:

            /* Need a buffer or package or (ACPI 2.0) String */

            switch (ACPI_GET_OBJECT_TYPE (*StackPtr))
            {
            case ACPI_TYPE_PACKAGE:
            case ACPI_TYPE_STRING:
            case ACPI_TYPE_BUFFER:

                /* Valid operand */
                break;

            default:
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Needed [Buf/Str/Pkg], found [%s] %p\n",
                    AcpiUtGetObjectTypeName (*StackPtr), *StackPtr));

                return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
            }
            goto NextOperand;


        default:

            /* Unknown type */

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Internal - Unknown ARGI type %X\n",
                ThisArgType));

            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }

        /*
         * Make sure that the original object was resolved to the
         * required object type (Simple cases only).
         */
        Status = AcpiExCheckObjectType (TypeNeeded,
                        ACPI_GET_OBJECT_TYPE (*StackPtr), *StackPtr);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

NextOperand:
        /*
         * If more operands needed, decrement StackPtr to point
         * to next operand on stack
         */
        if (GET_CURRENT_ARG_TYPE (ArgTypes))
        {
            StackPtr--;
        }

    }   /* while (*Types) */

    return_ACPI_STATUS (Status);
}


