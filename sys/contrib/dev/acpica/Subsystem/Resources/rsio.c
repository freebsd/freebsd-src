/*******************************************************************************
 *
 * Module Name: rsio - AcpiRsIoResource
 *                     AcpiRsFixedIoResource
 *                     AcpiRsIoStream
 *                     AcpiRsFixedIoStream
 *                     AcpiRsDmaResource
 *                     AcpiRsDmaStream
 *              $Revision: 9 $
 *
 ******************************************************************************/

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

#define __RSIO_C__

#include "acpi.h"

#define _COMPONENT          RESOURCE_MANAGER
        MODULE_NAME         ("rsio")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsIoResource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                          stream
 *              BytesConsumed           - UINT32 pointer that is filled with
 *                                          the number of bytes consumed from
 *                                          the ByteStreamBuffer
 *              OutputBuffer            - Pointer to the user's return buffer
 *              StructureSize           - UINT32 pointer that is filled with
 *                                          the number of bytes in the filled
 *                                          in structure
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *                  structure pointed to by the OutputBuffer.  Return the
 *                  number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsIoResource (
    UINT8                   *ByteStreamBuffer,
    UINT32                  *BytesConsumed,
    UINT8                   **OutputBuffer,
    UINT32                  *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    RESOURCE                *OutputStruct = (RESOURCE *) * OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    UINT32                  StructSize = sizeof (IO_RESOURCE) +
                                         RESOURCE_LENGTH_NO_DATA;


    FUNCTION_TRACE ("RsIoResource");

    /*
     * The number of bytes consumed are Constant
     */
    *BytesConsumed = 8;

    OutputStruct->Id = Io;

    /*
     * Check Decode
     */
    Buffer += 1;
    Temp8 = *Buffer;

    OutputStruct->Data.Io.IoDecode = Temp8 & 0x01;

    /*
     * Check MinBase Address
     */
    Buffer += 1;
    MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

    OutputStruct->Data.Io.MinBaseAddress = Temp16;

    /*
     * Check MaxBase Address
     */
    Buffer += 2;
    MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

    OutputStruct->Data.Io.MaxBaseAddress = Temp16;

    /*
     * Check Base alignment
     */
    Buffer += 2;
    Temp8 = *Buffer;

    OutputStruct->Data.Io.Alignment = Temp8;

    /*
     * Check RangeLength
     */
    Buffer += 1;
    Temp8 = *Buffer;

    OutputStruct->Data.Io.RangeLength = Temp8;

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsFixedIoResource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                          stream
 *              BytesConsumed           - UINT32 pointer that is filled with
 *                                          the number of bytes consumed from
 *                                          the ByteStreamBuffer
 *              OutputBuffer            - Pointer to the user's return buffer
 *              StructureSize           - UINT32 pointer that is filled with
 *                                          the number of bytes in the filled
 *                                          in structure
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *                  structure pointed to by the OutputBuffer.  Return the
 *                  number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsFixedIoResource (
    UINT8                   *ByteStreamBuffer,
    UINT32                  *BytesConsumed,
    UINT8                   **OutputBuffer,
    UINT32                  *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    RESOURCE                *OutputStruct = (RESOURCE *) * OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    UINT32                  StructSize = sizeof (FIXED_IO_RESOURCE) +
                                         RESOURCE_LENGTH_NO_DATA;


    FUNCTION_TRACE ("RsFixedIoResource");

    /*
     * The number of bytes consumed are Constant
     */
    *BytesConsumed = 4;

    OutputStruct->Id = FixedIo;

    /*
     * Check Range Base Address
     */
    Buffer += 1;
    MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

    OutputStruct->Data.FixedIo.BaseAddress = Temp16;

    /*
     * Check RangeLength
     */
    Buffer += 2;
    Temp8 = *Buffer;

    OutputStruct->Data.FixedIo.RangeLength = Temp8;

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsIoStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - UINT32 pointer that is filled with
 *                                          the number of bytes of the
 *                                          OutputBuffer used
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *                  the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsIoStream (
    RESOURCE                *LinkedList,
    UINT8                   **OutputBuffer,
    UINT32                  *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;


    FUNCTION_TRACE ("RsIoStream");

    /*
     * The descriptor field is static
     */
    *Buffer = 0x47;
    Buffer += 1;

    /*
     * Io Information Byte
     */
    Temp8 = (UINT8) (LinkedList->Data.Io.IoDecode & 0x01);

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the Range minimum base address
     */
    Temp16 = (UINT16) LinkedList->Data.Io.MinBaseAddress;

    MOVE_UNALIGNED16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the Range maximum base address
     */
    Temp16 = (UINT16) LinkedList->Data.Io.MaxBaseAddress;

    MOVE_UNALIGNED16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the base alignment
     */
    Temp8 = (UINT8) LinkedList->Data.Io.Alignment;

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the range length
     */
    Temp8 = (UINT8) LinkedList->Data.Io.RangeLength;

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = (UINT32) ((NATIVE_UINT) Buffer -
                     (NATIVE_UINT) *OutputBuffer);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsFixedIoStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - UINT32 pointer that is filled with
 *                                          the number of bytes of the
 *                                          OutputBuffer used
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *                  the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsFixedIoStream (
    RESOURCE                *LinkedList,
    UINT8                   **OutputBuffer,
    UINT32                  *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;


    FUNCTION_TRACE ("RsFixedIoStream");

    /*
     * The descriptor field is static
     */
    *Buffer = 0x4B;

    Buffer += 1;

    /*
     * Set the Range base address
     */
    Temp16 = (UINT16) LinkedList->Data.FixedIo.BaseAddress;

    MOVE_UNALIGNED16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the range length
     */
    Temp8 = (UINT8) LinkedList->Data.FixedIo.RangeLength;

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = (UINT32) ((NATIVE_UINT) Buffer -
                     (NATIVE_UINT) *OutputBuffer);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDmaResource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                          stream
 *              BytesConsumed           - UINT32 pointer that is filled with
 *                                          the number of bytes consumed from
 *                                          the ByteStreamBuffer
 *              OutputBuffer            - Pointer to the user's return buffer
 *              StructureSize           - UINT32 pointer that is filled with
 *                                          the number of bytes in the filled
 *                                          in structure
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *                  structure pointed to by the OutputBuffer.  Return the
 *                  number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsDmaResource (
    UINT8                   *ByteStreamBuffer,
    UINT32                  *BytesConsumed,
    UINT8                   **OutputBuffer,
    UINT32                  *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    RESOURCE                *OutputStruct = (RESOURCE *) * OutputBuffer;
    UINT8                   Temp8 = 0;
    UINT8                   Index;
    UINT8                   i;
    UINT32                  StructSize = sizeof(DMA_RESOURCE) +
                                         RESOURCE_LENGTH_NO_DATA;


    FUNCTION_TRACE ("RsDmaResource");

    /*
     * The number of bytes consumed are Constant
     */
    *BytesConsumed = 3;
    OutputStruct->Id = Dma;

    /*
     * Point to the 8-bits of Byte 1
     */
    Buffer += 1;
    Temp8 = *Buffer;

    /* Decode the IRQ bits */

    for (i = 0, Index = 0; Index < 8; Index++)
    {
        if ((Temp8 >> Index) & 0x01)
        {
            OutputStruct->Data.Dma.Channels[i] = Index;
            i++;
        }
    }
    OutputStruct->Data.Dma.NumberOfChannels = i;


    /*
     * Calculate the structure size based upon the number of interrupts
     */
    StructSize += (OutputStruct->Data.Dma.NumberOfChannels - 1) * 4;

    /*
     * Point to Byte 2
     */
    Buffer += 1;
    Temp8 = *Buffer;

    /*
     * Check for transfer preference (Bits[1:0])
     */
    OutputStruct->Data.Dma.Transfer = Temp8 & 0x03;

    if (0x03 == OutputStruct->Data.Dma.Transfer)
    {
        return_ACPI_STATUS (AE_BAD_DATA);
    }

    /*
     * Get bus master preference (Bit[2])
     */
    OutputStruct->Data.Dma.BusMaster = (Temp8 >> 2) & 0x01;

    /*
     * Get channel speed support (Bits[6:5])
     */
    OutputStruct->Data.Dma.Type = (Temp8 >> 5) & 0x03;

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDmaStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - UINT32 pointer that is filled with
 *                                          the number of bytes of the
 *                                          OutputBuffer used
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *                  the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsDmaStream (
    RESOURCE                *LinkedList,
    UINT8                   **OutputBuffer,
    UINT32                  *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    UINT8                   Index;


    FUNCTION_TRACE ("RsDmaStream");


    /*
     * The descriptor field is static
     */
    *Buffer = 0x2A;
    Buffer += 1;
    Temp8 = 0;

    /*
     * Loop through all of the Channels and set the mask bits
     */
    for (Index = 0;
         Index < LinkedList->Data.Dma.NumberOfChannels;
         Index++)
    {
        Temp16 = (UINT16) LinkedList->Data.Dma.Channels[Index];
        Temp8 |= 0x1 << Temp16;
    }

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the DMA Info
     */
    Temp8 = (UINT8) ((LinkedList->Data.Dma.Type & 0x03) << 5);
    Temp8 |= ((LinkedList->Data.Dma.BusMaster & 0x01) << 2);
    Temp8 |= (LinkedList->Data.Dma.Transfer & 0x03);

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = (UINT32) ((NATIVE_UINT) Buffer -
                     (NATIVE_UINT) *OutputBuffer);

    return_ACPI_STATUS (AE_OK);
}

