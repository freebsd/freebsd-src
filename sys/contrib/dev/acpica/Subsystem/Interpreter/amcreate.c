/******************************************************************************
 *
 * Module Name: amcreate - Named object creation
 *              $Revision: 50 $
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

#define __AMCREATE_C__

#include "acpi.h"
#include "acparser.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acevents.h"
#include "acdispat.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amcreate")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateField
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *              Operands            - List of operands for the opcode
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute CreateField operators: CreateBitFieldOp,
 *              CreateByteFieldOp, CreateWordFieldOp, CreateDWordFieldOp,
 *              CreateFieldOp (which define fields in buffers)
 *
 * ALLOCATION:  Deletes CreateFieldOp's count operand descriptor
 *
 *
 *  ACPI SPECIFICATION REFERENCES:
 *  DefCreateBitField   :=  CreateBitFieldOp    SrcBuf  BitIdx    NameString
 *  DefCreateByteField  :=  CreateByteFieldOp   SrcBuf  ByteIdx   NameString
 *  DefCreateDWordField :=  CreateDWordFieldOp  SrcBuf  ByteIdx   NameString
 *  DefCreateField      :=  CreateFieldOp       SrcBuf  BitIdx    NumBits     NameString
 *  DefCreateWordField  :=  CreateWordFieldOp   SrcBuf  ByteIdx   NameString
 *  BitIndex            :=  TermArg=>Integer
 *  ByteIndex           :=  TermArg=>Integer
 *  NumBits             :=  TermArg=>Integer
 *  SourceBuff          :=  TermArg=>Buffer
 *
 ******************************************************************************/


ACPI_STATUS
AcpiAmlExecCreateField (
    UINT8                   *AmlPtr,
    UINT32                  AmlLength,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *TmpDesc;


    FUNCTION_TRACE ("AmlExecCreateField");


    /* Create the region descriptor */

    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_FIELD_UNIT);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Construct the field object */

    ObjDesc->FieldUnit.Access       = (UINT8) ACCESS_ANY_ACC;
    ObjDesc->FieldUnit.LockRule     = (UINT8) GLOCK_NEVER_LOCK;
    ObjDesc->FieldUnit.UpdateRule   = (UINT8) UPDATE_PRESERVE;

    /*
     * Allocate a method object for this field unit
     */

    ObjDesc->FieldUnit.Extra = AcpiCmCreateInternalObject (
                                        INTERNAL_TYPE_EXTRA);
    if (!ObjDesc->FieldUnit.Extra)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Remember location in AML stream of the field unit
     * opcode and operands -- since the buffer and index
     * operands must be evaluated.
     */

    ObjDesc->FieldUnit.Extra->Extra.Pcode       = AmlPtr;
    ObjDesc->FieldUnit.Extra->Extra.PcodeLength = AmlLength;
    ObjDesc->FieldUnit.Node = Node;


    /*
     * This operation is supposed to cause the destination Name to refer
     * to the defined FieldUnit -- it must not store the constructed
     * FieldUnit object (or its current value) in some location that the
     * Name may already be pointing to.  So, if the Name currently contains
     * a reference which would cause AcpiAmlExecStore() to perform an indirect
     * store rather than setting the value of the Name itself, clobber that
     * reference before calling AcpiAmlExecStore().
     */

    /* Type of Name's existing value */

    switch (AcpiNsGetType (Node))
    {

    case ACPI_TYPE_FIELD_UNIT:

    case INTERNAL_TYPE_ALIAS:
    case INTERNAL_TYPE_BANK_FIELD:
    case INTERNAL_TYPE_DEF_FIELD:
    case INTERNAL_TYPE_INDEX_FIELD:

        TmpDesc = AcpiNsGetAttachedObject (Node);
        if (TmpDesc)
        {
            /*
             * There is an existing object here;  delete it and zero out the
             * object field within the Node
             */

            DUMP_PATHNAME (Node,
                "AmlExecCreateField: Removing Current Reference",
                TRACE_BFIELD, _COMPONENT);

            DUMP_ENTRY (Node, TRACE_BFIELD);
            DUMP_STACK_ENTRY (TmpDesc);

            AcpiCmRemoveReference (TmpDesc);
            AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) Node, NULL,
                                    ACPI_TYPE_ANY);
        }

        /* Set the type to ANY (or the store below will fail) */

        ((ACPI_NAMESPACE_NODE *) Node)->Type = ACPI_TYPE_ANY;

        break;


    default:

        break;
    }


    /* Store constructed field descriptor in result location */

    Status = AcpiAmlExecStore (ObjDesc, (ACPI_OPERAND_OBJECT *) Node, WalkState);

    /*
     * If the field descriptor was not physically stored (or if a failure
     * above), we must delete it
     */
    if (ObjDesc->Common.ReferenceCount <= 1)
    {
        AcpiCmRemoveReference (ObjDesc);
    }


    return_ACPI_STATUS (AE_OK);


Cleanup:

    /* Delete region object and method subobject */

    if (ObjDesc)
    {
        /* Remove deletes both objects! */

        AcpiCmRemoveReference (ObjDesc);
        ObjDesc = NULL;
    }

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateAlias
 *
 * PARAMETERS:  Operands            - List of operands for the opcode
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new named alias
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreateAlias (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_NAMESPACE_NODE     *SourceNode;
    ACPI_NAMESPACE_NODE     *AliasNode;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AmlExecCreateAlias");


    /* Get the source/alias operands (both NTEs) */

    Status = AcpiDsObjStackPopObject ((ACPI_OPERAND_OBJECT  **) &SourceNode,
                                        WalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Don't pop it, it gets removed in the calling routine
     */

    AliasNode = AcpiDsObjStackGetValue (0, WalkState);

    /* Add an additional reference to the object */

    AcpiCmAddReference (SourceNode->Object);

    /*
     * Attach the original source Node to the new Alias Node.
     */
    Status = AcpiNsAttachObject (AliasNode, SourceNode->Object,
                                    SourceNode->Type);


    /*
     * The new alias assumes the type of the source, but it points
     * to the same object.  The reference count of the object has two
     * additional references to prevent deletion out from under either the
     * source or the alias Node
     */

    /* Since both operands are NTEs, we don't need to delete them */

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateEvent
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new event object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreateEvent (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE ("AmlExecCreateEvent");


 BREAKPOINT3;

    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_EVENT);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Create the actual OS semaphore */

    /* TBD: [Investigate] should be created with 0 or 1 units? */

    Status = AcpiOsCreateSemaphore (ACPI_NO_UNIT_LIMIT, 1,
                                    &ObjDesc->Event.Semaphore);
    if (ACPI_FAILURE (Status))
    {
        AcpiCmRemoveReference (ObjDesc);
        goto Cleanup;
    }

    /* Attach object to the Node */

    Status = AcpiNsAttachObject (AcpiDsObjStackGetValue (0, WalkState),
                                    ObjDesc, (UINT8) ACPI_TYPE_EVENT);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsDeleteSemaphore (ObjDesc->Event.Semaphore);
        AcpiCmRemoveReference (ObjDesc);
        goto Cleanup;
    }


Cleanup:

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateMutex
 *
 * PARAMETERS:  InterpreterMode     - Current running mode (load1/Load2/Exec)
 *              Operands            - List of operands for the opcode
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new mutex object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreateMutex (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *SyncDesc;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE_PTR ("AmlExecCreateMutex", WALK_OPERANDS);


    /* Get the operand */

    Status = AcpiDsObjStackPopObject (&SyncDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Attempt to allocate a new object */

    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_MUTEX);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Create the actual OS semaphore */

    Status = AcpiOsCreateSemaphore (1, 1, &ObjDesc->Mutex.Semaphore);
    if (ACPI_FAILURE (Status))
    {
        AcpiCmRemoveReference (ObjDesc);
        goto Cleanup;
    }

    ObjDesc->Mutex.SyncLevel = (UINT8) SyncDesc->Number.Value;

    /* ObjDesc was on the stack top, and the name is below it */

    Status = AcpiNsAttachObject (AcpiDsObjStackGetValue (0, WalkState),
                                ObjDesc, (UINT8) ACPI_TYPE_MUTEX);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsDeleteSemaphore (ObjDesc->Mutex.Semaphore);
        AcpiCmRemoveReference (ObjDesc);
        goto Cleanup;
    }


Cleanup:

    /* Always delete the operand */

    AcpiCmRemoveReference (SyncDesc);

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateRegion
 *
 * PARAMETERS:  AmlPtr              - Pointer to the region declaration AML
 *              AmlLength           - Max length of the declaration AML
 *              Operands            - List of operands for the opcode
 *              InterpreterMode     - Load1/Load2/Execute
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new operation region object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreateRegion (
    UINT8                   *AmlPtr,
    UINT32                  AmlLength,
    UINT32                  RegionSpace,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;


    FUNCTION_TRACE ("AmlExecCreateRegion");


    if (RegionSpace >= NUM_REGION_TYPES)
    {
        /* TBD: [Future] In ACPI 2.0, valid region space
         *  includes types 0-6 (Adding CMOS and PCIBARTarget).
         *  Also, types 0x80-0xff are defined as "OEM Region
         *  Space handler"
         *
         * Should this return an error, or should we just keep
         * going?  How do we handle the OEM region handlers?
         */
        REPORT_WARNING (("Invalid AddressSpace type %X\n", RegionSpace));
    }

    DEBUG_PRINT (TRACE_LOAD, ("AmlDoNode: Region Type [%s]\n",
                    AcpiGbl_RegionTypes[RegionSpace]));


    /* Get the Node from the object stack  */

    Node = (ACPI_NAMESPACE_NODE *) AcpiDsObjStackGetValue (0, WalkState);

    /* Create the region descriptor */

    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_REGION);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Allocate a method object for this region.
     */

    ObjDesc->Region.Extra =  AcpiCmCreateInternalObject (
                                        INTERNAL_TYPE_EXTRA);
    if (!ObjDesc->Region.Extra)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Remember location in AML stream of address & length
     * operands since they need to be evaluated at run time.
     */

    ObjDesc->Region.Extra->Extra.Pcode       = AmlPtr;
    ObjDesc->Region.Extra->Extra.PcodeLength = AmlLength;

    /* Init the region from the operands */

    ObjDesc->Region.SpaceId       = (UINT8) RegionSpace;
    ObjDesc->Region.Address       = 0;
    ObjDesc->Region.Length        = 0;


    /* Install the new region object in the parent Node */

    ObjDesc->Region.Node = Node;

    Status = AcpiNsAttachObject (Node, ObjDesc,
                                (UINT8) ACPI_TYPE_REGION);

    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /*
     * If we have a valid region, initialize it
     * Namespace is NOT locked at this point.
     */

    Status = AcpiEvInitializeRegion (ObjDesc, FALSE);

    if (ACPI_FAILURE (Status))
    {
        /*
         *  If AE_NOT_EXIST is returned, it is not fatal
         *  because many regions get created before a handler
         *  is installed for said region.
         */
        if (AE_NOT_EXIST == Status)
        {
            Status = AE_OK;
        }
    }

Cleanup:

    if (ACPI_FAILURE (Status))
    {
        /* Delete region object and method subobject */

        if (ObjDesc)
        {
            /* Remove deletes both objects! */

            AcpiCmRemoveReference (ObjDesc);
            ObjDesc = NULL;
        }
    }

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateProcessor
 *
 * PARAMETERS:  Op              - Op containing the Processor definition and
 *                                args
 *              ProcessorNTE    - Node for the containing Node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new processor object and populate the fields
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreateProcessor (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_HANDLE             ProcessorNTE)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE_PTR ("AmlExecCreateProcessor", Op);


    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_PROCESSOR);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        return_ACPI_STATUS (Status);
    }

    /* Install the new processor object in the parent Node */

    Status = AcpiNsAttachObject (ProcessorNTE, ObjDesc,
                                    (UINT8) ACPI_TYPE_PROCESSOR);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS(Status);
    }

    Arg = Op->Value.Arg;

    /* check existence */

    if (!Arg)
    {
        Status = AE_AML_NO_OPERAND;
        return_ACPI_STATUS (Status);
    }

    /* First arg is the Processor ID */

    ObjDesc->Processor.ProcId = (UINT8) Arg->Value.Integer;

    /* Move to next arg and check existence */

    Arg = Arg->Next;
    if (!Arg)
    {
        Status = AE_AML_NO_OPERAND;
        return_ACPI_STATUS (Status);
    }

    /* Second arg is the PBlock Address */

    ObjDesc->Processor.Address = (ACPI_IO_ADDRESS) Arg->Value.Integer;

    /* Move to next arg and check existence */

    Arg = Arg->Next;
    if (!Arg)
    {
        Status = AE_AML_NO_OPERAND;
        return_ACPI_STATUS (Status);
    }

    /* Third arg is the PBlock Length */

    ObjDesc->Processor.Length = (UINT8) Arg->Value.Integer;

    return_ACPI_STATUS (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreatePowerResource
 *
 * PARAMETERS:  Op              - Op containing the PowerResource definition
 *                                and args
 *              PowerResNTE     - Node for the containing Node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new PowerResource object and populate the fields
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreatePowerResource (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_HANDLE             PowerResNTE)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE_PTR ("AmlExecCreatePowerResource", Op);


    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_POWER);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        return_ACPI_STATUS (Status);
    }

    /* Install the new power resource object in the parent Node */

    Status = AcpiNsAttachObject (PowerResNTE, ObjDesc,
                                (UINT8) ACPI_TYPE_POWER);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS(Status);
    }

    Arg = Op->Value.Arg;

    /* check existence */

    if (!Arg)
    {
        Status = AE_AML_NO_OPERAND;
        return_ACPI_STATUS (Status);
    }

    /* First arg is the SystemLevel */

    ObjDesc->PowerResource.SystemLevel = (UINT8) Arg->Value.Integer;

    /* Move to next arg and check existence */

    Arg = Arg->Next;
    if (!Arg)
    {
        Status = AE_AML_NO_OPERAND;
        return_ACPI_STATUS (Status);
    }

    /* Second arg is the PBlock Address */

    ObjDesc->PowerResource.ResourceOrder = (UINT16) Arg->Value.Integer;

    return_ACPI_STATUS (AE_OK);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecCreateMethod
 *
 * PARAMETERS:  AmlPtr          - First byte of the method's AML
 *              AmlLength       - AML byte count for this method
 *              MethodFlags     - AML method flag byte
 *              Method          - Method Node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new method object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecCreateMethod (
    UINT8                   *AmlPtr,
    UINT32                  AmlLength,
    UINT32                  MethodFlags,
    ACPI_HANDLE             Method)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("AmlExecCreateMethod", Method);


    /* Create a new method object */

    ObjDesc = AcpiCmCreateInternalObject (ACPI_TYPE_METHOD);
    if (!ObjDesc)
    {
       return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Get the method's AML pointer/length from the Op */

    ObjDesc->Method.Pcode       = AmlPtr;
    ObjDesc->Method.PcodeLength = AmlLength;

    /*
     * First argument is the Method Flags (contains parameter count for the
     * method)
     */

    ObjDesc->Method.MethodFlags = (UINT8) MethodFlags;
    ObjDesc->Method.ParamCount  = (UINT8) (MethodFlags &
                                            METHOD_FLAGS_ARG_COUNT);

    /*
     * Get the concurrency count.  If required, a semaphore will be
     * created for this method when it is parsed.
     *
     * TBD: [Future]  for APCI 2.0, there will be a SyncLevel value, not
     * just a flag
     * Concurrency = SyncLevel + 1;.
     */

    if (MethodFlags & METHOD_FLAGS_SERIALIZED)
    {
        ObjDesc->Method.Concurrency = 1;
    }

    else
    {
        ObjDesc->Method.Concurrency = INFINITE_CONCURRENCY;
    }

    /* Attach the new object to the method Node */

    Status = AcpiNsAttachObject (Method, ObjDesc, (UINT8) ACPI_TYPE_METHOD);
    if (ACPI_FAILURE (Status))
    {
        AcpiCmDeleteObjectDesc (ObjDesc);
    }

    return_ACPI_STATUS (Status);
}


