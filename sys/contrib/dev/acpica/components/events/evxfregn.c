/******************************************************************************
 *
 * Module Name: evxfregn - External Interfaces, ACPI Operation Regions and
 *                         Address Spaces.
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2025, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#define EXPORT_ACPI_INTERFACES

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acevents.h>

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evxfregn")


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallAddressSpaceHandlerInternal
 *
 * PARAMETERS:  Device          - Handle for the device
 *              SpaceId         - The address space ID
 *              Handler         - Address of the handler
 *              Setup           - Address of the setup function
 *              Context         - Value passed to the handler on each access
 *              Run_Reg         - Run _REG methods for this address space?
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for all OpRegions of a given SpaceId.
 *
 * NOTE: This function should only be called after AcpiEnableSubsystem has
 * been called. This is because any _REG methods associated with the Space ID
 * are executed here, and these methods can only be safely executed after
 * the default handlers have been installed and the hardware has been
 * initialized (via AcpiEnableSubsystem.)
 * To avoid this problem pass FALSE for Run_Reg and later on call
 * AcpiExecuteRegMethods() to execute _REG.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiInstallAddressSpaceHandlerInternal (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context,
    BOOLEAN                 Run_Reg)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiInstallAddressSpaceHandler);


    /* Parameter validation */

    if (!Device)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsValidateHandle (Device);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Install the handler for all Regions for this Space ID */

    Status = AcpiEvInstallSpaceHandler (
        Node, SpaceId, Handler, Setup, Context);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    /* Run all _REG methods for this address space */

    if (Run_Reg)
    {
        AcpiEvExecuteRegMethods (Node, SpaceId, ACPI_REG_CONNECT);
    }

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_STATUS
AcpiInstallAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context)
{
    return AcpiInstallAddressSpaceHandlerInternal (Device, SpaceId, Handler, Setup, Context, TRUE);
}

ACPI_EXPORT_SYMBOL (AcpiInstallAddressSpaceHandler)

ACPI_STATUS
AcpiInstallAddressSpaceHandlerNo_Reg (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context)
{
    return AcpiInstallAddressSpaceHandlerInternal (Device, SpaceId, Handler, Setup, Context, FALSE);
}

ACPI_EXPORT_SYMBOL (AcpiInstallAddressSpaceHandlerNo_Reg)


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveAddressSpaceHandler
 *
 * PARAMETERS:  Device          - Handle for the device
 *              SpaceId         - The address space ID
 *              Handler         - Address of the handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a previously installed handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_OPERAND_OBJECT     *RegionObj;
    ACPI_OPERAND_OBJECT     **LastObjPtr;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiRemoveAddressSpaceHandler);


    /* Parameter validation */

    if (!Device)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsValidateHandle (Device);
    if (!Node ||
        ((Node->Type != ACPI_TYPE_DEVICE)    &&
         (Node->Type != ACPI_TYPE_PROCESSOR) &&
         (Node->Type != ACPI_TYPE_THERMAL)   &&
         (Node != AcpiGbl_RootNode)))
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Make sure the internal object exists */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        Status = AE_NOT_EXIST;
        goto UnlockAndExit;
    }

    /* Find the address handler the user requested */

    HandlerObj = ObjDesc->CommonNotify.Handler;
    LastObjPtr = &ObjDesc->CommonNotify.Handler;
    while (HandlerObj)
    {
        /* We have a handler, see if user requested this one */

        if (HandlerObj->AddressSpace.SpaceId == SpaceId)
        {
            /* Handler must be the same as the installed handler */

            if (HandlerObj->AddressSpace.Handler != Handler)
            {
                Status = AE_BAD_PARAMETER;
                goto UnlockAndExit;
            }

            /* Matched SpaceId, first dereference this in the Regions */

            ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
                "Removing address handler %p(%p) for region %s "
                "on Device %p(%p)\n",
                HandlerObj, Handler, AcpiUtGetRegionName (SpaceId),
                Node, ObjDesc));

            RegionObj = HandlerObj->AddressSpace.RegionList;

            /* Walk the handler's region list */

            while (RegionObj)
            {
                /*
                 * First disassociate the handler from the region.
                 *
                 * NOTE: this doesn't mean that the region goes away
                 * The region is just inaccessible as indicated to
                 * the _REG method
                 */
                AcpiEvDetachRegion (RegionObj, TRUE);

                /*
                 * Walk the list: Just grab the head because the
                 * DetachRegion removed the previous head.
                 */
                RegionObj = HandlerObj->AddressSpace.RegionList;
            }

            /* Remove this Handler object from the list */

            *LastObjPtr = HandlerObj->AddressSpace.Next;

            /* Now we can delete the handler object */

            AcpiUtRemoveReference (HandlerObj);
            goto UnlockAndExit;
        }

        /* Walk the linked list of handlers */

        LastObjPtr = &HandlerObj->AddressSpace.Next;
        HandlerObj = HandlerObj->AddressSpace.Next;
    }

    /* The handler does not exist */

    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "Unable to remove address handler %p for %s(%X), DevNode %p, obj %p\n",
        Handler, AcpiUtGetRegionName (SpaceId), SpaceId, Node, ObjDesc));

    Status = AE_NOT_EXIST;

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRemoveAddressSpaceHandler)


/*******************************************************************************
 *
 * FUNCTION:    AcpiExecuteRegMethods
 *
 * PARAMETERS:  Device          - Handle for the device
 *              SpaceId         - The address space ID
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute _REG for all OpRegions of a given SpaceId.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExecuteRegMethods (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiExecuteRegMethods);


    /* Parameter validation */

    if (!Device)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsValidateHandle (Device);
    if (Node)
    {
        /* Run all _REG methods for this address space */

        AcpiEvExecuteRegMethods (Node, SpaceId, ACPI_REG_CONNECT);
    }
    else
    {
        Status = AE_BAD_PARAMETER;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiExecuteRegMethods)
