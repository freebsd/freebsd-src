/*******************************************************************************
 *
 * Module Name: rsxface - Public interfaces to the resource manager
 *              $Revision: 1.37 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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


#define __RSXFACE_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acresrc.h>

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsxface")

/* Local macros for 16,32-bit to 64-bit conversion */

#define ACPI_COPY_FIELD(Out, In, Field)  ((Out)->Field = (In)->Field)
#define ACPI_COPY_ADDRESS(Out, In)                      \
    ACPI_COPY_FIELD(Out, In, ResourceType);              \
    ACPI_COPY_FIELD(Out, In, ProducerConsumer);          \
    ACPI_COPY_FIELD(Out, In, Decode);                    \
    ACPI_COPY_FIELD(Out, In, MinAddressFixed);           \
    ACPI_COPY_FIELD(Out, In, MaxAddressFixed);           \
    ACPI_COPY_FIELD(Out, In, Info);                      \
    ACPI_COPY_FIELD(Out, In, Granularity);               \
    ACPI_COPY_FIELD(Out, In, Minimum);                   \
    ACPI_COPY_FIELD(Out, In, Maximum);                   \
    ACPI_COPY_FIELD(Out, In, TranslationOffset);         \
    ACPI_COPY_FIELD(Out, In, AddressLength);             \
    ACPI_COPY_FIELD(Out, In, ResourceSource);


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetIrqRoutingTable
 *
 * PARAMETERS:  DeviceHandle    - a handle to the Bus device we are querying
 *              RetBuffer       - a pointer to a buffer to receive the
 *                                current resources for the device
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the IRQ routing table for a
 *              specific bus.  The caller must first acquire a handle for the
 *              desired bus.  The routine table is placed in the buffer pointed
 *              to by the RetBuffer variable parameter.
 *
 *              If the function fails an appropriate status will be returned
 *              and the value of RetBuffer is undefined.
 *
 *              This function attempts to execute the _PRT method contained in
 *              the object indicated by the passed DeviceHandle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetIrqRoutingTable  (
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiGetIrqRoutingTable ");


    /*
     * Must have a valid handle and buffer, So we have to have a handle
     * and a return buffer structure, and if there is a non-zero buffer length
     * we also need a valid pointer in the buffer. If it's a zero buffer length,
     * we'll be returning the needed buffer size, so keep going.
     */
    if (!DeviceHandle)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtValidateBuffer (RetBuffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiRsGetPrtMethodData (DeviceHandle, RetBuffer);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetCurrentResources
 *
 * PARAMETERS:  DeviceHandle    - a handle to the device object for the
 *                                device we are querying
 *              RetBuffer       - a pointer to a buffer to receive the
 *                                current resources for the device
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the current resources for a
 *              specific device.  The caller must first acquire a handle for
 *              the desired device.  The resource data is placed in the buffer
 *              pointed to by the RetBuffer variable parameter.
 *
 *              If the function fails an appropriate status will be returned
 *              and the value of RetBuffer is undefined.
 *
 *              This function attempts to execute the _CRS method contained in
 *              the object indicated by the passed DeviceHandle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetCurrentResources (
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiGetCurrentResources");


    /*
     * Must have a valid handle and buffer, So we have to have a handle
     * and a return buffer structure, and if there is a non-zero buffer length
     * we also need a valid pointer in the buffer. If it's a zero buffer length,
     * we'll be returning the needed buffer size, so keep going.
     */
    if (!DeviceHandle)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtValidateBuffer (RetBuffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiRsGetCrsMethodData (DeviceHandle, RetBuffer);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetPossibleResources
 *
 * PARAMETERS:  DeviceHandle    - a handle to the device object for the
 *                                device we are querying
 *              RetBuffer       - a pointer to a buffer to receive the
 *                                resources for the device
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get a list of the possible resources
 *              for a specific device.  The caller must first acquire a handle
 *              for the desired device.  The resource data is placed in the
 *              buffer pointed to by the RetBuffer variable.
 *
 *              If the function fails an appropriate status will be returned
 *              and the value of RetBuffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetPossibleResources (
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiGetPossibleResources");


    /*
     * Must have a valid handle and buffer, So we have to have a handle
     * and a return buffer structure, and if there is a non-zero buffer length
     * we also need a valid pointer in the buffer. If it's a zero buffer length,
     * we'll be returning the needed buffer size, so keep going.
     */
    if (!DeviceHandle)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtValidateBuffer (RetBuffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiRsGetPrsMethodData (DeviceHandle, RetBuffer);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiWalkResources
 *
 * PARAMETERS:  DeviceHandle    - a handle to the device object for the
 *                                device we are querying
 *              Path            - method name of the resources we want
 *                                (METHOD_NAME__CRS or METHOD_NAME__PRS)
 *              UserFunction    - called for each resource
 *              Context         - passed to UserFunction
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieves the current or possible resource list for the
 *              specified device.  The UserFunction is called once for
 *              each resource in the list.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiWalkResources (
    ACPI_HANDLE                 DeviceHandle,
    char                        *Path,
    ACPI_WALK_RESOURCE_CALLBACK UserFunction,
    void                        *Context)
{
    ACPI_STATUS                 Status;
    ACPI_BUFFER                 Buffer = {ACPI_ALLOCATE_BUFFER, NULL};
    ACPI_RESOURCE               *Resource;
    ACPI_RESOURCE               *BufferEnd;


    ACPI_FUNCTION_TRACE ("AcpiWalkResources");


    if (!DeviceHandle ||
        (ACPI_STRNCMP (Path, METHOD_NAME__CRS, sizeof (METHOD_NAME__CRS)) &&
         ACPI_STRNCMP (Path, METHOD_NAME__PRS, sizeof (METHOD_NAME__PRS))))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiRsGetMethodData (DeviceHandle, Path, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Setup pointers */

    Resource  = (ACPI_RESOURCE *) Buffer.Pointer;
    BufferEnd = ACPI_CAST_PTR (ACPI_RESOURCE,
                    ((UINT8 *) Buffer.Pointer + Buffer.Length));

    /* Walk the resource list */

    for (;;)
    {
        if (!Resource || Resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
        {
            break;
        }

        Status = UserFunction (Resource, Context);

        switch (Status)
        {
        case AE_OK:
        case AE_CTRL_DEPTH:

            /* Just keep going */

            Status = AE_OK;
            break;

        case AE_CTRL_TERMINATE:

            /* Exit now, with OK stats */

            Status = AE_OK;
            goto Cleanup;

        default:

            /* All others are valid exceptions */

            goto Cleanup;
        }

        /* Get the next resource descriptor */

        Resource = ACPI_NEXT_RESOURCE (Resource);

        /* Check for end-of-buffer */

        if (Resource >= BufferEnd)
        {
            goto Cleanup;
        }
    }

Cleanup:

    AcpiOsFree (Buffer.Pointer);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetCurrentResources
 *
 * PARAMETERS:  DeviceHandle    - a handle to the device object for the
 *                                device we are changing the resources of
 *              InBuffer        - a pointer to a buffer containing the
 *                                resources to be set for the device
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to set the current resources for a
 *              specific device.  The caller must first acquire a handle for
 *              the desired device.  The resource data is passed to the routine
 *              the buffer pointed to by the InBuffer variable.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetCurrentResources (
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *InBuffer)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiSetCurrentResources");


    /* Must have a valid handle and buffer */

    if ((!DeviceHandle)       ||
        (!InBuffer)           ||
        (!InBuffer->Pointer)  ||
        (!InBuffer->Length))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiRsSetSrsMethodData (DeviceHandle, InBuffer);
    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiResourceToAddress64
 *
 * PARAMETERS:  Resource                - Pointer to a resource
 *              Out                     - Pointer to the users's return
 *                                        buffer (a struct
 *                                        acpi_resource_address64)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: If the resource is an address16, address32, or address64,
 *              copy it to the address64 return buffer.  This saves the
 *              caller from having to duplicate code for different-sized
 *              addresses.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiResourceToAddress64 (
    ACPI_RESOURCE               *Resource,
    ACPI_RESOURCE_ADDRESS64     *Out)
{
    ACPI_RESOURCE_ADDRESS16     *Address16;
    ACPI_RESOURCE_ADDRESS32     *Address32;


    switch (Resource->Type)
    {
    case ACPI_RESOURCE_TYPE_ADDRESS16:

        Address16 = (ACPI_RESOURCE_ADDRESS16 *) &Resource->Data;
        ACPI_COPY_ADDRESS (Out, Address16);
        break;

    case ACPI_RESOURCE_TYPE_ADDRESS32:

        Address32 = (ACPI_RESOURCE_ADDRESS32 *) &Resource->Data;
        ACPI_COPY_ADDRESS (Out, Address32);
        break;

    case ACPI_RESOURCE_TYPE_ADDRESS64:

        /* Simple copy for 64 bit source */

        ACPI_MEMCPY (Out, &Resource->Data, sizeof (ACPI_RESOURCE_ADDRESS64));
        break;

    default:
        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}
