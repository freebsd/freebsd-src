/******************************************************************************
 *
 * Module Name: aeinstall - Installation of operation region handlers
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
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

#include "aecommon.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeinstall")


static ACPI_STATUS
AeRegionInit (
    ACPI_HANDLE             RegionHandle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

static ACPI_STATUS
AeInstallEcHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AeInstallPciHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AeInstallGedHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);


BOOLEAN                     AcpiGbl_DisplayRegionAccess = FALSE;
ACPI_CONNECTION_INFO        AeMyContext;


/*
 * We will override some of the default region handlers, especially
 * the SystemMemory handler, which must be implemented locally.
 * These handlers are installed "early" - before any _REG methods
 * are executed - since they are special in the sense that the ACPI spec
 * declares that they must "always be available". Cannot override the
 * DataTable region handler either -- needed for test execution.
 *
 * NOTE: The local region handler will simulate access to these address
 * spaces by creating a memory buffer behind each operation region.
 */
static ACPI_ADR_SPACE_TYPE  DefaultSpaceIdList[] =
{
    ACPI_ADR_SPACE_SYSTEM_MEMORY,
    ACPI_ADR_SPACE_SYSTEM_IO,
    ACPI_ADR_SPACE_PCI_CONFIG,
    ACPI_ADR_SPACE_EC
};

/*
 * We will install handlers for some of the various address space IDs.
 * Test one user-defined address space (used by aslts).
 */
#define ACPI_ADR_SPACE_USER_DEFINED1        0x80
#define ACPI_ADR_SPACE_USER_DEFINED2        0xE4

static ACPI_ADR_SPACE_TYPE  SpaceIdList[] =
{
    ACPI_ADR_SPACE_SMBUS,
    ACPI_ADR_SPACE_CMOS,
    ACPI_ADR_SPACE_PCI_BAR_TARGET,
    ACPI_ADR_SPACE_IPMI,
    ACPI_ADR_SPACE_GPIO,
    ACPI_ADR_SPACE_GSBUS,
    ACPI_ADR_SPACE_PLATFORM_COMM,
    ACPI_ADR_SPACE_PLATFORM_RT,
    ACPI_ADR_SPACE_FIXED_HARDWARE,
    ACPI_ADR_SPACE_USER_DEFINED1,
    ACPI_ADR_SPACE_USER_DEFINED2
};


/******************************************************************************
 *
 * FUNCTION:    AeRegionInit
 *
 * PARAMETERS:  Region init handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Opregion init function.
 *
 *****************************************************************************/

static ACPI_STATUS
AeRegionInit (
    ACPI_HANDLE                 RegionHandle,
    UINT32                      Function,
    void                        *HandlerContext,
    void                        **RegionContext)
{

    if (Function == ACPI_REGION_DEACTIVATE)
    {
        *RegionContext = NULL;
    }
    else
    {
        *RegionContext = RegionHandle;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeOverrideRegionHandlers
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Override the default region handlers for memory, i/o, and
 *              pci_config. Also install a handler for EC. This is part of
 *              the "install early handlers" functionality.
 *
 *****************************************************************************/

void
AeOverrideRegionHandlers (
    void)
{
    UINT32                  i;
    ACPI_STATUS             Status;

    /*
     * Install handlers that will override the default handlers for some of
     * the space IDs.
     */
    for (i = 0; i < ACPI_ARRAY_LENGTH (DefaultSpaceIdList); i++)
    {
        /* Install handler at the root object */

        Status = AcpiInstallAddressSpaceHandler (ACPI_ROOT_OBJECT,
            DefaultSpaceIdList[i], AeRegionHandler, AeRegionInit,
            &AeMyContext);

        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "Could not install an OpRegion handler for %s space(%u)",
                AcpiUtGetRegionName ((UINT8) DefaultSpaceIdList[i]),
                DefaultSpaceIdList[i]));
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeInstallRegionHandlers
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Install handlers for the address spaces other than
 *              SystemMemory, SystemIO, and PCI_CONFIG.
 *
 *****************************************************************************/

void
AeInstallRegionHandlers (
    void)
{
    UINT32                  i;
    ACPI_STATUS             Status;


    /*
     * Install handlers for some of the "device driver" address spaces
     * such as SMBus, etc.
     */
    for (i = 0; i < ACPI_ARRAY_LENGTH (SpaceIdList); i++)
    {
        /* Install handler at the root object */

        Status = AcpiInstallAddressSpaceHandler (ACPI_ROOT_OBJECT,
            SpaceIdList[i], AeRegionHandler, AeRegionInit,
            &AeMyContext);

        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "Could not install an OpRegion handler for %s space(%u)",
                AcpiUtGetRegionName((UINT8) SpaceIdList[i]), SpaceIdList[i]));
            return;
        }
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AeInstallGedHandler
 *
 * PARAMETERS:  ACPI_WALK_NAMESPACE callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk entire namespace, install a handler for every GED
 *              device found.
 *
 ******************************************************************************/
static ACPI_STATUS
AeInstallGedHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{

	ACPI_BUFFER             ReturnBuffer;
	ACPI_STATUS Status;
	ACPI_RESOURCE *ResourceList;
	ACPI_RESOURCE_EXTENDED_IRQ *extended_irq_rsc;
    ACPI_NAMESPACE_NODE     *Node;
	ACPI_NAMESPACE_NODE		*EvtMethodNode;

    ACPI_FUNCTION_ENTRY();

	/* Obtain the Namespace Node of this GED object handle. */
	Node = AcpiNsValidateHandle (ObjHandle);
	if (!Node)
	{
		return (AE_BAD_PARAMETER);
	}

	/*
	 * A GED device must have one _EVT method.
	 * Obtain the _EVT method and store it in the global
	 * GED register.
	 */
	Status = AcpiNsSearchOneScope (
		*ACPI_CAST_PTR (ACPI_NAME, METHOD_NAME__EVT),
		Node,
		ACPI_TYPE_METHOD,
		&EvtMethodNode
	);
	if (ACPI_FAILURE (Status))
	{
		AcpiOsPrintf ("Failed to obtain _EVT method for the GED device.\n");
		return Status;
	}

	ReturnBuffer.Pointer = NULL;
    ReturnBuffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	Status = AcpiGetCurrentResources (ObjHandle, &ReturnBuffer);
	if (ACPI_FAILURE (Status))
	{
		AcpiOsPrintf ("AcpiGetCurrentResources failed: %s\n",
			AcpiFormatException (Status));
		return Status;
	}

	/* Traverse the _CRS resource list */
	ResourceList = ACPI_CAST_PTR (ACPI_RESOURCE, ReturnBuffer.Pointer);
	while (ResourceList->Type != ACPI_RESOURCE_TYPE_END_TAG) {

		switch (ResourceList->Type) {
		 case ACPI_RESOURCE_TYPE_EXTENDED_IRQ: {

			/*
			 * Found an Interrupt resource. Link the interrupt resource
			 * and the _EVT method of this GED device in the GED event handler list.
			 */
			ACPI_GED_HANDLER_INFO *GedHandler =
			ACPI_ALLOCATE (sizeof (ACPI_SCI_HANDLER_INFO));
			if (!GedHandler)
			{
				return AE_NO_MEMORY;
			}

			GedHandler->Next = AcpiGbl_GedHandlerList;
			AcpiGbl_GedHandlerList = GedHandler;

			extended_irq_rsc = &ResourceList->Data.ExtendedIrq;

			GedHandler->IntId = extended_irq_rsc->Interrupts[0];
			GedHandler->EvtMethod = EvtMethodNode;

			AcpiOsPrintf ("Interrupt ID %d\n", extended_irq_rsc->Interrupts[0]);

			break;
		 }
		 default:

			AcpiOsPrintf ("Resource type %X\n", ResourceList->Type);
		}

		ResourceList = ACPI_NEXT_RESOURCE (ResourceList);
	}

	return AE_OK;
}

/*******************************************************************************
 *
 * FUNCTION:    AeInstallDeviceHandlers
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install handlers for all EC, PCI and GED devices in the namespace
 *
 ******************************************************************************/

ACPI_STATUS
AeInstallDeviceHandlers (
    void)
{

    /* Find all Embedded Controller devices */

    AcpiGetDevices ("PNP0C09", AeInstallEcHandler, NULL, NULL);

    /* Install a PCI handler */

    AcpiGetDevices ("PNP0A08", AeInstallPciHandler, NULL, NULL);

    /* Install a GED handler */

    AcpiGetDevices ("ACPI0013", AeInstallGedHandler, NULL, NULL);

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AeInstallEcHandler
 *
 * PARAMETERS:  ACPI_WALK_NAMESPACE callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk entire namespace, install a handler for every EC
 *              device found.
 *
 ******************************************************************************/

static ACPI_STATUS
AeInstallEcHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;


    /* Install the handler for this EC device */

    Status = AcpiInstallAddressSpaceHandler (ObjHandle,
        ACPI_ADR_SPACE_EC, AeRegionHandler, AeRegionInit, &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an OpRegion handler for EC device (%p)",
            ObjHandle));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AeInstallPciHandler
 *
 * PARAMETERS:  ACPI_WALK_NAMESPACE callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk entire namespace, install a handler for every PCI
 *              device found.
 *
 ******************************************************************************/

static ACPI_STATUS
AeInstallPciHandler (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;


    /* Install memory and I/O handlers for the PCI device */

    Status = AcpiInstallAddressSpaceHandler (ObjHandle,
        ACPI_ADR_SPACE_SYSTEM_IO, AeRegionHandler, AeRegionInit,
        &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an OpRegion handler for PCI device (%p)",
            ObjHandle));
    }

    Status = AcpiInstallAddressSpaceHandler (ObjHandle,
        ACPI_ADR_SPACE_SYSTEM_MEMORY, AeRegionHandler, AeRegionInit,
        &AeMyContext);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "Could not install an OpRegion handler for PCI device (%p)",
            ObjHandle));
    }

    return (AE_CTRL_TERMINATE);
}
