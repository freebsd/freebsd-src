/*******************************************************************************
 *
 * Module Name: rscalc - AcpiRsCalculateByteStreamLength
 *                       AcpiRsCalculateListLength
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

#define __RSCALC_C__

#include "acpi.h"

#define _COMPONENT          RESOURCE_MANAGER
        MODULE_NAME         ("rscalc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsCalculateByteStreamLength
 *
 * PARAMETERS:  LinkedList          - Pointer to the resource linked list
 *              SizeNeeded          - UINT32 pointer of the size buffer needed
 *                                      to properly return the parsed data
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Takes the resource byte stream and parses it once, calculating
 *              the size buffer needed to hold the linked list that conveys
 *              the resource data.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsCalculateByteStreamLength (
    RESOURCE                *LinkedList,
    UINT32                  *SizeNeeded)
{
    UINT32                  ByteStreamSizeNeeded = 0;
    UINT32                  SegmentSize;
    EXTENDED_IRQ_RESOURCE   *ExIrq = NULL;
    BOOLEAN                 Done = FALSE;


    FUNCTION_TRACE ("RsCalculateByteStreamLength");


    while (!Done)
    {

        /*
         * Init the variable that will hold the size to add to the
         *  total.
         */
        SegmentSize = 0;

        switch (LinkedList->Id)
        {
        case Irq:
            /*
             * IRQ Resource
             */
            /*
             * For an IRQ Resource, Byte 3, although optional, will
             *  always be created - it holds IRQ information.
             */
            SegmentSize = 4;
            break;

        case Dma:
            /*
             * DMA Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 3;
            break;

        case StartDependentFunctions:
            /*
             * Start Dependent Functions Resource
             */
            /*
             * For a StartDependentFunctions Resource, Byte 1,
             * although optional, will always be created.
             */
            SegmentSize = 2;
            break;

        case EndDependentFunctions:
            /*
             * End Dependent Functions Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 1;
            break;

        case Io:
            /*
             * IO Port Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 8;
            break;

        case FixedIo:
            /*
             * Fixed IO Port Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 4;
            break;

        case VendorSpecific:
            /*
             * Vendor Defined Resource
             */
            /*
             * For a Vendor Specific resource, if the Length is
             *  between 1 and 7 it will be created as a Small
             *  Resource data type, otherwise it is a Large
             *  Resource data type.
             */
            if(LinkedList->Data.VendorSpecific.Length > 7)
            {
                SegmentSize = 3;
            }
            else
            {
                SegmentSize = 1;
            }
            SegmentSize +=
                LinkedList->Data.VendorSpecific.Length;
            break;

        case EndTag:
            /*
             * End Tag
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 2;
            Done = TRUE;
            break;

        case Memory24:
            /*
             * 24-Bit Memory Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 12;
            break;

        case Memory32:
            /*
             * 32-Bit Memory Range Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 20;
            break;

        case FixedMemory32:
            /*
             * 32-Bit Fixed Memory Resource
             */
            /*
             * For this resource the size is static
             */
            SegmentSize = 12;
            break;

        case Address16:
            /*
             * 16-Bit Address Resource
             */
            /*
             * The base size of this byte stream is 16. If a
             *  Resource Source string is not NULL, add 1 for
             *  the Index + the length of the null terminated
             *  string Resource Source + 1 for the null.
             */
            SegmentSize = 16;

            if(NULL != LinkedList->Data.Address16.ResourceSource)
            {
                SegmentSize += (1 +
                    LinkedList->Data.Address16.ResourceSourceStringLength);
            }
            break;

        case Address32:
            /*
             * 32-Bit Address Resource
             */
            /*
             * The base size of this byte stream is 26. If a Resource
             *  Source string is not NULL, add 1 for the Index + the
             *  length of the null terminated string Resource Source +
             *  1 for the null.
             */
            SegmentSize = 26;

            if(NULL != LinkedList->Data.Address16.ResourceSource)
            {
                SegmentSize += (1 +
                    LinkedList->Data.Address16.ResourceSourceStringLength);
            }
            break;

        case ExtendedIrq:
            /*
             * Extended IRQ Resource
             */
            /*
             * The base size of this byte stream is 9. This is for an
             *  Interrupt table length of 1.  For each additional
             *  interrupt, add 4.
             * If a Resource Source string is not NULL, add 1 for the
             *  Index + the length of the null terminated string
             *  Resource Source + 1 for the null.
             */
            SegmentSize = 9;

            SegmentSize +=
                (LinkedList->Data.ExtendedIrq.NumberOfInterrupts -
                 1) * 4;

            if(NULL != ExIrq->ResourceSource)
            {
                SegmentSize += (1 +
                    LinkedList->Data.ExtendedIrq.ResourceSourceStringLength);
            }
            break;

        default:
            /*
             * If we get here, everything is out of sync,
             *  so exit with an error
             */
            return_ACPI_STATUS (AE_AML_ERROR);
            break;

        } /* switch (LinkedList->Id) */

        /*
         * Update the total
         */
        ByteStreamSizeNeeded += SegmentSize;

        /*
         * Point to the next object
         */
        LinkedList = (RESOURCE *) ((NATIVE_UINT) LinkedList +
                     (NATIVE_UINT) LinkedList->Length);
    }

    /*
     * This is the data the caller needs
     */
    *SizeNeeded = ByteStreamSizeNeeded;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsCalculateListLength
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource byte stream
 *              ByteStreamBufferLength  - Size of ByteStreamBuffer
 *              SizeNeeded              - UINT32 pointer of the size buffer
 *                                          needed to properly return the
 *                                          parsed data
 *
 * RETURN:      Status  AE_OK if okay, else a valid ACPI_STATUS code
 *
 * DESCRIPTION: Takes the resource byte stream and parses it once, calculating
 *              the size buffer needed to hold the linked list that conveys
 *              the resource data.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsCalculateListLength (
    UINT8                   *ByteStreamBuffer,
    UINT32                  ByteStreamBufferLength,
    UINT32                  *SizeNeeded)
{
    UINT32                  BufferSize = 0;
    UINT32                  BytesParsed = 0;
    UINT8                   NumberOfInterrupts = 0;
    UINT8                   NumberOfChannels = 0;
    UINT8                   ResourceType;
    UINT32                  StructureSize;
    UINT32                  BytesConsumed;
    UINT8                   *Buffer;
    UINT8                   Temp8;
    UINT16                  Temp16;
    UINT8                   Index;
    UINT8                   AdditionalBytes;


    FUNCTION_TRACE ("RsCalculateListLength");


    while (BytesParsed < ByteStreamBufferLength)
    {
        /*
         * Look at the next byte in the stream
         */
        ResourceType = *ByteStreamBuffer;

        /*
         * See if this is a small or large resource
         */
        if(ResourceType & 0x80)
        {
            /*
             * Large Resource Type
             */
            switch (ResourceType)
            {
            case MEMORY_RANGE_24:
                /*
                 * 24-Bit Memory Resource
                 */
                BytesConsumed = 12;

                StructureSize = sizeof (MEMORY24_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA;
                break;

            case LARGE_VENDOR_DEFINED:
                /*
                 * Vendor Defined Resource
                 */
                Buffer = ByteStreamBuffer;
                ++Buffer;

                MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);
                BytesConsumed = Temp16 + 3;

                /*
                 * Ensure a 32-bit boundary for the structure
                 */
                Temp16 = (UINT16) ROUND_UP_TO_32BITS (Temp16);

                StructureSize = sizeof (VENDOR_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (Temp16 * sizeof (UINT8));
                break;

            case MEMORY_RANGE_32:
                /*
                 * 32-Bit Memory Range Resource
                 */

                BytesConsumed = 20;

                StructureSize = sizeof (MEMORY32_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA;
                break;

            case FIXED_MEMORY_RANGE_32:
                /*
                 * 32-Bit Fixed Memory Resource
                 */
                BytesConsumed = 12;

                StructureSize = sizeof(FIXED_MEMORY32_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA;
                break;

            case DWORD_ADDRESS_SPACE:
                /*
                 * 32-Bit Address Resource
                 */
                Buffer = ByteStreamBuffer;

                ++Buffer;
                MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

                BytesConsumed = Temp16 + 3;

                /*
                 * Resource Source Index and Resource Source are
                 *  optional elements.  Check the length of the
                 *  Bytestream.  If it is greater than 23, that
                 *  means that an Index exists and is followed by
                 *  a null termininated string.  Therefore, set
                 *  the temp variable to the length minus the minimum
                 *  byte stream length plus the byte for the Index to
                 *  determine the size of the NULL terminiated string.
                 */
                if (23 < Temp16)
                {
                    Temp8 = (UINT8) (Temp16 - 24);
                }
                else
                {
                    Temp8 = 0;
                }

                /*
                 * Ensure a 32-bit boundary for the structure
                 */
                Temp8 = (UINT8) ROUND_UP_TO_32BITS (Temp8);

                StructureSize = sizeof (ADDRESS32_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (Temp8 * sizeof (UINT8));
                break;

            case WORD_ADDRESS_SPACE:
                /*
                 * 16-Bit Address Resource
                 */
                Buffer = ByteStreamBuffer;

                ++Buffer;
                MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

                BytesConsumed = Temp16 + 3;

                /*
                 * Resource Source Index and Resource Source are
                 *  optional elements.  Check the length of the
                 *  Bytestream.  If it is greater than 13, that
                 *  means that an Index exists and is followed by
                 *  a null termininated string.  Therefore, set
                 *  the temp variable to the length minus the minimum
                 *  byte stream length plus the byte for the Index to
                 *  determine the size of the NULL terminiated string.
                 */
                if (13 < Temp16)
                {
                    Temp8 = (UINT8) (Temp16 - 14);
                }
                else
                {
                    Temp8 = 0;
                }

                /*
                 * Ensure a 32-bit boundry for the structure
                 */
                Temp8 = (UINT8) ROUND_UP_TO_32BITS (Temp8);

                StructureSize = sizeof (ADDRESS16_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (Temp8 * sizeof (UINT8));
                break;

            case EXTENDED_IRQ:
                /*
                 * Extended IRQ
                 */
                Buffer = ByteStreamBuffer;

                ++Buffer;
                MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

                BytesConsumed = Temp16 + 3;

                /*
                 * Point past the length field and the
                 *  Interrupt vector flags to save off the
                 *  Interrupt table length to the Temp8 variable.
                 */
                Buffer += 3;
                Temp8 = *Buffer;

                /*
                 * To compensate for multiple interrupt numbers,
                 *  Add 4 bytes for each additional interrupts
                 *  greater than 1
                 */
                AdditionalBytes = (UINT8) ((Temp8 - 1) * 4);

                /*
                 * Resource Source Index and Resource Source are
                 *  optional elements.  Check the length of the
                 *  Bytestream.  If it is greater than 9, that
                 *  means that an Index exists and is followed by
                 *  a null termininated string.  Therefore, set
                 *  the temp variable to the length minus the minimum
                 *  byte stream length plus the byte for the Index to
                 *  determine the size of the NULL terminiated string.
                 */
                if (9 + AdditionalBytes < Temp16)
                {
                    Temp8 = (UINT8) (Temp16 - (9 + AdditionalBytes));
                }

                else
                {
                    Temp8 = 0;
                }

                /*
                 * Ensure a 32-bit boundry for the structure
                 */
                Temp8 = (UINT8) ROUND_UP_TO_32BITS (Temp8);

                StructureSize = sizeof (EXTENDED_IRQ_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (AdditionalBytes * sizeof (UINT8)) +
                                (Temp8 * sizeof (UINT8));

                break;

/* TBD: [Future] 64-bit not currently supported */
/*
            case 0x8A:
                break;
*/

            default:
                /*
                 * If we get here, everything is out of sync,
                 *  so exit with an error
                 */
                return_ACPI_STATUS (AE_AML_ERROR);
                break;
            }
        }

        else
        {
            /*
             * Small Resource Type
             *  Only bits 7:3 are valid
             */
            ResourceType >>= 3;

            switch (ResourceType)
            {
            case IRQ_FORMAT:
                /*
                 * IRQ Resource
                 */
                /*
                 * Determine if it there are two or three
                 *  trailing bytes
                 */
                Buffer = ByteStreamBuffer;
                Temp8 = *Buffer;

                if(Temp8 & 0x01)
                {
                    BytesConsumed = 4;
                }

                else
                {
                    BytesConsumed = 3;
                }

                /*
                 * Point past the descriptor
                 */
                ++Buffer;

                /*
                 * Look at the number of bits set
                 */
                MOVE_UNALIGNED16_TO_16 (&Temp16, Buffer);

                for (Index = 0; Index < 16; Index++)
                {
                    if (Temp16 & 0x1)
                    {
                        ++NumberOfInterrupts;
                    }

                    Temp16 >>= 1;
                }

                StructureSize = sizeof (IO_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (NumberOfInterrupts * sizeof (UINT32));
                break;


            case DMA_FORMAT:

                /*
                 * DMA Resource
                 */
                Buffer = ByteStreamBuffer;

                BytesConsumed = 3;

                /*
                 * Point past the descriptor
                 */
                ++Buffer;

                /*
                 * Look at the number of bits set
                 */
                Temp8 = *Buffer;

                for(Index = 0; Index < 8; Index++)
                {
                    if(Temp8 & 0x1)
                    {
                        ++NumberOfChannels;
                    }

                    Temp8 >>= 1;
                }

                StructureSize = sizeof (DMA_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (NumberOfChannels * sizeof (UINT32));
                break;


            case START_DEPENDENT_TAG:

                /*
                 * Start Dependent Functions Resource
                 */
                /*
                 * Determine if it there are two or three trailing bytes
                 */
                Buffer = ByteStreamBuffer;
                Temp8 = *Buffer;

                if(Temp8 & 0x01)
                {
                    BytesConsumed = 2;
                }
                else
                {
                    BytesConsumed = 1;
                }


                StructureSize =
                        sizeof (START_DEPENDENT_FUNCTIONS_RESOURCE) +
                        RESOURCE_LENGTH_NO_DATA;
                break;


            case END_DEPENDENT_TAG:

                /*
                 * End Dependent Functions Resource
                 */
                BytesConsumed = 1;
                StructureSize = RESOURCE_LENGTH;
                break;


            case IO_PORT_DESCRIPTOR:
                /*
                 * IO Port Resource
                 */
                BytesConsumed = 8;
                StructureSize = sizeof (IO_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA;
                break;


            case FIXED_LOCATION_IO_DESCRIPTOR:

                /*
                 * Fixed IO Port Resource
                 */
                BytesConsumed = 4;
                StructureSize = sizeof (FIXED_IO_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA;
                break;


            case SMALL_VENDOR_DEFINED:

                /*
                 * Vendor Specific Resource
                 */
                Buffer = ByteStreamBuffer;

                Temp8 = *Buffer;
                Temp8 = (UINT8) (Temp8 & 0x7);
                BytesConsumed = Temp8 + 1;

                /*
                 * Ensure a 32-bit boundry for the structure
                 */
                Temp8 = (UINT8) ROUND_UP_TO_32BITS (Temp8);
                StructureSize = sizeof (VENDOR_RESOURCE) +
                                RESOURCE_LENGTH_NO_DATA +
                                (Temp8 * sizeof (UINT8));
                break;


            case END_TAG:

                /*
                 * End Tag
                 */
                BytesConsumed = 2;
                StructureSize = RESOURCE_LENGTH;
                break;


            default:
                /*
                 * If we get here, everything is out of sync,
                 *  so exit with an error
                 */
                return_ACPI_STATUS (AE_AML_ERROR);
                break;

            } /* switch */

        }  /* if(ResourceType & 0x80) */

        /*
         * Update the return value and counter
         */
        BufferSize += StructureSize;
        BytesParsed += BytesConsumed;

        /*
         * Set the byte stream to point to the next resource
         */
        ByteStreamBuffer += BytesConsumed;

    }

    /*
     * This is the data the caller needs
     */
    *SizeNeeded = BufferSize;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsCalculatePciRoutingTableLength
 *
 * PARAMETERS:  PackageObject           - Pointer to the package object
 *              BufferSizeNeeded        - UINT32 pointer of the size buffer
 *                                          needed to properly return the
 *                                          parsed data
 *
 * RETURN:      Status  AE_OK
 *
 * DESCRIPTION: Given a package representing a PCI routing table, this
 *                calculates the size of the corresponding linked list of
 *                descriptions.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsCalculatePciRoutingTableLength (
    ACPI_OPERAND_OBJECT     *PackageObject,
    UINT32                  *BufferSizeNeeded)
{
    UINT32                  NumberOfElements;
    UINT32                  TempSizeNeeded;
    ACPI_OPERAND_OBJECT     **TopObjectList;
    UINT32                  Index;
    ACPI_OPERAND_OBJECT     *PackageElement;
    ACPI_OPERAND_OBJECT     **SubObjectList;
    BOOLEAN                 NameFound;
    UINT32                  TableIndex;


    FUNCTION_TRACE ("AcpiRsCalculatePciRoutingTableLength");


    NumberOfElements = PackageObject->Package.Count;

    /*
     * Calculate the size of the return buffer.
     * The base size is the number of elements * the sizes of the
     * structures.  Additional space for the strings is added below.
     * The minus one is to subtract the size of the UINT8 Source[1]
     * member because it is added below.
     *
     * NOTE: The NumberOfElements is incremented by one to add an end
     * table structure that is essentially a structure of zeros.
     */
    TempSizeNeeded = (NumberOfElements + 1) *
                       (sizeof (PCI_ROUTING_TABLE) - 1);

    /*
     * But each PRT_ENTRY structure has a pointer to a string and
     * the size of that string must be found.
     */
    TopObjectList = PackageObject->Package.Elements;

    for (Index = 0; Index < NumberOfElements; Index++)
    {
        /*
         * Dereference the sub-package
         */
        PackageElement = *TopObjectList;

        /*
         * The SubObjectList will now point to an array of the
         * four IRQ elements: Address, Pin, Source and SourceIndex
         */
        SubObjectList = PackageElement->Package.Elements;

        /*
         * Scan the IrqTableElements for the Source Name String
         */
        NameFound = FALSE;

        for (TableIndex = 0; TableIndex < 4 && !NameFound; TableIndex++)
        {
            if (ACPI_TYPE_STRING == (*SubObjectList)->Common.Type)
            {
                NameFound = TRUE;
            }

            else
            {
                /*
                 * Look at the next element
                 */
                SubObjectList++;
            }
        }

        /*
         * Was a String type found?
         */
        if (TRUE == NameFound)
        {
            /*
             * The length String.Length field includes the
             * terminating NULL
             */
            TempSizeNeeded += (*SubObjectList)->String.Length;
        }

        else
        {
            /*
             * If no name was found, then this is a NULL, which is
             *  translated as a UINT32 zero.
             */
            TempSizeNeeded += sizeof(UINT32);
        }

        /*
         * Point to the next ACPI_OPERAND_OBJECT
         */
        TopObjectList++;
    }

    /* Align the count before returning it */

    *BufferSizeNeeded = ROUND_UP_TO_32BITS (TempSizeNeeded);

    return_ACPI_STATUS (AE_OK);
}