/******************************************************************************
 *
 * Module Name: excreate - Named object creation
 *              $Revision: 100 $
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

#define __EXCREATE_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acevents.h"
#include "actables.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("excreate")


#ifndef ACPI_NO_METHOD_EXECUTION
/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateAlias
 *
 * PARAMETERS:  WalkState            - Current state, contains operands
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new named alias
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateAlias (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_NAMESPACE_NODE     *TargetNode;
    ACPI_NAMESPACE_NODE     *AliasNode;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("ExCreateAlias");


    /* Get the source/alias operands (both namespace nodes) */

    AliasNode =  (ACPI_NAMESPACE_NODE *) WalkState->Operands[0];
    TargetNode = (ACPI_NAMESPACE_NODE *) WalkState->Operands[1];

    if (TargetNode->Type == ACPI_TYPE_LOCAL_ALIAS)
    {
        /*
         * Dereference an existing alias so that we don't create a chain
         * of aliases.  With this code, we guarantee that an alias is
         * always exactly one level of indirection away from the
         * actual aliased name.
         */
        TargetNode = (ACPI_NAMESPACE_NODE *) TargetNode->Object;
    }

    /*
     * For objects that can never change (i.e., the NS node will
     * permanently point to the same object), we can simply attach
     * the object to the new NS node.  For other objects (such as
     * Integers, buffers, etc.), we have to point the Alias node
     * to the original Node.
     */
    switch (TargetNode->Type)
    {
    case ACPI_TYPE_INTEGER:
    case ACPI_TYPE_STRING:
    case ACPI_TYPE_BUFFER:
    case ACPI_TYPE_PACKAGE:
    case ACPI_TYPE_BUFFER_FIELD:

        /*
         * The new alias has the type ALIAS and points to the original
         * NS node, not the object itself.  This is because for these
         * types, the object can change dynamically via a Store.
         */
        AliasNode->Type = ACPI_TYPE_LOCAL_ALIAS;
        AliasNode->Object = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, TargetNode);
        break;

    default:

        /* Attach the original source object to the new Alias Node */

        /*
         * The new alias assumes the type of the target, and it points
         * to the same object.  The reference count of the object has an
         * additional reference to prevent deletion out from under either the
         * target node or the alias Node
         */
        Status = AcpiNsAttachObject (AliasNode,
                                AcpiNsGetAttachedObject (TargetNode),
                                TargetNode->Type);
        break;
    }

    /* Since both operands are Nodes, we don't need to delete them */

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateEvent
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new event object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateEvent (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE ("ExCreateEvent");


    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_EVENT);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Create the actual OS semaphore, with zero initial units -- meaning
     * that the event is created in an unsignalled state
     */
    Status = AcpiOsCreateSemaphore (ACPI_NO_UNIT_LIMIT, 0,
                                    &ObjDesc->Event.Semaphore);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Attach object to the Node */

    Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) WalkState->Operands[0],
                                    ObjDesc, ACPI_TYPE_EVENT);

Cleanup:
    /*
     * Remove local reference to the object (on error, will cause deletion
     * of both object and semaphore if present.)
     */
    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateMutex
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new mutex object
 *
 *              Mutex (Name[0], SyncLevel[1])
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateMutex (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE_PTR ("ExCreateMutex", ACPI_WALK_OPERANDS);


    /* Create the new mutex object */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_MUTEX);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Create the actual OS semaphore.
     * One unit max to make it a mutex, with one initial unit to allow
     * the mutex to be acquired.
     */
    Status = AcpiOsCreateSemaphore (1, 1, &ObjDesc->Mutex.Semaphore);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Init object and attach to NS node */

    ObjDesc->Mutex.SyncLevel = (UINT8) WalkState->Operands[1]->Integer.Value;
    ObjDesc->Mutex.Node = (ACPI_NAMESPACE_NODE *) WalkState->Operands[0];

    Status = AcpiNsAttachObject (ObjDesc->Mutex.Node,
                ObjDesc, ACPI_TYPE_MUTEX);


Cleanup:
    /*
     * Remove local reference to the object (on error, will cause deletion
     * of both object and semaphore if present.)
     */
    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateRegion
 *
 * PARAMETERS:  AmlStart            - Pointer to the region declaration AML
 *              AmlLength           - Max length of the declaration AML
 *              Operands            - List of operands for the opcode
 *              WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new operation region object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateRegion (
    UINT8                   *AmlStart,
    UINT32                  AmlLength,
    UINT8                   RegionSpace,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *RegionObj2;


    ACPI_FUNCTION_TRACE ("ExCreateRegion");


    /* Get the Namespace Node */

    Node = WalkState->Op->Common.Node;

    /*
     * If the region object is already attached to this node,
     * just return
     */
    if (AcpiNsGetAttachedObject (Node))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Space ID must be one of the predefined IDs, or in the user-defined
     * range
     */
    if ((RegionSpace >= ACPI_NUM_PREDEFINED_REGIONS) &&
        (RegionSpace < ACPI_USER_REGION_BEGIN))
    {
        ACPI_REPORT_ERROR (("Invalid AddressSpace type %X\n", RegionSpace));
        return_ACPI_STATUS (AE_AML_INVALID_SPACE_ID);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "Region Type - %s (%X)\n",
                    AcpiUtGetRegionName (RegionSpace), RegionSpace));

    /* Create the region descriptor */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_REGION);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Remember location in AML stream of address & length
     * operands since they need to be evaluated at run time.
     */
    RegionObj2                  = ObjDesc->Common.NextObject;
    RegionObj2->Extra.AmlStart  = AmlStart;
    RegionObj2->Extra.AmlLength = AmlLength;

    /* Init the region from the operands */

    ObjDesc->Region.SpaceId = RegionSpace;
    ObjDesc->Region.Address = 0;
    ObjDesc->Region.Length  = 0;
    ObjDesc->Region.Node    = Node;

    /* Install the new region object in the parent Node */

    Status = AcpiNsAttachObject (Node, ObjDesc, ACPI_TYPE_REGION);


Cleanup:

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateTableRegion
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new DataTableRegion object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateTableRegion (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_TABLE_HEADER       *Table;
    ACPI_OPERAND_OBJECT     *RegionObj2;


    ACPI_FUNCTION_TRACE ("ExCreateTableRegion");


    /* Get the Node from the object stack  */

    Node = WalkState->Op->Common.Node;

    /*
     * If the region object is already attached to this node,
     * just return
     */
    if (AcpiNsGetAttachedObject (Node))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Find the ACPI table */

    Status = AcpiTbFindTable (Operand[1]->String.Pointer,
                              Operand[2]->String.Pointer,
                              Operand[3]->String.Pointer, &Table);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Create the region descriptor */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_REGION);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    RegionObj2                      = ObjDesc->Common.NextObject;
    RegionObj2->Extra.RegionContext = NULL;

    /* Init the region from the operands */

    ObjDesc->Region.SpaceId = REGION_DATA_TABLE;
    ObjDesc->Region.Address = (ACPI_PHYSICAL_ADDRESS) ACPI_TO_INTEGER (Table);
    ObjDesc->Region.Length  = Table->Length;
    ObjDesc->Region.Node    = Node;
    ObjDesc->Region.Flags   = AOPOBJ_DATA_VALID;

    /* Install the new region object in the parent Node */

    Status = AcpiNsAttachObject (Node, ObjDesc, ACPI_TYPE_REGION);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    Status = AcpiEvInitializeRegion (ObjDesc, FALSE);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_EXIST)
        {
            Status = AE_OK;
        }
        else
        {
            goto Cleanup;
        }
    }

    ObjDesc->Region.Flags |= AOPOBJ_SETUP_COMPLETE;


Cleanup:

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateProcessor
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new processor object and populate the fields
 *
 *              Processor (Name[0], CpuID[1], PblockAddr[2], PblockLength[3])
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateProcessor (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("ExCreateProcessor", WalkState);


    /* Create the processor object */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_PROCESSOR);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /*
     * Initialize the processor object from the operands
     */
    ObjDesc->Processor.ProcId  = (UINT8)           Operand[1]->Integer.Value;
    ObjDesc->Processor.Address = (ACPI_IO_ADDRESS) Operand[2]->Integer.Value;
    ObjDesc->Processor.Length  = (UINT8)           Operand[3]->Integer.Value;

    /* Install the processor object in the parent Node */

    Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) Operand[0],
                    ObjDesc, ACPI_TYPE_PROCESSOR);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreatePowerResource
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new PowerResource object and populate the fields
 *
 *              PowerResource (Name[0], SystemLevel[1], ResourceOrder[2])
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreatePowerResource (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE_PTR ("ExCreatePowerResource", WalkState);


    /* Create the power resource object */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_POWER);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Initialize the power object from the operands */

    ObjDesc->PowerResource.SystemLevel   = (UINT8)  Operand[1]->Integer.Value;
    ObjDesc->PowerResource.ResourceOrder = (UINT16) Operand[2]->Integer.Value;

    /* Install the  power resource object in the parent Node */

    Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) Operand[0],
                    ObjDesc, ACPI_TYPE_POWER);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}

#endif

/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateMethod
 *
 * PARAMETERS:  AmlStart        - First byte of the method's AML
 *              AmlLength       - AML byte count for this method
 *              WalkState       - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new method object
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExCreateMethod (
    UINT8                   *AmlStart,
    UINT32                  AmlLength,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    UINT8                   MethodFlags;


    ACPI_FUNCTION_TRACE_PTR ("ExCreateMethod", WalkState);


    /* Create a new method object */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_METHOD);
    if (!ObjDesc)
    {
       return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Save the method's AML pointer and length  */

    ObjDesc->Method.AmlStart  = AmlStart;
    ObjDesc->Method.AmlLength = AmlLength;

    /* disassemble the method flags */

    MethodFlags = (UINT8) Operand[1]->Integer.Value;

    ObjDesc->Method.MethodFlags = MethodFlags;
    ObjDesc->Method.ParamCount  = (UINT8) (MethodFlags & METHOD_FLAGS_ARG_COUNT);

    /*
     * Get the concurrency count.  If required, a semaphore will be
     * created for this method when it is parsed.
     */
    if (MethodFlags & METHOD_FLAGS_SERIALIZED)
    {
        /*
         * ACPI 1.0: Concurrency = 1
         * ACPI 2.0: Concurrency = (SyncLevel (in method declaration) + 1)
         */
        ObjDesc->Method.Concurrency = (UINT8)
                        (((MethodFlags & METHOD_FLAGS_SYNCH_LEVEL) >> 4) + 1);
    }
    else
    {
        ObjDesc->Method.Concurrency = INFINITE_CONCURRENCY;
    }

    /* Attach the new object to the method Node */

    Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) Operand[0],
                    ObjDesc, ACPI_TYPE_METHOD);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);

    /* Remove a reference to the operand */

    AcpiUtRemoveReference (Operand[1]);
    return_ACPI_STATUS (Status);
}


