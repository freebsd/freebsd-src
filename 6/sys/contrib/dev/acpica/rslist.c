/*******************************************************************************
 *
 * Module Name: rslist - Linked list utilities
 *              $Revision: 34 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsGetResourceType
 *
 * PARAMETERS:  ResourceStartByte       - Byte 0 of a resource descriptor
 *
 * RETURN:      The Resource Type (Name) with no extraneous bits
 *
 * DESCRIPTION: Extract the Resource Type/Name from the first byte of
 *              a resource descriptor.
 *
 ******************************************************************************/

UINT8
AcpiRsGetResourceType (
    UINT8                   ResourceStartByte)
{

    ACPI_FUNCTION_ENTRY ();


    /*
     * Determine if this is a small or large resource
     */
    switch (ResourceStartByte & ACPI_RDESC_TYPE_MASK)
    {
    case ACPI_RDESC_TYPE_SMALL:

        /*
         * Small Resource Type -- Only bits 6:3 are valid
         */
        return ((UINT8) (ResourceStartByte & ACPI_RDESC_SMALL_MASK));


    case ACPI_RDESC_TYPE_LARGE:

        /*
         * Large Resource Type -- All bits are valid
         */
        return (ResourceStartByte);


    default:
        /* No other types of resource descriptor */
        break;
    }

    return (0xFF);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsByteStreamToList
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource byte stream
 *              ByteStreamBufferLength  - Length of ByteStreamBuffer
 *              OutputBuffer            - Pointer to the buffer that will
 *                                        contain the output structures
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes the resource byte stream and parses it, creating a
 *              linked list of resources in the caller's output buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsByteStreamToList (
    UINT8                   *ByteStreamBuffer,
    UINT32                  ByteStreamBufferLength,
    UINT8                   *OutputBuffer)
{
    ACPI_STATUS             Status;
    ACPI_SIZE               BytesParsed = 0;
    UINT8                   ResourceType = 0;
    ACPI_SIZE               BytesConsumed = 0;
    UINT8                   *Buffer = OutputBuffer;
    ACPI_SIZE               StructureSize = 0;
    BOOLEAN                 EndTagProcessed = FALSE;
    ACPI_RESOURCE           *Resource;

    ACPI_FUNCTION_TRACE ("RsByteStreamToList");


    while (BytesParsed < ByteStreamBufferLength &&
            !EndTagProcessed)
    {
        /*
         * The next byte in the stream is the resource type
         */
        ResourceType = AcpiRsGetResourceType (*ByteStreamBuffer);

        switch (ResourceType)
        {
        case ACPI_RDESC_TYPE_MEMORY_24:
            /*
             * 24-Bit Memory Resource
             */
            Status = AcpiRsMemory24Resource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_LARGE_VENDOR:
            /*
             * Vendor Defined Resource
             */
            Status = AcpiRsVendorResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_MEMORY_32:
            /*
             * 32-Bit Memory Range Resource
             */
            Status = AcpiRsMemory32RangeResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_FIXED_MEMORY_32:
            /*
             * 32-Bit Fixed Memory Resource
             */
            Status = AcpiRsFixedMemory32Resource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_QWORD_ADDRESS_SPACE:
            /*
             * 64-Bit Address Resource
             */
            Status = AcpiRsAddress64Resource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_DWORD_ADDRESS_SPACE:
            /*
             * 32-Bit Address Resource
             */
            Status = AcpiRsAddress32Resource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_WORD_ADDRESS_SPACE:
            /*
             * 16-Bit Address Resource
             */
            Status = AcpiRsAddress16Resource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_EXTENDED_XRUPT:
            /*
             * Extended IRQ
             */
            Status = AcpiRsExtendedIrqResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_IRQ_FORMAT:
            /*
             * IRQ Resource
             */
            Status = AcpiRsIrqResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_DMA_FORMAT:
            /*
             * DMA Resource
             */
            Status = AcpiRsDmaResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_START_DEPENDENT:
            /*
             * Start Dependent Functions Resource
             */
            Status = AcpiRsStartDependFnsResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_END_DEPENDENT:
            /*
             * End Dependent Functions Resource
             */
            Status = AcpiRsEndDependFnsResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_IO_PORT:
            /*
             * IO Port Resource
             */
            Status = AcpiRsIoResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_FIXED_IO_PORT:
            /*
             * Fixed IO Port Resource
             */
            Status = AcpiRsFixedIoResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_SMALL_VENDOR:
            /*
             * Vendor Specific Resource
             */
            Status = AcpiRsVendorResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        case ACPI_RDESC_TYPE_END_TAG:
            /*
             * End Tag
             */
            EndTagProcessed = TRUE;
            Status = AcpiRsEndTagResource (ByteStreamBuffer,
                        &BytesConsumed, &Buffer, &StructureSize);
            break;


        default:
            /*
             * Invalid/Unknown resource type
             */
            Status = AE_AML_INVALID_RESOURCE_TYPE;
            break;
        }

        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * Update the return value and counter
         */
        BytesParsed += BytesConsumed;

        /*
         * Set the byte stream to point to the next resource
         */
        ByteStreamBuffer += BytesConsumed;

        /*
         * Set the Buffer to the next structure
         */
        Resource = ACPI_CAST_PTR (ACPI_RESOURCE, Buffer);
        Resource->Length = (UINT32) ACPI_ALIGN_RESOURCE_SIZE (Resource->Length);
        Buffer += ACPI_ALIGN_RESOURCE_SIZE (StructureSize);

    } /*  end while */

    /*
     * Check the reason for exiting the while loop
     */
    if (!EndTagProcessed)
    {
        return_ACPI_STATUS (AE_AML_NO_RESOURCE_END_TAG);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsListToByteStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              ByteSteamSizeNeeded     - Calculated size of the byte stream
 *                                        needed from calling
 *                                        AcpiRsGetByteStreamLength()
 *                                        The size of the OutputBuffer is
 *                                        guaranteed to be >=
 *                                        ByteStreamSizeNeeded
 *              OutputBuffer            - Pointer to the buffer that will
 *                                        contain the byte stream
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes the resource linked list and parses it, creating a
 *              byte stream of resources in the caller's output buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsListToByteStream (
    ACPI_RESOURCE           *LinkedList,
    ACPI_SIZE               ByteStreamSizeNeeded,
    UINT8                   *OutputBuffer)
{
    ACPI_STATUS             Status;
    UINT8                   *Buffer = OutputBuffer;
    ACPI_SIZE               BytesConsumed = 0;
    BOOLEAN                 Done = FALSE;


    ACPI_FUNCTION_TRACE ("RsListToByteStream");


    while (!Done)
    {
        switch (LinkedList->Id)
        {
        case ACPI_RSTYPE_IRQ:
            /*
             * IRQ Resource
             */
            Status = AcpiRsIrqStream (LinkedList, &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_DMA:
            /*
             * DMA Resource
             */
            Status = AcpiRsDmaStream (LinkedList, &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_START_DPF:
            /*
             * Start Dependent Functions Resource
             */
            Status = AcpiRsStartDependFnsStream (LinkedList,
                            &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_END_DPF:
            /*
             * End Dependent Functions Resource
             */
            Status = AcpiRsEndDependFnsStream (LinkedList,
                            &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_IO:
            /*
             * IO Port Resource
             */
            Status = AcpiRsIoStream (LinkedList, &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_FIXED_IO:
            /*
             * Fixed IO Port Resource
             */
            Status = AcpiRsFixedIoStream (LinkedList, &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_VENDOR:
            /*
             * Vendor Defined Resource
             */
            Status = AcpiRsVendorStream (LinkedList, &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_END_TAG:
            /*
             * End Tag
             */
            Status = AcpiRsEndTagStream (LinkedList, &Buffer, &BytesConsumed);

            /*
             * An End Tag indicates the end of the Resource Template
             */
            Done = TRUE;
            break;

        case ACPI_RSTYPE_MEM24:
            /*
             * 24-Bit Memory Resource
             */
            Status = AcpiRsMemory24Stream (LinkedList, &Buffer, &BytesConsumed);
            break;

        case ACPI_RSTYPE_MEM32:
            /*
             * 32-Bit Memory Range Resource
             */
            Status = AcpiRsMemory32RangeStream (LinkedList, &Buffer,
                        &BytesConsumed);
            break;

        case ACPI_RSTYPE_FIXED_MEM32:
            /*
             * 32-Bit Fixed Memory Resource
             */
            Status = AcpiRsFixedMemory32Stream (LinkedList, &Buffer,
                        &BytesConsumed);
            break;

        case ACPI_RSTYPE_ADDRESS16:
            /*
             * 16-Bit Address Descriptor Resource
             */
            Status = AcpiRsAddress16Stream (LinkedList, &Buffer,
                        &BytesConsumed);
            break;

        case ACPI_RSTYPE_ADDRESS32:
            /*
             * 32-Bit Address Descriptor Resource
             */
            Status = AcpiRsAddress32Stream (LinkedList, &Buffer,
                        &BytesConsumed);
            break;

        case ACPI_RSTYPE_ADDRESS64:
            /*
             * 64-Bit Address Descriptor Resource
             */
            Status = AcpiRsAddress64Stream (LinkedList, &Buffer,
                        &BytesConsumed);
            break;

        case ACPI_RSTYPE_EXT_IRQ:
            /*
             * Extended IRQ Resource
             */
            Status = AcpiRsExtendedIrqStream (LinkedList, &Buffer,
                        &BytesConsumed);
            break;

        default:
            /*
             * If we get here, everything is out of sync,
             *  so exit with an error
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Invalid descriptor type (%X) in resource list\n",
                LinkedList->Id));
            Status = AE_BAD_DATA;
            break;

        } /* switch (LinkedList->Id) */

        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * Set the Buffer to point to the open byte
         */
        Buffer += BytesConsumed;

        /*
         * Point to the next object
         */
        LinkedList = ACPI_PTR_ADD (ACPI_RESOURCE,
                        LinkedList, LinkedList->Length);
    }

    return_ACPI_STATUS (AE_OK);
}

