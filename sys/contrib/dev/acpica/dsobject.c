/******************************************************************************
 *
 * Module Name: dsobject - Dispatcher object management routines
 *              $Revision: 90 $
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

#define __DSOBJECT_C__

#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsobject")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsInitOneObject
 *
 * PARAMETERS:  ObjHandle       - Node
 *              Level           - Current nesting level
 *              Context         - Points to a init info struct
 *              ReturnValue     - Not used
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Callback from AcpiWalkNamespace.  Invoked for every object
 *              within the  namespace.
 *
 *              Currently, the only objects that require initialization are:
 *              1) Methods
 *              2) Op Regions
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsInitOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OBJECT_TYPE        Type;
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     *Info = (ACPI_INIT_WALK_INFO *) Context;
    UINT8                   TableRevision;


    ACPI_FUNCTION_NAME ("DsInitOneObject");


    Info->ObjectCount++;
    TableRevision = Info->TableDesc->Pointer->Revision;

    /*
     * We are only interested in objects owned by the table that
     * was just loaded
     */
    if (((ACPI_NAMESPACE_NODE *) ObjHandle)->OwnerId !=
            Info->TableDesc->TableId)
    {
        return (AE_OK);
    }


    /* And even then, we are only interested in a few object types */

    Type = AcpiNsGetType (ObjHandle);

    switch (Type)
    {
    case ACPI_TYPE_REGION:

        AcpiDsInitializeRegion (ObjHandle);

        Info->OpRegionCount++;
        break;


    case ACPI_TYPE_METHOD:

        Info->MethodCount++;

        if (!(AcpiDbgLevel & ACPI_LV_INIT))
        {
            ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "."));
        }

        /*
         * Set the execution data width (32 or 64) based upon the
         * revision number of the parent ACPI table.
         */
        if (TableRevision == 1)
        {
            ((ACPI_NAMESPACE_NODE *)ObjHandle)->Flags |= ANOBJ_DATA_WIDTH_32;
        }

        /*
         * Always parse methods to detect errors, we may delete
         * the parse tree below
         */
        Status = AcpiDsParseMethod (ObjHandle);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Method %p [%4.4s] - parse failure, %s\n",
                ObjHandle, (char *) &((ACPI_NAMESPACE_NODE *) ObjHandle)->Name,
                AcpiFormatException (Status)));

            /* This parse failed, but we will continue parsing more methods */

            break;
        }

        /*
         * Delete the parse tree.  We simple re-parse the method
         * for every execution since there isn't much overhead
         */
        AcpiNsDeleteNamespaceSubtree (ObjHandle);
        AcpiNsDeleteNamespaceByOwner (((ACPI_NAMESPACE_NODE *) ObjHandle)->Object->Method.OwningId);
        break;

    default:
        break;
    }

    /*
     * We ignore errors from above, and always return OK, since
     * we don't want to abort the walk on a single error.
     */
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsInitializeObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk the entire namespace and perform any necessary
 *              initialization on the objects found therein
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsInitializeObjects (
    ACPI_TABLE_DESC         *TableDesc,
    ACPI_NAMESPACE_NODE     *StartNode)
{
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     Info;


    ACPI_FUNCTION_TRACE ("DsInitializeObjects");


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "**** Starting initialization of namespace objects ****\n"));
    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "Parsing Methods:"));


    Info.MethodCount    = 0;
    Info.OpRegionCount  = 0;
    Info.ObjectCount    = 0;
    Info.TableDesc      = TableDesc;


    /* Walk entire namespace from the supplied root */

    Status = AcpiWalkNamespace (ACPI_TYPE_ANY, StartNode, ACPI_UINT32_MAX,
                    AcpiDsInitOneObject, &Info, NULL);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "WalkNamespace failed! %x\n", Status));
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
        "\n%d Control Methods found and parsed (%d nodes total)\n",
        Info.MethodCount, Info.ObjectCount));
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "%d Control Methods found\n", Info.MethodCount));
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "%d Op Regions found\n", Info.OpRegionCount));

    return_ACPI_STATUS (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsInitObjectFromOp
 *
 * PARAMETERS:  Op              - Parser op used to init the internal object
 *              Opcode          - AML opcode associated with the object
 *              ObjDesc         - Namespace object to be initialized
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
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_PARSE2_OBJECT      *ByteList;
    ACPI_OPERAND_OBJECT     *ArgDesc;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_NAME ("DsInitObjectFromOp");


    ObjDesc = *RetObjDesc;
    OpInfo = AcpiPsGetOpcodeInfo (Opcode);
    if (OpInfo->Class == AML_CLASS_UNKNOWN)
    {
        /* Unknown opcode */

        return (AE_TYPE);
    }


    /* Get and prepare the first argument */

    switch (ObjDesc->Common.Type)
    {
    case ACPI_TYPE_BUFFER:

        ObjDesc->Buffer.Node = (ACPI_NAMESPACE_NODE *) WalkState->Operands[0];

        /* First arg is a number */

        AcpiDsCreateOperand (WalkState, Op->Value.Arg, 0);
        ArgDesc = WalkState->Operands [WalkState->NumOperands - 1];
        AcpiDsObjStackPop (1, WalkState);

        /* Resolve the object (could be an arg or local) */

        Status = AcpiExResolveToValue (&ArgDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            AcpiUtRemoveReference (ArgDesc);
            return (Status);
        }

        /* We are expecting a number */

        if (ArgDesc->Common.Type != ACPI_TYPE_INTEGER)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Expecting number, got obj: %p type %X\n",
                ArgDesc, ArgDesc->Common.Type));
            AcpiUtRemoveReference (ArgDesc);
            return (AE_TYPE);
        }

        /* Get the value, delete the internal object */

        ObjDesc->Buffer.Length = (UINT32) ArgDesc->Integer.Value;
        AcpiUtRemoveReference (ArgDesc);

        /* Allocate the buffer */

        if (ObjDesc->Buffer.Length == 0)
        {
            ObjDesc->Buffer.Pointer = NULL;
            ACPI_REPORT_WARNING (("Buffer created with zero length in AML\n"));
            break;
        }

        else
        {
            ObjDesc->Buffer.Pointer = ACPI_MEM_CALLOCATE (
                                            ObjDesc->Buffer.Length);

            if (!ObjDesc->Buffer.Pointer)
            {
                return (AE_NO_MEMORY);
            }
        }

        /*
         * Second arg is the buffer data (optional) ByteList can be either
         * individual bytes or a string initializer.
         */
        Arg = Op->Value.Arg;         /* skip first arg */

        ByteList = (ACPI_PARSE2_OBJECT *) Arg->Next;
        if (ByteList)
        {
            if (ByteList->Opcode != AML_INT_BYTELIST_OP)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Expecting bytelist, got: %p\n",
                    ByteList));
                return (AE_TYPE);
            }

            ACPI_MEMCPY (ObjDesc->Buffer.Pointer, ByteList->Data,
                         ObjDesc->Buffer.Length);
        }

        break;


    case ACPI_TYPE_PACKAGE:

        /*
         * When called, an internal package object has already been built and
         * is pointed to by ObjDesc.  AcpiDsBuildInternalObject builds another
         * internal package object, so remove reference to the original so
         * that it is deleted.  Error checking is done within the remove
         * reference function.
         */
        AcpiUtRemoveReference (ObjDesc);
        Status = AcpiDsBuildInternalObject (WalkState, Op, RetObjDesc);
        break;

    case ACPI_TYPE_INTEGER:
        ObjDesc->Integer.Value = Op->Value.Integer;
        break;


    case ACPI_TYPE_STRING:
        ObjDesc->String.Pointer = Op->Value.String;
        ObjDesc->String.Length = ACPI_STRLEN (Op->Value.String);

        /*
         * The string is contained in the ACPI table, don't ever try
         * to delete it
         */
        ObjDesc->Common.Flags |= AOPOBJ_STATIC_POINTER;
        break;


    case ACPI_TYPE_METHOD:
        break;


    case INTERNAL_TYPE_REFERENCE:

        switch (OpInfo->Type)
        {
        case AML_TYPE_LOCAL_VARIABLE:

            /* Split the opcode into a base opcode + offset */

            ObjDesc->Reference.Opcode = AML_LOCAL_OP;
            ObjDesc->Reference.Offset = Opcode - AML_LOCAL_OP;
            break;


        case AML_TYPE_METHOD_ARGUMENT:

            /* Split the opcode into a base opcode + offset */

            ObjDesc->Reference.Opcode = AML_ARG_OP;
            ObjDesc->Reference.Offset = Opcode - AML_ARG_OP;
            break;


        default: /* Constants, Literals, etc.. */

            if (Op->Opcode == AML_INT_NAMEPATH_OP)
            {
                /* Node was saved in Op */

                ObjDesc->Reference.Node = Op->Node;
            }

            ObjDesc->Reference.Opcode = Opcode;
            break;
        }

        break;


    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unimplemented data type: %x\n",
            ObjDesc->Common.Type));

        break;
    }

    return (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsBuildInternalSimpleObj
 *
 * PARAMETERS:  Op              - Parser object to be translated
 *              ObjDescPtr      - Where the ACPI internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a parser Op object to the equivalent namespace object
 *              Simple objects are any objects other than a package object!
 *
 ****************************************************************************/

static ACPI_STATUS
AcpiDsBuildInternalSimpleObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    char                    *Name;


    ACPI_FUNCTION_TRACE ("DsBuildInternalSimpleObj");


    if (Op->Opcode == AML_INT_NAMEPATH_OP)
    {
        /*
         * This is an object reference.  If this name was
         * previously looked up in the namespace, it was stored in this op.
         * Otherwise, go ahead and look it up now
         */
        if (!Op->Node)
        {
            Status = AcpiNsLookup (WalkState->ScopeInfo, Op->Value.String,
                            ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
                            ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE, NULL,
                            (ACPI_NAMESPACE_NODE **) &(Op->Node));

            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_NOT_FOUND)
                {
                    Name = NULL;
                    AcpiNsExternalizeName (ACPI_UINT32_MAX, Op->Value.String, NULL, &Name);

                    if (Name)
                    {
                        ACPI_REPORT_WARNING (("Reference %s at AML %X not found\n",
                                    Name, Op->AmlOffset));
                        ACPI_MEM_FREE (Name);
                    }

                    else
                    {
                        ACPI_REPORT_WARNING (("Reference %s at AML %X not found\n",
                                   Op->Value.String, Op->AmlOffset));
                    }

                    *ObjDescPtr = NULL;
                }

                else
                {
                    return_ACPI_STATUS (Status);
                }
            }
        }
    }

    /* Create and init the internal ACPI object */

    ObjDesc = AcpiUtCreateInternalObject ((AcpiPsGetOpcodeInfo (Op->Opcode))->ObjectType);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitObjectFromOp (WalkState, Op, Op->Opcode, &ObjDesc);
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
 * FUNCTION:    AcpiDsBuildInternalPackageObj
 *
 * PARAMETERS:  Op              - Parser object to be translated
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
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("DsBuildInternalPackageObj");


    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_PACKAGE);
    *ObjDescPtr = ObjDesc;
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    if (Op->Opcode == AML_VAR_PACKAGE_OP)
    {
        /*
         * Variable length package parameters are evaluated JIT
         */
        return_ACPI_STATUS (AE_OK);
    }

    /* The first argument must be the package length */

    Arg = Op->Value.Arg;
    ObjDesc->Package.Count = Arg->Value.Integer32;

    /*
     * Allocate the array of pointers (ptrs to the
     * individual objects) Add an extra pointer slot so
     * that the list is always null terminated.
     */
    ObjDesc->Package.Elements = ACPI_MEM_CALLOCATE (
                            (ObjDesc->Package.Count + 1) * sizeof (void *));

    if (!ObjDesc->Package.Elements)
    {
        AcpiUtDeleteObjectDesc (ObjDesc);
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    ObjDesc->Package.NextElement = ObjDesc->Package.Elements;

    /*
     * Now init the elements of the package
     */
    Arg = Arg->Next;
    while (Arg)
    {
        if (Arg->Opcode == AML_PACKAGE_OP)
        {
            Status = AcpiDsBuildInternalPackageObj (WalkState, Arg,
                                        ObjDesc->Package.NextElement);
        }

        else
        {
            Status = AcpiDsBuildInternalSimpleObj (WalkState, Arg,
                                        ObjDesc->Package.NextElement);
        }

        ObjDesc->Package.NextElement++;
        Arg = Arg->Next;
    }

    ObjDesc->Package.Flags |= AOPOBJ_DATA_VALID;
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsBuildInternalObject
 *
 * PARAMETERS:  Op              - Parser object to be translated
 *              ObjDescPtr      - Where the ACPI internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a parser Op object to the equivalent namespace
 *              object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsBuildInternalObject (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_STATUS             Status;


    switch (Op->Opcode)
    {
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP:

        Status = AcpiDsBuildInternalPackageObj (WalkState, Op, ObjDescPtr);
        break;


    default:

        Status = AcpiDsBuildInternalSimpleObj (WalkState, Op, ObjDescPtr);
        break;
    }

    return (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsCreateNode
 *
 * PARAMETERS:  Op              - Parser object to be translated
 *              ObjDescPtr      - Where the ACPI internal object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION:
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

    if (!Op->Value.Arg)
    {
        /* No arguments, there is nothing to do */

        return_ACPI_STATUS (AE_OK);
    }

    /* Build an internal object for the argument(s) */

    Status = AcpiDsBuildInternalObject (WalkState, Op->Value.Arg, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Re-type the object according to it's argument */

    Node->Type = ObjDesc->Common.Type;

    /* Init obj */

    Status = AcpiNsAttachObject (Node, ObjDesc, Node->Type);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


