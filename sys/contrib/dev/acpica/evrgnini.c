/******************************************************************************
 *
 * Module Name: evrgnini- ACPI AddressSpace (OpRegion) init
 *              $Revision: 66 $
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


#define __EVRGNINI_C__

#include "acpi.h"
#include "acevents.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evrgnini")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvSystemMemoryRegionSetup
 *
 * PARAMETERS:  RegionObj           - Region we are interested in
 *              Function            - Start or stop
 *              HandlerContext      - Address space handler context
 *              RegionContext       - Region specific context
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Do any prep work for region handling, a nop for now
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvSystemMemoryRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext)
{
    ACPI_OPERAND_OBJECT     *RegionDesc = (ACPI_OPERAND_OBJECT *) Handle;
    ACPI_MEM_SPACE_CONTEXT  *LocalRegionContext;


    ACPI_FUNCTION_TRACE ("EvSystemMemoryRegionSetup");


    if (Function == ACPI_REGION_DEACTIVATE)
    {
        if (*RegionContext)
        {
            ACPI_MEM_FREE (*RegionContext);
            *RegionContext = NULL;
        }
        return_ACPI_STATUS (AE_OK);
    }

    /* Create a new context */

    LocalRegionContext = ACPI_MEM_CALLOCATE (sizeof (ACPI_MEM_SPACE_CONTEXT));
    if (!(LocalRegionContext))
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Save the region length and address for use in the handler */

    LocalRegionContext->Length  = RegionDesc->Region.Length;
    LocalRegionContext->Address = RegionDesc->Region.Address;

    *RegionContext = LocalRegionContext;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvIoSpaceRegionSetup
 *
 * PARAMETERS:  RegionObj           - Region we are interested in
 *              Function            - Start or stop
 *              HandlerContext      - Address space handler context
 *              RegionContext       - Region specific context
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Do any prep work for region handling
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvIoSpaceRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext)
{
    ACPI_FUNCTION_TRACE ("EvIoSpaceRegionSetup");


    if (Function == ACPI_REGION_DEACTIVATE)
    {
        *RegionContext = NULL;
    }
    else
    {
        *RegionContext = HandlerContext;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvPciConfigRegionSetup
 *
 * PARAMETERS:  RegionObj           - Region we are interested in
 *              Function            - Start or stop
 *              HandlerContext      - Address space handler context
 *              RegionContext       - Region specific context
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Do any prep work for region handling
 *
 * MUTEX:       Assumes namespace is not locked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvPciConfigRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_INTEGER            Temp;
    ACPI_PCI_ID             *PciId = *RegionContext;
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *RegionObj = (ACPI_OPERAND_OBJECT  *) Handle;
    ACPI_DEVICE_ID          ObjectHID;


    ACPI_FUNCTION_TRACE ("EvPciConfigRegionSetup");


    HandlerObj = RegionObj->Region.AddrHandler;
    if (!HandlerObj)
    {
        /*
         * No installed handler. This shouldn't happen because the dispatch
         * routine checks before we get here, but we check again just in case.
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
            "Attempting to init a region %p, with no handler\n", RegionObj));
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    if (Function == ACPI_REGION_DEACTIVATE)
    {
        if (PciId)
        {
            ACPI_MEM_FREE (PciId);
            *RegionContext = NULL;
        }

        return_ACPI_STATUS (Status);
    }

    /* Create a new context */

    PciId = ACPI_MEM_CALLOCATE (sizeof (ACPI_PCI_ID));
    if (!PciId)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /*
     * For PCI Config space access, we have to pass the segment, bus,
     * device and function numbers.  This routine must acquire those.
     */

    /*
     * First get device and function numbers from the _ADR object
     * in the parent's scope.
     */
    Node = AcpiNsGetParentNode (RegionObj->Region.Node);

    /* Evaluate the _ADR object */

    Status = AcpiUtEvaluateNumericObject (METHOD_NAME__ADR, Node, &Temp);

    /*
     * The default is zero, and since the allocation above zeroed
     * the data, just do nothing on failure.
     */
    if (ACPI_SUCCESS (Status))
    {
        PciId->Device   = ACPI_HIWORD (ACPI_LODWORD (Temp));
        PciId->Function = ACPI_LOWORD (ACPI_LODWORD (Temp));
    }

    /*
     * Get the _SEG and _BBN values from the device upon which the handler
     * is installed.
     *
     * We need to get the _SEG and _BBN objects relative to the PCI BUS device.
     * This is the device the handler has been registered to handle.
     */

    /*
     * If the AddrHandler.Node is still pointing to the root, we need
     * to scan upward for a PCI Root bridge and re-associate the OpRegion
     * handlers with that device.
     */
    if (HandlerObj->AddrHandler.Node == AcpiGbl_RootNode)
    {
        /*
         * Node is currently the parent object
         */
        while (Node != AcpiGbl_RootNode)
        {
            Status = AcpiUtExecute_HID (Node, &ObjectHID);
            if (ACPI_SUCCESS (Status))
            {
                /* Got a valid _HID, check if this is a PCI root */

                if (!(ACPI_STRNCMP (ObjectHID.Buffer, PCI_ROOT_HID_STRING,
                                    sizeof (PCI_ROOT_HID_STRING))))
                {
                    /* Install a handler for this PCI root bridge */

                    Status = AcpiInstallAddressSpaceHandler ((ACPI_HANDLE) Node,
                                        ACPI_ADR_SPACE_PCI_CONFIG,
                                        ACPI_DEFAULT_HANDLER, NULL, NULL);
                    if (ACPI_FAILURE (Status))
                    {
                        ACPI_REPORT_ERROR (("Could not install PciConfig handler for %4.4s, %s\n",
                            Node->Name.Ascii, AcpiFormatException (Status)));
                    }
                    break;
                }
            }

            Node = AcpiNsGetParentNode (Node);
        }
    }
    else
    {
        Node = HandlerObj->AddrHandler.Node;
    }

    /*
     * The PCI segment number comes from the _SEG method
     */
    Status = AcpiUtEvaluateNumericObject (METHOD_NAME__SEG, Node, &Temp);
    if (ACPI_SUCCESS (Status))
    {
        PciId->Segment = ACPI_LOWORD (Temp);
    }

    /*
     * The PCI bus number comes from the _BBN method
     */
    Status = AcpiUtEvaluateNumericObject (METHOD_NAME__BBN, Node, &Temp);
    if (ACPI_SUCCESS (Status))
    {
        PciId->Bus = ACPI_LOWORD (Temp);
    }

    /*
     * Complete this device's PciId
     */
    AcpiOsDerivePciId (Node, RegionObj->Region.Node, &PciId);

    *RegionContext = PciId;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvPciBarRegionSetup
 *
 * PARAMETERS:  RegionObj           - Region we are interested in
 *              Function            - Start or stop
 *              HandlerContext      - Address space handler context
 *              RegionContext       - Region specific context
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Do any prep work for region handling
 *
 * MUTEX:       Assumes namespace is not locked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvPciBarRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext)
{
    ACPI_FUNCTION_TRACE ("EvPciBarRegionSetup");


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvCmosRegionSetup
 *
 * PARAMETERS:  RegionObj           - Region we are interested in
 *              Function            - Start or stop
 *              HandlerContext      - Address space handler context
 *              RegionContext       - Region specific context
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Do any prep work for region handling
 *
 * MUTEX:       Assumes namespace is not locked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvCmosRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext)
{
    ACPI_FUNCTION_TRACE ("EvCmosRegionSetup");


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvDefaultRegionSetup
 *
 * PARAMETERS:  RegionObj           - Region we are interested in
 *              Function            - Start or stop
 *              HandlerContext      - Address space handler context
 *              RegionContext       - Region specific context
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Do any prep work for region handling
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvDefaultRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext)
{
    ACPI_FUNCTION_TRACE ("EvDefaultRegionSetup");


    if (Function == ACPI_REGION_DEACTIVATE)
    {
        *RegionContext = NULL;
    }
    else
    {
        *RegionContext = HandlerContext;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitializeRegion
 *
 * PARAMETERS:  RegionObj       - Region we are initializing
 *              AcpiNsLocked    - Is namespace locked?
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initializes the region, finds any _REG methods and saves them
 *              for execution at a later time
 *
 *              Get the appropriate address space handler for a newly
 *              created region.
 *
 *              This also performs address space specific initialization.  For
 *              example, PCI regions must have an _ADR object that contains
 *              a PCI address in the scope of the definition.  This address is
 *              required to perform an access to PCI config space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitializeRegion (
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsLocked)
{
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_ADR_SPACE_TYPE     SpaceId;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *MethodNode;
    ACPI_NAME               *RegNamePtr = (ACPI_NAME *) METHOD_NAME__REG;
    ACPI_OPERAND_OBJECT     *RegionObj2;


    ACPI_FUNCTION_TRACE_U32 ("EvInitializeRegion", AcpiNsLocked);


    if (!RegionObj)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (RegionObj->Common.Flags & AOPOBJ_OBJECT_INITIALIZED)
    {
        return_ACPI_STATUS (AE_OK);
    }

    RegionObj2 = AcpiNsGetSecondaryObject (RegionObj);
    if (!RegionObj2)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    Node = AcpiNsGetParentNode (RegionObj->Region.Node);
    SpaceId = RegionObj->Region.SpaceId;

    RegionObj->Region.AddrHandler = NULL;
    RegionObj2->Extra.Method_REG = NULL;
    RegionObj->Common.Flags &= ~(AOPOBJ_SETUP_COMPLETE);
    RegionObj->Common.Flags |= AOPOBJ_OBJECT_INITIALIZED;

    /*
     * Find any "_REG" method associated with this region definition
     */
    Status = AcpiNsSearchNode (*RegNamePtr, Node,
                                   ACPI_TYPE_METHOD, &MethodNode);
    if (ACPI_SUCCESS (Status))
    {
        /*
         * The _REG method is optional and there can be only one per region
         * definition.  This will be executed when the handler is attached
         * or removed
         */
        RegionObj2->Extra.Method_REG = MethodNode;
    }

    /*
     * The following loop depends upon the root Node having no parent
     * ie: AcpiGbl_RootNode->ParentEntry being set to NULL
     */
    while (Node)
    {
        /*
         * Check to see if a handler exists
         */
        HandlerObj = NULL;
        ObjDesc = AcpiNsGetAttachedObject (Node);
        if (ObjDesc)
        {
            /*
             * Can only be a handler if the object exists
             */
            switch (Node->Type)
            {
            case ACPI_TYPE_DEVICE:

                HandlerObj = ObjDesc->Device.AddrHandler;
                break;

            case ACPI_TYPE_PROCESSOR:

                HandlerObj = ObjDesc->Processor.AddrHandler;
                break;

            case ACPI_TYPE_THERMAL:

                HandlerObj = ObjDesc->ThermalZone.AddrHandler;
                break;

            default:
                /* Ignore other objects */
                break;
            }

            while (HandlerObj)
            {
                /* Is this handler of the correct type? */

                if (HandlerObj->AddrHandler.SpaceId == SpaceId)
                {
                    /* Found correct handler */

                    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
                        "Found handler %p for region %p in obj %p\n",
                        HandlerObj, RegionObj, ObjDesc));

                    Status = AcpiEvAttachRegion (HandlerObj, RegionObj,
                                AcpiNsLocked);

                    return_ACPI_STATUS (AE_OK);
                }

                /* Try next handler in the list */

                HandlerObj = HandlerObj->AddrHandler.Next;
            }
        }

        /*
         * This node does not have the handler we need;
         * Pop up one level
         */
        Node = AcpiNsGetParentNode (Node);
    }

    /*
     * If we get here, there is no handler for this region
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "No handler for RegionType %s(%X) (RegionObj %p)\n",
        AcpiUtGetRegionName (SpaceId), SpaceId, RegionObj));

    return_ACPI_STATUS (AE_NOT_EXIST);
}

