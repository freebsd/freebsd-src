/*******************************************************************************
 *
 * Module Name: rslist - Linked list utilities
 *              $Revision: 1.47 $
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

#define __RSLIST_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acresrc.h>

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rslist")


/* Local prototypes */

static ACPI_RSCONVERT_INFO *
AcpiRsGetConversionInfo (
    UINT8                   ResourceType);

static ACPI_STATUS
AcpiRsValidateResourceLength (
    AML_RESOURCE            *Aml);


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsValidateResourceLength
 *
 * PARAMETERS:  Aml                 - Pointer to the AML resource descriptor
 *
 * RETURN:      Status - AE_OK if the resource length appears valid
 *
 * DESCRIPTION: Validate the ResourceLength. Fixed-length descriptors must
 *              have the exact length; variable-length descriptors must be
 *              at least as long as the minimum. Certain Small descriptors
 *              can vary in size by at most one byte.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiRsValidateResourceLength (
    AML_RESOURCE            *Aml)
{
    ACPI_RESOURCE_INFO      *ResourceInfo;
    UINT16                  MinimumAmlResourceLength;
    UINT16                  ResourceLength;


    ACPI_FUNCTION_ENTRY ();


    /* Get the size and type info about this resource descriptor */

    ResourceInfo = AcpiRsGetResourceInfo (Aml->SmallHeader.DescriptorType);
    if (!ResourceInfo)
    {
        return (AE_AML_INVALID_RESOURCE_TYPE);
    }

    ResourceLength = AcpiUtGetResourceLength (Aml);
    MinimumAmlResourceLength = ResourceInfo->MinimumAmlResourceLength;

    /* Validate based upon the type of resource, fixed length or variable */

    if (ResourceInfo->LengthType == ACPI_FIXED_LENGTH)
    {
        /* Fixed length resource, length must match exactly */

        if (ResourceLength != MinimumAmlResourceLength)
        {
            return (AE_AML_BAD_RESOURCE_LENGTH);
        }
    }
    else if (ResourceInfo->LengthType == ACPI_VARIABLE_LENGTH)
    {
        /* Variable length resource, must be at least the minimum */

        if (ResourceLength < MinimumAmlResourceLength)
        {
            return (AE_AML_BAD_RESOURCE_LENGTH);
        }
    }
    else
    {
        /* Small variable length resource, allowed to be (Min) or (Min-1) */

        if ((ResourceLength > MinimumAmlResourceLength) ||
            (ResourceLength < (MinimumAmlResourceLength - 1)))
        {
            return (AE_AML_BAD_RESOURCE_LENGTH);
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetConversionInfo
 *
 * PARAMETERS:  ResourceType        - Byte 0 of a resource descriptor
 *
 * RETURN:      Pointer to the resource conversion info table
 *
 * DESCRIPTION: Get the conversion table associated with this resource type
 *
 ******************************************************************************/

static ACPI_RSCONVERT_INFO *
AcpiRsGetConversionInfo (
    UINT8                   ResourceType)
{
    ACPI_FUNCTION_ENTRY ();


    /* Determine if this is a small or large resource */

    if (ResourceType & ACPI_RESOURCE_NAME_LARGE)
    {
        /* Large Resource Type -- bits 6:0 contain the name */

        if (ResourceType > ACPI_RESOURCE_NAME_LARGE_MAX)
        {
            return (NULL);
        }

        return (AcpiGbl_LgGetResourceDispatch [
                    (ResourceType & ACPI_RESOURCE_NAME_LARGE_MASK)]);
    }
    else
    {
        /* Small Resource Type -- bits 6:3 contain the name */

        return (AcpiGbl_SmGetResourceDispatch [
                    ((ResourceType & ACPI_RESOURCE_NAME_SMALL_MASK) >> 3)]);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsConvertAmlToResources
 *
 * PARAMETERS:  AmlBuffer           - Pointer to the resource byte stream
 *              AmlBufferLength     - Length of AmlBuffer
 *              OutputBuffer        - Pointer to the buffer that will
 *                                    contain the output structures
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes the resource byte stream and parses it, creating a
 *              linked list of resources in the caller's output buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsConvertAmlToResources (
    UINT8                   *AmlBuffer,
    UINT32                  AmlBufferLength,
    UINT8                   *OutputBuffer)
{
    UINT8                   *Buffer = OutputBuffer;
    ACPI_STATUS             Status;
    ACPI_SIZE               BytesParsed = 0;
    ACPI_RESOURCE           *Resource;
    ACPI_RSDESC_SIZE        DescriptorLength;
    ACPI_RSCONVERT_INFO     *Info;


    ACPI_FUNCTION_TRACE ("RsConvertAmlToResources");


    /* Loop until end-of-buffer or an EndTag is found */

    while (BytesParsed < AmlBufferLength)
    {
        /* Get the conversion table associated with this Descriptor Type */

        Info = AcpiRsGetConversionInfo (*AmlBuffer);
        if (!Info)
        {
            /* No table indicates an invalid resource type */

            return_ACPI_STATUS (AE_AML_INVALID_RESOURCE_TYPE);
        }

        DescriptorLength = AcpiUtGetDescriptorLength (AmlBuffer);

        /*
         * Perform limited validation of the resource length, based upon
         * what we know about the resource type
         */
        Status = AcpiRsValidateResourceLength (
                    ACPI_CAST_PTR (AML_RESOURCE, AmlBuffer));
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Convert the AML byte stream resource to a local resource struct */

        Status = AcpiRsConvertAmlToResource (
                    ACPI_CAST_PTR (ACPI_RESOURCE, Buffer),
                    ACPI_CAST_PTR (AML_RESOURCE, AmlBuffer),
                    Info);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR ((
                "Could not convert AML resource (type %X) to resource, %s\n",
                *AmlBuffer, AcpiFormatException (Status)));
            return_ACPI_STATUS (Status);
        }

        /* Set the aligned length of the new resource descriptor */

        Resource = ACPI_CAST_PTR (ACPI_RESOURCE, Buffer);
        Resource->Length = (UINT32) ACPI_ALIGN_RESOURCE_SIZE (Resource->Length);

        /* Normal exit on completion of an EndTag resource descriptor */

        if (AcpiUtGetResourceType (AmlBuffer) == ACPI_RESOURCE_NAME_END_TAG)
        {
            return_ACPI_STATUS (AE_OK);
        }

        /* Update counter and point to the next input resource */

        BytesParsed += DescriptorLength;
        AmlBuffer += DescriptorLength;

        /* Point to the next structure in the output buffer */

        Buffer += Resource->Length;
    }

    /* Completed buffer, but did not find an EndTag resource descriptor */

    return_ACPI_STATUS (AE_AML_NO_RESOURCE_END_TAG);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsConvertResourcesToAml
 *
 * PARAMETERS:  Resource            - Pointer to the resource linked list
 *              AmlSizeNeeded       - Calculated size of the byte stream
 *                                    needed from calling AcpiRsGetAmlLength()
 *                                    The size of the OutputBuffer is
 *                                    guaranteed to be >= AmlSizeNeeded
 *              OutputBuffer        - Pointer to the buffer that will
 *                                    contain the byte stream
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes the resource linked list and parses it, creating a
 *              byte stream of resources in the caller's output buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsConvertResourcesToAml (
    ACPI_RESOURCE           *Resource,
    ACPI_SIZE               AmlSizeNeeded,
    UINT8                   *OutputBuffer)
{
    UINT8                   *AmlBuffer = OutputBuffer;
    UINT8                   *EndAmlBuffer = OutputBuffer + AmlSizeNeeded;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("RsConvertResourcesToAml");


    /* Walk the resource descriptor list, convert each descriptor */

    while (AmlBuffer < EndAmlBuffer)
    {
        /* Validate the Resource Type */

        if (Resource->Type > ACPI_RESOURCE_TYPE_MAX)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Invalid descriptor type (%X) in resource list\n",
                Resource->Type));
            return_ACPI_STATUS (AE_BAD_DATA);
        }

        /* Perform the conversion */

        Status = AcpiRsConvertResourceToAml (Resource,
                    ACPI_CAST_PTR (AML_RESOURCE, AmlBuffer),
                    AcpiGbl_SetResourceDispatch[Resource->Type]);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("Could not convert resource (type %X) to AML, %s\n",
                Resource->Type, AcpiFormatException (Status)));
            return_ACPI_STATUS (Status);
        }

        /* Perform final sanity check on the new AML resource descriptor */

        Status = AcpiRsValidateResourceLength (
                    ACPI_CAST_PTR (AML_RESOURCE, AmlBuffer));
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Check for end-of-list, normal exit */

        if (Resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
        {
            /* An End Tag indicates the end of the input Resource Template */

            return_ACPI_STATUS (AE_OK);
        }

        /*
         * Extract the total length of the new descriptor and set the
         * AmlBuffer to point to the next (output) resource descriptor
         */
        AmlBuffer += AcpiUtGetDescriptorLength (AmlBuffer);

        /* Point to the next input resource descriptor */

        Resource = ACPI_PTR_ADD (ACPI_RESOURCE, Resource, Resource->Length);

        /* Check for end-of-list, normal exit */

    }

    /* Completed buffer, but did not find an EndTag resource descriptor */

    return_ACPI_STATUS (AE_AML_NO_RESOURCE_END_TAG);
}

