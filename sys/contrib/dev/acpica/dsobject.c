/******************************************************************************
 *
 * Module Name: dsobject - Dispatcher object management routines
 *              $Revision: 117 $
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

#define __DSOBJECT_C__

#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsobject")


#ifndef ACPI_NO_METHOD_EXECUTION
/*****************************************************************************
 *
 * FUNCTION:    AcpiDsBuildInternalObject
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Op              - Parser object to be translated
 *              ObjDescPtr      - Where the ACPI internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a parser Op object to the equivalent namespace object
 *              Simple objects are any objects other than a package object!
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsBuildInternalObject (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("DsBuildInternalObject");


    *ObjDescPtr = NULL;
    if (Op->Common.AmlOpcode == AML_INT_NAMEPATH_OP)
    {
        /*
         * This is an named object reference.  If this name was
         * previously looked up in the namespace, it was stored in this op.
         * Otherwise, go ahead and look it up now
         */
        if (!Op->Common.Node)
        {
            Status = AcpiNsLookup (WalkState->ScopeInfo, Op->Common.Value.String,
                            ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
                            ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE, NULL,
                            (ACPI_NAMESPACE_NODE **) &(Op->Common.Node));

            if (ACPI_FAILURE (Status))
            {
                ACPI_REPORT_NSERROR (Op->Common.Value.String, Status);
                return_ACPI_STATUS (Status);
            }
        }
    }

    /* Create and init the internal ACPI object */

    ObjDesc = AcpiUtCreateInternalObject ((AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode))->ObjectType);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitObjectFromOp (WalkState, Op, Op->Common.AmlOpcode, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ObjDesc);
        return_ACPI_STATUS (Status);
    }

    *ObjDescPtr = ObjDesc;
    return_ACPI_STATUS (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsBuildInternalBufferObj
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Op              - Parser object to be translated
 *              BufferLength    - Length of the buffer
 *              ObjDescPtr      - Where the ACPI internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a parser Op package object to the equivalent
 *              namespace object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsBuildInternalBufferObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  BufferLength,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *ByteList;
    UINT32                  ByteListLength = 0;


    ACPI_FUNCTION_TRACE ("DsBuildInternalBufferObj");


    ObjDesc = *ObjDescPtr;
    if (ObjDesc)
    {
        /*
         * We are evaluating a Named buffer object "Name (xxxx, Buffer)".
         * The buffer object already exists (from the NS node)
         */
    }
    else
    {
        /* Create a new buffer object */

        ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_BUFFER);
        *ObjDescPtr = ObjDesc;
        if (!ObjDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }
    }

    /*
     * Second arg is the buffer data (optional) ByteList can be either
     * individual bytes or a string initializer.  In either case, a
     * ByteList appears in the AML.
     */
    Arg = Op->Common.Value.Arg;         /* skip first arg */

    ByteList = Arg->Named.Next;
    if (ByteList)
    {
        if (ByteList->Common.AmlOpcode != AML_INT_BYTELIST_OP)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Expecting bytelist, got AML opcode %X in op %p\n",
                ByteList->Common.AmlOpcode, ByteList));

            AcpiUtRemoveReference (ObjDesc);
            return (AE_TYPE);
        }

        ByteListLength = (UINT32) ByteList->Common.Value.Integer;
    }

    /*
     * The buffer length (number of bytes) will be the larger of:
     * 1) The specified buffer length and
     * 2) The length of the initializer byte list
     */
    ObjDesc->Buffer.Length = BufferLength;
    if (ByteListLength > BufferLength)
    {
        ObjDesc->Buffer.Length = ByteListLength;
    }

    /* Allocate the buffer */

    if (ObjDesc->Buffer.Length == 0)
    {
        ObjDesc->Buffer.Pointer = NULL;
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "Buffer defined with zero length in AML, creating\n"));
    }
    else
    {
        ObjDesc->Buffer.Pointer = ACPI_MEM_CALLOCATE (
                                        ObjDesc->Buffer.Length);
        if (!ObjDesc->Buffer.Pointer)
        {
            AcpiUtDeleteObjectDesc (ObjDesc);
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Initialize buffer from the ByteList (if present) */

        if (ByteList)
        {
            ACPI_MEMCPY (ObjDesc->Buffer.Pointer, ByteList->Named.Data,
                         ByteListLength);
        }
    }

    ObjDesc->Buffer.Flags |= AOPOBJ_DATA_VALID;
    Op->Common.Node = (ACPI_NAMESPACE_NODE *) ObjDesc;
    return_ACPI_STATUS (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsBuildInternalPackageObj
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Op              - Parser object to be translated
 *              PackageLength   - Number of elements in the package
 *              ObjDescPtr      - Where the ACPI internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a parser Op package object to the equivalent
 *              namespace object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsBuildInternalPackageObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  PackageLength,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_PARSE_OBJECT       *Parent;
    ACPI_OPERAND_OBJECT     *ObjDesc = NULL;
    UINT32                  PackageListLength;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  i;


    ACPI_FUNCTION_TRACE ("DsBuildInternalPackageObj");


    /* Find the parent of a possibly nested package */

    Parent = Op->Common.Parent;
    while ((Parent->Common.AmlOpcode == AML_PACKAGE_OP)     ||
           (Parent->Common.AmlOpcode == AML_VAR_PACKAGE_OP))
    {
        Parent = Parent->Common.Parent;
    }

    ObjDesc = *ObjDescPtr;
    if (ObjDesc)
    {
        /*
         * We are evaluating a Named package object "Name (xxxx, Package)".
         * Get the existing package object from the NS node
         */
    }
    else
    {
        ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_PACKAGE);
        *ObjDescPtr = ObjDesc;
        if (!ObjDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        ObjDesc->Package.Node = Parent->Common.Node;
    }

    ObjDesc->Package.Count = PackageLength;

    /* Count the number of items in the package list */

    PackageListLength = 0;
    Arg = Op->Common.Value.Arg;
    Arg = Arg->Common.Next;
    while (Arg)
    {
        PackageListLength++;
        Arg = Arg->Common.Next;
    }

    /*
     * The package length (number of elements) will be the greater
     * of the specified length and the length of the initializer list
     */
    if (PackageListLength > PackageLength)
    {
        ObjDesc->Package.Count = PackageListLength;
    }

    /*
     * Allocate the pointer array (array of pointers to the
     * individual objects). Add an extra pointer slot so
     * that the list is always null terminated.
     */
    ObjDesc->Package.Elements = ACPI_MEM_CALLOCATE (
                ((ACPI_SIZE) ObjDesc->Package.Count + 1) * sizeof (void *));

    if (!ObjDesc->Package.Elements)
    {
        AcpiUtDeleteObjectDesc (ObjDesc);
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /*
     * Now init the elements of the package
     */
    i = 0;
    Arg = Op->Common.Value.Arg;
    Arg = Arg->Common.Next;
    while (Arg)
    {
        if (Arg->Common.AmlOpcode == AML_INT_RETURN_VALUE_OP)
        {
            /* Object (package or buffer) is already built */

            ObjDesc->Package.Elements[i] = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, Arg->Common.Node);
        }
        else
        {
            Status = AcpiDsBuildInternalObject (WalkState, Arg,
                                        &ObjDesc->Package.Elements[i]);
        }

        i++;
        Arg = Arg->Common.Next;
    }

    ObjDesc->Package.Flags |= AOPOBJ_DATA_VALID;
    Op->Common.Node = (ACPI_NAMESPACE_NODE *) ObjDesc;
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsCreateNode
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Node            - NS Node to be initialized
 *              Op              - Parser object to be translated
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create the object to be associated with a namespace node
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsCreateNode (
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE_PTR ("DsCreateNode", Op);


    /*
     * Because of the execution pass through the non-control-method
     * parts of the table, we can arrive here twice.  Only init
     * the named object node the first time through
     */
    if (AcpiNsGetAttachedObject (Node))
    {
        return_ACPI_STATUS (AE_OK);
    }

    if (!Op->Common.Value.Arg)
    {
        /* No arguments, there is nothing to do */

        return_ACPI_STATUS (AE_OK);
    }

    /* Build an internal object for the argument(s) */

    Status = AcpiDsBuildInternalObject (WalkState, Op->Common.Value.Arg, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Re-type the object according to its argument */

    Node->Type = ACPI_GET_OBJECT_TYPE (ObjDesc);

    /* Attach obj to node */

    Status = AcpiNsAttachObject (Node, ObjDesc, Node->Type);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}

#endif /* ACPI_NO_METHOD_EXECUTION */


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsInitObjectFromOp
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Op              - Parser op used to init the internal object
 *              Opcode          - AML opcode associated with the object
 *              RetObjDesc      - Namespace object to be initialized
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize a namespace object from a parser Op and its
 *              associated arguments.  The namespace object is a more compact
 *              representation of the Op and its arguments.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsInitObjectFromOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT16                  Opcode,
    ACPI_OPERAND_OBJECT     **RetObjDesc)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("DsInitObjectFromOp");


    ObjDesc = *RetObjDesc;
    OpInfo = AcpiPsGetOpcodeInfo (Opcode);
    if (OpInfo->Class == AML_CLASS_UNKNOWN)
    {
        /* Unknown opcode */

        return_ACPI_STATUS (AE_TYPE);
    }

    /* Perform per-object initialization */

    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_BUFFER:

        /*
         * Defer evaluation of Buffer TermArg operand
         */
        ObjDesc->Buffer.Node      = (ACPI_NAMESPACE_NODE *) WalkState->Operands[0];
        ObjDesc->Buffer.AmlStart  = Op->Named.Data;
        ObjDesc->Buffer.AmlLength = Op->Named.Length;
        break;


    case ACPI_TYPE_PACKAGE:

        /*
         * Defer evaluation of Package TermArg operand
         */
        ObjDesc->Package.Node      = (ACPI_NAMESPACE_NODE *) WalkState->Operands[0];
        ObjDesc->Package.AmlStart  = Op->Named.Data;
        ObjDesc->Package.AmlLength = Op->Named.Length;
        break;


    case ACPI_TYPE_INTEGER:

        switch (OpInfo->Type)
        {
        case AML_TYPE_CONSTANT:
            /*
             * Resolve AML Constants here - AND ONLY HERE!
             * All constants are integers.
             * We mark the integer with a flag that indicates that it started life
             * as a constant -- so that stores to constants will perform as expected (noop).
             * (ZeroOp is used as a placeholder for optional target operands.)
             */
            ObjDesc->Common.Flags = AOPOBJ_AML_CONSTANT;

            switch (Opcode)
            {
            case AML_ZERO_OP:

                ObjDesc->Integer.Value = 0;
                break;

            case AML_ONE_OP:

                ObjDesc->Integer.Value = 1;
                break;

            case AML_ONES_OP:

                ObjDesc->Integer.Value = ACPI_INTEGER_MAX;

                /* Truncate value if we are executing from a 32-bit ACPI table */

#ifndef ACPI_NO_METHOD_EXECUTION
                AcpiExTruncateFor32bitTable (ObjDesc);
#endif
                break;

            case AML_REVISION_OP:

                ObjDesc->Integer.Value = ACPI_CA_SUPPORT_LEVEL;
                break;

            default:

                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown constant opcode %X\n", Opcode));
                Status = AE_AML_OPERAND_TYPE;
                break;
            }
            break;


        case AML_TYPE_LITERAL:

            ObjDesc->Integer.Value = Op->Common.Value.Integer;
            break;


        default:
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown Integer type %X\n", OpInfo->Type));
            Status = AE_AML_OPERAND_TYPE;
            break;
        }
        break;


    case ACPI_TYPE_STRING:

        ObjDesc->String.Pointer = Op->Common.Value.String;
        ObjDesc->String.Length = (UINT32) ACPI_STRLEN (Op->Common.Value.String);

        /*
         * The string is contained in the ACPI table, don't ever try
         * to delete it
         */
        ObjDesc->Common.Flags |= AOPOBJ_STATIC_POINTER;
        break;


    case ACPI_TYPE_METHOD:
        break;


    case ACPI_TYPE_LOCAL_REFERENCE:

        switch (OpInfo->Type)
        {
        case AML_TYPE_LOCAL_VARIABLE:

            /* Split the opcode into a base opcode + offset */

            ObjDesc->Reference.Opcode = AML_LOCAL_OP;
            ObjDesc->Reference.Offset = Opcode - AML_LOCAL_OP;

#ifndef ACPI_NO_METHOD_EXECUTION
            Status = AcpiDsMethodDataGetNode (AML_LOCAL_OP, ObjDesc->Reference.Offset,
                        WalkState, (ACPI_NAMESPACE_NODE **) &ObjDesc->Reference.Object);
#endif
            break;


        case AML_TYPE_METHOD_ARGUMENT:

            /* Split the opcode into a base opcode + offset */

            ObjDesc->Reference.Opcode = AML_ARG_OP;
            ObjDesc->Reference.Offset = Opcode - AML_ARG_OP;
            break;

        default: /* Other literals, etc.. */

            if (Op->Common.AmlOpcode == AML_INT_NAMEPATH_OP)
            {
                /* Node was saved in Op */

                ObjDesc->Reference.Node = Op->Common.Node;
            }

            ObjDesc->Reference.Opcode = Opcode;
            break;
        }
        break;


    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unimplemented data type: %X\n",
            ACPI_GET_OBJECT_TYPE (ObjDesc)));

        Status = AE_AML_OPERAND_TYPE;
        break;
    }

    return_ACPI_STATUS (Status);
}


