/*******************************************************************************
 *
 * Module Name: rsmem24 - Memory resource descriptors
 *              $Revision: 24 $
 *
 ******************************************************************************/

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

#define __RSMEMORY_C__

#include "acpi.h"
#include "acresrc.h"

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsmemory")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsMemory24Resource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                        stream
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        consumed the ByteStreamBuffer is
 *                                        returned
 *              OutputBuffer            - Pointer to the return data buffer
 *              StructureSize           - Pointer to where the number of bytes
 *                                        in the return data struct is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *              structure pointed to by the OutputBuffer.  Return the
 *              number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsMemory24Resource (
    UINT8                   *ByteStreamBuffer,
    ACPI_SIZE               *BytesConsumed,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    ACPI_RESOURCE           *OutputStruct = (void *) *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    ACPI_SIZE               StructSize = ACPI_SIZEOF_RESOURCE (ACPI_RESOURCE_MEM24);


    ACPI_FUNCTION_TRACE ("RsMemory24Resource");


    /*
     * Point past the Descriptor to get the number of bytes consumed
     */
    Buffer += 1;

    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);
    Buffer += 2;
    *BytesConsumed = (ACPI_SIZE) Temp16 + 3;
    OutputStruct->Id = ACPI_RSTYPE_MEM24;

    /*
     * Check Byte 3 the Read/Write bit
     */
    Temp8 = *Buffer;
    Buffer += 1;
    OutputStruct->Data.Memory24.ReadWriteAttribute = Temp8 & 0x01;

    /*
     * Get MinBaseAddress (Bytes 4-5)
     */
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);
    Buffer += 2;
    OutputStruct->Data.Memory24.MinBaseAddress = Temp16;

    /*
     * Get MaxBaseAddress (Bytes 6-7)
     */
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);
    Buffer += 2;
    OutputStruct->Data.Memory24.MaxBaseAddress = Temp16;

    /*
     * Get Alignment (Bytes 8-9)
     */
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);
    Buffer += 2;
    OutputStruct->Data.Memory24.Alignment = Temp16;

    /*
     * Get RangeLength (Bytes 10-11)
     */
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);
    OutputStruct->Data.Memory24.RangeLength = Temp16;

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = (UINT32) StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsMemory24Stream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        used in the OutputBuffer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *              the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsMemory24Stream (
    ACPI_RESOURCE           *LinkedList,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;


    ACPI_FUNCTION_TRACE ("RsMemory24Stream");


    /*
     * The descriptor field is static
     */
    *Buffer = 0x81;
    Buffer += 1;

    /*
     * The length field is static
     */
    Temp16 = 0x09;
    ACPI_MOVE_16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the Information Byte
     */
    Temp8 = (UINT8) (LinkedList->Data.Memory24.ReadWriteAttribute & 0x01);
    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the Range minimum base address
     */
    ACPI_MOVE_32_TO_16 (Buffer, &LinkedList->Data.Memory24.MinBaseAddress);
    Buffer += 2;

    /*
     * Set the Range maximum base address
     */
    ACPI_MOVE_32_TO_16 (Buffer, &LinkedList->Data.Memory24.MaxBaseAddress);
    Buffer += 2;

    /*
     * Set the base alignment
     */
    ACPI_MOVE_32_TO_16 (Buffer, &LinkedList->Data.Memory24.Alignment);
    Buffer += 2;

    /*
     * Set the range length
     */
    ACPI_MOVE_32_TO_16 (Buffer, &LinkedList->Data.Memory24.RangeLength);
    Buffer += 2;

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = ACPI_PTR_DIFF (Buffer, *OutputBuffer);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsMemory32RangeResource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                        stream
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        consumed the ByteStreamBuffer is
 *                                        returned
 *              OutputBuffer            - Pointer to the return data buffer
 *              StructureSize           - Pointer to where the number of bytes
 *                                        in the return data struct is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *              structure pointed to by the OutputBuffer.  Return the
 *              number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsMemory32RangeResource (
    UINT8                   *ByteStreamBuffer,
    ACPI_SIZE               *BytesConsumed,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    ACPI_RESOURCE           *OutputStruct = (void *) *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    ACPI_SIZE               StructSize = ACPI_SIZEOF_RESOURCE (ACPI_RESOURCE_MEM32);


    ACPI_FUNCTION_TRACE ("RsMemory32RangeResource");


    /*
     * Point past the Descriptor to get the number of bytes consumed
     */
    Buffer += 1;

    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);
    Buffer += 2;
    *BytesConsumed = (ACPI_SIZE) Temp16 + 3;

    OutputStruct->Id = ACPI_RSTYPE_MEM32;

    /*
     *  Point to the place in the output buffer where the data portion will
     *  begin.
     *  1. Set the RESOURCE_DATA * Data to point to its own address, then
     *  2. Set the pointer to the next address.
     *
     *  NOTE: OutputStruct->Data is cast to UINT8, otherwise, this addition adds
     *  4 * sizeof(RESOURCE_DATA) instead of 4 * sizeof(UINT8)
     */

    /*
     * Check Byte 3 the Read/Write bit
     */
    Temp8 = *Buffer;
    Buffer += 1;

    OutputStruct->Data.Memory32.ReadWriteAttribute = Temp8 & 0x01;

    /*
     * Get MinBaseAddress (Bytes 4-7)
     */
    ACPI_MOVE_32_TO_32 (&OutputStruct->Data.Memory32.MinBaseAddress, Buffer);
    Buffer += 4;

    /*
     * Get MaxBaseAddress (Bytes 8-11)
     */
    ACPI_MOVE_32_TO_32 (&OutputStruct->Data.Memory32.MaxBaseAddress, Buffer);
    Buffer += 4;

    /*
     * Get Alignment (Bytes 12-15)
     */
    ACPI_MOVE_32_TO_32 (&OutputStruct->Data.Memory32.Alignment, Buffer);
    Buffer += 4;

    /*
     * Get RangeLength (Bytes 16-19)
     */
    ACPI_MOVE_32_TO_32 (&OutputStruct->Data.Memory32.RangeLength, Buffer);

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = (UINT32) StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsFixedMemory32Resource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                        stream
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        consumed the ByteStreamBuffer is
 *                                        returned
 *              OutputBuffer            - Pointer to the return data buffer
 *              StructureSize           - Pointer to where the number of bytes
 *                                        in the return data struct is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *              structure pointed to by the OutputBuffer.  Return the
 *              number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsFixedMemory32Resource (
    UINT8                   *ByteStreamBuffer,
    ACPI_SIZE               *BytesConsumed,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    ACPI_RESOURCE           *OutputStruct = (void *) *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    ACPI_SIZE               StructSize = ACPI_SIZEOF_RESOURCE (ACPI_RESOURCE_FIXED_MEM32);


    ACPI_FUNCTION_TRACE ("RsFixedMemory32Resource");


    /*
     * Point past the Descriptor to get the number of bytes consumed
     */
    Buffer += 1;
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);

    Buffer += 2;
    *BytesConsumed = (ACPI_SIZE) Temp16 + 3;

    OutputStruct->Id = ACPI_RSTYPE_FIXED_MEM32;

    /*
     * Check Byte 3 the Read/Write bit
     */
    Temp8 = *Buffer;
    Buffer += 1;
    OutputStruct->Data.FixedMemory32.ReadWriteAttribute = Temp8 & 0x01;

    /*
     * Get RangeBaseAddress (Bytes 4-7)
     */
    ACPI_MOVE_32_TO_32 (&OutputStruct->Data.FixedMemory32.RangeBaseAddress, Buffer);
    Buffer += 4;

    /*
     * Get RangeLength (Bytes 8-11)
     */
    ACPI_MOVE_32_TO_32 (&OutputStruct->Data.FixedMemory32.RangeLength, Buffer);

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = (UINT32) StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsMemory32RangeStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        used in the OutputBuffer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *              the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsMemory32RangeStream (
    ACPI_RESOURCE           *LinkedList,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;


    ACPI_FUNCTION_TRACE ("RsMemory32RangeStream");


    /*
     * The descriptor field is static
     */
    *Buffer = 0x85;
    Buffer += 1;

    /*
     * The length field is static
     */
    Temp16 = 0x11;

    ACPI_MOVE_16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the Information Byte
     */
    Temp8 = (UINT8) (LinkedList->Data.Memory32.ReadWriteAttribute & 0x01);
    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the Range minimum base address
     */
    ACPI_MOVE_32_TO_32 (Buffer, &LinkedList->Data.Memory32.MinBaseAddress);
    Buffer += 4;

    /*
     * Set the Range maximum base address
     */
    ACPI_MOVE_32_TO_32 (Buffer, &LinkedList->Data.Memory32.MaxBaseAddress);
    Buffer += 4;

    /*
     * Set the base alignment
     */
    ACPI_MOVE_32_TO_32 (Buffer, &LinkedList->Data.Memory32.Alignment);
    Buffer += 4;

    /*
     * Set the range length
     */
    ACPI_MOVE_32_TO_32 (Buffer, &LinkedList->Data.Memory32.RangeLength);
    Buffer += 4;

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = ACPI_PTR_DIFF (Buffer, *OutputBuffer);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsFixedMemory32Stream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        used in the OutputBuffer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *              the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsFixedMemory32Stream (
    ACPI_RESOURCE           *LinkedList,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;


    ACPI_FUNCTION_TRACE ("RsFixedMemory32Stream");


    /*
     * The descriptor field is static
     */
    *Buffer = 0x86;
    Buffer += 1;

    /*
     * The length field is static
     */
    Temp16 = 0x09;

    ACPI_MOVE_16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the Information Byte
     */
    Temp8 = (UINT8) (LinkedList->Data.FixedMemory32.ReadWriteAttribute & 0x01);
    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the Range base address
     */
    ACPI_MOVE_32_TO_32 (Buffer,
                            &LinkedList->Data.FixedMemory32.RangeBaseAddress);
    Buffer += 4;

    /*
     * Set the range length
     */
    ACPI_MOVE_32_TO_32 (Buffer,
                            &LinkedList->Data.FixedMemory32.RangeLength);
    Buffer += 4;

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = ACPI_PTR_DIFF (Buffer, *OutputBuffer);
    return_ACPI_STATUS (AE_OK);
}

