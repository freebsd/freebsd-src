/******************************************************************************
 *
 * Module Name: dsobject - Dispatcher object management routines
 *              $Revision: 48 $
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

#define __DSOBJECT_C__

#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"

#define _COMPONENT          DISPATCHER
        MODULE_NAME         ("dsobject")


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
    OBJECT_TYPE_INTERNAL    Type;
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     *Info = (ACPI_INIT_WALK_INFO *) Context;


    Info->ObjectCount++;

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

        DEBUG_PRINT_RAW (ACPI_OK, ("."));


        /*
         * Always parse methods to detect errors, we may delete
         * the parse tree below
         */

        Status = AcpiDsParseMethod (ObjHandle);

        /* TBD: [Errors] what do we do with an error? */

        if (ACPI_FAILURE (Status))
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("DsInitOneObject: Method %p [%4.4s] parse failed! %s\n",
                ObjHandle, &((ACPI_NAMESPACE_NODE *)ObjHandle)->Name,
                AcpiCmFormatException (Status)));
            break;
        }

        /*
         * Keep the parse tree only if we are parsing all methods
         * at init time (versus just-in-time)
         */

        if (AcpiGbl_WhenToParseMethods != METHOD_PARSE_AT_INIT)
        {
            AcpiNsDeleteNamespaceSubtree (ObjHandle);
        }

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


    FUNCTION_TRACE ("DsInitializeObjects");


    DEBUG_PRINT (TRACE_DISPATCH,
        ("DsInitializeObjects: **** Starting initialization of namespace objects ****\n"));
    DEBUG_PRINT_RAW (ACPI_OK, ("Parsing Methods:"));


    Info.MethodCount = 0;
    Info.OpRegionCount = 0;
    Info.ObjectCount = 0;
    Info.TableDesc = TableDesc;


    /* Walk entire namespace from the supplied root */

    Status = AcpiWalkNamespace (ACPI_TYPE_ANY, StartNode,
                                ACPI_UINT32_MAX, AcpiDsInitOneObject,
                                &Info, NULL);
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("DsInitializeObjects: WalkNamespace failed! %x\n", Status));
    }

    DEBUG_PRINT_RAW (ACPI_OK,
        ("\n%d Control Methods found and parsed (%d nodes total)\n",
        Info.MethodCount, Info.ObjectCount));
    DEBUG_PRINT (TRACE_DISPATCH,
        ("DsInitializeObjects: %d Control Methods found\n", Info.MethodCount));
    DEBUG_PRINT (TRACE_DISPATCH,
        ("DsInitializeObjects: %d Op Regions found\n", Info.OpRegionCount));

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
    ACPI_OPERAND_OBJECT     **ObjDesc)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_PARSE2_OBJECT      *ByteList;
    ACPI_OPERAND_OBJECT     *ArgDesc;
    ACPI_OPCODE_INFO        *OpInfo;


    OpInfo = AcpiPsGetOpcodeInfo (Opcode);
    if (ACPI_GET_OP_TYPE (OpInfo) != ACPI_OP_TYPE_OPCODE)
    {
        /* Unknown opcode */

        return (AE_TYPE);
    }


    /* Get and prepare the first argument */

    switch ((*ObjDesc)->Common.Type)
    {
    case ACPI_TYPE_BUFFER:

        /* First arg is a number */

        AcpiDsCreateOperand (WalkState, Op->Value.Arg);
        ArgDesc = WalkState->Operands [WalkState->NumOperands - 1];
        AcpiDsObjStackPop (1, WalkState);

        /* Resolve the object (could be an arg or local) */

        Status = AcpiAmlResolveToValue (&ArgDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            AcpiCmRemoveReference (ArgDesc);
            return (Status);
        }

        /* We are expecting a number */

        if (ArgDesc->Common.Type != ACPI_TYPE_NUMBER)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("InitObject: Expecting number, got obj: %p type %X\n",
                ArgDesc, ArgDesc->Common.Type));
            AcpiCmRemoveReference (ArgDesc);
            return (AE_TYPE);
        }

        /* Get the value, delete the internal object */

        (*ObjDesc)->Buffer.Length = (UINT32) ArgDesc->Number.Value;
        AcpiCmRemoveReference (ArgDesc);

        /* Allocate the buffer */

        if ((*ObjDesc)->Buffer.Length == 0)
        {
            (*ObjDesc)->Buffer.Pointer = NULL;
            REPORT_WARNING (("Buffer created with zero length in AML\n"));
            break;
        }

        else
        {
            (*ObjDesc)->Buffer.Pointer =
                            AcpiCmCallocate ((*ObjDesc)->Buffer.Length);

            if (!(*ObjDesc)->Buffer.Pointer)
            {
                return (AE_NO_MEMORY);
            }
        }

        /*
         * Second arg is the buffer data (optional)
         * ByteList can be either individual bytes or a
         * string initializer!
         */

        /* skip first arg */
        Arg = Op->Value.Arg;
        ByteList = (ACPI_PARSE2_OBJECT *) Arg->Next;
        if (ByteList)
        {
            if (ByteList->Opcode != AML_BYTELIST_OP)
            {
                DEBUG_PRINT (ACPI_ERROR,
                    ("InitObject: Expecting bytelist, got: %x\n",
                    ByteList));
                return (AE_TYPE);
            }

            MEMCPY ((*ObjDesc)->Buffer.Pointer, ByteList->Data,
                    (*ObjDesc)->Buffer.Length);
        }

        break;


    case ACPI_TYPE_PACKAGE:

        /*
         * When called, an internal package object has already
         *  been built and is pointed to by *ObjDesc.
         *  AcpiDsBuildInternalObject build another internal
         *  package object, so remove reference to the original
         *  so that it is deleted.  Error checking is done
         *  within the remove reference function.
         */
        AcpiCmRemoveReference(*ObjDesc);

        Status = AcpiDsBuildInternalObject (WalkState, Op, ObjDesc);
        break;

    case ACPI_TYPE_NUMBER:
        (*ObjDesc)->Number.Value = Op->Value.Integer;
        break;


    case ACPI_TYPE_STRING:
        (*ObjDesc)->String.Pointer = Op->Value.String;
        (*ObjDesc)->String.Length = STRLEN (Op->Value.String);
        break;


    case ACPI_TYPE_METHOD:
        break;


    case INTERNAL_TYPE_REFERENCE:

        switch (ACPI_GET_OP_CLASS (OpInfo))
        {
        case OPTYPE_LOCAL_VARIABLE:

            /* Split the opcode into a base opcode + offset */

            (*ObjDesc)->Reference.OpCode = AML_LOCAL_OP;
            (*ObjDesc)->Reference.Offset = Opcode - AML_LOCAL_OP;
            break;

        case OPTYPE_METHOD_ARGUMENT:

            /* Split the opcode into a base opcode + offset */

            (*ObjDesc)->Reference.OpCode = AML_ARG_OP;
            (*ObjDesc)->Reference.Offset = Opcode - AML_ARG_OP;
            break;

        default: /* Constants, Literals, etc.. */

            if (Op->Opcode == AML_NAMEPATH_OP)
            {
                /* Node was saved in Op */

                (*ObjDesc)->Reference.Node = Op->Node;
            }

            (*ObjDesc)->Reference.OpCode = Opcode;
            break;
        }

        break;


    default:

        DEBUG_PRINT (ACPI_ERROR,
            ("InitObject: Unimplemented data type: %x\n",
            (*ObjDesc)->Common.Type));

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

ACPI_STATUS
AcpiDsBuildInternalSimpleObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    OBJECT_TYPE_INTERNAL    Type;
    ACPI_STATUS             Status;
    UINT32                  Length;
    char                    *Name;


    FUNCTION_TRACE ("DsBuildInternalSimpleObj");


    if (Op->Opcode == AML_NAMEPATH_OP)
    {
        /*
         * This is an object reference.  If The name was
         * previously looked up in the NS, it is stored in this op.
         * Otherwise, go ahead and look it up now
         */

        if (!Op->Node)
        {
            Status = AcpiNsLookup (WalkState->ScopeInfo,
                            Op->Value.String, ACPI_TYPE_ANY,
                            IMODE_EXECUTE,
                            NS_SEARCH_PARENT | NS_DONT_OPEN_SCOPE,
                            NULL,
                            (ACPI_NAMESPACE_NODE **)&(Op->Node));

            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_NOT_FOUND)
                {
                    Name = NULL;
                    AcpiNsExternalizeName (ACPI_UINT32_MAX, Op->Value.String, &Length, &Name);

                    if (Name)
                    {
                        REPORT_WARNING (("Reference %s AML 0x%X not found\n",
                                    Name, Op->AmlOffset));
                        AcpiCmFree (Name);
                    }
                    else
                    {
                        REPORT_WARNING (("Reference %s AML 0x%X not found\n",
                                   Op->Value.String, Op->AmlOffset));
                    }
                    *ObjDescPtr = NULL;
                    return_ACPI_STATUS (AE_OK);
                }

                return_ACPI_STATUS (Status);
            }
        }

        /*
         * The reference will be a Reference
         * TBD: [Restructure] unless we really need a separate
         *  type of INTERNAL_TYPE_REFERENCE change
         *  AcpiDsMapOpcodeToDataType to handle this case
         */
        Type = INTERNAL_TYPE_REFERENCE;
    }
    else
    {
        Type = AcpiDsMapOpcodeToDataType (Op->Opcode, NULL);
    }


    /* Create and init the internal ACPI object */

    ObjDesc = AcpiCmCreateInternalObject (Type);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitObjectFromOp (WalkState, Op,
                                        Op->Opcode, &ObjDesc);

    if (ACPI_FAILURE (Status))
    {
        AcpiCmRemoveReference (ObjDesc);
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


    FUNCTION_TRACE ("DsBuildInternalPackageObj");


    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_PACKAGE);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* The first argument must be the package length */

    Arg = Op->Value.Arg;
    ObjDesc->Package.Count = Arg->Value.Integer;

    /*
     * Allocate the array of pointers (ptrs to the
     * individual objects) Add an extra pointer slot so
     * that the list is always null terminated.
     */

    ObjDesc->Package.Elements =
                AcpiCmCallocate ((ObjDesc->Package.Count + 1) *
                sizeof (void *));

    if (!ObjDesc->Package.Elements)
    {
        /* Package vector allocation failure   */

        REPORT_ERROR (("DsBuildInternalPackageObj: Package vector allocation failure\n"));

        AcpiCmDeleteObjectDesc (ObjDesc);
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

    *ObjDescPtr = ObjDesc;
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


    if (Op->Opcode == AML_PACKAGE_OP)
    {
        Status = AcpiDsBuildInternalPackageObj (WalkState, Op,
                                                ObjDescPtr);
    }

    else
    {
        Status = AcpiDsBuildInternalSimpleObj (WalkState, Op,
                                                ObjDescPtr);
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


    FUNCTION_TRACE_PTR ("DsCreateNode", Op);


    if (!Op->Value.Arg)
    {
        /* No arguments, there is nothing to do */

        return_ACPI_STATUS (AE_OK);
    }


    /* Build an internal object for the argument(s) */

    Status = AcpiDsBuildInternalObject (WalkState,
                                        Op->Value.Arg, &ObjDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /* Re-type the object according to it's argument */

    Node->Type = ObjDesc->Common.Type;

    /* Init obj */

    Status = AcpiNsAttachObject ((ACPI_HANDLE) Node, ObjDesc,
                                    (UINT8) Node->Type);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    return_ACPI_STATUS (Status);


Cleanup:

    AcpiCmRemoveReference (ObjDesc);

    return_ACPI_STATUS (Status);
}


