
/******************************************************************************
 *
 * Module Name: amresop - AML Interpreter operand/object resolution
 *              $Revision: 15 $
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

#define __AMRESOP_C__

#include "acpi.h"
#include "amlcode.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"
#include "acevents.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amresop")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlResolveOperands
 *
 * PARAMETERS:  Opcode              Opcode being interpreted
 *              StackPtr            Top of operand stack
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert stack entries to required types
 *
 *      Each nibble in ArgTypes represents one required operand
 *      and indicates the required Type:
 *
 *      The corresponding stack entry will be converted to the
 *      required type if possible, else return an exception
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlResolveOperands (
    UINT16                  Opcode,
    ACPI_OPERAND_OBJECT     **StackPtr,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status = AE_OK;
    UINT8                   ObjectType;
    ACPI_HANDLE             TempHandle;
    UINT32                  ArgTypes;
    ACPI_OPCODE_INFO        *OpInfo;
    UINT32                  ThisArgType;


    FUNCTION_TRACE_U32 ("AmlResolveOperands", Opcode);


    OpInfo = AcpiPsGetOpcodeInfo (Opcode);
    if (ACPI_GET_OP_TYPE (OpInfo) != ACPI_OP_TYPE_OPCODE)
    {
        return_ACPI_STATUS (AE_AML_BAD_OPCODE);
    }


    ArgTypes = OpInfo->RuntimeArgs;
    if (ArgTypes == ARGI_INVALID_OPCODE)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlResolveOperands: Internal error - %X is not a runtime opcode\n", Opcode));
        Status = AE_AML_INTERNAL;
        goto Cleanup;
    }

    DEBUG_PRINT (TRACE_EXEC,
        ("AmlResolveOperands: Opcode %X OperandTypes=%X \n",
        Opcode, ArgTypes));


   /*
     * Normal exit is with *Types == '\0' at end of string.
     * Function will return an exception from within the loop upon
     * finding an entry which is not, and cannot be converted
     * to, the required type; if stack underflows; or upon
     * finding a NULL stack entry (which "should never happen").
     */

    while (GET_CURRENT_ARG_TYPE (ArgTypes))
    {
        if (!StackPtr || !*StackPtr)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlResolveOperands: Internal error - null stack entry at %X\n", StackPtr));
            Status = AE_AML_INTERNAL;
            goto Cleanup;
        }

        /* Extract useful items */

        ObjDesc = *StackPtr;

        /* Decode the descriptor type */

        if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))
        {
            /* Node */

            ObjectType = ((ACPI_NAMESPACE_NODE *) ObjDesc)->Type;
        }

        else if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_INTERNAL))
        {
            /* ACPI internal object */

            ObjectType = ObjDesc->Common.Type;

            /* Check for bad ACPI_OBJECT_TYPE */

            if (!AcpiAmlValidateObjectType (ObjectType))
            {
                DEBUG_PRINT (ACPI_ERROR,
                    ("AmlResolveOperands: Bad operand object type [0x%x]\n",
                    ObjectType));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }

            if (ObjectType == (UINT8) INTERNAL_TYPE_REFERENCE)
            {
                /*
                 * Decode the Reference
                 */

                OpInfo = AcpiPsGetOpcodeInfo (Opcode);
                if (ACPI_GET_OP_TYPE (OpInfo) != ACPI_OP_TYPE_OPCODE)
                {
                    return_ACPI_STATUS (AE_AML_BAD_OPCODE);
                }


                switch (ObjDesc->Reference.OpCode)
                {
                case AML_ZERO_OP:
                case AML_ONE_OP:
                case AML_ONES_OP:
                case AML_DEBUG_OP:
                case AML_NAME_OP:
                case AML_INDEX_OP:
                case AML_ARG_OP:
                case AML_LOCAL_OP:

                    DEBUG_ONLY_MEMBERS (DEBUG_PRINT (ACPI_INFO,
                        ("Reference Opcode: %s\n", OpInfo->Name)));
                    break;

                default:
                    DEBUG_PRINT (ACPI_INFO,
                        ("Reference Opcode: Unknown [%02x]\n",
                        ObjDesc->Reference.OpCode));

                    Status = AE_AML_OPERAND_TYPE;
                    goto Cleanup;
                    break;
                }
            }

        }

        else
        {
            /* Invalid descriptor */

            DEBUG_PRINT (ACPI_ERROR,
                ("Bad descriptor type 0x%X in Obj %p\n",
                ObjDesc->Common.DataType, ObjDesc));

            Status = AE_AML_OPERAND_TYPE;
            goto Cleanup;
        }


        /*
         * Decode a character from the type string
         */

        ThisArgType = GET_CURRENT_ARG_TYPE (ArgTypes);
        INCREMENT_ARG_LIST (ArgTypes);


        switch (ThisArgType)
        {

        case ARGI_REFERENCE:   /* Reference */
        case ARGI_TARGETREF:

            /* Need an operand of type INTERNAL_TYPE_REFERENCE */

            if (VALID_DESCRIPTOR_TYPE (ObjDesc, ACPI_DESC_TYPE_NAMED))             /* direct name ptr OK as-is */
            {
                break;
            }

            if (INTERNAL_TYPE_REFERENCE != ObjectType)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Reference, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }

            if (AML_NAME_OP == ObjDesc->Reference.OpCode)
            {
                /*
                 * Convert an indirect name ptr to direct name ptr and put
                 * it on the stack
                 */

                TempHandle = ObjDesc->Reference.Object;
                AcpiCmRemoveReference (ObjDesc);
                (*StackPtr) = TempHandle;
            }
            break;


        case ARGI_NUMBER:   /* Number */

            /* Need an operand of type ACPI_TYPE_NUMBER */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if (ACPI_TYPE_NUMBER != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Number, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_STRING:

            /* Need an operand of type ACPI_TYPE_STRING or ACPI_TYPE_BUFFER */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if ((ACPI_TYPE_STRING != (*StackPtr)->Common.Type) &&
                (ACPI_TYPE_BUFFER != (*StackPtr)->Common.Type))
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed String or Buffer, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_BUFFER:

            /* Need an operand of type ACPI_TYPE_BUFFER */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if (ACPI_TYPE_BUFFER != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Buffer, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_MUTEX:

            /* Need an operand of type ACPI_TYPE_MUTEX */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if (ACPI_TYPE_MUTEX != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Mutex, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_EVENT:

            /* Need an operand of type ACPI_TYPE_EVENT */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if (ACPI_TYPE_EVENT != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed AcpiEvent, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_REGION:

            /* Need an operand of type ACPI_TYPE_REGION */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if (ACPI_TYPE_REGION != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Region, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


         case ARGI_IF:   /* If */

            /* Need an operand of type INTERNAL_TYPE_IF */

            if (INTERNAL_TYPE_IF != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed If, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_PACKAGE:   /* Package */

            /* Need an operand of type ACPI_TYPE_PACKAGE */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            if (ACPI_TYPE_PACKAGE != (*StackPtr)->Common.Type)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Package, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        case ARGI_ANYTYPE:


            /*
             * We don't want to resolve IndexOp reference objects during
             * a store because this would be an implicit DeRefOf operation.
             * Instead, we just want to store the reference object.
             */

            if ((Opcode == AML_STORE_OP) &&
                ((*StackPtr)->Common.Type == INTERNAL_TYPE_REFERENCE) &&
                ((*StackPtr)->Reference.OpCode == AML_INDEX_OP))
            {
                break;
            }

            /* All others must be resolved */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            /* All types OK, so we don't perform any typechecks */

            break;


        case ARGI_DATAOBJECT:
            /*
             * ARGI_DATAOBJECT is only used by the SizeOf operator.
             *
             * The ACPI specification allows SizeOf to return the size of
             *  a Buffer, String or Package.  However, the MS ACPI.SYS AML
             *  Interpreter also allows an Node reference to return without
             *  error with a size of 4.
             */

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            /* Need a buffer, string, package or Node reference */

            if (((*StackPtr)->Common.Type != ACPI_TYPE_BUFFER) &&
                ((*StackPtr)->Common.Type != ACPI_TYPE_STRING) &&
                ((*StackPtr)->Common.Type != ACPI_TYPE_PACKAGE) &&
                ((*StackPtr)->Common.Type != INTERNAL_TYPE_REFERENCE))
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Buf/Str/Pkg, found %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }

            /*
             * If this is a reference, only allow a reference to an Node.
             */
            if ((*StackPtr)->Common.Type == INTERNAL_TYPE_REFERENCE)
            {
                if (!(*StackPtr)->Reference.Node)
                {
                    DEBUG_PRINT (ACPI_INFO,
                        ("AmlResolveOperands: Needed Node reference, found %s Obj=%p\n",
                        AcpiCmGetTypeName (ObjectType), *StackPtr));
                    Status = AE_AML_OPERAND_TYPE;
                    goto Cleanup;
                }
            }

            break;


        case ARGI_COMPLEXOBJ:

            Status = AcpiAmlResolveToValue (StackPtr, WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

            /* Need a buffer or package */

            if (((*StackPtr)->Common.Type != ACPI_TYPE_BUFFER) &&
                ((*StackPtr)->Common.Type != ACPI_TYPE_PACKAGE))
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("AmlResolveOperands: Needed Package, Buf/Pkg %s Obj=%p\n",
                    AcpiCmGetTypeName (ObjectType), *StackPtr));
                Status = AE_AML_OPERAND_TYPE;
                goto Cleanup;
            }
            break;


        /* Unknown abbreviation passed in */

        default:
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlResolveOperands: Internal error - Unknown arg type %X\n",
                ThisArgType));
            Status = AE_BAD_PARAMETER;
            goto Cleanup;

        }   /* switch (*Types++) */


        /*
         * If more operands needed, decrement StackPtr to point
         * to next operand on stack (after checking for underflow).
         */
        if (GET_CURRENT_ARG_TYPE (ArgTypes))
        {
            StackPtr--;
        }

    }   /* while (*Types) */


Cleanup:

  return_ACPI_STATUS (Status);
}


