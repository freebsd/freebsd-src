/*******************************************************************************
 *
 * Module Name: rsaddr - AcpiRsAddress16Resource
 *                       AcpiRsAddress16Stream
 *                       AcpiRsAddress32Resource
 *                       AcpiRsAddress32Stream
 *              $Revision: 11 $
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

#define __RSADDR_C__

#include "acpi.h"

#define _COMPONENT          RESOURCE_MANAGER
        MODULE_NAME         ("rsaddr")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsAddress16Resource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                              stream
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
AcpiRsAddress16Resource (
    UINT8                   *ByteStreamBuffer,
    UINT32                  *BytesConsumed,
    UINT8                   **OutputBuffer,
    UINT32                  *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    RESOURCE                *OutputStruct = (RESOURCE *) * OutputBuffer;
    UINT16                  Temp16;
    UINT8                   Temp8;
    UINT32                  Index;
    UINT32                  StructSize = sizeof(ADDRESS16_RESOURCE) +
                                         RESOURCE_LENGTH_NO_DATA;


    FUNCTION_TRACE ("RsAddress16Resource");

    /*
     * Point past the Descriptor to get the number of bytes consumed
     */
    Buffer += 1;

    MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

    *BytesConsumed = Temp16 + 3;

    OutputStruct->Id = Address16;

    OutputStruct->Length = StructSize;

    /*
     * Get the Resource Type (Byte3)
     */
    Buffer += 2;
    Temp8 = *Buffer;

    /* Values 0-2 are valid */
    if (Temp8 > 2)
    {
        return_ACPI_STATUS (AE_AML_ERROR);
    }

    OutputStruct->Data.Address16.ResourceType = Temp8 & 0x03;

    /*
     * Get the General Flags (Byte4)
     */
    Buffer += 1;
    Temp8 = *Buffer;

    /*
     * Producer / Consumer
     */
    OutputStruct->Data.Address16.ProducerConsumer = Temp8 & 0x01;

    /*
     * Decode
     */
    OutputStruct->Data.Address16.Decode = (Temp8 >> 1) & 0x01;

    /*
     * Min Address Fixed
     */
    OutputStruct->Data.Address16.MinAddressFixed = (Temp8 >> 2) & 0x01;

    /*
     * Max Address Fixed
     */
    OutputStruct->Data.Address16.MaxAddressFixed = (Temp8 >> 3) & 0x01;

    /*
     * Get the Type Specific Flags (Byte5)
     */
    Buffer += 1;
    Temp8 = *Buffer;

    if (MEMORY_RANGE == OutputStruct->Data.Address16.ResourceType)
    {
        OutputStruct->Data.Address16.Attribute.Memory.ReadWriteAttribute =
                (UINT16) (Temp8 & 0x01);
        OutputStruct->Data.Address16.Attribute.Memory.CacheAttribute =
                (UINT16) ((Temp8 >> 1) & 0x0F);
    }

    else
    {
        if (IO_RANGE == OutputStruct->Data.Address16.ResourceType)
        {
            OutputStruct->Data.Address16.Attribute.Io.RangeAttribute =
                (UINT16) (Temp8 & 0x03);
        }

        else
        {
            /* BUS_NUMBER_RANGE == Address32Data->ResourceType */
            /* Nothing needs to be filled in */
        }
    }

    /*
     * Get Granularity (Bytes 6-7)
     */
    Buffer += 1;
    MOVE_UNALIGNED16_TO_16 (&OutputStruct->Data.Address16.Granularity,
                            Buffer);

    /*
     * Get MinAddressRange (Bytes 8-9)
     */
    Buffer += 2;
    MOVE_UNALIGNED16_TO_16 (&OutputStruct->Data.Address16.MinAddressRange,
                            Buffer);

    /*
     * Get MaxAddressRange (Bytes 10-11)
     */
    Buffer += 2;
    MOVE_UNALIGNED16_TO_16
        (&OutputStruct->Data.Address16.MaxAddressRange,
         Buffer);

    /*
     * Get AddressTranslationOffset (Bytes 12-13)
     */
    Buffer += 2;
    MOVE_UNALIGNED16_TO_16
        (&OutputStruct->Data.Address16.AddressTranslationOffset,
         Buffer);

    /*
     * Get AddressLength (Bytes 14-15)
     */
    Buffer += 2;
    MOVE_UNALIGNED16_TO_16
        (&OutputStruct->Data.Address16.AddressLength,
         Buffer);

    /*
     * Resource Source Index (if present)
     */
    Buffer += 2;

    /*
     * This will leave us pointing to the Resource Source Index
     *  If it is present, then save it off and calculate the
     *  pointer to where the null terminated string goes:
     *  Each Interrupt takes 32-bits + the 5 bytes of the
     *  stream that are default.
     */
    if (*BytesConsumed > 16)
    {
        /* Dereference the Index */

        Temp8 = *Buffer;
        OutputStruct->Data.Address16.ResourceSourceIndex =
                (UINT32) Temp8;

        /* Point to the String */

        Buffer += 1;

        /* Copy the string into the buffer */

        Index = 0;

        while (0x00 != *Buffer)
        {
            OutputStruct->Data.Address16.ResourceSource[Index] =
                *Buffer;

            Buffer += 1;
            Index += 1;
        }

        /*
         * Add the terminating null
         */
        OutputStruct->Data.Address16.ResourceSource[Index] = 0x00;

        OutputStruct->Data.Address16.ResourceSourceStringLength =
                Index + 1;

        /*
         * In order for the StructSize to fall on a 32-bit boundry,
         *  calculate the length of the string and expand the
         *  StructSize to the next 32-bit boundry.
         */
        Temp8 = (UINT8) (Index + 1);
        StructSize += ROUND_UP_TO_32BITS (Temp8);
        OutputStruct->Length = StructSize;
    }
    else
    {
        OutputStruct->Data.Address16.ResourceSourceIndex = 0x00;
        OutputStruct->Data.Address16.ResourceSourceStringLength = 0;
        OutputStruct->Data.Address16.ResourceSource[0] = 0x00;
    }

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsAddress16Stream
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
AcpiRsAddress16Stream (
    RESOURCE                *LinkedList,
    UINT8                   **OutputBuffer,
    UINT32                  *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT8                   *LengthField;
    UINT8                   Temp8;
    NATIVE_CHAR             *TempPointer = NULL;
    UINT32                  ActualBytes;


    FUNCTION_TRACE ("RsAddress16Stream");

    /*
     * The descriptor field is static
     */
    *Buffer = 0x88;
    Buffer += 1;

    /*
     * Save a pointer to the Length field - to be filled in later
     */
    LengthField = Buffer;
    Buffer += 2;

    /*
     * Set the Resource Type (Memory, Io, BusNumber)
     */
    Temp8 = (UINT8) (LinkedList->Data.Address16.ResourceType & 0x03);
    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the general flags
     */
    Temp8 = (UINT8) (LinkedList->Data.Address16.ProducerConsumer & 0x01);

    Temp8 |= (LinkedList->Data.Address16.Decode & 0x01) << 1;
    Temp8 |= (LinkedList->Data.Address16.MinAddressFixed & 0x01) << 2;
    Temp8 |= (LinkedList->Data.Address16.MaxAddressFixed & 0x01) << 3;

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the type specific flags
     */
    Temp8 = 0;

    if (MEMORY_RANGE == LinkedList->Data.Address16.ResourceType)
    {
        Temp8 = (UINT8)
            (LinkedList->Data.Address16.Attribute.Memory.ReadWriteAttribute &
             0x01);

        Temp8 |=
            (LinkedList->Data.Address16.Attribute.Memory.CacheAttribute &
             0x0F) << 1;
    }

    else if (IO_RANGE == LinkedList->Data.Address16.ResourceType)
    {
        Temp8 = (UINT8)
            (LinkedList->Data.Address16.Attribute.Io.RangeAttribute &
             0x03);
    }

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the address space granularity
     */
    MOVE_UNALIGNED16_TO_16 (Buffer,
                            &LinkedList->Data.Address16.Granularity);
    Buffer += 2;

    /*
     * Set the address range minimum
     */
    MOVE_UNALIGNED16_TO_16 (Buffer,
                            &LinkedList->Data.Address16.MinAddressRange);
    Buffer += 2;

    /*
     * Set the address range maximum
     */
    MOVE_UNALIGNED16_TO_16 (Buffer,
                            &LinkedList->Data.Address16.MaxAddressRange);
    Buffer += 2;

    /*
     * Set the address translation offset
     */
    MOVE_UNALIGNED16_TO_16 (Buffer,
                    &LinkedList->Data.Address16.AddressTranslationOffset);
    Buffer += 2;

    /*
     * Set the address length
     */
    MOVE_UNALIGNED16_TO_16 (Buffer,
                            &LinkedList->Data.Address16.AddressLength);
    Buffer += 2;

    /*
     * Resource Source Index and Resource Source are optional
     */
    if (0 != LinkedList->Data.Address16.ResourceSourceStringLength)
    {
        Temp8 = (UINT8) LinkedList->Data.Address16.ResourceSourceIndex;

        *Buffer = Temp8;
        Buffer += 1;

        TempPointer = (NATIVE_CHAR *) Buffer;

        /*
         * Copy the string
         */
        STRCPY (TempPointer, LinkedList->Data.Address16.ResourceSource);

        /*
         * Buffer needs to be set to the length of the sting + one for the
         *  terminating null
         */
        Buffer += (STRLEN (LinkedList->Data.Address16.ResourceSource) + 1);
    }

    /*
     * Return the number of bytes consumed in this operation
     */
    ActualBytes = (UINT32) ((NATIVE_UINT) Buffer -
                  (NATIVE_UINT) *OutputBuffer);

    *BytesConsumed = ActualBytes;

    /*
     * Set the length field to the number of bytes consumed
     * minus the header size (3 bytes)
     */
    ActualBytes -= 3;
    MOVE_UNALIGNED16_TO_16 (LengthField, &ActualBytes);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsAddress32Resource
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
AcpiRsAddress32Resource (
    UINT8                   *ByteStreamBuffer,
    UINT32                  *BytesConsumed,
    UINT8                   **OutputBuffer,
    UINT32                  *StructureSize)
{
    UINT8                   *Buffer;
    RESOURCE                *OutputStruct;
    UINT16                  Temp16;
    UINT8                   Temp8;
    UINT32                  StructSize;
    UINT32                  Index;


    FUNCTION_TRACE ("RsAddress32Resource");

    Buffer = ByteStreamBuffer;

    OutputStruct = (RESOURCE *) *OutputBuffer;

    StructSize = sizeof (ADDRESS32_RESOURCE) +
                 RESOURCE_LENGTH_NO_DATA;

    /*
     * Point past the Descriptor to get the number of bytes consumed
     */
    Buffer += 1;
    MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

    *BytesConsumed = Temp16 + 3;

    OutputStruct->Id = Address32;

    /*
     * Get the Resource Type (Byte3)
     */
    Buffer += 2;
    Temp8 = *Buffer;

    /* Values 0-2 are valid */
    if(Temp8 > 2)
    {
        return_ACPI_STATUS (AE_AML_ERROR);
    }

    OutputStruct->Data.Address32.ResourceType = Temp8 & 0x03;

    /*
     * Get the General Flags (Byte4)
     */
    Buffer += 1;
    Temp8 = *Buffer;

    /*
     * Producer / Consumer
     */
    OutputStruct->Data.Address32.ProducerConsumer = Temp8 & 0x01;

    /*
     * Decode
     */
    OutputStruct->Data.Address32.Decode = (Temp8 >> 1) & 0x01;

    /*
     * Min Address Fixed
     */
    OutputStruct->Data.Address32.MinAddressFixed = (Temp8 >> 2) & 0x01;

    /*
     * Max Address Fixed
     */
    OutputStruct->Data.Address32.MaxAddressFixed = (Temp8 >> 3) & 0x01;

    /*
     * Get the Type Specific Flags (Byte5)
     */
    Buffer += 1;
    Temp8 = *Buffer;

    if (MEMORY_RANGE == OutputStruct->Data.Address32.ResourceType)
    {
        OutputStruct->Data.Address32.Attribute.Memory.ReadWriteAttribute =
                (UINT16) (Temp8 & 0x01);

        OutputStruct->Data.Address32.Attribute.Memory.CacheAttribute =
                (UINT16) ((Temp8 >> 1) & 0x0F);
    }

    else
    {
        if (IO_RANGE == OutputStruct->Data.Address32.ResourceType)
        {
            OutputStruct->Data.Address32.Attribute.Io.RangeAttribute =
                (UINT16) (Temp8 & 0x03);
        }

        else
        {
            /* BUS_NUMBER_RANGE == OutputStruct->Data.Address32.ResourceType */
            /* Nothing needs to be filled in */
        }
    }

    /*
     * Get Granularity (Bytes 6-9)
     */
    Buffer += 1;
    MOVE_UNALIGNED32_TO_32 (&OutputStruct->Data.Address32.Granularity,
                            Buffer);

    /*
     * Get MinAddressRange (Bytes 10-13)
     */
    Buffer += 4;
    MOVE_UNALIGNED32_TO_32 (&OutputStruct->Data.Address32.MinAddressRange,
                            Buffer);

    /*
     * Get MaxAddressRange (Bytes 14-17)
     */
    Buffer += 4;
    MOVE_UNALIGNED32_TO_32 (&OutputStruct->Data.Address32.MaxAddressRange,
                            Buffer);

    /*
     * Get AddressTranslationOffset (Bytes 18-21)
     */
    Buffer += 4;
    MOVE_UNALIGNED32_TO_32
                (&OutputStruct->Data.Address32.AddressTranslationOffset,
                 Buffer);

    /*
     * Get AddressLength (Bytes 22-25)
     */
    Buffer += 4;
    MOVE_UNALIGNED32_TO_32 (&OutputStruct->Data.Address32.AddressLength,
                            Buffer);

    /*
     * Resource Source Index (if present)
     */
    Buffer += 4;

    /*
     * This will leave us pointing to the Resource Source Index
     *  If it is present, then save it off and calculate the
     *  pointer to where the null terminated string goes:
     *  Each Interrupt takes 32-bits + the 5 bytes of the
     *  stream that are default.
     */
    if (*BytesConsumed > 26)
    {
        /* Dereference the Index */

        Temp8 = *Buffer;
        OutputStruct->Data.Address32.ResourceSourceIndex = (UINT32)Temp8;

        /* Point to the String */

        Buffer += 1;

        /* Copy the string into the buffer */

        Index = 0;

        while (0x00 != *Buffer)
        {
            OutputStruct->Data.Address32.ResourceSource[Index] = *Buffer;
            Buffer += 1;
            Index += 1;
        }

        /*
         * Add the terminating null
         */
        OutputStruct->Data.Address32.ResourceSource[Index] = 0x00;

        OutputStruct->Data.Address32.ResourceSourceStringLength = Index + 1;

        /*
         * In order for the StructSize to fall on a 32-bit boundry,
         *  calculate the length of the string and expand the
         *  StructSize to the next 32-bit boundry.
         */
        Temp8 = (UINT8) (Index + 1);
        StructSize += ROUND_UP_TO_32BITS (Temp8);
    }

    else
    {
        OutputStruct->Data.Address32.ResourceSourceIndex = 0x00;
        OutputStruct->Data.Address32.ResourceSourceStringLength = 0;
        OutputStruct->Data.Address32.ResourceSource[0] = 0x00;
    }

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
 * FUNCTION:    AcpiRsAddress32Stream
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
AcpiRsAddress32Stream (
    RESOURCE                *LinkedList,
    UINT8                   **OutputBuffer,
    UINT32                  *BytesConsumed)
{
    UINT8                   *Buffer;
    UINT16                  *LengthField;
    UINT8                   Temp8;
    NATIVE_CHAR             *TempPointer;


    FUNCTION_TRACE ("RsAddress32Stream");

    Buffer = *OutputBuffer;

    /*
     * The descriptor field is static
     */
    *Buffer = 0x87;
    Buffer += 1;

    /*
     * Set a pointer to the Length field - to be filled in later
     */

    LengthField = (UINT16 *)Buffer;
    Buffer += 2;

    /*
     * Set the Resource Type (Memory, Io, BusNumber)
     */
    Temp8 = (UINT8) (LinkedList->Data.Address32.ResourceType & 0x03);

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the general flags
     */
    Temp8 = (UINT8) (LinkedList->Data.Address32.ProducerConsumer & 0x01);
    Temp8 |= (LinkedList->Data.Address32.Decode & 0x01) << 1;
    Temp8 |= (LinkedList->Data.Address32.MinAddressFixed & 0x01) << 2;
    Temp8 |= (LinkedList->Data.Address32.MaxAddressFixed & 0x01) << 3;

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the type specific flags
     */
    Temp8 = 0;

    if(MEMORY_RANGE == LinkedList->Data.Address32.ResourceType)
    {
        Temp8 = (UINT8)
            (LinkedList->Data.Address32.Attribute.Memory.ReadWriteAttribute &
            0x01);

        Temp8 |=
            (LinkedList->Data.Address32.Attribute.Memory.CacheAttribute &
             0x0F) << 1;
    }

    else if (IO_RANGE == LinkedList->Data.Address32.ResourceType)
    {
        Temp8 = (UINT8)
            (LinkedList->Data.Address32.Attribute.Io.RangeAttribute &
             0x03);
    }

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the address space granularity
     */
    MOVE_UNALIGNED32_TO_32 (Buffer,
                            &LinkedList->Data.Address32.Granularity);
    Buffer += 4;

    /*
     * Set the address range minimum
     */
    MOVE_UNALIGNED32_TO_32 (Buffer,
                            &LinkedList->Data.Address32.MinAddressRange);
    Buffer += 4;

    /*
     * Set the address range maximum
     */
    MOVE_UNALIGNED32_TO_32 (Buffer,
                            &LinkedList->Data.Address32.MaxAddressRange);
    Buffer += 4;

    /*
     * Set the address translation offset
     */
    MOVE_UNALIGNED32_TO_32 (Buffer,
                    &LinkedList->Data.Address32.AddressTranslationOffset);
    Buffer += 4;

    /*
     * Set the address length
     */
    MOVE_UNALIGNED32_TO_32 (Buffer,
                            &LinkedList->Data.Address32.AddressLength);
    Buffer += 4;

    /*
     * Resource Source Index and Resource Source are optional
     */
    if (0 != LinkedList->Data.Address32.ResourceSourceStringLength)
    {
        Temp8 = (UINT8) LinkedList->Data.Address32.ResourceSourceIndex;

        *Buffer = Temp8;
        Buffer += 1;

        TempPointer = (NATIVE_CHAR *) Buffer;

        /*
         * Copy the string
         */
        STRCPY (TempPointer, LinkedList->Data.Address32.ResourceSource);

        /*
         * Buffer needs to be set to the length of the sting + one for the
         *  terminating null
         */
        Buffer += (STRLEN (LinkedList->Data.Address32.ResourceSource) + 1);
    }

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = (UINT32) ((NATIVE_UINT) Buffer -
                     (NATIVE_UINT) *OutputBuffer);

    /*
     * Set the length field to the number of bytes consumed
     *  minus the header size (3 bytes)
     */
    *LengthField = (UINT16) (*BytesConsumed - 3);

    return_ACPI_STATUS (AE_OK);
}

