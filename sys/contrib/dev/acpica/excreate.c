/******************************************************************************
 *
 * Module Name: excreate - Named object creation
 *              $Revision: 79 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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
#include "acparser.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acevents.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("excreate")


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateAlias
 *
 * PARAMETERS:  WalkState            - Current state, contains List of
 *                                      operands for the opcode
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
    ACPI_NAMESPACE_NODE     *SourceNode;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("ExCreateAlias");


    /* Get the source/alias operands (both namespace nodes) */

    SourceNode = (ACPI_NAMESPACE_NODE *) WalkState->Operands[1];


    /* Attach the original source object to the new Alias Node */

    Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) WalkState->Operands[0],
                                    AcpiNsGetAttachedObject (SourceNode),
                                    SourceNode->Type);

    /*
     * The new alias assumes the type of the source, but it points
     * to the same object.  The reference count of the object has an
     * additional reference to prevent deletion out from under either the
     * source or the alias Node
     */

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


    FUNCTION_TRACE ("ExCreateEvent");


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
                                    ObjDesc, (UINT8) ACPI_TYPE_EVENT);

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


    FUNCTION_TRACE_PTR ("ExCreateMutex", WALK_OPERANDS);


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

    Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) WalkState->Operands[0],
                                ObjDesc, (UINT8) ACPI_TYPE_MUTEX);


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
    ACPI_OPERAND_OBJECT     *RegionObj2 = NULL;


    FUNCTION_TRACE ("ExCreateRegion");


    /* Get the Node from the object stack  */

    Node = WalkState->Op->Node;

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
        REPORT_ERROR (("Invalid AddressSpace type %X\n", RegionSpace));
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

    Status = AcpiNsAttachObject (Node, ObjDesc, (UINT8) ACPI_TYPE_REGION);


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
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("ExCreateTableRegion");

/*
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_REGION);
    if (!ObjDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }


Cleanup:
*/

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateProcessor
 *
 * PARAMETERS:  Op              - Op containing the Processor definition and
 *                                args
 *              ProcessorNode   - Parent Node for the processor object
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


    FUNCTION_TRACE_PTR ("ExCreateProcessor", WalkState);


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
                    ObjDesc, (UINT8) ACPI_TYPE_PROCESSOR);


    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreatePowerResource
 *
 * PARAMETERS:  Op              - Op containing the PowerResource definition
 *                                and args
 *              PowerNode       - Parent Node for the power object
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


    FUNCTION_TRACE_PTR ("ExCreatePowerResource", WalkState);


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
                    ObjDesc, (UINT8) ACPI_TYPE_POWER);


    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExCreateMethod
 *
 * PARAMETERS:  AmlStart        - First byte of the method's AML
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
AcpiExCreateMethod (
    UINT8                   *AmlStart,
    UINT32                  AmlLength,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    UINT8                   MethodFlags;


    FUNCTION_TRACE_PTR ("ExCreateMethod", WalkState);


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
                    ObjDesc, (UINT8) ACPI_TYPE_METHOD);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);

    /* Remove a reference to the operand */

    AcpiUtRemoveReference (Operand[1]);
    return_ACPI_STATUS (Status);
}


